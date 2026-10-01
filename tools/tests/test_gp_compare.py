# tools/tests/test_gp_compare.py
import contextlib, gzip, io, os, shutil, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_compare as gc
import gp_session as gs
import title_compare as tc

DAC = bytes(i for i in range(256) for _ in range(3))   # grey: index i -> (i, i, i)


def ipx(k):
    """A port frame whose every index is k, except row k (index k+1): distinct rows."""
    idx = bytearray([k]) * 64000
    idx[320 * k:320 * (k + 1)] = bytes([k + 1]) * 320
    return bytes(idx) + DAC


def rgb(k):
    return gc.expand_ipx(ipx(k))


class Dirs(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp(prefix='gpcmp-test-')
        self.addCleanup(shutil.rmtree, self.d, True)
        self.cap = os.path.join(self.d, 'cap')
        self.port = os.path.join(self.d, 'port')
        os.makedirs(self.cap)
        os.makedirs(self.port)

    def write(self, port_ks, cap_frames):
        for i, k in enumerate(port_ks):
            with open(os.path.join(self.port, 'frame_%05d.ipx' % i), 'wb') as f:
                f.write(ipx(k))
        for j, c in enumerate(cap_frames):
            with gzip.open(os.path.join(self.cap, 'frame_%05d.raw.gz' % j), 'wb') as f:
                f.write(c)
        with open(os.path.join(self.cap, 'window.txt'), 'w') as f:
            f.write(''.join('%05d %d\n' % (j, 100 + j) for j in range(len(cap_frames))))

    def run_claim(self, n, report=False, max_start=100):
        cap = gc.Lazy(gc._paths(self.cap, 'frame_%05d.raw.gz'), gc.load_capture_frame)
        port = gc.Lazy(gc._paths(self.port, 'frame_%05d.ipx'), gc.load_port_frame)
        rows = [tc.row_hashes(port[m]) for m in range(len(port))]
        out = []
        rc, first, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, n, report, out.append, max_start)
        return rc, first, out


class TestExpand(unittest.TestCase):
    def test_expand_is_dac_of_index(self):
        data = bytearray(ipx(3))
        data[64000 + 3 * 7:64000 + 3 * 7 + 3] = b'\x01\x02\x03'
        data[0] = 7
        out = gc.expand_ipx(bytes(data))
        self.assertEqual(out[0:3], b'\x01\x02\x03')
        self.assertEqual(out[3:6], bytes([3, 3, 3]))
        self.assertEqual(len(out), tc.FRAME_BYTES)

    def test_expand_rejects_a_short_frame(self):
        with self.assertRaises(ValueError):
            gc.expand_ipx(b'\x00' * 100)


class TestFrames(Dirs):
    def test_clean_and_splice_are_explained(self):
        b = 5000
        self.write([1, 2, 3], [rgb(1), rgb(1)[:b] + rgb(2)[b:], rgb(2), rgb(3)])
        rc, first, out = self.run_claim(4)
        self.assertEqual((rc, first), (0, None), out)

    def test_first_unexplained_against_the_ratchet(self):
        bad = bytes([9]) * tc.FRAME_BYTES
        self.write([1, 2, 3], [rgb(1), rgb(2), bad, rgb(3)])
        self.assertEqual(self.run_claim(2)[:2], (0, 2))
        rc, first, out = self.run_claim(3)
        self.assertEqual((rc, first), (1, 2))
        self.assertTrue(any('FAIL: first unexplained 2 < ratchet N 3' in l for l in out), out)

    def test_black_frames_are_skipped(self):
        self.write([1, 2], [rgb(1), bytes(tc.FRAME_BYTES), rgb(2)])
        self.assertEqual(self.run_claim(3)[:2], (0, None))

    def test_window_start_is_the_port_first_frame(self):
        self.write([1, 2], [rgb(5), rgb(6), rgb(1), rgb(2)])
        rc, first, out = self.run_claim(4)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(out[0].startswith('gp_compare: t: frames: window from capture 2 (raw 102)'), out)

    def test_a_match_beyond_the_window_is_found(self):
        ks = list(range(1, 80))
        self.write(ks, [rgb(1), rgb(79)])
        self.assertEqual(self.run_claim(2)[:2], (0, None))

    def test_unpinned_n_fails(self):
        self.write([1], [rgb(1)])
        rc, first, out = self.run_claim(None)
        self.assertEqual((rc, first), (1, None), out)
        self.assertTrue(any('FAIL: the ratchet N is not pinned' in l for l in out), out)

    def test_an_unreachable_n_fails(self):
        self.write([1, 2], [rgb(1), rgb(2)])
        rc, first, out = self.run_claim(3)       # 2 capture frames: N 3 can never be met
        self.assertEqual((rc, first), (1, None), out)
        self.assertTrue(any('FAIL: N 3 > end 2' in l for l in out), out)

    def test_an_improved_first_unexplained_is_said(self):
        bad = bytes([9]) * tc.FRAME_BYTES
        self.write([1, 2], [rgb(1), rgb(2), bad])
        rc, first, out = self.run_claim(1)
        self.assertEqual((rc, first), (0, 2), out)
        self.assertTrue(any('first unexplained 2, ratchet N 1 ok (improved: raise N)' in l for l in out), out)

    def test_report_goes_past_the_first_unexplained(self):
        b1, b2 = bytes([9]) * tc.FRAME_BYTES, bytes([8]) * tc.FRAME_BYTES
        self.write([1, 2], [rgb(1), b1, rgb(2), b2])
        rc, first, out = self.run_claim(0, report=True)
        self.assertEqual(first, 1, out)
        self.assertEqual(sum('UNEXPLAINED capture' in l for l in out), 2, out)
        self.assertTrue(any('FIRST UNEXPLAINED capture 1 (raw 101)' in l for l in out), out)
        rc, first, out = self.run_claim(0, report=False)
        self.assertEqual(sum('UNEXPLAINED capture' in l for l in out), 1, out)   # enforced: stops at the first

    def test_the_window_start_cannot_slide_forward(self):
        # review 1, Important 1: a port that regressed to a screen recurring later in the
        # capture moved the window start past the frame that set N, and went green
        cap = [rgb(9), rgb(2), rgb(3), rgb(7), rgb(1), rgb(2)]
        self.write([9, 2, 3], cap)
        rc, first, out = self.run_claim(3, max_start=0)
        self.assertEqual((rc, first), (0, 3), out)
        self.write([1, 2, 3], cap)                       # regressed port: starts at capture 4
        rc, first, out = self.run_claim(3, max_start=0)
        self.assertEqual(rc, 1, out)
        self.assertTrue(any('window starts at capture 4 (raw 104) > pinned start 0' in l for l in out), out)
        rc, first, out = self.run_claim(3, max_start=10)  # a loose pin: start >= N still fails
        self.assertEqual(rc, 1, out)
        self.assertTrue(any('window start 4 >= ratchet N 3' in l for l in out), out)

    def test_an_earlier_window_start_is_said(self):
        self.write([1, 2], [rgb(1), rgb(2)])
        rc, first, out = self.run_claim(2, max_start=1)
        self.assertEqual(rc, 0, out)
        self.assertTrue(any('window start 0 < pinned start 1 (improved: lower the pin)' in l for l in out), out)

    def test_an_unset_start_pin_fails(self):
        self.write([1, 2], [rgb(1), rgb(2)])
        rc, first, out = self.run_claim(2, max_start=None)
        self.assertEqual(rc, 1, out)
        self.assertTrue(any('window-start pin is not set' in l for l in out), out)

    def test_report_mode_prints_no_verdict_and_checks_no_pin(self):
        bad = bytes([9]) * tc.FRAME_BYTES
        self.write([1, 2], [rgb(1), bad])
        rc, first, out = self.run_claim(None, report=True, max_start=None)
        self.assertEqual(first, 1, out)
        self.assertFalse(any('ratchet N' in l or 'FAIL' in l or 'pinned' in l or 'pin is' in l for l in out), out)

    def test_a_fully_explained_run_names_the_exact_pin(self):
        self.write([1, 2], [rgb(1), rgb(2)])
        rc, first, out = self.run_claim(1)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('every item is explained: N = 2 is the exact pin' in l for l in out), out)
        out2 = self.run_claim(2)[2]
        self.assertFalse(any('exact pin' in l for l in out2), out2)

    def test_coverage_is_reported_for_a_port_frame_the_capture_never_shows(self):
        # review 1, Important 3: a garbage port frame between two good ones explains nothing,
        # and the full-dump search never looks at it; it is reported, not ratcheted
        self.write([1, 99, 2], [rgb(1), rgb(2)])
        rc, first, out = self.run_claim(2)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('1 non-black port frame(s) up to port 2 not exhibited' in l and '[1]' in l
                            for l in out), out)

    def test_named_gap_order_is_not_claimed(self):
        # review 1, Important 3: the claim is order-free (record §I item 9). These pass on
        # purpose; if an order check is ever added, this test must change with the record.
        self.write([1, 99, 2], [rgb(1), rgb(2)])
        self.assertEqual(self.run_claim(2)[:2], (0, None))        # an inserted, wrong port frame
        self.write([1, 2, 3], [rgb(1), rgb(3), rgb(2), rgb(1)])
        self.assertEqual(self.run_claim(4)[:2], (0, None))        # a capture that goes back in time


def _t(f, **kw):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, **kw)
    return 'T ' + gs.format_s(0, vals, 0, 0, 0)[2:]


def _S(f, **kw):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, **kw)
    return gs.format_s(0, vals, 0, 0, 0)


class TestTrace(unittest.TestCase):
    def test_first_difference_skips_unsnapshotted_frames(self):
        port = [_t(f, rng=f) for f in range(10, 20)]
        cap = [_S(f, rng=f if f != 17 else 0) for f in range(10, 20) if f != 12]
        out = []
        rc, first = gc.trace_claim('t', cap, port, 17, out.append)
        self.assertEqual((rc, first), (0, 17), out)
        self.assertIn('1 without a capture snapshot', out[0])
        rc, _ = gc.trace_claim('t', cap, port, 18, out.append)
        self.assertEqual(rc, 1)
        self.assertTrue(any('FAIL: first differing 17 < ratchet N 18' in l for l in out), out)

    def test_tick_is_reported_not_ratcheted(self):
        port = [_t(f, tick=f) for f in range(3)]
        cap = [_S(f, tick=0) for f in range(3)]
        out = []
        rc, first = gc.trace_claim('t', cap, port, 3, out.append)
        self.assertEqual((rc, first), (0, None), out)
        self.assertIn('first tick difference f=1', out[0])


    def test_normalised_fields_are_converted_not_compared_raw(self):
        # the record's conversions (section H items 1-2): the port reads t508 one tick
        # later, and ent is a pointer in two address spaces
        base = 'B ms=0 base=00266000 ptr=0000FE20'
        cap = [base] + [_S(f, t508=f, ent=0x2A2BEC) for f in range(3)]
        port = [_t(f, t508=f + 1, ent=0xBCBEC) for f in range(3)]
        out = []
        rc, first = gc.trace_claim('t', cap, port, 3, out.append)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('t508 0 of 3 differ' in l and 'ent 0 of 3 differ' in l for l in out), out)
        # a raw comparison would differ at every frame; a wrong conversion must be shown
        port[1] = _t(1, t508=1, ent=0xBCBEC)            # t508 not one tick later at f=1
        port[2] = _t(2, t508=3, ent=0x2A2BEC)           # ent left in the capture's address space
        out = []
        rc, first = gc.trace_claim('t', cap, port, 3, out.append)
        self.assertEqual((rc, first), (0, None), out)   # reported, never ratcheted
        self.assertTrue(any('ent 1 of 3 differ (first f=2)' in l and 't508 1 of 3 differ (first f=1)' in l
                            for l in out), out)

    def test_report_mode_prints_no_ratchet_verdict(self):
        port = [_t(f, rng=f) for f in range(3)]
        cap = [_S(f, rng=f if f != 2 else 7) for f in range(3)]
        out = []
        rc, first = gc.trace_claim('t', cap, port, None, out.append, True)
        self.assertEqual((rc, first), (0, 2), out)
        self.assertFalse(any('ratchet N' in l or 'FAIL' in l for l in out), out)
        self.assertTrue(any('first difference f=2' in l for l in out), out)

    def test_nothing_compared_fails(self):
        out = []
        rc, first = gc.trace_claim('t', [_S(f) for f in range(3)], [_t(f) for f in range(10, 13)], 0, out.append)
        self.assertEqual((rc, first), (1, None), out)
        self.assertTrue(any('nothing compared' in l for l in out), out)


class TestCli(Dirs):
    def cli(self, *args):
        buf = io.StringIO()
        old = sys.argv
        sys.argv = ['gp_compare.py'] + list(args)
        try:
            with contextlib.redirect_stdout(buf):
                rc = gc.main()
        finally:
            sys.argv = old
        return rc, buf.getvalue()

    def dump(self, n_frames=2, with_log=True):
        self.write(list(range(1, n_frames + 1)), [rgb(k) for k in range(1, n_frames + 1)])
        with open(os.path.join(self.port, 'trace.txt'), 'w') as f:
            f.write(''.join(_t(k) + '\n' for k in range(10, 13)))
        if with_log:
            with open(os.path.join(self.cap, 'poll.log'), 'w') as f:
                f.write(''.join(_S(k, t508=k - 1) + '\n' for k in range(10, 13)))

    def test_an_absent_capture_skips(self):
        rc, out = self.cli('--scenario', 'gp-x', '--capture', os.path.join(self.d, 'none'), '--port', self.port)
        self.assertEqual(rc, 0)
        self.assertIn('no capture at', out)
        self.assertIn('(skipped)', out)

    def test_a_present_capture_needs_pinned_values(self):
        self.dump()
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port)
        self.assertEqual(rc, 1, out)
        self.assertEqual(out.count('FAIL: the ratchet N is not pinned'), 2, out)
        self.assertIn('FAIL: the window-start pin is not set', out)
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                           '--min-first', '2', '--trace-min-first', '13')
        self.assertEqual(rc, 1, out)                     # the start pin alone is still unset
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                           '--min-first', '2', '--trace-min-first', '13', '--max-start', '0')
        self.assertEqual(rc, 0, out)
        self.assertIn('0 differing through 12; ratchet N 13 ok', out)

    def test_the_capture_identity_pin(self):
        # review 1, Important 3: the pins belong to one capture; a different poll.log (a re-capture)
        # must fail, not skip, and say the pins are invalidated
        self.dump()
        import hashlib
        good = hashlib.sha256(open(os.path.join(self.cap, 'poll.log'), 'rb').read()).hexdigest()
        base = ['--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                '--min-first', '2', '--trace-min-first', '13', '--max-start', '0']
        rc, out = self.cli(*base, '--capture-sha256', good, '--capture-frames', '2')
        self.assertEqual(rc, 0, out)
        self.assertIn('matches the pin', out)
        rc, out = self.cli(*base, '--capture-sha256', '0' * 64, '--capture-frames', '2')
        self.assertEqual(rc, 1, out)
        self.assertIn('a re-capture invalidates the pinned N, F and window start', out)
        rc, out = self.cli(*base, '--capture-sha256', good, '--capture-frames', '3')
        self.assertEqual(rc, 1, out)                          # the frame count differs
        self.assertIn('FAIL: poll.log sha256', out)
        rc, out = self.cli(*base, '--capture-sha256', '')
        self.assertEqual(rc, 1, out)
        self.assertIn('capture identity (poll.log sha256) is not pinned', out)
        # changed poll.log content: the same pin no longer matches
        with open(os.path.join(self.cap, 'poll.log'), 'a') as f:
            f.write(_S(99) + '\n')
        rc, out = self.cli(*base, '--capture-sha256', good, '--capture-frames', '2')
        self.assertEqual(rc, 1, out)
        # report mode never checks the identity
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port, '--report',
                           '--capture-sha256', good)
        self.assertEqual(rc, 0, out)
        self.assertNotIn('re-capture', out)

    def test_a_capture_without_a_poll_log_fails(self):
        self.dump(with_log=False)
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                           '--min-first', '2', '--trace-min-first', '13', '--max-start', '0')
        self.assertEqual(rc, 1, out)
        self.assertIn('has no poll.log', out)

    def test_report_mode_exits_zero_on_a_failing_claim(self):
        self.dump()
        os.remove(os.path.join(self.cap, 'frame_00001.raw.gz'))
        with gzip.open(os.path.join(self.cap, 'frame_00001.raw.gz'), 'wb') as f:
            f.write(bytes([9]) * tc.FRAME_BYTES)
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                           '--min-first', '2', '--trace-min-first', '13', '--max-start', '0')
        self.assertEqual(rc, 1, out)
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port, '--report')
        self.assertEqual(rc, 0, out)
        self.assertIn('FIRST UNEXPLAINED capture 1', out)
        self.assertNotIn('ratchet N', out)

    def test_report_mode_exits_zero_when_the_claims_cannot_run(self):
        # review 1, Important 2: "window empty" and "nothing compared" return 1 from the
        # claims; make gp-report must still exit 0 (main's report guard is load-bearing)
        self.write([1, 2], [rgb(5), rgb(6)])                     # no capture frame shows port 0
        with open(os.path.join(self.port, 'trace.txt'), 'w') as f:
            f.write(''.join(_t(k) + '\n' for k in range(10, 13)))
        with open(os.path.join(self.cap, 'poll.log'), 'w') as f:
            f.write(''.join(_S(k) + '\n' for k in range(50, 53)))   # no f in common: nothing compared
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port, '--report')
        self.assertEqual(rc, 0, out)
        self.assertIn('window empty', out)
        self.assertIn('nothing compared', out)
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                           '--min-first', '1', '--trace-min-first', '1', '--max-start', '0')
        self.assertEqual(rc, 1, out)


if __name__ == '__main__':
    unittest.main()
