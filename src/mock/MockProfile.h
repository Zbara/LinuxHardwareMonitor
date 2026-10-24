#pragma once

#include <QString>
#include <QVector>

#include <optional>

// A hardware profile for simulation mode (see mocks/*.json). Every number in a profile is
// made up for testing the UI; none of it describes real measurements.

// How one simulated sensor behaves: a mean-reverting random walk around `base`, clamped to
// [min, max]. `variation` is the typical step size.
struct SignalSpec {
    double base = 0;
    double min = 0;
    double max = 100;
    double variation = 0;

    static SignalSpec make(double base, double min, double max, double variation) {
        return {base, min, max, variation};
    }
};

struct MockCpu {
    QString vendor = QStringLiteral("GenuineIntel"); // or AuthenticAMD
    QString model = QStringLiteral("Mock CPU");
    int cores = 8;
    int threads = 16;
    int sockets = 1;
    double baseMhz = 2000;
    double maxMhz = 4000;
    int l1Kb = 0, l2Kb = 0, l3Kb = 0;
    bool virtualization = true;
    double powerLimitW = -1;
    SignalSpec temperature = SignalSpec::make(50, 30, 90, 4);
    SignalSpec usage = SignalSpec::make(15, 0, 100, 10);
    SignalSpec frequencyMhz = SignalSpec::make(2500, 800, 4000, 300);
};

struct MockMemory {
    double totalGb = 16;
    double swapGb = 0;
    SignalSpec usage = SignalSpec::make(35, 10, 90, 2);
};

struct MockGpu {
    QString vendor = QStringLiteral("NVIDIA"); // NVIDIA / AMD / Intel
    QString model = QStringLiteral("Mock GPU");
    int vramMb = 8192;
    QString driver;
    QString pciId;
    int maxClockMhz = 2500;
    bool hasFan = true;
    SignalSpec temperature = SignalSpec::make(55, 30, 90, 3);
    SignalSpec usage = SignalSpec::make(20, 0, 100, 12);
    SignalSpec powerW = SignalSpec::make(60, 5, 200, 10);
    SignalSpec clockMhz = SignalSpec::make(1500, 300, 2500, 200);
    SignalSpec vramUsage = SignalSpec::make(30, 5, 95, 2); // percent of VRAM
    SignalSpec fanPercent = SignalSpec::make(40, 0, 100, 4);
};

struct MockDisk {
    QString name = QStringLiteral("nvme0n1");
    QString model = QStringLiteral("Mock SSD");
    double sizeGb = 1000;
    double usedFraction = 0.4;
    bool rotational = false;
    SignalSpec temperature = SignalSpec::make(45, 30, 80, 2);
    SignalSpec readMbps = SignalSpec::make(5, 0, 3000, 8);
    SignalSpec writeMbps = SignalSpec::make(3, 0, 2500, 6);
    SignalSpec busy = SignalSpec::make(5, 0, 100, 5);
};

struct MockNet {
    QString name = QStringLiteral("enp5s0");
    QString kind = QStringLiteral("Ethernet"); // Ethernet / Wi-Fi
    QString ssid;
    int speedMbps = 1000;
    QString vendor = QStringLiteral("Mock Networks");
    QString model;
    SignalSpec downMbps = SignalSpec::make(0.5, 0, 120, 1);
    SignalSpec upMbps = SignalSpec::make(0.2, 0, 60, 0.5);
    SignalSpec signalDbm = SignalSpec::make(-55, -85, -35, 3); // Wi-Fi only
};

struct MockFan {
    QString name = QStringLiteral("Fan");
    SignalSpec rpm = SignalSpec::make(1500, 600, 3500, 120);
};

struct MockBattery {
    bool present = false;
    double designWh = 60;
    double fullWh = 55;
    int cycles = 100;
    QString status = QStringLiteral("Discharging");
    SignalSpec charge = SignalSpec::make(70, 5, 100, 1);
    SignalSpec powerW = SignalSpec::make(12, 3, 60, 2);
};

// Something that happens to the simulated hardware at a given time.
struct MockScenario {
    // overheat | gpu_loss | sensor_dropout | fan_change
    QString type;
    // "cpu", "gpu0", "fan:<name>" or "disk:<name>"
    QString target;
    double startS = 0;
    double durationS = 10;
    double peak = 100; // overheat: temperature to reach
    double rpm = 0;    // fan_change: new speed
};

struct MockProfile {
    QString name = QStringLiteral("Simulated system");
    quint32 seed = 1;
    MockCpu cpu;
    MockMemory memory;
    QVector<MockGpu> gpus;
    QVector<MockDisk> disks;
    QVector<MockNet> nets;
    QVector<MockFan> fans;
    MockBattery battery;
    QVector<MockScenario> scenarios;

    // Parses and validates a profile. On failure returns nullopt and a human-readable reason.
    static std::optional<MockProfile> parse(const QByteArray &json, QString *error = nullptr);
    static std::optional<MockProfile> load(const QString &path, QString *error = nullptr);
};
