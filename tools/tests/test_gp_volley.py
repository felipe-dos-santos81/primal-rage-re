# tools/tests/test_gp_volley.py (volleyball truth Task 4: the mode-0x21 entry)
import io
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
try:
    import gp_volley as gv
except ImportError:
    gv = None


def _s(f, mode=6, **kw):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS + gs.WIN_EXTRA}
    vals.update(dict(f=f, mode=mode, b1f=3, afc=0, abc=2, raw=0, kb=0))
    vals.update(kw)
    return gs.format_s(0, vals, 0, 0x1E, 0x1E, gs.SNAP_FIELDS + gs.WIN_EXTRA)


def _log(entry=0x1000, ret=0x16C0, score=1, clock=None, clears=None, **entry_kw):
    """Mode 6, then mode 0x21 from `entry` to `ret`, then mode 6. The default
    clock decrements 0x1E -> 0 one unit per S record; the side-1 score is 1 from
    the second mode snapshot; the return clears the mode's stores."""
    lines = [_s(0xFFF),
             'I ms=0 f=0FFF step=12 press=p1.b0 scan=16 lin=00000000 old=80 bios=1675 ring=1 late=0',
             'I ms=0 f=0FFF step=12 press=p2.b0 scan=47 lin=00000000 old=80 bios=4700 ring=1 late=0']
    clocks = list(clock) if clock is not None else list(range(0x1E, -1, -1))
    for k, clk in enumerate(clocks):
        f = entry + k
        s1 = 0 if k == 0 else score
        vals = dict(ball=0x1083F0, clk=clk, s0=0, s1=s1, b8c1=1, b8c5=1)
        vals.update(entry_kw if k == 0 else {})
        lines.append(_s(f, mode=0x21, **vals))
    back = dict(ball=0, clk=0, b8c5=0, s0=0, s1=0)
    if clears:
        back.update(clears)
    lines.append(_s(ret, mode=6, **back))
    lines.append('X ms=0 f=%04X step=24 end' % (ret + 0x40))
    return lines


def _check(lines, trace=None):
    out = io.StringIO()
    if gv is None or not callable(getattr(gv, 'evidence', None)):
        return 1, 'evidence missing'
    rc = gv.evidence('gp-u30-volleyball', lines, lambda s: out.write(s + '\n'), trace)
    return rc, out.getvalue()


def _drop(lines, names):
    """The same log with the named tokens removed from every S record (the
    missing-field and missing-afc paths)."""
    out = []
    for line in lines:
        if line.startswith('S '):
            line = ' '.join(p for p in line.split() if p.partition('=')[0] not in names)
        out.append(line)
    return out


class TestFieldsAndScenario(unittest.TestCase):
    def test_the_mode21_fields_are_appended_with_their_widths(self):
        self.assertEqual(gs.SNAP_FIELDS[-7:], (
            ('ball', 0x108864, 4), ('abc', 0x104ABC, 4),
            ('s0', 0x10889C, 1), ('s1', 0x10889D, 1), ('clk', 0x1088AA, 2),
            ('b8c1', 0x1088C1, 1), ('b8c5', 0x1088C5, 1)))
        vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
        vals.update(f=0x1234, ball=0x1083F0, abc=2, s0=0, s1=1, clk=0x1E,
                    b8c1=1, b8c5=1)
        line = gs.format_s(1, vals, 0, 0, 0)
        for name, want in (('ball', '001083F0'), ('abc', '00000002'), ('s0', '00'),
                           ('s1', '01'), ('clk', '001E'), ('b8c1', '01'),
                           ('b8c5', '01')):
            self.assertIn('%s=%s' % (name, want), line)
        self.assertEqual({n: gs.parse(line)[n] for n in gv.FIELDS}, {
            'ball': 0x1083F0, 'abc': 2, 's0': 0, 's1': 1, 'clk': 0x1E,
            'b8c1': 1, 'b8c5': 1})

    def test_the_scenario_reproduces_the_route_frames_and_stops_after_the_return(self):
        sc = gs.SCENARIOS['gp-u30-volleyball']
        self.assertEqual((sc['stop_mode'], sc['stop_delay']), (0x21, 1800))
        self.assertIn('gp-u30-volleyball', gs.STOP_AT_END)
        self.assertEqual(sc['extra'], gs.WIN_EXTRA)
        # The route's measured frames (record §2), as the scheduler targets them:
        # the boot at_frame is the press spin (319) + 1; the pads are the S
        # frames route.script's `bits` lines hold.
        steps = sc['steps']
        self.assertEqual(steps[0], ('at_frame', 320, ('key', 'enter')))
        self.assertEqual(steps[1][:2], ('after_mode', 0x27))
        self.assertEqual(steps[1][2], 150)
        self.assertEqual(steps[5][:3], ('after_mode', 0x06, 882))
        pads = [st for st in steps if st[-1][0] == 'pad']
        self.assertEqual(pads[0], ('after_mode', 0x10, 57, ('pad', ('p1.right',), 5)))
        self.assertEqual(pads[2], ('after_mode', 0x06, 882, ('pad', ('p2.start',), 5)))
        self.assertEqual(pads[-1], ('after', 70, ('pad', ('p2.b0',), 5)))
        # A mode-less run still has a replayable end.
        self.assertEqual(steps[-1], ('after', 700, ('end',)))

    def test_at_frame_is_an_absolute_target_not_a_delta(self):
        s = gs.Schedule((('at_frame', 320, ('key', 'enter')), ('after', 1, ('end',))))
        self.assertEqual(s.due(317), [])
        self.assertEqual(s.due(318), [])
        self.assertEqual(s.due(319), [(0, ('key', 'enter'))])
        self.assertEqual(s.frame_of[0], 320)
        self.assertIsNone(s.end_frame)
        self.assertEqual(s.due(320), [])
        self.assertEqual(s.end_frame, 321)


class TestChecker(unittest.TestCase):
    def test_a_clean_capture_passes(self):
        rc, text = _check(_log())
        self.assertEqual(rc, 0, text)
        self.assertIn('mode 0x21 at f=1000', text)
        self.assertIn('return to mode 6 at f=16C0', text)
        self.assertIn('not in the S fields', text)       # no --trace

    def test_a_poke_fails(self):
        lines = _log()
        lines.insert(1, 'W ms=0 f=0FFF step=12 addr=00104B1F len=1 was=02 now=03 late=0 race=0')
        rc, text = _check(lines)
        self.assertEqual(rc, 1)
        self.assertIn('poke (W record)', text)

    def test_an_entry_store_mutation_fails(self):
        # One store per case: the 0x4BD98/0x4BE19 values the mode's entry makes.
        for field, bad in (('b1f', 1), ('afc', 3), ('abc', 1), ('clk', 0x1D),
                           ('b8c1', 0), ('b8c5', 0), ('ball', 0), ('s1', 1)):
            rc, text = _check(_log(**{field: bad}))
            self.assertEqual(rc, 1, (field, text))
            self.assertIn(field, text)

    def test_a_clock_rise_or_a_multi_unit_fall_fails(self):
        rc, text = _check(_log(clock=[0x1E, 0x1D, 0x1E, 0]))
        self.assertEqual(rc, 1)
        self.assertIn('clk rose', text)
        rc, text = _check(_log(clock=[0x1E, 0x1C, 0x10, 0]))
        self.assertEqual(rc, 1)
        self.assertIn('more than one unit', text)

    def test_no_point_fails(self):
        rc, text = _check(_log(score=0))
        self.assertEqual(rc, 1)
        self.assertIn('no point', text)

    def test_a_return_clear_mutation_fails(self):
        for field, bad in (('clk', 1), ('ball', 0x1083F0), ('b8c5', 1),
                           ('s0', 1), ('s1', 1)):
            rc, text = _check(_log(clears={field: bad}))
            self.assertEqual(rc, 1, (field, text))
            self.assertIn('at the return', text)

    def test_a_minus_one_clk_at_the_end_fails(self):
        rc, text = _check(_log(clock=list(range(0x1E, 0, -1))))
        self.assertEqual(rc, 1)
        self.assertIn('want 0', text)

    def test_no_mode21_reports_the_residue(self):
        lines = [_s(0x10), _s(0x11, clk=0x1E), 'X ms=0 f=0020 step=4 end']
        rc, text = _check(lines)
        self.assertEqual(rc, 1)
        self.assertIn('mode 0x21 never observed', text)
        self.assertIn('clk max 30', text)

    def test_the_trace_entry_count(self):
        self.assertIsNotNone(gv)
        import tempfile
        out = io.StringIO()
        with tempfile.TemporaryDirectory() as d:
            tr = os.path.join(d, 'trace.txt')
            with open(tr, 'w') as f:
                f.write('V f=1000 e=001083F0 ty=06 fl=20 n=08 x=00000000 h=00000000 '
                        'd=00000000 vx=0000 vy=0000\n')
            rc = gv.trace_count(tr, 0x1000, lambda s: out.write(s + '\n'))
            self.assertEqual(rc, 0, out.getvalue())
            self.assertIn('001083F0', out.getvalue())
            self.assertIn('+0x1F=8', out.getvalue())
            # The count must be the measured entry, not just any type-6 V line.
            with open(tr, 'w') as f:
                f.write('V f=1000 e=00108414 ty=06 fl=20 n=08 x=00000000 h=00000000 '
                        'd=00000000 vx=0000 vy=0000\n')
            out = io.StringIO()
            rc = gv.trace_count(tr, 0x1000, lambda s: out.write(s + '\n'))
            self.assertEqual(rc, 1)
            self.assertIn('001083F0', out.getvalue())
            with open(tr, 'w') as f:
                f.write('V f=1000 e=001083F0 ty=06 fl=20 n=07 x=00000000 h=00000000 '
                        'd=00000000 vx=0000 vy=0000\n')
            out = io.StringIO()
            rc = gv.trace_count(tr, 0x1000, lambda s: out.write(s + '\n'))
            self.assertEqual(rc, 1)
            self.assertIn('want 8', out.getvalue())

    def test_the_trace_state_checks_the_port_entry_and_return(self):
        self.assertIsNotNone(gv)
        import tempfile

        bases = {
            'T entry': dict(mode='0021', b1f='03', afc='0000', abc='00000002',
                            ball='001083F0', clk='001E', s0='00', s1='00',
                            b8c1='01', b8c5='01'),
            'T return': dict(mode='0006', b1f='03', afc='0000', abc='00000002',
                             ball='00000000', clk='0000', s0='00', s1='00',
                             b8c1='01', b8c5='00'),
            'Vg entry': dict(b60='00', b61='00', ball='001083F0', abc='00000002',
                             s0='00', s1='00', clk='001E', c1='01', c5='01',
                             s0x='00000000', s0h='00000000', s0d='00000000',
                             s1x='00000000', s1h='00000000', s1d='00000000'),
            'Vg return': dict(b60='00', b61='00', ball='00000000', abc='00000002',
                              s0='00', s1='00', clk='0000', c1='01', c5='00',
                              s0x='00000000', s0h='00000000', s0d='00000000',
                              s1x='00000000', s1h='00000000', s1d='00000000'),
        }

        def trace(mut=None, drop_return=False, omit=()):
            vals = {k: dict(v) for k, v in bases.items()}
            if mut is not None:
                group, field, value = mut
                vals[group][field] = value

            def line(kind, f, d):
                return '%s f=%04X %s' % (
                    kind, f, ' '.join('%s=%s' % (k, v) for k, v in d.items()
                                      if not (kind == 'T' and f == 0x1000 and k in omit)))
            lines = [line('T', 0x1000, vals['T entry']),
                     line('Vg', 0x1000, vals['Vg entry'])]
            if not drop_return:
                lines += [line('T', 0x16C0, vals['T return']),
                          line('Vg', 0x16C0, vals['Vg return'])]
            return '\n'.join(lines) + '\n'

        mutations = [
            ('T entry', 'mode', '0006'), ('T entry', 'b1f', '01'),
            ('T entry', 'afc', '0003'), ('T entry', 'abc', '00000001'),
            ('T entry', 'ball', '00000000'), ('T entry', 'clk', '001D'),
            ('T entry', 's0', '01'), ('T entry', 's1', '01'),
            ('T entry', 'b8c1', '00'), ('T entry', 'b8c5', '00'),
            ('T return', 'mode', '0021'), ('T return', 'b1f', '01'),
            ('T return', 'afc', '0003'), ('T return', 'abc', '00000001'),
            ('T return', 'ball', '001083F0'), ('T return', 'clk', '0001'),
            ('T return', 's0', '01'), ('T return', 's1', '01'),
            ('T return', 'b8c1', '00'), ('T return', 'b8c5', '01'),
            ('Vg entry', 'ball', '00108414'), ('Vg entry', 'abc', '00000001'),
            ('Vg entry', 's0', '01'), ('Vg entry', 's1', '01'),
            ('Vg entry', 'clk', '001D'), ('Vg entry', 'c1', '00'),
            ('Vg entry', 'c5', '00'),
            ('Vg return', 'ball', '001083F0'), ('Vg return', 'abc', '00000001'),
            ('Vg return', 's0', '01'), ('Vg return', 's1', '01'),
            ('Vg return', 'clk', '0001'), ('Vg return', 'c1', '00'),
            ('Vg return', 'c5', '01'),
        ]
        with tempfile.TemporaryDirectory() as d:
            tr = os.path.join(d, 'trace.txt')
            with open(tr, 'w') as f:
                f.write(trace())
            out = io.StringIO()
            self.assertEqual(gv.trace_state(tr, 0x1000, 0x16C0,
                                            lambda s: out.write(s + '\n')), 0,
                             out.getvalue())
            for group, field, value in mutations:
                with open(tr, 'w') as f:
                    f.write(trace(mut=(group, field, value)))
                out = io.StringIO()
                rc = gv.trace_state(tr, 0x1000, 0x16C0, lambda s: out.write(s + '\n'))
                self.assertEqual(rc, 1, (group, field, value, out.getvalue()))
                self.assertIn(field, out.getvalue(), (group, field, value))
            # A T line with the field token absent is the same failure (the
            # checker reads the parsed record, not a fixed column).
            for omit in (('abc',), ('clk',), ('b1f',)):
                with open(tr, 'w') as f:
                    f.write(trace(omit=omit))
                out = io.StringIO()
                rc = gv.trace_state(tr, 0x1000, 0x16C0, lambda s: out.write(s + '\n'))
                self.assertEqual(rc, 1, (omit, out.getvalue()))
                self.assertIn(omit[0], out.getvalue(), (omit, out.getvalue()))
            with open(tr, 'w') as f:
                f.write(trace(drop_return=True))
            out = io.StringIO()
            rc = gv.trace_state(tr, 0x1000, 0x16C0, lambda s: out.write(s + '\n'))
            self.assertEqual(rc, 1)
            self.assertIn('no record at f=16C0', out.getvalue())

    def test_the_trace_state_is_called_with_the_capture_entry_and_return(self):
        import tempfile
        out = io.StringIO()
        with tempfile.TemporaryDirectory() as d:
            tr = os.path.join(d, 'trace.txt')
            with open(tr, 'w') as f:
                f.write('T f=1000 mode=0021 b1f=03 afc=0000 abc=00000002 '
                        'ball=001083F0 clk=001E s0=00 s1=00 b8c1=01 b8c5=01\n'
                        'Vg f=1000 b60=00 b61=00 ball=001083F0 abc=00000002 s0=00 s1=00 '
                        'clk=001E c1=01 c5=01 s0x=00000000 s0h=00000000 s0d=00000000 '
                        's1x=00000000 s1h=00000000 s1d=00000000\n'
                        'V f=1000 e=001083F0 ty=06 fl=20 n=08 x=00000000 h=00000000 '
                        'd=00000000 vx=0000 vy=0000\n'
                        'T f=16C0 mode=0006 b1f=03 afc=0000 abc=00000002 '
                        'ball=00000000 clk=0000 s0=00 s1=00 b8c1=01 b8c5=00\n'
                        'Vg f=16C0 b60=00 b61=00 ball=00000000 abc=00000002 s0=00 s1=00 '
                        'clk=0000 c1=01 c5=00 s0x=00000000 s0h=00000000 s0d=00000000 '
                        's1x=00000000 s1h=00000000 s1d=00000000\n')
            rc = gv.evidence('gp-u30-volleyball', _log(), lambda s: out.write(s + '\n'), tr)
            self.assertEqual(rc, 0, out.getvalue())
            self.assertIn('T/Vg entry f=1000', out.getvalue())
            self.assertIn('return f=16C0', out.getvalue())

    def test_a_missing_appended_field_fails(self):
        # The checker reads the S fields by name; a missing appended field is a
        # FAIL, not a silently skipped claim.
        for name in ('clk', 'abc', 'b8c1'):
            rc, text = _check(_drop(_log(), (name,)))
            self.assertEqual(rc, 1, (name, text))
            self.assertIn('lack the appended mode-0x21 fields', text)
            self.assertIn(name, text)

    def test_a_missing_afc_fails(self):
        # afc is a WIN_EXTRA (the scenario's extra), not a SNAP_FIELD; its
        # absence is a separate failure from the appended-field list.
        rc, text = _check(_drop(_log(), ('afc',)))
        self.assertEqual(rc, 1, text)
        self.assertIn('lack afc', text)

    def test_one_sided_pad_input_fails(self):
        # The trigger claim needs both human sides; a log with only P1's press
        # is not a two-human input path.
        lines = [l for l in _log() if not (l.startswith('I ') and 'press=p2.' in l)]
        rc, text = _check(lines)
        self.assertEqual(rc, 1, text)
        self.assertIn('both sides', text)

    def test_a_score_at_four_fails_at_the_three_point_bound(self):
        rc, text = _check(_log(score=4))
        self.assertEqual(rc, 1)
        self.assertIn('3-point bound', text)
        self.assertIn('0x4C321', text)

    def test_sha_ok_pins_the_poll_log_bytes(self):
        if gv is None or not callable(getattr(gv, 'sha_ok', None)):
            self.fail('sha_ok missing')
        import hashlib
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            cap = os.path.join(d, 'gp-u30-volleyball')
            os.makedirs(cap)
            with open(os.path.join(cap, 'poll.log'), 'wb') as f:
                f.write(b'poll-bytes\n')
            sha = hashlib.sha256(b'poll-bytes\n').hexdigest()
            out = io.StringIO()
            self.assertTrue(gv.sha_ok('gp-u30-volleyball', cap, None,
                                      lambda s: out.write(s + '\n')))
            self.assertTrue(gv.sha_ok('gp-u30-volleyball', cap, sha,
                                      lambda s: out.write(s + '\n')))
            for pinned in ('', sha[:-1] + ('0' if sha[-1] != '0' else '1')):
                out = io.StringIO()
                self.assertFalse(gv.sha_ok('gp-u30-volleyball', cap, pinned,
                                           lambda s: out.write(s + '\n')))
                self.assertIn('re-measure', out.getvalue())

    def test_cli_skips_an_absent_capture_and_pins_the_name_and_sha(self):
        if gv is None or not callable(getattr(gv, 'main', None)):
            self.fail('main missing')
        import hashlib
        import tempfile
        from unittest import mock
        with tempfile.TemporaryDirectory() as d:
            cap = os.path.join(d, 'gp-u30-volleyball')
            os.makedirs(cap)
            with open(os.path.join(cap, 'poll.log'), 'w') as f:
                f.write('\n'.join(_log()) + '\n')
            with mock.patch.object(sys, 'argv',
                                   ['gp_volley.py', 'check', '--capture', cap]):
                self.assertEqual(gv.main(), 0)
            # a missing capture at the scenario's own path skips (exit 0)
            with mock.patch.object(sys, 'argv',
                                   ['gp_volley.py', 'check', '--capture',
                                    os.path.join(d, 'missing', 'gp-u30-volleyball')]):
                self.assertEqual(gv.main(), 0)
            # the wrong basename is not this scenario
            with mock.patch.object(sys, 'argv',
                                   ['gp_volley.py', 'check', '--capture', d]):
                self.assertEqual(gv.main(), 1)
            # --trace names a file; an absent one fails
            with mock.patch.object(sys, 'argv',
                                   ['gp_volley.py', 'check', '--capture', cap,
                                    '--trace', os.path.join(d, 'absent.txt')]):
                self.assertEqual(gv.main(), 1)
            with open(os.path.join(cap, 'poll.log'), 'wb') as f:
                f.write(b'\n'.join(l.encode() for l in _log()) + b'\n')
            with open(os.path.join(cap, 'poll.log'), 'rb') as f:
                sha = hashlib.sha256(f.read()).hexdigest()
            for pinned, want in ((sha, 0), (sha[:-1] + ('0' if sha[-1] != '0' else '1'), 1),
                                 ('', 1)):
                with mock.patch.object(sys, 'argv',
                                       ['gp_volley.py', 'check', '--capture', cap,
                                        '--capture-sha256', pinned]):
                    self.assertEqual(gv.main(), want, pinned)
            # a checker failure exits 1
            with open(os.path.join(cap, 'poll.log'), 'w') as f:
                f.write('\n'.join(_log(score=0)) + '\n')
            with mock.patch.object(sys, 'argv',
                                   ['gp_volley.py', 'check', '--capture', cap]):
                self.assertEqual(gv.main(), 1)


if __name__ == '__main__':
    unittest.main()
