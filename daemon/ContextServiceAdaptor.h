// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <QDBusAbstractAdaptor>
#include <QStringList>
#include <QVariantMap>

namespace LinuxGitShell
{

class ContextService;

inline constexpr auto ContextServiceName = "org.linuxgitshell.Experimental.Context1";
inline constexpr auto ContextObjectPath = "/org/linuxgitshell/Context";

// Exposes ContextService as dbus/org.linuxgitshell.Experimental.Context1.xml.
class ContextServiceAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.linuxgitshell.Experimental.Context1")

  public:
    explicit ContextServiceAdaptor(ContextService* service);

  public Q_SLOTS:
    qulonglong RequestContext(const QStringList& paths);

  Q_SIGNALS:
    void ContextReady(const QString& path, const QVariantMap& context, qulonglong generation);

  private:
    ContextService* service;
};

} // namespace LinuxGitShell
