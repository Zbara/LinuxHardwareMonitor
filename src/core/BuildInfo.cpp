#include "BuildInfo.h"

#include <QtGlobal>

// Set by CMake (see the git block next to HWMON_VERSION); absent in a build without git.
#ifndef HWMON_VERSION
#define HWMON_VERSION "unknown"
#endif
#ifndef HWMON_GIT_DESCRIBE
#define HWMON_GIT_DESCRIBE ""
#endif
#ifndef HWMON_GIT_DATE
#define HWMON_GIT_DATE ""
#endif

namespace BuildInfo {

QString version() {
    return QStringLiteral(HWMON_VERSION);
}

QString build() {
    const QString describe = QStringLiteral(HWMON_GIT_DESCRIBE);
    if (describe.isEmpty())
        return {};
    const QString date = QStringLiteral(HWMON_GIT_DATE);
    return date.isEmpty() ? describe : QStringLiteral("%1 (%2)").arg(describe, date);
}

QString qtRuntime() {
    return QString::fromLatin1(qVersion());
}

QString qtBuiltWith() {
    return QStringLiteral(QT_VERSION_STR);
}

} // namespace BuildInfo
