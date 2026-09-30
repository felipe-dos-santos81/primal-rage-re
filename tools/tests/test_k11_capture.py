# tools/tests/test_k11_capture.py
import os, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import k11_capture as kc
import k11_fields as kf


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


if __name__ == '__main__':
    unittest.main()
