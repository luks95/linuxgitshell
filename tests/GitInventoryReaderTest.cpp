// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/GitInventoryReader.h"
#include "GitTestSupport.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace std::chrono_literals;

namespace
{

[[nodiscard]] LinuxGitShell::GitInventoryResult
readInventory(LinuxGitShell::GitInventoryReader& reader,
              const LinuxGitShell::GitInventoryRequest& request)
{
    QSignalSpy finishedSpy(&reader, &LinuxGitShell::GitInventoryReader::finished);
    if (reader.start(request) != LinuxGitShell::GitInventoryStartResult::Accepted)
    {
        return {};
    }
    if (finishedSpy.isEmpty() && !finishedSpy.wait(10000))
    {
        return {};
    }
    return qvariant_cast<LinuxGitShell::GitInventoryResult>(finishedSpy.takeFirst().at(0));
}

} // namespace

class GitInventoryReaderTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void initTestCase();
    void parsesNulSeparatedPathsAndDeduplicates();
    void rejectsMalformedOutput();
    void listsTrackedPathsOfRealRepository();
    void emptyRepositoryHasEmptyInventory();
    void skipsParsingWhenOutputDigestMatches();
    void keepsUnusualFilenamesVerbatim();
    void reportsNotRepository();
    void overflowYieldsNoPartialInventory();
    void rejectsInvalidRequests();
};

void GitInventoryReaderTest::initTestCase()
{
    GitTestSupport::isolateGitConfiguration();
    qRegisterMetaType<LinuxGitShell::GitInventoryResult>();
}

void GitInventoryReaderTest::parsesNulSeparatedPathsAndDeduplicates()
{
    const auto parsed = LinuxGitShell::GitInventoryParser::parse(
        QByteArrayLiteral("a.txt\0dir/b.txt\0a.txt\0dir/b.txt\0c\0"));
    QVERIFY(parsed.paths.has_value());
    QCOMPARE(*parsed.paths, (QStringList{QStringLiteral("a.txt"), QStringLiteral("dir/b.txt"),
                                         QStringLiteral("c")}));

    const auto empty = LinuxGitShell::GitInventoryParser::parse({});
    QVERIFY(empty.paths.has_value());
    QVERIFY(empty.paths->isEmpty());
}

void GitInventoryReaderTest::rejectsMalformedOutput()
{
    auto parsed = LinuxGitShell::GitInventoryParser::parse(QByteArrayLiteral("a.txt"));
    QVERIFY(!parsed.paths.has_value());
    QCOMPARE(parsed.error, LinuxGitShell::GitInventoryParseError::MissingTerminator);

    parsed = LinuxGitShell::GitInventoryParser::parse(QByteArray("a\0\0b\0", 5));
    QVERIFY(!parsed.paths.has_value());
    QCOMPARE(parsed.error, LinuxGitShell::GitInventoryParseError::EmptyPath);
}

void GitInventoryReaderTest::listsTrackedPathsOfRealRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("repo"));
    QVERIFY(GitTestSupport::createRepository(root));
    QVERIFY(QDir().mkpath(QDir(root).filePath(QStringLiteral("sub/deeper"))));
    QVERIFY(GitTestSupport::writeTextFile(QDir(root).filePath(QStringLiteral("sub/deeper/x.txt")),
                                          "x\n"));
    QVERIFY(
        GitTestSupport::writeTextFile(QDir(root).filePath(QStringLiteral("untracked.txt")), "u\n"));
    QVERIFY(GitTestSupport::runGit(root, {QStringLiteral("add"), QStringLiteral("sub")}));

    LinuxGitShell::GitInventoryReader reader;
    LinuxGitShell::GitInventoryRequest request;
    request.repositoryRoot = root;
    request.timeout = 10s;
    const auto result = readInventory(reader, request);
    QCOMPARE(result.error, LinuxGitShell::GitInventoryReadError::None);
    QVERIFY(result.paths.has_value());
    QStringList paths = *result.paths;
    paths.sort();
    QCOMPARE(paths, (QStringList{QStringLiteral("file.txt"), QStringLiteral("sub/deeper/x.txt")}));
}

void GitInventoryReaderTest::skipsParsingWhenOutputDigestMatches()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("repo"));
    QVERIFY(GitTestSupport::createRepository(root));

    LinuxGitShell::GitInventoryReader reader;
    LinuxGitShell::GitInventoryRequest request;
    request.repositoryRoot = root;
    request.timeout = 10s;
    const auto first = readInventory(reader, request);
    QCOMPARE(first.error, LinuxGitShell::GitInventoryReadError::None);
    QVERIFY(first.paths.has_value());
    QVERIFY(!first.unchanged);
    QVERIFY(!first.outputDigest.isEmpty());

    request.unchangedOutputDigest = first.outputDigest;
    const auto same = readInventory(reader, request);
    QCOMPARE(same.error, LinuxGitShell::GitInventoryReadError::None);
    QVERIFY(same.unchanged);
    QVERIFY(!same.paths.has_value());
    QCOMPARE(same.outputDigest, first.outputDigest);

    // Any change to the tracked set yields a different digest and a full parse again.
    QVERIFY(
        GitTestSupport::writeTextFile(QDir(root).filePath(QStringLiteral("another.txt")), "a\n"));
    QVERIFY(GitTestSupport::runGit(root, {QStringLiteral("add"), QStringLiteral("another.txt")}));
    const auto changed = readInventory(reader, request);
    QVERIFY(!changed.unchanged);
    QVERIFY(changed.paths.has_value());
    QVERIFY(changed.outputDigest != first.outputDigest);
    QVERIFY(changed.paths->contains(QStringLiteral("another.txt")));
}

void GitInventoryReaderTest::emptyRepositoryHasEmptyInventory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(
        GitTestSupport::runGit(directory.path(), {QStringLiteral("init"), QStringLiteral(".")}));

    LinuxGitShell::GitInventoryReader reader;
    LinuxGitShell::GitInventoryRequest request;
    request.repositoryRoot = directory.path();
    const auto result = readInventory(reader, request);
    QCOMPARE(result.error, LinuxGitShell::GitInventoryReadError::None);
    QVERIFY(result.paths.has_value());
    QVERIFY(result.paths->isEmpty());
}

void GitInventoryReaderTest::keepsUnusualFilenamesVerbatim()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(
        GitTestSupport::runGit(directory.path(), {QStringLiteral("init"), QStringLiteral(".")}));
    const QStringList names{QStringLiteral("with space.txt"), QStringLiteral("tab\there.txt"),
                            QStringLiteral("ünï.txt"), QStringLiteral("-dash.txt"),
                            QStringLiteral("new\nline.txt")};
    for (const QString& name : names)
    {
        QVERIFY(GitTestSupport::writeTextFile(QDir(directory.path()).filePath(name), "x\n"));
    }
    QVERIFY(
        GitTestSupport::runGit(directory.path(), {QStringLiteral("add"), QStringLiteral("-A")}));

    LinuxGitShell::GitInventoryReader reader;
    LinuxGitShell::GitInventoryRequest request;
    request.repositoryRoot = directory.path();
    const auto result = readInventory(reader, request);
    QCOMPARE(result.error, LinuxGitShell::GitInventoryReadError::None);
    QVERIFY(result.paths.has_value());
    QStringList expected = names;
    expected.sort();
    QStringList actual = *result.paths;
    actual.sort();
    QCOMPARE(actual, expected);
}

void GitInventoryReaderTest::reportsNotRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LinuxGitShell::GitInventoryReader reader;
    LinuxGitShell::GitInventoryRequest request;
    request.repositoryRoot = directory.path();
    const auto result = readInventory(reader, request);
    QCOMPARE(result.error, LinuxGitShell::GitInventoryReadError::NotRepository);
    QVERIFY(!result.paths.has_value());
}

void GitInventoryReaderTest::overflowYieldsNoPartialInventory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(
        GitTestSupport::runGit(directory.path(), {QStringLiteral("init"), QStringLiteral(".")}));
    for (int index = 0; index < 50; ++index)
    {
        QVERIFY(GitTestSupport::writeTextFile(
            QDir(directory.path()).filePath(QStringLiteral("file-%1.txt").arg(index)), "x\n"));
    }
    QVERIFY(
        GitTestSupport::runGit(directory.path(), {QStringLiteral("add"), QStringLiteral("-A")}));

    LinuxGitShell::GitInventoryReader reader;
    LinuxGitShell::GitInventoryRequest request;
    request.repositoryRoot = directory.path();
    request.maxOutputBytes = 64;
    const auto result = readInventory(reader, request);
    QCOMPARE(result.error, LinuxGitShell::GitInventoryReadError::OutputLimitExceeded);
    QVERIFY(!result.paths.has_value());
}

void GitInventoryReaderTest::rejectsInvalidRequests()
{
    LinuxGitShell::GitInventoryReader reader;
    LinuxGitShell::GitInventoryRequest request;
    QCOMPARE(reader.start(request), LinuxGitShell::GitInventoryStartResult::InvalidPath);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    request.repositoryRoot = directory.path();
    request.timeout = 0ms;
    QCOMPARE(reader.start(request), LinuxGitShell::GitInventoryStartResult::InvalidTimeout);
    request.timeout.reset();
    request.maxOutputBytes = 0;
    QCOMPARE(reader.start(request), LinuxGitShell::GitInventoryStartResult::InvalidOutputLimit);
}

QTEST_GUILESS_MAIN(GitInventoryReaderTest)
#include "GitInventoryReaderTest.moc"
