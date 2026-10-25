#include "core/HardwareInfo.h"
#include "core/SystemInfo.h"

#include <QTemporaryDir>
#include <QtTest>

namespace {
void write(const QString &path, const QByteArray &content) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}
} // namespace

class TstHardwareInfo : public QObject {
    Q_OBJECT
private slots:
    void aDiskBelongsOnlyToItsOwnController() {
        QVERIFY(HardwareInfo::diskBelongsToController("nvme1n1", "nvme1"));
        QVERIFY(HardwareInfo::diskBelongsToController("nvme1n1p2", "nvme1"));
        QVERIFY(HardwareInfo::diskBelongsToController("sda", "sda"));
        QVERIFY(!HardwareInfo::diskBelongsToController("nvme10n1", "nvme1")); // a different controller
        QVERIFY(!HardwareInfo::diskBelongsToController("nvme1n1", "nvme10"));
        QVERIFY(!HardwareInfo::diskBelongsToController("sdab", "sda"));
        QVERIFY(!HardwareInfo::diskBelongsToController("nvme0n1", ""));
    }

    void netCountersAreTheChangingPartOfNet() {
        QTemporaryDir dir;
        const QString base = dir.path() + "/eth0";
        QVERIFY(QDir().mkpath(base + "/statistics"));
        auto put = [](const QString &path, const QString &text) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(text.toUtf8());
        };
        put(base + "/mtu", "1500\n");
        put(base + "/address", "aa:bb:cc:dd:ee:ff\n");
        put(base + "/statistics/rx_bytes", "1000\n");
        const auto c = HardwareInfo::netCounters("eth0", dir.path());
        QCOMPARE(c.mtu, 1500);
        QCOMPARE(c.rxBytes, qint64(1000));
        QVERIFY(c.mac.isEmpty());      // the static facts are not read here
        QVERIFY(c.busAddress.isEmpty());
        QCOMPARE(HardwareInfo::net("eth0", dir.path()).mac, QString("aa:bb:cc:dd:ee:ff"));
        QCOMPARE(HardwareInfo::net("eth0", dir.path()).rxBytes, qint64(1000));
    }

    void diskDetails() {
        QTemporaryDir dir;
        write(dir.path() + "/nvme0n1/device/serial", "SN123 \n");
        write(dir.path() + "/nvme0n1/device/firmware_rev", "FW9\n");
        write(dir.path() + "/nvme0n1/queue/logical_block_size", "512\n");
        const auto d = HardwareInfo::disk("nvme0n1", dir.path());
        QCOMPARE(d.serial, QString("SN123"));
        QCOMPARE(d.firmware, QString("FW9"));
        QCOMPARE(d.interfaceName, QString("NVMe"));
        QCOMPARE(d.logicalBlockSize, 512);

        const auto none = HardwareInfo::disk("sdz", dir.path());
        QVERIFY(none.serial.isEmpty());
        QCOMPARE(none.logicalBlockSize, -1);
    }

    void partitionsMapToTheirDisk() {
        QTemporaryDir dir;
        // /sys/block/nvme0n1/nvme0n1p2/partition, and /sys/class/block/<name> -> symlinks
        write(dir.path() + "/devices/nvme0n1/nvme0n1p2/partition", "2\n");
        QDir().mkpath(dir.path() + "/devices/nvme0n1");
        QDir().mkpath(dir.path() + "/class");
        QVERIFY(QFile::link(dir.path() + "/devices/nvme0n1/nvme0n1p2", dir.path() + "/class/nvme0n1p2"));
        QVERIFY(QFile::link(dir.path() + "/devices/nvme0n1", dir.path() + "/class/nvme0n1"));
        QCOMPARE(HardwareInfo::parentDisk("nvme0n1p2", dir.path() + "/class"), QString("nvme0n1"));
        QCOMPARE(HardwareInfo::parentDisk("nvme0n1", dir.path() + "/class"), QString("nvme0n1"));
        QCOMPARE(HardwareInfo::parentDisk("unknown", dir.path() + "/class"), QString("unknown"));

        const auto usage = HardwareInfo::diskUsage(
            "nvme0n1",
            {{"/dev/nvme0n1p2", 1000, 400}, {"/dev/nvme0n1p2", 1000, 400}, // bind mount: counted once
             {"/dev/sda1", 5000, 0}, {"tmpfs", 10, 10}},
            dir.path() + "/class");
        QCOMPARE(usage.usedBytes, qint64(600));
        QCOMPARE(usage.totalBytes, qint64(1000));

        const auto none = HardwareInfo::diskUsage("nvme1n1", {{"/dev/nvme0n1p2", 1000, 400}}, dir.path() + "/class");
        QCOMPARE(none.usedBytes, qint64(-1)); // unknown, not zero
    }

    void netDetails() {
        QTemporaryDir dir;
        const QString eth = dir.path() + "/eth0";
        write(eth + "/address", "aa:bb:cc:dd:ee:ff\n");
        write(eth + "/mtu", "1500\n");
        write(eth + "/duplex", "full\n");
        write(eth + "/statistics/rx_bytes", "1000\n");
        write(eth + "/statistics/tx_bytes", "2000\n");
        write(eth + "/statistics/rx_packets", "10\n");
        write(eth + "/statistics/tx_packets", "20\n");
        write(eth + "/statistics/rx_errors", "1\n");
        write(eth + "/statistics/tx_errors", "2\n");
        write(dir.path() + "/pci/0000:03:00.0/vendor", "0x8086\n");
        write(dir.path() + "/pci/0000:03:00.0/device", "0x15F3\n");
        QDir().mkpath(dir.path() + "/drv/igc");
        QVERIFY(QFile::link(dir.path() + "/pci/0000:03:00.0", eth + "/device"));
        QVERIFY(QFile::link(dir.path() + "/drv/igc", dir.path() + "/pci/0000:03:00.0/driver"));

        const auto d = HardwareInfo::net("eth0", dir.path());
        QCOMPARE(d.mac, QString("aa:bb:cc:dd:ee:ff"));
        QCOMPARE(d.mtu, 1500);
        QCOMPARE(d.rxBytes, qint64(1000));
        QCOMPARE(d.txPackets, qint64(20));
        QCOMPARE(d.errors, qint64(3));
        QCOMPARE(d.vendorId, QString("8086"));
        QCOMPARE(d.deviceId, QString("15f3"));
        QCOMPARE(d.busAddress, QString("0000:03:00.0"));
        QCOMPARE(d.driver, QString("igc"));

        const auto missing = HardwareInfo::net("nope", dir.path());
        QCOMPARE(missing.rxBytes, qint64(-1));
        QVERIFY(missing.mac.isEmpty());
    }

    void pciIdsLookup() {
        QTemporaryDir dir;
        write(dir.path() + "/pci.ids",
              "# comment\n"
              "8085  Other Corp\n\t0001  Not this one\n"
              "8086  Intel Corporation\n\t15f3  Ethernet Controller I225-V\n\t\t1043 8672  Subsystem\n\t7af0  Wi-Fi 6E AX211\n"
              "8087  Next Vendor\n\t15f3  Wrong device\n");
        const QString ids = dir.path() + "/pci.ids";
        auto r = HardwareInfo::pciNames("8086", "15F3", ids);
        QCOMPARE(r.first, QString("Intel Corporation"));
        QCOMPARE(r.second, QString("Ethernet Controller I225-V"));
        r = HardwareInfo::pciNames("8086", "9999", ids);
        QCOMPARE(r.first, QString("Intel Corporation"));
        QVERIFY(r.second.isEmpty());
        QVERIFY(HardwareInfo::pciNames("ffff", "0001", ids).first.isEmpty());
        QVERIFY(HardwareInfo::pciNames("", "", ids).first.isEmpty());
    }

    void cpuBaseFrequencyAndPowerLimit() {
        QTemporaryDir dir;
        write(dir.path() + "/proc/cpuinfo", "processor : 0\n");
        write(dir.path() + "/cpu/cpu0/cpufreq/base_frequency", "2200000\n");
        write(dir.path() + "/pc/intel-rapl:0/constraint_0_power_limit_uw", "55000000\n");
        const SystemInfo i = SystemInfo::read(dir.path() + "/proc", dir.path() + "/cpu", dir.path() + "/pc");
        QCOMPARE(i.baseMhz, 2200.0);
        QCOMPARE(i.powerLimitW, 55.0);
        const SystemInfo none = SystemInfo::read("/nonexistent", "/nonexistent", "/nonexistent");
        QCOMPARE(none.baseMhz, -1.0);
        QCOMPARE(none.powerLimitW, -1.0);
    }
};

QTEST_GUILESS_MAIN(TstHardwareInfo)
#include "tst_hardwareinfo.moc"
