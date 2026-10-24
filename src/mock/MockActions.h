#pragma once

#include "MockProfile.h"

#include <QString>
#include <QVector>

// One button of the in-app simulation panel: "make this happen now".
struct MockAction {
    // overheat_cpu | overheat_gpu | gpu_loss | cpu_sensor_dropout | fan_stop | fan_boost | disk_dropout
    QString kind;
    QString targetName; // what the user sees: "CPU", "AMD Radeon RX 7900 XTX", "CPU Fan", "nvme0n1"
    MockScenario scenario; // start_s is filled in when the action runs
};

namespace MockActions {

// The actions that make sense for this profile's hardware (no GPU actions without a GPU, ...).
QVector<MockAction> build(const MockProfile &profile);

} // namespace MockActions
