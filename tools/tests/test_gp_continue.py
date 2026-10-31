# tools/tests/test_gp_continue.py (live truth Task 5: the continue screen 0x0E;
# final truth Task 4: the taken-continue branch 0x2791C)
import io
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
import gp_continue as gc

FIELDS = gs.SNAP_FIELDS + gs.WIN_EXTRA


def _rec(kind, f, mode, **kw):
    vals = {n: 0 for n, _, _ in FIELDS}
    vals.update(dict(f=f, mode=mode, t508=1, t50c=2, cred=4))
    vals.update(kw)
    s = gs.format_s(0, vals, 0, 0x1E, 0x1E, FIELDS)
    return s if kind == 'S' else 'T ' + s[len('S ms=0 '):].rsplit(' kb=', 1)[0]


def _log(kind='S'):
    out = [] if kind == 'T' else ['B ms=0 base=00266000 ptr=0000FE20']
    out.append(_rec(kind, 0x14B6, 0x0C, w10d=0, cnt=0, b25=3))
    out.append(_rec(kind, 0x14C0, 0x0E, w10d=0, cnt=0x0F, b25=0, b1f=0))
    for n in range(0x0E, -1, -1):
        f = 0x14C0 + (0x0F - n) * 0x40
        out.append(_rec(kind, f, 0x0E, w10d=0, cnt=n, b25=0, b1f=0))
    out.append(_rec(kind, 0x18C1, 0x15, w10d=0, cnt=0xFF, b25=0, b1f=0))
    if kind == 'S':
        out.append(gs.format_w(0, 0x14BF, 6, 0x10780A, b'\x00', b'\x78', 0, 0))
    return out


def _w(f, addr, now):
    return gs.format_w(0, f, 0, addr, b'\x00', now, 0, 0)


def _taken_log(kind='S', take_cnt=0x0D, take_b1f=1, take_cred=3, take_s0_5a=0,
               post=0x40):
    """A taken screen: mode 0x0E opens at 0x14C0 (cnt 0x0F), reaches the prompt
    arm at 0x1540 (cnt 0x0D), is taken one frame after a prompt frame, and the
    restarted round runs `post` records of mode 0x0C. The route's four harness
    writes are the W rows."""
    out = [] if kind == 'T' else ['B ms=0 base=00266000 ptr=0000FE20']
    out.append(_rec(kind, 0x14B6, 0x0C, w10d=0, cnt=0, b25=3))
    if take_cnt == 0x0F:
        out.append(_rec(kind, 0x14C0, 0x0E, w10d=0, cnt=0x0F, b25=3, b1f=0, s0_5a=0x78))
        take_f = 0x14C1
    else:
        out.append(_rec(kind, 0x14C0, 0x0E, w10d=0, cnt=0x0F, b25=3, b1f=0, s0_5a=0x78))
        out.append(_rec(kind, 0x1500, 0x0E, w10d=0, cnt=0x0E, b25=3, b1f=0, s0_5a=0x78))
        out.append(_rec(kind, 0x1540, 0x0E, w10d=0, cnt=0x0D, b25=3, b1f=0, s0_5a=0x78))
        out.append(_rec(kind, 0x1560, 0x0E, w10d=0, cnt=0x0D, b25=3, b1f=0, s0_5a=0x78))
        take_f = 0x1561
    out.append(_rec(kind, take_f, 0x0C, w10d=0, cnt=take_cnt, b25=3,
                    b1f=take_b1f, cred=take_cred, s0_5a=take_s0_5a))
    for n in range(1, post):
        out.append(_rec(kind, take_f + n, 0x0C, w10d=0, cnt=take_cnt, b25=3,
                        b1f=take_b1f, cred=take_cred))
    if kind == 'S':
        out += [_w(0x47F, 0x108106, b'\x80' * 7), _w(0x47F, 0x10789E, b'\x78'),
                _w(0x84D, 0x10789E, b'\x78'), _w(0x14BF, 0x10780A, b'\x78')]
    return out


def _check(lines):
    out = io.StringIO()
    if not callable(getattr(gc, 'evidence', None)):
        return 1, 'evidence missing'
    rc = gc.evidence('gp-u28-continue', lines, lambda s: out.write(s + '\n'))
    return rc, out.getvalue()


def _check_taken(lines):
    out = io.StringIO()
    rc = gc.evidence(gc.TAKEN_SCENARIO, lines, lambda s: out.write(s + '\n'), taken=True)
    return rc, out.getvalue()


class TestEvidence(unittest.TestCase):
    def test_the_full_prompt_countdown_and_expiry_pass(self):
        rc, text = _check(_log())
        self.assertEqual(rc, 0, text)
        self.assertIn('countdown 15->0 (16/16 values)', text)
        self.assertIn('PRESS START prompt path at f=1540 (cnt=13 < 14', text)
        self.assertIn('mode 0x15, b25=0', text)

    def test_the_ko_must_be_on_the_incumbent_side(self):
        lines = [l.replace('w10d=00', 'w10d=01') for l in _log()]
        rc, text = _check(lines)
        self.assertEqual(rc, 1)
        self.assertIn('w10d=1', text)

    def test_the_complete_descending_countdown_is_required(self):
        lines = [l for l in _log() if ' cnt=0D ' not in l]
        rc, text = _check(lines)
        self.assertEqual(rc, 1)
        self.assertIn('countdown', text)

    def test_a_credit_spend_is_not_the_prompt_path(self):
        lines = [l.replace('cred=00000004', 'cred=00000003') if ' cnt=0E ' in l else l
                 for l in _log()]
        rc, text = _check(lines)
        self.assertEqual(rc, 1)
        self.assertIn('cred=3', text)

    def test_a_taken_continue_is_not_this_claim(self):
        lines = [l.replace('b1f=00', 'b1f=01') if ' cnt=0E ' in l else l for l in _log()]
        rc, text = _check(lines)
        self.assertEqual(rc, 1)
        self.assertIn('continue was taken', text)

    def test_the_expiry_is_required(self):
        rc, text = _check([l for l in _log() if ' mode=0015 ' not in l])
        self.assertEqual(rc, 1)
        self.assertIn('expiry', text)

    def test_the_ko_poke_is_labelled(self):
        rc, text = _check([l for l in _log() if not l.startswith('W ')])
        self.assertEqual(rc, 1)
        self.assertIn('KO poke', text)


class TestTakenEvidence(unittest.TestCase):
    def test_the_take_spend_and_round_restart_pass(self):
        rc, text = _check_taken(_taken_log())
        self.assertEqual(rc, 0, text)
        self.assertIn('take at f=1561: mode 0x0E -> 0x0C', text)
        self.assertIn('+0x5A 0x78 -> 0', text)
        self.assertIn('cred 4 -> 3', text)
        self.assertIn('b1f 0 -> 1', text)
        self.assertIn('prompt arm cnt=0D at f=1560 (below 0x0E)', text)
        self.assertIn('the round restarted ok', text)

    def test_the_expiry_only_u28_capture_is_rejected(self):
        rc, text = _check_taken(_log())
        self.assertEqual(rc, 1)
        self.assertIn("not the 0x2791C take's mode 0x0C", text)

    def test_a_synthetic_no_take_is_rejected(self):
        lines = [l for l in _taken_log() if ' mode=000C ' not in l or ' f=14B6 ' in l]
        lines.append(_rec('S', 0x18C1, 0x15, w10d=0, cnt=0xFF, b25=0, b1f=0))
        rc, text = _check_taken(lines)
        self.assertEqual(rc, 1)
        self.assertIn("not the 0x2791C take's mode 0x0C", text)

    def test_a_credit_write_is_rejected(self):
        rc, text = _check_taken(_taken_log() + [_w(0x1300, 0x105C00, b'\x03')])
        self.assertEqual(rc, 1)
        self.assertIn('credit write', text)

    def test_a_poke_at_or_after_the_screen_is_rejected(self):
        rc, text = _check_taken(_taken_log() + [_w(0x1550, 0x10789E, b'\x78')])
        self.assertEqual(rc, 1)
        self.assertIn('poke-induced', text)

    def test_an_unknown_route_poke_is_rejected(self):
        rc, text = _check_taken(_taken_log() + [_w(0x0200, 0x104AF2, b'\x01')])
        self.assertEqual(rc, 1)
        self.assertIn('not one of the route', text)

    def test_a_duplicate_route_poke_is_rejected(self):
        rc, text = _check_taken(_taken_log() + [_w(0x0900, 0x10789E, b'\x78')])
        self.assertEqual(rc, 1)
        self.assertIn('duplicate write is a new poke', text)

    def test_the_incumbent_poke_is_required(self):
        rc, text = _check_taken([l for l in _taken_log()
                                 if not l.startswith('W ') or '10780A' not in l])
        self.assertEqual(rc, 1)
        self.assertIn('incumbent KO poke', text)

    def test_the_take_must_clear_the_ko(self):
        rc, text = _check_taken(_taken_log(take_s0_5a=0x78))
        self.assertEqual(rc, 1)
        self.assertIn('+0x5A = 78', text)

    def test_the_take_must_hold_the_spend_and_the_b1f_store(self):
        rc, text = _check_taken(_taken_log(take_cred=4))
        self.assertEqual(rc, 1)
        self.assertIn('not 3', text)
        rc, text = _check_taken(_taken_log(take_b1f=0))
        self.assertEqual(rc, 1)
        self.assertIn('not (w10d != 0) + 1 = 1', text)

    def test_the_take_must_be_at_the_prompt_arm(self):
        rc, text = _check_taken(_taken_log(take_cnt=0x0F))
        self.assertEqual(rc, 1)
        self.assertIn('only below 0x0E', text)

    def test_the_restarted_round_must_be_in_the_window(self):
        rc, text = _check_taken(_taken_log(post=4))
        self.assertEqual(rc, 1)
        self.assertIn('at least', text)

    def test_the_screen_must_stay_cleared(self):
        lines = _taken_log()
        lines.append(_rec('S', 0x1600, 0x15, w10d=0, cnt=0xFF, b25=0, b1f=0))
        rc, text = _check_taken(lines)
        self.assertEqual(rc, 1)
        self.assertIn('not the restarted round', text)


class TestPath(unittest.TestCase):
    def test_the_port_reproduces_the_trace_carried_fields(self):
        if not callable(getattr(gc, 'path', None)):
            self.fail('path missing')
        out = io.StringIO()
        rc = gc.path('gp-u28-continue', _log(), _log('T'), lambda s: out.write(s + '\n'))
        self.assertEqual(rc, 0, out.getvalue())
        self.assertIn('17/17 milestones reproduced', out.getvalue())

    def test_a_port_countdown_or_expiry_difference_fails(self):
        if not callable(getattr(gc, 'path', None)):
            self.fail('path missing')
        lines = [l.replace('cnt=0D', 'cnt=0C') if ' cnt=0D ' in l else l for l in _log('T')]
        out = io.StringIO()
        rc = gc.path('gp-u28-continue', _log(), lines, lambda s: out.write(s + '\n'))
        self.assertEqual(rc, 1)
        self.assertIn('cnt', out.getvalue())

    def test_the_taken_milestones_are_reproduced(self):
        out = io.StringIO()
        rc = gc.path(gc.TAKEN_SCENARIO, _taken_log(), _taken_log('T'),
                     lambda s: out.write(s + '\n'), taken=True)
        self.assertEqual(rc, 0, out.getvalue())
        self.assertIn('4/4 milestones reproduced', out.getvalue())

    def test_a_taken_port_difference_fails(self):
        lines = [l.replace('b1f=01', 'b1f=00') if ' f=1561 ' in l else l
                 for l in _taken_log('T')]
        out = io.StringIO()
        rc = gc.path(gc.TAKEN_SCENARIO, _taken_log(), lines,
                     lambda s: out.write(s + '\n'), taken=True)
        self.assertEqual(rc, 1)
        self.assertIn('b1f', out.getvalue())

    def test_a_no_take_capture_has_no_taken_milestones(self):
        out = io.StringIO()
        rc = gc.path(gc.TAKEN_SCENARIO, _log(), _log('T'),
                     lambda s: out.write(s + '\n'), taken=True)
        self.assertEqual(rc, 1)
        self.assertIn('does not hold the take', out.getvalue())


class TestScenario(unittest.TestCase):
    def test_the_route_reuses_u10_then_pokes_the_incumbent_and_expires(self):
        self.assertIn('gp-u28-continue', gs.SCENARIOS)
        scn = gs.SCENARIOS['gp-u28-continue']
        prefix_len = len(gs.WIN_MENU) + 2
        self.assertEqual(scn['steps'][:-1], gs.SCENARIOS['gp-u10-ending']['steps'][:prefix_len])
        self.assertEqual(scn['steps'][-1], ('after_entry', 0x0C, 1, 10, gs.KO_P1))
        self.assertIsNot(scn['steps'], gs.SCENARIOS['gp-u10-ending']['steps'])
        self.assertEqual(scn['extra'], gs.WIN_EXTRA)
        self.assertEqual(scn['stop_mode'], 0x15)
        self.assertEqual(scn['stop_delay'], 0)
        self.assertIn('gp-u28-continue', gs.STOP_AT_END)

    def test_the_taken_route_clones_u28_then_presses_start_at_the_prompt(self):
        self.assertIn(gc.TAKEN_SCENARIO, gs.SCENARIOS)
        scn = gs.SCENARIOS[gc.TAKEN_SCENARIO]
        u28 = gs.SCENARIOS['gp-u28-continue']['steps']
        self.assertEqual(scn['steps'][:len(u28)], u28)
        self.assertEqual(scn['steps'][len(u28)], gs.CONTINUE_TAKEN_PRESS)
        self.assertEqual(gs.CONTINUE_TAKEN_PRESS,
                         ('after_entry', 0x0E, 1, 0xA0, ('pad', ('p1.start',), 6)))
        self.assertEqual(scn['steps'][-1], ('after_entry', 0x0E, 1, 0x100, ('end',)))
        self.assertEqual(scn['extra'], gs.WIN_EXTRA)
        self.assertNotIn('stop_mode', scn)
        self.assertIn(gc.TAKEN_SCENARIO, gs.STOP_AT_END)
        self.assertIsNot(scn['steps'], gs.SCENARIOS['gp-u28-continue']['steps'])

    def test_the_taken_schedule_fires_at_the_prompt_and_at_its_end(self):
        # The measured anchors (record §F4): mode 0x0E opens at f=0x14C0, the
        # prompt arm is cnt<0x0E from f=0x1540, the press is injected in the
        # spin of f=0x155F (iteration 0x1560) and the end is f=0x15C0.
        steps = (gs.CONTINUE_TAKEN_PRESS, ('after_entry', 0x0E, 1, 0x100, ('end',)))
        s = gs.Schedule(steps)
        s.on_snap(0x14B6, 0x0C)
        s.on_snap(0x14C0, 0x0E)
        self.assertEqual(s.due(0x155E), [])
        fired = s.due(0x155F)
        self.assertEqual(len(fired), 1)
        self.assertEqual(fired[0][1], ('pad', ('p1.start',), 6))
        self.assertEqual(s.frame_of[fired[0][0]], 0x1560)
        self.assertIsNone(s.end_frame)
        self.assertEqual(s.due(0x15BF), [])
        self.assertEqual(s.end_frame, 0x15C0)
        self.assertTrue(s.ended(0x15C0))


if __name__ == '__main__':
    unittest.main()
