/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDebug>
#include <QPointer>
#include <QCoreApplication>
#include <QThread>

#include "global.h"
#include "qmlapplicationinterface.h"
#include "qmlapplicationinterfaceextension.h"
#include "qmlnotification.h"
#include "notification.h"
#include "ipcwrapperobject.h"
#include "utilities.h"

QT_BEGIN_NAMESPACE_AM

QmlApplicationInterface *QmlApplicationInterface::s_instance = nullptr;

QmlApplicationInterface::QmlApplicationInterface(const QString &dbusConnectionName, const QString &dbusNotificationBusName, QObject *parent)
    : ApplicationInterface(parent)
    , m_connection(dbusConnectionName)
    , m_notificationConnection(dbusNotificationBusName)
{
    if (QmlApplicationInterface::s_instance)
        qCritical("ERROR: only one instance of QmlApplicationInterface is allowed");
    s_instance = this;
}

bool QmlApplicationInterface::initialize()
{
    // we are working with very small delays in the milli-second range here, so a linear factor
    // to support valgrind would have to be very large and probably conflict with usage elsewhere
    // in the codebase, where the ranges are normally in the seconds.
    static const int timeout = timeoutFactor() * timeoutFactor();

    auto tryConnect = [](const QString &service, const QString &path, const QString &interfaceName,
                         const QDBusConnection &conn, QObject *parent) -> QDBusInterface * {
        for (int i = 0; i < 100; ++i) {
            QDBusInterface *iface = new QDBusInterface(service, path, interfaceName, conn, parent);
            if (!iface->lastError().isValid())
                return iface;
            delete iface;
            QThread::msleep(static_cast<unsigned long>(timeout));
        }
        return nullptr;
    };

    m_runtimeIf = tryConnect(qSL(""), qSL("/RuntimeInterface"), qSL("io.qt.ApplicationManager.RuntimeInterface"),
                             m_connection, this);
    m_applicationIf = tryConnect(qSL(""), qSL("/ApplicationInterface"), qSL("io.qt.ApplicationManager.ApplicationInterface"),
                                 m_connection, this);
    m_notifyIf = tryConnect(qSL("org.freedesktop.Notifications"), qSL("/org/freedesktop/Notifications"), qSL("org.freedesktop.Notifications"),
                            m_notificationConnection, this);


    if (!m_applicationIf) {
        qCritical("ERROR: could not connect to the ApplicationInterface on the P2P D-Bus");
        return false;
    }
    if (!m_runtimeIf) {
        qCritical("ERROR: could not connect to the RuntimeInterface on the P2P D-Bus");
        return false;
    }
    if (!m_applicationIf->isValid()) {
        qCritical("ERROR: ApplicationInterface on the P2P D-Bus is not valid: %s",
                  qPrintable(m_applicationIf->lastError().message()));
        return false;
    }
    if (!m_runtimeIf->isValid()) {
        qCritical("ERROR: RuntimeInterface on the P2P D-Bus is not valid: %s",
                  qPrintable(m_runtimeIf->lastError().message()));
        return false;
    }

    bool ok = true;
    ok = ok && connect(m_runtimeIf, SIGNAL(startApplication(QString,QString,QString,QString,QVariantMap,QVariantMap)),
                       this,        SIGNAL(startApplication(QString,QString,QString,QString,QVariantMap,QVariantMap)));

    if (!ok)
        qCritical("ERROR: could not connect the RuntimeInterface via D-Bus: %s", qPrintable(m_runtimeIf->lastError().name()));

    ok = ok && connect(m_applicationIf, SIGNAL(quit()), this, SIGNAL(quit()));
    ok = ok && connect(m_applicationIf, SIGNAL(memoryLowWarning()), this, SIGNAL(memoryLowWarning()));
    ok = ok && connect(m_applicationIf, SIGNAL(memoryCriticalWarning()), this, SIGNAL(memoryCriticalWarning()));
    ok = ok && connect(m_applicationIf, SIGNAL(openDocument(QString,QString)), this, SIGNAL(openDocument(QString,QString)));
    ok = ok && connect(m_applicationIf, SIGNAL(slowAnimationsChanged(bool)), this, SIGNAL(slowAnimationsChanged(bool)));

    if (!ok)
        qCritical("ERROR: could not connect the ApplicationInterface via D-Bus: %s", qPrintable(m_applicationIf->lastError().name()));

    if (m_notifyIf) {
        ok = ok && connect(m_notifyIf, SIGNAL(NotificationClosed(uint,uint)), this, SLOT(notificationClosed(uint,uint)));
        ok = ok && connect(m_notifyIf, SIGNAL(ActionInvoked(uint,QString)), this, SLOT(notificationActionTriggered(uint,QString)));

        if (!ok)
            qCritical("ERROR: could not connect the org.freedesktop.Notifications interface via D-Bus: %s", qPrintable(m_notifyIf->lastError().name()));
    } else {
        qCritical("ERROR: could not create the org.freedesktop.Notifications interface on D-Bus");
    }

    QmlApplicationInterfaceExtension::initialize(m_connection);

    if (ok)
        finishedInitialization();
    return ok;
}

QString QmlApplicationInterface::applicationId() const
{
    if (m_appId.isEmpty() && m_applicationIf->isValid())
        m_appId = m_applicationIf->property("applicationId").toString();
    return m_appId;
}

QVariantMap QmlApplicationInterface::name() const
{
    return m_name;
}

QUrl QmlApplicationInterface::icon() const
{
    return QUrl::fromLocalFile(m_icon);
}

QString QmlApplicationInterface::version() const
{
    return m_version;
}

Notification *QmlApplicationInterface::createNotification()
{
    QmlNotification *n = new QmlNotification(this, Notification::Dynamic);
    return n;
}

void QmlApplicationInterface::acknowledgeQuit() const
{
    QCoreApplication::instance()->quit();
}

void QmlApplicationInterface::finishedInitialization()
{
    if (m_applicationIf->isValid())
        m_applicationIf->asyncCall(qSL("finishedInitialization"));
}

QVariantMap QmlApplicationInterface::systemProperties() const
{
    return m_systemProperties;
}

QVariantMap QmlApplicationInterface::applicationProperties() const
{
    return m_applicationProperties;
}

uint QmlApplicationInterface::notificationShow(QmlNotification *n)
{
    if (n && m_notifyIf && m_notifyIf->isValid()) {
        QDBusReply<uint> newId = m_notifyIf->call(qSL("Notify"), applicationId(), n->notificationId(),
                                                  n->icon().toString(), n->summary(), n->body(),
                                                  n->libnotifyActionList(), n->libnotifyHints(),
                                                  n->timeout());
        if (newId.isValid()) {
            m_allNotifications << n;
            return newId;
        }
    }
    return 0;
}


void QmlApplicationInterface::notificationClose(QmlNotification *n)
{
    if (n && m_notifyIf && m_notifyIf->isValid())
        m_notifyIf->asyncCall(qSL("CloseNotification"), n->notificationId());
}

void QmlApplicationInterface::notificationClosed(uint notificationId, uint reason)
{
    qDebug("Notification was closed signal: %u", notificationId);
    for (const QPointer<QmlNotification> &n : m_allNotifications) {
        if (n->notificationId() == notificationId) {
            n->libnotifyNotificationClosed(reason);
            m_allNotifications.removeAll(n);
            break;
        }
    }
}

void QmlApplicationInterface::notificationActionTriggered(uint notificationId, const QString &actionId)
{
    qDebug("Notification action triggered signal: %u %s", notificationId, qPrintable(actionId));
    for (const QPointer<QmlNotification> &n : m_allNotifications) {
        if (n->notificationId() == notificationId) {
            n->libnotifyActionInvoked(actionId);
            break;
        }
    }
}

QT_END_NAMESPACE_AM
