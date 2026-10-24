#include "core/SensorHistory.h"

#include <QtTest>

class TstSensorHistory : public QObject {
    Q_OBJECT
private slots:
    void keepsNewestWhenFull() {
        SensorHistory h(3);
        for (int i = 1; i <= 5; ++i)
            h.push(i, double(i));
        const auto s = h.samples();
        QCOMPARE(s.size(), 3);
        QCOMPARE(s[0].value, 3.0);
        QCOMPARE(s[2].value, 5.0);
        QCOMPARE(h.size(), std::size_t(3));
    }

    void missingValueIsAGapNotZero() {
        SensorHistory h(4);
        h.push(1, 20.0);
        h.push(2, std::nullopt);
        const auto s = h.samples();
        QVERIFY(s[0].valid);
        QVERIFY(!s[1].valid);
    }

    void shrinkKeepsNewest() {
        SensorHistory h(5);
        for (int i = 1; i <= 5; ++i)
            h.push(i, double(i));
        h.setCapacity(2);
        const auto s = h.samples();
        QCOMPARE(s.size(), 2);
        QCOMPARE(s[0].value, 4.0);
        h.push(6, 6.0);
        QCOMPARE(h.samples()[1].value, 6.0);
    }

    void growKeepsEverything() {
        SensorHistory h(2);
        h.push(1, 1.0);
        h.push(2, 2.0);
        h.setCapacity(10);
        QCOMPARE(h.samples().size(), 2);
        h.push(3, 3.0);
        QCOMPARE(h.samples().size(), 3);
    }
};

QTEST_APPLESS_MAIN(TstSensorHistory)
#include "tst_sensorhistory.moc"
