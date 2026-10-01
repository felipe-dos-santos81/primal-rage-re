# Reverse completion E2: triage of the non-Ghidra entry points (record)

Spec: `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track E2, §6 ("the 575 split into
a counted list with evidence per row"). Plan: `2026-10-01-reverse-e2-entry-triage.md`. Inputs: U0's record
(`2026-09-30-gameplay-u0-derivations.md` §U0.12, "a dword/immediate scan finds 575 plausible entries (after
`ret`/padding) in no Ghidra function"), E1's harness (`tools/diff_emu.py`, record
`2026-09-30-reverse-completion-e1-derivations.md` §E.6), the committed Ghidra export
`port/decomp/prage.functions.csv` (1352 rows: 1207 real + 145 `.image`), and `port/src` at `e9271df`.

**Status of the numbers.** Every figure in §E2.1-§E2.9 was measured in the planning scratch run
(2026-10-01, the code the plan's Tasks 2-5 contain, run against the image below). They are
**preliminary: re-measured in the plan's Task 5** and pinned there by `RealImageTests`; Task 7 replaces
this paragraph with the Task 5 figures, or records each difference and its cause.

**The image.** `build/diffrun --exe data/game/C/PRAGE.EXE --image-out FILE` at `e9271df`: 1 028 304 bytes
from `0x10000` (to `0x10B0D0`, the data object's end), sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`
(the same bytes from two separate dumps). It is the LE image with the fixups applied, so every
displacement below is a runtime address (AGENTS.md: raw-file displacements are pre-fixup).

## §E2.1 The universe: U0's 575 reproduced, and the defect in its rule

U0's scan code is not in the repo. Its description ("a dword/immediate scan ... after `ret`/padding ...
in no Ghidra function") leaves five choices open. A grid of 512 variants (scratch `grid.py`) crossed:
dwords at every byte offset or only 4-aligned ones; dwords from the whole image or from the data object
only; with or without Ghidra-instruction operands; four filler sets; `ret` only or `ret`/`jmp` before the
filler; the upper bound `0x5D000` or `0x73B14`; with or without "decodes as one instruction". **Exactly
one choice of those (two grid rows, which differ only in whether the `.image` noise rows count as Ghidra
functions, a choice that changes nothing) gives 575:**

- (a) the value of a 4-byte little-endian word at **any byte offset of the data object**
  `[0x80000, 0x10B0D0)`, **or** an immediate or memory displacement of an instruction of a Ghidra function
  (a linear decode of `[entry, entry + size)` for every real row of the export in the code object);
- (b) in `[0x10000, 0x5D000)` (`tools/port_progress.py`'s `RUNTIME_BASE`) and in no Ghidra function's
  `[entry, entry + size)`;
- (c) a `ret` right before it (`C3`, or `C2 iw` three bytes before) once the fillers are skipped
  backwards, at most 16 bytes: `90` (nop), `8B C0` (`mov eax,eax`, E1 §E.5's `0x19066`), `8D 40 00`
  (`lea eax,[eax+0]`) and `00` (k7-k12 §1.2's zero fill at `0x475C0`). The planning grid's wider filler
  set (adding `8D 76 00`, `8D 74 26 00`, `8D 80 00000000`, `89 C0`, `CC`) gives the same 575 (scratch
  `chk.py`: the symmetric difference is empty);
- (d) its first instruction decodes.

So the reconstruction reproduces U0's count exactly, and the set holds every address U0 names: its 27
unported callbacks, its six unported finisher entries, its animation examples `0x241A8`, `0x37DCC`,
`0x400E0`, and its span-writer ends `0x521DC` and `0x5CF00` (scratch `chk.py`).

**The defect.** U0's rule skips every filler first and tests the `ret` once (`after_ret_u0`). The `00`
that ends `ret 4` (`C2 04 00`) is then eaten as a zero filler and the `C2` is never seen, so **an entry
right after `ret N` (N < 256) is missed.** The corrected rule (`after_ret`) tests the `ret` at V and again
after each filler. It admits exactly four more addresses, so the plan standardises on **579** and every
row carries a `u0` column (575 say yes):

| addr | bytes before | what it is | class (§E2.2) |
|---|---|---|---|
| `0x10602` | `ret 4` at `0x105FF` | the filler `mov eax,eax` before `0x10604` | data (filler) |
| `0x10604` | `ret 4`, `mov eax,eax` | `mov eax,[0x81E10]; inc dword [0x81E10]; ret` (12 bytes), stored by the immediate at `0x10647` | code-immediate, unported |
| `0x36114` | `ret 4` at `0x36111` | `push ebx/ecx/edx/esi; mov ecx,eax ...` (164 bytes) | anim-target (dword `0xD27E4` after word `0xD000` at `0xD27E2`), unported |
| `0x3C87C` | `ret 8` at `0x3C876`, `lea eax,[eax+0]` | the switch table read by `jmp [eax*4+0x3C87C]` at `0x3CAE0` | data |

Two of the four are real unported code that U0's count left out (`0x10604`, `0x36114`).

## §E2.2 The classes and their evidence (defined before the tool ran on the image)

**Trusted code.** The instructions the classification may cite: a linear decode of every Ghidra function
of the code object, plus the body (`diff_emu.static_scan` from the entry, extended through every switch
`jmp [reg*4+T]` bounded by its own `cmp reg,N; ja` among the five instructions before it) of every entry
the evidence proves, to a fixpoint: a candidate with a table class (rules 1-6), a table slot value outside
Ghidra that is entry-like (after a `ret`, decodes; above `0x5D000` only for a span table, §E2.3), a target
of a trusted `call` in `[0x10000, 0x5D000)` outside Ghidra, and a plausible immediate of a trusted
instruction. **Tables** are read by a trusted `call`/`jmp [reg*4 + T]`: the slots run from T while the
dword is a code address, at most 512, and stop at the first address another trusted instruction uses as
a displacement. The reason an entry is trusted is recomputed from the final index; an entry whose evidence
the final index no longer holds is reported as stale (the planning run has none).

**The rules, first match wins** (each row's evidence names the proving address):

| # | class | rule | evidence string | target? |
|---|---|---|---|---|
| 1 | finisher | V is a dword of `0xBDAE4[0..6]` or `0xBDB00[0..6]` (U0 §U0.4: `[0xBDAE4 + char*4]`, `[0xBDB00 + char*4]`) | `dword BDAF0` | yes: finishers |
| 2 | move-callback | V is the +0 dword of a move-table entry `0xA3528 + (c*64 + r)*20`, c < 7, r < 64 (U0 §U0.12) | `dword A46D0 (char 3, reaction 0x22)` | yes: callbacks |
| 3 | span-writer | V is a slot of a span table (§E2.3) | `dword 80D10 = table 80D0C[1], read by `call` at 57FE4` | yes: span-writers |
| 4 | call-table | V is a slot of another table read by a `call` | as rule 3 | yes: voice or other |
| 5 | jump-table | V is a slot of a table read by a `jmp` | as rule 3 | yes: voice or other |
| 6 | anim-target | a dword equal to V sits in the data object right after an animation command word W with `W & 0x8000` (a command, not a sprite id: `0x2A408`), `W & 0x6000 == 0x4000` (an inline dword follows: `0x2B8F8`), opcode `(W >> 8) & 0x1F` in {0x10, 0x11, 0x15} (`0x2B2A0` calls through `DS_00105BD4` for these: `actors.c` `anim_indirect`); or four bytes after a `0x1F` prefix word whose low byte is such an opcode | `dword E88D4 after the opcode word D100 at E88D2` | yes: animation-targets |
| 7 | mid-instruction | V lies strictly inside a trusted instruction | `inside the instruction at 40003 (code of 3FFDC)` | no |
| 8 | data | V is a trusted memory displacement; or V starts with a filler (a zero filler needs `00 00`); or the scan from V hits undecodable bytes | `memory operand of the instruction at 11017` | no |
| 9 | code-immediate | V is an immediate of a trusted instruction (a stored callback) | `immediate of the instruction at 10647` | yes: voice or other |
| 10 | direct | a trusted `call` targets V | `` `call` at 4255D `` | yes: voice or other |
| 11 | interior | V is a trusted instruction of another entry's body, or a trusted `jmp`/`jcc` targets V | `` `je` at 186ED in the code of 186D0 `` | no |
| 12 | data-pointer | an aligned dword in the data object equals V (a pointer table no trusted indexed read names) | `aligned dword BB9E8` | yes: voice or other |
| 13 | interior | V is inside the scan from an untrusted entry X (§E2.5), V != X | `an instruction of the scan from 32BDC, which only the rel32 at 33571 (untrusted bytes) reaches` | no |
| 14 | dead | none of the above and no `E8`/`E9`/`0F 8x` rel32 at any byte offset below `0x5D000` lands on V | `no rel32 at any code offset, ...` | no |
| 15 | unclassified | a rel32 lands on V only from untrusted bytes | `rel32 at 17000 outside the trusted code; ...` | no |

**The brief's categories map onto these:** "a direct call/jmp" is rule 10 (a `jmp` to V from trusted code
makes V part of that code: rule 11); "a jump-table or pointer-table entry in data" is rules 4, 5 and 12;
"an animation-opcode stream dword" is rule 6; "the sprite span writer table" is rule 3; "a move-table
callback" is rule 2; "a finisher entry" is rule 1; "dead code" is rule 14; "data" is rules 7, 8 and 11/13
(not an entry). **"A voice site" is a property of a body, not a way of being reached**: a target whose body
holds a direct `call`/`jmp 0x2C3FC` (k7-k12 §0.2's voice entry) goes to the `voice` batch when its class has
no batch of its own, and a target row lists its voice sites (a non-target row lists none, §E2.7).

**Batches** (the P track's input, spec §4): finishers, callbacks, span-writers, animation-targets by class;
`voice` for a rules-4/5/9/10/12 target with a voice site; `other` for the rest; `-` for a non-target.

## §E2.3 The span tables (U0's range corrected by the raw)

U0 said "the table `0x80CB0..0x81308`, read by code at `0x57FE7..`; writers `0x521DC..0x5CF00`". The raw
has five tables, found by their readers (capstone over the image):

| table | reader (trusted) | index bound at the reader | slots the walk takes |
|---|---|---|---|
| `0x80C8C` | `call [edx*4+0x80C8C]` at `0x51EBF` in `0x51E5C` (and `0x51F35` in `0x51ED8`) | `and edx,0x7F` at `0x51EBC`: 128 | 32 (to `0x80D0C`, the next table) |
| `0x80D0C` | `call [eax*4+0x80D0C]` at `0x57FE4` in `0x57F80` | `and al,0x3F` at `0x57FD6`: 64 | 64 |
| `0x80E0C` | `call [eax*4+0x80E0C]` at `0x57FAA` in `0x57F80` | `test al,al; jns` at `0x57F93`: 0..0x7F | 129 (to `0x81010`) |
| `0x81010` | `call [eax*4+0x81010]` at `0x5D278`, span code **above** `0x5D000` (`0x5D218` is `0x80C8C[1]`) | not read | 64 (to `0x81110`) |
| `0x81110` | `call [eax*4+0x81110]` at `0x521AE`/`0x521BF` in `0x5215C` | `mov eax,0x80; cmp ecx,eax; jl` at `0x521A5`: 1..0x80 | 128 (to `0x81310`, which `0x51E91` reads as data) |

`0x51E5C` is the renderer's span blit (`sprite.c`); a table read by a `call` inside a span writer's body is a
span table too (the fixpoint), which is how `0x80D0C`, `0x80E0C`, `0x81110` (inside `0x57F80`, `0x5215C`, the
`0x80C8C` slots) and `0x81010` (inside `0x5D218`) are found. **The runtime cut `0x5D000` cuts through the
span code**: `0x5D004` (`0x81110[127]`), `0x5D218` (`0x80C8C[1]`) and `0x5D28F` (`0x80C8C[17]`) are span
code above it. They are outside the universe (rule (b)) and appear in the supplement (§E2.5).

**Named gap:** `0x80E0C[128]` = the dword `0x8100C` = `0x57E3C` lies beyond `0x57FAA`'s 7-bit index; it is
classified span-writer by the walk, but which reader reaches index 128 is open (the other readers of
`0x80E0C`, among them `0x580BD`, `0x581AE`, `0x58358`, `0x58392`, `0x58431` in `0x57FFB`, were not bounded).

## §E2.4 "Already ported"

A Ghidra function is ported by `tools/port_progress.py`'s rule (a `/* 0xADDR` anywhere on a line or an
`fn_register(0xADDR`). Any other address is ported only by the **strict** rule: an `fn_register(0xADDR` or
a `/* 0xADDR` comment **at column 0** (the one-function header of PORTING.md), because a non-Ghidra address
inside a comment line is usually a mid-function note (`fighter.c`'s `/* 0x210D4: 0x210C4's stream */`).
`sprite.c`'s `/* PORT: 0x57F80 ...` notes do not count: the port re-implements the span dispatchers as
one blit and has no C function per writer (Decision D2 in the plan).

## §E2.5 Outside the universe: the supplement, the untrusted entries, stale entries

- **Supplement:** trusted entries (§E2.2) that are not candidates: helpers reached only by a call, stored
  callbacks whose immediate is in non-Ghidra trusted code, and span code above the cut. 131 in the planning
  run, 35 unported (30 `stubs`, 5 `leaf`); 102 by an immediate, 26 by a call, 3 by a span slot
  (`0x5D004`, `0x5D218`, `0x5D28F`).
- **Untrusted entries:** plausible rel32 targets (after a `ret`, decodes, below the cut, not trusted) that
  only untrusted bytes reach. 30 in the planning run; 22 are already ported (the K11/service-menu code
  `0x2C9CC..0x34168`: the port found them by other means) and 19 are live-Ghidra entries; 8 are unported:
  `0x158ED 0x19AD4 0x19DD5 0x26163 0x26226 0x34962 0x45444 0x49078`. How their callers are reached is open
  (named gap): a jump table inside the code object, a computed address, or dead code.
- **Rule 13 exists because of `0x32E00`:** the prototype called it dead (no reference at all), but its
  "ret" is the `C3` of `mov ebx,eax` (`89 C3` at `0x32DFE`): it is the middle of the function `0x32BDC`
  (2 240 bytes to the next boundary), which only the rel32 at `0x33571` reaches, itself in untrusted bytes.
  With rule 13 the planning run has no `dead` and no `unclassified` row.
- **Stale:** 0 in the planning run.

## §E2.6 What E1 can verify today, per target (`readiness`)

First match, on the target's own body and its direct-call tree (breadth first in address order):
`stack-args` (its own `ret N`: E1 binds registers only, E1 §E.6.3); `stubs (X in F)` (the nearest thing in
the tree the emulator cannot run: an unmodeled instruction, an indirect `call`/`jmp` that is not a bounded
switch, or a truncated scan); `callees (...)` (all runnable, but a callee below the cut is unported);
`allow-list` (every callee is ported: E1's allow-list runs it today); `leaf` (no call). Readiness is
static: a stack-argument read without `ret N` and an EAX-only output (E1 §E.6.2) are not detected here.

Unported targets (329; the planning run had 332 before U6a's ports, §E2.9): **leaf 234** (227 span writers, 6 animation
targets, `0x10604`),
**allow-list 6** (`0x475EC`, `0x47608` callbacks; three animation targets; `0x29CFC`), **stubs 89**,
callees 0, stack-args 0. The `stubs` blockers: the animation dispatcher's code-pointer call at `0x2B56D`
in `0x2B2A0` (58 rows), `actor_spawn`'s indirect at `0x2B0E9` in `0x2AE14` (22), `0x18384` in `0x18350`
(2), `0x6811A` in `0x680F0` (1), `in` in `0x1B890` (1, `0x1BDF4`), `0x62006` in `0x62003` (1), and the
four span dispatchers' own table calls (`0x5215C`, `0x57F80`, `0x57FFB`, `0x58CBD`, which are
`0x80C8C` slots and so span-writer rows).

**The call-stub gap (E1 §E.6.1) gates track P.** Every one of the 85 non-span `stubs` rows has a clean
own body (no indirect, no unmodeled instruction, no `ret N`): the blocker is always inside a ported
callee's tree. They call 43 distinct direct callees: 35 ported and 8 not (`0x1BBAC 0x22404 0x23960
0x2BDB8 0x2BDE8 0x2BEF4 0x2D62C 0x3A9D8`); the most frequent are `0x2C3FC` (voice, 35 rows), `0x2BC30`
(31), `0x2AE14` (22) and `0x3C4CC` (21), all ported. So stubbing at the direct-callee boundary (§E2.10)
makes all 85, and 28 of the 30 `stubs` supplement entries (the other two are span code with their own
table call), runnable.

## §E2.7 Voice sites

A scan of every byte offset below `0x5D000` finds 303 rel32 `call`/`jmp` to `0x2C3FC` (k7-k12 §0.2's
count). 134 lie outside the Ghidra functions. Placed in the first body (target rows, then the supplement,
then the untrusted entries) whose scan holds the site: **48 in unported code** (39 in target rows, 9 in
supplement entries: all among U0's 66), **67 in ported code** (51 rows, 12 supplement, 4 untrusted),
**19 in no body**: U0's 17 plus `0x39EDA` and `0x45F8C`. So U0's 66 split 39 / 9 / 17 plus 1 in a ported row (`0x23243`, in `0x23208`, ported by U6a). The 19, with the
nearest plausible start before each (a backward search for an after-`ret` address outside Ghidra), whether
that start's scan reaches the site, and what references the start:

| site | nearest start | scan reaches it | dword anywhere in the image | rel32 | ported |
|---|---|---|---|---|---|
| 11A3D | 11A30 | yes | - | - | no |
| 11C38 | 11BC9 | no | - | - | no |
| 155EA | 1556C | yes | 155A7 | - | no |
| 15802 | 15791 | no | - | - | no |
| 158CD | 15835 | yes | 15798 | - | no |
| 1599B | 15960 | yes | - | - | no |
| 228EF | 2284C | yes | - | - | no |
| 2292B | 228F9 | yes | - | - | no |
| 23E9D | 23E14 | yes | 23D2C | - | no |
| 295FD | 295CD | yes | - | - | no |
| 39EDA | 39E0A | yes | - | - | no |
| 3D730 | 3D6E0 | yes | - | - | no |
| 41880 | 41878 | yes | - | - | no |
| 45F8C | 45F7D | no | - | - | no |
| 475D9 | 475C0 | yes | - | - | no |
| 48518 | 484F0 | yes | 48440 | - | no |
| 48548 | 48521 | no | - | - | no |
| 4B0B4 | 4B024 | yes | 4B064 | - | no |
| 4B0BE | 4B024 | yes | 4B064 | - | no |

The dwords at `0x155A7`, `0x15798`, `0x23D2C`, `0x48440`, `0x4B064` are inside the code object: switch
tables embedded in code, which the universe (data-object dwords only) does not read. These 19 are the
voice batch's open list (named gap): each needs its reaching evidence before P can port it.

## §E2.8 The live Ghidra project (evidence, never a class)

Query (read-only, 2026-10-01, the MCP instance, project `rage`, `/PRAGE.EXE`): `get_function_count` = 1504;
a read-only script listing `FunctionManager.getFunctions(true)` with each body's address ranges: 1359 real
functions + 145 `.image` noise. The committed export has 1207 + 145. **Every committed entry is live; 152
live entries are absent from the export (129 below `0x5D000`), and 24 shared entries have a different body**
(e.g. `0x186D0`'s body is `0x18625..0x186C0` plus `0x186D0..0x18712`). Those 176 rows are the committed file
`docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt` (Appendix A; 180 lines with its four
comment lines; sha256 `fe938f1c2bffcbd0c406e2ea0b706a107678de7ef33f81663a754fc293a69eb7`). The tool reads it
with `--live`; without it the column reads `-`. Planning run: **22 candidates are live entries** (17 move
callbacks, the animation targets `0x22338` and `0x458D4`, the direct `0x29CFC` and `0x3E800`, the finisher
`0x402FC`) and **4 are inside a live body** (`0x18625` in `0x186D0`, `0x18AAE` in `0x18B04`, `0x2C1AD` in
`0x2C1C8`, `0x40303` in `0x402FC`), which agrees with their `interior` class.

## §E2.9 Results (planning run, re-measured in Task 5)

`python3 tools/entry_triage.py --image IMG --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt
--out T --expect 579 --expect-u0 575` (8.4 s on the planning host). **Task 5 re-measured every figure on the same image
(sha1 above) with `port/src` at `098cb7a`; the three lines below are the Task 5 run.** The planning run (`port/src` at
`e9271df`) read `targets 332 unported, 163 ported` and `49 in unported code, 66 in ported code`; nothing else differs.
The difference is exactly U6a's four ports (strict `/* 0xADDR` headers added after the planning run; record
`2026-10-01-gameplay-u6-derivations.md` §U6.2 `0x23208` (e3a5d77), §U6.3 `0x3A588` (f9fbfc7), §U6.4 `0x3640C`
(7aea1f8), §U6.5 `0x37DCC` (396229d)): three of them are rows (`0x23208` a move-callback, `0x3640C` and `0x37DCC`
animation targets; each moves unported to ported, so callbacks 27/44 to 26/45, animation targets 65/47 to 63/49,
targets 332/163 to 329/166), `0x3A588` follows data rather than a `ret` and is outside the universe, and the one
voice site that moves is `0x23243` (in `0x23208`; 49/66 to 48/67). The E1 readiness counts follow
(leaf 236 to 234, stubs 90 to 89). Re-running the Task 5 code with those four headers removed reproduces the
planning figures exactly. No class rule (§E2.2) and no class count moved.

```
entry-triage: 579 candidates (575 by U0's rule); finisher=9 move-callback=71 span-writer=231 call-table=7 anim-target=112 mid-instruction=3 data=75 code-immediate=15 direct=2 interior=6 data-pointer=48
entry-triage: targets 329 unported, 166 ported; supplement 131 (35 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 48 in unported code, 67 in ported code, 19 nowhere
```

| class | rows | U0's | | batch | unported | ported |
|---|---|---|---|---|---|---|
| finisher | 9 | 9 | | callbacks | 26 | 45 |
| move-callback | 71 | 71 | | finishers | 6 | 3 |
| span-writer | 231 | 231 | | voice | 0 | 15 |
| call-table | 7 | 7 | | animation-targets | 63 | 49 |
| anim-target | 112 | 111 | | span-writers | 231 | 0 |
| mid-instruction | 3 | 3 | | other | 3 | 54 |
| data | 75 | 73 | | | | |
| code-immediate | 15 | 14 | | | | |
| direct | 2 | 2 | | | | |
| interior | 6 | 6 | | | | |
| data-pointer | 48 | 48 | | | | |
| jump-table, dead, unclassified | 0 | 0 | | | | |
| **total** | **579** | **575** | | **targets** | **329** | **166** |

Cross-checks against U0: the 26 unported move callbacks are exactly U0's 27 less `0x23208` (ported by U6a); the 6 unported finishers are
exactly U0's six (the tenth finisher value, `0x40BBC`, and the 72nd move-callback value, `0x14B90`, are
Ghidra functions, so outside the universe); U0's three animation examples are `anim-target`. The
animation targets' dwords all lie in `0xD213A..0xEE3BC` (56 after `D100`, 31 after `D500`, 24 after
`D000`; no `0x1F`-prefix form), U0's "`0xD2xxx..0xEDxxx` streams".

**The gate, in a full copy of the tree at `e9271df` with the plan's changes applied:** `make verify` exit 0
(with the `entry-triage` step printing the three lines above), the 45 oracle lines equal to
`k7-k12/scratch/oracle-lines-base.txt`, the `make audio-render` WAV identical to `before-t2.wav`, and
`port_progress.py` `771 1203 64` / `731 731 100`.

**The counted target list for track P** (unported, by batch): callbacks 26, finishers 6, animation
targets 63, span writers 231 (or the six dispatchers, Decision D2), other 3 (`0x10604`, `0x1BDF4`,
`0x29CFC`), voice 0 rows (the 48 + 19 voice sites ride with their bodies' batches, §E2.7); plus the
supplement's 35 unported entries and the 8 unported untrusted entries (§E2.5).

## §E2.10 Follow-up (not a task): the call-stub design E1 §E.6.1 names

It gates track P (§E2.6: 85 + 28 targets wait on it). A proposal, to be planned as its own unit:

1. **A stub is data in the `Spec`:** `(callee address, EAX it returns, bytes it writes)` per case. Both sides
   apply the same writes and return the same EAX, and both record the call.
2. **Original side** (`diff_emu.run_original(..., stubs=...)`): in the code hook, a direct `call` to a
   stubbed address records `(target, the seven registers, the top four stack dwords)`, applies the writes,
   sets EAX and resumes at the next instruction (no push, no callee bytes run). An **indirect** call is
   resolved at run time from its operand and treated the same way when the target is stubbed or
   allow-listed; this lifts the `0x2B56D` and `0x2B0E9` blockers without a static answer.
3. **Port side:** the C code calls its callees directly, so interception needs a seam in `port/src`: a
   macro at the top of each stubbable ported function, compiled only into a second core library linked
   into `diffrun` (`-DPR_DIFF_STUBS`); the normal build is unchanged, so no oracle line can move. Indirect
   calls already pass through `fn_resolve`, whose miss log records them (spec §5.2). Link-time
   interposition is not an option on this host (macOS `ld64` has no `--wrap`, and a second strong
   definition of a symbol in the same static archive is a duplicate).
4. **Compared:** the ordered call list `(target, arguments)` on both sides (spec §5.1(3); closes E1
   §E.6.8), beside EAX and the changed bytes. A stubbed call made with a wrong argument must be a
   detected mutant (the self-check gains one).
5. **Scope:** the 43 direct callees of the stub rows (8 of them unported: stub those, or port them
   first); the four most frequent (`0x2C3FC`, `0x2BC30`,
   `0x3C4CC`, `0x2AE14`) cover most rows. Stack arguments (`ret N`) still need E1 §E.6.3's binding form.

## §E2.11 Named gaps

- `0x80E0C[128]` (`0x57E3C`): which reader reaches it (§E2.3).
- The runtime cut at `0x5D000` excludes the span code `0x5D004`, `0x5D218`, `0x5D28F` from the universe;
  they are listed in the supplement, not classified.
- The 8 unported untrusted entries and how their callers are reached (§E2.5).
- The 19 voice sites in no body (§E2.7).
- Readiness is static (§E2.6): stack reads without `ret N`, non-EAX outputs and input-dependent indirect
  targets are not seen.
- The table walk stops at the next displacement; a table whose end no displacement marks can overrun into
  adjacent code addresses. The span tables' bounds above are checked by hand; the call/jump tables of
  rules 4-5 (7 rows) are not.

## Appendix A: the live-Ghidra delta file

Task 1 writes this block, byte for byte (from the line after the opening fence to the line before the
closing fence), to `docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt`; its sha256 must be
`fe938f1c2bffcbd0c406e2ea0b706a107678de7ef33f81663a754fc293a69eb7`.

```text
# Live Ghidra functions that differ from port/decomp/prage.functions.csv (record 2026-10-01-reverse-e2 §E2.8).
# Query (read-only, 2026-10-01, project rage, /PRAGE.EXE, 1504 functions = 1359 real + 145 .image):
# FunctionManager.getFunctions(true), one line per real function: entry,lo-hi[;lo-hi...] (the body ranges, inclusive);
# kept: entries absent from the export (152) and entries whose body is not [entry, entry + size) (24).
00010034,00010034-00010034
00010035,00010035-000100c0
000106c4,000106c4-000106d9
000106dc,000106dc-000106e0
00014814,00014814-0001490a
0001490c,0001490c-00014937
00014cc4,00014cc4-00014d78
00014d7c,00014d7c-00014e42
00015160,00015160-0001519e
00015208,00015208-0001527a
0001527c,0001527c-000152d0
000186d0,00018625-000186c0;000186d0-00018712
00018b04,00018aae-00018af5;00018b04-00018b41
00019c60,00019c60-00019cf0
00019d34,00019d34-00019dd7
0001ae20,0001ae20-0001ae27
0001ae28,0001ae28-0001aedd
00020fa0,00020fa0-00020fde
00020fe0,00020fe0-00021041
000210c4,000210c4-00021111
000215b0,000215b0-00021693
00021994,00021994-00021a84
00021da4,00021da4-00021e0d
00021e10,00021e10-00021ea1
00021ea4,00021ea4-00021f2c
00021f88,00021f88-0002201b
0002201c,0002201c-0002208a
0002208c,0002208c-000220f3
000220f4,000220f4-00022185
00022188,00022188-000221fd
00022200,00022200-00022293
00022294,00022294-00022336
00022338,00022338-00022402
00022404,00022404-00022492
00022ce4,00022ce4-00022d8a
00022d8c,00022d8c-00022e40
00023178,00023178-000231bf
000231c0,000231c0-00023207
00023208,00023208-0002324f
00023250,00023250-000232b0
000232b4,000232b4-0002338d
00023530,00023530-000235c0
00023960,00023960-00023a78
00029cfc,00029cfc-00029d00
0002bdb8,0002bdb8-0002bde6
0002bde8,0002bde8-0002bdff
0002bef4,0002bef4-0002bef8
0002c1c8,0002c1ad-0002c1d3
0002c9cc,0002c9cc-0002c9e6
0002c9e8,0002c9e8-0002ca02
0002cc74,0002cc74-0002cccc
0002cd30,0002cd30-0002cefe
0002cf00,0002cf00-0002d2e2
0002e11c,0002e11c-0002e17e
0002e218,0002e218-0002e245
0002e248,0002e248-0002e5e0
0002e5e4,0002e5e4-0002e932
0002ebbc,0002ebbc-0002ebcb
0002ef48,0002ef48-0002efc2
0002f48c,0002f48c-0002f4ba
0002f99c,0002f99c-0002f9ca
00030fe8,00030fe8-00031134
000314a0,000314a0-0003157a
000319b0,000319b0-000319e6;00031a44-00031a67
00031a78,00031a78-00031adc;00031b5d-00031b80
00031b94,00031b94-00031bdd;00031c4a-00031c67
00031c78,00031c78-00031cf8;00031deb-00031e08
000328b8,000328b8-0003291c
00032f54,00032f54-00032f94
00032f98,00032f98-00033057
00033058,00033058-0003322d
00033230,00033230-00033454
00033458,00033458-0003355c
000340bc,000340bc-00034167
00034168,00034168-0003427b
00035e40,00035e40-00035e6a
00038034,00038034-000380c3
0003a9d8,0003a9d8-0003aa51
0003bc70,0003bc70-0003bcdf
0003bce0,0003bce0-0003bd89
0003bd8c,0003bd8c-0003bdac
0003bf70,0003bf70-0003c046
0003c048,0003c048-0003c0a0
0003c0ec,0003c0ec-0003c128
0003c12c,0003c12c-0003c146
0003c404,0003c404-0003c47c
0003c600,0003c600-0003c64e;0003c6a0-0003c6a4
0003d3ac,0003d3ac-0003d3e1
0003d484,0003d484-0003d4d8
0003d4dc,0003d4dc-0003d672
0003d858,0003d858-0003d8a8
0003d8ac,0003d8ac-0003d9e2
0003dd14,0003dd14-0003dd82
0003de54,0003de54-0003dfbc
0003dfc0,0003dfc0-0003e060
0003e0f0,0003e0f0-0003e15c
0003e484,0003e484-0003e4c2
0003e62c,0003e62c-0003e695
0003e800,0003e800-0003e8e3
0003ec20,0003ec20-0003ecf5
0003ed78,0003ed78-0003edb6
0003efe0,0003efe0-0003f01e
0003f184,0003f184-0003f1ed
0003f1f0,0003f1f0-0003f281
0003f284,0003f284-0003f305
0003f360,0003f360-0003f3f3
0003f3f4,0003f3f4-0003f44e
0003f450,0003f450-0003f4b7
0003f4b8,0003f4b8-0003f549
0003f54c,0003f54c-0003f5ba
0003f5bc,0003f5bc-0003f64f
0003f650,0003f650-0003f6f2
0003f7f4,0003f7f4-0003f858
0003f85c,0003f85c-0003f9ab
000401d4,000401d4-000402fa
000402fc,000402fc-00040356
00044d78,00044d78-00044db6
00044db8,00044db8-00044e0a
00044fb4,00044fb4-00044ff2
00044ff4,00044ff4-00045044
00045158,00045158-000451ea
000451ec,000451ec-00045236
000455a0,000455a0-0004563c
00045640,00045640-000456a4
000456a8,000456a8-00045784
00045878,00045878-000458d2
000458d4,000458d4-00045904
000459f4,000459f4-00045a32
00045a34,00045a34-00045a6f
00047648,00047648-00047686
000477a8,000477a8-000477e6
000477e8,000477e8-0004782d
000478d4,000478d4-00047983
00048054,00048054-000480b3
00048170,00048170-00048251
00048668,00048668-000486f6
000488b8,000488b8-00048963
0004e350,0004e350-0004e437;0004e48f-0004e4bf;0004e4da-0004e5a1
0005dac1,0005dac1-0005dac2
0005dbf4,0005dbf4-0005dbf5
0005dbf6,0005dbf6-0005dc0e
0005dd2c,0005dd2c-0005dd2d
0005dd2e,0005dd2e-0005dd5c
0005dd5d,0005dd5d-0005dd5e
0005dd5f,0005dd5f-0005dd85
0005dd86,0005dd86-0005dd87
0005dd88,0005dd88-0005ddac
0005ddad,0005ddad-0005ddae
0005ddaf,0005ddaf-0005ddcf
0005dfeb,0005dfeb-0005dfec
0005dfed,0005dfed-0005dff9
0005e783,0005e783-0005e865;0005e880-0005e89a;0005e8a0-0005e94a;0005e950-0005e9b3;0005e9c0-0005eb03;0005eb10-0005eb2b;0005eb30-0005eb3b;0005eb40-0005eb4b;0005eb50-0005eb5b;0005eb60-0005eb69;0005eb70-0005eb7b;0005eb80-0005eb8b;0005eb90-0005eb95;0005eba0-0005eba5;0005ebb0-0005ebb3;0005ebc0-0005ebc5;0005ebd0-0005ebd8;0005ebe0-0005ebe8;0005ebf0-0005ebf8;0005ec00-0005ec0c;0005ec10-0005ec21;0005ec30-0005ec4b;0005ec50-0005ec5b;0005ec60-0005ec6b;0005ec70-0005ec7b;0005ec80-0005ec89;0005ec90-0005ec9b;0005eca0-0005ecab;0005ecb0-0005ecb5;0005ecc0-0005ecc5;0005ecd0-0005ecd3;0005ece0-0005ece5;0005ecf0-0005ecf8;0005ed00-0005ed08;0005ed10-0005ed18;0005ed20-0005ed2c;0005ed30-0005ed41;0005ed50-0005ed6b;0005ed70-0005ed7b;0005ed80-0005ed8b;0005ed90-0005ed9b;0005eda0-0005eda9;0005edb0-0005edbb;0005edc0-0005edcb;0005edd0-0005edd5;0005ede0-0005ede5;0005edf0-0005edf3;0005ee00-0005ee05;0005ee10-0005ee18;0005ee20-0005ee28;0005ee30-0005ee38;0005ee40-0005ee4c;0005ee50-0005ee62;0005ee70-0005ee8b;0005ee90-0005ee9b;0005eea0-0005eeab;0005eeb0-0005eebb;0005eec0-0005eec9;0005eed0-0005eedb;0005eee0-0005eeeb;0005eef0-0005eef5;0005ef00-0005ef05;0005ef10-0005ef13;0005ef20-0005ef25;0005ef30-0005ef38;0005ef40-0005ef48;0005ef50-0005ef58;0005ef60-0005ef6c;0005ef70-0005ef91;0005efa0-0005efcc;0005efd0-0005f033;0005f040-0005f188;0005f190-0005f2d6;0005f2e0-0005f427;0005f430-0005f57b;0005f580-0005f5ac;0005f5b0-0005f613;0005f620-0005f644;0005f650-0005f706;0005f710-0005f73a;0005f740-0005f757
0005f758,0005f758-0005f817;0005f820-0005f87f;0005f890-0005f8aa;0005f8b0-0005f95a;0005f960-0005f97a;0005f980-0005fac3;0005fad0-0005faeb;0005faf0-0005fafb;0005fb00-0005fb0b;0005fb10-0005fb1b;0005fb20-0005fb29;0005fb30-0005fb3b;0005fb40-0005fb4b;0005fb50-0005fb55;0005fb60-0005fb65;0005fb70-0005fb73;0005fb80-0005fb85;0005fb90-0005fb98;0005fba0-0005fba8;0005fbb0-0005fbb8;0005fbc0-0005fbcc;0005fbd0-0005fbe1;0005fbf0-0005fc0b;0005fc10-0005fc1b;0005fc20-0005fc2b;0005fc30-0005fc3b;0005fc40-0005fc49;0005fc50-0005fc5b;0005fc60-0005fc6b;0005fc70-0005fc75;0005fc80-0005fc85;0005fc90-0005fc93;0005fca0-0005fca5;0005fcb0-0005fcb8;0005fcc0-0005fcc8;0005fcd0-0005fcd8;0005fce0-0005fcec;0005fcf0-0005fd01;0005fd10-0005fd2b;0005fd30-0005fd3b;0005fd40-0005fd4b;0005fd50-0005fd5b;0005fd60-0005fd69;0005fd70-0005fd7b;0005fd80-0005fd8b;0005fd90-0005fd95;0005fda0-0005fda5;0005fdb0-0005fdb3;0005fdc0-0005fdc5;0005fdd0-0005fdd8;0005fde0-0005fde8;0005fdf0-0005fdf8;0005fe00-0005fe0c;0005fe10-0005fe22;0005fe30-0005fe4b;0005fe50-0005fe5b;0005fe60-0005fe6b;0005fe70-0005fe7b;0005fe80-0005fe89;0005fe90-0005fe9b;0005fea0-0005feab;0005feb0-0005feb5;0005fec0-0005fec5;0005fed0-0005fed3;0005fee0-0005fee5;0005fef0-0005fef8;0005ff00-0005ff08;0005ff10-0005ff18;0005ff20-0005ff2c;0005ff30-0005ff51;0005ff60-0005ff88;0005ff90-0005ffb5;0005ffc0-00060022;00060030-0006004a;00060050-00060198;000601a0-000602e6;000602f0-00060437;00060440-0006058b;00060590-000605b8;000605c0-000605e5;000605f0-00060652;00060660-000606a0;000606b0-000606d5;000606e0-00060742;00060750-000607b3;000607c0-000607ea;000607f0-00060815;00060820-00060882;00060890-00060897
00060898,00060898-00060925;00060950-000609ab;000609b0-00060a18;00060a20-00060a8f
0006180a,0006180a-0006180e
00061a47,00061a47-00061a4b
00061c6a,00061c6a-00061cd1
0006245c,0006245c-0006245d
00064464,00064464-0006448e
00064490,00064490-000649ad
00067130,00067130-000671d4;0006722b-00067579
0006ae80,0006ae7b-0006af8a
0006b7eb,0006b7eb-0006b807
0006b808,0006b808-0006b83e
0006be9d,0006be9d-0006beba
0006c1aa,0006c1aa-0006c1ae
0006c5db,0006c5db-0006c6c3
0006f506,0006f500-0006f5c7
0006fb28,0006fb28-0006fc5b;0006fe80-0006fe84;0006fea4-0007065b;00070661-00070668;00070689-0007068d;000706a3-000707a7;000707c8-00070828;00070849-00070a43;00070a64-00070b4d;00070b6e-00070c65;00070c86-00070cee;00070d0f-00070d1a;00070d41-00070ef7;00070f18-000710ac;000710cd-0007123d
0007065c,0007065c-00070660
000716aa,000716aa-0007170f;00071713-000717a5
00072942,00072942-00072a0b
00072a66,00072a66-00072a8c
00072b16,00072b16-00072b67
000732a6,000732a6-000732d0
000738a0,000738a0-000738b1;000738c3-000738c5
```
