#include "SystemInfo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QRegularExpression>

namespace {

QByteArray readAll(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

qint64 kbField(const QByteArray &meminfo, const char *name) {
    for (const QByteArray &line : meminfo.split('\n')) {
        if (!line.startsWith(name))
            continue;
        bool ok = false;
        const qint64 v = line.mid(line.indexOf(':') + 1).simplified().split(' ').value(0).toLongLong(&ok);
        return ok ? v : -1;
    }
    return -1;
}

} // namespace

qint64 SystemInfo::parseCacheSizeKb(const QString &text) {
    if (text.size() < 2)
        return -1;
    bool ok = false;
    const qint64 n = text.left(text.size() - 1).toLongLong(&ok);
    if (!ok)
        return -1;
    switch (text.back().toUpper().unicode()) {
    case 'K': return n;
    case 'M': return n * 1024;
    case 'G': return n * 1024 * 1024;
    }
    return -1;
}

SystemInfo SystemInfo::readStatic(const QString &root, const QString &cpuRoot, const QString &powercapRoot) {
    SystemInfo info;

    const QString cpuinfo = QString::fromUtf8(readAll(root + QLatin1String("/cpuinfo")));
    static const QRegularExpression model(QStringLiteral("^model name\\s*:\\s*(.+)$"),
                                          QRegularExpression::MultilineOption);
    static const QRegularExpression processor(QStringLiteral("^processor\\s*:"),
                                              QRegularExpression::MultilineOption);
    if (const auto m = model.match(cpuinfo); m.hasMatch())
        info.cpuModel = m.captured(1).simplified();
    auto it = processor.globalMatch(cpuinfo);
    while (it.hasNext()) {
        it.next();
        ++info.logicalCpus;
    }

    static const QRegularExpression cores(QStringLiteral("^cpu cores\\s*:\\s*(\\d+)"),
                                          QRegularExpression::MultilineOption);
    static const QRegularExpression physId(QStringLiteral("^physical id\\s*:\\s*(\\d+)"),
                                           QRegularExpression::MultilineOption);
    static const QRegularExpression flags(QStringLiteral("^flags\\s*:.*\\b(vmx|svm)\\b"),
                                          QRegularExpression::MultilineOption);
    QSet<QString> ids;
    auto pit = physId.globalMatch(cpuinfo);
    while (pit.hasNext())
        ids.insert(pit.next().captured(1));
    info.sockets = static_cast<int>(ids.size());
    if (const auto m = cores.match(cpuinfo); m.hasMatch())
        info.physicalCores = m.captured(1).toInt() * std::max<int>(info.sockets, 1);
    info.virtualization = flags.match(cpuinfo).hasMatch();

    bool khzOk = false;
    const double maxKhz = readAll(cpuRoot + QLatin1String("/cpu0/cpufreq/cpuinfo_max_freq")).trimmed().toDouble(&khzOk);
    if (khzOk)
        info.maxMhz = maxKhz / 1000.0;
    const double baseKhz = readAll(cpuRoot + QLatin1String("/cpu0/cpufreq/base_frequency")).trimmed().toDouble(&khzOk);
    if (khzOk && baseKhz > 0)
        info.baseMhz = baseKhz / 1000.0;
    const double limitUw = readAll(powercapRoot + QLatin1String("/intel-rapl:0/constraint_0_power_limit_uw")).trimmed().toDouble(&khzOk);
    if (khzOk && limitUw > 0)
        info.powerLimitW = limitUw / 1e6;

    // Caches of cpu0: sysfs lists index0..N with level/type/size.
    const QStringList idx = QDir(cpuRoot + QLatin1String("/cpu0/cache")).entryList({QStringLiteral("index*")}, QDir::Dirs);
    for (const QString &i : idx) {
        const QString base = cpuRoot + QLatin1String("/cpu0/cache/") + i;
        const QString type = QString::fromUtf8(readAll(base + QLatin1String("/type"))).trimmed();
        const int level = QString::fromUtf8(readAll(base + QLatin1String("/level"))).trimmed().toInt();
        const qint64 kb = parseCacheSizeKb(QString::fromUtf8(readAll(base + QLatin1String("/size"))).trimmed());
        const bool known = type == QLatin1String("Data") || type == QLatin1String("Instruction")
                           || type == QLatin1String("Unified");
        if (kb < 0 || !known)
            continue;
        qint64 *slot = level == 1 ? &info.l1Kb : level == 2 ? &info.l2Kb : level == 3 ? &info.l3Kb : nullptr;
        if (slot)
            *slot = (*slot < 0 ? 0 : *slot) + kb;
    }

    return info;
}

SystemInfo SystemInfo::readDynamic(const QString &root) {
    SystemInfo info;
    // file-nr: allocated, unused, max
    bool handlesOk = false;
    const qint64 handles = readAll(root + QLatin1String("/sys/fs/file-nr")).simplified().split(' ').value(0).toLongLong(&handlesOk);
    if (handlesOk)
        info.fileHandles = handles;

    bool ok = false;
    const double up = readAll(root + QLatin1String("/uptime")).split(' ').value(0).toDouble(&ok);
    if (ok)
        info.uptimeSeconds = up;

    const QByteArray mem = readAll(root + QLatin1String("/meminfo"));
    info.memTotalKb = kbField(mem, "MemTotal:");
    info.memAvailableKb = kbField(mem, "MemAvailable:");
    const qint64 cached = kbField(mem, "Cached:");
    const qint64 buffers = kbField(mem, "Buffers:");
    if (cached >= 0)
        info.memCachedKb = cached + std::max<qint64>(buffers, 0);
    info.commitLimitKb = kbField(mem, "CommitLimit:");
    info.committedKb = kbField(mem, "Committed_AS:");
    info.slabReclaimableKb = kbField(mem, "SReclaimable:");
    info.slabUnreclaimableKb = kbField(mem, "SUnreclaim:");
    info.shmemKb = kbField(mem, "Shmem:");
    info.buffersKb = buffers;
    info.dirtyKb = kbField(mem, "Dirty:");
    info.swapTotalKb = kbField(mem, "SwapTotal:");
    info.swapFreeKb = kbField(mem, "SwapFree:");

    const QList<QByteArray> load = readAll(root + QLatin1String("/loadavg")).simplified().split(' ');
    if (load.size() >= 3) {
        bool a = false, b = false, c = false;
        const double l1 = load[0].toDouble(&a), l5 = load[1].toDouble(&b), l15 = load[2].toDouble(&c);
        if (a && b && c) {
            info.load1 = l1;
            info.load5 = l5;
            info.load15 = l15;
        }
    }
    return info;
}

SystemInfo SystemInfo::read(const QString &root, const QString &cpuRoot, const QString &powercapRoot) {
    SystemInfo info = readStatic(root, cpuRoot, powercapRoot);
    info.mergeDynamic(readDynamic(root));
    return info;
}

void SystemInfo::mergeDynamic(const SystemInfo &d) {
    uptimeSeconds = d.uptimeSeconds;
    memTotalKb = d.memTotalKb;
    memAvailableKb = d.memAvailableKb;
    memCachedKb = d.memCachedKb;
    commitLimitKb = d.commitLimitKb;
    committedKb = d.committedKb;
    slabReclaimableKb = d.slabReclaimableKb;
    slabUnreclaimableKb = d.slabUnreclaimableKb;
    shmemKb = d.shmemKb;
    buffersKb = d.buffersKb;
    dirtyKb = d.dirtyKb;
    swapTotalKb = d.swapTotalKb;
    swapFreeKb = d.swapFreeKb;
    fileHandles = d.fileHandles;
    load1 = d.load1;
    load5 = d.load5;
    load15 = d.load15;
}

namespace {
QString readLine(const QString &path) {
    return QString::fromUtf8(readAll(path)).trimmed();
}
} // namespace

QString SystemInfo::diskModel(const QString &name, const QString &sysBlock) {
    return readLine(sysBlock + QLatin1Char('/') + name + QLatin1String("/device/model")).simplified();
}

qint64 SystemInfo::diskSizeBytes(const QString &name, const QString &sysBlock) {
    bool ok = false;
    const qint64 sectors = readLine(sysBlock + QLatin1Char('/') + name + QLatin1String("/size")).toLongLong(&ok);
    return ok ? sectors * 512 : -1; // /sys/block/*/size is always in 512-byte units
}

bool SystemInfo::diskIsRotational(const QString &name, const QString &sysBlock) {
    return readLine(sysBlock + QLatin1Char('/') + name + QLatin1String("/queue/rotational")) == QLatin1String("1");
}

QString SystemInfo::netKind(const QString &iface, const QString &sysNet) {
    const QString dir = sysNet + QLatin1Char('/') + iface;
    if (QFileInfo::exists(dir + QLatin1String("/wireless")) || QFileInfo::exists(dir + QLatin1String("/phy80211")))
        return QStringLiteral("Wi-Fi");
    // Virtual devices (bridges, veth, tun...) have no parent device in sysfs.
    if (!QFileInfo::exists(dir + QLatin1String("/device")))
        return QStringLiteral("Virtual");
    return QStringLiteral("Ethernet");
}

int SystemInfo::netSpeedMbps(const QString &iface, const QString &sysNet) {
    bool ok = false;
    const int v = readLine(sysNet + QLatin1Char('/') + iface + QLatin1String("/speed")).toInt(&ok);
    return ok && v > 0 ? v : -1; // the kernel reports -1 when the link speed is unknown
}

QString SystemInfo::netState(const QString &iface, const QString &sysNet) {
    return readLine(sysNet + QLatin1Char('/') + iface + QLatin1String("/operstate"));
}
