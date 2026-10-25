#include "DiskHealthMonitor.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QMetaObject>
#include <QTimer>

namespace {

constexpr auto kService = "org.freedesktop.UDisks2";
constexpr auto kPath = "/org/freedesktop/UDisks2";
constexpr int kPollMs = 60 * 1000;

} // namespace

class DiskHealthMonitor::Worker : public QObject {
    Q_OBJECT
public slots:
    void setActive(bool active) {
        if (!m_timer) {
            // A connection of its own: this object lives on the worker thread.
            m_bus = QDBusConnection::connectToBus(QDBusConnection::SystemBus, QStringLiteral("hwmon-disk-health"));
            m_timer = new QTimer(this);
            connect(m_timer, &QTimer::timeout, this, &Worker::poll);
        }
        if (active) {
            m_timer->start(kPollMs);
            poll();
        } else {
            m_timer->stop();
        }
    }
    void stop() {
        delete m_timer;
        m_timer = nullptr;
        m_bus = QDBusConnection(QString());
        QDBusConnection::disconnectFromBus(QStringLiteral("hwmon-disk-health"));
    }

signals:
    void updated(const DiskHealthMap &health);

private slots:
    void poll() {
        DiskHealthMap result;
        QDBusMessage get = QDBusMessage::createMethodCall(QLatin1String(kService), QLatin1String(kPath),
                                                          QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                                                          QStringLiteral("GetManagedObjects"));
        const QDBusMessage reply = m_bus.call(get, QDBus::Block, 5000);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
            const auto objects = qdbus_cast<DiskHealthParser::ObjectMap>(reply.arguments().constFirst().value<QDBusArgument>());
            for (const DiskHealthParser::Drive &drive : DiskHealthParser::drives(objects)) {
                QVariantMap attrs;
                if (!drive.nvme.isEmpty()) {
                    // The detailed NVMe log (wear, data written, errors); allowed for the local session.
                    QDBusMessage call = QDBusMessage::createMethodCall(
                        QLatin1String(kService), drive.path.path(), QStringLiteral("org.freedesktop.UDisks2.NVMe.Controller"),
                        QStringLiteral("SmartGetAttributes"));
                    call << QVariantMap();
                    const QDBusMessage r = m_bus.call(call, QDBus::Block, 10000);
                    if (r.type() == QDBusMessage::ReplyMessage && !r.arguments().isEmpty())
                        attrs = qdbus_cast<QVariantMap>(r.arguments().constFirst().value<QDBusArgument>());
                }
                const DiskHealth health = DiskHealthParser::build(drive, attrs);
                if (health.available())
                    result.insert(drive.blockName, health);
            }
        }
        emit updated(result);
    }

private:
    QDBusConnection m_bus{QString()};
    QTimer *m_timer = nullptr;
};

DiskHealthMonitor::DiskHealthMonitor(QObject *parent) : QObject(parent), m_worker(new Worker) {
    qDBusRegisterMetaType<DiskHealthParser::InterfaceMap>();
    qDBusRegisterMetaType<DiskHealthParser::ObjectMap>();
    qRegisterMetaType<DiskHealth>();
    qRegisterMetaType<DiskHealthMap>();
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &Worker::updated, this, &DiskHealthMonitor::updated, Qt::QueuedConnection);
    m_thread.start();
}

DiskHealthMonitor::~DiskHealthMonitor() {
    QMetaObject::invokeMethod(m_worker, "stop", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void DiskHealthMonitor::setActive(bool active) {
    if (active == m_active)
        return;
    m_active = active;
    QMetaObject::invokeMethod(m_worker, "setActive", Qt::QueuedConnection, Q_ARG(bool, active));
}

#include "DiskHealthMonitor.moc"
