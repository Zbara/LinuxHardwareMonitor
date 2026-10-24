#pragma once

#include "ISensorProvider.h"

// Laptop batteries and the AC adapter from /sys/class/power_supply (no root needed).
// Missing attributes simply produce no sensor; nothing is guessed.
class BatteryProvider : public ISensorProvider {
public:
    explicit BatteryProvider(QString root = QStringLiteral("/sys/class/power_supply"));

    QString name() const override { return QStringLiteral("battery"); }
    QVector<SensorReading> poll() override;

private:
    QString m_root;
};
