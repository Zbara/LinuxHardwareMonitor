#pragma once

#include <QDBusObjectPath>
#include <QHash>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// SMART data of a drive as udisks2 (org.freedesktop.UDisks2, system bus) reports it. Reading it needs no
// root: udisks2 answers local, active sessions. Anything the drive or udisks2 does not provide is left
// unset (-1 / empty), never guessed.
struct DiskHealth {
    enum class Kind { None, Nvme, Ata };
    enum class Status { Unknown, Good, Warning, Failing };

    Kind kind = Kind::None;
    qint64 powerOnHours = -1;
    double temperatureC = -1.0;       // < 0: unknown
    QStringList criticalWarnings;     // NVMe, e.g. "spare", "temperature"; empty = none
    int percentUsed = -1;             // NVMe wear estimate, may exceed 100
    int availableSpare = -1;          // NVMe, percent
    int spareThreshold = -1;
    qint64 dataReadBytes = -1, dataWrittenBytes = -1;
    qint64 powerCycles = -1, unsafeShutdowns = -1, mediaErrors = -1, errorLogEntries = -1;
    QString selfTestStatus;           // "success", "in_progress", ... (NVMe)
    bool smartFailing = false;        // ATA: the drive says it is failing
    qint64 badSectors = -1;           // ATA

    bool available() const { return kind != Kind::None; }
    Status status() const;
};

// Block device name ("nvme0n1") -> health.
using DiskHealthMap = QHash<QString, DiskHealth>;

namespace DiskHealthParser {

using InterfaceMap = QMap<QString, QVariantMap>;                // interface name -> properties
using ObjectMap = QMap<QDBusObjectPath, InterfaceMap>;           // GetManagedObjects() of udisks2

// What was read from the drive's objects before the NVMe attribute call.
struct Drive {
    QString blockName;     // "nvme0n1", "sda"
    QDBusObjectPath path;  // the drive object, for the SmartGetAttributes call
    QVariantMap nvme;      // org.freedesktop.UDisks2.NVMe.Controller properties
    QVariantMap ata;       // org.freedesktop.UDisks2.Drive.Ata properties
};

// The drives (with their whole-disk block device names) found in a GetManagedObjects() reply.
QVector<Drive> drives(const ObjectMap &objects);

// Builds the health record from the properties and, for NVMe, the SmartGetAttributes() result.
DiskHealth build(const Drive &drive, const QVariantMap &nvmeAttributes);

} // namespace DiskHealthParser

Q_DECLARE_METATYPE(DiskHealthParser::InterfaceMap)
Q_DECLARE_METATYPE(DiskHealthParser::ObjectMap)
Q_DECLARE_METATYPE(DiskHealth)
Q_DECLARE_METATYPE(DiskHealthMap)
