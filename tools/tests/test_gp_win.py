# tools/tests/test_gp_win.py (gameplay U9/U10: the win path and the ending under pokes)
import io, os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
import gp_win as gw

FIELDS = gs.SNAP_FIELDS + gs.WIN_EXTRA


def _rec(kind, f, mode, kw):
    vals = {n: 0 for n, _, _ in FIELDS}
    vals.update(f=f, mode=mode, t508=1, t50c=2, **kw)
    s = gs.format_s(0, vals, 0, 0x1E, 0x1E, FIELDS)
    return s if kind == 'S' else 'T ' + s[len('S ms=0 '):].rsplit(' kb=', 1)[0]


def _marks(**at):
    """m106/m10a from {land index: byte}."""
    b = bytearray(8)
    for i, v in at.items():
        b[int(i[1:])] = v
    return dict(m106=int.from_bytes(b[:4], 'little'), m10a=int.from_bytes(b[4:], 'little'))


# The U9 path (frames from the port preview of record §W.7): (first f, last f, mode, fields).
U9 = [(0x100, 0x140, 0x03, {}), (0x141, 0x292, 0x27, {}), (0x293, 0x2F5, 0x10, {}),
      (0x2F6, 0x485, 0x17, {}),
      (0x486, 0x48F, 0x06, dict(b1e=1, afc=5)),
      (0x490, 0x853, 0x08, dict(b1e=1, afc=5, s1_5a=0x78, w2=1, ad4=gw.NO_RESULT)),
      (0x854, 0x85D, 0x06, dict(b1e=2, afc=5, w2=1, ad4=gw.NO_RESULT)),
      (0x85E, 0xAB5, 0x09, dict(b1e=2, afc=5, s1_5a=0x78, w2=2, ad4=0)),
      (0xAB6, 0xBA6, 0x17, dict(afc=5, w2=2, **_marks(l5=0x80))),
      (0xBA7, 0xC13, 0x12, dict(afc=5, w2=2, **_marks(l5=0x80))),
      (0xC14, 0xD77, 0x12, dict(afc=5, w2=2, t104=1, **_marks(l5=0x80))),
      (0xD78, 0xDB4, 0x06, dict(b1e=1, afc=3, t104=1, ad4=gw.NO_RESULT, **_marks(l5=0x80)))]


def _log(path, kind='S', drop=lambda f: False, change=None):
    out = []
    for f0, f1, mode, kw in path:
        for f in range(f0, f1 + 1):
            if drop(f):
                continue
            k = dict(kw)
            if change:
                k = change(f, mode, k)
            out.append(_rec(kind, f, mode, k))
    return out


class TestEvidence(unittest.TestCase):
    def test_the_u9_path_passes(self):
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', _log(U9), lambda s: out.write(s + '\n')), 0)
        self.assertIn('8/8 milestones ok', out.getvalue())

    def test_a_wrong_character_fails(self):
        L = _log(U9, change=lambda f, m, k: dict(k, c0=1) if f >= 0x486 else k)
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', L, lambda s: out.write(s + '\n')), 1)
        self.assertIn('FAIL: round 1: P1 is character 0', out.getvalue())

    def test_a_cpu_round_win_fails(self):
        L = _log(U9, change=lambda f, m, k: dict(k, w2=0, w3=1) if m == 0x08 else k)
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', L, lambda s: out.write(s + '\n')), 1)
        self.assertIn('FAIL: round 1 KO', out.getvalue())

    def test_an_unmarked_land_fails(self):
        L = _log(U9, change=lambda f, m, k: dict(k, m10a=0) if m == 0x12 else k)
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', L, lambda s: out.write(s + '\n')), 1)
        self.assertIn('FAIL: the conquered-lands screen', out.getvalue())

    def test_no_win_fields_fails(self):
        L = [l.split(' afc=')[0] + ' kb=0000 head=001E tail=001E' for l in _log(U9)]
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', L, lambda s: out.write(s + '\n')), 1)
        self.assertIn('records no WIN fields', out.getvalue())

    def test_a_death_done_poke_must_find_the_byte_clear(self):
        L = _log([(0x10, 0x20, 0x0D, {})])
        ok = gs.format_w(0, 0x15, 9, 0x104B0C, b'\x00', b'\x01', 0, 0)
        self.assertEqual(gw.death_done_set(L + [ok], gs.snapshots(L)), [])
        bad = gs.format_w(0, 0x16, 9, 0x104B0C, b'\x01', b'\x01', 0, 0)
        self.assertEqual(gw.death_done_set(L + [bad], gs.snapshots(L)), [0x16])
        L2 = _log([(0x10, 0x20, 0x0D, dict(b0c=1))])
        self.assertEqual(gw.death_done_set(L2 + [ok], gs.snapshots(L2)), [0x15])


class TestPath(unittest.TestCase):
    def test_the_same_path_reproduces_every_milestone(self):
        n, rows = gw.reproduced('gp-u9-win', _log(U9, drop=lambda f: f % 3 == 0), _log(U9, 'T'))
        self.assertEqual((n, len(rows)), (8, 8))

    def test_a_late_mode_counts_up_to_it(self):
        port = _log([(f0 + (2 if m == 0x12 else 0), f1, m, k) for f0, f1, m, k in U9], 'T')
        n, rows = gw.reproduced('gp-u9-win', _log(U9), port)
        self.assertEqual(n, 5)                    # the conquered-lands screen is the 6th row
        self.assertEqual((rows[5][1], rows[5][2]), (0xBA7, 0xBA9))

    def test_rows_cover_both_scenarios_in_order(self):
        self.assertEqual(sorted(gw.MILESTONES), ['gp-u10-ending', 'gp-u9-win'])
        for name, rows in gw.MILESTONES.items():
            for label, mode, k, pred in rows:
                self.assertTrue(0 < mode < 0x40 and k >= 1, (name, label))
        u10 = [(m, k) for _, m, k, _ in gw.MILESTONES['gp-u10-ending']]
        self.assertEqual(u10.count((0x0D, 7)), 1)
        self.assertLess(u10.index((0x0D, 1)), u10.index((0x0C, 2)))
        self.assertLess(u10.index((0x0C, 7)), u10.index((0x0D, 7)))
        self.assertEqual(u10[-1], (0x03, 1))


if __name__ == '__main__':
    unittest.main()
