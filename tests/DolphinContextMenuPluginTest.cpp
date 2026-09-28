// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "DBusTestSupport.h"
#include "GitTestSupport.h"

#include <KAbstractFileItemActionPlugin>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KPluginFactory>
#include <KPluginMetaData>

#include <QAction>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QScopedPointer>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QWidget>

#include <algorithm>
#include <vector>

namespace
{

KAbstractFileItemActionPlugin* loadPlugin(QObject* parent)
{
    const KPluginMetaData metadata(QStringLiteral(DOLPHIN_CONTEXT_PLUGIN_PATH));
    return KPluginFactory::instantiatePlugin<KAbstractFileItemActionPlugin>(metadata, parent)
        .plugin;
}

} // namespace

class DolphinContextMenuPluginTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    // Runs first, before other tests trigger service activation from the event loop.
    void buildsMenuWithoutWaitingForGitOrService();
    void exposesExpectedMetadata();
    void loadsAndCreatesConservativeActions();
    void launchesSelectedPathAsOneArgument();
};

void DolphinContextMenuPluginTest::initTestCase()
{
    GitTestSupport::isolateGitConfiguration();
    QVERIFY2(QDBusConnection::sessionBus().isConnected(), "run this test through dbus-run-session");
}

void DolphinContextMenuPluginTest::buildsMenuWithoutWaitingForGitOrService()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("repo"));
    QVERIFY(GitTestSupport::createRepository(root));

    // A git executable that records any invocation from this process. The activated service
    // inherits the bus environment, not this PATH, so only in-process Git would reach it.
    const QString fakeGitDirectory = directory.filePath(QStringLiteral("fake-git"));
    const QString marker = directory.filePath(QStringLiteral("git-was-run"));
    QVERIFY(QDir().mkpath(fakeGitDirectory));
    const QString fakeGit = QDir(fakeGitDirectory).filePath(QStringLiteral("git"));
    QVERIFY(GitTestSupport::writeTextFile(fakeGit, QByteArrayLiteral("#!/bin/sh\ntouch \"") +
                                                       QFile::encodeName(marker) +
                                                       QByteArrayLiteral("\"\n")));
    QVERIFY(QFile::setPermissions(fakeGit, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    const QByteArray originalPath = qgetenv("PATH");
    qputenv("PATH", QFile::encodeName(fakeGitDirectory) + ':' + originalPath);
    const auto restorePath = qScopeGuard([&originalPath] { qputenv("PATH", originalPath); });

    QVERIFY(DBusTestSupport::stopContextService());
    QScopedPointer<KAbstractFileItemActionPlugin> plugin(loadPlugin(nullptr));
    QVERIFY(plugin);
    QWidget parentWidget;
    const KFileItem item(QUrl::fromLocalFile(root), QStringLiteral("inode/directory"));
    const KFileItemListProperties selection(KFileItemList{item});

    constexpr int Iterations = 200;
    std::vector<qint64> nanoseconds;
    nanoseconds.reserve(Iterations);
    for (int iteration = 0; iteration < Iterations; ++iteration)
    {
        QElapsedTimer timer;
        timer.start();
        const QList<QAction*> actions = plugin->actions(selection, &parentWidget);
        nanoseconds.push_back(timer.nsecsElapsed());
        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions.constFirst()->text(), QStringLiteral("Open with LinuxGitShell"));
        qDeleteAll(actions);
    }

    // A synchronous request from actions() would already have activated the service.
    QVERIFY(!DBusTestSupport::isContextServiceRegistered());

    std::sort(nanoseconds.begin(), nanoseconds.end());
    const auto percentile = [&nanoseconds](std::size_t value)
    { return static_cast<double>(nanoseconds.at(nanoseconds.size() * value / 100)) / 1e6; };
    const double p95 = percentile(95);
    qInfo("actions() latency over %d calls: p50 %.3f ms, p95 %.3f ms, max %.3f ms", Iterations,
          percentile(50), p95, static_cast<double>(nanoseconds.back()) / 1e6);
    // The 2 ms budget is measured natively; this bound only catches blocking work under CI load.
    QVERIFY2(p95 < 20.0, "actions() took longer than a non-blocking lookup should");

    // The deferred request leaves from the event loop and activates the service.
    QVERIFY(QTest::qWaitFor([] { return DBusTestSupport::isContextServiceRegistered(); }, 15000));
    QTest::qWait(500);
    QVERIFY(!QFileInfo::exists(marker));
}

void DolphinContextMenuPluginTest::exposesExpectedMetadata()
{
    const KPluginMetaData metadata(QStringLiteral(DOLPHIN_CONTEXT_PLUGIN_PATH));

    QVERIFY(metadata.isValid());
    QCOMPARE(metadata.name(), QStringLiteral("LinuxGitShell"));
    QVERIFY(metadata.mimeTypes().contains(QStringLiteral("application/octet-stream")));
    QVERIFY(metadata.mimeTypes().contains(QStringLiteral("inode/directory")));
}

void DolphinContextMenuPluginTest::loadsAndCreatesConservativeActions()
{
    QScopedPointer<KAbstractFileItemActionPlugin> plugin(loadPlugin(nullptr));
    QVERIFY(plugin);
    QWidget parentWidget;

    const KFileItem localItem(QUrl::fromLocalFile(QStringLiteral("/tmp/repository")),
                              QStringLiteral("inode/directory"));
    const KFileItemListProperties localSelection(KFileItemList{localItem});
    const QList<QAction*> localActions = plugin->actions(localSelection, &parentWidget);
    QCOMPARE(localActions.size(), 1);
    QCOMPARE(localActions.constFirst()->text(), QStringLiteral("Open with LinuxGitShell"));
    QCOMPARE(localActions.constFirst()->parent(), static_cast<QObject*>(&parentWidget));

    const KFileItem remoteItem(QUrl(QStringLiteral("sftp://example.invalid/repository")),
                               QStringLiteral("inode/directory"));
    const KFileItemListProperties remoteSelection(KFileItemList{remoteItem});
    QVERIFY(plugin->actions(remoteSelection, &parentWidget).isEmpty());

    const KFileItemListProperties multipleSelection(KFileItemList{localItem, localItem});
    QVERIFY(plugin->actions(multipleSelection, &parentWidget).isEmpty());
}

void DolphinContextMenuPluginTest::launchesSelectedPathAsOneArgument()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString helperPath = temporaryDirectory.filePath(QStringLiteral("linuxgitshell"));
    QVERIFY(QFile::copy(QStringLiteral(DOLPHIN_PLUGIN_LAUNCH_HELPER_PATH), helperPath));
    const QFile::Permissions permissions =
        QFile::permissions(helperPath) | QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup;
    QVERIFY(QFile::setPermissions(helperPath, permissions));

    const QString outputPath = temporaryDirectory.filePath(QStringLiteral("argument.txt"));
    const QByteArray originalPath = qgetenv("PATH");
    qputenv("PATH", QFile::encodeName(temporaryDirectory.path()));
    qputenv("LINUXGITSHELL_PLUGIN_TEST_OUTPUT", QFile::encodeName(outputPath));

    QScopedPointer<KAbstractFileItemActionPlugin> plugin(loadPlugin(nullptr));
    QVERIFY(plugin);
    QWidget parentWidget;
    const QString selectedPath = QString::fromUtf8("/tmp/répôt with spaces/file\nname.cpp");
    const KFileItem localItem(QUrl::fromLocalFile(selectedPath),
                              QStringLiteral("application/octet-stream"));
    const KFileItemListProperties localSelection(KFileItemList{localItem});
    const QList<QAction*> actions = plugin->actions(localSelection, &parentWidget);
    QCOMPARE(actions.size(), 1);

    actions.constFirst()->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(outputPath), 5000);

    QFile output(outputPath);
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(output.readAll()), selectedPath);

    qputenv("PATH", originalPath);
    qunsetenv("LINUXGITSHELL_PLUGIN_TEST_OUTPUT");
}

QTEST_MAIN(DolphinContextMenuPluginTest)

#include "DolphinContextMenuPluginTest.moc"
