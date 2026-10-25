#pragma once

#include <QString>
#include <Qt>

// The colour theme setting: "system" follows the desktop's dark/light preference, "dark" and "light" are
// fixed. The dark theme is the default (the spec asks for it) when the desktop does not say.
namespace ThemeMode {

QString normalize(const QString &mode);                 // anything unknown -> "dark"
QString fromLegacy(bool darkTheme);                     // the old boolean setting -> "dark" / "light"
bool isDark(const QString &mode, Qt::ColorScheme system);

} // namespace ThemeMode
