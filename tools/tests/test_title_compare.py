# tools/tests/test_title_compare.py
import os, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOL = os.path.join(ROOT, "tools", "title_compare.py")

FRAME_W, FRAME_H = 320, 200
FRAME_BYTES = FRAME_W * FRAME_H * 3


def write_frame(d, i, fill):
    with open(os.path.join(d, "frame_%04d.raw" % i), "wb") as fh:
        fh.write(bytes([fill]) * FRAME_BYTES)


def run_tool(capture, port, frames):
    # These cases feed the tool one synthetic capture. Under an inherited
    # PR_ORACLE_REQUIRED=1 the tool also demands a second capture for its
    # determinism proof and returns 1, so the outcome would depend on the
    # caller's environment. Drop the variable: the synthetic cases test the
    # classifier, not the oracle-required policy.
    env = dict(os.environ)
    env.pop("PR_ORACLE_REQUIRED", None)
    return subprocess.run(
        [sys.executable, TOOL, "--capture", capture, "--port", port,
         "--frames", str(frames)],
        capture_output=True, text=True, env=env)


class TitleCompareTest(unittest.TestCase):
    def test_reports_total_non_alignment_without_crashing(self):
        # A capture whose every frame matches no port frame, no splice and no
        # transition row: zero exhibited frames. The oracle must report that and
        # fail, not raise IndexError deriving the window from an empty idx list.
        with tempfile.TemporaryDirectory() as d:
            port = os.path.join(d, "port")
            capture = os.path.join(d, "capture")
            os.mkdir(port)
            os.mkdir(capture)
            write_frame(port, 0, 0x00)
            write_frame(port, 1, 0xFF)
            write_frame(capture, 0, 0x55)

            r = run_tool(capture, port, 2)

            self.assertNotEqual(r.returncode, 0)
            self.assertNotIn("Traceback", r.stderr)
            self.assertNotIn("IndexError", r.stderr)
            self.assertIn("all unexplained", r.stdout)
            self.assertIn("exhibited 0/2", r.stdout)

    def test_partially_exhibited_capture_report_is_unchanged(self):
        # Guard the existing path: a capture that exhibits a port frame keeps the
        # window/counts report. Port frame 0 is all-zero, frame 1 all-ones, and
        # the captured frame is port frame 0 verbatim (a clean sample).
        with tempfile.TemporaryDirectory() as d:
            port = os.path.join(d, "port")
            capture = os.path.join(d, "capture")
            os.mkdir(port)
            os.mkdir(capture)
            write_frame(port, 0, 0x00)
            write_frame(port, 1, 0xFF)
            write_frame(capture, 0, 0x00)

            r = run_tool(capture, port, 2)

            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertNotIn("IndexError", r.stderr)
            self.assertIn("window distinct [0..0]", r.stdout)
            self.assertIn("1 frames in window: 1 clean", r.stdout)
            self.assertIn("exhibited 1/2", r.stdout)


class Splice3Test(unittest.TestCase):
    # Record §47-A: title_compare.splice3, the named three-frame splice after a
    # loader screen. Frames 0..3 are four distinct fills; frame 1 is the loader
    # screen, so the splice is c = f1[:b1] ++ f2[b1:b2] ++ f3[b2:].
    def setUp(self):
        sys.path.insert(0, os.path.join(ROOT, "tools"))
        import title_compare
        self.tc = title_compare
        self.f = [bytes([v]) * FRAME_BYTES for v in (0x10, 0x20, 0x30, 0x40)]
        self.b1, self.b2 = 125 * 960, 180 * 960
        self.c = (self.f[1][:self.b1] + self.f[2][self.b1:self.b2]
                  + self.f[3][self.b2:])

    def test_exact_splice_from_a_loader_screen(self):
        self.assertEqual(self.tc.splice3(self.c, self.f, {1}),
                         (1, self.b1, self.b2))

    def test_first_frame_must_be_a_loader_screen(self):
        self.assertIsNone(self.tc.splice3(self.c, self.f, {0}))
        self.assertIsNone(self.tc.splice3(self.c, self.f, set()))

    def test_middle_frame_must_not_be_a_loader_screen(self):
        self.assertIsNone(self.tc.splice3(self.c, self.f, {1, 2}))

    def test_one_foreign_byte_in_the_middle_fails(self):
        c = bytearray(self.c)
        c[150 * 960] = 0x77
        self.assertIsNone(self.tc.splice3(bytes(c), self.f, {1}))

    def test_two_frame_splice_is_not_a_three_frame_splice(self):
        c = self.f[1][:self.b1] + self.f[3][self.b1:]
        self.assertIsNone(self.tc.splice3(c, self.f, {1}))

    def test_splice_ending_in_the_middle_frame_fails(self):
        # b2 would be FRAME_BYTES: a two-frame splice N ++ N+1.
        c = self.f[1][:self.b1] + self.f[2][self.b1:]
        self.assertIsNone(self.tc.splice3(c, self.f, {1}))

    def test_splice_starting_in_the_middle_frame_fails(self):
        # b1 would be 0: a two-frame splice N+1 ++ N+2.
        c = self.f[2][:self.b2] + self.f[3][self.b2:]
        self.assertIsNone(self.tc.splice3(c, self.f, {1}))

    def test_next_clean_anchor(self):
        # The next capture frame's clean frame must be N + 2.
        self.assertEqual(self.tc.splice3(self.c, self.f, {1}, 3),
                         (1, self.b1, self.b2))
        self.assertIsNone(self.tc.splice3(self.c, self.f, {1}, 2))


if __name__ == "__main__":
    unittest.main()
