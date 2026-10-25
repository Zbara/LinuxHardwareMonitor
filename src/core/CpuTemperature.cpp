#include "CpuTemperature.h"

#include <algorithm>

namespace CpuTemperature {

QString sourceName(Source source) {
    switch (source) {
    case Source::HottestCore: return QStringLiteral("hottest");
    case Source::AverageCores: return QStringLiteral("average");
    case Source::Package: break;
    }
    return QStringLiteral("package");
}

Source sourceFromName(const QString &name) {
    if (name == QLatin1String("hottest"))
        return Source::HottestCore;
    if (name == QLatin1String("average"))
        return Source::AverageCores;
    return Source::Package;
}

bool isPackageName(const QString &name) {
    return name.startsWith(QLatin1String("Package")) || name.startsWith(QLatin1String("Tctl"))
           || name.startsWith(QLatin1String("Tdie"));
}

bool isCoreName(const QString &name) {
    return name.startsWith(QLatin1String("Core ")) || name.startsWith(QLatin1String("Tccd"));
}

namespace {

bool isCpuTemperature(const SensorReading &r) {
    return r.category == DeviceCategory::Cpu && r.type == SensorType::Temperature;
}

} // namespace

void addDerived(QVector<SensorReading> &readings) {
    int cores = 0, available = 0;
    double sum = 0.0, hottest = 0.0;
    const SensorReading *anchor = nullptr; // the device the derived sensors are listed under
    for (const SensorReading &r : std::as_const(readings)) {
        if (!isCpuTemperature(r) || r.id.startsWith(QLatin1String("derived/")))
            continue;
        if (!anchor || isPackageName(r.name))
            anchor = &r;
        if (!isCoreName(r.name))
            continue;
        ++cores;
        if (!r.available)
            continue;
        if (available == 0 || r.value > hottest)
            hottest = r.value;
        sum += r.value;
        ++available;
    }
    if (cores < 2 || !anchor)
        return; // one core reading says nothing the package sensor does not

    auto make = [&](const char *id, const QString &name, double value) {
        SensorReading d;
        d.id = QLatin1String(id);
        d.deviceId = anchor->deviceId;
        d.deviceName = anchor->deviceName;
        d.category = DeviceCategory::Cpu;
        d.name = name;
        d.type = SensorType::Temperature;
        d.unit = anchor->unit;
        d.available = available > 0;
        d.value = available > 0 ? value : 0.0;
        d.timestamp = anchor->timestamp;
        return d;
    };
    const SensorReading avg = make(kAverageId, QStringLiteral("Average of cores"), available > 0 ? sum / available : 0.0);
    const SensorReading max = make(kHottestId, QStringLiteral("Hottest core"), hottest);
    readings.push_back(avg);
    readings.push_back(max);
}

Choice pick(const QVector<SensorReading> &readings, Source wanted) {
    const SensorReading *package = nullptr, *hottest = nullptr, *derivedAvg = nullptr, *derivedMax = nullptr;
    for (const SensorReading &r : readings) {
        if (!isCpuTemperature(r) || !r.available)
            continue;
        if (r.id == QLatin1String(kAverageId))
            derivedAvg = &r;
        else if (r.id == QLatin1String(kHottestId))
            derivedMax = &r;
        else {
            if (isPackageName(r.name) && (!package || r.value > package->value))
                package = &r;
            if (!hottest || r.value > hottest->value)
                hottest = &r;
        }
    }
    if (wanted == Source::AverageCores && derivedAvg)
        return {derivedAvg, Source::AverageCores};
    if (wanted == Source::HottestCore && derivedMax)
        return {derivedMax, Source::HottestCore};
    if (package)
        return {package, Source::Package};
    return {hottest, Source::Package}; // no package sensor: the hottest reading stands in for it
}

} // namespace CpuTemperature
