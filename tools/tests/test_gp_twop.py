# tools/tests/test_gp_twop.py (gameplay U7: the gp-twop scenario and the two-human check)
import contextlib, io, os, sys, tempfile, unittest
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
import gp_twop as gt


# The S fields a KO needs in addition to SNAP_FIELDS (gs.WIN_EXTRA): the damage
# bytes' KO value 0x78, the round wins and the match result are WIN fields, so a
# KO log carries them all (test_gp_win.py's line shape).
FIELDS = gs.SNAP_FIELDS + gs.WIN_EXTRA


def _s(f, mode=6, b1f=3, cred=4, new=0, held=0, e0=None, e2=None, r0=0, r1=0,
       s0_5a=0, s1_5a=0, w2=0, w3=0, ad4=0, b0c=0):
    vals = {n: 0 for n, _, _ in FIELDS}
    vals.update(f=f, mode=mode, b1f=b1f, cred=cred, new=new, held=held, t508=1, t50c=2,
                e0=gt.pad_word(new, held, 0) if e0 is None else e0,
                r0=r0, r1=r1,
                e2=gt.pad_word(new, held, 1) if e2 is None else e2,
                s0_5a=s0_5a, s1_5a=s1_5a, w2=w2, w3=w3, ad4=ad4, b0c=b0c)
    return gs.format_s(0, vals, 0, 0x1E, 0x1E, FIELDS)


def _match():
    """A two-human log: b1f 1 -> 3 at 0x300 (cred stays 4), a fight with both pressing, X at 0x510."""
    L = [_s(f, mode=0x10, b1f=1) for f in range(0x2F0, 0x300)]
    L += [_s(f, mode=0x10) for f in range(0x300, 0x310)]
    L += [_s(0x400, mode=5, e2=0xA000)]                      # character 1's entrance (0x246EE)
    L += [_s(0x500, held=0x10002000)]                         # P1 right held, P2 left held
    L += [_s(0x501, new=0x02000400, held=0x02000400)]         # P1 b1 new, P2 b2 new
    L += [_s(0x502, held=0x10000000, e0=0)]                  # P1 right held, word 0: the 0x24C96 arm
    L += ['X ms=0 f=0510 step=11 end']
    return L


class TestScenario(unittest.TestCase):
    def test_steps_fire_in_order(self):
        # mode 0x27 at 0x141, 0x2D at 0x26E, 0x10 at 0x293 (gp-idle-loss run 1,
        # record §G.18); mode 6 at 0x5F0 (an example frame)
        s = gs.Schedule(gs.SCENARIOS['gp-twop']['steps'])
        self.assertEqual(s.due_boot(gs.ENTER_WAIT), [(0, ('key', 'enter'))])
        s.on_mode(0x141, 0x27)
        self.assertEqual(s.due(0x141 + 149), [(1, ('key', 'enter'))])
        self.assertEqual(s.due(0x141 + 299), [(2, ('key', 'enter'))])
        s.on_mode(0x26E, 0x2D)
        self.assertEqual(s.due(0x292), [])
        s.on_mode(0x293, 0x10)
        self.assertEqual(s.due(0x293 + 58), [])
        self.assertEqual(s.due(0x293 + 59), [(3, ('pad', ('p2.start',), 5))])
        self.assertEqual(s.due(0x293 + 119), [(4, ('pad', ('p1.right',), 5))])
        self.assertEqual(s.due(0x293 + 149), [(5, ('pad', ('p2.left',), 5))])
        self.assertEqual(s.due(0x293 + 179), [(6, ('pad', ('p1.b0',), 5))])
        self.assertEqual(s.due(0x293 + 209), [(7, ('pad', ('p2.b0',), 5))])
        s.on_mode(0x5F0, 6)
        self.assertEqual(s.due(0x5F0 + 59), [(8, ('pad', ('p1.right', 'p2.left'), 30))])
        self.assertEqual(s.due(0x5F0 + 119), [(9, ('pad', ('p1.b1', 'p2.b2'), 5))])
        self.assertEqual(s.due(0x5F0 + 149), [(10, ('pad', ('p1.b2', 'p2.b1'), 5))])
        self.assertIsNone(s.end_frame)
        self.assertEqual(s.due(0x5F0 + 209), [])
        self.assertEqual((s.end_frame, s.fired, s.total), (0x5F0 + 210, 11, 11))

    def test_the_presses_are_both_sides_pads(self):
        # spec §3.2: P2's start is F2 (scan 0x3C, kb 0x0001), b0 Home; P1's b0 is U
        acts = [st[-1] for st in gs.SCENARIOS['gp-twop']['steps'] if st[-1][0] == 'pad']
        names = [n for a in acts for n in a[1]]
        self.assertEqual(sorted(set(n[:2] for n in names)), ['p1', 'p2'])
        self.assertEqual(gs.expand(acts[0]), [('p2.start', 0x3C, 0x3C00, 5)])
        self.assertTrue(all(a[2] >= 2 for a in acts))       # a 1-frame press never reaches the level


def _join(new=0x100, before_mode=6, join_mode=0x17, held=0, before_held=0):
    """A mid-round join (0x28CC8/0x28DA4): `before_mode` with P1 only, then F2 at
    0x400 (new 0x100 = DS_0009ACBC[1]) takes b1f 1 -> 3 in the join's mode 0x17 and
    cred stays 4; X at 0x440, with no mode-6 record after the join."""
    L = [_s(f, mode=before_mode, b1f=1, held=before_held) for f in range(0x3F0, 0x400)]
    L += [_s(0x400, mode=join_mode, b1f=3, new=new, held=held)]
    L += [_s(f, mode=join_mode) for f in range(0x401, 0x440)]
    L += ['X ms=0 f=0440 step=9 end']
    return L


class TestMidRoundJoin(unittest.TestCase):
    """The join-only claim (gp-u15-join): the capture ends inside the join's
    mode-0x17 window, where no fight press can happen."""

    def test_the_join_check_does_not_require_a_fight_press(self):
        # gp-keys-fight's own join (f=0x85A): mode 6 -> 0x17, b1f 1 -> 3, cred 4 -> 4,
        # new=0x100; the two-human rule fails on both presses, the join rule passes.
        L = _join()
        self.assertEqual(gt.two_human(L)['fail'],
                         ['side 0 never pressed a key in the fight (mode 6)',
                          'side 1 never pressed a key in the fight (mode 6)'])
        r = gt.two_human(L, join_only=True)
        self.assertEqual(r['fail'], [])
        self.assertEqual((r['join'], r['end'], r['cred_before'], r['cred_join']),
                         (0x400, 0x440, 4, 4))
        self.assertEqual((r['before_mode'], r['join_mode'], r['b1f_before'], r['b1f_join']),
                         (6, 0x17, 1, 3))

    def test_the_join_must_come_from_the_fight(self):
        r = gt.two_human(_join(before_mode=0x10), join_only=True)
        self.assertIn('is not mid-round', r['fail'][0])

    def test_the_join_must_enter_the_join_window(self):
        # 0x28DA4 stores mode 0x17 (0x28E3A/0x28E66): an accepted start that stays in
        # mode 6 is not the join path
        r = gt.two_human(_join(join_mode=6), join_only=True)
        self.assertIn('join window', r['fail'][0])

    def test_the_joining_sides_start_mask_must_be_pressed(self):
        # DS_0009ACBC[1] is F2's level bit (0x00000100, gp_keys.P1_B0_NEW's side-1 twin)
        r = gt.two_human(_join(new=0), join_only=True)
        self.assertIn('start mask', r['fail'][0])
        self.assertEqual(gt.two_human(_join(), join_only=True)['start_edge'], 'new')

    def test_the_held_edge_across_missed_frames_is_accepted(self):
        # gp-u15-join: the harness missed f=0x832/0x833 where the bit rose, so the join
        # record (0x834) holds F2 with new=0 and the record before it (0x831) did not
        lines = [line for line in _join(new=0, held=gt.START_MASK[1])
                 if not (gs.parse(line) and gs.parse(line).get('f') in (0x3FE, 0x3FF))]
        r = gt.two_human(lines, join_only=True)
        self.assertEqual((r['fail'], r['start_edge']), ([], 'held'))

    def test_a_held_rise_between_consecutive_frames_is_not_proven(self):
        r = gt.two_human(_join(new=0, held=gt.START_MASK[1]), join_only=True)
        self.assertEqual(r['start_edge'], None)
        self.assertIn('not newly pressed at f=400', r['fail'][0])

    def test_the_join_without_a_prior_snapshot_fails(self):
        lines = [_s(0x400, mode=0x17, b1f=3, new=gt.START_MASK[1]),
                 'X ms=0 f=0440 step=9 end']
        r = gt.two_human(lines, join_only=True)
        self.assertTrue(any('has no S record before it' in failure for failure in r['fail']))

    def test_a_start_held_before_the_join_is_not_newly_pressed(self):
        r = gt.two_human(_join(new=0, held=gt.START_MASK[1], before_held=gt.START_MASK[1]), join_only=True)
        self.assertEqual(len(r['fail']), 1)
        self.assertIn('not newly pressed at f=400 (new 00000000, held 00000100; the record before it held 00000100)',
                      r['fail'][0])


class TestTwoHuman(unittest.TestCase):
    def test_pad_word_is_0x4f644(self):
        # gp-pads f=0x3D6 (record §T.2.1): new = held = 0x24001000 -> e0 2424, e2 1010
        self.assertEqual(gt.pad_word(0x24001000, 0x24001000, 0), 0x2424)
        self.assertEqual(gt.pad_word(0x24001000, 0x24001000, 1), 0x1010)
        self.assertEqual(gt.pad_word(0, 0x24001000, 1), 0x1000)

    def test_a_two_human_match_passes(self):
        r = gt.two_human(_match())
        self.assertEqual(r['fail'], [])
        self.assertEqual((r['join'], r['end'], r['cred_before'], r['cred_join']), (0x300, 0x510, 4, 4))
        self.assertEqual(r['pressed'], [2, 2])
        self.assertEqual(r['counts'][1].get('entrance'), 1)
        self.assertEqual(r['counts'][0].get('zero'), 1)

    def test_a_cpu_word_fails(self):
        L = _match()[:-1] + [_s(0x503, e2=0x2020)] + _match()[-1:]   # gp-idle-loss's first CPU word (f=0x77C)
        r = gt.two_human(L)
        self.assertEqual(len(r['fail']), 1)
        self.assertIn('side 1 command word 2020 at f=503', r['fail'][0])

    def test_records_after_the_end_are_not_checked(self):
        L = _match() + [_s(0x511, b1f=1, e2=0x2020)]           # past X: the capture's idle tail
        self.assertEqual(gt.two_human(L)['fail'], [])

    def test_a_credit_debited_at_the_join_fails(self):
        # 0x2CA93 skips the debit 0x2CA9C when b1f != 0 (record §T.1.3): a capture whose credit drops at the join is not this path
        L = [_s(f, mode=0x10, b1f=1, cred=5) for f in range(0x2F0, 0x300)] + _match()[16:]
        r = gt.two_human(L)
        self.assertEqual((r['cred_before'], r['cred_join']), (5, 4))
        self.assertEqual(len(r['fail']), 1)
        self.assertIn('credit 5 -> 4 at the join f=300', r['fail'][0])

    def test_one_human_fails(self):
        L = [l.replace('b1f=03', 'b1f=01') for l in _match()]
        self.assertEqual(gt.two_human(L)['fail'], ['b1f never reaches 3 (no S record has both sides human)'])

    def test_b1f_dropping_after_the_join_fails(self):
        L = _match()[:-1] + [_s(0x503, b1f=1)] + _match()[-1:]
        self.assertIn('b1f=1 at f=503', gt.two_human(L)['fail'][0])

    def test_a_side_that_never_pressed_fails(self):
        L = _match()[:-4] + [_s(0x500, held=0x10000000)] + _match()[-1:]   # P1 only
        self.assertEqual(gt.two_human(L)['fail'], ['side 1 never pressed a key in the fight (mode 6)'])

    def test_the_entrance_word_is_allowed_in_mode_5_only(self):
        L = _match()[:-1] + [_s(0x503, e2=0xA000)] + _match()[-1:]   # mode 6
        self.assertIn('side 1 command word A000', gt.two_human(L)['fail'][0])

    def test_a_port_trace_reads_as_s_records(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 'trace.txt')
            with open(p, 'w') as f:
                f.write('\n'.join('T' + l[1:] for l in _match() if l.startswith('S ')) + '\n')
            r = gt.two_human(gt.load(p))
        self.assertEqual((r['join'], r['end'], r['fail']), (0x300, None, []))

    def test_mode_path(self):
        p = gt.mode_path(_match())
        self.assertEqual([(f, m, b1f, cred) for _, f, m, b1f, cred, _, _ in p],
                         [(0x2F0, 0x10, 1, 4), (0x400, 5, 3, 4), (0x500, 6, 3, 4)])


class TestFinisher(unittest.TestCase):
    def lines(self, reaction=0x32, mode12=True):
        tail = [_s(0x508, mode=0x09, r0=reaction)]
        if mode12:
            tail.append(_s(0x509, mode=0x12))
        return _match()[:-1] + tail + _match()[-1:]

    def test_finisher_requires_a_reaction_and_the_end(self):
        lines = self.lines()
        r = gt.finisher(lines)
        self.assertEqual(r['fail'], [])
        self.assertEqual(r['reactions'], [0x32])
        self.assertTrue(r['mode9'])
        self.assertTrue(r['mode12'])
        no_react = [l for l in lines if 'r0=32' not in l and 'r1=32' not in l]
        r = gt.finisher(no_react)
        self.assertIn('no finisher reaction 0x32..0x34 in any S record', r['fail'])

    def test_finisher_requires_mode_0x12(self):
        r = gt.finisher(self.lines(mode12=False))
        self.assertTrue(r['mode9'])
        self.assertFalse(r['mode12'])
        self.assertIn('mode 0x12 not reached', r['fail'])

    def test_finisher_requires_mode_0x09(self):
        lines = [_s(0x508, mode=0x12, r0=0x32), 'X ms=0 f=0510 step=11 end']
        r = gt.finisher(lines)
        self.assertFalse(r['mode9'])
        self.assertIn('mode 0x09 not reached', r['fail'])

    def test_finisher_accepts_a_reaction_in_r1(self):
        lines = [_s(0x508, mode=0x09, r1=0x34), _s(0x509, mode=0x12)]
        self.assertEqual(gt.finisher(lines)['reactions'], [0x34])

    def test_cli_finisher_runs_both_checks_and_returns_failure(self):
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, 'poll.log'), 'w') as f:
                f.write('\n'.join(self.lines(reaction=0)) + '\n')
            stdout = io.StringIO()
            with mock.patch.object(sys, 'argv', ['gp_twop.py', 'check', '--capture', d, '--finisher']):
                with contextlib.redirect_stdout(stdout):
                    rc = gt.main()
        self.assertEqual(rc, 1)
        self.assertIn('two-human match: ok', stdout.getvalue())
        self.assertIn('no finisher reaction 0x32..0x34 in any S record', stdout.getvalue())

    def test_cli_finisher_can_allow_a_missing_reaction(self):
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, 'poll.log'), 'w') as f:
                f.write('\n'.join(self.lines(reaction=0)) + '\n')
            stdout = io.StringIO()
            with mock.patch.object(sys, 'argv', ['gp_twop.py', 'check', '--capture', d,
                                                  '--finisher', '--allow-missing-reaction']):
                with contextlib.redirect_stdout(stdout):
                    rc = gt.main()
        self.assertEqual(rc, 0)
        self.assertIn('two-human match: ok', stdout.getvalue())
        self.assertIn('finisher: ok; mode 0x09 reached, mode 0x12 reached, reactions []',
                      stdout.getvalue())

    def test_cli_finisher_allowing_a_missing_reaction_still_requires_mode_0x12(self):
        lines = self.lines(reaction=0, mode12=False)
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, 'poll.log'), 'w') as f:
                f.write('\n'.join(lines) + '\n')
            stdout = io.StringIO()
            with mock.patch.object(sys, 'argv', ['gp_twop.py', 'check', '--capture', d,
                                                  '--finisher', '--allow-missing-reaction']):
                with contextlib.redirect_stdout(stdout):
                    rc = gt.main()
        self.assertEqual(rc, 1)
        self.assertIn('two-human match: ok', stdout.getvalue())
        self.assertIn('mode 0x12 not reached', stdout.getvalue())

    def test_cli_rejects_allowing_a_missing_reaction_without_finisher(self):
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, 'poll.log'), 'w') as f:
                f.write('')
            stderr = io.StringIO()
            with mock.patch.object(sys, 'argv', ['gp_twop.py', 'check', '--capture', d,
                                                  '--allow-missing-reaction']):
                with contextlib.redirect_stderr(stderr):
                    with self.assertRaises(SystemExit) as cm:
                        gt.main()
        self.assertEqual(cm.exception.code, 2)
        self.assertIn('--allow-missing-reaction requires --finisher', stderr.getvalue())

    def test_cli_join_and_finisher_are_mutually_exclusive(self):
        stderr = io.StringIO()
        with mock.patch.object(sys, 'argv', ['gp_twop.py', 'check', '--capture', 'capture',
                                             '--join', '--finisher']):
            with contextlib.redirect_stderr(stderr):
                with self.assertRaises(SystemExit) as cm:
                    gt.main()
        self.assertEqual(cm.exception.code, 2)
        self.assertIn('not allowed with argument', stderr.getvalue())


def _ko_log(mode=0x08, ad4=0xFFFFFFFF, b0c=1, poke=None, jump=False, winner_ko=False):
    """A two-human log with a hand-dealt KO of side 1, built on _s()."""
    L = [_s(f, mode=0x10, b1f=1) for f in range(0x2F0, 0x300)]
    L += [_s(f, mode=0x10) for f in range(0x300, 0x310)]
    L += [_s(0x400, mode=0x06, b1f=3, new=0x01000100,
             s1_5a=0x00 if jump else 0x10)]
    if poke is not None:
        L.append('W ms=0 f=0408 step=1 addr=%08X len=1 was=10 now=78 late=0 race=0' % poke)
    if jump:
        L.append(_s(0x410, mode=0x06, b1f=3, new=0x01000100, s1_5a=0x78, w2=1))
    else:
        L.append(_s(0x410, mode=0x06, b1f=3, new=0x01000100, s1_5a=0x78, w2=1,
                    s0_5a=0x78 if winner_ko else 0))
    L += [_s(0x420, mode=mode, b1f=3, w2=1, ad4=ad4, b0c=b0c)]
    L += ['X ms=0 f=0430 step=1 end']
    return L


class TestKO(unittest.TestCase):
    """The hand-dealt KO evidence (plan 2026-10-26-fought-match-truth §F3): a
    fought KO is the loser's +0x5A reaching 0x78 from a live value in a mode-6
    entry that no poke wrote and that did not time out, and the result naming
    the winner."""

    def test_a_hand_dealt_ko_passes_and_reports_death_done(self):
        r = gt.ko(_ko_log(), winner=0)
        self.assertEqual(r['fail'], [])
        self.assertEqual(r['ko'], 0x410)
        self.assertEqual(r['death'], 0x420)

    def test_a_time_out_fails(self):
        lines = [_s(0x400, mode=0x06, b1f=3, new=0x01000100, s1_5a=0x10),
                 _s(0x410, mode=0x07, b1f=3, s1_5a=0x10)]
        self.assertIn('time-out', ' '.join(gt.ko(lines)['fail']))

    def test_a_poked_ko_fails(self):
        self.assertIn('poke', ' '.join(gt.ko(_ko_log(poke=0x10789E))['fail']))

    def test_a_jump_from_zero_to_ko_fails(self):
        self.assertTrue(gt.ko(_ko_log(jump=True))['fail'])

    def test_the_winner_must_not_be_ko_d(self):
        self.assertTrue(gt.ko(_ko_log(winner_ko=True), winner=0)['fail'])

    def test_a_draw_or_wrong_side_result_fails(self):
        self.assertTrue(gt.ko(_ko_log(mode=0x09, ad4=2, b0c=0))['fail'])

    def _raw_shape(self, before, last):
        """The measured shape (record §F3.2): the KO and the mode change share the
        iteration that deals it, so the last mode-6 records still read a live
        value and the first 0x78 is the mode-8 result record's."""
        return ([_s(f, mode=0x10, b1f=1) for f in range(0x2F0, 0x300)] +
                [_s(f, mode=0x10) for f in range(0x300, 0x310)] +
                [_s(0x400, mode=0x06, new=0x01000100, s1_5a=before),
                 _s(0x410, mode=0x06, new=0x01000100, s1_5a=last),
                 _s(0x420, mode=0x08, w2=1, s1_5a=0x78, ad4=0xFFFFFFFF),
                 'X ms=0 f=0430 step=1 end'])

    def test_a_ko_seen_on_the_round_result_passes(self):
        # gp-u14-win-live's fought KO: mode 6 f=894 s0_5a=71, mode 8 f=898 s0_5a=78
        r = gt.ko(self._raw_shape(0x10, 0x71))
        self.assertEqual((r['fail'], r['ko'], r['round'], r['result'], r['death']),
                         ([], 0x420, 0x420, None, None))

    def test_a_zero_to_ko_jump_on_the_result_record_fails(self):
        # gp-u9-win's poke-shaped jump (mode 6 f=489 at 0, mode 8 f=48D at 78) with
        # no W record at all: the live-value rule alone rejects it
        self.assertIn('jumps to 78', ' '.join(gt.ko(self._raw_shape(0, 0))['fail']))

    def test_a_capture_without_the_win_fields_fails(self):
        # gp-twop/gp-u16-twop2/gp-u23's shape: the damage bytes (SNAP_FIELDS) exist
        # but ad4/w2/w3/b0c (WIN_EXTRA) do not, so no KO and no result can be shown
        def strip(line):
            return (gs.format_s(0, gs.parse(line), 0, 0x1E, 0x1E, gs.SNAP_FIELDS)
                    if line.startswith('S ') else line)
        self.assertIn('no WIN fields', ' '.join(gt.ko([strip(l) for l in _ko_log()])['fail']))

    def test_a_one_human_log_fails_on_the_two_human_claim(self):
        lines = [l.replace('b1f=03', 'b1f=01') for l in _ko_log()]
        self.assertIn('b1f never reaches 3', ' '.join(gt.ko(lines)['fail']))

    def test_the_round_result_must_name_the_winner(self):
        # ad4 stays NO_RESULT (the match is still open) but w3 = 1: the round went
        # to side 1, so side 0 is not the winner (0x27C48's counter)
        lines = _ko_log()
        lines[-2] = _s(0x420, mode=0x08, b1f=3, w3=1, ad4=0xFFFFFFFF, b0c=1)
        self.assertIn('names side 1 (w3 = 1', ' '.join(gt.ko(lines)['fail']))

    def test_the_match_result_must_name_the_winner(self):
        # the round result names side 0, but the match result's counter is side 1's
        lines = _ko_log()
        lines.insert(-1, _s(0x425, mode=0x09, b1f=3, w3=1, ad4=0, b0c=1))
        self.assertIn('match result at f=425 names side 1 (w3 = 1',
                      ' '.join(gt.ko(lines)['fail']))

    def test_a_round_two_ko_after_a_split_and_a_two_one_match_pass(self):
        # w2/w3 are cumulative (0x27C48): round 1 is side 1's (w2=0, w3=1), so the
        # round-2 result after side 0's KO is level (1-1) and names side 0 only by
        # its counter rising there; the 2-1 match result then has w2=2 > w3=1.
        # The old w[winner] > 0 and w[loser] == 0 rule rejected both (review fix).
        L = [_s(f, mode=0x10, b1f=1) for f in range(0x2F0, 0x300)]
        L += [_s(f, mode=0x10) for f in range(0x300, 0x310)]
        L += [_s(0x400, mode=0x06, b1f=3, new=0x01000100, s0_5a=0x10, s1_5a=0)]
        L += [_s(0x410, mode=0x06, b1f=3, new=0x01000100, s0_5a=0x78, s1_5a=0)]
        L += [_s(0x420, mode=0x08, b1f=3, w2=0, w3=1, ad4=0xFFFFFFFF)]
        L += [_s(0x500, mode=0x06, b1f=3, new=0x01000100, s1_5a=0x10, w2=0, w3=1)]
        L += [_s(0x510, mode=0x06, b1f=3, new=0x01000100, s1_5a=0x78, w2=1, w3=1)]
        L += [_s(0x520, mode=0x08, b1f=3, w2=1, w3=1, ad4=0xFFFFFFFF)]
        L += [_s(0x600, mode=0x06, b1f=3, new=0x01000100, s1_5a=0x10, w2=1, w3=1)]
        L += [_s(0x610, mode=0x06, b1f=3, new=0x01000100, s1_5a=0x78, w2=2, w3=1)]
        L += [_s(0x620, mode=0x09, b1f=3, w2=2, w3=1, ad4=0)]
        L += ['X ms=0 f=0630 step=1 end']
        r = gt.ko(L, winner=0)
        self.assertEqual(r['fail'], [])
        self.assertEqual((r['ko'], r['round'], r['result']), (0x510, 0x520, 0x620))

    def test_a_winner_1_hand_dealt_ko_passes(self):
        # the mirror of _ko_log: side 0 KO'd, side 1's counter and ad4 name it
        # (the winner-1 pass path had no passing case, review fix)
        L = [_s(f, mode=0x10, b1f=1) for f in range(0x2F0, 0x300)]
        L += [_s(f, mode=0x10) for f in range(0x300, 0x310)]
        L += [_s(0x400, mode=0x06, b1f=3, new=0x01000100, s0_5a=0x10)]
        L += [_s(0x410, mode=0x06, b1f=3, new=0x01000100, s0_5a=0x78)]
        L += [_s(0x420, mode=0x09, b1f=3, w2=0, w3=1, ad4=1)]
        L += ['X ms=0 f=0430 step=1 end']
        r = gt.ko(L, winner=1)
        self.assertEqual(r['fail'], [])
        self.assertEqual((r['ko'], r['result']), (0x410, 0x420))

    def test_a_stale_death_byte_is_not_this_kos_death(self):
        # b0c is not cleared between rounds (spec §7.2): a prior round's 1 at the
        # KO is not a 0 -> 1 transition, so no death frame is reported
        L = [_s(f, mode=0x10, b1f=1) for f in range(0x2F0, 0x300)]
        L += [_s(f, mode=0x10) for f in range(0x300, 0x310)]
        L += [_s(0x400, mode=0x06, b1f=3, new=0x01000100, s1_5a=0x10, b0c=1)]
        L += [_s(0x410, mode=0x06, b1f=3, new=0x01000100, s1_5a=0x78, b0c=1)]
        L += [_s(0x420, mode=0x08, b1f=3, w2=1, ad4=0xFFFFFFFF, b0c=1)]
        L += ['X ms=0 f=0430 step=1 end']
        r = gt.ko(L)
        self.assertEqual(r['fail'], [])
        self.assertIsNone(r['death'])

    def test_a_ko_with_no_result_after_it_fails(self):
        # a capture that stops at the KO: the damage byte alone is not the round's
        # result, which is what names the winner
        lines = _ko_log()[:-2] + ['X ms=0 f=0430 step=1 end']
        self.assertIn('no result after the KO', ' '.join(gt.ko(lines)['fail']))

    def _run(self, lines, args):
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, 'poll.log'), 'w') as f:
                f.write('\n'.join(lines) + '\n')
            stdout = io.StringIO()
            with mock.patch.object(sys, 'argv', ['gp_twop.py', 'check', '--capture', d] + args):
                with contextlib.redirect_stdout(stdout):
                    rc = gt.main()
        return rc, stdout.getvalue()

    def test_cli_ko_passes_a_hand_dealt_ko(self):
        rc, out = self._run(_ko_log(), ['--ko'])
        self.assertEqual(rc, 0)
        self.assertIn('hand-dealt KO: ok', out)

    def test_cli_ko_passes_the_winner_side(self):
        rc, out = self._run(_ko_log(), ['--ko', '--winner', '1'])
        self.assertEqual(rc, 1)
        self.assertIn('side 0', out)

    def test_cli_ko_is_mutually_exclusive_with_join_and_finisher(self):
        for other in ('--join', '--finisher'):
            stderr = io.StringIO()
            with mock.patch.object(sys, 'argv', ['gp_twop.py', 'check', '--capture', 'capture',
                                                 '--ko', other]):
                with contextlib.redirect_stderr(stderr):
                    with self.assertRaises(SystemExit) as cm:
                        gt.main()
            self.assertEqual(cm.exception.code, 2)
            self.assertIn('not allowed with argument', stderr.getvalue())

    def test_cli_ko_accepts_t_records_from_a_trace(self):
        # --trace is the same check over a port trace.txt (the T records carry the
        # same fields); load() reads any T-record file as a trace, not only one
        # named trace.txt (review fix: the basename rule was undocumented and
        # --ko's snapshots are S-only otherwise).
        lines = [('T' + l[1:]) if l.startswith('S ') else l for l in _ko_log()]
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'dump.txt')
            with open(path, 'w') as f:
                f.write('\n'.join(lines) + '\n')
            stdout = io.StringIO()
            with mock.patch.object(sys, 'argv',
                                   ['gp_twop.py', 'check', '--ko', '--trace', path]):
                with contextlib.redirect_stdout(stdout):
                    rc = gt.main()
        self.assertEqual(rc, 0)
        self.assertIn('hand-dealt KO: ok', stdout.getvalue())


if __name__ == '__main__':
    unittest.main()
