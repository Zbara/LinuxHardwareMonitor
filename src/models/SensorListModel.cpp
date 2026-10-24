#include "SensorListModel.h"

#include <QCollator>

#include <algorithm>

namespace {

bool lessThan(const SensorReading &a, const SensorReading &b) {
    static const QCollator collator = [] {
        QCollator c;
        c.setNumericMode(true);
        return c;
    }();
    if (int c = collator.compare(a.deviceName, b.deviceName))
        return c < 0;
    if (a.deviceId != b.deviceId)
        return a.deviceId < b.deviceId;
    if (a.type != b.type)
        return a.type < b.type;
    return collator.compare(a.name, b.name) < 0;
}

} // namespace

SensorListModel::SensorListModel(QObject *parent) : QAbstractListModel(parent) {}

int SensorListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant SensorListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows[index.row()];
    const SensorReading &r = row.reading;
    switch (role) {
    case IdRole:         return r.id;
    case DeviceIdRole:   return r.deviceId;
    case DeviceNameRole: return r.deviceName;
    case NameRole:       return r.name;
    case TypeRole:       return static_cast<int>(r.type);
    case ValueRole:      return r.available ? QVariant(r.value) : QVariant();
    case UnitRole:       return r.unit;
    case AvailableRole:  return r.available;
    case MinRole:        return row.hasStats ? QVariant(row.min) : QVariant();
    case MaxRole:        return row.hasStats ? QVariant(row.max) : QVariant();
    case HasStatsRole:   return row.hasStats;
    }
    return {};
}

QHash<int, QByteArray> SensorListModel::roleNames() const {
    return {
        {IdRole, "sensorId"},
        {DeviceIdRole, "deviceId"},
        {DeviceNameRole, "deviceName"},
        {NameRole, "name"},
        {TypeRole, "type"},
        {ValueRole, "value"},
        {UnitRole, "unit"},
        {AvailableRole, "available"},
        {MinRole, "minValue"},
        {MaxRole, "maxValue"},
        {HasStatsRole, "hasStats"},
    };
}

void SensorListModel::update(const SensorReading &reading, bool hasStats, double min, double max) {
    const auto it = m_index.constFind(reading.id);
    if (it != m_index.constEnd()) {
        const int i = it.value();
        Row &row = m_rows[i];
        row.reading = reading;
        row.hasStats = hasStats;
        row.min = min;
        row.max = max;
        emit dataChanged(index(i), index(i));
        return;
    }

    const auto pos = std::lower_bound(m_rows.begin(), m_rows.end(), reading,
                                      [](const Row &row, const SensorReading &r) {
                                          return lessThan(row.reading, r);
                                      });
    const int at = static_cast<int>(pos - m_rows.begin());
    beginInsertRows({}, at, at);
    m_rows.insert(pos, Row{reading, min, max, hasStats});
    m_index.clear();
    for (int i = 0; i < m_rows.size(); ++i)
        m_index.insert(m_rows[i].reading.id, i);
    endInsertRows();
}

SensorProxyModel::SensorProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
}

void SensorProxyModel::setFilterText(const QString &text) {
    const QString normalized = text.trimmed().toLower();
    if (normalized == m_filter)
        return;
    m_filter = normalized;
    invalidateFilter();
    emit filterTextChanged();
}

void SensorProxyModel::setTypeFilter(int type) {
    if (type == m_type)
        return;
    m_type = type;
    invalidateFilter();
    emit typeFilterChanged();
}

bool SensorProxyModel::filterAcceptsRow(int row, const QModelIndex &parent) const {
    const QModelIndex idx = sourceModel()->index(row, 0, parent);
    const int type = sourceModel()->data(idx, SensorListModel::TypeRole).toInt();
    if (type == static_cast<int>(SensorType::Text))
        return false;
    if (m_type >= 0 && type != m_type)
        return false;
    if (m_filter.isEmpty())
        return true;
    return sourceModel()->data(idx, SensorListModel::NameRole).toString().toLower().contains(m_filter)
        || sourceModel()->data(idx, SensorListModel::DeviceNameRole).toString().toLower().contains(m_filter);
}
