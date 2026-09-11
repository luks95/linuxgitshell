// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/gitcore/GitStatus.h"

#include <QTest>

namespace
{

[[nodiscard]] QByteArray objectId(char digit) { return QByteArray(40, digit); }

[[nodiscard]] QByteArray trackedRecord(const QByteArray& prefix, const QByteArray& path)
{
    return prefix + QByteArrayLiteral(" 100644 100644 100644 ") + objectId('a') + ' ' +
           objectId('b') + ' ' + path + '\0';
}

} // namespace

class GitStatusParserTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void parsesEmptyStatus();
    void parsesInitialAndDetachedHeaders();
    void parsesHeadersAndOrdinaryEntries();
    void mapsTrackedStates_data();
    void mapsTrackedStates();
    void parsesUntrackedIgnoredAndUnusualPaths();
    void parsesRenameAndCopyEntries();
    void parsesUnmergedEntryAndStageMetadata();
    void parsesUnmergedStates_data();
    void parsesUnmergedStates();
    void parsesSubmoduleState();
    void ignoresUnknownHeaders();
    void rejectsMalformedInput_data();
    void rejectsMalformedInput();
};

void GitStatusParserTest::parsesEmptyStatus()
{
    const auto result = LinuxGitShell::GitStatusParser::parse({});
    QCOMPARE(result.error, LinuxGitShell::GitStatusParseError::None);
    QVERIFY(result.status.has_value());
    QVERIFY(result.status.value_or(LinuxGitShell::GitStatusSnapshot{}).isClean());
}

void GitStatusParserTest::parsesInitialAndDetachedHeaders()
{
    const auto initial = LinuxGitShell::GitStatusParser::parse(
        QByteArrayLiteral("# branch.oid (initial)\0# branch.head main\0"));
    QCOMPARE(initial.error, LinuxGitShell::GitStatusParseError::None);
    const auto initialStatus = initial.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    QVERIFY(initialStatus.isInitial);
    QVERIFY(initialStatus.headObjectId.isEmpty());
    QCOMPARE(initialStatus.currentBranch, QStringLiteral("main"));
    QVERIFY(!initialStatus.isDetachedHead);

    const QByteArray detachedOutput = QByteArrayLiteral("# branch.oid ") + objectId('a') +
                                      QByteArrayLiteral("\0# branch.head (detached)\0");
    const auto detached = LinuxGitShell::GitStatusParser::parse(detachedOutput);
    QCOMPARE(detached.error, LinuxGitShell::GitStatusParseError::None);
    const auto detachedStatus = detached.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    QVERIFY(!detachedStatus.isInitial);
    QCOMPARE(detachedStatus.headObjectId, QString::fromLatin1(objectId('a')));
    QVERIFY(detachedStatus.currentBranch.isEmpty());
    QVERIFY(detachedStatus.isDetachedHead);
}

void GitStatusParserTest::parsesHeadersAndOrdinaryEntries()
{
    QByteArray output;
    output += QByteArrayLiteral("# branch.oid ") + objectId('a') + '\0';
    output += QByteArrayLiteral("# branch.head main\0");
    output += QByteArrayLiteral("# branch.upstream origin/main\0");
    output += QByteArrayLiteral("# branch.ab +12 -3\0");
    output += QByteArrayLiteral("# stash 2\0");
    output += trackedRecord(QByteArrayLiteral("1 M. N..."), QByteArrayLiteral("staged.txt"));
    output += trackedRecord(QByteArrayLiteral("1 .M N..."), QByteArrayLiteral("working.txt"));
    output += trackedRecord(QByteArrayLiteral("1 MM N..."), QByteArrayLiteral("combined.txt"));

    const auto result = LinuxGitShell::GitStatusParser::parse(output);
    QCOMPARE(result.error, LinuxGitShell::GitStatusParseError::None);
    QVERIFY(result.status.has_value());
    const auto status = result.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    QCOMPARE(status.headObjectId, QString::fromLatin1(objectId('a')));
    QCOMPARE(status.currentBranch, QStringLiteral("main"));
    QVERIFY(!status.isDetachedHead);
    QCOMPARE(status.upstream, QStringLiteral("origin/main"));
    QCOMPARE(status.aheadCount.value_or(-1), 12);
    QCOMPARE(status.behindCount.value_or(-1), 3);
    QCOMPARE(status.stashCount.value_or(-1), 2);
    QCOMPARE(status.entries.size(), 3);
    QVERIFY(!status.isClean());
    QCOMPARE(status.entries.at(0).indexState, LinuxGitShell::GitFileState::Modified);
    QCOMPARE(status.entries.at(0).workTreeState, LinuxGitShell::GitFileState::Unmodified);
    QCOMPARE(status.entries.at(1).indexState, LinuxGitShell::GitFileState::Unmodified);
    QCOMPARE(status.entries.at(1).workTreeState, LinuxGitShell::GitFileState::Modified);
    QCOMPARE(status.entries.at(2).indexState, LinuxGitShell::GitFileState::Modified);
    QCOMPARE(status.entries.at(2).workTreeState, LinuxGitShell::GitFileState::Modified);
}

void GitStatusParserTest::mapsTrackedStates_data()
{
    QTest::addColumn<QByteArray>("statusCode");
    QTest::addColumn<LinuxGitShell::GitFileState>("indexState");
    QTest::addColumn<LinuxGitShell::GitFileState>("workTreeState");

    QTest::newRow("staged-modified")
        << QByteArrayLiteral("M.") << LinuxGitShell::GitFileState::Modified
        << LinuxGitShell::GitFileState::Unmodified;
    QTest::newRow("unstaged-modified")
        << QByteArrayLiteral(".M") << LinuxGitShell::GitFileState::Unmodified
        << LinuxGitShell::GitFileState::Modified;
    QTest::newRow("added") << QByteArrayLiteral("A.") << LinuxGitShell::GitFileState::Added
                           << LinuxGitShell::GitFileState::Unmodified;
    QTest::newRow("deleted") << QByteArrayLiteral(".D") << LinuxGitShell::GitFileState::Unmodified
                             << LinuxGitShell::GitFileState::Deleted;
    QTest::newRow("type-changed") << QByteArrayLiteral("T.")
                                  << LinuxGitShell::GitFileState::TypeChanged
                                  << LinuxGitShell::GitFileState::Unmodified;
}

void GitStatusParserTest::mapsTrackedStates()
{
    QFETCH(QByteArray, statusCode);
    QFETCH(LinuxGitShell::GitFileState, indexState);
    QFETCH(LinuxGitShell::GitFileState, workTreeState);

    const QByteArray output =
        trackedRecord(QByteArrayLiteral("1 ") + statusCode + QByteArrayLiteral(" N..."),
                      QByteArrayLiteral("file.txt"));
    const auto result = LinuxGitShell::GitStatusParser::parse(output);
    QCOMPARE(result.error, LinuxGitShell::GitStatusParseError::None);
    const auto status = result.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    QCOMPARE(status.entries.size(), 1);
    QCOMPARE(status.entries.constFirst().indexState, indexState);
    QCOMPARE(status.entries.constFirst().workTreeState, workTreeState);
}

void GitStatusParserTest::parsesUntrackedIgnoredAndUnusualPaths()
{
    const QByteArray untrackedPath = QByteArrayLiteral("-leading path/ünicode\nline.txt");
    const QByteArray ignoredPath = QByteArrayLiteral("build output/cache.bin");
    QByteArray output = QByteArrayLiteral("? ") + untrackedPath + '\0';
    output += QByteArrayLiteral("! ") + ignoredPath + '\0';

    const auto result = LinuxGitShell::GitStatusParser::parse(output);
    QCOMPARE(result.error, LinuxGitShell::GitStatusParseError::None);
    const auto status = result.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    QCOMPARE(status.entries.size(), 2);
    QCOMPARE(status.entries.at(0).kind, LinuxGitShell::GitStatusRecordKind::Untracked);
    QCOMPARE(status.entries.at(0).workTreeState, LinuxGitShell::GitFileState::Untracked);
    QCOMPARE(status.entries.at(0).path, QString::fromUtf8(untrackedPath));
    QCOMPARE(status.entries.at(1).kind, LinuxGitShell::GitStatusRecordKind::Ignored);
    QCOMPARE(status.entries.at(1).workTreeState, LinuxGitShell::GitFileState::Ignored);
    QCOMPARE(status.entries.at(1).path, QString::fromUtf8(ignoredPath));

    const auto ignoredOnly =
        LinuxGitShell::GitStatusParser::parse(QByteArrayLiteral("! generated/cache.bin\0"));
    QVERIFY(ignoredOnly.status.value_or(LinuxGitShell::GitStatusSnapshot{}).isClean());
}

void GitStatusParserTest::parsesRenameAndCopyEntries()
{
    QByteArray output = trackedRecord(QByteArrayLiteral("2 R. N..."),
                                      QByteArrayLiteral("R087 destination name.txt"));
    output.chop(1);
    output += QByteArrayLiteral("\0original name.txt\0");
    output += trackedRecord(QByteArrayLiteral("2 .C N..."),
                            QByteArrayLiteral("C100 copied destination.txt"));
    output.chop(1);
    output += QByteArrayLiteral("\0copy source.txt\0");

    const auto result = LinuxGitShell::GitStatusParser::parse(output);
    QCOMPARE(result.error, LinuxGitShell::GitStatusParseError::None);
    const auto status = result.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    QCOMPARE(status.entries.size(), 2);
    const auto& renamed = status.entries.at(0);
    QCOMPARE(renamed.indexState, LinuxGitShell::GitFileState::Renamed);
    QCOMPARE(renamed.path, QStringLiteral("destination name.txt"));
    QCOMPARE(renamed.originalPath, QStringLiteral("original name.txt"));
    QCOMPARE(renamed.similarityScore.value_or(-1), 87);
    const auto& copied = status.entries.at(1);
    QCOMPARE(copied.workTreeState, LinuxGitShell::GitFileState::Copied);
    QCOMPARE(copied.path, QStringLiteral("copied destination.txt"));
    QCOMPARE(copied.originalPath, QStringLiteral("copy source.txt"));
    QCOMPARE(copied.similarityScore.value_or(-1), 100);
}

void GitStatusParserTest::parsesUnmergedEntryAndStageMetadata()
{
    QByteArray output = QByteArrayLiteral("u UU N... 100644 100644 100644 100644 ") +
                        objectId('a') + ' ' + objectId('b') + ' ' + objectId('c') +
                        QByteArrayLiteral(" conflicted.txt\0");

    const auto result = LinuxGitShell::GitStatusParser::parse(output);
    QCOMPARE(result.error, LinuxGitShell::GitStatusParseError::None);
    const auto status = result.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    QCOMPARE(status.entries.size(), 1);
    const auto& entry = status.entries.constFirst();
    QCOMPARE(entry.kind, LinuxGitShell::GitStatusRecordKind::Unmerged);
    QVERIFY(entry.isConflicted());
    QCOMPARE(entry.indexState, LinuxGitShell::GitFileState::Unmerged);
    QCOMPARE(entry.workTreeState, LinuxGitShell::GitFileState::Unmerged);
    QCOMPARE(entry.modes.size(), 4);
    QCOMPARE(entry.objectIds,
             QList<QString>({QString::fromLatin1(objectId('a')), QString::fromLatin1(objectId('b')),
                             QString::fromLatin1(objectId('c'))}));
}

void GitStatusParserTest::parsesUnmergedStates_data()
{
    QTest::addColumn<QByteArray>("statusCode");
    QTest::addColumn<LinuxGitShell::GitFileState>("indexState");
    QTest::addColumn<LinuxGitShell::GitFileState>("workTreeState");

    QTest::newRow("both-deleted") << QByteArrayLiteral("DD") << LinuxGitShell::GitFileState::Deleted
                                  << LinuxGitShell::GitFileState::Deleted;
    QTest::newRow("added-by-us") << QByteArrayLiteral("AU") << LinuxGitShell::GitFileState::Added
                                 << LinuxGitShell::GitFileState::Unmerged;
    QTest::newRow("deleted-by-them")
        << QByteArrayLiteral("UD") << LinuxGitShell::GitFileState::Unmerged
        << LinuxGitShell::GitFileState::Deleted;
    QTest::newRow("added-by-them")
        << QByteArrayLiteral("UA") << LinuxGitShell::GitFileState::Unmerged
        << LinuxGitShell::GitFileState::Added;
    QTest::newRow("deleted-by-us")
        << QByteArrayLiteral("DU") << LinuxGitShell::GitFileState::Deleted
        << LinuxGitShell::GitFileState::Unmerged;
    QTest::newRow("both-added") << QByteArrayLiteral("AA") << LinuxGitShell::GitFileState::Added
                                << LinuxGitShell::GitFileState::Added;
    QTest::newRow("both-modified")
        << QByteArrayLiteral("UU") << LinuxGitShell::GitFileState::Unmerged
        << LinuxGitShell::GitFileState::Unmerged;
}

void GitStatusParserTest::parsesUnmergedStates()
{
    QFETCH(QByteArray, statusCode);
    QFETCH(LinuxGitShell::GitFileState, indexState);
    QFETCH(LinuxGitShell::GitFileState, workTreeState);

    const QByteArray output = QByteArrayLiteral("u ") + statusCode +
                              QByteArrayLiteral(" N... 100644 100644 100644 100644 ") +
                              objectId('a') + ' ' + objectId('b') + ' ' + objectId('c') +
                              QByteArrayLiteral(" conflict.txt\0");
    const auto result = LinuxGitShell::GitStatusParser::parse(output);
    QCOMPARE(result.error, LinuxGitShell::GitStatusParseError::None);
    const auto status = result.status.value_or(LinuxGitShell::GitStatusSnapshot{});
    QCOMPARE(status.entries.size(), 1);
    QCOMPARE(status.entries.constFirst().indexState, indexState);
    QCOMPARE(status.entries.constFirst().workTreeState, workTreeState);
    QVERIFY(status.entries.constFirst().isConflicted());
}

void GitStatusParserTest::parsesSubmoduleState()
{
    const QByteArray output =
        trackedRecord(QByteArrayLiteral("1 .M SCMU"), QByteArrayLiteral("module"));
    const auto result = LinuxGitShell::GitStatusParser::parse(output);
    QCOMPARE(result.error, LinuxGitShell::GitStatusParseError::None);
    const auto entry =
        result.status.value_or(LinuxGitShell::GitStatusSnapshot{}).entries.constFirst();
    QVERIFY(entry.submodule.isSubmodule);
    QVERIFY(entry.submodule.commitChanged);
    QVERIFY(entry.submodule.trackedChanges);
    QVERIFY(entry.submodule.untrackedChanges);
}

void GitStatusParserTest::ignoresUnknownHeaders()
{
    const auto result =
        LinuxGitShell::GitStatusParser::parse(QByteArrayLiteral("# future.header value\0"));
    QCOMPARE(result.error, LinuxGitShell::GitStatusParseError::None);
    QVERIFY(result.status.has_value());
}

void GitStatusParserTest::rejectsMalformedInput_data()
{
    QTest::addColumn<QByteArray>("output");
    QTest::addColumn<LinuxGitShell::GitStatusParseError>("error");

    QTest::newRow("missing-terminator")
        << QByteArrayLiteral("? file.txt") << LinuxGitShell::GitStatusParseError::MissingTerminator;
    QTest::newRow("empty-record") << QByteArrayLiteral("? file.txt\0\0")
                                  << LinuxGitShell::GitStatusParseError::MalformedRecord;
    QTest::newRow("unsupported-record")
        << QByteArrayLiteral("x data\0") << LinuxGitShell::GitStatusParseError::UnsupportedRecord;
    QTest::newRow("bad-status") << trackedRecord(QByteArrayLiteral("1 X. N..."),
                                                 QByteArrayLiteral("file.txt"))
                                << LinuxGitShell::GitStatusParseError::InvalidStatusCode;
    QTest::newRow("bad-submodule")
        << trackedRecord(QByteArrayLiteral("1 M. SXYZ"), QByteArrayLiteral("file.txt"))
        << LinuxGitShell::GitStatusParseError::InvalidSubmoduleState;
    QTest::newRow("bad-mode") << (QByteArrayLiteral("1 M. N... 10x644 100644 100644 ") +
                                  objectId('a') + ' ' + objectId('b') +
                                  QByteArrayLiteral(" file.txt\0"))
                              << LinuxGitShell::GitStatusParseError::InvalidMetadata;
    QTest::newRow("bad-header") << QByteArrayLiteral("# branch.ab +one -2\0")
                                << LinuxGitShell::GitStatusParseError::InvalidHeader;
    QTest::newRow("missing-original-path")
        << trackedRecord(QByteArrayLiteral("2 R. N..."), QByteArrayLiteral("R100 destination.txt"))
        << LinuxGitShell::GitStatusParseError::MissingOriginalPath;
    QByteArray invalidScore =
        trackedRecord(QByteArrayLiteral("2 R. N..."), QByteArrayLiteral("R101 destination.txt"));
    invalidScore += QByteArrayLiteral("original.txt\0");
    QTest::newRow("bad-rename-score")
        << invalidScore << LinuxGitShell::GitStatusParseError::InvalidRenameScore;
}

void GitStatusParserTest::rejectsMalformedInput()
{
    QFETCH(QByteArray, output);
    QFETCH(LinuxGitShell::GitStatusParseError, error);

    const auto result = LinuxGitShell::GitStatusParser::parse(output);
    QCOMPARE(result.error, error);
    QVERIFY(!result.status.has_value());
    QVERIFY(result.recordIndex >= 0);
}

QTEST_GUILESS_MAIN(GitStatusParserTest)

#include "GitStatusParserTest.moc"
