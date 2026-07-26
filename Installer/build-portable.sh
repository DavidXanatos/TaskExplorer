#!/bin/bash
#
# Builds TaskExplorer in a container with an old glibc, so the result runs on a
# wide range of distributions instead of only on machines at least as new as this
# one.
#
# Usage: Installer/build-portable.sh [qt-prefix]
#
#   qt-prefix   the gcc_64 directory of a Qt installation, e.g.
#               ~/Qt/6.11.1/gcc_64. Defaults to $QTDIR, then to the newest
#               ~/Qt/*/gcc_64 found.
#
# Why the host's Qt is mounted rather than installed in the image: Qt's official
# Linux binaries are themselves built against glibc 2.34, so they run unchanged
# inside the image, and there is no sense downloading a second copy. The moc/uic/
# rcc tools run there too.
#
# The build output lands in Bin/linux-x86_64 as usual, and the bundle is produced
# by the ordinary post-build step - except that it now picks up the *container's*
# libstdc++, libdw and xcb libraries, which is the whole point.
#
set -eu

SRCDIR=$(cd "$(dirname "$0")/.." && pwd)
IMAGE=taskexplorer-portable

QTPREFIX="${1:-${QTDIR:-}}"
if [ -z "$QTPREFIX" ]; then
	QTPREFIX=$(ls -d "$HOME"/Qt/*/gcc_64 2>/dev/null | sort -V | tail -1 || true)
fi

if [ -z "$QTPREFIX" ] || [ ! -d "$QTPREFIX/lib" ]; then
	echo "build-portable: no Qt found; pass the gcc_64 directory as the first argument" >&2
	exit 1
fi

if command -v podman > /dev/null 2>&1; then
	RUNTIME=podman
elif command -v docker > /dev/null 2>&1; then
	RUNTIME=docker
else
	echo "build-portable: needs podman or docker" >&2
	exit 1
fi

echo "build-portable: runtime $RUNTIME, Qt $QTPREFIX"

$RUNTIME build -t "$IMAGE" -f "$SRCDIR/Installer/Dockerfile.portable" "$SRCDIR/Installer"

#
# A separate build directory from the host's, because CMake caches absolute
# compiler paths and the two toolchains are not interchangeable. Sharing one
# would mean a reconfigure on every switch.
#
# :z on the mounts is for SELinux hosts (Fedora, RHEL); harmless elsewhere.
#
$RUNTIME run --rm \
	-v "$SRCDIR":/src:z \
	-v "$QTPREFIX":"$QTPREFIX":ro,z \
	-e QTPREFIX="$QTPREFIX" \
	"$IMAGE" \
	bash -euc '
		cmake -S /src -B /src/build-portable -G Ninja \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_PREFIX_PATH="$QTPREFIX" \
			-DTE_BUNDLE_QT=ON
		cmake --build /src/build-portable -j"$(nproc)"
	'

echo
echo "build-portable: done. Verify the floor with:"
echo "  objdump -T $SRCDIR/Bin/linux-x86_64/TaskExplorer | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1"
