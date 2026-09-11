// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/GitConfigReader.h"

#include <QDir>
#include <QFileInfo>

namespace LinuxGitShell
{

GitConfigReader::GitConfigReader(QObject* parent) : QObject(parent)
{
    connect(&runner, &GitProcessRunner::finished, this, &GitConfigReader::processFinished);
}

GitConfigStartResult GitConfigReader::start(const GitConfigRequest& request)
{
    if (runner.isRunning())
        return GitConfigStartResult::Busy;
    if (request.timeout.has_value() && request.timeout->count() <= 0)
        return GitConfigStartResult::InvalidTimeout;
    const QFileInfo pathInfo(request.path);
    if (request.path.isEmpty() || !pathInfo.exists())
        return GitConfigStartResult::InvalidPath;

    requestedPath = QDir::cleanPath(pathInfo.absoluteFilePath());
    const QString workingDirectory =
        pathInfo.isDir() ? requestedPath : QDir::cleanPath(pathInfo.absolutePath());
    GitProcessRequest processRequest;
    processRequest.arguments = {QStringLiteral("--no-optional-locks"),
                                QStringLiteral("-C"),
                                workingDirectory,
                                QStringLiteral("config"),
                                QStringLiteral("--null"),
                                QStringLiteral("--list"),
                                QStringLiteral("--show-origin"),
                                QStringLiteral("--show-scope")};
    if (request.includeFiles)
        processRequest.arguments.append(QStringLiteral("--includes"));
    processRequest.environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    processRequest.timeout = request.timeout;

    switch (runner.start(processRequest))
    {
    case GitProcessStartResult::Accepted:
        return GitConfigStartResult::Accepted;
    case GitProcessStartResult::Busy:
        return GitConfigStartResult::Busy;
    case GitProcessStartResult::InvalidTimeout:
        return GitConfigStartResult::InvalidTimeout;
    case GitProcessStartResult::InvalidProgram:
        break;
    }
    return GitConfigStartResult::RunnerRejected;
}

void GitConfigReader::cancel() { runner.cancel(); }

bool GitConfigReader::isRunning() const { return runner.isRunning(); }

void GitConfigReader::processFinished(const GitProcessResult& processResult)
{
    GitConfigResult result;
    result.requestedPath = requestedPath;
    result.gitResult = processResult;
    result.gitResult.standardOutput.clear();
    result.error = errorFor(processResult);
    if (result.error == GitConfigReadError::None)
    {
        const GitConfigParseResult parsed = GitConfigParser::parse(processResult.standardOutput);
        result.parseError = parsed.error;
        result.gitResult.standardOutput = parsed.sanitizedOutput;
        if (parsed.config.has_value())
            result.config = parsed.config;
        else
            result.error = GitConfigReadError::ParseFailure;
    }
    Q_EMIT finished(result);
}

GitConfigReadError GitConfigReader::errorFor(const GitProcessResult& processResult)
{
    switch (processResult.completionReason)
    {
    case GitProcessCompletionReason::FailedToStart:
        return GitConfigReadError::GitUnavailable;
    case GitProcessCompletionReason::Cancelled:
        return GitConfigReadError::Cancelled;
    case GitProcessCompletionReason::TimedOut:
        return GitConfigReadError::TimedOut;
    case GitProcessCompletionReason::Crashed:
        return GitConfigReadError::GitFailure;
    case GitProcessCompletionReason::Completed:
        break;
    }
    if (processResult.exitCode.value_or(-1) == 0)
        return GitConfigReadError::None;
    if (processResult.standardError.toLower().contains("permission denied"))
        return GitConfigReadError::PermissionDenied;
    return GitConfigReadError::GitFailure;
}

} // namespace LinuxGitShell
