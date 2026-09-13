// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include <KAbstractFileItemActionPlugin>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KPluginFactory>
#include <KPluginMetaData>

#include <QAction>
#include <QFile>
#include <QFileInfo>
#include <QScopedPointer>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QWidget>

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
    void exposesExpectedMetadata();
    void loadsAndCreatesConservativeActions();
    void launchesSelectedPathAsOneArgument();
};

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
