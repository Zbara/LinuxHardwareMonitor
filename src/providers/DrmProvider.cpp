#include "DrmProvider.h"

#include "core/HardwareInfo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

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

// First readable numeric file among the candidates (kernel versions and drivers differ).
std::optional<double> firstNumber(const QStringList &candidates) {
    for (const QString &path : candidates)
        if (const auto v = readNumber(path))
            return v;
    return std::nullopt;
}

} // namespace

DrmProvider::DrmProvider(QString drmRoot, QString pciIdsPath)
    : m_root(std::move(drmRoot)), m_pciIds(std::move(pciIdsPath)) {}

QVector<SensorReading> DrmProvider::poll() {
    QVector<SensorReading> out;
    const QDateTime now = QDateTime::currentDateTime();

    // "card0", "card1", ... — not the connector entries like "card1-DP-1" or "renderD128".
    static const QRegularExpression cardRe(QStringLiteral("^card\\d+$"));
    QStringList cards = QDir(m_root).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    cards = cards.filter(cardRe);

    for (const QString &card : std::as_const(cards)) {
        const QString cardDir = m_root + QLatin1Char('/') + card;
        const QString dev = cardDir + QLatin1String("/device");
        const QString vendor = readText(dev + QLatin1String("/vendor")).remove(QLatin1String("0x")).toLower();
        const QString deviceId = readText(dev + QLatin1String("/device")).remove(QLatin1String("0x")).toLower();
        if (vendor.isEmpty())
            continue;
        if (vendor == QLatin1String("10de"))
            continue; // NVIDIA: NVML provides everything

        const QString driverPath = QFileInfo(dev + QLatin1String("/driver")).canonicalFilePath();
        const QString driver = driverPath.isEmpty() ? QString() : QFileInfo(driverPath).fileName();
        const QString busPath = QFileInfo(dev).canonicalFilePath();
        const QString bus = busPath.isEmpty() ? card : QFileInfo(busPath).fileName();
        if (driver.isEmpty())
            continue; // not bound to a driver: nothing to read

        // Same scheme as HwmonProvider (driver name + parent PCI device) so both group together.
        const QString gpuId = driver + QLatin1Char('@') + bus;
        const QString gpuName = HardwareInfo::gpuName(vendor, deviceId, m_pciIds);

        auto add = [&](const QString &key, const QString &name, SensorType type, const QString &unit,
                       std::optional<double> value) {
            SensorReading r;
            r.id = gpuId + QLatin1Char('/') + key;
            r.deviceId = gpuId;
            r.deviceName = gpuName;
            r.category = DeviceCategory::Gpu;
            r.name = name;
            r.type = type;
            r.unit = unit;
            r.available = value.has_value();
            r.value = value.value_or(0.0);
            r.timestamp = now;
            out.push_back(r);
        };
        auto addText = [&](const QString &key, const QString &name, const QString &text) {
            if (text.isEmpty())
                return;
            add(key, name, SensorType::Text, QString(), 0.0);
            out.last().text = text;
        };

        if (vendor == QLatin1String("1002")) { // AMD
            add(QStringLiteral("util"), QStringLiteral("GPU Utilization"), SensorType::Utilization, QStringLiteral("%"),
                readNumber(dev + QLatin1String("/gpu_busy_percent")));
            const auto used = readNumber(dev + QLatin1String("/mem_info_vram_used"));
            const auto total = readNumber(dev + QLatin1String("/mem_info_vram_total"));
            constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
            add(QStringLiteral("mem_used"), QStringLiteral("GPU Memory Used"), SensorType::Memory, QStringLiteral("GB"),
                used ? std::optional<double>(*used / kGiB) : std::nullopt);
            if (total && *total > 0) // total is a fixed fact: only emit it when known
                add(QStringLiteral("mem_total"), QStringLiteral("GPU Memory Total"), SensorType::Memory, QStringLiteral("GB"),
                    *total / kGiB);
        } else if (vendor == QLatin1String("8086")) { // Intel: clocks only; load needs perf counters
            const auto cur = firstNumber({cardDir + QLatin1String("/gt_cur_freq_mhz"), dev + QLatin1String("/tile0/gt0/freq0/cur_freq"),
                                          cardDir + QLatin1String("/gt/gt0/rps_cur_freq_mhz")});
            const auto max = firstNumber({cardDir + QLatin1String("/gt_max_freq_mhz"), dev + QLatin1String("/tile0/gt0/freq0/max_freq"),
                                          cardDir + QLatin1String("/gt/gt0/rps_max_freq_mhz")});
            add(QStringLiteral("clock"), QStringLiteral("GPU Clock"), SensorType::Frequency, QStringLiteral("MHz"), cur);
            if (max)
                add(QStringLiteral("clock_max"), QStringLiteral("GPU Max Clock"), SensorType::Frequency, QStringLiteral("MHz"), max);
        }

        addText(QStringLiteral("driver"), QStringLiteral("Driver"), driver);
        addText(QStringLiteral("pci_id"), QStringLiteral("PCI device ID"),
                QStringLiteral("%1:%2").arg(vendor.toUpper(), deviceId.toUpper()));
        addText(QStringLiteral("pci_bus"), QStringLiteral("PCI bus address"), bus);
    }
    return out;
}
