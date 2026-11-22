// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "linuxgitshell/gitcore/GitProcessRunner.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include <chrono>
#include <optional>

namespace LinuxGitShell
{

enum class GitInventoryParseError
{
    None,
    MissingTerminator,
    EmptyPath,
};

struct GitInventoryParseResult
{
    // Unique repository-relative paths in order of first appearance. Unmerged index stages repeat
    // a name, so duplicates are removed here.
    std::optional<QStringList> paths;
    GitInventoryParseError error = GitInventoryParseError::None;
};

// Parses `git ls-files --cached -z` output.
class GitInventoryParser final
{
  public:
    [[nodiscard]] static GitInventoryParseResult parse(const QByteArray& output);
};

struct GitInventoryRequest
{
    QString repositoryRoot;
    std::optional<std::chrono::milliseconds> timeout;
    std::optional<qint64> maxOutputBytes;
    // SHA-256 of an earlier output. When the new output hashes the same, the result is marked
    // `unchanged` and the (large) output is not parsed again.
    QByteArray unchangedOutputDigest;
};

enum class GitInventoryStartResult
{
    Accepted,
    Busy,
    InvalidPath,
    InvalidTimeout,
    InvalidOutputLimit,
    RunnerRejected,
};

enum class GitInventoryReadError
{
    None,
    NotRepository,
    GitUnavailable,
    PermissionDenied,
    Cancelled,
    TimedOut,
    OutputLimitExceeded,
    GitFailure,
    ParseFailure,
};

struct GitInventoryResult
{
    QString repositoryRoot;
    std::optional<QStringList> paths;
    // SHA-256 of the raw output; set whenever Git completed successfully.
    QByteArray outputDigest;
    // The output matched `unchangedOutputDigest`: `paths` is intentionally absent.
    bool unchanged = false;
    GitInventoryReadError error = GitInventoryReadError::None;
    GitInventoryParseError parseError = GitInventoryParseError::None;
    GitProcessResult gitResult;
};

// Reads the complete tracked-path inventory of one repository. Partial output (timeout, overflow,
// failure) is never exposed as a path list.
class GitInventoryReader final : public QObject
{
    Q_OBJECT

  public:
    explicit GitInventoryReader(QObject* parent = nullptr);
    ~GitInventoryReader() override = default;

    GitInventoryReader(const GitInventoryReader&) = delete;
    GitInventoryReader& operator=(const GitInventoryReader&) = delete;
    GitInventoryReader(GitInventoryReader&&) = delete;
    GitInventoryReader& operator=(GitInventoryReader&&) = delete;

    [[nodiscard]] GitInventoryStartResult start(const GitInventoryRequest& request);
    void cancel();
    [[nodiscard]] bool isRunning() const;

  Q_SIGNALS:
    void finished(const LinuxGitShell::GitInventoryResult& result);

  private:
    void processFinished(const GitProcessResult& processResult);
    [[nodiscard]] static GitInventoryReadError errorFor(const GitProcessResult& processResult);

    GitProcessRunner runner;
    QString repositoryRoot;
    QByteArray unchangedDigest;
};

} // namespace LinuxGitShell

Q_DECLARE_METATYPE(LinuxGitShell::GitInventoryReadError)
Q_DECLARE_METATYPE(LinuxGitShell::GitInventoryResult)
