#include "SensorManager.h"

#include <QDataStream>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QSet>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>

// Lives on the worker thread. Timer events are delivered serially, so a slow
// cycle can never overlap the next one.
class Poller : public QObject {
    Q_OBJECT
public:
    static constexpr int kSecondPollDelayMs = 250;
    explicit Poller(std::vector<std::unique_ptr<ISensorProvider>> providers)
        : m_providers(std::move(providers)) {}

public slots:
    void start(int intervalMs) {
        if (!m_timer) {
            m_timer = new QTimer(this);
            connect(m_timer, &QTimer::timeout, this, &Poller::pollOnce);
        }
        m_timer->start(intervalMs);
        pollOnce();
        // Rates (CPU load, disk and network throughput) need two samples. A quick second poll fills them
        // in within a fraction of a second instead of showing N/A for a whole update interval at start.
        QTimer::singleShot(kSecondPollDelayMs, this, &Poller::pollOnce);
    }

    void setInterval(int intervalMs) {
        if (m_timer)
            m_timer->setInterval(intervalMs);
    }

    void pollOnce() {
        QVector<SensorReading> all;
        for (auto &provider : m_providers) {
            try {
                all += provider->poll();
            } catch (...) {
                // One failing provider must not affect the others.
            }
        }
        emit polled(all);
    }

signals:
    void polled(const QVector<SensorReading> &readings);

private:
    std::vector<std::unique_ptr<ISensorProvider>> m_providers;
    QTimer *m_timer = nullptr;
};

SensorManager::SensorManager(QObject *parent) : QObject(parent) {
    qRegisterMetaType<SensorReading>();
    qRegisterMetaType<QVector<SensorReading>>();
}

SensorManager::~SensorManager() {
    stop();
}

void SensorManager::addProvider(std::unique_ptr<ISensorProvider> provider) {
    m_pendingProviders.push_back(std::move(provider));
}

std::size_t SensorManager::historyCapacity() const {
    const qint64 n = static_cast<qint64>(m_historySeconds) * 1000 / std::max(m_intervalMs, 1) + 1;
    return static_cast<std::size_t>(std::max<qint64>(n, 2));
}

void SensorManager::start(int intervalMs) {
    if (m_poller)
        return;
    m_intervalMs = intervalMs;

    m_poller = new Poller(std::move(m_pendingProviders));
    m_pendingProviders.clear();
    m_poller->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_poller, &QObject::deleteLater);
    connect(m_poller, &Poller::polled, this, &SensorManager::applyReadings, Qt::QueuedConnection);
    m_thread.start();
    QMetaObject::invokeMethod(m_poller, "start", Qt::QueuedConnection, Q_ARG(int, intervalMs));
}

void SensorManager::stop() {
    if (!m_poller)
        return;
    m_thread.quit();
    m_thread.wait();
    m_poller = nullptr; // deleted by deleteLater when the thread finished
}

void SensorManager::setInterval(int intervalMs) {
    m_intervalMs = intervalMs;
    setHistorySeconds(m_historySeconds);
    if (m_poller)
        QMetaObject::invokeMethod(m_poller, "setInterval", Qt::QueuedConnection,
                                  Q_ARG(int, intervalMs));
}

void SensorManager::setHistorySeconds(int seconds) {
    m_historySeconds = seconds;
    const std::size_t cap = historyCapacity();
    for (State &s : m_states)
        s.history.setCapacity(cap);
}

QVector<SensorHistory::Sample> SensorManager::history(const QString &sensorId) const {
    const auto it = m_states.constFind(sensorId);
    return it == m_states.constEnd() ? QVector<SensorHistory::Sample>() : it->history.samples();
}

namespace {
constexpr quint32 kHistoryMagic = 0x48574d48; // "HWMH"
constexpr quint32 kHistoryVersion = 1;
} // namespace

QByteArray SensorManager::exportHistory(qint64 nowMs, int maxSamples) const {
    QByteArray raw;
    QDataStream out(&raw, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);

    QVector<QPair<QString, QVector<SensorHistory::Sample>>> items;
    for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) {
        if (it->last.type == SensorType::Text)
            continue;
        QVector<SensorHistory::Sample> samples = it->history.samples();
        if (samples.size() > maxSamples)
            samples.remove(0, samples.size() - maxSamples);
        bool anyValid = false;
        for (const auto &sample : std::as_const(samples))
            anyValid = anyValid || sample.valid;
        if (anyValid) // a sensor that never produced a value has nothing worth keeping
            items.push_back({it.key(), samples});
    }

    // The save time is stored so samples keep their absolute timestamps after a restart.
    out << kHistoryMagic << kHistoryVersion << static_cast<qint64>(nowMs) << static_cast<quint32>(items.size());
    for (const auto &item : std::as_const(items)) {
        out << item.first << static_cast<quint32>(item.second.size());
        for (const auto &sample : item.second) {
            // Times relative to "now" fit in 32 bits; a gap is stored as NaN.
            out << static_cast<qint32>(std::clamp<qint64>(sample.timestampMs - nowMs, INT32_MIN, 0))
                << (sample.valid ? static_cast<float>(sample.value) : std::numeric_limits<float>::quiet_NaN());
        }
    }
    return qCompress(raw, 6);
}

int SensorManager::importHistory(const QByteArray &data, qint64 nowMs) {
    const QByteArray raw = qUncompress(data);
    if (raw.isEmpty())
        return 0;
    QDataStream in(raw);
    in.setVersion(QDataStream::Qt_6_0);
    in.setFloatingPointPrecision(QDataStream::SinglePrecision);

    quint32 magic = 0, version = 0, count = 0;
    qint64 savedAtMs = 0;
    in >> magic >> version >> savedAtMs >> count;
    if (in.status() != QDataStream::Ok || magic != kHistoryMagic || version != kHistoryVersion || count > 100000)
        return 0;

    const qint64 oldest = nowMs - static_cast<qint64>(m_historySeconds) * 1000;
    int staged = 0;
    QHash<QString, QVector<SensorHistory::Sample>> pending;
    for (quint32 i = 0; i < count; ++i) {
        QString id;
        quint32 n = 0;
        in >> id >> n;
        if (in.status() != QDataStream::Ok || n > 100000)
            return 0; // truncated or hostile data: take nothing
        QVector<SensorHistory::Sample> samples;
        samples.reserve(static_cast<qsizetype>(n));
        for (quint32 j = 0; j < n; ++j) {
            qint32 relative = 0;
            float value = 0;
            in >> relative >> value;
            if (in.status() != QDataStream::Ok)
                return 0;
            const qint64 ts = savedAtMs + relative;
            if (ts < oldest)
                continue; // outside the history window
            SensorHistory::Sample sample;
            sample.timestampMs = ts;
            sample.valid = !std::isnan(value);
            sample.value = sample.valid ? static_cast<double>(value) : 0.0;
            samples.push_back(sample);
        }
        if (!samples.isEmpty()) {
            pending.insert(id, samples);
            ++staged;
        }
    }
    m_pendingHistory = pending;
    return staged;
}

SensorManager::HistoryStats SensorManager::historyStats(const QString &sensorId) const {
    HistoryStats st;
    double sum = 0.0;
    qint64 first = 0, last = 0;
    for (const auto &s : history(sensorId)) {
        if (!s.valid)
            continue;
        if (!st.valid) {
            st.valid = true;
            st.min = st.max = s.value;
            first = s.timestampMs;
        }
        st.min = std::min(st.min, s.value);
        st.max = std::max(st.max, s.value);
        sum += s.value;
        last = s.timestampMs;
        ++st.samples;
    }
    if (st.valid) {
        st.average = sum / st.samples;
        st.spanSeconds = (last - first) / 1000.0;
    }
    return st;
}

int SensorManager::exportCsv(QIODevice &out, const QString &onlySensorId) const {
    auto quote = [](const QString &field) {
        if (!field.contains(QLatin1Char(',')) && !field.contains(QLatin1Char('"'))
            && !field.contains(QLatin1Char('\n')))
            return field;
        QString f = field;
        f.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + f + QLatin1Char('"');
    };

    out.write("sensor_id,device,sensor,unit,timestamp,value\n");
    int count = 0;
    for (const auto &row : m_model.rows()) {
        const SensorReading &r = row.reading;
        if (r.type == SensorType::Text || (!onlySensorId.isEmpty() && r.id != onlySensorId))
            continue;
        const QString prefix = quote(r.id) + QLatin1Char(',') + quote(r.deviceName) + QLatin1Char(',')
                               + quote(r.name) + QLatin1Char(',') + quote(r.unit) + QLatin1Char(',');
        for (const auto &s : history(r.id)) {
            const QString line = prefix
                                 + QDateTime::fromMSecsSinceEpoch(s.timestampMs).toString(Qt::ISODateWithMs)
                                 + QLatin1Char(',') + (s.valid ? QString::number(s.value, 'f', 3) : QString())
                                 + QLatin1Char('\n');
            out.write(line.toUtf8());
            ++count;
        }
    }
    return count;
}

void SensorManager::applyReadings(const QVector<SensorReading> &readings) {
    QElapsedTimer profile;
    profile.start();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QSet<QString> seen;

    auto apply = [&](const SensorReading &r) {
        auto it = m_states.find(r.id);
        if (it == m_states.end()) {
            it = m_states.insert(r.id, State{r, SensorHistory(historyCapacity()), false, 0, 0});
            // Samples from the previous run, if any, come first (the clock gap between runs stays a gap).
            const auto restored = m_pendingHistory.take(r.id);
            for (const auto &old : restored)
                it->history.push(old.timestampMs, old.valid ? std::optional<double>(old.value) : std::nullopt);
            // The program was not running between the two sessions: end the restored run with an explicit
            // gap sample, or a chart would join the last old and the first new sample with a straight line.
            if (!restored.isEmpty() && restored.last().valid)
                it->history.push(restored.last().timestampMs + 1, std::nullopt);
        }
        State &s = it.value();
        s.last = r;
        if (r.available) {
            s.min = s.hasStats ? std::min(s.min, r.value) : r.value;
            s.max = s.hasStats ? std::max(s.max, r.value) : r.value;
            s.hasStats = true;
        }
        s.history.push(nowMs, r.available ? std::optional<double>(r.value) : std::nullopt);
        m_model.update(r, s.hasStats, s.min, s.max);
    };

    for (const SensorReading &r : readings) {
        if (seen.contains(r.id))
            continue; // the same physical sensor reported twice
        seen.insert(r.id);
        apply(r);
    }

    // Sensors that were known but are no longer reported have disappeared.
    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (seen.contains(it.key()))
            continue;
        SensorReading gone = it->last;
        gone.available = false;
        gone.timestamp = QDateTime::currentDateTime();
        apply(gone);
    }

    m_applyNs += profile.nsecsElapsed();
    emit updated();
}

#include "SensorManager.moc"
