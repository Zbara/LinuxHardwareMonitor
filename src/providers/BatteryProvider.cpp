#include "BatteryProvider.h"

#include <QDir>
#include <QFile>

#include <cmath>
#include <optional>

namespace {

QString readText(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed() : QString();
}

std::optional<double> readNumber(const QString &path) {
    bool ok = false;
    const double v = readText(path).toDouble(&ok);
    return ok ? std::optional<double>(v) : std::nullopt;
}

} // namespace

BatteryProvider::BatteryProvider(QString root) : m_root(std::move(root)) {}

QVector<SensorReading> BatteryProvider::poll() {
    QVector<SensorReading> out;
    const QDateTime now = QDateTime::currentDateTime();

    QStringList supplies = QDir(m_root).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    // Is any mains adapter online?
    std::optional<bool> acOnline;
    for (const QString &s : std::as_const(supplies)) {
        const QString dir = m_root + QLatin1Char('/') + s;
        if (readText(dir + QLatin1String("/type")) == QLatin1String("Mains")) {
            if (const auto online = readNumber(dir + QLatin1String("/online")))
                acOnline = acOnline.value_or(false) || *online > 0;
        }
    }

    for (const QString &supply : std::as_const(supplies)) {
        const QString dir = m_root + QLatin1Char('/') + supply;
        if (readText(dir + QLatin1String("/type")) != QLatin1String("Battery"))
            continue;
        // Skip peripherals (mice, headsets) that report only a scope other than the system.
        const QString scope = readText(dir + QLatin1String("/scope"));
        if (!scope.isEmpty() && scope != QLatin1String("System"))
            continue;

        const QString deviceId = QStringLiteral("power:") + supply;
        QString model = readText(dir + QLatin1String("/model_name"));
        const QString deviceName = model.isEmpty() ? QStringLiteral("Battery") : QStringLiteral("Battery — ") + model;

        auto add = [&](const QString &key, const QString &name, SensorType type, const QString &unit,
                       std::optional<double> value) {
            SensorReading r;
            r.id = deviceId + QLatin1Char('/') + key;
            r.deviceId = deviceId;
            r.deviceName = deviceName;
            r.category = DeviceCategory::Other;
            r.name = name;
            r.type = type;
            r.unit = unit;
            r.available = value.has_value() && std::isfinite(*value);
            r.value = r.available ? *value : 0.0;
            r.timestamp = now;
            out.push_back(r);
        };
        auto addText = [&](const QString &key, const QString &name, const QString &text) {
            if (text.isEmpty())
                return;
            SensorReading r;
            r.id = deviceId + QLatin1Char('/') + key;
            r.deviceId = deviceId;
            r.deviceName = deviceName;
            r.category = DeviceCategory::Other;
            r.name = name;
            r.type = SensorType::Text;
            r.available = true;
            r.text = text;
            r.timestamp = now;
            out.push_back(r);
        };

        const auto voltageNow = readNumber(dir + QLatin1String("/voltage_now"));           // µV
        const auto voltageDesign = readNumber(dir + QLatin1String("/voltage_min_design")); // µV
        const double nominalUv = voltageDesign.value_or(voltageNow.value_or(0));

        // Energy in µWh: native energy_* files, or charge (µAh) × nominal voltage.
        auto energyUwh = [&](const QString &energyFile, const QString &chargeFile) -> std::optional<double> {
            if (const auto e = readNumber(dir + QLatin1Char('/') + energyFile))
                return e;
            if (const auto c = readNumber(dir + QLatin1Char('/') + chargeFile); c && nominalUv > 0)
                return *c * nominalUv / 1e6;
            return std::nullopt;
        };
        const auto now_ = energyUwh(QStringLiteral("energy_now"), QStringLiteral("charge_now"));
        const auto full = energyUwh(QStringLiteral("energy_full"), QStringLiteral("charge_full"));
        const auto design = energyUwh(QStringLiteral("energy_full_design"), QStringLiteral("charge_full_design"));

        std::optional<double> percent = readNumber(dir + QLatin1String("/capacity"));
        if (!percent && now_ && full && *full > 0)
            percent = 100.0 * *now_ / *full;
        add(QStringLiteral("charge"), QStringLiteral("Battery Charge"), SensorType::Utilization, QStringLiteral("%"), percent);

        auto wh = [](std::optional<double> uwh) { return uwh ? std::optional<double>(*uwh / 1e6) : std::nullopt; };
        add(QStringLiteral("energy"), QStringLiteral("Battery Energy"), SensorType::Energy, QStringLiteral("Wh"), wh(now_));
        add(QStringLiteral("energy_full"), QStringLiteral("Battery Full Capacity"), SensorType::Energy, QStringLiteral("Wh"), wh(full));
        add(QStringLiteral("energy_design"), QStringLiteral("Battery Design Capacity"), SensorType::Energy, QStringLiteral("Wh"), wh(design));
        if (full && design && *design > 0)
            add(QStringLiteral("health"), QStringLiteral("Battery Health"), SensorType::Utilization, QStringLiteral("%"),
                100.0 * *full / *design);

        // Power draw (W): power_now, else current × voltage.
        std::optional<double> watts;
        if (const auto p = readNumber(dir + QLatin1String("/power_now")))
            watts = std::abs(*p) / 1e6;
        else if (const auto c = readNumber(dir + QLatin1String("/current_now")); c && voltageNow)
            watts = std::abs(*c) * *voltageNow / 1e12;
        add(QStringLiteral("power"), QStringLiteral("Battery Power"), SensorType::Power, QStringLiteral("W"), watts);
        add(QStringLiteral("voltage"), QStringLiteral("Battery Voltage"), SensorType::Voltage, QStringLiteral("V"),
            voltageNow ? std::optional<double>(*voltageNow / 1e6) : std::nullopt);

        addText(QStringLiteral("status"), QStringLiteral("Battery status"), readText(dir + QLatin1String("/status")));
        addText(QStringLiteral("technology"), QStringLiteral("Battery technology"), readText(dir + QLatin1String("/technology")));
        addText(QStringLiteral("manufacturer"), QStringLiteral("Battery manufacturer"), readText(dir + QLatin1String("/manufacturer")));
        addText(QStringLiteral("model"), QStringLiteral("Battery model"), model);
        addText(QStringLiteral("serial"), QStringLiteral("Battery serial"), readText(dir + QLatin1String("/serial_number")));
        addText(QStringLiteral("cycles"), QStringLiteral("Battery cycles"), readText(dir + QLatin1String("/cycle_count")));
        if (acOnline)
            addText(QStringLiteral("ac"), QStringLiteral("AC adapter"), *acOnline ? QStringLiteral("online") : QStringLiteral("offline"));
    }
    return out;
}
