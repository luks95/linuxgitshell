// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "OverlayClient.h"

#include "linuxgitshell/repositorycontext/OverlayWire.h"
#include "linuxgitshell/repositorycontext/RepositoryContextSnapshot.h"

#include <QCoreApplication>
#include <QDir>
#include <QLoggingCategory>
#include <QPointer>

#include <algorithm>
#include <utility>

namespace LinuxGitShell::Dolphin
{

Q_LOGGING_CATEGORY(dolphinOverlayLog, "linuxgitshell.dolphin.overlays")

namespace
{

using std::chrono::milliseconds;
constexpr qsizetype MaxPathLength = 4096;

[[nodiscard]] milliseconds untilTimer(OverlayClient::Clock::time_point when,
                                      OverlayClient::Clock::time_point now)
{
    // At least one millisecond: a zero interval would spin the event loop.
    return std::max(milliseconds(1), std::chrono::duration_cast<milliseconds>(when - now));
}

} // namespace

OverlayClient::OverlayClient(std::unique_ptr<OverlayTransport> transport,
                             const OverlayClientLimits& limits, QObject* parent)
    : QObject(parent), transport(std::move(transport)), limits(limits)
{
    renewTimer.setInterval(limits.renewInterval);
    expiryTimer.setSingleShot(true);
    timeoutTimer.setSingleShot(true);
    retryTimer.setSingleShot(true);
    connect(&renewTimer, &QTimer::timeout, this, &OverlayClient::renewNow);
    connect(&expiryTimer, &QTimer::timeout, this, &OverlayClient::expireNow);
    connect(&timeoutTimer, &QTimer::timeout, this, &OverlayClient::checkTimeouts);
    connect(&retryTimer, &QTimer::timeout, this, &OverlayClient::scheduleSend);

    connect(this->transport.get(), &OverlayTransport::requestAccepted, this,
            &OverlayClient::acceptRequest);
    connect(this->transport.get(), &OverlayTransport::requestFailed, this,
            &OverlayClient::failRequest);
    connect(this->transport.get(), &OverlayTransport::initialResults, this,
            &OverlayClient::receiveInitial);
    connect(this->transport.get(), &OverlayTransport::recordsChanged, this,
            &OverlayClient::receiveChanges);
    connect(this->transport.get(), &OverlayTransport::ownerLost, this,
            [this]
            {
                // Everything learned from the previous service is void; ask the new one again,
                // after a backoff so that an exiting session is not immediately re-activated.
                currentGeneration = 0;
                for (const auto& request : std::as_const(inFlight))
                {
                    for (const QString& key : request.keys)
                    {
                        if (const auto it = entries.find(key); it != entries.end())
                        {
                            it->inFlight = false;
                        }
                    }
                }
                inFlight.clear();
                resetEvidence();
                for (auto it = entries.begin(); it != entries.end(); ++it)
                {
                    markWanted(it.key(), it.value());
                }
                ++failures;
                retryAfterFailure();
            });
}

OverlayClient::~OverlayClient() = default;

OverlayClient& OverlayClient::instance()
{
    // Owned by the application rather than a plugin object, because Dolphin may create several
    // plugin objects. Constructing it performs no D-Bus work.
    static QPointer<OverlayClient> client;
    if (client.isNull())
    {
        client = new OverlayClient(std::make_unique<DBusOverlayTransport>(), OverlayClientLimits{},
                                   QCoreApplication::instance());
    }
    return *client;
}

QString OverlayClient::pathKeyForUrl(const QUrl& url)
{
    if (!url.isLocalFile())
    {
        return {};
    }
    const QString path = url.toLocalFile();
    return path.size() > MaxPathLength ? QString() : repositoryContextPathKey(path);
}

QString OverlayClient::iconNameFor(RepositoryStatusState state)
{
    switch (state)
    {
    case RepositoryStatusState::Clean:
        return QStringLiteral("emblem-linuxgitshell-clean");
    case RepositoryStatusState::Modified:
        return QStringLiteral("emblem-linuxgitshell-modified");
    case RepositoryStatusState::Added:
        return QStringLiteral("emblem-linuxgitshell-added");
    case RepositoryStatusState::Conflict:
        return QStringLiteral("emblem-linuxgitshell-conflict");
    case RepositoryStatusState::Untracked:
        return QStringLiteral("emblem-linuxgitshell-untracked");
    case RepositoryStatusState::Deleted:
    case RepositoryStatusState::Ignored:
        // Modeled from the start; their glyphs ship later, and they never fall back to Clean.
    case RepositoryStatusState::Unknown:
        break;
    }
    return {};
}

OverlayClient::Clock::time_point OverlayClient::now() const
{
    return limits.clock ? limits.clock() : Clock::now();
}

bool OverlayClient::isEnabled() const { return enabled; }

qsizetype OverlayClient::keyCount() const { return entries.size(); }

qsizetype OverlayClient::inFlightCount() const { return inFlight.size(); }

quint64 OverlayClient::generation() const { return currentGeneration; }

QStringList OverlayClient::lookup(const QString& pathKey)
{
    if (!enabled || pathKey.isEmpty())
    {
        return {};
    }

    const auto it = entries.find(pathKey);
    if (it == entries.end())
    {
        Entry& entry = ensureEntry(pathKey);
        markWanted(pathKey, entry);
        return {};
    }

    Entry& entry = it.value();
    touch(entry);
    if (entry.hasEvidence && now() >= entry.expires)
    {
        // Lookup also expires, so an idle view cannot keep an icon past its freshness. The
        // repaint happens through this very call; ask the service for a fresh answer.
        clearEvidence(pathKey, entry);
        markWanted(pathKey, entry);
    }
    if (!entry.hasEvidence || entry.icon.isEmpty())
    {
        entry.shown.clear();
        return {};
    }
    entry.shown = entry.icon;
    return {entry.icon};
}

OverlayClient::Entry& OverlayClient::ensureEntry(const QString& pathKey)
{
    lru.push_front(pathKey);
    Entry entry;
    entry.lruPosition = lru.begin();
    entries.insert(pathKey, entry);
    if (entries.size() > limits.maxKeys)
    {
        evictOldest();
    }
    if (!renewTimer.isActive() && enabled)
    {
        renewTimer.start();
    }
    // Erasing the evicted entry may relocate others, so the reference is taken last.
    return entries.find(pathKey).value();
}

void OverlayClient::touch(Entry& entry) { lru.splice(lru.begin(), lru, entry.lruPosition); }

void OverlayClient::evictOldest()
{
    if (lru.empty())
    {
        return;
    }
    const QString key = lru.back();
    const auto it = entries.find(key);
    if (it != entries.end())
    {
        // Eviction also clears whatever the host still displays for this item.
        if (!it->shown.isEmpty())
        {
            clearedKeys.insert(key);
            queueNotification(key);
        }
        entries.erase(it);
    }
    lru.pop_back();
    releaseQueue.append(key);
    scheduleSend();
}

void OverlayClient::markWanted(const QString& pathKey, Entry& entry)
{
    if (entry.wanted || entry.inFlight)
    {
        return;
    }
    entry.wanted = true;
    wantedKeys.append(pathKey);
    scheduleSend();
}

void OverlayClient::clearEvidence(const QString& pathKey, Entry& entry)
{
    entry.hasEvidence = false;
    entry.icon.clear();
    entry.state = RepositoryStatusState::Unknown;
    entry.expires = Clock::time_point();
    if (!entry.shown.isEmpty())
    {
        queueNotification(pathKey);
    }
}

void OverlayClient::queueNotification(const QString& pathKey)
{
    notifyKeys.insert(pathKey);
    if (!notifyScheduled)
    {
        notifyScheduled = true;
        QTimer::singleShot(0, this, &OverlayClient::flushNotifications);
    }
}

void OverlayClient::flushNotifications()
{
    notifyScheduled = false;
    const QSet<QString> cleared = std::exchange(clearedKeys, {});
    const QSet<QString> changed = std::exchange(notifyKeys, {});
    for (const QString& key : cleared)
    {
        Q_EMIT iconsChanged(key, {});
    }
    for (const QString& key : changed)
    {
        const auto it = entries.find(key);
        if (it == entries.end() || it->icon == it->shown)
        {
            continue;
        }
        it->shown = it->icon;
        Q_EMIT iconsChanged(key, it->icon.isEmpty() ? QStringList() : QStringList{it->icon});
    }
}

void OverlayClient::scheduleSend()
{
    if (sendScheduled || !enabled)
    {
        return;
    }
    sendScheduled = true;
    // Requests leave after the paint callback returned, never from inside it.
    QTimer::singleShot(0, this, &OverlayClient::sendWanted);
}

void OverlayClient::sendWanted()
{
    sendScheduled = false;
    if (!enabled)
    {
        return;
    }
    sendReleases();

    const Clock::time_point current = now();
    if (current < backoffUntil)
    {
        if (!retryTimer.isActive())
        {
            retryTimer.start(untilTimer(backoffUntil, current));
        }
        return;
    }

    while (inFlight.size() < limits.maxInFlightRequests && !wantedKeys.isEmpty())
    {
        QStringList batch;
        while (!wantedKeys.isEmpty() && batch.size() < limits.maxBatch)
        {
            const QString key = wantedKeys.takeFirst();
            const auto it = entries.find(key);
            if (it == entries.end() || !it->wanted)
            {
                continue;
            }
            it->wanted = false;
            it->inFlight = true;
            batch.append(key);
        }
        if (batch.isEmpty())
        {
            break;
        }
        Request request;
        request.keys = batch;
        request.sentAt = current;
        request.deadline = current + limits.requestTimeout;
        const quint64 id = ++lastRequestId;
        inFlight.insert(id, request);
        transport->request(id, batch);
    }
    armTimers();
}

void OverlayClient::sendReleases()
{
    while (!releaseQueue.isEmpty())
    {
        const qsizetype count = std::min(releaseQueue.size(), limits.maxBatch);
        const QStringList batch = releaseQueue.mid(0, count);
        releaseQueue.remove(0, count);
        transport->release(batch);
    }
}

void OverlayClient::armTimers()
{
    if (inFlight.isEmpty())
    {
        timeoutTimer.stop();
        return;
    }
    Clock::time_point earliest = Clock::time_point::max();
    for (const Request& request : std::as_const(inFlight))
    {
        earliest = std::min(earliest, request.deadline);
    }
    timeoutTimer.start(untilTimer(earliest, now()));
}

void OverlayClient::checkTimeouts()
{
    const Clock::time_point current = now();
    QList<quint64> expired;
    for (auto it = inFlight.cbegin(); it != inFlight.cend(); ++it)
    {
        if (it->deadline <= current)
        {
            expired.append(it.key());
        }
    }
    for (const quint64 id : std::as_const(expired))
    {
        failRequest(id);
    }
    armTimers();
}

void OverlayClient::armExpiry(Clock::time_point when)
{
    if (when >= armedExpiry)
    {
        return;
    }
    armedExpiry = when;
    expiryTimer.start(untilTimer(when, now()));
}

void OverlayClient::expireNow()
{
    const Clock::time_point current = now();
    armedExpiry = Clock::time_point::max();
    Clock::time_point next = Clock::time_point::max();
    for (auto it = entries.begin(); it != entries.end(); ++it)
    {
        Entry& entry = it.value();
        if (!entry.hasEvidence)
        {
            continue;
        }
        if (entry.expires <= current)
        {
            // Without fresh evidence the icon goes away; a renewed answer is requested.
            clearEvidence(it.key(), entry);
            markWanted(it.key(), entry);
        }
        else
        {
            next = std::min(next, entry.expires);
        }
    }
    if (next != Clock::time_point::max())
    {
        armExpiry(next);
    }
}

void OverlayClient::renewNow()
{
    if (!enabled)
    {
        return;
    }
    if (entries.isEmpty())
    {
        renewTimer.stop();
        return;
    }
    // Retained observations are renewed oldest-use last so that busy items refresh first.
    for (const QString& key : lru)
    {
        if (const auto it = entries.find(key); it != entries.end())
        {
            markWanted(key, it.value());
        }
    }
}

void OverlayClient::applyRecord(const QVariantMap& map, Clock::time_point base)
{
    const std::optional<OverlayRecord> record = overlayRecordFromVariantMap(map);
    if (!record.has_value())
    {
        // Unknown or malformed values never preserve a previous assertion.
        const QVariant path = map.value(QStringLiteral("path"));
        if (path.typeId() == QMetaType::QString)
        {
            if (const auto it = entries.find(path.toString()); it != entries.end())
            {
                clearEvidence(it.key(), it.value());
            }
        }
        return;
    }

    const auto it = entries.find(record->path);
    if (it == entries.end())
    {
        return;
    }
    Entry& entry = it.value();
    if (record->revision < entry.revision)
    {
        return; // a late reply must not overwrite newer knowledge
    }
    entry.revision = record->revision;

    if (record->validity != RepositoryStatusValidity::Ready || record->validForMs == 0)
    {
        clearEvidence(it.key(), entry);
        return;
    }
    entry.state = record->state;
    entry.icon = iconNameFor(record->state);
    entry.hasEvidence = true;
    // Age by the time already spent: initial answers from when they were requested, updates from
    // receipt. Transport delay cannot be subtracted; freshness is best effort.
    entry.expires = base + milliseconds(record->validForMs);
    armExpiry(entry.expires);
    if (entry.icon != entry.shown)
    {
        queueNotification(it.key());
    }
}

void OverlayClient::acceptRequest(quint64 requestId, quint64 generation,
                                  const QStringList& acceptedPaths)
{
    if (!inFlight.contains(requestId))
    {
        return;
    }
    if (generation == 0)
    {
        failRequest(requestId);
        return;
    }
    adoptGeneration(generation, requestId);
    const auto it = inFlight.find(requestId);
    if (it == inFlight.end())
    {
        return;
    }

    Request& request = it.value();
    request.accepted = true;
    const QSet<QString> accepted(acceptedPaths.cbegin(), acceptedPaths.cend());
    bool rejected = false;
    for (const QString& key : std::as_const(request.keys))
    {
        const auto entry = entries.find(key);
        if (accepted.contains(key))
        {
            request.awaiting.insert(key);
        }
        else if (entry != entries.end())
        {
            // Over capacity or malformed on the service side: retry with backoff.
            entry->inFlight = false;
            markWanted(key, entry.value());
            rejected = true;
        }
    }
    if (rejected)
    {
        ++failures;
        retryAfterFailure();
    }
    if (request.awaiting.isEmpty())
    {
        completeRequest(requestId);
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void OverlayClient::adoptGeneration(quint64 generation, quint64 keepRequestId)
{
    if (currentGeneration == generation)
    {
        return;
    }
    const bool restart = currentGeneration != 0;
    currentGeneration = generation;
    if (!restart)
    {
        return;
    }

    // A new service instance numbers its revisions afresh and knows none of our observations.
    for (auto it = inFlight.begin(); it != inFlight.end();)
    {
        if (it.key() == keepRequestId)
        {
            ++it;
            continue;
        }
        for (const QString& key : std::as_const(it->keys))
        {
            if (const auto entry = entries.find(key); entry != entries.end())
            {
                entry->inFlight = false;
            }
        }
        it = inFlight.erase(it);
    }
    resetEvidence();
    for (auto it = entries.begin(); it != entries.end(); ++it)
    {
        markWanted(it.key(), it.value());
    }
}

void OverlayClient::resetEvidence()
{
    for (auto it = entries.begin(); it != entries.end(); ++it)
    {
        it->revision = 0;
        clearEvidence(it.key(), it.value());
    }
}

void OverlayClient::receiveInitial(quint64 requestId, const QList<QVariantMap>& records,
                                   quint64 generation)
{
    const auto it = inFlight.find(requestId);
    if (it == inFlight.end() || generation != currentGeneration || !it->accepted)
    {
        return;
    }
    const Clock::time_point sentAt = it->sentAt;
    QStringList seen;
    for (const QVariantMap& map : records)
    {
        applyRecord(map, sentAt);
        const QVariant path = map.value(QStringLiteral("path"));
        if (path.typeId() == QMetaType::QString)
        {
            seen.append(path.toString());
        }
    }
    // applyRecord() cannot invalidate this request, but re-find to stay safe against reentrancy.
    const auto current = inFlight.find(requestId);
    if (current == inFlight.end())
    {
        return;
    }
    for (const QString& path : std::as_const(seen))
    {
        current->awaiting.remove(path);
    }
    if (current->awaiting.isEmpty())
    {
        completeRequest(requestId);
    }
}

void OverlayClient::receiveChanges(const QList<QVariantMap>& records, quint64 generation)
{
    if (generation != currentGeneration || generation == 0)
    {
        return;
    }
    const Clock::time_point current = now();
    for (const QVariantMap& map : records)
    {
        applyRecord(map, current);
    }
}

void OverlayClient::completeRequest(quint64 requestId)
{
    const auto it = inFlight.find(requestId);
    if (it == inFlight.end())
    {
        return;
    }
    for (const QString& key : std::as_const(it->keys))
    {
        if (const auto entry = entries.find(key); entry != entries.end())
        {
            entry->inFlight = false;
        }
    }
    inFlight.erase(it);
    failures = 0;
    backoffUntil = Clock::time_point();
    armTimers();
    if (!wantedKeys.isEmpty())
    {
        scheduleSend();
    }
}

void OverlayClient::failRequest(quint64 requestId)
{
    const auto it = inFlight.find(requestId);
    if (it == inFlight.end())
    {
        return;
    }
    const QStringList keys = it->keys;
    inFlight.erase(it);
    for (const QString& key : keys)
    {
        const auto entry = entries.find(key);
        if (entry == entries.end())
        {
            continue;
        }
        // On transport failure or timeout the stale presentation goes away, then retry.
        entry->inFlight = false;
        clearEvidence(key, entry.value());
        markWanted(key, entry.value());
    }
    ++failures;
    retryAfterFailure();
    armTimers();
}

void OverlayClient::retryAfterFailure()
{
    milliseconds delay = limits.minBackoff;
    for (int step = 1; step < std::min(failures, 16) && delay < limits.maxBackoff; ++step)
    {
        delay *= 2;
    }
    delay = std::min(delay, limits.maxBackoff);
    backoffUntil = now() + delay;
    retryTimer.start(delay);
    qCDebug(dolphinOverlayLog) << "Overlay request failed; retrying in" << delay.count() << "ms";
}

void OverlayClient::setEnabled(bool on)
{
    if (enabled == on)
    {
        return;
    }
    enabled = on;
    if (on)
    {
        return; // resumes lazily through the next lookup
    }

    renewTimer.stop();
    expiryTimer.stop();
    timeoutTimer.stop();
    retryTimer.stop();
    armedExpiry = Clock::time_point::max();

    // Clear what the host still shows, then release everything and drop the subscription.
    QStringList keys;
    keys.reserve(entries.size());
    QStringList shown;
    for (auto it = entries.cbegin(); it != entries.cend(); ++it)
    {
        keys.append(it.key());
        if (!it->shown.isEmpty())
        {
            shown.append(it.key());
        }
    }
    entries.clear();
    lru.clear();
    wantedKeys.clear();
    inFlight.clear();
    notifyKeys.clear();
    clearedKeys.clear();
    releaseQueue.clear();
    currentGeneration = 0;
    failures = 0;
    backoffUntil = Clock::time_point();

    for (const QString& key : std::as_const(shown))
    {
        Q_EMIT iconsChanged(key, {});
    }
    for (qsizetype start = 0; start < keys.size(); start += limits.maxBatch)
    {
        transport->release(keys.mid(start, limits.maxBatch));
    }
    transport->stop();
}

} // namespace LinuxGitShell::Dolphin
