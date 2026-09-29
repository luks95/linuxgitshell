// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "linuxgitshell/repositorycontext/RepositoryContextCache.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

namespace LinuxGitShell::Dolphin
{

enum class ContextMenuKind
{
    // No LinuxGitShell entry: remote, mixed, oversized, outside-repository, or unresolved
    // multiple selections.
    None,
    // One local item without a warm snapshot, or with a failed lookup: the safe generic action.
    Generic,
    // A warm snapshot places the whole selection in one repository.
    Repository,
};

struct ContextMenuDecision
{
    ContextMenuKind kind = ContextMenuKind::None;
    // Path passed to the external application: the selected path for one item, the repository
    // root for several items in the same repository.
    QString launchPath;
    QList<RepositoryContextOperation> operations;
    // Cold or stale keys to refresh asynchronously after the menu is built.
    QStringList refreshKeys;
};

// Decides the context-menu entries from KIO URLs and memory-only snapshot lookups. It performs no
// filesystem, Git, or IPC work, so it is safe inside KAbstractFileItemActionPlugin::actions().
class ContextMenuPolicy final
{
  public:
    using Lookup = std::function<RepositoryContextLookup(const QString& pathKey)>;

    static constexpr qsizetype MaxSelectionItems = 256;

    [[nodiscard]] static ContextMenuDecision decide(const QList<QUrl>& urls, const Lookup& lookup);
};

} // namespace LinuxGitShell::Dolphin
