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

    def test_the_device_words_are_read_through_the_block_pointer(self):
        # record §H6: dev1/dev2 are [DS_00101514] + 0x2D4/0x2D6, so read_snap needs the
        # block pointer; without it there is no address to read and the value is 0
        m = _mem()
        ptr = 0xFE20
        m[ptr + 0x2D4:ptr + 0x2D6] = (2).to_bytes(2, 'little')
        m[ptr + 0x2D6:ptr + 0x2D8] = (6).to_bytes(2, 'little')
        # a decoy at the offset a fixed-DS read would use, so that path is distinguishable
        _put(m, 0x2D4, 2, 0xEEEE)
        v = gc.read_snap(m, BASE, ptr=ptr)
        self.assertEqual((v['dev1'], v['dev2']), (2, 6))
        # without the pointer (the pre-block and consistency reads): 0, never the decoy
        self.assertEqual(gc.read_snap(m, BASE)['dev1'], 0)
        self.assertEqual(gc.read_snap(m, BASE)['dev2'], 0)
        # and the S record written with the pointer carries them
        rec = gs.parse(gs.format_s(0, dict(v, f=7), 0, 0x1E, 0x1E))
        self.assertEqual((rec['dev1'], rec['dev2']), (2, 6))

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

    def test_the_wall_deadline_releases_a_key_with_no_frame_change(self):
        # record §H6.9: inside the service menu the frame counter freezes, so a
        # hold is also wall-deadlined (`hold` frames at the game's rate) and the
        # poller's per-pass release_due ends it; the I record carries the frame it
        # still read (unchanged) and the wall release's ms
        m, log = _mem(), io.StringIO()
        ptr = 0xFE20
        m[ptr + gs.KEYTAB_OFF + 0x50] = 0xFF
        inj = gc.Injector(m, ptr, log, wall=True)
        inj.press(2, 0x25B, 'down', 0x50, 0x5000, 5, 0, 1000)
        self.assertEqual(m[ptr + gs.KEYTAB_OFF + 0x50], 0x7F)
        deadline = 1000 + 5 * gs.FRAME_MS
        inj.release_due(0x25B, int(deadline) - 1)      # the frame never moves
        self.assertEqual(m[ptr + gs.KEYTAB_OFF + 0x50], 0x7F)       # not yet
        inj.release_due(0x25B, int(deadline) + 1)
        self.assertEqual(m[ptr + gs.KEYTAB_OFF + 0x50], 0xFF)       # the wall deadline
        recs = [gs.parse(l) for l in log.getvalue().splitlines()]
        self.assertEqual((recs[0]['f'], recs[1]['f']), (0x25B, 0x25B))
        self.assertEqual(recs[1]['release'], 'down')
        # a frame-keyed scenario keeps the frame rule alone (no wall release)
        m2 = _mem()
        m2[ptr + gs.KEYTAB_OFF + 0x50] = 0xFF
        inj2 = gc.Injector(m2, ptr, io.StringIO())
        inj2.press(2, 0x25B, 'down', 0x50, 0x5000, 5, 0, 1000)
        inj2.release_due(0x25B, int(deadline) + 1)
        self.assertEqual(m2[ptr + gs.KEYTAB_OFF + 0x50], 0x7F)      # still held

    def test_pad_bios_off_queues_nothing(self):
        m, log = _mem(), io.StringIO()
        inj = gc.Injector(m, 0xFE20, log, pad_bios=False)
        inj.press(1, 0x100, 'p1.up', 0x1F, 0x1F73, 2, 0, 10)
        self.assertEqual(m[0x41C:0x41E], (0x1E).to_bytes(2, 'little'))
        inj.press(2, 0x100, 'enter', 0x1C, 0x1C0D, 3, 0, 10)          # a BIOS key still queues
        self.assertEqual(m[0x41C:0x41E], (0x20).to_bytes(2, 'little'))


class TestPinGuard(unittest.TestCase):
    """Task 9 (two-human KO; record 2026-10-26-fought-match-truth-derivations
    §F4.5/§F4.7): the pin guard queues a BIOS word only while the accepted S
    records are contiguous and the backlog is under the cap, so a missed S(c-1)
    fails the run at once instead of a full time-limit run."""

    def guard(self, frames):
        g = gc.PinGuard()
        for f in frames:
            g.snap(f)
        return g

    def test_a_missed_S_predecessor_fails_fast(self):
        # §F4.7: a word queued at the first frame after a gap (S(0x592) missed)
        # is declined and marks the run gapped.
        g = self.guard([0x57B, 0x593])
        self.assertFalse(g.queue(0x593))
        self.assertTrue(g.gapped)
        self.assertEqual(g.gap_f, 0x593)
        # §F4.5: a word consumed at detection frame c with S(c-1) missed (S(0x7C6))
        # is likewise gapped.
        g2 = self.guard([0x7C4])
        self.assertFalse(g2.pin_ok(0x7C7))
        self.assertTrue(g2.gapped)
        self.assertEqual(g2.gap_f, 0x7C7)

    def test_the_accepted_path_queues(self):
        g = self.guard([0x10, 0x11])
        self.assertTrue(g.queue(0x11))
        self.assertEqual(g.backlog, 1)
        self.assertFalse(g.gapped)
        g.consume()
        self.assertEqual(g.backlog, 0)

    def test_the_backlog_cap_is_a_counted_skip(self):
        g = self.guard([0x10, 0x11])
        for _ in range(g.cap):
            self.assertTrue(g.queue(0x11))
        self.assertEqual(g.backlog, g.cap)
        self.assertFalse(g.queue(0x11))         # over the cap: no word queued
        self.assertEqual(g.skips, 1)
        self.assertEqual(g.backlog, g.cap)      # the declined word did not queue

    def test_the_skip_counter_is_reported(self):
        g = self.guard([0x10, 0x11])
        for _ in range(g.cap + 1):
            g.queue(0x11)
        checks = dict(gc.run_checks('_t', [], gs.Schedule(()), 0, 0, g))
        label = 'capture pins: 1 BIOS words skipped (backlog over the %d-entry cap)' % gc.PinGuard.BACKLOG_CAP
        self.assertIn(label, checks)
        self.assertFalse(checks[label])
        # without a guard (a wall scenario) the check is present and passes
        checks0 = dict(gc.run_checks('_t', [], gs.Schedule(()), 0, 0))
        self.assertTrue(checks0['capture pins: 0 BIOS words skipped (backlog over the %d-entry cap)'
                               % gc.PinGuard.BACKLOG_CAP])

    def test_press_consults_the_guard(self):
        m, log = _mem(), io.StringIO()
        ptr = 0xFE20
        g = gc.PinGuard()
        g.snap(0x100)
        g.snap(0x101)
        inj = gc.Injector(m, ptr, log, guard=g)
        inj.press(1, 0x101, 'p1.up', 0x1F, 0x1F73, 2, 0, 10)
        self.assertEqual(m[0x41C:0x41E], (0x20).to_bytes(2, 'little'))   # queued: tail 0x1E -> 0x20
        m2, log2 = _mem(), io.StringIO()
        g2 = gc.PinGuard()
        g2.snap(0x100)
        g2.snap(0x102)                              # S(0x101) missed
        inj2 = gc.Injector(m2, ptr, log2, guard=g2)
        inj2.press(1, 0x102, 'p1.up', 0x1F, 0x1F73, 2, 0, 10)
        self.assertTrue(g2.gapped)
        self.assertEqual(m2[0x41C:0x41E], (0x1E).to_bytes(2, 'little'))  # declined: no word queued


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
        # fix round: a second failed run is kept under a numbered suffix, never
        # replacing the first (the first two-human KO's .failed was lost to the
        # old rename-and-rmtree path, so its poll.log is only in the record).
        self.assertEqual(gc.publish(staged('bad2'), out, False), out + '.failed.1')
        with open(os.path.join(out, 'poll.log')) as f:
            self.assertEqual(f.read(), 'good')
        with open(os.path.join(out + '.failed', 'poll.log')) as f:
            self.assertEqual(f.read(), 'bad')
        with open(os.path.join(out + '.failed.1', 'poll.log')) as f:
            self.assertEqual(f.read(), 'bad2')
        self.assertEqual(gc.publish(staged('good2'), out, True), out)
        with open(os.path.join(out, 'poll.log')) as f:
            self.assertEqual(f.read(), 'good2')
        # a good publish replaces <out> but leaves every .failed artifact alone
        self.assertEqual(sorted(os.listdir(d)),
                         ['gp-x', 'gp-x.failed', 'gp-x.failed.1'])


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
        self.assertFalse(p.stop_if_due(0x10A + gc.STOP_TAIL - 1, 0.0))
        self.assertEqual(p.proc.sent, [])
        self.assertTrue(p.stop_if_due(0x10A + gc.STOP_TAIL, 0.0))
        self.assertEqual((p.proc.sent, p.signal_f), ([gc.signal.SIGTERM], 0x10A + gc.STOP_TAIL))
        self.assertFalse(p.stop_if_due(0x10A + gc.STOP_TAIL + 1, 0.0))      # once
        self.assertEqual(len(p.proc.sent), 1)

    def test_stop_on_gap_sigterms_once(self):
        # Task 9: a missed S(c-1) marks the guard gapped, and the poller SIGTERMs
        # once and records the gap frame instead of running on to the time limit.
        p = self.poller(gc.STOP_TAIL)
        p.guard.gapped, p.guard.gap_f = True, 0x7C7
        self.assertTrue(p.stop_on_gap())
        self.assertEqual((p.proc.sent, p.gap_f, p.reason),
                         ([gc.signal.SIGTERM], 0x7C7, 'gap'))
        self.assertFalse(p.stop_on_gap())                                   # once
        self.assertEqual(len(p.proc.sent), 1)

    def test_no_gap_means_no_sigterm(self):
        p = self.poller(gc.STOP_TAIL)
        self.assertFalse(p.stop_on_gap())                                   # not gapped
        self.assertEqual(p.proc.sent, [])


    def test_no_tail_runs_to_the_time_limit(self):
        p = self.poller(None)
        self.assertFalse(p.stop_if_due(0x2000, 0.0))
        self.assertEqual(p.proc.sent, [])
        p = self.poller(gc.STOP_TAIL)
        p.sched.end_frame = None            # the end never came
        self.assertFalse(p.stop_if_due(0x2000, 0.0))

    def test_wait_kills_a_dosbox_that_ignores_the_sigterm(self):
        p = self.poller(0)
        p.proc = self.Proc(exits_on_term=False)
        p.stop_if_due(0x10A, 0.0)
        p.signal_t -= 10
        self.assertEqual(gc.wait_dosbox(p.proc, p, grace=5), -9)
        self.assertTrue(p.proc.killed)
        p = self.poller(0)
        p.stop_if_due(0x10A, 0.0)
        self.assertEqual(gc.wait_dosbox(p.proc, p, grace=5), 0)
        self.assertFalse(p.proc.killed)

    def test_a_wall_end_stops_tail_frames_later(self):
        # record §H6.9: a wall-scenario end names a moment (end_wall), not a frame
        p = gc.Poller('/nonexistent', '/nonexistent', (('after_wall', 1.0, ('end',)),), None)
        p.sched.last_wall = 1.0
        p.sched.due_wall(2.0)                          # end_wall = 2.0
        self.assertIsNotNone(p.sched.end_wall)
        p.stop_tail, p.proc = gc.STOP_TAIL, self.Proc()
        tail_s = gc.STOP_TAIL * gs.FRAME_MS / 1000.0
        self.assertFalse(p.stop_if_due(0x10, 2.0 + tail_s - 0.01))
        self.assertEqual(p.proc.sent, [])
        self.assertTrue(p.stop_if_due(0x10, 2.0 + tail_s))
        self.assertEqual(p.proc.sent, [gc.signal.SIGTERM])

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
        self.assertIn('gp-u17-device2', gs.STOP_AT_END)      # U17: nothing past the clear is compared
        for name in ('gp-pads', 'gp-idle-loss', 'gp-idle-loss-run2', 'gp-u5-charsel', 'gp-keys-fight'):
            self.assertNotIn(name, gs.STOP_AT_END)
        for name in gs.STOP_AT_END & set(gs.SCENARIOS):    # a stop needs an end frame
            last = gs.SCENARIOS[name]['steps'][-1]
            self.assertTrue(last[-1] == ('end',) or last[0] == 'until_mode'
                            or gs.SCENARIOS[name].get('stop_mode') is not None, name)

    def test_the_poller_passes_the_stop_modes_to_the_schedule(self):
        p = gc.Poller('/tmp/mem', '/tmp/log', (), None,
                      stop_mode=(0x08, 0x09), stop_delay=240, skip_on_mode=(0x08, 0x09))
        self.assertEqual((p.sched.stop_modes, p.sched.stop_delay, p.sched.skip_modes),
                         ((0x08, 0x09), 240, (0x08, 0x09)))

    def test_steps_ok_accepts_a_skipped_round(self):
        s = gs.Schedule((('after', 1, ('pad', ('p1.b0',), 4)),), skip_on_mode=0x08)
        s.on_mode(10, 0x08)
        self.assertTrue(s.steps_ok)                 # the one step is deliberately skipped
        # the check names the skip's mode: a time-up round (0x07) is a failed run,
        # so a bare "(1 skipped): ok" must not read as a completed script
        label, ok = next(c for c in gc.run_checks('_t', [], s, 0, 0)
                         if c[0].startswith('steps fired'))
        self.assertEqual(label, 'steps fired 0/1 (1 skipped); last skip on mode 0x08')
        self.assertTrue(ok)

    def test_the_step_check_names_the_mode_a_stop_ended_the_capture_on(self):
        steps = (('boot', 0.0, ('key', 'enter')),
                 ('after_mode', 0x27, 30, ('pad', ('p1.start',), 4)))
        s = gs.Schedule(steps, stop_mode=0x27, stop_delay=10)
        s.on_mode(0x10, 0x27)                       # the smoke's own shape: 1 of 2 steps
        label, ok = next(c for c in gc.run_checks('_t', [], s, 0, 0)
                         if c[0].startswith('steps fired'))
        self.assertEqual(label, 'steps fired 0/2 (0 skipped); stopped on mode 0x27')
        self.assertTrue(ok)                         # the stop is the script's end


class TestMemsize(unittest.TestCase):
    """The poked final runs DOSBox-X with memsize=64, a harness value of those scenarios
    only (record §W.13): gp-u13-ending2, gp-u18-talon-ending, gp-u19-vertigo-ending,
    gp-u20-chaos-ending, gp-u21-armadon-ending and gp-u22-diablo-ending share
    gp-u10-ending's final and its seven poked KOs, and every other scenario's argv is
    the one its capture was taken with. gp-u25-endurance-end (state coverage Task 5)
    and gp-u26-side1-ending (Task 6) also set memsize=64: both reproduce the poked-KO /
    ending path that ran out of memory at 16 MB in the original (the §W.13 OOM), so
    they carry the same harness value."""
    MEM = ['-set', 'dosbox memsize=64']
    POKED_FINAL = ('gp-u10-ending', 'gp-u13-ending2', 'gp-u18-talon-ending',
                   'gp-u19-vertigo-ending', 'gp-u20-chaos-ending',
                   'gp-u21-armadon-ending', 'gp-u22-diablo-ending',
                   'gp-u25-endurance-end', 'gp-u26-side1-ending')

    def cmd(self, name, root='/r'):         # as gp_capture.main builds it
        scn = gs.SCENARIOS[name]
        return gc.dosbox_cmd(root, os.path.join(root, 'C'), os.path.join(root, 'CD', 'RAGECD.ISO'),
                             scn['time_limit'], name in gs.STOP_AT_END, scn.get('memsize'))

    def test_only_the_poked_finals_set_it(self):
        for name in gs.SCENARIOS:
            cmd, old = self.cmd(name), gc.dosbox_cmd('/r', '/r/C', '/r/CD/RAGECD.ISO',
                                                     gs.SCENARIOS[name]['time_limit'],
                                                     name in gs.STOP_AT_END)
            if name in self.POKED_FINAL:
                i = cmd.index('dosbox memory file=/r/guest.mem') + 1
                self.assertEqual(cmd[i:i + 2], self.MEM)
                self.assertEqual(cmd[:i] + cmd[i + 2:], old)
            else:
                self.assertFalse([a for a in cmd if 'memsize' in a], name)
                self.assertEqual(cmd, old, name)

    def test_the_captures_argv(self):
        # The argv= line of each capture's session.txt (shlex.join of the command): one
        # scenario without the flag, gp-u10-ending with it, after the memory file pair.
        import shlex
        seen = 0
        for name in ('gp-u9-win', 'gp-u10-ending'):
            path = os.path.join(ROOT, 'data', 'k11-captures', name, 'session.txt')
            if not os.path.isfile(path):
                continue
            with open(path) as f:
                argv = shlex.split(next(l for l in f if l.startswith('argv='))[5:])
            mem = next(a for a in argv if a.startswith('dosbox memory file='))
            root = os.path.dirname(mem.split('=', 1)[1])
            cmd = self.cmd(name, root)
            cmd[0] = argv[0]                        # the host's dosbox-x path
            self.assertEqual(cmd, argv, name)
            i = argv.index(mem) + 1
            self.assertEqual(argv[i:i + 2] == self.MEM, name == 'gp-u10-ending', name)
            seen += 1
        if not seen:
            self.skipTest('no gp-u9-win / gp-u10-ending capture')


class TestWallScenario(unittest.TestCase):
    """record §H6.9: gp-u17-device2's steps are wall-paced because the service
    menu's sub-loops freeze the master frame counter; the frame-keyed port script
    cannot follow them yet, so the capture's port-script CHECK reports it deferred
    instead of failing, and port_script refuses to write a broken script."""

    def test_the_port_script_is_deferred_not_broken(self):
        with self.assertRaises(gs.ScriptError) as e:
            gs.port_script('gp-u17-device2', [])
        self.assertIn('wall-paced steps', str(e.exception))
        sched = gs.Schedule(gs.SCENARIOS['gp-u17-device2']['steps'])
        rows = gc.run_checks('gp-u17-device2',
                             ['B ms=0 base=00266000 ptr=0000FE20'], sched, 0, 0)
        ps = [r for r in rows if r[0].startswith('port script v2')]
        self.assertEqual(len(ps), 1)
        self.assertTrue(ps[0][1], ps)
        self.assertIn('deferred: wall-paced steps', ps[0][0])


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

    def test_a_frame_counter_step_between_the_snapshot_and_the_write_is_a_race(self):
        # review of plan U9/U10 Task 3: the re-read's f alone differs (t508 unmoved)
        m, log = _mem(), io.StringIO()
        ref = self._ref(m)
        stepped = lambda: dict(gc.read_snap(m, BASE), f=ref['f'] + 1)
        self.assertEqual(gc.apply_poke(m, BASE, log, 5, 0x48F, ((0x10789E, b'\x78'),), 1, 12, ref, stepped), 1)
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


class TestCapturePreflight(unittest.TestCase):
    """Task 3: gp_capture.main runs the split capture preflight before it stages
    anything, so a host that is not ready for a capture writes no capture. The
    preflight's own checks are in test_capture_manifest; here only the wiring
    into gp_capture is guarded."""

    def test_gp_capture_imports_the_preflight(self):
        import capture_manifest
        self.assertIs(gc.cm, capture_manifest)
        self.assertIs(gc.cm.preflight_capture, capture_manifest.preflight_capture)

    def test_main_calls_the_preflight(self):
        import inspect
        src = inspect.getsource(gc.main)
        self.assertIn('cm.preflight_capture(', src)
        self.assertIn('preflight failed; no capture written', src)

    def test_main_fails_fast_on_a_gap(self):
        # Task 9: a missed S(c-1) exits before the frames are written (no capture),
        # and the summary CHECK is fed the guard's skip counter.
        import inspect
        src = inspect.getsource(gc.main)
        self.assertIn('poll.gap_f is not None', src)
        self.assertIn('no capture written', src)
        self.assertIn('run_checks(a.scenario, lines, poll.sched, n, len(idx), poll.guard)', src)


if __name__ == '__main__':
    unittest.main()
