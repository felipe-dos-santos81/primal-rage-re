#!/usr/bin/env python3
"""E2: triage of the entry candidates Ghidra never listed (spec 2026-09-30-reverse-completion-design
§4 E2, §6; record docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md).

The universe (record §E2.1; U0 §U0.12's "575 plausible entries"): every address V that
  (a) equals a 4-byte little-endian value at ANY byte offset of the data object [DATA_LO, image end),
      or an immediate or a memory displacement of an instruction of a Ghidra function (a linear
      decode of [entry, entry + size) for every row of port/decomp/prage.functions.csv without '::'
      whose entry is in the code object);
  (b) lies in [CODE_LO, RUNTIME_BASE) and in no Ghidra function's [entry, entry + size);
  (c) follows a `ret` (C3 just before, or C2 three bytes before), directly or after FILLERS
      skipped backwards (at most MAX_FILL bytes), the ret tested before each filler is skipped;
  (d) decodes as one instruction.
U0's own rule (after_ret_u0) skipped the fillers greedily before testing, so the 00 that ends
`ret 4` (C2 04 00) was eaten as a filler: every row says whether U0's rule admits it (§E2.1).
Every candidate gets exactly one class: the first rule of CLASS_ORDER whose evidence exists (§E2.2).

  tools/entry_triage.py --image IMG [--functions CSV] [--src DIR] [--live FILE]
                        [--out TABLE.md | --check TABLE.md] [--expect N] [--expect-u0 N]
"""
import argparse
import bisect
import collections
import csv
import glob
import os
import re
import sys

import diff_emu as de

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODE_LO = 0x10000          # AGENTS.md: code object 0x10000-0x73B14
CODE_HI = 0x73B14
DATA_LO = 0x80000          # AGENTS.md: data object 0x80000-0x10B0CF
RUNTIME_BASE = 0x5D000     # tools/port_progress.py: WATCOM libc and DOS/4GW glue from here up
# The fillers between functions (§E2.1): nop, `mov eax,eax` (E1 record §E.5, 0x19066),
# `lea eax,[eax+0]`, and zero fill (k7-k12 §1.2, 0x475C0). Order matters: 8D 40 00 before 00.
FILLERS = (b"\x90", b"\x8b\xc0", b"\x8d\x40\x00", b"\x00")
MAX_FILL = 16
FINISHER_TABLES = (0xBDAE4, 0xBDB00)          # U0 §U0.4: one dword per character
FINISHER_SLOTS = 7
MOVE_TABLE, MOVE_STRIDE, MOVE_CHARS, MOVE_REACTIONS = 0xA3528, 20, 7, 64   # U0 §U0.12: callback at +0
ANIM_CODE_OPS = (0x10, 0x11, 0x15)            # 0x2B2A0's opcodes that call through DS_00105BD4 (actors.c)
SPAN_BLIT = 0x51E5C                           # sprite.c: the renderer's span blit (§E2.3)
VOICE_FN = 0x2C3FC                            # k7-k12 §0.2: the voice entry every voice site calls
MAX_TABLE = 512

CLASS_ORDER = ("finisher", "move-callback", "span-writer", "call-table", "jump-table", "anim-target",
               "mid-instruction", "data", "code-immediate", "direct", "interior", "data-pointer",
               "dead", "unclassified")
NOT_TARGET = ("mid-instruction", "data", "interior", "dead", "unclassified")
BATCH = {"finisher": "finishers", "move-callback": "callbacks", "span-writer": "span-writers",
         "anim-target": "animation-targets"}
BATCHES = ("callbacks", "finishers", "voice", "animation-targets", "span-writers", "other")


def load_ghidra(path):
    """Sorted (entry, size) of the Ghidra functions (rows without '::', the .image noise)."""
    with open(path) as f:
        return sorted((int(r["entry"], 16), int(r["size"])) for r in csv.DictReader(f)
                      if "::" not in r["entry"])


def load_ported(src):
    """(strict, loose). strict: an fn_register(0xADDR or a `/* 0xADDR` comment at column 0 (§E2.4);
    loose: tools/port_progress.py's rule (a `/* 0xADDR` anywhere on a line, or fn_register)."""
    strict, loose = set(), set()
    for ext in ("c", "h"):
        for f in glob.glob(os.path.join(src, "**", "*." + ext), recursive=True):
            if f.endswith("symbols.h"):
                continue
            with open(f, errors="ignore") as fh:
                t = fh.read()
            reg = {int(m, 16) for m in re.findall(r"fn_register\(\s*0x([0-9A-Fa-f]+)", t)}
            strict |= reg | {int(m, 16) for m in re.findall(r"^/\* 0x([0-9A-Fa-f]{4,6})\b", t, re.M)}
            loose |= reg | {int(m, 16) for m in re.findall(r"/\* 0x([0-9A-Fa-f]{4,6})\b", t)}
    return strict, loose


def load_live(path):
    """The live Ghidra project's functions that differ from the committed export (§E2.8): one line per
    function, `entry,lo-hi[;lo-hi...]` in hex (the body's address ranges, inclusive)."""
    live = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            e, rs = line.split(",")
            live[int(e, 16)] = [tuple(int(x, 16) for x in r.split("-")) for r in rs.split(";")]
    return live


class Triage:
    def __init__(self, image, ghidra, ported_strict=(), ported_ghidra=(), live=None):
        self.img = image
        self.ghidra = list(ghidra)
        self.gstart = [s for s, _ in self.ghidra]
        self.gentries = set(self.gstart)
        self.ported_strict = set(ported_strict)
        self.ported_ghidra = set(ported_ghidra)
        self.live = dict(live or {})
        self._dec = {}
        self._rel32 = None
        self._closure = {}

    # ---- bytes and instructions
    def ins(self, a):
        if a not in self._dec:
            self._dec[a] = de.decode_at(self.img, a)
        return self._dec[a]

    def u8(self, a):
        """The byte at a, or -1 outside the image (never Python's wrap-around index)."""
        return self.img.data[a - self.img.base] if self.img.contains(a) else -1

    def u16(self, a):
        return int.from_bytes(self.img.bytes_at(a, 2), "little")

    def u32(self, a):
        return int.from_bytes(self.img.bytes_at(a, 4), "little")

    def in_ghidra(self, a):
        i = bisect.bisect_right(self.gstart, a) - 1
        return i >= 0 and a < self.ghidra[i][0] + self.ghidra[i][1]

    @staticmethod
    def direct_target(ins):
        if ins is not None and ins.operands and ins.operands[0].type == de.cx86.X86_OP_IMM and (
                ins.group(de.capstone.CS_GRP_JUMP) or ins.group(de.capstone.CS_GRP_CALL)
                or ins.mnemonic in ("loop", "loope", "loopne")):
            return ins.operands[0].imm & 0xFFFFFFFF
        return None

    # ---- the universe (§E2.1)
    def ret_before(self, a):
        return self.u8(a - 1) == 0xC3 or self.u8(a - 3) == 0xC2

    def filler_before(self, a):
        for f in FILLERS:
            if self.img.contains(a - len(f), len(f)) and self.img.bytes_at(a - len(f), len(f)) == f:
                return len(f)
        return 0

    def after_ret(self, v):
        """The ret is tested at v and again after each filler skipped backwards (§E2.1 (c))."""
        a = v
        while v - a <= MAX_FILL:
            if self.ret_before(a):
                return True
            n = self.filler_before(a)
            if not n:
                return False
            a -= n
        return False

    def after_ret_u0(self, v):
        """U0's rule, reconstructed (§E2.1): skip every filler first, then test the ret once."""
        a = v
        while v - a <= MAX_FILL:
            n = self.filler_before(a)
            if not n:
                break
            a -= n
        return self.ret_before(a)

    def plausible(self, v):
        return (CODE_LO <= v < RUNTIME_BASE and not self.in_ghidra(v) and self.after_ret(v)
                and self.ins(v) is not None)

    def candidates(self):
        self.dwords = collections.defaultdict(list)       # value -> data-object addresses (any offset)
        d, base = self.img.data, self.img.base
        for off in range(max(DATA_LO - base, 0), len(d) - 3):
            v = int.from_bytes(d[off:off + 4], "little")
            if CODE_LO <= v < RUNTIME_BASE:
                self.dwords[v].append(base + off)
        operands = set()
        for s, n in self.ghidra:
            if CODE_LO <= s < CODE_HI:
                for ins in de.disasm_range(self.img, s, s + n):
                    for op in ins.operands:
                        if op.type == de.cx86.X86_OP_IMM:
                            operands.add(op.imm & 0xFFFFFFFF)
                        elif op.type == de.cx86.X86_OP_MEM:
                            operands.add(op.mem.disp & 0xFFFFFFFF)
        return sorted(v for v in set(self.dwords) | operands if self.plausible(v))
