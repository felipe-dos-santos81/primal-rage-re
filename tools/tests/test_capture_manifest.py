# tools/tests/test_capture_manifest.py
import json
import os
import shutil
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import capture_manifest as cm


def _write(path, data):
    if isinstance(data, str):
        data = data.encode('utf-8')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(data)


def _poll_log(first, last):
    """A synthetic gp poll.log with S records at f=first..last."""
    lines = ['B ms=0 base=00266000 ptr=0000FE20']
    for f in range(first, last + 1):
        lines.append('S ms=0 f=%04X mode=0003 st=0000 tick=00000000' % f)
    return '\n'.join(lines) + '\n'


def make_k11(root, rel, first=4, last=10, dro=('prage_000.dro',), n_frames=3):
    d = os.path.join(root, rel)
    os.makedirs(d, exist_ok=True)
    _write(os.path.join(d, 'poll.log'), _poll_log(first, last))
    for n in dro:
        _write(os.path.join(d, n), b'dro-bytes')
    for i in range(n_frames):
        _write(os.path.join(d, 'frame_%05d.raw.gz' % i), b'frame' + bytes([i]))
    return d


def make_frame_dump(root, rel, n_frames=3):
    d = os.path.join(root, rel)
    os.makedirs(d, exist_ok=True)
    for i in range(n_frames):
        _write(os.path.join(d, 'frame_%04d.raw' % i), b'raw' + bytes([i]))
    return d


class TestManifest(unittest.TestCase):
    def _manifest(self, root, capture_dirs):
        return cm.generate_manifest(capture_dirs, root)

    def test_hash_and_count_check_passes(self):
        with tempfile.TemporaryDirectory() as root:
            rel = 'data/k11-captures/gp-x'
            make_k11(root, rel)
            manifest = self._manifest(root, [rel])
            ok, problems, present, skipped = cm.check_manifest(manifest, root, [rel])
            self.assertTrue(ok, problems)
            self.assertEqual((present, skipped), (1, 0))

    def test_a_poll_log_byte_change_fails(self):
        with tempfile.TemporaryDirectory() as root:
            rel = 'data/k11-captures/gp-x'
            make_k11(root, rel)
            manifest = self._manifest(root, [rel])
            poll = os.path.join(root, rel, 'poll.log')
            with open(poll, 'rb') as f:
                data = bytearray(f.read())
            data[0] ^= 0xFF          # one byte: the B record's first byte
            with open(poll, 'wb') as f:
                f.write(data)
            ok, problems, _, _ = cm.check_manifest(manifest, root, [rel])
            self.assertFalse(ok)
            self.assertTrue(any('poll.log sha256 mismatch' in p for p in problems))

    def test_a_declared_absence_skips(self):
        with tempfile.TemporaryDirectory() as root:
            rel = 'data/k11-captures/gp-u14-win-live'
            entry = cm.compute_identity(rel, root)       # absent -> declared
            self.assertTrue(entry['declared'])
            manifest = {'version': 1, 'captures': [entry]}
            ok, problems, present, skipped = cm.check_manifest(manifest, root, [rel])
            self.assertTrue(ok, problems)
            self.assertEqual((present, skipped), (0, 1))

    def test_a_declared_gap_that_is_present_is_stale(self):
        with tempfile.TemporaryDirectory() as root:
            rel = 'data/k11-captures/gp-x'
            make_k11(root, rel)
            manifest = {'version': 1, 'captures': [
                {'path': rel, 'kind': 'k11', 'declared': True, 'reason': 'test'}]}
            ok, problems, _, _ = cm.check_manifest(manifest, root, [rel])
            self.assertFalse(ok)
            self.assertTrue(any('stale' in p for p in problems))

    def test_a_missing_dro_fails(self):
        with tempfile.TemporaryDirectory() as root:
            rel = 'data/k11-captures/gp-x'
            make_k11(root, rel)
            manifest = self._manifest(root, [rel])
            os.remove(os.path.join(root, rel, 'prage_000.dro'))
            ok, problems, _, _ = cm.check_manifest(manifest, root, [rel])
            self.assertFalse(ok)
            self.assertTrue(any('.dro inventory' in p for p in problems))

    def test_the_frame_dump_family(self):
        with tempfile.TemporaryDirectory() as root:
            rel = 'data/title-captures/title'
            make_frame_dump(root, rel)
            manifest = self._manifest(root, [rel])
            entry = manifest['captures'][0]
            self.assertEqual(entry['kind'], 'frame-dump')
            self.assertNotIn('poll_log_sha256', entry)
            ok, problems, present, _ = cm.check_manifest(manifest, root, [rel])
            self.assertTrue(ok, problems)
            self.assertEqual(present, 1)
            # one frame byte changed -> content hash mismatch
            with open(os.path.join(root, rel, 'frame_0001.raw'), 'rb') as f:
                data = bytearray(f.read())
            data[-1] ^= 0xFF
            with open(os.path.join(root, rel, 'frame_0001.raw'), 'wb') as f:
                f.write(data)
            ok, problems, _, _ = cm.check_manifest(manifest, root, [rel])
            self.assertFalse(ok)
            self.assertTrue(any('content hash mismatch' in p for p in problems))


class TestPinBaseline(unittest.TestCase):
    def _pin_file(self, d, pin='ATTRACT_MAX_MISSING = 74'):
        p = os.path.join(d, 'pins.txt')
        _write(p, '# >>> PIN-BASELINE-BEGIN attract\n%s\n# <<< PIN-BASELINE-END attract\n' % pin)
        return p

    def test_a_pin_digit_change_fails(self):
        with tempfile.TemporaryDirectory() as d:
            p = self._pin_file(d)
            baseline = os.path.join(d, 'baseline.sha256')   # a temp path, never the real one
            cm.write_baseline(baseline, cm.compute_pin_digest([p]), 'test')
            self.assertTrue(cm.check_pins(baseline, [p])[0])
            p2 = self._pin_file(d, 'ATTRACT_MAX_MISSING = 75')   # one pin digit
            ok, problems, _ = cm.check_pins(baseline, [p2])
            self.assertFalse(ok)
            self.assertTrue(any('mismatch' in x for x in problems))

    def test_a_missing_baseline_fails(self):
        with tempfile.TemporaryDirectory() as d:
            p = self._pin_file(d)
            ok, problems, _ = cm.check_pins(os.path.join(d, 'absent.sha256'), [p])
            self.assertFalse(ok)
            self.assertTrue(any('missing' in x for x in problems))

    def test_trace_prefix_pins_are_inside_the_pin_baseline(self):
        regions = cm.pin_regions([os.path.join(ROOT, 'tools', 'audio_compare.py')])
        by_name = {name: body for _path, name, body in regions}
        self.assertIn('trace-prefix-pins', by_name)
        self.assertIn('TRACE_PREFIX_PINS = {', by_name['trace-prefix-pins'])

    def test_unbalanced_markers_are_errors(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 'pins.txt')
            _write(p, '# >>> PIN-BASELINE-BEGIN x\nATTRACT_MAX_MISSING = 74\n')   # no END
            with self.assertRaises(SystemExit):
                cm.pin_regions([p])


class TestCaptureDirs(unittest.TestCase):
    def test_the_makefile_parser(self):
        with tempfile.TemporaryDirectory() as d:
            mk = os.path.join(d, 'Makefile')
            _write(mk, 'CAPTURE_DIRS = \\\n\t$(K11_CAPTURES)/gp-idle-loss \\\n\t$(SMK_CAPTURES)/twi5 \\\n\t$(TITLE_CAPTURES)/frontend\n')
            self.assertEqual(cm.read_capture_dirs(mk),
                             ['data/k11-captures/gp-idle-loss', 'data/smk-captures/twi5',
                              'data/title-captures/frontend'])


class TestPreflightVerify(unittest.TestCase):
    def _game(self, d, pr=True, index=True, cmos=None):
        g = os.path.join(d, 'C')
        os.makedirs(g, exist_ok=True)
        if pr:
            _write(os.path.join(g, 'PRAGE.EXE'), b'exe')
        if index:
            _write(os.path.join(g, 'INDEX'), b'index')
        if cmos is not None:
            _write(os.path.join(g, 'CMOS'), cmos)
        return g

    def test_missing_prage_fails(self):
        with tempfile.TemporaryDirectory() as d:
            g = self._game(d, pr=False)
            gd = os.path.join(d, 'ghidra_data.bin')
            _write(gd, b'data')
            ok, problems = cm.preflight_verify(game_dir=g, ghidra_data=gd)
            self.assertFalse(ok)
            self.assertTrue(any('PRAGE.EXE missing' in p for p in problems))

    def test_missing_ghidra_fails(self):
        with tempfile.TemporaryDirectory() as d:
            g = self._game(d)
            ok, problems = cm.preflight_verify(game_dir=g, ghidra_data=os.path.join(d, 'absent'))
            self.assertFalse(ok)
            self.assertTrue(any('ghidra_data.bin missing' in p for p in problems))

    def test_a_nonzero_cmos_fails(self):
        with tempfile.TemporaryDirectory() as d:
            g = self._game(d, cmos=b'\x01' * cm.CMOS_BYTES)
            gd = os.path.join(d, 'ghidra_data.bin')
            _write(gd, b'data')
            ok, problems = cm.preflight_verify(game_dir=g, ghidra_data=gd)
            self.assertFalse(ok)
            self.assertTrue(any('CMOS is' in p for p in problems))

    def test_clean_preconditions_pass(self):
        with tempfile.TemporaryDirectory() as d:
            g = self._game(d, cmos=b'\x00' * cm.CMOS_BYTES)
            gd = os.path.join(d, 'ghidra_data.bin')
            _write(gd, b'data')
            ok, problems = cm.preflight_verify(game_dir=g, ghidra_data=gd)
            self.assertTrue(ok, problems)


class TestPreflightCapture(unittest.TestCase):
    def test_missing_tools_and_unwritable_root_fail(self):
        with tempfile.TemporaryDirectory() as d:
            ok, problems = cm.preflight_capture(d, which=lambda t: None, load_avg=0.0, ncores=4)
            self.assertFalse(ok)
            self.assertTrue(any('required tool' in p for p in problems))

    def test_load_above_cores_fails(self):
        with tempfile.TemporaryDirectory() as d:
            ok, problems = cm.preflight_capture(d, which=lambda t: '/bin/' + t,
                                                load_avg=9.0, ncores=4, avail=1 << 30)
            self.assertFalse(ok)
            self.assertTrue(any('load average' in p for p in problems))

    def test_guest_memsize_above_free_memory_fails(self):
        with tempfile.TemporaryDirectory() as d:
            ok, problems = cm.preflight_capture(d, memsize=64, which=lambda t: '/bin/' + t,
                                                load_avg=0.0, ncores=4, avail=0)
            self.assertFalse(ok)
            self.assertTrue(any('memsize' in p for p in problems))

    def test_clean_capture_preflight_passes(self):
        with tempfile.TemporaryDirectory() as d:
            ok, problems = cm.preflight_capture(d, which=lambda t: '/bin/' + t,
                                                load_avg=1.0, ncores=4, avail=1 << 30)
            self.assertTrue(ok, problems)


if __name__ == '__main__':
    unittest.main()
