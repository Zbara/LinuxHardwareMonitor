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

Pick the package for your system (`ubuntu26.04` or `debian13`) and install it:

```
sudo apt install ./linux-hardware-monitor_{version}_<system>_amd64.deb
```

The package depends on the Qt version of its distribution, so the one for another release will not install.
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
