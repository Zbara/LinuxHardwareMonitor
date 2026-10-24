#include "SensorHistory.h"

#include <algorithm>

SensorHistory::SensorHistory(std::size_t capacity)
    : m_buffer(std::max<std::size_t>(capacity, 1)) {}

void SensorHistory::push(qint64 timestampMs, std::optional<double> value) {
    const Sample sample{timestampMs, value.value_or(0.0), value.has_value()};
    const std::size_t cap = m_buffer.size();
    if (m_size < cap) {
        m_buffer[(m_start + m_size) % cap] = sample;
        ++m_size;
    } else {
        m_buffer[m_start] = sample;
        m_start = (m_start + 1) % cap;
    }
}

void SensorHistory::setCapacity(std::size_t capacity) {
    capacity = std::max<std::size_t>(capacity, 1);
    if (capacity == m_buffer.size())
        return;

    const QVector<Sample> old = samples();
    m_buffer.assign(capacity, Sample{});
    m_start = 0;
    m_size = 0;
    const qsizetype skip = std::max<qsizetype>(0, old.size() - static_cast<qsizetype>(capacity));
    for (qsizetype i = skip; i < old.size(); ++i)
        m_buffer[m_size++] = old[i];
}

QVector<SensorHistory::Sample> SensorHistory::samples() const {
    QVector<Sample> out;
    out.reserve(static_cast<qsizetype>(m_size));
    for (std::size_t i = 0; i < m_size; ++i)
        out.push_back(m_buffer[(m_start + i) % m_buffer.size()]);
    return out;
}
