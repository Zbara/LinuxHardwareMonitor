#include "core/ProcessScanner.h"
#include "models/ProcessListModel.h"

#include <QtTest>

namespace {

// Fields after ")" start at state; utime=field 14, stime=15, num_threads=20,
// starttime=22, rss=24.
QByteArray statLine(int pid, const QString &comm, quint64 utime, quint64 stime, int threads = 1,
                    quint64 start = 100, quint64 rss = 10) {
    QStringList f = {"S", "1", "0", "0", "0", "0", "0", "0", "0", "0", "0"};
    f << QString::number(utime) << QString::number(stime) << "0" << "0" << "20" << "0"
      << QString::number(threads) << "0" << QString::number(start) << "0" << QString::number(rss);
    return QByteArray::number(pid) + " (" + comm.toUtf8() + ") " + f.join(' ').toUtf8() + "\n";
}

void write(const QString &path, const QByteArray &content) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

QByteArray cpuLine(quint64 busy, quint64 idle) {
    return "cpu  " + QByteArray::number(busy) + " 0 0 " + QByteArray::number(idle) + " 0 0 0 0 0 0\n";
}

} // namespace

class TstProcesses : public QObject {
    Q_OBJECT
private slots:
    void parsesStatEvenWithTrickyComm() {
        const auto s = ProcessScanner::parseStat(statLine(42, "we ird) (name", 7, 3, 4, 555, 99));
        QVERIFY(s);
        QCOMPARE(s->comm, QString("we ird) (name"));
        QCOMPARE(s->cpuTicks, quint64(10));
        QCOMPARE(s->threads, 4);
        QCOMPARE(s->startTime, quint64(555));
        QCOMPARE(s->rssPages, quint64(99));
        QCOMPARE(s->state, QString("S"));
        QCOMPARE(s->nice, 0);
        QVERIFY(!ProcessScanner::parseStat("garbage"));
        QVERIFY(!ProcessScanner::parseStat("1 (x) S 1"));
    }

    void detailsOfOneProcess() {
        QTemporaryDir dir;
        write(dir.path() + "/uptime", "1000.00 5000.00\n");
        write(dir.path() + "/42/stat", statLine(42, "worker", 300, 100, 3, 50000, 25)); // starttime 50000 ticks
        write(dir.path() + "/42/cmdline", QByteArray("/usr/bin/worker\0--flag", 22));
        write(dir.path() + "/42/stat", statLine(42, "worker", 300, 100, 3, 50000, 25));
        write(dir.path() + "/1/stat", statLine(1, "init", 0, 0)); // the parent (ppid is 1 in statLine)
        QVERIFY(QFile::link("/usr/bin/env", dir.path() + "/42/exe"));
        QDir().mkpath(dir.path() + "/42/fd");
        write(dir.path() + "/42/fd/0", "");
        write(dir.path() + "/42/fd/1", "");

        const QVariantMap d = ProcessScanner::details(42, dir.path());
        QCOMPARE(d["name"].toString(), QString("worker"));
        QCOMPARE(d["ppid"].toInt(), 1);
        QCOMPARE(d["parentName"].toString(), QString("init"));
        QCOMPARE(d["threads"].toInt(), 3);
        QCOMPARE(d["cmdline"].toString(), QString("/usr/bin/worker --flag"));
        QCOMPARE(d["exe"].toString(), QString("/usr/bin/env"));
        QCOMPARE(d["openFiles"].toInt(), 2);
        const double hz = double(sysconf(_SC_CLK_TCK));
        QCOMPARE(d["cpuSeconds"].toDouble(), 400.0 / hz);
        QCOMPARE(d["runningSeconds"].toDouble(), 1000.0 - 50000.0 / hz);

        QVERIFY(ProcessScanner::details(999, dir.path()).isEmpty()); // no such process
        QVERIFY(!d.contains("cwd"));                                  // unreadable -> left out
    }

    void computesCpuShareFromTwoScans() {
        QTemporaryDir dir;
        write(dir.path() + "/stat", cpuLine(100, 900));
        write(dir.path() + "/10/stat", statLine(10, "busy", 100, 0));
        write(dir.path() + "/10/cmdline", QByteArray("/usr/bin/busy\0--flag", 20));
        write(dir.path() + "/abc/stat", "ignored");

        ProcessScanner scanner(dir.path());
        auto first = scanner.scan();
        QCOMPARE(first.size(), 1);
        QCOMPARE(first[0].cpuPercent, 0.0); // needs a baseline
        QCOMPARE(first[0].cmdline, QString("/usr/bin/busy --flag"));
        QCOMPARE(first[0].rssBytes, quint64(10) * quint64(sysconf(_SC_PAGESIZE)));

        // Machine advanced 1000 ticks, process used 250 of them.
        write(dir.path() + "/stat", cpuLine(600, 1400));
        write(dir.path() + "/10/stat", statLine(10, "busy", 300, 50));
        auto second = scanner.scan();
        QCOMPARE(second[0].cpuPercent, 25.0);
    }

    void pidReuseDoesNotProduceBogusCpu() {
        QTemporaryDir dir;
        write(dir.path() + "/stat", cpuLine(100, 900));
        write(dir.path() + "/10/stat", statLine(10, "old", 900, 0, 1, 100));
        ProcessScanner scanner(dir.path());
        scanner.scan();

        write(dir.path() + "/stat", cpuLine(200, 1800));
        write(dir.path() + "/10/stat", statLine(10, "new", 5, 0, 1, 999)); // new start time
        QCOMPARE(scanner.scan()[0].cpuPercent, 0.0);
    }

    void exitedProcessesDisappear() {
        QTemporaryDir dir;
        write(dir.path() + "/stat", cpuLine(1, 1));
        write(dir.path() + "/1/stat", statLine(1, "init", 0, 0));
        write(dir.path() + "/2/stat", statLine(2, "child", 0, 0));
        ProcessScanner scanner(dir.path());
        QCOMPARE(scanner.scan().size(), 2);
        QVERIFY(QDir(dir.path() + "/2").removeRecursively());
        QCOMPARE(scanner.scan().size(), 1);
    }

    void missingProcRootIsEmpty() {
        ProcessScanner scanner("/nonexistent/proc");
        QVERIFY(scanner.scan().isEmpty());
    }

    void diskRateFromProcIo() {
        QTemporaryDir dir;
        write(dir.path() + "/stat", cpuLine(100, 900));
        write(dir.path() + "/10/stat", statLine(10, "writer", 1, 0));
        write(dir.path() + "/10/io", "rchar: 1\nread_bytes: 1000\nwrite_bytes: 2000\n");
        write(dir.path() + "/11/stat", statLine(11, "other", 1, 0)); // no io file: not readable

        ProcessScanner scanner(dir.path());
        scanner.scanAt(1000000);
        write(dir.path() + "/10/io", "rchar: 1\nread_bytes: 3000\nwrite_bytes: 6000\n");
        const auto r = scanner.scanAt(1002000); // 2 s later, +6000 bytes
        for (const auto &p : r) {
            if (p.pid == 10)
                QCOMPARE(p.diskBytesPerSec, 3000.0);
            else
                QCOMPARE(p.diskBytesPerSec, -1.0);
        }
    }

    void groupsProcessesByName() {
        ProcessListModel model;
        auto p = [](int pid, const QString &name, double cpu, quint64 mem) {
            ProcessInfo i;
            i.pid = pid;
            i.name = name;
            i.user = "u";
            i.cpuPercent = cpu;
            i.rssBytes = mem;
            i.threads = 2;
            return i;
        };
        model.update({p(1, "chrome", 5, 100), p(2, "chrome", 10, 200), p(3, "solo", 1, 50)});

        // chrome: group + 2 children; solo: a single row without children.
        QCOMPARE(model.rowCount(), 4);
        auto find = [&](const QString &key) {
            for (int i = 0; i < model.rowCount(); ++i)
                if (model.data(model.index(i), ProcessListModel::KeyRole).toString() == key)
                    return model.index(i);
            return QModelIndex();
        };
        const auto g = find("g:chrome");
        QVERIFY(g.isValid());
        QVERIFY(model.data(g, ProcessListModel::HasChildrenRole).toBool());
        QCOMPARE(model.data(g, ProcessListModel::CountRole).toInt(), 2);
        QCOMPARE(model.data(g, ProcessListModel::CpuRole).toDouble(), 15.0);
        QCOMPARE(model.data(g, ProcessListModel::MemoryRole).toULongLong(), quint64(300));
        QCOMPARE(model.data(g, ProcessListModel::ThreadsRole).toInt(), 4);
        QVERIFY(!model.data(find("g:solo"), ProcessListModel::HasChildrenRole).toBool());
        QCOMPARE(model.data(find("g:solo"), ProcessListModel::PidRole).toInt(), 3);
        QCOMPARE(model.pidsFor("g:chrome").size(), 2);
        QCOMPARE(model.pidsFor("p:2"), QVariantList{2});
    }

    void updatesIncrementally() {
        ProcessListModel model;
        auto p = [](int pid, const QString &name, double cpu) {
            ProcessInfo i;
            i.pid = pid;
            i.name = name;
            i.cpuPercent = cpu;
            return i;
        };
        QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);

        model.update({p(1, "a", 1), p(2, "b", 2)});
        model.update({p(2, "b", 9), p(3, "c", 3)});
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(resets.count(), 0);
        for (int i = 0; i < model.rowCount(); ++i)
            if (model.data(model.index(i), ProcessListModel::NameRole).toString() == "b")
                QCOMPARE(model.data(model.index(i), ProcessListModel::CpuRole).toDouble(), 9.0);
    }

    void proxyKeepsChildrenUnderTheirApplication() {
        ProcessListModel model;
        ProcessProxyModel proxy;
        proxy.setSourceModel(&model);
        auto p = [](int pid, const QString &name, double cpu) {
            ProcessInfo i;
            i.pid = pid;
            i.name = name;
            i.user = "root";
            i.cpuPercent = cpu;
            return i;
        };
        model.update({p(1, "alpha", 5), p(2, "alpha", 5), p(3, "beta", 50), p(4, "gamma", 20)});

        proxy.sortBy("name");
        proxy.sortBy("cpu"); // biggest first
        auto name = [&](int row) { return proxy.data(proxy.index(row, 0), ProcessListModel::NameRole).toString(); };
        // Collapsed: only application rows (children hidden).
        QCOMPARE(proxy.rowCount(), 3);
        QCOMPARE(name(0), QString("beta"));
        QCOMPARE(name(1), QString("gamma")); // 20 %
        QCOMPARE(name(2), QString("alpha")); // 5 % + 5 % = 10 % combined
    }

    void expandingShowsChildrenRightBelowTheGroup() {
        ProcessListModel model;
        ProcessProxyModel proxy;
        proxy.setSourceModel(&model);
        auto p = [](int pid, const QString &name, double cpu) {
            ProcessInfo i;
            i.pid = pid;
            i.name = name;
            i.cpuPercent = cpu;
            return i;
        };
        model.update({p(1, "alpha", 30), p(2, "alpha", 20), p(3, "beta", 40), p(4, "gamma", 1)});
        proxy.sortBy("name");
        proxy.sortBy("cpu");
        QCOMPARE(proxy.rowCount(), 3);

        proxy.toggleExpanded("alpha");
        QCOMPARE(proxy.rowCount(), 5);
        auto key = [&](int row) { return proxy.data(proxy.index(row, 0), ProcessListModel::KeyRole).toString(); };
        // alpha (50 % combined) first, then its processes by CPU, then beta.
        QCOMPARE(key(0), QString("g:alpha"));
        QCOMPARE(key(1), QString("p:1"));
        QCOMPARE(key(2), QString("p:2"));
        QCOMPARE(key(3), QString("g:beta"));
        QVERIFY(proxy.data(proxy.index(0, 0), ProcessProxyModel::ExpandedRole).toBool());

        proxy.toggleExpanded("alpha");
        QCOMPARE(proxy.rowCount(), 3);
    }

    void searchFindsChildrenAndAutoExpands() {
        ProcessListModel model;
        ProcessProxyModel proxy;
        proxy.setSourceModel(&model);
        auto p = [](int pid, const QString &name, const QString &cmd) {
            ProcessInfo i;
            i.pid = pid;
            i.name = name;
            i.cmdline = cmd;
            return i;
        };
        model.update({p(1, "chrome", "/opt/chrome --type=gpu"), p(2, "chrome", "/opt/chrome --type=renderer"),
                      p(3, "bash", "-bash")});
        proxy.setFilterText("renderer");
        // The application row plus the one matching child.
        QCOMPARE(proxy.rowCount(), 2);
        proxy.setFilterText("BASH");
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setFilterText("3");
        QCOMPARE(proxy.rowCount(), 1); // PID of the single-process application
        proxy.setFilterText("");
        QCOMPARE(proxy.rowCount(), 2);
    }

    void shortNumericQueryDoesNotMatchEverything() {
        ProcessListModel model;
        ProcessProxyModel proxy;
        proxy.setSourceModel(&model);
        auto p = [](int pid, const QString &name, const QString &cmd = {}) {
            ProcessInfo i;
            i.pid = pid;
            i.name = name;
            i.user = "zbara";
            i.cmdline = cmd;
            return i;
        };
        // PIDs and command lines full of 1s, names without any.
        model.update({p(1, "init"), p(11, "alpha", "/opt/alpha --port 8081"), p(120, "beta", "/usr/bin/beta -v1"),
                      p(310, "gamma"), p(4128, "delta")});

        proxy.setFilterText("1");
        QCOMPARE(proxy.rowCount(), 1); // exactly PID 1 (init), not every PID containing a 1
        QCOMPARE(proxy.data(proxy.index(0, 0), ProcessListModel::NameRole).toString(), QString("init"));

        proxy.setFilterText("11");
        QCOMPARE(proxy.rowCount(), 1); // PID 11 exactly
        proxy.setFilterText("12");
        QCOMPARE(proxy.rowCount(), 0); // no PID 12; "12" is too short to search command lines

        proxy.setFilterText("808"); // 3+ characters also search command lines
        QCOMPARE(proxy.rowCount(), 1);

        proxy.setFilterText("alp"); // names are substring matches
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setFilterText("ZBA"); // so are owners, case-insensitively
        QCOMPARE(proxy.rowCount(), 5);
    }

    void sortDirectionToggles() {
        ProcessListModel model;
        ProcessProxyModel proxy;
        proxy.setSourceModel(&model);
        ProcessInfo a, b;
        a.pid = 1; a.name = "aaa"; a.cpuPercent = 1;
        b.pid = 2; b.name = "bbb"; b.cpuPercent = 9;
        model.update({a, b});
        proxy.sortBy("name");
        QVERIFY(!proxy.sortDescending());
        auto first = [&] { return proxy.data(proxy.index(0, 0), ProcessListModel::NameRole).toString(); };
        QCOMPARE(first(), QString("aaa"));
        proxy.sortBy("name");
        QCOMPARE(first(), QString("bbb"));
        proxy.sortBy("cpu");
        QCOMPARE(first(), QString("bbb"));
    }
};

QTEST_GUILESS_MAIN(TstProcesses)
#include "tst_processes.moc"
