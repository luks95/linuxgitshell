// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <QStringList>

namespace LinuxGitShell
{

[[nodiscard]] QStringList sanitizeGitArguments(const QStringList& arguments);

} // namespace LinuxGitShell
