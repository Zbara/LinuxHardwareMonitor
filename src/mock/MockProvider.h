#pragma once

#include "MockProfile.h"
#include "providers/ISensorProvider.h"

#include <QElapsedTimer>
#include <QMutex>
#include <QRandomGenerator>
#include <QVector>

// Produces the same sensors (ids, devices, units) as the real providers would for the
// profile's hardware, so the UI cannot tell the difference. Replaces all hardware providers
// in simulation mode.
class MockProvider : public ISensorProvider {
public:
    explicit MockProvider(MockProfile profile);

    QString name() const override { return QStringLiteral("mock"); }
    QVector<SensorReading> poll() override { return pollAt(static_cast<qint64>(double(m_clock.elapsed()) * m_speed)); }
    // Runs the simulated clock faster (e.g. to reach a scenario without waiting). Default 1.
    void setSpeed(double factor) { m_speed = factor > 0 ? factor : 1.0; }

    // Same as poll() with an explicit simulated clock (milliseconds since start), for tests.
    QVector<SensorReading> pollAt(qint64 elapsedMs);

    const MockProfile &profile() const { return m_profile; }

    // Scenarios started while the app runs (from the simulation panel). Safe to call from any
    // thread; they take effect on the next poll. `inject` starts at the current simulated time.
    void inject(MockScenario scenario);
    void injectAt(MockScenario scenario, double startS);
    void clearInjected();
    double simulatedSeconds() const { return double(m_clock.elapsed()) * m_speed / 1000.0; }

private:
    // Mean-reverting random walk clamped to its spec.
    struct Walker {
        SignalSpec spec;
        double value = 0;
        double next(QRandomGenerator &rng);
    };

    Walker makeWalker(const SignalSpec &spec);

    // Scenario helpers
    double overheatFactor(const QString &target, double t) const; // 0..1
    double overheatPeak(const QString &target, double fallback) const;
    // `value` pulled towards the scenario peak by the current overheat factor.
    double withOverheat(const QString &target, double t, double value) const;
    bool active(const QString &type, const QString &target, double t) const;
    std::optional<double> fanOverride(const QString &fan, double t) const;

    MockProfile m_profile;
    QMutex m_mutex;                    // guards m_injected
    QVector<MockScenario> m_injected;
    QVector<MockScenario> m_scenarios; // profile + injected, snapshot used by the current poll
    QRandomGenerator m_rng;
    QElapsedTimer m_clock;
    double m_speed = 1.0;

    Walker m_cpuTemp, m_cpuFreq, m_memUsage;
    QVector<Walker> m_threadUsage;
    struct GpuWalkers { Walker temp, usage, power, clock, vram, fan, enc, dec, memCtl; };
    QVector<GpuWalkers> m_gpus;
    struct DiskWalkers { Walker temp, read, write, busy; };
    QVector<DiskWalkers> m_disks;
    struct NetWalkers { Walker down, up, signal; };
    QVector<NetWalkers> m_nets;
    QVector<Walker> m_fans;
    Walker m_batteryCharge, m_batteryPower;
};
