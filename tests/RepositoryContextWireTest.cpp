// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/repositorycontext/RepositoryContextWire.h"

#include <QTest>

using LinuxGitShell::repositoryContextFromVariantMap;
using LinuxGitShell::RepositoryContextOperation;
using LinuxGitShell::RepositoryContextSnapshot;
using LinuxGitShell::RepositoryContextState;
using LinuxGitShell::repositoryContextToVariantMap;
using LinuxGitShell::RepositoryContextType;

namespace
{

RepositoryContextSnapshot insideSnapshot(RepositoryContextType type)
{
    RepositoryContextSnapshot snapshot;
    snapshot.state = RepositoryContextState::InsideRepository;
    snapshot.repositoryRoot = QString::fromUtf8("/tmp/répôt with spaces/-repo\nname");
    snapshot.type = type;
    snapshot.operations = {RepositoryContextOperation::Merge, RepositoryContextOperation::Rebase,
                           RepositoryContextOperation::CherryPick,
                           RepositoryContextOperation::Revert, RepositoryContextOperation::Bisect};
    snapshot.isRepositoryRoot = true;
    snapshot.hasRemote = true;
    snapshot.hasUpstream = true;
    return snapshot;
}

QVariantMap insideMap()
{
    return repositoryContextToVariantMap(insideSnapshot(RepositoryContextType::Normal));
}

} // namespace

class RepositoryContextWireTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void roundTripsRepositorySnapshots_data();
    void roundTripsRepositorySnapshots();
    void encodesStableNames();
    void roundTripsNegativeStatesWithStateOnly_data();
    void roundTripsNegativeStatesWithStateOnly();
    void ignoresUnknownKeysAndOperations();
    void rejectsInvalidMaps();
};

void RepositoryContextWireTest::roundTripsRepositorySnapshots_data()
{
    QTest::addColumn<RepositoryContextType>("type");
    QTest::newRow("normal") << RepositoryContextType::Normal;
    QTest::newRow("bare") << RepositoryContextType::Bare;
    QTest::newRow("worktree") << RepositoryContextType::LinkedWorktree;
    QTest::newRow("submodule") << RepositoryContextType::Submodule;
}

void RepositoryContextWireTest::roundTripsRepositorySnapshots()
{
    QFETCH(RepositoryContextType, type);
    const RepositoryContextSnapshot snapshot = insideSnapshot(type);

    const auto decoded = repositoryContextFromVariantMap(repositoryContextToVariantMap(snapshot));

    QVERIFY(decoded.has_value());
    QVERIFY(decoded.value_or(RepositoryContextSnapshot{}) == snapshot);
}

void RepositoryContextWireTest::encodesStableNames()
{
    const QVariantMap map = insideMap();

    QCOMPARE(map.value(QStringLiteral("state")).toString(), QStringLiteral("inside"));
    QCOMPARE(map.value(QStringLiteral("type")).toString(), QStringLiteral("normal"));
    QCOMPARE(map.value(QStringLiteral("operations")).toStringList(),
             (QStringList{QStringLiteral("merge"), QStringLiteral("rebase"),
                          QStringLiteral("cherry-pick"), QStringLiteral("revert"),
                          QStringLiteral("bisect")}));
    QCOMPARE(repositoryContextToVariantMap(insideSnapshot(RepositoryContextType::LinkedWorktree))
                 .value(QStringLiteral("type"))
                 .toString(),
             QStringLiteral("worktree"));
}

void RepositoryContextWireTest::roundTripsNegativeStatesWithStateOnly_data()
{
    QTest::addColumn<RepositoryContextState>("state");
    QTest::addColumn<QString>("name");
    QTest::newRow("outside") << RepositoryContextState::OutsideRepository
                             << QStringLiteral("outside");
    QTest::newRow("unavailable") << RepositoryContextState::Unavailable
                                 << QStringLiteral("unavailable");
    QTest::newRow("error") << RepositoryContextState::DiscoveryError << QStringLiteral("error");
}

void RepositoryContextWireTest::roundTripsNegativeStatesWithStateOnly()
{
    QFETCH(RepositoryContextState, state);
    QFETCH(QString, name);
    RepositoryContextSnapshot snapshot;
    snapshot.state = state;
    snapshot.repositoryRoot = QStringLiteral("/ignored");

    const QVariantMap map = repositoryContextToVariantMap(snapshot);
    QCOMPARE(map.keys(), QStringList{QStringLiteral("state")});
    QCOMPARE(map.value(QStringLiteral("state")).toString(), name);

    const auto decoded = repositoryContextFromVariantMap(map);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded.value_or(RepositoryContextSnapshot{}).state, state);
    QVERIFY(decoded.value_or(RepositoryContextSnapshot{}).repositoryRoot.isEmpty());
}

void RepositoryContextWireTest::ignoresUnknownKeysAndOperations()
{
    QVariantMap map = insideMap();
    map.insert(QStringLiteral("futureField"), 42);
    map.insert(QStringLiteral("operations"),
               QStringList{QStringLiteral("merge"), QStringLiteral("future-operation")});

    const auto decoded = repositoryContextFromVariantMap(map);

    QVERIFY(decoded.has_value());
    QCOMPARE(decoded.value_or(RepositoryContextSnapshot{}).operations,
             QList{RepositoryContextOperation::Merge});
}

void RepositoryContextWireTest::rejectsInvalidMaps()
{
    QVERIFY(!repositoryContextFromVariantMap({}).has_value());
    QVERIFY(!repositoryContextFromVariantMap(
                 {{QStringLiteral("state"), QStringLiteral("future-state")}})
                 .has_value());

    QVariantMap withoutRoot = insideMap();
    withoutRoot.remove(QStringLiteral("repositoryRoot"));
    QVERIFY(!repositoryContextFromVariantMap(withoutRoot).has_value());

    QVariantMap unknownType = insideMap();
    unknownType.insert(QStringLiteral("type"), QStringLiteral("future-type"));
    QVERIFY(!repositoryContextFromVariantMap(unknownType).has_value());
}

QTEST_GUILESS_MAIN(RepositoryContextWireTest)

#include "RepositoryContextWireTest.moc"
