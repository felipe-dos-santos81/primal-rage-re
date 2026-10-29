# tools/tests/test_gp_endurance.py (state coverage Task 5: ENDURANCE's advance and match end)
import io
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_endurance as ge
import gp_session as gs


def _rec(kind, f, mode, kw):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, mode=mode, t508=1, t50c=2, **kw)
    s = gs.format_s(0, vals, 0, 0x1E, 0x1E)
    return s if kind == 'S' else 'T ' + s[len('S ms=0 '):].rsplit(' kb=', 1)[0]


def _log(path, kind='S'):
    out = [] if kind == 'T' else ['B ms=0 base=00266000 ptr=0000FE20']
    for f0, f1, mode, kw in path:
        for f in range(f0, f1 + 1):
            out.append(_rec(kind, f, mode, kw))
    return out


# The ENDURANCE match-end path (frames modelled on gp-u12's mode path): the MAIN
# MENU, the ENDURANCE row, the team select, the team-pass wipe (mode 0x17 entry
# 1), mode 0x30 (the counters reset), mode 0x31's four entries with the loser
# (side 1, P2) walking its roster c1 = 04 -> 06 -> 01 -> 03 and af1 0 -> 1 -> 2
# -> 3, mode 0x33 (af1 = 4), and the results screen (mode 0x17 entry 2, b25 = 0,
# the winner side 0 w10d = 0). The KO'd side is side 1, so the counter that
# reaches 4 is af1 (DS_00104AF1), not af0: game_mode_32_step increments
# DS_00104AF0[s] for s = DS_00104B09, the side whose +0x5A reached 0x78.
PATH = [
    (0x100, 0x120, 0x27, {}),
    (0x121, 0x130, 0x2B, {}),
    (0x131, 0x140, 0x10, {}),
    (0x141, 0x150, 0x17, {}),                       # team-pass wipe: mode 0x17 entry 1
    (0x151, 0x160, 0x30, dict(af0=0, af1=0)),       # the fight prep resets the counters
    (0x161, 0x170, 0x31, dict(c1=4, af1=0)),        # fight entry 1: P2 is class 4
    (0x171, 0x180, 0x32, dict(c1=4, af1=1)),        # KO 1 advances the roster
    (0x181, 0x190, 0x31, dict(c1=6, af1=1)),        # re-entry 2
    (0x191, 0x1A0, 0x32, dict(c1=6, af1=2)),        # KO 2
    (0x1A1, 0x1B0, 0x31, dict(c1=1, af1=2)),        # re-entry 3
    (0x1B1, 0x1C0, 0x32, dict(c1=1, af1=3)),        # KO 3
    (0x1C1, 0x1D0, 0x31, dict(c1=3, af1=3)),        # re-entry 4
    (0x1D1, 0x1E0, 0x32, dict(c1=3, af1=4)),        # KO 4: af1 = 4
    (0x1E1, 0x1F0, 0x33, dict(af1=4, w10d=0, b25=3)),   # the match-end wait
    (0x1F1, 0x200, 0x17, dict(b25=0, w10d=0)),      # the results screen: 0x17 entry 2
]


class TestEvidence(unittest.TestCase):
    def run_check(self, lines):
        out = io.StringIO()
        rc = ge.evidence('gp-u25-endurance-end', lines, lambda s: out.write(s + '\n'))
        return rc, out.getvalue()

    def test_the_endurance_match_end_passes(self):
        rc, text = self.run_check(_log(PATH))
        self.assertEqual(rc, 0, text)
        self.assertIn('7/7 milestones ok', text)

    def test_the_roster_walks_four_characters(self):
        rc, text = self.run_check(_log(PATH))
        self.assertEqual(rc, 0, text)
        for cls in ('c1=4', 'c1=6', 'c1=1', 'c1=3'):
            self.assertIn(cls, text)

    def test_a_three_ko_roster_fails(self):
        # only three KOs: af1 reaches 3, mode 0x33 is never entered
        rc, text = self.run_check(_log(PATH[:12]))
        self.assertEqual(rc, 1)
        self.assertIn('mode 0x33', text)

    def test_a_missing_mode_31_reentry_fails(self):
        # the four KOs run back to back with no mode-0x31 re-entry between them,
        # so the mode-0x31 entry-2 milestone (the re-entry) is missing
        merged = PATH[:6] + [
            (0x171, 0x1E0, 0x32, dict(c1=3, af1=4)),  # one continuous mode-0x32 run
        ] + PATH[13:]
        rc, text = self.run_check(_log(merged))
        self.assertEqual(rc, 1)
        self.assertIn('mode 0x31', text)

    def test_the_counter_is_af1_not_af0(self):
        # the loser is side 1 (P2), so af1 reaches 4 while af0 stays 0; a log
        # that credits side 0 instead (af0=4) fails the mode-0x31-entry rows,
        # whose c1 (side 1's char) can no longer walk the roster
        wrong = [(f0, f1, m, dict(kw, af0=4, af1=0) if m in (0x31, 0x33) else kw)
                 for f0, f1, m, kw in PATH]
        rc, text = self.run_check(_log(wrong))
        self.assertEqual(rc, 1)

    def test_the_results_screen_requires_b25_cleared_and_the_winner(self):
        bad = PATH[:-1] + [(0x1F1, 0x200, 0x17, dict(b25=3, w10d=0))]
        rc, text = self.run_check(_log(bad))
        self.assertEqual(rc, 1)
        self.assertIn('results screen', text)


class TestPath(unittest.TestCase):
    def test_the_same_path_reproduces_every_milestone(self):
        n, rows = ge.reproduced('gp-u25-endurance-end', _log(PATH), _log(PATH, 'T'))
        self.assertEqual((n, len(rows)), (7, 7))
        out = io.StringIO()
        rc = ge.path('gp-u25-endurance-end', _log(PATH), _log(PATH, 'T'), 7, None,
                     lambda s: out.write(s + '\n'))
        self.assertEqual(rc, 0)
        self.assertIn('0 not reproduced through 6; ratchet N 7 ok', out.getvalue())

    def test_a_capture_without_mode_27_fails_cleanly(self):
        out = io.StringIO()
        rc = ge.path('gp-u25-endurance-end', _log([(0x100, 0x110, 0x03, {})]),
                     _log(PATH, 'T'), 0, None, lambda s: out.write(s + '\n'))
        self.assertEqual(rc, 1)
        self.assertIn('FAIL: mode 0x27 never observed', out.getvalue())


if __name__ == '__main__':
    unittest.main()
