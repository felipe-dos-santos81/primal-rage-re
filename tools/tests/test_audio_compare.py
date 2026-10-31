# tools/tests/test_audio_compare.py
"""The register-stream oracle's unit tests (record 2026-10-25-music-truth §M4).

Each case builds a synthetic `.dro` (the DOSBox-X v2 container `opl_trace.py`
parses) and a synthetic `PR_AUDIO_DUMP` port stream, then runs the tool's CLI
(`test_align_groups_port_side_dropped_write` calls `align_groups` directly).
The cases are the oracle's contract: an identical pair passes; a dropped, extra
or wrong write fails (at its first tick group, or at the anchor's four-tick
prefix when the mutation breaks that prefix); an unanchored stream fails; a
capture that is absent is skipped by name; a present capture with a missing
expected file fails; `summary` fails a partial inventory, a `NOT RUN` row and a
`FAIL` row; the declared intra-tick order model, the declared one-tick capture
label model and the unclaimed request-boundary tick do not fail.
"""
import os
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOL = os.path.join(ROOT, "tools", "audio_compare.py")
sys.path.insert(0, os.path.join(ROOT, "tools"))
import audio_compare  # noqa: E402
import opl_trace  # noqa: E402

# The synthetic port section's song handle and bank (not a shipped bank).
DC = 0x0A800008
BANK = 0x12345678
ANCHOR_TICK = 60


def _delay_ms(ms):
    """Encode `ms` of delay as delay256 commands (each adds value+1 ms)."""
    out = []
    while ms > 0:
        step = min(ms, 256)
        out += [0x7E, step - 1]
        ms -= step
    return out


def make_dro(events, delay256=0x7E, delay_shift8=0x7F):
    """Build a v2 DBRAWOPL file from [(ms, reg, value), ...] (non-decreasing)."""
    regs = []
    for _ms, reg, _val in events:
        lo = reg & 0xFF
        if lo not in regs:
            regs.append(lo)
    assert len(regs) < delay256 - 1, 'too many registers for the synthetic table'
    table = bytes(regs)
    body = bytearray()
    now = 0
    for ms, reg, val in events:
        if ms < now:
            raise ValueError('events must be non-decreasing in ms')
        body += bytes(_delay_ms(ms - now))
        now = ms
        idx = regs.index(reg & 0xFF)
        code = idx | (0x80 if reg >= 0x100 else 0)
        body += bytes([code, val])
    header = opl_trace.HEADER.pack(b'DBRAWOPL', 2, 0, 1, events[-1][0] if events else 0,
                                   2, 0, 0, delay256, delay_shift8, len(table))
    return header + table + bytes(body)


def ms_for(rel):
    """The capture ms whose round-half-up mapping is relative tick `rel`."""
    return (rel * 1000 + 59) // 120


def synthetic_pair(port_writes, cap_writes, port_anchor=ANCHOR_TICK):
    """A port section and a capture anchored at the same note.

    `port_writes` are (relative tick, reg, val); `cap_writes` likewise. The
    port's key-on is `0xB0=0x2B` at `port_anchor`; the capture's at ms 0.
    """
    port = [('S', port_anchor,
             'cc=00000000 dc=%08X vol=50 bank=%08X inits=1 ticks=0' % (DC, BANK))]
    n = 0
    # The anchor note's preamble then the key-on, as the sequencer writes it.
    for reg, val in ((0x20, 0x01), (0x40, 0x02), (0xB0, 0x2B)):
        port.append(('W', port_anchor, 0, n, reg, val, 0xFF))
        n += 1
    cap = [(0, 0x20, 0x01), (0, 0x40, 0x02), (0, 0xB0, 0x2B)]
    for rel, reg, val in port_writes:
        port.append(('W', port_anchor + rel, rel, n, reg, val, 0))
        n += 1
    for rel, reg, val in cap_writes:
        cap.append((ms_for(rel), reg, val))
    cap.sort(key=lambda e: e[0])
    return port, cap


def write_port(d, rows):
    path = os.path.join(d, 'port.txt')
    with open(path, 'w') as f:
        for row in rows:
            if row[0] == 'S':
                f.write('S %d %s\n' % (row[1], row[2]))
            else:
                f.write('W %d %d %d %04X %02X %02X\n'
                        % (row[1], row[2], row[3], row[4], row[5], row[6]))
    return path


def write_capture(d, events, name='prage_000.dro'):
    path = os.path.join(d, name)
    with open(path, 'wb') as f:
        f.write(make_dro(events))
    return path


def write_port_sections(d, sections):
    """`sections`: [(bank, [(tick, reg, val), ...]), ...]; each section starts
    at its own frame with an S line (unless `metadata` is False) and its writes
    reset the song tick to 60, so the parser sees a new section. Returns the
    path."""
    path = os.path.join(d, 'port.txt')
    n = 0
    with open(path, 'w') as f:
        for si, item in enumerate(sections):
            bank, writes = item[0], item[1]
            metadata = item[2] if len(item) > 2 else True
            dc = item[3] if len(item) > 3 else (0x0A800000 | (si + 1))
            frame = si * 10
            if metadata:
                f.write('S %d cc=00000000 dc=%08X vol=50 bank=%08X inits=%d ticks=0\n'
                        % (frame, dc, bank, si + 1))
            for tick, reg, val in writes:
                f.write('W %d %d %d %04X %02X 00\n'
                        % (frame, tick, n, reg, val))
                n += 1
    return path


def write_capture_sections(d, sections):
    """`sections`: [(anchor_ms, [(rel, reg, val), ...]), ...] -> one `.dro`."""
    events = []
    for anchor_ms, writes in sections:
        for rel, reg, val in writes:
            events.append((anchor_ms + ms_for(rel), reg, val))
    events.sort(key=lambda e: e[0])
    return write_capture(d, events)


def run_tool(args, env=None):
    e = dict(os.environ)
    e.pop('PR_ORACLE_REQUIRED', None)
    e.update(env or {})
    return subprocess.run([sys.executable, TOOL] + args,
                          capture_output=True, text=True, env=e)


def compare(d, port, capture, *extra):
    return run_tool(['compare', '--name', 'synthetic', '--port', port,
                     '--capture', capture, '--game-dir', ''] + list(extra))


def base_case():
    port = [(1, 0x21, 0x03), (2, 0x22, 0x04), (3, 0x23, 0x05),
            (4, 0x24, 0x06), (5, 0x25, 0x07), (6, 0x26, 0x08),
            (7, 0xB1, 0x2B)]
    return port, list(port)


def one_tick_later_pair(drop='none', keep_empty=True):
    """The replayable one-tick-later shapes: the port's A..G at rel 1..7 (with
    a boundary write at rel 8) and the capture's A at rel 1, an empty rel 2,
    then B..G at rel 3..8. `drop='port-b'` is the reviewer's exact case: the
    port's B is deleted and its empty tick retained; `drop='capture-b'` deletes
    the capture's B instead. `keep_empty=False` removes the empty tick from the
    capture by moving C..G one tick earlier."""
    port = [(1, 0x21, 0x01), (2, 0x22, 0x02), (3, 0x23, 0x03),
            (4, 0x24, 0x04), (5, 0x25, 0x05), (6, 0x26, 0x06),
            (7, 0x27, 0x07), (8, 0xB1, 0x0B)]
    cap = [(1, 0x21, 0x01), (3, 0x22, 0x02), (4, 0x23, 0x03),
           (5, 0x24, 0x04), (6, 0x25, 0x05), (7, 0x26, 0x06),
           (8, 0x27, 0x07)]
    if drop == 'port-b':
        port = [w for w in port if w != (2, 0x22, 0x02)]
    elif drop == 'capture-b':
        cap = [w for w in cap if w[1] != 0x22]
    if not keep_empty:
        # remove the empty tick's label by moving C..G one tick earlier: the
        # shape the pre-fix whole-group branch could skip B for.
        cap = [(rel - 1 if rel >= 3 else rel, reg, val) for rel, reg, val in cap]
    return port, cap


SEC_A = [(60, 0x20, 0x01), (60, 0xB0, 0x2B), (61, 0x21, 0x01),
         (62, 0x22, 0x02), (63, 0xB1, 0x0B)]
SEC_B = [(60, 0x20, 0x01), (60, 0xB0, 0x2B), (61, 0x21, 0x02),
         (62, 0x22, 0x02), (63, 0xB1, 0x0B)]
SEC_A5 = [(60, 0x20, 0x01), (60, 0xB0, 0x2B), (61, 0x21, 0x01),
          (62, 0x22, 0x02), (63, 0x23, 0x03), (64, 0x24, 0x04),
          (65, 0xB1, 0x0B)]
CAP_A = [(0, 0x20, 0x01), (0, 0xB0, 0x2B), (1, 0x21, 0x01),
         (2, 0x22, 0x02)]
CAP_B = [(0, 0x20, 0x01), (0, 0xB0, 0x2B), (1, 0x21, 0x02),
         (2, 0x22, 0x02)]


class AudioCompareTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.d = self.tmp.name

    def tearDown(self):
        self.tmp.cleanup()

    def case(self, port_writes, cap_writes, *extra):
        port, cap = synthetic_pair(port_writes, cap_writes)
        p = write_port(self.d, port)
        c = write_capture(self.d, cap)
        return compare(self.d, p, c, *extra)

    def test_identical_pair_passes(self):
        r = self.case(*base_case())
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('synthetic: ok', r.stdout)
        self.assertIn('claimed ticks 0..6', r.stdout)

    def test_one_dropped_write_fails_with_the_first_difference(self):
        # Drop the tick-5 write from the port: the capture still has it, and
        # the port's later writes keep the section's boundary where it was.
        port, cap = base_case()
        port = [w for w in port if w[0] != 5]
        r = self.case(port, cap)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('FIRST DIFFERENCE', r.stdout)
        self.assertIn('port relative tick 5', r.stdout)

    def test_one_extra_write_fails(self):
        port, cap = base_case()
        port = port[:4] + [(5, 0x27, 0x09)] + port[4:]
        r = self.case(port, cap)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('FIRST DIFFERENCE', r.stdout)

    def test_one_wrong_value_fails(self):
        port, cap = base_case()
        cap = [(rel, reg, 0x09 if rel == 5 else val) for rel, reg, val in cap]
        r = self.case(port, cap)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('FIRST DIFFERENCE', r.stdout)

    def test_intra_tick_order_is_the_declared_model(self):
        # Same writes at the same ticks, reversed inside tick 5: the per-tick
        # multiset is equal, so the oracle passes (the declared order model).
        port, _ = base_case()
        port = port + [(5, 0xB2, 0x0A), (5, 0xB4, 0x0B)]
        port.sort(key=lambda w: w[0])
        cap = list(port)
        at5 = [i for i, w in enumerate(cap) if w[0] == 5]
        cap[at5[-1]], cap[at5[-2]] = cap[at5[-2]], cap[at5[-1]]
        r = self.case(port, cap)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('synthetic: ok', r.stdout)

    def test_one_tick_capture_label_is_the_declared_model(self):
        # One write one music tick later in the capture (the measured DRO ms
        # jitter, record §M4): the group alignment moves it.
        port, cap = base_case()
        cap = [(rel + 1 if rel == 5 else rel, reg, val) for rel, reg, val in cap]
        r = self.case(port, cap)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('label slack', r.stdout)

    def test_boundary_tick_is_not_claimed(self):
        # The port's final tick is the request boundary: a write there is not
        # part of the claim (the capture's stop burst can be a tick earlier).
        port, cap = base_case()
        cap = [w for w in cap if w[0] < 7]
        r = self.case(port, cap)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('boundary tick 7', r.stdout)

    def test_unanchored_stream_fails(self):
        # The capture's first key-on is a different register: no anchor.
        port, _cap = base_case()
        cap_events = [(ms_for(0), 0x20, 0x01), (ms_for(0), 0xB1, 0x2B),
                      (ms_for(1), 0x21, 0x03)]
        p = write_port(self.d, synthetic_pair(port, [])[0])
        c = write_capture(self.d, cap_events)
        r = compare(self.d, p, c)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('unanchored', r.stdout)

    def test_align_groups_port_side_dropped_write(self):
        # The reviewer's exact group-level case: the baseline is A..G against
        # A, empty, B..G (ok, claimed 7); the mutant deletes the PORT's B and
        # retains its empty tick. The pre-fix branch1 (`pg == nxt_c` without
        # `not cg`) skipped the capture's non-empty B and returned ok/7; the
        # fixed branch1 requires the skipped group empty and fails.
        A, B, C, D, E, F, G = (('A',), ('B',), ('C',), ('D',), ('E',),
                               ('F',), ('G',))
        port = [[A], [B], [C], [D], [E], [F], [G]]
        cap = [[A], [], [B], [C], [D], [E], [F], [G]]
        r = audio_compare.align_groups(port, cap)
        self.assertTrue(r.ok, r.reason)
        self.assertEqual(r.claimed, 7)
        mutant_port = [[A], [], [C], [D], [E], [F], [G]]
        r = audio_compare.align_groups(mutant_port, cap)
        self.assertFalse(r.ok, 'the port-side dropped B was accepted')
        self.assertNotEqual(r.claimed, 7)

    def test_dropped_write_port_side_one_tick_later_fails(self):
        # The reviewer's exact case through the public comparator: the baseline
        # passes, then the PORT's B is deleted and its empty tick retained.
        # (The pre-fix alignment accepted it; the fixed one fails.)
        port, cap = one_tick_later_pair()
        r = self.case(port, cap)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('synthetic: ok', r.stdout)
        port, cap = one_tick_later_pair(drop='port-b')
        r = self.case(port, cap)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('synthetic: FAIL', r.stdout)
        self.assertNotIn('synthetic: ok', r.stdout)

    def test_dropped_write_capture_side_one_tick_later_fails(self):
        # The capture-side counterpart (not the reviewer's exact case): the
        # capture's B is deleted; the port retains its B and the capture's
        # empty tick. The mutated capture fails the anchor's four-tick prefix
        # check on both the pre-fix and the fixed tool, so this is an
        # anchor-prefix case, not a tick-group (branch) case.
        port, cap = one_tick_later_pair()
        r = self.case(port, cap)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('synthetic: ok', r.stdout)
        port, cap = one_tick_later_pair(drop='capture-b')
        r = self.case(port, cap)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('synthetic: FAIL', r.stdout)
        self.assertNotIn('synthetic: ok', r.stdout)

    def test_dropped_write_capture_side_one_tick_earlier_fails(self):
        # The capture's B is deleted and C..G are one tick earlier. The mutated
        # capture no longer matches the port's first four tick groups, so both
        # the pre-fix and the fixed tool report the section unanchored: an
        # anchor-prefix counterpart, not a whole-group (branch) case.
        port, cap = one_tick_later_pair()
        r = self.case(port, cap)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        port, cap = one_tick_later_pair(drop='capture-b', keep_empty=False)
        r = self.case(port, cap)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('synthetic: FAIL', r.stdout)
        self.assertNotIn('synthetic: ok', r.stdout)

    def test_dropped_write_capture_groups_one_tick_earlier_fails(self):
        # The public-comparator form of Critical 1: the capture drops its A and
        # keeps B/C at rel 1/2, so the whole-group branch could match the
        # capture's current group against the port's next group and skip the
        # port's A. The baseline with A present passes; deleting it must fail
        # (on the fixed tool the anchor's four-tick prefix refuses it, since
        # the prefix uses the same alignment).
        port = [(1, 0x21, 0x01), (2, 0x22, 0x02), (3, 0x23, 0x03),
                (4, 0xB1, 0x0B)]
        base = [(1, 0x21, 0x01), (2, 0x22, 0x02), (3, 0x23, 0x03)]
        r = self.case(port, base)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        mutant = [(1, 0x22, 0x02), (2, 0x23, 0x03)]
        r = self.case(port, mutant)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('synthetic: FAIL', r.stdout)
        self.assertNotIn('synthetic: ok', r.stdout)

    def test_penultimate_tick_difference_fails(self):
        # The last claimed tick (last_rel - 1) is not the unclaimed request
        # boundary; a difference there must fail. A low --min-ticks isolates
        # the first-difference behavior from the short-claim check.
        port, cap = base_case()
        cap = [w for w in cap if w[0] != 6]
        r = self.case(port, cap)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('FIRST DIFFERENCE', r.stdout)
        self.assertIn('port relative tick 6', r.stdout)
        self.assertNotIn('pinned minimum', r.stdout)

    def test_boundary_tick_difference_is_not_claimed(self):
        # Only tick last_rel is the unclaimed boundary: a difference there does
        # not fail, and the claim is the ticks before it.
        port, cap = base_case()
        cap = [w for w in cap if w[0] != 7]
        r = self.case(port, cap, '--min-ticks', '1')
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('boundary tick 7', r.stdout)
        self.assertIn('claimed ticks 0..6', r.stdout)

    def test_partial_capture_loss_fails(self):
        p = write_port(self.d, synthetic_pair([], [])[0])
        c1 = write_capture(self.d, [(ms_for(0), 0xB0, 0x2B)])
        missing = os.path.join(self.d, 'prage_001.dro')
        r = compare(self.d, p, c1, '--capture', missing)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('MISSING capture file', r.stdout)
        self.assertIn('prage_001.dro', r.stdout)

    def test_expected_files_inventory(self):
        r = run_tool(['expected-files', '--scenario', 'gp-pads'])
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout.split(), ['prage_000.dro', 'prage_001.dro'])
        r = run_tool(['expected-files', '--scenario', 'gp-u5-charsel'])
        self.assertEqual(r.stdout.split(), ['prage_000.dro'])

    def test_summary_partial_inventory_fails(self):
        cap = os.path.join(self.d, 'captures')
        os.makedirs(os.path.join(cap, 'gp-pads'))
        with open(os.path.join(cap, 'gp-pads', 'prage_000.dro'), 'wb') as f:
            f.write(make_dro([(0, 0xB0, 0x2B)]))
        rep = os.path.join(self.d, 'reports', 'gp-pads')
        os.makedirs(rep)
        with open(os.path.join(rep, 'report.json'), 'w') as f:
            f.write('{"name":"gp-pads","ok":true,"total_ticks":30,"min_ticks":30,'
                    '"named_gaps":[],"skipped":null}\n')
        r = run_tool(['summary', '--captures', cap, '--reports',
                      os.path.join(self.d, 'reports')])
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('summary FAIL: gp-pads: missing prage_001.dro', r.stdout)

    def test_summary_not_run_and_fail_rows_fail(self):
        cap = os.path.join(self.d, 'captures')
        os.makedirs(os.path.join(cap, 'gp-pads'))
        for name in ('prage_000.dro', 'prage_001.dro'):
            with open(os.path.join(cap, 'gp-pads', name), 'wb') as f:
                f.write(make_dro([(0, 0xB0, 0x2B)]))
        rep = os.path.join(self.d, 'reports')
        r = run_tool(['summary', '--captures', cap, '--reports', rep])
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('summary FAIL: gp-pads: NOT RUN', r.stdout)
        os.makedirs(os.path.join(rep, 'gp-pads'))
        with open(os.path.join(rep, 'gp-pads', 'report.json'), 'w') as f:
            f.write('{"name":"gp-pads","ok":false,"total_ticks":0,"min_ticks":30,'
                    '"named_gaps":[],"skipped":null}\n')
        r = run_tool(['summary', '--captures', cap, '--reports', rep])
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('summary FAIL: gp-pads: FAIL', r.stdout)

    def test_absent_capture_skips_by_name(self):
        p = write_port(self.d, synthetic_pair([], [])[0])
        absent = os.path.join(self.d, 'no-such-dir', 'absent.dro')
        r = compare(self.d, p, absent)
        self.assertEqual(r.returncode, 3, r.stdout + r.stderr)
        self.assertIn('SKIP synthetic: no capture', r.stdout)

    def test_short_claim_fails_the_minimum(self):
        r = self.case(*base_case(), '--min-ticks', '99')
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('pinned minimum 99', r.stdout)

    def test_allow_unanchored_names_the_gap(self):
        port, _cap = base_case()
        cap_events = [(ms_for(0), 0x20, 0x01), (ms_for(0), 0xB1, 0x2B)]
        p = write_port(self.d, synthetic_pair(port, [])[0])
        c = write_capture(self.d, cap_events)
        r = compare(self.d, p, c, '--allow-unanchored', '%08X' % BANK)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('UNANCHORED named gap', r.stdout)

    def test_allow_diff_names_the_gap(self):
        port, cap = base_case()
        cap = [(rel, reg, 0x09 if rel == 5 else val) for rel, reg, val in cap]
        r = self.case(port, cap, '--allow-diff', '%08X' % BANK)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('DIFF ALLOWED named gap', r.stdout)

    def test_diff_allowed_matched_prefix_pin_fails_one_tick_short(self):
        # The capture differs at tick 5, so ticks 0..4 are the measured matched
        # prefix. A minimum of 5 passes; raising that same section's pin to 6
        # must fail rather than letting the named remainder erase the prefix.
        port, cap = base_case()
        cap = [(rel, reg, 0x09 if rel == 5 else val) for rel, reg, val in cap]
        args = ('--allow-diff', '%08X' % BANK)
        r = self.case(port, cap, *args, '--prefix-pin', '0:5')
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('claimed matched prefix 5 ticks', r.stdout)
        r = self.case(port, cap, *args, '--prefix-pin', '0:6')
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('claimed matched prefix 5 ticks, pinned minimum 6', r.stdout)

    def test_anchored_section_shorter_than_prefix_pin_fails(self):
        # A full match does not make a short section stronger than its pin:
        # base_case claims ticks 0..6, so the minimum of 8 must fail.
        r = self.case(*base_case(), '--prefix-pin', '0:8')
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('claimed matched prefix 7 ticks, pinned minimum 8', r.stdout)

    def test_diff_allowed_prefix_difference_at_tick_zero_claims_zero(self):
        # Bypass anchor selection to isolate the already-computed alignment
        # prefix: a capture that differs in its first group has no prefix to
        # claim, not a one-tick claim and not the named section's whole length.
        r = audio_compare.align_groups([[(0x21, 0x03)]], [[(0x22, 0x04)]])
        self.assertFalse(r.ok)
        self.assertEqual(r.first_tick, 0)
        self.assertEqual(r.claimed, 0)

    def test_diff_allowed_section_without_a_prefix_pin_fails(self):
        # Section 0 anchors and differs at tick 4. Section 1 is fully matched,
        # so its pin is satisfied and isolates section 0's missing-pin guard.
        p = write_port_sections(self.d, [(0xAAAA0001, SEC_A5),
                                         (0xBBBB0002, SEC_B)])
        cap_a_diff = [(0, 0x20, 0x01), (0, 0xB0, 0x2B),
                      (1, 0x21, 0x01), (2, 0x22, 0x02), (3, 0x23, 0x03),
                      (4, 0x24, 0x09)]
        c = write_capture_sections(self.d, [(0, cap_a_diff), (1000, CAP_B)])
        r = compare(self.d, p, c, '--allow-diff', 'AAAA0001',
                    '--prefix-pin', '1:1')
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('section 0 bank AAAA0001: diff-allowed matched prefix '
                      'has no per-section pin', r.stdout)
        self.assertNotIn('absent from the port dump', r.stdout)

    def test_pinned_prefix_index_absent_fails(self):
        r = self.case(*base_case(), '--prefix-pin', '99:1')
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('section 99: pinned matched prefix 1 ticks is absent '
                      'from the port dump', r.stdout)

    def test_malformed_prefix_pin_is_a_usage_error(self):
        r = self.case(*base_case(), '--prefix-pin', '1')
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        self.assertIn('usage:', r.stderr)
        self.assertIn('expected INDEX:TICKS', r.stderr)
        self.assertNotIn('Traceback', r.stderr)

    def test_capture_end_reads_the_x_record(self):
        cap = os.path.join(self.d, 'cap')
        os.makedirs(cap)
        with open(os.path.join(cap, 'poll.log'), 'w') as f:
            f.write('P ms=0 f=0000 mode=0000 st=0000 tick=00000000\n')
            f.write('X ms=1 f=04E2 step=1 end\n')
        r = run_tool(['capture-end', '--capture', cap, '--max', '1400'])
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout.strip(), '1250')

    def test_second_bank_carrier_tl_attrs_9_3_are_excluded(self):
        # The measured unexcluded second-bank carrier-TL rows (record
        # claim-coverage §C3): 0x145=0x16 attr 0x09 and attr 0x03 in
        # gp-u23-u16-finisher's s16selmo+0x25C must be excluded. The first
        # bank's carrier-TL stays excluded only on the declared channels 1/4:
        # an attr outside that set still compares.
        for lo in (0x43, 0x44, 0x45, 0x4B, 0x4C, 0x4D, 0x53, 0x54, 0x55):
            self.assertTrue(audio_compare.documented_excluded(lo, 0x01), 'first-bank ch1')
            self.assertTrue(audio_compare.documented_excluded(lo, 0x04), 'first-bank ch4')
            self.assertFalse(audio_compare.documented_excluded(lo, 0x09), 'first-bank ch9 still compares')
            self.assertFalse(audio_compare.documented_excluded(lo, 0x03), 'first-bank ch3 still compares')
            self.assertFalse(audio_compare.documented_excluded(lo, 0xFF), 'first-bank unattributed still compares')
        for lo in (0x143, 0x144, 0x145, 0x14B, 0x14C, 0x14D, 0x153, 0x154, 0x155):
            self.assertTrue(audio_compare.documented_excluded(lo, 0x01), 'second-bank ch1')
            self.assertTrue(audio_compare.documented_excluded(lo, 0x04), 'second-bank ch4')
            self.assertTrue(audio_compare.documented_excluded(lo, 0x09), 'second-bank ch9 (measured 0x145=0x16)')
            self.assertTrue(audio_compare.documented_excluded(lo, 0x03), 'second-bank ch3 (measured 0x145=0x16)')
        # 0xBD stays unconditional in both banks.
        self.assertTrue(audio_compare.documented_excluded(0xBD, 0xFF))
        self.assertTrue(audio_compare.documented_excluded(0x1BD, 0x00))

    def test_no_key_on_section_is_a_named_gap(self):
        # The sample-queue hand-off s16snd2+0x21BFA (record claim-coverage §C3)
        # writes registers but never a key-on; it is nothing-to-claim and is
        # reported by name, not a failure (the pre-fix tool failed it).
        p = write_port_sections(self.d, [(0xAAAA0001,
                                          [(60, 0x40, 0x01), (61, 0x43, 0x02)])])
        c = write_capture_sections(self.d, [(0, [(0, 0xB0, 0x2B)])])
        r = compare(self.d, p, c, '--expect-sections', 'AAAA0001:named')
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('named gap', r.stdout)

    # The rotation anchor (record claim-coverage §C3.2): the port's section
    # first key-on is 0xB1=0x36 (channel 1, cursor 0); the capture's tail note
    # 0xB2=0x2B (channel 2) advances the capture's cursor, so k = 2 - 0 = 2 and
    # the capture's section is the port's shifted +2 channels.
    _ROT_PORT = [(0xAAAA0001,
                  [(60, 0xB1, 0x36), (61, 0xB2, 0x23), (62, 0xB3, 0x27),
                   (63, 0xB4, 0x2E), (64, 0xB5, 0x32), (65, 0xB6, 0x2F)])]
    _ROT_CAP = [(0, [(0, 0xB2, 0x2B)]),
                (100, [(0, 0xB3, 0x36), (1, 0xB4, 0x23), (2, 0xB5, 0x27),
                        (3, 0xB6, 0x2E), (4, 0xB7, 0x32), (5, 0xB8, 0x2F)])]
    _ROT_WRONG = [(0, [(0, 0xB2, 0x2B)]),
                  (100, [(0, 0xB4, 0x36), (1, 0xB5, 0x23), (2, 0xB6, 0x27),
                          (3, 0xB7, 0x2E), (4, 0xB8, 0x32), (5, 0x1B0, 0x2F)])]

    def rotation_case(self, cap_sections, *extra):
        p = write_port_sections(self.d, self._ROT_PORT)
        c = write_capture_sections(self.d, cap_sections)
        return compare(self.d, p, c, *extra)

    def test_rotation_anchor_matches_a_rotated_section(self):
        r = self.rotation_case(self._ROT_CAP)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('synthetic: ok', r.stdout)

    def test_rotation_anchor_rejects_a_wrongly_rotated_copy(self):
        # The capture's section is rotated one channel further (+3, channels
        # 4..9) but its tail note is unchanged, so the derived k stays 2 and the
        # candidate's register (0xB4) is not the port's first key-on rotated by
        # 2 (0xB3): a searched k would accept it, the derived k rejects it.
        r = self.rotation_case(self._ROT_WRONG)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('synthetic: FAIL', r.stdout)
        self.assertNotIn('synthetic: ok', r.stdout)

    def test_pinned_prefix_lost_when_rotation_anchor_is_unanchored_fails(self):
        # The named unanchored allowance must not erase a previously pinned
        # prefix: without the lost-prefix guard this same named gap passes.
        r = self.rotation_case(self._ROT_WRONG,
                               '--allow-unanchored', 'AAAA0001',
                               '--prefix-pin', '0:1')
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('matched-prefix claim lost (unanchored-allowed), '
                      'pinned minimum 1', r.stdout)

    def test_rotation_anchor_rejects_a_shuffled_capture(self):
        # The section's notes are reordered (0x27 and 0x23 swapped): the anchor
        # candidate still matches but the first 4 tick groups differ.
        shuffled = [(0, [(0, 0xB2, 0x2B)]),
                    (100, [(0, 0xB3, 0x36), (1, 0xB5, 0x27), (2, 0xB4, 0x23),
                           (3, 0xB6, 0x2E), (4, 0xB7, 0x32), (5, 0xB8, 0x2F)])]
        r = self.rotation_case(shuffled)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('synthetic: FAIL', r.stdout)
        self.assertNotIn('synthetic: ok', r.stdout)

    def test_rotation_anchor_rejects_a_dropped_write(self):
        # The capture's rel-4 note (0xB7=0x32) is dropped: the anchor's four-tick
        # prefix passes, then the full comparison fails at the drop.
        dropped = [(0, [(0, 0xB2, 0x2B)]),
                   (100, [(0, 0xB3, 0x36), (1, 0xB4, 0x23), (2, 0xB5, 0x27),
                          (3, 0xB6, 0x2E), (5, 0xB8, 0x2F)])]
        r = self.rotation_case(dropped)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('synthetic: FAIL', r.stdout)
        self.assertNotIn('synthetic: ok', r.stdout)


    # ---- final-review regressions (2026-10-26) -------------------------

    def test_unmatched_capture_write_in_claimed_window_fails(self):
        # C1: the port's A is at rel 2, the capture's A is one tick early at
        # rel 1, and the capture has an extra X at rel 2 (the last claimed
        # tick). The extra write must fail; the old tool ignored it.
        port, _ = synthetic_pair([(2, 0x21, 0x01), (3, 0xB1, 0x0B)], [])
        p = write_port(self.d, port)
        c = write_capture(self.d, [(ms_for(0), 0xB0, 0x2B),
                                   (ms_for(1), 0x21, 0x01),
                                   (ms_for(2), 0x22, 0x02)])
        r = compare(self.d, p, c)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('synthetic: FAIL', r.stdout)

    def test_duplicate_port_section_reusing_one_capture_occurrence_fails(self):
        # C2: two identical port sections but only one capture occurrence. The
        # second must not reuse the first's anchor.
        p = write_port_sections(self.d, [(0xAAAA0001, SEC_A),
                                         (0xAAAA0001, SEC_A)])
        c = write_capture_sections(self.d, [(0, CAP_A)])
        r = compare(self.d, p, c)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('synthetic: FAIL', r.stdout)

    def test_repeated_song_with_later_capture_occurrence_passes(self):
        p = write_port_sections(self.d, [(0xAAAA0001, SEC_A),
                                         (0xAAAA0001, SEC_A)])
        c = write_capture_sections(self.d, [(0, CAP_A), (1000, CAP_A)])
        r = compare(self.d, p, c)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('synthetic: ok', r.stdout)

    def test_reordered_port_sections_fail(self):
        # The port's B section is not the capture's A section: the prefix/full
        # comparison rejects the wrong occurrence, then the second port section
        # has no earlier A occurrence left.
        p = write_port_sections(self.d, [(0xBBBB0002, SEC_B),
                                         (0xAAAA0001, SEC_A)])
        c = write_capture_sections(self.d, [(0, CAP_A), (1000, CAP_B)])
        r = compare(self.d, p, c)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('synthetic: FAIL', r.stdout)

    def test_expected_section_inventory_missing_song_fails(self):
        # I1: the whole-song-loss guard. The port has only the first of the two
        # expected sections; the tick minimum alone would pass.
        p = write_port_sections(self.d, [(0xAAAA0001, SEC_A)])
        c = write_capture_sections(self.d, [(0, CAP_A), (1000, CAP_B)])
        r = compare(self.d, p, c, '--expect-sections',
                    'AAAA0001:matched,BBBB0002:matched')
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('expected 2 section(s)', r.stdout)

    def test_expected_section_inventory_present_passes(self):
        p = write_port_sections(self.d, [(0xAAAA0001, SEC_A),
                                         (0xBBBB0002, SEC_B)])
        c = write_capture_sections(self.d, [(0, CAP_A), (1000, CAP_B)])
        r = compare(self.d, p, c, '--expect-sections',
                    'AAAA0001:matched,BBBB0002:matched')
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('synthetic: ok', r.stdout)

    def test_parse_port_new_song_missing_metadata(self):
        # I2: the second section has no S line; it must not borrow the first
        # song's bank. (The old parser used s_lines[-1].)
        p = write_port_sections(self.d, [(0xAAAA0001, SEC_A, True),
                                         (0xBBBB0002, SEC_B, False)])
        sections, _ = audio_compare.parse_port(p)
        self.assertTrue(sections[0].attributed)
        self.assertFalse(sections[1].attributed)
        self.assertEqual(sections[1].dc, 0)
        self.assertEqual(sections[1].bank, 0)

    def test_parse_port_same_song_restart_missing_metadata(self):
        p = write_port_sections(self.d, [(0xAAAA0001, SEC_A, True),
                                         (0xAAAA0001, SEC_A, False, 0x0A800001)])
        sections, _ = audio_compare.parse_port(p)
        self.assertTrue(sections[0].attributed)
        self.assertFalse(sections[1].attributed)
        self.assertEqual(sections[1].bank, 0)

    def test_unattributed_section_fails(self):
        p = write_port_sections(self.d, [(0xAAAA0001, SEC_A, True),
                                         (0xAAAA0001, SEC_A, False, 0x0A800001)])
        c = write_capture_sections(self.d, [(0, CAP_A), (1000, CAP_A)])
        r = compare(self.d, p, c)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('unattributed', r.stdout)

    def test_same_song_restart_with_metadata_is_attributed(self):
        p = write_port_sections(self.d, [(0xAAAA0001, SEC_A, True),
                                         (0xAAAA0001, SEC_A, True, 0x0A800001)])
        sections, _ = audio_compare.parse_port(p)
        self.assertTrue(sections[0].attributed)
        self.assertTrue(sections[1].attributed)
        self.assertEqual(sections[1].dc, 0x0A800001)
        c = write_capture_sections(self.d, [(0, CAP_A), (1000, CAP_A)])
        r = compare(self.d, p, c)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_summary_allows_the_declared_u17_skip_only(self):
        cap = os.path.join(self.d, 'captures')
        os.makedirs(os.path.join(cap, 'gp-u17-device2'))
        with open(os.path.join(cap, 'gp-u17-device2', 'prage_000.dro'), 'wb') as f:
            f.write(make_dro([(0, 0xB0, 0x2B)]))
        rep = os.path.join(self.d, 'reports')
        os.makedirs(os.path.join(rep, 'gp-u17-device2'))
        with open(os.path.join(rep, 'gp-u17-device2', 'report.json'), 'w') as f:
            f.write('{"name":"gp-u17-device2","ok":true,"total_ticks":0,'
                    '"min_ticks":0,"named_gaps":[],"skipped":'
                    '"no frame-keyed port script (wall-paced; record \u00a7H6.9)"}\n')
        r = run_tool(['summary', '--captures', cap, '--reports', rep])
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertNotIn('gp-u17-device2: unexpected skip', r.stdout)
        with open(os.path.join(rep, 'gp-u17-device2', 'report.json'), 'w') as f:
            f.write('{"name":"gp-u17-device2","ok":true,"total_ticks":0,'
                    '"min_ticks":0,"named_gaps":[],"skipped":"something else"}\n')
        r = run_tool(['summary', '--captures', cap, '--reports', rep])
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('unexpected skip', r.stdout)

    # ---- the requested-but-unclaimed bank residue (final-truth §F7.2) ----

    def test_trace_residue_table_is_the_measured_partition(self):
        """The shipped table: 12 claimed + 10 residue + 2 reset-only + 2
        never-requested = 26 distinct banks; every non-zero first-diff tick
        equals its section's pinned matched prefix; the measured 0x05E4/0x08
        split is the claimed 0x08 bank."""
        self.assertEqual(audio_compare.check_trace_residue(), [])
        self.assertEqual(len(audio_compare.TRACE_CLAIMED_BANKS), 12)
        self.assertEqual(len(audio_compare.TRACE_RESIDUE), 10)
        self.assertEqual(len(audio_compare.TRACE_RESET_ONLY), 2)
        self.assertEqual(len(audio_compare.TRACE_NEVER_REQUESTED), 2)
        self.assertEqual(audio_compare.TRACE_CLAIMED_BANKS[0x5A56E385],
                         's16grave.gra+0x08')
        self.assertEqual(audio_compare.TRACE_RESET_ONLY[0x13211DEB][0],
                         's16grave.gra+0x05E4')
        for bank, row in audio_compare.TRACE_RESIDUE.items():
            self.assertEqual(len(row), 5, bank)
            _name, scenario, index, tick, cause = row
            self.assertIn(cause, audio_compare.RESIDUE_CAUSES)
            if tick:
                self.assertEqual(
                    audio_compare.TRACE_PREFIX_PINS[scenario][index], tick,
                    'bank %08X' % bank)

    def test_swapped_claim_and_reset_only_banks_fail(self):
        """The 0x05E4/0x08 split guard: swapping the two measured banks in a
        copy of the tables fails, because the claimed bank's pinned first
        key-on (frame 5472 tick 60) would then belong to a reset-only bank."""
        claimed = dict(audio_compare.TRACE_CLAIMED_BANKS)
        reset = dict(audio_compare.TRACE_RESET_ONLY)
        del claimed[0x5A56E385]
        claimed[0x13211DEB] = reset[0x13211DEB][0]
        del reset[0x13211DEB]
        reset[0x5A56E385] = ('s16grave.gra+0x08', 'gp-u10-ending', 5443, 25, 1)
        problems = audio_compare.check_bank_partition(claimed, reset)
        self.assertTrue(problems)
        self.assertTrue(any('measured key-on but is not claimed' in p
                            or 'measured key-on but is reset-only' in p
                            for p in problems), problems)

    def test_reset_only_bank_forming_a_section_fails(self):
        """The runtime guard with the swapped tables: the ending's 1,200-tick
        section carries 0x5A56E385, which the old table called reset-only."""
        class _S:
            bank = 0x5A56E385
        class _R:
            index = 11
            section = _S()
            status = 'anchored'
            first_diff = None
        old_claimed = dict(audio_compare.TRACE_CLAIMED_BANKS)
        old_reset = dict(audio_compare.TRACE_RESET_ONLY)
        del old_claimed[0x5A56E385]
        old_reset[0x5A56E385] = ('s16grave.gra+0x08', 'gp-u10-ending', 5443, 25, 1)
        problems = audio_compare.bank_partition_violation(
            'gp-u10-ending', [_R()], [], [], old_claimed, old_reset)
        self.assertTrue(problems)
        self.assertTrue(any('reset-only bank formed a section' in p
                            for p in problems), problems)

    def test_trace_residue_moved_one_tick_earlier_fails(self):
        """The mutation: one first-diff tick one earlier no longer equals the
        measured prefix pin, so the region guard fails (the 'move one
        first-diff tick one earlier' mutation of the task)."""
        table = dict(audio_compare.TRACE_RESIDUE)
        bank = 0x8FE546D4                       # conti+0x08, tick 960
        name, scenario, index, tick, cause = table[bank]
        table[bank] = (name, scenario, index, tick - 1, cause)
        problems = audio_compare.check_trace_residue(table)
        self.assertTrue(problems)
        self.assertTrue(any('does not equal the pinned prefix' in p
                            for p in problems), problems)

    def test_trace_residue_moved_first_difference_fails_a_run(self):
        """The runtime guard: a run whose measured section differs one tick
        earlier than the pin fails instead of silently reclassifying."""
        class _S:
            bank = 0x8FE546D4
        class _R:
            index = 9
            section = _S()
            status = 'diff-allowed'
            first_diff = 959
        problems = audio_compare.residue_violation('gp-idle-loss', [_R()])
        self.assertTrue(problems)
        self.assertTrue(any('first difference moved' in p for p in problems),
                        problems)

    def test_trace_residue_unanchored_row_lost_fails(self):
        """A tick-0 row is the measured unanchored bank; if the port's run
        anchors it (a stronger claim), the status no longer matches the pin."""
        class _S:
            bank = 0x20DA829D
        class _R:
            index = 3
            section = _S()
            status = 'anchored'
            first_diff = None
        problems = audio_compare.residue_violation('gp-twop', [_R()])
        self.assertTrue(problems)
        self.assertTrue(any('expected unanchored-allowed' in p for p in problems),
                        problems)

if __name__ == '__main__':
    unittest.main()
