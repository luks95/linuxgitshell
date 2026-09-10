// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/ProjectInfo.h"

#include <QtGlobal>
#include <kcoreaddons_version.h>

namespace LinuxGitShell
{

QString ProjectInfo::name() { return QStringLiteral("LinuxGitShell"); }

QString ProjectInfo::version() { return QStringLiteral(LINUXGITSHELL_VERSION); }

QString ProjectInfo::qtVersion() { return QString::fromLatin1(qVersion()); }

QString ProjectInfo::kfVersion() { return QStringLiteral(KCOREADDONS_VERSION_STRING); }

} // namespace LinuxGitShell
