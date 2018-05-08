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

#pragma once

#include <QObject>
#include <QString>
#include <QProcess>
#include <QPointer>
#include <QVariantMap>

#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QQuickItem)

QT_BEGIN_NAMESPACE_AM

class Application;
class AbstractContainer;
class AbstractRuntime;
class AbstractContainer;

class AbstractRuntimeManager : public QObject
{
    Q_OBJECT

public:
    AbstractRuntimeManager(const QString &id, QObject *parent = nullptr);

    static QString defaultIdentifier();

    QString identifier() const;
    virtual bool inProcess() const;
    virtual bool supportsQuickLaunch() const;

    virtual AbstractRuntime *create(AbstractContainer *container, const Application *app) = 0;

    QVariantMap configuration() const;
    void setConfiguration(const QVariantMap &configuration);

    QVariantMap systemPropertiesBuiltIn() const;
    QVariantMap systemProperties3rdParty() const;
    void setSystemProperties(const QVariantMap &thirdParty, const QVariantMap &builtIn);

    QVariantMap systemOpenGLConfiguration() const;
    void setSystemOpenGLConfiguration(const QVariantMap &openGLConfiguration);

private:
    QString m_id;
    QVariantMap m_configuration;
    QVariantMap m_systemPropertiesBuiltIn;
    QVariantMap m_systemProperties3rdParty;
    QVariantMap m_systemOpenGLConfiguration;
};


class AbstractRuntime : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/Runtime 1.0")

    Q_PROPERTY(AbstractContainer *container READ container)

public:
    virtual ~AbstractRuntime();

    enum State {
        Inactive,
        Startup,
        Active,
        Shutdown
    };

    AbstractContainer *container() const;
    const Application *application() const;
    AbstractRuntimeManager *manager() const;

    virtual bool needsLauncher() const;
    virtual bool isQuickLauncher() const;
    virtual bool attachApplicationToQuickLauncher(const Application *app);

    State state() const;

    enum { SecurityTokenSize = 16 };
    QByteArray securityToken() const;

    virtual void openDocument(const QString &document, const QString &mimeType);

    virtual void setSlowAnimations(bool slow);

    void setInProcessQmlEngine(QQmlEngine *view);
    QQmlEngine* inProcessQmlEngine() const;

    virtual qint64 applicationProcessId() const = 0;

    QVariantMap systemProperties() const;

    virtual bool start() = 0;
    virtual void stop(bool forceKill = false) = 0;

#if !defined(AM_HEADLESS)
    virtual void inProcessSurfaceItemReleased(QQuickItem *);
#endif

signals:
    void stateChanged(QT_PREPEND_NAMESPACE_AM(AbstractRuntime::State) newState);
    void finished(int exitCode, QProcess::ExitStatus status);

#if !defined(AM_HEADLESS)
    // these signals are for in-process mode runtimes only
    void inProcessSurfaceItemReady(QQuickItem *window);
    void inProcessSurfaceItemClosing(QQuickItem *window);
#endif

protected:
    explicit AbstractRuntime(AbstractContainer *container, const Application *app, AbstractRuntimeManager *manager);
    void setState(State newState);

    QVariantMap configuration() const;

    AbstractContainer *m_container;
    QPointer<const Application> m_app;
    AbstractRuntimeManager *m_manager;

    QByteArray m_securityToken;
    QQmlEngine *m_inProcessQmlEngine = nullptr;
    State m_state = Inactive;

    friend class AbstractRuntimeManager;
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QT_PREPEND_NAMESPACE_AM(AbstractRuntime *))
