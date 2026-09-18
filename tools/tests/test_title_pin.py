# tools/tests/test_title_pin.py
import os, shutil, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOL = os.path.join(ROOT, "tools", "title_pin.py")
EXE = os.path.join(ROOT, "data", "game", "C", "PRAGE.EXE")

# Format reference A2. The three title draws carry the ranges the port's LCG
# must reproduce; the opcode-8 site is the in-window consumer (value 0 both sides).
DRAW_SITES = [
    (0x650E9, bytes.fromhex("e842b50400"), bytes.fromhex("b80c000000"), 0x5A),
    (0x650F5, bytes.fromhex("e836b50400"), bytes.fromhex("b86f000000"), 0x7E),
    (0x6510B, bytes.fromhex("e820b50400"), bytes.fromhex("b800000000"), 2),
]
PATCH_SITES = DRAW_SITES + [
    (0x7E289, bytes.fromhex("e8a2230300"), bytes.fromhex("b800000000"), None),
]

def lcg_draws():
    s, out = 0xABCD, []
    for _off, _orig, _repl, rng in DRAW_SITES:
        s = (s * 0xB90D12B9 + 0x38CE051F) & 0xFFFFFFFF
        out.append(((s >> 16) * (rng & 0xFFFF)) >> 16)
    return out

class TitlePinTest(unittest.TestCase):
    def run_tool(self, src, out):
        return subprocess.run([sys.executable, TOOL, "--src", src, "--out", out],
                              capture_output=True, text=True)

    def test_patches_all_sites_and_nothing_else(self):
        with tempfile.TemporaryDirectory() as d:
            out = os.path.join(d, "PRAGE_PIN.EXE")
            r = self.run_tool(EXE, out)
            self.assertEqual(r.returncode, 0, r.stderr)
            with open(EXE, "rb") as fh: a = fh.read()
            with open(out, "rb") as fh: b = fh.read()
            self.assertEqual(len(a), len(b))
            expect = bytearray(a)
            for off, _orig, repl, _rng in PATCH_SITES:
                expect[off:off + 5] = repl
            self.assertEqual(b, bytes(expect))

    def test_patch_sites_hold_the_original_calls(self):
        with open(EXE, "rb") as fh: a = fh.read()
        for off, orig, _repl, _rng in PATCH_SITES:
            self.assertEqual(a[off:off + 5], orig, hex(off))

    def test_patched_values_are_the_lcg_results_for_their_ranges(self):
        # The immediates must equal rng_seed(0xABCD) + the real draws, or the
        # port (which takes the real draws) would diverge on the entry frame.
        vals = lcg_draws()
        self.assertEqual(vals, [12, 111, 0])
        for (_off, _orig, repl, _rng), v in zip(DRAW_SITES, vals):
            self.assertEqual(int.from_bytes(repl[1:], "little"), v)

    def test_refuses_an_already_patched_file(self):
        with tempfile.TemporaryDirectory() as d:
            once = os.path.join(d, "one.exe")
            twice = os.path.join(d, "two.exe")
            self.assertEqual(self.run_tool(EXE, once).returncode, 0)
            r = self.run_tool(once, twice)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("site", r.stderr)
            self.assertFalse(os.path.exists(twice))

    def test_refuses_a_wrong_file_and_writes_nothing(self):
        with tempfile.TemporaryDirectory() as d:
            bad = os.path.join(d, "bad.exe")
            out = os.path.join(d, "out.exe")
            with open(bad, "wb") as fh: fh.write(b"\x00" * 4096)
            r = self.run_tool(bad, out)
            self.assertNotEqual(r.returncode, 0)
            self.assertFalse(os.path.exists(out))

    def test_refuses_an_out_under_data(self):
        # data/ is git-ignored and this repo has no remote: overwriting it
        # would destroy the only copy of the game.
        out = os.path.join(ROOT, "data", "game", "C", "PRAGE_PIN.EXE")
        r = self.run_tool(EXE, out)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn(out, r.stderr)
        self.assertFalse(os.path.exists(out))

    def test_refuses_an_out_under_data_mixed_case(self):
        # APFS is case-insensitive: Data/ resolves to the real data/ dir. Assert
        # refusal directly, without assuming the host filesystem's case rules.
        out = os.path.join(ROOT, "Data", "game", "C", "PRAGE_PIN.EXE")
        r = self.run_tool(EXE, out)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn(out, r.stderr)
        self.assertFalse(os.path.exists(out))

    def test_refuses_a_missing_src(self):
        with tempfile.TemporaryDirectory() as d:
            missing = os.path.join(d, "nope.exe")
            out = os.path.join(d, "out.exe")
            r = self.run_tool(missing, out)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn(missing, r.stderr)
            self.assertFalse(os.path.exists(out))

    def test_refuses_out_equal_to_src(self):
        with tempfile.TemporaryDirectory() as d:
            victim = os.path.join(d, "PRAGE.EXE")
            shutil.copyfile(EXE, victim)
            with open(victim, "rb") as fh: before = fh.read()
            r = self.run_tool(victim, victim)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn(victim, r.stderr)
            with open(victim, "rb") as fh: self.assertEqual(fh.read(), before)

    def test_never_writes_under_data(self):
        with tempfile.TemporaryDirectory() as d:
            out = os.path.join(d, "PRAGE_PIN.EXE")
            self.run_tool(EXE, out)
            for root, _, files in os.walk(os.path.join(ROOT, "data")):
                for f in files:
                    if f.endswith("_PIN.EXE"):
                        self.fail(f"pin wrote {os.path.join(root, f)}")

if __name__ == "__main__":
    unittest.main()
