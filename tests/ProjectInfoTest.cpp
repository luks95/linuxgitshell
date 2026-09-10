// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/ProjectInfo.h"

#include <QTest>

class ProjectInfoTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void exposesBuildInformation();
};

void ProjectInfoTest::exposesBuildInformation()
{
    QCOMPARE(LinuxGitShell::ProjectInfo::name(), QStringLiteral("LinuxGitShell"));
    QVERIFY(!LinuxGitShell::ProjectInfo::version().isEmpty());
    QVERIFY(!LinuxGitShell::ProjectInfo::qtVersion().isEmpty());
    QVERIFY(!LinuxGitShell::ProjectInfo::kfVersion().isEmpty());
}

QTEST_GUILESS_MAIN(ProjectInfoTest)

#include "ProjectInfoTest.moc"
