# tools/tests/test_k11_compare.py
import os, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOL = os.path.join(ROOT, 'tools', 'k11_compare.py')
W, H = 320, 200
ROW = W * 3


def frame(v):
    """Every row distinct (so row hashes separate frames), seeded by v."""
    return b''.join(bytes([(v + r) & 0xFF]) * ROW for r in range(H))


def splice(a, b, row):
    return a[:row * ROW] + b[row * ROW:]


def put(d, frames):
    os.makedirs(d, exist_ok=True)
    for i, f in enumerate(frames):
        with open(os.path.join(d, 'frame_%04d.raw' % i), 'wb') as fh:
            fh.write(f)


def run(capture, port, *extra, env=None):
    e = dict(os.environ)
    e.pop('PR_ORACLE_REQUIRED', None)
    e.update(env or {})
    return subprocess.run([sys.executable, TOOL, '--capture', capture, '--port', port] + list(extra),
                          capture_output=True, text=True, env=e)


class K11Compare(unittest.TestCase):
    A, B, C, X, Y = frame(1), frame(50), frame(90), frame(200), frame(120)

    def case(self, capture, port, screens, *extra, env=None):
        with tempfile.TemporaryDirectory() as d:
            put(os.path.join(d, 'cap'), capture)
            put(os.path.join(d, 'port'), port)
            with open(os.path.join(d, 'port', 'screens.txt'), 'w') as fh:
                fh.write(screens)
            return run(os.path.join(d, 'cap'), os.path.join(d, 'port'), *extra, env=env)

    def test_all_explained_passes(self):
        r = self.case([self.X, self.A, splice(self.A, self.B, 100), self.B], [self.A, self.B],
                      'key 0 settled 0\nend settled 1\n')
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn('0 unexplained in the window', r.stdout)

    def test_unexplained_in_window_fails(self):
        r = self.case([self.X, self.A, self.Y, self.B], [self.A, self.B], 'key 0 settled 0\nend settled 1\n')
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('FIRST UNEXPLAINED capture 2', r.stdout)

    def test_missing_settled_screen_fails(self):
        r = self.case([self.A, self.B], [self.A, self.C, self.B],
                      'key 0 settled 0\nkey 1 settled 1\nend settled 2\n')
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('missing [1]', r.stdout)

    def test_all_black_is_excluded(self):
        r = self.case([self.A, bytes(W * H * 3), self.B], [self.A, self.B], 'key 0 settled 0\nend settled 1\n')
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn('1 all-black', r.stdout)

    def test_absent_capture_skips_unless_required(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertEqual(run(os.path.join(d, 'nope'), d).returncode, 0)
            self.assertEqual(run(os.path.join(d, 'nope'), d, '--required').returncode, 1)

    def test_inherited_oracle_required_is_ignored(self):
        # the Makefile's k11-oracle skips without the capture even when the
        # caller exports PR_ORACLE_REQUIRED=1 (verify does, for other targets):
        # only an explicit --required makes an absent capture fail
        with tempfile.TemporaryDirectory() as d:
            r = run(os.path.join(d, 'nope'), d, env={'PR_ORACLE_REQUIRED': '1'})
            self.assertEqual(r.returncode, 0, r.stdout)
            self.assertIn('skipped', r.stdout)

    def test_report_always_exits_zero(self):
        r = self.case([self.X, self.A, self.Y, self.B], [self.A, self.B],
                      'key 0 settled 0\nend settled 1\n', '--report')
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn('differs in rows', r.stdout)

    def test_repeated_screen_is_credited(self):
        # the final settled screen equals an earlier port frame (the MAIN MENU
        # drawn again): explain() names the first, and both count (record §A.5)
        r = self.case([self.X, self.A, self.B, self.A], [self.A, self.B, self.A],
                      'key 0 settled 0\nkey 1 settled 1\nend settled 2\n')
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn('settled screens exhibited 3/3; missing []', r.stdout)

    def test_allowed_name_admits_only_its_index(self):
        sys.path.insert(0, os.path.join(ROOT, 'tools'))
        import k11_compare as kc
        self.assertEqual(sorted(kc.K11_ALLOWED_UNEXPLAINED['walk']), [138, 151])
        import title_compare as tc
        port = [self.A, self.B]
        rows = [tc.row_hashes(p) for p in port]
        cap = [self.X, self.A, self.Y, self.B]
        import contextlib, io
        saved = kc.K11_ALLOWED_UNEXPLAINED
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                kc.K11_ALLOWED_UNEXPLAINED = {'t': {2: 'test'}}
                self.assertEqual(kc.compare('t', cap, list(range(4)), port, rows, [0], 1, False), 0)
                kc.K11_ALLOWED_UNEXPLAINED = {'t': {3: 'test'}}
                self.assertEqual(kc.compare('t', cap, list(range(4)), port, rows, [0], 1, False), 1)
        finally:
            kc.K11_ALLOWED_UNEXPLAINED = saved

    def test_capture_past_the_final_screen_fails(self):
        # the port stops at B while the capture goes on (C, B, then A): the
        # window END must be the capture's last non-black frame (review 1)
        r = self.case([self.X, self.A, self.B, self.C, self.B, self.A], [self.A, self.B, self.C],
                      'key 0 settled 0\nend settled 1\n')
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn("capture continues past the port's final screen at 5", r.stdout)

    def test_trailing_black_capture_frames_pass(self):
        r = self.case([self.X, self.A, self.B, bytes(W * H * 3)], [self.A, self.B],
                      'key 0 settled 0\nend settled 1\n')
        self.assertEqual(r.returncode, 0, r.stdout)

    def test_empty_window_fails(self):
        r = self.case([self.X], [self.A, self.B], 'key 0 settled 0\nend settled 1\n')
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('window empty', r.stdout)


if __name__ == '__main__':
    unittest.main()
