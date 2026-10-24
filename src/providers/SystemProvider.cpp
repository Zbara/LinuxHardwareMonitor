#include "SystemProvider.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <algorithm>
#include <cctype>

namespace {

QByteArray readAll(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

SensorReading make(const QString &key, const QString &name, SensorType type, const QString &unit,
                   const std::optional<double> &value, const QDateTime &now) {
    SensorReading r;
    r.id = QStringLiteral("system/") + key;
    r.deviceId = QStringLiteral("system");
    r.deviceName = QStringLiteral("System");
    r.category = DeviceCategory::System;
    r.name = name;
    r.type = type;
    r.unit = unit;
    r.available = value.has_value();
    r.value = value.value_or(0.0);
    r.timestamp = now;
    return r;
}

} // namespace

SystemProvider::SystemProvider(QString procRoot, QString cpuRoot)
    : m_proc(std::move(procRoot)), m_cpu(std::move(cpuRoot)) {}

namespace {

// "cpu[N] user nice system idle iowait irq softirq steal ..."
std::optional<SystemProvider::CpuTimes> parseCpuLine(const QByteArray &line) {
    const QList<QByteArray> parts = line.simplified().split(' ');
    if (parts.size() < 5)
        return std::nullopt;
    SystemProvider::CpuTimes t;
    for (int i = 1; i < std::min<int>(parts.size(), 9); ++i) {
        bool ok = false;
        const quint64 v = parts[i].toULongLong(&ok);
        if (!ok)
            return std::nullopt;
        t.total += v;
        if (i == 4 || i == 5)
            t.idle += v;
    }
    return t;
}

// 0..100, or nullopt when there is no earlier sample or time did not advance.
std::optional<double> utilisation(const std::optional<SystemProvider::CpuTimes> &prev,
                                  const SystemProvider::CpuTimes &cur) {
    if (!prev || cur.total <= prev->total)
        return std::nullopt;
    const double dTotal = double(cur.total - prev->total);
    const double dIdle = double(cur.idle - std::min(cur.idle, prev->idle));
    return std::clamp(100.0 * (1.0 - dIdle / dTotal), 0.0, 100.0);
}

} // namespace

std::optional<SystemProvider::CpuTimes> SystemProvider::parseCpuStat(const QByteArray &procStat) {
    for (const QByteArray &line : procStat.split('\n'))
        if (line.startsWith("cpu "))
            return parseCpuLine(line);
    return std::nullopt;
}

QMap<int, SystemProvider::CpuTimes> SystemProvider::parseCpuStatPerCore(const QByteArray &procStat) {
    QMap<int, CpuTimes> out;
    for (const QByteArray &line : procStat.split('\n')) {
        if (!line.startsWith("cpu") || line.size() < 4 || !std::isdigit(static_cast<unsigned char>(line[3])))
            continue;
        bool ok = false;
        const int idx = line.mid(3, line.indexOf(' ') - 3).toInt(&ok);
        const auto t = parseCpuLine(line);
        if (ok && t)
            out.insert(idx, *t);
    }
    return out;
}

std::optional<SystemProvider::MemInfo> SystemProvider::parseMemInfo(const QByteArray &meminfo) {
    MemInfo m;
    bool haveTotal = false, haveAvail = false;
    for (const QByteArray &line : meminfo.split('\n')) {
        const bool total = line.startsWith("MemTotal:");
        const bool avail = line.startsWith("MemAvailable:");
        if (!total && !avail)
            continue;
        const QList<QByteArray> parts = line.simplified().split(' ');
        bool ok = false;
        const quint64 v = parts.value(1).toULongLong(&ok);
        if (!ok)
            return std::nullopt;
        (total ? m.totalKb : m.availableKb) = v;
        (total ? haveTotal : haveAvail) = true;
    }
    if (!haveTotal || !haveAvail || m.totalKb == 0)
        return std::nullopt;
    return m;
}

std::optional<SystemProvider::Freq> SystemProvider::parseCpuInfoMhz(const QByteArray &cpuinfo) {
    static const QRegularExpression re(QStringLiteral("^cpu MHz\\s*:\\s*([0-9.]+)\\s*$"),
                                       QRegularExpression::MultilineOption);
    const QString text = QString::fromLatin1(cpuinfo);
    double sum = 0, mx = 0;
    int n = 0;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const double v = it.next().captured(1).toDouble();
        sum += v;
        mx = std::max(mx, v);
        ++n;
    }
    if (n == 0)
        return std::nullopt;
    return Freq{sum / n, mx};
}

std::optional<SystemProvider::Freq> SystemProvider::readFrequencies() const {
    // Prefer cpufreq (kHz per core); fall back to /proc/cpuinfo.
    const QStringList cpus = QDir(m_cpu).entryList({QStringLiteral("cpu[0-9]*")},
                                                   QDir::Dirs | QDir::NoDotAndDotDot);
    double sum = 0, mx = 0;
    int n = 0;
    for (const QString &cpu : cpus) {
        bool ok = false;
        const double khz = readAll(m_cpu + QLatin1Char('/') + cpu
                                   + QLatin1String("/cpufreq/scaling_cur_freq"))
                               .trimmed().toDouble(&ok);
        if (!ok)
            continue;
        sum += khz / 1000.0;
        mx = std::max(mx, khz / 1000.0);
        ++n;
    }
    if (n > 0)
        return Freq{sum / n, mx};
    return parseCpuInfoMhz(readAll(m_proc + QLatin1String("/cpuinfo")));
}

QVector<SensorReading> SystemProvider::poll() {
    const QDateTime now = QDateTime::currentDateTime();
    QVector<SensorReading> out;

    // CPU utilisation needs two samples; the first poll has none.
    const QByteArray procStat = readAll(m_proc + QLatin1String("/stat"));
    std::optional<double> util;
    if (const auto cur = parseCpuStat(procStat)) {
        util = utilisation(m_prev, *cur);
        m_prev = cur;
    }
    out.push_back(make("cpu_util", QStringLiteral("CPU Usage"), SensorType::Utilization,
                       QStringLiteral("%"), util, now));

    const auto cores = parseCpuStatPerCore(procStat);
    for (auto it = cores.constBegin(); it != cores.constEnd(); ++it) {
        const auto prev = m_prevCores.contains(it.key()) ? std::optional<CpuTimes>(m_prevCores[it.key()])
                                                         : std::nullopt;
        SensorReading r = make(QStringLiteral("cpu%1_util").arg(it.key()).toLatin1().constData(),
                               QStringLiteral("CPU %1 Usage").arg(it.key()), SensorType::Utilization,
                               QStringLiteral("%"), utilisation(prev, it.value()), now);
        r.deviceId = QStringLiteral("cpu-cores");
        r.deviceName = QStringLiteral("CPU Cores");
        out.push_back(r);
    }
    m_prevCores = cores;

    if (const auto f = readFrequencies()) {
        out.push_back(make("cpu_freq_avg", QStringLiteral("CPU Frequency (avg)"),
                           SensorType::Frequency, QStringLiteral("MHz"), f->avgMhz, now));
        out.push_back(make("cpu_freq_max", QStringLiteral("CPU Frequency (max core)"),
                           SensorType::Frequency, QStringLiteral("MHz"), f->maxMhz, now));
    }

    const auto mem = parseMemInfo(readAll(m_proc + QLatin1String("/meminfo")));
    std::optional<double> memPct, memUsedGb;
    if (mem && mem->availableKb <= mem->totalKb) {
        const double used = double(mem->totalKb - mem->availableKb);
        memPct = 100.0 * used / double(mem->totalKb);
        memUsedGb = used / (1024.0 * 1024.0);
    }
    out.push_back(make("mem_util", QStringLiteral("Memory Usage"), SensorType::Utilization,
                       QStringLiteral("%"), memPct, now));
    out.push_back(make("mem_used", QStringLiteral("Memory Used"), SensorType::Memory,
                       QStringLiteral("GB"), memUsedGb, now));
    return out;
}
