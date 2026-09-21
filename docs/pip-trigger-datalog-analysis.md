# Ford TFI PIP trigger: validating the decoder against a TunerStudio datalog

How to sanity-check `configureFordPip6` / `configureFordPip8`
(`firmware/controllers/trigger/decoders/trigger_ford.cpp`) against a real
engine `.mlg` log, what the 2026-09-20 inline-6 log showed, why the wheel was
rotated +7 deg, and the exact steps to repeat this when the V8 PIP8 log arrives.

Tooling lives in `tools/pip_trigger_analysis/` (python3, no packages):

- `mlg.py` - reads MegaLogViewer `.mlg` (TunerStudio and rusEFI console
  variants, header format 1 and 2; field header size is inferred from the
  header offsets so both 55- and 89-byte field records work). `mlg.py FILE`
  lists channels; `mlg.py FILE chan1 chan2 ...` dumps CSV.
- `analyze_pip.py --trigger pip6|pip8 FILE.mlg [...]` - the whole analysis
  below in one command. It parses the *current* coded wheel out of
  `trigger_ford.cpp`, so it never drifts from the decoder.

Run from the repo root (WSL: `python3 tools/pip_trigger_analysis/analyze_pip.py
--trigger pip8 "/mnt/c/Users/<you>/Documents/TunerStudioProjects/<proj>/DataLogs/<file>.mlg"`).
On this machine Windows has no `python3` on PATH - use the WSL shell.

## 1. What a good log needs

- Engine running for at least ~20 s with some RPM variation (idle to 2500+
  is ideal). Two of the six 2026-09-20 logs had the engine off and are useless.
- Default TunerStudio logging already includes everything needed:
  `RPM`, `sync: instant RPM`, `sync: instant RPM range`,
  `trgSync: Trigger Latest Ratio`, `trgtriggerStateIndex`,
  `trgsync: wheel sync counter`, `Trigger Error Counter`,
  `trgtriggerCountersError`, `sync: We have sync`,
  `Sync: trigger angle error`, `revolutionCounterSinceStart`, `Warning: last`.
- The firmware signature is embedded in the log header; the tool prints it.
  Check it is the fork's build (`3342137515` family in 2026-09), not an
  upstream nightly - see `docs/local-dev-setup.md` section 0 for the
  autoupdater trap.

## 2. How to read the output

1. **Sync health** - `Trigger Error Counter` and `trgtriggerCountersError`
   must not increase while running; `sync-lost events` must be 0; `wheel sync
   counter` and `revolutionCounter` must advance by the same amount, and that
   amount must be crank revs / 2 (PIP is a cam-speed wheel, one sync per 720).
   An error at engine stop is normal.
2. **Per-index table** - `trgtriggerStateIndex` is the position in the event
   list starting at the sync point (index 0 = the event where sync fires).
   `Trigger Latest Ratio` only updates on sync-edge (FALL) events, so each
   value appears on a fall index and repeats on the following rise index.
   Ratio = (interval ending at this fall) / (interval before it).
3. **Instant RPM vs RPM** - `sync: instant RPM` is computed *per edge* as
   (coded angle difference back to the edge ~90 deg earlier) / (measured
   time) - `firmware/controllers/trigger/instant_rpm_calculator.cpp`. It uses
   the *coded* angles, so a tooth that is physically not where the code says
   produces a fixed per-index bias. `RPM` is sync-to-sync and immune.
   Discriminator: if `instant RPM range` is a constant percentage of RPM from
   idle to high RPM and locked to index, it is geometry; real combustion
   speed ripple is large at idle and shrinks with RPM.
4. **Geometry back-out** - the tool aligns the measured per-fall ratio
   sequence to the coded one (best rotation), then chains them into
   intervals normalised to 720 deg and prints measured vs coded intervals
   and implied fall angles. `|delta| <= 2 deg` is noise; a stable 5+ deg
   delta is a wheel-model error worth fixing.
5. **Warnings** - 9007/9008 `PRIMARY_BAD_TOOTH_TIMING_EARLY/LATE` fire when a
   tooth arrives >10 deg from where the decoder expected it at >1000 RPM
   (`trigger_central.cpp`). Non-fatal (tooth still accepted), but a
   persistent 9007 on one index means that index's coded angle is off.
6. **Sync window check** - exactly one fall should print `SYNC POINT`
   (primary window contains its ratio AND second window contains the previous
   ratio). A second fall marked `primary only` is fine as long as it is not
   also a `SYNC POINT`.

## 3. Findings, inline 6 (1995 F150 4.9L), log `2026-09-20_21.57.52.mlg`

Firmware `rusEFI master.2026.09.20.uaefi_pro.3342137515` (fork commit
`e5d40890bc`, sync edge Fall, tooth 6 fall coded at 720). 32 s, 935-2804 RPM.

| Check | Result |
|---|---|
| Trigger errors / sync losses while running | 0 / 0 |
| Syncs vs crank revs | +512 syncs, ~1023 revs -> one sync per cycle, none missed |
| Ratio spread p5-p95 per index | +-0.02 across a 3:1 RPM range |
| instant RPM - RPM | -4.5% .. +7.7%; per-cycle range 11.5% of RPM, same % at 857 RPM (other logs) -> geometry |
| `Sync: trigger angle error` | median -0.3, but +4.0 at index 8 and -5.0 at index 9, max +13 |
| Warning | 9007 present |

Measured fall-to-fall ratios by index (0,1 / 2,3 / ... pairs):
0.835 / 1.000 / 0.984 / 1.009 / 0.795 / 1.522.
Coded (old layout 78/138 ... 678/720): 0.870 / 1 / 1 / 1 / 0.850 / 1.353.

Backed-out intervals: 144.5 / 120.8 / 120.8 / 118.8 / 119.9 / 95.3 deg versus
coded 138 / 120 / 120 / 120 / 120 / 102. The five normal falls sit on a clean
120 deg grid; the sync tooth (tooth 6) falling edge is ~7 deg earlier than
the old model - the short tooth is ~35 deg wide, not 42, and the long gap
~145 deg, not 138.

Why it matters: spark events are scheduled from the last coded tooth before
the target angle. Events in the 0-78 deg window were scheduled off the tooth 6
fall, which really happened 7 deg earlier than the ECU believed, so the
cylinder with TDC at 62.5 (tdcPosition 662.5 + 120 k) fired ~7 deg early.
Cylinder 1 (TDC 662.5, scheduled off the correct tooth 5 fall) was right, so
a timing light on #1 alone could not show it. Instant RPM itself was harmless
in this tune (`alwaysInstantRpm = no`, `useIdleTimingPidControl = no`).

## 4. The fix and the rule that shaped it

First attempt (`7be50d41d2`): move only the tooth 6 fall 720 -> 713. **Wrong**:
`MultiChannelStateSequence::checkSwitchTimes()`
(`firmware/controllers/core/state_sequence.cpp`) requires the last event of a
trigger shape to be at exactly 720 and raises
`firmwareError(CUSTOM_ERR_WAVE_1, "last switch time has to be 1/720 ...")`.
Unit Tests and Configs CI went red; on hardware this is a critical error at
boot and the engine would not run. The firmware jobs still compiled - a green
firmware build proves nothing about shape validity, only the unit tests do.

Correct fix (`67773c417d`, `283584a79a`): rotate the **whole wheel +7 deg**
and move `tdcPosition` by the same +7 deg:

| Event | Old | New |
|---|---|---|
| T1 rise / fall | 78 / 138 | 85 / 145 |
| T2 | 198 / 258 | 205 / 265 |
| T3 | 318 / 378 | 325 / 385 |
| T4 | 438 / 498 | 445 / 505 |
| T5 | 558 / 618 | 565 / 625 |
| T6 rise / fall | 678 / 720 | 685 / 720 |
| tdcPosition | 662.5 | 669.5 |

Fall-to-fall intervals become 145/120/120/120/120/95 = measured. Sync windows
unchanged ([0.70,0.95], [1.20,1.70]); measured 0.835 sync gap and 1.52 second
gap sit well inside, and the tooth 6 candidate (0.795) is still rejected by
its second gap (~1.0). Because trigger offset = `tdcPosition +
globalTriggerAngleOffset`, cylinder 1 timing is unchanged (TDC is still 44.5
deg after the tooth 5 fall); only the previously-early cylinder moves.
Re-running the tool against the new shape gives residual interval errors of
<= 1.2 deg.

Rise edges could not be checked from fall-to-fall ratios; the per-index
instant RPM at rise indices was clean (+-40), so they were shifted with the
falls and left otherwise alone.

## 5. Procedure for the V8 (PIP8) log

1. Confirm what is on the ECU: the log header signature must be the fork's
   build and `configureFordPip8` in the checked-out tree must be what that
   build compiled (`git log -1 -- firmware/controllers/trigger/decoders/trigger_ford.cpp`).
2. `python3 tools/pip_trigger_analysis/analyze_pip.py --trigger pip8 <log>.mlg`
3. Gate on section 2 items 1 and 6 first (sync health, exactly one SYNC
   POINT). If sync is not clean, stop: geometry numbers from a wheel that is
   not syncing are meaningless.
4. Read the interval deltas. PIP8 coded intervals are 103.5 / 90 x6 / 76.5
   (ratios 1.353 / 0.870 / 1 / 1 / 1 / 1 / 1 / 0.850). Expect the same
   failure mode as the I6 if the signature tooth was modelled from a
   table rather than measured: the 76.5 short interval and 103.5 long
   interval absorbing an equal and opposite error.
5. If a stable delta > ~3 deg shows on the short/long pair: rotate every
   edge by the delta so the signature fall stays at 720, move `tdcPosition`
   by the same amount, keep the last event at exactly 720, leave the sync
   windows unless the measured ratios approach their edges. Push, and watch
   **Unit Tests**, not just the firmware jobs, before flashing.
6. Verify on the truck with a timing light on a cylinder *other than* #1.

## 6. Change history

| Date | Commit | Change |
|---|---|---|
| 2026-05-07 | `0bc9a146f0`.. | hand-written PIP6 wheel replaces generic `configureFordPip(s, 6)` |
| 2026-09-19 | `a44df96cf9`, `e5d40890bc` | refactor; sync edge -> Fall for PIP6 and PIP8 |
| 2026-09-21 | `7be50d41d2` | tooth 6 fall 720 -> 713 (**broken**: last event must be 720; do not flash) |
| 2026-09-21 | `67773c417d` | whole PIP6 wheel rotated +7 deg, tdcPosition 669.5 |
| 2026-09-21 | `283584a79a` | restore PIP8 tdcPosition 662.5 (touched by mistake in the previous commit) |
