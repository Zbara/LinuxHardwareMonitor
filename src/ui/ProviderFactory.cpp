#include "ProviderFactory.h"

#include "mock/MockProvider.h"
#include "providers/BatteryProvider.h"
#include "providers/DrmProvider.h"
#include "providers/HwmonProvider.h"
#include "providers/IoProvider.h"
#include "providers/NvidiaProvider.h"
#include "providers/SystemProvider.h"

std::vector<std::unique_ptr<ISensorProvider>> createProviders(const AppOptions &options, MockProvider **mockOut) {
    std::vector<std::unique_ptr<ISensorProvider>> list;
    if (mockOut)
        *mockOut = nullptr;
    if (options.mock) {
        // Simulation mode: one provider stands in for all the hardware ones.
        auto mock = std::make_unique<MockProvider>(*options.mock);
        mock->setSpeed(options.mockSpeed);
        if (mockOut)
            *mockOut = mock.get();
        list.push_back(std::move(mock));
        return list;
    }
    // Empty roots mean the real /sys and /proc; fixture mode points them at test trees.
    auto orDefault = [](const QString &path, const QString &fallback) { return path.isEmpty() ? fallback : path; };
    const QString proc = orDefault(options.procRoot, QStringLiteral("/proc"));
    const QString cpu = orDefault(options.cpuRoot, QStringLiteral("/sys/devices/system/cpu"));
    list.push_back(std::make_unique<HwmonProvider>(orDefault(options.hwmonRoot, QStringLiteral("/sys/class/hwmon"))));
    list.push_back(std::make_unique<SystemProvider>(proc, cpu));
    list.push_back(std::make_unique<IoProvider>(proc));
    list.push_back(std::make_unique<BatteryProvider>(orDefault(options.powerSupplyRoot, QStringLiteral("/sys/class/power_supply"))));
    list.push_back(std::make_unique<DrmProvider>(orDefault(options.drmRoot, QStringLiteral("/sys/class/drm"))));
    if (!options.fixtureMode()) // the NVIDIA driver is the real machine's, not the fixture's
        list.push_back(std::make_unique<NvidiaProvider>());
    return list;
}
