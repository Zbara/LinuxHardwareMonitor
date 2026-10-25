#!/bin/bash
# Builds the RPM (and the source RPM) of packaging/linux-hardware-monitor.spec from a source archive, runs rpmlint
# on them and copies them to <out dir>. Meant for a Fedora container (CI, or `docker run` locally); as root.
#   tools/build_rpm.sh linux-hardware-monitor-0.2.1.tar.gz /out
set -euo pipefail
export LC_ALL=C.UTF-8 # a container has no locale; rpmlint and the spec parser warn about it
tarball=$(realpath "$1")
out=$(realpath -m "$2")
root=$(cd "$(dirname "$0")/.." && pwd)
spec="$root/packaging/linux-hardware-monitor.spec"
top=$(mktemp -d)

dnf -y -q install rpm-build rpmlint 'dnf-command(builddep)' >/dev/null
dnf -y -q builddep "$spec" >/dev/null

mkdir -p "$top"/{SOURCES,SPECS,BUILD,RPMS,SRPMS} "$out"
cp "$tarball" "$top/SOURCES/"
rpmbuild -ba "$spec" --define "_topdir $top"
cp "$top"/RPMS/*/*.rpm "$top"/SRPMS/*.rpm "$out/"

echo "--- rpmlint"
# Errors fail the build; warnings are listed.
rpmlint "$out"/*.rpm "$spec" | tee "$top/rpmlint.txt" || true
if grep -qE ' E: ' "$top/rpmlint.txt"; then
    echo "rpmlint reported errors" >&2
    exit 1
fi
ls -la "$out"
