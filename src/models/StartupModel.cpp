#include "StartupModel.h"

StartupModel::StartupModel(StartupManager manager, QObject *parent)
    : QAbstractListModel(parent), m_manager(std::move(manager)) {}

int StartupModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant StartupModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const StartupEntry &e = m_rows[index.row()];
    switch (role) {
    case IdRole:      return e.id;
    case NameRole:    return e.name;
    case CommentRole: return e.comment;
    case ExecRole:    return e.exec;
    case SourceRole:  return e.source;
    case EnabledRole: return e.enabled;
    }
    return {};
}

QHash<int, QByteArray> StartupModel::roleNames() const {
    return {{IdRole, "id"},         {NameRole, "name"},     {CommentRole, "comment"},
            {ExecRole, "exec"},     {SourceRole, "source"}, {EnabledRole, "enabled"}};
}

int StartupModel::enabledCount() const {
    int n = 0;
    for (const StartupEntry &e : m_rows)
        n += e.enabled;
    return n;
}

void StartupModel::refresh() {
    beginResetModel();
    m_rows = m_manager.scan();
    endResetModel();
    emit countsChanged();
}

bool StartupModel::setEnabled(const QString &id, bool enabled) {
    if (!m_manager.setEnabled(id, enabled))
        return false;
    refresh(); // the entry may have moved from System to User
    return true;
}
