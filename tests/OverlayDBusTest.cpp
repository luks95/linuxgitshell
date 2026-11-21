// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

// Runs the real linuxgitshell-daemon on the private session bus created by dbus-run-session and
// talks to it through a proxy generated from dbus/org.linuxgitshell.Experimental.Overlays1.xml.

#include "DBusTestSupport.h"
#include "GitTestSupport.h"
#include "overlayinterface.h"

#include "linuxgitshell/repositorycontext/OverlayWire.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusPendingReply>
#include <QDir>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <functional>
#include <memory>

using namespace LinuxGitShell;
using OverlayInterface = OrgLinuxgitshellExperimentalOverlays1Interface;

namespace
{

constexpr int WaitMilliseconds = 15000;

[[nodiscard]] bool isRegistered(QLatin1StringView name)
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(name).value();
}

[[nodiscard]] bool startDaemon(std::unique_ptr<QProcess>& daemon)
{
    daemon = std::make_unique<QProcess>();
    daemon->setProgram(QStringLiteral(DAEMON_EXECUTABLE_PATH));
    // Debug logging records counts and events, never paths; tests read it to observe releases.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_LOGGING_RULES"),
                       QStringLiteral("linuxgitshell.daemon.debug=true"));
    environment.insert(QStringLiteral("QT_FORCE_STDERR_LOGGING"), QStringLiteral("1"));
    daemon->setProcessEnvironment(environment);
    daemon->setProcessChannelMode(QProcess::SeparateChannels);
    daemon->start();
    return daemon->waitForStarted(5000) &&
           QTest::qWaitFor([] { return isRegistered(OverlayServiceName); }, 5000);
}

[[nodiscard]] bool stopDaemon(std::unique_ptr<QProcess>& daemon)
{
    if (daemon == nullptr)
    {
        return true;
    }
    daemon->terminate();
    const bool stopped = daemon->waitForFinished(5000);
    daemon.reset();
    return QTest::qWaitFor([] { return !isRegistered(OverlayServiceName); }, 5000) && stopped;
}

[[nodiscard]] QList<OverlayRecord> decode(const QList<QVariantMap>& maps)
{
    QList<OverlayRecord> records;
    for (const QVariantMap& map : maps)
    {
        const auto record = overlayRecordFromVariantMap(map);
        if (record.has_value())
        {
            records.append(*record);
        }
    }
    return records;
}

// Waits until some batch delivered through `spy` satisfies `matches` for one of its records.
[[nodiscard]] std::optional<OverlayRecord>
waitForRecord(QSignalSpy& spy, int recordsArgument,
              const std::function<bool(const OverlayRecord&)>& matches)
{
    std::optional<OverlayRecord> found;
    const auto scan = [&]
    {
        for (const QList<QVariant>& arguments : std::as_const(spy))
        {
            for (const OverlayRecord& record :
                 decode(qvariant_cast<QList<QVariantMap>>(arguments.at(recordsArgument))))
            {
                if (matches(record))
                {
                    found = record;
                    return true;
                }
            }
        }
        return false;
    };
    if (!scan())
    {
        (void)QTest::qWaitFor(scan, WaitMilliseconds);
    }
    return found;
}

} // namespace

class OverlayDBusTest final : public QObject
{
    Q_OBJECT

    std::unique_ptr<QProcess> daemon;

  private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void deliversDirectedResultsAfterTheReply();
    void keepsClientsIsolated();
    void rejectsInvalidRequestsWithNamedError();
    void refreshRepositoryRequiresObservation();
    void releasesObservationsWhenAClientDisconnects();
    void serviceNamesAreBothOwnedByOneDaemon();
    void activatesOnFirstRequest();
};

void OverlayDBusTest::initTestCase()
{
    GitTestSupport::isolateGitConfiguration();
    qDBusRegisterMetaType<QList<QVariantMap>>();
    QVERIFY2(QDBusConnection::sessionBus().isConnected(), "run this test through dbus-run-session");
    QVERIFY(!isRegistered(OverlayServiceName));
}

void OverlayDBusTest::init()
{
    // Activation must start from a bus with no daemon.
    if (QLatin1StringView(QTest::currentTestFunction()) !=
        QLatin1StringView("activatesOnFirstRequest"))
    {
        QVERIFY(startDaemon(daemon));
    }
}

void OverlayDBusTest::cleanup()
{
    QVERIFY(stopDaemon(daemon));
    QVERIFY(DBusTestSupport::stopContextService());
    QVERIFY(QTest::qWaitFor([] { return !isRegistered(OverlayServiceName); }, 5000));
}

void OverlayDBusTest::deliversDirectedResultsAfterTheReply()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = QDir(directory.path()).canonicalPath() + QStringLiteral("/repo");
    QVERIFY(GitTestSupport::createRepository(root));
    const QString file = root + QStringLiteral("/file.txt");
    QVERIFY(GitTestSupport::writeTextFile(file, "changed\n"));

    OverlayInterface overlays(QString(OverlayServiceName), QString(OverlayObjectPath),
                              QDBusConnection::sessionBus());
    QVERIFY(overlays.isValid());
    QSignalSpy results(&overlays, &OverlayInterface::OverlayResults);
    QSignalSpy changed(&overlays, &OverlayInterface::OverlaysChanged);

    QDBusPendingReply<qulonglong, QStringList> reply =
        overlays.RequestOverlays({file, QStringLiteral("relative/path")}, 7);
    reply.waitForFinished();
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
    const quint64 generation = reply.argumentAt<0>();
    QVERIFY(generation != 0);
    QCOMPARE(reply.argumentAt<1>(), QStringList{file});

    // Exactly one initial record per accepted path, then the collected state.
    QVERIFY(results.wait(WaitMilliseconds));
    QCOMPARE(results.size(), 1);
    const QList<QVariant> initial = results.constFirst();
    QCOMPARE(initial.at(0).toULongLong(), quint64(7));
    QCOMPARE(initial.at(2).toULongLong(), generation);
    const QList<OverlayRecord> initialRecords =
        decode(qvariant_cast<QList<QVariantMap>>(initial.at(1)));
    QCOMPARE(initialRecords.size(), 1);
    QCOMPARE(initialRecords.constFirst().path, file);
    QCOMPARE(initialRecords.constFirst().validity, RepositoryStatusValidity::Pending);
    QCOMPARE(initialRecords.constFirst().state, RepositoryStatusState::Unknown);

    const auto ready = waitForRecord(
        changed, 0, [&file](const OverlayRecord& record)
        { return record.path == file && record.validity == RepositoryStatusValidity::Ready; });
    QVERIFY(ready.has_value());
    QCOMPARE(ready->state, RepositoryStatusState::Modified);
    QCOMPARE(ready->repositoryRoot, root);
    QVERIFY(ready->validForMs > 0);
    QVERIFY(ready->revision > initialRecords.constFirst().revision);
    QCOMPARE(changed.constFirst().at(1).toULongLong(), generation);
}

void OverlayDBusTest::keepsClientsIsolated()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = QDir(directory.path()).canonicalPath() + QStringLiteral("/repo");
    QVERIFY(GitTestSupport::createRepository(root));
    const QString file = root + QStringLiteral("/file.txt");

    QDBusConnection second = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                           QStringLiteral("overlay-second"));
    QVERIFY(second.isConnected());
    {
        OverlayInterface first(QString(OverlayServiceName), QString(OverlayObjectPath),
                               QDBusConnection::sessionBus());
        OverlayInterface other(QString(OverlayServiceName), QString(OverlayObjectPath), second);
        QSignalSpy firstResults(&first, &OverlayInterface::OverlayResults);
        QSignalSpy firstChanged(&first, &OverlayInterface::OverlaysChanged);
        QSignalSpy otherResults(&other, &OverlayInterface::OverlayResults);
        QSignalSpy otherChanged(&other, &OverlayInterface::OverlaysChanged);

        QDBusPendingReply<qulonglong, QStringList> reply = other.RequestOverlays({file}, 1);
        reply.waitForFinished();
        QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
        QVERIFY(otherResults.wait(WaitMilliseconds));
        QVERIFY(waitForRecord(otherChanged, 0, [](const OverlayRecord& record)
                              { return record.validity == RepositoryStatusValidity::Ready; })
                    .has_value());

        // Notifications are unicast: the first client never observed this path.
        QTest::qWait(300);
        QCOMPARE(firstResults.size(), 0);
        QCOMPARE(firstChanged.size(), 0);
    }
    QDBusConnection::disconnectFromBus(QStringLiteral("overlay-second"));
}

void OverlayDBusTest::rejectsInvalidRequestsWithNamedError()
{
    OverlayInterface overlays(QString(OverlayServiceName), QString(OverlayObjectPath),
                              QDBusConnection::sessionBus());
    QDBusPendingReply<qulonglong, QStringList> zeroId = overlays.RequestOverlays({}, 0);
    zeroId.waitForFinished();
    QVERIFY(zeroId.isError());
    QCOMPARE(zeroId.error().name(), QString(OverlayInvalidRequestError));

    QStringList tooMany;
    for (int index = 0; index < 257; ++index)
    {
        tooMany.append(QStringLiteral("/proc/%1").arg(index));
    }
    QDBusPendingReply<qulonglong, QStringList> excessive = overlays.RequestOverlays(tooMany, 1);
    excessive.waitForFinished();
    QVERIFY(excessive.isError());
    QCOMPARE(excessive.error().name(), QString(OverlayInvalidRequestError));

    // Partial acceptance is explicit and does not wait for Git.
    QSignalSpy results(&overlays, &OverlayInterface::OverlayResults);
    QDBusPendingReply<qulonglong, QStringList> partial =
        overlays.RequestOverlays({QStringLiteral("/proc/self"), QStringLiteral("relative")}, 2);
    partial.waitForFinished();
    QVERIFY(partial.isValid());
    QCOMPARE(partial.argumentAt<1>(), QStringList{QStringLiteral("/proc/self")});
    QVERIFY(results.wait(WaitMilliseconds));
    const auto records = decode(qvariant_cast<QList<QVariantMap>>(results.constFirst().at(1)));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.constFirst().validity, RepositoryStatusValidity::Unavailable);
}

void OverlayDBusTest::refreshRepositoryRequiresObservation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = QDir(directory.path()).canonicalPath() + QStringLiteral("/repo");
    QVERIFY(GitTestSupport::createRepository(root));
    const QString file = root + QStringLiteral("/file.txt");

    OverlayInterface overlays(QString(OverlayServiceName), QString(OverlayObjectPath),
                              QDBusConnection::sessionBus());
    QSignalSpy changed(&overlays, &OverlayInterface::OverlaysChanged);

    QDBusPendingReply<bool> unobserved = overlays.RefreshRepository(root);
    unobserved.waitForFinished();
    QVERIFY(unobserved.isValid());
    QVERIFY(!unobserved.value());

    QDBusPendingReply<qulonglong, QStringList> reply = overlays.RequestOverlays({file}, 1);
    reply.waitForFinished();
    QVERIFY(reply.isValid());
    QVERIFY(waitForRecord(changed, 0,
                          [](const OverlayRecord& record)
                          {
                              return record.state == RepositoryStatusState::Clean &&
                                     record.validity == RepositoryStatusValidity::Ready;
                          })
                .has_value());

    QVERIFY(GitTestSupport::writeTextFile(file, "edited\n"));
    QDBusPendingReply<bool> observed = overlays.RefreshRepository(root);
    observed.waitForFinished();
    QVERIFY(observed.isValid());
    QVERIFY(observed.value());
    QVERIFY(waitForRecord(changed, 0, [](const OverlayRecord& record)
                          { return record.state == RepositoryStatusState::Modified; })
                .has_value());
}

void OverlayDBusTest::releasesObservationsWhenAClientDisconnects()
{
    QByteArray log;
    connect(daemon.get(), &QProcess::readyReadStandardError, this,
            [this, &log] { log.append(daemon->readAllStandardError()); });

    {
        // The connection closes only when every QDBusConnection copy is gone, so keep them scoped.
        QDBusConnection transient = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, QStringLiteral("overlay-transient"));
        QVERIFY(transient.isConnected());
        OverlayInterface overlays(QString(OverlayServiceName), QString(OverlayObjectPath),
                                  transient);
        QDBusPendingReply<qulonglong, QStringList> reply =
            overlays.RequestOverlays({QStringLiteral("/proc/self")}, 1);
        reply.waitForFinished();
        QVERIFY(reply.isValid());
        QCOMPARE(reply.argumentAt<1>().size(), 1);
        QVERIFY(
            QTest::qWaitFor([&log] { return log.contains("accepted 1 observations 1"); }, 5000));
        QVERIFY(!log.contains("client disconnected"));
    }
    QDBusConnection::disconnectFromBus(QStringLiteral("overlay-transient"));
    QVERIFY(QTest::qWaitFor([&log] { return log.contains("Overlay client disconnected"); }, 5000));
    daemon->disconnect(this);
}

void OverlayDBusTest::serviceNamesAreBothOwnedByOneDaemon()
{
    QVERIFY(isRegistered(OverlayServiceName));
    QVERIFY(isRegistered(QLatin1StringView("org.linuxgitshell.Experimental.Context1")));
    const QDBusConnectionInterface* bus = QDBusConnection::sessionBus().interface();
    QCOMPARE(
        bus->serviceOwner(OverlayServiceName).value(),
        bus->serviceOwner(QLatin1StringView("org.linuxgitshell.Experimental.Context1")).value());

    QProcess second;
    second.setProgram(QStringLiteral(DAEMON_EXECUTABLE_PATH));
    second.start();
    QVERIFY(second.waitForFinished(5000));
    QCOMPARE(second.exitCode(), 1);
    QCOMPARE(daemon->state(), QProcess::Running);
}

void OverlayDBusTest::activatesOnFirstRequest()
{
    QVERIFY(!isRegistered(OverlayServiceName));
    OverlayInterface overlays(QString(OverlayServiceName), QString(OverlayObjectPath),
                              QDBusConnection::sessionBus());
    QDBusPendingReply<qulonglong, QStringList> reply = overlays.RequestOverlays({}, 1);
    reply.waitForFinished();
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));
    QVERIFY(reply.argumentAt<0>() != 0);
    QVERIFY(isRegistered(OverlayServiceName));
}

QTEST_GUILESS_MAIN(OverlayDBusTest)
#include "OverlayDBusTest.moc"
