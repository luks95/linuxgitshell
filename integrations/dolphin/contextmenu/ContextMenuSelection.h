// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

namespace LinuxGitShell::Dolphin
{

class ContextMenuSelection final
{
  public:
    [[nodiscard]] static std::optional<QString> singleLocalPath(const QList<QUrl>& urls);
};

} // namespace LinuxGitShell::Dolphin
