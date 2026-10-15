# tools/tests/test_gp_compare.py
import contextlib, gzip, hashlib, io, os, shutil, sys, tempfile, unittest

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

    def test_the_moves_claim_compares_the_move_fields_only(self):
        # record gameplay-u6 §U6.11: rng differs at f=2 (the trace's business), r0 at f=5
        port = [_t(f, rng=f, r0=0xFF if f < 5 else 0x20) for f in range(10)]
        cap = [_S(f, rng=f if f != 2 else 9, r0=0xFF if f < 6 else 0x20) for f in range(10)]
        out = []
        rc, first = gc.trace_claim('t', cap, port, 5, out.append, False, gs.MOVE_FIELDS, 'moves')
        self.assertEqual((rc, first), (0, 5), out)
        self.assertTrue(any('moves: first difference f=5 (5) in r0' in l for l in out), out)
        self.assertFalse(any('tick' in l or 'normalised' in l for l in out), out)
        rc, _ = gc.trace_claim('t', cap, port, 6, out.append, False, gs.MOVE_FIELDS, 'moves')
        self.assertEqual(rc, 1)
        self.assertTrue(any('moves: FAIL: first differing 5 < ratchet N 6' in l for l in out), out)
        rc, first = gc.trace_claim('t', cap, port, 2, out.append)
        self.assertEqual((rc, first), (0, 2))                 # the trace claim is unchanged

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

    def test_the_moves_claim_runs_when_asked_or_reported_with_its_fields(self):
        self.dump()                                   # a poll.log with the U6 fields (SNAP_FIELDS)
        rc, out = self.cli('--report', '--scenario', 'gp-x', '--capture', self.cap, '--port', self.port)
        self.assertIn('moves: 3 frames compared', out)
        with open(os.path.join(self.cap, 'poll.log'), 'w') as f:      # an older capture
            f.write(''.join(' '.join(p for p in _S(k, t508=k - 1).split()
                                     if p.split('=')[0] not in gs.MOVE_FIELDS) + '\n' for k in range(10, 13)))
        rc, out = self.cli('--report', '--scenario', 'gp-x', '--capture', self.cap, '--port', self.port)
        self.assertEqual(rc, 0)
        self.assertNotIn('moves:', out)

    def test_the_moves_pin_on_a_capture_without_the_fields_fails_cleanly(self):
        self.dump()
        with open(os.path.join(self.cap, 'poll.log'), 'w') as f:
            f.write(''.join(' '.join(p for p in _S(k, t508=k - 1).split()
                                     if p.split('=')[0] not in gs.MOVE_FIELDS) + '\n' for k in range(10, 13)))
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                           '--min-first', '2', '--trace-min-first', '13', '--max-start', '0', '--moves-min-first', '13')
        self.assertEqual(rc, 1, out)
        self.assertIn('moves: FAIL: the capture records no move fields', out)

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


class BandAllowance(Dirs):
    # Track G task 5 (record §G5): the per-frame named allowance, the pattern
    # title_compare.ATTRACT2_SPLICE3_ALLOWED (3545) sets. A named capture frame is
    # explained ONLY if its band bytes equal the pinned sha256 AND exactly one
    # adjacent port pair (P, P+1) in the CURRENT dump byte-binds the rows outside
    # the band. Review Focus #1: a mutation of a bound port frame's dump bytes
    # un-explains the capture frame again; the allowance is per-frame per-bytes,
    # never per-scene.
    #
    # The fixture: port [1, 2, 3] (ipx(k): flat k except row k = k+1); the allowed
    # synthetic frame (capture 1) is port0's rows 0..80, an 11-row band of value 7
    # (a third state: no port frame holds it, no splice point produces it, and 11
    # rows are more than the transition model's one), and port1's rows 92..199.
    # Capture 0 is rgb(1) clean (the window start), captures 2 and 3 rgb(2), rgb(3).
    def setUp(self):
        super().setUp()
        R = tc.ROW
        self.c = rgb(1)[:81 * R] + bytes(bytearray(b'\x07') * (11 * R)) + rgb(2)[92 * R:]
        self.sha = hashlib.sha256(self.c[81 * R:92 * R]).hexdigest()
        self.bands = ((1, 81, 92, self.sha),)

    def write_fixture(self):
        self.write([1, 2, 3], [rgb(1), self.c, rgb(2), rgb(3)])

    def run_band(self, n=4, **kw):
        cap = gc.Lazy(gc._paths(self.cap, 'frame_%05d.raw.gz'), gc.load_capture_frame)
        port = gc.Lazy(gc._paths(self.port, 'frame_%05d.ipx'), gc.load_port_frame)
        rows = [tc.row_hashes(port[m]) for m in range(len(port))]
        out = []
        rc, first, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, n,
                                      kw.get('report', False), out.append,
                                      kw.get('max_start', 0), self.bands)
        return rc, first, out

    def test_the_named_bytes_are_explained(self):
        self.write_fixture()
        rc, first, out = self.run_band()
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('allowed by name as a band splice: port 0/1' in l for l in out), out)
        self.assertTrue(any('1 band-allowed' in l for l in out), out)

    def test_a_mutated_bound_port_frame_unexplains_it_again(self):
        # Review Focus #1: mutate the dump's bottom-bind frame (port 1: 2 -> 5)
        # without touching the capture. The unique-pair bind fails, the allowance
        # FAILs, the frame is unexplained again, and the ratchet catches it.
        self.write_fixture()
        self.assertEqual(self.run_band()[0], 0)
        with open(os.path.join(self.port, 'frame_00001.ipx'), 'wb') as f:
            f.write(ipx(5))
        rc, first, out = self.run_band()
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in l and 'no unique adjacent port pair' in l
                            for l in out), out)
        self.assertTrue(any('FAIL: first unexplained 1 < ratchet N 4' in l for l in out), out)

    def test_different_band_bytes_are_not_the_named_frame(self):
        # Review Focus #1 (the capture side): the same top/bottom with a different
        # band (value 8) is a DIFFERENT byte layout and must not be silenced.
        R = tc.ROW
        self.write_fixture()
        with gzip.open(os.path.join(self.cap, 'frame_00001.raw.gz'), 'wb') as f:
            f.write(rgb(1)[:81 * R] + bytes(bytearray(b'\x08') * (11 * R)) + rgb(2)[92 * R:])
        rc, first, out = self.run_band()
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in l and 'not the named' in l for l in out), out)

    def test_ambiguous_pairs_are_not_the_named_frame(self):
        # never a class: two adjacent pairs bind the same rows (port [1,2,1,2]) ->
        # no unique bind -> the named frame is not explained.
        self.write([1, 2, 1, 2], [rgb(1), self.c])
        rc, first, out = self.run_band(3)
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('no unique adjacent port pair' in l for l in out), out)

    def test_a_named_frame_the_model_explains_is_a_stale_name(self):
        self.write_fixture()
        with gzip.open(os.path.join(self.cap, 'frame_00001.raw.gz'), 'wb') as f:
            f.write(rgb(2))                          # now clean: the name is stale
        rc, first, out = self.run_band()
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('retire the name' in l for l in out), out)

    def test_the_table_names_the_measured_gp_keys_fight_frame(self):
        # the pinned recipe of record §G5: capture 835 (raw 3934), rows 0..80 to
        # port f=0x832 / 92..199 to f=0x833, band sha d9c0ac62.. (§K.12's finding
        # re-measured at 5f874678). A changed name is a changed claim: it must move
        # with a re-measurement in the record, never silently.
        self.assertEqual(gc.BANDS_ALLOWED['gp-keys-fight'],
                         ((835, 81, 92,
                           'd9c0ac62f7e0c0d4082fba8d195eae4f767c56a7887bdf7346450511f998cad8'),))


class Scan3Allowance(Dirs):
    # Track G task 6 (record §G6): the named three-frame scan-out allowance, the
    # BANDS_ALLOWED pattern (§G5) widened to the mode-8 entry's catch-up shape:
    # rows [0,r1) of the still-held old screen (pinned by hash, and at least one
    # EARLIER dump frame must hold them), rows [r1,r2) == port[B] and rows
    # [r2,200) == port[B+1] for EXACTLY ONE adjacent pair (vacuous middle when
    # r1 == r2 — the U9 shape). Review Focus #1: a mutation of any bound dump
    # frame un-explains the capture frame again; per-frame per-bytes, never
    # per-scene, never a third-frame splice in the model itself.
    #
    # The fixture: port [1, 2, 3] (ipx(k): flat k except row k = k+1); the named
    # frame (capture 1) is rgb(1)'s rows 0..1, rgb(2)'s rows 2..3 and rgb(3)'s
    # rows 4..199 — three segments, no adjacent pair splices them, so the
    # two-frame model leaves it unexplained and only the recipe can answer.
    def setUp(self):
        super().setUp()
        R = tc.ROW
        self.c = rgb(1)[:2 * R] + rgb(2)[2 * R:4 * R] + rgb(3)[4 * R:]
        self.sha = hashlib.sha256(rgb(1)[:2 * R]).hexdigest()
        self.entry = (1, 2, 4, self.sha)
        self.s3 = (self.entry,)

    def write_fixture(self):
        self.write([1, 2, 3], [rgb(1), self.c, rgb(2), rgb(3)])

    def run_scan3(self, n=4, **kw):
        cap = gc.Lazy(gc._paths(self.cap, 'frame_%05d.raw.gz'), gc.load_capture_frame)
        port = gc.Lazy(gc._paths(self.port, 'frame_%05d.ipx'), gc.load_port_frame)
        rows = [tc.row_hashes(port[m]) for m in range(len(port))]
        out = []
        rc, first, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, n,
                                      kw.get('report', False), out.append,
                                      kw.get('max_start', 0), None, self.s3)
        return rc, first, out

    def test_the_named_scan_out_is_explained(self):
        self.write_fixture()
        rc, first, out = self.run_scan3()
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('allowed by name as a three-frame scan-out: top rows 0..1 '
                            'held by port 0, port 1 rows 2..3, port 2 rows 4..199' in l
                            for l in out), out)
        self.assertTrue(any('1 scan3-allowed' in l for l in out), out)

    def test_a_mutated_bound_pair_frame_unexplains_it_again(self):
        # Review Focus #1: mutate the dump's middle-bind frame (port 1: 2 -> 5)
        # without touching the capture. The unique-pair bind fails, the allowance
        # FAILs, the frame is unexplained again and the ratchet catches it.
        self.write_fixture()
        self.assertEqual(self.run_scan3()[0], 0)
        with open(os.path.join(self.port, 'frame_00001.ipx'), 'wb') as f:
            f.write(ipx(5))
        rc, first, out = self.run_scan3()
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in l and 'no unique adjacent port pair' in l
                            for l in out), out)
        self.assertTrue(any('FAIL: first unexplained 1 < ratchet N 4' in l for l in out), out)

    def test_a_mutated_top_holder_is_named(self):
        # The top rows are bound by existence, not uniqueness: mutate every
        # earlier holder's top rows (port 0 rows 0..1 to value 9) and the recipe
        # refuses — no frame before B holds the named top.
        R = tc.ROW
        self.write_fixture()
        mutated = bytearray(rgb(1))
        mutated[:2 * R] = bytes([9]) * (2 * R)
        ex, why = gc.scan3_explain(self.c, [bytes(mutated), rgb(2), rgb(3)], self.entry)
        self.assertIsNone(ex)
        self.assertIn('no port frame before 1 holds its named top rows', why)

    def test_different_top_bytes_are_not_the_named_frame(self):
        # Review Focus #1 (the capture side): the same mid/bottom with a
        # different top (value 8) is a DIFFERENT byte layout and must not be
        # silenced (the hash pins the named top rows).
        R = tc.ROW
        self.write_fixture()
        with gzip.open(os.path.join(self.cap, 'frame_00001.raw.gz'), 'wb') as f:
            f.write(bytes([8]) * (2 * R) + rgb(2)[2 * R:4 * R] + rgb(3)[4 * R:])
        rc, first, out = self.run_scan3()
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in l and 'not the named' in l for l in out), out)

    def test_ambiguous_pairs_are_not_the_named_frame(self):
        # never a class: two adjacent pairs bind the same rows (port [1,2,3,2,3])
        # -> no unique bind -> the named frame is not explained.
        self.write([1, 2, 3, 2, 3], [rgb(1), self.c])
        rc, first, out = self.run_scan3(3)
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('no unique adjacent port pair' in l for l in out), out)

    def test_the_vacuous_middle_shape_is_explained(self):
        # the U9 two-segment shape (r1 == r2): top rows 0..4 of rgb(1) plus the
        # bottom rows 5..199 of rgb(3); the pair (2, 3) binds the bottom alone
        # and frame 2 (no pixel claimed) is NOT exhibited.
        R = tc.ROW
        c = rgb(1)[:5 * R] + rgb(3)[5 * R:]
        s3 = ((1, 5, 5, hashlib.sha256(rgb(1)[:5 * R]).hexdigest()),)
        self.write([1, 2, 3], [rgb(1), c, rgb(2), rgb(3)])
        cap = gc.Lazy(gc._paths(self.cap, 'frame_%05d.raw.gz'), gc.load_capture_frame)
        port = gc.Lazy(gc._paths(self.port, 'frame_%05d.ipx'), gc.load_port_frame)
        rows = [tc.row_hashes(port[m]) for m in range(len(port))]
        out = []
        rc, first, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, 4,
                                      False, out.append, 0, None, s3)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('three-frame scan-out: top rows 0..4 held by port 0, '
                            'port 2 rows 5..199 the named bytes' in l for l in out), out)

    def test_a_named_frame_the_model_explains_is_a_stale_name(self):
        self.write_fixture()
        with gzip.open(os.path.join(self.cap, 'frame_00001.raw.gz'), 'wb') as f:
            f.write(rgb(2))                          # now clean: the name is stale
        rc, first, out = self.run_scan3()
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('retire the name' in l for l in out), out)

    def test_the_table_names_the_measured_scanouts(self):
        # the pinned recipes of record §G6: gp-idle-loss 2064 (ports 1666/1669/
        # 1670), gp-u9-win 346 (218/221, vacuous middle), gp-u10-ending 331
        # (216/218/219). A changed name is a changed claim: it must move with a
        # re-measurement in the record, never silently.
        self.assertEqual(gc.SCAN3_ALLOWED['gp-idle-loss'],
                         ((2064, 3, 98,
                           '7bdf2c561c38a264337bc74abd093172ac99ceebb8c818635e189530cf1c58dc'),))
        self.assertEqual(gc.SCAN3_ALLOWED['gp-u9-win'],
                         ((346, 5, 5,
                           '703828af123edf1a8c3bade8d42bdd56a68576cd87e13de7339bdd203dc8f0b6'),))
        self.assertEqual(gc.SCAN3_ALLOWED['gp-u10-ending'],
                         ((331, 2, 64,
                           '2d6fb6ad829baae61c65a844fb44f05cb51393322afdce4ab8117960e1306ddd'),))


class LoaderAllowance(Dirs):
    # Track G task 7 (record §G7): the named loader-frame allowance for the lazy
    # INDEX read's `- LOADING -` blit (0x1B3AC's draw=1 path), which a 70.09 Hz
    # scan-out can catch over whatever screen the aperture held. The port's
    # per-read model collapses the original's disk-block interleaving (§45-A), so
    # its dump holds the base screen and the overlay at their own draw times, not
    # their composition. The recipe: a pinned region hash, ONE unique base port
    # frame equal to the capture outside the region, and at least one port frame
    # holding every pixel where the capture differs from the base. Review Focus
    # #3: a different byte layout fails (the region hash, the base bind and the
    # overlay existence each catch their own mutation).
    #
    # The fixture: port [4, 1, 2, 3] (ipx(k): flat k except row k = k+1); the
    # named frame (capture 1) is rgb(1)'s bytes outside rows 2..4 and rgb(2)'s
    # pixels inside rows 2..4, x 0..3 — no adjacent pair splices it (three
    # differing rows), so only the recipe can answer. Capture 0 is rgb(4) (the
    # window start), so mutating the base frame (port 1, not port 0) cannot
    # empty the window.
    def setUp(self):
        super().setUp()
        R = tc.ROW
        c = bytearray(rgb(1))
        t = rgb(2)
        for r in range(2, 5):
            c[r * R:r * R + 3 * 4] = t[r * R:r * R + 3 * 4]
        self.c = bytes(c)
        self.sha = gc.loader_region_sha(self.c, 2, 5, 0, 4)
        self.entry = (1, 2, 5, 0, 4, self.sha)
        self.loader = (self.entry,)

    def write_fixture(self):
        self.write([4, 1, 2, 3], [rgb(4), self.c, rgb(2), rgb(3)])

    def run_loader(self, n=4, **kw):
        cap = gc.Lazy(gc._paths(self.cap, 'frame_%05d.raw.gz'), gc.load_capture_frame)
        port = gc.Lazy(gc._paths(self.port, 'frame_%05d.ipx'), gc.load_port_frame)
        rows = [tc.row_hashes(port[m]) for m in range(len(port))]
        out = []
        rc, first, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, n,
                                      kw.get('report', False), out.append,
                                      kw.get('max_start', 0), None, None, self.loader)
        return rc, first, out

    def test_the_named_bytes_are_explained(self):
        self.write_fixture()
        rc, first, out = self.run_loader()
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('allowed by name as the loader frame: port 1 base rows outside '
                            '2..4, the overlay rows 2..4 x 0..3 held by port 2' in l for l in out), out)
        self.assertTrue(any('1 loader-allowed' in l for l in out), out)

    def test_a_mutated_overlay_holder_unexplains_it_again(self):
        # Review Focus #1: mutate the dump frame holding the overlay (port 2:
        # 2 -> 5) without touching the capture. No frame holds the 12 overlay
        # pixels, the allowance FAILs and the ratchet catches the frame.
        self.write_fixture()
        self.assertEqual(self.run_loader()[0], 0)
        with open(os.path.join(self.port, 'frame_00002.ipx'), 'wb') as f:
            f.write(ipx(5))
        rc, first, out = self.run_loader()
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in l and 'no port frame holds its 12 overlay pixel(s)'
                            in l for l in out), out)
        self.assertTrue(any('FAIL: first unexplained 1 < ratchet N 4' in l for l in out), out)

    def test_a_mutated_base_unexplains_it_again(self):
        # Review Focus #1: mutate the dump's base frame (port 1: 1 -> 9). The
        # capture no longer equals exactly one port frame outside the region.
        self.write_fixture()
        with open(os.path.join(self.port, 'frame_00001.ipx'), 'wb') as f:
            f.write(ipx(9))
        rc, first, out = self.run_loader()
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in l
                            and '0 port frames hold its rows outside 2..4, not exactly one' in l
                            for l in out), out)

    def test_different_region_bytes_are_not_the_named_frame(self):
        # Review Focus #3 (the capture side): the same base with a different
        # overlay (rgb(3)'s pixels) is a DIFFERENT byte layout and must not be
        # silenced — the pinned region hash refuses it.
        self.write_fixture()
        R = tc.ROW
        t = rgb(3)
        d = bytearray(self.c)
        for r in range(2, 5):
            d[r * R:r * R + 3 * 4] = t[r * R:r * R + 3 * 4]
        with gzip.open(os.path.join(self.cap, 'frame_00001.raw.gz'), 'wb') as f:
            f.write(bytes(d))
        rc, first, out = self.run_loader()
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in l and 'not the named' in l for l in out), out)

    def test_an_ambiguous_base_is_not_the_named_frame(self):
        # never a class: two port frames hold the bytes outside the region
        # (port [4, 1, 1, 2, 3]) -> no unique base -> the named frame is not explained.
        self.write([4, 1, 1, 2, 3], [rgb(4), self.c])
        rc, first, out = self.run_loader(3)
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('2 port frames hold its rows outside 2..4, not exactly one' in l
                            for l in out), out)

    def test_the_overlay_may_be_held_by_more_than_one_frame(self):
        # the U10 shape: the port dumps every image of an iteration, so two
        # frames can hold the overlay. The recipe binds it by existence, so a
        # single holder's mutation is not felt (record §G7's named limit); the
        # ≥1 holder requirement is what the mutated-holder test proves.
        self.write([4, 1, 2, 2, 3], [rgb(4), self.c, rgb(2), rgb(3)])
        rc, first, out = self.run_loader()
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('the overlay rows 2..4 x 0..3 held by port 2 3' in l for l in out), out)

    def test_a_named_frame_the_model_explains_is_a_stale_name(self):
        self.write_fixture()
        with gzip.open(os.path.join(self.cap, 'frame_00001.raw.gz'), 'wb') as f:
            f.write(rgb(1))                          # now clean: the name is stale
        rc, first, out = self.run_loader()
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('retire the name' in l for l in out), out)

    def test_the_table_names_the_measured_loader_frames(self):
        # the pinned recipes of record §G7: gp-u9-win 2209 (base port 1777,
        # overlay port 1805) and gp-u10-ending 2062 (base port 1686, overlay
        # ports 1688/1689), both at rows 192..197 x 0..85, region sha
        # f2089c52.. (the same 166-px loader text; the six full rows' sha
        # a886c0f3..). A changed name is a changed claim: it must move with a
        # re-measurement in the record, never silently.
        self.assertEqual(gc.LOADER_ALLOWED['gp-u9-win'],
                         ((2209, 192, 198, 0, 86,
                           'f2089c52c182e444f992971ea91cd21df68f0c5807344d1b2f00d164f062eb03'),))
        self.assertEqual(gc.LOADER_ALLOWED['gp-u10-ending'],
                         ((2062, 192, 198, 0, 86,
                           'f2089c52c182e444f992971ea91cd21df68f0c5807344d1b2f00d164f062eb03'),))
        self.assertEqual(set(gc.LOADER_ALLOWED), {'gp-u9-win', 'gp-u10-ending'})


if __name__ == '__main__':
    unittest.main()
