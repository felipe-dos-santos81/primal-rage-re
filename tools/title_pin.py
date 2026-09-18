#!/usr/bin/env python3
"""Patch a COPY of PRAGE.EXE so FUN_0005d7dc returns a constant 0.

The title's logo start X/speed/direction come from three RNG draws whose value
depends on how many times the master loop's spin called the RNG -- a timing
quantity. Constant-returning the RNG makes the composite independent of that
count, which makes the capture reproducible (the spec's oracle gate).

Fails closed: the original bytes at the patch site are verified before writing,
so a wrong, truncated or already-patched binary aborts and writes nothing.
Never writes under data/.
Usage: title_pin.py --src data/game/C/PRAGE.EXE --out /tmp/pin/PRAGE.EXE"""
import argparse, os

# FUN_0005d7dc: push ebx; push edx; and eax,0xffff; ...; pop edx; pop ebx; ret.
# 42 bytes (Ghidra size), file 0xB0630..0xB0659: obj0 code maps file = va +
# 0x52E54 (verified against neighbours FUN_0005d808 @ +0x108.. and FUN_0005d812:
# file 0xB065C == 0x5D808 + 0x52E54). Patching the entry with the 3-byte stub
# replaces push ebx/push edx/and eax, so the ret is stack-balanced.
SIG = bytes.fromhex("535225ffff00008bd8a1d8f60600")
STUB = bytes.fromhex("31c0c3")
PATCH_OFF = 0xB0630

# Repo root from __file__, so the data/ guard holds whatever the caller's CWD is.
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA_DIR = os.path.join(REPO_ROOT, "data")

def guard(src, out):
    if os.path.realpath(out) == os.path.realpath(src):
        raise SystemExit("title_pin: refusing --out %s: resolves to --src, "
                         "nothing written" % out)
    data = os.path.realpath(DATA_DIR)
    real = os.path.realpath(out)
    if real == data or real.startswith(data + os.sep):
        raise SystemExit("title_pin: refusing --out %s: resolves under %s, "
                         "nothing written" % (out, DATA_DIR))

def patch(src, out):
    guard(src, out)
    with open(src, "rb") as f:
        img = bytearray(f.read())
    if img[PATCH_OFF:PATCH_OFF + len(SIG)] != SIG:
        raise SystemExit("title_pin: signature mismatch at 0x%X -- wrong or "
                         "already-patched binary, nothing written" % PATCH_OFF)
    img[PATCH_OFF:PATCH_OFF + 3] = STUB
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
    print("title_pin: wrote %s (0x5D7DC entry -> xor eax,eax; ret)" % a.out)

if __name__ == "__main__":
    main()
