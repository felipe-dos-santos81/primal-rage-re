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
