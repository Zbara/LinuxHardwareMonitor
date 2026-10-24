#pragma once

#include "ISensorProvider.h"

#include <QMap>

#include <optional>

// CPU load, CPU frequency and RAM usage from procfs/sysfs. Both roots are
// injectable so the provider can be tested against a fake tree.
class SystemProvider : public ISensorProvider {
public:
    explicit SystemProvider(QString procRoot = QStringLiteral("/proc"),
                            QString cpuRoot = QStringLiteral("/sys/devices/system/cpu"));

    QString name() const override { return QStringLiteral("system"); }
    QVector<SensorReading> poll() override;

    struct CpuTimes {
        quint64 total = 0;
        quint64 idle = 0;
    };
    // Parses the aggregate "cpu" line of /proc/stat.
    static std::optional<CpuTimes> parseCpuStat(const QByteArray &procStat);

    // Per logical CPU ("cpu0", "cpu1", ...) from /proc/stat, keyed by index.
    static QMap<int, CpuTimes> parseCpuStatPerCore(const QByteArray &procStat);

    struct MemInfo {
        quint64 totalKb = 0;
        quint64 availableKb = 0;
    };
    static std::optional<MemInfo> parseMemInfo(const QByteArray &meminfo);

    // Average and maximum per-core frequency in MHz from /proc/cpuinfo.
    struct Freq {
        double avgMhz = 0;
        double maxMhz = 0;
    };
    static std::optional<Freq> parseCpuInfoMhz(const QByteArray &cpuinfo);

private:
    std::optional<Freq> readFrequencies() const;

    QString m_proc;
    QString m_cpu;
    std::optional<CpuTimes> m_prev;
    QMap<int, CpuTimes> m_prevCores;
};
