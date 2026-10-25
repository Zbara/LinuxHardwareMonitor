#pragma once

#include "SensorTypes.h"

#include <QByteArray>
#include <QDateTime>
#include <QVector>

// The sensor readings as one JSON document, for `--dump-json` (scripts, bug reports, headless machines).
// An unavailable sensor has "value": null - never 0.
namespace SensorSnapshot {

QString typeName(SensorType type);
QString categoryName(DeviceCategory category);

// `mode` is "real", "simulation (<profile>)" or "test data".
QByteArray toJson(const QVector<SensorReading> &readings, const QString &version, const QString &mode,
                  const QDateTime &when, bool compact = false);

} // namespace SensorSnapshot
