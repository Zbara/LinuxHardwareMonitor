# Releasing

Packages are built by GitHub Actions for **Ubuntu 26.04**, **Ubuntu 24.04** and **Debian 13** (a container of each, so the
dependencies are right for that release) and published as a GitHub release. Nothing is built by hand.

## Cut a release

1. Pick the version `X.Y.Z` and change it everywhere:
   - `CMakeLists.txt`: `project(LinuxHardwareMonitor VERSION X.Y.Z ...)`
   - `packaging/changelog`: add a new top entry `linux-hardware-monitor (X.Y.Z-1) unstable; urgency=medium` with the
     changes and a correct date (`date -R`)
   - `packaging/io.github.zbara.LinuxHardwareMonitor.metainfo.xml`: a new `<release version="X.Y.Z" date="...">` first
   - the `.TH` line of `packaging/LinuxHardwareMonitor.1`
   `python3 tools/check_release_metadata.py` (also the `release_metadata` test) fails if one of them is forgotten.
2. Run the tests and, if you want to see what CI will do, the lint locally (see below).
3. Commit, then tag and push:
   ```
   git tag -a vX.Y.Z -m "Linux Hardware Monitor X.Y.Z"
   git push origin main vX.Y.Z
   ```
4. The `Release` workflow builds and tests both packages, runs `lintian` and `appstreamcli`, writes `SHA256SUMS`,
   signs it when a key is configured and creates the release with the notes from `packaging/changelog`.

A change to the packaging alone (same program version) uses a new revision: `X.Y.Z-2` in the changelog and
`HWMON_DEB_REVISION` in `CMakeLists.txt`.

## Signing

The `.deb` files are not signed themselves; the signature covers `SHA256SUMS`, which lists them.

The release key is **455F 2B9C 5659 DD1B ECB7 3300 F2AA EDFB 7B8C B932** (ed25519, "Zbara (Linux Hardware Monitor
releases)", valid until **2029-10-24**; extend it with `gpg --quick-set-expire <fingerprint> 3y` and publish the new
`docs/signing-key.asc` before it runs out). The workflow signs when these repository secrets exist (Settings >
Secrets and variables > Actions), otherwise it publishes unsigned with checksums only:

- `GPG_PRIVATE_KEY`: `gpg --armor --export-secret-keys <fingerprint>`
- `GPG_PASSPHRASE`: its passphrase

The private key, the passphrase and a copy of the revocation certificate (`~/.gnupg/openpgp-revocs.d/<fingerprint>.rev`)
must be backed up somewhere safe (a password manager). Whoever has the key can sign releases in your name; if it is
ever exposed, revoke it with the certificate and publish a new one in the README. Nothing secret is stored in this
repository: only the public key (`docs/signing-key.asc`) and the fingerprint.

The release contains `SHA256SUMS.asc` and `signing-key.asc`. Users verify with the commands in the README
(they must compare the fingerprint, otherwise the signature only proves that the same key signed the files).

## Checking a package locally

```
cmake -S . -B build -G Ninja -DHWMON_DISTRO_TAG=ubuntu26.04
cmake --build build && (cd build && cpack -G DEB)
lintian --no-cfg --fail-on error,warning build/*.deb       # needs the lintian package
appstreamcli validate --no-net packaging/*.metainfo.xml
desktop-file-validate packaging/linux-hardware-monitor.desktop
```

## Supported distributions

The program builds with Qt 6.2 or newer: Ubuntu 22.04 (6.2), Ubuntu 24.04 and Debian 12 (6.4), Debian 13 (6.8),
Ubuntu 26.04 (6.10). The few calls that are newer than Qt 6.2 are guarded by `QT_VERSION` checks or have a portable
form. A Qt 6.2 build needs the extra package `libqt6opengl6-dev` (Qt6Quick depends on Qt6OpenGL there). To add a release, add a line to the
matrix in `.github/workflows/ci.yml` (`image` and the `tag` used in the file name). The package depends on the Qt of
the distribution it was built on (`qt6-declarative-private-abi (= <version>)`), so a package never installs on a
different release than the one it was built for.

## Other ways to publish

A PPA or the Debian archive need a *source* package (`debian/` with debhelper), not this CPack output; that is
separate work. GitHub releases need none of it.
