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

    void zeroThresholdDisables() {
        OverheatMonitor m;
        m.setThreshold(0);
        QCOMPARE(m.update({temp("a", 200)}).size(), 0);
    }
};

QTEST_APPLESS_MAIN(TstOverheatMonitor)
#include "tst_overheatmonitor.moc"
