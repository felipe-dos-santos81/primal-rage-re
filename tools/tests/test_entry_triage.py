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
    # a slot of two tables of different kinds: the jump table 80300 (read first, lower address) and the
    # call table 80800 (read by the Ghidra function 18300); the call slot names it (§E2.2: call before jmp)
    w.fn(0x18300, bytes.fromhex("FF1485") + le32(0x80800) + b"\xc3")
    w.entry(0x17300, XOR_RET)
    w.put(0x80304, le32(0x17300))
    w.put(0x80800, le32(0x17300))
    # above the cut: a slot of the call table 80200 (first found) and of the span table 80600 (names it)
    w.entry(0x5D0A0, XOR_RET)
    w.put(0x80208, le32(0x5D0A0))
    w.put(0x80604, le32(0x5D0A0))
    return w


EXPECTED = {
    0x10200: "code-immediate", 0x10300: "data", 0x10400: "direct", 0x10500: "interior",
    0x12000: "finisher", 0x12100: "move-callback", 0x1210F: "mid-instruction", 0x12113: "interior",
    0x14000: "span-writer", 0x14100: "span-writer", 0x14200: "span-writer", 0x15000: "call-table",
    0x15100: "jump-table", 0x16000: "anim-target", 0x16100: "anim-target", 0x16200: "dead",
    0x16300: "unclassified", 0x16400: "data-pointer", 0x16500: "data", 0x16600: "data",
    0x16900: "data-pointer", 0x16B00: "dead", 0x17204: "interior", 0x17300: "call-table",
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


def classified(w=None):
    t = world_triage(w)
    cands = t.candidates()
    t.load_tables()
    t.build(cands)
    return t, {v: t.classify(v) for v in cands}


@needs_capstone
class ClassTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t, cls.got = classified()

    def test_every_planted_candidate_gets_its_class(self):
        self.assertEqual({a: c for a, (c, _) in self.got.items()}, EXPECTED)

    def test_the_evidence_names_the_proving_address(self):
        ev = {a: e for a, (_, e) in self.got.items()}
        self.assertEqual(ev[0x12000], "dword BDAE4")
        self.assertEqual(ev[0x12100], "dword A3F8C (char 2, reaction 0x05)")
        self.assertEqual(ev[0x14000], "dword 80100 = table 80100[0], read by `call` at 51E5C")
        self.assertEqual(ev[0x14100], "dword 80180 = table 80180[0], read by `call` at 14000")
        self.assertEqual(ev[0x14200], "dword 80600 = table 80600[0], read by `call` at 5D025")
        self.assertEqual(ev[0x15100], "dword 80300 = table 80300[0], read by `jmp` at 11100")
        self.assertEqual(ev[0x17300], "dword 80800 = table 80800[0], read by `call` at 18300")
        self.assertEqual(ev[0x16000], "dword 90002 after the opcode word D100 at 90000")
        self.assertEqual(ev[0x16100], "dword 90014 after the opcode word DF11 at 90010")
        self.assertEqual(ev[0x1210F], "inside the instruction at 1210C (code of 12100)")
        self.assertEqual(ev[0x12113], "an instruction of the code of 12100")
        self.assertEqual(ev[0x10500], "`jge` at 10011 in the code of 10000")
        self.assertEqual(ev[0x10400], "`call` at 1000C")
        self.assertEqual(ev[0x10300], "memory operand of the instruction at 10007")
        self.assertEqual(ev[0x10200], "immediate of the instruction at 10000")
        self.assertEqual(ev[0x16500], "filler at 16500 (8bc031)")
        self.assertEqual(ev[0x16600], "undecodable bytes on the scan from 16600")
        self.assertEqual(ev[0x16400], "aligned dword 90040")
        self.assertEqual(ev[0x17204], "an instruction of the scan from 17200, which only the rel32 at 17020 "
                                      "(untrusted bytes) reaches")
        self.assertEqual(ev[0x16300], "rel32 at 17000 outside the trusted code; unaligned dwords 90032")
        self.assertTrue(ev[0x16200].startswith("no rel32 at any code offset"), ev[0x16200])

    def test_the_table_classes_win_over_a_plain_aligned_dword(self):
        # 0x12000 is also the aligned dword at 0x80400; without the finisher table it is a data pointer
        w = build_world()
        w.put(0xBDAE4, le32(0))
        self.assertEqual(classified(w)[1][0x12000][0], "data-pointer")

    def test_a_span_table_read_above_the_runtime_cut_is_found(self):
        # 5D020 is trusted as a slot of the span table 80100 although it lies above RUNTIME_BASE
        self.assertIn(0x5D020, self.t.entries)
        self.assertEqual(self.t.span_tables, {0x80100, 0x80180, 0x80600})

    def test_an_entry_whose_evidence_the_final_index_lost_is_stale(self):
        self.assertEqual(self.t.stale, [0x5D040])
        self.assertIsNone(self.t.reason(0x5D040))
        self.assertEqual(self.t.entries[0x5D040], "slot 80108 of table 80100, read by `call` at 51E5C")

    def test_above_the_cut_only_span_code_is_trusted(self):
        self.assertNotIn(0x5D060, self.t.entries)      # slot 80204 of the call table 80200
        self.assertNotIn(0x5D080, self.t.entries)      # called by the Ghidra function 18200
        self.assertIn(0x5D060, self.t.slots)
        self.assertIn(0x5D080, self.t.calls)

    def test_the_untrusted_entries(self):
        self.assertEqual(self.t.untrusted(), {0x16300: [0x17000], 0x17200: [0x17020]})


@needs_capstone
class SwitchTests(unittest.TestCase):
    def test_a_bounded_switch_extends_the_body_and_an_unbounded_one_stays_indirect(self):
        w = World()
        # 10000: cmp al,1 / ja 0x10010 / and eax,0xff / jmp dword [eax*4+0x80000]; 10010: ret
        w.put(0x10000, bytes.fromhex("3C01" "770C" "25FF000000" "FF2485") + le32(0x80000))
        w.put(0x10010, b"\xc3")
        w.put(0x80000, le32(0x10020) + le32(0x10030) + le32(0x10040))
        w.put(0x10020, bytes.fromhex("31C0C3"))
        w.put(0x10030, bytes.fromhex("40C3"))
        w.put(0x10040, bytes.fromhex("48C3"))          # case 2: beyond `cmp al,1`, not part of the body
        # 10100: the same jmp with no guard
        w.put(0x10100, bytes.fromhex("FF2485") + le32(0x80000))
        t = T.Triage(w.image(), [])
        t.bodies = {}
        body = t.scan(0x10000)
        self.assertIn(0x10020, body.insns)
        self.assertIn(0x10030, body.insns)
        self.assertNotIn(0x10040, body.insns)
        self.assertEqual(body.indirect, [])
        self.assertEqual(t.scan(0x10100).indirect, [0x10100])


LIVE = {0x12000: [(0x12000, 0x12002)], 0x103F0: [(0x103F0, 0x10410)]}


@needs_capstone
class RowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        w = build_world()
        cls.t = T.Triage(w.image(), sorted(w.ghidra), {0x10200}, {T.VOICE_FN}, LIVE)
        cls.rows = {r["addr"]: r for r in cls.t.run()}
        cls.supp = {s["addr"]: s for s in cls.t.supplement()}

    def test_run_keeps_the_classes(self):
        self.assertEqual({a: r["cls"] for a, r in self.rows.items()}, EXPECTED)

    def test_batches_and_targets(self):
        b = {a: r["batch"] for a, r in self.rows.items()}
        self.assertEqual((b[0x12000], b[0x12100], b[0x14000], b[0x16000]),
                         ("finishers", "callbacks", "span-writers", "animation-targets"))
        self.assertEqual((b[0x15000], b[0x10400], b[0x16900]), ("voice", "other", "other"))
        for a in (0x10300, 0x10500, 0x1210F, 0x12113, 0x16200, 0x16300, 0x16500, 0x16600, 0x17204):
            self.assertEqual(b[a], "-", "%X" % a)
            self.assertEqual(self.rows[a]["e1"], "-", "%X" % a)

    def test_the_voice_column_names_the_site_for_targets_only(self):
        self.assertEqual(self.rows[0x15000]["voice"], [0x15000])
        self.assertEqual(self.rows[0x17204]["voice"], [])      # its scan reaches 17205's call: not a target

    def test_the_u0_column(self):
        self.assertFalse(self.rows[0x16900]["u0"])
        self.assertEqual(sum(1 for r in self.rows.values() if r["u0"]), len(EXPECTED) - 1)

    def test_size_runs_to_the_next_candidate_or_ghidra_entry(self):
        self.assertEqual(self.rows[0x10500]["size"], 0x11000 - 0x10500)   # the Ghidra function 11000
        self.assertEqual(self.rows[0x12000]["size"], 0x12100 - 0x12000)   # the candidate 12100

    def test_the_live_column_is_evidence_only(self):
        self.assertEqual(self.rows[0x12000]["live"], "entry")
        self.assertEqual(self.rows[0x10400]["live"], "body of 103F0")
        self.assertEqual(self.rows[0x10200]["live"], "-")
        self.assertEqual(self.rows[0x12000]["cls"], "finisher")

    def test_the_supplement_holds_the_helper_the_stored_entry_and_the_span_code(self):
        self.assertEqual({a: s["why"] for a, s in self.supp.items()},
                         {0x13000: "called at 12100", 0x13100: "immediate at 12105",
                          0x5D020: "slot 80104 of table 80100, read by `call` at 51E5C",
                          0x5D040: "stale: slot 80108 of table 80100, read by `call` at 51E5C",
                          0x5D0A0: "slot 80604 of table 80600, read by `call` at 5D025"})

    def test_e1_readiness(self):
        e1 = {a: r["e1"] for a, r in self.rows.items()}
        self.assertEqual(e1[0x10200], "leaf")
        self.assertEqual(e1[0x15000], "allow-list")
        self.assertEqual(e1[0x12100], "callees (13000)")
        self.assertEqual(e1[0x14000], "stubs (indirect at 14000 in 14000)")
        self.assertEqual(e1[0x15100], "stubs (in in 18000)")
        self.assertEqual(e1[0x16000], "stack-args")

    def test_ported_rows(self):
        self.assertTrue(self.rows[0x10200]["ported"])
        self.assertFalse(self.rows[0x10400]["ported"])

    def test_voice_sites_are_placed(self):
        v = {x["site"]: (x["entry"], x["kind"]) for x in self.t.voice_placement(list(self.rows.values()),
                                                                             list(self.supp.values()))}
        self.assertEqual(v, {0x15000: (0x15000, "row"), 0x17205: (0x17200, "untrusted"), 0x17010: (None, "-")})


@needs_capstone
class PortedRoutingTests(unittest.TestCase):
    """is_ported routes a Ghidra entry to the port_progress rule and any other address to the strict
    rule (§E2.4); the two sets disagree on purpose so a mixed-up lookup is visible."""

    @classmethod
    def setUpClass(cls):
        w = build_world()
        # strict: 10200 (non-Ghidra) and 18000 (a Ghidra entry); loose: VOICE_FN and 18100 (Ghidra
        # entries) and 10400 (non-Ghidra, a comment that is not at column 0)
        cls.t = T.Triage(w.image(), sorted(w.ghidra), {0x10200, 0x18000}, {T.VOICE_FN, 0x18100, 0x10400})

    def test_a_non_ghidra_address_follows_the_strict_rule(self):
        self.assertTrue(self.t.is_ported(0x10200))
        self.assertFalse(self.t.is_ported(0x10400))        # only in the loose set
        self.assertFalse(self.t.is_ported(0x10300))        # in neither

    def test_a_ghidra_entry_follows_the_loose_rule(self):
        self.assertTrue(self.t.is_ported(T.VOICE_FN))
        self.assertTrue(self.t.is_ported(0x18100))
        self.assertFalse(self.t.is_ported(0x18000))        # only in the strict set
        self.assertFalse(self.t.is_ported(0x18200))        # in neither

    def test_the_rows_carry_the_same_routing(self):
        rows = {r["addr"]: r["ported"] for r in self.t.run()}
        self.assertEqual((rows[0x10200], rows[0x10400], rows[0x10300]), (True, False, False))


@needs_capstone
class TableCapTests(unittest.TestCase):
    def test_a_table_walk_stops_at_max_table_slots(self):
        w = World()
        w.fn(0x10000, bytes.fromhex("FF1485") + le32(0x81000) + b"\xc3")    # call dword [eax*4+0x81000]; ret
        n = T.MAX_TABLE + 8
        vals = [0x20000 + 0x10 * i for i in range(n)]                      # code addresses, no displacement stop
        w.put(0x81000, b"".join(le32(v) for v in vals))
        t = T.Triage(w.image(), sorted(w.ghidra))
        t.cands, t.cset = [], set()
        t.build([])
        self.assertEqual(t.slots[vals[T.MAX_TABLE - 1]][0][2], T.MAX_TABLE - 1)    # slot 511: in
        self.assertNotIn(vals[T.MAX_TABLE], t.slots)                                # slot 512: out
        self.assertNotIn(vals[T.MAX_TABLE + 7], t.slots)


@needs_capstone
class CliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        d = self.tmp.name
        w = build_world()
        self.img = os.path.join(d, "image.bin")
        with open(self.img, "wb") as f:
            f.write(bytes(w.data))
        self.csv = os.path.join(d, "functions.csv")
        with open(self.csv, "w") as f:
            f.write("entry,size,name,n_callers,n_callees,decompiled\n")
            for s, n in sorted(w.ghidra):
                f.write("%08x,%d,FUN_%08x,0,0,ok\n" % (s, n, s))
        self.src = os.path.join(d, "src")
        os.mkdir(self.src)
        with open(os.path.join(self.src, "a.c"), "w") as f:
            f.write("/* 0x10200 — record */\n/* 0x2C3FC — record */\n")
        self.out = os.path.join(d, "table.md")

    def tearDown(self):
        self.tmp.cleanup()

    def main(self, *extra):
        import contextlib
        import io
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = T.main(["--image", self.img, "--functions", self.csv, "--src", self.src] + list(extra))
        return rc, buf.getvalue()

    def test_the_counts_the_table_and_the_check(self):
        # 24 = len(EXPECTED): fix round 1 (3be31c4) planted 17300 for the slot-precedence rule after the
        # brief counted 23.
        rc, text = self.main("--out", self.out, "--expect", "24", "--expect-u0", "23")
        self.assertEqual(rc, 0, text)
        self.assertIn("entry-triage: 24 candidates (23 by U0's rule)", text)
        with open(self.out) as f:
            table = f.read()
        self.assertIn("| 15000 | 256 | call-table | dword 80200 = table 80200[0], read by `call` at 11000 | yes | - "
                      "| no | voice | allow-list | 15000 |", table)
        self.assertIn("| 16900 | 512 | data-pointer | aligned dword 90064 | no | - | no | other | leaf | - |", table)
        self.assertIn("| 10200 | 256 | code-immediate | immediate of the instruction at 10000 | yes | - | yes | other "
                      "| leaf | - |", table)
        self.assertIn("| 17205 | 17200 | untrusted | no |", table)
        self.assertIn("| 17200 | 17020 | - | no |", table)
        self.assertEqual(self.main("--check", self.out)[0], 0)
        with open(self.out, "a") as f:
            f.write("edited\n")
        rc, text = self.main("--check", self.out)
        self.assertEqual(rc, 1)
        self.assertIn("differs from a fresh run", text)

    def test_a_wrong_expected_count_fails(self):
        self.assertEqual(self.main("--expect", "23")[0], 1)
        self.assertEqual(self.main("--expect-u0", "24")[0], 1)


DIFFRUN = os.path.join(ROOT, "build", "diffrun")
EXE = os.path.join(os.environ.get("PR_GAME_DIR", os.path.join(ROOT, "data", "game", "C")), "PRAGE.EXE")
LIVE_FILE = os.path.join(ROOT, "docs", "superpowers", "plans", "2026-10-01-reverse-e2-live-functions.txt")
# U6a ported four non-Ghidra addresses with strict headers after the planning run (record gameplay-u6 §U6.2
# 0x23208, §U6.3 0x3A588, §U6.4 0x3640C, §U6.5 0x37DCC; commits e3a5d77, f9fbfc7, 7aea1f8, 396229d). Three are rows
# of the table (0x23208 a move-callback, 0x3640C and 0x37DCC animation targets); 0x3A588 follows data, not a
# `ret`, so it is not in the universe. They are the only differences from the planning run's "ported" figures.
U6A_ROWS = {0x23208, 0x3640C, 0x37DCC}
U0_CALLBACKS = {int(x, 16) for x in (
    "14EF8 14F50 15478 21114 21374 22938 22A00 231C0 23208 237D0 2381C 3C048 3D10C 3D1EC 3DADC 3DB34 "
    "3DCEC 3F0A8 475EC 47608 47624 47720 47874 47FCC 48608 48964 489A0").split()}
U0_FINISHERS = {0x1567C, 0x15908, 0x23BF8, 0x23EC0, 0x402FC, 0x45D14}
U0_VOICE = {int(x, 16) for x in (
    "11A3D 11C38 14F46 14F9E 154DD 1550B 155EA 156CA 15780 15802 158CD 15956 1599B 212AF 2236D 223EF "
    "224E2 2260E 228EF 2292B 22AA7 231FB 23243 23810 2385C 23BD8 23E9D 23F05 24001 2406D 24214 242DA "
    "24317 243CF 243ED 2455E 295FD 342EF 3440D 3448B 34517 345A3 3462F 3D12D 3D395 3D730 3DAC5 3DB2A "
    "3DB82 3F0C1 4012D 40137 402B1 402E0 41880 45C8C 45D0A 475D9 4779D 478C7 47DF7 47E1B 48518 48548 "
    "4B0B4 4B0BE").split()}
# Measured on the image `diffrun --image-out` writes at e9271df (record §E2.9). A change of a class
# rule moves these: update them with the record, never alone.
REAL_CLASSES = {"finisher": 9, "move-callback": 71, "span-writer": 231, "call-table": 7, "anim-target": 112,
                "mid-instruction": 3, "data": 75, "code-immediate": 15, "direct": 2, "interior": 6,
                "data-pointer": 48}


@needs_capstone
@unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
                     "build/diffrun or PRAGE.EXE absent")
class RealImageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        img = os.path.join(cls.tmp.name, "image.bin")
        subprocess.run([DIFFRUN, "--exe", EXE, "--image-out", img], check=True, capture_output=True)
        strict, loose = T.load_ported(os.path.join(ROOT, "port", "src"))
        ghidra = T.load_ghidra(os.path.join(ROOT, "port", "decomp", "prage.functions.csv"))
        cls.t = T.Triage(E.Image.load(img), ghidra, strict, loose & {s for s, _ in ghidra}, T.load_live(LIVE_FILE))
        cls.rows = cls.t.run()
        cls.by = {r["addr"]: r for r in cls.rows}
        cls.supp = cls.t.supplement()

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_the_universe_is_579_and_u0s_rule_gives_575(self):
        self.assertEqual(len(self.rows), 579)
        self.assertEqual(sum(1 for r in self.rows if r["u0"]), 575)
        self.assertEqual({r["addr"] for r in self.rows if not r["u0"]}, {0x10602, 0x10604, 0x36114, 0x3C87C})

    def test_the_class_counts(self):
        self.assertEqual(dict(collections.Counter(r["cls"] for r in self.rows)), REAL_CLASSES)

    def test_the_unported_callbacks_and_finishers_are_u0s(self):
        un = lambda c: {r["addr"] for r in self.rows if r["cls"] == c and not r["ported"]}
        self.assertEqual(un("move-callback"), U0_CALLBACKS - {0x23208})       # 0x23208: ported by U6a (§U6.2)
        self.assertEqual(un("finisher"), U0_FINISHERS)

    def test_u0s_animation_examples_are_animation_targets(self):
        for a in (0x241A8, 0x37DCC, 0x400E0):
            self.assertEqual(self.by[a]["cls"], "anim-target", "%X" % a)

    def test_no_entry_is_stale(self):
        self.assertEqual(self.t.stale, [])

    def test_the_span_tables(self):
        self.assertEqual(self.t.span_tables, {0x80C8C, 0x80D0C, 0x80E0C, 0x81010, 0x81110})

    def test_the_live_column(self):
        self.assertEqual(collections.Counter(r["live"].split(" ")[0] for r in self.rows),
                         {"-": 553, "entry": 22, "body": 4})

    def test_the_voice_sites(self):
        sites = self.t.rel32_anywhere(T.VOICE_FN)
        self.assertEqual(len(sites), 303)               # k7-k12 §0.2
        placed = {v["site"]: v for v in self.t.voice_placement(self.rows, self.supp)}
        self.assertTrue(U0_VOICE <= set(placed))
        self.assertEqual(collections.Counter((placed[s]["kind"], placed[s]["ported"]) for s in U0_VOICE),
                         {("row", False): 39, ("row", True): 1, ("supplement", False): 9, ("-", False): 17})
        # the one row site that moved to ported is 0x23243, in the body of 0x23208 (ported by U6a, §U6.2)
        self.assertEqual((placed[0x23243]["entry"], placed[0x23243]["kind"], placed[0x23243]["ported"]),
                         (0x23208, "row", True))

    def test_the_four_u6a_ports(self):
        self.assertEqual({a for a in U6A_ROWS if self.by[a]["ported"]}, U6A_ROWS)
        self.assertNotIn(0x3A588, self.by)                  # follows data, not a ret: outside the universe
        self.assertTrue(self.t.is_ported(0x3A588))          # but its strict header is there
        self.assertEqual(sum(1 for r in self.rows if r["batch"] != "-" and r["ported"]), 166)   # 163 + U6a's 3
