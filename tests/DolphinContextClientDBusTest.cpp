// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

// Runs on the private bus of dbus-run-session. XDG_DATA_DIRS points the bus at a generated
// activation file for the built linuxgitshell-daemon, so requests exercise real D-Bus activation.

#include "DBusTestSupport.h"
#include "GitTestSupport.h"
#include "RepositoryContextClient.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using LinuxGitShell::RepositoryContextFreshness;
using LinuxGitShell::repositoryContextPathKey;
using LinuxGitShell::RepositoryContextSnapshot;
using LinuxGitShell::RepositoryContextState;
using LinuxGitShell::Dolphin::DBusRepositoryContextTransport;
using LinuxGitShell::Dolphin::RepositoryContextClient;
using LinuxGitShell::Dolphin::RepositoryContextClientLimits;

namespace
{

[[nodiscard]] std::unique_ptr<RepositoryContextClient>
makeClient(const QString& serviceName = LinuxGitShell::RepositoryContextServiceName)
{
    return std::make_unique<RepositoryContextClient>(
        std::make_unique<DBusRepositoryContextTransport>(serviceName, nullptr),
        RepositoryContextClientLimits{});
}

[[nodiscard]] bool waitForContext(RepositoryContextClient& client, const QString& pathKey)
{
    return QTest::qWaitFor([&client, &pathKey]
                           { return client.lookup(pathKey).snapshot.has_value(); }, 15000);
}

} // namespace

class DolphinContextClientDBusTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanupTestCase();
    void activatesServiceAndWarmsCache();
    void serviceRestartDropsSnapshots();
    void missingServiceFailsWithoutBlocking();
};

void DolphinContextClientDBusTest::initTestCase()
{
    GitTestSupport::isolateGitConfiguration();
    QVERIFY2(QDBusConnection::sessionBus().isConnected(), "run this test through dbus-run-session");
    QVERIFY(QDBusConnection::sessionBus().interface()->activatableServiceNames().value().contains(
        LinuxGitShell::RepositoryContextServiceName));
}

void DolphinContextClientDBusTest::init() { QVERIFY(DBusTestSupport::stopContextService()); }

void DolphinContextClientDBusTest::cleanupTestCase()
{
    QVERIFY(DBusTestSupport::stopContextService());
}

void DolphinContextClientDBusTest::activatesServiceAndWarmsCache()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("repo"));
    QVERIFY(GitTestSupport::createRepository(root));
    const QString rootKey = repositoryContextPathKey(root);
    const QString outsideKey = repositoryContextPathKey(directory.path());

    const auto client = makeClient();
    QVERIFY(!DBusTestSupport::isContextServiceRegistered());
    client->refreshLater({rootKey, outsideKey});

    QVERIFY(waitForContext(*client, rootKey));
    QVERIFY(waitForContext(*client, outsideKey));
    QVERIFY(DBusTestSupport::isContextServiceRegistered());
    QVERIFY(client->generation() != 0);
    QCOMPARE(client->inFlightCount(), 0);

    const RepositoryContextSnapshot inside =
        client->lookup(rootKey).snapshot.value_or(RepositoryContextSnapshot{});
    QCOMPARE(inside.state, RepositoryContextState::InsideRepository);
    QCOMPARE(inside.repositoryRoot, QFileInfo(root).canonicalFilePath());
    QVERIFY(inside.isRepositoryRoot);
    QCOMPARE(client->lookup(outsideKey).snapshot.value_or(RepositoryContextSnapshot{}).state,
             RepositoryContextState::OutsideRepository);
}

void DolphinContextClientDBusTest::serviceRestartDropsSnapshots()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString first = repositoryContextPathKey(directory.filePath(QStringLiteral("first")));
    const QString second = repositoryContextPathKey(directory.filePath(QStringLiteral("second")));
    QVERIFY(QDir().mkpath(first));
    QVERIFY(QDir().mkpath(second));

    const auto client = makeClient();
    client->refreshLater({first});
    QVERIFY(waitForContext(*client, first));
    const quint64 firstGeneration = client->generation();

    QVERIFY(DBusTestSupport::stopContextService());
    client->refreshLater({second});
    QVERIFY(waitForContext(*client, second));

    QVERIFY(client->generation() != firstGeneration);
    QCOMPARE(client->lookup(first).freshness, RepositoryContextFreshness::Cold);
}

void DolphinContextClientDBusTest::missingServiceFailsWithoutBlocking()
{
    const auto client = makeClient(QStringLiteral("org.linuxgitshell.Test.Missing"));
    QSignalSpy unavailable(client.get(), &RepositoryContextClient::contextUnavailable);
    const QString pathKey = QStringLiteral("/tmp");

    QElapsedTimer elapsed;
    elapsed.start();
    client->refreshLater({pathKey});
    QVERIFY(elapsed.elapsed() < 100);

    QVERIFY(unavailable.wait(5000));
    QCOMPARE(unavailable.constFirst().at(0).toStringList(), QStringList{pathKey});
    QCOMPARE(client->inFlightCount(), 0);
    QCOMPARE(client->lookup(pathKey).freshness, RepositoryContextFreshness::Cold);
}

QTEST_GUILESS_MAIN(DolphinContextClientDBusTest)

#include "DolphinContextClientDBusTest.moc"
