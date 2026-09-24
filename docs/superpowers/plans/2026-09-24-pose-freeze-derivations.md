# The pose/freeze cycle — raw-byte derivation (Task 1)

**Scope.** The freeze's remaining layer, derived from the raw so Tasks 2–4 can
implement without re-deriving. Cycle-3's record
(`docs/superpowers/plans/2026-09-22-combat-fidelity-derivations.md`) §10 named
the chain and the closure; this record pins every value, the three resolutions
(the winner gate, the `0x19020` no-op, the measurement plan) and the per-group
size-gate verdicts. **No porting code.**

**The two halves.** The freeze needs both: the **unfreeze half** (`0x140E4` +
`0x170A0` + callees) writes `DS_00100AF8`/`AFC` in `camera_decay` (`0x17580`),
and the **pose-entry half** (`0x193B0` + `0x3B714` + `0x3AAFC` + the pose
family) is what `fighter_pass_a`'s tail (`0x1958C`, `0x1974D`) runs when those
flags survive. Neither alone moves the demo.

---

## 0. Addressing, reproduction and corrections

### 0.1 The image and the formulas

* Ghidra address == linear address == object base + offset. Code object
  `0x10000`–`0x73B14`; data object `0x80000`–`0x10B0CF`. A global Ghidra calls
  `DAT_0008xxxx` is at **DS offset `0xxxx`** (subtract `0x80000`).
* Original state lives in `mem[]` at its linear address, accessed as
  `DSB/DSW/DSD(addr)`.
* **The slot model.** `slot(side) = DS_001077B0 + side*0x94`; the fighter
  *record* pointer is `rec = DSD(slot)`. `fighter_ctx_same` (`0x33950`) fills
  `ctx[0]=side`, `ctx[1]=1-side`, `ctx[2]=slot(side)`, `ctx[3]=slot(1-side)`,
  `ctx[4]=rec(side)`, `ctx[5]=rec(1-side)`. The demo's characters
  (`DS_0010816A[side]`, copied to `slot+0x7A` by `0x33CB8`) are **0 (T-rex,
  side 0)** and **3 (raptor, side 1)**.
* **The projection idiom** the two halves share (`0x140E4`, `0x15C30`,
  `0x17FA0`): `((s32)(x >> 6) * K + 0x800) / 0x1000` with truncation toward
  zero, K = `0xF3D` (x) or `0xD56` (y) — the raw's `IMUL; ADD 0x800; SAR
  0x1F; SHL 0xC; SBB; SAR 0xC` sequence (`0x14140`..`0x14156`).

### 0.2 Reproduction

* Every function body is from `ghidra_decompile_function` /
  `ghidra_disassemble_function` at the cited address (project `rage`,
  program `/PRAGE.EXE`); the per-section calls are in §9.
* The closure figures (§5) are recomputed from `port/decomp/prage.calls.csv` +
  `prage.functions.csv` by the cycle-3 §10.4 method, verbatim (§5.1).
* The runtime state bytes (§3, §4) are the port's per-frame demo values,
  sampled by a temporary trace added to `port/tests/test_frontend.c` and
  reverted before this commit (`git status` clean; the trace ran with
  `PR_FRONTEND_DUMP=/tmp/t1dump ./build/run_tests`, log kept at
  `/tmp/t1trace.txt`). The port's demo state is the original's: cycle-3 §3.3
  measured "both trees hold … `+0x5F = 0x01`" at loop 1071.

### 0.3 Corrections against the brief/plan (raw wins)

1. **`0x18950`'s two table-index bytes are `slot+0x7A` and `slot+0x5F`, not
   `slot+0x2A`/`slot+0x0F`.** The raw reads `[0x10782A + side*0x94]` (the
   character byte; `0x10782A - 0x1077B0 = 0x7A`) and `[0x10780F + side*0x94]`
   (the reaction/animation byte; `0x10780F - 0x1077B0 = 0x5F`) at `0x18975`,
   `0x1897C`, `0x18982`, `0x1899B` (disassembly). The brief's offsets are
   `0x50` low (a different slot base). §3 uses the raw addresses.
2. **`0x170A0` reads `B00`/`B08`/`B0C` and does not write them or `AFC`.** They
   are `camera_decay`'s outputs (`0x1761F`/`0x17641`/`0x17653`/`0x1767B`).
   `0x170A0` consumes `B08[side]` (`0x1726E`/`0x17287`/`0x172F8`/`0x1730A`/
   `0x17330`/`0x1733B`) and `B00[side]` (`0x172A0`/`0x172D7`) to compute its own
   outputs. `AFC` is written by `0x17580`'s zero (`0x175F0`) and by `0x1958C`'s
   winner logic (`0x195C6`/`0x1969C`/`0x196D0`), not by `0x170A0`. The written
   set is in §1.3.
3. **The seeded `DS_00100B54` does not survive to `AF8`.** `camera_decay`
   zeroes `B54` at `0x175E4`, and `0x170A0` calls `0x16DA4` (`0x17565`), which
   **rewrites** `B54` (`0x16DC8`/`0x16E26`/`0x16F79`/`0x1706F`) before `AF8[side]
   = B54` (`0x1756A`/`0x1756F`). `B54`'s post-value comes from `0x16DA4`'s
   sprite path (Ghidra drops ~40 unreachable blocks; §1.4), so it is a **named
   gap** and the Task 2 test asserts the gating, not a seeded `B54` (§8.1).
4. **The winner gate is in scope.** The raw's two `0x18950` queries return
   `(0,1)` on the demo's frames and zero `AF8` (side 1 wins); the port's
   take-both-as-0 reaches the position compares and zeroes `AFC` (side 0 wins).
   They differ, so `0x18950` gates the demo's winner (§3). This **conflicts**
   with cycle-3 §10.2's "the T-rex leaves 9/8"; both are raw/measured, so the
   conflict is carried to §7 with both addresses.
5. **`0x193B0`'s `+0x18`/`+0x1C`/`+0x43`/`+0x14`/`+0xC`/`+0x90`/`+0x84`/`+0x8A`
   are on the slot, not the record.** `ctx[2] = slot(side)` (`0x33950`:
   `&DAT_001077b0 + side*0x25` dwords = `0x1077B0 + side*0x94`), and the raw's
   `[ESP+8]`/`[ESP+0xc]` are the two slots (`0x193CA`/`0x193D2`/`0x194B2`/
   `0x19508`/`0x1952F`/`0x19549`/`0x1956E`/`0x19576`). The brief's "record
   `+0x18`/`+0x1C`/`+0x43`/`+0x14`" reads **slot**. §2.2 uses the raw
   attribution.

---

## 1. The unfreeze half (`0x140E4` + `0x170A0`)

### 1.1 The owner chain

`FUN_00026254` (the state-7 master frame, 415 B) runs, in order:

* `0x262C1`/`0x262E4` `CALL 0x17FA0` — `camera_project(0)`/`(1)` (ported).
* `0x262E9` `CALL 0x17580` — **`camera_decay`** (ported, `camera.c:357`).
* `0x262EE` `CALL 0x1958C` — **`fighter_pass_a`** (ported, `fighter.c:291`).

So `camera_decay` writes `AF8`/`AFC` **before** `fighter_pass_a` reads them
(the cycle-3 §10.3 operational conclusion, re-confirmed by the disassembly).

`0x17580` (`camera_decay`, 330 B) zeroes `B54`/`AF8`/`AFC` (`0x175E4`/`0x175EA`/
`0x175F0`), decays the four `B00`/`B04`/`B08`/`B0C` values and the four word
countdowns, then — **the unported tail**:

```
0x17680  MOV EAX,[0x001077b0]        ; slot0
0x17685  MOV DX,word[EDX+0x56]       ; EDX = [0x107844] = slot1; DX = slot1+0x56
0x1768F  MOV AX,word[EAX+0x56]       ; AX = slot0+0x56
0x17698  CALL 0x000140e4             ; 0x140E4(actor0, actor1) -> AL
0x1769D  TEST AL,AL
0x1769F  JZ 0x000176c4               ; no overlap -> return (AF8=AFC=0)
0x176A1  CMP byte[0x00100b60],0x0
0x176A8  JZ 0x000176b1
0x176AA  XOR EAX,EAX
0x176AC  CALL 0x000170a0             ; 0x170A0(0) if B60 != 0
0x176B1  CMP byte[0x00100b61],0x0
0x176B8  JZ 0x000176c4
0x176BA  MOV EAX,0x1
0x176BF  CALL 0x000170a0             ; 0x170A0(1) if B61 != 0
0x176C4  ... return
```

`DS_00100B60`/`B61` are the per-side "unfrozen" gates; the `0x56` words are the
slots' actor indices (the `0x1014EC` actor table's rows).

### 1.2 `0x140E4` — the box-overlap bool (388 B, `0x140E4`..`0x14267`)

`0x140E4(EAX=actor0, EDX=actor1)` returns `AL = 1` iff the two actors' screen
boxes overlap; it writes **no global of its own** (`0x1B544`'s handle
expansion aside). For each of the two actor indices it builds a rect (the raw
at `0x140EE`..`0x1425A`):

* `EBX = DSD(0x1014EC) + actor*0x20` (the actor record);
  `id = word[EBX] & 0x7FFF` (`0x140FB`/`0x140FE`);
  `sprite = 0x1B544(DSD(0xA8B30 + id*4))` (`0x14101`/`0x14108`).
* `local_a = (s32)DSD(sprite+2) >> 16` (`0x14114`/`0x14123`); when
  `word[EBX] & 0x8000` (`0x14117`/`0x1412E`),
  `local_a = (s16)word[sprite] - local_a - 1` (`0x14132`..`0x14137`).
* `x1 = project_x(DSD(EBX+4)) - local_a` (`0x1413A`..`0x14159`);
  `y1 = project_y(DSD(EBX+8)) - ((s32)DSD(sprite+4) >> 16)`
  (`0x1415F`..`0x14180`).
* `x2 = x1 + (s16)word[sprite]` (`0x14184`..`0x1418D`);
  `y2 = y1 + ((s32)DSD(sprite) >> 16)` (`0x14191`..`0x141A7`).
* Then `0x14080(rect0, rect0, rect1)` (`0x1424E`/`0x14256`/`0x1425A`): the
  bytes at `0x1424E` are `8d 44 24 10` (LEA EAX,[ESP+0x10]) and
  `8d 54 24 10` (LEA EDX,[ESP+0x10]) with `EBX = ESP`, so the intersection is
  computed **in place** on rect0; `AL = 0` iff empty.

`0x14080` (97 B) is the intersection-and-test: `out[0..1] = max(a,b)` (left/top),
`out[2..3] = min(a,b)` (right/bottom), return 1 iff `right >= left && bottom >=
top`.

### 1.3 `0x170A0` — the `AF8` writer (1 248 B, `0x170A0`..`0x1757F`)

`0x170A0(EAX = side)`. **Guard** (`0x170AB`..`0x170E2`): with
`other = 1 - side`, return immediately unless
`DSW(0x107824 + other*0x94) == 0` **and** `DSW(0x107826 + other*0x94) <= 1`
(`0x170C5`/`0x170D3`/`0x170DF`). `0x107824`/`0x107826` are the two per-side
word countdowns `camera_decay` decrements (`0x17585`..`0x175D5`), so this gate
is "the other side is not in hit-stun/recovery".

**Body** (per side, after the guard):

* `[ESP+0x14] = 0x1B544(DSD(0xA8B30 + (0x17EEC(side) + DSD(0x100AF0+side*4))*4))`
  (`0x170F1`..`0x17111`) — the side's resolved sprite A.
* `[ESP+0x18] = 0x1B544(...)` for `other` (`0x17119`..`0x17138`) — sprite B.
* Visibility gates on `DSB(0x100AC8 + side*4)`'s `+2`/`+3` (`0x1713E`/`0x17147`/
  `0x1714E`/`0x17151`): both must be `>= 1`, else return. The other side is
  optional (`0x17164`..`0x17182` zero `EDI` when its `+2`/`+3` fail).
* `0x15C30(side, &box[4], DSD(0x100AF0+side*4), 0)` (`0x171CC`) clips the 4-byte
  box (`0x100AC8+side*4`) against the actor's screen bounds; `0x15C30` is the
  per-side projection + clip (654 B, calls `0x1B544`/`0x15B90`).
* The **facing** bit `word[rec+0x28] & 0x4000` (`0x1724E`..`0x1726C`) selects the
  `+0x2` byte offset added to `AE0[side]` (`0x1726E`..`0x1728F`).
* `AE0[side] = B08[side] + ECX (+ byte[0x100ACA+side*4])` (`0x1728F`).
* `AD8[side] = B00[side] + [ESP+0xc] + byte[0x100ACB+side*4]/2` (`0x172C0`).
* `AE8[side] = AD8[side] - B00[other]` (`0x172E5`).
* Three `0x181D0` screen-sync calls (`0x17362`, `0x1743C`, `0x174E5`) with the
  pointer arguments `0x100B14`/`0x100B1C`/`0x100B24`/`0x100B28`/`0x100B34` and
  `0x100B10`/`0x100B18`/`0x100B20`/`0x100B2C`/`0x100B30`; each returns a bool
  and a non-zero return aborts (`0x17367`/`0x17441`/`0x174EA`).
* `B10 += [ESP+0xc]` (`0x174F9`); `B30 += [ESP+0x24]` when sprite B is present
  (`0x17507`).
* `B38 = |B14 - B34|` (`0x1750D`..`0x17539`); `B40 = B1C` (`0x17553`).
* `0x16DA4(...)` (`0x17565`) — **the `B54` writer** (see §1.4).
* `AF8[side] = DSD(0x100B54)` (`0x1756A`/`0x1756F`).

**Written fields (complete, from the disassembly).** `AE0[side]` (`0x1728F`),
`AD8[side]` (`0x172C0`), `AE8[side]` (`0x172E5`); `B10` (`0x174F9`), `B30`
(`0x17507`), `B38` (`0x17539`), `B40` (`0x17553`); `B14`/`B1C`/`B20`/`B24`/
`B28`/`B2C`/`B30`/`B34` through the three `0x181D0` pointer arguments;
`AF8[side]` (`0x1756F`); `B54` (through `0x16DA4`). **Read**: `B00[side]`,
`B00[other]`, `B08[side]`, `B08[other]`, `B1C`, `B10`, `B14`, `B30`, `B34`,
`B54`, `AF0[side]`, `AF0[other]`.

### 1.4 The callees (size / ported status)

| callee | size | ported? | role |
|---|---|---|---|
| `0x14080` | 97 B | **no** | rect intersection + non-empty test |
| `0x1B544` | 200 B | yes (`res_resolve`, `platform/res.c`) | handle → pointer |
| `0x17EEC` | 107 B | yes (`camera_char_const`, `camera.c:32`) | per-character ground constant |
| `0x15C30` | 654 B | **no** | per-side projection + box clip (calls `0x1B544`, `0x15B90`) |
| `0x15F48` | 97 B | **no** | bit-plane blit helper (calls `0x61A70`) |
| `0x15FD4` | 423 B | **no** | bit-plane shift/merge blit |
| `0x181D0` | 219 B | **no** | box clip/visibility predicate (7 args, returns 0/1/2/3) |
| `0x16DA4` | 729 B | **no** | the `B54` writer (calls `0x1631C` → `0x17EEC`/`0x1B544`/`0x41030`) |
| `0x61A70` | 24 B | yes (`mem_fill` idiom, `actors.c:133-135`) | `mem_fill` |

`0x16DA4`'s decompile is dominated by "Removing unreachable block" warnings
(~40), and `B54` is written only inside it (`0x16DC8`/`0x16E26`/`0x16F79`/
`0x1706F`; `ghidra_search_instructions 100b54`) — so `B54`'s post-value is a
**named gap** (§7.3).

### 1.5 The port's current stub

`port/src/game/camera.c:373-375`:

```c
/* PORT: 0x17698 0x140E4 (bool) and 0x176AC/0x176BF 0x170A0 are unported
 * (the fighter screen-sync path); the DS_00100B60/61 tail is skipped and
 * those bytes keep the values camera_project wrote. */
```

The port zeroes `B54`/`AF8`/`AFC` (`camera.c:365-367`) and never runs the
`0x17698`..`0x176BF` tail, so `AF8`/`AFC` stay 0 at every demo frame — measured:
the trace shows `af8=0 afc=0` for all 900 state-7 frames.

### 1.6 Porting plan (Task 2)

One C function per original, address-tagged, in `camera.c`:

* `camera_box_overlap` (`0x140E4`) and a static `camera_rect_clip` (`0x14080`).
* `camera_unfreeze` (`0x170A0`) plus its new callees, one C function each:
  `camera_box_clip` (`0x15C30`), `camera_sync_pixel` (`0x15F48`) and
  `camera_sync_row` (`0x15FD4`) — the two write the `0x100BD3` 0x25-byte
  buffer `0x170A0` builds (`0x173A6` `mem_fill`, `0x173C4`/`0x173E6`) —,
  `camera_sync_visible` (`0x181D0`), `camera_winner_height` (`0x16DA4`) and
  `camera_sprite_height` (`0x1631C`, which also needs `0x41030`, 361 B,
  unported). `0x140E4`'s projection reuses the module's existing
  `camera_project` idiom; `0x1B544` is `res_resolve`; `0x61A70` is `mem_fill`.
  If `0x15F48`/`0x15FD4` turn out to duplicate an existing render helper, wire
  them to it rather than re-porting (the repo's reuse rule) — but they are not
  in `port/src` today (the `port=False` column, §1.4), so they are new.
* Wire the `0x17698`..`0x176BF` tail into `camera_decay` at `camera.c:373`:
  call `camera_box_overlap(DSW(slot0+0x56), DSW(slot1+0x56))`, and on `AL != 0`
  call `camera_unfreeze(0)` when `B60 != 0` and `camera_unfreeze(1)` when
  `B61 != 0`.

The `B60`/`B61` gates are `DSB(0x100B60)`/`DSB(0x100B61)`; the countdown gate is
`DSW(0x107824 + other*0x94)`/`DSW(0x107826 + other*0x94)`.

### 1.7 Unit-test values

See §8.1. In brief: seed the actor table + slots so `0x140E4` returns 0 or 1;
seed `B60`/`B61`; assert `AF8`/`AFC` per the raw's gates, and
`AF8[side] == DSD(0x100B54)` when the gate passes (the `B54` numeric is the
named gap §7.3).

---

## 2. The pose-entry half (`0x193B0` + `0x3B714` + `0x3AAFC` + the pose family)

### 2.1 The chain

`fighter_pass_a` (`0x1958C`) computes `bl`/`al` (`0x19653`/`0x19661`) and, after
the `+0x40` overrides and the position compares, runs the winner tail at
`0x19720`:

```
0x19720  CMP [0x00100af8],0; JZ 0x19736
0x19729  CMP byte[0x0010783a],0; JZ 0x19736
0x19732  XOR EAX,EAX                 ; side 0
0x19734  JMP 0x1974d
0x19736  CMP [0x00100afc],0; JZ 0x19752
0x1973f  CMP byte[0x001078ce],0; JZ 0x19752
0x19748  MOV EAX,0x1                 ; side 1
0x1974d  CALL 0x000193b0             ; 0x193B0(side)
0x19752  ... return
```

So `0x193B0(side)` runs for **one** side per frame: side 0 when `AF8 != 0 &&
byte[0x10783A] != 0`, else side 1 when `AFC != 0 && byte[0x1078CE] != 0`.

The callers are exhaustive (cycle-3 §10.3): the pose family
(`0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8`) ← `0x3AAFC` only; `0x3AAFC` ←
`0x3B714` only; `0x3B714` ← `0x193B0` only; `0x193B0` ← `0x1958C` only.

### 2.2 `0x193B0` — the winner's per-frame body (475 B, `0x193B0`..`0x1958B`)

`0x193B0(side)` (Ghidra shows no params; `EAX` = side). `0x33950` (`0x193B9`)
fills `ctx` at `ESP`: `ctx[0]=side`, `ctx[1]=1-side`, `ctx[2]=slot(side)`,
`ctx[3]=slot(1-side)`, `ctx[4]=rec(side)`, `ctx[5]=rec(1-side)`. The body
(disassembly `0x193B0`..`0x1958A`):

* `0x34D8C(side)` (`0x193C1`, 78 B, ported) — the facing latch
  (`slot+0x59 = 1`/`0xFF`, `0x34D8C`).
* `slot(side)+0x43 &= 0xFC` (`0x193CA`); `slot(1-side)+0x43 &= 0xFC` (`0x193D2`).
* `0x1922C(1-side)` (`0x193DA`, 147 B, ported).
* `cVar1 = 0x3962C(side)` (`0x193E4`, 126 B, ported); if non-zero →
  `slot(side)+0x8A = 0` (`0x193F1`), `0x18B44(slot(side), 0x29A)` (`0x19401`,
  129 B, ported), `0x39A10(rec(1-side))` (`0x1940A`, 34 B, ported), return.
* else `cVar1 = 0x396AC(side)` (`0x19419`, 139 B, ported); if non-zero → the
  same three, return.
* else if `word[0x100B50 + side*2] != word[slot(side)+0x84]` (`0x1945A`):
  `word[0x100B50 + side*2] = word[slot(side)+0x84]` (`0x19472`),
  `0x19164(side)` (`0x1947C`, 198 B, **unported**), `slot(side)+0x8A = 0`
  (`0x19485`);
  * if `slot(1-side)+0x54 != 2 && slot(1-side)+0x52 != 0x11` (`0x19490`/
    `0x19496`): `0x3C148(1-side)` (`0x194A0`, 36 B, ported),
    `0x3C16C(1-side)` (`0x194A9`, 34 B, ported);
  * if `slot(side)+0x1C == 0` (`0x194B2`): **`0x3B714(EAX=slot(1-side),
    EDX=slot(side))`** (`0x19526`);
    else (the `0x2BD44` arm): `0x2BD44(rec(1-side))` when
    `byte[rec(1-side)+0x4B] != 0` and its `0x1014F4` row's `+0x60 != 0`
    (`0x194EE`); `slot(1-side)+0x90 = 5` (`0x194F7`); call
    `slot(side)+0x1C` (`0x19505`); `slot(side)+0x18 = 0`,
    `slot(side)+0x1C = 0` (`0x1950C`/`0x19517`);
  * `if (slot(1-side)+0x14 != 0) { r = (*slot(1-side)+0x14)(); if (r)
    slot(1-side)+0x14 = 0; }` (`0x1952F`..`0x19542`);
    `slot(1-side)+0x18 = 0`, `slot(1-side)+0x1C = 0`, `slot(1-side)+0xC = 0`
    (`0x1954D`/`0x19558`/`0x19563`); `slot(1-side)+0x43 &= 0xF7` (`0x1956E`);
    `slot(side)+0x8A = 0` (`0x19576`); `0x192DC(side)` (`0x19580`, 212 B,
    **unported**).

**Written fields** (all on the **slot**, not the record): `word[0x100B50 +
side*2]`, `slot+0x8A` (= 0), `slot+0x43` (masks), `slot+0x18`, `slot+0x1C`,
`slot+0xC`, `slot+0x14`, `slot+0x90` (= 5), `slot+0x59` (via `0x34D8C`).

### 2.3 `0x3B714` — the reaction applier (449 B, `0x3B714`..`0x3B8DC`)

`0x3B714(param_1 = ?, param_2 = ?)`: `local_20 = 1 - byte[(*param_2)+0x51]`;
`0x33950` (`0x3B73A`); `cVar2 = 0x3C59C()` (`0x3B748`, 48 B, ported) — when 0:

* `local_24 = byte[param_2 + 0x5F]`; if `== 0xFF` → `0x62003()` (a stub, out of
  scope); else `0x3AFC4()` (116 B, ported).
* If `(word[local_28+2] >> 8 & 8) == 0`: `cVar2 = 0x39EFC()` (66 B, **unported**);
  if `cVar2 == 0 || (word[local_28+2] >> 8 & 0x40) != 0`:
  * if `param_1+0x52 == 4`: `rec+0x34 = 0`, `param_1+0x4E = 0`, `local_1c = 4`;
    else `local_1c = 9`.
  * `param_1+0x65 = byte[param_2+0x5F]`; `local_18 = 0x3B298()` (441 B, ported);
    `if (param_1+0x54 != 2) 0x3B080()` (177 B, ported);
    `if (local_40+0x52 == 4) 0x3AE9C()` (294 B, **unported**).
  * if `local_18 == 0`: (the `0x2BD44` arm) then **`0x3AAFC()`** (`0x3B877` →
    `0x3AAFC`, with `local_4c = local_24`); else `local_40+0x8A = 0`,
    `0x3AD98()` (260 B, ported).
  * `0x3B6C4()` (77 B, **unported**); `param_1+0x41 |= 0x80`.

**Written fields.** `rec+0x34`, `param_1+0x4E`, `param_1+0x65`, `param_1+0x41`,
`local_40+0x8A`.

### 2.4 `0x3AAFC` — the reaction applier / pose dispatcher (667 B)

`0x3AAFC()`: `0x33A10()` (88 B, ported), `0x18B04()` (134 B, ported),
`0x3AFC4()` (ported); `uVar2 = (byte[local_38+0x5F] == -1) ? 0 :
word[local_18+2]`; `0x39834()` (407 B, ported); if `0x3A280()` (32 B,
**unported**) → `0x2C3FC()` (the voice stub); `0x3A0FC()` (355 B, **unported**);
`slot+0x6C++`; `slot+0x42 |= 2`; `local_38+0x42 |= 1`.

Then the dispatch on `uVar2`'s bits and `slot+0x54`:

* `(uVar2 & 0x40)`: `0x18B04()`, `0x39F40(0x14)`, `slot+0x90 = 5`.
* `slot+0x53 == 6 && slot+0x7A == 1`: `0x39F40(0x14)`, `slot+0x90 = 5`.
* `slot+0x54 == 2`: `0x39F40(0x14)`, `slot+0x90 = 5`.
* `extraout_ECX & 0x200`: same.
* `extraout_ECX & 0x100`: `slot+0x52 = 0x3AA54()` (167 B, **unported**),
  `slot+0x90 = 6`.
* `extraout_ECX & 0x2000`: `slot+0x52 = 0x36D20()` (120 B, ported).
* else, on `slot+0x54`: `0` → (`ECX & 4`) **`0x3A650()`**; (`ECX & 8`) == 0 →
  **`0x3A504()`**; else **`0x3A79C()`**; `< 2` → **`0x3A8E8()`**; `!= 3` → the
  `0` arm.

**Written fields.** `slot+0x6C`, `slot+0x42` (bits 0/1), `slot+0x52`,
`slot+0x90`, and (through the pose family) the pose state.

### 2.5 The pose family (4 × 116 B)

`0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8` are identical except for the callback
and the two `0x107Dxx` table bases. Each takes `EAX = side` and `EDX` (a word,
`BX` = its high half, `CL` = its low byte), runs `0x33A10` (`0x3A50E`, 88 B,
ported; `ctx[0]=1-side`, `ctx[1]=side`, `ctx[2]=slot(1-side)`, `ctx[3]=slot(side)`,
`ctx[4]=rec(1-side)`, `ctx[5]=rec(side)`), then (disassembly `0x3A504`):

* `rec(side)+0x24 = 0` (`0x3A517`, `ctx[5]`);
* **`slot(side)+0x52 = 0x10`** (`0x3A522`, `ctx[3]`);
  **`slot(side)+0x53 = 0x0A`** (`0x3A52A`); **`slot(side)+0x54 = 0`**
  (`0x3A532`); `slot(side)+0x10 = callback` (`0x3A53A`);
  `slot(side)+0x58 = 0` (`0x3A545`);
  `slot(side)+0x7E = byte[0xBECF8] + (EDX & 0xFF)` (`0x3A549`/`0x3A554`);
* `word[0x107Dxx + side*2] = word[slot(side)+0x2C]` (`0x3A55F`/`0x3A563`);
  `word[0x107Dyy + side*2] = EDX >> 16` (`0x3A56B`).

| function | callback (`slot+0x10`) | table pair |
|---|---|---|
| `0x3A504` | `LAB_0003A43C` | `0x107D14` / `0x107D10` |
| `0x3A650` | `LAB_0003A588` | `0x107D0C` / `0x107D00` |
| `0x3A79C` | `LAB_0003A6D4` | `0x107D08` / `0x107D04` |
| `0x3A8E8` | `LAB_0003A820` | `0x107CF8` / `0x107CFC` |

So the pose state the record §10.2 observed (`+0x52 = 0x10`, `+0x53 = 0x0A`,
`+0x54 = 0`) is set here, identically by all four members — the `+0x54 = 0`
variant. `LAB_0003A6D4` is absent from `prage.functions.csv` (a data pointer
only, `prage.c:24880`), so `0x3A79C`'s callback's size is unnamed (§7.4).

### 2.6 The callees (size / ported status)

| callee | size | ported? | needed by |
|---|---|---|---|
| `0x33950` | 90 B | yes (`fighter_ctx_same`) | `0x193B0`, `0x3B714` |
| `0x34D8C` | 78 B | yes | `0x193B0` |
| `0x1922C` | 147 B | yes | `0x193B0` |
| `0x3962C` | 126 B | yes | `0x193B0` |
| `0x396AC` | 139 B | yes | `0x193B0` |
| `0x18B44` | 129 B | yes | `0x193B0` |
| `0x39A10` | 34 B | yes (`fighter_39a10`) | `0x193B0` |
| `0x19164` | 198 B | **no** | `0x193B0` |
| `0x3C148` | 36 B | yes | `0x193B0` |
| `0x3C16C` | 34 B | yes | `0x193B0` |
| `0x192DC` | 212 B | **no** | `0x193B0` |
| `0x2BD44` | 89 B | yes | `0x193B0`, `0x3B714` |
| `0x3B714` | 449 B | **no** | `0x193B0` |
| `0x3C59C` | 48 B | yes | `0x3B714` |
| `0x39EFC` | 66 B | **no** | `0x3B714` |
| `0x3AE9C` | 294 B | **no** | `0x3B714` |
| `0x3B6C4` | 77 B | **no** | `0x3B714` |
| `0x3B298` | 441 B | yes | `0x3B714` |
| `0x3B080` | 177 B | yes | `0x3B714` |
| `0x3AD98` | 260 B | yes | `0x3B714` |
| `0x3AFC4` | 116 B | yes | `0x3B714`, `0x3AAFC` |
| `0x33A10` | 88 B | yes | `0x3AAFC`, pose family |
| `0x18B04` | 134 B | yes | `0x3AAFC` |
| `0x39834` | 407 B | yes | `0x3AAFC` |
| `0x3A280` | 32 B | **no** | `0x3AAFC` |
| `0x3A0FC` | 355 B | **no** | `0x3AAFC` |
| `0x39F40` | 112 B | yes | `0x3AAFC` |
| `0x36D20` | 120 B | yes | `0x3AAFC` |
| `0x3AA54` | 167 B | **no** | `0x3AAFC` |
| `0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8` | 116 B each | **no** | `0x3AAFC` |
| `0x2C3FC` | 1 268 B | stub (out of scope) | `0x3AAFC` |

### 2.7 The port's current stub

`port/src/game/fighter.c:363-369`:

```c
tail:
    /* 0x19720: whichever flag survives runs 0x193B0 (a named gap §7.6). */
    if (DSD(DS_00100AF8) != 0 && DSB(DS_0010783A) != 0) {
        /* PORT: 0x1974D 0x193B0(0) — named gap (§7.6). */
    } else if (DSD(DS_00100AFC) != 0 && DSB(DS_001078CE) != 0) {
        /* PORT: 0x1974D 0x193B0(1) — named gap (§7.6). */
    }
```

The tail's gate is transcribed; only the `0x193B0` calls are missing. The
pose state (`+0x52 = 0x10`, `+0x53 = 0x0A`) is measured by
`test_frontend.c`'s `s7_saw10`/`s7_saw0a` (printed, not asserted).

### 2.8 Porting plan (Task 3)

One C function per original, address-tagged, in `fighter.c` (tightly coupled to
its statics): `fighter_winner_body` (`0x193B0`), `fighter_reaction` (`0x3B714`),
`fighter_reaction_apply` (`0x3AAFC`), the four pose setters
(`fighter_pose_3a504`/`_3a650`/`_3a79c`/`_3a8e8`), and their new callees
`0x19164`, `0x192DC`, `0x39EFC`, `0x3AE9C`, `0x3B6C4`, `0x3A280`, `0x3A0FC`,
`0x3AA54` (plus whatever §5's closure shows). Wire the tail into
`fighter_pass_a` at `fighter.c:364-368`: call `fighter_winner_body(0)` in the
`AF8` arm and `fighter_winner_body(1)` in the `AFC` arm.

### 2.9 Unit-test values

See §8.2. Seed the winner flag and the slot; call `fighter_pass_a` (or
`fighter_winner_body` directly); assert `slot+0x52 == 0x10`,
`slot+0x53 == 0x0A`, `slot+0x54 == 0`, and `word[0x100B50 + side*2] ==
word[slot+0x84]`.

---

## 3. The winner gate (`0x18950`) — resolution

### 3.1 The body (152 B, `0x18950`..`0x189E7`, 0 callees)

`0x18950(EAX = param_1, EDX = param_2)`:

```
EBX = param_1*0x94 ; EDX = param_2*0x94          (0x18953..0x18970)
ESI = byte[EBX + 0x10780F]  ; = state[param_1]  (0x18975)
CL  = byte[EDX + 0x10780F]  ; = state[param_2]  (0x1897C)
BL  = byte[EBX + 0x10782A]  ; = char[param_1]   (0x18982)
AL  = byte[EDX + 0x10782A]  ; = char[param_2]   (0x1899B)
EAX = char[param_2] + char[param_1]*10           (0x1898E..0x189A1)
EDX = PTR_DAT_000a1290[EAX]                      (0x189A3)   ; the row
EAX = row + state[param_1]*8                     (0x189AA/0x189B1)
if CL < 0x20: test dword[row + state_1*8] & (1 << (CL & 0x1F))       (0x189B8..0x189C3)
else:         test dword[row + state_1*8 + 4] & (1 << ((CL-0x20)&0x1F)) (0x189CB..0x189DA)
return 1 on set, else 0
```

So `0x18950(a,b)` = "in the pair table for characters `a`,`b`, is bit
`state(b)` set in `a`'s state record?" — a move-connectivity (cancel/connect)
table.

### 3.2 The table

`PTR_DAT_000a1290` (100 dwords at `0xA1290`; index = `char_b + char_a*10`). The
demo's characters are `char_0 = 0`, `char_1 = 3`, so:

* `0x18950(0,1)` → index `3` → row `0x0009F290` (`ghidra_read_memory 0xA1290`:
  dword[3] = `90 f2 09 00`).
* `0x18950(1,0)` → index `30` → row `0x0009FE90` (dword[30] = `90 fe 09 00`).

Both rows read `0x9F290`/`0x9FE90` are **byte-identical**: each 8-byte record at
offset `k*8` is `AA AA 00 00 00 00 00 00` (dword `0x0000AAAA`) when `k` is even
and `00 00 FF 00 00 00 00 00` (dword `0x0000FF00`) when `k` is odd
(`ghidra_read_memory 0x9F290` / `0x9FE90`, 128 B each).

### 3.3 The demo's runtime state (measured)

The temporary trace (§0.2) over the demo's state-7 window (loop frames
1070..1969):

* slot 0 (T-rex): `slot+0x7A = 0`, `slot+0x5F = 0x0B` from loop 1072 on.
* slot 1 (raptor): `slot+0x7A = 3`, `slot+0x5F = 0x01` from loop 1071 on.
* `slot+0x40` bit 7: **clear for both** every frame (the `+0x40` overrides at
  `0x19666`..`0x19690` never fire).
* `word[slot+0x88]`: `slot0 = i-1072`, `slot1 = i-1071` — so
  `word[slot0+0x88] < word[slot1+0x88]` for the whole hold.
* `byte[slot+0x5A] = 0` for both.
* `AF8 = AFC = 0` every frame (the port never sets them).

The T-rex's `0x0B` matches cycle-3 §10.2; the raptor's `0x01` matches cycle-3
§3.3's Task 4 correction ("both trees hold … `+0x5F = 0x01`").

### 3.4 The two returns

* `0x18950(0,1)`: row `0x9F290`, offset `state_0*8 = 0x0B*8 = 0x58` → odd
  record → dword `0x0000FF00`; bit `state_1 = 1` → `0xFF00 & 2 = 0` → **0**.
* `0x18950(1,0)`: row `0x9FE90`, offset `state_1*8 = 1*8 = 8` → odd record →
  dword `0x0000FF00`; bit `state_0 = 0x0B = 11` → `0xFF00 & 0x800 = 0x800` →
  **1**.

So `bl = 0x18950(0,1) = 0`, `al = 0x18950(1,0) = 1`.

### 3.5 The verdict

The raw at `0x19692`..`0x196B7`:

```
0x19692 TEST BL,BL ; 0x19694 JZ 0x196A7
0x19696 TEST AL,AL ; 0x19698 JNZ 0x196A7
0x1969A AFC = 0                      ; BL && !AL -> side 0 wins
0x196A7 TEST AL,AL ; 0x196A9 JZ 0x196BC
0x196AB TEST BL,BL ; 0x196AD JNZ 0x196BC
0x196AF AF8 = 0                      ; AL && !BL -> side 1 wins
```

With `bl = 0`, `al = 1` and the `+0x40` overrides clear, the raw takes
`0x196AF` → **`AF8 = 0` → side 1 (the raptor) wins**, and `0x1973F`/`0x19748`
runs `0x193B0(1)`.

The port's take-both-as-0 (`fighter.c:328-330`) reaches the position compares at
`0x196BC`:

```
0x196BC MOV AX,[0x00107838] ; = word[slot0+0x88]
0x196C2 MOV DX,[0x001078cc] ; = word[slot1+0x88]
0x196C9 CMP AX,DX ; 0x196CC JGE 0x196D7
0x196CE AFC = 0             ; word[slot0+0x88] < word[slot1+0x88] -> side 0 wins
```

With `word[slot0+0x88] < word[slot1+0x88]`, the port zeroes `AFC` → **side 0
(the T-rex) wins**.

**The two differ.** The raw's `0x18950` queries gate the demo's winner: the raw
makes the raptor the winner at the 9/8 hold, the port's assumption makes the
T-rex. **`0x18950` is in scope (Task 4 exists).**

**The conflict (raw/measured vs raw/measured).** Cycle-3 §10.2 measured "the
original's T-rex leaves 9/8 for the pose state" — i.e. side 0 wins — which
contradicts the raw's `AF8 = 0` at the entry frame. Both are raw-derived. A
**plausible reconciliation**: the winner is recomputed each frame, so if the
raptor wins at loop 1072 and the T-rex at loop 1078, **both** pose, the raptor
first — and §10.2's "T-rex **6 frames later**" (loop 1078) is the T-rex posing
**second**, with the record not noting the raptor's earlier exit. The record
does **not** fit either value: §7.1 carries the conflict, and Task 3/5 measures
which side poses first (the `s7_saw10`/`s7_saw0a` counters name the slot). If
the measurement shows side 0 poses first, the `0x18950` table/state derivation
must be re-examined; if side 1 poses first, this section is confirmed and
§10.2 was mislabelled.

**Porting plan / test values** (§8.3): `fighter_connect_query` (`0x18950`)
wired at `fighter.c:328`; seed the character table and the two state bytes;
assert the return for the demo's `(0,1)`/`(1,0)` = `(0,1)`.

---

## 4. The `0x19020` no-op — confirmation

### 4.1 The body (70 B, `0x19020`..`0x19065`, 0 callees)

```
0x19023 MOV EDX,EAX
0x19025 SHL EAX,3 ; ADD EAX,EDX ; SHL EAX,2 ; ADD EAX,EDX ; SHL EAX,2   ; EAX = side*0x94
0x19032 CMP dword ptr [EAX + 0x1077c8],0x0    ; slot+0x18
0x19039 JZ 0x00019062                          ; == 0 -> return, writes nothing
0x1903B MOV EBX,EAX
0x1903F CALL dword ptr [EBX + 0x1077c8]        ; the slot+0x18 callback
0x19045 SHL EDX,2
0x19048 TEST EAX,EAX
0x1904A JNZ 0x0001905a
0x1904C MOV dword ptr [EDX + 0x100af8],0x1     ; AF8[hi(return)] = 1
0x1905A MOV dword ptr [EDX + 0x100af8],ECX     ; else AF8[hi(return)] = 0
0x19062 RET
```

`slot+0x18` is `0x1077C8` (`0x1077B0 + 0x18`). The guard `0x19032`/`0x19039`
returns before both `AF8` writes when it is 0.

### 4.2 Why the demo never sets `slot+0x18`

`slot+0x18` is set to `0x3FD30` only by `0x3FF08`
(`0x3FF77 MOV dword ptr [ECX+0x18],0x3FD30`). `ghidra_get_xrefs_to 0x3FF08`
returns exactly one: `0x40026` (an `UNCONDITIONAL_CALL` inside `0x3FFDC`).
`ghidra_get_xrefs_to 0x3FFDC` returns **zero** references — no code reaches the
closer script. `0x1077C8` is BSS (zeroed at load), so `slot+0x18 == 0` on every
demo frame, `0x19020` writes nothing, and the port's stub at `fighter.c:299`
stays a **named gap** (not a functional gap).

---

## 5. The size-gate verdicts

### 5.1 The method (cycle-3 §10.4, verbatim)

From `port/decomp/prage.calls.csv` (callee edges) + `prage.functions.csv`
(sizes). BFS from the roots, inclusive. **"new"** = in the closure AND not the
`0x6xxxx` runtime, not the RNG (`0x5Dxxx`), not the five known stubs
(`0x2C3FC`, `0x2EA64`, `0x62002`/`0x62003`/`0x6201B`), and **not named anywhere
in `port/src` except the generated `symbols.h`** (the naive heuristic: the
address appears as `0xADDR` in a `.c`/`.h`). **"true-new"** = naive + the roots
that are named-but-unported. The heuristic undercounts because a function merely
*named in a comment* counts as ported.

### 5.2 Per group

| group | roots | closure | naive-new | true-new |
|---|---|---|---|---|
| unfreeze | `0x140E4`, `0x170A0` | 193 f / 25 260 B | 32 f / 5 332 B | **34 f / 6 968 B** |
| pose-entry | `0x193B0`, `0x3B714`, `0x3AAFC`, `0x3A79C` | 310 f / 43 170 B | 53 f / 7 607 B | **54 f / 8 082 B** |
| union | the two above | 325 f / 48 326 B | 65 f / 11 020 B | **68 f / 13 131 B** |
| winner gate | `0x18950` | 1 f / 152 B | 0 f / 0 B | **1 f / 152 B** |
| *(closer script, out of scope)* | `0x3FD30`, `0x3FF08` | 305 f / 44 324 B | 40 f / 4 880 B | 41 f / 5 046 B |

The naive/true-new figures reproduce cycle-3 §10.4's corrected union exactly
(naive 65 f / 11 020 B, true-new 68 f / 13 131 B; the `+2 111 B` is
`0x140E4` 388 + `0x170A0` 1 248 + `0x193B0` 475). The pose-entry naive
(53 f / 7 607 B) matches the design's table.

### 5.3 Verdicts

* **The union is over the gate (68 f / 13 131 B ≥ both the ~4 KB and ~20-function
  lines) — and that is exactly why this cycle exists.** Cycle-3's gate triggered
  and routed the union here; the design already ratified it as this cycle's
  scope. The gate's forward purpose is to catch a **further** expansion beyond
  the union.
* **The unfreeze half (34 f / 6 968 B) and the pose-entry half (54 f / 8 082 B)
  are each individually over the gate**, but each is half of the already-ratified
  union — no new work, no new follow-on.
* **The winner gate (1 f / 152 B) is under the gate by both measures.** It is
  the only genuinely-new group this record adds, and it fits this cycle.
* **No group grew past the record's measurement.** No additional follow-on cycle
  is created.

---

## 6. The measurement plan (Task 5)

Task 5 establishes the observable with:

1. **Invocation.** `make demo-oracle`. The recipe (Makefile:196-205) removes
   `/tmp/pr_frontend_dump`, runs
   `PR_FRONTEND_DET=/tmp/pr_frontend_dump PR_GAME_DIR=$(GAME_DIR)
   ./build/run_tests` (the determinism driver, which writes `/tmp/pr_frontend_dump/run1`
   — the port's RGB24 frame dump plus `select.log`), then
   `python3 tools/title_compare.py --demo --capture data/title-captures/frontend
   --port /tmp/pr_frontend_dump/run1`.
2. **The oracle's fallback.** `tools/title_compare.py:481` calls `check_capture`
   on the demo port frames with `capture_lo = fe_b + 1`. When no capture frame
   after the front-end window explains any demo port frame, `res is None`
   (`:484`) and the script prints the report-only fallback (`:500-513`):
   `"0 clean, 0 splice, 0 transition, N unexplained"` and the first
   content-bearing capture frame — then **exits 0**. Leaving this fallback (a
   real `res`) is the acceptance's concrete witness.
3. **The 482/834 witness.** The port's fight frame that byte-matches capture
   frame 834 is dumped frame **482** (`/tmp/pr_frontend_dump/run1/frame_0482.raw`,
   the state-7 entry at loop 1071). The comparison is a per-byte RGB24 diff of
   the 320×200 frame against `data/title-captures/frontend/frame_0834.raw`:
   cycle-3 §10.5 measured **18 294 B (9.5 %) / 6 194 px** at frame 482, entirely
   the two fighters (left 1 549 px + right 4 645 px). The current diff is
   reproduced by the same per-byte count (a `python3` one-liner over the two
   files); the teal-mask silhouette IoU (region x 185..320, y 85..200; mask
   `g>90 && b>90 && g>r+30 && b>r+30`) is **1.000** at frame 482 (0.974 at 483).
4. **The residual first-unexplained frame.** Recorded with its evidence, not
   asserted to advance: it may remain the **831/832** entry (whose held-frame
   presentation is un-derivable; §7.6), and the later capture frames stay
   unexplained because the port dump is bounded by state 7's 900-frame timer
   (`test_frontend.c:738`, dump 0..1380) while the oracle window is
   `[831..3616]`. The report names both.

---

## 7. Named gaps

1. **The winner gate's conflict (new; §3.5).** The raw's `0x18950` gives
   `AF8 = 0` (side 1 / raptor wins) at the 9/8 hold; cycle-3 §10.2 measured the
   T-rex (side 0) leaving 9/8. Both are raw/measured. **Owner:** Task 3 (the
   `s7_saw10`/`s7_saw0a` counters name the slot) and Task 5 (the demo
   measurement). Evidence: `0x18950` disassembly; `PTR_DAT_000a1290` at
   `0xA1290`; the trace's `st0_5f=0x0B`, `st1_5f=0x01`, `w88_0 < w88_1`;
   cycle-3 §10.2's table.
2. **The demo's per-side `slot+0x5F` states are measured, not derived.** The
   trace reads them from the port, which tracks the original (cycle-3 §3.3), but
   they are not statically pinned from the raw. **Owner:** Task 3's assertion
   seeds them; Task 5 re-measures if the pose appears on the other side.
3. **`B54`'s post-value (`0x16DA4`).** `B54` is written only by `0x16DA4`
   (`0x16DC8`/`0x16E26`/`0x16F79`/`0x1706F`) and zeroed by `camera_decay`
   (`0x175E4`). `0x16DA4`'s body is ~40 unreachable blocks to Ghidra, and its
   `0x1631C` → `0x41030` (361 B) sprite path is unported. **Owner:** Task 2's
   `camera_winner_height`; the test asserts `AF8[side] == DSD(0x100B54)`, not a
   fitted `B54`.
4. **`LAB_0003A6D4`** (the `0x3A79C` pose callback) is absent from
   `prage.functions.csv` (a data pointer only, `prage.c:24880`); its size/role
   are unnamed. **Owner:** Task 3, if the `0x3A79C` arm is reached.
5. **`0x19020`** — confirmed no-op (§4); the port's stub at `fighter.c:299`
   stays a named gap, now with the raw proof.
6. **831/832's held-frame presentation** — un-derivable (the post-read ISR tick
   count is a host/emulator property; cycle-2 §9.6). Out of scope; the
   measurement plan (§6) records the residual.
7. **`0x38154`** — the `S+0x54 == 5` arm of `0x36638`/`0x385B0`; no in-scope
   path reaches it (`fighter.c:1248`). Unchanged.
8. **The `0x3Fxxx` closer script** (`0x3FD30`/`0x3FF08`/`0x3FFDC`) — never runs
   (§4.2; `0x3FFDC` has zero code xrefs). Out of scope.
9. **The `0x3Fxxx`-vs-gate concern** — the closer script's closure (41 f /
   5 046 B, §5.2) is over the gate, but it is unreachable, so it is not work.
   The gate applies only to reachable work; no follow-on.
10. **`0x2C3FC`** (the character voice, called from `0x3AAFC` and `0x3B714`) —
    the existing audio stub; out of scope.

No gap is dropped: every item is closed, re-scoped or carried with its owner.

---

## 8. Unit-test values (the substitutions for Tasks 2–4)

Slot base `S = DS_001077B0 + side*0x94`; `rec = DSD(S)`. All seeds are
sentinels that differ from the post-condition (never an unseeded BSS-zero), and
every assertion must fail under a mutation of the code it tests.

### 8.1 Task 2 — the unfreeze half (`port/tests/test_fight.c`)

`camera_decay` is `void`; the observable is `DS_00100AF8`/`AFC` and `B54`.

* **No-overlap arm.** Seed the actor table `DS_001014EC` and the two slots'
  `+0x56` actor indices so `0x140E4` returns 0 (the boxes disjoint); seed
  `B54 = 0xDEADBEEF`, `B60 = 1`, `B61 = 1`. Call `camera_decay()`.
  Assert `DSD(0x100B54) == 0` (`0x175E4` zeroed it; `0x170A0` not reached),
  `DSD(0x100AF8) == 0`, `DSD(0x100AFC) == 0` (`0x1769F`). Mutation: forcing
  `0x140E4` to return 1 flips `AF8`.
* **`B60` gate.** Overlap = 1, `B60 = 0`, `B61 = 1` (and side 1's guard passes:
  `DSW(0x107824) == 0 && DSW(0x107826) <= 1`). Assert `AF8[0] == 0`
  (`0x176A8`) and `AF8[1] == DSD(0x100B54)`.
* **`B61` gate.** Overlap = 1, `B60 = 1`, `B61 = 0`. Assert `AF8[0] ==
  DSD(0x100B54)` and `AF8[1] == 0` (`0x176B8`).
* **Both gates.** Overlap = 1, `B60 = B61 = 1`, both guards pass. Assert
  `AF8[0] == AF8[1] == DSD(0x100B54)` and `DSD(0x100B54) != 0xDEADBEEF`.
* **The `0x170A0` guard.** Seed the *other* side's `DSW(0x107824) = 1`
  (or `DSW(0x107826) = 2`); overlap = 1, `B60 = B61 = 1`. Assert `AF8[side] ==
  0` (`0x170C5`/`0x170E2`).

### 8.2 Task 3 — the pose-entry chain (`port/tests/test_fight.c`)

* **The pose state.** Seed the winner flag (`AF8[0] != 0`, `byte[0x10783A] != 0`)
  and the slot; call `fighter_pass_a()` (or `fighter_winner_body(0)` directly).
  Assert `DSB(S+0x52) == 0x10`, `DSB(S+0x53) == 0x0A`, `DSB(S+0x54) == 0`, and
  `DSW(0x100B50 + side*2) == DSW(slot+0x84)` (the `0x1945A`/`0x19472` store).
  Mutation: a pose setter that writes `+0x52 = 9` fails.
* **The side-1 arm.** Seed `AF8 = 0`, `AFC != 0`, `byte[0x1078CE] != 0`; assert
  the side-1 slot's pose state.
* **The `0x3AAFC` dispatch.** Seed `slot+0x54 = 0` and the reaction code so the
  `(ECX & 8) == 0` arm runs; assert `slot+0x52 == 0x10` (the `0x3A504` variant).

### 8.3 Task 4 — the winner gate (`port/tests/test_fight.c`)

* **The demo's returns.** Seed `byte[0x10782A] = 0` (slot0 char), `byte[0x1078BE] = 3`
  (slot1 char), `byte[0x10780F] = 0x0B` (slot0 state), `byte[0x1078A3] = 0x01`
  (slot1 state). Assert `fighter_connect_query(0, 1) == 0` and
  `fighter_connect_query(1, 0) == 1` (the demo's `(0,1)`).
  Mutation: swapping the two char bytes or the two state bytes flips a return.
* **The winner selection.** Seed `AF8 = AFC = 1`, `byte[0x10783A] = byte[0x1078CE] = 1`,
  the above characters/states, `slot+0x40` bit 7 clear both, and
  `word[0x107838] = 0 < word[0x1078CC] = 1`; call `fighter_pass_a`. Assert
  `AF8 == 0` (the raw's `0x196AF`) and `AFC != 0`. Mutation: taking both queries
  as 0 (the current stub) leaves `AFC == 0` and fails.

---

## 9. Provenance

* **§1.** `ghidra_disassemble_function 0x140E4`, `0x17580`, `0x170A0`;
  `ghidra_decompile_function 0x140E4`, `0x14080`, `0x170A0`, `0x17EEC`,
  `0x61A70`, `0x181D0`, `0x15F48`, `0x15FD4`, `0x16DA4`, `0x15C30`, `0x1631C`;
  `ghidra_get_function_callers 0x170A0` (one: `0x17580`), `0x140E4` (three);
  `ghidra_get_function_callers 0x17580` (seven, including `0x26254`);
  `ghidra_disassemble_function 0x26254` (`0x262E9 CALL 0x17580`, `0x262EE CALL
  0x1958C`); `ghidra_search_instructions 100b54` (the `B54` writers),
  `ghidra_read_memory 0x1424E` (the `LEA EAX/EDX,[ESP+0x10]` in-place
  intersection).
* **§2.** `ghidra_disassemble_function 0x1958C` (`0x19632`..`0x19752` — the
  `0x18950` calls and the winner tail); `ghidra_decompile_function 0x193B0`,
  `0x3B714`, `0x3AAFC`, `0x3A504`, `0x3A650`, `0x3A79C`, `0x3A8E8`;
  `ghidra_decompile_function 0x33950`; `ghidra_search_instructions 100b50`
  (the `0x193B0`-only `[0x100B50+side*2]` writer); the callee table from
  `prage.calls.csv`/`prage.functions.csv`.
* **§3.** `ghidra_disassemble_function 0x18950`; `ghidra_decompile_function
  0x18950`; `ghidra_read_memory 0xA1290` (400 B — `PTR_DAT_000a1290`),
  `0x9F290`, `0x9FE90` (128 B each). The runtime states from the temporary
  `test_frontend.c` trace (`/tmp/t1trace.txt`), reverted.
* **§4.** `ghidra_decompile_function 0x19020`; `ghidra_disassemble_function
  0x19020` (`0x19032 CMP dword ptr [EAX+0x1077c8],0x0; 0x19039 JZ 0x19062`);
  `ghidra_get_xrefs_to 0x3FF08` (one: `0x40026`), `0x3FFDC` (zero).
* **§5.** Recomputed from `port/decomp/prage.calls.csv` +
  `prage.functions.csv`; the port-named set from `port/src/**/*.{c,h}` minus
  `symbols.h`.
* **§6.** `Makefile:196-205`; `tools/title_compare.py:481-514`;
  `test_frontend.c:738`.
