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
  E ms=<int> reason=<exit|time-limit|end> rc=<int>   end: stopped at the script's end (STOP_AT_END)"""
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
    # Plan gameplay-u6b, record gameplay-u6 §U6.11 (appended, so older poll.log
    # lines simply lack them): the last reaction each side applied (0x34E2C's
    # 0x34EF6 store to DS_001088A8 + side), the slots' characters (+0x7A) and
    # slot 0's +0x43 (0x1A6AC's block bits 0x20/0x10).
    ('r0', 0x1088A8, 1), ('r1', 0x1088A9, 1), ('c0', 0x10782A, 1), ('c1', 0x1078BE, 1),
    ('s0_43', 0x1077F3, 1),
)
TRACE_FIELDS = ('mode', 'st', 'raw', 'pad', 'e0', 'e2', 'rng', 'cred', 's0_5a', 's1_5a')
MOVE_FIELDS = ('c0', 'c1', 'r0', 'r1', 's0_43')     # gp_compare's moves claim (record gameplay-u6 §U6.11)

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

# U11 (record 2026-10-01-gameplay-u11 §K.2): the keys the int 16h key loop
# 0x24D08..0x24EE7 and its blocking readers (the prompt 0x24A64, the pause
# 0x24E54) act on in a match. Space, y and n are the standard set-1 make words
# (scan << 8 | ascii); an Alt-letter is its scan with ascii 0, the form the
# extended-key arms 0x24D96..0x24DB8 test. The key-state side writes the
# letter's own scan: nothing reads Alt's scan 0x38 (record §K.2), and S (0x1F),
# M (0x32) and N (0x31) are also P1's up, b3 and b2 in the default binding.
KEYS.update({'space': (0x39, 0x3920), 'y': (0x15, 0x1579), 'n': (0x31, 0x316E),
             'alt-q': (0x10, 0x1000), 'alt-s': (0x1F, 0x1F00), 'alt-m': (0x32, 0x3200)})

_DEC = ('ms', 'rc', 'step', 'late', 'ring')
_TEXT = ('reason', 'press', 'release')


def raw_to_kb(raw):
    """DS_000E1C30 = (+0x2D8 << 24) | (+0x2D9 << 8) (0x500C4) -> the kb word."""
    return ((raw >> 24) & 0xFF) << 8 | (raw >> 8) & 0xFF


def format_s(ms, vals, kb, head, tail, fields=SNAP_FIELDS):
    body = ' '.join('%s=%0*X' % (n, 2 * sz, vals[n]) for n, _, sz in fields)
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
    # U4 (spec §4.4): MAIN MENU -> START MENU -> LEFT PLAYER ARCADE, P1 idle
    # through the match to game over and mode 3. The 150-frame gaps are the
    # planner's probe's 2.5 s and the 200 s limit covers its 169.9 s (harness
    # values, not game values).
    'gp-idle-loss': dict(time_limit=200, steps=(
        ('boot', ENTER_WAIT, ('key', 'enter')),       # mode 3 -> 0x27, MAIN MENU on "Start"
        ('after_mode', 0x27, 150, ('key', 'enter')),  # START MENU, cursor on row 0 (spec §3.3)
        ('after', 150, ('key', 'enter')),             # LEFT PLAYER ARCADE: mode 0x2D
        ('until_mode', 0x03, 0),                      # back in mode 3 after game over
    )),
}
SCENARIOS['gp-idle-loss-run2'] = dict(SCENARIOS['gp-idle-loss'])   # the determinism run (spec §7 Q1)

# Plan gameplay-u6b (record gameplay-u6 §U6.12): P1 (Sauron, character 0, the
# pick time-out's) performs twelve attempts against the CPU in round 1, 100
# frames apart from 10 frames after mode 6 begins (harness values): the
# keyboard-table entries 0x1A, 0x00, 0x01, 0x1B, 0x06, 0x15 (reactions 0x10,
# 0x20, 0x24, 0x11, 0x2D, 0x3D) twice, each through tools/gp_moves.py presses
# (facing 0, a 4-frame step). `gp_moves.py steps --char 0 --facing 0 --moves
# 1A,00,01,1B,06,15,1A,00,01,1B,06,15 --gap 100 --step 4` prints these steps
# (tools/tests/test_gp_moves.py regenerates them from the image).
U6_MOVES_STEPS = (
    ('after', 100, ('pad', ('p1.left',), 4)),
    ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
    ('after', 4, ('pad', ('p1.right',), 2)),
    ('after', 4, ('pad', ('p1.right',), 4)),
    ('after', 92, ('pad', ('p1.down',), 4)),
    ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
    ('after', 4, ('pad', ('p1.up',), 4)),
    ('after', 4, ('pad', ('p1.left',), 4)),
    ('after', 92, ('pad', ('p1.b2', 'p1.b3'), 4)),
    ('after', 100, ('pad', ('p1.down',), 2)),
    ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
    ('after', 4, ('pad', ('p1.down',), 4)),
    ('after', 4, ('pad', ('p1.up',), 4)),
    ('after', 92, ('pad', ('p1.left',), 4)),
    ('after', 4, ('pad', ('p1.down',), 4)),
    ('after', 4, ('pad', ('p1.up',), 4)),
    ('after', 92, ('pad', ('p1.b0', 'p1.b1'), 4)),
    ('after', 100, ('pad', ('p1.left',), 4)),
    ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
    ('after', 4, ('pad', ('p1.right',), 2)),
    ('after', 4, ('pad', ('p1.right',), 4)),
    ('after', 92, ('pad', ('p1.down',), 4)),
    ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
    ('after', 4, ('pad', ('p1.up',), 4)),
    ('after', 4, ('pad', ('p1.left',), 4)),
    ('after', 92, ('pad', ('p1.b2', 'p1.b3'), 4)),
    ('after', 100, ('pad', ('p1.down',), 2)),
    ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
    ('after', 4, ('pad', ('p1.down',), 4)),
    ('after', 4, ('pad', ('p1.up',), 4)),
    ('after', 92, ('pad', ('p1.left',), 4)),
    ('after', 4, ('pad', ('p1.down',), 4)),
    ('after', 4, ('pad', ('p1.up',), 4)),
)
SCENARIOS['gp-u6-moves'] = dict(time_limit=130, steps=(
    ('boot', ENTER_WAIT, ('key', 'enter')),       # mode 3 -> 0x27
    ('after_mode', 0x27, 150, ('key', 'enter')),  # START MENU
    ('after', 150, ('key', 'enter')),             # LEFT PLAYER ARCADE: mode 0x2D
    ('after_mode', 0x06, 10, ('pad', ('p1.b0', 'p1.b1'), 4)),   # attempt 1 (offset 0)
) + U6_MOVES_STEPS + (('after', 92, ('end',)),))  # 100 frames after the last attempt began
# The re-capture after gp-u6-moves was contaminated by unscripted keyboard input
# from f=0x9CB (U6b task-7 report §6.5, check-input; user decision; record
# gameplay-u6 §U6.22): the same steps object and time limit under a new name (record
# §G.24 item 5), stopped at its end (STOP_AT_END).
SCENARIOS['gp-u6-moves-b'] = dict(SCENARIOS['gp-u6-moves'])

# U5 (record 2026-10-01-gameplay-u5 §C5.4): LEFT PLAYER ARCADE as gp-idle-loss, then
# P1 walks the character-select cursor DS_00108166[0] (0x43B24: e0 bits 0x10 right
# while < 6, 0x20 left while > 0, 0x40 down +4 then clamped to 6, 0x80 up -4 while >= 4)
# over all seven cells, 0 (left: the clamp, a no-op) 1 2 3 6 5 4 0 1, confirms on 1 with
# p1.start (e0 bit 0, 0x43CAD) and runs to the first frame of mode 6 (the round).
# Harness values: the hold 6 (>= 2 for the level, 0x500C4; < 0x1F, the 0xC000C000 repeat
# first period 0x1E that the mode 0x27 menu arms at 0x251C6), the 60-frame wait into mode
# 0x10 and the 40-frame gaps (the confirm lands 420 frames in, before the earliest
# pick time-out 14 * 64 = 896 frames in: countdown 0xF, 0x437C6, stepped at f & 0x3F == 0),
# and the 60 s limit (the mode-6 frame is expected near 47 s, §C5.7).
CHARSEL_WALK = ('p1.left', 'p1.right', 'p1.right', 'p1.right', 'p1.down',
                'p1.left', 'p1.left', 'p1.up', 'p1.right')
SCENARIOS['gp-u5-charsel'] = dict(time_limit=60, steps=(
    ('boot', ENTER_WAIT, ('key', 'enter')),           # mode 3 -> 0x27, MAIN MENU on "Start"
    ('after_mode', 0x27, 150, ('key', 'enter')),      # START MENU, cursor on row 0 (spec §3.3)
    ('after', 150, ('key', 'enter')),                 # LEFT PLAYER ARCADE: mode 0x2D
    ('after_mode', 0x10, 60, ('pad', (CHARSEL_WALK[0],), 6)),
) + tuple(('after', 40, ('pad', (n,), 6)) for n in CHARSEL_WALK[1:]) + (
    ('after', 40, ('pad', ('p1.start',), 6)),         # confirm cursor 1
    ('until_mode', 0x06, 0),                          # the round's first frame
))

# U11 (plan 2026-10-01-gameplay-u11-in-match-keys.md, record §K.6): the
# in-match keys, pressed in round 1 (mode 6) of the gp-idle-loss path, then
# ABANDON CONQUEST's yes in the join's mode 0x17. The order makes every event
# change the latch DS_00105F30 (record §K.6). A prompt's or the pause's answer
# is queued in its opener's spin (`after` 0), so the blocking reader (0x24A64,
# 0x24E54) finds it at once and f never has to advance inside the blocking
# loop (spec §7 Q5, record §K.5). Harness values, not game values: the
# 10-frame gaps exceed the 1-4 iteration BIOS latency (record §G.7.3) plus
# HOLD_FRAMES; the 20 frames after F2 keep the ESC inside the join's 0x78-frame
# mode 0x17 (0x28E75); 90 s covers mode 6 at 55.1 s in gp-idle-loss (§G.18)
# plus the restart's boot movies (14.7 s in gp-pads, §G.7.3). KEYS_EXTRA are
# the S fields this scenario adds: the latch (0x24D4D), the sample pause byte
# (0x1D220 xor byte [0x1028DB],1) and the music pause byte (written at 0x1D1C2
# and 0x1D213, in the function entered at 0x1D1B0).
KEYS_EXTRA = (('lat', 0x105F30, 4), ('spz', 0x1028DB, 1), ('mpz', 0x1028DA, 1))
SCENARIOS['gp-keys-fight'] = dict(time_limit=90, extra=KEYS_EXTRA, steps=(
    ('boot', ENTER_WAIT, ('key', 'enter')),         # 0: mode 3 -> 0x27
    ('after_mode', 0x27, 150, ('key', 'enter')),    # 1: START MENU
    ('after', 150, ('key', 'enter')),               # 2: LEFT PLAYER ARCADE (mode 0x2D)
    ('after_mode', 0x06, 10, ('key', 'enter')),     # 3: Enter in a fight: latched only (0x24ECF)
    ('after', 10, ('key', 'space')),                # 4: - PAUSED - (0x24E19) ...
    ('after', 0, ('key', 'space')),                 # 5: ... ended by the next space (0x24E67)
    ('after', 10, ('key', 'alt-s')),                # 6: samples paused (0x24DC9 0x1D220)
    ('after', 10, ('key', 'esc')),                  # 7: ABANDON CONQUEST? Y/N (0x24EC5) ...
    ('after', 0, ('key', 'n')),                     # 8: ... no (0x24AB5)
    ('after', 10, ('key', 'alt-m')),                # 9: music paused (0x24DBF 0x1D1B0)
    ('after', 10, ('key', 'alt-q')),                # 10: QUIT TO DOS? Y/N (0x24DDF) ...
    ('after', 0, ('key', 'n')),                     # 11: ... no
    ('after', 10, ('key', 'alt-s')),                # 12: samples back
    ('after', 10, ('key', 'alt-m')),                # 13: music back
    ('after', 10, ('pad', ('p1.start',), 3)),       # 14: F1 = P1 b0 (side 0 already in: 0x28CD7)
    ('after', 10, ('pad', ('p2.start',), 3)),       # 15: F2 = P2 joins (0x2525F 0x28CC8, 0x25269 0x28DA4)
    ('after', 20, ('key', 'esc')),                  # 16: ABANDON CONQUEST? Y/N in mode 0x17 ...
    ('after', 0, ('key', 'y')),                     # 17: ... yes: the 0x24AB0 soft restart
    ('until_mode', 0x03, 0),                        # 18: the restart's mode 3 (0x20CE6 0x10E80)
))


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


# The scenarios whose capture stops at the script's end (gp_capture.STOP_TAIL
# frames after the X record) instead of running to time_limit. Opt-in: every
# other scenario still records its post-end tail (gp-idle-loss's, and the
# post-restart movie gp-keys-fight's plan counts on). gp-u6-moves ran 130 s for
# an X at 75 s, 278 MB (U6b task-7 report §6.1); it and its re-capture
# gp-u6-moves-b (a copy of it) are defined on U6b's branch. A name not in
# SCENARIOS is inert.
STOP_AT_END = frozenset({'gp-u6-moves', 'gp-u6-moves-b'})


class ScriptError(Exception):
    pass


def snapshots(lines):
    out = {}
    for l in lines:
        r = parse(l)
        if r and r['kind'] == 'S':
            out.setdefault(r['f'], r)
    return out


def port_script(name, lines, end=None):
    """poll.log v2 -> port script v2 (spec §4.1). Keys at their consumption
    frame (the H record paired FIFO with the I press, pinned by S(f-1) showing
    the old head); bits at each change of S.raw, pinned by S(f-1). The key loop
    0x24D08..0x24EE7 drains every queued word in one iteration, so the poller
    writes one H record per consumed word and S(f) must show the head of the
    last H record of frame f (record §G.4)."""
    recs = [r for r in (parse(l) for l in lines) if r]
    snap = snapshots(lines)
    presses = [r for r in recs if r['kind'] == 'I' and 'press' in r and r.get('bios') is not None]
    for k, p in enumerate(presses):
        if p.get('ring') == 0:
            raise ScriptError('key %d (%s) at f=%X was not queued: the BIOS ring was full'
                              % (k, p['press'], p['f']))
    heads = [r for r in recs if r['kind'] == 'H']
    if len(heads) < len(presses):
        raise ScriptError('%d BIOS words queued, %d consumed' % (len(presses), len(heads)))
    last_head, before = {}, {}
    for k, h in enumerate(heads):
        last_head[h['f']] = h['head']
        if h['f'] not in before:                # the head before frame f's first word
            before[h['f']] = heads[k - 1]['head'] if k else None
    keys = []
    for k, (p, h) in enumerate(zip(presses, heads)):
        c = h['f']
        prev = snap.get(c - 1)
        old = before[c]
        if prev is None or prev['head'] == h['head'] or (old is not None and prev['head'] != old):
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
    xrec = next((r for r in recs if r['kind'] == 'X'), None)
    if xrec is None:
        raise ScriptError('the scenario end (X record) was not reached')
    last = xrec['f']
    if end is not None:
        if end > last:
            raise ScriptError('--end %d is past the capture end f=%X' % (end, last))
        if end < keys[0][0]:
            raise ScriptError('--end %d is before the Enter (f=%d): the script would not start' % (end, keys[0][0]))
        last = end
    out = ['# gp port script v2: scenario %s%s' % (name, '' if end is None else ' (cut at %d)' % end),
           'enter_frame %d' % p27['f'],
           'enter_state %04X' % p27['st']]
    ev = [(c, 0, i, 'key %d %02X %02X' % (c, s, a)) for i, (c, s, a) in enumerate(keys) if c <= last]
    ev += [(f, 1, 0, 'bits %d %04X' % (f, kb)) for f, kb in bits if f <= last]
    out += [t for _, _, _, t in sorted(ev)]
    out.append('end %d' % last)
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
    ap.add_argument('--end', type=int)
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
            text = port_script(a.scenario, f.read().splitlines(), end=a.end)
        except ScriptError as e:
            print('gp_session: port-script: %s: %s' % (a.scenario, e))
            return 1
    with open(a.out, 'w') as f:
        f.write(text)
    print('gp_session: port-script: %s: wrote %s (%d lines)' % (a.scenario, a.out, text.count('\n')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
