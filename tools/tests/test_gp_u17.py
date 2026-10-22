# tools/tests/test_gp_u17.py — host input truth U17 (record §H6)
import os, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
import gp_u17 as gu

TOOL = os.path.join(ROOT, 'tools', 'gp_u17.py')
STEPS = gs.SCENARIOS['gp-u17-device2']['steps']
LEAVE_F = 0x11F          # the leave's I record: it fires in the spin of F - 1


def _s(f, mode=0x27, dev1=0, dev2=0):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, mode=mode, dev1=dev1, dev2=dev2, t508=1, t50c=2)
    return gs.format_s(0, vals, 0, 0x1E, 0x30)


def _log(dev1_at=None, dev1_sticky=None, dev2=0, mode_at=None, last=('p1.down', 'esc'),
         dev_fields=True):
    """A minimal gp-u17-device2 poll.log: the boot in mode 3, the menu in mode
    0x27, the walk's last two presses, the selection (`dev1_at` = {f: value}) and
    the word after it (`dev1_sticky`)."""
    fields = gs.SNAP_FIELDS if dev_fields else tuple(
        (n, d, s) for n, d, s in gs.SNAP_FIELDS if n not in gs.DEV_FIELDS)
    L = ['B ms=0 base=00266000 ptr=0000FE20']
    for f in range(0x100, 0x106):
        m = (mode_at or {}).get(f, 3 if f < 0x105 else 0x27)
        v = {n: 0 for n, _, _ in fields}
        v.update(f=f, mode=m, dev2=dev2, t508=1, t50c=2)
        L.append(gs.format_s(0, v, 0, 0x1E, 0x30, fields))
    sel = dev1_at or {}
    for f in range(0x106, 0x130):
        v = {n: 0 for n, _, _ in fields}
        m = (mode_at or {}).get(f, 0x27)
        d1 = sel.get(f, 0)
        if dev1_sticky is not None and f > LEAVE_F:
            d1 = dev1_sticky
        v.update(f=f, mode=m, dev1=d1, dev2=dev2, t508=1, t50c=2)
        L.append(gs.format_s(0, v, 0, 0x1E, 0x30, fields))
    L.append('I ms=1 f=%04X step=25 press=%s scan=2D lin=0000FE4B old=AD bios=2D78 ring=0 late=0'
             % (LEAVE_F - 1, last[0]))
    L.append('I ms=2 f=%04X step=26 press=%s scan=01 lin=0000FE1F old=81 bios=011B ring=0 late=0'
             % (LEAVE_F, last[1]))
    L.append('X ms=3 f=012F step=27 end')
    return L


def _rows(lines, **kw):
    return gu.check(lines, **kw)


class TestCheck(unittest.TestCase):
    def test_a_clean_capture_passes_and_dates_the_clear(self):
        rows = _rows(_log(dev1_at={0x120: 2}))
        self.assertTrue(all(r[1] for r in rows), rows)
        self.assertIn(('clear frame', True, 'f=121 (289)'), rows)

    def test_the_clear_frame_pin(self):
        self.assertTrue(all(r[1] for r in _rows(_log(dev1_at={0x120: 2}), clear_frame=0x121)))
        rows = _rows(_log(dev1_at={0x120: 2}), clear_frame=0x120)
        self.assertFalse(all(r[1] for r in rows))
        self.assertIn(('the clear frame is the pinned one', False,
                       'measured f=121, pinned f=120'), rows)

    def test_a_sticky_word_fails(self):
        # the regression this pins: the presence answer never clears the word
        rows = _rows(_log(dev1_at={0x120: 2}, dev1_sticky=2))
        self.assertFalse(all(r[1] for r in rows))
        self.assertIn(('the clear is observed after the leave', False,
                       'leave f=11F, first dev1 = 0 at none'), rows)
        # and the selection run itself is accepted (all of it reads 2)
        self.assertTrue([r for r in rows if r[0].startswith('the selection is seen')][0][1])

    def test_the_wrong_device_fails(self):
        rows = _rows(_log(dev1_at={0x120: 4}))
        self.assertFalse([r for r in rows if r[0].startswith('the selection is seen')][0][1])

    def test_the_expected_device_is_a_parameter(self):
        # one press is one +2 step by the 0x0C-tick repeat gate (module docstring); a
        # measured capture that disagrees passes its value, and the label names it
        rows = _rows(_log(dev1_at={0x120: 4}), device=4)
        self.assertTrue(all(r[1] for r in rows), rows)
        self.assertIn('the selection is seen: dev1 = 4 (one p1.down from KEYBOARD)',
                      [r[0] for r in rows])
        self.assertIn(('the clear follows the selection', True,
                       'last dev1 = 4 at f=120, clear at f=121'), rows)
        rows = _rows(_log(dev1_at={0x120: 2}), device=4)
        self.assertFalse([r for r in rows if r[0].startswith('the selection is seen')][0][1])

    def test_an_old_capture_is_not_a_u17_capture(self):
        rows = _rows(_log(dev_fields=False))
        self.assertEqual(len(rows), 1)
        self.assertFalse(rows[0][1])

    def test_the_leave_must_be_the_scenario_s_last_two_presses(self):
        rows = _rows(_log(dev1_at={0x120: 2}, last=('p1.down', 'enter')))
        self.assertFalse([r for r in rows if r[0].startswith('the scenario ends')][0][1])

    def test_the_mode_path(self):
        rows = _rows(_log(dev1_at={0x120: 2}, mode_at={0x110: 0x28}))
        self.assertFalse([r for r in rows if r[0].startswith('mode stays')][0][1])

    def test_dev2_is_never_selected(self):
        rows = _rows(_log(dev1_at={0x120: 2}, dev2=0x2D4))
        self.assertFalse([r for r in rows if r[0].startswith('dev2 stays')][0][1])

    def test_an_unseen_selection_is_a_failure_unless_declared(self):
        rows = _rows(_log())
        self.assertFalse([r for r in rows if r[0].startswith('the selection is seen')][0][1])
        rows = _rows(_log(), unseen_ok=True)
        self.assertTrue(all(r[1] for r in rows), rows)
        self.assertIn(('clear frame', True, 'f=120 (288)'), rows)

    def test_no_leave_record(self):
        lines = [l for l in _log(dev1_at={0x120: 2}) if not l.startswith('I ')]
        rows = _rows(lines)
        self.assertFalse([r for r in rows if r[0].startswith('the leave key')][0][1])


class TestPath(unittest.TestCase):
    def test_one_line_per_change(self):
        rows = gu.path(_log(dev1_at={0x120: 2}))
        self.assertEqual(rows, ['f=100 dev1=0 dev2=0', 'f=120 dev1=2 dev2=0', 'f=121 dev1=0 dev2=0'])


class TestCli(unittest.TestCase):
    def _run(self, log, *extra):
        d = tempfile.mkdtemp(prefix='gp-u17-')
        self.addCleanup(lambda: __import__('shutil').rmtree(d, True))
        with open(os.path.join(d, 'poll.log'), 'w') as f:
            f.write('\n'.join(log) + '\n')
        env = dict(os.environ)
        env.pop('PR_ORACLE_REQUIRED', None)
        return subprocess.run([sys.executable, TOOL, 'check', '--capture', d] + list(extra),
                              capture_output=True, text=True, env=env)

    def test_ok_and_fail_exit_codes(self):
        r = self._run(_log(dev1_at={0x120: 2}))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('check=ok gp-u17-device2', r.stdout)
        r = self._run(_log(dev1_at={0x120: 2}), '--clear-frame', '0x120')
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('check=FAIL gp-u17-device2', r.stdout)

    def test_a_missing_poll_log_exits_2(self):
        d = tempfile.mkdtemp(prefix='gp-u17-')
        self.addCleanup(lambda: __import__('shutil').rmtree(d, True))
        r = subprocess.run([sys.executable, TOOL, 'check', '--capture', d],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)


class TestScenario(unittest.TestCase):
    def test_the_steps_are_harness_values_a_capture_can_follow(self):
        # the boot Enter, the mode-0x27 Enter and one step per walk key, none of
        # them an until_mode (the mode never changes), and the end 60 frames on
        self.assertEqual([s[0] for s in STEPS[:2]], ['boot', 'after_mode'])
        self.assertTrue(all(s[0] == 'after' for s in STEPS[2:]))
        self.assertEqual(STEPS[-1], ('after', 60, ('end',)))
        # a pad step holds 5 frames (one level edge through 0x500C4's debounce),
        # a key step HOLD_FRAMES, and every gap is the 30-frame harness value
        for s in STEPS[2:-1]:
            act = s[-1]
            self.assertEqual(s[1], 30, s)
            if act[0] == 'pad':
                self.assertEqual((act[1], act[2]), (('p1.down',), 5), s)
            else:
                self.assertEqual(gs.expand(act)[0][3], gs.HOLD_FRAMES, s)
        self.assertEqual(gs.SCENARIOS['gp-u17-device2']['time_limit'], 90)
        self.assertNotIn('arm', gs.SCENARIOS['gp-u17-device2'])   # the Enter arm


if __name__ == '__main__':
    unittest.main()
