#!/usr/bin/env python3
"""Prints the notes of a GitHub release: the newest entry of packaging/changelog plus install and
verification instructions. Used by .github/workflows/release.yml."""
import re
from pathlib import Path

root = Path(__file__).resolve().parent.parent
changelog = (root / "packaging/changelog").read_text()
entry = re.split(r"\n -- ", changelog, maxsplit=1)[0]
lines = entry.splitlines()
version = re.match(r"linux-hardware-monitor \(([^)]+)\)", lines[0]).group(1)
changes = "\n".join(line[2:] if line.startswith("  ") else line for line in lines[1:] if line.strip())

upstream = version.rsplit("-", 1)[0]
print(f"""## Linux Hardware Monitor {upstream}

{changes}

### Install

**Debian and Ubuntu.** Pick the package for your system (`ubuntu26.04`, `ubuntu24.04`, `ubuntu22.04` or `debian13`):

```
sudo apt install ./linux-hardware-monitor_{version}_<system>_amd64.deb
```

**Fedora.**

```
sudo dnf install ./linux-hardware-monitor-{upstream}-1.fc*.x86_64.rpm
```

**Arch Linux.** The `PKGBUILD` of this release downloads the source archive (below) and builds the package:

```
curl -LO https://github.com/Zbara/LinuxHardwareMonitor/releases/download/v{upstream}/PKGBUILD
makepkg -si
```

The DEB and the RPM depend on the Qt version of the distribution they were built for, so the one for another release will
not install; other systems can build the program from `linux-hardware-monitor-{upstream}.tar.gz` (see the README).
(If `apt` prints a notice about the `_apt` user, copy the file out of your home directory first, e.g. to `/tmp`.)

### Verify the download

```
sha256sum -c SHA256SUMS --ignore-missing
```

If the release contains `SHA256SUMS.asc`, it is a detached signature of `SHA256SUMS`:

```
gpg --import signing-key.asc
gpg --verify SHA256SUMS.asc SHA256SUMS
```
""")
