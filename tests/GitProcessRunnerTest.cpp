// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/GitCommandSanitizer.h"
#include "linuxgitshell/gitcore/GitProcessRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <chrono>

using namespace std::chrono_literals;

class GitProcessRunnerTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    void capturesOutputAndExitCode();
    void preservesArgumentsEnvironmentAndWorkingDirectory();
    void reportsNonZeroExitAsCompleted();
    void reportsStartupFailure();
    void cancelsAnActiveProcess();
    void distinguishesTimeoutFromCancellation();
    void rejectsInvalidAndConcurrentRequestsAndCanBeReused();
    void sanitizesCredentialBearingArguments();

  private:
    [[nodiscard]] static QString helperPath();
    [[nodiscard]] static LinuxGitShell::GitProcessResult takeResult(QSignalSpy& spy);
};

void GitProcessRunnerTest::initTestCase()
{
    qRegisterMetaType<LinuxGitShell::GitProcessResult>();
    QVERIFY2(!helperPath().isEmpty(), "GitProcessTestHelper was not found beside the test binary");
}

QString GitProcessRunnerTest::helperPath()
{
    return QStandardPaths::findExecutable(QStringLiteral("GitProcessTestHelper"),
                                          {QCoreApplication::applicationDirPath()});
}

LinuxGitShell::GitProcessResult GitProcessRunnerTest::takeResult(QSignalSpy& spy)
{
    return qvariant_cast<LinuxGitShell::GitProcessResult>(spy.takeFirst().at(0));
}

void GitProcessRunnerTest::capturesOutputAndExitCode()
{
    LinuxGitShell::GitProcessRunner runner;
    QSignalSpy startedSpy(&runner, &LinuxGitShell::GitProcessRunner::started);
    QSignalSpy stdoutSpy(&runner, &LinuxGitShell::GitProcessRunner::standardOutputReceived);
    QSignalSpy stderrSpy(&runner, &LinuxGitShell::GitProcessRunner::standardErrorReceived);
    QSignalSpy finishedSpy(&runner, &LinuxGitShell::GitProcessRunner::finished);

    LinuxGitShell::GitProcessRequest request;
    request.program = helperPath();
    request.arguments = {QStringLiteral("--streams")};

    QCOMPARE(runner.start(request), LinuxGitShell::GitProcessStartResult::Accepted);
    QCOMPARE(finishedSpy.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);

    const auto result = takeResult(finishedSpy);
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(!stdoutSpy.isEmpty());
    QVERIFY(!stderrSpy.isEmpty());
    QCOMPARE(result.standardOutput, QByteArrayLiteral("stdout-data"));
    QCOMPARE(result.standardError, QByteArrayLiteral("stderr-data"));
    QCOMPARE(result.completionReason, LinuxGitShell::GitProcessCompletionReason::Completed);
    QVERIFY(result.exitCode.has_value());
    QCOMPARE(*result.exitCode, 0);
    QVERIFY(result.exitStatus.has_value());
    QCOMPARE(*result.exitStatus, QProcess::NormalExit);
}

void GitProcessRunnerTest::preservesArgumentsEnvironmentAndWorkingDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString workingDirectory =
        temporaryDirectory.filePath(QStringLiteral("path with spaces/Ünicode/-leading"));
    QVERIFY(QDir().mkpath(workingDirectory));

    LinuxGitShell::GitProcessRunner runner;
    QSignalSpy finishedSpy(&runner, &LinuxGitShell::GitProcessRunner::finished);
    LinuxGitShell::GitProcessRequest request;
    request.program = helperPath();
    request.arguments = {QStringLiteral("--context"), QStringLiteral("LINUXGITSHELL_TEST_VALUE"),
                         QStringLiteral("argument with spaces"), QStringLiteral("unicøde"),
                         QStringLiteral("-leading")};
    request.workingDirectory = workingDirectory;
    request.environment.insert(QStringLiteral("LINUXGITSHELL_TEST_VALUE"),
                               QStringLiteral("välue with spaces"));

    QCOMPARE(runner.start(request), LinuxGitShell::GitProcessStartResult::Accepted);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    const auto result = takeResult(finishedSpy);
    QCOMPARE(result.completionReason, LinuxGitShell::GitProcessCompletionReason::Completed);

    const QJsonObject context = QJsonDocument::fromJson(result.standardOutput).object();
    QCOMPARE(QDir::cleanPath(context.value(QStringLiteral("workingDirectory")).toString()),
             QDir::cleanPath(workingDirectory));
    QCOMPARE(context.value(QStringLiteral("environment")).toString(),
             QStringLiteral("välue with spaces"));
    const QJsonArray arguments = context.value(QStringLiteral("arguments")).toArray();
    QCOMPARE(arguments.size(), 3);
    QCOMPARE(arguments.at(0).toString(), QStringLiteral("argument with spaces"));
    QCOMPARE(arguments.at(1).toString(), QStringLiteral("unicøde"));
    QCOMPARE(arguments.at(2).toString(), QStringLiteral("-leading"));
}

void GitProcessRunnerTest::reportsNonZeroExitAsCompleted()
{
    LinuxGitShell::GitProcessRunner runner;
    QSignalSpy finishedSpy(&runner, &LinuxGitShell::GitProcessRunner::finished);
    LinuxGitShell::GitProcessRequest request;
    request.program = helperPath();
    request.arguments = {QStringLiteral("--exit"), QStringLiteral("23")};

    QCOMPARE(runner.start(request), LinuxGitShell::GitProcessStartResult::Accepted);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    const auto result = takeResult(finishedSpy);
    QCOMPARE(result.completionReason, LinuxGitShell::GitProcessCompletionReason::Completed);
    QVERIFY(result.exitCode.has_value());
    QCOMPARE(*result.exitCode, 23);
    QVERIFY(result.exitStatus.has_value());
    QCOMPARE(*result.exitStatus, QProcess::NormalExit);
}

void GitProcessRunnerTest::reportsStartupFailure()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    LinuxGitShell::GitProcessRunner runner;
    QSignalSpy finishedSpy(&runner, &LinuxGitShell::GitProcessRunner::finished);
    LinuxGitShell::GitProcessRequest request;
    request.program = temporaryDirectory.filePath(QStringLiteral("missing-git-executable"));

    QCOMPARE(runner.start(request), LinuxGitShell::GitProcessStartResult::Accepted);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    const auto result = takeResult(finishedSpy);
    QCOMPARE(result.completionReason, LinuxGitShell::GitProcessCompletionReason::FailedToStart);
    QVERIFY(!result.exitCode.has_value());
    QVERIFY(result.processError.has_value());
    QCOMPARE(*result.processError, QProcess::FailedToStart);
    QVERIFY(!result.errorString.isEmpty());
}

void GitProcessRunnerTest::cancelsAnActiveProcess()
{
    LinuxGitShell::GitProcessRunner runner;
    QSignalSpy stdoutSpy(&runner, &LinuxGitShell::GitProcessRunner::standardOutputReceived);
    QSignalSpy finishedSpy(&runner, &LinuxGitShell::GitProcessRunner::finished);
    LinuxGitShell::GitProcessRequest request;
    request.program = helperPath();
    request.arguments = {QStringLiteral("--wait")};

    QCOMPARE(runner.start(request), LinuxGitShell::GitProcessStartResult::Accepted);
    QTRY_VERIFY_WITH_TIMEOUT(!stdoutSpy.isEmpty(), 5000);
    runner.cancel();
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    QCOMPARE(takeResult(finishedSpy).completionReason,
             LinuxGitShell::GitProcessCompletionReason::Cancelled);
}

void GitProcessRunnerTest::distinguishesTimeoutFromCancellation()
{
    LinuxGitShell::GitProcessRunner runner;
    QSignalSpy finishedSpy(&runner, &LinuxGitShell::GitProcessRunner::finished);
    LinuxGitShell::GitProcessRequest request;
    request.program = helperPath();
    request.arguments = {QStringLiteral("--wait")};
    request.timeout = 100ms;

    QCOMPARE(runner.start(request), LinuxGitShell::GitProcessStartResult::Accepted);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    QCOMPARE(takeResult(finishedSpy).completionReason,
             LinuxGitShell::GitProcessCompletionReason::TimedOut);
}

void GitProcessRunnerTest::rejectsInvalidAndConcurrentRequestsAndCanBeReused()
{
    LinuxGitShell::GitProcessRunner runner;
    LinuxGitShell::GitProcessRequest invalidRequest;
    invalidRequest.program.clear();
    QCOMPARE(runner.start(invalidRequest), LinuxGitShell::GitProcessStartResult::InvalidProgram);

    invalidRequest.program = helperPath();
    invalidRequest.timeout = 0ms;
    QCOMPARE(runner.start(invalidRequest), LinuxGitShell::GitProcessStartResult::InvalidTimeout);

    QSignalSpy stdoutSpy(&runner, &LinuxGitShell::GitProcessRunner::standardOutputReceived);
    QSignalSpy finishedSpy(&runner, &LinuxGitShell::GitProcessRunner::finished);
    LinuxGitShell::GitProcessRequest request;
    request.program = helperPath();
    request.arguments = {QStringLiteral("--wait")};
    QCOMPARE(runner.start(request), LinuxGitShell::GitProcessStartResult::Accepted);
    QCOMPARE(runner.start(request), LinuxGitShell::GitProcessStartResult::Busy);
    QTRY_VERIFY_WITH_TIMEOUT(!stdoutSpy.isEmpty(), 5000);
    runner.cancel();
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    takeResult(finishedSpy);

    request.arguments = {QStringLiteral("--exit"), QStringLiteral("0")};
    request.timeout.reset();
    QCOMPARE(runner.start(request), LinuxGitShell::GitProcessStartResult::Accepted);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    QCOMPARE(takeResult(finishedSpy).completionReason,
             LinuxGitShell::GitProcessCompletionReason::Completed);
}

void GitProcessRunnerTest::sanitizesCredentialBearingArguments()
{
    const QString credentialUrl =
        QStringLiteral("https://alice") + QStringLiteral(":secret@example.com/repository.git");
    const QStringList original = {
        credentialUrl,
        QStringLiteral("https://example.com/repository?access_token=abc&keep=yes"),
        QStringLiteral("--password=hunter2"), QStringLiteral("--token"),
        QStringLiteral("token-value"), QStringLiteral("plain")};

    const QStringList sanitized = LinuxGitShell::sanitizeGitArguments(original);

    QCOMPARE(original.at(2), QStringLiteral("--password=hunter2"));
    QCOMPARE(sanitized.size(), original.size());
    QVERIFY(!sanitized.join(QLatin1Char(' ')).contains(QStringLiteral("alice")));
    QVERIFY(!sanitized.join(QLatin1Char(' ')).contains(QStringLiteral("secret")));
    QVERIFY(!sanitized.join(QLatin1Char(' ')).contains(QStringLiteral("hunter2")));
    QVERIFY(!sanitized.join(QLatin1Char(' ')).contains(QStringLiteral("token-value")));
    QVERIFY(!sanitized.join(QLatin1Char(' ')).contains(QStringLiteral("abc")));
    QVERIFY(sanitized.join(QLatin1Char(' ')).contains(QStringLiteral("REDACTED")));
    QCOMPARE(sanitized.constLast(), QStringLiteral("plain"));
}

QTEST_GUILESS_MAIN(GitProcessRunnerTest)

#include "GitProcessRunnerTest.moc"
