#include "core/SystemInfo.h"

#include <QtTest>

namespace {
void write(const QString &path, const QByteArray &content) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}
} // namespace

class TstSystemInfo : public QObject {
    Q_OBJECT
private slots:
    void readsFakeProc() {
        QTemporaryDir dir;
        write(dir.path() + "/cpuinfo",
              "processor\t: 0\nmodel name\t: Fake  CPU @ 3GHz\n\nprocessor\t: 1\nmodel name\t: Fake CPU @ 3GHz\n");
        write(dir.path() + "/uptime", "1234.56 5000.00\n");
        write(dir.path() + "/meminfo",
              "MemTotal: 8000000 kB\nMemAvailable: 2000000 kB\nBuffers: 100 kB\nCached: 900 kB\n"
              "SwapTotal: 4000 kB\nSwapFree: 1000 kB\nCommitLimit: 9000 kB\nCommitted_AS: 5000 kB\n"
              "SReclaimable: 70 kB\nSUnreclaim: 30 kB\nShmem: 11 kB\nDirty: 7 kB\n");
        write(dir.path() + "/loadavg", "0.50 1.25 2.00 1/200 999\n");
        write(dir.path() + "/sys/fs/file-nr", "12345\t0\t9223372036854775807\n");

        const SystemInfo i = SystemInfo::read(dir.path());
        QCOMPARE(i.cpuModel, QString("Fake CPU @ 3GHz"));
        QCOMPARE(i.logicalCpus, 2);
        QCOMPARE(i.uptimeSeconds, 1234.56);
        QCOMPARE(i.memTotalKb, qint64(8000000));
        QCOMPARE(i.memAvailableKb, qint64(2000000));
        QCOMPARE(i.memCachedKb, qint64(1000));
        QCOMPARE(i.swapTotalKb, qint64(4000));
        QCOMPARE(i.load5, 1.25);
        QCOMPARE(i.fileHandles, qint64(12345));
        QCOMPARE(i.commitLimitKb, qint64(9000));
        QCOMPARE(i.committedKb, qint64(5000));
        QCOMPARE(i.slabReclaimableKb, qint64(70));
        QCOMPARE(i.slabUnreclaimableKb, qint64(30));
        QCOMPARE(i.shmemKb, qint64(11));
        QCOMPARE(i.buffersKb, qint64(100));
        QCOMPARE(i.dirtyKb, qint64(7));
    }

    void cpuTopologyAndLimits() {
        QTemporaryDir dir;
        write(dir.path() + "/proc/cpuinfo",
              "processor : 0\nphysical id : 0\ncpu cores : 8\nflags : fpu vmx sse\n"
              "processor : 1\nphysical id : 0\ncpu cores : 8\nflags : fpu vmx sse\n");
        write(dir.path() + "/cpu/cpu0/cpufreq/cpuinfo_max_freq", "5400000\n");
        const SystemInfo i = SystemInfo::read(dir.path() + "/proc", dir.path() + "/cpu");
        QCOMPARE(i.sockets, 1);
        QCOMPARE(i.physicalCores, 8);
        QCOMPARE(i.maxMhz, 5400.0);
        QVERIFY(i.virtualization);
    }

    void cacheSizes() {
        QCOMPARE(SystemInfo::parseCacheSizeKb("48K"), qint64(48));
        QCOMPARE(SystemInfo::parseCacheSizeKb("36M"), qint64(36 * 1024));
        QCOMPARE(SystemInfo::parseCacheSizeKb("junk"), qint64(-1));
        QCOMPARE(SystemInfo::parseCacheSizeKb(""), qint64(-1));

        QTemporaryDir dir;
        auto idx = [&](int i, const QByteArray &level, const QByteArray &type, const QByteArray &size) {
            const QString b = dir.path() + QString("/cpu/cpu0/cache/index%1/").arg(i);
            write(b + "level", level + "\n");
            write(b + "type", type + "\n");
            write(b + "size", size + "\n");
        };
        idx(0, "1", "Data", "48K");
        idx(1, "1", "Instruction", "32K");
        idx(2, "2", "Unified", "2048K");
        idx(3, "3", "Unified", "36864K");
        const SystemInfo i = SystemInfo::read(dir.path() + "/proc", dir.path() + "/cpu");
        QCOMPARE(i.l1Kb, qint64(80));
        QCOMPARE(i.l2Kb, qint64(2048));
        QCOMPARE(i.l3Kb, qint64(36864));
    }

    void diskAndNetFacts() {
        QTemporaryDir dir;
        write(dir.path() + "/block/sda/device/model", "Fake  Disk 3000  \n");
        write(dir.path() + "/block/sda/size", "2048\n");
        write(dir.path() + "/block/sda/queue/rotational", "1\n");
        QCOMPARE(SystemInfo::diskModel("sda", dir.path() + "/block"), QString("Fake Disk 3000"));
        QCOMPARE(SystemInfo::diskSizeBytes("sda", dir.path() + "/block"), qint64(1048576));
        QVERIFY(SystemInfo::diskIsRotational("sda", dir.path() + "/block"));
        QCOMPARE(SystemInfo::diskSizeBytes("nope", dir.path() + "/block"), qint64(-1));

        write(dir.path() + "/net/wlan0/phy80211/x", "");
        write(dir.path() + "/net/wlan0/operstate", "up\n");
        write(dir.path() + "/net/wlan0/speed", "-1\n");
        write(dir.path() + "/net/eth0/device/x", "");
        write(dir.path() + "/net/eth0/speed", "1000\n");
        write(dir.path() + "/net/docker0/operstate", "down\n");
        const QString net = dir.path() + "/net";
        QCOMPARE(SystemInfo::netKind("wlan0", net), QString("Wi-Fi"));
        QCOMPARE(SystemInfo::netKind("eth0", net), QString("Ethernet"));
        QCOMPARE(SystemInfo::netKind("docker0", net), QString("Virtual"));
        QCOMPARE(SystemInfo::netSpeedMbps("wlan0", net), -1);
        QCOMPARE(SystemInfo::netSpeedMbps("eth0", net), 1000);
        QCOMPARE(SystemInfo::netState("wlan0", net), QString("up"));
    }

    void staticAndDynamicPartsAddUpToRead() {
        QTemporaryDir dir;
        write(dir.path() + "/cpuinfo", "processor : 0\nmodel name : Fake CPU\nprocessor : 1\n");
        write(dir.path() + "/uptime", "100.0 1.0\n");
        write(dir.path() + "/meminfo", "MemTotal: 1000 kB\nMemAvailable: 400 kB\n");
        const SystemInfo full = SystemInfo::read(dir.path());
        const SystemInfo st = SystemInfo::readStatic(dir.path());
        const SystemInfo dyn = SystemInfo::readDynamic(dir.path());
        QCOMPARE(st.cpuModel, QString("Fake CPU"));
        QCOMPARE(st.logicalCpus, 2);
        QCOMPARE(st.memTotalKb, qint64(-1));   // the static part does not read memory
        QCOMPARE(dyn.logicalCpus, 0);          // ... and the dynamic part does not parse cpuinfo
        QVERIFY(dyn.cpuModel.isEmpty());

        // The cached-static + fresh-dynamic combination equals a full read, and follows changes.
        SystemInfo merged = st;
        merged.mergeDynamic(dyn);
        QCOMPARE(merged.cpuModel, full.cpuModel);
        QCOMPARE(merged.memAvailableKb, full.memAvailableKb);
        QCOMPARE(merged.uptimeSeconds, full.uptimeSeconds);
        write(dir.path() + "/meminfo", "MemTotal: 1000 kB\nMemAvailable: 100 kB\n");
        merged.mergeDynamic(SystemInfo::readDynamic(dir.path()));
        QCOMPARE(merged.memAvailableKb, qint64(100));
        QCOMPARE(merged.cpuModel, QString("Fake CPU")); // the static facts survived
    }

    void missingProcMeansUnknownNotZero() {
        const SystemInfo i = SystemInfo::read("/nonexistent/proc");
        QVERIFY(i.cpuModel.isEmpty());
        QCOMPARE(i.uptimeSeconds, -1.0);
        QCOMPARE(i.memTotalKb, qint64(-1));
        QCOMPARE(i.load1, -1.0);
        QCOMPARE(i.fileHandles, qint64(-1));
    }
};

QTEST_APPLESS_MAIN(TstSystemInfo)
#include "tst_systeminfo.moc"
