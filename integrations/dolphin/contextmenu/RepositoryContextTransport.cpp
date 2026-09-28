// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "RepositoryContextTransport.h"

#include "linuxgitshell/repositorycontext/RepositoryContextWire.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace LinuxGitShell::Dolphin
{
namespace
{

// Bounds how long an unanswered call, including service activation, keeps its watcher alive.
constexpr int RequestTimeoutMilliseconds = 25000;

} // namespace

DBusRepositoryContextTransport::DBusRepositoryContextTransport(QObject* parent)
    : DBusRepositoryContextTransport(RepositoryContextServiceName, parent)
{
}

DBusRepositoryContextTransport::DBusRepositoryContextTransport(const QString& serviceName,
                                                               QObject* parent)
    : RepositoryContextTransport(parent), serviceName(serviceName)
{
}

void DBusRepositoryContextTransport::requestContext(const QStringList& pathKeys)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
    {
        Q_EMIT requestFailed(pathKeys);
        return;
    }

    if (!subscribed)
    {
        subscribed = bus.connect(QString(), RepositoryContextObjectPath,
                                 RepositoryContextInterfaceName, RepositoryContextReadySignal, this,
                                 SLOT(onContextReady(QString, QVariantMap, qulonglong)));
    }

    QDBusMessage message = QDBusMessage::createMethodCall(serviceName, RepositoryContextObjectPath,
                                                          RepositoryContextInterfaceName,
                                                          RepositoryContextRequestMethod);
    message << pathKeys;

    auto* watcher =
        new QDBusPendingCallWatcher(bus.asyncCall(message, RequestTimeoutMilliseconds), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, pathKeys](QDBusPendingCallWatcher* finishedWatcher)
            {
                const QDBusPendingReply<qulonglong> reply = *finishedWatcher;
                finishedWatcher->deleteLater();
                if (reply.isError() || reply.value() == 0)
                {
                    Q_EMIT requestFailed(pathKeys);
                    return;
                }
                Q_EMIT requestAccepted(reply.value());
            });
}

void DBusRepositoryContextTransport::onContextReady(const QString& pathKey,
                                                    const QVariantMap& context,
                                                    qulonglong generation)
{
    Q_EMIT contextReceived(pathKey, context, generation);
}

} // namespace LinuxGitShell::Dolphin
