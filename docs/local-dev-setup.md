# Local dev setup: syncing this fork and compiling firmware on Windows/WSL

This documents the two recurring tasks for this fork (`billitheo-tech/rusefi-PIP6`):
pulling in upstream `rusefi/rusefi` changes, and compiling firmware locally on a
Windows machine. Written after doing both for the first time in 2026-07;
see `docs/report.md` for the full session narrative.

## 0. GitHub Actions on this fork: it compiles, and it publishes binaries

Investigated 2026-09-13 because the standing belief was "the firmware has never
compiled on GitHub". That was true only for the first two commits in May 2026;
it has not been true since.

### Why the May 2026 CI runs were red

| Commit (pre-rebase hash) | What failed | Root cause |
|---|---|---|
| `19630d8dee`, `345ead6cbe` | `Firmware on Windows`, `Unit Tests`, `Simulator`, `Configs & Live Docs` - all at the compile step | Real compile error in `configureFordPip6`: `setTriggerSynchronizationGap(0.55, 0.95)` and `setSecondTriggerSynchronizationGap(1.45, 1.85)` were called with two arguments, but both take **one** (`trigger_structure.h`). |
| `110180414b` | `rusEFI validate console` (Java tests) | Fixed the compile error by switching to `setTriggerSynchronizationGap2(from, to)` / `setSecondTriggerSynchronizationGap2(from, to)`. The Java failure was **also red on upstream `rusefi/rusefi` at the same base commit** `1c766c0027` - inherited, not ours. |
| `110180414b` | `Unit Tests on Windows` | Failed in a `Post Run` cleanup step while Linux unit tests passed on the same commit - a runner flake. |

Every push since the 2026-07-14 rebase (`a3b2cec54e`, `4d814079df`,
`1e66f50511`) is fully green. The only red since is a 2026-08-11 nightly that
died in an unrelated `mre_f4` simulator step.

### Consequence: flashable uaefi_pro firmware is published from the public fork

`.github/workflows/build-firmware.yaml` (`Firmware at GHA`) runs on **every push
to master and every night at 00:27 UTC** (`cron: '27 0 * * *'`) - the schedule
trigger is not gated to `rusefi/rusefi`, so it fires on forks too. Each run builds
all ~40 boards and uploads, as workflow artifacts with 90-day retention:

- `rusefi_bundle_uaefi_pro.zip` (~65 MB: `rusefi_update.srec`, `.ini`, console)
- `rusefi_bundle_uaefi_pro_autoupdate.zip`
- on non-push runs also `rusefi_uaefi_pro.bin` / `.hex` / `.elf` / `.map` / `.dfu`

Anyone logged in to GitHub can download artifacts from a public repo's runs, so
the PIP6 trigger code is out there compiled and ready to flash, not just as
source. Upload to rusefi.com's server and the "Nightly" GitHub release do **not**
happen from the fork - those steps require `github.repository == 'rusefi/rusefi'`
and secrets the fork does not have.

To stop publishing binaries while keeping the source public (the source-sharing
obligation is met by the repo itself):

1. GitHub -> fork -> **Settings -> Actions -> General -> "Disable actions"**.
   This is the only switch that also stops the nightly cron. It needs the repo
   owner's login; it cannot be done from a clone.
2. Optionally delete already-published artifacts: each run page ->
   artifact row -> trash icon (or let them expire; the API lists `expires_at`).

Do this **before** the `git push --force-with-lease` in section 1 - that push is
itself a `push` event and will trigger a fresh full build otherwise.

**Decision 2026-09-13: Actions stays enabled for now** (owner's call - close to
the project goal, CI green is useful). Revisit before any product ships. The
switch above is the one to flip when that time comes.

Checking without the `gh` CLI (none installed on this machine; the public API
needs no token for reads):

```bash
# recent runs
curl -s "https://api.github.com/repos/billitheo-tech/rusefi-PIP6/actions/runs?per_page=20"
# artifacts of one run
curl -s "https://api.github.com/repos/billitheo-tech/rusefi-PIP6/actions/runs/<run_id>/artifacts?name=rusefi_bundle_uaefi_pro.zip"
```

Run *logs* are not readable without a token (403); job/step names and
conclusions are (`.../runs/<run_id>/jobs`).

## 1. Syncing this fork with upstream rusefi/rusefi

There is no separate "uaefi pro" branch upstream - `uaefi_pro` is just one board
variant built from `rusefi/rusefi`'s regular `master`. "Sync with the latest uaefi
pro snapshot" means sync this fork's `master` with upstream `rusefi/rusefi`'s
`master`.

```bash
# one-time: add the upstream remote
git remote add upstream https://github.com/rusefi/rusefi.git

# fetch latest upstream history (also pulls submodule refs)
git fetch upstream master

# a previous local build leaves tracked *generated* files modified
# (firmware/controllers/generated/*, firmware/tunerstudio/generated/*.ini,
# hw_layer/mass_storage/ramdisk_image*.h, VariableRegistryValues.java).
# They are build outputs, never to be committed - discard them or the rebase
# refuses to start. Check `git status --short` first: it should list ONLY
# generated files before you run this.
git checkout -- .

# see how far behind we are and confirm upstream did not touch our file
git rev-list --count master..upstream/master
git log --oneline master..upstream/master -- firmware/controllers/trigger/decoders/trigger_ford.cpp

# rebase local commits on top of upstream - do a dry run on a throwaway branch
# first if you want to check for conflicts before touching master:
git branch -f _sync_test HEAD
git checkout _sync_test
git rebase upstream/master
# inspect, then: git checkout master && git branch -D _sync_test

# once happy, rebase master for real
git checkout master
git rebase upstream/master
git branch -D _sync_test

# confirm the custom trigger code came through byte-identical
git diff origin/master master -- firmware/controllers/trigger/decoders/trigger_ford.cpp   # expect empty

# CRITICAL - see gotcha #1 below before compiling
git submodule status | grep '^+'        # lists what is stale (3 of 17 on 2026-09-13)
git submodule update --init --recursive

# rebase rewrites commit hashes, so this fork's origin needs a force-push
git push --force-with-lease origin master
```

Force-push is safe here only because the rebased commits are ours alone (the
Ford PIP6 trigger work) and nothing else depends on their old hashes. Always
confirm with whoever owns the fork before force-pushing a shared branch.

### Gotcha #1: stale submodules after rebase/pull (real incident, 2026-07-14)

Rebasing (or any other operation that moves `HEAD` across commits) updates the
gitlink pointers for `firmware/ChibiOS`, `firmware/ChibiOS-Contrib`, and
`firmware/libfirmware`, but does **not** update the actual checked-out content of
those submodules. If you compile without running `git submodule update`, you get
confusing compile errors that look like real code bugs but aren't, e.g.:

```
console/binary/ts_can_channel.cpp:118:108: error: 'ECU_ISO_TP_SETTINGS' is not a
member of 'bench_test_packet_ids_e'
```

`bench_test_packet_ids_e` was extended in a newer `firmware/libfirmware` commit
than what was checked out. Fix:

```bash
git submodule status   # any entry prefixed with '+' is out of sync
git submodule update --init --recursive
```

Run this after every `git pull`/`git rebase`/`git fetch && git reset` against
upstream, not just the first time. It bit again on 2026-09-13: after the rebase
`firmware/ChibiOS`, `firmware/libfirmware` and `unit_tests/googletest` were all
stale (`+`-prefixed) until updated.

### Sync history

| Date | Upstream commits pulled in | Conflicts | trigger_ford.cpp touched upstream? |
|---|---|---|---|
| 2026-07-14 | ~2 months' worth | none | no (only `configureFordCoyote`) |
| 2026-09-13 | 897 | none | no |

## 2a. New Windows PC, from zero to `rusefi.bin` (checklist)

Everything needed to reproduce the local build on a machine that has never seen
this project. Each step was either performed on the current machine or verified
against it on 2026-09-13. Budget ~1 hour of setup plus ~35 min for the first build.

**A. Windows side**

1. Windows 10/11 with virtualization enabled in BIOS. Install WSL2 + Ubuntu
   from an admin PowerShell, then reboot:
   ```powershell
   wsl --install -d Ubuntu
   ```
   Current machine: Ubuntu 26.04 LTS, WSL2 kernel 6.6.x. Any Ubuntu 22.04+
   should be fine - the version constraint that matters is the ARM gcc range
   below, not the distro.
2. Install Git for Windows (https://git-scm.com). Default options are fine,
   including `core.autocrlf=true` - `.gitattributes` forces `eol=lf` on `*.sh`,
   `*.c/.cpp/.h`, `*.mk`, `*.env` etc., so scripts stay Unix-clean even in a
   Windows-side checkout. (Current machine: git 2.54 Windows-side, 2.53 inside WSL.)
3. Set git identity on both sides (Windows and inside WSL):
   ```bash
   git config --global user.name  "billitheo"
   git config --global user.email "billitheo@gmail.com"
   ```
4. GitHub auth for pushing: either an HTTPS Personal Access Token (classic,
   `repo` scope) entered when git prompts, or SSH keys added to the GitHub
   account. Cloning needs no auth - the fork is public.

**B. Clone the fork (with submodules)**

Pick the location deliberately - see gotcha #2. NTFS (`C:\Users\<you>\rusefi-PIP6`)
is what this machine uses and is convenient from Windows tools, but costs
roughly 3x build time versus a WSL-native path (`~/rusefi-PIP6`). Either way the
build runs *inside* WSL.

```bash
# from a WSL shell; drop --recurse-submodules and run the submodule line later if bandwidth is a problem
git clone --recurse-submodules https://github.com/billitheo-tech/rusefi-PIP6.git
cd rusefi-PIP6
git remote add upstream https://github.com/rusefi/rusefi.git
git submodule update --init --recursive     # 17 submodules; no-op if --recurse-submodules worked
```

Expect ~2.7 GB in `.git` plus the working tree. If you cloned onto NTFS from
Windows Git instead, it is the same repo - WSL sees it at
`/mnt/c/Users/<you>/rusefi-PIP6`.

**C. Toolchain inside WSL**

```bash
sudo apt-get update
sudo apt-get install -y make git xxd zip 7zip mtools dosfstools python3 \
    gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
    openjdk-17-jdk-headless
```

Verify before building:

```bash
arm-none-eabi-gcc --version | head -1   # need 11.3.1 <= version < 15.0; current machine 14.2.1
java -version                            # 17 is what CI uses; 11 and 25 both known to work
make --version | head -1                 # 4.x
git -C ~/rusefi-PIP6 submodule status | grep -c '^[+-]'   # must print 0
```

If the distro's `gcc-arm-none-eabi` is outside the accepted range, use the
project's pinned toolchain instead: `cd firmware && ./provide_gcc.sh` (or the
whole `./setup_linux_environment.sh`), which drops one under
`~/.rusefi-tools/gcc-arm-none-eabi/bin` - put that on `PATH` ahead of `/usr/bin`.

Optional, only for unit tests / simulator / Windows console cross-build:
`sudo apt-get install -y build-essential gcc-multilib g++-multilib g++-mingw-w64 gcc-mingw-w64`.

**D. Build**

```bash
cd ~/rusefi-PIP6/firmware/config/boards/hellen/uaefi      # or /mnt/c/Users/<you>/rusefi-PIP6/...
./compile_firmware_pro.sh 2>&1 | tee ~/compile_uaefi_pro.log
```

First build on this machine (NTFS, 8 vCPU, 7 GB): **31m 40s**, of which the
first ~10 min is Gradle downloading and building the Java code generators (needs
internet the first time; cached under `~/.gradle` afterwards).

**E. Verify** (do not trust the exit code alone - see section 3)

```bash
grep -c "error:" ~/compile_uaefi_pro.log                     # 0
ls -la ../../../../build/rusefi.{bin,srec,elf}                # fresh timestamps
grep -aoE "uaefi_pro|TT_FORD_TFI_PIP_6" ../../../../build/rusefi.bin | sort -u   # both lines
grep -m1 signature ../../../../tunerstudio/generated/rusefi_uaefi_pro.ini      # this .ini matches this .bin
```

**F. Outputs to keep together**

| File | Use |
|---|---|
| `firmware/build/rusefi.bin` | complete image for DFU / ST-Link flashing of a blank or bricked ECU |
| `firmware/build/rusefi.srec` | raw firmware in S-record form (not the bootloader-update image) |
| `firmware/tunerstudio/generated/rusefi_uaefi_pro.ini` | TunerStudio project file; its `signature` must equal the one baked into the `.bin` |
| `firmware/deliver/rusefi_update.srec` + bundle zip | only produced by the `bundle` target (`bash bin/compile.sh config/boards/hellen/uaefi/meta-info-uaefi_pro.env build_both_bundles` from `firmware/`); this is the file the rusEFI console/bootloader updater flashes |

**G. Ongoing** - every later session is section 1 (sync) followed by D and E.

## 2. Compiling firmware locally (Windows, via WSL)

`firmware/readme.md` states Windows is supported via Cygwin or WSL, and Linux
(so WSL's native ext4 filesystem, not the `/mnt/c` NTFS mount) is recommended -
NTFS is much slower for a build this size. A verified, working setup:

- Windows: WSL2 with an Ubuntu distro (`wsl --install -d Ubuntu` if you don't
  have one).
- Clone (or keep) the repo **inside the WSL filesystem** (e.g. `~/rusefi-PIP6`)
  for build speed, in addition to / instead of a `/mnt/c/...` checkout. See
  gotcha #2 below for what this means in practice.
- Inside WSL Ubuntu, from the repo's `firmware/` directory, the official
  one-shot setup script is `firmware/setup_linux_environment.sh`:

  ```bash
  cd firmware
  ./setup_linux_environment.sh
  ```

  This runs `git submodule update --init`, `apt-get update`, installs
  `misc/actions/ubuntu-install-tools.sh`'s package list plus
  `build-essential gcc gdb gcc-multilib make openjdk-11-jdk-headless xxd`,
  and downloads/pins an `arm-none-eabi-gcc` toolchain under
  `~/.rusefi-tools/gcc-arm-none-eabi` (added to `PATH` via `~/.profile`).

  Equivalently, on this machine the ARM toolchain came from the
  `gcc-arm-none-eabi` apt package instead (`arm-none-eabi-gcc 14.2.1`), which
  also satisfies the version requirement and matches what CI pins
  (`14.2.Rel1` in `.github/workflows/build-firmware.yaml` /
  `hardware-ci.yaml`). Either source works, as long as `arm-none-eabi-gcc` on
  `PATH` is between 11.3.1 and 15.0 (`firmware/gcc_version_check.c`).

### What is actually required (verified inventory, 2026-09-13)

The machine that has built `uaefi_pro` successfully twice (2026-07-14 and
2026-09-13) runs **Ubuntu 26.04 LTS on WSL2** (kernel 6.6.114, 8 vCPU, 7 GB RAM)
and has exactly this installed - checked with `dpkg-query`, not assumed:

| Package | Version | Role |
|---|---|---|
| `gcc-arm-none-eabi` | 15:14.2.rel1-1 (`arm-none-eabi-gcc 14.2.1`) | the cross-compiler; must be within 11.3.1-15.0 (`firmware/gcc_version_check.c`); same major as CI's `14.2.Rel1` |
| `binutils-arm-none-eabi` | 2.45 | linker/objcopy for the above |
| `libnewlib-arm-none-eabi` | 4.6.0 | C library for the ARM target |
| `libstdc++-arm-none-eabi-newlib` | 15:14.2.rel1-1 | C++ library for the ARM target |
| `make` | 4.4.1 | build driver (`firmware/bin/compile.sh` runs `make -j$(nproc)`) |
| `git` | 2.53 | submodules, `git describe` for version stamping |
| a JDK - `openjdk-11-jdk-headless` is installed; **the `java` on PATH is OpenJDK 25.0.3-ea** | | Gradle + `java_tools/` code generators (`config_definition`, `enum_to_string`) run at the start of every board build. CI uses Java 17; 25 works |
| `xxd` | 9.1 | hex dumps used by the config generators |
| `mtools`, `dosfstools` | 4.0.49 / 4.2 | build the FAT ramdisk image (`hw_layer/mass_storage/ramdisk_image*.h`) |
| `zip`, `7zip` | | bundle packaging |
| `python3` | 3.14 | some helper scripts |

Install command that reproduces this set on a fresh Ubuntu/WSL:

```bash
sudo apt-get update
sudo apt-get install -y make git xxd zip 7zip mtools dosfstools python3 \
    gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
    openjdk-17-jdk-headless
git submodule update --init --recursive
```

**Not installed here, and not needed for the firmware image**: host `gcc`,
`g++`, `build-essential`, `gcc-multilib`/`g++-multilib`, `gcc-mingw-w64`/
`g++-mingw-w64`, `gdb`, `sshpass`, `colordiff`. Those are what the official
scripts (`firmware/setup_linux_environment.sh` -> `misc/actions/ubuntu-install-tools.sh`)
add, and they are for the **unit tests** (host gcc/g++), the **simulator**
(host gcc + multilib) and **cross-building the Windows console** (mingw). If you
only ever need `rusefi.bin`/`.srec` for the ECU, skip them. If you want to run
`unit_tests/test.sh` locally, add `build-essential gcc-multilib g++-multilib`.

The official one-shot alternative is still valid and does everything above plus
the extras, plus downloads its own pinned ARM toolchain to
`~/.rusefi-tools/gcc-arm-none-eabi` (added to `PATH` via `~/.profile`):

```bash
cd firmware && ./setup_linux_environment.sh
```

On this machine that script was **not** used (`~/.rusefi-tools` does not exist);
the apt `gcc-arm-none-eabi` package was installed instead. Either way is fine as
long as `arm-none-eabi-gcc --version` lands in the accepted range.

### "~50 dependencies" on first install - yes, expected

`ubuntu-install-tools.sh` + the extra packages in `setup_linux_environment.sh`
look like a short list, but several of them are metapackages that apt expands
into many transitive installs on a fresh system:

- `g++-mingw-w64` / `gcc-mingw-w64` - a **second, full cross-compiler
  toolchain** (Windows-target GCC), pulls in ~15-20 packages
  (`binutils-mingw-w64-*`, `gcc-mingw-w64-base`, `gcc-mingw-w64-{i686,x86-64}`,
  `mingw-w64-{common,i686-dev,x86-64-dev}`, etc). Used to cross-build Windows
  console/updater executables from Linux CI.
- `gcc-multilib` / `g++-multilib` - pulls in 32-bit runtime/dev libs
  (`lib32gcc-s1`, `libc6-dev-i386`, etc).
- `openjdk-11-jdk-headless` - a full JDK, even "headless" pulls ~20-30
  packages (`ca-certificates-java`, `fonts-dejavu-core`, font/crypto/locale
  support, etc). Needed for the Java code-gen tools (`java_tools/`) that run
  as part of every board compile.
- `build-essential` - baseline compiler/libc-dev/make chain, another ~10.

So seeing apt report on the order of 50 new packages the first time is normal,
not a sign anything is wrong. It's a one-time cost; nothing here needs
reinstalling on subsequent syncs.

Note: the script installs `openjdk-11-jdk-headless` specifically, but CI
(`.github/actions/setup-java`) uses Java 17 (Zulu), and this machine's WSL
Ubuntu had Java 25 (`openjdk 25.0.3-ea`) already installed and it built fine.
Newer JDKs than 11 have worked in practice; don't block on getting exactly 11.

### Gotcha #2: two independent clones can exist (`/mnt/c/...` vs `~/...`)

Because `/mnt/c/Users/<you>/rusefi-PIP6` (Windows-mounted) and
`~/rusefi-PIP6` (WSL-native) can both be valid, independent git clones of the
same fork, they can silently diverge - e.g. one has been rebased/pushed and the
other hasn't. Before trusting a build artifact or debugging a "the code doesn't
match" mystery, check which clone you're actually in and compare
`git rev-parse HEAD` / `git log --oneline -3` between them. This is not a bug,
just something to keep straight - both are legitimate, it's up to you to know
which one produced a given binary.

Concrete state on 2026-09-13: `~/rusefi-PIP6` (WSL-native) was still at the
**May** commit `110180414b` - it was never rebased in July. Both the 2026-07-14
and the 2026-09-13 builds came from the NTFS clone
`/mnt/c/Users/bhunt/rusefi-PIP6`, which is the one that tracks `origin/master`.
Treat the WSL-native clone as abandoned unless you deliberately re-sync it
(`cd ~/rusefi-PIP6 && git fetch origin && git reset --hard origin/master &&
git submodule update --init --recursive`).

## 3. Board compile scripts (uaefi family)

Each board+variant has its own script under
`firmware/config/boards/hellen/uaefi/`, all thin wrappers around
`firmware/bin/compile.sh` with a variant-specific `meta-info*.env`:

| Script | Variant | `PROJECT_CPU` | Notes |
|---|---|---|---|
| `compile_firmware.sh` | `uaefi` (base) | `ARCH_STM32F4` | |
| `compile_firmware_pro.sh` | `uaefi_pro` | `ARCH_STM32F7` | Matches `rusefi_uaefi_pro.ini`; used for the Ford 4.9L PIP6 hardware |
| `compile_firmware_h7.sh` | `uaefi_pro_h7` | `ARCH_STM32H7` (`STM32H723xx`) | |
| `compile_uaefi-bundle.sh` | `uaefi` (base) | `ARCH_STM32F4` | Builds the full distributable bundle (console + update files), not just the firmware image |

```bash
cd firmware/config/boards/hellen/uaefi
./compile_firmware_pro.sh
```

From Windows without opening a WSL shell (this is how the 2026-09-13 build was
driven, with the log kept for the record):

```powershell
wsl.exe -e bash -lc 'cd /mnt/c/Users/bhunt/rusefi-PIP6/firmware/config/boards/hellen/uaefi && ./compile_firmware_pro.sh' 2>&1 | Tee-Object compile_uaefi_pro.log
```

**Verify success by artifacts, not exit code.** `firmware/bin/compile.sh` has
been observed to exit 0 even when `make` failed (see CLAUDE.md). After the run
check that `firmware/build/rusefi.elf` has a fresh timestamp, and that
`grep -c "error:" compile.log` is 0. A cheap identity check on the result:

```bash
grep -aoE "uaefi_pro|TT_FORD_TFI_PIP_6" firmware/build/rusefi.bin | sort -u
```

Both strings must appear - the first proves the board variant, the second that
the PIP6 trigger type is compiled in.

The default `make` target (what these scripts invoke) writes to
`firmware/build/`: `rusefi.bin`, `rusefi.hex`, `rusefi.srec`, `rusefi.elf`,
`rusefi_crc32.bin`. `firmware/deliver/` and the full bundle zip only get
populated by the `bundle`/`build_both_bundles` targets (see `compile_uaefi-bundle.sh`
or `firmware/bundle.mk`), which CLAUDE.md's Build Commands section describes.

Identifying which physical board you have: read the part number silk-screened
on the main STM32 MCU package itself (`STM32H723` = H7, `STM32F7...` = F7/pro,
`STM32F4...` = base). There is no ESP32 or other secondary chip involved in
this identification for the uaefi family.

## 4. Known-good firmware backups

See `known-good-firmware/` at the repo root for preserved, verified-working
firmware builds (currently: `ford-4.9L-pip6-uaefi_pro-2026-05-08/`), kept
independent of any single machine/clone.
