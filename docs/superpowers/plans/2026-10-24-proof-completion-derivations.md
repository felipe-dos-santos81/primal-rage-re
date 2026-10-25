# Proof Completion — derivation record

Status: written 2026-10-24, task 1 of the proof-completion plan
(`docs/superpowers/plans/2026-10-24-proof-completion.md`), against the reviewed
spec `docs/superpowers/specs/2026-10-24-proof-completion-design.md`.

This record is the authoritative raw-byte source for the tasks that implement
the phase. It holds the **measured** baseline (§P1) the phase's closure compares
against, and the **span-writer census** (§P2) that Tasks 2, 3, 7 and 11 cite.
Every number below was measured on the phase's base commit (`ff9482fb`, branch
`proof-completion`); where the plan predicted a figure that the measurement
disagrees with, the measured figure is recorded here and the disagreement is
named (§P2.6).

Reproduce (from the repo root):

```bash
python3 tools/port_progress.py
make build && make diff-verify
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/e2img
python3 -m unittest tools.tests.test_diff_verify
```

The image `diffrun --image-out` writes is the **fixup-applied** one: raw-file
offsets are pre-fixup (`mem_load_le` + `mem_load_le_fixups`, `port/src/mem.c`),
so a raw-file read of a table names other addresses (AGENTS.md, "Address
model"). Every table below was read from the applied image.

---

## P1 — The measured baseline

### P1.1 `port_progress`

```
$ python3 tools/port_progress.py
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)
```

799 of Ghidra's 1203 indexed functions (66%) and 759 of 759 portable ones. The
phase adds no `FN_` function and no `fn_register`, so both lines must be
unchanged at closure; the 66% figure is "of Ghidra's 1203 indexed functions".

### P1.2 The differential counter

`make diff-verify` (exit 0) printed, verbatim:

```
diff-verify: 1031/1031 functions VERIFIED; 14983/14983 mutants detected; 0 named gaps; 813/844 rows with callees closed (187 have none); 806/844 closed on full rows; 8 ported functions with no row (45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape exceptions] + 37 runtime; the scan-visible subset is 42 = 5 + 37). The closure rule (record J §J1): an allow-mode callee outside the portable set - runtime >= 0x5D000, host-owned/deferred in tools/port_classification.txt, or an address the decomp index never lists as a function - does not keep a row open, provided the port tree does not implement the address in any class (no fn_register, no column-0 `/* 0xADDR` header), while a recorded stub or real callee and a ported function without a row do. Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
```

The same line is pinned by `tools/tests/test_diff_verify.py`'s
`test_the_self_check_counts_functions_mutants_gaps_and_closed_rows` (the
`assertIn` at the end of that method). The unittest run in the same target
printed `Ran 217 tests ... OK` before this task's census class and `Ran 222
tests ... OK` after it (the two harness modules, `test_diff_emu` and
`test_diff_verify`; the count itself is not pinned), and the counter line is
byte-identical in both runs — this task adds tests, not a row.

The "before" figures the phase moves (all read from the line, not predicted):

| figure | before |
|---|---|
| functions VERIFIED | 1031 |
| mutants detected | 14983 |
| named gaps | 0 |
| rows with callees / closed | 844 / 813 |
| rows with callees / full | 844 / 806 |
| open rows (844 - 813) | 31 |
| ported functions with no row | 8 (45 `FN_` ported-unrowed: 8 portable + 37 runtime) |
| weaker rows (the `WEAKER` set) | 12 |
| unrowed (the `UNROWED` set) | 8 |

### P1.3 The capture inventory

**35 expected capture directories** (enumerated from the oracle targets, never
globbed; spec §6.3):

- **30 K11/gp** — the 26 gp-oracle scenarios (`gp-idle-loss`, `gp-u5-charsel`,
  `gp-u6-moves-b`, `gp-keys-fight`, `gp-twop`, the seven `gp-u8-*`
  [`attract-start`, `endurance`, `handicap`, `left-training`, `right-arcade`,
  `right-training`, `tug-of-war`], `gp-u9-win`, `gp-u10-ending`,
  `gp-u12-endurance`, `gp-u13-ending2`, `gp-u14-win-live`, `gp-u15-join`,
  `gp-u16-twop2`, `gp-u17-device2`, `gp-u18-talon-ending`,
  `gp-u19-vertigo-ending`, `gp-u20-chaos-ending`, `gp-u21-armadon-ending`,
  `gp-u22-diablo-ending`, `gp-u23-u16-finisher`) plus `walk`, `menuesc`,
  `save-reload` and `gp-pads`.
- **2 Smacker** — `data/smk-captures/twi5`, `data/smk-captures/twg`.
- **3 title** — `data/title-captures/title`, `title2`, `frontend`.

**34 present, 1 absent by design**: `data/k11-captures/gp-u14-win-live` is the
U14 R1 named gap (the fought live win; endgame record §E2/§E11 — Task 8 was
declined and its two permitted attempts are spent).

`data/k11-captures/` also holds **five** directories that are **not** among the
35: `de`, `diags`, `idle` (K11 scenarios no oracle target consumes),
`gp-idle-loss-run2` and `gp-u6-moves` (superseded capture names; the oracles
use `gp-idle-loss` and `gp-u6-moves-b`). They are not expected, not counted,
and not a gate input — which is why a directory listing (34 entries there) is
not the gate's inventory.

**Which are enforced** by plain `make verify`:

| capture | mechanism | absent capture |
|---|---|---|
| `smk/twi5`, `smk/twg` | `smk-oracle` under `PR_ORACLE_REQUIRED=1`; `smk_compare.py` enforces absence itself | fails |
| `title` | `title-oracle` under `PR_ORACLE_REQUIRED=1`; `title_compare.py` | fails |
| `title2` | optional second capture (`$(wildcard ...)`) — proves determinism only | not compared |
| `frontend` | `frontend-oracle` (not under `PR_ORACLE_REQUIRED`): the comparison is enforced — any unexplained frame fails — whenever the directory exists | prints "not compared", exit 0 |
| the 30 K11/gp | each oracle target skips without its directory (exit 0), even under `PR_ORACLE_REQUIRED` (none passes `--required`) | skips |

So **3 directories are enforced** (smk/twi5, smk/twg, title), **2 are compared
only when present** (title2, frontend), and **30 skip silently with exit 0**.
`attract-oracle` is a target on the `title` capture (it derives the attract
boundary from the same dump), **not** a fourth capture directory.

### P1.4 The 24 gp scenario pins

Every enforced gameplay oracle pins `MIN_FIRST` (the first unexplained capture
frame ratchet), `TRACE_MIN_FIRST`, `MAX_START`, `MIN_REACH`, the capture's
`poll.log` sha256 and its frame count; some add `MOVES_MIN_FIRST`,
`WIN_MIN_FIRST`/`MILESTONES` or `MIN_EFFECTS`. 24 scenarios replay (the 26
gp-oracle directories minus `gp-u14-win-live`, whose pins are empty, and
`gp-u17-device2`, whose frame/trace ratchets are empty — its wall-paced walk has
no replay, record §H6.9). All values are `Makefile` pins as measured on this
base:

| scenario (Makefile id) | MIN_FIRST | TRACE_MIN_FIRST | MAX_START | MIN_REACH | FRAMES | extra |
|---|---|---|---|---|---|---|
| gp-idle-loss (`IDLE_LOSS`) | 7559 | 8320 | 90 | 6300 | 8173 | — |
| gp-u5-charsel (`CHARSEL`) | 516 | 1513 | 100 | 358 | 1464 | — |
| gp-u6-moves-b (`MOVES`) | 2139 | 3248 | 88 | 1737 | 2205 | moves 3248 |
| gp-keys-fight (`KEYS`) | 1136 | 2164 | 100 | 788 | 1166 | 11 effects |
| gp-twop (`TWOP`) | 612 | 1506 | 83 | 463 | 680 | moves 1506 |
| gp-u8-right-arcade (`MODES_RA`) | 1072 | 2274 | 88 | 825 | 1140 | — |
| gp-u8-left-training (`MODES_LT`) | 1076 | 2338 | 83 | 847 | 1141 | — |
| gp-u8-right-training (`MODES_RT`) | 1098 | 2402 | 90 | 845 | 1167 | — |
| gp-u8-tug-of-war (`MODES_TW`) | 1107 | 2466 | 104 | 826 | 1176 | — |
| gp-u8-handicap (`MODES_HC`) | 1022 | 2274 | 80 | 789 | 1081 | — |
| gp-u8-endurance (`MODES_EN`) | 278 | 1174 | 90 | 163 | 308 | — |
| gp-u8-attract-start (`MODES_AS`) | 1087 | 2018 | 80 | 823 | 1156 | — |
| gp-u12-endurance (`U12`) | 732 | 1931 | 100 | 490 | 772 | — |
| gp-u9-win (`WIN`) | 2452 | 3503 | 100 | 1976 | 2521 | win 3503, 8 milestones |
| gp-u10-ending (`ENDING`) | 5437 | 9954 | 83 | 4662 | 5704 | win 9954, 30 milestones |
| gp-u13-ending2 (`U13`) | 5904 | 10069 | 96 | 4952 | 5904 | win 10069, 30 milestones |
| gp-u15-join (`U15`) | 805 | 2158 | 90 | 590 | 805 | — |
| gp-u16-twop2 (`U16`) | 8069 | 8637 | 100 | 6817 | 8091 | moves 8637 |
| gp-u23-u16-finisher (`U23`) | 4095 | 5449 | 106 | 3439 | 5464 | moves 5467 |
| gp-u18-talon-ending (`U18`) | 5735 | 9959 | 85 | 4864 | 5749 | win 9959, 30 milestones |
| gp-u19-vertigo-ending (`U19`) | 5991 | 10012 | 85 | 4989 | 6004 | win 10012, 30 milestones |
| gp-u20-chaos-ending (`U20`) | 5776 | 10070 | 83 | 4890 | 5790 | win 10070, 30 milestones |
| gp-u21-armadon-ending (`U21`) | 5932 | 10070 | 90 | 4973 | 5946 | win 10070, 30 milestones |
| gp-u22-diablo-ending (`U22`) | 5798 | 10090 | 74 | 4929 | 5811 | win 10090, 30 milestones |

The capture-identity pins (`GP_*_CAPTURE_SHA256`, all of `poll.log`) are in the
`Makefile` next to each row; they are not repeated here. `gp-u14-win-live`'s
eight `GP_U14_*` pins are empty (its oracle keeps the absent-capture skip);
`gp-u17-device2` pins `CAPTURE_FRAMES` 155, `DEVICE` 2, `CLEAR_FRAME` 761,
`DEVICE_BEFORE` 149, `DEVICE_FRAME` 150, `DEVICE_ROWS` 40:46 and the two screen
sha256s, with the four ratchet pins empty.

### P1.5 The K11, front-end and attract pins

| pin | value | where |
|---|---|---|
| `ATTRACT_MIN_PORT_EXH` | 173 | `Makefile` (port attract frames the `title` capture exhibits) |
| `--expect-first` (attract) | 215 | `Makefile` `attract-oracle` (first divergence = the last attract capture frame) |
| `FRONTEND_END` | 1884 | `Makefile` (the front-end window's END, sub-project A A1b) |
| `DEMO_FIGHT_MIN_FIRST` | 1886 | `Makefile` (the demo-fight ratchet; the capture's end + 1) |
| `ATTRACT2_MIN_FIRST` | 3617 | `Makefile` (the attract cycle-2 ratchet) |
| `K11_WALK_MIN_EXH` | 36 | `Makefile` (distinct port frames the walk capture exhibits) |
| `K11_MENUESC_MIN_EXH` | 184 | `Makefile` (the menuesc restart) |
| `K11_OPEN_END['menuesc']` | 388 | `tools/k11_compare.py` (the restart capture's window END ratchet) |

The narrow-claim caveats are unchanged and are what Tasks 8–10 widen: the K11
oracles have **no capture-identity pin** (Task 9 adds `poll.log` + frame count),
`FRONTEND_ALLOWED_UNEXPLAINED = (832, 833)` has **no byte pin** (Task 9), and
`K11_ALLOWED_UNEXPLAINED` (`walk` 138, 151) has no byte pin (Task 9). The
gameplay oracles claim no order and no coverage count (Task 8/9).

---

## P2 — The span-writer census

The original reaches its unrolled copy routines through five tables. The span
blit `0x51E5C` dispatches on the node's type; the six renderers it reaches call
a writer by run length. The port replaces all of it with four C loops
(`port/src/platform/sprite.c`: `copy_run`, `rle_row`,
`sprite_render_rle`, `sprite_render_rle_clipped`, `sprite_render_raw`,
`sprite_render_shear`), and the differential row `render_list_blit`
(`tools/diff_verify.py`) exercises 11 of the classes today.

### P2.1 The five tables and the index formulas

Decoded from `/tmp/e2img` (the fixup-applied image). Addresses are linear;
`0x80C8C`-`0x81110` are in the data object.

| table | address | slots | contents |
|---|---|---|---|
| renderer dispatch | `0x80C8C` | 32 | six renderers + the bare `ret`; 7 distinct, 25 duplicate slots |
| fill, backward | `0x80D0C` | 64 | one writer per run length `n = 0..63` |
| literal, backward | `0x80E0C` | 129 | `n = 0..127` plus the slot at `0x8100C` (`n = 128`) |
| fill, forward | `0x81010` | 64 | one writer per `n = 0..63` |
| literal, forward | `0x81110` | 128 | `n = 0..127`; `0x81110[128]` is the separate bank dword `0x81310` |

Totals: **417 slots**, **392 distinct targets**, **385 writer slots** (all
distinct: no writer slot is a duplicate). The 25 duplicates are all in the
renderer table.

Index formulas (disassembled at the call sites):

- `0x51E5C`: `call dword ptr [(type & 0x7F) * 4 + 0x80C8C]` (0x51EBF). The
  mask is `0x7F`, so a type `>= 0x20` indexes past the 32-entry table into the
  fill-backward table's first entries (a domain exclusion, §P2.3).
- `0x5D218` (unclipped RLE, type 1) and `0x5D28F` (clipped RLE, type 0x11):
  literal `call [eax*4 + 0x81110]` (0x5D23E/0x5D375) with `eax` = the 7-bit
  control byte (`b < 0x80`, 0x5D227 `jns`) or, on the straddle paths, the
  overhang remainder (`neg edx`, 0x5D36F/0x5D373); fill `call [eax*4 + 0x81010]`
  (0x5D278) with `eax = b & 0x3F` (0x5D26A).
- `0x57F80` (mirrored RLE, type 9) and `0x57FFB` (mirrored + clipped, type
  0x19): literal `call [eax*4 + 0x80E0C]` (0x57FAA/0x580BD), fill
  `call [eax*4 + 0x80D0C]` (0x57FE4/0x580FD).
- `0x58CBD` (raw, types 2/0x12) and `0x5215C` (shear, types 4/6/0x14/0x16)
  copy `vis` bytes in 128-byte chunks: `mov eax,0x80; cmp ecx,eax; jl ...;
  call [eax*4 + 0x81110]` (0x58CE3..0x58CEC, 0x521A5..0x521AE), then a
  remainder `mov eax,ecx; call [eax*4 + 0x81110]` (0x58CFB/0x58CFD,
  0x521BD/0x521BF). The chunk index is **128**, i.e. `0x81110[128]` =
  `0x81310[0]` (§P2.3).

### P2.2 The 27-entry class key

The class key is `(table, run-length class)`, plus one class for the renderer
table's bare `ret`. The run-length classes:

- `n<4` — the tail-only runs (n = 0 is the bare `ret`; 1..3 write bytes/words
  with no dword group);
- `4mod4`, `1mod4`, `2mod4`, `3mod4` — `n >= 4` with that remainder;
- `wordtail2`, `wordtail3` — **fill tables only**: at `n >= 14` a 2-byte tail
  becomes a **word** write (0x585F0 `n=14`: 3 dwords + `mov word [edi+0xC], dx`;
  0x58600 `n=15`: word + `dl`; mirrored 0x52284/0x52294) where `n=6/7` write
  two/three **bytes** (0x58580/0x5858C). The literal tables have no such rule:
  forward `n=2/3` write a word/word+byte (0x58D20/0x58D2C) and backward
  `n=2/3` write two/three bytes (0x52990/0x529A0), so their tails are the
  ordinary `2mod4`/`3mod4` classes;
- `chunk128` — the 128-byte chunk index (§P2.3).

Counts and representatives (measured; the census test pins this table):

| class key | slots | representative |
|---|---|---|
| `fill_fwd/n<4` | 4 | `0x58550` (n=0, `ret`) |
| `fill_fwd/4mod4` | 15 | `0x58570` (n=4) |
| `fill_fwd/1mod4` | 15 | `0x58578` (n=5) |
| `fill_fwd/2mod4` | 2 | `0x58580` (n=6) |
| `fill_fwd/3mod4` | 2 | `0x5858C` (n=7) |
| `fill_fwd/wordtail2` | 13 | `0x585F0` (n=14) |
| `fill_fwd/wordtail3` | 13 | `0x58600` (n=15) |
| `fill_bwd/n<4` | 4 | `0x521DC` |
| `fill_bwd/4mod4` | 15 | `0x521FC` |
| `fill_bwd/1mod4` | 15 | `0x52204` |
| `fill_bwd/2mod4` | 2 | `0x52210` |
| `fill_bwd/3mod4` | 2 | `0x5221C` |
| `fill_bwd/wordtail2` | 13 | `0x52284` |
| `fill_bwd/wordtail3` | 13 | `0x52294` |
| `lit_fwd/n<4` | 4 | `0x58CBC` |
| `lit_fwd/4mod4` | 31 | `0x58D40` |
| `lit_fwd/1mod4` | 31 | `0x58D4C` |
| `lit_fwd/2mod4` | 31 | `0x58D60` |
| `lit_fwd/3mod4` | 31 | `0x58D74` |
| `lit_fwd/chunk128` | 1 | `0x5D110` (the out-of-table slot) |
| `lit_bwd/n<4` | 4 | `0x52980` |
| `lit_bwd/4mod4` | 31 | `0x529B4` |
| `lit_bwd/1mod4` | 31 | `0x529C4` |
| `lit_bwd/2mod4` | 31 | `0x529DC` |
| `lit_bwd/3mod4` | 31 | `0x529F8` |
| `lit_bwd/chunk128` | 1 | `0x57E3C` (**dead**) |
| `ret` | 1 | `0x51E58` |

**27 classes, 26 reachable, one dead** (`lit_bwd/chunk128`, §P2.3).

Arithmetic check: 4+15+15+2+2+13+13 = 64 per fill table; 4+31+31+31+31+1 = 129
for `lit_bwd` (its 129 counted slots); 4+31+31+31+31+1 = 129 for `lit_fwd`
(128 counted slots + the out-of-table `0x5D110`); plus the `ret`.

### P2.3 The dead and reachable edge cases

**`0x80E0C[128] = 0x57E3C` is dead.** The mirrored RLE renderer's literal index
is the control byte itself, and a literal control byte is 7-bit (`b < 0x80`;
0x57FAA `call [eax*4+0x80E0C]` after 0x57F93 `test al,al; jns`), so index 128 is
unreachable there; the clipped mirrored renderer's straddle path passes an
overhang remainder (`neg edx`, 0x581AC/0x5817C), which is at most the run
length, 127. The slot exists physically because the 129-slot walk continues
past `0x80E0C`'s last reachable entry: `0x8100C` is not a known table base (the
next table starts at `0x81010`), so `0x8100C` reads as `0x80E0C[128]` and holds
the code pointer `0x57E3C`. `0x57E3C` is a complete 128-literal backward writer
(32 `bswap` dwords) that no read reaches. E2 triaged it anyway (it is a table
slot): `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md` lists `57E3C`
as a `span-writer`.

**`0x81110[128] = 0x81310[0] = 0x5D110` is reachable.** The forward literal
table's counted walk stops at 128 slots because `0x81310` is itself a direct
displacement (`mov ebx,dword ptr [eax*4 + 0x81310]` at 0x51E91 and 0x51F07 —
the bank dword the blit reads), but the raw and shear renderers index `0x81110`
with `eax = 0x80` (0x58CE3/0x521A5), i.e. slot 128 = `0x81310`, and call the
code pointer it holds. `0x5D110` is a complete 128-literal forward writer (32
dword stores, no tail), reachable whenever `vis >= 128` (a raw or sheared span
at least 128 pixels wide). It is **not** one of the 385 slots (it is the 386th
writer target), and E2 never triaged it (it is not a table slot).

**The type `>= 0x20` domain exclusion.** `0x51E5C` masks the node type with
`0x7F`, not `0x1F` (0x51EBF), so a type in `0x20..0x7F` indexes 32..127 of the
32-entry renderer table — the fill-backward and literal-backward writer tables
read as code pointers. No `0x14328`-built node has such a type (`0x14268` builds
1, 2, 9, 6 and their clipped forms), and the port's `switch (n->type & 0x1Fu)`
maps everything outside its ten live entries to the original's no-op stub
(`port/src/platform/sprite.c`). The census's renderer count is therefore over
the 32 slots, and the domain stays excluded from any claim.

**The bank-0 policy.** The bank table is 256 dwords at `0x81310`
(`mov ebx,dword ptr [eax*4 + 0x81310]`, 0x51E91/0x51F07, `eax` = the node's
bank byte, `[pal_ptr + 8]`): entry 0 is the stale code pointer `0x0005D110` and
entry `n >= 1` is `(n - 1) * 0x01010101`. The fill colour table starts at
`0x81314` = bank entry 1 (`mov edx,dword ptr [edx*4 + 0x81314]`,
0x5D26F/0x57FDB), so colour `c` is bank entry `c + 1` (`c * 0x01010101`) —
one region, two bases. `sprite.h`/`sprite.c`'s `sprite_bank_offset` maps bank
byte 0 to offset 0 ("no offset") and does not reproduce the stale dword;
`port/tests/test_platform.c:1729` pins `DSD(0x81310) == 0x0005D110` as a table
entry (`"bank[0] is the stale pointer"`), not as a rendered offset. The census
reports `0x5D110` as a reachable *writer*, not as a bank value.

### P2.4 The E2 triage split

The committed E2 table lists **231 `span-writer` rows**. Intersecting them with
the 385 writer slots gives **227 triaged** and **158 untriaged**:

- the four rows that are `span-writer` but not writer slots are the renderers
  whose only slot is a span table: `0x5215C` (table `0x80C8C[4]`), `0x57F80`
  (`[9]`), `0x57FFB` (`[25]`), `0x58CBD` (`[2]`/`[18]`); 231 - 4 = 227;
- `0x57E3C` (the dead writer) **is** triaged (it is a slot);
- `0x5D110` is **not** triaged (not a slot, §P2.3);
- `0x51E58` (the `ret`) and the other renderers `0x5D218`/`0x5D28F` are not E2
  rows (a `ret`-only body and bodies with no table slot).

The E2 universe is **not** extended: `--expect 579 --expect-u0 575` stays
byte-identical (NG5). The 158 untriaged writers are a named gap of this phase:
the span sweep proves the port's behaviour for them, not their E2 class.

### P2.5 The live/dead pair in `0x24B08..0x24C5C`

Ghidra's index has `0x249F0` (size 280, so it ends at `0x24B08`) and the next
entry `0x24C5C`; the 340-byte gap between them holds **three** occupants:

| range | what | evidence |
|---|---|---|
| `0x24B08..0x24B50` | a **dead** code block | preceded by the `ret` at `0x24B07` (the tail of `0x249F0`), then 3 bytes of `lea eax,[eax]` padding (`0x24B51..0x24B53`) |
| `0x24B54..0x24B89` | the **live** hook `0x24B54` | installed by `0x25AE8` at `0x25B97` (`mov edx,0x24B54` at 0x25B8D, `mov [0x104AE4],edx` at 0x25B97), called through the ported `fn_resolve(DS_00104AE4)` dispatcher |
| `0x24B8C..0x24C5B` | a **52-entry** dword table | read by `jmp dword ptr cs:[eax*4 + 0x24B8C]` at 0x24F01, bounded by `cmp ax,0x33; ja` at 0x24EF2 |

`0x24B08` is dead by four independent facts: no dword anywhere in the
fixup-applied image equals `0x24B08` (so no fixup record targets it either — a
fixup would have written that value into a dword), no `rel32` call/jmp reaches
it, it is not in `port/decomp/prage.functions.csv`, and the instruction before
it is `0x24B07`'s `ret` (so no fall-through enters it). It is a partial
copy of the inline block at `0x24F09`: the same config-field decode
(`config_field_get(0x29)`, `mov eax,0x29; call 0x2D974`) and the same four
stores — `DS_00104528` (the decoded value), `DS_00105B3A`
(`(value & 0x100) >> 4`), `DS_0010452C` (`(value & 0xF0) >> 4`),
`DS_001088D0` (`(value & 0xF) * 5 + 0x1E`) — with a different register
allocation, and without the inline block's `mov dword [DS_00104AB8],1`, `call
0x257A4` (`game_coin_divert`) and `jmp 0x2540F` tail. It is **not** ported (the
plan's Task 7 adds a `fn_resolve(0x24B08) == NULL` negative test).

Notation: those stores are `DS_00104528`, `DS_00105B3A`, `DS_0010452C`,
`DS_001088D0` — runtime linear addresses (`0x00104528` etc.; `DS_001088D0`'s DS
offset is `0x88D0`). Earlier notes that wrote `0x888D0` or `0x104528` as a "DS
offset" mix the two spaces; `symbols.h`'s `DS_` name is the runtime linear
address, and the port reads them as `DSD(DS_001088D0)`.

`0x24B54` is live, ported, registered and capture-exercised:
`game_hook_24b54` (`port/src/game/flow.c`, `fn_register(0x24B54u,
game_hook_24b54)` in `port/src/game/actors.c`), stored only by `0x25AE8`'s
`DS_00104B1D != 0` arm. `DS_00104B1D` (`b1d`) takes 01, 02, 03 and 04 in six
gp captures: `gp-u8-left-training` and `gp-u8-right-training` (01),
`gp-u8-tug-of-war` (02), `gp-u8-endurance` and `gp-u12-endurance` (03),
`gp-u8-handicap` (04) — read from each `poll.log`'s `b1d=` field. So the census
must not be read as "only one body in `0x24B08..0x24C5C`": the gap holds a dead
block, a live hook and a switch table.

### P2.6 The census test and its mutation proof

`tools/tests/test_diff_verify.py`'s `SpanCensusTests` (module constants
`SPAN_*`) decodes the five tables from the image `diffrun --exe ... --image-out`
writes in `setUpClass` (it does **not** read `PRAGE.EXE` at import time), and
asserts:

1. `test_the_five_tables_and_their_slots` — 417 slots, 392 distinct, the 32
   renderer slots with the seven distinct targets (`0x51E58`, `0x5D218`,
   `0x58CBD`, `0x5215C`, `0x57F80`, `0x5D28F`, `0x57FFB`), the 25 duplicate
   renderer slots, and `0x51E58`'s byte is `C3`;
2. `test_the_writer_slots_and_the_dead_and_reachable_edges` — 385 writer slots,
   385 distinct, 384 reachable + the dead `0x57E3C`, and
   `0x81110[128] == 0x81310[0] == 0x5D110` outside the 385;
3. `test_the_class_census_is_27_classes_26_reachable` — the 27-key set, its
   per-class counts and representative targets, the dead class
   `lit_bwd/chunk128`, 26 reachable;
4. `test_the_e2_triage_split_is_227_and_158` — 227/158, the four renderer rows,
   `0x57E3C` triaged, `0x5D110` not;
5. `test_the_class_key_is_the_code_shape` (capstone-gated) — each class's slots
   share one store-width signature (dword runs collapsed; the fills' word tail
   at 14/15; the literals' word tail forward vs byte tails backward; the
   backward literal dwords `bswap`ped), and the tail-only runs' four shapes per
   table.

The test skips without `build/diffrun` or `PRAGE.EXE` (unless
`PR_ORACLE_REQUIRED=1`). Its assertions were each proved able to fail by a
temporary mutation, then restored:

| mutation | test that failed |
|---|---|
| `SPAN_CLASS_COUNTS[("fill_fwd","wordtail2")] 13 -> 12` | class census |
| `SPAN_DEAD 0x57E3C -> 0x57E3D` | writer/dead/reachable edges |
| `SPAN_SHAPES[("fill_fwd","wordtail2")] "dw" -> "dbb"` | class key is the code shape |
| the 227 -> 228 in the E2 assertion | E2 split |
| `SPAN_RENDERERS` loses `0x57FFB` | five tables |
| `span_class`'s fill word-tail threshold `n >= 14` -> `n >= 18` (the key itself) | class key is the code shape (`fill_fwd[14] 0x585F0`: got `dw`, want `dbb`) and the class census |

After the restores the class passes 5/5 (clear `tools/tests/__pycache__` after a
restore: an identical-size rewrite inside the same second can leave Python
running the mutated bytecode).

**Plan-vs-measurement corrections recorded here:**

- The plan's class key lists "the 14/15 word tail" and "the 128 chunk" without
  saying they are per-table: the measurement gives **two** word-tail classes per
  fill table (`wordtail2` for `n%4==2, n>=14`, `wordtail3` for `n%4==3,
  n>=15`) and **two** 128-chunk classes (forward reachable, backward dead). That
  is what makes the count 27 (4x5 + 4 + 2 + 1), not 25. The 27th class's
  forward entry (`0x5D110`) is the 386th writer target, outside the 385 slots —
  the plan's "385 writers (384 reachable + 1 dead)" counts slots, and the
  "26 reachable" class count includes the out-of-table forward chunk. Both are
  stated above so later tasks do not have to re-derive the asymmetry.
- The plan says `0x24B08` is "a dead duplicate of the inline block at
  `0x24F09`". Measured: it duplicates the block's **head** (the config-field
  `0x29` decode and the four stores) and lacks its `DS_00104AB8` store and its
  `0x257A4`/`0x2540F` tail; the mnemonics differ by register allocation.
- The plan's capture arithmetic ("35 expected; 34 present") holds, but
  `data/k11-captures/` holds five extra directories the 35 do not name
  (`de`, `diags`, `idle`, `gp-idle-loss-run2`, `gp-u6-moves`), so a count of the
  directory listing is not the gate's inventory (§P1.3).

---

## P3 — The minimal class-complete span fixture (Task 2)

Task 2 turns the census of §P2 into a proof: three new `render_list_blit`
cases draw one RLE row forward, mirrored and as a raw row, and a coverage test
runs the original through them with the writer rows' **representatives** as
`real` calls and asserts the arrived target set is exactly one target per live
class.

### P3.1 The fixture

`tools/diff_verify.py` (`D5B_M_*`, the `m0`..`m2` cases of
`render_list_blit`). One 85-pixel row in **56 bytes**:

```
00 | 01 13 | 02 14 14 | 03 15 15 15 | 04 16x4 | 05 17x5 | 06 18x6 | 07 19x7
80 21 | 81 22 | 82 23 | 83 24 | 84 25 | 85 26 | 86 27 | 87 28 | 8E 2F | 8F 30
```

Literals 0..7 and fills 0..7/14/15 sum to 28 + 57 = 85 pixels. Generated by
`bytes.fromhex` from the brief's stream, length-checked (56 bytes, 85 pixels
decoded).

Two sprite ids, in **entry 2's payload** (`D4B_RES2 = 0x10AA00`, so the
handles' index is not the constant 1); the fixture's own data pokes stay in
`0x10AA00..0x10AAE0`:

| id | descriptor | header | pixels |
|---|---|---|---|
| `0x0230` | `0x01000000` | width 85, height 1, origin 0,0 | handle `0x01000010` → the 56-byte stream at `0x10AA10` |
| `0x0231` | `0x01000050` | width -128, height -1 (raw), origin 0,0 | handle `0x01000060` → 128 bytes `0x50..0xCF` at `0x10AA60` |

The palette entry's bank byte is `0xFE` (offset `0xFD`), as in b8/b9: every
dword bank add overflows, so a byte-wise add cannot pass. The 128 raw source
bytes are `bytes(range(0x50, 0xD0))`; with `+0xFD` and the running carry every
byte's sum overflows, so the four bytes of a group are not independent adds.

The three cases:

| case | psets | renderer | arrivals |
|---|---|---|---|
| `m0` | `0x0230` at (10,2) layer 0; `0x0230` at (10,99) **layer 1** | type 1 (RLE), type 5 (`ret`) | 13 |
| `m1` | `0x8230` at (84,2) | type 9 (RLE mirrored) | 12 |
| `m2` | `0x0231` at (10,2) | type 2 (raw, 128 px) | 1 |

The hflip id's `0x14268` origin rewrite (`xorg = width - xorg - 1 = 84`) puts
the mirrored span at x 0..84; the forward span is x 10..94; the raw row is
x 10..137 (the buffer seed covers 0..127, the rest is zero BSS on both sides).
`d5b_bl` gained optional `sprites=`/`more=` parameters so the fixture reuses
its buffer, row-offset, palette and shear pokes.

### P3.2 The 37 writer targets and the 28 allow entries

Measured from the fixup-applied image and confirmed by the original's recorded
arrivals. The three cases reach **37 distinct writer slots** (the ruling R1
figure):

| table | run lengths | targets |
|---|---|---|
| `fill_fwd` | 0..7, 14, 15 | `0x58550 0x58554 0x5855C 0x58564 0x58570 0x58578 0x58580 0x5858C 0x585F0 0x58600` |
| `fill_bwd` | 0..7, 14, 15 | `0x521DC 0x521E0 0x521E8 0x521F0 0x521FC 0x52204 0x52210 0x5221C 0x52284 0x52294` |
| `lit_fwd` | 0..7 | `0x58CBC 0x58D14 0x58D20 0x58D2C 0x58D40 0x58D4C 0x58D60 0x58D74` |
| `lit_bwd` | 0..7 | `0x52980 0x52984 0x52990 0x529A0 0x529B4 0x529C4 0x529DC 0x529F8` |
| `lit_fwd/chunk128` | 128 | `0x5D110` (`0x81110[128] == 0x81310[0]`) |

plus the renderer table's bare `ret` `0x51E58`, already an allow entry
(38 arrivals in all). **`0x57E3C` is not added**: it is the dead
`lit_bwd/chunk128` slot and no read reaches it (§P2.3).

`D5B_BLIT_ALLOW`'s existing 13 writer entries:
`0x521F0 0x5222C 0x529A0 0x529C4 0x52A18 0x58564 0x58578 0x5859C 0x58D20
0x58D2C 0x58D40 0x58D4C 0x58D90`. The fixture's 37 reach **9** of them
(`0x521F0 0x529A0 0x529C4 0x58564 0x58578 0x58D20 0x58D2C 0x58D40 0x58D4C`);
the other four (`0x5222C`, `0x52A18`, `0x5859C`, `0x58D90`, run lengths 8/13
the b* cases draw) are not reached by it. So **28** entries are new, not the
plan's 24, and the ruling's "the existing 13 overlap" is off by four: the
overlap is **9 of 13**, measured. The allow tuple is 51 distinct entries.

### P3.3 The fourth pset (plan correction)

The plan's step 2 lists **three psets** and step 1 claims they reach all 26
live classes. Measured: they reach **25**. The 27th class's reachable half,
the renderer table's bare `ret` (`0x51E58`), is selected by a *node type*
(`0, 3, 5, 0xA, 0x15, 0x1A, 0x1F`), not by a writer-table run length: no
control byte in the stream can reach it. Without it the arrived set is 37
writer targets = 25 classes, short of the 26 the design claims.

The fix is a fourth pset in `m0`: the same `0x0230` sprite at (10, 99) with
**layer 1**, which sets the type's bit 2 → type 5 → `0x51E58`. The layer-1
rule takes y from `0x107A4E + (0xF0AEC >> 6)`, so `m0` carries b5's layer-1
globals (`g3e=0, g4e=0, aec = d5b_at_px(3, 0xD56) << 6`) and the ret node
lands at (10, 3): a **different row** from the forward span, so `@t5` (the
mutant that draws a `ret` slot) is caught by `m0` and does not reproduce the
normal pset's bytes. The node draws nothing (the `ret`), as in b0/b5. The
arrivals are unchanged in count (13 in `m0`), and the 37 writer targets are
untouched.

### P3.4 The coverage test

`tools/tests/test_diff_verify.py`, `SpanCensusTests.
test_the_minimal_fixture_reaches_every_live_class` (`@needs_unicorn`; it
reuses the census class's dumped image):

1. the three cases must exist (`m0`, `m1`, `m2` in order);
2. the 26 **representatives** of the live classes (`SPAN_CLASS_REP` minus the
   dead class) are the run's `calls=` (mode `real`), so every arrival is
   recorded; `allow_calls=spec.allow_calls` keeps the non-representative run
   lengths (1..3, and the already-drawn 8/13) legal;
3. each case must end `ok`, and the union of arrivals must equal the 26
   representatives;
4. the arrived targets' classes (from the image's own tables, via
   `span_class`) must be exactly the 26 live classes.

The run is **fail-closed**: a payload edit that changes a run length arrives at
a target outside the representatives, which is allowed (no stop) but not
recorded, so assertion 3 fails; an edit that leaves the allowed set (a run
length 9, say) stops the run as `unmodeled`, so assertion 3's outcome check
fails. The claim is 26/26 classes, 37/37 fixture targets, no dead target.

`make diff-verify` and the census class pass. The pin test
(`RealFunctionTests.test_each_d1_mutant_is_caught_by_what_it_breaks`) was
re-measured (see §P3.5).

**RED.** With `m0`'s case id renamed to `x0` (the tree the test was written
against: the cases do not exist):

```
AssertionError: Lists differ: ['m1', 'm2'] != ['m0', 'm1', 'm2']
FAILED (failures=1)
```

**Class-drop mutation.** Two mutations were run and restored:

1. the plan's literal removal of `8E 2F`: the row decodes 71 pixels and reads
   on into the zero gap, where an unmodelled code pointer is reached —
   `AssertionError: 'unmodeled' != 'ok' … m0: indirect call to 0x5B410 at
   0x5D23E`. The test fails, but as a stop rather than a named class.
2. the same-length rewrite `8E 2F` → `87 2F 87 2F` (58 bytes, still 85
   pixels): the `fill_fwd/wordtail2` and `fill_bwd/wordtail2` classes are no
   longer reached, and the test names them —

```
AssertionError: Lists differ: [336516, 361968] != [] …
missing classes: fill_bwd/wordtail2 (0x52284), fill_fwd/wordtail2 (0x585F0)
```

After the restore the class passes 6/6 (clear `tools/__pycache__` and
`tools/tests/__pycache__` after a restore, as in §P2.6).

### P3.5 The re-measured mutant pins

`D5B_CASES`/`D1_CASES` (the exact case set that catches each
`render_list_blit` mutant) re-measured from the harness's own problems, all 11
mutants still `MISMATCH`:

| mutant | cases |
|---|---|
| `@bank` | b0..b9, m0, m1, m2 (was b0..b9) |
| `@rowtab` | b0..b9, m0, m1, m2 (was b0..b9) |
| `@t5` | b5, m0 (was b5) |
| `@nomirror` | b0, b1, b6, b9, m1 (was b0, b1, b6, b9) |
| `@clipb`, `@noshear`, `@t0a`, `@lr`, `@notop`, `@nomirrorclip`, `@stride` | unchanged |

`@bank`, `@rowtab` and `@nomirror` are still caught (the plan's step 4
condition); no mutant's pin lost a case. `@stride` is not caught by the
fixture (its sprites are one row tall, so the 320/321 stride never changes a
destination) and keeps its b0..b9 pin. No tail/carry mutant exists in this row
to add a case for; the bank-carry behaviour is exercised by the byte
comparison (`m0`..`m2` all draw with bank `0xFE`, and `@bank` now fails in all
three).

---

## P4 — The exhaustive span-writer and both-edges fixture (Task 3)

Task 3 keeps §P3's class proof and extends the same `render_list_blit` row to
every reachable writer target, then covers the two clipped renderers'
both-edges entries. The target arithmetic follows ruling R9: **385 writer
slots**, plus the reachable out-of-table `0x5D110`, make **386 distinct writer
targets**; the arrived set is **385 = 384 reachable slots + `0x5D110`**, and the
one absent target is the dead `0x57E3C`. The plan's `384/385` is the slot-only
reading, not a reason to omit the 386th target.

### P4.1 The committed generator and the measured stream

`d5b_x_rows()` in `tools/diff_verify.py` is the committed generator. It packs
literal runs 0..127 followed by fill runs 0..63, whole and in that order, into
320-pixel rows, then pads each row with transparent runs of at most 63 pixels.
Literal `n` carries bytes `0x40 + ((n+i)&0x3F)`; fill `n` carries colour
`0x40+n`. With bank byte `0xFE` (offset `0xFD`), every drawn pixel exercises
the dword bank carry. The 37 rows are concatenated in `D5B_X_STREAM`.

Measured from that function's own output:

```
rows                 37
bytes                8434
sha256               96d6ddfe14b1aec2dbcac9cb0e7b590806060d54c26971685a16494bf4fc5a11
unpadded run bytes   8384 (8256 literal + 128 fill)
transparent controls 50 (8434 - 8384), 1696 transparent padding pixels
```

Reproduce from the repo root:

```bash
python3 - <<'PY'
import hashlib
import tools.diff_verify as V
print(len(V.D5B_X_ROWS), len(V.D5B_X_STREAM))
print(hashlib.sha256(V.D5B_X_STREAM).hexdigest())
PY
# 37 8434
# 96d6ddfe14b1aec2dbcac9cb0e7b590806060d54c26971685a16494bf4fc5a11
```

The coverage test pins all three of rows, byte length and hash. As a negative
check, changing only `D5B_X_STREAM`'s final byte in memory made it fail with
actual hash `a1cc60f26e56a834e3b8fb7c007ca765dd30f625fc9684c22da389ad64d6255a`
against the pin above; restoring it made the test green again.

### P4.2 Resource, list and destination layout

The fixture uses preloaded resource **entry 3**, whose record starts at
`0x10A63C`: its payload pointer at `0x10A64C` is `0x100000`, and
`d4b_at(0x018xxxxx)` resolves into that payload. The earlier two entries and
all old cases are unchanged apart from carrying this additional resource
record.

| object | handle | resolved address / extent |
|---|---|---|
| RLE descriptor | `0x01800000` | `0x100000` |
| raw descriptor | `0x01800010` | `0x100010` |
| clipped descriptor | `0x01800020` | `0x100020` |
| 8,434-byte RLE stream | `0x01802000` + row offset | `0x102000..0x1040F2` (end exclusive) |
| 128 raw bytes `0x50..0xCF` | `0x01804100` | `0x104100..0x104180` |
| 28-byte, two-row clipped stream | `0x01804200` | `0x104200..0x10421C` |
| list nodes | — | `0x0F0000` |
| psets | — | `0x0F1000` |
| back buffer | — | `0x0F2000` |

The list placement is clear of the layer-1 global `0xF0AEC`. Every used row-table
entry is `y*320+0x40`; `d5b_x_rowtab()` packs those entries into 64-byte pokes.
The destination is otherwise left at the image's zero bytes (`seed=False`), so
no large buffer-seed pokes hide the fixture's own writes. The transparent runs
have no writer target; every nonempty literal/fill run draws nonzero
bank-adjusted output.

### P4.3 Why the differential cases are split (R10)

`diffrun`'s `case_t` has **40 pokes of 64 bytes**
(`port/tests/diff_runner.c`). One 8,434-byte stream needs 132 data pokes before
its descriptor/list pokes, and the 128-pset raw list alone needs 80 packed
pokes. Raising that limit or adding a generated-poke protocol would be the
harness rewrite the spec's NG6 excludes. The fixture therefore preserves the
exact generated stream and sweep but partitions their differential execution:

- **x00..x36:** one row per case, 37 cases. Each has a height-one, width-320
  descriptor pointing at that row's offset in the one concatenated stream, and
  two psets: normal `0x0210` at `(0,0)` and hflip `0x8210` at `(319,100)`. The
  normal pset is at `(0,0)`; the hflip origin rewrite gives `xorg=319` and
  `node.x=0`. The union covers every literal run 0..127 and fill run 0..63 in
  both directions. These cases carry 21..26 pokes each.
- **r0..r3:** 32 psets per case, 128 in all. The raw descriptor is width `-128`,
  height `-1`; item `k` is at `x=-k, y=k` (`k=0..127`), hence `vis=128-k` and
  literal-forward writer indices 1..128, including `0x81110[128] = 0x5D110`.
  Each case carries exactly 40 pokes.
- **c0:** the 18 clipped psets in one two-row case, 32 pokes.

The coverage test rejects any new case with more than 40 pokes, so a later
fixture edit cannot silently fall off the C parser's end. This partitioning is
not a claim that one 37-row sprite was executed in a single case: multi-row
RLE sequencing remains covered by the existing b* cases and by c0's two rows,
while the exhaustive claim is the generated stream identity, per-row byte
equivalence and arrived-target union.

### P4.4 The 18 both-edges clipped psets

`D5B_X_CLIP_STREAM` is two identical 14-byte rows. Each row's fill lengths are

```
16, 63, 63, 63, 63, 46, 16       # 330 pixels, colours 0x41..0x47
```

The descriptor is width 330, height 2. For every `a=1..9`:

- normal pset `0x0212` is at `x=-a`, `y=2*(a-1)`;
- hflip pset `0x8212` is at screen `x=329-a`, `y=18+2*(a-1)`; its origin rewrite
  gives `xorg=329`, hence `node.x=-a` as required.

Both have `clip_l=a` and `clip_r=10-a`, so both edges are nonzero and the visible
width is 320. The leading and trailing 16-pixel fills have visible lengths
`16-a` and `6+a` (swapped by hflip), covering 7..15 and specifically the 14/15
word tails. The 18 psets occupy distinct pairs of destination rows.

The original run executes **`0x5D580`** (normal clipped RLE's both-edges entry)
and **`0x582F4`** (mirrored clipped RLE's both-edges entry). The coverage test
asserts both instruction addresses, and also records the four writer arrivals
that prove the clipped paths, not merely the unclipped fixture, reach the
word-tail bodies: forward `0x585F0`/`0x58600` (14/15) and backward
`0x52284`/`0x52294` (14/15). The test also pins all 18 ids and coordinates;
duplicating the `a=1` normal pset in place of `a=2` fails that assertion.
Removing the nine hflip psets in memory made the test fail with `0x582F4` in
`missing both-edges renderer path`; restoring them made it green.

### P4.5 The measured arrived set and the allow list

The coverage test decodes the five tables from the fixup-applied image, derives
`targets = (the 385 writer slots) | {0x5D110}`, checks `len(targets) == 386`,
and expects `targets - {0x57E3C}` exactly. It runs x00..x36 and r0..r3 with all
385 expected writers as `real` recorded calls. Measured result:

```
writer slots                  385
distinct targets              386 (385 slots + 0x5D110)
expected reachable targets    385 (384 reachable slots + 0x5D110)
arrived targets               385
missing targets               none
dead 0x57E3C arrivals         none
both-edges entries            0x5D580, 0x582F4
clipped 14/15 arrivals        0x585F0, 0x58600, 0x52284, 0x52294
```

`D5B_SPAN_REACHABLE` is the explicit 385-address form of that measured set.
`D5B_BLIT_ALLOW` is now **395 distinct entries = ten non-writer entries + 385
writers**, up from 51; the 344 added entries are all reached by the fixture.
The test also intersects the allow list with the image's 386 targets and
requires that intersection to equal the arrived set, so neither an unreached
writer nor the dead writer can be added silently.

The RED/GREEN and mutation evidence is:

1. **RED before the fixture:** the new coverage test failed its first assertion
   with `Lists differ: [] != ['x00', 'x01', ... 'x36']` (37 missing cases).
2. **GREEN:** after the fixture landed, the focused test passed in 0.747 s;
   `SpanCensusTests` then passed 7/7 in 0.795 s. The real row passed
   **55 cases, 30/30 blocks, VERIFIED**.
3. **One target drop:** in x36, replacing fill 63's two bytes `BF 7F` with
   transparent 63 + transparent 0 (`FF C0`) preserves the byte count and pixel
   count but removes that writer in both directions. The test failed with
   `missing targets: 0x52948, 0x58C84`; restoring `BF 7F` made it green.
4. **Clip drop:** removing c0's nine hflip psets failed on missing `0x582F4`.
5. **Stream identity:** the final-byte mutation in §P4.1 failed the sha256 pin.

No case exposed a port divergence: all 55 real cases compare byte-exactly.

### P4.6 Re-measured `D1_CASES` and the unchanged gate

The exact mutant-catching case sets were re-measured from
`verify_all(..., mutants=True)`'s own `problems`, not inferred from the fixture:

| mutant | measured case set |
|---|---|
| `@bank`, `@rowtab` | b0..b9, c0, m0, m1, m2, r0..r3, x00..x36 |
| `@nomirror` | b0, b1, b6, b9, m1, x00..x36 |
| `@lr` | b1, b4, c0 |
| `@nomirrorclip` | b2, b3, b4, c0 |
| `@stride` | b0..b9, c0 |
| `@clipb`, `@noshear`, `@t5`, `@t0a`, `@notop` | unchanged |

All 11 mutants remain `MISMATCH` with kind `byte`. The focused pin check was
green for all 11; removing c0 from `@lr`'s expected set then failed with
`['b1', 'b4', 'c0'] != ['b1', 'b4']`, proving the new pin fails in the tested
direction. The one-row x and raw cases do not catch `@stride`; c0's second row
does.

Final Task 3 verification:

```
$ make diff-verify
Ran 224 tests in 149.221s
OK
render_list_blit | 0x14328 | 55 | 30/30 | VERIFIED
... 1031/1031 functions VERIFIED; 14983/14983 mutants detected; 0 named gaps;
813/844 rows with callees closed (187 have none); 806/844 closed on full rows;
8 ported functions with no row ...

$ python3 tools/port_progress.py
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)

$ make entry-triage
Ran 44 tests ... OK
entry-triage: 579 candidates (575 by U0's rule); ... span-writer=231 ...
```

The differential counter is byte-identical to §P1.2, the E2 table is unchanged,
and no portable function or capture/pixel/state pin moved.

---

## P5 — The two ported card steps' rows (Task 4)

`flow_card_ramp_step` (`0x25FAC`) and `flow_bonus_card_b_step` (`0x26194`) are
update-table entries the port implements, opens with `PR_SEAM0`, and had no row;
each was a blocker in a comment on `game_frame_head`'s row. Task 4 gives both a
row and adds no port code (`git diff` touches only the harness and the record).

### P5.1 The raw bytes

Both disassembled from the fixup-applied image (`./build/diffrun --exe
data/game/C/PRAGE.EXE --image-out`, base `0x10000`); Ghidra has no function for
either address, so these are the scan's own leaders.

`0x25FAC..0x25FD9` — `push edx; mov eax,[0x104acc]; inc byte [eax+0x2d]; xor
edx,edx; mov dx,[eax+0x2c]; cmp edx,0x1000; jl 0x25FD8; mov dh,[0x104ae8]; and
dh,0xbf; mov word [eax+0x2c],0x1000; mov [0x104ae8],dh; pop edx; ret`. Three
leaders (`0x25FAC`, `0x25FC3`, `0x25FD8`). The increment is a byte at `+0x2D`,
i.e. `+0x100` added to the word at `+0x2C` with **no carry out of it**
(`0xFF10 -> 0x0010`); the compare is on the zero-extended word (`mov dx`,
`cmp edx,0x1000; jl`), so every word `0x1000..0xFFFF` takes the clamp — the
signed-negative `0x8000..0xFFFF` included, because the 16-bit subtraction
`edx - 0x1000` is `0x7000..0xEFFF` (SF = 0, OF = 0) and `jl` is not taken. The
clamp loads the byte, clears bit `0x40` and stores it back (a read-modify-write,
not a blind `and` of a constant). EDX is pushed/popped and EAX is overwritten.

`0x26194..0x26253` — `push ebx; push ecx; push edx; mov al,[0x104b0f]; cmp
al,1; jb 0x261AE; jbe 0x261F4; cmp al,2; je 0x26226; ...` Twelve leaders:
`0x26194`, `0x261A0`, `0x261A2`, `0x261AA`, `0x261AE`, `0x261B6`, `0x261DA`,
`0x261F4`, `0x26206`, `0x26226`, `0x2623E`, `0x26250`. **Plan conflict: the
plan says the row "should be 8/8 blocks".** The scan's measured total is 12
(each state's test, each return-only arm and the shared `pop/pop/pop/ret` tail
is its own leader); the row is 12/12. The plan's number is recorded here and in
the task report, and the measured one is pinned. The state word is the byte
`0x104B0F`: 0 adds `0x80` to the record's word `+0x2C` (`0x261BB..0x261CA`, a
16-bit store; the `jl` on the zero-extended word as above), at or above
`0x1000` stores `0x1000`, `0x104B10 = 0x3C` and state 1; 1 decrements the timer
(`dec dl`, signed `jg 0x26250`) and at or below 0 stores state 2 *then* calls
`0x2BC30(card, 0xE91D4, 1.0)` (the float is pushed, `ret 4`); 2 subtracts
`0x200` (`sub edx,0x200`, the word stored, unsigned `ja` on `dx`), at zero
stores 0, calls `0x2B150(card)` and clears `0x104AEA` bit `0x01`; anything else
returns at `0x261AA`. Both callees are recorded stubs: `0x2BC30` takes
EAX/EDX/`[esp+4]` (the frame bits) and pops 4, clobbering EDX; `0x2B150` takes
EAX and preserves the rest (the `_CALLEE_CLOBBER_FIXES` entry, record §C2b.3).

### P5.2 The cases and the measured coverage

`flow_card_ramp_step` (mask 0: the `0x24CEF` dispatcher reloads EAX):

| case | record word `+0x2C` | after the increment | expected |
|---|---|---|---|
| p0 | 0x0EF0 | 0x0FF0 | below the clamp: no write to `+0x2C`, `0x104AE8` unchanged |
| p1 | 0x0F00 | 0x1000 | at it: word 0x1000, `0x104AE8` 0xFF -> 0xBF |
| p2 | 0x7F10 | 0x8010 | zero-extended, still clamps: word 0x1000, `0x104AE8` 0x7F -> 0x3F |
| p3 | 0xFF10 | 0x0010 | `+0x2D` wraps, no carry into `+0x2E`: no clamp |

Every case seeds `+0x2E`/`+0x2F` (`aa bb`) and `0x104AE8`, so a carry out of
`+0x2D` and a wrong bit are both visible. 3/3 leaders hit.

`flow_bonus_card_b_step` (mask 0, the same dispatcher), 10 cases:

| case | state `0x104B0F` | word | timer | expected |
|---|---|---|---|---|
| q0 | 0 | 0x0F00 | 0x55 | 0x0F80 < 0x1000: return, no write |
| q1 | 0 | 0x0F80 | 0x00 | 0x1000: clamp, word 0x1000, timer 0x3C, state 1 |
| q2 | 0 | 0xFFF0 | 0x55 | 0x0070: the truncated word, no clamp |
| q3 | 0 | 0x7F90 | 0x00 | 0x8010: zero-extended, still clamps |
| q4 | 1 | 0x1234 | 0x02 | timer 1 > 0: return |
| q5 | 1 | 0x1234 | 0x01 | timer 0: state 2, `0x2BC30(card, 0xE91D4, 1.0)` |
| q6 | 1 | 0x1234 | 0x00 | timer 0xFF: signed-negative, the same arm |
| q7 | 2 | 0x0300 | 0x00 | 0x0100 != 0: return, word stored |
| q8 | 2 | 0x0200 | 0x00 | 0: word 0, `0x2B150(card)`, `0x104AEA` 0xFF -> 0xFE |
| q9 | 0x80 | 0x0300 | 0x00 | above 2: return, state unchanged |

12/12 leaders hit, nothing named unhit.

### P5.3 The mutants and the measured pins

Ten mutants, each a plausible porting bug; the kind set and the exact cases
that alone catch it were measured from `verify_all(..., mutants=True)`'s own
`problems` (not predicted):

| mutant | defect | kinds | cases |
|---|---|---|---|
| `flow_card_ramp_step@nocarry` | the word incremented as a dword (the carry reaches `+0x2E`) | byte | p0, p3 |
| `flow_card_ramp_step@signed` | the clamp test on `(s16)word`, so 0x8010 returns | byte | p2 |
| `flow_card_ramp_step@bit` | clears bit 0x04 (`and 0xFB`) not 0x40 | byte | p1, p2 |
| `flow_bonus_card_b_step@carry` | state 0's compare on the untruncated sum | byte | q2 |
| `flow_bonus_card_b_step@signed` | state 0's compare signed | byte | q3 |
| `flow_bonus_card_b_step@timer` | the timer test `>= 0` instead of `> 0` | byte, call #0 | q5 |
| `flow_bonus_card_b_step@utimer` | the timer test unsigned (`t != 0`) | byte, call #0 | q6 |
| `flow_bonus_card_b_step@state` | `st >= 2` where the raw tests `st == 2` | byte | q9 |
| `flow_bonus_card_b_step@bit` | clears the arm byte's bit 0x80 instead of 0x01 | byte | q8 |
| `flow_bonus_card_b_step@dead` | state 2's zero end skips `0x2B150` | call #0 | q8 |

The pin is `PC_MASKS`/`PC_KINDS`/`PC_CASES` in
`tools/tests/test_diff_verify.py`, asserted by
`test_each_pc_mutant_is_caught_by_what_it_breaks` (which also pins each row's
`hit == total` and an empty `unhit`).

RED/GREEN, each measured on the test the pin lives in:

1. **Removing p3** from the ramp spec: the focused test failed
   `AssertionError: Lists differ: ['p0'] != ['p0', 'p3']` for `@nocarry`, while
   the row stayed 3/3 — the case pin is load-bearing on its own.
2. **Removing q9**: the focused test failed on `@state` (`'byte'` lost) — q9 is
   the only case that reaches the `st > 2` arm; and the coverage assertion
   fails too: with q9 dropped the row measures `('PARTIAL', 11, 12,
   ['0x261aa'])`, not `('VERIFIED', 12, 12, [])`.
3. **A faithful `@dead`** (`D1C_CB_MUT(dead, D1C_CB_NONE)`, rebuilt): the
   focused `--self-check` printed `flow_bonus_card_b_step@dead: NOT DETECTED
   (no eax or byte difference, and no call difference on any case)` and the pin
   failed with `'call #0'` missing. Restored, it is detected on q8 again.

No case exposed a port divergence: every real case compares byte-exactly on the
first run.

### P5.4 The re-measured counter and the blocker change

`make diff-verify` (exit 0, `Ran 225 tests ... OK`), verbatim:

```
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps;
814/845 rows with callees closed (188 have none); 807/845 closed on full rows;
8 ported functions with no row (45 FN_ ported-unrowed: 8 portable
[5 registry-visible + 3 header-shape exceptions] + 37 runtime; the scan-visible subset is 42 = 5 + 37).
```

Against §P1.2: functions 1031 -> 1033, mutants 14983 -> 14993 (+3 + 7), the
callee-less rows 187 -> 188 (the ramp row is a leaf), rows with callees 844 ->
845, closed 813 -> 814, full 806 -> 807. **The design's §7 arithmetic predicted
`14990` mutants and `825/845` closed — both are wrong**; the measured line is
the pin (the design calls its own figures "approximately ... the arithmetic is a
check, not a target"). `game_frame_head`'s two e2-field blockers `0x25FAC` and
`0x26194` now read `stub VERIFIED` in its row's callee column; the row stays
open on the longjmp stub `0x65431` alone (Task 5's scan-only stop), and its
`33/161` coverage and every other pin are unchanged.

The focused commands (Task 4 step 3) print:

```
| flow_card_ramp_step | 0x25FAC | 4 | 3/3 | VERIFIED | - |
| flow_bonus_card_b_step | 0x26194 | 10 | 12/12 | VERIFIED | 2B150 stub VERIFIED, 2BC30 stub VERIFIED |
```

(the callee column reads `unverified` under `--function`, which runs no other
row — record E3 §E3.8).

`python3 tools/port_progress.py` stays `799 1203 66` / `759 759 100` and
`make entry-triage` stays `579 candidates (575 by U0's rule); ... span-writer=231 ...`:
the task adds no `FN_` function, no `fn_register` and no capture/pixel pin.

---

## P6 — The sprintf allow and the scan-only longjmp stop (Task 5)

Task 5 closes twelve rows without adding a case, a portable function or a capture:
`text_number_core` closes **full** by running the real sprintf tree, and the ten
`game_frame_*` rows plus `game_quit_prompt` close **weak** by declaring the
unreached longjmp only to the static scanner.

### P6.1 `text_number_core`: the existing cases prove the real formatter

The old row stubbed `0x65546` and made the stub write `42\0`. The replacement is
`allow_calls=D3A_SPRINTF`, with no `0x65546` call-set entry. `D3A_SPRINTF` moved
earlier in `tools/diff_verify.py` only because this C3G row is constructed before
the D3A rows; its nine addresses are unchanged:

```
0x65546 0x6C8EB 0x6CB6C 0x6CC9F 0x6CE84
0x6CCFA 0x65533 0x72D20 0x72CD6
```

The raw's format is present and exercised, so no new format case is needed:
the fixup-applied image holds `25 69 00 00` (`"%i\0\0"`) at `0x80B40`, and the
raw instruction at `0x2EF2D` calls sprintf with that string. All three existing
cases (`n0`/`n1`/`n2`, value 42, three destinations seeded with `A5`) ran the
original formatter and the port's `snprintf` path to byte-identical output. A
pre-change one-off `dataclasses.replace(calls=(), allow_calls=D3A_SPRINTF)` run
and the committed row both measured:

```
| text_number_core | 0x2EF24 | 3 | 1/1 | VERIFIED |
problems: []
unhit: []
```

`--function text_number_core --self-check` measured **4/4 mutants detected**.
The exact kinds and catching case sets below come from `verify_all(...,
mutants=True)`'s own `problems`, not from the old stub pins:

| mutant | measured kinds | cases | representative difference |
|---|---|---|---|
| `@val` | `byte` | n0,n1,n2 | original digit `0x32`, port `0x33` (the mutant formats 43) |
| `@dest` | `byte` | n0,n1,n2 | original writes at EDX, mutant at EDX+4 |
| `@call` | `byte`, `eax` | n0,n1,n2 | original returns 2 and writes `42\0`; mutant returns 8 over the seeded bytes |
| `@len` | `eax` | n0,n1,n2 | original 2, mutant 3 |

Thus `@val` moved from a stub-call difference to a real output-byte difference,
and `@call` lost its call-list kind. The case sets did not move. The pins live
in **`C3G_KINDS` / `C3G_CASES`** in the current file (the brief's "C3F" names
the historical batch, not these dictionaries); C3F itself is unchanged. The
EAX mask stays `0xFFFFFFFF`, the exact real-name and mutant-name sets are
unchanged, and `0x65546` left the stub-clobber dictionary. The stale
`actors.c` comment now says that D3A runs this tree in the existing rows and in
`text_number_core`; only the comment changed, not the C formatting path.

RED/GREEN: before the row change, the new allow test failed with
`spec.allow_calls == ()` against the nine-entry tuple; after it, the same test
passes and also requires the row to be full and closed. As a pin mutation,
restoring `@val`'s expected kind to `{'call #0'}` made
`test_each_c3g_mutant_is_caught_by_what_it_breaks` fail with `byte` in the
actual set; the measured pin was then restored.

### P6.2 Zero arrivals at `0x65431`, measured before and after the move

Before removing the stub, a one-off run used its recorded arrival as the
instrument: every original case of all eleven specs ran with its existing
`case_calls`, and the run counted both a recorded `0x65431` call and an executed
`0x65431` instruction. Measured:

```
game_frame_keys       9 cases, ok 9,  no arrivals
game_frame_keys2      3 cases, ok 3,  no arrivals
game_frame_head      10 cases, ok 10, no arrivals
game_frame_fight     17 cases, ok 17, no arrivals
game_frame_tails     11 cases, ok 11, no arrivals
game_frame_m25       11 cases, ok 11, no arrivals
game_frame_match     12 cases, ok 12, no arrivals
game_frame_front     14 cases, ok 14, no arrivals
game_frame_start     12 cases, ok 12, no arrivals
game_frame_rr         3 cases, ok 3,  no arrivals
game_quit_prompt     14 cases, ok 14, no arrivals
TOTAL: 11 rows, 116 cases, recorded_arrivals=0, executed_arrivals=0
```

The same measurement after the move again gave `outcomes={'ok': 116}` and zero
recorded/executed arrivals. This is the evidence for removing the declaration;
it is not an inference from the `unhit_named` text. Those named blocks stay:
`0x25201` for the game-frame rows and `0x24A9C` for the quit prompt.

### P6.3 `Spec.stop` is scanner-only and fails closed at runtime

`Spec` gains `stop: tuple = ()`. `verify_gap` and `spec_original` pass
`[k.addr for k in spec.calls] + list(spec.stop)` to `E.static_scan`. Nothing
else consumes it: not `case_calls`, `cases_text`, `run_original`,
`SpecResult.callees`, or `closed_rows`. Consequently a stop address limits
recursive descent but is neither an allowed call nor a recorded stub/real
callee. The default is empty and every other spec is unchanged.

`D5H_LONGJMP` is now the integer `0x65431`, with `D5H_STOP = (0x65431,)`.
`D5H_GF_COMMON` lost its former `E.Call`, and `d5h_gf_spec` supplies `stop` to
exactly the ten game-frame rows. `D6A_PROMPT_CALLS` lost the same `E.Call`, and
only `game_quit_prompt` uses that tuple and `D5H_STOP`. Measurements over
`V.SPECS` found 11 stop specs, no other `D5H_GF_COMMON` user outside those rows,
no `D6A_PROMPT_CALLS` user besides the prompt, and no remaining `0x65431` call
declaration; no tuple split was needed.

The scan measurements show why both pieces are necessary:

| row | old call-set stop | longjmp removed, no `stop` | scan-only `stop` |
|---|---:|---:|---:|
| `game_frame_head` | 161 leaders / 618 instructions | **170 / 656** | **161 / 618** |
| `game_quit_prompt` | 18 / 84 | **27 / 122** | **18 / 84** |

All scans are untruncated. The no-stop scans add exactly the nine runtime
leaders `0x65431`, `0x6544F`, `0x65450`, `0x65468`, `0x6546A`, `0x65475`,
`0x65477`, `0x65482`, `0x65484`; the stop scans contain no leader at or above
`0x5D000`. The regression test checks both sides of that table, so deleting the
stop cannot preserve a green claim.

The fail-closed guard starts a synthetic original run at the restart block
itself. The raw bytes are:

```
0x25201  mov edx,1
0x25206  mov eax,0x1044F4
0x2520B  jmp 0x65431
```

With the old stub this returned `ok` and recorded
`0x65431(0x1044F4, 1)` — the RED result. With the new spec's calls it executes
`0x25201` and `0x65431`, records no call, and stops:

```
outcome = unmodeled
 detail = indirect call to 0x6C8EA at 0x6543A
  calls = []
```

`0x6543A` is the runtime's `call dword ptr [0xF0900]`. This is an unmodelled
call, not a returning stub, which is the required behavior if a future case
reaches the restart arm. The quit arm is the corresponding
`0x24A9C..0x24AB0` block (resume, save, then the same tail jump). Because that
reachable block remains unexercised, `0x249F0` joined `WEAKER`; the ten
`0x24C5C` rows were already weak. The stub-clobber dictionary therefore lost
`0x65431` as well as `0x65546`. Injecting both addresses as synthetic stubs
made the dictionary test fail; the injection was confined to the one-off
process and restored.

### P6.4 The twelve closed rows and the measured counter

The focused real run reported all twelve `VERIFIED`, with no unnamed unhit
blocks and no `0x65431` callee. `text_number_core` is 1/1 and full; every
game-frame row keeps its previous hit count out of 161, and `game_quit_prompt`
is 16/18 with its two existing named unhit blocks. The
closure assertions name all twelve rows and require the eleven restart rows to
be weak. This closes one full row and eleven weak rows, not twelve full rows.

The direct full self-check and the final `make diff-verify` both exited 0 and
printed, verbatim:

```
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps; 826/845 rows with callees closed (188 have none); 808/845 closed on full rows; 8 ported functions with no row (45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape exceptions] + 37 runtime; the scan-visible subset is 42 = 5 + 37). The closure rule (record J §J1): an allow-mode callee outside the portable set - runtime >= 0x5D000, host-owned/deferred in tools/port_classification.txt, or an address the decomp index never lists as a function - does not keep a row open, provided the port tree does not implement the address in any class (no fn_register, no column-0 `/* 0xADDR` header), while a recorded stub or real callee and a ported function without a row do. Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
```

Against §P5.4: functions 1033 -> 1033, mutants 14993 -> 14993, callee-less
rows 188 -> 188, rows with callees 845 -> 845, closed 814 -> **826**, full
807 -> **808**, open 31 -> **19**, and `WEAKER` 12 -> **13**. The expected
intermediate counter is therefore measured, not fitted. Moving the counter pin
one step down to 825 made its test fail against the actual 826 line; the pin was
restored to 826.

Verification totals:

```
python3 -m unittest tools.tests.test_diff_verify
Ran 145 tests ... OK                         # includes the three RED/GREEN tests

make diff-verify
Ran 228 tests ... OK                         # test_diff_emu + test_diff_verify
... 826/845 ... 808/845 ... exit 0

python3 tools/port_progress.py
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)

make entry-triage
Ran 44 tests ... OK
entry-triage: 579 candidates (575 by U0's rule); ... span-writer=231 ...
```

No real-name, EAX-mask, case-set, pixel/state, capture or E2 pin moved. The only
mutant-kind changes are the three measured `text_number_core` entries above;
the only stub-clobber removals are the two addresses whose stubs were deleted.

---

## P7 — The short write and the declared differences (Task 6)

Task 6 covers `0x1B084`'s last block with a sixth, terminal case and adds two
rows to `WEAKER` (one of them a row the counter had counted full). It adds no
portable game code and no capture: `git diff` touches `tools/diff_emu.py`,
`tools/diff_verify.py`, `tools/tests/test_diff_verify.py` and this record.

### P7.1 The short-write model

`Machine.short_write = (name, n)` (default `None`) makes each write to the one
declared file `name` come back after at most `n` bytes. In `_F1Files._fwrite`
the request's total is still the callee's `imul ebx,edx` (0x62056); the model
then writes the request's first `min(total, n)` bytes at the stream's position,
does **not** extend the file over the rest of the request, advances the position
by what landed, and returns the whole items written, `written // size` — which is
the callee's item count, not a byte count. With `short_write` absent (every other
case and every other row) `written == total` and `total // size == count`, so the
existing behaviour is byte-identical; `sc_save("s5", ...)` is the only case that
sets it. `Machine.__post_init__` rejects a declaration that names no file, names
a file the case does not declare, is not a `(name, count)` pair, or has a count
outside `0..F1_MAXFILE` (0x800): a mis-declared short write must fail at
construction, not silently write the whole request.

The raw bytes that need it (fixup-applied image, disassembled at base 0x10000):

```
0x1B0CA  bb f8 07 00 00     mov ebx,0x7f8      ; the item count
0x1B0DC  ba 01 00 00 00     mov edx,1          ; the item size
0x1B0E1  e8 4b 6f 04 00     call 0x62031       ; fwrite
0x1B0E6  89 c2              mov edx,eax        ; the returned count
0x1B0EA  e8 d1 6d 04 00     call 0x61ec0       ; fclose
0x1B0EF  81 fa f8 07 00 00  cmp edx,0x7f8
0x1B0F5  74 0e              je 0x1B105
0x1B0F7  b8 01 00 00 00     mov eax,1
0x1B0FC  b2 01              mov dl,1
0x1B0FE  e8 00 6f 04 00     call 0x62003       ; the exit chain, code 1
0x1B103  eb 06              jmp 0x1b10b
```

The scan's eight leaders are `0x1B084`, `0x1B0A2`, `0x1B0BA`, `0x1B0CA`,
`0x1B0F7`, `0x1B105`, `0x1B109`, `0x1B10B`; before the case the row was 7/8 with
`0x1B0F7` the one unhit leader (the old `unhit_named` reason said why: the
modelled fwrite never came back short).

### P7.2 The `s5` case and the block

`s5` writes its own buffer image (`sc_image(7, 3, bad=0x33)`; no byte of it is
compared, the case being terminal), but its fwrite answers
`0x7F7` of the `0x7F8` bytes — `SC_LEN - 1`, the row's own buffer length minus
one (the same one-byte-short shape as the load row's `l6`, record §G2). The case
is `exits=True` (`E.Machine(exits=True)`), `short=SC_LEN - 1` and
`terminal=("exit", 1)`; the row's `allow_calls` gained `SC_EXIT` and its call set
gained `SC_FLUSH` (`E.Call(0x6C4CF, (), clobbers=("ecx","edx","ebp"))`), since the
exit chain's flush is reached from `0x6271B` and cannot run (its stream table at
`0xF0A0E` holds live close pointers). Measured from the harness's own run:

```
s5 terminal   ('exit', 1)   exits True   short_write (b'cmos', 2039 = 0x7F7)
outcome       exit          detail terminate with code 1 at 0x62723
executed      0x1B0F7 in the set            (the arm, not the fall-through path)
calls         [0x2D4EC, 0x2D4EC, 0x6C4CF]   (two touches, then the flush)
file_opens    [(b'cmos', b'wb')]
row           VERIFIED, 6 cases, 8/8 blocks, no unnamed unhit
```

Because the case declares its terminal, §G1's skip applies: its port-side
values, bytes and calls are compared against nothing, so the arm's exit is
proved and the port's declared failed-write flag path (config.c, PORT) is not
claimed to agree. The mutants do not see the case either: `@flag`'s measured
catching set stays `s0..s4` (a terminal case is skipped before `compare`).

### P7.3 The reopen: the accepted trade

The row now declares `SC_FLUSH` (`0x6C4CF`), a **recorded stub** at a runtime
address. The closure rule excuses an allow-mode runtime callee but not a recorded
stub (§J1), and `0x6C4CF` has no row of its own, so the row leaves the closed
figure. Measured from `closed_rows` over the full run:

```
config_file_save callees    10D0C allow, 10D34 allow, 2D4EC stub VERIFIED,
                            61A70 allow VERIFIED, 62002 allow, 62003 allow,
                            6201B allow, 65490 allow, 654C7 allow, 6C4CF stub
unverified, not excused     [0x6C4CF]      (the only blocker)
row weaker                  True            (0x1B084 stays in WEAKER)
```

So the phase trades a covered block and a closed weak row for a covered block
and an open row with one named blocker. `0x1B084` becomes one of the 20 open
rows the terminal census (Task 7) records, beside `config_file_load` and
`menu_fatal_error`, which were already open on the same flush.

### P7.4 The weaker reclassifications

`WEAKER` gains `0x51ED8` and `0x1D0BC` (`0x249F0` joined in Task 5), 13 -> 15
entries, and the comment now reads "poked/partial/terminal **or a declared byte
difference**":

- `0x51ED8` (`sprite_blit_aperture`) is a full, closed row whose cases carry
  `H2Case.declared` — the renderers' node scratch `+0x14`/`+0x38`, where the
  original's renderer leaves a value the port's by-value renderers do not
  (record §H.3, `h2_scratch`). A declared difference is not a full row, so the
  full figure moves 808 -> 807 (806 -> 805 on the §P1 baseline) on this one
  address.
- `0x1D0BC` (`sound_buffers_alloc`) was already neither full nor closed: its two
  failure arms `0x1D10E`/`0x1D183` are unhit (they end in the runtime's printf
  `0x62734`) and its row is open on the `0x1C308` stub. It joins the set so the
  classification names it; the counter does not move for it.

### P7.5 The re-measured pins, the counter and the unchanged gates

`SC_KINDS`/`SC_CASES` were re-measured against the harness's own `problems`
(`verify_all(..., mutants=True)` on the row) and are **unchanged** for all 19
mutants: no kind and no case set moved. The call set's order keeps `0x2D4EC` at
`call #0` and `0x6C4CF` at `call #1`; no non-terminal case reaches the flush, so
no kind gained a `call #1`. The row's 19 mutants are still detected 19/19.

The direct `--self-check` run and `make diff-verify` exited 0 and printed (the
counter line wrapped below):

```
$ python3 tools/diff_verify.py --self-check
| config_file_save | 0x1B084 | 6 | 8/8 | VERIFIED |
... 19/19 mutants detected
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps;
825/845 rows with callees closed (188 have none); 807/845 closed on full rows; 8 ported
functions with no row (45 FN_ ported-unrowed: ... 42 = 5 + 37). ...

$ make diff-verify
Ran 231 tests in 148.240s
OK
... 825/845 ... 807/845 ... exit 0

$ python3 tools/port_progress.py
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)

$ make entry-triage
Ran 44 tests ... OK
entry-triage: 579 candidates (575 by U0's rule); ... span-writer=231 ...
```

Against §P6.4: functions 1033 -> 1033, mutants 14993 -> 14993, the callee-less
rows 188 -> 188, rows with callees 845 -> 845, closed 826 -> **825**, full
808 -> **807**, open 19 -> **20**, `WEAKER` 13 -> **15**. The expected figures in
the plan (825/845, 807/845, 20 open, 15 weaker) are therefore measured, not
fitted. No real-name, EAX-mask, stub-clobber, pixel/state, capture or E2 pin
moved; `port_progress` and the E2 table are byte-identical.

The three new tests and their mutation proofs (each mutated, run, and restored):

| test | mutation | failure |
|---|---|---|
| `test_the_save_row_covers_the_short_write_and_reopens_on_the_flush` | `_fwrite` returns `count` (ignores `written`) | `('MISMATCH', 7, 8, [110839]) != ('VERIFIED', 8, 8, [])` — 110839 = 0x1B0F7: the terminal declaration is missed and the arm unhit |
| the same | `s5` built without `exits`/`short` | first assertion: `(None, False, None) != (('exit', 1), True, (b'cmos', 2039))` |
| the same | `SC_FLUSH` dropped from the call set | `Items in the second set but not the first: 443599` (0x6C4CF) |
| `test_the_weaker_entries_name_the_declared_differences` | `0x51ED8` removed from `WEAKER` | `AssertionError: 14 != 15` |
| `test_the_self_check_counts_functions_mutants_gaps_and_closed_rows` | the pin moved 825 -> 826 | the assertion fails against the measured 825 line |
| `test_a_short_write_needs_a_declared_file_and_a_bounded_byte_count` | the whole `short_write` validation block disabled | `AssertionError: ValueError not raised`, on all four sub-cases |

The `s5` case is what proves the model: without it the arm is unreachable, and
the row's own `unhit_named` reason (deleted here) was the evidence that no
earlier case could reach it.

---

## P8 — The terminal census, the divide guard, and the dead/live block (Task 7)

### P8.1 The machine-readable terminal census

The census is data in `tools/tests/test_diff_verify.py`: `TERMINAL_CENSUS`,
`UNROWED_CENSUS` and `WEAKER_CENSUS`. The Markdown tables below are generated
from those objects, not maintained as a second hand-written list. The test
computes `closed_rows` over the real results and compares both the open-row
identities and every row's measured unexcused blockers with the census. It also
requires the 15 weaker keys to equal `WEAKER`, every real row's `weaker` flag to
agree with that set, and the eight portable-unrowed keys to equal `UNROWED`.

There are **20 open rows but 19 distinct original addresses**:
`attract_config_volumes_unscaled` and `attract_config_volumes` are two specs at
`0x2C8F0`. The identity comparison is therefore `(name, entry)`, and the sorted
address comparison retains the duplicate. The 20 comprise 15 AIL-thunk rows,
`sound_buffers_alloc`, `input_sampler`, and the three runtime-flush rows
(`config_file_load`, `config_file_save`, `menu_fatal_error`). "No row" here means
no faithful closing row against the existing port and its declared non-goals;
a harness-authored device or runtime model is not such a row.

Citation abbreviations in the generated tables: `pixel-and-proof` is
`2026-10-22-pixel-and-proof-closure-derivations.md`; `host-input-truth` is
`2026-10-21-host-input-truth-derivations.md`; `reverse-g`, `reverse-h` and
`reverse-n` are the corresponding `2026-10-14` records; `reverse-d6a` is
`2026-10-13-reverse-d6a-derivations.md`; `proof-completion` is this record.

| row | original | blocker(s) | class | why no closing row can exist | evidence |
|---|---|---|---|---|---|
| `snd_music_stop` | `0x1CA6C` | `0x5DEAF` | AIL runtime stub | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `snd_sample_playing` | `0x1CE70` | `0x5DD03` | AIL runtime stub | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `snd_samples_stop_all` | `0x1CD9C` | `0x5DC0F`, `0x5DC8B`, `0x5DD03` | AIL runtime stubs | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `snd_sample_stop` | `0x1CE04` | `0x5DC0F`, `0x5DC8B`, `0x5DD03` | AIL runtime stubs | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `snd_sample_queue` | `0x1CC28` | `0x5DC0F`, `0x5DC8B`, `0x5DD03` | AIL runtime stubs | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `snd_music_playing` | `0x1CA40` | `0x5DEED` | AIL runtime stub | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `input_sampler` | `0x1BBAC` | `0x4FBBB`, `0x4FC05`, `0x4FCFD`, `0x4FDF5` | host-owned PIT/game-port stubs | All 48 sampler blocks are proved with modeled host answers; rows for the four probes would verify harness-owned hardware models, which NG4 excludes. | host-input-truth §H4.2/§H4.5; pixel-and-proof §P7.6 |
| `sound_music_pause_toggle` | `0x1D1B0` | `0x5DEAF` | AIL runtime stub | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `sound_music_volume` | `0x1CAB8` | `0x5DECA`, `0x5DEED` | AIL runtime stubs | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `sound_sfx_volume` | `0x1CED4` | `0x5DCC5`, `0x5DD03` | AIL runtime stubs | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `sound_buffers_alloc` | `0x1D0BC` | `0x1C308` | host-owned audio/allocation stub | The driver/allocation boundary is host-owned and has no portable original-C binding; its recorded stub stays explicit rather than gaining a harness-only row. | pixel-and-proof §P7.6; port_classification 0x1C308 (record §50-D) |
| `attract_config_volumes_unscaled` | `0x2C8F0` | `0x5DCC5`, `0x5DD03`, `0x5DECA`, `0x5DEED` | AIL runtime stubs | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `attract_config_volumes` | `0x2C8F0` | `0x5DCC5`, `0x5DD03`, `0x5DECA`, `0x5DEED` | AIL runtime stubs | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `sound_sample_start` | `0x1CB18` | `0x5DC0F`, `0x5DC2A`, `0x5DC4D`, `0x5DC70`, `0x5DCA6`, `0x5DCC5`, `0x5DCE4` | AIL runtime stubs | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `fight_char_screen_open` | `0x43738` | `0x5DCC5`, `0x5DD03`, `0x5DECA`, `0x5DEED` | AIL runtime stubs | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `fight_char_screen_open_both` | `0x444C8` | `0x5DCC5`, `0x5DD03`, `0x5DECA`, `0x5DEED` | AIL runtime stubs | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |
| `config_file_load` | `0x1AFE8` | `0x6C4CF` | runtime exit-flush stub | The original's short-read exit reaches the runtime flush with live host close pointers; it has no portable row, and l6 already declares the exit-vs-defaults difference. | reverse-g §G2; pixel-and-proof §P7.6 |
| `config_file_save` | `0x1B084` | `0x6C4CF` | runtime exit-flush stub | Task 6's s5 covers the short-write arm but declares its exit difference; the original's exit then reaches the unrowed runtime flush, so the covered row is intentionally open. | proof-completion §P7.2-§P7.3 |
| `menu_fatal_error` | `0x2EA68` | `0x6C4CF` | runtime exit-flush stub | Every case declares the original's exit versus the port's return; the exit chain reaches the same unrowed runtime flush, so no port-side comparison can close the row. | reverse-g §G2; pixel-and-proof §P7.6 |
| `sound_shutdown` | `0x1D018` | `0x5D86A`, `0x5DC8B`, `0x5DD03`, `0x5DEAF`, `0x5DEED` | AIL runtime stubs | The AIL implementation is host-owned (NG4); the runtime thunk has no portable C binding, so a callee row would validate a harness-authored audio model, not game code. | pixel-and-proof §P7.6; proof-completion §P8.1 |

### P8.2 The eight portable unrowed replacements

| original | replacement | why a row would mask the boundary | evidence |
|---|---|---|---|
| `0x1B120` | res_load_index plus res_load_present and the host INDEX loader | The withdrawn poked row assigned 0x1B3AC's read/mark work and an invented frame effect to the port; it compared a composition, not the C function. | reverse-n §N.3.1/§N.4 |
| `0x1B3AC` | res_load_present's head, res_load_index's host reads/marks, and res_resolve's mark | No single C function spans the raw body; the port adds the ISR read-stall model and omits the raw driver stores, so a row would bind a synthetic composition. | reverse-n §N.3/§N.3.5/§N.4 |
| `0x1BEC4` | game_main, game_init, the host boot loaders, and game_shutdown | The boot chain has no one C counterpart and immediately reaches the BIOS query; a row would compare a harness-assembled boot against host startup. | reverse-n §N.1 |
| `0x1C930` | the host music-request/sequencer path (title_music_start) | The raw's AIL handle gates, copies, stops and clear have no port counterparts; a row would replace them with fabricated handle state. | reverse-n §N.2.3 |
| `0x1CF40` | game_audio_init's host audio objects, mixer reset and FAT.OPL load | The raw returns and stores ten AIL handles while the port deliberately stores different sentinels and does extra host work; a row would mask those differences. | reverse-n §N.2.2 |
| `0x20C10` | game_init, game_init_resume, and game_main's game_loop/restart call | The raw setjmp frame is split across those functions; entering only the resume tail lacks its stack frame, so no faithful single binding exists. | reverse-n §N.1 |
| `0x47370` | game_string_table_load's host file loader and fixed string handle | The port does not reproduce the raw allocator/pager, language index, free/reload or size stores; a row would validate an invented loader model. | reverse-n §N.3/§N.3.5 |
| `0x4FBA2` | int10h_query's host-window mode answer 0x13 | The raw returns BIOS AL from int 0x10; poking it would compare a harness-chosen constant with the port's constant and prove nothing about the BIOS. | reverse-n §N.1; reverse-d6a §4 |

### P8.3 The 15 weaker rows and why none is full

| original | why it cannot be full | evidence |
|---|---|---|
| `0x1AFE8` | l6 declares the short-read exit-versus-defaults difference; that case compares no port side. | reverse-g §G2 |
| `0x1B084` | s5 declares the short-write exit-versus-failed-write-return difference; its port side is not compared. | proof-completion §P7.2 |
| `0x1B544` | The row covers only the resident arm; the slow tail-jump arm is named unhit (2/11 blocks). | reverse-g §G3 |
| `0x1BE30` | The teardown row pokes the BIOS int 0x10 and leaves three 0x1ADE4 blocks unhit (5/8). | reverse-n §N.2.1 |
| `0x1C740` | The movie row pokes runtime/Smacker entry points and covers only the port's prefix (4/23 blocks). | reverse-n §N.3 |
| `0x1D0BC` | The two buffer-failure arms 0x1D10E/0x1D183, ending in runtime printf, remain unhit. | proof-completion §P7.4 |
| `0x249F0` | The reachable restart arm 0x24A9C remains unexercised after the longjmp became scan-only. | proof-completion §P6.3 |
| `0x24C5C` | The key rows omit the Alt-J/calibration path through 0x5004A and the restart arm by design. | reverse-h §H.2 |
| `0x255CC` | The loop row leaves the full-copy and second-RNG blocks 0x25680/0x256D4 unhit (18/20). | reverse-g §G4 |
| `0x2CADC` | Three raw calls the port deliberately omits are nop pokes; only the seven calls the port makes are compared. | reverse-d6a §4 |
| `0x2EA30` | The SET-lock clear block 0x2EA49 is named unhit (7/8 blocks). | reverse-g §G3 |
| `0x2EA68` | Every case declares exit-versus-return, so the row proves the original but compares no port side. | reverse-g §G2 |
| `0x2EA74` | The key wait leaves the 0x5004A calibration path and two other blocks named unhit (20/23). | reverse-h §H.2 |
| `0x2EA78` | The key wait leaves the 0x5004A calibration path and two other blocks named unhit (20/23). | reverse-h §H.2 |
| `0x51ED8` | The aperture row declares the renderer node-scratch +0x14/+0x38 byte difference (h2_scratch). | proof-completion §P7.4; reverse-h §H.3 |

### P8.4 The divide-fault census and the one guard

All addresses below were re-read from the fixup-applied image written by
`build/diffrun`; the two `idiv ebx` instructions in mode 0x12 are at **0x42203
and 0x4221C** (not at the C assignment's start or end):

```
421FC  29 ca           sub edx,ecx
421FE  89 d0           mov eax,edx
42200  c1 fa 1f        sar edx,0x1f
42203  f7 fb           idiv ebx
...
42217  89 d0           mov eax,edx
42219  c1 fa 1f        sar edx,0x1f
4221C  f7 fb           idiv ebx
```

State 6 computes `step = (word[bg+0x2C] - 0x180) / 64` before starting the
scroll animation. The animation is external INDEX data, so the EXE does not
exclude `step == 0`. `flow.c` now calls, before the corresponding C division,

```
if (step == 0) host_cpu_fault(0x00u, 0x42203u, "divide by zero", 1);
if (step == 0) host_cpu_fault(0x00u, 0x4221Cu, "divide by zero", 1);
```

The first guard is the observable one for this input; the second names the
raw's second site rather than reusing the first address. The literal 1 is
intentional (`SVC_FAULT_EXIT` is file-local to `svcmenu.c`).

The other sites need no code change:

| site | proof and disposition |
|---|---|
| `0x39ADF` (`0x39AC8`, `idiv ebx` by `n*n`) | Ghidra finds exactly two callers, `0x39C0D` (launch, `n = word[0x107A60]`) and `0x39DAD` (fall, `m = word[0x107A70]`). Those words are written only by `0x39F65`/`0x39F6C` in `0x39F40`. The 14 direct `0x39F40` sites and the four forwarding `0x3A2A0` callers below give positive 16-bit `n` and `m`, so the divisor cannot be zero on a shipped call path. No guard added. |
| `0x39C6D` (`0x39B30`, `idiv ebx` by `(s16)(n+m)`) | The same launch-time producer makes both addends positive; the signed-16 sum cannot be zero on those paths. No guard added. |
| `0x38A59` (`0x38A38`, `idiv ecx` by `word[0x107A40]`) | The only writes to that word are `0x3879A` and `0x38948`; both read `0xBDDFC[i]`. `0x38910`'s only caller passes zero, `0x20DF4` clamps its stage to seven, and the mode-0x12/0x13 stage picks/wraps stay in 0..7. The measured words 0..7 are `60 50 74 50 90 A0 40 50` (hex), all nonzero. The loop call at `0x25606` is gated by `0x107A54`, armed only after `0x38730` has loaded the divisor; the other call is `0x387DD` in that setup. No guard added. |
| `0x2E910` (`0x2E5E4`, unsigned `div esi`) | `0x2E248` has one production caller, at `0x32C18`; its argument at `0x32BFF` is `push 0x32640`, and the bytes there are `03 00`. Thus `len = 1`. At `0x2E8D5..0x2E8E3`, `edx = len << 2`, `cmp esi,edx; jae 0x2E8E5; mov esi,edx` makes `unit >= 4*len = 4` before `div esi`. The new unit test below proves that arithmetic through the public formatter/line interface. No guard added. |
| `0x4F55A` in `flow_round_timer_step` (`0x4F4E8`) | Already guarded by `if (DSD(DS_001088D0) == 0u) return;`. The stock divisor is `(v & 0xF)*5 + 0x1E`, hence 0x1E..0x69. The zero-divisor return remains the documented PORT deviation; unchanged. |
| `0x4F5E0` in `flow_bonus_count_step` (`0x4F5C8`) | The same existing zero return and the same nine stock stores' `(v & 0xF)*5 + 0x1E` proof. The row is full on its exercised domain; the zero-divisor deviation stays separately documented, not folded into this guard. |

The `0x39F40` argument census behind the two fighter proofs is (raw call site,
`n`, `m`; the stack word is `m`):

| raw site | n | m | raw site | n | m |
|---|---:|---:|---|---:|---:|
| `0x3AC00`/`0x3AC4A`/`0x3AC89` | 0x0C | 0x14 | `0x3ACC3` | 0x0F | 0x14 |
| `0x3A0D4` | 0x0F | 0x14 | `0x39FE6` | 0x0F | 0x14 |
| `0x3B57C` | 0x0F | 0x14 | `0x21F1A` | 0x1A | 0x24 |
| `0x221EB` | 0x17 | 0x1D | `0x3F2FA` | 0x1D | 0x27 |
| `0x3F5AF` | 0x17 | 0x1D | `0x48429` | 0x0F | 0x14 |
| `0x223D1` (state 6/default/7) | 0x0F/0x0F/0x13 | 0x0D/0x10/0x1E | `0x3A350` (forwards `0x3A2A0`) | see below | see below |

The four `0x3A2A0` callers supply the remaining `(n,m)` pairs: `0x21023`
`(0x12,0x14)`, `0x44DFF` `(0x0F,0x14)`, `0x45039` `(0x18,0x1A)`, and
`0x4522B` `(0x0F,0x14)`. In particular `0x22396/0x223A9/0x223BC` visibly push
`0x10/0x0D/0x1E` and load ECX with `0x0F/0x0F/0x13` before the shared call at
`0x223D1`; these are raw bytes, not values inferred from a C formula.

For the `0x2E910` test, histogram 0 is seeded with only bucket 0 = 1. After
`audit_hist_format(..., 0x32640)`, the test checks the raw label bytes and
`state[+0x20] = 1` (len), with `state[+4] = 1` (the pre-clamp maximum). At
width 22, `bar = 22 - 9 - 1 - 7 = 5`; with `len = 1`, `max = 1`, and the clamped
`unit = 4`, `units = 5 + 2 = 7` and `full = 4`, so exactly one whole `03` cell
is emitted. The exact expected line is `"  0-  9: 1 100% \x03"`. Units 1, 2
and 3 would emit 5, 3 and 2 such cells, so this is an executable proof of
`unit = 4`, not a restatement of the caller comment. Temporarily changing the
clamp to `len*3` made this assertion fail (and temporarily registering
`0x24B08` made its negative assertion fail); restoring both returned the whole
C suite to green.

### P8.5 `0x24B08` is dead; `0x24B54` is live, and `0x24B8C` is a table

The gap between Ghidra's `0x249F0` (which ends at `0x24B08`) and `0x24C5C`
contains three different objects. The dead block is not one function with the
live hook, and the table is not code. The dead block's complete disassembly is:

```
24B07  c3                    ret                         ; end of 0x249F0
24B08  53                    push ebx
24B09  52                    push edx
24B0A  b8 29 00 00 00        mov eax,0x29
24B0F  e8 60 8e 00 00        call 0x2d974
24B14  89 c2                 mov edx,eax
24B16  a3 28 45 10 00        mov [0x104528],eax
24B1B  81 e2 00 01 00 00     and edx,0x100
24B21  89 c3                 mov ebx,eax
24B23  c1 ea 04              shr edx,4
24B26  83 e3 0f              and ebx,0xf
24B29  88 15 3a 5b 10 00     mov [0x105b3a],dl
24B2F  8d 14 9d 00 00 00 00  lea edx,[ebx*4]
24B36  25 f0 00 00 00        and eax,0xf0
24B3B  01 da                 add edx,ebx
24B3D  c1 e8 04              shr eax,4
24B40  83 c2 1e              add edx,0x1e
24B43  a2 2c 45 10 00        mov [0x10452c],al
24B48  89 15 d0 88 10 00     mov [0x1088d0],edx
24B4E  5a                    pop edx
24B4F  5b                    pop ebx
24B50  c3                    ret
24B51  8d 40 00              lea eax,[eax]                ; three-byte alignment pad
```

This duplicates the head of the inline block at `0x24F09`: field `0x29` and the
same four stores, with different register allocation. It lacks the inline
block's `DS_00104AB8 = 1`, `call 0x257A4`, and `jmp 0x2540F` tail, so "partial
copy" is the accurate correction to the older "duplicate" shorthand. The
runtime names of its stores are `DS_00104528`, `DS_00105B3A`, `DS_0010452C`,
`DS_001088D0`; the last one's DS offset is `0x88D0`, not a raw-file address.

There is no entry path to it:

```
Ghidra get_xrefs_to(0x24B08)                    []
capstone direct control-flow targets 0x24B08   []  (call/jmp/jcc/loop, rel8/rel32)
LE-fixup-applied image dword == 0x00024B08     []
port/decomp/prage.functions.csv entry          absent
preceding instruction                          0x24B07: ret
```

The empty dword scan also excludes a fixup record targeting it: such a record
would have written that value into the fixup-applied image. It has no `FN_`
symbol, no port header or `fn_register`, and no differential spec. The new C
check first runs `actors_init()` and proves the live neighbour resolves to
`game_hook_24b54`, then requires `fn_resolve(0x24B08u) == NULL`; the live
assertion is what makes the negative non-vacuous. A temporary registration of
`0x24B08` failed that check.

The next object is the live hook. Its installation and body are:

```
25B8D  ba 54 4b 02 00        mov edx,0x24b54
25B97  89 15 e4 4a 10 00     mov [0x104ae4],edx

24B54  52                    push edx
24B55  31 c0                 xor eax,eax
24B57  e8 88 a6 02 00        call 0x4f1e4
24B5C  b8 01 00 00 00        mov eax,1
24B61  ba 27 00 00 00        mov edx,0x27
24B66  e8 89 6f 00 00        call 0x2baf4
24B6B  30 e4                 xor ah,ah
24B6D  66 89 15 00 4b 10 00  mov [0x104b00],dx
24B74  88 25 1d 4b 10 00     mov [0x104b1d],ah
24B7A  31 d2                 xor edx,edx
24B7C  88 25 1f 4b 10 00     mov [0x104b1f],ah
24B82  89 15 b8 4a 10 00     mov [0x104ab8],edx
24B88  5a                    pop edx
24B89  c3                    ret
```

`actors.c` registers it as `fn_register(0x24B54u, game_hook_24b54)`. It is not a
Ghidra `FN_` function and has no `SPECS` row, so the `FN_`-based unrowed guard
does not flag it; `ported_unrowed` confirms it is in neither the portable nor
runtime unrowed set. Per R4 that is a record outcome, not a reason to invent a
row. The installation arm is `DS_00104B1D != 0`; the six existing captures
exercise every such value (read-only scans of their `poll.log` `b1d=` fields):

| capture | nonzero `b1d` |
|---|---:|
| `gp-u8-left-training` | 01 |
| `gp-u8-right-training` | 01 |
| `gp-u8-tug-of-war` | 02 |
| `gp-u8-endurance` | 03 |
| `gp-u12-endurance` | 03 |
| `gp-u8-handicap` | 04 |

Finally, `0x24B8C..0x24C5B` is a 52-entry dword table. At `0x24EF2`,
`cmp ax,0x33; ja 0x2540F` bounds the index, and `0x24F01` is
`jmp dword ptr cs:[eax*4 + 0x24B8C]`. Its first four entries are
`0x2540F, 0x2521A, 0x25224, 0x25238`; entries 48..51 are
`0x2519E, 0x251A8, 0x251B2, 0x251BC`. Thus the 340-byte gap is exactly the dead
block, alignment, live hook, alignment, and switch table described in §P2.5.

### P8.6 Tests, mutation evidence, and the unchanged counters

The census test was first run with empty machine tables: the terminal and
weaker assertions failed with `0 != 20` and `0 != 15`; after filling the data,
the three focused tests passed (`Ran 3 tests ... OK`). A synthetic extra open
row (`synthetic_open`, `0xDEAD`) then failed exactly the identity comparison:

```
AssertionError: Items in the first set but not the second:
('synthetic_open', 57005)
```

That mutation was restored. The `0x24B54` test also confirms registration from
the port-tree scan, absence from `FN_`/`SPECS`, and absence from both unrowed
sets; `0x24B08` is absent from the port, index and `FN_` populations.

The fault test seeds `word[bg+0x2C] = 0x180`. One child installs a fault hook
and proves `(exc,eip) = (0,0x42203)`; a second runs without a hook and must
exit 1 with `divide by zero\n`. Before the guard, this host (AArch64, whose
native divide does not trap here) ran on to state 7: both children returned 99,
there was no hook report and no status-1 exit, so the new checks failed. With
the guard they pass. The mutation is therefore proved in the brief's allowed
"SIGFPE or no fault" form; no x86 SIGFPE is claimed for this host.

Final verification after all restorations:

```
$ PR_ORACLE_REQUIRED=1 ./build/run_tests
all checks passed

$ python3 -m unittest tools.tests.test_diff_verify
Ran 151 tests in 147.230s
OK

$ make diff-verify
Ran 234 tests in 181.451s
OK
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps;
825/845 rows with callees closed (188 have none); 807/845 closed on full rows;
8 ported functions with no row (45 FN_ ported-unrowed: 8 portable
[5 registry-visible + 3 header-shape exceptions] + 37 runtime; the scan-visible
subset is 42 = 5 + 37). ...

$ python3 tools/port_progress.py
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)

$ make entry-triage
Ran 44 tests ... OK
entry-triage: 579 candidates (575 by U0's rule); ... span-writer=231 ...
```

Task 6's figures are unchanged: 20 open rows, 15 `WEAKER`, 8 `UNROWED`, and no
new spec or counter movement. No capture, `FN_` function, portable function, or
E2 entry was added; the only production behaviour change is the mode-0x12
zero-step fault.

---

## P9 — Existential gameplay content order (Task 8)

### P9.1 The measured rule and the two cursors

`tools/gp_compare.py` now builds an `md5(port[m]) -> [m]` occurrence map. For a
clean frame it chooses the smallest occurrence whose index is at least `last`.
For splices and transitions it enumerates the model-valid candidates and chooses
the smallest highest exhibited index at least `last`. If the local search has no
feasible candidate, `all_order_candidates` enumerates clean, splice and
transition explanations over the whole dump. A failure reports capture, raw,
kind, `last` and the candidate high indices. The check covers exactly
`[start, first_unexplained)`, excluding all-black frames; band, scan3 and overlay
frames are order-neutral.

The meaningful-contribution test uses the earliest valid splice boundary `lo`
(the model admits `low[:b] + high[b:]` for `lo <= b <= hi`). It is deliberately
not the inverted capture-vs-high test:

```
low contribution:  first_diff(low, high) < lo
high contribution: common_suffix(low, high) < FRAME_BYTES - lo
```

Thus some byte before the boundary differs from the high frame, and some byte
at/after it differs from the low frame. Transition contribution uses its
`from_low`/`from_high` byte counts. A degenerate suffix cannot exhibit a later
frame merely because the two frames share bytes there.

The order pass has its own local-search cursor, advanced with the chosen
minimum-feasible candidate. The older pixel/coverage pass keeps its separate
`p = max(exhibited)` cursor and its reach/coverage behaviour. Sharing that older
cursor is incorrect for repeated screens: it can select a later duplicate and
make a valid earlier occurrence look like a rewind. This separation leaves the
existing pixel and state pins untouched. The docstring now claims content order,
not display order, maps repeats to the earliest feasible occurrence, and calls
mixed allowances neutral; it still does not claim that every port frame appears.

### P9.2 RED/GREEN and mutation evidence

The five requested tests were added before the implementation. The first run
was RED for the intended missing behavior:

```
$ python3 -m unittest tools.tests.test_gp_compare
Ran 74 tests in 1.316s
FAILED (failures=5)
```

The failures were the rewind, occurrence-map, forward-splice, degenerate-splice
and neutral-allowance tests. After the first implementation, the real U22 run
exposed a separate cursor defect. A reduced six-frame A/B fixture was added and
watched fail before the cursor fix:

```
gp_compare: t: order: order failure capture 3 (raw 3): kind=transition last=5 candidates=[3]
```

The fixture has two valid pairs for its first splice; choosing the minimum
feasible `(0,1)` rather than the coverage cursor's later `(4,5)` makes the rest
of the sequence align. After the independent-cursor fix its focused test passed,
and the final full tool suite passed:

```
$ python3 -m unittest tools.tests.test_gp_compare
Ran 75 tests in 1.615s
OK
```

The pre-existing overlay fixture's final `rgb(1)` capture was retained; a later
identical port occurrence was added so that its old capture bytes are valid
under the new content-order claim. No allowance assertion was removed.

The degenerate-splice assertion was also mutation-tested in-process by replacing
`order_splice_exhibited` with the old unfiltered `exhibited` behavior. The mutant
was caught (the test fails when the guard is removed):

```
mutant=unfiltered-splice tests=1 failures=1 errors=0
```

### P9.3 All 24 existing scenario dumps

No capture was made or regenerated. The report-only runs used the existing
`/tmp/pr_gp_dump` dumps for idle-loss, charsel, moves-b, keys-fight, twop and
U12, and `/tmp/opencode/gp_dumps` for the other 18 scenarios. The harness ran
`python3 tools/gp_compare.py --report` for each scenario (four workers), saved
logs under `/tmp/opencode/task8-order-final`, and checked both the process status
and the order line:

```
SCENARIOS=24 ORDER_FAILURE_SCENARIOS=0
```

Every scenario printed `order: 0 failures`; the classified-frame counts were:

| scenario | order-classified frames |
|---|---:|
| `gp-idle-loss` | 7454 |
| `gp-u5-charsel` | 406 |
| `gp-u6-moves-b` | 2042 |
| `gp-keys-fight` | 1024 |
| `gp-twop` | 519 |
| `gp-u12-endurance` | 622 |
| `gp-u8-right-arcade` | 974 |
| `gp-u8-left-training` | 983 |
| `gp-u8-right-training` | 998 |
| `gp-u8-tug-of-war` | 993 |
| `gp-u8-handicap` | 932 |
| `gp-u8-endurance` | 182 |
| `gp-u8-attract-start` | 1001 |
| `gp-u9-win` | 2338 |
| `gp-u10-ending` | 5333 |
| `gp-u13-ending2` | 5786 |
| `gp-u15-join` | 707 |
| `gp-u16-twop2` | 7957 |
| `gp-u18-talon-ending` | 5628 |
| `gp-u19-vertigo-ending` | 5884 |
| `gp-u20-chaos-ending` | 5670 |
| `gp-u21-armadon-ending` | 5822 |
| `gp-u22-diablo-ending` | 5702 |
| `gp-u23-u16-finisher` | 3979 |

The three measured anomalies were then traced through the production helper,
not inferred from the green summary:

| scenario / capture (raw) | local state | full-dump feasible explanation | result |
|---|---|---|---|
| U8 right-training 585 (3336) | clean 392, `last=393` | high 394, exhibits `{393,394}` | chose 394; 0 order failure |
| U9 win 1896 (4508) | clean 1534, `last=1535` | high 1536, exhibits `{1535,1536}` | chose 1536; 0 order failure |
| U22 diablo 5033 (8994) | scan3, `last=4300` | named scan exhibits `{4300,4302,4303}` | neutral; 5034 (8995) clean 4302 passes |

For the two forward splices, the byte measurements at the selected pair were:

```
U8  pair 393/394:  lo=80229  hi=184320  differences before lo=1260  after lo=497
U9  pair 1535/1536: lo=22446  hi=98388   differences before lo=12    after lo=9487
```

Both sides therefore make a meaningful contribution. The capture SHA256s are
`e79b7c7d0921cdf1109f498f0812c147fed4c5e3c313f18c953409de377c315b` (U8 585)
and `447c9a298387205250ab8aee5eaa0dc753e975d5a25f76735a205f6678215606`
(U9 1896). U22's 5033 recipe is the already-pinned scan3 allowance; it does not
advance `last`, so the following clean frame is not falsely called a rewind.

### P9.4 The discarded shared-cursor attempt (exact bytes)

The first implementation reused the legacy classifications and failed U22 at
capture 5429 (raw 12999), rather than adding an allowance. Its exact report was:

```
kind=splice last=4770 candidates=[4671, 4691, 4711, 4731, 4751]
data=[(4670,69699,70554), (4690,69699,70554),
      (4710,69699,70554), (4730,69699,70554), (4750,69699,70554)]
capture sha256=f01a50e04e51173fd86ee37d14d24ccecba7846900b10e47bc4abaecfe0c1f8b
```

For the last candidate pair (4750,4751), the capture and port bytes around the
boundaries were:

```
capture[69680:69720] 301018300404040404041018301018301010283c41793c41793c41794141823c41793c41793c4179
low4750[69680:69720] 301018300404040404041018301018301010283c41793c41793c41794141823c41793c41793c4179
high4751[69680:69720] 300404040404040000000404041018301414243c41793c41793c41794141823c41793c41793c4179
capture[70540:70580] 0c0c1010280c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0000000000000000000c0c0c141424141c2c141c
low4750[70540:70580] 0c0c1010280c0c0c0c0c0c0c0c0c0000000c0c0c0000000000000000000c0c0c101028141c2c141c
high4751[70540:70580] 0c0c1010280c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0000000000000000000c0c0c141424141c2c141c
```

The pair's full hashes are
`a76028e595e9e29e0fecdfc60711e473a8b29da25d5cc55d26cf25182d42ca6d` (4750)
and `bbf2de6ecb0682e637b07f62e3d16c7f5e7ad4f3e24ece8cdf81362a9b041d71`
(4751). The failure was a search-cursor artifact, not a new pixel allowance:
the independent-cursor regression test and all 24 final runs pass with no new
named bytes and no Makefile pin changes.

### P9.5 Scope and unchanged gates

Only `tools/gp_compare.py`, `tools/tests/test_gp_compare.py` and this record
changed. No capture, portable function, `FN_` registration, Makefile pin or E2
entry was added. The final independent checks were:

```
$ python3 tools/port_progress.py
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)

$ make entry-triage
Ran 44 tests in 6.844s
OK
entry-triage: 579 candidates (575 by U0's rule); ...

$ make diff-verify
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps;
825/845 rows with callees closed (188 have none); 807/845 closed on full rows;
8 ported functions with no row (45 FN_ ported-unrowed: 8 portable
[5 registry-visible + 3 header-shape exceptions] + 37 runtime; the scan-visible
subset is 42 = 5 + 37). ...

$ git diff --check
# no output
```

---

## P10 — Coverage pins, K11 identity/order, and allowance honesty (Task 9)

### P10.1 The canonical missing-class rule

The implementation uses the reviewed measurements' exact rule. If `seen` is the
legacy frame claim's exhibited port-index set and `reach` is its highest index,
first form the unexhibited, non-black occurrences

```
M = { m | 0 <= m <= reach, m not in seen, port[m] is not all-black }
```

and then collapse byte-identical members of `M`. The ratchet is the number of
distinct byte strings in `M`; the report names the lowest missing occurrence of
each byte string. An exhibited duplicate does not erase a different missing
occurrence's class. The order pass has no input to this set.

This distinction is load-bearing. A whole-dump class-difference experiment gave
24 for gp-idle-loss and 11 for the front-end, whereas canonicalising the legacy
missing-occurrence set gives the reviewed **29** and **30**. The spec's
"30, not 11" resolves the ambiguity; the 24/11 experiment was discarded, not
pinned. The preliminary 0..64 gameplay range is also not a pin: the implemented
flag measures 72 for U20, and the tool output is authoritative.

`title_compare.canonical_missing` owns the shared calculation. gp supplies its
row hashes to exclude black frames without another full RGB scan; the title and
attract callers test the RGB24 bytes. Attract2 unions the ordinary exhibited set
with `(N, N+1, N+2)` for each validated named splice3 before deriving `reach`, so
classes beyond reach are excluded and the allowance's middle frame is credited.
The claim remains **bounded-missing**, not full coverage: a state the capture
never sampled and a same-count class substitution remain named gaps.

### P10.2 The 24 measured gameplay pins

The measurement used the existing captures and Task 8 dumps only. A temporary
`--max-missing 999999` run elicited each value (never a pin); the final harness
then ran the exact value and `value - 1` with every pre-existing frame, trace,
start, reach, identity and moves pin unchanged. Exact passed all 24; the
one-fewer bound failed all 24 with the same measured count and `N > pinned
N-1`; every exact run also printed `order: 0 failures`.

| Makefile id | scenario | `MAX_MISSING` | exact / `pin - 1` |
|---|---|---:|---|
| `IDLE_LOSS` | gp-idle-loss | 29 | pass / fail |
| `CHARSEL` | gp-u5-charsel | 4 | pass / fail |
| `MOVES` | gp-u6-moves-b | 7 | pass / fail |
| `KEYS` | gp-keys-fight | 9 | pass / fail |
| `TWOP` | gp-twop | 5 | pass / fail |
| `MODES_RA` | gp-u8-right-arcade | 7 | pass / fail |
| `MODES_LT` | gp-u8-left-training | 8 | pass / fail |
| `MODES_RT` | gp-u8-right-training | 5 | pass / fail |
| `MODES_TW` | gp-u8-tug-of-war | 6 | pass / fail |
| `MODES_HC` | gp-u8-handicap | 6 | pass / fail |
| `MODES_EN` | gp-u8-endurance | 7 | pass / fail |
| `MODES_AS` | gp-u8-attract-start | 7 | pass / fail |
| `U12` | gp-u12-endurance | 6 | pass / fail |
| `WIN` | gp-u9-win | 12 | pass / fail |
| `ENDING` | gp-u10-ending | 65 | pass / fail |
| `U13` | gp-u13-ending2 | 59 | pass / fail |
| `U15` | gp-u15-join | 5 | pass / fail |
| `U16` | gp-u16-twop2 | 23 | pass / fail |
| `U18` | gp-u18-talon-ending | 57 | pass / fail |
| `U19` | gp-u19-vertigo-ending | 51 | pass / fail |
| `U20` | gp-u20-chaos-ending | 72 | pass / fail |
| `U21` | gp-u21-armadon-ending | 55 | pass / fail |
| `U22` | gp-u22-diablo-ending | 54 | pass / fail |
| `U23` | gp-u23-u16-finisher | 9 | pass / fail |

The summary was `SCENARIOS 24 EXACT_AND_MUTATION_PASS 24`. Every enforced
Makefile command passes the new flag, including the dynamic U8 and U9/U10
commands. There is deliberately no pin for gp-u14 (absent), gp-u17 (no frame
replay) or gp-pads (smoke). An enforced gp comparison with no `--max-missing`
fails as unpinned; report mode keeps its no-ratchet contract.

### P10.3 Front-end and attract coverage extension

The shared helper fit, so the optional extension shipped rather than leaving the
front-end named gap. The front-end coverage flag requires the independently
pinned window start; no port-derived-start-only coverage claim was added.

| claim | measured pins | exact | one-fewer bound |
|---|---|---|---|
| front-end | `FRONTEND_START=560`, `FRONTEND_MAX_MISSING=30` | `30 missing class(es), pin 30`, rc 0 | `30 > pinned 29`, rc 1 |
| attract prefix | `ATTRACT_MAX_MISSING=74` (both title captures) | both `74 ..., pin 74`, rc 0 | `74 > pinned 73`, rc 1 |
| attract cycle 2 | `ATTRACT2_MAX_MISSING=126` | `126 ..., pin 126`, rc 0 | `126 > pinned 125`, rc 1 |

The front-end window remained `[560..1884]`; the attract windows and existing
`--expect-first 215` / `--min-port-exh 173` claims were unchanged. Attract2 still
has zero unexplained frames and its only splice3 remains capture 3545, cycle-2
2191/2192/2193 at bytes 120000/172800. Its measured reach includes that splice's
three frames and excludes later unexhibited classes.

### P10.4 K11 capture identity and order

The identity is `poll.log` sha256 plus the capture frame count, not a hash of
every pixel. Both values were re-read from the existing captures before pinning:

| scenario | poll.log sha256 | frames | order result |
|---|---|---:|---|
| walk | `aea00a7d523763d1ac4de19f0c5a65d663a8f9409378fde9c1bdf6f4adef391f` | 171 | 0 failures, 51 classified frames |
| menuesc | `aacd32423ac3879739c2f93bf8d87b44c5fb14803b7c69ad712173f5f70f161e` | 766 | 0 failures, 279 classified frames |

K11 calls the same `gp_compare.order_claim` occurrence search, with the two
named walk mid-draw frames order-neutral. A synthetic A/C/B/C rewind fails;
repeated occurrences remain feasible. The existing windows, settled-screen and
index-coverage claims are unchanged (`walk [90..170]`, `menuesc [106..388]`).

Real-capture mutations fail before pixel comparison: a wrong sha prints
`capture: FAIL: poll.log sha256`, changing 171 to 170 frames prints the same
identity mismatch, and omitting either identity option prints `capture identity
is not pinned`. Unit cases cover the same wrong-sha/count/unpinned branches.

### P10.5 Byte-pinned and stale allowances

The full RGB24 capture-frame hashes measured for the named unexplained frames
are:

| allowance | frame | sha256 |
|---|---:|---|
| front-end | 832 | `4918d7e577ec1cfb64ff8a96981c5a2157b33919cf3e5c3bf4956198aa7959c7` |
| front-end | 833 | `c9436f54a95cd503ee3efab6983f64a1eb33228c5520beeaeadecb8a7a51eb07` |
| K11 walk | 138 | `1ec090d071db5fba8e05fcb2ca631d4982a8524306069ffb714eaeac8d3d3e01` |
| K11 walk | 151 | `e1fc034f3bc3c5b64beee1a2e922a00c9b29ea4223372315b20bdb551e96c67a` |

A name inside its claim window now fails if its bytes differ, if the ordinary
model explains it, or if it lacks its required hash. Both consumers of the
front-end names (`--frontend` and `--demo-fight`) use the same audit wrapper;
the demo-fight stale-name regression failed before that wiring and passes the
original capture after it. gp's band, scan3 and overlay `retire the name` notes
are failures too. `K11_ALLOWED_MISSING` fails
when a named missing port frame becomes exhibited or falls outside the claim.
The empty K11-missing and attract2-unexplained tables have synthetic tests that
add a name and watch the stale check fail; the empty sets alone are not evidence.

The real-capture mutation runs used symlinked copies under `/tmp`, never a write
to `data/`. Flipping one byte failed front-end 832 and walk 138 at their sha
pins. Replacing those frames with a clean port frame failed them as stale;
replacing attract2 3545 with cycle-2 frame 2191 failed
`ATTRACT2_SPLICE3_ALLOWED ... clean now; retire the name`. All three gp
allowance kinds also have direct stale-name failure tests, and their existing
byte recipes are unchanged.

### P10.6 Tests and unchanged gates

The RED batches failed first for the intended reasons: gp's canonical/ratchet
and three stale-name cases, K11 identity/order/byte/stale cases, title allowance
helpers, the optional flags, and the mixed-occurrence canonicalisation case.
The final focused run is:

```
$ python3 -m unittest tools.tests.test_gp_compare tools.tests.test_k11_compare tools.tests.test_title_compare
Ran 121 tests in 2.707s
OK
```

No capture was made or regenerated, no portable source changed, and no existing
pixel/state pin moved. Independent checks measured:

```
$ python3 tools/port_progress.py
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)

$ make entry-triage
Ran 44 tests in 8.015s
OK
entry-triage: 579 candidates (575 by U0's rule); ...

$ make diff-verify
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps;
825/845 rows with callees closed (188 have none); 807/845 closed on full rows;
8 ported functions with no row (45 FN_ ported-unrowed: 8 portable
[5 registry-visible + 3 header-shape exceptions] + 37 runtime; the scan-visible
subset is 42 = 5 + 37). ...

$ python3 -m py_compile tools/gp_compare.py tools/k11_compare.py \
    tools/title_compare.py tools/attract_compare.py
$ git diff --check
# no output
```

### P10.7 Full retained-dump target run

The actual Makefile targets (not only direct comparator calls) then ran with
`GP_MODES_KEEP=1 GP_WIN_KEEP=1`:

```
make GP_MODES_KEEP=1 GP_WIN_KEEP=1 \
  gp-oracle gp-charsel-oracle gp-moves-oracle gp-keys-oracle gp-twop-oracle \
  gp-modes-oracle gp-u12-oracle gp-win-oracle gp-ending-oracle gp-u13-oracle \
  gp-u15-oracle gp-u16-oracle gp-u18-oracle gp-u19-oracle gp-u20-oracle \
  gp-u21-oracle gp-u22-oracle gp-u23-oracle \
  k11-oracle frontend-oracle attract-oracle attract2-oracle
# exit 0
```

The complete log is `/tmp/opencode/task9-all-oracles.log`. A read-back asserted
all 24 scenarios' exact `N missing class(es), pin N` and `order: 0 failures`
lines, both K11 identity/order lines, front-end 30, attract 74 on both captures,
and attract2 126; no `improved: lower the pin` line appeared. Dumps were retained.
The companion suites used by those targets also passed independently:

```
$ python3 -m unittest tools.tests.test_gp_modes tools.tests.test_gp_twop \
    tools.tests.test_gp_win tools.tests.test_gp_keys
Ran 103 tests in 1.701s
OK
```

---

## P11 — Strict gate preflight, unit-suite trust, and exact pins (Task 10)

### P11.1 The explicit inventory and strict/partial modes

`CAPTURE_DIRS` is the 35-path list measured in §P1.3, in the same order: the 26
gp-oracle directories, `walk`, `menuesc`, `save-reload`, `gp-pads`, the two
Smacker directories and the three title directories. It does not glob either
capture root. `verify` depends on `gate-check`; direct oracle targets keep their
own absent-capture behavior. `GATE_STRICT` and `PR_PINS_EXACT` are exported, so
recursive makes and the Python comparators receive them.

The real checkout measured:

```
$ make gate-check
gate: captures 34/35 present, 1 missing (strict=1, pins_exact=1)
gate: declared gap (absent): data/k11-captures/gp-u14-win-live
```

(The first explicit exact gate printed the same counts with `pins_exact=1`
while the Makefile default was still 0; the line above is the post-flip default.)
A scratch title-capture root held symlinks to `title` and `frontend` but no
`title2`. Against that root, strict mode printed 33/35, named both the declared
U14 gap and the undeclared `title2` absence, and exited 2 through make;
`GATE_STRICT=0` printed the same inventory plus `partial mode tolerates the
undeclared absence(s)` and exited 0. Adding the present `walk` directory to
`GATE_NAMED_GAPS` failed even in partial mode with `declared gap is present
(stale)`. Thus both failure directions were exercised without writing to
`data/`.

`GATE_ORACLE_ENV` is `PR_ORACLE_REQUIRED=1` in strict mode and `env -u
PR_ORACLE_REQUIRED` in partial mode for the smk/title/attract recursive makes.
The front-end family remains compare-if-present in strict mode and gets the same
`env -u` in partial mode, so a caller-exported value cannot defeat partial mode.
The unit, tool and re-extract commands keep their explicit
`PR_ORACLE_REQUIRED=1`, as the task requires.

### P11.2 Unit-suite trust

The verify unit command, the restart command and both `make test` arms clear all
seven driver selectors with `env -u`; verify alone adds
`PR_FN_MISS_REQUIRED=1`. An actual stray-selector run was a full suite, not the
title driver:

```
$ PR_TITLE_DUMP=/tmp/opencode/task10-stray-title make test
test_title: PR_TITLE_DUMP unset, title dump driver skipped
all checks passed
# exit 0
```

For the miss-log negative, a background loop continuously removed
`frames/fn_miss.txt` throughout `make verify`'s `--check`/unit prelude (a
one-shot watcher lost the race and is not evidence). The check completed, the
unit run failed as required, and make exited 2:

```
FAIL .../test_platform.c:536: PR_FN_MISS_REQUIRED=1 but frames/fn_miss.txt is absent
make: *** [Makefile:2031: verify] Error 1
```

A following `make test`, without `PR_FN_MISS_REQUIRED` and without the file,
printed the absent-log informational line and `all checks passed` (exit 0). The
standalone binary showed the same split before the Makefile wiring: required
exit 1 on the missing file, ordinary exit 0.

The `CHECK(1, ...)` after the `0x129FC` sweep is deleted. Its enclosing test is
not assertion-less: every character/reaction mismatch still executes the real
`CHECK(0, ...)` at the sweep, and the surrounding dust/hit cases retain their
assertions. `tools.tests.test_tick_vector` joined the verify tool command with
`PR_GAME_DIR=$(GAME_DIR)`. With a nonexistent `PR_GAME_DIR` and
`PR_ORACLE_REQUIRED=1`, its six tests ran and failed on the absent EXE instead
of reporting six skips; with the real EXE they ran `......`, `Ran 6 tests`, `OK`.
The verify comment/echo now name `test_fn_misslog` and `frames/fn_miss.txt`, the
entry-triage comment no longer says verify exports `PR_ORACLE_REQUIRED`, and
`make clean` no longer names the unread/unproduced `title_screen_ref.ppm`.

### P11.3 Exact-mode failure branches and their RED/GREEN proof

With `PR_PINS_EXACT=1`, an improvement prints `FAIL` and `re-pin` and returns 1;
with it 0, the prior informational `improved:` behavior remains. The guarded
measurements are:

- gp window start, window reach, frame/trace/moves ratchets, and the canonical
  coverage count; a fully explained ratchet whose N is below the measured end
  also requires re-pinning;
- front-end window END, demo-fight and attract2 ratchets (including their fully
  explained forms), and the shared title/attract coverage count;
- K11 distinct exhibited count and `K11_OPEN_END` END (the two previously
  missing non-exact `improved:` reports were added first);
- attract prefix distinct port-exhibited count;
- gp-keys effects ratchet.

The coverage guards are included because spec §6.3 says every
`improved: raise/lower` branch, including the branches introduced by Task 8.
No comparison threshold changed in non-exact mode.

The first focused run, before implementation, had 14 failures at the new exact
or missing-improvement assertions. After implementation the same four modules
passed 146 tests; they also passed 146 with `PR_PINS_EXACT=1` in the invoking
environment. Their module setup selects partial mode for legacy synthetic cases
with deliberately loose pins, while each exact case opts in explicitly, so the
gate's exported mode cannot make a test depend on its caller. The final focused
run, including the tick vector, was:

```
$ python3 -m unittest tools.tests.test_gp_compare tools.tests.test_title_compare \
    tools.tests.test_k11_compare tools.tests.test_gp_keys tools.tests.test_tick_vector
Ran 154 tests in 3.531s
OK
```

The same 154 also passed with `PR_PINS_EXACT=1` in the caller's environment.
The reachable fully explained demo/attract2 assertions were mutation-tested:
disabling those two guards made both fail (`0 != 1`), restoring the guards made
them pass. The K11 open-end exact failure also asserts that no contradictory
`END ...: ok` line remains.

### P11.4 The explicit exact full gate and default flip

While the Makefile default was still `PR_PINS_EXACT=0`, the complete gate ran:

```
$ PR_PINS_EXACT=1 make verify
gate: captures 34/35 present, 1 missing (strict=1, pins_exact=1)
...
Ran 340 tests in 3.743s                         # k11/gp tools + tick vector
OK
...
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps; 825/845 rows with callees closed (188 have none); 807/845 closed on full rows; 8 ported functions with no row ...
entry-triage: 579 candidates (575 by U0's rule); ...
all checks passed
# exit 0
```

The complete log is `/tmp/opencode/task10-exact-full-gate.log`. A read-back found
no `improved:`, `ratchet improved` or `re-pin` line. Every measured value already
equalled its pin, so **no pixel/state/coverage pin moved and no re-pin commit was
needed**. Only after that green run did the default become
`PR_PINS_EXACT ?= $(GATE_STRICT)`. A post-flip `GATE_STRICT=0 make gate-check`
printed `pins_exact=0`; strict printed 1. No capture was created or modified, and
no portable game source changed.

