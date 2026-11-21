// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <KOverlayIconPlugin>

#include <QStringList>
#include <QUrl>

namespace LinuxGitShell::Dolphin
{

// Thin KIO adapter: getOverlays() is a paint callback, so it only consults the process-wide
// OverlayClient cache. Git execution, polling, and D-Bus traffic stay outside Dolphin's paint path.
class OverlayIconPlugin final : public KOverlayIconPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.overlayicon.linuxgitshell")

  public:
    OverlayIconPlugin();

    [[nodiscard]] QStringList getOverlays(const QUrl& item) override;
};

} // namespace LinuxGitShell::Dolphin
