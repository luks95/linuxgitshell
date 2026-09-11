// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/GitConfig.h"

#include "linuxgitshell/gitcore/GitCommandSanitizer.h"

namespace LinuxGitShell
{
namespace
{

[[nodiscard]] QString textFromGit(const QByteArray& text) { return QString::fromLocal8Bit(text); }

[[nodiscard]] GitConfigScope configScope(const QByteArray& value)
{
    if (value == QByteArrayLiteral("system"))
        return GitConfigScope::System;
    if (value == QByteArrayLiteral("global"))
        return GitConfigScope::Global;
    if (value == QByteArrayLiteral("local"))
        return GitConfigScope::Local;
    if (value == QByteArrayLiteral("worktree"))
        return GitConfigScope::Worktree;
    if (value == QByteArrayLiteral("command"))
        return GitConfigScope::Command;
    return GitConfigScope::Unknown;
}

[[nodiscard]] GitConfigOriginType originType(const QByteArray& value)
{
    if (value.startsWith("file:"))
        return GitConfigOriginType::File;
    if (value.startsWith("standard input:"))
        return GitConfigOriginType::StandardInput;
    if (value.startsWith("blob:"))
        return GitConfigOriginType::Blob;
    if (value.startsWith("command line:"))
        return GitConfigOriginType::CommandLine;
    return GitConfigOriginType::Unknown;
}

[[nodiscard]] bool sensitiveKey(const QString& key)
{
    const QString lowerKey = key.toLower();
    return lowerKey.contains(QStringLiteral("password")) ||
           lowerKey.contains(QStringLiteral("token")) ||
           lowerKey.contains(QStringLiteral("secret")) ||
           lowerKey.contains(QStringLiteral("privatekey")) ||
           lowerKey.endsWith(QStringLiteral(".extraheader"));
}

[[nodiscard]] QString sanitizeKey(const QString& key, bool& changed)
{
    QString sanitized = key;
    const qsizetype schemeEnd = sanitized.indexOf(QStringLiteral("://"));
    const qsizetype credentialsEnd =
        schemeEnd < 0 ? -1 : sanitized.indexOf(QLatin1Char('@'), schemeEnd + 3);
    if (credentialsEnd >= 0)
    {
        sanitized.replace(schemeEnd + 3, credentialsEnd - schemeEnd - 3,
                          QStringLiteral("REDACTED"));
        changed = true;
    }
    return sanitized;
}

[[nodiscard]] QString sanitizeValue(const QString& value, bool isSensitive, bool& changed)
{
    if (isSensitive)
    {
        changed = true;
        return QStringLiteral("REDACTED");
    }
    const QString sanitized = sanitizeGitArguments({value}).constFirst();
    changed = sanitized != value;
    return sanitized;
}

[[nodiscard]] GitConfigParseResult failure(GitConfigParseError error, qsizetype entryIndex)
{
    GitConfigParseResult result;
    result.error = error;
    result.entryIndex = entryIndex;
    return result;
}

} // namespace

QList<GitConfigEntry> GitConfigSnapshot::entriesForKey(const QString& key) const
{
    QList<GitConfigEntry> matches;
    for (const GitConfigEntry& entry : entries)
    {
        if (entry.key.compare(key, Qt::CaseInsensitive) == 0)
        {
            matches.append(entry);
        }
    }
    return matches;
}

GitConfigParseResult GitConfigParser::parse(const QByteArray& output)
{
    GitConfigSnapshot snapshot;
    if (output.isEmpty())
    {
        GitConfigParseResult result;
        result.config = snapshot;
        return result;
    }
    if (!output.endsWith('\0'))
    {
        return failure(GitConfigParseError::MissingTerminator, 0);
    }

    QList<QByteArray> fields = output.split('\0');
    fields.removeLast();
    if (fields.size() % 3 != 0)
    {
        return failure(GitConfigParseError::IncompleteEntry, fields.size() / 3);
    }

    GitConfigParseResult result;
    for (qsizetype fieldIndex = 0; fieldIndex < fields.size(); fieldIndex += 3)
    {
        const qsizetype entryIndex = fieldIndex / 3;
        const QByteArray& scopeField = fields.at(fieldIndex);
        const QByteArray& originField = fields.at(fieldIndex + 1);
        const QByteArray& keyValueField = fields.at(fieldIndex + 2);
        if (scopeField.isEmpty())
            return failure(GitConfigParseError::InvalidScope, entryIndex);
        if (originField.isEmpty())
            return failure(GitConfigParseError::InvalidOrigin, entryIndex);
        const qsizetype separator = keyValueField.indexOf('\n');
        if (separator <= 0)
            return failure(GitConfigParseError::InvalidKeyValue, entryIndex);

        GitConfigEntry entry;
        entry.scope = configScope(scopeField);
        entry.originType = originType(originField);
        entry.scopeName = textFromGit(scopeField);
        entry.origin = textFromGit(originField);
        const QString originalKey = textFromGit(keyValueField.first(separator));
        const QString originalValue = textFromGit(keyValueField.sliced(separator + 1));
        bool keyChanged = false;
        bool valueChanged = false;
        const bool valueIsSensitive = sensitiveKey(originalKey);
        entry.key = sanitizeKey(originalKey, keyChanged);
        entry.value = sanitizeValue(originalValue, valueIsSensitive, valueChanged);
        entry.isSensitive = keyChanged || valueChanged;
        snapshot.entries.append(entry);

        result.sanitizedOutput += scopeField + '\0';
        result.sanitizedOutput += originField + '\0';
        result.sanitizedOutput += entry.key.toLocal8Bit() + '\n' + entry.value.toLocal8Bit() + '\0';
    }
    result.config = snapshot;
    return result;
}

} // namespace LinuxGitShell
