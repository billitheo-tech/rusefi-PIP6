#!/usr/bin/env python3
"""Minimal MLG (MegaLogViewer binary log) reader for TunerStudio / rusEFI logs.

Usage:
  mlg.py FILE            -> list fields
  mlg.py FILE f1 f2 ...  -> dump CSV of time + named fields (substring match, case-insensitive)
"""
import struct, sys

TYPES = {0: 'B', 1: 'b', 2: 'H', 3: 'h', 4: 'I', 5: 'i', 6: 'q', 7: 'f', 10: 'B'}


def read(path):
    d = open(path, 'rb').read()
    assert d[:5] == b'MLVLG', 'not an MLG file'
    fmt_ver = struct.unpack('>H', d[6:8])[0]
    info_start, data_begin = struct.unpack('>II', d[12:20])
    rec_len, nfields = struct.unpack('>HH', d[20:24])
    fhdr = (info_start - 24) // nfields
    fields = []
    off = 24
    for i in range(nfields):
        h = d[off:off + fhdr]
        t = h[0]
        name = h[1:35].split(b'\0')[0].decode('latin1')
        units = h[35:45].split(b'\0')[0].decode('latin1')
        scale, transform = struct.unpack('>ff', h[46:54])
        if fhdr == 89:  # rusEFI writer layout: byteSize,name34,unit11,scale,zero,precision,category
            units = h[35:46].split(b'\0')[0].decode('latin1')
            scale, transform = struct.unpack('>f', h[46:50])[0], 0.0
        fields.append((name, units, t, scale, transform))
        off += fhdr
    info = d[info_start:data_begin].decode('latin1', 'replace')
    # record layout: type(1) counter(1) ts(2) data(rec_len) crc(1)
    fmt = '>' + ''.join(TYPES.get(f[2], 'B') for f in fields)
    assert struct.calcsize(fmt) == rec_len, (struct.calcsize(fmt), rec_len, [f[2] for f in fields])
    rows = []
    p = data_begin
    n = len(d)
    step = 4 + rec_len + 1
    while p + step <= n:
        rtype = d[p]
        if rtype == 0:
            ts = struct.unpack('>H', d[p + 2:p + 4])[0]
            vals = struct.unpack(fmt, d[p + 4:p + 4 + rec_len])
            rows.append((ts, vals))
            p += step
        elif rtype == 1:
            # marker: type, counter, ts(2), 50-byte text? -> TS writes 2+2+... be tolerant: skip 4+50+1
            p += 4 + 50 + 1
        else:
            p += 1  # resync
    return fmt_ver, info, fields, rows


def main():
    path = sys.argv[1]
    ver, info, fields, rows = read(path)
    if len(sys.argv) == 2:
        print(f'# version={ver} fields={len(fields)} records={len(rows)}')
        print('# info:', info.strip().replace('\n', ' | ')[:300])
        for i, (n, u, t, s, tr) in enumerate(fields):
            print(f'{i:3d} {n:34s} [{u}] type={t} scale={s} tr={tr}')
        return
    wanted = [w.lower() for w in sys.argv[2:]]
    idx = []
    for w in wanted:
        m = [i for i, f in enumerate(fields) if f[0].lower() == w] or [i for i, f in enumerate(fields) if w in f[0].lower()]
        if not m:
            sys.exit(f'no field matching {w!r}')
        idx.append(m[0])
    print('t,' + ','.join(fields[i][0] for i in idx))
    t0 = None
    wrap = 0
    prev = None
    for ts, vals in rows:
        if prev is not None and ts < prev:
            wrap += 65536
        prev = ts
        t = (ts + wrap) / 100.0
        if t0 is None:
            t0 = t
        out = []
        for i in idx:
            n, u, ty, s, tr = fields[i]
            v = vals[i]
            if ty != 7:
                v = (v + tr) * s if s else v
            out.append(f'{v:.4f}' if isinstance(v, float) else str(v))
        print(f'{t - t0:.2f},' + ','.join(out))


if __name__ == '__main__':
    main()
