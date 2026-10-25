#include "ThermalWatch.h"

QVector<ThermalWatch::Event> ThermalWatch::update(const QVector<SensorReading> &readings, qint64 cpuThrottleCount, qint64 nowMs) {
    QVector<Event> events;

    double hottest = NAN;
    for (const SensorReading &r : readings) {
        if (r.type == SensorType::Temperature && r.available
            && (r.category == DeviceCategory::Cpu || r.category == DeviceCategory::Gpu)
            && (std::isnan(hottest) || r.value > hottest))
            hottest = r.value;
    }

    // Processor: the first reading is only the baseline (the count since boot says nothing about now).
    if (cpuThrottleCount >= 0) {
        if (m_lastCpuCount >= 0 && cpuThrottleCount > m_lastCpuCount
            && (m_lastCpuAlertMs < 0 || nowMs - m_lastCpuAlertMs >= kCooldownMs)) {
            m_lastCpuAlertMs = nowMs;
            events.push_back({Kind::CpuThrottled, QString(), hottest});
        }
        m_lastCpuCount = cpuThrottleCount;
    }

    for (const SensorReading &r : readings) {
        if (r.type == SensorType::Text && r.id.endsWith(QLatin1String("/throttle")) && r.available
            && r.text.contains(QLatin1String("thermal"))) {
            const qint64 last = m_lastGpuAlertMs.value(r.id, -1);
            if (last < 0 || nowMs - last >= kCooldownMs) {
                m_lastGpuAlertMs.insert(r.id, nowMs);
                events.push_back({Kind::GpuThrottled, r.deviceName, hottest});
            }
        }
        // Fans in RPM only: a graphics card fan reported in percent legitimately idles at 0.
        if (r.type == SensorType::FanSpeed && r.unit == QLatin1String("RPM") && r.available) {
            FanState &fan = m_fans[r.id];
            if (r.value > 300.0) {
                fan.wasSpinning = true;
                fan.zeroPolls = 0;
                fan.alerted = false;
            } else if (r.value < 1.0 && fan.wasSpinning) {
                ++fan.zeroPolls;
                if (fan.zeroPolls >= kStoppedPolls && !fan.alerted && !std::isnan(hottest) && hottest >= kHotCelsius) {
                    fan.alerted = true;
                    events.push_back({Kind::FanStopped, r.name, hottest});
                }
            }
        }
    }
    return events;
}
