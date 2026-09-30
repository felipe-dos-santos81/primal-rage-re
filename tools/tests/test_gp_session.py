# tools/tests/test_gp_session.py
import os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs


class TestTables(unittest.TestCase):
    def test_pad_bits_follow_the_isr_sampler(self):
        # spec §3.2: P1 byte is kb >> 8, P2 byte kb & 0xFF; up..right 0x80..0x10, b0..b3 1..8
        want = {'up': 0x80, 'down': 0x40, 'left': 0x20, 'right': 0x10,
                'b0': 0x01, 'b1': 0x02, 'b2': 0x04, 'b3': 0x08, 'start': 0x01}
        for n, bit in want.items():
            self.assertEqual(gs.PAD['p1.' + n][2], bit << 8, n)
            self.assertEqual(gs.PAD['p2.' + n][2], bit, n)

    def test_pad_scans_are_the_config_table_and_f1_f2(self):
        self.assertEqual([gs.PAD['p1.' + n][0] for n in ('up', 'down', 'left', 'right', 'b0', 'b1', 'b2', 'b3')],
                         [0x1F, 0x2D, 0x2C, 0x2E, 0x16, 0x17, 0x31, 0x32])
        self.assertEqual([gs.PAD['p2.' + n][0] for n in ('up', 'down', 'left', 'right', 'b0', 'b1', 'b2', 'b3')],
                         [0x48, 0x50, 0x4B, 0x4D, 0x47, 0x49, 0x4F, 0x51])
        self.assertEqual((gs.PAD['p1.start'][0], gs.PAD['p2.start'][0]), (0x3B, 0x3C))

    def test_raw_to_kb(self):
        # 0x500C4: raw = (+0x2D8 << 24) | (+0x2D9 << 8)
        self.assertEqual(gs.raw_to_kb(0x81004000), 0x8140)
        self.assertEqual(gs.raw_to_kb(0x00000100), 0x0001)
        self.assertEqual(gs.raw_to_kb(0), 0)

    def test_snap_fields_cover_the_spin_predicate_and_trace(self):
        names = [n for n, _, _ in gs.SNAP_FIELDS]
        for n in ('f', 't508', 't50c', 'raw', 'pad', 'e0', 'e2', 'rng') + gs.TRACE_FIELDS:
            self.assertIn(n, names)
        self.assertEqual(dict((n, (d, s)) for n, d, s in gs.SNAP_FIELDS)['f'], (0x0EF6DC, 2))


class TestFormat(unittest.TestCase):
    def test_s_record_round_trip(self):
        vals = {n: (i * 0x1111) & ((1 << (8 * s)) - 1) for i, (n, _, s) in enumerate(gs.SNAP_FIELDS)}
        line = gs.format_s(1234, vals, 0x4000, 0x1E, 0x20)
        rec = gs.parse(line)
        self.assertEqual(rec['kind'], 'S')
        self.assertEqual(rec['ms'], 1234)
        for n in vals:
            self.assertEqual(rec[n], vals[n], n)
        self.assertEqual((rec['kb'], rec['head'], rec['tail']), (0x4000, 0x1E, 0x20))

    def test_parse_keeps_names_and_decimals(self):
        rec = gs.parse('I ms=10 f=011D step=2 press=p1.up scan=1F lin=00010093 old=FF bios=1F73 ring=1 late=0')
        self.assertEqual((rec['press'], rec['step'], rec['f'], rec['late']), ('p1.up', 2, 0x11D, 0))
        rec = gs.parse('E ms=5 reason=time-limit rc=0')
        self.assertEqual((rec['reason'], rec['rc']), ('time-limit', 0))
        self.assertIsNone(gs.parse('   '))


if __name__ == '__main__':
    unittest.main()
