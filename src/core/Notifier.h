#pragma once

#include <QDBusMessage>
#include <QString>

// Desktop notifications through org.freedesktop.Notifications (no extra libraries, works on GNOME, KDE, ...).
// Failures are silent: a desktop without a notification daemon simply shows nothing.
class Notifier {
public:
    static QDBusMessage buildMessage(const QString &summary, const QString &body);
    static void send(const QString &summary, const QString &body);
};
