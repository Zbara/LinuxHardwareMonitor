#include "CpuThrottleReader.h"

#include <QDir>
#include <QFile>
#include <QSet>

CpuThrottleReader::CpuThrottleReader(QString cpuRoot) : m_root(std::move(cpuRoot)) {}

qint64 CpuThrottleReader::read() {
    if (!m_discovered) {
        m_discovered = true;
        QSet<QString> packages;
        QStringList cpus = QDir(m_root).entryList({QStringLiteral("cpu[0-9]*")}, QDir::Dirs | QDir::NoDotAndDotDot);
        cpus.sort();
        for (const QString &cpu : std::as_const(cpus)) {
            QFile id(m_root + QLatin1Char('/') + cpu + QStringLiteral("/topology/physical_package_id"));
            const QString package = id.open(QIODevice::ReadOnly) ? QString::fromLatin1(id.readAll()).trimmed() : QStringLiteral("0");
            if (packages.contains(package))
                continue; // one reading per package: the counter is the same on all its cores
            const QString file = m_root + QLatin1Char('/') + cpu + QStringLiteral("/thermal_throttle/package_throttle_count");
            if (QFile::exists(file)) {
                packages.insert(package);
                m_files << file;
            }
        }
    }
    if (m_files.isEmpty())
        return -1;
    qint64 total = 0;
    for (const QString &path : std::as_const(m_files)) {
        QFile f(path);
        bool ok = false;
        const qint64 n = f.open(QIODevice::ReadOnly) ? f.readAll().trimmed().toLongLong(&ok) : 0;
        if (!ok)
            return -1;
        total += n;
    }
    return total;
}
