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


# A synthetic timeout/health drawn round: rounds 1 and 2 split 1-1 (b1e 1 -> 2,
# ad4 = -1, w2 = w3 = 1), then a round-3 record carrying the timeout precondition
# (the poked clock 0 and both +0x5A bytes at 0x30, equal), then mode 0x0A (ad4 =
# 0xFFFFFFFF, the 1-1 match undecided; flow_round_over cleared the health to 0 and
# reset the clock to 0x1E) and mode 0x0B. The W records mirror the real capture's
# three pokes: round 1's KO_P2 (0x10789E = 0x78, step 4), round 2's KO_P1
# (0x10780A = 0x78, step 5), then the round-3 timeout poke (step 6) -- the clock
# 0x1088F2 = 0 and 0x10780A = health_a, 0x10789E = health_b. The KO health writes
# precede the timeout ones, so a check that reads the first health write would read
# 0x78/0x78 and pass even for a divergent timeout poke; the fix binds the health to
# the clock poke's step.
def _timeout_log(health_a=b'\x30', health_b=b'\x30'):
    out = [_rec(0x480, 0x06, b1e=1, adc=3, ad4=NO_RESULT, w2=0, w3=0),
           _rec(0x84E, 0x06, b1e=2, adc=3, ad4=NO_RESULT, w2=1, w3=1),
           _rec(0xC1C, 0x06, b1e=3, adc=3, ad4=NO_RESULT, w2=1, w3=1,
                s0_5a=0x30, s1_5a=0x30, clock=0),
           _rec(0xD00, 0x0A, b1e=3, adc=3, ad4=NO_RESULT, w2=1, w3=1, clock=0x1E),
           _rec(0xD20, 0x0B, b1e=3, adc=3, ad4=NO_RESULT, w2=1, w3=1, clock=0x1E)]
    out += [gs.format_w(0, 0x489, 4, gd.HEALTH_DS[1], b'\x00', b'\x78', 0, 0),   # round 1 KO_P2
            gs.format_w(0, 0x857, 5, gd.HEALTH_DS[0], b'\x00', b'\x78', 0, 0),   # round 2 KO_P1
            gs.format_w(0, 0xC1C, 6, gd.CLOCK_DS, b'\x1E', b'\x00', 0, 0),
            gs.format_w(0, 0xC1C, 6, gd.HEALTH_DS[0], b'\x00', health_a, 0, 0),
            gs.format_w(0, 0xC1C, 6, gd.HEALTH_DS[1], b'\x00', health_b, 0, 0)]
    return out


def _check_timeout(lines):
    out = io.StringIO()
    rc = gd.evidence_timeout('gp-draw-timeout', lines, lambda s: out.write(s + '\n'))
    return rc, out.getvalue()


class TestTimeoutEvidence(unittest.TestCase):
    def test_a_synthetic_timeout_draw_passes(self):
        rc, text = _check_timeout(_timeout_log())
        self.assertEqual(rc, 0)
        self.assertIn('ad4 = 0xFFFFFFFF, b1e = adc = 3', text)
        self.assertIn('mode 0x0A at f=D00, mode 0x0B at f=D20 ok', text)

    def test_the_ko_draw_log_fails_timeout(self):
        # the two predicates are disjoint: the KO draw's ad4 = 2 is not the timeout's -1
        rc, text = _check_timeout(_log())
        self.assertEqual(rc, 1)
        self.assertIn('no S record has ad4 = 0xFFFFFFFF and mode 0x0A', text)

    def test_the_timeout_log_fails_the_default_check(self):
        # and the other direction: the timeout draw's ad4 = -1 is not the KO draw's 2
        rc, text = _check(_timeout_log())
        self.assertEqual(rc, 1)
        self.assertIn('no S record has ad4 = 2', text)

    def test_b1e_must_equal_adc_three(self):
        rc, text = _check_timeout(
            [l for l in _timeout_log() if ' f=0D00 ' not in l and ' f=0D20 ' not in l]
            + [_rec(0xD00, 0x0A, b1e=2, adc=3, ad4=NO_RESULT, w2=1, w3=1),
               _rec(0xD20, 0x0B, b1e=2, adc=3, ad4=NO_RESULT, w2=1, w3=1)])
        self.assertEqual(rc, 1)
        self.assertIn('not b1e = adc = 3', text)

    def test_the_health_within_three_is_required(self):
        rc, text = _check_timeout(
            [l for l in _timeout_log() if ' f=0D00 ' not in l and ' f=0D20 ' not in l]
            + [_rec(0xD00, 0x0A, b1e=3, adc=3, ad4=NO_RESULT, w2=1, w3=1, s0_5a=0x20, s1_5a=0x00),
               _rec(0xD20, 0x0B, b1e=3, adc=3, ad4=NO_RESULT, w2=1, w3=1)])
        self.assertEqual(rc, 1)
        self.assertIn('|s0_5a - s1_5a| = 32 > 3', text)

    def test_the_clock_poke_is_required(self):
        rc, text = _check_timeout([l for l in _timeout_log() if 'addr=001088F2' not in l])
        self.assertEqual(rc, 1)
        self.assertIn('no W record wrote 0x1088F2 = 0', text)

    def test_the_health_pokes_must_be_within_three(self):
        # the round-1/2 KO pokes (0x78/0x78) precede the timeout poke; a loose `next`
        # would read them and pass even though the timeout poke's health is 0x30/0x40
        # (16 > 3). The fix binds the health to the clock poke's step, so this fails.
        rc, text = _check_timeout(_timeout_log(health_a=b'\x30', health_b=b'\x40'))
        self.assertEqual(rc, 1)
        self.assertIn('more than 3 apart', text)

    def test_the_timeout_health_must_be_below_the_ko(self):
        # a timeout poke whose +0x5A bytes sit at the KO 0x78 is a KO, not a timeout
        # draw (0x27FA8 takes the KO branch when either byte is >= 0x78); the check
        # asserts both below 0x78 even when they are within 3 of each other.
        rc, text = _check_timeout(_timeout_log(health_a=b'\x78', health_b=b'\x78'))
        self.assertEqual(rc, 1)
        self.assertIn('at the KO 0x78', text)


class TestTimeoutScenario(unittest.TestCase):
    def test_the_scenario_pokes_two_kos_then_the_timeout_draw(self):
        scn = gs.SCENARIOS['gp-draw-timeout']
        pokes = [st for st in scn['steps'] if isinstance(st[-1], tuple) and st[-1][0] == 'poke']
        self.assertEqual(pokes, [('after_entry', 0x06, 1, 10, gs.KO_P2),
                                 ('after_entry', 0x06, 2, 10, gs.KO_P1),
                                 ('after_entry', 0x06, 3, 10, gs.TIMEOUT_DRAW_POKE)])
        self.assertEqual(scn['extra'], gs.DRAW_EXTRA)
        self.assertEqual(scn['stop_mode'], 0x0B)
        self.assertEqual(scn['stop_delay'], 60)
        self.assertIn('gp-draw-timeout', gs.STOP_AT_END)

    def test_the_timeout_poke_sets_the_health_and_clock(self):
        self.assertEqual(gs.TIMEOUT_DRAW_POKE[1],
                         ((0x10780A, b'\x30'), (0x10789E, b'\x30'), (0x1088F2, b'\x00')))


if __name__ == '__main__':
    unittest.main()
