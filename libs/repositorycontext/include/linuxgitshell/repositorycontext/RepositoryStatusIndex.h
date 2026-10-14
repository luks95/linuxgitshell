// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <chrono>
#include <optional>

namespace LinuxGitShell
{

enum class RepositoryFileState
{
    Clean,
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    TypeChanged,
    Conflict,
    Untracked,
    Ignored,
};

enum class RepositoryStatusState
{
    Unknown,
    Clean,
    Ignored,
    Untracked,
    Deleted,
    Added,
    Modified,
    Conflict,
};

enum class RepositoryStatusValidity
{
    Ready,
    Pending,
    Stale,
    Degraded,
    Unavailable,
};

struct RepositoryStatusEntry
{
    QString path;
    QString originalPath;
    RepositoryFileState indexState = RepositoryFileState::Clean;
    RepositoryFileState workTreeState = RepositoryFileState::Clean;
    bool isDirectory = false;
    bool isSubmodule = false;
    bool submoduleCommitChanged = false;
    bool submoduleTrackedChanges = false;
    bool submoduleUntrackedChanges = false;
};

struct RepositoryStatusSnapshot
{
    quint64 revision = 0;
    std::chrono::steady_clock::time_point capturedAt;
    RepositoryStatusValidity validity = RepositoryStatusValidity::Pending;
    // The provider sets this only after all required collectors succeed without truncation.
    bool coverageComplete = false;
    QStringList trackedPaths;
    QList<RepositoryStatusEntry> entries;
    // Descendants belong to another index; the boundary's own parent-repository entry is retained.
    QStringList excludedRoots;
};

struct RepositoryStatusIndexLimits
{
    qsizetype maxRecords = 250000;
    qsizetype maxAccountedBytes = 64 * 1024 * 1024;
    std::chrono::steady_clock::duration freshness = std::chrono::seconds(30);
};

struct RepositoryStatusIndexOptions
{
    bool recursive = true;
    bool includeUntracked = true;
    bool includeIgnored = false;
};

enum class RepositoryStatusStoreResult
{
    Accepted,
    ObsoleteRevision,
    InvalidSnapshot,
    LimitExceeded,
};

struct RepositoryStatusLookup
{
    RepositoryStatusState state = RepositoryStatusState::Unknown;
    RepositoryStatusValidity validity = RepositoryStatusValidity::Unavailable;
    quint64 revision = 0;
    std::optional<RepositoryStatusEntry> entry;
};

// One repository's complete snapshot. Qt Core only: no Git, filesystem, IPC, or implicit clock.
// Paths are canonical relative keys (no '.', '..', empty components, or trailing slash).
// Only lookup accepts the empty key, for the repository root. Not thread-safe.
class RepositoryStatusIndex final
{
  public:
    using Clock = std::chrono::steady_clock;

    explicit RepositoryStatusIndex(const QString& repositoryRoot,
                                   const RepositoryStatusIndexLimits& limits = {},
                                   const RepositoryStatusIndexOptions& options = {});

    // Revisions are strictly increasing within this instance's daemon generation. Invalid newer
    // input clears the previous payload and records Degraded; obsolete replies leave it untouched.
    [[nodiscard]] RepositoryStatusStoreResult replace(const RepositoryStatusSnapshot& snapshot);
    [[nodiscard]] RepositoryStatusLookup lookup(const QString& relativePath,
                                                Clock::time_point now) const;
    [[nodiscard]] bool invalidate(RepositoryStatusValidity validity, quint64 revision);
    // Evict the whole snapshot, never individual records of a supposedly complete index.
    // Preserve the revision watermark so late replies cannot restore an evicted snapshot.
    void clear();

    [[nodiscard]] QString repositoryRoot() const;
    [[nodiscard]] quint64 revision() const;
    [[nodiscard]] qsizetype recordCount() const;
    [[nodiscard]] qsizetype accountedBytes() const;

  private:
    struct Node
    {
        bool tracked = false;
        bool directory = false;
        RepositoryStatusState aggregate = RepositoryStatusState::Clean;
        std::optional<RepositoryStatusEntry> entry;
    };

    [[nodiscard]] bool reserveStorage(qsizetype bytes, qsizetype records = 0);
    [[nodiscard]] bool ensureNode(const QString& path);
    [[nodiscard]] bool ensureAncestors(const QString& path);
    [[nodiscard]] bool belowBoundary(const QString& path) const;
    [[nodiscard]] RepositoryStatusState directState(const QString& path, const Node* node) const;
    void aggregateDirectories();
    void discard(RepositoryStatusValidity validity);
    [[nodiscard]] RepositoryStatusStoreResult build(const RepositoryStatusSnapshot& snapshot);

    QString root;
    RepositoryStatusIndexLimits limits;
    RepositoryStatusIndexOptions options;
    QHash<QString, Node> nodes;
    QSet<QString> boundaries;
    quint64 currentRevision = 0;
    Clock::time_point capturedAt;
    RepositoryStatusValidity currentValidity = RepositoryStatusValidity::Unavailable;
    qsizetype bytesUsed = 0;
    qsizetype recordsUsed = 0;
};

} // namespace LinuxGitShell
