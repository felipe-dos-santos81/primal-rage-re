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
        self.assertEqual(gc.publish(staged('good2'), out, True), out)
        with open(os.path.join(out, 'poll.log')) as f:
            self.assertEqual(f.read(), 'good2')
        self.assertEqual(sorted(os.listdir(d)), ['gp-x', 'gp-x.failed'])


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


if __name__ == '__main__':
    unittest.main()
