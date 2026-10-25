# tools/tests/test_title_compare.py
import contextlib, hashlib, io, os, subprocess, sys, tempfile, unittest

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


def run_frontend(capture, port, frames, pin, *extra):
    # The enforced front-end gate with its window-end pin (sub-project A A1b).
    env = dict(os.environ)
    env.pop("PR_ORACLE_REQUIRED", None)
    return subprocess.run(
        [sys.executable, TOOL, "--frontend", "--frontend-end", str(pin),
         "--capture", capture, "--port", port, "--frames", str(frames)] + list(extra),
        capture_output=True, text=True, env=env)


class FrontendEndPinTest(unittest.TestCase):
    # Sub-project A A1b: the front-end window end (the last capture frame a port
    # frame is exhibited by) is pinned. A port that renders fewer frames stops
    # the window earlier, but the frames it drops fall OUTSIDE the shrunken
    # window, so the unexplained set stays empty and only the end pin catches it.
    def _fixtures(self, d):
        port = os.path.join(d, "port")
        capture = os.path.join(d, "capture")
        os.mkdir(port)
        os.mkdir(capture)
        write_frame(port, 0, 0x11)
        write_frame(port, 1, 0x22)
        write_frame(capture, 0, 0x11)
        write_frame(capture, 1, 0x22)
        return port, capture

    def test_full_dump_meets_the_pin(self):
        with tempfile.TemporaryDirectory() as d:
            port, capture = self._fixtures(d)
            r = run_frontend(capture, port, 2, 1)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn("window distinct [0..1]", r.stdout)
            self.assertIn("0 unexplained", r.stdout)

    def test_one_frame_shorter_dump_fails_the_pin(self):
        with tempfile.TemporaryDirectory() as d:
            port, capture = self._fixtures(d)
            # One port frame: capture frame 1 is no longer exhibited, so it falls
            # outside the window [0..0], leaves the unexplained set empty, and
            # the end pin is the only thing that can fail.
            r = run_frontend(capture, port, 1, 1)
            self.assertEqual(r.returncode, 1, r.stderr)
            self.assertIn("window ends at capture 0 < pinned end 1", r.stdout)
            self.assertIn("0 unexplained", r.stdout)

    def test_coverage_pin_requires_the_measured_window_start(self):
        with tempfile.TemporaryDirectory() as d:
            port, capture = self._fixtures(d)
            write_frame(port, 1, 0x55)  # an unexhibited class between the two matches
            write_frame(port, 2, 0x22)
            args = ('--frontend-start', '0', '--max-missing', '1')
            r = run_frontend(capture, port, 3, 1, *args)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn('coverage: 1 missing class(es), pin 1', r.stdout)
            r = run_frontend(capture, port, 3, 1, '--frontend-start', '0',
                             '--max-missing', '0')
            self.assertEqual(r.returncode, 1, r.stdout)
            self.assertIn('FAIL: 1 missing class(es) > pinned 0', r.stdout)
            r = run_frontend(capture, port, 3, 1, '--frontend-start', '0',
                             '--max-missing', '2')
            self.assertEqual(r.returncode, 0, r.stdout)
            self.assertIn('improved: lower the pin', r.stdout)
            r = run_frontend(capture, port, 3, 1, '--max-missing', '1')
            self.assertEqual(r.returncode, 1, r.stdout)
            self.assertIn('window-start pin is not set', r.stdout)
            r = run_frontend(capture, port, 3, 1, '--frontend-start', '1',
                             '--max-missing', '1')
            self.assertEqual(r.returncode, 1, r.stdout)
            self.assertIn('window starts at capture 0 != pinned start 1', r.stdout)

    def test_demo_fight_also_rejects_a_stale_frontend_allowance(self):
        # --demo-fight uses the same two front-end names when it decides whether
        # its window is empty. A clean named frame must not bypass the audit.
        sys.path.insert(0, os.path.join(ROOT, 'tools'))
        import title_compare as tc
        with tempfile.TemporaryDirectory() as d:
            port, capture = self._fixtures(d)
            write_frame(capture, 2, 0)
            saved = (tc.FRONTEND_ALLOWED_UNEXPLAINED,
                     tc.FRONTEND_ALLOWED_UNEXPLAINED_SHA256, sys.argv)
            out = io.StringIO()
            try:
                tc.FRONTEND_ALLOWED_UNEXPLAINED = (1,)
                tc.FRONTEND_ALLOWED_UNEXPLAINED_SHA256 = {
                    1: hashlib.sha256(tc.load(os.path.join(capture, 'frame_0001.raw'))).hexdigest()}
                sys.argv = ['title_compare.py', '--demo-fight', '--demo-fight-min-first', '3',
                            '--capture', capture, '--port', port, '--frames', '2']
                with contextlib.redirect_stdout(out):
                    rc = tc.main()
            finally:
                tc.FRONTEND_ALLOWED_UNEXPLAINED, tc.FRONTEND_ALLOWED_UNEXPLAINED_SHA256, sys.argv = saved
            self.assertEqual(rc, 1, out.getvalue())
            self.assertIn('allowed frame 1 is stale: clean now', out.getvalue())


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


class AllowanceHonestyTest(unittest.TestCase):
    def setUp(self):
        sys.path.insert(0, os.path.join(ROOT, "tools"))
        import title_compare
        self.tc = title_compare
        self.frames = [bytes([v]) * FRAME_BYTES for v in (0x11, 0x55, 0x22)]

    def audit(self, kinds, shas):
        out = []
        used, bad = self.tc.unexplained_allowances(
            'synthetic', self.frames, kinds, (0, 2), (1,), shas, out.append)
        return used, bad, out

    def test_unexplained_allowance_checks_bytes_and_stale_names(self):
        kinds = [('clean', 0), ('unexplained', None), ('clean', 2)]
        good = {1: hashlib.sha256(self.frames[1]).hexdigest()}
        used, bad, out = self.audit(kinds, good)
        self.assertEqual((used, bad), ([1], 0), out)
        used, bad, out = self.audit(kinds, {1: '0' * 64})
        self.assertEqual((used, bad), ([], 1), out)
        self.assertTrue(any('sha256' in line for line in out), out)
        kinds[1] = ('clean', 1)
        used, bad, out = self.audit(kinds, good)
        self.assertEqual((used, bad), ([], 1), out)
        self.assertTrue(any('stale' in line and 'retire the name' in line
                            for line in out), out)

    def test_empty_attract2_set_has_a_live_stale_name_check(self):
        # ATTRACT2_ALLOWED_UNEXPLAINED is empty. A synthetic entry must fail
        # when its frame is explained; an empty set alone would prove nothing.
        kinds = [('clean', 0), ('clean', 1), ('clean', 2)]
        out = []
        saved = self.tc.ATTRACT2_ALLOWED_UNEXPLAINED
        self.assertEqual(saved, {})
        try:
            self.tc.ATTRACT2_ALLOWED_UNEXPLAINED = {1: 0}
            used, bad = self.tc.unexplained_allowances(
                'attract2', None, kinds, (0, 2),
                self.tc.ATTRACT2_ALLOWED_UNEXPLAINED, None, out.append)
        finally:
            self.tc.ATTRACT2_ALLOWED_UNEXPLAINED = saved
        self.assertEqual((used, bad), ([], 1), out)
        self.assertTrue(any('stale' in line for line in out), out)

    def test_splice3_stale_name_fails(self):
        out = []
        bad = self.tc.splice3_stale('attract2', 1, 'clean', 0, 2, out.append)
        self.assertEqual(bad, 1, out)
        self.assertTrue(any('FAIL:' in line and 'retire the name' in line
                            for line in out), out)
        self.assertEqual(self.tc.splice3_stale('attract2', 1, 'unexplained', 0, 2,
                                               out.append), 0)

    def test_attract2_all_black_named_frame_is_stale(self):
        # Mutating the named capture frame to all-black must reach the stale
        # allowance failure, not pass as an ordinary black capture artifact.
        with tempfile.TemporaryDirectory() as d:
            capture = os.path.join(d, 'capture')
            port = os.path.join(d, 'port')
            cycle2 = os.path.join(port, 'cycle2')
            for path in (capture, cycle2):
                os.makedirs(path)
            write_frame(port, 0, 0x11)
            write_frame(port, 1, 0x22)
            write_frame(capture, 0, 0x11)
            write_frame(capture, 1, 0x55)
            write_frame(capture, 1, 0)  # the black-frame mutation
            write_frame(capture, 2, 0x33)
            write_frame(cycle2, 0, 0x33)
            write_frame(cycle2, 1, 0x44)
            saved = self.tc.ATTRACT2_SPLICE3_ALLOWED, sys.argv
            out = io.StringIO()
            try:
                self.tc.ATTRACT2_SPLICE3_ALLOWED = (1,)
                sys.argv = ['title_compare.py', '--attract2', '--attract2-min-first', '2',
                            '--capture', capture, '--port', port]
                with contextlib.redirect_stdout(out):
                    rc = self.tc.main()
            finally:
                self.tc.ATTRACT2_SPLICE3_ALLOWED, sys.argv = saved
            self.assertEqual(rc, 1, out.getvalue())
            self.assertIn('ATTRACT2_SPLICE3_ALLOWED but is artifact now; retire the name',
                          out.getvalue())

    def test_attract2_coverage_credits_splice3_and_excludes_beyond_reach(self):
        port = [bytes([v]) * FRAME_BYTES for v in (0x10, 0x20, 0x30, 0x40, 0x50)]
        missing, reach = self.tc.attract2_coverage(port, {0, 1, 3}, [])
        self.assertEqual((missing, reach), ([2], 3))  # port 4 is beyond reach
        missing, reach = self.tc.attract2_coverage(port, {0, 1, 3}, [(1, 10, 20)])
        self.assertEqual((missing, reach), ([], 3))


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
