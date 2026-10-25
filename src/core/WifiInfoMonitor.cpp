#include "WifiInfoMonitor.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
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

// Plain method calls: a QDBusInterface would introspect the object on every use, which costs an extra
// round trip per property on this 5 s poll.
QDBusMessage call(QDBusConnection &bus, const QString &path, const QString &iface, const QString &method,
                  const QVariantList &args) {
    QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String(kNm), path, iface, method);
    msg.setArguments(args);
    return bus.call(msg, QDBus::Block, 1500);
}

// All properties of one interface in a single round trip (7 on the access point alone).
QVariantMap allProps(QDBusConnection &bus, const QString &path, const QString &iface) {
    const QDBusMessage reply = call(bus, path, QStringLiteral("org.freedesktop.DBus.Properties"),
                                    QStringLiteral("GetAll"), {iface});
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return {};
    return qdbus_cast<QVariantMap>(reply.arguments().constFirst().value<QDBusArgument>());
}

WifiInfo query(QDBusConnection &bus, const QString &ifaceName) {
    WifiInfo info;
    const QDBusMessage device = call(bus, QLatin1String(kNmPath), QStringLiteral("org.freedesktop.NetworkManager"),
                                     QStringLiteral("GetDeviceByIpIface"), {ifaceName});
    if (device.type() != QDBusMessage::ReplyMessage || device.arguments().isEmpty())
        return info;

    const QString devicePath = device.arguments().constFirst().value<QDBusObjectPath>().path();
    const QVariantMap wireless = allProps(bus, devicePath, QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless"));
    const QString apPath = wireless.value(QStringLiteral("ActiveAccessPoint")).value<QDBusObjectPath>().path();
    if (apPath.isEmpty() || apPath == QLatin1String("/"))
        return info;

    const QVariantMap ap = allProps(bus, apPath, QStringLiteral("org.freedesktop.NetworkManager.AccessPoint"));
    info.connected = true;
    info.ssid = WifiHelpers::decodeSsid(ap.value(QStringLiteral("Ssid")).toByteArray());
    info.bssid = ap.value(QStringLiteral("HwAddress")).toString();
    info.frequencyMhz = ap.value(QStringLiteral("Frequency")).toInt();
    info.channel = WifiHelpers::channelForFrequency(info.frequencyMhz);
    info.band = WifiHelpers::bandForFrequency(info.frequencyMhz);
    info.strengthPercent = ap.value(QStringLiteral("Strength")).toInt();
    info.mode = WifiHelpers::modeLabel(ap.value(QStringLiteral("Mode")).toUInt());
    info.security = WifiHelpers::securityLabel(ap.value(QStringLiteral("Flags")).toUInt(),
                                               ap.value(QStringLiteral("WpaFlags")).toUInt(),
                                               ap.value(QStringLiteral("RsnFlags")).toUInt());
    const uint kbit = wireless.value(QStringLiteral("Bitrate")).toUInt();
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
