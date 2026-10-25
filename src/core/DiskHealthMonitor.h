#pragma once

#include "DiskHealth.h"

#include <QObject>
#include <QThread>

// Reads the SMART data of the drives from udisks2 on a thread of its own (an NVMe query is an admin
// command to the drive, so it is not done on the GUI thread) once a minute, and only while something
// shows it. Without udisks2, or for drives without SMART, there is simply no data.
class DiskHealthMonitor : public QObject {
    Q_OBJECT
public:
    explicit DiskHealthMonitor(QObject *parent = nullptr);
    ~DiskHealthMonitor() override;

    // Polls (at once, then every minute) only while active.
    void setActive(bool active);
    bool active() const { return m_active; }

signals:
    // Block device name ("nvme0n1") -> health.
    void updated(const DiskHealthMap &health);

private:
    class Worker;
    QThread m_thread;
    Worker *m_worker;
    bool m_active = false;
};
