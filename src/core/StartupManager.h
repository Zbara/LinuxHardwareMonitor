#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

struct StartupEntry {
    QString id;        // desktop file name, e.g. "foo.desktop"
    QString name;
    QString comment;
    QString exec;
    QString source;    // "User" or "System"
    bool enabled = true;
};
Q_DECLARE_METATYPE(StartupEntry)

// Lists and toggles XDG autostart entries (like Task Manager's Startup tab).
// A user entry overrides a system entry of the same file name; disabling a
// system entry writes an override into the user directory, never touching /etc.
class StartupManager {
public:
    explicit StartupManager(QString userDir = defaultUserDir(),
                            QStringList systemDirs = defaultSystemDirs());

    QVector<StartupEntry> scan() const;
    bool setEnabled(const QString &id, bool enabled);

    static QString defaultUserDir();
    static QStringList defaultSystemDirs();

    // Parses the [Desktop Entry] group into key/value pairs (first occurrence wins).
    static QHash<QString, QString> parseDesktop(const QString &text);
    // Returns `text` with `key=value` set inside [Desktop Entry] (added if missing).
    static QString withKey(const QString &text, const QString &key, const QString &value);
    static bool isEnabled(const QHash<QString, QString> &entry);

private:
    QString m_userDir;
    QStringList m_systemDirs;
};
