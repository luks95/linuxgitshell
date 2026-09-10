// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

namespace LinuxGitShell
{

class ProjectInfo final
{
  public:
    [[nodiscard]] static QString name();
    [[nodiscard]] static QString version();
    [[nodiscard]] static QString qtVersion();
    [[nodiscard]] static QString kfVersion();
};

} // namespace LinuxGitShell
