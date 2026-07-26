# Linux port — things that need testing by hand

Everything in the Linux backend that I could verify programmatically, I did:
each `/proc` and `/sys` reader was cross-checked against `ps`, `ss`, `free`,
`nproc`, `ip`, `resolvectl`, `loginctl`, `systemctl`, `taskset`, `ionice` and
`dd` throughput, and the GUI was driven to confirm the data reaches the views.

This file lists what I **could not** verify, and why. Each item says what to do,
what you should see, and what it means if it misbehaves.

Ordered roughly by how likely a problem is to matter.

---

## 1. The Windows build — highest priority

**Nothing about this has been compiled on Windows.** There is no MSVC on the
development machine.

The port touches shared code, not just `API/Linux`. These are the files a
Windows build will exercise differently, and where a regression would show up:

| File | Change | Risk if wrong |
|---|---|---|
| `API/ProcessInfo.cpp` | `SProcessUID` shift is now `#ifdef`'d per platform | Windows path is unchanged, but confirm the `#ifdef WIN32` branch is taken |
| `Common/StatusEx.h` | `CResult` copy ctor / `operator=` rewritten | Template was never instantiated before; if anything now instantiates it, it must still compile |
| `Common/Variant.h` | `CVariant`'s converting ctor is now SFINAE-constrained | Anything relying on the old unconstrained ctor will now fail to compile |
| `Common/Buffer.cpp` | `max()` → `std::max()` with an explicit cast | Trivial, but it is in a hot path |
| `MiscHelpers/Common/Common.h` | `__time64_t` → `qint64`, `L''` → `u''` | Same 64-bit type on MSVC; confirm no signature mismatch |
| `MiscHelpers/Common/Settings.h` | union members `_Bool`/`_Int`/… renamed to `vBool`/`vInt`/… | Purely internal to a macro |
| `GUI/TaskInfo/HandlesView.cpp` | filter-widget reads moved inside `#ifdef WIN32` | Windows behaviour should be identical; verify the type filter still works |
| `main.cpp` | restructured; duplicate DPI block removed | **Check the Windows startup path carefully** — service/worker modes, KSI driver init, elevation |

Also: the qmake build is gone. `TaskExplorer.vcxproj` is untouched, but if you
built via qmake you now need CMake.

**Test:** build on Windows, run, confirm the process tree, services, handles and
the KSI driver dialog all behave as before.

---

## 2. Service control — needs your password

`Start` / `Stop` / `Pause` / `Continue` on a systemd unit go through polkit,
which prompts for credentials. I verified the D-Bus plumbing and the error
mapping, but deliberately did not authenticate as you, and did not stop any
running service on your machine.

**Test:** Services panel → right-click a unit → Start / Stop.

**Expected:** your desktop's polkit agent prompts for a password; after
authenticating the unit's state changes within a refresh or two.

**Suggested safe target:** a unit you can bounce without consequence, e.g.
`cups-browsed.service`.

**If it fails**, the error text distinguishes the causes:

- *"…no interactive authentication agent is available"* — no polkit agent in
  the session. Real bug only if you **are** in a desktop session.
- *"Not authorised to…"* — polkit denied you. Check your policy.
- *"…not supported by the running version of systemd"* — Pause/Continue need
  systemd ≥ 246 (`FreezeUnit`/`ThawUnit`).

**What is deliberately not implemented:** `Delete`. It returns an explanation
rather than acting. A systemd unit file normally belongs to a distribution
package; the error names `systemctl disable` / `systemctl mask` instead.

---

## 3. DNS cache — needs your password

**Flush** goes through polkit via `resolve1.Manager.FlushCaches`.

**Test:** the DNS cache flush action.
**Expected:** prompt, then success. On a system without systemd-resolved this
is a silent no-op — correct, since glibc's resolver does not cache.

### 3a. The DNS cache view

The cache contents are read from systemd-resolved. They are deliberately *not*
on its D-Bus interface, which offers only `FlushCaches` and `CacheStatistics`;
the contents come from `DumpCache` on its **varlink** socket, which is what
`resolvectl show-cache` itself calls. Verified against that command: 31 entries
parsed, with A, AAAA, CNAME, PTR, MX and TXT records all rendering.

**This costs an administrator authentication.** The polkit action
`org.freedesktop.resolve1.dump-cache` is `auth_admin_keep`, so:

- **Running elevated** the view fills in with no prompt at all.
- **Unprivileged** you get **one** password prompt, the first time the view
  refreshes. Authenticate and it keeps working for as long as polkit remembers
  it (`_keep`), silently re-reading on every refresh.
- **Decline the prompt** and the view stays empty, and it will not ask again
  while you stay on the tab — the view refreshes on a timer, and asking every
  time would put a password dialog on screen every few seconds. **Leaving the
  tab and coming back asks again**, so a cancelled prompt is retryable without
  restarting.

The prompt is therefore rationed per *visit* to the view, not per process run.
Once authentication succeeds no prompt appears at all, because the
non-interactive call keeps working for as long as polkit remembers it.

**Test:** open the DNS cache view unprivileged and cancel — empty, and no
repeated prompts while you stay there. Switch to another tab and back — it
should ask again. Authenticate, and confirm entries appear, keep updating, and
that switching tabs no longer produces a prompt.

Expired records that resolved has not yet evicted are shown greyed out, which is
the same treatment the Windows build gives entries that have aged out.

---

## 4. Hardware hotplug

The uevent monitor is bound and listening (confirmed: netlink family 15,
multicast group 1), but I could not generate a real hotplug event — creating
loop devices or dummy interfaces needs root, and this is your live machine.

**Test:** with TaskExplorer open on the Disk or Network tab, plug in a USB
stick, or bring an interface up/down (`ip link set … up`).

**Expected:** the device appears in the Disk / Network list within ~1 second,
without needing a manual refresh.

**If nothing happens:** devices still appear on the next periodic refresh, so
the failure is "slow" rather than "broken". Only `block`, `net` and `drm`
subsystem events are acted on — other subsystems are ignored on purpose.

---

## 5. Anything requiring root or ptrace access

Ubuntu defaults to `kernel.yama.ptrace_scope=1`, which restricts ptrace to
descendant processes. As an unprivileged user, TaskExplorer therefore cannot
read certain things for processes it did not start. This is **correct
behaviour**, not a bug — but it means these paths are only exercised when you
run as root or grant `CAP_SYS_PTRACE`.

| Feature | Unprivileged | As root |
|---|---|---|
| Per-process disk I/O (`/proc/<pid>/io`) | blank for other users' processes | populated |
| Memory tab / hex editor for other users' processes | fails to open | works |
| String search across all processes | skips unreadable processes | scans everything |
| Thread stack traces | reports "ptrace access was denied" | real frames with symbols |

**Fixed since the first pass:** selecting a thread could crash TaskExplorer,
most visibly when running elevated because the trace then actually succeeds.
`TraceStack()` is called from the GUI thread but a `CLinuxThread` lives on the
API worker thread, so the `QProcess` was created with a cross-thread parent
(Qt refused it and logged "Cannot create children for a parent that is in a
different thread"), and the completion handler then ran on the worker thread
while the process object belonged to the GUI thread. The two raced over the same
object until `readAllStandardError()` walked a corrupted vtable. Reproduced
under gdb, fixed by handling completion with a direct connection so all access
stays on one thread — 54 warnings and a reliable crash became 0 and 0 over 24
consecutive thread selections, with traces still returning symbolised frames.
| Command line, cwd, environment for other users | blank | populated |
| Open file descriptors for other users | absent | present |

### 5a. "Restart Elevated" — needs your password

The toolbar/menu entry was a no-op on Linux until now (`OnElevate()` was
entirely inside `#ifdef WIN32`). It relaunches TaskExplorer through a graphical
privilege escalation helper — **pkexec** by preference, falling back to `kdesu`,
`lxqt-sudo`, `gksudo`, `gksu`.

I verified the invocation form is accepted by pkexec (it opened its prompt
rather than rejecting the command), but did **not** authenticate, so the
round trip is untested.

**Test:** click *Restart Elevated*.

**Expected:** a password prompt, then TaskExplorer reappears running as root.
The entry hides itself once elevated, since `RootAvaiable()` then returns true.

**What to watch for:**

- **A blank or missing window after authenticating** means the elevated process
  could not reach the X server. The launcher passes `DISPLAY`, `XAUTHORITY` and
  `QT_QPA_PLATFORM` explicitly, because pkexec deliberately clears the
  environment — and it falls back to *your* `~/.Xauthority` when `XAUTHORITY`
  is unset, since root's `$HOME` would otherwise send it looking in `/root`.
  If it still fails, check what `XAUTHORITY` was set to in the session.

- **Cancelling the prompt** should leave the current instance running and show
  "Could not restart elevated…". There is a known limitation here: the check
  waits 2.5 s before deciding. If you take longer than that to type your
  password the old instance closes first, so cancelling *after* that point
  leaves nothing running. Not dangerous, but worth knowing.

- **Under Wayland** this returns an explanatory error instead of trying, because
  a compositor will not accept a connection from a process running as another
  user. Use `sudo` from a terminal there.

- **Two instances briefly coexisting** is expected and harmless — the
  single-instance key differs between elevated and unelevated (`TaskExplorer`
  vs `UTaskExplorer`), and the new process is passed `-multi`.

### 5b. Other routes to root

**Test:** `sudo ./TaskExplorer` — the plain route, and the one to fall back on
if 5a misbehaves. Confirm the table above fills in.

Worth testing separately: `setcap cap_sys_ptrace+ep ./TaskExplorer` — the code
accepts `CAP_SYS_PTRACE`/`CAP_SYS_ADMIN` from `/proc/self/status` `CapEff`, not
just uid 0, so this should unlock the table above *without* running the whole
GUI as root. I have not confirmed that path end to end, and it is the better
option if it works: a task manager does not need to be root to read `/proc`.

---

## 6. Destructive process actions

Implemented and straightforward, but I did not fire them at real processes:

- **Terminate** — `SIGTERM`, or `SIGKILL` with "force".
- **Suspend / Resume** — `SIGSTOP` / `SIGCONT`.
- **Set priority** — `setpriority`. Lowering a nice value needs `CAP_SYS_NICE`,
  so going below 0 is expected to fail unprivileged.
- **Set affinity** — `sched_setaffinity`.
- **Set I/O priority** — `ioprio_set`. The realtime class needs `CAP_SYS_ADMIN`.

**Test:** on a process you own and can afford to lose (`sleep 600 &` is ideal).

---

## 7. Window management (X11)

Enumeration is verified — window list, titles, PID attribution all cross-checked
against `xprop`. The **actions** are not:

- Bring to front, Close, Minimize, Maximize, Always-on-top
- Set opacity — **requires a compositing manager**; without one the property is
  set and silently ignored. That is expected, not a failure.
- Highlight — draws an inverting frame around the window and erases it. Should
  leave nothing behind; if it does, that is a bug worth reporting.

**Test:** Windows tab of a GUI process → try each action.

### 7a. Tray icon — restoring the window

On Linux this is a **single click**, not a double click as on Windows.

Modern desktops (KDE Plasma, GNOME with an extension, anything else using the
StatusNotifierItem D-Bus protocol) have **no concept of a double click on a tray
icon** — the panel exposes only an `Activate` call, which Qt reports as a single
`Trigger`. Confirmed here: a double click on the icon delivers two `Trigger`
events and never a `DoubleClick`. Wiring the behaviour to `DoubleClick`, as the
Windows code does, therefore left no way to get the window back at all.

**Test:** minimise to tray, then single-click the icon — the window should come
back, focused and raised. Click again — it should hide.

**Test on other desktops:** on a panel still using the old XEmbed tray protocol,
Qt may deliver `DoubleClick` as well. `DoubleClick` is deliberately ignored on
Linux so this cannot toggle twice and end up where it started; single click
should behave identically everywhere.

---

## 8. Core dumps

*Context menu → Miscellaneous → Create Crash Dump → Minimal / Limited / Normal / Full.*

This writes a real **ELF core file**, the same format the kernel produces when a
process dies and the same one `gcore` writes, so the result opens with:

```
gdb /path/to/the/executable  thedump.core
```

"Minimal" dumps anonymous memory only (heap and stacks — where a program's own
state lives) plus the first page of each mapped file so gdb can identify it. The
other three add the file-backed pages, which is much larger but self-contained.

Verified here against a purpose-built multithreaded target: both threads present
with correct LWP ids, fully symbolised backtraces with correct argument values,
globals and heap-pointer contents readable, and shared libraries resolved. The
process resumed correctly afterwards each time, and Cancel removed the partial
file without leaving the target stopped.

**What to check yourself:**

- Dumping a **large, real** application (a browser tab, a database). Multi-GB
  cores are where the timing, the progress bar and Cancel actually get exercised.
  Note the target is held stopped for the duration — that is required for a
  consistent snapshot and is what `gcore` does too, but it is visible on a busy
  process.
- **Unprivileged, on a process that is not a descendant.** Yama blocks the
  ptrace attach, so the memory is dumped but the register state is not. This is
  expected: the dialog says so, and gdb will open the core but cannot produce a
  backtrace. Running elevated gets the registers.
- **A 32-bit process** should be refused with a clear message rather than
  producing a core gdb would misread.

**Under Wayland:** the window views will be **empty**, and no action will work.
This is by design — Wayland deliberately provides no protocol for one client to
enumerate or manipulate another's windows. Under XWayland you will see X clients
only. Worth confirming it degrades quietly rather than erroring.

## 9. Linux-specific features with no Windows counterpart

These are additions rather than ports — things Linux exposes that Windows has no
equivalent for. Two of them fill the holes left by views that were compiled out.

### 9a. Control Group tab — the counterpart of the Windows "Job" view

*Process properties → Control Group.*

A cgroup is what a Windows job object is: a set of processes with shared
accounting and shared limits. On a systemd machine every process is in one, and
the hierarchy (slices, scopes, services) is how the system is really organised.

Shows the cgroup path, memory current/peak/limit/swap, CPU time, **throttling**
(periods and time lost to a `cpu.max` quota — the cause of slowness that looks
inexplicable from inside the process), task counts and per-cgroup pressure.

Verified against the kernel files for plasmashell: 225 MB current / 467 MB peak
/ 234 MB swap / 95 s CPU, controllers `memory pids` — all exact.

**Read only by design.** Writing limits means either running as root or asking
systemd to change the unit's properties; that is a deliberate administrative act
and does not belong behind a properties tab.

**Test:** compare against `systemd-cgtop`, and look at a unit that actually has
limits (`systemctl set-property foo.service MemoryMax=100M`) to see the limit
and throttling rows populate.

### 9b. Security tab — the counterpart of the Windows "Token" view

*Process properties → Security.*

A Windows token holds user, groups and privileges in one object. Linux spreads
the same thing across several places, and this collects them: identity,
container, LSM confinement, seccomp state, `no_new_privs`, all five capability
sets decoded to `CAP_*` names, namespaces compared against pid 1's, OOM score
and inotify watches.

Verified: `cupsd` shows its 41 effective capabilities and profile
`/usr/sbin/cupsd (enforce)`; `plasmashell` shows inheritable `CAP_WAKE_ALARM`
(kernel: `CapInh 0000000800000000`, bit 35 — exact).

**Test:** a snap (`firefox` shows `snap.firefox.firefox (enforce)`, seccomp
`Filtered`, and a private mount namespace), and something with file
capabilities such as `ping`.

### 9c. Pressure (PSI) graph

Replaces the GDI-object and window-object plots, which were permanently empty
boxes on Linux. Three lines — CPU, memory, I/O — showing the share of the last
10 seconds in which at least one task was stalled waiting for that resource.
The tooltip adds the "full" figures (time when *everything* was stalled) and the
60 second averages.

Verified live: with three times as many spinners as CPUs the graph tracked the
kernel from 0.04% to 49%.

The Samba and RAS/VPN plots are also gone from the Linux defaults — their
counters come from Windows-only providers and would sit at zero for ever.

**Getting the new defaults on an existing install:** the default set only
applies when nothing has been configured, so a profile created before this
change keeps the old Windows layout, empty boxes and all. Right-click the graph
bar → **Restore Default Plots** rebuilds it from the platform default. That
entry is new; "Reset Plot" and "Reset All Plots" only clear the plotted history
and never touched the layout. The restored layout is written to the settings
immediately rather than at shutdown, so it survives even if the process is
killed.

**If the graph is flat at zero and never moves**, check the kernel has PSI:
`cat /proc/pressure/cpu`. Absent means `CONFIG_PSI` off or booted with `psi=0`;
the tooltip says so rather than showing a misleading zero.

### 9d. File I/O and Memory-mapped I/O meters

Both read zero before — the Linux backend only ever populated the Disk and
Network totals. They now have real sources:

- **MMapIO** — `pgpgin`/`pgpgout` from `/proc/vmstat`, the traffic between the
  page cache and the block devices. These are kilobytes; confirmed against
  `/proc/diskstats`, whose sector counts come out at exactly twice them.
- **FileIO** — the logical read/write traffic of every process (`rchar`/`wchar`),
  summed from the per-process counters already read each cycle, so it costs no
  extra reads. Deltas are accumulated rather than summing cumulative values, so
  a process exiting cannot make the total jump backwards.

The pair is worth having because they answer different questions. Verified with
a loop doing cached reads: **FileIO 12 GB/s while MMapIO stayed at 147 KB/s and
Disk at 0%** — the reads were served from the page cache, so they are real file
I/O that never touched a disk. An independent measurement over the same window
gave 11.2–11.6 GB/s.

**Caveat worth knowing:** `/proc/<pid>/io` is readable only for your own
processes without privileges, so unprivileged FileIO covers the current user's
processes and running elevated it covers everything. Disk and MMapIO are always
system-wide.

**Test:** `dd if=<large cached file> of=/dev/null bs=1M` in a loop should move
FileIO hard while leaving Disk and MMapIO near zero. Writing to a real disk and
calling `sync` should move all three. (Note `cat file > /dev/null` is *not* a
valid test — coreutils short-circuits it with `copy_file_range` and does almost
no I/O at all.)

### 9e. New process-tree columns

Off by default — enable through the column header context menu.

| Column | Notes |
|---|---|
| **OOM score** / **OOM adjust** | the kernel's current badness, and the settable bias. Sorting by score answers "what dies first if memory runs out" |
| **Container** | docker, podman, cri-o, LXC, systemd-nspawn, snap, flatpak — named where the runtime's convention allows, otherwise "namespaced (mnt, net)" |
| **Confinement** | the AppArmor profile or SELinux context |
| **Inotify watches** | counted on a slow cadence; see below |
| **Control group** | the cgroup path |

**Inotify watches** are worth explaining: exhausting `fs.inotify.max_user_watches`
(128540 here) breaks things far away from the cause — an editor stops noticing
file changes, a sync client silently stalls. Sorting by this column finds the
consumer. It is sampled every 10 seconds rather than every refresh because
counting needs a readlink per open descriptor.

**Test:** `sysctl fs.inotify.max_user_watches` against the column total.

### 9f. Daemons view: all unit types, and the journal

The tab is called **Daemons** on Linux, not Services: "service" there names one
systemd unit type, and this list holds several. It previously listed `.service`
units only. It now also shows
sockets, timers, mounts, automounts, paths, swaps, scopes and slices — 301 units
here against 195 services, matching `systemctl list-units --all` exactly, per
type. Two new columns make the mix readable: **Type** (the unit suffix) and
**Display Name** (systemd's own description).

`.device` and `.target` units are deliberately excluded: devices are one-to-one
udev mirrors and targets are synchronisation points with no process or state —
together they would add ~170 rows that never do anything.

**View Log** in the context menu opens `journalctl -u <unit> -e` in your
terminal. Deliberately the real tool rather than a built-in log pane: the
journal's own pager, filtering and follow mode are better than anything worth
reimplementing here.

**Test:** right-click a unit → View Log. Also confirm **Open Process** works —
it was wired only on Windows before, so it did nothing on Linux.

---

## 9g. The privileged helper (TaskHelper)

Some of what a task manager wants to show is simply unreadable without
privileges. Under the default `kernel.yama.ptrace_scope=1` this includes
processes belonging to *your own user* that this process did not start:

| Wanted | File | Refused because |
|---|---|---|
| I/O counters | `/proc/<pid>/io` | mode 400, other user |
| environment | `/proc/<pid>/environ` | mode 400, other user |
| open files | `/proc/<pid>/fd/` | mode 500, other user |
| memory map | `/proc/<pid>/maps` | readable mode, ptrace-gated at open |
| memory | `/proc/<pid>/mem` | needs PTRACE_MODE_ATTACH |
| stacks, core dumps | ptrace | ptrace_scope, or other user |

Rather than run the whole GUI as root to get these, TaskExplorer starts the
small Qt-free `TaskHelper` elevated and asks it. Same binary and same
length-prefixed `CVariant` protocol as the Windows helper, over an AF_UNIX
socket in `$XDG_RUNTIME_DIR` created 0600 and chowned to the requesting user.

It is **off by default**, because switching it on raises an authentication
prompt and a task manager must not do that merely because it was started.
Turn it on with **Tasks → Use Privileged Helper** (visible only when not
already root). Turning it off terminates the helper.

**What routes through it, once enabled:**

- I/O counters, batched — one request per refresh for every process whose
  `io` could not be read, not one per process
- environment and working directory, on demand
- open file descriptors — the symlink targets and `fdinfo` for the whole
  process in one reply
- memory reads for the memory view, when both `/proc/<pid>/mem` and
  `process_vm_readv` are refused
- thread stack traces (unprivileged helper, deliberately — see section 5)
- core dumps: the helper stops the threads and returns their register sets,
  the maps and the auxiliary vector; the GUI still writes the ELF file

**Why core dumps are a session rather than a call.** A useful core needs every
thread held stopped for the whole time its memory is read, or the snapshot is
internally inconsistent. So `DumpAttach` stops the threads and hands back what
is only valid while stopped, memory is then read with as many `ReadProcMemory`
calls as it takes, and `DumpDetach` releases them. The ELF format knowledge,
the progress reporting and the file ownership all stay in the GUI; the helper
only supplies privileged primitives.

**A stopped process must never be orphaned.** All three of the helper's exit
paths release the target, and this was verified: SIGTERM (handled, exits
through the loop), SIGKILL (the kernel detaches a dead tracer and resumes its
tracees), and the idle timeout. In each case the target went from state `t`
back to `S`.

**The helper is not a trusted-input parser.** It accepts a pid and a leaf name
and checks the leaf against a whitelist. Verified refused: `maps`, `status`,
`../../etc/shadow`, `fd/../../../etc/shadow`, `fd/x`, `fd/`, `fd/3;ls`. Only
`exe`, `cwd`, `root` and `fd/<digits>` resolve as links.

**Test:**

1. With the helper off, find a process of another user (`cupsd`, `systemd`) and
   confirm its I/O columns and Files tab are empty.
2. **Tasks → Use Privileged Helper** → authenticate once.
3. The same columns should fill in, and the Files tab should list descriptors.
4. Dump another user's process — it should produce a core with register state,
   loadable in gdb with a real backtrace.
5. Uncheck the menu item; `pgrep -x TaskHelper` should go to zero.

**Not verified here, because it needs your password:** everything above from
step 2 onward. What was verified without elevation is the whole mechanism —
protocol, batching, whitelist, dump session, thread stop and resume, and all
three cleanup paths — using an unprivileged helper against a target that opts
in with `prctl(PR_SET_PTRACER_ANY)`. Only the privilege itself is untested, and
that part is the kernel's, not this code's.

---

## 10. Other distributions

Qt and the other libraries that cannot be relied upon are bundled next to the
executable and found through `$ORIGIN`, so `Bin/linux-x86_64` is a
self-contained directory that runs from wherever it is unpacked. The one thing
bundling cannot fix is the glibc floor, which is set by the machine the build
was compiled on - see **Installer/README-portable.md**, and note that a build
made on a current distribution is fine for development but not shippable.

Everything was developed and tested on Ubuntu 26.04 with systemd 259, KDE/X11,
cgroup v2, on a QEMU guest. These are the parts most likely to differ elsewhere:

- **Non-systemd** (Devuan, Alpine, Void) — the services list should be empty
  rather than erroring; sockets and everything else should still work.
- **cgroup v1** — service detection parses the `name=systemd` hierarchy line;
  only the v2 unified path has been exercised.
- **A real GPU** — `gpu_busy_percent` (amdgpu) and VRAM reporting are
  **completely untested**; this VM has an emulated `bochs-drm` adapter with no
  counters at all. NVIDIA needs NVML and is not implemented.
- **A machine with >64 CPUs** — the affinity mask is 64-bit because that is what
  the shared interface uses. `LinuxGetAffinity` sets a `truncated` flag, but the
  GUI does not surface it yet.
- **32-bit or ARM64** — `/proc/cpuinfo` topology parsing falls back to logical
  CPU count when `physical id` / `core id` are absent (as on arm64). The ELF
  arch string reader handles both classes but has only seen x86-64.
- **A container** — `sock_diag` may be refused, in which case the code falls
  back to `/proc/net` (losing per-socket byte counters, which is the documented
  trade-off).

---

## 11. Distribution logos

The `DistroLogos/` folder next to the executable ships **empty except for a
README** — I did not add artwork, since which logos you may redistribute is a
licensing decision for you.

Fallback to the distribution's own installed logo is verified working
(picked up Ubuntu's from `/usr/share/pixmaps`), as is install-dir priority
(a test PNG overrode it without a rebuild).

**Test:** drop `ubuntu.png` / `debian.png` / `fedora.png` etc. into
`Bin/<platform>/DistroLogos/` and confirm they appear. Naming rules are in that
folder's `README.txt`.

---

## 12. Long-running stability

The longest continuous run during development was a few minutes. Not tested:

- Memory growth over hours (the per-refresh `/proc` walk allocates heavily).
- Behaviour across suspend/resume — CPU and disk counters are delta-based and
  guarded against going backwards, but this was never exercised for real.
- A system with thousands of processes or sockets. `UpdateOpenFileList` walks
  every process's fd table and is the most expensive refresh in the backend.

---

## Known-empty, by design — not bugs

So these are not mistaken for regressions:

| View | Why |
|---|---|
| Heap | glibc publishes no enumerable heap arenas to an outside observer |
| Windows (under Wayland) | no cross-client window protocol exists |
| GPU utilisation (this VM) | emulated adapter exposes no counters |
| DNS cache entries (no systemd-resolved) | glibc's resolver does not cache, so there is nothing to list |
| Token, Job, GDI, .NET, RPC, Pool, Atom | Windows kernel concepts; these views are compiled out |
| UDP socket transfer rates | the kernel has no `tcp_info` counterpart for UDP |
| "Other" line on the File I/O graph | Linux keeps no tally of operations that are neither reads nor writes; the line is omitted rather than drawn flat |
| Per-socket rates on a container without `sock_diag` | `/proc/net` exposes queue depths only |

---

## Still unimplemented

Tracked as `linux-todo` comments in `TaskExplorer/API/Linux/`:

- **Stack traces are unwound by TaskHelper**, which links libdwfl (elfutils)
  directly. The earlier implementation shelled out to `eu-stack` and then
  `eu-addr2line` and parsed their text; that is gone, along with its parsing
  bugs. `eu-stack` is no longer required at all.

  Why a helper process rather than in-process libdwfl: unwinding parses DWARF out
  of arbitrary, sometimes corrupt binaries, and it holds a ptrace attachment
  which blocks every other debugger on the system while held. A crash or hang
  costs a helper the GUI respawns. The helper also has no Qt linked at all.

  What this bought, measured on a 129-thread process: **127 ms → 46 ms**, because
  `eu-stack` has no per-thread option and had to unwind every thread (1033
  frames) to keep the ten you asked for. libdwfl's `dwfl_getthread_frames`
  unwinds exactly one. Frames now also carry the **offset into the symbol**
  (`sleep+0x3d`) and a source location, in one pass instead of two processes.

  **debuginfod is disabled in the helper, deliberately.** This one is worth
  knowing about: `/etc/debuginfod/elfutils.urls` sets
  `DEBUGINFOD_URLS=https://debuginfod.ubuntu.com` system-wide, and
  `dwfl_standard_find_debuginfo` will then try to *download* debug information
  for any module that has none locally — with no timeout. Viewing the stack of a
  process built without `-g` blocked indefinitely. Proven by measurement:
  `dwfl_module_getsrc` hung with it set and returned instantly with it cleared.

  This also corrects something recorded here earlier. The `eu-stack` "hang" noted
  in section 5 was almost certainly this, not a libdw defect — same code path,
  same missing-debuginfo trigger. libdw itself unwound 129 threads in 0.1 ms.

  Beyond the hang, a task manager should not make network requests to draw a
  list, the request tells a third party which binaries are being inspected, and
  the helper often runs as root. Locally present debug info is still used,
  including the compressed MiniDebugInfo in `.gnu_debugdata`.

- **Thread stack traces** — *implemented*, using `eu-stack` from elfutils
  (full DWARF CFI unwinding, so it works on binaries built without frame
  pointers). Two things to know:
  - it needs the **elfutils** package; without it the stack view says so
  - it needs **ptrace access**, so unprivileged it only works for descendants
    and otherwise reports why. Confirmed working when running elevated.
- **Module injection** — would need ptrace to force a `dlopen` into the target.
  This is the last genuinely unimplemented process action.
- **Closing a socket** (`SOCK_DESTROY`) and **closing another process's file
  descriptor** — the kernel offers no general interface for either.
- **Freeze / UnFreeze** — hidden by design, not pending. The cgroup v2 freezer
  acts on a whole cgroup, so freezing a desktop app would freeze the session
  around it. `Suspend` / `Resume` (SIGSTOP/SIGCONT) are the per-process
  equivalent and are available.
- **Wine / WSL process classification**, `RLIMIT_RSS` — cosmetic gaps.

### Landed since the first pass — worth a look

| Feature | What to check |
|---|---|
| Core dumps | see section 8 |
| Debugger attach | *Miscellaneous → Debug* opens your terminal running `gdb -p <pid>`. Unchecking it explains that only the debugger can detach, and names the tracer. |
| Main Window menu | Bring in front / Restore / Minimize / Maximize / Close now work — they were built but wired only on Windows, so they previously did nothing. Close and Quit are the same action on X11. |
| Description column | now filled from `.note.package` (the packaging metadata Ubuntu embeds) or the application's `.desktop` entry. Daemons stay blank on purpose rather than being described by guesswork. |
| Version / Company columns | from `.note.package`, else the SONAME or resolved library file name (`libstdc++.so.6` → `6.0.35`). |
| Driver "Binary Path" | resolved from `modules.dep`. All 52 modules loaded here matched `modinfo -n` exactly. |
| Handle access column | full `O_*` decoding — "Read/Write, Append", "Path only", and so on. |
| Thread I/O priority and affinity | now readable and settable per thread, not just per process. |
| Thread "Start Address" | resolved to `module+offset` when the kernel supplies an address — but **current kernels hardcode `kstkeip` to 0 for everyone, root included**, so in practice this column stays empty. Verified: even a process reading its own `/proc/self/stat` gets 0. The same applies to the thread Stack Usage figure. Left in place because the code is free and correct if a kernel ever provides the field again. |
| "Elevated" column | now also true for a process holding capabilities without being root (`ping`, `dumpcap`). |
