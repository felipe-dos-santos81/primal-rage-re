#!/usr/bin/env python3
"""Pixel-exact comparison of port frames against captured original frames.
Absent capture: skip (exit 0) unless PR_ORACLE_REQUIRED=1, then fail.
Usage: smk_compare.py --capture DIR --port DIR [--frames N]"""
import argparse, os, sys

def load_raw(path):
    with open(path, 'rb') as f:
        return f.read()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port', required=True)
    ap.add_argument('--frames', type=int, default=0)
    a = ap.parse_args()
    required = os.environ.get('PR_ORACLE_REQUIRED') == '1'
    if not os.path.isdir(a.capture):
        print(f"smk_compare: no capture at {a.capture} "
              f"({'FAIL (required)' if required else 'skipped'})")
        return 1 if required else 0
    n = a.frames or len([f for f in os.listdir(a.capture) if f.endswith('.raw')])
    bad = 0
    for i in range(n):
        cf = os.path.join(a.capture, 'frame_%04d.raw' % i)
        pf = os.path.join(a.port, 'frame_%04d.raw' % i)
        if not (os.path.exists(cf) and os.path.exists(pf)):
            print(f"frame {i}: missing ({cf} or {pf})"); bad += 1; continue
        c, p = load_raw(cf), load_raw(pf)
        if len(c) != len(p) or c != p:
            d = next((k for k in range(min(len(c), len(p))) if c[k] != p[k]), None)
            print(f"frame {i}: MISMATCH len {len(c)}/{len(p)} first diff at {d}")
            bad += 1
    print(f"smk_compare: {n - bad}/{n} frames match")
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
