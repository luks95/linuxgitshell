// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "OverlaySettings.h"

#include <KConfigGroup>

namespace LinuxGitShell
{

OverlayProviderOptions overlayProviderOptions(const KSharedConfigPtr& config)
{
    OverlayProviderOptions options;
    options.recursive =
        config->group(QStringLiteral("Overlays")).readEntry("RecursiveFolders", true);
    return options;
}

} // namespace LinuxGitShell
