// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/RepositoryDiscovery.h"

#include <QDir>
#include <QFileInfo>

namespace LinuxGitShell
{
namespace
{

[[nodiscard]] QList<QByteArray> outputLines(const QByteArray& output)
{
    QList<QByteArray> lines = output.split('\n');
    while (!lines.isEmpty() && lines.constLast().isEmpty())
    {
        lines.removeLast();
    }
    for (QByteArray& line : lines)
    {
        if (line.endsWith('\r'))
        {
            line.chop(1);
        }
    }
    return lines;
}

[[nodiscard]] QString pathFromGit(const QByteArray& path)
{
    return QDir::cleanPath(QString::fromLocal8Bit(path));
}

[[nodiscard]] QString textFromGit(const QByteArray& text) { return QString::fromLocal8Bit(text); }

[[nodiscard]] bool processSucceeded(const GitProcessResult& result)
{
    return result.completionReason == GitProcessCompletionReason::Completed &&
           result.exitCode.value_or(-1) == 0;
}

} // namespace

RepositoryDiscovery::RepositoryDiscovery(QObject* parent) : QObject(parent)
{
    connect(&runner, &GitProcessRunner::finished, this, &RepositoryDiscovery::processFinished);
}

RepositoryDiscoveryStartResult RepositoryDiscovery::discover(const QString& path)
{
    if (stage != Stage::Idle)
    {
        return RepositoryDiscoveryStartResult::Busy;
    }

    const QFileInfo pathInfo(path);
    if (path.isEmpty() || !pathInfo.exists())
    {
        return RepositoryDiscoveryStartResult::InvalidPath;
    }

    result = {};
    repository = {};
    result.requestedPath = QDir::cleanPath(pathInfo.absoluteFilePath());

    // Keep the requested path for the caller, but resolve links before selecting Git's working
    // directory. In particular, the parent of a symlink to a file may be outside the repository
    // even though the target itself is inside it.
    const QString canonicalPath = pathInfo.canonicalFilePath();
    const QFileInfo discoveryPathInfo(canonicalPath.isEmpty() ? result.requestedPath
                                                              : canonicalPath);
    discoveryDirectory = discoveryPathInfo.isDir()
                             ? QDir::cleanPath(discoveryPathInfo.absoluteFilePath())
                             : QDir::cleanPath(discoveryPathInfo.absolutePath());
    stage = Stage::Probe;

    const bool started =
        startGit({QStringLiteral("rev-parse"), QStringLiteral("--path-format=absolute"),
                  QStringLiteral("--git-dir"), QStringLiteral("--git-common-dir"),
                  QStringLiteral("--is-bare-repository"), QStringLiteral("--is-inside-work-tree")});
    if (!started)
    {
        stage = Stage::Idle;
        return RepositoryDiscoveryStartResult::Busy;
    }

    return RepositoryDiscoveryStartResult::Accepted;
}

void RepositoryDiscovery::cancel() { runner.cancel(); }

bool RepositoryDiscovery::isRunning() const { return stage != Stage::Idle; }

bool RepositoryDiscovery::startGit(const QStringList& arguments)
{
    GitProcessRequest request;
    request.arguments = {QStringLiteral("-C"), discoveryDirectory};
    request.arguments.append(arguments);
    request.environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    return runner.start(request) == GitProcessStartResult::Accepted;
}

bool RepositoryDiscovery::startOperations()
{
    stage = Stage::Operations;
    return startGit({QStringLiteral("rev-parse"), QStringLiteral("--path-format=absolute"),
                     QStringLiteral("--git-path"), QStringLiteral("MERGE_HEAD"),
                     QStringLiteral("--git-path"), QStringLiteral("rebase-merge"),
                     QStringLiteral("--git-path"), QStringLiteral("rebase-apply"),
                     QStringLiteral("--git-path"), QStringLiteral("CHERRY_PICK_HEAD"),
                     QStringLiteral("--git-path"), QStringLiteral("REVERT_HEAD"),
                     QStringLiteral("--git-path"), QStringLiteral("BISECT_START"),
                     QStringLiteral("--git-path"), QStringLiteral("BISECT_LOG")});
}

bool RepositoryDiscovery::startHead()
{
    stage = Stage::Head;
    return startGit(
        {QStringLiteral("branch"), QStringLiteral("--show-current"), QStringLiteral("--no-color")});
}

bool RepositoryDiscovery::startUpstream()
{
    stage = Stage::Upstream;
    return startGit({QStringLiteral("for-each-ref"), QStringLiteral("--format=%(upstream:short)"),
                     QStringLiteral("refs/heads/") + repository.currentBranch});
}

bool RepositoryDiscovery::startAheadBehind()
{
    stage = Stage::AheadBehind;
    return startGit({QStringLiteral("rev-list"), QStringLiteral("--left-right"),
                     QStringLiteral("--count"), QStringLiteral("HEAD...@{upstream}")});
}

bool RepositoryDiscovery::startRemotes()
{
    stage = Stage::Remotes;
    return startGit({QStringLiteral("remote")});
}

void RepositoryDiscovery::processFinished(const GitProcessResult& processResult)
{
    result.gitResults.append(processResult);
    if (!processSucceeded(processResult))
    {
        finishError(errorFor(processResult));
        return;
    }

    if (stage == Stage::Probe)
    {
        if (!parseProbe(processResult))
        {
            finishError(RepositoryDiscoveryError::InvalidOutput);
            return;
        }
        if (repository.type == RepositoryType::Bare)
        {
            if (!startOperations())
            {
                finishError(RepositoryDiscoveryError::GitFailure);
            }
            return;
        }

        stage = Stage::WorkTree;
        if (!startGit({QStringLiteral("rev-parse"), QStringLiteral("--path-format=absolute"),
                       QStringLiteral("--show-toplevel"),
                       QStringLiteral("--show-superproject-working-tree")}))
        {
            finishError(RepositoryDiscoveryError::GitFailure);
        }
        return;
    }

    if (stage == Stage::WorkTree)
    {
        if (!parseWorkTree(processResult))
        {
            finishError(RepositoryDiscoveryError::InvalidOutput);
            return;
        }
        if (!startOperations())
        {
            finishError(RepositoryDiscoveryError::GitFailure);
        }
        return;
    }

    if (stage == Stage::Operations)
    {
        if (!parseOperations(processResult))
        {
            finishError(RepositoryDiscoveryError::InvalidOutput);
            return;
        }
        if (!startHead())
        {
            finishError(RepositoryDiscoveryError::GitFailure);
        }
        return;
    }

    if (stage == Stage::Head)
    {
        if (!parseHead(processResult))
        {
            finishError(RepositoryDiscoveryError::InvalidOutput);
            return;
        }
        const bool started = repository.isDetachedHead ? startRemotes() : startUpstream();
        if (!started)
        {
            finishError(RepositoryDiscoveryError::GitFailure);
        }
        return;
    }

    if (stage == Stage::Upstream)
    {
        if (!parseUpstream(processResult))
        {
            finishError(RepositoryDiscoveryError::InvalidOutput);
            return;
        }
        const bool started = repository.upstream.isEmpty() ? startRemotes() : startAheadBehind();
        if (!started)
        {
            finishError(RepositoryDiscoveryError::GitFailure);
        }
        return;
    }

    if (stage == Stage::AheadBehind)
    {
        if (!parseAheadBehind(processResult))
        {
            finishError(RepositoryDiscoveryError::InvalidOutput);
            return;
        }
        if (!startRemotes())
        {
            finishError(RepositoryDiscoveryError::GitFailure);
        }
        return;
    }

    if (stage == Stage::Remotes)
    {
        if (!parseRemotes(processResult))
        {
            finishError(RepositoryDiscoveryError::InvalidOutput);
            return;
        }
        finishSuccess();
    }
}

bool RepositoryDiscovery::parseProbe(const GitProcessResult& processResult)
{
    const QList<QByteArray> lines = outputLines(processResult.standardOutput);
    if (lines.size() != 4 || lines.at(0).isEmpty() || lines.at(1).isEmpty())
    {
        return false;
    }

    const QByteArray bareValue = lines.at(2).trimmed();
    const QByteArray insideWorkTreeValue = lines.at(3).trimmed();
    if ((bareValue != QByteArrayLiteral("true") && bareValue != QByteArrayLiteral("false")) ||
        (insideWorkTreeValue != QByteArrayLiteral("true") &&
         insideWorkTreeValue != QByteArrayLiteral("false")))
    {
        return false;
    }

    repository.gitDirectory = pathFromGit(lines.at(0));
    repository.commonGitDirectory = pathFromGit(lines.at(1));
    repository.isInsideWorkTree = insideWorkTreeValue == QByteArrayLiteral("true");
    if (bareValue == QByteArrayLiteral("true"))
    {
        repository.type = RepositoryType::Bare;
        repository.repositoryRoot = repository.gitDirectory;
    }
    return true;
}

bool RepositoryDiscovery::parseWorkTree(const GitProcessResult& processResult)
{
    const QList<QByteArray> lines = outputLines(processResult.standardOutput);
    if (lines.isEmpty() || lines.size() > 2 || lines.constFirst().isEmpty())
    {
        return false;
    }

    repository.workTree = pathFromGit(lines.at(0));
    repository.repositoryRoot = repository.workTree;
    if (lines.size() == 2 && !lines.at(1).isEmpty())
    {
        repository.superprojectWorkingTree = pathFromGit(lines.at(1));
        repository.type = RepositoryType::Submodule;
    }
    else if (repository.gitDirectory != repository.commonGitDirectory)
    {
        repository.type = RepositoryType::LinkedWorktree;
    }
    else
    {
        repository.type = RepositoryType::Normal;
    }
    return true;
}

bool RepositoryDiscovery::parseOperations(const GitProcessResult& processResult)
{
    const QList<QByteArray> lines = outputLines(processResult.standardOutput);
    if (lines.size() != 7)
    {
        return false;
    }

    if (QFileInfo::exists(pathFromGit(lines.at(0))))
    {
        repository.operations.append(RepositoryOperation::Merge);
    }
    if (QFileInfo::exists(pathFromGit(lines.at(1))) || QFileInfo::exists(pathFromGit(lines.at(2))))
    {
        repository.operations.append(RepositoryOperation::Rebase);
    }
    if (QFileInfo::exists(pathFromGit(lines.at(3))))
    {
        repository.operations.append(RepositoryOperation::CherryPick);
    }
    if (QFileInfo::exists(pathFromGit(lines.at(4))))
    {
        repository.operations.append(RepositoryOperation::Revert);
    }
    if (QFileInfo::exists(pathFromGit(lines.at(5))) || QFileInfo::exists(pathFromGit(lines.at(6))))
    {
        repository.operations.append(RepositoryOperation::Bisect);
    }
    return true;
}

bool RepositoryDiscovery::parseHead(const GitProcessResult& processResult)
{
    const QList<QByteArray> lines = outputLines(processResult.standardOutput);
    if (lines.size() > 1)
    {
        return false;
    }

    repository.currentBranch = lines.isEmpty() ? QString{} : textFromGit(lines.constFirst());
    repository.isDetachedHead = repository.currentBranch.isEmpty();
    return true;
}

bool RepositoryDiscovery::parseUpstream(const GitProcessResult& processResult)
{
    const QList<QByteArray> lines = outputLines(processResult.standardOutput);
    if (lines.size() > 1)
    {
        return false;
    }

    repository.upstream = lines.isEmpty() ? QString{} : textFromGit(lines.constFirst());
    return true;
}

bool RepositoryDiscovery::parseAheadBehind(const GitProcessResult& processResult)
{
    const QList<QByteArray> lines = outputLines(processResult.standardOutput);
    if (lines.size() != 1)
    {
        return false;
    }

    const QList<QByteArray> counts = lines.constFirst().simplified().split(' ');
    if (counts.size() != 2)
    {
        return false;
    }

    bool aheadValid = false;
    bool behindValid = false;
    const qint64 ahead = counts.at(0).toLongLong(&aheadValid);
    const qint64 behind = counts.at(1).toLongLong(&behindValid);
    if (!aheadValid || !behindValid || ahead < 0 || behind < 0)
    {
        return false;
    }
    repository.aheadCount = ahead;
    repository.behindCount = behind;
    return true;
}

bool RepositoryDiscovery::parseRemotes(const GitProcessResult& processResult)
{
    const QList<QByteArray> lines = outputLines(processResult.standardOutput);
    for (const QByteArray& line : lines)
    {
        if (line.isEmpty())
        {
            return false;
        }
        repository.remotes.append(textFromGit(line));
    }
    return true;
}

void RepositoryDiscovery::finishSuccess()
{
    result.repository = repository;
    result.error = RepositoryDiscoveryError::None;
    const RepositoryDiscoveryResult completedResult = result;
    stage = Stage::Idle;
    Q_EMIT finished(completedResult);
}

void RepositoryDiscovery::finishError(RepositoryDiscoveryError error)
{
    result.repository.reset();
    result.error = error;
    const RepositoryDiscoveryResult completedResult = result;
    stage = Stage::Idle;
    Q_EMIT finished(completedResult);
}

RepositoryDiscoveryError RepositoryDiscovery::errorFor(const GitProcessResult& processResult)
{
    switch (processResult.completionReason)
    {
    case GitProcessCompletionReason::FailedToStart:
        return RepositoryDiscoveryError::GitUnavailable;
    case GitProcessCompletionReason::Cancelled:
        return RepositoryDiscoveryError::Cancelled;
    case GitProcessCompletionReason::TimedOut:
        return RepositoryDiscoveryError::TimedOut;
    case GitProcessCompletionReason::Crashed:
        return RepositoryDiscoveryError::GitFailure;
    case GitProcessCompletionReason::Completed:
        break;
    }

    const QByteArray errorOutput = processResult.standardError.toLower();
    if (errorOutput.contains("not a git repository"))
    {
        return RepositoryDiscoveryError::NotRepository;
    }
    if (errorOutput.contains("permission denied"))
    {
        return RepositoryDiscoveryError::PermissionDenied;
    }
    return RepositoryDiscoveryError::GitFailure;
}

} // namespace LinuxGitShell
