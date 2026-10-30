# tools/tests/test_gp_continue.py (live truth Task 5: the continue screen 0x0E)
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
    vals.update(f=f, mode=mode, t508=1, t50c=2, cred=4, **kw)
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


def _check(lines):
    out = io.StringIO()
    if not callable(getattr(gc, 'evidence', None)):
        return 1, 'evidence missing'
    rc = gc.evidence('gp-u28-continue', lines, lambda s: out.write(s + '\n'))
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


if __name__ == '__main__':
    unittest.main()
