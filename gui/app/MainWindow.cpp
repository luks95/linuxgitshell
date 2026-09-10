// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "MainWindow.h"

#include "linuxgitshell/gitcore/ProjectInfo.h"

#include <KLocalizedString>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace LinuxGitShell
{

MainWindow::MainWindow(const QString& requestedPath, QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(ProjectInfo::name());
    resize(640, 360);

    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);

    auto* title = new QLabel(QStringLiteral("<h1>%1</h1>").arg(ProjectInfo::name()), content);
    auto* status = new QLabel(i18n("Bootstrap ready"), content);
    auto* description = new QLabel(i18n("The Qt/KDE application shell is working. Git repository "
                                        "discovery will be added in Phase 1."),
                                   content);
    description->setWordWrap(true);

    const QString pathMessage = requestedPath.isEmpty() ? i18n("No repository path was provided.")
                                                        : i18n("Requested path: %1", requestedPath);
    auto* path = new QLabel(pathMessage, content);

    auto* versions =
        new QLabel(i18n("LinuxGitShell %1 · Qt %2 · KDE Frameworks %3", ProjectInfo::version(),
                        ProjectInfo::qtVersion(), ProjectInfo::kfVersion()),
                   content);

    layout->addWidget(title);
    layout->addWidget(status);
    layout->addWidget(description);
    layout->addWidget(path);
    layout->addStretch();
    layout->addWidget(versions);

    setCentralWidget(content);
}

} // namespace LinuxGitShell
