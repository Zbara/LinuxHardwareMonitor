#include "core/StartupManager.h"
#include "models/StartupModel.h"

#include <QtTest>

namespace {
void write(const QString &path, const QByteArray &content) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}
QByteArray desktop(const QByteArray &name, const QByteArray &extra = {}) {
    return "[Desktop Entry]\nType=Application\nName=" + name + "\nExec=/usr/bin/" + name + "\n" + extra;
}
} // namespace

class TstStartup : public QObject {
    Q_OBJECT
private slots:
    void parsesEnabledState() {
        QVERIFY(StartupManager::isEnabled(StartupManager::parseDesktop("[Desktop Entry]\nName=a\n")));
        QVERIFY(!StartupManager::isEnabled(StartupManager::parseDesktop("[Desktop Entry]\nHidden=true\n")));
        QVERIFY(!StartupManager::isEnabled(
            StartupManager::parseDesktop("[Desktop Entry]\nX-GNOME-Autostart-enabled=false\n")));
        // keys outside [Desktop Entry] (e.g. actions) must not leak in
        QVERIFY(StartupManager::isEnabled(
            StartupManager::parseDesktop("[Desktop Entry]\nName=a\n[Desktop Action x]\nHidden=true\n")));
    }

    void withKeyReplacesOrInsertsInsideTheMainGroup() {
        const QString text = "[Desktop Entry]\nName=a\nHidden=false\n\n[Desktop Action x]\nName=x\n";
        const QString replaced = StartupManager::withKey(text, "Hidden", "true");
        QVERIFY(replaced.contains("Hidden=true"));
        QVERIFY(!replaced.contains("Hidden=false"));

        const QString inserted = StartupManager::withKey(text, "Foo", "bar");
        QVERIFY(inserted.indexOf("Foo=bar") < inserted.indexOf("[Desktop Action x]"));
        QVERIFY(inserted.contains("Name=x")); // other groups untouched
    }

    void scanMergesUserOverSystemAndSorts() {
        QTemporaryDir dir;
        write(dir.path() + "/user/b.desktop", desktop("b-user"));
        write(dir.path() + "/sys/b.desktop", desktop("b-system"));
        write(dir.path() + "/sys/a.desktop", desktop("alpha", "Hidden=true\n"));
        StartupManager m(dir.path() + "/user", {dir.path() + "/sys"});
        const auto list = m.scan();
        QCOMPARE(list.size(), 2);
        QCOMPARE(list[0].name, QString("alpha"));
        QCOMPARE(list[0].source, QString("System"));
        QVERIFY(!list[0].enabled);
        QCOMPARE(list[1].name, QString("b-user")); // user entry wins
        QCOMPARE(list[1].source, QString("User"));
    }

    void disablingASystemEntryWritesAnOverrideAndLeavesTheOriginal() {
        QTemporaryDir dir;
        write(dir.path() + "/sys/a.desktop", desktop("alpha"));
        StartupManager m(dir.path() + "/user", {dir.path() + "/sys"});
        QVERIFY(m.setEnabled("a.desktop", false));

        QVERIFY(QFile::exists(dir.path() + "/user/a.desktop"));
        QFile orig(dir.path() + "/sys/a.desktop");
        QVERIFY(orig.open(QIODevice::ReadOnly));
        QVERIFY(!orig.readAll().contains("Hidden"));

        QVERIFY(!m.scan().first().enabled);
        QVERIFY(m.setEnabled("a.desktop", true));
        QVERIFY(m.scan().first().enabled);
    }

    void rejectsUnsafeOrUnknownIds() {
        QTemporaryDir dir;
        write(dir.path() + "/sys/a.desktop", desktop("alpha"));
        StartupManager m(dir.path() + "/user", {dir.path() + "/sys"});
        QVERIFY(!m.setEnabled("../sys/a.desktop", false));
        QVERIFY(!m.setEnabled("nope.desktop", false));
        QVERIFY(!m.setEnabled("a.txt", false));
        QVERIFY(!m.setEnabled("", false));
    }

    void modelRefreshesAfterToggle() {
        QTemporaryDir dir;
        write(dir.path() + "/user/a.desktop", desktop("alpha"));
        StartupModel model(StartupManager(dir.path() + "/user", {}));
        model.refresh();
        QCOMPARE(model.rowCount(), 1);
        QVERIFY(model.data(model.index(0), StartupModel::EnabledRole).toBool());
        QCOMPARE(model.enabledCount(), 1);
        QSignalSpy counts(&model, &StartupModel::countsChanged);
        QVERIFY(model.setEnabled("a.desktop", false));
        QVERIFY(!model.data(model.index(0), StartupModel::EnabledRole).toBool());
        QCOMPARE(model.enabledCount(), 0);
        QCOMPARE(model.rowCountInt(), 1);
        QVERIFY(counts.count() >= 1);
        QVERIFY(!model.setEnabled("missing.desktop", false));
    }
};

QTEST_GUILESS_MAIN(TstStartup)
#include "tst_startup.moc"
