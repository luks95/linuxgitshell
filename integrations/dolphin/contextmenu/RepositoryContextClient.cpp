// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "RepositoryContextClient.h"

#include "linuxgitshell/repositorycontext/RepositoryContextWire.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QPointer>
#include <QTimer>

#include <utility>

namespace LinuxGitShell::Dolphin
{

Q_LOGGING_CATEGORY(dolphinContextClientLog, "linuxgitshell.dolphin.context")

RepositoryContextClient::RepositoryContextClient(
    std::unique_ptr<RepositoryContextTransport> transport,
    const RepositoryContextClientLimits& limits, QObject* parent)
    : QObject(parent), transport(std::move(transport)), limits(limits), cache(limits.cache)
{
    connect(this->transport.get(), &RepositoryContextTransport::requestAccepted, this,
            &RepositoryContextClient::adoptGeneration);
    connect(this->transport.get(), &RepositoryContextTransport::requestFailed, this,
            &RepositoryContextClient::forgetFailed);
    connect(this->transport.get(), &RepositoryContextTransport::contextReceived, this,
            &RepositoryContextClient::receiveContext);
}

RepositoryContextClient& RepositoryContextClient::instance()
{
    // Owned by the application rather than a plugin object, because Dolphin may create a new
    // plugin object for each context menu. Constructing it performs no D-Bus work.
    static QPointer<RepositoryContextClient> client;
    if (client.isNull())
    {
        client = new RepositoryContextClient(std::make_unique<DBusRepositoryContextTransport>(),
                                             RepositoryContextClientLimits{},
                                             QCoreApplication::instance());
    }
    return *client;
}

RepositoryContextLookup RepositoryContextClient::lookup(const QString& pathKey)
{
    return cache.lookup(pathKey, Clock::now());
}

void RepositoryContextClient::refreshLater(const QStringList& pathKeys)
{
    const Clock::time_point now = Clock::now();
    for (const QString& pathKey : pathKeys)
    {
        if (pathKey.isEmpty() || queued.contains(pathKey) || isInFlight(pathKey, now))
        {
            continue;
        }
        if (queued.size() + inFlight.size() >= limits.maxInFlight)
        {
            inFlight.removeIf([this, now](const auto& entry)
                              { return now - entry.value() >= limits.inFlightTimeout; });
            if (queued.size() + inFlight.size() >= limits.maxInFlight)
            {
                break;
            }
        }
        queued.append(pathKey);
    }

    if (!queued.isEmpty() && !sendScheduled)
    {
        sendScheduled = true;
        QTimer::singleShot(0, this, &RepositoryContextClient::sendQueued);
    }
}

quint64 RepositoryContextClient::generation() const { return cache.generation(); }

qsizetype RepositoryContextClient::inFlightCount() const { return inFlight.size(); }

void RepositoryContextClient::sendQueued()
{
    sendScheduled = false;
    const QStringList pathKeys = std::exchange(queued, {});
    if (pathKeys.isEmpty())
    {
        return;
    }

    const Clock::time_point now = Clock::now();
    for (const QString& pathKey : pathKeys)
    {
        inFlight.insert(pathKey, now);
    }
    transport->requestContext(pathKeys);
}

void RepositoryContextClient::adoptGeneration(quint64 generation)
{
    // A different generation means the service restarted, so older snapshots are dropped.
    if (generation != 0 && generation != cache.generation())
    {
        cache.setGeneration(generation);
    }
}

void RepositoryContextClient::receiveContext(const QString& pathKey, const QVariantMap& context,
                                             quint64 generation)
{
    inFlight.remove(pathKey);
    const std::optional<RepositoryContextSnapshot> snapshot =
        repositoryContextFromVariantMap(context);
    if (!snapshot.has_value())
    {
        qCDebug(dolphinContextClientLog) << "Ignored an unrecognized repository context";
        return;
    }

    adoptGeneration(generation);
    if (cache.store(pathKey, snapshot.value(), generation, Clock::now()))
    {
        Q_EMIT contextUpdated(pathKey);
    }
}

void RepositoryContextClient::forgetFailed(const QStringList& pathKeys)
{
    qCDebug(dolphinContextClientLog)
        << "Repository context request failed for" << pathKeys.size() << "paths";
    for (const QString& pathKey : pathKeys)
    {
        inFlight.remove(pathKey);
    }
    Q_EMIT contextUnavailable(pathKeys);
}

bool RepositoryContextClient::isInFlight(const QString& pathKey, Clock::time_point now) const
{
    const auto it = inFlight.constFind(pathKey);
    return it != inFlight.cend() && now - it.value() < limits.inFlightTimeout;
}

} // namespace LinuxGitShell::Dolphin
