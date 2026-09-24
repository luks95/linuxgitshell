// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "RepositoryContextTransport.h"

#include "linuxgitshell/repositorycontext/RepositoryContextCache.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include <chrono>
#include <memory>

namespace LinuxGitShell::Dolphin
{

struct RepositoryContextClientLimits
{
    qsizetype maxInFlight = 256;
    std::chrono::steady_clock::duration inFlightTimeout = std::chrono::seconds(30);
    RepositoryContextCacheLimits cache;
};

// Process-wide snapshot client for the Dolphin plugin. lookup() and refreshLater() are safe on the
// synchronous context-menu path: they only touch memory and never wait. Requests leave from the
// event loop after the caller returns, and replies fill the bounded cache for later menus.
class RepositoryContextClient final : public QObject
{
    Q_OBJECT

  public:
    RepositoryContextClient(std::unique_ptr<RepositoryContextTransport> transport,
                            const RepositoryContextClientLimits& limits, QObject* parent = nullptr);

    // The instance shared by every plugin object in this process, owned by the application.
    [[nodiscard]] static RepositoryContextClient& instance();

    [[nodiscard]] RepositoryContextLookup lookup(const QString& pathKey);
    void refreshLater(const QStringList& pathKeys);

    [[nodiscard]] quint64 generation() const;
    [[nodiscard]] qsizetype inFlightCount() const;

  Q_SIGNALS:
    void contextUpdated(const QString& pathKey);
    // The service could not be reached or refused the request; the keys stay cold.
    void contextUnavailable(const QStringList& pathKeys);

  private:
    using Clock = RepositoryContextCache::Clock;

    void sendQueued();
    void adoptGeneration(quint64 generation);
    void receiveContext(const QString& pathKey, const QVariantMap& context, quint64 generation);
    void forgetFailed(const QStringList& pathKeys);
    [[nodiscard]] bool isInFlight(const QString& pathKey, Clock::time_point now) const;

    std::unique_ptr<RepositoryContextTransport> transport;
    RepositoryContextClientLimits limits;
    RepositoryContextCache cache;
    QStringList queued;
    QHash<QString, Clock::time_point> inFlight;
    bool sendScheduled = false;
};

} // namespace LinuxGitShell::Dolphin
