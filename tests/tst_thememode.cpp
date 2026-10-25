#include "core/ThemeMode.h"

#include <QtTest>

using namespace ThemeMode;

class TstThemeMode : public QObject {
    Q_OBJECT
private slots:
    void fixedModesIgnoreTheDesktop() {
        for (Qt::ColorScheme s : {Qt::ColorScheme::Dark, Qt::ColorScheme::Light, Qt::ColorScheme::Unknown}) {
            QVERIFY(isDark("dark", s));
            QVERIFY(!isDark("light", s));
        }
    }

    void systemFollowsTheDesktopAndDefaultsToDark() {
        QVERIFY(isDark("system", Qt::ColorScheme::Dark));
        QVERIFY(!isDark("system", Qt::ColorScheme::Light));
        QVERIFY(isDark("system", Qt::ColorScheme::Unknown)); // no preference: the default theme
    }

    void unknownValuesAreDark() {
        QCOMPARE(normalize("garbage"), QString("dark"));
        QCOMPARE(normalize(""), QString("dark"));
        QCOMPARE(normalize("system"), QString("system"));
        QCOMPARE(normalize("light"), QString("light"));
        QVERIFY(isDark("garbage", Qt::ColorScheme::Light));
    }

    void theOldBooleanSettingIsKept() {
        QCOMPARE(fromLegacy(true), QString("dark"));
        QCOMPARE(fromLegacy(false), QString("light"));
    }
};

QTEST_GUILESS_MAIN(TstThemeMode)
#include "tst_thememode.moc"
