#pragma once

#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QThread>

// Details of the access point a Wi-Fi interface is connected to.
struct WifiInfo {
    bool connected = false;
    QString ssid;
    QString bssid;
    int frequencyMhz = -1;
    int channel = -1;
    QString band;          // "2.4 GHz" / "5 GHz" / "6 GHz"
    int bitrateMbps = -1;  // current link rate
    int strengthPercent = -1;
    QString security;      // Open / WEP / WPA / WPA2 / WPA2/WPA3 / WPA3 / ...
    QString mode;          // Infrastructure / Ad-hoc / Access point
};
using WifiInfoMap = QHash<QString, WifiInfo>; // interface name -> details
Q_DECLARE_METATYPE(WifiInfo)
Q_DECLARE_METATYPE(WifiInfoMap)

namespace WifiHelpers {
int channelForFrequency(int mhz);                // -1 when outside the Wi-Fi bands
QString bandForFrequency(int mhz);               // empty when outside the Wi-Fi bands
// NetworkManager AccessPoint flag words -> a short security label.
QString securityLabel(uint flags, uint wpaFlags, uint rsnFlags);
QString modeLabel(uint nmMode);
QString decodeSsid(const QByteArray &raw);
} // namespace WifiHelpers

// Asks NetworkManager (system D-Bus, no root) about the connected access point of every
// wireless interface, every few seconds on its own thread. Missing NetworkManager just means
// no details.
class WifiInfoMonitor : public QObject {
    Q_OBJECT
public:
    explicit WifiInfoMonitor(QObject *parent = nullptr);
    ~WifiInfoMonitor() override;

signals:
    void updated(const WifiInfoMap &info);

private:
    class Worker;
    QThread m_thread;
    Worker *m_worker;
};
