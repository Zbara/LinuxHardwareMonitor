#include "core/Notifier.h"

#include <QtTest>

class TstNotifier : public QObject {
    Q_OBJECT
private slots:
    void buildsNotifyCall() {
        const QDBusMessage msg = Notifier::buildMessage("Title", "Body");
        QCOMPARE(msg.service(), QString("org.freedesktop.Notifications"));
        QCOMPARE(msg.path(), QString("/org/freedesktop/Notifications"));
        QCOMPARE(msg.member(), QString("Notify"));
        const QList<QVariant> a = msg.arguments();
        QCOMPARE(a.size(), 8); // the Notify signature is (susssasa{sv}i)
        QCOMPARE(a[3].toString(), QString("Title"));
        QCOMPARE(a[4].toString(), QString("Body"));
        // These types decide the D-Bus signature: u, as, a{sv}, i
        QCOMPARE(a[1].metaType(), QMetaType::fromType<uint>());
        QCOMPARE(a[5].metaType(), QMetaType::fromType<QStringList>());
        QCOMPARE(a[6].metaType(), QMetaType::fromType<QVariantMap>());
        // The spec wants the urgency hint as a byte; a plain int is rejected by strict servers.
        QCOMPARE(a[6].toMap().value("urgency").metaType(), QMetaType::fromType<uchar>());
        QCOMPARE(a[7].metaType(), QMetaType::fromType<int>());
    }
};

QTEST_GUILESS_MAIN(TstNotifier)
#include "tst_notifier.moc"
