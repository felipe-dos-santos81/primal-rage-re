# Demo fight, cycle 2 — closure: the state-9 globe, `0x3C88C`, the `0x3CF38` hit chain, the timer exit and the RNG source (Task 1)

Register-level derivation of the attract demo fight's closure from the shipped
`data/game/C/PRAGE.EXE`: what draws the state-9 hold's globe, what the `0x3C88C`
arena-render gap contributes, the full `0x3CF38` hit chain and **its size**, the
900-frame timer exit and the continue sequence, and where the state-6 RNG
nondeterminism originates. This is the record Tasks 2–6 implement from. It
changes no engine source (one comment marker in `port/src/game/fighter.c`, Task 1
Step 9).

Everything below is read from the shipped bytes. Data addresses are quoted
**post-relocation** (Ghidra's fixed-up view, whose address == linear address ==
object base + offset), as `port/spec/game_flow.md` and
`docs/superpowers/plans/2026-09-20-demo-fight-derivations.md` §0.1 require; the
raw-file form is pre-fixup and is used only for position-relative `E8` call
targets. On any conflict between this record and the bytes, the bytes win and the
conflict is recorded in §0.3.

**The cycle's three decisions, up front:**

1. **The `0x3CF38` chain is 45 functions in transitive closure (~8.2 KB), of
   which 22 functions / 2720 bytes are new to the port** (§3). That is a
   re-scope-sized number, not a footnote.
2. **The state-9 divergence is a render gap, not a state-entry error** (§1). The
   port reaches state 9 at the same frame and with the same values; the globe's
   projected layer never runs because the port skips `0x20DF4`, whose `0x38730`
   call is the only writer of `DS_00107A54`.
3. **The state-6 RNG nondeterminism has NO single pinnable origin** (§5). The
   seed is one deterministic site (`0x20C62 = 0xABCD`); the divergence is the
   unbounded, host-timed spin count in `0x255CC`. **This is an escalation to the
   human, not a licence to extend the constant pins.** Task 2 as planned cannot
   proceed as written.

---

## 0. Addressing, reproduction and corrections

### 0.1 The image and the two formulas

Same image and convention as `2026-09-20-demo-fight-derivations.md` §0.1: LE
image at file `0x290A4`; code object `0x10000`–`0x73B14` (file offset
`VA + 0x52E54`), data object `0x80000`–`0x10B0CF` (file offset `VA + 0x46E54`).
The code object is fully file-backed (no BSS), and a direct `E8 rel32` is
position-relative, so raw disassembly resolves code→code calls exactly; data
operands (the per-attack tables, the reaction tables, the RNG state) are taken
from Ghidra, whose fixups are applied.

### 0.2 Reproduction

The `E8`-target scans below are a read-only scratch script (not committed): it
disassembles the code object with 32-bit `capstone` from file offset
`VA + 0x52E54` and resolves `call` operands as `addr + 5 + rel`. Function
extents, caller counts and decompilation quotations are Ghidra's (`project
rage`, program `/PRAGE.EXE`), and the byte counts in §3 use Ghidra's
`body_end - entry`.

### 0.3 Corrections against the plan/brief (raw wins)

1. **The timer exit and the continue sequence are NOT unported** (corrects
   `2026-09-21-demo-fight-closure.md` Task 6's premise). `0x11BCC` and
   `0x11D04`'s case 7 are already transcribed at `port/src/game/flow.c:1349-1365`
   and were reached by cycle 1 (its own record derives them in §6.2). §4 states
   exactly what runs and what the port already reaches; only the out-of-scope
   `0x2C3FC(0x100)` voice is skipped.
2. **The chain does not touch health.** §3 derives what the hit chain actually
   writes, and a numeric hit-point field is not among them. The health-bar pass
   `0x33F08` (already ported, `fight_health_bars`) derives its sprite from the
   actor's world-Y word, not from anything the chain writes. If "health" in the
   brief meant an HP counter, the bytes show none in this chain; the reaction
   state, the stun timers and the hit counters are what the chain transitions.
3. **`0x3C88C` and the hit chain share four functions.** `0x3C88C` (§2) calls
   `0x3C600`/`0x3C6A8` — the same two the hit chain calls — so Tasks 4 and 5
   must port them once, in whichever lands first. Sizing them twice would
   double-count (see §3.7).
4. **`0x32BAC`'s Ghidra body is one byte.** Ghidra records
   `body_start == body_end == 0x32BAC`; the raw extent is `0x32BAC..0x32BD8`
   (0x2C, ending at the `RET` at `0x32BD8`, next entry `0x32BDA`). This record
   uses the raw extent for it.
5. **`0x186D0` and `0x18B04` are entered through a shared epilogue.**
   Ghidra gives `0x186D0` `body_start = 0x18625` (the tail of `0x18540`) and
   `0x18B04` `body_start = 0x18AAE`. The byte counts in §3 use `body_end - entry`
   so the shared tails are counted once, in the earlier function.

---

## 1. The state-9 globe render (Step 1)

### 1.1 The state entry is correct

The state-9 arm of the dispatcher is `0x11D04` case 9. Its whole body is the
timer (cycle-1 record §6.2, confirmed against `0x11D04`'s jump table at
`0x11CDC`): `DS_000F0A6A = (u16)(DS_000F0A6A - 1)`; when the new value is 0,
`DS_000F0A64 = DS_000F0A6C`. **It draws nothing.** The port's transcription at
`port/src/game/flow.c:1369-1375` matches, and every state's tail runs the same
`0x10DB0`/`0x10E18`/`0x2BF08` trio (`flow.c:1387-1389`).

State 9 is entered from state 3's phase 1 at `0x1262F`–`0x1264C`: the handoff
actor's `+0x1C` is forced to `0x1E00`, `DS_000F0A6C = 6`, `DS_000F0A64 = 9`,
`DS_000F0A6A = 0xF0`, `DS_000F0A72 = 0`. The port transcribes this at
`port/src/game/flow.c:664-672`, and its own measurement puts state 3's entry at
loop 589 / dumped 0, state 9 at dumped 240, state 6 at dumped 481
(`port/spec/game_flow.md:628-632`). The capture's frame 811 maps to the port's
dumped frame 264, inside the hold. **The port reaches state 9 at the right frame
with the right globals** — the state entry is not the defect.

### 1.2 The producer, the display-list path and the globals

State 9 emits no draw of its own, so the globe is drawn by the two per-frame
subsystems that run regardless of state:

* **the actor/display-list path** — `actors_update` (`0x2A31C`, called from
  `0x24C5C`'s tail, `port/src/game/flow.c:1275`) syncs every live record's pset,
  the render process table (`DS_000A86C4`) runs, `render_list_sort` (`0x1C3FC`)
  orders it, and `render_list` (`0x14328`) projects and blits it
  (`port/src/game/flow.c:1196-1198`); and
* **the attract scroll/zoom projection** — `render_scroll_edge` (`0x389C4`) and
  `render_scroll_fill` (`0x38A38`), called from the master loop `0x255CC` at
  `0x25601`/`0x25606` **only when `DS_00107A54 != 0`**
  (`port/src/game/flow.c:1187-1190`).

The globe actor itself is spawned by state 3: `0x12484` phase 0 walks the
front-end list and, for the entry whose first dword is the resource handle
`0x3E688`, spawns a type-3 palette effect (`effects_spawn(node, 2, 0x3E688)`,
`port/src/game/flow.c:635-639`); `0x12658`'s handoff (`0x9AC44`/`0x9AC58`/
`0x9AC6C`) spawns the three presentation actors and stores the first at
`DS_000F0A58` (`port/src/game/flow.c:598-619`). The palette layer is composed by
`effects_step` (`0x134C0`, `port/src/game/flow.c:1200`).

**The globals the globe reads:** `DS_000F0A58` (the handoff actor),
`DS_000F0A54`/`DS_000F0A50`/`DS_000F0A4C` (the attract actors),
`DS_00107A54` (the projection enable), `DS_00107A38`/`DS_00107A3A`/
`DS_00107A48`/`DS_00107A4A`/`DS_00107A4C`/`DS_00107A50` (the scroll/zoom tables
`0x389C4`/`0x38A38` maintain), and `DS_000F0AF0` (the camera x the shear table
accumulates).

### 1.3 Which is wrong: the render

The capture frame 811 is the "WHO WILL RULE THE NEW URTH?" globe; the closest
port frame (264) differs by **205 bytes in rows 98..144** — a sprite/content band
on the globe, not a whole-frame change (`port/spec/game_flow.md:673-686`,
`README.md:296-299`). The port draws the globe's island/landmass content on only
some rotation steps (`+264`/`+276`/`+282`/`+288` omit it; `+270` shows a smaller
one). The capture's demo fight does not begin until capture 836 (§9), 25 frames
later, so the difference cannot be a landing hit. **The state entry is correct;
the render is wrong.**

### 1.4 The address that explains the missing layer — and the named gap

`DS_00107A54` is written by exactly three sites (`0x4F200`, `0x4F228`, `0x38730`)
and read only at `0x255F8`. `0x38730` is the attract scene/zoom setup: it seeds
`DS_00107A4E = DS_000BDE1C[i]`, `DS_00107A42 = DS_000BDE2C[i]`,
`DS_00107A50 = DS_000BDE0C[i]`, `DS_00107A40 = DS_000BDDFC[i]`, derives
`DS_00107A3A = DS_00107A50 >> 5` and `DS_00107A38 = DS_00107A48 >> 6`, calls
`0x387F4`/`0x38890`/`0x38A38`, and **sets `DS_00107A54 = 1`** (`0x38730` body).
Its only callers are `0x20DF4` at `0x20E7F`/`0x20E90`, i.e. the state-6 fight
reset that **the port skipped as a named gap** (`2026-09-20-demo-fight-derivations.md`
§10.7; `port/src/game/flow.c:785-790` ports only `0x49300` from `0x20DF4`).

Consequence: **the port's `DS_00107A54` is never set, so the scroll/zoom
projection never runs**, and the globe's projected layer is coarser. The evidence
is a chain of addresses, not a fitted value: `0x20E7F → 0x38730 → DS_00107A54 = 1
→ 0x25601/0x25606`, gated at `0x255F8`.

**Named gap (Step 6).** The exact pixel content the projection contributes at
capture 811 (which of the `0x389C4`/`0x38A38` tables selects the globe's
landmass band, and the `0x38730` argument `i` the demo passes at `0x20E7F`) is
**not statically pinned**: `0x20DF4`'s argument to `0x38730` comes from its own
`param_2`/`param_3` register handoff, and its caller `0x11A8C` passes only
`(draw1, 1)` (cycle-1 §10.7). Task 3 must port `0x38730` with its argument
derived from `0x20DF4`'s raw body, and if the argument cannot be pinned the
globe layer is a Step 6 gap, **not** an invented table index.

---

## 2. The `0x3C88C` state-7 arena render gap (Step 2)

### 2.1 What calls it, and how often

`0x3CB68` (`fight_slot_pass`, `port/src/game/fight.c:145-158`) is the only
caller. Its body, confirmed at `0x3CB68`–`0x3CBC2`:

```
0x3CB68  DS_00107EDC = 0
0x3CB74  do {
             DS_00107EE4 = (u8)fighter_actor_bit15_clear(side)   ; 0x3CB89/0x3CB96
             DS_00107ED8 = 0
             do { 0x3C88C(); DS_00107ED8++ } while (DS_00107ED8 < 0x20)
             DS_00107EDC++
         } while (DS_00107EDC < 2)
```

So `0x3C88C` is called **64 times per arena frame** (2 sides × 32 slots). The
port's `fight_slot_pass` already keeps `DS_00107ED8`/`DC`/`EE4` and loops; the
inner call is the `/* PORT: ... */` skip at `port/src/game/fight.c:152-154`.

### 2.2 What `0x3C88C` contributes

`0x3C88C` (0x3C88C–0x3CB65, 730 B) is the **per-slot attack-frame state machine
that arms and clears the hitboxes the collision chain scans**. Its four callees
are `0x3C600`, `0x3C6A8`, `0x3C758`, `0x3C800`; it reads
`DS_00107ED8` (slot index `i`), `DS_00107EDC` (side), `DS_00107EE4` (the
facing/bit15 value), and the per-slot arrays (§3.2). Its structure, from the raw
at `0x3C88C`–`0x3CB65` and the jump table at `0x3C87C`
(`{0x3CAE8, 0x3CB5D, 0x3C912, 0x3CB41}`):

* `iVar2 = 0x3C600(side, i)` — the per-attack-frame descriptor pointer (§3.2).
  If it is 0, `0x3C6A8(side, 0, i)` clears the slot and returns.
* `iVar10 = (s16)word[0x107D58 + side*0x40 + i*2]` — the **phase**. `iVar10 < 0`
  or `> 8` clears via `0x3C6A8(EDX=0)`.
* **phase 8**: decrement `word[0x107DD8 + side*0x40 + i*2]`; while the signed
  result is `>= 1` return; at 0 clear (`0x3C6A8(EDX=0)`) and return.
* **phase 0**: `0x3C758(side, ...)` (the connect/input test, §2.3); on 0 call
  `0x3C6A8(EDX=1)`; then if `frame_ptr[0] == 0` and the phase word is non-zero
  set `word[0x107D58 + ...] = 8` — **this is the arming store the hit chain's
  `0x3CD44` scans for.**
* **phase 1**: `0x3C758`; if non-zero clear via `0x3C6A8`.
* **phases 2..7**: decrement `0x107DD8`; at 0 clear and reload the phase; read
  `word[0x107E58 + side*0x40 + i*2]`; when `2 <= phase < 8` call
  `0x3C800` (the connect/displacement test, §2.3) and, on success, write its
  out-value back into `0x107DD8`; then `0x3C758` dispatches 0..3
  (`0x3CAE8`/`0x3CB5D`/`0x3C912`/`0x3CB41`): 0 re-arms, 2 clears, 3 stores the
  accumulated value into `0x107E58`.

### 2.3 The two callees the hit chain does not need

* **`0x3C758(side, out)`** (body_end `0x3C7FF` − entry `0x3C758` = 167 B): reads the attack-frame
  descriptor (`0x3C600`), maps the frame's own input words through `0x3C6E8`
  (three calls), ORs them into `*out`, and compares against the side's command
  word `DS_001088E0[side]`: returns 2 when a command bit overlaps, 0 when the
  accumulated mask equals `*out`, 3 when partially overlapping, else 1. It is
  the "the player pressed the right button on this frame" test.
* **`0x3C800(side, out, idx)`** (0x3C800–0x3C878, 120 B): the same command-bit
  test; on a hit it returns 1 and writes
  `word[0x3C600(side, i) + 0x0A + phase*0x14] >> 16` into `*out` — the
  frame's displacement.

Neither draws. `0x3C88C`'s "render" is a misnomer carried from cycle 1: **the
arena render gap is that the attack-frame machine never runs, so no hitbox is
ever armed** (`0x107D58[...]` never becomes 8), which is also why the fight
stalls at `0x3CF38` (§3).

### 2.4 Is the state-7 arena render otherwise complete? — and the "broken globe" attribution

Yes, within the port's model: the six `camera_project` calls, `camera_decay`,
`fighter_pass_a`/`b`, `fighter_think`, `fight_slot_pass`, the two `fight_hud_pass`
calls, `fight_effects_pass` and `camera_scene_step` are all wired in
`fight_arena_frame` (`port/src/game/fight.c:492-527`). The observed symptom is the
**stalled arena** — the port's state-7 output freezes at the `0x3CF38` stall
(`port/spec/game_flow.md:687-692`): `0x3C88C` never arms a hitbox, so the
collision chain never resolves. The address that explains the stall is `0x3C88C`
(the arming store, §2.2), with `0x3C6A8` the only other writer of `0x107D58`.

**Correction to the spec's attribution (raw wins).** The cycle-2 plan and
`port/spec/game_flow.md:664` call `0x3C88C` "the broken globe background". The
bytes say otherwise: `0x3C88C` reads the attack-frame descriptor (`0x3C600`), the
three per-slot arrays and the command word (`0x3C758`/`0x3C800`) and **draws
nothing** — it is the hitbox machine, and its failure is the fight stall, not a
background. The state-7 "broken globe background" is the §1.4 projection gap
(`DS_00107A54` never set), the same address chain as the state-9 globe. Task 4
must not expect `0x3C88C` to repair the background; the schedule in the plan
(Task 3 state-9 globe, then Task 4 `0x3C88C`) fixes the projection before the
hitbox machine, which is the right order.

---

## 3. The `0x3CF38` hit chain — and its size (Step 3)

### 3.1 The entry, its callers, and what it returns

`0x3CF38(side)` (EAX = side; 0x3CF38–0x3D002, 202 B). Body (raw `0x3CF38`):

```
i = 0x3CD44(side)                          ; the active hitbox index, or -1
if (i == -1) return 0
if (0x10 <= slot+0x5F <= 0x17 && i >= 0x1C) return 0   ; 0x3CF64..0x3CF75
if (0x3CE58(side, i) != 0) {              ; validated: store the reaction
    slot+0x7C++                           ; 0x3CF9C/0x3CFA6  hit counter (byte)
    slot+0x55 = (u8)i                     ; 0x3CFAE        hit index (byte)
    0x3C6A8(side, 0, i)                   ; 0x3CFB4        consume the hitbox
    if (slot+0x63 == 0) 0x32BAC(slot+0x7A) ; 0x3CFB9/0x3CFCA  hit sound
    return 1                              ; 0x3CFCF (AL=1)
}
if (slot+0x53 == 0) slot+0x5F = 0xFF      ; 0x3CFD6/0x3CFDF
slot+0x55 = 0xFF                          ; 0x3CFF4
return 0
```

Callers on the demo path: `0x35D7C` at `0x35DE8` (`fighter_state_35d7c`,
`port/src/game/fighter.c:1304`), `0x3BDDC` at `0x3BE61`
(`fighter_attack_consume`, `fighter.c:1332`), and `0x19068` at `0x190E7`
(`fighter_pass_b`, `fighter.c:341`). All three are `/* PORT: ... */` skips today.

### 3.2 The per-slot arrays and the tables (widths)

| address | width | meaning | writers / readers |
|---|---|---|---|
| `0x107D58 + side*0x40 + i*2` | `u16[0x20]` | **attack phase**; 8 = armed | `0x3C6A8` (write), `0x3CD44` scan, `0x3C88C` |
| `0x107DD8 + side*0x40 + i*2` | `u16[0x20]` | **hit-stun countdown** | `0x3C6A8` (seed), `0x3C88C` (dec) |
| `0x107E58 + side*0x40 + i*2` | `u16[0x20]` | per-hit displacement accumulator | `0x3C6A8` (0), `0x3C88C` |

(All three are dword-read at `0x107D56`/`0x107DD6` and arithmetic-shifted right
16, which selects the `+2` word; the effective arrays start at `0x107D58`/
`0x107DD8`/`0x107E58`. `0x3C6A8`'s store form
`word[ECX + EBX*2 + 0x107D58]` with `ECX = side << 6` confirms the stride.)

The per-attack-frame descriptor is `0x3C600(side, i)` (0x3C600–0x3C6A4, 164 B):

```
sel = (slot+0x63 != 0) ? 2 : word[DS_00101514 + 0x2D4 + side*2]
switch (sel) { case 0/4/6: return dword[0xC6B9C + (char*0x20+i)*8];
               case 2:     return dword[0xC619C + (char*0x20+i)*8];
               default:    return (the caller's ECX) }
```

`char = DSB(slot+0x7A)`. The two 8-byte entries are
`{u32 frame_table; u16 reaction; u8 d; u8 e}`:
`0xC619C` is the player-controller table and `0xC6B9C` the CPU table. The
**demo** has `slot+0x63 = 1` (set by `fight_char_select`, `fighter.c:85`), so
`sel = 2` and the demo reads **`0xC619C`**. Verified entry 0:
`0xC619C` = `3c fe 0b 00 | 20 00 | 01 00` → frame_table `0x000BFE3C`,
reaction `0x0020`, `d = 0x01`, `e = 0x00`.

The per-attack-frame table (`0xBFE3C`, stride `0x14`) carries the stun countdown
at `+0xC`: entry 0 is `00 00 08 00 00 40 00 00 00 00 00 00 03 00 00 00` →
`word[+0xC] = 3`.

### 3.3 `0x3CD44` — find the armed hitbox

`0x3CD44(side)` (65 B):
```
for i = 0..0x1F:
    if ((s16)word[0x107D58 + side*0x40 + i*2] == 8 && 0x3CCEC(side, i)) return i
return -1
```

### 3.4 `0x3CCEC` — the hitbox is valid against the target's stance

`0x3CCEC(side, i)` (87 B):
```
char = DSB(slot+0x7A); entry = char*0x20 + i
e = DSB(0xC619C + entry*8 + 7)          ; entry+7
d = DSB(0xC619C + entry*8 + 6)          ; entry+6
if (e != 0 && DSB(slot+0x54) == 2) return 1
if (d != 0 && (DSB(slot+0x54) == 1 || DSB(slot+0x54) == 0)) return 1
return 0
```
So `d`/`e` are the attack frame's "valid against stance" flags: `e` for the
stance `0x54 == 2` (crouch), `d` for `0x54 in {0,1}` (stand/walk).

### 3.5 `0x3CE58` — validate and drive the reaction

`0x3CE58(side, i)` (0x3CE58–0x3CF37, 223 B):
```
if (0x3CE24(side, i) == 0) return 0                       ; 0x3CE5E
if (i < 0 || i >= 0x20) return 1                          ; 0x3CE6B/0x3CE73
if (DS_00104B00 in {0x21, 0x22}) {                        ; 0x3CE7C..0x3CE8C
    if (0x4CE70(side, reaction) == 0) return 0            ; 0x3CEB9
}
0x34D8C(side)                                             ; 0x3CEC8  +0x59 flags
reaction = (s16)(word at 0xC619C + entry*8 + 4)           ; 0x3CEEC..0x3CEF3
if (reaction == 0x10)      reaction = 0x3CBC4(side)       ; 0x3CF04
else if (reaction == 0x11) reaction = 0x3CC58(side)       ; 0x3CF0D
slot+0x5F = (u8)reaction                                  ; 0x3CF25
0x34E2C(side, reaction)                                   ; 0x3CF2E
return 1
```

`0x3CE24(side, i)` (0x3CE24–0x3CE57, 51 B) gates on `(s8)(dword[slot+0x53] >> 24)
<= 5` (that is **byte `slot+0x56`**), then returns `0x3CD94(side, i) != 0`.
`0x3CD94(side, i)` (0x3CD94–0x3CE20, 140 B) is the hit-stun immunity test:
```
r = DSB(slot+0x5F)
if (r == 0xFF) return 1
if (r < 0 || r >= 0x40) return 0
c = (s16)(word at 0xC619C + (char*0x20+i)*8 + 4)          ; the reaction index
bit = c & 0x3F
mask = dword[0xA182C + char*0x200 + r*8]                  ; 64-bit per-char mask
if (c < 0x20) return (mask >> bit) & 1
else          return (dword[0xA182C + char*0x200 + r*8 + 4] >> (c-0x20)) & 1
```
`0xA182C` is 512 bytes = 64 rows of 8 bytes, one table per character
(`char*0x200`), indexed by the current reaction `r`. The rows fall in three
groups: rows `0x00..0x0F` are `00 00 ff 00 ff ff ff 7f` (dword0 bits
`0x10..0x17`, dword1 bits `0x00..0x1E`); rows `0x10..0x1F` are
`00 00 00 00 ff ff ff 7f` (dword0 clear, dword1 bits `0x00..0x1E`); rows
`0x20..0x3F` are all zero. So a fresh `r = 0xFF` returns 1, and a target in
reaction `r` can only be re-hit by an attack whose reaction `c` has row `r`'s
bit set.

`0x3CBC4`/`0x3CC58` (145 B each) pick a reaction variant from `DS_001088E0[side]`
bit `0x4000`, `0x1DDF4` (109 B, the attacker/defender geometry test via the two
`0x187FC` slot latches), and `0x4CE70` (175 B, the per-character reaction
allow-list indexed by the reaction id).

### 3.6 `0x34E2C` — the reaction driver (the "reaction-animation path")

`0x34E2C(side, reaction)` (0x34E2C–0x3504F, 548 B). With
`self_slot = slot[side]`, `other_slot = slot[1-side]`, `self_rec = *self_slot`,
`other_rec = *other_slot`:

```
if (reaction == 0xFF) return                                        ; 0x34E36
self_slot+0x88 = 0; self_slot+0x8A = 1                              ; 0x34E9E/0x34EAE
if (self_slot+0x54 == 2) DS_001078F8[side] = 0                      ; 0x34EBC
if (self_slot+0x54 != 2) 0x18B04(side)                              ; 0x34ECF
0x1922C(side)                                                       ; 0x34ED7
word[self_slot+0x84]++                                              ; 0x34EE0/0x34EEB
self_slot+0x5F = reaction; DS_001088A8[side] = reaction             ; 0x34EE7/0x34EF6
DSB(self_rec+0x63) = 0                                              ; 0x34EFC
if (DSB(0x1078FA) == 2) { self_rec+0x59 = 1; other_rec+0x59 = 0xFF } ; 0x34F07..0x34F47
0x3AFC4(side, reaction, &anim)      ; the (0xDE114, 0xA3528, 0xA6728) triple
if (*(u32*)(anim[1]+4) != 0) {   ; 0x34F64; anim[1] = 0xA3528 + idx*0x14
    sound = word[0xE9308 + (u8)DSB(anim[0]+7)*2]                    ; 0x34F97
    0x2C3FC(sound)                                                  ; 0x34FA4 (voice)
    if (self_slot+0x54 != 2) 0x3C4CC(self_rec, 0x40000000)          ; 0x34FBC
    else                     0x3C520(self_rec, 0x40000000)          ; 0x34FCC
    if (self_slot+0x52 != 4) self_slot+0x52 = 9                     ; 0x34FDB
    self_slot+0x53 = 8                                              ; 0x34FE3
    word[self_slot+0x6A]++                                          ; 0x34FF7
    if ((word[anim[2]+2] & 0x10) != 0) self_slot+0x41 |= 0x80       ; 0x35004
} else {
    self_slot+0x5F = 0xFF                                           ; 0x3500E
}
if (*(u32*)(anim[1]) != 0) { 0x2C3FC(sound); self_slot+0x5F = reaction;
                             call *(void**)anim[1] }                ; 0x35012..0x35045
```

`0x3C4CC`/`0x3C520`/`0x3C480` (82/76/75 B) are the animation-start + screen-anchor
wrappers over `0x2BC30` (`actors_anim_begin`), `0x339AC` (the ctx builder from a
record), `0x188AC`/`0x188DC`/`0x1890C` (the screen-anchor setters), and the
`slot+0x52` dispatch. `0x34D8C` (78 B) is the `+0x59` palette-flash pair.
`0x3C6A8(side, value, i)` (60 B) seeds/clears the three arrays and stores
`word[frame_table + 0xC + value*0x14]` at `0x107DD8[...]` (the frame index is the `value` argument, not the slot index). `0x32BAC` (44 B) plays the
two hit sounds `char+0x1C` / `ebx+0x1C` through `0x2DAE4` (a sound wrapper).

**The `+0x52` transitions the chain drives are table-gated, not unconditional.**
The `slot+0x52 = 9` / `slot+0x53 = 8` / `slot+0x6A++` arm runs only when
`*(u32*)(anim[1]+4) != 0`. That field is table data at
`0xA3528 + ((char<<6)+reaction)*0x14 + 4`, and it varies: char 0 / reaction 0
reads `0x000C8B80` (nonzero → the arm runs), while char 0 / reaction `0x20` —
the demo's own first-hit reaction, because `0xC619C` entry 0's reaction field is
`0x20` — reads `0` (the arm is **not** taken; the driver falls to the
`else` arm and then the callback block below). A second `else`
(`slot+0x5F = 0xFF`) is followed by an unconditional block when `*(u32*)anim[1]`
is nonzero: it sets `slot+0x5F = reaction` and calls `*(void**)anim[1]`
(char 0 / reaction 0x20: `0x0003D17C`). On the `0x35D7C` / `0x3BDDC` no-hit arm,
`slot+0x54 = 2`, `slot+0x53 = 4`. `+0x52 == 9` is a `0x34B14` table no-op
(entries 9..16); when it is taken the reaction plays through the animation
stream, not a per-frame handler.

**A health/HP field is not written by the chain.** What it does write:
`slot+0x7C` (byte, hit counter), `slot+0x55` (byte, hit index), `slot+0x5F`
(byte, reaction), `+0x84`/`+0x6A` (words, counters), `+0x8A`/`+0x88`,
`slot+0x52`/`+0x53`, `slot+0x41` bit 0x80, `slot+0x59`, `DS_001088A8[side]`,
`DS_001078F8[side]`, and `self_rec+0x63`. The health-bar pass `0x33F08`
(`fight_health_bars`, `fight.c:113-141`) reads `slot+0x24` (a `DS_000BDA8C[char]`
table) and the actor's world-Y word — nothing the chain writes.

### 3.7 The chain's size (the cycle's first-class deliverable)

Ghidra's direct-`E8` transitive closure from `0x3CF38` reaches **45 functions**.
To avoid double-counting the shared epilogues (§0.3.5) the byte column is
`body_end - entry` (and the raw extent `0x32BAC..0x32BD8` for `0x32BAC`, §0.3.4).

**New to the port (22 functions, 2720 bytes):** the hit detection, reaction
selection and reaction driver the port has no equivalent for.

| function | bytes | role | section |
|---|---|---|---|
| `0x3CF38` | 202 | hit wrapper / dispatcher | §3.1 |
| `0x3CD44` | 65 | armed-hitbox scan | §3.3 |
| `0x3CCEC` | 87 | stance-validity test | §3.4 |
| `0x3CE58` | 223 | validate + drive reaction | §3.5 |
| `0x3CE24` | 51 | `slot+0x56` gate + immunity | §3.5 |
| `0x3CD94` | 140 | hit-stun immunity bitmask | §3.5 |
| `0x3CBC4` | 145 | reaction variant A | §3.5 |
| `0x3CC58` | 145 | reaction variant B | §3.5 |
| `0x34D8C` | 78 | `+0x59` palette flash | §3.6 |
| `0x34E2C` | 548 | reaction driver | §3.6 |
| `0x3C6A8` | 60 | seed/clear the three arrays | §3.2/§2.2 |
| `0x3C600` | 164 | per-attack-frame descriptor | §3.2 |
| `0x3C4CC` | 82 | anim-start wrapper | §3.6 |
| `0x3C520` | 76 | anim-start + anchor | §3.6 |
| `0x3C480` | 75 | anim-start + anchor | §3.6 |
| `0x339AC` | 98 | ctx builder from a record | §3.6 |
| `0x4CE70` | 175 | per-char reaction allow-list | §3.5 |
| `0x1DDF4` | 109 | attacker/defender geometry | §3.5 |
| `0x32BAC` | 44 | hit sound | §3.6 |
| `0x188AC` | 45 | screen-anchor setter | §3.6 |
| `0x188DC` | 44 | screen-anchor setter | §3.6 |
| `0x1890C` | 64 | screen-anchor setter | §3.6 |

**Present in the port or already reached under another name (18 functions,
~4200 bytes)** — these are not new work, but Task 5 must confirm each port's
semantics match. Two of them (`0x1922C`, `0x18B04`) are *reached but declared
incomplete* (cycle-1 §7.12 gaps): they stay in this bucket because they are not
new functions, but they are not fully ported either:
`0x33950` (`fighter_ctx_same`), `0x3AFC4` (`fighter_anim_triple`), `0x186D0`
(`fighter_slot_latch`), `0x18714`, `0x187FC` (`ai_distance`'s helper), `0x1881C`,
`0x1922C` (gap §7.12), `0x18B04` (gap §7.12), `0x18350`, `0x18428`, `0x18460`,
`0x18540`, `0x2B2A0` (`spawn_anim_opcode`), `0x2B8F8`, `0x2BC30`
(`actors_anim_begin`), `0x2A408`, `0x29F34`, `0x1A570`
(`fighter_actor_bit15_clear`).

**Out-of-scope stubs (5 functions in the closure, ~1313 bytes):** `0x2C3FC` (the
1267-byte voice dispatcher), `0x2EA64` (a 1-byte `ret`), and
`0x62002`/`0x62003`/`0x6201B` (the `0x62003` error path). `0x32BAC`'s own callee
`0x2DAE4` (a sound wrapper) lies outside the Ghidra closure only because
`0x32BAC`'s Ghidra body is one byte (§0.3.4); it is a stub too. Their port calls
stay `/* PORT: */` skips.

**Total closure: 45 functions, ~8.2 KB. The new work is 22 functions / 2720
bytes (~2.7 KB).** Of those 22, `0x3C600` (164 B) and `0x3C6A8` (60 B) are shared
with Task 4: `0x3C88C` (§2) is not itself in the `0x3CF38` closure, and its own
new work is `0x3C88C` (730) + `0x3C758` (167) + `0x3C800` (120) = 1017 bytes plus
the same two shared helpers (224 B). The 224 is counted once in the 2720. This is
a single-derivation-sized chain; Task 5's "stop and report" gate (§Task 5 Step 1)
is **not** triggered.

### 3.8 Does the chain consume RNG? No.

A byte scan of every function extent in the closure (including `0x3C88C` and its
two draw-helper callees) for `call 0x5D7DC` finds **zero** sites. The chain is
RNG-free. The RNG draw a hit *does* eventually cause is in the downstream
`0x39040` (which reads `DS_001088A8[side]` at `0x391E9` and draws `0x5D7DC`),
reached through the `+0x52` handlers, not through `0x3CF38`. **Consequence for
Task 5:** porting the chain cannot itself shift the Oracle's RNG stream; if a
post-hit divergence appears it is in the `+0x52` handler or `0x39040`, and its
source is derivable there — not a licence to fit a pin.

### 3.9 The `0x3C88C`/hit-chain overlap (Task 4 ↔ Task 5)

`0x3C88C` calls `0x3C600` and `0x3C6A8`; the hit chain calls both too. Tasks 4
and 5 must own each on its first landing and cross-reference; the 2720-byte "new"
figure above counts them once (in Task 5's table).

---

## 4. The timer exit and the continue sequence (Step 4)

### 4.1 The exit runs `0x11BCC`

The state-7 arm (`0x11D04` case 7, `0x11E67`–`0x11E9A`) is in cycle-1 §6.2 and
matches `port/src/game/flow.c:1349-1365`: store `DS_000F0A6A = (u16)(timer-1)`;
when the new value is 0 call `0x11BCC`; else run `0x263F4` + `0x33F08`.

`0x11BCC` (41 B, raw confirmed):
```
DSB(DS_00104B19+2) = 0
DSB(DS_00104B15) = 0
DSW(DS_000F0A64) = DSW(DS_000F0A6C)
0x29D60()                       ; a ret-only no-op
0x2C3FC(0x100)                  ; voice, out of scope
```

### 4.2 The continue sequence is `DS_000F0A6C = DS_000F0A72`, and for the demo it is 0

`DS_000F0A6C` is written by: `0x10DEE` (`0x10DB0`'s write), `0x10E56`
(`0x10E18`'s write), `0x119E6` (`0x11000`), `0x11E2E` (state 5 → 6),
`0x11BB9` (state 6 → `DS_000F0A72`), and `0x12628` (state 3 → 6). In the demo's chain state 3's phase
1 sets `DS_000F0A72 = 0` at `0x1264C`, so state 6's `DS_000F0A6C = DS_000F0A72`
(`0x11BB9`) is **0**, and the 900-frame exit hands to **state 0, the attract
sub-machine** (`0x11000`). The nonzero continuations (`DS_000F0A72 ∈ {3,4,5}`
from the attract phase, `0x1150E`/`0x11517`/`0x1151F`, `port/src/game/attract.c:304-307`)
belong to the *next* attract loop, not this demo run.

### 4.3 What the port already reaches

* **The timer exit:** already ported (`flow.c:1354-1364`) and reached — the dump
  that cycle 1 measured ends exactly there (`port/spec/game_flow.md:628-632`).
  Only the `0x2C3FC(0x100)` voice is skipped (out of scope).
* **The continue sequence:** the handoff to state 0 is ported (`flow.c:1358`);
  `attract_step`'s `DS_000F0A72` setup for a nonzero continuation is ported
  (`attract.c:304-307`). Task 6's premise that both are unported is corrected
  (§0.3.1). What Task 6 can still add is a unit test pinning the case-7
  transition (§7.6) and the record's statement that the demo loops back to the
  attract.

---

## 5. The state-6 RNG nondeterminism's source (Step 5)

### 5.1 The seed is a single deterministic site

The only store to the LCG state `DS_000EF6D8` in the image is
`0x20C62 mov dword[0xEF6D8], EBX` with `EBX = 0xABCD` set at `0x20C53`, inside
`0x20C10` (the init). `0x5D7DC` (raw and `port/src/game/rng.c`) is the LCG
`state = state*0xB90D12B9 + 0x38CE051F; return ((state>>16)*(range&0xFFFF))>>16`.
`DS_000EF6D8` is BSS (zero at load) and is seeded before the first draw, so there
is **no uninitialised or timing-dependent read at the seed**. The port reproduces
this exactly (`flow.c:1100 rng_seed(0xABCD)`).

### 5.2 The divergence is the unbounded host-timed spin, and it has no single site

The master loop `0x255CC` draws RNG at **two** sites, both `rng(0x7FFF)`
(`EBP = 0x7FFF`):

* the body draw at `0x256B1` (`EAX = EBP`), after the present/swap and before
  `0x1CF20`; and
* the **spin** draw at `0x256D6`, inside
  `while (DS_000F0A0C - 1 == DS_000F0A08) { 0x256D6 rng_step(); }`
  (`0x256C6`–`0x256DB`).

`DS_000F0A08` is the VBlank tick counter (ISR-incremented); `DS_000F0A0C` is the
loop counter. The spin runs a **host-timed, unbounded** number of times per
presented frame, and **each spin advances the LCG**. So the stream position at
state 6 is the seed plus a variable number of draws that depends on the host's
real-time pacing, not on any game state.

### 5.3 Verdict: no single pinnable origin — escalate

**There is no single pinnable origin for the state-6 nondeterminism.** The seed
(`0x20C62`) is a single *deterministic* site and pinning it changes nothing; the
variability is the *count* of draws from `0x255CC`'s spin (`0x256D6` plus the
body draw `0x256B1`), which is not one site but a loop. A constant-replacement
pin at the two `0x11AAD`/`0x11AE9` draw sites forces the picks but does **not**
advance the pinned original's LCG, which is exactly the two-draw offset cycle 1
recorded (`port/spec/game_flow.md:728-737`). No in-place constant pin can both
force a value and advance the LCG, so the current pin shape cannot align the
stream.

**This is the escalation the design spec's Risks section anticipates.** Task 2 as
planned ("replace the two constant pins with the single pin Task 1 Step 5
identified") **cannot proceed as written**: there is no single pin. The options
are the human's to choose, and the record does not pick one:

1. pin `0x256B1`/`0x256D6` (the master-loop draws) to non-advancing
   `mov eax,0` in the pinned original and make the port's `game_loop` also stop
   drawing there, so both streams carry only the "meaningful" draws — a
   two-site change and a port behaviour change;
2. leave the constant pins and accept that no oracle result can support the
   claim that the port's state-6 RNG handling is faithful (cycle 1's position);
   or
3. model the master loop's spin count in the port (i.e. drive the port from the
   same paced counter), which changes the port's frame model.

Until the human rules, Task 2 is BLOCKED and no later task may compare frames
past the character picks.

---

## 6. What could not be determined (named gaps)

1. **The state-9 globe's projected content** (§1.4). `0x38730` is the producer of
   `DS_00107A54` and the scroll/zoom tables, called only from `0x20DF4` at
   `0x20E7F`/`0x20E90`; the demo argument `i` to `0x38730` is a register handoff
   in `0x20DF4` not statically pinned, and the `0x389C4`/`0x38A38` table rows it
   selects are not decoded. Evidence: `0x20DF4`, `0x38730`, `0x255F8`,
   `0x25601`/`0x25606`.
2. **`0x20DF4`'s remaining resets** (cycle-1 §10.7): `0x29B70`, `0x2C390`,
   `0x12750`, `0x49300` (ported), `0x28E98`, `0x34978`, `0x2C074`, `0x2BAF4`
   (ported), `0x38730`, `0x412A0`. `0x38730` is now load-bearing (§1.4).
3. **`0x3C88C`'s phase-2..7 body and the `0x3C800` out-value** (§2.2/§2.3): the
   control flow and the arming store are pinned, but the exact `0x3C800`
   displacement arithmetic (`frame_table + 0x0A + phase*0x14` high word) and the
   `0x3C758` return semantics are transcribed, not unit-pinned.
4. **The reaction tables' semantics** (§3.6): `0xDE114`/`0xA3528`/`0xA6728` are
   the same triples `fighter_anim_triple` already indexes, but the *meaning* of
   the `0xA3528` entry fields (`+0`, `+4`), the `0xE9308` sound index and the
   callback invoked at `0x35045` are not decoded. The shipped fixture's values
   and its branch are pinned in §7.11; the table-driven tail is §6.9.
5. **`0x39040`'s hit-reaction RNG follow-up** (§3.8): it reads `DS_001088A8` and
   draws `0x5D7DC`, so it *is* a second RNG consumer on the hit path; its body is
   not decoded and it is not in the chain's closure.
6. **The think chain's interiors** (already cycle-1 §7.12) remain gaps and are
   marked unexercised in §8.
7. **`0x1DDF4` (109 B), the attacker/defender geometry test** (§3.5, §7.11): it
   compares `|0x187FC(...)|` against `|0x1881C(...)|` and a per-character
   threshold, and both helpers latch the slots through `0x186D0` and read actor
   positions. Its result is not statically pinnable without seeding the actor
   pool, and the `0x12`/`0x10` (or `0x13`/`0x11`) reaction-variant tail of
   `0x3CBC4`/`0x3CC58` depends on it. Evidence: `0x1DDF4`, `0x187FC`, `0x1881C`.
8. **`0x3C4CC` / `0x3C520` / `0x3C480` (82/76/75 B), the anim-start wrappers**
   (§3.6): their only output is a `0x2BC30` (`actors_anim_begin`) call plus a
   screen-anchor write. `0x3C4CC`'s dispatch is pinned (`slot+0x52` in
   {0,1,2,5,0xE,0x15} → `0x2BC30`, else `0x3C480`), but the animation records
   and the anchor arithmetic are the same machinery cycle-1 already lists as
   gaps. Declared here rather than given invented outputs.
9. **`0x34E2C`'s table-driven tail** (§3.6, §7.11): the head writes and the
   shipped fixture's branch are pinned, but the `0xA3528` entry fields and the
   `0x3D17C` callback at `*(u32*)anim[1]` are not decoded. The `+0x52 = 9` arm
   is reachable only for a reaction whose `0xA3528+4` is nonzero (witness char
   0 / reaction 0 = `0x000C8B80`; char 0 / reaction 0x20 = `0`).

---

## 7. Unit-test values per function (Step 7)

Everything below is "seed these `mem[]` globals / record fields, call this
function, read these globals", so a test that cannot call `game_init()` can use
them. `DSD/DSW/DSB` are the port's typed accessors. Where a value cannot be
pinned it is in §6, not here.

### 7.0 `0x11D04` case 9 — the state-9 hold (Task 3's state transition)

Input: `DSW(DS_000F0A64) = 9`; `DSW(DS_000F0A6A) = 0xF0`;
`DSW(DS_000F0A6C) = 6`. Call `game_state_step()`. Expected:
`DSW(DS_000F0A6A) == 0xEF` and `DSW(DS_000F0A64) == 9` (still holding).
Input B: `DSW(DS_000F0A6A) = 1` → `DSW(DS_000F0A6A) == 0`,
`DSW(DS_000F0A64) == 6` (the handoff). The **render** half of Task 3 is §1.4
(`0x38730` via `0x20DF4` → `DS_00107A54`) and its unpinned argument is the
§6.1 gap.

### 7.1 `0x3CF38` — `hit_chain_resolve(side)`

Input A: `DSD(0x1077B0) = P`; `DSW(0x107D58 + 0) = 8` (armed hitbox 0);
`DSB(P+0x7A) = 0`; `DSB(P+0x54) = 0`; `DSB(P+0x56) = 0`; `DSB(P+0x5F) = 0xFF`;
`DSB(P+0x63) = 1`. Expected: returns 1; `DSB(P+0x55) == 0`;
`DSB(P+0x5F) == 0x20`; `DSB(P+0x7C) == 1`; `DSW(0x107D58 + 0) == 0` (consumed).
Input B (mutate the arm): `DSW(0x107D58 + 0) = 7` → returns 0, `DSB(P+0x5F) == 0xFF`,
`DSB(P+0x55) == 0xFF`, and `DSB(P+0x7C)` unchanged (seed it `0x40`).

### 7.2 `0x3CD44` — armed-hitbox scan

Input: `DSB(P+0x7A) = 0`; `DSB(P+0x54) = 0`; all `word[0x107D58 + i*2] = 0` for
`side 0`, except `word[0x107D58 + 0x0A] = 8` (index 5). Expected: returns 5.
Input B: no phase word is 8 → returns `0xFFFFFFFF`.

### 7.3 `0x3CCEC` — stance validity

Input A: `DSB(P+0x7A) = 0`; `DSB(P+0x54) = 0`. Entry 0's `d = 0x01` (byte
`0xC61A2`), so expected returns 1. Input B: `DSB(P+0x54) = 2` and entry 0's
`e = 0x00` (`0xC61A3`) → returns 0. Input C: seed `DSB(0xC61A3) = 1` with
`DSB(P+0x54) = 2` → returns 1 (proves the `e` branch).

### 7.4 `0x3CE58` — validate and drive the reaction

The shipped fixture (char 0, hitbox 0) makes `0x3CE58` call
`0x3AFC4(0, 0x20, &anim)`, so the test must seed the char that selects the
triple: `DSB(P+0x7A) = 0` gives `anim[1] = 0xA3528 + ((0<<6)+0x20)*0x14 =
0xA37A8`, whose bytes are `7c d1 03 00 | 00 00 00 00 | ...` (i.e.
`*(u32*)0xA37A8 = 0x0003D17C`, `*(u32*)(0xA37A8+4) = 0`).

Input A: `DSD(0x1077B0) = P`; `DSB(P+0x7A) = 0`; `DSB(P+0x54) = 0`;
`DSB(P+0x56) = 0`; `DSB(P+0x5F) = 0xFF`; `DSW(0x107D58 + 0) = 8`;
`DSW(0x104B00) = 3` (not 0x21/0x22). Expected: returns 1; `DSW(P+0x84)`
incremented by 1; `DSB(P+0x5F) == 0x20`. **`DSB(P+0x52)` is unchanged** (seed it
`0x55`) and `DSB(P+0x53)` unchanged: the fixture's `*(u32*)(0xA37A8+4) == 0`, so
`0x34E2C` does not take the `+0x52 = 9` arm (§3.6). Input B: `DSB(P+0x56) = 6`
→ returns 0 (the `0x3CE24` gate, `(s8)(dword[slot+0x53] >> 24) <= 5`).
Input C (the `+0x52 = 9` arm): as Input A but overwrite the shipped reaction
field `DSW(0xC619C + 4) = 0x0000` so the reaction is 0, whose table entry reads
`*(u32*)(0xA3528+4) = 0x000C8B80 != 0`. Expected: returns 1;
`DSB(P+0x52) == 9`; `DSB(P+0x53) == 8`; `DSW(P+0x6A)` incremented by 1.

### 7.5 `0x3CD94` — hit-stun immunity

Input: `DSB(P+0x5F) = 0xFF` → returns 1 (the `0xFF` early-out). Input B:
`DSB(P+0x7A) = 0`, `DSB(P+0x5F) = 0`, reaction `c = 0x20` (entry 0): the `c >=
0x20` branch reads `dword[0xA182C + char*0x200 + 0*8 + 4]`; char 0 row 0 is
`00 00 ff 00 ff ff ff 7f` (dword1 = `0x7FFFFFFF`), so bit 0 is 1 → returns 1.
Input C: `DSB(P+0x5F) = 0x20` (row 0x20, all zero) →
`dword[0xA182C + 0x20*8 + 4] == 0` → returns 0. **Input D (the refuted case):**
`DSB(P+0x5F) = 0x0F` returns **1**, not 0 — row 0x0F is
`00 00 ff 00 ff ff ff 7f`, identical to row 0. Only rows `0x20..0x3F` are zero
(the three groups are in §3.5).

### 7.6 `0x11D04` case 7 and `0x11BCC` — the timer exit

Input A: `DSW(DS_000F0A64) = 7`; `DSW(DS_000F0A6A) = 1`;
`DSW(DS_000F0A6C) = 0`; `DSB(DS_00104B15) = 1`;
`DSB(DS_00104B19+2) = 1`. Call `game_state_step()`. Expected:
`DSW(DS_000F0A6A) == 0`; `DSW(DS_000F0A64) == 0`; `DSB(DS_00104B15) == 0`;
`DSB(DS_00104B19+2) == 0`; `0x263F4` did not run.
Input B: `DSW(DS_000F0A6A) = 2` → `DSW(DS_000F0A6A) == 1`, state still 7, arena
ran. (Ported at `flow.c:1354`, so the test guards the transcription rather than
new code.)

### 7.7 `0x3C6A8` — seed/clear a slot

Input: `DSD(0x1077B0) = P`; `DSB(P+0x63) = 1`; `DSB(P+0x7A) = 0`; call
`0x3C6A8(side=0, value=0, i=0)`. Expected: `DSW(0x107D58 + 0) == 0`;
`DSW(0x107E58 + 0) == 0`; `DSW(0x107DD8 + 0) == DSW(0xBFE3C + 0xC) == 3`.

### 7.8 `0x3C600` — per-attack-frame descriptor

Input: `DSB(P+0x63) = 1`; `DSB(P+0x7A) = 0`. Expected: returns
`DSD(0xC619C + 0) == 0x000BFE3C`. Input B: `DSB(P+0x63) = 0` and
`DSW(DSD(DS_00101514) + 0x2D4) = 2` → returns `DSD(0xC619C + 0)` (the sel-2
path); with that word = 0 → the sel-0/4/6 path returns `DSD(0xC6B9C + 0)`.

### 7.9 `0x3CB68` / `0x3C88C` — the 2×32 slot pass (Task 4)

Input: `DSD(0x1077B0) = P`; `DSB(P+0x63) = 1`; `DSB(P+0x7A) = 0`;
`DSW(0x107D58 + 0) = 8`; `DSW(0x107DD8 + 0) = 3`;
`DSD(DS_001014EC) + DSW(actor)*0x20` seeded so `fighter_actor_bit15_clear(0)` is
0 or 1 as needed. Expected: after `0x3C88C`, `DSW(0x107DD8 + 0) == 2` (phase 8
decrement, still >= 1). Input B: `DSW(0x107DD8 + 0) = 0` → the signed new value
`-1 < 1` clears: `DSW(0x107D58 + 0) == 0` and `DSW(0x107DD8 + 0) == 3`.
Input C: `DSW(0x107D58 + 0) = 0`, `0x3C758`'s command word
`DSW(0x1088E0) = 0x1010` set to overlap the frame's input words → the phase
becomes 8 (`DSW(0x107D58 + 0) == 8`); with `DSW(0x1088E0) = 0` it stays 0.

### 7.10 `0x3B134` / `0x47063` (Task 2 reference)

Not new; §5 is the RNG deliverable. The two draw sites to reference are
`0x11AAD` (`rng(7)`) and `0x11AE9` (`rng(6)`), and the seed site is `0x20C62`.

### 7.11 The remaining new chain functions

These 11 functions are in §3.7's "new to the port" table and are not the
hit-detection core of §7.1–§7.9. Each is given its pinned fixture; the four
functions that cannot be pinned are §6.7–§6.9.

**`0x3CE24(side, i)` — the stance/hit-stun gate.** Input: `DSB(P+0x56) = 6`
→ returns 0 (the signed `slot+0x56 <= 5` gate fails; `slot+0x56` is the high byte
of the dword at `slot+0x53`). Input B: `DSB(P+0x56) = 0`, `DSB(P+0x5F) = 0xFF`
→ `0x3CD94` returns 1 → returns 1.

**`0x3CBC4(side)` — reaction variant A.** Input: `DSB(P+0x54) = 2` → returns
`0x16`. Input B: `DSB(P+0x54) = 0`,
`DSW(DS_001088E0 + side*2) = 0x4000` (bit `0x4000` set) → returns `0x14`. The
`0x12`/`0x10` tail routes through `0x1DDF4` (§6.7).

**`0x3CC58(side)` — reaction variant B.** Same shape: `0x17` at
`DSB(P+0x54) = 2`, `0x15` at bit `0x4000`; tail `0x13`/`0x11` via §6.7.

**`0x34D8C(side)` — the `+0x59` palette-flash pair.** Input:
`DSB(0x1078FA) = 2`; `DSD(0x1077B0) = P0`, `DSD(0x107844) = P1`; seed
`DSB(P0+0x59) = 0x55`, `DSB(P1+0x59) = 0x55`. Call `0x34D8C(0)`. Expected:
`DSB(P0+0x59) == 1` and `DSB(P1+0x59) == 0xFF`. Input B: `DSB(0x1078FA) = 1` →
both unchanged.

**`0x34E2C(side, reaction)` — the reaction driver, head.** Input A:
`reaction = 0xFF` → returns with no writes (seed `P+0x5F = 0xAA`; assert it
stays). Input B (the shipped demo fixture, char 0 / reaction 0x20): seed
`DSD(0x1077B0) = P`, `DSB(P+0x7A) = 0`, `DSB(P+0x54) = 0`,
`DSB(0x1078FA) = 1`, `DSW(P+0x84) = 0x10`; call `0x34E2C(0, 0x20)`. The table
entry `0xA3528 + ((0<<6)+0x20)*0x14 = 0xA37A8` reads `*(u32*)0xA37A8 = 0x0003D17C`
and `*(u32*)(0xA37A8+4) = 0`. Expected: `DSW(P+0x84) == 0x11`;
`DSB(P+0x88) == 0`; `DSB(P+0x8A) == 1`; `DSB(P+0x5F) == 0x20`;
`DSB(DS_001088A8) == 0x20`; `DSB(P+0x52)` **unchanged** (seed `0x55`) because the
`+0x52 = 9` arm needs `*(u32*)(0xA37A8+4) != 0`. Input C (the arm): as Input B
but overwrite `DSW(0xC619C + 4) = 0x0000` (reaction 0, `*(u32*)(0xA3528+4) =
0x000C8B80`) → `DSB(P+0x52) == 9`, `DSB(P+0x53) == 8`,
`DSW(P+0x6A) == seed+1`. The tail is §6.9.

**`0x339AC(out, rec)` — the ctx builder from a record.** Input:
`DSD(0x1077B0) = P0`, `DSD(0x107844) = P1`, `DSB(rec+0x51) = 1`. Expected:
`out[0] == 1`, `out[1] == 0`, `out[2] == 0x107844`, `out[3] == 0x1077B0`,
`out[4] == P1`, `out[5] == P0`.

**`0x4CE70(side, reaction)` — the per-character reaction allow-list.** Input:
`DSB(0x1077B0 + 0x7A) = 0` (char 0), `reaction = 0x28` → returns 0. Input B:
char 0, `reaction = 0x20` → returns 1.

**`0x188AC(side, x, y)` — the record-anchor setter.** Input: `side = 0`,
`DSD(0x1077B0) = P`; call with `x = 0x1111`, `y = 0x2222`. Expected:
`DSD(P+0x18) == 0x1111`, `DSD(P+0x1C) == 0x2222` (then `0x186D0(0)` runs).

**`0x188DC(side, x)` — the `+0x2C` anchor setter.** Input: `side = 0`,
`DSD(0x1077B0) = P`, `DSD(P+0x2C) = 0xAA`; call with `x = 0x3333`. Expected:
`DSD(P+0x2C) == 0x3333`. Its `0x18714` tail (the `rec+0x18` write) is §6.7.

**`0x1890C(side, y)` — the `+0x1C` anchor setter.** Input: `side = 0`,
`DSD(0x1077B0) = P`, `DSB(P+0x42) = 0x08` (bit 3, so `0x186D0` latches
`slot+0x30 = rec+0x1C`), `DSB(P+0x41) = 0`, `DSD(P+0x18) = 0xAA`,
`DSD(P+0x1C) = 0x100`; call with `y = 0x60`. Expected: `DSD(P+0x1C) == 0x60`
(`rec+0x1C += y − slot+0x30`, and `slot+0x30` was just latched to `rec+0x1C`).

**`0x32BAC(char)` — the two hit sounds (call-order seam).** `0x2DAE4` is an
unported out-of-scope sound wrapper, so the assertion is a call-order seam.
Input: `char = 0` (EAX), `EBX = 0` → exactly two `0x2DAE4` calls:
`0x2DAE4(0x1C, 1)` then `0x2DAE4(0x1B, 1)` (`EAX = char+1+0x1B`, then
`EBX+0x1B`; EDX = 1). Input B: `EBX = 7` (`EBX+1 > 6`) → one call,
`0x2DAE4(0x1C, 1)`.

---

## 8. The think chain is unexercised (Step 9)

Every writer of `slot+0x64` in the image sets `0xFF` — `0x33CFB` (spawn, ported
`fighter.c:132`), `0x33B00` (case 19 only), `0x3B6B8`, `0x3B985`, `0x3B9D2` —
and the only non-`0xFF` writer `0x2A620` writes an *actor record*, not the slot.
So `0x3B464` returns at `0x3B49F` for both demo fighters and
`fight_command_map` (`0x3B134`) is unreachable from the demo. This is already
recorded at `2026-09-20-demo-fight-derivations.md` §11.4 and the port carries it
as comments through `fighter_think`/`fighter_think_side`/`fighter_command_map`.
Task 1 Step 9 adds one explicit `/* PORT: ... */` marker at the chain's entry
(`port/src/game/fighter.c`, above `fighter_think`) stating the disposition and
that its unit tests are its only evidence; the interactive match owns it. No
code is removed.

---

## 9. Corrections to the cycle-1 motion plan (Step 8)

### 9.1 Amendment 5's capture numbers

Amendment 5 (`docs/superpowers/plans/2026-09-20-demo-fight-motion.md:109-110`,
and the same numbers in the Gate at line 669) states the demo fight begins at
capture **839**, **28** capture frames after the divergence. Those are the
pre-recapture values. The post-recapture values are **836** and **25**
(`README.md:299`, `port/spec/game_flow.md:685-686`), and the `- LOADING -` screen
is at capture **834** (`port/spec/game_flow.md:743-744`). The plan is corrected
to "does not begin until capture 836 (833 is all-black, 834 the `- LOADING -`
screen), 25 capture frames after the divergence", and the Gate at line 669 to
"before the demo fight begins at capture 836".

### 9.2 The front-end window figures

The plan quotes the enforced front-end window as `[557..813]` / 257 frames at
five sites (lines 24, 484, 506, 528, 614). Reality after the recapture is
`[557..810]` / 254 (`README.md:265-266`, `port/spec/game_flow.md:641-649`).
Every one of the five sites is corrected to `[557..810]` / `254`. These are
capture-derived **indices**, not the oracle's claim; the claim
(`0 unexplained`) is unchanged.

---

## 10. Provenance

* Raw bytes: `data/game/C/PRAGE.EXE` (read-only), 32-bit `capstone` over the code
  object at file offset `VA + 0x52E54` for the `E8` scans; data operands from
  Ghidra (`project rage`, `/PRAGE.EXE`), whose address == linear address.
* Decompilation and function extents: Ghidra (`body_start`/`body_end`); the
  decompiler quotations for `0x3CF38`, `0x3CD44`, `0x3CCEC`, `0x3CD94`,
  `0x3CE24`, `0x3CE58`, `0x3C6A8`, `0x3C600`, `0x34E2C`, `0x3C4CC`, `0x3C520`,
  `0x3C480`, `0x339AC`, `0x4CE70`, `0x1DDF4`, `0x32BAC`, `0x3C88C`, `0x3C758`,
  `0x3C800`, `0x38730`, `0x4F200`, `0x4F228`, `0x255CC`, `0x5D7DC`, `0x39040`.
* Port cross-references: `port/src/game/fighter.c` (the think chain and the
  `+0x52` handlers), `port/src/game/fight.c` (`fight_slot_pass`, the arena frame,
  the HUD spine), `port/src/game/flow.c` (state 6/7/9, the master loop),
  `port/src/game/effects.c` (`0x13C70`/`0x134C0`/`0x38730`'s callers),
  `port/src/game/rng.c` (`0x5D7DC`).
* Prior derivations used: `2026-09-20-demo-fight-derivations.md` §3.5, §5.9, §6,
  §7, §10 (the think chain's death, the spawn chain, the `0x20DF4` gap, the
  `0x34B14` table), `2026-09-20-frontend-chain-derivations.md` §5 (the camera),
  `2026-09-21-demo-fight-closure-design.md` (the starting state and the risks).
* This record is the delivery of Task 1 of
  `docs/superpowers/plans/2026-09-21-demo-fight-closure.md`. The §0.3 corrections
  are against the plan/brief; the named gaps are in §6; the RNG verdict is §5.3.
