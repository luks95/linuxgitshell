// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/repositorycontext/RepositoryContextCache.h"

namespace LinuxGitShell
{

namespace
{

template <typename Hash> QString leastRecentlyUsedKey(const Hash& hash)
{
    QString key;
    quint64 oldest = 0;
    bool found = false;
    for (auto it = hash.cbegin(); it != hash.cend(); ++it)
    {
        if (!found || it.value().lastUsed < oldest)
        {
            key = it.key();
            oldest = it.value().lastUsed;
            found = true;
        }
    }
    return key;
}

} // namespace

RepositoryContextCache::RepositoryContextCache()
    : RepositoryContextCache(RepositoryContextCacheLimits{})
{
}

RepositoryContextCache::RepositoryContextCache(const RepositoryContextCacheLimits& limits)
    : limits(limits)
{
}

RepositoryContextLookup RepositoryContextCache::lookup(const QString& pathKey,
                                                       Clock::time_point now)
{
    const auto aliasIt = aliases.find(pathKey);
    if (aliasIt == aliases.end())
    {
        return {};
    }

    AliasRecord& alias = aliasIt.value();
    if (!isFresh(alias, now))
    {
        return {RepositoryContextFreshness::Stale, std::nullopt};
    }

    RepositoryContextSnapshot snapshot;
    snapshot.state = alias.state;
    alias.lastUsed = ++useCounter;

    if (alias.state == RepositoryContextState::InsideRepository)
    {
        const auto repositoryIt = repositories.find(alias.repositoryRoot);
        if (repositoryIt == repositories.end())
        {
            return {};
        }

        RepositoryRecord& repository = repositoryIt.value();
        repository.lastUsed = useCounter;
        snapshot.repositoryRoot = alias.repositoryRoot;
        snapshot.type = repository.type;
        snapshot.operations = repository.operations;
        snapshot.isRepositoryRoot = alias.isRepositoryRoot;
        snapshot.hasRemote = repository.hasRemote;
        snapshot.hasUpstream = repository.hasUpstream;
    }

    return {RepositoryContextFreshness::Warm, snapshot};
}

RepositoryContextSelectionLookup
RepositoryContextCache::lookupSelection(const QStringList& pathKeys, Clock::time_point now)
{
    RepositoryContextSelectionLookup result;
    std::optional<QString> sharedRoot;
    bool compatible = !pathKeys.isEmpty();

    for (const QString& pathKey : pathKeys)
    {
        const RepositoryContextLookup entry = lookup(pathKey, now);
        if (!entry.snapshot.has_value())
        {
            compatible = false;
            if (!result.refreshKeys.contains(pathKey))
            {
                result.refreshKeys.append(pathKey);
            }
            continue;
        }

        const RepositoryContextSnapshot& snapshot = entry.snapshot.value();
        if (!sharedRoot.has_value())
        {
            sharedRoot = snapshot.repositoryRoot;
        }
        if (snapshot.state != RepositoryContextState::InsideRepository ||
            sharedRoot.value() != snapshot.repositoryRoot)
        {
            compatible = false;
        }
        result.snapshots.append(snapshot);
    }

    if (compatible)
    {
        result.sharedRepositoryRoot = sharedRoot;
    }
    return result;
}

bool RepositoryContextCache::store(const QString& pathKey,
                                   const RepositoryContextSnapshot& snapshot, quint64 generation,
                                   Clock::time_point now)
{
    if (pathKey.isEmpty() || generation == 0 || generation != currentGeneration)
    {
        return false;
    }

    const bool inside = snapshot.state == RepositoryContextState::InsideRepository;
    if (inside && snapshot.repositoryRoot.isEmpty())
    {
        return false;
    }

    const QString previousRoot = aliases.value(pathKey).repositoryRoot;
    const quint64 used = ++useCounter;

    AliasRecord alias;
    alias.state = snapshot.state;
    alias.storedAt = now;
    alias.lastUsed = used;
    if (inside)
    {
        alias.repositoryRoot = snapshot.repositoryRoot;
        alias.isRepositoryRoot = snapshot.isRepositoryRoot;

        RepositoryRecord repository;
        repository.type = snapshot.type;
        repository.operations = snapshot.operations;
        repository.hasRemote = snapshot.hasRemote;
        repository.hasUpstream = snapshot.hasUpstream;
        repository.lastUsed = used;
        repositories.insert(snapshot.repositoryRoot, repository);
    }
    aliases.insert(pathKey, alias);

    if (!previousRoot.isEmpty() && previousRoot != alias.repositoryRoot)
    {
        removeRepositoryIfUnreferenced(previousRoot);
    }

    evictRepositories();
    evictAliases();
    return true;
}

void RepositoryContextCache::setGeneration(quint64 generation)
{
    if (generation != currentGeneration)
    {
        clear();
        currentGeneration = generation;
    }
}

quint64 RepositoryContextCache::generation() const { return currentGeneration; }

void RepositoryContextCache::invalidateRepository(const QString& repositoryRoot)
{
    removeRepository(repositoryRoot);
}

void RepositoryContextCache::invalidateSubtree(const QString& pathKey)
{
    if (pathKey.isEmpty())
    {
        return;
    }

    const QString prefix =
        pathKey.endsWith(QLatin1Char('/')) ? pathKey : pathKey + QLatin1Char('/');
    const auto inSubtree = [&pathKey, &prefix](const QString& key)
    { return key == pathKey || key.startsWith(prefix); };

    QStringList removedRoots;
    for (auto it = repositories.cbegin(); it != repositories.cend(); ++it)
    {
        if (inSubtree(it.key()))
        {
            removedRoots.append(it.key());
        }
    }
    for (const QString& root : std::as_const(removedRoots))
    {
        removeRepository(root);
    }

    QStringList referencedRoots;
    aliases.removeIf(
        [&inSubtree, &referencedRoots](const auto& entry)
        {
            if (!inSubtree(entry.key()))
            {
                return false;
            }
            if (!entry.value().repositoryRoot.isEmpty())
            {
                referencedRoots.append(entry.value().repositoryRoot);
            }
            return true;
        });
    for (const QString& root : std::as_const(referencedRoots))
    {
        removeRepositoryIfUnreferenced(root);
    }
}

void RepositoryContextCache::clear()
{
    repositories.clear();
    aliases.clear();
}

qsizetype RepositoryContextCache::repositoryCount() const { return repositories.size(); }

qsizetype RepositoryContextCache::aliasCount() const { return aliases.size(); }

bool RepositoryContextCache::isFresh(const AliasRecord& alias, Clock::time_point now) const
{
    const Clock::duration freshness = alias.state == RepositoryContextState::InsideRepository
                                          ? limits.repositoryFreshness
                                          : limits.negativeFreshness;
    return now - alias.storedAt < freshness;
}

void RepositoryContextCache::removeRepository(const QString& repositoryRoot)
{
    repositories.remove(repositoryRoot);
    aliases.removeIf([&repositoryRoot](const auto& entry)
                     { return entry.value().repositoryRoot == repositoryRoot; });
}

void RepositoryContextCache::removeRepositoryIfUnreferenced(const QString& repositoryRoot)
{
    for (auto it = aliases.cbegin(); it != aliases.cend(); ++it)
    {
        if (it.value().repositoryRoot == repositoryRoot)
        {
            return;
        }
    }
    repositories.remove(repositoryRoot);
}

void RepositoryContextCache::removeAlias(const QString& pathKey)
{
    const QString repositoryRoot = aliases.take(pathKey).repositoryRoot;
    if (!repositoryRoot.isEmpty())
    {
        removeRepositoryIfUnreferenced(repositoryRoot);
    }
}

void RepositoryContextCache::evictAliases()
{
    while (aliases.size() > limits.maxPathAliases && !aliases.isEmpty())
    {
        removeAlias(leastRecentlyUsedKey(aliases));
    }
}

void RepositoryContextCache::evictRepositories()
{
    while (repositories.size() > limits.maxRepositories && !repositories.isEmpty())
    {
        removeRepository(leastRecentlyUsedKey(repositories));
    }
}

} // namespace LinuxGitShell
