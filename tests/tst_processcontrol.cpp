#include "core/ProcessControl.h"
#include "core/ProcessScanner.h"

#include <QFile>
#include <QProcess>
#include <QtTest>

#include <sys/resource.h>
#include <unistd.h>

#include <cerrno>

class TstProcessControl : public QObject {
    Q_OBJECT
private slots:
    void refusesInitAndSelfAndNegativePids() {
        QCOMPARE(ProcessControl::sendSignal(1, false), EINVAL);
        QCOMPARE(ProcessControl::sendSignal(0, true), EINVAL);
        QCOMPARE(ProcessControl::sendSignal(-1, true), EINVAL); // kill(-1) would signal everything
        QCOMPARE(ProcessControl::sendSignal(int(getpid()), true), EINVAL);
        QCOMPARE(ProcessControl::setPriority(1, 10), EINVAL);
        QCOMPARE(ProcessControl::setPriority(int(getpid()), 10), EINVAL);
    }

    void terminatesAChildProcess() {
        QProcess child;
        child.start("sleep", {"60"});
        QVERIFY(child.waitForStarted());
        const int pid = int(child.processId()); // processId() reads 0 once the child has exited
        QCOMPARE(ProcessControl::sendSignal(pid, false), 0);
        QVERIFY(child.waitForFinished(3000));
        QCOMPARE(child.exitStatus(), QProcess::CrashExit); // died from SIGTERM
        // Already gone: ESRCH, which callers treat as "nothing to do".
        QCOMPARE(ProcessControl::sendSignal(pid, false), ESRCH);
    }

    void suspendAndResumeAChild() {
        QProcess child;
        child.start("sleep", {"60"});
        QVERIFY(child.waitForStarted());
        const int pid = int(child.processId());
        auto state = [pid] {
            QFile f(QString("/proc/%1/stat").arg(pid));
            if (!f.open(QIODevice::ReadOnly))
                return QChar();
            const QByteArray stat = f.readAll();
            return QChar(stat.at(stat.lastIndexOf(") ") + 2)); // the state letter follows the command name
        };
        QCOMPARE(ProcessControl::setSuspended(pid, true), 0);
        QTRY_COMPARE_WITH_TIMEOUT(state(), QChar('T'), 2000);
        QCOMPARE(ProcessControl::setSuspended(pid, false), 0);
        QTRY_VERIFY_WITH_TIMEOUT(state() != QChar('T'), 2000);
        QCOMPARE(ProcessControl::setSuspended(1, true), EINVAL);          // never init
        QCOMPARE(ProcessControl::setSuspended(int(getpid()), true), EINVAL); // never ourselves
        child.kill();
        child.waitForFinished(3000);
        QCOMPARE(ProcessControl::setSuspended(pid, false), ESRCH);
    }

    void forceKillWorks() {
        QProcess child;
        child.start("sleep", {"60"});
        QVERIFY(child.waitForStarted());
        QCOMPARE(ProcessControl::sendSignal(int(child.processId()), true), 0);
        QVERIFY(child.waitForFinished(3000));
    }

    void loweringPriorityWorksButRaisingNeedsPrivileges() {
        QProcess child;
        child.start("sleep", {"60"});
        QVERIFY(child.waitForStarted());
        const int pid = int(child.processId());

        QCOMPARE(ProcessControl::setPriority(pid, 10), 0);
        errno = 0;
        QCOMPARE(getpriority(PRIO_PROCESS, id_t(pid)), 10);
        // The details reader sees the new nice value too.
        QCOMPARE(ProcessScanner::details(pid)["nice"].toInt(), 10);

        if (geteuid() != 0) // root may raise it again
            QCOMPARE(ProcessControl::setPriority(pid, -5), EACCES);
        // Out-of-range values are clamped, not rejected.
        QCOMPARE(ProcessControl::setPriority(pid, 100), 0);
        QCOMPARE(getpriority(PRIO_PROCESS, id_t(pid)), 19);

        child.kill();
        child.waitForFinished(3000);
    }

    void affinityRoundTrip() {
        QProcess child;
        child.start("sleep", {"60"});
        QVERIFY(child.waitForStarted());
        const int pid = int(child.processId());

        int err = -1;
        const QVector<int> original = ProcessControl::affinity(pid, &err);
        QCOMPARE(err, 0);
        QVERIFY(!original.isEmpty());

        // Pin to a single CPU that is currently allowed, then read it back.
        QCOMPARE(ProcessControl::setAffinity(pid, {original.first()}), 0);
        QCOMPARE(ProcessControl::affinity(pid), QVector<int>({original.first()}));

        // Invalid requests are rejected without touching the process.
        QCOMPARE(ProcessControl::setAffinity(pid, {}), EINVAL);
        QCOMPARE(ProcessControl::setAffinity(pid, {-1}), EINVAL);
        QCOMPARE(ProcessControl::setAffinity(1, {0}), EINVAL);
        QCOMPARE(ProcessControl::affinity(pid), QVector<int>({original.first()}));

        // A CPU that does not exist is refused by the kernel.
        QVERIFY(ProcessControl::setAffinity(pid, {900}) != 0);

        QCOMPARE(ProcessControl::setAffinity(pid, original), 0);
        QCOMPARE(ProcessControl::affinity(pid), original);

        child.kill();
        child.waitForFinished(3000);

        ProcessControl::affinity(999999999, &err); // no such process
        QVERIFY(err != 0);
    }

    void detailsOfARealProcess() {
        const QVariantMap d = ProcessScanner::details(int(getpid()));
        QCOMPARE(d["pid"].toInt(), int(getpid()));
        QVERIFY(d["threads"].toInt() >= 1);
        QVERIFY(d["rssBytes"].toULongLong() > 0);
        QVERIFY(d["runningSeconds"].toDouble() >= 0);
        QVERIFY(!d["exe"].toString().isEmpty()); // our own process: readable
        QVERIFY(d["openFiles"].toInt() > 0);
        QVERIFY(ProcessScanner::details(999999999).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstProcessControl)
#include "tst_processcontrol.moc"
