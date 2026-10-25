#pragma once

#include "SensorTypes.h"

#include <QHash>
#include <QString>
#include <QVector>

#include <cmath>

// Spots the situations a temperature threshold alone does not catch:
//  - the processor slowing itself down because of heat (the kernel's thermal throttle counter grew),
//  - a graphics card whose driver reports a thermal slowdown (a text sensor "<gpu>/throttle"),
//  - a fan that was spinning and now reads 0 RPM while the CPU or GPU is hot.
// Every event is raised once per episode (and throttling at most once per cooldown), so a machine that
// hovers at its limit does not produce a stream of messages. Pure logic: time and readings come in.
class ThermalWatch {
public:
    enum class Kind { CpuThrottled, GpuThrottled, FanStopped };

    struct Event {
        Kind kind;
        QString subject;                  // device or fan name
        double temperature = NAN;         // the hottest CPU/GPU reading, when known
    };

    // `cpuThrottleCount` is the kernel's cumulative throttle count, -1 when it is not available.
    QVector<Event> update(const QVector<SensorReading> &readings, qint64 cpuThrottleCount, qint64 nowMs);

    static constexpr qint64 kCooldownMs = 10 * 60 * 1000;
    static constexpr double kHotCelsius = 70.0; // a stopped fan only matters when something is this hot
    static constexpr int kStoppedPolls = 4;     // consecutive 0 RPM readings before it counts as stopped

private:
    struct FanState {
        bool wasSpinning = false;
        int zeroPolls = 0;
        bool alerted = false;
    };
    qint64 m_lastCpuCount = -1;
    qint64 m_lastCpuAlertMs = -1;
    QHash<QString, qint64> m_lastGpuAlertMs;
    QHash<QString, FanState> m_fans;
};
