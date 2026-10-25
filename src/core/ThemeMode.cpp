#include "ThemeMode.h"

namespace ThemeMode {

QString normalize(const QString &mode) {
    if (mode == QLatin1String("system") || mode == QLatin1String("light"))
        return mode;
    return QStringLiteral("dark");
}

QString fromLegacy(bool darkTheme) {
    return darkTheme ? QStringLiteral("dark") : QStringLiteral("light");
}

bool isDark(const QString &mode, Qt::ColorScheme system) {
    const QString m = normalize(mode);
    if (m == QLatin1String("light"))
        return false;
    if (m == QLatin1String("system"))
        return system != Qt::ColorScheme::Light; // a desktop that does not say (Unknown) gets the default
    return true;
}

} // namespace ThemeMode
