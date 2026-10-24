#include "core/HardwareInfo.h"
#include "providers/DrmProvider.h"
#include "providers/HwmonProvider.h"

#include <QtTest>

namespace {

void write(const QString &path, const QByteArray &content) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

const SensorReading *find(const QVector<SensorReading> &v, const QString &id) {
    for (const auto &r : v)
        if (r.id == id)
            return &r;
    return nullptr;
}

// A minimal pci.ids with the three GPU vendors.
QString writePciIds(const QString &dir) {
    const QString path = dir + "/pci.ids";
    write(path,
          "1002  Advanced Micro Devices, Inc. [AMD/ATI]\n"
          "\t744c  Navi 31 [Radeon RX 7900 XT/7900 XTX/7900M]\n"
          "\t15bf  Phoenix1\n"
          "8086  Intel Corporation\n"
          "\ta780  Raptor Lake-S GT1 [UHD Graphics 770]\n"
          "10de  NVIDIA Corporation\n"
          "\t28a0  AD107M [GeForce RTX 4060 Max-Q / Mobile]\n");
    return path;
}

// <root>/pci/<bus> is the PCI device; <root>/drm/cardN/device and the hwmon device point at it.
void makeGpu(const QString &root, const QString &card, const QString &bus, const QString &vendor, const QString &device,
             const QString &driver) {
    write(root + "/pci/" + bus + "/vendor", vendor.toUtf8() + "\n");
    write(root + "/pci/" + bus + "/device", device.toUtf8() + "\n");
    QDir().mkpath(root + "/drivers/" + driver);
    QVERIFY(QFile::link(root + "/drivers/" + driver, root + "/pci/" + bus + "/driver"));
    QDir().mkpath(root + "/drm/" + card);
    QVERIFY(QFile::link(root + "/pci/" + bus, root + "/drm/" + card + "/device"));
}

} // namespace

class TstDrm : public QObject {
    Q_OBJECT
private slots:
    void gpuNamesComeFromPciIds() {
        QTemporaryDir dir;
        const QString ids = writePciIds(dir.path());
        QCOMPARE(HardwareInfo::gpuName("1002", "744C", ids), QString("AMD Radeon RX 7900 XT/7900 XTX/7900M"));
        QCOMPARE(HardwareInfo::gpuName("8086", "a780", ids), QString("Intel UHD Graphics 770"));
        QCOMPARE(HardwareInfo::gpuName("10de", "28a0", ids), QString("NVIDIA GeForce RTX 4060 Max-Q / Mobile"));
        QCOMPARE(HardwareInfo::gpuName("1002", "15bf", ids), QString("AMD Phoenix1")); // no bracketed name
        QCOMPARE(HardwareInfo::gpuName("1002", "ffff", ids), QString("GPU (AMD)"));    // unknown device
        QCOMPARE(HardwareInfo::gpuName("abcd", "0001", ids), QString("GPU"));          // unknown vendor
    }

    void amdCardReportsLoadAndMemory() {
        QTemporaryDir dir;
        const QString ids = writePciIds(dir.path());
        makeGpu(dir.path(), "card0", "0000:03:00.0", "0x1002", "0x744c", "amdgpu");
        write(dir.path() + "/drm/card0/device/gpu_busy_percent", "37\n");
        write(dir.path() + "/drm/card0/device/mem_info_vram_used", "2147483648\n");   // 2 GiB
        write(dir.path() + "/drm/card0/device/mem_info_vram_total", "25769803776\n"); // 24 GiB
        QDir().mkpath(dir.path() + "/drm/card0-DP-1"); // connector entries must be ignored
        QDir().mkpath(dir.path() + "/drm/renderD128");

        DrmProvider p(dir.path() + "/drm", ids);
        const auto r = p.poll();
        const QString id = "amdgpu@0000:03:00.0";
        QCOMPARE(find(r, id + "/util")->value, 37.0);
        QCOMPARE(find(r, id + "/mem_used")->value, 2.0);
        QCOMPARE(find(r, id + "/mem_total")->value, 24.0);
        QCOMPARE(find(r, id + "/util")->category, DeviceCategory::Gpu);
        QCOMPARE(find(r, id + "/util")->deviceName, QString("AMD Radeon RX 7900 XT/7900 XTX/7900M"));
        QCOMPARE(find(r, id + "/pci_id")->text, QString("1002:744C"));
        QCOMPARE(find(r, id + "/driver")->text, QString("amdgpu"));
    }

    void drmAndHwmonSensorsOfOneGpuShareADeviceId() {
        QTemporaryDir dir;
        const QString ids = writePciIds(dir.path());
        makeGpu(dir.path(), "card0", "0000:03:00.0", "0x1002", "0x744c", "amdgpu");
        write(dir.path() + "/drm/card0/device/gpu_busy_percent", "10\n");
        // the matching hwmon device: name + a "device" link to the same PCI device
        write(dir.path() + "/hwmon/hwmon0/name", "amdgpu\n");
        write(dir.path() + "/hwmon/hwmon0/temp1_input", "58000\n");
        write(dir.path() + "/hwmon/hwmon0/temp1_label", "edge\n");
        write(dir.path() + "/hwmon/hwmon0/fan1_input", "1450\n");
        QVERIFY(QFile::link(dir.path() + "/pci/0000:03:00.0", dir.path() + "/hwmon/hwmon0/device"));

        const auto hw = HwmonProvider(dir.path() + "/hwmon").poll();
        const auto drm = DrmProvider(dir.path() + "/drm", ids).poll();
        const SensorReading *temp = find(hw, "amdgpu@0000:03:00.0/temp1");
        const SensorReading *util = find(drm, "amdgpu@0000:03:00.0/util");
        QVERIFY(temp && util);
        QCOMPARE(temp->deviceId, util->deviceId);       // one physical GPU = one device
        QCOMPARE(temp->category, DeviceCategory::Gpu);
        QCOMPARE(find(hw, "amdgpu@0000:03:00.0/fan1")->unit, QString("RPM"));
    }

    void intelCardReportsClocksOnly() {
        QTemporaryDir dir;
        const QString ids = writePciIds(dir.path());
        makeGpu(dir.path(), "card0", "0000:00:02.0", "0x8086", "0xa780", "i915");
        write(dir.path() + "/drm/card0/gt_cur_freq_mhz", "350\n");
        write(dir.path() + "/drm/card0/gt_max_freq_mhz", "1500\n");
        const auto r = DrmProvider(dir.path() + "/drm", ids).poll();
        const QString id = "i915@0000:00:02.0";
        QCOMPARE(find(r, id + "/clock")->value, 350.0);
        QCOMPARE(find(r, id + "/clock_max")->value, 1500.0);
        QVERIFY(!find(r, id + "/util"));    // load is not available from sysfs: no sensor, not a fake zero
        QVERIFY(!find(r, id + "/mem_used"));
    }

    void nvidiaCardsAreLeftToNvml() {
        QTemporaryDir dir;
        makeGpu(dir.path(), "card1", "0000:01:00.0", "0x10de", "0x28a0", "nvidia");
        QVERIFY(DrmProvider(dir.path() + "/drm").poll().isEmpty());
    }

    void hybridSystemWithTwoCards() {
        QTemporaryDir dir;
        const QString ids = writePciIds(dir.path());
        makeGpu(dir.path(), "card0", "0000:00:02.0", "0x8086", "0xa780", "i915");
        write(dir.path() + "/drm/card0/gt_cur_freq_mhz", "400\n");
        makeGpu(dir.path(), "card1", "0000:03:00.0", "0x1002", "0x744c", "amdgpu");
        write(dir.path() + "/drm/card1/device/gpu_busy_percent", "55\n");
        makeGpu(dir.path(), "card2", "0000:01:00.0", "0x10de", "0x28a0", "nvidia");

        const auto r = DrmProvider(dir.path() + "/drm", ids).poll();
        QSet<QString> devices;
        for (const auto &s : r)
            devices.insert(s.deviceId);
        QCOMPARE(devices, QSet<QString>({"i915@0000:00:02.0", "amdgpu@0000:03:00.0"})); // NVIDIA is NvidiaProvider's
    }

    void missingFilesAreUnavailableNotZeroAndMissingRootIsEmpty() {
        QTemporaryDir dir;
        makeGpu(dir.path(), "card0", "0000:03:00.0", "0x1002", "0x744c", "amdgpu"); // no gpu_busy_percent
        const auto r = DrmProvider(dir.path() + "/drm").poll();
        QVERIFY(!find(r, "amdgpu@0000:03:00.0/util")->available);
        QVERIFY(!find(r, "amdgpu@0000:03:00.0/mem_used")->available);
        QVERIFY(!find(r, "amdgpu@0000:03:00.0/mem_total")); // a fixed fact is only emitted when known
        QVERIFY(DrmProvider("/nonexistent/drm").poll().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstDrm)
#include "tst_drm.moc"
