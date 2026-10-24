# tools/tests/test_tick_vector.py
"""The INT 8 tick vector's raw bytes (record docs/superpowers/plans/2026-10-23-endgame-truth-derivations.md
§E1.6, §E10). Raw-byte pin only: it recomputes the record's addresses and arithmetic from the original's
bytes, and has no port counterpart (the port models the callback, not the vector). Pre-fixup code bytes are
read at VA + 0x52E54 (AGENTS.md), so a code or data pointer immediate in them is still an object offset: the
code object's base 0x10000 and the data object's 0x80000 are added where a pointer is checked."""
import os
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(os.environ.get('PR_GAME_DIR', os.path.join(ROOT, 'data', 'game', 'C')), 'PRAGE.EXE')
CODE_FILE_OFF = 0x52E54          # obj-0 file offset = VA + 0x52E54
CODE_BASE = 0x10000
DATA_BASE = 0x80000
PIT_CLOCK = 1193182              # the standard PC PIT input clock (the bytes give the divisor, not this)


def code(va, n):
    with open(EXE, 'rb') as f:
        f.seek(va + CODE_FILE_OFF)
        return f.read(n)


def call_target(va):
    """The rel32 target of the call/jmp at `va` (rel32 is relocation-independent)."""
    b = code(va, 5)
    assert b[0] in (0xE8, 0xE9), hex(b[0])
    return (va + 5 + int.from_bytes(b[1:5], 'little', signed=True)) & 0xFFFFFFFF


@unittest.skipUnless(os.path.isfile(EXE), 'needs data/game/C/PRAGE.EXE')
class TickVectorTests(unittest.TestCase):
    def test_the_installer_sets_vector_8_to_0x6627e(self):
        b = code(0x66519, 0x15)                                  # 0x66519..0x6652D, 21 bytes
        self.assertEqual(b.hex(' '), 'b8 08 00 00 00 ba 7e 62 05 00 66 8c cb b4 25 1e 66 8e db cd 21')
        self.assertEqual(b[0], 0xB8)                             # mov eax,8   (AL = vector 8)
        self.assertEqual(int.from_bytes(b[1:5], 'little'), 8)
        self.assertEqual(b[5], 0xBA)                             # mov edx,0x0005627E
        self.assertEqual(int.from_bytes(b[6:10], 'little'), 0x5627E)
        self.assertEqual(b[10:13].hex(' '), '66 8c cb')           # mov bx,cs
        self.assertEqual(b[13:15].hex(' '), 'b4 25')              # mov ah,0x25 (DOS set-vector)
        self.assertEqual(b[15:19].hex(' '), '1e 66 8e db')        # push ds; mov ds,bx
        self.assertEqual(b[19:21].hex(' '), 'cd 21')              # int 0x21; 0x6652C is the cd
        # the immediate is a code-object offset: the object-0 fixup makes the handler's linear address
        self.assertEqual(int.from_bytes(b[6:10], 'little') + CODE_BASE, 0x6627E)

    def test_the_handler_is_a_runtime_ail_routine(self):
        # 0x6627E: cmp dword cs:[0xEFD12],0 (re-entrancy guard); the disp32 is pre-fixup 0x6FD12
        b = code(0x6627E, 0x15)
        self.assertEqual(b[:8].hex(' '), '2e 83 3d 12 fd 06 00 00')
        self.assertEqual(int.from_bytes(b[3:7], 'little') + DATA_BASE, 0xEFD12)
        self.assertEqual(b[8:14].hex(' '), '0f 87 11 01 00 00')   # ja 0x6639D
        self.assertEqual(b[14:0x15].hex(' '), '60 1e 06 0f a0 0f a8')  # pushal; push ds/es/fs/gs

    def test_the_pit_routine_sends_mode_0x36_to_port_0x43(self):
        b = code(0x663A4, 0x20)
        self.assertEqual(b[:8].hex(' '), '55 8b ec 53 56 57 9c fa')  # push ebp; mov ebp,esp; pushes; cli
        self.assertEqual(b[8:12].hex(' '), 'b0 36 e6 43')            # mov al,0x36; out 0x43,al (0x663AC)
        self.assertEqual(b[0x16:0x18].hex(' '), 'e6 40')             # out 0x40,al   divisor low  (0x663BA)
        self.assertEqual(b[0x1C:0x1E].hex(' '), 'e6 40')             # out 0x40,al   divisor high (0x663C0)

    def test_the_divisor_converter_gives_19887_for_the_ail_period(self):
        # 0x663D4: cmp [ebp+8],0xD68D; mov ebx,0x20BC; mov ecx,0x2710; mul ecx; div ebx
        b = code(0x663D4, 0x2C)
        self.assertEqual(b[6:11].hex(' '), 'b8 00 00 00 00')          # mov eax,0 (the >= max arm)
        self.assertEqual(int.from_bytes(b[14:18], 'little'), 0xD68D)  # the max period, 54925
        self.assertEqual(b[18:20].hex(' '), '73 11')                  # jae 0x663F9
        self.assertEqual(int.from_bytes(b[24:28], 'little'), 0x20BC)  # 8,380  at 0x663EB
        self.assertEqual(int.from_bytes(b[29:33], 'little'), 0x2710)  # 10,000 at 0x663F0
        self.assertEqual(b[33:39].hex(' '), 'f7 e1 f7 f3 50 e8')      # mul ecx; div ebx; push eax; call
        # the period: AIL's rate setter 0x66BB8 does 1,000,000/60 at 0x66BD0
        r = code(0x66BB8, 0x1B)
        self.assertEqual(r[0x10:0x15].hex(' '), 'b8 40 42 0f 00')     # mov eax,0xF4240 at 0x66BC8
        self.assertEqual(int.from_bytes(r[0x10:0x15][1:], 'little'), 1_000_000)
        self.assertEqual(r[0x15:0x18].hex(' '), '8b 5d 0c')           # mov ebx,[ebp+0xC] (the rate)
        self.assertEqual(r[0x18:0x1A].hex(' '), 'f7 f3')              # div ebx at 0x66BD0
        period = 1_000_000 // 60
        self.assertEqual(period, 16666)
        divisor, rem = divmod(period * 10000, 0x20BC)
        self.assertEqual((divisor, rem), (19887, 6940))
        rate = PIT_CLOCK / divisor
        self.assertAlmostEqual(rate, 59.998, places=3)                # standard clock / the bytes' divisor

    def test_the_ail_callback_is_registered_and_started_at_60_hz(self):
        b = code(0x1CFED, 0x20)
        self.assertEqual(b[0:5].hex(' '), '68 f4 bd 00 00')           # push 0x1BDF4 (pre-fixup 0xBDF4)
        self.assertEqual(int.from_bytes(b[1:5], 'little') + CODE_BASE, 0x1BDF4)
        self.assertEqual(call_target(0x1CFF2), 0x5DA12)                # AIL register_timer
        self.assertEqual(b[0xD:0xF].hex(' '), '6a 3c')                 # push 0x3C at 0x1CFFA (60 Hz)
        self.assertEqual(call_target(0x1CFFF), 0x5DA87)                # AIL set_timer_frequency
        self.assertEqual(call_target(0x1D008), 0x5DAA6)                # AIL start_timer

    def test_0x2d62c_is_the_counter_not_the_isr(self):
        # 0x2D62C: inc dword [0x105D88]; mov eax,eax; ret (9 bytes); the disp32 is pre-fixup 0x85D88
        b = code(0x2D62C, 9)
        self.assertEqual(b.hex(' '), 'ff 05 88 5d 08 00 8b c0 c3')
        self.assertEqual(int.from_bytes(b[2:6], 'little') + DATA_BASE, 0x105D88)
        self.assertEqual(call_target(0x1BE28), 0x2D62C)                # 0x1BDF4 calls it at 0x1BE28
        self.assertEqual(call_target(0x1BE1C), 0x1BBAC)                # the input sampler, just before


if __name__ == '__main__':
    unittest.main()
