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


# Scenarios (spec §4.1). Harness values: the 150-frame gaps follow the
# planner's probe's 2.5 s (spec §3.5); time limits cover the probe's timings.
SCENARIOS = {
    # U1 Task 8: every pad name, one frame each, 30 apart, on the MAIN MENU
    # (mode 0x27: 0x500C4 runs every frame, spec §3.1). One frame: the menu
    # acts on the pad level DS_000E1C34, which a one-frame press never reaches
    # (0x500C4 keeps a changed bit's old level), so the MAIN MENU stays put;
    # the chord uses only bits outside the menu's mask 0xC300C000 (record §G.7:
    # run 1's chord held p1.b1 into the level and 0x2FFC4 returned -1, the
    # 0x2520B restart).
    'gp-pads': dict(time_limit=75, steps=(
        ('boot', ENTER_WAIT, ('key', 'enter')),
        ('after_mode', 0x27, 120, ('pad', ('p1.up',), 1)),
    ) + tuple(('after', 30, ('pad', (n,), 1)) for n in (
        'p1.down', 'p1.left', 'p1.right', 'p1.b0', 'p1.b1', 'p1.b2', 'p1.b3', 'p1.start',
        'p2.up', 'p2.down', 'p2.left', 'p2.right', 'p2.b0', 'p2.b1', 'p2.b2', 'p2.b3', 'p2.start'))
      + (('after', 30, ('pad', ('p1.left', 'p1.b2', 'p2.right'), 5)),     # a chord held 5
         ('after', 60, ('end',))),
    ),
}


def expand(action):
    """An action -> [(name, scan, bios_word, hold_frames)]."""
    if action[0] == 'key':
        scan, word = KEYS[action[1]]
        return [(action[1], scan, word, HOLD_FRAMES)]
    if action[0] == 'pad':
        return [(n, PAD[n][0], PAD[n][1], action[2]) for n in action[1]]
    return []


class Schedule:
    """Steps: ('boot', s, act) | ('after_mode', mode, n, act) | ('after', n, act)
    | ('until_mode', mode, n); an ('end',) action ends the scenario at its frame.
    An action for frame F fires at the first spin snapshot with f >= F - 1, so
    iteration F samples it (spec §3.1)."""

    def __init__(self, steps):
        self.steps = list(steps)
        self.i = 0
        self.prev_frame = None
        self.frame_of = {}          # step -> its frame F (set when it fires)
        self.mode_first = {}
        self.end_frame = None
        self.fired = 0
        self.total = sum(1 for st in self.steps if st[0] != 'until_mode' and st[-1] != ('end',))

    def _target(self, st):
        if st[0] == 'after_mode':
            f0 = self.mode_first.get(st[1])
            return None if f0 is None else f0 + st[2]
        if st[0] == 'after':
            return None if self.prev_frame is None else self.prev_frame + st[1]
        return None

    def on_mode(self, f, mode):
        if self.i < len(self.steps):
            st = self.steps[self.i]
            if st[0] == 'until_mode' and mode == st[1] and self.end_frame is None:
                self.end_frame = f + st[2]
                self.i += 1
                return
        if self.i > 0 or self.steps[0][0] != 'boot':
            self.mode_first.setdefault(mode, f)

    def due_boot(self, now_s):
        if self.i < len(self.steps) and self.steps[self.i][0] == 'boot' and now_s >= self.steps[self.i][1]:
            self.i += 1
            self.fired += 1
            return [(self.i - 1, self.steps[self.i - 1][2])]
        return []

    def due(self, f):
        out = []
        while self.i < len(self.steps):
            st = self.steps[self.i]
            if st[0] in ('boot', 'until_mode'):
                break
            F = self._target(st)
            if F is None or f < F - 1:
                break
            self.prev_frame = F
            self.frame_of[self.i] = F
            self.i += 1
            if st[-1] == ('end',):
                self.end_frame = F
                break
            self.fired += 1
            out.append((self.i - 1, st[-1]))
        return out

    def ended(self, f):
        return self.end_frame is not None and f >= self.end_frame


class ScriptError(Exception):
    pass


def snapshots(lines):
    out = {}
    for l in lines:
        r = parse(l)
        if r and r['kind'] == 'S':
            out.setdefault(r['f'], r)
    return out


def port_script(name, lines):
    """poll.log v2 -> port script v2 (spec §4.1). Keys at their consumption
    frame (the H record paired FIFO with the I press, pinned by S(f-1) showing
    the old head); bits at each change of S.raw, pinned by S(f-1). The key loop
    0x24D08..0x24EE7 drains every queued word in one iteration, so the poller
    writes one H record per consumed word and S(f) must show the head of the
    last H record of frame f (record §G.4)."""
    recs = [r for r in (parse(l) for l in lines) if r]
    snap = snapshots(lines)
    presses = [r for r in recs if r['kind'] == 'I' and 'press' in r and r.get('bios') is not None]
    heads = [r for r in recs if r['kind'] == 'H']
    if len(heads) < len(presses):
        raise ScriptError('%d BIOS words queued, %d consumed' % (len(presses), len(heads)))
    last_head = {}
    for h in heads:
        last_head[h['f']] = h['head']
    keys = []
    for k, (p, h) in enumerate(zip(presses, heads)):
        c = h['f']
        prev = snap.get(c - 1)
        if prev is None or prev['head'] == h['head']:
            raise ScriptError('key %d (%s) consumption at f=%X unpinned' % (k, p['press'], c))
        if c in snap and snap[c]['head'] != last_head[c]:
            raise ScriptError('key %d (%s): S(%X) disagrees with its H record' % (k, p['press'], c))
        word = p['bios']
        keys.append((c, word >> 8, word & 0xFF))
    p27 = next((r for r in recs if r['kind'] in ('S', 'P') and r.get('mode') == 0x27), None)
    if p27 is None:
        raise ScriptError('mode 0x27 never observed')
    if not keys or keys[0][0] != p27['f']:
        raise ScriptError('the first key (f=%s) is not the frame mode 0x27 appears (f=%X)'
                          % (keys[0][0] if keys else None, p27['f']))
    bits, prev_kb = [], 0
    for f in sorted(snap):
        if f < p27['f']:
            continue
        kb = raw_to_kb(snap[f]['raw'])
        if kb != prev_kb:
            if f - 1 not in snap:
                raise ScriptError('pad change at f=%X unpinned (no S record at f=%X)' % (f, f - 1))
            bits.append((f, kb))
            prev_kb = kb
    end = next((r for r in recs if r['kind'] == 'X'), None)
    if end is None:
        raise ScriptError('the scenario end (X record) was not reached')
    out = ['# gp port script v2: scenario %s' % name,
           'enter_frame %d' % p27['f'],
           'enter_state %04X' % p27['st']]
    ev = [(c, 0, i, 'key %d %02X %02X' % (c, s, a)) for i, (c, s, a) in enumerate(keys)]
    ev += [(f, 1, 0, 'bits %d %04X' % (f, kb)) for f, kb in bits]
    out += [t for _, _, _, t in sorted(ev)]
    out.append('end %d' % end['f'])
    return '\n'.join(out) + '\n'


def trace_diff(a_lines, b_lines):
    a, b = snapshots(a_lines), snapshots(b_lines)
    common = sorted(set(a) & set(b))
    first = field = tick_first = None
    for f in common:
        if tick_first is None and a[f]['tick'] != b[f]['tick']:
            tick_first = f
        if first is None:
            for n in TRACE_FIELDS:
                if a[f][n] != b[f][n]:
                    first, field = f, n
                    break
    return dict(first=first, field=field, compared=len(common), tick_first=tick_first)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('port-script', 'trace-diff'))
    ap.add_argument('--scenario')
    ap.add_argument('--capture')
    ap.add_argument('--out')
    ap.add_argument('--a')
    ap.add_argument('--b')
    a = ap.parse_args()
    if a.cmd == 'trace-diff':
        with open(os.path.join(a.a, 'poll.log')) as fa, open(os.path.join(a.b, 'poll.log')) as fb:
            r = trace_diff(fa.read().splitlines(), fb.read().splitlines())
        print('gp_session: trace-diff: %d frames compared; first difference %s%s; first tick difference %s'
              % (r['compared'], 'none' if r['first'] is None else 'f=%X' % r['first'],
                 '' if r['field'] is None else ' (%s)' % r['field'],
                 'none' if r['tick_first'] is None else 'f=%X' % r['tick_first']))
        return 0
    with open(os.path.join(a.capture, 'poll.log')) as f:
        try:
            text = port_script(a.scenario, f.read().splitlines())
        except ScriptError as e:
            print('gp_session: port-script: %s: %s' % (a.scenario, e))
            return 1
    with open(a.out, 'w') as f:
        f.write(text)
    print('gp_session: port-script: %s: wrote %s (%d lines)' % (a.scenario, a.out, text.count('\n')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
