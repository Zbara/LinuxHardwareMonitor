#include "Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace {
constexpr auto kFileName = "linux-hardware-monitor.desktop";
}

QString Autostart::defaultDir() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/autostart");
}

Autostart::Autostart(QString dir, QString execPath)
    : m_dir(std::move(dir)),
      m_exec(execPath.isEmpty() ? QCoreApplication::applicationFilePath() : std::move(execPath)) {}

QString Autostart::filePath() const {
    return m_dir + QLatin1Char('/') + QLatin1String(kFileName);
}

bool Autostart::isEnabled() const {
    return QFile::exists(filePath());
}

bool Autostart::setEnabled(bool enabled) {
    if (!enabled)
        return !QFile::exists(filePath()) || QFile::remove(filePath());

    if (!QDir().mkpath(m_dir))
        return false;
    QFile f(filePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;

    // Quote the path and escape what the Exec key treats specially.
    QString exec = m_exec;
    exec.replace(QLatin1Char('\\'), QStringLiteral("\\\\")).replace(QLatin1Char('"'), QStringLiteral("\\\""));
    const QString content = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Linux Hardware Monitor\n"
        "Comment=Hardware sensors and task manager\n"
        "Exec=\"%1\"\n"
        "Terminal=false\n"
        "X-GNOME-Autostart-enabled=true\n").arg(exec);
    return f.write(content.toUtf8()) >= 0 && f.error() == QFileDevice::NoError;
}
