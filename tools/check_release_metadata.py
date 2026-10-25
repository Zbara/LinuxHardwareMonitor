#!/usr/bin/env python3
"""Checks that the version is the same everywhere it is written down.

CMakeLists.txt (project VERSION) is the source of truth. The Debian changelog must start with
<version>-<revision>, the AppStream file must list that version as a release, and the manual page
header must mention it, and so must the RPM spec and the PKGBUILD. Forgetting one of them gives a package that lintian or the software centre
rejects, or one that reports the wrong version.
"""
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
errors = []

cmake = (root / "CMakeLists.txt").read_text()
m = re.search(r"project\(LinuxHardwareMonitor VERSION (\d+\.\d+\.\d+)", cmake)
if not m:
    sys.exit("CMakeLists.txt: no 'project(LinuxHardwareMonitor VERSION X.Y.Z' found")
version = m.group(1)
rev = re.search(r"set\(HWMON_DEB_REVISION (\d+)\)", cmake)
revision = rev.group(1) if rev else "1"

changelog = (root / "packaging/changelog").read_text()
head = re.match(r"linux-hardware-monitor \(([^)]+)\) ", changelog)
if not head:
    errors.append("packaging/changelog: first line is not 'linux-hardware-monitor (<version>) ...'")
elif head.group(1) != f"{version}-{revision}":
    errors.append(f"packaging/changelog: top entry is {head.group(1)}, expected {version}-{revision}")

meta = (root / "packaging/io.github.zbara.LinuxHardwareMonitor.metainfo.xml").read_text()
releases = re.findall(r'<release version="([^"]+)"', meta)
if not releases or releases[0] != version:
    errors.append(f"metainfo: newest release is {releases[0] if releases else 'missing'}, expected {version}")

man = (root / "packaging/LinuxHardwareMonitor.1").read_text()
if f"linux-hardware-monitor {version}" not in man.splitlines()[0]:
    errors.append(f"packaging/LinuxHardwareMonitor.1: the .TH line does not mention {version}")

spec = (root / "packaging/linux-hardware-monitor.spec").read_text()
sv = re.search(r"^Version:\s*(\S+)", spec, re.M)
if not sv or sv.group(1) != version:
    errors.append(f"packaging/linux-hardware-monitor.spec: Version is {sv.group(1) if sv else 'missing'}, expected {version}")
top = re.search(r"^%changelog\s*\n\* (\w{3} \w{3} \d+ \d{4}) .* - (\S+)", spec, re.M)
if not top:
    errors.append("packaging/linux-hardware-monitor.spec: no %changelog entry")
else:
    import datetime
    try:
        datetime.datetime.strptime(top.group(1), "%a %b %d %Y")
    except ValueError:
        errors.append(f"packaging/linux-hardware-monitor.spec: '{top.group(1)}' is not a valid date with the right weekday")
    if not top.group(2).startswith(version + "-"):
        errors.append(f"packaging/linux-hardware-monitor.spec: newest %changelog entry is {top.group(2)}, expected {version}-*")

pkgbuild = (root / "packaging/arch/PKGBUILD").read_text()
pv = re.search(r"^pkgver=(\S+)", pkgbuild, re.M)
if not pv or pv.group(1) != version:
    errors.append(f"packaging/arch/PKGBUILD: pkgver is {pv.group(1) if pv else 'missing'}, expected {version}")

# Author and website: ProjectInfo.h is the source; the files that cannot include it must say the same.
info = (root / "src/core/ProjectInfo.h").read_text()
def constant(name):
    m = re.search(r'k%s = "([^"]+)"' % name, info)
    if not m:
        sys.exit(f"src/core/ProjectInfo.h: k{name} not found")
    return m.group(1)
website, project, author = constant("Website"), constant("ProjectUrl"), constant("Author")
for path, needles in {
    "README.md": [website, project, author],
    "packaging/LinuxHardwareMonitor.1": [website, project],
    "packaging/io.github.zbara.LinuxHardwareMonitor.metainfo.xml": [website, project],
    "packaging/copyright": [website, project],
}.items():
    text = (root / path).read_text()
    for needle in needles:
        if needle not in text:
            errors.append(f"{path}: does not mention {needle} (see src/core/ProjectInfo.h)")

if errors:
    print("release metadata is out of sync:")
    for e in errors:
        print("  -", e)
    sys.exit(1)
print(f"release metadata consistent: {version}-{revision}")
