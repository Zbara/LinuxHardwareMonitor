#include "core/CommandLine.h"

#include <QFile>
#include <QRegularExpression>
#include <QtTest>

class TstCommandLine : public QObject {
    Q_OBJECT
private slots:
    void recognisesHelpAndVersion() {
        QVERIFY(CommandLine::wantsHelp({"--help"}));
        QVERIFY(CommandLine::wantsHelp({"--page", "1", "-h"}));
        QVERIFY(!CommandLine::wantsHelp({"--page", "1"}));
        QVERIFY(CommandLine::wantsVersion({"--version"}));
        QVERIFY(CommandLine::wantsVersion({"-v"}));
        QVERIFY(!CommandLine::wantsVersion({}));
    }

    void versionCarriesTheProjectVersion() {
        QVERIFY(CommandLine::version().startsWith("LinuxHardwareMonitor "));
        QVERIFY(!CommandLine::version().endsWith("unknown"));
    }

    void helpListsEveryFlag() {
        const QString text = CommandLine::help("prog");
        QVERIFY(text.startsWith("Usage: prog [options]"));
        for (const auto &f : CommandLine::flags())
            QVERIFY2(text.contains(QString::fromLatin1(f.name)), f.name);
    }

    // The table must not drift from main.cpp and the option parser: every flag the sources look
    // for appears in the help, and every documented flag is used somewhere.
    void tableMatchesTheSources() {
        const QString text = CommandLine::help("prog");
        QSet<QString> used;
        for (const QString &file : {QStringLiteral(SOURCE_DIR "/src/main.cpp")}) {
            QFile f(file);
            QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(file));
            const QString src = QString::fromUtf8(f.readAll());
            auto it = QRegularExpression(R"re("(--[a-z][a-z-]*)")re").globalMatch(src);
            while (it.hasNext())
                used.insert(it.next().captured(1));
        }
        QVERIFY(!used.isEmpty());
        for (const QString &flag : used)
            QVERIFY2(text.contains(flag + " ") || text.contains(flag + "\n") || text.contains(flag + ","),
                     qPrintable("undocumented flag " + flag));
    }
};

QTEST_GUILESS_MAIN(TstCommandLine)
#include "tst_commandline.moc"
