#!/bin/bash
# Builds the Arch package from packaging/arch/PKGBUILD and a source archive, runs namcap, and writes the files for
# the release to <out dir>: the package, and the PKGBUILD (with the real checksum and the release URL) with its
# .SRCINFO. Meant for an archlinux container (CI, or `docker run` locally); as root (makepkg itself runs as a user).
#   tools/build_arch.sh linux-hardware-monitor-0.2.1.tar.gz /out
set -euo pipefail
tarball=$(realpath "$1")
out=$(realpath -m "$2")
root=$(cd "$(dirname "$0")/.." && pwd)

# makepkg checks the run-time dependencies too (without -s it does not install them).
pacman -Syu --noconfirm --needed base-devel cmake ninja python dbus ttf-dejavu namcap git \
    qt6-base qt6-declarative hicolor-icon-theme >/dev/null
id builder >/dev/null 2>&1 || useradd -m builder
work=/home/builder/pkg
rm -rf "$work" && mkdir -p "$work" "$out"
cp "$root/packaging/arch/PKGBUILD" "$work/PKGBUILD"
sum=$(sha256sum "$tarball" | cut -d' ' -f1)

# 1. the release PKGBUILD: the real checksum (the URL is the release asset of the same name)
sed -i "s|^sha256sums=.*|sha256sums=('$sum')|" "$work/PKGBUILD"
cp "$work/PKGBUILD" "$out/PKGBUILD"
(cd "$work" && cp "$tarball" . && chown -R builder: . && su builder -c 'makepkg --printsrcinfo > .SRCINFO') && cp "$work/.SRCINFO" "$out/.SRCINFO"

# 2. build it from the local archive instead of downloading the release
src_name=$(basename "$tarball")
sed -i "s|^source=.*|source=(\"\$pkgname-\$pkgver.tar.gz::file://$work/$src_name\")|" "$work/PKGBUILD"
chown -R builder: "$work"
su builder -c "cd $work && makepkg --noconfirm -f"

echo "--- namcap"
namcap "$out/PKGBUILD" || true
# The package of debug symbols that makepkg splits off is skipped: namcap reports its build-id links as dangling.
main_pkg=$(ls "$work"/*.pkg.tar.* | grep -v -- '-debug-')
namcap "$main_pkg" | tee /tmp/namcap.txt || true
if grep -qE ' E: ' /tmp/namcap.txt; then
    echo "namcap reported errors" >&2
    exit 1
fi
cp "$main_pkg" "$out/"
ls -la "$out"
