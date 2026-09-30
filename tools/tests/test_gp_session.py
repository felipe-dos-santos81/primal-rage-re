# tools/tests/test_gp_session.py
import os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs


class TestTables(unittest.TestCase):
    def test_pad_bits_follow_the_isr_sampler(self):
        # spec §3.2: P1 byte is kb >> 8, P2 byte kb & 0xFF; up..right 0x80..0x10, b0..b3 1..8
        want = {'up': 0x80, 'down': 0x40, 'left': 0x20, 'right': 0x10,
                'b0': 0x01, 'b1': 0x02, 'b2': 0x04, 'b3': 0x08, 'start': 0x01}
        for n, bit in want.items():
            self.assertEqual(gs.PAD['p1.' + n][2], bit << 8, n)
            self.assertEqual(gs.PAD['p2.' + n][2], bit, n)

    def test_pad_scans_are_the_config_table_and_f1_f2(self):
        self.assertEqual([gs.PAD['p1.' + n][0] for n in ('up', 'down', 'left', 'right', 'b0', 'b1', 'b2', 'b3')],
                         [0x1F, 0x2D, 0x2C, 0x2E, 0x16, 0x17, 0x31, 0x32])
        self.assertEqual([gs.PAD['p2.' + n][0] for n in ('up', 'down', 'left', 'right', 'b0', 'b1', 'b2', 'b3')],
                         [0x48, 0x50, 0x4B, 0x4D, 0x47, 0x49, 0x4F, 0x51])
        self.assertEqual((gs.PAD['p1.start'][0], gs.PAD['p2.start'][0]), (0x3B, 0x3C))

    def test_raw_to_kb(self):
        # 0x500C4: raw = (+0x2D8 << 24) | (+0x2D9 << 8)
        self.assertEqual(gs.raw_to_kb(0x81004000), 0x8140)
        self.assertEqual(gs.raw_to_kb(0x00000100), 0x0001)
        self.assertEqual(gs.raw_to_kb(0), 0)

    def test_snap_fields_cover_the_spin_predicate_and_trace(self):
        names = [n for n, _, _ in gs.SNAP_FIELDS]
        for n in ('f', 't508', 't50c', 'raw', 'pad', 'e0', 'e2', 'rng') + gs.TRACE_FIELDS:
            self.assertIn(n, names)
        self.assertEqual(dict((n, (d, s)) for n, d, s in gs.SNAP_FIELDS)['f'], (0x0EF6DC, 2))


class TestFormat(unittest.TestCase):
    def test_s_record_round_trip(self):
        vals = {n: (i * 0x1111) & ((1 << (8 * s)) - 1) for i, (n, _, s) in enumerate(gs.SNAP_FIELDS)}
        line = gs.format_s(1234, vals, 0x4000, 0x1E, 0x20)
        rec = gs.parse(line)
        self.assertEqual(rec['kind'], 'S')
        self.assertEqual(rec['ms'], 1234)
        for n in vals:
            self.assertEqual(rec[n], vals[n], n)
        self.assertEqual((rec['kb'], rec['head'], rec['tail']), (0x4000, 0x1E, 0x20))

    def test_parse_keeps_names_and_decimals(self):
        rec = gs.parse('I ms=10 f=011D step=2 press=p1.up scan=1F lin=00010093 old=FF bios=1F73 ring=1 late=0')
        self.assertEqual((rec['press'], rec['step'], rec['f'], rec['late']), ('p1.up', 2, 0x11D, 0))
        rec = gs.parse('E ms=5 reason=time-limit rc=0')
        self.assertEqual((rec['reason'], rec['rc']), ('time-limit', 0))
        self.assertIsNone(gs.parse('   '))


class TestSchedule(unittest.TestCase):
    STEPS = (('boot', 25.0, ('key', 'enter')),
             ('after_mode', 0x27, 150, ('key', 'enter')),
             ('after', 150, ('pad', ('p1.up', 'p1.b1'), 4)),
             ('until_mode', 0x03, 2))

    def test_boot_fires_on_wall_time_only(self):
        s = gs.Schedule(self.STEPS)
        self.assertEqual(s.due_boot(24.9), [])
        self.assertEqual(s.due_boot(25.0), [(0, ('key', 'enter'))])
        self.assertEqual(s.due_boot(99.0), [])

    def test_after_fires_at_the_spin_before_its_frame(self):
        s = gs.Schedule(self.STEPS)
        s.due_boot(25.0)
        s.on_mode(0x120, 0x27)
        self.assertEqual(s.due(0x120 + 148), [])
        self.assertEqual(s.due(0x120 + 149), [(1, ('key', 'enter'))])   # frame F = f0 + 150
        self.assertEqual(s.due(0x120 + 150 + 148), [])
        self.assertEqual(s.due(0x120 + 150 + 149), [(2, ('pad', ('p1.up', 'p1.b1'), 4))])

    def test_a_late_snapshot_still_fires_once(self):
        s = gs.Schedule(self.STEPS)
        s.due_boot(25.0)
        s.on_mode(0x120, 0x27)
        self.assertEqual(s.due(0x120 + 160), [(1, ('key', 'enter'))])
        self.assertEqual(s.due(0x120 + 161), [])

    def test_until_mode_counts_only_after_the_last_action(self):
        s = gs.Schedule(self.STEPS)
        s.on_mode(0x10, 0x03)                  # mode 3 before the steps: ignored
        s.due_boot(25.0)
        s.on_mode(0x120, 0x27)
        s.due(0x120 + 149)
        s.due(0x120 + 299)
        self.assertIsNone(s.end_frame)
        s.on_mode(0x2000, 0x03)
        self.assertEqual(s.end_frame, 0x2002)
        self.assertFalse(s.ended(0x2001))
        self.assertTrue(s.ended(0x2002))
        self.assertEqual((s.fired, s.total), (3, 3))

    def test_expand(self):
        self.assertEqual(gs.expand(('key', 'enter')), [('enter', 0x1C, 0x1C0D, gs.HOLD_FRAMES)])
        self.assertEqual(gs.expand(('pad', ('p1.up', 'p2.b3'), 4)),
                         [('p1.up', 0x1F, 0x1F73, 4), ('p2.b3', 0x51, 0x5100, 4)])
        with self.assertRaises(KeyError):
            gs.expand(('pad', ('p3.up',), 1))


def _s(f, head=0x1E, raw=0, mode=0x27, st=0, **kw):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, raw=raw, mode=mode, st=st, t508=1, t50c=2, **kw)
    return gs.format_s(0, vals, gs.raw_to_kb(raw), head, 0x30)


def _log(extra_after=()):
    L = ['B ms=0 base=00266000 ptr=0000FE20']
    L += [_s(f, mode=3) for f in range(0x118, 0x11E)]
    L.append('I ms=1 f=011D step=0 press=enter scan=1C lin=00010090 old=FF bios=1C0D ring=1 late=0')
    L += [_s(0x11E, mode=3), _s(0x11F, mode=3)]
    L.append('H ms=2 f=0120 head=0020')
    L += [_s(f, head=0x20) for f in range(0x120, 0x126)]
    L.append('I ms=3 f=0125 step=1 press=p1.up scan=1F lin=00010093 old=FF bios=1F73 ring=1 late=0')
    L.append('H ms=4 f=0126 head=0022')
    L += [_s(0x126, head=0x22, raw=0x80000000), _s(0x127, head=0x22, raw=0x80000000)]
    # 0x12A: a pad change with no BIOS word (a --no-pad-bios press): bits only
    L += [_s(f, head=0x22, raw=0x100 if f == 0x12A else 0) for f in range(0x128, 0x131)]
    L += list(extra_after)
    L.append('X ms=5 f=0130 step=2 end')
    L.append('E ms=6 reason=time-limit rc=0')
    return L


class TestPortScript(unittest.TestCase):
    def setUp(self):
        gs.SCENARIOS['_t'] = dict(time_limit=1, steps=())

    def tearDown(self):
        gs.SCENARIOS.pop('_t', None)

    def test_script_keys_by_consumption_and_bits_by_raw(self):
        text = gs.port_script('_t', _log())
        lines = [l for l in text.splitlines() if not l.startswith('#')]
        self.assertEqual(lines, ['enter_frame 288', 'enter_state 0000',
                                 'key 288 1C 0D', 'key 294 1F 73',
                                 'bits 294 8000', 'bits 296 0000',
                                 'bits 298 0001', 'bits 299 0000', 'end 304'])

    def test_unpinned_key_is_an_error(self):
        L = [l for l in _log() if not (l.startswith('S ') and gs.parse(l)['f'] == 0x11F)]
        with self.assertRaises(gs.ScriptError):
            gs.port_script('_t', L)

    def test_bits_need_the_previous_snapshot(self):
        L = [l for l in _log() if not (l.startswith('S ') and gs.parse(l)['f'] == 0x129)]
        with self.assertRaises(gs.ScriptError):
            gs.port_script('_t', L)

    def test_enter_frame_must_be_the_first_key(self):
        L = [l.replace('mode=0027', 'mode=0003') if l.startswith('S ') and gs.parse(l)['f'] == 0x120 else l
             for l in _log()]
        with self.assertRaises(gs.ScriptError):
            gs.port_script('_t', L)

    def test_a_chord_is_consumed_in_one_iteration(self):
        # 0x24D08..0x24EE7 drains every queued word in one iteration (record
        # §G.4): three words queued in the spin of 0x12C, all consumed by 0x12D;
        # the poller writes one H record per word (heads 24, 26, 28).
        L = [l for l in _log() if l[0] not in 'XE' and not (l.startswith('S ') and gs.parse(l)['f'] >= 0x12C)]
        L += [_s(0x12C, head=0x22)]
        for k, (n, w) in enumerate((('p1.up', '1F73'), ('p1.b1', '1769'), ('p2.left', '4BE0'))):
            L.append('I ms=7 f=012C step=3 press=%s scan=00 lin=00010000 old=FF bios=%s ring=1 late=0' % (n, w))
        L += ['H ms=8 f=012D head=0024', 'H ms=8 f=012D head=0026', 'H ms=8 f=012D head=0028']
        L += [_s(f, head=0x28) for f in range(0x12D, 0x131)]
        L += ['X ms=9 f=0130 step=4 end']
        lines = [l for l in gs.port_script('_t', L).splitlines() if l.startswith('key')]
        self.assertEqual(lines, ['key 288 1C 0D', 'key 294 1F 73',
                                 'key 301 1F 73', 'key 301 17 69', 'key 301 4B E0'])
        # a word consumed after S(0x12D) was taken (a blocking loop, f frozen) is rejected
        L2 = L[:-1] + ['H ms=10 f=012D head=002A', 'X ms=9 f=0130 step=4 end']
        L2.insert(L2.index('H ms=8 f=012D head=0024'),
                  'I ms=7 f=012C step=3 press=p1.b2 scan=00 lin=00010000 old=FF bios=316E ring=1 late=0')
        with self.assertRaises(gs.ScriptError):
            gs.port_script('_t', L2)


class TestTraceDiff(unittest.TestCase):
    def test_first_difference_and_tick_apart(self):
        a = [_s(f, rng=f) for f in range(10)]
        b = [_s(f, rng=f if f < 7 else 0) for f in range(10) if f != 3]
        b[0] = _s(0, rng=0, tick=5)
        r = gs.trace_diff(a, b)
        self.assertEqual((r['first'], r['field'], r['compared'], r['tick_first']), (7, 'rng', 9, 0))

    def test_identical_traces(self):
        a = [_s(f) for f in range(4)]
        self.assertEqual(gs.trace_diff(a, list(a))['first'], None)


if __name__ == '__main__':
    unittest.main()
