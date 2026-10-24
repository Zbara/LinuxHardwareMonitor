#pragma once

#include <QDateTime>
#include <QHash>
#include <QMetaType>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <optional>

struct ProcessInfo {
    int pid = 0;
    int ppid = 0;
    QString name;
    QString cmdline;
    QString user;
    QString state;       // single letter from /proc/<pid>/stat: R, S, D, Z, ...
    double cpuPercent = 0.0; // share of the whole machine, like Windows Task Manager
    quint64 rssBytes = 0;
    int threads = 0;
    double diskBytesPerSec = -1.0; // -1: /proc/<pid>/io is not readable (other users' processes)
};
Q_DECLARE_METATYPE(QVector<ProcessInfo>)

// Reads /proc/<pid>/stat for every process. CPU usage is computed from the
// difference to the previous scan, so the first scan reports 0 for everybody.
// The proc root is injectable for tests.
class ProcessScanner {
public:
    explicit ProcessScanner(QString procRoot = QStringLiteral("/proc"));

    QVector<ProcessInfo> scan() { return scanAt(QDateTime::currentMSecsSinceEpoch()); }
    // Same as scan() with an explicit clock, for tests.
    QVector<ProcessInfo> scanAt(qint64 nowMs);

    struct Stat {
        QString comm;
        QString state;
        int ppid = 0;
        quint64 cpuTicks = 0; // utime + stime
        int threads = 0;
        quint64 startTime = 0;
        quint64 rssPages = 0;
        int nice = 0;
        quint64 vsizeBytes = 0;
    };
    // comm may contain spaces and parentheses, so fields are located relative
    // to the last ')'.
    static std::optional<Stat> parseStat(const QByteArray &stat);

    // One-off, more expensive look at a single process (for the Properties dialog).
    // Empty map when the process does not exist; unreadable fields are left out.
    static QVariantMap details(int pid, const QString &procRoot = QStringLiteral("/proc"));

private:
    struct Prev {
        quint64 cpuTicks = 0;
        quint64 startTime = 0;
        quint64 diskBytes = 0;
        bool hasDisk = false;
    };

    QString userName(uint uid);
    quint64 totalCpuTicks() const;

    QString m_root;
    QHash<int, Prev> m_prev;
    QHash<int, QString> m_cmdlineCache; // keyed by pid; invalidated when startTime changes
    QHash<int, quint64> m_cmdlineStart;
    QHash<uint, QString> m_users;
    quint64 m_prevTotal = 0;
    qint64 m_prevMs = 0;
    long m_pageSize;
};
