#pragma once

#include "core/SensorTypes.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QSortFilterProxyModel>

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

// What the Sensors page shows: numeric sensors only (Text rows carry static facts for other pages),
// optionally narrowed by a search string (sensor or device name) and a sensor type.
class SensorProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    // -1 = every type, otherwise a SensorType value
    Q_PROPERTY(int typeFilter READ typeFilter WRITE setTypeFilter NOTIFY typeFilterChanged)
public:
    explicit SensorProxyModel(QObject *parent = nullptr);

    QString filterText() const { return m_filter; }
    void setFilterText(const QString &text);
    int typeFilter() const { return m_type; }
    void setTypeFilter(int type);
    // When set, only the sensors whose id is in `favorites` are listed.
    Q_PROPERTY(bool favoritesOnly READ favoritesOnly WRITE setFavoritesOnly NOTIFY favoritesOnlyChanged)
    bool favoritesOnly() const { return m_favoritesOnly; }
    void setFavoritesOnly(bool on);
    void setFavorites(const QStringList &ids);

signals:
    void filterTextChanged();
    void typeFilterChanged();
    void favoritesOnlyChanged();

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;

private:
    QString m_filter;
    int m_type = -1;
    bool m_favoritesOnly = false;
    QSet<QString> m_favorites;
};
