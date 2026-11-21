// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "linuxgitshell/gitcore/GitStatus.h"
#include "linuxgitshell/repositorycontext/RepositoryStatusIndex.h"

#include <QStringList>

namespace LinuxGitShell
{

struct RepositoryStatusTranslationInput
{
    quint64 revision = 0;
    RepositoryStatusIndex::Clock::time_point capturedAt;
    // Both collectors must have completed without truncation for the same refresh revision.
    GitStatusSnapshot status;
    QStringList trackedPaths;
    // Nested repository roots (relative keys) whose contents belong to another index.
    QStringList excludedRoots;
};

// Translates Git collector output into the model-owned snapshot, keeping RepositoryContext free
// of any GitCore dependency. Porcelain directory terminators are stripped here; path validation is
// left to RepositoryStatusIndex::replace().
[[nodiscard]] RepositoryStatusSnapshot
translateRepositoryStatus(const RepositoryStatusTranslationInput& input);

} // namespace LinuxGitShell
