// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "OverlayIconPlugin.h"

#include "OverlayClient.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QLoggingCategory>

namespace LinuxGitShell::Dolphin
{

Q_DECLARE_LOGGING_CATEGORY(dolphinOverlayLog)

namespace
{

// Overlays default to on when the plugin is installed. Settings are read once at start-up; the
// full settings interface and live reload belong to the Settings GUI phase.
[[nodiscard]] bool overlaysEnabledInSettings()
{
    const KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("linuxgitshellrc"));
    return config->group(QStringLiteral("Overlays")).readEntry("Enabled", true);
}

} // namespace

OverlayIconPlugin::OverlayIconPlugin()
{
    // No D-Bus work here: a disabled start-up performs no requests or background work, and an
    // enabled one only connects signals. Requests start from the first paint callback's event loop.
    OverlayClient& client = OverlayClient::instance();
    if (!overlaysEnabledInSettings())
    {
        client.setEnabled(false);
        qCDebug(dolphinOverlayLog) << "Overlays are disabled in settings";
    }
    connect(&client, &OverlayClient::iconsChanged, this,
            [this](const QString& pathKey, const QStringList& icons)
            { Q_EMIT overlaysChanged(QUrl::fromLocalFile(pathKey), icons); });
}

QStringList OverlayIconPlugin::getOverlays(const QUrl& item)
{
    return OverlayClient::instance().lookup(OverlayClient::pathKeyForUrl(item));
}

} // namespace LinuxGitShell::Dolphin
