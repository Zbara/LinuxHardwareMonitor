#pragma once

#include <QPair>
#include <QString>
#include <QVector>

// Static-ish hardware facts read from sysfs / pci.ids. Every root is injectable
// for tests; unknown values are empty strings or -1, never made up.
namespace HardwareInfo {

struct DiskDetails {
    QString serial;
    QString firmware;
    QString interfaceName; // NVMe / SATA / USB / MMC / Virtual
    int logicalBlockSize = -1;
    bool removable = false;
};
DiskDetails disk(const QString &name, const QString &sysBlock = QStringLiteral("/sys/block"));

// The whole disk a block device belongs to ("nvme0n1p2" -> "nvme0n1").
QString parentDisk(const QString &deviceName, const QString &sysClassBlock = QStringLiteral("/sys/class/block"));

struct Volume {
    QString device;   // e.g. "/dev/nvme0n1p2"
    qint64 totalBytes = 0;
    qint64 freeBytes = 0;
};
struct DiskUsage {
    qint64 usedBytes = -1;
    qint64 totalBytes = -1;
};
// Used / total space of the filesystems mounted from partitions of `disk`
// (each device counted once, so bind mounts do not double count).
DiskUsage diskUsage(const QString &disk, const QVector<Volume> &volumes,
                    const QString &sysClassBlock = QStringLiteral("/sys/class/block"));
QVector<Volume> mountedVolumes(); // from QStorageInfo

// True when the temperature sensor of `controller` (the parent device of an hwmon sensor, "nvme1" or "sda")
// belongs to the block device `disk`: the same name or a namespace/partition suffix ("nvme1n1", "nvme1n1p2"),
// never a longer name that merely starts the same way ("nvme10n1").
bool diskBelongsToController(const QString &disk, const QString &controller);

struct NetDetails {
    QString mac;
    QString driver;
    QString busAddress;      // PCI address, e.g. 0000:00:14.3
    QString vendorId, deviceId; // "8086", "7af0"
    int mtu = -1;
    QString duplex;
    qint64 rxBytes = -1, txBytes = -1, rxPackets = -1, txPackets = -1, errors = -1, dropped = -1;
};
NetDetails net(const QString &iface, const QString &sysNet = QStringLiteral("/sys/class/net"));
// Only the fields that change at run time (MTU, duplex, counters). `net()` also resolves the device
// links and PCI ids, which is wasteful to repeat every second for every interface.
NetDetails netCounters(const QString &iface, const QString &sysNet = QStringLiteral("/sys/class/net"));

// Looks a PCI vendor/device up in pci.ids; either part may be empty when unknown.
QPair<QString, QString> pciNames(const QString &vendorId, const QString &deviceId, const QString &pciIdsPath = {});

// Display name for a GPU from its PCI ids, e.g. "AMD Radeon RX 7900 XT/7900 XTX/7900M" or
// "Intel Iris Xe Graphics". Uses the bracketed marketing name from pci.ids when there is one.
// Falls back to "GPU (AMD)" / "GPU (Intel)" / "GPU (NVIDIA)" / "GPU" when the id is unknown.
QString gpuName(const QString &vendorId, const QString &deviceId, const QString &pciIdsPath = {});

} // namespace HardwareInfo
