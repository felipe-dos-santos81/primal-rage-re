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


class AvailabilityTests(unittest.TestCase):
    def test_both_unicorn_and_capstone_are_required(self):
        # the harness skips (spec 5.5) when either is missing; capstone used to be imported
        # unconditionally, so a host without it errored instead of skipping
        from unittest import mock
        with mock.patch.object(E, "capstone", None):
            self.assertFalse(E.available())
        with mock.patch.object(E, "unicorn", None):
            self.assertFalse(E.available())
        with mock.patch.object(E, "capstone", None), self.assertRaises(RuntimeError):
            E.run_original(E.Image(bytes(0x100)), 0x10000)


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

    def test_a_write_in_the_range_the_emulator_stack_used_to_occupy_is_a_write(self):
        # The stack used to sit at 0x3F00000.., inside the port's mem[] range, so an original write
        # there was dropped from the diff. It is now outside MEM_SIZE and every write below
        # MEM_SIZE is diffed. 10000: mov [0x3F80000],eax; ret   (the target holds 0 before)
        r = E.run_original(image(bytes.fromhex("A3" "0000F803" "C3")), 0x10000, regs={"eax": 0x12345678})
        self.assertEqual(r.writes, {0x3F80000: 0x78, 0x3F80001: 0x56, 0x3F80002: 0x34, 0x3F80003: 0x12})
        self.assertEqual(r.outside, [(0x3F80000, 4)])      # outside the image, so it is also reported

    def test_the_private_stack_is_outside_the_port_memory_range(self):
        self.assertGreaterEqual(E.STACK_LOW, E.MEM_SIZE)
        self.assertGreater(E.SENTINEL, E.STACK_LOW)
        self.assertLess(E.STACK_TOP, E.SENTINEL)

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



# ---- E3: call stubs and stack arguments (record 2026-10-01-reverse-e3 §E3.2, §E3.5) ------------

def program(parts):
    """An image from {address: hex bytes}."""
    data = bytearray(0x80100)
    for at, h in parts.items():
        b = bytes.fromhex(h)
        data[at - 0x10000:at - 0x10000 + len(b)] = b
    return E.Image(bytes(data))


# 10000: mov eax,5; mov edx,7; call 0x10020; mov [0x80000],eax; ret
# 10020: mov dword [0x80004],0x11111111; mov eax,9; ret
CALLER = {0x10000: "B805000000" "BA07000000" "E811000000" "A300000800" "C3",
          0x10020: "C70504000800" "11111111" "B809000000" "C3"}
SEED = {0x80000: le32(0xFFFFFFFF), 0x80004: le32(0x22222222)}


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class CallStubTests(unittest.TestCase):
    def test_a_stubbed_call_is_recorded_and_its_bytes_do_not_run(self):
        r = E.run_original(program(CALLER), 0x10000, pokes=SEED,
                           calls=(E.Call(0x10020, ("eax", "edx"), eax=0x42),))
        self.assertEqual((r.outcome, r.calls), ("ok", [(0x10020, (5, 7))]))
        self.assertEqual(r.writes, {0x80000: 0x42, 0x80001: 0, 0x80002: 0, 0x80003: 0})   # 0x80004 untouched
        self.assertNotIn(0x1002A, r.executed)

    def test_a_real_call_runs_the_callee_and_is_recorded(self):
        r = E.run_original(program(CALLER), 0x10000, pokes=SEED,
                           calls=(E.Call(0x10020, ("eax",), mode="real"),))
        self.assertEqual((r.outcome, r.calls, r.regs["eax"]), ("ok", [(0x10020, (5,))], 9))
        self.assertEqual(r.writes[0x80004], 0x11)

    def test_stub_writes_land_at_an_address_or_at_an_argument_plus_offset(self):
        c = E.Call(0x10020, ("eax", "edx"), writes=((None, 0x80008, b"\xab"), (0, 0x80010 - 5, b"\xcd")))
        r = E.run_original(program(CALLER), 0x10000, pokes=SEED, calls=(c,))
        self.assertEqual((r.writes[0x80008], r.writes[0x80010]), (0xAB, 0xCD))

    def test_a_stub_write_outside_the_image_stops_the_run(self):
        c = E.Call(0x10020, ("eax",), writes=((None, 0x2000000, b"\x01"),))
        r = E.run_original(program(CALLER), 0x10000, calls=(c,))
        self.assertEqual(r.outcome, "unmodeled")
        self.assertIn("outside the image", r.detail)

    # 10000: push 0x33; call 0x10020; ret      10020: mov eax,[esp+4]; ret 4
    PUSHER = {0x10000: "6A33" "E819000000" "C3", 0x10020: "8B442404" "C20400"}

    def test_a_stub_pops_its_stack_arguments_and_records_them(self):
        r = E.run_original(program(self.PUSHER), 0x10000, calls=(E.Call(0x10020, ("s0",), pop=4),))
        self.assertEqual((r.outcome, r.calls), ("ok", [(0x10020, (0x33,))]))
        r = E.run_original(program(self.PUSHER), 0x10000, calls=(E.Call(0x10020, ("s0",), pop=0),),
                           max_insns=1000)
        self.assertNotEqual(r.outcome, "ok")      # the caller's ret then pops 0x33 as its return address

    def test_a_case_sets_the_stack_arguments(self):
        r = E.run_original(program({0x10000: "8B442404" "C20400"}), 0x10000, regs={"s0": 0x1234})
        self.assertEqual((r.outcome, r.regs["eax"]), ("ok", 0x1234))

    def test_an_indirect_call_through_a_register_is_resolved_at_run_time(self):
        r = E.run_original(program({0x10000: "FFD0" "C3", 0x10020: "C3"}), 0x10000, regs={"eax": 0x10020},
                           calls=(E.Call(0x10020, ("eax",)),))
        self.assertEqual((r.outcome, r.calls), ("ok", [(0x10020, (0x10020,))]))

    def test_an_indirect_call_through_a_table_is_resolved_at_run_time(self):
        # 10000: call [ebx*4 + 0x80000]; ret
        r = E.run_original(program({0x10000: "FF149D00000800" "C3", 0x10020: "C3"}), 0x10000,
                           regs={"ebx": 1}, pokes={0x80004: le32(0x10020)}, calls=(E.Call(0x10020, ("ebx",)),))
        self.assertEqual((r.outcome, r.calls), ("ok", [(0x10020, (1,))]))

    def test_an_indirect_call_to_an_unlisted_target_names_the_target(self):
        r = E.run_original(program({0x10000: "FFD0" "C3"}), 0x10000, regs={"eax": 0x10030},
                           calls=(E.Call(0x10020),))
        self.assertEqual((r.outcome, r.detail), ("unmodeled", "indirect call to 0x10030 at 0x10000"))

    def test_an_indirect_call_to_an_allowed_target_runs(self):
        r = E.run_original(program({0x10000: "FFD0" "C3", 0x10020: "B809000000" "C3"}), 0x10000,
                           regs={"eax": 0x10020}, allow_calls={0x10020})
        self.assertEqual((r.outcome, r.regs["eax"], r.calls), ("ok", 9, []))

    def test_a_tail_jump_to_a_stubbed_entry_returns_to_the_caller(self):
        r = E.run_original(program({0x10000: "E91B000000", 0x10020: "CC"}), 0x10000, regs={"eax": 3},
                           calls=(E.Call(0x10020, ("eax",), eax=8),))
        self.assertEqual((r.outcome, r.calls, r.regs["eax"]), ("ok", [(0x10020, (3,))], 8))

    def test_falling_into_a_call_set_address_is_not_a_call(self):
        r = E.run_original(program({0x10000: "90" "C3"}), 0x10000, calls=(E.Call(0x10001),))
        self.assertEqual((r.outcome, r.calls), ("ok", []))

    def test_a_call_declaration_is_checked(self):
        for kw in ({"args": ("eax", "foo")}, {"mode": "skip"}, {"mode": "real", "eax": 1},
                   {"mode": "real", "pop": 4}):
            with self.subTest(kw=kw), self.assertRaises(ValueError):
                E.Call(0x10020, **kw)


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class CallScanTests(unittest.TestCase):
    def test_a_jump_to_a_call_set_address_is_a_tail_call_not_scanned(self):
        img = program({0x10000: "E91B000000", 0x10020: "40" "C3"})
        self.assertEqual(sorted(E.static_scan(img, 0x10000).insns), [0x10000, 0x10020, 0x10021])
        self.assertEqual(sorted(E.static_scan(img, 0x10000, stop=[0x10020]).insns), [0x10000])


if __name__ == "__main__":
    unittest.main()
