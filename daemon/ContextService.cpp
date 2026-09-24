// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "ContextService.h"

#include "DaemonLogging.h"

#include <QFileInfo>
#include <QRandomGenerator>

#include <algorithm>
#include <utility>

namespace LinuxGitShell
{
namespace
{

using Clock = RepositoryContextCache::Clock;

[[nodiscard]] quint64 newGeneration()
{
    quint64 generation = 0;
    while (generation == 0)
    {
        generation = QRandomGenerator::system()->generate64();
    }
    return generation;
}

[[nodiscard]] RepositoryContextType contextType(RepositoryType type)
{
    switch (type)
    {
    case RepositoryType::Normal:
        return RepositoryContextType::Normal;
    case RepositoryType::Bare:
        return RepositoryContextType::Bare;
    case RepositoryType::LinkedWorktree:
        return RepositoryContextType::LinkedWorktree;
    case RepositoryType::Submodule:
        return RepositoryContextType::Submodule;
    }
    return RepositoryContextType::Normal;
}

[[nodiscard]] RepositoryContextOperation contextOperation(RepositoryOperation operation)
{
    switch (operation)
    {
    case RepositoryOperation::Merge:
        return RepositoryContextOperation::Merge;
    case RepositoryOperation::Rebase:
        return RepositoryContextOperation::Rebase;
    case RepositoryOperation::CherryPick:
        return RepositoryContextOperation::CherryPick;
    case RepositoryOperation::Revert:
        return RepositoryContextOperation::Revert;
    case RepositoryOperation::Bisect:
        return RepositoryContextOperation::Bisect;
    }
    return RepositoryContextOperation::Merge;
}

[[nodiscard]] RepositoryContextSnapshot errorSnapshot()
{
    RepositoryContextSnapshot snapshot;
    snapshot.state = RepositoryContextState::DiscoveryError;
    return snapshot;
}

} // namespace

ContextService::ContextService(QObject* parent) : ContextService(ContextServiceLimits{}, parent) {}

ContextService::ContextService(const ContextServiceLimits& limits, QObject* parent)
    : QObject(parent), limits(limits), cache(limits.cache), currentGeneration(newGeneration())
{
    qRegisterMetaType<RepositoryDiscoveryResult>();
    cache.setGeneration(currentGeneration);

    const std::size_t workerCount = std::max<std::size_t>(1, limits.concurrentDiscoveries);
    for (std::size_t index = 0; index < workerCount; ++index)
    {
        auto worker = std::make_unique<Worker>();
        worker->discovery = std::make_unique<RepositoryDiscovery>();
        worker->timeout.setSingleShot(true);
        Worker* const workerPointer = worker.get();
        connect(workerPointer->discovery.get(), &RepositoryDiscovery::finished, this,
                [this, workerPointer](const RepositoryDiscoveryResult& result)
                { discoveryFinished(*workerPointer, result); });
        connect(&workerPointer->timeout, &QTimer::timeout, this,
                [workerPointer]
                {
                    workerPointer->timedOut = true;
                    workerPointer->discovery->cancel();
                });
        workers.push_back(std::move(worker));
    }
}

ContextService::~ContextService()
{
    for (const auto& worker : workers)
    {
        disconnect(worker->discovery.get(), nullptr, this, nullptr);
        disconnect(&worker->timeout, nullptr, this, nullptr);
        worker->discovery->cancel();
    }
}

quint64 ContextService::requestContext(const QStringList& paths)
{
    const Clock::time_point now = Clock::now();
    qsizetype ignored = 0;
    qsizetype warm = 0;
    qsizetype queued = 0;

    for (qsizetype index = 0; index < paths.size(); ++index)
    {
        if (index >= limits.maxPathsPerRequest)
        {
            ignored += paths.size() - index;
            break;
        }

        const QString& path = paths.at(index);
        const QString pathKey =
            path.size() > limits.maxPathLength ? QString() : repositoryContextPathKey(path);
        if (pathKey.isEmpty())
        {
            ++ignored;
            continue;
        }
        if (pending.contains(pathKey))
        {
            continue;
        }

        const RepositoryContextLookup lookup = cache.lookup(pathKey, now);
        if (lookup.snapshot.has_value())
        {
            queueReply(pathKey, lookup.snapshot.value());
            ++warm;
            continue;
        }
        if (queue.size() >= limits.maxQueuedPaths)
        {
            ++ignored;
            continue;
        }
        queue.append(pathKey);
        pending.insert(pathKey);
        ++queued;
    }

    qCDebug(daemonLog) << "Context request: warm" << warm << "queued" << queued << "ignored"
                       << ignored << "pending" << pending.size();
    startDiscoveries();
    return currentGeneration;
}

quint64 ContextService::generation() const { return currentGeneration; }

void ContextService::startDiscoveries()
{
    for (const auto& worker : workers)
    {
        while (worker->pathKey.isEmpty() && !queue.isEmpty())
        {
            const QString pathKey = queue.takeFirst();
            if (worker->discovery->discover(pathKey) == RepositoryDiscoveryStartResult::Accepted)
            {
                worker->pathKey = pathKey;
                worker->timedOut = false;
                worker->timeout.start(limits.discoveryTimeout);
                worker->elapsed.start();
                break;
            }

            // Missing paths and start failures are short-lived errors, answered asynchronously
            // like every other reply.
            pending.remove(pathKey);
            const RepositoryContextSnapshot snapshot = errorSnapshot();
            (void)cache.store(pathKey, snapshot, currentGeneration, Clock::now());
            queueReply(pathKey, snapshot);
        }
    }
}

void ContextService::discoveryFinished(Worker& worker, const RepositoryDiscoveryResult& result)
{
    worker.timeout.stop();
    const QString pathKey = std::exchange(worker.pathKey, QString());
    pending.remove(pathKey);

    const std::optional<RepositoryContextSnapshot> snapshot = snapshotFor(result, worker.timedOut);
    qCDebug(daemonLog) << "Context discovery finished in" << worker.elapsed.elapsed() << "ms,"
                       << "error" << static_cast<int>(result.error) << "timed out"
                       << worker.timedOut;
    if (snapshot.has_value() && !pathKey.isEmpty())
    {
        (void)cache.store(pathKey, snapshot.value(), currentGeneration, Clock::now());
        publish(pathKey, snapshot.value());
    }
    startDiscoveries();
}

void ContextService::publish(const QString& pathKey, const RepositoryContextSnapshot& snapshot)
{
    Q_EMIT contextReady(pathKey, snapshot, currentGeneration);
}

void ContextService::queueReply(const QString& pathKey, const RepositoryContextSnapshot& snapshot)
{
    queuedReplies.append({pathKey, snapshot});
    if (!flushScheduled)
    {
        flushScheduled = true;
        QTimer::singleShot(0, this, &ContextService::flushQueuedReplies);
    }
}

void ContextService::flushQueuedReplies()
{
    flushScheduled = false;
    const auto replies = std::exchange(queuedReplies, {});
    for (const auto& [pathKey, snapshot] : replies)
    {
        publish(pathKey, snapshot);
    }
}

std::optional<RepositoryContextSnapshot>
ContextService::snapshotFor(const RepositoryDiscoveryResult& result, bool timedOut)
{
    if (result.repository.has_value())
    {
        const RepositoryInfo& repository = result.repository.value();
        RepositoryContextSnapshot snapshot;
        snapshot.state = RepositoryContextState::InsideRepository;
        snapshot.repositoryRoot = repository.repositoryRoot;
        snapshot.type = contextType(repository.type);
        for (const RepositoryOperation operation : repository.operations)
        {
            snapshot.operations.append(contextOperation(operation));
        }
        const QString canonicalPath = QFileInfo(result.requestedPath).canonicalFilePath();
        snapshot.isRepositoryRoot = result.requestedPath == repository.repositoryRoot ||
                                    canonicalPath == repository.repositoryRoot;
        snapshot.hasRemote = !repository.remotes.isEmpty();
        snapshot.hasUpstream = !repository.upstream.isEmpty();
        return snapshot;
    }

    switch (result.error)
    {
    case RepositoryDiscoveryError::NotRepository:
    {
        RepositoryContextSnapshot snapshot;
        snapshot.state = RepositoryContextState::OutsideRepository;
        return snapshot;
    }
    case RepositoryDiscoveryError::Cancelled:
        if (!timedOut)
        {
            return std::nullopt;
        }
        return errorSnapshot();
    case RepositoryDiscoveryError::None:
    case RepositoryDiscoveryError::GitUnavailable:
    case RepositoryDiscoveryError::PermissionDenied:
    case RepositoryDiscoveryError::InvalidOutput:
    case RepositoryDiscoveryError::TimedOut:
    case RepositoryDiscoveryError::GitFailure:
        break;
    }
    return errorSnapshot();
}

} // namespace LinuxGitShell
