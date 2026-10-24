#pragma once

#include "ISensorProvider.h"

#include <optional>

// Reads /sys/class/hwmon (or any tree with the same layout). The directory is
// rescanned on every poll, so hot-plugged devices appear and removed ones
// disappear without special handling.
class HwmonProvider : public ISensorProvider {
public:
    explicit HwmonProvider(QString rootPath = QStringLiteral("/sys/class/hwmon"));

    QString name() const override { return QStringLiteral("hwmon"); }
    QVector<SensorReading> poll() override;

    // Parses a raw sysfs integer and multiplies by scale. Returns nullopt for
    // empty or malformed content.
    static std::optional<double> parseRaw(const QByteArray &raw, double scale);

    static DeviceCategory categoryForDriver(const QString &driverName);

    // Human-readable device title from the hwmon driver name and optional model string.
    static QString displayName(const QString &driverName, const QString &model);

private:
    QString m_root;
};
