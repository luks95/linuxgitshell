// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "OverlayTransport.h"

#include "linuxgitshell/repositorycontext/RepositoryStatusIndex.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <chrono>
#include <functional>
#include <list>
#include <memory>

namespace LinuxGitShell::Dolphin
{

struct OverlayClientLimits
{
    using Clock = std::chrono::steady_clock;

    qsizetype maxKeys = 4096;
    qsizetype maxBatch = 256;
    qsizetype maxInFlightRequests = 4;
    std::chrono::milliseconds renewInterval = std::chrono::seconds(30);
    std::chrono::milliseconds requestTimeout = std::chrono::seconds(5);
    std::chrono::milliseconds minBackoff = std::chrono::seconds(1);
    std::chrono::milliseconds maxBackoff = std::chrono::seconds(30);
    // Replaceable monotonic time source for deterministic tests.
    std::function<Clock::time_point()> clock;
};

// Process-wide status cache behind the overlay plugin. lookup() is a paint callback: it touches
// memory only, never waits, and never emits. Requests leave from the event loop, answers fill a
// bounded cache, and iconsChanged() reports every icon that must be repainted or cleared.
class OverlayClient final : public QObject
{
    Q_OBJECT

  public:
    using Clock = OverlayClientLimits::Clock;

    OverlayClient(std::unique_ptr<OverlayTransport> transport, const OverlayClientLimits& limits,
                  QObject* parent = nullptr);
    ~OverlayClient() override;

    // The instance shared by every plugin object in this process, owned by the application.
    [[nodiscard]] static OverlayClient& instance();

    // Local `file:` URLs only; anything else has no key and therefore no overlay.
    [[nodiscard]] static QString pathKeyForUrl(const QUrl& url);
    // Empty for states that have no icon (Unknown, Ignored, Deleted).
    [[nodiscard]] static QString iconNameFor(RepositoryStatusState state);

    // Icon names for a path key. Returns nothing when there is no fresh Ready evidence, and then
    // queues a request for later.
    [[nodiscard]] QStringList lookup(const QString& pathKey);

    [[nodiscard]] bool isEnabled() const;
    // Disabling clears every icon, releases all observations, and stops all work.
    void setEnabled(bool enabled);

    // Diagnostics and deterministic tests.
    void expireNow();
    void renewNow();
    [[nodiscard]] qsizetype keyCount() const;
    [[nodiscard]] qsizetype inFlightCount() const;
    [[nodiscard]] quint64 generation() const;

  Q_SIGNALS:
    void iconsChanged(const QString& pathKey, const QStringList& icons);

  private:
    struct Entry
    {
        RepositoryStatusState state = RepositoryStatusState::Unknown;
        // Icon implied by the current evidence, and the icon last reported to the host.
        QString icon;
        QString shown;
        Clock::time_point expires;
        quint64 revision = 0;
        bool hasEvidence = false;
        bool wanted = false;
        bool inFlight = false;
        std::list<QString>::iterator lruPosition;
    };

    struct EarlyInitial
    {
        QList<QVariantMap> records;
        quint64 generation = 0;
    };

    struct Request
    {
        QStringList keys;
        QSet<QString> awaiting;
        Clock::time_point sentAt;
        Clock::time_point deadline;
        bool accepted = false;
        // Initial records may overtake the acceptance of their own request when several are in
        // flight; they are kept (bounded) and applied once the request is accepted.
        QList<EarlyInitial> early;
        qsizetype earlyRecords = 0;
    };

    [[nodiscard]] Clock::time_point now() const;
    [[nodiscard]] Entry& ensureEntry(const QString& pathKey);
    void touch(Entry& entry);
    void evictOldest();
    void markWanted(const QString& pathKey, Entry& entry);
    void clearEvidence(const QString& pathKey, Entry& entry);
    void queueNotification(const QString& pathKey);
    void flushNotifications();
    void scheduleSend();
    void sendWanted();
    void sendReleases();
    void armTimers();
    void armExpiry(Clock::time_point when);
    void applyRecord(const QVariantMap& map, Clock::time_point base);
    void acceptRequest(quint64 requestId, quint64 generation, const QStringList& acceptedPaths);
    void receiveInitial(quint64 requestId, const QList<QVariantMap>& records, quint64 generation);
    void applyInitial(quint64 requestId, const QList<QVariantMap>& records, quint64 generation);
    void receiveChanges(const QList<QVariantMap>& records, quint64 generation);
    void completeRequest(quint64 requestId);
    void failRequest(quint64 requestId);
    void retryAfterFailure();
    void resetEvidence();
    void adoptGeneration(quint64 generation, quint64 keepRequestId);
    void checkTimeouts();

    std::unique_ptr<OverlayTransport> transport;
    OverlayClientLimits limits;
    bool enabled = true;
    quint64 currentGeneration = 0;
    quint64 lastRequestId = 0;
    int failures = 0;
    Clock::time_point backoffUntil;
    QHash<QString, Entry> entries;
    std::list<QString> lru;
    QStringList wantedKeys;
    QHash<quint64, Request> inFlight;
    QStringList releaseQueue;
    QSet<QString> notifyKeys;
    QSet<QString> clearedKeys;
    bool sendScheduled = false;
    bool notifyScheduled = false;
    QTimer renewTimer;
    QTimer expiryTimer;
    QTimer timeoutTimer;
    QTimer retryTimer;
    Clock::time_point armedExpiry = Clock::time_point::max();
};

} // namespace LinuxGitShell::Dolphin
