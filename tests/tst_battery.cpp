#include "providers/BatteryProvider.h"

#include <QtTest>

namespace {
void write(const QString &path, const QByteArray &content) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}
const SensorReading *find(const QVector<SensorReading> &v, const QString &id) {
    for (const auto &r : v)
        if (r.id == id)
            return &r;
    return nullptr;
}
} // namespace

class TstBattery : public QObject {
    Q_OBJECT
private slots:
    void readsChargeBasedBattery() {
        QTemporaryDir dir;
        const QString b = dir.path() + "/BAT1";
        write(b + "/type", "Battery\n");
        write(b + "/capacity", "80\n");
        write(b + "/status", "Discharging\n");
        write(b + "/charge_now", "4000000\n");        // 4 Ah
        write(b + "/charge_full", "5000000\n");
        write(b + "/charge_full_design", "5500000\n");
        write(b + "/voltage_now", "16000000\n");       // 16 V
        write(b + "/voltage_min_design", "15000000\n"); // 15 V nominal
        write(b + "/current_now", "1500000\n");        // 1.5 A
        write(b + "/cycle_count", "38\n");
        write(b + "/model_name", "AP21A7T\n");
        write(dir.path() + "/ACAD/type", "Mains\n");
        write(dir.path() + "/ACAD/online", "0\n");

        BatteryProvider p(dir.path());
        const auto r = p.poll();
        QCOMPARE(find(r, "power:BAT1/charge")->value, 80.0);
        QCOMPARE(find(r, "power:BAT1/energy")->value, 60.0);         // 4 Ah × 15 V
        QCOMPARE(find(r, "power:BAT1/energy_full")->value, 75.0);
        QCOMPARE(find(r, "power:BAT1/energy_design")->value, 82.5);
        QCOMPARE(find(r, "power:BAT1/health")->value, 100.0 * 75.0 / 82.5);
        QCOMPARE(find(r, "power:BAT1/power")->value, 24.0);          // 1.5 A × 16 V
        QCOMPARE(find(r, "power:BAT1/voltage")->value, 16.0);
        QCOMPARE(find(r, "power:BAT1/status")->text, QString("Discharging"));
        QCOMPARE(find(r, "power:BAT1/cycles")->text, QString("38"));
        QCOMPARE(find(r, "power:BAT1/ac")->text, QString("offline"));
        QCOMPARE(find(r, "power:BAT1/charge")->deviceName, QString("Battery — AP21A7T"));
    }

    void prefersNativeEnergyAndPowerFiles() {
        QTemporaryDir dir;
        const QString b = dir.path() + "/BAT0";
        write(b + "/type", "Battery\n");
        write(b + "/energy_now", "30000000\n");  // µWh
        write(b + "/energy_full", "60000000\n");
        write(b + "/power_now", "12000000\n");   // µW
        BatteryProvider p(dir.path());
        const auto r = p.poll();
        QCOMPARE(find(r, "power:BAT0/charge")->value, 50.0); // derived: no capacity file
        QCOMPARE(find(r, "power:BAT0/energy")->value, 30.0);
        QCOMPARE(find(r, "power:BAT0/power")->value, 12.0);
        QVERIFY(!find(r, "power:BAT0/health")); // no design capacity: not guessed
    }

    void missingAttributesAreUnavailableNotZero() {
        QTemporaryDir dir;
        write(dir.path() + "/BAT0/type", "Battery\n");
        BatteryProvider p(dir.path());
        const auto r = p.poll();
        QVERIFY(!find(r, "power:BAT0/charge")->available);
        QVERIFY(!find(r, "power:BAT0/power")->available);
        QVERIFY(!find(r, "power:BAT0/voltage")->available);
    }

    void ignoresNonBatteriesAndPeripherals() {
        QTemporaryDir dir;
        write(dir.path() + "/ACAD/type", "Mains\n");
        write(dir.path() + "/hidpp_battery_0/type", "Battery\n");
        write(dir.path() + "/hidpp_battery_0/scope", "Device\n");
        write(dir.path() + "/hidpp_battery_0/capacity", "50\n");
        BatteryProvider p(dir.path());
        QVERIFY(p.poll().isEmpty());
        BatteryProvider missing("/nonexistent/power_supply");
        QVERIFY(missing.poll().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstBattery)
#include "tst_battery.moc"
