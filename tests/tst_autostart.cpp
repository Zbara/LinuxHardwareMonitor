#include "core/Autostart.h"

#include <QtTest>

class TstAutostart : public QObject {
    Q_OBJECT
private slots:
    void enableWritesDesktopEntryAndDisableRemovesIt() {
        QTemporaryDir dir;
        Autostart a(dir.path() + "/autostart", "/opt/hw monitor/app");
        QVERIFY(!a.isEnabled());

        QVERIFY(a.setEnabled(true)); // creates the missing directory too
        QVERIFY(a.isEnabled());
        QFile f(a.filePath());
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(f.readAll());
        QVERIFY(text.contains("[Desktop Entry]"));
        QVERIFY(text.contains("Exec=\"/opt/hw monitor/app\""));
        QVERIFY(text.contains("Type=Application"));

        QVERIFY(a.setEnabled(true)); // idempotent
        QVERIFY(a.setEnabled(false));
        QVERIFY(!a.isEnabled());
        QVERIFY(a.setEnabled(false)); // disabling twice is fine
    }

    void failsCleanlyWhenDirectoryIsNotWritable() {
        Autostart a("/proc/nonexistent/autostart", "/bin/true");
        QVERIFY(!a.setEnabled(true));
        QVERIFY(!a.isEnabled());
    }
};

QTEST_APPLESS_MAIN(TstAutostart)
#include "tst_autostart.moc"
