# tools/tests/test_k11_capture.py
import os, sys, tempfile, unittest
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import k11_capture as kc
import k11_fields as kf
import k11_session as ks


def _p(ms, tick, f=0x100, mode=3, kb=0):
    vals = {n: 0 for n, _, _ in ks.POLL_FIELDS}
    vals.update(f=f, tick=tick, mode=mode)
    return ks.format_p(ms, vals, kb, 0x1E)


def _k(ms, tick, word=0x1C0D):
    return 'K ms=%d tick=%08X f=0100 key=%04X' % (ms, tick, word)


class Capture(unittest.TestCase):
    def test_guard_refuses_everything_but_a_capture_subdir(self):
        for bad in ('/tmp/x', os.path.join(ROOT, 'data', 'game', 'C', 'X'),
                    os.path.join(ROOT, 'data', 'k11-captures'),
                    os.path.join(ROOT, 'data', 'title-captures', 'k11')):
            with self.assertRaises(SystemExit, msg=bad):
                kc.guard_out(bad)
        ok = os.path.join(ROOT, 'data', 'k11-captures', 'walk')
        self.assertEqual(kc.guard_out(ok), os.path.realpath(ok))

    def test_find_base_uses_the_anchor_and_the_check_words(self):
        buf = bytearray(0x200000)
        buf[0x40000:0x40008] = b'RAGE.S16'                      # a decoy without check words
        base = 0x100000
        buf[base + 0x2D:base + 0x35] = b'RAGE.S16'              # data VA 0x8002D
        buf[base + 0x1AFD8:base + 0x1AFE0] = bytes.fromhex('1400040080110032')   # data VA 0x9AFD8
        self.assertEqual(kc.find_base(buf), base)
        self.assertIsNone(kc.find_base(bytearray(0x1000)))

    def test_check_cmos(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertEqual(kc.check_cmos(d), 'absent')
            with open(os.path.join(d, 'CMOS'), 'wb') as f:
                f.write(bytes(2040))
            self.assertEqual(kc.check_cmos(d), 'zero')
            with open(os.path.join(d, 'CMOS'), 'wb') as f:
                f.write(b'\x01' + bytes(2039))
            with self.assertRaises(SystemExit):
                kc.check_cmos(d)

    def test_dosbox_cmd_has_the_verified_flags(self):
        cmd = kc.dosbox_cmd('/r', '/r/C', '/r/CD/RAGECD.ISO', 'walk', 75, 25, 1)
        s = ' '.join(cmd)
        for want in ('-time-limit 75', 'dosbox memory file=/r/guest.mem',
                     'log logfile=/r/dosbox.log', 'MOUNT C "/r/C" -ro',
                     'AUTOTYPE -w 25 -p 1 enter enter down', 'DX-CAPTURE /V /O PRAGE.EXE -f'):
            self.assertIn(want, s)
        for arg in kc.LOG_CON_ARGS:
            self.assertIn(arg, cmd)

    def test_bios_new_keys_wraps(self):
        mem = bytearray(0x500)
        mem[0x43C:0x43E] = (0x1C0D).to_bytes(2, 'little')     # slot 0x3C
        mem[0x41E:0x420] = (0x011B).to_bytes(2, 'little')     # slot 0x1E after the wrap
        self.assertEqual(kc.bios_new_keys(mem, 0x3C, 0x20, 0x1E, 0x3E), [0x1C0D, 0x011B])

    def test_field_pokes_lists_only_changed_bytes(self):
        d = [0] * 63
        d[1] = (1 << 14) | (3 << 6)
        window = bytearray(b'\x11' * (kf.WIN_HI - kf.WIN_LO))
        pokes = kc.field_pokes(window, d, [('field', 1, 0xAB)])
        self.assertEqual(pokes, [(kf.NIB_DS + 1 - kf.WIN_LO, 0x11, 0xB1),
                                 (kf.NIB_DS + 2 - kf.WIN_LO, 0x11, 0x1A)])
        self.assertEqual(window, bytearray(b'\x11' * (kf.WIN_HI - kf.WIN_LO)))   # not mutated


    def test_dosbox_cmd_without_autotype(self):
        cmd = kc.dosbox_cmd('/r', '/r/C', '/r/CD/RAGECD.ISO', 'walk', 75, 25, 1, autotype=False)
        self.assertFalse(any('AUTOTYPE' in c for c in cmd))
        self.assertIn('DX-CAPTURE /V /O PRAGE.EXE -f', cmd)

    def test_schedule_follows_autotype_timing(self):
        self.assertEqual(kc.schedule(('enter', ',', ',', 'esc', 'down'), 25, 1),
                         [(25, 'enter'), (28, 'esc'), (29, 'down')])

    def test_bios_insert_appends_wraps_and_refuses_when_full(self):
        mem = bytearray(0x500)
        mem[0x480:0x484] = bytes.fromhex('1e003e00')              # start 0x1E, end 0x3E
        mem[0x41A:0x41E] = bytes.fromhex('3c003c00')              # head = tail = 0x3C
        self.assertTrue(kc.bios_insert(mem, 0x50E0))
        self.assertEqual(mem[0x43C:0x43E], bytes.fromhex('e050'))
        self.assertEqual(mem[0x41C:0x41E], bytes.fromhex('1e00'))  # wrapped to start
        mem[0x41A:0x41C] = bytes.fromhex('2000')                   # head two past the tail
        self.assertFalse(kc.bios_insert(mem, 0x011B))             # next slot is the head: full
        self.assertEqual(mem[0x41C:0x41E], bytes.fromhex('1e00'))


class SaveReloadStaging(unittest.TestCase):
    """The save-reload capture's new machinery (§J3.2, §J3.8(c)): the writable
    boot-1 mount is the one scenario that opens C:, so every other scenario's
    read-only default and the zero-CMOS gate must stay in place."""

    def test_mount_is_read_only_unless_the_writable_boot_asks(self):
        cmd = kc.dosbox_cmd('/r', '/r/C', '/r/CD/RAGECD.ISO', 'save-reload', 55, 25, 1,
                            boot=1, ro=False)
        self.assertIn('MOUNT C "/r/C"', cmd)
        self.assertNotIn('MOUNT C "/r/C" -ro', cmd)
        cmd = kc.dosbox_cmd('/r', '/r/C', '/r/CD/RAGECD.ISO', 'save-reload', 55, 25, 1,
                            boot=2)
        self.assertIn('MOUNT C "/r/C" -ro', cmd)
        self.assertIn('AUTOTYPE -w 25 -p 1 enter', ' '.join(cmd))   # boot 2's one key

    def test_check_cmos_aborts_every_scenario_before_dosbox_runs(self):
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, 'CMOS'), 'wb') as f:
                f.write(b'\x01' + bytes(2039))
            exe = os.path.join(d, 'PRAGE.EXE')
            with open(exe, 'wb') as f:
                f.write(b'')
            for name in sorted(ks.SCENARIOS):
                argv = ['k11_capture.py', '--scenario', name, '--out',
                        os.path.join(ROOT, 'data', 'k11-captures', 'x'),
                        '--game-dir', d, '--exe', exe]
                with mock.patch.object(sys, 'argv', argv), \
                     mock.patch.object(kc.subprocess, 'run') as dosbox:
                    with self.assertRaises(SystemExit) as cm:
                        kc.main()
                self.assertIn('2040 zero bytes', str(cm.exception), name)
                dosbox.assert_not_called()      # the zero-CMOS gate precedes DOSBox

    def test_failed_boot2_leaves_no_partial_capture_dir(self):
        """A boot-2 CHECK failure must publish nothing: boot 1 writes its CMOS
        and boot1 poll.log before boot 2 runs, so returning there used to leave
        a partial data/…/save-reload that a later oracle reads as a real
        capture (and fails on the missing poll.log)."""
        with tempfile.TemporaryDirectory() as d:
            out = os.path.join(d, 'save-reload')
            exe = os.path.join(d, 'PRAGE.EXE')
            with open(exe, 'wb') as f:
                f.write(b'')
            game = os.path.join(d, 'game')

            def fake_stage(root, exe_path, game_dir):
                os.makedirs(game, exist_ok=True)
                return game

            def fake_run_boot(root, game_dir, iso, descs, scenario, boot, pokes,
                              limit, enter_wait, pace, autotype, ro):
                os.makedirs(os.path.join(root, 'avi'), exist_ok=True)
                with open(os.path.join(root, 'poll.log'), 'w') as f:
                    f.write('')
                return dict(cmd=['dosbox'], root=root, lines=[], wall=1.0, rc=0,
                            reason='time-limit', avi_dir=os.path.join(root, 'avi'), avis=[])

            def fake_poll_checks(scenario, boot, lines, pokes):
                return ([('script', boot == 1)], [])         # boot 2's CHECK fails

            argv = ['k11_capture.py', '--scenario', 'save-reload', '--out', out,
                    '--game-dir', game, '--exe', exe]
            with mock.patch.object(sys, 'argv', argv), \
                 mock.patch.object(kc, 'guard_out', return_value=out), \
                 mock.patch.object(kc, 'check_cmos', return_value='zero'), \
                 mock.patch.object(kc.tcap, 'stage', side_effect=fake_stage), \
                 mock.patch.object(kc.kf, 'load_descriptors', return_value=[]), \
                 mock.patch.object(kc, 'stage_cmos_real', return_value='file'), \
                 mock.patch.object(kc, 'read_cmos', return_value=bytes(2040)), \
                 mock.patch.object(kc, 'snapshot_tree', return_value={}), \
                 mock.patch.object(kc, 'snapshot_mtimes', return_value={}), \
                 mock.patch.object(kc, 'saved_cmos_check', return_value=(True, '')), \
                 mock.patch.object(kc, 'staging_check', return_value=(True, '')), \
                 mock.patch.object(kc, 'run_boot', side_effect=fake_run_boot), \
                 mock.patch.object(kc, 'poll_checks', side_effect=fake_poll_checks), \
                 mock.patch.object(kc, 'print_checks'):
                self.assertEqual(kc.main(), 1)
            self.assertFalse(os.path.exists(out), 'a failed boot 2 left a capture dir')
            left = [n for n in os.listdir(d) if 'save-reload' in n]
            self.assertEqual(left, [], 'a failed boot 2 left %r' % left)

    def test_stage_cmos_real_replaces_the_symlink_with_a_copy(self):
        with tempfile.TemporaryDirectory() as d:
            gdir = os.path.join(d, 'g')
            game = os.path.join(d, 'C')
            os.makedirs(gdir)
            os.makedirs(game)
            with open(os.path.join(gdir, 'CMOS'), 'wb') as f:
                f.write(b'\x00' * 32)
            os.symlink(os.path.join(gdir, 'CMOS'), os.path.join(game, 'CMOS'))
            self.assertEqual(kc.stage_cmos_real(game), 'copied')
            self.assertFalse(os.path.islink(os.path.join(game, 'CMOS')))
            with open(os.path.join(game, 'CMOS'), 'rb') as f:
                self.assertEqual(f.read(), b'\x00' * 32)
            with open(os.path.join(gdir, 'CMOS'), 'rb') as f:
                self.assertEqual(f.read(), b'\x00' * 32)     # the game dir is untouched
            self.assertEqual(kc.stage_cmos_real(game), 'file')   # boot 2 reuses it
            empty = os.path.join(d, 'C2')
            os.makedirs(empty)
            self.assertEqual(kc.stage_cmos_real(empty), 'absent')

    def test_cmos_valid_and_saved_cmos_check(self):
        good = bytearray(2040)
        good[0x10] = 3
        s = 0
        for b in good[:2039]:
            s ^= b
        good[2039] = s
        self.assertTrue(kc.cmos_valid(bytes(good)))
        bad = bytearray(good)
        bad[2039] ^= 1
        self.assertFalse(kc.cmos_valid(bytes(bad)))          # a mutation of the XOR byte
        self.assertFalse(kc.cmos_valid(bytes(good[:100])))   # a mutation of the length
        self.assertFalse(kc.cmos_valid(b''))
        staged = bytes(2040)
        ok, text = kc.saved_cmos_check(bytes(good), staged)
        self.assertTrue(ok, text)
        ok, text = kc.saved_cmos_check(staged, staged)       # no save reached the file
        self.assertFalse(ok)
        self.assertIn('unchanged', text)
        ok, text = kc.saved_cmos_check(bytes(good[:100]), staged)
        self.assertFalse(ok)
        ok, text = kc.saved_cmos_check(None, staged)
        self.assertFalse(ok)
        ok, text = kc.saved_cmos_check(bytes(good), None)    # the game dir had no CMOS
        self.assertTrue(ok, text)

    def test_staging_check(self):
        ok, text = kc.staging_check({'A': 'file'}, {'A': 'file'}, {}, {})
        self.assertTrue(ok, text)
        ok, text = kc.staging_check({'A': 'file'}, {'A': 'file', 'CMOS': 'file'}, {}, {})
        self.assertTrue(ok, text)                            # the save may appear
        ok, text = kc.staging_check({'A': 'file'}, {'A': 'file', 'NEW': 'file'}, {}, {})
        self.assertFalse(ok)
        self.assertIn('NEW', text)
        ok, text = kc.staging_check({'L': 'link:/x'}, {'L': 'link:/y'}, {}, {})
        self.assertFalse(ok)
        self.assertIn('L', text)
        ok, text = kc.staging_check({}, {}, {'S16.FAT': 'file:1:100'}, {'S16.FAT': 'file:2:100'})
        self.assertFalse(ok)
        self.assertIn('game dir', text)

    def test_read_ranges_maps_base_plus_the_file_offset(self):
        base = 0x100000
        mm = bytearray(0x200000)
        for rid, va, n in kc.SAVE_RANGES:
            at = base + va - ks.DATA_BASE_VA
            for i in range(n):
                mm[at + i] = (i + 1) & 0xFF
        ranges = {rid: (at, data) for rid, at, data in kc.read_ranges(mm, base)}
        for rid, va, n in kc.SAVE_RANGES:
            at, data = ranges[rid]
            self.assertEqual(at, base + va - ks.DATA_BASE_VA)
            self.assertEqual(len(data), n)
            self.assertEqual(data, bytes(((i + 1) & 0xFF) for i in range(n)))

    def test_ranges_logged_and_settled(self):
        lines = ['B ms=1 base=00266000']
        lines += ['R ms=3 tick=00000100 id=%s img=%s' % (rid, '11' * n)
                  for rid, _, n in kc.SAVE_RANGES]
        lines += [_k(25000, 0x300), _p(25001, 0x300, f=0x155, mode=0x27)]
        recs = [ks.parse(x) for x in lines]
        ok, text = kc.ranges_logged(recs)
        self.assertTrue(ok, text)
        ok, text = kc.ranges_settled(recs)
        self.assertTrue(ok, text)
        late = lines + ['R ms=26000 tick=00000400 id=k0 img=%s' % ('33' * 0x26)]
        ok, text = kc.ranges_settled([ks.parse(x) for x in late])
        self.assertFalse(ok)
        self.assertIn('after the Enter', text)
        missing = [x for x in lines if 'id=hs2' not in x]
        ok, text = kc.ranges_logged([ks.parse(x) for x in missing])
        self.assertFalse(ok)
        self.assertIn('hs2', text)

    def test_poll_checks_wire_the_script_and_the_ranges(self):
        lines = ['B ms=1 base=00266000', _p(2, 0x100)]
        lines += ['R ms=3 tick=00000100 id=%s img=%s' % (rid, '11' * n)
                  for rid, _, n in kc.SAVE_RANGES]
        lines += [_k(25000, 0x300), _p(25001, 0x300, f=0x155, mode=0x27)]
        checks, recs = kc.poll_checks('save-reload', 2, lines, ())
        self.assertTrue(all(ok for _, ok in checks), checks)
        self.assertTrue(recs)
        stray = lines + [_k(26000, 0x400, 0x50E0)]          # a key the scenario never sent
        checks, _ = kc.poll_checks('save-reload', 2, stray, ())
        self.assertFalse(dict(checks)['script'])


if __name__ == '__main__':
    unittest.main()
