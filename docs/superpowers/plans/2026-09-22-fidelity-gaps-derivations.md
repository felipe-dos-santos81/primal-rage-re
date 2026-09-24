# Closing the small fidelity gaps — cycle: raw-byte derivation (Task 1)

**Cycle** (`docs/superpowers/specs/2026-09-22-fidelity-gaps-design.md`, plan
`docs/superpowers/plans/2026-09-22-fidelity-gaps.md`) closes the four small
named gaps cycle 3 left open: the five unported `+0x52` handlers, `0x349C8`'s
bit-6/7 deep callees, the loader flush scope, and the attract/scene palette
drivers. This record is the authoritative value source for Tasks 2–5: every
value cites the address, the capture or the measured run that proves it. On any
plan-vs-raw conflict the raw wins, with the correction and its address recorded
(§0.4).

**No porting code is in this task.** The temporary traces the derivation used
were reverted; `git status` is clean.

---

## 0. Addressing, the record model and corrections

### 0.1 The image and the formulas

`data/game/C/PRAGE.EXE` (read-only). Ghidra address == linear address == object
base + offset. Code object `0x10000`–`0x73B14`, data object `0x80000`–`0x10B0CF`.
`DSB/DSW/DSD(addr)` in the port index the same linear addresses. The raw-file
formulas (`VA + 0x52E54` code, `VA + 0x46E54` data) are **pre-fixup**; every
data address here comes from Ghidra (fixups applied) or the port's
`port/decomp/prage.c`.

### 0.2 The two-record model (the correction every handler needs)

The brief and cycle-3 §2.2 say "the handler writes `+0x52`/`+0x53`/`+0x54`".
There are **two** distinct records, each with a `+0x52` and several overlapping
offsets. Getting this wrong puts every write on the wrong record:

* **the slot** `S = DS_001077B0 + side*0x94` (the 0x94-byte state record).
  `DS_001077A8[side] = S` (`0x33CA0`, ported at `fighter.c:173`). The
  **dispatch state `S+0x52`** lives here, as do `S+0x40`, `S+0x41`, `S+0x42`,
  `S+0x43`, `S+0x4C` (word), `S+0x53`, `S+0x54`, `S+0x57`, `S+0x58`, `S+0x5A`,
  `S+0x5D`, `S+0x63`, `S+0x74`, `S+0x78`, `S+0x7A` (char), `S+0x84`, `S+0x8E`,
  `S+0x92`.
* **the actor** `F = DSD(S)` (the animation/position record, a second struct).
  It has its own `F+0x52` (the **animation frame**, `(s8)`), `F+0x58` (the
  **frame step**, `(s8)`), `F+0x18` (x), `F+0x1C` (y), `F+0x08` (cursor),
  `F+0x0C`, `F+0x24`, `F+0x28`, `F+0x29`, `F+0x2C`, `F+0x34`, `F+0x36`,
  `F+0x42`, `F+0x43`, `F+0x4F` (a dword whose high byte is `F+0x52`), `F+0x51`
  (side), `F+0x56`.

Evidence: `FUN_00035f84` reads `side = *(byte*)(param_2 + 0x51)` (actor) and
writes `(&DAT_00107802)[side*0x94] = 0x14` (slot+0x52, `0x36037`) and
`*(iVar1 + 0x24) = 0` (`iVar1 = DSD(slot)`, actor+0x24, `0x3602A`);
`FUN_0003c148(side)` writes `DSD(slot)+0x34/+0x43/+0x42` (`0x3C155`/`0x3C15B`/
`0x3C160`); `FUN_00036638(slot)` writes `slot+0x40/0x41/0x43/0x52/0x53/0x54/
0x57`; `FUN_00035c1c(slot, actor)` writes `actor+0x52` (`0x35C2F`). The two
`+0x52` are independent.

**Handler ABI (from the dispatch disassembly `0x34B6C`, `0x34BF4`):**
`fight_health_sync` sets `ECX = DSD(DS_001077A8 + side*4)` (**slot**),
`ESI = DSD(ECX)` (**actor**), `EDX = side`; it calls each handler with
**EAX = ECX = slot, EDX = ESI = actor**, and the three-argument handlers also
**EBX = side** (`0x34C15`/`0x34C62`/`0x34D78`). `__regparm3` = EAX, EDX, EBX.

### 0.3 Reproduction

* **Static**: Ghidra MCP, project `rage`, program `/PRAGE.EXE`
  (`ghidra_connect_instance` with project `rage`); extents and call graph from
  `port/decomp/prage.functions.csv` / `prage.calls.csv` (fixups applied), and
  `ghidra_decompile_function` / `ghidra_disassemble_function` /
  `ghidra_get_function_callers` / `ghidra_read_memory`.
* **Port**: `cmake --build build`; the front-end driver
  `PR_FRONTEND_DUMP=<dir> PR_GAME_DIR=data/game/C ./build/run_tests` writes
  `select.log` (`loop phase hash`, plus a temporary state/tick trace, reverted)
  and `frame_%04d.raw` (RGB24) from the state-3 entry on. The capture is
  `data/title-captures/frontend/frame_%04d.raw` (3617 frames).
* **The port's helper names** (Tasks 2–5 call these): `0x2BC30` =
  `actors_anim_begin(rec, stream, frame_bits)`; `0x2BCF4` =
  `actors_anim_seek(rec, stream)`; `0x2C3FC` = the voice stub; `0x29BC8` =
  `palette_acquire`-wrapper; `0x1A570`/`0x1A640`/`0x1A5D4`/`0x365C8`/`0x36638`/
  `0x367DC`/`0x188AC`/`0x1922C`/`0x3C480`/`0x3BDDC`/`0x18B04`/`0x2EA30` are
  ported (grep `port/src`).

### 0.4 Corrections against the plan/brief (raw wins)

1. **The `+0x52` state is on the slot, not the actor** (§0.2). The plan's
   "the fields it writes (`+0x52`/`+0x53`/`+0x54`/`+0x5x`)" is ambiguous;
   the dispatch and every state write is on `S`, while `0x35C1C` writes the
   *actor's* `F+0x52` (the animation frame). Evidence: `0x34BF4`
   `MOV AL,[ECX+0x52]` with `ECX = DSD(0x1077A8+side*4)`; `0x35C2F`
   `ADD byte[EBX+0x52],DH` with `EBX = EDX = ESI = actor`.
2. **`0x4F83C` is a distinct code-pointer target, not a tail of `0x4F7F4`.**
   It has its own prologue at `0x4F83C` (`PUSH EBX; PUSH EDX`), its own body
   ending `RET` at `0x4F88C`, and is referenced by four data pointers at
   `0xE8916`, `0xE893C`, `0xE8958`, `0xE896E` (`ghidra_search_byte_patterns`
   for `3c f8 04 00`). Ghidra did not create a function there, which is why
   `get_function_callers 0x4F83C` returns nothing and `get_function_callers
   0x33874` misses the `0x4F872` call; `get_xrefs_to 0x33874` shows both
   `0x4F81F` (in `0x4F7F4`) and `0x4F872` (in `0x4F83C`). The spec's
   representation rule (§Architecture) is confirmed: a distinct named C
   function with its address tag.
3. **The 169-tick static hold is NOT the load/stall model** (§5). The port's
   master-loop gate passes every frame during the hold
   (`DS_00101508 == DS_0010150C` at loop 902..1069); the held frame is
   **byte-identical to capture 830** (port `frame_0314.raw` vs
   `frontend/frame_0830.raw` = 0 bytes differ). It is the **state-9
   match-start countdown screen**, a real divergence in the hold's length, not
   `RES_READ_BYTES_PER_TICK`. The brief's hypothesis is refuted.
4. **`0x35D20`'s animation entry is `0x2BC30` (a *begin*), not `0x2BCF4`.**
   `0x35D4B` calls `0x2BC30` with `EAX = actor`, `EDX = stream`,
   `PUSH 0x3F800000`; the stream is `DSD(0xC8A90 + char*4)` when `S+0x43 & 2`
   else `DSD(0xC8B08 + char*4)` (`0x35D31`/`0x35D3F`). Cycle-3 §2.2 recorded
   only "`0x2BC30(1.0)`"; the stream source is the new value.
5. **`0x33B00`'s copy sources.** The state-19 dispatch (`0x34D36`..`0x34D67`)
   passes `0x33B00(i, 0x107BD0 + i*0x94, 0x107B00 + i*0x68)`: the slot copy
   source is `0x107BD0 + i*0x94`, the actor copy source is `0x107B00 + i*0x68`
   (0x68 bytes = 0x1A dwords). Cycle-3 §2.2 said only "bulk-copies the slot
   record (0x25 dwords) in/out"; the two source bases and the actor's 0x1A
   dwords are the new values.
6. **`0x33874`'s argument order.** The decompile shows one parameter; the
   disassembly (`0x33879`/`0x33883`) shows it uses both EAX and EDX: it
   resolves **EDX** via `0x1B544` and reads the descriptor from **EAX**. The
   drivers call `0x33874(DS_000F0A48, DSD(0xC98A0 + counter*4))`
   (`0x4F809`/`0x4F81D`/`0x4F81F`). The decompile's `param_1` is EAX = the
   palette entry, not the record.
7. **`0x385B0`'s call site is `0x36884`, not a `+0x54` arm** (a fix-round-1
   correction of this record). `0x36870` calls `0x385B0` at its top, guarded by
   `DSW(0x104B00) == 0x25` (`0x36878 MOV DX,[0x104b00]; 0x3687f CMP EDX,0x25;
   0x36882 JNZ 0x3688e; 0x36884 CALL 0x385b0`); the `+0x54` switch is the else
   branch, and its case 2 (`0x36B91`/`0x36BAC`) calls `0x3C520(2.0f)`. The
   first revision placed `0x385B0` in the `+0x54 == 2` arm, which would have
   mis-wired Task 3.
8. **`0x4F83C`'s `DS_00104AD0` bit-0 clear is at function level** (a
   fix-round-1 correction). `0x4F85D JZ 0x4f883` on `F0A48 == 0` jumps straight
   to `0x4F883 AND byte[0x104ad0],0xfe`; the `0x4F881 JC 0x4f88a` skips it only
   when `counter < 10`. The first revision nested the clear inside the
   `F0A48 != 0` branch, which would leave bit 0 set (a one-frame spurious
   enable) on the `F0A48 == 0` path.
9. **`0x33874`'s descriptor is `DSD(0xF0A48)`, not `0xC98A0`** (a fix-round-1
   correction). `0xC98A0` is a ten-handle array (`ghidra_read_memory 0xC98A0` =
   `28 f1 96 03 … 28 ed 96 03`, ten `0x0396xxxx` handles) read only by the two
   drivers (`0x4F810`/`0x4F861`) as the EDX resolver input; the descriptor the
   enqueue walks is its EAX argument, `DSD(0xF0A48)` (the runtime result of
   `palette_acquire(0x396ED28)`, `0x110D8`). The first revision named
   `0xC98A0`, which would have misdirected Task 5.

---

## 1. The five unported `+0x52` handlers (Step 1)

### 1.0 The dispatch and the port's current handling

`fight_health_sync` (`0x34B6C`, body `0x34B6C..0x34D88`) dispatches on
`S+0x52` through the 22-entry table at `0x34B14` (`0x34BF4`:
`MOV AL,[ECX+0x52]; CMP AL,0x15; JA 0x34C08; AND EAX,0xFF; JMP
CS:[EAX*4+0x34B14]`). The five entries this cycle ports:

| `S+0x52` | table entry | handler | entry address |
|---|---|---|---|
| 1 | `0x34C15` | `0x359E0` | `0x359E0` (412 B) |
| 2 | `0x34C26` | `0x35C1C`, then `0x35D20` when the return is non-zero | `0x35C1C` (154 B), `0x35D20` (89 B) |
| 8 | `0x34C8D` | `0x37464` | `0x37464` (475 B) |
| 19 | `0x34D22` | inline `S+0x8E--`, then `0x33B00` per live slot | `0x33B00` (279 B) |
| 20 | `0x34D69` | `0x35E6C` | `0x35E6C` (279 B) |

The port's dispatch is `port/src/game/fight.c:485-528`: cases 1, 2, 8, 19 and
20 are `/* PORT: … unported (§7.10) */ break;`. Every other case is wired.

### 1.1 `0x359E0` — state 1 (`fighter_state_359e0(S, F, side)`)

Disassembly `0x359E0..0x35B7B` (decompile `FUN_000359e0`; the decompile drops
the `*2` on both tables — the disassembly is authoritative):

```
if (0x365C8(S, F, side) != 0) { S[0x43] |= 0x40; 0x35B7C(S, F); return; }   // 0x359EA..0x359FF
if (((DSW(0x1088E0 + side*2) >> 8) & 0xC0) != 0) { 0x367DC(S, F); return; } // 0x35A09..0x35A21
if (0x1A5D4(side) != 0) { if ((S[0x43] & 2) == 0) { 0x35B7C(S, F); return; } }   // 0x35A2B..0x35A40
else { if (0x1A640(side) == 0 || (S[0x43] & 1) == 0) { 0x35B7C(S, F); return; } }// 0x35A4A..0x35A5F
if ((S[0x41] & 0x40) && ((DSW(0x1088E0 + side*2) >> 8) & 5) != 5) S[0x41] &= 0xBF; // 0x35A69..0x35A8F
if ((S[0x41] & 0x40) == 0 && (DSB(0xEF6DC) & 1) == 0) return;                    // 0x35A92..0x35AA7
if (0x1A5D4(side) != 0) {                                                        // 0x35AAD..0x35AF3
    d = (s32)DSD(0xBDBEC + (u8)S[0x7A] * 2) >> 16;                               // 0x35AC8
    if (0x1A570(side) != 0) F[0x18] -= d; else F[0x18] += d;                     // 0x35AD5/0x35AEE
} else {                                                                         // 0x35AF5..0x35B2D
    d = (s32)DSD(0xBDC00 + (u8)S[0x7A] * 2) >> 16;                               // 0x35B05
    if (0x1A570(side) != 0) F[0x18] += d; else F[0x18] -= d;                     // 0x35B12/0x35B2B
}
0x35C1C(S, F);                                                                   // 0x35B34
frame = (s8)(F[0x4F] >> 24);                       // == (s8)F[0x52]             // 0x35B39..0x35B3F
S[0x4C] = (s16)((s8)*(F[0x0C] + frame*2)) << 6;                                  // 0x35B44..0x35B4F
if ((DSW(F[0x28]) >> 8) & 0x40) S[0x4C] = -S[0x4C];                              // 0x35B53..0x35B66
0x1883C(side, (s16)S[0x4C], 0);                                                  // 0x35B6A..0x35B72
```

**Writes:** `S+0x43` (bit 0x40), `S+0x41` (bit 0x40 cleared), `F+0x18` (x),
`S+0x4C` (word), `F+0x52`/`F+0x29`/`F+0x28` (via `0x35C1C`), `S+0x2C`/`S+0x30`
(via `0x1883C`), and `F+0x18` again (via `0x1883C`'s `0x18714`).
**Callees:** `0x365C8`✓, `0x35B7C`✗, `0x367DC`✓, `0x1A5D4`✓, `0x1A640`✓,
`0x1A570`✓, `0x35C1C`✗, `0x1883C`✗ (`✓` = ported, `✗` = new this cycle).

### 1.2 `0x35C1C` + `0x35D20` — state 2

**`0x35C1C(S, F)` → the clamp flag** (`0x35C1C..0x35CB5`; decompile
`FUN_00035c1c`):

```
max = (s8)DSB(0xBDA3E + (u8)S[0x7A] * 2);                    // 0x35C25, the low byte of the word 0x35B7C reads; compared signed (`0x35C47 CMP DL,DH; 0x35C49 JG`)
F[0x52] = (s8)(F[0x52] + F[0x58]);                           // 0x35C2F
r = 0;                                                        // ECX
if ((s8)F[0x52] < 0)        { F[0x52] = max - 1; r = 1; }     // 0x35C3B..0x35C42
else if (max <= (s8)F[0x52]){ F[0x52] = 0;       r = 1; }     // 0x35C47..0x35C50
F[0x29] |= 8;                                                 // 0x35C5A/0x35C7F
if (S[0x43] & 2) EDX = (u16)DSW(DSD(0xC8A68 + (u8)S[0x7A]*4) + ((s8)F[0x52])*2); // 0x35C5E..0x35C77
else             EDX = (u16)DSW(DSD(0xC8AE0 + (u8)S[0x7A]*4) + ((s8)F[0x52])*2); // 0x35C83..0x35C9E
actors_anim_seek(F, EDX);                                     // 0x35CA3 -> 0x2BCF4
F[0x28] |= 4;                                                 // 0x35CA8
return r;
```

**Writes:** `F+0x52` (the animation frame), `F+0x29` (bit 8), `F+0x28` (bit 4),
and `F+0x08`/`F+0x28`/the pset via `0x2BCF4`. **Callees:** `0x2BCF4`✓.

**`0x35D20(S, F)`** (`0x35D20..0x35D78`; decompile `FUN_00035d20`):

```
stream = (S[0x43] & 2) ? DSD(0xC8A90 + (u8)S[0x7A]*4) : DSD(0xC8B08 + (u8)S[0x7A]*4); // 0x35D26..0x35D46
actors_anim_begin(F, stream, 0x3F800000);                    // 0x35D4B -> 0x2BC30
S[0x52] = 9;                                                  // 0x35D53
S[0x43] &= 0xFC;                                             // 0x35D57/0x35D5D
if (S[0x54] == 4) DSB(0x1078FE) = 1;                         // 0x35D60..0x35D65
if (S[0x53] != 0x0D) S[0x53] = 0;                            // 0x35D6C..0x35D72
```

**Writes:** `S+0x52` (= 9), `S+0x43` (bits 0/1 cleared), `S+0x53` (= 0 unless
0x0D), `DS_001078FE` (= 1 when `S+0x54 == 4`), and the actor via `0x2BC30`.
**Callees:** `0x2BC30`✓.

### 1.3 `0x37464` — state 8 (`fighter_state_37464(side)`)

Disassembly `0x37464..0x3763E` (decompile `FUN_00037464`). The dispatch passes
`EAX = side` (`0x34C8D MOV EAX,EDX`):

```
other = 1 - side;
S  = 0x1077B0 + side*0x94;  So = 0x1077B0 + other*0x94;
F  = DSD(S);                Fo = DSD(So);
base = (s16)DSW(DSD(0x1078DC) + (u8)S[0x7A]*14 + (u8)So[0x7A]*2);   // 0x374D1..0x3753C
                                                                     // (0x1078DC is a pointer; 0x374E3 loads [0x1078DC])
if (0x1A570(other) == 0) X = (s16)Fo[0x18] + base;                  // 0x3750B..0x3753C
else                     X = (s16)Fo[0x18] - base;                  // 0x374D1..0x37509
diff = (s16)F[0x18] - X;                                            // 0x3754D
switch (S[0x57]) {                                                  // 0x3754A..0x3755F
case 0: if (|diff| <= 0x200) { S[0x52] = 9; F[0x18] = (s16)X; 0x35B7C(S, F); return; } break; // 0x37565..0x37592
case 1: if (|diff| <= 0x200) { S[0x43] |= 0x40; 0x36638(S, F); return; } break;               // 0x37597..0x375BA
case 2: return;                                                                                // 0x37636
default: break;
}
if (DSB(0xEF6DC) & 1) {                                             // 0x375BF..0x375CE
    0x35C1C(S, F);                                                  // 0x375D8
    frame = (s8)(F[0x4F] >> 24);
    S[0x4C] = (s16)((s8)*(F[0x0C] + frame*2)) << 6;                 // 0x375EC..0x3760C
    if ((DSW(F[0x28]) >> 8) & 0x40) S[0x4C] = -S[0x4C];             // 0x37610..0x37625
    0x1883C(side, (s16)S[0x4C], 0);                                 // 0x37629..0x37631
}
```

**Writes:** `S+0x52` (= 9, case 0), `F+0x18` (case 0), `S+0x43` (bit 0x40,
case 1), `S+0x4C` (default arm), and the `0x35C1C`/`0x1883C` fields.
**Callees:** `0x1A570`✓, `0x35B7C`✗, `0x36638`✓, `0x35C1C`✗, `0x1883C`✗.
**Data:** `0x1078DC` (a **pointer** to the per-character-pair approach table —
the table is at `DSD(0x1078DC)`, 14 B per side-char + 2 B per other-char;
`0x374E3`/`0x3751D` load `[0x1078DC]` before indexing. The pointer's default is
`0xBD89C`, written by `0x36F10` at `0x3704B` and reset by `0x37D18` at
`0x37D7B`; the static image has it 0), `0xEF6DC` (the frame counter).
**Correction (Task 3):** the first revision's `DSW(0x1078DC + …)` omitted the
dereference, so the port's `fighter_state_37464` reads the pointer word as the
table. The faithful fix is `DSW(DSD(0x1078DC) + …)` **plus** the `0x1078DC`
initialization, which the raw performs in `0x36F10` (the unported pose/winner
chain). Until that chain lands the port cannot be faithful here; Task 3 corrects
this record and flags the port (`fighter.c` `fighter_state_37464`'s
`/* TODO(verify) */`) rather than half-fixing it.

### 1.4 `0x33B00` — state 19 (`fighter_state_33b00(side, src, dst2)`)

The dispatch (`0x34D22..0x34D67`) is **inline in `0x34B6C`** and is not a
separate function: it decrements `S+0x8E` and, when the new value is `< 0`,
loops `i = 0..1` over the two slots, calling `0x33B00(i, 0x107BD0 + i*0x94,
0x107B00 + i*0x68)` for every slot whose `S_i+0x52 == 0x13`. The port's
dispatch (`fight.c:520`) is a no-op and must inline this loop.

Body `0x33B00..0x33C16` (decompile `FUN_00033b00`; EAX = side, EDX = src,
EBX = dst2):

```
S = 0x1077B0 + side*0x94;
a = S[0x5A]; b = S[0x5D];                       // 0x33B21/0x33B2D (saved)
memcpy(S, src, 0x94);                           // 0x33B42, 0x25 dwords
0x2EA30(0x2700);                                // 0x33B44, the interrupt lock
F = DSD(S);
f0 = F[0]; f1 = F[1];                           // 0x33B4F/0x33B54 (saved)
memcpy(F, dst2, 0x68);                          // 0x33B62, 0x1A dwords
F[0] = f0; F[1] = f1;                           // 0x33B6D/0x33B79
0x2EA30(0x2700);                                // 0x33B7C
S[0x5A] = a; S[0x5D] = b;                       // 0x33B85/0x33B95
if (DSD(S + 0x08) != 0) { S[0x64] = 0xFF; DSD(S + 0x08) = 0; }   // 0x33B9B..0x33BA9
if (S[0x5A] >= 0x78 && DSW(0x104B00) != 3) 0x36E78(S);          // 0x33BC2..0x33BE1
0x29BC8(side, DSD(S), (u8)S[0x7A]);             // 0x33BF4..0x33C0A
```

**Writes:** the whole `S` (0x94 bytes), the whole `F` (0x68 bytes) except its
first two dwords, `S+0x5A`/`S+0x5D` restored, `S+0x64` (= 0xFF), `S+0x08`
(= 0), and the `0x36E78`/`0x29BC8` fields. **Callees:** `0x2EA30`✓,
`0x36E78`✗, `0x29BC8`✓ (palette re-acquire).

### 1.5 `0x35E6C` — state 20 (`fighter_state_35e6c(S, F)`)

Disassembly `0x35E6C..0x35F82` (decompile `FUN_00035e6c`; on entry **EAX =
slot, EDX = actor** — the dispatch's `0x34D69 MOV EDX,ESI; MOV EAX,ECX`):

```
side = (u8)F[0x51]; other = 1 - side;
So = 0x1077B0 + other*0x94; Fo = DSD(So);
S[0x41] |= 0x80;                                                    // 0x35ED0
0x2C3FC(0x6D);                                                      // 0x35ED9, the voice (out of scope)
bVar = ((DSW(0x1088E0 + side*2) >> 8) & 3) != 0
    && ((DSW(0x1088E0 + side*2) >> 8) & 0xC) != 0;                  // 0x35EE1..0x35F0E
if (!bVar && 0x3BDDC(side) != 0) { 0x18B04(side); return; }          // 0x35F12..0x35F2A
S[0x54] = 0; S[0x52] = 9; S[0x53] = 0;                              // 0x35F2F..0x35F3E
DSW(S + 0x84)++;  DSW(S + 0x92) = 0;                                // 0x35F37..0x35F4F
0x3C480(F, DSD(0xC8B58 + (u8)S[0x7A]*4), 0x40000000);               // 0x35F58..0x35F68
0x188AC(side, F[0x18], 0);                                          // 0x35F6D..0x35F79
```

**Writes:** `S+0x41` (bit 0x80), `S+0x54` (= 0), `S+0x52` (= 9), `S+0x53`
(= 0), `S+0x84` (word, ++), `S+0x92` (word, = 0), and the `0x3C480`/`0x188AC`
fields. **Callees:** `0x2C3FC`✓ (stub), `0x3BDDC`✓, `0x18B04`✓, `0x3C480`✓,
`0x188AC`✓.

### 1.6 The three new helpers

* **`0x35B7C(S, F)`** (158 B, `0x35B7C..0x35C19`; disassembly; EAX = slot,
  EDX = actor):
  `S[0x52] = 2; if (S[0x53] != 0x0D) S[0x53] = 0;`
  `max = (s16)DSW(0xBDA3E + (u8)S[0x7A]*2)` (**a word**; `0x35B99`);
  `frame = (s8)(F[0x4F] >> 24)` (= `(s8)F[0x52]`, `0x35BAD`);
  `if (frame < max/2) F[0x58] = -(s8)(frame/3);`
  `else F[0x58] = (s8)((max - (s8)F[0x52]) / 3);`  (`0x35BB7`..`0x35BF3`;
  the divisions are signed `IDIV`, truncating toward zero);
  `if (F[0x58] == 0 || 0x35C1C(S, F) != 0) 0x35D20(S, F);` (`0x35BF6`..`0x35C0D`).
  **Callees:** `0x35C1C`✗, `0x35D20`✗.
* **`0x1883C(side, a, b)`** (109 B, `0x1883C..0x188A8`; decompile
  `FUN_0001883c`): calls `0x186D0(0)`, `0x186D0(1)` (the screen latch, ported
  at `fighter_slot_latch`), then `S+0x2C += a` (`0x1077DC`),
  `S+0x30 += b` (`0x1077E0`), then `F[0x18] = 0x18714(side)` and
  `F[0x1C] = 0x18788(side)`. Evidence `0x18846`..`0x188A1`.
  **Callees:** `0x186D0`✓, `0x18714`✗, `0x18788`✗.
* **`0x36E78(S)`** (152 B; decompile `FUN_00036e78`): if `S[0x5B] != 0`:
  `S[0x5B] = 0; S[0x5A] = 0x78 - S[0x5B](old); S[0x41] |= 8; return;`
  else `S[0x5D] = 0; S[0x40] = (S[0x40] & 0xFBFFF7FF) | 0x101000;`
  `other = DS_001077A8[(u8)F[0x51] ^ 1]`; if non-zero: `other[0x5D] = 0;
  other[0x43] &= 0xFB; 0x29BC8(...); DSB(0x104AE9) &= 0xFE;`
  Evidence `0x36E82`..`0x36F03`. **Callees:** `0x29BC8`✓.
  **`0x29BC8`'s arguments (fix-round-2):** the record is `DSD(slot)` (`0x36EF0`
  `MOV EBX,[EAX*4 + 0x1077B0]`), the character is `DSB(slot + 0x7A)` (`0x36EF7`
  `MOV DL,[EAX*4 + 0x10782A]`, `0x10782A = 0x1077B0 + 0x7A`), and the side is
  `DSB(0x1078FF)` (`0x36ED5`). The character is the **slot's**, not the actor's;
  a first revision read it from the actor (which is only 0x68 bytes, so `+0x7A`
  is outside the copied record).

### 1.7 Porting plan and test values (§8 holds the substitution table)

* **Task 2** adds `fighter_state_359e0`, `fighter_state_35c1c`,
  `fighter_state_35d20`, `fighter_state_37464`, `fighter_state_33b00`,
  `fighter_state_35e6c`, and the helpers `fighter_state_35b7c`,
  `fighter_1883c`, `fighter_36e78` (one C function per original, header
  `/* 0xADDR — spec §1 */`), and wires the five dispatch cases in
  `fight.c` (including the inline state-19 `S+0x8E` countdown loop).
* `0x35B7C`/`0x35C1C`/`0x35D20` are mutually recursive (`0x35B7C` → `0x35C1C`
  → `0x35D20`; `0x35D20` → `0x2BC30`); declare them before use.
* The `0x33B00` copy is a raw `memcpy` of `mem[]`; use `memcpy(mem + S, mem +
  src, 0x94)` and `memcpy(mem + F, mem + dst2, 0x68)` — **not** `DSD/DSB`
  loops (the 0x25/0x1A dwords include the record's first dwords, which are then
  restored).

---

## 2. `0x349C8`'s bit-6/7 deep callees (Step 2)

### 2.1 `0x385B0` — the slot reset (`fighter_385b0(F)`)

Only caller: `0x36870`, at **`0x36884`**, guarded by `DSW(0x104B00) == 0x25`
(`0x36878 MOV DX,[0x104b00]; 0x3687f CMP EDX,0x25; 0x36882 JNZ 0x3688e;
0x36884 CALL 0x385b0`). `ghidra_get_xrefs_to 0x385B0` returns exactly this one
call. It is **not** in any `+0x54` arm: the `+0x54` switch is `0x36870`'s else
branch, and its case 2 (`S+0x54 == 2`) calls `0x3C520` (`0x36B91`/`0x36BAC`,
`EDX = DSD(0xC89F0 + char*4)`, `PUSH 0x40000000`). `0x385B0` is reached via
`0x36870`, whose own callers are `0x37178` (the `0x349C8` bit-6 arm) and
`0x3FD30`.

Body (disassembly `0x385B0..0x3872F`; `EAX = ECX = param_1` = the **actor**
`F`; `side = F[0x51]`; `S = 0x1077B0 + side*0x94`, `So = 0x1077B0 + other*0x94`):

```
side = (u8)F[0x51]; other = 1 - side; S = 0x1077B0 + side*0x94;      // 0x385BA..0x385E6
DS_00100AF8[side] = 0;                                               // 0x38625
F[0x28] &= 0xDF;                                                     // 0x3863D/0x38649
S[0x62] = 0; S[0x8A] = 0; DSW(S+0x84)++;                            // 0x3864C/0x38657/0x3865F
0x164E8();                                                           // 0x38666
DSD(S+0x40) &= 0xCCF3BFFF;                                           // 0x3866B
F[0x42] = 0;                                                         // 0x38672
S[0x5F] = 0xFF; S[0x55] = 0xFF;                                      // 0x38676/0x3867A
DSD(S+0x0C) = 0; DSD(S+0x10) = 0; DSD(S+0x18) = 0; DSD(S+0x1C) = 0;  // 0x3867E..0x38693
S[0x67] = 0; S[0x65] = 0xFF; DSW(S+0x74) = 0;                        // 0x3869A/0x3869E/0x386A5
if (S[0x54] == 0 || S[0x54] == 1) {                                  // 0x386AB..0x386B2
    S[0x68] = 0; F[0x44] = 0; F[0x43] = 0; F[0x42] = 0; F[0x34] = 0; F[0x36] = 0; DSD(F+0x1C) = 0; // 0x386B4..0x386D2
}
if (S[0x54] != 5) {                                                  // 0x386D9..0x386E2
    S[0x54] = 0; S[0x41] &= 0x7F; F[0x4C] = 0;                       // 0x386E7..0x386F3
    actors_anim_begin(F, DSD(0xC8950 + (u8)S[0x7A]*4), 0x40400000);  // 0x386F7..0x38708
    F[0x4D] = 0x1E; S[0x52] = 0; S[0x53] = 0;                        // 0x3870D..0x38719
} else {
    0x38154(side);                                                   // 0x3871F/0x38724 (the +0x54==5 arm)
}
```

**Callees:** `0x164E8`✗, `0x2BC30`✓, `0x38154`✗ (the `S+0x54 == 5` arm is
already a named gap in the ported `0x36638`, `fighter.c:1236`).

### 2.2 `0x39A10` — the timer write (`fighter_39a10(F, value)`)

Body (decompile `FUN_00039a10`): `word[0x107824 + side*0x94] = value` where
`side = (u8)F[0x51]`, i.e. **`DSW(S + 0x74) = (u16)value`**. Callers
(`ghidra_get_function_callers 0x39A10`): `0x193B0`, `0x36F10`, `0x37D18`,
`0x3FDD8`, `0x47A00`. The cycle's reachable caller is `0x37D18` (the
`0x349C8` bit-7 arm); the others are the pose/winner chain (`0x193B0`,
`0x36F10`, `0x3FDD8`) and `0x47A00` (unported).

### 2.3 Porting plan and test values

* **Task 3** adds `fighter_385b0(F)` and `fighter_39a10(F, value)`, wired at
  the record's call sites: `0x385B0` at `0x36870`'s **`DSW(0x104B00) == 0x25`
  arm** (`0x36884`, the function's top; the `+0x54` switch is the else branch),
  `0x39A10` at `0x37D18` (and, when the pose chain lands, the rest).
* The record does **not** port `0x36870`/`0x37178`/`0x37D18` themselves in this
  cycle: they are the `0x349C8` bit-6/7 arms, which are named gaps
  (`fighter.c:1320`/`:1324`). Task 3 ports the two callees at the call sites the
  record names, so they are reachable only once those arms land. If Task 3's
  step finds the arms unported and the callees unreachable, the honest outcome
  is to wire the callees where the port's `0x36870`/`0x37D18` equivalents will
  call them and record the reachability gap (the repo forbids shipping
  unreachable code).

---

## 3. The loader flush scope (Step 3)

### 3.1 What the raw shows faithful

`0x1C470` (the flush, `FUN_0001c470`) walks the dirty list from the base
`&DAT_00107498` to the head `DAT_00107798`, writes each record's DAC range
through ports `0x3C8`/`0x3C9`, marks it consumed (`piVar7[1] = -1`), then sets
`DAT_00107798 = &DAT_00107498`. Both the loader draw (`0x1C65C` → `0x1C470`,
only when the string is non-empty) and the master loop (`0x25672`) call the
**same whole-list drain**. The scope difference is therefore entirely **what is
on the list at each call**:

* the raw's `palette_acquire` (`0x33754`) enqueues via `0x33734` **after** the
  resource resolve (`res.c:258-265` documents this: `palette_record` runs after
  `res_resolve`), so at the loader draw's `0x1C470` the list holds only the
  initial `0x33734` record and the font palette `0x80997C`;
* the arena's records are enqueued later and **survive to the `0x25672` gate
  flush**.

Evidence: `0x1C470` (`0x1C470`..`0x1C510`), `0x1C65C` (`0x1C6C6`), `0x25672`,
cycle-2 record §9.5 (`2026-09-21-demo-fight-closure-derivations.md:1273-1300`),
cycle-3 record §4.1.

### 3.2 The port's divergence (cycle-3 §4.1)

The port's loader draw (`res_load_present` → `text_blit_string` →
`gfx_flush_palette`, `actors.c:1590`) drains the arena's records instead, so its
gate flush is empty and its DAC already holds the arena palette at the loader
frame. Cycle-3 §4.3 measured the divergence **oracle-neutral**: removing the
loader's `gfx_flush_palette()` and re-dumping produced **1381/1381
byte-identical frames** — the per-iteration gate flush drains the same records,
so the loader flush's scope is invisible to the dump. That is the *context*,
not the goal: the raw's scope is still the faithful one.

### 3.3 The exact change

The port must make the loader draw's flush drain exactly the records present at
that call, as the raw does, while the gate flush drains the rest. Three files,
matching the raw's owners:

* **`port/src/platform/gfx.c`** — `gfx_flush_palette()` gains a boundary: it
  drains from the base to a **loader boundary cursor** that `res_load_present`
  sets to the current head *before* the loader draw, and to the head for the
  gate call. The raw's `0x1C470` is the same routine at both sites; the
  boundary is the port's model of "the list contents at that call", which the
  raw gets for free from its enqueue order. `palette_record` (0x33734) is
  unchanged.
* **`port/src/platform/res.c`** — `res_load_present` captures the head before
  calling `text_blit_string` and passes it to the scoped flush (the raw's
  `res_resolve`-then-`palette_record` order at `0x1B5E0`/`0x1B47A`).
* **`port/src/game/flow.c`** — the gate's `gfx_flush_palette()` (`0x25672`,
  `flow.c:1256`) drains to the head (unbounded), so the arena's records reach
  the DAC at the gate, exactly as the raw's `0x25672`.

**The raw pins the ordering, not a new boundary primitive.** If a Task-4 trace
shows the port's loader draw already runs before the frame's palette records
(so the divergence is elsewhere), the change reduces to the ordering fix and the
boundary is dropped; the record must not ship a boundary that the raw does not
need. The raw-pinned facts are: the two call sites, the whole-list drain, and
`palette_record`-after-`res_resolve`.

### 3.4 The assertion that proves the scope

In `port/tests/test_frontend.c`: drive the state-6 loader path and assert

* the records present at the loader draw are the initial `0x33734` record and
  the font palette `0x80997C` (their `{ptr; first; count; flag}` tuples), and
* the arena's records (`0x0A838B44`, `0x0105FF3C`, `0x1BB9FCD8`, …) are still
  on the list **after** the loader draw and reach the `0x25672` gate flush.

The pre-fix port fails it (the loader draw drains the arena's records and the
gate list is empty). Mutation proof: revert the scope → the assertion fails.
The measured oracle-neutrality (1381/1381) is re-confirmed on the ladder.

### 3.5 Task-4 correction (the raw wins; measured)

**§3.1's second half and §3.3 are refuted by the raw and by a live measurement.**
The raw's `0x1C470` is a **whole-list** drain at every call (`0x1C470`..`0x1C510`,
no scope parameter — §3.1's first sentence), and the original's state-6 loader
draws drain the accumulated records **including the arena's**:

* `ghidra_disassemble_function 0x1C470` confirms the drain is
  `piVar7 = &DAT_00107498` → `DAT_00107798`, then `DAT_00107798 = &DAT_00107498`.
* Live DOSBox-X poll (base `0x266000`, `/tmp/t4_dirty_base.py`, which writes the
  poll's `/tmp/t4_dirty.csv`): the state-6 handler reads its files
  **incrementally** (file reads gain `21`, `55`, `60`, `5`, `33`, `36`, `32`,
  `57`, `34` over 53.37→54.32 s), and the dirty list goes `{002a3470,0,1,0} {0080997c,1,1,1}`
  (the loader draw, 98.339 s) → `0` → **9 arena records** (98.379 s) → `0`
  (98.392 s, still inside the handler's blocking read: `1508` climbs 4→25 while
  `150C` is frozen at 3). So the arena records are drained by a **subsequent
  loader draw**, not "survive to the `0x25672` gate flush".

The port's loader draws do the same (traced: `{0080997c}` → 5 arena records → 4
→ 3 → 0 → 0), so **the port's loader flush scope is already faithful** and
§3.3's boundary is **not shipped** — the raw does not need it (§3.3's own caveat:
"the record must not ship a boundary that the raw does not need"). Implementing
it literally drains nothing at the first draw and loses the font record (the
boundary is captured before the glyph walk's own `0x80997C` acquire), so it is
strictly less faithful.

**The pinned divergence §3.4 names is the initial record.** The raw's
`0x336C0` ends with a `0x33734` enqueue at `0x336F6`-`0x3370A`
(`EBX = 0xBD470`, `EDX = 1`, `EAX = 0`: the tuple `{ ptr = 0xBD470; first = 0;
count = 1; flag = 0 }`); the port's `palette_list_init` omitted it. The record's
data word is zero (the raw `0xBD470` reads `00000000`), so the upload is black
(DAC[0]) — the same value the port's `gfx_dac` clear leaves — but it is the
record the raw's loader flush drains. Task 4 enqueues it
(`palette_list_init`), so the loader draw's list is `{initial, font}` as the
raw's.

**Out of this task's scope:** the loader-frame **gate** (the raw's fails because
the read blocks `150C`; the port's passes because its payloads are resident) is
the read/gate model cycle-2 §9.6 measured and deferred — not the flush scope.

---

## 4. The attract/scene palette drivers (Step 4)

### 4.1 The dispatcher and the mask

`attract_scene_tick` (`0x292AC`, ported at `attract.c:19`) iterates the 16-bit
mask `DS_00104AD0`; for each set bit `i` it calls
`(*(code*)(DS_000A8744 + i*4))(i)` (`0x292BA`..`0x292CE`). The table at
`0xA8744` reads `{0x0004F7F4, 0x0005D812, 0x0005D812, …}` (Ghidra
`read_memory`): **entry 0 is `0x4F7F4`; entries 1..15 are the `0x5D812` stub**
(`xor eax,eax; ret`). `0x292AC`'s only caller is `FUN_000255CC` (the master
loop; ported at `flow.c:1228`).

### 4.2 `0x4F7F4` — the advance (`FUN_0004f7f4`, 70 B)

Disassembly `0x4F7F4..0x4F839`:

```
if (DSD(0xF0A48) != 0) {                                   // 0x4F7F6, the attract palette entry
    record = DSD(0xC98A0 + (u8)DS_001088F1 * 4);           // 0x4F7FF..0x4F810
    DS_001088F1++;                                          // 0x4F80E/0x4F817
    0x33874(DS_000F0A48, record);                          // 0x4F81D/0x4F81F
    if ((u8)DS_001088F1 < 10) return;                      // 0x4F824..0x4F82E
}
DS_00104AD0 &= 0xFE;                                       // 0x4F830 (clears bit 0)
```

The decompile's `FUN_00033874(param_2)` is wrong: EAX at the call is
`DS_000F0A48` (`0x4F809`) and EDX is the record (`0x4F81D`); `param_2` is
unused. The `EAX = DSD(0xF0A48)` load is the port comment's "overwrites eax
from DS_000F0A48" (`attract.c:26`).

### 4.3 `0x4F83C` — the start (`FUN_0004f83c`, 80 B) — shape resolved

**A distinct code-pointer target** (§0.4.2): its own prologue at `0x4F83C`,
body `0x4F83C..0x4F88C`, `RET` at `0x4F88C`, four data pointers at `0xE8916`,
`0xE893C`, `0xE8958`, `0xE896E`.

Disassembly `0x4F83C..0x4F88C`:

```
DS_00104AD0 |= 1;                                          // 0x4F83E..0x4F849 (sets bit 0)
DS_001088F1 = 0;                                           // 0x4F847/0x4F855
if (DSD(0xF0A48) != 0) {                                   // 0x4F84F/0x4F85B
    DS_001088F1 = 1;                                       // 0x4F85F/0x4F86C
    0x33874(DS_000F0A48, DSD(0xC98A0));                    // 0x4F861..0x4F872
    if ((u8)DS_001088F1 < 10) return;                      // 0x4F877..0x4F881 (JC 0x4F88A)
}
DS_00104AD0 &= 0xFE;                                       // 0x4F883 (function level: reached from the
                                                           //  F0A48 == 0 path via JZ 0x4F883, and from
                                                           //  the counter >= 10 path)
```

So `0x4F83C` **starts** the sequence (bit 0, counter = 1, enqueue record[0]);
the bit-0 clear is at **function level** — it runs on the `F0A48 == 0` path
(`0x4F85D JZ 0x4F883`) and on the counter-≥10 path, so `0x4F83C` leaves bit 0
set only on the `F0A48 != 0` path (where `counter == 1 < 10` skips it via
`0x4F881 JC 0x4F88A`). `0x4F7F4` **advances** it (counter++, enqueue
record[counter], clear bit 0 at ≥ 10 — the same function-level shape). The
`0xC98A0` array holds the ten per-step **handles**; `DS_000F0A48` is the
attract's palette **entry** (set at `attract.c:168`, `0x110D8`).

### 4.4 `0x33874` — the palette reflow / re-enqueue (`FUN_00033874`, 143 B)

Called by both drivers (`get_xrefs_to 0x33874` = `0x4F81F`, `0x4F872`) as
`0x33874(DSD(0xF0A48), DSD(0xC98A0 + counter*4))`. Its **EAX** argument
(`EBX = EAX`, `0x3387F`) is the **descriptor**; its **EDX** argument is the
**handle**, resolved via `0x1B544` (`0x33881`/`0x33883`). The two are distinct
structures:

* **the descriptor = `DSD(0xF0A48)`**, the palette ownership table entry the
  attract's `palette_acquire(0x396ED28)` returned (`0x110D8` stores it into
  `DS_000F0A48`). Its layout is the port's own `palette_acquire` entry
  (`actors.c:270-273`): `{handle; rc; start; len}` at 0x10 stride, so the
  enqueue's `[EAX]` = handle, `[EAX+8]` = start, `[EAX+0xC]` = len.
* **the handle array = `0xC98A0`**: ten resource handles
  (`ghidra_read_memory 0xC98A0` = `28 f1 96 03 … 28 ed 96 03`, ten
  `0x0396xxxx` values), read only by the two drivers
  (`0x4F810`/`0x4F861`) as the resolver input. The enqueue never references it.

The function reflows the palette table from the descriptor and re-enqueues the
changed entries onto the `DS_00107798` dirty list (`0x33879` = the head):

* if `descriptor.len < resolved.count`: walk the table from the descriptor at
  `EAX+0x10` strides (bounded by `0x107798`), and for each entry whose
  `prev.start + prev.len > next.start`, set `next.start = prev.start + prev.len`
  and append `{next.handle; next.start; next.len; 1}` (`0x338B4`..`0x338F5`);
* else if `handle != descriptor.handle`: set `descriptor.handle = handle` and
  append `{handle; descriptor.start; resolved.count; 1}`
  (`0x3389B`..`0x338AF`).

The record's fields map to the port's `palette_record`/`gfx_flush_palette`
tuple `{ptr; first; count; flag}`; `flag = 1` means "resolve `ptr`" in
`gfx_flush_palette` (`gfx.c:97`). **The walk branch is NOT the same reflow as
`palette_acquire`'s tail (`actors.c:276-283`)** — the first revision's claim was
refuted by the raw and Task 5's port (§8.4): `0x33874` has **no zero-entry skip**
(no `CMP [EAX],0` counterpart at `0x338C4`; `palette_acquire` has one at
`0x337E9`) and its break is **`<=`** (`0x338D1 JLE`) where `palette_acquire`'s is
**`==`** (`0x337FB JZ`), so `palette_reflow` implements `0x33874`'s own shape.
The descriptor's layout and the branch shapes are pinned. The **residual Task-5
verification** — the exact `[resolved]` semantics and whether the reflow is
byte-identical to `palette_acquire`'s tail — is **resolved**: `0x1B544`
preserves `EDX` (`PUSH EDX 0x1B546`; `POP EDX` at `0x1B578`/`0x1B5A2`/`0x1B608`),
so `0x33897` compares the handle, and the walk is not byte-identical (above). No
`0xC98A0` read is needed for the descriptor.

### 4.5 The `DS_00104AD0` bit-0 protocol and the `DS_001088F1` counter

* `DS_00104AD0` is the 16-bit driver mask (`attract_scene_tick`); bit 0 alone
  has a handler (`0x4F7F4`). `0x4F83C` sets it and, at **function level**,
  clears it when the counter is not `< 10` — so it leaves bit 0 set only on the
  `F0A48 != 0` path (counter 1); `0x4F7F4` clears it at counter ≥ 10.
* `DS_001088F1` (a byte) is the step counter: `0x4F83C` = 1, `0x4F7F4`
  increments; the sequence runs 10 steps (`0x4F82B`/`0x4F87E` compare to 0xA).
* The gate `DS_000F0A48` (the attract palette entry) is non-zero only once the
  attract's phase-2 palette is acquired (`0x110D8`, `attract.c:168`).

### 4.6 The attract-215 question

**Yes, the attract oracle's first divergence at capture 215 is
palette-related.** Cycle-2 §9.5 (lines 1273-1300) measured it: capture 215 is
**black plus exactly the loader's 498 bytes** (`- LOADING -`), while the port's
title phase-0 frame presents the composed title content (105 222 non-black
bytes). The mechanism is the **presented DAC state at the frame where the draw
lands**: the port's frame-end flush (`0x25672`) has uploaded the frame's
acquired palettes before the present; the capture shows only the loader's own
range (`0x1C65C` → `0x1C470`, uploading `0x80997C`).

The `0x4F7F4`/`0x4F83C` animation **is on the attract path** (it is dispatched
every frame by `0x292AC` while bit 0 is set) but it is **not the 215
mechanism** — 215 is the §3 flush scope. So Task 5's drivers are measured
against the attract palette animation (the DAC range the drivers re-enqueue),
and the 215 claim's owner is §3, not §4. Porting the drivers alone should not
move 215; the ladder confirms it.

### 4.7 Porting plan and test values

* **Task 5** adds `attract_palette_advance` (`0x4F7F4`),
  `attract_palette_start` (`0x4F83C`, a distinct named function per the spec's
  representation rule) and `attract_palette_enqueue` (`0x33874`), wired into
  `attract_scene_tick` (`attract.c:19`) — replace the `fn_resolve` no-arg call
  for table entry 0 with the direct `0x4F7F4` call (the raw's `0x292C1`).
  `0x4F83C`'s four `0xE8916`-table callers stay unwired until the scene chain
  that uses them is ported; the record names them so the reachability is
  explicit (the repo forbids shipping unreachable code — Task 5 records whether
  wiring `0x4F83C` is reachable).
* Test values: seed `DS_000F0A48 = 1`, `DS_001088F1 = 3`, `DS_00104AD0 = 1`;
  call `0x4F7F4()`; assert `DS_001088F1 == 4`, the enqueue used
  `DSD(0xC98A0 + 3*4)`, and `DS_00104AD0` still has bit 0. Seed
  `DS_001088F1 = 9` → after the call `DS_00104AD0 & 1 == 0`. Call `0x4F83C()`
  → assert `DS_00104AD0 & 1 == 1`, `DS_001088F1 == 1`, and the enqueue used
  `DSD(0xC98A0)`.

---

## 5. The 169-tick static hold (Step 5)

### 5.1 The port's trace

`PR_FRONTEND_DUMP` + a temporary `select.log` trace (state and tick pair,
reverted) over the 2000-loop window:

| loop | state (`DS_000F0A64`) | phase (`DS_000F0A6F`) | `DS_00101508` | `DS_0010150C` | `DS_001014FC` | log hash (`E87A4`) |
|---|---|---|---|---|---|---|
| 829 | 9 | 1 | 240 | 240 | 0 | changing |
| 901 | 9 | 1 | 312 | 312 | 0 | 2263017225 |
| 902..1069 | 9 | 1 | 313..480 | 313..480 | 0 | **419846186 (constant)** |
| 1069 | **6** | 1 | 480 | 480 | 0 | 419846186 |
| 1070 | **7** | 0 | 65 | 65 | 0 | 952198597 |

The state sequence is `3` (loop 589) → `9` (829) → `6` (1069) → `7` (1070) →
`0` (1970). The **dumped** frames are byte-static for dump 312..480 = loop
901..1069 (**169 frames**, exactly the brief's figure; the log's presented
buffer `DS_000E87A4` goes constant one frame later, at loop 902 — the dump hook
reads `gfx_display()`/`DS_000E87A0`, which is a frame ahead of the log's
`DS_000E87A4`). **The gate passes every frame** (`DS_00101508 == DS_0010150C`
throughout), and `DS_001014FC` is 0 — so `RES_READ_BYTES_PER_TICK` (which
advances `DS_00101508` only at `res_load_present`, `res.c:60`, the state-6
load) is not involved.

### 5.2 The verdict: a real divergence, not the load/stall model

* The held frame is **byte-identical to capture 830**: port `frame_0314.raw`
  (and every dumped frame 312..480) vs
  `data/title-captures/frontend/frame_0830.raw` = **0 bytes differ** (dump 311
  differs by 4 629 B). The port's state-9 render is correct at that point.
  **Reproducible:** `PR_FRONTEND_DUMP=/tmp/ff PR_FRONTEND_DUMP_FRAMES=315
  PR_GAME_DIR=data/game/C ./build/run_tests` writes the dump; the measurement is
  `dump 310/311 = 4 629 B`, `dump 312..314 = 0 B` against `frame_0830.raw`
  (re-run at fix-round 1; the earlier `/tmp/ffhold*` run's frames were the same).
* The port reaches capture-830's content at loop 901 (dump 312) and holds it
  for 169 frames; the capture reaches it at 830 and the loader follows at 831.
  The capture's corresponding frames (by the front-end window's capture↔port
  mapping, capture ≈ loop−29) change by 10 000–150 000 bytes per frame
  (measured: capture 829→830 = 4 629 B, 830→831 = 119 130 B, 900→901 =
  120 377 B — so the capture's state-9 screen animates right up to the loader).
* State 9 is the **match-start countdown** (`flow.c:1432-1438`; entered with
  `DS_000F0A6A = 0x12C`, `DS_000F0A6C = 6` from the state-5 arm `0x11E1D`/
  `0x11E2E`/`0x11E35`). The divergence is the countdown's **screen animation
  stopping ~169 ticks before the countdown exits**, not the countdown's
  arithmetic (`0x12C` is raw-faithful) and not the master-loop gate.

**So it is a real divergence, owned by the state-9 screen's actor-animation
advance** (the screen reaches its final pose and holds while the countdown
continues; the capture's animates until the loader). It is **oracle-neutral**:
the front-end oracle's window ends at port frame 258 and the demo oracle's
starts at port frame 481, so port frames 259..480 are uncovered and no enforced
claim moves. It is recorded as a named gap (§7), not fixed in this cycle (its
owner is the state-9 animation, not one of the four in-scope gaps).

---

## 6. The size-gate verdicts (Step 6)

### 6.1 The method (cycle-3 §10.4)

From `prage.calls.csv`/`prage.functions.csv`; "new" = not the `0x6xxxx`
runtime, not the RNG (`0x5Dxxx`), not the five known stubs (`0x2C3FC`,
`0x2EA64`, `0x62002`/`0x62003`/`0x6201B`), and not named anywhere in
`port/src` except the generated `symbols.h`. Stubs are leaves (their callees
are the audio path, out of scope). The "named in a comment" caveat: a function
merely *named* in a port comment counts as ported, so the **naive** figure
undercounts; the **true-new** adds the roots that are named-but-unported (the
handlers/callees themselves). `0x4F83C`'s size (80 B) is from its extent
(`0x4F83C..0x4F88C`); Ghidra has no function there.

### 6.2 Per group

| group | closure | naive-new | true-new (naive + named roots) | in-scope genuinely-new |
|---|---|---|---|---|
| A — the five handlers (`0x359E0`,`0x35C1C`,`0x35D20`,`0x37464`,`0x33B00`,`0x35E6C`) | 256 f / 35 375 B | 15 f / 2 079 B | **21 f / 3 767 B** | **10 f / 2 223 B** |
| B — the deep callees (`0x385B0`,`0x39A10`) | 206 f / 28 309 B | 14 f / 1 974 B | **14 f / 1 974 B** | **2 f / 418 B** |
| C — the loader flush scope | — | 0 f / 0 B | 0 f / 0 B | **0 f / 0 B** |
| D — the attract drivers (`0x4F7F4`,`0x4F83C`,`0x33874`) | 178 f / 20 317 B | 13 f / 1 767 B | **14 f / 1 837 B** | **3 f / 293 B** |
| union A+B+D | 262 f / 36 098 B | 20 f / 2 732 B | **27 f / 4 490 B** | **15 f / 2 934 B** |

**The inflation, named with its path.** Both the naive and the true-new figures
are inflated by ~2 000 B / 11 functions of the original's **resource-loader /
allocator / runtime** path, which the port already replaces. The BFS reaches it
through the resolver `0x1B544` — e.g. `0x33874 → 0x1B544 → 0x1E774 →
0x1C308 → 0x1E458 → 0x1E30C → 0x1E62C` (`0x1E30C` 329 B, `0x1E458` 465 B,
`0x1E62C` 169 B), `0x1B544 → 0x1B3AC → 0x1C65C → 0x1C5E8 → 0x1C528`
(190 B), and `0x1B544 → 0x1D290 → 0x1BE30 → 0x1D018 → 0x5D86A → … →
0x1071D/0x107EF/0x108E8/0x109CA` (337 B). `res_resolve`/`res_alloc` are the
port's replacements; these are not new work for this cycle. The same path
inflates the group-D closure the spec already measured at "~3 functions / ~300 B
of palette work" — the in-scope column reproduces it exactly.

The in-scope genuinely-new per group is therefore: **A 10 f / 2 223 B, B 2 f /
418 B, C 0, D 3 f / 293 B; union 15 f / 2 934 B** — below both thresholds
(4 KB, 20 functions).

### 6.3 Verdicts

* **A (the five handlers): fits this cycle.** In-scope 10 f / 2 223 B.
* **B (the deep callees): fits this cycle.** In-scope 2 f / 418 B.
* **C (the loader flush scope): fits this cycle.** A scope change, no new
  functions.
* **D (the attract drivers): fits this cycle.** In-scope 3 f / 293 B (the
  spec's own measurement).
* **No group becomes a follow-on cycle.** The union's in-scope work is
  15 f / 2 934 B. The mechanical true-new (27 f / 4 490 B) crosses the 4 KB
  line only because of the shared resource-loader/runtime path named above.

**Task order (the record's authority):** A (2 223 B) and C (0 B) are
independent; B (418 B) and D (293 B) are the smallest. The provisional
smallest-first order stands except that C (the flush scope) has the lowest
line-count risk and should go first if the reviewer wants the oracle-neutral
change banked early; the record leaves the order to the reviewer.

---

## 7. Named gaps — the final inventory (Task 6)

**Closed in this cycle** (each with its assertion and the ladder's result):

1. **The five unported `+0x52` handlers** (Task 2, `83253c2`/`a344f54`). Ported
   and dispatched: `fighter_state_359e0` (state 1), `fighter_state_35c1c` +
   `fighter_state_35d20` (2), `fighter_state_37464` (8), `fighter_state_33b00`
   (19, with its inline `+0x8E` countdown wired in `fight.c`),
   `fighter_state_35e6c` (20). Assertion: 29 `CHECK_EQ_INT` in
   `test_fight.c:check_gap_handlers`, six mutation proofs reproduced. Ladder
   green, every enforced claim unmoved. **Closed.**
2. **`0x349C8`'s bit-6/7 deep callees** (Task 3, `019e404`/`7c0cd91`/`9b88b20`/
   `15c08e1`/`847e228`). `0x385B0` wired at `0x36884` (under
   `DSW(0x104B00) == 0x25`), `0x39A10` at `0x37D57`. The callees' call sites
   live in their callers, so Task 3 ported the **minimal caller chain**
   (`0x37178`/`0x36870`/`0x379C4`/`0x164E8` for bit 6, `0x37D18` for bit 7 — 5
   functions / ~1 741 B) to keep them reachable, and the human **ratified** the
   scope expansion (the reviewer audited it function-by-function: faithful,
   needed, minimal). Assertion: 15 `CHECK_EQ_INT` in
   `test_fight.c:check_deep_callees`, five mutation proofs. **Closed.**
   (The record §2.3 had the callers out of scope; the ratification is the same
   logic as the `0x13xxx` re-scope inverted — there the callers were unreachable,
   so the work went out; here the callees must stay reachable, so the minimal
   caller chain came in.)
3. **The loader flush scope** (Task 4, `abd804d`). The record §3's premise — a
   "scoped flush" boundary — was **refuted by the raw**: `0x1C470` is a
   **whole-list drain** (`while (piVar7 != DAT_00107798)`, no scope parameter;
   three call sites `0x1C6C6`/`0x25672`/`0x2EADB`), the original's loader draws
   drain the accumulated records, and the port's unscoped flush was already
   faithful (§3.5). The real gap was the **missing initial record**
   `{0xBD470, 0, 1, 0}` (`0x336C0`'s `0x33734` enqueue: `EBX = 0xBD470`,
   `EDX = 1`, `EAX = 0`), now enqueued via `palette_record`. Assertion:
   `test_frontend.c:485-489` (seed `0xDEADBEEF`, post-condition `0xBD470`),
   mutation reproduced (dropping the enqueue gives the five reported failures).
   **Closed** (re-scoped by the raw; the raw wins).
   *Carried concern (out of scope):* the record §3 conflates the flush scope with
   the loader-frame **gate** — the raw's gate fails while the port's passes
   because the port's payloads are resident (cycle-2 §9.6's read/gate model).
   Named, not fixed here.
4. **The attract/scene palette drivers** (Task 5, `5ba5904`). `0x4F7F4`
   (`attract_palette_advance`), `0x4F83C` (`attract_palette_start` — a distinct
   named function per the spec's representation rule, Q10) and `0x33874`
   (`palette_reflow`) ported; `0x4F7F4` registered into `attract_scene_tick`.
   37 assertions, 4 mutation proofs. The attract claim is **unmoved**
   (`FIRST DIVERGENCE at capture frame 215`): `DS_00104AD0` bit 0 is never set at
   runtime (image value 0; the only setter is `0x4F83C`, whose callers are
   unported), so neither driver runs. **Closed.**
   *Accepted exception (Task 5's I1):* `0x4F83C` ships with **no production
   caller** (its four `0xE8916`-table callers are unported), so it is test-only —
   the shape the repo's no-unreachable-code rule normally forbids. It is
   brief-mandated (the spec's representation rule, Q10) and carried here as an
   **explicit accepted exception**, not silently shipped; the reviewer required
   it be carried as one.
5. **`0x33874`'s residual semantics** (§4.4/§8.4). **Closed** by Task 5: the
   descriptor is `DSD(0xF0A48)`, the handle `DSD(0xC98A0 + counter*4)`, the
   resolver `0x1B544` preserves `EDX` (so `0x33897` compares the handle), and the
   walk branch is **not** byte-identical to `palette_acquire`'s tail (no
   zero-entry skip; break `<=` vs `==`). `palette_reflow` implements `0x33874`'s
   own shape.

**Carried forward (out of scope, with owners):**

6. **The state-9 hold's screen animation ends ~169 ticks before the countdown
   exits** (§5). The port holds capture-830's frame byte-static for loop
   902..1069; the capture's state-9 screen animates until the loader at 831.
   **Owner:** the state-9 screen's actor-animation advance (the countdown's
   render content), not the load model and not one of the four in-scope gaps.
   Oracle-neutral (neither oracle covers port frames 259..480). Evidence: §5.1's
   trace, the byte-identical `frame_0314.raw` ↔ `frame_0830.raw`. **Not closed;
   out of this cycle's scope.**
7. **`0x38154`** (the `S+0x54 == 5` arm of `0x36638`/`0x385B0`) stays the
   existing named gap (`fighter.c:1236`); no in-scope path reaches it.
8. **The pose/freeze subsystem** (`0x19020` → `0x193B0` → `0x3B714` → `0x3AAFC`
   → the pose family; 68 new funcs / 10 467 B) → **cycle 4** (cycle 3's record
   §10; the size gate triggered).
9. **The demo oracle's `res is None`** (`tools/title_compare.py:484-514`) →
   **cycle 4**: it is the freeze's symptom — of the port's 398 distinct
   demo-frame hashes exactly one appears in the whole 3617-frame capture, at
   capture 830, which `capture_lo = 831` excludes. The oracle's report-only
   fallback is correct behaviour and is not repaired.
10. **The `0x13xxx` effect call sites** (`0x29B74`, `0x41578`) → **the
    interactive match**: the caller functions are reachable only via `0x24C5C`'s
    unported modes and the menu/match chain (`0x277C0`, `0x28788`, `0x27A2C`,
    `0x416D4`, `0x41C28`, the input handlers); porting them alone ships
    unreachable code.
11. **831/832's held-frame presentation** — un-derivable (the post-read ISR tick
    count is a host/emulator property, two live-RAM polls disagreeing Δ=2 vs
    Δ=3; cycle-2 §9.6).
12. **The interactive match** (the mode graph `DS_00104B00`, the `0x257A4` coin
    divert, `0x1EEB0`, `0x1F458`, the player screens and human input) —
    **unowned** by any cycle to date.
13. **The audio gaps** — streamed Smacker audio (2b-ii) and the attract's
    `0x2C3FC` voice calls remain.

No gap is dropped and no claim is stronger than its evidence: the four in-scope
gaps are closed with their assertions and the unmoved ladder; every out-of-scope
item carries its owner.

---

## 8. Unit-test values (the substitutions for Tasks 2–5)

### 8.1 Task 2 — the five handlers (`port/tests/test_fight.c`)

Each test seeds a sentinel that differs from the post-condition (never an
unseeded BSS-zero), calls the handler, and asserts the raw's post-state. The
slot base is `S = DS_001077B0 + side*0x94`; `F = DSD(S)`.

* **`0x35D20`:** seed `S[0x52]=0xFF`, `S[0x43]=0xFF`, `S[0x53]=0x05`,
  `S[0x54]=4`; call `0x35D20(S, F)`; assert `S[0x52]==9`, `S[0x43]==0xFC`,
  `S[0x53]==0`, `DSB(0x1078FE)==1`. Mutation: revert the `S[0x43]` clear →
  `0xFF != 0xFC`.
* **`0x35C1C`:** seed `F[0x52]=0`, `F[0x58]=0xFF` (a step of −1) and
  `S[0x7A]=0`; `max = DSB(0xBDA3E)`; assert `F[0x52] == max-1` (the underflow
  clamp) and the return `== 1`; and `F[0x29] & 8`, `F[0x28] & 4`. Mutation:
  revert the clamp → `F[0x52] == 0xFF`.
* **`0x359E0`:** seed `S[0x41]=0x40`, `DSB(0xEF6DC)=1`, `S[0x7A]=0`,
  `F[0x52]=0`, `F[0x0C]=0x1000`, `F[0x28]=0`, `F[0x18]=0x400`; call
  `0x359E0(S, F, 0)`; assert `S[0x4C] == (s16)((s8)*(mem+0x1000)) << 6` and the
  `F[0x18]` delta. Mutation: drop the `<< 6` → the assertion fails.
* **`0x37464`:** seed `S[0x57]=0`, `S[0x7A]=0`, `So[0x7A]=0`, `F[0x18]=0x100`,
  `Fo[0x18]=0x100 + base`; call `0x37464(0)`; assert `S[0x52]==9`,
  `F[0x18]==(s16)(Fo[0x18] ± base)`, and `0x35B7C` ran (`S[0x52]` transitions
  through 2 if it did). Mutation: perturb `base` → the assertion fails.
* **`0x33B00`:** seed the `0x107BD0 + i*0x94` source and the `0x107B00 + i*0x68`
  actor source with sentinels; call `0x33B00(0, src, dst2)`; assert the slot's
  0x94 bytes equal the source (except `S[0x5A]`/`S[0x5D]` restored) and the
  actor's 0x68 bytes equal the actor source with `F[0]`/`F[1]` restored.
  Mutation: swap the two sources → the assertion fails.
* **`0x35E6C`:** seed `S[0x54]=0xFF`, `S[0x52]=0`, `S[0x53]=0xFF`,
  `DSW(S+0x84)=0`, `DSW(S+0x92)=0xFF`; call `0x35E6C(S, F)`; assert
  `S[0x52]==9`, `S[0x53]==0`, `S[0x54]==0`, `DSW(S+0x84)==1`,
  `DSW(S+0x92)==0`, `S[0x41] & 0x80`. Mutation: revert the `S+0x84` increment →
  `0 != 1`.

**Task-2 implementation corrections (the raw wins).** The recipes above are
incomplete against the raw control flow and the unit process (which loads no
image), so the shipped assertions in `port/tests/test_fight.c`
(`check_gap_handlers`) seed more, and the two `0x35C1C`/`0x359E0` mutations
differ:

* **`0x35C1C`:** `max = DSB(0xBDA3E)` is 0 without the image, so the record's
  seed (`F[0x52]=0`, `F[0x58]=0xFF`) clamps the frame to `max-1 = 0xFF` and the
  raw's seek index `(s8)F[0x52]*2 = -2` reads `mem[-2]` — out of bounds. The
  test seeds `DSB(0xBDA3E)=4` (max), so the frame clamps to 3 and the index is
  `mem[6]`. The record's mutation ("revert the clamp") would crash on that same
  `-2`; the shipped mutation writes `max` instead of `max-1` (index `mem[8]`)
  and fails `3 != 4` at the frame assertion.
* **`0x359E0`:** the record's seed leaves `0x1A5D4`/`0x1A640` zero and
  `S[0x43]` bit 1 clear, so the raw takes the `0x35B7C` early return at
  `0x35A5F` and never reaches the `S[0x4C]`/`F[0x18]` tail the assertion reads
  (raw `0x35A2B`..`0x35A40` / `0x35A4A`..`0x35A5F`). The test sets command
  `0x2000` (`0x1A5D4 != 0`, raw `0x1A5D4` reads the command's bit 0x2000),
  `S[0x43] |= 2`, `DSD(0xBDBEC)=0x30000` (d = 3) and `S[0x42] |= 8` (so the
  `0x1883C` latch and `0x18714` round-trip `rec+0x18` unchanged, letting the
  assertion read the post-delta value). `mem[0x1000] = 5` makes the `<< 6`
  visible; the mutation drops the shift.
* **`0x33B00`:** the record's "swap the two sources" mutation makes the slot's
  first dword the actor pointer `0xAAAAAAAA` and the next `memcpy` writes to
  `0xAAAAAAAA` (out of bounds). The shipped mutation changes only the actor
  copy's source (`mem + dst2` → `mem + src`), failing the actor's `+8` dword.
* **`0x37464`:** the record's seed gives `base = 0` (no image), so the x
  assertion is vacuous; the test seeds `DSW(0x1078DC) = 0x10`.
* **`0x29BC8`/`0x18788`:** the plan's helper list omits `0x18788` (the raw
  `0x18714` twin, `0x1883C`'s second callee) and `0x29BC8` (the record calls it
  "ported" but the port had only inlined it in the spawn). Both are added as
  `hit_record_y`/`fighter_29bc8` (one C function per original).

### 8.2 Task 3 — the deep callees (`port/tests/test_fight.c`)

* **`0x385B0`:** seed `DS_00100AF8[side]=1`, `F[0x28]=0xFF`,
  `S[0x40]=0xFFFFFFFF`, `F[0x42]=0xFF`, `S[0x54]=2`; call `0x385b0(F)`; assert
  `DSD(0x100AF8+side*4)==0`, `F[0x28]==0xDF`, `S[0x40]==0xCCF3BFFF`,
  `F[0x42]==0`, `S[0x54]==0`, `S[0x52]==0`, `S[0x53]==0`. Mutation: drop the
  `F[0x28] &= 0xDF` → `0xFF != 0xDF`.
* **`0x39A10`:** seed `DSW(S+0x74)=0xFFFF`, `F[0x51]=side`; call
  `0x39a10(F, 0x1234)`; assert `DSW(S+0x74)==0x1234`. Mutation: write the wrong
  slot → the assertion fails.

**Task-3 implementation corrections (the raw wins).** The recipes above are
incomplete against the raw control flow, and the two callers are unported, so
the shipped assertions in `port/tests/test_fight.c` (`check_deep_callees`) seed
more and wire the callees' real call chains:

* **`0x385B0` (test values):** the recipe's `F[0x28]==0xDF` and
  `S[0x40]==0xCCF3BFFF` miss the tail: `0x385B0`'s `S+0x41 &= 0x7F` (raw
  `0x386EE`) clears `S+0x40` bit 0x8000 (`0xCCF33FFF`), and the
  `actors_anim_begin` tail (`0x2BC30`) does `rec+0x28 &= 0xEB` (raw `0x2BC52`),
  so `F[0x28]` is `0xCB`, not `0xDF`. The test asserts the actual post-states
  and seeds `S+0x52`/`S+0x53` (the recipe left them BSS-zero). Mutation: drop the
  `F[0x28] &= 0xDF` → `0xEB != 0xCB` at `test_fight.c:2437`.
* **`0x385B0`'s and `0x39A10`'s callers are unported** (record §2.3). To satisfy
  the brief's "wire each at its real call site" and the repo's no-unreachable-
  code rule, Task 3 ports the minimal chain too: `0x37D18` (the bit-7 arm's
  callee), and `0x37178` + `0x36870` + `0x379C4` + `0x164E8` (the bit-6 arm's
  chain). Evidence: `0x349C8` `0x349EA CALL 0x37178` and `0x34A29 CALL 0x37D18`;
  `0x37178` `0x37256`/`0x3732F CALL 0x36870`; `0x36870` `0x36884 CALL 0x385B0`
  under `DSW(0x104B00)==0x25`; `0x37D18` `0x37D57 CALL 0x39A10`. The chain is
  off the demo path (`+0x42` bits 6/7 are never set there), so the ladder is
  unmoved. The `0x385B0` `S+0x54 == 5` arm (`0x38154`) stays the existing named
  gap (§7.4).

### 8.3 Task 4 — the loader flush scope (`port/tests/test_frontend.c`)

* **Corrected by Task 4 (§3.5; the raw wins).** The recipe's "the arena records
  survived to the `0x25672` gate flush" is refuted: the raw's loader draws drain
  the accumulated arena records too. The shipped assertion is the raw's
  **initial record**: seed the dirty list's base record with a sentinel that
  differs from the post-condition (`0xDEADBEEF`), call `palette_list_init`
  (`0x336C0`), and assert the base record is `{ DS_000BD470; 0; 1; 0 }` and the
  head is `base + 0x10`; then call `gfx_flush_palette` (the loader draw's
  `0x1C470`) and assert the record is consumed (`first = -1`) and the head is
  the base. Mutation: drop the `0x33734` enqueue → `test_frontend.c:485`
  (`1078424 != 1078440`), `:486` (`-559038737 != 775280`).

### 8.4 Task 5 — the attract drivers (`port/tests/test_attract.c`)

* **`0x4F7F4`:** seed `DS_000F0A48=1`, `DS_001088F1=3`, `DS_00104AD0=1`; call
  the port's `0x4F7F4`; assert `DS_001088F1==4`, the enqueue used
  `DSD(0xC98A0+3*4)`, `DS_00104AD0 & 1 == 1`. Seed `DS_001088F1=9` → assert
  `DS_00104AD0 & 1 == 0`. Mutation: drop the `>= 10` clear → bit 0 stays set.
* **`0x4F83C`:** seed `DS_00104AD0=0`, `DS_001088F1=7`, `DS_000F0A48=1`; call
  the port's `0x4F83C`; assert `DS_00104AD0 & 1 == 1`, `DS_001088F1==1`, the
  enqueue used `DSD(0xC98A0)`. Mutation: drop the `DS_001088F1=1` store →
  `7 != 1`.

**Task-5 implementation corrections (the raw wins).**

* **The `0x33874` walk is NOT byte-identical to `palette_acquire`'s tail**
  (§4.4's residual). Two differences, both in `0x33874`'s disassembly:
  (a) the walk has **no zero-entry skip** — `0x337E9`'s `CMP [EAX],0 / JZ
  0x33825` has no counterpart at `0x338C4`; (b) the break is **`<=` (signed),
  `0x338D1 JLE 0x338F7`**, whereas `palette_acquire` breaks on **`==`**
  (`0x337FB JZ 0x3382F`). So `0x33874` reflows while `prev.start + prev.len >
  next.start` and does not skip free entries; the port's `palette_reflow`
  implements `0x33874`'s own shape, not `palette_acquire`'s.
* **`0x1B544` preserves EDX** (§4.4 confirmed): `PUSH EDX` at `0x1B546` and
  `POP EDX` at every return (`0x1B578`, `0x1B5A2`, `0x1B608`), so the `AND
  EDX,0x7FFFFF` at `0x1B563` is internal and `0x33897` compares the **handle**,
  not the offset. The `[EAX]` at `0x3388B` is the resolved resource's leading
  word (the colour count), the same `*res_resolve(handle)` the port's
  `palette_acquire` reads.
* **The test recipe's `DS_000F0A48=1` is unsafe** (the carried no-image /
  out-of-bounds warning). `0x33874` would treat mem offset 1 as a table entry
  and either walk `0x11..0x107798` or write the low-memory vector area. The
  shipped test uses a scratch descriptor at `0x3F00100` and overrides the
  `0xC98A0` entries with non-resolving sentinels, so `0x33874`'s else branch
  runs deterministically (NULL resolve → count 0) with no loader-presentation
  side effect. The walk branch is driven directly with a fake descriptor/next
  pair at the ownership table's tail (`0x107778`/`0x107788`).
* **Wiring:** the record's "direct `0x4F7F4` call" is realized as a one-time
  `fn_register(0x4F7F4, attract_palette_advance)` in `attract_scene_tick`
  (`attract.c`); a direct entry-0 call would bypass the dispatch test's
  registered probe (`test_attract.c`'s `scene_probe0`), which the table
  override must still reach. `0x33874` is ported as `palette_reflow` beside its
  siblings `palette_acquire`/`palette_release` in `actors.c`, not in
  `attract.c` (the palette ownership table's owner).
* **`0x4F83C` reachability:** its four `0xE8916`-table callers are unported, so
  `0x4F83C` is not reached at runtime; the port exposes it and the test drives
  it. `DS_00104AD0` bit 0 is never set in the shipped run (its image value is 0
  and only `0x4F83C` sets it), so `0x4F7F4` is likewise not reached at runtime
  and the attract claim is unmoved.

---

## 9. Provenance

* **Raw bytes / decompilation:** `data/game/C/PRAGE.EXE` (read-only), Ghidra
  MCP project `rage` program `/PRAGE.EXE` (fixups applied). Functions read:
  `0x34B6C` (dispatch, disassembly), `0x359E0`/`0x35C1C`/`0x35D20`/`0x37464`/
  `0x33B00`/`0x35E6C` (disassembly), `0x35B7C`/`0x1883C`/`0x36E78`/`0x385B0`/
  `0x39A10`/`0x4F7F4`/`0x4F83C`/`0x33874`/`0x292AC`/`0x1C470`/`0x1C65C`/
  `0x1B544`/`0x1D290` (decompile/disassembly), `0x35F84`/`0x3C148`/`0x3C16C`/
  `0x36638`/`0x365C8`/`0x36E2C` (the record model). Data tables:
  `0xA8744` (the driver table), `0xE8900..0xE8980` (the `0x4F83C` pointers at
  `0xE8916`/`0xE893C`/`0xE8958`/`0xE896E`), `0x0BDA3E`/`0x0BDC00`/`0x0BDBEC`/
  `0x1078DC`/`0xC8A68`/`0xC8AE0`/`0xC8A90`/`0xC8B08`/`0xC8B58`/`0xC98A0`
  (read via `ghidra_read_memory` where needed).
* **Function/call graph:** `port/decomp/prage.functions.csv`,
  `prage.calls.csv`; `ghidra_get_function_callers` for `0x385B0`, `0x39A10`,
  `0x4F7F4`, `0x33874`, `0x292AC`; `ghidra_get_xrefs_to` for `0x4F83C`,
  `0x33874`, `0x4F7F4`; `ghidra_search_byte_patterns` for `3c f8 04 00`.
* **Port:** `port/src/game/fight.c` (the dispatch, `485-528`),
  `port/src/game/fighter.c`/`.h`, `port/src/game/actors.c`
  (`palette_acquire`/`palette_record`/`text_blit_string`),
  `port/src/platform/gfx.c` (`gfx_flush_palette`/`palette_record`),
  `port/src/platform/res.c` (`res_load_present`/`res_resolve`),
  `port/src/game/flow.c` (`game_state_6`/`game_state_step`/`game_loop`),
  `port/src/game/attract.c` (`attract_scene_tick`), `port/src/game/camera.c`
  (`0x1A570`), `port/src/symbols.h` (the generated names).
* **Measured:** the front-end dump `PR_FRONTEND_DUMP=/tmp/ffhold*`
  (`select.log` + `frame_*.raw`), the capture
  `data/title-captures/frontend/frame_0830.raw`, and the temporary
  state/tick trace in `test_frontend.c` (reverted). The cycle-3 record
  (`2026-09-22-combat-fidelity-derivations.md` §2.2/§2.3/§4/§10.4) and the
  cycle-2 record (`2026-09-21-demo-fight-closure-derivations.md` §9.5/§9.6)
  supply the cross-references.
* **The size-gate computation** used `prage.calls.csv`/`prage.functions.csv`
  with the §6.1 method; the per-group roots are in §6.2.

---

## 10. Outcome (Task 6 — recorded)

**The cycle closed all four in-scope gaps, moved no enforced oracle claim, and
left every out-of-scope gap with its owner.** The size gate did not trigger (the
union's in-scope closure is 15 functions / 2 934 B, §6.3).

**The four gaps.**

* **The five `+0x52` handlers (Task 2)** — ported and dispatched. 29
  `CHECK_EQ_INT` (`test_fight.c:check_gap_handlers`), six mutation proofs.
* **`0x349C8`'s bit-6/7 deep callees (Task 3)** — `0x385B0`/`0x39A10` wired at
  the raw's sites (`0x36884`, `0x37D57`); the minimal caller chain (5 functions /
  ~1 741 B) ported and **ratified by the human** so the callees stay reachable.
  15 `CHECK_EQ_INT` (`test_fight.c:check_deep_callees`), five mutation proofs.
* **The loader flush scope (Task 4)** — the record's "scoped flush" premise was
  **refuted by the raw** (`0x1C470` is a whole-list drain; the port's unscoped
  flush was already faithful) and the real gap — the missing initial record
  `{0xBD470, 0, 1, 0}` — was shipped and asserted (`test_frontend.c:485-489`).
* **The attract/scene palette drivers (Task 5)** — `0x4F7F4`/`0x4F83C`/`0x33874`
  ported; `0x4F83C` is an **accepted exception** (test-only, its callers
  unported; brief-mandated by the spec's representation rule, Q10). 37
  assertions, four mutation proofs. The attract claim is unmoved (215).

**The 169-tick hold's verdict (Task 1 §5).** A **real divergence**, not the
load/stall model: the port holds capture-830's frame byte-static for loop
902..1069 while the capture's state-9 screen animates to the loader at 831. Its
owner is the state-9 screen's actor-animation advance; it is oracle-neutral and
carried forward (named gap 6).

**The size gate (§6).** No group becomes a follow-on: in-scope A 10 f / 2 223 B,
B 2 f / 418 B, C 0, D 3 f / 293 B; union 15 f / 2 934 B — below both thresholds.
The mechanical true-new (27 f / 4 490 B) crosses 4 KB only because of the shared
resource-loader/runtime path (`0x1B544` et al.), which the port already replaces.

**Claim-move policy.** No enforced claim moved — title
`54 clean, 55 splice, 2 transition, 0 unexplained` and `54 clean, 57 splice,
0 unexplained`; attract `FIRST DIVERGENCE at capture frame 215`; front-end
`[560..830]` / 271 frames `0 unexplained`; smk `120/120` + `41/41`; C-vs-Python
`9866 writes byte-exact`; `symbols.h` regenerates byte-identically; 0 warnings.

**Corrections recorded (the raw wins).** §0.4 items 7–9 (the fix-round-1
corrections); §1.6/§8.1 (the Task-2 handler recipes); §1.3/§8.2 (Task 3's
`0x1078DC` deref and post-states); §3.5/§8.3 (Task 4's refuted scope); §4.4/§8.4
(Task 5's walk shape). The stale documentation (the `0x13xxx` render path, the
camera-chain deferral, `0x13B3C`'s deadness, `game_flow.md:240-247`) is corrected
in Task 6's commit.

**Out-of-scope gaps carried, with owners (§7):** the freeze (`0x19020` chain) and
the demo oracle's `res is None` → cycle 4; the `0x13xxx` call sites → the
interactive match; 831/832 (un-derivable); the interactive match (unowned); the
audio gaps; the state-9 hold and `0x38154` (existing named gaps); Task 4's
flush-scope-vs-gate concern (cycle-2's read/gate model).
