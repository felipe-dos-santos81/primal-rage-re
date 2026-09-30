# tools/tests/test_gp_compare.py
import gzip, io, os, shutil, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_compare as gc
import gp_session as gs
import title_compare as tc

DAC = bytes(i for i in range(256) for _ in range(3))   # grey: index i -> (i, i, i)


def ipx(k):
    """A port frame whose every index is k, except row k (index k+1): distinct rows."""
    idx = bytearray([k]) * 64000
    idx[320 * k:320 * (k + 1)] = bytes([k + 1]) * 320
    return bytes(idx) + DAC


def rgb(k):
    return gc.expand_ipx(ipx(k))


class Dirs(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp(prefix='gpcmp-test-')
        self.addCleanup(shutil.rmtree, self.d, True)
        self.cap = os.path.join(self.d, 'cap')
        self.port = os.path.join(self.d, 'port')
        os.makedirs(self.cap)
        os.makedirs(self.port)

    def write(self, port_ks, cap_frames):
        for i, k in enumerate(port_ks):
            with open(os.path.join(self.port, 'frame_%05d.ipx' % i), 'wb') as f:
                f.write(ipx(k))
        for j, c in enumerate(cap_frames):
            with gzip.open(os.path.join(self.cap, 'frame_%05d.raw.gz' % j), 'wb') as f:
                f.write(c)
        with open(os.path.join(self.cap, 'window.txt'), 'w') as f:
            f.write(''.join('%05d %d\n' % (j, 100 + j) for j in range(len(cap_frames))))

    def run_claim(self, n, report=False):
        cap = gc.Lazy(gc._paths(self.cap, 'frame_%05d.raw.gz'), gc.load_capture_frame)
        port = gc.Lazy(gc._paths(self.port, 'frame_%05d.ipx'), gc.load_port_frame)
        rows = [tc.row_hashes(port[m]) for m in range(len(port))]
        out = []
        rc, first, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, n, report, out.append)
        return rc, first, out


class TestExpand(unittest.TestCase):
    def test_expand_is_dac_of_index(self):
        data = bytearray(ipx(3))
        data[64000 + 3 * 7:64000 + 3 * 7 + 3] = b'\x01\x02\x03'
        data[0] = 7
        out = gc.expand_ipx(bytes(data))
        self.assertEqual(out[0:3], b'\x01\x02\x03')
        self.assertEqual(out[3:6], bytes([3, 3, 3]))
        self.assertEqual(len(out), tc.FRAME_BYTES)

    def test_expand_rejects_a_short_frame(self):
        with self.assertRaises(ValueError):
            gc.expand_ipx(b'\x00' * 100)


if __name__ == '__main__':
    unittest.main()
