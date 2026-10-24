#pragma once

#include <QMetaType>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVector>

// Installed RAM modules as described by the firmware (SMBIOS type 17).
struct MemoryModule {
    QString locator;      // e.g. "Controller0-ChannelA-DIMM0"
    QString bankLocator;
    qint64 sizeMb = 0;    // 0 for an empty slot
    QString type;         // DDR5
    QString typeDetail;   // Synchronous
    QString formFactor;   // SODIMM / DIMM
    int speedMts = 0;     // configured speed, else the rated speed
    int ratedSpeedMts = 0;
    QString manufacturer;
    QString partNumber;
    int rank = 0;
};
Q_DECLARE_METATYPE(MemoryModule)

struct MemoryInfo {
    int slotsTotal = 0;          // from the memory array (type 16), else the number of devices listed
    QVector<MemoryModule> modules; // installed modules only
    QString errorCorrection;     // "None", "Multi-bit ECC", ...
    int slotsUsed() const { return static_cast<int>(modules.size()); }
    bool valid() const { return slotsTotal > 0 || !modules.isEmpty(); }
};
Q_DECLARE_METATYPE(MemoryInfo)

namespace DmiMemory {
// Parses `dmidecode -t memory` output. Empty (invalid) result for anything unrecognisable.
MemoryInfo parse(const QString &dmidecodeOutput);
} // namespace DmiMemory

// Runs `pkexec dmidecode -t memory` (polkit asks the user for the administrator password) off the
// GUI thread's critical path. The command is injectable so tests can use a harmless script.
class DmiMemoryReader : public QObject {
    Q_OBJECT
public:
    explicit DmiMemoryReader(QString program = QStringLiteral("pkexec"),
                             QStringList arguments = {QStringLiteral("dmidecode"), QStringLiteral("-t"), QStringLiteral("memory")},
                             QObject *parent = nullptr);
    ~DmiMemoryReader() override;

    bool running() const { return m_process.state() != QProcess::NotRunning; }
    void start();

    enum class Failure { Cancelled, NotAvailable, Failed, NoData };
    Q_ENUM(Failure)

signals:
    void finished(const MemoryInfo &info);
    void failed(DmiMemoryReader::Failure kind, const QString &message);

private:
    void onFinished(int exitCode, QProcess::ExitStatus status);

    QString m_program;
    QStringList m_arguments;
    QProcess m_process;
    bool m_startFailed = false;
};
