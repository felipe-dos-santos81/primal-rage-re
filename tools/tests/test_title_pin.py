# tools/tests/test_title_pin.py
import os, shutil, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOL = os.path.join(ROOT, "tools", "title_pin.py")
EXE = os.path.join(ROOT, "data", "game", "C", "PRAGE.EXE")

SIG = bytes.fromhex("535225ffff0000 8bd8a1d8f60600".replace(" ", ""))
STUB = bytes.fromhex("31c0c3")

class TitlePinTest(unittest.TestCase):
    def run_tool(self, src, out):
        return subprocess.run([sys.executable, TOOL, "--src", src, "--out", out],
                              capture_output=True, text=True)

    def test_patches_stub_and_leaves_the_rest_identical(self):
        with tempfile.TemporaryDirectory() as d:
            out = os.path.join(d, "PRAGE_PIN.EXE")
            r = self.run_tool(EXE, out)
            self.assertEqual(r.returncode, 0, r.stderr)
            with open(EXE, "rb") as fh:
                a = fh.read()
            with open(out, "rb") as fh:
                b = fh.read()
            self.assertEqual(len(a), len(b))
            self.assertEqual(b, a[:0xB0630] + STUB + a[0xB0633:])

    def test_patch_site_holds_the_original_signature(self):
        with open(EXE, "rb") as fh:
            a = fh.read()
        self.assertEqual(a[0xB0630:0xB0630 + len(SIG)], SIG)

    def test_refuses_an_already_patched_file(self):
        with tempfile.TemporaryDirectory() as d:
            once = os.path.join(d, "one.exe")
            twice = os.path.join(d, "two.exe")
            self.assertEqual(self.run_tool(EXE, once).returncode, 0)
            r = self.run_tool(once, twice)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("signature", r.stderr)
            self.assertFalse(os.path.exists(twice))

    def test_refuses_a_wrong_file_and_writes_nothing(self):
        with tempfile.TemporaryDirectory() as d:
            bad = os.path.join(d, "bad.exe")
            out = os.path.join(d, "out.exe")
            with open(bad, "wb") as fh:
                fh.write(b"\x00" * 4096)
            r = self.run_tool(bad, out)
            self.assertNotEqual(r.returncode, 0)
            self.assertFalse(os.path.exists(out))

    def test_never_writes_under_data(self):
        with tempfile.TemporaryDirectory() as d:
            out = os.path.join(d, "PRAGE_PIN.EXE")
            self.run_tool(EXE, out)
            for root, _, files in os.walk(os.path.join(ROOT, "data")):
                for f in files:
                    if f.endswith("_PIN.EXE"):
                        self.fail(f"pin wrote {os.path.join(root, f)}")

    def test_refuses_an_out_under_data(self):
        out = os.path.join(ROOT, "data", "game", "C", "PRAGE_PIN.EXE")
        r = self.run_tool(EXE, out)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn(out, r.stderr)
        self.assertFalse(os.path.exists(out))

    def test_refuses_out_equal_to_src(self):
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "PRAGE.EXE")
            shutil.copyfile(EXE, src)
            with open(src, "rb") as fh:
                before = fh.read()
            r = self.run_tool(src, src)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn(src, r.stderr)
            with open(src, "rb") as fh:
                self.assertEqual(fh.read(), before)

if __name__ == "__main__":
    unittest.main()
