// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/GitStatus.h"

#include <array>

namespace LinuxGitShell
{
namespace
{

struct RecordFields
{
    QList<QByteArray> fields;
    QByteArray path;
};

[[nodiscard]] GitStatusParseResult failure(GitStatusParseError error, qsizetype recordIndex)
{
    GitStatusParseResult result;
    result.error = error;
    result.recordIndex = recordIndex;
    return result;
}

[[nodiscard]] std::optional<RecordFields> splitRecord(const QByteArray& record,
                                                      qsizetype fieldCount)
{
    RecordFields result;
    qsizetype fieldStart = 0;
    for (qsizetype index = 0; index < fieldCount; ++index)
    {
        const qsizetype separator = record.indexOf(' ', fieldStart);
        if (separator < 0)
        {
            return std::nullopt;
        }
        result.fields.append(record.sliced(fieldStart, separator - fieldStart));
        fieldStart = separator + 1;
    }
    result.path = record.sliced(fieldStart);
    if (result.path.isEmpty())
    {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] QString textFromGit(const QByteArray& text) { return QString::fromLocal8Bit(text); }

[[nodiscard]] std::optional<GitFileState> fileState(char code)
{
    switch (code)
    {
    case '.':
        return GitFileState::Unmodified;
    case 'M':
        return GitFileState::Modified;
    case 'A':
        return GitFileState::Added;
    case 'D':
        return GitFileState::Deleted;
    case 'R':
        return GitFileState::Renamed;
    case 'C':
        return GitFileState::Copied;
    case 'T':
        return GitFileState::TypeChanged;
    case 'U':
        return GitFileState::Unmerged;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] bool parseStatus(const QByteArray& value, GitStatusEntry& entry)
{
    if (value.size() != 2)
    {
        return false;
    }
    const auto indexState = fileState(value.at(0));
    const auto workTreeState = fileState(value.at(1));
    if (!indexState.has_value() || !workTreeState.has_value())
    {
        return false;
    }
    entry.indexState = *indexState;
    entry.workTreeState = *workTreeState;
    return true;
}

[[nodiscard]] bool parseSubmodule(const QByteArray& value, GitSubmoduleState& state)
{
    if (value.size() != 4)
    {
        return false;
    }
    if (value == QByteArrayLiteral("N..."))
    {
        return true;
    }
    if (value.at(0) != 'S' || (value.at(1) != 'C' && value.at(1) != '.') ||
        (value.at(2) != 'M' && value.at(2) != '.') || (value.at(3) != 'U' && value.at(3) != '.'))
    {
        return false;
    }
    state.isSubmodule = true;
    state.commitChanged = value.at(1) == 'C';
    state.trackedChanges = value.at(2) == 'M';
    state.untrackedChanges = value.at(3) == 'U';
    return true;
}

[[nodiscard]] bool isMode(const QByteArray& value)
{
    if (value.size() != 6)
    {
        return false;
    }
    for (const char byte : value)
    {
        if (byte < '0' || byte > '7')
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool isObjectId(const QByteArray& value)
{
    if (value.size() != 40 && value.size() != 64)
    {
        return false;
    }
    for (const char byte : value)
    {
        const bool digit = byte >= '0' && byte <= '9';
        const bool lowerHex = byte >= 'a' && byte <= 'f';
        const bool upperHex = byte >= 'A' && byte <= 'F';
        if (!digit && !lowerHex && !upperHex)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parseTrackedMetadata(const QList<QByteArray>& fields, GitStatusEntry& entry)
{
    for (qsizetype index = 3; index <= 5; ++index)
    {
        if (!isMode(fields.at(index)))
        {
            return false;
        }
        entry.modes.append(textFromGit(fields.at(index)));
    }
    for (qsizetype index = 6; index <= 7; ++index)
    {
        if (!isObjectId(fields.at(index)))
        {
            return false;
        }
        entry.objectIds.append(textFromGit(fields.at(index)));
    }
    return true;
}

[[nodiscard]] bool isUnmergedStatus(const QByteArray& value)
{
    static constexpr std::array statuses = {"DD", "AU", "UD", "UA", "DU", "AA", "UU"};
    for (const char* status : statuses)
    {
        if (value == status)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] GitStatusParseError parseOrdinary(const QByteArray& record,
                                                GitStatusSnapshot& snapshot)
{
    const auto recordFields = splitRecord(record, 8);
    if (!recordFields.has_value())
    {
        return GitStatusParseError::MalformedRecord;
    }
    const auto& fields = recordFields->fields;
    GitStatusEntry entry;
    entry.kind = GitStatusRecordKind::Ordinary;
    if (fields.at(0) != QByteArrayLiteral("1") || !parseStatus(fields.at(1), entry))
    {
        return GitStatusParseError::InvalidStatusCode;
    }
    if (entry.indexState == GitFileState::Unmerged || entry.workTreeState == GitFileState::Unmerged)
    {
        return GitStatusParseError::InvalidStatusCode;
    }
    if (!parseSubmodule(fields.at(2), entry.submodule))
    {
        return GitStatusParseError::InvalidSubmoduleState;
    }
    if (!parseTrackedMetadata(fields, entry))
    {
        return GitStatusParseError::InvalidMetadata;
    }
    entry.path = textFromGit(recordFields->path);
    snapshot.entries.append(entry);
    return GitStatusParseError::None;
}

[[nodiscard]] GitStatusParseError parseRenamedOrCopied(const QByteArray& record,
                                                       GitStatusSnapshot& snapshot,
                                                       const QByteArray& originalPath)
{
    const auto recordFields = splitRecord(record, 9);
    if (!recordFields.has_value() || originalPath.isEmpty())
    {
        return GitStatusParseError::MalformedRecord;
    }
    const auto& fields = recordFields->fields;
    GitStatusEntry entry;
    entry.kind = GitStatusRecordKind::RenamedOrCopied;
    if (fields.at(0) != QByteArrayLiteral("2") || !parseStatus(fields.at(1), entry))
    {
        return GitStatusParseError::InvalidStatusCode;
    }
    if (!parseSubmodule(fields.at(2), entry.submodule))
    {
        return GitStatusParseError::InvalidSubmoduleState;
    }
    if (!parseTrackedMetadata(fields, entry))
    {
        return GitStatusParseError::InvalidMetadata;
    }

    const QByteArray score = fields.at(8);
    if (score.size() < 2 || (score.at(0) != 'R' && score.at(0) != 'C'))
    {
        return GitStatusParseError::InvalidRenameScore;
    }
    bool scoreValid = false;
    const int similarityScore = score.sliced(1).toInt(&scoreValid);
    if (!scoreValid || similarityScore < 0 || similarityScore > 100 ||
        !fields.at(1).contains(score.at(0)))
    {
        return GitStatusParseError::InvalidRenameScore;
    }
    entry.similarityScore = similarityScore;
    entry.path = textFromGit(recordFields->path);
    entry.originalPath = textFromGit(originalPath);
    snapshot.entries.append(entry);
    return GitStatusParseError::None;
}

[[nodiscard]] GitStatusParseError parseUnmerged(const QByteArray& record,
                                                GitStatusSnapshot& snapshot)
{
    const auto recordFields = splitRecord(record, 10);
    if (!recordFields.has_value())
    {
        return GitStatusParseError::MalformedRecord;
    }
    const auto& fields = recordFields->fields;
    GitStatusEntry entry;
    entry.kind = GitStatusRecordKind::Unmerged;
    if (fields.at(0) != QByteArrayLiteral("u") || !isUnmergedStatus(fields.at(1)) ||
        !parseStatus(fields.at(1), entry))
    {
        return GitStatusParseError::InvalidStatusCode;
    }
    if (!parseSubmodule(fields.at(2), entry.submodule))
    {
        return GitStatusParseError::InvalidSubmoduleState;
    }
    for (qsizetype index = 3; index <= 6; ++index)
    {
        if (!isMode(fields.at(index)))
        {
            return GitStatusParseError::InvalidMetadata;
        }
        entry.modes.append(textFromGit(fields.at(index)));
    }
    for (qsizetype index = 7; index <= 9; ++index)
    {
        if (!isObjectId(fields.at(index)))
        {
            return GitStatusParseError::InvalidMetadata;
        }
        entry.objectIds.append(textFromGit(fields.at(index)));
    }
    entry.path = textFromGit(recordFields->path);
    snapshot.entries.append(entry);
    return GitStatusParseError::None;
}

[[nodiscard]] GitStatusParseError parseSimplePath(const QByteArray& record,
                                                  GitStatusSnapshot& snapshot)
{
    if (record.size() < 3 || record.at(1) != ' ')
    {
        return GitStatusParseError::MalformedRecord;
    }
    GitStatusEntry entry;
    entry.path = textFromGit(record.sliced(2));
    if (record.at(0) == '?')
    {
        entry.kind = GitStatusRecordKind::Untracked;
        entry.workTreeState = GitFileState::Untracked;
    }
    else
    {
        entry.kind = GitStatusRecordKind::Ignored;
        entry.workTreeState = GitFileState::Ignored;
    }
    snapshot.entries.append(entry);
    return GitStatusParseError::None;
}

[[nodiscard]] GitStatusParseError parseHeader(const QByteArray& record, GitStatusSnapshot& snapshot)
{
    const qsizetype separator = record.indexOf(' ', 2);
    if (record.size() < 4 || !record.startsWith("# ") || separator < 0 ||
        separator == record.size() - 1)
    {
        return GitStatusParseError::InvalidHeader;
    }
    const QByteArray name = record.sliced(2, separator - 2);
    const QByteArray value = record.sliced(separator + 1);
    if (name == QByteArrayLiteral("branch.oid"))
    {
        if (value == QByteArrayLiteral("(initial)"))
        {
            snapshot.isInitial = true;
            snapshot.headObjectId.clear();
        }
        else if (isObjectId(value))
        {
            snapshot.headObjectId = textFromGit(value);
        }
        else
        {
            return GitStatusParseError::InvalidHeader;
        }
    }
    else if (name == QByteArrayLiteral("branch.head"))
    {
        snapshot.isDetachedHead = value == QByteArrayLiteral("(detached)");
        snapshot.currentBranch = snapshot.isDetachedHead ? QString{} : textFromGit(value);
    }
    else if (name == QByteArrayLiteral("branch.upstream"))
    {
        snapshot.upstream = textFromGit(value);
    }
    else if (name == QByteArrayLiteral("branch.ab"))
    {
        const QList<QByteArray> values = value.split(' ');
        if (values.size() != 2 || !values.at(0).startsWith('+') || !values.at(1).startsWith('-'))
        {
            return GitStatusParseError::InvalidHeader;
        }
        bool aheadValid = false;
        bool behindValid = false;
        const qint64 ahead = values.at(0).sliced(1).toLongLong(&aheadValid);
        const qint64 behind = values.at(1).sliced(1).toLongLong(&behindValid);
        if (!aheadValid || !behindValid || ahead < 0 || behind < 0)
        {
            return GitStatusParseError::InvalidHeader;
        }
        snapshot.aheadCount = ahead;
        snapshot.behindCount = behind;
    }
    else if (name == QByteArrayLiteral("stash"))
    {
        bool countValid = false;
        const qint64 count = value.toLongLong(&countValid);
        if (!countValid || count < 0)
        {
            return GitStatusParseError::InvalidHeader;
        }
        snapshot.stashCount = count;
    }
    return GitStatusParseError::None;
}

} // namespace

bool GitStatusEntry::isConflicted() const { return kind == GitStatusRecordKind::Unmerged; }

bool GitStatusSnapshot::isClean() const
{
    for (const GitStatusEntry& entry : entries)
    {
        if (entry.kind != GitStatusRecordKind::Ignored)
        {
            return false;
        }
    }
    return true;
}

GitStatusParseResult GitStatusParser::parse(const QByteArray& output)
{
    GitStatusSnapshot snapshot;
    if (output.isEmpty())
    {
        GitStatusParseResult result;
        result.status = snapshot;
        return result;
    }
    if (!output.endsWith('\0'))
    {
        return failure(GitStatusParseError::MissingTerminator, 0);
    }

    QList<QByteArray> records = output.split('\0');
    records.removeLast();
    for (qsizetype index = 0; index < records.size(); ++index)
    {
        const qsizetype statusRecordIndex = index;
        const QByteArray& record = records.at(index);
        if (record.isEmpty())
        {
            return failure(GitStatusParseError::MalformedRecord, index);
        }

        GitStatusParseError error = GitStatusParseError::None;
        switch (record.at(0))
        {
        case '#':
            error = parseHeader(record, snapshot);
            break;
        case '1':
            error = parseOrdinary(record, snapshot);
            break;
        case '2':
            if (index + 1 >= records.size())
            {
                return failure(GitStatusParseError::MissingOriginalPath, index);
            }
            ++index;
            error = parseRenamedOrCopied(record, snapshot, records.at(index));
            break;
        case 'u':
            error = parseUnmerged(record, snapshot);
            break;
        case '?':
        case '!':
            error = parseSimplePath(record, snapshot);
            break;
        default:
            error = GitStatusParseError::UnsupportedRecord;
            break;
        }
        if (error != GitStatusParseError::None)
        {
            return failure(error, statusRecordIndex);
        }
    }

    GitStatusParseResult result;
    result.status = snapshot;
    return result;
}

} // namespace LinuxGitShell
