// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace LinuxGitShell::Dolphin
{

// Asynchronous channel to the repository context service. requestContext() must return without
// waiting for the service; outcomes arrive later through the signals.
class RepositoryContextTransport : public QObject
{
    Q_OBJECT

  public:
    using QObject::QObject;

    virtual void requestContext(const QStringList& pathKeys) = 0;

  Q_SIGNALS:
    void requestAccepted(quint64 generation);
    void requestFailed(const QStringList& pathKeys);
    void contextReceived(const QString& pathKey, const QVariantMap& context, quint64 generation);
};

// Talks to org.linuxgitshell.Experimental.Context1 on the session bus. It never uses generated
// proxies, which resolve the name owner synchronously, and it subscribes to replies without a
// sender filter so that subscribing needs no name lookup either.
class DBusRepositoryContextTransport final : public RepositoryContextTransport
{
    Q_OBJECT

  public:
    explicit DBusRepositoryContextTransport(QObject* parent = nullptr);
    DBusRepositoryContextTransport(const QString& serviceName, QObject* parent);

    void requestContext(const QStringList& pathKeys) override;

  private Q_SLOTS:
    void onContextReady(const QString& pathKey, const QVariantMap& context, qulonglong generation);

  private:
    QString serviceName;
    bool subscribed = false;
};

} // namespace LinuxGitShell::Dolphin
