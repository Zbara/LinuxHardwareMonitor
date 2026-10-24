#include "OverheatMonitor.h"

QVector<OverheatMonitor::Alert> OverheatMonitor::update(const QVector<SensorReading> &readings) {
    QVector<Alert> alerts;
    if (m_threshold <= 0.0) {
        m_alerted.clear();
        return alerts;
    }

    for (const SensorReading &r : readings) {
        if (r.type != SensorType::Temperature || !r.available)
            continue;
        if (r.value >= m_threshold) {
            if (!m_alerted.contains(r.id)) {
                m_alerted.insert(r.id);
                alerts.push_back({r.id, r.deviceName, r.name, r.value});
            }
        } else if (r.value < m_threshold - kHysteresis) {
            m_alerted.remove(r.id);
        }
    }
    return alerts;
}
