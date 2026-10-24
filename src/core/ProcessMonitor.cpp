#include "ProcessMonitor.h"

#include <QMetaObject>
#include <QTimer>

#include <algorithm>

class ProcessMonitor::Worker : public QObject {
    Q_OBJECT
public slots:
    void start(int intervalMs) {
        m_scanner = ProcessScanner(); // drop stale CPU baselines from before the pause
        if (!m_timer) {
            m_timer = new QTimer(this);
            connect(m_timer, &QTimer::timeout, this, &Worker::scan);
        }
        m_timer->start(intervalMs);
        scan();
    }
    void stop() {
        if (m_timer)
            m_timer->stop();
    }
    void setInterval(int intervalMs) {
        if (m_timer)
            m_timer->setInterval(intervalMs);
    }

signals:
    void scanned(const QVector<ProcessInfo> &processes);

private slots:
    void scan() { emit scanned(m_scanner.scan()); }

private:
    ProcessScanner m_scanner;
    QTimer *m_timer = nullptr;
};

ProcessMonitor::ProcessMonitor(QObject *parent) : QObject(parent), m_worker(new Worker) {
    qRegisterMetaType<QVector<ProcessInfo>>();
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &Worker::scanned, this, &ProcessMonitor::scanned, Qt::QueuedConnection);
    m_thread.start();
}

ProcessMonitor::~ProcessMonitor() {
    m_thread.quit();
    m_thread.wait();
}

void ProcessMonitor::setActive(bool active) {
    if (active == m_active)
        return;
    m_active = active;
    if (active)
        QMetaObject::invokeMethod(m_worker, "start", Qt::QueuedConnection, Q_ARG(int, m_intervalMs));
    else
        QMetaObject::invokeMethod(m_worker, "stop", Qt::QueuedConnection);
}

void ProcessMonitor::setInterval(int ms) {
    m_intervalMs = std::max(ms, 1000); // /proc scans are heavier than sensor reads
    QMetaObject::invokeMethod(m_worker, "setInterval", Qt::QueuedConnection, Q_ARG(int, m_intervalMs));
}

#include "ProcessMonitor.moc"
