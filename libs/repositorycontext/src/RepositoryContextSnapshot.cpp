// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/repositorycontext/RepositoryContextSnapshot.h"

#include <QDir>

namespace LinuxGitShell
{

QString repositoryContextPathKey(const QString& localPath)
{
    if (localPath.isEmpty() || !QDir::isAbsolutePath(localPath))
    {
        return {};
    }

    return QDir::cleanPath(localPath);
}

} // namespace LinuxGitShell
