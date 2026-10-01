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
        self.assertEqual(sorted(self.real), ["anim_10fa8", "anim_3e4e4", "config_codeword_len",
                                             "config_credit_spend", "fighter_23130", "fighter_3640c",
                                             "fighter_37dcc", "fighter_45878", "fighter_ctx_same",
                                             "fighter_slot_flag", "hit_anim_ctx", "hit_anim_start_b",
                                             "host_1b890", "rng_next"])
        for name, r in self.real.items():
            if name == "host_1b890":       # the named gap (record E3 §E3.8), tested on its own below
                continue
            self.assertEqual((r.verdict, r.problems, r.unhit, r.hit), ("VERIFIED", [], [], r.total), name)
            self.assertEqual(r.outside, [], name)

    def test_every_mutant_is_reported_as_a_mismatch(self):
        self.assertEqual(sorted(self.mut), [
            "anim_10fa8@mutant", "anim_3e4e4@mutant",
            "config_codeword_len@mutant", "config_credit_spend@mutant", "config_credit_spend@signed",
            "fighter_23130@novoice", "fighter_23130@voice", "fighter_3640c@mutant", "fighter_37dcc@mutant",
            "fighter_45878@mutant", "fighter_ctx_same@mutant", "fighter_slot_flag@mutant",
            "hit_anim_ctx@mutant", "hit_anim_start_b@mutant", "hit_anim_start_b@set", "rng_next@mutant"])
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
            "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF})
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
        kinds = {"fighter_23130@voice": {"call #1"}, "fighter_23130@novoice": {"call #1"},
                 "fighter_45878@mutant": {"call #0"}, "anim_10fa8@mutant": {"call #0"},
                 "hit_anim_start_b@mutant": {"call #0"}, "hit_anim_start_b@set": {"call #0"},
                 "anim_3e4e4@mutant": {"call #0", "byte"},
                 "fighter_ctx_same@mutant": {"byte"}, "hit_anim_ctx@mutant": {"byte"}}
        for name, want in kinds.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)
        # the 0x3C480 arm only: every state outside {0,1,2,5,0xE,0x15} takes it, in cases 0..0x16 and the
        # three out-of-range bytes; the dispatch set itself is pinned by @set, which moves state 0 alone
        arm = sorted("h%X" % st for st in list(range(0x17)) + [0x7F, 0x80, 0xFF]
                     if st not in (0, 1, 2, 5, 0xE, 0x15))
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["hit_anim_start_b@mutant"].problems}), arm)
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["hit_anim_start_b@set"].problems}), ["h0"])

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
        self.assertIn("diff-verify: 13/13 functions VERIFIED; 16/16 mutants detected; 1 named gaps; "
                      "8/13 with every callee VERIFIED.", out.getvalue())


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
