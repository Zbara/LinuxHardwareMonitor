#include "core/OverheatMonitor.h"

#include <QtTest>

namespace {
SensorReading temp(const QString &id, double v, bool available = true) {
    SensorReading r;
    r.id = id;
    r.name = id;
    r.type = SensorType::Temperature;
    r.value = v;
    r.available = available;
    return r;
}
} // namespace

class TstOverheatMonitor : public QObject {
    Q_OBJECT
private slots:
    void alertsOncePerCrossing() {
        OverheatMonitor m;
        m.setThreshold(90);
        QCOMPARE(m.update({temp("a", 89)}).size(), 0);
        QCOMPARE(m.update({temp("a", 91)}).size(), 1);
        QCOMPARE(m.update({temp("a", 95)}).size(), 0);
    }

    void rearmsAfterHysteresis() {
        OverheatMonitor m;
        m.setThreshold(90);
        m.update({temp("a", 91)});
        QCOMPARE(m.update({temp("a", 87)}).size(), 0); // inside hysteresis band
        QCOMPARE(m.update({temp("a", 91)}).size(), 0);
        m.update({temp("a", 80)});
        QCOMPARE(m.update({temp("a", 91)}).size(), 1);
    }

    void ignoresUnavailableAndNonTemperature() {
        OverheatMonitor m;
        m.setThreshold(90);
        SensorReading fan = temp("f", 5000);
        fan.type = SensorType::FanSpeed;
        QCOMPARE(m.update({temp("a", 120, false), fan}).size(), 0);
    }

    void sensorThresholdReplacesTheGlobalOne() {
        OverheatMonitor m;
        m.setThreshold(90);
        m.setSensorThreshold("disk", 60);
        QCOMPARE(m.sensorThreshold("disk"), 60.0);
        QCOMPARE(m.sensorThreshold("cpu"), 0.0);
        const auto first = m.update({temp("disk", 65), temp("cpu", 65)});
        QCOMPARE(first.size(), 1); // only the disk crossed its own limit
        QCOMPARE(first[0].sensorId, QString("disk"));
        QCOMPARE(m.update({temp("disk", 66)}).size(), 0); // once per crossing
        QCOMPARE(m.update({temp("cpu", 91)}).size(), 1);  // the others still use 90
        m.update({temp("disk", 50)});                      // re-armed below 60 - 5
        QCOMPARE(m.update({temp("disk", 61)}).size(), 1);
    }

    void sensorThresholdCanBeHigherAndCanBeRemoved() {
        OverheatMonitor m;
        m.setThreshold(90);
        m.setSensorThreshold("gpu", 100); // a card that is allowed to run hot
        QCOMPARE(m.update({temp("gpu", 95)}).size(), 0);
        QCOMPARE(m.update({temp("gpu", 101)}).size(), 1);
        m.setSensorThreshold("gpu", 0);   // back to the global limit; judged afresh
        QCOMPARE(m.sensorThreshold("gpu"), 0.0);
        QCOMPARE(m.update({temp("gpu", 95)}).size(), 1);
    }

    void sensorThresholdWorksWhenTheGlobalOneIsOff() {
        OverheatMonitor m;
        m.setThreshold(0);
        m.setSensorThreshold("ssd", 70);
        QCOMPARE(m.update({temp("ssd", 75), temp("cpu", 150)}).size(), 1);
    }

    void derivedSensorsAreNotCheckedTwice() {
        OverheatMonitor m;
        m.setThreshold(90);
        QCOMPARE(m.update({temp("derived/cpu_temp_hottest_core", 99), temp("derived/cpu_temp_avg", 95)}).size(), 0);
        QCOMPARE(m.update({temp("core0", 99)}).size(), 1); // the real sensor still alerts
    }

    void oneMessagePerDeviceWithTheHottestSensor() {
        auto alert = [](const char *device, const char *sensor, double v) {
            return OverheatMonitor::Alert{QString(sensor), QString(device), QString(sensor), v};
        };
        const auto groups = OverheatMonitor::groupByDevice({alert("CPU", "Core 1", 91), alert("GPU", "GPU", 93), alert("CPU", "Package", 97),
                                                            alert("CPU", "Core 2", 92)});
        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups[0].deviceName, QString("CPU"));   // devices keep the order they first appeared in
        QCOMPARE(groups[0].sensorName, QString("Package"));
        QCOMPARE(groups[0].value, 97.0);
        QCOMPARE(groups[0].others, 2);
        QCOMPARE(groups[1].deviceName, QString("GPU"));
        QCOMPARE(groups[1].others, 0);
        QVERIFY(OverheatMonitor::groupByDevice({}).isEmpty());
    }

    void zeroThresholdDisables() {
        OverheatMonitor m;
        m.setThreshold(0);
        QCOMPARE(m.update({temp("a", 200)}).size(), 0);
    }
};

QTEST_APPLESS_MAIN(TstOverheatMonitor)
#include "tst_overheatmonitor.moc"
