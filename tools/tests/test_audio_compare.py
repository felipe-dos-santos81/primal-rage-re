# tools/tests/test_audio_compare.py
"""The register-stream oracle's unit tests (record 2026-10-25-music-truth §M4).

Each case builds a synthetic `.dro` (the DOSBox-X v2 container `opl_trace.py`
parses) and a synthetic `PR_AUDIO_DUMP` port stream, then runs the tool's CLI.
The cases are the oracle's contract: an identical pair passes; a dropped, extra
or wrong write fails at its first tick group; an unanchored stream fails; a
capture that is absent is skipped by name; the declared intra-tick order model,
the declared one-tick capture label model and the unclaimed request-boundary
tick do not fail.
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
    port = [('S', 0,
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

    def test_absent_capture_skips_by_name(self):
        p = write_port(self.d, synthetic_pair([], [])[0])
        r = compare(self.d, p, os.path.join(self.d, 'absent.dro'))
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

    def test_capture_end_reads_the_x_record(self):
        cap = os.path.join(self.d, 'cap')
        os.makedirs(cap)
        with open(os.path.join(cap, 'poll.log'), 'w') as f:
            f.write('P ms=0 f=0000 mode=0000 st=0000 tick=00000000\n')
            f.write('X ms=1 f=04E2 step=1 end\n')
        r = run_tool(['capture-end', '--capture', cap, '--max', '1400'])
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout.strip(), '1250')


if __name__ == '__main__':
    unittest.main()
