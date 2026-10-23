# tools/tests/test_gp_compare.py
import contextlib, gzip, hashlib, io, os, shutil, subprocess, sys, tempfile, unittest

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

    def run_claim(self, n, report=False, max_start=100, max_unexplained=gc.REPORT_MAX):
        cap = gc.Lazy(gc._paths(self.cap, 'frame_%05d.raw.gz'), gc.load_capture_frame)
        port = gc.Lazy(gc._paths(self.port, 'frame_%05d.ipx'), gc.load_port_frame)
        rows = [tc.row_hashes(port[m]) for m in range(len(port))]
        out = []
        rc, first, _, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, n, report,
                                         out.append, max_start,
                                         max_unexplained=max_unexplained)
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

    def test_the_report_cap_can_be_lifted(self):
        bad = [bytes([9]) * tc.FRAME_BYTES] * 7
        self.write([1, 2], [rgb(1), rgb(2)] + bad)
        for cap, want in ((gc.REPORT_MAX, gc.REPORT_MAX), (0, 7)):
            rc, first, out = self.run_claim(0, report=True, max_unexplained=cap)
            self.assertEqual(sum('UNEXPLAINED capture' in l for l in out), want, out)
        rc, first, out = self.run_claim(0, report=False)      # enforced mode still stops at 1
        self.assertEqual(sum('UNEXPLAINED capture' in l for l in out), 1, out)

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

    def test_the_reach_pin_catches_a_shrunken_window(self):
        # sub-project A §A1 (the under-render blind spot): the window and coverage are
        # computed up to the reach p, so a port that renders fewer screens just shrinks
        # them and passes; the reach must be at least the pinned one.
        out = []
        self.assertEqual(gc.reach_check('s', 40, 50, out=out.append), 1)   # shrank -> FAIL
        self.assertEqual(gc.reach_check('s', 50, 50, out=out.append), 0)   # exact -> ok
        self.assertEqual(gc.reach_check('s', 60, 50, out=out.append), 0)   # grew -> ok (raise)
        self.assertEqual(gc.reach_check('s', 40, None, out=out.append), 1)  # unpinned -> FAIL
        self.assertTrue(any('pinned reach 50' in l for l in out), out)
        self.assertTrue(any('improved: raise the pin' in l for l in out), out)
        self.assertTrue(any('reach pin is not set' in l for l in out), out)

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


class AttractPortExh(unittest.TestCase):
    # Sub-project A A1c: attract_compare's port attract frames exhibited
    # (len(port_exh), the set the capture's explanations name) is pinned. A port
    # that renders fewer attract frames exhibits fewer; the count is independent
    # of the port dump's own length, so it cannot be shrunk away.
    ATTRACT_TOOL = os.path.join(ROOT, 'tools', 'attract_compare.py')

    def _run(self, cap, port, *extra):
        env = dict(os.environ)
        env.pop('PR_ORACLE_REQUIRED', None)
        return subprocess.run(
            [sys.executable, self.ATTRACT_TOOL, '--capture', cap, '--port', port] + list(extra),
            capture_output=True, text=True, env=env)

    def _write(self, d, fills, fmt='frame_%04d.raw'):
        for i, v in enumerate(fills):
            with open(os.path.join(d, fmt % i), 'wb') as f:
                f.write(bytes([v]) * tc.FRAME_BYTES)

    def _fixtures(self):
        d = tempfile.mkdtemp(prefix='attract-reach-')
        self.addCleanup(shutil.rmtree, d, True)
        cap = os.path.join(d, 'capture')
        port = os.path.join(d, 'port')
        attract, title = os.path.join(port, 'attract'), os.path.join(port, 'title')
        for p in (cap, attract, title):
            os.makedirs(p)
        # Two identical attract capture frames (both exhibit port attract 0) and
        # one title frame. Port attract holds a second screen the capture never
        # shows, so len(port_exh) is 1 while the dump is 2 long.
        self._write(cap, [0x11, 0x11, 0x33])
        self._write(attract, [0x11, 0x22])
        self._write(title, [0x33])
        return cap, port

    def test_the_pin_passes_at_the_measured_count(self):
        cap, port = self._fixtures()
        r = self._run(cap, port, '--min-port-exh', '1')
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('port attract frames exhibited 1/2', r.stdout)

    def test_the_pin_fails_when_the_port_exhibits_fewer(self):
        cap, port = self._fixtures()
        r = self._run(cap, port, '--min-port-exh', '2')
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('FAIL: port attract frames exhibited 1 < pinned 2', r.stdout)


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

    def test_the_device_claim_is_opt_in_and_catches_a_sticky_word(self):
        # record §H6: dev1 reads 2 at f=5 (the selection) and 0 from f=6. The default
        # trace claim never looks at it; --dev-words (DEV_FIELDS added to the fields)
        # catches a port that leaves the word set -- the no-pad presence regression.
        cap = [_S(f, dev1=2 if f == 5 else 0) for f in range(10)]
        port = [_t(f, dev1=2 if f >= 5 else 0) for f in range(10)]
        out = []
        self.assertEqual(gc.trace_claim('t', cap, port, 9, out.append)[:2], (0, None), out)
        out = []
        rc, first = gc.trace_claim('t', cap, port, 9, out.append, False,
                                   gs.TRACE_FIELDS + gs.DEV_FIELDS)
        self.assertEqual((rc, first), (1, 6), out)     # the clear is the first difference
        self.assertTrue(any('first difference f=6 (6) in dev1' in l for l in out), out)

    def test_the_device_fields_must_be_in_the_capture(self):
        # an older capture has no dev1/dev2: the flag's claim FAILs instead of
        # comparing nothing (the existing captures' pins stay where they were)
        cap = [_S(f, dev1=0, dev2=0) for f in range(3)]
        self.assertTrue(gc.has_dev_fields(cap))
        old = [(n, d, s) for n, d, s in gs.SNAP_FIELDS if n not in gs.DEV_FIELDS]
        stripped = ['S ms=1 %s kb=0000 head=001E tail=001E'
                    % ' '.join('%s=%0*X' % (n, 2 * s, 1) for n, _, s in old)]
        self.assertFalse(gc.has_dev_fields(stripped))
        self.assertFalse(gc.has_dev_fields([]))

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

    def test_the_device_pin_on_a_capture_without_the_fields_fails_cleanly(self):
        # record §H6.4: --dev-words over an older poll.log prints the FAIL and returns
        # 1 before the claim; comparing a field the capture lacks must not raise (the
        # report mode goes on with the plain fields, its exit is 0 anyway)
        self.dump()
        pins = ('--min-first', '2', '--trace-min-first', '13', '--max-start', '0', '--min-reach', '1')
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                           '--dev-words', *pins)
        self.assertEqual(rc, 0, out)                    # the fields are in SNAP_FIELDS
        with open(os.path.join(self.cap, 'poll.log'), 'w') as f:      # an older capture
            f.write(''.join(' '.join(p for p in _S(k, t508=k - 1).split()
                                     if p.split('=')[0] not in gs.DEV_FIELDS) + '\n'
                            for k in range(10, 13)))
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                           '--dev-words', *pins)
        self.assertEqual(rc, 1, out)
        self.assertIn('trace: FAIL: the capture records no device-word fields', out)
        self.assertNotIn('Traceback', out)              # not a KeyError
        rc, out = self.cli('--report', '--scenario', 'gp-x', '--capture', self.cap,
                           '--port', self.port, '--dev-words')
        self.assertEqual(rc, 0, out)
        self.assertIn('FAIL: the capture records no device-word fields', out)

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
        self.assertIn('FAIL: the window-reach pin is not set', out)
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                           '--min-first', '2', '--trace-min-first', '13')
        self.assertEqual(rc, 1, out)                     # the start and reach pins are still unset
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                           '--min-first', '2', '--trace-min-first', '13', '--max-start', '0',
                           '--min-reach', '1')
        self.assertEqual(rc, 0, out)
        self.assertIn('0 differing through 12; ratchet N 13 ok', out)

    def test_the_capture_identity_pin(self):
        # review 1, Important 3: the pins belong to one capture; a different poll.log (a re-capture)
        # must fail, not skip, and say the pins are invalidated
        self.dump()
        import hashlib
        with open(os.path.join(self.cap, 'poll.log'), 'rb') as f:
            good = hashlib.sha256(f.read()).hexdigest()
        base = ['--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                '--min-first', '2', '--trace-min-first', '13', '--max-start', '0', '--min-reach', '1']
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

    def test_the_report_max_option(self):
        self.dump()                                   # clean frames 0-1 and the logs
        bad = [bytes([9]) * tc.FRAME_BYTES] * 7
        self.write([1, 2], [rgb(1), rgb(2)] + bad)    # frames 0-1 clean, 2-8 unexplained
        rc, out = self.cli('--report', '--scenario', 'gp-x', '--capture', self.cap,
                           '--port', self.port)
        self.assertEqual(rc, 0)
        self.assertEqual(out.count('UNEXPLAINED capture'), gc.REPORT_MAX, out)
        rc, out = self.cli('--report', '--scenario', 'gp-x', '--capture', self.cap,
                           '--port', self.port, '--max-unexplained', '0')
        self.assertEqual(rc, 0)
        self.assertEqual(out.count('UNEXPLAINED capture'), 7, out)


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
        rc, first, _, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, n,
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

    def test_the_table_names_the_measured_u13_frame(self):
        # Task 3 (record §P3): U13's capture 5120 (raw 8839) at the ending's
        # mode-0x15 fade. Rows 1..4 are the 4940-frame dump's ONLY unheld rows,
        # row 0 is port 4316/4317/4318's (identical there) and the whole range
        # rows 5..199 is port 4317's alone, so the pair (4316, 4317) is the
        # unique match. The band is the unheld run's ends (1..4), 1280 pixels
        # (3840 bytes) of which exactly 13 pixels differ from port 4317's rows;
        # its cause is a named gap (§P3). This test pins the tuple only: the
        # mutation proof is delegated to the class fixture's sub-tests (a mutated
        # band byte / bound port frame un-explains it) and the manual real-dump
        # run in record §P3.5.
        self.assertEqual(gc.BANDS_ALLOWED['gp-u13-ending2'],
                         ((5120, 1, 5,
                           '56b900b1562b3abdf600bb678e31abed485327c292ea26eb022fe7daf0e32250'),))


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
        rc, first, _, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, n,
                                      kw.get('report', False), out.append,
                                      kw.get('max_start', 0), None, kw.get('s3', self.s3))
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
        rc, first, _, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, 4,
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

    def test_u13_scan3_recipe_is_pinned_and_mutations_fail(self):
        # Task 2 (record §P2): U13's capture 4248 is its own three-segment
        # recipe — rows 0..45 to port 3549, 46..96 to 3550, 97..199 to 3551,
        # the pair (3550, 3551) unique in the 4940-frame dump — not another
        # scenario's frame or pair. Task 3 (§P3) adds capture 4320, the same
        # class 72 capture frames later: rows 0..17 to port 3613, 18..107 to
        # 3614, 108..199 to 3615, the split forced by the bytes (3614 does not
        # hold rows 14..17, 3615 does not hold 88..107) and the pair unique.
        # This test PINS the two tuples only: the recipe's mutations are proved
        # on the fixture by the sub-tests below, and on the real dump by the
        # manual mutation run that record §P3.5 reports (the dump is
        # git-ignored, so it cannot be a unit case) — the name does not claim a
        # dump test.
        self.assertEqual(gc.SCAN3_ALLOWED['gp-u13-ending2'],
                         ((4248, 46, 97,
                           'fc1044a6a9e5bb6cee142ce02811c4a4823585747351089bd1514a76685fc6e6'),
                          (4320, 18, 108,
                           'c8fa5b9305b132f09d1853594b40429750636f08da70c5e154dc5945ce4b8cb4')))

        # The fixture's named frame is bound by its top hash and by the unique
        # adjacent pair; changing either un-explains it (and names which bind).
        self.write_fixture()
        self.assertEqual(self.run_scan3()[0], 0)
        for changed, why in (((1, 2, 4, '0' * 64), 'its top rows 0..1 are'),
                             ((1, 2, 3, self.sha), 'no unique adjacent port pair')):
            with self.subTest(changed=changed):
                rc, first, out = self.run_scan3(s3=(changed,))
                self.assertEqual((rc, first), (1, 1), out)
                self.assertTrue(any('FAIL: allowed capture 1' in l and why in l for l in out), out)

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
        self.assertEqual(gc.SCAN3_ALLOWED['gp-u16-twop2'],
                         ((4089, 1, 66,
                           '8eeaad13ee80ed904df9a8d600d3972f34959d6d5a0692cdbd319003b80376f6'),))


class OverlayAllowance(Dirs):
    # Track G task 7 (record §G7): the named per-frame glyph-overlay allowance,
    # the SCAN3_ALLOWED pattern for a loader presentation the replay houses at
    # another frame. Review Focus #3: a named frame is explained ONLY by its own
    # byte recipe — T1's bytes on the mask {i: T1[i] != T2[i]}, S's off it —
    # never a class, and a different byte layout of the same frame fails.
    #
    # The fixture: port [rgb(1), T1, T2, S]. T1 is the loader's presentation
    # frame: a black screen with GV-index glyph pixels (index 5 in the grey DAC
    # -> (5,5,5)); T2 is the same indices under a DAC whose index 5 is black, so
    # the mask {T1 != T2} is exactly the glyph pixels (the real pair is the
    # loader frame with the palette flushed and the same frame with the stale
    # black palette). S is a grey screen; the named capture frame is S with the
    # glyph pixels replaced by T1's — a composition no clean/splice/transition
    # expresses. The capture's other frames are the window start and the tail.
    GLYPH = ((3, 5), (4, 6), (4, 9))
    GV = 5

    def setUp(self):
        super().setUp()
        self.t1, self.t2 = self.presentation_frames(DAC)
        self.s = rgb(9)
        self.entry = (1, 1, 2, 3, self.glyph_sha(self.t1, self.t2))
        self.ov = (self.entry,)
        self.write_fixture()

    def presentation_frames(self, dac):
        """(the visible presentation frame, the same indices under the stale
        black palette): every index 0 (black) but the glyph pixels' index 5."""
        idx = bytearray(64000)
        for r, x in self.GLYPH:
            idx[r * 320 + x] = self.GV
        stale = bytearray(dac)
        for c in range(3):
            stale[3 * self.GV + c] = 0
        return bytes(idx) + bytes(dac), bytes(idx) + bytes(stale)

    def glyph_sha(self, t1, t2):
        a, b = gc.expand_ipx(t1), gc.expand_ipx(t2)
        mask = gc.overlay_mask(a, b)
        return hashlib.sha256(b''.join(a[3 * i:3 * i + 3] for i in mask)).hexdigest()

    def captured(self, screen):
        c = bytearray(screen)
        for r, x in self.GLYPH:
            i = (r * 320 + x) * 3
            c[i:i + 3] = bytes([self.GV] * 3)
        return bytes(c)

    def write_fixture(self, cap_frame=None):
        ks = [1, None, None, 9]
        for i, k in enumerate(ks):
            data = self.t1 if i == 1 else self.t2 if i == 2 else ipx(k)
            with open(os.path.join(self.port, 'frame_%05d.ipx' % i), 'wb') as f:
                f.write(data)
        frames = [rgb(1), self.captured(self.s) if cap_frame is None else cap_frame, self.s, rgb(1)]
        for j, c in enumerate(frames):
            with gzip.open(os.path.join(self.cap, 'frame_%05d.raw.gz' % j), 'wb') as f:
                f.write(c)
        with open(os.path.join(self.cap, 'window.txt'), 'w') as f:
            f.write(''.join('%05d %d\n' % (j, 100 + j) for j in range(len(frames))))

    def run_overlay(self, n=4, **kw):
        cap = gc.Lazy(gc._paths(self.cap, 'frame_%05d.raw.gz'), gc.load_capture_frame)
        port = gc.Lazy(gc._paths(self.port, 'frame_%05d.ipx'), gc.load_port_frame)
        rows = [tc.row_hashes(port[m]) for m in range(len(port))]
        out = []
        rc, first, _, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, n,
                                      kw.get('report', False), out.append,
                                      kw.get('max_start', 0), None, None,
                                      kw.get('ov', self.ov))
        return rc, first, out

    def test_the_named_overlay_is_explained(self):
        rc, first, out = self.run_overlay()
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('allowed by name as a glyph overlay: 3 px of port 1 over port 3 '
                            '(the mask from port 2, the named glyph bytes)' in l for l in out), out)
        self.assertTrue(any('1 overlay-allowed' in l for l in out), out)

    def test_u13_overlay_recipe_is_pinned_and_mutations_fail(self):
        # Task 7 (record gameplay-truth-ii §J5): U13's capture 2188 is its own
        # loader strip recipe, not U10's frame or port pair.
        self.assertEqual(gc.OVERLAY_ALLOWED['gp-u13-ending2'],
                         ((2188, 1799, 1798, 1800,
                           'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),))

        # The fixture's captured overlay is byte-pinned by its SHA and off-mask
        # screen source. Changing either value in the named recipe un-explains it.
        wrong_sha = (1, 1, 2, 3, '0' * 64)
        wrong_screen = (1, 1, 2, 2, self.entry[4])
        for changed in (wrong_sha, wrong_screen):
            with self.subTest(changed=changed):
                rc, first, out = self.run_overlay(ov=(changed,))
                self.assertEqual((rc, first), (1, 1), out)
                self.assertTrue(any('FAIL: allowed capture 1' in l and
                                    'is not its named glyph overlay' in l for l in out), out)

    def test_a_mutated_screen_frame_unexplains_it_again(self):
        # Review Focus #3: mutate S's bytes (frame 3: 9 -> 4) without touching
        # the capture. The off-mask pixels no longer match, the allowance FAILs
        # and the ratchet catches the frame.
        self.assertEqual(self.run_overlay()[0], 0)
        with open(os.path.join(self.port, 'frame_00003.ipx'), 'wb') as f:
            f.write(ipx(4))
        rc, first, out = self.run_overlay()
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in l and 'is not its named glyph overlay' in l
                            for l in out), out)
        self.assertTrue(any('FAIL: first unexplained 1 < ratchet N 4' in l for l in out), out)

    def test_a_mutated_overlay_frame_unexplains_it_again(self):
        # the overlay frame's bytes on the mask are the glyph fingerprint: a
        # different DAC colour for the glyph index is a different presentation.
        self.assertEqual(self.run_overlay()[0], 0)
        dac = bytearray(DAC)
        for c in range(3):
            dac[3 * self.GV + c] = 7
        t1b, _ = self.presentation_frames(bytes(dac))
        with open(os.path.join(self.port, 'frame_00001.ipx'), 'wb') as f:
            f.write(t1b)
        rc, first, out = self.run_overlay()
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in l and 'its glyph pixels are' in l
                            for l in out), out)

    def test_a_mutated_mask_source_unexplains_it_again(self):
        # T2 is the mask's indicator: flushing it (the same DAC as T1) empties
        # the mask, so the rule expects S everywhere and the capture's glyph
        # pixels are unexplained again.
        self.assertEqual(self.run_overlay()[0], 0)
        with open(os.path.join(self.port, 'frame_00002.ipx'), 'wb') as f:
            f.write(self.t1)
        rc, first, out = self.run_overlay()
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in l and 'its glyph pixels are' in l
                            for l in out), out)

    def test_a_shifted_glyph_in_the_capture_is_not_the_named_frame(self):
        # Review Focus #3 (the capture side): the same screen with the glyphs one
        # pixel over is a DIFFERENT byte layout and must not be silenced.
        shifted = bytearray(self.s)
        for r, x in self.GLYPH:
            i = (r * 320 + x + 1) * 3
            shifted[i:i + 3] = bytes([self.GV] * 3)
        self.write_fixture(cap_frame=bytes(shifted))
        rc, first, out = self.run_overlay()
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in l and 'is not its named glyph overlay' in l
                            for l in out), out)

    def test_the_mask_sources_bytes_are_only_an_indicator(self):
        # the named limit, documented so that closing it must move the record: a
        # T2 whose non-mask pixels change (here the stale palette keeps the same
        # `!=` relation) leaves the recipe — and the explanation — unchanged.
        dac = bytearray(DAC)
        for c in range(3):
            dac[3 * self.GV + c] = 1
        _, t2b = self.presentation_frames(bytes(dac))
        with open(os.path.join(self.port, 'frame_00002.ipx'), 'wb') as f:
            f.write(t2b)
        rc, first, out = self.run_overlay()
        self.assertEqual((rc, first), (0, None), out)

    def test_a_named_frame_the_model_explains_is_a_stale_name(self):
        self.write_fixture(cap_frame=self.s)          # now clean: the name is stale
        rc, first, out = self.run_overlay()
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('retire the name' in l for l in out), out)

    def test_the_table_names_the_measured_overlays(self):
        # the pinned recipes of record §G7: gp-u9-win 2209 (port 1776's glyphs
        # over port 1777, the mask from port 1775) and gp-u10-ending 2062 (port
        # 1685 over 1686, mask from 1684) — both the same glyph fingerprint
        # (de1c1bb2..). A changed name is a changed claim: it must move with a
        # re-measurement in the record, never silently.
        self.assertEqual(gc.OVERLAY_ALLOWED['gp-u9-win'],
                         ((2209, 1776, 1775, 1777,
                           'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),))
        self.assertEqual(gc.OVERLAY_ALLOWED['gp-u10-ending'],
                         ((2062, 1685, 1684, 1686,
                           'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),))
        self.assertEqual(gc.OVERLAY_ALLOWED['gp-u16-twop2'],
                         ((8066, 6815, 6814, 6816,
                           'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),))


if __name__ == '__main__':
    unittest.main()
