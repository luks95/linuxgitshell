// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "OverlaySettings.h"

#include <KConfigGroup>

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace LinuxGitShell;

class OverlaySettingsTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void recursionDefaultsToOn();
    void recursionCanBeDisabled();
    void unrelatedGroupsAreIgnored();

  private:
    [[nodiscard]] static KSharedConfigPtr open(const QTemporaryDir& directory,
                                               const QByteArray& text);
};

KSharedConfigPtr OverlaySettingsTest::open(const QTemporaryDir& directory, const QByteArray& text)
{
    // An isolated file, so a contributor's own configuration is never read.
    const QString path = QDir(directory.path()).filePath(QStringLiteral("linuxgitshellrc"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(text) != text.size())
    {
        return {};
    }
    file.close();
    return KSharedConfig::openConfig(path, KConfig::SimpleConfig);
}

void OverlaySettingsTest::recursionDefaultsToOn()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const KSharedConfigPtr config = open(directory, "");
    QVERIFY(config);
    QVERIFY(overlayProviderOptions(config).recursive);
}

void OverlaySettingsTest::recursionCanBeDisabled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const KSharedConfigPtr config = open(directory, "[Overlays]\nRecursiveFolders=false\n");
    QVERIFY(config);
    QVERIFY(!overlayProviderOptions(config).recursive);
}

void OverlaySettingsTest::unrelatedGroupsAreIgnored()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    // Overlays disabled in Dolphin does not change how the daemon propagates folder states.
    const KSharedConfigPtr config =
        open(directory, "[Overlays]\nEnabled=false\n[Other]\nRecursiveFolders=false\n");
    QVERIFY(config);
    QVERIFY(overlayProviderOptions(config).recursive);
}

QTEST_GUILESS_MAIN(OverlaySettingsTest)
#include "OverlaySettingsTest.moc"
