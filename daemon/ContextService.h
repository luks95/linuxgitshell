// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "linuxgitshell/gitcore/RepositoryDiscovery.h"
#include "linuxgitshell/repositorycontext/RepositoryContextCache.h"

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

namespace LinuxGitShell
{

struct ContextServiceLimits
{
    qsizetype maxPathsPerRequest = 256;
    qsizetype maxPathLength = 4096;
    qsizetype maxQueuedPaths = 1024;
    std::size_t concurrentDiscoveries = 2;
    std::chrono::milliseconds discoveryTimeout = std::chrono::seconds(10);
    RepositoryContextCacheLimits cache;
};

// Resolves repository context for absolute local paths outside Dolphin. Requests return at once;
// every accepted path is answered later through contextReady(), from the service cache when warm
// or after an asynchronous RepositoryDiscovery otherwise. Paths are never logged.
class ContextService final : public QObject
{
    Q_OBJECT

  public:
    explicit ContextService(QObject* parent = nullptr);
    explicit ContextService(const ContextServiceLimits& limits, QObject* parent = nullptr);
    ~ContextService() override;

    ContextService(const ContextService&) = delete;
    ContextService& operator=(const ContextService&) = delete;
    ContextService(ContextService&&) = delete;
    ContextService& operator=(ContextService&&) = delete;

    // Invalid, oversized, and excess paths are ignored; the returned generation identifies this
    // service instance so that clients can drop snapshots after a restart.
    quint64 requestContext(const QStringList& paths);
    [[nodiscard]] quint64 generation() const;

  Q_SIGNALS:
    void contextReady(const QString& pathKey,
                      const LinuxGitShell::RepositoryContextSnapshot& snapshot, quint64 generation);

  private:
    struct Worker
    {
        std::unique_ptr<RepositoryDiscovery> discovery;
        QTimer timeout;
        QElapsedTimer elapsed;
        QString pathKey;
        bool timedOut = false;
    };

    void startDiscoveries();
    void discoveryFinished(Worker& worker, const RepositoryDiscoveryResult& result);
    void publish(const QString& pathKey, const RepositoryContextSnapshot& snapshot);
    // Defers replies produced while handling a request so that they follow the method reply.
    void queueReply(const QString& pathKey, const RepositoryContextSnapshot& snapshot);
    void flushQueuedReplies();
    [[nodiscard]] static std::optional<RepositoryContextSnapshot>
    snapshotFor(const RepositoryDiscoveryResult& result, bool timedOut);

    ContextServiceLimits limits;
    RepositoryContextCache cache;
    quint64 currentGeneration = 0;
    std::vector<std::unique_ptr<Worker>> workers;
    QStringList queue;
    QSet<QString> pending;
    QList<std::pair<QString, RepositoryContextSnapshot>> queuedReplies;
    bool flushScheduled = false;
};

} // namespace LinuxGitShell
