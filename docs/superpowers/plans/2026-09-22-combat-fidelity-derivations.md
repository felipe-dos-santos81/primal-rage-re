# Combat/render fidelity — cycle 3: raw-byte derivation (Task 1)

**Cycle 3** (`docs/superpowers/specs/2026-09-22-combat-fidelity-design.md`, plan
`docs/superpowers/plans/2026-09-22-combat-fidelity.md`) closes the three residual
gaps cycle 2 (`demo-fight-closure`, merged at `a2d7ac3`) left open. This record
is the authoritative value source for Tasks 2–4: every value cites the address,
the capture, the measured run or the command that proves it. On any plan-vs-raw
conflict the raw wins, with the correction and its address recorded (§0.3).

**No porting code is in this task.** The temporary traces the derivation used
were reverted; `git status` is clean.

---

## 0. Addressing, reproduction and corrections

### 0.1 The image and the formulas

`data/game/C/PRAGE.EXE` (read-only). Ghidra address == linear address == object
base + offset. Code object `0x10000`–`0x73B14`, data object `0x80000`–`0x10B0CF`.
`DSB/DSW/DSD(addr)` in the port index the same linear addresses. The raw-file
formulas (`VA + 0x52E54` code, `VA + 0x46E54` data) are **pre-fixup**; every data
address in this record comes from Ghidra (fixups applied) or the port's
`port/decomp/prage.c`.

### 0.2 Reproduction

* **Static**: Ghidra MCP, project `rage`, program `/PRAGE.EXE`
  (`ghidra_connect_instance` with project `rage`); function extents and the
  call graph from `port/decomp/prage.functions.csv` / `prage.calls.csv`
  (Ghidra export, fixups applied).
* **Original, live**: DOSBox-X 2026.08.31,
  `-set "dosbox memory file=/tmp/prage.mem"` (a **live** guest-RAM image; values
  change between reads), the game staged at `/tmp/t5a_stage`. The DOS/4GW
  relocation base is re-derived **per run** by searching the image for the data
  object's `"RAGE.S16"` (data VA `0x8002D`), so
  `data_linear(va) = base + (va − 0x80000)` and a runtime linear address is also
  the memory-file offset. Scripts: `/tmp/t1_pal.py` (the palette table),
  `/tmp/t1_chars.py` (the character/variant), `/tmp/t1_pose.py` (the fighter
  fields). `BP` never fires in this build; `BPLM` works. This run's base was
  `0x266000`.
* **Port**: `cmake --build build`; the front-end dump driver
  `PR_FRONTEND_DUMP=<dir> PR_GAME_DIR=data/game/C ./build/run_tests` writes
  `frame_%04d.raw` (RGB24 through `gfx_dac`) from the state-3 entry on; the demo
  arena's first frame is dumped **481**, the second **482**. The demo oracle is
  `make demo-oracle` (report-only).

### 0.3 Corrections against the plan/brief (raw wins)

1. **The palette divergence is NOT the acquisition sequence.** The spec
   (`2026-09-22-combat-fidelity-design.md:33-38`) and the brief's Step 1 premise
   that "the divergence is in the sequence of acquisitions" is **refuted**. The
   original's and the port's `palette_acquire` sequences are **identical**: the
   same 11 entries, the same order, the same `start`/`len` for every one
   (§1.3). The only difference is the **T-rex's palette handle** — the original
   acquires `0x1BB9FCD8`, the port `0x1BB9FD58` — selected by the palette-variant
   flag `DS_00105B34[0]`, which the front-end dump driver's sentinel seed
   perturbs (§1.4). Evidence: the two tables in §1.3, `0xA8A28`, `0x41350`.
2. **`0x1BB9FD58`'s DAC range is `start=142 len=31` in both.** The port's entry
   is `0x1BB9FD58` at `start=142`; the original's entry at the same `start=142`
   is `0x1BB9FCD8`. `0x1BB9FD58` is **not in the original's table at all** (the
   original never acquires it). The spec's "the original's differs" is true of
   the *handle*, not of the *range*. Evidence: §1.3.
3. **No `0x34B14` handler returns `+0x52` from 9 to 4.** The brief's Step 2
   question ("which of them returns `+0x52` from 9 to 4?") is answered: **none**.
   `+0x52 = 9` is a table no-op (entries 9, 10, 11, 14, 15, 16); the twelve
   handlers write `+0x52 ∈ {0, 5, 9, 0x12, 0x14, 0x15}` and never 4. The writer
   of `+0x52 = 4` is `0x36870`'s `+0x54 == 2` arm at `0x36B91` (§2.3), a callee
   of `0x37178` (a `0x349C8` sub-handler) and of `0x3FD30` — not a table
   handler.
4. **The handler tail's closure is not materially larger than a cycle.** The
   union of the twelve handlers' closure and the `+0x53` machine's closure
   (`0x3531C`, already scoped by cycle 2) is 317 functions / 46 808 B, of which
   the **genuinely-new work is 36 functions / 4 335 bytes** — and beyond
   `0x3531C`'s own closure the handlers add only **10 functions / 1 182 bytes**
   (§2.4). The full closure is inflated by the shared `0x6xxxx` runtime library,
   the RNG, res/render and effects. The brief's stop condition is **not**
   triggered. Evidence: §2.4's tables.

---

## 1. The palette acquisition sequence (Step 1)

### 1.1 The engine is faithful

`palette_acquire` (`0x33754`) is the search/first-free/`start = prev.start +
prev.len`/reflow body the port already carries at `actors.c:251-285`. Its
`palette_record` (`0x33734`) appends `{ptr; first; count; flag}` at the
`DS_00107798` head. The port's body is confirmed faithful to the raw
(Ghidra `FUN_00033754` / `FUN_00033734`); **do not re-derive it**. The engine's
call sites are the six direct callers Ghidra reports for `0x33754`:
`FUN_00011000` (the attract, 8 calls), `FUN_0001c5e8` (the loader glyph, 1),
`FUN_0002a17c` (the character palette, 1), `FUN_0002ae14` (`actor_spawn`, 1),
`FUN_000387f4` (`render_scroll_scene_a`, 4), `FUN_00043818` (4).

### 1.2 The original's arena table, measured

Live guest RAM at the demo's state 7 (`DS_000F0A64 == 7`), base `0x266000`
(`/tmp/t1_pal.py`). The ownership table `DS_00107618..DS_00107798` reads, in
acquisition order (the order is the table order; `start = prev.start + prev.len`):

| idx | handle | rc | start | len | what |
|---|---|---|---|---|---|
| 0 | `0080997C` | 109 | 1 | 1 | the loader font palette (`0x1C5E8`'s own acquire) |
| 1 | `0A838B44` | 9 | 2 | 70 | arena backdrop |
| 2 | `0105FF3C` | 4 | 72 | 48 | scene palette 1 (`0x387F4`) |
| 3 | `00809984` | 1 | 120 | 1 | scene palette 3 (`0x387F4`) |
| 4 | `0080998C` | 55 | 121 | 1 | scene palette 4 (`0x387F4`) |
| 5 | `0A838C60` | 3 | 122 | 20 | arena |
| 6 | **`1BB9FCD8`** | 1 | **142** | 31 | **the T-rex character palette, variant 1** |
| 7 | `0105FEB0` | 2 | 173 | 2 | T-rex sub-palette |
| 8 | **`1BB9FE00`** | 2 | **175** | 9 | **the T-rex dust palette, variant 1** |
| 9 | `10A50F70` | 1 | 184 | 31 | the raptor character palette, variant 0 |
| 10 | `10A51118` | 2 | 215 | 9 | the raptor dust palette, variant 0 |

The scene palette 2 (`0x387F4`'s second acquire, `0x080997C`) is the loader font
palette already at index 0, so it only bumps that entry's refcount (hence
`rc=109`); it does not add a row. The `rc` values are host-timing-dependent (the
run length); the order and the `start`/`len` are not.

### 1.3 The port's arena table, measured, and the one difference

The port's table at the arena's first frame (dumped 482; temporary one-shot dump
in `fighter_state_3531c`, reverted):

| idx | handle | start | len | original | same? |
|---|---|---|---|---|---|
| 0 | `0080997C` | 1 | 1 | `0080997C` | ✓ |
| 1 | `0A838B44` | 2 | 70 | `0A838B44` | ✓ |
| 2 | `0105FF3C` | 72 | 48 | `0105FF3C` | ✓ |
| 3 | `00809984` | 120 | 1 | `00809984` | ✓ |
| 4 | `0080998C` | 121 | 1 | `0080998C` | ✓ |
| 5 | `0A838C60` | 122 | 20 | `0A838C60` | ✓ |
| 6 | `1BB9FD58` | 142 | 31 | `1BB9FCD8` | **✗ handle** |
| 7 | `0105FEB0` | 173 | 2 | `0105FEB0` | ✓ |
| 8 | `1BB9FDD8` | 175 | 9 | `1BB9FE00` | **✗ handle** |
| 9 | `10A50F70` | 184 | 31 | `10A50F70` | ✓ |
| 10 | `10A51118` | 215 | 9 | `10A51118` | ✓ |

**The order and every DAC range are identical.** The only divergence is the
T-rex's two handles: the original's `1BB9FCD8`/`1BB9FE00` versus the port's
`1BB9FD58`/`1BB9FDD8`. So the first call at which the sequences diverge is **not
a `palette_acquire` call at all** — it is the handle the T-rex's
`0x2A17C`→`0x33754` acquire receives.

### 1.4 The owner: the palette-variant flag, set by `0x41350`

The T-rex's handle is `DS_000A8A98[ch][DS_00105B34[side]]` (`fighter.c:185-187`,
`0x29BCD`). `0xA8A98[0] = 0x0A8A28`; the row `0xA8A28` reads
`{0x1BB9FD58, 0x1BB9FCD8, 0x1BB9FC58, 0x1BB9FBD8}` (Ghidra `read_memory`), so
**variant 0 → `0x1BB9FD58`, variant 1 → `0x1BB9FCD8`**. The dust palette is
`DS_000A8AF8[ch]` (variant 0) or `DS_000A8B14[ch]` (variant 1)
(`fight.c:139-142`, `0x29CDC`); `0xA8AF8[0] = 0x1BB9FDD8`, `0xA8B14[0] =
0x1BB9FE00`.

`DS_00105B34[side]` is set by **`0x41350`** (`fight_char_select`,
`port/src/game/fight.c:234-247`), which the state-6 handler `0x11A8C` calls once
per side (`0x11ACD` side 0, `0x11AFE` side 1; `flow.c:821/831`):

```
0x4136D  DSB(0x10816A + side) = C835A[char_index]   ; unless DS_00104B1D == 1
0x41398  DSB(0x105B34 + side) = 0
0x413A4  if (DSB(0x10816A + side) == DSB(0x10816A + (side^1))     ; 0x413A4
          && DSB(0x105B34 + (side^1)) == 0)
0x413B5      DSB(0x105B34 + side) = 1
```

**Measured (original, state 7, `/tmp/t1_chars.py`):** chars `[0, 3]`, variant
`[1, 0]`; side 0 (T-rex, ch 0) variant 1 → `0xA8A28[1] = 0x1BB9FCD8`. The
`0x41350` code plus the measured variant **infer** (a deduction, not a direct
read) that the original's `DS_0010816A[1]` was **0** at side 0's call
(variant[0]=1 requires `char[0] == char[1]` and `variant[1] == 0`; `char[0]` was
just written to 0, so `char[1]` was 0 — the BSS value). Side 1 then writes
`char[1] = 3` and, with `variant[0] == 1`, leaves `variant[1] = 0`.

**Measured (port, `/tmp/t1_chars.py` trace, reverted):** with the driver's seed
as shipped (`port/tests/test_frontend.c:475-476`,
`DSB(DS_0010816A) = DSB(DS_0010816A+1) = 0xFF`), side 0 reads
`char=0 variant=0 other_char=255`; with the seed at the original's BSS value
(`0`), side 0 reads `char=0 variant=1 other_char=0` and side 1
`char=3 variant=0 other_char=0 other_variant=1`. **So the divergence is the
front-end dump driver's sentinel seed of `DS_0010816A[1]`, not the engine and
not the acquisition order.** The engine `fight_char_select` is faithful.

### 1.5 The measured effect on the arena frame

`frame_0482.raw` against capture `frontend/frame_0834.raw`
(`/tmp/t1_diff.py`):

| tree | total | T-rex region (x<190) | raptor region (x≥190) |
|---|---|---|---|
| seed `0xFF` (as shipped) | 42 667 B (22.2%), 15 067 px | 6 363 px | 8 704 px |
| seed `0` (the original's BSS) | **30 536 B (15.9%), 10 602 px** | **1 898 px** | 8 704 px |

The T-rex's residual falls by 4 465 px (70%); the raptor's is **unchanged** (its
handle already matched). The remaining 1 898 + 8 704 px is the pose residual
(§3). The fix belongs at the caller — the dump driver's seed — with the engine
`0x41350` unchanged; **the engine itself needs no change.**

**Task 2's re-scope (the human's ruling).** The spec's "the palette acquisition
order" is not the owner; **Task 2 fixes the front-end dump driver's seed**
(`port/tests/test_frontend.c:475-476`) and records the reference change the seed
fix causes. The port's dump changes, so the front-end oracle's derived window
indices may move and the reference may need **one re-capture**; the **claims stay
the invariant, not the indices** (title `54 clean, 55 splice, 2 transition, 0
unexplained` / `54 clean, 57 splice, 0`; attract `FIRST DIVERGENCE at capture
frame 215`; front-end `0 unexplained`; smk `120/120` + `41/41`). The Gate's
second claim is measured by the unit assertion plus the arena byte-diff, not by
`make demo-oracle` (§7.0).

---

## 2. The twelve `0x34B14` handlers and their size (Step 2)

### 2.1 The dispatch

`fight_health_sync` (`0x34B6C`, body `0x34B6C..0x34D88`) dispatches on
`slot+0x52` through the 22-entry table at `0x34B14` (`0x34BF4`: `mov
al,[ecx+0x52]; cmp al,0x15; ja default; and eax,0xff; jmp
dword ptr cs:[eax*4 + 0x34B14]`). The dispatch site is **inside
`FUN_00034B6C`**, not `0x36E2C`: `0x36E2C` is a **predicate** (it returns 1
only when `+0x42 & 0x10` and `+0x52 ∈ {0,1,5,6,7}` and `+0x54 <= 1`) that
`0x34B6C` calls at `0x34B8E`; its return gates the position branch
(`0x34B97..0x34BEF`) and a zero falls through to the dispatch
(`0x34B93 TEST AL,AL; 0x34B95 JZ 0x34BF4`). Entries **9, 10, 11, 14, 15, 16**
(`0x34D83`) are the epilogue no-op; entries **12 (`0x34C9A`) and 13 (`0x34CA9`)
are real handlers** (the table at `0x34B14` read from Ghidra confirms the
dwords). The port's table is in record §11.3 of
`2026-09-20-demo-fight-derivations.md`; the twelve **gap** entries are below.

### 2.2 The twelve handlers

Each row: the handler's entry, its callees (from Ghidra / `prage.calls.csv`), and
the `+0x52`/`+0x53`/`+0x54`/`+0x57` writes. `rec` is the fighter record
(`DSD(slot)`); the slot base is `DS_001077B0 + side*0x94`.

| `+0x52` | entry | handler | body / callees | `+0x52` | `+0x53` | `+0x54` | `+0x57` |
|---|---|---|---|---|---|---|---|
| 1 | `0x34C15` | `0x359E0` | `0x365C8`; on set `+0x43 \|= 0x40` then `0x35B7C`; `0x367DC`; `0x1A5D4`/`0x1A640`/`0x1A570`; on the walk arm `0x35C1C` then `rec+0x4C = BD C00/BD BEC[+0x7A]`, `0x1883C`; moves the fighter x (`rec+0x18`) | — | — | — | — |
| 2 | `0x34C26` | `0x35C1C` | `+0x52 += +0x58`; clamp `[0, BDA3E[char*2]-1]`; `+0x29 \|= 8`; `+0x28 \|= 4`; `0x2BCF4` | **frame step** (0..n) | — | — | — |
| 2 | `0x34C26` | `0x35D20` | `0x2BC30(1.0)`; `+0x43 &= 0xFC`; if `+0x54==4` `DS_001078FE=1` | **9** | **0** unless `+0x53==0xD` | — | — |
| 4 | `0x34C53` | `0x35F84` | `+0x41 \|= 0x80`; landing gate (`BD882[char*2]>>16 ≥ rec+0x30` && `rec` anim `+0x36 ≤ 0`): `0x1922C`, `slot+0x24=0`, `0x3C148`, `0x3C16C` | **0x14** | **4** | — | — |
| 5 | `0x34C62` | `0x36430` | `0x365C8` (`+0x43` bit 0x40), `0x36638`; if `0x1088E0[side]` bit 0x40 and `0x1A640` → `0x2BC30(3.0)`; else `0x2BC30(2.0)`; else `0x3BDDC` | **9** or **0x15** | — | **0** (the `0x15` arm) | — |
| 7 | `0x34C80` | `0x399CC` | `0x33A10`; `+0x41 \|= 4`; `+0x74 = 0`; if `slot!=0 && slot+0x5D==0` `0x367DC` | — | — | — | — |
| 8 | `0x34C8D` | `0x37464` | the approach/landing machine on `+0x54` (`0..2`); on the `+0x54==0/1` in-range arms `0x35B7C`/`0x36638`; else `0x35C1C`+`0x1883C` | **9** (`0x3757B`) | — | — | — |
| 12 | `0x34C9A` | `0x361C8` | `+0x41 \|= 0x80`; switch on `+0x58`: case 3 with `\|+0x32>>16\| < 0x11` | **9** | — | — | — |
| 13 | `0x34CA9` | `0x36300` | `+0x41 \|= 0x80`; switch on `+0x58`: case 3 with `\|+0x32>>16\| < 0x11` | **9** | — | — | — |
| 17 | `0x34CB8` | `0x36710` | on `0x107808[side]` 0→1→2; at 2 with `+0x36==0 && +0x1C==0`: `+0x43=8`, `0x2C3FC` | **9** | — | — | — |
| 19 | `0x34D22` | `0x33B00` | bulk-copies the slot record (0x25 dwords) in/out, `0x2EA30` (interrupt lock), `0x36E78`, `0x29BC8` (the character palette re-acquire) | — | — | — | — |
| 20 | `0x34D69` | `0x35E6C` | `+0x41 \|= 0x80`; `0x2C3FC`; if `0x1088E0[side]` bits `(3&0xC)!=0` skip `0x3BDDC`; `0x3C480(2.0)`, `0x188AC` | **9** | **0** | — | — |
| 21 | `0x34D78` | `0x364FC` | `0x365C8` (`+0x43` bit 0x40), `0x36638`; if `0x1088E0[side]` bit 0x40 and `0x1A640==0` → `+0x52=5`, `0x2BC30(2.0)`; else `0x2BC30(2.0)`, `+0x54=0` | **9** or **5** | — | **0** (the `9` arm) | — |
| — | `0x349C8` bit 6 | `0x37178` | `DS_001078FE=0`; `+0x40 &= 0xFFBFFFBF`; `+0x42` bit 6; on the `-1`/in-range arms calls `0x36870`, else `0x35838` | **9** (`0x37241`/`0x3731A`) | — | — | **0** (`0x37361`/`0x37397`) or **1** (`0x373F8`/`0x3743A`) |
| — | `0x349C8` bit 7 | `0x37D18` | `0x2BC30(4.0)`, `0x39A10`, `0x2C3FC`; sets the other slot's `rec+0x59 = 0xFF` and `+0x40 \|= 0x801000` | **9** | **3** | **3** | — |

**None of the twelve table handlers writes `+0x57`** (an instruction scan of
`MOV byte ptr [reg+0x57], imm` finds no site in their extents). The `+0x57`
writers in the tail are `0x36638` (`+0x57 = 2` on its `+0x54 == 4` arm,
`0x366F2`), `0x37178` (`+0x57 ∈ {0, 1}`, `0x37361`/`0x37397`/`0x373F8`/`0x3743A`),
`0x3FD30` (`+0x57 = 1`, `0x3FDC9`) and `0x3FF08` (`+0x57 = 0`, `0x3FF87`).

### 2.3 Which returns `+0x52` from 9 to 4? — `0x36870`, not a handler

A scan of every `MOV byte ptr [reg+0x52], imm` in the image
(`ghidra_search_instructions`) finds the writers of `+0x52 = 4` at `0x21908`
(in `FUN_000216EC`), `0x36B91` (in `FUN_00036870`) and `0x3FDA4` (in
`FUN_0003FD30`); **no `0x34B14` handler writes 4.** The `+0x52 = 9` state is a
table no-op (entries 9, 10, 11, 14, 15, 16), so the 9→4 transition is not driven
by the dispatch.

`0x36870` (856 B) switches on `slot+0x54` (`(char)local_1c[0x15]`):

```
case 2 (0x36B91):
    slot+0x52 = 4;  slot+0x53 = 0;  0x3C520(2.0)
```

That is exactly the original's measured s1 state at t = 54.3349:
`(+0x52=4, +0x53=0, +0x54=2)` (§3.1). `0x36870` is called by `0x37178` (a
`0x349C8` sub-handler) and by `0x3FD30` (`0x3FDA4` sets `+0x52=4, +0x53=4`
then calls `0x36870`). It is named in the cycle-2 spec's stall list
("§7.10: `0x35F84`, `0x36870`, `0x235C4`, `0x370F0`, …").

**So the loop-1400 stall's 9→4 transition is `0x36870`'s `+0x54 == 2` arm
(`0x36B91`), reached via the `+0x53` machine / `0x349C8`'s bit-6 sub-handler —
not one of the twelve table handlers.** Task 3's port must land `0x36870` (and
the `+0x53` machine it hangs off, already ported as `fighter_state_3531c` /
`fighter_state_350d0`), not a `0x34B14` handler.

### 2.4 The closure size (the stop-condition measurement)

From `port/decomp/prage.calls.csv` (Ghidra callees) and `prage.functions.csv`
(sizes); "new" = not the `0x6xxxx` runtime library, not the RNG (`0x5Dxxx`), not
the five known stubs (`0x2C3FC`, `0x2EA64`, `0x62002`/`0x62003`/`0x6201B`), and
not already named anywhere in `port/src`.

| root | closure | new (not in port, non-infra) |
|---|---|---|
| the twelve handlers + `0x37178` + `0x37D18` | 311 funcs / 44 604 B | 36 funcs / 4 335 B |
| `0x3531C` (cycle 2's `+0x53` machine) | 281 funcs / 39 478 B | 26 funcs / 3 153 B |
| **union** | **317 funcs / 46 808 B** | **36 funcs / 4 335 B** |
| handlers' new **not already in `0x3531C`'s new** | — | **10 funcs / 1 182 B** |

The ten incremental functions are `0x164E8` (12), `0x18788` (116), `0x1883C`
(109), `0x35B7C` (158), `0x36E78` (152), `0x379C4` (147), `0x385B0` (384),
`0x39A10` (34), `0x3C148` (36), `0x3C16C` (34). The 36 genuinely-new functions
include `0x36870` (856), `0x37178` (746), `0x385B0` (384), `0x1E30C` (329),
`0x1E458` (465), `0x1E62C` (169), `0x1CC28` (370) and the render/effects
helpers; the remainder of the closure is shared infrastructure the port already
names.

**Size case: the handler tail is not materially larger than a cycle.** The
union's genuinely-new work (36 funcs / 4 335 B) is comparable to cycle 2's hit
chain (22 funcs / 2 720 B) and to the `+0x53` machine (26 / 3 153); the
incremental work over `0x3531C` is 10 funcs / 1 182 B. **No re-scope stop is
triggered.** The full closure (317 / 46 808 B) is inflated by the `0x6xxxx`
runtime library and the shared res/render/effects infrastructure, exactly as
cycle 2's Task 6b reported for `0x3531C`.

---

## 3. The pose selector (Step 3)

### 3.1 What selects the raptor's sprite

The sprite id is the animation stream's `0xCD40` form: `id = word[cursor+2] +
read_var(0x40)`, where `read_var(0x40)` is the **fighter record's `+0x52`**
(`anim_read_var` `0x29F34`, `port/src/game/actors.c:303`; cycle 2's Task 5b). The
raptor's stream head is `0xD2136`; its `0xCD40` id base is `0x16B5`, so
`id = 0x16B5 + rec+0x52`. `rec+0x52` is advanced by the idle tick `0x37A58`
(landed by Task 5b): `rec+0x52 += rec+0x58`, where `rec+0x58 ∈ {0, 0xFF}` is
re-drawn by `rng(2)` on the 3-frame countdown `rec+0x4C`. **So the sprite is
selected by `rec+0x52` (the frame variable), and the stream/cursor by the slot's
`+0x52`/`+0x53` state machine** (which animation was started).

### 3.2 The original, measured (`/tmp/t1_pose.py`)

At the demo's state-7 entry (t = 53.744, the capture-834-equivalent frames):

| field | original (raptor, side 1) | port (dumped 481-483) |
|---|---|---|
| slot `+0x52` | **9** | **0** |
| slot `+0x53` | **8** | **0** |
| slot `+0x54` | 0 | 0 |
| slot `+0x57` | 0 | 0 |
| anim cursor (`rec+0x08`) | `0xD2316` (head `0xD2136`) | `0xD2140` |
| `rec+0x52` | 0 → 8 (incrementing) | 0 → 7 (incrementing) |

The original's raptor slot then cycles `9 → 0 → 3 → 4 → (16,10) → (9,11) → 18
→ …` over state 7 (§3.3). The port's raptor **stays at `+0x52=0` for 43
state-7 frames, then `+0x52=14`, … and only reaches `+0x52=9, +0x53=8` at
dumped frame ~661 (loop 1400)** — the driver's `state-7 last +0x52 change at
loop frame 1400` — where it then stays. So the port's raptor is at a different
slot state *and* a different stream cursor at the same frame.

### 3.3 Verdict: a subsystem, not a single derivable state

The divergence is **the raptor's `+0x52`/`+0x53` state machine's phase**, not one
field, one tick or one stream:

* the sprite formula (`0x16B5 + rec+0x52`) and the idle tick are already
  faithful (Task 5b landed `0x37A58`, and the port's `rec+0x52` advances);
* the *slot state* differs (`0` vs `9`) at the same frame, and the *cursor*
  differs (`0xD2140` vs `0xD2316`);
* the state machine's input is the demo's command word (`0x47208`'s generator,
  RNG-driven) and the `0x34B14`/`0x36870` handlers this cycle ports.

**It is a named gap with its evidence, not a fitted value.** The pose residual
after the palette fix (§1.5) is 1 898 px (T-rex) + 8 704 px (raptor); the raptor
region is the state machine's phase, and it is expected to move only once the
handlers and the `+0x53` machine's missing arms land (Tasks 3–4). Task 4 must
**not** fit a cursor or a state; if the state machine's phase cannot be pinned
from the raw after Tasks 2–3 land, it stays a named gap.

**Task 4's correction (raw + measured; this section's port values are stale).**
§3.2's port column (`+0x52 = 0`, cursor `0xD2140`) was measured at the base
commit, before Tasks 3b/3c. Measured at HEAD, the port's raptor **matches the
original's 9/8 entry exactly**: at loop 1071 (dumped frame 482) both trees hold
`+0x52 = 9`, `+0x53 = 8`, `+0x54 = 0`, `+0x5F = 0x01`, `+0x6A = 0x0001`, cursor
`0xD2316`, `rec+0x52 = 0`; the port's cursor then tracks the original's through
the hold. The teal-mask silhouette IoU (region x 185..320, y 85..200; mask
`g>90 && b>90 && g>r+30 && b>r+30`) of the port's dumped frame 482 against
capture 834 is **1.000** (frame 483: 0.974), and the arena byte-diff at frame
482 is **18 294 B / 6 194 px** (Task 2's 30 536 B / 10 602 px). **So the
`+0x52=0`/cursor-`0xD2140` divergence §3.2/§3.3 report no longer exists.**

The residual is the **exit** from 9/8 to the pose state `0x10`/`0x0A` (see §10).

---

## 4. The loader flush scope's separability (Step 4)

### 4.1 The mechanism (cycle 2's Task 5c, re-confirmed)

The original's loader flush (`0x1C470` via `0x1C65C`, the `- LOADING -` text
draw) drains only the `0x33734` initial and the `0x80997C` font palettes; the
arena's records survive to the master loop's `0x25672` flush. The port's loader
flush drains the arena's records instead, so its gate flush is empty and its DAC
already holds the arena palette at the loader frame. Evidence: cycle-2 record
§9.6 (Task 5c), `0x1C65C`/`0x1C470`, `0x25672`.

### 4.2 Separability — yes, as a code change

The flush scope / enqueue order is a `platform/res.c` (`res_load_present`),
`platform/gfx.c` (`gfx_flush_palette`, `palette_record`) and `game/flow.c`
ordering concern. The 831/832 held-frame gap needs the **post-read ISR tick
count** (`0x1BDF4`'s `INC EDX`/`MOV [0x101508],EDX` at `0x1BE0E`/`0x1BE10`), a
host/emulator property the two live-RAM polls disagree on (Δ=2 vs Δ=3). The two
are independent owners, so the flush scope **is separable** from the held-frame
presentation.

### 4.3 Does fixing it alone move `make demo-oracle`? — No, measured

Two measurements, not a prediction:

1. **The loader flush has zero effect on the port's dumped frames.** Removing
   `text_blit_string`'s `gfx_flush_palette()` (`actors.c:1590`, `0x1C6C6`) and
   re-dumping produces **1382/1382 byte-identical frames** (all frames 0..1381
   hash-equal to the unmodified tree; `frame_0481` and `frame_0482` both
   identical). The port's per-iteration frame-end flush drains the same records,
   so the loader flush's scope is invisible to the dump.
2. **The demo oracle's fallback ignores the port.** `make demo-oracle` reports
   `demo: window distinct [831..3616]`, `0 clean, 0 splice, 0 transition, 2779
   unexplained`, `port frames exhibited 0/1068`, `first unexplained captured
   frame 832`. `tools/title_compare.py:484-514` takes the `res is None` branch
   (no capture frame explains any demo port frame) and prints the first
   content-bearing **capture** frame without re-examining the port. So the first
   unexplained 832 cannot move from any port render change until that fallback is
   fixed.

Consistent with cycle 2's Task 5c measurement ("making the port's loader-frame
gate fail does not advance the demo oracle"). **Fixing the loader flush scope
alone does not move `make demo-oracle` at all.**

---

## 5. The RNG question (Step 5)

**Yes — the `0x34B14` tail consumes RNG (`0x5D7DC`).** A scan of the twelve
handlers' closure (311 functions) for a direct `call 0x5D7DC` finds two caller
functions, and `0x39040` alone has **three** call sites (the review's correction;
the conclusion is stronger than the first pass claimed):

| caller | sites | what |
|---|---|---|
| `0x39040` | `0x391C3` (`rng(2)`, arg load `0x391BE`), `0x391FE` (`rng(3)`, arg load `0x391F9`), `0x39228` (`rng(2)`, arg load `0x39223`) | the per-side round/timer pass; `0x350D0` calls it at `0x35244`, `0x36870` at `0x36974` |
| `0x2B2A0` | in the animation opcode interpreter | `spawn_anim_opcode`, called from `0x2BC30`/`0x2AE14` |

(`0x391E9` is **not** a call site — it is `MOV AL,[EBX+0x1088A8]`, the
`rng(0x40)` gate's argument load; the first pass mis-cited it. The `0x36870`
call to `0x39040` is at `0x36974`; `0x36979` is its return address.)

`0x39040` is already ported (`fighter_39040`, `fighter.c:1482`) and already
draws `rng_next(2u)` (its site is the raw's `0x391C3`, the port comment's
`0x391CA` is the store after it), and `0x350D0` already calls it
(`fighter.c:1668`). The new handlers add draws where they reach `0x39040`
(`0x33B00`, `0x36870`) or `0x2B2A0` (`0x33B00`'s palette re-acquire path does
not; `0x359E0`/`0x367DC` do not). **Consequence: porting the handlers will change
the RNG stream alignment.**

**The determinism answer (the human's ruling).** Task 3 must **assert the port's
draw sequence matches the original's at the same points** — a unit test on the
draw order and values (the cycle-2 pattern: seed, drive the frame, assert the
draws). **Pin at the source as cycle 2 did only if the streams drift.** The
port's RNG is seeded (`rng_seed`), the cycle-2 determinism gate
(`PR_FRONTEND_DET`, `54 agree / 0 disagree`) is the arbiter, and a drift found
after Task 3 lands is derived at its site, never fitted.

---

## 6. Named gaps

1. **The fighters' exit from 9/8 to the pose state `0x10`/`0x0A`** (§3.3
   correction, §10). **Resolved as a cause, named as a gap.** The port's raptor
   now matches the original's 9/8 entry (slot `+0x52=9`, `+0x53=8`, cursor
   `0xD2316`), and its silhouette matches capture 834 (IoU 1.000); the
   `+0x52=0`/cursor-`0xD2140` divergence §3.2/§3.3 report was stale (pre-Tasks
   3b/3c). The residual is the **exit** to the pose state: it is reached only via
   `0x19020` → `0x193B0` → `0x3B714` → `0x3AAFC` → the `0x3A504`/`0x3A650`/
   `0x3A79C`/`0x3A8E8` pose family, and `0x19020` is unported, so
   `DS_00100AF8`/`AFC` stay 0 and `fighter_pass_a`'s tail never runs. **Its
   closure is 68 new funcs / 10 467 B — materially larger than a task (§10.4),
   so Task 4 landed no port.** The pose state is measured as
   `s7_saw10`/`s7_saw0a` (printed, not asserted). A follow-on cycle owns it,
   together with the `0x3Fxxx` closer script Task 3b re-scoped.
2. **The demo oracle's `res is None` fallback** (`tools/title_compare.py:484-514`)
   ignores the port, so the first-unexplained 832 is a capture-only figure and
   cannot move from a port change. Any cycle-3 Gate claim measured through the
   demo oracle must account for this.
3. **831/832's held-frame presentation** stays out of scope (spec's Out): it
   needs the post-read ISR ticks, a host property. This record does not claim it.
4. **`0x349C8`'s bit 6/7 sub-handlers' internal resource reads** (`0x37178`'s
   `0x36870` arms, `0x37D18`'s `0x39A10`) are transcribed at the call level
   (§2.2) but their deep callees (`0x39040`, `0x36870`'s `0x385B0`) are named in
   the closure table (§2.4) without a line-by-line body derivation; Task 3
   derives each as it ports it.
5. **The `0x0A838B44`/`0x0A838C60` arena-palette producers — resolved.** They
   are the actor descriptors' own palette handles (`dp+0x10`), acquired by
   `0x2AE14`'s `param_1[4]` arm (the port's `actor_spawn`,
   `port/src/game/actors.c:1392-1393`), not `0x43818` (whose four acquires are
   `0x98EC50C`/`0x98EC514`/`0x8099AC`/`0x809984`, `0x43886-0x438A9`). No
   hand-loaded immediate exists for them (`ghidra_search_instructions` finds no
   `MOV EAX, 0xA838B44`); they come from the descriptor table, so a Task 2 unit
   test that drives the acquisition path must spawn the arena actors, not call
   `palette_acquire` with a literal.

---

## 7. Unit-test values (the Tasks 2–4 substitutions)

### 7.0 The Gate's second claim's measurement (the human's ruling)

**The Gate's second claim (the arena's character palette matches the raw's DAC
range, and the arena's byte-diff drops to the level the poses alone explain) is
measured by the unit assertion plus the arena's byte-diff — not by
`make demo-oracle`.** `make demo-oracle`'s `res is None` fallback
(`tools/title_compare.py:484-514`) ignores the port, so its `first unexplained
832` cannot move from any port change (§4.3, §6.2). The claim's evidence of
record is therefore:

* the unit assertion on the palette table entry's `start` (§7.1), and
* the measured arena byte-diff before and after (§7.1's table), reported with
  the pose residual named (§3.3).

`make demo-oracle` stays report-only and its 832 is not the Gate's figure.

### 7.1 Task 2 — the palette order / variant

* **The T-rex's DAC range (both trees):** `start = 142`, `len = 31`.
* **The original's T-rex handle:** `0x1BB9FCD8` (`0xA8A28[1]`); **the port's as
  shipped:** `0x1BB9FD58` (`0xA8A28[0]`).
* **The flag:** `DS_00105B34[0] == 1` (original) vs `0` (port's dump driver);
  `DS_0010816A[0] == 0`, `DS_0010816A[1] == 3` after `0x41350` runs.
* **The unit assertion:** call `fight_char_select(0, char_index)` with
  `DS_00104B1D != 1` and the *original's* pre-state (`DS_0010816A[0..1] = 0`),
  then `CHECK_EQ_INT((int)DSB(DS_00105B34), 1)` and
  `CHECK_EQ_INT((int)DSD(0xA8A28 + DSB(DS_00105B34) * 4), 0x1BB9FCD8)`. Seed
  the sentinel: with `DS_0010816A[1] = 0xFF` (the current driver) the assertion
  fails (variant 0), which is the mutation proof.
* **The seed's design constraint (Task 2's fix direction).** The driver's seed
  must not perturb the variant, so `DS_0010816A[1]` must be seeded to a value
  that equals `char[0]` at side 0's call. The only value that both preserves the
  original's variant and still proves the engine wrote `char[1]` is **0** (it
  differs from the post-state 3, so `CHECK_EQ_INT(DSB(DS_0010816A+1), 3)` is
  falsifiable). `DS_0010816A[0]` may stay `0xFF` (it is overwritten before the
  variant test). Measured: seed `[0]=0xFF, [1]=0` gives variant 1 and the
  original's handles; seed `[0]=[1]=0xFF` gives variant 0.
* **The measured byte-diff:** `frame_0482` vs capture 834 = 42 667 B (22.2%,
  15 067 px) with the seed, 30 536 B (15.9%, 10 602 px) with the original's BSS
  value; the T-rex region 6 363 → 1 898 px, the raptor region 8 704 → 8 704 px.
* **Task 2's re-scope (the human's ruling).** Task 2 **fixes the front-end dump
  driver's seed** (`port/tests/test_frontend.c:475-476`), not the engine and not
  the acquisition order. Because the seed change alters the port's dump, it
  **records the reference change it causes**: the front-end oracle's derived
  window indices may move and the reference may need **one re-capture**. The
  **claims stay the invariant, not the indices** — the title
  `54 clean, 55 splice, 2 transition, 0 unexplained` / `54 clean, 57 splice, 0`,
  the attract `FIRST DIVERGENCE at capture frame 215`, the front-end
  `0 unexplained` and the smk `120/120` + `41/41` are the things that must not
  move; a window index that moves with the seed fix is re-derived and recorded,
  never treated as a regression. Evidence: §1.4/§1.5.

### 7.2 Task 3 — the `0x34B14` handlers

* **The table** (§2.2) is the substitution source for the plan's `<...>`.
* **`+0x52 = 9` is a no-op** (entries 9, 10, 11, 14, 15, 16, `0x34D83`);
  **no handler writes `+0x52 = 4`.** The 9→4 writer is `0x36870`'s `+0x54 == 2`
  arm at `0x36B91` (`+0x52 = 4; +0x53 = 0; 0x3C520(2.0)`).
* **The original's state-7 s1 trajectory** (§3.2/§3.3) is the assertion source:
  `+0x52` cycles `0,1,3,4,5,9,0x0E,0x10,0x12` and `+0x53` cycles
  `0,4,7,8,10,11`; `+0x52=4` pairs with `+0x53=0, +0x54=2`.
* **The closure:** 317 funcs / 46 808 B union; 36 funcs / 4 335 B new; 10 funcs
  / 1 182 B incremental over `0x3531C`.

### 7.3 Task 4 — the poses

* **The raptor's stream head:** `0xD2136`; **the sprite formula:** `0x16B5 +
  rec+0x52`; **the idle tick:** `0x37A58` (`rec+0x52 += rec+0x58`, `rec+0x58 ∈
  {0, 0xFF}` from `rng(2)` on the `rec+0x4C` 3-frame countdown).
* **The original's first-state-7-frame fields:** slot `+0x52=9`, `+0x53=8`,
  `+0x54=0`, `+0x57=0`; cursor `0xD2316`; `rec+0x52` 0→8.
* **The port's at HEAD (Task 4's correction):** slot `+0x52=9`, `+0x53=8`,
  `+0x54=0`; cursor `0xD2316`; `rec+0x52` 0→8 — **identical** (the §7.3
  "port's `+0x52=0`/cursor `0xD2140`" was the base commit's, before Tasks
  3b/3c).
* **The residual is the exit to the pose state `0x10`/`0x0A`** (§10): the
  `0x19020` → `0x193B0` → `0x3B714` → `0x3AAFC` → pose-family chain, closure
  68 new funcs / 10 467 B — **the Step-1 size gate triggers; no port landed**.
  The assertion is therefore the measured-not-asserted `s7_saw10`/`s7_saw0a`,
  not a fitted cursor or state.

---

## 8. Provenance

* **Raw bytes / decompilation:** `data/game/C/PRAGE.EXE` (read-only), Ghidra
  MCP project `rage` program `/PRAGE.EXE` (fixups applied; `FUN_00033754`,
  `FUN_00033734`, `FUN_00041350`, `FUN_00036870`, `FUN_0003fd30`, the twelve
  handlers, `FUN_000350d0`, `FUN_0003531c`), and `port/decomp/prage.c`,
  `prage.functions.csv`, `prage.calls.csv`.
* **Data tables:** `0xA8A28` (the character-palette rows), `0xA8AF8`/`0xA8B14`
  (the dust-palette rows), `0xA8A98` (the row pointers), `0x105B34` (the variant
  bytes) — all read from Ghidra `read_memory`.
* **Live measurements:** `/tmp/t1_pal.py` (the original's palette table,
  `/tmp/t1_pal.csv`), `/tmp/t1_chars.py` (chars/variant/rows), `/tmp/t1_pose.py`
  (the fighter fields), `/tmp/t1_diff.py` (the frame diffs), the port traces
  (`fight_char_select`, `fighter_state_3531c`, `text_blit_string`) — all
  reverted. `make demo-oracle` (report-only) and
  `PR_FRONTEND_DUMP=/tmp/t1_pd*`.
* **Prior records:** `2026-09-20-demo-fight-derivations.md` §7.10/§11.3 (the
  `0x34B14` table), `2026-09-21-demo-fight-closure-derivations.md` §9.5/§9.6
  (the palette/DAC gap, the gate, the loader flush scope), and the cycle-2
  reports (`task-5b-report.md`, `task-6b-report.md`).
* **Port cross-references:** `game/actors.c` (`palette_acquire`, `0x2A17C`,
  `text_blit_string`), `game/fight.c` (`fight_char_select`, `fight_dust_value`,
  `fighter_state_3531c`), `game/fighter.c` (`fighter_state_350d0`,
  `fighter_39040`), `game/flow.c` (`game_state_6`), `platform/gfx.c`
  (`gfx_flush_palette`, `palette_record`), `platform/res.c`
  (`res_load_present`), `tests/test_frontend.c` (the `DS_0010816A` seed).

---

## 9. Task 3c — the screen-anchor/offset (the demo-AI's distance input)

### 9.1 The divergence and its first frame

Task 3b aligned the state-7 entry's LCG but the demo-AI command words still
diverged: at the **2nd** state-7 frame (loop 1072) the port's `cmd0` was `0x1010`
and its slot 1 was `00/00`, where the original's were `0x4848` and `09/08`. The
first divergence is one frame earlier: at loop 1071 the port's `cmd1` was
`0x0000` and slot 1 `00/00`, the original's `0x0002` / `09/08`, with the AI
blocks matching (`b1` state 2, weight `0x82`). So the AI's *input* differed, not
its logic: `ai_pick`'s band (port `+0x2D=02`, original `01`) came from a
different `ai_distance` (0x187FC = `slot0+0x2C - slot1+0x2C`).

### 9.2 The owner: `0x18540`/`0x18350`, the port's named gap

`slot+0x2C` is the latched screen position, not the record's world x:
`0x186D0` (`fighter_slot_latch`) sets
`slot+0x2C = rec+0x18 + DS_00100AB0[side*8]`. Measured on the live original
(`/tmp/t3c_pos.py`): at the state-6 frame `rec+0x18 = -6144/6144` — **the same as
the port's** — while `lat = -5824/5888`, the difference being the camera offsets
`DS_00100AB0 = 0x140 / 0xFFFFFF00`. The port transcribed the `+0x100AB0/AB4`
reads but left `0x18627 0x18540(side)` and `0x1864D 0x18350(side, anchor)` as a
named gap (§6.3 of the cycle-2 record), so its offsets stayed 0.

* **`0x18540(side)`** (229 B) seeds `DS_00100AF0[side]` = the slot's actor word
  (`DSW(DSD(0x1014EC) + DSW(rec+0x56)*0x20) & 0x7FFF`) minus the character's
  camera constant — the jump table `0x18524` maps char **0 and >6 to the default**
  `0xE6DD0`, char 1..6 to `0xE39D0/0xECBD8/0xD2134/0xEA604/0xD3E08/0xE061C`
  (the same mapping as `camera_char_const`), clamped to 0 when negative or at/over
  `0xE6DB4[char]`.
* **`0x18350(side, anchor)`** (185 B) writes `DS_00100AB0/AB4[side*8]` from the
  character's anchor-indexed signed-byte pair — the jump table `0x18334` maps
  char 0/5 and >6 to `0xCEB00`, char 1/6 to `0xCF399`, char 2/3/4 to
  `0xCFC32/0xD033B/0xD0A44` — the x negated when the actor's bit 15 is set
  (`0x1A570`), both shifted left 6.
* `0x18460` (193 B) and `0x18428` (27 B) are **effect-free in this build**: the
  `0x1840C` jump table resolves to `0x18408` (`0x18350`'s epilogue), so the
  `0x18428` dispatch writes nothing (the port's fixup-applied `mem[]` holds the
  seven `0x18408` entries, read back from the test driver).

### 9.3 The measured effect

With the gap ported, the camera offsets match the original's frame by frame
(`/tmp/t3c_pos.py` vs the port's `PR_T3C_TRACE`): `0x140/0xFFFFFF00` (state-6),
`0x80/0` (frame 1), `0, 0x880/0xFFFFF9C0`, …; the AI block's band/move/step
pointer match (`b0 +0x2D=0x0F, +0x2E=0x41, +0x1C` → VA `0x8AB7C`); and both
command words match (`cmd0 = 0x4848` at loop 1072, `cmd1 = 0x0002` at 1071).

### 9.4 The residual (a named gap)

The port's slots reach the original's `+0x52=9, +0x53=8` no-op at loop 1072 but do
**not** advance to the original's `+0x52=0x10, +0x53=0x0A` (measured original,
`/tmp/t3c_64.py`). `+0x52=9` is a table no-op and `slot+0x64` is `0xFF` in **both**
trees (so `0x3B464`'s think chain returns at `0x3B49F` in both — the port's
comment stands), so the transition is not the think chain: the `+0x53=8` arm
(`0x3531C` case 8, `0x354BC`) re-arms `+0x53=8` every frame because
`hit_chain_resolve` (`0x3CF38`) returns 0 — no phase-8 hitbox is armed, because
the port's animation cursor differs (Task 1 §3.3, Task 4's pose gap). The
`+0x52=0x10` writers are the `0x3A504/0x3A650/0x3A79C/0x3A8E8/0x3A95C` family
(and `0x235C4`/`0x39F40`/`0x3C358`), i.e. the pose machine, not the AI. **The
residual is a separate subsystem; the demo-AI block state and the command words
match.**

### 9.5 Provenance (Task 3c)

`ghidra_read_memory` at `0x18524`/`0x1840C`/`0x18334`/`0xE39D0`/`0xEA604`/
`0xE6DB4`/`0xE6DD0`/`0xA1774`/`0xCEB00`/`0xD033B`; `ghidra_disassemble_function`
at `0x18540`/`0x186D0`/`0x18460`/`0x18428`/`0x18350`; live measurements
`/tmp/t3c_pos.py` (the camera offsets and `rec+0x18`) and `/tmp/t3c_64.py`
(`slot+0x64`/`+0x5F`); the port's env-gated `PR_T3C_TRACE` dump (reverted). The
front-end oracle's window is `[560..830]` (`271: 117 clean / 153 splice`,
`0 unexplained`) and is **unmoved** by this task — the `[557..810]`/254 text the
comment carried was pre-existing stale drift (already flagged in Task 2's
review), and the window covers port frames 0..258 while the fight starts at port
frame 481.

---

## 10. Task 4 — the pose state's exit from 9/8 (the freeze's remaining layer)

### 10.1 The brief's model is refuted (raw + measured)

The brief (§Task 4) and §9.4 said the freeze's next layer is "`0x3531C` case 8
re-arms `+0x53 = 8` because `hit_chain_resolve` (`0x3CF38`) returns 0 for want of
a phase-8 hitbox, and the port's animation cursor differs". **Both premises are
false.**

1. **`0x3531C` case 8 does not re-arm forever.** Its second gate is
   `0x3548A MOV DX,word[0xBDBE8]; 0x35491 CMP DX,[EAX+0x88]; 0x35498 JLE 0x354E5`
   — `word[0xBDBE8] = 3` (`ghidra_read_memory 0xBDBE8` = `03 00 00 32`), so once
   `slot+0x88 >= 3` case 8 **returns before touching `+0x53`** (only `+0x56`
   increments, `0x3547A`). It cannot hold the state; it merely stops writing.
2. **The port's animation cursor does not differ** (§3.3's correction): at the
   9/8 entry both trees hold cursor `0xD2316` and `rec+0x52 = 0`, and the port
   tracks the original through the hold.

### 10.2 The measured divergence: the pose state is never entered

Measured live (DOSBox-X, `/tmp/t4_orig.py`, base `0x266000`) vs the port's
per-frame trace (`PR_T4_TRACE`, reverted):

| | original | port |
|---|---|---|
| T-rex (`s0`) 9/8 entry | `+0x52=09 +0x53=08 +0x5F=0b`, `+0x56=00` | **identical** |
| T-rex 6 frames later | `+0x52=10 +0x53=0A +0x54=00 +0x5F=0b`, `+0x56=05` | `+0x52=09 +0x53=08`, `+0x56=1c` (frozen) |
| T-rex hitbox phases | `ph[0]=8` armed, then the hit consumes | `ph` all 0 |
| `DS_00100AF8`/`AFC` | non-zero (a winner) | **0 / 0** |

So the original's T-rex leaves 9/8 for the **pose state `0x10`/`0x0A`** (the
`+0x54 = 0` variant, i.e. one of `0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8`) on the
6th frame of the hold; the port stays at 9/8 for the whole window. The raptor
(`s1`) follows the T-rex: its exit from 9/8 at `+0x88 = 0x1c` (t = 54.255) is the
same pose machine, not a `0x3531C` arm.

### 10.3 The owner: `0x19020` → `0x193B0` → `0x3B714` → `0x3AAFC` → the pose family

The pose family's `+0x52 = 0x10` writers are reached only through this chain
(`ghidra_get_function_callers`, exhaustive):

* `0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8` ← `0x3AAFC` only;
  `0x3A95C` ← `0x3B464` (the think driver, unreachable in the demo because
  `slot+0x64 == 0xFF`) and `0x3B464`'s other callers.
* `0x3AAFC` (667 B, the reaction applier: it switches on the reaction code and
  `slot+0x54`, and calls `0x39F40` or one of the pose family) ← `0x3B714` only.
* `0x3B714` (449 B) ← `0x193B0` only.
* `0x193B0` (the winner's per-frame body: `0x33950`, `0x34D8C`, `0x1922C`,
  `0x3962C`, `0x396AC`, `0x19164`, `0x39A10`, `0x18B44`, `0x3C148`/`0x3C16C`,
  and `0x3B714` when `slot+0x1C == 0`) ← `0x1958C` only.
* `0x1958C` (`fighter_pass_a`, **ported**) calls `0x193B0` at `0x1974D`
  (`0x19720 CMP [0x100AF8],0; ... 0x1974D CALL 0x193B0`), gated on
  `DS_00100AF8 != 0 && DSB(0x10783A) != 0` (side 0) or the `AFC`/`0x1078CE` pair
  (side 1).
* `DS_00100AF8[side]` is written by **`0x19020(side)`** (`0x19020` calls the
  callback at `slot[side]+0x18` and sets `DS_00100AF8[side] = (result == 0)`).
  `0x19020` is the port's named gap at `fighter.c:287`. `slot+0x18` is set to
  `0x3FD30` by `0x3FF08` (Task 3b's closer chain, `0xA50C0`/`0x3FFDC`).

**So the freeze's remaining layer is the `0x19020`/`0x3Fxxx` closer chain
(Task 3b's re-scoped subsystem) feeding `0x193B0`/`0x3B714`/`0x3AAFC` and the
pose family.** In the port `DS_00100AF8`/`AFC` are 0 at every state-7 frame
(measured), so `fighter_pass_a`'s tail — and therefore `0x193B0` and the pose
state — can never run.

### 10.4 The closure size (the stop-condition measurement)

From `prage.calls.csv`/`prage.functions.csv`; "new" = not the `0x6xxxx` runtime,
not the RNG (`0x5Dxxx`), not the five known stubs, and not named anywhere in
`port/src` except the generated `symbols.h`:

| root set | closure | new |
|---|---|---|
| `0x193B0` + `0x3B714` + `0x3AAFC` + `0x3A79C` | 308 funcs / 43 170 B | **55 funcs / 7 793 B** |
| `0x3FD30` + `0x3FF08` + `0x3FFDC` (the closer script) | 303 / 44 324 B | 45 / 5 623 B |
| **union** | 345 / 51 648 B | **68 funcs / 10 467 B** |

The union's genuinely-new work is **68 functions / 10 467 bytes** (of which
`0x18C14` 1035, `0x392A0` 907, `0x3AAFC` 667, `0x3B714` 449, `0x385B0` 384,
`0x3A0FC` 355, `0x3AE9C` 294, `0x38ED0` 284, `0x192DC` 212, `0x19164` 198, the
five pose-family members 116 each, …). **Size case: the closure is materially
larger than a task** — ~2.4× the handler tail's 36 / 4 335 (§2.4) and ~3.3×
cycle 2's `+0x53` machine (26 / 3 153). Task 3b's first attempt already scoped the
closer chain at 15–20 funcs / ~3–4 KB and was re-scoped for the same reason.
**Task 4 triggers the brief's Step-1 size gate: no port was landed.**

### 10.5 The measured effect on the arena (no code landed)

* Raptor teal-mask IoU vs capture 834 (region x 185..320, y 85..200):
  **1.000** at port frame 482 (0.974 at 483) — the §3.2 "0.522" is stale.
* Arena byte-diff `frame_0482.raw` vs capture 834: **18 294 B (9.5 %) /
  6 194 px**, T-rex region 1 773 px, raptor region 4 421 px (Task 2's
  30 536 B / 10 602 px, T-rex 1 898, raptor 8 704).
* The fight's last `+0x52` state-change frame: **1072** (unmoved by Task 4);
  the state-7 timer exit is `s7_last == 1969` (unmoved).
* The pose state is measured in `test_frontend.c` as `s7_saw10`/`s7_saw0a`
  (printed, not asserted — a named gap, like `s7_hit`).

### 10.6 Provenance (Task 4)

`ghidra_disassemble_function` at `0x3531C`/`0x34E2C`/`0x354F0`/`0x1958C`/
`0x33C78`; `ghidra_decompile_function` at `0x3A504`/`0x3A650`/`0x3A79C`/
`0x3A8E8`/`0x3A95C`/`0x3AAFC`/`0x3B714`/`0x193B0`/`0x1958C`/`0x19020`/`0x18950`/
`0x38434`/`0x382C4`/`0x385B0`; `ghidra_get_function_callers` for the pose
family, `0x3AAFC`, `0x3B714`, `0x193B0`, `0x367DC`, `0x39F40`, `0x193B0`;
`ghidra_search_instructions` for the `+0x53`/`+0x56`/`+0x88` writers;
`ghidra_read_memory` at `0xBDBE8` (`03 00 00 32`). Live measurement
`/tmp/t4_orig.py` (DOSBox-X, base `0x266000`) and the port's env-gated
`PR_T4_TRACE` (reverted). Frame diffs `/tmp/t1_diff.py` and the teal-mask IoU
computed from `data/title-captures/frontend/frame_0834.raw`.
