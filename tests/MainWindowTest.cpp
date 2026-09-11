// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "MainWindow.h"

#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

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
    process.setArguments(arguments);
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

[[nodiscard]] bool writeFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) >= 0;
}

[[nodiscard]] CommandResult initializeRepository(const QString& repositoryPath)
{
    CommandResult result = runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral("-b"),
                                                   QStringLiteral("main"), QStringLiteral(".")});
    if (!commandSucceeded(result))
    {
        return result;
    }
    result = runGit(repositoryPath, {QStringLiteral("config"), QStringLiteral("user.name"),
                                     QStringLiteral("LinuxGitShell Test")});
    if (!commandSucceeded(result))
    {
        return result;
    }
    return runGit(repositoryPath, {QStringLiteral("config"), QStringLiteral("user.email"),
                                   QStringLiteral("test@example.invalid")});
}

[[nodiscard]] bool waitForLoad(LinuxGitShell::MainWindow& window, bool expectedSuccess)
{
    QSignalSpy loadSpy(&window, &LinuxGitShell::MainWindow::loadFinished);
    if (loadSpy.isEmpty() && !loadSpy.wait(20000))
    {
        return false;
    }
    return loadSpy.takeLast().constFirst().toBool() == expectedSuccess;
}

template <typename Widget> [[nodiscard]] Widget* requiredChild(QWidget& window, const char* name)
{
    return window.findChild<Widget*>(QString::fromLatin1(name));
}

} // namespace

class MainWindowTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    void displaysRepositoryStatusAndConfiguration();
    void displaysCleanRepository();
    void displaysRepositoryFailureDiagnostics();
};

void MainWindowTest::initTestCase()
{
    qputenv("GIT_CONFIG_NOSYSTEM", QByteArrayLiteral("1"));
    qputenv("GIT_CONFIG_GLOBAL", QByteArrayLiteral("/dev/null"));
    qputenv("GIT_TERMINAL_PROMPT", QByteArrayLiteral("0"));
    const CommandResult version = runGit(QDir::tempPath(), {QStringLiteral("--version")});
    QVERIFY2(commandSucceeded(version), version.standardError.constData());
}

void MainWindowTest::displaysRepositoryStatusAndConfiguration()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("repository"));
    QVERIFY(QDir().mkpath(repositoryPath));

    CommandResult command = initializeRepository(repositoryPath);
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(writeFile(QDir(repositoryPath).filePath(QStringLiteral("tracked.txt")),
                      QByteArrayLiteral("original\n")));
    command = runGit(repositoryPath, {QStringLiteral("add"), QStringLiteral("tracked.txt")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath,
                     {QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("initial")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    QVERIFY(writeFile(QDir(repositoryPath).filePath(QStringLiteral("tracked.txt")),
                      QByteArrayLiteral("modified\n")));
    QVERIFY(writeFile(QDir(repositoryPath).filePath(QStringLiteral("staged.txt")),
                      QByteArrayLiteral("staged\n")));
    command = runGit(repositoryPath, {QStringLiteral("add"), QStringLiteral("staged.txt")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    QVERIFY(writeFile(QDir(repositoryPath).filePath(QStringLiteral("untracked.txt")),
                      QByteArrayLiteral("untracked\n")));

    LinuxGitShell::MainWindow window(repositoryPath);
    QVERIFY(waitForLoad(window, true));

    const auto* root = requiredChild<QLabel>(window, "rootValue");
    const auto* branch = requiredChild<QLabel>(window, "branchValue");
    const auto* status = requiredChild<QLabel>(window, "statusValue");
    const auto* config = requiredChild<QTreeWidget>(window, "configTable");
    QVERIFY(root != nullptr);
    QVERIFY(branch != nullptr);
    QVERIFY(status != nullptr);
    QVERIFY(config != nullptr);
    QCOMPARE(root->text(), QDir::cleanPath(repositoryPath));
    QCOMPARE(branch->text(), QStringLiteral("main"));
    QVERIFY(status->text().contains(QStringLiteral("Staged: 1")));
    QVERIFY(status->text().contains(QStringLiteral("Modified: 1")));
    QVERIFY(status->text().contains(QStringLiteral("Untracked: 1")));
    QVERIFY(status->text().contains(QStringLiteral("Conflicts: 0")));

    bool foundUserName = false;
    for (int index = 0; index < config->topLevelItemCount(); ++index)
    {
        const QTreeWidgetItem* item = config->topLevelItem(index);
        if (item->text(2) == QStringLiteral("user.name"))
        {
            QCOMPARE(item->text(0), QStringLiteral("local"));
            QCOMPARE(item->text(3), QStringLiteral("LinuxGitShell Test"));
            foundUserName = true;
        }
    }
    QVERIFY(foundUserName);
}

void MainWindowTest::displaysCleanRepository()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("clean"));
    QVERIFY(QDir().mkpath(repositoryPath));
    const CommandResult command = initializeRepository(repositoryPath);
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    LinuxGitShell::MainWindow window(repositoryPath);
    QVERIFY(waitForLoad(window, true));
    const auto* status = requiredChild<QLabel>(window, "statusValue");
    QVERIFY(status != nullptr);
    QCOMPARE(status->text(), QStringLiteral("Working tree clean"));
}

void MainWindowTest::displaysRepositoryFailureDiagnostics()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    LinuxGitShell::MainWindow window(temporaryDirectory.path());
    QVERIFY(waitForLoad(window, false));
    const auto* state = requiredChild<QLabel>(window, "stateLabel");
    const auto* diagnostics = requiredChild<QPlainTextEdit>(window, "diagnosticOutput");
    QVERIFY(state != nullptr);
    QVERIFY(diagnostics != nullptr);
    QCOMPARE(state->text(), QStringLiteral("Not a Git repository."));
    QVERIFY(!diagnostics->toPlainText().isEmpty());
}

QTEST_MAIN(MainWindowTest)

#include "MainWindowTest.moc"
