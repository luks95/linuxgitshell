// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/repositorycontext/OverlayWire.h"

#include <array>
#include <utility>

namespace LinuxGitShell
{
namespace
{

constexpr QLatin1StringView PathKey("path");
constexpr QLatin1StringView RepositoryRootKey("repositoryRoot");
constexpr QLatin1StringView StateKey("state");
constexpr QLatin1StringView ValidityKey("validity");
constexpr QLatin1StringView RevisionKey("revision");
constexpr QLatin1StringView ValidForMsKey("validForMs");

// Fixed per-record overhead: keys, type signatures, and padding of the six fields.
constexpr qsizetype RecordOverheadBytes = 192;

template <typename Enum, std::size_t Size>
using NameTable = std::array<std::pair<Enum, QLatin1StringView>, Size>;

constexpr NameTable<RepositoryStatusState, 8> StateNames{{
    {RepositoryStatusState::Unknown, QLatin1StringView("unknown")},
    {RepositoryStatusState::Clean, QLatin1StringView("clean")},
    {RepositoryStatusState::Ignored, QLatin1StringView("ignored")},
    {RepositoryStatusState::Untracked, QLatin1StringView("untracked")},
    {RepositoryStatusState::Deleted, QLatin1StringView("deleted")},
    {RepositoryStatusState::Added, QLatin1StringView("added")},
    {RepositoryStatusState::Modified, QLatin1StringView("modified")},
    {RepositoryStatusState::Conflict, QLatin1StringView("conflict")},
}};

constexpr NameTable<RepositoryStatusValidity, 5> ValidityNames{{
    {RepositoryStatusValidity::Ready, QLatin1StringView("ready")},
    {RepositoryStatusValidity::Pending, QLatin1StringView("pending")},
    {RepositoryStatusValidity::Stale, QLatin1StringView("stale")},
    {RepositoryStatusValidity::Degraded, QLatin1StringView("degraded")},
    {RepositoryStatusValidity::Unavailable, QLatin1StringView("unavailable")},
}};

template <typename Enum, std::size_t Size>
[[nodiscard]] QString nameOf(const NameTable<Enum, Size>& table, Enum value)
{
    for (const auto& [entry, name] : table)
    {
        if (entry == value)
        {
            return name;
        }
    }
    return {};
}

template <typename Enum, std::size_t Size>
[[nodiscard]] std::optional<Enum> valueOf(const NameTable<Enum, Size>& table, const QString& name)
{
    for (const auto& [entry, tableName] : table)
    {
        if (name == tableName)
        {
            return entry;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<QString> stringField(const QVariantMap& map, QLatin1StringView key)
{
    const auto it = map.constFind(key);
    if (it == map.cend() || it->typeId() != QMetaType::QString)
    {
        return std::nullopt;
    }
    return it->toString();
}

} // namespace

QString overlayStateName(RepositoryStatusState state) { return nameOf(StateNames, state); }

QString overlayValidityName(RepositoryStatusValidity validity)
{
    return nameOf(ValidityNames, validity);
}

QVariantMap overlayRecordToVariantMap(const OverlayRecord& record)
{
    QVariantMap map;
    map.insert(PathKey, record.path);
    map.insert(RepositoryRootKey, record.repositoryRoot);
    map.insert(StateKey, overlayStateName(record.state));
    map.insert(ValidityKey, overlayValidityName(record.validity));
    map.insert(RevisionKey, QVariant::fromValue<quint64>(record.revision));
    map.insert(ValidForMsKey, QVariant::fromValue<quint32>(record.validForMs));
    return map;
}

std::optional<OverlayRecord> overlayRecordFromVariantMap(const QVariantMap& map)
{
    const std::optional<QString> path = stringField(map, PathKey);
    const std::optional<QString> root = stringField(map, RepositoryRootKey);
    const std::optional<QString> stateName = stringField(map, StateKey);
    const std::optional<QString> validityName = stringField(map, ValidityKey);
    const auto revision = map.constFind(RevisionKey);
    const auto validFor = map.constFind(ValidForMsKey);
    if (!path.has_value() || path->isEmpty() || !root.has_value() || !stateName.has_value() ||
        !validityName.has_value() || revision == map.cend() ||
        revision->typeId() != QMetaType::ULongLong || validFor == map.cend() ||
        validFor->typeId() != QMetaType::UInt)
    {
        return std::nullopt;
    }
    const auto state = valueOf(StateNames, *stateName);
    const auto validity = valueOf(ValidityNames, *validityName);
    if (!state.has_value() || !validity.has_value())
    {
        return std::nullopt;
    }

    OverlayRecord record;
    record.path = *path;
    record.repositoryRoot = *root;
    record.revision = revision->toULongLong();
    record.validity = *validity;
    if (*validity == RepositoryStatusValidity::Ready)
    {
        record.state = *state;
        record.validForMs = validFor->toUInt();
    }
    return record;
}

qsizetype overlayRecordEncodedSizeEstimate(const OverlayRecord& record)
{
    // UTF-8 expands each UTF-16 code unit to at most three bytes; D-Bus adds a length prefix.
    return RecordOverheadBytes + 3 * (record.path.size() + record.repositoryRoot.size());
}

QList<QList<OverlayRecord>> splitOverlayRecords(const QList<OverlayRecord>& records,
                                                qsizetype maxRecords, qsizetype maxBytes)
{
    QList<QList<OverlayRecord>> batches;
    qsizetype bytes = 0;
    for (const OverlayRecord& record : records)
    {
        const qsizetype size = overlayRecordEncodedSizeEstimate(record);
        if (batches.isEmpty() || batches.constLast().size() >= maxRecords ||
            bytes + size > maxBytes)
        {
            batches.append(QList<OverlayRecord>());
            bytes = 0;
        }
        batches.last().append(record);
        bytes += size;
    }
    return batches;
}

} // namespace LinuxGitShell
