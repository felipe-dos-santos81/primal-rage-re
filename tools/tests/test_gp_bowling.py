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
    """Three s*_52=9 rising edges per side grow b892 0 -> 2 -> 4. The third
    pair's increments are visible; only the crossing increment can be hidden by
    the mode entry/reset, so no snapshot needs to show 6."""
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


def _stagger_log(counts=(1, 3, 5, 7), window=True, entry=True, reject=None, **entry_kw):
    """The staggered route's shape: one attempt per count entry, 150 frames
    apart, each a +2 `b892` increment inside its `s0_53 = 7` window and two
    state-9 edges per side; the clear at the attempt's fifth sample, then the
    mode-0x25 entry (record §F5.2/§F5.3)."""
    lines = [_s(0x4FF),
             'I ms=0 f=04FF step=12 press=p1.right scan=2E lin=00000000 old=80 bios=2E63 ring=1 late=0',
             'I ms=0 f=04FF step=12 press=p2.left scan=4B lin=00000000 old=80 bios=4BE0 ring=1 late=0']
    f, prev = 0x500, 0
    on = 7 if window else 0
    for v in counts:
        lines.append(_s(f, mode=6, r0=0x22, r1=0x22, s0_52=9, s1_52=9, s0_53=on, b892=prev))
        lines.append(_s(f + 1, mode=6, r0=0x22, r1=0x22, s0_52=9, s1_52=9, s0_53=on, b892=v))
        lines.append(_s(f + 2, mode=6, r0=0x22, r1=0x22, s0_52=16, s1_52=16, s0_53=on, b892=v))
        lines.append(_s(f + 3, mode=6, r0=0x22, r1=0x22, s0_52=9, s1_52=9, s0_53=on, b892=v))
        lines.append(_s(f + 4, mode=6, r0=0x22, r1=0x22, s0_53=0, b892=v))
        prev, f = v, f + 0x96
    if reject is not None:
        a, b, st = reject
        lines.append(_s(f, mode=6, r0=0x22, r1=0x22, s0_53=0, b892=a))
        lines.append(_s(f + 1, mode=6, r0=0x22, r1=0x22, s0_53=st, b892=b))
        f += 2
    if entry:
        vals = dict(mode=0x25, b8c4=1, b8a6=0xB4, b8c0=1, b892=0)
        vals.update(entry_kw)
        lines.append(_s(f, **vals))
        lines.append(_s(f + 1, mode=0x25, b8c4=1, b8a6=0xB4, b8c0=1))
    lines.append('X ms=0 f=%04X step=24 end' % (f + 0x40))
    return lines


def _check_staggered(lines):
    out = io.StringIO()
    rc = gb.evidence_staggered('gp-bowling-2', lines, lambda s: out.write(s + '\n'))
    return rc, out.getvalue()


class TestSnapshotFields(unittest.TestCase):
    def test_the_mode25_fields_are_appended_with_their_widths(self):
        # A missing/inserted field, a wrong address, or a word read as a byte
        # breaks both the DOS snapshot and the port's T-record contract.
        self.assertEqual(gs.SNAP_FIELDS[-8:], (
            ('b892', 0x108892, 2), ('b8c4', 0x1088C4, 1),
            ('b8a6', 0x1088A6, 2), ('b8bd', 0x1088BD, 1),
            ('b415', 0x104B15, 1), ('b8c0', 0x1088C0, 1),
            ('s0_53', 0x107803, 1), ('camx', 0x0F0AF0, 4)))
        vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
        vals.update(f=0x1234, b892=6, b8c4=1, b8a6=0xB4, b8bd=0, b415=0, b8c0=1,
                    s0_53=7, camx=0xFFFFCD00)
        line = gs.format_s(1, vals, 0, 0, 0)
        self.assertIn('b892=0006', line)
        self.assertIn('b8c4=01', line)
        self.assertIn('b8a6=00B4', line)
        self.assertIn('b8bd=00', line)
        self.assertIn('b415=00', line)
        self.assertIn('b8c0=01', line)
        self.assertIn('s0_53=07', line)
        self.assertIn('camx=FFFFCD00', line)
        self.assertEqual({n: gs.parse(line)[n] for n in
                          ('b892', 'b8c4', 'b8a6', 'b8bd', 'b415', 'b8c0',
                           's0_53', 'camx')},
                         {'b892': 6, 'b8c4': 1, 'b8a6': 0xB4,
                          'b8bd': 0, 'b415': 0, 'b8c0': 1,
                          's0_53': 7, 'camx': 0xFFFFCD00})


class TestScenario(unittest.TestCase):
    def test_the_corrected_route_walks_in_and_plays_move_zero_three_times(self):
        # Dropping the 30-frame walk-in, an attempt, or the measured 150-frame
        # attempt spacing changes the route's counter/state-9 evidence.
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

    def test_the_staggered_route_plays_five_attempts_and_stops_on_the_mode(self):
        # Final truth Task 5 (record §F5): the same mirror-ARMADON fight and move
        # 0, five attempts at the measured 150-frame spacing, no poke, and the
        # capture ends on mode 0x25 (60 frames in) or 300 frames after the fifth
        # attempt when the mode never opens.
        self.assertIn('gp-bowling-2', gs.SCENARIOS)
        scn = gs.SCENARIOS['gp-bowling-2']
        self.assertEqual(scn['steps'][:12], gs.SCENARIOS['gp-bowling']['steps'][:12])
        attempts = scn['steps'][12:-1]
        self.assertEqual(len(attempts), 20)
        for k in range(5):
            self.assertEqual(attempts[k * 4:(k + 1) * 4], (
                ('after', 60 if k == 0 else 142,
                 ('pad', ('p1.right', 'p2.left'), 4)),
                ('after', 0, ('pad', ('p1.b0', 'p1.b2', 'p2.b0', 'p2.b2'), 12)),
                ('after', 4, ('pad', ('p1.down', 'p2.down'), 4)),
                ('after', 4, ('pad', ('p1.left', 'p2.right'), 4))))
        self.assertEqual(scn['steps'][-1], ('after', 300, ('end',)))
        self.assertEqual(scn['stop_mode'], 0x25)
        self.assertEqual(scn['stop_delay'], 60)
        self.assertIn('gp-bowling-2', gs.STOP_AT_END)
        self.assertFalse(any(st[-1][0] == 'poke' for st in scn['steps'][:-1]))
        # the route's attempts 2-5 repeat the fix route's measured attempt-2/3 steps
        self.assertEqual(attempts[4:], gs.BOWLING_MOVE0_ATTEMPTS[4:] * 2)


class TestEvidence(unittest.TestCase):
    def test_a_success_without_a_visible_six_passes(self):
        # Only the increment crossing 6 and 0x4E11C's entry/reset can share an
        # iteration. Requiring a sampled 6 can reject a real success.
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

    def test_the_sampled_counter_must_be_non_decreasing_apart_from_one_rejection(self):
        lines = [l.replace('b892=0004', 'b892=0002') if ' f=0505 ' in l else l
                 for l in _log()]
        rc, text = _check(lines)
        self.assertEqual(rc, 1)
        self.assertIn('non-decreasing', text)

    def test_one_rejection_decrement_is_allowed_but_mode25_is_still_required(self):
        # A strict-monotonicity check rejects the raw's same-iteration 6 -> 5
        # rejection arm (0x4E131/0x4E189), even when the capture never enters.
        values = {0x502: 1, 0x503: 3, 0x504: 5, 0x505: 6, 0x506: 5}
        lines = []
        for line in _log():
            if line.startswith('S ') and gs.parse(line)['f'] in values:
                value = values[gs.parse(line)['f']]
                line = ' '.join('b892=%04X' % value if p.startswith('b892=') else p
                                for p in line.split())
            lines.append(line)
        rc, text = _check(lines)
        self.assertEqual(rc, 0, text)
        rc, text = _check([l.replace('mode=0025', 'mode=0006') for l in lines])
        self.assertEqual(rc, 1)
        self.assertIn('mode 0x25 never observed', text)

    def test_state9_edge_uses_the_last_snapshot_not_the_previous_frame(self):
        # Restoring the f-1 lookup loses the f=0504 edge across the f=0503 gap.
        lines = []
        for line in _log():
            if ' f=0503 ' in line:
                continue
            if ' f=0504 ' in line or ' f=0506 ' in line:
                line = line.replace('s0_52=00', 's0_52=09').replace('s1_52=00', 's1_52=09')
            elif ' f=0505 ' in line:
                line = line.replace('s0_52=09', 's0_52=00').replace('s1_52=09', 's1_52=00')
            lines.append(line)
        rc, text = _check(lines)
        self.assertEqual(rc, 0, text)

    def test_each_side_must_reach_callback_animation_state9_three_times(self):
        for field in ('s0_52', 's1_52'):
            with self.subTest(field=field):
                lines = [l.replace(field + '=09', field + '=00') if ' f=0503 ' in l else l
                         for l in _log()]
                rc, text = _check(lines)
                self.assertEqual(rc, 1)
                self.assertIn(field, text)
                self.assertIn('state-9', text)
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


class TestStaggered(unittest.TestCase):
    """Final truth Task 5's five-attempt route check (record §F5)."""

    def test_the_fourth_attempt_crossing_passes(self):
        # The three-attempt route measured 5; a fourth +1..+2 puts the clear
        # frame's count at 6..7 and the entry stores reset it to 0.
        rc, text = _check_staggered(_stagger_log())
        self.assertEqual(rc, 0, text)
        self.assertIn('b892 max 7', text)
        self.assertIn('state-9 edges 8/8', text)
        self.assertIn('camx=+0', text)

    def test_the_count_must_reach_six(self):
        # 1,3,5,5: the three-attempt route's shape one attempt short.
        rc, text = _check_staggered(_stagger_log(counts=(1, 3, 5, 5)))
        self.assertEqual(rc, 1)
        self.assertIn('b892 reached 5, not >= 6', text)

    def test_the_camera_must_be_inside_the_plus_minus_0x3300_gate(self):
        for cam, ok in ((0x3300, True), (-0x3300 & 0xFFFFFFFF, True),
                        (0x3301, False), (-0x3301 & 0xFFFFFFFF, False)):
            with self.subTest(cam=cam):
                rc, text = _check_staggered(_stagger_log(camx=cam))
                self.assertEqual(rc, 0 if ok else 1, text)
                if not ok:
                    self.assertIn('outside', text)
                    self.assertIn('0x4E13D', text)

    def test_the_rejection_decrement_is_the_one_minus_one_from_six(self):
        rc, text = _check_staggered(_stagger_log(reject=(7, 6, 0)))
        self.assertEqual(rc, 0, text)
        self.assertIn('1 rejection decrement', text)
        for reject, why in (((7, 6, 7), 's0_53'), ((5, 4, 0), '>= 6'), ((7, 5, 0), '>-1')):
            with self.subTest(why=why):
                rc, text = _check_staggered(_stagger_log(reject=reject))
                self.assertEqual(rc, 1, text)
                self.assertIn('rejection arm', text)

    def test_an_increment_is_at_most_two_a_frame(self):
        rc, text = _check_staggered(_stagger_log(counts=(1, 3, 5, 8)))
        self.assertEqual(rc, 1)
        self.assertIn('more than the two', text)

    def test_state9_edges_are_bounded_by_the_routes_attempts(self):
        rc, text = _check_staggered(_stagger_log(counts=(1,)))
        self.assertEqual(rc, 1)
        self.assertIn('want at least three', text)
        rc, text = _check_staggered(_stagger_log(counts=(1, 3, 5, 7, 7, 7, 7)))
        self.assertEqual(rc, 1)
        self.assertIn('measured per attempt', text)

    def test_the_reaction_window_and_the_gate_byte_are_required(self):
        rc, text = _check_staggered(_stagger_log(window=False))
        self.assertEqual(rc, 1)
        self.assertIn('never reads 7', text)
        rc, text = _check_staggered(_stagger_log(s0_53=7))
        self.assertEqual(rc, 1)
        self.assertIn('0x263DE', text)

    def test_a_poke_cannot_substitute_for_the_trigger(self):
        lines = _stagger_log()
        lines.insert(-2, gs.format_w(0, 0x700, 14, 0x108892,
                                    b'\x01\x00', b'\x08\x00', 0, 0))
        rc, text = _check_staggered(lines)
        self.assertEqual(rc, 1)
        self.assertIn('poke', text)

    def test_the_prior_failed_artifacts_still_fail_the_trigger(self):
        # Neither real capture entered mode 0x25: both must fail with the same
        # verdict the live-truth task recorded (§L6.2/§L6.3), now with the
        # measured residue appended when the new fields exist.
        for name in ('gp-bowling.failed', 'gp-bowling-fix.failed'):
            path = os.path.join(ROOT, 'data', 'k11-captures', name, 'poll.log')
            if not os.path.isfile(path):
                self.skipTest('the .failed artifacts are git-ignored')
            with open(path) as f:
                lines = f.read().splitlines()
            out = io.StringIO()
            rc = gb.evidence_staggered(name, lines, lambda s: out.write(s + '\n'))
            text = out.getvalue()
            self.assertEqual(rc, 1, text)
            self.assertIn('mode 0x25 never observed', text)
            self.assertIn('b892 max', text)

    def test_cli_staggered_pins_the_scenario_name(self):
        if gb is None or not callable(getattr(gb, 'main', None)):
            self.fail('main missing')
        with tempfile.TemporaryDirectory() as d:
            cap = os.path.join(d, 'gp-bowling-2')
            os.makedirs(cap)
            with open(os.path.join(cap, 'poll.log'), 'w') as f:
                f.write('\n'.join(_stagger_log()) + '\n')
            with mock.patch.object(sys, 'argv',
                                   ['gp_bowling.py', 'check', '--staggered', '--capture', cap]):
                self.assertEqual(gb.main(), 0)
            with mock.patch.object(sys, 'argv',
                                   ['gp_bowling.py', 'check', '--staggered', '--capture', d]):
                self.assertEqual(gb.main(), 1)
            with open(os.path.join(cap, 'poll.log'), 'w') as f:
                f.write('\n'.join(_stagger_log(counts=(1, 3, 5, 5))) + '\n')
            with mock.patch.object(sys, 'argv',
                                   ['gp_bowling.py', 'check', '--staggered', '--capture', cap]):
                self.assertEqual(gb.main(), 1)


if __name__ == '__main__':
    unittest.main()
