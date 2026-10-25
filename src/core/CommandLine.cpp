#include "CommandLine.h"

namespace CommandLine {

#ifndef HWMON_VERSION
#define HWMON_VERSION "unknown"
#endif

const QList<Flag> &flags() {
    static const QList<Flag> list = {
        {"--mock", "<profile.json>", "simulate other hardware from a profile (see mocks/README.md)"},
        {"--mock-speed", "<factor>", "run the simulated clock faster, e.g. 10"},
        {"--run-scenario", "<n>", "trigger simulation action number n (0 is the first) right away"},
        {"--scenario-panel", "", "open the simulation scenario panel at start"},
        {"--hwmon-root", "<dir>", "read hwmon data from a prepared directory tree instead of /sys/class/hwmon"},
        {"--proc-root", "<dir>", "read /proc data from a prepared directory tree"},
        {"--cpu-root", "<dir>", "read CPU topology from a prepared tree instead of /sys/devices/system/cpu"},
        {"--power-supply-root", "<dir>", "read batteries from a prepared tree instead of /sys/class/power_supply"},
        {"--drm-root", "<dir>", "read graphics cards from a prepared tree instead of /sys/class/drm"},
        {"--dump-json", "", "print every sensor once as JSON and exit (no window, works without a display)"},
        {"--dump-delay", "<ms>", "with --dump-json: wait between the two samples that rates need (default 500)"},
        {"--compact", "", "with --dump-json: one line instead of indented output"},
        {"--page", "<0-6>", "open this page first (Performance, Processes, Services, Startup, Sensors, Overview, Settings)"},
        {"--resource", "<n>", "select this resource on the Performance page"},
        {"--cores", "", "show the logical processors view of the CPU"},
        {"--load-modules", "", "load the RAM module details at start (on real hardware this asks for administrator rights)"},
        {"--size", "<w> <h>", "window size in pixels"},
        {"--screenshot", "<file.png>", "save a screenshot of the window and exit"},
        {"--screenshot-delay", "<ms>", "wait this long before the screenshot (default 3500) so the charts have history"},
        {"--close-after", "<ms>", "close the window after this many milliseconds"},
        {"--help, -h", "", "show this text"},
        {"--version, -v", "", "show the version"},
    };
    return list;
}

QString version() {
    return QStringLiteral("LinuxHardwareMonitor ") + QStringLiteral(HWMON_VERSION);
}

QString help(const QString &program) {
    QString out = QStringLiteral("Usage: %1 [options]\n\nSensors, performance graphs and a task manager for Linux.\n"
                                 "Without options the real hardware of this computer is shown.\n\nOptions:\n")
                      .arg(program);
    int width = 0;
    QStringList left;
    for (const Flag &f : flags()) {
        const QString text = QString::fromLatin1(f.name)
                             + (*f.argument ? QLatin1Char(' ') + QString::fromLatin1(f.argument) : QString());
        left << text;
        width = qMax(width, int(text.size()));
    }
    for (int i = 0; i < left.size(); ++i)
        out += QStringLiteral("  %1  %2\n").arg(left[i], -width).arg(QString::fromLatin1(flags()[i].description));
    return out;
}

bool wantsHelp(const QStringList &args) {
    return args.contains(QStringLiteral("--help")) || args.contains(QStringLiteral("-h"));
}

bool wantsVersion(const QStringList &args) {
    return args.contains(QStringLiteral("--version")) || args.contains(QStringLiteral("-v"));
}

} // namespace CommandLine
