# tools/tests/test_gp_session.py
import os, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_moves as gm
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

    def test_the_u6_fields_are_appended(self):
        # plan gameplay-u6b, record gameplay-u6 §U6.11: after every U1 field, so older
        # poll.log lines are a prefix of the new format (U17's device words and the
        # fought-match phase's slot fields follow them)
        self.assertEqual(gs.SNAP_FIELDS[-11:-6], (('r0', 0x1088A8, 1), ('r1', 0x1088A9, 1),
                                                  ('c0', 0x10782A, 1), ('c1', 0x1078BE, 1),
                                                  ('s0_43', 0x1077F3, 1)))
        self.assertEqual(gs.MOVE_FIELDS, ('c0', 'c1', 'r0', 'r1', 's0_43'))
        self.assertFalse(set(gs.MOVE_FIELDS) & set(gs.TRACE_FIELDS))

    def test_the_device_fields_are_appended_and_pointer_relative(self):
        # host input truth U17, record §H6: dev1/dev2 follow the U1/U6 fields (older S
        # lines lack them) and live behind the key-config pointer DS_00101514, not at a
        # fixed DS address (their offsets are below DATA_BASE_VA)
        self.assertEqual(gs.SNAP_FIELDS[-6:-4], (('dev1', gs.KB_DEV1, 2), ('dev2', gs.KB_DEV2, 2)))
        self.assertEqual((gs.KB_DEV1, gs.KB_DEV2), (0x2D4, 0x2D6))
        self.assertEqual(gs.DEV_FIELDS, ('dev1', 'dev2'))
        self.assertEqual(set(gs.DEV_FIELDS), set(gs.POINTER_FIELDS))
        self.assertLess(gs.KB_DEV1, gs.DATA_BASE_VA)        # not a DS address
        # Opt-in: only gp-u17-device2 asserts them (gp_compare --dev-words)
        self.assertFalse(set(gs.DEV_FIELDS) & set(gs.TRACE_FIELDS))
        self.assertFalse(set(gs.DEV_FIELDS) & set(gs.MOVE_FIELDS))
        old = [(n, d, s) for n, d, s in gs.SNAP_FIELDS if n not in gs.DEV_FIELDS]
        body = ' '.join('%s=%0*X' % (n, 2 * s, 3) for n, _, s in old)
        snaps = gs.snapshots(['S ms=1 %s kb=0000 head=001E tail=001E' % body])
        self.assertNotIn('dev1', snaps[3])                  # absent, not zero
        self.assertEqual(snaps[3]['rng'], 3)

    def test_the_state_fields_are_appended_with_their_addresses_and_widths(self):
        self.assertEqual(gs.SNAP_FIELDS[-4:],
                         (('s0_30', 0x1077E0, 4), ('s0_42', 0x1077F2, 2),
                          ('s1_30', 0x107874, 4), ('s1_42', 0x107886, 2)))
        vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
        vals.update({'f': 0x1234, 's0_30': 0x202E, 's0_42': 0x0820,
                     's1_30': 0x13FF, 's1_42': 0x0008})
        line = gs.format_s(1, vals, 0, 0, 0)
        self.assertIn('s0_30=0000202E', line)
        self.assertIn('s0_42=0820', line)
        self.assertIn('s1_30=000013FF', line)
        self.assertIn('s1_42=0008', line)

    def test_an_old_s_line_without_the_state_fields_still_parses(self):
        old = ('S ms=1 f=1234 mode=0006 st=0000 tick=00000000 t508=00000000 '
               't50c=00000000 raw=00000000 pad=00000000 new=00000000 held=00000000 '
               'e0=0000 e2=0000 rng=00000000 cred=00000000 fp=00 b1d=00 b1f=03 b25=00 '
               'w10d=00 cnt=00 s0_52=00 s0_54=00 s0_5a=00 s1_52=00 s1_54=00 s1_5a=00 '
               'ent=00000000 r0=00 r1=00 c0=00 c1=00 s0_43=00 dev1=0000 dev2=0000 '
               'kb=0000 head=001E tail=003E')
        r = gs.parse(old)
        self.assertEqual(r['kind'], 'S')
        self.assertNotIn('s0_30', r)

    def test_the_u17_scenario_walks_to_modify_controls(self):
        # record §H6.6/§H6.9: the K11 walk's own keys (k11_session.WALK[2:24] after
        # the boot Enter and the START MENU Enter) to MODIFY CONTROLS, then one Down
        # (device 0 -> 2), the MODIFY CONTROLS leave, the OPTIONS MENU leave (the
        # master loop resumes so the cleared word is in an S record), and a wall end
        import k11_session as ks
        sc = gs.SCENARIOS['gp-u17-device2']
        self.assertTrue(sc.get('wall'))                  # wall-paced: the menu freezes f
        self.assertIn('gp-u17-device2', gs.STOP_AT_END)
        steps = sc['steps']
        self.assertEqual(steps[0], ('boot', gs.ENTER_WAIT, ('key', 'enter')))
        self.assertEqual(steps[1], ('after_mode', 0x27, 150, ('key', 'enter')))
        want = list(ks.WALK[2:24]) + ['down', 'esc', 'esc']
        self.assertEqual([s[-1][1] for s in steps[2:-1]], want)
        self.assertTrue(all(s[0] == 'after_wall' and s[1] == gs.U17_GAP for s in steps[2:-1]))
        self.assertTrue(all(s[-1][0] == 'key' and s[-1][2] == gs.U17_HOLD for s in steps[2:-1]))
        self.assertEqual(steps[-1], ('after_wall', 1.5, ('end',)))
        self.assertEqual(len(steps), 2 + 25 + 1)
        # the walk's own Down arrow, not P1's bound key (the K11 walk's scan)
        self.assertEqual(gs.KEYS['down'], (0x50, 0x5000))
        self.assertEqual(gs.KEYS['down'][0], ks.KEYS['down'][0])
        # the hold is under MODIFY CONTROLS' 0x0C-tick repeat gate (200 ms) and over
        # one menu poll (0x2EA78(1) = 3 ticks = 50 ms): the one-step derivation
        self.assertLess(gs.U17_HOLD * gs.FRAME_MS, 12 * gs.FRAME_MS)
        self.assertGreaterEqual(gs.U17_HOLD * gs.FRAME_MS, 3 * gs.FRAME_MS)

    def test_the_port_t_line_writes_every_snap_field_in_order(self):
        # port/tests/test_game.c gp_trace_line: the capture's S names and order (spec §4.2).
        # plan gameplay-u11 Re-baseline (merge order): the T line is SNAP_FIELDS then
        # KEYS_EXTRA (the order gp-keys-fight's S lines have). One test for both units' checks
        # (U11 rebase onto U6b): review of U6b Task 1 (m1): the names alone let a placeholder of
        # the wrong width or a DS_ symbol of the wrong address through.
        import re
        with open(os.path.join(ROOT, 'port', 'tests', 'test_game.c')) as fh:
            src = fh.read()
        body = src[src.index('static void gp_trace_line(void)'):]
        call = body[:body.index(');\n    gp_trace_lines++')]
        # comments (/* */ and //) go before the format string and the arguments are read
        call = re.sub(r'/\*.*?\*/|//[^\n]*', '', call, flags=re.S)
        fmt = ''.join(re.findall(r'"([^"]*)"', call))
        self.assertTrue(fmt.startswith('T '), fmt)
        fields = gs.SNAP_FIELDS + gs.KEYS_EXTRA + gs.WIN_EXTRA     # + plan U9/U10's (record §W.2)
        parts = fmt[2:].replace('\\n', '').split()
        self.assertEqual([p.split('=')[0] for p in parts], [n for n, _, _ in fields])
        # each placeholder is as wide as the field (2 hex digits per byte)
        self.assertEqual([int(w) for w in re.findall(r'%0(\d)X', fmt)], [2 * s for _, _, s in fields])
        rest = call[call.rindex('"') + 1:].lstrip(' ,')
        args, depth, cur = [], 0, ''
        for ch in rest:
            if ch == ',' and depth == 0:
                args.append(cur.strip())
                cur = ''
                continue
            depth += (ch == '(') - (ch == ')')
            cur += ch
        args.append(cur.strip())
        self.assertEqual(len(parts), len(fields))
        self.assertEqual(len(args), len(fields), args)
        acc = {1: 'DSB', 2: 'DSW', 4: 'DSD'}
        for part, arg, (n, addr, size) in zip(parts, args, fields):
            self.assertEqual(part, '%s=%%0%dX' % (n, 2 * size), n)
            # review of U6b Task 2 (m4): the accessor reads the field's size
            self.assertRegex(arg, r'\b%s\(' % acc[size], (n, arg))
            # the five U6 fields (record gameplay-u6 §U6.11) and the three U11 key fields
            # (record gameplay-u11 §K.3) read the field's address
            if n in gs.MOVE_FIELDS or (n, addr, size) in gs.KEYS_EXTRA + gs.WIN_EXTRA:
                m = re.search(r'\bDS_([0-9A-F]{8})(?:\s*\+\s*(0x[0-9A-Fa-f]+|[0-9]+)u?)?\)', arg)
                self.assertIsNotNone(m, (n, arg))
                got = int(m.group(1), 16) + (int(m.group(2), 0) if m.group(2) else 0)
                self.assertEqual(got, addr, (n, arg))
            # KEYS_EXTRA: accessor by size on the field's own DS_ symbol, no offset
            if (n, addr, size) in gs.KEYS_EXTRA + gs.WIN_EXTRA:
                self.assertEqual(arg, '(unsigned)%s(DS_%08X)' % (acc[size], addr), n)

    def test_an_s_line_without_the_u6_fields_still_parses(self):
        old = [(n, d, s) for n, d, s in gs.SNAP_FIELDS if n not in gs.MOVE_FIELDS]
        body = ' '.join('%s=%0*X' % (n, 2 * s, 7) for n, _, s in old)
        snaps = gs.snapshots(['S ms=1 %s kb=0000 head=001E tail=001E' % body])
        self.assertEqual(snaps[7]['rng'], 7)
        self.assertNotIn('r0', snaps[7])


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

    def test_after_wall_fires_on_the_poller_clock(self):
        # record §H6.9: the service menu's sub-loops freeze f, so its steps are
        # wall-paced; the end is a wall moment too (end_wall, not end_frame)
        s = gs.Schedule((('boot', 25.0, ('key', 'enter')),
                         ('after_mode', 0x27, 10, ('key', 'enter')),
                         ('after_wall', 1.0, ('key', 'down')),
                         ('after_wall', 1.0, ('key', 'esc')),
                         ('after_wall', 1.5, ('end',))))
        self.assertEqual(s.due_boot(25.0), [(0, ('key', 'enter'))])
        s.due_wall(26.4)                                # the poller's clock, every pass
        s.on_mode(0x100, 0x27)
        self.assertEqual(s.due(0x100 + 8), [])
        self.assertEqual(s.due(0x100 + 9), [(1, ('key', 'enter'))])   # last_wall = 26.4
        self.assertEqual(s.due_wall(27.3), [])                        # 26.4 + 1.0 = 27.4
        self.assertEqual(s.due_wall(27.4), [(2, ('key', 'down'))])
        self.assertEqual(s.due_wall(28.0), [])
        self.assertEqual(s.due_wall(28.4), [(3, ('key', 'esc'))])
        self.assertEqual(s.due_wall(29.8), [])                        # the end at 29.9
        self.assertFalse(s.ended_wall(29.8))
        self.assertEqual(s.due_wall(29.9), [])
        self.assertTrue(s.ended_wall(29.9))
        self.assertEqual(s.fired, 4)                    # the end is not an action
        self.assertIsNone(s.end_frame)                   # and it names no frame

    def test_a_wall_step_never_fires_due_from_a_spin(self):
        # due(f) stops at an after_wall step: only the wall clock reaches it
        s = gs.Schedule((('boot', 1.0, ('key', 'enter')),
                         ('after_wall', 1.0, ('key', 'down'))))
        s.due_boot(1.0)
        self.assertEqual(s.due(0xFFFFFFFF), [])
        self.assertEqual(s.due_wall(2.0), [(1, ('key', 'down'))])

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

    def test_stop_on_mode_arms_the_end_frame_and_counts_as_done(self):
        steps = (('after', 10, ('pad', ('p1.b0',), 4)),
                 ('after', 10, ('pad', ('p1.b1',), 4)))
        s = gs.Schedule(steps, stop_mode=0x08, stop_delay=30)
        s.on_mode(100, 0x06)
        s.on_mode(200, 0x08)
        self.assertEqual(s.end_frame, 230)
        self.assertFalse(s.ended(229))
        self.assertTrue(s.ended(230))
        self.assertTrue(s.stopped_on_mode)
        self.assertTrue(s.steps_ok)                 # the unfinished steps are intentional

    def test_skip_on_mode_drops_the_round_and_resumes_at_the_next_anchor(self):
        steps = (('after', 10, ('pad', ('p1.b0',), 4)),
                 ('after', 10, ('pad', ('p1.b1',), 4)),
                 ('after_entry', 0x06, 2, 10, ('pad', ('p1.b0',), 4)),
                 ('after', 10, ('pad', ('p1.b1',), 4)))
        s = gs.Schedule(steps, skip_on_mode=0x08)
        s.on_mode(100, 0x06)
        s.on_mode(200, 0x08)
        self.assertEqual(s.i, 2)                    # the next after_entry stays armed
        self.assertEqual(s.skipped, 2)              # both remaining round steps dropped
        self.assertFalse(s.steps_ok)                # the anchor and its tail still to fire

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


    def test_consumption_needs_the_old_head_before(self):
        # review 1: S(c - 1) must show exactly the head before frame c's words,
        # not merely a head other than H's.
        L = [_s(0x125, head=0x24) if l.startswith('S ') and gs.parse(l)['f'] == 0x125 else l for l in _log()]
        with self.assertRaises(gs.ScriptError):
            gs.port_script('_t', L)

    def test_end_to_last_s_accepts_the_capture_tail(self):
        # U13's shape (record §E1.4): the capture's last S record (f=271D's X,
        # then 55 more S records to f=2754) lies past its X record, so a script
        # that replays the tail needs the opt-in; the default refuses it as before.
        L = _log(extra_after=[_s(f, head=0x22) for f in range(0x131, 0x161)])
        with self.assertRaises(gs.ScriptError):
            gs.port_script('_t', L, end=0x160)
        lines = [l for l in gs.port_script('_t', L, end=0x160, end_to_last_s=True).splitlines()
                 if not l.startswith('#')]
        self.assertEqual(lines[-1], 'end 352')
        # a scenario whose X is its last S: the flag changes nothing
        self.assertEqual(gs.port_script('_t', _log(), end_to_last_s=True),
                         gs.port_script('_t', _log()))

    def test_a_press_the_full_ring_dropped_is_named(self):
        L = _log()
        k = next(i for i, l in enumerate(L) if 'press=p1.up' in l)
        L[k] = L[k].replace('ring=1', 'ring=0')
        with self.assertRaises(gs.ScriptError) as cm:
            gs.port_script('_t', L)
        self.assertIn('ring was full', str(cm.exception))

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


class TestIdleLoss(unittest.TestCase):
    def test_steps_fire_on_the_probe_path(self):
        # spec §3.5: mode 0x27 first at 0x120, back in mode 3 at 0x22BA
        s = gs.Schedule(gs.SCENARIOS['gp-idle-loss']['steps'])
        self.assertEqual(s.due_boot(gs.ENTER_WAIT), [(0, ('key', 'enter'))])
        s.on_mode(0x120, 0x27)
        self.assertEqual(s.due(0x120 + 149), [(1, ('key', 'enter'))])
        self.assertEqual(s.due(0x120 + 299), [(2, ('key', 'enter'))])
        for f, m in ((0x247, 0x2D), (0x26C, 0x10), (0x7B5, 6), (0x1F84, 0x13), (0x2200, 0x1E)):
            s.on_mode(f, m)
        self.assertIsNone(s.end_frame)
        s.on_mode(0x22BA, 0x03)
        self.assertEqual((s.end_frame, s.fired, s.total), (0x22BA, 3, 3))
        self.assertEqual(gs.SCENARIOS['gp-idle-loss-run2']['steps'], gs.SCENARIOS['gp-idle-loss']['steps'])

    def test_end_truncates_the_script(self):
        gs.SCENARIOS['_t'] = dict(time_limit=1, steps=())
        try:
            text = gs.port_script('_t', _log(), end=295)
            lines = [l for l in text.splitlines() if not l.startswith('#')]
            self.assertEqual(lines, ['enter_frame 288', 'enter_state 0000',
                                     'key 288 1C 0D', 'key 294 1F 73', 'bits 294 8000', 'end 295'])
            # a cut before the second key: the key and its bits are dropped (the plan's
            # end=295 keeps every key, so it cannot see the key filter; record §G.17)
            text = gs.port_script('_t', _log(), end=290)
            lines = [l for l in text.splitlines() if not l.startswith('#')]
            self.assertEqual(lines, ['enter_frame 288', 'enter_state 0000', 'key 288 1C 0D', 'end 290'])
            with self.assertRaises(gs.ScriptError):
                gs.port_script('_t', _log(), end=400)
            with self.assertRaises(gs.ScriptError) as cm:         # review 1 minor 11: before the Enter
                gs.port_script('_t', _log(), end=287)
            self.assertIn('before the Enter', str(cm.exception))
            self.assertIn('key 288 1C 0D', gs.port_script('_t', _log(), end=288))        # past the capture's X record
        finally:
            gs.SCENARIOS.pop('_t', None)


class TestCharsel(unittest.TestCase):
    """gp-u5-charsel (record 2026-10-01-gameplay-u5 §C5.4): the walk, modelled on the raw."""

    @staticmethod
    def stick(c, bit):
        # 0x43B24's stick (0x43BB5..0x43C99) on the signed cursor byte DS_00108166[side]
        if bit == 0x10 and c < 6:
            return c + 1
        if bit == 0x20 and c > 0:
            return c - 1
        if bit == 0x40:
            c = c + 4 if c < 4 else c
            return 6 if c >= 7 else c
        if bit == 0x80 and c >= 4:
            return c - 4
        return c

    def walk(self):
        steps = gs.SCENARIOS['gp-u5-charsel']['steps']
        i = next(k for k, st in enumerate(steps) if st[0] == 'after_mode' and st[1] == 0x10)
        return steps[i:]

    def test_the_walk_visits_every_cell_and_confirms_on_1(self):
        c, seen = 0, [0]                       # 0xC8880: P1 starts on 0 (0x4370D, DS_00108173 == 0)
        for st in self.walk():
            if st[0] == 'until_mode':
                break
            (name,), hold = st[-1][1], st[-1][2]
            self.assertTrue(2 <= hold < 0x1F, name)
            bit = gs.PAD[name][2] >> 8
            if bit & 0xF0:
                c = self.stick(c, bit & 0xF0)
                seen.append(c)
            else:
                self.assertEqual(bit & 1, 1, name)          # 0x43CAD: e0 bit 0 confirms
        self.assertEqual(seen, [0, 0, 1, 2, 3, 6, 5, 4, 0, 1])
        self.assertEqual(sorted(set(seen)), list(range(7)))

    def test_presses_are_separate_edges_before_the_time_out(self):
        w = self.walk()
        gaps = [st[2] if st[0] == 'after_mode' else st[1] for st in w if st[0] != 'until_mode']
        holds = [st[-1][2] for st in w if st[0] != 'until_mode']
        for g, h in zip(gaps[1:], holds):
            self.assertGreaterEqual(g, h + 2)      # a released bit falls before the next press
        self.assertLess(sum(gaps), 14 * 64)        # the earliest pick time-out
        self.assertEqual(w[-1], ('until_mode', 0x06, 0))

    def test_schedule_fires_the_walk(self):
        s = gs.Schedule(gs.SCENARIOS['gp-u5-charsel']['steps'])
        s.due_boot(gs.ENTER_WAIT)
        s.on_mode(0x141, 0x27)
        s.due(0x141 + 149); s.due(0x141 + 299)
        s.on_mode(0x26E, 0x2D); s.on_mode(0x293, 0x10)
        fired = []
        for f in range(0x293, 0x293 + 500):
            fired += [(f + 1, a) for _, a in s.due(f)]
        self.assertEqual([f for f, _ in fired], [0x293 + 60 + 40 * k for k in range(10)])
        self.assertEqual(fired[-1][1], ('pad', ('p1.start',), 6))
        s.on_mode(0x439, 0x1A)
        self.assertIsNone(s.end_frame)
        s.on_mode(0x5EE, 0x06)
        self.assertEqual((s.end_frame, s.fired, s.total), (0x5EE, 13, 13))


class TestU6Moves(unittest.TestCase):
    def test_the_moves_fire_from_round_1(self):
        # plan gameplay-u6b: the idle-loss menu path, then 34 press groups from 10 frames
        # after mode 6 begins, the end 1200 frames after the first (record §U6.12)
        s = gs.Schedule(gs.SCENARIOS['gp-u6-moves']['steps'])
        self.assertEqual(s.due_boot(gs.ENTER_WAIT), [(0, ('key', 'enter'))])
        s.on_mode(0x141, 0x27)
        self.assertEqual(s.due(0x141 + 149), [(1, ('key', 'enter'))])
        self.assertEqual(s.due(0x141 + 299), [(2, ('key', 'enter'))])
        for f, m in ((0x26E, 0x2D), (0x293, 0x10), (0x77A, 5)):
            s.on_mode(f, m)
            self.assertEqual(s.due(f), [])
        s.on_mode(0x7F5, 6)
        self.assertEqual(s.due(0x7F5 + 8), [])
        self.assertEqual(s.due(0x7FF - 1), [(3, ('pad', ('p1.b0', 'p1.b1'), 4))])
        fired = 4
        for f in range(0x7FF, 0xCAF + 1):
            fired += len(s.due(f))
        self.assertEqual((fired, s.total, s.end_frame), (37, 37, 0x7FF + 1200))

    def test_the_recapture_scenario_is_a_copy_that_stops_at_its_end(self):
        # U6b re-capture (user decision): gp-u6-moves-b differs from gp-u6-moves only by name
        a, b = gs.SCENARIOS['gp-u6-moves'], gs.SCENARIOS['gp-u6-moves-b']
        self.assertIsNot(a, b)
        self.assertEqual(a, b)
        self.assertIs(a['steps'], b['steps'])
        self.assertEqual(b['time_limit'], 130)
        self.assertIn('gp-u6-moves-b', gs.STOP_AT_END)


class TestWinPokes(unittest.TestCase):
    """Plan gameplay-u9-u10 (record 2026-10-02-gameplay-u9-u10-derivations.md §W.8)."""
    STEPS = (('boot', 25.0, ('key', 'enter')),
             ('after_entry', 0x06, 2, 10, ('poke', ((0x10789E, b'\x78'),))),
             ('after_entry', 0x06, 3, 60, ('end',)))

    def test_after_entry_keys_on_the_kth_s_entry(self):
        s = gs.Schedule(self.STEPS)
        s.on_snap(0x50, 0x03)
        s.on_snap(0x60, 0x06)                     # before the boot step: not counted
        s.due_boot(25.0)
        for f, mode in ((0x100, 0x27), (0x200, 0x06), (0x210, 0x06), (0x300, 0x08), (0x400, 0x06)):
            s.on_snap(f, mode)
        self.assertEqual(s.entry_frame, {(0x27, 1): 0x100, (0x06, 1): 0x200, (0x08, 1): 0x300,
                                         (0x06, 2): 0x400})
        self.assertEqual(s.due(0x400 + 8), [])
        self.assertEqual(s.due(0x400 + 9), [(1, ('poke', ((0x10789E, b'\x78'),)))])
        s.on_snap(0x500, 0x09)
        s.on_snap(0x600, 0x06)
        self.assertFalse(s.ended(0x600 + 59))
        s.due(0x600 + 59)
        self.assertEqual(s.end_frame, 0x63C)
        self.assertEqual((s.fired, s.total), (2, 2))

    def test_w_record_round_trip(self):
        w = gs.format_w(12, 0x48F, 5, 0x108106, b'\x00' * 7, b'\x80' * 7, 0, 0)
        self.assertEqual(w, 'W ms=12 f=048F step=5 addr=00108106 len=7 was=00000000000000 '
                            'now=80808080808080 late=0 race=0')
        r = gs.parse(w)
        self.assertEqual((r['kind'], r['f'], r['addr'], r['len'], r['was'], r['now'], r['race']),
                         ('W', 0x48F, 0x108106, 7, '00000000000000', '80808080808080', 0))
        self.assertEqual(gs.lands_marked(0, 0), b'\x80' * 7)
        self.assertEqual(gs.lands_marked(1, 3), b'\xC3' * 7)
        self.assertEqual(gs.WIN_FIELDS[:4], ('afc', 'ad4', 'w2', 'w3'))

    def test_the_scenarios(self):
        u9 = gs.SCENARIOS['gp-u9-win']['steps']
        self.assertEqual([st[-1] for st in u9
                          if isinstance(st[-1], tuple) and st[-1][0] == 'poke'],
                         [gs.KO_P2, gs.KO_P2])
        u10 = gs.SCENARIOS['gp-u10-ending']['steps']
        pokes = [st for st in u10 if isinstance(st[-1], tuple) and st[-1][0] == 'poke']
        self.assertEqual(len(pokes), 2 + 14)
        self.assertEqual(pokes[0][-1][1], ((0x108106, b'\x80' * 7), (0x10789E, b'\x78')))
        self.assertEqual([(st[1], st[2]) for st in pokes[2:]],
                         [(m, k) for k in range(1, 8) for m in (0x0C, 0x0D)])
        self.assertEqual(pokes[1][-1], gs.KO_P2)                          # round 2
        self.assertEqual([st[-1] for st in pokes[2:]], [gs.KO_P2, gs.DEATH_DONE] * 7)
        u9_pokes = [st for st in u9 if isinstance(st[-1], tuple) and st[-1][0] == 'poke']
        self.assertEqual([st[3] for st in u9_pokes + pokes], [10] * (2 + 16))   # every frame offset
        self.assertEqual(u10[-1], ('until_mode', 0x03, 0))
        for name in ('gp-u9-win', 'gp-u10-ending'):
            self.assertIn(name, gs.STOP_AT_END)
            self.assertEqual(gs.SCENARIOS[name]['extra'], gs.WIN_EXTRA)


class TestEnduranceFight(unittest.TestCase):
    """gp-u12-endurance (plan 2026-10-20-gameplay-truth-ii, record §J4): the ENDURANCE
    team pass 0x44798 and modes 0x30/0x31, past gp-u8-endurance's D3 cut."""

    def test_the_endurance_fight_scenario_reaches_the_team_pass(self):
        scn = gs.SCENARIOS['gp-u12-endurance']
        steps = scn['steps']
        self.assertEqual(scn['time_limit'], 70)
        self.assertEqual(steps, gs.SCENARIOS['gp-u8-endurance']['steps'][:-1]
                         + gs.ENDURANCE_PICKS + (('until_mode', 0x30, 300),))
        self.assertTrue(any(s[0] == 'until_mode' and s[1] in (0x10, 0x30) for s in steps))
        self.assertTrue(any(s[0] in ('after', 'after_mode', 'after_entry') for s in scn['steps']))

    def test_the_capture_stops_at_the_scenarios_end(self):
        # The 70 s limit leaves ~18 s of idle fight past the end frame; gp_capture
        # stops STOP_TAIL frames after it (a name not in STOP_AT_END would run on).
        self.assertIn('gp-u12-endurance', gs.STOP_AT_END)


class TestSecondEnding(unittest.TestCase):
    """gp-u13-ending2 (plan 2026-10-20-gameplay-truth-ii Task 5; record
    2026-10-01-gameplay-u5 §C5.4/§C5.7, 2026-10-02-gameplay-u9-u10 §W.6): the
    non-character-0 ending. The character-select cursor starts on cell 0 (0xC8880 = 00 05)
    and each p1.right moves it one cell (0x43B24); the confirm is 0x43D60's
    DS_0010816A = 0xC8882[cursor] (0x43D6E/0x43D74) and 0xC8882 = 00 02 03 01 06 04 05,
    so cell 3 is class 1 (BLIZZARD), whose ending is DS_0010782A[DS_00104AD4 * 0x94] = 1.
    U5's capture confirms cell 1 and shows TALON (class 2), not BLIZZARD."""

    def test_the_cursor_reaches_cell_three_before_the_confirm(self):
        steps = gs.SCENARIOS['gp-u13-ending2']['steps']
        u10 = gs.SCENARIOS['gp-u10-ending']['steps']
        i = next(k for k, st in enumerate(steps)
                 if st[0] == 'after_mode' and st[1] == 0x10)
        j = next(k for k, st in enumerate(steps) if st[-1] == ('pad', ('p1.start',), 6))
        self.assertEqual([st[-1] for st in steps[i:j]], [('pad', ('p1.right',), 4)] * 3)
        self.assertEqual([st[2] if st[0] == 'after_mode' else st[1] for st in steps[i:j]],
                         [60, 30, 30])
        self.assertEqual(steps[j], ('after', 30, ('pad', ('p1.start',), 6)))
        self.assertEqual(steps[:i], u10[:i])                    # the same menu path
        self.assertIsNot(steps, u10)                            # a new tuple, u10 unchanged
        self.assertIn('gp-u13-ending2', gs.STOP_AT_END)

    def test_the_first_poke_marks_blizzards_lands(self):
        # 0x41C28 state 1 tallies a mark only when (mark & 0x7F) == DS_0010782A[side] |
        # side << 6 (0x41D39/0x41D5C): SAURON's 0x80 bytes count no land for P1 = class 1.
        steps = gs.SCENARIOS['gp-u13-ending2']['steps']
        u10 = gs.SCENARIOS['gp-u10-ending']['steps']
        i = next(k for k, st in enumerate(steps) if st[0] == 'after_entry' and st[1] == 0x06)
        k = next(k for k, st in enumerate(u10) if st[0] == 'after_entry' and st[1] == 0x06)
        self.assertEqual(steps[i][4], ('poke', ((0x108106, gs.lands_marked(0, 1)),
                                                (0x10789E, b'\x78'))))
        self.assertEqual(steps[i + 1:], u10[k + 1:])            # the rest is u10's
        self.assertEqual(u10[k][4], ('poke', ((0x108106, gs.lands_marked(0, 0)),
                                              (0x10789E, b'\x78'))))


class TestTalonEnding(unittest.TestCase):
    """gp-u18-talon-ending (endgame truth Task 3; record §E1.1): P1 confirms the
    character-select cell 1, which 0xC8882 = 00 02 03 01 06 04 05 (0x43D6E/0x43D74) maps
    to class 2 TALON, whose ending is DS_0010782A[DS_00104AD4 * 0x94] = 2 (title id
    0xE0, TALON'S DREAM). ending_menu(cell) is WIN_MENU's prefix with the cell's right
    presses before the confirm: cell 0 is WIN_MENU, cell 3 is WIN_MENU1 (BLIZZARD)."""

    def test_ending_menu_walks_the_cell_before_the_confirm(self):
        menu = gs.WIN_MENU
        one = gs.ending_menu(1)
        self.assertEqual(one[:len(menu) - 1], menu[:-1])     # the same menu path
        self.assertEqual(one[len(menu) - 1:], (
            ('after_mode', 0x10, 60, ('pad', ('p1.right',), 4)),
            ('after', 30, ('pad', ('p1.start',), 6)),
        ))
        self.assertEqual(gs.ending_menu(0), gs.WIN_MENU)
        self.assertEqual(gs.ending_menu(3), gs.WIN_MENU1)

    def test_the_scenario_marks_talons_lands_and_clones_u10s_tail(self):
        scn = gs.SCENARIOS['gp-u18-talon-ending']
        self.assertIn('gp-u18-talon-ending', gs.STOP_AT_END)
        self.assertEqual((scn['time_limit'], scn['memsize'], scn['extra']),
                         (240, 64, gs.WIN_EXTRA))
        steps, u10 = scn['steps'], gs.SCENARIOS['gp-u10-ending']['steps']
        i = next(k for k, st in enumerate(steps) if st[0] == 'after_entry' and st[1] == 0x06)
        k = next(k for k, st in enumerate(u10) if st[0] == 'after_entry' and st[1] == 0x06)
        # the cell-1 walk itself (ending_menu's own test pins the full helper shape)
        self.assertEqual((steps[3][0], steps[3][1], steps[3][2], steps[3][-1]),
                         ('after_mode', 0x10, 60, ('pad', ('p1.right',), 4)))
        self.assertEqual(steps[4], ('after', 30, ('pad', ('p1.start',), 6)))
        self.assertEqual(steps[i], ('after_entry', 0x06, 1, 10,
                                    ('poke', ((0x108106, gs.lands_marked(0, 2)),
                                              (0x10789E, b'\x78')))))
        self.assertEqual(gs.lands_marked(0, 2), b'\x82' * 7)   # 0x80 | side 0 | class 2
        self.assertEqual(steps[i + 1:], u10[k + 1:])           # the rest is u10's
        self.assertEqual(len(u10), 21)                         # u10 itself, not extended
        self.assertEqual(u10[k][4], ('poke', ((0x108106, gs.lands_marked(0, 0)),
                                              (0x10789E, b'\x78'))))


class TestVertigoEnding(unittest.TestCase):
    """gp-u19-vertigo-ending (endgame truth Task 4; record §E1.1/§E4): P1 confirms the
    character-select cell 2, which 0xC8882 = 00 02 03 01 06 04 05 (0x43D6E/0x43D74) maps
    to class 3 VERTIGO, whose ending is DS_0010782A[DS_00104AD4 * 0x94] = 3 (title id
    0xE1, THE PALACE OF VERTIGO). ending_menu(2) is two p1.right presses from cell 0, so
    the walk is cell 2's own; the scene is otherwise Task 3's TALON arrangement."""

    def test_the_walk_is_two_rights_from_cell_zero(self):
        menu, steps = gs.WIN_MENU, gs.SCENARIOS['gp-u19-vertigo-ending']['steps']
        self.assertEqual(steps[:len(menu) - 1], menu[:-1])        # the same menu path
        self.assertEqual(steps[3:6], (
            ('after_mode', 0x10, 60, ('pad', ('p1.right',), 4)),  # cell 0 -> 1
            ('after', 30, ('pad', ('p1.right',), 4)),             # 1 -> 2 (class 3)
            ('after', 30, ('pad', ('p1.start',), 6)),             # confirm cursor 2
        ))

    def test_the_scenario_marks_vertigos_lands_and_clones_u10s_tail(self):
        scn = gs.SCENARIOS['gp-u19-vertigo-ending']
        self.assertIn('gp-u19-vertigo-ending', gs.STOP_AT_END)
        self.assertEqual((scn['time_limit'], scn['memsize'], scn['extra']),
                         (240, 64, gs.WIN_EXTRA))
        steps, u10 = scn['steps'], gs.SCENARIOS['gp-u10-ending']['steps']
        i = next(k for k, st in enumerate(steps) if st[0] == 'after_entry' and st[1] == 0x06)
        k = next(k for k, st in enumerate(u10) if st[0] == 'after_entry' and st[1] == 0x06)
        self.assertEqual(steps[i], ('after_entry', 0x06, 1, 10,
                                    ('poke', ((0x108106, gs.lands_marked(0, 3)),
                                              (0x10789E, b'\x78')))))
        self.assertEqual(gs.lands_marked(0, 3), b'\x83' * 7)      # 0x80 | side 0 | class 3
        self.assertEqual(steps[i + 1:], u10[k + 1:])              # the rest is u10's
        self.assertEqual(len(u10), 21)                            # u10 itself, not extended


class TestChaosEnding(unittest.TestCase):
    """gp-u20-chaos-ending (endgame truth Task 5; record §E1.1/§E5): P1 confirms the
    character-select cell 4, which 0xC8882 = 00 02 03 01 06 04 05 (0x43D6E/0x43D74) maps
    to class 6 CHAOS, whose ending is DS_0010782A[DS_00104AD4 * 0x94] = 6 (title id
    0xE4, CHAOS' REDEMPTION). ending_menu(4) is four p1.right presses from cell 0, so
    the walk is cell 4's own; the scene is otherwise Task 3's TALON arrangement."""

    def test_the_walk_is_four_rights_from_cell_zero(self):
        menu, steps = gs.WIN_MENU, gs.SCENARIOS['gp-u20-chaos-ending']['steps']
        self.assertEqual(steps[:len(menu) - 1], menu[:-1])        # the same menu path
        self.assertEqual(steps[3:8], (
            ('after_mode', 0x10, 60, ('pad', ('p1.right',), 4)),  # cell 0 -> 1
            ('after', 30, ('pad', ('p1.right',), 4)),             # 1 -> 2
            ('after', 30, ('pad', ('p1.right',), 4)),             # 2 -> 3
            ('after', 30, ('pad', ('p1.right',), 4)),             # 3 -> 4 (class 6)
            ('after', 30, ('pad', ('p1.start',), 6)),             # confirm cursor 4
        ))

    def test_the_scenario_marks_chaos_lands_and_clones_u10s_tail(self):
        scn = gs.SCENARIOS['gp-u20-chaos-ending']
        self.assertIn('gp-u20-chaos-ending', gs.STOP_AT_END)
        self.assertEqual((scn['time_limit'], scn['memsize'], scn['extra']),
                         (240, 64, gs.WIN_EXTRA))
        steps, u10 = scn['steps'], gs.SCENARIOS['gp-u10-ending']['steps']
        i = next(k for k, st in enumerate(steps) if st[0] == 'after_entry' and st[1] == 0x06)
        k = next(k for k, st in enumerate(u10) if st[0] == 'after_entry' and st[1] == 0x06)
        self.assertEqual(steps[i], ('after_entry', 0x06, 1, 10,
                                    ('poke', ((0x108106, gs.lands_marked(0, 6)),
                                              (0x10789E, b'\x78')))))
        self.assertEqual(gs.lands_marked(0, 6), b'\x86' * 7)      # 0x80 | side 0 | class 6
        self.assertEqual(steps[i + 1:], u10[k + 1:])              # the rest is u10's
        self.assertEqual(len(u10), 21)                            # u10 itself, not extended


class TestArmadonEnding(unittest.TestCase):
    """gp-u21-armadon-ending (endgame truth Task 6; record §E1.1/§E6): P1 confirms the
    character-select cell 5, which 0xC8882 = 00 02 03 01 06 04 05 (0x43D6E/0x43D74) maps
    to class 4 ARMADON, whose ending is DS_0010782A[DS_00104AD4 * 0x94] = 4 (title id
    0xE2, THE MEDITATION OF ARMADON). ending_menu(5) is five p1.right presses from cell 0,
    so the walk is cell 5's own; the scene is otherwise Task 3's TALON arrangement."""

    def test_the_walk_is_five_rights_from_cell_zero(self):
        menu, steps = gs.WIN_MENU, gs.SCENARIOS['gp-u21-armadon-ending']['steps']
        self.assertEqual(steps[:len(menu) - 1], menu[:-1])        # the same menu path
        self.assertEqual(steps[3:9], (
            ('after_mode', 0x10, 60, ('pad', ('p1.right',), 4)),  # cell 0 -> 1
            ('after', 30, ('pad', ('p1.right',), 4)),             # 1 -> 2
            ('after', 30, ('pad', ('p1.right',), 4)),             # 2 -> 3
            ('after', 30, ('pad', ('p1.right',), 4)),             # 3 -> 4
            ('after', 30, ('pad', ('p1.right',), 4)),             # 4 -> 5 (class 4)
            ('after', 30, ('pad', ('p1.start',), 6)),             # confirm cursor 5
        ))

    def test_the_scenario_marks_armadons_lands_and_clones_u10s_tail(self):
        scn = gs.SCENARIOS['gp-u21-armadon-ending']
        self.assertIn('gp-u21-armadon-ending', gs.STOP_AT_END)
        self.assertEqual((scn['time_limit'], scn['memsize'], scn['extra']),
                         (240, 64, gs.WIN_EXTRA))
        steps, u10 = scn['steps'], gs.SCENARIOS['gp-u10-ending']['steps']
        i = next(k for k, st in enumerate(steps) if st[0] == 'after_entry' and st[1] == 0x06)
        k = next(k for k, st in enumerate(u10) if st[0] == 'after_entry' and st[1] == 0x06)
        self.assertEqual(steps[i], ('after_entry', 0x06, 1, 10,
                                    ('poke', ((0x108106, gs.lands_marked(0, 4)),
                                              (0x10789E, b'\x78')))))
        self.assertEqual(gs.lands_marked(0, 4), b'\x84' * 7)      # 0x80 | side 0 | class 4
        self.assertEqual(steps[i + 1:], u10[k + 1:])              # the rest is u10's
        self.assertEqual(len(u10), 21)                            # u10 itself, not extended


class TestDiabloEnding(unittest.TestCase):
    """gp-u22-diablo-ending (endgame truth Task 7; record §E1.1/§E7): P1 confirms the
    character-select cell 6, which 0xC8882 = 00 02 03 01 06 04 05 (0x43D6E/0x43D74) maps
    to class 5 DIABLO, whose ending is DS_0010782A[DS_00104AD4 * 0x94] = 5 (title id
    0xE3, DIABLO'S HELL). ending_menu(6) is six p1.right presses from cell 0, so the walk
    is cell 6's own; the scene is otherwise Task 3's TALON arrangement."""

    def test_the_walk_is_six_rights_from_cell_zero(self):
        menu, steps = gs.WIN_MENU, gs.SCENARIOS['gp-u22-diablo-ending']['steps']
        self.assertEqual(steps[:len(menu) - 1], menu[:-1])        # the same menu path
        self.assertEqual(steps[3:10], (
            ('after_mode', 0x10, 60, ('pad', ('p1.right',), 4)),  # cell 0 -> 1
            ('after', 30, ('pad', ('p1.right',), 4)),             # 1 -> 2
            ('after', 30, ('pad', ('p1.right',), 4)),             # 2 -> 3
            ('after', 30, ('pad', ('p1.right',), 4)),             # 3 -> 4
            ('after', 30, ('pad', ('p1.right',), 4)),             # 4 -> 5
            ('after', 30, ('pad', ('p1.right',), 4)),             # 5 -> 6 (class 5)
            ('after', 30, ('pad', ('p1.start',), 6)),             # confirm cursor 6
        ))

    def test_the_scenario_marks_diablos_lands_and_clones_u10s_tail(self):
        scn = gs.SCENARIOS['gp-u22-diablo-ending']
        self.assertIn('gp-u22-diablo-ending', gs.STOP_AT_END)
        self.assertEqual((scn['time_limit'], scn['memsize'], scn['extra']),
                         (240, 64, gs.WIN_EXTRA))
        steps, u10 = scn['steps'], gs.SCENARIOS['gp-u10-ending']['steps']
        i = next(k for k, st in enumerate(steps) if st[0] == 'after_entry' and st[1] == 0x06)
        k = next(k for k, st in enumerate(u10) if st[0] == 'after_entry' and st[1] == 0x06)
        self.assertEqual(steps[i], ('after_entry', 0x06, 1, 10,
                                    ('poke', ((0x108106, gs.lands_marked(0, 5)),
                                              (0x10789E, b'\x78')))))
        self.assertEqual(gs.lands_marked(0, 5), b'\x85' * 7)      # 0x80 | side 0 | class 5
        self.assertEqual(steps[i + 1:], u10[k + 1:])              # the rest is u10's
        self.assertEqual(len(u10), 21)                            # u10 itself, not extended


class TestLiveWin(unittest.TestCase):
    """gp-u14-win-live (plan 2026-10-20-gameplay-truth-ii Task 6; spec W5): a match won
    by fought hits, without the WIN_MENU pokes. The hit sequence is the builder's; the
    160-attempt form's one authorized run (2026-10-26) measured P1 KO'd in both rounds
    (record 2026-10-26-fought-match-truth §F2), so the pins stay empty and these tests
    are shape-only."""

    def test_the_live_win_has_no_pokes(self):
        scn = gs.SCENARIOS['gp-u14-win-live']
        self.assertFalse(any(s[-1][0] == 'poke' for s in scn['steps'] if isinstance(s[-1], tuple)))
        self.assertEqual(scn['extra'], gs.WIN_EXTRA)
        self.assertEqual(scn['time_limit'], 260)
        self.assertEqual((scn['stop_mode'], scn['stop_delay']), (0x12, 0))
        self.assertEqual(scn['skip_on_mode'], (0x08, 0x09))

    def test_each_round_has_160_attempts_and_a_walk_every_eight(self):
        steps = gs.SCENARIOS['gp-u14-win-live']['steps']
        anchors = [st for st in steps if st[0] == 'after_entry']
        self.assertEqual([(a[1], a[2], a[3]) for a in anchors], [(0x06, 1, 10), (0x06, 2, 10)])
        for a in anchors:
            i = steps.index(a)
            j = next((k for k in range(i + 1, len(steps))
                      if steps[k][0] == 'after_entry'), len(steps))
            block = steps[i + 1:j]
            attempts = [a] + [st for st in block
                              if st[-1][1] in (gs.U14_PUNCH, gs.U14_KICK)]
            walks = [st for st in block if st[-1][1] == ('p1.right',)]
            self.assertEqual(len(attempts), 160)
            self.assertEqual([st[-1][1] for st in attempts[:17]],
                             [gs.U14_PUNCH, gs.U14_KICK] * 8 + [gs.U14_PUNCH])
            self.assertTrue(all(st[0] == 'after' and st[-1][2] == 4 for st in block
                                if st[-1][1] in (gs.U14_PUNCH, gs.U14_KICK)))
            self.assertEqual(len(walks), 19)          # at k = 8, 16, ..., 152
            self.assertTrue(all(st[-1][2] == 12 for st in walks))   # the hold, not the gap
        self.assertIn('gp-u14-win-live', gs.STOP_AT_END)


class TestTwopKo(unittest.TestCase):
    """gp-twop-ko (plan 2026-10-26-fought-match-truth Task 4; record §F4): a real
    two-human match (gp-twop's start, whose kept walk-in step carries P2's own
    fight press) with one hand-dealt KO of P2, and the result window after it. No
    pokes: gp_twop.ko is the judge, and a time-out is a failed task."""

    def test_the_two_human_ko_scenario_is_one_round_with_the_walk_in_press(self):
        scn = gs.SCENARIOS['gp-twop-ko']
        self.assertIn('gp-twop-ko', gs.STOP_AT_END)
        self.assertEqual((scn['time_limit'], scn['extra'], scn['stop_mode'], scn['stop_delay']),
                         (240, gs.WIN_EXTRA, (0x08, 0x09), 240))
        self.assertFalse(any(s[-1][0] == 'poke' for s in scn['steps'] if isinstance(s[-1], tuple)))
        steps = scn['steps']
        self.assertEqual(steps[:9], gs.steps_before_end(gs.SCENARIOS['gp-twop']['steps'])[:9])
        self.assertEqual(steps[8][-1], ('pad', ('p1.right', 'p2.left'), 30))   # the walk-in press
        block = steps[9:]
        attempts = [st for st in block if st[-1][1] in (gs.U14_PUNCH, gs.U14_KICK)]
        walks = [st for st in block if st[-1][1] == ('p1.right',)]
        self.assertEqual(len(attempts), 140)
        self.assertEqual(len(walks), 17)                 # at k = 8, 16, ..., 136
        self.assertEqual(block[0], ('after', 30, ('pad', gs.U14_PUNCH, 4)))
        self.assertTrue(all(st[0] == 'after' and st[-1][2] == 4 for st in attempts))
        # The spacing, not only the count (Task 2's review): the builder's 1A/1B
        # alternation, 16 frames between attempts (U16_GAP), a 12-frame hold on each
        # walk (U16_WALK) after every eighth attempt (U16_GROUP) and only there, with
        # the attempt after a walk 20 frames on (the walk's 12 + 8). The literals are
        # the builder's own values; the constant assertion keeps the two in step.
        self.assertEqual((gs.U16_GAP, gs.U16_WALK, gs.U16_GROUP), (16, 12, 8))
        self.assertEqual([st[-1][1] for st in attempts[:17]],
                         [gs.U14_PUNCH, gs.U14_KICK] * 8 + [gs.U14_PUNCH])
        seen = 0
        for i, st in enumerate(block):
            name = st[-1][1]
            if name in (gs.U14_PUNCH, gs.U14_KICK):
                seen += 1
                want = 30 if i == 0 else (20 if block[i - 1][-1][1] == ('p1.right',) else 16)
                self.assertEqual(st[1], want, 'attempt %d' % seen)
            else:
                self.assertEqual(name, ('p1.right',))
                self.assertEqual(st[1], 16)
                self.assertEqual(st[-1][2], 12)
                self.assertEqual(seen % 8, 0)                # after every eight attempts
                self.assertTrue(0 < seen < 140)              # never before the first or after the last
        self.assertEqual(seen, 140)


class TestHarnessSmoke(unittest.TestCase):
    """gp-harness-smoke (plan 2026-10-26-fought-match-truth Task 2): the disposable
    stop-on-mode preflight, not a measurement and not a gate-inventoried capture."""

    def test_the_smoke_is_a_throwaway_stop_on_mode_preflight(self):
        scn = gs.SCENARIOS['gp-harness-smoke']
        self.assertIn('gp-harness-smoke', gs.STOP_AT_END)
        self.assertFalse(any(s[-1][0] == 'poke' for s in scn['steps']))
        self.assertEqual(scn['steps'][-1][0], 'after_mode')
        self.assertEqual(scn['steps'][-1][-1][0], 'pad')


class TestMidMatchJoin(unittest.TestCase):
    """gp-u15-join (plan 2026-10-20-gameplay-truth-ii Task 7): P2 presses F2 in the
    gp-idle-loss fight. The join poll 0x28CC8 (mode 6's 0x2525F) accepts side 1's
    start mask, sets b1f and calls the credit spend whose 0x2CA93 skips the debit;
    0x28DA4 stores the join's mode 0x17."""

    def test_the_join_scenario_presses_f2_in_the_fight(self):
        scn = gs.SCENARIOS['gp-u15-join']
        self.assertTrue(any(s[0] == 'after_mode' and s[1] == 0x06 and 'p2.start' in s[3][1]
                            for s in scn['steps']))

    def test_the_menu_path_ends_sixty_frames_after_the_join(self):
        steps = gs.SCENARIOS['gp-u15-join']['steps']
        i = next(k for k, s in enumerate(steps) if s[0] == 'after_mode' and s[1] == 0x06)
        self.assertEqual(steps[i][:3], ('after_mode', 0x06, 60))
        self.assertEqual(steps[i][3], ('pad', ('p2.start',), 5))
        self.assertEqual(steps[i + 1], ('after', 60, ('end',)))
        self.assertEqual(steps[:i], gs.SCENARIOS['gp-idle-loss']['steps'][:3])
        self.assertIn('gp-u15-join', gs.STOP_AT_END)


def real_image():
    """The fixed-up image build/diffrun dumps, or None (skip; test_gp_moves' fixture)."""
    diffrun = os.path.join(ROOT, 'build', 'diffrun')
    exe = os.path.join(ROOT, 'data', 'game', 'C', 'PRAGE.EXE')
    if not (os.path.exists(diffrun) and os.path.exists(exe)):
        return None
    fd, path = tempfile.mkstemp(suffix='.bin')
    os.close(fd)
    try:
        subprocess.run([diffrun, '--exe', exe, '--image-out', path], check=True, capture_output=True)
        return gm.Image.load(path)
    finally:
        os.remove(path)


class TestExtendedTwop(unittest.TestCase):
    """gp-u16-twop2 (plan 2026-10-20-gameplay-truth-ii Task 8; design W7): gp-twop's
    two-human start, extended past the first match's script end to the match result
    (mode 9; mode 8 is the undecided round's result), with the three finisher starters
    (0x37640/0x37774/0x37898, reactions 0x32..0x34) attempted on the defeated P2. The
    press sequence is the builder's and UNMEASURED (Task 12's capture measures it)."""

    def test_the_extended_twop_scenario_runs_past_the_first_match(self):
        scn = gs.SCENARIOS['gp-u16-twop2']
        self.assertGreater(scn['time_limit'], gs.SCENARIOS['gp-twop']['time_limit'])
        self.assertTrue(any(s[0] == 'until_mode' and s[1] in (0x09, 0x12) for s in scn['steps']))

    def test_the_two_human_start_is_preserved(self):
        # The join (F2 in the character select, 0x11F28(1)), both confirms (0x43CAD) and
        # both sides' mode-6 presses are gp-twop's own steps, in order: the whole prefix
        # before gp-twop's end, whose boundary is computed rather than an index that a
        # future gp-twop step could shift.
        steps = gs.SCENARIOS['gp-u16-twop2']['steps']
        prefix = gs.steps_before_end(gs.SCENARIOS['gp-twop']['steps'])
        self.assertEqual(steps[:len(prefix)], prefix)
        self.assertEqual(steps[len(prefix) - 2:len(prefix)],
                         (('after', 60, ('pad', ('p1.b1', 'p2.b2'), 5)),
                          ('after', 30, ('pad', ('p1.b2', 'p2.b1'), 5))))
        self.assertIn('gp-u16-twop2', gs.STOP_AT_END)

    def test_the_finisher_entries_are_pressed_on_the_match_result(self):
        # gp_moves.py steps --char 2 --facing 0 --moves 15,16,17 --gap 100 --step 4:
        # TALON's three finisher entries (0x15/0x16/0x17 -> reactions 0x32/0x33/0x34,
        # the starters 0x37640/0x37774/0x37898), each attempt's b0+b2 held 16, 100
        # frames apart. (0x17..0x19 are characters 0/1/3/4/5/6's entries; for char 2
        # 0x17 is the reaction-0x34 starter and 0x18/0x19 are reactions 0x24/0x25.)
        steps = gs.SCENARIOS['gp-u16-twop2']['steps']
        i = next(k for k, s in enumerate(steps) if s[0] == 'after_entry' and s[1] == 0x09)
        self.assertEqual(steps[i][:4], ('after_entry', 0x09, 1, 10))   # the match result
        fin = steps[i:-1]
        self.assertEqual([s[-1][2] for s in fin if s[-1][1] == ('p1.b0', 'p1.b2')], [16, 16, 16])
        self.assertEqual(len(fin), 15)                                 # the anchor + 14 steps
        self.assertEqual(steps[-1], ('until_mode', 0x12, 0))          # after the presses

    def test_the_finisher_steps_are_gp_moves_output_for_entries_15_16_17(self):
        # TestU6Moves' regeneration (test_gp_moves.TestRealImage): the constant must be
        # the tool's own output for TALON's entries, so a wrong --moves pairing fails.
        img = real_image()
        if img is None:
            self.skipTest('no build/diffrun or data/game/C/PRAGE.EXE')
        att = gm.plan(gm.decode(img, 2), [0x15, 0x16, 0x17], 0, 100, 4)
        first, steps = gm.scenario_steps(att)
        self.assertEqual(first, 0)
        self.assertEqual(gs.U16_FINISHER, tuple(steps))


class TestU23Finisher(unittest.TestCase):
    """gp-u23-u16-finisher (plan 2026-10-23-endgame-truth Task 2; record §E2): the
    U16 finisher reaction captured live. gp-twop's start and a 32-attempt first
    round are the same as gp-u16-twop2; the second round is the plan's labelled
    fallback (P2's damage byte poked to 0x77, then 12 real 1A/1B hits, one of
    which KOs), then TALON's three finisher entries on the match result. The
    reaction is the claim: gp_twop.finisher runs without its allowance."""

    def test_the_scenario_stops_at_the_conquered_lands_screen(self):
        scn = gs.SCENARIOS['gp-u23-u16-finisher']
        self.assertIn('gp-u23-u16-finisher', gs.STOP_AT_END)
        self.assertEqual(scn['time_limit'], 300)
        self.assertEqual(scn['steps'][-1], ('until_mode', 0x12, 0))

    def test_the_second_round_is_the_labelled_poked_damage_fallback(self):
        steps = gs.SCENARIOS['gp-u23-u16-finisher']['steps']
        prefix = gs.steps_before_end(gs.SCENARIOS['gp-twop']['steps'])
        self.assertEqual(steps[:len(prefix)], prefix)
        i06 = next(k for k, s in enumerate(steps)
                   if s[0] == 'after_entry' and s[1] == 0x06 and s[2] == 2)
        self.assertEqual(steps[i06][:4], ('after_entry', 0x06, 2, 10))
        self.assertEqual(steps[i06][-1], gs.U23_POKED_DAMAGE)
        self.assertEqual(gs.U23_POKED_DAMAGE, ('poke', ((0x10789E, b'\x77'),)))
        self.assertEqual(steps[i06 + 1], ('after', 8, ('pad', gs.U14_PUNCH, 4)))  # a real hit
        # round 1 stays gp-u16-twop2's 32 attempts; round 2 has 12 real hits after the
        # poke (record §E2: the published capture's mode-6 entry is f=0x523 and its
        # first s1_5a change f=0x5A4, 0x81 into the round).
        self.assertEqual(len([s for s in steps[len(prefix):i06]
                              if isinstance(s[-1], tuple) and s[-1][0] == 'pad'
                              and s[-1][1] in (gs.U14_PUNCH, gs.U14_KICK)]), 32)
        i09 = next(k for k, s in enumerate(steps) if s[0] == 'after_entry' and s[1] == 0x09)
        self.assertEqual(len([s for s in steps[i06 + 1:i09]
                              if isinstance(s[-1], tuple) and s[-1][0] == 'pad'
                              and s[-1][1] in (gs.U14_PUNCH, gs.U14_KICK)]), 12)
        self.assertEqual(steps[i09][:4], ('after_entry', 0x09, 1, 100))

    def test_the_finisher_entries_follow_the_match_result(self):
        steps = gs.SCENARIOS['gp-u23-u16-finisher']['steps']
        i = next(k for k, s in enumerate(steps) if s[0] == 'after_entry' and s[1] == 0x09)
        want = (('after_entry', 0x09, 1, 100, gs.U16_FINISHER[0][-1]),) + gs.U16_FINISHER[1:]
        self.assertEqual(steps[i:-1], want)
        self.assertEqual(len(steps[i:-1]), 15)                         # the anchor + 14 steps
        self.assertEqual([s[-1][2] for s in steps[i:-1] if s[-1][1] == ('p1.b0', 'p1.b2')],
                         [16, 16, 16])


class TestPokeScript(unittest.TestCase):
    def setUp(self):
        gs.SCENARIOS['_t'] = dict(time_limit=1, steps=())

    def tearDown(self):
        gs.SCENARIOS.pop('_t', None)

    W = 'W ms=4 f=0128 step=3 addr=0010789E len=1 was=00 now=78 late=0 race=0'

    def test_a_poke_is_replayed_at_the_next_frame(self):
        text = gs.port_script('_t', _log(extra_after=[self.W]))
        self.assertIn('poke 297 0010789E 78', text.splitlines())        # 0x128 + 1
        self.assertEqual(text.splitlines()[-1], 'end 304')

    def test_a_raced_poke_is_refused(self):
        with self.assertRaisesRegex(gs.ScriptError, 'raced'):
            gs.port_script('_t', _log(extra_after=[self.W.replace('race=0', 'race=1')]))

    def test_a_poke_without_its_snapshot_is_refused(self):
        with self.assertRaisesRegex(gs.ScriptError, 'unpinned'):
            gs.port_script('_t', _log(extra_after=[self.W.replace('f=0128', 'f=0140')]))

    def test_end_cuts_the_pokes(self):
        text = gs.port_script('_t', _log(extra_after=[self.W]), end=0x128)
        self.assertNotIn('poke', text)


if __name__ == '__main__':
    unittest.main()
