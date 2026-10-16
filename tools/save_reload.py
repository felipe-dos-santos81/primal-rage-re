#!/usr/bin/env python3
"""The save-reload oracle (track J task 7, record 2026-10-15-reverse-j
§J3.6): the two claims over data/k11-captures/save-reload, the capture
tools/k11_capture.py makes from two boots of the pinned original.

  write  the port's replay of boot 1 (the 11-key run that leaves the OPTIONS
         MENU, whose menu_run return calls config_file_save at 0x2CBAF) with
         PR_K11_SAVE set to a scratch path must write a file byte-identical
         (2040 bytes) to the capture's CMOS. It starts from the same
         condition when the capture's session.txt `cmos=` equals the game
         dir's state (J3.6); the scratch file's absence is what makes the
         port read the game dir's copy (host_save_read's fallback).
         It does not cover the other save sites (0x1BEB5, 0x24AA1), DOS's
         filename resolution, the failure paths or any CMOS but this one.
  load   the port booted from a copy of the capture's CMOS (PR_K11_SAVE)
         must hold, at the pinned frame (boot 2's enter_frame: the PR_K11_DUMP
         dump's frame with that master counter) and in every later dumped
         frame, the same five storage ranges of §J3.3 as boot 2's poll.log at
         the pin (the last L/R-located R record at or before the mode-0x27 P
         record). A difference is reported with its frame, range and byte.
         It does not cover the display, the kinds outside the five ranges,
         host_save_read's semantics versus DOS's or anything past the pin.

The capture's poll.log sha256 and frame count are pinned by the Makefile and
re-checked here (another capture fails). Skips without the capture (the
Makefile checks the directory first, like k11-oracle: PR_ORACLE_REQUIRED is
not read). Usage:
  save_reload.py write --capture DIR --scratch FILE [--game-dir DIR] [--cmos-sha256 HEX]
  save_reload.py load  --capture DIR --port DIR --enter-frame N
                       [--poll-sha256 HEX] [--capture-frames N]"""
import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import k11_capture as kc
import k11_session as ks


def sha256(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def first_difference(a, b):
    """The lowest index where a and b differ, or None when equal. A length
    difference is a difference at min(len)."""
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            return i
    return None if len(a) == len(b) else min(len(a), len(b))


def write_claim(scratch, capture_cmos, cmos_sha256=None):
    """(ok, text): the write claim (§J3.6). The capture's CMOS must exist, be
    a valid 2040-byte save and match its pinned sha256; the replay's scratch
    file must then be byte-identical to it."""
    if not os.path.isfile(capture_cmos):
        return False, 'no CMOS at %s' % capture_cmos
    with open(capture_cmos, 'rb') as f:
        want = f.read()
    if cmos_sha256 and hashlib.sha256(want).hexdigest() != cmos_sha256.lower():
        return False, 'CMOS sha256 %s != the pinned %s' % (hashlib.sha256(want).hexdigest(),
                                                          cmos_sha256.lower())
    if not kc.cmos_valid(want):
        return False, 'the capture CMOS is not %d bytes with the XOR last byte (0x1B041)' % kc.CMOS_BYTES
    if not os.path.isfile(scratch):
        return False, 'no file at %s: the replay wrote no save (PR_K11_SAVE set?)' % scratch
    with open(scratch, 'rb') as f:
        got = f.read()
    off = first_difference(got, want)
    if off is None:
        return True, ('the replay wrote %d bytes identical to the capture\'s CMOS (sha256 %s)'
                      % (len(got), hashlib.sha256(want).hexdigest()))
    a = got[off:off + 1]
    b = want[off:off + 1]
    return False, ('the replay first differs from the capture CMOS at offset 0x%X '
                   '(replay %s, capture %s; lengths %d/%d)'
                   % (off, a.hex() or '-', b.hex() or '-', len(got), len(want)))


def session_cmos(capture_dir):
    """The capture's `cmos=` condition from session.txt (J3.2), or None."""
    path = os.path.join(capture_dir, 'session.txt')
    if not os.path.isfile(path):
        return None
    with open(path) as f:
        for line in f:
            for part in line.split():
                if part.startswith('cmos='):
                    return part[5:]
    return None


def ranges_at_pin(recs):
    """{id: bytes} for the five ranges at the pin: the last R record at or
    before the mode-0x27 P record the Enter produced (§J3.6). KeyError names
    the first missing piece."""
    _, p27 = ks.enter_record(recs)
    if p27 is None:
        raise KeyError('no mode-0x27 P record: the Enter was never seen')
    out = {}
    for rid in kc.RANGE_IDS:
        rs = [r for r in recs if r['kind'] == 'R' and r['id'] == rid and r['ms'] <= p27['ms']]
        if not rs:
            raise KeyError('range %s has no R record at or before the pin (ms=%d)' % (rid, p27['ms']))
        out[rid] = rs[-1]['img']
    return out


def parse_ranges_line(line):
    """One <idx> f=<hex4> tick=<hex8> cfg=.. k0=.. hs0=.. hs1=.. hs2=.. line
    of the PR_K11_DUMP driver's ranges.txt."""
    parts = line.split()
    rec = {'idx': int(parts[0]), 'ranges': {}}
    for p in parts[1:]:
        key, _, val = p.partition('=')
        if key in ('f', 'tick'):
            rec[key] = int(val, 16)
        elif key in kc.RANGE_IDS:
            rec['ranges'][key] = bytes.fromhex(val)
    return rec


def load_ranges(path):
    with open(path) as f:
        return [parse_ranges_line(l) for l in f if l.strip()]


def capture_identity(capture_dir, poll_sha256=None, frames=None):
    """(ok, text): boot 2's poll.log sha256 and the frame_*.raw count against
    the Makefile's pins (J3.7(e))."""
    path = os.path.join(capture_dir, 'poll.log')
    if not os.path.isfile(path):
        return False, 'no poll.log at %s' % path
    got = sha256(path)
    if poll_sha256 and got != poll_sha256.lower():
        return False, 'poll.log sha256 %s != the pinned %s' % (got, poll_sha256.lower())
    n = len([f for f in os.listdir(capture_dir) if f.startswith('frame_') and f.endswith('.raw')])
    if frames is not None and n != frames:
        return False, '%d frame_*.raw files != the pinned %d frames' % (n, frames)
    return True, 'poll.log sha256 %s, %d frames' % (got, n)


def load_claim(recs, port_lines, enter_frame):
    """(ok, text): the load claim (§J3.6). The capture must enter mode 0x27 at
    the pinned frame with its ranges settled, and every port dump line from
    that frame on must hold the capture's at-pin bytes; the first difference
    names its line, frame, range and byte."""
    _, p27 = ks.enter_record(recs)
    if p27 is None:
        return False, 'the capture has no mode-0x27 P record after the first K'
    if p27['f'] != enter_frame:
        return False, ('the capture enters mode 0x27 at f=%d, not the pinned %d'
                       % (p27['f'], enter_frame))
    ok, why = kc.ranges_settled(recs)
    if not ok:
        return False, why
    try:
        want = ranges_at_pin(recs)
    except KeyError as e:
        return False, str(e)
    tail = [l for l in port_lines if l['f'] >= enter_frame]
    if not tail or tail[0]['f'] != enter_frame:
        return False, ('the port dump has no frame at the pinned f=%04X (its first is %s)'
                       % (enter_frame, ('f=%04X' % tail[0]['f']) if tail else 'none'))
    for l in tail:
        for rid in kc.RANGE_IDS:
            if rid not in l['ranges']:
                return False, 'the port dump line %d (f=%04X) has no %s' % (l['idx'], l['f'], rid)
            off = first_difference(l['ranges'][rid], want[rid])
            if off is not None:
                a = l['ranges'][rid][off:off + 1]
                b = want[rid][off:off + 1]
                return False, ('the port dump line %d (f=%04X) %s first differs at byte 0x%X '
                               '(port %s, capture %s)' % (l['idx'], l['f'], rid, off,
                                                          a.hex() or '-', b.hex() or '-'))
    return True, ('the port holds the capture\'s five ranges at f=%04X and every later '
                  'dumped frame (%d compared)' % (enter_frame, len(tail)))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    w = sub.add_parser('write', help='the write claim: the boot-1 replay against the capture CMOS')
    w.add_argument('--capture', required=True)
    w.add_argument('--scratch', required=True)
    w.add_argument('--game-dir', default=kc.DEFAULT_GAME_DIR)
    w.add_argument('--cmos-sha256')
    l = sub.add_parser('load', help='the load claim: the boot-2 dump against the capture poll.log')
    l.add_argument('--capture', required=True)
    l.add_argument('--port', required=True)
    l.add_argument('--enter-frame', required=True, type=lambda s: int(s, 0))
    l.add_argument('--poll-sha256')
    l.add_argument('--capture-frames', type=int)
    a = ap.parse_args()
    if a.cmd == 'write':
        state = kc.check_cmos(a.game_dir)               # the capture's condition (J3.6)
        sess = session_cmos(a.capture)
        if sess is None:
            print('save_reload: write: no cmos= in %s/session.txt' % a.capture)
            return 1
        if sess != state:
            print('save_reload: write: the capture started on cmos=%s, the game dir now says %s '
                  '(the data changed since the capture)' % (sess, state))
            return 1
        cmos = kc.cmos_path(a.capture)
        ok, text = write_claim(a.scratch, cmos, a.cmos_sha256)
        print('save_reload: write: %s%s' % ('' if ok else 'FAIL: ', text))
        return 0 if ok else 1
    ok, text = capture_identity(a.capture, a.poll_sha256, a.capture_frames)
    if not ok:
        print('save_reload: load: FAIL: %s' % text)
        return 1
    print('save_reload: load: capture identity: %s' % text)
    with open(os.path.join(a.capture, 'poll.log')) as f:
        recs = [r for r in (ks.parse(x) for x in f.read().splitlines()) if r]
    ranges_path = os.path.join(a.port, 'ranges.txt')
    if not os.path.isfile(ranges_path):
        print('save_reload: load: FAIL: no ranges.txt in %s (the PR_K11_DUMP driver writes it)'
              % a.port)
        return 1
    ok, text = load_claim(recs, load_ranges(ranges_path), a.enter_frame)
    print('save_reload: load: %s%s' % ('' if ok else 'FAIL: ', text))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
