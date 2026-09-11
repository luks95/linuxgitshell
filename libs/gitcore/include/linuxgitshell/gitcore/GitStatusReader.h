// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "linuxgitshell/gitcore/GitProcessRunner.h"
#include "linuxgitshell/gitcore/GitStatus.h"

#include <QObject>
#include <QString>

#include <chrono>
#include <optional>

namespace LinuxGitShell
{

struct GitStatusRequest
{
    QString path;
    bool includeIgnored = false;
    bool detectRenames = true;
    std::optional<std::chrono::milliseconds> timeout;
};

enum class GitStatusStartResult
{
    Accepted,
    Busy,
    InvalidPath,
    InvalidTimeout,
    RunnerRejected,
};

enum class GitStatusReadError
{
    None,
    NotRepository,
    NoWorkingTree,
    GitUnavailable,
    PermissionDenied,
    Cancelled,
    TimedOut,
    GitFailure,
    ParseFailure,
};

struct GitStatusResult
{
    QString requestedPath;
    std::optional<GitStatusSnapshot> status;
    GitStatusReadError error = GitStatusReadError::None;
    GitStatusParseError parseError = GitStatusParseError::None;
    GitProcessResult gitResult;
};

class GitStatusReader final : public QObject
{
    Q_OBJECT

  public:
    explicit GitStatusReader(QObject* parent = nullptr);
    ~GitStatusReader() override = default;

    GitStatusReader(const GitStatusReader&) = delete;
    GitStatusReader& operator=(const GitStatusReader&) = delete;
    GitStatusReader(GitStatusReader&&) = delete;
    GitStatusReader& operator=(GitStatusReader&&) = delete;

    [[nodiscard]] GitStatusStartResult start(const GitStatusRequest& request);
    void cancel();
    [[nodiscard]] bool isRunning() const;

  Q_SIGNALS:
    void finished(const LinuxGitShell::GitStatusResult& result);

  private:
    void processFinished(const GitProcessResult& processResult);
    [[nodiscard]] static GitStatusReadError errorFor(const GitProcessResult& processResult);

    GitProcessRunner runner;
    QString requestedPath;
};

} // namespace LinuxGitShell

Q_DECLARE_METATYPE(LinuxGitShell::GitStatusReadError)
Q_DECLARE_METATYPE(LinuxGitShell::GitStatusResult)
