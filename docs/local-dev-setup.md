# Local dev setup: syncing this fork and compiling firmware on Windows/WSL

This documents the two recurring tasks for this fork (`billitheo-tech/rusefi-PIP6`):
pulling in upstream `rusefi/rusefi` changes, and compiling firmware locally on a
Windows machine. Written after doing both for the first time in 2026-07;
see `docs/report.md` for the full session narrative.

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

# rebase local commits on top of upstream - do a dry run on a throwaway branch
# first if you want to check for conflicts before touching master:
git branch -f _sync_test HEAD
git checkout _sync_test
git rebase upstream/master
# inspect, then: git checkout master && git branch -D _sync_test

# once happy, rebase master for real
git checkout master
git rebase upstream/master

# CRITICAL - see gotcha #1 below before compiling
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
upstream, not just the first time.

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
