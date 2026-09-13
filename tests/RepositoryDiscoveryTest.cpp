// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/RepositoryDiscovery.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace
{

struct CommandResult
{
    bool started = false;
    bool finished = false;
    int exitCode = -1;
    QByteArray standardOutput;
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
    result.standardOutput = process.readAllStandardOutput();
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

[[nodiscard]] LinuxGitShell::RepositoryDiscoveryResult
discoverRepository(LinuxGitShell::RepositoryDiscovery& discovery, const QString& path)
{
    QSignalSpy finishedSpy(&discovery, &LinuxGitShell::RepositoryDiscovery::finished);
    if (discovery.discover(path) != LinuxGitShell::RepositoryDiscoveryStartResult::Accepted)
    {
        return {};
    }
    if (finishedSpy.isEmpty() && !finishedSpy.wait(5000))
    {
        return {};
    }
    return qvariant_cast<LinuxGitShell::RepositoryDiscoveryResult>(finishedSpy.takeFirst().at(0));
}

} // namespace

class RepositoryDiscoveryTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    void rejectsMissingPath();
    void reportsPathOutsideRepository();
    void discoversNormalRepositoryFromNestedFile();
    void discoversRepositoryThroughDirectorySymlink();
    void discoversRepositoryThroughFileSymlink();
    void discoversRepositoryFromLongPath();
    void preservesCaseSensitiveRepositoryIdentity();
    void discoversBareRepository();
    void discoversLinkedWorktree();
    void discoversWorkTreeWithSeparateGitDirectory();
    void discoversSubmodule();
    void discoversBranchUpstreamDivergenceAndRemotes();
    void discoversDetachedHead();
    void detectsOperationInProgress_data();
    void detectsOperationInProgress();
    void detectsRealMergeInProgress();
};

void RepositoryDiscoveryTest::initTestCase()
{
    qputenv("GIT_CONFIG_NOSYSTEM", QByteArrayLiteral("1"));
    qputenv("GIT_CONFIG_GLOBAL", QByteArrayLiteral("/dev/null"));
    qputenv("GIT_TERMINAL_PROMPT", QByteArrayLiteral("0"));
    qRegisterMetaType<LinuxGitShell::RepositoryDiscoveryResult>();
    qRegisterMetaType<LinuxGitShell::RepositoryOperation>();
    const CommandResult version = runGit(QDir::tempPath(), {QStringLiteral("--version")});
    QVERIFY2(commandSucceeded(version), version.standardError.constData());
}

void RepositoryDiscoveryTest::rejectsMissingPath()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    LinuxGitShell::RepositoryDiscovery discovery;
    QCOMPARE(discovery.discover(temporaryDirectory.filePath(QStringLiteral("missing"))),
             LinuxGitShell::RepositoryDiscoveryStartResult::InvalidPath);
    QVERIFY(!discovery.isRunning());
}

void RepositoryDiscoveryTest::reportsPathOutsideRepository()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    LinuxGitShell::RepositoryDiscovery discovery;
    const auto result = discoverRepository(discovery, temporaryDirectory.path());
    QVERIFY(!result.requestedPath.isEmpty());
    QCOMPARE(result.error, LinuxGitShell::RepositoryDiscoveryError::NotRepository);
    QVERIFY(!result.repository.has_value());
    QCOMPARE(result.gitResults.size(), 1);
}

void RepositoryDiscoveryTest::discoversNormalRepositoryFromNestedFile()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath =
        temporaryDirectory.filePath(QStringLiteral("repo with spaces-Ünicode"));
    QVERIFY(QDir().mkpath(repositoryPath));
    const CommandResult init =
        runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(init), init.standardError.constData());

    const QString nestedPath = QDir(repositoryPath).filePath(QStringLiteral("deep/-leading"));
    QVERIFY(QDir().mkpath(nestedPath));
    const QString filePath = QDir(nestedPath).filePath(QStringLiteral("tracked later.txt"));
    QVERIFY(writeTextFile(filePath, QByteArrayLiteral("content")));

    LinuxGitShell::RepositoryDiscovery discovery;
    const auto result = discoverRepository(discovery, filePath);
    QVERIFY(!result.requestedPath.isEmpty());
    QCOMPARE(result.error, LinuxGitShell::RepositoryDiscoveryError::None);
    QVERIFY(result.repository.has_value());
    const auto repository = result.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QCOMPARE(repository.type, LinuxGitShell::RepositoryType::Normal);
    QCOMPARE(repository.repositoryRoot, QDir::cleanPath(repositoryPath));
    QCOMPARE(repository.workTree, QDir::cleanPath(repositoryPath));
    QCOMPARE(repository.gitDirectory,
             QDir::cleanPath(QDir(repositoryPath).filePath(QStringLiteral(".git"))));
    QCOMPARE(repository.commonGitDirectory, repository.gitDirectory);
    QVERIFY(QFileInfo(repository.gitDirectory).isDir());
    QVERIFY(repository.superprojectWorkingTree.isEmpty());
    QVERIFY(repository.isInsideWorkTree);
    QVERIFY(!repository.currentBranch.isEmpty());
    QVERIFY(!repository.isDetachedHead);
    QVERIFY(repository.upstream.isEmpty());
    QVERIFY(!repository.aheadCount.has_value());
    QVERIFY(!repository.behindCount.has_value());
    QVERIFY(repository.remotes.isEmpty());
    QVERIFY(repository.operations.isEmpty());
    QCOMPARE(result.gitResults.size(), 6);
}

void RepositoryDiscoveryTest::discoversRepositoryThroughDirectorySymlink()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("repository"));
    const QString nestedPath = QDir(repositoryPath).filePath(QStringLiteral("nested"));
    const QString symlinkPath = temporaryDirectory.filePath(QStringLiteral("repository-link"));
    QVERIFY(QDir().mkpath(nestedPath));
    const CommandResult init =
        runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(init), init.standardError.constData());
    QVERIFY(QFile::link(repositoryPath, symlinkPath));
    QVERIFY(QFileInfo(symlinkPath).isSymbolicLink());

    LinuxGitShell::RepositoryDiscovery discovery;
    const auto result =
        discoverRepository(discovery, QDir(symlinkPath).filePath(QStringLiteral("nested")));
    QVERIFY(!result.requestedPath.isEmpty());
    QVERIFY(result.repository.has_value());
    const auto repository = result.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QCOMPARE(repository.type, LinuxGitShell::RepositoryType::Normal);
    QCOMPARE(repository.repositoryRoot, QDir::cleanPath(repositoryPath));
}

void RepositoryDiscoveryTest::discoversRepositoryThroughFileSymlink()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("repository"));
    const QString nestedPath = QDir(repositoryPath).filePath(QStringLiteral("nested"));
    const QString targetPath = QDir(nestedPath).filePath(QStringLiteral("target.txt"));
    const QString symlinkPath = temporaryDirectory.filePath(QStringLiteral("target-link.txt"));
    QVERIFY(QDir().mkpath(nestedPath));
    const CommandResult init =
        runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(init), init.standardError.constData());
    QVERIFY(writeTextFile(targetPath, QByteArrayLiteral("content")));
    QVERIFY(QFile::link(targetPath, symlinkPath));
    QVERIFY(QFileInfo(symlinkPath).isSymbolicLink());

    LinuxGitShell::RepositoryDiscovery discovery;
    const auto result = discoverRepository(discovery, symlinkPath);
    QCOMPARE(result.requestedPath, QDir::cleanPath(symlinkPath));
    QCOMPARE(result.error, LinuxGitShell::RepositoryDiscoveryError::None);
    QVERIFY(result.repository.has_value());
    const auto repository = result.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QCOMPARE(repository.repositoryRoot, QDir::cleanPath(repositoryPath));
}

void RepositoryDiscoveryTest::discoversRepositoryFromLongPath()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("repository"));
    QVERIFY(QDir().mkpath(repositoryPath));
    const CommandResult init =
        runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(init), init.standardError.constData());

    QString longPath = repositoryPath;
    int segmentNumber = 0;
    while (longPath.size() < 1400)
    {
        const QString segment = QStringLiteral("segment-%1-%2")
                                    .arg(segmentNumber++, 3, 10, QLatin1Char('0'))
                                    .arg(QString(64, QLatin1Char('x')));
        longPath = QDir(longPath).filePath(segment);
    }
    QVERIFY2(QDir().mkpath(longPath), qPrintable(longPath));

    LinuxGitShell::RepositoryDiscovery discovery;
    const auto result = discoverRepository(discovery, longPath);
    QCOMPARE(result.error, LinuxGitShell::RepositoryDiscoveryError::None);
    QVERIFY(result.repository.has_value());
    const auto repository = result.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QCOMPARE(repository.repositoryRoot, QDir::cleanPath(repositoryPath));
}

void RepositoryDiscoveryTest::preservesCaseSensitiveRepositoryIdentity()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString upperPath = temporaryDirectory.filePath(QStringLiteral("Repository"));
    const QString lowerPath = temporaryDirectory.filePath(QStringLiteral("repository"));
    QVERIFY(QDir().mkpath(upperPath));
    QVERIFY(QDir().mkpath(lowerPath));
    if (QFileInfo(upperPath).canonicalFilePath() == QFileInfo(lowerPath).canonicalFilePath())
    {
        QSKIP("The test filesystem is case-insensitive");
    }

    CommandResult command = runGit(upperPath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(lowerPath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    LinuxGitShell::RepositoryDiscovery upperDiscovery;
    const auto upperResult = discoverRepository(upperDiscovery, upperPath);
    QVERIFY(upperResult.repository.has_value());
    const auto upperRepository = upperResult.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QCOMPARE(upperRepository.repositoryRoot, QDir::cleanPath(upperPath));

    LinuxGitShell::RepositoryDiscovery lowerDiscovery;
    const auto lowerResult = discoverRepository(lowerDiscovery, lowerPath);
    QVERIFY(lowerResult.repository.has_value());
    const auto lowerRepository = lowerResult.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QCOMPARE(lowerRepository.repositoryRoot, QDir::cleanPath(lowerPath));
    QVERIFY(upperRepository.repositoryRoot != lowerRepository.repositoryRoot);
}

void RepositoryDiscoveryTest::discoversBareRepository()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("bare.git"));
    QVERIFY(QDir().mkpath(repositoryPath));
    const CommandResult init = runGit(
        repositoryPath, {QStringLiteral("init"), QStringLiteral("--bare"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(init), init.standardError.constData());

    LinuxGitShell::RepositoryDiscovery discovery;
    const auto result = discoverRepository(discovery, repositoryPath);
    QVERIFY(!result.requestedPath.isEmpty());
    QVERIFY(result.repository.has_value());
    const auto repository = result.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QCOMPARE(repository.type, LinuxGitShell::RepositoryType::Bare);
    QCOMPARE(repository.repositoryRoot, QDir::cleanPath(repositoryPath));
    QCOMPARE(repository.gitDirectory, QDir::cleanPath(repositoryPath));
    QCOMPARE(repository.commonGitDirectory, QDir::cleanPath(repositoryPath));
    QVERIFY(repository.workTree.isEmpty());
    QVERIFY(!repository.isInsideWorkTree);
    QVERIFY(!repository.currentBranch.isEmpty());
    QVERIFY(!repository.isDetachedHead);
    QVERIFY(repository.operations.isEmpty());
    QCOMPARE(result.gitResults.size(), 5);
}

void RepositoryDiscoveryTest::discoversLinkedWorktree()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString mainPath = temporaryDirectory.filePath(QStringLiteral("main"));
    const QString linkedPath = temporaryDirectory.filePath(QStringLiteral("linked worktree"));
    QVERIFY(QDir().mkpath(mainPath));
    CommandResult command = runGit(mainPath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(writeTextFile(QDir(mainPath).filePath(QStringLiteral("file.txt")),
                          QByteArrayLiteral("initial")));
    command = runGit(mainPath, {QStringLiteral("add"), QStringLiteral("file.txt")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(mainPath, {QStringLiteral("commit"), QStringLiteral("-m"),
                                QStringLiteral("Initial test commit")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(mainPath, {QStringLiteral("worktree"), QStringLiteral("add"),
                                QStringLiteral("-b"), QStringLiteral("linked-test"), linkedPath});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    LinuxGitShell::RepositoryDiscovery discovery;
    const auto result = discoverRepository(discovery, linkedPath);
    QVERIFY(!result.requestedPath.isEmpty());
    QVERIFY(result.repository.has_value());
    const auto repository = result.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QCOMPARE(repository.type, LinuxGitShell::RepositoryType::LinkedWorktree);
    QCOMPARE(repository.workTree, QDir::cleanPath(linkedPath));
    QCOMPARE(repository.commonGitDirectory,
             QDir::cleanPath(QDir(mainPath).filePath(QStringLiteral(".git"))));
    QVERIFY(repository.gitDirectory != repository.commonGitDirectory);
    QVERIFY(QFileInfo(QDir(linkedPath).filePath(QStringLiteral(".git"))).isFile());
}

void RepositoryDiscoveryTest::discoversWorkTreeWithSeparateGitDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString workTreePath = temporaryDirectory.filePath(QStringLiteral("work tree"));
    const QString gitDirectoryPath = temporaryDirectory.filePath(QStringLiteral("metadata.git"));
    QVERIFY(QDir().mkpath(workTreePath));

    const CommandResult init = runGit(temporaryDirectory.path(),
                                      {QStringLiteral("init"), QStringLiteral("--separate-git-dir"),
                                       gitDirectoryPath, workTreePath});
    QVERIFY2(commandSucceeded(init), init.standardError.constData());
    QVERIFY(QFileInfo(QDir(workTreePath).filePath(QStringLiteral(".git"))).isFile());

    LinuxGitShell::RepositoryDiscovery discovery;
    const auto result = discoverRepository(discovery, workTreePath);
    QCOMPARE(result.error, LinuxGitShell::RepositoryDiscoveryError::None);
    QVERIFY(result.repository.has_value());
    const auto repository = result.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QCOMPARE(repository.type, LinuxGitShell::RepositoryType::Normal);
    QCOMPARE(repository.repositoryRoot, QDir::cleanPath(workTreePath));
    QCOMPARE(repository.workTree, QDir::cleanPath(workTreePath));
    QCOMPARE(repository.gitDirectory, QDir::cleanPath(gitDirectoryPath));
    QCOMPARE(repository.commonGitDirectory, QDir::cleanPath(gitDirectoryPath));
}

void RepositoryDiscoveryTest::discoversSubmodule()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString sourcePath = temporaryDirectory.filePath(QStringLiteral("source"));
    const QString superprojectPath = temporaryDirectory.filePath(QStringLiteral("superproject"));
    QVERIFY(QDir().mkpath(sourcePath));
    QVERIFY(QDir().mkpath(superprojectPath));

    CommandResult command = runGit(sourcePath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(writeTextFile(QDir(sourcePath).filePath(QStringLiteral("module.txt")),
                          QByteArrayLiteral("module")));
    command = runGit(sourcePath, {QStringLiteral("add"), QStringLiteral("module.txt")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(sourcePath, {QStringLiteral("commit"), QStringLiteral("-m"),
                                  QStringLiteral("Initial module commit")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    command = runGit(superprojectPath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(superprojectPath,
                     {QStringLiteral("-c"), QStringLiteral("protocol.file.allow=always"),
                      QStringLiteral("submodule"), QStringLiteral("add"), sourcePath,
                      QStringLiteral("modules/child")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    const QString submodulePath = QDir(superprojectPath).filePath(QStringLiteral("modules/child"));
    LinuxGitShell::RepositoryDiscovery discovery;
    const auto result = discoverRepository(discovery, submodulePath);
    QVERIFY(!result.requestedPath.isEmpty());
    QVERIFY(result.repository.has_value());
    const auto repository = result.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QCOMPARE(repository.type, LinuxGitShell::RepositoryType::Submodule);
    QCOMPARE(repository.workTree, QDir::cleanPath(submodulePath));
    QCOMPARE(repository.superprojectWorkingTree, QDir::cleanPath(superprojectPath));
    QVERIFY(repository.gitDirectory.contains(QStringLiteral("/.git/modules/")));
    QVERIFY(QFileInfo(QDir(submodulePath).filePath(QStringLiteral(".git"))).isFile());
}

void RepositoryDiscoveryTest::discoversBranchUpstreamDivergenceAndRemotes()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString originPath = temporaryDirectory.filePath(QStringLiteral("origin.git"));
    const QString localPath = temporaryDirectory.filePath(QStringLiteral("local"));
    const QString otherPath = temporaryDirectory.filePath(QStringLiteral("other"));
    QVERIFY(QDir().mkpath(originPath));
    QVERIFY(QDir().mkpath(localPath));

    CommandResult command =
        runGit(originPath, {QStringLiteral("init"), QStringLiteral("--bare"), QStringLiteral("-b"),
                            QStringLiteral("main"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(localPath, {QStringLiteral("init"), QStringLiteral("-b"),
                                 QStringLiteral("main"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(writeTextFile(QDir(localPath).filePath(QStringLiteral("base.txt")),
                          QByteArrayLiteral("base")));
    command = runGit(localPath, {QStringLiteral("add"), QStringLiteral("base.txt")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(
        localPath, {QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("Base commit")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(localPath, {QStringLiteral("remote"), QStringLiteral("add"),
                                 QStringLiteral("origin"), originPath});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(localPath, {QStringLiteral("push"), QStringLiteral("-u"),
                                 QStringLiteral("origin"), QStringLiteral("main")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    command = runGit(temporaryDirectory.path(), {QStringLiteral("clone"), originPath, otherPath});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(writeTextFile(QDir(localPath).filePath(QStringLiteral("local.txt")),
                          QByteArrayLiteral("local")));
    command = runGit(localPath, {QStringLiteral("add"), QStringLiteral("local.txt")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(localPath, {QStringLiteral("commit"), QStringLiteral("-m"),
                                 QStringLiteral("Local commit")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    QVERIFY(writeTextFile(QDir(otherPath).filePath(QStringLiteral("remote.txt")),
                          QByteArrayLiteral("remote")));
    command = runGit(otherPath, {QStringLiteral("add"), QStringLiteral("remote.txt")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(otherPath, {QStringLiteral("commit"), QStringLiteral("-m"),
                                 QStringLiteral("Remote commit")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(otherPath,
                     {QStringLiteral("push"), QStringLiteral("origin"), QStringLiteral("main")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(localPath, {QStringLiteral("fetch"), QStringLiteral("origin")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    LinuxGitShell::RepositoryDiscovery discovery;
    const auto result = discoverRepository(discovery, localPath);
    QVERIFY(result.repository.has_value());
    const auto repository = result.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QCOMPARE(repository.currentBranch, QStringLiteral("main"));
    QVERIFY(!repository.isDetachedHead);
    QCOMPARE(repository.upstream, QStringLiteral("origin/main"));
    QCOMPARE(repository.aheadCount.value_or(-1), 1);
    QCOMPARE(repository.behindCount.value_or(-1), 1);
    QCOMPARE(repository.remotes, QStringList{QStringLiteral("origin")});
}

void RepositoryDiscoveryTest::discoversDetachedHead()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("detached"));
    QVERIFY(QDir().mkpath(repositoryPath));
    CommandResult command = runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral("-b"),
                                                    QStringLiteral("main"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(writeTextFile(QDir(repositoryPath).filePath(QStringLiteral("file.txt")),
                          QByteArrayLiteral("content")));
    command = runGit(repositoryPath, {QStringLiteral("add"), QStringLiteral("file.txt")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath, {QStringLiteral("commit"), QStringLiteral("-m"),
                                      QStringLiteral("Initial commit")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath, {QStringLiteral("checkout"), QStringLiteral("--detach"),
                                      QStringLiteral("--quiet")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    LinuxGitShell::RepositoryDiscovery discovery;
    const auto result = discoverRepository(discovery, repositoryPath);
    QVERIFY(result.repository.has_value());
    const auto repository = result.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QVERIFY(repository.currentBranch.isEmpty());
    QVERIFY(repository.isDetachedHead);
    QVERIFY(repository.upstream.isEmpty());
    QVERIFY(!repository.aheadCount.has_value());
    QVERIFY(!repository.behindCount.has_value());
}

void RepositoryDiscoveryTest::detectsOperationInProgress_data()
{
    QTest::addColumn<QString>("marker");
    QTest::addColumn<bool>("directory");
    QTest::addColumn<LinuxGitShell::RepositoryOperation>("operation");

    QTest::newRow("merge") << QStringLiteral("MERGE_HEAD") << false
                           << LinuxGitShell::RepositoryOperation::Merge;
    QTest::newRow("rebase-merge") << QStringLiteral("rebase-merge") << true
                                  << LinuxGitShell::RepositoryOperation::Rebase;
    QTest::newRow("rebase-apply") << QStringLiteral("rebase-apply") << true
                                  << LinuxGitShell::RepositoryOperation::Rebase;
    QTest::newRow("cherry-pick") << QStringLiteral("CHERRY_PICK_HEAD") << false
                                 << LinuxGitShell::RepositoryOperation::CherryPick;
    QTest::newRow("revert") << QStringLiteral("REVERT_HEAD") << false
                            << LinuxGitShell::RepositoryOperation::Revert;
    QTest::newRow("bisect-start") << QStringLiteral("BISECT_START") << false
                                  << LinuxGitShell::RepositoryOperation::Bisect;
    QTest::newRow("bisect-log") << QStringLiteral("BISECT_LOG") << false
                                << LinuxGitShell::RepositoryOperation::Bisect;
}

void RepositoryDiscoveryTest::detectsOperationInProgress()
{
    QFETCH(QString, marker);
    QFETCH(bool, directory);
    QFETCH(LinuxGitShell::RepositoryOperation, operation);

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("operation"));
    QVERIFY(QDir().mkpath(repositoryPath));
    const CommandResult init =
        runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(init), init.standardError.constData());
    const QString markerPath = QDir(repositoryPath).filePath(QStringLiteral(".git/") + marker);
    if (directory)
    {
        QVERIFY(QDir().mkpath(markerPath));
    }
    else
    {
        QVERIFY(writeTextFile(markerPath, QByteArrayLiteral("test marker")));
    }

    LinuxGitShell::RepositoryDiscovery discovery;
    const auto result = discoverRepository(discovery, repositoryPath);
    QVERIFY(result.repository.has_value());
    const auto repository = result.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QCOMPARE(repository.operations.size(), 1);
    QCOMPARE(repository.operations.constFirst(), operation);
}

void RepositoryDiscoveryTest::detectsRealMergeInProgress()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("merge-conflict"));
    QVERIFY(QDir().mkpath(repositoryPath));

    CommandResult command = runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral("-b"),
                                                    QStringLiteral("main"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    const QString filePath = QDir(repositoryPath).filePath(QStringLiteral("conflict.txt"));
    QVERIFY(writeTextFile(filePath, QByteArrayLiteral("base\n")));
    command = runGit(repositoryPath, {QStringLiteral("add"), QStringLiteral("conflict.txt")});
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
    QVERIFY(QFileInfo(QDir(repositoryPath).filePath(QStringLiteral(".git/MERGE_HEAD"))).isFile());

    LinuxGitShell::RepositoryDiscovery discovery;
    const auto result = discoverRepository(discovery, repositoryPath);
    QVERIFY(result.repository.has_value());
    const auto repository = result.repository.value_or(LinuxGitShell::RepositoryInfo{});
    QVERIFY(repository.operations.contains(LinuxGitShell::RepositoryOperation::Merge));
}

QTEST_GUILESS_MAIN(RepositoryDiscoveryTest)

#include "RepositoryDiscoveryTest.moc"
