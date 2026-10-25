#include "AppOptions.h"

#include <algorithm>

bool parseAppOptions(const QStringList &args, AppOptions &options, QString *error) {
    auto value = [&](const QString &flag) {
        const int i = args.indexOf(flag);
        return i >= 0 && i + 1 < args.size() ? args.at(i + 1) : QString();
    };
    options.mockPath = value(QStringLiteral("--mock"));
    if (const QString speed = value(QStringLiteral("--mock-speed")); !speed.isEmpty())
        options.mockSpeed = std::max(0.1, speed.toDouble());
    options.hwmonRoot = value(QStringLiteral("--hwmon-root"));
    options.procRoot = value(QStringLiteral("--proc-root"));
    options.cpuRoot = value(QStringLiteral("--cpu-root"));
    options.powerSupplyRoot = value(QStringLiteral("--power-supply-root"));
    options.drmRoot = value(QStringLiteral("--drm-root"));

    if (options.mockPath.isEmpty())
        return true;
    if (options.fixtureMode()) {
        if (error)
            *error = QStringLiteral("--mock cannot be combined with --hwmon-root / --proc-root / --cpu-root / --power-supply-root / --drm-root");
        return false;
    }
    QString loadError;
    options.mock = MockProfile::load(options.mockPath, &loadError);
    if (!options.mock) {
        if (error)
            *error = QStringLiteral("Cannot use mock profile %1: %2").arg(options.mockPath, loadError);
        return false; // never silently fall back to real data when simulation was requested
    }
    return true;
}
