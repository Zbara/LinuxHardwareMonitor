#pragma once

#include "core/ProcessScanner.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QVariantList>

// One row per application (processes sharing a name are grouped, as in
// Windows Task Manager) plus one child row per process when a group has more
// than one member. Rows are keyed and updated incrementally so views keep
// scroll position and selection between scans.
class ProcessListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        KeyRole = Qt::UserRole + 1,
        IsGroupRole,
        HasChildrenRole,
        PidRole,
        NameRole,
        GroupNameRole,
        CountRole,
        OwnerRole,
        StateRole,
        CpuRole,
        MemoryRole,
        DiskRole,
        ThreadsRole,
        CmdlineRole,
        SearchRole,
        GroupCpuRole,
        GroupMemoryRole,
        GroupDiskRole,
        GroupThreadsRole,
        GroupOwnerRole,
        LastRole
    };

    struct Row {
        QString key;           // "g:<name>" for applications, "p:<pid>" for child processes
        bool isGroup = true;
        bool hasChildren = false;
        int pid = 0;           // 0 for multi-process groups
        QString name;
        QString groupName;
        int count = 1;
        QString owner;
        QString state;
        double cpu = 0;
        quint64 memory = 0;
        double disk = -1;      // -1: unknown
        int threads = 0;
        QString cmdline;
        QString nameLower;
        QString ownersLower;   // every distinct owner of the members, space separated
        QString cmdlineLower;  // command lines of all members
        QVector<int> pids;     // PIDs this row stands for
        QString searchText;    // lower-case
        double gCpu = 0;
        quint64 gMemory = 0;
        double gDisk = -1;
        int gThreads = 0;
        QString gOwner;
    };

    explicit ProcessListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void update(const QVector<ProcessInfo> &processes);

    // Search rule used by the process table: the query is matched against the name and the
    // owner(s); an all-digit query also matches a PID exactly (so "1" does not match every
    // PID containing a 1); command lines are searched only for queries of 3+ characters.
    bool matches(int row, const QString &lowerQuery) const;

    // PIDs behind a row key ("g:<name>" -> all members, "p:<pid>" -> that one).
    QVariantList pidsFor(const QString &key) const;

private:
    QVector<Row> m_rows;
    QHash<QString, int> m_index;
    QHash<QString, QVector<int>> m_groupPids;
};

// Hierarchical sort (children stay under their application), search filter and
// expand/collapse state.
class ProcessProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QString sortKey READ sortKey NOTIFY sortChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending NOTIFY sortChanged)
public:
    enum { ExpandedRole = ProcessListModel::LastRole + 1, LevelRole };

    explicit ProcessProxyModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const { return m_filter; }
    void setFilterText(const QString &text);
    QString sortKey() const { return m_key; }
    bool sortDescending() const { return m_descending; }

    // Clicking the same column again flips the direction.
    Q_INVOKABLE void sortBy(const QString &key);
    Q_INVOKABLE void toggleExpanded(const QString &groupName);
    Q_INVOKABLE QVariantList pidsFor(const QString &key) const;

signals:
    void filterTextChanged();
    void sortChanged();

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    bool expanded(const QString &groupName) const;
    void resort();

    QString m_filter;
    QString m_key = QStringLiteral("cpu");
    bool m_descending = true;
    QSet<QString> m_expanded;
};
