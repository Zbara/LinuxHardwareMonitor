#include "WifiInfoMonitor.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QTimer>

namespace WifiHelpers {

int channelForFrequency(int mhz) {
    if (mhz == 2484)
        return 14;
    if (mhz >= 2412 && mhz <= 2472)
        return (mhz - 2407) / 5;
    if (mhz >= 5160 && mhz <= 5885)
        return (mhz - 5000) / 5;
    if (mhz >= 5955 && mhz <= 7115)
        return (mhz - 5950) / 5; // 6 GHz
    return -1;
}

QString bandForFrequency(int mhz) {
    if (mhz >= 2400 && mhz < 2500)
        return QStringLiteral("2.4 GHz");
    if (mhz >= 5000 && mhz < 5900)
        return QStringLiteral("5 GHz");
    if (mhz >= 5900 && mhz < 7200)
        return QStringLiteral("6 GHz");
    return {};
}

QString securityLabel(uint flags, uint wpaFlags, uint rsnFlags) {
    constexpr uint kPrivacy = 0x1;
    constexpr uint kPsk = 0x100, k8021x = 0x200, kSae = 0x400, kOwe = 0x800;
    if (rsnFlags & (kPsk | k8021x | kSae | kOwe)) {
        if (rsnFlags & kOwe)
            return QStringLiteral("OWE");
        if (rsnFlags & k8021x)
            return QStringLiteral("WPA2-Enterprise");
        if ((rsnFlags & kSae) && (rsnFlags & kPsk))
            return QStringLiteral("WPA2/WPA3");
        return (rsnFlags & kSae) ? QStringLiteral("WPA3") : QStringLiteral("WPA2");
    }
    if (wpaFlags & (kPsk | k8021x))
        return (wpaFlags & k8021x) ? QStringLiteral("WPA-Enterprise") : QStringLiteral("WPA");
    if (flags & kPrivacy)
        return QStringLiteral("WEP");
    return QStringLiteral("Open");
}

QString modeLabel(uint nmMode) {
    switch (nmMode) {
    case 1: return QStringLiteral("Ad-hoc");
    case 2: return QStringLiteral("Infrastructure");
    case 3: return QStringLiteral("Access point");
    }
    return {};
}

QString decodeSsid(const QByteArray &raw) {
    return QString::fromUtf8(raw);
}

} // namespace WifiHelpers

namespace {

constexpr auto kNm = "org.freedesktop.NetworkManager";
constexpr auto kNmPath = "/org/freedesktop/NetworkManager";

QVariant prop(QDBusConnection &bus, const QString &path, const QString &iface, const QString &name) {
    QDBusInterface props(QLatin1String(kNm), path, QStringLiteral("org.freedesktop.DBus.Properties"), bus);
    props.setTimeout(1500);
    QDBusReply<QDBusVariant> reply = props.call(QStringLiteral("Get"), iface, name);
    return reply.isValid() ? reply.value().variant() : QVariant();
}

WifiInfo query(QDBusConnection &bus, const QString &ifaceName) {
    WifiInfo info;
    QDBusInterface manager(QLatin1String(kNm), QLatin1String(kNmPath), QStringLiteral("org.freedesktop.NetworkManager"), bus);
    manager.setTimeout(1500);
    QDBusReply<QDBusObjectPath> device = manager.call(QStringLiteral("GetDeviceByIpIface"), ifaceName);
    if (!device.isValid())
        return info;

    const QString wireless = QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless");
    const QString devicePath = device.value().path();
    const QVariant ap = prop(bus, devicePath, wireless, QStringLiteral("ActiveAccessPoint"));
    const QString apPath = ap.value<QDBusObjectPath>().path();
    if (apPath.isEmpty() || apPath == QLatin1String("/"))
        return info;

    const QString apIface = QStringLiteral("org.freedesktop.NetworkManager.AccessPoint");
    info.connected = true;
    info.ssid = WifiHelpers::decodeSsid(prop(bus, apPath, apIface, QStringLiteral("Ssid")).toByteArray());
    info.bssid = prop(bus, apPath, apIface, QStringLiteral("HwAddress")).toString();
    info.frequencyMhz = prop(bus, apPath, apIface, QStringLiteral("Frequency")).toInt();
    info.channel = WifiHelpers::channelForFrequency(info.frequencyMhz);
    info.band = WifiHelpers::bandForFrequency(info.frequencyMhz);
    info.strengthPercent = prop(bus, apPath, apIface, QStringLiteral("Strength")).toInt();
    info.mode = WifiHelpers::modeLabel(prop(bus, apPath, apIface, QStringLiteral("Mode")).toUInt());
    info.security = WifiHelpers::securityLabel(prop(bus, apPath, apIface, QStringLiteral("Flags")).toUInt(),
                                               prop(bus, apPath, apIface, QStringLiteral("WpaFlags")).toUInt(),
                                               prop(bus, apPath, apIface, QStringLiteral("RsnFlags")).toUInt());
    const uint kbit = prop(bus, devicePath, wireless, QStringLiteral("Bitrate")).toUInt();
    info.bitrateMbps = kbit > 0 ? static_cast<int>(kbit / 1000) : -1;
    return info;
}

} // namespace

class WifiInfoMonitor::Worker : public QObject {
    Q_OBJECT
public slots:
    void start() {
        // A connection of its own: this object lives on the worker thread.
        m_bus = QDBusConnection::connectToBus(QDBusConnection::SystemBus, QStringLiteral("hwmon-wifi-info"));
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &Worker::poll);
        m_timer->start(5000);
        poll();
    }
    void stop() {
        delete m_timer;
        m_timer = nullptr;
        m_bus = QDBusConnection(QString());
        QDBusConnection::disconnectFromBus(QStringLiteral("hwmon-wifi-info"));
    }

signals:
    void updated(const WifiInfoMap &info);

private slots:
    void poll() {
        WifiInfoMap result;
        // Wireless interfaces are the ones with a "wireless" directory in sysfs.
        const QStringList ifaces = QDir(QStringLiteral("/sys/class/net")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &iface : ifaces)
            if (QFileInfo::exists(QStringLiteral("/sys/class/net/%1/wireless").arg(iface)))
                result.insert(iface, query(m_bus, iface));
        emit updated(result);
    }

private:
    QDBusConnection m_bus{QString()};
    QTimer *m_timer = nullptr;
};

WifiInfoMonitor::WifiInfoMonitor(QObject *parent) : QObject(parent), m_worker(new Worker) {
    qRegisterMetaType<WifiInfo>();
    qRegisterMetaType<WifiInfoMap>();
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &Worker::updated, this, &WifiInfoMonitor::updated, Qt::QueuedConnection);
    m_thread.start();
    QMetaObject::invokeMethod(m_worker, "start", Qt::QueuedConnection);
}

WifiInfoMonitor::~WifiInfoMonitor() {
    QMetaObject::invokeMethod(m_worker, "stop", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

#include "WifiInfoMonitor.moc"
