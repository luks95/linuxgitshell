// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "ContextService.h"
#include "GitTestSupport.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <chrono>

using LinuxGitShell::ContextService;
using LinuxGitShell::ContextServiceLimits;
using LinuxGitShell::RepositoryContextOperation;
using LinuxGitShell::repositoryContextPathKey;
using LinuxGitShell::RepositoryContextSnapshot;
using LinuxGitShell::RepositoryContextState;
using LinuxGitShell::RepositoryContextType;

using namespace std::chrono_literals;

namespace
{

using Replies = QHash<QString, RepositoryContextSnapshot>;

// Requests context and waits for `expected` replies, returning them by path key.
[[nodiscard]] Replies resolve(ContextService& service, const QStringList& paths, qsizetype expected)
{
    QSignalSpy spy(&service, &ContextService::contextReady);
    const quint64 generation = service.requestContext(paths);
    while (spy.size() < expected)
    {
        if (!spy.wait(10000))
        {
            break;
        }
    }

    Replies replies;
    for (const QList<QVariant>& arguments : std::as_const(spy))
    {
        if (arguments.at(2).toULongLong() == generation)
        {
            replies.insert(arguments.at(0).toString(),
                           qvariant_cast<RepositoryContextSnapshot>(arguments.at(1)));
        }
    }
    return replies;
}

[[nodiscard]] QString canonical(const QString& path) { return QFileInfo(path).canonicalFilePath(); }

} // namespace

class ContextServiceTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    void generationIsRandomNonZeroAndStable();
    void resolvesNormalRepositoryRootAndNestedFile();
    void resolvesBareRepository();
    void resolvesLinkedWorktree();
    void resolvesSubmodule();
    void resolvesRemoteUpstreamAndOperation();
    void reportsOutsideRepository();
    void reportsMissingPathAsError();
    void normalizesUnusualPathKeys();
    void reportsNewlineInRepositoryRootAsError();
    void ignoresInvalidPaths();
    void limitsPathsPerRequest();
    void answersRepeatedRequestFromCache();
    void deduplicatesPendingPaths();
    void timesOutSlowDiscovery();
};

void ContextServiceTest::initTestCase()
{
    GitTestSupport::isolateGitConfiguration();
    QVERIFY(GitTestSupport::runGit(QDir::tempPath(), {QStringLiteral("--version")}));
}

void ContextServiceTest::generationIsRandomNonZeroAndStable()
{
    ContextService first;
    ContextService second;

    QVERIFY(first.generation() != 0);
    QCOMPARE(first.requestContext({}), first.generation());
    QVERIFY(first.generation() != second.generation());
}

void ContextServiceTest::resolvesNormalRepositoryRootAndNestedFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("repo"));
    QVERIFY(GitTestSupport::createRepository(root));
    const QString file = QDir(root).filePath(QStringLiteral("file.txt"));

    ContextService service;
    const Replies replies = resolve(service, {root, file}, 2);

    QCOMPARE(replies.size(), 2);
    const RepositoryContextSnapshot rootContext = replies.value(root);
    QCOMPARE(rootContext.state, RepositoryContextState::InsideRepository);
    QCOMPARE(rootContext.type, RepositoryContextType::Normal);
    QCOMPARE(rootContext.repositoryRoot, canonical(root));
    QVERIFY(rootContext.isRepositoryRoot);
    QVERIFY(rootContext.operations.isEmpty());
    QVERIFY(!rootContext.hasRemote);
    QVERIFY(!rootContext.hasUpstream);

    const RepositoryContextSnapshot fileContext = replies.value(file);
    QCOMPARE(fileContext.state, RepositoryContextState::InsideRepository);
    QCOMPARE(fileContext.repositoryRoot, canonical(root));
    QVERIFY(!fileContext.isRepositoryRoot);
}

void ContextServiceTest::resolvesBareRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString bare = directory.filePath(QStringLiteral("bare.git"));
    QVERIFY(QDir().mkpath(bare));
    QVERIFY(GitTestSupport::runGit(bare, {QStringLiteral("init"), QStringLiteral("--bare")}));

    ContextService service;
    const RepositoryContextSnapshot context = resolve(service, {bare}, 1).value(bare);

    QCOMPARE(context.state, RepositoryContextState::InsideRepository);
    QCOMPARE(context.type, RepositoryContextType::Bare);
    QCOMPARE(context.repositoryRoot, canonical(bare));
    QVERIFY(context.isRepositoryRoot);
}

void ContextServiceTest::resolvesLinkedWorktree()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString main = directory.filePath(QStringLiteral("main"));
    const QString worktree = directory.filePath(QStringLiteral("linked"));
    QVERIFY(GitTestSupport::createRepository(main));
    QVERIFY(GitTestSupport::runGit(main,
                                   {QStringLiteral("worktree"), QStringLiteral("add"), worktree}));
    QVERIFY(QFileInfo(QDir(worktree).filePath(QStringLiteral(".git"))).isFile());

    ContextService service;
    const RepositoryContextSnapshot context = resolve(service, {worktree}, 1).value(worktree);

    QCOMPARE(context.state, RepositoryContextState::InsideRepository);
    QCOMPARE(context.type, RepositoryContextType::LinkedWorktree);
    QCOMPARE(context.repositoryRoot, canonical(worktree));
    QVERIFY(context.isRepositoryRoot);
}

void ContextServiceTest::resolvesSubmodule()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = directory.filePath(QStringLiteral("source"));
    const QString superproject = directory.filePath(QStringLiteral("superproject"));
    QVERIFY(GitTestSupport::createRepository(source));
    QVERIFY(GitTestSupport::createRepository(superproject));
    QVERIFY(
        GitTestSupport::runGit(superproject, {QStringLiteral("submodule"), QStringLiteral("add"),
                                              source, QStringLiteral("modules/child")}));
    const QString submodule = QDir(superproject).filePath(QStringLiteral("modules/child"));
    const QString submoduleFile = QDir(submodule).filePath(QStringLiteral("file.txt"));

    ContextService service;
    const Replies replies = resolve(service, {submodule, submoduleFile, superproject}, 3);

    QCOMPARE(replies.value(submodule).type, RepositoryContextType::Submodule);
    QCOMPARE(replies.value(submodule).repositoryRoot, canonical(submodule));
    QVERIFY(replies.value(submodule).isRepositoryRoot);
    QCOMPARE(replies.value(submoduleFile).repositoryRoot, canonical(submodule));
    QVERIFY(!replies.value(submoduleFile).isRepositoryRoot);
    QCOMPARE(replies.value(superproject).type, RepositoryContextType::Normal);
    QCOMPARE(replies.value(superproject).repositoryRoot, canonical(superproject));
}

void ContextServiceTest::resolvesRemoteUpstreamAndOperation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString origin = directory.filePath(QStringLiteral("origin"));
    const QString clone = directory.filePath(QStringLiteral("clone"));
    QVERIFY(GitTestSupport::createRepository(origin));
    QVERIFY(GitTestSupport::runGit(directory.path(), {QStringLiteral("clone"), origin, clone}));
    QVERIFY(GitTestSupport::writeTextFile(QDir(clone).filePath(QStringLiteral(".git/MERGE_HEAD")),
                                          "0000000000000000000000000000000000000000\n"));

    ContextService service;
    const RepositoryContextSnapshot context = resolve(service, {clone}, 1).value(clone);

    QCOMPARE(context.state, RepositoryContextState::InsideRepository);
    QVERIFY(context.hasRemote);
    QVERIFY(context.hasUpstream);
    QCOMPARE(context.operations, QList{RepositoryContextOperation::Merge});
}

void ContextServiceTest::reportsOutsideRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ContextService service;
    const Replies replies = resolve(service, {directory.path()}, 1);

    QCOMPARE(replies.value(directory.path()).state, RepositoryContextState::OutsideRepository);
}

void ContextServiceTest::reportsMissingPathAsError()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString missing = directory.filePath(QStringLiteral("missing"));

    ContextService service;
    const Replies replies = resolve(service, {missing}, 1);

    QCOMPARE(replies.value(missing).state, RepositoryContextState::DiscoveryError);
}

void ContextServiceTest::normalizesUnusualPathKeys()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QString::fromUtf8("répôt with spaces/-name"));
    QVERIFY(GitTestSupport::createRepository(root));
    const QString file = QDir(root).filePath(QStringLiteral("-file\nname.txt"));
    QVERIFY(GitTestSupport::writeTextFile(file, "content\n"));
    const QStringList requested{root + QStringLiteral("/./"),
                                root + QStringLiteral("/sub/../-file\nname.txt")};

    ContextService service;
    const Replies replies = resolve(service, requested, 2);

    QCOMPARE(repositoryContextPathKey(requested.at(0)), root);
    QCOMPARE(repositoryContextPathKey(requested.at(1)), file);
    QCOMPARE(replies.value(root).repositoryRoot, canonical(root));
    QVERIFY(replies.value(root).isRepositoryRoot);
    QCOMPARE(replies.value(file).repositoryRoot, canonical(root));
    QVERIFY(!replies.value(file).isRepositoryRoot);
}

void ContextServiceTest::reportsNewlineInRepositoryRootAsError()
{
    // Git reports repository paths one per line, so RepositoryDiscovery cannot represent a root
    // containing a newline. The service must degrade to a short-lived error, not a wrong root.
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("repo\nname"));
    QVERIFY(GitTestSupport::createRepository(root));

    ContextService service;
    const Replies replies = resolve(service, {root}, 1);

    QCOMPARE(replies.value(root).state, RepositoryContextState::DiscoveryError);
}

void ContextServiceTest::ignoresInvalidPaths()
{
    ContextService service;
    QSignalSpy spy(&service, &ContextService::contextReady);

    QCOMPARE(service.requestContext({QString(), QStringLiteral("relative/path"),
                                     QStringLiteral("/") + QString(5000, QLatin1Char('a'))}),
             service.generation());

    QVERIFY(!spy.wait(200));
}

void ContextServiceTest::limitsPathsPerRequest()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ContextServiceLimits limits;
    limits.maxPathsPerRequest = 1;
    ContextService service(limits);
    QSignalSpy spy(&service, &ContextService::contextReady);

    (void)service.requestContext({directory.path(), directory.filePath(QStringLiteral("other"))});

    QVERIFY(spy.wait(10000));
    QVERIFY(!spy.wait(300));
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), directory.path());
}

void ContextServiceTest::answersRepeatedRequestFromCache()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("repo"));
    QVERIFY(GitTestSupport::createRepository(root));

    ContextService service;
    QCOMPARE(resolve(service, {root}, 1).value(root).state,
             RepositoryContextState::InsideRepository);

    // Only the service cache can still report a repository once its metadata is gone.
    QVERIFY(QDir(QDir(root).filePath(QStringLiteral(".git"))).removeRecursively());
    QCOMPARE(resolve(service, {root}, 1).value(root).state,
             RepositoryContextState::InsideRepository);
}

void ContextServiceTest::deduplicatesPendingPaths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("repo"));
    QVERIFY(GitTestSupport::createRepository(root));

    ContextService service;
    QSignalSpy spy(&service, &ContextService::contextReady);
    (void)service.requestContext({root, root});
    (void)service.requestContext({root});

    QVERIFY(spy.wait(10000));
    QVERIFY(!spy.wait(300));
    QCOMPARE(spy.size(), 1);
}

void ContextServiceTest::timesOutSlowDiscovery()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fakeGit = directory.filePath(QStringLiteral("git"));
    QVERIFY(GitTestSupport::writeTextFile(fakeGit, "#!/bin/sh\nexec sleep 30\n"));
    QVERIFY(QFile::setPermissions(fakeGit, QFileDevice::ReadOwner | QFileDevice::ExeOwner));

    const QByteArray originalPath = qgetenv("PATH");
    qputenv("PATH", QFile::encodeName(directory.path()) + ':' + originalPath);
    const auto restorePath = qScopeGuard([&originalPath] { qputenv("PATH", originalPath); });

    ContextServiceLimits limits;
    limits.discoveryTimeout = 100ms;
    ContextService service(limits);
    const Replies replies = resolve(service, {directory.path()}, 1);

    QCOMPARE(replies.value(directory.path()).state, RepositoryContextState::DiscoveryError);
}

QTEST_GUILESS_MAIN(ContextServiceTest)

#include "ContextServiceTest.moc"
