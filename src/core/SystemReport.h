#pragma once

#include <QString>
#include <QVariantMap>

// A plain-text description of this computer and of the program, meant to be pasted into a bug report.
// It contains hardware models and versions only: no serial numbers, MAC or IP addresses, Wi-Fi names,
// user or host names.
namespace SystemReport {

struct Environment {
    QString appVersion;
    QString build;    // commit the binary was built from, "" when unknown
    QString os;       // pretty product name
    QString kernel;
    QString qtVersion;
    QString renderer; // "software", "opengl", ...
    QString language;
    QString mode;     // "" for the real machine, else a note such as "simulation (profile.json)"
};

// `details` is AppController::details().
QString format(const QVariantMap &details, const Environment &env);

} // namespace SystemReport
