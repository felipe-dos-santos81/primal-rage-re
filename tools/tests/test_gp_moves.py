# tools/tests/test_gp_moves.py (plan gameplay-u6b, record gameplay-u6 §U6.10-§U6.12)
import os, struct, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_moves as gm
import gp_session as gs

# Character 0's entry 0x00 as the image holds it (record §U6.10): reaction 0x20,
# d 1, e 0, the callback 0x3D17C, five phases.
PH20 = [(0x80000, 0x4000, 0, 3), (0x80000, 0x4000, 0, 3), (0x20000, 0x4000, 0xC000, 15),
        (0x10500, 0x4A00, 0xC000, 15), (0, 0, 0, 10)]
DESC = 0x0D0000


def synthetic():
    """An image from 0x10000 holding one keyboard entry (character 0, i 0) and
    its CPU-table reaction/stance bytes and move-table callback."""
    data = bytearray(0x0D1000 - gm.IMAGE_BASE)
    def d(a, v): struct.pack_into('<I', data, a - gm.IMAGE_BASE, v)
    def w(a, v): struct.pack_into('<H', data, a - gm.IMAGE_BASE, v)
    d(gm.KB_TABLE, DESC)
    w(gm.CPU_TABLE + 4, 0x20)
    data[gm.CPU_TABLE + 6 - gm.IMAGE_BASE] = 1
    d(gm.MOVE_TABLE + 0x20 * 20, 0x3D17C)
    for k, (m0, m1, m2, c) in enumerate(PH20):
        a = DESC + k * gm.PHASE
        d(a, m0); d(a + 4, m1); d(a + 8, m2); w(a + 0xC, c)
    return gm.Image(bytes(data))


def snap(f, **kw):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, **kw)
    return gs.parse(gs.format_s(0, vals, 0, 0, 0))


class TestDecode(unittest.TestCase):
    def test_decode_reads_the_keyboard_table_and_the_cpu_reaction(self):
        t = gm.decode(synthetic(), 0)
        self.assertEqual(sorted(t), [0])
        r, d, e, cb, ph = t[0]
        self.assertEqual((r, d, e, cb), (0x20, 1, 0, 0x3D17C))
        self.assertEqual(ph, PH20)               # stops at the first m0 == 0

    def test_absolute_folds_the_facing_bits(self):
        self.assertEqual(gm.absolute(0x80000, 0), 0x2000)    # held back = held left
        self.assertEqual(gm.absolute(0x80000, 1), 0x1000)
        self.assertEqual(gm.absolute(0x10500, 0), 0x0510)    # new forward = new right
        self.assertEqual(gm.absolute(0x10500, 1), 0x0520)
        self.assertEqual(gm.absolute(0x40000, 0), 0x0020)
        self.assertEqual(gm.absolute(0x20000, 0), 0x1000)

    def test_tokens_split_held_and_new(self):
        self.assertEqual(gm.tokens(0x0510), (['p1.b0', 'p1.b2'], ['p1.right']))
        self.assertEqual(gm.tokens(0x0303), (['p1.b0', 'p1.b1'], ['p1.b0', 'p1.b1']))


class TestPresses(unittest.TestCase):
    def test_the_0x20_motion(self):
        # buttons held from 0; back, forward, forward re-pressed after a 2-frame release
        self.assertEqual(gm.presses(PH20, 0, 4),
                         [(0, 'p1.b0', 12), (0, 'p1.b2', 12), (0, 'p1.left', 4),
                          (4, 'p1.right', 2), (8, 'p1.right', 4)])

    def test_a_repress_needs_the_debounce_gap(self):
        with self.assertRaises(ValueError):
            gm.presses(PH20, 0, 2)

    def test_a_chord_is_one_phase(self):
        self.assertEqual(gm.presses([(0x3, 0xC00, 0, 3), (0, 0, 0, 5)], 0, 4),
                         [(0, 'p1.b0', 4), (0, 'p1.b1', 4)])

    def test_plan_and_steps(self):
        t = {0: (0x20, 1, 0, 0x3D17C, PH20), 0x1A: (0x10, 1, 1, 0, [(0x3, 0xC00, 0, 3), (0, 0, 0, 5)])}
        att = gm.plan(t, [0x1A, 0], 0, 100, 4)
        self.assertEqual([(o, i) for o, i, _ in att], [(0, 0x1A), (100, 0)])
        first, steps = gm.scenario_steps(att)
        self.assertEqual(first, 0)
        self.assertEqual(steps, [('after', 0, ('pad', ('p1.b0', 'p1.b1'), 4)),
                                 ('after', 100, ('pad', ('p1.left',), 4)),
                                 ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
                                 ('after', 4, ('pad', ('p1.right',), 2)),
                                 ('after', 4, ('pad', ('p1.right',), 4))])

    def test_kb_levels(self):
        self.assertEqual(gm.kb_levels([(0, 'p1.b0', 4), (2, 'p1.left', 4)], 100),
                         [(100, 0x0100), (102, 0x2100), (104, 0x2000), (106, 0)])


class TestScript(unittest.TestCase):
    TEXT = ('# gp port script v2: scenario x (cut at 50)\nenter_frame 10\nenter_state 0000\n'
            'key 10 1C 0D\nbits 30 0100\nend 50\n')

    def test_merge_orders_by_frame_keys_first(self):
        out = gm.merge_script(self.TEXT, [(10, 0x2000), (20, 0)])
        self.assertEqual(out.splitlines()[3:], ['key 10 1C 0D', 'bits 10 2000', 'bits 20 0000',
                                                'bits 30 0100', 'end 50'])

    def test_merge_refuses_bits_past_the_end(self):
        with self.assertRaises(ValueError):
            gm.merge_script(self.TEXT, [(51, 0)])


class TestCheck(unittest.TestCase):
    T = {0: (0x20, 1, 0, 0x3D17C, PH20), 0x1A: (0x10, 1, 1, 0, [(0x3, 0xC00, 0, 3), (0, 0, 0, 5)])}

    def test_performed_not_shown_and_no_snapshot(self):
        att = gm.plan(self.T, [0x1A, 0, 0x1A], 0, 10, 4)
        snaps = {f: snap(f, r0=0xFF) for f in range(100, 120)}
        snaps[103] = snap(103, r0=0x12)          # a 0x10 variant (0x3CBC4)
        snaps[104] = snap(104, r0=0x12)
        res = gm.check(snaps, 100, att, self.T, 10)
        self.assertEqual([v for _, _, v, _ in res], ['performed', 'not shown', 'no-snapshot'])
        self.assertEqual(res[0][3], 103)

    def test_a_reaction_already_shown_is_not_a_new_attempt(self):
        att = gm.plan(self.T, [0], 0, 10, 4)
        snaps = {f: snap(f, r0=0x20) for f in range(99, 110)}
        self.assertEqual(gm.check(snaps, 100, att, self.T, 10)[0][2], 'not shown')

    def test_a_block_is_the_slot_bits(self):
        att = gm.plan(self.T, [gm.BLOCK], 0, 10, 4)
        self.assertEqual(att[0][2], [(0, 'p1.left', 6)])
        snaps = {f: snap(f, s0_43=0x80) for f in range(100, 110)}
        self.assertEqual(gm.check(snaps, 100, att, self.T, 10)[0][2], 'not shown')
        snaps[105] = snap(105, s0_43=0xA0)
        self.assertEqual(gm.check(snaps, 100, att, self.T, 10)[0][2:], ('performed', 105))

    def test_a_block_already_held_is_not_a_new_attempt(self):
        # review of U6b Task 3: 0x1A6AC leaves +0x43 bit 0x20/0x10 set while blocking
        att = gm.plan(self.T, [gm.BLOCK], 0, 10, 4)
        snaps = {f: snap(f, s0_43=0x20) for f in range(99, 110)}
        self.assertEqual(gm.check(snaps, 100, att, self.T, 10)[0][2], 'not shown')

    def test_only_the_block_bits_count(self):
        att = gm.plan(self.T, [gm.BLOCK], 0, 10, 4)
        snaps = {f: snap(f, s0_43=0xCF) for f in range(99, 110)}
        snaps[105] = snap(105, s0_43=0xCF | 0x04)
        self.assertEqual(gm.check(snaps, 100, att, self.T, 10)[0][2], 'not shown')

    def test_ff_reaction_is_never_performed(self):
        # 0xFF = no reaction since the reset at 0x4952A: r0 0xFF -> 0xFF shows nothing
        att = gm.plan(self.T, [0, 0x1A], 0, 10, 4)
        snaps = {f: snap(f, r0=0xFF) for f in range(99, 125)}
        self.assertEqual([v for _, _, v, _ in gm.check(snaps, 100, att, self.T, 10)],
                         ['not shown', 'not shown'])


class TestCli(unittest.TestCase):
    def test_check_on_a_capture_without_the_move_fields_fails_cleanly(self):
        # a capture made before U6b Task 1: S lines without r0/s0_43 (no KeyError)
        old = [(n, d, z) for n, d, z in gs.SNAP_FIELDS if n not in gs.MOVE_FIELDS]
        body = ' '.join('%s=%0*X' % (n, 2 * z, 1) for n, _, z in old)
        lines = ['I ms=1 f=0100 step=0 press=p1.b0 scan=16 lin=0 old=96 bios=1675 ring=1 late=0',
                 'S ms=2 %s kb=0000 head=001E tail=001E' % body]
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, 'poll.log'), 'w') as f:
                f.write('\n'.join(lines) + '\n')
            img = os.path.join(d, 'image.bin')
            with open(img, 'wb') as f:
                f.write(synthetic().data)
            r = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'gp_moves.py'), 'check',
                                '--image', img, '--char', '0', '--moves', '00', '--gap', '10',
                                '--step', '4', '--capture', d], capture_output=True, text=True)
        self.assertEqual(r.returncode, 1, r.stderr)
        self.assertIn('records no move fields', r.stdout)
        self.assertEqual(r.stderr, '')


class TestFirstPress(unittest.TestCase):
    def test_the_first_p1_press_names_the_attempt_frame(self):
        lines = ['I ms=1 f=0140 step=0 press=enter scan=1C lin=00010070 old=9C bios=1C0D ring=1 late=0',
                 'I ms=2 f=07FE step=3 press=p1.b0 scan=16 lin=0001006A old=96 bios=1675 ring=1 late=0',
                 'I ms=3 f=0862 step=4 press=p1.left scan=2C lin=00010080 old=AC bios=2C7A ring=1 late=0']
        self.assertEqual(gm.first_press(lines), 0x7FF)
        self.assertIsNone(gm.first_press(lines[:1]))


def real_image():
    """The fixed-up image dumped by build/diffrun, or None (skip)."""
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


class TestRealImage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.img = real_image()

    def setUp(self):
        if self.img is None:
            self.skipTest('no build/diffrun or data/game/C/PRAGE.EXE')

    def test_character_0_entries(self):
        t = gm.decode(self.img, 0)
        self.assertEqual({i: t[i][0] for i in (0, 1, 6, 0x15, 0x1A, 0x1B)},
                         {0: 0x20, 1: 0x24, 6: 0x2D, 0x15: 0x3D, 0x1A: 0x10, 0x1B: 0x11})
        self.assertEqual(t[0][4], PH20)
        self.assertEqual({i: t[i][3] for i in (0, 1, 6, 0x15)},
                         {0: 0x3D17C, 1: 0x3F0A8, 6: 0x3D1EC, 0x15: 0x3C048})

    def test_no_keyboard_reaction_is_ff_or_beyond_the_move_table(self):
        # r0 resets to 0xFF (0x4952A): 'no reaction since the reset' must not be an expected value
        for c in range(7):
            for i, ent in gm.decode(self.img, c).items():
                self.assertLess(ent[0], 0x40, (c, i))
                self.assertTrue(set(gm.expected({i: ent}, i)).isdisjoint({0xFF}), (c, i))


if __name__ == '__main__':
    unittest.main()
