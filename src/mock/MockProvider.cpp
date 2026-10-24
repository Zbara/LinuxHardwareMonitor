#include "MockProvider.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace {

// Rough normal distribution (sum of uniforms), good enough for sensor noise.
double gaussian(QRandomGenerator &rng) {
    double s = 0;
    for (int i = 0; i < 6; ++i)
        s += rng.generateDouble();
    return (s - 3.0) / 0.7071; // variance 6/12 = 0.5
}

QString slug(const QString &s) {
    QString out;
    for (const QChar c : s)
        out += c.isLetterOrNumber() ? c.toLower() : QLatin1Char('_');
    return out;
}

// "nvme0n1" -> "nvme0" (the controller the real hwmon device hangs off); "sda" stays "sda".
QString controllerOf(const QString &disk) {
    static const QRegularExpression nvme(QStringLiteral("^(nvme\\d+)n\\d+$"));
    const auto m = nvme.match(disk);
    return m.hasMatch() ? m.captured(1) : disk;
}

} // namespace

double MockProvider::Walker::next(QRandomGenerator &rng) {
    value += (spec.base - value) * 0.12 + gaussian(rng) * spec.variation * 0.35;
    value = std::clamp(value, spec.min, spec.max);
    return value;
}

MockProvider::Walker MockProvider::makeWalker(const SignalSpec &spec) {
    return Walker{spec, spec.base};
}

MockProvider::MockProvider(MockProfile profile) : m_profile(std::move(profile)), m_rng(m_profile.seed) {
    m_clock.start();
    const MockCpu &cpu = m_profile.cpu;
    m_cpuTemp = makeWalker(cpu.temperature);
    m_cpuFreq = makeWalker(cpu.frequencyMhz);
    m_memUsage = makeWalker(m_profile.memory.usage);
    for (int i = 0; i < cpu.threads; ++i) {
        // Threads are not all equally busy: spread the base load deterministically.
        SignalSpec s = cpu.usage;
        s.base = std::clamp(cpu.usage.base * (0.4 + 1.2 * ((i * 7) % 10) / 10.0), s.min, s.max);
        m_threadUsage.push_back(makeWalker(s));
    }
    for (const MockGpu &g : std::as_const(m_profile.gpus)) {
        GpuWalkers w;
        w.temp = makeWalker(g.temperature);
        w.usage = makeWalker(g.usage);
        w.power = makeWalker(g.powerW);
        w.clock = makeWalker(g.clockMhz);
        w.vram = makeWalker(g.vramUsage);
        w.fan = makeWalker(g.fanPercent);
        w.enc = makeWalker(SignalSpec::make(0, 0, 100, 0));
        w.dec = makeWalker(SignalSpec::make(0, 0, 100, 0));
        w.memCtl = makeWalker(SignalSpec::make(g.usage.base * 0.6, 0, 100, g.usage.variation * 0.5));
        m_gpus.push_back(w);
    }
    for (const MockDisk &d : std::as_const(m_profile.disks))
        m_disks.push_back({makeWalker(d.temperature), makeWalker(d.readMbps), makeWalker(d.writeMbps), makeWalker(d.busy)});
    for (const MockNet &n : std::as_const(m_profile.nets))
        m_nets.push_back({makeWalker(n.downMbps), makeWalker(n.upMbps), makeWalker(n.signalDbm)});
    for (const MockFan &f : std::as_const(m_profile.fans))
        m_fans.push_back(makeWalker(f.rpm));
    m_batteryCharge = makeWalker(m_profile.battery.charge);
    m_batteryPower = makeWalker(m_profile.battery.powerW);
}

void MockProvider::inject(MockScenario scenario) {
    injectAt(std::move(scenario), simulatedSeconds());
}

void MockProvider::injectAt(MockScenario scenario, double startS) {
    scenario.startS = startS;
    QMutexLocker lock(&m_mutex);
    m_injected.push_back(std::move(scenario));
}

void MockProvider::clearInjected() {
    QMutexLocker lock(&m_mutex);
    m_injected.clear();
}

double MockProvider::overheatFactor(const QString &target, double t) const {
    double f = 0;
    for (const MockScenario &s : m_scenarios) {
        if (s.type != QLatin1String("overheat") || s.target != target)
            continue;
        if (t < s.startS || t > s.startS + s.durationS)
            continue;
        const double ramp = s.durationS / 3.0; // up for a third, hold for a third, down for a third
        const double x = t - s.startS;
        f = std::max(f, x < ramp ? x / ramp : (x > s.durationS - ramp ? (s.durationS - x) / ramp : 1.0));
    }
    return f;
}

double MockProvider::overheatPeak(const QString &target, double fallback) const {
    double peak = fallback;
    for (const MockScenario &s : m_scenarios)
        if (s.type == QLatin1String("overheat") && s.target == target)
            peak = std::max(peak, s.peak);
    return peak;
}

double MockProvider::withOverheat(const QString &target, double t, double value) const {
    const double f = overheatFactor(target, t);
    return f > 0 ? value + (overheatPeak(target, value) - value) * f : value;
}

bool MockProvider::active(const QString &type, const QString &target, double t) const {
    for (const MockScenario &s : m_scenarios)
        if (s.type == type && s.target == target && t >= s.startS && t <= s.startS + s.durationS)
            return true;
    return false;
}

std::optional<double> MockProvider::fanOverride(const QString &fan, double t) const {
    std::optional<double> rpm;
    for (const MockScenario &s : m_scenarios)
        if (s.type == QLatin1String("fan_change") && s.target == QLatin1String("fan:") + fan
            && t >= s.startS && t <= s.startS + s.durationS)
            rpm = s.rpm;
    return rpm;
}

QVector<SensorReading> MockProvider::pollAt(qint64 elapsedMs) {
    const double t = double(elapsedMs) / 1000.0;
    const QDateTime now = QDateTime::currentDateTime();
    QVector<SensorReading> out;
    {
        QMutexLocker lock(&m_mutex);
        m_scenarios = m_profile.scenarios + m_injected;
    }

    auto add = [&](const QString &deviceId, const QString &deviceName, DeviceCategory category, const QString &id,
                   const QString &name, SensorType type, const QString &unit, std::optional<double> value) {
        SensorReading r;
        r.id = id;
        r.deviceId = deviceId;
        r.deviceName = deviceName;
        r.category = category;
        r.name = name;
        r.type = type;
        r.unit = unit;
        r.available = value.has_value();
        r.value = value.value_or(0.0);
        r.timestamp = now;
        out.push_back(r);
    };
    auto addText = [&](const QString &deviceId, const QString &deviceName, DeviceCategory category, const QString &id,
                       const QString &name, const QString &text) {
        if (text.isEmpty())
            return;
        add(deviceId, deviceName, category, id, name, SensorType::Text, QString(), 0.0);
        out.last().text = text;
    };

    // ------------------------------------------------------------------ CPU
    const MockCpu &cpu = m_profile.cpu;
    const bool amd = cpu.vendor.contains(QLatin1String("AMD"), Qt::CaseInsensitive);
    const QString cpuDevice = QStringLiteral("mock:cpu");
    const QString cpuName = amd ? QStringLiteral("CPU (AMD)") : QStringLiteral("CPU (Intel)");

    double temp = m_cpuTemp.next(m_rng);
    temp = withOverheat(QStringLiteral("cpu"), t, temp);
    const bool cpuTempDropped = active(QStringLiteral("sensor_dropout"), QStringLiteral("cpu"), t);
    const QString packageName = amd ? QStringLiteral("Tctl") : QStringLiteral("Package id 0");
    add(cpuDevice, cpuName, DeviceCategory::Cpu, cpuDevice + QLatin1String("/package"), packageName, SensorType::Temperature,
        QStringLiteral("°C"), cpuTempDropped ? std::nullopt : std::optional<double>(temp));
    for (int i = 0; i < cpu.cores; ++i) {
        const double core = temp - 2 + ((i * 13) % 5);
        add(cpuDevice, cpuName, DeviceCategory::Cpu, cpuDevice + QStringLiteral("/core%1").arg(i), QStringLiteral("Core %1").arg(i),
            SensorType::Temperature, QStringLiteral("°C"), cpuTempDropped ? std::nullopt : std::optional<double>(core));
    }

    const QString sys = QStringLiteral("mock:system");
    double usageSum = 0;
    QVector<double> usage(cpu.threads);
    for (int i = 0; i < cpu.threads; ++i) {
        usage[i] = m_threadUsage[i].next(m_rng);
        usageSum += usage[i];
    }
    add(sys, QStringLiteral("System"), DeviceCategory::System, QStringLiteral("system/cpu_util"), QStringLiteral("CPU Usage"),
        SensorType::Utilization, QStringLiteral("%"), usageSum / cpu.threads);
    const double freq = m_cpuFreq.next(m_rng);
    add(sys, QStringLiteral("System"), DeviceCategory::System, QStringLiteral("system/cpu_freq_avg"), QStringLiteral("CPU Frequency (avg)"),
        SensorType::Frequency, QStringLiteral("MHz"), freq);
    add(sys, QStringLiteral("System"), DeviceCategory::System, QStringLiteral("system/cpu_freq_max"),
        QStringLiteral("CPU Frequency (max core)"), SensorType::Frequency, QStringLiteral("MHz"),
        std::min(cpu.maxMhz, freq * 1.12 + 80));
    const double memPct = m_memUsage.next(m_rng);
    add(sys, QStringLiteral("System"), DeviceCategory::System, QStringLiteral("system/mem_util"), QStringLiteral("Memory Usage"),
        SensorType::Utilization, QStringLiteral("%"), memPct);
    add(sys, QStringLiteral("System"), DeviceCategory::System, QStringLiteral("system/mem_used"), QStringLiteral("Memory Used"),
        SensorType::Memory, QStringLiteral("GB"), memPct / 100.0 * m_profile.memory.totalGb);
    for (int i = 0; i < cpu.threads; ++i)
        add(QStringLiteral("cpu-cores"), QStringLiteral("CPU Cores"), DeviceCategory::System, QStringLiteral("system/cpu%1_util").arg(i),
            QStringLiteral("CPU %1 Usage").arg(i), SensorType::Utilization, QStringLiteral("%"), usage[i]);

    // ------------------------------------------------------------------ fans
    for (int i = 0; i < m_profile.fans.size(); ++i) {
        const MockFan &fan = m_profile.fans[i];
        double rpm = m_fans[i].next(m_rng);
        if (const auto override = fanOverride(fan.name, t))
            rpm = *override;
        const bool dropped = active(QStringLiteral("sensor_dropout"), QStringLiteral("fan:") + fan.name, t);
        add(QStringLiteral("mock:fans"), QStringLiteral("Fans"), DeviceCategory::Other, QStringLiteral("mock:fans/") + slug(fan.name),
            fan.name, SensorType::FanSpeed, QStringLiteral("RPM"), dropped ? std::nullopt : std::optional<double>(rpm));
    }

    // ------------------------------------------------------------------ GPUs
    for (int i = 0; i < m_profile.gpus.size(); ++i) {
        const MockGpu &g = m_profile.gpus[i];
        GpuWalkers &w = m_gpus[i];
        const QString target = QStringLiteral("gpu%1").arg(i);
        // Advance every walker even when hidden so the sequence does not depend on scenarios.
        double gtemp = w.temp.next(m_rng);
        const double util = w.usage.next(m_rng);
        const double power = w.power.next(m_rng);
        const double clock = w.clock.next(m_rng);
        const double vram = w.vram.next(m_rng);
        const double fan = w.fan.next(m_rng);
        const double memCtl = w.memCtl.next(m_rng);
        gtemp = withOverheat(target, t, gtemp);
        if (active(QStringLiteral("gpu_loss"), target, t))
            continue; // the whole device vanishes, as if it fell off the bus

        const QString dev = QStringLiteral("mock:gpu%1").arg(i);
        const QString dname = g.model;
        const bool tempDropped = active(QStringLiteral("sensor_dropout"), target, t);
        // Which sensors exist depends on the driver, exactly as with the real providers:
        //   NVIDIA (NVML): everything, fan in %;  AMD (amdgpu hwmon + DRM): no encoder/limit sensors, fan in RPM;
        //   Intel (i915/xe): clocks and power only - no load, VRAM or temperature without extra privileges.
        const bool intel = g.vendor.compare(QLatin1String("Intel"), Qt::CaseInsensitive) == 0;
        const bool nvidia = g.vendor.compare(QLatin1String("NVIDIA"), Qt::CaseInsensitive) == 0;
        auto gpuAdd = [&](const QString &key, const QString &name, SensorType type, const QString &unit, std::optional<double> v) {
            add(dev, dname, DeviceCategory::Gpu, dev + QLatin1Char('/') + key, name, type, unit, v);
        };
        if (!intel) {
            gpuAdd(QStringLiteral("temp"), QStringLiteral("GPU Temperature"), SensorType::Temperature, QStringLiteral("°C"),
                   tempDropped ? std::nullopt : std::optional<double>(gtemp));
            gpuAdd(QStringLiteral("util"), QStringLiteral("GPU Utilization"), SensorType::Utilization, QStringLiteral("%"), util);
        }
        gpuAdd(QStringLiteral("power"), QStringLiteral("GPU Power"), SensorType::Power, QStringLiteral("W"), power);
        gpuAdd(QStringLiteral("clock"), QStringLiteral("GPU Clock"), SensorType::Frequency, QStringLiteral("MHz"), clock);
        if (!intel && g.vramMb > 0) {
            gpuAdd(QStringLiteral("mem_used"), QStringLiteral("GPU Memory Used"), SensorType::Memory, QStringLiteral("GB"),
                   vram / 100.0 * g.vramMb / 1024.0);
            gpuAdd(QStringLiteral("mem_total"), QStringLiteral("GPU Memory Total"), SensorType::Memory, QStringLiteral("GB"),
                   g.vramMb / 1024.0);
        }
        if (nvidia) {
            gpuAdd(QStringLiteral("util_mem"), QStringLiteral("GPU Memory Controller"), SensorType::Utilization, QStringLiteral("%"), memCtl);
            gpuAdd(QStringLiteral("power_limit"), QStringLiteral("GPU Power Limit"), SensorType::Power, QStringLiteral("W"), g.powerW.max);
        }
        if (nvidia || intel)
            gpuAdd(QStringLiteral("clock_max"), QStringLiteral("GPU Max Clock"), SensorType::Frequency, QStringLiteral("MHz"), g.maxClockMhz);
        if (g.hasFan && !intel) {
            if (nvidia)
                gpuAdd(QStringLiteral("fan"), QStringLiteral("GPU Fan"), SensorType::FanSpeed, QStringLiteral("%"), fan);
            else
                gpuAdd(QStringLiteral("fan"), QStringLiteral("GPU Fan"), SensorType::FanSpeed, QStringLiteral("RPM"), fan * 30.0); // amdgpu reports RPM
        }
        addText(dev, dname, DeviceCategory::Gpu, dev + QLatin1String("/driver"), QStringLiteral("Driver version"), g.driver);
        addText(dev, dname, DeviceCategory::Gpu, dev + QLatin1String("/pci_id"), QStringLiteral("PCI device ID"), g.pciId);
    }

    // ------------------------------------------------------------------ disks
    double totalRead = 0, totalWrite = 0;
    for (int i = 0; i < m_profile.disks.size(); ++i) {
        const MockDisk &d = m_profile.disks[i];
        DiskWalkers &w = m_disks[i];
        const double dtemp = w.temp.next(m_rng);
        const double read = w.read.next(m_rng);
        const double write = w.write.next(m_rng);
        const double busy = w.busy.next(m_rng);
        totalRead += read;
        totalWrite += write;

        const QString hwmonDevice = (d.name.startsWith(QLatin1String("nvme")) ? QStringLiteral("nvme") : QStringLiteral("drivetemp"))
                                    + QLatin1Char('@') + controllerOf(d.name);
        const bool dropped = active(QStringLiteral("sensor_dropout"), QStringLiteral("disk:") + d.name, t);
        add(hwmonDevice, d.name.startsWith(QLatin1String("nvme")) ? QStringLiteral("NVMe — ") + d.model : d.model, DeviceCategory::Storage, hwmonDevice + QLatin1String("/temp1"),
            QStringLiteral("Composite"), SensorType::Temperature, QStringLiteral("°C"),
            dropped ? std::nullopt : std::optional<double>(dtemp));
        add(QStringLiteral("disk"), QStringLiteral("Disks"), DeviceCategory::Other, QStringLiteral("disk/%1_read").arg(d.name),
            d.name + QLatin1String(" Read"), SensorType::Throughput, QStringLiteral("MB/s"), read);
        add(QStringLiteral("disk"), QStringLiteral("Disks"), DeviceCategory::Other, QStringLiteral("disk/%1_write").arg(d.name),
            d.name + QLatin1String(" Write"), SensorType::Throughput, QStringLiteral("MB/s"), write);
        add(QStringLiteral("disk"), QStringLiteral("Disks"), DeviceCategory::Other, QStringLiteral("disk/%1_busy").arg(d.name),
            d.name + QLatin1String(" Active time"), SensorType::Utilization, QStringLiteral("%"), busy);
    }
    if (!m_profile.disks.isEmpty()) {
        add(QStringLiteral("disk"), QStringLiteral("Disks"), DeviceCategory::Other, QStringLiteral("disk/total_read"),
            QStringLiteral("Read (total)"), SensorType::Throughput, QStringLiteral("MB/s"), totalRead);
        add(QStringLiteral("disk"), QStringLiteral("Disks"), DeviceCategory::Other, QStringLiteral("disk/total_write"),
            QStringLiteral("Write (total)"), SensorType::Throughput, QStringLiteral("MB/s"), totalWrite);
    }

    // ------------------------------------------------------------------ network
    double totalDown = 0, totalUp = 0;
    for (int i = 0; i < m_profile.nets.size(); ++i) {
        const MockNet &n = m_profile.nets[i];
        NetWalkers &w = m_nets[i];
        const double down = w.down.next(m_rng);
        const double up = w.up.next(m_rng);
        const double sig = w.signal.next(m_rng);
        totalDown += down;
        totalUp += up;
        add(QStringLiteral("net"), QStringLiteral("Network"), DeviceCategory::Other, QStringLiteral("net/%1_read").arg(n.name),
            n.name + QLatin1String(" Download"), SensorType::Throughput, QStringLiteral("MB/s"), down);
        add(QStringLiteral("net"), QStringLiteral("Network"), DeviceCategory::Other, QStringLiteral("net/%1_write").arg(n.name),
            n.name + QLatin1String(" Upload"), SensorType::Throughput, QStringLiteral("MB/s"), up);
        if (n.kind == QLatin1String("Wi-Fi"))
            add(QStringLiteral("net"), QStringLiteral("Network"), DeviceCategory::Other, QStringLiteral("net/%1_signal").arg(n.name),
                n.name + QLatin1String(" Signal"), SensorType::Signal, QStringLiteral("dBm"), sig);
    }
    if (!m_profile.nets.isEmpty()) {
        add(QStringLiteral("net"), QStringLiteral("Network"), DeviceCategory::Other, QStringLiteral("net/total_read"),
            QStringLiteral("Download (total)"), SensorType::Throughput, QStringLiteral("MB/s"), totalDown);
        add(QStringLiteral("net"), QStringLiteral("Network"), DeviceCategory::Other, QStringLiteral("net/total_write"),
            QStringLiteral("Upload (total)"), SensorType::Throughput, QStringLiteral("MB/s"), totalUp);
    }

    // ------------------------------------------------------------------ battery
    if (m_profile.battery.present) {
        const MockBattery &b = m_profile.battery;
        const QString dev = QStringLiteral("power:BAT0");
        const QString dname = QStringLiteral("Battery — Simulated");
        const double charge = m_batteryCharge.next(m_rng);
        const double power = m_batteryPower.next(m_rng);
        add(dev, dname, DeviceCategory::Other, dev + QLatin1String("/charge"), QStringLiteral("Battery Charge"), SensorType::Utilization,
            QStringLiteral("%"), charge);
        add(dev, dname, DeviceCategory::Other, dev + QLatin1String("/energy"), QStringLiteral("Battery Energy"), SensorType::Energy,
            QStringLiteral("Wh"), charge / 100.0 * b.fullWh);
        add(dev, dname, DeviceCategory::Other, dev + QLatin1String("/energy_full"), QStringLiteral("Battery Full Capacity"),
            SensorType::Energy, QStringLiteral("Wh"), b.fullWh);
        add(dev, dname, DeviceCategory::Other, dev + QLatin1String("/energy_design"), QStringLiteral("Battery Design Capacity"),
            SensorType::Energy, QStringLiteral("Wh"), b.designWh);
        add(dev, dname, DeviceCategory::Other, dev + QLatin1String("/health"), QStringLiteral("Battery Health"), SensorType::Utilization,
            QStringLiteral("%"), 100.0 * b.fullWh / b.designWh);
        add(dev, dname, DeviceCategory::Other, dev + QLatin1String("/power"), QStringLiteral("Battery Power"), SensorType::Power,
            QStringLiteral("W"), power);
        add(dev, dname, DeviceCategory::Other, dev + QLatin1String("/voltage"), QStringLiteral("Battery Voltage"), SensorType::Voltage,
            QStringLiteral("V"), 15.4 + charge / 100.0);
        addText(dev, dname, DeviceCategory::Other, dev + QLatin1String("/status"), QStringLiteral("Battery status"), b.status);
        addText(dev, dname, DeviceCategory::Other, dev + QLatin1String("/cycles"), QStringLiteral("Battery cycles"), QString::number(b.cycles));
        addText(dev, dname, DeviceCategory::Other, dev + QLatin1String("/ac"), QStringLiteral("AC adapter"),
                b.status == QLatin1String("Discharging") ? QStringLiteral("offline") : QStringLiteral("online"));
    }
    return out;
}
