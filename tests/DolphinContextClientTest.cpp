// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "RepositoryContextClient.h"

#include "linuxgitshell/repositorycontext/RepositoryContextWire.h"

#include <QSignalSpy>
#include <QTest>

#include <chrono>
#include <memory>

using LinuxGitShell::RepositoryContextFreshness;
using LinuxGitShell::RepositoryContextSnapshot;
using LinuxGitShell::RepositoryContextState;
using LinuxGitShell::repositoryContextToVariantMap;
using LinuxGitShell::Dolphin::RepositoryContextClient;
using LinuxGitShell::Dolphin::RepositoryContextClientLimits;
using LinuxGitShell::Dolphin::RepositoryContextTransport;

using namespace std::chrono_literals;

namespace
{

class FakeTransport final : public RepositoryContextTransport
{
    Q_OBJECT

  public:
    using RepositoryContextTransport::RepositoryContextTransport;

    void requestContext(const QStringList& pathKeys) override { requests.append(pathKeys); }

    QList<QStringList> requests;
};

struct ClientFixture
{
    explicit ClientFixture(const RepositoryContextClientLimits& limits = {})
    {
        auto fake = std::make_unique<FakeTransport>();
        transport = fake.get();
        client = std::make_unique<RepositoryContextClient>(std::move(fake), limits);
    }

    FakeTransport* transport = nullptr;
    std::unique_ptr<RepositoryContextClient> client;
};

QVariantMap insideContext(const QString& root)
{
    RepositoryContextSnapshot snapshot;
    snapshot.state = RepositoryContextState::InsideRepository;
    snapshot.repositoryRoot = root;
    return repositoryContextToVariantMap(snapshot);
}

constexpr quint64 Generation = 11;

} // namespace

class DolphinContextClientTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void refreshWaitsForEventLoop();
    void batchesAndDeduplicatesRequests();
    void ignoresEmptyKeys();
    void storesReceivedContext();
    void newGenerationDropsSnapshots();
    void failedRequestCanBeRetried();
    void ignoresUnrecognizedContext();
    void expiredInFlightRequestIsResent();
    void boundsInFlightRequests();
};

void DolphinContextClientTest::refreshWaitsForEventLoop()
{
    ClientFixture fixture;

    QCOMPARE(fixture.client->lookup(QStringLiteral("/repo")).freshness,
             RepositoryContextFreshness::Cold);
    fixture.client->refreshLater({QStringLiteral("/repo")});

    // Nothing leaves before the caller, such as KAbstractFileItemActionPlugin::actions(), returns.
    QVERIFY(fixture.transport->requests.isEmpty());
    QTRY_COMPARE(fixture.transport->requests.size(), 1);
    QCOMPARE(fixture.transport->requests.constFirst(), QStringList{QStringLiteral("/repo")});
    QCOMPARE(fixture.client->inFlightCount(), 1);
}

void DolphinContextClientTest::batchesAndDeduplicatesRequests()
{
    ClientFixture fixture;

    fixture.client->refreshLater({QStringLiteral("/a"), QStringLiteral("/b")});
    fixture.client->refreshLater({QStringLiteral("/a")});
    QTRY_COMPARE(fixture.transport->requests.size(), 1);
    QCOMPARE(fixture.transport->requests.constFirst(),
             (QStringList{QStringLiteral("/a"), QStringLiteral("/b")}));

    fixture.client->refreshLater({QStringLiteral("/a")});
    QTest::qWait(50);
    QCOMPARE(fixture.transport->requests.size(), 1);
}

void DolphinContextClientTest::ignoresEmptyKeys()
{
    ClientFixture fixture;

    fixture.client->refreshLater({QString()});
    QTest::qWait(50);

    QVERIFY(fixture.transport->requests.isEmpty());
}

void DolphinContextClientTest::storesReceivedContext()
{
    ClientFixture fixture;
    QSignalSpy updated(fixture.client.get(), &RepositoryContextClient::contextUpdated);
    fixture.client->refreshLater({QStringLiteral("/repo")});
    QTRY_COMPARE(fixture.transport->requests.size(), 1);

    Q_EMIT fixture.transport->requestAccepted(Generation);
    Q_EMIT fixture.transport->contextReceived(QStringLiteral("/repo"),
                                              insideContext(QStringLiteral("/repo")), Generation);

    QCOMPARE(updated.size(), 1);
    QCOMPARE(fixture.client->generation(), Generation);
    QCOMPARE(fixture.client->inFlightCount(), 0);
    const auto lookup = fixture.client->lookup(QStringLiteral("/repo"));
    QCOMPARE(lookup.freshness, RepositoryContextFreshness::Warm);
    QCOMPARE(lookup.snapshot.value_or(RepositoryContextSnapshot{}).repositoryRoot,
             QStringLiteral("/repo"));
}

void DolphinContextClientTest::newGenerationDropsSnapshots()
{
    ClientFixture fixture;
    Q_EMIT fixture.transport->contextReceived(QStringLiteral("/a"),
                                              insideContext(QStringLiteral("/a")), Generation);
    QCOMPARE(fixture.client->lookup(QStringLiteral("/a")).freshness,
             RepositoryContextFreshness::Warm);

    Q_EMIT fixture.transport->contextReceived(QStringLiteral("/b"),
                                              insideContext(QStringLiteral("/b")), Generation + 1);

    QCOMPARE(fixture.client->generation(), Generation + 1);
    QCOMPARE(fixture.client->lookup(QStringLiteral("/a")).freshness,
             RepositoryContextFreshness::Cold);
    QCOMPARE(fixture.client->lookup(QStringLiteral("/b")).freshness,
             RepositoryContextFreshness::Warm);
}

void DolphinContextClientTest::failedRequestCanBeRetried()
{
    ClientFixture fixture;
    QSignalSpy unavailable(fixture.client.get(), &RepositoryContextClient::contextUnavailable);
    fixture.client->refreshLater({QStringLiteral("/repo")});
    QTRY_COMPARE(fixture.transport->requests.size(), 1);

    Q_EMIT fixture.transport->requestFailed({QStringLiteral("/repo")});
    QCOMPARE(unavailable.size(), 1);
    QCOMPARE(fixture.client->inFlightCount(), 0);
    QCOMPARE(fixture.client->lookup(QStringLiteral("/repo")).freshness,
             RepositoryContextFreshness::Cold);

    fixture.client->refreshLater({QStringLiteral("/repo")});
    QTRY_COMPARE(fixture.transport->requests.size(), 2);
}

void DolphinContextClientTest::ignoresUnrecognizedContext()
{
    ClientFixture fixture;
    QSignalSpy updated(fixture.client.get(), &RepositoryContextClient::contextUpdated);

    Q_EMIT fixture.transport->contextReceived(
        QStringLiteral("/repo"), {{QStringLiteral("state"), QStringLiteral("future")}}, Generation);

    QVERIFY(updated.isEmpty());
    QCOMPARE(fixture.client->generation(), 0U);
    QCOMPARE(fixture.client->lookup(QStringLiteral("/repo")).freshness,
             RepositoryContextFreshness::Cold);
}

void DolphinContextClientTest::expiredInFlightRequestIsResent()
{
    RepositoryContextClientLimits limits;
    limits.inFlightTimeout = 0ms;
    ClientFixture fixture(limits);

    fixture.client->refreshLater({QStringLiteral("/repo")});
    QTRY_COMPARE(fixture.transport->requests.size(), 1);
    fixture.client->refreshLater({QStringLiteral("/repo")});

    QTRY_COMPARE(fixture.transport->requests.size(), 2);
}

void DolphinContextClientTest::boundsInFlightRequests()
{
    RepositoryContextClientLimits limits;
    limits.maxInFlight = 2;
    ClientFixture fixture(limits);

    fixture.client->refreshLater(
        {QStringLiteral("/a"), QStringLiteral("/b"), QStringLiteral("/c")});
    QTRY_COMPARE(fixture.transport->requests.size(), 1);
    QCOMPARE(fixture.transport->requests.constFirst(),
             (QStringList{QStringLiteral("/a"), QStringLiteral("/b")}));

    Q_EMIT fixture.transport->contextReceived(QStringLiteral("/a"),
                                              insideContext(QStringLiteral("/a")), Generation);
    fixture.client->refreshLater({QStringLiteral("/c")});
    QTRY_COMPARE(fixture.transport->requests.size(), 2);
    QCOMPARE(fixture.transport->requests.constLast(), QStringList{QStringLiteral("/c")});
}

QTEST_GUILESS_MAIN(DolphinContextClientTest)

#include "DolphinContextClientTest.moc"
