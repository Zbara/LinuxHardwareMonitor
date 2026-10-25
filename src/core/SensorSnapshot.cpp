#include "SensorSnapshot.h"

#include <QCollator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace SensorSnapshot {

QString typeName(SensorType type) {
    switch (type) {
    case SensorType::Temperature: return QStringLiteral("temperature");
    case SensorType::FanSpeed: return QStringLiteral("fan");
    case SensorType::Voltage: return QStringLiteral("voltage");
    case SensorType::Power: return QStringLiteral("power");
    case SensorType::Utilization: return QStringLiteral("utilization");
    case SensorType::Frequency: return QStringLiteral("frequency");
    case SensorType::Memory: return QStringLiteral("memory");
    case SensorType::Throughput: return QStringLiteral("throughput");
    case SensorType::Signal: return QStringLiteral("signal");
    case SensorType::Energy: return QStringLiteral("energy");
    case SensorType::Text: return QStringLiteral("text");
    }
    return QStringLiteral("unknown");
}

QString categoryName(DeviceCategory category) {
    switch (category) {
    case DeviceCategory::Cpu: return QStringLiteral("cpu");
    case DeviceCategory::Gpu: return QStringLiteral("gpu");
    case DeviceCategory::Storage: return QStringLiteral("storage");
    case DeviceCategory::System: return QStringLiteral("system");
    case DeviceCategory::Other: return QStringLiteral("other");
    }
    return QStringLiteral("other");
}

QByteArray toJson(const QVector<SensorReading> &readings, const QString &version, const QString &mode,
                  const QDateTime &when, bool compact) {
    QVector<SensorReading> sorted = readings;
    std::stable_sort(sorted.begin(), sorted.end(), [](const SensorReading &a, const SensorReading &b) {
        if (a.deviceName != b.deviceName)
            return a.deviceName < b.deviceName;
        return a.id < b.id;
    });

    QJsonArray sensors;
    for (const SensorReading &r : std::as_const(sorted)) {
        QJsonObject o;
        o["id"] = r.id;
        o["device"] = r.deviceName;
        o["device_id"] = r.deviceId;
        o["category"] = categoryName(r.category);
        o["name"] = r.name;
        o["type"] = typeName(r.type);
        o["unit"] = r.unit;
        o["available"] = r.available;
        if (r.type == SensorType::Text) {
            if (r.available)
                o["text"] = r.text;
        } else if (r.available && std::isfinite(r.value)) {
            o["value"] = r.value;
        } else {
            o["value"] = QJsonValue::Null; // unknown is unknown, not zero
        }
        sensors.append(o);
    }

    QJsonObject root;
    root["program"] = QStringLiteral("LinuxHardwareMonitor");
    root["version"] = version;
    root["mode"] = mode;
    root["time"] = when.toUTC().toString(Qt::ISODate);
    root["sensor_count"] = sensors.size();
    root["sensors"] = sensors;
    return QJsonDocument(root).toJson(compact ? QJsonDocument::Compact : QJsonDocument::Indented);
}

} // namespace SensorSnapshot
