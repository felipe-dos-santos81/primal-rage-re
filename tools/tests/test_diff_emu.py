"""Unit tests for tools/diff_emu.py on small hand-assembled images (no PRAGE.EXE needed).

Every program is listed with its assembly. Memory is seeded with values that differ from what the
program writes (a write of the value already there, or an unseeded zero, would prove nothing).
"""
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import diff_emu as E

REQUIRED = os.environ.get("PR_ORACLE_REQUIRED") == "1"


def le32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


def image(code, at=0x10000):
    """An image from 0x10000 to 0x90100 (so 0x80000 is inside it) with `code` placed at `at`."""
    data = bytearray(0x80100)
    data[at - 0x10000:at - 0x10000 + len(code)] = code
    return E.Image(bytes(data))


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed (pip install -r tools/requirements-diff.txt)")
class RunOriginalTests(unittest.TestCase):
    def test_unicorn_is_installed_when_required(self):
        self.assertTrue(E.available())

    # 10000: mov eax,[0x80000]; add eax,5; mov [0x80004],eax; ret
    ADD5 = bytes.fromhex("A1000008" "00" "0505000000" "A304000800" "C3")

    def test_returns_eax_and_reports_every_changed_byte(self):
        img = image(self.ADD5)
        pokes = {0x80000: le32(0x10), 0x80004: le32(0xFFFFFFFF)}   # seeded: all 4 bytes will change
        r = E.run_original(img, 0x10000, pokes=pokes)
        self.assertEqual(r.outcome, "ok")
        self.assertEqual(r.regs["eax"], 0x15)
        self.assertEqual(r.writes, {0x80004: 0x15, 0x80005: 0, 0x80006: 0, 0x80007: 0})

    def test_a_write_of_the_value_already_there_is_not_a_change(self):
        img = image(self.ADD5)
        pokes = {0x80000: le32(0x10), 0x80004: le32(0x15)}
        self.assertEqual(E.run_original(img, 0x10000, pokes=pokes).writes, {})

    def test_registers_are_inputs(self):
        # 10000: lea eax,[eax+edx*2]; ret
        img = image(bytes.fromhex("8D0450" "C3"))
        r = E.run_original(img, 0x10000, regs={"eax": 100, "edx": 7})
        self.assertEqual(r.regs["eax"], 114)

    def test_stack_traffic_is_not_a_write(self):
        # 10000: push eax; pop eax; ret
        r = E.run_original(image(bytes.fromhex("50" "58" "C3")), 0x10000, regs={"eax": 9})
        self.assertEqual((r.outcome, r.writes), ("ok", {}))

    def test_an_invalid_instruction_is_a_fault(self):
        r = E.run_original(image(bytes.fromhex("0F0B")), 0x10000)          # ud2
        self.assertEqual(r.outcome, "fault")

    def test_int_is_unmodeled_and_named(self):
        r = E.run_original(image(bytes.fromhex("CD21" "C3")), 0x10000)      # int 0x21
        self.assertEqual(r.outcome, "unmodeled")
        self.assertIn("int at 0x10000", r.detail)

    def test_port_io_is_unmodeled(self):
        r = E.run_original(image(bytes.fromhex("EC" "C3")), 0x10000)        # in al,dx
        self.assertEqual((r.outcome, r.detail), ("unmodeled", "in at 0x10000"))

    def test_a_loop_that_never_returns_times_out(self):
        r = E.run_original(image(bytes.fromhex("EBFE")), 0x10000, max_insns=1000)   # jmp $
        self.assertEqual(r.outcome, "timeout")

    # 10000: call 0x10010; ret      10010: mov eax,7; ret
    def call_image(self):
        code = bytearray(0x20)
        code[0:6] = bytes.fromhex("E80B000000" "C3")
        code[0x10:0x16] = bytes.fromhex("B807000000" "C3")
        return image(bytes(code))

    def test_a_call_outside_the_allow_list_is_unmodeled(self):
        r = E.run_original(self.call_image(), 0x10000)
        self.assertEqual(r.outcome, "unmodeled")
        self.assertIn("call 0x10010 from 0x10000", r.detail)

    def test_an_allowed_call_runs_the_callee_from_the_original_bytes(self):
        r = E.run_original(self.call_image(), 0x10000, allow_calls={0x10010})
        self.assertEqual((r.outcome, r.regs["eax"]), ("ok", 7))

    def test_an_indirect_call_is_unmodeled(self):
        r = E.run_original(image(bytes.fromhex("FFD0" "C3")), 0x10000, regs={"eax": 0x10010})   # call eax
        self.assertEqual((r.outcome, r.detail), ("unmodeled", "indirect call at 0x10000"))

    def test_reads_outside_the_image_are_reported_and_read_zero(self):
        # 10000: mov eax,[0x1000000]; ret
        r = E.run_original(image(bytes.fromhex("A100000001" "C3")), 0x10000, regs={"eax": 0xFFFFFFFF})
        self.assertEqual((r.outcome, r.regs["eax"]), ("ok", 0))
        self.assertEqual(r.outside, [(0x1000000, 4)])

    def test_a_poke_outside_the_image_is_rejected(self):
        with self.assertRaises(ValueError):
            E.run_original(image(bytes.fromhex("C3")), 0x10000, pokes={0x2000000: b"\x01"})

    def test_runs_are_independent(self):
        img = image(self.ADD5)
        pokes = {0x80000: le32(1), 0x80004: le32(0xFFFFFFFF)}
        a = E.run_original(img, 0x10000, pokes=pokes)
        b = E.run_original(img, 0x10000, pokes=pokes)
        self.assertEqual((a.regs, a.writes), (b.regs, b.writes))


# 10000: cmp eax,0; je 10008; inc eax; jmp 10009; (10008) dec eax; (10009) ret
DIAMOND = bytes.fromhex("83F800" "7403" "40" "EB01" "48" "C3")


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class StaticScanTests(unittest.TestCase):
    def test_blocks_of_a_diamond(self):
        info = E.static_scan(image(DIAMOND), 0x10000)
        self.assertEqual(info.leaders, [0x10000, 0x10005, 0x10008, 0x10009])
        self.assertEqual((info.indirect, info.truncated), ([], False))

    def test_coverage_reports_the_unhit_block(self):
        img = image(DIAMOND)
        info = E.static_scan(img, 0x10000)
        taken = E.run_original(img, 0x10000, regs={"eax": 0})       # je taken: skips 0x10005
        hit, unhit = E.coverage(info, taken.executed)
        self.assertEqual((hit, unhit), ([0x10000, 0x10008, 0x10009], [0x10005]))
        other = E.run_original(img, 0x10000, regs={"eax": 1})
        _, unhit2 = E.coverage(info, taken.executed | other.executed)
        self.assertEqual(unhit2, [])

    def test_an_indirect_jump_is_flagged(self):
        info = E.static_scan(image(bytes.fromhex("FFE0")), 0x10000)    # jmp eax
        self.assertEqual(info.indirect, [0x10000])

    def test_a_direct_target_outside_the_image_is_unresolved(self):
        # jmp 0x7000000
        rel = (0x7000000 - 0x10005) & 0xFFFFFFFF
        info = E.static_scan(image(b"\xE9" + le32(rel)), 0x10000)
        self.assertEqual(info.unresolved, [0x7000000])

    def test_a_tail_jump_is_followed_into_the_next_function(self):
        # 10000: jmp 0x10010      10010: xor eax,eax; ret
        code = bytearray(0x20)
        code[0:5] = b"\xE9" + le32(0x10010 - 0x10005)
        code[0x10:0x13] = bytes.fromhex("31C0" "C3")
        info = E.static_scan(image(bytes(code)), 0x10000)
        self.assertEqual(info.leaders, [0x10000, 0x10010])

    def test_a_back_edge_terminates_and_makes_the_target_a_leader(self):
        # 10000: dec ecx; 10001: jnz 0x10000; 10003: ret
        info = E.static_scan(image(bytes.fromhex("49" "75FD" "C3")), 0x10000)
        self.assertEqual(info.leaders, [0x10000, 0x10003])
        self.assertEqual(sorted(info.insns), [0x10000, 0x10001, 0x10003])

    def test_the_instruction_budget_marks_the_scan_truncated(self):
        img = image(b"\x90" * 20 + b"\xC3")
        cut = E.static_scan(img, 0x10000, max_insns=5)
        self.assertEqual((cut.truncated, len(cut.insns)), (True, 5))
        full = E.static_scan(img, 0x10000, max_insns=1000)
        self.assertEqual((full.truncated, len(full.insns)), (False, 21))

    def test_loop_targets_and_fall_through_are_leaders(self):
        # 10000: loop 0x10003; 10002: ret; 10003: ret  (10003 is reachable only through the loop)
        info = E.static_scan(image(bytes.fromhex("E201" "C3" "C3")), 0x10000)
        self.assertEqual(info.leaders, [0x10000, 0x10002, 0x10003])
        for op in ("E1", "E0"):                                      # loope, loopne
            info = E.static_scan(image(bytes.fromhex(op + "01" "C3" "C3")), 0x10000)
            self.assertEqual(info.leaders, [0x10000, 0x10002, 0x10003])

    def test_undecodable_bytes_mark_the_scan_truncated(self):
        # 0F 04 is not an instruction in capstone's 32-bit x86 decoder
        info = E.static_scan(image(bytes.fromhex("0F04")), 0x10000)
        self.assertEqual((info.truncated, info.insns), (True, {}))


if __name__ == "__main__":
    unittest.main()
