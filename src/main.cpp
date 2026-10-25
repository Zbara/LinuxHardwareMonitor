#include "ui/AppController.h"

#include "core/BuildInfo.h"
#include "core/CommandLine.h"
#include "core/CpuTemperature.h"
#include "core/QuitSignals.h"
#include "core/SensorSnapshot.h"
#include "ui/ProviderFactory.h"
#include "ui/TrayIcon.h"

#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QThread>
#include <QFileInfo>
#include <QTextStream>
#include <QQuickWindow>
#include <QTimer>

#include <cmath>

namespace {

// `--dump-json [--dump-delay <ms>] [--compact]` plus the data-source flags. Rates (CPU load, disk and
// network speed) need two samples, so the sensors are polled twice and the second round is printed.
int runDump(int argc, char *argv[], const QStringList &args) {
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LinuxHardwareMonitor"));
    app.setOrganizationName(QStringLiteral("LinuxHardwareMonitor"));

    AppOptions options;
    QString error;
    if (!parseAppOptions(args, options, &error)) {
        QTextStream(stderr) << error << Qt::endl;
        return 2;
    }
    int delayMs = 500;
    if (const int i = args.indexOf(QStringLiteral("--dump-delay")); i >= 0 && i + 1 < args.size())
        delayMs = qBound(0, args.at(i + 1).toInt(), 60000);

    auto providers = createProviders(options);
    auto pollAll = [&providers] {
        QVector<SensorReading> all;
        for (auto &provider : providers) {
            try {
                all += provider->poll();
            } catch (...) {
                // One failing provider must not take the others down.
            }
        }
        return all;
    };
    pollAll();
    QThread::msleep(static_cast<unsigned long>(delayMs));
    QVector<SensorReading> readings = pollAll();
    CpuTemperature::addDerived(readings); // the same extra sensors the application lists

    const QString mode = options.mock ? QStringLiteral("simulation (%1)").arg(QFileInfo(options.mockPath).fileName())
                         : options.fixtureMode() ? QStringLiteral("test data")
                                                 : QStringLiteral("real");
    const QByteArray json = SensorSnapshot::toJson(readings, BuildInfo::version(), mode,
                                                   QDateTime::currentDateTime(), args.contains(QStringLiteral("--compact")));
    QFile out;
    if (!out.open(stdout, QIODevice::WriteOnly) || out.write(json) != json.size()) {
        QTextStream(stderr) << "cannot write to the standard output" << Qt::endl;
        return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char *argv[]) {
    // --help and --version answer before any Qt GUI object exists, so they work without a display.
    {
        QStringList args;
        for (int i = 1; i < argc; ++i)
            args << QString::fromLocal8Bit(argv[i]);
        const bool help = CommandLine::wantsHelp(args), version = CommandLine::wantsVersion(args);
        if (help || version) {
            const QString text = help ? CommandLine::help(QFileInfo(QString::fromLocal8Bit(argv[0])).fileName())
                                      : CommandLine::versionDetails();
            QTextStream(stdout) << text << Qt::endl;
            return 0;
        }
    }
    // --dump-json prints the sensors once and exits. It runs before any GUI object exists, so it also
    // works on a machine without a display.
    {
        QStringList args;
        for (int i = 1; i < argc; ++i)
            args << QString::fromLocal8Bit(argv[i]);
        if (args.contains(QStringLiteral("--dump-json")))
            return runDump(argc, argv, args);
    }
    // The interface is flat 2D; the software scene graph needs ~150 MB less memory than the OpenGL one
    // and does not wake the discrete GPU. QT_QUICK_BACKEND=rhi (or any other value) overrides it.
    QString rendererName = qEnvironmentVariable("QT_QUICK_BACKEND", qEnvironmentVariable("QSG_RHI_BACKEND", QStringLiteral("default")));
    if (!qEnvironmentVariableIsSet("QT_QUICK_BACKEND") && !qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
        rendererName = QStringLiteral("software");
    }
    QApplication app(argc, argv); // QSystemTrayIcon needs the widgets application
    app.setApplicationName(QStringLiteral("LinuxHardwareMonitor"));
    app.setOrganizationName(QStringLiteral("LinuxHardwareMonitor"));
    // Lets Wayland shells match the window to packaging/linux-hardware-monitor.desktop (name and icon).
    app.setDesktopFileName(QStringLiteral("linux-hardware-monitor"));
    app.setWindowIcon(QIcon(QPixmap::fromImage(TrayIcon::renderAppIcon(256))));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // ---- hardware data source: real by default, simulation/fixtures only when asked for
    AppOptions options;
    {
        QString error;
        if (!parseAppOptions(app.arguments(), options, &error)) {
            qCritical("%s", qPrintable(error));
            return 2;
        }
    }

    AppController controller(options);
    controller.setRendererName(rendererName);

    QQmlApplicationEngine engine;
    // Re-evaluate every qsTr() binding when the language changes.
    QObject::connect(&controller, &AppController::languageChanged, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
    // A window that could not be created ends the program with an error. objectCreated with a null object is the
    // form that exists on every Qt 6 (objectCreationFailed is Qt 6.4+).
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [](QObject *object, const QUrl &) { if (!object) QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    engine.loadFromModule("HwMonitor", "Main");
#else
    engine.addImportPath(QStringLiteral("qrc:/qt/qml")); // Qt 6.5 does this itself
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/HwMonitor/qml/Main.qml"))); // loadFromModule needs Qt 6.5
#endif

    // ---- system tray: shows the CPU temperature, optional "keep running when the window is closed"
    TrayController tray;
    controller.setTrayAvailable(TrayController::isAvailable());
    auto syncTray = [&] {
        tray.setVisible(controller.trayEnabled());
        // With a tray the app lives on after the window is closed; without one, closing quits as usual.
        QGuiApplication::setQuitOnLastWindowClosed(!(controller.trayEnabled() && controller.trayAvailable()));
    };
    QObject::connect(&controller, &AppController::trayEnabledChanged, &app, syncTray);
    syncTray();
    tray.setStyle(controller.trayStyle());
    QObject::connect(&controller, &AppController::trayStyleChanged, &tray, [&] { tray.setStyle(controller.trayStyle()); });
    tray.setFahrenheit(controller.useFahrenheit());
    QObject::connect(&controller, &AppController::useFahrenheitChanged, &tray, [&] { tray.setFahrenheit(controller.useFahrenheit()); });
    QObject::connect(&controller, &AppController::languageChanged, &tray, &TrayController::retranslate);
    QObject::connect(&controller, &AppController::highlightsUpdated, &tray, [&] {
        const QVariantList list = controller.highlights();
        // app.highlights order: 0 CPU temperature, 4 CPU load, 6 memory load
        auto value = [&](int i) {
            const QVariantMap m = i < list.size() ? list[i].toMap() : QVariantMap();
            return m.value(QStringLiteral("available")).toBool() ? m.value(QStringLiteral("value")).toDouble() : double(NAN);
        };
        tray.update(value(4), value(0), value(6));
    });
    QObject::connect(&tray, &TrayController::quitRequested, &app, &QCoreApplication::quit);
    QObject::connect(&tray, &TrayController::toggleWindowRequested, &app, [&engine] {
        const auto roots = engine.rootObjects();
        auto *window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
        if (!window)
            return;
        if (window->isVisible() && window->isActive()) {
            window->hide();
        } else {
            window->show();
            window->raise();
            window->requestActivate();
        }
    });

    // `--page <n>` selects the initial page (for screenshots).
    if (const int i = app.arguments().indexOf(QStringLiteral("--page")); i >= 0 && i + 1 < app.arguments().size()) {
        const auto roots = engine.rootObjects();
        if (!roots.isEmpty())
            roots.first()->setProperty("pageIndex", app.arguments().at(i + 1).toInt());
    }

    // `--resource <n>` selects the Performance resource (for screenshots).
    if (const int i = app.arguments().indexOf(QStringLiteral("--resource")); i >= 0 && i + 1 < app.arguments().size()) {
        const auto roots = engine.rootObjects();
        if (!roots.isEmpty())
            roots.first()->setProperty("resourceIndex", app.arguments().at(i + 1).toInt());
    }

    // `--cores` shows the per-core CPU charts (for screenshots).
    if (app.arguments().contains(QStringLiteral("--cores"))) {
        const auto roots = engine.rootObjects();
        if (!roots.isEmpty())
            roots.first()->setProperty("coreView", true);
    }

    // `--size <w> <h>` resizes the window (for screenshots).
    if (const int i = app.arguments().indexOf(QStringLiteral("--size")); i >= 0 && i + 2 < app.arguments().size()) {
        const auto roots = engine.rootObjects();
        if (!roots.isEmpty()) {
            roots.first()->setProperty("width", app.arguments().at(i + 1).toInt());
            roots.first()->setProperty("height", app.arguments().at(i + 2).toInt());
        }
    }

    // `--close-after <ms>`: close the main window after a delay (exercises the real close path).
    if (const int i = app.arguments().indexOf(QStringLiteral("--close-after")); i >= 0 && i + 1 < app.arguments().size()) {
        QTimer::singleShot(app.arguments().at(i + 1).toInt(), &app, [&engine] {
            const auto roots = engine.rootObjects();
            if (auto *window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first()))
                window->close();
        });
    }

    // `--load-modules`: load the RAM module details at startup (simulation mode answers instantly).
    if (app.arguments().contains(QStringLiteral("--load-modules")))
        controller.loadMemoryModules();

    // `--run-scenario <n>` triggers simulation action n; `--scenario-panel` opens the panel (screenshots).
    if (const int i = app.arguments().indexOf(QStringLiteral("--run-scenario")); i >= 0 && i + 1 < app.arguments().size())
        controller.runSimulationAction(app.arguments().at(i + 1).toInt());
    if (app.arguments().contains(QStringLiteral("--scenario-panel"))) {
        const auto roots = engine.rootObjects();
        if (!roots.isEmpty())
            roots.first()->setProperty("scenarioPanel", true);
    }

    // `--screenshot <file.png>`: save the first window after a few polls and exit.
    const QStringList args = app.arguments();
    if (const int i = args.indexOf(QStringLiteral("--screenshot")); i >= 0 && i + 1 < args.size()) {
        const QString path = args.at(i + 1);
        // `--screenshot-delay <ms>` waits longer (default 3.5 s) so the charts have history.
        int delayMs = 3500;
        if (const int d = args.indexOf(QStringLiteral("--screenshot-delay")); d >= 0 && d + 1 < args.size())
            delayMs = qMax(0, args.at(d + 1).toInt());
        QTimer::singleShot(delayMs, &app, [&engine, path] {
            const auto windows = engine.rootObjects();
            auto *window = windows.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(windows.first());
            QCoreApplication::exit(window && window->grabWindow().save(path) ? 0 : 1);
        });
    }

    // Ctrl+C, `kill` and the end of the session quit normally (history saved, threads joined).
    QuitSignals quitSignals;
    QObject::connect(&quitSignals, &QuitSignals::quitRequested, &app, [](int) { QCoreApplication::quit(); });
    return app.exec();
}
