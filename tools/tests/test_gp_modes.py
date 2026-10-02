# tools/tests/test_gp_modes.py
import os, struct, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
import gp_modes as gm

EXE = os.path.join(ROOT, 'data', 'game', 'C', 'PRAGE.EXE')
IDLE = os.path.join(ROOT, 'data', 'k11-captures', 'gp-idle-loss')
CODE_FILE_OFF = 0x52E54          # obj-0 file offset = VA + 0x52E54 (AGENTS.md); immediates need no fixup


def _s(f, mode, cred=5, b1d=0, b1f=0, ent=0, raw=0, st=0):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, mode=mode, cred=cred, b1d=b1d, b1f=b1f, ent=ent, raw=raw, st=st, t508=1, t50c=2)
    return gs.format_s(0, vals, gs.raw_to_kb(raw), 0x1E, 0x30)


OFF = 0x266000 - 0x80000        # a capture's base minus DATA_BASE_VA (the B record below)


def _row_log(mode, b1d, b1f, spend, reach=6, stay=False):
    """A minimal poll.log of a START MENU row: MAIN MENU, START MENU open, the
    row's mode (P only, as gp-idle-loss's 0x2D at poll.log:631), the divert, the
    character select, `reach`, the X record."""
    L = ['B ms=0 base=00266000 ptr=0000FE20']
    L += [_s(f, 3) for f in range(0x100, 0x110)]
    L += [_s(f, 0x27, ent=0xBCBEC + OFF) for f in range(0x110, 0x120)]
    L += [_s(f, 0x27, ent=gm.START_ENTRY + OFF) for f in range(0x120, 0x130)]
    L.append('P ms=1 f=0130 mode=%04X st=0000 tick=00000000' % mode)
    L += [_s(f, 0x1A, cred=5 - spend, b1d=b1d, b1f=b1f) for f in range(0x131, 0x140)]
    L += [_s(f, 0x10, cred=5 - spend, b1d=b1d, b1f=b1f) for f in range(0x140, 0x150)]
    if not stay:
        L += [_s(f, reach, cred=5 - spend, b1d=b1d, b1f=b1f) for f in range(0x150, 0x160)]
    L.append('X ms=2 f=015F step=4 end')
    return L


def _attract_log():
    L = ['B ms=0 base=00266000 ptr=0000FE20']
    L += [_s(f, 3) for f in range(0x100, 0x110)]
    L += [_s(0x110, 3, raw=0x01000000), _s(0x111, 3, raw=0x01000000)]
    L.append('P ms=1 f=0112 mode=001A st=0000 tick=00000000')
    L += [_s(f, 0x1A, cred=4, b1f=1) for f in range(0x112, 0x120)]
    L += [_s(f, 0x10, cred=4, b1f=1) for f in range(0x120, 0x130)]
    L += [_s(f, 6, cred=4, b1f=1) for f in range(0x130, 0x140)]
    L.append('X ms=2 f=013F step=1 end')
    return L


def _failed(res):
    return [label for label, ok, _ in res if not ok]


class TestRowsFromTheRaw(unittest.TestCase):
    @unittest.skipUnless(os.path.isfile(EXE), 'needs data/game/C/PRAGE.EXE')
    def test_setters_and_divert_arguments(self):
        with open(EXE, 'rb') as f:
            img = f.read()

        def at(va, n):
            return img[va + CODE_FILE_OFF:va + CODE_FILE_OFF + n]
        # 0x2CBC4 + 0x18 k: push edx; mov edx,M (ba M 0 0 0); xor ah,ah (30 e4) | mov ah,N (b4 N)
        setter = {}
        for k in range(7):
            b = at(0x2CBC4 + 0x18 * k, 8)
            self.assertEqual((b[0], b[1]), (0x52, 0xBA))
            setter[b[2]] = 0 if b[6:8] == b'\x30\xe4' else b[7]
        # the divert argument per mode: the `mov <reg>, N` that feeds 0x257A4 (record §U8.2)
        divert = {0x28: (0x24F51, 0xB8), 0x29: (0x24FAF, 0xB8), 0x2A: (0x25002, 0xBB), 0x2B: (0x25187, 0xBA),
                  0x2C: (0x2505C, 0xB8), 0x2D: (0x250BF, 0xB8), 0x2E: (0x2511C, 0xB8)}
        spend = set()
        for va in range(0x24F09, 0x2519E):          # the handlers 0x28..0x2F: calls to 0x2CA7C
            b = at(va, 5)
            if b[0] == 0xE8 and va + 5 + struct.unpack('<i', b[1:])[0] == 0x2CA7C:
                spend.add(va)
        self.assertEqual(spend, {0x250BA, 0x25117, 0x25173})   # modes 0x2D, 0x2E and 0x2F
        for name, w in gm.ROWS.items():
            if w['mode'] is None:
                continue
            self.assertEqual(setter[w['mode']], w['b1d'], name)
            va, op = divert[w['mode']]
            self.assertEqual(at(va, 2), bytes((op, w['b1f'])), name)
            self.assertEqual(w['spend'], int(w['mode'] in (0x2D, 0x2E)), name)


class TestCheck(unittest.TestCase):
    def test_each_row_passes_on_its_own_log(self):
        for name, w in gm.ROWS.items():
            if w['mode'] is None:
                continue
            L = _row_log(w['mode'], w['b1d'], w['b1f'], w['spend'], w['reach'], w['stay'])
            self.assertEqual(_failed(gm.check(name, L)), [], name)

    def test_a_wrong_row_fails(self):
        L = _row_log(0x29, 1, 3, 0)                  # RIGHT PLAYER TRAINING's log
        self.assertIn('the row mode 0x28 appears', _failed(gm.check('gp-u8-left-training', L)))
        L = _row_log(0x2A, 2, 3, 0)
        bad = _failed(gm.check('gp-u8-handicap', L))
        self.assertIn('no other game-start mode', bad)

    def test_divert_and_credits_are_checked(self):
        self.assertIn('the divert: b1d=2 b1f=3', _failed(gm.check('gp-u8-tug-of-war', _row_log(0x2A, 2, 1, 0))))
        self.assertIn('credits spent 1', _failed(gm.check('gp-u8-right-arcade', _row_log(0x2E, 0, 2, 0))))
        self.assertIn('credits spent 0', _failed(gm.check('gp-u8-handicap', _row_log(0x2C, 4, 3, 1))))

    def test_start_menu_must_be_open(self):
        L = [l.replace('ent=%08X' % (gm.START_ENTRY + OFF), 'ent=%08X' % (0xBCBEC + OFF)) for l in _row_log(0x2E, 0, 2, 1)]
        self.assertIn('START MENU open (ent 0xBCCDC) before the select', _failed(gm.check('gp-u8-right-arcade', L)))

    def test_reach_and_end(self):
        L = [l for l in _row_log(0x2A, 2, 3, 0) if not (l.startswith('S ') and gs.parse(l)['mode'] == 6)]
        self.assertIn('mode 0x6 reached', _failed(gm.check('gp-u8-tug-of-war', L)))
        L = [l for l in _row_log(0x2A, 2, 3, 0) if not l.startswith('X ')]
        self.assertIn('the scenario end (X record)', _failed(gm.check('gp-u8-tug-of-war', L)))

    def test_endurance_must_stay_in_the_team_select(self):
        L = _row_log(0x2B, 3, 3, 0, reach=0x10, stay=True)
        self.assertEqual(_failed(gm.check('gp-u8-endurance', L)), [])
        L2 = L[:-1] + [_s(0x15E, 0x1A, b1d=3, b1f=3), 'X ms=2 f=015F step=4 end']
        self.assertIn('still in mode 0x10 at the end', _failed(gm.check('gp-u8-endurance', L2)))

    def test_attract_start(self):
        self.assertEqual(_failed(gm.check('gp-u8-attract-start', _attract_log())), [])
        L = [l.replace('raw=01000000', 'raw=00000000') for l in _attract_log()]
        self.assertIn('the P1 start bit reached the bitmap in mode 3', _failed(gm.check('gp-u8-attract-start', L)))
        L = _attract_log()
        L.insert(5, _s(0x104, 0x27))
        self.assertIn('no mode 0x27 (no Enter)', _failed(gm.check('gp-u8-attract-start', L)))

    @unittest.skipUnless(os.path.isfile(os.path.join(IDLE, 'poll.log')), 'needs data/k11-captures/gp-idle-loss')
    def test_the_u4_capture_is_row_0(self):
        with open(os.path.join(IDLE, 'poll.log')) as f:
            L = f.read().splitlines()
        self.assertEqual(_failed(gm.check('gp-idle-loss', L)), [])
        self.assertIn('the row mode 0x2E appears', _failed(gm.check('gp-u8-right-arcade', L)))


class TestScenarios(unittest.TestCase):
    def test_every_row_has_a_scenario_and_the_walk_is_derived(self):
        # START MENU opens on row 0 (spec §3.3); row k is k p1.down, row 6 one p1.up (the wrap)
        for name, w in gm.ROWS.items():
            if name == 'gp-idle-loss' or w['mode'] is None:
                continue
            steps = gs.SCENARIOS[name]['steps']
            acts = [st[-1] for st in steps if isinstance(st[-1], tuple)]
            want = [('pad', ('p1.up',), 4)] if w['row'] == 6 else [('pad', ('p1.down',), 4)] * w['row']
            self.assertEqual([a for a in acts if a[0] == 'pad'], want, name)
            self.assertEqual([a for a in acts if a[0] == 'key'], [('key', 'enter')] * 3, name)
            self.assertEqual(steps[-1], ('until_mode', w['reach'], 300), name)

    def test_row_schedule_fires_the_walk(self):
        s = gs.Schedule(gs.SCENARIOS['gp-u8-tug-of-war']['steps'])
        self.assertEqual(s.due_boot(gs.ENTER_WAIT), [(0, ('key', 'enter'))])
        s.on_mode(0x141, 0x27)
        self.assertEqual(s.due(0x141 + 149), [(1, ('key', 'enter'))])
        got = [s.due(0x141 + 149 + 60 * k) for k in range(1, 6)]
        self.assertEqual(got, [[(k + 1, ('pad', ('p1.down',), 4))] for k in range(1, 5)] + [[(6, ('key', 'enter'))]])
        s.on_mode(0x303, 0x2A)
        s.on_mode(0x328, 0x10)
        self.assertIsNone(s.end_frame)
        s.on_mode(0x875, 6)
        self.assertEqual((s.end_frame, s.fired, s.total), (0x875 + 300, 7, 7))


def _arm_log():
    """The attract start: mode 3, F1 pressed (boot step, its BIOS word 3B00),
    sampled from f 0x110 (raw bit 24), the word consumed at 0x112, the wipe."""
    L = ['B ms=0 base=00266000 ptr=0000FE20']
    L += [_s(f, 3, st=4) for f in range(0x100, 0x110)]
    L.append('I ms=1 f=010F step=0 press=p1.start scan=3B lin=0001008F old=FF bios=3B00 ring=1 late=0')
    L += [_s(0x110, 3, st=4, raw=0x01000000), _s(0x111, 3, st=4, raw=0x01000000)]
    L.append('H ms=2 f=0112 head=0020')
    L.append('P ms=2 f=0112 mode=001A st=0004 tick=00000000')
    L += [_s(0x112, 0x1A, cred=4, b1f=1, raw=0x01000000), _s(0x113, 0x1A, cred=4, b1f=1, raw=0x01000000)]
    L = [l.replace('head=001E', 'head=0020') if l.startswith('S ') and gs.parse(l)['f'] >= 0x112 else l for l in L]
    L.append('I ms=3 f=0113 step=0 release=p1.start lin=0001008F')
    L += [_s(f, 0x1A, cred=4, b1f=1).replace('head=001E', 'head=0020') for f in range(0x114, 0x118)]
    L.append('X ms=4 f=0117 step=1 end')
    return L


class TestPadArm(unittest.TestCase):
    def test_the_attract_start_scenario(self):
        sc = gs.SCENARIOS['gp-u8-attract-start']
        self.assertEqual(sc.get('arm'), 'pad')
        self.assertEqual(sc['steps'], (('boot', gs.ENTER_WAIT, ('pad', ('p1.start',), 4)), ('until_mode', 6, 300)))
        self.assertEqual(gm.ROWS['gp-u8-attract-start']['reach'], 6)

    def test_the_script_arms_on_the_press(self):
        text = gs.port_script('gp-u8-attract-start', _arm_log())
        self.assertEqual(text.splitlines()[1:], ['arm pad', 'enter_frame 272', 'enter_state 0004',
                                                 'bits 272 0100', 'key 274 3B 00', 'bits 276 0000', 'end 279'])

    def test_the_arm_must_be_pinned_in_mode_3(self):
        L = [l for l in _arm_log() if not (l.startswith('S ') and gs.parse(l)['f'] == 0x10F)]
        with self.assertRaises(gs.ScriptError):
            gs.port_script('gp-u8-attract-start', L)
        L = [l.replace('raw=01000000', 'raw=00000000') for l in _arm_log()]
        with self.assertRaises(gs.ScriptError):
            gs.port_script('gp-u8-attract-start', L)
        L = [l.replace('mode=0003', 'mode=0027') if l.startswith('S ') and gs.parse(l)['f'] in (0x10F, 0x110) else l
             for l in _arm_log()]                       # 0x11D04 runs in mode 3 only (0x25238)
        with self.assertRaises(gs.ScriptError):
            gs.port_script('gp-u8-attract-start', L)

    def test_an_enter_scenario_still_needs_mode_0x27(self):
        with self.assertRaises(gs.ScriptError):
            gs.port_script('gp-u8-right-arcade', _arm_log())

    def test_the_capture_check_follows_the_arm(self):
        import gp_capture as gc
        s = gs.Schedule(())
        res = dict((n.split(' (')[0], ok) for n, ok in gc.run_checks('gp-u8-attract-start', _arm_log(), s, 3, 3))
        self.assertTrue(res['mode left 3 after the pad arm'])
        self.assertTrue(res['port script v2'])
        bad = [l for l in _arm_log() if not (l.startswith('P ') or (l.startswith('S ') and gs.parse(l)['mode'] == 0x1A))]
        res = dict((n.split(' (')[0], ok) for n, ok in gc.run_checks('gp-u8-attract-start', bad, s, 3, 3))
        self.assertFalse(res['mode left 3 after the pad arm'])


if __name__ == '__main__':
    unittest.main()
