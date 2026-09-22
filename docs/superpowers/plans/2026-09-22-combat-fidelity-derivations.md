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
   `+0x52 = 9` is a table no-op (entries 9..16); the twelve handlers write
   `+0x52 ∈ {0, 5, 9, 0x12, 0x14, 0x15}` and never 4. The writer of
   `+0x52 = 4` is `0x36870`'s `+0x54 == 2` arm at `0x36B91` (§2.3), a callee of
   `0x37178` (a `0x349C8` sub-handler) and of `0x3FD30` — not a table handler.
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
`0x41350` code plus the measured variant prove the original's `DS_0010816A[1]`
was **0** at side 0's call (variant[0]=1 requires `char[0] == char[1]` and
`variant[1] == 0`; `char[0]` was just written to 0, so `char[1]` was 0 — the BSS
value). Side 1 then writes `char[1] = 3` and, with `variant[0] == 1`, leaves
`variant[1] = 0`.

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

---

## 2. The twelve `0x34B14` handlers and their size (Step 2)

### 2.1 The dispatch

`0x36E2C` (`fight_health_sync` `0x34B6C` calls it at `0x34B8E`) dispatches on
`slot+0x52` through the 22-entry table at `0x34B14` (`0x34BF4`: `mov
al,[ecx+0x52]; cmp al,0x15; ja default; and eax,0xff; jmp
dword ptr cs:[eax*4 + 0x34B14]`). Entries 9..16 (`0x34D83`) are an epilogue
no-op. The port's table is in record §11.3 of
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
table no-op (entries 9..16), so the 9→4 transition is not driven by the dispatch.

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
handlers' closure (311 functions) for a direct `call 0x5D7DC` finds two sites:

| caller | site | what |
|---|---|---|
| `0x39040` | `0x391E9` (cycle-2 §3.8) | the per-side round/timer pass; `0x350D0` calls it at `0x35244`, `0x36870` at `0x36979` |
| `0x2B2A0` | in the animation opcode interpreter | `spawn_anim_opcode`, called from `0x2BC30`/`0x2AE14` |

`0x39040` is already ported (`fighter_39040`, `fighter.c:1482`) and already
draws `rng_next(2u)` (`0x391CA`), and `0x350D0` already calls it
(`fighter.c:1668`). The new handlers add draws where they reach `0x39040`
(`0x33B00`, `0x36870`) or `0x2B2A0` (`0x33B00`'s palette re-acquire path does
not; `0x359E0`/`0x367DC` do not). **Consequence: porting the handlers will change
the RNG stream alignment**, so a determinism answer is needed before Task 3
commits to a pin. This is **flagged to the human** (the brief's instruction),
not pinned here; the port's RNG is seeded (`rng_seed`) and the cycle-2
determinism gate (`PR_FRONTEND_DET`, `54 agree / 0 disagree`) is the arbiter.

---

## 6. Named gaps

1. **The raptor's pose state-machine phase** (§3.3). The port's raptor is at
   `+0x52=0` where the original is at `9` at the same frame, and its stream
   cursor differs (`0xD2140` vs `0xD2316`). It is the `+0x52`/`+0x53` machine's
   phase, not a single field; it may not be pin-able until Tasks 2–3 land. If it
   cannot be pinned, it stays a named gap — never a fitted cursor or state.
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

### 7.2 Task 3 — the `0x34B14` handlers

* **The table** (§2.2) is the substitution source for the plan's `<...>`.
* **`+0x52 = 9` is a no-op** (entries 9..16, `0x34D83`); **no handler writes
  `+0x52 = 4`.** The 9→4 writer is `0x36870`'s `+0x54 == 2` arm at `0x36B91`
  (`+0x52 = 4; +0x53 = 0; 0x3C520(2.0)`).
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
* **The port's:** slot `+0x52=0`, `+0x53=0`; cursor `0xD2140`; `rec+0x52` 0→7.
* **The divergence is a subsystem** (§3.3) — the assertion must be a state-machine
  phase check, not a fitted cursor.

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
