#!/usr/bin/env python3
"""State coverage Task 11 (plan §G7(a), record §D4): apply the 5120 band's
PASS-A / PASS-B / FAIL / INCONCLUSIVE decision to one S record's aperture read.

The diagnostic capture (gp-u13-band) records the 14 aperture bytes (13 band
offsets + the control, gp_session.BAND_APERTURE) on every S record. This tool
reads the S record at a frame f and classifies it against the band's measured
port colours (the port frame at that f, from the gp-u13-ending2 replay dump)
and capture colours (gp-u13-ending2's frame 5120), using gp_compare.decide_aperture.
The thresholds are the named outcomes, not a tolerance: PASS-A (the differing
pixels are not in video memory -> a capture-pipeline artifact), PASS-B (the
aperture holds the difference -> a nameable port writer), FAIL (both
contradicted -> a valid read matching neither), INCONCLUSIVE (the read is not
video memory at all -> invalid, the band stays a named gap). Stdlib only;
read-only against the capture and dump.

Usage:
  gp_band.py --poll-log data/k11-captures/gp-u13-band/poll.log --f 0x1826 \
             --port-frame /tmp/pr_gp_dump/gp-u13-ending2/frame_04316.ipx \
             --capture-frame data/k11-captures/gp-u13-ending2/frame_05120.raw.gz
"""
import argparse
import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_compare as gc
import gp_session as gs


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
    indices = [idx[r * 320 + x] for r, x in offsets]
    rgb = [dac[3 * i:3 * i + 3] for i in indices]
    return indices, rgb


def capture_colours(rgb, offsets):
    """The RGB triple at each offset of a capture frame (320*200*3)."""
    return [rgb[3 * (r * 320 + x):3 * (r * 320 + x) + 3] for r, x in offsets]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--poll-log', required=True)
    ap.add_argument('--f', required=True, type=lambda s: int(s, 0))
    ap.add_argument('--port-frame', required=True)
    ap.add_argument('--capture-frame', required=True)
    a = ap.parse_args()

    with open(a.poll_log) as f:
        ap_bytes = aperture_at(f.read().splitlines(), a.f)
    with open(a.port_frame, 'rb') as f:
        port_bytes = f.read()
    port_indices, port_rgb = port_colours(port_bytes, gs.BAND_APERTURE)
    with gzip.open(a.capture_frame) as f:
        cap_rgb = capture_colours(f.read(), gs.BAND_APERTURE)

    control = len(gs.BAND_APERTURE) - 1
    decision = gc.decide_aperture(ap_bytes, port_bytes[64000:],
                                  port_rgb, cap_rgb, control)

    print('aperture at f=%04X (13 band offsets + control (1,100)):' % a.f)
    for k, (r, x) in enumerate(gs.BAND_APERTURE):
        tag = 'control' if k == control else 'band'
        print('  %s (%d,%d): ap=%02X port_idx=%02X port_rgb=%s capture_rgb=%s'
              % (tag, r, x, ap_bytes[k], port_indices[k], port_rgb[k].hex(),
                 cap_rgb[k].hex()))
    print('decision: %s' % decision)
    return 0


if __name__ == '__main__':
    sys.exit(main())
