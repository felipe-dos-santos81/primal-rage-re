# tools/tests/test_gp_capture.py
import io, os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_capture as gc
import gp_session as gs

BASE = 0x266000


def _mem():
    m = bytearray(0x400000)
    m[0x41A:0x41C] = (0x1E).to_bytes(2, 'little')      # BDA head/tail/start/end
    m[0x41C:0x41E] = (0x1E).to_bytes(2, 'little')
    m[0x480:0x482] = (0x1E).to_bytes(2, 'little')
    m[0x482:0x484] = (0x3E).to_bytes(2, 'little')
    return m


def _put(m, ds, size, v):
    o = BASE + ds - gs.DATA_BASE_VA
    m[o:o + size] = v.to_bytes(size, 'little')


class TestSnapshot(unittest.TestCase):
    def test_read_snap_uses_the_base(self):
        m = _mem()
        _put(m, 0x0EF6DC, 2, 0x1234)
        _put(m, 0x0EF6D8, 4, 0xDEADBEEF)
        v = gc.read_snap(m, BASE)
        self.assertEqual((v['f'], v['rng']), (0x1234, 0xDEADBEEF))

    def test_spinning_is_the_0x256C6_predicate_with_wrap(self):
        self.assertTrue(gc.spinning({'t50c': 2, 't508': 1}))
        self.assertFalse(gc.spinning({'t50c': 2, 't508': 2}))
        self.assertTrue(gc.spinning({'t50c': 0, 't508': 0xFFFFFFFF}))

    def test_consistent_rejects_a_moving_counter(self):
        v = {'f': 5, 't508': 1, 't50c': 2}
        self.assertTrue(gc.consistent(v, dict(v)))
        self.assertFalse(gc.consistent(v, dict(v, f=6)))
        self.assertFalse(gc.consistent(v, dict(v, t508=2)))

    def test_ring_steps_one_head_per_consumed_word(self):
        # record §G.4: the key loop drains the ring in one iteration
        self.assertEqual(gc.ring_steps(0x22, 0x28, 0x1E, 0x3E), [0x24, 0x26, 0x28])
        self.assertEqual(gc.ring_steps(0x3C, 0x20, 0x1E, 0x3E), [0x1E, 0x20])     # wraps at the end
        self.assertEqual(gc.ring_steps(0x22, 0x22, 0x1E, 0x3E), [])


class TestInjector(unittest.TestCase):
    def test_press_holds_and_queues_once(self):
        m, log = _mem(), io.StringIO()
        ptr = 0xFE20
        m[ptr + gs.KEYTAB_OFF + 0x1F] = 0xFF
        inj = gc.Injector(m, ptr, log)
        inj.press(1, 0x100, 'p1.up', 0x1F, 0x1F73, 2, 0, 10)
        self.assertEqual(m[ptr + gs.KEYTAB_OFF + 0x1F], 0x7F)
        self.assertEqual(m[0x400 + 0x1E:0x400 + 0x20], (0x1F73).to_bytes(2, 'little'))
        inj.release_due(0x100, 11)
        self.assertEqual(m[ptr + gs.KEYTAB_OFF + 0x1F], 0x7F)       # held for iterations 0x101, 0x102
        inj.release_due(0x101, 12)
        self.assertEqual(m[ptr + gs.KEYTAB_OFF + 0x1F], 0x7F)       # record §G.7: still down at the spin of 0x101
        inj.release_due(0x102, 13)
        self.assertEqual(m[ptr + gs.KEYTAB_OFF + 0x1F], 0xFF)       # released at the spin of 0x102 (F - 1 + n)
        recs = [gs.parse(l) for l in log.getvalue().splitlines()]
        self.assertEqual([r.get('press') or r.get('release') for r in recs], ['p1.up', 'p1.up'])
        self.assertEqual(recs[0]['bios'], 0x1F73)

    def test_pad_bios_off_queues_nothing(self):
        m, log = _mem(), io.StringIO()
        inj = gc.Injector(m, 0xFE20, log, pad_bios=False)
        inj.press(1, 0x100, 'p1.up', 0x1F, 0x1F73, 2, 0, 10)
        self.assertEqual(m[0x41C:0x41E], (0x1E).to_bytes(2, 'little'))
        inj.press(2, 0x100, 'enter', 0x1C, 0x1C0D, 3, 0, 10)          # a BIOS key still queues
        self.assertEqual(m[0x41C:0x41E], (0x20).to_bytes(2, 'little'))


class TestAccept(unittest.TestCase):
    def test_accept_needs_the_spin_across_both_reads(self):
        # Review Focus 1: v spinning, v2 the logged read, v3 a re-read after it
        v = {'f': 5, 't508': 1, 't50c': 2}
        self.assertTrue(gc.accept(v, dict(v), dict(v)))
        self.assertFalse(gc.accept(dict(v, t50c=1), dict(v), dict(v)))     # not spinning
        self.assertFalse(gc.accept(v, dict(v, f=6), dict(v, f=6)))         # an iteration between v and v2
        self.assertFalse(gc.accept(v, dict(v), dict(v, t508=2)))           # a tick during v2's read


class TestOutput(unittest.TestCase):
    def test_guard_requires_gp_prefix(self):
        ok = os.path.join(ROOT, 'data', 'k11-captures', 'gp-x')
        self.assertEqual(gc.guard_gp(ok), os.path.realpath(ok))
        for bad in (os.path.join(ROOT, 'data', 'k11-captures', 'walk'),
                    os.path.join(ROOT, 'data', 'k11-captures', 'walk', 'gp-x'),     # review 1: nested
                    os.path.join(ROOT, 'data', 'game', 'C', 'gp-x'), '/tmp/gp-x'):
            with self.assertRaises(SystemExit):
                gc.guard_gp(bad)

    def test_frames_stream_to_gzip(self):
        import shutil, tempfile
        d = tempfile.mkdtemp(prefix='gpcap-test-')
        self.addCleanup(shutil.rmtree, d, True)
        frames = [bytes([i]) * gc.FRAME_BYTES for i in range(5)]
        yielded = []

        def gen(paths):
            for fr in frames:
                yielded.append(fr[0])
                yield fr
        orig = gc.sc.read_avi_frames
        gc.sc.read_avi_frames = gen
        try:
            n = gc.write_frames(d, ['x.avi'], [1, 3])
        finally:
            gc.sc.read_avi_frames = orig
        self.assertEqual(n, 2)
        import gzip
        for k, raw in enumerate((1, 3)):
            with gzip.open(os.path.join(d, 'frame_%05d.raw.gz' % k)) as f:
                self.assertEqual(f.read(), frames[raw])
        self.assertEqual(sorted(os.listdir(d)), ['frame_00000.raw.gz', 'frame_00001.raw.gz', 'window.txt'])
        with open(os.path.join(d, 'window.txt')) as f:
            self.assertEqual(f.read(), '00000 1\n00001 3\n')
        self.assertEqual(yielded, [0, 1, 2, 3])        # review 1: decoding stops after the last wanted frame

    def test_publish_keeps_a_good_capture_from_a_failing_rerun(self):
        import shutil, tempfile
        d = tempfile.mkdtemp(prefix='gpcap-test-')
        self.addCleanup(shutil.rmtree, d, True)
        out = os.path.join(d, 'gp-x')

        def staged(tag):
            st = gc.stage_dir(out)
            os.makedirs(st)
            with open(os.path.join(st, 'poll.log'), 'w') as f:
                f.write(tag)
            return st
        self.assertEqual(gc.publish(staged('good'), out, True), out)
        self.assertEqual(gc.publish(staged('bad'), out, False), out + '.failed')
        with open(os.path.join(out, 'poll.log')) as f:
            self.assertEqual(f.read(), 'good')
        with open(os.path.join(out + '.failed', 'poll.log')) as f:
            self.assertEqual(f.read(), 'bad')
        self.assertEqual(sorted(os.listdir(d)), ['gp-x', 'gp-x.failed'])
        self.assertEqual(gc.publish(staged('good2'), out, True), out)
        with open(os.path.join(out, 'poll.log')) as f:
            self.assertEqual(f.read(), 'good2')
        # re-review: a good publish removes a stale <out>.failed (U3/U4 glob gp-*)
        self.assertEqual(sorted(os.listdir(d)), ['gp-x'])


def _s(f, raw=0, kb=None, mode=0x27):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, raw=raw, mode=mode, t508=1, t50c=2)
    return gs.format_s(0, vals, gs.raw_to_kb(raw) if kb is None else kb, 0x1E, 0x1E)


class TestChecks(unittest.TestCase):
    ENTER = 'I ms=1 f=0010 step=0 press=enter scan=1C lin=00010090 old=FF bios=1C0D ring=1 late=0'

    def _checks(self, lines, n=3, want=3):
        s = gs.Schedule(())
        return dict((name.split(' (')[0], ok) for name, ok in gc.run_checks('_t', lines, s, n, want))

    def test_mode_0x27_must_follow_the_enter(self):
        good = ['B ms=0 base=00266000 ptr=0000FE20', _s(0x10, mode=3), self.ENTER, _s(0x11, mode=0x27)]
        self.assertTrue(self._checks(good)['mode 0x27 after the Enter'])
        bad = ['B ms=0 base=00266000 ptr=0000FE20', _s(0x10, mode=0x27), self.ENTER, _s(0x11, mode=0x27)]
        self.assertFalse(self._checks(bad)['mode 0x27 after the Enter'])
        self.assertFalse(self._checks(good[:3])['mode 0x27 after the Enter'])

    def test_kb_equals_raw_and_frames_written_are_checks(self):
        L = ['B ms=0 base=00266000 ptr=0000FE20', self.ENTER, _s(0x11, raw=0x80000000)]
        self.assertTrue(self._checks(L)['snapshots kb == raw'])
        self.assertFalse(self._checks(L + [_s(0x12, raw=0x80000000, kb=0)])['snapshots kb == raw'])
        self.assertTrue(self._checks(L, 3, 3)['frames written 3/3'])
        self.assertFalse(self._checks(L, 2, 3)['frames written 2/3'])


def _st(f, kb=0, tail=0x1E):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, mode=6, t508=1, t50c=2)
    return gs.format_s(0, vals, kb, 0x1E, tail)


def _press(f, name, scan, word, lin, ring=1):
    return ('I ms=1 f=%04X step=1 press=%s scan=%02X lin=%08X old=FF bios=%04X ring=%d late=0'
            % (f, name, scan, lin, word, ring))


def _release(f, name, lin):
    return 'I ms=1 f=%04X step=1 release=%s lin=%08X' % (f, name, lin)


class TestUnscriptedInput(unittest.TestCase):
    """Task capture-hygiene: every key word in the ring and every kb bit must
    come from an injection (U6b task-7 report §6.5)."""
    LIN = 0x10000 + gs.KEYTAB_OFF + 0x17

    def clean(self):
        # p1.b1 pressed at the spin of 0x10 (after S(0x10)), released at the spin
        # of 0x12: its word shows in S(0x11)'s tail, its bit in S(0x11)..S(0x12).
        return ['B ms=0 base=00266000 ptr=00010000', _st(0x10),
                _press(0x10, 'p1.b1', 0x17, 0x1769, self.LIN),
                _st(0x11, 0x0200, 0x20), _st(0x12, 0x0200, 0x20),
                _release(0x12, 'p1.b1', self.LIN),
                _st(0x13, 0, 0x20), _st(0x14, 0, 0x20)]

    def test_a_clean_log_passes(self):
        self.assertEqual(gc.input_check(self.clean()), ('no unscripted input', True))

    def test_an_extra_bios_word_fails(self):
        L = self.clean()
        L[-1] = _st(0x14, 0, 0x22)          # the tail moves with no I record
        u = gc.unscripted_input(L)
        self.assertEqual(u['words'], [0x14])
        self.assertEqual(gc.input_check(L),
                         ('unscripted input: 1 key words, 0 pad presses, 0 pad releases, first at f=14', False))

    def test_a_ring_full_press_queues_nothing(self):
        L = self.clean()
        L[2] = _press(0x10, 'p1.b1', 0x17, 0x1769, self.LIN, ring=0)
        self.assertEqual(gc.unscripted_input(L)['words'], [0x11])

    def test_the_ring_wraps(self):
        L = ['B ms=0 base=00266000 ptr=00010000', _st(0x10, 0, 0x3C),
             _press(0x10, 'enter', 0x1C, 0x1C0D, 0x10090), _st(0x11, 0, 0x1E), _st(0x12, 0, 0x1E)]
        self.assertEqual(gc.input_check(L), ('no unscripted input', True))
        L[-1] = _st(0x12, 0, 0x22)
        self.assertEqual(gc.unscripted_input(L)['words'], [0x12, 0x12])

    def test_an_extra_pad_press_fails(self):
        L = self.clean()
        L[-1] = _st(0x14, 0x0100, 0x20)     # p1.b0 (U) with no injection
        self.assertEqual(gc.unscripted_input(L)['presses'], [(0x14, 0x0100)])
        self.assertEqual(gc.input_check(L),
                         ('unscripted input: 0 key words, 1 pad presses, 0 pad releases, first at f=14', False))

    def test_the_window_is_exact(self):
        L = self.clean()
        L[1] = _st(0x10, 0x0200)            # set before the press's own spin
        self.assertEqual(gc.unscripted_input(L)['presses'], [(0x10, 0x0200)])
        L = self.clean()
        L[6] = _st(0x13, 0x0200, 0x20)      # still set after the release's spin
        self.assertEqual(gc.unscripted_input(L)['presses'], [(0x13, 0x0200)])
        L = self.clean()
        L[4] = _st(0x12, 0, 0x20)           # a key-up inside the hold
        self.assertEqual(gc.unscripted_input(L)['releases'], [(0x12, 0x0200)])

    def test_a_held_run_counts_once(self):
        L = self.clean() + [_st(0x15, 0x0100, 0x20), _st(0x16, 0x0100, 0x20), _st(0x17, 0, 0x20),
                            _st(0x18, 0x0100, 0x20)]
        self.assertEqual(gc.unscripted_input(L)['presses'], [(0x15, 0x0100), (0x18, 0x0100)])

    def test_a_bios_key_on_a_pad_scan_explains_its_bit(self):
        # U11's 'n' writes scan 0x31, p1.b2's key: its bit 0x0400 is scripted.
        lin = 0x10000 + gs.KEYTAB_OFF + 0x31
        L = ['B ms=0 base=00266000 ptr=00010000', _st(0x10), _press(0x10, 'n', 0x31, 0x316E, lin),
             _st(0x11, 0x0400, 0x20), _release(0x11, 'n', lin), _st(0x12, 0, 0x20)]
        self.assertEqual(gc.input_check(L), ('no unscripted input', True))

    def test_run_checks_carries_it(self):
        L = self.clean()
        L[-1] = _st(0x14, 0x0100, 0x20)
        checks = dict(gc.run_checks('_t', L, gs.Schedule(()), 1, 1))
        self.assertIn('unscripted input: 0 key words, 1 pad presses, 0 pad releases, first at f=14', checks)
        self.assertFalse(checks['unscripted input: 0 key words, 1 pad presses, 0 pad releases, first at f=14'])
        self.assertTrue(dict(gc.run_checks('_t', self.clean(), gs.Schedule(()), 1, 1))['no unscripted input'])

    # The four captures this rule was measured on, read-only, by their poll.log
    # sha256 (a re-capture under the same name is another file: skipped).
    CAPTURES = {
        'gp-pads': ('9f8860340b77eaefe12a3c480638000609882b752791d6edf33d20067609fa0c', ('no unscripted input', True)),
        'gp-idle-loss': ('773e264731ea83a23623c6ec6cc5547165628d8adcf96f5a7c99c1c2b88c8447', ('no unscripted input', True)),
        'gp-u5-charsel': ('138fb537cd8c364bab0ccc68fc8fdd9079de19f3b0fc789d427f6d9652a18d7e', ('no unscripted input', True)),
        'gp-u6-moves': ('6bed58feec27f2b74a9dec0dafe6c34bd613715293f9a410c815b3f18ac368bd',
                        ('unscripted input: 63 key words, 51 pad presses, 0 pad releases, first at f=9CB', False)),
    }

    def test_the_measured_captures(self):
        import hashlib
        for name, (sha, want) in self.CAPTURES.items():
            with self.subTest(capture=name):
                path = os.path.join(ROOT, 'data', 'k11-captures', name, 'poll.log')
                if not os.path.exists(path):
                    self.skipTest('%s absent' % path)
                with open(path, 'rb') as f:
                    data = f.read()
                if hashlib.sha256(data).hexdigest() != sha:
                    self.skipTest('%s is not the measured capture' % path)
                self.assertEqual(gc.input_check(data.decode().splitlines()), want)


class TestCheckInputCli(unittest.TestCase):
    def run_cli(self, d):
        import contextlib
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = gc.check_input_main([d])
        return rc, out.getvalue()

    def test_missing_poll_log_and_non_gp_log_exit_2(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            rc, out = self.run_cli(d)
            self.assertEqual(rc, 2)
            self.assertIn('no poll.log in %s' % d, out)
            with open(os.path.join(d, 'poll.log'), 'w') as f:      # a K11 log: P/K records, no S
                f.write('B ms=0 base=00266000\nK ms=1 tick=00000001 f=0001 key=1C0D\n')
            rc, out = self.run_cli(d)
            self.assertEqual(rc, 2)
            self.assertIn('no S records: not a gp poll.log', out)

    def test_ok_and_fail_exit_codes(self):
        import tempfile
        t = TestUnscriptedInput()
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, 'poll.log'), 'w') as f:
                f.write('\n'.join(t.clean()) + '\n')
            self.assertEqual(self.run_cli(d), (0, 'check=ok no unscripted input\n'))
            bad = t.clean()
            bad[-1] = _st(0x14, 0x0100, 0x20)
            with open(os.path.join(d, 'poll.log'), 'w') as f:
                f.write('\n'.join(bad) + '\n')
            rc, out = self.run_cli(d)
            self.assertEqual(rc, 1)
            self.assertIn('check=FAIL unscripted input: 0 key words, 1 pad presses', out)


class TestStopAtEnd(unittest.TestCase):
    class Proc:
        def __init__(self, exits_on_term=True):
            self.sent, self.killed, self.rc, self.exits = [], False, None, exits_on_term

        def send_signal(self, sig):
            self.sent.append(sig)
            if self.exits:
                self.rc = 0

        def wait(self, timeout=None):
            if self.rc is None:
                raise gc.subprocess.TimeoutExpired('dosbox-x', timeout)
            return self.rc

        def kill(self):
            self.killed, self.rc = True, -9

    def poller(self, tail):
        p = gc.Poller('/nonexistent', '/nonexistent', (('after_mode', 6, 10, ('end',)),), None)
        p.sched.on_mode(0x100, 6)
        p.sched.due(0x109)                  # the end frame F = 0x10A
        p.stop_tail, p.proc = tail, self.Proc()
        return p

    def test_sigterm_comes_tail_frames_after_the_end(self):
        p = self.poller(gc.STOP_TAIL)
        self.assertFalse(p.stop_if_due(0x10A + gc.STOP_TAIL - 1))
        self.assertEqual(p.proc.sent, [])
        self.assertTrue(p.stop_if_due(0x10A + gc.STOP_TAIL))
        self.assertEqual((p.proc.sent, p.signal_f), ([gc.signal.SIGTERM], 0x10A + gc.STOP_TAIL))
        self.assertFalse(p.stop_if_due(0x10A + gc.STOP_TAIL + 1))      # once
        self.assertEqual(len(p.proc.sent), 1)

    def test_no_tail_runs_to_the_time_limit(self):
        p = self.poller(None)
        self.assertFalse(p.stop_if_due(0x2000))
        self.assertEqual(p.proc.sent, [])
        p = self.poller(gc.STOP_TAIL)
        p.sched.end_frame = None            # the end never came
        self.assertFalse(p.stop_if_due(0x2000))

    def test_wait_kills_a_dosbox_that_ignores_the_sigterm(self):
        p = self.poller(0)
        p.proc = self.Proc(exits_on_term=False)
        p.stop_if_due(0x10A)
        p.signal_t -= 10
        self.assertEqual(gc.wait_dosbox(p.proc, p, grace=5), -9)
        self.assertTrue(p.proc.killed)
        p = self.poller(0)
        p.stop_if_due(0x10A)
        self.assertEqual(gc.wait_dosbox(p.proc, p, grace=5), 0)
        self.assertFalse(p.proc.killed)

    def test_quit_warning_only_when_stopping(self):
        stop, base = gc.dosbox_cmd('/r', '/g', '/i', 10, True), gc.dosbox_cmd('/r', '/g', '/i', 10)
        self.assertNotIn('dosbox quit warning=false', base)
        i = stop.index('dosbox quit warning=false') - 1
        self.assertEqual(stop[:i] + stop[i + 2:], base)

    def test_opt_in_scenarios(self):
        # gp-idle-loss records its post-end tail on purpose; gp-keys-fight's plan
        # counts on its post-restart movie tail: neither stops early.
        self.assertIn('gp-u6-moves', gs.STOP_AT_END)
        self.assertIn('gp-u6-moves-b', gs.STOP_AT_END)      # U6b's re-capture (user decision)
        self.assertIn('gp-twop', gs.STOP_AT_END)            # U7: nothing past its end is compared (record §T.5)
        for name in ('gp-u8-right-arcade', 'gp-u8-left-training', 'gp-u8-right-training',
                     'gp-u8-tug-of-war', 'gp-u8-endurance', 'gp-u8-handicap', 'gp-u8-attract-start'):
            self.assertIn(name, gs.STOP_AT_END)             # U8 rows and the attract start: N and F sit at the replay's end (record §U8.12, §U8.14)
        for name in ('gp-pads', 'gp-idle-loss', 'gp-idle-loss-run2', 'gp-u5-charsel', 'gp-keys-fight'):
            self.assertNotIn(name, gs.STOP_AT_END)
        for name in gs.STOP_AT_END & set(gs.SCENARIOS):    # a stop needs an end frame
            last = gs.SCENARIOS[name]['steps'][-1]
            self.assertTrue(last[-1] == ('end',) or last[0] == 'until_mode', name)


class TestFire(unittest.TestCase):
    def test_late_is_judged_per_step(self):
        # review 1: two steps firing in one due() call; step 1 (F = 0x10A, due at
        # the spin of 0x109) is 3 frames late, step 2 (F = 0x10D) is on time.
        s = gs.Schedule((('after_mode', 0x27, 10, ('key', 'enter')),
                         ('after', 3, ('key', 'esc'))))
        s.on_mode(0x100, 0x27)
        self.assertEqual(gc.fire(s, 0x10C), [(0, ('key', 'enter'), 1), (1, ('key', 'esc'), 0)])
        s = gs.Schedule((('after_mode', 0x27, 10, ('key', 'enter')),))
        s.on_mode(0x100, 0x27)
        self.assertEqual(gc.fire(s, 0x109), [(0, ('key', 'enter'), 0)])


class TestPoke(unittest.TestCase):
    """Plan gameplay-u9-u10 (record 2026-10-02-gameplay-u9-u10-derivations.md §W.8)."""

    def _ref(self, m):
        _put(m, 0x0EF6DC, 2, 0x48F)
        _put(m, 0x101508, 4, 7)
        _put(m, 0x10150C, 4, 8)
        return gc.read_snap(m, BASE)

    def test_apply_poke_writes_at_the_data_base_and_logs(self):
        m, log = _mem(), io.StringIO()
        ref = self._ref(m)
        _put(m, 0x10789E, 1, 0x11)
        writes = ((0x108106, b'\x80' * 7), (0x10789E, b'\x78'))
        race = gc.apply_poke(m, BASE, log, 5, 0x48F, writes, 0, 12, ref, lambda: gc.read_snap(m, BASE))
        self.assertEqual(race, 0)
        o = BASE + 0x10789E - gs.DATA_BASE_VA
        self.assertEqual(m[o], 0x78)
        o = BASE + 0x108106 - gs.DATA_BASE_VA
        self.assertEqual(bytes(m[o:o + 8]), b'\x80' * 7 + b'\x00')
        self.assertEqual(log.getvalue().splitlines(), [
            gs.format_w(12, 0x48F, 5, 0x108106, b'\x00' * 7, b'\x80' * 7, 0, 0),
            gs.format_w(12, 0x48F, 5, 0x10789E, b'\x11', b'\x78', 0, 0)])

    def test_a_tick_between_the_snapshot_and_the_write_is_a_race(self):
        m, log = _mem(), io.StringIO()
        ref = self._ref(m)
        moved = lambda: dict(gc.read_snap(m, BASE), t508=8)
        self.assertEqual(gc.apply_poke(m, BASE, log, 5, 0x48F, ((0x10789E, b'\x78'),), 1, 12, ref, moved), 1)
        self.assertIn('late=1 race=1', log.getvalue())

    def test_the_poke_check(self):
        s = gs.Schedule((('after', 1, ('poke', ((0x108106, b'\x80' * 7), (0x10789E, b'\x78')))),))
        s.prev_frame = 0x100
        s.due(0x100)
        ok = gs.format_w(0, 0x100, 0, 0x108106, b'\x00' * 7, b'\x80' * 7, 0, 0)
        ok2 = gs.format_w(0, 0x100, 0, 0x10789E, b'\x00', b'\x78', 0, 0)
        self.assertEqual(gc.poke_check([ok, ok2], s), ('pokes written 2/2, 0 raced', True))
        self.assertEqual(gc.poke_check([ok], s), ('pokes written 1/2, 0 raced', False))
        raced = ok2.replace('race=0', 'race=1')
        self.assertEqual(gc.poke_check([ok, raced], s), ('pokes written 2/2, 1 raced', False))
        self.assertEqual(gc.poke_check([], gs.Schedule(())), ('pokes written 0/0, 0 raced', True))


if __name__ == '__main__':
    unittest.main()
