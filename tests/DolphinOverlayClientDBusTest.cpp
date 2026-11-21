// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

// Runs on the private bus of dbus-run-session. XDG_DATA_DIRS points the bus at generated activation
// files for the built linuxgitshell-daemon, so requests exercise real D-Bus activation.

#include "DBusTestSupport.h"
#include "GitTestSupport.h"
#include "OverlayClient.h"

#include "linuxgitshell/repositorycontext/OverlayWire.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDir>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <chrono>
#include <memory>

using namespace LinuxGitShell;
using namespace LinuxGitShell::Dolphin;
using namespace std::chrono_literals;

namespace
{

constexpr int WaitMilliseconds = 20000;

[[nodiscard]] bool isRegistered()
{
    return QDBusConnection::sessionBus()
        .interface()
        ->isServiceRegistered(OverlayServiceName)
        .value();
}

[[nodiscard]] std::unique_ptr<OverlayClient>
makeClient(const QString& serviceName = OverlayServiceName,
           std::chrono::milliseconds backoff = 100ms)
{
    OverlayClientLimits limits;
    limits.minBackoff = backoff;
    return std::make_unique<OverlayClient>(
        std::make_unique<DBusOverlayTransport>(serviceName, nullptr), limits);
}

[[nodiscard]] QStringList icon(const QString& name)
{
    return {QStringLiteral("emblem-linuxgitshell-") + name};
}

} // namespace

class DolphinOverlayClientDBusTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void activatesServiceAndShowsRealStates();
    void ignoresSignalsFromOtherSenders();
    void serviceLossClearsIconsAndRecoversWithNewGeneration();
    void missingServiceNeverBlocks();
    void disablingReleasesObservations();
};

void DolphinOverlayClientDBusTest::initTestCase()
{
    GitTestSupport::isolateGitConfiguration();
    qDBusRegisterMetaType<QList<QVariantMap>>();
    QVERIFY2(QDBusConnection::sessionBus().isConnected(), "run this test through dbus-run-session");
}

void DolphinOverlayClientDBusTest::init() { QVERIFY(DBusTestSupport::stopContextService()); }

void DolphinOverlayClientDBusTest::cleanup() { QVERIFY(DBusTestSupport::stopContextService()); }

void DolphinOverlayClientDBusTest::activatesServiceAndShowsRealStates()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = QDir(directory.path()).canonicalPath() + QStringLiteral("/repo");
    QVERIFY(GitTestSupport::createRepository(root));
    QVERIFY(GitTestSupport::writeTextFile(root + QStringLiteral("/untracked.txt"), "u\n"));
    QVERIFY(GitTestSupport::writeTextFile(root + QStringLiteral("/file.txt"), "changed\n"));
    QVERIFY(GitTestSupport::writeTextFile(root + QStringLiteral("/.gitignore"), "*.log\n"));
    QVERIFY(GitTestSupport::runGit(root, {QStringLiteral("add"), QStringLiteral(".gitignore")}));
    QVERIFY(GitTestSupport::runGit(
        root, {QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("ignore")}));

    QVERIFY(!isRegistered());
    auto client = makeClient();
    QSignalSpy changes(client.get(), &OverlayClient::iconsChanged);

    const QString modified = root + QStringLiteral("/file.txt");
    const QString untracked = root + QStringLiteral("/untracked.txt");
    const QString clean = root + QStringLiteral("/.gitignore");
    QElapsedTimer timer;
    timer.start();
    for (const QString& path : {modified, untracked, clean, root})
    {
        QCOMPARE(client->lookup(path), QStringList());
    }
    // Asking never reaches the bus from the callback: the service is not even started yet.
    QVERIFY(timer.elapsed() < 500);
    QVERIFY(!isRegistered());

    // The request leaves from the event loop and activates the service by name.
    QTRY_COMPARE_WITH_TIMEOUT(client->lookup(modified), icon(QStringLiteral("modified")),
                              WaitMilliseconds);
    QTRY_COMPARE_WITH_TIMEOUT(client->lookup(untracked), icon(QStringLiteral("untracked")),
                              WaitMilliseconds);
    QTRY_COMPARE_WITH_TIMEOUT(client->lookup(clean), icon(QStringLiteral("clean")),
                              WaitMilliseconds);
    QTRY_COMPARE_WITH_TIMEOUT(client->lookup(root), icon(QStringLiteral("modified")),
                              WaitMilliseconds);
    QVERIFY(isRegistered());
    QVERIFY(client->generation() != 0);
    QVERIFY(changes.size() >= 4);
}

void DolphinOverlayClientDBusTest::ignoresSignalsFromOtherSenders()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = QDir(directory.path()).canonicalPath() + QStringLiteral("/repo");
    QVERIFY(GitTestSupport::createRepository(root));
    const QString file = root + QStringLiteral("/file.txt");

    auto client = makeClient();
    QCOMPARE(client->lookup(file), QStringList());
    QTRY_COMPARE_WITH_TIMEOUT(client->lookup(file), icon(QStringLiteral("clean")),
                              WaitMilliseconds);
    const quint64 generation = client->generation();

    // Another process on the session bus targets this client with a believable lie.
    QDBusConnection spoofer = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                            QStringLiteral("overlay-spoofer"));
    QVERIFY(spoofer.isConnected());
    OverlayRecord lie;
    lie.path = file;
    lie.repositoryRoot = root;
    lie.state = RepositoryStatusState::Conflict;
    lie.validity = RepositoryStatusValidity::Ready;
    lie.revision = 1'000'000;
    lie.validForMs = 30000;
    QDBusMessage forged = QDBusMessage::createTargetedSignal(
        QDBusConnection::sessionBus().baseService(), QString(OverlayObjectPath),
        QString(OverlayInterfaceName), QStringLiteral("OverlaysChanged"));
    forged.setArguments({QVariant::fromValue(QList<QVariantMap>{overlayRecordToVariantMap(lie)}),
                         QVariant::fromValue<qulonglong>(generation)});
    QVERIFY(spoofer.send(forged));

    QDBusMessage forgedInitial = QDBusMessage::createTargetedSignal(
        QDBusConnection::sessionBus().baseService(), QString(OverlayObjectPath),
        QString(OverlayInterfaceName), QStringLiteral("OverlayResults"));
    forgedInitial.setArguments(
        {QVariant::fromValue<qulonglong>(1),
         QVariant::fromValue(QList<QVariantMap>{overlayRecordToVariantMap(lie)}),
         QVariant::fromValue<qulonglong>(generation)});
    QVERIFY(spoofer.send(forgedInitial));

    QSignalSpy changes(client.get(), &OverlayClient::iconsChanged);
    QTest::qWait(500);
    QCOMPARE(changes.size(), 0);
    QCOMPARE(client->lookup(file), icon(QStringLiteral("clean")));
    spoofer = QDBusConnection(QString());
    QDBusConnection::disconnectFromBus(QStringLiteral("overlay-spoofer"));
}

void DolphinOverlayClientDBusTest::serviceLossClearsIconsAndRecoversWithNewGeneration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = QDir(directory.path()).canonicalPath() + QStringLiteral("/repo");
    QVERIFY(GitTestSupport::createRepository(root));
    const QString file = root + QStringLiteral("/file.txt");

    auto client = makeClient();
    QSignalSpy changes(client.get(), &OverlayClient::iconsChanged);
    QCOMPARE(client->lookup(file), QStringList());
    QTRY_COMPARE_WITH_TIMEOUT(client->lookup(file), icon(QStringLiteral("clean")),
                              WaitMilliseconds);
    const quint64 firstGeneration = client->generation();
    const auto before = changes.size();

    // The daemon disappears: the authenticated owner is gone, so nothing cached can be trusted.
    // Replace the process outright so that the next owner is a different one.
    QVERIFY(DBusTestSupport::stopContextService());
    QTRY_VERIFY_WITH_TIMEOUT(changes.size() > before, WaitMilliseconds);
    QCOMPARE(changes.constLast().at(0).toString(), file);
    QCOMPARE(changes.constLast().at(1).toStringList(), QStringList());

    // The retained key is requested again from the new service instance, which numbers its
    // revisions afresh under a new generation.
    QTRY_COMPARE_WITH_TIMEOUT(client->lookup(file), icon(QStringLiteral("clean")),
                              WaitMilliseconds);
    QVERIFY(client->generation() != 0);
    QVERIFY(client->generation() != firstGeneration);
}

void DolphinOverlayClientDBusTest::missingServiceNeverBlocks()
{
    // A well-known name nobody owns and no activation file provides.
    auto client =
        makeClient(QStringLiteral("org.linuxgitshell.Experimental.OverlaysMissing"), 50ms);
    int ticks = 0;
    QTimer ticker;
    connect(&ticker, &QTimer::timeout, this, [&ticks] { ++ticks; });
    ticker.start(10);

    QElapsedTimer timer;
    timer.start();
    for (int index = 0; index < 200; ++index)
    {
        QCOMPARE(client->lookup(QStringLiteral("/repo/file%1").arg(index)), QStringList());
    }
    QVERIFY(timer.elapsed() < 500);

    // The event loop keeps running while requests fail and are retried with backoff.
    QTest::qWait(600);
    QVERIFY(ticks >= 20);
    QCOMPARE(client->lookup(QStringLiteral("/repo/file0")), QStringList());
    QVERIFY(!isRegistered());
}

void DolphinOverlayClientDBusTest::disablingReleasesObservations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = QDir(directory.path()).canonicalPath() + QStringLiteral("/repo");
    QVERIFY(GitTestSupport::createRepository(root));
    const QString file = root + QStringLiteral("/file.txt");

    auto client = makeClient();
    QSignalSpy changes(client.get(), &OverlayClient::iconsChanged);
    QCOMPARE(client->lookup(file), QStringList());
    QTRY_COMPARE_WITH_TIMEOUT(client->lookup(file), icon(QStringLiteral("clean")),
                              WaitMilliseconds);

    client->setEnabled(false);
    QCOMPARE(changes.constLast().at(1).toStringList(), QStringList());
    QCOMPARE(client->keyCount(), 0);
    // Nothing is asked for afterwards, and a late reply cannot bring an icon back.
    QCOMPARE(client->lookup(file), QStringList());
    QTest::qWait(300);
    QCOMPARE(client->keyCount(), 0);
    QCOMPARE(client->lookup(file), QStringList());
}

QTEST_GUILESS_MAIN(DolphinOverlayClientDBusTest)
#include "DolphinOverlayClientDBusTest.moc"
