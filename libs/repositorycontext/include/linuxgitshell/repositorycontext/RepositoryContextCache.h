// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "linuxgitshell/repositorycontext/RepositoryContextSnapshot.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <chrono>
#include <optional>

namespace LinuxGitShell
{

struct RepositoryContextCacheLimits
{
    qsizetype maxRepositories = 512;
    qsizetype maxPathAliases = 4096;
    std::chrono::steady_clock::duration repositoryFreshness = std::chrono::minutes(5);
    std::chrono::steady_clock::duration negativeFreshness = std::chrono::seconds(30);
};

enum class RepositoryContextFreshness
{
    Warm,
    Cold,
    Stale,
};

struct RepositoryContextLookup
{
    RepositoryContextFreshness freshness = RepositoryContextFreshness::Cold;
    // Present only for warm entries; cold and stale entries must fall back to generic actions.
    std::optional<RepositoryContextSnapshot> snapshot;
};

struct RepositoryContextSelectionLookup
{
    // Set only when every key is warm and inside the same repository.
    std::optional<QString> sharedRepositoryRoot;
    // Warm snapshots in selection order; complete only when refreshKeys is empty.
    QList<RepositoryContextSnapshot> snapshots;
    // Cold or stale keys that the client should refresh asynchronously.
    QStringList refreshKeys;
};

// Bounded path-to-repository snapshot cache. It performs no filesystem, Git, or IPC work, and takes
// explicit time points so that freshness is deterministic under test. Not thread-safe.
class RepositoryContextCache final
{
  public:
    using Clock = std::chrono::steady_clock;

    RepositoryContextCache();
    explicit RepositoryContextCache(const RepositoryContextCacheLimits& limits);

    [[nodiscard]] RepositoryContextLookup lookup(const QString& pathKey, Clock::time_point now);
    [[nodiscard]] RepositoryContextSelectionLookup lookupSelection(const QStringList& pathKeys,
                                                                   Clock::time_point now);

    // Rejects empty keys, inside-repository snapshots without a root, and replies whose generation
    // differs from the current non-zero service generation.
    [[nodiscard]] bool store(const QString& pathKey, const RepositoryContextSnapshot& snapshot,
                             quint64 generation, Clock::time_point now);

    // A different generation means the service restarted, so every snapshot is dropped.
    void setGeneration(quint64 generation);
    [[nodiscard]] quint64 generation() const;

    void invalidateRepository(const QString& repositoryRoot);
    void invalidateSubtree(const QString& pathKey);
    void clear();

    [[nodiscard]] qsizetype repositoryCount() const;
    [[nodiscard]] qsizetype aliasCount() const;

  private:
    struct RepositoryRecord
    {
        RepositoryContextType type = RepositoryContextType::Normal;
        QList<RepositoryContextOperation> operations;
        bool hasRemote = false;
        bool hasUpstream = false;
        quint64 lastUsed = 0;
    };

    struct AliasRecord
    {
        RepositoryContextState state = RepositoryContextState::Unavailable;
        QString repositoryRoot;
        bool isRepositoryRoot = false;
        Clock::time_point storedAt;
        quint64 lastUsed = 0;
    };

    [[nodiscard]] bool isFresh(const AliasRecord& alias, Clock::time_point now) const;
    void removeRepository(const QString& repositoryRoot);
    void removeRepositoryIfUnreferenced(const QString& repositoryRoot);
    void removeAlias(const QString& pathKey);
    void evictAliases();
    void evictRepositories();

    RepositoryContextCacheLimits limits;
    QHash<QString, RepositoryRecord> repositories;
    QHash<QString, AliasRecord> aliases;
    quint64 currentGeneration = 0;
    quint64 useCounter = 0;
};

} // namespace LinuxGitShell
