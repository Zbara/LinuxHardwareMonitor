#pragma once

#include "core/SensorTypes.h"

#include <QString>
#include <QVector>

// A source of sensor readings. poll() is always called from the single
// polling thread, never concurrently. It should return every sensor that is
// currently present; sensors that exist but cannot be read are returned with
// available == false. Sensors that stop being reported are marked unavailable
// by SensorManager.
class ISensorProvider {
public:
    virtual ~ISensorProvider() = default;

    virtual QString name() const = 0;
    virtual QVector<SensorReading> poll() = 0;
};
