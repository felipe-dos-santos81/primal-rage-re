#!/usr/bin/env python3
"""Ported-function progress: percentage for README.md, and the unported list.

A function counts as ported when its address is in port/src/symbols.h (FN_)
and appears as a `/* 0xADDR` comment header (anywhere on a line) or in an
`fn_register(0xADDR` call in port/src (.c and .h, not symbols.h). Data and mid-function addresses in
such headers do not count, so they cannot inflate the figure.

  tools/port_progress.py              # prints: ported total percent, then the
                                      # adjusted "ported portable percent"
  tools/port_progress.py --unported   # + one line per unported function:
                                      #   addr size callers callees
tools/port_classification.txt lists functions that are host-owned or deferred
(evidence per line); they leave the portable denominator and the unported list.
Runtime-library code (>= 0x5D000: WATCOM libc, DOS/4GW glue) is listed last;
the port uses the host libc for it, so it is not a porting target.
"""
import csv
import glob
import re
import sys

ROOT = __file__.rsplit('/tools/', 1)[0] if '/tools/' in __file__ else '.'
RUNTIME_BASE = 0x5D000


def main():
    fns = {int(m, 16) for m in re.findall(
        r'^#define FN_([0-9A-F]{8}) ', open(f'{ROOT}/port/src/symbols.h').read(), re.M)}
    txt = ''.join(open(f, errors='ignore').read()
                  for ext in ('c', 'h')
                  for f in glob.glob(f'{ROOT}/port/src/**/*.{ext}', recursive=True)
                  if not f.endswith('symbols.h'))
    ported = {int(m, 16) for m in re.findall(r'/\* 0x([0-9A-Fa-f]{4,6})\b', txt)}
    ported |= {int(m, 16) for m in re.findall(r'fn_register\(\s*0x([0-9A-Fa-f]+)', txt)}
    ported &= fns
    print(len(ported), len(fns), round(100 * len(ported) / len(fns)))
    skip = {}
    for ln in open(f'{ROOT}/tools/port_classification.txt'):
        if ln.strip() and not ln.startswith('#'):
            skip[int(ln.split()[0], 16)] = ln.split()[1]
    skip = {a: c for a, c in skip.items() if a in fns and a not in ported}
    portable = {a for a in fns if a < RUNTIME_BASE and a not in skip}
    done = len(ported & portable)
    print(done, len(portable), round(100 * done / len(portable)),
          '(portable: excludes %d host-owned/deferred and runtime >= %X)' % (len(skip), RUNTIME_BASE))
    if '--unported' not in sys.argv:
        return
    rows = {}
    for r in csv.DictReader(open(f'{ROOT}/port/decomp/prage.functions.csv')):
        if '::' not in r['entry']:
            rows[int(r['entry'], 16)] = r
    for a in sorted(fns - ported - set(skip), key=lambda a: (a >= RUNTIME_BASE, -int(rows[a]['size']) if a in rows else 0)):
        r = rows.get(a)
        if r:
            print('%05X %5s %3s %3s%s' % (a, r['size'], r['n_callers'], r['n_callees'],
                                        '  runtime' if a >= RUNTIME_BASE else ''))


main()
