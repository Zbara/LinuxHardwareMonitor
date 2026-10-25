#include "core/ThemeMode.h"

#include <QtTest>

using namespace ThemeMode;

class TstThemeMode : public QObject {
    Q_OBJECT
private slots:
    void fixedModesIgnoreTheDesktop() {
        for (SystemScheme s : {SystemScheme::Dark, SystemScheme::Light, SystemScheme::Unknown}) {
            QVERIFY(isDark("dark", s));
            QVERIFY(!isDark("light", s));
        }
    }

    void systemFollowsTheDesktopAndDefaultsToDark() {
        QVERIFY(isDark("system", SystemScheme::Dark));
        QVERIFY(!isDark("system", SystemScheme::Light));
        QVERIFY(isDark("system", SystemScheme::Unknown)); // no preference: the default theme
    }

    void unknownValuesAreDark() {
        QCOMPARE(normalize("garbage"), QString("dark"));
        QCOMPARE(normalize(""), QString("dark"));
        QCOMPARE(normalize("system"), QString("system"));
        QCOMPARE(normalize("light"), QString("light"));
        QVERIFY(isDark("garbage", SystemScheme::Light));
    }

    void theOldBooleanSettingIsKept() {
        QCOMPARE(fromLegacy(true), QString("dark"));
        QCOMPARE(fromLegacy(false), QString("light"));
    }
};

QTEST_GUILESS_MAIN(TstThemeMode)
#include "tst_thememode.moc"
