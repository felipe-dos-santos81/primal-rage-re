# tools/tests/test_diff_verify.py
"""Unit tests for tools/diff_verify.py.

The comparison logic is tested with fabricated port results on small hand-assembled images, so it
needs no PRAGE.EXE. The last class runs the real build/diffrun on the real functions; it skips when
either is absent (the other oracles skip the same way) unless PR_ORACLE_REQUIRED=1.
"""
import contextlib
import dataclasses
import io
import os
import stat
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import diff_emu as E
import diff_verify as V

REQUIRED = os.environ.get("PR_ORACLE_REQUIRED") == "1"
needs_unicorn = unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")


def le32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


def image(code):
    data = bytearray(0x80100)
    data[:len(code)] = code
    return E.Image(bytes(data))


def orig(eax, writes=None):
    regs = {r: 0 for r in E.REGS}
    regs["eax"] = eax
    return E.OrigResult("ok", "", regs, writes or {}, set(), [])


class ParseTests(unittest.TestCase):
    def test_cases_text_round_trips_through_the_parser(self):
        spec = V.Spec("f", 0x10000, [V.Case("a", {"eax": 5, "edx": 0x10}, {0x80000: b"\x01\x02"})])
        text = V.cases_text(spec, "f@mutant")
        self.assertEqual(text, "case a\nfn f@mutant\nreg eax 0x5\nreg edx 0x10\npoke 0x80000 0102\nend\n")

    def test_port_output_is_parsed(self):
        out = V.parse_port_output("case a\nret eax 0x7 mask 0xFF\nw 0x80004 0x15\nw 0x80005 0x00\nend\n"
                                  "case b\nerror unknown binding x\nend\n")
        self.assertEqual((out["a"].eax, out["a"].mask), (7, 0xFF))
        self.assertEqual(out["a"].writes, {0x80004: 0x15, 0x80005: 0})
        self.assertEqual(out["b"].error, "unknown binding x")

    def test_output_outside_a_case_is_rejected(self):
        with self.assertRaises(ValueError):
            V.parse_port_output("w 0x1 0x2\n")

    def test_truncated_or_malformed_output_is_rejected_not_parsed_as_a_result(self):
        # Each of these used to parse (a case with no ret/error line was eax 0, no writes), so
        # output cut after `case c5` could agree with an original that returns 0 and writes nothing.
        ok = "ret eax 0x1 mask 0xFF\n"
        for name, text in [
            ("a case still open at EOF", "case a\n"),
            ("ret without end", "case a\n" + ok),
            ("end without ret or error", "case a\nend\n"),
            ("two rets", "case a\n" + ok + ok + "end\n"),
            ("ret and error", "case a\n" + ok + "error x\nend\n"),
            ("two errors", "case a\nerror x\nerror y\nend\n"),
            ("duplicate case ids", "case a\n" + ok + "end\ncase a\n" + ok + "end\n"),
            ("a case opened inside an open case", "case a\ncase b\n" + ok + "end\n"),
            ("an unknown line", "case a\n" + ok + "bogus 1\nend\n"),
            ("a duplicate write address", "case a\n" + ok + "w 0x1 0x2\nw 0x1 0x3\nend\n"),
            ("end outside a case", ok + "end\n"),
        ]:
            with self.subTest(name):
                with self.assertRaises(ValueError):
                    V.parse_port_output(text)

    def test_duplicate_case_ids_in_a_spec_are_refused(self):
        with self.assertRaises(ValueError) as cm:
            V.Spec("f", 0x10000, [V.Case("a", {}), V.Case("a", {"eax": 1})])
        self.assertIn("duplicate case id", str(cm.exception))


class CompareTests(unittest.TestCase):
    def test_agreement_has_no_discrepancy(self):
        self.assertEqual(V.compare(orig(5, {0x80000: 1}), V.PortResult(5, 0xFFFFFFFF, {0x80000: 1})), [])

    def test_the_return_mask_decides_what_counts(self):
        # fighter_slot_flag returns through AL: the original leaves EAX = 0x201 where the port's C
        # return is 1. Bits 8+ are scratch no caller reads, so under mask 0xFF they agree, and
        # under a full mask they do not: the mask is what the binding states, not a loophole.
        self.assertEqual(V.compare(orig(0x201), V.PortResult(1, 0xFF), 0xFF), [])
        self.assertEqual(len(V.compare(orig(0x201), V.PortResult(1, 0xFFFFFFFF), 0xFFFFFFFF)), 1)

    def test_the_spec_mask_not_the_ports_decides(self):
        # The mask is the Spec's. A port that reports another mask is a discrepancy, so a binding
        # cannot narrow the comparison by itself.
        d = V.compare(orig(1), V.PortResult(1, 0xFF), 0xFFFFFFFF)
        self.assertEqual(d, ["port reports eax mask 0xFF, the spec states 0xFFFFFFFF"])
        d = V.compare(orig(1), V.PortResult(1, 0xFFFFFFFF), 0xFF)
        self.assertEqual(d, ["port reports eax mask 0xFFFFFFFF, the spec states 0xFF"])

    def test_a_port_reported_mask_of_zero_cannot_make_a_differing_eax_agree(self):
        d = V.compare(orig(5), V.PortResult(0, 0), 0xFFFFFFFF)
        self.assertIn("eax (mask 0xFFFFFFFF): original 0x5, port 0x0", d)
        self.assertIn("port reports eax mask 0x0, the spec states 0xFFFFFFFF", d)
        self.assertEqual(len(d), 2)

    def test_the_default_spec_mask_is_the_full_compare(self):
        self.assertEqual(V.Spec("f", 0x10000, [V.Case("a", {})]).eax_mask, 0xFFFFFFFF)

    def test_a_write_the_port_forgot_is_a_discrepancy(self):
        d = V.compare(orig(0, {0x107EE0: 1}), V.PortResult(0, 0xFFFFFFFF, {}))
        self.assertEqual(d, ["byte 0x107EE0: original 0x01, port unchanged"])

    def test_a_write_the_original_did_not_make_is_a_discrepancy(self):
        d = V.compare(orig(0, {}), V.PortResult(0, 0xFFFFFFFF, {0x105C00: 0}))
        self.assertEqual(d, ["byte 0x105C00: original unchanged, port 0x00"])

    def test_a_port_error_is_a_discrepancy(self):
        d = V.compare(orig(0), V.PortResult(0, 0xFFFFFFFF, {}, "unknown binding x"))
        self.assertIn("port: unknown binding x", d)


class MutantDetectionTests(unittest.TestCase):
    def res(self, **kw):
        return V.SpecResult("m@mutant", 0x10000, "MISMATCH", **kw)

    def test_only_a_real_eax_or_byte_difference_counts_as_detection(self):
        self.assertEqual(V.mutant_detection(self.res(diffs=2, problems=["a: eax"]))[0], True)

    def test_a_port_error_is_never_detection(self):
        # an unknown binding makes every case `error unknown binding ...`, which compare() reports
        # as a problem: that is a missing mutant, not a caught one
        ok, why = V.mutant_detection(self.res(diffs=0, problems=["a: port: unknown binding m@x"],
                                              port_errors=["a: unknown binding m@x"]))
        self.assertFalse(ok)
        self.assertIn("port error", why)
        self.assertIn("unknown binding", why)

    def test_a_port_error_beside_a_difference_is_still_not_detection(self):
        self.assertFalse(V.mutant_detection(self.res(diffs=1, port_errors=["b: boom"]))[0])

    def test_a_mutant_that_agrees_everywhere_is_not_detected(self):
        ok, why = V.mutant_detection(V.SpecResult("m", 0, "VERIFIED"))
        self.assertFalse(ok)
        self.assertIn("no eax or byte difference", why)


class MainArgumentTests(unittest.TestCase):
    def run_main(self, argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = V.main(argv)
        return rc, out.getvalue()

    def test_an_unknown_function_is_an_error_not_zero_of_zero(self):
        rc, out = self.run_main(["--function", "rng_nxt"])
        self.assertEqual(rc, 2)
        self.assertIn("rng_nxt", out)
        self.assertIn("rng_next", out)          # the known names are listed


# 10000: cmp eax,0; je 10008; inc eax; jmp 10009; (10008) dec eax; (10009) ret
DIAMOND = bytes.fromhex("83F800" "7403" "40" "EB01" "48" "C3")


@needs_unicorn
class VerifySpecTests(unittest.TestCase):
    ZERO = V.PortResult(0xFFFFFFFF, 0xFFFFFFFF)        # what a correct port returns for eax = 0 (dec)
    ONE = V.PortResult(2, 0xFFFFFFFF)                  # ... and for eax = 1 (inc)

    def test_every_block_exercised_and_agreeing_is_verified(self):
        spec = V.Spec("d", 0x10000, [V.Case("z", {"eax": 0}), V.Case("o", {"eax": 1})])
        r = V.verify_spec(spec, image(DIAMOND), {"z": self.ZERO, "o": self.ONE})
        self.assertEqual((r.verdict, r.hit, r.total, r.unhit), ("VERIFIED", 4, 4, []))

    def test_an_unexercised_block_is_partial_not_verified(self):
        spec = V.Spec("d", 0x10000, [V.Case("z", {"eax": 0})])
        r = V.verify_spec(spec, image(DIAMOND), {"z": self.ZERO})
        self.assertEqual((r.verdict, r.hit, r.total, r.unhit), ("PARTIAL", 3, 4, [0x10005]))

    def test_a_named_unhit_block_is_accepted(self):
        spec = V.Spec("d", 0x10000, [V.Case("z", {"eax": 0})], unhit_named={0x10005: "the inc arm needs eax != 0"})
        r = V.verify_spec(spec, image(DIAMOND), {"z": self.ZERO})
        self.assertEqual((r.verdict, r.unhit), ("VERIFIED", []))

    def test_a_difference_is_a_mismatch_even_with_full_coverage(self):
        spec = V.Spec("d", 0x10000, [V.Case("z", {"eax": 0}), V.Case("o", {"eax": 1})])
        r = V.verify_spec(spec, image(DIAMOND), {"z": self.ZERO, "o": V.PortResult(3, 0xFFFFFFFF)})
        self.assertEqual(r.verdict, "MISMATCH")
        self.assertEqual(r.problems, ["o: eax (mask 0xFFFFFFFF): original 0x2, port 0x3"])

    def test_a_port_that_returned_other_case_ids_is_an_error_not_a_keyerror(self):
        spec = V.Spec("d", 0x10000, [V.Case("z", {"eax": 0}), V.Case("o", {"eax": 1})])
        with self.assertRaises(ValueError) as cm:
            V.verify_spec(spec, image(DIAMOND), {"z": self.ZERO})
        self.assertIn("missing ['o']", str(cm.exception))
        with self.assertRaises(ValueError) as cm:
            V.verify_spec(spec, image(DIAMOND), {"z": self.ZERO, "o": self.ONE, "x": self.ONE})
        self.assertIn("extra ['x']", str(cm.exception))

    def test_port_errors_are_recorded_apart_from_differences(self):
        spec = V.Spec("d", 0x10000, [V.Case("z", {"eax": 0}), V.Case("o", {"eax": 1})])
        r = V.verify_spec(spec, image(DIAMOND), {"z": V.PortResult(0xFFFFFFFF, 0xFFFFFFFF, {}, "boom"),
                                                 "o": V.PortResult(3, 0xFFFFFFFF)})
        self.assertEqual((r.port_errors, r.diffs), (["z: boom"], 1))

    def test_a_port_that_reports_mask_zero_cannot_hide_a_wrong_eax(self):
        spec = V.Spec("d", 0x10000, [V.Case("z", {"eax": 0}), V.Case("o", {"eax": 1})])
        r = V.verify_spec(spec, image(DIAMOND), {"z": self.ZERO, "o": V.PortResult(0, 0)})
        self.assertEqual(r.verdict, "MISMATCH")
        self.assertEqual(r.problems, ["o: port reports eax mask 0x0, the spec states 0xFFFFFFFF",
                                      "o: eax (mask 0xFFFFFFFF): original 0x2, port 0x0"])

    def test_a_mask_only_discrepancy_is_not_a_mutant_detection(self):
        # EAX (under the Spec mask) and every byte agree; only the port's reported mask differs. That
        # is a MISMATCH, but it is neither an eax nor a byte difference, so it must not count as a
        # detected mutant (the old rule counted every problem that did not start with "port: ").
        spec = V.Spec("d", 0x10000, [V.Case("z", {"eax": 0}), V.Case("o", {"eax": 1})])
        r = V.verify_spec(spec, image(DIAMOND), {"z": V.PortResult(0xFFFFFFFF, 0xFF), "o": self.ONE})
        self.assertEqual(r.verdict, "MISMATCH")
        self.assertEqual(r.problems, ["z: port reports eax mask 0xFF, the spec states 0xFFFFFFFF"])
        self.assertEqual((r.diffs, r.port_errors), (0, []))
        ok, why = V.mutant_detection(r)
        self.assertFalse(ok)
        self.assertIn("no eax or byte difference", why)

    def test_the_specs_mask_is_applied_to_the_original(self):
        # DIAMOND leaves eax = 0x2 for eax = 1; with mask 0xFF a port that reports 2 under 0xFF agrees
        spec = V.Spec("d", 0x10000, [V.Case("z", {"eax": 0}), V.Case("o", {"eax": 1})], eax_mask=0xFF)
        r = V.verify_spec(spec, image(DIAMOND), {"z": V.PortResult(0xFF, 0xFF), "o": V.PortResult(2, 0xFF)})
        self.assertEqual((r.verdict, r.problems), ("VERIFIED", []))

    def test_an_unmodeled_instruction_makes_the_function_not_exercisable(self):
        spec = V.Spec("i", 0x10000, [V.Case("x", {})])
        r = V.verify_spec(spec, image(bytes.fromhex("CD21" "C3")), {"x": V.PortResult()})
        self.assertEqual(r.verdict, "NOT_EXERCISABLE")
        self.assertIn("int at 0x10000", r.problems[0])

    def test_an_indirect_jump_keeps_the_function_partial(self):
        # 10000: jmp eax (target unknown to the static scan) — the run returns through 0x10002
        code = bytes.fromhex("FFE0") + bytes(14) + bytes.fromhex("C3")        # 10010: ret
        spec = V.Spec("j", 0x10000, [V.Case("x", {"eax": 0x10010})])
        r = V.verify_spec(spec, image(code), {"x": V.PortResult(0x10010, 0xFFFFFFFF)})
        self.assertEqual(r.verdict, "PARTIAL")
        self.assertIn("indirect jmp/call at 0x10000", r.problems[0])

    def test_reads_outside_the_image_are_listed(self):
        code = bytes.fromhex("A100000001" "C3")            # mov eax,[0x1000000]; ret
        spec = V.Spec("o", 0x10000, [V.Case("x", {"eax": 0xFFFFFFFF})])
        r = V.verify_spec(spec, image(code), {"x": V.PortResult(0, 0xFFFFFFFF)})
        self.assertEqual((r.verdict, r.outside), ("VERIFIED", [(0x1000000, 4)]))
        self.assertIn("reads outside the image: 0x1000000+4", V.table_row(r))


class RunPortTests(unittest.TestCase):
    def test_a_runner_that_never_returns_is_an_error_not_a_hang(self):
        with tempfile.TemporaryDirectory() as d:
            script = os.path.join(d, "slow")
            with open(script, "w") as f:
                f.write("#!/bin/sh\nsleep 30\n")
            os.chmod(script, os.stat(script).st_mode | stat.S_IXUSR)
            with self.assertRaises(RuntimeError) as cm:
                V.run_port(script, "x.exe", os.path.join(d, "img"), "", timeout=1)
            self.assertIn("timed out", str(cm.exception))

    def test_a_runner_whose_output_is_cut_off_is_an_error(self):
        with tempfile.TemporaryDirectory() as d:
            script = os.path.join(d, "cut")
            with open(script, "w") as f:
                f.write("#!/bin/sh\nprintf 'case c5\\n'\n")
            os.chmod(script, os.stat(script).st_mode | stat.S_IXUSR)
            with self.assertRaises(ValueError):
                V.run_port(script, "x.exe", os.path.join(d, "img"), "")

    def test_a_failing_runner_is_an_error(self):
        with tempfile.TemporaryDirectory() as d:
            script = os.path.join(d, "bad")
            with open(script, "w") as f:
                f.write("#!/bin/sh\necho boom >&2\nexit 1\n")
            os.chmod(script, os.stat(script).st_mode | stat.S_IXUSR)
            with self.assertRaises(RuntimeError) as cm:
                V.run_port(script, "x.exe", os.path.join(d, "img"), "")
            self.assertIn("boom", str(cm.exception))


DIFFRUN = os.path.join(ROOT, "build", "diffrun")
EXE = os.path.join(os.environ.get("PR_GAME_DIR", os.path.join(ROOT, "data", "game", "C")), "PRAGE.EXE")

# Track P batch 1 (record 2026-10-02-reverse-p1): its rows with their EAX masks, and what alone catches
# each of its mutants.
P1_MASKS = {"fighter_1567c": 0xFF, "fighter_15908": 0xFF, "fighter_23ec0": 0xFF, "fighter_45d14": 0xFF,
            "fighter_actor_bit15_clear": 0xFF, "fighter_23bf8": 0xFFFFFFFF, "fighter_402fc": 0xFF,
            "fighter_15584": 0, "fighter_1579c": 0, "fighter_23d38": 0,
            "fighter_38034": 0, "fighter_23b68": 0, "fighter_401d4": 0,
            # final review I4: the finisher streams' 0xD100 targets (the dispatcher overwrites EAX)
            "fighter_156d4": 0, "fighter_23ca4": 0, "fighter_23868": 0, "fighter_3f174": 0}
P1_KINDS = {"fighter_1567c@mutant": {"call #0"}, "fighter_15908@mutant": {"call #1"},
            "fighter_23ec0@mutant": {"call #1 memory"}, "fighter_45d14@mutant": {"call #0"},
            "fighter_actor_bit15_clear@mutant": {"eax"}, "fighter_23bf8@mutant": {"call #1"},
            "fighter_23bf8@zero": {"eax"}, "fighter_23bf8@ne": {"call #1"},
            "fighter_402fc@mutant": {"call #0 memory"},
            "fighter_15584@mutant": {"call #1"}, "fighter_1579c@mutant": {"call #1"},
            "fighter_23d38@mutant": {"call #0", "call #1"},
            "fighter_23d38@ge": {"byte"}, "fighter_23d38@unsigned": {"byte", "call #0"},
            "fighter_23d38@bit": {"byte"}, "fighter_15584@ge": {"byte"},
            "fighter_1579c@ge": {"call #0", "call #1", "call #2", "byte"},
            "fighter_38034@mutant": {"call #1"}, "fighter_23b68@mutant": {"call #1"},
            "fighter_401d4@mutant": {"call #0", "call #1"},
            "fighter_401d4@no36": {"byte"}, "fighter_23d38@noneg": {"byte", "call #0"},
            # final review I1/I2: the stores the first seeds hid, and the two slots' +4 records
            "fighter_15584@no42": {"byte", "call #1 memory"}, "fighter_1579c@no42": {"byte", "call #1 memory"},
            "fighter_15584@own4": {"byte"},
            "fighter_23d38@noand": {"byte", "call #0 memory", "call #1 memory", "call #2 memory"},
            # final review I4: 0x23868's x offset by the other arm shows in the spawn's arguments alone
            "fighter_156d4@mutant": {"byte"}, "fighter_23ca4@mutant": {"byte"},
            "fighter_23868@mutant": {"call #0"}, "fighter_3f174@mutant": {"byte"}}

# Track P batch 2 (record 2026-10-02-reverse-p2): its rows with their EAX masks, and what alone catches each
# of its mutants.
P2_MASKS = {"fighter_237d0": 0, "fighter_2381c": 0, "fighter_3dadc": 0, "fighter_3db34": 0, "fighter_3d10c": 0,
            "fighter_22a00": 0, "fighter_229fc": 0, "fighter_14ef8": 0, "fighter_14f50": 0,
            "fighter_14fa8": 0, "fighter_14ff8": 0, "fighter_150ac": 0,
            "fighter_15478": 0, "fighter_3dcec": 0, "fighter_21114": 0,
            "fighter_21374": 0, "fighter_22938": 0, "fighter_2116c": 0xFFFFFFFF, "fighter_22510": 0xFFFFFFFF,
            "fighter_22404": 0, "fighter_211f0": 0, "fighter_22588": 0,
            "fighter_212cc": 0, "fighter_22638": 0}
P2_KINDS = {"fighter_237d0@mutant": {"call #0"}, "fighter_237d0@guard": {"byte", "call #0", "call #1"},
            "fighter_2381c@mutant": {"call #1"}, "fighter_3dadc@mutant": {"call #1 memory"},
            "fighter_3db34@mutant": {"call #0"}, "fighter_3d10c@mutant": {"call #0", "call #1"},
            "fighter_3d10c@side": {"byte"}, "fighter_3d10c@sext": {"byte"},
            "fighter_22a00@mutant": {"call #0 memory"}, "fighter_229fc@mutant": {"byte"},
            "fighter_14ef8@mutant": {"call #0"}, "fighter_14f50@mutant": {"call #0"},
            "fighter_14fa8@mutant": {"call #0"}, "fighter_14ff8@mutant": {"call #1"},
            "fighter_150ac@mutant": {"call #1"},
            "fighter_15478@mutant": {"call #0 memory"}, "fighter_3dcec@mutant": {"call #0"},
            "fighter_21114@mutant": {"call #0"}, "fighter_21114@side": {"byte", "call #0"},
            "fighter_21374@mutant": {"call #0"}, "fighter_22938@mutant": {"call #1"},
            "fighter_22938@order": {"call #0 memory", "call #1 memory"},
            "fighter_2116c@mutant": {"call #0"}, "fighter_2116c@unsigned": {"eax", "call #0"},
            "fighter_2116c@eax": {"eax"}, "fighter_22510@mutant": {"call #0"},
            "fighter_22510@ge": {"eax", "call #0"},
            "fighter_22404@mutant": {"call #1 memory"}, "fighter_22404@signed": {"call #2"},
            "fighter_211f0@mutant": {"call #2"}, "fighter_211f0@order": {"call #9 memory"},
            "fighter_211f0@zext": {"call #4"}, "fighter_211f0@slot": {"call #5"},
            "fighter_22588@mutant": {"call #1"}, "fighter_22588@order": {"call #4 memory"},
            "fighter_212cc@mutant": {"call #0"}, "fighter_212cc@signed": {"byte"},
            "fighter_212cc@side": {"byte", "call #0"}, "fighter_22638@mutant": {"call #1"},
            "fighter_22638@signed": {"byte"}, "fighter_22638@byte5d": {"byte"},
            "fighter_22638@char": {"byte"}}

# Track P batch 3 (record 2026-10-03-reverse-p3): its rows with their EAX masks, and what alone catches each
# of its mutants.
P3_MASKS = {"fighter_475ec": 0, "fighter_47608": 0, "fighter_47624": 0, "fighter_48964": 0, "fighter_489a0": 0,
            "fighter_47720": 0, "fighter_476fc": 0, "fighter_47648": 0xFFFFFFFF, "fighter_47688": 0,
            "fighter_47874": 0, "fighter_47830": 0, "fighter_47798": 0xFFFFFFFF, "fighter_477a8": 0xFFFFFFFF,
            "fighter_477e8": 0, "fighter_47fcc": 0, "fighter_47cb0": 0xFFFFFFFF, "fighter_47d24": 0,
            "fighter_47e9c": 0, "fighter_48608": 0, "fighter_48054": 0xFFFFFFFF, "fighter_480b4": 0,
            "fighter_48170": 0, "fighter_4811c": 0}
P3_KINDS = {"fighter_475ec@mutant": {"call #0 memory"}, "fighter_475ec@side": {"call #0"},
            "fighter_47608@mutant": {"call #0 memory"}, "fighter_47624@mutant": {"call #0 memory"},
            "fighter_48964@mutant": {"call #1"}, "fighter_48964@late": {"call #0 memory"},
            "fighter_48964@side": {"call #0"}, "fighter_48964@sext": {"call #0"},
            "fighter_489a0@mutant": {"call #0 memory"}, "fighter_489a0@swap": {"call #1"},
            "fighter_489a0@side": {"call #0"}, "fighter_489a0@sext": {"call #0"},
            "fighter_47720@mutant": {"call #0"}, "fighter_47720@ebx": {"byte", "call #0"},
            "fighter_476fc@mutant": {"byte"}, "fighter_476fc@sext": {"byte"},
            "fighter_47648@mutant": {"call #0"},
            "fighter_47688@mutant": {"call #2"}, "fighter_47688@pivot": {"call #2"},
            "fighter_47688@zext": {"call #3"}, "fighter_47688@eax": {"call #1", "call #2", "call #3"},
            "fighter_47874@mutant": {"call #1 memory"}, "fighter_47874@side": {"call #1"},
            "fighter_47874@sext": {"call #1"}, "fighter_47874@early": {"call #0 memory"},
            "fighter_47830@mutant": {"byte", "call #0"}, "fighter_47830@side": {"byte", "call #0"},
            "fighter_47830@order": {"call #0 memory"}, "fighter_47830@sext": {"byte", "call #0"},
            "fighter_47798@mutant": {"call #0"}, "fighter_47798@eax": {"eax"},
            "fighter_477a8@mutant": {"call #0"},
            "fighter_477e8@mutant": {"call #0"}, "fighter_477e8@order": {"call #1 memory"},
            "fighter_47fcc@mutant": {"call #0"}, "fighter_47fcc@order": {"call #0 memory"},
            "fighter_47cb0@mutant": {"call #0"}, "fighter_47cb0@ge": {"eax", "call #0"},
            "fighter_47cb0@lo": {"eax", "call #0"},
            "fighter_47d24@mutant": {"call #2"}, "fighter_47d24@order": {"call #6 memory"},
            "fighter_47d24@signed": {"call #3"}, "fighter_47d24@frame": {"call #1"},
            "fighter_47e9c@mutant": {"byte"}, "fighter_47e9c@slot": {"byte", "call #0"},
            "fighter_47e9c@signed": {"byte"}, "fighter_47e9c@order": {"call #0 memory"},
            "fighter_48608@mutant": {"call #1"}, "fighter_48608@order": {"call #1 memory"},
            "fighter_48608@side": {"byte"},
            "fighter_48054@mutant": {"call #0"}, "fighter_48054@eax": {"eax"},
            "fighter_480b4@mutant": {"call #4"}, "fighter_480b4@signed": {"call #4"},
            "fighter_480b4@char": {"call #4"},
            "fighter_48170@mutant": {"call #5", "call #6"},
            "fighter_48170@order": {"call #3 memory", "call #4 memory"},
            "fighter_48170@reset": {"call #4"},
            "fighter_4811c@mutant": {"call #0"}, "fighter_4811c@signed": {"byte", "call #0"},
            "fighter_4811c@order": {"call #0 memory"}}


@needs_unicorn
@unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
                     "build/diffrun or PRAGE.EXE absent")
class RealFunctionTests(unittest.TestCase):
    """The four ported functions against their original bytes, and the harness's own self-check."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        img = os.path.join(cls.tmp.name, "image.bin")
        cls.real = {r.name: r for r in V.verify_all(DIFFRUN, EXE, img, V.SPECS)}
        cls.mut = {r.name: r for r in V.verify_all(DIFFRUN, EXE, img, V.SPECS, mutants=True)}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_every_ported_function_agrees_with_the_original_on_every_block(self):
        self.assertEqual(sorted(self.real), sorted(["anim_10fa8", "anim_3e4e4", "config_codeword_len",
                                             "config_credit_spend", "fighter_23130", "fighter_3640c",
                                             "fighter_37dcc", "fighter_45878", "fighter_ctx_same",
                                             "fighter_slot_flag", "hit_anim_ctx", "hit_anim_start_b",
                                             "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)
                                            + list(P3_MASKS)))
        for name, r in self.real.items():
            if name == "host_1b890":       # the named gap (record E3 §E3.8), tested on its own below
                continue
            self.assertEqual((r.verdict, r.problems, r.unhit, r.hit), ("VERIFIED", [], [], r.total), name)
            self.assertEqual(r.outside, [], name)

    def test_every_mutant_is_reported_as_a_mismatch(self):
        self.assertEqual(sorted(self.mut), sorted([
            "anim_10fa8@mutant", "anim_3e4e4@mutant",
            "config_codeword_len@mutant", "config_credit_spend@mutant", "config_credit_spend@signed",
            "fighter_23130@novoice", "fighter_23130@reorder", "fighter_23130@voice", "fighter_3640c@mutant", "fighter_37dcc@mutant",
            "fighter_45878@mutant", "fighter_ctx_same@mutant", "fighter_slot_flag@mutant",
            "hit_anim_ctx@mutant", "hit_anim_start_b@mutant", "hit_anim_start_b@set", "rng_next@mutant"]
            + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS)))
        for name, r in self.mut.items():
            self.assertEqual(r.verdict, "MISMATCH", name)

    def test_the_unsigned_guard_case_is_the_only_one_that_catches_the_signed_mutant(self):
        # config_credit_spend 0x2CA7C: `cmp eax,[0x105c00]; ja` is unsigned. n = 0x80000001 against
        # 1 credit is "above" unsigned and negative signed; only case c5 tells them apart.
        r = self.mut["config_credit_spend@signed"]
        self.assertTrue(r.problems and all(p.startswith("c5:") for p in r.problems), r.problems)

    def test_a_forgotten_write_is_caught_by_the_byte_diff_alone(self):
        # fighter_slot_flag@mutant returns the right value and only forgets to set the bit.
        r = self.mut["fighter_slot_flag@mutant"]
        self.assertTrue(r.problems and not any("eax" in p for p in r.problems), r.problems)
        self.assertTrue(any("0x107EE0" in p for p in r.problems), r.problems)

    def test_truncated_real_output_is_rejected_where_it_used_to_verify(self):
        # config_credit_spend's last case is c5 (original: eax 0, no writes). Cutting the real
        # output after `case c5` used to parse as eax 0 / no writes and so agree with it.
        spec = [s for s in V.SPECS if s.name == "config_credit_spend"][0]
        with tempfile.NamedTemporaryFile("w", suffix=".cases", delete=False) as f:
            f.write(V.cases_text(spec, spec.name))
        try:
            full = subprocess.run([DIFFRUN, "--exe", EXE, "--image-out", os.path.join(self.tmp.name, "t.bin"),
                                   "--cases", f.name], capture_output=True, text=True, check=True).stdout
        finally:
            os.unlink(f.name)
        self.assertEqual(sorted(V.parse_port_output(full)), ["c1", "c2", "c3", "c4", "c5"])
        cut = full[:full.index("case c5\n") + len("case c5\n")]
        with self.assertRaises(ValueError):
            V.parse_port_output(cut)

    def test_a_missing_mutant_binding_is_not_counted_as_detected(self):
        spec = dataclasses.replace([s for s in V.SPECS if s.name == "rng_next"][0], mutants=("@doesnotexist",))
        r = V.verify_all(DIFFRUN, EXE, os.path.join(self.tmp.name, "m.bin"), [spec], mutants=True)[0]
        self.assertEqual(r.verdict, "MISMATCH")        # compare() still turns the port error into a problem
        ok, why = V.mutant_detection(r)
        self.assertFalse(ok)
        self.assertIn("unknown binding", why)

    def test_the_self_check_fails_when_a_mutant_binding_is_missing(self):
        spec = dataclasses.replace([s for s in V.SPECS if s.name == "rng_next"][0], mutants=("@doesnotexist",))
        out = io.StringIO()
        with mock.patch.object(V, "SPECS", [spec]), contextlib.redirect_stdout(out):
            rc = V.main(["--diffrun", DIFFRUN, "--exe", EXE, "--image", os.path.join(self.tmp.name, "s.bin"),
                         "--self-check"])
        self.assertEqual(rc, 1)
        self.assertIn("rng_next@doesnotexist: NOT DETECTED (", out.getvalue())
        self.assertIn("0/1 mutants detected", out.getvalue())

    def test_function_selects_one_spec(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = V.main(["--diffrun", DIFFRUN, "--exe", EXE, "--image", os.path.join(self.tmp.name, "f.bin"),
                         "--function", "rng_next", "--self-check"])
        self.assertEqual(rc, 0)
        self.assertIn("1/1 functions VERIFIED; 1/1 mutants detected", out.getvalue())

    def test_the_eax_mask_is_stated_by_the_spec_per_function(self):
        self.assertEqual({s.name: s.eax_mask for s in V.SPECS}, {
            "rng_next": 0xFFFFFFFF, "fighter_slot_flag": 0xFF,
            "config_credit_spend": 0xFFFFFFFF, "config_codeword_len": 0xFFFFFFFF,
            "fighter_3640c": 0, "fighter_37dcc": 0,
            "fighter_23130": 0xFF, "fighter_45878": 0, "anim_10fa8": 0, "anim_3e4e4": 0,
            "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
            **P1_MASKS, **P2_MASKS, **P3_MASKS})
        # with the full mask the slot-flag original's scratch bits (case f9: EAX = 0x201) differ
        spec = dataclasses.replace([s for s in V.SPECS if s.name == "fighter_slot_flag"][0],
                                   eax_mask=0xFFFFFFFF)
        r = V.verify_all(DIFFRUN, EXE, os.path.join(self.tmp.name, "k.bin"), [spec])[0]
        self.assertEqual(r.verdict, "MISMATCH")
        self.assertTrue(any(p.startswith("f9:") and "eax" in p for p in r.problems), r.problems)

    def test_the_slot_flag_case_with_scratch_eax_bits_exists(self):
        # case f9 is what makes the AL mask load-bearing: the original's EAX is 0x201 there.
        spec = [s for s in V.SPECS if s.name == "fighter_slot_flag"][0]
        c = [c for c in spec.cases if c.id == "f9"][0]
        img = E.Image.load(os.path.join(self.tmp.name, "image.bin"))
        o = E.run_original(img, spec.entry, c.regs, c.pokes)
        self.assertEqual(o.regs["eax"], 0x201)


    def test_an_error_case_leaves_no_state_behind(self):
        # 'bad' dirties 0x107EE0 with 0xFFFFFFFF (the clean value is 0, so a leak is visible: the
        # next case would return 1 and write nothing) and then fails the bounds check on its second
        # poke. 'ok' has no poke of its own at 0x107EE0, so it only sees a clean image if 'bad'
        # left none behind; run alone it must give the identical result.
        def case(cid, pokes):
            return "case %s\nfn fighter_slot_flag\nreg eax 5\n%send\n" % (cid, pokes)
        bad = case("bad", "poke 0x107EE0 ffffffff\npoke 0x2000000 01\n")
        ok = case("ok", "")
        img = os.path.join(self.tmp.name, "isolation.bin")
        both = V.run_port(DIFFRUN, EXE, img, bad + ok)
        alone = V.run_port(DIFFRUN, EXE, img, ok)
        self.assertTrue(both["bad"].error)
        self.assertEqual((both["ok"].eax, both["ok"].writes), (0, {0x107EE0: 0x20}))
        self.assertEqual((both["ok"].eax, both["ok"].writes), (alone["ok"].eax, alone["ok"].writes))

    # ---- E3's worked batch (record 2026-10-01-reverse-e3 §E3.6) ----

    def test_each_e3_mutant_is_caught_by_what_it_breaks(self):
        # @reorder (the slot stores moved after the 0x3C4CC call) agrees in EAX, every byte and the call
        # list: only the memory at call #0 tells it apart (record §E3.12). @set pokes the slot's state
        # byte around its call, so the memory at that call differs too.
        kinds = {"fighter_23130@voice": {"call #1"}, "fighter_23130@novoice": {"call #1"},
                 "fighter_23130@reorder": {"call #0 memory"},
                 "fighter_45878@mutant": {"call #0"}, "anim_10fa8@mutant": {"call #0"},
                 "hit_anim_start_b@mutant": {"call #0"}, "hit_anim_start_b@set": {"call #0", "call #0 memory"},
                 "anim_3e4e4@mutant": {"call #0", "byte"},
                 "fighter_ctx_same@mutant": {"byte"}, "hit_anim_ctx@mutant": {"byte"}}
        for name, want in kinds.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)
        # the 0x3C480 arm only: every state byte outside {0,1,2,5,0xE,0x15} takes it (250 of the 256
        # cases); the dispatch set itself is pinned by @set, which moves state 0 alone
        arm = sorted("h%X" % st for st in range(0x100) if st not in (0, 1, 2, 5, 0xE, 0x15))
        self.assertEqual(len(arm), 250)
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["hit_anim_start_b@mutant"].problems}), arm)
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["hit_anim_start_b@set"].problems}), ["h0"])

    def test_each_p1_mutant_is_caught_by_what_it_breaks(self):
        # track P batch 1 (record 2026-10-02-reverse-p1): what alone catches each mutant; every row with a
        # callee has one that only the call list or the memory at a call catches
        for name, want in P1_KINDS.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)
        # x above the threshold with AL set (case a6) is the only case that tells `<` from `!=`
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_23bf8@ne"].problems}), ["a6"])
        # Task 4 review: the boundaries and signedness only the added cases reach (0x23D38 case 0 x at the
        # bound, a negative x and F0AF0 - x = 0x80000000, the word 0xBFFF; the word 0x440 in case 3)
        for name, ids in (("fighter_23d38@ge", ["gE", "gF"]), ("fighter_23d38@unsigned", ["gG", "gH", "gI"]),
                          ("fighter_23d38@bit", ["gJ"]), ("fighter_15584@ge", ["c9"]),
                          ("fighter_1579c@ge", ["c7"]),
                          # Task 5 review: the store rec+0x36 = 0 (0x4027E) shows on t4 alone (its sentinel
                          # 0x3636); without the `neg` (0x23DC1) only gK's 0x80000001 passes
                          ("fighter_401d4@no36", ["t4"]), ("fighter_23d38@noneg", ["gK"]),
                          # final review I1: slot 1's +0x42 bit 2 clear (case 1, c1 alone), the held
                          # record's +0x29 bit 6 set where case 3 clears it (gA alone); I2: case 3's copy
                          # goes to the other slot's +4 record, which differs from the own slot's
                          ("fighter_15584@no42", ["c1"]), ("fighter_1579c@no42", ["c1"]),
                          ("fighter_23d38@noand", ["gA"]), ("fighter_15584@own4", ["c3", "c4", "c5", "c9"]),
                          # I4: an unsigned division differs on the negative distances only (k3, k5)
                          ("fighter_23ca4@mutant", ["k3", "k5"])):
            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)

    def test_each_p2_mutant_is_caught_by_what_it_breaks(self):
        # track P batch 2 (record 2026-10-02-reverse-p2): what alone catches each mutant; every row with a
        # callee has one that only the call list or the memory at a call catches
        for name, want in P2_KINDS.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)
        # the guard's high byte (case g0, slot+8 = 0x01000000) is the only case that tells a dword test from
        # a byte test
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_237d0@guard"].problems}), ["g0"])
        # 0x3DCEC's stream dword: only u1 pokes it; 0x21114's index by rec+0x51, not its other side: every case
        # (each has one pointer, on its own side, so the other index reads 0 or a pointer)
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_3dcec@mutant"].problems}), ["u1"])
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_21114@side"].problems}),
                         ["w0", "w1", "w2", "w3", "w4", "w5", "w6"])
        # the hooks' bounds: only k4's -16..16 tells the signed compares from unsigned ones; only j1's word
        # 0x14 tells `jg` from `jge`; the stub's EAX is returned (k1/k2/k4 differ when the port returns 1)
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_2116c@unsigned"].problems}), ["k4"])
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_22510@ge"].problems}), ["j1"])
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_2116c@eax"].problems}),
                         ["k1", "k2", "k4"])
        # 0x22404's distance is a signed word: only a2's negative entry tells it from a zero-extended one
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_22404@signed"].problems}), ["a2"])
        # 0x212CC: only m2's word -1 tells the signed bound; 0x22638: only pE's count 0x8001, pF's +0x5D 0x80 and pG's count 0x71 (the other slot's character)
        for name, ids in (("fighter_212cc@signed", ["m2"]), ("fighter_22638@signed", ["pE"]),
                          ("fighter_22638@byte5d", ["pF"]), ("fighter_22638@char", ["pG"])):
            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
        # 0x211F0's word at 0xA81B0 is zero-extended (b2 alone pokes it to 0xF000) and its 0x39834 byte is the own
        # slot's +0x5F (the slots' sentinels differ, so every case tells a wrong slot; plan P2 Task 7 review)
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_211f0@zext"].problems}), ["b2"])
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_211f0@slot"].problems}),
                         ["b0", "b1", "b2"])
        # 0x3D10C ignores EBX: g3 (rec+0x51 = 1, side 0) alone tells an index by side, and g4 (rec+0x51 = side = 0x80, so the side index agrees there)
        # alone a `movsx` for the `movzx` (plan P2 Task 2 review)
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_3d10c@side"].problems}), ["g3"])
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_3d10c@sext"].problems}), ["g4"])

    def test_each_p3_mutant_is_caught_by_what_it_breaks(self):
        # track P batch 3 (record 2026-10-03-reverse-p3): what alone catches each mutant; every row with a
        # callee has one that only the call list or the memory at a call catches
        for name, want in P3_KINDS.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)
        # 0x1A570's argument is EBX (side), not rec+0x51: only the cases where the two differ catch it
        for name, ids in (("fighter_475ec@side", ["s0", "s2"]), ("fighter_48964@side", ["q2", "q3"]),
                          ("fighter_489a0@side", ["q2", "q3"]),
                          # their rec+0x51 is read zero-extended (`xor eax,eax; mov al,[edx+0x51]`): q3's 0x80 alone
                          ("fighter_48964@sext", ["q3"]), ("fighter_489a0@sext", ["q3"]),
                          # 0x476FC's byte is zero-extended (c2's 0x80 alone), 0x47688's timer word signed (h4
                          # alone), its 0x39FB0 slot the other one (h1/h3, the cases that reach it)
                          ("fighter_476fc@sext", ["c2"]), ("fighter_47688@zext", ["h4"]),
                          ("fighter_47688@pivot", ["h1", "h3"]),
                          # 0x3B298's AL alone is tested: h3's stub EAX 0x100 (AL 0) alone tells the whole EAX;
                          # 0x3A95C's stance 0xF on every case that reaches it, h5's other +0x54 0x66 included
                          ("fighter_47688@eax", ["h3"]), ("fighter_47688@mutant", ["h2", "h4", "h5"]),
                          # 0x47830 skips only with both bits 0x100 and 0x800 (z1/z2 have one), indexes by
                          # rec+0x51 (every case: the other word takes the other branch), and 0x47798's 1 replaces
                          # the voice's EAX (w1's stub returns 0)
                          ("fighter_47830@mutant", ["z1", "z2"]),
                          ("fighter_47830@side", ["z0", "z1", "z2", "z3", "z4", "z5"]),
                          # rec+0x51 = 0x80 (v2/z5) is read zero-extended: each alone tells a `movsx`
                          ("fighter_47874@sext", ["v2"]), ("fighter_47830@sext", ["z5"]),
                          ("fighter_47798@eax", ["w1"]),
                          # 0x47CB0's bounds (k1's 3 alone tells `jg`, k2's 1 alone `jge`), 0x47D24's distance
                          # signed (b2 alone), 0x47E9C's count signed (eD alone)
                          ("fighter_47cb0@ge", ["k1"]), ("fighter_47cb0@lo", ["k2"]),
                          ("fighter_47d24@signed", ["b2"]), ("fighter_47e9c@signed", ["eD"]),
                          # 0x48054's 1 when the own +0x57 is set (n2, n3: each side), 0x480B4's distance signed
                          # (x2 on side 0, x3 on side 1: each a word of the other slot's character), 0x48170's
                          # 0x36D98 on the other slot (g1, g3: the cases whose 0x468D8 AL is set), 0x4811C's word
                          # signed (i5 alone)
                          ("fighter_48054@eax", ["n2", "n3"]), ("fighter_480b4@signed", ["x2", "x3"]),
                          ("fighter_48170@reset", ["g1", "g3"]), ("fighter_4811c@signed", ["i5"])):
            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)

    def test_each_stub_declares_the_registers_its_callee_clobbers(self):
        # Call.clobbers, re-derived from the bytes (record §E3.5's table, §E3.12)
        img = E.Image.load(os.path.join(self.tmp.name, "image.bin"))
        stubs = {k.addr: k.clobbers for s in V.SPECS for k in s.calls if k.mode == "stub"}
        self.assertEqual(stubs, {0x2C3FC: (), 0x2BC30: ("edx",), 0x3C4CC: ("edx",), 0x3C480: ("edx",),
                                 0x2AE14: ("ebx", "ecx", "edx"), 0x1A570: (), 0x2A17C: ("edx",),
                                 0x188AC: ("edx",), 0x38034: (), 0x34D8C: (),
                                 0x18C14: ("ebx", "edx", "ebp"), 0x18AF8: ("ebx", "ecx", "edx"),
                                 0x39834: ("edx", "ebp"), 0x39A10: ("edx",), 0x3C208: ("edx",), 0x3C358: (),
                                 0x22404: (), 0x36870: ("esi", "edi", "ebp"), 0x35838: ("ebx", "edx"),
                                 0x3B298: ("edx", "edi", "ebp"), 0x39FB0: (), 0x3A95C: ("edx",),
                                 0x3C190: ("edx",), 0x3B714: ("edx",), 0x48170: (), 0x3C148: (), 0x468D8: (),
                                 0x36D98: (), 0x188DC: ("edx",)})
        for addr, declared in stubs.items():
            self.assertEqual(E.callee_clobbers(img, addr), declared, hex(addr))

    # the hand resolutions of E.RESOLVED_JUMPS (record §E3.5, §E3.12): from the guard to the jump, the bytes
    WINDOWS = {
        0x18384: ["cmp al, 6", "ja 0x1838b", "and eax, 0xff", "lea esi, [eax*4]", "mov ecx, 0xcf399",
                  "lea eax, [edx*2]", "add ecx, eax", "jmp dword ptr cs:[esi + 0x18334]"],
        0x29DFE: ["cmp dx, 5", "ja 0x29e2d", "xor ebx, ebx", "mov bx, dx", "jmp dword ptr cs:[ebx*4 + 0x29d70]"],
        0x29E62: ["cmp dx, 5", "ja 0x29ea5", "xor esi, esi", "mov si, dx", "jmp dword ptr cs:[esi*4 + 0x29d88]"],
        0x29EDF: ["cmp dx, 5", "ja 0x29ee7", "xor ecx, ecx", "mov cx, dx", "jmp dword ptr cs:[ecx*4 + 0x29da0]"],
        0x29F78: ["cmp dx, 5", "ja 0x29faf", "xor eax, eax", "mov ax, dx", "jmp dword ptr cs:[eax*4 + 0x29eec]"],
        0x29FE5: ["cmp dx, 5", "ja 0x2a01c", "xor ecx, ecx", "mov cx, dx", "jmp dword ptr cs:[ecx*4 + 0x29f04]"],
        0x2A056: ["cmp dx, 5", "ja 0x2a05e", "xor ebx, ebx", "mov bx, dx", "jmp dword ptr cs:[ebx*4 + 0x29f1c]"],
        0x227BC: ["cmp al, 7", "ja 0x22930", "and eax, 0xff", "lea edx, [eax*4]", "mov eax, dword ptr [esp]",
                  "add eax, eax", "jmp dword ptr cs:[edx + 0x22618]"],
    }

    def test_each_resolved_jump_table_matches_the_bytes(self):
        # the guard bounds the index the jump uses (al masked into eax then esi = eax*4; dx moved into a
        # cleared register), so the table holds the cmp's imm + 1 entries, each a target in the image
        img = E.Image.load(os.path.join(self.tmp.name, "image.bin"))
        self.assertEqual(sorted(E.RESOLVED_JUMPS), sorted(self.WINDOWS))
        for jmp, (guard, table, n) in E.RESOLVED_JUMPS.items():
            got, a = [], guard
            while a <= jmp:
                ins = E.decode_at(img, a)
                got.append("%s %s" % (ins.mnemonic, ins.op_str))
                a += ins.size
            self.assertEqual(got, self.WINDOWS[jmp], hex(jmp))
            cmp = E.decode_at(img, guard)
            self.assertEqual(cmp.operands[1].imm + 1, n, hex(jmp))
            self.assertIn("0x%x]" % table, got[-1])
            targets = E.resolved_cases(img, E.RESOLVED_JUMPS[jmp])
            self.assertTrue(len(targets) == n and all(img.contains(t) for t in targets), hex(jmp))
            # switch_cases does not bound these forms itself: the resolution is what follows them
            self.assertIn(jmp, E.static_scan(img, jmp, switches=True).indirect)

    def test_without_the_resolutions_the_callees_clobber_edi_and_ebp(self):
        # the conservative result: an unresolved jump clobbers everything, which reaches 0x2BC30, 0x3C4CC and
        # 0x3C480 through 0x2A408 -> 0x29F34 (and 0x18350); the resolutions are what keep the declared sets
        img = E.Image.load(os.path.join(self.tmp.name, "image.bin"))
        for addr in (0x2BC30, 0x3C4CC, 0x3C480):
            self.assertEqual(E.callee_clobbers(img, addr, {}), ("edx", "edi", "ebp"), hex(addr))
            self.assertEqual(E.callee_clobbers(img, addr), ("edx",), hex(addr))
        self.assertEqual(E.callee_clobbers(img, 0x2AE14, {}), ("ebx", "ecx", "edx"))

    def test_the_named_gap_is_0x1b890s_in(self):
        r = self.real["host_1b890"]
        self.assertEqual((r.verdict, r.gap, r.problems), ("NAMED_GAP", "in at 0x1B899", []))

    def spec(self, name, **kw):
        return dataclasses.replace([s for s in V.SPECS if s.name == name][0], **kw)

    def test_a_call_set_callee_without_a_port_seam_is_a_mismatch(self):
        # 0x33950 (fighter_ctx_same) has no PR_SEAM: recorded on the original side only
        spec = self.spec("fighter_23130", allow_calls=(),
                         calls=(E.Call(0x33950, (), mode="real"), V.HIT_B, V.VOICE))
        r = V.verify_all(DIFFRUN, EXE, os.path.join(self.tmp.name, "n.bin"), [spec])[0]
        self.assertEqual(r.verdict, "MISMATCH")
        self.assertTrue(r.problems[0].startswith("v0: call #0: original 0x33950(), port 0x3C4CC("), r.problems)

    def test_a_real_callee_runs_on_both_sides_and_its_own_calls_are_recorded(self):
        # 0x3C4CC run, not stubbed, inside 0x23130: its 0x2BC30 or 0x3C480 call joins the list
        spec = self.spec("fighter_23130", allow_calls=(0x33950, 0x339AC),
                         calls=(E.Call(0x3C4CC, ("eax", "edx", "s0"), mode="real"), V.ANIM_BEGIN, V.HIT_A, V.VOICE),
                         cases=[V.Case("r0", {"eax": V.DS_SLOTS, "edx": V.E3_REC, "ebx": 0},
                                       {V.E3_REC + 0x51: b"\x00", V.DS_SLOTS: le32(V.E3_REC2),
                                        V.DS_SLOTS + 0x52: b"\x00"})])
        img = os.path.join(self.tmp.name, "r.bin")
        r = V.verify_all(DIFFRUN, EXE, img, [spec])[0]
        self.assertEqual((r.verdict, r.problems), ("VERIFIED", []))
        o = E.run_original(E.Image.load(img), spec.entry, spec.cases[0].regs, spec.cases[0].pokes,
                           spec.allow_calls, calls=spec.calls)
        # EAX is fighter slot 0 and the record's +0x51 names side 0, so 0x3C4CC reads the +0x52 that
        # 0x23130 has just set to 9 (seeded 0, an arm-0x2BC30 state): the 0x3C480 arm (fighter.c's
        # record §43-C note on 0x23130)
        self.assertEqual([a for a, _ in o.calls], [0x3C4CC, 0x3C480, 0x2C3FC])

    def test_a_stub_write_relative_to_an_argument_lands_on_both_sides(self):
        anim = E.Call(0x2BC30, ("eax", "edx", "s0"), pop=4, writes=((0, 0x52, b"\x33"),))
        spec = self.spec("fighter_45878", calls=(anim,))
        img = os.path.join(self.tmp.name, "w.bin")
        port = V.run_port(DIFFRUN, EXE, img, V.cases_text(spec, spec.name))
        self.assertEqual(port["b0"].writes.get(V.E3_REC + 0x52), 0x33)
        r = V.verify_spec(spec, E.Image.load(img), port)
        self.assertEqual((r.verdict, r.problems), ("VERIFIED", []))

    def test_the_self_check_counts_functions_mutants_gaps_and_closed_rows(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = V.main(["--diffrun", DIFFRUN, "--exe", EXE, "--image", os.path.join(self.tmp.name, "a.bin"),
                         "--self-check"])
        self.assertEqual(rc, 0)
        # the closed-row count is over the rows that have callees (63), the 14 without are counted apart
        self.assertIn("diff-verify: 77/77 functions VERIFIED; 150/150 mutants detected; 1 named gaps; "
                      "11/63 rows with callees closed (14 have none).", out.getvalue())


# ---- E3: the call list, named gaps, the callee column (record 2026-10-01-reverse-e3 §E3.4, §E3.8) --

def program(parts):
    data = bytearray(0x80100)
    for at, h in parts.items():
        b = bytes.fromhex(h)
        data[at - 0x10000:at - 0x10000 + len(b)] = b
    return E.Image(bytes(data))


# 10000: mov eax,5; mov edx,7; call 0x10020; mov [0x80000],eax; ret    10020: (not run when stubbed) ret
CALLER = program({0x10000: "B805000000" "BA07000000" "E811000000" "A300000800" "C3", 0x10020: "C3"})
STUB = E.Call(0x10020, ("eax", "edx"), eax=0x42)
WROTE = {0x80000: 0x42, 0x80001: 0, 0x80002: 0, 0x80003: 0}
SEEDED = {0x80000: le32(0xFFFFFFFF)}


# 10000: mov edx,0x80010; call 0x10020; ret    10020: ret    80010: 11 22 33 44 55 66 77 88
DEREF = program({0x10000: "BA10000800" "E816000000" "C3", 0x10020: "C3", 0x80010: "1122334455667788"})


# 10000: fnstcw [0x80024]; fld dword [0x80010]; fadd qword [0x80018]; fstp dword [0x80020]; ret
# 80010: 1.0f; 80018: the double 2^-24 + 2^-56. The exact sum lies just above the midpoint 1 + 2^-24: one
# rounding to the float (24- or 64-bit precision) gives 0x3F800001; 53-bit precision drops the 2^-56 first,
# lands on the midpoint and rounds to even, 0x3F800000.
X87 = program({0x10000: "D93D24000800" "D90510000800" "DC0518000800" "D91D20000800" "C3",
               0x80010: "0000803F", 0x80018: struct.pack("<d", 2.0 ** -24 + 2.0 ** -56).hex()})


@needs_unicorn
class X87ControlWordTests(unittest.TestCase):
    """The original runs at the game's x87 control word 0x127F (raw 0x72A83, the word at 0xF09B4; record
    2026-10-03-reverse-p3 §P3.6), not unicorn's reset 0x0000."""

    def test_the_original_runs_at_the_games_control_word(self):
        r = E.run_original(X87, 0x10000, pokes={0x80020: b"\xff" * 6})
        self.assertEqual(r.outcome, "ok")
        self.assertEqual(bytes(r.writes.get(0x80024 + k, 0xFF) for k in range(2)), le32(0x127F)[:2])
        self.assertEqual(bytes(r.writes.get(0x80020 + k, 0xFF) for k in range(4)), le32(0x3F800000))


@needs_unicorn
class DerefArgTests(unittest.TestCase):
    """A Call argument `[reg+N]` is the dword at reg + N when the callee is reached (record
    2026-10-02-reverse-p2 §P2.7: 0x18C14's flag bytes live on its caller's stack)."""

    def test_a_deref_argument_reads_the_dword_the_register_points_at(self):
        # the stub overwrites the buffer: the arguments are read before it runs
        stub = E.Call(0x10020, ("edx", "[edx]", "[edx+4]"), writes=((None, 0x80010, b"\0" * 8),))
        r = E.run_original(DEREF, 0x10000, calls=(stub,))
        self.assertEqual(r.outcome, "ok")
        self.assertEqual(r.calls, [(0x10020, (0x80010, 0x44332211, 0x88776655))])

    def test_a_malformed_deref_argument_is_refused(self):
        for a in ("[esp]", "[edx+x]", "[edx-4]", "edx+4", "[edx", "[edx+]", "[edx+\u00b2]"):
            with self.assertRaises(ValueError, msg=a):
                E.Call(0x10020, ("eax", a))

    def test_a_stub_write_based_on_a_deref_argument_is_refused(self):
        # a deref argument is the dword at an address, not an address: it cannot be a write's base
        for base in (1, 2):
            with self.assertRaises(ValueError, msg=base):
                E.Call(0x10020, ("edx", "[edx]", "[edx+4]"), writes=((base, 0, b"\0"),))
        E.Call(0x10020, ("edx", "[edx]"), writes=((0, 0, b"\0"),))


class CallParseTests(unittest.TestCase):
    def test_cases_text_emits_the_call_set(self):
        spec = V.Spec("f", 0x10000, [V.Case("a", {"eax": 5, "s0": 0x40400000})], calls=(
            E.Call(0x10020, ("eax",), eax=1, writes=((None, 0x80008, b"\xab"), (0, 4, b"\x01\x02"))),
            E.Call(0x10030, mode="real")))
        self.assertEqual(V.cases_text(spec, "f"),
                         "case a\nfn f\nreg eax 0x5\nreg s0 0x40400000\n"
                         "stub 0x10020 stub 0x1\nswrite abs 0x80008 ab\nswrite arg0 0x4 0102\n"
                         "stub 0x10030 real 0x0\nend\n")

    def test_call_lines_are_parsed_in_order(self):
        out = V.parse_port_output("case a\nc 0x10020 0x5 0x7\nc 0x10030\nret eax 0x0 mask 0x0\nend\n")
        self.assertEqual(out["a"].calls, [(0x10020, (5, 7)), (0x10030, ())])

    def test_memory_lines_belong_to_the_call_before_them(self):
        out = V.parse_port_output("case a\nc 0x10020\nm 0x80000 0x1\nc 0x10030 0x5\nm 0x80000 0x1\n"
                                  "m 0x80001 0x2\nret eax 0x0 mask 0x0\nend\n")
        self.assertEqual(out["a"].calls, [(0x10020, ()), (0x10030, (5,))])
        self.assertEqual(out["a"].call_mem, [{0x80000: 1}, {0x80000: 1, 0x80001: 2}])

    def test_a_memory_line_outside_a_call_or_repeated_is_rejected(self):
        for name, text in [("before any call", "case a\nm 0x80000 0x1\nret eax 0x0 mask 0x0\nend\n"),
                           ("after the result", "case a\nc 0x10020\nret eax 0x0 mask 0x0\nm 0x80000 0x1\nend\n"),
                           ("twice at one call", "case a\nc 0x10020\nm 0x80000 0x1\nm 0x80000 0x2\n"
                                                 "ret eax 0x0 mask 0x0\nend\n")]:
            with self.subTest(name), self.assertRaises(ValueError):
                V.parse_port_output(text)

    def test_cases_text_emits_each_cases_own_stub_eax(self):
        spec = V.Spec("f", 0x10000, [V.Case("a", {}), V.Case("b", {}, {}, {0x10020: 0x99})],
                      calls=(E.Call(0x10020, eax=0x42),))
        self.assertEqual(V.cases_text(spec, "f"), "case a\nfn f\nstub 0x10020 stub 0x42\nend\n"
                                                  "case b\nfn f\nstub 0x10020 stub 0x99\nend\n")

    def test_a_stub_eax_must_name_a_stub_of_the_call_set(self):
        for calls in ((), (E.Call(0x10020, mode="real"),)):
            with self.subTest(calls=calls), self.assertRaises(ValueError):
                V.Spec("f", 0x10000, [V.Case("a", {}, {}, {0x10020: 1})], calls=calls)

    def test_only_rows_with_callees_count_toward_the_closed_figure(self):
        def row(entry, verdict, callees):
            return V.SpecResult("r%X" % entry, entry, verdict, callees=callees)
        a = row(0xA, "VERIFIED", [])                      # no callee: counted apart
        b = row(0xB, "VERIFIED", [(0xA, "stub")])         # its callee is VERIFIED: closed
        c = row(0xC, "VERIFIED", [(0xA, "stub"), (0xF, "stub")])   # 0xF has no row
        d = row(0xD, "MISMATCH", [(0xA, "allow")])
        verdicts = {r.entry: r.verdict for r in (a, b, c, d)}
        closed, with_callees = V.closed_rows([a, b, c, d], verdicts)
        self.assertEqual(([r.entry for r in closed], [r.entry for r in with_callees]), ([0xB], [0xB, 0xC, 0xD]))

    def test_a_call_line_after_the_result_is_rejected(self):
        with self.assertRaises(ValueError):
            V.parse_port_output("case a\nret eax 0x0 mask 0x0\nc 0x10020\nend\n")

    def test_an_address_both_allowed_and_in_the_call_set_is_refused(self):
        with self.assertRaises(ValueError):
            V.Spec("f", 0x10000, [], allow_calls=(0x10020,), calls=(E.Call(0x10020),))

    def test_the_table_marks_each_callee_with_its_own_check(self):
        r = V.SpecResult("f", 0x10000, "VERIFIED", 1, 1, 1, callees=[(0x10020, "stub"), (0x10030, "allow")])
        self.assertIn("| 10020 stub VERIFIED, 10030 allow unverified |", V.table_row(r, {0x10020: "VERIFIED"}))


@needs_unicorn
class CallVerifyTests(unittest.TestCase):
    def spec(self, **kw):
        return V.Spec("c", 0x10000, [V.Case("x", {}, SEEDED)], calls=(STUB,), **kw)

    def test_agreeing_calls_verify(self):
        r = V.verify_spec(self.spec(), CALLER, {"x": V.PortResult(0x42, writes=WROTE, calls=[(0x10020, (5, 7))])})
        self.assertEqual((r.verdict, r.hit, r.total, r.problems), ("VERIFIED", 1, 1, []))

    def test_a_missing_call_is_a_mismatch_and_counts_as_a_detection(self):
        r = V.verify_spec(self.spec(), CALLER, {"x": V.PortResult(0x42, writes=WROTE)})
        self.assertEqual((r.verdict, r.problems), ("MISMATCH", ["x: call #0: original 0x10020(0x5, 0x7), port none"]))
        self.assertEqual(V.mutant_detection(r), (True, ""))

    def test_a_call_with_another_argument_is_a_mismatch(self):
        r = V.verify_spec(self.spec(), CALLER, {"x": V.PortResult(0x42, writes=WROTE, calls=[(0x10020, (5, 8))])})
        self.assertEqual(r.problems, ["x: call #0: original 0x10020(0x5, 0x7), port 0x10020(0x5, 0x8)"])

    def test_an_extra_call_is_a_mismatch(self):
        r = V.verify_spec(self.spec(), CALLER, {"x": V.PortResult(
            0x42, writes=WROTE, calls=[(0x10020, (5, 7)), (0x10020, (5, 7))])})
        self.assertEqual(r.problems, ["x: call #1: original none, port 0x10020(0x5, 0x7)"])

    def test_an_indirect_call_that_resolved_does_not_keep_the_function_partial(self):
        img = program({0x10000: "FFD0" "C3", 0x10020: "C3"})
        spec = V.Spec("i", 0x10000, [V.Case("x", {"eax": 0x10020})], calls=(E.Call(0x10020, ("eax",)),))
        r = V.verify_spec(spec, img, {"x": V.PortResult(0, calls=[(0x10020, (0x10020,))])})
        self.assertEqual((r.verdict, r.problems), ("VERIFIED", []))

    def test_an_args_free_indirect_target_against_a_port_reporting_args_is_a_mismatch(self):
        # D3: the spec declares args=() for the indirect target (compared by address only); a port that
        # later registers it reports its arguments, and the row must turn MISMATCH until the spec names them
        img = program({0x10000: "FFD0" "C3", 0x10020: "C3"})
        spec = V.Spec("i", 0x10000, [V.Case("x", {"eax": 0x10020})], calls=(E.Call(0x10020, ()),))
        r = V.verify_spec(spec, img, {"x": V.PortResult(0, calls=[(0x10020, ())])})
        self.assertEqual((r.verdict, r.problems), ("VERIFIED", []))
        r = V.verify_spec(spec, img, {"x": V.PortResult(0, calls=[(0x10020, (0x10020,))])})
        self.assertEqual((r.verdict, r.problems), ("MISMATCH", ["x: call #0: original 0x10020(), port 0x10020(0x10020)"]))

    # ---- E3 final review (record §E3.12) ----

    # 10000: mov byte [0x80000],1; call 0x10020; mov byte [0x80001],2; call 0x10020; ret    10020: int3
    TWO_CALLS = program({0x10000: "C60500000800" "01" "E814000000" "C60501000800" "02" "E808000000" "C3",
                         0x10020: "CC"})

    def test_a_store_moved_across_a_stubbed_call_is_a_mismatch(self):
        spec = V.Spec("m", 0x10000, [V.Case("x", {}, {0x80000: b"\xff\xff"})], calls=(E.Call(0x10020),))
        final, calls = {0x80000: 1, 0x80001: 2}, [(0x10020, ()), (0x10020, ())]
        good = V.PortResult(0, 0, final, calls=calls, call_mem=[{0x80000: 1}, dict(final)])
        r = V.verify_spec(dataclasses.replace(spec, eax_mask=0), self.TWO_CALLS, {"x": good})
        self.assertEqual((r.verdict, r.problems), ("VERIFIED", []))
        late = V.PortResult(0, 0, final, calls=calls, call_mem=[{}, dict(final)])    # the store after the call
        r = V.verify_spec(dataclasses.replace(spec, eax_mask=0), self.TWO_CALLS, {"x": late})
        self.assertEqual(r.problems, ["x: call #0 memory: 1 bytes changed so far in the original, 0 in the port; "
                                      "first difference at 0x80000: original 0x01, port unchanged"])
        self.assertEqual(V.mutant_detection(r), (True, ""))

    # 10000: mov edx,7; mov ebx,3; call 0x10020; mov [0x80000],edx; mov [0x80004],ebx; ret    10020: int3
    READS_EDX = program({0x10000: "BA07000000" "BB03000000" "E811000000" "891500000800" "891D04000800" "C3",
                         0x10020: "CC"})

    def test_a_caller_that_reads_a_clobbered_register_diverges_from_the_port(self):
        # the port's C cannot read a callee's EDX: what it computes is what the caller would with EDX
        # preserved (7). With the clobber declared the original reads the poison instead, so the row is
        # a MISMATCH: such a caller needs that callee run `real`, not stubbed (record §E3.12)
        seed = {0x80000: le32(0xFFFFFFFF), 0x80004: le32(0xFFFFFFFF)}
        port = V.PortResult(0, 0, {0x80000: 7, 0x80001: 0, 0x80002: 0, 0x80003: 0,
                                   0x80004: 3, 0x80005: 0, 0x80006: 0, 0x80007: 0}, calls=[(0x10020, ())])
        spec = V.Spec("k", 0x10000, [V.Case("x", {}, seed)], eax_mask=0, calls=(E.Call(0x10020),))
        r = V.verify_spec(spec, self.READS_EDX, {"x": port})
        self.assertEqual((r.verdict, r.problems), ("VERIFIED", []))
        spec = dataclasses.replace(spec, calls=(E.Call(0x10020, clobbers=("edx",)),))
        r = V.verify_spec(spec, self.READS_EDX, {"x": port})
        self.assertEqual(r.verdict, "MISMATCH")
        self.assertEqual(r.problems[0], "x: byte 0x80000: original 0x%02X, port 0x07" % (E.CLOBBER_POISON & 0xFF))

    def test_a_stub_eax_varied_per_case_catches_a_port_that_hard_codes_it(self):
        # CALLER stores the stub's EAX: a port that writes the constant 0x42 agrees with a fixed stub EAX
        cases = [V.Case("a", {}, SEEDED), V.Case("b", {}, SEEDED, {0x10020: 0x99})]
        fixed = V.PortResult(0x42, writes=WROTE, calls=[(0x10020, (5, 7))])
        r = V.verify_spec(V.Spec("c", 0x10000, cases[:1], calls=(STUB,)), CALLER, {"a": fixed})
        self.assertEqual((r.verdict, r.problems), ("VERIFIED", []))
        r = V.verify_spec(V.Spec("c", 0x10000, cases, calls=(STUB,)), CALLER, {"a": fixed, "b": fixed})
        self.assertEqual(r.verdict, "MISMATCH")
        self.assertTrue(r.problems and all(p.startswith("b:") for p in r.problems), r.problems)
        passed = V.PortResult(0x99, writes={**WROTE, 0x80000: 0x99}, calls=[(0x10020, (5, 7))])
        r = V.verify_spec(V.Spec("c", 0x10000, cases, calls=(STUB,)), CALLER, {"a": fixed, "b": passed})
        self.assertEqual((r.verdict, r.problems), ("VERIFIED", []))

    # 10000: cmp al,2; ja 10010; and eax,0xff; jmp [eax*4+0x10100]; (10010) ret; 10011..10013 ret
    SWITCH = program({0x10000: "3C02" "770C" "25FF000000" "FF248500010100" "C3" "C3C3C3",
                      0x10100: "11000100" "12000100" "13000100"})

    def test_a_bounded_switch_counts_its_cases_as_blocks(self):
        cases = [V.Case("s%d" % n, {"eax": n}) for n in range(4)]
        port = {"s%d" % n: V.PortResult(n) for n in range(4)}
        r = V.verify_spec(V.Spec("s", 0x10000, cases), self.SWITCH, port)
        self.assertEqual((r.verdict, r.hit, r.total), ("VERIFIED", 6, 6))
        r = V.verify_spec(V.Spec("s", 0x10000, cases[:3]), self.SWITCH, {k: port[k] for k in ("s0", "s1", "s2")})
        self.assertEqual((r.verdict, r.unhit), ("PARTIAL", [0x10010]))


@needs_unicorn
class GapTests(unittest.TestCase):
    IN = program({0x10000: "EC" "C3"})           # in al,dx; ret

    def test_a_named_gap_holds_when_every_case_stops_on_it(self):
        r = V.verify_gap(V.Spec("g", 0x10000, [V.Case("x", {})], mutants=(), gap="in at 0x10000"), self.IN)
        self.assertEqual((r.verdict, r.problems, r.gap), ("NAMED_GAP", [], "in at 0x10000"))

    def test_a_gap_spec_with_no_cases_is_refused(self):
        with self.assertRaises(ValueError):
            V.Spec("g", 0x10000, [], mutants=(), gap="in at 0x10000")

    def test_a_misnamed_gap_is_a_mismatch(self):
        r = V.verify_gap(V.Spec("g", 0x10000, [V.Case("x", {})], mutants=(), gap="in at 0x10001"), self.IN)
        self.assertEqual(r.verdict, "MISMATCH")

    def test_a_gap_the_original_runs_through_is_stale(self):
        r = V.verify_gap(V.Spec("g", 0x10000, [V.Case("x", {})], mutants=(), gap="in at 0x10000"),
                         program({0x10000: "C3"}))
        self.assertEqual(r.verdict, "MISMATCH")
        self.assertIn("the original ok (returned)", r.problems[0])


@unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
                     "build/diffrun or PRAGE.EXE absent")
class DiffrunStubTests(unittest.TestCase):
    """The port side of the call stubs (record E3 §E3.3): diffrun's stub lines and the PR_SEAM hook."""

    def run_text(self, text):
        with tempfile.TemporaryDirectory() as d:
            return V.run_port(DIFFRUN, EXE, os.path.join(d, "img"), text)

    def test_stubbed_callees_are_recorded_with_their_c_arguments_in_order(self):
        # 0x23130: slot 0x10A200 (+0x52..+0x54 seeded 7F), rec 0x10A300, side 1
        out = self.run_text("case a\nfn fighter_23130\nreg eax 0x10A200\nreg edx 0x10A300\nreg ebx 0x1\n"
                            "poke 0x10A252 7f7f7f\nstub 0x3C4CC stub 0x0\nstub 0x2C3FC stub 0x1\nend\n")["a"]
        self.assertEqual(out.calls, [(0x3C4CC, (0x10A300, 0xE4872, 0x40400000)), (0x2C3FC, (0x7C,))])
        self.assertEqual((out.eax, out.error), (1, ""))
        self.assertEqual([out.writes.get(a) for a in (0x10A252, 0x10A253, 0x10A254)], [9, 7, 0])

    def test_the_port_reports_the_memory_at_each_call(self):
        # 0x23130 stores the slot's +0x52..+0x54 and +0x0C before 0x3C4CC (seeded 7F and FFFFFFFF, so each
        # store is a change); @reorder makes them after it, so its first call sees none of them
        text = ("case a\nfn %s\nreg eax 0x10A200\nreg edx 0x10A300\nreg ebx 0x1\n"
                "poke 0x10A252 7f7f7f\npoke 0x10A20C ffffffff\nstub 0x3C4CC stub 0x0\nstub 0x2C3FC stub 0x1\nend\n")
        stores = {0x10A252: 9, 0x10A253: 7, 0x10A254: 0, 0x10A20C: 0, 0x10A20D: 0, 0x10A20E: 0, 0x10A20F: 0}
        out = self.run_text(text % "fighter_23130")["a"]
        self.assertEqual(out.call_mem, [stores, stores])
        out = self.run_text(text % "fighter_23130@reorder")["a"]
        self.assertEqual(out.call_mem, [{}, stores])
        self.assertEqual(out.writes, stores)

    def test_a_real_callee_runs_and_its_own_calls_are_recorded(self):
        # slot 0 is the fighter slot and the record's +0x51 names side 0, so 0x3C4CC reads the +0x52
        # that 0x23130 has just set to 9 (seeded 0): its 0x3C480 arm, with the slot's record 0x10A400
        out = self.run_text("case r\nfn fighter_23130\nreg eax 0x1077B0\nreg edx 0x10A300\nreg ebx 0x0\n"
                            "poke 0x1077B0 00a41000\npoke 0x107802 00\npoke 0x10A351 00\n"
                            "stub 0x3C4CC real 0x0\nstub 0x2BC30 stub 0x0\nstub 0x3C480 stub 0x0\n"
                            "stub 0x2C3FC stub 0x1\nend\n")["r"]
        self.assertEqual(out.calls, [(0x3C4CC, (0x10A300, 0xE4872, 0x40400000)),
                                     (0x3C480, (0x10A400, 0xE4872, 0x40400000)), (0x2C3FC, (0x7C,))])

    def test_stack_slots_reach_the_binding(self):
        # 0x3C4CC alone: side 1 (+0x51), slot 1's record 0x10A400 and state 5: the 0x2BC30 arm
        out = self.run_text("case s\nfn hit_anim_start_b\nreg eax 0x10A300\nreg edx 0xE4872\nreg s0 0x40400000\n"
                            "poke 0x10A351 01\npoke 0x107844 00a41000\npoke 0x107896 05\n"
                            "stub 0x2BC30 stub 0x0\nstub 0x3C480 stub 0x0\nend\n")["s"]
        self.assertEqual(out.calls, [(0x2BC30, (0x10A400, 0xE4872, 0x40400000))])

    def test_stub_writes_land_at_an_address_or_an_argument_plus_offset(self):
        out = self.run_text("case w\nfn fighter_45878\nreg eax 0x10A200\nreg edx 0x10A300\n"
                            "stub 0x2BC30 stub 0x0\nswrite abs 0x10A500 5a\nswrite arg0 0x52 33\nend\n")["w"]
        self.assertEqual((out.writes.get(0x10A500), out.writes.get(0x10A352)), (0x5A, 0x33))

    def test_a_stub_write_outside_the_image_is_an_error(self):
        out = self.run_text("case x\nfn fighter_45878\nreg eax 0x10A200\nreg edx 0x10A300\n"
                            "stub 0x2BC30 stub 0x0\nswrite abs 0x2000000 01\nend\n")["x"]
        self.assertEqual(out.error, "a stub write outside the image")

    def test_the_animation_targets_are_registered(self):
        out = self.run_text("case t\nfn anim_10fa8\nstub 0x2AE14 stub 0x0\nend\n")["t"]
        self.assertEqual(out.calls, [(0x2AE14, (0x9AD08, 0, 0xE4, 0, 0))])

    def test_a_malformed_stub_line_is_refused(self):
        for line in ("stub 0x2BC30 skip 0x0", "stub 0x2BC30 stub", "swrite abs 0x10A500 5a"):
            with self.subTest(line=line), self.assertRaises(RuntimeError):
                self.run_text("case m\nfn fighter_45878\n%s\nend\n" % line)

    # ---- Task 6 review folds (e7, e8) ----

    def test_a_stub_write_relative_to_argument_1_lands_there(self):
        # 0x2BC30's arguments are (rec, stream, frame): arg1 is the stream, whatever arg0 is
        out = self.run_text("case a\nfn fighter_45878\nreg eax 0x10A200\nreg edx 0x10A300\n"
                            "stub 0x2BC30 stub 0x0\nswrite arg1 0x10 a7\nend\n")["a"]
        stream = out.calls[0][1][1]
        self.assertNotEqual(stream, out.calls[0][1][0])
        self.assertEqual((out.error, out.writes.get(stream + 0x10)), ("", 0xA7))
        self.assertNotIn(out.calls[0][1][0] + 0x10, out.writes)

    def test_a_stub_write_naming_an_argument_the_callee_does_not_report_is_refused(self):
        # 0x2AE14 reports five arguments (arg0..arg4); arg5 does not exist
        out = self.run_text("case n\nfn anim_10fa8\nstub 0x2AE14 stub 0x0\nswrite arg5 0x0 5a\nend\n")["n"]
        self.assertEqual(out.error, "a stub write names argument 5, but 0x2AE14 reports 5 arguments")
        self.assertEqual(out.writes, {})

    def test_a_swrite_after_a_real_stub_line_is_refused(self):
        # a real callee declares no effect (Python's Call refuses the same)
        with self.assertRaises(RuntimeError):
            self.run_text("case r\nfn fighter_45878\nreg eax 0x10A200\nreg edx 0x10A300\n"
                          "stub 0x2BC30 real 0x0\nswrite abs 0x10A500 5a\nend\n")

    def test_the_argument_index_of_a_swrite_is_a_plain_decimal(self):
        for base in ("arg0x1", "arg+1", "arg-1", "arg", "arg1x", "arg8"):
            with self.subTest(base=base), self.assertRaises(RuntimeError):
                self.run_text("case d\nfn fighter_45878\nreg eax 0x10A200\nreg edx 0x10A300\n"
                              "stub 0x2BC30 stub 0x0\nswrite %s 0x0 5a\nend\n" % base)
        # what cases_text emits (decimal) is accepted: arg1, and arg8 is refused above
        out = self.run_text("case d\nfn fighter_45878\nreg eax 0x10A200\nreg edx 0x10A300\n"
                            "stub 0x2BC30 stub 0x0\nswrite arg1 0x10 5a\nend\n")["d"]
        self.assertEqual(out.error, "")

    def test_every_fn_register_call_site_is_reached_by_the_registration(self):
        # the fail-closed rule of D3 holds for every ported code pointer: an address a ported module
        # registers resolves in diffrun (no miss reported); an unregistered one is reported
        import re
        sites = set()
        addrs = set()
        symbols = open(os.path.join(ROOT, "port/src/symbols.h")).read()
        for dirpath, _, files in os.walk(os.path.join(ROOT, "port/src")):
            for f in files:
                if not f.endswith(".c") or f == "mem.c":
                    continue
                text = open(os.path.join(dirpath, f)).read()
                for m in re.finditer(r"\bfn_register\(\s*(0x[0-9A-Fa-f]+|FN_[0-9A-Fa-f]+)", text):
                    sites.add(f)
                    a = m.group(1)
                    if a.startswith("FN_"):
                        a = re.search(r"#define %s (0x[0-9A-Fa-f]+)u" % a, symbols).group(1)
                    addrs.add(int(a, 16))
        self.assertEqual(sites, {"actors.c", "attract.c", "effects.c", "svcmenu.c"})
        self.assertGreater(len(addrs), 90)
        for want in (0x1324C, 0x4F7F4, 0x2CB74, 0x2CAC0, 0x10FA8):
            self.assertIn(want, addrs)
        # a stub line for the address makes a reported miss visible as a `c` line
        text = "".join("case r%X\nfn fn_resolved\nreg eax 0x%X\nstub 0x%X stub 0x0\nend\n" % (a, a, a)
                       for a in sorted(addrs))
        text += "case miss\nfn fn_resolved\nreg eax 0x10020\nstub 0x10020 stub 0x0\nend\n"
        out = self.run_text(text)
        for a in sorted(addrs):
            self.assertEqual((out["r%X" % a].eax, out["r%X" % a].calls), (1, []), hex(a))
        self.assertEqual((out["miss"].eax, out["miss"].calls), (0, [(0x10020, ())]))


if __name__ == "__main__":
    unittest.main()
