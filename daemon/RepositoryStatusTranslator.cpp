// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "RepositoryStatusTranslator.h"

namespace LinuxGitShell
{

namespace
{

[[nodiscard]] RepositoryFileState translate(GitFileState state)
{
    switch (state)
    {
    case GitFileState::Unmodified:
        return RepositoryFileState::Clean;
    case GitFileState::Modified:
        return RepositoryFileState::Modified;
    case GitFileState::Added:
        return RepositoryFileState::Added;
    case GitFileState::Deleted:
        return RepositoryFileState::Deleted;
    case GitFileState::Renamed:
        return RepositoryFileState::Renamed;
    case GitFileState::Copied:
        return RepositoryFileState::Copied;
    case GitFileState::TypeChanged:
        return RepositoryFileState::TypeChanged;
    case GitFileState::Unmerged:
        return RepositoryFileState::Conflict;
    case GitFileState::Untracked:
        return RepositoryFileState::Untracked;
    case GitFileState::Ignored:
        return RepositoryFileState::Ignored;
    }
    return RepositoryFileState::Clean;
}

// Strips a single trailing '/' (Git marks untracked and ignored directories this way).
[[nodiscard]] QString withoutTerminator(const QString& path, bool* hadTerminator = nullptr)
{
    const bool terminated = path.size() > 1 && path.endsWith(QLatin1Char('/'));
    if (hadTerminator != nullptr)
    {
        *hadTerminator = terminated;
    }
    return terminated ? path.chopped(1) : path;
}

[[nodiscard]] RepositoryStatusEntry translate(const GitStatusEntry& source)
{
    RepositoryStatusEntry entry;
    entry.path = withoutTerminator(source.path, &entry.isDirectory);
    entry.originalPath = withoutTerminator(source.originalPath);
    if (source.isConflicted())
    {
        // A conflict is a single unmerged state; do not rely on how XY letters were decoded.
        entry.indexState = RepositoryFileState::Conflict;
        entry.workTreeState = RepositoryFileState::Conflict;
    }
    else
    {
        entry.indexState = translate(source.indexState);
        entry.workTreeState = translate(source.workTreeState);
    }
    entry.isSubmodule = source.submodule.isSubmodule;
    entry.submoduleCommitChanged = source.submodule.commitChanged;
    entry.submoduleTrackedChanges = source.submodule.trackedChanges;
    entry.submoduleUntrackedChanges = source.submodule.untrackedChanges;
    return entry;
}

} // namespace

RepositoryStatusSnapshot translateRepositoryStatus(const RepositoryStatusTranslationInput& input)
{
    RepositoryStatusSnapshot snapshot;
    snapshot.revision = input.revision;
    snapshot.capturedAt = input.capturedAt;
    snapshot.validity = RepositoryStatusValidity::Ready;
    snapshot.coverageComplete = true;
    snapshot.trackedPaths = input.trackedPaths;
    snapshot.excludedRoots = input.excludedRoots;
    snapshot.entries.reserve(input.status.entries.size());
    for (const GitStatusEntry& source : input.status.entries)
    {
        snapshot.entries.append(translate(source));
    }
    return snapshot;
}

} // namespace LinuxGitShell
