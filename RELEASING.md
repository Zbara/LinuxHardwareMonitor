# Releasing

Packages are built by GitHub Actions for **Ubuntu 26.04** and **Debian 13** (a container of each, so the
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
   git push origin master vX.Y.Z
   ```
4. The `Release` workflow builds and tests both packages, runs `lintian` and `appstreamcli`, writes `SHA256SUMS`,
   signs it when a key is configured and creates the release with the notes from `packaging/changelog`.

A change to the packaging alone (same program version) uses a new revision: `X.Y.Z-2` in the changelog and
`HWMON_DEB_REVISION` in `CMakeLists.txt`.

## Signing (optional, recommended)

The `.deb` files are not signed themselves; the signature covers `SHA256SUMS`, which lists them.

1. Make a key for releases (once): `gpg --quick-generate-key "Your Name <you@example.org>" ed25519 sign 3y`
2. Add two repository secrets (Settings > Secrets and variables > Actions):
   - `GPG_PRIVATE_KEY`: the output of `gpg --armor --export-secret-keys <key id>`
   - `GPG_PASSPHRASE`: its passphrase
3. Without `GPG_PRIVATE_KEY` the release is simply published unsigned. With it, the release also contains
   `SHA256SUMS.asc` and `signing-key.asc`. Publish the key's fingerprint somewhere users can compare it
   (the README, your profile), otherwise the signature only proves that the same key signed the files.

Users verify with:
```
gpg --import signing-key.asc && gpg --verify SHA256SUMS.asc SHA256SUMS && sha256sum -c SHA256SUMS --ignore-missing
```

## Checking a package locally

```
cmake -S . -B build -G Ninja -DHWMON_DISTRO_TAG=ubuntu26.04
cmake --build build && (cd build && cpack -G DEB)
lintian --no-cfg --fail-on error,warning build/*.deb       # needs the lintian package
appstreamcli validate --no-net packaging/*.metainfo.xml
desktop-file-validate packaging/linux-hardware-monitor.desktop
```

## Supported distributions

The program needs Qt 6.5 or newer, which rules out Ubuntu 24.04 (Qt 6.4). To add a release, add a line to the
matrix in `.github/workflows/ci.yml` (`image` and the `tag` used in the file name). The package depends on the Qt of
the distribution it was built on (`qt6-declarative-private-abi (= <version>)`), so a package never installs on a
different release than the one it was built for.

## Other ways to publish

A PPA or the Debian archive need a *source* package (`debian/` with debhelper), not this CPack output; that is
separate work. GitHub releases need none of it.
