#!/usr/bin/env python3
"""Starts the real application once for every page and fails when the QML does not load or prints a QML error.

The unit tests never load the interface, so a missing QML module, a wrong resource path or a type that does not
exist in an older Qt (the window then simply closes with exit code 1) would pass them all. This is the check for
that. Pages are loaded lazily, so every page is visited.

usage: smoke_pages.py <path to LinuxHardwareMonitor>
"""
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

PAGES = range(7)  # Performance, Processes, Services, Startup, Sensors, Overview, Settings
# Messages of the QML engine; harmless platform noise (portals, XDG_RUNTIME_DIR, GL) is not matched.
QML_PROBLEM = re.compile(
    r"failed to load component|is not installed|module .* is not|is not a type|Unable to assign|"
    r"TypeError|ReferenceError|Cannot read property|Cannot assign|Binding loop|qrc:/.*(Error|error)|"
    r"QML .*(error|Error)|No such file or directory|Syntax error")


def run(binary, page):
    with tempfile.TemporaryDirectory() as home:
        env = dict(os.environ, QT_QPA_PLATFORM="offscreen", XDG_CONFIG_HOME=home + "/c", XDG_DATA_HOME=home + "/d",
                   XDG_RUNTIME_DIR=home)
        os.chmod(home, 0o700)
        try:
            p = subprocess.run([binary, "--page", str(page), "--close-after", "3000"], env=env,
                               capture_output=True, text=True, timeout=60)
        except subprocess.TimeoutExpired:
            return page, ["did not close within 60 s"]
        problems = [l for l in (p.stdout + p.stderr).splitlines() if QML_PROBLEM.search(l)]
        if p.returncode != 0:
            problems.insert(0, f"exit code {p.returncode}")
        return page, problems


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    binary = sys.argv[1]
    failed = False
    with ThreadPoolExecutor(max_workers=len(PAGES)) as pool:
        for page, problems in pool.map(lambda p: run(binary, p), PAGES):
            print(f"page {page}: {'ok' if not problems else 'FAILED'}")
            for line in problems:
                print("    " + line)
            failed |= bool(problems)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
