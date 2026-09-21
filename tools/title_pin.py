#!/usr/bin/env python3
"""Patch a COPY of PRAGE.EXE to pin the RNG draws the title and the demo consume.

0x121A0 draws three values on entry and uses them for the logo's start X, speed
and gravity. Each `call 0x5D7DC` is replaced in place by `mov eax, imm32` holding
the value the port's own LCG (seed 0xABCD) produces for that call's range, so the
port reproduces the same three values by seeding and taking the real draws and the
logo keeps its motion. A fourth site pins the anim stream's opcode-8 handler
(0x2B2A0) to 0, because that handler is the only in-window RNG consumer and its
value would otherwise depend on the master loop's unbounded, host-timed spin.

Two more sites pin the master loop 0x255CC's own draws: the body draw at 0x256B1
(file 0x78505) and the spin draw at 0x256D6 (file 0x7852A), both `rng(0x7FFF)`
(EBP = 0x7FFF at 0x255E4). The spin runs a host-timed, unbounded number of times
per presented frame and each iteration advances the LCG, so the stream position at
state 6 depends on real-time pacing. Pinning both to a non-advancing `mov eax,0`
— and making the port's game_loop stop drawing at the body site, which is the only
one the port modelled (it never had the spin) — leaves both streams carrying only
the consumption-site draws, so the reference's LCG state after the state-6 picks
matches the port's. The state-6 character picks (0x11A8C: draw1 = `rng(7)` at
0x11AAD, draw2 = `rng(6)` at 0x11AE9) are therefore **not** pinned: they are drawn
from the aligned stream and the oracle validates them instead of being forced to
the port's values. All six sites are behaviour pins.

PATCHES entries are `(offset, original_bytes, replacement_bytes)` of equal length
(the length is not fixed). Fails closed: every patch site's original bytes are
verified before anything is written, so a wrong, truncated or already-patched
binary aborts and writes nothing. Refuses to write over the source or anywhere
under the repo's data/.
Usage: title_pin.py --src data/game/C/PRAGE.EXE --out /tmp/pr_title_pin/PRAGE.EXE"""
import argparse, os

# (file offset, original bytes, replacement bytes). Format reference A2. The
# replacement must be the same length as the original (in-place, no size change).
PATCHES = [
    (0x650E9, bytes.fromhex("e842b50400"), bytes.fromhex("b80c000000")),  # 12
    (0x650F5, bytes.fromhex("e836b50400"), bytes.fromhex("b86f000000")),  # 111
    (0x6510B, bytes.fromhex("e820b50400"), bytes.fromhex("b800000000")),  # 0
    (0x7E289, bytes.fromhex("e8a2230300"), bytes.fromhex("b800000000")),  # opcode 8
    (0x78505, bytes.fromhex("e826810300"), bytes.fromhex("b800000000")),  # 0x256B1 master body
    (0x7852A, bytes.fromhex("e801810300"), bytes.fromhex("b800000000")),  # 0x256D6 master spin
]
DATA_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data")

def _fold(path):
    # realpath does not canonicalize case, and APFS is case-insensitive; casefold
    # so Data/ and data/ compare equal.
    return os.path.normcase(path).casefold()

def guard(src, out):
    real_src = os.path.realpath(src)
    real_out = os.path.realpath(out)
    if _fold(real_src) == _fold(real_out):
        raise SystemExit("title_pin: refusing to write over the source: %s" % out)
    real_data = os.path.realpath(DATA_DIR)
    fold_out, fold_data = _fold(real_out), _fold(real_data)
    if fold_out == fold_data or fold_out.startswith(fold_data + os.sep):
        raise SystemExit("title_pin: refusing to write under data/: %s" % out)
    # Filesystem-accurate backstop: the output's directory is the data/ dir.
    if os.path.isdir(real_data) and os.path.isdir(os.path.dirname(real_out)):
        try:
            if os.path.samefile(real_data, os.path.dirname(real_out)):
                raise SystemExit("title_pin: refusing to write under data/: %s" % out)
        except OSError:
            pass

def patch(src, out):
    guard(src, out)
    for off, orig, repl in PATCHES:
        if len(orig) != len(repl):
            raise SystemExit("title_pin: patch table error at 0x%X -- replacement "
                             "length differs, nothing written" % off)
    try:
        with open(src, "rb") as f:
            img = bytearray(f.read())
    except OSError as e:
        raise SystemExit("title_pin: cannot read --src %s: %s" % (src, e))
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
    print("title_pin: wrote %s (pinned: title entry 12, 111, 0 + anim opcode-8 0 + "
          "master-loop draws 0x256B1, 0x256D6 -> 0)" % a.out)

if __name__ == "__main__":
    main()
