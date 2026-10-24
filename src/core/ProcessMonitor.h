#pragma once

#include "ProcessScanner.h"

#include <QObject>
#include <QThread>

// Scans /proc on a worker thread, but only while active (i.e. while the
// process page is visible), so an idle monitor costs nothing.
class ProcessMonitor : public QObject {
    Q_OBJECT
public:
    explicit ProcessMonitor(QObject *parent = nullptr);
    ~ProcessMonitor() override;

    void setActive(bool active);
    bool active() const { return m_active; }
    void setInterval(int ms);

signals:
    void scanned(const QVector<ProcessInfo> &processes);

private:
    class Worker;
    QThread m_thread;
    Worker *m_worker;
    bool m_active = false;
    int m_intervalMs = 1000;
};
