#pragma once

#include "core/Autostart.h"
#include "core/DmiMemory.h"
#include "core/HardwareInfo.h"
#include "mock/MockActions.h"
#include "mock/MockProfile.h"
#include "core/Localization.h"
#include "core/WifiInfoMonitor.h"
#include "core/ServiceManager.h"
#include "core/OverheatMonitor.h"
#include "core/ProcessMonitor.h"
#include "core/SystemInfo.h"
#include "core/SensorManager.h"
#include "models/ProcessListModel.h"
#include "models/ServiceModel.h"
#include "models/StartupModel.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QSettings>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>

// How the app gets its hardware data. By default everything is read from the real machine;
// the other modes are opt-in (command line) and always announced in the UI.
struct AppOptions {
    // Simulation mode: replace all hardware providers with a generated profile.
    std::optional<MockProfile> mock;
    QString mockPath;
    double mockSpeed = 1.0; // simulated clock speed (scenarios happen faster)
    // Fixture mode: read the real parsers from prepared directory trees instead of /sys and /proc.
    QString hwmonRoot;
    QString procRoot;
    QString cpuRoot;
    QString powerSupplyRoot;
    QString drmRoot;

    bool fixtureMode() const {
        return !hwmonRoot.isEmpty() || !procRoot.isEmpty() || !cpuRoot.isEmpty() || !powerSupplyRoot.isEmpty() || !drmRoot.isEmpty();
    }
};

// The single object QML talks to: models, dashboard summaries, settings.
class MockProvider;

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(SensorListModel *sensors READ sensors CONSTANT)
    Q_PROPERTY(SensorProxyModel *sensorFilter READ sensorFilter CONSTANT)
    Q_PROPERTY(ServiceProxyModel *services READ services CONSTANT)
    Q_PROPERTY(bool servicesActive READ servicesActive WRITE setServicesActive NOTIFY servicesActiveChanged)
    Q_PROPERTY(int servicesRunning READ servicesRunning NOTIFY serviceSummaryChanged)
    Q_PROPERTY(int servicesFailed READ servicesFailed NOTIFY serviceSummaryChanged)
    Q_PROPERTY(int servicesStopped READ servicesStopped NOTIFY serviceSummaryChanged)
    Q_PROPERTY(int servicesTotal READ servicesTotal NOTIFY serviceSummaryChanged)
    Q_PROPERTY(QString servicesMessage READ servicesMessage NOTIFY servicesMessageChanged)
    Q_PROPERTY(StartupModel *startup READ startup CONSTANT)
    Q_PROPERTY(ProcessProxyModel *processes READ processes CONSTANT)
    Q_PROPERTY(bool processesActive READ processesActive WRITE setProcessesActive NOTIFY processesActiveChanged)
    Q_PROPERTY(int processCount READ processCount NOTIFY processSummaryChanged)
    Q_PROPERTY(int threadCount READ threadCount NOTIFY processSummaryChanged)
    Q_PROPERTY(double processCpuTotal READ processCpuTotal NOTIFY processSummaryChanged)
    Q_PROPERTY(QVariantList highlights READ highlights NOTIFY summariesChanged)
    Q_PROPERTY(int sensorCount READ sensorCount NOTIFY sensorSummaryChanged)
    Q_PROPERTY(int sensorsAvailable READ sensorsAvailable NOTIFY sensorSummaryChanged)
    // RAM module details from the firmware; needs administrator rights, so only on request.
    // {state: idle|loading|ready|error, error, slotsUsed, slotsTotal, speedMts, type, ..., modules: [...]}
    Q_PROPERTY(QVariantMap memoryModules READ memoryModules NOTIFY memoryModulesChanged)
    Q_PROPERTY(QVariantMap details READ details NOTIFY summariesChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY summariesChanged)
    Q_PROPERTY(int intervalMs READ intervalMs WRITE setIntervalMs NOTIFY intervalMsChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)
    // "" (real hardware), "mock" (simulated profile) or "fixtures" (prepared test directories)
    Q_PROPERTY(QString simulationKind READ simulationKind CONSTANT)
    Q_PROPERTY(QString simulationName READ simulationName CONSTANT)
    // Buttons for the simulation panel (empty outside simulation mode): [{kind, target}]
    Q_PROPERTY(QVariantList simulationActions READ simulationActions CONSTANT)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QVariantList languages READ languages NOTIFY languageChanged)
    // Keep running in the system tray when the window is closed.
    Q_PROPERTY(bool trayEnabled READ trayEnabled WRITE setTrayEnabled NOTIFY trayEnabledChanged)
    // "app" (program icon) or "temperature" (CPU temperature on a coloured tile)
    Q_PROPERTY(QStringList favoriteSensors READ favoriteSensors NOTIFY favoriteSensorsChanged)
    Q_PROPERTY(QString trayStyle READ trayStyle WRITE setTrayStyle NOTIFY trayStyleChanged)
    Q_PROPERTY(bool trayAvailable READ trayAvailable NOTIFY trayAvailableChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    Q_PROPERTY(bool overheatNotify READ overheatNotify WRITE setOverheatNotify NOTIFY overheatNotifyChanged)
    Q_PROPERTY(int overheatThreshold READ overheatThreshold WRITE setOverheatThreshold NOTIFY overheatThresholdChanged)
    Q_PROPERTY(int historySeconds READ historySeconds WRITE setHistorySeconds NOTIFY historySecondsChanged)
public:
    explicit AppController(AppOptions options = {}, QObject *parent = nullptr);

    SensorListModel *sensors() { return m_manager.model(); }
    SensorProxyModel *sensorFilter() { return &m_sensorProxy; }
    ServiceProxyModel *services() { return &m_serviceProxy; }
    bool servicesActive() const { return m_serviceManager.active(); }
    void setServicesActive(bool active);
    int servicesRunning() const { return m_serviceModel.countWithState(QStringLiteral("running")); }
    int servicesFailed() const { return m_serviceModel.countWithState(QStringLiteral("failed")); }
    int servicesStopped() const { return m_serviceModel.countWithState(QStringLiteral("stopped")) + m_serviceModel.countWithState(QStringLiteral("exited")); }
    int servicesTotal() const { return m_serviceModel.rowCount(); }
    QString servicesMessage() const { return m_servicesMessage; }
    // action: "start", "stop" or "restart"; the result arrives through servicesMessage.
    Q_INVOKABLE void serviceAction(const QString &name, const QString &action);
    StartupModel *startup() { return &m_startup; }
    ProcessProxyModel *processes() { return &m_processProxy; }
    bool processesActive() const { return m_processMonitor.active(); }
    void setProcessesActive(bool active);
    int processCount() const { return m_processCount; }
    int threadCount() const { return m_threadCount; }
    double processCpuTotal() const { return m_processCpu; }
    QVariantList highlights() const { return m_highlights; }
    QVariantList devices() const { return m_devices; }
    QVariantMap details() const { return m_details; }
    QVariantMap memoryModules() const { return m_memoryModules; }
    Q_INVOKABLE void loadMemoryModules();
    int sensorCount() const;
    int sensorsAvailable() const;

    int intervalMs() const { return m_intervalMs; }
    void setIntervalMs(int ms);
    bool darkTheme() const { return m_dark; }
    void setDarkTheme(bool dark);
    QString simulationKind() const { return m_options.mock ? QStringLiteral("mock") : m_options.fixtureMode() ? QStringLiteral("fixtures") : QString(); }
    QString simulationName() const { return m_options.mock ? m_options.mock->name : m_options.hwmonRoot.isEmpty() ? m_options.procRoot : m_options.hwmonRoot; }
    QVariantList simulationActions() const;
    Q_INVOKABLE void runSimulationAction(int index);
    Q_INVOKABLE void clearSimulationScenarios();
    // "" follows the system language; otherwise a code from `languages`.
    QString language() const { return m_languageSetting; }
    void setLanguage(const QString &code);
    QVariantList languages() const;
    bool trayEnabled() const { return m_trayEnabled; }
    void setTrayEnabled(bool enabled);
    QStringList favoriteSensors() const { return m_favorites; }
    Q_INVOKABLE void toggleFavorite(const QString &sensorId);
    QString trayStyle() const { return m_trayStyle; }
    void setTrayStyle(const QString &style);
    bool trayAvailable() const { return m_trayAvailable; }
    void setTrayAvailable(bool available);
    Q_INVOKABLE void quit();
    bool autostart() const { return m_autostart.isEnabled(); }
    void setAutostart(bool enabled);
    bool overheatNotify() const { return m_overheatNotify; }
    void setOverheatNotify(bool on);
    int overheatThreshold() const { return m_overheat.threshold(); }
    void setOverheatThreshold(int celsius);
    int historySeconds() const { return m_historySeconds; }
    void setHistorySeconds(int seconds);

    // Startup tab: reload the list / toggle an entry. Returns an error message or "".
    Q_INVOKABLE void refreshStartup() { m_startup.refresh(); }
    Q_INVOKABLE QString setStartupEnabled(const QString &id, bool enabled);

    // Small persistent UI state (window size, last page, ...), stored under "ui/".
    Q_INVOKABLE QVariant uiSetting(const QString &key, const QVariant &fallback = {}) const;
    Q_INVOKABLE void setUiSetting(const QString &key, const QVariant &value);

    Q_INVOKABLE void copyToClipboard(const QString &text);

    // Sends SIGTERM (or SIGKILL when force is set). Returns an error message,
    // or an empty string on success.
    Q_INVOKABLE QString endProcess(int pid, bool force);
    // Scheduling priority via nice (-20 high .. 19 low). Returns an error message or "".
    Q_INVOKABLE QString setProcessPriority(int pid, int nice);
    // Logical CPUs a process may run on / restrict it to the given CPU indices.
    Q_INVOKABLE QVariantList processAffinity(int pid) const;
    Q_INVOKABLE QString setProcessAffinity(int pid, const QVariantList &cpus);
    Q_INVOKABLE QVariantMap processDetails(int pid) const { return ProcessScanner::details(pid); }
    // Same for several PIDs (an application group); returns the first error.
    Q_INVOKABLE QString endProcesses(const QVariantList &pids, bool force);

    // Writes the history to a timestamped CSV in the Documents folder.
    // Returns the file path, or an empty string on failure.
    Q_INVOKABLE QString exportHistory();
    // CSV of a single sensor; returns the file path or "" on failure.
    Q_INVOKABLE QString exportSensor(const QString &sensorId);
    // {min, max, average, samples, spanSeconds} over the history window; empty when there is no data.
    Q_INVOKABLE QVariantMap sensorStats(const QString &sensorId) const;

    // Points are (seconds ago <= 0, value); gaps have a NaN value.
    Q_INVOKABLE QVariantList history(const QString &sensorId) const;

signals:
    void summariesChanged();
    void processesActiveChanged();
    void processSummaryChanged();
    void overheat(const QString &message);
    void overheatThresholdChanged();
    void overheatNotifyChanged();
    void autostartChanged();
    void trayEnabledChanged();
    void trayStyleChanged();
    void favoriteSensorsChanged();
    void trayAvailableChanged();
    void languageChanged();
    void sensorSummaryChanged();
    void memoryModulesChanged();
    void servicesActiveChanged();
    void serviceSummaryChanged();
    void servicesMessageChanged();
    void intervalMsChanged();
    void darkThemeChanged();
    void historySecondsChanged();

private:
    void rebuildSummaries();
    QString historyFilePath() const;
    void saveHistory();
    void printProfile();
    void applyMemoryInfo(const MemoryInfo &info);
    void fillHardwareLists(const QHash<QString, const SensorListModel::Row *> &byId);

    SensorManager m_manager;
    QString exportCsvFile(const QString &label, const QString &sensorId);
    OverheatMonitor m_overheat;
    bool m_overheatNotify = false; // opt-in: no popups unless the user asks for them
    Autostart m_autostart;
    Localization m_localization;
    QString m_languageSetting;
    bool m_trayEnabled = false;
    QStringList m_favorites;
    QString m_trayStyle = QStringLiteral("app");
    bool m_trayAvailable = false;
    ServiceListModel m_serviceModel;
    ServiceProxyModel m_serviceProxy;
    SensorProxyModel m_sensorProxy;
    ServiceManager m_serviceManager;
    QString m_servicesMessage;
    bool m_servicesListFailed = false;
    StartupModel m_startup;
    ProcessListModel m_processModel;
    ProcessProxyModel m_processProxy;
    ProcessMonitor m_processMonitor;
    int m_processCount = 0;
    int m_threadCount = 0;
    double m_processCpu = 0.0;
    QSettings m_settings;
    QVariantList m_highlights;
    QVariantList m_devices;
    QVariantMap m_details;
    std::optional<SystemInfo> m_staticSystemInfo;
    qint64 m_summaryNs = 0; // HWMON_PROFILE diagnostics
    AppOptions m_options;
    DmiMemoryReader m_dmiReader;
    QVariantMap m_memoryModules{{QStringLiteral("state"), QStringLiteral("idle")}};
    MockProvider *m_mockProvider = nullptr; // owned by the SensorManager once started
    QVector<MockAction> m_mockActions;
    QElapsedTimer m_simClock;
    WifiInfoMonitor m_wifiMonitor;
    WifiInfoMap m_wifi;
    QVector<HardwareInfo::Volume> m_volumes;
    qint64 m_volumesStamp = 0;
    QHash<QString, QStringList> m_ipCache;
    qint64 m_ipStamp = 0;
    QHash<QString, QVariantMap> m_staticInfo; // per disk / interface facts that never change
    int m_intervalMs;
    int m_historySeconds;
    bool m_dark;
};
