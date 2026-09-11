// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/GitStatusReader.h"

#include <QDir>
#include <QFileInfo>

namespace LinuxGitShell
{

GitStatusReader::GitStatusReader(QObject* parent) : QObject(parent)
{
    connect(&runner, &GitProcessRunner::finished, this, &GitStatusReader::processFinished);
}

GitStatusStartResult GitStatusReader::start(const GitStatusRequest& request)
{
    if (runner.isRunning())
    {
        return GitStatusStartResult::Busy;
    }
    if (request.timeout.has_value() && request.timeout->count() <= 0)
    {
        return GitStatusStartResult::InvalidTimeout;
    }

    const QFileInfo pathInfo(request.path);
    if (request.path.isEmpty() || !pathInfo.exists())
    {
        return GitStatusStartResult::InvalidPath;
    }

    requestedPath = QDir::cleanPath(pathInfo.absoluteFilePath());
    const QString workingDirectory =
        pathInfo.isDir() ? requestedPath : QDir::cleanPath(pathInfo.absolutePath());

    GitProcessRequest processRequest;
    processRequest.arguments = {
        QStringLiteral("--no-optional-locks"),
        QStringLiteral("-C"),
        workingDirectory,
        QStringLiteral("status"),
        QStringLiteral("--porcelain=v2"),
        QStringLiteral("-z"),
        QStringLiteral("--branch"),
        QStringLiteral("--show-stash"),
        QStringLiteral("--untracked-files=all"),
        QStringLiteral("--ignore-submodules=none"),
        request.detectRenames ? QStringLiteral("--renames") : QStringLiteral("--no-renames"),
    };
    if (request.includeIgnored)
    {
        processRequest.arguments.append(QStringLiteral("--ignored=matching"));
    }
    processRequest.environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    processRequest.timeout = request.timeout;

    switch (runner.start(processRequest))
    {
    case GitProcessStartResult::Accepted:
        return GitStatusStartResult::Accepted;
    case GitProcessStartResult::Busy:
        return GitStatusStartResult::Busy;
    case GitProcessStartResult::InvalidTimeout:
        return GitStatusStartResult::InvalidTimeout;
    case GitProcessStartResult::InvalidProgram:
        break;
    }
    return GitStatusStartResult::RunnerRejected;
}

void GitStatusReader::cancel() { runner.cancel(); }

bool GitStatusReader::isRunning() const { return runner.isRunning(); }

void GitStatusReader::processFinished(const GitProcessResult& processResult)
{
    GitStatusResult result;
    result.requestedPath = requestedPath;
    result.gitResult = processResult;
    result.error = errorFor(processResult);
    if (result.error == GitStatusReadError::None)
    {
        const GitStatusParseResult parsed = GitStatusParser::parse(processResult.standardOutput);
        result.parseError = parsed.error;
        if (parsed.status.has_value())
        {
            result.status = parsed.status;
        }
        else
        {
            result.error = GitStatusReadError::ParseFailure;
        }
    }
    Q_EMIT finished(result);
}

GitStatusReadError GitStatusReader::errorFor(const GitProcessResult& processResult)
{
    switch (processResult.completionReason)
    {
    case GitProcessCompletionReason::FailedToStart:
        return GitStatusReadError::GitUnavailable;
    case GitProcessCompletionReason::Cancelled:
        return GitStatusReadError::Cancelled;
    case GitProcessCompletionReason::TimedOut:
        return GitStatusReadError::TimedOut;
    case GitProcessCompletionReason::Crashed:
        return GitStatusReadError::GitFailure;
    case GitProcessCompletionReason::Completed:
        break;
    }

    if (processResult.exitCode.value_or(-1) == 0)
    {
        return GitStatusReadError::None;
    }
    const QByteArray errorOutput = processResult.standardError.toLower();
    if (errorOutput.contains("not a git repository"))
    {
        return GitStatusReadError::NotRepository;
    }
    if (errorOutput.contains("must be run in a work tree"))
    {
        return GitStatusReadError::NoWorkingTree;
    }
    if (errorOutput.contains("permission denied"))
    {
        return GitStatusReadError::PermissionDenied;
    }
    return GitStatusReadError::GitFailure;
}

} // namespace LinuxGitShell
