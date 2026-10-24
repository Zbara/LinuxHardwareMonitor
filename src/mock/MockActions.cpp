#include "MockActions.h"

namespace MockActions {

QVector<MockAction> build(const MockProfile &profile) {
    QVector<MockAction> out;
    auto add = [&](const QString &kind, const QString &name, const QString &type, const QString &target, double duration,
                   double peak = 100, double rpm = 0) {
        MockAction a;
        a.kind = kind;
        a.targetName = name;
        a.scenario.type = type;
        a.scenario.target = target;
        a.scenario.durationS = duration;
        a.scenario.peak = peak;
        a.scenario.rpm = rpm;
        out.push_back(a);
    };

    add(QStringLiteral("overheat_cpu"), profile.cpu.model, QStringLiteral("overheat"), QStringLiteral("cpu"), 30, 100);
    add(QStringLiteral("cpu_sensor_dropout"), profile.cpu.model, QStringLiteral("sensor_dropout"), QStringLiteral("cpu"), 20);
    for (int i = 0; i < profile.gpus.size(); ++i) {
        const QString target = QStringLiteral("gpu%1").arg(i);
        add(QStringLiteral("overheat_gpu"), profile.gpus[i].model, QStringLiteral("overheat"), target, 30, 100);
        add(QStringLiteral("gpu_loss"), profile.gpus[i].model, QStringLiteral("gpu_loss"), target, 20);
    }
    for (const MockFan &fan : profile.fans) {
        const QString target = QStringLiteral("fan:") + fan.name;
        add(QStringLiteral("fan_stop"), fan.name, QStringLiteral("sensor_dropout"), target, 20);
        add(QStringLiteral("fan_boost"), fan.name, QStringLiteral("fan_change"), target, 20, 0, fan.rpm.max);
    }
    for (const MockDisk &disk : profile.disks)
        add(QStringLiteral("disk_dropout"), disk.name, QStringLiteral("sensor_dropout"), QStringLiteral("disk:") + disk.name, 20);
    return out;
}

} // namespace MockActions
