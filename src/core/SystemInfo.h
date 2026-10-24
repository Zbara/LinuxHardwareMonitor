#pragma once

#include <QString>
#include <QStringList>

// Static-ish facts shown in the performance details (model name, uptime, RAM
// breakdown, load average). Everything is optional: -1 / empty means unknown.
struct SystemInfo {
    QString cpuModel;
    int logicalCpus = 0;
    double uptimeSeconds = -1;
    qint64 memTotalKb = -1;
    qint64 memAvailableKb = -1;
    qint64 memCachedKb = -1; // Cached + Buffers
    qint64 commitLimitKb = -1, committedKb = -1;
    qint64 slabReclaimableKb = -1, slabUnreclaimableKb = -1;
    qint64 shmemKb = -1, buffersKb = -1, dirtyKb = -1;
    qint64 fileHandles = -1; // allocated system-wide file handles (Task Manager: "Handles")
    qint64 swapTotalKb = -1;
    qint64 swapFreeKb = -1;
    double load1 = -1, load5 = -1, load15 = -1;

    int physicalCores = 0;  // 0 = unknown
    int sockets = 0;
    double maxMhz = -1;     // cpufreq limit of cpu0
    double baseMhz = -1;    // intel_pstate base_frequency, when exposed
    double powerLimitW = -1; // long-term package power limit (RAPL constraint_0), when readable
    bool virtualization = false; // vmx/svm flag present
    qint64 l1Kb = -1, l2Kb = -1, l3Kb = -1; // cpu0 caches (L1 = data + instruction)

    // "48K" / "2M" -> KiB, -1 if unparsable.
    static qint64 parseCacheSizeKb(const QString &text);

    // Facts that never change while the program runs (CPU model, topology, caches, limits) ...
    static SystemInfo readStatic(const QString &procRoot = QStringLiteral("/proc"),
                                 const QString &cpuRoot = QStringLiteral("/sys/devices/system/cpu"),
                                 const QString &powercapRoot = QStringLiteral("/sys/class/powercap"));
    // ... and the numbers that change every second (uptime, memory, swap, load, open files).
    static SystemInfo readDynamic(const QString &procRoot = QStringLiteral("/proc"));
    // Copies the dynamic fields of `dynamic` into this object.
    void mergeDynamic(const SystemInfo &dynamic);

    // Both at once. Reading /proc/cpuinfo is not cheap on many-core machines: refresh loops should
    // call readStatic() once and readDynamic() each time.
    static SystemInfo read(const QString &procRoot = QStringLiteral("/proc"),
                           const QString &cpuRoot = QStringLiteral("/sys/devices/system/cpu"),
                           const QString &powercapRoot = QStringLiteral("/sys/class/powercap"));

    // Block device facts from /sys/block/<name>. Empty / -1 when unknown.
    static QString diskModel(const QString &name, const QString &sysBlock = QStringLiteral("/sys/block"));
    static qint64 diskSizeBytes(const QString &name, const QString &sysBlock = QStringLiteral("/sys/block"));
    static bool diskIsRotational(const QString &name, const QString &sysBlock = QStringLiteral("/sys/block"));

    // Network interface facts from /sys/class/net/<name>.
    static QString netKind(const QString &iface, const QString &sysNet = QStringLiteral("/sys/class/net")); // "Wi-Fi" / "Ethernet" / "Virtual"
    static int netSpeedMbps(const QString &iface, const QString &sysNet = QStringLiteral("/sys/class/net")); // -1 unknown
    static QString netState(const QString &iface, const QString &sysNet = QStringLiteral("/sys/class/net")); // operstate
};
