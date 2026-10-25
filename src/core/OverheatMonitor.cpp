#include "OverheatMonitor.h"

void OverheatMonitor::setSensorThreshold(const QString &sensorId, double celsius) {
    if (celsius > 0.0)
        m_perSensor.insert(sensorId, celsius);
    else
        m_perSensor.remove(sensorId);
    m_alerted.remove(sensorId); // judge the sensor afresh against its new limit
}

QVector<OverheatMonitor::Alert> OverheatMonitor::update(const QVector<SensorReading> &readings) {
    QVector<Alert> alerts;
    if (m_threshold <= 0.0 && m_perSensor.isEmpty()) {
        m_alerted.clear();
        return alerts;
    }

    for (const SensorReading &r : readings) {
        if (r.type != SensorType::Temperature || !r.available)
            continue;
        const double limit = m_perSensor.value(r.id, m_threshold);
        if (limit <= 0.0) {
            m_alerted.remove(r.id);
            continue;
        }
        if (r.value >= limit) {
            if (!m_alerted.contains(r.id)) {
                m_alerted.insert(r.id);
                alerts.push_back({r.id, r.deviceName, r.name, r.value});
            }
        } else if (r.value < limit - kHysteresis) {
            m_alerted.remove(r.id);
        }
    }
    return alerts;
}
