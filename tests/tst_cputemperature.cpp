#include "core/CpuTemperature.h"

#include <QtTest>

using namespace CpuTemperature;

namespace {

SensorReading cpuTemp(const QString &name, double v, bool ok = true) {
    SensorReading r;
    r.id = "coretemp/" + name;
    r.deviceId = "coretemp@0";
    r.deviceName = "CPU (Intel)";
    r.category = DeviceCategory::Cpu;
    r.name = name;
    r.type = SensorType::Temperature;
    r.unit = "°C";
    r.value = v;
    r.available = ok;
    return r;
}

QVector<SensorReading> intel() {
    return {cpuTemp("Package id 0", 70), cpuTemp("Core 0", 60), cpuTemp("Core 1", 64), cpuTemp("Core 2", 56)};
}

const SensorReading *find(const QVector<SensorReading> &v, const char *id) {
    for (const SensorReading &r : v)
        if (r.id == QLatin1String(id))
            return &r;
    return nullptr;
}

} // namespace

class TstCpuTemperature : public QObject {
    Q_OBJECT
private slots:
    void namesRoundTripAndUnknownMeansPackage() {
        for (Source s : {Source::Package, Source::HottestCore, Source::AverageCores})
            QCOMPARE(sourceFromName(sourceName(s)), s);
        QCOMPARE(sourceFromName("garbage"), Source::Package);
        QCOMPARE(sourceFromName(""), Source::Package);
    }

    void derivedSensorsAreComputedFromTheCoresOnly() {
        QVector<SensorReading> v = intel();
        addDerived(v);
        QCOMPARE(v.size(), 6);
        const SensorReading *avg = find(v, kAverageId), *hot = find(v, kHottestId);
        QVERIFY(avg && hot);
        QCOMPARE(avg->value, 60.0);                 // (60 + 64 + 56) / 3: the package reading is not a core
        QCOMPARE(hot->value, 64.0);
        QVERIFY(avg->available && hot->available);
        QCOMPARE(avg->deviceId, QString("coretemp@0")); // listed under the CPU device
        QCOMPARE(avg->category, DeviceCategory::Cpu);
        QCOMPARE(avg->unit, QString("°C"));
    }

    void unavailableCoresAreSkippedAndAllUnavailableMeansUnavailable() {
        QVector<SensorReading> v{cpuTemp("Package id 0", 70), cpuTemp("Core 0", 60), cpuTemp("Core 1", 0, false), cpuTemp("Core 2", 64)};
        addDerived(v);
        QCOMPARE(find(v, kAverageId)->value, 62.0);

        QVector<SensorReading> gone{cpuTemp("Core 0", 1, false), cpuTemp("Core 1", 2, false)};
        addDerived(gone);
        QVERIFY(find(gone, kAverageId) && !find(gone, kAverageId)->available); // never 0 as a real value
        QVERIFY(!find(gone, kHottestId)->available);
    }

    void nothingIsAddedWithoutPerCoreSensors() {
        QVector<SensorReading> onlyPackage{cpuTemp("Tctl", 55)};
        addDerived(onlyPackage);
        QCOMPARE(onlyPackage.size(), 1);
        QVector<SensorReading> oneCore{cpuTemp("Package id 0", 70), cpuTemp("Core 0", 60)};
        addDerived(oneCore);
        QCOMPARE(oneCore.size(), 2);
        SensorReading gpu = cpuTemp("Core 0", 50);
        gpu.category = DeviceCategory::Gpu;
        QVector<SensorReading> other{gpu, gpu};
        addDerived(other);
        QCOMPARE(other.size(), 2); // only CPU sensors count
        // Running twice must not derive from the derived values.
        QVector<SensorReading> v = intel();
        addDerived(v);
        addDerived(v);
        QCOMPARE(v.size(), 8);
    }

    void pickFollowsTheSetting() {
        QVector<SensorReading> v = intel();
        addDerived(v);
        auto c = pick(v, Source::Package);
        QCOMPARE(c.reading->value, 70.0);
        QCOMPARE(c.used, Source::Package);
        c = pick(v, Source::HottestCore);
        QCOMPARE(c.reading->value, 64.0);
        QCOMPARE(c.used, Source::HottestCore);
        c = pick(v, Source::AverageCores);
        QCOMPARE(c.reading->value, 60.0);
        QCOMPARE(c.used, Source::AverageCores);
    }

    void pickFallsBackInsteadOfShowingNothing() {
        // A machine without per-core sensors (or a simulation): the request cannot be met, the package stands in.
        QVector<SensorReading> v{cpuTemp("Tctl", 55)};
        addDerived(v);
        auto c = pick(v, Source::AverageCores);
        QCOMPARE(c.reading->value, 55.0);
        QCOMPARE(c.used, Source::Package); // the UI is told what was really used

        // No package sensor at all: the hottest CPU sensor.
        QVector<SensorReading> noPackage{cpuTemp("Core 0", 58), cpuTemp("Core 1", 63)};
        QCOMPARE(pick(noPackage, Source::Package).reading->value, 63.0);

        // No CPU temperature: nothing, not a made-up value.
        QVERIFY(pick({}, Source::Package).reading == nullptr);
        QVERIFY(pick({cpuTemp("Package id 0", 70, false)}, Source::Package).reading == nullptr);
    }
};

QTEST_GUILESS_MAIN(TstCpuTemperature)
#include "tst_cputemperature.moc"
