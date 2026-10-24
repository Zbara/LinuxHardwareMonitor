#include "providers/IoProvider.h"

#include <QtTest>

namespace {

void write(const QString &path, const QByteArray &content) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

QByteArray netDev(quint64 rx, quint64 tx) {
    return "Inter-|   Receive                            |  Transmit\n"
           " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
           "    lo: 999999 10 0 0 0 0 0 0 999999 10 0 0 0 0 0 0\n"
           "  eth0: " + QByteArray::number(rx) + " 10 0 0 0 0 0 0 " + QByteArray::number(tx)
           + " 10 0 0 0 0 0 0\n";
}

QByteArray diskStats(quint64 sectorsRead, quint64 sectorsWritten, quint64 busyMs = 0) {
    return "   8       0 sda 1 0 " + QByteArray::number(sectorsRead) + " 1 1 0 "
           + QByteArray::number(sectorsWritten) + " 1 0 " + QByteArray::number(busyMs) + " 1\n"
           "   8       1 sda1 1 0 99999999 1 1 0 99999999 1 0 1 1\n"
           "   7       0 loop0 1 0 99999999 1 1 0 99999999 1 0 1 1\n";
}

const SensorReading *find(const QVector<SensorReading> &v, const QString &id) {
    for (const auto &r : v)
        if (r.id == id)
            return &r;
    return nullptr;
}

} // namespace

class TstIoProvider : public QObject {
    Q_OBJECT
private slots:
    void parsesNetDevAndSkipsLoopback() {
        const auto n = IoProvider::parseNetDev(netDev(1000, 2000));
        QCOMPARE(n.size(), 1);
        QCOMPARE(n["eth0"].readBytes, quint64(1000));
        QCOMPARE(n["eth0"].writeBytes, quint64(2000));
    }

    void parsesDiskStatsWholeDisksOnly() {
        const auto d = IoProvider::parseDiskStats(diskStats(10, 20));
        QCOMPARE(d.size(), 1);
        QCOMPARE(d["sda"].readBytes, quint64(10 * 512));
        QCOMPARE(d["sda"].writeBytes, quint64(20 * 512));
    }

    void ratesNeedTwoSamples() {
        QTemporaryDir dir;
        write(dir.path() + "/net/dev", netDev(0, 0));
        write(dir.path() + "/diskstats", diskStats(0, 0));
        IoProvider p(dir.path());

        const auto first = p.pollAt(1000000);
        QVERIFY(!find(first, "net/total_read")->available);
        QVERIFY(!find(first, "disk/sda_read")->available);

        // 2 s later: 4 MB received, 2 MB sent; 4000 sectors (~2.05 MB) read.
        write(dir.path() + "/net/dev", netDev(4000000, 2000000));
        write(dir.path() + "/diskstats", diskStats(4000, 0));
        const auto second = p.pollAt(1002000);
        QCOMPARE(find(second, "net/eth0_read")->value, 2.0);
        QCOMPARE(find(second, "net/total_write")->value, 1.0);
        QVERIFY(qFuzzyCompare(find(second, "disk/sda_read")->value, 4000.0 * 512 / 2 / 1e6));
        QCOMPARE(find(second, "disk/sda_write")->value, 0.0);
    }

    void parsesWirelessSignal() {
        const auto w = IoProvider::parseWireless(
            "Inter-| sta-|   Quality        |   Discarded packets               | Missed | WE\n"
            " face | tus | link level noise |  nwid  crypt   frag  retry   misc | beacon | 22\n"
            "wlan0: 0000   70.  -40.  -256        0      0      0      0      0        0\n"
            "wlan1: 0000   0.  0.  -256        0      0      0      0      0        0\n");
        QCOMPARE(w.size(), 1);
        QCOMPARE(w["wlan0"], -40.0);

        QTemporaryDir dir;
        write(dir.path() + "/net/wireless", "h\nh\nwlan0: 0000 70. -61. -256 0 0 0 0 0 0\n");
        IoProvider p(dir.path());
        const auto r = p.pollAt(1000);
        QCOMPARE(find(r, "net/wlan0_signal")->value, -61.0);
        QCOMPARE(find(r, "net/wlan0_signal")->type, SensorType::Signal);
        QVERIFY(find(r, "net/wlan0_signal")->available);
    }

    void diskActiveTime() {
        QTemporaryDir dir;
        write(dir.path() + "/diskstats", diskStats(0, 0, 1000));
        IoProvider p(dir.path());
        QVERIFY(!find(p.pollAt(1000000), "disk/sda_busy")->available);
        // 500 ms busy during 2 s -> 25 %
        write(dir.path() + "/diskstats", diskStats(0, 0, 1500));
        const auto r = p.pollAt(1002000);
        QCOMPARE(find(r, "disk/sda_busy")->value, 25.0);
        QCOMPARE(find(r, "disk/sda_busy")->unit, QString("%"));
        QCOMPARE(find(r, "disk/sda_busy")->type, SensorType::Utilization);
    }

    void counterResetIsUnavailableNotNegative() {
        QTemporaryDir dir;
        write(dir.path() + "/net/dev", netDev(5000000, 5000000));
        IoProvider p(dir.path());
        p.pollAt(1000000);
        write(dir.path() + "/net/dev", netDev(10, 10));
        const auto r = p.pollAt(1001000);
        QVERIFY(!find(r, "net/eth0_read")->available);
    }

    void missingProcGivesOnlyUnavailableTotals() {
        IoProvider p("/nonexistent/proc");
        const auto r = p.pollAt(1000);
        for (const auto &s : r)
            QVERIFY(!s.available);
    }
};

QTEST_APPLESS_MAIN(TstIoProvider)
#include "tst_ioprovider.moc"
