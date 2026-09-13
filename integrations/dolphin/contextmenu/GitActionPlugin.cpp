// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "GitActionPlugin.h"

#include "ContextMenuSelection.h"

#include <KFileItemListProperties>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>
#include <QIcon>
#include <QLoggingCategory>
#include <QProcess>
#include <QStringList>
#include <QWidget>

namespace LinuxGitShell::Dolphin
{

Q_LOGGING_CATEGORY(dolphinContextMenuLog, "linuxgitshell.dolphin.contextmenu")

K_PLUGIN_CLASS_WITH_JSON(GitActionPlugin, "linuxgitshellfileitemaction.json")

GitActionPlugin::GitActionPlugin(QObject* parent, const QVariantList& arguments)
    : KAbstractFileItemActionPlugin(parent)
{
    Q_UNUSED(arguments)
    qCDebug(dolphinContextMenuLog) << "Loaded LinuxGitShell file-item action plugin";
}

QList<QAction*> GitActionPlugin::actions(const KFileItemListProperties& fileItemInfos,
                                         QWidget* parentWidget)
{
    const std::optional<QString> path =
        ContextMenuSelection::singleLocalPath(fileItemInfos.urlList());
    if (!path.has_value())
    {
        return {};
    }

    auto* action = new QAction(QIcon::fromTheme(QStringLiteral("git")),
                               i18nd("linuxgitshell", "Open with LinuxGitShell"), parentWidget);

    connect(action, &QAction::triggered, this,
            [this, selectedPath = *path]()
            {
                const bool started = QProcess::startDetached(QStringLiteral("linuxgitshell"),
                                                             QStringList{selectedPath});
                if (!started)
                {
                    qCWarning(dolphinContextMenuLog)
                        << "Could not start the LinuxGitShell application";
                    Q_EMIT error(i18nd("linuxgitshell", "Could not start LinuxGitShell."));
                }
            });

    return {action};
}

} // namespace LinuxGitShell::Dolphin

#include "GitActionPlugin.moc"
