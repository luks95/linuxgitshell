// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "linuxgitshell/repositorycontext/RepositoryContextWire.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QTest>

#include <csignal>
#include <sys/types.h>

// Helpers for tests that run on the private session bus created by dbus-run-session.
namespace DBusTestSupport
{

[[nodiscard]] inline bool isContextServiceRegistered()
{
    const QDBusConnectionInterface* bus = QDBusConnection::sessionBus().interface();
    return bus != nullptr &&
           bus->isServiceRegistered(LinuxGitShell::RepositoryContextServiceName).value();
}

// Terminates an activated context service, if any, and waits until its name is released.
[[nodiscard]] inline bool stopContextService()
{
    if (!isContextServiceRegistered())
    {
        return true;
    }
    const uint pid = QDBusConnection::sessionBus().interface()->servicePid(
        LinuxGitShell::RepositoryContextServiceName);
    if (pid == 0 || ::kill(static_cast<pid_t>(pid), SIGTERM) != 0)
    {
        return false;
    }
    return QTest::qWaitFor([] { return !isContextServiceRegistered(); }, 5000);
}

} // namespace DBusTestSupport
