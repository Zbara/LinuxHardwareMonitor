#include "mock/MockActions.h"
#include "mock/MockFacts.h"
#include "mock/MockProfile.h"
#include "mock/MockProvider.h"
#include "providers/HwmonProvider.h"

#include <QtTest>

namespace {

const QString kSource = QStringLiteral(QT_TESTCASE_SOURCEDIR);
QString mocksDir() { return kSource + "/../mocks"; }
QString fixture(const QString &rel) { return kSource + "/fixtures/" + rel; }

const SensorReading *find(const QVector<SensorReading> &v, const QString &id) {
    for (const auto &r : v)
        if (r.id == id)
            return &r;
    return nullptr;
}
int countWithPrefix(const QVector<SensorReading> &v, const QString &prefix) {
    int n = 0;
    for (const auto &r : v)
        n += r.id.startsWith(prefix);
    return n;
}

MockProfile profile(const QString &name) {
    QString error;
    const auto p = MockProfile::load(mocksDir() + "/" + name + ".json", &error);
    if (!p)
        qWarning() << name << error;
    return p.value_or(MockProfile());
}

} // namespace

class TstMock : public QObject {
    Q_OBJECT
private slots:
    // ---- profile parsing --------------------------------------------------
    void everyShippedProfileLoadsAndProducesSensors() {
        const QStringList files = QDir(mocksDir()).entryList({"*.json"}, QDir::Files);
        QVERIFY(files.size() >= 5);
        for (const QString &file : files) {
            QString error;
            const auto p = MockProfile::load(mocksDir() + "/" + file, &error);
            QVERIFY2(p.has_value(), qPrintable(file + ": " + error));
            MockProvider provider(*p);
            const auto readings = provider.pollAt(0);
            QVERIFY2(find(readings, "system/cpu_util"), qPrintable(file));
            QCOMPARE(countWithPrefix(readings, "system/cpu") >= p->cpu.threads, true);
            for (const auto &r : readings)
                if (r.type != SensorType::Text)
                    QVERIFY2(std::isfinite(r.value), qPrintable(file + " " + r.id));
        }
    }

    void rejectsBadProfiles_data() {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<QString>("expected");
        QTest::newRow("not json") << QByteArray("hello") << "not valid JSON";
        QTest::newRow("no cpu") << QByteArray(R"({"gpu":{}})") << "needs a \"cpu\" section";
        QTest::newRow("threads < cores") << QByteArray(R"({"cpu":{"cores":8,"threads":4}})") << "threads";
        QTest::newRow("base outside range")
            << QByteArray(R"({"cpu":{"temperature":{"base":120,"min":30,"max":90}}})") << "min <= base <= max";
        QTest::newRow("negative variation")
            << QByteArray(R"({"cpu":{"usage":{"base":10,"min":0,"max":100,"variation":-1}}})") << "variation";
        QTest::newRow("spec not an object") << QByteArray(R"({"cpu":{"temperature":5}})") << "must be an object";
        QTest::newRow("bad network kind") << QByteArray(R"({"cpu":{},"networks":[{"kind":"Token Ring"}]})") << "kind";
        QTest::newRow("unknown scenario") << QByteArray(R"({"cpu":{},"scenarios":[{"type":"meteor"}]})") << "unknown";
        QTest::newRow("zero duration")
            << QByteArray(R"({"cpu":{},"scenarios":[{"type":"overheat","duration_s":0}]})") << "duration_s";
        QTest::newRow("fan_change without rpm")
            << QByteArray(R"({"cpu":{},"scenarios":[{"type":"fan_change","target":"fan:X"}]})") << "rpm";
        QTest::newRow("zero memory") << QByteArray(R"({"cpu":{},"memory":{"total_gb":0}})") << "total_gb";
    }
    void rejectsBadProfiles() {
        QFETCH(QByteArray, json);
        QFETCH(QString, expected);
        QString error;
        QVERIFY(!MockProfile::parse(json, &error).has_value());
        QVERIFY2(error.contains(expected), qPrintable(error));
    }

    void acceptsBothSensorLayoutsAndFillsDefaults() {
        QString error;
        // sensors nested under "sensors" (as in the proposal) or directly on the object
        const auto nested = MockProfile::parse(R"({"cpu":{"sensors":{"temperature":{"base":65,"min":35,"max":95,"variation":8}}}})", &error);
        QVERIFY2(nested, qPrintable(error));
        QCOMPARE(nested->cpu.temperature.base, 65.0);
        const auto flat = MockProfile::parse(R"({"cpu":{"temperature":{"base":70}}})", &error);
        QVERIFY2(flat, qPrintable(error));
        QCOMPARE(flat->cpu.temperature.base, 70.0);
        QVERIFY(flat->cpu.temperature.min <= 70.0 && flat->cpu.temperature.max >= 70.0); // derived, still consistent
        QCOMPARE(flat->cpu.threads, flat->cpu.cores * 2);
        QVERIFY(flat->gpus.isEmpty());
    }

    // ---- generated sensors ------------------------------------------------
    void valuesStayWithinTheirBounds() {
        MockProvider p(profile("amd-desktop"));
        const auto spec = p.profile().cpu.temperature;
        for (int i = 0; i < 600; ++i) {
            const auto r = p.pollAt(i * 1000);
            const double temp = find(r, "mock:cpu/package")->value;
            QVERIFY2(temp >= spec.min && temp <= spec.max, qPrintable(QString::number(temp)));
            const double usage = find(r, "system/cpu_util")->value;
            QVERIFY(usage >= 0 && usage <= 100);
            QVERIFY(find(r, "system/cpu_freq_max")->value <= p.profile().cpu.maxMhz);
        }
    }

    void sameSeedGivesTheSameSequence() {
        MockProvider a(profile("amd-desktop")), b(profile("amd-desktop"));
        for (int i = 0; i < 20; ++i) {
            const auto ra = a.pollAt(i * 1000), rb = b.pollAt(i * 1000);
            QCOMPARE(find(ra, "mock:cpu/package")->value, find(rb, "mock:cpu/package")->value);
            QCOMPARE(find(ra, "system/cpu_util")->value, find(rb, "system/cpu_util")->value);
        }
        MockProfile other = profile("amd-desktop");
        other.seed = 12345;
        MockProvider c(other);
        MockProvider d(profile("amd-desktop"));
        bool differs = false;
        for (int i = 0; i < 20 && !differs; ++i)
            differs = find(c.pollAt(i * 1000), "mock:cpu/package")->value != find(d.pollAt(i * 1000), "mock:cpu/package")->value;
        QVERIFY(differs);
    }

    void sensorsFollowTheRealProvidersConventions() {
        MockProvider p(profile("amd-desktop"));
        const auto r = p.pollAt(0);
        // CPU: AMD naming, one temperature per core, one usage sensor per thread
        QCOMPARE(find(r, "mock:cpu/package")->name, QString("Tctl"));
        QCOMPARE(find(r, "mock:cpu/package")->category, DeviceCategory::Cpu);
        QCOMPARE(countWithPrefix(r, "mock:cpu/core"), 16);
        QCOMPARE(countWithPrefix(r, "system/cpu0_util") + countWithPrefix(r, "system/cpu31_util"), 2);
        QVERIFY(!find(r, "system/cpu32_util"));
        // GPU ids the Performance page looks for
        for (const char *suffix : {"temp", "util", "power", "clock", "mem_used", "mem_total", "fan"})
            QVERIFY2(find(r, QString("mock:gpu0/") + suffix), suffix);
        // AMD has no NVML-only sensors, and amdgpu reports the fan in RPM
        QVERIFY(!find(r, "mock:gpu0/util_mem") && !find(r, "mock:gpu0/power_limit"));
        QCOMPARE(find(r, "mock:gpu0/fan")->unit, QString("RPM"));
        QCOMPARE(find(r, "mock:gpu0/temp")->category, DeviceCategory::Gpu);
        QCOMPARE(find(r, "mock:gpu0/mem_total")->value, 24.0);
        // disks / network / fans
        QVERIFY(find(r, "disk/nvme0n1_busy") && find(r, "disk/sda_read") && find(r, "disk/total_write"));
        QCOMPARE(find(r, "nvme@nvme0/temp1")->category, DeviceCategory::Storage); // matches the nvme0n1 temperature lookup
        QVERIFY(find(r, "net/enp5s0_read") && find(r, "net/total_read"));
        QVERIFY(!find(r, "net/enp5s0_signal")); // Ethernet has no signal
        QCOMPARE(countWithPrefix(r, "mock:fans/"), 3);
        QVERIFY(find(r, "net/tun0_read") && !find(r, "net/tun0_signal")); // a VPN-style virtual interface
    }

    void multipleGpusOfDifferentVendorsAreSeparateDevices() {
        MockProvider hybrid(profile("hybrid-amd-nvidia-laptop"));
        const auto r = hybrid.pollAt(0);
        // gpu0 = AMD iGPU, gpu1 = NVIDIA dGPU
        QCOMPARE(find(r, "mock:gpu0/temp")->deviceName, QString("AMD Radeon 780M Graphics"));
        QCOMPARE(find(r, "mock:gpu1/temp")->deviceName, QString("NVIDIA GeForce RTX 4060 Laptop GPU"));
        QVERIFY(find(r, "mock:gpu1/util_mem"));            // NVML-style sensors only on NVIDIA
        QVERIFY(!find(r, "mock:gpu0/util_mem"));
        QCOMPARE(find(r, "mock:gpu1/fan")->unit, QString("%"));
        QVERIFY(!find(r, "mock:gpu0/fan"));                // the iGPU has no fan
        QCOMPARE(find(r, "mock:gpu1/mem_total")->value, 8.0);
        QCOMPARE(find(r, "mock:gpu0/mem_total")->value, 2.0);
    }

    void intelIntegratedGpuReportsOnlyWhatTheDriverCan() {
        MockProvider p(profile("intel-amd-desktop"));
        const auto r = p.pollAt(0);
        // gpu0 = Intel iGPU: clocks and power, but no load / temperature / VRAM
        QVERIFY(find(r, "mock:gpu0/clock") && find(r, "mock:gpu0/clock_max") && find(r, "mock:gpu0/power"));
        for (const char *missing : {"util", "temp", "mem_used", "mem_total", "fan"})
            QVERIFY2(!find(r, QString("mock:gpu0/") + missing), missing);
        // gpu1 = Radeon: full AMD set
        QVERIFY(find(r, "mock:gpu1/util") && find(r, "mock:gpu1/temp") && find(r, "mock:gpu1/mem_total"));
        QCOMPARE(find(r, "mock:gpu1/mem_total")->value, 12.0);
    }

    void gpuLossAffectsOnlyTheTargetedCard() {
        MockProfile p = profile("hybrid-amd-nvidia-laptop");
        MockScenario loss;
        loss.type = "gpu_loss";
        loss.target = "gpu1";
        loss.startS = 5;
        loss.durationS = 10;
        p.scenarios.push_back(loss);
        MockProvider provider(p);
        const auto during = provider.pollAt(8000);
        QCOMPARE(countWithPrefix(during, "mock:gpu1/"), 0);
        QVERIFY(countWithPrefix(during, "mock:gpu0/") > 0); // the iGPU keeps reporting
    }

    void minimalProfileHasOnlyWhatItDeclares() {
        MockProvider p(profile("minimal-system"));
        const auto r = p.pollAt(0);
        QCOMPARE(countWithPrefix(r, "mock:gpu"), 0);
        QCOMPARE(countWithPrefix(r, "mock:fans/"), 0);
        QCOMPARE(countWithPrefix(r, "power:"), 0);
        QVERIFY(find(r, "disk/sda_busy"));
        QCOMPARE(countWithPrefix(r, "system/cpu0_util") + countWithPrefix(r, "system/cpu1_util"), 2);
    }

    void wifiAndBatteryProfilesProduceTheirSensors() {
        MockProvider p(profile("nvidia-laptop"));
        const auto r = p.pollAt(0);
        QVERIFY(find(r, "net/wlp0s20f3_signal"));
        QCOMPARE(find(r, "net/wlp0s20f3_signal")->unit, QString("dBm"));
        QVERIFY(find(r, "power:BAT0/charge") && find(r, "power:BAT0/health"));
        QCOMPARE(find(r, "power:BAT0/status")->text, QString("Charging"));
        QCOMPARE(find(r, "mock:cpu/package")->name, QString("Package id 0")); // Intel naming
    }

    // ---- scenarios --------------------------------------------------------
    void overheatRampsToThePeakAndBack() {
        MockProvider p(profile("stress-test"));
        auto temp = [&](double s) { return find(p.pollAt(qint64(s * 1000)), "mock:cpu/package")->value; };
        QVERIFY(temp(0) < 95);                       // normal before the scenario
        QVERIFY(temp(20) > 99.9);                    // plateau at the peak (scenario: t = 10..40 s, peak 100)
        QVERIFY(temp(25) > 99.9);
        double after = temp(60);
        QVERIFY2(after < 95, qPrintable(QString::number(after))); // back to normal
    }

    void gpuLossRemovesTheWholeDeviceTemporarily() {
        MockProvider p(profile("stress-test"));
        QVERIFY(countWithPrefix(p.pollAt(0), "mock:gpu0/") > 0);
        QCOMPARE(countWithPrefix(p.pollAt(55 * 1000), "mock:gpu0/"), 0); // 50..70 s
        QVERIFY(countWithPrefix(p.pollAt(80 * 1000), "mock:gpu0/") > 0); // back
    }

    void sensorDropoutMakesOneSensorUnavailable() {
        MockProvider p(profile("stress-test"));
        QVERIFY(find(p.pollAt(0), "mock:fans/cpu_fan")->available);
        const auto during = p.pollAt(80 * 1000); // 75..90 s
        QVERIFY(!find(during, "mock:fans/cpu_fan")->available);
        QVERIFY(find(during, "mock:fans/case_fan_1")->available); // others unaffected
        QVERIFY(find(p.pollAt(100 * 1000), "mock:fans/cpu_fan")->available);
    }

    void fanChangeOverridesTheSpeed() {
        MockProvider p(profile("stress-test"));
        QVERIFY(find(p.pollAt(0), "mock:fans/case_fan_1")->value < 1900);
        QCOMPARE(find(p.pollAt(100 * 1000), "mock:fans/case_fan_1")->value, 4800.0); // 95..115 s
        QVERIFY(find(p.pollAt(130 * 1000), "mock:fans/case_fan_1")->value < 1900);
    }

    void gpuOverheatCanExceedTheNormalRange() {
        MockProvider p(profile("stress-test"));
        QVERIFY(find(p.pollAt(135 * 1000), "mock:gpu0/temp")->value > 100); // 120..150 s, peak 105
    }

    void diskDropoutUsesTheControllerDeviceId() {
        MockProvider p(profile("stress-test"));
        QVERIFY(!find(p.pollAt(165 * 1000), "nvme@nvme0/temp1")->available); // 160..175 s
    }

    void simulatedMemoryModulesMatchTheProfile() {
        const MemoryInfo desktop = MockFacts::memory(profile("amd-desktop"));
        QCOMPARE(desktop.slotsTotal, 4);
        QCOMPARE(desktop.slotsUsed(), 2);
        QCOMPARE(desktop.modules[0].sizeMb * 2, qint64(64) * 1024);  // 64 GB split over two modules
        QCOMPARE(desktop.modules[0].formFactor, QString("DIMM"));     // no battery: a desktop
        const MemoryInfo laptop = MockFacts::memory(profile("amd-laptop"));
        QCOMPARE(laptop.modules[0].formFactor, QString("SODIMM"));    // battery: a laptop
        QCOMPARE(MockFacts::memory(profile("minimal-system")).slotsUsed(), 1); // 4 GB: one module
    }

    // ---- scenarios triggered at run time (simulation panel) ------------------
    void injectedScenarioWorksLikeAProfileScenario() {
        MockProvider p(profile("amd-desktop"));
        MockScenario s;
        s.type = "overheat";
        s.target = "cpu";
        s.durationS = 9;
        s.peak = 100;
        p.injectAt(s, 5.0); // t = 5..14 s
        QVERIFY(find(p.pollAt(0), "mock:cpu/package")->value < 95);
        QVERIFY(find(p.pollAt(9000), "mock:cpu/package")->value > 99.9); // plateau
        p.clearInjected();
        QVERIFY(find(p.pollAt(9000), "mock:cpu/package")->value < 95);   // cancelled
    }

    void injectedGpuLossAndFanStop() {
        MockProvider p(profile("hybrid-amd-nvidia-laptop"));
        MockScenario loss;
        loss.type = "gpu_loss";
        loss.target = "gpu1";
        loss.durationS = 10;
        p.injectAt(loss, 2.0);
        MockScenario stop;
        stop.type = "sensor_dropout";
        stop.target = "fan:CPU Fan";
        stop.durationS = 10;
        p.injectAt(stop, 2.0);
        const auto r = p.pollAt(5000);
        QCOMPARE(countWithPrefix(r, "mock:gpu1/"), 0);
        QVERIFY(!find(r, "mock:fans/cpu_fan")->available);
        QVERIFY(find(r, "mock:fans/gpu_fan")->available);
    }

    void injectIsThreadSafeAndUsesTheSimulatedClock() {
        MockProvider p(profile("amd-desktop"));
        p.setSpeed(10.0);
        QTest::qWait(60);
        QVERIFY(p.simulatedSeconds() > 0.3); // ~0.6 s simulated after 60 ms at 10x
        MockScenario s;
        s.type = "overheat";
        s.target = "cpu";
        s.durationS = 30;
        p.inject(s);                          // starts "now" in simulated time
        QThread *t = QThread::create([&] { p.inject(s); });
        t->start();
        t->wait();
        delete t;
        QVERIFY(find(p.poll(), "mock:cpu/package")); // polling afterwards still works
    }

    void simulationActionsMatchTheHardware() {
        auto kinds = [](const MockProfile &p) {
            QStringList k;
            for (const auto &a : MockActions::build(p))
                k << a.kind;
            return k;
        };
        const auto minimal = kinds(profile("minimal-system"));
        QVERIFY(minimal.contains("overheat_cpu"));
        QVERIFY(!minimal.contains("overheat_gpu") && !minimal.contains("gpu_loss") && !minimal.contains("fan_stop"));
        QVERIFY(minimal.contains("disk_dropout"));

        const auto hybrid = MockActions::build(profile("hybrid-amd-nvidia-laptop"));
        int gpuLoss = 0, fanBoost = 0;
        for (const auto &a : hybrid) {
            gpuLoss += a.kind == "gpu_loss";
            fanBoost += a.kind == "fan_boost";
            QVERIFY(!a.scenario.type.isEmpty() && a.scenario.durationS > 0);
            if (a.kind == "fan_boost")
                QVERIFY(a.scenario.rpm > 0 && a.scenario.target.startsWith("fan:"));
        }
        QCOMPARE(gpuLoss, 2);   // one per GPU
        QCOMPARE(fanBoost, 2);  // one per fan
        QCOMPARE(hybrid.first().scenario.target, QString("cpu"));
    }

    // ---- fixture mode: the real hwmon parser on prepared trees -------------
    void amdFixtureIsDiscoveredAndParsed() {
        HwmonProvider p(fixture("amd/hwmon"));
        const auto r = p.poll();
        QCOMPARE(find(r, "k10temp/temp1")->name, QString("Tctl"));
        QCOMPARE(find(r, "k10temp/temp1")->value, 65.0);
        QCOMPARE(find(r, "k10temp/temp1")->category, DeviceCategory::Cpu);
        QCOMPARE(find(r, "k10temp/temp4")->name, QString("Tccd2"));
        QCOMPARE(find(r, "amdgpu/temp2")->name, QString("junction"));
        QCOMPARE(find(r, "amdgpu/temp2")->category, DeviceCategory::Gpu);
        QCOMPARE(find(r, "amdgpu/power1")->value, 120.0);   // µW -> W
        QCOMPARE(find(r, "amdgpu/freq1")->value, 1500.0);   // Hz -> MHz
        QCOMPARE(find(r, "amdgpu/in0")->value, 0.9);        // mV -> V
        QCOMPARE(find(r, "amdgpu/fan1")->value, 1450.0);
        QCOMPARE(find(r, "nvme/temp1")->category, DeviceCategory::Storage);
        QCOMPARE(find(r, "nct6798/fan2")->value, 900.0);
        QCOMPARE(countWithPrefix(r, "nct6798/"), 3);
    }

    void intelFixtureIsDiscoveredAndParsed() {
        HwmonProvider p(fixture("intel/hwmon"));
        const auto r = p.poll();
        QCOMPARE(find(r, "coretemp/temp1")->name, QString("Package id 0"));
        QCOMPARE(find(r, "coretemp/temp1")->category, DeviceCategory::Cpu);
        QCOMPARE(countWithPrefix(r, "coretemp/"), 5);
        QCOMPARE(find(r, "nvme/temp1")->value, 39.0);
    }

    void brokenFixtureValuesBecomeUnavailableNotZero() {
        HwmonProvider p(fixture("minimal/hwmon"));
        const auto r = p.poll();
        QCOMPARE(find(r, "acpitz/temp1")->value, 45.0);
        QVERIFY(!find(r, "broken/temp1")->available); // "N/A"
        QVERIFY(!find(r, "broken/temp2")->available); // implausible
    }
};

QTEST_GUILESS_MAIN(TstMock)
#include "tst_mock.moc"
