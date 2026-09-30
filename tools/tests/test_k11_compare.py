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
            self.assertEqual(run(os.path.join(d, 'nope'), d, env={'PR_ORACLE_REQUIRED': '1'}).returncode, 1)

    def test_report_always_exits_zero(self):
        r = self.case([self.X, self.A, self.Y, self.B], [self.A, self.B],
                      'key 0 settled 0\nend settled 1\n', '--report')
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn('differs in rows', r.stdout)

    def test_empty_window_fails(self):
        r = self.case([self.X], [self.A, self.B], 'key 0 settled 0\nend settled 1\n')
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('window empty', r.stdout)


if __name__ == '__main__':
    unittest.main()
