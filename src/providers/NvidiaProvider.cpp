#include "NvidiaProvider.h"

#include <utility>

namespace {

using nvmlReturn_t = int;
using nvmlDevice_t = void *;
constexpr nvmlReturn_t NVML_SUCCESS = 0;

struct nvmlMemory_t {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

struct nvmlPciInfo_t {
    char busIdLegacy[16];
    unsigned int domain, bus, device, pciDeviceId, pciSubSystemId;
    char busId[32];
};

struct nvmlUtilization_t {
    unsigned int gpu;
    unsigned int memory;
};

} // namespace

struct NvidiaProvider::Api {
    QLibrary lib{QStringLiteral("libnvidia-ml.so.1")};

    nvmlReturn_t (*init)() = nullptr;
    nvmlReturn_t (*shutdown)() = nullptr;
    nvmlReturn_t (*deviceCount)(unsigned int *) = nullptr;
    nvmlReturn_t (*handleByIndex)(unsigned int, nvmlDevice_t *) = nullptr;
    nvmlReturn_t (*getName)(nvmlDevice_t, char *, unsigned int) = nullptr;
    nvmlReturn_t (*getUuid)(nvmlDevice_t, char *, unsigned int) = nullptr;
    nvmlReturn_t (*getTemperature)(nvmlDevice_t, int sensor, unsigned int *) = nullptr;
    nvmlReturn_t (*getUtilization)(nvmlDevice_t, nvmlUtilization_t *) = nullptr;
    nvmlReturn_t (*getPower)(nvmlDevice_t, unsigned int *) = nullptr;
    nvmlReturn_t (*getClock)(nvmlDevice_t, int clockType, unsigned int *) = nullptr;
    nvmlReturn_t (*getMemory)(nvmlDevice_t, nvmlMemory_t *) = nullptr;
    nvmlReturn_t (*getFan)(nvmlDevice_t, unsigned int *) = nullptr;
    nvmlReturn_t (*getMaxClock)(nvmlDevice_t, int clockType, unsigned int *) = nullptr;
    nvmlReturn_t (*getEncoder)(nvmlDevice_t, unsigned int *, unsigned int *) = nullptr;
    nvmlReturn_t (*getDecoder)(nvmlDevice_t, unsigned int *, unsigned int *) = nullptr;
    nvmlReturn_t (*getPci)(nvmlDevice_t, nvmlPciInfo_t *) = nullptr;
    nvmlReturn_t (*getPowerLimit)(nvmlDevice_t, unsigned int *) = nullptr;
    nvmlReturn_t (*driverVersion)(char *, unsigned int) = nullptr;
    nvmlReturn_t (*cudaVersion)(int *) = nullptr;
    bool initialised = false;

    template <typename F> bool resolve(F &fn, const char *symbol) {
        fn = reinterpret_cast<F>(lib.resolve(symbol));
        return fn != nullptr;
    }

    // Core symbols are mandatory; the metric getters are optional.
    bool load() {
        if (!lib.load())
            return false;
        const bool core = resolve(init, "nvmlInit_v2") && resolve(shutdown, "nvmlShutdown")
                          && resolve(deviceCount, "nvmlDeviceGetCount_v2")
                          && resolve(handleByIndex, "nvmlDeviceGetHandleByIndex_v2")
                          && resolve(getTemperature, "nvmlDeviceGetTemperature");
        resolve(getName, "nvmlDeviceGetName");
        resolve(getUuid, "nvmlDeviceGetUUID");
        resolve(getUtilization, "nvmlDeviceGetUtilizationRates");
        resolve(getPower, "nvmlDeviceGetPowerUsage");
        resolve(getClock, "nvmlDeviceGetClockInfo");
        resolve(getMemory, "nvmlDeviceGetMemoryInfo");
        resolve(getFan, "nvmlDeviceGetFanSpeed");
        resolve(getMaxClock, "nvmlDeviceGetMaxClockInfo");
        resolve(getEncoder, "nvmlDeviceGetEncoderUtilization");
        resolve(getDecoder, "nvmlDeviceGetDecoderUtilization");
        resolve(getPci, "nvmlDeviceGetPciInfo_v3");
        resolve(getPowerLimit, "nvmlDeviceGetEnforcedPowerLimit");
        resolve(driverVersion, "nvmlSystemGetDriverVersion");
        if (!resolve(cudaVersion, "nvmlSystemGetCudaDriverVersion_v2"))
            resolve(cudaVersion, "nvmlSystemGetCudaDriverVersion");
        return core;
    }
};

NvidiaProvider::NvidiaProvider() = default;

NvidiaProvider::~NvidiaProvider() {
    if (m_api && m_api->initialised)
        m_api->shutdown();
}

bool NvidiaProvider::ensureInitialised() {
    if (m_triedInit)
        return m_ready;
    m_triedInit = true;

    auto api = std::make_unique<Api>();
    if (!api->load() || api->init() != NVML_SUCCESS)
        return false;
    api->initialised = true;
    m_api = std::move(api);
    m_ready = true;
    char version[96] = {};
    if (m_api->driverVersion && m_api->driverVersion(version, sizeof version) == NVML_SUCCESS)
        m_driverVersion = QString::fromUtf8(version);
    int cuda = 0;
    if (m_api->cudaVersion && m_api->cudaVersion(&cuda) == NVML_SUCCESS && cuda > 0)
        m_cudaVersion = QStringLiteral("%1.%2").arg(cuda / 1000).arg(cuda % 1000 / 10);
    enumerate();
    return true;
}

void NvidiaProvider::enumerate() {
    unsigned int count = 0;
    if (m_api->deviceCount(&count) != NVML_SUCCESS)
        return;

    for (unsigned int i = 0; i < count; ++i) {
        nvmlDevice_t h = nullptr;
        if (m_api->handleByIndex(i, &h) != NVML_SUCCESS)
            continue;

        char buf[96] = {};
        Gpu gpu;
        gpu.handle = h;

        QString uuid;
        if (m_api->getUuid && m_api->getUuid(h, buf, sizeof buf) == NVML_SUCCESS)
            uuid = QString::fromUtf8(buf);
        gpu.deviceId = QStringLiteral("nvidia:") + (uuid.isEmpty() ? QString::number(i) : uuid);

        QString name = QStringLiteral("NVIDIA GPU %1").arg(i);
        buf[0] = '\0';
        if (m_api->getName && m_api->getName(h, buf, sizeof buf) == NVML_SUCCESS)
            name = QString::fromUtf8(buf);
        gpu.deviceName = name;

        if (m_api->getPci) {
            nvmlPciInfo_t pci{};
            if (m_api->getPci(h, &pci) == NVML_SUCCESS) {
                // Vendor id is the low 16 bits of the combined device id.
                gpu.pciId = QStringLiteral("%1:%2").arg(pci.pciDeviceId & 0xffff, 4, 16, QLatin1Char('0')).arg(pci.pciDeviceId >> 16, 4, 16, QLatin1Char('0')).toUpper();
                gpu.pciSubsystem = QStringLiteral("%1:%2").arg(pci.pciSubSystemId & 0xffff, 4, 16, QLatin1Char('0')).arg(pci.pciSubSystemId >> 16, 4, 16, QLatin1Char('0')).toUpper();
                gpu.pciBus = QString::fromLatin1(pci.busId);
            }
        }

        m_gpus.push_back(std::move(gpu));
    }
}

QVector<SensorReading> NvidiaProvider::poll() {
    QVector<SensorReading> out;
    if (!ensureInitialised())
        return out;

    const QDateTime now = QDateTime::currentDateTime();

    for (const Gpu &gpu : m_gpus) {
        auto add = [&](const QString &key, const QString &name, SensorType type,
                       const QString &unit, bool ok, double value) {
            SensorReading r;
            r.id = gpu.deviceId + QLatin1Char('/') + key;
            r.deviceId = gpu.deviceId;
            r.deviceName = gpu.deviceName;
            r.category = DeviceCategory::Gpu;
            r.name = name;
            r.type = type;
            r.unit = unit;
            r.available = ok;
            r.value = ok ? value : 0.0;
            r.timestamp = now;
            out.push_back(r);
        };

        unsigned int v = 0;
        // Evaluated before the call to add(): argument evaluation order is unspecified.
        const bool tempOk = m_api->getTemperature(gpu.handle, 0, &v) == NVML_SUCCESS;
        add(QStringLiteral("temp"), QStringLiteral("GPU Temperature"), SensorType::Temperature,
            QStringLiteral("°C"), tempOk, v);

        if (m_api->getUtilization) {
            nvmlUtilization_t u{};
            const bool ok = m_api->getUtilization(gpu.handle, &u) == NVML_SUCCESS;
            add(QStringLiteral("util"), QStringLiteral("GPU Utilization"), SensorType::Utilization,
                QStringLiteral("%"), ok, u.gpu);
        }
        if (m_api->getPower) {
            const bool ok = m_api->getPower(gpu.handle, &v) == NVML_SUCCESS;
            add(QStringLiteral("power"), QStringLiteral("GPU Power"), SensorType::Power,
                QStringLiteral("W"), ok, v / 1000.0);
        }
        if (m_api->getClock) {
            const bool ok = m_api->getClock(gpu.handle, 0 /* graphics */, &v) == NVML_SUCCESS;
            add(QStringLiteral("clock"), QStringLiteral("GPU Clock"), SensorType::Frequency,
                QStringLiteral("MHz"), ok, v);
        }
        if (m_api->getMemory) {
            nvmlMemory_t mem{};
            const bool ok = m_api->getMemory(gpu.handle, &mem) == NVML_SUCCESS;
            add(QStringLiteral("mem_used"), QStringLiteral("GPU Memory Used"), SensorType::Memory,
                QStringLiteral("GB"), ok, double(mem.used) / (1024.0 * 1024.0 * 1024.0));
            add(QStringLiteral("mem_total"), QStringLiteral("GPU Memory Total"), SensorType::Memory,
                QStringLiteral("GB"), ok, double(mem.total) / (1024.0 * 1024.0 * 1024.0));
        }
        if (m_api->getUtilization) {
            nvmlUtilization_t u{};
            const bool ok = m_api->getUtilization(gpu.handle, &u) == NVML_SUCCESS;
            add(QStringLiteral("util_mem"), QStringLiteral("GPU Memory Controller"), SensorType::Utilization,
                QStringLiteral("%"), ok, u.memory);
        }
        for (const auto &codec : {std::make_pair(m_api->getEncoder, "enc"), std::make_pair(m_api->getDecoder, "dec")}) {
            if (!codec.first)
                continue;
            unsigned int usage = 0, period = 0;
            const bool ok = codec.first(gpu.handle, &usage, &period) == NVML_SUCCESS;
            add(QString::fromLatin1(codec.second),
                QString::fromLatin1(codec.second) == QLatin1String("enc") ? QStringLiteral("GPU Video Encode")
                                                                         : QStringLiteral("GPU Video Decode"),
                SensorType::Utilization, QStringLiteral("%"), ok, usage);
        }
        if (m_api->getMaxClock) {
            const bool ok = m_api->getMaxClock(gpu.handle, 0 /* graphics */, &v) == NVML_SUCCESS;
            add(QStringLiteral("clock_max"), QStringLiteral("GPU Max Clock"), SensorType::Frequency,
                QStringLiteral("MHz"), ok, v);
        }
        if (m_api->getPowerLimit) {
            const bool ok = m_api->getPowerLimit(gpu.handle, &v) == NVML_SUCCESS;
            add(QStringLiteral("power_limit"), QStringLiteral("GPU Power Limit"), SensorType::Power,
                QStringLiteral("W"), ok, v / 1000.0);
        }
        // Static facts, reported as text sensors.
        auto addText = [&](const QString &key, const QString &name, const QString &text) {
            if (text.isEmpty())
                return;
            add(key, name, SensorType::Text, QString(), true, 0);
            out.last().text = text;
        };
        addText(QStringLiteral("driver"), QStringLiteral("Driver version"), m_driverVersion);
        addText(QStringLiteral("cuda"), QStringLiteral("CUDA version"), m_cudaVersion);
        addText(QStringLiteral("pci_id"), QStringLiteral("PCI device ID"), gpu.pciId);
        addText(QStringLiteral("pci_subsystem"), QStringLiteral("PCI subsystem ID"), gpu.pciSubsystem);
        addText(QStringLiteral("pci_bus"), QStringLiteral("PCI bus address"), gpu.pciBus);

        if (m_api->getFan) {
            // Reported as a percentage of maximum; absent on many laptops.
            const bool ok = m_api->getFan(gpu.handle, &v) == NVML_SUCCESS;
            add(QStringLiteral("fan"), QStringLiteral("GPU Fan"), SensorType::FanSpeed,
                QStringLiteral("%"), ok, v);
        }
    }
    return out;
}
