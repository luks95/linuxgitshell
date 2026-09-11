// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>

#include <optional>

namespace LinuxGitShell
{

enum class GitStatusRecordKind
{
    Ordinary,
    RenamedOrCopied,
    Unmerged,
    Untracked,
    Ignored,
};

enum class GitFileState
{
    Unmodified,
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    TypeChanged,
    Unmerged,
    Untracked,
    Ignored,
};

struct GitSubmoduleState
{
    bool isSubmodule = false;
    bool commitChanged = false;
    bool trackedChanges = false;
    bool untrackedChanges = false;
};

struct GitStatusEntry
{
    GitStatusRecordKind kind = GitStatusRecordKind::Ordinary;
    GitFileState indexState = GitFileState::Unmodified;
    GitFileState workTreeState = GitFileState::Unmodified;
    GitSubmoduleState submodule;
    QString path;
    QString originalPath;
    std::optional<int> similarityScore;
    QList<QString> modes;
    QList<QString> objectIds;

    [[nodiscard]] bool isConflicted() const;
};

struct GitStatusSnapshot
{
    QString headObjectId;
    QString currentBranch;
    QString upstream;
    std::optional<qint64> aheadCount;
    std::optional<qint64> behindCount;
    std::optional<qint64> stashCount;
    QList<GitStatusEntry> entries;
    bool isInitial = false;
    bool isDetachedHead = false;

    [[nodiscard]] bool isClean() const;
};

enum class GitStatusParseError
{
    None,
    MissingTerminator,
    MalformedRecord,
    UnsupportedRecord,
    InvalidHeader,
    InvalidStatusCode,
    InvalidSubmoduleState,
    InvalidMetadata,
    InvalidRenameScore,
    MissingOriginalPath,
};

struct GitStatusParseResult
{
    std::optional<GitStatusSnapshot> status;
    GitStatusParseError error = GitStatusParseError::None;
    qsizetype recordIndex = -1;
};

class GitStatusParser final
{
  public:
    [[nodiscard]] static GitStatusParseResult parse(const QByteArray& output);
};

} // namespace LinuxGitShell

Q_DECLARE_METATYPE(LinuxGitShell::GitStatusRecordKind)
Q_DECLARE_METATYPE(LinuxGitShell::GitFileState)
Q_DECLARE_METATYPE(LinuxGitShell::GitStatusParseError)
