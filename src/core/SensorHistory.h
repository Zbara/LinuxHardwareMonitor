#pragma once

#include <QVector>
#include <QtGlobal>

#include <cstddef>
#include <optional>

// Bounded ring buffer of timestamped samples. Missing values are stored as
// invalid samples so that charts can show a gap instead of a zero.
class SensorHistory {
public:
    struct Sample {
        qint64 timestampMs = 0;
        double value = 0.0;
        bool valid = false;
    };

    explicit SensorHistory(std::size_t capacity);

    void push(qint64 timestampMs, std::optional<double> value);
    void setCapacity(std::size_t capacity);

    std::size_t size() const { return m_size; }
    std::size_t capacity() const { return m_buffer.size(); }

    // Oldest first.
    QVector<Sample> samples() const;

private:
    std::vector<Sample> m_buffer;
    std::size_t m_start = 0;
    std::size_t m_size = 0;
};
