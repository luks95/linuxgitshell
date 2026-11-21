// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QList>
#include <QStringList>
#include <QVariantMap>

#include "OverlayStatusProvider.h"

namespace LinuxGitShell
{

// The object registered on the bus. QtDBus establishes the call context (sender, error replies)
// on the registered object, never on its adaptors, so the adaptor reads it from here.
class OverlayServiceObject final : public QObject, public QDBusContext
{
    Q_OBJECT

  public:
    explicit OverlayServiceObject(OverlayStatusProvider& provider, QObject* parent = nullptr);

    [[nodiscard]] OverlayStatusProvider& provider() const;

  private:
    OverlayStatusProvider& statusProvider;
};

// Exposes OverlayStatusProvider as dbus/org.linuxgitshell.Experimental.Overlays1.xml. The interface
// name in Q_CLASSINFO must match OverlayInterfaceName. Records are sent as unicast signals to the
// observing caller; the declared signals exist only for introspection and are never emitted.
class OverlayServiceAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.linuxgitshell.Experimental.Overlays1")

  public:
    explicit OverlayServiceAdaptor(OverlayServiceObject* object);

  public Q_SLOTS:
    qulonglong RequestOverlays(const QStringList& paths, qulonglong requestId,
                               QStringList& acceptedPaths);
    void ReleaseOverlays(const QStringList& paths);
    bool RefreshRepository(const QString& root);

  Q_SIGNALS:
    void OverlayResults(qulonglong requestId, const QList<QVariantMap>& records,
                        qulonglong generation);
    void OverlaysChanged(const QList<QVariantMap>& records, qulonglong generation);

  private:
    static constexpr qsizetype MaxMessageBytes = 900 * 1024;

    void sendTo(const QString& owner, const char* signalName, const QVariantList& arguments) const;
    void sendRecords(const QString& owner, const char* signalName, quint64 requestId,
                     bool withRequestId, const QList<OverlayRecord>& records,
                     quint64 generation) const;

    OverlayServiceObject* object;
    OverlayStatusProvider* provider;
    QDBusServiceWatcher watcher;
};

} // namespace LinuxGitShell
