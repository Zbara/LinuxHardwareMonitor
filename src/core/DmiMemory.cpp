#include "DmiMemory.h"

#include <QRegularExpression>

namespace DmiMemory {

namespace {

// "16 GB" / "8192 MB" / "512 kB" -> MB; 0 when there is no module.
qint64 parseSizeMb(const QString &text) {
    static const QRegularExpression re(QStringLiteral("^(\\d+)\\s*(TB|GB|MB|kB)$"));
    const auto m = re.match(text.trimmed());
    if (!m.hasMatch())
        return 0;
    const qint64 n = m.captured(1).toLongLong();
    const QString unit = m.captured(2);
    return unit == QLatin1String("TB") ? n * 1024 * 1024 : unit == QLatin1String("GB") ? n * 1024 : unit == QLatin1String("MB") ? n : n / 1024;
}

// "5200 MT/s" or "2400 MHz" -> number; 0 for "Unknown".
int parseSpeed(const QString &text) {
    static const QRegularExpression re(QStringLiteral("^(\\d+)"));
    const auto m = re.match(text.trimmed());
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

QString clean(const QString &v) {
    const QString t = v.trimmed();
    static const QStringList placeholders = {QStringLiteral("Unknown"), QStringLiteral("Not Specified"), QStringLiteral("None"),
                                             QStringLiteral("To Be Filled By O.E.M."), QStringLiteral("Undefined"), QStringLiteral("N/A")};
    return placeholders.contains(t, Qt::CaseInsensitive) ? QString() : t;
}

} // namespace

MemoryInfo parse(const QString &text) {
    MemoryInfo info;
    int devicesListed = 0;

    // Blocks are separated by blank lines; the second line names the block ("Memory Device").
    const QStringList blocks = text.split(QRegularExpression(QStringLiteral("\\n\\s*\\n")));
    for (const QString &block : blocks) {
        const QStringList lines = block.split(QLatin1Char('\n'));
        QString title;
        QHash<QString, QString> kv;
        for (const QString &raw : lines) {
            if (raw.startsWith(QLatin1Char('\t')) || raw.startsWith(QLatin1String("    "))) {
                const QString line = raw.trimmed();
                const int colon = line.indexOf(QLatin1Char(':'));
                if (colon > 0)
                    kv.insert(line.left(colon).trimmed(), line.mid(colon + 1).trimmed());
            } else if (!raw.trimmed().isEmpty() && !raw.startsWith(QLatin1String("Handle")) && !raw.startsWith(QLatin1Char('#'))
                       && title.isEmpty()) {
                title = raw.trimmed();
            }
        }

        if (title == QLatin1String("Physical Memory Array")) {
            info.slotsTotal += kv.value(QStringLiteral("Number Of Devices")).toInt();
            if (info.errorCorrection.isEmpty())
                info.errorCorrection = kv.value(QStringLiteral("Error Correction Type"));
        } else if (title == QLatin1String("Memory Device")) {
            ++devicesListed;
            MemoryModule m;
            m.sizeMb = parseSizeMb(kv.value(QStringLiteral("Size")));
            if (m.sizeMb <= 0)
                continue; // "No Module Installed"
            m.locator = kv.value(QStringLiteral("Locator"));
            m.bankLocator = clean(kv.value(QStringLiteral("Bank Locator")));
            m.type = clean(kv.value(QStringLiteral("Type")));
            m.typeDetail = clean(kv.value(QStringLiteral("Type Detail")));
            m.formFactor = clean(kv.value(QStringLiteral("Form Factor")));
            m.ratedSpeedMts = parseSpeed(kv.value(QStringLiteral("Speed")));
            const int configured = parseSpeed(kv.value(QStringLiteral("Configured Memory Speed"), kv.value(QStringLiteral("Configured Clock Speed"))));
            m.speedMts = configured > 0 ? configured : m.ratedSpeedMts;
            m.manufacturer = clean(kv.value(QStringLiteral("Manufacturer")));
            m.partNumber = clean(kv.value(QStringLiteral("Part Number")));
            m.rank = kv.value(QStringLiteral("Rank")).toInt();
            info.modules.push_back(m);
        }
    }
    if (info.slotsTotal <= 0)
        info.slotsTotal = devicesListed; // no array block: the listed devices are all the slots we know of
    return info;
}

} // namespace DmiMemory

DmiMemoryReader::DmiMemoryReader(QString program, QStringList arguments, QObject *parent)
    : QObject(parent), m_program(std::move(program)), m_arguments(std::move(arguments)) {
    qRegisterMetaType<MemoryInfo>();
    connect(&m_process, &QProcess::finished, this, &DmiMemoryReader::onFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_startFailed = true;
        emit failed(Failure::NotAvailable, tr("%1 could not be started").arg(m_program));
    });
}

DmiMemoryReader::~DmiMemoryReader() {
    if (running()) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

void DmiMemoryReader::start() {
    if (running())
        return;
    m_startFailed = false;
    m_process.start(m_program, m_arguments);
}

void DmiMemoryReader::onFinished(int exitCode, QProcess::ExitStatus status) {
    if (m_startFailed)
        return;
    // pkexec: 126 = the user dismissed the password dialog (or was not authorised), 127 = could not authenticate.
    if (status == QProcess::NormalExit && (exitCode == 126 || exitCode == 127)) {
        emit failed(Failure::Cancelled, tr("Authentication was cancelled or denied"));
        return;
    }
    const QString output = QString::fromUtf8(m_process.readAllStandardOutput());
    if (status != QProcess::NormalExit || exitCode != 0) {
        const QString err = QString::fromUtf8(m_process.readAllStandardError()).trimmed();
        emit failed(Failure::Failed, err.isEmpty() ? tr("The command failed (exit code %1)").arg(exitCode) : err);
        return;
    }
    const MemoryInfo info = DmiMemory::parse(output);
    if (!info.valid()) {
        emit failed(Failure::NoData, tr("The firmware did not report any memory module information"));
        return;
    }
    emit finished(info);
}
