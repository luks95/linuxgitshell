// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/repositorycontext/OverlayWire.h"

#include <QTest>

using namespace LinuxGitShell;
using State = RepositoryStatusState;
using Validity = RepositoryStatusValidity;

namespace
{

[[nodiscard]] OverlayRecord ready(const QString& path, State state = State::Modified)
{
    OverlayRecord record;
    record.path = path;
    record.repositoryRoot = QStringLiteral("/repo");
    record.state = state;
    record.validity = Validity::Ready;
    record.revision = 42;
    record.validForMs = 12345;
    return record;
}

} // namespace

class OverlayWireTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void roundTripsEveryStateAndValidity_data();
    void roundTripsEveryStateAndValidity();
    void encodesTypedFields();
    void nonReadyRecordsAreNormalizedToUnknown();
    void rejectsMissingOrMistypedRequiredFields_data();
    void rejectsMissingOrMistypedRequiredFields();
    void rejectsUnknownEnumerationValues();
    void ignoresUnknownOptionalKeys();
    void splitsByCountAndEncodedSize();
};

void OverlayWireTest::roundTripsEveryStateAndValidity_data()
{
    QTest::addColumn<int>("state");
    QTest::addColumn<int>("validity");
    for (int state = static_cast<int>(State::Unknown); state <= static_cast<int>(State::Conflict);
         ++state)
    {
        QTest::addRow("state %d", state) << state << static_cast<int>(Validity::Ready);
    }
    for (const Validity validity :
         {Validity::Pending, Validity::Stale, Validity::Degraded, Validity::Unavailable})
    {
        QTest::addRow("validity %d", static_cast<int>(validity))
            << static_cast<int>(State::Unknown) << static_cast<int>(validity);
    }
}

void OverlayWireTest::roundTripsEveryStateAndValidity()
{
    QFETCH(int, state);
    QFETCH(int, validity);
    OverlayRecord record = ready(QStringLiteral("/repo/file"));
    record.state = static_cast<State>(state);
    record.validity = static_cast<Validity>(validity);
    if (record.validity != Validity::Ready)
    {
        record.validForMs = 0;
    }

    const auto decoded = overlayRecordFromVariantMap(overlayRecordToVariantMap(record));
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->path, record.path);
    QCOMPARE(decoded->repositoryRoot, record.repositoryRoot);
    QCOMPARE(decoded->state, record.state);
    QCOMPARE(decoded->validity, record.validity);
    QCOMPARE(decoded->revision, record.revision);
    QCOMPARE(decoded->validForMs, record.validForMs);
}

void OverlayWireTest::encodesTypedFields()
{
    const QVariantMap map = overlayRecordToVariantMap(ready(QStringLiteral("/repo/f")));
    QCOMPARE(map.value(QStringLiteral("state")).toString(), QStringLiteral("modified"));
    QCOMPARE(map.value(QStringLiteral("validity")).toString(), QStringLiteral("ready"));
    // D-Bus types: revision is `t` and validForMs is `u`.
    QCOMPARE(map.value(QStringLiteral("revision")).typeId(), QMetaType::ULongLong);
    QCOMPARE(map.value(QStringLiteral("validForMs")).typeId(), QMetaType::UInt);
}

void OverlayWireTest::nonReadyRecordsAreNormalizedToUnknown()
{
    // A hostile or buggy peer must not preserve a Clean assertion without Ready evidence.
    QVariantMap map = overlayRecordToVariantMap(ready(QStringLiteral("/repo/f"), State::Clean));
    map.insert(QStringLiteral("validity"), QStringLiteral("stale"));
    const auto decoded = overlayRecordFromVariantMap(map);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->state, State::Unknown);
    QCOMPARE(decoded->validity, Validity::Stale);
    QCOMPARE(decoded->validForMs, quint32(0));
}

void OverlayWireTest::rejectsMissingOrMistypedRequiredFields_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<bool>("remove");
    for (const QString& key :
         {QStringLiteral("path"), QStringLiteral("repositoryRoot"), QStringLiteral("state"),
          QStringLiteral("validity"), QStringLiteral("revision"), QStringLiteral("validForMs")})
    {
        QTest::addRow("missing %s", qPrintable(key)) << key << true;
        QTest::addRow("mistyped %s", qPrintable(key)) << key << false;
    }
}

void OverlayWireTest::rejectsMissingOrMistypedRequiredFields()
{
    QFETCH(QString, key);
    QFETCH(bool, remove);
    QVariantMap map = overlayRecordToVariantMap(ready(QStringLiteral("/repo/f")));
    if (remove)
    {
        map.remove(key);
    }
    else
    {
        // An int for the string fields, a string for the numeric ones, and a signed number where
        // an unsigned one is required.
        const bool numeric =
            key == QStringLiteral("revision") || key == QStringLiteral("validForMs");
        map.insert(key, numeric ? QVariant(QStringLiteral("x")) : QVariant(7));
    }
    QVERIFY(!overlayRecordFromVariantMap(map).has_value());

    QVariantMap signedNumber = overlayRecordToVariantMap(ready(QStringLiteral("/repo/f")));
    signedNumber.insert(QStringLiteral("revision"), qint64(5));
    QVERIFY(!overlayRecordFromVariantMap(signedNumber).has_value());
}

void OverlayWireTest::rejectsUnknownEnumerationValues()
{
    QVariantMap map = overlayRecordToVariantMap(ready(QStringLiteral("/repo/f")));
    map.insert(QStringLiteral("state"), QStringLiteral("sparkling"));
    QVERIFY(!overlayRecordFromVariantMap(map).has_value());

    map = overlayRecordToVariantMap(ready(QStringLiteral("/repo/f")));
    map.insert(QStringLiteral("validity"), QStringLiteral("eventually"));
    QVERIFY(!overlayRecordFromVariantMap(map).has_value());

    map = overlayRecordToVariantMap(ready(QStringLiteral("/repo/f")));
    map.insert(QStringLiteral("path"), QString());
    QVERIFY(!overlayRecordFromVariantMap(map).has_value());
}

void OverlayWireTest::ignoresUnknownOptionalKeys()
{
    QVariantMap map = overlayRecordToVariantMap(ready(QStringLiteral("/repo/f")));
    map.insert(QStringLiteral("futureField"), QStringLiteral("whatever"));
    const auto decoded = overlayRecordFromVariantMap(map);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->state, State::Modified);
}

void OverlayWireTest::splitsByCountAndEncodedSize()
{
    QVERIFY(splitOverlayRecords({}, 256, 1000000).isEmpty());

    QList<OverlayRecord> records;
    for (int index = 0; index < 5; ++index)
    {
        records.append(ready(QStringLiteral("/repo/%1").arg(index)));
    }
    auto batches = splitOverlayRecords(records, 2, 1000000);
    QCOMPARE(batches.size(), 3);
    QCOMPARE(batches.at(0).size(), 2);
    QCOMPARE(batches.at(2).size(), 1);
    QCOMPARE(batches.at(2).constFirst().path, QStringLiteral("/repo/4"));

    // Long multibyte paths are budgeted by the worst-case UTF-8 size.
    QList<OverlayRecord> longRecords;
    for (int index = 0; index < 4; ++index)
    {
        longRecords.append(ready(QStringLiteral("/") + QString(4000, QChar(0x20AC))));
    }
    batches = splitOverlayRecords(longRecords, 256, 30000);
    QCOMPARE(batches.size(), 2);
    for (const auto& batch : batches)
    {
        qsizetype bytes = 0;
        for (const OverlayRecord& record : batch)
        {
            bytes += overlayRecordEncodedSizeEstimate(record);
        }
        QVERIFY(bytes <= 30000);
        QVERIFY(!batch.isEmpty());
    }

    // An oversized single record still travels alone rather than being dropped.
    batches = splitOverlayRecords({longRecords.constFirst()}, 256, 100);
    QCOMPARE(batches.size(), 1);
    QCOMPARE(batches.constFirst().size(), 1);
}

QTEST_GUILESS_MAIN(OverlayWireTest)
#include "OverlayWireTest.moc"
