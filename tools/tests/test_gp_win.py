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


ALL7 = _marks(l0=0x80, l1=0x80, l2=0x80, l3=0x80, l4=0x80, l5=0x80, l6=0x80)


def _u10(stale_k=None, order_swap=False, held=7, c82=1, ending_c0=0):
    """The U10 path as (n frames, mode, fields) segments, numbered on from f = 0x100: the
    U9 path to mode 9 with the seven lands poked, the tour's end, the final (seven
    opponents), mode 0xF, the ending and the high-score entry. Returns [(f0, f1, mode, kw)]
    and the first f of every mode-0xD entry."""
    segs = [(0x41, 0x03, {}), (0x152, 0x27, {}), (0x63, 0x10, {}),
            (0x20, 0x06, dict(b1e=1, afc=5)),
            (0x20, 0x08, dict(b1e=1, afc=5, s1_5a=0x78, w2=1, ad4=gw.NO_RESULT, **ALL7)),
            (0x20, 0x06, dict(b1e=2, afc=5, w2=1, ad4=gw.NO_RESULT, **ALL7)),
            (0x20, 0x09, dict(b1e=2, afc=5, s1_5a=0x78, w2=2, ad4=0, **ALL7)),
            (0x20, 0x17, dict(afc=5, w2=2, **ALL7)),
            (0x10, 0x12, dict(afc=5, w2=2, **ALL7))]
    if order_swap:      # the marks cleared and the world-domination count before the seventh land
        segs += [(0x10, 0x12, dict(afc=5, w2=2, c82=1, t104=0)), (0x10, 0x12, dict(afc=5, w2=2, c82=1, t104=7))]
    else:
        segs += [(0x10, 0x12, dict(afc=5, w2=2, t104=held, **ALL7)),
                 (0x10, 0x12, dict(afc=5, w2=2, t104=held, c82=c82))]
    segs += [(0x10, 0x23, dict(afc=5)), (0x10, 0x22, dict(afc=5)), (0x10, 0x24, dict(afc=7)),
             (0x10, 0x0C, dict(afc=7, b14=1, b21=0))]
    for k in range(1, 8):
        segs.append((0x10, 0x0D, dict(afc=7, b14=1, s1_5a=0x78, b21=stale_k if k == stale_k else k - 1)))
        if k < 7:
            segs.append((0x10, 0x0C, dict(afc=7, b14=1, b21=k)))
    segs += [(0x10, 0x0F, dict(afc=7, b21=7)), (0x10, 0x15, {}),
             (0x10, 0x1F, dict(c0=ending_c0)), (0x10, 0x15, {}), (0x10, 0x1F, dict(c0=ending_c0)),
             (0x10, 0x15, {}), (0x10, 0x1E, {}), (0x10, 0x03, {})]
    out, f, d = [], 0x100, []
    for n, mode, kw in segs:
        if mode == 0x0D:
            d.append(f + 5)         # the poke 5 frames into the entry
        out.append((f, f + n - 1, mode, dict(kw)))
        f += n
    return out, d


def _pokes(d):
    return [gs.format_w(0, f, 9, 0x104B0C, b'\x00', b'\x01', 0, 0) for f in d]


class TestU10(unittest.TestCase):
    def check(self, **kw):
        path, d = _u10(**kw)
        out = io.StringIO()
        rc = gw.evidence('gp-u10-ending', _log(path) + _pokes(d), lambda s: out.write(s + '\n'))
        return rc, out.getvalue()

    def test_the_u10_path_passes_and_reproduces(self):
        rc, text = self.check()
        self.assertEqual(rc, 0)
        self.assertIn('30/30 milestones ok', text)
        path, d = _u10()
        n, rows = gw.reproduced('gp-u10-ending', _log(path, drop=lambda f: f % 3 == 0), _log(path, 'T'))
        self.assertEqual((n, len(rows)), (30, 30))
        out = io.StringIO()
        self.assertEqual(gw.path('gp-u10-ending', _log(path), _log(path, 'T'), 30, None,
                                 lambda s: out.write(s + '\n')), 0)
        self.assertIn('0 not reproduced through 29; ratchet N 30 ok', out.getvalue())

    def test_a_stale_kill_count_fails(self):
        rc, text = self.check(stale_k=3)
        self.assertEqual(rc, 1)
        self.assertIn("FAIL: final opponent 3 KO'd", text)

    def test_the_seventh_land_is_required(self):
        rc, text = self.check(held=6)
        self.assertEqual(rc, 1)
        self.assertIn('FAIL: 0x41C28 state 3 counts the seventh land', text)

    def test_world_domination_needs_its_count(self):
        rc, text = self.check(c82=0)
        self.assertEqual(rc, 1)
        self.assertIn('FAIL: WORLD DOMINATION', text)

    def test_milestones_come_in_order(self):
        rc, text = self.check(order_swap=True)
        self.assertEqual(rc, 1)
        self.assertIn('before the previous milestone', text)

    def test_the_ending_must_be_saurons(self):
        rc, text = self.check(ending_c0=3)
        self.assertEqual(rc, 1)
        self.assertIn("FAIL: SAURON's ending", text)

    def test_a_consumed_death_done_byte_fails(self):
        path, d = _u10()
        # the poke landed after mode 0xD consumed the game's own byte: the S record at the
        # poke is already in mode 0xC (or 0xD with a moved KO count) and finds the byte clear
        out = io.StringIO()
        late = [f + 0x10 for f in d[:1]] + d[1:]
        self.assertEqual(gw.evidence('gp-u10-ending', _log(path) + _pokes(late), lambda s: out.write(s + '\n')), 1)
        self.assertIn('did not find a clear byte in the mode-0xD entry', out.getvalue())
        L = _log([(0x10, 0x14, 0x0D, dict(b21=1)), (0x15, 0x19, 0x0D, dict(b21=2))])
        self.assertEqual(gw.death_done_set(_pokes([0x17]), gs.snapshots(L)), [0x17])
        self.assertEqual(gw.death_done_set(_pokes([0x12]), gs.snapshots(L)), [])


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
        L = _log(U9, change=lambda f, m, k: dict(k, w2=1, w3=1) if m == 0x08 else k)
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
    def test_a_capture_without_mode_27_fails_cleanly(self):
        cap = _log([(0x100, 0x140, 0x03, {}), (0x141, 0x150, 0x10, {})])
        self.assertEqual(gw.reproduced('gp-u9-win', cap, _log(U9, 'T'))[0], 0)
        out = io.StringIO()
        self.assertEqual(gw.path('gp-u9-win', cap, _log(U9, 'T'), 0, None, lambda s: out.write(s + '\n')), 1)
        self.assertIn('FAIL: mode 0x27 never observed', out.getvalue())

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
