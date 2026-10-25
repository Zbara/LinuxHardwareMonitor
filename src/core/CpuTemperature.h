#pragma once

#include "SensorTypes.h"

#include <QString>
#include <QVector>

// Which of the CPU's temperature readings is "the" CPU temperature. Intel reports one sensor for the
// whole package (the hottest spot, what throttling reacts to) and one per core, AMD a Tctl/Tdie and
// one per chiplet. Tools disagree on the choice, so it is a setting.
namespace CpuTemperature {

enum class Source { Package, HottestCore, AverageCores };

QString sourceName(Source source);                 // "package", "hottest", "average"
Source sourceFromName(const QString &name);       // unknown names mean Package

// Sensors computed from the per-core readings, added to the list a provider cycle returned so that they
// get a history like any other sensor. Present only when there are at least two core readings.
inline constexpr const char *kAverageId = "derived/cpu_temp_avg";
inline constexpr const char *kHottestId = "derived/cpu_temp_hottest_core";
void addDerived(QVector<SensorReading> &readings);

struct Choice {
    const SensorReading *reading = nullptr; // nullptr: the machine has no CPU temperature at all
    Source used = Source::Package;          // what was actually used (a fallback may differ from the request)
};
// The reading to show for `wanted`. When the derived sensor is missing or unavailable (a machine without
// per-core sensors), or no package sensor exists, it falls back: wanted -> package -> hottest CPU sensor.
Choice pick(const QVector<SensorReading> &readings, Source wanted);

bool isPackageName(const QString &name);          // "Package id 0", "Tctl", "Tdie"
bool isCoreName(const QString &name);             // "Core 12", "Tccd1"

} // namespace CpuTemperature
