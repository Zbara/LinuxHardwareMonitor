#include "core/ServiceManager.h"
#include "models/ServiceModel.h"

#include <QDBusMetaType>
#include <QtTest>

namespace {
SystemdUnit unit(const QString &name, const QString &load, const QString &active, const QString &sub) {
    SystemdUnit u;
    u.name = name;
    u.description = name + " description";
    u.loadState = load;
    u.activeState = active;
    u.subState = sub;
    return u;
}
ServiceInfo service(const QString &name, const QString &state) {
    ServiceInfo s;
    s.name = name;
    s.description = name + " desc";
    s.state = state;
    return s;
}
} // namespace

class TstServices : public QObject {
    Q_OBJECT
private slots:
    void mapsSystemdStatesToDisplayStates() {
        QCOMPARE(ServiceManager::displayState("active", "running"), QString("running"));
        QCOMPARE(ServiceManager::displayState("active", "exited"), QString("exited"));
        QCOMPARE(ServiceManager::displayState("inactive", "dead"), QString("stopped"));
        QCOMPARE(ServiceManager::displayState("failed", "failed"), QString("failed"));
        QCOMPARE(ServiceManager::displayState("activating", "start"), QString("starting"));
        QCOMPARE(ServiceManager::displayState("weird", ""), QString("other"));
    }

    void keepsOnlyRealServices() {
        const auto list = ServiceManager::toServices({
            unit("ssh.service", "loaded", "active", "running"),
            unit("sysinit.target", "loaded", "active", "active"),
            unit("ghost.service", "not-found", "inactive", "dead"),
            unit("getty@tty1.service", "loaded", "active", "running"),
        });
        QCOMPARE(list.size(), 2);
        QCOMPARE(list[0].name, QString("ssh")); // suffix removed
        QCOMPARE(list[1].name, QString("getty@tty1"));
        QCOMPARE(list[0].state, QString("running"));
    }

    void dbusStructMatchesSystemdsListUnitsSignature() {
        // A wrong field order or type would make every ListUnits() reply fail to parse.
        ServiceManager mgr; // registers the metatypes
        QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<SystemdUnit>()), QByteArray("(ssssssouso)"));
        QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<QList<SystemdUnit>>()), QByteArray("a(ssssssouso)"));
    }

    void modelUpdatesIncrementally() {
        ServiceListModel m;
        QSignalSpy resets(&m, &QAbstractItemModel::modelReset);
        QSignalSpy removed(&m, &QAbstractItemModel::rowsRemoved);
        QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);

        m.update({service("a", "running"), service("b", "stopped")});
        QCOMPARE(m.rowCount(), 2);
        m.update({service("a", "running"), service("c", "failed")});
        QCOMPARE(m.rowCount(), 2);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(changed.count(), 0); // unchanged rows are not reported
        m.update({service("a", "failed"), service("c", "failed")});
        QCOMPARE(changed.count(), 1);
        QCOMPARE(resets.count(), 0);
        QCOMPARE(m.countWithState("failed"), 2);
    }

    void proxyFiltersAndSorts() {
        ServiceListModel m;
        ServiceProxyModel proxy;
        proxy.setSourceModel(&m);
        m.update({service("zeta", "running"), service("alpha", "stopped"), service("beta", "failed")});
        auto names = [&] {
            QStringList l;
            for (int i = 0; i < proxy.rowCount(); ++i)
                l << proxy.data(proxy.index(i, 0), ServiceListModel::NameRole).toString();
            return l;
        };
        QCOMPARE(names(), QStringList({"alpha", "beta", "zeta"}));

        proxy.sortBy("status"); // failed first, then running, then stopped
        QCOMPARE(names(), QStringList({"beta", "zeta", "alpha"}));
        proxy.sortBy("status"); // reversed
        QCOMPARE(names(), QStringList({"alpha", "zeta", "beta"}));
        proxy.sortBy("name");
        proxy.sortBy("name");
        QCOMPARE(names(), QStringList({"zeta", "beta", "alpha"}));

        proxy.setFilterText("BET"); // case-insensitive, matches beta
        QCOMPARE(names(), QStringList({"beta"}));
        proxy.setFilterText("zeta desc"); // also searches the description
        QCOMPARE(names(), QStringList({"zeta"}));
    }

    void stateFilterCombinesWithTextFilter() {
        ServiceListModel m;
        ServiceProxyModel proxy;
        proxy.setSourceModel(&m);
        m.update({service("a", "running"), service("b", "stopped"), service("c", "exited"), service("d", "failed"),
                  service("ab", "running")});
        proxy.setStateFilter("running");
        QCOMPARE(proxy.rowCount(), 2);
        proxy.setStateFilter("stopped"); // one-shots that finished count as stopped
        QCOMPARE(proxy.rowCount(), 2);
        proxy.setStateFilter("failed");
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setStateFilter("running");
        proxy.setFilterText("ab");
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setStateFilter("");
        proxy.setFilterText("");
        QCOMPARE(proxy.rowCount(), 5);
    }

    void rejectsUnsafeActionRequests() {
        ServiceManager mgr;
        QSignalSpy spy(&mgr, &ServiceManager::actionFinished);
        mgr.requestAction("../evil", "start");
        mgr.requestAction("ssh", "reboot");
        QCOMPARE(spy.count(), 2);
        QVERIFY(!spy.at(0).at(1).toString().isEmpty());
        QVERIFY(!spy.at(1).at(1).toString().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstServices)
#include "tst_services.moc"
