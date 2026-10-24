#include "HardwareInfo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStorageInfo>

namespace HardwareInfo {

namespace {

QString readLine(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed() : QString();
}

qint64 readNumber(const QString &path) {
    bool ok = false;
    const qint64 v = readLine(path).toLongLong(&ok);
    return ok ? v : -1;
}

QString stripHexPrefix(QString s) {
    if (s.startsWith(QLatin1String("0x")))
        s = s.mid(2);
    return s.toLower();
}

} // namespace

DiskDetails disk(const QString &name, const QString &sysBlock) {
    const QString base = sysBlock + QLatin1Char('/') + name;
    DiskDetails d;
    d.serial = readLine(base + QLatin1String("/device/serial"));
    d.firmware = readLine(base + QLatin1String("/device/firmware_rev"));
    if (d.firmware.isEmpty())
        d.firmware = readLine(base + QLatin1String("/device/rev"));
    const qint64 block = readNumber(base + QLatin1String("/queue/logical_block_size"));
    d.logicalBlockSize = block > 0 ? static_cast<int>(block) : -1;
    d.removable = readLine(base + QLatin1String("/removable")) == QLatin1String("1");

    if (name.startsWith(QLatin1String("nvme")))
        d.interfaceName = QStringLiteral("NVMe");
    else if (name.startsWith(QLatin1String("mmcblk")))
        d.interfaceName = QStringLiteral("MMC");
    else if (name.startsWith(QLatin1String("vd")) || name.startsWith(QLatin1String("xvd")))
        d.interfaceName = QStringLiteral("Virtual");
    else if (name.startsWith(QLatin1String("sd")) || name.startsWith(QLatin1String("hd"))) {
        // sd* covers SATA, SAS and USB; the sysfs path tells them apart.
        const QString real = QFileInfo(base).canonicalFilePath();
        d.interfaceName = real.contains(QLatin1String("/usb")) ? QStringLiteral("USB") : QStringLiteral("SATA");
    }
    return d;
}

QString parentDisk(const QString &deviceName, const QString &sysClassBlock) {
    const QString canonical = QFileInfo(sysClassBlock + QLatin1Char('/') + deviceName).canonicalFilePath();
    if (!canonical.isEmpty() && QFileInfo::exists(canonical + QLatin1String("/partition")))
        return QFileInfo(canonical).dir().dirName(); // partitions live inside their disk's directory
    return deviceName;
}

DiskUsage diskUsage(const QString &diskName, const QVector<Volume> &volumes, const QString &sysClassBlock) {
    DiskUsage u;
    QSet<QString> seen;
    for (const Volume &v : volumes) {
        if (!v.device.startsWith(QLatin1String("/dev/")) || seen.contains(v.device))
            continue;
        seen.insert(v.device);
        if (parentDisk(v.device.mid(5), sysClassBlock) != diskName)
            continue;
        u.usedBytes = (u.usedBytes < 0 ? 0 : u.usedBytes) + (v.totalBytes - v.freeBytes);
        u.totalBytes = (u.totalBytes < 0 ? 0 : u.totalBytes) + v.totalBytes;
    }
    return u;
}

QVector<Volume> mountedVolumes() {
    QVector<Volume> out;
    for (const QStorageInfo &s : QStorageInfo::mountedVolumes()) {
        if (!s.isValid() || !s.isReady())
            continue;
        out.push_back({QString::fromUtf8(s.device()), s.bytesTotal(), s.bytesAvailable()});
    }
    return out;
}

NetDetails net(const QString &iface, const QString &sysNet) {
    const QString base = sysNet + QLatin1Char('/') + iface;
    NetDetails d;
    d.mac = readLine(base + QLatin1String("/address"));
    const QString driver = QFileInfo(base + QLatin1String("/device/driver")).canonicalFilePath();
    d.driver = driver.isEmpty() ? QString() : QFileInfo(driver).fileName();
    const QString dev = QFileInfo(base + QLatin1String("/device")).canonicalFilePath();
    d.busAddress = dev.isEmpty() ? QString() : QFileInfo(dev).fileName();
    d.vendorId = stripHexPrefix(readLine(base + QLatin1String("/device/vendor")));
    d.deviceId = stripHexPrefix(readLine(base + QLatin1String("/device/device")));
    d.mtu = static_cast<int>(readNumber(base + QLatin1String("/mtu")));
    d.duplex = readLine(base + QLatin1String("/duplex"));
    d.rxBytes = readNumber(base + QLatin1String("/statistics/rx_bytes"));
    d.txBytes = readNumber(base + QLatin1String("/statistics/tx_bytes"));
    d.rxPackets = readNumber(base + QLatin1String("/statistics/rx_packets"));
    d.txPackets = readNumber(base + QLatin1String("/statistics/tx_packets"));
    const qint64 rxErr = readNumber(base + QLatin1String("/statistics/rx_errors"));
    const qint64 txErr = readNumber(base + QLatin1String("/statistics/tx_errors"));
    if (rxErr >= 0 && txErr >= 0)
        d.errors = rxErr + txErr;
    const qint64 rxDrop = readNumber(base + QLatin1String("/statistics/rx_dropped"));
    const qint64 txDrop = readNumber(base + QLatin1String("/statistics/tx_dropped"));
    if (rxDrop >= 0 && txDrop >= 0)
        d.dropped = rxDrop + txDrop;
    return d;
}

QPair<QString, QString> pciNames(const QString &vendorId, const QString &deviceId, const QString &pciIdsPath) {
    if (vendorId.isEmpty())
        return {};
    QString path = pciIdsPath;
    if (path.isEmpty()) {
        for (const QString &candidate : {QStringLiteral("/usr/share/hwdata/pci.ids"), QStringLiteral("/usr/share/misc/pci.ids")})
            if (QFileInfo::exists(candidate)) {
                path = candidate;
                break;
            }
    }
    QFile f(path);
    if (path.isEmpty() || !f.open(QIODevice::ReadOnly))
        return {};

    const QByteArray vendor = vendorId.toLower().toLatin1();
    const QByteArray device = deviceId.toLower().toLatin1();
    QString vendorName, deviceName;
    bool inVendor = false;
    while (!f.atEnd()) {
        const QByteArray line = f.readLine();
        if (line.startsWith('#') || line.trimmed().isEmpty())
            continue;
        if (!line.startsWith('\t')) {
            if (inVendor)
                break; // reached the next vendor
            if (line.startsWith(vendor + "  ")) {
                inVendor = true;
                vendorName = QString::fromUtf8(line.mid(vendor.size() + 2)).trimmed();
            }
        } else if (inVendor && !line.startsWith("\t\t") && !device.isEmpty() && line.mid(1).startsWith(device + "  ")) {
            deviceName = QString::fromUtf8(line.mid(1 + device.size() + 2)).trimmed();
            break;
        }
    }
    return {vendorName, deviceName};
}

QString gpuName(const QString &vendorId, const QString &deviceId, const QString &pciIdsPath) {
    const QString vendor = vendorId.toLower();
    const QString shortVendor = vendor == QLatin1String("1002") ? QStringLiteral("AMD")
                                : vendor == QLatin1String("8086") ? QStringLiteral("Intel")
                                : vendor == QLatin1String("10de") ? QStringLiteral("NVIDIA") : QString();
    const QString fallback = shortVendor.isEmpty() ? QStringLiteral("GPU") : QStringLiteral("GPU (%1)").arg(shortVendor);

    QString device = pciNames(vendor, deviceId, pciIdsPath).second;
    if (device.isEmpty())
        return fallback;
    // "Navi 31 [Radeon RX 7900 XT/7900 XTX/7900M]" -> the part in brackets is the product name.
    const int open = device.lastIndexOf(QLatin1Char('['));
    const int close = device.lastIndexOf(QLatin1Char(']'));
    if (open >= 0 && close > open)
        device = device.mid(open + 1, close - open - 1);
    if (shortVendor.isEmpty() || device.startsWith(shortVendor, Qt::CaseInsensitive))
        return device;
    return shortVendor + QLatin1Char(' ') + device;
}

} // namespace HardwareInfo
