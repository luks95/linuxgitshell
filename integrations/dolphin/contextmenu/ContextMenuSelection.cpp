// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "ContextMenuSelection.h"

namespace LinuxGitShell::Dolphin
{

std::optional<QString> ContextMenuSelection::singleLocalPath(const QList<QUrl>& urls)
{
    if (urls.size() != 1)
    {
        return std::nullopt;
    }

    const QUrl& url = urls.constFirst();
    if (!url.isLocalFile())
    {
        return std::nullopt;
    }

    QString path = url.toLocalFile();
    if (path.isEmpty())
    {
        return std::nullopt;
    }

    return path;
}

} // namespace LinuxGitShell::Dolphin
