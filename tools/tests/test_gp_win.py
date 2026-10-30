# tools/tests/test_gp_win.py (gameplay U9/U10: the win path and the ending under pokes)
import io, os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
import gp_win as gw

FIELDS = gs.SNAP_FIELDS + gs.WIN_EXTRA
# c82b (slot 1's world-domination count, 0x1078C6) is SNAP_FIELDS' side-1 twin of
# WIN_EXTRA's c82, so the side-1 log shares FIELDS.
FIELDS_S1 = FIELDS


def _rec(kind, f, mode, kw):
    return _rec_fields(kind, f, mode, kw, FIELDS)


def _rec_fields(kind, f, mode, kw, fields):
    vals = {n: 0 for n, _, _ in fields}
    vals.update(f=f, mode=mode, t508=1, t50c=2, **kw)
    s = gs.format_s(0, vals, 0, 0x1E, 0x1E, fields)
    return s if kind == 'S' else 'T ' + s[len('S ms=0 '):].rsplit(' kb=', 1)[0]


def _marks(**at):
    """m106/m10a from {land index: byte}."""
    b = bytearray(8)
    for i, v in at.items():
        b[int(i[1:])] = v
    return dict(m106=int.from_bytes(b[:4], 'little'), m10a=int.from_bytes(b[4:], 'little'))


# The U9 path (frames from the port preview of record §W.7): (first f, last f, mode, fields).
U9 = [(0x100, 0x140, 0x03, {}), (0x141, 0x292, 0x27, {}), (0x293, 0x2F5, 0x10, {}),
      (0x2F6, 0x485, 0x17, {}),
      (0x486, 0x48F, 0x06, dict(b1e=1, afc=5)),
      (0x490, 0x853, 0x08, dict(b1e=1, afc=5, s1_5a=0x78, w2=1, ad4=gw.NO_RESULT)),
      (0x854, 0x85D, 0x06, dict(b1e=2, afc=5, w2=1, ad4=gw.NO_RESULT)),
      (0x85E, 0xAB5, 0x09, dict(b1e=2, afc=5, s1_5a=0x78, w2=2, ad4=0)),
      (0xAB6, 0xBA6, 0x17, dict(afc=5, w2=2, **_marks(l5=0x80))),
      (0xBA7, 0xC13, 0x12, dict(afc=5, w2=2, **_marks(l5=0x80))),
      (0xC14, 0xD77, 0x12, dict(afc=5, w2=2, t104=1, **_marks(l5=0x80))),
      (0xD78, 0xDB4, 0x06, dict(b1e=1, afc=3, t104=1, ad4=gw.NO_RESULT, **_marks(l5=0x80)))]


def _log(path, kind='S', drop=lambda f: False, change=None):
    out = []
    for f0, f1, mode, kw in path:
        for f in range(f0, f1 + 1):
            if drop(f):
                continue
            k = dict(kw)
            if change:
                k = change(f, mode, k)
            out.append(_rec(kind, f, mode, k))
    return out


ALL7 = _marks(l0=0x80, l1=0x80, l2=0x80, l3=0x80, l4=0x80, l5=0x80, l6=0x80)


def _u10(stale_k=None, order_swap=False, held=7, c82=1, ending_c0=0):
    """The U10 path as (n frames, mode, fields) segments, numbered on from f = 0x100: the
    U9 path to mode 9 with the seven lands poked, the tour's end, the final (seven
    opponents), mode 0xF, the ending and the high-score entry. Returns [(f0, f1, mode, kw)]
    and the first f of every mode-0xD entry."""
    segs = [(0x41, 0x03, {}), (0x152, 0x27, {}), (0x63, 0x10, {}),
            (0x20, 0x06, dict(b1e=1, afc=5)),
            (0x20, 0x08, dict(b1e=1, afc=5, s1_5a=0x78, w2=1, ad4=gw.NO_RESULT, **ALL7)),
            (0x20, 0x06, dict(b1e=2, afc=5, w2=1, ad4=gw.NO_RESULT, **ALL7)),
            (0x20, 0x09, dict(b1e=2, afc=5, s1_5a=0x78, w2=2, ad4=0, **ALL7)),
            (0x20, 0x17, dict(afc=5, w2=2, **ALL7)),
            (0x10, 0x12, dict(afc=5, w2=2, **ALL7))]
    if order_swap:      # the marks cleared and the world-domination count before the seventh land
        segs += [(0x10, 0x12, dict(afc=5, w2=2, c82=1, t104=0)), (0x10, 0x12, dict(afc=5, w2=2, c82=1, t104=7))]
    else:
        segs += [(0x10, 0x12, dict(afc=5, w2=2, t104=held, **ALL7)),
                 (0x10, 0x12, dict(afc=5, w2=2, t104=held, c82=c82))]
    segs += [(0x10, 0x23, dict(afc=5)), (0x10, 0x22, dict(afc=5)), (0x10, 0x24, dict(afc=7)),
             (0x10, 0x0C, dict(afc=7, b14=1, b21=0))]
    for k in range(1, 8):
        segs.append((0x10, 0x0D, dict(afc=7, b14=1, s1_5a=0x78, b21=stale_k if k == stale_k else k - 1)))
        if k < 7:
            segs.append((0x10, 0x0C, dict(afc=7, b14=1, b21=k)))
    segs += [(0x10, 0x0F, dict(afc=7, b21=7)), (0x10, 0x15, {}),
             (0x10, 0x1F, dict(c0=ending_c0)), (0x10, 0x15, {}), (0x10, 0x1F, dict(c0=ending_c0)),
             (0x10, 0x15, {}), (0x10, 0x1E, {}), (0x10, 0x03, {})]
    out, f, d = [], 0x100, []
    for n, mode, kw in segs:
        if mode == 0x0D:
            d.append(f + 5)         # the poke 5 frames into the entry
        out.append((f, f + n - 1, mode, dict(kw)))
        f += n
    return out, d


def _pokes(d):
    return [gs.format_w(0, f, 9, 0x104B0C, b'\x00', b'\x01', 0, 0) for f in d]


ALL7_C4 = _marks(l0=0xC4, l1=0xC4, l2=0xC4, l3=0xC4, l4=0xC4, l5=0xC4, l6=0xC4)


def _log_s1(path, kind='S', drop=lambda f: False, change=None):
    """_log over the side-1 fields (FIELDS_S1: c82b appended)."""
    out = []
    for f0, f1, mode, kw in path:
        for f in range(f0, f1 + 1):
            if drop(f):
                continue
            k = dict(kw)
            if change:
                k = change(f, mode, k)
            out.append(_rec_fields(kind, f, mode, k, FIELDS_S1))
    return out


def _u10_side1(stale_k=None, order_swap=False, held=7, c82b=1, ending_c1=4, ad4=1):
    """The side-1 ending path (gp-u26-side1-ending) as (n frames, mode, fields)
    segments, numbered on from f = 0x100: _u10's shape with the winner's side
    swapped — P2 wins, so s0_5a is KO'd, w2/w3 are swapped, ad4 = 1, the land
    marks are 0xC4 (ARMADON's, side 1), the seventh-land tally is t104's high
    byte (DS_00108104[1]) and the world-domination count is c82b (slot 1's +0x82).
    Returns [(f0, f1, mode, kw)] and the first f of every mode-0xD entry."""
    segs = [(0x41, 0x03, {}), (0x152, 0x27, {}), (0x63, 0x10, {}),
            (0x20, 0x06, dict(c1=4, b1e=1, afc=5)),
            (0x20, 0x08, dict(c1=4, b1e=1, afc=5, s0_5a=0x78, w2=0, w3=1,
                              ad4=gw.NO_RESULT, **ALL7_C4)),
            (0x20, 0x06, dict(b1e=2, afc=5, w3=1, ad4=gw.NO_RESULT, **ALL7_C4)),
            (0x20, 0x09, dict(b1e=2, afc=5, s0_5a=0x78, w3=2, ad4=1, **ALL7_C4)),
            (0x20, 0x17, dict(afc=5, w3=2, **ALL7_C4)),
            (0x10, 0x12, dict(c1=4, afc=5, w3=2, **ALL7_C4))]
    if order_swap:
        segs += [(0x10, 0x12, dict(afc=5, w3=2, c82b=1, t104=0)),
                 (0x10, 0x12, dict(afc=5, w3=2, c82b=1, t104=held << 8))]
    else:
        segs += [(0x10, 0x12, dict(afc=5, w3=2, t104=held << 8, **ALL7_C4)),
                 (0x10, 0x12, dict(afc=5, w3=2, t104=held << 8, c82b=c82b))]
    segs += [(0x10, 0x23, dict(afc=5)), (0x10, 0x22, dict(afc=5)), (0x10, 0x24, dict(afc=7)),
             (0x10, 0x0C, dict(afc=7, b14=1, b21=0))]
    for k in range(1, 8):
        segs.append((0x10, 0x0D, dict(afc=7, b14=1, s0_5a=0x78,
                                      b21=stale_k if k == stale_k else k - 1)))
        if k < 7:
            segs.append((0x10, 0x0C, dict(afc=7, b14=1, b21=k)))
    segs += [(0x10, 0x0F, dict(afc=7, b21=7)), (0x10, 0x15, {}),
             (0x10, 0x1F, dict(c1=ending_c1, ad4=ad4)), (0x10, 0x15, {}),
             (0x10, 0x1F, dict(c1=ending_c1, ad4=ad4)),
             (0x10, 0x15, {}), (0x10, 0x1E, {}), (0x10, 0x03, {})]
    out, f, d = [], 0x100, []
    for n, mode, kw in segs:
        if mode == 0x0D:
            d.append(f + 5)
        out.append((f, f + n - 1, mode, dict(kw)))
        f += n
    return out, d


class TestU10(unittest.TestCase):
    def check(self, **kw):
        path, d = _u10(**kw)
        out = io.StringIO()
        rc = gw.evidence('gp-u10-ending', _log(path) + _pokes(d), lambda s: out.write(s + '\n'))
        return rc, out.getvalue()

    def test_the_u10_path_passes_and_reproduces(self):
        rc, text = self.check()
        self.assertEqual(rc, 0)
        self.assertIn('30/30 milestones ok', text)
        path, d = _u10()
        n, rows = gw.reproduced('gp-u10-ending', _log(path, drop=lambda f: f % 3 == 0), _log(path, 'T'))
        self.assertEqual((n, len(rows)), (30, 30))
        out = io.StringIO()
        self.assertEqual(gw.path('gp-u10-ending', _log(path), _log(path, 'T'), 30, None,
                                 lambda s: out.write(s + '\n')), 0)
        self.assertIn('0 not reproduced through 29; ratchet N 30 ok', out.getvalue())

    def test_a_stale_kill_count_fails(self):
        rc, text = self.check(stale_k=3)
        self.assertEqual(rc, 1)
        self.assertIn("FAIL: final opponent 3 KO'd", text)

    def test_the_seventh_land_is_required(self):
        rc, text = self.check(held=6)
        self.assertEqual(rc, 1)
        self.assertIn('FAIL: 0x41C28 state 3 counts the seventh land', text)

    def test_world_domination_needs_its_count(self):
        rc, text = self.check(c82=0)
        self.assertEqual(rc, 1)
        self.assertIn('FAIL: WORLD DOMINATION', text)

    def test_milestones_come_in_order(self):
        rc, text = self.check(order_swap=True)
        self.assertEqual(rc, 1)
        self.assertIn('before the previous milestone', text)

    def test_the_ending_must_be_saurons(self):
        rc, text = self.check(ending_c0=3)
        self.assertEqual(rc, 1)
        self.assertIn("FAIL: SAURON's ending", text)

    def test_a_cpu_round_win_at_round_1_fails(self):
        # the U10 round-1 row's own w3 == 0 (Task 6 re-review): a CPU round win in mode 8
        path, d = _u10()
        L = _log(path, change=lambda f, m, k: dict(k, w3=1) if m == 0x08 else k)
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u10-ending', L + _pokes(d), lambda s: out.write(s + '\n')), 1)
        self.assertIn("FAIL: round 1 KO with the seven lands poked P1's", out.getvalue())

    def test_a_consumed_death_done_byte_fails(self):
        path, d = _u10()
        # the poke landed after mode 0xD consumed the game's own byte: the S record at the
        # poke is already in mode 0xC (or 0xD with a moved KO count) and finds the byte clear
        out = io.StringIO()
        late = [f + 0x10 for f in d[:1]] + d[1:]
        self.assertEqual(gw.evidence('gp-u10-ending', _log(path) + _pokes(late), lambda s: out.write(s + '\n')), 1)
        self.assertIn('did not find a clear byte in the mode-0xD entry', out.getvalue())
        L = _log([(0x10, 0x14, 0x0D, dict(b21=1)), (0x15, 0x19, 0x0D, dict(b21=2))])
        self.assertEqual(gw.death_done_set(_pokes([0x17]), gs.snapshots(L)), [0x17])
        self.assertEqual(gw.death_done_set(_pokes([0x12]), gs.snapshots(L)), [])


class TestEvidence(unittest.TestCase):
    def test_u13_ending_is_blizzard(self):
        rows = gw.MILESTONES['gp-u13-ending2']
        label, mode, k, pred = next(r for r in rows if r[1] == 0x1F)
        self.assertIn('BLIZZARD', label)
        self.assertIn('c0=1', label)

    def test_the_u9_path_passes(self):
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', _log(U9), lambda s: out.write(s + '\n')), 0)
        self.assertIn('8/8 milestones ok', out.getvalue())

    def test_a_wrong_character_fails(self):
        L = _log(U9, change=lambda f, m, k: dict(k, c0=1) if f >= 0x486 else k)
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', L, lambda s: out.write(s + '\n')), 1)
        self.assertIn('FAIL: round 1: P1 is character 0', out.getvalue())

    def test_a_cpu_round_win_fails(self):
        L = _log(U9, change=lambda f, m, k: dict(k, w2=1, w3=1) if m == 0x08 else k)
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', L, lambda s: out.write(s + '\n')), 1)
        self.assertIn('FAIL: round 1 KO', out.getvalue())

    def test_an_unmarked_land_fails(self):
        L = _log(U9, change=lambda f, m, k: dict(k, m10a=0) if m == 0x12 else k)
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', L, lambda s: out.write(s + '\n')), 1)
        self.assertIn('FAIL: the conquered-lands screen', out.getvalue())

    def test_no_win_fields_fails(self):
        L = [l.split(' afc=')[0] + ' kb=0000 head=001E tail=001E' for l in _log(U9)]
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', L, lambda s: out.write(s + '\n')), 1)
        self.assertIn('records no WIN fields', out.getvalue())

    def test_a_death_done_poke_must_find_the_byte_clear(self):
        L = _log([(0x10, 0x20, 0x0D, {})])
        ok = gs.format_w(0, 0x15, 9, 0x104B0C, b'\x00', b'\x01', 0, 0)
        self.assertEqual(gw.death_done_set(L + [ok], gs.snapshots(L)), [])
        bad = gs.format_w(0, 0x16, 9, 0x104B0C, b'\x01', b'\x01', 0, 0)
        self.assertEqual(gw.death_done_set(L + [bad], gs.snapshots(L)), [0x16])
        L2 = _log([(0x10, 0x20, 0x0D, dict(b0c=1))])
        self.assertEqual(gw.death_done_set(L2 + [ok], gs.snapshots(L2)), [0x15])


# gp-u14-win-live's milestone rows are _START + _MATCH1: (0x10,1), (0x06,1), (0x08,1),
# (0x06,2), (0x09,1), (0x12,1). The match rule reads the (0x09,1) row's frame; the fixture
# puts the two mode-6 entries and the two results at these frames.
MATCH_FRAMES = [0x141, 0x201, 0x2F5, 0x301, 0x3F5, 0x401]


def _fought_match(b0c=0, live=0x10, result=(0, 2, 0), timeout=False):
    """A fought 2-0 match-1 win with no poke: the mode-6 entries carry a live damage
    value and the mode-8/9 results carry 0x78. `result` = (ad4, w2, w3)."""
    ad4, w2, w3 = result
    tail_mode = 0x07 if timeout else 0x06
    return _log([
        (0x100, 0x140, 0x03, {}),
        (0x141, 0x200, 0x27, {}),
        (0x201, 0x2F4, 0x06, dict(b1f=3, c0=0, b1e=1, s1_5a=live)),
        (0x2F5, 0x300, 0x08, dict(b1f=3, c0=0, b1e=1, s1_5a=0x78, w2=1, w3=0,
                                  ad4=gw.NO_RESULT)),
        (0x301, 0x3EF, 0x06, dict(b1f=3, b1e=2, s1_5a=live, w2=1)),
        (0x3F0, 0x3F4, tail_mode, dict(b1f=3, b1e=2, s1_5a=live, w2=1)),
        (0x3F5, 0x400, 0x09, dict(b1f=3, s1_5a=0x78, w2=w2, w3=w3, ad4=ad4, b0c=b0c)),
        (0x401, 0x410, 0x12, dict(c0=0, m106=0x82, m10a=0)),
    ])


def _poke(f, addr):
    return gs.format_w(0, f, 1, addr, b'\x10', b'\x78', 0, 0)


class TestLiveMatch(unittest.TestCase):
    """The match rule split from the final's (Final Truth Task 3, record §F3.1): a fought
    match-1 win needs no b0c; 0x37FF6's write is the final's 0x0C->0x0D chain, not the
    match's. The U9 fixture satisfies every gp-u14 milestone row but is poked (its mode-6
    entry has no live damage value)."""

    def run_match(self, lines, frames=None):
        out = io.StringIO()
        rc = gw.live_match('gp-u14-win-live', MATCH_FRAMES if frames is None else frames,
                           lines, gs.snapshots(lines), lambda s: out.write(s + '\n'))
        return rc, out.getvalue()

    def test_the_match_rule_passes_a_fought_two_human_win(self):
        rc, text = self.run_match(_fought_match())
        self.assertEqual(rc, 0)
        self.assertIn('the match result at f=3F5 names side 0', text)
        self.assertIn('(w2 = 2 > w3 = 0)', text)

    def test_the_milestones_alone_do_not_prove_a_fought_ko(self):
        # the U9 fixture satisfies every gp-u14 milestone row, but its round-2 mode-6
        # entry carries no live damage value (the KO is the poked mode-9 record's 0x78)
        L = _log(U9)
        snaps = gs.snapshots(L)
        frames = gw.milestone_frames('gp-u14-win-live', snaps, gw.start_frame(snaps))
        rc, text = self.run_match(L, frames)
        self.assertEqual(rc, 1)
        self.assertIn('no live 0 < s1_5a < 78', text)

    def test_the_match_result_must_name_the_winner(self):
        rc, text = self.run_match(_fought_match(result=(1, 0, 2)))   # the artifact's shape
        self.assertEqual(rc, 1)
        self.assertIn('names side 1', text)
        self.assertIn('not a 2-0 match win for side 0', text)

    def test_the_winner_needs_two_round_wins(self):
        rc, text = self.run_match(_fought_match(result=(0, 1, 0)))
        self.assertEqual(rc, 1)
        self.assertIn('not a 2-0 match win for side 0', text)

    def test_a_timeout_before_the_match_result_fails(self):
        rc, text = self.run_match(_fought_match(timeout=True))
        self.assertEqual(rc, 1)
        self.assertIn('time-out: mode 7 at f=3F0', text)

    def test_a_no_live_value_fails(self):
        rc, text = self.run_match(_fought_match(live=0))
        self.assertEqual(rc, 1)
        self.assertIn('no live 0 < s1_5a < 78', text)

    def test_a_ko_poke_is_refused(self):
        rc, text = self.run_match(_fought_match() + [_poke(0x408, 0x10789E)])
        self.assertEqual(rc, 1)
        self.assertIn('a poke at f=408 covers 0010789E', text)

    def test_a_death_done_poke_is_refused(self):
        # the match rule still refuses a W record on the death-done byte, but it no longer
        # requires the byte to be 1 (that is the final-only rule)
        rc, text = self.run_match(_fought_match() + [_poke(0x408, 0x104B0C)])
        self.assertEqual(rc, 1)
        self.assertIn('a poke at f=408 covers 00104B0C', text)
        rc, text = self.run_match(_fought_match(b0c=0))
        self.assertEqual(rc, 0)
        self.assertNotIn('b0c', text)

    @unittest.skipUnless(os.path.isfile(os.path.join(ROOT, 'data', 'k11-captures',
                                                     'gp-u14-win-live.failed', 'poll.log')),
                         'the .failed artifact is git-ignored')
    def test_the_failed_artifacts_fought_ko_is_not_a_win(self):
        # the artifact's round 1/2 KOs are the game's own (s0_5a 71 -> 78, 63 -> 78), and
        # gp_twop.ko --arcade passes for the side it names; the match rule for side 0 fails
        # because the match result names side 1 (w3 = 2 > w2 = 0).
        import gp_twop as gt
        with open(os.path.join(ROOT, 'data', 'k11-captures', 'gp-u14-win-live.failed',
                               'poll.log')) as f:
            lines = f.read().splitlines()
        self.assertEqual(gt.ko(lines, winner=1, arcade=True)['fail'], [])
        frames = [None, None, None, None, 0xF9F, None]
        out = io.StringIO()
        rc = gw.live_match('gp-u14-win-live', frames, lines, gs.snapshots(lines),
                           lambda s: out.write(s + '\n'))
        self.assertEqual(rc, 1)
        self.assertIn('the match result at f=F9F names side 1', out.getvalue())
        self.assertIn('not a 2-0 match win for side 0', out.getvalue())


_U10_MODE0F = next(i for i, r in enumerate(gw.MILESTONES['gp-u10-ending'])
                   if (r[1], r[2]) == (0x0F, 1))
_U10_FRAMES = [0x100 if i == _U10_MODE0F else None
               for i in range(len(gw.MILESTONES['gp-u10-ending']))]


class TestLiveFinal(unittest.TestCase):
    """The final-only rule: b0c = 1 is required on the final path (0x0C -> 0x0D -> 0x0F),
    where 0x37FF6's write is the one the U10 DEATH_DONE poke replaced. No scenario is in
    FINAL_WIN yet (the fought final is not captured); the synthetic row uses
    gp-u10-ending's mode-0x0F milestone."""

    def _final(self, b0c=1):
        return _log([(0x100, 0x120, 0x0F, dict(b21=7))],
                    change=lambda f, m, k: dict(k, b0c=b0c) if f >= 0x110 else k)

    def run_final(self, lines):
        out = io.StringIO()
        rc = gw.live_final('gp-u10-ending', _U10_FRAMES, lines, gs.snapshots(lines),
                           lambda s: out.write(s + '\n'))
        return rc, out.getvalue()

    def test_a_final_row_requires_b0c_after_the_mode_0f_entry(self):
        rc, text = self.run_final(self._final(1))
        self.assertEqual(rc, 0)
        self.assertIn('(b0c = 1 at f=110, no DEATH_DONE poke)', text)
        rc, text = self.run_final(self._final(0))
        self.assertEqual(rc, 1)
        self.assertIn('no S record with b0c = 1 at or after mode 0x0F', text)

    def test_b0c_before_the_mode_0f_entry_does_not_count(self):
        # the game clears b0c on the way in (mode 0x24's 0x2703D / mode 0x17's 0x27200):
        # a stale pre-entry 1 is not the final path's death-done write
        L = _log([(0x100, 0x120, 0x0F, dict(b21=7))],
                 change=lambda f, m, k: dict(k, b0c=1 if f < 0x100 else 0))
        rc, text = self.run_final(L)
        self.assertEqual(rc, 1)
        self.assertIn('no S record with b0c = 1 at or after mode 0x0F', text)

    def test_a_death_done_poke_is_refused(self):
        rc, text = self.run_final(self._final(0) + [_poke(0x111, 0x104B0C)])
        self.assertEqual(rc, 1)
        self.assertIn('a poke at f=111 covers 00104B0C', text)


class TestTalonEnding(unittest.TestCase):
    """gp-u18-talon-ending (endgame truth Task 3; record §E1.1): TALON is character 2,
    character-select cell 1, title id 0xE0 (TALON'S DREAM). The mode-0x1F row's label
    is the character name, not the title; the land mark is 0x80 | side 0 | 2 = 0x82."""

    def test_start_char_labels_the_character_and_cell(self):
        rows = gw.start_char(2, 1)
        self.assertEqual([(r[1], r[2]) for r in rows], [(0x10, 1), (0x06, 1)])
        self.assertIn('character 2', rows[1][0])
        self.assertIn('cursor 1', rows[1][0])
        self.assertTrue(rows[1][3](dict(c0=2, b1e=1)))
        self.assertFalse(rows[1][3](dict(c0=1, b1e=1)))
        self.assertFalse(rows[1][3](dict(c0=2, b1e=2)))
        # the label and the predicate follow the arguments (cell 3 is character 1)
        blizzard = gw.start_char(1, 3)
        self.assertIn('character 1', blizzard[1][0])
        self.assertIn('cursor 3', blizzard[1][0])
        self.assertTrue(blizzard[1][3](dict(c0=1, b1e=1)))
        self.assertFalse(blizzard[1][3](dict(c0=2, b1e=1)))

    def test_the_milestones_end_with_talons_ending(self):
        rows = gw.MILESTONES['gp-u18-talon-ending']
        self.assertEqual(len(rows), 30)
        self.assertEqual([r[:3] for r in rows[:2]], [r[:3] for r in gw.start_char(2, 1)])
        ending = [r for r in rows if (r[1], r[2]) == (0x1F, 1)]
        self.assertEqual(len(ending), 1)
        label, _, _, pred = ending[0]
        self.assertTrue(label.startswith("TALON's ending"), label)
        self.assertIn('c0=2', label)
        self.assertNotIn('DREAM', label)          # the title is a 0xA80E4 fact, not the label
        self.assertTrue(pred(dict(ad4=0, c0=2)))
        self.assertFalse(pred(dict(ad4=0, c0=1)))

    def test_the_round_1_row_expects_talons_land_marks(self):
        rows = gw.MILESTONES['gp-u18-talon-ending']
        round1 = [r for r in rows if (r[1], r[2]) == (0x08, 1)]
        self.assertEqual(len(round1), 1)
        marks82 = _marks(l0=0x82, l1=0x82, l2=0x82, l3=0x82, l4=0x82, l5=0x82, l6=0x82)
        self.assertTrue(round1[0][3](dict(c0=2, s1_5a=0x78, w2=1, w3=0, ad4=gw.NO_RESULT,
                                          **marks82)))
        self.assertFalse(round1[0][3](dict(c0=2, s1_5a=0x78, w2=1, w3=0, ad4=gw.NO_RESULT,
                                           **ALL7)))       # SAURON's 0x80 is not TALON's

    def test_the_seventh_land_row_is_the_t104_tally(self):
        rows = gw.MILESTONES['gp-u18-talon-ending']
        seventh = [r for r in rows if (r[1], r[2]) == (0x12, 1)
                   and r[0].startswith('0x41C28 state 3')]
        self.assertEqual(len(seventh), 1)
        self.assertEqual(seventh[0][3](dict(t104=7)), True)
        self.assertEqual(seventh[0][3](dict(t104=6)), False)
        # the 0x82 marks are the mode-8 row's; the seventh land counts DS_00108104[0],
        # so the row holds with no mark bytes at all
        self.assertTrue(seventh[0][3](dict(t104=7, m106=0, m10a=0)))


class TestVertigoEnding(unittest.TestCase):
    """gp-u19-vertigo-ending (endgame truth Task 4; record §E1.1/§E4): VERTIGO is
    character 3, character-select cell 2, title id 0xE1 (THE PALACE OF VERTIGO). The
    mode-0x1F row's label is the character name, not the title; the land mark is
    0x80 | side 0 | 3 = 0x83."""

    def test_start_char_labels_the_character_and_cell(self):
        rows = gw.start_char(3, 2)
        self.assertEqual([(r[1], r[2]) for r in rows], [(0x10, 1), (0x06, 1)])
        self.assertIn('character 3', rows[1][0])
        self.assertIn('cursor 2', rows[1][0])
        self.assertTrue(rows[1][3](dict(c0=3, b1e=1)))
        self.assertFalse(rows[1][3](dict(c0=2, b1e=1)))
        self.assertFalse(rows[1][3](dict(c0=3, b1e=2)))

    def test_the_milestones_end_with_vertigos_ending(self):
        rows = gw.MILESTONES['gp-u19-vertigo-ending']
        self.assertEqual(len(rows), 30)
        self.assertEqual([r[:3] for r in rows[:2]], [r[:3] for r in gw.start_char(3, 2)])
        ending = [r for r in rows if (r[1], r[2]) == (0x1F, 1)]
        self.assertEqual(len(ending), 1)
        label, _, _, pred = ending[0]
        self.assertTrue(label.startswith("VERTIGO's ending"), label)
        self.assertIn('c0=3', label)
        self.assertNotIn('PALACE', label)         # the title is a 0xA80E4 fact, not the label
        self.assertTrue(pred(dict(ad4=0, c0=3)))
        self.assertFalse(pred(dict(ad4=0, c0=2)))
        self.assertFalse(pred(dict(ad4=1, c0=3)))

    def test_the_round_1_row_expects_vertigos_land_marks(self):
        rows = gw.MILESTONES['gp-u19-vertigo-ending']
        round1 = [r for r in rows if (r[1], r[2]) == (0x08, 1)]
        self.assertEqual(len(round1), 1)
        marks83 = _marks(l0=0x83, l1=0x83, l2=0x83, l3=0x83, l4=0x83, l5=0x83, l6=0x83)
        self.assertTrue(round1[0][3](dict(c0=3, s1_5a=0x78, w2=1, w3=0, ad4=gw.NO_RESULT,
                                          **marks83)))
        self.assertFalse(round1[0][3](dict(c0=3, s1_5a=0x78, w2=1, w3=0, ad4=gw.NO_RESULT,
                                           **_marks(l0=0x82, l1=0x82, l2=0x82, l3=0x82,
                                                    l4=0x82, l5=0x82, l6=0x82))))

    def test_the_seventh_land_row_is_the_t104_tally(self):
        rows = gw.MILESTONES['gp-u19-vertigo-ending']
        seventh = [r for r in rows if (r[1], r[2]) == (0x12, 1)
                   and r[0].startswith('0x41C28 state 3')]
        self.assertEqual(len(seventh), 1)
        self.assertEqual(seventh[0][3](dict(t104=7)), True)
        self.assertEqual(seventh[0][3](dict(t104=6)), False)
        self.assertTrue(seventh[0][3](dict(t104=7, m106=0, m10a=0)))


class TestChaosEnding(unittest.TestCase):
    """gp-u20-chaos-ending (endgame truth Task 5; record §E1.1/§E5): CHAOS is character
    6, character-select cell 4, title id 0xE4 (CHAOS' REDEMPTION). The mode-0x1F row's
    label is the character name, not the title; the land mark is 0x80 | side 0 | 6 = 0x86."""

    def test_start_char_labels_the_character_and_cell(self):
        rows = gw.start_char(6, 4)
        self.assertEqual([(r[1], r[2]) for r in rows], [(0x10, 1), (0x06, 1)])
        self.assertIn('character 6', rows[1][0])
        self.assertIn('cursor 4', rows[1][0])
        self.assertTrue(rows[1][3](dict(c0=6, b1e=1)))
        self.assertFalse(rows[1][3](dict(c0=5, b1e=1)))
        self.assertFalse(rows[1][3](dict(c0=6, b1e=2)))

    def test_the_milestones_end_with_chaos_ending(self):
        rows = gw.MILESTONES['gp-u20-chaos-ending']
        self.assertEqual(len(rows), 30)
        self.assertEqual([r[:3] for r in rows[:2]], [r[:3] for r in gw.start_char(6, 4)])
        ending = [r for r in rows if (r[1], r[2]) == (0x1F, 1)]
        self.assertEqual(len(ending), 1)
        label, _, _, pred = ending[0]
        self.assertTrue(label.startswith("CHAOS's ending"), label)
        self.assertIn('c0=6', label)
        self.assertNotIn('REDEMPTION', label)    # the title is a 0xA80E4 fact, not the label
        self.assertTrue(pred(dict(ad4=0, c0=6)))
        self.assertFalse(pred(dict(ad4=0, c0=5)))
        self.assertFalse(pred(dict(ad4=1, c0=6)))

    def test_the_round_1_row_expects_chaos_land_marks(self):
        rows = gw.MILESTONES['gp-u20-chaos-ending']
        round1 = [r for r in rows if (r[1], r[2]) == (0x08, 1)]
        self.assertEqual(len(round1), 1)
        marks86 = _marks(l0=0x86, l1=0x86, l2=0x86, l3=0x86, l4=0x86, l5=0x86, l6=0x86)
        self.assertTrue(round1[0][3](dict(c0=6, s1_5a=0x78, w2=1, w3=0, ad4=gw.NO_RESULT,
                                          **marks86)))
        self.assertFalse(round1[0][3](dict(c0=6, s1_5a=0x78, w2=1, w3=0, ad4=gw.NO_RESULT,
                                           **_marks(l0=0x85, l1=0x85, l2=0x85, l3=0x85,
                                                    l4=0x85, l5=0x85, l6=0x85))))

    def test_the_seventh_land_row_is_the_t104_tally(self):
        rows = gw.MILESTONES['gp-u20-chaos-ending']
        seventh = [r for r in rows if (r[1], r[2]) == (0x12, 1)
                   and r[0].startswith('0x41C28 state 3')]
        self.assertEqual(len(seventh), 1)
        self.assertEqual(seventh[0][3](dict(t104=7)), True)
        self.assertEqual(seventh[0][3](dict(t104=6)), False)
        self.assertTrue(seventh[0][3](dict(t104=7, m106=0, m10a=0)))


class TestArmadonEnding(unittest.TestCase):
    """gp-u21-armadon-ending (endgame truth Task 6; record §E1.1/§E6): ARMADON is
    character 4, character-select cell 5, title id 0xE2 (THE MEDITATION OF ARMADON). The
    mode-0x1F row's label is the character name, not the title; the land mark is
    0x80 | side 0 | 4 = 0x84."""

    def test_the_milestones_end_with_armadons_ending(self):
        rows = gw.MILESTONES['gp-u21-armadon-ending']
        self.assertEqual(len(rows), 30)
        # the start rows are start_char(4, 5)'s: character-select mode 0x10 and round 1's
        # mode 0x06 c0=4/b1e=1, with the character and the cursor in the label (Task 4/5
        # cover the helper's generic shape; these are ARMADON's own cell and class)
        self.assertEqual([r[:3] for r in rows[:2]], [r[:3] for r in gw.start_char(4, 5)])
        self.assertEqual([(r[1], r[2]) for r in rows[:2]], [(0x10, 1), (0x06, 1)])
        self.assertIn('character 4', rows[1][0])
        self.assertIn('cursor 5', rows[1][0])
        self.assertTrue(rows[1][3](dict(c0=4, b1e=1)))
        self.assertFalse(rows[1][3](dict(c0=3, b1e=1)))
        self.assertFalse(rows[1][3](dict(c0=4, b1e=2)))
        ending = [r for r in rows if (r[1], r[2]) == (0x1F, 1)]
        self.assertEqual(len(ending), 1)
        label, _, _, pred = ending[0]
        self.assertTrue(label.startswith("ARMADON's ending"), label)
        self.assertIn('c0=4', label)
        self.assertNotIn('MEDITATION', label)    # the title is a 0xA80E4 fact, not the label
        self.assertTrue(pred(dict(ad4=0, c0=4)))
        self.assertFalse(pred(dict(ad4=0, c0=3)))
        self.assertFalse(pred(dict(ad4=1, c0=4)))

    def test_the_round_1_row_expects_armadons_land_marks(self):
        rows = gw.MILESTONES['gp-u21-armadon-ending']
        round1 = [r for r in rows if (r[1], r[2]) == (0x08, 1)]
        self.assertEqual(len(round1), 1)
        marks84 = _marks(l0=0x84, l1=0x84, l2=0x84, l3=0x84, l4=0x84, l5=0x84, l6=0x84)
        self.assertTrue(round1[0][3](dict(c0=4, s1_5a=0x78, w2=1, w3=0, ad4=gw.NO_RESULT,
                                          **marks84)))
        self.assertFalse(round1[0][3](dict(c0=4, s1_5a=0x78, w2=1, w3=0, ad4=gw.NO_RESULT,
                                           **_marks(l0=0x86, l1=0x86, l2=0x86, l3=0x86,
                                                    l4=0x86, l5=0x86, l6=0x86))))

    def test_the_seventh_land_row_is_the_t104_tally(self):
        rows = gw.MILESTONES['gp-u21-armadon-ending']
        seventh = [r for r in rows if (r[1], r[2]) == (0x12, 1)
                   and r[0].startswith('0x41C28 state 3')]
        self.assertEqual(len(seventh), 1)
        self.assertEqual(seventh[0][3](dict(t104=7)), True)
        self.assertEqual(seventh[0][3](dict(t104=6)), False)
        self.assertTrue(seventh[0][3](dict(t104=7, m106=0, m10a=0)))


class TestSide1Ending(unittest.TestCase):
    """gp-u26-side1-ending (state coverage Task 6; plan 2026-10-28-state-coverage Task 6): the side-1 (P2) tour and
    ending. P2 confirms character-select cell 5 (ARMADON, class 4) with p2.start, wins
    the match and the final (KO_P1 pokes P1's +0x5A DS_0010780A), and the mode-0x1F
    handler 0x208F8 reads the winner's char DS_0010782A[DS_00104AD4 * 0x94] = c1 = 4
    (side-independent). The land marks are P2's 0x80 | 1 << 6 | 4 = 0xC4; the
    seventh-land tally is DS_00108104[1] (t104's high byte); the world-domination
    count is slot 1's +0x82 (c82b)."""

    def test_the_side1_ending_rows_are_armadons(self):
        rows = gw.MILESTONES['gp-u26-side1-ending']
        self.assertEqual(len(rows), 30)
        self.assertEqual([r[:3] for r in rows[:2]], [r[:3] for r in gw.start_char_side1(4, 5)])
        self.assertEqual([(r[1], r[2]) for r in rows[:2]], [(0x10, 1), (0x06, 1)])
        self.assertIn('character 4', rows[1][0])
        self.assertIn('cursor 5', rows[1][0])
        self.assertTrue(rows[1][3](dict(c1=4, b1e=1)))
        self.assertFalse(rows[1][3](dict(c1=3, b1e=1)))
        self.assertFalse(rows[1][3](dict(c1=4, b1e=2)))
        ending = [r for r in rows if (r[1], r[2]) == (0x1F, 1)]
        self.assertEqual(len(ending), 1)
        label, _, _, pred = ending[0]
        self.assertTrue(label.startswith("ARMADON's ending"), label)
        self.assertIn('c1=4', label)
        self.assertNotIn('MEDITATION', label)    # the title is a 0xA80E4 fact, not the label
        self.assertTrue(pred(dict(ad4=1, c1=4)))
        self.assertFalse(pred(dict(ad4=0, c1=4)))     # P1 won (ad4=0), not P2
        self.assertFalse(pred(dict(ad4=1, c1=3)))

    def test_the_round_1_row_expects_side1_land_marks(self):
        rows = gw.MILESTONES['gp-u26-side1-ending']
        round1 = [r for r in rows if (r[1], r[2]) == (0x08, 1)]
        self.assertEqual(len(round1), 1)
        self.assertTrue(round1[0][3](dict(c1=4, s0_5a=0x78, w2=0, w3=1, ad4=gw.NO_RESULT,
                                          **ALL7_C4)))
        self.assertFalse(round1[0][3](dict(c1=4, s0_5a=0x78, w2=0, w3=1, ad4=gw.NO_RESULT,
                                            **_marks(l0=0x84, l1=0x84, l2=0x84, l3=0x84,
                                                     l4=0x84, l5=0x84, l6=0x84))))

    def test_the_seventh_land_row_reads_side1s_tally(self):
        rows = gw.MILESTONES['gp-u26-side1-ending']
        seventh = [r for r in rows if (r[1], r[2]) == (0x12, 1)
                   and r[0].startswith('0x41C28 state 3')]
        self.assertEqual(len(seventh), 1)
        self.assertTrue(seventh[0][3](dict(t104=0x0700)))    # side 1's byte is the high byte
        self.assertFalse(seventh[0][3](dict(t104=0x0007)))   # side 0's tally, not side 1's
        self.assertTrue(seventh[0][3](dict(t104=0x0700, m106=0, m10a=0)))

    def test_the_world_domination_row_reads_slot1s_count(self):
        rows = gw.MILESTONES['gp-u26-side1-ending']
        wd = [r for r in rows if r[0].startswith('WORLD DOMINATION')]
        self.assertEqual(len(wd), 1)
        self.assertTrue(wd[0][3](dict(c82b=1, m106=0, m10a=0)))
        self.assertFalse(wd[0][3](dict(c82b=0, m106=0, m10a=0)))

    def test_the_final_opponents_are_p1_koed(self):
        rows = gw.MILESTONES['gp-u26-side1-ending']
        kos = [r for r in rows if r[1] == 0x0D and r[0].startswith('final opponent')]
        self.assertEqual(len(kos), 7)
        for i, r in enumerate(kos, 1):
            self.assertTrue(r[3](dict(s0_5a=0x78, b21=i - 1, b0c=0)), i)
            self.assertFalse(r[3](dict(s0_5a=0, b21=i - 1, b0c=0)), i)

    def test_a_side1_log_passes_and_ad4_0_fails(self):
        path, d = _u10_side1()
        out = io.StringIO()
        rc = gw.evidence('gp-u26-side1-ending', _log_s1(path) + _pokes(d),
                         lambda s: out.write(s + '\n'))
        self.assertEqual(rc, 0, out.getvalue())
        self.assertIn('30/30 milestones ok', out.getvalue())
        # the same log with ad4 = 0 (P1 won) fails the mode-0x1F row
        bad, _ = _u10_side1(ad4=0)
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u26-side1-ending', _log_s1(bad) + _pokes(d),
                                     lambda s: out.write(s + '\n')), 1)
        self.assertIn("FAIL: ARMADON's ending", out.getvalue())

    def test_a_side1_log_reproduces_against_itself(self):
        path, d = _u10_side1()
        n, rows = gw.reproduced('gp-u26-side1-ending',
                                _log_s1(path, drop=lambda f: f % 3 == 0), _log_s1(path, 'T'))
        self.assertEqual((n, len(rows)), (30, 30))

    def test_the_p1_ending_checks_still_pass(self):
        # Task 6's side-1 additions must not move the P1 ending milestones:
        # gp-u18-talon-ending (TALON) and gp-u21-armadon-ending (ARMADON) keep 30 rows
        # each and their mode-0x1F rows stay side 0 (ad4=0, c0=char).
        for name, char in (('gp-u18-talon-ending', 2), ('gp-u21-armadon-ending', 4)):
            rows = gw.MILESTONES[name]
            self.assertEqual(len(rows), 30, name)
            ending = [r for r in rows if (r[1], r[2]) == (0x1F, 1)]
            self.assertEqual(len(ending), 1, name)
            self.assertTrue(ending[0][3](dict(ad4=0, c0=char)), name)
            self.assertFalse(ending[0][3](dict(ad4=1, c0=char)), name)


class TestDiabloEnding(unittest.TestCase):
    """gp-u22-diablo-ending (endgame truth Task 7; record §E1.1/§E7): DIABLO is
    character 5, character-select cell 6, title id 0xE3 (DIABLO'S HELL). The mode-0x1F
    row's label is the character name, not the title; the land mark is
    0x80 | side 0 | 5 = 0x85."""

    def test_the_milestones_end_with_diablos_ending(self):
        rows = gw.MILESTONES['gp-u22-diablo-ending']
        self.assertEqual(len(rows), 30)
        # the start rows are start_char(5, 6)'s: character-select mode 0x10 and round 1's
        # mode 0x06 c0=5/b1e=1, with the character and the cursor in the label (Task 4/5/6
        # cover the helper's generic shape; these are DIABLO's own cell and class)
        self.assertEqual([r[:3] for r in rows[:2]], [r[:3] for r in gw.start_char(5, 6)])
        self.assertEqual([(r[1], r[2]) for r in rows[:2]], [(0x10, 1), (0x06, 1)])
        self.assertIn('character 5', rows[1][0])
        self.assertIn('cursor 6', rows[1][0])
        self.assertTrue(rows[1][3](dict(c0=5, b1e=1)))
        self.assertFalse(rows[1][3](dict(c0=4, b1e=1)))
        self.assertFalse(rows[1][3](dict(c0=5, b1e=2)))
        ending = [r for r in rows if (r[1], r[2]) == (0x1F, 1)]
        self.assertEqual(len(ending), 1)
        label, _, _, pred = ending[0]
        self.assertTrue(label.startswith("DIABLO's ending"), label)
        self.assertIn('c0=5', label)
        self.assertNotIn('HELL', label)          # the title is a 0xA80E4 fact, not the label
        self.assertTrue(pred(dict(ad4=0, c0=5)))
        self.assertFalse(pred(dict(ad4=0, c0=4)))
        self.assertFalse(pred(dict(ad4=1, c0=5)))

    def test_the_round_1_row_expects_diablos_land_marks(self):
        rows = gw.MILESTONES['gp-u22-diablo-ending']
        round1 = [r for r in rows if (r[1], r[2]) == (0x08, 1)]
        self.assertEqual(len(round1), 1)
        marks85 = _marks(l0=0x85, l1=0x85, l2=0x85, l3=0x85, l4=0x85, l5=0x85, l6=0x85)
        self.assertTrue(round1[0][3](dict(c0=5, s1_5a=0x78, w2=1, w3=0, ad4=gw.NO_RESULT,
                                          **marks85)))
        self.assertFalse(round1[0][3](dict(c0=5, s1_5a=0x78, w2=1, w3=0, ad4=gw.NO_RESULT,
                                           **_marks(l0=0x84, l1=0x84, l2=0x84, l3=0x84,
                                                    l4=0x84, l5=0x84, l6=0x84))))

    def test_the_seventh_land_row_is_the_t104_tally(self):
        rows = gw.MILESTONES['gp-u22-diablo-ending']
        seventh = [r for r in rows if (r[1], r[2]) == (0x12, 1)
                   and r[0].startswith('0x41C28 state 3')]
        self.assertEqual(len(seventh), 1)
        self.assertEqual(seventh[0][3](dict(t104=7)), True)
        self.assertEqual(seventh[0][3](dict(t104=6)), False)
        self.assertTrue(seventh[0][3](dict(t104=7, m106=0, m10a=0)))


class TestPath(unittest.TestCase):
    def test_a_capture_without_mode_27_fails_cleanly(self):
        cap = _log([(0x100, 0x140, 0x03, {}), (0x141, 0x150, 0x10, {})])
        self.assertEqual(gw.reproduced('gp-u9-win', cap, _log(U9, 'T'))[0], 0)
        out = io.StringIO()
        self.assertEqual(gw.path('gp-u9-win', cap, _log(U9, 'T'), 0, None, lambda s: out.write(s + '\n')), 1)
        self.assertIn('FAIL: mode 0x27 never observed', out.getvalue())

    def test_the_same_path_reproduces_every_milestone(self):
        n, rows = gw.reproduced('gp-u9-win', _log(U9, drop=lambda f: f % 3 == 0), _log(U9, 'T'))
        self.assertEqual((n, len(rows)), (8, 8))

    def test_a_late_mode_counts_up_to_it(self):
        port = _log([(f0 + (2 if m == 0x12 else 0), f1, m, k) for f0, f1, m, k in U9], 'T')
        n, rows = gw.reproduced('gp-u9-win', _log(U9), port)
        self.assertEqual(n, 5)                    # the conquered-lands screen is the 6th row
        self.assertEqual((rows[5][1], rows[5][2]), (0xBA7, 0xBA9))

    def test_the_live_win_uses_the_unpoked_match_rows(self):
        # gp-u14-win-live (plan 2026-10-20-gameplay-truth-ii Task 6; spec W5): a match won
        # by fought hits, so the poked rows' seven-lands expectations do not apply; the
        # rows are exactly the shared start and match-1 rows.
        rows = gw.MILESTONES['gp-u14-win-live']
        self.assertEqual([r[:3] for r in rows], [r[:3] for r in gw._START + gw._MATCH1])

    def test_rows_cover_all_scenarios_in_order(self):
        self.assertEqual(sorted(gw.MILESTONES),
                         ['gp-u10-ending', 'gp-u13-ending2', 'gp-u14-win-live',
                          'gp-u18-talon-ending', 'gp-u19-vertigo-ending',
                          'gp-u20-chaos-ending', 'gp-u21-armadon-ending',
                          'gp-u22-diablo-ending', 'gp-u26-side1-ending', 'gp-u9-win'])
        for name, rows in gw.MILESTONES.items():
            for label, mode, k, pred in rows:
                self.assertTrue(0 < mode < 0x40 and k >= 1, (name, label))
        for name in ('gp-u10-ending', 'gp-u13-ending2', 'gp-u18-talon-ending',
                     'gp-u19-vertigo-ending', 'gp-u20-chaos-ending',
                     'gp-u21-armadon-ending', 'gp-u22-diablo-ending',
                     'gp-u26-side1-ending'):
            rows = [(m, k) for _, m, k, _ in gw.MILESTONES[name]]
            self.assertEqual(len(rows), 30)
            self.assertEqual(rows.count((0x0D, 7)), 1)
            self.assertLess(rows.index((0x0D, 1)), rows.index((0x0C, 2)))
            self.assertLess(rows.index((0x0C, 7)), rows.index((0x0D, 7)))
            self.assertEqual(rows[-1], (0x03, 1))

    def test_the_second_ending_pins_character_one(self):
        rows = gw.MILESTONES['gp-u13-ending2']
        ending = [r for r in rows if r[1] == 0x1F and r[2] == 1]
        self.assertTrue(ending)
        self.assertEqual(ending[0][3](dict(ad4=0, c0=1)), True)
        self.assertEqual(ending[0][3](dict(ad4=0, c0=0)), False)
        start = [r for r in rows if r[1] == 0x06 and r[2] == 1]
        self.assertTrue(start)
        self.assertEqual(start[0][3](dict(c0=1, b1e=1)), True)
        self.assertEqual(start[0][3](dict(c0=0, b1e=1)), False)
        # the round-1 row's land marks follow c0: BLIZZARD's 0x81, not SAURON's 0x80
        round1 = [r for r in rows if r[1] == 0x08 and r[2] == 1]
        self.assertTrue(round1)
        self.assertEqual(round1[0][3](dict(c0=1, s1_5a=0x78, w2=1, w3=0, ad4=gw.NO_RESULT,
                                           **_marks(l0=0x81, l1=0x81, l2=0x81, l3=0x81,
                                                    l4=0x81, l5=0x81, l6=0x81))), True)
        self.assertEqual(round1[0][3](dict(c0=1, s1_5a=0x78, w2=1, w3=0, ad4=gw.NO_RESULT,
                                           **ALL7)), False)


if __name__ == '__main__':
    unittest.main()
