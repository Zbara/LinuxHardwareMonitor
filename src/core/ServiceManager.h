#pragma once

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QTimer>
#include <QVector>

// One row of systemd's Manager.ListUnits() reply: a(ssssssouso).
struct SystemdUnit {
    QString name, description, loadState, activeState, subState, following;
    QDBusObjectPath path;
    uint jobId = 0;
    QString jobType;
    QDBusObjectPath jobPath;
};
Q_DECLARE_METATYPE(SystemdUnit)
Q_DECLARE_METATYPE(QList<SystemdUnit>)

QDBusArgument &operator<<(QDBusArgument &arg, const SystemdUnit &unit);
const QDBusArgument &operator>>(const QDBusArgument &arg, SystemdUnit &unit);

// What the UI needs to know about one service.
struct ServiceInfo {
    QString name;        // without the ".service" suffix
    QString description;
    QString loadState;   // loaded, not-found, masked, ...
    QString activeState; // active, inactive, failed, activating, ...
    QString subState;    // running, exited, dead, failed, ...
    // Simplified state for display: running, exited, stopped, failed, starting, other.
    QString state;
};
Q_DECLARE_METATYPE(QVector<ServiceInfo>)

// Talks to systemd over the system D-Bus (no shell commands, no root for listing).
// Starting/stopping goes through polkit, which may show an authentication dialog.
class ServiceManager : public QObject {
    Q_OBJECT
public:
    explicit ServiceManager(QObject *parent = nullptr);

    bool active() const { return m_timer.isActive(); }
    void setActive(bool active); // polls every few seconds while true

    Q_INVOKABLE void refresh();
    // action: "start", "stop" or "restart"
    void requestAction(const QString &serviceName, const QString &action);

    // Maps systemd's (activeState, subState) to the simplified display state.
    static QString displayState(const QString &activeState, const QString &subState);
    // Keeps only *.service units that are not hidden/not-found placeholders.
    static QVector<ServiceInfo> toServices(const QList<SystemdUnit> &units);

signals:
    void servicesReady(const QVector<ServiceInfo> &services);
    void failed(const QString &message);      // listing failed (e.g. no systemd)
    void actionFinished(const QString &serviceName, const QString &error); // empty error = success

private:
    QDBusConnection m_bus;
    QTimer m_timer;
    bool m_refreshing = false;
};
