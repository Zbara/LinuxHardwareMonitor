#pragma once

#include <QObject>

class QSocketNotifier;

// Turns SIGINT, SIGTERM and SIGHUP (Ctrl+C in a terminal, `kill`, the end of a login session) into a
// normal QCoreApplication::quit(), so the history is saved and threads finish cleanly. The handler
// itself only writes a byte to a pipe, the one thing that is safe in a signal handler.
class QuitSignals : public QObject {
    Q_OBJECT
public:
    explicit QuitSignals(QObject *parent = nullptr);
    ~QuitSignals() override;

    // Number of the signal received first, 0 until then (for the exit message and the tests).
    int received() const { return m_received; }

signals:
    void quitRequested(int signal);

private:
    QSocketNotifier *m_notifier = nullptr;
    int m_received = 0;
};
