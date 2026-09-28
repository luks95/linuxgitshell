// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "ContextService.h"
#include "ContextServiceAdaptor.h"
#include "DaemonLogging.h"

#include "linuxgitshell/gitcore/ProjectInfo.h"
#include "linuxgitshell/repositorycontext/RepositoryContextWire.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("linuxgitshell-daemon"));
    QCoreApplication::setApplicationVersion(LinuxGitShell::ProjectInfo::version());

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("LinuxGitShell session service (experimental repository context)"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(application);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
    {
        qCCritical(LinuxGitShell::daemonLog) << "Session bus unavailable";
        return 1;
    }

    LinuxGitShell::ContextService service;
    new LinuxGitShell::ContextServiceAdaptor(&service);

    // Register the object before the name so that activated clients never see a missing object.
    if (!bus.registerObject(LinuxGitShell::RepositoryContextObjectPath, &service))
    {
        qCCritical(LinuxGitShell::daemonLog) << "Could not register the context object";
        return 1;
    }
    if (!bus.registerService(LinuxGitShell::RepositoryContextServiceName))
    {
        qCWarning(LinuxGitShell::daemonLog) << "Context service is already running";
        return 1;
    }

    // A session service must not outlive its bus, for example after logout or dbus-run-session.
    bus.connect(QString(), QStringLiteral("/org/freedesktop/DBus/Local"),
                QStringLiteral("org.freedesktop.DBus.Local"), QStringLiteral("Disconnected"),
                &application, SLOT(quit()));

    qCDebug(LinuxGitShell::daemonLog) << "Context service started";
    return QCoreApplication::exec();
}
