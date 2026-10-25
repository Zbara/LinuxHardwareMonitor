#include "ui/TrayIcon.h"

#include <QPainter>
#include <QtTest>

#include <cmath>

class TstTray : public QObject {
    Q_OBJECT
private slots:
    void colourFollowsTemperature() {
        QCOMPARE(TrayIcon::colorForTemperature(40), TrayIcon::colorForTemperature(69.9));
        QVERIFY(TrayIcon::colorForTemperature(75) != TrayIcon::colorForTemperature(50));
        QVERIFY(TrayIcon::colorForTemperature(90) != TrayIcon::colorForTemperature(75));
        QVERIFY(TrayIcon::colorForTemperature(85) != TrayIcon::colorForTemperature(84.9)); // boundary
    }

    void rendersAnIconWithTheRequestedColourAndText() {
        const QColor red = TrayIcon::colorForTemperature(95);
        const QImage icon = TrayIcon::render("95", red);
        QCOMPARE(icon.size(), QSize(64, 64));
        // The corner is transparent (rounded); the strip above the text is the background colour. (A point at the
        // side would be hit by wide fonts such as DejaVu Bold, whose two digits span almost the whole width.)
        QCOMPARE(qAlpha(icon.pixel(0, 0)), 0);
        const QColor edge = icon.pixelColor(32, 4);
        QCOMPARE(edge.alpha(), 255);
        QCOMPARE(edge.red(), red.red());
        // Text needs a font; a minimal container (CI) may have none, and then every digit is the same "missing glyph" box.
        const bool canDrawText = TrayIcon::render("1", red) != TrayIcon::render("8", red); // tofu boxes look alike
        if (canDrawText) {
            // Some white text pixels exist in the centre area.
            int white = 0;
            for (int y = 16; y < 48; ++y)
                for (int x = 12; x < 52; ++x)
                    white += icon.pixelColor(x, y).lightness() > 200;
            QVERIFY2(white > 30, qPrintable(QString::number(white)));
            // A different text renders differently.
            QVERIFY(TrayIcon::render("12", red) != icon);
        }
        // Three characters ("100") still fit inside the icon.
        QVERIFY(!TrayIcon::render("100", red).isNull());
    }

    void dumpSamplesWhenAskedTo() {
        // TRAY_DUMP_DIR=<dir> saves a contact sheet of sample icons for a visual check.
        const QString dir = qEnvironmentVariable("TRAY_DUMP_DIR");
        if (dir.isEmpty())
            QSKIP("set TRAY_DUMP_DIR to dump sample icons");
        QImage sheet(6 * 72, 72, QImage::Format_ARGB32);
        sheet.fill(QColor("#14171c"));
        QPainter p(&sheet);
        const QList<QPair<QString, double>> samples = {{"45", 45}, {"68", 68}, {"74", 74}, {"92", 92}, {"100", 100}, {"7", 20}};
        for (int i = 0; i < samples.size(); ++i)
            p.drawImage(4 + i * 72, 4, TrayIcon::render(samples[i].first, TrayIcon::colorForTemperature(samples[i].second)));
        p.end();
        QVERIFY(sheet.save(dir + "/tray-icons.png"));
    }

    void appIconIsDrawnInCodeAtAnySize() {
        for (int size : {22, 64, 256}) {
            const QImage icon = TrayIcon::renderAppIcon(size);
            QCOMPARE(icon.size(), QSize(size, size));
            QCOMPARE(qAlpha(icon.pixel(0, 0)), 0);                       // rounded corner is transparent
            QVERIFY(icon.pixelColor(size / 2, size / 6).alpha() > 200);  // the dark tile is opaque
        }
        // It really contains the blue load curve (accent colour pixels).
        const QImage icon = TrayIcon::renderAppIcon(256);
        int blue = 0;
        for (int y = 60; y < 200; ++y)
            for (int x = 40; x < 216; ++x) {
                const QColor c = icon.pixelColor(x, y);
                blue += c.blue() > 200 && c.red() < 120;
            }
        QVERIFY2(blue > 200, qPrintable(QString::number(blue)));
        QVERIFY(TrayIcon::renderAppIcon(64) != TrayIcon::render("70", TrayIcon::colorForTemperature(70)));
    }

    void dumpAppIconWhenAskedTo() {
        const QString dir = qEnvironmentVariable("TRAY_DUMP_DIR");
        if (dir.isEmpty())
            QSKIP("set TRAY_DUMP_DIR to dump the app icon");
        QImage sheet(3 * 80 + 8, 88, QImage::Format_ARGB32);
        sheet.fill(QColor("#2b2b2b"));
        QPainter p(&sheet);
        p.drawImage(4, 4, TrayIcon::renderAppIcon(22));
        p.drawImage(36, 4, TrayIcon::renderAppIcon(48));
        p.drawImage(96, 4, TrayIcon::renderAppIcon(80));
        p.end();
        QVERIFY(sheet.save(dir + "/app-icon.png"));
    }

    void customSizeIsHonoured() {
        QCOMPARE(TrayIcon::render("7", Qt::blue, 22).size(), QSize(22, 22));
    }

    void fahrenheitIsConvertedForDisplayOnly() {
        QCOMPARE(TrayIcon::toFahrenheit(0), 32.0);
        QCOMPARE(TrayIcon::toFahrenheit(100), 212.0);
        QCOMPARE(TrayIcon::toFahrenheit(-40), -40.0);
        QCOMPARE(TrayIcon::tooltip("App", 12.0, 67.2, 38.0, true), QString("App\nCPU 12 % · 153 °F · RAM 38 %"));
        QCOMPARE(TrayIcon::tooltip("App", 12.0, NAN, 38.0, true), QString("App\nCPU 12 % · RAM 38 %")); // still nothing invented
    }

    void tooltipOmitsUnknownValues() {
        QCOMPARE(TrayIcon::tooltip("App", 12.4, 67.2, 38.0), QString("App\nCPU 12 % · 67 °C · RAM 38 %"));
        QCOMPARE(TrayIcon::tooltip("App", 12.0, NAN, NAN), QString("App\nCPU 12 %"));
        QCOMPARE(TrayIcon::tooltip("App", NAN, NAN, NAN), QString("App")); // nothing invented, nothing shown as 0
    }
};

QTEST_MAIN(TstTray)
#include "tst_tray.moc"
