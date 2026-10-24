#include "MockProfile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

struct Context {
    QString *error;
    bool failed = false;
    void fail(const QString &message) {
        if (!failed && error)
            *error = message;
        failed = true;
    }
};

double number(const QJsonObject &o, const QString &key, double fallback) {
    const QJsonValue v = o.value(key);
    return v.isDouble() ? v.toDouble() : fallback;
}

// {"base":..,"min":..,"max":..,"variation":..}; missing fields are derived from `fallback`.
SignalSpec spec(const QJsonObject &parent, const QString &key, SignalSpec fallback, const QString &path, Context &ctx) {
    const QJsonValue value = parent.value(key);
    if (value.isUndefined())
        return fallback;
    if (!value.isObject()) {
        ctx.fail(QStringLiteral("%1.%2 must be an object like {\"base\": 50, \"min\": 30, \"max\": 90}").arg(path, key));
        return fallback;
    }
    const QJsonObject o = value.toObject();
    SignalSpec s;
    s.base = number(o, QStringLiteral("base"), fallback.base);
    s.min = number(o, QStringLiteral("min"), o.contains(QStringLiteral("base")) ? std::min(fallback.min, s.base) : fallback.min);
    s.max = number(o, QStringLiteral("max"), o.contains(QStringLiteral("base")) ? std::max(fallback.max, s.base) : fallback.max);
    s.variation = number(o, QStringLiteral("variation"), fallback.variation);
    if (s.min > s.max || s.base < s.min || s.base > s.max)
        ctx.fail(QStringLiteral("%1.%2: need min <= base <= max (got %3 / %4 / %5)")
                     .arg(path, key).arg(s.min).arg(s.base).arg(s.max));
    if (s.variation < 0)
        ctx.fail(QStringLiteral("%1.%2: variation must not be negative").arg(path, key));
    return s;
}

QString text(const QJsonObject &o, const QString &key, const QString &fallback) {
    const QJsonValue v = o.value(key);
    return v.isString() ? v.toString() : fallback;
}

int integer(const QJsonObject &o, const QString &key, int fallback, int lo, int hi, const QString &path, Context &ctx) {
    if (!o.contains(key))
        return fallback;
    const int v = static_cast<int>(number(o, key, fallback));
    if (v < lo || v > hi)
        ctx.fail(QStringLiteral("%1.%2 must be between %3 and %4").arg(path, key).arg(lo).arg(hi));
    return v;
}

} // namespace

std::optional<MockProfile> MockProfile::parse(const QByteArray &json, QString *error) {
    Context ctx{error};
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = QStringLiteral("not valid JSON: %1").arg(parseError.errorString());
        return std::nullopt;
    }
    const QJsonObject root = doc.object();

    MockProfile p;
    if (root.value(QStringLiteral("system")).isObject())
        p.name = text(root.value(QStringLiteral("system")).toObject(), QStringLiteral("name"), p.name);
    p.name = text(root, QStringLiteral("name"), p.name);
    p.seed = static_cast<quint32>(number(root, QStringLiteral("seed"), 1));

    // ---- cpu (required: every system has one)
    if (!root.value(QStringLiteral("cpu")).isObject()) {
        if (error)
            *error = QStringLiteral("the profile needs a \"cpu\" section");
        return std::nullopt;
    }
    const QJsonObject cpu = root.value(QStringLiteral("cpu")).toObject();
    p.cpu.vendor = text(cpu, QStringLiteral("vendor"), p.cpu.vendor);
    p.cpu.model = text(cpu, QStringLiteral("model"), p.cpu.model);
    p.cpu.cores = integer(cpu, QStringLiteral("cores"), p.cpu.cores, 1, 512, QStringLiteral("cpu"), ctx);
    p.cpu.threads = integer(cpu, QStringLiteral("threads"), p.cpu.cores * 2, 1, 1024, QStringLiteral("cpu"), ctx);
    if (p.cpu.threads < p.cpu.cores)
        ctx.fail(QStringLiteral("cpu.threads (%1) must not be smaller than cpu.cores (%2)").arg(p.cpu.threads).arg(p.cpu.cores));
    p.cpu.sockets = integer(cpu, QStringLiteral("sockets"), 1, 1, 8, QStringLiteral("cpu"), ctx);
    p.cpu.baseMhz = number(cpu, QStringLiteral("base_mhz"), p.cpu.baseMhz);
    p.cpu.maxMhz = number(cpu, QStringLiteral("max_mhz"), p.cpu.maxMhz);
    p.cpu.l1Kb = static_cast<int>(number(cpu, QStringLiteral("l1_kb"), 0));
    p.cpu.l2Kb = static_cast<int>(number(cpu, QStringLiteral("l2_kb"), 0));
    p.cpu.l3Kb = static_cast<int>(number(cpu, QStringLiteral("l3_kb"), 0));
    p.cpu.virtualization = cpu.value(QStringLiteral("virtualization")).toBool(true);
    p.cpu.powerLimitW = number(cpu, QStringLiteral("power_limit_w"), -1);
    const QJsonObject sensors = cpu.value(QStringLiteral("sensors")).toObject();
    const QJsonObject &sensorSource = sensors.isEmpty() ? cpu : sensors; // accept both layouts
    p.cpu.temperature = spec(sensorSource, QStringLiteral("temperature"), p.cpu.temperature, QStringLiteral("cpu"), ctx);
    p.cpu.usage = spec(sensorSource, QStringLiteral("usage"), p.cpu.usage, QStringLiteral("cpu"), ctx);
    SignalSpec freqDefault = SignalSpec::make((p.cpu.baseMhz + p.cpu.maxMhz) / 2, 400, p.cpu.maxMhz, 250);
    p.cpu.frequencyMhz = spec(sensorSource, QStringLiteral("frequency_mhz"), freqDefault, QStringLiteral("cpu"), ctx);

    // ---- memory
    const QJsonObject mem = root.value(QStringLiteral("memory")).toObject();
    p.memory.totalGb = number(mem, QStringLiteral("total_gb"), p.memory.totalGb);
    p.memory.swapGb = number(mem, QStringLiteral("swap_gb"), p.memory.swapGb);
    if (p.memory.totalGb <= 0)
        ctx.fail(QStringLiteral("memory.total_gb must be positive"));
    p.memory.usage = spec(mem, QStringLiteral("usage"), p.memory.usage, QStringLiteral("memory"), ctx);

    // ---- gpus: "gpu" (one) or "gpus" (list)
    QJsonArray gpuArray = root.value(QStringLiteral("gpus")).toArray();
    if (root.value(QStringLiteral("gpu")).isObject())
        gpuArray.prepend(root.value(QStringLiteral("gpu")).toObject());
    for (int i = 0; i < gpuArray.size(); ++i) {
        const QJsonObject g = gpuArray[i].toObject();
        const QString path = QStringLiteral("gpus[%1]").arg(i);
        MockGpu gpu;
        gpu.vendor = text(g, QStringLiteral("vendor"), gpu.vendor);
        gpu.model = text(g, QStringLiteral("model"), gpu.model);
        gpu.vramMb = integer(g, QStringLiteral("vram_mb"), gpu.vramMb, 0, 1 << 20, path, ctx);
        gpu.driver = text(g, QStringLiteral("driver"), gpu.driver);
        gpu.pciId = text(g, QStringLiteral("pci_id"), gpu.pciId);
        gpu.maxClockMhz = integer(g, QStringLiteral("max_clock_mhz"), gpu.maxClockMhz, 100, 10000, path, ctx);
        gpu.hasFan = g.value(QStringLiteral("has_fan")).toBool(true);
        const QJsonObject gs = g.value(QStringLiteral("sensors")).toObject();
        const QJsonObject &src = gs.isEmpty() ? g : gs;
        gpu.temperature = spec(src, QStringLiteral("temperature"), gpu.temperature, path, ctx);
        gpu.usage = spec(src, QStringLiteral("usage"), gpu.usage, path, ctx);
        gpu.powerW = spec(src, QStringLiteral("power_w"), gpu.powerW, path, ctx);
        gpu.clockMhz = spec(src, QStringLiteral("clock_mhz"), SignalSpec::make(gpu.maxClockMhz * 0.6, 300, gpu.maxClockMhz, 200), path, ctx);
        gpu.vramUsage = spec(src, QStringLiteral("vram_usage"), gpu.vramUsage, path, ctx);
        gpu.fanPercent = spec(src, QStringLiteral("fan_percent"), gpu.fanPercent, path, ctx);
        p.gpus.push_back(gpu);
    }

    // ---- disks, networks, fans, battery
    const QJsonArray disks = root.value(QStringLiteral("disks")).toArray();
    for (int i = 0; i < disks.size(); ++i) {
        const QJsonObject d = disks[i].toObject();
        const QString path = QStringLiteral("disks[%1]").arg(i);
        MockDisk disk;
        disk.name = text(d, QStringLiteral("name"), QStringLiteral("nvme%1n1").arg(i));
        disk.model = text(d, QStringLiteral("model"), disk.model);
        disk.sizeGb = number(d, QStringLiteral("size_gb"), disk.sizeGb);
        disk.usedFraction = number(d, QStringLiteral("used_fraction"), disk.usedFraction);
        disk.rotational = d.value(QStringLiteral("rotational")).toBool(false);
        disk.temperature = spec(d, QStringLiteral("temperature"), disk.temperature, path, ctx);
        disk.readMbps = spec(d, QStringLiteral("read_mbps"), disk.readMbps, path, ctx);
        disk.writeMbps = spec(d, QStringLiteral("write_mbps"), disk.writeMbps, path, ctx);
        disk.busy = spec(d, QStringLiteral("busy"), disk.busy, path, ctx);
        p.disks.push_back(disk);
    }
    const QJsonArray nets = root.value(QStringLiteral("networks")).toArray();
    for (int i = 0; i < nets.size(); ++i) {
        const QJsonObject n = nets[i].toObject();
        const QString path = QStringLiteral("networks[%1]").arg(i);
        MockNet net;
        net.name = text(n, QStringLiteral("name"), QStringLiteral("eth%1").arg(i));
        net.kind = text(n, QStringLiteral("kind"), net.kind);
        if (net.kind != QLatin1String("Ethernet") && net.kind != QLatin1String("Wi-Fi") && net.kind != QLatin1String("Virtual"))
            ctx.fail(QStringLiteral("%1.kind must be \"Ethernet\", \"Wi-Fi\" or \"Virtual\"").arg(path));
        net.ssid = text(n, QStringLiteral("ssid"), net.ssid);
        net.speedMbps = static_cast<int>(number(n, QStringLiteral("speed_mbps"), net.speedMbps));
        net.vendor = text(n, QStringLiteral("vendor"), net.vendor);
        net.model = text(n, QStringLiteral("model"), net.model);
        net.downMbps = spec(n, QStringLiteral("down_mbps"), net.downMbps, path, ctx);
        net.upMbps = spec(n, QStringLiteral("up_mbps"), net.upMbps, path, ctx);
        net.signalDbm = spec(n, QStringLiteral("signal_dbm"), net.signalDbm, path, ctx);
        p.nets.push_back(net);
    }
    const QJsonArray fans = root.value(QStringLiteral("fans")).toArray();
    for (int i = 0; i < fans.size(); ++i) {
        const QJsonObject f = fans[i].toObject();
        MockFan fan;
        fan.name = text(f, QStringLiteral("name"), QStringLiteral("Fan %1").arg(i + 1));
        fan.rpm = spec(f, QStringLiteral("rpm"), fan.rpm, QStringLiteral("fans[%1]").arg(i), ctx);
        p.fans.push_back(fan);
    }
    if (root.value(QStringLiteral("battery")).isObject()) {
        const QJsonObject b = root.value(QStringLiteral("battery")).toObject();
        p.battery.present = true;
        p.battery.designWh = number(b, QStringLiteral("design_wh"), p.battery.designWh);
        p.battery.fullWh = number(b, QStringLiteral("full_wh"), p.battery.fullWh);
        p.battery.cycles = static_cast<int>(number(b, QStringLiteral("cycles"), p.battery.cycles));
        p.battery.status = text(b, QStringLiteral("status"), p.battery.status);
        p.battery.charge = spec(b, QStringLiteral("charge"), p.battery.charge, QStringLiteral("battery"), ctx);
        p.battery.powerW = spec(b, QStringLiteral("power_w"), p.battery.powerW, QStringLiteral("battery"), ctx);
    }

    // ---- scenarios
    const QJsonArray scenarios = root.value(QStringLiteral("scenarios")).toArray();
    for (int i = 0; i < scenarios.size(); ++i) {
        const QJsonObject s = scenarios[i].toObject();
        const QString path = QStringLiteral("scenarios[%1]").arg(i);
        MockScenario sc;
        sc.type = text(s, QStringLiteral("type"), QString());
        sc.target = text(s, QStringLiteral("target"), QStringLiteral("cpu"));
        sc.startS = number(s, QStringLiteral("start_s"), 0);
        sc.durationS = number(s, QStringLiteral("duration_s"), 10);
        sc.peak = number(s, QStringLiteral("peak"), 100);
        sc.rpm = number(s, QStringLiteral("rpm"), 0);
        static const QStringList known = {QStringLiteral("overheat"), QStringLiteral("gpu_loss"),
                                          QStringLiteral("sensor_dropout"), QStringLiteral("fan_change")};
        if (!known.contains(sc.type))
            ctx.fail(QStringLiteral("%1.type \"%2\" is unknown (use overheat, gpu_loss, sensor_dropout or fan_change)").arg(path, sc.type));
        if (sc.startS < 0 || sc.durationS <= 0)
            ctx.fail(QStringLiteral("%1: start_s must be >= 0 and duration_s > 0").arg(path));
        if (sc.type == QLatin1String("fan_change") && sc.rpm <= 0)
            ctx.fail(QStringLiteral("%1: fan_change needs a positive \"rpm\"").arg(path));
        if (sc.type == QLatin1String("fan_change") && !sc.target.startsWith(QLatin1String("fan:")))
            ctx.fail(QStringLiteral("%1: fan_change needs a target like \"fan:CPU Fan\"").arg(path));
        p.scenarios.push_back(sc);
    }

    if (ctx.failed)
        return std::nullopt;
    return p;
}

std::optional<MockProfile> MockProfile::load(const QString &path, QString *error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("cannot read %1: %2").arg(path, f.errorString());
        return std::nullopt;
    }
    return parse(f.readAll(), error);
}
