// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "OverlayStatusProvider.h"
#include "GitTestSupport.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <chrono>

using namespace LinuxGitShell;
using namespace std::chrono_literals;
using State = RepositoryStatusState;
using Validity = RepositoryStatusValidity;

namespace
{

constexpr int WaitMilliseconds = 15000;
const QString Alice = QStringLiteral(":1.100");
const QString Bob = QStringLiteral(":1.200");

[[nodiscard]] QString child(const QString& root, const QString& relative)
{
    return QDir(root).filePath(relative);
}

// Collects what one owner receives, as a client would, and checks revision ordering.
class Recorder final : public QObject
{
  public:
    Recorder(OverlayStatusProvider& provider, const QString& owner)
    {
        connect(&provider, &OverlayStatusProvider::initialResults, this,
                [this, owner](const QString& sender, quint64 requestId,
                              const QList<OverlayRecord>& records, quint64)
                {
                    if (sender != owner)
                    {
                        return;
                    }
                    initialRequests.append({requestId, records});
                    for (const OverlayRecord& record : records)
                    {
                        latest.insert(record.path, record);
                    }
                });
        connect(&provider, &OverlayStatusProvider::overlaysChanged, this,
                [this, owner](const QString& sender, const QList<OverlayRecord>& records, quint64)
                {
                    if (sender != owner)
                    {
                        return;
                    }
                    ++changedBatches;
                    for (const OverlayRecord& record : records)
                    {
                        const auto it = latest.constFind(record.path);
                        if (it != latest.cend() &&
                            changedRevisions.value(record.path) >= record.revision)
                        {
                            revisionViolation = true;
                        }
                        changedRevisions.insert(record.path, record.revision);
                        latest.insert(record.path, record);
                    }
                });
    }

    [[nodiscard]] OverlayRecord record(const QString& path) const { return latest.value(path); }
    [[nodiscard]] bool is(const QString& path, State state,
                          Validity validity = Validity::Ready) const
    {
        const auto it = latest.constFind(path);
        return it != latest.cend() && it->state == state && it->validity == validity;
    }

    QList<std::pair<quint64, QList<OverlayRecord>>> initialRequests;
    QHash<QString, OverlayRecord> latest;
    QHash<QString, quint64> changedRevisions;
    int changedBatches = 0;
    bool revisionViolation = false;
};

struct Fixture
{
    QTemporaryDir directory;
    QString root;
};

// Committed: clean.txt, modified.txt, .gitignore. Working tree: modified.txt changed, untracked.txt
// and ignored.log added.
[[nodiscard]] bool createFixture(Fixture& fixture)
{
    if (!fixture.directory.isValid())
    {
        return false;
    }
    fixture.root = QDir(fixture.directory.path()).canonicalPath() + QStringLiteral("/repo");
    const QString& root = fixture.root;
    return QDir().mkpath(root) &&
           GitTestSupport::runGit(root, {QStringLiteral("init"), QStringLiteral(".")}) &&
           GitTestSupport::writeTextFile(child(root, QStringLiteral("clean.txt")), "c\n") &&
           GitTestSupport::writeTextFile(child(root, QStringLiteral("modified.txt")), "m\n") &&
           GitTestSupport::writeTextFile(child(root, QStringLiteral(".gitignore")), "*.log\n") &&
           GitTestSupport::runGit(root, {QStringLiteral("add"), QStringLiteral("-A")}) &&
           GitTestSupport::runGit(
               root, {QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("base")}) &&
           GitTestSupport::writeTextFile(child(root, QStringLiteral("modified.txt")),
                                         "changed\n") &&
           GitTestSupport::writeTextFile(child(root, QStringLiteral("untracked.txt")), "u\n") &&
           GitTestSupport::writeTextFile(child(root, QStringLiteral("ignored.log")), "l\n");
}

[[nodiscard]] OverlayProviderLimits fastLimits()
{
    OverlayProviderLimits limits;
    limits.debounce = 10ms;
    limits.pollInterval = 1h;
    return limits;
}

} // namespace

class OverlayStatusProviderTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    void answersPendingAtOnceThenPublishesStates();
    void warmRequestIsReadyWithRemainingFreshness();
    void rejectsInvalidRequestsAndDeduplicates();
    void reportsPartialCapacityExplicitly();
    void disabledRecursionKeepsFileStatesButNoFolderSummary();
    void outsideRepositoryIsUnavailable();
    void excludedLocationsAreUnavailable();
    void bareRepositoryIsUnavailable();
    void sparseCheckoutIsUnavailable();
    void refreshRepositoryInvalidatesThenRepublishes();
    void refreshRepositoryRequiresAnObservedRepository();
    void pollPicksUpExternalChanges();
    void unchangedPollsRenewFreshnessAndLaterChangesAreStillSeen();
    void releaseStopsPublishingAndPolling();
    void leasesExpireAndRenewalExtendsThem();
    void staleSnapshotYieldsUnknownThenRefreshes();
    void nestedRepositoryOwnsItsContents();
    void outputOverflowDegradesWithoutClaimingClean();
    void gitFailureDegradesAndRecovers();
};

void OverlayStatusProviderTest::initTestCase()
{
    GitTestSupport::isolateGitConfiguration();
    qRegisterMetaType<OverlayRecord>();
    qRegisterMetaType<QList<OverlayRecord>>();
}

void OverlayStatusProviderTest::answersPendingAtOnceThenPublishesStates()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    OverlayStatusProvider provider(fastLimits());
    Recorder alice(provider, Alice);

    const QStringList paths{child(fixture.root, QStringLiteral("clean.txt")),
                            child(fixture.root, QStringLiteral("modified.txt")),
                            child(fixture.root, QStringLiteral("untracked.txt")),
                            child(fixture.root, QStringLiteral("ignored.log")), fixture.root};
    const OverlayRequestResult result = provider.requestOverlays(Alice, paths, 1);
    QCOMPARE(result.error, OverlayRequestError::None);
    QCOMPARE(result.generation, provider.generation());
    QVERIFY(result.generation != 0);
    QCOMPARE(result.acceptedPaths, paths);

    // Nothing is delivered before the caller's method reply has been sent.
    QCOMPARE(alice.initialRequests.size(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(alice.initialRequests.size(), 1, WaitMilliseconds);
    const auto& [requestId, records] = alice.initialRequests.constFirst();
    QCOMPARE(requestId, quint64(1));
    QCOMPARE(records.size(), paths.size());
    for (qsizetype index = 0; index < paths.size(); ++index)
    {
        QCOMPARE(records.at(index).path, paths.at(index));
        QCOMPARE(records.at(index).state, State::Unknown);
        QCOMPARE(records.at(index).validity, Validity::Pending);
        QCOMPARE(records.at(index).validForMs, quint32(0));
    }

    QTRY_VERIFY_WITH_TIMEOUT(alice.is(paths.at(0), State::Clean), WaitMilliseconds);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(paths.at(1), State::Modified), WaitMilliseconds);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(paths.at(2), State::Untracked), WaitMilliseconds);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(paths.at(3), State::Ignored), WaitMilliseconds);
    // The repository root summarizes its descendants.
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(paths.at(4), State::Modified), WaitMilliseconds);
    for (const QString& path : paths)
    {
        QCOMPARE(alice.record(path).repositoryRoot, fixture.root);
        QVERIFY(alice.record(path).validForMs > 0);
        QVERIFY(alice.record(path).validForMs <= 30000);
    }
    QVERIFY(!alice.revisionViolation);
    QCOMPARE(provider.repositoryCount(), 1);
    QCOMPARE(provider.observationCount(), paths.size());
}

void OverlayStatusProviderTest::warmRequestIsReadyWithRemainingFreshness()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    OverlayStatusProvider provider(fastLimits());
    Recorder alice(provider, Alice);
    Recorder bob(provider, Bob);
    const QString file = child(fixture.root, QStringLiteral("modified.txt"));

    QCOMPARE(provider.requestOverlays(Alice, {file}, 1).acceptedPaths, QStringList{file});
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Modified), WaitMilliseconds);

    // A second client on an unchanged snapshot still gets one initial record with the state.
    const OverlayRequestResult second = provider.requestOverlays(Bob, {file}, 1);
    QCOMPARE(second.acceptedPaths, QStringList{file});
    QTRY_COMPARE_WITH_TIMEOUT(bob.initialRequests.size(), 1, WaitMilliseconds);
    const OverlayRecord record = bob.initialRequests.constFirst().second.constFirst();
    QCOMPARE(record.state, State::Modified);
    QCOMPARE(record.validity, Validity::Ready);
    QVERIFY(record.validForMs > 0);
    QCOMPARE(record.repositoryRoot, fixture.root);
}

void OverlayStatusProviderTest::rejectsInvalidRequestsAndDeduplicates()
{
    OverlayStatusProvider provider(fastLimits());
    Recorder alice(provider, Alice);

    QCOMPARE(provider.requestOverlays(Alice, {QStringLiteral("/tmp/x")}, 0).error,
             OverlayRequestError::InvalidRequest);
    QCOMPARE(provider.requestOverlays(QString(), {QStringLiteral("/tmp/x")}, 1).error,
             OverlayRequestError::InvalidRequest);
    QStringList tooMany;
    for (int index = 0; index < 257; ++index)
    {
        tooMany.append(QStringLiteral("/proc/%1").arg(index));
    }
    QCOMPARE(provider.requestOverlays(Alice, tooMany, 2).error,
             OverlayRequestError::InvalidRequest);
    QCOMPARE(provider.observationCount(), 0);

    // Malformed, oversized, and duplicate keys are not accepted; the rest keep their order.
    const QString oversized = QStringLiteral("/proc/") + QString(5000, QLatin1Char('a'));
    const OverlayRequestResult result = provider.requestOverlays(
        Alice,
        {QStringLiteral("relative/path"), QString(), oversized, QStringLiteral("/proc/a/../b"),
         QStringLiteral("/proc/b"), QStringLiteral("/proc/c//")},
        3);
    QCOMPARE(result.error, OverlayRequestError::None);
    QCOMPARE(result.acceptedPaths,
             (QStringList{QStringLiteral("/proc/b"), QStringLiteral("/proc/c")}));

    // The id stays reserved until its initial records were delivered.
    QCOMPARE(provider.requestOverlays(Alice, {QStringLiteral("/proc/d")}, 3).error,
             OverlayRequestError::InvalidRequest);
    QTRY_COMPARE_WITH_TIMEOUT(alice.initialRequests.size(), 1, WaitMilliseconds);
    QCOMPARE(alice.initialRequests.constFirst().second.size(), 2);
    QCOMPARE(provider.requestOverlays(Alice, {QStringLiteral("/proc/d")}, 3).error,
             OverlayRequestError::None);

    // An empty accepted list queues no work and produces no records.
    QCOMPARE(provider.requestOverlays(Alice, {}, 4).acceptedPaths.size(), 0);
}

void OverlayStatusProviderTest::reportsPartialCapacityExplicitly()
{
    OverlayProviderLimits limits = fastLimits();
    limits.maxObservations = 2;
    OverlayStatusProvider provider(limits);
    Recorder alice(provider, Alice);

    const OverlayRequestResult result = provider.requestOverlays(
        Alice, {QStringLiteral("/proc/a"), QStringLiteral("/proc/b"), QStringLiteral("/proc/c")},
        1);
    QCOMPARE(result.error, OverlayRequestError::None);
    QCOMPARE(result.acceptedPaths.size(), 2);
    QCOMPARE(provider.observationCount(), 2);
    QTRY_COMPARE_WITH_TIMEOUT(alice.initialRequests.size(), 1, WaitMilliseconds);
    QCOMPARE(alice.initialRequests.constFirst().second.size(), 2);

    // Renewing a retained key never needs new capacity.
    QCOMPARE(provider.requestOverlays(Alice, {QStringLiteral("/proc/a")}, 2).acceptedPaths.size(),
             1);
}

void OverlayStatusProviderTest::disabledRecursionKeepsFileStatesButNoFolderSummary()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    QVERIFY(QDir().mkpath(child(fixture.root, QStringLiteral("dir"))));
    QVERIFY(
        GitTestSupport::writeTextFile(child(fixture.root, QStringLiteral("dir/inner.txt")), "i\n"));
    QVERIFY(GitTestSupport::runGit(fixture.root, {QStringLiteral("add"), QStringLiteral("dir")}));
    QVERIFY(GitTestSupport::runGit(
        fixture.root, {QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("dir")}));
    QVERIFY(GitTestSupport::writeTextFile(child(fixture.root, QStringLiteral("dir/inner.txt")),
                                          "changed\n"));

    OverlayProviderOptions options;
    options.recursive = false;
    OverlayStatusProvider provider(fastLimits(), options);
    Recorder alice(provider, Alice);
    const QString file = child(fixture.root, QStringLiteral("dir/inner.txt"));
    const QString folder = child(fixture.root, QStringLiteral("dir"));

    QCOMPARE(provider.requestOverlays(Alice, {file, folder, fixture.root}, 1).acceptedPaths.size(),
             3);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Modified), WaitMilliseconds);
    // Without recursion an ordinary folder carries no summary: evidence is complete, state unknown.
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(folder, State::Unknown), WaitMilliseconds);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(fixture.root, State::Unknown), WaitMilliseconds);
}

void OverlayStatusProviderTest::outsideRepositoryIsUnavailable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString file = child(QDir(directory.path()).canonicalPath(), QStringLiteral("plain.txt"));
    QVERIFY(GitTestSupport::writeTextFile(file, "x\n"));
    OverlayStatusProvider provider(fastLimits());
    Recorder alice(provider, Alice);

    QCOMPARE(provider.requestOverlays(Alice, {file, QDir(directory.path()).canonicalPath()}, 1)
                 .acceptedPaths.size(),
             2);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Unknown, Validity::Unavailable),
                             WaitMilliseconds);
    QTRY_VERIFY_WITH_TIMEOUT(
        alice.is(QDir(directory.path()).canonicalPath(), State::Unknown, Validity::Unavailable),
        WaitMilliseconds);
    QCOMPARE(provider.repositoryCount(), 0);

    // A path that does not exist is unavailable too.
    const QString missing =
        child(QDir(directory.path()).canonicalPath(), QStringLiteral("missing"));
    QCOMPARE(provider.requestOverlays(Alice, {missing}, 2).acceptedPaths.size(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(missing, State::Unknown, Validity::Unavailable),
                             WaitMilliseconds);
}

void OverlayStatusProviderTest::excludedLocationsAreUnavailable()
{
    OverlayStatusProvider provider(fastLimits());
    Recorder alice(provider, Alice);
    const QStringList paths{QStringLiteral("/proc/self/status"), QStringLiteral("/sys/kernel"),
                            QStringLiteral("/dev/null"), QStringLiteral("/run/user")};
    QCOMPARE(provider.requestOverlays(Alice, paths, 1).acceptedPaths, paths);
    QTRY_COMPARE_WITH_TIMEOUT(alice.initialRequests.size(), 1, WaitMilliseconds);
    for (const OverlayRecord& record : alice.initialRequests.constFirst().second)
    {
        QCOMPARE(record.state, State::Unknown);
        QCOMPARE(record.validity, Validity::Unavailable);
    }
    QCOMPARE(provider.repositoryCount(), 0);
}

void OverlayStatusProviderTest::bareRepositoryIsUnavailable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString bare = QDir(directory.path()).canonicalPath() + QStringLiteral("/bare.git");
    QVERIFY(QDir().mkpath(bare));
    QVERIFY(GitTestSupport::runGit(
        bare, {QStringLiteral("init"), QStringLiteral("--bare"), QStringLiteral(".")}));
    OverlayStatusProvider provider(fastLimits());
    Recorder alice(provider, Alice);

    QCOMPARE(provider.requestOverlays(Alice, {bare}, 1).acceptedPaths.size(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(bare, State::Unknown, Validity::Unavailable),
                             WaitMilliseconds);
}

void OverlayStatusProviderTest::sparseCheckoutIsUnavailable()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    QVERIFY(
        GitTestSupport::runGit(fixture.root, {QStringLiteral("sparse-checkout"),
                                              QStringLiteral("init"), QStringLiteral("--cone")}));
    OverlayStatusProvider provider(fastLimits());
    Recorder alice(provider, Alice);
    const QString file = child(fixture.root, QStringLiteral("clean.txt"));

    QCOMPARE(provider.requestOverlays(Alice, {file}, 1).acceptedPaths.size(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Unknown, Validity::Unavailable),
                             WaitMilliseconds);
}

void OverlayStatusProviderTest::refreshRepositoryInvalidatesThenRepublishes()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    OverlayStatusProvider provider(fastLimits());
    Recorder alice(provider, Alice);
    const QString file = child(fixture.root, QStringLiteral("clean.txt"));

    QCOMPARE(provider.requestOverlays(Alice, {file}, 1).acceptedPaths.size(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Clean), WaitMilliseconds);
    const int batchesBefore = alice.changedBatches;

    QVERIFY(GitTestSupport::writeTextFile(file, "edited\n"));
    QVERIFY(provider.refreshRepository(Alice, fixture.root));
    // The invalidation is published on a later turn, after the call returned.
    QCOMPARE(alice.changedBatches, batchesBefore);
    QTRY_VERIFY_WITH_TIMEOUT(alice.changedBatches > batchesBefore, WaitMilliseconds);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Modified), WaitMilliseconds);
    QVERIFY(!alice.revisionViolation);
}

void OverlayStatusProviderTest::refreshRepositoryRequiresAnObservedRepository()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    OverlayStatusProvider provider(fastLimits());
    Recorder alice(provider, Alice);
    const QString file = child(fixture.root, QStringLiteral("clean.txt"));

    QVERIFY(!provider.refreshRepository(Alice, fixture.root));
    QCOMPARE(provider.requestOverlays(Alice, {file}, 1).acceptedPaths.size(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Clean), WaitMilliseconds);

    QVERIFY(!provider.refreshRepository(Bob, fixture.root));
    QVERIFY(!provider.refreshRepository(Alice, QStringLiteral("/nonexistent")));
    QVERIFY(provider.refreshRepository(Alice, fixture.root));
}

void OverlayStatusProviderTest::pollPicksUpExternalChanges()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    OverlayProviderLimits limits = fastLimits();
    limits.pollInterval = 100ms;
    OverlayStatusProvider provider(limits);
    Recorder alice(provider, Alice);
    const QString file = child(fixture.root, QStringLiteral("clean.txt"));

    QCOMPARE(provider.requestOverlays(Alice, {file}, 1).acceptedPaths.size(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Clean), WaitMilliseconds);

    QVERIFY(GitTestSupport::writeTextFile(file, "edited behind our back\n"));
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Modified), WaitMilliseconds);

    // Unchanged polls still renew freshness for every observed key.
    const int batches = alice.changedBatches;
    QTRY_VERIFY_WITH_TIMEOUT(alice.changedBatches > batches, WaitMilliseconds);
    QVERIFY(alice.is(file, State::Modified));
    QVERIFY(!alice.revisionViolation);
}

void OverlayStatusProviderTest::unchangedPollsRenewFreshnessAndLaterChangesAreStillSeen()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    OverlayProviderLimits limits = fastLimits();
    limits.pollInterval = 50ms;
    limits.freshness = 400ms;
    OverlayStatusProvider provider(limits);
    Recorder alice(provider, Alice);
    const QString clean = child(fixture.root, QStringLiteral("clean.txt"));
    const QString modified = child(fixture.root, QStringLiteral("modified.txt"));

    QCOMPARE(provider.requestOverlays(Alice, {clean, modified}, 1).acceptedPaths.size(), 2);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(modified, State::Modified), WaitMilliseconds);

    // Several unchanged polls span more than the freshness window: if they only republished and
    // never renewed the snapshot, the records would turn Stale.
    const int batches = alice.changedBatches;
    QTRY_VERIFY_WITH_TIMEOUT(alice.changedBatches >= batches + 12, WaitMilliseconds);
    QVERIFY(alice.is(clean, State::Clean));
    QVERIFY(alice.is(modified, State::Modified));
    QVERIFY(alice.record(modified).validForMs > 0);
    QVERIFY(!alice.revisionViolation);

    // Changes after skipped rebuilds are still picked up, in both directions.
    QVERIFY(GitTestSupport::writeTextFile(clean, "edited\n"));
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(clean, State::Modified), WaitMilliseconds);
    QVERIFY(GitTestSupport::writeTextFile(clean, "c\n"));
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(clean, State::Clean), WaitMilliseconds);
    QVERIFY(GitTestSupport::runGit(fixture.root,
                                   {QStringLiteral("add"), QStringLiteral("untracked.txt")}));
    const QString untracked = child(fixture.root, QStringLiteral("untracked.txt"));
    QCOMPARE(provider.requestOverlays(Alice, {untracked}, 2).acceptedPaths.size(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(untracked, State::Added), WaitMilliseconds);
    QVERIFY(!alice.revisionViolation);
}

void OverlayStatusProviderTest::releaseStopsPublishingAndPolling()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    OverlayProviderLimits limits = fastLimits();
    limits.pollInterval = 50ms;
    OverlayStatusProvider provider(limits);
    Recorder alice(provider, Alice);
    const QString file = child(fixture.root, QStringLiteral("clean.txt"));

    QCOMPARE(provider.requestOverlays(Alice, {file}, 1).acceptedPaths.size(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Clean), WaitMilliseconds);

    provider.releaseOverlays(Alice, {file});
    QCOMPARE(provider.observationCount(), 0);
    QTest::qWait(300); // let an in-flight refresh settle
    const int batches = alice.changedBatches;
    QVERIFY(GitTestSupport::writeTextFile(file, "after release\n"));
    QTest::qWait(400);
    QCOMPARE(alice.changedBatches, batches);

    // Releasing an owner drops everything it observed, including after a bus disconnect.
    QCOMPARE(provider.requestOverlays(Alice, {file}, 2).acceptedPaths.size(), 1);
    QCOMPARE(provider.observationCount(), 1);
    provider.releaseOwner(Alice);
    QCOMPARE(provider.observationCount(), 0);
    provider.releaseOwner(Alice); // idempotent
}

void OverlayStatusProviderTest::leasesExpireAndRenewalExtendsThem()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    OverlayProviderLimits limits = fastLimits();
    auto fakeNow = OverlayStatusProvider::Clock::now();
    limits.clock = [&fakeNow] { return fakeNow; };
    OverlayStatusProvider provider(limits);
    Recorder alice(provider, Alice);
    const QString kept = child(fixture.root, QStringLiteral("clean.txt"));
    const QString lapsing = child(fixture.root, QStringLiteral("modified.txt"));

    QCOMPARE(provider.requestOverlays(Alice, {kept, lapsing}, 1).acceptedPaths.size(), 2);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(kept, State::Clean), WaitMilliseconds);

    fakeNow += 60s;
    QCOMPARE(provider.requestOverlays(Alice, {kept}, 2).acceptedPaths.size(), 1); // renewal
    fakeNow += 40s; // 100 s after the first request, 40 s after the renewal
    provider.expireLeases();
    QCOMPARE(provider.observationCount(), 1);
    fakeNow += 60s;
    provider.expireLeases();
    QCOMPARE(provider.observationCount(), 0);
}

void OverlayStatusProviderTest::staleSnapshotYieldsUnknownThenRefreshes()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    OverlayProviderLimits limits = fastLimits();
    auto fakeNow = OverlayStatusProvider::Clock::now();
    limits.clock = [&fakeNow] { return fakeNow; };
    OverlayStatusProvider provider(limits);
    Recorder alice(provider, Alice);
    const QString file = child(fixture.root, QStringLiteral("clean.txt"));

    QCOMPARE(provider.requestOverlays(Alice, {file}, 1).acceptedPaths.size(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Clean), WaitMilliseconds);
    // Remaining freshness shrinks with the clock and never restarts a full lifetime.
    fakeNow += 10s;
    QCOMPARE(provider.requestOverlays(Alice, {file}, 2).acceptedPaths.size(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(alice.initialRequests.size(), 2, WaitMilliseconds);
    const OverlayRecord aged = alice.initialRequests.at(1).second.constFirst();
    QCOMPARE(aged.validity, Validity::Ready);
    QVERIFY(aged.validForMs <= 20000);
    QVERIFY(aged.validForMs > 15000);

    fakeNow += 25s; // past the 30 s freshness: no evidence, so no state
    QCOMPARE(provider.requestOverlays(Alice, {file}, 3).acceptedPaths.size(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(alice.initialRequests.size(), 3, WaitMilliseconds);
    const OverlayRecord stale = alice.initialRequests.at(2).second.constFirst();
    QCOMPARE(stale.state, State::Unknown);
    QCOMPARE(stale.validity, Validity::Stale);
    QCOMPARE(stale.validForMs, quint32(0));

    // The request also started a refresh that renews the answer.
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Clean), WaitMilliseconds);
}

void OverlayStatusProviderTest::nestedRepositoryOwnsItsContents()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    const QString inner = child(fixture.root, QStringLiteral("inner"));
    QVERIFY(GitTestSupport::createRepository(inner));
    QVERIFY(GitTestSupport::writeTextFile(child(inner, QStringLiteral("file.txt")), "dirty\n"));

    OverlayStatusProvider provider(fastLimits());
    Recorder alice(provider, Alice);
    const QString outerFile = child(fixture.root, QStringLiteral("clean.txt"));
    const QString innerFile = child(inner, QStringLiteral("file.txt"));

    // Asked together, the inner path first attaches to the outer repository because its nested
    // roots are not known yet. It must be moved to its own repository once they are.
    QCOMPARE(provider.requestOverlays(Alice, {outerFile, innerFile, inner}, 1).acceptedPaths.size(),
             3);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(outerFile, State::Clean), WaitMilliseconds);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(innerFile, State::Modified), WaitMilliseconds);
    QCOMPARE(alice.record(innerFile).repositoryRoot, inner);
    QCOMPARE(alice.record(outerFile).repositoryRoot, fixture.root);
    // The nested root directory is an untracked entry of the outer repository.
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(inner, State::Untracked), WaitMilliseconds);
    QCOMPARE(alice.record(inner).repositoryRoot, fixture.root);
    QCOMPARE(provider.repositoryCount(), 2);
    QVERIFY(!alice.revisionViolation);
}

void OverlayStatusProviderTest::outputOverflowDegradesWithoutClaimingClean()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    OverlayProviderLimits limits = fastLimits();
    limits.maxOutputBytes = 10;
    OverlayStatusProvider provider(limits);
    Recorder alice(provider, Alice);
    const QString file = child(fixture.root, QStringLiteral("clean.txt"));

    QCOMPARE(provider.requestOverlays(Alice, {file}, 1).acceptedPaths.size(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Unknown, Validity::Degraded), WaitMilliseconds);
    QCOMPARE(alice.record(file).validForMs, quint32(0));
}

void OverlayStatusProviderTest::gitFailureDegradesAndRecovers()
{
    Fixture fixture;
    QVERIFY(createFixture(fixture));
    OverlayProviderLimits limits = fastLimits();
    limits.pollInterval = 100ms;
    limits.backoffShiftLimit = 0;
    OverlayStatusProvider provider(limits);
    Recorder alice(provider, Alice);
    const QString file = child(fixture.root, QStringLiteral("clean.txt"));

    QCOMPARE(provider.requestOverlays(Alice, {file}, 1).acceptedPaths.size(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Clean), WaitMilliseconds);

    // Corrupting HEAD makes the next collection fail; the earlier Clean must not survive.
    const QString head = child(fixture.root, QStringLiteral(".git/HEAD"));
    QFile headFile(head);
    QVERIFY(headFile.open(QIODevice::ReadOnly));
    const QByteArray original = headFile.readAll();
    headFile.close();
    QVERIFY(GitTestSupport::writeTextFile(head, "garbage\n"));
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Unknown, Validity::Degraded), WaitMilliseconds);

    QVERIFY(GitTestSupport::writeTextFile(head, original));
    QTRY_VERIFY_WITH_TIMEOUT(alice.is(file, State::Clean), WaitMilliseconds);
    QVERIFY(!alice.revisionViolation);
}

QTEST_GUILESS_MAIN(OverlayStatusProviderTest)
#include "OverlayStatusProviderTest.moc"
