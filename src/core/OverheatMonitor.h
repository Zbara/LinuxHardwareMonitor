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

    // The alerts of one cycle collapsed to one entry per device: its hottest sensor and how many others
    // crossed the limit with it. (A processor has dozens of core sensors that rise together.)
    struct Group {
        QString deviceName;
        QString sensorName;
        double value = 0.0;
        int others = 0;
    };
    static QVector<Group> groupByDevice(const QVector<Alert> &alerts);

    void setThreshold(double celsius) { m_threshold = celsius; }
    double threshold() const { return m_threshold; }

    // A threshold of its own for one sensor (Celsius); it replaces the global one for that sensor.
    // A value <= 0 removes the override. Overrides work even when the global threshold is 0.
    void setSensorThreshold(const QString &sensorId, double celsius);
    double sensorThreshold(const QString &sensorId) const { return m_perSensor.value(sensorId, 0.0); }
    void setSensorThresholds(const QHash<QString, double> &all) { m_perSensor = all; m_alerted.clear(); }

    // A threshold <= 0 disables alerts (for the sensors without an override).
    QVector<Alert> update(const QVector<SensorReading> &readings);

private:
    static constexpr double kHysteresis = 5.0;
    double m_threshold = 90.0;
    QHash<QString, double> m_perSensor;
    QSet<QString> m_alerted;
};
