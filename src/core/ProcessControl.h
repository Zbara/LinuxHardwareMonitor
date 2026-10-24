#pragma once

// Thin, testable wrappers around kill(2) and setpriority(2). They return 0 on success or an
// errno value, and refuse the targets that must never be signalled: pid <= 1 (init, or the
// whole process group / every process for kill(-1)) and this process itself.
#include <QVector>

namespace ProcessControl {

int sendSignal(int pid, bool force); // SIGTERM, or SIGKILL when force is set
int setPriority(int pid, int nice);  // nice is clamped to -20..19

// CPUs (logical processor indices) the process may run on; empty with *error set on failure.
QVector<int> affinity(int pid, int *error = nullptr);
// Restricts the process to `cpus`. An empty list is invalid (EINVAL).
int setAffinity(int pid, const QVector<int> &cpus);

} // namespace ProcessControl
