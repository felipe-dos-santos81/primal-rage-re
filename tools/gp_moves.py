#!/usr/bin/env python3
"""Gameplay moves (plan 2026-10-01-gameplay-u6b-moves-capture, record
2026-10-01-gameplay-u6-derivations §U6.10-§U6.12): decode a character's keyboard
command table from the fixed-up image, turn a move's phases into frame-keyed
pad presses, and check from a poll.log which attempts the original performed.

The tables (raw, record §U6.10): 0x3C600 reads the descriptor of (side, i) from
0xC6B9C when the side's slot +0x63 is 0 and its device word
[DS_00101514]+0x2D4/+0x2D6 is 0, 4 or 6 (the jump table 0x3C5E4: a keyboard
player), and from 0xC619C when +0x63 != 0 (the CPU). Both are 7 characters x
0x20 entries x 8 bytes {u32 desc; u16 reaction; u8 d; u8 e}; the reaction
(0x3CEAD/0x3CEEC) and the stance bytes (0x3CD10/0x3CD29) are read from
0xC619C for both. A descriptor is up to 8 phases of 0x14 bytes: m0 (+0, the
bits to collect), m1 (+4), m2 (+8, reset) and the countdown word +0xC; 0x3C88C
advances a phase once the command word 0x1088E0 has covered m0 and arms the
entry when the next phase's m0 is 0. The command word (0x4F644): low byte = the
P1 pad bits newly pressed this frame, high byte = the level (a new bit is in
both). Bits 0x10000/0x20000/0x40000/0x80000 are facing-relative (0x3C6E8).

Usage:
  gp_moves.py list  --image IMG --char C
  gp_moves.py steps --image IMG --char C --facing F --moves I,I,... --gap G --step S
  gp_moves.py dry   --image IMG --char C --facing F --moves I,I,... --gap G --step S
                    --capture DIR --start F0 --end E --out PATH
  gp_moves.py check --image IMG --char C --facing F --moves I,I,... --gap G --step S
                    --capture DIR"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

IMAGE_BASE = 0x10000        # build/diffrun --image-out dumps mem[] from 0x10000 (record E1 §E.1)
KB_TABLE = 0x0C6B9C         # 0x3C66E: the keyboard player's descriptors (sel 0, 4, 6)
CPU_TABLE = 0x0C619C        # 0x3C699: the CPU's descriptors; +4 reaction, +6 d, +7 e for both
MOVE_TABLE = 0x0A3528       # 0x3AFC4: 20-byte rows (char * 64 + reaction), the callback dword at +0
ENTRIES, ENTRY = 0x20, 8
PHASE, MAX_PHASES = 0x14, 8
NAMES = ('p1.b0', 'p1.b1', 'p1.b2', 'p1.b3', 'p1.right', 'p1.left', 'p1.down', 'p1.up')
# 0x500C4 keeps a bit that changed this iteration at its old level (spec §3.1,
# record gameplay-ground-truth §G.7), so a release shorter than 2 frames never
# reaches the level 0x4F644 reads: a re-press is released 2 frames first.
REPRESS = 2
BLOCK = 'block'             # a move-list item: hold back for one gap (0x1AB5C's block test)
# 0x3CE58: an entry whose reaction is 0x10 or 0x11 applies 0x3CBC4's or 0x3CC58's variant.
VARIANTS = {0x10: (0x10, 0x12, 0x14, 0x16), 0x11: (0x11, 0x13, 0x15, 0x17)}


class Image:
    def __init__(self, data, base=IMAGE_BASE):
        self.data, self.base = data, base

    @classmethod
    def load(cls, path):
        with open(path, 'rb') as f:
            return cls(f.read())

    def d(self, a):
        return struct.unpack_from('<I', self.data, a - self.base)[0]

    def w(self, a):
        return struct.unpack_from('<H', self.data, a - self.base)[0]

    def b(self, a):
        return self.data[a - self.base]


def decode(img, char):
    """{i: (reaction, d, e, callback, [(m0, m1, m2, count), ...])} for the
    keyboard table's non-empty entries of `char`."""
    out = {}
    for i in range(ENTRIES):
        e = (char * ENTRIES + i) * ENTRY
        desc = img.d(KB_TABLE + e)
        if not desc:
            continue
        reaction = img.w(CPU_TABLE + e + 4)
        phases = []
        for k in range(MAX_PHASES):
            a = desc + k * PHASE
            ph = (img.d(a), img.d(a + 4), img.d(a + 8), img.w(a + 0xC))
            phases.append(ph)
            if ph[0] == 0:
                break
        cb = img.d(MOVE_TABLE + (char * 64 + reaction) * 20) if reaction < 64 else 0
        out[i] = (reaction, img.b(CPU_TABLE + e + 6), img.b(CPU_TABLE + e + 7), cb, phases)
    return out


def absolute(mask, facing):
    """0x3C6E8: the facing-relative bits folded into the command word."""
    a, b, c, d = (0x20, 0x2000, 0x10, 0x1000) if facing else (0x10, 0x1000, 0x20, 0x2000)
    out = mask & 0xFFFF
    for bit, v in ((0x10000, a), (0x40000, c), (0x20000, b), (0x80000, d)):
        if mask & bit:
            out |= v
    return out


def tokens(word):
    """A command word -> (held names, new names)."""
    return ([NAMES[k] for k in range(8) if word & (0x100 << k)],
            [NAMES[k] for k in range(8) if word & (1 << k)])


def presses(phases, facing, step):
    """A move's phases -> sorted [(offset, name, hold)]. The buttons the last
    phase asks for as held are pressed at offset 0 and held to the end: with a
    b0/b1 and a b2/b3 bit held, 0x351DB..0x35201 skips the up-jump 0x3BDDC, so
    a motion through up stays a motion. The phases with m0 != 0 then run `step`
    frames apart (phase 1 is skipped when it repeats phase 0: 0x3C88C leaves it
    on the first frame m0 is no longer whole). At each phase every other name
    the phase does not ask for is released, a name it asks for as new is
    (re)pressed there (released REPRESS frames first when it was held), and a name
    it asks for as held is held from there; all are released one step after
    the last phase."""
    seq = [p[0] for p in phases if p[0]]
    if len(seq) > 1 and seq[1] == seq[0]:
        seq = seq[:1] + seq[2:]
    keep = set(n for n in tokens(absolute(seq[-1], facing))[0] if n in NAMES[:4]) if seq else set()
    since, ev, t = {}, [], 0
    for m0 in seq:
        held, new = tokens(absolute(m0, facing))
        need = set(held) | set(new) | keep
        for n in sorted(since):
            if n not in need or n in new:
                end = t - REPRESS if n in new else t
                if end <= since[n]:
                    raise ValueError('%s re-pressed with no gap at offset %d' % (n, t))
                ev.append((since[n], n, end - since[n]))
                del since[n]
        for n in NAMES:
            if n in need and n not in since:
                since[n] = t
        t += step
    for n, s0 in since.items():
        ev.append((s0, n, t - s0))
    return sorted(ev)


def back(facing):
    """The held-back name: 0x80000 folded for this facing."""
    return tokens(absolute(0x80000, facing))[0][0]


def plan(table, moves, facing, gap, step):
    """[(attempt offset, item, [(offset, name, hold)])]: one attempt per item,
    `gap` frames apart (a harness value); an item is a table index or BLOCK
    (hold back for gap - step frames)."""
    out = []
    for k, item in enumerate(moves):
        t = k * gap
        if item == BLOCK:
            ev = [(t, back(facing), gap - step)]
        else:
            ev = [(t + o, n, h) for o, n, h in presses(table[item][4], facing, step)]
        out.append((t, item, ev))
    return out


def scenario_steps(attempts):
    """The attempts -> (first offset, [('after', delta, ('pad', names, hold))]):
    the presses grouped by (frame, hold); the first step's delta is 0 and the
    caller anchors it (an 'after_mode' step)."""
    groups = {}
    for _, _, ev in attempts:
        for f, n, h in ev:
            groups.setdefault((f, h), []).append(n)
    keys = sorted(groups)
    steps, prev = [], keys[0][0]
    for f, h in keys:
        steps.append(('after', f - prev, ('pad', tuple(sorted(groups[(f, h)])), h)))
        prev = f
    return keys[0][0], steps


def kb_levels(events, base):
    """[(frame, name, hold)] -> [(F, kb)]: the kb level iteration F samples (a
    press made for F, gp_session.Schedule), at every frame the level changes."""
    frames = sorted({f for f, _, _ in events} | {f + h for f, _, h in events})
    out = []
    for f in frames:
        kb = 0
        for s, n, h in events:
            if s <= f < s + h:
                kb |= gs.PAD[n][2]
        out.append((base + f, kb))
    return out


def merge_script(text, bits):
    """A port script v2 (gp_session.port_script) plus `bits F KB` lines, in the
    generator's order (by frame; keys before bits at one frame)."""
    lines = text.splitlines()
    head = [l for l in lines if l.startswith(('#', 'enter_'))]
    end = [l for l in lines if l.startswith('end ')]
    last = int(end[0].split()[1])
    ev = []
    for k, l in enumerate(lines):
        p = l.split()
        if p and p[0] in ('key', 'bits'):
            ev.append((int(p[1]), 0 if p[0] == 'key' else 1, k, l))
    for k, (f, kb) in enumerate(bits):
        if f > last:
            raise ValueError('bits at %d past the script end %d' % (f, last))
        ev.append((f, 1, len(lines) + k, 'bits %d %04X' % (f, kb)))
    return '\n'.join(head + [l for _, _, _, l in sorted(ev)] + end) + '\n'


def expected(table, item):
    """The r0 values an attempt counts as performed (BLOCK: None, see check)."""
    if item == BLOCK:
        return None
    r = table[item][0]
    return VARIANTS.get(r, (r,))


def check(snaps, f0, attempts, table, gap):
    """Per attempt: (offset, item, verdict, f). A move is performed when, in
    [F, F + gap), some S record shows r0 in its expected set and the record
    before it does not (0xFF, the reset value 0x4952A, is in no table reaction:
    the 7 characters' keyboard reactions are all < 0x40); a block when s0_43 &
    0x30 (0x1A6AC's 0x20/0x10) becomes set (the record before it has neither).
    F = f0 + offset; 'no-snapshot' when no S record lies in the window."""
    out = []
    for t, item, _ in attempts:
        F = f0 + t
        fs = [f for f in range(F, F + gap) if f in snaps]
        hit = None
        for f in fs:
            s = snaps[f]
            if item == BLOCK:
                # 0x1A6AC sets +0x43 bit 0x20/0x10 and leaves it set: a block is
                # the bits becoming set, not a block already held at F.
                prev = snaps.get(f - 1)
                if s['s0_43'] & 0x30 and prev is not None and not prev['s0_43'] & 0x30:
                    hit = f
                    break
            else:
                want = expected(table, item)
                prev = snaps.get(f - 1)
                if s['r0'] in want and prev is not None and prev['r0'] not in want:
                    hit = f
                    break
        verdict = 'no-snapshot' if not fs else ('performed' if hit is not None else 'not shown')
        out.append((t, item, verdict, hit))
    return out


def first_press(lines):
    """F of the scenario's first pad press: its I record's f is the spin
    snapshot of F - 1 (gp_session.Schedule)."""
    for r in (gs.parse(l) for l in lines):
        if r and r['kind'] == 'I' and str(r.get('press', '')).startswith('p1.'):
            return r['f'] + 1
    return None


def _moves(s):
    return [BLOCK if m == BLOCK else int(m, 16) for m in s.split(',')]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('list', 'steps', 'dry', 'check'))
    ap.add_argument('--image', required=True)
    ap.add_argument('--char', type=int, required=True)
    ap.add_argument('--facing', type=int, default=0)
    ap.add_argument('--moves')
    ap.add_argument('--gap', type=int)
    ap.add_argument('--step', type=int)
    ap.add_argument('--capture')
    ap.add_argument('--start', type=lambda s: int(s, 0))
    ap.add_argument('--end', type=lambda s: int(s, 0))
    ap.add_argument('--out')
    a = ap.parse_args()
    table = decode(Image.load(a.image), a.char)
    if a.cmd == 'list':
        for i, (r, d, e, cb, ph) in sorted(table.items()):
            print('i=%02X reaction=%02X d=%d e=%d callback=%05X phases=%s'
                  % (i, r, d, e, cb, ' '.join('%05X/%05X/%05X/%d' % p for p in ph)))
        return 0
    attempts = plan(table, _moves(a.moves), a.facing, a.gap, a.step)
    if a.cmd == 'steps':
        first, steps = scenario_steps(attempts)
        print('first offset %d' % first)
        for st in steps:
            print('        %r,' % (st,))
        return 0
    if a.cmd == 'dry':
        with open(os.path.join(a.capture, 'poll.log')) as f:
            text = gs.port_script('gp-u6-moves-dry', f.read().splitlines(), end=a.end)
        ev = [e for _, _, evs in attempts for e in evs]
        with open(a.out, 'w') as f:
            f.write(merge_script(text, kb_levels(ev, a.start)))
        print('gp_moves: dry: wrote %s (%d attempts from f=%d)' % (a.out, len(attempts), a.start))
        return 0
    with open(os.path.join(a.capture, 'poll.log')) as f:
        lines = f.read().splitlines()
    F1 = first_press(lines)
    if F1 is None:
        print('gp_moves: check: no p1 press in %s' % a.capture)
        return 1
    f0 = F1 - scenario_steps(attempts)[0]
    snaps = gs.snapshots(lines)
    if not snaps or any(k not in r for r in snaps.values() for k in ('r0', 's0_43')):
        print('gp_moves: check: the capture records no move fields (r0, s0_43): nothing checked')
        return 1
    n = 0
    for t, item, verdict, hit in check(snaps, f0, attempts, table, a.gap):
        n += verdict == 'performed'
        print('gp_moves: check: attempt at f=%X %s: %s%s'
              % (f0 + t, 'block' if item == BLOCK else 'i=%02X' % item, verdict,
                 '' if hit is None else ' (f=%X)' % hit))
    print('gp_moves: check: %d of %d attempts performed' % (n, len(attempts)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
