# tools/tests/test_gp_keys.py — U11 in-match keys (record 2026-10-01-gameplay-u11 §K)
import io
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_capture as gc
import gp_keys as gk
import gp_session as gs

FIELDS = gs.SNAP_FIELDS + gs.SCENARIOS['gp-keys-fight']['extra']
STEPS = gs.SCENARIOS['gp-keys-fight']['steps']
BASE = 0x266000


def _word(step):
    act = STEPS[step][-1]
    return gs.KEYS[act[1]][1] if act[0] == 'key' else gs.PAD[act[1][0]][1]


def _fight(mut=None):
    """A synthetic gp-keys-fight poll.log that does what record §K.6 derives:
    boot in mode 3 with 5 credits, the three menu Enters, round 1 (mode 6, 4
    credits, side 0 in) from f=0x200, then each event of gk.EVENTS 10 frames
    apart from f=0x20A, every word consumed at its event's frame c. `mut`
    (label -> {field: value}) overrides the state an event leaves at its judged
    frame; 'split' consumes the event's second word one frame later."""
    mut = mut or {}
    st = {n: 0 for n, _, _ in FIELDS}
    st.update(mode=0x03, cred=5, t508=1, t50c=2, rng=gk.RESTART_SEED)
    change = {0x101: dict(mode=0x27), 0x103: dict(mode=0x2D, cred=4, b1f=1), 0x200: dict(mode=0x06, rng=0x1234)}
    press, heads, skip = [], [], set()
    for s, f in ((0, 0x101), (1, 0x102), (2, 0x103)):
        press.append((f - 1, s, 0x1C0D))
        heads.append(f)
    f, pz = 0x20A, {'spz': 0, 'mpz': 0}
    for label, step, n, rule in gk.EVENTS:
        for k in range(n):
            press.append((f - 1, step + k, _word(step + k)))
            heads.append(f + (1 if k and mut.get(label) == 'split' else 0))
        kind = rule[0]
        at = f
        if kind == 'latched':
            change[f] = dict(lat=rule[1])
        elif kind == 'cleared':
            change[f] = dict(lat=0)
        elif kind == 'toggled':
            pz[rule[2]] ^= 1
            change[f] = {'lat': rule[1], rule[2]: pz[rule[2]]}
        elif kind == 'b0':
            change[f] = dict(raw=0x01000000, lat=rule[1])
            change[f + 1] = dict(new=0x01000000, held=0x01000000, e0=0x0101)
            change[f + 2] = dict(new=0, e0=0x0100)
            change[f + 3] = dict(raw=0)
            change[f + 4] = dict(held=0, e0=0)
            at = f + 1
        elif kind == 'join':
            change[f] = dict(raw=0x100, lat=rule[1])
            change[f + 1] = dict(new=0x100, held=0x100, mode=gk.JOIN_MODE, b1f=3)
            change[f + 2] = dict(new=0)
            change[f + 3] = dict(raw=0)
            at = f + 1
        else:
            skip.add(f)                              # 0x24AB0 abandons iteration c
            change[f + 1] = dict(mode=0x03, rng=gk.RESTART_SEED, cred=5, b1f=0)
            at = f + 1
        if isinstance(mut.get(label), dict):
            change.setdefault(at, {}).update(mut[label])
        f += 10
    last = f + 4
    out = []
    for g in range(0x100, last):
        while press and press[0][0] == g:
            pf, s, w = press.pop(0)
            out.append('I ms=0 f=%04X step=%d press=k scan=%02X lin=00010000 old=FF bios=%04X ring=1 late=0'
                       % (pf, s, w >> 8, w))
        for h in [h for h in heads if h == g]:
            out.append('H ms=0 f=%04X head=0020' % h)
        heads = [h for h in heads if h != g]
        st.update(change.get(g, {}))
        if g in skip or 0x104 <= g < 0x1F0:
            continue                                 # the restart's iteration; the menus and the select
        out.append(gs.format_s(0, dict(st, f=g), gs.raw_to_kb(st['raw']), 0x1E, 0x1E, FIELDS))
    out.append('X ms=0 f=%04X step=18 end' % (last - 1))
    return out


def _port(lines):
    """The port's trace for a capture: the same records as T lines."""
    return ['T ' + l[2:] for l in lines if l.startswith('S ')]


class TestKeys(unittest.TestCase):
    def test_keys_are_bios_make_words(self):
        # scan << 8 | ascii; an Alt-letter has ascii 0 (the 0x24D96 arms)
        self.assertEqual([gs.KEYS[k] for k in ('space', 'y', 'n', 'alt-q', 'alt-s', 'alt-m')],
                         [(0x39, 0x3920), (0x15, 0x1579), (0x31, 0x316E),
                          (0x10, 0x1000), (0x1F, 0x1F00), (0x32, 0x3200)])
        self.assertEqual(gs.KEYS['enter'], (0x1C, 0x1C0D))         # unchanged
        self.assertEqual(gs.KEYS['esc'], (0x01, 0x011B))

    def test_an_answer_fires_with_its_opener(self):
        s = gs.Schedule(STEPS)
        s.due_boot(gs.ENTER_WAIT)
        s.on_mode(0x141, 0x27)
        s.due(0x141 + 149)
        s.due(0x141 + 299)
        s.on_mode(0x7F5, 0x06)
        self.assertEqual([x[0] for x in s.due(0x7F5 + 9)], [3])
        self.assertEqual([x[0] for x in s.due(0x7F5 + 19)], [4, 5])      # space, space in one spin
        self.assertEqual([x[0] for x in s.due(0x7F5 + 29)], [6])
        self.assertEqual([x[0] for x in s.due(0x7F5 + 39)], [7, 8])      # esc, n
        F = s.frame_of[8]
        self.assertEqual(F, 0x7F5 + 40)
        # steps 9-17 in order, each with its `after` gap exactly as the scenario defines it
        want = [(10, ('key', 'alt-m')), (10, ('key', 'alt-q')), (0, ('key', 'n')),
                (10, ('key', 'alt-s')), (10, ('key', 'alt-m')),
                (10, ('pad', ('p1.start',), 3)), (10, ('pad', ('p2.start',), 3)),
                (20, ('key', 'esc')), (0, ('key', 'y'))]
        self.assertEqual([(st[1], st[2]) for st in STEPS[9:18] if st[0] == 'after'], want)
        self.assertEqual([st[0] for st in STEPS[9:18]], ['after'] * 9)
        self.assertEqual(STEPS[18], ('until_mode', 0x03, 0))
        self.assertEqual(len(STEPS), 19)
        fired, frames = [], []
        for gap, _ in want:
            F += gap
            frames.append(F)
            fired += s.due(F - 1)
        self.assertEqual([x[0] for x in fired], list(range(9, 18)))
        self.assertEqual([x[1] for x in fired], [a for _, a in want])
        self.assertEqual([s.frame_of[k] for k in range(9, 18)], frames)
        self.assertEqual(s.total, 18)

    def test_extra_fields_are_read_and_logged(self):
        m = bytearray(0x400000)
        o = BASE + 0x105F30 - gs.DATA_BASE_VA
        m[o:o + 4] = (0x1B).to_bytes(4, 'little')
        m[BASE + 0x1028DA - gs.DATA_BASE_VA] = 1
        v = gc.read_snap(m, BASE, FIELDS)
        self.assertEqual((v['lat'], v['mpz'], v['spz']), (0x1B, 1, 0))
        self.assertNotIn('lat', gc.read_snap(m, BASE))                    # the default is unchanged
        rec = gs.parse(gs.format_s(0, dict(v, f=7), 0, 0x1E, 0x1E, FIELDS))
        self.assertEqual((rec['lat'], rec['mpz']), (0x1B, 1))
        self.assertNotIn('lat', gs.parse(gs.format_s(0, v, 0, 0x1E, 0x1E)))


class TestEvidence(unittest.TestCase):
    def test_the_raw_effects_pass(self):
        L = _fight()
        rows = gk.judge_all(L, gk.records(L, 'S'))
        self.assertEqual([r[0] for r in rows], [e[0] for e in gk.EVENTS])
        self.assertEqual([r[3] for r in rows], [[]] * len(gk.EVENTS))

    def test_each_rule_can_fail(self):
        cases = {'enter': {'lat': 0}, 'pause': {'lat': 0x20}, 'alt-s on': {'spz': 0},
                 'esc-n': {'mpz': 1}, 'alt-m on': {'lat': 0}, 'altq-n': {'lat': 0x6E},
                 'alt-s off': {'mpz': 0}, 'alt-m off': {'mpz': 1}, 'f1': {'e0': 0x0100},
                 'f2': {'cred': 3}, 'esc-y': {'rng': 0x1234}}
        self.assertEqual(sorted(cases), sorted(e[0] for e in gk.EVENTS))
        for label, m in cases.items():
            L = _fight({label: m})
            bad = [r[0] for r in gk.judge_all(L, gk.records(L, 'S')) if r[3]]
            self.assertIn(label, bad, label)

    def test_an_unchanged_latch_cannot_show_an_event(self):
        L = _fight({'pause': {'lat': 0x0D}})          # the pause left the Enter's latch
        bad = [r[0] for r in gk.judge_all(L, gk.records(L, 'S')) if r[3]]
        self.assertEqual(bad, ['pause'])
        L = _fight()
        rec = gk.records(L, 'S')
        c = [r for r in gk.judge_all(L, rec) if r[0] == 'enter'][0][1]
        rec[c - 1] = dict(rec[c - 1], lat=0x0D)
        self.assertIn('already', ' '.join(gk.judge(('latched', 0x0D), c, None, rec, 5)))

    def test_a_restart_must_abandon_its_iteration(self):
        L = _fight()
        rec = gk.records(L, 'S')
        c = [r for r in gk.judge_all(L, rec) if r[0] == 'esc-y'][0][1]
        self.assertEqual(gk.judge(('restart',), c, None, rec, 5), [])
        rec[c] = dict(rec[c - 1], f=c)
        self.assertIn('abandons', ' '.join(gk.judge(('restart',), c, None, rec, 5)))

    def test_an_answer_read_a_frame_later_fails(self):
        L = _fight({'esc-n': 'split'})
        bad = [(r[0], r[3]) for r in gk.judge_all(L, gk.records(L, 'S')) if r[3]]
        self.assertEqual([b[0] for b in bad], ['esc-n'])
        self.assertIn('want one', bad[0][1][0])


class TestEffects(unittest.TestCase):
    def _run(self, cap, port, n, sha=None, cmd='effects'):
        with tempfile.TemporaryDirectory() as d:
            os.makedirs(os.path.join(d, 'cap'))
            os.makedirs(os.path.join(d, 'port'))
            if cap is not None:
                with open(os.path.join(d, 'cap', 'poll.log'), 'w') as f:
                    f.write('\n'.join(cap) + '\n')
            with open(os.path.join(d, 'port', 'trace.txt'), 'w') as f:
                f.write('\n'.join(port) + '\n')
            argv = ['gp_keys.py', cmd, '--capture', os.path.join(d, 'cap'), '--port', os.path.join(d, 'port')]
            argv += [] if n is None else ['--min-effects', str(n)]
            argv += [] if sha is None else ['--capture-sha256', sha]
            out, old = io.StringIO(), sys.argv
            sys.argv = argv
            try:
                with redirect_stdout(out):
                    rc = gk.main()
            finally:
                sys.argv = old
            return rc, out.getvalue()

    def test_a_faithful_port_reproduces_every_event(self):
        L = _fight()
        rc, out = self._run(L, _port(L), len(gk.EVENTS))
        self.assertEqual(rc, 0, out)
        self.assertIn('first not reproduced 11, ratchet N 11 ok', out)

    def test_the_ratchet_fails_below_n_and_unpinned(self):
        L = _fight()
        bad = _port(_fight({'alt-s off': {'spz': 1}}))                 # the port misses event 6
        rc, out = self._run(L, bad, 7)
        self.assertEqual(rc, 1)
        self.assertIn('first not reproduced 6 < ratchet N 7', out)
        self.assertEqual(self._run(L, bad, 6)[0], 0)
        self.assertEqual(self._run(L, _port(L), None)[0], 1)            # unpinned
        self.assertEqual(self._run(L, _port(L), 12)[0], 1)              # N past the events

    def test_another_capture_fails_the_pin(self):
        L = _fight()
        rc, out = self._run(L, _port(L), 11, sha='0' * 64)
        self.assertEqual(rc, 1)
        self.assertIn('re-pin', out)
        rc, out = self._run(L, _port(L), 11, sha='')
        self.assertEqual(rc, 1)
        self.assertIn('not pinned', out)
        good = gk.hashlib.sha256(('\n'.join(L) + '\n').encode()).hexdigest()
        self.assertEqual(self._run(L, _port(L), 11, sha=good)[0], 0)

    def test_evidence_cli(self):
        L = _fight()
        self.assertEqual(self._run(L, [], None, cmd='evidence')[0], 0)
        rc, out = self._run(_fight({'f2': {'b1f': 1}}), [], None, cmd='evidence')
        self.assertEqual(rc, 1)
        self.assertIn('10 of 11 events FAIL', out)

    def test_an_absent_capture_skips(self):
        rc, out = self._run(None, [], 11)
        self.assertEqual(rc, 0)
        self.assertIn('skipped', out)


if __name__ == '__main__':
    unittest.main()
