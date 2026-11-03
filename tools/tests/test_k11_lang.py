# tools/tests/test_k11_lang.py (completion truth Task 2, R28: the k11-lang checker)
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import k11_fields as kf
import k11_session as ks
try:
    import k11_lang as kl
except ImportError:
    kl = None

EXE = os.path.join(ROOT, 'data', 'game', 'C', 'PRAGE.EXE')
WIN = kf.WIN_HI - kf.WIN_LO
BIOS = {'down': 0x50E0, 'up': 0x48E0, 'left': 0x4BE0, 'right': 0x4DE0}


def _word(name):
    if name in BIOS:
        return BIOS[name]
    return ks.key_word(name)


def _img(descs, value):
    img = bytearray(WIN)
    kf.set_(img, descs, kl.FIELD, value)
    return bytes(img)


def _log(descs, before=0x142095, after=0x1142095, change_ms=39012,
         drop_key=None, poke=False):
    keys = list(ks.keys_for(kl.SCENARIO))
    if drop_key is not None:
        del keys[drop_key]
    lines = ['P ms=26000 mode=0027']
    for i, k in enumerate(keys):
        lines.append('K ms=%d tick=0 f=0000 key=%04X' % (25002 + i * 1000, _word(k)))
    lines.append('F ms=1000 tick=0 img=%s' % _img(descs, before).hex())
    if after is not None:
        lines.append('F ms=%d tick=0 img=%s' % (change_ms, _img(descs, after).hex()))
    if poke:
        lines.append('W ms=2000 ds=00105208 linear=00105208 old=00 new=01')
    return lines


@unittest.skipIf(kl is None or not os.path.isfile(EXE),
                 'k11_lang or the raw PRAGE.EXE is absent')
class K11LangTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.descs = kf.load_descriptors(EXE)

    def run_check(self, lines):
        out = []
        return kl.check(lines, self.descs, out=out.append), out

    def test_the_capture_state_passes(self):
        rc, out = self.run_check(_log(self.descs))
        self.assertEqual(rc, 0, '\n'.join(out))
        self.assertTrue(any('01142095' in line and '39012' in line for line in out),
                        '\n'.join(out))

    def test_no_language_change_fails(self):
        rc, _ = self.run_check(_log(self.descs, after=None))
        self.assertEqual(rc, 1)

    def test_a_two_step_delta_fails(self):
        rc, _ = self.run_check(_log(self.descs, after=0x142095 + (2 << 24)))
        self.assertEqual(rc, 1)

    def test_a_change_before_the_enter_fails(self):
        rc, _ = self.run_check(_log(self.descs, change_ms=30000))
        self.assertEqual(rc, 1)

    def test_a_missing_key_fails(self):
        rc, _ = self.run_check(_log(self.descs, drop_key=13))
        self.assertEqual(rc, 1)

    def test_a_poke_fails(self):
        rc, _ = self.run_check(_log(self.descs, poke=True))
        self.assertEqual(rc, 1)


if __name__ == '__main__':
    unittest.main()
