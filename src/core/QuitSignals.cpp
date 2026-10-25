#include "QuitSignals.h"

#include <QSocketNotifier>

#include <atomic>
#include <csignal>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace {

int g_pipe[2] = {-1, -1};
constexpr int kSignals[] = {SIGINT, SIGTERM, SIGHUP};

// How many signals arrived. A lock-free atomic is safe to use in a signal handler (incrementing a volatile is
// deprecated in C++20).
std::atomic<int> g_count{0};
static_assert(std::atomic<int>::is_always_lock_free);

void handler(int sig) {
    // A second signal means the first did not get the program out (a stuck thread, a blocked call):
    // give up on the orderly shutdown and let the default action end the process.
    if (g_count.fetch_add(1) >= 1) {
        ::signal(sig, SIG_DFL);
        ::raise(sig);
        return;
    }
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
    g_count = 0;
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
