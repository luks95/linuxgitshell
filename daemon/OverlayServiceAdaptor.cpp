// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "OverlayServiceAdaptor.h"

#include "DaemonLogging.h"

#include "linuxgitshell/repositorycontext/OverlayWire.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>

namespace LinuxGitShell
{

OverlayServiceObject::OverlayServiceObject(OverlayStatusProvider& provider, QObject* parent)
    : QObject(parent), statusProvider(provider)
{
}

OverlayStatusProvider& OverlayServiceObject::provider() const { return statusProvider; }

OverlayServiceAdaptor::OverlayServiceAdaptor(OverlayServiceObject* object)
    : QDBusAbstractAdaptor(object), object(object), provider(&object->provider()),
      watcher(QString(), QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForUnregistration)
{
    qDBusRegisterMetaType<QList<QVariantMap>>();

    connect(provider, &OverlayStatusProvider::initialResults, this,
            [this](const QString& owner, quint64 requestId, const QList<OverlayRecord>& records,
                   quint64 generation)
            { sendRecords(owner, "OverlayResults", requestId, true, records, generation); });
    connect(provider, &OverlayStatusProvider::overlaysChanged, this,
            [this](const QString& owner, const QList<OverlayRecord>& records, quint64 generation)
            { sendRecords(owner, "OverlaysChanged", 0, false, records, generation); });

    // A vanished client releases everything it observed; leases expire anyway if this is missed.
    connect(&watcher, &QDBusServiceWatcher::serviceUnregistered, this,
            [this](const QString& owner)
            {
                this->provider->releaseOwner(owner);
                watcher.removeWatchedService(owner);
                qCDebug(daemonLog) << "Overlay client disconnected";
            });
}

qulonglong OverlayServiceAdaptor::RequestOverlays(const QStringList& paths, qulonglong requestId,
                                                  QStringList& acceptedPaths)
{
    const QString owner = object->message().service();
    const OverlayRequestResult result =
        provider->requestOverlays(owner, paths, static_cast<quint64>(requestId));
    if (result.error != OverlayRequestError::None)
    {
        object->sendErrorReply(QString(OverlayInvalidRequestError),
                               QStringLiteral("Invalid overlay request"));
        return 0;
    }
    acceptedPaths = result.acceptedPaths;
    if (!acceptedPaths.isEmpty() && !watcher.watchedServices().contains(owner))
    {
        watcher.addWatchedService(owner);
    }
    return static_cast<qulonglong>(result.generation);
}

void OverlayServiceAdaptor::ReleaseOverlays(const QStringList& paths)
{
    provider->releaseOverlays(object->message().service(), paths);
}

bool OverlayServiceAdaptor::RefreshRepository(const QString& root)
{
    return provider->refreshRepository(object->message().service(), root);
}

void OverlayServiceAdaptor::sendTo(const QString& owner, const char* signalName,
                                   const QVariantList& arguments) const
{
    QDBusMessage signal = QDBusMessage::createTargetedSignal(owner, QString(OverlayObjectPath),
                                                             QString(OverlayInterfaceName),
                                                             QString::fromLatin1(signalName));
    signal.setArguments(arguments);
    QDBusConnection::sessionBus().send(signal);
}

void OverlayServiceAdaptor::sendRecords(const QString& owner, const char* signalName,
                                        quint64 requestId, bool withRequestId,
                                        const QList<OverlayRecord>& records,
                                        quint64 generation) const
{
    // Keep every message below the ceiling, including long multibyte paths.
    const auto batches =
        splitOverlayRecords(records, provider->maxRecordsPerMessage(), MaxMessageBytes);
    for (const QList<OverlayRecord>& batch : batches)
    {
        QList<QVariantMap> encoded;
        encoded.reserve(batch.size());
        for (const OverlayRecord& record : batch)
        {
            encoded.append(overlayRecordToVariantMap(record));
        }
        QVariantList arguments;
        if (withRequestId)
        {
            arguments.append(QVariant::fromValue<qulonglong>(requestId));
        }
        arguments.append(QVariant::fromValue(encoded));
        arguments.append(QVariant::fromValue<qulonglong>(generation));
        sendTo(owner, signalName, arguments);
    }
}

} // namespace LinuxGitShell
