# tools/tests/test_gp_side1.py
import io, os, shutil, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_side1 as gs1

DAC = bytes(i for i in range(256) for _ in range(3))   # grey: index i -> (i, i, i)


def ipx(v):
    """A solid frame whose every index is v (renders to solid grey v)."""
    return bytes([v]) * 64000 + DAC


def write_dump(d, modes):
    """Write frames.txt and frame_%05d.ipx for `modes` (one mode per frame)."""
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, 'frames.txt'), 'w') as f:
        for i, mode in enumerate(modes):
            f.write('%05d f=%04X tick=%08X mode=%04X\n' % (i, i, i, mode))
    for i, mode in enumerate(modes):
        with open(os.path.join(d, 'frame_%05d.ipx' % i), 'wb') as f:
            f.write(ipx(i))


def out(sink):
    return lambda s: sink.write(s + '\n')


class Compare(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp(prefix='gpside1-test-')
        self.addCleanup(shutil.rmtree, self.d, True)
        self.a = os.path.join(self.d, 'a')
        self.b = os.path.join(self.d, 'b')

    def test_identical_mode_1f_frames_pass(self):
        # modes 0x10 0x1F 0x1F 0x20 -> two mode-0x1F frames (indices 1, 2)
        write_dump(self.a, [0x10, 0x1F, 0x1F, 0x20])
        write_dump(self.b, [0x10, 0x1F, 0x1F, 0x20])
        sink = io.StringIO()
        rc, count = gs1.compare(self.a, self.b, 2, out(sink))
        self.assertEqual((rc, count), (0, 2), sink.getvalue())
        self.assertIn('2 mode-0x1F frames compared', sink.getvalue())

    def test_a_mutated_byte_fails(self):
        write_dump(self.a, [0x10, 0x1F, 0x1F, 0x20])
        write_dump(self.b, [0x10, 0x1F, 0x1F, 0x20])
        # mutate one index byte of b's first mode-0x1F frame (port 1) -> RGB changes
        p = os.path.join(self.b, 'frame_00001.ipx')
        with open(p, 'rb') as f:
            data = bytearray(f.read())
        data[0] = 7
        with open(p, 'wb') as f:
            f.write(data)
        sink = io.StringIO()
        rc, count = gs1.compare(self.a, self.b, 2, out(sink))
        self.assertEqual(rc, 1, sink.getvalue())
        self.assertIn('differs in rendered RGB', sink.getvalue())

    def test_a_wrong_count_fails(self):
        write_dump(self.a, [0x10, 0x1F, 0x1F, 0x20])
        write_dump(self.b, [0x10, 0x1F, 0x1F, 0x20])
        sink = io.StringIO()
        rc, _ = gs1.compare(self.a, self.b, 3, out(sink))
        self.assertEqual(rc, 1, sink.getvalue())
        self.assertIn('!= pinned', sink.getvalue())

    def test_an_absent_replay_skips(self):
        write_dump(self.a, [0x10, 0x1F, 0x1F, 0x20])
        sink = io.StringIO()
        rc, count = gs1.compare(self.a, os.path.join(self.d, 'missing'), 2, out(sink))
        self.assertEqual((rc, count), (0, None), sink.getvalue())
        self.assertIn('skipped', sink.getvalue())


if __name__ == '__main__':
    unittest.main()
