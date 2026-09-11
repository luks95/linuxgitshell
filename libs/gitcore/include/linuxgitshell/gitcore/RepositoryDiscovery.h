// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "linuxgitshell/gitcore/GitProcessRunner.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace LinuxGitShell
{

enum class RepositoryType
{
    Normal,
    Bare,
    LinkedWorktree,
    Submodule,
};

enum class RepositoryOperation
{
    Merge,
    Rebase,
    CherryPick,
    Revert,
    Bisect,
};

struct RepositoryInfo
{
    RepositoryType type = RepositoryType::Normal;
    QString repositoryRoot;
    QString workTree;
    QString gitDirectory;
    QString commonGitDirectory;
    QString superprojectWorkingTree;
    QString currentBranch;
    QString upstream;
    std::optional<qint64> aheadCount;
    std::optional<qint64> behindCount;
    QStringList remotes;
    QList<RepositoryOperation> operations;
    bool isInsideWorkTree = false;
    bool isDetachedHead = false;
};

enum class RepositoryDiscoveryError
{
    None,
    NotRepository,
    GitUnavailable,
    PermissionDenied,
    InvalidOutput,
    Cancelled,
    TimedOut,
    GitFailure,
};

enum class RepositoryDiscoveryStartResult
{
    Accepted,
    Busy,
    InvalidPath,
};

struct RepositoryDiscoveryResult
{
    QString requestedPath;
    std::optional<RepositoryInfo> repository;
    RepositoryDiscoveryError error = RepositoryDiscoveryError::None;
    QList<GitProcessResult> gitResults;
};

class RepositoryDiscovery final : public QObject
{
    Q_OBJECT

  public:
    explicit RepositoryDiscovery(QObject* parent = nullptr);
    ~RepositoryDiscovery() override = default;

    RepositoryDiscovery(const RepositoryDiscovery&) = delete;
    RepositoryDiscovery& operator=(const RepositoryDiscovery&) = delete;
    RepositoryDiscovery(RepositoryDiscovery&&) = delete;
    RepositoryDiscovery& operator=(RepositoryDiscovery&&) = delete;

    [[nodiscard]] RepositoryDiscoveryStartResult discover(const QString& path);
    void cancel();
    [[nodiscard]] bool isRunning() const;

  Q_SIGNALS:
    void finished(const LinuxGitShell::RepositoryDiscoveryResult& result);

  private:
    enum class Stage
    {
        Idle,
        Probe,
        WorkTree,
        Operations,
        Head,
        Upstream,
        AheadBehind,
        Remotes,
    };

    [[nodiscard]] bool startGit(const QStringList& arguments);
    [[nodiscard]] bool startOperations();
    [[nodiscard]] bool startHead();
    [[nodiscard]] bool startUpstream();
    [[nodiscard]] bool startAheadBehind();
    [[nodiscard]] bool startRemotes();
    void processFinished(const GitProcessResult& processResult);
    [[nodiscard]] bool parseProbe(const GitProcessResult& processResult);
    [[nodiscard]] bool parseWorkTree(const GitProcessResult& processResult);
    [[nodiscard]] bool parseOperations(const GitProcessResult& processResult);
    [[nodiscard]] bool parseHead(const GitProcessResult& processResult);
    [[nodiscard]] bool parseUpstream(const GitProcessResult& processResult);
    [[nodiscard]] bool parseAheadBehind(const GitProcessResult& processResult);
    [[nodiscard]] bool parseRemotes(const GitProcessResult& processResult);
    void finishSuccess();
    void finishError(RepositoryDiscoveryError error);
    [[nodiscard]] static RepositoryDiscoveryError errorFor(const GitProcessResult& processResult);

    GitProcessRunner runner;
    Stage stage = Stage::Idle;
    QString discoveryDirectory;
    RepositoryDiscoveryResult result;
    RepositoryInfo repository;
};

} // namespace LinuxGitShell

Q_DECLARE_METATYPE(LinuxGitShell::RepositoryType)
Q_DECLARE_METATYPE(LinuxGitShell::RepositoryOperation)
Q_DECLARE_METATYPE(LinuxGitShell::RepositoryDiscoveryError)
Q_DECLARE_METATYPE(LinuxGitShell::RepositoryDiscoveryResult)
