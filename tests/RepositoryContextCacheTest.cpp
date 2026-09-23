// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/repositorycontext/RepositoryContextCache.h"

#include <QTest>

#include <chrono>

using LinuxGitShell::RepositoryContextCache;
using LinuxGitShell::RepositoryContextCacheLimits;
using LinuxGitShell::RepositoryContextFreshness;
using LinuxGitShell::RepositoryContextOperation;
using LinuxGitShell::repositoryContextPathKey;
using LinuxGitShell::RepositoryContextSnapshot;
using LinuxGitShell::RepositoryContextState;
using LinuxGitShell::RepositoryContextType;

using namespace std::chrono_literals;

namespace
{

constexpr quint64 Generation = 7;
constexpr RepositoryContextCache::Clock::time_point Start{};

RepositoryContextSnapshot warmSnapshot(const LinuxGitShell::RepositoryContextLookup& lookup)
{
    return lookup.snapshot.value_or(RepositoryContextSnapshot{});
}

RepositoryContextSnapshot inside(const QString& root, bool isRoot = false,
                                 RepositoryContextType type = RepositoryContextType::Normal)
{
    RepositoryContextSnapshot snapshot;
    snapshot.state = RepositoryContextState::InsideRepository;
    snapshot.repositoryRoot = root;
    snapshot.type = type;
    snapshot.isRepositoryRoot = isRoot;
    return snapshot;
}

RepositoryContextSnapshot withState(RepositoryContextState state)
{
    RepositoryContextSnapshot snapshot;
    snapshot.state = state;
    return snapshot;
}

RepositoryContextCache readyCache(const RepositoryContextCacheLimits& limits = {})
{
    RepositoryContextCache cache(limits);
    cache.setGeneration(Generation);
    return cache;
}

} // namespace

class RepositoryContextCacheTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void pathKeyIsLexicalAndRejectsRelativePaths();
    void pathKeyPreservesUnusualNames();
    void unknownPathIsCold();
    void storedSnapshotIsWarm();
    void preservesRepositoryTypes();
    void rejectsInvalidStores();
    void positiveEntriesExpireAfterRepositoryFreshness();
    void negativeEntriesExpireAfterNegativeFreshness();
    void generationChangeDropsEverySnapshot();
    void latestRepositoryDataIsSharedByAliases();
    void movingAliasReleasesUnreferencedRepository();
    void evictsLeastRecentlyUsedAlias();
    void evictingRepositoryRemovesItsAliases();
    void invalidateRepositoryRemovesItsAliases();
    void invalidateSubtreeRemovesNestedEntriesOnly();
    void selectionSharesRepositoryOnlyWhenAllWarm();
    void selectionAcrossRepositoriesHasNoSharedRoot();
    void selectionOutsideRepositoryHasNoSharedRoot();
};

void RepositoryContextCacheTest::pathKeyIsLexicalAndRejectsRelativePaths()
{
    QCOMPARE(repositoryContextPathKey(QStringLiteral("/home/user/repo/")),
             QStringLiteral("/home/user/repo"));
    QCOMPARE(repositoryContextPathKey(QStringLiteral("/home/user/./repo/src/../file")),
             QStringLiteral("/home/user/repo/file"));
    QCOMPARE(repositoryContextPathKey(QStringLiteral("//home//user")),
             QStringLiteral("/home/user"));
    QCOMPARE(repositoryContextPathKey(QStringLiteral("/")), QStringLiteral("/"));
    QVERIFY(repositoryContextPathKey(QString()).isEmpty());
    QVERIFY(repositoryContextPathKey(QStringLiteral("relative/path")).isEmpty());
}

void RepositoryContextCacheTest::pathKeyPreservesUnusualNames()
{
    const QString path = QString::fromUtf8("/tmp/répôt with spaces/-file\nname.cpp");

    QCOMPARE(repositoryContextPathKey(path), path);
    QCOMPARE(repositoryContextPathKey(QStringLiteral("/tmp/Case")), QStringLiteral("/tmp/Case"));
    QVERIFY(repositoryContextPathKey(QStringLiteral("/tmp/Case")) !=
            repositoryContextPathKey(QStringLiteral("/tmp/case")));
}

void RepositoryContextCacheTest::unknownPathIsCold()
{
    RepositoryContextCache cache = readyCache();
    const auto lookup = cache.lookup(QStringLiteral("/repo"), Start);

    QCOMPARE(lookup.freshness, RepositoryContextFreshness::Cold);
    QVERIFY(!lookup.snapshot.has_value());
}

void RepositoryContextCacheTest::storedSnapshotIsWarm()
{
    RepositoryContextCache cache = readyCache();
    RepositoryContextSnapshot snapshot = inside(QStringLiteral("/repo"), true);
    snapshot.operations = {RepositoryContextOperation::Merge};
    snapshot.hasRemote = true;
    snapshot.hasUpstream = true;

    QVERIFY(cache.store(QStringLiteral("/repo"), snapshot, Generation, Start));
    const auto lookup = cache.lookup(QStringLiteral("/repo"), Start + 1s);

    QCOMPARE(lookup.freshness, RepositoryContextFreshness::Warm);
    QVERIFY(lookup.snapshot.has_value());
    QVERIFY(warmSnapshot(lookup) == snapshot);
}

void RepositoryContextCacheTest::preservesRepositoryTypes()
{
    RepositoryContextCache cache = readyCache();
    const QList<std::pair<QString, RepositoryContextType>> repositories{
        {QStringLiteral("/normal"), RepositoryContextType::Normal},
        {QStringLiteral("/bare.git"), RepositoryContextType::Bare},
        {QStringLiteral("/worktree"), RepositoryContextType::LinkedWorktree},
        {QStringLiteral("/normal/module"), RepositoryContextType::Submodule},
    };

    for (const auto& [root, type] : repositories)
    {
        QVERIFY(cache.store(root, inside(root, true, type), Generation, Start));
    }
    for (const auto& [root, type] : repositories)
    {
        const auto lookup = cache.lookup(root, Start);
        QVERIFY(lookup.snapshot.has_value());
        QCOMPARE(warmSnapshot(lookup).type, type);
        QCOMPARE(warmSnapshot(lookup).repositoryRoot, root);
    }
    QCOMPARE(cache.repositoryCount(), 4);
}

void RepositoryContextCacheTest::rejectsInvalidStores()
{
    RepositoryContextCache unset;
    QVERIFY(!unset.store(QStringLiteral("/repo"), inside(QStringLiteral("/repo")), 0, Start));

    RepositoryContextCache cache = readyCache();
    QVERIFY(!cache.store(QString(), inside(QStringLiteral("/repo")), Generation, Start));
    QVERIFY(!cache.store(QStringLiteral("/repo"), inside(QString()), Generation, Start));
    QVERIFY(!cache.store(QStringLiteral("/repo"), inside(QStringLiteral("/repo")), Generation + 1,
                         Start));
    QCOMPARE(cache.aliasCount(), 0);
    QCOMPARE(cache.repositoryCount(), 0);
}

void RepositoryContextCacheTest::positiveEntriesExpireAfterRepositoryFreshness()
{
    RepositoryContextCache cache = readyCache();
    QVERIFY(cache.store(QStringLiteral("/repo/file"), inside(QStringLiteral("/repo")), Generation,
                        Start));

    QCOMPARE(cache.lookup(QStringLiteral("/repo/file"), Start + 5min - 1s).freshness,
             RepositoryContextFreshness::Warm);

    const auto expired = cache.lookup(QStringLiteral("/repo/file"), Start + 5min);
    QCOMPARE(expired.freshness, RepositoryContextFreshness::Stale);
    QVERIFY(!expired.snapshot.has_value());
}

void RepositoryContextCacheTest::negativeEntriesExpireAfterNegativeFreshness()
{
    RepositoryContextCache cache = readyCache();
    const QList<RepositoryContextState> states{RepositoryContextState::OutsideRepository,
                                               RepositoryContextState::Unavailable,
                                               RepositoryContextState::DiscoveryError};

    for (const RepositoryContextState state : states)
    {
        QVERIFY(cache.store(QStringLiteral("/outside"), withState(state), Generation, Start));
        const auto warm = cache.lookup(QStringLiteral("/outside"), Start + 29s);
        QCOMPARE(warm.freshness, RepositoryContextFreshness::Warm);
        QCOMPARE(warmSnapshot(warm).state, state);
        QCOMPARE(cache.lookup(QStringLiteral("/outside"), Start + 30s).freshness,
                 RepositoryContextFreshness::Stale);
    }
    QCOMPARE(cache.repositoryCount(), 0);
}

void RepositoryContextCacheTest::generationChangeDropsEverySnapshot()
{
    RepositoryContextCache cache = readyCache();
    QVERIFY(
        cache.store(QStringLiteral("/repo"), inside(QStringLiteral("/repo")), Generation, Start));

    cache.setGeneration(Generation);
    QCOMPARE(cache.aliasCount(), 1);

    cache.setGeneration(Generation + 1);
    QCOMPARE(cache.generation(), Generation + 1);
    QCOMPARE(cache.aliasCount(), 0);
    QCOMPARE(cache.repositoryCount(), 0);
    QCOMPARE(cache.lookup(QStringLiteral("/repo"), Start).freshness,
             RepositoryContextFreshness::Cold);
    QVERIFY(
        !cache.store(QStringLiteral("/repo"), inside(QStringLiteral("/repo")), Generation, Start));
}

void RepositoryContextCacheTest::latestRepositoryDataIsSharedByAliases()
{
    RepositoryContextCache cache = readyCache();
    QVERIFY(cache.store(QStringLiteral("/repo"), inside(QStringLiteral("/repo"), true), Generation,
                        Start));

    RepositoryContextSnapshot rebasing = inside(QStringLiteral("/repo"));
    rebasing.operations = {RepositoryContextOperation::Rebase};
    QVERIFY(cache.store(QStringLiteral("/repo/src/file.cpp"), rebasing, Generation, Start + 1s));

    const auto root = cache.lookup(QStringLiteral("/repo"), Start + 2s);
    QVERIFY(root.snapshot.has_value());
    QVERIFY(warmSnapshot(root).isRepositoryRoot);
    QCOMPARE(warmSnapshot(root).operations, QList{RepositoryContextOperation::Rebase});

    const auto file = cache.lookup(QStringLiteral("/repo/src/file.cpp"), Start + 2s);
    QVERIFY(file.snapshot.has_value());
    QVERIFY(!warmSnapshot(file).isRepositoryRoot);
    QCOMPARE(cache.repositoryCount(), 1);
    QCOMPARE(cache.aliasCount(), 2);
}

void RepositoryContextCacheTest::movingAliasReleasesUnreferencedRepository()
{
    RepositoryContextCache cache = readyCache();
    QVERIFY(
        cache.store(QStringLiteral("/work"), inside(QStringLiteral("/old")), Generation, Start));
    QVERIFY(cache.store(QStringLiteral("/work"),
                        withState(RepositoryContextState::OutsideRepository), Generation, Start));

    QCOMPARE(cache.repositoryCount(), 0);
    QCOMPARE(warmSnapshot(cache.lookup(QStringLiteral("/work"), Start)).state,
             RepositoryContextState::OutsideRepository);
}

void RepositoryContextCacheTest::evictsLeastRecentlyUsedAlias()
{
    RepositoryContextCacheLimits limits;
    limits.maxPathAliases = 2;
    RepositoryContextCache cache = readyCache(limits);

    QVERIFY(cache.store(QStringLiteral("/a"), inside(QStringLiteral("/a")), Generation, Start));
    QVERIFY(cache.store(QStringLiteral("/b"), inside(QStringLiteral("/b")), Generation, Start));
    QVERIFY(cache.lookup(QStringLiteral("/a"), Start).snapshot.has_value());
    QVERIFY(cache.store(QStringLiteral("/c"), inside(QStringLiteral("/c")), Generation, Start));

    QCOMPARE(cache.aliasCount(), 2);
    QCOMPARE(cache.repositoryCount(), 2);
    QCOMPARE(cache.lookup(QStringLiteral("/a"), Start).freshness, RepositoryContextFreshness::Warm);
    QCOMPARE(cache.lookup(QStringLiteral("/b"), Start).freshness, RepositoryContextFreshness::Cold);
    QCOMPARE(cache.lookup(QStringLiteral("/c"), Start).freshness, RepositoryContextFreshness::Warm);
}

void RepositoryContextCacheTest::evictingRepositoryRemovesItsAliases()
{
    RepositoryContextCacheLimits limits;
    limits.maxRepositories = 1;
    RepositoryContextCache cache = readyCache(limits);

    QVERIFY(cache.store(QStringLiteral("/a"), inside(QStringLiteral("/a")), Generation, Start));
    QVERIFY(
        cache.store(QStringLiteral("/a/file"), inside(QStringLiteral("/a")), Generation, Start));
    QVERIFY(cache.store(QStringLiteral("/b"), inside(QStringLiteral("/b")), Generation, Start));

    QCOMPARE(cache.repositoryCount(), 1);
    QCOMPARE(cache.aliasCount(), 1);
    QCOMPARE(cache.lookup(QStringLiteral("/a/file"), Start).freshness,
             RepositoryContextFreshness::Cold);
    QCOMPARE(cache.lookup(QStringLiteral("/b"), Start).freshness, RepositoryContextFreshness::Warm);
}

void RepositoryContextCacheTest::invalidateRepositoryRemovesItsAliases()
{
    RepositoryContextCache cache = readyCache();
    QVERIFY(cache.store(QStringLiteral("/a"), inside(QStringLiteral("/a")), Generation, Start));
    QVERIFY(
        cache.store(QStringLiteral("/a/file"), inside(QStringLiteral("/a")), Generation, Start));
    QVERIFY(cache.store(QStringLiteral("/b"), inside(QStringLiteral("/b")), Generation, Start));

    cache.invalidateRepository(QStringLiteral("/a"));

    QCOMPARE(cache.repositoryCount(), 1);
    QCOMPARE(cache.aliasCount(), 1);
    QCOMPARE(cache.lookup(QStringLiteral("/b"), Start).freshness, RepositoryContextFreshness::Warm);
}

void RepositoryContextCacheTest::invalidateSubtreeRemovesNestedEntriesOnly()
{
    RepositoryContextCache cache = readyCache();
    // A worktree outside the invalidated subtree whose alias lives inside it.
    QVERIFY(cache.store(
        QStringLiteral("/home/user/projects/link"),
        inside(QStringLiteral("/srv/worktree"), false, RepositoryContextType::LinkedWorktree),
        Generation, Start));
    // A repository inside the subtree with an alias outside it.
    QVERIFY(cache.store(QStringLiteral("/home/user/projects/app"),
                        inside(QStringLiteral("/home/user/projects/app"), true), Generation,
                        Start));
    QVERIFY(cache.store(QStringLiteral("/mnt/alias"),
                        inside(QStringLiteral("/home/user/projects/app")), Generation, Start));
    // Siblings sharing a textual prefix must survive.
    QVERIFY(cache.store(QStringLiteral("/home/user/projects-old"),
                        withState(RepositoryContextState::OutsideRepository), Generation, Start));

    cache.invalidateSubtree(QStringLiteral("/home/user/projects"));

    QCOMPARE(cache.aliasCount(), 1);
    QCOMPARE(cache.repositoryCount(), 0);
    QCOMPARE(cache.lookup(QStringLiteral("/home/user/projects-old"), Start).freshness,
             RepositoryContextFreshness::Warm);
    QCOMPARE(cache.lookup(QStringLiteral("/mnt/alias"), Start).freshness,
             RepositoryContextFreshness::Cold);
}

void RepositoryContextCacheTest::selectionSharesRepositoryOnlyWhenAllWarm()
{
    RepositoryContextCache cache = readyCache();
    const QStringList keys{QStringLiteral("/repo/a"), QStringLiteral("/repo/b")};
    QVERIFY(cache.store(keys.at(0), inside(QStringLiteral("/repo")), Generation, Start));

    const auto partial = cache.lookupSelection(keys, Start);
    QVERIFY(!partial.sharedRepositoryRoot.has_value());
    QCOMPARE(partial.refreshKeys, QStringList{keys.at(1)});

    QVERIFY(cache.store(keys.at(1), inside(QStringLiteral("/repo")), Generation, Start));
    const auto complete = cache.lookupSelection(keys, Start);
    QCOMPARE(complete.sharedRepositoryRoot, std::optional{QStringLiteral("/repo")});
    QCOMPARE(complete.snapshots.size(), 2);
    QVERIFY(complete.refreshKeys.isEmpty());

    const auto expired = cache.lookupSelection(keys + keys, Start + 5min);
    QVERIFY(!expired.sharedRepositoryRoot.has_value());
    QCOMPARE(expired.refreshKeys, keys);

    QVERIFY(!cache.lookupSelection({}, Start).sharedRepositoryRoot.has_value());
}

void RepositoryContextCacheTest::selectionAcrossRepositoriesHasNoSharedRoot()
{
    RepositoryContextCache cache = readyCache();
    QVERIFY(cache.store(QStringLiteral("/repo/file"), inside(QStringLiteral("/repo")), Generation,
                        Start));
    QVERIFY(
        cache.store(QStringLiteral("/repo/module/file"),
                    inside(QStringLiteral("/repo/module"), false, RepositoryContextType::Submodule),
                    Generation, Start));

    const auto lookup = cache.lookupSelection(
        {QStringLiteral("/repo/file"), QStringLiteral("/repo/module/file")}, Start);

    QVERIFY(!lookup.sharedRepositoryRoot.has_value());
    QCOMPARE(lookup.snapshots.size(), 2);
    QVERIFY(lookup.refreshKeys.isEmpty());
}

void RepositoryContextCacheTest::selectionOutsideRepositoryHasNoSharedRoot()
{
    RepositoryContextCache cache = readyCache();
    QVERIFY(cache.store(QStringLiteral("/repo/file"), inside(QStringLiteral("/repo")), Generation,
                        Start));
    QVERIFY(cache.store(QStringLiteral("/tmp/file"),
                        withState(RepositoryContextState::OutsideRepository), Generation, Start));

    const auto lookup =
        cache.lookupSelection({QStringLiteral("/repo/file"), QStringLiteral("/tmp/file")}, Start);

    QVERIFY(!lookup.sharedRepositoryRoot.has_value());
    QVERIFY(lookup.refreshKeys.isEmpty());
}

QTEST_GUILESS_MAIN(RepositoryContextCacheTest)

#include "RepositoryContextCacheTest.moc"
