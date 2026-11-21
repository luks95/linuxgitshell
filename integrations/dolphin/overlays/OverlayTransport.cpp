// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "OverlayTransport.h"

#include "linuxgitshell/repositorycontext/OverlayWire.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include <utility>

namespace LinuxGitShell::Dolphin
{
namespace
{

// Initial records are promised within seconds; a call that outlives this is a failed request.
constexpr int RequestTimeoutMilliseconds = 5000;
constexpr QLatin1StringView BusService("org.freedesktop.DBus");
constexpr QLatin1StringView BusPath("/org/freedesktop/DBus");
constexpr QLatin1StringView BusInterface("org.freedesktop.DBus");

// QtDBus hands over `aa{sv}` either already converted (the type is registered) or as a raw
// QDBusArgument, depending on registration order; accept both and nothing else.
[[nodiscard]] bool readRecords(const QVariant& value, QList<QVariantMap>& records)
{
    if (value.metaType() == QMetaType::fromType<QList<QVariantMap>>())
    {
        records = value.value<QList<QVariantMap>>();
        return true;
    }
    if (value.canConvert<QDBusArgument>())
    {
        const QDBusArgument argument = qvariant_cast<QDBusArgument>(value);
        if (argument.currentSignature() != QLatin1String("aa{sv}"))
        {
            return false;
        }
        records = qdbus_cast<QList<QVariantMap>>(argument);
        return true;
    }
    return false;
}

} // namespace

DBusOverlayTransport::DBusOverlayTransport(QObject* parent)
    : DBusOverlayTransport(OverlayServiceName, parent)
{
}

DBusOverlayTransport::DBusOverlayTransport(const QString& serviceName, QObject* parent)
    : OverlayTransport(parent), serviceName(serviceName)
{
    qDBusRegisterMetaType<QList<QVariantMap>>();
}

void DBusOverlayTransport::subscribe()
{
    if (subscribed)
    {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    // No sender filter: filtering by a well-known name makes QtDBus resolve its owner with a
    // blocking call. The sender is authenticated in the slots instead.
    const bool results =
        bus.connect(QString(), QString(OverlayObjectPath), QString(OverlayInterfaceName),
                    QStringLiteral("OverlayResults"), this, SLOT(onOverlayResults(QDBusMessage)));
    const bool changes =
        bus.connect(QString(), QString(OverlayObjectPath), QString(OverlayInterfaceName),
                    QStringLiteral("OverlaysChanged"), this, SLOT(onOverlaysChanged(QDBusMessage)));
    const bool owners = bus.connect(QString(), QString(BusPath), QString(BusInterface),
                                    QStringLiteral("NameOwnerChanged"), this,
                                    SLOT(onNameOwnerChanged(QDBusMessage)));
    subscribed = results && changes && owners;
}

void DBusOverlayTransport::request(quint64 requestId, const QStringList& paths)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
    {
        Q_EMIT requestFailed(requestId);
        return;
    }
    subscribe();

    QDBusMessage message = QDBusMessage::createMethodCall(serviceName, QString(OverlayObjectPath),
                                                          QString(OverlayInterfaceName),
                                                          QStringLiteral("RequestOverlays"));
    message << paths << QVariant::fromValue<qulonglong>(requestId);

    const quint64 callEpoch = epoch;
    auto* watcher =
        new QDBusPendingCallWatcher(bus.asyncCall(message, RequestTimeoutMilliseconds), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, requestId, callEpoch](QDBusPendingCallWatcher* finished)
            {
                const QDBusPendingReply<qulonglong, QStringList> reply = *finished;
                finished->deleteLater();
                if (callEpoch != epoch)
                {
                    return; // stop() abandoned this call
                }
                if (reply.isError() || reply.argumentAt<0>() == 0)
                {
                    Q_EMIT requestFailed(requestId);
                    return;
                }
                // Method replies carry no sender in QtDBus, so the owner is asked of the bus
                // itself, asynchronously. Accepting waits for it, which also keeps signals behind
                // the reply.
                resolveOwner(requestId, callEpoch, reply.argumentAt<0>(), reply.argumentAt<1>());
            });
}

void DBusOverlayTransport::resolveOwner(quint64 requestId, quint64 callEpoch, quint64 generation,
                                        const QStringList& acceptedPaths)
{
    QDBusMessage message =
        QDBusMessage::createMethodCall(QString(BusService), QString(BusPath), QString(BusInterface),
                                       QStringLiteral("GetNameOwner"));
    message << serviceName;
    auto* watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(message, RequestTimeoutMilliseconds), this);
    connect(
        watcher, &QDBusPendingCallWatcher::finished, this,
        [this, requestId, callEpoch, generation, acceptedPaths](QDBusPendingCallWatcher* finished)
        {
            const QDBusPendingReply<QString> reply = *finished;
            finished->deleteLater();
            if (callEpoch != epoch)
            {
                return; // stop() abandoned this call
            }
            const QString current = reply.isError() ? QString() : reply.value();
            if (current.isEmpty())
            {
                Q_EMIT requestFailed(requestId);
                return;
            }
            if (!owner.isEmpty() && owner != current)
            {
                // A different process now serves the name: everything cached is untrusted.
                owner.clear();
                early.clear();
                Q_EMIT ownerLost();
                Q_EMIT requestFailed(requestId);
                return;
            }
            const bool firstOwner = owner.isEmpty();
            owner = current;
            Q_EMIT requestAccepted(requestId, generation, acceptedPaths);
            if (firstOwner)
            {
                adoptOwner(current);
            }
        });
}

void DBusOverlayTransport::release(const QStringList& paths)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || owner.isEmpty() || paths.isEmpty())
    {
        return; // nothing is observed without a known owner; leases expire on their own
    }
    // Addressed to the authenticated owner by unique name, so releasing never activates a service.
    QDBusMessage message = QDBusMessage::createMethodCall(owner, QString(OverlayObjectPath),
                                                          QString(OverlayInterfaceName),
                                                          QStringLiteral("ReleaseOverlays"));
    message << paths;
    message.setAutoStartService(false);
    (void)bus.asyncCall(message, RequestTimeoutMilliseconds);
}

void DBusOverlayTransport::stop()
{
    ++epoch;
    early.clear();
    owner.clear();
    if (!subscribed)
    {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.disconnect(QString(), QString(OverlayObjectPath), QString(OverlayInterfaceName),
                   QStringLiteral("OverlayResults"), this, SLOT(onOverlayResults(QDBusMessage)));
    bus.disconnect(QString(), QString(OverlayObjectPath), QString(OverlayInterfaceName),
                   QStringLiteral("OverlaysChanged"), this, SLOT(onOverlaysChanged(QDBusMessage)));
    bus.disconnect(QString(), QString(BusPath), QString(BusInterface),
                   QStringLiteral("NameOwnerChanged"), this,
                   SLOT(onNameOwnerChanged(QDBusMessage)));
    subscribed = false;
}

bool DBusOverlayTransport::decode(const QDBusMessage& message, bool isInitial,
                                  EarlyMessage& decoded)
{
    const QList<QVariant> arguments = message.arguments();
    decoded.isInitial = isInitial;
    decoded.sender = message.service();
    if (isInitial)
    {
        if (arguments.size() != 3 || !readRecords(arguments.at(1), decoded.records))
        {
            return false;
        }
        decoded.requestId = arguments.at(0).toULongLong();
        decoded.generation = arguments.at(2).toULongLong();
    }
    else
    {
        if (arguments.size() != 2 || !readRecords(arguments.at(0), decoded.records))
        {
            return false;
        }
        decoded.generation = arguments.at(1).toULongLong();
    }
    return decoded.generation != 0;
}

void DBusOverlayTransport::onOverlayResults(const QDBusMessage& message)
{
    EarlyMessage decoded;
    if (!decode(message, true, decoded))
    {
        return;
    }
    if (owner.isEmpty())
    {
        // The reply that reveals the owner may be handled after its first signals; keep a few.
        if (early.size() < MaxEarlyMessages)
        {
            early.append(decoded);
        }
        return;
    }
    deliver(decoded);
}

void DBusOverlayTransport::onOverlaysChanged(const QDBusMessage& message)
{
    EarlyMessage decoded;
    if (!decode(message, false, decoded))
    {
        return;
    }
    if (owner.isEmpty())
    {
        if (early.size() < MaxEarlyMessages)
        {
            early.append(decoded);
        }
        return;
    }
    deliver(decoded);
}

void DBusOverlayTransport::deliver(const EarlyMessage& message)
{
    if (message.sender != owner)
    {
        return; // not the authenticated service
    }
    if (message.isInitial)
    {
        Q_EMIT initialResults(message.requestId, message.records, message.generation);
    }
    else
    {
        Q_EMIT recordsChanged(message.records, message.generation);
    }
}

void DBusOverlayTransport::adoptOwner(const QString& sender)
{
    const QList<EarlyMessage> held = std::exchange(early, {});
    for (const EarlyMessage& message : held)
    {
        if (message.sender == sender)
        {
            deliver(message);
        }
    }
}

void DBusOverlayTransport::onNameOwnerChanged(const QDBusMessage& message)
{
    // Only the bus itself announces ownership changes.
    if (message.service() != BusService)
    {
        return;
    }
    const QList<QVariant> arguments = message.arguments();
    if (arguments.size() != 3 || arguments.at(0).toString() != serviceName)
    {
        return;
    }
    const QString oldOwner = arguments.at(1).toString();
    if (!owner.isEmpty() && oldOwner == owner)
    {
        owner.clear();
        early.clear();
        Q_EMIT ownerLost();
    }
}

} // namespace LinuxGitShell::Dolphin
