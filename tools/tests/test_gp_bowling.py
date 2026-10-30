# tools/tests/test_gp_bowling.py (live truth Task 6: the mode-0x25 bowling entry)
import io
import os
import sys
import tempfile
import unittest
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
try:
    import gp_bowling as gb
except ImportError:
    gb = None


def _s(f, mode=6, **kw):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, mode=mode, b1f=3, c0=4, c1=4, t508=1, t50c=2, **kw)
    return gs.format_s(0, vals, 0, 0x1E, 0x1E)


def _log():
    """Three reaction-0x22 entries per side grow b892 0 -> 2 -> 4. The third
    pair's +2 and the mode entry/reset share an iteration, so no snapshot shows 6."""
    return [
        _s(0x500),
        'I ms=0 f=0500 step=12 press=p1.right scan=2E lin=00000000 old=80 bios=2E63 ring=1 late=0',
        'I ms=0 f=0500 step=12 press=p2.left scan=4B lin=00000000 old=80 bios=4BE0 ring=1 late=0',
        _s(0x501, r0=0x22, r1=0x22, s0_52=9, s1_52=9),
        'I ms=0 f=0501 step=13 press=p1.b0 scan=16 lin=00000000 old=80 bios=1675 ring=1 late=0',
        'I ms=0 f=0501 step=13 press=p2.b2 scan=4F lin=00000000 old=80 bios=4F00 ring=1 late=0',
        _s(0x502, r0=0x22, r1=0x22, b892=2),
        _s(0x503, r0=0x22, r1=0x22, b892=2, s0_52=9, s1_52=9),
        _s(0x504, r0=0x22, r1=0x22, b892=4),
        _s(0x505, r0=0x22, r1=0x22, b892=4, s0_52=9, s1_52=9),
        _s(0x506, r0=0x22, r1=0x22, b892=4),
        _s(0x507, mode=0x25, b8c4=1, b8a6=0xB4, b8c0=1),
        _s(0x508, mode=0x25, b8c4=1, b8a6=0xB4, b8c0=1),
        'X ms=0 f=0620 step=24 end',
    ]


def _check(lines):
    out = io.StringIO()
    if gb is None or not callable(getattr(gb, 'evidence', None)):
        return 1, 'evidence missing'
    rc = gb.evidence('gp-bowling', lines, lambda s: out.write(s + '\n'))
    return rc, out.getvalue()


class TestSnapshotFields(unittest.TestCase):
    def test_the_mode25_fields_are_appended_with_their_widths(self):
        # A missing/inserted field, a wrong address, or a word read as a byte
        # breaks both the DOS snapshot and the port's T-record contract.
        self.assertEqual(gs.SNAP_FIELDS[-6:], (
            ('b892', 0x108892, 2), ('b8c4', 0x1088C4, 1),
            ('b8a6', 0x1088A6, 2), ('b8bd', 0x1088BD, 1),
            ('b415', 0x104B15, 1), ('b8c0', 0x1088C0, 1)))
        vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
        vals.update(f=0x1234, b892=6, b8c4=1, b8a6=0xB4, b8bd=0, b415=0, b8c0=1)
        line = gs.format_s(1, vals, 0, 0, 0)
        self.assertIn('b892=0006', line)
        self.assertIn('b8c4=01', line)
        self.assertIn('b8a6=00B4', line)
        self.assertIn('b8bd=00', line)
        self.assertIn('b415=00', line)
        self.assertIn('b8c0=01', line)
        self.assertEqual({n: gs.parse(line)[n] for n in
                          ('b892', 'b8c4', 'b8a6', 'b8bd', 'b415', 'b8c0')},
                         {'b892': 6, 'b8c4': 1, 'b8a6': 0xB4,
                          'b8bd': 0, 'b415': 0, 'b8c0': 1})


class TestScenario(unittest.TestCase):
    def test_the_corrected_route_walks_in_and_plays_move_zero_three_times(self):
        # Dropping the 30-frame walk-in, an attempt, or the measured 150-frame
        # attempt spacing leaves the counter/re-entry guard unsatisfied.
        self.assertIn('gp-bowling', gs.SCENARIOS)
        scn = gs.SCENARIOS['gp-bowling']
        steps = scn['steps']
        self.assertIsNot(steps, gs.SCENARIOS['gp-twop']['steps'])
        self.assertEqual(steps[:3], gs.SCENARIOS['gp-twop']['steps'][:3])
        self.assertEqual(steps[3], ('after_mode', 0x10, 60, ('pad', ('p2.start',), 5)))
        self.assertEqual([st[-1] for st in steps[4:9]],
                         [('pad', ('p1.right',), 5)] * 5)
        self.assertEqual([st[1] for st in steps[4:9]], [60, 30, 30, 30, 30])
        self.assertEqual(steps[9:12], (
            ('after', 30, ('pad', ('p1.b0',), 5)),
            ('after', 30, ('pad', ('p2.b0',), 5)),
            ('after_mode', 0x06, 60, ('pad', ('p1.right', 'p2.left'), 30))))
        attempts = steps[12:-1]
        self.assertEqual(len(attempts), 12)
        for k in range(3):
            first = 0 if k == 0 else 142
            self.assertEqual(attempts[k * 4:(k + 1) * 4], (
                ('after', 60 if k == 0 else first, ('pad', ('p1.right', 'p2.left'), 4)),
                ('after', 0, ('pad', ('p1.b0', 'p1.b2', 'p2.b0', 'p2.b2'), 12)),
                ('after', 4, ('pad', ('p1.down', 'p2.down'), 4)),
                ('after', 4, ('pad', ('p1.left', 'p2.right'), 4))))
        self.assertEqual(steps[-1], ('until_mode', 0x25, 300))
        self.assertIn('gp-bowling-fix', gs.SCENARIOS)
        self.assertEqual(gs.SCENARIOS['gp-bowling-fix']['steps'], steps)
        for name in ('gp-bowling', 'gp-bowling-fix'):
            self.assertIn(name, gs.STOP_AT_END)
            self.assertFalse(any(st[-1][0] == 'poke' for st in steps[:-1]))


class TestEvidence(unittest.TestCase):
    def test_a_success_without_a_visible_six_passes(self):
        # The third pair's increment and 0x4E11C's entry/reset share an
        # iteration. Requiring a sampled 6 rejects every real success.
        self.assertNotIn('b892=0006', '\n'.join(_log()))
        rc, text = _check(_log())
        self.assertEqual(rc, 0, text)
        self.assertIn('b892=4', text)
        self.assertIn('r0=r1=0x22', text)
        self.assertIn('mode 0x25', text)
        self.assertIn('f=507', text)

    def test_the_entry_predicates_are_all_required(self):
        # One table-driven mutation per raw predicate: accepting any of these
        # would name a different mode/path as the bowling entry.
        mutations = (
            ('b1f=03', 'b1f=01', 'b1f'),
            ('b1d=00', 'b1d=02', 'b1d'),
            ('c0=04', 'c0=03', 'c0'),
            ('c1=04', 'c1=03', 'c1'),
            ('b25=00', 'b25=01', 'b25'),
            ('b8a6=00B4', 'b8a6=00B3', 'b8a6'),
            ('b8bd=00', 'b8bd=01', 'b8bd'),
            ('b415=00', 'b415=01', 'b415'),
            ('b8c0=01', 'b8c0=00', 'b8c0'),
        )
        for old, new, field in mutations:
            with self.subTest(field=field):
                lines = [l.replace(old, new) if ' mode=0025 ' in l else l
                         for l in _log()]
                rc, text = _check(lines)
                self.assertEqual(rc, 1, text)
                self.assertIn(field, text)

    def test_the_sampled_counter_must_reach_four(self):
        rc, text = _check([l.replace('b892=0004', 'b892=0002') for l in _log()])
        self.assertEqual(rc, 1)
        self.assertIn('b892', text)
        self.assertIn('>= 4', text)

    def test_the_sampled_counter_must_be_monotone(self):
        lines = [l.replace('b892=0004', 'b892=0002') if ' f=0505 ' in l else l
                 for l in _log()]
        rc, text = _check(lines)
        self.assertEqual(rc, 1)
        self.assertIn('monotone', text)

    def test_each_side_must_reenter_the_reaction_three_times(self):
        for field in ('s0_52', 's1_52'):
            with self.subTest(field=field):
                lines = [l.replace(field + '=09', field + '=00') if ' f=0503 ' in l else l
                         for l in _log()]
                rc, text = _check(lines)
                self.assertEqual(rc, 1)
                self.assertIn(field, text)
                self.assertIn('three', text)

    def test_the_entry_must_be_the_zero_to_one_transition(self):
        lines = [l.replace('b8c4=00', 'b8c4=01') for l in _log()]
        rc, text = _check(lines)
        self.assertEqual(rc, 1)
        self.assertIn('0 -> 1', text)

    def test_both_reactions_and_real_pad_input_are_required(self):
        for field in ('r0', 'r1'):
            with self.subTest(field=field):
                rc, text = _check([l.replace(field + '=22', field + '=21') for l in _log()])
                self.assertEqual(rc, 1)
                self.assertIn(field, text)
        rc, text = _check([l for l in _log() if not l.startswith('I ')])
        self.assertEqual(rc, 1)
        self.assertIn('real pad input', text)

    def test_a_poke_cannot_substitute_for_the_trigger(self):
        lines = _log()[:-1] + [gs.format_w(0, 0x502, 14, 0x108892,
                                           b'\x01\x00', b'\x06\x00', 0, 0)] + _log()[-1:]
        rc, text = _check(lines)
        self.assertEqual(rc, 1)
        self.assertIn('poke', text)

    def test_mode25_is_required(self):
        rc, text = _check([l.replace('mode=0025', 'mode=0006') for l in _log()])
        self.assertEqual(rc, 1)
        self.assertIn('never observed', text)

    def test_cli_check_returns_the_evidence_verdict(self):
        if gb is None or not callable(getattr(gb, 'main', None)):
            self.fail('main missing')
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, 'poll.log'), 'w') as f:
                f.write('\n'.join(_log()) + '\n')
            with mock.patch.object(sys, 'argv', ['gp_bowling.py', 'check', '--capture', d]):
                self.assertEqual(gb.main(), 0)
            with open(os.path.join(d, 'poll.log'), 'w') as f:
                f.write('\n'.join(_log()).replace('b892=0004', 'b892=0002') + '\n')
            with mock.patch.object(sys, 'argv', ['gp_bowling.py', 'check', '--capture', d]):
                self.assertEqual(gb.main(), 1)


if __name__ == '__main__':
    unittest.main()
