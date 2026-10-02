# tools/tests/test_gp_twop.py (gameplay U7: the gp-twop scenario and the two-human check)
import os, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
import gp_twop as gt


def _s(f, mode=6, b1f=3, cred=4, new=0, held=0, e0=None, e2=None):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, mode=mode, b1f=b1f, cred=cred, new=new, held=held, t508=1, t50c=2,
                e0=gt.pad_word(new, held, 0) if e0 is None else e0,
                e2=gt.pad_word(new, held, 1) if e2 is None else e2)
    return gs.format_s(0, vals, 0, 0x1E, 0x1E)


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


if __name__ == '__main__':
    unittest.main()
