#include "IoProvider.h"

#include <QFile>
#include <QRegularExpression>

#include <algorithm>

namespace {

QByteArray readAll(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

IoProvider::IoProvider(QString procRoot) : m_proc(std::move(procRoot)) {}

QHash<QString, IoProvider::Counters> IoProvider::parseNetDev(const QByteArray &netDev) {
    QHash<QString, Counters> out;
    for (const QByteArray &line : netDev.split('\n')) {
        const int colon = line.indexOf(':');
        if (colon < 0)
            continue; // header lines
        const QString name = QString::fromLatin1(line.left(colon).trimmed());
        if (name == QLatin1String("lo"))
            continue;
        // rx: bytes packets errs drop fifo frame compressed multicast, tx: bytes ...
        const QList<QByteArray> f = line.mid(colon + 1).simplified().split(' ');
        if (f.size() < 9)
            continue;
        bool okRx = false, okTx = false;
        Counters c;
        c.readBytes = f[0].toULongLong(&okRx);
        c.writeBytes = f[8].toULongLong(&okTx);
        if (okRx && okTx)
            out.insert(name, c);
    }
    return out;
}

QHash<QString, IoProvider::Counters> IoProvider::parseDiskStats(const QByteArray &diskStats) {
    // Whole physical disks only; partitions would double count.
    static const QRegularExpression whole(
        QStringLiteral("^(sd[a-z]+|vd[a-z]+|hd[a-z]+|xvd[a-z]+|nvme\\d+n\\d+|mmcblk\\d+)$"));
    QHash<QString, Counters> out;
    for (const QByteArray &line : diskStats.split('\n')) {
        // major minor name reads merged sectorsRead ms writes merged sectorsWritten ...
        const QList<QByteArray> f = line.simplified().split(' ');
        if (f.size() < 10)
            continue;
        const QString name = QString::fromLatin1(f[2]);
        if (!whole.match(name).hasMatch())
            continue;
        bool okR = false, okW = false, okB = true;
        const quint64 r = f[5].toULongLong(&okR);
        const quint64 w = f[9].toULongLong(&okW);
        const quint64 busy = f.size() > 12 ? f[12].toULongLong(&okB) : 0;
        if (okR && okW && okB)
            out.insert(name, Counters{r * 512, w * 512, busy}); // sectors are always 512 bytes here
    }
    return out;
}

QHash<QString, double> IoProvider::parseWireless(const QByteArray &wireless) {
    QHash<QString, double> out;
    for (const QByteArray &line : wireless.split('\n')) {
        const int colon = line.indexOf(':');
        if (colon < 0)
            continue; // header lines
        // status link level noise ...; the numbers carry a trailing '.'
        const QList<QByteArray> f = line.mid(colon + 1).simplified().split(' ');
        if (f.size() < 3)
            continue;
        QByteArray level = f[2];
        if (level.endsWith('.'))
            level.chop(1);
        bool ok = false;
        const double dbm = level.toDouble(&ok);
        // Drivers that report quality only leave the level at 0 or a raw 0..255 value.
        if (ok && dbm < 0 && dbm > -150)
            out.insert(QString::fromLatin1(line.left(colon).trimmed()), dbm);
    }
    return out;
}

void IoProvider::addGroup(QVector<SensorReading> &out, const QString &deviceId,
                          const QString &deviceName, const QString &prefix, const QString &readName,
                          const QString &writeName, bool withBusy,
                          const QHash<QString, Counters> &now,
                          QHash<QString, Counters> &prev, double dt, const QDateTime &stamp) {
    auto make = [&](const QString &key, const QString &name, std::optional<double> value,
                    SensorType type = SensorType::Throughput, const QString &unit = QStringLiteral("MB/s")) {
        const std::optional<double> &mbps = value;
        SensorReading r;
        r.id = prefix + QLatin1Char('/') + key;
        r.deviceId = deviceId;
        r.deviceName = deviceName;
        r.category = DeviceCategory::Other;
        r.name = name;
        r.type = type;
        r.unit = unit;
        r.available = mbps.has_value();
        r.value = mbps.value_or(0.0);
        r.timestamp = stamp;
        out.push_back(r);
    };

    auto rate = [&](quint64 cur, quint64 old) -> std::optional<double> {
        if (dt <= 0 || cur < old) // counter reset or first sample
            return std::nullopt;
        return double(cur - old) / dt / 1e6;
    };

    double totalRead = 0, totalWrite = 0;
    bool haveTotals = false;
    QStringList names = now.keys();
    std::sort(names.begin(), names.end());
    for (const QString &n : std::as_const(names)) {
        const Counters &c = now[n];
        const auto p = prev.constFind(n);
        std::optional<double> r, w;
        if (p != prev.constEnd()) {
            r = rate(c.readBytes, p->readBytes);
            w = rate(c.writeBytes, p->writeBytes);
        }
        make(n + QLatin1String("_read"), n + QLatin1Char(' ') + readName, r);
        make(n + QLatin1String("_write"), n + QLatin1Char(' ') + writeName, w);
        if (withBusy) {
            // ms of I/O per second of wall time -> percent
            std::optional<double> busy;
            if (p != prev.constEnd() && dt > 0 && c.busyMs >= p->busyMs)
                busy = std::min(100.0, double(c.busyMs - p->busyMs) / (dt * 10.0));
            make(n + QLatin1String("_busy"), n + QStringLiteral(" Active time"), busy,
                 SensorType::Utilization, QStringLiteral("%"));
        }
        if (r && w) {
            totalRead += *r;
            totalWrite += *w;
            haveTotals = true;
        }
    }
    make(QStringLiteral("total_read"), readName + QStringLiteral(" (total)"),
         haveTotals ? std::optional<double>(totalRead) : std::nullopt);
    make(QStringLiteral("total_write"), writeName + QStringLiteral(" (total)"),
         haveTotals ? std::optional<double>(totalWrite) : std::nullopt);
    prev = now;
}

QVector<SensorReading> IoProvider::pollAt(qint64 nowMs) {
    QVector<SensorReading> out;
    const QDateTime stamp = QDateTime::fromMSecsSinceEpoch(nowMs);
    const double dt = m_prevMs > 0 ? double(nowMs - m_prevMs) / 1000.0 : 0.0;

    const auto net = parseNetDev(readAll(m_proc + QLatin1String("/net/dev")));
    const auto disk = parseDiskStats(readAll(m_proc + QLatin1String("/diskstats")));

    addGroup(out, QStringLiteral("net"), QStringLiteral("Network"), QStringLiteral("net"),
             QStringLiteral("Download"), QStringLiteral("Upload"), false, net, m_prevNet, dt, stamp);
    addGroup(out, QStringLiteral("disk"), QStringLiteral("Disks"), QStringLiteral("disk"),
             QStringLiteral("Read"), QStringLiteral("Write"), true, disk, m_prevDisk, dt, stamp);

    const auto wireless = parseWireless(readAll(m_proc + QLatin1String("/net/wireless")));
    QStringList wifi = wireless.keys();
    std::sort(wifi.begin(), wifi.end());
    for (const QString &n : std::as_const(wifi)) {
        SensorReading r;
        r.id = QStringLiteral("net/") + n + QStringLiteral("_signal");
        r.deviceId = QStringLiteral("net");
        r.deviceName = QStringLiteral("Network");
        r.category = DeviceCategory::Other;
        r.name = n + QStringLiteral(" Signal");
        r.type = SensorType::Signal;
        r.unit = QStringLiteral("dBm");
        r.available = true;
        r.value = wireless[n];
        r.timestamp = stamp;
        out.push_back(r);
    }

    m_prevMs = nowMs;
    return out;
}
