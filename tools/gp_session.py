#!/usr/bin/env python3
"""Gameplay ground-truth sessions (spec 2026-09-30-gameplay-ground-truth-design.md
§4.1, record §G.2..§G.4): the constants, the pad table, the frame-keyed
scenario scheduler, the poll.log v2 format, the port-script v2 generator and
the trace diff. Stdlib only; tools/gp_capture.py is the DOSBox-X side.

poll.log v2, one record per line:
  B ms=<int> base=<hex8> ptr=<hex8>
  S ms=<int> f=<hex4> <SNAP_FIELDS>=<hex> ... kb=<hex4> head=<hex4> tail=<hex4>
        a consistent end-of-iteration snapshot (the master loop's spin state)
  P ms=<int> f=<hex4> mode=<hex4> st=<hex4> tick=<hex8>   read outside a spin state
  H ms=<int> f=<hex4> head=<hex4>                         the BIOS head moved
  I ms=<int> f=<hex4> step=<n> press=<name> scan=<hex2> lin=<hex8> old=<hex2>
        bios=<hex4|-> ring=<0|1|-> late=<0|1>
  I ms=<int> f=<hex4> step=<n> release=<name> lin=<hex8>
  X ms=<int> f=<hex4> step=<n> end
  E ms=<int> reason=<exit|time-limit> rc=<int>"""
import argparse
import os
import sys

DATA_BASE_VA = 0x80000
KB_PTR_DS = 0x101514        # DS_00101514 -> the key block (+0x254 key state, +0x2D8/+0x2D9 bitmap)
KEYTAB_OFF = 0x254          # IRQ1 key-state table, bit 7 = released (demo-pose §50-C.3)
ENTER_WAIT = 25.0           # s, harness: the K11 boot wait (k11_session.ENTER_WAIT)
HOLD_FRAMES = 3             # harness: AUTOTYPE's 3-tick press (record named-gaps-a §A.9)
BDA_HEAD, BDA_TAIL = 0x41A, 0x41C

SNAP_FIELDS = (
    ('f', 0x0EF6DC, 2), ('mode', 0x104B00, 2), ('st', 0x0F0A64, 2),
    ('tick', 0x101500, 4), ('t508', 0x101508, 4), ('t50c', 0x10150C, 4),
    ('raw', 0x0E1C30, 4), ('pad', 0x0E1C34, 4), ('new', 0x1088E4, 4), ('held', 0x1088D8, 4),
    ('e0', 0x1088E0, 2), ('e2', 0x1088E2, 2), ('rng', 0x0EF6D8, 4),
    ('cred', 0x105C00, 4), ('fp', 0x105D60, 1), ('b1d', 0x104B1D, 1), ('b1f', 0x104B1F, 1),
    ('b25', 0x104B25, 1), ('w10d', 0x10810D, 1), ('cnt', 0x108110, 1),
    ('s0_52', 0x107802, 1), ('s0_54', 0x107804, 1), ('s0_5a', 0x10780A, 1),
    ('s1_52', 0x107896, 1), ('s1_54', 0x107898, 1), ('s1_5a', 0x10789E, 1),
    ('ent', 0x10741C, 4),
)
TRACE_FIELDS = ('mode', 'st', 'raw', 'pad', 'e0', 'e2', 'rng', 'cred', 's0_5a', 's1_5a')

# BIOS keys: (scan, the BIOS word a press queues; record named-gaps-a §A.9).
KEYS = {'enter': (0x1C, 0x1C0D), 'esc': (0x01, 0x011B)}
# Pads (spec §3.2): (scan, BIOS word, kb bit). Letters and P2's keys use the
# config words at 0x122C62 (record §A.1.5); arrows the E0 form AUTOTYPE left
# (record §A.9); F1/F2 the standard make words. Spec §7 Q4 (ruled YES, record
# §G.2): a pad press queues its word once, as a keyboard's make code
# (gp_capture --no-pad-bios turns it off).
PAD = {
    'p1.up': (0x1F, 0x1F73, 0x8000), 'p1.down': (0x2D, 0x2D78, 0x4000),
    'p1.left': (0x2C, 0x2C7A, 0x2000), 'p1.right': (0x2E, 0x2E63, 0x1000),
    'p1.b0': (0x16, 0x1675, 0x0100), 'p1.b1': (0x17, 0x1769, 0x0200),
    'p1.b2': (0x31, 0x316E, 0x0400), 'p1.b3': (0x32, 0x326D, 0x0800),
    'p1.start': (0x3B, 0x3B00, 0x0100),
    'p2.up': (0x48, 0x48E0, 0x0080), 'p2.down': (0x50, 0x50E0, 0x0040),
    'p2.left': (0x4B, 0x4BE0, 0x0020), 'p2.right': (0x4D, 0x4DE0, 0x0010),
    'p2.b0': (0x47, 0x4700, 0x0001), 'p2.b1': (0x49, 0x4900, 0x0002),
    'p2.b2': (0x4F, 0x4F00, 0x0004), 'p2.b3': (0x51, 0x5100, 0x0008),
    'p2.start': (0x3C, 0x3C00, 0x0001),
}

_DEC = ('ms', 'rc', 'step', 'late', 'ring')
_TEXT = ('reason', 'press', 'release')


def raw_to_kb(raw):
    """DS_000E1C30 = (+0x2D8 << 24) | (+0x2D9 << 8) (0x500C4) -> the kb word."""
    return ((raw >> 24) & 0xFF) << 8 | (raw >> 8) & 0xFF


def format_s(ms, vals, kb, head, tail):
    body = ' '.join('%s=%0*X' % (n, 2 * sz, vals[n]) for n, _, sz in SNAP_FIELDS)
    return 'S ms=%d %s kb=%04X head=%04X tail=%04X' % (ms, body, kb, head, tail)


def parse(line):
    parts = line.split()
    if not parts:
        return None
    rec = {'kind': parts[0]}
    for part in parts[1:]:
        key, eq, val = part.partition('=')
        if not eq:
            rec[key] = True                 # the X record's bare `end`
        elif key in _TEXT:
            rec[key] = val
        elif val == '-':
            rec[key] = None
        elif key in _DEC:
            rec[key] = int(val)
        else:
            rec[key] = int(val, 16)
    return rec
