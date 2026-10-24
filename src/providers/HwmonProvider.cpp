#include "HwmonProvider.h"

#include "core/HardwareInfo.h"

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

#include <cmath>

namespace {

struct Kind {
    const char *prefix;
    SensorType type;
    double scale;
    const char *unit;
    const char *label;
};

constexpr Kind kKinds[] = {
    {"temp", SensorType::Temperature, 1e-3, "°C", "Temp"},
    {"fan", SensorType::FanSpeed, 1.0, "RPM", "Fan"},
    {"in", SensorType::Voltage, 1e-3, "V", "Voltage"},
    {"power", SensorType::Power, 1e-6, "W", "Power"},
    {"freq", SensorType::Frequency, 1e-6, "MHz", "Freq"},
};

const Kind *kindFor(const QString &prefix) {
    for (const Kind &k : kKinds)
        if (prefix == QLatin1String(k.prefix))
            return &k;
    return nullptr;
}

QString readText(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}

bool plausible(SensorType type, double v) {
    if (!std::isfinite(v))
        return false;
    switch (type) {
    case SensorType::Temperature: return v > -100.0 && v < 300.0;
    case SensorType::FanSpeed:
    case SensorType::Frequency:
    case SensorType::Power:       return v >= 0.0;
    default:                      return true;
    }
}

} // namespace

HwmonProvider::HwmonProvider(QString rootPath) : m_root(std::move(rootPath)) {}

std::optional<double> HwmonProvider::parseRaw(const QByteArray &raw, double scale) {
    bool ok = false;
    const qlonglong v = raw.trimmed().toLongLong(&ok);
    if (!ok)
        return std::nullopt;
    return static_cast<double>(v) * scale;
}

DeviceCategory HwmonProvider::categoryForDriver(const QString &driver) {
    static const QSet<QString> cpu = {"coretemp", "k10temp", "zenpower", "cpu_thermal", "fam15h_power"};
    static const QSet<QString> gpu = {"amdgpu", "nouveau", "radeon", "i915", "xe", "nvidia"};
    if (cpu.contains(driver))
        return DeviceCategory::Cpu;
    if (gpu.contains(driver))
        return DeviceCategory::Gpu;
    if (driver.startsWith(QLatin1String("nvme")) || driver == QLatin1String("drivetemp"))
        return DeviceCategory::Storage;
    return DeviceCategory::Other;
}

QString HwmonProvider::displayName(const QString &driver, const QString &model) {
    static const QHash<QString, QString> known = {
        {"coretemp", "CPU (Intel)"},        {"k10temp", "CPU (AMD)"},
        {"zenpower", "CPU (AMD)"},          {"acpitz", "ACPI Thermal Zone"},
        {"amdgpu", "GPU (AMD)"},            {"nouveau", "GPU (Nouveau)"},
        {"i915", "GPU (Intel)"},            {"ACAD", "AC Adapter"},
        {"acer", "Acer Laptop"},            {"asus", "ASUS Laptop"},
        {"thinkpad", "ThinkPad"},           {"dell_smm", "Dell"},
    };
    QString base = known.value(driver);
    if (base.isEmpty()) {
        if (driver.startsWith(QLatin1String("BAT")))
            base = QStringLiteral("Battery (%1)").arg(driver);
        else if (driver.startsWith(QLatin1String("iwlwifi")))
            base = QStringLiteral("Wi-Fi (iwlwifi)");
        else if (driver.startsWith(QLatin1String("acpitz")))
            base = QStringLiteral("ACPI Thermal Zone");
        else if (driver.startsWith(QLatin1String("nvme")))
            base = QStringLiteral("NVMe");
        else
            base = driver;
    }
    return model.isEmpty() ? base : base + QStringLiteral(" — ") + model;
}

QVector<SensorReading> HwmonProvider::poll() {
    QVector<SensorReading> out;

    QCollator collator;
    collator.setNumericMode(true);

    QStringList devices = QDir(m_root).entryList({QStringLiteral("hwmon*")},
                                                 QDir::Dirs | QDir::NoDotAndDotDot);
    std::sort(devices.begin(), devices.end(), collator);

    static const QRegularExpression fileRe(
        QStringLiteral("^(temp|fan|in|power|freq)(\\d+)_(input|average)$"));

    QHash<QString, int> idUses;
    const QDateTime now = QDateTime::currentDateTime();

    for (const QString &dirName : std::as_const(devices)) {
        const QString dirPath = m_root + QLatin1Char('/') + dirName;

        const QString driver = readText(dirPath + QLatin1String("/name"));
        const QString driverName = driver.isEmpty() ? dirName : driver;

        // hwmonN numbering is not stable across boots; the parent device is.
        QString deviceId = driverName;
        const QString parent = QFileInfo(dirPath + QLatin1String("/device")).canonicalFilePath();
        if (!parent.isEmpty())
            deviceId += QLatin1Char('@') + QFileInfo(parent).fileName();
        const int n = idUses[deviceId]++;
        if (n > 0)
            deviceId += QLatin1Char('#') + QString::number(n);

        QString title = displayName(driverName, readText(dirPath + QLatin1String("/device/model")));
        if (categoryForDriver(driverName) == DeviceCategory::Gpu) {
            // The same name DrmProvider uses, so both providers' sensors group under one GPU.
            const QString vendor = readText(dirPath + QLatin1String("/device/vendor")).remove(QLatin1String("0x"));
            const QString device = readText(dirPath + QLatin1String("/device/device")).remove(QLatin1String("0x"));
            if (!vendor.isEmpty())
                title = HardwareInfo::gpuName(vendor, device);
        }

        // Collect sensor files, preferring *_input over *_average.
        QMap<QString, QString> files; // "temp1" -> file name
        const QStringList entries = QDir(dirPath).entryList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            const auto m = fileRe.match(entry);
            if (!m.hasMatch())
                continue;
            const QString key = m.captured(1) + m.captured(2);
            if (!files.contains(key) || m.captured(3) == QLatin1String("input"))
                files[key] = entry;
        }

        QStringList keys = files.keys();
        std::sort(keys.begin(), keys.end(), collator);

        for (const QString &key : std::as_const(keys)) {
            const auto m = fileRe.match(files.value(key));
            const Kind *kind = kindFor(m.captured(1));
            if (!kind)
                continue;

            SensorReading r;
            r.id = deviceId + QLatin1Char('/') + key;
            r.deviceId = deviceId;
            r.deviceName = title;
            r.category = categoryForDriver(driverName);
            r.type = kind->type;
            r.unit = QString::fromUtf8(kind->unit);
            r.timestamp = now;

            const QString label = readText(dirPath + QLatin1Char('/') + key + QLatin1String("_label"));
            r.name = label.isEmpty()
                         ? QStringLiteral("%1 %2").arg(QLatin1String(kind->label), m.captured(2))
                         : label;

            QFile f(dirPath + QLatin1Char('/') + files.value(key));
            if (f.open(QIODevice::ReadOnly)) {
                const auto v = parseRaw(f.readAll(), kind->scale);
                if (v && plausible(kind->type, *v)) {
                    r.value = *v;
                    r.available = true;
                }
            }
            out.push_back(r);
        }
    }
    return out;
}
