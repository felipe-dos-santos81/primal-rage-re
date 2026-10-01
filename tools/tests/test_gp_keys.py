# tools/tests/test_gp_keys.py — U11 in-match keys (record 2026-10-01-gameplay-u11 §K)
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_capture as gc
import gp_session as gs

FIELDS = gs.SNAP_FIELDS + gs.SCENARIOS['gp-keys-fight']['extra']
STEPS = gs.SCENARIOS['gp-keys-fight']['steps']
BASE = 0x266000


class TestKeys(unittest.TestCase):
    def test_keys_are_bios_make_words(self):
        # scan << 8 | ascii; an Alt-letter has ascii 0 (the 0x24D96 arms)
        self.assertEqual([gs.KEYS[k] for k in ('space', 'y', 'n', 'alt-q', 'alt-s', 'alt-m')],
                         [(0x39, 0x3920), (0x15, 0x1579), (0x31, 0x316E),
                          (0x10, 0x1000), (0x1F, 0x1F00), (0x32, 0x3200)])
        self.assertEqual(gs.KEYS['enter'], (0x1C, 0x1C0D))         # unchanged
        self.assertEqual(gs.KEYS['esc'], (0x01, 0x011B))

    def test_an_answer_fires_with_its_opener(self):
        s = gs.Schedule(STEPS)
        s.due_boot(gs.ENTER_WAIT)
        s.on_mode(0x141, 0x27)
        s.due(0x141 + 149)
        s.due(0x141 + 299)
        s.on_mode(0x7F5, 0x06)
        self.assertEqual([x[0] for x in s.due(0x7F5 + 9)], [3])
        self.assertEqual([x[0] for x in s.due(0x7F5 + 19)], [4, 5])      # space, space in one spin
        self.assertEqual([x[0] for x in s.due(0x7F5 + 29)], [6])
        self.assertEqual([x[0] for x in s.due(0x7F5 + 39)], [7, 8])      # esc, n
        self.assertEqual(s.total, 18)

    def test_extra_fields_are_read_and_logged(self):
        m = bytearray(0x400000)
        o = BASE + 0x105F30 - gs.DATA_BASE_VA
        m[o:o + 4] = (0x1B).to_bytes(4, 'little')
        m[BASE + 0x1028DA - gs.DATA_BASE_VA] = 1
        v = gc.read_snap(m, BASE, FIELDS)
        self.assertEqual((v['lat'], v['mpz'], v['spz']), (0x1B, 1, 0))
        self.assertNotIn('lat', gc.read_snap(m, BASE))                    # the default is unchanged
        rec = gs.parse(gs.format_s(0, dict(v, f=7), 0, 0x1E, 0x1E, FIELDS))
        self.assertEqual((rec['lat'], rec['mpz']), (0x1B, 1))
        self.assertNotIn('lat', gs.parse(gs.format_s(0, v, 0, 0x1E, 0x1E)))


if __name__ == '__main__':
    unittest.main()
