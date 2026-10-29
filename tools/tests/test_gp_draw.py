# tools/tests/test_gp_draw.py (claim coverage Task 11: the drawn match 0x0A/0x0B)
import io, os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_draw as gd
import gp_session as gs

FIELDS = gs.SNAP_FIELDS + gs.DRAW_EXTRA
NO_RESULT = 0xFFFFFFFF          # DS_00104AD4 = -1: 0x27BA4's undecided match (0x25A0E)


def _rec(f, mode, **kw):
    vals = {n: 0 for n, _, _ in FIELDS}
    vals.update(f=f, mode=mode, t508=1, t50c=2, **kw)
    return gs.format_s(0, vals, 0, 0x1E, 0x1E, FIELDS)


# A synthetic drawn match: three mode-6 rounds (rounds 1 and 2 split 1-1, so B1E = 3 is
# the deciding round), then mode 0x0A (the round-over, flow_round_over 0x27ED8) and mode
# 0x0B (the winner pose, 0x28C38). The KO bytes s0_5a/s1_5a are left at 0: flow_round_over's
# 0x1D764 clears both +0x5A bytes before it stores mode 0x0A (record §48-K), so a draw's
# mode-0x0A record never shows them at 0x78.
def _log(a_kw=None, b_kw=None, include_a=True, include_b=True):
    out = [_rec(0x480, 0x06, b1e=1, adc=3, ad4=NO_RESULT, w2=0, w3=0),
           _rec(0x84E, 0x06, b1e=2, adc=3, ad4=NO_RESULT, w2=1, w3=1),
           _rec(0xC1C, 0x06, b1e=3, adc=3, ad4=NO_RESULT, w2=1, w3=1)]
    if include_a:
        a = dict(b1e=3, adc=3, ad4=2, w2=1, w3=1)
        a.update(a_kw or {})
        out.append(_rec(0xD00, 0x0A, **a))
    if include_b:
        b = dict(b1e=3, adc=3, ad4=2, w2=1, w3=1)
        b.update(b_kw or {})
        out.append(_rec(0xD20, 0x0B, **b))
    return out


def _check(lines):
    out = io.StringIO()
    rc = gd.evidence('gp-draw', lines, lambda s: out.write(s + '\n'))
    return rc, out.getvalue()


class TestEvidence(unittest.TestCase):
    def test_a_synthetic_draw_passes(self):
        rc, text = _check(_log())
        self.assertEqual(rc, 0)
        self.assertIn('ad4 = 2, b1e = adc = 3, w2 = w3 = 1', text)
        self.assertIn('mode 0x0A at f=D00, mode 0x0B at f=D20 ok', text)

    def test_a_win_is_not_a_draw(self):
        rc, text = _check(_log(a_kw=dict(ad4=0), b_kw=dict(ad4=0)))
        self.assertEqual(rc, 1)
        self.assertIn("no S record has ad4 = 2", text)

    def test_an_undecided_match_is_not_a_draw(self):
        # the brief's "two KOs" leave the match 1-1 at B1E = 2 with ad4 = -1, not 2:
        # 0x27BA4 computes d = ADC - B1E = 1 and neither counter leads by more than d.
        rc, text = _check([_rec(0x480, 0x06, b1e=1, adc=3, ad4=NO_RESULT, w2=0, w3=0),
                           _rec(0x84E, 0x06, b1e=2, adc=3, ad4=NO_RESULT, w2=1, w3=1),
                           _rec(0xC1C, 0x06, b1e=3, adc=3, ad4=NO_RESULT, w2=1, w3=1)])
        self.assertEqual(rc, 1)
        self.assertIn("no S record has ad4 = 2", text)

    def test_the_deciding_round_must_be_the_final(self):
        # ad4 = 2 at B1E = 2 != ADC = 3 is not a draw (0x27BA4 draws only at B1E == ADC)
        rc, text = _check(_log(a_kw=dict(b1e=2)))
        self.assertEqual(rc, 1)
        self.assertIn('has b1e = 2, not adc = 3', text)

    def test_the_round_win_counters_must_be_level(self):
        rc, text = _check(_log(a_kw=dict(w2=2, w3=1)))
        self.assertEqual(rc, 1)
        self.assertIn('has w2 = 2 != w3 = 1', text)

    def test_mode_0a_is_required(self):
        rc, text = _check(_log(include_a=False))
        self.assertEqual(rc, 1)
        self.assertIn('mode 0x0A', text)

    def test_mode_0b_is_required(self):
        rc, text = _check(_log(include_b=False))
        self.assertEqual(rc, 1)
        self.assertIn('mode 0x0B', text)

    def test_no_draw_fields_fails(self):
        # a capture that logged WIN_EXTRA but not adc records no DRAW fields
        L = [l.split(' adc=')[0] + ' kb=0000 head=001E tail=001E' for l in _log()]
        rc, text = _check(L)
        self.assertEqual(rc, 1)
        self.assertIn('records no DRAW fields', text)


class TestScenario(unittest.TestCase):
    def test_the_scenario_pokes_one_ko_each_way_then_both(self):
        scn = gs.SCENARIOS['gp-draw']
        pokes = [st for st in scn['steps'] if isinstance(st[-1], tuple) and st[-1][0] == 'poke']
        self.assertEqual(pokes, [('after_entry', 0x06, 1, 10, gs.KO_P2),
                                 ('after_entry', 0x06, 2, 10, gs.KO_P1),
                                 ('after_entry', 0x06, 3, 10, gs.KO_BOTH)])
        self.assertEqual(scn['extra'], gs.DRAW_EXTRA)
        self.assertEqual(scn['stop_mode'], 0x0B)
        self.assertIn('gp-draw', gs.STOP_AT_END)

    def test_the_draw_extra_adds_adc_to_win_extra(self):
        self.assertEqual([n for n, _, _ in gs.DRAW_EXTRA], [n for n, _, _ in gs.WIN_EXTRA] + ['adc'])
        by_name = {n: addr for n, addr, _ in gs.DRAW_EXTRA}
        self.assertEqual(by_name['adc'], 0x104ADC)
        self.assertEqual(by_name['ad4'], 0x104AD4)


if __name__ == '__main__':
    unittest.main()
