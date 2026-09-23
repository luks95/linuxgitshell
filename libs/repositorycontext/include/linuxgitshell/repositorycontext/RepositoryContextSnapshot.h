// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <QList>
#include <QString>

namespace LinuxGitShell
{

enum class RepositoryContextState
{
    InsideRepository,
    OutsideRepository,
    Unavailable,
    DiscoveryError,
};

enum class RepositoryContextType
{
    Normal,
    Bare,
    LinkedWorktree,
    Submodule,
};

enum class RepositoryContextOperation
{
    Merge,
    Rebase,
    CherryPick,
    Revert,
    Bisect,
};

// Only the fields required to decide context-menu availability. Repository-level fields are
// meaningful only when state is InsideRepository.
struct RepositoryContextSnapshot
{
    RepositoryContextState state = RepositoryContextState::Unavailable;
    QString repositoryRoot;
    RepositoryContextType type = RepositoryContextType::Normal;
    QList<RepositoryContextOperation> operations;
    bool isRepositoryRoot = false;
    bool hasRemote = false;
    bool hasUpstream = false;

    bool operator==(const RepositoryContextSnapshot&) const = default;
};

// Lexical cache key for an absolute local path. It never touches the filesystem, so it is safe on
// the synchronous context-menu path; symlink and `.git` indirection resolution belongs to the
// Git-backed service. Returns an empty string for empty or relative paths.
[[nodiscard]] QString repositoryContextPathKey(const QString& localPath);

} // namespace LinuxGitShell
