#include "core/DmiMemory.h"

#include <QSignalSpy>
#include <QtTest>

namespace {
const QString kSample = QStringLiteral(QT_TESTCASE_SOURCEDIR) + "/data/dmidecode-memory.txt";

QString sampleText() {
    QFile f(kSample);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}
} // namespace

class TstDmi : public QObject {
    Q_OBJECT
private slots:
    void parsesSlotsModulesAndSpeeds() {
        const MemoryInfo info = DmiMemory::parse(sampleText());
        QVERIFY(info.valid());
        QCOMPARE(info.slotsTotal, 4);
        QCOMPARE(info.slotsUsed(), 2); // the two "No Module Installed" slots are not modules
        QCOMPARE(info.errorCorrection, QString("None"));

        const MemoryModule &a = info.modules[0];
        QCOMPARE(a.locator, QString("Controller0-ChannelA-DIMM0"));
        QCOMPARE(a.sizeMb, qint64(16 * 1024));
        QCOMPARE(a.type, QString("DDR5"));
        QCOMPARE(a.typeDetail, QString("Synchronous"));
        QCOMPARE(a.formFactor, QString("SODIMM"));
        QCOMPARE(a.speedMts, 5200);       // configured speed wins over the rated 5600
        QCOMPARE(a.ratedSpeedMts, 5600);
        QCOMPARE(a.manufacturer, QString("Micron Technology"));
        QCOMPARE(a.partNumber, QString("MTC8C1084S1SC56B"));
        QCOMPARE(a.rank, 1);

        const MemoryModule &b = info.modules[1];
        QCOMPARE(b.sizeMb, qint64(8192));  // given in MB
        QCOMPARE(b.speedMts, 5600);        // no configured speed: falls back to the rated one
    }

    void placeholdersBecomeEmptyNotText() {
        const MemoryInfo info = DmiMemory::parse(
            "Memory Device\n\tSize: 4 GB\n\tLocator: DIMM0\n\tManufacturer: Unknown\n\tPart Number: To Be Filled By O.E.M.\n"
            "\tType: DDR3\n\tSpeed: Unknown\n");
        QCOMPARE(info.modules.size(), 1);
        QVERIFY(info.modules[0].manufacturer.isEmpty());
        QVERIFY(info.modules[0].partNumber.isEmpty());
        QCOMPARE(info.modules[0].speedMts, 0); // unknown, not invented
        QCOMPARE(info.slotsTotal, 1);          // no array block: the listed device count
    }

    void garbageIsInvalid() {
        QVERIFY(!DmiMemory::parse("").valid());
        QVERIFY(!DmiMemory::parse("/dev/mem: Permission denied\n").valid());
        QVERIFY(!DmiMemory::parse("# dmidecode 3.6\nNo SMBIOS nor DMI entry point found, sorry.\n").valid());
    }

    void readerDeliversParsedInfo() {
        DmiMemoryReader reader("/bin/cat", {kSample});
        QSignalSpy ok(&reader, &DmiMemoryReader::finished);
        QSignalSpy bad(&reader, &DmiMemoryReader::failed);
        reader.start();
        QVERIFY(ok.wait(5000));
        QCOMPARE(bad.count(), 0);
        const auto info = ok.first().first().value<MemoryInfo>();
        QCOMPARE(info.slotsUsed(), 2);
        QCOMPARE(info.slotsTotal, 4);
    }

    void cancelledAuthenticationIsNotAnError() {
        DmiMemoryReader reader("/bin/sh", {"-c", "exit 126"}); // what pkexec returns when the dialog is dismissed
        QSignalSpy bad(&reader, &DmiMemoryReader::failed);
        reader.start();
        QVERIFY(bad.wait(5000));
        QCOMPARE(bad.first().first().value<DmiMemoryReader::Failure>(), DmiMemoryReader::Failure::Cancelled);
    }

    void failuresCarryTheCommandsMessage() {
        DmiMemoryReader reader("/bin/sh", {"-c", "echo 'dmidecode: no access' >&2; exit 1"});
        QSignalSpy bad(&reader, &DmiMemoryReader::failed);
        reader.start();
        QVERIFY(bad.wait(5000));
        QCOMPARE(bad.first().first().value<DmiMemoryReader::Failure>(), DmiMemoryReader::Failure::Failed);
        QVERIFY(bad.first().at(1).toString().contains("no access"));
    }

    void emptyOutputMeansNoData() {
        DmiMemoryReader reader("/bin/sh", {"-c", "echo 'No SMBIOS found'; exit 0"});
        QSignalSpy bad(&reader, &DmiMemoryReader::failed);
        reader.start();
        QVERIFY(bad.wait(5000));
        QCOMPARE(bad.first().first().value<DmiMemoryReader::Failure>(), DmiMemoryReader::Failure::NoData);
    }

    void missingProgramIsReportedAsNotAvailable() {
        DmiMemoryReader reader("/nonexistent/pkexec-like");
        QSignalSpy bad(&reader, &DmiMemoryReader::failed);
        reader.start();
        QVERIFY(bad.wait(5000));
        QCOMPARE(bad.count(), 1); // exactly one report, not one from errorOccurred and one from finished
        QCOMPARE(bad.first().first().value<DmiMemoryReader::Failure>(), DmiMemoryReader::Failure::NotAvailable);
    }
};

QTEST_GUILESS_MAIN(TstDmi)
#include "tst_dmi.moc"
