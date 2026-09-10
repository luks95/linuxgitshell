// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <chrono>
#include <optional>

namespace LinuxGitShell
{

enum class GitProcessCompletionReason
{
    Completed,
    FailedToStart,
    Crashed,
    Cancelled,
    TimedOut,
};

enum class GitProcessStartResult
{
    Accepted,
    Busy,
    InvalidProgram,
    InvalidTimeout,
};

struct GitProcessRequest
{
    QString program = QStringLiteral("git");
    QStringList arguments;
    QString workingDirectory;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    std::optional<std::chrono::milliseconds> timeout;
};

struct GitProcessResult
{
    QByteArray standardOutput;
    QByteArray standardError;
    std::optional<int> exitCode;
    std::optional<QProcess::ExitStatus> exitStatus;
    GitProcessCompletionReason completionReason = GitProcessCompletionReason::Completed;
    std::optional<QProcess::ProcessError> processError;
    QString errorString;
};

class GitProcessRunner final : public QObject
{
    Q_OBJECT

  public:
    explicit GitProcessRunner(QObject* parent = nullptr);
    ~GitProcessRunner() override;

    GitProcessRunner(const GitProcessRunner&) = delete;
    GitProcessRunner& operator=(const GitProcessRunner&) = delete;
    GitProcessRunner(GitProcessRunner&&) = delete;
    GitProcessRunner& operator=(GitProcessRunner&&) = delete;

    [[nodiscard]] GitProcessStartResult start(const GitProcessRequest& request);
    void cancel();
    [[nodiscard]] bool isRunning() const;

  Q_SIGNALS:
    void started();
    void standardOutputReceived(const QByteArray& bytes);
    void standardErrorReceived(const QByteArray& bytes);
    void finished(const LinuxGitShell::GitProcessResult& result);

  private:
    void readStandardOutput();
    void readStandardError();
    void processErrorOccurred(QProcess::ProcessError error);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void requestStop(GitProcessCompletionReason reason);
    void complete(GitProcessCompletionReason reason, std::optional<int> exitCode,
                  std::optional<QProcess::ExitStatus> exitStatus);

    static constexpr int terminationGracePeriodMilliseconds = 250;

    QProcess process;
    QTimer timeoutTimer;
    QTimer killTimer;
    QByteArray standardOutput;
    QByteArray standardError;
    std::optional<QProcess::ProcessError> processError;
    std::optional<GitProcessCompletionReason> requestedCompletionReason;
    bool active = false;
};

} // namespace LinuxGitShell

Q_DECLARE_METATYPE(LinuxGitShell::GitProcessCompletionReason)
Q_DECLARE_METATYPE(LinuxGitShell::GitProcessResult)
