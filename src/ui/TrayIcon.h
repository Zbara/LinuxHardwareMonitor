#pragma once

#include <QColor>
#include <cmath>
#include <QImage>
#include <QObject>
#include <QString>


class QAction;
class QMenu;
class QSystemTrayIcon;

// Pure helpers for the tray icon (testable without a tray).
namespace TrayIcon {

// Square icon: rounded coloured background with `text` (short: "67", "100") centred on it.
QImage render(const QString &text, const QColor &background, int size = 64);

// The program's own icon (same artwork as packaging/linux-hardware-monitor.svg), drawn in code so
// it needs no SVG plugin at run time.
QImage renderAppIcon(int size = 64);

// Blue when cool, amber when warm, red when hot.
QColor colorForTemperature(double celsius);

// "CPU 12 % · 67 °C · RAM 38 %"; unknown values are left out, never shown as zero.
QString tooltip(const QString &appName, double cpuUsage, double cpuTemp, double memUsage);

} // namespace TrayIcon

// System tray icon showing the CPU temperature (or load when there is no sensor). Left click
// toggles the window; the menu has Show/Hide and Quit.
class TrayController : public QObject {
    Q_OBJECT
public:
    explicit TrayController(QObject *parent = nullptr);
    ~TrayController() override;

    // False in sessions without a system tray (some Wayland shells, offscreen).
    static bool isAvailable();

    // "app" (the program icon, default) or "temperature" (a coloured tile with the CPU temperature).
    void setStyle(const QString &style);
    void setVisible(bool visible);
    // Re-applies translated texts after a language change.
    void retranslate();
    // Redraws the icon and tooltip. NaN means "unknown".
    void update(double cpuUsage, double cpuTemp, double memUsage);

signals:
    void toggleWindowRequested();
    void quitRequested();

private:
    QSystemTrayIcon *m_tray;
    QMenu *m_menu;
    QAction *m_toggleAction;
    QAction *m_quitAction;
    QString m_style = QStringLiteral("app");
    double m_usage = NAN, m_temp = NAN, m_mem = NAN;
};
