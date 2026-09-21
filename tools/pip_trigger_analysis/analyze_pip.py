#!/usr/bin/env python3
"""Sanity-check a Ford TFI PIP trigger decoder against a TunerStudio .mlg datalog.

Reads the coded wheel straight out of firmware/controllers/trigger/decoders/trigger_ford.cpp
(configureFordPip6 / configureFordPip8), bins the logged trigger channels by
trgtriggerStateIndex, backs the *measured* fall-to-fall intervals out of the
logged sync ratios and compares them with the coded ones.

Usage (from repo root, python3 >= 3.8, no third-party packages):

  python3 tools/pip_trigger_analysis/analyze_pip.py --trigger pip6 "path/to/log.mlg"
  python3 tools/pip_trigger_analysis/analyze_pip.py --trigger pip8 "path/to/log.mlg" [more.mlg ...]

Required log channels (all are standard rusEFI output channels):
  Time, RPM, sync: instant RPM, sync: instant RPM range, trgSync: Trigger Latest Ratio,
  trgtriggerStateIndex, trgsync: wheel sync counter, Trigger Error Counter,
  trgtriggerCountersError, sync: We have sync, Sync: trigger angle error,
  revolutionCounterSinceStart, Warning: last, Error: Trigger

Method and the 2026-09 inline-6 findings: docs/pip-trigger-datalog-analysis.md
"""
import argparse
import math
import os
import re
import statistics as st
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mlg  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
CPP = os.path.join(REPO, 'firmware', 'controllers', 'trigger', 'decoders', 'trigger_ford.cpp')
FUNC = {'pip6': 'configureFordPip6', 'pip8': 'configureFordPip8'}

CH = {
    't': 'Time', 'rpm': 'RPM', 'irpm': 'sync: instant RPM', 'irr': 'sync: instant RPM range',
    'ratio': 'trgSync: Trigger Latest Ratio', 'idx': 'trgtriggerStateIndex',
    'sync': 'trgsync: wheel sync counter', 'terr': 'Trigger Error Counter',
    'tcerr': 'trgtriggerCountersError', 'hs': 'sync: We have sync',
    'aerr': 'Sync: trigger angle error', 'rev': 'revolutionCounterSinceStart',
    'warn': 'Warning: last', 'eflag': 'Error: Trigger',
}

WARN = {9002: 'PRIMARY_TOO_MANY_TEETH', 9003: 'PRIMARY_TOO_FEW_TEETH', 9006: 'PRIMARY_DOUBLED_EDGE',
        9007: 'PRIMARY_BAD_TOOTH_TIMING_EARLY', 9008: 'PRIMARY_BAD_TOOTH_TIMING_LATE'}


def coded_shape(trigger, cpp=CPP):
    """Return (tdc, sync_edge, [(angle, 'RISE'|'FALL'), ...]) parsed from trigger_ford.cpp."""
    src = open(cpp, encoding='utf-8', errors='replace').read()
    m = re.search(r'void\s+' + FUNC[trigger] + r'\s*\([^)]*\)\s*\{(.*?)\n\}', src, re.S)
    if not m:
        sys.exit(f'{FUNC[trigger]} not found in {cpp}')
    body = m.group(1)
    tdc = float(re.search(r'tdcPosition\s*=\s*([0-9.]+)', body).group(1))
    edge = re.search(r'SyncEdge::(\w+)', body).group(1)
    ev = [(float(a), s) for a, s in re.findall(r'addEventAngle\(\s*([0-9.]+)\s*,\s*TriggerValue::(RISE|FALL)', body)]
    gaps = re.findall(r'set(Second)?TriggerSynchronizationGap2\(\s*([0-9.]+)\s*,\s*([0-9.]+)', body)
    win = {('second' if g[0] else 'primary'): (float(g[1]), float(g[2])) for g in gaps}
    return tdc, edge, ev, win


def q(xs, p):
    xs = sorted(xs)
    return xs[int(p * (len(xs) - 1))]


def load_rows(path):
    ver, info, fields, rows = mlg.read(path)
    names = [f[0] for f in fields]
    idx = {}
    for k, n in CH.items():
        if n not in names:
            sys.exit(f'{path}: log lacks channel {n!r}')
        idx[k] = names.index(n)
    out = []
    for ts, vals in rows:
        r = {}
        for k, i in idx.items():
            n, u, ty, s, tr = fields[i]
            v = vals[i]
            if ty != 7:
                v = (v + tr) * s if s else v
            r[k] = float(v)
        out.append(r)
    sig = re.search(r'rusEFI [^"|]*', info)
    return (sig.group(0).strip() if sig else '?'), out


def analyze(path, trigger):
    tdc, edge, events, win = coded_shape(trigger)
    falls = [a for a, s in events if s == 'FALL']
    n = len(falls)
    coded_int = [(falls[i] - falls[i - 1]) % 720 or 720 for i in range(n)]  # interval ENDING at falls[i]
    coded_ratio = [coded_int[i] / coded_int[i - 1] for i in range(n)]      # ratio evaluated AT falls[i]

    sig, rows = load_rows(path)
    dur = rows[-1]['t'] - rows[0]['t']
    print(f'\n===== {os.path.basename(path)}')
    print(f'firmware: {sig}   samples {len(rows)} over {dur:.1f}s ({len(rows) / max(dur, 1e-9):.0f} Hz)')
    print(f'coded {FUNC[trigger]}: sync edge {edge}, tdcPosition {tdc}, {len(events)} events, {n} falls')
    print(f'coded falls      : ' + ' '.join(f'{a:7.1f}' for a in falls))
    print(f'coded intervals  : ' + ' '.join(f'{a:7.1f}' for a in coded_int) + '   (fall-to-fall, ending at that fall)')
    print(f'coded ratios     : ' + ' '.join(f'{a:7.3f}' for a in coded_ratio))
    if win:
        print(f'coded sync windows: primary {win.get("primary")}  second {win.get("second")}')

    run = [r for r in rows if r['rpm'] > 300 and r['hs'] > 0]
    if len(run) < 50:
        print(f'  engine not running/synced in this log (RPM max {max(r["rpm"] for r in rows):.0f}, '
              f'synced samples {len(run)}) - nothing to analyze')
        return
    rpms = [r['rpm'] for r in run]
    print(f'running+synced samples: {len(run)}  RPM min/med/max {min(rpms):.0f}/{st.median(rpms):.0f}/{max(rpms):.0f}')
    print(f'Trigger Error Counter {rows[0]["terr"]:.0f} -> {rows[-1]["terr"]:.0f}   trgtriggerCountersError '
          f'{rows[0]["tcerr"]:.0f} -> {rows[-1]["tcerr"]:.0f}   Error:Trigger flag ever set: '
          f'{any(r["eflag"] > 0 for r in rows)}')
    lost = sum(1 for a, b in zip(rows, rows[1:]) if b['hs'] < a['hs'])
    dsync = rows[-1]['sync'] - rows[0]['sync']
    drev = rows[-1]['rev'] - rows[0]['rev']
    revs = sum(r['rpm'] for r in rows) / 60 * dur / len(rows)
    print(f'sync-lost events {lost}   wheel sync counter +{dsync:.0f}   revolutionCounter +{drev:.0f}   '
          f'crank revs from RPM ~{revs:.0f} (cam-speed wheel -> expect syncs = revs/2)')
    codes = sorted(set(int(r['warn']) for r in rows if r['warn'] > 0))
    print('warning codes seen: ' + ', '.join(f'{c} {WARN.get(c, "")}'.strip() for c in codes))
    ae = [r['aerr'] for r in run]
    print(f'Sync: trigger angle error  min/med/max {min(ae):+.2f}/{st.median(ae):+.2f}/{max(ae):+.2f} deg  '
          f'(>10 deg at >1000rpm raises warning 9007/9008)')
    dev = [r['irpm'] - r['rpm'] for r in run]
    print(f'instantRPM - RPM: mean {st.mean(dev):+.1f}  p5/p95 {q(dev, .05):+.0f}/{q(dev, .95):+.0f} rpm')
    irr = [r['irr'] for r in run]
    print(f'instant RPM range per cycle: median {st.median(irr):.0f} = {100 * st.median(irr) / st.median(rpms):.1f}% of RPM '
          f'(constant %% across RPM => geometry error; shrinking with RPM => real crank speed fluctuation)')

    # ---- per trigger-state-index table
    by = defaultdict(list)
    for r in run:
        by[int(r['idx'])].append(r)
    print(f'\n{"idx":>3} {"n":>5} {"ratio med":>9} {"p5":>7} {"p95":>7} | {"instRPM-RPM med":>15} {"p5":>5} {"p95":>5} | {"angErr med":>10}')
    seq = []
    for k in sorted(by):
        rs = by[k]
        rat = [r['ratio'] for r in rs]
        d = [r['irpm'] - r['rpm'] for r in rs]
        a = [r['aerr'] for r in rs]
        print(f'{k:>3} {len(rs):>5} {st.median(rat):>9.3f} {q(rat, .05):>7.3f} {q(rat, .95):>7.3f} | '
              f'{st.median(d):>+15.0f} {q(d, .05):>+5.0f} {q(d, .95):>+5.0f} | {st.median(a):>+10.2f}')
        seq.append(st.median(rat))

    # ---- collapse to one ratio per fall (ratio only updates on the sync edge; the rise that
    #      follows repeats it), then align to the coded ratio sequence by rotation
    meas = []
    for v in seq:
        if not meas or abs(v - meas[-1]) > 0.004:
            meas.append(v)
    if len(by) != len(events):
        print(f'\n!! log has {len(by)} trigger state indices but {FUNC[trigger]} has {len(events)} events - '
              f'this log was not recorded with this wheel (or --trigger is wrong). Skipping geometry back-out.')
        return
    if len(meas) != n:
        # fall back: take every other index starting at 0
        meas = seq[0::2][:n]
    best = None
    for rot in range(n):
        cand = meas[rot:] + meas[:rot]
        err = sum((math.log(c / e)) ** 2 for c, e in zip(cand, coded_ratio))
        if best is None or err < best[0]:
            best = (err, rot, cand)
    _, rot, meas_al = best
    # back out intervals: d[i] = d[i-1] * ratio[i]; normalize to 720
    d = [1.0]
    for i in range(1, n):
        d.append(d[-1] * meas_al[i])
    # close the loop consistency check
    closure = d[0] / d[-1] - meas_al[0]
    scale = 720 / sum(d)
    meas_int = [x * scale for x in d]
    meas_falls = []
    acc = falls[0] - coded_int[0]  # anchor: start of the interval ending at coded falls[0]
    for i in range(n):
        acc += meas_int[i]
        meas_falls.append(acc % 720 or 720)

    print(f'\nmeasured ratios (aligned, rotation {rot}): ' + ' '.join(f'{a:7.3f}' for a in meas_al) +
          f'   loop closure error {closure:+.3f}')
    print(f'measured intervals                    : ' + ' '.join(f'{a:7.1f}' for a in meas_int))
    print(f'coded    intervals                    : ' + ' '.join(f'{a:7.1f}' for a in coded_int))
    print(f'interval delta (measured - coded)     : ' + ' '.join(f'{m - c:+7.1f}' for m, c in zip(meas_int, coded_int)))
    print(f'implied fall angles (anchored at fall0 interval start): ' + ' '.join(f'{a:7.1f}' for a in meas_falls))
    print(f'coded fall angles                                     : ' + ' '.join(f'{a:7.1f}' for a in falls))
    worst = max(range(n), key=lambda i: abs(meas_int[i] - coded_int[i]))
    print(f'\nlargest interval error: {meas_int[worst] - coded_int[worst]:+.1f} deg on the interval ending at coded fall {falls[worst]:.1f}')
    print('rule of thumb: |delta| <= 2 deg is noise/dynamics; a stable 5+ deg delta that does not scale with RPM is wheel geometry.')
    print('if you change the shape: the LAST event must stay at exactly 720 (checkSwitchTimes) - rotate the whole wheel and move')
    print('tdcPosition by the same amount instead of moving a single edge (see docs/pip-trigger-datalog-analysis.md).')
    for name, (lo, hi) in win.items():
        pass
    if win:
        prim = win.get('primary'); sec = win.get('second')
        print('\nsync window check against measured ratios:')
        for i in range(n):
            cur = meas_al[i]; prev = meas_al[i - 1]
            in_p = prim and prim[0] <= cur <= prim[1]
            in_s = sec and sec[0] <= prev <= sec[1]
            tag = 'SYNC POINT' if (in_p and in_s) else ('primary only' if in_p else '')
            print(f'  fall {falls[i]:6.1f}: ratio {cur:.3f} prev {prev:.3f}  {tag}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--trigger', choices=list(FUNC), required=True)
    ap.add_argument('logs', nargs='+')
    a = ap.parse_args()
    for p in a.logs:
        analyze(p, a.trigger)


if __name__ == '__main__':
    main()
