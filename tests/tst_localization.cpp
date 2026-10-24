#include "core/Localization.h"

#include <QCoreApplication>
#include <QtTest>

namespace {
void write(const QString &path, const QByteArray &content) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}
} // namespace

class TstLocalization : public QObject {
    Q_OBJECT
private slots:
    void jsonTranslatorLooksUpBySourceText() {
        JsonTranslator t;
        QVERIFY(t.loadJson(R"({"Hello": "Привет", "Todo": "", "%1 items": "%1 элементов"})"));
        QCOMPARE(t.translate("AnyContext", "Hello"), QString("Привет"));
        QCOMPARE(t.translate("Other", "%1 items"), QString("%1 элементов"));
        // Missing or empty entries fall through to the source text.
        QVERIFY(t.translate("X", "Unknown").isEmpty());
        QVERIFY(t.translate("X", "Todo").isEmpty());
        QCOMPARE(t.size(), 2);
    }

    void rejectsBrokenJson() {
        JsonTranslator t;
        QVERIFY(!t.loadJson("not json"));
        QVERIFY(!t.loadJson("[1,2,3]"));
        QVERIFY(t.isEmpty());
    }

    void resolvesLanguage() {
        QCOMPARE(Localization::resolve("de", {"ru-RU"}), QString("de")); // explicit choice wins
        QCOMPARE(Localization::resolve("", {"ru-RU", "en-US"}), QString("ru"));
        QCOMPARE(Localization::resolve("", {"uk_UA.UTF-8"}), QString("uk"));
        QCOMPARE(Localization::resolve("", {"ja-JP", "es-MX"}), QString("es")); // first supported one
        QCOMPARE(Localization::resolve("", {"ja-JP"}), QString("en"));
        QCOMPARE(Localization::resolve("", {}), QString("en"));
        QCOMPARE(Localization::resolve("xx", {"ru"}), QString("en")); // unknown explicit code
    }

    void switchesTranslationsAtRuntime() {
        QTemporaryDir dir;
        write(dir.path() + "/ru.json", R"({"Memory": "Память"})");
        write(dir.path() + "/de.json", R"({"Memory": "Arbeitsspeicher"})");

        Localization loc(dir.path());
        QCOMPARE(QCoreApplication::translate("Any", "Memory"), QString("Memory"));

        QCOMPARE(loc.setLanguage("ru"), QString("ru"));
        QCOMPARE(QCoreApplication::translate("Any", "Memory"), QString("Память"));

        QCOMPARE(loc.setLanguage("de"), QString("de"));
        QCOMPARE(QCoreApplication::translate("Any", "Memory"), QString("Arbeitsspeicher"));

        QCOMPARE(loc.setLanguage("en"), QString("en"));
        QCOMPARE(QCoreApplication::translate("Any", "Memory"), QString("Memory"));
    }

    void missingFileKeepsSourceLanguage() {
        QTemporaryDir dir; // no uk.json inside
        Localization loc(dir.path());
        loc.setLanguage("uk");
        QCOMPARE(QCoreApplication::translate("Any", "Memory"), QString("Memory"));
    }

    void shippedLanguagesAreListed() {
        QStringList codes;
        for (const auto &l : Localization::languages())
            codes << l.code;
        QVERIFY(codes.contains("en"));
        QVERIFY(codes.contains("ru"));
    }
};

QTEST_GUILESS_MAIN(TstLocalization)
#include "tst_localization.moc"
