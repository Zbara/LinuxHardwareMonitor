#include "core/DiskHealth.h"

#include <QtTest>

using namespace DiskHealthParser;

namespace {

InterfaceMap driveWith(const QString &extraIface, const QVariantMap &props) {
    InterfaceMap m;
    m["org.freedesktop.UDisks2.Drive"] = {{"Model", "Test"}};
    if (!extraIface.isEmpty())
        m[extraIface] = props;
    return m;
}

InterfaceMap block(const QString &drivePath, bool partition = false) {
    InterfaceMap m;
    m["org.freedesktop.UDisks2.Block"] = {{"Drive", QVariant::fromValue(QDBusObjectPath(drivePath))}};
    if (partition)
        m["org.freedesktop.UDisks2.Partition"] = {};
    return m;
}

const QVariantMap nvmeProps{{"SmartPowerOnHours", qulonglong(1741)}, {"SmartTemperature", ushort(338)},
                            {"SmartCriticalWarning", QStringList()}, {"SmartSelftestStatus", "success"}};
const QVariantMap nvmeAttrs{{"percent_used", uchar(8)}, {"avail_spare", uchar(100)}, {"spare_thresh", uchar(5)},
                            {"total_data_read", qulonglong(20247285760000)}, {"total_data_written", qulonglong(42921840128000)},
                            {"power_cycles", qulonglong(1966)}, {"unsafe_shutdowns", qulonglong(595)},
                            {"media_errors", qulonglong(0)}, {"num_err_log_entries", qulonglong(411)}};

} // namespace

class TstDiskHealth : public QObject {
    Q_OBJECT
private slots:
    void findsWholeDisksWithSmartAndSkipsTheRest() {
        ObjectMap o;
        o[QDBusObjectPath("/org/freedesktop/UDisks2/drives/Nvme_1")] = driveWith("org.freedesktop.UDisks2.NVMe.Controller", nvmeProps);
        o[QDBusObjectPath("/org/freedesktop/UDisks2/drives/Sata_1")] = driveWith("org.freedesktop.UDisks2.Drive.Ata", {{"SmartSupported", true}, {"SmartEnabled", true}});
        o[QDBusObjectPath("/org/freedesktop/UDisks2/drives/Cdrom_1")] = driveWith("", {}); // no SMART interface
        o[QDBusObjectPath("/org/freedesktop/UDisks2/block_devices/nvme0n1")] = block("/org/freedesktop/UDisks2/drives/Nvme_1");
        o[QDBusObjectPath("/org/freedesktop/UDisks2/block_devices/nvme0n1p1")] = block("/org/freedesktop/UDisks2/drives/Nvme_1", true);
        o[QDBusObjectPath("/org/freedesktop/UDisks2/block_devices/sda")] = block("/org/freedesktop/UDisks2/drives/Sata_1");
        o[QDBusObjectPath("/org/freedesktop/UDisks2/block_devices/sr0")] = block("/org/freedesktop/UDisks2/drives/Cdrom_1");
        o[QDBusObjectPath("/org/freedesktop/UDisks2/block_devices/loop0")] = block("/");

        QStringList names;
        for (const Drive &d : drives(o))
            names << d.blockName;
        names.sort();
        QCOMPARE(names, (QStringList{"nvme0n1", "sda"})); // no partition, no CD-ROM, no loop device
    }

    void buildsAnNvmeRecord() {
        Drive d;
        d.nvme = nvmeProps;
        const DiskHealth h = build(d, nvmeAttrs);
        QVERIFY(h.available());
        QCOMPARE(h.kind, DiskHealth::Kind::Nvme);
        QCOMPARE(h.powerOnHours, qint64(1741));
        QVERIFY(qAbs(h.temperatureC - 64.85) < 0.01);   // 338 K
        QCOMPARE(h.percentUsed, 8);
        QCOMPARE(h.availableSpare, 100);
        QCOMPARE(h.dataWrittenBytes, qint64(42921840128000));
        QCOMPARE(h.unsafeShutdowns, qint64(595));
        QCOMPARE(h.mediaErrors, qint64(0));
        QCOMPARE(h.selfTestStatus, QString("success"));
        QCOMPARE(h.status(), DiskHealth::Status::Good);
    }

    void missingAttributesStayUnknownNotZero() {
        Drive d;
        d.nvme = nvmeProps;
        const DiskHealth h = build(d, {}); // SmartGetAttributes was refused or failed
        QCOMPARE(h.percentUsed, -1);
        QCOMPARE(h.dataWrittenBytes, qint64(-1));
        QCOMPARE(h.mediaErrors, qint64(-1));
        QCOMPARE(h.powerOnHours, qint64(1741)); // the properties still count
    }

    void statusFollowsTheWarnings() {
        Drive d;
        d.nvme = nvmeProps;
        QVariantMap attrs = nvmeAttrs;
        attrs["avail_spare"] = uchar(3); // below the threshold of 5
        QCOMPARE(build(d, attrs).status(), DiskHealth::Status::Warning);
        attrs = nvmeAttrs;
        attrs["percent_used"] = uchar(100);
        QCOMPARE(build(d, attrs).status(), DiskHealth::Status::Warning);
        d.nvme["SmartCriticalWarning"] = QStringList{"temperature"};
        QCOMPARE(build(d, nvmeAttrs).status(), DiskHealth::Status::Warning);
        QCOMPARE(DiskHealth().status(), DiskHealth::Status::Unknown);
    }

    void buildsAnAtaRecord() {
        Drive d;
        d.ata = {{"SmartSupported", true}, {"SmartEnabled", true}, {"SmartFailing", false},
                 {"SmartPowerOnSeconds", qulonglong(36000)}, {"SmartTemperature", 310.0}, {"SmartNumBadSectors", qlonglong(0)}};
        DiskHealth h = build(d, {});
        QCOMPARE(h.kind, DiskHealth::Kind::Ata);
        QCOMPARE(h.powerOnHours, qint64(10));
        QVERIFY(qAbs(h.temperatureC - 36.85) < 0.01);
        QCOMPARE(h.status(), DiskHealth::Status::Good);
        d.ata["SmartFailing"] = true;
        QCOMPARE(build(d, {}).status(), DiskHealth::Status::Failing);
        d.ata["SmartFailing"] = false;
        d.ata["SmartNumBadSectors"] = qlonglong(4);
        QCOMPARE(build(d, {}).status(), DiskHealth::Status::Warning);
    }

    void ataWithoutSmartHasNoHealth() {
        Drive d;
        d.ata = {{"SmartSupported", true}, {"SmartEnabled", false}};
        QVERIFY(!build(d, {}).available());
        d.ata = {{"SmartSupported", false}, {"SmartEnabled", false}};
        QVERIFY(!build(d, {}).available());
    }
};

QTEST_GUILESS_MAIN(TstDiskHealth)
#include "tst_diskhealth.moc"
