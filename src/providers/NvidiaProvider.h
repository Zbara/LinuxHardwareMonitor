#pragma once

#include "ISensorProvider.h"

#include <QLibrary>

#include <memory>
#include <vector>

// NVIDIA GPUs through NVML. libnvidia-ml.so.1 is loaded at runtime; if it is
// missing or fails to initialise the provider simply reports no sensors.
class NvidiaProvider : public ISensorProvider {
public:
    NvidiaProvider();
    ~NvidiaProvider() override;

    QString name() const override { return QStringLiteral("nvidia"); }
    QVector<SensorReading> poll() override;

private:
    struct Api;
    struct Gpu {
        void *handle = nullptr;
        QString deviceId;
        QString deviceName;
        QString pciId, pciSubsystem, pciBus;
    };

    bool ensureInitialised();
    void enumerate();

    std::unique_ptr<Api> m_api;
    std::vector<Gpu> m_gpus;
    QString m_driverVersion, m_cudaVersion;
    bool m_triedInit = false;
    bool m_ready = false;
};
