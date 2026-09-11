// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/GitStatusReader.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <chrono>
#include <optional>

using namespace std::chrono_literals;

namespace
{

struct CommandResult
{
    bool started = false;
    bool finished = false;
    int exitCode = -1;
    QByteArray standardError;
};

[[nodiscard]] CommandResult runGit(const QString& workingDirectory, const QStringList& arguments)
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("GIT_CONFIG_NOSYSTEM"), QStringLiteral("1"));
    environment.insert(QStringLiteral("GIT_CONFIG_GLOBAL"), QStringLiteral("/dev/null"));
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(workingDirectory);
    process.setProgram(QStringLiteral(GIT_EXECUTABLE_PATH));
    process.setArguments({QStringLiteral("-c"), QStringLiteral("user.name=LinuxGitShell Tests"),
                          QStringLiteral("-c"),
                          QStringLiteral("user.email=tests@linuxgitshell.invalid")});
    QStringList completeArguments = process.arguments();
    completeArguments.append(arguments);
    process.setArguments(completeArguments);
    process.start();

    CommandResult result;
    result.started = process.waitForStarted(5000);
    if (!result.started)
    {
        result.standardError = process.errorString().toLocal8Bit();
        return result;
    }
    result.finished = process.waitForFinished(10000);
    result.exitCode = process.exitCode();
    result.standardError = process.readAllStandardError();
    return result;
}

[[nodiscard]] bool commandSucceeded(const CommandResult& result)
{
    return result.started && result.finished && result.exitCode == 0;
}

[[nodiscard]] bool writeTextFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }
    return file.write(contents) == contents.size();
}

[[nodiscard]] LinuxGitShell::GitStatusResult
readStatus(LinuxGitShell::GitStatusReader& reader, const QString& path, bool includeIgnored = false)
{
    QSignalSpy finishedSpy(&reader, &LinuxGitShell::GitStatusReader::finished);
    LinuxGitShell::GitStatusRequest request;
    request.path = path;
    request.includeIgnored = includeIgnored;
    if (reader.start(request) != LinuxGitShell::GitStatusStartResult::Accepted)
    {
        return {};
    }
    if (finishedSpy.isEmpty() && !finishedSpy.wait(10000))
    {
        return {};
    }
    return qvariant_cast<LinuxGitShell::GitStatusResult>(finishedSpy.takeFirst().at(0));
}

[[nodiscard]] std::optional<LinuxGitShell::GitStatusEntry>
entryFor(const LinuxGitShell::GitStatusSnapshot& status, const QString& path)
{
    for (const LinuxGitShell::GitStatusEntry& entry : status.entries)
    {
        if (entry.path == path)
        {
            return entry;
        }
    }
    return std::nullopt;
}

} // namespace

class GitStatusReaderTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    void rejectsInvalidRequestAndReportsNonRepository();
    void reportsBareRepository();
    void readsCleanRepository();
    void separatesStagedUnstagedAndCombinedChanges();
    void includesUntrackedAndOptInIgnoredFiles();
    void readsAddedDeletedRenamedAndUnicodePaths();
    void readsRealConflictAndIndexStages();
};

void GitStatusReaderTest::initTestCase()
{
    qputenv("GIT_CONFIG_NOSYSTEM", QByteArrayLiteral("1"));
    qputenv("GIT_CONFIG_GLOBAL", QByteArrayLiteral("/dev/null"));
    qputenv("GIT_TERMINAL_PROMPT", QByteArrayLiteral("0"));
    qRegisterMetaType<LinuxGitShell::GitStatusResult>();
    const CommandResult version = runGit(QDir::tempPath(), {QStringLiteral("--version")});
    QVERIFY2(commandSucceeded(version), version.standardError.constData());
}

void GitStatusReaderTest::rejectsInvalidRequestAndReportsNonRepository()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    LinuxGitShell::GitStatusReader reader;

    LinuxGitShell::GitStatusRequest request;
    request.path = temporaryDirectory.filePath(QStringLiteral("missing"));
    QCOMPARE(reader.start(request), LinuxGitShell::GitStatusStartResult::InvalidPath);
    request.path = temporaryDirectory.path();
    request.timeout = 0ms;
    QCOMPARE(reader.start(request), LinuxGitShell::GitStatusStartResult::InvalidTimeout);

    const auto result = readStatus(reader, temporaryDirectory.path());
    QCOMPARE(result.error, LinuxGitShell::GitStatusReadError::NotRepository);
    QVERIFY(!result.status.has_value());
    QVERIFY(!result.gitResult.standardError.isEmpty());
}

void GitStatusReaderTest::reportsBareRepository()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("bare.git"));
    QVERIFY(QDir().mkpath(repositoryPath));
    const CommandResult init = runGit(
        repositoryPath, {QStringLiteral("init"), QStringLiteral("--bare"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(init), init.standardError.constData());

    LinuxGitShell::GitStatusReader reader;
    const auto result = readStatus(reader, repositoryPath);
    QCOMPARE(result.error, LinuxGitShell::GitStatusReadError::NoWorkingTree);
    QVERIFY(!result.status.has_value());
}

void GitStatusReaderTest::readsCleanRepository()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("clean"));
    QVERIFY(QDir().mkpath(repositoryPath));
    CommandResult command = runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral("-b"),
                                                    QStringLiteral("main"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(writeTextFile(QDir(repositoryPath).filePath(QStringLiteral("tracked.txt")),
                          QByteArrayLiteral("tracked\n")));
    command = runGit(repositoryPath, {QStringLiteral("add"), QStringLiteral("tracked.txt")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath, {QStringLiteral("commit"), QStringLiteral("-m"),
                                      QStringLiteral("Initial commit")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    LinuxGitShell::GitStatusReader reader;
    const auto result = readStatus(reader, repositoryPath);
    QCOMPARE(result.error, LinuxGitShell::GitStatusReadError::None);
    QCOMPARE(result.parseError, LinuxGitShell::GitStatusParseError::None);
    QVERIFY(result.status.has_value());
    const auto status = result.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    QVERIFY(status.isClean());
    QCOMPARE(status.currentBranch, QStringLiteral("main"));
    QVERIFY(!status.headObjectId.isEmpty());
    QVERIFY(!result.gitResult.standardOutput.isEmpty());
}

void GitStatusReaderTest::separatesStagedUnstagedAndCombinedChanges()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("mixed"));
    QVERIFY(QDir().mkpath(repositoryPath));
    CommandResult command = runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    const QStringList paths = {QStringLiteral("staged.txt"), QStringLiteral("working.txt"),
                               QStringLiteral("combined.txt")};
    for (const QString& path : paths)
    {
        QVERIFY(writeTextFile(QDir(repositoryPath).filePath(path), QByteArrayLiteral("base\n")));
    }
    command = runGit(repositoryPath,
                     {QStringLiteral("add"), QStringLiteral("--"), QStringLiteral("staged.txt"),
                      QStringLiteral("working.txt"), QStringLiteral("combined.txt")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath, {QStringLiteral("commit"), QStringLiteral("-m"),
                                      QStringLiteral("Track files")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    for (const QString& path : paths)
    {
        QVERIFY(writeTextFile(QDir(repositoryPath).filePath(path), QByteArrayLiteral("first\n")));
    }
    command =
        runGit(repositoryPath, {QStringLiteral("add"), QStringLiteral("--"),
                                QStringLiteral("staged.txt"), QStringLiteral("combined.txt")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(writeTextFile(QDir(repositoryPath).filePath(QStringLiteral("combined.txt")),
                          QByteArrayLiteral("second\n")));

    LinuxGitShell::GitStatusReader reader;
    const auto result = readStatus(reader, repositoryPath);
    QVERIFY(result.status.has_value());
    const auto status = result.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    const auto staged =
        entryFor(status, QStringLiteral("staged.txt")).value_or(LinuxGitShell::GitStatusEntry{});
    const auto working =
        entryFor(status, QStringLiteral("working.txt")).value_or(LinuxGitShell::GitStatusEntry{});
    const auto combined =
        entryFor(status, QStringLiteral("combined.txt")).value_or(LinuxGitShell::GitStatusEntry{});
    QCOMPARE(staged.indexState, LinuxGitShell::GitFileState::Modified);
    QCOMPARE(staged.workTreeState, LinuxGitShell::GitFileState::Unmodified);
    QCOMPARE(working.indexState, LinuxGitShell::GitFileState::Unmodified);
    QCOMPARE(working.workTreeState, LinuxGitShell::GitFileState::Modified);
    QCOMPARE(combined.indexState, LinuxGitShell::GitFileState::Modified);
    QCOMPARE(combined.workTreeState, LinuxGitShell::GitFileState::Modified);
}

void GitStatusReaderTest::includesUntrackedAndOptInIgnoredFiles()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("ignored"));
    QVERIFY(QDir().mkpath(repositoryPath));
    CommandResult command = runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(writeTextFile(QDir(repositoryPath).filePath(QStringLiteral(".gitignore")),
                          QByteArrayLiteral("ignored.txt\n")));
    command = runGit(repositoryPath, {QStringLiteral("add"), QStringLiteral(".gitignore")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath, {QStringLiteral("commit"), QStringLiteral("-m"),
                                      QStringLiteral("Add ignore rule")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(writeTextFile(QDir(repositoryPath).filePath(QStringLiteral("untracked.txt")),
                          QByteArrayLiteral("untracked")));
    QVERIFY(writeTextFile(QDir(repositoryPath).filePath(QStringLiteral("ignored.txt")),
                          QByteArrayLiteral("ignored")));

    LinuxGitShell::GitStatusReader reader;
    const auto withoutIgnored = readStatus(reader, repositoryPath);
    QVERIFY(withoutIgnored.status.has_value());
    const auto defaultStatus = withoutIgnored.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    QVERIFY(entryFor(defaultStatus, QStringLiteral("untracked.txt")).has_value());
    QVERIFY(!entryFor(defaultStatus, QStringLiteral("ignored.txt")).has_value());

    const auto withIgnored = readStatus(reader, repositoryPath, true);
    QVERIFY(withIgnored.status.has_value());
    const auto completeStatus = withIgnored.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    const auto untracked = entryFor(completeStatus, QStringLiteral("untracked.txt"))
                               .value_or(LinuxGitShell::GitStatusEntry{});
    const auto ignored = entryFor(completeStatus, QStringLiteral("ignored.txt"))
                             .value_or(LinuxGitShell::GitStatusEntry{});
    QCOMPARE(untracked.kind, LinuxGitShell::GitStatusRecordKind::Untracked);
    QCOMPARE(ignored.kind, LinuxGitShell::GitStatusRecordKind::Ignored);
}

void GitStatusReaderTest::readsAddedDeletedRenamedAndUnicodePaths()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("paths"));
    QVERIFY(QDir().mkpath(repositoryPath));
    CommandResult command = runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    const QString deletedPath = QStringLiteral("deleted.txt");
    const QString originalPath = QStringLiteral("old name.txt");
    QVERIFY(writeTextFile(QDir(repositoryPath).filePath(deletedPath), QByteArrayLiteral("delete")));
    QVERIFY(
        writeTextFile(QDir(repositoryPath).filePath(originalPath), QByteArrayLiteral("rename")));
    command = runGit(repositoryPath,
                     {QStringLiteral("add"), QStringLiteral("--"), deletedPath, originalPath});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath, {QStringLiteral("commit"), QStringLiteral("-m"),
                                      QStringLiteral("Track paths")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    const QString addedPath = QStringLiteral("-ünicode\nadded.txt");
    const QString renamedPath = QStringLiteral("renamed destination.txt");
    QVERIFY(writeTextFile(QDir(repositoryPath).filePath(addedPath), QByteArrayLiteral("added")));
    command = runGit(repositoryPath, {QStringLiteral("add"), QStringLiteral("--"), addedPath});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(QFile::remove(QDir(repositoryPath).filePath(deletedPath)));
    command = runGit(repositoryPath,
                     {QStringLiteral("mv"), QStringLiteral("--"), originalPath, renamedPath});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    LinuxGitShell::GitStatusReader reader;
    const auto result = readStatus(reader, repositoryPath);
    QVERIFY(result.status.has_value());
    const auto status = result.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    const auto added = entryFor(status, addedPath).value_or(LinuxGitShell::GitStatusEntry{});
    const auto deleted = entryFor(status, deletedPath).value_or(LinuxGitShell::GitStatusEntry{});
    const auto renamed = entryFor(status, renamedPath).value_or(LinuxGitShell::GitStatusEntry{});
    QCOMPARE(added.indexState, LinuxGitShell::GitFileState::Added);
    QCOMPARE(deleted.workTreeState, LinuxGitShell::GitFileState::Deleted);
    QCOMPARE(renamed.indexState, LinuxGitShell::GitFileState::Renamed);
    QCOMPARE(renamed.originalPath, originalPath);
    QVERIFY(renamed.similarityScore.has_value());
}

void GitStatusReaderTest::readsRealConflictAndIndexStages()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("conflict"));
    QVERIFY(QDir().mkpath(repositoryPath));
    CommandResult command = runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral("-b"),
                                                    QStringLiteral("main"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    const QString conflictPath = QStringLiteral("conflict.txt");
    const QString filePath = QDir(repositoryPath).filePath(conflictPath);
    QVERIFY(writeTextFile(filePath, QByteArrayLiteral("base\n")));
    command = runGit(repositoryPath, {QStringLiteral("add"), conflictPath});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath, {QStringLiteral("commit"), QStringLiteral("-m"),
                                      QStringLiteral("Base commit")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath, {QStringLiteral("checkout"), QStringLiteral("-b"),
                                      QStringLiteral("feature"), QStringLiteral("--quiet")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(writeTextFile(filePath, QByteArrayLiteral("feature\n")));
    command = runGit(repositoryPath, {QStringLiteral("commit"), QStringLiteral("-am"),
                                      QStringLiteral("Feature change")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath, {QStringLiteral("checkout"), QStringLiteral("main"),
                                      QStringLiteral("--quiet")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(writeTextFile(filePath, QByteArrayLiteral("main\n")));
    command = runGit(repositoryPath, {QStringLiteral("commit"), QStringLiteral("-am"),
                                      QStringLiteral("Main change")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command =
        runGit(repositoryPath, {QStringLiteral("merge"), QStringLiteral("feature"),
                                QStringLiteral("--no-edit"), QStringLiteral("--no-gpg-sign")});
    QVERIFY(command.started);
    QVERIFY(command.finished);
    QVERIFY(command.exitCode != 0);

    LinuxGitShell::GitStatusReader reader;
    const auto result = readStatus(reader, repositoryPath);
    QVERIFY(result.status.has_value());
    const auto status = result.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    const auto conflict = entryFor(status, conflictPath).value_or(LinuxGitShell::GitStatusEntry{});
    QVERIFY(conflict.isConflicted());
    QCOMPARE(conflict.kind, LinuxGitShell::GitStatusRecordKind::Unmerged);
    QCOMPARE(conflict.modes.size(), 4);
    QCOMPARE(conflict.objectIds.size(), 3);
}

QTEST_GUILESS_MAIN(GitStatusReaderTest)

#include "GitStatusReaderTest.moc"
