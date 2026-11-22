// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/GitProcessRunner.h"

namespace LinuxGitShell
{

GitProcessRunner::GitProcessRunner(QObject* parent) : QObject(parent)
{
    timeoutTimer.setSingleShot(true);
    killTimer.setSingleShot(true);

    connect(&process, &QProcess::started, this, &GitProcessRunner::started);
    connect(&process, &QProcess::readyReadStandardOutput, this,
            &GitProcessRunner::readStandardOutput);
    connect(&process, &QProcess::readyReadStandardError, this,
            &GitProcessRunner::readStandardError);
    connect(&process, &QProcess::errorOccurred, this, &GitProcessRunner::processErrorOccurred);
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            &GitProcessRunner::processFinished);
    connect(&timeoutTimer, &QTimer::timeout, this,
            [this]() { requestStop(GitProcessCompletionReason::TimedOut); });
    connect(&killTimer, &QTimer::timeout, this,
            [this]()
            {
                if (active && process.state() != QProcess::NotRunning)
                {
                    process.kill();
                }
            });
}

GitProcessRunner::~GitProcessRunner()
{
    timeoutTimer.stop();
    killTimer.stop();
    if (process.state() != QProcess::NotRunning)
    {
        disconnect(&process, nullptr, this, nullptr);
        process.kill();
    }
}

GitProcessStartResult GitProcessRunner::start(const GitProcessRequest& request)
{
    if (active)
    {
        return GitProcessStartResult::Busy;
    }
    if (request.program.trimmed().isEmpty())
    {
        return GitProcessStartResult::InvalidProgram;
    }
    if (request.timeout.has_value() && request.timeout->count() <= 0)
    {
        return GitProcessStartResult::InvalidTimeout;
    }
    if (request.maxOutputBytes.has_value() && *request.maxOutputBytes <= 0)
    {
        return GitProcessStartResult::InvalidOutputLimit;
    }

    standardOutput.clear();
    standardError.clear();
    processError.reset();
    requestedCompletionReason.reset();
    outputLimit = request.maxOutputBytes;
    active = true;

    process.setProgram(request.program);
    process.setArguments(request.arguments);
    process.setWorkingDirectory(request.workingDirectory);
    process.setProcessEnvironment(request.environment);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();

    if (active && request.timeout.has_value())
    {
        timeoutTimer.start(*request.timeout);
    }

    return GitProcessStartResult::Accepted;
}

void GitProcessRunner::cancel() { requestStop(GitProcessCompletionReason::Cancelled); }

bool GitProcessRunner::isRunning() const { return active; }

void GitProcessRunner::readStandardOutput()
{
    const QByteArray bytes = process.readAllStandardOutput();
    if (bytes.isEmpty())
    {
        return;
    }

    appendBounded(standardOutput, bytes, true);
}

void GitProcessRunner::readStandardError()
{
    const QByteArray bytes = process.readAllStandardError();
    if (bytes.isEmpty())
    {
        return;
    }

    appendBounded(standardError, bytes, false);
}

void GitProcessRunner::appendBounded(QByteArray& target, const QByteArray& bytes,
                                     bool standardOutputStream)
{
    QByteArray accepted = bytes;
    bool exceeded = false;
    if (outputLimit.has_value())
    {
        const qint64 used = standardOutput.size() + standardError.size();
        const qint64 remaining = qMax<qint64>(0, *outputLimit - used);
        if (bytes.size() > remaining)
        {
            accepted = bytes.left(remaining);
            exceeded = true;
        }
    }

    if (!accepted.isEmpty())
    {
        target.append(accepted);
        if (standardOutputStream)
        {
            Q_EMIT standardOutputReceived(accepted);
        }
        else
        {
            Q_EMIT standardErrorReceived(accepted);
        }
    }
    if (exceeded)
    {
        requestStop(GitProcessCompletionReason::OutputLimitExceeded);
    }
}

void GitProcessRunner::processErrorOccurred(QProcess::ProcessError error)
{
    processError = error;
    if (error == QProcess::FailedToStart)
    {
        complete(requestedCompletionReason.value_or(GitProcessCompletionReason::FailedToStart),
                 std::nullopt, std::nullopt);
    }
}

void GitProcessRunner::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const GitProcessCompletionReason reason = requestedCompletionReason.value_or(
        exitStatus == QProcess::CrashExit ? GitProcessCompletionReason::Crashed
                                          : GitProcessCompletionReason::Completed);
    complete(reason, exitCode, exitStatus);
}

void GitProcessRunner::requestStop(GitProcessCompletionReason reason)
{
    if (!active || requestedCompletionReason.has_value())
    {
        return;
    }

    requestedCompletionReason = reason;
    timeoutTimer.stop();
    process.terminate();
    killTimer.start(terminationGracePeriodMilliseconds);
}

void GitProcessRunner::complete(GitProcessCompletionReason reason, std::optional<int> exitCode,
                                std::optional<QProcess::ExitStatus> exitStatus)
{
    if (!active)
    {
        return;
    }

    readStandardOutput();
    readStandardError();
    timeoutTimer.stop();
    killTimer.stop();

    GitProcessResult result;
    result.standardOutput = standardOutput;
    result.standardError = standardError;
    result.exitCode = exitCode;
    result.exitStatus = exitStatus;
    result.completionReason = reason;
    result.processError = processError;
    if (processError.has_value())
    {
        result.errorString = process.errorString();
    }

    active = false;
    requestedCompletionReason.reset();
    Q_EMIT finished(result);
}

} // namespace LinuxGitShell
