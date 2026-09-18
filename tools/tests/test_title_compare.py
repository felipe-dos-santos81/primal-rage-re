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
    return subprocess.run(
        [sys.executable, TOOL, "--capture", capture, "--port", port,
         "--frames", str(frames)],
        capture_output=True, text=True)


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


if __name__ == "__main__":
    unittest.main()
