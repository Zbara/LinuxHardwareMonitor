#include "Notifier.h"

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QVariantMap>

QDBusMessage Notifier::buildMessage(const QString &summary, const QString &body) {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    msg << QStringLiteral("Linux Hardware Monitor") // app_name
        << uint(0)                                    // replaces_id
        << QStringLiteral("dialog-warning")           // app_icon
        << summary << body
        << QStringList()                              // actions
        << QVariantMap{{QStringLiteral("desktop-entry"), QStringLiteral("linux-hardware-monitor")},
                       {QStringLiteral("urgency"), QVariant::fromValue<uchar>(2)}} // critical
        << int(-1);                                   // expire_timeout: server default
    return msg;
}

void Notifier::send(const QString &summary, const QString &body) {
    QDBusConnection::sessionBus().asyncCall(buildMessage(summary, body));
}
