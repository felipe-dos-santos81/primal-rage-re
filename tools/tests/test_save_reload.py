# tools/tests/test_save_reload.py
"""The save-reload oracle's tests (track J task 7, record §J3.6): the two claims
and the scenario parsing. Each claim assertion is mutation-proved by feeding it
a deliberately wrong input (a flipped byte, a moved frame, a changed capture)."""
import hashlib
import os
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import k11_capture as kc
import k11_session as ks
import save_reload as sr


def p_line(ms, tick, f=0x100, st=0, mode=3, kb=0):
    vals = {n: 0 for n, _, _ in ks.POLL_FIELDS}
    vals.update(f=f, st=st, mode=mode, tick=tick)
    return ks.format_p(ms, vals, kb, 0x1E)


def k_line(ms, tick, word=0x1C0D):
    return 'K ms=%d tick=%08X f=0100 key=%04X' % (ms, tick, word)


def range_bytes(v=0x11, one=None):
    """One uniform image per range ({id: bytes}); `one` overrides one id."""
    out = {rid: bytes([v]) * n for rid, _, n in kc.SAVE_RANGES}
    if one is not None:
        out.update(one)
    return out


def boot2_lines(enter_ms=25000, enter_tick=0x300, enter_f=0x155, extra=()):
    """A minimal boot-2 poll.log: the base, one L and one R per range (before
    the Enter), the Enter's K, its mode-0x27 P, then `extra` lines."""
    out = ['B ms=1 base=00266000']
    for rid, va, n in kc.SAVE_RANGES:
        out.append('L ms=2 base=00266000 id=%s at=%08X len=%d' % (rid, va, n))
        out.append('R ms=3 tick=00000100 id=%s img=%s' % (rid, (b'\x11' * n).hex()))
    out.append(k_line(enter_ms, enter_tick))
    out.append(p_line(enter_ms + 1, enter_tick, f=enter_f, mode=0x27))
    return out + list(extra)


def r_line(ms, tick, rid, img):
    return 'R ms=%d tick=%08X id=%s img=%s' % (ms, tick, rid, img.hex())


def port_line(idx, f, tick, ranges):
    return '%05d f=%04X tick=%08X %s' % (idx, f, tick,
        ' '.join('%s=%s' % (rid, ranges[rid].hex()) for rid, _, _ in kc.SAVE_RANGES))


class Scenario(unittest.TestCase):
    def test_save_reload_is_as_designed(self):
        sc = ks.SCENARIOS['save-reload']
        self.assertEqual(sc['keys'],
                         tuple('enter enter down down esc down enter enter right esc esc'.split()))
        self.assertEqual(sc['pokes'], ())
        self.assertEqual(sc['time_limit'], 55)
        self.assertTrue(sc['writable'])
        self.assertEqual(sc['boot2']['keys'], ('enter',))
        self.assertEqual(sc['boot2']['time_limit'], 45)

    def test_boot_selects_the_keys_in_autotype_and_port_script(self):
        self.assertTrue(ks.autotype_line('save-reload', 25, 1, boot=1).endswith(
            'enter enter down down esc down enter enter right esc esc'))
        self.assertEqual(ks.autotype_line('save-reload', 25, 1, boot=2),
                         'AUTOTYPE -w 25 -p 1 enter')
        self.assertEqual(ks.keys_for('save-reload', 2), ('enter',))
        self.assertEqual(ks.keys_for('walk', 1)[:2], ('enter', 'enter'))

        lines = [k_line(25000, 0x300), p_line(25001, 0x300, f=0x155, mode=0x27)]
        out = ks.port_script('save-reload', lines, boot=2)
        self.assertEqual(out, '# k11 port script v1: scenario save-reload\n'
                              'enter_frame 341\n'
                              'enter_state 0000\n'
                              'end %d\n' % ks.END_TAIL_TICKS)
        with self.assertRaises(ks.ScriptError):       # the 11-key boot rejects the one-Enter log
            ks.port_script('save-reload', lines, boot=1)


class Records(unittest.TestCase):
    def test_parse_l_and_r(self):
        rec = ks.parse('L ms=5 base=00266000 id=cfg at=0090CE4 len=2040')
        self.assertEqual((rec['kind'], rec['ms'], rec['base'], rec['id'], rec['at'], rec['len']),
                         ('L', 5, 0x266000, 'cfg', 0x90CE4, 2040))
        rec = ks.parse('R ms=9 tick=00000123 id=hs0 img=0011aaff')
        self.assertEqual((rec['kind'], rec['ms'], rec['tick'], rec['id'], rec['img']),
                         ('R', 9, 0x123, 'hs0', bytes.fromhex('0011aaff')))

    def test_enter_record_is_the_first_k_and_its_mode_27_p(self):
        lines = ['B ms=1 base=00266000',
                 k_line(10, 0x100),
                 p_line(11, 0x100, f=0x155, mode=3),     # still mode 3 at the Enter's frame
                 p_line(12, 0x101, f=0x156, mode=0x27)]
        recs = [ks.parse(x) for x in lines]
        k, p27 = ks.enter_record(recs)
        self.assertEqual((k['ms'], p27['f']), (10, 0x156))


class WriteClaim(unittest.TestCase):
    def cmos(self, tweak=None):
        data = bytearray(2040)
        data[0x10] = 0x5A
        if tweak is not None:
            tweak(data)
        s = 0
        for b in data[:2039]:
            s ^= b
        data[2039] = s
        return bytes(data)

    def test_write_claim(self):
        with tempfile.TemporaryDirectory() as d:
            cap = os.path.join(d, 'CMOS')
            scratch = os.path.join(d, 'scratch', 'cmos')
            data = self.cmos()
            with open(cap, 'wb') as f:
                f.write(data)
            ok, text = sr.write_claim(scratch, cap)
            self.assertFalse(ok)
            self.assertIn('no file', text)
            os.makedirs(os.path.dirname(scratch))
            with open(scratch, 'wb') as f:
                f.write(data)
            ok, text = sr.write_claim(scratch, cap)
            self.assertTrue(ok, text)
            with open(scratch, 'wb') as f:              # a one-bit mutation
                f.write(data[:0x7D0] + bytes([data[0x7D0] ^ 1]) + data[0x7D1:])
            ok, text = sr.write_claim(scratch, cap)
            self.assertFalse(ok)
            self.assertIn('0x7D0', text)
            with open(scratch, 'wb') as f:              # a short mutation
                f.write(data[:100])
            ok, text = sr.write_claim(scratch, cap)
            self.assertFalse(ok)
            with open(scratch, 'wb') as f:
                f.write(data)
            ok, text = sr.write_claim(scratch, cap, cmos_sha256=hashlib.sha256(data).hexdigest())
            self.assertTrue(ok, text)
            ok, text = sr.write_claim(scratch, cap, cmos_sha256='0' * 64)
            self.assertFalse(ok)
            self.assertIn('sha256', text)
            with open(cap, 'wb') as f:                  # a capture whose XOR is broken
                f.write(bytes(2040))
            ok, text = sr.write_claim(scratch, cap)
            self.assertFalse(ok)

    def test_session_cmos(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertIsNone(sr.session_cmos(d))
            with open(os.path.join(d, 'session.txt'), 'w') as f:
                f.write('scenario=save-reload\ncmos=zero pokes=[] input=inject\n')
            self.assertEqual(sr.session_cmos(d), 'zero')
            with open(os.path.join(d, 'session.txt'), 'w') as f:
                f.write('scenario=save-reload\n')
            self.assertIsNone(sr.session_cmos(d))


class LoadClaim(unittest.TestCase):
    def recs(self, lines):
        return [ks.parse(x) for x in lines]

    def test_ranges_at_pin_is_the_last_record_at_or_before_the_pin(self):
        before = range_bytes(0x11)
        after = range_bytes(0x11, one={'hs0': b'\x22' * 0x78})
        lines = boot2_lines(extra=[
            r_line(20000, 0x200, 'hs0', after['hs0']),          # a change before the Enter
        ])
        got = sr.ranges_at_pin(self.recs(lines))
        self.assertEqual(got['cfg'], before['cfg'])
        self.assertEqual(got['hs0'], after['hs0'])
        ok, text = kc.ranges_settled(self.recs(lines))
        self.assertTrue(ok, text)

    def test_settled_rejects_a_change_after_the_enter(self):
        lines = boot2_lines(extra=[r_line(26000, 0x400, 'k0', b'\x33' * 0x26)])
        ok, text = kc.ranges_settled(self.recs(lines))
        self.assertFalse(ok)
        self.assertIn('after the Enter', text)
        # ranges_at_pin keeps the pin's value: the late change is not the pin's
        self.assertEqual(sr.ranges_at_pin(self.recs(lines))['k0'], b'\x11' * 0x26)
        # and the claim refuses the capture rather than comparing against it
        ok, text = sr.load_claim(self.recs(lines), [], 0x155)
        self.assertFalse(ok)
        self.assertIn('after the Enter', text)

    def test_ranges_at_pin_needs_a_record_and_a_pin(self):
        lines = boot2_lines()
        recs = [r for r in self.recs(lines) if not (r['kind'] == 'R' and r['id'] == 'hs2')]
        with self.assertRaises(KeyError):
            sr.ranges_at_pin(recs)
        with self.assertRaises(KeyError):               # no mode-0x27 P record
            sr.ranges_at_pin(self.recs([k_line(10, 0x100)]))

    def test_load_claim_and_its_mutations(self):
        recs = self.recs(boot2_lines())
        want = range_bytes(0x11)
        lines = [port_line(0, 0x155, 0x300, want),
                 port_line(1, 0x160, 0x301, want),
                 port_line(2, 0x161, 0x302, want)]
        port = [sr.parse_ranges_line(x) for x in lines]
        ok, text = sr.load_claim(recs, port, 0x155)
        self.assertTrue(ok, text)
        self.assertIn('3', text)                        # the frames compared are reported

        bad = list(lines)
        bad[2] = port_line(2, 0x161, 0x302, range_bytes(0x11, one={'cfg': b'\x99' * 0x7F8}))
        ok, text = sr.load_claim(recs, [sr.parse_ranges_line(x) for x in bad], 0x155)
        self.assertFalse(ok)
        self.assertIn('f=0161', text)
        self.assertIn('cfg', text)
        self.assertIn('0x0', text)                      # the differing byte

        bad = list(lines)
        bad[0] = port_line(0, 0x155, 0x300, range_bytes(0x11, one={'k0': b'\x00' + b'\x11' * 0x25}))
        ok, text = sr.load_claim(recs, [sr.parse_ranges_line(x) for x in bad], 0x155)
        self.assertFalse(ok)
        self.assertIn('f=0155', text)

        ok, text = sr.load_claim(recs, [sr.parse_ranges_line(x) for x in lines[1:]], 0x155)
        self.assertFalse(ok)                            # no line at the pinned frame
        self.assertIn('f=0155', text)

        ok, text = sr.load_claim(recs, port, 0x156)     # the capture enters at another frame
        self.assertFalse(ok)
        self.assertIn('pinned', text)

    def test_parse_ranges_line_and_capture_identity(self):
        line = port_line(3, 0x160, 0x301, range_bytes(0x11))
        rec = sr.parse_ranges_line(line)
        self.assertEqual((rec['idx'], rec['f'], rec['tick']), (3, 0x160, 0x301))
        self.assertEqual(sorted(rec['ranges']), sorted(kc.RANGE_IDS))
        self.assertEqual(rec['ranges']['hs1'], b'\x11' * 0x1C)

        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, 'poll.log'), 'wb') as f:
                f.write(b'x' * 10)
            sha = hashlib.sha256(b'x' * 10).hexdigest()
            ok, text = sr.capture_identity(d, sha, 0)
            self.assertTrue(ok, text)
            with open(os.path.join(d, 'frame_0000.raw'), 'wb') as f:
                f.write(b'\x00')
            ok, text = sr.capture_identity(d, sha, 1)
            self.assertTrue(ok, text)
            ok, text = sr.capture_identity(d, sha, 2)
            self.assertFalse(ok)
            self.assertIn('frames', text)
            ok, text = sr.capture_identity(d, '0' * 64, 1)
            self.assertFalse(ok)
            self.assertIn('sha256', text)
            ok, text = sr.capture_identity(os.path.join(d, 'nope'), sha, 1)
            self.assertFalse(ok)


class LoadRangesFile(unittest.TestCase):
    def test_load_ranges_reads_the_port_dump(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'ranges.txt')
            with open(path, 'w') as f:
                f.write(port_line(0, 0x155, 0x300, range_bytes(0x11)) + '\n')
                f.write(port_line(1, 0x160, 0x301, range_bytes(0x11)) + '\n')
            got = sr.load_ranges(path)
            self.assertEqual([r['idx'] for r in got], [0, 1])
            self.assertEqual(got[1]['ranges']['cfg'], b'\x11' * 0x7F8)


if __name__ == '__main__':
    unittest.main()
