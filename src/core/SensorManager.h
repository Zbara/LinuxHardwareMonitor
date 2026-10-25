#pragma once

#include "SensorHistory.h"
#include "SensorTypes.h"
#include "models/SensorListModel.h"
#include "providers/ISensorProvider.h"

#include <QHash>
#include <QIODevice>
#include <QObject>
#include <QThread>

#include <memory>
#include <vector>

class Poller;

// Owns the providers, runs them on one worker thread, and folds the results
// into the model and the per-sensor history on the calling (GUI) thread.
class SensorManager : public QObject {
    Q_OBJECT
public:
    explicit SensorManager(QObject *parent = nullptr);
    ~SensorManager() override;

    // Must be called before start().
    void addProvider(std::unique_ptr<ISensorProvider> provider);

    void start(int intervalMs);
    void stop();

    void setInterval(int intervalMs);
    void setHistorySeconds(int seconds);

    SensorListModel *model() { return &m_model; }
    const SensorListModel *model() const { return &m_model; }
    // Total time spent in applyReadings() so far (for the HWMON_PROFILE diagnostics).
    qint64 applyNanoseconds() const { return m_applyNs; }
    QVector<SensorHistory::Sample> history(const QString &sensorId) const;

    // Summary of the valid samples in a sensor's history window.
    struct HistoryStats {
        bool valid = false; // false when the sensor has no valid sample
        double min = 0.0;
        double max = 0.0;
        double average = 0.0;
        int samples = 0;
        double spanSeconds = 0.0; // oldest to newest valid sample
    };
    HistoryStats historyStats(const QString &sensorId) const;

    // Writes the in-memory history as CSV (gaps have an empty value), for every sensor or only for
    // `onlySensorId`. Returns the number of samples written.
    int exportCsv(QIODevice &out, const QString &onlySensorId = {}) const;

    // Sensor history that survives restarts. exportHistory() serialises the recent samples of every
    // numeric sensor (compressed); importHistory() stages them, and each sensor picks its samples up
    // the first time it is reported again. Samples older than the history window, unknown ids and
    // corrupt or foreign data are ignored. Returns the number of sensors staged.
    QByteArray exportHistory(qint64 nowMs, int maxSamples = 900) const;
    int importHistory(const QByteArray &data, qint64 nowMs);

    // Folds one polling cycle into the state. Public so it can be driven
    // without a thread. Sensors absent from `readings` are marked unavailable.
    void applyReadings(const QVector<SensorReading> &readings);

signals:
    void updated();

private:
    struct State {
        SensorReading last;
        SensorHistory history;
        bool hasStats = false;
        double min = 0.0;
        double max = 0.0;
    };

    std::size_t historyCapacity() const;

    SensorListModel m_model;
    QHash<QString, State> m_states;
    QHash<QString, QVector<SensorHistory::Sample>> m_pendingHistory;
    std::vector<std::unique_ptr<ISensorProvider>> m_pendingProviders;
    QThread m_thread;
    Poller *m_poller = nullptr;
    qint64 m_applyNs = 0;
    int m_intervalMs = 1000;
    int m_historySeconds = 300;
};
