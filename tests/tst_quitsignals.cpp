#include "core/QuitSignals.h"

#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <csignal>

class TstQuitSignals : public QObject {
    Q_OBJECT
private slots:
    void signalsBecomeQtSignals() {
        for (int sig : {SIGTERM, SIGINT, SIGHUP}) {
            QuitSignals q; // a fresh object per signal: a second signal on the same one escalates (below)
            QSignalSpy spy(&q, &QuitSignals::quitRequested);
            ::raise(sig);
            QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
            QCOMPARE(spy.takeFirst().at(0).toInt(), sig);
            QCOMPARE(q.received(), sig);
        }
    }

    // A second signal while the first is still being handled means the orderly shutdown is stuck:
    // the default action must end the process. Checked in a child that never runs its event loop.
    void aSecondSignalEndsAStuckProcess() {
        QProcess child;
        child.start(APP_BINARY, {"--page", "0"});
        QVERIFY(child.waitForStarted(5000));
        QTest::qWait(1500);
        ::kill(static_cast<pid_t>(child.processId()), SIGSTOP); // the "stuck" state: nothing is processed
        ::kill(static_cast<pid_t>(child.processId()), SIGTERM);
        ::kill(static_cast<pid_t>(child.processId()), SIGTERM);
        ::kill(static_cast<pid_t>(child.processId()), SIGCONT);
        QVERIFY2(child.waitForFinished(5000), "the process survived two SIGTERMs");
    }

    // The real program: a SIGTERM must end it normally (exit code 0) and save the history first.
    void theAppQuitsCleanlyAndSavesItsHistory() {
        QTemporaryDir data, config;
        QVERIFY(data.isValid() && config.isValid());
        QProcess app;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("QT_QPA_PLATFORM", "offscreen");
        env.insert("XDG_DATA_HOME", data.path());
        env.insert("XDG_CONFIG_HOME", config.path());
        app.setProcessEnvironment(env);
        app.start(APP_BINARY, {"--page", "0"});
        QVERIFY2(app.waitForStarted(5000), qPrintable(app.errorString()));
        QTest::qWait(3000); // a few polls, so there is something to save
        QCOMPARE(app.state(), QProcess::Running);
        app.terminate(); // SIGTERM
        QVERIFY2(app.waitForFinished(8000), "the app did not exit after SIGTERM");
        QCOMPARE(app.exitStatus(), QProcess::NormalExit);
        QCOMPARE(app.exitCode(), 0);
        QVERIFY2(QFileInfo::exists(data.path() + "/LinuxHardwareMonitor/LinuxHardwareMonitor/history.bin"),
                 "history.bin was not written on SIGTERM");
    }
};

QTEST_GUILESS_MAIN(TstQuitSignals)
#include "tst_quitsignals.moc"
