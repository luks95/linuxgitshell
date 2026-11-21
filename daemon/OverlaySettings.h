// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "OverlayStatusProvider.h"

#include <KSharedConfig>

namespace LinuxGitShell
{

// Reads the daemon-side overlay settings from the `[Overlays]` group. Recursive propagation of
// folder states defaults to on and is independent of whether overlays are enabled in Dolphin.
// Settings are read once at daemon start-up; restart the daemon to apply a change.
[[nodiscard]] OverlayProviderOptions overlayProviderOptions(const KSharedConfigPtr& config);

} // namespace LinuxGitShell
