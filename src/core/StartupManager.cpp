#include "StartupManager.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QStandardPaths>

namespace {

QString readFile(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

bool isTrue(const QString &v) {
    return v.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
}

} // namespace

StartupManager::StartupManager(QString userDir, QStringList systemDirs)
    : m_userDir(std::move(userDir)), m_systemDirs(std::move(systemDirs)) {}

QString StartupManager::defaultUserDir() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/autostart");
}

QStringList StartupManager::defaultSystemDirs() {
    QStringList dirs;
    for (const QString &d : QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation))
        dirs << d + QStringLiteral("/autostart");
    dirs.removeAll(defaultUserDir());
    return dirs;
}

QHash<QString, QString> StartupManager::parseDesktop(const QString &text) {
    QHash<QString, QString> out;
    bool inEntry = false;
    for (const QString &raw : text.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inEntry = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (!inEntry || line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        if (!out.contains(key))
            out.insert(key, line.mid(eq + 1).trimmed());
    }
    return out;
}

bool StartupManager::isEnabled(const QHash<QString, QString> &entry) {
    if (isTrue(entry.value(QStringLiteral("Hidden"))))
        return false;
    const QString gnome = entry.value(QStringLiteral("X-GNOME-Autostart-enabled"));
    return gnome.isEmpty() || isTrue(gnome);
}

QString StartupManager::withKey(const QString &text, const QString &key, const QString &value) {
    QStringList lines = text.split(QLatin1Char('\n'));
    const QString assignment = key + QLatin1Char('=') + value;
    int groupStart = -1, groupEnd = static_cast<int>(lines.size());
    bool replaced = false;
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            if (groupStart >= 0) {
                groupEnd = i;
                break;
            }
            if (line == QLatin1String("[Desktop Entry]"))
                groupStart = i;
        } else if (groupStart >= 0 && line.startsWith(key + QLatin1Char('='))) {
            lines[i] = assignment;
            replaced = true;
        }
    }
    if (!replaced) {
        if (groupStart < 0) {
            lines.prepend(QStringLiteral("[Desktop Entry]"));
            groupStart = 0;
            groupEnd = static_cast<int>(lines.size());
        }
        // Insert after the last non-empty line of the group.
        int at = groupEnd;
        while (at - 1 > groupStart && lines[at - 1].trimmed().isEmpty())
            --at;
        lines.insert(at, assignment);
    }
    return lines.join(QLatin1Char('\n'));
}

QVector<StartupEntry> StartupManager::scan() const {
    QVector<StartupEntry> out;
    QHash<QString, int> index;

    auto addDir = [&](const QString &dir, const QString &source) {
        const QStringList files = QDir(dir).entryList({QStringLiteral("*.desktop")}, QDir::Files, QDir::Name);
        for (const QString &file : files) {
            if (index.contains(file))
                continue; // an earlier (higher priority) directory already provided it
            const auto entry = parseDesktop(readFile(dir + QLatin1Char('/') + file));
            if (entry.value(QStringLiteral("Type")) != QLatin1String("Application") && !entry.contains(QStringLiteral("Exec")))
                continue;
            StartupEntry e;
            e.id = file;
            e.name = entry.value(QStringLiteral("Name"), file);
            e.comment = entry.value(QStringLiteral("Comment"));
            e.exec = entry.value(QStringLiteral("Exec"));
            e.source = source;
            e.enabled = isEnabled(entry);
            index.insert(file, static_cast<int>(out.size()));
            out.push_back(e);
        }
    };

    addDir(m_userDir, QStringLiteral("User"));
    for (const QString &dir : m_systemDirs)
        addDir(dir, QStringLiteral("System"));

    std::sort(out.begin(), out.end(), [](const StartupEntry &a, const StartupEntry &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return out;
}

bool StartupManager::setEnabled(const QString &id, bool enabled) {
    // Only plain file names; never let a caller escape the autostart directories.
    if (id.isEmpty() || id.contains(QLatin1Char('/')) || !id.endsWith(QLatin1String(".desktop")))
        return false;

    const QString userPath = m_userDir + QLatin1Char('/') + id;
    QString text = readFile(userPath);
    if (text.isEmpty()) {
        for (const QString &dir : m_systemDirs) {
            text = readFile(dir + QLatin1Char('/') + id);
            if (!text.isEmpty())
                break;
        }
    }
    if (text.isEmpty())
        return false;

    text = withKey(text, QStringLiteral("Hidden"), enabled ? QStringLiteral("false") : QStringLiteral("true"));
    text = withKey(text, QStringLiteral("X-GNOME-Autostart-enabled"),
                   enabled ? QStringLiteral("true") : QStringLiteral("false"));
    if (!text.endsWith(QLatin1Char('\n')))
        text += QLatin1Char('\n');

    if (!QDir().mkpath(m_userDir))
        return false;
    QFile f(userPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(text.toUtf8()) >= 0 && f.error() == QFileDevice::NoError;
}
