#!/usr/bin/env python3
"""Gameplay ground-truth capture (spec 2026-09-30-gameplay-ground-truth-design.md
§4.1, record §G.5). The pinned original runs in DOSBox-X under DX-CAPTURE /V /O.
A poller thread maps the [dosbox] memory file, takes one consistent snapshot
per frame while the master loop spins ([DS_0010150C] - 1 == [DS_00101508],
0x256C6), fires the scenario's frame-keyed injections from that state and
writes poll.log (format: gp_session). Writes only data/k11-captures/gp-*/.
Usage: gp_capture.py --scenario NAME --out data/k11-captures/NAME [--exe PATH]
                     [--time-limit S] [--keep-avi] [--no-pad-bios]"""
import argparse
import gzip
import hashlib
import mmap
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs
import k11_capture as kc
import smk_capture as sc
import title_capture as tcap


def read_snap(mm, base):
    out = {}
    for name, ds, size in gs.SNAP_FIELDS:
        o = base + ds - gs.DATA_BASE_VA
        out[name] = int.from_bytes(mm[o:o + size], 'little')
    return out


def spinning(v):
    return (v['t50c'] - 1) & 0xFFFFFFFF == v['t508']


def consistent(v1, v2):
    return v1['f'] == v2['f'] and v1['t508'] == v2['t508']


def ring_steps(old, new, start, end):
    """The BIOS head after each word consumed from `old` to `new` (the key
    loop 0x24D08..0x24EE7 drains the ring in one iteration, record §G.4)."""
    out, h = [], old
    while h != new and len(out) < 16:
        h += 2
        if h >= end:
            h = start
        out.append(h)
    return out


class Injector:
    """Writes key presses into the IRQ1 key-state table (bit 7 clear = down)
    and, for a key or (pad_bios) a pad, queues its BIOS word once."""

    def __init__(self, mm, ptr, log, pad_bios=True):
        self.mm, self.ptr, self.log, self.pad_bios = mm, ptr, log, pad_bios
        self.held = []            # [(release_at_f, lin, old, name, step)]

    def press(self, step, f, name, scan, word, hold, late, ms):
        lin = self.ptr + gs.KEYTAB_OFF + scan
        old = self.mm[lin]
        self.mm[lin] = old & 0x7F
        self.held.append((f + hold - 1, lin, old, name, step))
        bios = word if (name in gs.KEYS or self.pad_bios) else None
        ring = kc.bios_insert(self.mm, word) if bios is not None else None
        self.log.write('I ms=%d f=%04X step=%d press=%s scan=%02X lin=%08X old=%02X bios=%s ring=%s late=%d\n'
                       % (ms, f, step, name, scan, lin, old, '-' if bios is None else '%04X' % bios,
                          '-' if ring is None else int(ring), late))

    def release_due(self, f, ms):
        for h in [h for h in self.held if f >= h[0]]:
            self.mm[h[1]] = h[2] | 0x80
            self.held.remove(h)
            self.log.write('I ms=%d f=%04X step=%d release=%s lin=%08X\n' % (ms, f, h[4], h[3], h[1]))
