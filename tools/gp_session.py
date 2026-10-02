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
  E ms=<int> reason=<exit|time-limit|end> rc=<int>   end: stopped at the script's end (STOP_AT_END)
  W ms=<int> f=<hex4> step=<n> addr=<hex8> len=<n> was=<hex> now=<hex> late=<0|1> race=<0|1>
        a memory poke written in the spin of f, read by iteration f + 1 (plan U9/U10)"""
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

_DEC = ('ms', 'rc', 'step', 'late', 'ring', 'len', 'race')
_TEXT = ('reason', 'press', 'release', 'was', 'now')


def raw_to_kb(raw):
    """DS_000E1C30 = (+0x2D8 << 24) | (+0x2D9 << 8) (0x500C4) -> the kb word."""
    return ((raw >> 24) & 0xFF) << 8 | (raw >> 8) & 0xFF


def format_s(ms, vals, kb, head, tail, fields=SNAP_FIELDS):
    body = ' '.join('%s=%0*X' % (n, 2 * sz, vals[n]) for n, _, sz in fields)
    return 'S ms=%d %s kb=%04X head=%04X tail=%04X' % (ms, body, kb, head, tail)


def format_w(ms, f, step, addr, was, now, late, race):
    """The W record of one poke write (plan U9/U10): `was` the bytes before,
    `now` the bytes written, hex."""
    return ('W ms=%d f=%04X step=%d addr=%08X len=%d was=%s now=%s late=%d race=%d'
            % (ms, f, step, addr, len(now), bytes(was).hex().upper(), bytes(now).hex().upper(), late, race))


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
    # U7 (plan 2026-10-01-gameplay-u7-two-players.md, record 2026-10-01-gameplay-u7-derivations.md
    # §T.3): LEFT PLAYER ARCADE (b1f = 1, credits 5 -> 4), then P2 joins in the
    # character select (0x11F28(1): a credit and P2's start mask 0x100 newly
    # pressed, 0x43B4E; b1f |= 2, no debit), each side moves its cursor once and
    # confirms (bit 0 of its command word, 0x43CAD; both confirmed ends the
    # select, 0x43BF0), and both press keys in the fight. Harness values: holds
    # of 5 (a press reaches the pad level one iteration late, record §G.7.2, so
    # a 1-frame press never acts), the 60/30-frame gaps, the end 60 frames after
    # the last press, and the 70 s limit (record §T.3).
    'gp-twop': dict(time_limit=70, steps=(
        ('boot', ENTER_WAIT, ('key', 'enter')),       # mode 3 -> 0x27, MAIN MENU on "Start"
        ('after_mode', 0x27, 150, ('key', 'enter')),  # START MENU, cursor on row 0 (spec §3.3)
        ('after', 150, ('key', 'enter')),             # LEFT PLAYER ARCADE: mode 0x2D
        ('after_mode', 0x10, 60, ('pad', ('p2.start',), 5)),   # P2 joins (F2)
        ('after', 60, ('pad', ('p1.right',), 5)),     # P1 cursor 0 -> 1
        ('after', 30, ('pad', ('p2.left',), 5)),      # P2 cursor 5 -> 4
        ('after', 30, ('pad', ('p1.b0',), 5)),        # P1 confirms (U)
        ('after', 30, ('pad', ('p2.b0',), 5)),        # P2 confirms (Home): the select ends
        ('after_mode', 0x06, 60, ('pad', ('p1.right', 'p2.left'), 30)),  # both walk in
        ('after', 60, ('pad', ('p1.b1', 'p2.b2'), 5)),
        ('after', 30, ('pad', ('p1.b2', 'p2.b1'), 5)),
        ('after', 60, ('end',)),
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
# pick time-out 14 * 64 = 896 frames in: countdown 0xF, stored at 0x437D1; 0x437C6
# stores 5 when DS_00108173 != 0; stepped at f & 0x3F == 0),
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


# Gameplay U8 (plan 2026-10-01-gameplay-u8-other-modes.md, record
# 2026-10-01-gameplay-u8-derivations.md §U8.3): START MENU rows 1..6 and the
# attract start. The walk: the mode-3 Enter (MAIN MENU on "Start"), the Enter
# that opens START MENU on row 0 (0x2CB74 -> 0x2FFC4(0xBCCCC)), one p1.down per
# row (menu_step 0x3055E: the level edge of 0x40004000) or, for row 6, one p1.up
# (0x30511..0x3052F: row -1 wraps to the count, 7), then the Enter that runs the
# row's setter (0x3046F..0x304A2). Harness values: U4's 150-frame gap; 60-frame
# gaps and a 4-frame hold (one level edge: the hold is under the 0x1E-frame
# repeat delay that 0x251C6..0x251DA re-arms every mode-0x27 frame); the end 300
# frames into the reach mode; time_limit = U4's wall timeline (§U8.6) + 6 s.
def _u8_menu(moves, reach, time_limit):
    steps = (('boot', ENTER_WAIT, ('key', 'enter')),
             ('after_mode', 0x27, 150, ('key', 'enter')))
    steps += tuple(('after', 60, ('pad', (m,), 4)) for m in moves)
    steps += (('after', 60, ('key', 'enter')), ('until_mode', reach, 300))
    return dict(time_limit=time_limit, steps=steps)


SCENARIOS['gp-u8-right-arcade'] = _u8_menu(('p1.down',), 6, 66)          # row 1, mode 0x2E
SCENARIOS['gp-u8-left-training'] = _u8_menu(('p1.down',) * 2, 6, 67)     # row 2, mode 0x28
SCENARIOS['gp-u8-right-training'] = _u8_menu(('p1.down',) * 3, 6, 68)    # row 3, mode 0x29
SCENARIOS['gp-u8-tug-of-war'] = _u8_menu(('p1.down',) * 4, 6, 69)        # row 4, mode 0x2A
# Row 5, mode 0x2B: the team select 0x44798 has no time-out (§U8.4), so the
# scenario ends 300 frames into mode 0x10.
SCENARIOS['gp-u8-endurance'] = _u8_menu(('p1.down',) * 5, 0x10, 46)
SCENARIOS['gp-u8-handicap'] = _u8_menu(('p1.up',), 6, 66)                # row 6, mode 0x2C

# The attract start (spec §3.4; 0x11D04 -> 0x11F28(0)): P1's start (F1) in mode
# 3 at the K11 boot wait, held 4; no Enter, so the port script's arm is the
# press (arm='pad', port_script's pad_arm).
SCENARIOS['gp-u8-attract-start'] = dict(time_limit=61, arm='pad', steps=(
    ('boot', ENTER_WAIT, ('pad', ('p1.start',), 4)),
    ('until_mode', 6, 300),
))


# Plan gameplay-u9-u10 (record 2026-10-02-gameplay-u9-u10-derivations.md §W.2-§W.6): the
# win path and the ending, reached under memory pokes (spec 2026-09-30-reverse-completion-design
# §4 G, §7 "Wins and endings without pokes"). A ('poke', ((addr, bytes), ...)) action writes
# the bytes at the linear address in the spin of F - 1 (the iteration F reads them), logged as
# W records; the port replays them as `poke` lines at F. WIN_EXTRA: the S fields both scenarios
# add (record §W.2): the stage word (0x25848), the match result (0x27BA4), the round wins
# (0x27C48), the round index, the final's KO count (0x274FC), the final flag (0x25C88), the
# death-done byte (0x37FF2), the lands-held bytes and the seven land marks (0x41C28, 0x286BC),
# slot 0's score (+0x3C) and its world-domination count (+0x82, 0x416C2).
WIN_EXTRA = (('afc', 0x104AFC, 2), ('ad4', 0x104AD4, 4), ('w2', 0x104AF2, 1), ('w3', 0x104AF3, 1),
             ('b1e', 0x104B1E, 1), ('b21', 0x104B21, 1), ('b14', 0x104B14, 1), ('b0c', 0x104B0C, 1),
             ('t104', 0x108104, 2), ('m106', 0x108106, 4), ('m10a', 0x10810A, 4),
             ('sc0', 0x1077EC, 4), ('c82', 0x107832, 1))
WIN_FIELDS = tuple(n for n, _, _ in WIN_EXTRA)
# P2's +0x5A damage byte (DS_0010789E) at 0x78: the KO the round-end checks test
# (0x27FA8 `cmp 0x78`, 0x272DC's DS_00104B12 side), record §W.3.
KO_P2 = ('poke', ((0x10789E, b'\x78'),))
# The byte 0x37EA0 sets at 0x37FF2 when a KO'd fighter's death animation ends; mode
# 0xD (0x274FC) waits for it, and a poked KO never starts that animation (record §W.5).
DEATH_DONE = ('poke', ((0x104B0C, b'\x01'),))


def lands_marked(side, char):
    """The seven land marks DS_00108106..0x10810C as 0x286BC writes a won land:
    0x80 | side << 6 | the winner's character (record §W.4)."""
    return bytes([0x80 | side << 6 | char]) * 7

# The menu path of gp-idle-loss (spec §4.4) and the immediate confirm of the
# character-select cursor 0 (character 0, SAURON; p1.start = e0 bit 0, 0x43CAD, as
# gp-u5-charsel's confirm, record gameplay-u5 §C5.4). Harness values: the 60-frame
# wait into mode 0x10 and the hold 6 (gp-u5-charsel's), the 10 frames into each
# fight mode before a poke (the round is live: 0x27FA8 runs every mode-6 frame).
WIN_MENU = (
    ('boot', ENTER_WAIT, ('key', 'enter')),           # mode 3 -> 0x27, MAIN MENU on "Start"
    ('after_mode', 0x27, 150, ('key', 'enter')),      # START MENU, cursor on row 0 (spec §3.3)
    ('after', 150, ('key', 'enter')),                 # LEFT PLAYER ARCADE: mode 0x2D
    ('after_mode', 0x10, 60, ('pad', ('p1.start',), 6)),   # confirm cursor 0: character 0
)
# U9 (record §W.6): two poked KOs win match 1 (0x27BA4: 2 wins of 3), the conquered-lands
# screen (mode 0x12) and the next opponent's wipe, to 60 frames into match 2's round 1.
# 110 s: 1.4x the port-predicted end at 78 s (record §W.7), a harness value.
SCENARIOS['gp-u9-win'] = dict(time_limit=110, extra=WIN_EXTRA, steps=WIN_MENU + (
    ('after_entry', 0x06, 1, 10, KO_P2),              # round 1: P2 KO'd, mode 8
    ('after_entry', 0x06, 2, 10, KO_P2),              # round 2: P1 wins the match, mode 9
    ('after_entry', 0x06, 3, 60, ('end',)),           # match 2, round 1
))
# U10 (record §W.6): round 1 also marks all seven lands P1's, so the won match is the
# seventh land (0x41C28 state 5: DS_00108104[0] == 7): WORLD DOMINATION, the health
# bonus (modes 0x23/0x22/0x24), the final (mode 0xC: seven opponents, each KO'd by a poke
# and replaced in mode 0xD once DEATH_DONE is poked), mode 0xF, the ending (mode 0x1F),
# the high-score entry (mode 0x1E) and back to mode 3. 240 s: 1.3x the port-predicted
# end at 185 s (record §W.7), a harness value. memsize=64: DOSBox-X's guest memory in MB,
# a harness value, not a game value (record §W.13): under DOSBox-X's default 16 MB the
# original printed "Primal Rage is out of memory." in the 4th final fight (f=0x14F6).
SCENARIOS['gp-u10-ending'] = dict(time_limit=240, extra=WIN_EXTRA, memsize=64, steps=WIN_MENU + (
    ('after_entry', 0x06, 1, 10, ('poke', ((0x108106, lands_marked(0, 0)), (0x10789E, b'\x78')))),
    ('after_entry', 0x06, 2, 10, KO_P2),
) + tuple(st for k in range(1, 8) for st in (
    ('after_entry', 0x0C, k, 10, KO_P2),              # the final's k-th opponent KO'd: mode 0xD
    ('after_entry', 0x0D, k, 10, DEATH_DONE),         # replaced (k < 7) or mode 0xF (k = 7)
)) + (
    ('until_mode', 0x03, 0),                          # back in mode 3 after the high-score entry
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
    | ('after_entry', mode, k, n, act) | ('until_mode', mode, n); an ('end',) action
    ends the scenario at its frame. after_entry: F = the frame of the k-th entry
    into `mode` (on_snap) + n.
    An action for frame F fires at the first spin snapshot with f >= F - 1, so
    iteration F samples it (spec §3.1)."""

    def __init__(self, steps):
        self.steps = list(steps)
        self.i = 0
        self.prev_frame = None
        self.frame_of = {}          # step -> its frame F (set when it fires)
        self.mode_first = {}
        self.snap_mode = None
        self.entry_count = {}
        self.entry_frame = {}       # (mode, k) -> the first S frame of the k-th entry
        self.end_frame = None
        self.fired = 0
        self.total = sum(1 for st in self.steps if st[0] != 'until_mode' and st[-1] != ('end',))

    def _target(self, st):
        if st[0] == 'after_mode':
            f0 = self.mode_first.get(st[1])
            return None if f0 is None else f0 + st[2]
        if st[0] == 'after':
            return None if self.prev_frame is None else self.prev_frame + st[1]
        if st[0] == 'after_entry':
            f0 = self.entry_frame.get((st[1], st[2]))
            return None if f0 is None else f0 + st[3]
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

    def on_snap(self, f, mode):
        """An accepted S record's mode (gp_capture.Poller, after on_mode): the k-th
        entry into a mode is the k-th S record whose mode differs from the previous
        S record's, counted, like mode_first, once the boot step fired. Only S records
        count: a P record can catch a mode word mid-iteration (plan U9/U10)."""
        prev, self.snap_mode = self.snap_mode, mode
        if prev is None or prev == mode or (self.i == 0 and self.steps and self.steps[0][0] == 'boot'):
            return
        k = self.entry_count.get(mode, 0) + 1
        self.entry_count[mode] = k
        self.entry_frame[(mode, k)] = f

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
# SCENARIOS is inert. gp-twop (U7, record §T.5) stops at its end too: its end is
# 60 frames after the last press and nothing past it is compared (gp_twop.py
# checks up to the X record; the port replay's script ends there, so a capture
# frame past it is unexplained either way; gp-twop-oracle pins N and F at or
# before it), so the ~23 s of idle fight the 70 s limit leaves would only cost bytes.
# The six gp-u8 START MENU rows (record §U8.12) stop too: their frame N is the
# capture frame after the port replay's last frame (the replay ends at the
# script's end) and F the replay's end + 1, so nothing later is compared, and
# the 60-frame tail still holds the frames past the end that N names; ENDURANCE
# ends 300 frames into its team select (D3), where the idle tail is a still
# screen. The attract start stops too (record §U8.14): the same arrangement, its N and F
# sit at the replay's end. gp-u9-win and gp-u10-ending (plan U9/U10) stop too: nothing
# past their end is compared.
STOP_AT_END = frozenset({'gp-u6-moves', 'gp-u6-moves-b', 'gp-twop',
                         'gp-u8-right-arcade', 'gp-u8-left-training', 'gp-u8-right-training',
                         'gp-u8-tug-of-war', 'gp-u8-endurance', 'gp-u8-handicap',
                         'gp-u8-attract-start', 'gp-u9-win', 'gp-u10-ending'})


class ScriptError(Exception):
    pass


def snapshots(lines):
    out = {}
    for l in lines:
        r = parse(l)
        if r and r['kind'] == 'S':
            out.setdefault(r['f'], r)
    return out


def pad_arm(snap, keys):
    """The arm of a scenario with arm='pad' (the attract start, record gameplay-u8
    §U8.3): no Enter, so the arm frame is the first S record whose bitmap is
    non-zero, pinned by S(f - 1) in mode 3 (0x11D04 runs in mode 3 only, 0x25238);
    every key is consumed at or after it."""
    f = next((f for f in sorted(snap) if raw_to_kb(snap[f]['raw']) != 0), None)
    if f is None:
        raise ScriptError('the pad arm never reached the bitmap')
    if f - 1 not in snap or snap[f - 1]['mode'] != 3 or snap[f]['mode'] != 3:
        raise ScriptError('the pad arm at f=%X is not pinned in mode 3' % f)
    if keys and keys[0][0] < f:
        raise ScriptError('a key (f=%X) is consumed before the pad arm (f=%X)' % (keys[0][0], f))
    return dict(f=f, st=snap[f]['st'], mode=snap[f]['mode'])


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
    pad = SCENARIOS.get(name, {}).get('arm', 'enter') == 'pad'
    if pad:
        p27 = pad_arm(snap, keys)
    else:
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
    pokes = []
    for k, w in enumerate(r for r in recs if r['kind'] == 'W'):
        if w['race']:
            raise ScriptError('poke %d (%08X) at f=%X raced the iteration' % (k, w['addr'], w['f']))
        if w['f'] not in snap:
            raise ScriptError('poke %d (%08X) at f=%X unpinned (no S record at f=%X)'
                              % (k, w['addr'], w['f'], w['f']))
        pokes.append((w['f'] + 1, w['addr'], w['now']))
    xrec = next((r for r in recs if r['kind'] == 'X'), None)
    if xrec is None:
        raise ScriptError('the scenario end (X record) was not reached')
    last = xrec['f']
    if end is not None:
        if end > last:
            raise ScriptError('--end %d is past the capture end f=%X' % (end, last))
        if end < p27['f']:
            raise ScriptError('--end %d is before the %s (f=%d): the script would not start'
                              % (end, 'pad arm' if pad else 'Enter', p27['f']))
        last = end
    out = ['# gp port script v2: scenario %s%s' % (name, '' if end is None else ' (cut at %d)' % end)]
    out += ['arm pad'] if pad else []
    out += ['enter_frame %d' % p27['f'], 'enter_state %04X' % p27['st']]
    ev = [(c, 0, i, 'key %d %02X %02X' % (c, s, a)) for i, (c, s, a) in enumerate(keys) if c <= last]
    ev += [(f, 1, 0, 'bits %d %04X' % (f, kb)) for f, kb in bits if f <= last]
    ev += [(f, 2, i, 'poke %d %08X %s' % (f, a, d)) for i, (f, a, d) in enumerate(pokes) if f <= last]
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
