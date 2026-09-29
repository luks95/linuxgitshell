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
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QScopeGuard>
#include <QScopedPointer>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QWidget>

#include <algorithm>
#include <functional>
#include <vector>

namespace
{

KAbstractFileItemActionPlugin* loadPlugin(QObject* parent)
{
    const KPluginMetaData metadata(QStringLiteral(DOLPHIN_CONTEXT_PLUGIN_PATH));
    return KPluginFactory::instantiatePlugin<KAbstractFileItemActionPlugin>(metadata, parent)
        .plugin;
}

// Replaces the linuxgitshell executable with a helper that records its single argument.
class LaunchCapture final
{
  public:
    LaunchCapture() : originalPath(qgetenv("PATH"))
    {
        const QString helperPath = directory.filePath(QStringLiteral("linuxgitshell"));
        ready = directory.isValid() &&
                QFile::copy(QStringLiteral(DOLPHIN_PLUGIN_LAUNCH_HELPER_PATH), helperPath) &&
                QFile::setPermissions(helperPath, QFile::permissions(helperPath) | QFile::ExeOwner |
                                                      QFile::ExeUser);
        qputenv("PATH", QFile::encodeName(directory.path()));
        qputenv("LINUXGITSHELL_PLUGIN_TEST_OUTPUT", QFile::encodeName(outputPath()));
    }

    ~LaunchCapture()
    {
        qputenv("PATH", originalPath);
        qunsetenv("LINUXGITSHELL_PLUGIN_TEST_OUTPUT");
    }

    LaunchCapture(const LaunchCapture&) = delete;
    LaunchCapture& operator=(const LaunchCapture&) = delete;
    LaunchCapture(LaunchCapture&&) = delete;
    LaunchCapture& operator=(LaunchCapture&&) = delete;

    [[nodiscard]] bool isReady() const { return ready; }

    // Triggers the action and returns the argument received by the launched process.
    [[nodiscard]] QString launch(QAction* action) const
    {
        QFile::remove(outputPath());
        action->trigger();
        if (!QTest::qWaitFor([this] { return QFileInfo::exists(outputPath()); }, 5000))
        {
            return {};
        }
        QFile output(outputPath());
        return output.open(QIODevice::ReadOnly) ? QString::fromUtf8(output.readAll()) : QString();
    }

  private:
    [[nodiscard]] QString outputPath() const
    {
        return directory.filePath(QStringLiteral("argument.txt"));
    }

    QTemporaryDir directory;
    QByteArray originalPath;
    bool ready = false;
};

[[nodiscard]] KFileItemListProperties selectionOf(const QStringList& paths)
{
    KFileItemList items;
    for (const QString& path : paths)
    {
        items.append(KFileItem(QUrl::fromLocalFile(path), QStringLiteral("inode/directory")));
    }
    return KFileItemListProperties(items);
}

[[nodiscard]] QMenu* repositoryMenu(const QList<QAction*>& actions)
{
    return actions.size() == 1 ? actions.constFirst()->menu() : nullptr;
}

// Opens menus until the asynchronous context reply changes the result, as repeated right-clicks
// would in Dolphin. Returns the final actions, owned by `parentWidget`.
[[nodiscard]] QList<QAction*>
actionsWhen(KAbstractFileItemActionPlugin& plugin, const KFileItemListProperties& selection,
            QWidget& parentWidget, const std::function<bool(const QList<QAction*>&)>& predicate)
{
    QList<QAction*> actions;
    // Callers verify the returned actions, which also report a timeout.
    (void)QTest::qWaitFor(
        [&]
        {
            qDeleteAll(actions);
            actions = plugin.actions(selection, &parentWidget);
            return predicate(actions);
        },
        15000);
    return actions;
}

[[nodiscard]] QStringList actionTexts(const QMenu* menu)
{
    QStringList texts;
    for (const QAction* action : menu->actions())
    {
        if (!action->isSeparator())
        {
            texts.append(action->text() +
                         (action->isEnabled() ? QString() : QStringLiteral(" [disabled]")));
        }
    }
    return texts;
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
    void showsRepositoryMenuOnceContextIsWarm();
    void showsOperationInProgress();
    void hidesMenuOutsideRepository();
    void opensRootForSelectionInOneRepository();
    void offersNothingForSelectionAcrossRepositories();
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
    const LaunchCapture capture;
    QVERIFY(capture.isReady());
    QScopedPointer<KAbstractFileItemActionPlugin> plugin(loadPlugin(nullptr));
    QVERIFY(plugin);
    QWidget parentWidget;
    const QString selectedPath = QString::fromUtf8("/tmp/répôt with spaces/file\nname.cpp");
    const KFileItem localItem(QUrl::fromLocalFile(selectedPath),
                              QStringLiteral("application/octet-stream"));
    const QList<QAction*> actions =
        plugin->actions(KFileItemListProperties(KFileItemList{localItem}), &parentWidget);
    QCOMPARE(actions.size(), 1);

    QCOMPARE(capture.launch(actions.constFirst()), selectedPath);
}

void DolphinContextMenuPluginTest::showsRepositoryMenuOnceContextIsWarm()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("repo"));
    QVERIFY(GitTestSupport::createRepository(root));
    const QString file = QDir(root).filePath(QStringLiteral("file.txt"));

    QScopedPointer<KAbstractFileItemActionPlugin> plugin(loadPlugin(nullptr));
    QVERIFY(plugin);
    QWidget parentWidget;
    const KFileItemListProperties selection = selectionOf({file});

    // The first menu is cold and offers the generic action while the context is requested.
    const QList<QAction*> cold = plugin->actions(selection, &parentWidget);
    QCOMPARE(cold.size(), 1);
    QCOMPARE(cold.constFirst()->text(), QStringLiteral("Open with LinuxGitShell"));

    const QList<QAction*> warm =
        actionsWhen(*plugin, selection, parentWidget, [](const QList<QAction*>& actions)
                    { return repositoryMenu(actions) != nullptr; });
    QMenu* menu = repositoryMenu(warm);
    QVERIFY(menu != nullptr);
    QCOMPARE(menu->title(), QStringLiteral("LinuxGitShell"));
    QCOMPARE(actionTexts(menu), QStringList{QStringLiteral("Show Status")});

    const LaunchCapture capture;
    QVERIFY(capture.isReady());
    QCOMPARE(capture.launch(menu->actions().constFirst()), file);
}

void DolphinContextMenuPluginTest::showsOperationInProgress()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("merging"));
    QVERIFY(GitTestSupport::createRepository(root));
    QVERIFY(GitTestSupport::writeTextFile(QDir(root).filePath(QStringLiteral(".git/MERGE_HEAD")),
                                          "0000000000000000000000000000000000000000\n"));

    QScopedPointer<KAbstractFileItemActionPlugin> plugin(loadPlugin(nullptr));
    QVERIFY(plugin);
    QWidget parentWidget;
    const QList<QAction*> actions =
        actionsWhen(*plugin, selectionOf({root}), parentWidget, [](const QList<QAction*>& actions)
                    { return repositoryMenu(actions) != nullptr; });

    QMenu* menu = repositoryMenu(actions);
    QVERIFY(menu != nullptr);
    QCOMPARE(actionTexts(menu), (QStringList{QStringLiteral("Show Status"),
                                             QStringLiteral("Merge in progress [disabled]")}));
}

void DolphinContextMenuPluginTest::hidesMenuOutsideRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString outside = directory.filePath(QStringLiteral("plain"));
    QVERIFY(QDir().mkpath(outside));

    QScopedPointer<KAbstractFileItemActionPlugin> plugin(loadPlugin(nullptr));
    QVERIFY(plugin);
    QWidget parentWidget;
    const QList<QAction*> actions =
        actionsWhen(*plugin, selectionOf({outside}), parentWidget,
                    [](const QList<QAction*>& actions) { return actions.isEmpty(); });

    QVERIFY(actions.isEmpty());
}

void DolphinContextMenuPluginTest::opensRootForSelectionInOneRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("repo"));
    QVERIFY(GitTestSupport::createRepository(root));
    const QString first = QDir(root).filePath(QStringLiteral("file.txt"));
    const QString second = QDir(root).filePath(QStringLiteral("other.txt"));
    QVERIFY(GitTestSupport::writeTextFile(second, "other\n"));

    QScopedPointer<KAbstractFileItemActionPlugin> plugin(loadPlugin(nullptr));
    QVERIFY(plugin);
    QWidget parentWidget;
    const KFileItemListProperties selection = selectionOf({first, second});

    // Cold multiple selections offer nothing, but request both items.
    QVERIFY(plugin->actions(selection, &parentWidget).isEmpty());
    const QList<QAction*> actions =
        actionsWhen(*plugin, selection, parentWidget, [](const QList<QAction*>& actions)
                    { return repositoryMenu(actions) != nullptr; });
    QMenu* menu = repositoryMenu(actions);
    QVERIFY(menu != nullptr);

    const LaunchCapture capture;
    QVERIFY(capture.isReady());
    QCOMPARE(capture.launch(menu->actions().constFirst()), QFileInfo(root).canonicalFilePath());
}

void DolphinContextMenuPluginTest::offersNothingForSelectionAcrossRepositories()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString first = directory.filePath(QStringLiteral("first"));
    const QString second = directory.filePath(QStringLiteral("second"));
    QVERIFY(GitTestSupport::createRepository(first));
    QVERIFY(GitTestSupport::createRepository(second));

    QScopedPointer<KAbstractFileItemActionPlugin> plugin(loadPlugin(nullptr));
    QVERIFY(plugin);
    QWidget parentWidget;
    for (const QString& root : {first, second})
    {
        QVERIFY(repositoryMenu(actionsWhen(
                    *plugin, selectionOf({root}), parentWidget, [](const QList<QAction*>& actions)
                    { return repositoryMenu(actions) != nullptr; })) != nullptr);
    }

    QVERIFY(plugin->actions(selectionOf({first, second}), &parentWidget).isEmpty());
}

QTEST_MAIN(DolphinContextMenuPluginTest)

#include "DolphinContextMenuPluginTest.moc"
