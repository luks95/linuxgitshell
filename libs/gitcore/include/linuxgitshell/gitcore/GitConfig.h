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

enum class GitConfigScope
{
    System,
    Global,
    Local,
    Worktree,
    Command,
    Unknown,
};

enum class GitConfigOriginType
{
    File,
    StandardInput,
    Blob,
    CommandLine,
    Unknown,
};

struct GitConfigEntry
{
    GitConfigScope scope = GitConfigScope::Unknown;
    GitConfigOriginType originType = GitConfigOriginType::Unknown;
    QString scopeName;
    QString origin;
    QString key;
    QString value;
    bool isSensitive = false;
};

struct GitConfigSnapshot
{
    QList<GitConfigEntry> entries;

    [[nodiscard]] QList<GitConfigEntry> entriesForKey(const QString& key) const;
};

enum class GitConfigParseError
{
    None,
    MissingTerminator,
    IncompleteEntry,
    InvalidScope,
    InvalidOrigin,
    InvalidKeyValue,
};

struct GitConfigParseResult
{
    std::optional<GitConfigSnapshot> config;
    GitConfigParseError error = GitConfigParseError::None;
    qsizetype entryIndex = -1;
    QByteArray sanitizedOutput;
};

class GitConfigParser final
{
  public:
    [[nodiscard]] static GitConfigParseResult parse(const QByteArray& output);
};

} // namespace LinuxGitShell

Q_DECLARE_METATYPE(LinuxGitShell::GitConfigScope)
Q_DECLARE_METATYPE(LinuxGitShell::GitConfigOriginType)
Q_DECLARE_METATYPE(LinuxGitShell::GitConfigParseError)
