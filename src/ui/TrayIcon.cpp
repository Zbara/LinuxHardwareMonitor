#include "TrayIcon.h"

#include <QAction>
#include <QFont>
#include <QIcon>
#include <QMenu>
#include <QLinearGradient>
#include <QPainter>
#include <QPolygonF>
#include <QPainterPath>
#include <QSystemTrayIcon>

#include <cmath>

namespace TrayIcon {

QColor colorForTemperature(double celsius) {
    if (celsius >= 85.0)
        return QColor(QStringLiteral("#e5534b"));
    if (celsius >= 70.0)
        return QColor(QStringLiteral("#d98a1f"));
    return QColor(QStringLiteral("#3d7fe0"));
}

QImage renderAppIcon(int size) {
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 256.0, size / 256.0); // draw in the SVG's 256x256 coordinate system

    // Tile
    QLinearGradient bg(0, 12, 0, 244);
    bg.setColorAt(0, QColor(QStringLiteral("#232a35")));
    bg.setColorAt(1, QColor(QStringLiteral("#14171c")));
    p.setBrush(bg);
    p.setPen(QPen(QColor(QStringLiteral("#2f353e")), 4));
    p.drawRoundedRect(QRectF(12, 12, 232, 232), 48, 48);

    // Grid
    p.setPen(QPen(QColor(QStringLiteral("#2f3a4a")), 2));
    for (int y : {92, 132, 172})
        p.drawLine(QPointF(44, y), QPointF(212, y));

    // Load curve with the faded fill below it
    const QPolygonF curve({{44, 176}, {76, 156}, {96, 166}, {124, 104}, {148, 148}, {168, 128}, {188, 140}, {212, 112}});
    QPolygonF area = curve;
    area << QPointF(212, 196) << QPointF(44, 196);
    QLinearGradient fill(0, 104, 0, 196);
    fill.setColorAt(0, QColor(76, 154, 255, 115));
    fill.setColorAt(1, QColor(76, 154, 255, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawPolygon(area);
    QPen line(QColor(QStringLiteral("#4c9aff")), 8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(line);
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(curve);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral("#4c9aff")));
    p.drawEllipse(QPointF(212, 112), 9, 9);
    return image;
}

QImage render(const QString &text, const QColor &background, int size) {
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    QPainterPath path;
    path.addRoundedRect(QRectF(1, 1, size - 2, size - 2), size * 0.22, size * 0.22);
    p.fillPath(path, background);

    // As large as fits: 1-2 characters fill the icon, 3 ("100") need a smaller font.
    QFont font;
    font.setBold(true);
    font.setPixelSize(static_cast<int>(size * (text.size() >= 3 ? 0.52 : 0.68)));
    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, text);
    return image;
}

double toFahrenheit(double celsius) {
    return celsius * 9.0 / 5.0 + 32.0;
}

QString tooltip(const QString &appName, double cpuUsage, double cpuTemp, double memUsage, bool fahrenheit) {
    QStringList parts;
    if (std::isfinite(cpuUsage))
        parts << QStringLiteral("CPU %1 %").arg(qRound(cpuUsage));
    if (std::isfinite(cpuTemp))
        parts << (fahrenheit ? QStringLiteral("%1 °F").arg(qRound(toFahrenheit(cpuTemp)))
                             : QStringLiteral("%1 °C").arg(qRound(cpuTemp)));
    if (std::isfinite(memUsage))
        parts << QStringLiteral("RAM %1 %").arg(qRound(memUsage));
    return parts.isEmpty() ? appName : appName + QLatin1Char('\n') + parts.join(QStringLiteral(" · "));
}

} // namespace TrayIcon

TrayController::TrayController(QObject *parent)
    : QObject(parent), m_tray(new QSystemTrayIcon(this)), m_menu(new QMenu) {
    m_toggleAction = m_menu->addAction(QString());
    connect(m_toggleAction, &QAction::triggered, this, &TrayController::toggleWindowRequested);
    m_menu->addSeparator();
    m_quitAction = m_menu->addAction(QString());
    connect(m_quitAction, &QAction::triggered, this, &TrayController::quitRequested);
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            emit toggleWindowRequested();
    });
    retranslate();
    update(NAN, NAN, NAN);
}

void TrayController::retranslate() {
    m_toggleAction->setText(tr("Show / hide window"));
    m_quitAction->setText(tr("Quit"));
    update(m_usage, m_temp, m_mem); // the tooltip carries the translated app name
}

TrayController::~TrayController() {
    m_tray->hide();
    delete m_menu;
}

bool TrayController::isAvailable() {
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayController::setStyle(const QString &style) {
    m_style = style == QLatin1String("temperature") ? style : QStringLiteral("app");
    update(m_usage, m_temp, m_mem);
}

void TrayController::setFahrenheit(bool on) {
    if (on == m_fahrenheit)
        return;
    m_fahrenheit = on;
    update(m_usage, m_temp, m_mem);
}

void TrayController::setVisible(bool visible) {
    m_tray->setVisible(visible && isAvailable());
}

void TrayController::update(double cpuUsage, double cpuTemp, double memUsage) {
    m_usage = cpuUsage;
    m_temp = cpuTemp;
    m_mem = memUsage;

    if (m_style == QLatin1String("app")) {
        // The program icon; the live numbers are still available in the tooltip.
        m_tray->setIcon(QIcon(QPixmap::fromImage(TrayIcon::renderAppIcon(64))));
        m_tray->setToolTip(TrayIcon::tooltip(tr("Hardware Monitor"), cpuUsage, cpuTemp, memUsage, m_fahrenheit));
        return;
    }

    QString text;
    QColor color;
    if (std::isfinite(cpuTemp)) {
        text = QString::number(qRound(m_fahrenheit ? TrayIcon::toFahrenheit(cpuTemp) : cpuTemp));
        color = TrayIcon::colorForTemperature(cpuTemp);
    } else if (std::isfinite(cpuUsage)) {
        text = QString::number(qRound(cpuUsage)); // no temperature sensor: show the load instead
        color = QColor(QStringLiteral("#17a2d8"));
    } else {
        text = QStringLiteral("–");
        color = QColor(QStringLiteral("#5f6a78"));
    }
    m_tray->setIcon(QIcon(QPixmap::fromImage(TrayIcon::render(text, color))));
    m_tray->setToolTip(TrayIcon::tooltip(tr("Hardware Monitor"), cpuUsage, cpuTemp, memUsage, m_fahrenheit));
}
