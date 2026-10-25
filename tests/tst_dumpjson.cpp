#include "core/SensorSnapshot.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QtTest>

namespace {

SensorReading make(const QString &id, SensorType type, double v, bool ok = true, const QString &device = "Dev") {
    SensorReading r;
    r.id = id;
    r.deviceId = device;
    r.deviceName = device;
    r.name = id;
    r.type = type;
    r.value = v;
    r.unit = "u";
    r.available = ok;
    return r;
}

QJsonObject runDump(const QStringList &extra, QProcessEnvironment env, int *exitCode = nullptr, QString *err = nullptr) {
    QProcess p;
    p.setProcessEnvironment(env);
    p.start(APP_BINARY, QStringList{"--dump-json", "--compact", "--dump-delay", "100"} + extra);
    if (!p.waitForFinished(20000)) {
        p.kill();
        return {};
    }
    if (exitCode)
        *exitCode = p.exitCode();
    if (err)
        *err = QString::fromUtf8(p.readAllStandardError());
    return QJsonDocument::fromJson(p.readAllStandardOutput()).object();
}

} // namespace

class TstDumpJson : public QObject {
    Q_OBJECT
private slots:
    void unavailableIsNullNeverZero() {
        SensorReading text = make("drv", SensorType::Text, 0);
        text.text = "595.99";
        const QByteArray json = SensorSnapshot::toJson(
            {make("b/temp", SensorType::Temperature, 55.5), make("a/fan", SensorType::FanSpeed, 0, false), text},
            "1.2", "real", QDateTime(QDate(2026, 10, 25), QTime(12, 0), Qt::UTC), true);
        const QJsonObject root = QJsonDocument::fromJson(json).object();
        QCOMPARE(root["program"].toString(), QString("LinuxHardwareMonitor"));
        QCOMPARE(root["version"].toString(), QString("1.2"));
        QCOMPARE(root["mode"].toString(), QString("real"));
        QCOMPARE(root["time"].toString(), QString("2026-10-25T12:00:00Z"));
        QCOMPARE(root["sensor_count"].toInt(), 3);

        QHash<QString, QJsonObject> byId;
        for (const QJsonValue &v : root["sensors"].toArray())
            byId[v.toObject()["id"].toString()] = v.toObject();
        QCOMPARE(byId["b/temp"]["value"].toDouble(), 55.5);
        QCOMPARE(byId["b/temp"]["type"].toString(), QString("temperature"));
        QVERIFY(byId["a/fan"]["value"].isNull());       // not 0
        QCOMPARE(byId["a/fan"]["available"].toBool(), false);
        QCOMPARE(byId["drv"]["text"].toString(), QString("595.99"));
        QVERIFY(!byId["drv"].contains("value"));          // text sensors have no number
        QVERIFY(!json.contains('\n') || json.endsWith('\n')); // compact: a single line
    }

    void sensorsComeOutInAStableOrder() {
        const auto readings = QVector<SensorReading>{make("z", SensorType::Power, 1, true, "B"), make("a", SensorType::Power, 1, true, "B"),
                                                     make("m", SensorType::Power, 1, true, "A")};
        const QJsonArray a = QJsonDocument::fromJson(SensorSnapshot::toJson(readings, "1", "real", {}, true)).object()["sensors"].toArray();
        QStringList ids;
        for (const QJsonValue &v : a)
            ids << v.toObject()["id"].toString();
        QCOMPARE(ids, (QStringList{"m", "a", "z"})); // by device, then id
    }

    void theProgramPrintsASimulationWithoutADisplay() {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove("DISPLAY");
        env.remove("WAYLAND_DISPLAY");
        env.remove("QT_QPA_PLATFORM");
        int code = -1;
        const QJsonObject root = runDump({"--mock", SOURCE_DIR "/mocks/amd-desktop.json"}, env, &code);
        QCOMPARE(code, 0);
        QCOMPARE(root["mode"].toString(), QString("simulation (amd-desktop.json)"));
        QVERIFY(root["sensor_count"].toInt() > 20);
        bool sawCpuLoad = false;
        for (const QJsonValue &v : root["sensors"].toArray()) {
            const QJsonObject s = v.toObject();
            if (s["id"].toString() == "system/cpu_util") {
                sawCpuLoad = true;
                QVERIFY(s["available"].toBool()); // the second sample made the rate available
            }
        }
        QVERIFY(sawCpuLoad);
    }

    void aBadProfileFailsInsteadOfShowingRealData() {
        int code = -1;
        QString err;
        const QJsonObject root = runDump({"--mock", SOURCE_DIR "/mocks/does-not-exist.json"}, QProcessEnvironment::systemEnvironment(), &code, &err);
        QCOMPARE(code, 2);
        QVERIFY(root.isEmpty());          // nothing was printed
        QVERIFY(err.contains("mock profile"));
    }
};

QTEST_GUILESS_MAIN(TstDumpJson)
#include "tst_dumpjson.moc"
