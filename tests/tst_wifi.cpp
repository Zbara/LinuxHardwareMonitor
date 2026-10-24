#include "core/WifiInfoMonitor.h"

#include <QtTest>

class TstWifi : public QObject {
    Q_OBJECT
private slots:
    void channelsAndBands() {
        QCOMPARE(WifiHelpers::channelForFrequency(2412), 1);
        QCOMPARE(WifiHelpers::channelForFrequency(2437), 6);
        QCOMPARE(WifiHelpers::channelForFrequency(2484), 14);
        QCOMPARE(WifiHelpers::channelForFrequency(5180), 36);
        QCOMPARE(WifiHelpers::channelForFrequency(5955), 1); // 6 GHz channel 1
        QCOMPARE(WifiHelpers::channelForFrequency(900), -1);
        QCOMPARE(WifiHelpers::bandForFrequency(2412), QString("2.4 GHz"));
        QCOMPARE(WifiHelpers::bandForFrequency(5180), QString("5 GHz"));
        QCOMPARE(WifiHelpers::bandForFrequency(6115), QString("6 GHz"));
        QVERIFY(WifiHelpers::bandForFrequency(0).isEmpty());
    }

    void securityLabels() {
        constexpr uint psk = 0x100, ent = 0x200, sae = 0x400, privacy = 0x1;
        QCOMPARE(WifiHelpers::securityLabel(0, 0, 0), QString("Open"));
        QCOMPARE(WifiHelpers::securityLabel(privacy, 0, 0), QString("WEP"));
        QCOMPARE(WifiHelpers::securityLabel(privacy, psk, 0), QString("WPA"));
        QCOMPARE(WifiHelpers::securityLabel(privacy, 0, psk), QString("WPA2"));
        QCOMPARE(WifiHelpers::securityLabel(privacy, 0, psk | sae), QString("WPA2/WPA3"));
        QCOMPARE(WifiHelpers::securityLabel(privacy, 0, sae), QString("WPA3"));
        QCOMPARE(WifiHelpers::securityLabel(privacy, 0, ent), QString("WPA2-Enterprise"));
    }

    void modesAndSsid() {
        QCOMPARE(WifiHelpers::modeLabel(2), QString("Infrastructure"));
        QVERIFY(WifiHelpers::modeLabel(9).isEmpty());
        QCOMPARE(WifiHelpers::decodeSsid("Caf\xC3\xA9 5G"), QString::fromUtf8("Café 5G"));
    }
};

QTEST_GUILESS_MAIN(TstWifi)
#include "tst_wifi.moc"
