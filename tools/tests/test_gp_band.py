# tools/tests/test_gp_band.py
"""Final truth Task 2 (record §F2): the holder search and the DAC-injection
decision. Both are read-only comparisons whose thresholds are the measured
values, so the tests seed every side and mutate one at a time."""
import contextlib
import gzip
import io
import os
import sys
import tempfile
import types
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_band as gb
import gp_session as gs

OFFS = gs.BAND_APERTURE
CONTROL = len(OFFS) - 1
# The measured port indices at the band offsets (frame_04316: 8 x 0x32, the
# 0x2E pixel, 4 x 0x4D, control 0x65, record §F2.1).
PORT_IDX = [0x32] * 7 + [0x2E] + [0x32] + [0x4D] * 4 + [0x65]


def frame(indices=None, dac=None):
    """An (idx, dac) pair. The default DAC gives index i the colour (i, 3i, 7i),
    so the rendered value identifies the index (the tests use small indices)."""
    idx = bytearray(64000)
    for k, (r, x) in enumerate(OFFS):
        idx[r * 320 + x] = (indices or PORT_IDX)[k]
    d = bytearray(768)
    for i in range(256):
        d[3 * i:3 * i + 3] = bytes((i, (i * 3) & 0xFF, (i * 7) & 0xFF))
    if dac is not None:
        for i, rgb in dac.items():
            d[3 * i:3 * i + 3] = rgb
    return bytes(idx), bytes(d)


class TestHolderSearch(unittest.TestCase):
    def test_a_whole_band_holder_needs_every_pixel_and_the_control(self):
        idx, dac = frame()
        cap = gb.rendered_at(idx, dac, OFFS)
        h = gb.holder_search(cap, OFFS, [('a', idx, dac)])
        self.assertEqual(h['band_control'], ['a'])
        self.assertEqual(h['band'], ['a'])
        self.assertEqual(h['rows'][1], ['a'])
        for k in range(len(OFFS)):
            self.assertEqual(h['pixels'][k], ['a'], k)
        self.assertEqual(h['control'], ['a'])

    def test_a_single_pixel_difference_drops_the_whole_band_and_its_row(self):
        idx, dac = frame()
        good = gb.rendered_at(idx, dac, OFFS)
        cap = list(good)
        cap[5] = bytes.fromhex('ff00ff')        # (2,297): row 2's second pixel
        h = gb.holder_search(cap, OFFS, [('a', idx, dac)])
        self.assertEqual(h['band'], [])
        self.assertEqual(h['band_control'], [])
        self.assertEqual(h['rows'][1], ['a'])   # row 1's four offsets still match
        self.assertEqual(h['rows'][2], [])      # row 2's two do not
        self.assertEqual(h['pixels'][5], [])    # the mutated pixel is held by none
        self.assertEqual(h['pixels'][4], ['a'])  # its row partner still matches

    def test_one_channel_difference_is_not_a_holder(self):
        # a comparison that read only one channel would wrongly count this frame
        idx, dac = frame()
        good = gb.rendered_at(idx, dac, OFFS)
        cap = list(good)
        cap[0] = bytes((good[0][0], good[0][1], (good[0][2] + 1) & 0xFF))
        h = gb.holder_search(cap, OFFS, [('a', idx, dac)])
        self.assertEqual(h['pixels'][0], [])
        self.assertEqual(h['band'], [])
        self.assertEqual(h['rows'][1], [])

    def test_the_dac_is_the_frame_s_own(self):
        # the comparison is index -> the frame's DAC: the same indices under a DAC
        # whose entry 0x32 differs render a different colour and are not holders
        idx, dac = frame()
        good = gb.rendered_at(idx, dac, OFFS)
        dac2 = dict(((i, bytes((i ^ 0xFF, 0, 0))) for i in (0x32, 0x2E, 0x4D, 0x65)))
        idx2, dacb = frame(dac=dac2)
        h = gb.holder_search(good, OFFS, [('same', idx, dac), ('other_dac', idx2, dacb)])
        self.assertEqual(h['band'], ['same'])
        self.assertEqual(h['pixels'][0], ['same'])

    def test_the_control_is_the_fourteenth_offset(self):
        self.assertEqual(CONTROL, 13)
        self.assertEqual(OFFS[CONTROL], (1, 100))
        # a frame matching only the control is not a band holder
        idx, dac = frame()
        good = gb.rendered_at(idx, dac, OFFS)
        h = gb.holder_search(good, OFFS, [('a', idx, dac)])
        self.assertEqual(h['control'], ['a'])


class TestDecideDac(unittest.TestCase):
    def _sides(self):
        sentinel = [gs.BAND_DAC_SENTINEL_RGB[i] for i in PORT_IDX]
        self.assertTrue(all(s is not None for s in sentinel))
        return PORT_IDX, sentinel

    def test_pass_a_needs_the_back_buffer_and_the_eight_sentinels(self):
        port, sentinel = self._sides()
        self.assertEqual(gb.decide_dac(PORT_IDX, PORT_IDX, sentinel, sentinel, CONTROL, True), 'PASS-A')

    def test_a_back_buffer_difference_is_pass_b_even_with_every_sentinel(self):
        port, sentinel = self._sides()
        bb = list(PORT_IDX)
        bb[6] = 0x40                     # what the original's renderer wrote at (3,21)
        self.assertEqual(gb.decide_dac(bb, port, sentinel, sentinel, CONTROL, True), 'PASS-B')

    def test_no_sentinel_with_the_right_back_buffer_is_the_present_layer(self):
        port, sentinel = self._sides()
        cap = [bytes.fromhex('000000')] * len(OFFS)
        cap[CONTROL] = sentinel[CONTROL]        # the control did turn yellow
        self.assertEqual(gb.decide_dac(port, port, cap, sentinel, CONTROL, True), 'PRESENT-LAYER')

    def test_the_control_must_turn_the_yellow_sentinel(self):
        port, sentinel = self._sides()
        cap = list(sentinel)
        cap[CONTROL] = bytes.fromhex('000000')
        self.assertEqual(gb.decide_dac(port, port, cap, sentinel, CONTROL, True), 'INCONCLUSIVE')

    def test_an_unproven_injection_is_inconclusive(self):
        port, sentinel = self._sides()
        self.assertEqual(gb.decide_dac(port, port, sentinel, sentinel, CONTROL, False), 'INCONCLUSIVE')

    def test_a_non_injected_control_is_not_a_gate(self):
        # the measured gp-u13-band-dac control is index 0x03, not the port's
        # 0x65: it cannot show a sentinel, and the band sentinels prove the
        # injection (record §F2.5)
        port, sentinel = self._sides()
        sentinel = list(sentinel)
        sentinel[CONTROL] = None
        self.assertEqual(gb.decide_dac(port, port, sentinel, sentinel, CONTROL, True), 'PASS-A')


class TestDerivedValues(unittest.TestCase):
    def test_the_sentinel_words_and_their_presented_colours(self):
        # 0x134C0's case-4 body subtracts 8 per channel, then 0x1C470 truncates
        # each to 6 bits and the VGA expands (v<<2)|(v>>4) (record §F2.2).
        self.assertEqual(gs.BAND_DAC_SENTINEL_RGB[0x32], bytes.fromhex('f700f7'))
        self.assertEqual(gs.BAND_DAC_SENTINEL_RGB[0x2E], bytes.fromhex('00f7f7'))
        self.assertEqual(gs.BAND_DAC_SENTINEL_RGB[0x4D], bytes.fromhex('f7f7f7'))
        self.assertEqual(gs.BAND_DAC_SENTINEL_RGB[0x65], bytes.fromhex('f7f700'))

    def test_the_six_writes_are_derived_from_the_two_records(self):
        self.assertEqual(len(gs.BAND_DAC_WRITES), 6)
        self.assertEqual(gs.BAND_DAC_WRITES, (
            (0x000F1B36, b'\x01'),                       # record 2's +0x0E
            (0x000F3372, b'\x01'),                       # record 5's +0x0E
            (0x000F1B80, b'\xff\x00\xff\x00'),           # 0x32 = 0x00FF00FF
            (0x000F1B70, b'\x00\xff\xff\x00'),           # 0x2E = 0x00FFFF00
            (0x000F1BEC, b'\xff\xff\xff\x00'),           # 0x4D = 0x00FFFFFF
            (0x000F33C0, b'\xff\xff\x00\x00'),           # 0x65 = 0x0000FFFF
        ))

    def test_the_scenario_is_u13_with_the_injection_and_the_stop(self):
        sc = gs.SCENARIOS['gp-u13-band-dac']
        u13 = gs.SCENARIOS['gp-u13-ending2']
        self.assertEqual(sc['steps'][:-2], u13['steps'][:-1])
        self.assertEqual(sc['steps'][-2], ('after_entry', 0x15, 1, 82, ('poke', gs.BAND_DAC_WRITES)))
        self.assertEqual(sc['steps'][-1], ('after_entry', 0x15, 1, 84, ('end',)))
        self.assertEqual((sc['time_limit'], sc['memsize']), (240, 64))
        self.assertEqual(sc['extra'], u13['extra'] + gs.BAND_DAC_EXTRA)
        self.assertEqual([n for n, _, _ in gs.BAND_DAC_EXTRA], [
            'bbp0', 'bbp1', 'bb00', 'bb01', 'bb02', 'bb03', 'bb04', 'bb05', 'bb06',
            'bb07', 'bb08', 'bb09', 'bb10', 'bb11', 'bb12', 'bbc',
            'eff0', 'eff1', 'efffree', 'effn'])
        self.assertEqual(dict((n, (d, s)) for n, d, s in gs.BAND_DAC_EXTRA)['bbp0'], (0x000E87A0, 4))
        self.assertEqual(dict((n, (d, s)) for n, d, s in gs.BAND_DAC_EXTRA)['effn'], (0x0009AF3D, 1))
        self.assertEqual(gs.BAND_DAC_EXTRA[2][1], 1 * 320 + 23)
        self.assertEqual(gs.BAND_DAC_EXTRA[15][1], 1 * 320 + 100)
        self.assertFalse(set(n for n, _, _ in gs.BAND_DAC_EXTRA) & gs.APERTURE_FIELD_NAMES)
        self.assertIn('gp-u13-band-dac', gs.STOP_AT_END)

    def test_the_poke_and_stop_are_entry_relative(self):
        # The mode-0x15 entry is run-dependent by one frame (0x17D4 in
        # gp-u13-ending2's capture, 0x17D5 in gp-u13-band's); after_entry is
        # relative, so n=82 always targets entry+82 (the band's phase frame) by
        # firing at the spin entry+81 (the W rows), and n=84 stops at entry+84.
        sc = gs.SCENARIOS['gp-u13-band-dac']
        for entry, poke_f, s_f, stop_f in ((0x17D4, 0x1825, 0x1826, 0x1828),
                                           (0x17D5, 0x1826, 0x1827, 0x1829)):
            F = entry + sc['steps'][-2][3]
            self.assertEqual(F, s_f, hex(entry))           # the iteration that samples it
            self.assertEqual(F - 1, poke_f, hex(entry))    # the W rows' spin
            self.assertEqual(entry + sc['steps'][-1][3], stop_f, hex(entry))

    def test_the_relative_schedule_fires_on_both_measured_entries(self):
        steps = gs.SCENARIOS['gp-u13-band-dac']['steps'][-2:]
        for entry, F, stop in ((0x17D4, 0x1826, 0x1828), (0x17D5, 0x1827, 0x1829)):
            s = gs.Schedule(steps)
            s.entry_frame[(0x15, 1)] = entry
            self.assertEqual(s.due(entry + 81), [(0, ('poke', gs.BAND_DAC_WRITES))])
            self.assertEqual(s.frame_of[0], F)
            self.assertNotIn(1, s.frame_of)                 # the stop is not due yet
            self.assertEqual(s.due(entry + 83), [])         # the end step, not a fired action
            self.assertEqual(s.frame_of[1], stop)

    def test_the_measured_mode_15_entry_frames(self):
        # The review measured 0x17D5 in gp-u13-band; gp-u13-ending2's capture is
        # 0x17D4. Pin both from the committed poll.logs (skip without the data).
        base = os.path.join(ROOT, 'data', 'k11-captures')
        want = {'gp-u13-ending2': 0x17D4, 'gp-u13-band': 0x17D5}
        for cap, f in want.items():
            path = os.path.join(base, cap, 'poll.log')
            if not os.path.isfile(path):
                self.skipTest('no %s capture' % cap)
            first = None
            with open(path) as fh:
                for line in fh:
                    r = gs.parse(line)
                    if r and r['kind'] == 'S' and r['mode'] == 0x15:
                        first = r['f']
                        break
            self.assertEqual(first, f, cap)


def _port_frame_file(path, indices=None):
    """A synthetic U13-dump .ipx: the 14 offsets' indices (the DAC is not read
    by --dac's port side, only the indices)."""
    idx = bytearray(64000)
    for k, (r, x) in enumerate(OFFS):
        idx[r * 320 + x] = (indices or PORT_IDX)[k]
    with open(path, 'wb') as f:
        f.write(bytes(idx) + bytes(768))


def _s_line(f, indices=None, control_bb=None):
    """A synthetic band-dac S record: every field present, the band values from
    the measured port frame. No WIN field matters to --dac; they are zero.
    `control_bb` overrides the control's back-buffer index (the measured capture
    has 0x03 where the port has 0x65)."""
    fields = gs.SNAP_FIELDS + gs.WIN_EXTRA + gs.BAND_DAC_EXTRA
    vals = {n: 0 for n, _, _ in fields}
    vals.update(f=f, mode=0x15)
    for k in range(CONTROL):
        vals['bb%02d' % k] = (indices or PORT_IDX)[k]
    vals['bbc'] = (indices or PORT_IDX)[CONTROL] if control_bb is None else control_bb
    vals.update(bbp0=0x2A6A550, bbp1=0x02A5AB50, eff0=0x000F842C,
                eff1=0x000F1B28, efffree=0x000F0B00, effn=12)
    return gs.format_s(0, vals, 0, 0x1E, 0x1E, fields=fields)


def _cap_frame(path, indices=None, control_rgb=None):
    """A synthetic capture frame whose 14 offsets carry the sentinel colours.
    `control_rgb` overrides the control pixel (the measured capture's index 0x03
    renders black there)."""
    rgb = bytearray(320 * 200 * 3)
    for k, (r, x) in enumerate(OFFS):
        rgb[3 * (r * 320 + x):3 * (r * 320 + x) + 3] = gs.BAND_DAC_SENTINEL_RGB[(indices or PORT_IDX)[k]]
    if control_rgb is not None:
        r, x = OFFS[CONTROL]
        rgb[3 * (r * 320 + x):3 * (r * 320 + x) + 3] = control_rgb
    with gzip.open(path, 'wb') as f:
        f.write(bytes(rgb))


class TestDacMain(unittest.TestCase):
    """The whole --dac path, not only decide_dac (review round 1, Critical 1):
    the capture also carries its route's win-path W rows, so the injected gate
    must filter to the six BAND_DAC_WRITES addresses, and the S frame is derived
    from the diagnostic rows' f + 1 (the entry frame is run-dependent)."""

    def _run(self, diag_rows, prior_rows=(), s_f=0x1827, control_bb=None,
             control_rgb=None):
        with tempfile.TemporaryDirectory() as d:
            poll = os.path.join(d, 'poll.log')
            with open(poll, 'w') as f:
                for line in prior_rows:
                    f.write(line + '\n')
                for line in diag_rows:
                    f.write(line + '\n')
                f.write(_s_line(s_f, control_bb=control_bb) + '\n')
            cap = os.path.join(d, 'cap')
            os.makedirs(cap)
            _cap_frame(os.path.join(cap, 'frame_00000.raw.gz'), control_rgb=control_rgb)
            pf = os.path.join(d, 'frame_04316.ipx')
            _port_frame_file(pf)
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                rc = gb.dac_main(types.SimpleNamespace(
                    poll_log=poll, capture=cap, port_frame=pf, f=None))
            return rc, buf.getvalue()

    def _diag(self, frame=0x1826, **kw):
        return [gs.format_w(0, frame, 0, addr, b'\x00' * len(data), data,
                            kw.get('late', 0), kw.get('race', 0))
                for addr, data in gs.BAND_DAC_WRITES]

    def test_dac_main_filters_the_route_w_rows(self):
        # the exact Critical 1 case: a prior U13 win-path W row before the six
        for prior in (gs.format_w(0, 0x4D9, 0, 0x108106, b'\x00' * 7, b'\x81' * 7, 0, 0),
                      gs.format_w(0, 0x1502, 0, 0x104B0C, b'\x00', b'\x01', 0, 0)):
            rc, out = self._run(self._diag(), prior_rows=[prior])
            self.assertEqual(rc, 0)
            self.assertIn('W rows: 6 diagnostic (of 7 total)', out)
            self.assertIn('frame(s) 1826', out)
            self.assertIn('S at f=1827', out)
            self.assertIn('verdict: PASS-A', out)

    def test_dac_main_derives_the_s_frame_from_the_w_frame(self):
        # the older-entry run's frame (W 0x1825 -> S 0x1826)
        rc, out = self._run(self._diag(frame=0x1825), s_f=0x1826)
        self.assertIn('S at f=1826', out)
        self.assertIn('verdict: PASS-A', out)

    def test_dac_main_accepts_a_non_injected_control(self):
        # the measured capture: the control's back-buffer index is 0x03 (the
        # port's 0x65), so it cannot show the 0x65 sentinel; the band sentinels
        # locate the frame and prove the injection (record §F2.5)
        rc, out = self._run(self._diag(), control_bb=0x03,
                            control_rgb=bytes.fromhex('000000'))
        self.assertIn('control (1,100): bb=03 port_idx=65 sentinel=none', out)
        self.assertIn('capture frames showing an injected sentinel: 1', out)
        self.assertIn('verdict: PASS-A', out)

    def test_dac_main_rejects_a_raced_diagnostic_write(self):
        rc, out = self._run(self._diag(race=1))
        self.assertIn('raced 6', out)
        self.assertIn('verdict: INCONCLUSIVE', out)

    def test_dac_main_rejects_a_late_diagnostic_write(self):
        rc, out = self._run(self._diag(late=1))
        self.assertIn('late 6', out)
        self.assertIn('verdict: INCONCLUSIVE', out)

    def test_dac_main_rejects_a_missing_diagnostic_write(self):
        rows = self._diag()[:-1]
        rc, out = self._run(rows)
        self.assertIn('W rows: 5 diagnostic (of 5 total)', out)
        self.assertIn('verdict: INCONCLUSIVE', out)

    def test_dac_main_reports_a_missing_s_record(self):
        rc, out = self._run(self._diag(), s_f=0x1830)
        self.assertIn('no S record at f=1827', out)
        self.assertIn('verdict: INCONCLUSIVE', out)


if __name__ == '__main__':
    unittest.main()
