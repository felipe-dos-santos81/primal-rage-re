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

    def test_a_write_base_must_be_none_or_an_argument_index(self):
        for args, w in ((("eax",), (1, 0x80000, b"\x01")), (("eax",), (-1, 0x80000, b"\x01")),
                        ((), (0, 0x80000, b"\x01"))):
            with self.subTest(args=args, w=w), self.assertRaises(ValueError):
                E.Call(0x10020, args, writes=(w,))
        E.Call(0x10020, ("eax", "edx"), writes=((1, 0x80000, b"\x01"), (None, 0x80000, b"\x01")))

    # 10000: test eax,eax; jne 10020; (10004) ret      10020: int3 (a stub's bytes never run)
    JCC = {0x10000: "85C0" "751C" "C3", 0x10020: "CC"}

    def test_a_taken_conditional_tail_jump_to_a_stubbed_entry_is_a_call(self):
        r = E.run_original(program(self.JCC), 0x10000, regs={"eax": 1},
                           calls=(E.Call(0x10020, ("eax",), eax=8),))
        self.assertEqual((r.outcome, r.calls, r.regs["eax"]), ("ok", [(0x10020, (1,))], 8))
        r = E.run_original(program(self.JCC), 0x10000, regs={"eax": 0}, calls=(E.Call(0x10020, ("eax",)),))
        self.assertEqual((r.outcome, r.calls), ("ok", []))

    def test_a_not_taken_conditional_jump_falling_into_a_call_set_address_is_not_a_call(self):
        # 10000: test eax,eax; jne 10005; (10004) nop; (10005) ret
        r = E.run_original(program({0x10000: "85C0" "7501" "90" "C3"}), 0x10000, regs={"eax": 0},
                           calls=(E.Call(0x10004),))
        self.assertEqual((r.outcome, r.calls), ("ok", []))

    def test_an_indirect_call_through_an_unmapped_operand_stops_as_unmodeled(self):
        # 10000: call [ebx]; ret   with ebx pointing where nothing is mapped
        r = E.run_original(program({0x10000: "FF13" "C3"}), 0x10000, regs={"ebx": 0x70000000})
        self.assertEqual((r.outcome, r.detail), ("unmodeled", "indirect call at 0x10000"))
        r = E.run_original(program({0x10000: "FF13" "C3"}), 0x10000, regs={"ebx": 0x70000000},
                           calls=(E.Call(0x10020),))
        self.assertEqual((r.outcome, r.detail), ("unmodeled", "indirect call at 0x10000"))


# ---- E3 final review: the memory at each call (I1) and the registers a stub clobbers (I2), §E3.12 ----

# 10000: mov byte [0x80000],1; call 0x10020; mov byte [0x80001],2; call 0x10020; ret    10020: int3
TWO_CALLS = {0x10000: "C60500000800" "01" "E814000000" "C60501000800" "02" "E808000000" "C3", 0x10020: "CC"}
# 10000: mov edx,7; mov ebx,3; call 0x10020; mov [0x80000],edx; mov [0x80004],ebx; ret    10020: int3
READS_EDX = {0x10000: "BA07000000" "BB03000000" "E811000000" "891500000800" "891D04000800" "C3", 0x10020: "CC"}


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class CallMemoryTests(unittest.TestCase):
    def test_each_call_records_the_bytes_changed_so_far(self):
        # the first call sees the first store only; the second sees both stores and the first stub's
        # write; the return addresses the calls push are on the private stack and are not reported
        stub = E.Call(0x10020, writes=((None, 0x80010, b"\x07"),))
        r = E.run_original(program(TWO_CALLS), 0x10000, calls=(stub,))
        self.assertEqual(r.outcome, "ok")
        self.assertEqual(r.call_mem, [{0x80000: 1}, {0x80000: 1, 0x80001: 2, 0x80010: 7}])

    def test_a_store_of_the_value_already_there_is_not_a_change_at_the_call(self):
        r = E.run_original(program(TWO_CALLS), 0x10000, pokes={0x80000: b"\x01"}, calls=(E.Call(0x10020),))
        self.assertEqual(r.call_mem, [{}, {0x80001: 2}])

    def test_a_real_call_records_the_memory_at_its_entry(self):
        r = E.run_original(program(CALLER), 0x10000, pokes=SEED, calls=(E.Call(0x10020, mode="real"),))
        self.assertEqual((r.calls, r.call_mem), ([(0x10020, ())], [{}]))
        self.assertEqual(r.writes[0x80004], 0x11)        # the callee's own store comes after the record


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class ClobberTests(unittest.TestCase):
    def test_a_stub_poisons_the_registers_it_clobbers_and_keeps_the_others(self):
        seed = {0x80000: le32(0xFFFFFFFF), 0x80004: le32(0xFFFFFFFF)}
        r = E.run_original(program(READS_EDX), 0x10000, pokes=seed, calls=(E.Call(0x10020, clobbers=("edx",)),))
        self.assertEqual((r.outcome, r.regs["edx"], r.regs["ebx"]), ("ok", E.CLOBBER_POISON, 3))
        self.assertEqual(r.writes, {**{0x80000 + i: b for i, b in enumerate(le32(E.CLOBBER_POISON))},
                                    0x80004: 3, 0x80005: 0, 0x80006: 0, 0x80007: 0})
        r = E.run_original(program(READS_EDX), 0x10000, pokes=seed, calls=(E.Call(0x10020),))
        self.assertEqual((r.regs["edx"], r.regs["ebx"]), (7, 3))

    def test_a_clobber_declaration_is_checked(self):
        for kw in ({"clobbers": ("eax",)}, {"clobbers": ("esp",)}, {"clobbers": ("s0",)},
                   {"mode": "real", "clobbers": ("edx",)}):
            with self.subTest(kw=kw), self.assertRaises(ValueError):
                E.Call(0x10020, **kw)
        E.Call(0x10020, clobbers=("ebx", "ecx", "edx", "esi", "edi", "ebp"))

    def test_the_clobbered_set_is_written_minus_what_entry_and_every_ret_restore(self):
        # push ebx; xor ebx,ebx; xor ecx,ecx; pop ebx; ret
        self.assertEqual(E.callee_clobbers(program({0x10000: "53" "31DB" "31C9" "5B" "C3"}), 0x10000), ("ecx",))
        # mov dh,1; ret: a write to a high byte clobbers its register
        self.assertEqual(E.callee_clobbers(program({0x10000: "B601" "C3"}), 0x10000), ("edx",))
        # push esi; xor esi,esi; test eax,eax; je 10009; pop esi; ret; (10009) ret: one ret skips the pop
        img = program({0x10000: "56" "31F6" "85C0" "7402" "5E" "C3" "C3"})
        self.assertEqual(E.callee_clobbers(img, 0x10000), ("esi",))
        # push ebx; push esi; xor esi,esi; add esp,0 between the pops; pop esi; pop ebx; ret
        img = program({0x10000: "53" "56" "31F6" "5E" "83C400" "5B" "C3"})
        self.assertEqual(E.callee_clobbers(img, 0x10000), ())

    def test_a_callees_clobbers_count_unless_the_caller_saves_them(self):
        # 10000: push ecx; call 10020; pop ecx; ret      10020: mov edx,1; mov ecx,2; ret
        img = program({0x10000: "51" "E81A000000" "59" "C3", 0x10020: "BA01000000" "B902000000" "C3"})
        self.assertEqual(E.callee_clobbers(img, 0x10020), ("ecx", "edx"))
        self.assertEqual(E.callee_clobbers(img, 0x10000), ("edx",))

    def test_an_indirect_call_clobbers_every_register_not_saved(self):
        self.assertEqual(E.callee_clobbers(program({0x10000: "FFD0" "C3"}), 0x10000),
                         ("ebx", "ecx", "edx", "esi", "edi", "ebp"))
        # push esi; push edi; push ebp; call eax; pop ebp; pop edi; pop esi; ret
        img = program({0x10000: "56" "57" "55" "FFD0" "5D" "5F" "5E" "C3"})
        self.assertEqual(E.callee_clobbers(img, 0x10000), ("ebx", "ecx", "edx"))

    def test_an_unresolved_indirect_jump_or_an_outside_target_clobbers_everything(self):
        # jmp eax: its targets are unknown (fix round 2: no longer skipped silently)
        self.assertEqual(E.callee_clobbers(program({0x10000: "FFE0"}), 0x10000),
                         ("ebx", "ecx", "edx", "esi", "edi", "ebp"))
        # jmp 0x5000, below the image
        self.assertEqual(E.callee_clobbers(program({0x10000: "E9FB4FFFFF"}), 0x10000),
                         ("ebx", "ecx", "edx", "esi", "edi", "ebp"))

    # 10000: push edi; cmp dx,1; ja 10050; xor ebx,ebx; mov bx,dx; jmp [ebx*4+0x10100]
    # 10030: xor esi,esi; pop edi; ret     10040: xor ecx,ecx; pop edi; ret     10050: pop edi; ret
    MOVED = program({0x10000: "57" "6683FA01" "7749" "31DB" "6689D3" "FF249D00010100",
                     0x10030: "31F6" "5F" "C3", 0x10040: "31C9" "5F" "C3", 0x10050: "5F" "C3",
                     0x10100: "30000100" "40000100"})

    def test_a_hand_resolved_jump_table_is_followed_and_without_it_everything_is_clobbered(self):
        # the index is moved from dx to ebx, so switch_cases does not bound it (record §E3.7)
        self.assertEqual(E.callee_clobbers(self.MOVED, 0x10000, {}), ("ebx", "ecx", "edx", "esi", "ebp"))
        self.assertEqual(E.callee_clobbers(self.MOVED, 0x10000, {0x1000C: (0x10001, 0x10100, 2)}),
                         ("ebx", "ecx", "esi"))

    def test_a_jump_into_the_restore_sequence_means_nothing_is_saved(self):
        # push ebx; push esi; xor esi,esi; test eax,eax; jne 10009; pop esi; (10009) pop ebx; ret: saved
        # nothing, so the written ESI and the popped EBX both count
        self.assertEqual(E.callee_clobbers(program({0x10000: "53" "56" "31F6" "85C0" "7501" "5E" "5B" "C3"}),
                                           0x10000), ("ebx", "esi"))
        # push ebx; xor ebx,ebx; test eax,eax; jne 10008; pop ebx; (10008) ret: a jump onto the ret skips the pop
        self.assertEqual(E.callee_clobbers(program({0x10000: "53" "31DB" "85C0" "7501" "5B" "C3"}), 0x10000),
                         ("ebx",))
        # the same jumping to the first pop skips no restore
        self.assertEqual(E.callee_clobbers(program({0x10000: "53" "56" "31F6" "85C0" "7500" "5E" "5B" "C3"}),
                                           0x10000), ())

    def test_mutual_recursion_reaches_the_fixpoint(self):
        # 10000: call 10020; ret      10020: xor esi,esi; call 10000; ret
        img = program({0x10000: "E81B000000" "C3", 0x10020: "31F6" "E8D9FFFFFF" "C3"})
        self.assertEqual(E.callee_clobbers(img, 0x10000), ("esi",))
        self.assertEqual(E.callee_clobbers(img, 0x10020), ("esi",))


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class CallScanTests(unittest.TestCase):
    def test_a_jump_to_a_call_set_address_is_a_tail_call_not_scanned(self):
        img = program({0x10000: "E91B000000", 0x10020: "40" "C3"})
        self.assertEqual(sorted(E.static_scan(img, 0x10000).insns), [0x10000, 0x10020, 0x10021])
        self.assertEqual(sorted(E.static_scan(img, 0x10000, stop=[0x10020]).insns), [0x10000])



# 10000: cmp al,2; ja 10010; and eax,0xff; jmp [eax*4+0x10100]; (10010) ret
# table 10100: 10011 10012 10013, each a ret (record E3 §E3.7)
SWITCH = {0x10000: "3C02" "770C" "25FF000000" "FF248500010100" "C3" "C3C3C3",
          0x10100: "11000100" "12000100" "13000100"}


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class SwitchScanTests(unittest.TestCase):
    def test_a_bounded_switch_is_followed_when_asked(self):
        info = E.static_scan(program(SWITCH), 0x10000, switches=True)
        self.assertEqual(info.indirect, [])
        self.assertEqual(info.leaders, [0x10000, 0x10004, 0x10010, 0x10011, 0x10012, 0x10013])

    def test_without_switches_the_scan_is_e1s(self):
        self.assertEqual(E.static_scan(program(SWITCH), 0x10000).indirect, [0x10009])

    def test_a_guard_on_another_register_does_not_bound_it(self):
        other = {**SWITCH, 0x10000: "80FB02" "770B" "25FF000000" "FF248500010100" "C3"}   # cmp bl,2
        self.assertEqual(E.static_scan(program(other), 0x10000, switches=True).indirect, [0x1000A])

    def test_a_guard_without_ja_does_not_bound_it(self):
        below = {**SWITCH, 0x10000: "3C02" "720C" "25FF000000" "FF248500010100" "C3"}     # jb, not ja
        self.assertEqual(E.static_scan(program(below), 0x10000, switches=True).indirect, [0x10009])

    def test_a_ja_that_tests_another_compare_does_not_bound_it(self):
        # cmp al,2; cmp bl,9; ja; and eax,0xff; jmp [eax*4+0x10100]: the ja tests bl, not al
        two = {**SWITCH, 0x10000: "3C02" "80FB09" "770C" "25FF000000" "FF248500010100" "C3"}
        self.assertEqual(E.static_scan(program(two), 0x10000, switches=True).indirect, [0x1000C])

    def test_an_index_replaced_after_the_ja_does_not_bound_it(self):
        # cmp al,2; ja; mov eax,ebx; jmp [eax*4+0x10100]: eax is no longer what was compared
        moved = {**SWITCH, 0x10000: "3C02" "7709" "89D8" "FF248500010100" "C3"}
        self.assertEqual(E.static_scan(program(moved), 0x10000, switches=True).indirect, [0x10006])
        # the same after the mask: cmp al,2; ja; and eax,0xff; mov eax,ebx; jmp [eax*4+0x10100]
        masked = {**SWITCH, 0x10000: "3C02" "770E" "25FF000000" "89D8" "FF248500010100" "C3"}
        self.assertEqual(E.static_scan(program(masked), 0x10000, switches=True).indirect, [0x1000B])

    def test_an_index_with_unchecked_high_bits_does_not_bound_it(self):
        # cmp al,2; ja; jmp [eax*4+0x10100]: bits 8-31 of eax are not bounded
        bare = {**SWITCH, 0x10000: "3C02" "7707" "FF248500010100" "C3"}
        self.assertEqual(E.static_scan(program(bare), 0x10000, switches=True).indirect, [0x10004])

    def test_a_movzx_and_a_move_to_another_register_keep_it_bounded(self):
        # cmp al,2; ja; movzx eax,al; mov ecx,ebx; jmp [eax*4+0x10100]
        ok = {**SWITCH, 0x10000: "3C02" "770C" "0FB6C0" "89D9" "FF248500010100" "C3" "C3C3C3"}
        info = E.static_scan(program(ok), 0x10000, switches=True)
        self.assertEqual(info.indirect, [])
        self.assertEqual(info.leaders, [0x10000, 0x10004, 0x10010, 0x10011, 0x10012, 0x10013])




# ---- track H batch h0 (record 2026-10-14-reverse-h §H.1): the machine model --------------------------------

# 10000: mov ah,1; int 0x16; setz cl; mov [0x80010],cl; mov ah,0; int 0x16; mov [0x80000],eax;
#        mov ah,1; int 0x16; setz cl; mov [0x80011],cl; ret
H0_KEYS = {0x10000: "B401" "CD16" "0F94C1" "880D10000800" "B400" "CD16" "A300000800"
                    "B401" "CD16" "0F94C1" "880D11000800" "C3"}
# 10000: mov edx,0x3da; (10005) in al,dx; test al,8; je 10005; mov edx,0x3c8; mov al,0x10; out dx,al;
#        inc edx; mov al,0xc5; out dx,al; ret
H0_PORTS = {0x10000: "BADA030000" "EC" "A808" "74FB" "BAC8030000" "B010" "EE" "42" "B0C5" "EE" "C3"}
H0_SEED = {0x80000: b"\xee" * 4, 0x80010: b"\xee\xee"}


def h0_screen_image():
    """An image up to 0xB0000 (the aperture inside it). 10000: mov [0xa0000],eax; mov [0xafa00],eax;
    call 10100; ret     10100: int3 (stubbed)"""
    data = bytearray(0xA0000)
    code = bytes.fromhex("A300000A00" "A300FA0A00" "E8F1000000" "C3")
    data[:len(code)] = code
    data[0x100] = 0xCC
    return E.Image(bytes(data))


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class H0MachineTests(unittest.TestCase):
    def test_int16_peeks_with_zf_reads_ax_and_counts_the_keys_left(self):
        r = E.run_original(program(H0_KEYS), 0x10000, regs={"eax": 0xABCD0000}, pokes=H0_SEED,
                           machine=E.Machine(keys=(0x1E61,)))
        self.assertEqual(r.outcome, "ok")
        # a key: ZF = 0; AH = 0 takes it into AX, EAX's upper half kept; then the queue is empty: ZF = 1
        self.assertEqual(r.writes, {0x80000: 0x61, 0x80001: 0x1E, 0x80002: 0xCD, 0x80003: 0xAB,
                                    0x80010: 0x00, 0x80011: 0x01})
        self.assertEqual(r.keys_left, 0)
        r = E.run_original(program(H0_KEYS), 0x10000, pokes=H0_SEED, machine=E.Machine(keys=(0x1E61, 0x3062)))
        self.assertEqual((r.outcome, r.writes[0x80011], r.keys_left), ("ok", 0x00, 1))   # a peek keeps the key

    def test_int16_stops_where_the_bios_would_wait_or_without_the_key_model(self):
        r = E.run_original(program(H0_KEYS), 0x10000, machine=E.Machine(keys=()))
        self.assertEqual((r.outcome, r.detail), ("unmodeled", "int 0x16 AH=0 at 0x1000F: the key queue is empty"))
        for m in (None, E.Machine(outs=(0x3C8,))):
            r = E.run_original(program(H0_KEYS), 0x10000, machine=m)
            self.assertEqual((r.outcome, r.detail, r.keys_left), ("unmodeled", "int at 0x10002", None))
        # 10000: mov ah,0x10; int 0x16; ret
        r = E.run_original(program({0x10000: "B410" "CD16" "C3"}), 0x10000, machine=E.Machine(keys=(1,)))
        self.assertEqual((r.outcome, r.detail), ("unmodeled", "int 0x16 AH=0x10 at 0x10002"))

    def test_in_returns_the_cases_bytes_and_out_is_recorded_as_the_device_keeps_it(self):
        m = E.Machine(ins=((0x3DA, (0xF7, 0x08)),), outs=(0x3C8, 0x3C9))
        r = E.run_original(program(H0_PORTS), 0x10000, machine=m)
        self.assertEqual(r.outcome, "ok")
        self.assertEqual(r.outs, [(0x3C8, 0x10), (0x3C9, 0xC5)])    # AL as written
        # one value with bit 3 clear: the spin reads again, past the case's values
        r = E.run_original(program(H0_PORTS), 0x10000, machine=E.Machine(ins=((0x3DA, (0xF7,)),), outs=(0x3C8,)))
        self.assertEqual((r.outcome, r.detail), ("unmodeled", "in from 0x3DA at 0x10005: its values are spent"))
        r = E.run_original(program(H0_PORTS), 0x10000, machine=E.Machine(ins=((0x3DA, (8,)),), outs=(0x3C8,)))
        self.assertEqual((r.outcome, r.detail), ("unmodeled", "out at 0x10015"))   # 0x3C9 is not recorded
        for m in (None, E.Machine(keys=())):
            r = E.run_original(program(H0_PORTS), 0x10000, machine=m)
            self.assertEqual((r.outcome, r.detail, r.outs), ("unmodeled", "in at 0x10005", []))
        # 10000: in al,0x40; ret
        r = E.run_original(program({0x10000: "E440" "C3"}), 0x10000, regs={"eax": 0x12345678},
                           machine=E.Machine(ins=((0x40, (0x9A,)),)))
        self.assertEqual((r.outcome, r.regs["eax"]), ("ok", 0x1234569A))

    def test_the_aperture_leaves_the_writes_and_the_call_memory(self):
        seed = {0xA0000: b"\xaa" * 4, 0xAFA00: b"\xbb" * 4}
        stub = (E.Call(0x10100),)
        r = E.run_original(h0_screen_image(), 0x10000, regs={"eax": 0x11223344}, pokes=seed, calls=stub,
                           machine=E.Machine(aperture=True))
        self.assertEqual(r.aperture, {0xA0000: 0x44, 0xA0001: 0x33, 0xA0002: 0x22, 0xA0003: 0x11})
        self.assertEqual(r.writes, {0xAFA00: 0x44, 0xAFA01: 0x33, 0xAFA02: 0x22, 0xAFA03: 0x11})   # past the 64000
        self.assertEqual(r.call_mem, [dict(r.writes)])
        r = E.run_original(h0_screen_image(), 0x10000, regs={"eax": 0x11223344}, pokes=seed, calls=stub)
        self.assertEqual((r.aperture, len(r.writes), len(r.call_mem[0])), ({}, 8, 8))

    def test_the_dac_keeps_six_bits_per_channel_and_advances_its_index_after_three(self):
        outs = [(0x3C8, 0xFF), (0x3C9, 0xC5), (0x3C9, 0x41), (0x3C9, 0x02), (0x43, 0x34),
                (0x3C9, 1), (0x3C9, 2), (0x3C9, 3), (0x3C9, 4)]
        self.assertEqual(E.dac_effects(outs), [("dac", 0xFF, 5, 1, 2), ("out", 0x43, 0x34), ("dac", 0, 1, 2, 3),
                                               ("dac-at", 1, (4,))])
        # an index the stream never set; an index set again restarts the channels; no DAC write: nothing
        self.assertEqual(E.dac_effects([(0x3C9, 1), (0x3C9, 2), (0x3C9, 3), (0x3C8, 7), (0x3C9, 9), (0x3C8, 7)]),
                         [("dac", None, 1, 2, 3), ("dac-at", 7, ())])
        self.assertEqual(E.dac_effects([(0x40, 1)]), [("out", 0x40, 1)])

    def test_a_machine_declaration_is_checked(self):
        for kw in ({"keys": (0x10000,)}, {"ins": ((0x3DA, ()),)}, {"ins": ((0x3DA, (0x100,)),)},
                   {"outs": (0x10000,)}):
            with self.subTest(kw=kw), self.assertRaises(ValueError):
                E.Machine(**kw)



# ---- track F batch f1 (record 2026-10-14-reverse-f §F.1): the file model ---------------------------------
# The modelled callees' entry addresses are the runtime's (fopen 0x61C60, fread 0x61CF6, fwrite 0x62031,
# fclose 0x61EC0); the programs below call them and save EAX to dwords of the 0xEE-seeded field at
# 0x80020 (a store of zero is then visible against the seed). The arguments follow the rows' call sites
# and the callees' prologues: fopen's name in EAX and mode in EDX (0x1AFF7/0x1AFFA at 0x1AFFB, §S.7.1);
# fread's and fwrite's buffer in EAX, size in EDX, count in EBX, stream in ECX (0x61CFC..0x61D00 and
# 0x62037..0x6203B: they save EAX/EDX, move ECX to the stream register and multiply EBX by EDX);
# fclose's stream in EAX (0x61EC2 `mov edx,eax`). fclose mirrors the file's final name/mode/length/
# bytes to its F1_SHADOW slot, which is why the buffer at 0x81000 starts 0xEE: the read must move bytes.
# 10000: mov eax,0x80000; mov edx,0x80010; call 0x61C60; mov [0x80020],eax; mov ecx,[0x80020];
#        mov eax,0x81000; mov edx,1; mov ebx,4; call 0x61CF6; mov [0x80024],eax; mov eax,[0x80020];
#        call 0x61EC0; mov [0x80028],eax; ret
F1_READ_CLOSE = ("B800000800" "BA10000800" "E8511C0500" "A320000800" "8B0D20000800"
                 "B800100800" "BA01000000" "BB04000000" "E8C81C0500" "A324000800"
                 "8B0520000800" "E8821E0500" "A328000800" "C3")
# 10000: mov eax,0x80000; mov edx,0x80010; call 0x61C60; mov [0x80020],eax; mov eax,0x81000; mov edx,1;
#        mov ebx,4; mov ecx,[0x80020]; call 0x62031; mov [0x80024],eax; mov eax,[0x80020];
#        call 0x61EC0; mov [0x80028],eax; ret
F1_WRITE_CLOSE = ("B800000800" "BA10000800" "E8511C0500" "A320000800" "B800100800"
                  "BA01000000" "BB04000000" "8B0D20000800" "E803200500" "A324000800"
                  "8B0520000800" "E8821E0500" "A328000800" "C3")
# 10000: mov eax,0x80000; mov edx,0x80010; call 0x61C60; mov [0x80020],eax; test eax,eax; je +1; ret; ret
F1_OPEN_OR_SKIP = "B800000800" "BA10000800" "E8511C0500" "A320000800" "85C0" "7401" "C3" "C3"
# 10000: mov eax,0x1234; call 0x61EC0; mov [0x80028],eax; ret
F1_CLOSE_BOGUS = "B834120000" "E8B61E0500" "A328000800" "C3"
F1_SEED = {0x80000: b"cmos\0\0\0\0\0", 0x80010: b"rb\0\0\0\0\0", 0x81000: bytes([0xEE] * 8),
           0x80020: bytes([0xEE] * 0xC)}


def f1_image(code):
    """An image from 0x10000 to 0x81100 (the `F1_SHADOW` span inside it) with `code` at the entry. The
    file functions' real bytes are never reached: the model answers the call at the entry address."""
    data = bytearray(0x81100 - 0x10000)
    b = bytes.fromhex(code)
    data[:len(b)] = b
    return E.Image(bytes(data))


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class F1FilesTests(unittest.TestCase):
    def slot(self, r, i=0):
        base = E.F1_SHADOW + E.F1_SLOT * i
        return (bytes(r.writes.get(base + k, 0) for k in range(5)),                   # name + NUL
                bytes(r.writes.get(base + 0x10 + k, 0) for k in range(3)),            # mode + NUL
                int.from_bytes(bytes(r.writes.get(base + 0x20 + k, 0) for k in range(4)), "little"),
                bytes(r.writes.get(base + 0x24 + k, 0) for k in range(4)))            # the content head

    def test_files_needs_a_declared_name_and_at_most_two(self):
        for files in ({}, {"cmos": b"1"}, {b"": b"1"}, {b"x" * 16: b"1"}, {b"a\0b": b"1"},
                      {b"a": "1"}, {b"a": b"1" * 2049}, {b"a": 1},
                      {b"a": b"1", b"b": b"2", b"c": b"3"}):
            with self.subTest(files=files), self.assertRaises(ValueError):
                E.Machine(files=files)
        for files in (None, {b"cmos": E.F1_ABSENT}, {b"cmos": E.F1_GONE}, {b"cmos": b"1"},
                      {b"a": b"1", b"b": E.F1_ABSENT}):
            E.Machine(files=files)                                                    # these are accepted

    def test_fread_copies_the_case_file_and_fclose_mirrors_name_mode_and_bytes(self):
        r = E.run_original(f1_image(F1_READ_CLOSE), 0x10000, pokes=F1_SEED,
                           machine=E.Machine(files={b"cmos": b"ABCDEFGHIJ"}))
        self.assertEqual(r.outcome, "ok")
        self.assertEqual(r.file_opens, [(b"cmos", b"rb")])                            # read from memory
        self.assertEqual([r.writes[0x80020], r.writes.get(0x80021, 0xEE), r.writes.get(0x80023, 0xEE)],
                         [1, 0, 0])                                                    # EAX = the handle, 1
        self.assertEqual(r.writes.get(0x80024), 4)                                    # four items read
        self.assertEqual(bytes(r.writes.get(0x81000 + i, 0xEE) for i in range(8)), b"ABCD\xEE\xEE\xEE\xEE")
        self.assertEqual(self.slot(r), (b"cmos\0", b"rb\0", 10, b"ABCD"))
        self.assertEqual(r.writes.get(0x80028), 0)                                    # fclose's 0, stored and seen

    def test_fread_is_short_at_end_of_file(self):
        r = E.run_original(f1_image(F1_READ_CLOSE), 0x10000, pokes=F1_SEED,
                           machine=E.Machine(files={b"cmos": b"AB"}))
        self.assertEqual(r.outcome, "ok")
        self.assertEqual(r.writes.get(0x80024), 2)                                    # whole items only
        self.assertEqual(bytes(r.writes.get(0x81000 + i, 0xEE) for i in range(4)), b"AB\xEE\xEE")
        self.assertEqual(self.slot(r)[2], 2)

    def test_fopen_of_a_missing_file_returns_zero_and_fclose_mirrors_nothing(self):
        for state in (E.F1_ABSENT, E.F1_GONE):
            r = E.run_original(f1_image(F1_OPEN_OR_SKIP), 0x10000, pokes=F1_SEED,
                               machine=E.Machine(files={b"cmos": state}))
            self.assertEqual(r.outcome, "ok")                                         # je took the skip
            self.assertEqual(r.file_opens, [(b"cmos", b"rb")])                         # the open is recorded
            self.assertEqual(r.writes.get(0x80020), 0)                                 # the NULL handle (0x1B002)
            mirrored = [a for a in r.writes if E.F1_SHADOW <= a < E.F1_SHADOW + 2 * E.F1_SLOT]
            self.assertEqual(mirrored, [])              # no fclose: the mirror's whole span stays unwritten

    def test_fwrite_creates_a_new_file_and_fclose_mirrors_it(self):
        r = E.run_original(f1_image(F1_WRITE_CLOSE), 0x10000,
                           pokes={**F1_SEED, **{0x80010: b"wb\0\0\0\0\0", 0x81000: b"WXYZ\xEE\xEE\xEE\xEE"}},
                           machine=E.Machine(files={b"cmos": E.F1_ABSENT}))
        self.assertEqual(r.outcome, "ok")
        self.assertEqual(r.file_opens, [(b"cmos", b"wb")])
        self.assertEqual(r.writes.get(0x80024), 4)                                     # fwrite returns the count
        self.assertEqual(self.slot(r), (b"cmos\0", b"wb\0", 4, b"WXYZ"))

    def test_fopen_w_replaces_the_stale_file_model_side(self):
        # the model rule (OPEN Q1): a write-mode open starts the file at offset 0
        r = E.run_original(f1_image(F1_WRITE_CLOSE), 0x10000,
                           pokes={**F1_SEED, **{0x80010: b"wb\0\0\0\0\0", 0x81000: b"WXYZ\xEE\xEE\xEE\xEE"}},
                           machine=E.Machine(files={b"cmos": b"0123456789"}))
        self.assertEqual(r.outcome, "ok")
        self.assertEqual(self.slot(r), (b"cmos\0", b"wb\0", 4, b"WXYZ"))               # not the old ten bytes

    def test_another_mode_an_unlisted_name_and_a_read_on_a_write_stream_stop_the_run(self):
        r = E.run_original(f1_image(F1_READ_CLOSE), 0x10000, pokes={**F1_SEED, **{0x80010: b"r+\0\0\0\0"}},
                           machine=E.Machine(files={b"cmos": E.F1_ABSENT}))
        self.assertEqual((r.outcome, r.detail.split(": ")[1]),
                         ("unmodeled", "the mode b'r+' is not one the rows use"))
        r = E.run_original(f1_image(F1_READ_CLOSE), 0x10000, pokes={**F1_SEED, **{0x80000: b"cfg\0\0\0\0\0"}},
                           machine=E.Machine(files={b"cmos": E.F1_ABSENT}))
        self.assertEqual((r.outcome, r.detail.split(": ")[1]),
                         ("unmodeled", "the name b'cfg' is not declared by the case"))
        r = E.run_original(f1_image(F1_READ_CLOSE), 0x10000, pokes={**F1_SEED, **{0x80010: b"wb\0\0\0\0\0"}},
                           machine=E.Machine(files={b"cmos": b"ABCDEFGHIJ"}))
        self.assertEqual((r.outcome, r.detail.split(": ")[1]),
                         ("unmodeled", "ECX is not a handle opened for reading"))

    def test_fclose_of_a_handle_the_model_never_opened_returns_minus_one(self):
        r = E.run_original(f1_image(F1_CLOSE_BOGUS), 0x10000, pokes=F1_SEED,
                           machine=E.Machine(files={b"cmos": E.F1_ABSENT}))
        self.assertEqual(r.outcome, "ok")
        self.assertEqual(int.from_bytes(bytes(r.writes.get(0x80028 + i, 0xEE) for i in range(4)), "little"),
                         0xFFFFFFFF)                                     # 0x61ECD's walk-failure answer

    def test_a_case_without_files_still_stops_on_the_call_exactly_as_before(self):
        img = f1_image(F1_READ_CLOSE)
        for m in (None, E.Machine(keys=()), E.Machine(aperture=True)):
            r = E.run_original(img, 0x10000, machine=m)
            self.assertEqual((r.outcome, r.detail, r.file_opens), ("unmodeled", "call 0x61C60 from 0x1000A", []))


# ---- track G batch g1 (record 2026-10-14-reverse-g §G1): the terminal exit --------------------------------
# The runtime's exit chain 0x62003..0x62723 (record 2026-10-13-reverse-s §S.7.1, the bytes re-verified
# over the fixup-applied image and in Ghidra on 2026-10-14): 0x62003 keeps the caller's EAX, the exit
# code (`mov edx,eax` 0x62004, restored `mov eax,edx` 0x62012), calls the cleanup vectors, and 0x6201B
# does the same again and tail-jumps (0x6202C -> 0x626EB -> 0x62710), where `push eax` saves the code,
# `mov eax,0; mov edx,0xff; call 0x6C4CF` flushes with EAX destroyed, `pop eax` 0x62720 restores the
# code, `mov ah,0x4C` 0x62721 replaces only the high byte, and `int 0x21` 0x62723 — DOS Terminate
# Process — ends the run with AL as its return code. The fixture below is that shape: the exit code 1
# arrives in EAX, survives the zeroing (the pop restores it), and nothing after the int executes.
# 10000: mov eax,1; push eax; mov eax,0; mov edx,0xff; pop eax; mov ah,0x4c; int 0x21;
#        mov [0x80000],eax; ret
G1_EXIT = "B801000000" "50" "B800000000" "BAFF000000" "58" "B44C" "CD21" "A300000800" "C3"
# 10000: mov eax,0x3d41; int 0x21; ret — the same int, a function the exit model does not answer
G1_INT21_OTHER = "B8413D0000" "CD21" "C3"
# 10000: mov eax,[0x80000]; inc eax; mov [0x80004],eax; ret — a run that never exits
G1_NO_EXIT = "A100000800" "FFC0" "A304000800" "C3"
G1_SEED = {0x80000: b"\xee" * 8}    # the canary's destination and the loader's source, both seeded apart


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class G1ExitTests(unittest.TestCase):
    def test_g1_an_exit_run_under_exits_records_the_terminal_and_stops_cleanly(self):
        r = E.run_original(image(bytes.fromhex(G1_EXIT)), 0x10000, pokes=G1_SEED, machine=E.Machine(exits=True))
        self.assertEqual((r.outcome, r.terminal), ("exit", ("exit", 1)))
        # the run stops AT the int: the store after it never executes, so the seeded bytes stay unwritten
        self.assertEqual(r.writes, {})
        self.assertNotIn(0x10015, r.executed)

    def test_g1_without_the_exit_model_the_exit_int_stops_unmodeled_exactly_as_today(self):
        img = image(bytes.fromhex(G1_EXIT))
        for m in (None, E.Machine()):                    # no machine, and the all-default machine
            r = E.run_original(img, 0x10000, pokes=G1_SEED, machine=m)
            self.assertEqual((r.outcome, r.detail, r.terminal), ("unmodeled", "int at 0x10013", None))
        # with exits the same int stops unmodeled when AH is not 0x4C: the model answers only the terminate
        r = E.run_original(image(bytes.fromhex(G1_INT21_OTHER)), 0x10000, machine=E.Machine(exits=True))
        self.assertEqual((r.outcome, r.detail, r.terminal), ("unmodeled", "int at 0x10005", None))

    def test_g1_a_run_that_never_exits_returns_with_no_terminal(self):
        r = E.run_original(image(bytes.fromhex(G1_NO_EXIT)), 0x10000, pokes=G1_SEED, machine=E.Machine(exits=True))
        self.assertEqual(r.outcome, "ok")
        self.assertIsNone(r.terminal)
        self.assertEqual(r.writes[0x80004], 0xEF)        # 0xEEEEEEEE + 1, the run completed


if __name__ == "__main__":
    unittest.main()
