// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/repositorycontext/RepositoryContextWire.h"

#include <QStringList>

#include <array>
#include <utility>

namespace LinuxGitShell
{
namespace
{

constexpr QLatin1StringView StateKey("state");
constexpr QLatin1StringView RepositoryRootKey("repositoryRoot");
constexpr QLatin1StringView TypeKey("type");
constexpr QLatin1StringView OperationsKey("operations");
constexpr QLatin1StringView IsRepositoryRootKey("isRepositoryRoot");
constexpr QLatin1StringView HasRemoteKey("hasRemote");
constexpr QLatin1StringView HasUpstreamKey("hasUpstream");

template <typename Enum, std::size_t Size>
using NameTable = std::array<std::pair<Enum, QLatin1StringView>, Size>;

constexpr NameTable<RepositoryContextState, 4> StateNames{{
    {RepositoryContextState::InsideRepository, QLatin1StringView("inside")},
    {RepositoryContextState::OutsideRepository, QLatin1StringView("outside")},
    {RepositoryContextState::Unavailable, QLatin1StringView("unavailable")},
    {RepositoryContextState::DiscoveryError, QLatin1StringView("error")},
}};

constexpr NameTable<RepositoryContextType, 4> TypeNames{{
    {RepositoryContextType::Normal, QLatin1StringView("normal")},
    {RepositoryContextType::Bare, QLatin1StringView("bare")},
    {RepositoryContextType::LinkedWorktree, QLatin1StringView("worktree")},
    {RepositoryContextType::Submodule, QLatin1StringView("submodule")},
}};

constexpr NameTable<RepositoryContextOperation, 5> OperationNames{{
    {RepositoryContextOperation::Merge, QLatin1StringView("merge")},
    {RepositoryContextOperation::Rebase, QLatin1StringView("rebase")},
    {RepositoryContextOperation::CherryPick, QLatin1StringView("cherry-pick")},
    {RepositoryContextOperation::Revert, QLatin1StringView("revert")},
    {RepositoryContextOperation::Bisect, QLatin1StringView("bisect")},
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
    for (const auto& [entry, entryName] : table)
    {
        if (entryName == name)
        {
            return entry;
        }
    }
    return std::nullopt;
}

} // namespace

QVariantMap repositoryContextToVariantMap(const RepositoryContextSnapshot& snapshot)
{
    QVariantMap map;
    map.insert(StateKey, nameOf(StateNames, snapshot.state));
    if (snapshot.state != RepositoryContextState::InsideRepository)
    {
        return map;
    }

    QStringList operations;
    for (const RepositoryContextOperation operation : snapshot.operations)
    {
        operations.append(nameOf(OperationNames, operation));
    }
    map.insert(RepositoryRootKey, snapshot.repositoryRoot);
    map.insert(TypeKey, nameOf(TypeNames, snapshot.type));
    map.insert(OperationsKey, operations);
    map.insert(IsRepositoryRootKey, snapshot.isRepositoryRoot);
    map.insert(HasRemoteKey, snapshot.hasRemote);
    map.insert(HasUpstreamKey, snapshot.hasUpstream);
    return map;
}

std::optional<RepositoryContextSnapshot> repositoryContextFromVariantMap(const QVariantMap& map)
{
    const std::optional<RepositoryContextState> state =
        valueOf(StateNames, map.value(StateKey).toString());
    if (!state.has_value())
    {
        return std::nullopt;
    }

    RepositoryContextSnapshot snapshot;
    snapshot.state = state.value();
    if (snapshot.state != RepositoryContextState::InsideRepository)
    {
        return snapshot;
    }

    const std::optional<RepositoryContextType> type =
        valueOf(TypeNames, map.value(TypeKey).toString());
    snapshot.repositoryRoot = map.value(RepositoryRootKey).toString();
    if (!type.has_value() || snapshot.repositoryRoot.isEmpty())
    {
        return std::nullopt;
    }

    snapshot.type = type.value();
    const QStringList operations = map.value(OperationsKey).toStringList();
    for (const QString& name : operations)
    {
        if (const auto operation = valueOf(OperationNames, name); operation.has_value())
        {
            snapshot.operations.append(operation.value());
        }
    }
    snapshot.isRepositoryRoot = map.value(IsRepositoryRootKey).toBool();
    snapshot.hasRemote = map.value(HasRemoteKey).toBool();
    snapshot.hasUpstream = map.value(HasUpstreamKey).toBool();
    return snapshot;
}

} // namespace LinuxGitShell
