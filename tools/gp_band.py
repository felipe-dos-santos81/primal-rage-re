#!/usr/bin/env python3
"""The 5120 band's two read-only experiments (final truth Task 2; record §F2).

  --holder-search   Step 1, no capture: does ANY port present frame hold the
                    capture-5120 band's 13 pixels exactly? The comparison is
                    per pixel (each of the 13 values), per row segment (the
                    four rows' offset sets) and for the whole band, and it is
                    made on the RENDERED RGB (the frame's index byte mapped
                    through its own 768-byte DAC), because the capture stores
                    RGB and a "holder" means the port can produce those values.
                    The control (1,100) is reported apart. The old design
                    experiment only compared port frames 4290..4345 through
                    the top/bottom splice; this is the whole-dump per-pixel
                    version (record §F2.1).

  --dac             Step 2's analysis: apply the back-buffer/sentinel decision
                    (gp_session.BAND_DAC_*) to one S record of the
                    gp-u13-band-dac capture. The capture's W rows, the S record's
                    back-buffer bytes and the capture frames whose control pixel
                    is the derived yellow sentinel are the evidence; the
                    thresholds are the design's named outcomes (PASS-A,
                    PASS-B, PRESENT-LAYER, INCONCLUSIVE), not tolerances. The W
                    rows are filtered to the six BAND_DAC_WRITES addresses (the
                    capture also carries its route's 17 win-path W rows) and the
                    S frame defaults to the diagnostic rows' f + 1 (the
                    iteration that presents the forced body).

  (default)         State coverage Task 11's aperture decision (record §D4),
                    kept for the gp-u13-band capture's S record.

Stdlib only; read-only against the capture and dump.
"""
import argparse
import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_compare as gc
import gp_session as gs

ROW = 320
IPX_BYTES = 64000 + 768
CONTROL = len(gs.BAND_APERTURE) - 1


def aperture_at(poll_lines, f):
    """The 14 aperture bytes of the S record at frame f, in BAND_APERTURE order."""
    for line in poll_lines:
        r = gs.parse(line)
        if r and r['kind'] == 'S' and r['f'] == f:
            return [r[n] for n, _, _ in gs.APERTURE_FIELDS]
    raise SystemExit('no S record at f=%X in the poll log' % f)


def port_colours(frame_bytes, offsets):
    """(indices, rgb) at each offset, from a port .ipx frame (64000 idx + 768 DAC)."""
    idx, dac = frame_bytes[:64000], frame_bytes[64000:]
    indices = [idx[r * ROW + x] for r, x in offsets]
    rgb = [dac[3 * i:3 * i + 3] for i in indices]
    return indices, rgb


def capture_colours(rgb, offsets):
    """The RGB triple at each offset of a capture frame (320*200*3)."""
    return [rgb[3 * (r * ROW + x):3 * (r * ROW + x) + 3] for r, x in offsets]


# ---- the holder search (record §F2.1) ------------------------------------

def load_ipx(path):
    """A port dump frame: 64000 index bytes then the 768-byte DAC."""
    with open(path, 'rb') as f:
        d = f.read()
    if len(d) != IPX_BYTES:
        raise SystemExit('%s is %d bytes, not %d' % (path, len(d), IPX_BYTES))
    return d[:64000], d[64000:]


def rendered_at(idx, dac, offsets):
    """The RGB triple at each offset of one port frame (index -> its DAC entry)."""
    return [dac[3 * idx[r * ROW + x]:3 * idx[r * ROW + x] + 3] for r, x in offsets]


def holder_search(capture_rgb, offsets, frames):
    """Per-pixel / per-row-segment / whole-band holders of `capture_rgb` (one
    RGB triple per offset) among `frames`, an iterable of (name, idx, dac).

    A holder is a frame whose RENDERED RGB equals the capture's at every offset
    of the group. The control is the last offset (gs.BAND_APERTURE's (1,100));
    the row segments are the band offsets grouped by their row, the control
    apart. Returns {pixels, rows, band, band_control, frames}."""
    control = len(offsets) - 1
    band = list(range(control))
    rows = {}
    for k in band:
        rows.setdefault(offsets[k][0], []).append(k)
    out = {'pixels': {k: [] for k in range(len(offsets))},
           'rows': {r: [] for r in rows},
           'control': [],
           'band': [],
           'band_control': [],
           'frames': 0}
    for name, idx, dac in frames:
        out['frames'] += 1
        rgb = rendered_at(idx, dac, offsets)
        for k in range(len(offsets)):
            if rgb[k] == capture_rgb[k]:
                out['pixels'][k].append(name)
        if rgb[control] == capture_rgb[control]:
            out['control'].append(name)
        for r, ks in rows.items():
            if all(rgb[k] == capture_rgb[k] for k in ks):
                out['rows'][r].append(name)
        if all(rgb[k] == capture_rgb[k] for k in band):
            out['band'].append(name)
            if rgb[control] == capture_rgb[control]:
                out['band_control'].append(name)
    return out


def _dump_frames(dump, limit=None):
    names = sorted(n for n in os.listdir(dump) if n.startswith('frame_') and n.endswith('.ipx'))
    if limit is not None:
        names = names[:limit]
    for n in names:
        idx, dac = load_ipx(os.path.join(dump, n))
        yield n, idx, dac


def holder_search_main(a):
    with gzip.open(a.capture_frame) as f:
        cap = capture_colours(f.read(), gs.BAND_APERTURE)
    h = holder_search(cap, gs.BAND_APERTURE, _dump_frames(a.dump, a.limit))
    print('capture frame %s: %d port frames searched' % (a.capture_frame, h['frames']))
    for k, (r, x) in enumerate(gs.BAND_APERTURE):
        tag = 'control' if k == CONTROL else 'band'
        names = h['pixels'][k]
        print('  %s pixel %2d (%d,%3d): capture=%s holders=%d%s'
              % (tag, k, r, x, cap[k].hex(), len(names),
                 '' if not names else ' ' + ' '.join(names[:8]) + (' ...' if len(names) > 8 else '')))
    for r, names in h['rows'].items():
        print('  row %d segment (%d offsets): holders=%d%s'
              % (r, len([k for k in range(CONTROL) if gs.BAND_APERTURE[k][0] == r]), len(names),
                 '' if not names else ' ' + ' '.join(names[:8]) + (' ...' if len(names) > 8 else '')))
    print('  control (%d,%d): holders=%d' % (gs.BAND_APERTURE[CONTROL][0],
                                             gs.BAND_APERTURE[CONTROL][1], len(h['control'])))
    print('  whole band (13): holders=%d %s' % (len(h['band']), ' '.join(h['band'][:8])))
    print('  whole band + control (14): holders=%d %s' % (len(h['band_control']),
                                                          ' '.join(h['band_control'][:8])))
    return 0


# ---- the DAC-injection decision (record §F2.2) ---------------------------

def decide_dac(bb, port_idx, cap_rgb, sentinel_rgb, control, injected):
    """The design's four outcomes for one capture frame that shows an injected
    sentinel at a band offset (final truth design §3.4; record §F2.5).

    bb: the S record's 14 back-buffer bytes (the just-presented buffer, 0xE87A0)
      in BAND_APERTURE order; port_idx: the port's own indices at the same 14
      offsets (the U13 replay's frame at the band frame); cap_rgb /
      sentinel_rgb: the capture's RGB and the derived sentinel RGB at each
      offset (sentinel_rgb[k] is None where the back-buffer index is not one of
      the four injected ones); injected: the caller's checks (an S record at the
      diagnostic W frame + 1 with the fields, the six W writes at one frame,
      unraced and not late, effn > 0) -- the control's index/colour is not part
      of it.

    PASS-B: a band offset's back buffer is not the port's index -- the game's
      own renderer wrote Q there, a content divergence to name and fix. Checked
      FIRST: Q may not be one of the four, so it would have no sentinel and the
      later sentinel test could not see it.
    PASS-A: every back-buffer byte matches the port's AND the eight index-0x32
      pixels show their sentinel -- the capture's pixels follow the game's
      current DAC, so the earlier blue is the capture/present model's, not
      content.
    PRESENT-LAYER: the back buffer matches but the sentinel does not show --
      the aperture held a state the present never wrote (the undumped-present
      class; terminal with the S record).
    INCONCLUSIVE: the injection is not proven (no S/fields, a race/late write),
      a band offset's index is not one of the four (with bb == port_idx there),
      or the control's own index was injected but its pixel did not show the
      sentinel (the DAC did not reach the present). A control whose index is
      NOT one of the four (the measured gp-u13-band-dac control is index 0x03,
      the port's 0x65) cannot turn a sentinel; it is reported, not a gate --
      the band sentinels are the injection proof."""
    if not injected:
        return 'INCONCLUSIVE'
    band = [k for k in range(len(bb)) if k != control]
    if any(bb[k] != port_idx[k] for k in band):
        return 'PASS-B'
    if any(sentinel_rgb[k] is None for k in band):
        return 'INCONCLUSIVE'
    if sentinel_rgb[control] is not None and cap_rgb[control] != sentinel_rgb[control]:
        return 'INCONCLUSIVE'
    eight = [k for k in band if port_idx[k] == 0x32]
    if eight and all(cap_rgb[k] == sentinel_rgb[k] for k in eight):
        return 'PASS-A'
    return 'PRESENT-LAYER'


def _s_record(lines, f):
    for line in lines:
        r = gs.parse(line)
        if r and r['kind'] == 'S' and r['f'] == f:
            return r
    return None


def _w_rows(lines):
    return [r for r in (gs.parse(l) for l in lines) if r and r['kind'] == 'W']


DIAG_W_ADDRS = frozenset(addr for addr, _ in gs.BAND_DAC_WRITES)


def _diag_w_rows(lines):
    """The diagnostic W rows: the six addresses of BAND_DAC_WRITES. The capture
    inherits its route's W rows (gp-u13-band-dac reaches the ending through
    gp-u13-ending2's 17 win-path pokes), so a whole-log count is not the
    diagnostic's; the six addresses are disjoint from every route poke
    (0x108106/0x10789E/0x104B0C)."""
    return [w for w in _w_rows(lines) if w['addr'] in DIAG_W_ADDRS]


def _sentinel_frames(cap_dir, offsets, expected):
    """Capture frames showing at least one injected sentinel (`expected[k]` is
    the sentinel colour of the offset's own back-buffer index, or None where
    that index was not injected). The band offsets are the locator: the
    measured capture's control index is 0x03, not the port's 0x65, so a
    control-only locator would miss the sentinel frame entirely."""
    want = [(k, rgb) for k, rgb in enumerate(expected) if rgb is not None]
    out = []
    names = sorted(n for n in os.listdir(cap_dir) if n.startswith('frame_') and n.endswith('.raw.gz'))
    for n in names:
        with gzip.open(os.path.join(cap_dir, n)) as f:
            rgb = f.read()
        v = capture_colours(rgb, offsets)
        if any(v[k] == s for k, s in want):
            out.append((n, v))
    return out


def dac_main(a):
    with open(a.poll_log) as f:
        lines = f.read().splitlines()
    diag = _diag_w_rows(lines)
    # The S record that carries the sentinel's presented frame is the one after
    # the diagnostic W rows' spin (the poke lands in the spin of f and the
    # iteration f+1 runs the forced case-4 body and presents it). The mode-0x15
    # entry frame is run-dependent by one frame (0x17D4 in gp-u13-ending2,
    # 0x17D5 in gp-u13-band), so the frame is derived, not pinned.
    f = a.f if a.f is not None else (diag[0]['f'] + 1 if diag else None)
    if f is None:
        print('no diagnostic W rows (BAND_DAC_WRITES) in the poll log')
        print('verdict: INCONCLUSIVE')
        return 0
    s = _s_record(lines, f)
    if s is None:
        print('no S record at f=%X (the diagnostic W frame + 1)' % f)
        print('verdict: INCONCLUSIVE')
        return 0
    missing = [n for n, _, _ in gs.BACKBUFFER_FIELDS + gs.BAND_EFF_FIELDS if n not in s]
    if missing:
        print('the S record at f=%X lacks the band-dac fields (%s); not a gp-u13-band-dac capture'
              % (f, ' '.join(missing)))
        print('verdict: INCONCLUSIVE')
        return 0
    bb = [s[n] for n, _, _ in gs.BACKBUFFER_FIELDS]
    if a.port_frame:
        with open(a.port_frame, 'rb') as pf:
            port_idx, _port_rgb = port_colours(pf.read(), gs.BAND_APERTURE)
    else:
        print('--port-frame is required (the U13 replay frame at the band frame)')
        return 2
    # The sentinel a pixel shows is set by ITS OWN back-buffer index (the
    # game's), not the port's; the control's measured index 0x03 is not one of
    # the four, so it has no expected sentinel and is reported, not gated.
    sentinel = [gs.BAND_DAC_SENTINEL_RGB.get(i) for i in bb]
    want = sorted((addr, bytes(data)) for addr, data in gs.BAND_DAC_WRITES)
    got = sorted((w['addr'], bytes.fromhex(w['now'])) for w in diag)
    injected = (len(diag) == len(want) and got == want
                and all(w['f'] == diag[0]['f'] for w in diag)
                and not any(w['race'] or w['late'] for w in diag)
                and s['effn'] > 0)
    print('S at f=%X: mode=0x%04X bbp0=%08X bbp1=%08X eff0=%08X eff1=%08X efffree=%08X effn=%d'
          % (f, s['mode'], s['bbp0'], s['bbp1'], s['eff0'], s['eff1'], s['efffree'], s['effn']))
    for k, (r, x) in enumerate(gs.BAND_APERTURE):
        tag = 'control' if k == CONTROL else 'band'
        print('  %s (%d,%3d): bb=%02X port_idx=%02X sentinel=%s'
              % (tag, r, x, bb[k], port_idx[k],
                 'none' if sentinel[k] is None else sentinel[k].hex()))
    print('W rows: %d diagnostic (of %d total), frame(s) %s; want %d, raced %d, late %d, payloads %s'
          % (len(diag), len(_w_rows(lines)),
             ' '.join('%X' % f for f in sorted(set(w['f'] for w in diag))) or '-',
             len(want), sum(1 for w in diag if w['race']), sum(1 for w in diag if w['late']),
             'match' if got == want else 'MISMATCH'))
    frames = _sentinel_frames(a.capture, gs.BAND_APERTURE, sentinel)
    print('capture frames showing an injected sentinel: %d %s'
          % (len(frames), ' '.join(n for n, _ in frames[:8])))
    verdicts = []
    for n, cap in frames:
        v = decide_dac(bb, port_idx, cap, sentinel, CONTROL, injected)
        verdicts.append(v)
        print('  %s: %s' % (n, ' '.join(c.hex() for c in cap[:CONTROL])))
        print('       control=%s verdict=%s' % (cap[CONTROL].hex(), v))
    if not frames:
        print('verdict: INCONCLUSIVE (no capture frame shows an injected sentinel)')
    elif len(set(verdicts)) == 1:
        print('verdict: %s' % verdicts[0])
    else:
        print('verdict: INCONCLUSIVE (the sentinel frames disagree: %s)'
              % ' '.join(sorted(set(verdicts))))
    return 0


# ---- the Task 11 aperture decision (kept) --------------------------------

def aperture_main(a):
    with open(a.poll_log) as f:
        ap_bytes = aperture_at(f.read().splitlines(), a.f)
    with open(a.port_frame, 'rb') as f:
        port_bytes = f.read()
    port_indices, port_rgb = port_colours(port_bytes, gs.BAND_APERTURE)
    with gzip.open(a.capture_frame) as f:
        cap_rgb = capture_colours(f.read(), gs.BAND_APERTURE)

    decision = gc.decide_aperture(ap_bytes, port_bytes[64000:],
                                  port_rgb, cap_rgb, CONTROL)

    print('aperture at f=%04X (13 band offsets + control (1,100)):' % a.f)
    for k, (r, x) in enumerate(gs.BAND_APERTURE):
        tag = 'control' if k == CONTROL else 'band'
        print('  %s (%d,%d): ap=%02X port_idx=%02X port_rgb=%s capture_rgb=%s'
              % (tag, r, x, ap_bytes[k], port_indices[k], port_rgb[k].hex(),
                 cap_rgb[k].hex()))
    print('decision: %s' % decision)
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--holder-search', action='store_true',
                    help='search a port dump for the capture band\'s exact values')
    ap.add_argument('--dac', action='store_true',
                    help='apply the back-buffer/sentinel decision to a gp-u13-band-dac capture')
    ap.add_argument('--dump')
    ap.add_argument('--limit', type=int)
    ap.add_argument('--poll-log')
    ap.add_argument('--capture', help='the gp-u13-band-dac capture directory')
    ap.add_argument('--f', type=lambda s: int(s, 0), default=None,
                    help='the S record frame; --dac defaults to the diagnostic W rows\' f + 1')
    ap.add_argument('--port-frame')
    ap.add_argument('--capture-frame')
    a = ap.parse_args()

    if a.holder_search:
        if not (a.dump and a.capture_frame):
            ap.error('--holder-search needs --dump and --capture-frame')
        return holder_search_main(a)
    if a.dac:
        if not (a.poll_log and a.capture):
            ap.error('--dac needs --poll-log and --capture')
        return dac_main(a)
    if not (a.poll_log and a.port_frame and a.capture_frame):
        ap.error('the aperture decision needs --poll-log, --port-frame and --capture-frame')
    if a.f is None:
        ap.error('the aperture decision needs --f')
    return aperture_main(a)


if __name__ == '__main__':
    sys.exit(main())
