#include "ServiceModel.h"

namespace {

// Lower is shown first when sorting by status.
int stateRank(const QString &state) {
    if (state == QLatin1String("failed"))   return 0;
    if (state == QLatin1String("starting")) return 1;
    if (state == QLatin1String("running"))  return 2;
    if (state == QLatin1String("exited"))   return 3;
    if (state == QLatin1String("stopped"))  return 4;
    return 5;
}

} // namespace

ServiceListModel::ServiceListModel(QObject *parent) : QAbstractListModel(parent) {}

int ServiceListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant ServiceListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const ServiceInfo &s = m_rows[index.row()];
    switch (role) {
    case NameRole:        return s.name;
    case DescriptionRole: return s.description;
    case StateRole:       return s.state;
    case ActiveRole:      return s.activeState;
    case SubRole:         return s.subState;
    case LoadRole:        return s.loadState;
    }
    return {};
}

QHash<int, QByteArray> ServiceListModel::roleNames() const {
    return {{NameRole, "name"},     {DescriptionRole, "description"}, {StateRole, "state"},
            {ActiveRole, "active"}, {SubRole, "sub"},                 {LoadRole, "load"}};
}

void ServiceListModel::update(const QVector<ServiceInfo> &services) {
    QHash<QString, const ServiceInfo *> incoming;
    for (const ServiceInfo &s : services)
        incoming.insert(s.name, &s);

    for (int row = static_cast<int>(m_rows.size()) - 1; row >= 0; --row) {
        if (incoming.contains(m_rows[row].name))
            continue;
        beginRemoveRows({}, row, row);
        m_rows.removeAt(row);
        endRemoveRows();
    }
    m_index.clear();
    for (int i = 0; i < m_rows.size(); ++i)
        m_index.insert(m_rows[i].name, i);

    QVector<ServiceInfo> added;
    for (const ServiceInfo &s : services) {
        const auto it = m_index.constFind(s.name);
        if (it == m_index.constEnd()) {
            added.push_back(s);
        } else if (m_rows[it.value()].state != s.state || m_rows[it.value()].subState != s.subState
                   || m_rows[it.value()].description != s.description) {
            m_rows[it.value()] = s;
            emit dataChanged(index(it.value()), index(it.value()));
        }
    }
    if (!added.isEmpty()) {
        const int first = static_cast<int>(m_rows.size());
        beginInsertRows({}, first, first + static_cast<int>(added.size()) - 1);
        m_rows += added;
        endInsertRows();
        for (int i = first; i < m_rows.size(); ++i)
            m_index.insert(m_rows[i].name, i);
    }
}

int ServiceListModel::countWithState(const QString &state) const {
    int n = 0;
    for (const ServiceInfo &s : m_rows)
        n += s.state == state;
    return n;
}

ServiceProxyModel::ServiceProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
}

void ServiceProxyModel::setFilterText(const QString &text) {
    if (text.trimmed().toLower() == m_filter)
        return;
    m_filter = text.trimmed().toLower();
    invalidateFilter();
    emit filterTextChanged();
}

void ServiceProxyModel::setStateFilter(const QString &state) {
    if (state == m_stateFilter)
        return;
    m_stateFilter = state;
    invalidateFilter();
    emit stateFilterChanged();
}

void ServiceProxyModel::sortBy(const QString &key) {
    if (key == m_key) {
        m_descending = !m_descending;
    } else {
        m_key = key;
        m_descending = false;
    }
    // Direction is applied in lessThan() so the name tie-break stays A-Z.
    sort(-1);
    sort(0, Qt::AscendingOrder);
    emit sortChanged();
}

bool ServiceProxyModel::filterAcceptsRow(int row, const QModelIndex &parent) const {
    const QModelIndex idx = sourceModel()->index(row, 0, parent);
    if (!m_stateFilter.isEmpty()) {
        const QString state = sourceModel()->data(idx, ServiceListModel::StateRole).toString();
        // "stopped" covers exited one-shots too: neither is running nor broken.
        const bool match = m_stateFilter == QLatin1String("stopped") ? (state == QLatin1String("stopped") || state == QLatin1String("exited"))
                                                                     : state == m_stateFilter;
        if (!match)
            return false;
    }
    if (m_filter.isEmpty())
        return true;
    return sourceModel()->data(idx, ServiceListModel::NameRole).toString().toLower().contains(m_filter)
           || sourceModel()->data(idx, ServiceListModel::DescriptionRole).toString().toLower().contains(m_filter);
}

bool ServiceProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    const QString ln = sourceModel()->data(left, ServiceListModel::NameRole).toString();
    const QString rn = sourceModel()->data(right, ServiceListModel::NameRole).toString();
    if (m_key == QLatin1String("status")) {
        const int lr = stateRank(sourceModel()->data(left, ServiceListModel::StateRole).toString());
        const int rr = stateRank(sourceModel()->data(right, ServiceListModel::StateRole).toString());
        if (lr != rr)
            return m_descending ? lr > rr : lr < rr;
        return ln.compare(rn, Qt::CaseInsensitive) < 0;
    }
    const int c = ln.compare(rn, Qt::CaseInsensitive);
    return m_descending ? c > 0 : c < 0;
}
