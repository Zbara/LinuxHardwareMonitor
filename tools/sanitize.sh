#!/usr/bin/env bash
# Builds the project in a separate directory with a sanitizer and runs the tests.
#   tools/sanitize.sh address   # AddressSanitizer + UndefinedBehaviorSanitizer (default)
#   tools/sanitize.sh thread    # ThreadSanitizer
# A smoke run of the app itself (real data, then a simulation) follows, closing the window the normal way.
set -euo pipefail

kind="${1:-address}"
root="$(cd "$(dirname "$0")/.." && pwd)"
build="${TMPDIR:-/tmp}/hwmon-sanitize-$kind"

case "$kind" in
  address) flags="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined" ;;
  thread)  flags="-fsanitize=thread -fno-omit-frame-pointer" ;;
  *) echo "usage: $0 [address|thread]" >&2; exit 2 ;;
esac

cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="$flags" -DCMAKE_EXE_LINKER_FLAGS="$flags" >/dev/null
cmake --build "$build"

# QtTest's watchdog thread is reported as a "thread leak" by TSan; Qt/libdbus/glib are not instrumented,
# so TSan also reports races inside them (queued-signal argument hand-over, tzset, ...). Look for
# reports whose stacks are in this project's code.
export ASAN_OPTIONS="detect_leaks=0"
export TSAN_OPTIONS="report_thread_leaks=0"
(cd "$build" && ctest --output-on-failure)

export QT_QPA_PLATFORM=offscreen XDG_CONFIG_HOME="$build/config"
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1"
for args in "--page 1 --close-after 5000" \
            "--mock $root/mocks/stress-test.json --mock-speed 20 --run-scenario 0 --close-after 5000"; do
  echo "== app: $args"
  timeout 90 "$build/LinuxHardwareMonitor" $args
done
echo "sanitizer ($kind) run finished"
