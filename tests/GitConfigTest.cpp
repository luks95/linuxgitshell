// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/GitConfig.h"
#include "linuxgitshell/gitcore/GitConfigReader.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <chrono>
#include <optional>
#include <utility>

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

class EnvironmentOverride final
{
  public:
    EnvironmentOverride(QByteArray name, std::optional<QByteArray> value)
        : name(std::move(name)), previousValue(qgetenv(this->name.constData())),
          wasSet(qEnvironmentVariableIsSet(this->name.constData()))
    {
        if (value.has_value())
            qputenv(this->name.constData(), *value);
        else
            qunsetenv(this->name.constData());
    }

    ~EnvironmentOverride()
    {
        if (wasSet)
            qputenv(name.constData(), previousValue);
        else
            qunsetenv(name.constData());
    }

    EnvironmentOverride(const EnvironmentOverride&) = delete;
    EnvironmentOverride& operator=(const EnvironmentOverride&) = delete;

  private:
    QByteArray name;
    QByteArray previousValue;
    bool wasSet;
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

[[nodiscard]] QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

[[nodiscard]] LinuxGitShell::GitConfigResult readConfig(LinuxGitShell::GitConfigReader& reader,
                                                        const QString& path)
{
    QSignalSpy finishedSpy(&reader, &LinuxGitShell::GitConfigReader::finished);
    LinuxGitShell::GitConfigRequest request;
    request.path = path;
    if (reader.start(request) != LinuxGitShell::GitConfigStartResult::Accepted)
        return {};
    if (finishedSpy.isEmpty() && !finishedSpy.wait(10000))
        return {};
    return qvariant_cast<LinuxGitShell::GitConfigResult>(finishedSpy.takeFirst().at(0));
}

} // namespace

class GitConfigTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    void parsesScopesOriginsRepeatedAndMultilineValues();
    void sanitizesSensitiveConfiguration();
    void rejectsMalformedOutput_data();
    void rejectsMalformedOutput();
    void readsSystemGlobalLocalAndWorktreeConfigWithoutWriting();
    void rejectsInvalidReaderRequests();
};

void GitConfigTest::initTestCase()
{
    qputenv("GIT_CONFIG_NOSYSTEM", QByteArrayLiteral("1"));
    qputenv("GIT_CONFIG_GLOBAL", QByteArrayLiteral("/dev/null"));
    qputenv("GIT_TERMINAL_PROMPT", QByteArrayLiteral("0"));
    qRegisterMetaType<LinuxGitShell::GitConfigResult>();
    const CommandResult version = runGit(QDir::tempPath(), {QStringLiteral("--version")});
    QVERIFY2(commandSucceeded(version), version.standardError.constData());
}

void GitConfigTest::parsesScopesOriginsRepeatedAndMultilineValues()
{
    QByteArray output;
    output += QByteArrayLiteral("global\0file:/tmp/global-config\0test.multi\none\0");
    output += QByteArrayLiteral("local\0file:.git/config\0test.multi\ntwo\0");
    output +=
        QByteArrayLiteral("worktree\0file:.git/config.worktree\0test.multiline\nline 1\nline 2\0");
    output += QByteArrayLiteral("future\0future-origin:\0test.empty\n\0");

    const auto result = LinuxGitShell::GitConfigParser::parse(output);
    QCOMPARE(result.error, LinuxGitShell::GitConfigParseError::None);
    QVERIFY(result.config.has_value());
    const auto config = result.config.value_or(LinuxGitShell::GitConfigSnapshot{});
    QCOMPARE(config.entries.size(), 4);
    QCOMPARE(config.entriesForKey(QStringLiteral("TEST.MULTI")).size(), 2);
    QCOMPARE(config.entries.at(0).scope, LinuxGitShell::GitConfigScope::Global);
    QCOMPARE(config.entries.at(0).originType, LinuxGitShell::GitConfigOriginType::File);
    QCOMPARE(config.entries.at(2).scope, LinuxGitShell::GitConfigScope::Worktree);
    QCOMPARE(config.entries.at(2).value, QStringLiteral("line 1\nline 2"));
    QCOMPARE(config.entries.at(3).scope, LinuxGitShell::GitConfigScope::Unknown);
    QCOMPARE(config.entries.at(3).originType, LinuxGitShell::GitConfigOriginType::Unknown);
    QVERIFY(config.entries.at(3).value.isEmpty());
    QCOMPARE(result.sanitizedOutput, output);
}

void GitConfigTest::sanitizesSensitiveConfiguration()
{
    const QByteArray credentialUrl =
        QByteArrayLiteral("https://alice") +
        QByteArrayLiteral(":credential@example.invalid/repository?access_token=value");
    const QByteArray authorization =
        QByteArrayLiteral("Authorization: Bearer ") + QByteArrayLiteral("credential-value");
    QByteArray output =
        QByteArrayLiteral("local\0file:.git/config\0remote.origin.url\n") + credentialUrl + '\0';
    output +=
        QByteArrayLiteral("local\0file:.git/config\0http.extraheader\n") + authorization + '\0';

    const auto result = LinuxGitShell::GitConfigParser::parse(output);
    QCOMPARE(result.error, LinuxGitShell::GitConfigParseError::None);
    const auto config = result.config.value_or(LinuxGitShell::GitConfigSnapshot{});
    QCOMPARE(config.entries.size(), 2);
    QVERIFY(config.entries.at(0).isSensitive);
    QVERIFY(config.entries.at(1).isSensitive);
    QVERIFY(!config.entries.at(0).value.contains(QStringLiteral("alice")));
    QCOMPARE(config.entries.at(1).value, QStringLiteral("REDACTED"));
    QVERIFY(!result.sanitizedOutput.contains(credentialUrl));
    QVERIFY(!result.sanitizedOutput.contains(authorization));
    QVERIFY(result.sanitizedOutput.contains("REDACTED"));
}

void GitConfigTest::rejectsMalformedOutput_data()
{
    QTest::addColumn<QByteArray>("output");
    QTest::addColumn<LinuxGitShell::GitConfigParseError>("error");

    QTest::newRow("missing-terminator") << QByteArrayLiteral("local\0file:.git/config\0key\nvalue")
                                        << LinuxGitShell::GitConfigParseError::MissingTerminator;
    QTest::newRow("incomplete") << QByteArrayLiteral("local\0file:.git/config\0")
                                << LinuxGitShell::GitConfigParseError::IncompleteEntry;
    QTest::newRow("empty-scope") << QByteArrayLiteral("\0file:.git/config\0key\nvalue\0")
                                 << LinuxGitShell::GitConfigParseError::InvalidScope;
    QTest::newRow("empty-origin") << QByteArrayLiteral("local\0\0key\nvalue\0")
                                  << LinuxGitShell::GitConfigParseError::InvalidOrigin;
    QTest::newRow("missing-key-separator") << QByteArrayLiteral("local\0file:.git/config\0key\0")
                                           << LinuxGitShell::GitConfigParseError::InvalidKeyValue;
}

void GitConfigTest::rejectsMalformedOutput()
{
    QFETCH(QByteArray, output);
    QFETCH(LinuxGitShell::GitConfigParseError, error);
    const auto result = LinuxGitShell::GitConfigParser::parse(output);
    QCOMPARE(result.error, error);
    QVERIFY(!result.config.has_value());
}

void GitConfigTest::readsSystemGlobalLocalAndWorktreeConfigWithoutWriting()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryPath = temporaryDirectory.filePath(QStringLiteral("repository"));
    const QString systemPath = temporaryDirectory.filePath(QStringLiteral("system.config"));
    const QString globalPath = temporaryDirectory.filePath(QStringLiteral("global.config"));
    QVERIFY(QDir().mkpath(repositoryPath));

    CommandResult command = runGit(repositoryPath, {QStringLiteral("init"), QStringLiteral(".")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command =
        runGit(repositoryPath, {QStringLiteral("config"), QStringLiteral("--file"), systemPath,
                                QStringLiteral("test.system"), QStringLiteral("system-value")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command =
        runGit(repositoryPath, {QStringLiteral("config"), QStringLiteral("--file"), globalPath,
                                QStringLiteral("test.global"), QStringLiteral("global-value")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath, {QStringLiteral("config"), QStringLiteral("--local"),
                                      QStringLiteral("test.local"), QStringLiteral("local-value")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath,
                     {QStringLiteral("config"), QStringLiteral("--local"), QStringLiteral("--add"),
                      QStringLiteral("test.multi"), QStringLiteral("first")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath,
                     {QStringLiteral("config"), QStringLiteral("--local"), QStringLiteral("--add"),
                      QStringLiteral("test.multi"), QStringLiteral("second")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command = runGit(repositoryPath,
                     {QStringLiteral("config"), QStringLiteral("--local"),
                      QStringLiteral("extensions.worktreeConfig"), QStringLiteral("true")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());
    command =
        runGit(repositoryPath, {QStringLiteral("config"), QStringLiteral("--worktree"),
                                QStringLiteral("test.worktree"), QStringLiteral("worktree-value")});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    const QString sensitiveValue =
        QStringLiteral("Authorization: Bearer ") + QStringLiteral("credential-value");
    command = runGit(repositoryPath, {QStringLiteral("config"), QStringLiteral("--local"),
                                      QStringLiteral("http.extraheader"), sensitiveValue});
    QVERIFY2(commandSucceeded(command), command.standardError.constData());

    const QString localConfigPath = QDir(repositoryPath).filePath(QStringLiteral(".git/config"));
    const QByteArray configBefore = readFile(localConfigPath);
    QVERIFY(!configBefore.isEmpty());
    EnvironmentOverride systemEnvironment(QByteArrayLiteral("GIT_CONFIG_SYSTEM"),
                                          systemPath.toLocal8Bit());
    EnvironmentOverride globalEnvironment(QByteArrayLiteral("GIT_CONFIG_GLOBAL"),
                                          globalPath.toLocal8Bit());
    EnvironmentOverride noSystem(QByteArrayLiteral("GIT_CONFIG_NOSYSTEM"), std::nullopt);

    LinuxGitShell::GitConfigReader reader;
    const auto result = readConfig(reader, repositoryPath);
    QCOMPARE(result.error, LinuxGitShell::GitConfigReadError::None);
    QCOMPARE(result.parseError, LinuxGitShell::GitConfigParseError::None);
    QVERIFY(result.config.has_value());
    const auto config = result.config.value_or(LinuxGitShell::GitConfigSnapshot{});
    QCOMPARE(config.entriesForKey(QStringLiteral("test.system")).constFirst().scope,
             LinuxGitShell::GitConfigScope::System);
    QCOMPARE(config.entriesForKey(QStringLiteral("test.global")).constFirst().scope,
             LinuxGitShell::GitConfigScope::Global);
    QCOMPARE(config.entriesForKey(QStringLiteral("test.local")).constFirst().scope,
             LinuxGitShell::GitConfigScope::Local);
    QCOMPARE(config.entriesForKey(QStringLiteral("test.worktree")).constFirst().scope,
             LinuxGitShell::GitConfigScope::Worktree);
    QCOMPARE(config.entriesForKey(QStringLiteral("test.multi")).size(), 2);
    const auto sensitiveEntries = config.entriesForKey(QStringLiteral("http.extraheader"));
    QCOMPARE(sensitiveEntries.size(), 1);
    QVERIFY(sensitiveEntries.constFirst().isSensitive);
    QCOMPARE(sensitiveEntries.constFirst().value, QStringLiteral("REDACTED"));
    QVERIFY(!result.gitResult.standardOutput.contains(sensitiveValue.toLocal8Bit()));
    QCOMPARE(readFile(localConfigPath), configBefore);
}

void GitConfigTest::rejectsInvalidReaderRequests()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    LinuxGitShell::GitConfigReader reader;
    LinuxGitShell::GitConfigRequest request;
    request.path = temporaryDirectory.filePath(QStringLiteral("missing"));
    QCOMPARE(reader.start(request), LinuxGitShell::GitConfigStartResult::InvalidPath);
    request.path = temporaryDirectory.path();
    request.timeout = 0ms;
    QCOMPARE(reader.start(request), LinuxGitShell::GitConfigStartResult::InvalidTimeout);
}

QTEST_GUILESS_MAIN(GitConfigTest)

#include "GitConfigTest.moc"
