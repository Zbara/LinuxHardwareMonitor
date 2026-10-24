#pragma once

#include "core/SensorTypes.h"

#include <QAbstractListModel>
#include <QHash>

// Flat list of all sensors, kept sorted by device so that QML can group it
// with ListView sections.
class SensorListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        DeviceIdRole,
        DeviceNameRole,
        NameRole,
        TypeRole,
        ValueRole,
        UnitRole,
        AvailableRole,
        MinRole,
        MaxRole,
        HasStatsRole,
    };

    struct Row {
        SensorReading reading;
        double min = 0.0;
        double max = 0.0;
        bool hasStats = false;
    };

    explicit SensorListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void update(const SensorReading &reading, bool hasStats, double min, double max);
    const QVector<Row> &rows() const { return m_rows; }

private:
    QVector<Row> m_rows;
    QHash<QString, int> m_index;
};
