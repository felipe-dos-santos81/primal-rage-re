# tools/tests/test_gp_u16_state.py (U16 R2: the s0_54 set/exit transition reader)
import os, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
import gp_u16_state as gu

TOOL = os.path.join(ROOT, 'tools', 'gp_u16_state.py')
SET_F, EXIT_F = 0x1532, 0x1533      # the old U23 capture's marker (record §F5.6)


def _s(f, mode=0x09, s0_54=0, s0_30=0x09C0, s0_42=0x8000, fields=gs.SNAP_FIELDS):
    vals = {n: 0 for n, _, _ in fields}
    vals.update(f=f, mode=mode, s0_54=s0_54, s0_30=s0_30, s0_42=s0_42, t508=1, t50c=2)
    return gs.format_s(0, vals, 0, 0x1E, 0x1E, fields)


def _log(set_at=None, exit_at=None, skip=(), upto=0x1539):
    """A minimal recovery-shaped log: mode 6 to 0x1485, mode 9 after it. `set_at` /
    `exit_at` are (frame, s0_30): the s0_54 = 2 and 0 records, adjacent by default
    (one iteration apart, the marker's shape). `skip` leaves a frame out (the
    poller's missed frame)."""
    L = ['B ms=0 base=00266000 ptr=0000FE20']
    for f in range(0x1480, upto):
        if f in skip:
            continue
        mode = 0x06 if f < 0x1486 else 0x09
        v54, v30 = 0, (0x0B40 if mode == 0x06 else 0x09C0)
        if set_at is not None and f == set_at[0]:
            v54, v30 = 2, set_at[1]
        if exit_at is not None and f == exit_at[0]:
            v54, v30 = 0, exit_at[1]
        L.append(_s(f, mode=mode, s0_54=v54, s0_30=v30))
    L.append('X ms=4 f=%04X step=76 end' % (upto - 1))
    return L


def _labels(res):
    return [label for label, _ in gu.report(res)]


class TestRead(unittest.TestCase):
    def test_a_complete_transition_reads_the_pair_and_the_states(self):
        res = gu.read(_log(set_at=(SET_F, 0x1234), exit_at=(EXIT_F, 0x1180)))
        self.assertTrue(res['has_54'] and res['has_state'])
        self.assertFalse(res['set_missing'])
        self.assertEqual((res['set_frame'], res['exit_frame']), (SET_F, EXIT_F))
        self.assertEqual(res['set_state'], dict(frame=SET_F, s0_54=2, s0_30=0x1234, s0_42=0x8000))
        self.assertEqual(res['exit_state'], dict(frame=EXIT_F, s0_54=0, s0_30=0x1180, s0_42=0x8000))
        # the gate reads the exit's previous iteration (0x48599 cmp [eax+0x30]); here
        # that is the set frame itself, one iteration before the exit
        self.assertEqual((res['gate_frame'], res['gate_state']['s0_30']), (SET_F, 0x1234))
        self.assertEqual(res['gate_relation'], 'below')
        rows = gu.report(res)
        self.assertIn('set frame', _labels(res))
        self.assertIn('exit frame', _labels(res))
        self.assertIn(('gate input y(1532)', 'present: s0_30=0x1234 (below 0x1400)'), rows)

    def test_a_missing_set_reports_it_and_the_old_marker_names_the_exit(self):
        # the test's fail-ability proof: the same log with the s0_54 = 2 record removed
        res = gu.read(_log(exit_at=(EXIT_F, 0x1180), skip=(SET_F,)))
        self.assertTrue(res['set_missing'])
        self.assertIsNone(res['set_frame'])
        self.assertTrue(res['anchored'])
        self.assertEqual(res['exit_frame'], EXIT_F)
        self.assertEqual(res['exit_state'], dict(frame=EXIT_F, s0_54=0, s0_30=0x1180, s0_42=0x8000))
        self.assertIsNone(res['gate_state'])              # y(0x1532) is not in the log
        self.assertIsNone(res['gate_relation'])
        rows = gu.report(res)
        self.assertIn('set frame missing', _labels(res))
        self.assertIn(('gate input y(1532)',
                       'absent: no S record at f=1532 (the previous iteration was not sampled)'), rows)
        self.assertIn("exit frame (the old U23 marker's, §F5.6)", _labels(res))

    def test_the_exit_gate_reads_the_previous_iteration_not_the_exit(self):
        # the port's own exit: 2 held for a run, the clear when the previous frame's
        # y falls below 0x1400 (0x155A's 0x147E is still above, 0x155B's 0x1104 is not)
        log = [_s(0x1486, s0_54=0), _s(0x1520, s0_54=2, s0_30=0x202E),
               _s(0x155B, s0_54=2, s0_30=0x1104), _s(0x155C, s0_54=0, s0_30=0x1180)]
        res = gu.read(log)
        self.assertEqual((res['set_frame'], res['exit_frame']), (0x1520, 0x155C))
        self.assertEqual(res['gate_frame'], 0x155B)
        self.assertEqual(res['gate_state']['s0_30'], 0x1104)
        self.assertEqual(res['gate_relation'], 'below')
        # the set's own y is above the gate: the set frame is not the deciding input
        self.assertEqual(res['set_state']['s0_30'], 0x202E)

    def test_a_marker_frame_outside_mode_9_is_not_an_exit(self):
        # .failed.run1's shape: the run never reached the finisher, so the old
        # marker's exit frame is a mode-6 record. Reporting it as an exit would be
        # a fabricated measurement.
        L = [_s(f, mode=0x06, s0_30=0x0B40) for f in range(0x1530, 0x1536)]
        L.append(_s(0x2000, mode=0x09))
        res = gu.read(L)
        self.assertEqual(res['mode9'], 0x2000)
        self.assertTrue(res['set_missing'])
        self.assertFalse(res['anchored'])
        self.assertIsNone(res['exit_frame'])
        self.assertIn('exit frame missing', _labels(res))

    def test_an_old_capture_without_the_state_fields_still_reads_the_frames(self):
        # the old U23 capture predates s0_30/s0_42 (record §F5.4): the transition
        # frames are still readable, the gate input is not
        fields = tuple((n, d, s) for n, d, s in gs.SNAP_FIELDS if n not in ('s0_30', 's0_42'))
        L = [_s(0x1486, s0_54=0, fields=fields), _s(SET_F, s0_54=2, fields=fields),
             _s(EXIT_F, s0_54=0, fields=fields)]
        res = gu.read(L)
        self.assertFalse(res['has_state'])
        self.assertEqual((res['set_frame'], res['exit_frame']), (SET_F, EXIT_F))
        self.assertEqual(res['set_state'], dict(frame=SET_F, s0_54=2, s0_30=None, s0_42=None))
        self.assertIn(('gate input y(1532)', 'unreadable: the S records have no s0_30'),
                      gu.report(res))

    def test_a_log_without_s0_54_is_not_a_capture(self):
        vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS if n != 's0_54'}
        L = [gs.format_s(0, dict(vals, f=0x100, mode=3), 0, 0x1E, 0x1E,
                         tuple((n, d, s) for n, d, s in gs.SNAP_FIELDS if n != 's0_54'))]
        res = gu.read(L)
        self.assertFalse(res['has_54'])
        self.assertIn('no s0_54', gu.report(res)[0][1])

    def test_a_port_trace_is_read_the_same_way(self):
        # a trace.txt's T records carry the same fields (PR_GP_DUMP), so the marker
        # compares the replay's side with the capture's
        T = ['T f=%04X mode=%04X s0_54=%02X s0_30=%08X s0_42=%04X'
             % (SET_F, 9, 2, 0x202E, 0x8000), 'T f=%04X mode=%04X s0_54=%02X s0_30=%08X s0_42=%04X'
             % (0x155C, 9, 0, 0x1180, 0x8000), 'T f=1486 mode=0009 s0_54=00 s0_30=00000BC0 s0_42=8000']
        res = gu.read(T)
        self.assertEqual((res['set_frame'], res['exit_frame']), (SET_F, 0x155C))
        self.assertEqual(res['gate_frame'], 0x155B)
        self.assertIsNone(res['gate_state'])                # 0x155B is not in the trace
        self.assertIsNone(res['gate_relation'])
        self.assertEqual(gu.path(T), ['f=1486 s0_54=00', 'f=1532 s0_54=02', 'f=155C s0_54=00'])


class TestCli(unittest.TestCase):
    def _run(self, log, *extra):
        d = tempfile.mkdtemp(prefix='gp-u16-state-')
        self.addCleanup(lambda: __import__('shutil').rmtree(d, True))
        with open(os.path.join(d, 'poll.log'), 'w') as f:
            f.write('\n'.join(log) + '\n')
        return subprocess.run([sys.executable, TOOL] + list(extra) + ['--capture', d],
                              capture_output=True, text=True)

    def test_read_and_path(self):
        r = self._run(_log(set_at=(SET_F, 0x1234), exit_at=(EXIT_F, 0x1180)), 'read')
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('set frame: f=1532', r.stdout)
        self.assertIn('exit state: f=1533 s0_54=0x0 s0_30=0x1180 s0_42=0x8000', r.stdout)
        r = self._run(_log(set_at=(SET_F, 0x1234), exit_at=(EXIT_F, 0x1180)), 'path')
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(r.stdout.splitlines(), ['f=1486 s0_54=00', 'f=1532 s0_54=02',
                                                 'f=1533 s0_54=00'])

    def test_the_marker_override_and_the_absent_log_exit_codes(self):
        # a missing marker override outside mode 9 leaves the exit unreported
        r = self._run(_log(set_at=None, exit_at=None), 'read', '--marker-exit', '0x1111')
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('set frame missing', r.stdout)
        self.assertIn('exit frame missing', r.stdout)
        # a poll.log without s0_54 is refused
        vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS if n != 's0_54'}
        fields = tuple((n, d, s) for n, d, s in gs.SNAP_FIELDS if n != 's0_54')
        L = [gs.format_s(0, dict(vals, f=0x100, mode=3), 0, 0x1E, 0x1E, fields)]
        self.assertEqual(self._run(L, 'read').returncode, 2)
        # and so is a directory with no log at all
        d = tempfile.mkdtemp(prefix='gp-u16-state-')
        self.addCleanup(lambda: __import__('shutil').rmtree(d, True))
        r = subprocess.run([sys.executable, TOOL, 'read', '--capture', d],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)


if __name__ == '__main__':
    unittest.main()
