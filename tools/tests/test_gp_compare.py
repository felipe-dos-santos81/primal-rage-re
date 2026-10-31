# tools/tests/test_gp_compare.py
import contextlib, gzip, hashlib, io, os, shutil, subprocess, sys, tempfile, unittest
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_compare as gc
import gp_session as gs
import title_compare as tc

DAC = bytes(i for i in range(256) for _ in range(3))   # grey: index i -> (i, i, i)
NOT_REQUESTED = object()
_SAVED_EXACT = None


def setUpModule():
    global _SAVED_EXACT
    _SAVED_EXACT = os.environ.get('PR_PINS_EXACT')
    os.environ['PR_PINS_EXACT'] = '0'


def tearDownModule():
    if _SAVED_EXACT is None:
        os.environ.pop('PR_PINS_EXACT', None)
    else:
        os.environ['PR_PINS_EXACT'] = _SAVED_EXACT


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

    def write_frames(self, port_frames, cap_frames):
        for i, data in enumerate(port_frames):
            with open(os.path.join(self.port, 'frame_%05d.ipx' % i), 'wb') as f:
                f.write(data)
        for j, c in enumerate(cap_frames):
            with gzip.open(os.path.join(self.cap, 'frame_%05d.raw.gz' % j), 'wb') as f:
                f.write(c)
        with open(os.path.join(self.cap, 'window.txt'), 'w') as f:
            f.write(''.join('%05d %d\n' % (j, 100 + j) for j in range(len(cap_frames))))

    def write(self, port_ks, cap_frames):
        self.write_frames([ipx(k) for k in port_ks], cap_frames)

    def run_claim(self, n, report=False, max_start=100, max_unexplained=gc.REPORT_MAX,
                  bands=None, scan3=None, overlay=None, max_missing=NOT_REQUESTED):
        cap = gc.Lazy(gc._paths(self.cap, 'frame_%05d.raw.gz'), gc.load_capture_frame)
        port = gc.Lazy(gc._paths(self.port, 'frame_%05d.ipx'), gc.load_port_frame)
        rows = [tc.row_hashes(port[m]) for m in range(len(port))]
        out = []
        kw = {} if max_missing is NOT_REQUESTED else {'max_missing': max_missing}
        rc, first, _, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, n, report,
                                         out.append, max_start, bands, scan3, overlay,
                                         max_unexplained=max_unexplained, **kw)
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
        with mock.patch.dict(os.environ, {'PR_PINS_EXACT': '0'}):
            rc, first, out = self.run_claim(1)
        self.assertEqual((rc, first), (0, 2), out)
        self.assertTrue(any('first unexplained 2, ratchet N 1 ok (improved: raise N)' in l for l in out), out)
        with mock.patch.dict(os.environ, {'PR_PINS_EXACT': '1'}):
            rc, first, out = self.run_claim(1)
        self.assertEqual((rc, first), (1, 2), out)
        self.assertTrue(any('FAIL:' in l and 're-pin' in l for l in out), out)

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
        with mock.patch.dict(os.environ, {'PR_PINS_EXACT': '0'}):
            rc, first, out = self.run_claim(2, max_start=1)
        self.assertEqual(rc, 0, out)
        self.assertTrue(any('window start 0 < pinned start 1 (improved: lower the pin)' in l for l in out), out)
        with mock.patch.dict(os.environ, {'PR_PINS_EXACT': '1'}):
            rc, first, out = self.run_claim(2, max_start=1)
        self.assertEqual(rc, 1, out)
        self.assertTrue(any('FAIL:' in l and 're-pin' in l for l in out), out)

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
        with mock.patch.dict(os.environ, {'PR_PINS_EXACT': '0'}):
            self.assertEqual(gc.reach_check('s', 60, 50, out=out.append), 0)   # grew -> ok (raise)
        self.assertEqual(gc.reach_check('s', 40, None, out=out.append), 1)  # unpinned -> FAIL
        self.assertTrue(any('pinned reach 50' in l for l in out), out)
        self.assertTrue(any('improved: raise the pin' in l for l in out), out)
        self.assertTrue(any('reach pin is not set' in l for l in out), out)
        with mock.patch.dict(os.environ, {'PR_PINS_EXACT': '1'}):
            self.assertEqual(gc.reach_check('s', 60, 50, out=out.append), 1)
        self.assertTrue(any('FAIL:' in l and 're-pin' in l for l in out), out)

    def test_report_mode_prints_no_verdict_and_checks_no_pin(self):
        bad = bytes([9]) * tc.FRAME_BYTES
        self.write([1, 2], [rgb(1), bad])
        rc, first, out = self.run_claim(None, report=True, max_start=None)
        self.assertEqual(first, 1, out)
        self.assertFalse(any('ratchet N' in l or 'FAIL' in l or 'pinned' in l or 'pin is' in l for l in out), out)

    def test_a_fully_explained_run_names_the_exact_pin(self):
        self.write([1, 2], [rgb(1), rgb(2)])
        with mock.patch.dict(os.environ, {'PR_PINS_EXACT': '0'}):
            rc, first, out = self.run_claim(1)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('every item is explained: N = 2 is the exact pin' in l for l in out), out)
        out2 = self.run_claim(2)[2]
        self.assertFalse(any('exact pin' in l for l in out2), out2)
        with mock.patch.dict(os.environ, {'PR_PINS_EXACT': '1'}):
            rc, first, out = self.run_claim(1)
        self.assertEqual((rc, first), (1, None), out)
        self.assertTrue(any('FAIL:' in l and 're-pin' in l for l in out), out)

    def test_coverage_is_reported_for_a_port_class_the_capture_never_shows(self):
        # A garbage port class between two good ones explains nothing. Repeated
        # byte-identical frames are one class and an all-black frame is excluded.
        black = bytes(64000) + DAC
        self.write_frames([ipx(1), ipx(99), ipx(99), black, ipx(2)],
                          [rgb(1), rgb(1), rgb(2)])
        rc, first, out = self.run_claim(3)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('1 non-black port content class(es) up to port 4 not exhibited' in l
                            and '[1]' in l for l in out), out)

    def test_coverage_keeps_a_class_with_an_unexhibited_occurrence(self):
        # Canonicalise the legacy missing-occurrence set, not the whole dump's
        # class set: the second rgb(1) is unexhibited even though port 0 is.
        self.write([1, 1, 2], [rgb(1), rgb(2)])
        rc, first, out = self.run_claim(2, max_missing=1)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('coverage: 1 missing class(es), pin 1' in l for l in out), out)
        rc, first, out = self.run_claim(2, max_missing=0)
        self.assertEqual(rc, 1, out)
        self.assertTrue(any('FAIL: 1 missing class(es) > pinned 0' in l for l in out), out)

    def test_coverage_pin_boundaries_and_unpinned(self):
        # The single unseen class is port 1. The exact pin passes, one below
        # fails, one above reports an improvement, and no pin fails closed.
        self.write([1, 99, 2], [rgb(1), rgb(2)])
        rc, first, out = self.run_claim(2, max_missing=1)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('coverage: 1 missing class(es), pin 1' in l for l in out), out)
        rc, first, out = self.run_claim(2, max_missing=0)
        self.assertEqual(rc, 1, out)
        self.assertTrue(any('FAIL: 1 missing class(es) > pinned 0' in l for l in out), out)
        with mock.patch.dict(os.environ, {'PR_PINS_EXACT': '0'}):
            rc, first, out = self.run_claim(2, max_missing=2)
        self.assertEqual(rc, 0, out)
        self.assertTrue(any('improved: lower the pin' in l for l in out), out)
        with mock.patch.dict(os.environ, {'PR_PINS_EXACT': '1'}):
            rc, first, out = self.run_claim(2, max_missing=2)
        self.assertEqual(rc, 1, out)
        self.assertTrue(any('FAIL:' in l and 're-pin' in l for l in out), out)
        rc, first, out = self.run_claim(2, max_missing=None)
        self.assertEqual(rc, 1, out)
        self.assertTrue(any('coverage pin is not set' in l for l in out), out)

    def test_order_rejects_a_rewind_at_capture_2(self):
        self.write([1, 2, 3], [rgb(1), rgb(3), rgb(2), rgb(1)])
        rc, first, out = self.run_claim(4)
        self.assertEqual((rc, first), (1, None), out)
        self.assertTrue(any('order failure capture 2 (raw 102)' in l and 'kind=clean' in l
                            and 'last=2' in l and 'candidates=[1]' in l for l in out), out)

    def test_order_uses_the_earliest_feasible_occurrence(self):
        # The second rgb(1) is the same held screen at port 2, not a rewind to port 0.
        self.write([1, 2, 1, 3], [rgb(1), rgb(2), rgb(1), rgb(3)])
        rc, first, out = self.run_claim(4)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('order: 0 failures' in l for l in out), out)

    def test_order_accepts_a_forward_mid_update_splice(self):
        # U8 585 / U9 1896 shape: capture 2 is clean at the older port 1, but is
        # also a mid-update splice of the forward pair (2, 3), with both sides
        # contributing bytes that differ from the other side.
        split = 10 * tc.FRAME_W
        low, high = ipx(3), ipx(4)
        middle = low[:split] + high[split:]
        middle_rgb = gc.expand_ipx(middle)
        self.write_frames([ipx(1), middle, low, high],
                          [rgb(1), rgb(3), middle_rgb, rgb(4)])
        rc, first, out = self.run_claim(4)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('order: 0 failures' in l for l in out), out)

    def test_order_search_window_follows_the_chosen_occurrence(self):
        # Several pairs explain the first splice. The minimum-feasible choice is
        # (0, 1), not the later (4, 5); using the coverage cursor's later pair
        # makes capture 3 look like a rewind. This is the U22 repeated-screen
        # shape in a small model, with no capture or dump dependency.
        saved = (tc.FRAME_W, tc.FRAME_H, tc.ROW, tc.FRAME_BYTES, gc.BLACK_ROW)
        try:
            tc.FRAME_W, tc.FRAME_H = 2, 3
            tc.ROW = tc.FRAME_W * 3
            tc.FRAME_BYTES = tc.ROW * tc.FRAME_H
            gc.BLACK_ROW = hashlib.md5(bytes(tc.ROW)).digest()
            a = bytes.fromhex('010301020101040104020201020203040202')
            b = bytes.fromhex('040304020101040201030403040203010103')
            port = [a, b, b, a, a, b]
            cap = [a, a[:3] + b[3:], a[:9] + b[9:],
                   b[:9] + a[9:], b[:11] + a[11:], b]
            rows = [tc.row_hashes(p) for p in port]
            out = []
            rc, first, _, _ = gc.frame_claim('t', cap, list(range(len(cap))), port, rows,
                                             len(cap), False, out.append, 0)
            self.assertEqual((rc, first), (0, None), out)
            self.assertTrue(any('order: 0 failures' in line for line in out), out)
        finally:
            tc.FRAME_W, tc.FRAME_H, tc.ROW, tc.FRAME_BYTES, gc.BLACK_ROW = saved

    def test_order_rejects_a_degenerate_splice_rewind(self):
        # Capture 2 is clean at port 1 after port 2 was exhibited. It can be
        # written as a suffix splice of (1, 2), but the suffix is identical in
        # both frames: port 2 contributes no later-frame-unique byte. Counting
        # that degenerate splice as exhibiting port 2 would launder the rewind.
        split = 10 * tc.FRAME_W
        later = bytearray(ipx(3))
        later[split:64000] = ipx(2)[split:64000]
        later = bytes(later)
        self.write_frames([ipx(1), ipx(2), later],
                          [rgb(1), gc.expand_ipx(later), rgb(2)])
        rc, first, out = self.run_claim(3)
        self.assertEqual((rc, first), (1, None), out)
        self.assertTrue(any('order failure capture 2 (raw 102)' in l and 'kind=clean' in l
                            and 'last=2' in l and 'candidates=[1]' in l for l in out), out)

    def test_order_treats_band_and_scan3_as_neutral(self):
        # U22 5033/5034 shape: the mixed frame binds later port frames, but the
        # next clean frame is still allowed to be the earlier occurrence. If a
        # mixed allowance advanced the order cursor, capture 2 would rewind.
        r = tc.ROW
        band = rgb(2)[:81 * r] + bytes([7]) * (11 * r) + rgb(3)[92 * r:]
        bands = ((1, 81, 92, hashlib.sha256(band[81 * r:92 * r]).hexdigest()),)
        self.write([1, 2, 3, 4], [rgb(1), band, rgb(2), rgb(3), rgb(4)])
        rc, first, out = self.run_claim(5, bands=bands)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('order: 0 failures' in l for l in out), out)

        scan = rgb(2)[:2 * r] + rgb(3)[2 * r:4 * r] + rgb(4)[4 * r:]
        scan3 = ((1, 2, 4, hashlib.sha256(rgb(2)[:2 * r]).hexdigest()),)
        self.write([1, 2, 3, 4], [rgb(1), scan, rgb(2), rgb(3), rgb(4)])
        rc, first, out = self.run_claim(5, scan3=scan3)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('order: 0 failures' in l for l in out), out)


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

    def _run(self, cap, port, *extra, env=None):
        e = dict(os.environ)
        e.pop('PR_ORACLE_REQUIRED', None)
        e['PR_PINS_EXACT'] = '0'
        e.update(env or {})
        return subprocess.run(
            [sys.executable, self.ATTRACT_TOOL, '--capture', cap, '--port', port] + list(extra),
            capture_output=True, text=True, env=e)

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

    def test_an_improved_port_exh_pin_requires_a_repin_in_exact_mode(self):
        cap, port = self._fixtures()
        r = self._run(cap, port, '--min-port-exh', '0')
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('improved: raise the pin', r.stdout)
        r = self._run(cap, port, '--min-port-exh', '0', env={'PR_PINS_EXACT': '1'})
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('FAIL:', r.stdout)
        self.assertIn('re-pin', r.stdout)

    def test_coverage_excludes_frames_beyond_reach(self):
        cap, port = self._fixtures()  # port attract 1 is never reached
        r = self._run(cap, port, '--max-missing', '0')
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('coverage: 0 missing class(es), pin 0', r.stdout)

    def test_coverage_collapses_duplicates_and_is_ratchetable(self):
        cap, port = self._fixtures()
        self._write(cap, [0x11, 0x33, 0x44])
        self._write(os.path.join(port, 'attract'), [0x11, 0x22, 0x22, 0x33])
        self._write(os.path.join(port, 'title'), [0x44])
        r = self._run(cap, port, '--max-missing', '1')
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('coverage: 1 missing class(es), pin 1', r.stdout)
        r = self._run(cap, port, '--max-missing', '0')
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('FAIL: 1 missing class(es) > pinned 0', r.stdout)
        r = self._run(cap, port, '--max-missing', '2')
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('improved: lower the pin', r.stdout)
        r = self._run(cap, port, '--max-missing', '2', env={'PR_PINS_EXACT': '1'})
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('re-pin', r.stdout)


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
        pins = ('--min-first', '2', '--trace-min-first', '13', '--max-start', '0', '--min-reach', '1',
                '--max-missing', '0')
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
        self.assertIn('FAIL: the coverage pin is not set', out)
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                           '--min-first', '2', '--trace-min-first', '13')
        self.assertEqual(rc, 1, out)                     # the start, reach and coverage pins are still unset
        rc, out = self.cli('--scenario', 'gp-x', '--capture', self.cap, '--port', self.port,
                           '--min-first', '2', '--trace-min-first', '13', '--max-start', '0',
                           '--min-reach', '1', '--max-missing', '0')
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
                '--min-first', '2', '--trace-min-first', '13', '--max-start', '0', '--min-reach', '1',
                '--max-missing', '0']
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
        self.assertEqual((rc, first), (1, None), out)
        self.assertTrue(any('FAIL:' in l and 'retire the name' in l for l in out), out)

    def test_an_all_black_named_frame_is_a_stale_name(self):
        self.write_fixture()
        with gzip.open(os.path.join(self.cap, 'frame_00001.raw.gz'), 'wb') as f:
            f.write(bytes(tc.FRAME_BYTES))
        rc, first, out = self.run_band()
        self.assertEqual((rc, first), (1, None), out)
        self.assertTrue(any('named in the allowance but artifact now; retire the name' in l
                            for l in out), out)
        self.assertFalse(any('FIRST UNEXPLAINED capture 1' in l for l in out), out)

    def test_the_table_names_the_measured_gp_keys_fight_frame(self):
        # the pinned recipe of record §G5: capture 835 (raw 3934), rows 0..80 to
        # port f=0x832 / 92..199 to f=0x833, band sha d9c0ac62.. (§K.12's finding
        # re-measured at 5f874678). A changed name is a changed claim: it must move
        # with a re-measurement in the record, never silently.
        self.assertEqual(gc.BANDS_ALLOWED['gp-keys-fight'],
                         ((835, 81, 92,
                           'd9c0ac62f7e0c0d4082fba8d195eae4f767c56a7887bdf7346450511f998cad8'),))

    def test_the_table_names_the_measured_u13_frame(self):
        # Task 3 (record §P3), re-derived in §E1.5 (endgame truth Task 9): U13's
        # capture 5120 (raw 8839) at the ending's mode-0x15 fade. Rows 1..4 are
        # the 4954-frame dump's ONLY unheld rows, row 0 is port 4316/4317/4318's
        # (identical there) and the whole range rows 5..199 is port 4317's alone,
        # so the pair (4316, 4317) is the unique match. The band is the unheld
        # run's ends (1..4), 1280 pixels (3840 bytes) of which exactly 13 pixels
        # differ from port 4317's rows — 8 at index 0x32, 1 at 0x2E, 4 at 0x4D
        # (§E1.5's table); its cause is a named gap (§E1.5 withdraws the
        # 0x01047345 corner-ornament candidate). This test pins the tuple only:
        # the mutation proof is the Band5120 fixture below (the 13-pixel shape,
        # and a mutated bound byte) and the manual real-dump run in record §P3.5.
        self.assertEqual(gc.BANDS_ALLOWED['gp-u13-ending2'],
                         ((5120, 1, 5,
                           '56b900b1562b3abdf600bb678e31abed485327c292ea26eb022fe7daf0e32250'),))

    def test_the_table_names_the_measured_u21_frames(self):
        # Endgame truth Task 6 (record §E6): ARMADON's ending's fifteen present-copy
        # scan-outs (contract §G5's class, measured on the 4,974-frame dump): each
        # binds one unique adjacent pair by its byte-equal segment(s) and pins its
        # band. 426 and 756 are round 1's mode-8 screen (ports 291/292, 577/578),
        # 4317 the final's mode-0x0D -> 0x0C entry (3610/3611), the eleven 4470..4560
        # the mode-0xF master screen (3740/3741 .. 3816/3817) and 5119 the ending's
        # mode-0x15 fade (4300/4301, the U13 5120 class). PIN only, as the U13 entry:
        # the recipe's mutations are proved on the fixture by this class's sub-tests
        # and on the real dump by the manual run record §E6 reports; the dump is not
        # retained.
        self.assertEqual(gc.BANDS_ALLOWED['gp-u21-armadon-ending'],
                         ((426, 58, 191,
                           '5d3d8e35e3f51cf6b311f324e6464e3867ff0d1b14245a6bc171596019018b4b'),
                          (756, 151, 154,
                           'e59ab79c2ef799dc9c62fd288ecb3beab3e0872789ef00233e3ebbe87c96a3de'),
                          (4317, 4, 32,
                           '3144a709b3ec68ebd634a739d50dc4d828745de7cabf039dd1bf22b13687fafb'),
                          (4470, 0, 47,
                           'ddafe873449ea92c58b677ed0eb03621b32e20e027b26bb642507bd91f6f0e6e'),
                          (4477, 0, 29,
                           '2d86bf24b3159f478651834c631e1f5444edde3a2546871b40e1d53e0db2dc19'),
                          (4486, 195, 200,
                           '1a44ec8841928d96d6f4fbd54f642751f01484928ba811f212f51b847506bf08'),
                          (4491, 0, 11,
                           '6831e908ba5d1ebcae85df1864be3b05c7d8606fcdb8c1bf80c3a96495e0082e'),
                          (4497, 0, 82,
                           'df42f0b55e5a6d540833176d9931e422279d4034524bdb733ae0c0a5691bf3ff'),
                          (4502, 0, 13,
                           '3cf9379061f7441b7c4471b55310e473981af591de9127889290b38c964c3563'),
                          (4505, 5, 32,
                           'c72d73dac4804087ccd98a88e53a5106f9fa8624eab027d6e6bc223bb7ce3555'),
                          (4511, 0, 65,
                           '30e62f335803a407aa4620248f09698f7c20f7f7c939d71b3969aa804d55469e'),
                          (4527, 135, 200,
                           '929bc3ad9b85d5c8d642b4d5c10dff9133c466a13a5f972d823330ffe1e5efc9'),
                          (4554, 103, 200,
                           'e10b51fb7b5bd6efa86e215f5bbbe4a745e845bc1bd25bccbfd5bc57d435f89f'),
                          (4560, 110, 200,
                           'b0ca25c494237c479259872d18dd2967d93bd49a1850b7f6de03ff15c4b66950'),
                          (5119, 0, 2,
                           '287a61f4cc91a282bfdd583a0ebf510b90d87de913545aeaa8ed76d60b0e950a')))

    def test_the_table_names_the_measured_u22_frames(self):
        # Endgame truth Task 7 (record §E7): DIABLO's ending's eight present-copy
        # scan-outs of the 4,930-frame dump, each binding one unique adjacent pair by its
        # byte-equal segment(s) with its band pinned. 1988/2014/2020 are on the
        # match-result screen (ports 1638/1639, 1660/1661, 1665/1666) and
        # 5008/5013/5015/5016/5024 at the ending's mode-0x15 fade (4270/4271, 4276/4277,
        # 4278/4279, 4280/4281, 4291/4292, the U13 5120 class). PIN only, as the U13/U21
        # entries: the recipe's mutations are proved on the fixture by this class's
        # sub-tests and on the real dump by the manual run record §E7 reports; the dump
        # is not retained.
        self.assertEqual(gc.BANDS_ALLOWED['gp-u22-diablo-ending'],
                         ((1988, 117, 122,
                           '8910a572dd6de6db88203905a608459ef595d210c64ffb39498a76e4270365b9'),
                          (2014, 0, 66,
                           '62708bc21c8c9429e72c92b7786eeb331ec4429ec7d909fe2100a4456475e71a'),
                          (2020, 0, 13,
                           '4e2d273ec17c1c645cde62e95f74cc75bb427a67ce58fb11fb5edffff58fdf54'),
                          (5008, 177, 191,
                           '3e0e4a3ce88a017d190e8f06e26b2f60e0a243210d3853dde3b3a304603021ef'),
                          (5013, 0, 2,
                           '0ba833920723fcc0d3cd9e8e408c84af8a78ac80139d448173a3f935ba7d9144'),
                          (5015, 0, 2,
                           'd6a7f2bc5434edc9cb86ec6fe81e32f44b09891dfadbf99206cee5c1b4fb69b6'),
                          (5016, 94, 134,
                           'b0f6fe0c5e29acab29da07a98ad2ffd274f39bbfc4ad9b2fc9809481ac4c7f06'),
                          (5024, 149, 189,
                           'f5d6102f0b72f59341a505962e2541e1fd83203c3f4fe1560b4d9c62cec2b673')))


class Band5120(Dirs):
    # Endgame truth Task 9 (record §E1.5): the U13 5120 shape, the one band entry
    # whose band is NOT a copy of a port present: of its four rows' 1280 pixels,
    # exactly 13 differ from the only adjacent pair that binds the rows outside
    # them — 8 at port index 0x32, 1 at 0x2E and 4 at 0x4D (the record's table).
    # The fixture builds the shape synthetically (no capture is read): port 1's
    # rows 1..4 carry those indices under DAC 000000/100000/00000c, the capture's
    # band has 00001c at the 8 and black at the 5. A band no port frame holds is
    # still explained by its name, and one mutated byte of the bottom-bound frame
    # un-explains it again (the BandAllowance harness).
    R = tc.ROW
    W = tc.FRAME_W
    PX_32 = ((1, 23), (1, 24), (1, 295), (1, 296), (2, 22), (2, 297), (3, 21), (3, 298))
    PX_2E = ((3, 74),)
    PX_4D = ((4, 27), (4, 28), (4, 291), (4, 292))
    run_band = BandAllowance.run_band

    def setUp(self):
        super().setUp()
        dac = bytearray(DAC)
        for i, c in ((0x32, b'\x00\x00\x00'), (0x2E, b'\x10\x00\x00'), (0x4D, b'\x00\x00\x0c')):
            dac[3 * i:3 * i + 3] = c
        idx = bytearray(ipx(2)[:self.W * tc.FRAME_H])
        for r, x in self.PX_32:
            idx[r * self.W + x] = 0x32
        for r, x in self.PX_2E:
            idx[r * self.W + x] = 0x2E
        for r, x in self.PX_4D:
            idx[r * self.W + x] = 0x4D
        self.p1 = bytes(idx) + bytes(dac)
        self.p1_rgb = gc.expand_ipx(self.p1)
        band = bytearray(self.p1_rgb[self.R:5 * self.R])
        for r, x in self.PX_32:
            o = ((r - 1) * self.W + x) * 3
            band[o:o + 3] = b'\x00\x00\x1c'
        for r, x in self.PX_2E + self.PX_4D:
            o = ((r - 1) * self.W + x) * 3
            band[o:o + 3] = b'\x00\x00\x00'
        self.c = rgb(1)[:self.R] + bytes(band) + self.p1_rgb[5 * self.R:]
        self.sha = hashlib.sha256(bytes(band)).hexdigest()
        self.bands = ((1, 1, 5, self.sha),)

    def write_fixture(self):
        for i, data in enumerate((ipx(1), self.p1, ipx(3))):
            with open(os.path.join(self.port, 'frame_%05d.ipx' % i), 'wb') as f:
                f.write(data)
        for j, c in enumerate((rgb(1), self.c, rgb(3))):
            with gzip.open(os.path.join(self.cap, 'frame_%05d.raw.gz' % j), 'wb') as f:
                f.write(c)
        with open(os.path.join(self.cap, 'window.txt'), 'w') as f:
            f.write(''.join('%05d %d\n' % (j, 100 + j) for j in range(3)))

    def test_the_fixture_carries_the_5120_shape(self):
        port_band = self.p1_rgb[self.R:5 * self.R]
        cap_band = self.c[self.R:5 * self.R]
        diff = [i for i in range(4 * self.W) if cap_band[3 * i:3 * i + 3] != port_band[3 * i:3 * i + 3]]
        self.assertEqual([(1 + i // self.W, i % self.W) for i in diff],
                         sorted(self.PX_32 + self.PX_2E + self.PX_4D))
        idx = self.p1[:self.W * tc.FRAME_H]
        at = [idx[(1 + i // self.W) * self.W + i % self.W] for i in diff]
        self.assertEqual((at.count(0x32), at.count(0x2E), at.count(0x4D)), (8, 1, 4))

    def test_a_band_no_port_frame_holds_is_explained(self):
        self.write_fixture()
        rc, first, out = self.run_band(3)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(any('allowed by name as a band splice: port 0/1' in l for l in out), out)
        self.assertTrue(any('1 band-allowed' in l for l in out), out)

    def test_a_mutated_bound_byte_unexplains_it_again(self):
        self.write_fixture()
        self.assertEqual(self.run_band(3)[0], 0)
        data = bytearray(self.p1)
        data[5 * self.W] ^= 1                      # one byte of the bottom-bound row
        with open(os.path.join(self.port, 'frame_00001.ipx'), 'wb') as f:
            f.write(data)
        rc, first, out = self.run_band(3)
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in l and 'no unique adjacent port pair' in l
                            for l in out), out)


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

    def test_an_occurrence_pinned_pair_selects_a_repeated_layout(self):
        # gp-twop-ko's wipe repeats the same byte layout three times. The fifth
        # tuple field names the measured occurrence without weakening the
        # ordinary four-field recipe's unique-pair rule above; a pin that does
        # not bind the rows must still refuse the frame.
        port = [rgb(1), rgb(2), rgb(3), rgb(2), rgb(3)]
        try:
            ex, why = gc.scan3_explain(self.c, port, self.entry + (1,))
        except ValueError as e:
            self.fail('occurrence-pinned recipe was rejected: %s' % e)
        self.assertEqual((ex, why), ([0, 1, 2], None))
        ex, why = gc.scan3_explain(self.c, port, self.entry + (0,))
        self.assertIsNone(ex)
        self.assertIn('pinned adjacent port pair 0/1 does not bind', why)

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
        self.assertEqual((rc, first), (1, None), out)
        self.assertTrue(any('FAIL:' in l and 'retire the name' in l for l in out), out)

    def test_an_all_black_named_frame_is_a_stale_name(self):
        self.write_fixture()
        with gzip.open(os.path.join(self.cap, 'frame_00001.raw.gz'), 'wb') as f:
            f.write(bytes(tc.FRAME_BYTES))
        rc, first, out = self.run_scan3()
        self.assertEqual((rc, first), (1, None), out)
        self.assertTrue(any('named in the scan-out allowance but artifact now; retire the name' in l
                            for l in out), out)
        self.assertFalse(any('FIRST UNEXPLAINED capture 1' in l for l in out), out)

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

    def test_u27_recipe_rejects_one_mutated_bound_byte(self):
        # Live truth Task 7: U27's capture 334 (raw 2837) binds top rows 0..2
        # to port 219, middle rows 3..62 to port 221 and bottom rows 63..199
        # to port 222, with (221, 222) the unique adjacent pair. Pin the measured
        # tuple, then prove the same three-segment shape fails when one byte of
        # its middle-bound port frame changes.
        self.assertEqual(gc.SCAN3_ALLOWED['gp-u27-name-entry'],
                         ((334, 3, 63,
                           'f1839ec475ffc5228cb673caafb0ac274e3221299b50e1be804ae5da887aef1b'),))
        R = tc.ROW
        c = rgb(1)[:3 * R] + rgb(2)[3 * R:63 * R] + rgb(3)[63 * R:]
        entry = (1, 3, 63, hashlib.sha256(rgb(1)[:3 * R]).hexdigest())
        self.write([1, 2, 3], [rgb(1), c, rgb(2), rgb(3)])
        self.assertEqual(self.run_scan3(s3=(entry,))[:2], (0, None))
        path = os.path.join(self.port, 'frame_00001.ipx')
        with open(path, 'rb') as f:
            mutated = bytearray(f.read())
        mutated[3 * 320 + 17] ^= 1
        with open(path, 'wb') as f:
            f.write(mutated)
        rc, first, out = self.run_scan3(s3=(entry,))
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in line and
                            'no unique adjacent port pair' in line for line in out), out)

    def test_u28_recipe_rejects_one_mutated_bound_byte(self):
        # Live truth Task 5: capture 336 (raw 2825) binds top rows 0..1 to the
        # earlier port 216, middle rows 2..9 to port 218 and bottom rows 10..199
        # to port 219, with (218, 219) the unique adjacent pair.
        self.assertIn('gp-u28-continue', gc.SCAN3_ALLOWED)
        self.assertEqual(gc.SCAN3_ALLOWED['gp-u28-continue'],
                         ((336, 2, 10,
                           '2d6fb6ad829baae61c65a844fb44f05cb51393322afdce4ab8117960e1306ddd'),))
        R = tc.ROW
        c = rgb(1)[:2 * R] + rgb(2)[2 * R:10 * R] + rgb(3)[10 * R:]
        entry = (1, 2, 10, hashlib.sha256(rgb(1)[:2 * R]).hexdigest())
        self.write([1, 2, 3], [rgb(1), c, rgb(2), rgb(3)])
        self.assertEqual(self.run_scan3(s3=(entry,))[:2], (0, None))
        path = os.path.join(self.port, 'frame_00001.ipx')
        with open(path, 'rb') as f:
            mutated = bytearray(f.read())
        mutated[2 * 320 + 17] ^= 1
        with open(path, 'wb') as f:
            f.write(mutated)
        rc, first, out = self.run_scan3(s3=(entry,))
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in line and
                            'no unique adjacent port pair' in line for line in out), out)

    def test_u29_two_segment_recipe_rejects_one_mutated_bound_byte(self):
        # Final truth Task 4 (record §F4): the taken-continue capture's 346
        # (raw 2823) is the same round-1 mode-8 entry's scan-out in the
        # TWO-segment shape (the U9/gp-draw one): top rows 0..1 are the earlier
        # port 218's and rows 2..199 are port 221's through the vacuous middle
        # pair (220, 221); ports 219/220 are unexhibited. PIN plus the
        # fixture's mutation: the real dump is not retained, so the shape's
        # bind is proved here.
        self.assertEqual(gc.SCAN3_ALLOWED['gp-u29-continue-taken'],
                         ((346, 2, 2,
                           '2d6fb6ad829baae61c65a844fb44f05cb51393322afdce4ab8117960e1306ddd'),))
        R = tc.ROW
        c = rgb(1)[:2 * R] + rgb(3)[2 * R:]
        entry = (1, 2, 2, hashlib.sha256(rgb(1)[:2 * R]).hexdigest())
        self.write([1, 2, 3], [rgb(1), c, rgb(3), rgb(3)])
        self.assertEqual(self.run_scan3(s3=(entry,))[:2], (0, None))
        path = os.path.join(self.port, 'frame_00002.ipx')
        with open(path, 'rb') as f:
            mutated = bytearray(f.read())
        mutated[2 * 320 + 17] ^= 1
        with open(path, 'wb') as f:
            f.write(mutated)
        rc, first, out = self.run_scan3(s3=(entry,))
        self.assertEqual((rc, first), (1, 1), out)
        self.assertTrue(any('FAIL: allowed capture 1' in line and
                            'no unique adjacent port pair' in line for line in out), out)

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
        # endgame truth Task 3 (record §E3): TALON's ending's own two three-present
        # scans at the final's mode-0x0D -> 0x0C transitions: capture 4099 (rows 0..63
        # port 3430's, 64..167 port 3431's, 168..199 port 3432's) and 4147 (0..45 port
        # 3474's, 46..168 port 3475's, 169..199 port 3476's). PIN only: the recipe's
        # mutations are proved on the fixture by this class's sub-tests and on the real
        # dump by the manual run record §E3 reports; the dump is not retained, so this
        # case does not run a dump test.
        self.assertEqual(gc.SCAN3_ALLOWED['gp-u18-talon-ending'],
                         ((4099, 64, 168,
                           '2fb5ec8f9b74cc393b7619ad560c9771ff75d987dc1bcdd46c2d751c87c15772'),
                          (4147, 46, 169,
                           'bc171866bbf274469cf629a494a4f948527c292bb9cb87d9b9906d0f8d963704')))
        # endgame truth Task 4 (record §E4): VERTIGO's ending's three scans — 2478 is
        # the health-bonus screen's (rows 0..32 held by 177/2020..2022, 33..147 port
        # 2023's, 148..199 port 2024's) and 4362/4430 are the final's (0..52 port
        # 3577's / 53..117 3578's / 118..199 3579's; and 0..11 port 3640's / 12..145
        # 3641's / 146..199 3642's). PIN only, as the U18 entry: the recipe's mutations
        # are proved on the fixture by this class's sub-tests and on the real dump by
        # the manual run record §E4 reports; the dump is not retained.
        self.assertEqual(gc.SCAN3_ALLOWED['gp-u19-vertigo-ending'],
                         ((2478, 33, 148,
                           '70d994ab367ffc4c077db8b677ae23029f85bb358b43a67efe3badd180e8bc0e'),
                          (4362, 53, 118,
                           '7b1d3e43b68a309a87c71f991a28f9aaf0597d789a50fe82c20deb8eaf0d66b0'),
                          (4430, 12, 146,
                           '83ca9cdcfbc340ebe24f6331d6ebdb6871ec07704108fc1e3bf139704adefdf9')))
        # endgame truth Task 5 (record §E5): CHAOS's ending's three scans — 381 is
        # round 1's mode-8 screen (row 0 port 260's, 1..157 port 263's, 158..199 port
        # 264's), 2350 the health-bonus screen (0..24 the earlier holders, 25..139 port
        # 1932's, 140..199 port 1933's) and 4272 the final's mode-0x0D -> 0x0C
        # transition (0..18 port 3566's, 19..90 port 3567's, 91..199 port 3568's). PIN
        # only, as the U18/U19 entries: the recipe's mutations are proved on the
        # fixture by this class's sub-tests and on the real dump by the manual run
        # record §E5 reports; the dump is not retained.
        self.assertEqual(gc.SCAN3_ALLOWED['gp-u20-chaos-ending'],
                         ((381, 1, 158,
                           '6ac5160d40dd5666f49650e2f9643c27dedceb5565424bb188405f306c1eb936'),
                          (2350, 25, 140,
                           '706572f06c0cdb32aa3e8efd4a52fb42da0bd30f2b4730b9b70ac28987103c68'),
                          (4272, 19, 91,
                           '9744b9869a3b9e4b749ddbdacba395ad527c0f3fbef5e40ff88eae19013e30be')))
        # endgame truth Task 7 (record §E7): DIABLO's ending's six three-present scans —
        # 380 is round 1's mode-8 screen (0..10 port 284's, 11..60 port 286's, 61..199
        # port 287's), 1267 mode 5 (0..24 the earlier holders 218..221 and 1026, 25..178
        # port 1027's, 179..199 port 1028's), 4136/4228 the final (0..23 port 3496's /
        # 24..149 3497's / 150..199 3498's; and 0..41 port 3579's / 42..144 3580's /
        # 145..199 3581's) and 5027/5033 the ending's mode-0x15 fade (0..118 port 4294's /
        # 119..121 4295's / 122..199 4296's; and 0..129 port 4300's / 130..197 4302's /
        # 198..199 4303's, 4301 nowhere on the frame). PIN only, as the U18/U19/U20
        # entries: the recipe's mutations are proved on the fixture by this class's
        # sub-tests and on the real dump by the manual run record §E7 reports; the dump is
        # not retained.
        self.assertEqual(gc.SCAN3_ALLOWED['gp-u22-diablo-ending'],
                         ((380, 11, 61,
                           'e9a10eb395f6b88c1d97654ecd76d736f8f6a08c9c5617867912719dc4be4ccc'),
                          (1267, 25, 179,
                           '9f20c2ce81c05a414a7b6fa5d0b2766944eef64f9ba8fe06b8b74ca70e8bb2a8'),
                          (4136, 24, 150,
                           '221c7921fd2463f65fb8a482b6c928207aaac3bf322b42e6e825abec17483240'),
                          (4228, 42, 145,
                           '13f698a7b1bebc862720e22041862f02b4c5f315540e3e86911a52f7f7370000'),
                          (5027, 119, 122,
                           '4fdc50e09875fafc1716dd46e40518e75cea280a5770b81daca0e8103c542927'),
                          (5033, 130, 198,
                           '9a5cd9bd7b6dc2c14642eab62211a5d64a5928dec2abb29b2002725ee333c17e')))


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
        # Keep the original capture's final rgb(1), with a later identical port
        # occurrence so this allowance fixture is valid under content order too.
        ks = [1, None, None, 9, 1]
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

    def test_overlay_recipe_mutations_fail(self):
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
        self.assertEqual((rc, first), (1, None), out)
        self.assertTrue(any('FAIL:' in l and 'retire the name' in l for l in out), out)

    def test_an_all_black_named_frame_is_a_stale_name(self):
        self.write_fixture(cap_frame=bytes(tc.FRAME_BYTES))
        rc, first, out = self.run_overlay()
        self.assertEqual((rc, first), (1, None), out)
        self.assertTrue(any('named in the overlay allowance but artifact now; retire the name' in l
                            for l in out), out)
        self.assertFalse(any('FIRST UNEXPLAINED capture 1' in l for l in out), out)

    def test_the_overlay_table_is_retired_by_the_music_path(self):
        # Music Truth Task 2 (record §M2.7): 0x1C930's game-side resolve now
        # reads the mode-0x12 entry's song through the resource layer at the
        # entry, so the loader lands on the entry screen in the replay's own
        # frame and every capture frame this table used to name is a clean
        # match. The nine retired names (capture frame): gp-u9-win 2209,
        # gp-u10-ending 2062, gp-u13-ending2 2188, gp-u16-twop2 8066,
        # gp-u18-talon-ending 2227, gp-u19-vertigo-ending 2312,
        # gp-u20-chaos-ending 2180, gp-u21-armadon-ending 2217,
        # gp-u22-diablo-ending 2191. A new name needs a fresh measured recipe.
        self.assertEqual(gc.OVERLAY_ALLOWED, {})


class ApertureDecision(unittest.TestCase):
    # State coverage Task 11 (plan §G7(a)): the 5120 band diagnostic's decision.
    # A synthetic DAC (0x32->black, 0x2E->100000, 0x4D->00000c, 0x33->00001c,
    # 0x00->black, 0x51->the control colour, everything else grey) and the band's
    # measured port/capture colours at the 14 offsets (13 band + 1 control, in
    # gs.BAND_APERTURE order) drive gc.decide_aperture. The four outcomes are the
    # design's PASS-A / PASS-B / FAIL ("both contradicted") plus INCONCLUSIVE for
    # a read that is not a measurement of the video memory at all (the DOSBox-X
    # memory file's unmapped 0xA0000 region returns a uniform byte). The 13 band
    # offsets must be exactly the measured set (8 index-0x32, 1 0x2E, 4 0x4D), so
    # the order in BAND_APERTURE is validated against them here too.
    PX_32 = {(1, 23), (1, 24), (1, 295), (1, 296), (2, 22), (2, 297), (3, 21), (3, 298)}
    PX_2E = {(3, 74)}
    PX_4D = {(4, 27), (4, 28), (4, 291), (4, 292)}
    CTRL = len(gs.BAND_APERTURE) - 1
    BAND = list(range(len(gs.BAND_APERTURE) - 1))

    def setUp(self):
        dac = bytearray(256 * 3)
        for i in range(256):                       # grey: an unmapped index is neither colour
            dac[3 * i:3 * i + 3] = bytes((i, i, i))
        for i, c in ((0x32, b'\x00\x00\x00'), (0x2E, b'\x10\x00\x00'),
                     (0x4D, b'\x00\x00\x0c'), (0x33, b'\x00\x00\x1c'),
                     (0x00, b'\x00\x00\x00'), (0x51, b'\x12\x34\x56')):
            dac[3 * i:3 * i + 3] = c
        self.dac = bytes(dac)
        port_idx, capture = [], []
        for off in gs.BAND_APERTURE:
            if off in self.PX_32:
                port_idx.append(0x32); capture.append(b'\x00\x00\x1c')
            elif off in self.PX_2E:
                port_idx.append(0x2E); capture.append(b'\x00\x00\x00')
            elif off in self.PX_4D:
                port_idx.append(0x4D); capture.append(b'\x00\x00\x00')
            else:                                  # the control: port == capture
                port_idx.append(0x51); capture.append(b'\x12\x34\x56')
        self.port_rgb = [self.dac[3 * i:3 * i + 3] for i in port_idx]
        self.capture_rgb = capture

    def decide(self, ap_idx):
        return gc.decide_aperture(ap_idx, self.dac, self.port_rgb,
                                  self.capture_rgb, self.CTRL)

    def test_the_13_band_offsets_are_the_measured_set(self):
        self.assertEqual(set(gs.BAND_APERTURE[:13]), self.PX_32 | self.PX_2E | self.PX_4D)
        self.assertEqual(gs.BAND_APERTURE[self.CTRL], (1, 100))

    def test_pass_a_the_aperture_holds_the_ports_indices(self):
        ap = [0x32 if o in self.PX_32 else 0x2E if o in self.PX_2E else 0x4D
              for o in gs.BAND_APERTURE[:13]] + [0x51]
        self.assertEqual(self.decide(ap), 'PASS-A')

    def test_pass_b_the_aperture_holds_the_captures_colours(self):
        ap = [0x33 if o in self.PX_32 else 0x00 for o in gs.BAND_APERTURE[:13]] + [0x51]
        self.assertEqual(self.decide(ap), 'PASS-B')

    def test_fail_a_valid_read_that_matches_neither_side(self):
        # A non-uniform read whose control agrees but whose one mutated 0x4D
        # pixel (grey 0x55) matches neither the port nor the capture: both
        # contradicted, the design's FAIL.
        ap = [0x32 if o in self.PX_32 else 0x2E if o in self.PX_2E else 0x4D
              for o in gs.BAND_APERTURE[:13]] + [0x51]
        ap[9] = 0x55                              # one 0x4D pixel -> grey
        self.assertEqual(self.decide(ap), 'FAIL')

    def test_inconclusive_the_control_disagrees_the_read_is_not_video_memory(self):
        # The probe's measured result: every aperture byte is 0xFF (the DOSBox-X
        # memory file holds MemBase, not vgamem). In this DAC DAC[0xFF] is grey,
        # not the control's 0x51 colour, so the read is rejected on the control:
        # not a measurement of the aperture, INCONCLUSIVE, not "both contradicted".
        self.assertEqual(self.decide([0xFF] * 14), 'INCONCLUSIVE')

    def test_inconclusive_the_control_gates_a_band_that_matches_the_port(self):
        # The 13 band offsets match the port's indices, but the control is
        # garbage (0x77 -> grey): the read is not video memory, so INCONCLUSIVE,
        # not PASS-A. This is what makes the control check load-bearing.
        ap = [0x32 if o in self.PX_32 else 0x2E if o in self.PX_2E else 0x4D
              for o in gs.BAND_APERTURE[:13]] + [0x77]
        self.assertEqual(self.decide(ap), 'INCONCLUSIVE')

    def test_inconclusive_a_uniform_read_is_not_video_memory(self):
        # The measured run's actual shape: the read is a single uniform byte whose
        # DAC colour coincidentally equals the control's (0x51 here; 0xFF -> black
        # in the capture's DAC). The control check passes, but the band's 13
        # pixels have >= 2 distinct colours on both sides, so a uniform read
        # cannot be a measurement of them: INCONCLUSIVE, never FAIL.
        self.assertEqual(self.decide([0x51] * 14), 'INCONCLUSIVE')


if __name__ == '__main__':
    unittest.main()
