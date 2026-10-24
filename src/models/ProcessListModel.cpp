#include "ProcessListModel.h"

#include <algorithm>

namespace {

QString commonOwner(const QVector<const ProcessInfo *> &members) {
    QString owner = members.first()->user;
    for (const ProcessInfo *p : members)
        if (p->user != owner)
            return {};
    return owner;
}

} // namespace

ProcessListModel::ProcessListModel(QObject *parent) : QAbstractListModel(parent) {}

int ProcessListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant ProcessListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows[index.row()];
    switch (role) {
    case KeyRole:          return r.key;
    case IsGroupRole:      return r.isGroup;
    case HasChildrenRole:  return r.hasChildren;
    case PidRole:          return r.pid;
    case NameRole:         return r.name;
    case GroupNameRole:    return r.groupName;
    case CountRole:        return r.count;
    case OwnerRole:        return r.owner;
    case StateRole:        return r.state;
    case CpuRole:          return r.cpu;
    case MemoryRole:       return static_cast<qulonglong>(r.memory);
    case DiskRole:         return r.disk;
    case ThreadsRole:      return r.threads;
    case CmdlineRole:      return r.cmdline;
    case SearchRole:       return r.searchText;
    case GroupCpuRole:     return r.gCpu;
    case GroupMemoryRole:  return static_cast<qulonglong>(r.gMemory);
    case GroupDiskRole:    return r.gDisk;
    case GroupThreadsRole: return r.gThreads;
    case GroupOwnerRole:   return r.gOwner;
    }
    return {};
}

QHash<int, QByteArray> ProcessListModel::roleNames() const {
    return {
        {KeyRole, "key"},         {IsGroupRole, "isGroup"},   {HasChildrenRole, "hasChildren"},
        {PidRole, "pid"},         {NameRole, "name"},         {GroupNameRole, "groupName"},
        {CountRole, "count"},     {OwnerRole, "owner"},       {StateRole, "state"},
        {CpuRole, "cpu"},         {MemoryRole, "memory"},     {DiskRole, "disk"},
        {ThreadsRole, "threads"}, {CmdlineRole, "cmdline"},
    };
}

void ProcessListModel::update(const QVector<ProcessInfo> &processes) {
    QHash<QString, QVector<const ProcessInfo *>> groups;
    for (const ProcessInfo &p : processes)
        groups[p.name].push_back(&p);

    QVector<Row> next;
    QHash<QString, QVector<int>> groupPids;

    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        const QString &name = it.key();
        const auto &members = it.value();

        Row g;
        g.key = QStringLiteral("g:") + name;
        g.name = g.groupName = name;
        g.count = static_cast<int>(members.size());
        g.hasChildren = members.size() > 1;
        g.owner = g.gOwner = commonOwner(members);
        bool anyDisk = false;
        double disk = 0;
        for (const ProcessInfo *p : members) {
            g.cpu += p->cpuPercent;
            g.memory += p->rssBytes;
            g.threads += p->threads;
            if (p->diskBytesPerSec >= 0) {
                disk += p->diskBytesPerSec;
                anyDisk = true;
            }
            groupPids[name].push_back(p->pid);
        }
        g.disk = anyDisk ? disk : -1;
        g.gCpu = g.cpu;
        g.gMemory = g.memory;
        g.gDisk = g.disk;
        g.gThreads = g.threads;
        g.searchText = name.toLower();
        g.nameLower = name.toLower();
        {
            QStringList owners;
            for (const ProcessInfo *p : members) {
                if (!owners.contains(p->user))
                    owners << p->user;
                g.pids.push_back(p->pid);
                g.cmdlineLower += p->cmdline.toLower() + QLatin1Char('\n');
            }
            g.ownersLower = owners.join(QLatin1Char(' ')).toLower();
        }
        if (!g.hasChildren) {
            const ProcessInfo &p = *members.first();
            g.pid = p.pid;
            g.state = p.state;
            g.cmdline = p.cmdline;
            g.searchText += QLatin1Char(' ') + QString::number(p.pid) + QLatin1Char(' ')
                            + p.user.toLower() + QLatin1Char(' ') + p.cmdline.toLower();
        } else {
            g.searchText += QLatin1Char(' ') + g.owner.toLower();
        }

        QVector<Row> children;
        if (g.hasChildren) {
            for (const ProcessInfo *p : members) {
                Row c;
                c.key = QStringLiteral("p:") + QString::number(p->pid);
                c.isGroup = false;
                c.pid = p->pid;
                c.name = name;
                c.groupName = name;
                c.count = 1;
                c.owner = p->user;
                c.state = p->state;
                c.cpu = p->cpuPercent;
                c.memory = p->rssBytes;
                c.disk = p->diskBytesPerSec;
                c.threads = p->threads;
                c.cmdline = p->cmdline;
                c.searchText = (name + QLatin1Char(' ') + QString::number(p->pid) + QLatin1Char(' ')
                                + p->user + QLatin1Char(' ') + p->cmdline).toLower();
                c.nameLower = name.toLower();
                c.ownersLower = p->user.toLower();
                c.cmdlineLower = p->cmdline.toLower();
                c.pids = {p->pid};
                c.gCpu = g.gCpu;
                c.gMemory = g.gMemory;
                c.gDisk = g.gDisk;
                c.gThreads = g.gThreads;
                c.gOwner = g.gOwner;
                children.push_back(c);
                g.searchText += QLatin1Char(' ') + QString::number(p->pid) + QLatin1Char(' ')
                                + p->cmdline.toLower();
            }
        }
        next.push_back(g);
        next += children;
    }
    m_groupPids = groupPids;

    QHash<QString, int> nextIndex;
    for (int i = 0; i < next.size(); ++i)
        nextIndex.insert(next[i].key, i);

    // Remove rows that are gone, bottom-up.
    for (int row = static_cast<int>(m_rows.size()) - 1; row >= 0; --row) {
        if (nextIndex.contains(m_rows[row].key))
            continue;
        beginRemoveRows({}, row, row);
        m_rows.removeAt(row);
        endRemoveRows();
    }
    m_index.clear();
    for (int i = 0; i < m_rows.size(); ++i)
        m_index.insert(m_rows[i].key, i);

    QVector<Row> added;
    for (const Row &r : std::as_const(next)) {
        const auto it = m_index.constFind(r.key);
        if (it == m_index.constEnd()) {
            added.push_back(r);
        } else {
            m_rows[it.value()] = r;
            emit dataChanged(index(it.value()), index(it.value()));
        }
    }
    if (!added.isEmpty()) {
        const int first = static_cast<int>(m_rows.size());
        beginInsertRows({}, first, first + static_cast<int>(added.size()) - 1);
        m_rows += added;
        endInsertRows();
        for (int i = first; i < m_rows.size(); ++i)
            m_index.insert(m_rows[i].key, i);
    }
}

bool ProcessListModel::matches(int row, const QString &q) const {
    if (q.isEmpty())
        return true;
    if (row < 0 || row >= m_rows.size())
        return false;
    const Row &r = m_rows[row];
    if (r.nameLower.contains(q) || r.ownersLower.contains(q))
        return true;
    bool isPid = false;
    const int pid = q.toInt(&isPid);
    if (isPid && r.pids.contains(pid))
        return true;
    return q.size() >= 3 && r.cmdlineLower.contains(q);
}

QVariantList ProcessListModel::pidsFor(const QString &key) const {
    QVariantList out;
    if (key.startsWith(QLatin1String("g:"))) {
        for (int pid : m_groupPids.value(key.mid(2)))
            out << pid;
    } else if (key.startsWith(QLatin1String("p:"))) {
        out << key.mid(2).toInt();
    }
    return out;
}

// ---------------------------------------------------------------------------

ProcessProxyModel::ProcessProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
}

QHash<int, QByteArray> ProcessProxyModel::roleNames() const {
    QHash<int, QByteArray> roles = sourceModel() ? sourceModel()->roleNames() : QHash<int, QByteArray>();
    roles.insert(ExpandedRole, "expanded");
    roles.insert(LevelRole, "level");
    return roles;
}

QVariant ProcessProxyModel::data(const QModelIndex &index, int role) const {
    if (role == ExpandedRole)
        return expanded(QSortFilterProxyModel::data(index, ProcessListModel::GroupNameRole).toString());
    if (role == LevelRole)
        return QSortFilterProxyModel::data(index, ProcessListModel::IsGroupRole).toBool() ? 0 : 1;
    return QSortFilterProxyModel::data(index, role);
}

bool ProcessProxyModel::expanded(const QString &groupName) const {
    // While searching, show every matching child.
    return !m_filter.isEmpty() || m_expanded.contains(groupName);
}

void ProcessProxyModel::setFilterText(const QString &text) {
    if (text == m_filter)
        return;
    m_filter = text.trimmed().toLower();
    invalidateFilter();
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {ExpandedRole});
    emit filterTextChanged();
}

void ProcessProxyModel::toggleExpanded(const QString &groupName) {
    if (!m_expanded.remove(groupName))
        m_expanded.insert(groupName);
    invalidateFilter();
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {ExpandedRole});
}

QVariantList ProcessProxyModel::pidsFor(const QString &key) const {
    const auto *src = qobject_cast<const ProcessListModel *>(sourceModel());
    return src ? src->pidsFor(key) : QVariantList();
}

void ProcessProxyModel::sortBy(const QString &key) {
    if (key == m_key) {
        m_descending = !m_descending;
    } else {
        m_key = key;
        // Numbers are most useful biggest-first, text A-Z.
        m_descending = key != QLatin1String("name") && key != QLatin1String("owner");
    }
    resort();
    emit sortChanged();
}

void ProcessProxyModel::resort() {
    // Direction is handled inside lessThan() so that children always stay
    // directly below their application.
    sort(-1);
    sort(0, Qt::AscendingOrder);
}

bool ProcessProxyModel::filterAcceptsRow(int row, const QModelIndex &parent) const {
    const QModelIndex idx = sourceModel()->index(row, 0, parent);
    const bool isGroup = sourceModel()->data(idx, ProcessListModel::IsGroupRole).toBool();
    if (m_filter.isEmpty())
        return isGroup || m_expanded.contains(sourceModel()->data(idx, ProcessListModel::GroupNameRole).toString());
    const auto *src = qobject_cast<const ProcessListModel *>(sourceModel());
    return src && src->matches(row, m_filter);
}

bool ProcessProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    const auto *m = sourceModel();
    auto get = [&](const QModelIndex &i, int role) { return m->data(i, role); };

    int cmp = 0;
    auto numeric = [&](int role) {
        const double a = get(left, role).toDouble(), b = get(right, role).toDouble();
        cmp = a < b ? -1 : a > b ? 1 : 0;
    };
    auto text = [&](int role) {
        cmp = get(left, role).toString().compare(get(right, role).toString(), Qt::CaseInsensitive);
    };

    if (m_key == QLatin1String("cpu"))          numeric(ProcessListModel::GroupCpuRole);
    else if (m_key == QLatin1String("memory"))  numeric(ProcessListModel::GroupMemoryRole);
    else if (m_key == QLatin1String("disk"))    numeric(ProcessListModel::GroupDiskRole);
    else if (m_key == QLatin1String("threads")) numeric(ProcessListModel::GroupThreadsRole);
    else if (m_key == QLatin1String("owner"))   text(ProcessListModel::GroupOwnerRole);
    else                                        text(ProcessListModel::GroupNameRole);
    if (cmp != 0)
        return m_descending ? cmp > 0 : cmp < 0;

    // Same sort value: keep each application together, ordered by name.
    const int byName = get(left, ProcessListModel::GroupNameRole).toString().compare(
        get(right, ProcessListModel::GroupNameRole).toString(), Qt::CaseInsensitive);
    if (byName != 0)
        return byName < 0;

    // Within one application: the group row first, then its processes.
    const bool lg = get(left, ProcessListModel::IsGroupRole).toBool();
    const bool rg = get(right, ProcessListModel::IsGroupRole).toBool();
    if (lg != rg)
        return lg;

    if (m_key == QLatin1String("cpu"))          numeric(ProcessListModel::CpuRole);
    else if (m_key == QLatin1String("memory"))  numeric(ProcessListModel::MemoryRole);
    else if (m_key == QLatin1String("disk"))    numeric(ProcessListModel::DiskRole);
    else if (m_key == QLatin1String("threads")) numeric(ProcessListModel::ThreadsRole);
    else                                        cmp = 0;
    if (cmp != 0)
        return m_descending ? cmp > 0 : cmp < 0;
    return get(left, ProcessListModel::PidRole).toInt() < get(right, ProcessListModel::PidRole).toInt();
}
