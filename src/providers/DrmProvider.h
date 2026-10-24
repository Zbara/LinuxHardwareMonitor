#pragma once

#include "ISensorProvider.h"

// GPU load, video memory and (Intel) clocks from /sys/class/drm. Covers the AMD amdgpu and Intel
// i915/xe drivers; NVIDIA cards are skipped because NvidiaProvider (NVML) owns them. Sensors are
// grouped under the same device id as the GPU's hwmon device (driver@PCI-address), so temperature,
// fan and power from HwmonProvider and load/memory from here show up as one GPU.
class DrmProvider : public ISensorProvider {
public:
    explicit DrmProvider(QString drmRoot = QStringLiteral("/sys/class/drm"), QString pciIdsPath = {});

    QString name() const override { return QStringLiteral("drm"); }
    QVector<SensorReading> poll() override;

private:
    QString m_root;
    QString m_pciIds;
};
