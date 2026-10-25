#include "core/SystemReport.h"

#include <QtTest>

class TstSystemReport : public QObject {
    Q_OBJECT
private slots:
    void describesTheMachine() {
        QVariantMap gpu{{"name", "NVIDIA GeForce RTX 4060"}, {"info", QVariantMap{{"driver", "550.1"}}}};
        QVariantMap disk{{"model", "Micron 3400"}, {"sizeBytes", 1.024e12}, {"serial", "SN-SECRET-123"}};
        QVariantMap wifi{{"model", "Intel CNVi WiFi"}, {"kind", "Wi-Fi"}, {"mac", "aa:bb:cc:dd:ee:ff"},
                         {"ipv4", QStringList{"192.168.1.77"}}, {"wifi", QVariantMap{{"ssid", "HomeNetwork"}}}};
        QVariantMap details{{"cpuModel", "Intel Core i9"}, {"sockets", 1}, {"physicalCores", 24}, {"logicalCpus", 32},
                            {"memTotalGb", 61.3}, {"gpus", QVariantList{gpu}}, {"disks", QVariantList{disk}},
                            {"nets", QVariantList{wifi}}, {"batteries", QVariantList{QVariantMap()}}};
        const QString text = SystemReport::format(details, {"0.1", "Ubuntu 26.04", "7.0.0", "6.10.2", "software", "ru", ""});

        QVERIFY(text.startsWith("Linux Hardware Monitor 0.1\n"));
        QVERIFY(text.contains("OS: Ubuntu 26.04"));
        QVERIFY(text.contains("Renderer: software"));
        QVERIFY(text.contains("CPU: Intel Core i9 (1 socket(s), 24 cores, 32 threads)"));
        QVERIFY(text.contains("Memory: 61.3 GB"));
        QVERIFY(text.contains("GPU 0: NVIDIA GeForce RTX 4060 (driver 550.1)"));
        QVERIFY(text.contains("Disk 0: Micron 3400, 1024 GB"));
        QVERIFY(text.contains("Network: Intel CNVi WiFi (Wi-Fi)"));
        QVERIFY(text.contains("Battery: present"));
        QVERIFY(!text.contains("Mode:")); // the real machine needs no note
    }

    void countsVirtualInterfacesInsteadOfListingThem() {
        QVariantList nets;
        for (int i = 0; i < 40; ++i)
            nets << QVariantMap{{"name", QString("veth%1").arg(i)}, {"kind", "Virtual"}};
        nets << QVariantMap{{"model", "Killer E2600"}, {"kind", "Ethernet"}};
        const QString text = SystemReport::format({{"nets", nets}}, {"0.1", {}, {}, {}, {}, {}, {}});
        QVERIFY(text.contains("Network: Killer E2600 (Ethernet); 40 virtual or unidentified interface(s)"));
        QVERIFY(!text.contains("veth"));
    }

    void neverLeaksPersonalData() {
        QVariantMap disk{{"model", "Disk"}, {"serial", "SN-SECRET-123"}};
        QVariantMap net{{"model", "Adapter"}, {"kind", "Ethernet"}, {"mac", "aa:bb:cc:dd:ee:ff"},
                        {"ipv4", QStringList{"192.168.1.77"}}, {"name", "wlp0s20f3"}, {"wifi", QVariantMap{{"ssid", "HomeNetwork"}, {"bssid", "C8:7F:54"}}}};
        const QString text = SystemReport::format({{"disks", QVariantList{disk}}, {"nets", QVariantList{net}}}, {"0.1", {}, {}, {}, {}, {}, {}});
        for (const char *secret : {"SN-SECRET-123", "aa:bb:cc", "192.168", "HomeNetwork", "C8:7F:54", "wlp0s20f3"})
            QVERIFY2(!text.contains(secret), secret);
    }

    void missingFactsAreLeftOutNotInvented() {
        const QString text = SystemReport::format({}, {"", "", "", "", "", "", "simulation (amd-desktop.json)"});
        QVERIFY(text.contains("(unknown version)"));
        QVERIFY(text.contains("Mode: simulation (amd-desktop.json)"));
        for (const char *label : {"CPU:", "Memory:", "GPU", "Disk", "Network:", "Battery:", "OS:", "Kernel:"})
            QVERIFY2(!text.contains(label), label);
    }
};

QTEST_GUILESS_MAIN(TstSystemReport)
#include "tst_systemreport.moc"
