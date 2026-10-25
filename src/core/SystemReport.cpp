#include "SystemReport.h"

#include <QStringList>

namespace SystemReport {

namespace {

QString gb(double bytes) {
    return QStringLiteral("%1 GB").arg(bytes / 1e9, 0, 'f', bytes >= 1e11 ? 0 : 1);
}

void add(QStringList &lines, const QString &label, const QString &value) {
    if (!value.trimmed().isEmpty())
        lines << QStringLiteral("%1: %2").arg(label, value.trimmed());
}

} // namespace

QString format(const QVariantMap &d, const Environment &env) {
    QStringList lines;
    lines << QStringLiteral("Linux Hardware Monitor %1").arg(env.appVersion.isEmpty() ? QStringLiteral("(unknown version)") : env.appVersion);
    add(lines, QStringLiteral("Mode"), env.mode);
    add(lines, QStringLiteral("OS"), env.os);
    add(lines, QStringLiteral("Kernel"), env.kernel);
    add(lines, QStringLiteral("Qt"), env.qtVersion);
    add(lines, QStringLiteral("Renderer"), env.renderer);
    add(lines, QStringLiteral("Language"), env.language);
    lines << QString();

    QString cpu = d.value(QStringLiteral("cpuModel")).toString();
    if (!cpu.isEmpty()) {
        const int sockets = d.value(QStringLiteral("sockets")).toInt();
        const int cores = d.value(QStringLiteral("physicalCores")).toInt();
        const int threads = d.value(QStringLiteral("logicalCpus")).toInt();
        QStringList topology;
        if (sockets > 0) topology << QStringLiteral("%1 socket(s)").arg(sockets);
        if (cores > 0) topology << QStringLiteral("%1 cores").arg(cores);
        if (threads > 0) topology << QStringLiteral("%1 threads").arg(threads);
        if (!topology.isEmpty())
            cpu += QStringLiteral(" (%1)").arg(topology.join(QStringLiteral(", ")));
    }
    add(lines, QStringLiteral("CPU"), cpu);
    if (const double total = d.value(QStringLiteral("memTotalGb")).toDouble(); total > 0)
        add(lines, QStringLiteral("Memory"), QStringLiteral("%1 GB").arg(total, 0, 'f', 1));

    const QVariantList gpus = d.value(QStringLiteral("gpus")).toList();
    for (int i = 0; i < gpus.size(); ++i) {
        const QVariantMap g = gpus[i].toMap();
        QString text = g.value(QStringLiteral("name")).toString();
        const QVariantMap info = g.value(QStringLiteral("info")).toMap();
        if (const QString driver = info.value(QStringLiteral("driver")).toString(); !driver.isEmpty())
            text += QStringLiteral(" (driver %1)").arg(driver);
        add(lines, QStringLiteral("GPU %1").arg(i), text);
    }

    const QVariantList disks = d.value(QStringLiteral("disks")).toList();
    for (int i = 0; i < disks.size(); ++i) {
        const QVariantMap k = disks[i].toMap();
        QString text = k.value(QStringLiteral("model")).toString();
        if (text.isEmpty())
            text = k.value(QStringLiteral("name")).toString();
        if (const double size = k.value(QStringLiteral("sizeBytes")).toDouble(); size > 0)
            text += QStringLiteral(", %1").arg(gb(size));
        add(lines, QStringLiteral("Disk %1").arg(i), text); // no serial number
    }

    // Adapters with a model are real hardware; the rest (bridges, container and VPN interfaces) can number
    // in the dozens and say nothing about the machine, so they are only counted.
    QStringList nets;
    int others = 0;
    for (const QVariant &v : d.value(QStringLiteral("nets")).toList()) {
        const QVariantMap n = v.toMap();
        const QString model = n.value(QStringLiteral("model")).toString();
        const QString kind = n.value(QStringLiteral("kind")).toString();
        if (model.isEmpty())
            ++others;
        else
            nets << (kind.isEmpty() ? model : QStringLiteral("%1 (%2)").arg(model, kind));
    }
    if (others > 0)
        nets << QStringLiteral("%1 virtual or unidentified interface(s)").arg(others);
    if (!nets.isEmpty())
        add(lines, QStringLiteral("Network"), nets.join(QStringLiteral("; "))); // adapters, never names or addresses

    if (!d.value(QStringLiteral("batteries")).toList().isEmpty())
        add(lines, QStringLiteral("Battery"), QStringLiteral("present"));

    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

} // namespace SystemReport
