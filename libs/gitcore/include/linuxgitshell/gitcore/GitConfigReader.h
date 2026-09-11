// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "linuxgitshell/gitcore/GitConfig.h"
#include "linuxgitshell/gitcore/GitProcessRunner.h"

#include <QObject>
#include <QString>

#include <chrono>
#include <optional>

namespace LinuxGitShell
{

struct GitConfigRequest
{
    QString path;
    bool includeFiles = true;
    std::optional<std::chrono::milliseconds> timeout;
};

enum class GitConfigStartResult
{
    Accepted,
    Busy,
    InvalidPath,
    InvalidTimeout,
    RunnerRejected,
};

enum class GitConfigReadError
{
    None,
    GitUnavailable,
    PermissionDenied,
    Cancelled,
    TimedOut,
    GitFailure,
    ParseFailure,
};

struct GitConfigResult
{
    QString requestedPath;
    std::optional<GitConfigSnapshot> config;
    GitConfigReadError error = GitConfigReadError::None;
    GitConfigParseError parseError = GitConfigParseError::None;
    GitProcessResult gitResult;
};

class GitConfigReader final : public QObject
{
    Q_OBJECT

  public:
    explicit GitConfigReader(QObject* parent = nullptr);
    ~GitConfigReader() override = default;

    GitConfigReader(const GitConfigReader&) = delete;
    GitConfigReader& operator=(const GitConfigReader&) = delete;
    GitConfigReader(GitConfigReader&&) = delete;
    GitConfigReader& operator=(GitConfigReader&&) = delete;

    [[nodiscard]] GitConfigStartResult start(const GitConfigRequest& request);
    void cancel();
    [[nodiscard]] bool isRunning() const;

  Q_SIGNALS:
    void finished(const LinuxGitShell::GitConfigResult& result);

  private:
    void processFinished(const GitProcessResult& processResult);
    [[nodiscard]] static GitConfigReadError errorFor(const GitProcessResult& processResult);

    GitProcessRunner runner;
    QString requestedPath;
};

} // namespace LinuxGitShell

Q_DECLARE_METATYPE(LinuxGitShell::GitConfigReadError)
Q_DECLARE_METATYPE(LinuxGitShell::GitConfigResult)
