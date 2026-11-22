// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "linuxgitshell/gitcore/GitInventoryReader.h"
#include "linuxgitshell/gitcore/GitStatusReader.h"
#include "linuxgitshell/gitcore/RepositoryDiscovery.h"
#include "linuxgitshell/repositorycontext/OverlayWire.h"
#include "linuxgitshell/repositorycontext/RepositoryStatusIndex.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace LinuxGitShell
{

enum class OverlayRequestError
{
    None,
    InvalidRequest,
};

struct OverlayRequestResult
{
    OverlayRequestError error = OverlayRequestError::None;
    quint64 generation = 0;
    // Deduplicated lexical keys that were queued and will receive exactly one initial record.
    QStringList acceptedPaths;
};

struct OverlayProviderLimits
{
    using Clock = RepositoryStatusIndex::Clock;

    qsizetype maxPathsPerRequest = 256;
    qsizetype maxPathLength = 4096;
    qsizetype maxObservations = 4096;
    qsizetype maxPendingResolutions = 1024;
    qsizetype maxRecordsPerBatch = 256;
    qsizetype maxRepositories = 64;
    qsizetype maxTotalIndexBytes = 128 * 1024 * 1024;
    std::size_t concurrentRefreshes = 2;
    std::size_t concurrentResolutions = 2;
    std::chrono::milliseconds collectionDeadline = std::chrono::seconds(10);
    std::chrono::milliseconds resolutionTimeout = std::chrono::seconds(10);
    std::chrono::milliseconds lease = std::chrono::seconds(90);
    std::chrono::milliseconds freshness = std::chrono::seconds(30);
    std::chrono::milliseconds pollInterval = std::chrono::seconds(15);
    // Expensive repositories poll less often: the interval is at least the last refresh's wall time
    // times this factor (a duty cycle of about 1/factor), up to maxAdaptivePoll. 0 disables it.
    int pollCostFactor = 50;
    std::chrono::milliseconds maxAdaptivePoll = std::chrono::seconds(60);
    std::chrono::milliseconds debounce = std::chrono::milliseconds(250);
    std::chrono::milliseconds housekeepingInterval = std::chrono::seconds(10);
    // Failed refreshes poll at pollInterval << min(failures, backoffShiftLimit), capped.
    int backoffShiftLimit = 3;
    std::chrono::milliseconds maxBackoff = std::chrono::seconds(120);
    qint64 maxOutputBytes = 16 * 1024 * 1024;
    RepositoryStatusIndexLimits index;
    // Replaceable monotonic time source for deterministic tests.
    std::function<Clock::time_point()> clock;
};

struct OverlayProviderOptions
{
    bool recursive = true;
};

// Collects repository status outside Dolphin and answers per-path overlay requests. Requests return
// at once; initial records follow on the next event-loop turn and later changes arrive through
// overlaysChanged(). Git runs once per repository refresh (one status plus one inventory
// command), never per path. Paths, repository contents, and Git output are never logged.
class OverlayStatusProvider final : public QObject
{
    Q_OBJECT

  public:
    using Clock = RepositoryStatusIndex::Clock;

    explicit OverlayStatusProvider(QObject* parent = nullptr);
    explicit OverlayStatusProvider(const OverlayProviderLimits& limits,
                                   const OverlayProviderOptions& options = {},
                                   QObject* parent = nullptr);
    ~OverlayStatusProvider() override;

    OverlayStatusProvider(const OverlayStatusProvider&) = delete;
    OverlayStatusProvider& operator=(const OverlayStatusProvider&) = delete;
    OverlayStatusProvider(OverlayStatusProvider&&) = delete;
    OverlayStatusProvider& operator=(OverlayStatusProvider&&) = delete;

    // `owner` identifies the caller (its unique session-bus name). `requestId` must be nonzero and
    // unique among the owner's requests whose initial records are still queued.
    [[nodiscard]] OverlayRequestResult requestOverlays(const QString& owner,
                                                       const QStringList& paths, quint64 requestId);
    void releaseOverlays(const QString& owner, const QStringList& paths);
    void releaseOwner(const QString& owner);
    // Accepted only for a repository that `owner` observes. Invalidates its answers at once and
    // queues a debounced refresh; never waits for Git.
    [[nodiscard]] bool refreshRepository(const QString& owner, const QString& repositoryRoot);
    // Drops expired leases and stops work for repositories nobody observes. Also run by a timer.
    void expireLeases();

    [[nodiscard]] quint64 generation() const;
    [[nodiscard]] qsizetype observationCount() const;
    [[nodiscard]] qsizetype repositoryCount() const;
    [[nodiscard]] qsizetype maxRecordsPerMessage() const;

  Q_SIGNALS:
    void initialResults(const QString& owner, quint64 requestId,
                        const QList<LinuxGitShell::OverlayRecord>& records, quint64 generation);
    void overlaysChanged(const QString& owner, const QList<LinuxGitShell::OverlayRecord>& records,
                         quint64 generation);

  private:
    enum class Resolution
    {
        Unresolved,
        Queued,
        Resolved,
        Unavailable,
        Failed,
    };

    struct Observation
    {
        Clock::time_point expires;
        Clock::time_point resolvedAt;
        Resolution resolution = Resolution::Unresolved;
        QString repositoryRoot;
        QString relativeKey;
    };

    class Collector final : public QObject
    {
      public:
        GitStatusReader statusReader;
        GitInventoryReader inventoryReader;
        QTimer deadline;
        QElapsedTimer elapsed;
        std::optional<GitStatusSnapshot> status;
        QSet<QString> boundaries;
        QByteArray statusDigest;
        quint64 invalidationsAtStart = 0;
        bool timedOut = false;
    };

    struct Repository
    {
        explicit Repository(const QString& repositoryRoot,
                            const RepositoryStatusIndexLimits& indexLimits,
                            const RepositoryStatusIndexOptions& indexOptions)
            : root(repositoryRoot), index(repositoryRoot, indexLimits, indexOptions)
        {
        }

        QString root;
        RepositoryStatusIndex index;
        QSet<QString> excludedRoots;
        Clock::time_point capturedAt;
        Clock::time_point lastUsed;
        // Bare, excluded, sparse, or otherwise unsupported: always answers Unavailable.
        bool unavailable = false;
        bool hasSnapshot = false;
        bool queued = false;
        bool refreshing = false;
        bool followUp = false;
        int failures = 0;
        std::chrono::milliseconds lastRefreshCost{0};
        quint64 invalidations = 0;
        // Digests of the Git status output (with boundaries) and of the tracked inventory behind
        // the stored snapshot. An identical refresh only renews freshness instead of re-reading
        // the inventory and rebuilding the index.
        QByteArray statusDigest;
        QByteArray inventoryDigest;
        QTimer poll;
        QTimer debounce;
        std::unique_ptr<Collector> collector;
    };

    struct ResolutionWorker
    {
        std::unique_ptr<RepositoryDiscovery> discovery;
        QTimer timeout;
        QString path;
        QString canonicalPath;
        bool isDirectory = false;
        bool timedOut = false;
    };

    struct QueuedInitial
    {
        QString owner;
        quint64 requestId = 0;
        QStringList paths;
    };

    [[nodiscard]] Clock::time_point now() const;
    [[nodiscard]] quint64 nextRevision();
    [[nodiscard]] OverlayRecord buildRecord(const QString& path, const Observation& observation);
    [[nodiscard]] Repository* repository(const QString& root) const;
    [[nodiscard]] Repository* findKnownRepository(const QString& path) const;
    [[nodiscard]] Repository* addRepository(const QString& root, bool unavailable);
    [[nodiscard]] bool evictIdleRepository();
    void destroyRepository(const QString& root);
    void attach(Observation& observation, Repository& repo, const QString& relativeKey);
    void resolveObservation(const QString& path, Observation& observation);
    void queueResolution(const QString& path);
    void startResolutions();
    void resolutionFinished(ResolutionWorker& worker, const RepositoryDiscoveryResult& result);
    void settlePath(const QString& path, Resolution resolution, const QString& repositoryRoot,
                    const QString& relativeKey);
    void attachUnresolved();
    void reevaluateBoundaries(Repository& repo);
    void emitForPath(const QString& path);
    void publishRepository(Repository& repo);
    void emitRecords(const QHash<QString, QList<OverlayRecord>>& byOwner);
    void queueInitial(const QString& owner, quint64 requestId, const QStringList& paths);
    void flushInitial();
    void ensureFresh(Repository& repo);
    void scheduleRefresh(Repository& repo, std::chrono::milliseconds delay);
    void enqueueRefresh(Repository& repo);
    void startRefreshes();
    void startRefresh(Repository& repo);
    void statusFinished(Repository& repo, const GitStatusResult& result);
    void inventoryFinished(Repository& repo, const GitInventoryResult& result);
    void completeRefresh(Repository& repo, const GitInventoryResult& result);
    void failRefresh(Repository& repo);
    void finishRefreshBookkeeping(Repository& repo);
    void disposeCollector(Repository& repo);
    void stopRepositoryWork(Repository& repo);
    void enforceIndexBudget(const Repository& current);
    [[nodiscard]] qsizetype observerCount(const QString& root) const;
    [[nodiscard]] std::chrono::milliseconds pollDelay(const Repository& repo) const;

    OverlayProviderLimits limits;
    OverlayProviderOptions options;
    quint64 currentGeneration = 0;
    quint64 revisionCounter = 0;
    QHash<QString, QHash<QString, Observation>> observations;
    qsizetype totalObservations = 0;
    QHash<QString, std::shared_ptr<Repository>> repositories;
    QList<QString> refreshQueue;
    std::size_t activeRefreshes = 0;
    QStringList resolutionQueue;
    QSet<QString> queuedResolutions;
    std::vector<std::unique_ptr<ResolutionWorker>> workers;
    QHash<QString, Clock::time_point> notRepositoryDirectories;
    QList<QueuedInitial> queuedInitial;
    QSet<QString> outstandingRequests;
    bool flushScheduled = false;
    bool resolutionScheduled = false;
    QTimer housekeeping;
};

} // namespace LinuxGitShell
