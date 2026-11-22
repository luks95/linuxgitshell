// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "OverlayStatusProvider.h"

#include "DaemonLogging.h"
#include "RepositoryStatusTranslator.h"

#include "linuxgitshell/repositorycontext/RepositoryContextSnapshot.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QStorageInfo>

#include <algorithm>
#include <limits>
#include <utility>

namespace LinuxGitShell
{
namespace
{

using Clock = OverlayStatusProvider::Clock;
using std::chrono::milliseconds;

[[nodiscard]] quint64 newGeneration()
{
    quint64 generation = 0;
    while (generation == 0)
    {
        generation = QRandomGenerator::system()->generate64();
    }
    return generation;
}

[[nodiscard]] bool hasPrefixComponents(const QString& root, const QString& path)
{
    if (root == QLatin1String("/"))
    {
        return path.startsWith(QLatin1Char('/'));
    }
    return path == root || path.startsWith(root + QLatin1Char('/'));
}

[[nodiscard]] QString relativeKeyFor(const QString& root, const QString& path)
{
    if (path == root)
    {
        return {};
    }
    return root == QLatin1String("/") ? path.mid(1) : path.mid(root.size() + 1);
}

// Pseudo-filesystems are never watched. Compared by component so that /runner is unaffected.
[[nodiscard]] bool isExcludedLocation(const QString& path)
{
    static const QStringList excluded{QStringLiteral("/proc"), QStringLiteral("/sys"),
                                      QStringLiteral("/dev"), QStringLiteral("/run")};
    return std::any_of(excluded.cbegin(), excluded.cend(), [&path](const QString& prefix)
                       { return hasPrefixComponents(prefix, path); });
}

// Mounted network filesystems are excluded for the MVP. A location whose filesystem cannot be
// classified is treated like one: a `file:` URL alone does not prove a local filesystem.
[[nodiscard]] bool isRemoteOrUnclassified(const QString& path)
{
    const QStorageInfo storage(path);
    if (!storage.isValid() || !storage.isReady())
    {
        return true;
    }
    const QString type = QString::fromLatin1(storage.fileSystemType()).toLower();
    static const QStringList remote{
        QStringLiteral("nfs"),    QStringLiteral("cifs"),      QStringLiteral("smb"),
        QStringLiteral("afs"),    QStringLiteral("ceph"),      QStringLiteral("gluster"),
        QStringLiteral("9p"),     QStringLiteral("sshfs"),     QStringLiteral("davfs"),
        QStringLiteral("rclone"), QStringLiteral("ncpfs"),     QStringLiteral("lustre"),
        QStringLiteral("beegfs"), QStringLiteral("fuse.sshfs")};
    return std::any_of(remote.cbegin(), remote.cend(),
                       [&type](const QString& name) { return type.contains(name); });
}

// Reports whether `relative` lies strictly below one of `boundaries` (component-aware).
[[nodiscard]] bool belowAny(const QSet<QString>& boundaries, const QString& relative)
{
    if (boundaries.isEmpty())
    {
        return false;
    }
    qsizetype slash = relative.indexOf(QLatin1Char('/'));
    while (slash >= 0)
    {
        if (boundaries.contains(relative.left(slash)))
        {
            return true;
        }
        slash = relative.indexOf(QLatin1Char('/'), slash + 1);
    }
    return false;
}

[[nodiscard]] QString canonicalLexicalPath(const QFileInfo& info)
{
    // A symlink is itself the observed item: resolve only its directory.
    if (info.isSymLink())
    {
        const QString directory = QFileInfo(info.absolutePath()).canonicalFilePath();
        return directory.isEmpty()
                   ? QString()
                   : QDir::cleanPath(directory + QLatin1Char('/') + info.fileName());
    }
    return info.canonicalFilePath();
}

} // namespace

OverlayStatusProvider::OverlayStatusProvider(QObject* parent)
    : OverlayStatusProvider(OverlayProviderLimits{}, OverlayProviderOptions{}, parent)
{
}

OverlayStatusProvider::OverlayStatusProvider(const OverlayProviderLimits& providerLimits,
                                             const OverlayProviderOptions& providerOptions,
                                             QObject* parent)
    : QObject(parent), limits(providerLimits), options(providerOptions),
      currentGeneration(newGeneration())
{
    qRegisterMetaType<OverlayRecord>();
    qRegisterMetaType<QList<OverlayRecord>>();
    qRegisterMetaType<RepositoryDiscoveryResult>();
    qRegisterMetaType<GitStatusResult>();
    qRegisterMetaType<GitInventoryResult>();

    limits.index.freshness = limits.freshness;

    const std::size_t workerCount = std::max<std::size_t>(1, limits.concurrentResolutions);
    for (std::size_t index = 0; index < workerCount; ++index)
    {
        auto worker = std::make_unique<ResolutionWorker>();
        worker->discovery = std::make_unique<RepositoryDiscovery>();
        worker->timeout.setSingleShot(true);
        ResolutionWorker* const workerPointer = worker.get();
        connect(workerPointer->discovery.get(), &RepositoryDiscovery::finished, this,
                [this, workerPointer](const RepositoryDiscoveryResult& result)
                { resolutionFinished(*workerPointer, result); });
        connect(&workerPointer->timeout, &QTimer::timeout, this,
                [workerPointer]
                {
                    workerPointer->timedOut = true;
                    workerPointer->discovery->cancel();
                });
        workers.push_back(std::move(worker));
    }

    housekeeping.setInterval(limits.housekeepingInterval);
    connect(&housekeeping, &QTimer::timeout, this, &OverlayStatusProvider::expireLeases);
}

OverlayStatusProvider::~OverlayStatusProvider()
{
    for (const auto& worker : workers)
    {
        disconnect(worker->discovery.get(), nullptr, this, nullptr);
        disconnect(&worker->timeout, nullptr, this, nullptr);
        worker->discovery->cancel();
    }
    for (const auto& repo : repositories)
    {
        stopRepositoryWork(*repo);
        disposeCollector(*repo);
    }
}

OverlayStatusProvider::Clock::time_point OverlayStatusProvider::now() const
{
    return limits.clock ? limits.clock() : Clock::now();
}

quint64 OverlayStatusProvider::nextRevision() { return ++revisionCounter; }

quint64 OverlayStatusProvider::generation() const { return currentGeneration; }

qsizetype OverlayStatusProvider::observationCount() const { return totalObservations; }

qsizetype OverlayStatusProvider::repositoryCount() const { return repositories.size(); }

qsizetype OverlayStatusProvider::maxRecordsPerMessage() const { return limits.maxRecordsPerBatch; }

OverlayRequestResult OverlayStatusProvider::requestOverlays(const QString& owner,
                                                            const QStringList& paths,
                                                            quint64 requestId)
{
    OverlayRequestResult result;
    result.generation = currentGeneration;

    const QString requestKey = owner + QLatin1Char('\n') + QString::number(requestId);
    if (owner.isEmpty() || requestId == 0 || paths.size() > limits.maxPathsPerRequest ||
        outstandingRequests.contains(requestKey))
    {
        result.error = OverlayRequestError::InvalidRequest;
        return result;
    }

    const Clock::time_point current = now();
    QSet<QString> seen;
    QStringList refreshedRepositories;
    auto& ownerObservations = observations[owner];
    for (const QString& path : paths)
    {
        const QString key =
            path.size() > limits.maxPathLength ? QString() : repositoryContextPathKey(path);
        if (key.isEmpty() || seen.contains(key))
        {
            continue;
        }
        seen.insert(key);

        auto it = ownerObservations.find(key);
        const bool isNew = it == ownerObservations.end();
        if (isNew)
        {
            if (totalObservations >= limits.maxObservations)
            {
                continue;
            }
            it = ownerObservations.insert(key, Observation{});
            ++totalObservations;
        }
        it->expires = current + limits.lease;
        result.acceptedPaths.append(key);

        Observation& observation = it.value();
        const bool retry = observation.resolution == Resolution::Failed ||
                           (observation.resolution == Resolution::Unavailable &&
                            current - observation.resolvedAt >= limits.freshness);
        if (isNew || retry)
        {
            observation.resolution = Resolution::Unresolved;
            resolveObservation(key, observation);
        }
        else if (observation.resolution == Resolution::Resolved)
        {
            if (Repository* const repo = repository(observation.repositoryRoot))
            {
                repo->lastUsed = current;
                ensureFresh(*repo);
            }
        }
    }
    if (ownerObservations.isEmpty())
    {
        observations.remove(owner);
    }

    if (!housekeeping.isActive() && totalObservations > 0)
    {
        housekeeping.start();
    }

    if (!result.acceptedPaths.isEmpty())
    {
        outstandingRequests.insert(requestKey);
        queueInitial(owner, requestId, result.acceptedPaths);
    }
    qCDebug(daemonLog) << "Overlay request: requested" << paths.size() << "accepted"
                       << result.acceptedPaths.size() << "observations" << totalObservations;
    return result;
}

void OverlayStatusProvider::releaseOverlays(const QString& owner, const QStringList& paths)
{
    auto it = observations.find(owner);
    if (it == observations.end())
    {
        return;
    }
    for (const QString& path : paths)
    {
        const QString key = repositoryContextPathKey(path);
        if (it->remove(key) > 0)
        {
            --totalObservations;
        }
    }
    if (it->isEmpty())
    {
        observations.erase(it);
    }
    expireLeases();
}

void OverlayStatusProvider::releaseOwner(const QString& owner)
{
    const auto it = observations.find(owner);
    if (it == observations.end())
    {
        return;
    }
    totalObservations -= it->size();
    observations.erase(it);
    expireLeases();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool OverlayStatusProvider::refreshRepository(const QString& owner, const QString& repositoryRoot)
{
    const auto it = observations.constFind(owner);
    if (it == observations.cend())
    {
        return false;
    }
    const QString root = QDir::cleanPath(repositoryRoot);
    const bool observed = std::any_of(it->cbegin(), it->cend(),
                                      [&root](const Observation& observation)
                                      {
                                          return observation.resolution == Resolution::Resolved &&
                                                 observation.repositoryRoot == root;
                                      });
    Repository* const repo = observed ? repository(root) : nullptr;
    if (repo == nullptr || repo->unavailable)
    {
        return false;
    }

    ++repo->invalidations;
    repo->hasSnapshot = false;
    (void)repo->index.invalidate(RepositoryStatusValidity::Pending, nextRevision());
    // Replies produced while handling a call must follow the method reply.
    QTimer::singleShot(0, this,
                       [this, root]
                       {
                           if (Repository* const current = repository(root))
                           {
                               publishRepository(*current);
                           }
                       });
    scheduleRefresh(*repo, limits.debounce);
    return true;
}

void OverlayStatusProvider::expireLeases()
{
    const Clock::time_point current = now();
    for (auto owner = observations.begin(); owner != observations.end();)
    {
        for (auto it = owner->begin(); it != owner->end();)
        {
            if (it->expires <= current)
            {
                it = owner->erase(it);
                --totalObservations;
            }
            else
            {
                ++it;
            }
        }
        owner = owner->isEmpty() ? observations.erase(owner) : std::next(owner);
    }

    for (auto it = notRepositoryDirectories.begin(); it != notRepositoryDirectories.end();)
    {
        it = current - it.value() >= limits.freshness ? notRepositoryDirectories.erase(it)
                                                      : std::next(it);
    }

    // Stop work nobody observes; the idle index is kept only until it is evicted or expires.
    for (const auto& repo : repositories)
    {
        if (observerCount(repo->root) == 0)
        {
            stopRepositoryWork(*repo);
        }
    }
    if (totalObservations == 0)
    {
        housekeeping.stop();
    }
}

qsizetype OverlayStatusProvider::observerCount(const QString& root) const
{
    qsizetype count = 0;
    for (const auto& owner : observations)
    {
        for (const Observation& observation : owner)
        {
            if (observation.resolution == Resolution::Resolved &&
                observation.repositoryRoot == root)
            {
                ++count;
            }
        }
    }
    return count;
}

OverlayStatusProvider::Repository* OverlayStatusProvider::repository(const QString& root) const
{
    const auto it = repositories.constFind(root);
    return it == repositories.cend() ? nullptr : it.value().get();
}

OverlayStatusProvider::Repository*
OverlayStatusProvider::findKnownRepository(const QString& path) const
{
    Repository* best = nullptr;
    // A nested repository's own root directory is an entry of its parent (submodule or untracked
    // directory), so the parent's index answers for it; its contents belong to the nested index.
    Repository* parentOfRoot = nullptr;
    for (const auto& repo : repositories)
    {
        if (!hasPrefixComponents(repo->root, path))
        {
            continue;
        }
        const QString relative = relativeKeyFor(repo->root, path);
        if (belowAny(repo->excludedRoots, relative))
        {
            continue;
        }
        if (best == nullptr || best->root.size() < repo->root.size())
        {
            best = repo.get();
        }
        if (repo->root != path && repo->excludedRoots.contains(relative) &&
            (parentOfRoot == nullptr || parentOfRoot->root.size() < repo->root.size()))
        {
            parentOfRoot = repo.get();
        }
    }
    return parentOfRoot != nullptr ? parentOfRoot : best;
}

OverlayStatusProvider::Repository* OverlayStatusProvider::addRepository(const QString& root,
                                                                        bool unavailable)
{
    if (Repository* const existing = repository(root))
    {
        return existing;
    }
    while (repositories.size() >= limits.maxRepositories)
    {
        if (!evictIdleRepository())
        {
            return nullptr;
        }
    }

    RepositoryStatusIndexOptions indexOptions;
    indexOptions.recursive = options.recursive;
    indexOptions.includeUntracked = true;
    indexOptions.includeIgnored = false;
    auto repo = std::make_unique<Repository>(root, limits.index, indexOptions);
    repo->unavailable = unavailable;
    repo->lastUsed = now();
    repo->poll.setSingleShot(true);
    repo->debounce.setSingleShot(true);
    Repository* const pointer = repo.get();
    connect(&pointer->poll, &QTimer::timeout, this,
            [this, pointer]
            {
                if (observerCount(pointer->root) > 0)
                {
                    scheduleRefresh(*pointer, std::chrono::milliseconds::zero());
                }
            });
    connect(&pointer->debounce, &QTimer::timeout, this,
            [this, pointer] { enqueueRefresh(*pointer); });
    if (!unavailable)
    {
        (void)repo->index.invalidate(RepositoryStatusValidity::Pending, nextRevision());
    }
    repositories.insert(root, std::move(repo));
    return pointer;
}

bool OverlayStatusProvider::evictIdleRepository()
{
    Repository* oldest = nullptr;
    for (const auto& repo : repositories)
    {
        if (observerCount(repo->root) == 0 &&
            (oldest == nullptr || repo->lastUsed < oldest->lastUsed))
        {
            oldest = repo.get();
        }
    }
    if (oldest == nullptr)
    {
        return false;
    }
    destroyRepository(oldest->root);
    return true;
}

void OverlayStatusProvider::destroyRepository(const QString& root)
{
    const auto it = repositories.find(root);
    if (it == repositories.end())
    {
        return;
    }
    Repository& repo = *it.value();
    stopRepositoryWork(repo);
    disposeCollector(repo);
    refreshQueue.removeAll(root);
    repositories.erase(it);
}

void OverlayStatusProvider::stopRepositoryWork(Repository& repo)
{
    repo.poll.stop();
    repo.debounce.stop();
    repo.followUp = false;
    if (repo.queued)
    {
        refreshQueue.removeAll(repo.root);
        repo.queued = false;
    }
    if (repo.refreshing)
    {
        disposeCollector(repo);
        repo.refreshing = false;
        if (activeRefreshes > 0)
        {
            --activeRefreshes;
        }
        startRefreshes();
    }
}

void OverlayStatusProvider::disposeCollector(Repository& repo)
{
    if (!repo.collector)
    {
        return;
    }
    // The collector may be the sender of the signal being handled, so it is destroyed later.
    Collector* const collector = repo.collector.release();
    collector->deadline.stop();
    collector->statusReader.disconnect();
    collector->inventoryReader.disconnect();
    collector->statusReader.cancel();
    collector->inventoryReader.cancel();
    collector->deleteLater();
}

void OverlayStatusProvider::attach(Observation& observation, Repository& repo,
                                   const QString& relativeKey)
{
    observation.repositoryRoot = repo.root;
    observation.relativeKey = relativeKey;
    observation.resolution = Resolution::Resolved;
    observation.resolvedAt = now();
    repo.lastUsed = observation.resolvedAt;
    ensureFresh(repo);
}

void OverlayStatusProvider::resolveObservation(const QString& path, Observation& observation)
{
    if (isExcludedLocation(path))
    {
        observation.resolution = Resolution::Unavailable;
        observation.resolvedAt = now();
        return;
    }
    if (Repository* const repo = findKnownRepository(path))
    {
        if (repo->unavailable)
        {
            observation.resolution = Resolution::Unavailable;
            observation.resolvedAt = now();
            observation.repositoryRoot = repo->root;
            return;
        }
        attach(observation, *repo, relativeKeyFor(repo->root, path));
        return;
    }
    if (queuedResolutions.contains(path))
    {
        observation.resolution = Resolution::Queued;
        return;
    }
    if (queuedResolutions.size() >= limits.maxPendingResolutions)
    {
        observation.resolution = Resolution::Failed;
        observation.resolvedAt = now();
        return;
    }
    observation.resolution = Resolution::Queued;
    queueResolution(path);
}

void OverlayStatusProvider::queueResolution(const QString& path)
{
    queuedResolutions.insert(path);
    resolutionQueue.append(path);
    // Start after the caller has finished updating its bookkeeping.
    if (!resolutionScheduled)
    {
        resolutionScheduled = true;
        QTimer::singleShot(0, this,
                           [this]
                           {
                               resolutionScheduled = false;
                               startResolutions();
                           });
    }
}

void OverlayStatusProvider::startResolutions()
{
    for (const auto& worker : workers)
    {
        while (worker->path.isEmpty() && !resolutionQueue.isEmpty())
        {
            const QString path = resolutionQueue.takeFirst();

            // A repository learned in the meantime answers without more Git work.
            if (Repository* const known = findKnownRepository(path))
            {
                queuedResolutions.remove(path);
                if (known->unavailable)
                {
                    settlePath(path, Resolution::Unavailable, known->root, {});
                }
                else
                {
                    settlePath(path, Resolution::Resolved, known->root,
                               relativeKeyFor(known->root, path));
                }
                continue;
            }

            const QFileInfo info(path);
            const QString canonical = canonicalLexicalPath(info);
            if (!info.exists() && !info.isSymLink())
            {
                queuedResolutions.remove(path);
                settlePath(path, Resolution::Unavailable, {}, {});
                continue;
            }
            if (canonical.isEmpty() || isExcludedLocation(canonical))
            {
                queuedResolutions.remove(path);
                settlePath(path, Resolution::Unavailable, {}, {});
                continue;
            }
            const bool isDirectory = info.isDir() && !info.isSymLink();
            const QString negativeKey =
                isDirectory ? canonical : QFileInfo(canonical).absolutePath();
            if (notRepositoryDirectories.contains(negativeKey))
            {
                queuedResolutions.remove(path);
                settlePath(path, Resolution::Unavailable, {}, {});
                continue;
            }

            worker->canonicalPath = canonical;
            worker->isDirectory = isDirectory;
            if (worker->discovery->discover(path) == RepositoryDiscoveryStartResult::Accepted)
            {
                worker->path = path;
                worker->timedOut = false;
                worker->timeout.start(limits.resolutionTimeout);
                break;
            }
            queuedResolutions.remove(path);
            settlePath(path, Resolution::Failed, {}, {});
        }
    }
}

void OverlayStatusProvider::resolutionFinished(ResolutionWorker& worker,
                                               const RepositoryDiscoveryResult& result)
{
    worker.timeout.stop();
    const QString path = std::exchange(worker.path, QString());
    const QString canonical = std::exchange(worker.canonicalPath, QString());
    const bool isDirectory = worker.isDirectory;
    queuedResolutions.remove(path);

    if (path.isEmpty())
    {
        startResolutions();
        return;
    }

    if (result.repository.has_value())
    {
        const RepositoryInfo& info = result.repository.value();
        const QString root = QDir::cleanPath(info.repositoryRoot);
        bool unavailable = info.type == RepositoryType::Bare || !info.isInsideWorkTree ||
                           root.isEmpty() || isExcludedLocation(root) ||
                           isRemoteOrUnclassified(root);
        // Sparse-checkout coverage rules are not implemented, so such repositories publish
        // Unavailable rather than risking omitted paths being reported clean.
        for (const QString& gitDirectory : {info.gitDirectory, info.commonGitDirectory})
        {
            if (!gitDirectory.isEmpty() &&
                QFileInfo::exists(gitDirectory + QStringLiteral("/info/sparse-checkout")))
            {
                unavailable = true;
            }
        }

        Repository* const repo = root.isEmpty() ? nullptr : addRepository(root, unavailable);
        if (repo == nullptr)
        {
            settlePath(path, Resolution::Failed, {}, {});
        }
        else if (repo->unavailable)
        {
            settlePath(path, Resolution::Unavailable, root, {});
        }
        else if (!hasPrefixComponents(root, canonical))
        {
            settlePath(path, Resolution::Unavailable, {}, {});
        }
        else
        {
            settlePath(path, Resolution::Resolved, root, relativeKeyFor(root, canonical));
        }
        attachUnresolved();
    }
    else if (result.error == RepositoryDiscoveryError::NotRepository)
    {
        if (notRepositoryDirectories.size() >= limits.maxPendingResolutions)
        {
            notRepositoryDirectories.clear();
        }
        notRepositoryDirectories.insert(
            isDirectory ? canonical : QFileInfo(canonical).absolutePath(), now());
        settlePath(path, Resolution::Unavailable, {}, {});
    }
    else
    {
        settlePath(path, Resolution::Failed, {}, {});
    }
    startResolutions();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void OverlayStatusProvider::settlePath(const QString& path, Resolution resolution,
                                       const QString& repositoryRoot, const QString& relativeKey)
{
    Repository* const repo = repositoryRoot.isEmpty() ? nullptr : repository(repositoryRoot);
    for (auto& owner : observations)
    {
        const auto it = owner.find(path);
        if (it == owner.end() ||
            (it->resolution != Resolution::Queued && it->resolution != Resolution::Unresolved))
        {
            continue;
        }
        if (resolution == Resolution::Resolved && repo != nullptr)
        {
            attach(it.value(), *repo, relativeKey);
        }
        else
        {
            it->resolution = resolution;
            it->repositoryRoot = repositoryRoot;
            it->resolvedAt = now();
        }
    }
    emitForPath(path);
}

void OverlayStatusProvider::reevaluateBoundaries(Repository& repo)
{
    if (repo.excludedRoots.isEmpty())
    {
        return;
    }
    // Paths requested before this repository knew its nested roots may belong to another index,
    // and a nested root directory itself belongs to this one.
    for (auto& owner : observations)
    {
        for (auto it = owner.begin(); it != owner.end(); ++it)
        {
            if (it->resolution != Resolution::Resolved)
            {
                continue;
            }
            if (it->repositoryRoot == repo.root)
            {
                if (belowAny(repo.excludedRoots, it->relativeKey))
                {
                    it->resolution = Resolution::Unresolved;
                    it->repositoryRoot.clear();
                    it->relativeKey.clear();
                    resolveObservation(it.key(), it.value());
                }
            }
            else if (it->relativeKey.isEmpty() &&
                     hasPrefixComponents(repo.root, it->repositoryRoot) &&
                     repo.excludedRoots.contains(relativeKeyFor(repo.root, it->repositoryRoot)))
            {
                attach(it.value(), repo, relativeKeyFor(repo.root, it->repositoryRoot));
            }
        }
    }
}

void OverlayStatusProvider::attachUnresolved()
{
    QSet<QString> changed;
    for (auto& owner : observations)
    {
        for (auto it = owner.begin(); it != owner.end(); ++it)
        {
            if (it->resolution != Resolution::Queued)
            {
                continue;
            }
            Repository* const repo = findKnownRepository(it.key());
            if (repo == nullptr || repo->unavailable)
            {
                continue;
            }
            attach(it.value(), *repo, relativeKeyFor(repo->root, it.key()));
            changed.insert(it.key());
        }
    }
    for (const QString& path : std::as_const(changed))
    {
        emitForPath(path);
    }
}

OverlayRecord OverlayStatusProvider::buildRecord(const QString& path,
                                                 const Observation& observation)
{
    OverlayRecord record;
    record.path = path;
    record.repositoryRoot = observation.repositoryRoot;
    switch (observation.resolution)
    {
    case Resolution::Unresolved:
    case Resolution::Queued:
        record.validity = RepositoryStatusValidity::Pending;
        return record;
    case Resolution::Unavailable:
        record.validity = RepositoryStatusValidity::Unavailable;
        return record;
    case Resolution::Failed:
        record.validity = RepositoryStatusValidity::Degraded;
        return record;
    case Resolution::Resolved:
        break;
    }

    const Repository* const repo = repository(observation.repositoryRoot);
    if (repo == nullptr || repo->unavailable)
    {
        record.validity = RepositoryStatusValidity::Unavailable;
        return record;
    }
    const Clock::time_point current = now();
    const RepositoryStatusLookup lookup = repo->index.lookup(observation.relativeKey, current);
    record.validity = lookup.validity;
    if (lookup.validity == RepositoryStatusValidity::Ready)
    {
        record.state = lookup.state;
        const auto remaining = repo->capturedAt + limits.freshness - current;
        record.validForMs = static_cast<quint32>(
            std::clamp<qint64>(std::chrono::duration_cast<milliseconds>(remaining).count(), 0,
                               std::numeric_limits<quint32>::max()));
    }
    return record;
}

void OverlayStatusProvider::emitForPath(const QString& path)
{
    QHash<QString, QList<OverlayRecord>> byOwner;
    const quint64 revision = nextRevision();
    for (auto owner = observations.cbegin(); owner != observations.cend(); ++owner)
    {
        const auto it = owner->constFind(path);
        if (it == owner->cend())
        {
            continue;
        }
        OverlayRecord record = buildRecord(path, it.value());
        record.revision = revision;
        byOwner[owner.key()].append(record);
    }
    emitRecords(byOwner);
}

void OverlayStatusProvider::publishRepository(Repository& repo)
{
    QHash<QString, QList<OverlayRecord>> byOwner;
    const quint64 revision = nextRevision();
    for (auto owner = observations.cbegin(); owner != observations.cend(); ++owner)
    {
        for (auto it = owner->cbegin(); it != owner->cend(); ++it)
        {
            if (it->resolution != Resolution::Resolved || it->repositoryRoot != repo.root)
            {
                continue;
            }
            OverlayRecord record = buildRecord(it.key(), it.value());
            record.revision = revision;
            byOwner[owner.key()].append(record);
        }
    }
    emitRecords(byOwner);
}

void OverlayStatusProvider::emitRecords(const QHash<QString, QList<OverlayRecord>>& byOwner)
{
    for (auto it = byOwner.cbegin(); it != byOwner.cend(); ++it)
    {
        const QList<OverlayRecord>& records = it.value();
        for (qsizetype start = 0; start < records.size(); start += limits.maxRecordsPerBatch)
        {
            Q_EMIT overlaysChanged(it.key(), records.mid(start, limits.maxRecordsPerBatch),
                                   currentGeneration);
        }
    }
}

void OverlayStatusProvider::queueInitial(const QString& owner, quint64 requestId,
                                         const QStringList& paths)
{
    queuedInitial.append({owner, requestId, paths});
    if (!flushScheduled)
    {
        flushScheduled = true;
        QTimer::singleShot(0, this, &OverlayStatusProvider::flushInitial);
    }
}

void OverlayStatusProvider::flushInitial()
{
    flushScheduled = false;
    const auto pendingReplies = std::exchange(queuedInitial, {});
    for (const QueuedInitial& reply : pendingReplies)
    {
        outstandingRequests.remove(reply.owner + QLatin1Char('\n') +
                                   QString::number(reply.requestId));
    }
    for (const QueuedInitial& reply : pendingReplies)
    {
        // Built now, not when requested, so that an initial record is never older than a
        // publication that already reached this caller.
        const quint64 revision = nextRevision();
        const auto& ownerObservations = observations.value(reply.owner);
        QList<OverlayRecord> records;
        records.reserve(reply.paths.size());
        for (const QString& path : reply.paths)
        {
            const auto it = ownerObservations.constFind(path);
            OverlayRecord record;
            if (it != ownerObservations.cend())
            {
                record = buildRecord(path, it.value());
            }
            else
            {
                // Released before delivery: the accepted path still gets its single record.
                record.path = path;
            }
            record.revision = revision;
            records.append(record);
        }
        for (qsizetype start = 0; start < records.size(); start += limits.maxRecordsPerBatch)
        {
            Q_EMIT initialResults(reply.owner, reply.requestId,
                                  records.mid(start, limits.maxRecordsPerBatch), currentGeneration);
        }
    }
}

std::chrono::milliseconds OverlayStatusProvider::pollDelay(const Repository& repo) const
{
    std::chrono::milliseconds delay = limits.pollInterval;
    for (int step = 0; step < std::min(repo.failures, limits.backoffShiftLimit); ++step)
    {
        delay *= 2;
    }
    return std::min(limits.maxBackoff, delay);
}

void OverlayStatusProvider::ensureFresh(Repository& repo)
{
    if (repo.unavailable || repo.refreshing || repo.queued || repo.debounce.isActive())
    {
        return;
    }
    const RepositoryStatusLookup lookup = repo.index.lookup(QString(), now());
    if (lookup.validity != RepositoryStatusValidity::Ready)
    {
        scheduleRefresh(repo, std::chrono::milliseconds::zero());
    }
    else if (!repo.poll.isActive())
    {
        repo.poll.start(pollDelay(repo));
    }
}

void OverlayStatusProvider::scheduleRefresh(Repository& repo, std::chrono::milliseconds delay)
{
    if (repo.unavailable)
    {
        return;
    }
    if (repo.refreshing)
    {
        repo.followUp = true;
        return;
    }
    if (repo.queued)
    {
        return;
    }
    repo.poll.stop();
    repo.debounce.start(delay);
}

void OverlayStatusProvider::enqueueRefresh(Repository& repo)
{
    if (repo.refreshing || repo.queued || repo.unavailable)
    {
        return;
    }
    repo.queued = true;
    refreshQueue.append(repo.root);
    startRefreshes();
}

void OverlayStatusProvider::startRefreshes()
{
    while (activeRefreshes < std::max<std::size_t>(1, limits.concurrentRefreshes) &&
           !refreshQueue.isEmpty())
    {
        const QString root = refreshQueue.takeFirst();
        Repository* const repo = repository(root);
        if (repo == nullptr)
        {
            continue;
        }
        repo->queued = false;
        if (observerCount(root) == 0)
        {
            continue;
        }
        startRefresh(*repo);
    }
}

void OverlayStatusProvider::startRefresh(Repository& repo)
{
    auto collector = std::make_unique<Collector>();
    collector->invalidationsAtStart = repo.invalidations;
    collector->deadline.setSingleShot(true);
    collector->elapsed.start();
    Repository* const repoPointer = &repo;
    Collector* const collectorPointer = collector.get();
    connect(&collector->statusReader, &GitStatusReader::finished, this,
            [this, repoPointer](const GitStatusResult& result)
            { statusFinished(*repoPointer, result); });
    connect(&collector->inventoryReader, &GitInventoryReader::finished, this,
            [this, repoPointer](const GitInventoryResult& result)
            { inventoryFinished(*repoPointer, result); });
    connect(&collector->deadline, &QTimer::timeout, this,
            [collectorPointer]
            {
                collectorPointer->timedOut = true;
                collectorPointer->statusReader.cancel();
                collectorPointer->inventoryReader.cancel();
            });

    GitStatusRequest request;
    request.path = repo.root;
    request.includeIgnored = true;
    request.detectRenames = true;
    request.timeout = limits.collectionDeadline;
    request.maxOutputBytes = limits.maxOutputBytes;

    repo.collector = std::move(collector);
    repo.refreshing = true;
    ++activeRefreshes;
    repo.collector->deadline.start(limits.collectionDeadline);
    if (repo.collector->statusReader.start(request) != GitStatusStartResult::Accepted)
    {
        failRefresh(repo);
    }
}

void OverlayStatusProvider::statusFinished(Repository& repo, const GitStatusResult& result)
{
    if (!repo.refreshing || !repo.collector)
    {
        return;
    }
    if (result.error != GitStatusReadError::None || !result.status.has_value())
    {
        failRefresh(repo);
        return;
    }
    repo.collector->status = result.status;

    // Nested repositories and submodules own their contents. Only entries Git reports as a
    // directory or submodule can be boundaries, so filesystem checks stay bounded by the status.
    QSet<QString> boundaries;
    for (const GitStatusEntry& entry : std::as_const(repo.collector->status->entries))
    {
        QString path = entry.path;
        if (path.size() > 1 && path.endsWith(QLatin1Char('/')))
        {
            path.chop(1);
        }
        const bool candidate =
            entry.submodule.isSubmodule ||
            (entry.kind == GitStatusRecordKind::Untracked && entry.path.endsWith(QLatin1Char('/')));
        if (candidate && !path.isEmpty() &&
            (entry.submodule.isSubmodule ||
             QFileInfo::exists(repo.root + QLatin1Char('/') + path + QStringLiteral("/.git"))))
        {
            boundaries.insert(path);
        }
    }
    QStringList sortedBoundaries = boundaries.values();
    sortedBoundaries.sort();
    QCryptographicHash digest(QCryptographicHash::Sha256);
    digest.addData(result.gitResult.standardOutput);
    digest.addData(QByteArrayView("\0", 1));
    digest.addData(sortedBoundaries.join(QLatin1Char('\n')).toUtf8());
    repo.collector->boundaries = std::move(boundaries);
    repo.collector->statusDigest = digest.result();

    GitInventoryRequest request;
    request.repositoryRoot = repo.root;
    request.timeout = limits.collectionDeadline;
    request.maxOutputBytes = limits.maxOutputBytes;
    if (repo.hasSnapshot && repo.collector->statusDigest == repo.statusDigest)
    {
        request.unchangedOutputDigest = repo.inventoryDigest;
    }
    if (repo.collector->inventoryReader.start(request) != GitInventoryStartResult::Accepted)
    {
        failRefresh(repo);
    }
}

void OverlayStatusProvider::inventoryFinished(Repository& repo, const GitInventoryResult& result)
{
    if (!repo.refreshing || !repo.collector)
    {
        return;
    }
    if (result.error != GitInventoryReadError::None ||
        (!result.paths.has_value() && !result.unchanged) || !repo.collector->status.has_value() ||
        repo.collector->timedOut)
    {
        failRefresh(repo);
        return;
    }
    completeRefresh(repo, result);
}

void OverlayStatusProvider::completeRefresh(Repository& repo, const GitInventoryResult& result)
{
    const qint64 elapsed = repo.collector->elapsed.elapsed();
    if (!repo.collector->status.has_value() || (!result.paths.has_value() && !result.unchanged))
    {
        failRefresh(repo);
        return;
    }
    const bool invalidated = repo.collector->invalidationsAtStart != repo.invalidations;

    RepositoryStatusTranslationInput input;
    input.revision = nextRevision();
    input.capturedAt = now();
    input.status = *repo.collector->status;
    if (result.paths.has_value())
    {
        input.trackedPaths = *result.paths;
    }
    QSet<QString> boundaries = std::move(repo.collector->boundaries);
    input.excludedRoots = boundaries.values();
    const QByteArray statusDigest = repo.collector->statusDigest;

    disposeCollector(repo);
    repo.refreshing = false;
    --activeRefreshes;

    // An unchanged inventory carries no paths, so it can only renew a snapshot that is still
    // stored exactly as it was when the digests were taken.
    const bool renewed =
        result.unchanged && !invalidated && repo.hasSnapshot && repo.statusDigest == statusDigest &&
        repo.inventoryDigest == result.outputDigest && repo.index.renew(input.capturedAt);
    if (invalidated || (result.unchanged && !renewed))
    {
        // Invalidation during collection discards its result and schedules another refresh. A
        // snapshot lost under an unchanged inventory is rebuilt from scratch by the next one.
        qCDebug(daemonLog) << "Overlay refresh discarded after" << elapsed << "ms";
        if (!invalidated)
        {
            repo.statusDigest.clear();
            repo.inventoryDigest.clear();
        }
        repo.followUp = true;
        finishRefreshBookkeeping(repo);
        startRefreshes();
        return;
    }

    if (renewed)
    {
        // Nothing Git reports changed: keep the index, only extend its freshness.
        qCDebug(daemonLog) << "Overlay refresh unchanged after" << elapsed << "ms";
        repo.capturedAt = input.capturedAt;
        repo.failures = 0;
        publishRepository(repo);
        finishRefreshBookkeeping(repo);
        startRefreshes();
        return;
    }

    const RepositoryStatusStoreResult stored = repo.index.replace(translateRepositoryStatus(input));
    qCDebug(daemonLog) << "Overlay refresh finished in" << elapsed << "ms, entries"
                       << input.status.entries.size() << "tracked" << input.trackedPaths.size()
                       << "stored" << static_cast<int>(stored);
    if (stored == RepositoryStatusStoreResult::Accepted)
    {
        repo.excludedRoots = std::move(boundaries);
        reevaluateBoundaries(repo);
        repo.capturedAt = input.capturedAt;
        repo.statusDigest = statusDigest;
        repo.inventoryDigest = result.outputDigest;
        repo.hasSnapshot = true;
        repo.failures = 0;
        enforceIndexBudget(repo);
    }
    else
    {
        ++repo.failures;
        repo.hasSnapshot = false;
    }
    publishRepository(repo);
    finishRefreshBookkeeping(repo);
    startRefreshes();
}

void OverlayStatusProvider::failRefresh(Repository& repo)
{
    qCDebug(daemonLog) << "Overlay refresh failed after"
                       << (repo.collector ? repo.collector->elapsed.elapsed() : 0) << "ms";
    disposeCollector(repo);
    if (repo.refreshing)
    {
        repo.refreshing = false;
        if (activeRefreshes > 0)
        {
            --activeRefreshes;
        }
    }
    ++repo.failures;
    repo.hasSnapshot = false;
    (void)repo.index.invalidate(RepositoryStatusValidity::Degraded, nextRevision());
    publishRepository(repo);
    finishRefreshBookkeeping(repo);
    startRefreshes();
}

void OverlayStatusProvider::finishRefreshBookkeeping(Repository& repo)
{
    if (observerCount(repo.root) == 0)
    {
        return;
    }
    if (repo.followUp)
    {
        repo.followUp = false;
        scheduleRefresh(repo, limits.debounce);
    }
    else
    {
        repo.poll.start(pollDelay(repo));
    }
}

void OverlayStatusProvider::enforceIndexBudget(const Repository& current)
{
    const auto total = [this]
    {
        qsizetype bytes = 0;
        for (const auto& repo : repositories)
        {
            bytes += repo->index.accountedBytes();
        }
        return bytes;
    };

    while (total() > limits.maxTotalIndexBytes)
    {
        Repository* victim = nullptr;
        for (const auto& repo : repositories)
        {
            if (repo.get() == &current || repo->index.accountedBytes() == 0 ||
                observerCount(repo->root) != 0)
            {
                continue;
            }
            if (victim == nullptr || repo->lastUsed < victim->lastUsed)
            {
                victim = repo.get();
            }
        }
        if (victim == nullptr)
        {
            break;
        }
        victim->index.clear();
        victim->hasSnapshot = false;
    }

    if (total() > limits.maxTotalIndexBytes)
    {
        // Everything else is observed: degrade the newest snapshot instead of exceeding the budget.
        if (Repository* const repo = repository(current.root))
        {
            (void)repo->index.invalidate(RepositoryStatusValidity::Degraded, nextRevision());
            repo->hasSnapshot = false;
            ++repo->failures;
        }
    }
}

} // namespace LinuxGitShell
