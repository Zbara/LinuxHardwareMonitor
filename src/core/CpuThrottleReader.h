#pragma once

#include <QString>
#include <QStringList>

// Reads the kernel's cumulative thermal throttle count of the processor package(s)
// (/sys/devices/system/cpu/cpuN/thermal_throttle/package_throttle_count, Intel). The count of every
// package is read once per call; the list of files is found on the first call only.
class CpuThrottleReader {
public:
    explicit CpuThrottleReader(QString cpuRoot = QStringLiteral("/sys/devices/system/cpu"));

    // Sum over the packages, -1 when the kernel does not provide the counter (AMD, VMs, old kernels).
    qint64 read();

private:
    QString m_root;
    bool m_discovered = false;
    QStringList m_files;
};
