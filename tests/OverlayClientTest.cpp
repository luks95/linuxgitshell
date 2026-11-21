// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "OverlayClient.h"

#include "linuxgitshell/repositorycontext/OverlayWire.h"

#include <QSignalSpy>
#include <QTest>

using namespace LinuxGitShell;
using namespace LinuxGitShell::Dolphin;
using namespace std::chrono_literals;
using State = RepositoryStatusState;
using Validity = RepositoryStatusValidity;

namespace
{

constexpr int WaitMilliseconds = 5000;
constexpr quint64 Generation = 5;

class FakeTransport final : public OverlayTransport
{
    Q_OBJECT

  public:
    void request(quint64 requestId, const QStringList& paths) override
    {
        requests.append({requestId, paths});
    }
    void release(const QStringList& paths) override { releases.append(paths); }
    void stop() override { stopped = true; }

    [[nodiscard]] quint64 lastId() const { return requests.constLast().first; }

    QList<std::pair<quint64, QStringList>> requests;
    QStringList releases;
    bool stopped = false;
};

[[nodiscard]] QVariantMap record(const QString& path, State state, quint64 revision,
                                 Validity validity = Validity::Ready, quint32 validForMs = 10000)
{
    OverlayRecord value;
    value.path = path;
    value.repositoryRoot = QStringLiteral("/repo");
    value.state = state;
    value.validity = validity;
    value.revision = revision;
    value.validForMs = validity == Validity::Ready ? validForMs : 0;
    return overlayRecordToVariantMap(value);
}

struct Harness
{
    // Backoff and timeouts use real timers, so tests of those pass fakeClock = false.
    explicit Harness(OverlayClientLimits limits = {}, bool fakeClock = true)
    {
        if (fakeClock)
        {
            limits.clock = [this] { return clock; };
        }
        auto owned = std::make_unique<FakeTransport>();
        transport = owned.get();
        client = std::make_unique<OverlayClient>(std::move(owned), limits);
        changes = std::make_unique<QSignalSpy>(client.get(), &OverlayClient::iconsChanged);
    }

    // Sends the queued request for `paths` and answers it, like a service on a warm cache.
    void answer(const QStringList& paths, State state, quint64 revision,
                quint64 generation = Generation)
    {
        QTRY_VERIFY_WITH_TIMEOUT(!transport->requests.isEmpty(), WaitMilliseconds);
        const quint64 id = transport->lastId();
        Q_EMIT transport->requestAccepted(id, generation, paths);
        QList<QVariantMap> records;
        for (const QString& path : paths)
        {
            records.append(record(path, state, revision));
        }
        Q_EMIT transport->initialResults(id, records, generation);
    }

    OverlayClient::Clock::time_point clock = OverlayClient::Clock::now();
    FakeTransport* transport = nullptr;
    std::unique_ptr<OverlayClient> client;
    std::unique_ptr<QSignalSpy> changes;
};

[[nodiscard]] QStringList icon(const QString& name)
{
    return {QStringLiteral("emblem-linuxgitshell-") + name};
}

} // namespace

class OverlayClientTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void lookupQueuesOneDeferredRequestAndNeverBlocks();
    void readyRecordShowsIconAndNotifiesOnce();
    void everyStateMapsToAnIconOrNone_data();
    void everyStateMapsToAnIconOrNone();
    void nonReadyAndMalformedRecordsClearIcons();
    void ignoresLowerRevisionsAndOtherGenerations();
    void generationChangeResetsRevisionsAndKeepsKeys();
    void initialRecordsMayOvertakeTheirOwnAcceptance();
    void initialFreshnessIsAgedFromRequestTime();
    void expiryClearsIconsAndRequestsRenewal();
    void failedRequestsClearIconsAndRetryWithBackoff();
    void rejectedPathsAreRetried();
    void requestTimeoutFailsTheRequest();
    void evictionClearsTheIconAndReleasesTheKey();
    void renewalRequestsRetainedKeysInBatches();
    void disablingClearsReleasesAndStops();
    void ownerLossClearsAndRequestsAgain();
    void pathKeysExistOnlyForLocalUrls();
};

void OverlayClientTest::lookupQueuesOneDeferredRequestAndNeverBlocks()
{
    Harness harness;
    QCOMPARE(harness.client->lookup(QStringLiteral("/a")), QStringList());
    QCOMPARE(harness.client->lookup(QStringLiteral("/a")), QStringList());
    QCOMPARE(harness.client->lookup(QStringLiteral("/b")), QStringList());
    QCOMPARE(harness.client->lookup(QString()), QStringList());

    // Nothing leaves from inside the paint callback.
    QCOMPARE(harness.transport->requests.size(), 0);
    QCOMPARE(harness.client->inFlightCount(), 0);
    QCOMPARE(harness.client->keyCount(), 2);

    QTRY_COMPARE_WITH_TIMEOUT(harness.transport->requests.size(), 1, WaitMilliseconds);
    QCOMPARE(harness.transport->requests.constFirst().second,
             (QStringList{QStringLiteral("/a"), QStringLiteral("/b")}));
    QCOMPARE(harness.client->inFlightCount(), 1);

    // A key that is already in flight is not asked for again.
    QCOMPARE(harness.client->lookup(QStringLiteral("/a")), QStringList());
    QTest::qWait(20);
    QCOMPARE(harness.transport->requests.size(), 1);
}

void OverlayClientTest::readyRecordShowsIconAndNotifiesOnce()
{
    Harness harness;
    const QString path = QStringLiteral("/repo/file");
    QCOMPARE(harness.client->lookup(path), QStringList());
    harness.answer({path}, State::Modified, 3);

    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 1, WaitMilliseconds);
    QCOMPARE(harness.changes->constFirst().at(0).toString(), path);
    QCOMPARE(harness.changes->constFirst().at(1).toStringList(), icon(QStringLiteral("modified")));
    QCOMPARE(harness.client->lookup(path), icon(QStringLiteral("modified")));
    QCOMPARE(harness.client->generation(), Generation);
    QCOMPARE(harness.client->inFlightCount(), 0);

    // An unchanged presentation is not reported again, even when freshness is renewed.
    Q_EMIT harness.transport->recordsChanged({record(path, State::Modified, 4)}, Generation);
    QTest::qWait(20);
    QCOMPARE(harness.changes->size(), 1);
}

void OverlayClientTest::everyStateMapsToAnIconOrNone_data()
{
    QTest::addColumn<int>("state");
    QTest::addColumn<QString>("expected");
    QTest::newRow("clean") << int(State::Clean) << QStringLiteral("emblem-linuxgitshell-clean");
    QTest::newRow("modified") << int(State::Modified)
                              << QStringLiteral("emblem-linuxgitshell-modified");
    QTest::newRow("added") << int(State::Added) << QStringLiteral("emblem-linuxgitshell-added");
    QTest::newRow("conflict") << int(State::Conflict)
                              << QStringLiteral("emblem-linuxgitshell-conflict");
    QTest::newRow("untracked") << int(State::Untracked)
                               << QStringLiteral("emblem-linuxgitshell-untracked");
    // Modeled but without a glyph yet: never an icon, and never Clean.
    QTest::newRow("deleted") << int(State::Deleted) << QString();
    QTest::newRow("ignored") << int(State::Ignored) << QString();
    QTest::newRow("unknown") << int(State::Unknown) << QString();
}

void OverlayClientTest::everyStateMapsToAnIconOrNone()
{
    QFETCH(int, state);
    QFETCH(QString, expected);
    QCOMPARE(OverlayClient::iconNameFor(static_cast<State>(state)), expected);

    Harness harness;
    const QString path = QStringLiteral("/repo/file");
    QCOMPARE(harness.client->lookup(path), QStringList());
    harness.answer({path}, static_cast<State>(state), 1);
    QTRY_COMPARE_WITH_TIMEOUT(harness.client->inFlightCount(), 0, WaitMilliseconds);
    QCOMPARE(harness.client->lookup(path),
             expected.isEmpty() ? QStringList() : QStringList{expected});
}

void OverlayClientTest::nonReadyAndMalformedRecordsClearIcons()
{
    Harness harness;
    const QString path = QStringLiteral("/repo/file");
    QCOMPARE(harness.client->lookup(path), QStringList());
    harness.answer({path}, State::Clean, 1);
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 1, WaitMilliseconds);
    QCOMPARE(harness.client->lookup(path), icon(QStringLiteral("clean")));

    // Every non-Ready validity removes the icon and tells the host to repaint.
    int expected = 1;
    quint64 revision = 2;
    for (const Validity validity :
         {Validity::Pending, Validity::Stale, Validity::Degraded, Validity::Unavailable})
    {
        Q_EMIT harness.transport->recordsChanged({record(path, State::Clean, revision++)},
                                                 Generation);
        Q_EMIT harness.transport->recordsChanged({record(path, State::Clean, revision++, validity)},
                                                 Generation);
        QTRY_VERIFY_WITH_TIMEOUT(harness.changes->size() > expected, WaitMilliseconds);
        expected = static_cast<int>(harness.changes->size());
        QCOMPARE(harness.changes->constLast().at(1).toStringList(), QStringList());
        QCOMPARE(harness.client->lookup(path), QStringList());
        // Restore the icon for the next round.
        Q_EMIT harness.transport->recordsChanged({record(path, State::Clean, revision++)},
                                                 Generation);
        QTRY_VERIFY_WITH_TIMEOUT(harness.changes->size() > expected, WaitMilliseconds);
        expected = static_cast<int>(harness.changes->size());
        QCOMPARE(harness.client->lookup(path), icon(QStringLiteral("clean")));
    }

    // An unknown value never preserves the previous Clean assertion.
    QVariantMap unknownState = record(path, State::Clean, revision++);
    unknownState.insert(QStringLiteral("state"), QStringLiteral("sparkling"));
    Q_EMIT harness.transport->recordsChanged({unknownState}, Generation);
    QTRY_VERIFY_WITH_TIMEOUT(harness.changes->size() > expected, WaitMilliseconds);
    QCOMPARE(harness.client->lookup(path), QStringList());
}

void OverlayClientTest::ignoresLowerRevisionsAndOtherGenerations()
{
    Harness harness;
    const QString path = QStringLiteral("/repo/file");
    QCOMPARE(harness.client->lookup(path), QStringList());
    harness.answer({path}, State::Modified, 10);
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 1, WaitMilliseconds);

    // Reordered and late replies, and anything from another service instance, change nothing.
    Q_EMIT harness.transport->recordsChanged({record(path, State::Clean, 9)}, Generation);
    Q_EMIT harness.transport->recordsChanged({record(path, State::Conflict, 99)}, Generation + 1);
    Q_EMIT harness.transport->recordsChanged({record(path, State::Conflict, 99)}, 0);
    QTest::qWait(20);
    QCOMPARE(harness.client->lookup(path), icon(QStringLiteral("modified")));
    QCOMPARE(harness.changes->size(), 1);

    Q_EMIT harness.transport->recordsChanged({record(path, State::Added, 11)}, Generation);
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 2, WaitMilliseconds);
    QCOMPARE(harness.client->lookup(path), icon(QStringLiteral("added")));

    // Records for paths nobody asked about are dropped rather than creating entries.
    Q_EMIT harness.transport->recordsChanged({record(QStringLiteral("/other"), State::Clean, 1)},
                                             Generation);
    QCOMPARE(harness.client->keyCount(), 1);
}

void OverlayClientTest::generationChangeResetsRevisionsAndKeepsKeys()
{
    Harness harness;
    const QString path = QStringLiteral("/repo/file");
    QCOMPARE(harness.client->lookup(path), QStringList());
    harness.answer({path}, State::Modified, 500);
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 1, WaitMilliseconds);
    QCOMPARE(harness.client->lookup(path), icon(QStringLiteral("modified")));

    // The service restarted: a renewal is answered with another generation.
    harness.client->renewNow();
    QTRY_COMPARE_WITH_TIMEOUT(harness.transport->requests.size(), 2, WaitMilliseconds);
    const quint64 id = harness.transport->lastId();
    Q_EMIT harness.transport->requestAccepted(id, Generation + 1, {path});
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 2, WaitMilliseconds);
    QCOMPARE(harness.changes->constLast().at(1).toStringList(), QStringList());
    QCOMPARE(harness.client->generation(), Generation + 1);
    QCOMPARE(harness.client->lookup(path), QStringList());

    // Old-generation messages are ignored; the new one numbers its revisions afresh.
    Q_EMIT harness.transport->recordsChanged({record(path, State::Clean, 600)}, Generation);
    Q_EMIT harness.transport->initialResults(id, {record(path, State::Clean, 1)}, Generation + 1);
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 3, WaitMilliseconds);
    QCOMPARE(harness.client->lookup(path), icon(QStringLiteral("clean")));
    // The retained key was not asked for twice: it was already part of the accepted request.
    QCOMPARE(harness.transport->requests.size(), 2);
}

void OverlayClientTest::initialRecordsMayOvertakeTheirOwnAcceptance()
{
    OverlayClientLimits limits;
    limits.maxBatch = 1; // two requests in flight at once
    Harness harness(limits);
    const QString first = QStringLiteral("/repo/first");
    const QString second = QStringLiteral("/repo/second");
    QCOMPARE(harness.client->lookup(first), QStringList());
    QCOMPARE(harness.client->lookup(second), QStringList());
    QTRY_COMPARE_WITH_TIMEOUT(harness.transport->requests.size(), 2, WaitMilliseconds);
    const quint64 firstId = harness.transport->requests.at(0).first;
    const quint64 secondId = harness.transport->requests.at(1).first;

    // The transport frees held messages when the first request is accepted, so the second
    // request's records arrive before its own acceptance. They must not be lost.
    Q_EMIT harness.transport->requestAccepted(firstId, Generation, {first});
    Q_EMIT harness.transport->initialResults(firstId, {record(first, State::Clean, 1)}, Generation);
    Q_EMIT harness.transport->initialResults(secondId, {record(second, State::Modified, 1)},
                                             Generation);
    QCOMPARE(harness.client->inFlightCount(), 1);
    Q_EMIT harness.transport->requestAccepted(secondId, Generation, {second});

    QTRY_COMPARE_WITH_TIMEOUT(harness.client->inFlightCount(), 0, WaitMilliseconds);
    QCOMPARE(harness.client->lookup(second), icon(QStringLiteral("modified")));
    QCOMPARE(harness.client->lookup(first), icon(QStringLiteral("clean")));
}

void OverlayClientTest::initialFreshnessIsAgedFromRequestTime()
{
    Harness harness;
    const QString path = QStringLiteral("/repo/file");
    QCOMPARE(harness.client->lookup(path), QStringList());
    QTRY_VERIFY_WITH_TIMEOUT(!harness.transport->requests.isEmpty(), WaitMilliseconds);
    const quint64 id = harness.transport->lastId();

    // The answer takes four seconds to arrive and claims five seconds of freshness.
    harness.clock += 4s;
    Q_EMIT harness.transport->requestAccepted(id, Generation, {path});
    Q_EMIT harness.transport->initialResults(
        id, {record(path, State::Clean, 1, Validity::Ready, 5000)}, Generation);
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 1, WaitMilliseconds);
    // Subtracting request time would leave one second; counting from the reply leaves five.
    QCOMPARE(harness.client->lookup(path), icon(QStringLiteral("clean")));
    harness.clock += 4s + 500ms;
    QCOMPARE(harness.client->lookup(path), QStringList());
}

void OverlayClientTest::expiryClearsIconsAndRequestsRenewal()
{
    Harness harness;
    const QString path = QStringLiteral("/repo/file");
    QCOMPARE(harness.client->lookup(path), QStringList());
    harness.answer({path}, State::Modified, 1);
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 1, WaitMilliseconds);
    QCOMPARE(harness.client->lookup(path), icon(QStringLiteral("modified")));
    const int requests = static_cast<int>(harness.transport->requests.size());

    harness.clock += 9s;
    harness.client->expireNow();
    QTest::qWait(20);
    QCOMPARE(harness.changes->size(), 1); // still fresh

    // Idle views lose the icon without another paint callback, and a fresh answer is requested.
    harness.clock += 2s;
    harness.client->expireNow();
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 2, WaitMilliseconds);
    QCOMPARE(harness.changes->constLast().at(1).toStringList(), QStringList());
    QTRY_COMPARE_WITH_TIMEOUT(harness.transport->requests.size(), requests + 1, WaitMilliseconds);
    QCOMPARE(harness.transport->requests.constLast().second, QStringList{path});
}

void OverlayClientTest::failedRequestsClearIconsAndRetryWithBackoff()
{
    OverlayClientLimits limits;
    limits.minBackoff = 150ms;
    Harness harness(limits, false);
    const QString path = QStringLiteral("/repo/file");
    QCOMPARE(harness.client->lookup(path), QStringList());
    harness.answer({path}, State::Clean, 1);
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 1, WaitMilliseconds);
    QCOMPARE(harness.client->lookup(path), icon(QStringLiteral("clean")));

    harness.client->renewNow();
    QTRY_COMPARE_WITH_TIMEOUT(harness.transport->requests.size(), 2, WaitMilliseconds);
    Q_EMIT harness.transport->requestFailed(harness.transport->lastId());

    // Stale presentation goes away at once; the retry waits for the backoff.
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 2, WaitMilliseconds);
    QCOMPARE(harness.client->lookup(path), QStringList());
    QTest::qWait(50);
    QCOMPARE(harness.transport->requests.size(), 2);
    QTRY_COMPARE_WITH_TIMEOUT(harness.transport->requests.size(), 3, WaitMilliseconds);
    QCOMPARE(harness.transport->requests.constLast().second, QStringList{path});

    // A second failure doubles the delay.
    Q_EMIT harness.transport->requestFailed(harness.transport->lastId());
    QTest::qWait(200);
    QCOMPARE(harness.transport->requests.size(), 3);
    QTRY_COMPARE_WITH_TIMEOUT(harness.transport->requests.size(), 4, WaitMilliseconds);
}

void OverlayClientTest::rejectedPathsAreRetried()
{
    OverlayClientLimits limits;
    limits.minBackoff = 100ms;
    Harness harness(limits, false);
    const QString kept = QStringLiteral("/repo/kept");
    const QString rejected = QStringLiteral("/repo/rejected");
    QCOMPARE(harness.client->lookup(kept), QStringList());
    QCOMPARE(harness.client->lookup(rejected), QStringList());
    QTRY_COMPARE_WITH_TIMEOUT(harness.transport->requests.size(), 1, WaitMilliseconds);
    const quint64 id = harness.transport->lastId();

    // The service accepted only one of the two paths: that one completes on its record.
    Q_EMIT harness.transport->requestAccepted(id, Generation, {kept});
    Q_EMIT harness.transport->initialResults(id, {record(kept, State::Clean, 1)}, Generation);
    QTRY_COMPARE_WITH_TIMEOUT(harness.client->inFlightCount(), 0, WaitMilliseconds);
    QTRY_COMPARE_WITH_TIMEOUT(harness.transport->requests.size(), 2, WaitMilliseconds);
    QCOMPARE(harness.transport->requests.constLast().second, QStringList{rejected});
    QCOMPARE(harness.client->lookup(kept), icon(QStringLiteral("clean")));
}

void OverlayClientTest::requestTimeoutFailsTheRequest()
{
    OverlayClientLimits limits;
    limits.requestTimeout = 50ms;
    limits.minBackoff = 50ms;
    // The timeout timer follows the real clock here, so use it instead of the fake one.
    auto owned = std::make_unique<FakeTransport>();
    FakeTransport* transport = owned.get();
    OverlayClient client(std::move(owned), limits);

    QCOMPARE(client.lookup(QStringLiteral("/repo/file")), QStringList());
    QTRY_COMPARE_WITH_TIMEOUT(transport->requests.size(), 1, WaitMilliseconds);
    QCOMPARE(client.inFlightCount(), 1);
    // No answer ever comes: the in-flight accounting is cleared and the key is asked for again.
    QTRY_COMPARE_WITH_TIMEOUT(transport->requests.size(), 2, WaitMilliseconds);
    QCOMPARE(transport->requests.constLast().second, QStringList{QStringLiteral("/repo/file")});
}

void OverlayClientTest::evictionClearsTheIconAndReleasesTheKey()
{
    OverlayClientLimits limits;
    limits.maxKeys = 2;
    Harness harness(limits);
    const QString first = QStringLiteral("/repo/a");
    QCOMPARE(harness.client->lookup(first), QStringList());
    harness.answer({first}, State::Modified, 1);
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 1, WaitMilliseconds);
    QCOMPARE(harness.client->lookup(first), icon(QStringLiteral("modified"))); // now displayed

    QCOMPARE(harness.client->lookup(QStringLiteral("/repo/b")), QStringList());
    QCOMPARE(harness.client->lookup(QStringLiteral("/repo/c")), QStringList());
    QCOMPARE(harness.client->keyCount(), 2);

    // The least recently used key loses its displayed icon and its observation.
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 2, WaitMilliseconds);
    QCOMPARE(harness.changes->constLast().at(0).toString(), first);
    QCOMPARE(harness.changes->constLast().at(1).toStringList(), QStringList());
    QTRY_COMPARE_WITH_TIMEOUT(harness.transport->releases, QStringList{first}, WaitMilliseconds);
}

void OverlayClientTest::renewalRequestsRetainedKeysInBatches()
{
    OverlayClientLimits limits;
    limits.maxBatch = 2;
    Harness harness(limits);
    const QStringList keys{QStringLiteral("/a"), QStringLiteral("/b"), QStringLiteral("/c"),
                           QStringLiteral("/d"), QStringLiteral("/e")};
    for (const QString& key : keys)
    {
        QCOMPARE(harness.client->lookup(key), QStringList());
    }
    // Initial requests respect the batch size and the in-flight cap.
    QTRY_COMPARE_WITH_TIMEOUT(harness.transport->requests.size(), 3, WaitMilliseconds);
    QCOMPARE(harness.transport->requests.at(0).second, (QStringList{keys.at(0), keys.at(1)}));
    QCOMPARE(harness.transport->requests.at(2).second, QStringList{keys.at(4)});
    for (const auto& [id, paths] : std::as_const(harness.transport->requests))
    {
        Q_EMIT harness.transport->requestAccepted(id, Generation, paths);
        QList<QVariantMap> records;
        for (const QString& path : paths)
        {
            records.append(record(path, State::Clean, 1));
        }
        Q_EMIT harness.transport->initialResults(id, records, Generation);
    }
    QTRY_COMPARE_WITH_TIMEOUT(harness.client->inFlightCount(), 0, WaitMilliseconds);

    harness.client->renewNow();
    QTRY_COMPARE_WITH_TIMEOUT(harness.transport->requests.size(), 6, WaitMilliseconds);
    QStringList renewed;
    for (qsizetype index = 3; index < harness.transport->requests.size(); ++index)
    {
        QVERIFY(harness.transport->requests.at(index).second.size() <= 2);
        renewed.append(harness.transport->requests.at(index).second);
    }
    renewed.sort();
    QCOMPARE(renewed, keys);
}

void OverlayClientTest::disablingClearsReleasesAndStops()
{
    Harness harness;
    const QString path = QStringLiteral("/repo/file");
    QCOMPARE(harness.client->lookup(path), QStringList());
    harness.answer({path}, State::Clean, 1);
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 1, WaitMilliseconds);
    QCOMPARE(harness.client->lookup(path), icon(QStringLiteral("clean")));

    harness.client->setEnabled(false);
    QVERIFY(!harness.client->isEnabled());
    // Displayed icons are cleared immediately and every observation is released.
    QCOMPARE(harness.changes->size(), 2);
    QCOMPARE(harness.changes->constLast().at(0).toString(), path);
    QCOMPARE(harness.changes->constLast().at(1).toStringList(), QStringList());
    QCOMPARE(harness.transport->releases, QStringList{path});
    QVERIFY(harness.transport->stopped);
    QCOMPARE(harness.client->keyCount(), 0);

    // A disabled client does no work at all.
    const int requests = static_cast<int>(harness.transport->requests.size());
    QCOMPARE(harness.client->lookup(path), QStringList());
    harness.client->renewNow();
    QTest::qWait(30);
    QCOMPARE(harness.transport->requests.size(), requests);
    QCOMPARE(harness.client->keyCount(), 0);
}

void OverlayClientTest::ownerLossClearsAndRequestsAgain()
{
    OverlayClientLimits limits;
    limits.minBackoff = 100ms;
    Harness harness(limits, false);
    const QString path = QStringLiteral("/repo/file");
    QCOMPARE(harness.client->lookup(path), QStringList());
    harness.answer({path}, State::Modified, 1);
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 1, WaitMilliseconds);
    QCOMPARE(harness.client->lookup(path), icon(QStringLiteral("modified")));

    Q_EMIT harness.transport->ownerLost();
    QTRY_COMPARE_WITH_TIMEOUT(harness.changes->size(), 2, WaitMilliseconds);
    QCOMPARE(harness.changes->constLast().at(1).toStringList(), QStringList());
    QCOMPARE(harness.client->generation(), quint64(0));
    QCOMPARE(harness.client->lookup(path), QStringList());

    // The retained key is requested from the next owner, after a backoff.
    QTest::qWait(30);
    QCOMPARE(harness.transport->requests.size(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(harness.transport->requests.size(), 2, WaitMilliseconds);
    QCOMPARE(harness.transport->requests.constLast().second, QStringList{path});
}

void OverlayClientTest::pathKeysExistOnlyForLocalUrls()
{
    QCOMPARE(OverlayClient::pathKeyForUrl(QUrl::fromLocalFile(QStringLiteral("/a/b/../c"))),
             QStringLiteral("/a/c"));
    QCOMPARE(OverlayClient::pathKeyForUrl(QUrl(QStringLiteral("https://example.org/a"))),
             QString());
    QCOMPARE(OverlayClient::pathKeyForUrl(QUrl(QStringLiteral("smb://host/share/file"))),
             QString());
    QCOMPARE(OverlayClient::pathKeyForUrl(QUrl(QStringLiteral("trash:/file"))), QString());
    QCOMPARE(OverlayClient::pathKeyForUrl(QUrl()), QString());
    QCOMPARE(OverlayClient::pathKeyForUrl(
                 QUrl::fromLocalFile(QStringLiteral("/") + QString(5000, QLatin1Char('a')))),
             QString());
    QCOMPARE(OverlayClient::pathKeyForUrl(QUrl::fromLocalFile(QStringLiteral("/with space/é"))),
             QStringLiteral("/with space/é"));
}

QTEST_GUILESS_MAIN(OverlayClientTest)
#include "OverlayClientTest.moc"
