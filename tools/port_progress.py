#!/usr/bin/env python3
"""Ported-function progress: percentage for README.md, and the unported list.

A function counts as ported when its address is in port/src/symbols.h (FN_)
and appears as a column-0 `/* 0xADDR` comment header, in an `fn_register(0xADDR`
call, or as a declared `PORT_PROGRESS_EXCEPTIONS` entry. The strict rule is the
same one `tools/diff_verify.py`'s `NonPortable.ported` scan and
`tools/entry_triage.py`'s `load_ported` use: the loose "anywhere in a comment"
form reads prose mentions as implementations, so it is not used here either.

  tools/port_progress.py              # prints: ported total percent, the
                                      # adjusted "ported portable percent", and
                                      # the class breakdown
  tools/port_progress.py --unported   # + one line per unported function:
                                      #   addr size callers callees

tools/port_classification.txt lists functions that are host-owned, deferred or
replaced (evidence per line); they leave the portable denominator and the
unported list. The `replaced` class is the clean-room runtime replacement layer
(the boot/movie/attract spine's runtime closure, record-§C4), all of it at or
above 0x5D000 so the portable denominator is unaffected. Runtime-library code
(>= 0x5D000: WATCOM libc, DOS/4GW glue, the AIL library) is the remainder; the
port uses the host libc / clean-room decoder for it, so it is not a porting
target.
"""
import csv
import glob
import re
import sys

ROOT = __file__.rsplit('/tools/', 1)[0] if '/tools/' in __file__ else '.'
RUNTIME_BASE = 0x5D000

# Addresses the strict registry scan cannot see but that the port does implement
# or deliberately replaces with an inline no-op, credited here by name so they
# are not left as an undeclared inline credit. Each is an FN_ address whose only
# port/src mention is a trailing `/* 0xADDR` comment (not a column-0 header, not
# an fn_register). The three header-shape entries are the same three the diff
# harness names `UNROWED_HEADER_SHAPE` (implemented but named mid-comment).
PORT_PROGRESS_EXCEPTIONS = {
    0x1B120: "header-shape: res_load_index names 0x1B120 mid-comment (res.h:1); "
              "diff_verify.py UNROWED_HEADER_SHAPE, record-§P10",
    0x20C10: "header-shape: game_init names 0x20C10 mid-comment (flow.c); "
              "diff_verify.py UNROWED_HEADER_SHAPE, record-§P10",
    0x4FBA2: "header-shape: int10h_query is a BIOS `int 10h` mode query the SDL host "
              "owns (flow.c:117); diff_verify.py UNROWED_HEADER_SHAPE, record-§P10",
    0x29D60: "ret-only no-op (bare `ret`); the port skips it via fn_resolve "
              "(record-§C1.4.3)",
    0x2EA30: "the interrupt-lock nesting counter, inert in the single-threaded port "
              "(actors.c:1124; record-§C1.4.3)",
    0x2EA64: "bare `ret`; the port calls it as a no-op (actors.c:2369, menu.c; "
              "record-§C1.4.3)",
    0x500BB: "inlined DPMI clock read (`mov eax,[0x101500]; ret`), in string_unlock "
              "and config.c (flow.c:256, config.c:860; record-§C1.4.3)",
    0x61A70: "runtime WATCOM memset wrapper, implemented as host_memset/mem_fill "
              "(actors.c:4332, mem.c:131; a column-0 header would re-open the 26 "
              "runtime allow-mode rows, diff_verify.py:35269-35272; record-§C1.4.3)",
}


def fn_addresses():
    """The symbols.h `FN_` function addresses (the same parse diff_verify.py uses)."""
    with open(f'{ROOT}/port/src/symbols.h') as f:
        text = f.read()
    return frozenset(int(m, 16) for m in re.findall(r'^#define FN_([0-9A-F]{8}) ', text, re.M))


def port_tree_text():
    """The port/src .c/.h text (symbols.h excluded), for the credit scans."""
    parts = []
    for ext in ('c', 'h'):
        for f in glob.glob(f'{ROOT}/port/src/**/*.{ext}', recursive=True):
            if f.endswith('symbols.h'):
                continue
            with open(f, errors='ignore') as fh:
                parts.append(fh.read())
    return ''.join(parts)


def ported_set(text=None):
    """The strict ported registry: column-0 header, fn_register, or a declared
    exception. `text` is `port_tree_text()` (passed in for the test's temp copy)."""
    text = port_tree_text() if text is None else text
    ported = {int(m, 16) for m in re.findall(r'^/\* 0x([0-9A-Fa-f]{4,6})\b', text, re.M)}
    ported |= {int(m, 16) for m in re.findall(r'fn_register\(\s*0x([0-9A-Fa-f]+)', text)}
    ported |= set(PORT_PROGRESS_EXCEPTIONS)
    return frozenset(ported & fn_addresses())


def inline_credits(text=None):
    """FN_ addresses credited only by a trailing/mid-comment `/* 0xADDR` (no
    column-0 header, no fn_register). Every one must be a declared exception."""
    text = port_tree_text() if text is None else text
    fns = fn_addresses()
    loose = {int(m, 16) for m in re.findall(r'/\* 0x([0-9A-Fa-f]{4,6})\b', text)} & fns
    col0 = {int(m, 16) for m in re.findall(r'^/\* 0x([0-9A-Fa-f]{4,6})\b', text, re.M)} & fns
    reg = {int(m, 16) for m in re.findall(r'fn_register\(\s*0x([0-9A-Fa-f]+)', text)} & fns
    return frozenset(loose - col0 - reg)


def classification():
    """tools/port_classification.txt as {addr: class}; host-owned/deferred/replaced."""
    cls = {}
    with open(f'{ROOT}/tools/port_classification.txt') as f:
        for ln in f:
            if ln.strip() and not ln.startswith('#'):
                cls[int(ln.split()[0], 16)] = ln.split()[1]
    return cls


def class_map(ported=None, cls=None):
    """One class per FN_ address: ported / host-owned / deferred / replaced / runtime."""
    ported = ported_set() if ported is None else ported
    cls = classification() if cls is None else cls
    out = {}
    for a in fn_addresses():
        if a in cls:
            out[a] = cls[a]
        elif a in ported:
            out[a] = 'ported'
        elif a >= RUNTIME_BASE:
            out[a] = 'runtime'
        else:
            out[a] = 'unclassified'
    return out


def counts(ported=None, cls=None):
    """(raw_ported, n_fns, portable_done, portable_total, class_breakdown)."""
    ported = ported_set() if ported is None else ported
    cls = classification() if cls is None else cls
    n_fns = len(fn_addresses())
    classes = class_map(ported, cls)
    breakdown = {}
    for a, c in classes.items():
        breakdown[c] = breakdown.get(c, 0) + 1
    portable_total = sum(1 for a in fn_addresses() if a < RUNTIME_BASE and a not in cls)
    portable_done = sum(1 for a in fn_addresses()
                        if a < RUNTIME_BASE and a not in cls and a in ported)
    return len(ported), n_fns, portable_done, portable_total, breakdown


def main():
    ported = ported_set()
    cls = classification()
    raw, n_fns, p_done, p_total, breakdown = counts(ported, cls)
    fns = fn_addresses()
    ported_portable = sum(1 for a in fns if a < RUNTIME_BASE and a in ported)
    print(raw, n_fns, round(100 * raw / n_fns))
    print(p_done, p_total, round(100 * p_done / p_total))
    print('ported %d (%d portable + %d runtime)  host-owned %d  deferred %d  '
          'replaced %d  runtime %d' % (
              breakdown.get('ported', 0), ported_portable,
              breakdown.get('ported', 0) - ported_portable,
              breakdown.get('host-owned', 0), breakdown.get('deferred', 0),
              breakdown.get('replaced', 0), breakdown.get('runtime', 0)))
    if '--unported' not in sys.argv:
        return
    rows = {}
    for r in csv.DictReader(open(f'{ROOT}/port/decomp/prage.functions.csv')):
        if '::' not in r['entry']:
            rows[int(r['entry'], 16)] = r
    for a in sorted(fn_addresses() - ported - set(cls),
                    key=lambda a: (a >= RUNTIME_BASE, -int(rows[a]['size']) if a in rows else 0)):
        r = rows.get(a)
        if r:
            print('%05X %5s %3s %3s%s' % (a, r['size'], r['n_callers'], r['n_callees'],
                                          '  runtime' if a >= RUNTIME_BASE else ''))


if __name__ == '__main__':
    main()
