#include "QuitSignals.h"

#include <QSocketNotifier>

#include <csignal>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace {

int g_pipe[2] = {-1, -1};
constexpr int kSignals[] = {SIGINT, SIGTERM, SIGHUP};

void handler(int sig) {
    const int saved = errno;
    const unsigned char byte = static_cast<unsigned char>(sig);
    // Nothing else is allowed here; a full pipe just means a quit is already pending.
    ssize_t ignored = ::write(g_pipe[1], &byte, 1);
    (void)ignored;
    errno = saved;
}

} // namespace

QuitSignals::QuitSignals(QObject *parent) : QObject(parent) {
    if (::pipe(g_pipe) != 0)
        return;
    for (int fd : g_pipe) {
        ::fcntl(fd, F_SETFD, FD_CLOEXEC);
        ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
    }
    m_notifier = new QSocketNotifier(g_pipe[0], QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this] {
        unsigned char byte = 0;
        if (::read(g_pipe[0], &byte, 1) != 1)
            return;
        if (m_received == 0)
            m_received = byte;
        emit quitRequested(byte);
    });
    struct sigaction sa {};
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    for (int sig : kSignals)
        ::sigaction(sig, &sa, nullptr);
}

QuitSignals::~QuitSignals() {
    struct sigaction sa {};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    for (int sig : kSignals)
        ::sigaction(sig, &sa, nullptr);
    for (int &fd : g_pipe) {
        if (fd >= 0)
            ::close(fd);
        fd = -1;
    }
}
