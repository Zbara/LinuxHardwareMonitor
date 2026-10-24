#include "providers/SystemProvider.h"

#include <QtTest>

class TstSystemProvider : public QObject {
    Q_OBJECT

    static void write(const QString &path, const QByteArray &content) {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
    }

    static const SensorReading *find(const QVector<SensorReading> &v, const QString &id) {
        for (const auto &r : v)
            if (r.id == id)
                return &r;
        return nullptr;
    }

private slots:
    void parsesCpuStat() {
        const auto t = SystemProvider::parseCpuStat(
            "cpu  100 0 50 800 50 0 0 0 0 0\ncpu0 1 1 1 1 1 1 1 1 0 0\n");
        QVERIFY(t);
        QCOMPARE(t->total, quint64(1000));
        QCOMPARE(t->idle, quint64(850));
        QVERIFY(!SystemProvider::parseCpuStat("garbage"));
    }

    void parsesMemInfo() {
        const auto m = SystemProvider::parseMemInfo(
            "MemTotal:       16000000 kB\nMemFree: 1 kB\nMemAvailable:    4000000 kB\n");
        QVERIFY(m);
        QCOMPARE(m->totalKb, quint64(16000000));
        QCOMPARE(m->availableKb, quint64(4000000));
        QVERIFY(!SystemProvider::parseMemInfo("MemTotal: 1 kB\n"));
    }

    void parsesCpuInfoMhz() {
        const auto f = SystemProvider::parseCpuInfoMhz("cpu MHz\t\t: 1000.5\ncpu MHz\t\t: 3000.5\n");
        QVERIFY(f);
        QCOMPARE(f->avgMhz, 2000.5);
        QCOMPARE(f->maxMhz, 3000.5);
    }

    void utilisationNeedsTwoSamplesAndIsNeverFaked() {
        QTemporaryDir dir;
        write(dir.path() + "/proc/stat", "cpu  100 0 100 800 0 0 0 0 0 0\n");
        write(dir.path() + "/proc/meminfo", "MemTotal: 8388608 kB\nMemAvailable: 4194304 kB\n");
        SystemProvider p(dir.path() + "/proc", dir.path() + "/cpu");

        auto first = p.poll();
        QVERIFY(!find(first, "system/cpu_util")->available);

        // +100 busy, +100 idle -> 50 %
        write(dir.path() + "/proc/stat", "cpu  150 0 150 900 0 0 0 0 0 0\n");
        auto second = p.poll();
        QVERIFY(find(second, "system/cpu_util")->available);
        QCOMPARE(find(second, "system/cpu_util")->value, 50.0);
        QCOMPARE(find(second, "system/mem_util")->value, 50.0);
        QCOMPARE(find(second, "system/mem_used")->value, 4.0);
    }

    void perCoreUtilisation() {
        const auto cores = SystemProvider::parseCpuStatPerCore(
            "cpu  10 0 0 90 0 0 0 0 0 0\ncpu0 5 0 0 45 0 0 0 0 0 0\ncpu1 5 0 0 45 0 0 0 0 0 0\nintr 1\n");
        QCOMPARE(cores.size(), 2);
        QCOMPARE(cores[1].total, quint64(50));

        QTemporaryDir dir;
        write(dir.path() + "/proc/stat",
              "cpu  100 0 0 100 0 0 0 0 0 0\ncpu0 50 0 0 50 0 0 0 0 0 0\ncpu1 50 0 0 50 0 0 0 0 0 0\n");
        SystemProvider p(dir.path() + "/proc", dir.path() + "/cpu");
        QVERIFY(!find(p.poll(), "system/cpu0_util")->available);
        // cpu0 fully busy, cpu1 fully idle during the interval.
        write(dir.path() + "/proc/stat",
              "cpu  200 0 0 200 0 0 0 0 0 0\ncpu0 150 0 0 50 0 0 0 0 0 0\ncpu1 50 0 0 150 0 0 0 0 0 0\n");
        const auto r = p.poll();
        QCOMPARE(find(r, "system/cpu0_util")->value, 100.0);
        QCOMPARE(find(r, "system/cpu1_util")->value, 0.0);
        QCOMPARE(find(r, "system/cpu0_util")->deviceName, QString("CPU Cores"));
    }

    void frequencyFromCpufreqAndFallback() {
        QTemporaryDir dir;
        write(dir.path() + "/cpu/cpu0/cpufreq/scaling_cur_freq", "1000000\n");
        write(dir.path() + "/cpu/cpu1/cpufreq/scaling_cur_freq", "3000000\n");
        SystemProvider p(dir.path() + "/proc", dir.path() + "/cpu");
        auto r = p.poll();
        QCOMPARE(find(r, "system/cpu_freq_avg")->value, 2000.0);
        QCOMPARE(find(r, "system/cpu_freq_max")->value, 3000.0);

        QTemporaryDir noCpufreq;
        write(noCpufreq.path() + "/proc/cpuinfo", "cpu MHz : 1500.0\n");
        SystemProvider q(noCpufreq.path() + "/proc", noCpufreq.path() + "/cpu");
        QCOMPARE(find(q.poll(), "system/cpu_freq_avg")->value, 1500.0);
    }

    void missingFilesGiveUnavailableNotZero() {
        SystemProvider p("/nonexistent/proc", "/nonexistent/cpu");
        const auto r = p.poll();
        QVERIFY(!r.isEmpty());
        for (const auto &s : r)
            QVERIFY2(!s.available, qPrintable(s.id));
        QVERIFY(!find(r, "system/cpu_freq_avg")); // no frequency source at all
    }
};

QTEST_APPLESS_MAIN(TstSystemProvider)
#include "tst_systemprovider.moc"
