#include "ProcessScanner.h"

#include "providers/SystemProvider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <pwd.h>
#include <unistd.h>

#include <algorithm>

namespace {

QByteArray readAll(const QString &path, qint64 maxBytes = -1) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return maxBytes < 0 ? f.readAll() : f.read(maxBytes);
}

} // namespace

ProcessScanner::ProcessScanner(QString procRoot)
    : m_root(std::move(procRoot)), m_pageSize(sysconf(_SC_PAGESIZE)) {}

std::optional<ProcessScanner::Stat> ProcessScanner::parseStat(const QByteArray &stat) {
    const int open = stat.indexOf('(');
    const int close = stat.lastIndexOf(')');
    if (open < 0 || close < open)
        return std::nullopt;

    // Fields after ")": state ppid ... (field 3 onwards in proc(5)).
    const QList<QByteArray> f = stat.mid(close + 1).simplified().split(' ');
    if (f.size() < 22)
        return std::nullopt;

    auto num = [&](int idx, bool *ok) { return f[idx].toULongLong(ok); };
    bool ok1, ok2, ok3, ok4, ok5, ok6, ok7, ok8;
    Stat s;
    s.comm = QString::fromUtf8(stat.mid(open + 1, close - open - 1));
    s.state = QString::fromLatin1(f[0]);
    s.ppid = f[1].toInt(&ok1);
    s.cpuTicks = num(11, &ok2) + num(12, &ok3); // utime + stime
    s.threads = f[17].toInt(&ok4);
    s.startTime = num(19, &ok5);
    s.rssPages = num(21, &ok6);
    s.nice = f[16].toInt(&ok7);
    s.vsizeBytes = num(20, &ok8);
    if (!(ok1 && ok2 && ok3 && ok4 && ok5 && ok6 && ok7 && ok8))
        return std::nullopt;
    return s;
}

QString ProcessScanner::userName(uint uid) {
    auto it = m_users.constFind(uid);
    if (it != m_users.constEnd())
        return *it;
    QString name = QString::number(uid);
    char buf[1024];
    passwd pw;
    passwd *result = nullptr;
    if (getpwuid_r(uid, &pw, buf, sizeof buf, &result) == 0 && result)
        name = QString::fromLocal8Bit(result->pw_name);
    m_users.insert(uid, name);
    return name;
}

quint64 ProcessScanner::totalCpuTicks() const {
    const auto t = SystemProvider::parseCpuStat(readAll(m_root + QLatin1String("/stat")));
    return t ? t->total : 0;
}

// read_bytes + write_bytes from /proc/<pid>/io; nullopt when not permitted.
static std::optional<quint64> readDiskBytes(const QString &path) {
    const QByteArray raw = readAll(path);
    if (raw.isEmpty())
        return std::nullopt;
    quint64 total = 0;
    int found = 0;
    for (const QByteArray &line : raw.split('\n')) {
        if (line.startsWith("read_bytes:") || line.startsWith("write_bytes:")) {
            bool ok = false;
            const quint64 v = line.mid(line.indexOf(':') + 1).trimmed().toULongLong(&ok);
            if (ok) {
                total += v;
                ++found;
            }
        }
    }
    return found == 2 ? std::optional<quint64>(total) : std::nullopt;
}

QVector<ProcessInfo> ProcessScanner::scanAt(qint64 nowMs) {
    QVector<ProcessInfo> out;
    const double dtSec = m_prevMs > 0 ? double(nowMs - m_prevMs) / 1000.0 : 0.0;
    const quint64 total = totalCpuTicks();
    const quint64 dTotal = total > m_prevTotal ? total - m_prevTotal : 0;

    QHash<int, Prev> nextPrev;
    const QStringList entries = QDir(m_root).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool isPid = false;
        const int pid = entry.toInt(&isPid);
        if (!isPid || pid <= 0)
            continue;

        const QString dir = m_root + QLatin1Char('/') + entry;
        const auto st = parseStat(readAll(dir + QLatin1String("/stat")));
        if (!st)
            continue; // process exited between listing and reading

        ProcessInfo p;
        p.pid = pid;
        p.ppid = st->ppid;
        p.name = st->comm;
        p.state = st->state;
        p.threads = st->threads;
        p.rssBytes = st->rssPages * static_cast<quint64>(m_pageSize);
        p.user = userName(QFileInfo(dir).ownerId());

        // /proc/<pid>/cmdline is constant for a process, so read it once.
        if (m_cmdlineStart.value(pid, ~quint64(0)) != st->startTime || !m_cmdlineCache.contains(pid)) {
            QByteArray raw = readAll(dir + QLatin1String("/cmdline"), 4096);
            raw.replace('\0', ' ');
            m_cmdlineCache[pid] = QString::fromUtf8(raw.trimmed());
            m_cmdlineStart[pid] = st->startTime;
        }
        p.cmdline = m_cmdlineCache.value(pid);

        const auto prev = m_prev.constFind(pid);
        if (prev != m_prev.constEnd() && prev->startTime == st->startTime && dTotal > 0
            && st->cpuTicks >= prev->cpuTicks) {
            p.cpuPercent = std::min(100.0, 100.0 * double(st->cpuTicks - prev->cpuTicks) / double(dTotal));
        }
        Prev cur{st->cpuTicks, st->startTime, 0, false};
        if (const auto disk = readDiskBytes(dir + QLatin1String("/io"))) {
            cur.diskBytes = *disk;
            cur.hasDisk = true;
            if (prev != m_prev.constEnd() && prev->startTime == st->startTime && prev->hasDisk && dtSec > 0
                && *disk >= prev->diskBytes)
                p.diskBytesPerSec = double(*disk - prev->diskBytes) / dtSec;
            else
                p.diskBytesPerSec = 0.0;
        }
        nextPrev.insert(pid, cur);
        out.push_back(p);
    }

    // Forget processes that are gone.
    for (auto it = m_cmdlineCache.begin(); it != m_cmdlineCache.end();) {
        if (!nextPrev.contains(it.key())) {
            m_cmdlineStart.remove(it.key());
            it = m_cmdlineCache.erase(it);
        } else {
            ++it;
        }
    }
    m_prev = std::move(nextPrev);
    m_prevTotal = total;
    m_prevMs = nowMs;
    return out;
}

QVariantMap ProcessScanner::details(int pid, const QString &procRoot) {
    const QString dir = procRoot + QLatin1Char('/') + QString::number(pid);
    const auto st = parseStat(readAll(dir + QLatin1String("/stat")));
    if (!st)
        return {};

    QVariantMap m;
    const long ticks = sysconf(_SC_CLK_TCK);
    const long page = sysconf(_SC_PAGESIZE);
    m["pid"] = pid;
    m["ppid"] = st->ppid;
    m["name"] = st->comm;
    m["state"] = st->state;
    m["nice"] = st->nice;
    m["threads"] = st->threads;
    m["rssBytes"] = static_cast<qulonglong>(st->rssPages) * static_cast<qulonglong>(page);
    m["vsizeBytes"] = static_cast<qulonglong>(st->vsizeBytes);
    if (ticks > 0) {
        m["cpuSeconds"] = double(st->cpuTicks) / double(ticks);
        bool ok = false;
        const double uptime = readAll(procRoot + QLatin1String("/uptime")).split(' ').value(0).toDouble(&ok);
        if (ok)
            m["runningSeconds"] = std::max(0.0, uptime - double(st->startTime) / double(ticks));
    }

    struct passwd pw;
    struct passwd *res = nullptr;
    char buf[1024];
    if (getpwuid_r(QFileInfo(dir).ownerId(), &pw, buf, sizeof buf, &res) == 0 && res)
        m["user"] = QString::fromLocal8Bit(res->pw_name);

    QByteArray cmd = readAll(dir + QLatin1String("/cmdline"), 8192);
    cmd.replace('\0', ' ');
    m["cmdline"] = QString::fromUtf8(cmd.trimmed());

    // These need to be our own process (or root); a failed readlink just means "unknown".
    const QString exe = QFileInfo(dir + QLatin1String("/exe")).symLinkTarget();
    if (!exe.isEmpty())
        m["exe"] = exe;
    const QString cwd = QFileInfo(dir + QLatin1String("/cwd")).symLinkTarget();
    if (!cwd.isEmpty())
        m["cwd"] = cwd;
    const QDir fds(dir + QLatin1String("/fd"));
    if (fds.exists() && fds.isReadable())
        m["openFiles"] = static_cast<int>(fds.entryList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::System).size());

    const auto parent = parseStat(readAll(procRoot + QLatin1Char('/') + QString::number(st->ppid) + QLatin1String("/stat")));
    if (parent)
        m["parentName"] = parent->comm;
    return m;
}
