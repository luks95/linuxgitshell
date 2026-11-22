// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "RepositoryStatusTranslator.h"
#include "GitTestSupport.h"
#include "linuxgitshell/gitcore/GitInventoryReader.h"
#include "linuxgitshell/gitcore/GitStatusReader.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace LinuxGitShell;
using State = RepositoryStatusState;

namespace
{

constexpr RepositoryStatusIndex::Clock::time_point Start{};

[[nodiscard]] QString child(const QString& root, const QString& relative)
{
    return QDir(root).filePath(relative);
}

// Runs both real collectors and translates their output into an index at revision 1.
[[nodiscard]] bool collect(const QString& root, RepositoryStatusIndex& index)
{
    GitStatusReader statusReader;
    QSignalSpy statusSpy(&statusReader, &GitStatusReader::finished);
    GitStatusRequest statusRequest;
    statusRequest.path = root;
    statusRequest.includeIgnored = true;
    if (statusReader.start(statusRequest) != GitStatusStartResult::Accepted ||
        (statusSpy.isEmpty() && !statusSpy.wait(10000)))
    {
        return false;
    }
    const auto status = qvariant_cast<GitStatusResult>(statusSpy.takeFirst().at(0));

    GitInventoryReader inventoryReader;
    QSignalSpy inventorySpy(&inventoryReader, &GitInventoryReader::finished);
    GitInventoryRequest inventoryRequest;
    inventoryRequest.repositoryRoot = root;
    if (inventoryReader.start(inventoryRequest) != GitInventoryStartResult::Accepted ||
        (inventorySpy.isEmpty() && !inventorySpy.wait(10000)))
    {
        return false;
    }
    const auto inventory = qvariant_cast<GitInventoryResult>(inventorySpy.takeFirst().at(0));
    if (!status.status.has_value() || !inventory.paths.has_value())
    {
        return false;
    }

    RepositoryStatusTranslationInput input;
    input.revision = 1;
    input.capturedAt = Start;
    input.status = *status.status;
    input.trackedPaths = *inventory.paths;
    return index.replace(translateRepositoryStatus(input)) == RepositoryStatusStoreResult::Accepted;
}

[[nodiscard]] State stateAt(const RepositoryStatusIndex& index, const QString& path)
{
    return index.lookup(path, Start).state;
}

} // namespace

class RepositoryStatusTranslatorTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    void mapsEveryGitStateAndStripsDirectoryTerminators();
    void conflictWinsOverUnmergedLetters();
    void realRepositoryClassifiesAllStates();
    void emptyRepositoryRootIsClean();
};

void RepositoryStatusTranslatorTest::initTestCase()
{
    GitTestSupport::isolateGitConfiguration();
    qRegisterMetaType<GitStatusResult>();
    qRegisterMetaType<GitInventoryResult>();
}

void RepositoryStatusTranslatorTest::mapsEveryGitStateAndStripsDirectoryTerminators()
{
    RepositoryStatusTranslationInput input;
    input.revision = 7;
    input.capturedAt = Start;
    input.trackedPaths = {QStringLiteral("a")};
    input.excludedRoots = {QStringLiteral("nested")};

    GitStatusEntry renamed;
    renamed.kind = GitStatusRecordKind::RenamedOrCopied;
    renamed.indexState = GitFileState::Renamed;
    renamed.workTreeState = GitFileState::Modified;
    renamed.path = QStringLiteral("new.txt");
    renamed.originalPath = QStringLiteral("old.txt");
    GitStatusEntry untrackedDirectory;
    untrackedDirectory.kind = GitStatusRecordKind::Untracked;
    untrackedDirectory.workTreeState = GitFileState::Untracked;
    untrackedDirectory.path = QStringLiteral("nested/");
    GitStatusEntry ignored;
    ignored.kind = GitStatusRecordKind::Ignored;
    ignored.workTreeState = GitFileState::Ignored;
    ignored.path = QStringLiteral("build/");
    GitStatusEntry submodule;
    submodule.indexState = GitFileState::Unmodified;
    submodule.workTreeState = GitFileState::Modified;
    submodule.submodule = {true, true, false, true};
    submodule.path = QStringLiteral("sub");
    GitStatusEntry typeChanged;
    typeChanged.indexState = GitFileState::TypeChanged;
    typeChanged.workTreeState = GitFileState::Deleted;
    typeChanged.path = QStringLiteral("t");
    GitStatusEntry copied;
    copied.kind = GitStatusRecordKind::RenamedOrCopied;
    copied.indexState = GitFileState::Copied;
    copied.path = QStringLiteral("c2");
    copied.originalPath = QStringLiteral("c1");
    GitStatusEntry added;
    added.indexState = GitFileState::Added;
    added.path = QStringLiteral("added");
    input.status.entries = {renamed, untrackedDirectory, ignored, submodule, typeChanged, copied,
                            added};

    const RepositoryStatusSnapshot snapshot = translateRepositoryStatus(input);
    QCOMPARE(snapshot.revision, quint64(7));
    QCOMPARE(snapshot.validity, RepositoryStatusValidity::Ready);
    QVERIFY(snapshot.coverageComplete);
    QCOMPARE(snapshot.trackedPaths, input.trackedPaths);
    QCOMPARE(snapshot.excludedRoots, input.excludedRoots);
    QCOMPARE(snapshot.entries.size(), 7);

    QCOMPARE(snapshot.entries.at(0).path, QStringLiteral("new.txt"));
    QCOMPARE(snapshot.entries.at(0).originalPath, QStringLiteral("old.txt"));
    QCOMPARE(snapshot.entries.at(0).indexState, RepositoryFileState::Renamed);
    QCOMPARE(snapshot.entries.at(0).workTreeState, RepositoryFileState::Modified);
    QCOMPARE(snapshot.entries.at(1).path, QStringLiteral("nested"));
    QVERIFY(snapshot.entries.at(1).isDirectory);
    QCOMPARE(snapshot.entries.at(1).workTreeState, RepositoryFileState::Untracked);
    QCOMPARE(snapshot.entries.at(2).path, QStringLiteral("build"));
    QVERIFY(snapshot.entries.at(2).isDirectory);
    QCOMPARE(snapshot.entries.at(2).workTreeState, RepositoryFileState::Ignored);
    QVERIFY(snapshot.entries.at(3).isSubmodule);
    QVERIFY(snapshot.entries.at(3).submoduleCommitChanged);
    QVERIFY(!snapshot.entries.at(3).submoduleTrackedChanges);
    QVERIFY(snapshot.entries.at(3).submoduleUntrackedChanges);
    QCOMPARE(snapshot.entries.at(4).indexState, RepositoryFileState::TypeChanged);
    QCOMPARE(snapshot.entries.at(4).workTreeState, RepositoryFileState::Deleted);
    QCOMPARE(snapshot.entries.at(5).indexState, RepositoryFileState::Copied);
    QCOMPARE(snapshot.entries.at(6).indexState, RepositoryFileState::Added);
    QVERIFY(!snapshot.entries.at(6).isDirectory);

    RepositoryStatusIndex index(QStringLiteral("/repo"));
    QCOMPARE(index.replace(snapshot), RepositoryStatusStoreResult::Accepted);
}

void RepositoryStatusTranslatorTest::conflictWinsOverUnmergedLetters()
{
    RepositoryStatusTranslationInput input;
    input.revision = 1;
    GitStatusEntry conflicted;
    conflicted.kind = GitStatusRecordKind::Unmerged;
    conflicted.indexState = GitFileState::Deleted;
    conflicted.workTreeState = GitFileState::Modified;
    conflicted.path = QStringLiteral("c");
    input.status.entries = {conflicted};

    const RepositoryStatusSnapshot snapshot = translateRepositoryStatus(input);
    QCOMPARE(snapshot.entries.at(0).indexState, RepositoryFileState::Conflict);
    QCOMPARE(snapshot.entries.at(0).workTreeState, RepositoryFileState::Conflict);
}

void RepositoryStatusTranslatorTest::realRepositoryClassifiesAllStates()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = child(directory.path(), QStringLiteral("repo"));
    QVERIFY(QDir().mkpath(root));
    QVERIFY(GitTestSupport::runGit(root, {QStringLiteral("init"), QStringLiteral(".")}));
    QVERIFY(GitTestSupport::writeTextFile(child(root, QStringLiteral(".gitignore")),
                                          "*.log\nbuild/\n"));
    QVERIFY(QDir().mkpath(child(root, QStringLiteral("src/deep"))));
    for (const QString& name : {QStringLiteral("clean.txt"), QStringLiteral("modified.txt"),
                                QStringLiteral("rename-me.txt"), QStringLiteral("src/deep/in.txt"),
                                QStringLiteral("conflict.txt"), QStringLiteral("gone.txt")})
    {
        QVERIFY(GitTestSupport::writeTextFile(child(root, name), "base\n"));
    }
    QVERIFY(GitTestSupport::runGit(root, {QStringLiteral("add"), QStringLiteral("-A")}));
    QVERIFY(GitTestSupport::runGit(
        root, {QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("base")}));

    // Build a merge conflict in conflict.txt.
    QVERIFY(GitTestSupport::runGit(
        root, {QStringLiteral("checkout"), QStringLiteral("-b"), QStringLiteral("side")}));
    QVERIFY(GitTestSupport::writeTextFile(child(root, QStringLiteral("conflict.txt")), "side\n"));
    QVERIFY(GitTestSupport::runGit(
        root, {QStringLiteral("commit"), QStringLiteral("-am"), QStringLiteral("side")}));
    QVERIFY(GitTestSupport::runGit(root, {QStringLiteral("checkout"), QStringLiteral("-")}));
    QVERIFY(GitTestSupport::writeTextFile(child(root, QStringLiteral("conflict.txt")), "main\n"));
    QVERIFY(GitTestSupport::runGit(
        root, {QStringLiteral("commit"), QStringLiteral("-am"), QStringLiteral("main")}));
    // The merge is expected to stop with a conflict, so its exit status is not checked.
    (void)GitTestSupport::runGit(root, {QStringLiteral("merge"), QStringLiteral("side")});

    QVERIFY(
        GitTestSupport::writeTextFile(child(root, QStringLiteral("modified.txt")), "changed\n"));
    QVERIFY(GitTestSupport::runGit(root, {QStringLiteral("mv"), QStringLiteral("rename-me.txt"),
                                          QStringLiteral("renamed.txt")}));
    QVERIFY(QFile::remove(child(root, QStringLiteral("gone.txt"))));
    QVERIFY(GitTestSupport::writeTextFile(child(root, QStringLiteral("staged.txt")), "new\n"));
    QVERIFY(GitTestSupport::runGit(root, {QStringLiteral("add"), QStringLiteral("staged.txt")}));
    QVERIFY(GitTestSupport::writeTextFile(child(root, QStringLiteral("src/deep/new.txt")), "u\n"));
    QVERIFY(GitTestSupport::writeTextFile(child(root, QStringLiteral("debug.log")), "log\n"));
    QVERIFY(QDir().mkpath(child(root, QStringLiteral("build/out"))));
    QVERIFY(GitTestSupport::writeTextFile(child(root, QStringLiteral("build/out/a.o")), "o\n"));

    RepositoryStatusIndex index(root);
    QVERIFY(collect(root, index));

    QCOMPARE(stateAt(index, QStringLiteral("clean.txt")), State::Clean);
    QCOMPARE(stateAt(index, QStringLiteral("modified.txt")), State::Modified);
    QCOMPARE(stateAt(index, QStringLiteral("renamed.txt")), State::Modified);
    QCOMPARE(stateAt(index, QStringLiteral("rename-me.txt")), State::Unknown);
    QCOMPARE(stateAt(index, QStringLiteral("conflict.txt")), State::Conflict);
    QCOMPARE(stateAt(index, QStringLiteral("gone.txt")), State::Deleted);
    QCOMPARE(stateAt(index, QStringLiteral("staged.txt")), State::Added);
    QCOMPARE(stateAt(index, QStringLiteral("src/deep/new.txt")), State::Untracked);
    QCOMPARE(stateAt(index, QStringLiteral("src/deep/in.txt")), State::Clean);
    QCOMPARE(stateAt(index, QStringLiteral("debug.log")), State::Ignored);
    QCOMPARE(stateAt(index, QStringLiteral("build")), State::Ignored);
    QCOMPARE(stateAt(index, QStringLiteral("build/out/a.o")), State::Ignored);
    QCOMPARE(stateAt(index, QStringLiteral("never/seen.txt")), State::Unknown);
    // Recursive directory summaries: conflict outranks everything beneath the root.
    QCOMPARE(stateAt(index, QStringLiteral("src")), State::Untracked);
    QCOMPARE(stateAt(index, QStringLiteral("")), State::Conflict);
}

void RepositoryStatusTranslatorTest::emptyRepositoryRootIsClean()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(
        GitTestSupport::runGit(directory.path(), {QStringLiteral("init"), QStringLiteral(".")}));
    RepositoryStatusIndex index(directory.path());
    QVERIFY(collect(directory.path(), index));
    QCOMPARE(stateAt(index, QStringLiteral("")), State::Clean);
}

QTEST_GUILESS_MAIN(RepositoryStatusTranslatorTest)
#include "RepositoryStatusTranslatorTest.moc"
