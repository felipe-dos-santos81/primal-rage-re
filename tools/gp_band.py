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
                    PASS-B, PRESENT-LAYER, INCONCLUSIVE), not tolerances.

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
    """The design's four outcomes for one capture frame whose control pixel is
    the yellow sentinel (final truth design §3.4; record §F2.2).

    bb: the S record's 14 back-buffer bytes (the just-presented buffer, 0xE87A0)
      in BAND_APERTURE order; port_idx: the port's own indices at the same 14
      offsets (the U13 replay's frame at f=0x1826); cap_rgb / sentinel_rgb: the
      capture's RGB and the derived sentinel RGB at each offset (None where the
      index is not one of the four injected ones); injected: the caller's checks
      (an S record at f=0x1826 with the diagnostic fields, the six W writes
      unraced, the control's back-buffer byte 0x65).

    PASS-B: a band offset's back buffer is not the port's index -- the game's
      own renderer wrote Q there, a content divergence to name and fix.
    PASS-A: every back-buffer byte matches the port's AND the eight index-0x32
      pixels show their sentinel -- the capture's pixels follow the game's
      current DAC, so the earlier blue is the capture/present model's, not
      content.
    PRESENT-LAYER: the back buffer matches but the sentinel does not show --
      the aperture held a state the present never wrote (the undumped-present
      class; terminal with the S record).
    INCONCLUSIVE: the injection is not proven (no S/fields, a race, the control
      not yellow, or a control whose back-buffer byte is not 0x65)."""
    if not injected:
        return 'INCONCLUSIVE'
    if sentinel_rgb[control] is None or cap_rgb[control] != sentinel_rgb[control]:
        return 'INCONCLUSIVE'
    band = [k for k in range(len(bb)) if k != control]
    if any(sentinel_rgb[k] is None for k in band):
        return 'INCONCLUSIVE'
    if any(bb[k] != port_idx[k] for k in band):
        return 'PASS-B'
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


def _capture_sentinel_frames(cap_dir, offsets, control, wanted):
    """Capture frames whose control pixel is `wanted` (the derived yellow
    sentinel), with their 14 colours."""
    out = []
    names = sorted(n for n in os.listdir(cap_dir) if n.startswith('frame_') and n.endswith('.raw.gz'))
    for n in names:
        with gzip.open(os.path.join(cap_dir, n)) as f:
            rgb = f.read()
        if rgb[3 * (offsets[control][0] * ROW + offsets[control][1]):
               3 * (offsets[control][0] * ROW + offsets[control][1]) + 3] != wanted:
            continue
        out.append((n, capture_colours(rgb, offsets)))
    return out


def dac_main(a):
    with open(a.poll_log) as f:
        lines = f.read().splitlines()
    s = _s_record(lines, a.f)
    if s is None:
        print('no S record at f=%X' % a.f)
        return 0
    missing = [n for n, _, _ in gs.BACKBUFFER_FIELDS + gs.BAND_EFF_FIELDS if n not in s]
    if missing:
        print('the S record at f=%X lacks the band-dac fields (%s); not a gp-u13-band-dac capture'
              % (a.f, ' '.join(missing)))
        return 0
    bb = [s[n] for n, _, _ in gs.BACKBUFFER_FIELDS]
    if a.port_frame:
        port_idx, _port_rgb = port_colours(open(a.port_frame, 'rb').read(), gs.BAND_APERTURE)
    else:
        print('--port-frame is required (the U13 replay frame at f=%X)' % a.f)
        return 2
    sentinel = [gs.BAND_DAC_SENTINEL_RGB.get(i) for i in port_idx]
    ws = _w_rows(lines)
    want = set((addr, bytes(data)) for addr, data in gs.BAND_DAC_WRITES)
    got = set((w['addr'], bytes.fromhex(w['now'])) for w in ws)
    injected = (bool(ws) and len(ws) == len(gs.BAND_DAC_WRITES)
                and not any(w['race'] for w in ws) and got == want
                and s['bbc'] == 0x65 and s['effn'] > 0)
    print('S at f=%X: mode=0x%04X bbp0=%08X bbp1=%08X eff0=%08X eff1=%08X efffree=%08X effn=%d'
          % (a.f, s['mode'], s['bbp0'], s['bbp1'], s['eff0'], s['eff1'], s['efffree'], s['effn']))
    for k, (r, x) in enumerate(gs.BAND_APERTURE):
        tag = 'control' if k == CONTROL else 'band'
        print('  %s (%d,%3d): bb=%02X port_idx=%02X sentinel=%s'
              % (tag, r, x, bb[k], port_idx[k],
                 'none' if sentinel[k] is None else sentinel[k].hex()))
    print('W rows: %d (want %d, raced %d, payloads %s)'
          % (len(ws), len(gs.BAND_DAC_WRITES), sum(1 for w in ws if w['race']),
             'match' if got == want else 'MISMATCH'))
    frames = _capture_sentinel_frames(a.capture, gs.BAND_APERTURE, CONTROL,
                                      gs.BAND_DAC_SENTINEL_RGB[0x65])
    print('capture frames with the yellow control sentinel: %d %s'
          % (len(frames), ' '.join(n for n, _ in frames[:8])))
    verdicts = []
    for n, cap in frames:
        v = decide_dac(bb, port_idx, cap, sentinel, CONTROL, injected)
        verdicts.append(v)
        print('  %s: %s' % (n, ' '.join(c.hex() for c in cap[:CONTROL])))
        print('       control=%s verdict=%s' % (cap[CONTROL].hex(), v))
    if not frames:
        print('verdict: INCONCLUSIVE (no capture frame shows the control sentinel)')
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
    ap.add_argument('--f', type=lambda s: int(s, 0), default=0x1826)
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
    return aperture_main(a)


if __name__ == '__main__':
    sys.exit(main())
