#include "OverheatMonitor.h"

void OverheatMonitor::setSensorThreshold(const QString &sensorId, double celsius) {
    if (celsius > 0.0)
        m_perSensor.insert(sensorId, celsius);
    else
        m_perSensor.remove(sensorId);
    m_alerted.remove(sensorId); // judge the sensor afresh against its new limit
}

QVector<OverheatMonitor::Group> OverheatMonitor::groupByDevice(const QVector<Alert> &alerts) {
    QVector<Group> groups;
    QHash<QString, int> index; // device -> position, keeps the order in which devices first appeared
    for (const Alert &a : alerts) {
        const auto found = index.constFind(a.deviceName);
        if (found == index.constEnd()) {
            index.insert(a.deviceName, static_cast<int>(groups.size()));
            groups.push_back({a.deviceName, a.sensorName, a.value, 0});
            continue;
        }
        Group &g = groups[*found];
        ++g.others;
        if (a.value > g.value) {
            g.value = a.value;
            g.sensorName = a.sensorName;
        }
    }
    return groups;
}

QVector<OverheatMonitor::Alert> OverheatMonitor::update(const QVector<SensorReading> &readings) {
    QVector<Alert> alerts;
    if (m_threshold <= 0.0 && m_perSensor.isEmpty()) {
        m_alerted.clear();
        return alerts;
    }

    for (const SensorReading &r : readings) {
        // Derived values (average / hottest core) are computed from sensors that are checked themselves.
        if (r.type != SensorType::Temperature || !r.available || r.id.startsWith(QLatin1String("derived/")))
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
