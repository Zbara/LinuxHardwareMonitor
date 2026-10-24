#include "AppController.h"

#include "core/ProcessControl.h"
#include "mock/MockFacts.h"
#include "mock/MockProvider.h"
#include "providers/BatteryProvider.h"
#include "providers/DrmProvider.h"
#include "providers/HwmonProvider.h"
#include "providers/IoProvider.h"
#include "providers/NvidiaProvider.h"
#include "providers/SystemProvider.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QDir>
#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QVariantMap>

#include <cerrno>
#include <csignal>
#include <sys/resource.h>
#include <cstring>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr int kDefaultIntervalMs = 1000;
constexpr int kDefaultHistorySeconds = 300;

QVariantMap highlight(const QString &title, const QString &unit,
                      const SensorListModel::Row *best, int available, int total) {
    QVariantMap m;
    m["title"] = title;
    m["unit"] = unit;
    m["available"] = best != nullptr;
    m["value"] = best ? QVariant(best->reading.value) : QVariant();
    m["sensorId"] = best ? best->reading.id : QString();
    m["sensorName"] = best ? best->reading.name : QString();
    m["availableCount"] = available;
    m["totalCount"] = total;
    return m;
}

} // namespace

AppController::AppController(AppOptions options, QObject *parent)
    : QObject(parent),
      m_options(std::move(options)),
      m_settings(QStringLiteral("LinuxHardwareMonitor"), QStringLiteral("LinuxHardwareMonitor")) {
    const int interval = m_settings.value("intervalMs", kDefaultIntervalMs).toInt();
    m_intervalMs = (interval == 500 || interval == 1000 || interval == 2000 || interval == 5000)
                       ? interval
                       : kDefaultIntervalMs;
    m_historySeconds = std::clamp(m_settings.value("historySeconds", kDefaultHistorySeconds).toInt(),
                                  30, 3600);
    m_dark = m_settings.value("darkTheme", true).toBool();

    m_trayEnabled = m_settings.value("trayEnabled", false).toBool();
    m_trayStyle = m_settings.value("trayStyle", "app").toString() == QLatin1String("temperature") ? QStringLiteral("temperature") : QStringLiteral("app");
    m_languageSetting = m_settings.value("language", QString()).toString();
    m_localization.setLanguage(m_languageSetting); // before any UI text is produced
    m_manager.setHistorySeconds(m_historySeconds);
    m_overheat.setThreshold(m_settings.value("overheatThreshold", 90).toInt());
    m_simClock.start();
    if (qEnvironmentVariableIsSet("HWMON_PROFILE")) { // prints per-second cost of the C++ side of each refresh
        auto *t = new QTimer(this);
        connect(t, &QTimer::timeout, this, &AppController::printProfile);
        t->start(5000);
    }

    // History across restarts: only for the real machine (simulated and fixture data would be misleading).
    if (!m_options.mock && !m_options.fixtureMode()) {
        QFile file(historyFilePath());
        if (file.open(QIODevice::ReadOnly))
            m_manager.importHistory(file.readAll(), QDateTime::currentMSecsSinceEpoch());
        connect(qApp, &QCoreApplication::aboutToQuit, this, &AppController::saveHistory);
        auto *autosave = new QTimer(this); // a crash or kill should not lose everything
        connect(autosave, &QTimer::timeout, this, &AppController::saveHistory);
        autosave->start(5 * 60 * 1000);
    }
    connect(&m_dmiReader, &DmiMemoryReader::finished, this, [this](const MemoryInfo &info) { applyMemoryInfo(info); });
    connect(&m_dmiReader, &DmiMemoryReader::failed, this, [this](DmiMemoryReader::Failure, const QString &message) {
        m_memoryModules = {{QStringLiteral("state"), QStringLiteral("error")}, {QStringLiteral("error"), message}};
        emit memoryModulesChanged();
    });
    if (m_options.mock) {
        // Simulation mode: one provider stands in for all the hardware ones.
        auto mock = std::make_unique<MockProvider>(*m_options.mock);
        mock->setSpeed(m_options.mockSpeed);
        m_mockProvider = mock.get();
        m_mockActions = MockActions::build(*m_options.mock);
        m_manager.addProvider(std::move(mock));
    } else {
        // Empty roots mean the real /sys and /proc; fixture mode points them at test trees.
        auto orDefault = [](const QString &path, const QString &fallback) { return path.isEmpty() ? fallback : path; };
        const QString proc = orDefault(m_options.procRoot, QStringLiteral("/proc"));
        const QString cpu = orDefault(m_options.cpuRoot, QStringLiteral("/sys/devices/system/cpu"));
        m_manager.addProvider(std::make_unique<HwmonProvider>(orDefault(m_options.hwmonRoot, QStringLiteral("/sys/class/hwmon"))));
        m_manager.addProvider(std::make_unique<SystemProvider>(proc, cpu));
        m_manager.addProvider(std::make_unique<IoProvider>(proc));
        m_manager.addProvider(std::make_unique<BatteryProvider>(orDefault(m_options.powerSupplyRoot, QStringLiteral("/sys/class/power_supply"))));
        m_manager.addProvider(std::make_unique<DrmProvider>(orDefault(m_options.drmRoot, QStringLiteral("/sys/class/drm"))));
        if (!m_options.fixtureMode()) // the NVIDIA driver is the real machine's, not the fixture's
            m_manager.addProvider(std::make_unique<NvidiaProvider>());
    }
    connect(&m_manager, &SensorManager::updated, this, &AppController::rebuildSummaries);
    connect(&m_wifiMonitor, &WifiInfoMonitor::updated, this, [this](const WifiInfoMap &info) { m_wifi = info; });
    m_serviceProxy.setSourceModel(&m_serviceModel);
    m_sensorProxy.setSourceModel(m_manager.model());
    connect(&m_serviceManager, &ServiceManager::servicesReady, this, [this](const QVector<ServiceInfo> &list) {
        m_serviceModel.update(list);
        if (m_servicesListFailed) { // the list works again, drop the stale error
            m_servicesListFailed = false;
            m_servicesMessage.clear();
            emit servicesMessageChanged();
        }
        emit serviceSummaryChanged();
    });
    connect(&m_serviceManager, &ServiceManager::failed, this, [this](const QString &error) {
        m_servicesListFailed = true;
        m_servicesMessage = tr("systemd is not available: %1").arg(error);
        emit servicesMessageChanged();
    });
    connect(&m_serviceManager, &ServiceManager::actionFinished, this,
            [this](const QString &name, const QString &error) {
                m_servicesMessage = error.isEmpty() ? QString() : tr("%1: %2").arg(name, error);
                emit servicesMessageChanged();
            });
    m_processProxy.setSourceModel(&m_processModel);
    m_processProxy.sortBy(QStringLiteral("name")); // initialises the sort role...
    m_processProxy.sortBy(QStringLiteral("cpu"));  // ...then sorts by CPU, descending
    connect(&m_processMonitor, &ProcessMonitor::scanned, this,
            [this](const QVector<ProcessInfo> &list) {
                m_processModel.update(list);
                m_processCount = static_cast<int>(list.size());
                m_threadCount = 0;
                m_processCpu = 0.0;
                for (const ProcessInfo &p : list) {
                    m_threadCount += p.threads;
                    m_processCpu += p.cpuPercent;
                }
                emit processSummaryChanged();
            });
    m_processMonitor.setInterval(m_intervalMs);
    m_manager.start(m_intervalMs);
}

void AppController::setIntervalMs(int ms) {
    if (ms == m_intervalMs)
        return;
    m_intervalMs = ms;
    m_settings.setValue("intervalMs", ms);
    m_manager.setInterval(ms);
    m_processMonitor.setInterval(ms);
    emit intervalMsChanged();
}

void AppController::setOverheatThreshold(int celsius) {
    celsius = std::clamp(celsius, 0, 150);
    if (celsius == m_overheat.threshold())
        return;
    m_overheat.setThreshold(celsius);
    m_settings.setValue("overheatThreshold", celsius);
    emit overheatThresholdChanged();
}

int AppController::sensorCount() const {
    int n = 0;
    for (const auto &row : m_manager.model()->rows())
        n += row.reading.type != SensorType::Text; // text rows are static facts, not sensors
    return n;
}

int AppController::sensorsAvailable() const {
    int n = 0;
    for (const auto &row : m_manager.model()->rows())
        n += row.reading.type != SensorType::Text && row.reading.available;
    return n;
}

void AppController::setServicesActive(bool active) {
    if (active == m_serviceManager.active())
        return;
    m_serviceManager.setActive(active);
    emit servicesActiveChanged();
}

void AppController::serviceAction(const QString &name, const QString &action) {
    m_servicesMessage.clear();
    emit servicesMessageChanged();
    m_serviceManager.requestAction(name, action);
}

void AppController::setProcessesActive(bool active) {
    if (active == m_processMonitor.active())
        return;
    m_processMonitor.setActive(active);
    emit processesActiveChanged();
}

namespace {

QString describe(int err) {
    switch (err) {
    case 0:      return {};
    case EINVAL: return AppController::tr("Refusing to signal this process");
    case EPERM:  return AppController::tr("Permission denied (the process belongs to another user)");
    case ESRCH:  return AppController::tr("The process no longer exists");
    default:     return QString::fromLocal8Bit(std::strerror(err));
    }
}

} // namespace

QString AppController::setProcessPriority(int pid, int nice) {
    const int err = ProcessControl::setPriority(pid, nice);
    // Raising priority (negative nice) or touching another user's process needs privileges.
    return err == EACCES || err == EPERM
               ? tr("Permission denied (raising priority or changing another user's process needs administrator rights)")
               : describe(err);
}

QVariantList AppController::processAffinity(int pid) const {
    QVariantList out;
    for (int cpu : ProcessControl::affinity(pid))
        out << cpu;
    return out;
}

QString AppController::setProcessAffinity(int pid, const QVariantList &cpus) {
    QVector<int> list;
    for (const QVariant &c : cpus)
        list.push_back(c.toInt());
    const int err = ProcessControl::setAffinity(pid, list);
    return err == EPERM || err == EACCES
               ? tr("Permission denied (raising priority or changing another user's process needs administrator rights)")
               : describe(err);
}

QString AppController::endProcess(int pid, bool force) {
    return describe(ProcessControl::sendSignal(pid, force));
}

QString AppController::endProcesses(const QVariantList &pids, bool force) {
    int firstError = 0;
    for (const QVariant &pid : pids) {
        const int err = ProcessControl::sendSignal(pid.toInt(), force);
        // A member that exited on its own is not a failure when ending a group.
        if (err != 0 && err != ESRCH && firstError == 0)
            firstError = err;
    }
    return describe(firstError);
}

QString AppController::exportHistory() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();
    const QString path = dir + QStringLiteral("/hwmonitor-history-")
                         + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))
                         + QStringLiteral(".csv");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    m_manager.exportCsv(file);
    return file.error() == QFileDevice::NoError ? path : QString();
}

QVariantList AppController::simulationActions() const {
    QVariantList out;
    for (const MockAction &a : m_mockActions) {
        QVariantMap m;
        m["kind"] = a.kind;
        m["target"] = a.targetName;
        out << m;
    }
    return out;
}

void AppController::runSimulationAction(int index) {
    if (!m_mockProvider || index < 0 || index >= m_mockActions.size())
        return;
    m_mockProvider->inject(m_mockActions[index].scenario);
}

void AppController::clearSimulationScenarios() {
    if (m_mockProvider)
        m_mockProvider->clearInjected();
}

void AppController::printProfile() {
    static qint64 lastSummary = 0, lastApply = 0;
    const qint64 s = m_summaryNs, a = m_manager.applyNanoseconds();
    qInfo("profile: rebuildSummaries %.2f ms/s, applyReadings %.2f ms/s (over 5 s)", double(s - lastSummary) / 5e6, double(a - lastApply) / 5e6);
    lastSummary = s;
    lastApply = a;
}

QString AppController::historyFilePath() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/history.bin");
}

void AppController::saveHistory() {
    const QString path = historyFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path); // atomic: a half-written file never replaces a good one
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(m_manager.exportHistory(QDateTime::currentMSecsSinceEpoch()));
    file.commit();
}

void AppController::applyMemoryInfo(const MemoryInfo &info) {
    QVariantMap m;
    m["state"] = QStringLiteral("ready");
    m["slotsTotal"] = info.slotsTotal;
    m["slotsUsed"] = info.slotsUsed();
    m["errorCorrection"] = info.errorCorrection;
    qint64 totalMb = 0;
    QVariantList modules;
    for (const MemoryModule &mod : info.modules) {
        totalMb += mod.sizeMb;
        QVariantMap mm;
        mm["locator"] = mod.locator;
        mm["sizeMb"] = mod.sizeMb;
        mm["type"] = mod.type;
        mm["speedMts"] = mod.speedMts;
        mm["formFactor"] = mod.formFactor;
        mm["manufacturer"] = mod.manufacturer;
        mm["partNumber"] = mod.partNumber;
        modules << mm;
    }
    m["totalMb"] = totalMb;
    m["modules"] = modules;
    // The module properties are normally identical; show the first installed module's values.
    if (!info.modules.isEmpty()) {
        const MemoryModule &first = info.modules.first();
        m["speedMts"] = first.speedMts;
        m["formFactor"] = first.formFactor;
        m["type"] = first.type;
        m["typeDetail"] = first.typeDetail;
    }
    m_memoryModules = m;
    emit memoryModulesChanged();
}

void AppController::loadMemoryModules() {
    if (m_dmiReader.running())
        return;
    if (m_options.mock) { // simulation: no privileged call, the profile describes the modules
        applyMemoryInfo(MockFacts::memory(*m_options.mock));
        return;
    }
    m_memoryModules = {{QStringLiteral("state"), QStringLiteral("loading")}};
    emit memoryModulesChanged();
    m_dmiReader.start();
}

void AppController::setLanguage(const QString &code) {
    if (code == m_languageSetting && m_localization.currentLanguage() == Localization::resolve(code, QLocale::system().uiLanguages()))
        return;
    m_languageSetting = code;
    m_settings.setValue("language", code);
    m_localization.setLanguage(code);
    rebuildSummaries(); // C++-side titles are produced with tr(), refresh them now
    emit languageChanged();
}

QVariantList AppController::languages() const {
    QVariantList out;
    QVariantMap system;
    system["code"] = QString();
    system["name"] = tr("System default");
    out << system;
    for (const auto &l : Localization::languages()) {
        QVariantMap m;
        m["code"] = l.code;
        m["name"] = l.nativeName; // always shown in its own language
        out << m;
    }
    return out;
}

void AppController::setTrayEnabled(bool enabled) {
    if (enabled == m_trayEnabled)
        return;
    m_trayEnabled = enabled;
    m_settings.setValue("trayEnabled", enabled);
    emit trayEnabledChanged();
}

void AppController::setTrayStyle(const QString &style) {
    const QString normalized = style == QLatin1String("temperature") ? style : QStringLiteral("app");
    if (normalized == m_trayStyle)
        return;
    m_trayStyle = normalized;
    m_settings.setValue("trayStyle", normalized);
    emit trayStyleChanged();
}

void AppController::setTrayAvailable(bool available) {
    if (available == m_trayAvailable)
        return;
    m_trayAvailable = available;
    emit trayAvailableChanged();
}

void AppController::quit() {
    QCoreApplication::quit();
}

void AppController::setAutostart(bool enabled) {
    if (enabled == m_autostart.isEnabled())
        return;
    m_autostart.setEnabled(enabled);
    emit autostartChanged(); // also resets the switch if writing the file failed
}

QString AppController::setStartupEnabled(const QString &id, bool enabled) {
    return m_startup.setEnabled(id, enabled) ? QString() : tr("Could not change this startup entry");
}

QVariant AppController::uiSetting(const QString &key, const QVariant &fallback) const {
    return m_settings.value(QStringLiteral("ui/") + key, fallback);
}

void AppController::setUiSetting(const QString &key, const QVariant &value) {
    m_settings.setValue(QStringLiteral("ui/") + key, value);
}

void AppController::copyToClipboard(const QString &text) {
    if (auto *clipboard = QGuiApplication::clipboard())
        clipboard->setText(text);
}

void AppController::setDarkTheme(bool dark) {
    if (dark == m_dark)
        return;
    m_dark = dark;
    m_settings.setValue("darkTheme", dark);
    emit darkThemeChanged();
}

void AppController::setHistorySeconds(int seconds) {
    seconds = std::clamp(seconds, 30, 3600);
    if (seconds == m_historySeconds)
        return;
    m_historySeconds = seconds;
    m_settings.setValue("historySeconds", seconds);
    m_manager.setHistorySeconds(seconds);
    emit historySecondsChanged();
}

QVariantList AppController::history(const QString &sensorId) const {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QVariantList out;
    const auto samples = m_manager.history(sensorId);
    out.reserve(samples.size());
    for (const auto &s : samples) {
        out.push_back(QPointF((s.timestampMs - now) / 1000.0,
                              s.valid ? s.value : std::numeric_limits<double>::quiet_NaN()));
    }
    return out;
}

namespace {

QVariantMap sensorRef(const SensorListModel::Row *row) {
    QVariantMap m;
    m["id"] = row ? row->reading.id : QString();
    m["unit"] = row ? row->reading.unit : QString();
    // Highest value seen this session (a "peak"), when there is one.
    m["max"] = row && row->hasStats ? QVariant(row->max) : QVariant();
    m["available"] = row && row->reading.available;
    m["value"] = row && row->reading.available ? QVariant(row->reading.value) : QVariant();
    return m;
}

} // namespace

// Per-disk, per-interface and per-GPU entries for the Performance page.
void AppController::fillHardwareLists(const QHash<QString, const SensorListModel::Row *> &byId) {
    auto find = [&](const QString &id) { return byId.value(id, nullptr); };

    // Mounted-filesystem usage changes slowly and listing mounts is comparatively expensive.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_volumesStamp > 10000) {
        m_volumes = HardwareInfo::mountedVolumes();
        m_volumesStamp = now;
    }
    if (now - m_ipStamp > 5000) {
        m_ipCache.clear();
        for (const QNetworkInterface &ni : QNetworkInterface::allInterfaces()) {
            QStringList addrs;
            for (const QNetworkAddressEntry &e : ni.addressEntries())
                addrs << e.ip().toString().section(QLatin1Char('%'), 0, 0) + QLatin1Char('/') + QString::number(e.prefixLength());
            m_ipCache.insert(ni.name(), addrs);
        }
        m_ipStamp = now;
    }

    // NVMe drives expose their temperature through a hwmon device whose parent
    // is the controller ("nvme0"); the block device is "nvme0n1".
    auto diskTemperature = [&](const QString &disk) -> const SensorListModel::Row * {
        const SensorListModel::Row *best = nullptr;
        for (const auto &row : m_manager.model()->rows()) {
            const SensorReading &r = row.reading;
            if (r.category != DeviceCategory::Storage || r.type != SensorType::Temperature)
                continue;
            const int at = r.deviceId.indexOf(QLatin1Char('@'));
            const QString parent = at >= 0 ? r.deviceId.mid(at + 1).section(QLatin1Char('#'), 0, 0) : QString();
            if (parent.isEmpty() || !disk.startsWith(parent))
                continue;
            if (!best || r.name == QLatin1String("Composite"))
                best = &row;
        }
        return best;
    };

    QStringList diskNames, netNames;
    for (auto it = byId.constBegin(); it != byId.constEnd(); ++it) {
        const QString &id = it.key();
        if (id.startsWith(QLatin1String("disk/")) && id.endsWith(QLatin1String("_busy")))
            diskNames << id.mid(5, id.size() - 5 - 5);
        else if (id.startsWith(QLatin1String("net/")) && id.endsWith(QLatin1String("_read"))
                 && id != QLatin1String("net/total_read"))
            netNames << id.mid(4, id.size() - 4 - 5);
    }
    diskNames.sort();
    netNames.sort();

    QVariantList disks;
    for (const QString &n : std::as_const(diskNames)) {
        QVariantMap &facts = m_staticInfo[QStringLiteral("disk:") + n];
        if (facts.isEmpty() && m_options.mock) {
            facts = MockFacts::disk(*m_options.mock, n);
        } else if (facts.isEmpty()) {
            facts["model"] = SystemInfo::diskModel(n);
            facts["sizeBytes"] = SystemInfo::diskSizeBytes(n);
            facts["rotational"] = SystemInfo::diskIsRotational(n);
            const auto details = HardwareInfo::disk(n);
            facts["serial"] = details.serial;
            facts["firmware"] = details.firmware;
            facts["interface"] = details.interfaceName;
            facts["blockSize"] = details.logicalBlockSize;
        }
        QVariantMap d = facts;
        d["name"] = n;
        d["read"] = sensorRef(find("disk/" + n + "_read"));
        d["write"] = sensorRef(find("disk/" + n + "_write"));
        d["busy"] = sensorRef(find("disk/" + n + "_busy"));
        d["temp"] = sensorRef(diskTemperature(n));
        if (!m_options.mock) {
            // /sys/block/<n>/stat: field 3 = sectors read, field 7 = sectors written (512 bytes each).
            QFile stat(QStringLiteral("/sys/block/%1/stat").arg(n));
            if (stat.open(QIODevice::ReadOnly)) {
                const QList<QByteArray> f = stat.readAll().simplified().split(' ');
                if (f.size() > 6) {
                    d["readTotalBytes"] = double(f[2].toULongLong()) * 512.0;
                    d["writeTotalBytes"] = double(f[6].toULongLong()) * 512.0;
                }
            }
        }
        if (!m_options.mock) {
            const auto usage = HardwareInfo::diskUsage(n, m_volumes);
            d["usedBytes"] = usage.usedBytes >= 0 ? QVariant(double(usage.usedBytes)) : QVariant();
            d["mountedBytes"] = usage.totalBytes >= 0 ? QVariant(double(usage.totalBytes)) : QVariant();
        }
        disks << d;
    }

    QVariantList nets;
    for (const QString &n : std::as_const(netNames)) {
        QVariantMap &facts = m_staticInfo[QStringLiteral("net:") + n];
        if (facts.isEmpty() && m_options.mock)
            facts = MockFacts::net(*m_options.mock, n);
        else if (facts.isEmpty()) {
            facts["kind"] = SystemInfo::netKind(n);
            const auto nd = HardwareInfo::net(n);
            const auto names = HardwareInfo::pciNames(nd.vendorId, nd.deviceId);
            facts["mac"] = nd.mac;
            facts["driver"] = nd.driver;
            facts["busAddress"] = nd.busAddress;
            facts["vendor"] = names.first;
            facts["model"] = names.second;
        }
        QVariantMap d = facts;
        d["name"] = n;
        if (!m_options.mock) {
            d["state"] = SystemInfo::netState(n);          // can change at runtime
            d["speedMbps"] = SystemInfo::netSpeedMbps(n);  // so can the negotiated speed
        }
        d["down"] = sensorRef(find("net/" + n + "_read"));
        d["up"] = sensorRef(find("net/" + n + "_write"));
        d["signal"] = sensorRef(find("net/" + n + "_signal"));
        if (!m_options.mock) {
            const auto nd = HardwareInfo::net(n); // counters and MTU/duplex change at runtime
            auto num = [](qint64 v) { return v >= 0 ? QVariant(double(v)) : QVariant(); };
            d["mtu"] = nd.mtu >= 0 ? QVariant(nd.mtu) : QVariant();
            d["duplex"] = nd.duplex;
            d["rxBytes"] = num(nd.rxBytes);
            d["txBytes"] = num(nd.txBytes);
            d["rxPackets"] = num(nd.rxPackets);
            d["txPackets"] = num(nd.txPackets);
            d["errors"] = num(nd.errors);
            d["dropped"] = num(nd.dropped);
            QString ipv4, ipv6;
            for (const QString &a : m_ipCache.value(n)) {
                if (a.contains(QLatin1Char(':')) && ipv6.isEmpty() && !a.startsWith(QLatin1String("fe80")))
                    ipv6 = a;
                else if (a.contains(QLatin1Char('.')) && ipv4.isEmpty())
                    ipv4 = a;
            }
            if (ipv6.isEmpty()) // fall back to link-local when that is all there is
                for (const QString &a : m_ipCache.value(n))
                    if (a.contains(QLatin1Char(':'))) { ipv6 = a; break; }
            d["ipv4"] = ipv4;
            d["ipv6"] = ipv6;
            if (!m_options.mock && m_wifi.contains(n)) {
                const WifiInfo &w = m_wifi[n];
                QVariantMap wm;
                wm["connected"] = w.connected;
                wm["ssid"] = w.ssid;
                wm["bssid"] = w.bssid;
                wm["frequencyMhz"] = w.frequencyMhz;
                wm["channel"] = w.channel;
                wm["band"] = w.band;
                wm["bitrateMbps"] = w.bitrateMbps;
                wm["security"] = w.security;
                wm["mode"] = w.mode;
                d["wifi"] = wm;
            }
        }
        nets << d;
    }

    // GPUs: group the sensors of each GPU device by type.
    struct Gpu {
        QString name;
        const SensorListModel::Row *util = nullptr, *temp = nullptr, *mem = nullptr,
                                   *power = nullptr, *clock = nullptr, *fan = nullptr, *memTotal = nullptr,
                                   *utilMem = nullptr, *enc = nullptr, *dec = nullptr, *clockMax = nullptr,
                                   *powerLimit = nullptr;
        QVariantMap texts;
    };
    QVector<Gpu> gpus;
    QHash<QString, int> gpuIndex;
    for (const auto &row : m_manager.model()->rows()) {
        const SensorReading &r = row.reading;
        if (r.category != DeviceCategory::Gpu)
            continue;
        int gi = gpuIndex.value(r.deviceId, -1);
        if (gi < 0) {
            gi = static_cast<int>(gpus.size());
            gpuIndex.insert(r.deviceId, gi);
            gpus.push_back(Gpu{r.deviceName});
        }
        Gpu &g = gpus[gi];
        if (r.type == SensorType::Text) {
            g.texts[r.id.section(QLatin1Char('/'), -1)] = r.text;
            continue;
        }
        if (r.id.endsWith(QLatin1String("/util_mem"))) { g.utilMem = &row; continue; }
        if (r.id.endsWith(QLatin1String("/enc"))) { g.enc = &row; continue; }
        if (r.id.endsWith(QLatin1String("/dec"))) { g.dec = &row; continue; }
        if (r.id.endsWith(QLatin1String("/clock_max"))) { g.clockMax = &row; continue; }
        if (r.id.endsWith(QLatin1String("/power_limit"))) { g.powerLimit = &row; continue; }
        switch (r.type) {
        case SensorType::Utilization:  g.util = g.util ? g.util : &row; break;
        case SensorType::Temperature:  g.temp = g.temp ? g.temp : &row; break;
        case SensorType::Memory:
            if (r.id.endsWith(QLatin1String("/mem_total")))
                g.memTotal = &row;
            else
                g.mem = g.mem ? g.mem : &row;
            break;
        case SensorType::Power:        g.power = g.power ? g.power : &row; break;
        case SensorType::Frequency:    g.clock = g.clock ? g.clock : &row; break;
        case SensorType::FanSpeed:     g.fan = g.fan ? g.fan : &row; break;
        default: break;
        }
    }
    QVariantList gpuList;
    for (const Gpu &g : std::as_const(gpus)) {
        QVariantMap m;
        m["name"] = g.name;
        m["util"] = sensorRef(g.util);
        m["temp"] = sensorRef(g.temp);
        m["mem"] = sensorRef(g.mem);
        m["memTotal"] = sensorRef(g.memTotal);
        m["power"] = sensorRef(g.power);
        m["clock"] = sensorRef(g.clock);
        m["fan"] = sensorRef(g.fan);
        m["utilMem"] = sensorRef(g.utilMem);
        m["enc"] = sensorRef(g.enc);
        m["dec"] = sensorRef(g.dec);
        m["clockMax"] = sensorRef(g.clockMax);
        m["powerLimit"] = sensorRef(g.powerLimit);
        m["info"] = g.texts;
        gpuList << m;
    }

    // Batteries: group the "power:<name>/..." sensors by device.
    QVariantList batteries;
    {
        QHash<QString, QVariantMap> byDevice;
        QStringList order;
        for (const auto &row : m_manager.model()->rows()) {
            const SensorReading &r = row.reading;
            if (!r.deviceId.startsWith(QLatin1String("power:")))
                continue;
            if (!byDevice.contains(r.deviceId)) {
                order << r.deviceId;
                byDevice[r.deviceId]["name"] = r.deviceName;
            }
            const QString key = r.id.section(QLatin1Char('/'), -1);
            if (r.type == SensorType::Text) {
                QVariantMap texts = byDevice[r.deviceId]["info"].toMap();
                texts[key] = r.text;
                byDevice[r.deviceId]["info"] = texts;
            } else {
                byDevice[r.deviceId][key] = sensorRef(&row);
            }
        }
        for (const QString &id : std::as_const(order))
            batteries << byDevice[id];
    }

    m_details["batteries"] = batteries;
    m_details["disks"] = disks;
    m_details["nets"] = nets;
    m_details["gpus"] = gpuList;
}

void AppController::rebuildSummaries() {
    static const bool profiling = qEnvironmentVariableIsSet("HWMON_PROFILE");
    QElapsedTimer profile;
    if (profiling)
        profile.start();
    struct Report {
        QElapsedTimer *timer;
        qint64 *total;
        ~Report() { if (timer->isValid()) *total += timer->nsecsElapsed(); }
    } report{&profile, &m_summaryNs};

    QVector<SensorReading> readings;
    for (const auto &row : m_manager.model()->rows())
        readings.push_back(row.reading);
    for (const auto &a : m_overheat.update(readings))
        emit overheat(tr("%1 — %2: %3 °C").arg(a.deviceName, a.sensorName).arg(a.value, 0, 'f', 1));

    struct Group {
        const SensorListModel::Row *hottest = nullptr;
        const SensorListModel::Row *fastestFan = nullptr;
        int available = 0;
        int total = 0;
    };
    struct DeviceInfo {
        QString id, name;
        DeviceCategory category = DeviceCategory::Other;
        const SensorListModel::Row *hottest = nullptr;
        int available = 0, total = 0;
    };

    Group cpu, gpu, storage;
    int fanAvailable = 0, fanTotal = 0;
    const SensorListModel::Row *fan = nullptr;
    QVector<DeviceInfo> devices;
    QHash<QString, int> deviceIndex;
    QHash<QString, const SensorListModel::Row *> byId;
    const SensorListModel::Row *gpuUtil = nullptr;
    const SensorListModel::Row *gpuMem = nullptr;
    QString gpuName;
    const SensorListModel::Row *cpuTemp = nullptr;

    for (const auto &row : m_manager.model()->rows()) {
        const SensorReading &r = row.reading;
        byId.insert(r.id, &row);
        // A stable "the CPU temperature" sensor: package / die readings win over single cores.
        if (r.category == DeviceCategory::Cpu && r.type == SensorType::Temperature && r.available) {
            const bool preferred = r.name.startsWith(QLatin1String("Package")) || r.name.startsWith(QLatin1String("Tctl"))
                                   || r.name.startsWith(QLatin1String("Tdie"));
            const bool curPreferred = cpuTemp && (cpuTemp->reading.name.startsWith(QLatin1String("Package"))
                                                  || cpuTemp->reading.name.startsWith(QLatin1String("Tctl"))
                                                  || cpuTemp->reading.name.startsWith(QLatin1String("Tdie")));
            if (!cpuTemp || (preferred && !curPreferred))
                cpuTemp = &row;
        }
        if (r.category == DeviceCategory::Gpu && gpuName.isEmpty())
            gpuName = r.deviceName;
        if (r.category == DeviceCategory::Gpu && r.available) {
            if (r.type == SensorType::Utilization && r.id.endsWith(QLatin1String("/util")) && (!gpuUtil || r.value > gpuUtil->reading.value))
                gpuUtil = &row;
            if (r.type == SensorType::Memory && !r.id.endsWith(QLatin1String("/mem_total")) && (!gpuMem || r.value > gpuMem->reading.value))
                gpuMem = &row;
        }

        int di = deviceIndex.value(r.deviceId, -1);
        if (di < 0) {
            di = static_cast<int>(devices.size());
            deviceIndex.insert(r.deviceId, di);
            devices.push_back({r.deviceId, r.deviceName, r.category, nullptr, 0, 0});
        }
        DeviceInfo &dev = devices[di];
        ++dev.total;
        if (r.available)
            ++dev.available;

        if (r.type == SensorType::FanSpeed && r.unit == QLatin1String("RPM")) {
            ++fanTotal;
            if (r.available) {
                ++fanAvailable;
                if (!fan || r.value > fan->reading.value)
                    fan = &row;
            }
        }
        if (r.type != SensorType::Temperature)
            continue;

        if (r.available && (!dev.hottest || r.value > dev.hottest->reading.value))
            dev.hottest = &row;

        Group *g = r.category == DeviceCategory::Cpu       ? &cpu
                   : r.category == DeviceCategory::Gpu     ? &gpu
                   : r.category == DeviceCategory::Storage ? &storage
                                                           : nullptr;
        if (!g)
            continue;
        ++g->total;
        if (r.available) {
            ++g->available;
            if (!g->hottest || r.value > g->hottest->reading.value)
                g->hottest = &row;
        }
    }

    m_highlights.clear();
    m_highlights << highlight(tr("CPU Temperature"), "°C", cpu.hottest, cpu.available, cpu.total)
                 << highlight(tr("GPU Temperature"), "°C", gpu.hottest, gpu.available, gpu.total)
                 << highlight(tr("Storage Temperature"), "°C", storage.hottest, storage.available,
                              storage.total)
                 << highlight(tr("Fan Speed"), "RPM", fan, fanAvailable, fanTotal);

    auto usable = [&](const char *id) {
        const auto *row = byId.value(QString::fromLatin1(id), nullptr);
        return row && row->reading.available ? row : nullptr;
    };
    auto single = [&](const QString &title, const QString &unit, const SensorListModel::Row *row) {
        return highlight(title, unit, row, row ? 1 : 0, 1);
    };
    m_highlights << single(tr("CPU Usage"), "%", usable("system/cpu_util"))
                 << single(tr("CPU Frequency"), "MHz", usable("system/cpu_freq_avg"))
                 << single(tr("Memory Usage"), "%", usable("system/mem_util"))
                 << single(tr("GPU Usage"), "%", gpuUtil)
                 << single(tr("GPU Memory"), "GB", gpuMem)
                 << single(tr("Network Download"), "MB/s", usable("net/total_read"))
                 << single(tr("Network Upload"), "MB/s", usable("net/total_write"))
                 << single(tr("Disk Read"), "MB/s", usable("disk/total_read"))
                 << single(tr("Disk Write"), "MB/s", usable("disk/total_write"));

    m_devices.clear();
    for (const DeviceInfo &d : std::as_const(devices)) {
        QVariantMap m;
        m["deviceId"] = d.id;
        m["deviceName"] = d.name;
        m["category"] = static_cast<int>(d.category);
        m["availableCount"] = d.available;
        m["totalCount"] = d.total;
        m["available"] = d.hottest != nullptr;
        m["temperature"] = d.hottest ? QVariant(d.hottest->reading.value) : QVariant();
        m["sensorId"] = d.hottest ? d.hottest->reading.id : QString();
        m_devices << m;
    }

    SystemInfo info;
    if (m_options.mock) {
        const auto *mem = byId.value(QStringLiteral("system/mem_util"), nullptr);
        info = MockFacts::systemInfo(*m_options.mock, mem && mem->reading.available ? mem->reading.value : 0.0,
                                     double(m_simClock.elapsed()) / 1000.0);
    } else {
        // The CPU model, topology and caches are read once; only the changing numbers every second.
        const QString proc = m_options.procRoot.isEmpty() ? QStringLiteral("/proc") : m_options.procRoot;
        if (!m_staticSystemInfo) {
            m_staticSystemInfo = SystemInfo::readStatic(proc, m_options.cpuRoot.isEmpty() ? QStringLiteral("/sys/devices/system/cpu")
                                                                                         : m_options.cpuRoot);
        }
        info = *m_staticSystemInfo;
        info.mergeDynamic(SystemInfo::readDynamic(proc));
    }
    auto gb = [](qint64 kb) { return kb < 0 ? QVariant() : QVariant(double(kb) / (1024.0 * 1024.0)); };
    m_details.clear();
    m_details["cpuModel"] = info.cpuModel;
    m_details["logicalCpus"] = info.logicalCpus;
    m_details["uptimeSeconds"] = info.uptimeSeconds;
    m_details["memTotalGb"] = gb(info.memTotalKb);
    m_details["memAvailableGb"] = gb(info.memAvailableKb);
    m_details["memUsedGb"] = (info.memTotalKb >= 0 && info.memAvailableKb >= 0)
                                 ? gb(info.memTotalKb - info.memAvailableKb) : QVariant();
    m_details["memCachedGb"] = gb(info.memCachedKb);
    m_details["memCommittedGb"] = gb(info.committedKb);
    m_details["memCommitLimitGb"] = gb(info.commitLimitKb);
    m_details["memSlabReclGb"] = gb(info.slabReclaimableKb);
    m_details["memSlabUnreclGb"] = gb(info.slabUnreclaimableKb);
    m_details["memSharedGb"] = gb(info.shmemKb);
    m_details["memBuffersGb"] = gb(info.buffersKb);
    m_details["memDirtyGb"] = gb(info.dirtyKb);
    m_details["swapTotalGb"] = gb(info.swapTotalKb);
    m_details["swapUsedGb"] = (info.swapTotalKb >= 0 && info.swapFreeKb >= 0)
                                  ? gb(info.swapTotalKb - info.swapFreeKb) : QVariant();
    m_details["load1"] = info.load1 >= 0 ? QVariant(info.load1) : QVariant();
    m_details["load5"] = info.load5 >= 0 ? QVariant(info.load5) : QVariant();
    m_details["load15"] = info.load15 >= 0 ? QVariant(info.load15) : QVariant();
    m_details["gpuName"] = gpuName;
    m_details["cpuTemp"] = sensorRef(cpuTemp);
    {
        QVariantList cores;
        for (int i = 0; i < info.logicalCpus; ++i) {
            const auto *row = byId.value(QStringLiteral("system/cpu%1_util").arg(i), nullptr);
            cores << (row && row->reading.available ? QVariant(row->reading.value) : QVariant());
        }
        m_details["coreUtil"] = cores;
    }
    m_details["fileHandles"] = info.fileHandles >= 0 ? QVariant(double(info.fileHandles)) : QVariant();
    m_details["physicalCores"] = info.physicalCores;
    m_details["sockets"] = info.sockets;
    m_details["maxMhz"] = info.maxMhz >= 0 ? QVariant(info.maxMhz) : QVariant();
    m_details["virtualization"] = info.virtualization;
    m_details["baseMhz"] = info.baseMhz >= 0 ? QVariant(info.baseMhz) : QVariant();
    m_details["powerLimitW"] = info.powerLimitW >= 0 ? QVariant(info.powerLimitW) : QVariant();
    m_details["l1Kb"] = info.l1Kb >= 0 ? QVariant(double(info.l1Kb)) : QVariant();
    m_details["l2Kb"] = info.l2Kb >= 0 ? QVariant(double(info.l2Kb)) : QVariant();
    m_details["l3Kb"] = info.l3Kb >= 0 ? QVariant(double(info.l3Kb)) : QVariant();
    fillHardwareLists(byId);
    emit summariesChanged();
    emit sensorSummaryChanged();
}
