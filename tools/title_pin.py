#!/usr/bin/env python3
"""Patch a COPY of PRAGE.EXE to pin the title's three RNG draws.

0x121A0 draws three values on entry and uses them for the logo's start X, speed
and gravity. Each `call 0x5D7DC` is replaced in place by `mov eax, imm32` holding
the value the port's own LCG (seed 0xABCD) produces for that call's range, so the
port reproduces the same three values by seeding and taking the real draws and the
logo keeps its motion. The master loop's spin draws are left alone: their values
are discarded and no longer influence the composite.

Fails closed: every patch site's original bytes are verified before anything is
written, so a wrong, truncated or already-patched binary aborts and writes nothing.
Refuses to write over the source or anywhere under the repo's data/.
Usage: title_pin.py --src data/game/C/PRAGE.EXE --out /tmp/pr_title_pin/PRAGE.EXE"""
import argparse, os

# (file offset, original 5 bytes, replacement). Format reference A2.
PATCHES = [
    (0x650E9, bytes.fromhex("e842b50400"), bytes.fromhex("b80c000000")),  # 12
    (0x650F5, bytes.fromhex("e836b50400"), bytes.fromhex("b86f000000")),  # 111
    (0x6510B, bytes.fromhex("e820b50400"), bytes.fromhex("b800000000")),  # 0
]
DATA_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data")

def guard(src, out):
    real_out = os.path.realpath(out)
    if real_out == os.path.realpath(src):
        raise SystemExit("title_pin: refusing to write over the source: %s" % out)
    real_data = os.path.realpath(DATA_DIR)
    if real_out == real_data or real_out.startswith(real_data + os.sep):
        raise SystemExit("title_pin: refusing to write under data/: %s" % out)

def patch(src, out):
    guard(src, out)
    with open(src, "rb") as f:
        img = bytearray(f.read())
    for off, orig, _repl in PATCHES:
        if img[off:off + len(orig)] != orig:
            raise SystemExit("title_pin: site mismatch at 0x%X -- wrong or "
                             "already-patched binary, nothing written" % off)
    for off, _orig, repl in PATCHES:
        img[off:off + len(repl)] = repl
    d = os.path.dirname(os.path.abspath(out))
    os.makedirs(d, exist_ok=True)
    tmp = out + ".part"
    with open(tmp, "wb") as f:
        f.write(img)
    os.replace(tmp, out)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    patch(a.src, a.out)
    print("title_pin: wrote %s (3 title draws -> 12, 111, 0)" % a.out)

if __name__ == "__main__":
    main()
