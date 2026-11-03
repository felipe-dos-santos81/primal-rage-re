#!/usr/bin/env python3
"""Completion truth Task 2 (R28): the k11-lang capture's language-reload state.

  k11_lang.py check --capture data/k11-captures/k11-lang [--exe data/game/C/PRAGE.EXE]

The reloaded *screen* is the K11 oracle's claim (`k11_compare.py`: 17/17 settled
screens exhibited, 0 unexplained, 201/201 non-black port frames). This checker
adds the capture-side state the screen follows from:

* field 0x29 (the packed CONFIG OPTIONS bits; the language is bits 24..26, the
  record-5 entry of the table at 0xA2EB4, shift 24, six values) decodes from the
  capture's `F` images as **0 before the edit and 1 after it**, and the changed
  value is exactly `old + (1 << 24)`: the Right adds one to the language nibble
  and changes no other bit;
* the change is first seen at/after the 15th key (the Enter that returns the
  editor's bits through 0x33668) and before the 16th (the Esc that leaves the
  OPTIONS MENU), so the capture itself shows the row edited;
* the 17 keys are the scenario's and there is no `W` record (real keys, no
  poke); mode 0x27 follows the first Enter (the session check repeats this).

The descriptors are the raw's bitfield table at code VA 0x2D300, read by
`tools/k11_fields.py`; the value pins are the shipped config's default field
(0x142095, language 0) and the one added nibble, not a fitted constant.
Stdlib only.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import k11_fields as kf
import k11_session as ks

SCENARIO = 'k11-lang'
FIELD = 0x29
LANG_SHIFT = 24
LANG_MASK = 0x7000000


def parse(lines):
    """(keys, frames, mode27, pokes): keys [(ms, word)], frames [(ms, image)],
    mode27 [ms the mode was 0x27 at], pokes count."""
    keys, frames, modes, pokes = [], [], [], 0
    for line in lines:
        parts = line.split()
        if not parts:
            continue
        rec = dict(p.split('=', 1) for p in parts[1:] if '=' in p)
        if parts[0] == 'K':
            keys.append((int(rec['ms']), int(rec['key'], 16)))
        elif parts[0] == 'F':
            frames.append((int(rec['ms']), bytes.fromhex(rec['img'])))
        elif parts[0] == 'P' and int(rec.get('mode', '0'), 16) == 0x27:
            modes.append(int(rec['ms']))
        elif parts[0] == 'W':
            pokes += 1
    return keys, frames, modes, pokes


def check(lines, descs, out=print):
    keys, frames, modes, pokes = parse(lines)
    wanted = [ks.key_word(k) for k in ks.keys_for(SCENARIO)]
    problems = []

    got = [ks.normalise(w) for _, w in keys]
    if got != wanted:
        problems.append('keys %s differ from the scenario %s'
                        % (['%04X' % w for w in got], ['%04X' % w for w in wanted]))
    if pokes:
        problems.append('%d W record(s): the capture must use real keys' % pokes)
    if not modes or not keys or modes[0] < keys[0][0]:
        problems.append('mode 0x27 is not observed after the first Enter')

    # The Enter that returns the editor's bits (key index 14, the 15th key) and
    # the Esc that leaves the OPTIONS MENU (index 15) bound the field write.
    if len(keys) != len(wanted):
        problems.append('cannot bound the field write: %d keys' % len(keys))
        enter_ms = esc_ms = None
    else:
        enter_ms, esc_ms = keys[14][0], keys[15][0]

    vals = [(ms, kf.get(img, descs, FIELD)) for ms, img in frames]
    if not vals:
        problems.append('no F record: the field image never changed')
        return 1 if problems else 0
    langs = [(ms, v, (v & LANG_MASK) >> LANG_SHIFT) for ms, v in vals]
    before = [x for x in langs if x[2] == 0]
    after = [x for x in langs if x[2] == 1]
    if not before or not after:
        problems.append('the language nibble is not seen both 0 and 1 (%s)'
                        % ', '.join('%d@%dms' % (x[2], x[0]) for x in langs))
        detail = ''
    else:
        first_after = after[0]
        last_before = before[-1][1]
        if first_after[1] != last_before + (1 << LANG_SHIFT):
            problems.append('field 0x29 %08X -> %08X is not the one-nibble edit'
                            % (last_before, first_after[1]))
        if enter_ms is None or not (enter_ms <= first_after[0] < esc_ms):
            problems.append('the language change at %d ms is not between the '
                            'Enter (%s ms) and the Esc (%s ms)'
                            % (first_after[0], enter_ms, esc_ms))
        if any(x[2] != 1 for x in langs[len(before):]):
            problems.append('the language nibble does not stay 1 to the end')
        detail = ('field 0x29 %08X (language 0) -> %08X (language 1) at %d ms, '
                  'between key 14 (Enter, %s ms) and key 15 (Esc, %s ms)'
                  % (last_before, first_after[1], first_after[0], enter_ms, esc_ms))

    if problems:
        for p in problems:
            out('k11_lang: %s: evidence: FAIL: %s' % (SCENARIO, p))
        return 1
    out('k11_lang: %s: %d keys, no poke, mode 0x27 after the Enter' % (SCENARIO, len(keys)))
    out('k11_lang: %s: %s' % (SCENARIO, detail))
    out('k11_lang: %s: ok' % SCENARIO)
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check',))
    ap.add_argument('--capture', required=True)
    ap.add_argument('--exe', default=os.path.join(os.environ.get('PR_GAME_DIR', os.path.join('data', 'game', 'C')),
                                                  'PRAGE.EXE'),
                    help='the raw whose 0x2D300 descriptor table is read (default $PR_GAME_DIR or data/game/C)')
    a = ap.parse_args()
    poll = os.path.join(a.capture, 'poll.log')
    if not os.path.isfile(poll):
        print('k11_lang: %s: no poll.log (skipped)' % a.capture)
        return 0
    if not os.path.isfile(a.exe):
        print('k11_lang: %s missing (the 0x2D300 descriptor table)' % a.exe)
        return 2
    descs = kf.load_descriptors(a.exe)
    with open(poll) as f:
        return check(f.read().splitlines(), descs)


if __name__ == '__main__':
    sys.exit(main())
