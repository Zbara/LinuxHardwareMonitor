#include "ServiceManager.h"

#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace {
constexpr auto kService = "org.freedesktop.systemd1";
constexpr auto kPath = "/org/freedesktop/systemd1";
constexpr auto kManager = "org.freedesktop.systemd1.Manager";
constexpr auto kSuffix = ".service";
} // namespace

QDBusArgument &operator<<(QDBusArgument &arg, const SystemdUnit &u) {
    arg.beginStructure();
    arg << u.name << u.description << u.loadState << u.activeState << u.subState << u.following << u.path
        << u.jobId << u.jobType << u.jobPath;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, SystemdUnit &u) {
    arg.beginStructure();
    arg >> u.name >> u.description >> u.loadState >> u.activeState >> u.subState >> u.following >> u.path
        >> u.jobId >> u.jobType >> u.jobPath;
    arg.endStructure();
    return arg;
}

ServiceManager::ServiceManager(QObject *parent) : QObject(parent), m_bus(QDBusConnection::systemBus()) {
    qDBusRegisterMetaType<SystemdUnit>();
    qDBusRegisterMetaType<QList<SystemdUnit>>();
    qRegisterMetaType<QVector<ServiceInfo>>();
    m_timer.setInterval(3000);
    connect(&m_timer, &QTimer::timeout, this, &ServiceManager::refresh);
}

void ServiceManager::setActive(bool active) {
    if (active == m_timer.isActive())
        return;
    if (active) {
        m_timer.start();
        refresh();
    } else {
        m_timer.stop();
    }
}

QString ServiceManager::displayState(const QString &active, const QString &sub) {
    if (active == QLatin1String("failed"))
        return QStringLiteral("failed");
    if (active == QLatin1String("activating") || active == QLatin1String("reloading"))
        return QStringLiteral("starting");
    if (active == QLatin1String("active"))
        return sub == QLatin1String("running") ? QStringLiteral("running") : QStringLiteral("exited");
    if (active == QLatin1String("inactive") || active == QLatin1String("deactivating"))
        return QStringLiteral("stopped");
    return QStringLiteral("other");
}

QVector<ServiceInfo> ServiceManager::toServices(const QList<SystemdUnit> &units) {
    QVector<ServiceInfo> out;
    for (const SystemdUnit &u : units) {
        if (!u.name.endsWith(QLatin1String(kSuffix)))
            continue;
        // Units that were only referenced by others, never installed.
        if (u.loadState == QLatin1String("not-found"))
            continue;
        // Template instances like "getty@tty1" are real, but "user@.service" templates are not listed anyway.
        ServiceInfo s;
        s.name = u.name.chopped(static_cast<qsizetype>(qstrlen(kSuffix)));
        s.description = u.description;
        s.loadState = u.loadState;
        s.activeState = u.activeState;
        s.subState = u.subState;
        s.state = displayState(u.activeState, u.subState);
        out.push_back(s);
    }
    return out;
}

void ServiceManager::refresh() {
    if (m_refreshing)
        return; // never overlap requests
    m_refreshing = true;

    QDBusInterface iface(QLatin1String(kService), QLatin1String(kPath), QLatin1String(kManager), m_bus);
    auto *watcher = new QDBusPendingCallWatcher(iface.asyncCall(QStringLiteral("ListUnits")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        m_refreshing = false;
        QDBusPendingReply<QList<SystemdUnit>> reply = *w;
        if (reply.isError())
            emit failed(reply.error().message());
        else
            emit servicesReady(toServices(reply.value()));
        w->deleteLater();
    });
}

void ServiceManager::requestAction(const QString &serviceName, const QString &action) {
    const QHash<QString, QString> methods = {
        {QStringLiteral("start"), QStringLiteral("StartUnit")},
        {QStringLiteral("stop"), QStringLiteral("StopUnit")},
        {QStringLiteral("restart"), QStringLiteral("RestartUnit")},
    };
    const QString method = methods.value(action);
    // Only plain service names; never let the UI address other unit types.
    if (method.isEmpty() || serviceName.isEmpty() || serviceName.contains(QLatin1Char('/'))) {
        emit actionFinished(serviceName, QStringLiteral("invalid request"));
        return;
    }

    QDBusInterface iface(QLatin1String(kService), QLatin1String(kPath), QLatin1String(kManager), m_bus);
    auto *watcher = new QDBusPendingCallWatcher(
        iface.asyncCall(method, serviceName + QLatin1String(kSuffix), QStringLiteral("replace")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, serviceName](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        emit actionFinished(serviceName, reply.isError() ? reply.error().message() : QString());
        w->deleteLater();
        refresh();
    });
}
