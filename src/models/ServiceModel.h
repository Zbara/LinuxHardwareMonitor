#pragma once

#include "core/ServiceManager.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSortFilterProxyModel>

class ServiceListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role { NameRole = Qt::UserRole + 1, DescriptionRole, StateRole, ActiveRole, SubRole, LoadRole };

    explicit ServiceListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Incremental update by service name (keeps scroll position and selection).
    void update(const QVector<ServiceInfo> &services);

    int countWithState(const QString &state) const;

private:
    QVector<ServiceInfo> m_rows;
    QHash<QString, int> m_index;
};

// Text filter plus sorting by name or by status (failed first).
class ServiceProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    // "" (all), "running", "stopped" or "failed"
    Q_PROPERTY(QString stateFilter READ stateFilter WRITE setStateFilter NOTIFY stateFilterChanged)
    Q_PROPERTY(QString sortKey READ sortKey NOTIFY sortChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending NOTIFY sortChanged)
public:
    explicit ServiceProxyModel(QObject *parent = nullptr);

    QString filterText() const { return m_filter; }
    void setFilterText(const QString &text);
    QString stateFilter() const { return m_stateFilter; }
    void setStateFilter(const QString &state);
    QString sortKey() const { return m_key; }
    bool sortDescending() const { return m_descending; }
    Q_INVOKABLE void sortBy(const QString &key);

signals:
    void filterTextChanged();
    void stateFilterChanged();
    void sortChanged();

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QString m_filter;
    QString m_stateFilter;
    QString m_key = QStringLiteral("name");
    bool m_descending = false;
};
