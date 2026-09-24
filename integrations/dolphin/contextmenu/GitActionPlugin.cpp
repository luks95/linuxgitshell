// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "GitActionPlugin.h"

#include "ContextMenuPolicy.h"
#include "RepositoryContextClient.h"

#include <KFileItemListProperties>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>
#include <QIcon>
#include <QLoggingCategory>
#include <QMenu>
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
    // Memory-only lookups: cold or stale entries are refreshed from the event loop after this
    // menu is built, so a later menu can use them. Git, discovery, and IPC never run here.
    RepositoryContextClient& contextClient = RepositoryContextClient::instance();
    const ContextMenuDecision decision =
        ContextMenuPolicy::decide(fileItemInfos.urlList(), [&contextClient](const QString& pathKey)
                                  { return contextClient.lookup(pathKey); });
    if (!decision.refreshKeys.isEmpty())
    {
        contextClient.refreshLater(decision.refreshKeys);
    }

    switch (decision.kind)
    {
    case ContextMenuKind::None:
        return {};
    case ContextMenuKind::Generic:
    {
        auto* open = new QAction(i18nd("linuxgitshell", "Open with LinuxGitShell"), parentWidget);
        launchOnTrigger(open, decision.launchPath);
        return {open};
    }
    case ContextMenuKind::Repository:
        break;
    }

    auto* menu = new QMenu(i18nd("linuxgitshell", "LinuxGitShell"), parentWidget);
    menu->setIcon(QIcon::fromTheme(QStringLiteral("git")));
    auto* showStatus = new QAction(i18nd("linuxgitshell", "Show Status"), menu);
    launchOnTrigger(showStatus, decision.launchPath);
    menu->addAction(showStatus);
    if (!decision.operations.isEmpty())
    {
        menu->addSeparator();
        for (const RepositoryContextOperation operation : decision.operations)
        {
            auto* notice = new QAction(operationText(operation), menu);
            notice->setEnabled(false);
            menu->addAction(notice);
        }
    }
    return {menu->menuAction()};
}

void GitActionPlugin::launchOnTrigger(QAction* action, const QString& path)
{
    action->setIcon(QIcon::fromTheme(QStringLiteral("git")));
    connect(action, &QAction::triggered, this,
            [this, path]()
            {
                // The path is one process argument; no shell is involved.
                if (!QProcess::startDetached(QStringLiteral("linuxgitshell"), QStringList{path}))
                {
                    qCWarning(dolphinContextMenuLog)
                        << "Could not start the LinuxGitShell application";
                    Q_EMIT error(i18nd("linuxgitshell", "Could not start LinuxGitShell."));
                }
            });
}

QString GitActionPlugin::operationText(RepositoryContextOperation operation)
{
    switch (operation)
    {
    case RepositoryContextOperation::Merge:
        return i18nd("linuxgitshell", "Merge in progress");
    case RepositoryContextOperation::Rebase:
        return i18nd("linuxgitshell", "Rebase in progress");
    case RepositoryContextOperation::CherryPick:
        return i18nd("linuxgitshell", "Cherry-pick in progress");
    case RepositoryContextOperation::Revert:
        return i18nd("linuxgitshell", "Revert in progress");
    case RepositoryContextOperation::Bisect:
        return i18nd("linuxgitshell", "Bisect in progress");
    }
    return {};
}

} // namespace LinuxGitShell::Dolphin

#include "GitActionPlugin.moc"
