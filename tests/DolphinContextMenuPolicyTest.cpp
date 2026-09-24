// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "ContextMenuPolicy.h"

#include <QHash>
#include <QTest>
#include <QUrl>

using LinuxGitShell::RepositoryContextFreshness;
using LinuxGitShell::RepositoryContextLookup;
using LinuxGitShell::RepositoryContextOperation;
using LinuxGitShell::RepositoryContextSnapshot;
using LinuxGitShell::RepositoryContextState;
using LinuxGitShell::RepositoryContextType;
using LinuxGitShell::Dolphin::ContextMenuDecision;
using LinuxGitShell::Dolphin::ContextMenuKind;
using LinuxGitShell::Dolphin::ContextMenuPolicy;

namespace
{

// Memory-only lookup table standing in for RepositoryContextClient; unknown keys are cold.
class FakeContexts
{
  public:
    void set(const QString& pathKey, const RepositoryContextSnapshot& snapshot)
    {
        snapshots.insert(pathKey, snapshot);
    }
    void setStale(const QString& pathKey) { stale.append(pathKey); }

    [[nodiscard]] ContextMenuDecision decide(const QList<QUrl>& urls) const
    {
        return ContextMenuPolicy::decide(urls,
                                         [this](const QString& pathKey) -> RepositoryContextLookup
                                         {
                                             if (stale.contains(pathKey))
                                             {
                                                 return {RepositoryContextFreshness::Stale, {}};
                                             }
                                             const auto it = snapshots.constFind(pathKey);
                                             if (it == snapshots.cend())
                                             {
                                                 return {};
                                             }
                                             return {RepositoryContextFreshness::Warm, it.value()};
                                         });
    }

  private:
    QHash<QString, RepositoryContextSnapshot> snapshots;
    QStringList stale;
};

RepositoryContextSnapshot inside(const QString& root,
                                 const QList<RepositoryContextOperation>& operations = {})
{
    RepositoryContextSnapshot snapshot;
    snapshot.state = RepositoryContextState::InsideRepository;
    snapshot.repositoryRoot = root;
    snapshot.operations = operations;
    return snapshot;
}

RepositoryContextSnapshot withState(RepositoryContextState state)
{
    RepositoryContextSnapshot snapshot;
    snapshot.state = state;
    return snapshot;
}

QUrl local(const QString& path) { return QUrl::fromLocalFile(path); }

} // namespace

class DolphinContextMenuPolicyTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void rejectsEmptyRemoteMixedAndOversizedSelections();
    void coldItemOffersGenericActionAndRefresh();
    void staleItemOffersGenericActionAndRefresh();
    void preservesUnusualSelectedPath();
    void itemInsideRepositoryOffersRepositoryActions();
    void bareRepositoryOffersRepositoryActions();
    void itemOutsideRepositoryOffersNothing();
    void failedLookupFallsBackToGenericAction();
    void itemsInSameRepositoryOpenRepositoryRoot();
    void itemsAcrossRepositoriesOfferNothing();
    void itemsWithOutsideMemberOfferNothing();
    void partlyColdSelectionOffersNothingAndRefreshesColdItems();
};

void DolphinContextMenuPolicyTest::rejectsEmptyRemoteMixedAndOversizedSelections()
{
    const FakeContexts contexts;

    QCOMPARE(contexts.decide({}).kind, ContextMenuKind::None);
    QCOMPARE(contexts.decide({QUrl(QStringLiteral("sftp://example.invalid/repository"))}).kind,
             ContextMenuKind::None);
    QCOMPARE(contexts
                 .decide({local(QStringLiteral("/tmp/one")),
                          QUrl(QStringLiteral("sftp://example.invalid/two"))})
                 .kind,
             ContextMenuKind::None);

    QList<QUrl> oversized;
    for (qsizetype index = 0; index <= ContextMenuPolicy::MaxSelectionItems; ++index)
    {
        oversized.append(local(QStringLiteral("/tmp/item%1").arg(index)));
    }
    const ContextMenuDecision decision = contexts.decide(oversized);
    QCOMPARE(decision.kind, ContextMenuKind::None);
    QVERIFY(decision.refreshKeys.isEmpty());
}

void DolphinContextMenuPolicyTest::coldItemOffersGenericActionAndRefresh()
{
    const FakeContexts contexts;

    const ContextMenuDecision decision = contexts.decide({local(QStringLiteral("/repo/./file"))});

    QCOMPARE(decision.kind, ContextMenuKind::Generic);
    QCOMPARE(decision.launchPath, QStringLiteral("/repo/./file"));
    QCOMPARE(decision.refreshKeys, QStringList{QStringLiteral("/repo/file")});
}

void DolphinContextMenuPolicyTest::staleItemOffersGenericActionAndRefresh()
{
    FakeContexts contexts;
    contexts.setStale(QStringLiteral("/repo"));

    const ContextMenuDecision decision = contexts.decide({local(QStringLiteral("/repo"))});

    QCOMPARE(decision.kind, ContextMenuKind::Generic);
    QCOMPARE(decision.refreshKeys, QStringList{QStringLiteral("/repo")});
}

void DolphinContextMenuPolicyTest::preservesUnusualSelectedPath()
{
    const QString path = QString::fromUtf8("/tmp/répôt with spaces/-file\nname.cpp");
    FakeContexts contexts;
    contexts.set(path, inside(QStringLiteral("/tmp/répôt with spaces")));

    const ContextMenuDecision decision = contexts.decide({local(path)});

    QCOMPARE(decision.kind, ContextMenuKind::Repository);
    QCOMPARE(decision.launchPath, path);
}

void DolphinContextMenuPolicyTest::itemInsideRepositoryOffersRepositoryActions()
{
    FakeContexts contexts;
    contexts.set(QStringLiteral("/repo/src"),
                 inside(QStringLiteral("/repo"),
                        {RepositoryContextOperation::Merge, RepositoryContextOperation::Bisect}));

    const ContextMenuDecision decision = contexts.decide({local(QStringLiteral("/repo/src/"))});

    QCOMPARE(decision.kind, ContextMenuKind::Repository);
    QCOMPARE(decision.launchPath, QStringLiteral("/repo/src/"));
    QCOMPARE(decision.operations,
             (QList{RepositoryContextOperation::Merge, RepositoryContextOperation::Bisect}));
    QVERIFY(decision.refreshKeys.isEmpty());
}

void DolphinContextMenuPolicyTest::bareRepositoryOffersRepositoryActions()
{
    FakeContexts contexts;
    RepositoryContextSnapshot bare = inside(QStringLiteral("/srv/bare.git"));
    bare.type = RepositoryContextType::Bare;
    bare.isRepositoryRoot = true;
    contexts.set(QStringLiteral("/srv/bare.git"), bare);

    const ContextMenuDecision decision = contexts.decide({local(QStringLiteral("/srv/bare.git"))});

    QCOMPARE(decision.kind, ContextMenuKind::Repository);
    QCOMPARE(decision.launchPath, QStringLiteral("/srv/bare.git"));
}

void DolphinContextMenuPolicyTest::itemOutsideRepositoryOffersNothing()
{
    FakeContexts contexts;
    contexts.set(QStringLiteral("/tmp"), withState(RepositoryContextState::OutsideRepository));

    const ContextMenuDecision decision = contexts.decide({local(QStringLiteral("/tmp"))});

    QCOMPARE(decision.kind, ContextMenuKind::None);
    QVERIFY(decision.refreshKeys.isEmpty());
}

void DolphinContextMenuPolicyTest::failedLookupFallsBackToGenericAction()
{
    FakeContexts contexts;
    contexts.set(QStringLiteral("/error"), withState(RepositoryContextState::DiscoveryError));
    contexts.set(QStringLiteral("/unavailable"), withState(RepositoryContextState::Unavailable));

    QCOMPARE(contexts.decide({local(QStringLiteral("/error"))}).kind, ContextMenuKind::Generic);
    QCOMPARE(contexts.decide({local(QStringLiteral("/unavailable"))}).kind,
             ContextMenuKind::Generic);
    QVERIFY(contexts.decide({local(QStringLiteral("/error"))}).refreshKeys.isEmpty());
}

void DolphinContextMenuPolicyTest::itemsInSameRepositoryOpenRepositoryRoot()
{
    FakeContexts contexts;
    contexts.set(QStringLiteral("/repo/a"),
                 inside(QStringLiteral("/repo"), {RepositoryContextOperation::Rebase}));
    contexts.set(QStringLiteral("/repo/b"),
                 inside(QStringLiteral("/repo"), {RepositoryContextOperation::Rebase}));

    const ContextMenuDecision decision =
        contexts.decide({local(QStringLiteral("/repo/a")), local(QStringLiteral("/repo/b"))});

    QCOMPARE(decision.kind, ContextMenuKind::Repository);
    QCOMPARE(decision.launchPath, QStringLiteral("/repo"));
    QCOMPARE(decision.operations, QList{RepositoryContextOperation::Rebase});
}

void DolphinContextMenuPolicyTest::itemsAcrossRepositoriesOfferNothing()
{
    FakeContexts contexts;
    contexts.set(QStringLiteral("/repo/file"), inside(QStringLiteral("/repo")));
    contexts.set(QStringLiteral("/repo/module/file"), inside(QStringLiteral("/repo/module")));

    const ContextMenuDecision decision = contexts.decide(
        {local(QStringLiteral("/repo/file")), local(QStringLiteral("/repo/module/file"))});

    QCOMPARE(decision.kind, ContextMenuKind::None);
    QVERIFY(decision.refreshKeys.isEmpty());
}

void DolphinContextMenuPolicyTest::itemsWithOutsideMemberOfferNothing()
{
    FakeContexts contexts;
    contexts.set(QStringLiteral("/repo/file"), inside(QStringLiteral("/repo")));
    contexts.set(QStringLiteral("/tmp/file"), withState(RepositoryContextState::OutsideRepository));

    QCOMPARE(
        contexts.decide({local(QStringLiteral("/tmp/file")), local(QStringLiteral("/repo/file"))})
            .kind,
        ContextMenuKind::None);
}

void DolphinContextMenuPolicyTest::partlyColdSelectionOffersNothingAndRefreshesColdItems()
{
    FakeContexts contexts;
    contexts.set(QStringLiteral("/repo/a"), inside(QStringLiteral("/repo")));

    const ContextMenuDecision decision =
        contexts.decide({local(QStringLiteral("/repo/a")), local(QStringLiteral("/repo/b")),
                         local(QStringLiteral("/repo/b/"))});

    QCOMPARE(decision.kind, ContextMenuKind::None);
    QCOMPARE(decision.refreshKeys, QStringList{QStringLiteral("/repo/b")});
}

QTEST_GUILESS_MAIN(DolphinContextMenuPolicyTest)

#include "DolphinContextMenuPolicyTest.moc"
