#include "MockFacts.h"

#include <algorithm>

namespace MockFacts {

const MockDisk *findDisk(const MockProfile &profile, const QString &name) {
    for (const MockDisk &d : profile.disks)
        if (d.name == name)
            return &d;
    return nullptr;
}

const MockNet *findNet(const MockProfile &profile, const QString &name) {
    for (const MockNet &n : profile.nets)
        if (n.name == name)
            return &n;
    return nullptr;
}

SystemInfo systemInfo(const MockProfile &profile, double memUtilPercent, double elapsedSeconds) {
    const MockCpu &cpu = profile.cpu;
    SystemInfo info;
    info.cpuModel = cpu.model;
    info.logicalCpus = cpu.threads;
    info.physicalCores = cpu.cores;
    info.sockets = cpu.sockets;
    info.maxMhz = cpu.maxMhz;
    info.baseMhz = cpu.baseMhz;
    info.powerLimitW = cpu.powerLimitW;
    info.l1Kb = cpu.l1Kb > 0 ? cpu.l1Kb : -1;
    info.l2Kb = cpu.l2Kb > 0 ? cpu.l2Kb : -1;
    info.l3Kb = cpu.l3Kb > 0 ? cpu.l3Kb : -1;
    info.virtualization = cpu.virtualization;
    info.uptimeSeconds = 3600.0 + elapsedSeconds; // a simulated machine that has been up for an hour

    const double totalKb = profile.memory.totalGb * 1024.0 * 1024.0;
    const double usedFraction = std::clamp(memUtilPercent / 100.0, 0.0, 1.0);
    info.memTotalKb = static_cast<qint64>(totalKb);
    info.memAvailableKb = static_cast<qint64>(totalKb * (1.0 - usedFraction));
    info.memCachedKb = static_cast<qint64>(totalKb * 0.2);
    info.buffersKb = static_cast<qint64>(totalKb * 0.02);
    info.commitLimitKb = static_cast<qint64>(totalKb * 0.5 + profile.memory.swapGb * 1024.0 * 1024.0);
    info.committedKb = static_cast<qint64>(totalKb * usedFraction * 1.4);
    info.slabReclaimableKb = static_cast<qint64>(totalKb * 0.04);
    info.slabUnreclaimableKb = static_cast<qint64>(totalKb * 0.01);
    info.shmemKb = static_cast<qint64>(totalKb * 0.01);
    info.dirtyKb = 2048;
    info.swapTotalKb = static_cast<qint64>(profile.memory.swapGb * 1024.0 * 1024.0);
    info.swapFreeKb = info.swapTotalKb;
    // Load average roughly follows the simulated CPU usage.
    const double load = profile.cpu.usage.base / 100.0 * profile.cpu.threads;
    info.load1 = load;
    info.load5 = load * 0.9;
    info.load15 = load * 0.8;
    info.fileHandles = -1; // unknown rather than invented
    return info;
}

MemoryInfo memory(const MockProfile &profile) {
    MemoryInfo info;
    info.slotsTotal = 4;
    info.errorCorrection = QStringLiteral("None");
    const int count = profile.memory.totalGb >= 8 ? 2 : 1;
    for (int i = 0; i < count; ++i) {
        MemoryModule m;
        m.locator = QStringLiteral("Controller%1-ChannelA-DIMM0").arg(i);
        m.bankLocator = QStringLiteral("BANK %1").arg(i * 2);
        m.sizeMb = static_cast<qint64>(profile.memory.totalGb * 1024.0 / count);
        m.type = QStringLiteral("DDR5");
        m.typeDetail = QStringLiteral("Synchronous");
        m.formFactor = profile.battery.present ? QStringLiteral("SODIMM") : QStringLiteral("DIMM");
        m.speedMts = 5200;
        m.ratedSpeedMts = 5600;
        m.manufacturer = QStringLiteral("Simulated Memory Co.");
        m.partNumber = QStringLiteral("SIM-DDR5-5200");
        m.rank = 1;
        info.modules.push_back(m);
    }
    return info;
}

QVariantMap disk(const MockProfile &profile, const QString &name) {
    QVariantMap m;
    const MockDisk *d = findDisk(profile, name);
    if (!d)
        return m;
    const bool nvme = d->name.startsWith(QLatin1String("nvme"));
    m["model"] = d->model;
    m["sizeBytes"] = static_cast<qint64>(d->sizeGb * 1e9);
    m["rotational"] = d->rotational;
    m["serial"] = QStringLiteral("SIM-%1").arg(qHash(d->name) % 100000, 5, 10, QLatin1Char('0'));
    m["firmware"] = QStringLiteral("SIM1.0");
    m["interface"] = nvme ? QStringLiteral("NVMe") : QStringLiteral("SATA");
    m["blockSize"] = 512;
    m["usedBytes"] = d->sizeGb * 1e9 * std::clamp(d->usedFraction, 0.0, 1.0);
    m["mountedBytes"] = d->sizeGb * 1e9;
    return m;
}

QVariantMap net(const MockProfile &profile, const QString &name) {
    QVariantMap m;
    const MockNet *n = findNet(profile, name);
    if (!n)
        return m;
    const int index = static_cast<int>(n - profile.nets.constData());
    const bool wifi = n->kind == QLatin1String("Wi-Fi");
    const bool virtualIf = n->kind == QLatin1String("Virtual");
    m["kind"] = n->kind;
    m["vendor"] = n->vendor;
    m["model"] = n->model;
    m["mac"] = virtualIf ? QString() : QStringLiteral("02:00:00:00:00:%1").arg(index + 1, 2, 16, QLatin1Char('0')); // tunnels have no MAC
    m["driver"] = wifi ? QStringLiteral("sim-wifi") : virtualIf ? QStringLiteral("tun") : QStringLiteral("sim-eth");
    m["busAddress"] = virtualIf ? QString() : QStringLiteral("0000:00:%1.0").arg(index + 1, 2, 16, QLatin1Char('0'));
    m["state"] = QStringLiteral("up");
    m["speedMbps"] = n->speedMbps;
    m["mtu"] = 1500;
    m["duplex"] = wifi ? QString() : QStringLiteral("full");
    m["ipv4"] = QStringLiteral("192.0.2.%1/24").arg(10 + index); // TEST-NET-1, reserved for documentation
    m["ipv6"] = QString();
    if (wifi) {
        QVariantMap w;
        w["connected"] = true;
        w["ssid"] = n->ssid.isEmpty() ? QStringLiteral("SimulatedWiFi") : n->ssid;
        w["bssid"] = QStringLiteral("02:00:00:00:10:%1").arg(index + 1, 2, 16, QLatin1Char('0'));
        w["frequencyMhz"] = 5180;
        w["channel"] = 36;
        w["band"] = QStringLiteral("5 GHz");
        w["bitrateMbps"] = n->speedMbps;
        w["security"] = QStringLiteral("WPA2/WPA3");
        w["mode"] = QStringLiteral("Infrastructure");
        m["wifi"] = w;
    }
    return m;
}

} // namespace MockFacts
