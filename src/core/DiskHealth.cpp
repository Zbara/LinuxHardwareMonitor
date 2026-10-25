#include "DiskHealth.h"

namespace {

constexpr auto kDriveIface = "org.freedesktop.UDisks2.Drive";
constexpr auto kNvmeIface = "org.freedesktop.UDisks2.NVMe.Controller";
constexpr auto kAtaIface = "org.freedesktop.UDisks2.Drive.Ata";
constexpr auto kBlockIface = "org.freedesktop.UDisks2.Block";
constexpr auto kPartitionIface = "org.freedesktop.UDisks2.Partition";

qint64 asInt(const QVariant &v, qint64 fallback = -1) {
    bool ok = false;
    const qint64 n = v.toLongLong(&ok);
    return v.isValid() && ok ? n : fallback;
}

} // namespace

DiskHealth::Status DiskHealth::status() const {
    if (kind == Kind::None)
        return Status::Unknown;
    if (smartFailing)
        return Status::Failing;
    if (kind == Kind::Nvme) {
        const bool spareLow = availableSpare >= 0 && spareThreshold >= 0 && availableSpare < spareThreshold;
        if (!criticalWarnings.isEmpty() || spareLow || percentUsed >= 100)
            return Status::Warning;
    }
    if (kind == Kind::Ata && badSectors > 0)
        return Status::Warning;
    return Status::Good;
}

namespace DiskHealthParser {

QVector<Drive> drives(const ObjectMap &objects) {
    QHash<QString, Drive> byPath; // drive object path -> drive
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        if (!it.value().contains(QLatin1String(kDriveIface)))
            continue;
        Drive d;
        d.path = it.key();
        d.nvme = it.value().value(QLatin1String(kNvmeIface));
        d.ata = it.value().value(QLatin1String(kAtaIface));
        if (d.nvme.isEmpty() && d.ata.isEmpty())
            continue; // no SMART interface: nothing to show
        byPath.insert(it.key().path(), d);
    }

    QVector<Drive> result;
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        const InterfaceMap &ifaces = it.value();
        if (!ifaces.contains(QLatin1String(kBlockIface)) || ifaces.contains(QLatin1String(kPartitionIface)))
            continue; // whole disks only
        const QString drivePath = ifaces.value(QLatin1String(kBlockIface)).value(QStringLiteral("Drive"))
                                      .value<QDBusObjectPath>().path();
        auto drive = byPath.find(drivePath);
        if (drive == byPath.end() || !drive->blockName.isEmpty())
            continue; // not a SMART drive, or already named by an earlier block device
        const QString name = it.key().path().section(QLatin1Char('/'), -1);
        if (name.startsWith(QLatin1String("dm_")) || name.startsWith(QLatin1String("loop")))
            continue;
        drive->blockName = name;
    }
    for (const Drive &d : std::as_const(byPath))
        if (!d.blockName.isEmpty())
            result.push_back(d);
    return result;
}

DiskHealth build(const Drive &drive, const QVariantMap &attrs) {
    DiskHealth h;
    if (!drive.nvme.isEmpty()) {
        h.kind = DiskHealth::Kind::Nvme;
        h.powerOnHours = asInt(drive.nvme.value(QStringLiteral("SmartPowerOnHours")));
        const qint64 kelvin = asInt(drive.nvme.value(QStringLiteral("SmartTemperature")));
        h.temperatureC = kelvin > 0 ? double(kelvin) - 273.15 : -1.0;
        h.criticalWarnings = drive.nvme.value(QStringLiteral("SmartCriticalWarning")).toStringList();
        h.selfTestStatus = drive.nvme.value(QStringLiteral("SmartSelftestStatus")).toString();
        h.percentUsed = int(asInt(attrs.value(QStringLiteral("percent_used"))));
        h.availableSpare = int(asInt(attrs.value(QStringLiteral("avail_spare"))));
        h.spareThreshold = int(asInt(attrs.value(QStringLiteral("spare_thresh"))));
        h.dataReadBytes = asInt(attrs.value(QStringLiteral("total_data_read")));
        h.dataWrittenBytes = asInt(attrs.value(QStringLiteral("total_data_written")));
        h.powerCycles = asInt(attrs.value(QStringLiteral("power_cycles")));
        h.unsafeShutdowns = asInt(attrs.value(QStringLiteral("unsafe_shutdowns")));
        h.mediaErrors = asInt(attrs.value(QStringLiteral("media_errors")));
        h.errorLogEntries = asInt(attrs.value(QStringLiteral("num_err_log_entries")));
    } else if (!drive.ata.isEmpty()) {
        const QVariantMap &a = drive.ata;
        // udisks2 reports SmartEnabled/SmartSupported; without SMART there is no health to show.
        if (!a.value(QStringLiteral("SmartSupported")).toBool() || !a.value(QStringLiteral("SmartEnabled")).toBool())
            return h;
        h.kind = DiskHealth::Kind::Ata;
        h.smartFailing = a.value(QStringLiteral("SmartFailing")).toBool();
        const qint64 seconds = asInt(a.value(QStringLiteral("SmartPowerOnSeconds")));
        h.powerOnHours = seconds >= 0 ? seconds / 3600 : -1;
        const double kelvin = a.value(QStringLiteral("SmartTemperature")).toDouble();
        h.temperatureC = kelvin > 0 ? kelvin - 273.15 : -1.0;
        h.badSectors = asInt(a.value(QStringLiteral("SmartNumBadSectors")));
        h.selfTestStatus = a.value(QStringLiteral("SmartSelftestStatus")).toString();
    }
    return h;
}

} // namespace DiskHealthParser
