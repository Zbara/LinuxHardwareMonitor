#pragma once

#include "ISensorProvider.h"

#include <QHash>

#include <optional>

// Network and disk throughput from /proc/net/dev and /proc/diskstats. Rates
// are derived from counter deltas between polls, so the first poll reports
// every rate as unavailable.
class IoProvider : public ISensorProvider {
public:
    explicit IoProvider(QString procRoot = QStringLiteral("/proc"));

    QString name() const override { return QStringLiteral("io"); }
    QVector<SensorReading> poll() override { return pollAt(QDateTime::currentMSecsSinceEpoch()); }

    // Same as poll() with an explicit clock, for tests.
    QVector<SensorReading> pollAt(qint64 nowMs);

    struct Counters {
        quint64 readBytes = 0; // received / read
        quint64 writeBytes = 0; // transmitted / written
        quint64 busyMs = 0;     // disks only: time spent doing I/O
    };
    // Interface or disk name -> cumulative byte counters.
    static QHash<QString, Counters> parseNetDev(const QByteArray &netDev);
    static QHash<QString, Counters> parseDiskStats(const QByteArray &diskStats);

    // Signal level in dBm per wireless interface from /proc/net/wireless.
    static QHash<QString, double> parseWireless(const QByteArray &wireless);

private:
    void addGroup(QVector<SensorReading> &out, const QString &deviceId, const QString &deviceName,
                  const QString &prefix, const QString &readName, const QString &writeName, bool withBusy,
                  const QHash<QString, Counters> &now, QHash<QString, Counters> &prev,
                  double dtSeconds, const QDateTime &stamp);

    QString m_proc;
    qint64 m_prevMs = 0;
    QHash<QString, Counters> m_prevNet;
    QHash<QString, Counters> m_prevDisk;
};
