# tools/tests/test_gp_u17.py — host input truth U17 (record §H6)
import os, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
import gp_u17 as gu

TOOL = os.path.join(ROOT, 'tools', 'gp_u17.py')
SCENARIO = 'gp-u17-device2'
STEPS = gs.SCENARIOS[SCENARIO]['steps']
LEAVE_F = 0x11F          # the leave's I record: it fires in the spin of F - 1


def _s(f, mode=0x27, dev1=0, dev2=0, head=0x1E, tail=0x30):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, mode=mode, dev1=dev1, dev2=dev2, t508=1, t50c=2)
    return gs.format_s(0, vals, 0, head, tail)


def _log(dev1_at=None, dev1_sticky=None, dev2=0, mode_at=None, last=('down', 'esc', 'esc'),
         dev_fields=True):
    """A minimal gp-u17-device2 poll.log: the boot in mode 3, the menu in mode
    0x27, the walk's last three presses (the device step and the two leaves), the
    selection (`dev1_at` = {f: value}) and the word after it (`dev1_sticky`)."""
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
    for k, name in enumerate(last):
        L.append('I ms=%d f=%04X step=%d press=%s scan=%s lin=0000FE4B old=AD bios=%s ring=0 late=0'
                 % (k + 1, LEAVE_F - 2 + k, 24 + k, name,
                    '50' if name == 'down' else '01', '5000' if name == 'down' else '011B'))
    L.append('X ms=4 f=012F step=27 end')
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
        self.assertIn('the selection is seen: dev1 = 4 (one Down from KEYBOARD)',
                      [r[0] for r in rows])
        self.assertIn(('the clear follows the selection', True,
                       'last dev1 = 4 at f=120, clear at f=121'), rows)
        rows = _rows(_log(dev1_at={0x120: 2}), device=4)
        self.assertFalse([r for r in rows if r[0].startswith('the selection is seen')][0][1])

    def test_an_old_capture_is_not_a_u17_capture(self):
        rows = _rows(_log(dev_fields=False))
        self.assertEqual(len(rows), 1)
        self.assertFalse(rows[0][1])

    def test_the_leave_must_be_the_scenario_s_last_three_presses(self):
        rows = _rows(_log(dev1_at={0x120: 2}, last=('down', 'enter', 'esc')))
        self.assertFalse([r for r in rows if r[0].startswith('the scenario ends')][0][1])

    def test_the_mode_path(self):
        rows = _rows(_log(dev1_at={0x120: 2}, mode_at={0x110: 0x28}))
        self.assertFalse([r for r in rows if r[0].startswith('mode stays')][0][1])

    def test_dev2_is_never_selected(self):
        rows = _rows(_log(dev1_at={0x120: 2}, dev2=0x2D4))
        self.assertFalse([r for r in rows if r[0].startswith('dev2 stays')][0][1])

    def test_a_wall_paced_capture_does_not_sample_the_selection(self):
        # record §H6.9: the service menu's sub-loops have no spin/S record, so the
        # device write is unobservable in the poll; the claim is the end state
        rows = _rows(_log())
        self.assertTrue(all(r[1] for r in rows), rows)
        self.assertIn(('the selection is not sampled (wall-paced menu: no S record while one is open)',
                       True, 'pinned by the end state; the write is unobservable in the poll'), rows)
        # a nonzero sample still has to be the derived device
        rows = _rows(_log(dev1_at={0x120: 4}))
        self.assertFalse([r for r in rows if r[0].startswith('the selection is seen')][0][1])

    def test_no_leave_record(self):
        lines = [l for l in _log(dev1_at={0x120: 2}) if not l.startswith('I ')]
        rows = _rows(lines)
        self.assertFalse([r for r in rows if r[0].startswith('the leave key')][0][1])


class TestWallShape(unittest.TestCase):
    def test_a_wall_shaped_capture_passes_every_row(self):
        # record §H6.9: the shape a wall-paced run leaves -- the boot in mode 3, the
        # menu in 0x27, 25 keys pressed AND released (wall deadlines) at one frozen
        # frame, the master loop back after the OPTIONS MENU leave, the X record.
        # The harness's unscripted-input check must accept the frozen-frame keys.
        import gp_capture as gc
        names = [st[-1][1] for st in gs.SCENARIOS[SCENARIO]['steps'][2:-1]]
        L = ['B ms=0 base=00266000 ptr=0000FE20']
        for f in range(0x120, 0x130):
            L.append(_s(f, mode=3))
        for f in range(0x130, 0x140):
            L.append(_s(f))
        head = 0x1E
        for k, name in enumerate(names):
            scan, word = gs.KEYS[name]
            head += 2
            if head >= 0x3E:
                head = 0x1E
            L.append('I ms=%d f=%04X step=%d press=%s scan=%02X lin=%08X old=FF bios=%04X ring=1 late=0'
                     % (30000 + k * 1000, 0x140, 2 + k, name, scan, 0x10000 + scan, word))
            L.append('I ms=%d f=%04X step=%d release=%s lin=%08X'
                     % (30083 + k * 1000, 0x140, 2 + k, name, 0x10000 + scan))
        for f in range(0x141, 0x160):
            L.append(_s(f, head=head, tail=head))
        L.append('X ms=61000 f=015F step=27 end')
        self.assertEqual(gc.input_check(L), ('no unscripted input', True))
        rows = _rows(L)
        self.assertTrue(all(r[1] for r in rows), rows)
        self.assertIn(('clear frame', True, 'f=141 (321)'), rows)


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

    def test_the_capture_identity_pins(self):
        # record §H6.9: this scenario has no frame/trace replay yet, so the pinned
        # poll.log sha256 and frame count live in this tool
        import hashlib, shutil
        d = tempfile.mkdtemp(prefix='gp-u17-')
        self.addCleanup(shutil.rmtree, d, True)
        text = '\n'.join(_log(dev1_at={0x120: 2})) + '\n'
        poll = os.path.join(d, 'poll.log')
        with open(poll, 'w') as f:
            f.write(text)
        for k in range(2):
            with open(os.path.join(d, 'frame_%05d.raw.gz' % k), 'wb') as f:
                f.write(b'x')
        sha = hashlib.sha256(text.encode()).hexdigest()
        env = dict(os.environ)
        env.pop('PR_ORACLE_REQUIRED', None)

        def run(*extra):
            return subprocess.run([sys.executable, TOOL, 'check', '--capture', d] + list(extra),
                                  capture_output=True, text=True, env=env)

        self.assertEqual(run('--capture-sha256', sha, '--capture-frames', '2').returncode, 0)
        r = run('--capture-sha256', '0' * 64)
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('FAIL the capture identity matches the pin', r.stdout)
        r = run('--capture-frames', '3')
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('FAIL the frame count matches the pin', r.stdout)

    def test_a_missing_poll_log_exits_2(self):
        d = tempfile.mkdtemp(prefix='gp-u17-')
        self.addCleanup(lambda: __import__('shutil').rmtree(d, True))
        r = subprocess.run([sys.executable, TOOL, 'check', '--capture', d],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)


class TestScenario(unittest.TestCase):
    def test_the_steps_are_harness_values_a_capture_can_follow(self):
        # record §H6.9: the boot Enter and the mode-0x27 Enter are frame-keyed (the
        # MAIN MENU runs in the master loop); every walk key and the end are wall-
        # keyed, because the service menu's sub-loops freeze the frame counter
        self.assertEqual([s[0] for s in STEPS[:2]], ['boot', 'after_mode'])
        self.assertTrue(all(s[0] == 'after_wall' for s in STEPS[2:]))
        self.assertEqual(STEPS[-1], ('after_wall', 1.5, ('end',)))
        # each walk key is the K11 walk's own key, held 5 frames (83 ms: over one
        # 50 ms menu poll, under the 200 ms repeat gate), 1 s apart
        for s in STEPS[2:-1]:
            self.assertEqual(s[1], gs.U17_GAP, s)
            self.assertEqual(s[-1][0], 'key', s)
            self.assertEqual(gs.expand(s[-1])[0][3], gs.U17_HOLD, s)
        # the end fits: the boot wait + the mode-0x27 wait + 25 keys + 1.5 s
        self.assertEqual(gs.SCENARIOS['gp-u17-device2']['time_limit'], 90)
        self.assertLess(gs.ENTER_WAIT + 150 / 60.05 + 25 * gs.U17_GAP + 1.5, 90)
        self.assertTrue(gs.SCENARIOS['gp-u17-device2'].get('wall'))
        self.assertNotIn('arm', gs.SCENARIOS['gp-u17-device2'])   # the Enter arm


if __name__ == '__main__':
    unittest.main()
