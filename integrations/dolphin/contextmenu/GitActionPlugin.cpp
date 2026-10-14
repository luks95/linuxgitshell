// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "GitActionPlugin.h"

#include "ContextMenuPolicy.h"
#include "RepositoryContextClient.h"

#include <KFileItemListProperties>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>
#include <QElapsedTimer>
#include <QIcon>
#include <QLoggingCategory>
#include <QMenu>
#include <QProcess>
#include <QScopeGuard>
#include <QStringList>
#include <QWidget>

namespace LinuxGitShell::Dolphin
{

// Choose the plugin's language per string without changing Dolphin's process-wide locale.
const QStringList englishLanguages{QStringLiteral("en")};

Q_LOGGING_CATEGORY(dolphinContextMenuLog, "linuxgitshell.dolphin.contextmenu")
Q_LOGGING_CATEGORY(dolphinContextMenuTimingLog, "linuxgitshell.dolphin.timing", QtInfoMsg)

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
    QElapsedTimer timer;
    const bool measure = dolphinContextMenuTimingLog().isDebugEnabled();
    if (measure)
    {
        timer.start();
    }
    ContextMenuKind measuredKind = ContextMenuKind::None;
    const auto reportTiming = qScopeGuard(
        [&timer, measure, &measuredKind, &fileItemInfos]()
        {
            if (measure)
            {
                const qint64 elapsed = timer.nsecsElapsed();
                qCDebug(dolphinContextMenuTimingLog).nospace()
                    << "actions() kind=" << static_cast<int>(measuredKind)
                    << " selected=" << fileItemInfos.urlList().size() << " elapsed_ns=" << elapsed;
            }
        });

    // Memory-only lookups: cold or stale entries are refreshed from the event loop after this
    // menu is built, so a later menu can use them. Git, discovery, and IPC never run here.
    RepositoryContextClient& contextClient = RepositoryContextClient::instance();
    const ContextMenuDecision decision =
        ContextMenuPolicy::decide(fileItemInfos.urlList(), [&contextClient](const QString& pathKey)
                                  { return contextClient.lookup(pathKey); });
    measuredKind = decision.kind;
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
        auto* open = new QAction(
            ki18nd("linuxgitshell", "Open with LinuxGitShell").toString(englishLanguages),
            parentWidget);
        launchOnTrigger(open, decision.launchPath);
        return {open};
    }
    case ContextMenuKind::Repository:
        break;
    }

    auto* menu = new QMenu(ki18nd("linuxgitshell", "LinuxGitShell").toString(englishLanguages),
                           parentWidget);
    menu->setIcon(QIcon::fromTheme(QStringLiteral("git")));
    auto* showStatus =
        new QAction(ki18nd("linuxgitshell", "Show Status").toString(englishLanguages), menu);
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
                    Q_EMIT error(ki18nd("linuxgitshell", "Could not start LinuxGitShell.")
                                     .toString(englishLanguages));
                }
            });
}

QString GitActionPlugin::operationText(RepositoryContextOperation operation)
{
    switch (operation)
    {
    case RepositoryContextOperation::Merge:
        return ki18nd("linuxgitshell", "Merge in progress").toString(englishLanguages);
    case RepositoryContextOperation::Rebase:
        return ki18nd("linuxgitshell", "Rebase in progress").toString(englishLanguages);
    case RepositoryContextOperation::CherryPick:
        return ki18nd("linuxgitshell", "Cherry-pick in progress").toString(englishLanguages);
    case RepositoryContextOperation::Revert:
        return ki18nd("linuxgitshell", "Revert in progress").toString(englishLanguages);
    case RepositoryContextOperation::Bisect:
        return ki18nd("linuxgitshell", "Bisect in progress").toString(englishLanguages);
    }
    return {};
}

} // namespace LinuxGitShell::Dolphin

#include "GitActionPlugin.moc"
