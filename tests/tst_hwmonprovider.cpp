#include "providers/HwmonProvider.h"

#include <QtTest>

class TstHwmonProvider : public QObject {
    Q_OBJECT

    static void write(const QString &path, const QByteArray &content) {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
    }

    static const SensorReading *find(const QVector<SensorReading> &v, const QString &id) {
        for (const auto &r : v)
            if (r.id == id)
                return &r;
        return nullptr;
    }

private slots:
    void parsesAndConverts() {
        QCOMPARE(*HwmonProvider::parseRaw("45000\n", 1e-3), 45.0);
        QCOMPARE(*HwmonProvider::parseRaw("1200", 1.0), 1200.0);
        QCOMPARE(*HwmonProvider::parseRaw("12000000", 1e-6), 12.0);
        QVERIFY(!HwmonProvider::parseRaw("", 1.0));
        QVERIFY(!HwmonProvider::parseRaw("abc", 1.0));
    }

    void friendlyDeviceNames() {
        QCOMPARE(HwmonProvider::displayName("coretemp", ""), QString("CPU (Intel)"));
        QCOMPARE(HwmonProvider::displayName("nvme", "Micron 3400"), QString("NVMe — Micron 3400"));
        QCOMPARE(HwmonProvider::displayName("BAT1", ""), QString("Battery (BAT1)"));
        QCOMPARE(HwmonProvider::displayName("mystery", ""), QString("mystery"));
    }

    void readsTemperatureWithLabel() {
        QTemporaryDir dir;
        const QString d = dir.path() + "/hwmon0";
        write(d + "/name", "coretemp\n");
        write(d + "/temp1_input", "52500\n");
        write(d + "/temp1_label", "Package id 0\n");

        HwmonProvider p(dir.path());
        const auto readings = p.poll();
        QCOMPARE(readings.size(), 1);
        QCOMPARE(readings[0].name, QString("Package id 0"));
        QCOMPARE(readings[0].value, 52.5);
        QCOMPARE(readings[0].unit, QString("°C"));
        QVERIFY(readings[0].available);
        QCOMPARE(readings[0].category, DeviceCategory::Cpu);
    }

    void handlesMissingAndCorruptValues() {
        QTemporaryDir dir;
        const QString d = dir.path() + "/hwmon0";
        write(d + "/name", "x\n");
        write(d + "/temp1_input", "garbage\n");
        write(d + "/temp2_input", "999999999\n"); // implausible
        write(d + "/fan1_input", "");

        HwmonProvider p(dir.path());
        const auto readings = p.poll();
        QCOMPARE(readings.size(), 3);
        for (const auto &r : readings)
            QVERIFY2(!r.available, qPrintable(r.id));
    }

    void missingRootGivesNoSensors() {
        HwmonProvider p("/nonexistent/path/for/test");
        QVERIFY(p.poll().isEmpty());
    }

    void multipleDevicesAndKinds() {
        QTemporaryDir dir;
        write(dir.path() + "/hwmon0/name", "k10temp\n");
        write(dir.path() + "/hwmon0/temp1_input", "40000\n");
        write(dir.path() + "/hwmon1/name", "nvme\n");
        write(dir.path() + "/hwmon1/temp1_input", "35000\n");
        write(dir.path() + "/hwmon2/name", "nct6798\n");
        write(dir.path() + "/hwmon2/fan1_input", "900\n");
        write(dir.path() + "/hwmon2/in0_input", "1200\n");
        write(dir.path() + "/hwmon2/power1_average", "5000000\n");

        HwmonProvider p(dir.path());
        const auto r = p.poll();
        QCOMPARE(r.size(), 5);
        QCOMPARE(find(r, "nvme/temp1")->category, DeviceCategory::Storage);
        QCOMPARE(find(r, "nct6798/fan1")->value, 900.0);
        QCOMPARE(find(r, "nct6798/in0")->value, 1.2);
        QCOMPARE(find(r, "nct6798/power1")->value, 5.0);
    }

    void idsAreStableAndIndependentOfHwmonNumber() {
        QTemporaryDir a, b;
        write(a.path() + "/hwmon0/name", "nvme\n");
        write(a.path() + "/hwmon0/temp1_input", "30000\n");
        write(b.path() + "/hwmon7/name", "nvme\n");
        write(b.path() + "/hwmon7/temp1_input", "31000\n");

        const auto ra = HwmonProvider(a.path()).poll();
        const auto rb = HwmonProvider(b.path()).poll();
        QCOMPARE(ra[0].id, rb[0].id);

        HwmonProvider p(a.path());
        QCOMPARE(p.poll()[0].id, p.poll()[0].id);
    }

    void duplicateDriverNamesGetDistinctIds() {
        QTemporaryDir dir;
        write(dir.path() + "/hwmon0/name", "nvme\n");
        write(dir.path() + "/hwmon0/temp1_input", "30000\n");
        write(dir.path() + "/hwmon1/name", "nvme\n");
        write(dir.path() + "/hwmon1/temp1_input", "31000\n");

        const auto r = HwmonProvider(dir.path()).poll();
        QCOMPARE(r.size(), 2);
        QVERIFY(r[0].id != r[1].id);
    }

    void rediscoversDevices() {
        QTemporaryDir dir;
        write(dir.path() + "/hwmon0/name", "a\n");
        write(dir.path() + "/hwmon0/temp1_input", "30000\n");
        HwmonProvider p(dir.path());
        QCOMPARE(p.poll().size(), 1);

        write(dir.path() + "/hwmon1/name", "b\n");
        write(dir.path() + "/hwmon1/temp1_input", "31000\n");
        QCOMPARE(p.poll().size(), 2);

        QVERIFY(QDir(dir.path() + "/hwmon0").removeRecursively());
        const auto r = p.poll();
        QCOMPARE(r.size(), 1);
        QCOMPARE(r[0].id, QString("b/temp1"));
    }
};

QTEST_APPLESS_MAIN(TstHwmonProvider)
#include "tst_hwmonprovider.moc"
