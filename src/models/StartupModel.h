#pragma once

#include "core/StartupManager.h"

#include <QAbstractListModel>

class StartupModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCountInt NOTIFY countsChanged)
    Q_PROPERTY(int enabledCount READ enabledCount NOTIFY countsChanged)
public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, CommentRole, ExecRole, SourceRole, EnabledRole };

    explicit StartupModel(StartupManager manager = StartupManager(), QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int rowCountInt() const { return static_cast<int>(m_rows.size()); }
    int enabledCount() const;
    void refresh();
    // Returns false (and leaves the model unchanged) when the entry could not be written.
    bool setEnabled(const QString &id, bool enabled);

signals:
    void countsChanged();

private:
    StartupManager m_manager;
    QVector<StartupEntry> m_rows;
};
