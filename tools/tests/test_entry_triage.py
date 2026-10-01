"""Unit tests for tools/entry_triage.py (record docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md).

The synthetic tests build one hand-assembled image (WORLD) with one candidate per class; every byte
sequence is listed with its assembly. The real-image tests run the tool on the image
`build/diffrun --image-out` writes and pin the universe and the class counts (record §E2.9).
"""
import collections
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import diff_emu as E
import entry_triage as T

REQUIRED = os.environ.get("PR_ORACLE_REQUIRED") == "1"
needs_capstone = unittest.skipUnless(E.capstone is not None or REQUIRED, "capstone not installed")


def le32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


def rel32(at, target, op=b"\xe8"):
    """call/jmp rel32 at `at` to `target` (E8/E9 + displacement from the next instruction)."""
    return op + le32(target - (at + len(op) + 4))


class World:
    """An image from 0x10000 to 0xC0000 (the code object, the runtime cut at 0x5D000, the data object
    from 0x80000, the move table 0xA3528 and the finisher tables 0xBDAE4/0xBDB00 all inside it)."""

    def __init__(self):
        self.data = bytearray(0xB0000)
        self.ghidra = []

    def put(self, addr, b):
        self.data[addr - 0x10000:addr - 0x10000 + len(b)] = b

    def fn(self, addr, b):
        """A Ghidra function: its bytes, listed as (entry, size)."""
        self.put(addr, b)
        self.ghidra.append((addr, len(b)))

    def entry(self, addr, b):
        """A body after a `ret` byte."""
        self.put(addr - 1, b"\xc3")
        self.put(addr, b)

    def image(self):
        return E.Image(bytes(self.data))


XOR_RET = bytes.fromhex("31C0C3")                    # xor eax,eax; ret


def build_world():
    w = World()
    # G1 10000: mov dword [eax+0x1c],0x10200 / mov eax,[0x10300] / call 0x10400 / jge 0x10500 / ret
    g1 = bytearray(bytes.fromhex("C7401C") + le32(0x10200) + b"\xa1" + le32(0x10300))
    g1 += rel32(0x10000 + len(g1), 0x10400)
    g1 += rel32(0x10000 + len(g1), 0x10500, b"\x0f\x8d") + b"\xc3"
    w.fn(0x10000, bytes(g1))
    # the span blit 51E5C: call dword [eax*4+0x80100]; ret
    w.fn(T.SPAN_BLIT, bytes.fromhex("FF1485") + le32(0x80100) + b"\xc3")
    # 11000: call dword [eax*4+0x80200]; ret                         (a call table outside the span code)
    w.fn(0x11000, bytes.fromhex("FF1485") + le32(0x80200) + b"\xc3")
    # 11100: jmp dword [eax*4+0x80300]; ret                          (a jump table)
    w.fn(0x11100, bytes.fromhex("FF2485") + le32(0x80300) + b"\xc3")
    w.fn(T.VOICE_FN, b"\xc3")                          # the voice entry, a ported leaf
    w.fn(0x18000, bytes.fromhex("ECC3"))               # in al,dx; ret
    # 18100: xor eax,eax / ret / inc eax (18103, after a ret but inside this Ghidra function) /
    # mov eax,0x5D030 (an operand above the runtime cut) / ret
    w.fn(0x18100, bytes.fromhex("31C0C340B8") + le32(0x5D030) + b"\xc3")
    w.put(0x90078, le32(0x18103))
    w.entry(0x5D030, XOR_RET)
    w.entry(0x10200, XOR_RET)                          # code-immediate
    w.entry(0x10300, bytes.fromhex("40C3"))            # data: G1 reads it as a memory operand
    w.entry(0x10400, XOR_RET)                          # direct: G1 calls it
    w.entry(0x10500, XOR_RET)                          # interior: G1's jge lands on it
    w.entry(0x12000, XOR_RET)                          # finisher
    w.put(0xBDAE4, le32(0x12000))
    w.put(0x80400, le32(0x12000))                      # and an aligned dword: the finisher rule wins
    # 12100 move callback (char 2, reaction 5): call 0x13000 / mov dword [eax+0x10],0x13100 /
    # mov edi,0x9090C390 (1210C; its immediate's C3 is at 1210E) / mov ebx,eax / xor eax,eax (12113) / ret
    mc = bytearray(rel32(0x12100, 0x13000) + bytes.fromhex("C74010") + le32(0x13100))
    mc += b"\xbf" + bytes.fromhex("90C39090") + bytes.fromhex("89C3") + bytes.fromhex("31C0") + b"\xc3"
    w.entry(0x12100, bytes(mc))
    w.put(T.MOVE_TABLE + (2 * 64 + 5) * 20, le32(0x12100))
    w.put(0x80500, le32(0x1210F))                      # mid-instruction: inside the mov edi,imm at 1210C
    w.put(0x80504, le32(0x12113))                      # interior: the xor after `89 C3`
    w.put(0x13000, XOR_RET)                            # helper: called only from 12100
    w.entry(0x13100, XOR_RET)                          # stored by 12100's immediate
    # 14000 span writer: call dword [eax*4+0x80180]; ret              (a second-level span table)
    w.entry(0x14000, bytes.fromhex("FF1485") + le32(0x80180) + b"\xc3")
    w.put(0x80100, le32(0x14000))
    w.entry(0x14100, XOR_RET)
    w.put(0x80180, le32(0x14100))
    # 5D020, above the runtime cut, is slot 1 of the span table 80100:
    # mov eax,[0x80108] (ends the table at its slot 2 once 5D020 is trusted) / call dword [eax*4+0x80600] / ret
    w.entry(0x5D020, b"\xa1" + le32(0x80108) + bytes.fromhex("FF1485") + le32(0x80600) + b"\xc3")
    w.put(0x80104, le32(0x5D020))
    w.entry(0x5D040, XOR_RET)                          # slot 2 until 5D020 is trusted: a stale entry
    w.put(0x80108, le32(0x5D040))
    w.entry(0x5D060, XOR_RET)                          # above the cut in a call table that is not span code
    w.put(0x80204, le32(0x5D060))
    w.fn(0x18200, rel32(0x18200, 0x5D080) + b"\xc3")  # call 0x5D080 (above the cut); ret
    w.entry(0x5D080, XOR_RET)
    w.entry(0x14200, XOR_RET)                          # span writer through 5D020's table
    w.put(0x80600, le32(0x14200))
    # 15000 call table: call 0x2C3FC; ret                              (a voice site, a ported callee)
    w.entry(0x15000, rel32(0x15000, T.VOICE_FN) + b"\xc3")
    w.put(0x80200, le32(0x15000))
    # 15100 jump table: call 0x18000; ret                              (`in` in its call tree)
    w.entry(0x15100, rel32(0x15100, 0x18000) + b"\xc3")
    w.put(0x80300, le32(0x15100))
    # 16000 animation target: the word D100 at 90000, the dword at 90002; body xor eax,eax; ret 4
    w.entry(0x16000, bytes.fromhex("31C0C20400"))
    w.put(0x90000, bytes.fromhex("00D1") + le32(0x16000))
    w.entry(0x16100, XOR_RET)                          # the 0x1F prefix form: DF11, an operand word, the dword
    w.put(0x90010, bytes.fromhex("11DF3412") + le32(0x16100))
    w.entry(0x16200, XOR_RET)                          # D200 is opcode 0x12, not a code opcode: dead
    w.put(0x90020, bytes.fromhex("00D2") + le32(0x16200))
    w.entry(0x16300, XOR_RET)                          # B100 is mode 0x2000; a rel32 in untrusted bytes
    w.put(0x90030, bytes.fromhex("00B1") + le32(0x16300))
    w.put(0x17000, rel32(0x17000, 0x16300))
    w.entry(0x16400, XOR_RET)                          # data-pointer: an aligned dword only
    w.put(0x90040, le32(0x16400))
    w.entry(0x16500, bytes.fromhex("8BC031C0C3"))      # starts with the filler mov eax,eax: data
    w.put(0x90044, le32(0x16500))
    w.entry(0x16600, bytes.fromhex("40DDC8"))          # inc eax, then undecodable DD C8: data
    w.put(0x90048, le32(0x16600))
    # 16900 after `ret 4` (C2 04 00): U0's greedy rule eats the 00 as a filler and misses it
    w.put(0x168FD, bytes.fromhex("C20400"))
    w.put(0x16900, XOR_RET)
    w.put(0x90064, le32(0x16900))
    # 17200, reached only by the rel32 at 17020 (untrusted bytes): xor eax,eax / mov ebx,eax /
    # inc eax (17204, after the C3 of `89 C3`) / call 0x2C3FC (17205, a voice site) / ret
    w.entry(0x17200, bytes.fromhex("31C089C340") + rel32(0x17205, T.VOICE_FN) + b"\xc3")
    w.put(0x17020, rel32(0x17020, 0x17200))
    w.put(0x90061, le32(0x17204))                      # an unaligned dword only
    # 16B00: the word 5100 has mode 0x4000 and opcode 0x11 but not the command bit 0x8000: dead
    w.entry(0x16B00, XOR_RET)
    w.put(0x90070, bytes.fromhex("0051") + le32(0x16B00))
    # not candidates
    w.entry(0x16A00, XOR_RET)                          # its only dword is in the code object (17100)
    w.put(0x17100, le32(0x16A00))
    w.put(0x166FF, b"\x41")
    w.put(0x16700, XOR_RET)                            # no ret before it
    w.put(0x9004C, le32(0x16700))
    w.put(0x90050, le32(0x10002))                      # inside G1
    w.entry(0x5D010, XOR_RET)                          # at or above RUNTIME_BASE
    w.put(0x90054, le32(0x5D010))
    w.entry(0x16800, bytes.fromhex("DDC8C3"))          # its first instruction does not decode
    w.put(0x90058, le32(0x16800))
    w.put(0x17010, rel32(0x17010, T.VOICE_FN))         # a voice call in untrusted bytes, in no body
    return w


EXPECTED = {
    0x10200: "code-immediate", 0x10300: "data", 0x10400: "direct", 0x10500: "interior",
    0x12000: "finisher", 0x12100: "move-callback", 0x1210F: "mid-instruction", 0x12113: "interior",
    0x14000: "span-writer", 0x14100: "span-writer", 0x14200: "span-writer", 0x15000: "call-table",
    0x15100: "jump-table", 0x16000: "anim-target", 0x16100: "anim-target", 0x16200: "dead",
    0x16300: "unclassified", 0x16400: "data-pointer", 0x16500: "data", 0x16600: "data",
    0x16900: "data-pointer", 0x16B00: "dead", 0x17204: "interior",
}


def world_triage(w=None):
    w = w or build_world()
    return T.Triage(w.image(), sorted(w.ghidra), {0x10200}, {T.VOICE_FN})


@needs_capstone
class UniverseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t = world_triage()
        cls.cands = cls.t.candidates()

    def test_the_universe_is_exactly_the_planted_candidates(self):
        # 13100 is stored by 12100's immediate (not Ghidra code) and 5D020 is above the cut: not in it
        self.assertEqual(self.cands, sorted(EXPECTED))

    def test_each_exclusion_rule_bites(self):
        for a in (0x16700, 0x10002, 0x5D010, 0x16800, 0x13100, 0x5D020, 0x16A00, 0x18103, 0x5D030):
            self.assertNotIn(a, self.cands, "%X" % a)

    def test_u0s_greedy_rule_misses_the_entry_after_ret_4(self):
        self.assertTrue(self.t.after_ret(0x16900))
        self.assertFalse(self.t.after_ret_u0(0x16900))
        self.assertTrue(self.t.after_ret_u0(0x10200))

    def test_fillers_are_skipped_backwards_in_order(self):
        w = World()
        w.put(0x10000, bytes.fromhex("C3" "8D4000" "90" "8BC0" "0000"))
        t = T.Triage(w.image(), [])
        self.assertTrue(t.after_ret(0x10009))          # 00 00, 8B C0, 90, 8D 40 00, then C3
        self.assertTrue(t.after_ret_u0(0x10009))
        w.put(0x10010, bytes.fromhex("C3" "8D4000"))
        t = T.Triage(w.image(), [])
        self.assertTrue(t.after_ret(0x10014))          # 8D 40 00 must be tried before 00
        self.assertTrue(t.after_ret_u0(0x10014))

    def test_ret_imm16_and_the_byte_level_c3_count(self):
        w = World()
        w.put(0x10010, bytes.fromhex("C20400" "90"))
        w.put(0x10020, bytes.fromhex("89C3"))          # mov ebx,eax: its C3 byte admits 0x10022 (§E2.1)
        t = T.Triage(w.image(), [])
        self.assertTrue(t.after_ret(0x10013))
        self.assertTrue(t.after_ret(0x10014))
        self.assertFalse(t.after_ret_u0(0x10013))      # U0's rule ate the 00 of `ret 4`
        self.assertFalse(t.after_ret_u0(0x10014))
        self.assertTrue(t.after_ret(0x10022))
        self.assertFalse(t.after_ret(0x10021))

    def test_the_first_bytes_of_the_image_never_wrap_around(self):
        w = World()
        w.put(0x10000, b"\x00\x90")
        w.data[-1] = 0xC3                              # a C3 at the image's last byte
        t = T.Triage(w.image(), [])
        self.assertEqual(t.u8(0xFFFF), -1)
        self.assertFalse(t.after_ret(0x10002))
        self.assertFalse(t.after_ret_u0(0x10002))

    def test_more_than_max_fill_bytes_of_filler_is_not_after_a_ret(self):
        w = World()
        w.put(0x10100, b"\xc3" + b"\x90" * (T.MAX_FILL + 2))
        t = T.Triage(w.image(), [])
        self.assertFalse(t.after_ret(0x10101 + T.MAX_FILL + 2))
        self.assertTrue(t.after_ret(0x10101 + T.MAX_FILL))


class LoaderTests(unittest.TestCase):
    def test_strict_and_loose(self):
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, "a.c"), "w") as f:
                f.write("/* 0x12345 — record */\nvoid f(void) { fn_register(0x23456u, g); x(); /* 0x34567 */ }\n")
            with open(os.path.join(d, "symbols.h"), "w") as f:
                f.write("/* 0x45678 */\n")
            strict, loose = T.load_ported(d)
        self.assertEqual(strict, {0x12345, 0x23456})
        self.assertEqual(loose, {0x12345, 0x23456, 0x34567})

    def test_ghidra_rows_skip_the_image_noise(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "f.csv")
            with open(p, "w") as f:
                f.write("entry,size,name,n_callers,n_callees,decompiled\n00010024,16,F,1,0,ok\n"
                        ".image::00062e89,1,G,0,0,ok\n00010010,8,H,0,0,ok\n")
            self.assertEqual(T.load_ghidra(p), [(0x10010, 8), (0x10024, 16)])

    def test_the_live_delta(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "live.txt")
            with open(p, "w") as f:
                f.write("# entry,ranges\n000186d0,00018625-000186c0;000186d0-00018712\n00010034,00010034-00010034\n")
            self.assertEqual(T.load_live(p), {0x186D0: [(0x18625, 0x186C0), (0x186D0, 0x18712)],
                                              0x10034: [(0x10034, 0x10034)]})


@needs_capstone
class DiffEmuHelperTests(unittest.TestCase):
    def test_decode_at_and_disasm_range(self):
        img = E.Image(bytes.fromhex("31C0" "40" "C3" "DDC8"))   # xor eax,eax / inc eax / ret / undecodable
        self.assertEqual(E.decode_at(img, 0x10000).mnemonic, "xor")
        self.assertIsNone(E.decode_at(img, 0x10004))
        self.assertEqual([i.address for i in E.disasm_range(img, 0x10000, 0x10006)], [0x10000, 0x10002, 0x10003])
