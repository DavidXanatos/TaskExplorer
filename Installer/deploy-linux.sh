#!/bin/bash
#
# Bundles Qt and the few other libraries TaskExplorer cannot rely on finding,
# so that one build runs on any reasonably current distribution rather than
# needing to be rebuilt per distro and per distro version.
#
# Usage: deploy-linux.sh <app-dir> <qt-prefix> <binary> [binary...]
#
# Layout produced, which is deliberately the same one Qt itself uses:
#
#     TaskExplorer                 RUNPATH $ORIGIN/lib
#     TaskHelper                   RUNPATH $ORIGIN/lib
#     qt.conf                      points Qt at ./plugins
#     lib/libQt6*.so.6 ...
#     plugins/platforms/libqxcb.so
#     plugins/imageformats/...
#
# Mirroring Qt's layout is what makes this work without patchelf: Qt's own
# libraries are built with RUNPATH $ORIGIN and its plugins with
# $ORIGIN/../../lib, so once lib/ and plugins/ are siblings every relative path
# already resolves. Nothing has to be rewritten after copying.
#
# What is bundled, and what is deliberately not:
#
#   Bundled - Qt, Qt's own ICU build (version-specific, no distro has .73),
#   the GCC runtime (libstdc++/libgcc_s/libatomic - Qt 6.11 needs
#   GLIBCXX_3.4.29, newer than several supported distributions provide),
#   elfutils (libdw/libelf - the helper links them and they are not installed
#   everywhere), and the xcb-util family plus libxcb-cursor, which Qt 6
#   requires and which most distributions do not install by default. That last
#   one is the single most common reason a Qt 6 binary refuses to start.
#
#   Not bundled - glibc, the OpenGL/X11 client stack, glib/gio, D-Bus,
#   systemd, fontconfig and freetype: all of these are coupled to the running
#   system's drivers, buses or configuration, and shipping our own copy is how
#   you get a binary that starts but renders nothing. Also not bundled:
#   OpenSSL, on purpose - TLS should use the system's patched libraries, not a
#   frozen copy of ours.
#
set -eu

APPDIR="${1:?app dir required}"
QTDIR="${2:?qt prefix required}"
shift 2
BINARIES="$*"

QTLIB="$QTDIR/lib"
QTPLUGINS="$QTDIR/plugins"

if [ ! -d "$QTLIB" ]; then
	echo "deploy-linux: no Qt libraries at $QTLIB" >&2
	exit 1
fi

mkdir -p "$APPDIR/lib"

#
# Plugin set.
#
# Qt loads these by dlopen, so nothing in the link map reveals them and they
# have to be listed. A plugin whose own dependencies are missing simply fails
# to load, which is why the optional ones below are cheap to include.
#
PLUGINS="
platforms/libqxcb.so
xcbglintegrations/libqxcb-glx-integration.so
xcbglintegrations/libqxcb-egl-integration.so
imageformats/libqsvg.so
imageformats/libqico.so
imageformats/libqjpeg.so
imageformats/libqgif.so
iconengines/libqsvgicon.so
tls/libqopensslbackend.so
tls/libqcertonlybackend.so
platformthemes/libqxdgdesktopportal.so
platforminputcontexts/libcomposeplatforminputcontextplugin.so
platforminputcontexts/libqibusplatforminputcontextplugin.so
networkinformation/libqglib.so
networkinformation/libqnetworkmanager.so
"
#
# Wayland is deliberately absent. The window enumeration in API/Linux/X11Helper
# is X11-only, so under a Wayland session the app runs through XWayland and the
# xcb plugin - which works - whereas a native Wayland plugin would start and
# then show an empty Windows tab. Better to be honestly X11 than subtly broken.
#

#
# Libraries to bundle even though they are not Qt's, keyed by SONAME.
#
# Each is here for a specific reason; see the header comment. Resolved by SONAME
# against this system rather than by hardcoded path, so this works on any build
# host regardless of where its distribution keeps them.
#
EXTRA_SONAMES="
libstdc++.so.6
libgcc_s.so.1
libatomic.so.1
libdw.so.1
libelf.so.1
libxcb-cursor.so.0
libxcb-icccm.so.4
libxcb-image.so.0
libxcb-keysyms.so.1
libxcb-render-util.so.0
libxcb-util.so.1
libxkbcommon.so.0
libxkbcommon-x11.so.0
"

copied=0
skipped=0

# Copies one file, dereferencing symlinks and naming the result after the
# SONAME the loader will actually ask for.
copy_lib()
{
	src="$1"
	dst="$APPDIR/lib/$2"

	if [ -f "$dst" ] && [ ! "$src" -nt "$dst" ]; then
		skipped=$((skipped + 1))
		return 0
	fi

	cp -L "$src" "$dst"
	chmod 644 "$dst"
	copied=$((copied + 1))
}

#
# The SONAMEs an object declares it needs.
#
# Deliberately objdump and not ldd. ldd reports what the loader *would resolve
# today*, which depends on what is already in lib/ and on what the build machine
# happens to have installed - so a host with a distro Qt alongside the real one
# silently satisfies, say, libQt6Widgets from /usr/lib, the file never gets
# bundled, and the result only fails on someone else's machine. Asking for the
# declared names instead makes this independent of both.
#
needed_sonames()
{
	objdump -p "$1" 2>/dev/null | awk '/NEEDED/ { print $2 }'
}

#
# Transitive closure of everything the given objects need that Qt provides.
#
# A worklist rather than one pass, because the Qt libraries need each other -
# and libQt6XcbQpa is reached only through the xcb platform plugin, never from
# an executable.
#
QT_DONE=""
bundle_qt_closure()
{
	queue=""
	for f in "$@"; do
		[ -f "$f" ] || continue
		queue="$queue $(needed_sonames "$f")"
	done

	while [ -n "$(printf '%s' "$queue" | tr -d ' ')" ]; do
		soname=$(printf '%s' "$queue" | tr ' ' '\n' | grep -v '^$' | head -1)
		queue=$(printf '%s' "$queue" | tr ' ' '\n' | grep -v '^$' | tail -n +2 | tr '\n' ' ')

		case " $QT_DONE " in
			*" $soname "*) continue ;;
		esac
		QT_DONE="$QT_DONE $soname"

		# Only bundle it if Qt is the one providing it.
		[ -f "$QTLIB/$soname" ] || continue

		copy_lib "$QTLIB/$soname" "$soname"
		queue="$queue $(needed_sonames "$QTLIB/$soname")"
	done
}

#
# Resolves a SONAME to a path on this system, for the libraries we ship that are
# not Qt's. Uses the linker cache rather than ldd for the same reason as above.
#
resolve_system_soname()
{
	soname="$1"

	# The GCC runtime is best asked of the compiler that built this.
	case "$soname" in
		libstdc++.so.6|libgcc_s.so.1|libatomic.so.1)
			path=$(${CC:-gcc} -print-file-name="$soname" 2>/dev/null || true)
			if [ -n "$path" ] && [ "$path" != "$soname" ] && [ -f "$path" ]; then
				printf '%s' "$path"
				return 0
			fi
			;;
	esac

	ldconfig -p 2>/dev/null | awk -v s="$soname" '
		$1 == s && /x86-64/ { print $NF; exit }
		$1 == s && !seen    { alt = $NF; seen = 1 }
		END { if (!printed && seen) print alt }' | head -1
}

echo "deploy-linux: bundling Qt from $QTDIR"

# ---- Qt libraries needed by the executables ----
bundle_qt_closure $BINARIES

# ---- plugins, and the Qt libraries they pull in ----
for rel in $PLUGINS; do
	src="$QTPLUGINS/$rel"
	if [ ! -f "$src" ]; then
		echo "deploy-linux: optional plugin not present, skipping: $rel"
		continue
	fi

	dstdir="$APPDIR/plugins/$(dirname "$rel")"
	mkdir -p "$dstdir"

	dst="$dstdir/$(basename "$rel")"
	if [ ! -f "$dst" ] || [ "$src" -nt "$dst" ]; then
		cp -L "$src" "$dst"
		chmod 755 "$dst"
		copied=$((copied + 1))
	else
		skipped=$((skipped + 1))
	fi

	bundle_qt_closure "$src"
done

# ---- the non-Qt libraries we ship on purpose ----
for soname in $EXTRA_SONAMES; do
	path=$(resolve_system_soname "$soname")

	if [ -z "$path" ] || [ ! -f "$path" ]; then
		echo "deploy-linux: could not locate $soname, not bundled"
		continue
	fi

	copy_lib "$path" "$soname"
done

#
# qt.conf, so Qt looks for plugins next to the application instead of in the
# absolute paths compiled into it - which point at this build machine's Qt.
#
# Without this the app would silently fall back to a system Qt's plugins if any
# happened to be installed, or find none at all and abort with "could not load
# the Qt platform plugin xcb".
#
QTCONF="$APPDIR/qt.conf"
if [ ! -f "$QTCONF" ]; then
	cat > "$QTCONF" <<-EOF
	[Paths]
	Prefix = .
	Plugins = plugins
	Libraries = lib
	EOF
	echo "deploy-linux: wrote qt.conf"
fi

#
# Report the bundle's effective glibc floor.
#
# This is the number that decides which distributions the build actually runs
# on, and it is the *highest* requirement across every shipped object - not just
# the executables. That is easy to get wrong: bundling the build host's
# libstdc++ or libdw to help portability can quietly raise the floor above what
# Qt itself needs, and the result then refuses to start on precisely the older
# systems the bundling was meant to support. Copying a newer library in can
# never lower the floor; only building on an older host can.
#
# So this is printed on every build rather than left to be discovered later.
#
glibc_floor_of()
{
	objdump -T "$1" 2>/dev/null | grep -o 'GLIBC_[0-9.]*' | sed 's/GLIBC_//' | sort -uV | tail -1
}

QT_FLOOR=$(
	for f in "$QTLIB"/libQt6*.so.6; do
		[ -f "$f" ] || continue
		glibc_floor_of "$f"
	done | sort -uV | tail -1
)

BUNDLE_FLOOR=""
OFFENDERS=""

for f in $BINARIES "$APPDIR"/lib/*.so* "$APPDIR"/plugins/*/*.so; do
	[ -f "$f" ] || continue

	v=$(glibc_floor_of "$f")
	[ -n "$v" ] || continue

	if [ -z "$BUNDLE_FLOOR" ] || [ "$(printf '%s\n%s\n' "$BUNDLE_FLOOR" "$v" | sort -V | tail -1)" = "$v" ]; then
		BUNDLE_FLOOR="$v"
	fi

	# Anything asking for more than Qt does is what limits the build.
	if [ -n "$QT_FLOOR" ] && [ "$v" != "$QT_FLOOR" ] \
	   && [ "$(printf '%s\n%s\n' "$QT_FLOOR" "$v" | sort -V | tail -1)" = "$v" ]; then
		OFFENDERS="$OFFENDERS $(basename "$f")=$v"
	fi
done

echo "deploy-linux: glibc floor - Qt needs $QT_FLOOR, this bundle needs $BUNDLE_FLOOR"

if [ -n "$OFFENDERS" ]; then
	echo "deploy-linux: the following raise it above Qt's own requirement:"
	for o in $OFFENDERS; do
		echo "deploy-linux:     ${o%%=*} needs glibc ${o##*=}"
	done
	echo "deploy-linux: this build will not start on a system older than glibc $BUNDLE_FLOOR."
	echo "deploy-linux: to reach $QT_FLOOR, build on a host with that glibc - see Installer/README-portable.md"
fi

echo "deploy-linux: $copied copied, $skipped already current"
