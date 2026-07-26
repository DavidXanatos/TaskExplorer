# Portable Linux builds

The goal is one binary that runs across a wide range of distributions, rather
than a separate build per distro and per distro version. Two independent things
decide whether that works.

## 1. Libraries the target may not have — solved by bundling

Handled automatically by `deploy-linux.sh`, which runs as a post-build step, so
`Bin/linux-x86_64` is always a self-contained directory:

```
TaskExplorer            RPATH $ORIGIN/lib
TaskHelper              RPATH $ORIGIN/lib
qt.conf                 points Qt at ./plugins
lib/                    Qt, ICU, libstdc++, elfutils, xcb-util, …
plugins/platforms/      libqxcb.so
plugins/imageformats/   …
```

`$ORIGIN` is expanded by the loader to the directory holding the executable, so
the bundle works from wherever it is unpacked — no wrapper script, no
`LD_LIBRARY_PATH`.

Two details that are easy to get wrong:

- **The layout mirrors Qt's own on purpose.** Qt's libraries are built with
  `RUNPATH $ORIGIN` and its plugins with `$ORIGIN/../../lib`. Keeping `lib/` and
  `plugins/` as siblings means every relative path already resolves, so nothing
  has to be rewritten after copying and `patchelf` is not needed.

- **`DT_RPATH`, not `DT_RUNPATH`** (`-Wl,--disable-new-dtags`). `DT_RUNPATH`
  applies only to the object carrying it, while `DT_RPATH` is inherited down the
  dependency chain. Several bundled libraries depend on other bundled libraries
  and carry no search path of their own — `libdw`→`libelf`,
  `libxkbcommon-x11`→`libxkbcommon`, `libxcb-image`→`libxcb-util`. With
  `DT_RUNPATH` those resolve from the system instead, and the bundle turns out to
  be incomplete only on a machine that lacks them.

What is deliberately **not** bundled, because shipping our own copy is how you
get a binary that starts but renders nothing, or that misses security fixes:
glibc, the OpenGL/X11 client stack, glib/gio, D-Bus, systemd, fontconfig,
freetype — and OpenSSL, which should always be the system's patched build.

Wayland is also absent on purpose: window enumeration in `API/Linux/X11Helper`
is X11-only, so under a Wayland session the app runs through XWayland and the
xcb plugin, which works. A native Wayland plugin would start and then show an
empty Windows tab.

## 2. The glibc floor — only solved by the build host

This is the part bundling cannot fix. A binary's minimum glibc is fixed by the
glibc it was **compiled and linked against**. Copying a newer library into the
bundle does not lower it — it *raises* it, because the floor is the highest
requirement across every shipped object, not just the executables.

The build reports this on every run:

```
deploy-linux: glibc floor - Qt needs 2.34, this bundle needs 2.38
deploy-linux: the following raise it above Qt's own requirement:
deploy-linux:     libstdc++.so.6 needs glibc 2.38
deploy-linux:     libdw.so.1 needs glibc 2.38
deploy-linux: this build will not start on a system older than glibc 2.38.
```

A build on a current distribution is fine for development, but it is not
shippable: the bundled `libstdc++` and `libdw` pull the floor up to the build
host's own glibc.

### Building portably

```sh
Installer/build-portable.sh ~/Qt/6.11.1/gcc_64
```

This builds inside AlmaLinux 9 (glibc 2.34, GCC 11) and mounts the host's Qt,
which needs no installing because Qt's official binaries are themselves built
against glibc 2.34. Output goes to `Bin/linux-x86_64` as usual, but the bundled
runtime libraries now come from the container, so the floor drops to **2.34**.

Needs `podman` or `docker`.

### Why 2.34 is the target

2.34 is Qt 6.11's own requirement, so it is the lowest floor achievable without
changing Qt:

```sh
objdump -T ~/Qt/6.11.1/gcc_64/lib/libQt6Core.so.6 | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1
```

TaskExplorer's own code already sits exactly there — its only 2.34 symbols are
`dlopen`, `dlsym` and `__libc_start_main`, none of which can be avoided.

A floor of 2.34 covers, roughly: Ubuntu 22.04 and later, Debian 12 and later,
RHEL/AlmaLinux/Rocky 9 and later, Fedora 35 and later, openSUSE Leap 15.6 and
later, and rolling distributions. It excludes RHEL 8, Debian 11 and Ubuntu 20.04
(all glibc 2.28–2.31). Reaching those would mean building against an older Qt —
Qt 6.5 LTS is the usual candidate — and measuring its floor with the command
above rather than assuming one.

### A trap worth knowing about

Since glibc 2.38, `strtol`/`strtoul` and friends are redirected to
`__isoc23_strtol` etc. in C++ mode — unconditionally, no `-std` flag avoids it.
A single call raises the floor of the whole binary to 2.38. `TaskHelper` hit
exactly this; it now uses a local `ParseUInt` instead. If the reported floor
jumps to 2.38 after adding code, this is the first thing to check:

```sh
objdump -T Bin/linux-x86_64/TaskHelper | grep __isoc23
```

## Status

The bundling in part 1 is verified: the tree was copied to an unrelated
directory and run with `LD_LIBRARY_PATH` and `QT_PLUGIN_PATH` unset, and every
Qt library, ICU, libstdc++, the xcb chain, the platform plugin and the image
format plugins all resolved from the bundle, with nothing from the build
machine's Qt mapped into the process.

The container build in part 2 is **not yet verified** — the development machine
has no container runtime installed, so the recipe has been written but not run.
Expect to iterate on the package list, and on any place where the source needs a
newer compiler than GCC 11.
