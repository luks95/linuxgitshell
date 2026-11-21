// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/GitInventoryReader.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace LinuxGitShell
{

GitInventoryParseResult GitInventoryParser::parse(const QByteArray& output)
{
    GitInventoryParseResult result;
    if (output.isEmpty())
    {
        result.paths = QStringList();
        return result;
    }
    if (!output.endsWith('\0'))
    {
        result.error = GitInventoryParseError::MissingTerminator;
        return result;
    }

    QStringList paths;
    QSet<QString> seen;
    qsizetype start = 0;
    while (start < output.size())
    {
        const qsizetype end = output.indexOf('\0', start);
        if (end == start)
        {
            result.error = GitInventoryParseError::EmptyPath;
            return result;
        }
        const QString path = QString::fromLocal8Bit(output.constData() + start, end - start);
        if (!seen.contains(path))
        {
            seen.insert(path);
            paths.append(path);
        }
        start = end + 1;
    }
    result.paths = paths;
    return result;
}

GitInventoryReader::GitInventoryReader(QObject* parent) : QObject(parent)
{
    connect(&runner, &GitProcessRunner::finished, this, &GitInventoryReader::processFinished);
}

GitInventoryStartResult GitInventoryReader::start(const GitInventoryRequest& request)
{
    if (runner.isRunning())
    {
        return GitInventoryStartResult::Busy;
    }
    if (request.timeout.has_value() && request.timeout->count() <= 0)
    {
        return GitInventoryStartResult::InvalidTimeout;
    }

    const QFileInfo rootInfo(request.repositoryRoot);
    if (request.repositoryRoot.isEmpty() || !rootInfo.isDir())
    {
        return GitInventoryStartResult::InvalidPath;
    }

    repositoryRoot = QDir::cleanPath(rootInfo.absoluteFilePath());

    GitProcessRequest processRequest;
    processRequest.arguments = {
        QStringLiteral("--no-optional-locks"),
        QStringLiteral("-C"),
        repositoryRoot,
        QStringLiteral("ls-files"),
        QStringLiteral("--cached"),
        QStringLiteral("-z"),
    };
    processRequest.environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    processRequest.timeout = request.timeout;
    processRequest.maxOutputBytes = request.maxOutputBytes;

    switch (runner.start(processRequest))
    {
    case GitProcessStartResult::Accepted:
        return GitInventoryStartResult::Accepted;
    case GitProcessStartResult::Busy:
        return GitInventoryStartResult::Busy;
    case GitProcessStartResult::InvalidTimeout:
        return GitInventoryStartResult::InvalidTimeout;
    case GitProcessStartResult::InvalidOutputLimit:
        return GitInventoryStartResult::InvalidOutputLimit;
    case GitProcessStartResult::InvalidProgram:
        break;
    }
    return GitInventoryStartResult::RunnerRejected;
}

void GitInventoryReader::cancel() { runner.cancel(); }

bool GitInventoryReader::isRunning() const { return runner.isRunning(); }

void GitInventoryReader::processFinished(const GitProcessResult& processResult)
{
    GitInventoryResult result;
    result.repositoryRoot = repositoryRoot;
    result.gitResult = processResult;
    result.error = errorFor(processResult);
    if (result.error == GitInventoryReadError::None)
    {
        const GitInventoryParseResult parsed =
            GitInventoryParser::parse(processResult.standardOutput);
        result.parseError = parsed.error;
        if (parsed.paths.has_value())
        {
            result.paths = parsed.paths;
        }
        else
        {
            result.error = GitInventoryReadError::ParseFailure;
        }
    }
    Q_EMIT finished(result);
}

GitInventoryReadError GitInventoryReader::errorFor(const GitProcessResult& processResult)
{
    switch (processResult.completionReason)
    {
    case GitProcessCompletionReason::FailedToStart:
        return GitInventoryReadError::GitUnavailable;
    case GitProcessCompletionReason::Cancelled:
        return GitInventoryReadError::Cancelled;
    case GitProcessCompletionReason::TimedOut:
        return GitInventoryReadError::TimedOut;
    case GitProcessCompletionReason::OutputLimitExceeded:
        return GitInventoryReadError::OutputLimitExceeded;
    case GitProcessCompletionReason::Crashed:
        return GitInventoryReadError::GitFailure;
    case GitProcessCompletionReason::Completed:
        break;
    }

    if (processResult.exitCode.value_or(-1) == 0)
    {
        return GitInventoryReadError::None;
    }
    const QByteArray errorOutput = processResult.standardError.toLower();
    if (errorOutput.contains("not a git repository"))
    {
        return GitInventoryReadError::NotRepository;
    }
    if (errorOutput.contains("permission denied"))
    {
        return GitInventoryReadError::PermissionDenied;
    }
    return GitInventoryReadError::GitFailure;
}

} // namespace LinuxGitShell
