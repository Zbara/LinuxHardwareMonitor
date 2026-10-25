#include "ProcessControl.h"

#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>

namespace ProcessControl {

namespace {
bool refused(int pid) {
    return pid <= 1 || pid == static_cast<int>(getpid());
}
} // namespace

int sendSignal(int pid, bool force) {
    if (refused(pid))
        return EINVAL;
    return ::kill(pid, force ? SIGKILL : SIGTERM) == 0 ? 0 : errno;
}

int setSuspended(int pid, bool suspended) {
    if (refused(pid))
        return EINVAL;
    return ::kill(pid, suspended ? SIGSTOP : SIGCONT) == 0 ? 0 : errno;
}

int setPriority(int pid, int nice) {
    if (refused(pid))
        return EINVAL;
    errno = 0;
    return ::setpriority(PRIO_PROCESS, static_cast<id_t>(pid), std::clamp(nice, -20, 19)) == 0 ? 0 : errno;
}

QVector<int> affinity(int pid, int *error) {
    if (error)
        *error = 0;
    QVector<int> cpus;
    if (pid <= 0) {
        if (error)
            *error = EINVAL;
        return cpus;
    }
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(pid, sizeof set, &set) != 0) {
        if (error)
            *error = errno;
        return cpus;
    }
    for (int i = 0; i < CPU_SETSIZE; ++i)
        if (CPU_ISSET(i, &set))
            cpus.push_back(i);
    return cpus;
}

int setAffinity(int pid, const QVector<int> &cpus) {
    if (refused(pid) || cpus.isEmpty())
        return EINVAL;
    cpu_set_t set;
    CPU_ZERO(&set);
    for (int cpu : cpus) {
        if (cpu < 0 || cpu >= CPU_SETSIZE)
            return EINVAL;
        CPU_SET(cpu, &set);
    }
    return sched_setaffinity(pid, sizeof set, &set) == 0 ? 0 : errno;
}

} // namespace ProcessControl
