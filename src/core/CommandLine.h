#pragma once

#include <QString>
#include <QStringList>

// Text for --help and --version. The flag table is the single list of what main.cpp understands.
namespace CommandLine {

struct Flag {
    const char *name;
    const char *argument; // "" when the flag takes none
    const char *description;
};

const QList<Flag> &flags();

QString version();         // the first line of `--version`: "LinuxHardwareMonitor 0.1.0" (scripts rely on it)
QString versionDetails();  // all of `--version`: that line plus the build, Qt and license
QString help(const QString &program);

// True when `args` (argv without the program name) asks for help or the version.
bool wantsHelp(const QStringList &args);
bool wantsVersion(const QStringList &args);

} // namespace CommandLine
