# tools/tests/test_gp_band.py
"""Final truth Task 2 (record §F2): the holder search and the DAC-injection
decision. Both are read-only comparisons whose thresholds are the measured
values, so the tests seed every side and mutate one at a time."""
import os
import sys
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

    def test_a_control_index_without_a_sentinel_is_inconclusive(self):
        port, sentinel = self._sides()
        sentinel = list(sentinel)
        sentinel[CONTROL] = None
        self.assertEqual(gb.decide_dac(port, port, sentinel, sentinel, CONTROL, True), 'INCONCLUSIVE')


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

    def test_the_frame_reaches_the_band_s_record(self):
        # the relative steps are pinned to the mode-0x15 entry (f=0x17D4 in the
        # U13 capture): 82 -> the poke's spin at f=0x1825, 84 -> the stop at
        # f=0x1828 (record §F2.3)
        sc = gs.SCENARIOS['gp-u13-band-dac']
        entry = 0x17D4
        self.assertEqual(entry + sc['steps'][-2][3], 0x1826)     # the iteration that samples it
        self.assertEqual(entry + sc['steps'][-1][3], 0x1828)


if __name__ == '__main__':
    unittest.main()
