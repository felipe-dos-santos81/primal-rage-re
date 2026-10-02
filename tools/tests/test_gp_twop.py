# tools/tests/test_gp_twop.py (gameplay U7: the gp-twop scenario and the two-human check)
import os, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs


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


if __name__ == '__main__':
    unittest.main()
