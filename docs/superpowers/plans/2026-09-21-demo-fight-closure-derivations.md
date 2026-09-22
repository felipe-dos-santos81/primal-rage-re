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

**Correction (Task 3, measured — the raw refutes the state-9 half of the claim
above).** The argument **is** pinned: `i = draw1`, the state-6 RNG draw, from
`0x20DF4`'s own body (`0x20DF7 MOV EBX,EAX`; `0x20E01 CMP EAX,0x7` /
`0x20E06 MOV EBX,0x7`; `0x20E7D MOV EAX,EBX`; `0x20E7F CALL 0x38730`) and its
caller (`0x11AC1 MOV AX,BX`; `0x11AC4 CALL 0x20DF4`, with `BX = draw1` from
`0x11AB2`). But the projection explains the **state-7 background only, not the
state-9 hold frame the oracle window opens on**: `0x38730` cannot have run before
the hold, because state 9 is entered from state 3's phase 1
(`0x1262F`–`0x1264C`, which sets `DS_000F0A6C = 6`) and state 6 runs after it,
and every other `0x20DF4` call site (`0x25A95`, `0x25BE6`, `0x269BE`, `0x270E0`,
`0x295E4`; and `0x20E90`'s caller `0x4256A`) reads `DS_00104AFC`, which only
state 6 writes (`0x11AB4`) — so at the hold the original's `DS_00107A54` is 0 as
well. Measured on the Task 3 tree: a byte diff of the demo dump against the same
tree with the `render_scroll_setup` wiring removed shows the **first changed
frame is dumped 481** (the state-6 entry) and all 900 frames 481..1380 change;
frames 0..480 are byte-identical. The oracle's first-unexplained frame therefore
does not move (capture 816, the hold's globe). The hold's divergence is its
**globe actor animation** — the gap cycle 1 already named at
`port/spec/game_flow.md:790-792` (the `0x3E688` palette-driven zoom-actor globe
render advanced through `0x2A31C`): the port's hold (dumped 240..480) renders 12
distinct images in a 6-frame cycle whose first four steps match the capture's
812..815 byte-exactly, while the capture's globe advances through ~19 distinct
rotation steps before the fight at 834; the port's step 5 (dumped 264) already
differs by 205 bytes and the gap grows. The state-9 frame is **not** a
`DS_00107A54` gap.

### 1.5 The state-9 globe actor's animation (Task 3 continuation)

The hold's divergence is **one animation-interpreter call the port skipped**:
the opcode-0x11 indirect target `0x12720`.

**The draw path.** `0x12658` (state 3's handoff) spawns three presentation actors
— descriptors `0x9AC44`/`0x9AC58`/`0x9AC6C`, layers 0xE0/0xE2/0xE2 — and stores
the first at `DS_000F0A58` (`game_state_3_handoff`). The globe zooms through
state 3's phase-1 tracking (dumped 0..239) and **rotates through the actors'
animation streams during the state-9 hold** (dumped 240..): the rotation is the
three layers' sprite ids advancing together, one step per 6 frames
(`rec+0x24 = 6.0`; the descriptor `dp[5]` hold is set by the stream). Layer 1
walks the 13-word table at `0x0E89C0` (`0x20F..0x21A`), layer 2 reads
`0x021B + rec+0x52` (`0xE89DA`), layer 3 `0x0228 + rec+0x52` (`0xE89E8`).

**The missing call.** Layer 1's stream dispatches opcode 0x11 at `0xE89A8`: the
word there is `0xD100` (opcode 0x11, mode 0x4000), so `anim_operand` loads the
code pointer `0x12720` (the dword at `0xE89AA`) into `DS_00105BD4` and
`0x2B57F`'s `call dword[0x105BD4]` reaches it. `0x12720` spawns the globe's
**fourth layer** — a child of `DS_000F0A58` (`a5 = its pset slot | 0x400`,
descriptor `0x9AC80`: stream `0x0E89F6`, frame hold 7, layer 0xE2, ids
`0x235 + rec+0x52`). The port's `actors_init` registered only `0x10FA8`, so
`anim_indirect`'s `fn_resolve(0x12720)` returned NULL and the fourth layer never
spawned; the port's composite then froze at dumped 312 while the capture's
rotation ran to capture 830.

**Size (Step 1's gate).** One function, **44 bytes** of original code
(`0x12720`..`0x1274B`, the `RET` at `0x1274B`; raw extent confirmed by
disassembly), plus its `fn_register` line — not a subsystem. (The first
statement of this paragraph said 18 bytes / `0x12720..0x1273B`; the raw's
`CALL 0x2AE14` at `0x12743` and the pops/RET at `0x12748`..`0x1274B` refute
that. Task 4's correction, per the ledger's parked minor.)

**Measured.** With the target registered, the demo dump's first changed frame is
dumped **264** — exactly the frame the capture's 816 lands on — and the
capture's globe frames 816..822 and 824..830 now match port frames exactly
(816 == 264, 817 == 270, 819 == 277..281, 824 == 291, 829 == 306..311,
830 == 312..339; 823 is a splice). The demo oracle's first-unexplained frame
advances **816 → 832**, and the next divergence is the `- LOADING -` screen the
spec already names (`port/spec/game_flow.md`, "the `- LOADING -` screen the
capture shows at capture 834, which the port does not draw") — not the
state-7/dust gaps.

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
  `while (DS_0010150C - 1 == DS_00101508) { 0x256D6 rng_step(); }`
  (`0x256C6`–`0x256DB`).

`DS_00101508` (`0x256CC CMP EAX,[0x101508]`) is the VBlank tick counter
(ISR-incremented); `DS_0010150C` (`0x256BB MOV EAX,[0x10150C]`) is the loop
counter. The spin runs a **host-timed, unbounded** number of times per
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

### 5.4 Task 2's outcome (the human's ruling: Option 1 — fix the driver and the port)

The human ruled Option 1. The master-loop pin landed (both draws at
`0x256B1`/`0x256D6` → non-advancing `mov eax,0`; the port's `game_loop` body
draw removed; `tools/title_pin.py`, commit `53b4cef`) and the reference became
deterministic — but the two streams still did not meet, because §5.2 counted
only the master loop's draws and **two deterministic sources were missing**:

1. **The attract's 26 draws.** The attract's voice tick `0x10F28` (called from
   `0x11559` while `DS_0009AD58 == 0`, set at `0x11092`) draws `rng(0x2D)` /
   `rng(2)` / `rng(0x3C)` at `0x10F4D`/`0x10F76`/`0x10F95` before the title.
   The title's three draws are pinned to constants (no advance) and the only
   draw site reachable from states 2..5 is the already-pinned opcode-8 handler
   (`0x2B2A0`), so the reference's state-6 entry is the seed advanced by the
   attract's draws. The port's own attract reaches the `attract_step` case 0xB
   handoff with `DS_000EF6D8 == 0x4308698B` — seed `0xABCD` advanced exactly
   **26** steps (measured; the attract oracle shows the capture's attract is
   that same run). The front-end driver (`test_frontend.c`) enters at state 2
   and skipped them; it now re-seeds to that state
   (`FRONTEND_RNG_AFTER_ATTRACT`), the same pattern the title driver uses for
   the pinned title. The capture stays valid — **no third re-capture**.
2. **The dust builder's draws.** `fighter_spawn(0)` (`0x33EB4` → `0x33C78`,
   gate `DS_00104B14 == 0`) calls the dust builder `0x494A8`, whose loop
   (`0x49540`/`0x4967F`, bound `slot+0x81`) draws **three** values per
   iteration — `0x49388`'s unconditional draw (`0x493AB`), `rng(0x1800)`
   (`0x495DF`) and `rng(step)` (`0x495FC`) — between `0x11AAD` and `0x11AE9`.
   The demo's `slot+0x81` is 2 (0x49300 seeds `DS_001088CC = 2`,
   `DS_000C9520` divides `slot+0x3C = 0`), so **six** draws sit between the
   picks. The port's `fighter_spawn_slot` skipped the whole builder;
   `fight_dust_build` (`port/src/game/fight.c`) now ports it — the entry
   traffic, the `0x49388`/`0x29CDC`/`0x496AC` helpers and the `0x2AE14` actor
   spawn. (§10.5 of the cycle-1 record said "loops `n` times … two RNG values
   per iteration"; the raw wins: the bound is `slot+0x81` and `0x49388` draws
   once per iteration. Corrected there.)

With both fixed the capture's picks are reproduced exactly: at seed+26,
`rng(7)` = 0; after the six dust draws `rng(6)` = 3, so the characters are
`0xC835A[0] = 0` and `0xC835A[3] = 3` — the re-captured demo's fighters, and now
the port's driver's too (verified frame-for-frame against the capture). The
master-loop spin was real (it is what made the reference nondeterministic) but
it was **not the only offset**.

**Residual (named gap).** The dust entries' type-0 processing — `0x49C78`'s
default arm → `0x4AAD0` and its callees — is still unported (its transitive
closure is ~189 functions / ~27 KB including CRT stubs; the demo-relevant
subset is the 0x4Bxxx dust behaviour). The dust's *actor* is spawned and
rendered: the aligned stream's picks are `0xC9524` indices 0, 1, 3, 4 → the
descriptors `0xBB470`/`0xBB484`/`0xBB4AC`/`0xBB4C0`, types
`0x20`/`0x21`/`0x23`/`0x24`, whose per-type callbacks at `0xBB9DC + type*0xC`
are all the `0x5D812` stub. Its motion and despawn are not. The demo window's
first unexplained frame is still **816** (the state-9 hold render), so the
oracle cannot measure the dust's pixels until Task 3 lands.

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
10. **`0x12C70` — the camera-x step seed at the state-6 entry (Task 4's
    follow-up, the arena-render task's input).** `0x20DF4` calls `0x12C70` at
    `0x20E6A`, whose whole body is `MOV word ptr [0x000F0AFC],0x400` (`0x12C70`;
    `RET` at `0x12C79`; sole caller `0x20DF4`, verified by xref). The port does
    not call it, so `DS_000F0AFC` stays BSS 0 until something else writes it;
    `camera_x_commit` (`port/src/game/camera.c:200-207`) reads it as the step
    (`s32 step = (s32)DSW(DS_000F0AFC)`), so the fight's camera-x step starts at
    0 instead of 0x400. With step 0 and a nonzero delta the commit takes the
    `mag > step` arm, adds ±0 and never reaches the `else` that re-seeds the step
    to 0x400 — so on the mode-1 path the camera x cannot move and the
    projection's shear stride (`DS_000F0AF0 << 8`, `render_scroll_fill`) cannot
    grow. Scope note: the camera *mode* is the byte at `DS_000F0AFE`, which this
    reset does not write; the port's measured mode is 0 (`camera_mode_track_player`,
    which writes `DS_000F0AF0` directly) on the first state-7 frames, where the
    measured `DS_000F0AF0` is 0 — so this gap is a named input for the arena task,
    not a proven cause of the port's current camera value. The same reset's two
    word stores `0x20E5C` (`word[0xF0AFA] = CX = 0`) and `0x20E63`
    (`word[0xF0AF8] = SI = 0`) are also unported (both are BSS-zero, so
    net-faithful today). Do not port here — the arena render's fidelity owns the
    camera. Evidence: `0x20DF4` disassembly, `0x12C70` (xref: exactly one caller,
    `0x20E6A` in `FUN_00020df4`), `camera.c:197-210`.

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

### 9.3 Task 4's derivation — what capture 832..836 actually are (raw wins)

The closure plan's Task 4 premise ("cycle 1 recorded capture 836 as all-black
and 837 as the `- LOADING -` screen, so 832..836 is the fight's entry") is
**refuted by the re-captured frontend capture**. The distinct-frame profile of
the current `data/title-captures/frontend` is:

| capture | content |
|---|---|
| 830 | the state-9 hold's globe (port 312..339 match byte-exactly) |
| 831 | **all zero** (the oracle drops it as an artifact) |
| 832 | black except rows **192..197**, x 0.. (`- LOADING -`), 498 non-zero bytes |
| 833 | rows 0..94 black; rows 95..199 the arena (the fighters differ from 834) |
| 834..836 | the arena frame: beach, both fighters, `NO ANIMALS...`, `CREDITS : 5` |

So **833..836 are the fight's first frames, not "the entry"**: only 832 is
pre-fight. 833 is a capture-time tear — rows 141..199 are byte-identical to
834's, rows 95..140 (the fighters) differ — i.e. the top band is the previous
frame's black and the rest is the arena frame the 70.09 Hz sampler caught
mid-scanout. Explaining 833..836 therefore requires the **arena render's pixel
fidelity**, which is Task 5's "state-7 arena render" gap, not an entry screen.

**831 = `0x2BAF4(1)`'s blackout.** The state-6 frame's `0x20DF4` calls
`0x2BAF4` (`0x20E73 mov eax,1` / `0x20E78 call 0x2baf4`) in the same EDX branch
as `0x38730`. `0x2BAF4`'s param_1 != 0 arm runs `0x52106` — which waits on
`in 0x3DA` and writes 256 × 3 zero bytes to the DAC ports `0x3C8`/`0x3C9` —
and `0x336C0` (the palette-list reset). The frame is drawn but the DAC is black,
so the capture's 831 is all zero. (The port's `actors_reset()` ports that arm;
`make demo-oracle` drops all-black capture frames, so the oracle never requires
this frame.)

**832 = the lazy resource loader's `- LOADING -` screen.** `0x1B544` (the
handle→pointer resolver, ported as `res_resolve`) calls `0x1B3AC` when the
entry's `+0xc` lacks the resident bit `0x1000000`. `0x1B3AC`'s head:

```
0x1B3DA test cl,cl ; jz 0x1B3F4        ; CL = BL = 1 from 0x1B5E0/0x1B5E7
0x1B3DE mov eax,0x1e9                  ; localisation string 489
0x1B3E3 mov ebx,0xe6                   ; y = (0xE6*0xD56+0x800)>>12 = 192
0x1B3E8 xor edx,edx                    ; x = 0
0x1B3EA call 0x1c500                   ; the string reader (game_string_get)
0x1B3EF call 0x1c65c                   ; draw it: 0x1C5E8 per glyph + 0x1C470
0x1B3F8 mov [0x1014fc],dl              ; the master loop's full-copy flag
```

String 489 decodes from `ENGLISH.TXT` to `- LOADING -` (id 489 = 0x1E9;
`0x474E4`'s length-XOR). `0x1C65C` draws at `(param_2*0xF3D+0x800)>>12`,
`(EBX*0xD56+0x800)>>12` = (0, 192) via `0x1C5E8`, which reads the **same font
table** `0xBCD7C` the port's `text_glyph_emit` uses but blits directly through
`0x51ED8` (no glyph actor) and calls `0x1C470`'s DAC flush. Measured on the
capture: 832's only content is rows 192..197 — the arithmetic's y.

**The trigger is real in the port's own frame order.** A temporary log of the
first resolve of each resource index (a debug build, not committed) shows the
fight's resources are first resolved at loop frame 1071 = the first state-7
frame: `s16beach` (21), `s16trb` (32), `s16cob` (33), `s16cobsh` (34),
`s16rex` (55), `s16rexsh` (57), `s16statu` (2). So the port resolves them at
exactly the frame the original would load them; what it lacks is the
*presentation*: `res_load_index` reads every INDEX resource eagerly at init
(`res.c:94-106`), so `res_resolve` never loads and never draws.

**The state-6 branch is three calls, and the port had two.** `0x20DF4`'s
EDX != 0 block (`0x20E73`..`0x20E8A`) runs `0x2BAF4` (0x20E78), `0x38730`
(0x20E7F) and `0x412A0` (0x20E86). The port ports `0x38730`; Task 4 added
`0x2BAF4` and this record names `0x412A0` a gap. The reset's eight pre-branch
calls and its two word stores (`0x20E5C`/`0x20E63`) are a separate gap; §6.10
names `0x12C70` (`word[0xF0AFC] = 0x400`) because the arena render's camera step
reads it (`camera.c:200-207`):

* `0x412A0(i)` (76 B, `0x412A0..0x412EC`) spawns the scene's props: for each
  12-byte triple in `0xC82CC[i]` (scene 0 = `0xC7F78`; 5 entries, the terminator
  a zero first dword) it calls `0x2AE14(desc=[esi], a2=[esi+4], a3=0,
  a4=(s32)[esi+6]>>16, a5=0)`, then `0x2C320(i)`. Scene 0's descriptors are
  sprite ids `0x2EF..0x2F3` = `0xA8B30` entries resolving to `s16beach`
  descriptor indices 1, 0, 2, **4 (the temple/columns)**, 3. `0xC7F58[i]` is a
  ret-only/no-op pointer (`0x412EC` is the function's own `RET`, `0x5D812` is
  `return 0`), so it spawns nothing.
* `0x2C320(i)` (101 B by the end-minus-start convention: `0x2C320`..`0x2C385`, the
  `RET` at `0x2C385`) spawns `n = DSW(0xBBD98 + i*2)` crowd actors
  from `0xBBDA8[i]` (12-byte records: a2 `[esi]`, a3 `(s16)[esi+2]`, a4
  `(s16)[esi+4]`, the descriptor `0xBB9D8[[esi+0xa]*3]`, a5
  `([esi+0xb]<<16)|word[esi+8]`) and stores `DS_00105C08`.

The capture's arena content the port lacks — the temple columns and the rocks —
is exactly these props. Measured (a temporary patch, not committed): with
`0x2BAF4` alone the attract's globe and its held text vanish and the arena's
backdrop and both fighters appear, but the port's first arena frame still
differs from capture 834 by **68729 of 192000 RGB bytes (~36 %)**, and the
port's first fighter (char 0, the gold T-rex) renders its torso as a flat
triangle. Those are the Task 5 render gaps.

**Size (Step 1's gate).** The entry is **not one screen**: it is the state-6
branch's `0x412A0`/`0x2C320` (portable: `0x412A0` 76 B — `0x412A0`..`0x412EC`,
the `RET` at `0x412EC` — plus `0x2C320` 101 B — `0x2C320`..`0x2C385`, the `RET`
at `0x2C385`; **177 B** by this record's end-minus-start convention, 179 B
counting both `RET`s — and the six tables `0xC82CC`, `0xC7F78`, `0xC7F58`,
`0xBBD98`, `0xBBDA8`, `0xBB9D8`, all already in `mem[]`; the earlier "~150 B of
code + 4 tables" understated both) plus the **lazy-loader presentation**, which
is a `platform/res.c` rework (residency + on-demand reads + the
`0x1C65C`/`0x1C5E8` direct glyph path), plus the arena render fidelity the
gate's 833..836 need. This is a re-scope, not a silent overrun: reported to the
human with the sizes above.

### 9.4 Task 4's second pass — the loader's presentation, the props, and the frame-order gap

**The loader is flag-driven, not eager.** `0x1B120`'s entry walk
(`0x1B1FA`..`0x1B28E`) loads a resource at init only when the INDEX entry's
`+0xC` dword has bit `0x1000000` set; the other entries are allocated
(`0x1B232`/`0x1B241`) and left unread, to be loaded on their first `0x1B544`
resolve. The shipped INDEX sets that bit for exactly two resources: index 1
`s16fonts.gra` (flag 0x01) and index 2 `s16statu.gra` (0x01); the other 67 carry
0x02. The init path calls `0x1B3AC` with `BL = 0` (`0x1B250 xor ebx,ebx`), so no
LOADING screen is drawn at init; the lazy path (`0x1B5E0 mov ebx,1` /
`0x1B5E7 mov edx,ebx` / `0x1B5E9 call 0x1b3ac`) passes BL = DL = 1, so
`0x1B3AC` draws string 489 at (0,192).

**What the port must model (small, not a subsystem).** (a) the residency state
so `res_resolve` can tell a first resolve from a later one (the port reads every
payload eagerly today, `res.c:94-106`); (b) `0x1B3AC`'s presentation head —
`0x1C500(0x1E9)` + `0x1C65C` + `DS_001014FC = 1`; (c) the glyph path `0x1C5E8`
(the font table `0xBCD7C`, the sprite node builder, `0x51ED8` = the port's
`sprite_blit`, `palette_acquire(0x80997C)`) and `0x1C65C`'s cursor arithmetic
(`(v*0xF3D+0x800)>>12`, `(v*0xD56+0x800)>>12` = (0,192) for `0x1B3AC`'s
(EDX=0, EBX=0xE6)). The port already owns every piece except the residency bit
and the cursor loop.

**Named gap — the load point inside the frame.** The screen must be drawn where
the original draws it, and that is **not** the port's first resolve. The attract
oracle measures capture 215 as the port's frame plus 498 bytes from row 192
(`attract_compare: ... still differs at 498 byte(s) (first row 192 byte
184320)`): the original's text sits **on top of** the frame's content and is
presented for one frame (the swap discards it). The port's first fight resolve
is inside `render_scroll_setup` (state 6, `0x387F4`'s `0x1B544` call at
`0x38814`), *before* the composition, so a draw there is covered by the arena.
Candidates the record could not pin: `0x1CF20`'s per-frame audio service (the
samples are INDEX resources — `s16sound.gra`, index 5 — so a first playback can
trigger `0x1B3AC` after the composition) and the state machine's own late
resolves. Pinning it needs a dosbox-x breakpoint on `0x1B3AC` logging the
caller's return address, or the port's first-resolve order checked against the
capture's frame content.

**The props (`0x412A0`/`0x2C320`) — ported (Task 4).** `0x412A0(scene)` walks
the 12-byte triples of `0xC82CC[scene]` until a zero first dword and spawns
`0x2AE14(desc=[e], a2=[e+4], a3=(s16)[e+8], a4=0, a5=0)`, then `0x2C320(scene)`,
then `0xC7F58[scene]()` — which is a no-op target (`0x412EC` is `0x412A0`'s own
`RET`, `0x5D812` is `xor eax,eax; ret`), so the port does not issue it.
`0x2C320(scene)` spawns `n = DSW(0xBBD98 + scene*2)` records from
`0xBBDA8[scene]`: `desc = 0xBB9D8[[e+0xA]*3]`, `a2 = [e]`, `a3 = (s16)[e+6]`,
`a4 = (s16)[e+4]`, `a5 = ([e+0xB]<<16) | word[e+8]`, and stores the table at
`DS_00105C08`. Scene 0 (`0xC82CC[0] = 0xC7F78`, 5 triples; `0xBBD98[0] = 3`)
issues 8 spawns and the crowd's first actor (descriptor `0xC7850`, whose
`dword0` is the animation stream `0xE8EB2`) runs the animation walk, whose
opcode-`0x0C` targets (the stream words at `0xE8EB2`/`0xE8EBC` are `0xCC01`,
`(word>>8)&0x1F = 0x0C`, dispatched at `0x2B484` via `anim_operand`'s
mode-0x4000 load of `DS_00105BD4`) spawn 2 children from
`0xC7864`/`0xC7878` (a5 = 0x407), so
the active list grows by 10. Opcode `0x11` (`0x2B57F`) only does
`anim_indirect` and spawns nothing on this stream. Scene 2 (`0xC82CC[2] = 0xC7FF0`, 8 triples;
`0xBBD98[2] = 0`) spawns 8 and leaves `DS_00105C08` untouched. The port ports
both in `port/src/game/fight.c`, called from `game_state_6` at the raw's
position (`0x20E86`, after `0x38730`, before `0x41350`); the first rendered
frame changes at dumped 481 (21182 RGB bytes).

**The attract claim conflict.** Landing the LOADING screen necessarily explains
capture 215 (the same 498-byte overlay), so `attract-oracle`'s
`--expect-first 215` would fail ("EXPECTED first divergence at capture frame
215, but the whole attract prefix was explained"). That pin is a known-gap
marker for exactly this task's content, so its update is a human decision; the
loader was therefore derived and NOT landed in this pass.

### 9.5 Task 4's third pass — the load point measured, the overlay landed, the claims re-measured

**The load point, measured in dosbox-x.** The installed `PRAGE.EXE` was run
under DOSBox-X's interactive debugger driven over a pty (this 2026.08.31 build
has no debugger MCP server; the pty carries the debugger's ncurses commands and
its output window text). DOS/4GW relocates the image, so the runtime addresses
were first pinned from a `[dosbox] memory file` dump: the file bytes of code VA
`0x1B5E9` appear at linear `0x20C5E9` (delta `+0x1F1000`, code object base
`0x201000`) and of data VA `0x8002D` ("RAGE.S16") at `0x26602D` (delta
`+0x1E6000`, data object base `0x266000`), so the loader's full-copy flag
`DS_001014FC` is at linear `0x2E74FC`. A linear memory-change breakpoint there
(`BPLM 2E74FC`) hit four times: (1) t≈1.8 s, the init preload's `0x1B3F8`
store (`EBX = 0`); (2) t≈6.8 s, `FUN_0002EA78`'s clear (`0x2EAC9`, the
movie/present path); (3) t≈39.7 s, the first lazy load — the break is inside
`0x1B3AC`'s file I/O with the 12-byte entry name `s16slabs.gra` on its stack;
(4) the master loop's clear. At hit 3 the `SS:ESP` stack (0x200 bytes) gives
the return chain `0x1B408` (inside `0x1B3AC`) ← `0x1B5EE` (`0x1B544`'s lazy
call) ← `0x33761` (`0x33754` palette_acquire) ← `0x2B09C` (`0x2AE14`
actor_spawn) ← `0x38B62` (`0x38B18` frontend_spawn_row) ← … ← `0x11D8D`
(`0x11D04`'s tail) ← `0x2523D` (`0x24C5C`) ← `0x25610` (`0x255CC`). So the
original's first lazy load is a **spawn's palette acquire inside the state
machine, i.e. inside `game_frame` before the render-list composition** — the
same point the port's own resolve order reaches (`palette_acquire` from
`actor_spawn`; the port's first lazy resolves are the attract phase-2
`0x110D8` acquire of index 7 `s16title` and the title phase-0 rows' index 0
`s16slabs` / index 8 `s16attrc`, traced with a temporary log).

**Landed (this pass).** `0x1C5E8`/`0x1C65C` are `text_blit_glyph`/
`text_blit_string` in `port/src/game/actors.c`; `0x1B3AC`'s head
(`0x1B3EA`/`0x1B3EF` + the `0x1B3F8` flag store) is `res_load_present` in
`port/src/platform/res.c`, called with `draw = 1` from the first resolve of a
non-preloaded entry and `draw = 0` from the init preload walk. The entry's
`+0xC` flags keep the original's meanings (`0x1000000` preload, `0x20000000`
read, `0x1B47A`). The port's payloads stay eagerly read (documented in
`res.c`: nothing can observe the read timing) but the *presentation* is
flag-driven, so it fires exactly once per lazy entry. A/B against the
pre-change tree: the overlay changes **exactly the 498 bytes at rows 192..197,
columns 0..85** of the port's first text-bearing frame (dumped 481 in the
demo run) and nothing else.

**The claims did not move (raw/measurement wins over the expectation).**
`make attract-oracle` still reports `FIRST DIVERGENCE at capture frame 215`
with the same 498-byte residual: the port's overlay now lands in the title's
phase-0 frame (`title/frame_0000.raw`), but the oracle compares capture 215
against the *attract* frames, and the port's text-bearing frames carry content
the capture's 215 does not. `make demo-oracle` still reports first unexplained
832: the port's text-bearing frame (481) also carries the arena render, while
capture 832 is black + text. Neither claim is a fitted number:
`--expect-first 215` still holds and is left unchanged.

**Named gap — the attract→title transition's presented DAC/palette state.**
Capture 215 is *black plus exactly the loader's 498 bytes* (nothing else); the
port's title phase-0 frame presents the composed title content (105222
non-black bytes, the loader's pixels covered). This is not the title's render:
the title oracle is green on its own window, and the reviewer's measurement
shows the port's title-frame-0 text region is byte-identical to capture 216's.
It is the *presented DAC state at the frame where the draw lands*, the same
class as the demo's 831/832 at the state-6 entry:

* Port side, measured: a temporary trace at the title dump prints
  `gfx_dac non-black entries=126 first=1 last=127` for `title/frame_0000.raw`
  — the frame-end flush (`0x25672`) has uploaded the frame's acquired palettes
  before the present, so the content is visible.
* Original side, from the capture: at 215 the only visible palette is the
  loader's own range, i.e. only the flush the draw itself runs (`0x1C65C` →
  `0x1C470`, uploading `0x80997C`'s range) had taken effect at the copy.
* Candidate mechanisms, both consistent with 215/216: (a) the original's
  frame-end flush did not upload the frame's other palettes before the copy;
  (b) the original's phase-0 composition drew nothing, leaving the buffer
  black plus the text. The capture cannot separate them (216's text region
  shows content, not the text, so the text was covered or never composed).
* Addresses to start from: `0x52106` (the `0x2BAF4` DAC blackout),
  `0x336C0` (the palette-list reset), `0x1C470` (the draw's own flush),
  `0x2563E` (the master loop's tick gate) / `0x25672` (its flush) / `0x25677`
  (the copy), `0x2EA78` (the flag-checked present path that copies *before*
  flushing), and `0x1223F`/`0x1224F` (the title phase-0 caption and the draw's
  spawn row). **Owner:** the render/palette-fidelity task (the re-scoped Task
  5 / cycle 2's render task), together with the demo's state-6 DAC state.

**Raw correction to the Task 4 brief.** The brief's deferred-minor text says
scene 2's walk-running descriptors carry `word[8] = 0x1200/0x1200/0x1240`.
The raw at `0xC78DC`/`0xC78F0`/`0xC7904` (`data/game/C/PRAGE.EXE` data object)
reads `0x1200/0x1200/0x1200`; the `0x0040` is the *next* word (`+0x0A`, the
extent), so the third value is a transposition. The walk bit is `0x0800` in
`word[8]` (`0x2AE14`'s `>> 8 & 8` test): `0x1200` runs the walk, `0x1A00` and
`0x5A00` skip it.

**A pre-existing dirty-list overflow (fixed, and the bound now measured).**
`palette_record` (`0x33734`) never bounds the head; the original relies on
`0x1C470` draining the list every frame. The test suite drives `game_frame()`
and `effects_step()` without the loop's drain, so `test_effects` already
overflowed the 24 records at `DS_00107498..DS_00107618` before this pass (the
pre-change tree writes 17 records past the list, silently corrupting the
ownership table and above; in the loader-pass tree 4 records reach the
ownership table's start). The new presentation's extra records tipped the
corruption into `DS_001077A8`/`DS_001077B0` and made `fight_health_bars` follow
a wild pointer.

**The faithful path's per-drain bound is measured, not assumed.** A temporary
counter in `palette_record` (reset by both `gfx_flush_palette` and
`palette_list_init`) over the three real drivers gives: **demo run 10 records
between drains** (0 dropped), **attract run 7** (0 dropped), **title run 7**
(0 dropped) — all below the 24 the list holds, so no faithful path reaches the
bound. The test drivers now drain at their fixture boundaries
(`test_effects.c`'s `fixture_begin`, the loop's per-frame `0x25672`), which
removes the overflow at its root: the tests peak at 24 records in one interval
(at the bound, 0 dropped) and the pre-fix 4 drops are gone. `palette_record`
keeps a *loud* drop (`fprintf(stderr, ...)`) as the safety net, and
`gfx_flush_palette` treats an out-of-region head as empty; both guards share
the same head-recovery so an uninitialized head cannot write at `mem[0]`.

### 9.6 Task 5a — the state-6 entry's black frames are the master loop's gate (pinned)

**The gate, measured in the live guest RAM.** DOSBox-X run of the pinned
original with `-set "dosbox memory file=/tmp/prage.mem"`; the data-object base is
recovered per run from `"RAGE.S16"` (data VA `0x8002D`), so
`linear(va) = base + (va − 0x80000)`. Polling every ~4 ms across the state-6
entry (`DS_000F0A64`, the loader flag `DS_001014FC`, the two buffer pointers
`DS_000E87A0`/`DS_000E87A4`, and the tick pair `DS_00101508`/`DS_0010150C`):

| t (s) | state | `1014FC` | `E87A0`/`E87A4` | `1508` | `150C` |
|---|---|---|---|---|---|
| 53.692 | 6 | 0 | `2FB038`/`084410` | 479 | 480 |
| 53.698 | 6 | 0 | `2FB038`/`084410` | **0** | **0** |
| 53.729 | 6 | 1 | `2FB038`/`084410` | 1 | 0 |
| 53.779 | 6 | 1 | `2FB038`/`084410` | 3 | **3** |
| 54.104 | 6 | 1 | `2FB038`/`084410` | 23 | **3** |
| 54.617 | 6 | 1 | `2FB038`/`084410` | 54 | 54 |
| 54.642 | **7** | 1 | `2FB038`/`084410` | 55 | 55 |
| 54.726 | 7 | 1 | `2FB038`/`084410` | 61 | 59 |
| 54.732 | 7 | **0** | **`084410`/`2FB038`** | 61 | 62 |

At 53.698 `0x52106` (via `0x2BAF4` ← `0x20DF4` at `0x20E78`) sets **both**
counters to 0 (`0x52108`/`0x5210D`) and blacks the DAC. The state-6 handler then
spends ~55 timer ticks in its resource loads: `150C` stays at 0..3 while the
timer ISR `0x1BDF4` (`INC EDX`/`MOV [0x101508],EDX` at `0x1BE0E`/`0x1BE10`)
advances `1508` from 0 to ~55. At the gate `CMP EAX,[0x101508]` (`0x25643`) the
two differ, so `FUN_0001c3fc` (sort), `FUN_00014328` (render), `FUN_0001c470`
(flush), the copy (`0x25680`) and `FUN_00050188` (swap) are **all skipped**.
`0x50188` (`0x5018A`-`0x5019B`) is exactly the `E87A0`↔`E87A4` exchange, and the
table above shows it did **not** run for the whole ~1 s: the display holds the
frame presented before the blackout, rendered through the blacked DAC → the
capture's 831 (all black). The copy finally runs at 54.732, when `150C` catches
`1508` (62 vs 61): the capture's 832 onward.

So **the missing gate is the root cause of the 831/832 class**, not the DAC
contents: the previous pass's "the composition ran on the loader frame" and "the
arena palettes were flushed" are both true but irrelevant — on the loader frame
the *present* itself is gated, and the frames that do present after the catch-up
carry the composition. The state-6 frame never reaches the composition.

**The stall is a reproducible per-entry read cost, not an I/O-cache effect (a
correction).** The earlier text here read the second state-6 entry's samples as
`150C` tracking `1508` and concluded the first entry's read was the uncached one.
Re-reading `/tmp/t5a_swap.csv` row by row refutes that: the 118.931 entry blocks
for ~0.94 s with `150C` frozen at 3 for 63 samples and at 34 for 56 samples while
`1508` climbs 0→56 (distribution `0×12, 3×63, 28×4, 29×12, 34×56, 55×3, 56×1`);
the first entry shows the same shape (`0×12, 3×73, 31×3, 32×12, 36×47, 54×1`).
The `3/3`, `28/28`, `55/55` values the earlier text quoted are the handful of
catch-up samples where `150C` momentarily equals `1508`. So the two 9→6 entries
block **55 and 56 ticks** and the 7→6 entry (t≈97.795, the one actually after the
demo's state-7 exit; the 118.931 entry is preceded by state 9 from t≈113.925)
blocks **27**. The stall is repeatable per transition, i.e. a function of the
resources read.

**The stall duration is derivable from the bytes read.** A temporary trace in the
port's `res_resolve` (`PR_T5A_TRACE`) names the entries the state-6 handler
resolves: `s16beach` (233128 B) + `s16rex` (3812084 B) + `s16cob` (2438316 B) =
**6483528 bytes**, which block 55 ticks → **117882 bytes/tick**. The 7→6 entry
resolves a smaller set (the new characters), consistent with its 27. This is a
derived rate, not a fitted per-frame constant; the port's reads are deterministic
(the RNG is seeded), so the same entries are resolved each run.

**What the port needs (the size case).** `port/src/game/flow.c` (`game_loop`,
`1226`-`1246`) has no gate: it sorts, renders, `gfx_flush_palette()`,
`gfx_present()` and `swap_buffers()` every iteration, and its
`DS_00101508`/`DS_0010150C` are written only with 0 and never advanced. The
implementation (done, stashed as `task5a: gate+counter+stall+display-hold WIP`)
adds: the gate around sort/render/flush/copy/swap; `game_loop_begin` for the
loop prologue the per-frame drivers must not repeat; the read stall in
`res_load_present` (`DS_00101508 += bytes / 117882`); and a display-hold buffer
(`gfx_display`) so a gate-failed frame dumps the held frame the screen shows, not
the back buffer `0x52106` just zeroed. It makes the port's state-6 frame black
and the following frames the held/arena frames, and `--check` passes. **But the
dump hooks are per-iteration, so the title window's held frames shift the
oracles:** title moves `54 clean, 55 splice, 2 transition, 0 unexplained` →
`44 clean, 45 splice, 2 transition, 0 unexplained` and `54 clean, 57 splice, 0` →
`44 clean, 47 splice, 0`. The title/attract/frontend/demo windows and the
title/attract claims therefore need re-derivation, and the demo oracle's
`res is None` fallback (title_compare.py `489`-`514`) ignores the port entirely,
so its "first unexplained 832" cannot move until that fallback is fixed. That is
the re-scope: a display-hold/oracle-window pass, not this task.

**Correction (green-first pass, `7e4663a`): the port also needs the loader's
re-sync, and with it the windows do not move.** The gate model as written above
advances `DS_00101508` on a read but never applies `0x1B3AC`'s tail
(`0x1B45F`/`0x1B464`: `DS_0010150C = DS_00101508`), so the gate fails on the
loader frame and the loader's presentation is never dumped. The measurement: the
pre-gate attract dump holds the loader frame at frames 2..5 (498 nz); the
gate-model dump replaces them with the arena (148024 nz), and a per-iteration
trace shows `E87A4` holding the loader frame (`e4nz=166`) while the gate fails
for the read's 13 ticks and `gfx_display()` holds the previous frame. The fix is
one line in `res_load_present` after the stall advance. With it the attract
oracle returns to `FIRST DIVERGENCE at capture frame 215` (was 1) and the title
window to `[216..326]`, 111 frames, `54 clean, 55 splice, 2 transition, 0
unexplained` / `54 clean, 57 splice, 0` (was `[236..326]`, 91, `44/45/2/0` /
`44/47/0`), determinism `54 agree, 0 disagree`; `test_title.c`'s presented count
is 96 again. So the title/attract *claims* never moved and no oracle model was
needed — the `78`-frame window was the gate model's artifact.

**The VGA-retrace ordering (the second named measurement).** `0x1C470` waits for
the VGA status bit before its DAC writes (`0x1C481`-`0x1C489`: `MOV EDX,0x3da` /
`IN AL,DX` / `TEST AL,0x8` / `JZ`) and `0x52106` waits the same way before its
256×3 blackout writes (`0x5212E`-`0x52131`). The copy (`0x25680`, the
`movsd` loop) has no retrace wait. So both DAC operations are retrace-synced and
the copy is not; the ordering across the entry is flush/blackout → copy, and the
retrace sync cannot separate the two candidates (the DAC state at the copy is
simply the last `0x1C470`/`0x52106` write).

**Task 5c — the loader's flush scope and the enqueue order (the presentation
path's mechanism, a named gap).** Task 5c measured the state-6 entry again (the
DOSBox-X live-RAM route of Task 5a, the data base recovered per run from
`"RAGE.S16"`; polled every ~2 ms) and named the mechanism:

* **Candidate (a) — the loader's flush scope/timing — is the mechanism.** The
  original's loader flush (`0x1C470` via `0x1C65C`) drains only the `0x33734`
  initial palette (`002a3470`, `flag=0`) and the `0x80997C` font palette
  (`0080997c`, the loader text's own acquire); the arena's **4** palette records
  (`flag=1`) are enqueued after it and **survive to the gate's `0x25672`
  flush**, which drains them. The port's loader flush drains the arena's records
  instead (`nrec=5`, the `0a838b44`/`0105ff3c`/`00809984`/`0080998c`/`0a838c60`
  enqueues), so its gate flush is **empty** and its DAC already holds the arena
  palette at the loader frame.
* **Candidate (b) is refuted.** The port's `render_list` call is inside the gate
  (`flow.c:1250`, `0x2566D`, matching the raw's `0x25643` gate → `0x2566D`
  render → `0x25672` flush), and the original's render list is **non-empty**
  (`2e7574`) at the gate-pass frame, so the render overwrites `E87A4` in both.
* **The decisive byte comparison.** The port's `mem + DS_000E87A4` after the
  state-6 loader draw is **byte-identical to capture 832** (diff 0, both 498
  non-zero bytes / 166 px), but the gate's own `render_list` overwrites it
  before `gfx_present`, so it is never exhibited.

**The remaining work is a named gap.** Exposing the held frame needs the
original's **non-atomic read** — the state-6 handler blocks in `0x1B3AC` for the
whole ~1 s the poll shows (`150C` frozen at 3 while `1508` climbs 3→54), and the
port's atomic read (`res.c` loads every payload at init) makes the gate always
pass — plus the **palette enqueue order** (`palette_acquire`'s `start = prev
.start + prev.len`, which fixes each palette's DAC range). Modelling the block
needs either a host-timing value (forbidden) or a re-work of the read/gate model
in `res.c`/`flow.c`; scoping the loader's flush would be unfaithful (the raw's
`0x1C470` drains the whole list). That is a **subsystem, deliberately deferred by
the human ruling** ("accept 831/832 as a documented named gap and move to Task
6" is the open branch). The measurement lives in
`.superpowers/sdd/2026-09-21-demo-fight-closure/task-5c-report.md` (that session
reverted its traces and committed no code).

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
