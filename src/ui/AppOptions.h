#pragma once

#include "mock/MockProfile.h"

#include <QString>
#include <QStringList>

#include <optional>

// How the app gets its hardware data. By default everything is read from the real machine;
// the other modes are opt-in (command line) and always announced in the UI.
struct AppOptions {
    // Simulation mode: replace all hardware providers with a generated profile.
    std::optional<MockProfile> mock;
    QString mockPath;
    double mockSpeed = 1.0; // simulated clock speed (scenarios happen faster)
    // Fixture mode: read the real parsers from prepared directory trees instead of /sys and /proc.
    QString hwmonRoot;
    QString procRoot;
    QString cpuRoot;
    QString powerSupplyRoot;
    QString drmRoot;

    bool fixtureMode() const {
        return !hwmonRoot.isEmpty() || !procRoot.isEmpty() || !cpuRoot.isEmpty() || !powerSupplyRoot.isEmpty() || !drmRoot.isEmpty();
    }
};

// Reads the data-source flags (--mock, --mock-speed, --hwmon-root, --proc-root, --cpu-root,
// --power-supply-root, --drm-root) from `args` (without the program name). Returns false and sets
// *error when they are unusable, e.g. a bad profile or a mix of simulation and test directories; the
// program must then exit instead of falling back to real data.
bool parseAppOptions(const QStringList &args, AppOptions &options, QString *error);
