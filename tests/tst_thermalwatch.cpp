#include "core/CpuThrottleReader.h"
#include "core/ThermalWatch.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {

SensorReading temp(double v, DeviceCategory c = DeviceCategory::Cpu) {
    SensorReading r;
    r.id = "cpu/temp";
    r.name = "Package";
    r.type = SensorType::Temperature;
    r.category = c;
    r.value = v;
    r.available = true;
    return r;
}

SensorReading fan(double rpm, bool ok = true) {
    SensorReading r;
    r.id = "fan1";
    r.name = "CPU Fan";
    r.deviceName = "Laptop";
    r.type = SensorType::FanSpeed;
    r.unit = "RPM";
    r.value = rpm;
    r.available = ok;
    return r;
}

SensorReading gpuThrottle(const QString &text) {
    SensorReading r;
    r.id = "nvidia:GPU-1/throttle";
    r.name = "Clock throttling";
    r.deviceName = "NVIDIA GPU";
    r.type = SensorType::Text;
    r.text = text;
    r.available = true;
    return r;
}

void put(const QString &path, const QString &text) {
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(text.toUtf8());
}

} // namespace

class TstThermalWatch : public QObject {
    Q_OBJECT
private slots:
    void cpuThrottleNeedsABaselineAndRespectsTheCooldown() {
        ThermalWatch w;
        QCOMPARE(w.update({temp(90)}, 5000, 0).size(), 0);       // the count since boot is only a baseline
        QCOMPARE(w.update({temp(90)}, 5000, 1000).size(), 0);    // unchanged
        const auto first = w.update({temp(91)}, 5010, 2000);
        QCOMPARE(first.size(), 1);
        QCOMPARE(first[0].kind, ThermalWatch::Kind::CpuThrottled);
        QCOMPARE(first[0].temperature, 91.0);
        QCOMPARE(w.update({temp(91)}, 5090, 3000).size(), 0);    // still throttling: no second message
        QCOMPARE(w.update({temp(91)}, 5200, 2000 + ThermalWatch::kCooldownMs).size(), 1); // a new episode
        QCOMPARE(w.update({temp(91)}, -1, 3000000).size(), 0);   // no counter on this machine: nothing
    }

    void gpuThermalSlowdownOnlyForHeat() {
        ThermalWatch w;
        QCOMPARE(w.update({gpuThrottle("none")}, -1, 0).size(), 0);
        QCOMPARE(w.update({gpuThrottle("power")}, -1, 1000).size(), 0);          // a power cap is normal under load
        const auto e = w.update({gpuThrottle("thermal,power")}, -1, 2000);
        QCOMPARE(e.size(), 1);
        QCOMPARE(e[0].kind, ThermalWatch::Kind::GpuThrottled);
        QCOMPARE(e[0].subject, QString("NVIDIA GPU"));
        QCOMPARE(w.update({gpuThrottle("thermal")}, -1, 3000).size(), 0);        // cooldown
    }

    void aStoppedFanIsReportedOnceAndOnlyWhenHot() {
        ThermalWatch w;
        w.update({fan(2400), temp(60)}, -1, 0);                                  // it was spinning
        for (int i = 0; i < 5; ++i)
            QCOMPARE(w.update({fan(0), temp(60)}, -1, 1000 + i).size(), 0);      // 0 RPM but cool: a normal idle stop
        QVector<ThermalWatch::Event> got;
        for (int i = 0; i < 3; ++i)
            got += w.update({fan(0), temp(85)}, -1, 2000 + i);
        QCOMPARE(got.size(), 1);                                                 // once
        QCOMPARE(got[0].kind, ThermalWatch::Kind::FanStopped);
        QCOMPARE(got[0].subject, QString("CPU Fan"));
        QCOMPARE(got[0].temperature, 85.0);
        QCOMPARE(w.update({fan(0), temp(90)}, -1, 3000).size(), 0);
        w.update({fan(2000), temp(70)}, -1, 4000);                               // it runs again: re-armed
        QVector<ThermalWatch::Event> again;
        for (int i = 0; i < 5; ++i)
            again += w.update({fan(0), temp(88)}, -1, 5000 + i);
        QCOMPARE(again.size(), 1);
    }

    void aFanThatNeverSpunOrIsUnavailableIsIgnored() {
        ThermalWatch w;
        for (int i = 0; i < 10; ++i)
            QCOMPARE(w.update({fan(0), temp(95)}, -1, i).size(), 0);             // never seen spinning (e.g. absent fan)
        ThermalWatch w2;
        w2.update({fan(2000), temp(95)}, -1, 0);
        for (int i = 0; i < 10; ++i)
            QCOMPARE(w2.update({fan(0, false), temp(95)}, -1, i).size(), 0);     // N/A is not 0
    }

    void percentFansAreIgnored() {
        ThermalWatch w;
        SensorReading gpuFan = fan(0);
        gpuFan.unit = "%";
        SensorReading spinning = gpuFan;
        spinning.value = 60;
        w.update({spinning, temp(95, DeviceCategory::Gpu)}, -1, 0);
        for (int i = 0; i < 10; ++i)
            QCOMPARE(w.update({gpuFan, temp(95, DeviceCategory::Gpu)}, -1, i).size(), 0); // zero-RPM idle of a GPU
    }

    void readsThePackageCounterOncePerPackage() {
        QTemporaryDir dir;
        // two packages with two cores each; the count is per package
        for (int cpu = 0; cpu < 4; ++cpu) {
            const QString base = QString("%1/cpu%2").arg(dir.path()).arg(cpu);
            put(base + "/topology/physical_package_id", cpu < 2 ? "0\n" : "1\n");
            put(base + "/thermal_throttle/package_throttle_count", cpu < 2 ? "10\n" : "5\n");
        }
        CpuThrottleReader reader(dir.path());
        QCOMPARE(reader.read(), qint64(15)); // 10 + 5, not 10+10+5+5
        put(dir.path() + "/cpu0/thermal_throttle/package_throttle_count", "12\n");
        QCOMPARE(reader.read(), qint64(17)); // re-read every call
    }

    void noCounterMeansMinusOne() {
        QTemporaryDir dir;
        put(dir.path() + "/cpu0/topology/physical_package_id", "0\n"); // an AMD-like tree: no thermal_throttle
        CpuThrottleReader reader(dir.path());
        QCOMPARE(reader.read(), qint64(-1));
        QCOMPARE(CpuThrottleReader(dir.path() + "/missing").read(), qint64(-1));
    }
};

QTEST_GUILESS_MAIN(TstThermalWatch)
#include "tst_thermalwatch.moc"
