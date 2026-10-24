#pragma once

#include <QString>

// XDG autostart entry (~/.config/autostart/*.desktop) for this application.
class Autostart {
public:
    // `dir` and `execPath` are injectable for tests.
    explicit Autostart(QString dir = defaultDir(), QString execPath = {});

    bool isEnabled() const;
    bool setEnabled(bool enabled);

    static QString defaultDir();
    QString filePath() const;

private:
    QString m_dir;
    QString m_exec;
};
