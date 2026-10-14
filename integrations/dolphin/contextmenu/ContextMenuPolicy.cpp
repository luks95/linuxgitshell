// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "ContextMenuPolicy.h"

#include "linuxgitshell/repositorycontext/RepositoryContextSnapshot.h"

#include <optional>

namespace LinuxGitShell::Dolphin
{
namespace
{

struct SelectedItem
{
    QString path;
    QString pathKey;
};

[[nodiscard]] std::optional<QList<SelectedItem>> localItems(const QList<QUrl>& urls)
{
    if (urls.isEmpty() || urls.size() > ContextMenuPolicy::MaxSelectionItems)
    {
        return std::nullopt;
    }

    QList<SelectedItem> items;
    items.reserve(urls.size());
    for (const QUrl& url : urls)
    {
        if (!url.isLocalFile())
        {
            return std::nullopt;
        }
        SelectedItem item{url.toLocalFile(), QString()};
        item.pathKey = repositoryContextPathKey(item.path);
        if (item.pathKey.isEmpty())
        {
            return std::nullopt;
        }
        items.append(item);
    }
    return items;
}

[[nodiscard]] ContextMenuDecision decideSingle(const SelectedItem& item,
                                               const ContextMenuPolicy::Lookup& lookup)
{
    ContextMenuDecision decision;
    const RepositoryContextLookup entry = lookup(item.pathKey);
    if (!entry.snapshot.has_value())
    {
        decision.kind = ContextMenuKind::Generic;
        decision.launchPath = item.path;
        decision.refreshKeys = {item.pathKey};
        return decision;
    }

    const RepositoryContextSnapshot& snapshot = entry.snapshot.value();
    switch (snapshot.state)
    {
    case RepositoryContextState::InsideRepository:
        decision.kind = ContextMenuKind::Repository;
        decision.launchPath = item.path;
        decision.operations = snapshot.operations;
        break;
    case RepositoryContextState::OutsideRepository:
        decision.kind = ContextMenuKind::None;
        break;
    case RepositoryContextState::Unavailable:
    case RepositoryContextState::DiscoveryError:
        // The application reports the typed failure when opened.
        decision.kind = ContextMenuKind::Generic;
        decision.launchPath = item.path;
        break;
    }
    return decision;
}

[[nodiscard]] ContextMenuDecision decideMultiple(const QList<SelectedItem>& items,
                                                 const ContextMenuPolicy::Lookup& lookup)
{
    ContextMenuDecision decision;
    QString sharedRoot;
    bool sameRepository = true;

    for (const SelectedItem& item : items)
    {
        const RepositoryContextLookup entry = lookup(item.pathKey);
        if (!entry.snapshot.has_value())
        {
            sameRepository = false;
            if (!decision.refreshKeys.contains(item.pathKey))
            {
                decision.refreshKeys.append(item.pathKey);
            }
            continue;
        }

        const RepositoryContextSnapshot& snapshot = entry.snapshot.value();
        if (sharedRoot.isEmpty())
        {
            sharedRoot = snapshot.repositoryRoot;
            decision.operations = snapshot.operations;
        }
        if (snapshot.state != RepositoryContextState::InsideRepository ||
            snapshot.repositoryRoot != sharedRoot)
        {
            sameRepository = false;
        }
    }

    if (!sameRepository || sharedRoot.isEmpty())
    {
        decision.operations.clear();
        return decision;
    }
    decision.kind = ContextMenuKind::Repository;
    decision.launchPath = sharedRoot;
    return decision;
}

} // namespace

ContextMenuDecision ContextMenuPolicy::decide(const QList<QUrl>& urls, const Lookup& lookup)
{
    const std::optional<QList<SelectedItem>> items = localItems(urls);
    if (!items.has_value())
    {
        return {};
    }
    if (items->size() == 1)
    {
        return decideSingle(items->constFirst(), lookup);
    }
    return decideMultiple(items.value(), lookup);
}

} // namespace LinuxGitShell::Dolphin
