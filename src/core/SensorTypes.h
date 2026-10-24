#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>

enum class SensorType {
    Temperature,
    FanSpeed,
    Voltage,
    Power,
    Utilization,
    Frequency,
    Memory,
    Throughput,
    Signal,
    Energy, // watt-hours (battery capacity)
    Text // static facts shown as text (driver version, PCI id, ...); not a numeric sensor
};

enum class DeviceCategory {
    Cpu,
    Gpu,
    Storage,
    System,
    Other
};

struct SensorReading {
    QString id;          // stable across polls and (as far as possible) across boots
    QString deviceId;
    QString deviceName;
    DeviceCategory category = DeviceCategory::Other;
    QString name;
    SensorType type = SensorType::Temperature;
    double value = 0.0;  // meaningless unless available == true
    QString text;        // only for SensorType::Text
    QString unit;
    bool available = false;
    QDateTime timestamp;
};

Q_DECLARE_METATYPE(SensorReading)
Q_DECLARE_METATYPE(QVector<SensorReading>)
