#include "core/SensorManager.h"

#include <QBuffer>
#include <QtTest>

namespace {

SensorReading reading(const QString &id, double value, bool available = true) {
    SensorReading r;
    r.id = id;
    r.deviceId = "dev";
    r.deviceName = "Device";
    r.name = id;
    r.type = SensorType::Temperature;
    r.unit = "°C";
    r.value = value;
    r.available = available;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}

} // namespace

class TstSensorManager : public QObject {
    Q_OBJECT
private slots:
    void historyStatsAndSingleSensorCsv() {
        SensorManager m;
        QVERIFY(!m.historyStats("a").valid);
        m.applyReadings({reading("a", 40), reading("b", 1)});
        m.applyReadings({reading("a", 60), reading("b", 2)});
        m.applyReadings({reading("a", 0, false), reading("b", 3)}); // a gap is not a sample
        const auto st = m.historyStats("a");
        QVERIFY(st.valid);
        QCOMPARE(st.samples, 2);
        QCOMPARE(st.min, 40.0);
        QCOMPARE(st.max, 60.0);
        QCOMPARE(st.average, 50.0);

        QBuffer all, one;
        all.open(QIODevice::WriteOnly);
        one.open(QIODevice::WriteOnly);
        const int nAll = m.exportCsv(all);
        const int nOne = m.exportCsv(one, "a");
        QVERIFY(nOne > 0 && nOne < nAll);
        QVERIFY(!one.data().contains("\nb,")); // only the requested sensor
        QVERIFY(one.data().contains("\na,"));
    }

    void sensorProxyFiltersByTextAndType() {
        SensorManager m;
        SensorReading cpu = reading("cpu_temp", 50);
        cpu.deviceName = "CPU";
        cpu.name = "Package";
        SensorReading fan = reading("fan1", 1200);
        fan.deviceName = "Chassis";
        fan.name = "Fan 1";
        fan.type = SensorType::FanSpeed;
        SensorReading driver = reading("drv", 0);
        driver.type = SensorType::Text;
        m.applyReadings({cpu, fan, driver});

        SensorProxyModel proxy;
        proxy.setSourceModel(m.model());
        QCOMPARE(proxy.rowCount(), 2); // Text sensors are never listed

        proxy.setFilterText("  PACK ");
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setFilterText("chassis"); // matches the device name too
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setFilterText("");

        proxy.setTypeFilter(static_cast<int>(SensorType::FanSpeed));
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setFilterText("package");
        QCOMPARE(proxy.rowCount(), 0); // both conditions apply
        proxy.setTypeFilter(-1);
        proxy.setFilterText("");
        QCOMPARE(proxy.rowCount(), 2);

        proxy.setFavorites({"fan1"});
        QCOMPARE(proxy.rowCount(), 2); // the flag alone changes nothing
        proxy.setFavoritesOnly(true);
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setFavorites({"fan1", "cpu_temp", "unknown"});
        QCOMPARE(proxy.rowCount(), 2); // ids that no longer exist are harmless
        proxy.setFavorites({});
        QCOMPARE(proxy.rowCount(), 0);
    }

    void tracksMinMax() {
        SensorManager m;
        m.applyReadings({reading("a", 40)});
        m.applyReadings({reading("a", 60)});
        m.applyReadings({reading("a", 50)});
        const auto &row = m.model()->rows()[0];
        QCOMPARE(row.min, 40.0);
        QCOMPARE(row.max, 60.0);
    }

    void dedupesSamePhysicalSensor() {
        SensorManager m;
        m.applyReadings({reading("a", 40), reading("a", 41)});
        QCOMPARE(m.model()->rowCount(), 1);
        QCOMPARE(m.history("a").size(), 1);
    }

    void disappearedSensorBecomesUnavailableThenReturns() {
        SensorManager m;
        m.applyReadings({reading("a", 40), reading("b", 30)});
        m.applyReadings({reading("b", 31)});
        QCOMPARE(m.model()->rowCount(), 2);

        auto availability = [&](const QString &id) {
            for (const auto &row : m.model()->rows())
                if (row.reading.id == id)
                    return row.reading.available;
            return true;
        };
        QVERIFY(!availability("a"));
        QVERIFY(availability("b"));

        const auto h = m.history("a");
        QVERIFY(h[0].valid);
        QVERIFY(!h[1].valid);

        m.applyReadings({reading("a", 45), reading("b", 31)});
        QVERIFY(availability("a"));
        QCOMPARE(m.model()->rowCount(), 2);
    }

    void unavailableReadingIsNeverZeroInModel() {
        SensorManager m;
        m.applyReadings({reading("a", 0, false)});
        const QModelIndex i = m.model()->index(0);
        QVERIFY(!m.model()->data(i, SensorListModel::ValueRole).isValid());
        QVERIFY(!m.model()->data(i, SensorListModel::HasStatsRole).toBool());
    }

    void newSensorsAppearAtRuntime() {
        SensorManager m;
        m.applyReadings({reading("a", 1)});
        m.applyReadings({reading("a", 1), reading("b", 2)});
        QCOMPARE(m.model()->rowCount(), 2);
    }

    void exportsCsvWithGaps() {
        SensorManager m;
        auto r = reading("a,b", 41.5);
        m.applyReadings({r});
        m.applyReadings({}); // sensor vanishes -> gap
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QCOMPARE(m.exportCsv(buf), 2);
        const QList<QByteArray> lines = buf.data().split('\n');
        QCOMPARE(lines[0], QByteArray("sensor_id,device,sensor,unit,timestamp,value"));
        QVERIFY(lines[1].startsWith("\"a,b\",Device,"));
        QVERIFY(lines[1].endsWith(",41.500"));
        QVERIFY(lines[2].endsWith(","));
    }

    void historySurvivesAnExportImportRoundTrip() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        SensorManager a;
        a.setInterval(1000);
        for (int i = 0; i < 5; ++i)
            a.applyReadings({reading("t", 40 + i), reading("gone", 1, false)});
        const QByteArray blob = a.exportHistory(now);
        QVERIFY(!blob.isEmpty());

        SensorManager b;
        QCOMPARE(b.importHistory(blob, now + 20000), 1); // loaded 20 s after saving; "gone" never had a value
        b.applyReadings({reading("t", 99), reading("other", 5)});
        const auto h = b.history("t");
        QCOMPARE(h.size(), 7);                 // 5 restored + a gap marker + the new reading
        QCOMPARE(h[0].value, 40.0);
        QCOMPARE(h[4].value, 44.0);
        QVERIFY(!h[5].valid);                  // the downtime is a gap: no line across it
        QCOMPARE(h[5].timestampMs, h[4].timestampMs + 1);
        QCOMPARE(h[6].value, 99.0);
        // Restored samples keep the times they were taken at (not shifted to the load time).
        QVERIFY(h[4].timestampMs <= now && h[4].timestampMs > now - 5000);
        QCOMPARE(b.history("other").size(), 1); // unknown to the file: starts fresh
    }

    void expiredSamplesGapsAndUnknownSensorsAreHandled() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        SensorManager a;
        a.applyReadings({reading("t", 40)});
        a.applyReadings({}); // sensor vanishes: a gap sample
        a.applyReadings({reading("t", 42)});
        const QByteArray blob = a.exportHistory(now);

        SensorManager fresh;
        fresh.setHistorySeconds(60);
        QCOMPARE(fresh.importHistory(blob, now + 10 * 60 * 1000), 0); // 10 minutes later: everything is too old

        SensorManager soon;
        QCOMPARE(soon.importHistory(blob, now + 1000), 1);
        soon.applyReadings({reading("t", 50)});
        const auto h = soon.history("t");
        QCOMPARE(h.size(), 5);
        QVERIFY(h[0].valid && !h[1].valid && h[2].valid && !h[3].valid && h[4].valid); // gaps stay gaps, not zeros
    }

    void corruptOrForeignHistoryDataIsIgnored() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        SensorManager m;
        QCOMPARE(m.importHistory({}, now), 0);
        QCOMPARE(m.importHistory("definitely not a history file", now), 0);
        QCOMPARE(m.importHistory(qCompress(QByteArray("HWMH garbage"), 1), now), 0);

        SensorManager source;
        source.applyReadings({reading("t", 1)});
        QByteArray good = qUncompress(source.exportHistory(now));
        QCOMPARE(m.importHistory(qCompress(good.left(good.size() - 3), 1), now), 0); // truncated
        good[7] = 9; // an unsupported version (bytes 4..7 are the big-endian version)
        QCOMPARE(m.importHistory(qCompress(good, 1), now), 0);
        m.applyReadings({reading("t", 5)});
        QCOMPARE(m.history("t").size(), 1); // nothing leaked in from the bad files
    }

    void exportKeepsOnlyTheNewestSamplesAndSkipsTextSensors() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        SensorManager a;
        a.setHistorySeconds(3600);
        for (int i = 0; i < 50; ++i) {
            SensorReading text = reading("label", 0);
            text.type = SensorType::Text;
            a.applyReadings({reading("t", i), text});
        }
        SensorManager b;
        b.setHistorySeconds(3600);
        QCOMPARE(b.importHistory(a.exportHistory(now, 10), now + 1000), 1); // "label" is not exported
        b.applyReadings({reading("t", 0)});
        QCOMPARE(b.history("t").size(), 12); // newest 10 + the gap marker + the new reading
        QCOMPARE(b.history("t")[0].value, 40.0);
    }

    void historyCapacityFollowsSettings() {
        SensorManager m;
        m.setInterval(1000);
        m.setHistorySeconds(5);
        for (int i = 0; i < 20; ++i)
            m.applyReadings({reading("a", i)});
        QCOMPARE(m.history("a").size(), 6);
    }
};

QTEST_GUILESS_MAIN(TstSensorManager)
#include "tst_sensormanager.moc"
