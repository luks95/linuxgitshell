// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "ContextMenuSelection.h"

#include <QTest>
#include <QUrl>

using LinuxGitShell::Dolphin::ContextMenuSelection;

class DolphinContextMenuSelectionTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void rejectsEmptySelection();
    void rejectsRemoteSelection();
    void rejectsMultipleItems();
    void preservesSingleLocalPath();
};

void DolphinContextMenuSelectionTest::rejectsEmptySelection()
{
    QVERIFY(!ContextMenuSelection::singleLocalPath({}).has_value());
}

void DolphinContextMenuSelectionTest::rejectsRemoteSelection()
{
    const QList<QUrl> urls{QUrl(QStringLiteral("sftp://example.invalid/repository/file.cpp"))};

    QVERIFY(!ContextMenuSelection::singleLocalPath(urls).has_value());
}

void DolphinContextMenuSelectionTest::rejectsMultipleItems()
{
    const QList<QUrl> localUrls{QUrl::fromLocalFile(QStringLiteral("/tmp/one")),
                                QUrl::fromLocalFile(QStringLiteral("/tmp/two"))};
    const QList<QUrl> mixedUrls{QUrl::fromLocalFile(QStringLiteral("/tmp/one")),
                                QUrl(QStringLiteral("sftp://example.invalid/two"))};

    QVERIFY(!ContextMenuSelection::singleLocalPath(localUrls).has_value());
    QVERIFY(!ContextMenuSelection::singleLocalPath(mixedUrls).has_value());
}

void DolphinContextMenuSelectionTest::preservesSingleLocalPath()
{
    const QString path = QString::fromUtf8("/tmp/répôt with spaces/file\nname.cpp");
    const std::optional<QString> selectedPath =
        ContextMenuSelection::singleLocalPath({QUrl::fromLocalFile(path)});

    QVERIFY(selectedPath.has_value());
    QCOMPARE(selectedPath.value_or(QString()), path);
}

QTEST_GUILESS_MAIN(DolphinContextMenuSelectionTest)

#include "DolphinContextMenuSelectionTest.moc"
