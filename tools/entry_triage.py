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
Every candidate gets exactly one class: the first rule of §E2.2 whose evidence exists (`classify` applies
them in that order; CLASS_ORDER is only the order the rendered table lists the classes in).

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
    loose: the address set of tools/port_progress.py's rule before its last step (a `/* 0xADDR` anywhere
    on a line, or fn_register). port_progress then intersects it with the symbols.h `FN_` addresses; the
    caller does that (`loose & {Ghidra entries}`), and the two agree on today's tree (771 ported)."""
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

    # ---- the data tables (§E2.2 rules 1-2)
    def load_tables(self):
        self.fin = collections.defaultdict(list)
        for T in FINISHER_TABLES:
            for i in range(FINISHER_SLOTS):
                if self.u32(T + 4 * i):
                    self.fin[self.u32(T + 4 * i)].append(T + 4 * i)
        self.mcb = collections.defaultdict(list)
        for c in range(MOVE_CHARS):
            for r in range(MOVE_REACTIONS):
                e = MOVE_TABLE + (c * MOVE_REACTIONS + r) * MOVE_STRIDE
                if self.u32(e):
                    self.mcb[self.u32(e)].append((e, c, r))

    # ---- trusted code (§E2.2): Ghidra's functions, then the entries the evidence proves, to a fixpoint
    def entry_like(self, v):
        """An entry outside Ghidra anywhere in the code object (the RUNTIME_BASE cut is the universe's,
        not the trusted code's: span code continues above it, §E2.3)."""
        return (CODE_LO <= v < CODE_HI and not self.in_ghidra(v) and self.after_ret(v)
                and self.ins(v) is not None)

    def index(self):
        self.imm, self.disp, self.rel, self.calls = (collections.defaultdict(list) for _ in range(4))
        self.readers = collections.defaultdict(list)
        for a in sorted(self.trusted):
            ins = self.trusted[a]
            t = self.direct_target(ins)
            if t is not None:
                (self.calls if ins.mnemonic == "call" else self.rel)[t].append(a)
                continue
            for op in ins.operands:
                if op.type == de.cx86.X86_OP_IMM:
                    self.imm[op.imm & 0xFFFFFFFF].append(a)
                elif op.type == de.cx86.X86_OP_MEM:
                    T = op.mem.disp & 0xFFFFFFFF
                    self.disp[T].append(a)
                    if ins.mnemonic in ("call", "jmp") and op.mem.index != 0 and op.mem.scale == 4:
                        self.readers[T].append((ins.mnemonic, a))
        self.slots = collections.defaultdict(list)       # value -> (slot, table, index, kind, reader)
        for T, rs in sorted(self.readers.items()):
            kind, a = min(rs, key=lambda r: r[1])
            p = T
            while (self.img.contains(p, 4) and CODE_LO <= self.u32(p) < CODE_HI and p - T < 4 * MAX_TABLE
                   and (p == T or p not in self.disp)):
                self.slots[self.u32(p)].append((p, T, (p - T) // 4, kind, a))
                p += 4
        span = {T for T, rs in self.readers.items()
                if any(k == "call" and self.owner[a] == SPAN_BLIT for k, a in rs)}
        while True:                                      # a table read inside a span writer is a span table
            wcode = {a for v, ss in self.slots.items() if v in self.bodies and any(s[1] in span for s in ss)
                     for a in self.bodies[v].insns}
            more = {T for T, rs in self.readers.items() if any(k == "call" and a in wcode for k, a in rs)}
            if more <= span:
                break
            span |= more
        self.span_tables = span
        self.tstarts = sorted(self.trusted)

    def switch_targets(self, insns, a):
        """The case targets of the switch `jmp dword ptr [reg*4 + T]` at a, bounded by its own
        `cmp <reg>, N; ja` (N + 1 cases) among the five instructions before it; None without that guard."""
        ins = self.ins(a)
        mem = [op for op in ins.operands if op.type == de.cx86.X86_OP_MEM]
        if ins.mnemonic != "jmp" or not mem or mem[0].mem.index == 0 or mem[0].mem.scale != 4:
            return None
        for b in sorted(x for x in insns if a - 24 <= x < a)[-5:]:
            c = self.ins(b)
            if (c.mnemonic == "cmp" and len(c.operands) == 2 and c.operands[0].type == de.cx86.X86_OP_REG
                    and c.operands[1].type == de.cx86.X86_OP_IMM):
                T = mem[0].mem.disp & 0xFFFFFFFF
                n = (c.operands[1].imm & 0xFF if c.operands[1].size == 1 else c.operands[1].imm) + 1
                if not self.img.contains(T, 4 * n):
                    return None
                return [self.u32(T + 4 * k) for k in range(n)]
        return None

    def scan(self, e):
        """static_scan from e, extended through every bounded switch (§E2.2): the body P would port."""
        if e in self.bodies:
            return self.bodies[e]
        info = de.static_scan(self.img, e)
        insns, leaders, indirect = dict(info.insns), set(info.leaders), []
        truncated, unresolved = info.truncated, list(info.unresolved)
        work, seen = list(info.indirect), set()
        while work:
            a = work.pop()
            if a in seen:
                continue
            seen.add(a)
            targets = self.switch_targets(insns, a)
            if targets is None:
                indirect.append(a)
                continue
            for t in targets:
                sub = de.static_scan(self.img, t)
                for x, n in sub.insns.items():
                    insns.setdefault(x, n)
                leaders |= set(sub.leaders)
                truncated = truncated or sub.truncated
                unresolved += sub.unresolved
                work += sub.indirect
        self.bodies[e] = de.StaticInfo(sorted(leaders), insns, sorted(set(indirect)), sorted(set(unresolved)),
                                       truncated)
        return self.bodies[e]

    def trust(self, e):
        for a in sorted(self.scan(e).insns):
            if a not in self.trusted:
                self.trusted[a] = self.ins(a)
                self.owner[a] = e

    def build(self, cands):
        self.cands = cands
        self.cset = set(cands)
        self.trusted, self.owner, self.bodies = {}, {}, {}
        for s, n in self.ghidra:
            if CODE_LO <= s < CODE_HI:
                for ins in de.disasm_range(self.img, s, s + n):
                    self.trusted[ins.address] = ins
                    self.owner[ins.address] = s
        self.entries = {}                                # trusted non-Ghidra entry -> how it is reached
        while True:
            self.index()
            pool = (self.cset | set(self.slots) | set(self.calls) | set(self.imm)) - set(self.entries)
            added = {}
            for v in sorted(pool):
                why = self.reason(v)
                if why:
                    added[v] = why
            if not added:
                break
            self.entries.update(added)
            for e in sorted(added):
                self.trust(e)
        # a table walk can shrink as more code is trusted (a newly trusted reader ends it): an entry
        # whose evidence the final index no longer holds is stale and is reported, never dropped
        self.stale = sorted(e for e in self.entries if self.reason(e) is None)
        for v in cands:
            self.scan(v)

    def reason(self, v):
        """Why v is a trusted entry, from the current index (§E2.2), or None."""
        if v in self.cset:
            hit = self.table_class(v)
            if hit:
                return hit[1]
        elif v in self.slots and self.entry_like(v):
            p, T, k, kind, a = self.best_slot(v)
            if v < RUNTIME_BASE or any(s[1] in self.span_tables for s in self.slots[v]):
                return "slot %X of table %X, read by `%s` at %X" % (p, T, kind, a)
        if v in self.calls and CODE_LO <= v < RUNTIME_BASE and not self.in_ghidra(v):
            return "called at %X" % self.calls[v][0]
        if v in self.imm and self.plausible(v):
            return "immediate at %X" % self.imm[v][0]
        return None

    # ---- the classes (§E2.2), first match wins
    def anim_word(self, p):
        """The opcode word that makes the dword at p an animation code pointer, or None (§E2.2 rule 6)."""
        for at, prefix in ((p - 2, False), (p - 4, True)):
            if not self.img.contains(at, 2):
                continue
            w = self.u16(at)
            if not (w & 0x8000) or (w & 0x6000) != 0x4000:
                continue
            op = (w >> 8) & 0x1F
            if not prefix and op in ANIM_CODE_OPS:
                return at, w
            if prefix and op == 0x1F and (w & 0xFF) in ANIM_CODE_OPS:
                return at, w
        return None

    def best_slot(self, v):
        """The slot of v that names its class (§E2.2 rules 3-5): any span table first, then a table read
        by a `call`, then one read by a `jmp`; the first found among equals."""
        return min(self.slots[v], key=lambda s: 0 if s[1] in self.span_tables else 1 if s[3] == "call" else 2)

    def table_class(self, v):
        if v in self.fin:
            return "finisher", "dword %s" % " ".join("%X" % p for p in self.fin[v])
        if v in self.mcb:
            return "move-callback", " ".join("dword %X (char %d, reaction 0x%02X)" % x for x in self.mcb[v])
        if v in self.slots:
            p, T, k, kind, a = self.best_slot(v)
            c = "span-writer" if T in self.span_tables else ("call-table" if kind == "call" else "jump-table")
            return c, "dword %X = table %X[%d], read by `%s` at %X" % (p, T, k, kind, a)
        for p in self.dwords.get(v, ()):
            w = self.anim_word(p)
            if w:
                return "anim-target", "dword %X after the opcode word %04X at %X" % (p, w[1], w[0])
        return None

    def covering(self, v):
        """The trusted instruction that holds v strictly inside it, or None."""
        i = bisect.bisect_right(self.tstarts, v) - 1
        while i >= 0 and self.tstarts[i] > v - 16:
            a = self.tstarts[i]
            if a < v < a + self.trusted[a].size:
                return a
            i -= 1
        return None

    def starts_with_fill(self, v):
        """True when v begins with a filler (a zero filler needs two zero bytes: `add [eax],al`)."""
        return any(self.img.bytes_at(v, len(f)) == f for f in FILLERS[:-1]) or self.img.bytes_at(v, 2) == b"\x00\x00"

    def rel32_anywhere(self, v):
        """Byte level: every E8/E9/0F 8x rel32 at any offset below RUNTIME_BASE that lands on v."""
        if self._rel32 is None:
            self._rel32 = collections.defaultdict(list)
            d, base = self.img.data, self.img.base
            for o in range(0, min(RUNTIME_BASE - base - 5, len(d) - 6)):
                b = d[o]
                if b in (0xE8, 0xE9):
                    t = base + o + 5 + int.from_bytes(d[o + 1:o + 5], "little", signed=True)
                elif b == 0x0F and 0x80 <= d[o + 1] <= 0x8F:
                    t = base + o + 6 + int.from_bytes(d[o + 2:o + 6], "little", signed=True)
                else:
                    continue
                self._rel32[t].append(base + o)
        return self._rel32.get(v, [])

    def untrusted(self):
        """Code outside the trusted set that only untrusted bytes reach (§E2.5): every rel32 target X
        below RUNTIME_BASE that is a plausible entry, not trusted, and whose rel32 sites are all outside
        the trusted instructions -> those sites; and self.uowner, each instruction of their scans -> the
        first such X (in address order)."""
        if not hasattr(self, "uentries"):
            self.rel32_anywhere(0)
            self.uentries = {t: s for t, s in sorted(self._rel32.items())
                             if t not in self.trusted and self.plausible(t)
                             and not any(a in self.trusted for a in s)}
            self.uowner = {}
            for x in self.uentries:
                for a in sorted(de.static_scan(self.img, x).insns):
                    self.uowner.setdefault(a, x)
        return self.uentries

    def classify(self, v):
        hit = self.table_class(v)
        if hit:
            return hit
        a = self.covering(v)
        if a is not None:
            return "mid-instruction", "inside the instruction at %X (code of %X)" % (a, self.owner[a])
        if v in self.disp:
            return "data", "memory operand of the instruction at %X" % self.disp[v][0]
        if self.starts_with_fill(v):
            return "data", "filler at %X (%s)" % (v, self.img.bytes_at(v, 3).hex())
        if self.bodies[v].truncated:
            return "data", "undecodable bytes on the scan from %X" % v
        if v in self.imm:
            return "code-immediate", "immediate of the instruction at %X" % self.imm[v][0]
        if v in self.calls:
            return "direct", "`call` at %X" % self.calls[v][0]
        if v in self.trusted and self.owner[v] != v:
            return "interior", "an instruction of the code of %X" % self.owner[v]
        if v in self.rel:
            a = self.rel[v][0]
            return "interior", "`%s` at %X in the code of %X" % (self.trusted[a].mnemonic, a, self.owner[a])
        aligned = [p for p in self.dwords.get(v, ()) if p % 4 == 0]
        if aligned:
            return "data-pointer", "aligned dword %s" % " ".join("%X" % p for p in aligned[:4])
        self.untrusted()
        x = self.uowner.get(v)
        if x is not None and x != v:
            return "interior", "an instruction of the scan from %X, which only the rel32 at %s (untrusted bytes) reaches" % (
                x, " ".join("%X" % s for s in self.uentries[x][:4]))
        refs = " ".join("%X" % p for p in self.dwords.get(v, ())[:4])
        far = self.rel32_anywhere(v)
        if not far:
            return "dead", "no rel32 at any code offset, no immediate, no aligned dword; unaligned dwords %s" % refs
        return "unclassified", "rel32 at %s outside the trusted code; unaligned dwords %s" % (
            " ".join("%X" % x for x in far[:4]), refs)

    # ---- per-row facts (§E2.4-§E2.8)
    def is_ported(self, a):
        """A Ghidra function by port_progress's rule; any other address by the strict rule (§E2.4)."""
        return a in self.ported_ghidra if a in self.gentries else a in self.ported_strict

    def live_status(self, v):
        """Evidence only, never a class (§E2.8): `entry` when the live project has a function at v,
        `body of X` when v is inside the body of the live function X, else `-`."""
        if v in self.live:
            return "entry"
        for e in sorted(self.live):
            if any(lo <= v <= hi for lo, hi in self.live[e]):
                return "body of %X" % e
        return "-"

    def next_boundary(self, v):
        """The first Ghidra entry or candidate above v, else RUNTIME_BASE: the row's size bound."""
        nxt = [RUNTIME_BASE]
        i = bisect.bisect_right(self.gstart, v)
        if i < len(self.gstart):
            nxt.append(self.gstart[i])
        j = bisect.bisect_right(self.cands, v)
        if j < len(self.cands):
            nxt.append(self.cands[j])
        return min(nxt)

    def body_facts(self, info):
        calls, voice, retn, unmodeled = set(), [], False, set()
        for a in sorted(info.insns):
            ins = self.ins(a)
            t = self.direct_target(ins)
            if ins.mnemonic in ("call", "jmp") and t == VOICE_FN:
                voice.append(a)
            if ins.mnemonic == "call" and t is not None:
                calls.add(t)
            if ins.mnemonic == "ret" and ins.operands:
                retn = True
            if ins.mnemonic in de.UNMODELED_MNEMONICS:
                unmodeled.add(ins.mnemonic)
        return calls, voice, retn, sorted(unmodeled)

    def closure(self, e):
        """The direct-call tree under e, breadth first in address order: (callees, blocker). The blocker
        names the nearest thing in the tree the emulator cannot run unaided (§E2.6): an unmodeled
        instruction, an indirect call or jump that is not a bounded switch, or a truncated scan, with
        the function that holds it."""
        if e in self._closure:
            return self._closure[e]
        seen, queue, blocker = {e}, collections.deque([e]), None
        while queue:
            f = queue.popleft()
            info = self.scan(f)
            calls, _, _, unmodeled = self.body_facts(info)
            if blocker is None:
                if unmodeled:
                    blocker = "%s in %X" % (unmodeled[0], f)
                elif info.indirect:
                    blocker = "indirect at %X in %X" % (info.indirect[0], f)
                elif info.truncated:
                    blocker = "truncated scan in %X" % f
            for t in sorted(calls):
                if CODE_LO <= t < CODE_HI and t not in seen:
                    seen.add(t)
                    queue.append(t)
        seen.discard(e)
        self._closure[e] = (seen, blocker)
        return self._closure[e]

    def readiness(self, e):
        """What E1 can do with the entry today (§E2.6), first match:
        stack-args  the entry returns with `ret N` (E1 has no stack-argument binding, E1 §E.6.3);
        stubs       something in its call tree cannot run in the emulator (named);
        callees     every callee in its tree runs, but one below RUNTIME_BASE is not ported yet (port it
                    first, or stub it);
        allow-list  every callee in its tree is ported: E1's allow-list runs it today;
        leaf        no call at all."""
        _, _, retn, _ = self.body_facts(self.scan(e))
        if retn:
            return "stack-args"
        tree, blocker = self.closure(e)
        if blocker:
            return "stubs (%s)" % blocker
        unported = sorted(t for t in tree if t < RUNTIME_BASE and not self.is_ported(t))
        if unported:
            return "callees (%s)" % " ".join("%X" % t for t in unported[:4])
        return "allow-list" if tree else "leaf"

    def run(self):
        cands = self.candidates()
        self.load_tables()
        self.build(cands)
        rows = []
        for v in cands:
            c, ev = self.classify(v)
            target = c not in NOT_TARGET
            voice = self.body_facts(self.bodies[v])[1] if target else []
            batch = "-" if not target else BATCH.get(c) or ("voice" if voice else "other")
            rows.append(dict(addr=v, size=self.next_boundary(v) - v, cls=c, evidence=ev,
                             u0=self.after_ret_u0(v), live=self.live_status(v), ported=self.is_ported(v),
                             batch=batch, e1=self.readiness(v) if target else "-", voice=voice))
        return rows

    def supplement(self):
        """Trusted entries outside the universe and outside Ghidra (§E2.5): helpers and stored callbacks."""
        out = []
        for e in sorted(set(self.entries) - self.cset):
            voice = self.body_facts(self.scan(e))[1]
            out.append(dict(addr=e, why=self.reason(e) or "stale: " + self.entries[e], live=self.live_status(e),
                            ported=self.is_ported(e),
                            e1=self.readiness(e), voice=voice))
        return out

    def voice_placement(self, rows, supp):
        """Every rel32 call/jmp to VOICE_FN outside the Ghidra functions, placed in the body of a target
        row, a supplement entry or an untrusted entry, or nowhere (§E2.7)."""
        self.untrusted()
        owners = ([(r["addr"], "row") for r in rows if r["batch"] != "-"]
                  + [(s["addr"], "supplement") for s in supp]
                  + [(x, "untrusted") for x in self.uentries])
        out = []
        for s in self.rel32_anywhere(VOICE_FN):
            if self.in_ghidra(s):
                continue
            where = None
            for e, kind in owners:
                body = self.scan(e) if kind != "untrusted" else de.static_scan(self.img, e)
                if s in body.insns:
                    where = (e, kind)
                    break
            out.append(dict(site=s, entry=where[0] if where else None, kind=where[1] if where else "-",
                            ported=self.is_ported(where[0]) if where else False))
        return out
