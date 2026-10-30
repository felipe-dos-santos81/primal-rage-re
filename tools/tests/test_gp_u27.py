# tools/tests/test_gp_u27.py (live truth Task 7: the name-entry path)
import io
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
import gp_u27 as gu


CAP_FIELDS = gs.SNAP_FIELDS + gs.WIN_EXTRA + gs.NAME_ENTRY_EXTRA
PORT_FIELDS = gs.SNAP_FIELDS + gs.WIN_EXTRA
SCORE = 0x005217A0

# The measured records. The name dwords are little-endian memory bytes: A, BA,
# DBA and the finaliser's space-padded "ABD ".
ROWS = (
    (0x217E, dict(b25=0x00)),
    (0x217F, dict(b25=0x04)),
    (0x2183, dict(b25=0x05)),
    (0x2193, dict(ncnt=1)),
    (0x21B0, dict(ncnt=2)),
    (0x21CE, dict(ncnt=3)),
    (0x220D, dict(name=0x00000041)),
    (0x2226, dict(name=0x00004241)),
    (0x2240, dict(name=0x00444241)),
    (0x2243, dict(name=0x20444241)),
    (0x22CE, dict(b25=0x06)),
    (0x23FC, dict(b25=0x07)),
    (0x23FD, dict(b25=0x0A)),
    (0x24B9, dict(mode=3, b25=0, name=0x20444241)),
)


def _rec(kind, f, kw):
    fields = CAP_FIELDS if kind == 'S' else PORT_FIELDS
    vals = {n: 0 for n, _, _ in fields}
    vals.update(f=f, mode=0x1E, sc0=SCORE)
    vals.update(kw)
    s = gs.format_s(0, vals, 0, 0x1E, 0x1E, fields=fields)
    return s if kind == 'S' else 'T ' + s[len('S ms=0 '):].rsplit(' kb=', 1)[0]


def _log(rows=ROWS, kind='S'):
    return [_rec(kind, f, kw) for f, kw in rows]


def _replace(f, **kw):
    return [(at, dict(old, **kw) if at == f else old) for at, old in ROWS]


class TestEvidence(unittest.TestCase):
    def run_check(self, lines):
        out = io.StringIO()
        rc = gu.evidence('gp-u27-name-entry', lines, lambda s: out.write(s + '\n'))
        return rc, out.getvalue()

    def test_the_capture_covers_states_letters_score_and_finaliser(self):
        rc, text = self.run_check(_log())
        self.assertEqual(rc, 0, text)
        self.assertIn('6/6 sub-states ok', text)
        self.assertIn('3/3 letter counts ok', text)
        self.assertIn('4/4 name-byte stages ok', text)
        self.assertIn('finaliser "ABD"', text)
        self.assertIn('mode 3', text)

    def test_a_wrong_substate_fails(self):
        rc, text = self.run_check(_log(_replace(0x22CE, b25=5)))
        self.assertEqual(rc, 1)
        self.assertIn('f=22CE', text)
        self.assertIn('b25', text)

    def test_a_letter_count_or_name_mutation_fails(self):
        for f, kw in ((0x21B0, dict(ncnt=3)), (0x2226, dict(name=0x00004341))):
            with self.subTest(f=f):
                rc, text = self.run_check(_log(_replace(f, **kw)))
                self.assertEqual(rc, 1)
                self.assertIn('f=%X' % f, text)

    def test_the_finaliser_requires_padded_abd(self):
        rc, text = self.run_check(_log(_replace(0x2243, name=0x20444341)))
        self.assertEqual(rc, 1)
        self.assertIn('finaliser', text)


class TestPath(unittest.TestCase):
    def run_path(self, cap, port):
        out = io.StringIO()
        rc = gu.path('gp-u27-name-entry', cap, port, lambda s: out.write(s + '\n'))
        return rc, out.getvalue()

    def test_the_port_reproduces_b25_at_the_capture_frames(self):
        rc, text = self.run_path(_log(), _log(kind='T'))
        self.assertEqual(rc, 0, text)
        self.assertIn('6/6 sub-states reproduced', text)

    def test_the_port_path_rejects_a_wrong_b25(self):
        rc, text = self.run_path(_log(), _log(_replace(0x23FC, b25=6), kind='T'))
        self.assertEqual(rc, 1)
        self.assertIn('f=23FC', text)

    def test_a_record_without_b25_prints_a_dash_not_minus_one(self):
        cap = [l.replace(' b25=00', '', 1) if ' f=217E ' in l else l for l in _log()]
        rc, text = self.run_path(cap, _log(kind='T'))
        self.assertEqual(rc, 1)
        self.assertIn('f=217E b25=00 capture -- port 00: FAIL', text)
        self.assertNotIn('-1', text)


if __name__ == '__main__':
    unittest.main()
