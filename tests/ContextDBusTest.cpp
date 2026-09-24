// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

// Runs the real linuxgitshell-daemon on the private session bus created by dbus-run-session and
// talks to it through a proxy generated from dbus/org.linuxgitshell.Experimental.Context1.xml.

#include "GitTestSupport.h"
#include "contextinterface.h"

#include "linuxgitshell/repositorycontext/RepositoryContextWire.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingReply>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using LinuxGitShell::repositoryContextFromVariantMap;
using LinuxGitShell::RepositoryContextSnapshot;
using LinuxGitShell::RepositoryContextState;
using LinuxGitShell::RepositoryContextType;

namespace
{

constexpr QLatin1StringView ServiceName("org.linuxgitshell.Experimental.Context1");
constexpr QLatin1StringView ObjectPath("/org/linuxgitshell/Context");

using ContextInterface = OrgLinuxgitshellExperimentalContext1Interface;

[[nodiscard]] bool isServiceRegistered()
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(ServiceName).value();
}

[[nodiscard]] quint64 requestContext(ContextInterface& context, const QStringList& paths)
{
    QDBusPendingReply<qulonglong> reply = context.RequestContext(paths);
    reply.waitForFinished();
    return reply.isValid() ? reply.value() : 0;
}

[[nodiscard]] bool startDaemon(std::unique_ptr<QProcess>& daemon)
{
    daemon = std::make_unique<QProcess>();
    daemon->setProgram(QStringLiteral(DAEMON_EXECUTABLE_PATH));
    daemon->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    daemon->start();
    return daemon->waitForStarted(5000) &&
           QTest::qWaitFor([] { return isServiceRegistered(); }, 5000);
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
    return QTest::qWaitFor([] { return !isServiceRegistered(); }, 5000) && stopped;
}

} // namespace

class ContextDBusTest final : public QObject
{
    Q_OBJECT

    std::unique_ptr<QProcess> daemon;

  private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void resolvesRepositoryOverSessionBus();
    void rejectsSecondInstance();
    void restartChangesGeneration();
};

void ContextDBusTest::initTestCase()
{
    GitTestSupport::isolateGitConfiguration();
    QVERIFY2(QDBusConnection::sessionBus().isConnected(), "run this test through dbus-run-session");
    QVERIFY(!isServiceRegistered());
}

void ContextDBusTest::init() { QVERIFY(startDaemon(daemon)); }

void ContextDBusTest::cleanup() { QVERIFY(stopDaemon(daemon)); }

void ContextDBusTest::resolvesRepositoryOverSessionBus()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("repo"));
    QVERIFY(GitTestSupport::createRepository(root));

    ContextInterface context(ServiceName, ObjectPath, QDBusConnection::sessionBus());
    QVERIFY(context.isValid());
    QSignalSpy spy(&context, &ContextInterface::ContextReady);

    const quint64 generation = requestContext(context, {root, QStringLiteral("relative")});
    QVERIFY(generation != 0);
    QVERIFY(spy.wait(10000));
    QVERIFY(!spy.wait(300));
    QCOMPARE(spy.size(), 1);

    const QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), root);
    QCOMPARE(arguments.at(2).toULongLong(), generation);

    const auto snapshot = repositoryContextFromVariantMap(arguments.at(1).toMap());
    QVERIFY(snapshot.has_value());
    const RepositoryContextSnapshot value = snapshot.value_or(RepositoryContextSnapshot{});
    QCOMPARE(value.state, RepositoryContextState::InsideRepository);
    QCOMPARE(value.type, RepositoryContextType::Normal);
    QCOMPARE(value.repositoryRoot, QFileInfo(root).canonicalFilePath());
    QVERIFY(value.isRepositoryRoot);
}

void ContextDBusTest::rejectsSecondInstance()
{
    QProcess second;
    second.setProgram(QStringLiteral(DAEMON_EXECUTABLE_PATH));
    second.start();

    QVERIFY(second.waitForFinished(5000));
    QCOMPARE(second.exitStatus(), QProcess::NormalExit);
    QCOMPARE(second.exitCode(), 1);
    QCOMPARE(daemon->state(), QProcess::Running);
}

void ContextDBusTest::restartChangesGeneration()
{
    quint64 firstGeneration = 0;
    {
        ContextInterface context(ServiceName, ObjectPath, QDBusConnection::sessionBus());
        firstGeneration = requestContext(context, {});
    }
    QVERIFY(firstGeneration != 0);

    QVERIFY(stopDaemon(daemon));
    QVERIFY(startDaemon(daemon));

    ContextInterface context(ServiceName, ObjectPath, QDBusConnection::sessionBus());
    const quint64 secondGeneration = requestContext(context, {});
    QVERIFY(secondGeneration != 0);
    QVERIFY(secondGeneration != firstGeneration);
}

QTEST_GUILESS_MAIN(ContextDBusTest)

#include "ContextDBusTest.moc"
