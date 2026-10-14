// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/repositorycontext/RepositoryStatusIndex.h"

#include <QTest>

#include <chrono>

using namespace LinuxGitShell;
using namespace std::chrono_literals;

namespace
{
using State = RepositoryStatusState;
using FileState = RepositoryFileState;
using Validity = RepositoryStatusValidity;
using StoreResult = RepositoryStatusStoreResult;
constexpr RepositoryStatusIndex::Clock::time_point Start{};

RepositoryStatusSnapshot complete(quint64 revision = 1)
{
    RepositoryStatusSnapshot snapshot;
    snapshot.revision = revision;
    snapshot.capturedAt = Start;
    snapshot.validity = Validity::Ready;
    snapshot.coverageComplete = true;
    return snapshot;
}

// Fixture arguments intentionally follow Git's index/work-tree (XY) order.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
RepositoryStatusEntry entry(const QString& path, FileState index,
                            FileState workTree = FileState::Clean, bool directory = false)
{
    RepositoryStatusEntry result;
    result.path = path;
    result.indexState = index;
    result.workTreeState = workTree;
    result.isDirectory = directory;
    return result;
}

State stateAt(const RepositoryStatusIndex& index, const QString& path)
{
    return index.lookup(path, Start).state;
}
} // namespace

class RepositoryStatusIndexTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void needsCompleteEvidenceForClean();
    void emptyRepositoryRootIsClean();
    void preservesCombinedStates_data();
    void preservesCombinedStates();
    void precedenceIsIndependentOfEntryOrder();
    void ignoredPrefixesRespectTrackedExceptions();
    void directoryOptions_data();
    void directoryOptions();
    void disabledRecursionRetainsExplicitStates();
    void renameReplacesBothAncestries();
    void submoduleFlags_data();
    void submoduleFlags();
    void nestedOwnershipUsesComponentBoundaries();
    void preservesUnusualPaths();
    void rejectsInvalidPaths_data();
    void rejectsInvalidPaths();
    void rejectsMalformedEntries();
    void freshnessIsDeterministic();
    void nonReadySnapshotsClearDetails_data();
    void nonReadySnapshotsClearDetails();
    void revisionsPreventLateResurrection();
    void boundsDiscardWholeSnapshot();
    void accountsBoundariesDetailsAndAncestors();
    void duplicateInventoryIsDeduplicated();
    void handlesDeepPathsWithoutRecursion();
};

void RepositoryStatusIndexTest::needsCompleteEvidenceForClean()
{
    RepositoryStatusIndex index(QStringLiteral("/repo"));
    QCOMPARE(stateAt(index, QStringLiteral("clean.txt")), State::Unknown);
    auto snapshot = complete();
    snapshot.trackedPaths = {QStringLiteral("src/clean.txt")};
    snapshot.entries = {entry(QStringLiteral("new.txt"), FileState::Untracked)};
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(stateAt(index, QStringLiteral("src/clean.txt")), State::Clean);
    QCOMPARE(stateAt(index, QStringLiteral("src")), State::Clean);
    QCOMPARE(stateAt(index, QStringLiteral("new.txt")), State::Untracked);
    QCOMPARE(stateAt(index, QStringLiteral("missing.txt")), State::Unknown);
    QCOMPARE(stateAt(index, QStringLiteral("missing-dir")), State::Unknown);
    snapshot.revision = 2;
    snapshot.coverageComplete = false;
    QCOMPARE(index.replace(snapshot), StoreResult::InvalidSnapshot);
    QCOMPARE(index.lookup(QStringLiteral("src/clean.txt"), Start).validity, Validity::Degraded);
    QCOMPARE(stateAt(index, QStringLiteral("src/clean.txt")), State::Unknown);
    QCOMPARE(index.recordCount(), 0);
}

void RepositoryStatusIndexTest::emptyRepositoryRootIsClean()
{
    RepositoryStatusIndex index(QStringLiteral("/repo/./"));
    QCOMPARE(index.repositoryRoot(), QStringLiteral("/repo"));
    QCOMPARE(index.replace(complete()), StoreResult::Accepted);
    QCOMPARE(stateAt(index, QString()), State::Clean);
    QCOMPARE(stateAt(index, QStringLiteral("anything")), State::Unknown);
    RepositoryStatusIndex invalidRoot(QStringLiteral("relative"));
    QCOMPARE(invalidRoot.replace(complete()), StoreResult::InvalidSnapshot);
}

void RepositoryStatusIndexTest::preservesCombinedStates_data()
{
    QTest::addColumn<FileState>("indexState");
    QTest::addColumn<FileState>("workTreeState");
    QTest::addColumn<State>("expected");
    QTest::newRow("modified") << FileState::Clean << FileState::Modified << State::Modified;
    QTest::newRow("staged") << FileState::Modified << FileState::Clean << State::Modified;
    QTest::newRow("added-modified") << FileState::Added << FileState::Modified << State::Modified;
    QTest::newRow("added-deleted") << FileState::Added << FileState::Deleted << State::Added;
    QTest::newRow("deleted-untracked")
        << FileState::Deleted << FileState::Untracked << State::Deleted;
    QTest::newRow("rename") << FileState::Renamed << FileState::Clean << State::Modified;
    QTest::newRow("copy") << FileState::Copied << FileState::Clean << State::Added;
    QTest::newRow("type-change") << FileState::Clean << FileState::TypeChanged << State::Modified;
    QTest::newRow("conflict") << FileState::Conflict << FileState::Modified << State::Conflict;
    QTest::newRow("conflict-reverse")
        << FileState::Modified << FileState::Conflict << State::Conflict;
    QTest::newRow("ignored") << FileState::Ignored << FileState::Clean << State::Ignored;
}

void RepositoryStatusIndexTest::preservesCombinedStates()
{
    QFETCH(FileState, indexState);
    QFETCH(FileState, workTreeState);
    QFETCH(State, expected);
    RepositoryStatusIndex index(QStringLiteral("/repo"));
    auto snapshot = complete();
    auto detail = entry(QStringLiteral("dest/file"), indexState, workTreeState);
    detail.originalPath = QStringLiteral("source/file");
    snapshot.entries = {detail};
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    const auto result = index.lookup(detail.path, Start);
    QCOMPARE(result.state, expected);
    QVERIFY(result.entry.has_value());
    const auto stored = result.entry.value_or(RepositoryStatusEntry{});
    QCOMPARE(stored.indexState, indexState);
    QCOMPARE(stored.workTreeState, workTreeState);
    QCOMPARE(stored.originalPath, detail.originalPath);
}

void RepositoryStatusIndexTest::precedenceIsIndependentOfEntryOrder()
{
    const QList<FileState> states{FileState::Ignored, FileState::Untracked, FileState::Deleted,
                                  FileState::Added,   FileState::Modified,  FileState::Conflict};
    const QList<State> expected{State::Ignored, State::Untracked, State::Deleted,
                                State::Added,   State::Modified,  State::Conflict};
    RepositoryStatusIndexOptions options;
    options.includeIgnored = true;
    for (qsizetype strongest = 0; strongest < states.size(); ++strongest)
    {
        for (bool reverse : {false, true})
        {
            RepositoryStatusIndex index(QStringLiteral("/repo"), {}, options);
            auto snapshot = complete();
            snapshot.trackedPaths = {QStringLiteral("dir/clean")};
            for (qsizetype i = 0; i <= strongest; ++i)
            {
                const auto record = entry(QStringLiteral("dir/file%1").arg(i), states[i]);
                if (reverse)
                {
                    snapshot.entries.prepend(record);
                }
                else
                {
                    snapshot.entries.append(record);
                }
            }
            QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
            QCOMPARE(stateAt(index, QStringLiteral("dir")), expected[strongest]);
            QCOMPARE(stateAt(index, QString()), expected[strongest]);
        }
    }
}

void RepositoryStatusIndexTest::ignoredPrefixesRespectTrackedExceptions()
{
    RepositoryStatusIndex index(QStringLiteral("/repo"));
    auto snapshot = complete();
    snapshot.trackedPaths = {QStringLiteral("build/keep"), QStringLiteral("build/modified")};
    snapshot.entries = {entry(QStringLiteral("build"), FileState::Ignored, FileState::Clean, true),
                        entry(QStringLiteral("build/modified"), FileState::Modified)};
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(stateAt(index, QStringLiteral("build/keep")), State::Clean);
    QCOMPARE(stateAt(index, QStringLiteral("build/modified")), State::Modified);
    QCOMPARE(stateAt(index, QStringLiteral("build/unlisted/deep")), State::Ignored);
    QCOMPARE(stateAt(index, QStringLiteral("build2/unlisted")), State::Unknown);
    QCOMPARE(stateAt(index, QStringLiteral("build")), State::Modified);
    QCOMPARE(stateAt(index, QString()), State::Modified);
}

void RepositoryStatusIndexTest::directoryOptions_data()
{
    QTest::addColumn<bool>("untracked");
    QTest::addColumn<bool>("ignored");
    QTest::addColumn<State>("newDirectory");
    QTest::addColumn<State>("ignoredDirectory");
    QTest::newRow("default") << true << false << State::Untracked << State::Clean;
    QTest::newRow("both") << true << true << State::Untracked << State::Ignored;
    QTest::newRow("neither") << false << false << State::Clean << State::Clean;
    QTest::newRow("ignored-only") << false << true << State::Clean << State::Ignored;
}

void RepositoryStatusIndexTest::directoryOptions()
{
    QFETCH(bool, untracked);
    QFETCH(bool, ignored);
    QFETCH(State, newDirectory);
    QFETCH(State, ignoredDirectory);
    RepositoryStatusIndexOptions options;
    options.includeUntracked = untracked;
    options.includeIgnored = ignored;
    RepositoryStatusIndex index(QStringLiteral("/repo"), {}, options);
    auto snapshot = complete();
    snapshot.entries = {entry(QStringLiteral("new/file"), FileState::Untracked),
                        entry(QStringLiteral("output/file"), FileState::Ignored)};
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(stateAt(index, QStringLiteral("new")), newDirectory);
    QCOMPARE(stateAt(index, QStringLiteral("output")), ignoredDirectory);
    QCOMPARE(stateAt(index, QStringLiteral("new/file")), State::Untracked);
    QCOMPARE(stateAt(index, QStringLiteral("output/file")), State::Ignored);
}

void RepositoryStatusIndexTest::disabledRecursionRetainsExplicitStates()
{
    RepositoryStatusIndexOptions options;
    options.recursive = false;
    RepositoryStatusIndex index(QStringLiteral("/repo"), {}, options);
    auto snapshot = complete();
    snapshot.trackedPaths = {QStringLiteral("src/clean")};
    snapshot.entries = {
        entry(QStringLiteral("src/file"), FileState::Modified),
        entry(QStringLiteral("ignored"), FileState::Ignored, FileState::Clean, true),
        entry(QStringLiteral("new"), FileState::Untracked, FileState::Clean, true)};
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(stateAt(index, QStringLiteral("src")), State::Unknown);
    QCOMPARE(stateAt(index, QString()), State::Unknown);
    QCOMPARE(stateAt(index, QStringLiteral("src/clean")), State::Clean);
    QCOMPARE(stateAt(index, QStringLiteral("src/file")), State::Modified);
    QCOMPARE(stateAt(index, QStringLiteral("ignored")), State::Ignored);
    QCOMPARE(stateAt(index, QStringLiteral("new")), State::Untracked);
}

void RepositoryStatusIndexTest::renameReplacesBothAncestries()
{
    RepositoryStatusIndex index(QStringLiteral("/repo"));
    auto first = complete();
    first.trackedPaths = {QStringLiteral("old/file"), QStringLiteral("old/keep")};
    first.entries = {entry(QStringLiteral("old/file"), FileState::Modified)};
    QCOMPARE(index.replace(first), StoreResult::Accepted);
    QCOMPARE(stateAt(index, QStringLiteral("old")), State::Modified);
    auto second = complete(2);
    second.trackedPaths = {QStringLiteral("new/file"), QStringLiteral("old/keep")};
    auto renamed = entry(QStringLiteral("new/file"), FileState::Renamed);
    renamed.originalPath = QStringLiteral("old/file");
    second.entries = {renamed};
    QCOMPARE(index.replace(second), StoreResult::Accepted);
    QCOMPARE(stateAt(index, QStringLiteral("old/file")), State::Unknown);
    QCOMPARE(stateAt(index, QStringLiteral("old")), State::Clean);
    QCOMPARE(stateAt(index, QStringLiteral("new")), State::Modified);
    QCOMPARE(index.lookup(QStringLiteral("new/file"), Start)
                 .entry.value_or(RepositoryStatusEntry{})
                 .originalPath,
             renamed.originalPath);
    second.revision = 3;
    second.entries.clear();
    second.trackedPaths = {QStringLiteral("new/file")};
    QCOMPARE(index.replace(second), StoreResult::Accepted);
    QCOMPARE(stateAt(index, QStringLiteral("old")), State::Unknown);
    QCOMPARE(stateAt(index, QStringLiteral("new")), State::Clean);
    QCOMPARE(stateAt(index, QString()), State::Clean);
}

void RepositoryStatusIndexTest::submoduleFlags_data()
{
    QTest::addColumn<bool>("commit");
    QTest::addColumn<bool>("tracked");
    QTest::addColumn<bool>("untracked");
    QTest::addColumn<bool>("conflict");
    QTest::addColumn<State>("expected");
    QTest::newRow("clean") << false << false << false << false << State::Clean;
    QTest::newRow("commit") << true << false << false << false << State::Modified;
    QTest::newRow("tracked") << false << true << false << false << State::Modified;
    QTest::newRow("untracked") << false << false << true << false << State::Untracked;
    QTest::newRow("both") << false << true << true << false << State::Modified;
    QTest::newRow("conflict") << true << true << true << true << State::Conflict;
}

void RepositoryStatusIndexTest::submoduleFlags()
{
    QFETCH(bool, commit);
    QFETCH(bool, tracked);
    QFETCH(bool, untracked);
    QFETCH(bool, conflict);
    QFETCH(State, expected);
    RepositoryStatusIndex index(QStringLiteral("/repo"));
    auto snapshot = complete();
    auto module =
        entry(QStringLiteral("module"), conflict ? FileState::Conflict : FileState::Clean);
    module.isSubmodule = true;
    module.submoduleCommitChanged = commit;
    module.submoduleTrackedChanges = tracked;
    module.submoduleUntrackedChanges = untracked;
    // Real porcelain reports .M for untracked-only submodule contents (S..U).
    if (untracked)
    {
        module.workTreeState = FileState::Modified;
    }
    snapshot.entries = {module};
    snapshot.excludedRoots = {module.path};
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(stateAt(index, module.path), expected);
    QCOMPARE(stateAt(index, QStringLiteral("module/child")), State::Unknown);
    const auto result = index.lookup(module.path, Start);
    QVERIFY(result.entry.has_value());
    const auto stored = result.entry.value_or(RepositoryStatusEntry{});
    QCOMPARE(stored.submoduleUntrackedChanges, untracked);
    QCOMPARE(stored.workTreeState, module.workTreeState);
}

void RepositoryStatusIndexTest::nestedOwnershipUsesComponentBoundaries()
{
    RepositoryStatusIndex index(QStringLiteral("/repo"));
    auto snapshot = complete();
    snapshot.excludedRoots = {QStringLiteral("nested"), QStringLiteral("build/nested")};
    snapshot.trackedPaths = {QStringLiteral("nested/wrong"), QStringLiteral("nested2/right")};
    snapshot.entries = {entry(QStringLiteral("nested/wrong"), FileState::Conflict),
                        entry(QStringLiteral("build"), FileState::Ignored, FileState::Clean, true)};
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(stateAt(index, QStringLiteral("nested/wrong")), State::Unknown);
    QCOMPARE(stateAt(index, QStringLiteral("nested2/right")), State::Clean);
    QCOMPARE(stateAt(index, QStringLiteral("build/nested/child")), State::Unknown);
    QCOMPARE(stateAt(index, QStringLiteral("build/nested2/child")), State::Ignored);
    QCOMPARE(stateAt(index, QString()), State::Clean);
}

void RepositoryStatusIndexTest::preservesUnusualPaths()
{
    RepositoryStatusIndex index(QStringLiteral("/repo"));
    auto snapshot = complete();
    snapshot.trackedPaths = {QString::fromUtf8("dír with spaces/-file\nname"), QStringLiteral("A"),
                             QStringLiteral("a"), QStringLiteral("back\\slash")};
    snapshot.entries = {entry(QStringLiteral("A"), FileState::Deleted)};
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(stateAt(index, snapshot.trackedPaths[0]), State::Clean);
    QCOMPARE(stateAt(index, QStringLiteral("A")), State::Deleted);
    QCOMPARE(stateAt(index, QStringLiteral("a")), State::Clean);
    QCOMPARE(stateAt(index, QStringLiteral("back\\slash")), State::Clean);
}

void RepositoryStatusIndexTest::rejectsInvalidPaths_data()
{
    QTest::addColumn<QString>("path");
    QTest::newRow("empty") << QString();
    QTest::newRow("absolute") << QStringLiteral("/outside");
    QTest::newRow("escape") << QStringLiteral("../outside");
    QTest::newRow("internal-escape") << QStringLiteral("dir/../outside");
    QTest::newRow("dot") << QStringLiteral("dir/./file");
    QTest::newRow("double-slash") << QStringLiteral("dir//file");
    QTest::newRow("trailing-slash") << QStringLiteral("dir/");
    QTest::newRow("git-admin") << QStringLiteral("dir/.git/config");
    QTest::newRow("nul") << QString::fromUtf8("a\0b", 3);
}

void RepositoryStatusIndexTest::rejectsInvalidPaths()
{
    QFETCH(QString, path);
    for (int location = 0; location < 4; ++location)
    {
        RepositoryStatusIndex index(QStringLiteral("/repo"));
        auto snapshot = complete();
        snapshot.trackedPaths = {QStringLiteral("safe")};
        QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
        if (!path.isEmpty())
        {
            QCOMPARE(stateAt(index, path), State::Unknown);
        }
        snapshot.revision = 2;
        if (location == 0)
        {
            snapshot.trackedPaths.append(path);
        }
        else if (location == 1)
        {
            snapshot.entries = {entry(path, FileState::Modified)};
        }
        else if (location == 2)
        {
            auto renamed = entry(QStringLiteral("new"), FileState::Renamed);
            renamed.originalPath = path;
            snapshot.entries = {renamed};
        }
        else
        {
            snapshot.excludedRoots = {path};
        }
        QCOMPARE(index.replace(snapshot), StoreResult::InvalidSnapshot);
        QCOMPARE(stateAt(index, QStringLiteral("safe")), State::Unknown);
        QCOMPARE(index.recordCount(), 0);
        QCOMPARE(index.accountedBytes(), 0);
    }
}

void RepositoryStatusIndexTest::rejectsMalformedEntries()
{
    RepositoryStatusIndex index(QStringLiteral("/repo"));
    auto snapshot = complete();
    snapshot.entries = {entry(QStringLiteral("file"), static_cast<FileState>(999))};
    QCOMPARE(index.replace(snapshot), StoreResult::InvalidSnapshot);
    snapshot.revision++;
    snapshot.entries = {entry(QStringLiteral("file"), FileState::Modified),
                        entry(QStringLiteral("file"), FileState::Added)};
    QCOMPARE(index.replace(snapshot), StoreResult::InvalidSnapshot);
    snapshot.revision++;
    snapshot.entries = {entry(QStringLiteral("file"), FileState::Clean)};
    snapshot.entries[0].submoduleUntrackedChanges = true;
    QCOMPARE(index.replace(snapshot), StoreResult::InvalidSnapshot);
    snapshot.revision++;
    snapshot.validity = static_cast<Validity>(999);
    QCOMPARE(index.replace(snapshot), StoreResult::InvalidSnapshot);
}

void RepositoryStatusIndexTest::freshnessIsDeterministic()
{
    RepositoryStatusIndexLimits limits;
    limits.freshness = 10s;
    RepositoryStatusIndex index(QStringLiteral("/repo"), limits);
    auto snapshot = complete();
    snapshot.entries = {entry(QStringLiteral("file"), FileState::Modified)};
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(index.lookup(QStringLiteral("file"), Start + 9999ms).state, State::Modified);
    const auto expired = index.lookup(QStringLiteral("file"), Start + 10s);
    QCOMPARE(expired.state, State::Unknown);
    QCOMPARE(expired.validity, Validity::Stale);
    QVERIFY(!expired.entry.has_value());
    QCOMPARE(index.lookup(QStringLiteral("file"), Start - 1ms).validity, Validity::Stale);
    snapshot.revision = 2;
    snapshot.capturedAt = Start + 10s;
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(index.lookup(QStringLiteral("file"), Start + 10s).state, State::Modified);
    limits.freshness = 0s;
    RepositoryStatusIndex noFreshness(QStringLiteral("/repo"), limits);
    QCOMPARE(noFreshness.replace(complete()), StoreResult::Accepted);
    QCOMPARE(noFreshness.lookup(QString(), Start).validity, Validity::Stale);
}

void RepositoryStatusIndexTest::nonReadySnapshotsClearDetails_data()
{
    QTest::addColumn<Validity>("validity");
    QTest::newRow("pending") << Validity::Pending;
    QTest::newRow("stale") << Validity::Stale;
    QTest::newRow("degraded") << Validity::Degraded;
    QTest::newRow("unavailable") << Validity::Unavailable;
}

void RepositoryStatusIndexTest::nonReadySnapshotsClearDetails()
{
    QFETCH(Validity, validity);
    for (bool explicitInvalidation : {false, true})
    {
        RepositoryStatusIndex index(QStringLiteral("/repo"));
        auto snapshot = complete();
        snapshot.entries = {entry(QStringLiteral("file"), FileState::Modified)};
        QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
        if (explicitInvalidation)
        {
            QVERIFY(index.invalidate(validity, 2));
        }
        else
        {
            snapshot.revision = 2;
            snapshot.validity = validity;
            QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
        }
        const auto result = index.lookup(QStringLiteral("file"), Start);
        QCOMPARE(result.validity, validity);
        QCOMPARE(result.state, State::Unknown);
        QVERIFY(!result.entry.has_value());
        QCOMPARE(index.recordCount(), 0);
        QCOMPARE(index.accountedBytes(), 0);
    }
}

void RepositoryStatusIndexTest::revisionsPreventLateResurrection()
{
    RepositoryStatusIndex index(QStringLiteral("/repo"));
    auto snapshot = complete(3);
    snapshot.trackedPaths = {QStringLiteral("clean")};
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QVERIFY(!index.invalidate(Validity::Pending, 2));
    QVERIFY(!index.invalidate(Validity::Ready, 4));
    QVERIFY(!index.invalidate(static_cast<Validity>(999), 4));
    snapshot.revision = 2;
    snapshot.coverageComplete = false;
    QCOMPARE(index.replace(snapshot), StoreResult::ObsoleteRevision);
    QCOMPARE(stateAt(index, QStringLiteral("clean")), State::Clean);
    QVERIFY(index.invalidate(Validity::Pending, 4));
    snapshot = complete(4);
    QCOMPARE(index.replace(snapshot), StoreResult::ObsoleteRevision);
    snapshot.revision = 5;
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    index.clear();
    QCOMPARE(index.revision(), quint64(5));
    QCOMPARE(index.recordCount(), 0);
    QCOMPARE(index.replace(snapshot), StoreResult::ObsoleteRevision);
    QCOMPARE(index.lookup(QString(), Start).validity, Validity::Unavailable);
    snapshot.revision = 6;
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(stateAt(index, QString()), State::Clean);
    snapshot.revision = 0;
    QCOMPARE(index.replace(snapshot), StoreResult::ObsoleteRevision);
}

void RepositoryStatusIndexTest::boundsDiscardWholeSnapshot()
{
    RepositoryStatusIndexLimits limits;
    limits.maxRecords = 3;
    RepositoryStatusIndex index(QStringLiteral("/repo"), limits);
    auto snapshot = complete();
    snapshot.trackedPaths = {QStringLiteral("dir/file")};
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(index.recordCount(), 3);
    snapshot.revision = 2;
    snapshot.trackedPaths.append(QStringLiteral("other"));
    QCOMPARE(index.replace(snapshot), StoreResult::LimitExceeded);
    QCOMPARE(index.lookup(QStringLiteral("dir/file"), Start).validity, Validity::Degraded);
    QCOMPARE(stateAt(index, QStringLiteral("dir/file")), State::Unknown);
    QCOMPARE(index.recordCount(), 0);
    QCOMPARE(index.accountedBytes(), 0);
    for (qsizetype bound : {qsizetype(0), qsizetype(-1)})
    {
        limits.maxRecords = bound;
        RepositoryStatusIndex disabled(QStringLiteral("/repo"), limits);
        QCOMPARE(disabled.replace(complete()), StoreResult::LimitExceeded);
        QCOMPARE(stateAt(disabled, QString()), State::Unknown);
    }
}

void RepositoryStatusIndexTest::accountsBoundariesDetailsAndAncestors()
{
    RepositoryStatusIndex reference(QStringLiteral("/repo"));
    auto snapshot = complete();
    snapshot.trackedPaths = {QStringLiteral("dir/file")};
    snapshot.excludedRoots = {QStringLiteral("nested")};
    auto renamed = entry(QStringLiteral("dir/file"), FileState::Renamed);
    renamed.originalPath = QStringLiteral("old/file");
    snapshot.entries = {renamed};
    QCOMPARE(reference.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(reference.recordCount(), 4); // Root, dir, file, ownership boundary.
    RepositoryStatusIndexLimits limits;
    limits.maxAccountedBytes = reference.accountedBytes();
    RepositoryStatusIndex exact(QStringLiteral("/repo"), limits);
    QCOMPARE(exact.replace(snapshot), StoreResult::Accepted);
    limits.maxAccountedBytes--;
    RepositoryStatusIndex tooSmall(QStringLiteral("/repo"), limits);
    QCOMPARE(tooSmall.replace(snapshot), StoreResult::LimitExceeded);
    QCOMPARE(tooSmall.accountedBytes(), 0);
    limits.maxAccountedBytes = 0;
    RepositoryStatusIndex disabled(QStringLiteral("/repo"), limits);
    QCOMPARE(disabled.replace(complete()), StoreResult::LimitExceeded);
}

void RepositoryStatusIndexTest::duplicateInventoryIsDeduplicated()
{
    RepositoryStatusIndex index(QStringLiteral("/repo"));
    auto snapshot = complete();
    snapshot.trackedPaths = {QStringLiteral("conflict"), QStringLiteral("conflict"),
                             QStringLiteral("conflict")};
    snapshot.entries = {entry(QStringLiteral("conflict"), FileState::Conflict)};
    snapshot.excludedRoots = {QStringLiteral("nested"), QStringLiteral("nested")};
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(index.recordCount(), 3);
    QCOMPARE(stateAt(index, QStringLiteral("conflict")), State::Conflict);
}

void RepositoryStatusIndexTest::handlesDeepPathsWithoutRecursion()
{
    RepositoryStatusIndex index(QStringLiteral("/repo"));
    auto snapshot = complete();
    const QString path = QStringLiteral("dir/").repeated(400) + QStringLiteral("file");
    snapshot.entries = {entry(path, FileState::Modified)};
    QCOMPARE(index.replace(snapshot), StoreResult::Accepted);
    QCOMPARE(index.recordCount(), 402);
    QCOMPARE(stateAt(index, QString()), State::Modified);
    QCOMPARE(stateAt(index, QStringLiteral("dir/dir")), State::Modified);
    QCOMPARE(stateAt(index, path), State::Modified);
}

QTEST_GUILESS_MAIN(RepositoryStatusIndexTest)
#include "RepositoryStatusIndexTest.moc"
