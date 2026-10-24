#pragma once

#include "SensorTypes.h"

#include <QHash>
#include <QSet>
#include <QVector>

// Detects temperature sensors crossing a threshold. A sensor alerts once when
// it reaches the threshold and is re-armed after dropping `hysteresis` degrees
// below it, so a sensor hovering around the limit does not alert every poll.
class OverheatMonitor {
public:
    struct Alert {
        QString sensorId;
        QString deviceName;
        QString sensorName;
        double value = 0.0;
    };

    void setThreshold(double celsius) { m_threshold = celsius; }
    double threshold() const { return m_threshold; }

    // A threshold <= 0 disables alerts.
    QVector<Alert> update(const QVector<SensorReading> &readings);

private:
    static constexpr double kHysteresis = 5.0;
    double m_threshold = 90.0;
    QSet<QString> m_alerted;
};
