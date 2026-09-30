# tools/tests/test_k11_fields.py
import os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import k11_fields as kf

EXE = os.path.join(ROOT, 'data', 'game', 'C', 'PRAGE.EXE')


def blank():
    return bytearray(kf.WIN_HI - kf.WIN_LO)


class SyntheticDescriptor(unittest.TestCase):
    """Hand-computed from the 0x2D974/0x2DA0C listings, independent of the exe."""
    # 2 nibbles at nibble offset 3, no byte part: (1 << 14) | (3 << 6)
    D = [0] * 63
    D[1] = (1 << 14) | (3 << 6)
    # 1 nibble at nibble offset 4 plus byte-table entry 5: (0 << 14) | (4 << 6) | 5
    D[2] = (4 << 6) | 5

    def test_get_reads_the_listing_order(self):
        img = blank()
        img[kf.NIB_DS + 1 - kf.WIN_LO] = 0xC0     # high nibble C read last (0x2D9CD)
        img[kf.NIB_DS + 2 - kf.WIN_LO] = 0x0D     # low nibble D read first (0x2D9AB)
        self.assertEqual(kf.get(img, self.D, 1), 0xDC)

    def test_set_then_get_and_neighbour_nibbles_kept(self):
        img = blank()
        img[kf.NIB_DS + 1 - kf.WIN_LO] = 0x07     # low nibble belongs to a neighbour
        img[kf.NIB_DS + 2 - kf.WIN_LO] = 0x90     # high nibble belongs to a neighbour
        self.assertEqual(kf.set_(img, self.D, 1, 0xAB), 0)
        self.assertEqual(img[kf.NIB_DS + 1 - kf.WIN_LO], 0xB7)   # 0x2DA90
        self.assertEqual(img[kf.NIB_DS + 2 - kf.WIN_LO], 0x9A)   # 0x2DAAE
        self.assertEqual(kf.get(img, self.D, 1), 0xAB)

    def test_byte_part_is_the_low_byte(self):
        img = blank()
        kf.set_(img, self.D, 2, 0x3C5)
        self.assertEqual(img[kf.BYTES_DS + 5 - kf.WIN_LO], 0xC5)  # 0x2DA34
        self.assertEqual(kf.get(img, self.D, 2), 0x3C5)

    def test_out_of_range_field(self):
        self.assertEqual(kf.get(blank(), self.D, 0x3F), 0xFFFFFFFF)
        self.assertEqual(kf.set_(blank(), self.D, 0x3F, 1), -1)


@unittest.skipUnless(os.path.isfile(EXE) or os.environ.get('PR_ORACLE_REQUIRED') == '1',
                     'data/game/C/PRAGE.EXE absent')
class RealDescriptors(unittest.TestCase):
    def setUp(self):
        self.d = kf.load_descriptors(EXE)

    def test_descriptors_match_the_k11_record(self):
        self.assertEqual(self.d[0x35], 0xE0C0)                     # K11 record §K11.4
        widths = {3: 20, 4: 20, 5: 20, 6: 16, 7: 16, 8: 16, 9: 16, 0xA: 24,
                  0xB: 16, 0xC: 24, 0xD: 16, 0x11: 16, 0x12: 32, 0x13: 32,
                  0x29: 32, 0x2A: 4}                               # §K11.7, §A.1
        for f, bits in widths.items():
            self.assertEqual(kf.width(self.d[f]), bits, hex(f))

    def test_round_trip_and_isolation_for_every_field(self):
        for f in range(0x3F):
            bits = kf.width(self.d[f])
            mask = (1 << bits) - 1 if bits < 32 else 0xFFFFFFFF
            for v in (0, 1, mask, 0xA5A5A5A5 & mask):
                img = bytearray(b'\x5a' * (kf.WIN_HI - kf.WIN_LO))
                before = {g: kf.get(img, self.d, g) for g in range(0x3F)}
                kf.set_(img, self.d, f, v)
                self.assertEqual(kf.get(img, self.d, f), v, (hex(f), hex(v)))
                for g in range(0x3F):
                    if g != f and not self._overlap(f, g):
                        self.assertEqual(kf.get(img, self.d, g), before[g], (hex(f), hex(g)))
                self.assertEqual(img[kf.DD8_DS - kf.WIN_LO], 0x5A)  # dirty bits untouched

    def test_field_2a_keeps_four_bits(self):
        img = bytearray(kf.WIN_HI - kf.WIN_LO)
        kf.set_(img, self.d, 0x2A, 0x13)
        self.assertEqual(kf.get(img, self.d, 0x2A), 0x3)          # bit 4 cannot be stored

    def _overlap(self, f, g):
        def cells(x):
            s = set()
            start, n = (x >> 6) & 0xFF, ((x >> 14) & 7) + 1
            s.update(('n', i) for i in range(start, start + n))
            if x & 0x3F:
                s.add(('b', x & 0x3F))
            return s
        return bool(cells(self.d[f]) & cells(self.d[g]))


if __name__ == '__main__':
    unittest.main()
