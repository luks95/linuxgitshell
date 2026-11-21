// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

// Loads the built overlay module the way KIO does and runs on the private bus of dbus-run-session.
// It is registered twice: with overlays enabled and, through an isolated KConfig directory, with
// overlays disabled. Neither run reads a contributor's own settings.

#include "DBusTestSupport.h"
#include "GitTestSupport.h"

#include <KOverlayIconPlugin>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QElapsedTimer>
#include <QPluginLoader>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <algorithm>
#include <vector>

namespace
{

constexpr int WaitMilliseconds = 20000;
constexpr int PaintCallbacks = 10000;

[[nodiscard]] bool isRegistered()
{
    return QDBusConnection::sessionBus()
        .interface()
        ->isServiceRegistered(QStringLiteral("org.linuxgitshell.Experimental.Overlays1"))
        .value();
}

[[nodiscard]] bool disabledRun()
{
    return qEnvironmentVariableIsSet("LINUXGITSHELL_TEST_OVERLAYS_DISABLED");
}

} // namespace

class DolphinOverlayPluginTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void loadsAsAKioOverlayPlugin();
    void paintCallbacksNeverTouchGitOrTheBus();
    void showsRealStatesThroughTheDaemon();
    void disabledPluginPerformsNoWork();

  private:
    [[nodiscard]] KOverlayIconPlugin* plugin();

    QPluginLoader loader{QStringLiteral(OVERLAY_PLUGIN_PATH)};
};

void DolphinOverlayPluginTest::initTestCase()
{
    GitTestSupport::isolateGitConfiguration();
    QVERIFY2(QDBusConnection::sessionBus().isConnected(), "run this test through dbus-run-session");
    QVERIFY(DBusTestSupport::stopContextService());
}

void DolphinOverlayPluginTest::cleanupTestCase() { QVERIFY(DBusTestSupport::stopContextService()); }

KOverlayIconPlugin* DolphinOverlayPluginTest::plugin()
{
    return qobject_cast<KOverlayIconPlugin*>(loader.instance());
}

void DolphinOverlayPluginTest::loadsAsAKioOverlayPlugin()
{
    QVERIFY2(loader.load(), qPrintable(loader.errorString()));
    QCOMPARE(loader.metaData().value(QStringLiteral("IID")).toString(),
             QStringLiteral("org.kde.overlayicon.linuxgitshell"));
    QVERIFY(plugin() != nullptr);
    // Loading and constructing the plugin neither starts nor contacts the service.
    QVERIFY(!isRegistered());
}

void DolphinOverlayPluginTest::paintCallbacksNeverTouchGitOrTheBus()
{
    if (disabledRun())
    {
        QSKIP("covered by the enabled run");
    }
    KOverlayIconPlugin* overlay = plugin();
    QVERIFY(overlay != nullptr);
    QSignalSpy changes(overlay, &KOverlayIconPlugin::overlaysChanged);

    // Pseudo-filesystem paths are answered by the daemon without any Git work, so the later
    // requests stay cheap while the callbacks below are measured.
    QList<QUrl> urls;
    urls.reserve(PaintCallbacks);
    for (int index = 0; index < PaintCallbacks; ++index)
    {
        urls.append(QUrl::fromLocalFile(QStringLiteral("/proc/overlay-test/%1").arg(index)));
    }
    // Non-local items have no overlay at all.
    QCOMPARE(overlay->getOverlays(QUrl(QStringLiteral("https://example.org/file"))), QStringList());
    QCOMPARE(overlay->getOverlays(QUrl(QStringLiteral("trash:/file"))), QStringList());

    std::vector<qint64> nanoseconds;
    nanoseconds.reserve(PaintCallbacks);
    QElapsedTimer total;
    total.start();
    for (const QUrl& url : std::as_const(urls))
    {
        QElapsedTimer call;
        call.start();
        QVERIFY(overlay->getOverlays(url).isEmpty());
        nanoseconds.push_back(call.nsecsElapsed());
    }
    const qint64 elapsed = total.elapsed();

    // Not one request, Git command, or activation happened before returning to the event loop.
    QVERIFY(!isRegistered());
    QCOMPARE(changes.size(), 0);

    std::ranges::sort(nanoseconds);
    qInfo().nospace() << "getOverlays() x" << PaintCallbacks << " cold: total " << elapsed
                      << " ms, p50 " << nanoseconds[nanoseconds.size() / 2] << " ns, p95 "
                      << nanoseconds[nanoseconds.size() * 95 / 100] << " ns, max "
                      << nanoseconds.back() << " ns";
}

void DolphinOverlayPluginTest::showsRealStatesThroughTheDaemon()
{
    if (disabledRun())
    {
        QSKIP("covered by the enabled run");
    }
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = QDir(directory.path()).canonicalPath() + QStringLiteral("/repo");
    QVERIFY(GitTestSupport::createRepository(root));
    QVERIFY(GitTestSupport::writeTextFile(root + QStringLiteral("/file.txt"), "changed\n"));
    QVERIFY(GitTestSupport::writeTextFile(root + QStringLiteral("/untracked.txt"), "u\n"));

    KOverlayIconPlugin* overlay = plugin();
    QVERIFY(overlay != nullptr);
    QSignalSpy changes(overlay, &KOverlayIconPlugin::overlaysChanged);
    const QUrl modified = QUrl::fromLocalFile(root + QStringLiteral("/file.txt"));
    const QUrl untracked = QUrl::fromLocalFile(root + QStringLiteral("/untracked.txt"));
    QCOMPARE(overlay->getOverlays(modified), QStringList());
    QCOMPARE(overlay->getOverlays(untracked), QStringList());

    // The answer arrives as a repaint request, never as a blocking call.
    QTRY_VERIFY_WITH_TIMEOUT(
        std::any_of(changes.cbegin(), changes.cend(),
                    [&modified](const QList<QVariant>& arguments)
                    {
                        return arguments.at(0).toUrl() == modified &&
                               arguments.at(1).toStringList() ==
                                   QStringList{QStringLiteral("emblem-linuxgitshell-modified")};
                    }),
        WaitMilliseconds);
    QTRY_COMPARE_WITH_TIMEOUT(overlay->getOverlays(modified),
                              QStringList{QStringLiteral("emblem-linuxgitshell-modified")},
                              WaitMilliseconds);
    QTRY_COMPARE_WITH_TIMEOUT(overlay->getOverlays(untracked),
                              QStringList{QStringLiteral("emblem-linuxgitshell-untracked")},
                              WaitMilliseconds);
    QVERIFY(isRegistered());
}

void DolphinOverlayPluginTest::disabledPluginPerformsNoWork()
{
    if (!disabledRun())
    {
        QSKIP("covered by the disabled run");
    }
    KOverlayIconPlugin* overlay = plugin();
    QVERIFY(overlay != nullptr);
    QSignalSpy changes(overlay, &KOverlayIconPlugin::overlaysChanged);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = QDir(directory.path()).canonicalPath() + QStringLiteral("/repo");
    QVERIFY(GitTestSupport::createRepository(root));
    const QUrl file = QUrl::fromLocalFile(root + QStringLiteral("/file.txt"));

    for (int index = 0; index < PaintCallbacks; ++index)
    {
        QVERIFY(overlay->getOverlays(file).isEmpty());
    }
    // Disabled in settings: no request, no activation, no timers, no repaint requests.
    QTest::qWait(1000);
    QVERIFY(!isRegistered());
    QCOMPARE(changes.size(), 0);
    QVERIFY(overlay->getOverlays(file).isEmpty());
}

QTEST_GUILESS_MAIN(DolphinOverlayPluginTest)
#include "DolphinOverlayPluginTest.moc"
