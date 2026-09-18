# Design: Primal Rage (DOS) → SDL3 port — sub-project 4a-ii: the actor system and the title

Sub-project 4a-ii of the decomposition. This is the second half of what began as
"4a, the actor system"; brainstorming re-sliced it when exploration showed the
draw path is a three-stage pipeline. **4a-i, the sprite compositor, is specified
separately** in `2026-09-17-sprite-compositor-design.md` and is a prerequisite:
it delivers the display node, the span renderers and `render_list()`. 4a-ii
covers everything above it — the actor pool, the pset sync, the
animation-stream interpreter, the title state, and the pinned pixel-exact
oracle.

Sub-projects 1 (engine core), 2a (audio/AIL) and 2b-i (Smacker video) are
complete and merged on `main` (`2761e55`).

## 1. Context, and what this cycle inherits

The actor system is the cluster the engine-core report listed under
"sub-project 4" but never enumerated: the pool at `0x29xxx`–`0x2Bxxx`, the pset
sync, and the animation interpreter. It has **168 callers** on `0x2AE14` alone
and is shared by the menus, the attract cycle and the fight, which is why it is
the right thing to build before any of them.

Exploration established the full pipeline, and this cycle owns three of the four
stages:

| Stage | Function | Owned by |
|---|---|---|
| Actor update + pset sync | `0x2A31C` → `0x2A1FC` → `0x2A820` | **4a-ii** |
| Animation → sprite id | `0x2A408`, `0x29F34` | **4a-ii** |
| List ordering | `0x1C3FC` / `0x1C3A0` | 4a-i |
| Composite → pixels | `0x14328` → `0x51E5C` → renderers | 4a-i |

Corrections that landed during exploration and change the design:

* **`DAT_000BBDC8` is static, not runtime-populated.** `flow.c:333-337` claims
  it is zero in `PRAGE.EXE` and filled at runtime. It is a static table in the
  data object, stride 12 bytes, `byte 0` = case selector, `dword +4` = resource
  handle. This makes the sound-table id → resource mapping **extractable**, not
  unknown.
* **The title's `FUN_0002C3FC(0x41)`/`(0x43)` are case 5 — voice cancels, not a
  music request.** 2a's report and `flow.c:233-241` assert they are a case-1
  title-music request; both table records are case 5, and the handles
  `0x383B6F4`/`0x3837440` point into `s16title.gra`. 2a's *behaviour* still
  passed its 9340-write oracle, so this invalidates the stated **rationale**,
  not the result. `flow.c`'s comment must be corrected in this cycle.
* **The title's sprites come from `S16ATTRC.GRA` (index 8), not
  `S16TITLE.GRA` (index 7).** Ids `0x2C11…` resolve through the static table
  `DAT_000A8B30` to resource 8. The port's `TITLE_RES 7` was a choice made for
  the fake title and applies only to the bank the old fake read.
* **`0x2BAF4` is the state-begin reset, not an actor draw** — it zeroes both
  process masks, rebuilds the actor pool's free-list, and seeds the back buffer.
* **The sub-palette rule is solved** (4a-i): it is a per-pixel index offset
  `+ (bank - 1)`. The first draft listed it as an open item and gave
  `sprite.c` a `pal_map[256]` interface; both were wrong.
* **The pset `+0x0C` "scale" is not consumed by the display path.** The only
  scaling is the fixed 320/336, 200/240 position projection in `0x14328`, owned
  by 4a-i. The disputed 4.12-vs-8.8 question is therefore moot for rendering.

Decisions taken during brainstorming, still binding:

| # | Decision |
|---|---|
| 1 | **DoD = the actor system + the real title composite, pixel-exact.** |
| 2 | **The RNG is pinned at the title boundary** by a capture-only `PRAGE.EXE` patch; the port re-seeds at state-1 entry and consumes one draw per master-loop iteration. |
| 3 | **Full actor core**, deferring only ops the title provably never reaches, each named in a `/* PORT: */` comment. |
| 4 | **Units = `game/rng.{c,h}` and `game/actors.{c,h}`**; the compositor units belong to 4a-i. |

## 2. Scope

### In scope

* **The RNG**: `0x5D7DC` LCG, seed `0xABCD` from `0x20C10`, consumed once per
  master-loop iteration where the original consumes it unconditionally.
* **The actor pool**: `DAT_001014F4`, 0x68-byte records, `0xEBA0` bytes (580
  records), free-list sentinels `DAT_00105B3C`/`DAT_00105B40`, alloc `0x2AC80`,
  free `0x249C0`, splice helpers `0x249B0`/`0x249D0`.
* **Spawn** `0x2AE14` (825 B, 168 callers) and its support (`0x2AD40`,
  `0x2A820`, `0x29DB8`, `0x2EA30`).
* **State-begin reset** `0x2BAF4`.
* **The pset sync**: `0x2A31C` walking the active list `DAT_00105BCC`, calling
  `0x2A1FC` → `0x2A820`, which writes pset position, layer and frame id.
* **The animation-stream interpreter**: `0x2A408` (literal sprite id from the
  stream, `word & 0x7FFF`) and `0x29F34` (computed ids), plus
  `0x2A690`/`0x2A1FC`.
* **The spawn emit path's call into 4a-i's display-list insert** — `0x2AE14`
  allocates a pset slot, and when the per-type render check
  (`DAT_000BB9DC[type*0xC]`) says visible it calls 4a-i's node alloc + sorted
  insert with `node->pset`. The node pool, the insert primitive and the sort
  pass all belong to 4a-i; this cycle only calls them.
* **The title state** `0x121A0` transcribed, replacing the port's fake
  (`TITLE_FRAMES`, full-screen `S16TITLE` frames, `s_title_ready`).
* **The oracle**: the capture-only patch, the title capture, `make title-oracle`,
  and the pixel-exact comparison.
* The `flow.c` correction for the `DAT_000BBDC8` comment and the title-music
  rationale.

### Non-goals

* The compositor: display node, span renderers, `render_list`, projection,
  clipping, the list sort. Delivered by 4a-i.
* States 2/3/4, the `0x11000` attract sub-machine, the menu input poll `0x11F28`
  (4b).
* EEPROM/`CMOS` and high scores (4c).
* Fonts, text rendering, `ENGLISH.TXT`, the string table (4d).
* `0x2BF08` — deferred on the §7 hypothesis, with a named fallback.
* `0x10DB0`/`0x10E18` — deferred with evidence (input-edge gated).
* Fight-only actor ops, each `/* PORT: */`-named at its site (sub-project 5).
* The tick interrupt vector (sub-project 1 carry-over).

## 3. Definition of done

1. The port reaches state 1 and renders the real `0x121A0` composite — no faked
   frames, no `TITLE_FRAMES`.
2. **Pixel-exact** against the captured original title, frame for frame, over
   the pinned window, at **zero tolerance**. The window is every frame from
   state-1 entry until `DAT_000F0A66` reaches `0x11` (`0x600 / 0x10` = 96
   frames), not a sample.
3. A **reproducibility gate**: two captures under the same pin must reproduce
   byte-identically, else the oracle is void and the pin is revisited.
4. `make verify` green, including an LCG unit test against a table computed from
   `state = state*-0x46F2ED47 + 0x38CE051F` seeded `0xABCD`.
5. The capture-only patch is confined to `tools/`, its rung recorded in the
   cycle report, and `data/` untouched.

## 4. Evidence the plan builds on

### 4.1 The RNG is deterministic in source but not in time

`0x20C10` seeds it with a hardcoded `0xABCD` (`prage.c:11428`). `0x5D7DC`:

```c
DAT_000ef6d8 = DAT_000ef6d8 * -0x46F2ED47 + 0x38CE051F;
return (DAT_000ef6d8 >> 0x10) * (param_1 & 0xffff) >> 0x10;   /* rand() % range */
```

But `0x255CC` consumes it once per frame **plus an unbounded spin**:

```c
FUN_0005d7dc();                                  /* one unconditional call */
DAT_0010150c = DAT_0010150c + 1;
while (DAT_0010150c + -1 == DAT_00101508) {      /* wait for the 60 Hz tick */
  FUN_0005d7dc();                                /* timing-dependent count */
}
```

`0x121A0` draws three values on entry to set the title logo's start X, speed and
direction, so the original's own title varies run-to-run and the RNG state at
entry depends on the spin count. Hence the pin; it is not a port defect.

### 4.2 `0x121A0` — the title state, transcribed

Phase `DAT_000F0A6F == 0`, in order:
1. `FUN_0002C3FC(0x41)`, `FUN_0002C3FC(0x43)` — case 5 voice cancels.
2. `FUN_0004F1E4()`; `FUN_0002BAF4()`; `FUN_00038910()`.
3. Mode-1 branch: clear → `FUN_0001C500()` + `FUN_0002F198()`; set →
   `FUN_0002AE14(desc 0x9AE3C)`.
4. Four `FUN_00038B18(0x9AC1C, …)` with `(edx,ebx)` = `(0,0)`, `(0x2A,0)`,
   `(0,0x1E)`, `(0x2A,0x1E)`.
5. Three `FUN_0005D7DC` draws: `iVar1 = rand(0x5A)`; `iVar2 = rand(0x7E)*0x40 +
   0x280`; `iVar3 = rand(2)`, and if nonzero `iVar2 = -iVar2`.
6. `DAT_00107A50 = iVar2/2 + 0x1500`.
7. Spawn `0x9AC30` → `DAT_000F0A58`; `DAT_000F0A66 = 0x600`;
   `rec58+0x34 = -iVar2/0x5F`; `rec58+0x2C = 0xAA`;
   `rec58+0x36 = (iVar1 << 6)/0x5F`.
8. Spawn `0x9AC94` → `DAT_000F0A54`; `DAT_000F0A6F++`.

Phase `DAT_000F0A6F < 2`:
1. `DAT_000F0A66 -= 0x10`.
2. If `< 0x11`: `0x1C500`, `0x2F280`, `0x2B150(rec58)`, `0x2B150(rec54)`, spawn
   `0x9ACA8` → `0x000F0A54`, then walk `0x33904` calling `0x13C70` on nodes whose
   type pointer is `&DAT_0003E688`, and `DAT_000F0A6F++`.
3. `DAT_00107A50 += (rec58+0x32 >> 16) / 2`.
4. `rec58+0x2C = 0x40000 / DAT_000F0A66`.

Exit (`DAT_000F0A6F == 2 && DAT_0009AF3D == 0`): `DAT_000F0A6F = 0`;
`DAT_000F0A64 = 2`.

Always: `DAT_00107A3A = DAT_00107A50 >> 5`.

Descriptor `0x9AC30` = `{dword ptr 0x0E9116; byte+4 0x00; byte+5 0x01;
word+6 0x0000; word+8 0x2000; word+10 0x0080; word+12 0x0010;
dword+16 0x04197C6C}`; `0x9AC94` = `{0x0E897A, 0, 0x10, 0, 0x1000,
0x419776C}`; `0x9AC1C` = `{0x2BEF, …, word+12 0x1000, dword+16 0x3E688}`.
`dword+16` is the palette handle (res 8); `dword+0` is the animation-stream
pointer; `byte+4` indexes the per-type render check table `DAT_000BB9DC`
(stride 0xC).

### 4.3 `0x2AE14` — spawn (input: `eax` = descriptor ptr, plus register args)

Descriptor fields, verified against four real descriptors:

| desc off | width | record field |
|---|---|---|
| `+0x00` | dword | `+0x08` script/stream pointer |
| `+0x04` | byte | `+0x48` actor type id (indexes `DAT_000BB9DC`) |
| `+0x05` | byte | `+0x20` and `+0x24` as float, then `+0x20 -= 1.0` if nonzero |
| `+0x06` | word | `+0x2E` pset/anim id |
| `+0x08` | word | `+0x28` flags, masked `0xFFC3` |
| `+0x0A` | word | `+0x40` (`<< 6`) |
| `+0x0C` | word | `+0x2C` |
| `+0x10` | dword | nonzero ⇒ `FUN_00033754()` supplies the pset `+0x18` palette |

Record writes: `+0x56` pool index; `+0x08`, `+0x48`, `+0x20/+0x24`, `+0x2A = 0`,
`+0x4A` parent slot, `+0x2C`, `+0x51/+0x55 = 0`, `+0x4B` child slot, `+0x38 = 0`,
`+0x4F` child ref-count, `+0x59`, `+0x4E`, `+0x43`, `+0x44`, `+0x60`, `+0x61`,
`+0x40`, `+0x50`, `+0x52..+0x54`, `+0x34/+0x36 = 0`, `+0x2E`, `+0x5A` layer,
`+0x5F = 1`, `+0x18/+0x1C`, `+0x28`, `+0x2B |= 0x80`, `+0x32` spawn x.
Pset writes (base `DAT_001014EC + index*0x20`): `+0x00` sprite id (`0x1E1` when
the type check returned 2), `+0x02` = `rec+0x2E | (rec+0x5F ? 0x800 : 0)`,
`+0x18` palette handle, `+0x0A`.

`__regparm3` hides register arguments in the decompilation; the exact
EAX/EDX/EBX/ECX binding for args 2–5 is **not** recoverable from the decomp alone.
The plan pins each call site by disassembly rather than guessing (see §9 risk 2).

### 4.4 The pool and free-list

`0x2BAF4` walks `while (v < DAT_001014F4 + 0xEBA0)` freeing each 0x68 record via
`0x249C0`, then resets the sentinels: free `DAT_00105B3C`/`DAT_00105B40`, active
`DAT_00105BCC`/`DAT_00105BD0`. It also zeroes the process masks `DAT_00104AE8`
and `DAT_00104AEC`, and seeds the back buffer from `DAT_000E87A0` (16000
dwords + remainder). `DAT_001014F4` and `DAT_001014EC` are both results of the
`0x1C308` bump allocator, so both pool addresses are runtime values.

Records are free-list members or active-list members; there is no in-record
magic. `+0x28 & 0x08` is the dead bit, set by `0x2B150`.

### 4.5 Pointer-valued globals store `mem[]` offsets

`surface_setup` stores `DSD(DS_001014E4)` into `DSD(DS_000E87A0)` and every
consumer writes `mem + DSD(DS_000E87A0)`. The pool bases follow the same
convention.

## 5. Architecture

### 5.1 `port/src/game/rng.{c,h}` — new

Owns the LCG. State in `mem[]` at `DS_000EF6D8`, per the port's rule that
original engine state lives in `mem[]`.

```c
void rng_seed(u32 s);
u32  rng_next(u32 range);   /* (state>>16)*(range&0xffff)>>16 */
```

### 5.2 `port/src/game/actors.{c,h}` — new

Owns the pool, its free-list, the spawn, the state-begin reset, the pset sync,
the animation-stream interpreter, the record field accessors, and the
display-list node alloc/insert. It does **not** own the sort or the composite
(4a-i) and never touches the framebuffer.

```c
void actors_init(void);             /* res_alloc(0xEBA0); store offset in DS_001014F4 */
void actors_reset(void);            /* 0x2BAF4 */
u32  actor_alloc(void);             /* 0x2AC80 */
void actor_free(u32 off);           /* 0x249C0 */
u32  actor_spawn(const u32 *desc);  /* 0x2AE14; 0 on pool exhaustion */
void actors_update(void);           /* 0x2A31C -> 0x2A1FC -> 0x2A820 */
u32  anim_next_sprite_id(u32 rec);  /* 0x2A408 / 0x29F34 */
```

### 5.3 Changed: `platform/res.{c,h}`

`res.h` gains `u32 res_alloc(u32 size)`, exposing the existing static bump
allocator. One added public function, justified because it mirrors `0x1C308`,
which the port already documents as replaced; the alternative — a fixed pool
address — would invent an address the original does not have.

### 5.4 Changed: `game/flow.c`

* `game_state_title` becomes a transcription of `0x121A0`, keyed on
  `DS_000F0A6F` (replacing `s_title_ready`), calling `actors_reset()` on entry.
* `TITLE_FRAMES`, `s_title_chunks`, `s_title_idx`, `s_title_hold`, `title_load`
  and the fake frame cycling are deleted.
* `game_init` gains `actors_init()` and `rng_seed(0xABCD)` (mirroring `0x20C10`).
* `game_loop` gains `actors_update()` and the one-per-iteration `rng_next()`,
  placed **after present and swap and before `game_audio_service()`**, matching
  the original's position relative to the present and `0x1CF20`. It already
  calls `render_list_sort()` and `render_list()` from 4a-i.
* The `DAT_000BBDC8` comment is corrected (static table, not runtime) and the
  `0x41`/`0x43` comment restated as voice cancels.

## 6. Data flow

1. `game_init`: `actors_init()` reserves the pool; `rng_seed(0xABCD)`.
2. State 1 first frame: `0x121A0` phase 0 → `actors_reset()` → three pinned
   `rng_next` draws → two `actor_spawn`; each spawn emits a pset and inserts a
   display-list node.
3. Every frame: `game_loop` → `input_pump` → `game_frame` (`actors_update`
   syncs records → psets) → `render_list_sort()` → `render_list()` (4a-i) →
   `gfx_flush_palette` → `gfx_present` → `rng_next()` → `game_audio_service`.
4. `0x121A0` phase 1 advances `DAT_000F0A66` by `0x10` per frame and rewrites the
   logo's `+0x2C` from `0x40000 / DAT_000F0A66`.

Ordering is the requirement: on the entry frame the three spawn draws happen
during `game_frame` (before the present), and that frame's unconditional RNG
draw happens after the present. The port keeps that order.

## 7. Error handling and invariants

**No dynamic allocation after init.** The pool is one `res_alloc(0xEBA0)`.
`res_alloc` returning 0 is fatal through the existing `game_fatal`, matching the
original's `0x1D290`; nothing downstream can recover.

**Pool invariants.** `actor_spawn` returns 0 on exhaustion and every caller
tolerates 0, matching `0x2AC80`. `actor_free` validates the offset is inside the
pool and 0x68-aligned before touching the free-list; an invalid offset is
ignored rather than linked, so a transcription bug degrades to a leak instead of
a corrupt heap. `actors_reset` frees every record before rebuilding the list.

**RNG safety.** `rng_next(0)` returns 0 with no undefined shift; a `range` wider
than 16 bits is masked as the original does.

**`0x2BF08` is deferred on a hypothesis, not on inertness.** It *is* called every
frame of state 1 (after `0x121A0`). It early-returns when
`DAT_000EF6DC & 0x1F != 0`; on the other frames — multiples of 32, so 2–3 times
inside the 96-frame window — it runs a string-cursor tick (`0x1C500` → the
`0x474E4` deobfuscator) and pset housekeeping (`0x2F198`/`0x2F280`/`0x2F4BC`).
Hypothesis: none of that reaches `DAT_000E87A4` unless a message is active, and
`DAT_00105C00` is only set from `0x11F28` on menu input. Detectable signature:
the hypothesis is wrong iff the oracle drifts specifically at frames 32, 64, 96
and not before or between. Fallback: absorb `0x2BF08` and the minimal
`0x1C500`/`0x474E4`/`0x1E75C` chain, which drags the string table in from 4d.

**`0x10DB0`/`0x10E18` are deferred with evidence.** Both gate on
`DAT_000F0A71 == 0` and two bits of the input state `DAT_001088D8`, then latch
`DAT_000F0A71`. With no input those bits stay zero and the branches are never
taken — a stronger claim than the `0x2BF08` one, since it assumes nothing about
what the deferred code draws.

**Oracle safety.** The patch script operates on a *copy* under a temporary
directory, never on `data/`; it verifies the original bytes at every patch site
and **fails closed** on mismatch, so a wrong or already-patched binary aborts
rather than producing a bogus oracle.

**The pin, as a two-rung ladder settled in the first plan task.** Rung 1 depends
on the LE load base being addressable from patched code:

1. **Preferred** — insert a re-seed at `0x121A0` entry using the runtime address
   of `DAT_000EF6D8`, and NOP the spin's `call 0x5D7DC`. Both sides then hold
   `0xABCD` at the three draws. 2b showed the base is reproducible (`0x266000`).
2. **Fallback** — if the base is unstable, patch `0x5D7DC` in the capture copy to
   a fixed-return stub and mirror that constant in the port's oracle build.
   Still pixel-exact and identically pinned; the LCG's real behaviour is then
   carried by the unit test instead of the oracle.

Re-seeding makes the three draws independent of everything before it; NOP-ing
the spin plus one draw per iteration makes them independent of everything after.
Either alone is probably sufficient; both together survive the case where an
actor op quietly draws RNG inside the window.

**Invariants**
1. Exactly one `rng_next()` per master-loop iteration; `rng_seed` at title
   entry only.
2. `actors_reset()` runs on state-1 entry before any spawn.
3. Every reachable op is transcribed or a `/* PORT: */`-named stub; no silent
   stub.
4. `make verify` stays green, and the title-oracle gate keeps the
   skip-when-absent / fail-when-required contract 2b established.

## 8. Open items

* **`0x2AE14` register-argument binding** for args 2–5 — not recoverable from the
  decompilation; pinned by disassembly in the plan.
* **`0x29F34`'s computed-id enumeration** — `0x9AC94`'s stream begins `40 CD`
  (bit15 set), so its ids come from `0x29F34` rather than the literal reader.
  Must be transcribed before the second title object can draw.
* **`DS_00107900`'s producer `0x38A38`** (the mode-1 shear ramp) — needed unless
  the title never uses mode 1.
* The record's remaining ~20 fields are named, not implemented, until an op the
  title reaches needs them.
* `0x47370`'s real role: mislabelled "EEPROM read" at `flow.c:490`; it takes
  magic `0x33340001` and drives the `0x1E6D8`/`0x1E75C`/`0x1E808` string-table
  trio. Resolved in 4c.

## 9. Risks

1. **The pin mechanism.** Depends on the load base. Mitigated by the ladder, the
   reproducibility gate, and settling it in the first plan task before actor
   code is written.
2. **`0x2AE14`'s descriptor and argument binding.** Wrong offsets or a wrong
   register mapping show up as an oracle mismatch. Mitigated by pinning each of
   the four real call sites by disassembly rather than inferring from the
   decompilation.
3. **`0x29F34` semantics.** The second title object depends on it; if it is not
   transcribed the oracle fails on that object's frames and the failure is
   localised but ambiguous. Mitigated by extracting it before the spawn task.
4. **`0x2BF08` hypothesis** (§7) could be wrong, pulling the string table in
   mid-cycle. Mitigated by the frame-32/64/96 signature and the named fallback.
5. **Actor field map.** A wrong offset shows up as an oracle mismatch, not a
   crash.

**Explicit acceptance.** The RNG's unpinned interleaving with the spin is not
verified and will not be. The two proofs together cover what matters: the LCG
algorithm and call order by unit test, the composite by oracle, the spin by
design exclusion.

## 10. Testing and verification

**Unit tests** (`port/tests/`, existing `run_tests.c` pattern):

| Test | Proves |
|---|---|
| LCG | N outputs from seed `0xABCD` match the recurrence exactly |
| pool | alloc/free/reuse ordering; exhaustion returns 0; free-list intact after `actors_reset` |
| spawn | field initialisation for a representative descriptor, both negative and positive height |
| pset sync | `0x2A820` writes position and layer from a parent-relative record |
| reset | both process masks zeroed, every record freed, back buffer seeded from `A0` |

**Oracle.** A title capture reusing `smk_capture.py`'s DOSBox-X invocation
(`-defaultconf -fastlaunch`, `DX-CAPTURE /V`, RGB24) with the same
frame-alignment bootstrap 2b used. `make title-oracle` is a thin sibling of 2b's
`smk-oracle`: skip when the capture is absent, fail when `PR_ORACLE_REQUIRED=1`
and absent. Comparison is frame-indexed, pixel-exact, zero tolerance — no
tolerance parameter exists, as in 2b.

**DoD #2 refinement, 2026-09-18 (human-approved).** The literal
"96/96 frames byte-identical, index-for-index" gate was found to be unsatisfiable
against the pinned capture, for a reason outside the port: the original updates
the VGA aperture progressively at `0x255CC` with no retrace wait, while DOSBox-X's
DX-CAPTURE samples at 70.09 Hz against the game's ~60 Hz. Roughly one captured
frame in six is therefore a **tear** — a horizontal band from game frame *N*
above a band from frame *N+1*. Task 10 measured the consequence: the port matches
the capture exactly, whole frame, wherever the capture is clean (54/96 vs one
capture, 53/96 vs a second); the remaining frames have no clean sample at all
(the sampler caught each once, mid-write), and the two independent captures
disagree on precisely the torn samples. A torn capture frame is not a sample of
any game frame, so it cannot witness or falsify anything.

The gate is therefore defined tear-aware, still with **zero pixel tolerance**:
every captured frame must be exactly `port[N][0..b) ++ port[N+1][b..192000)` for some
port frame *N* and splice byte *b* (a clean frame is `b = 0`; the splice is at a byte
offset, not a row boundary, because two measured captures split inside a scanline —
`row 144 = port32[:732] ++ port31[732:]`). Every port frame in the window must be
exhibited by at least one capture (its bytes appear at their correct offsets, as some
frame's prefix or suffix). Both captures must satisfy this independently, and their
clean (`b = 0`) samples must be byte-identical to each other. The splice byte is
derived from the data, never supplied as a tolerance, and no threshold, mask, crop or
frame-skip is permitted. No captured frame may be explained by bytes from
non-adjacent port frames.

**Coverage refinement, 2026-09-18 (human-approved).** At 60 Hz logic against a
70.09 Hz sampler the capture can lose a whole game frame, and the two it loses are the
window's transitions: port frame 0 (boot→title) and frame 95 (title→state 2) are
exhibited by neither capture, being displayed for under one capture interval. The
coverage requirement is therefore: **every port frame in 1..94 must be exhibited by
both captures**, and at most one frame at each end of the window (0 and 95) may be
unexhibited, and only when its adjacent frame is exhibited exactly. Measured: 94/96
exhibited by both, and the two captures' clean samples agree 53/53.



**What each proof covers**
* Unit tests: the LCG and its call order; pool bookkeeping; spawn field mapping;
  pset sync.
* The oracle: the title composite end to end across the pinned window.
* Neither: the RNG's real interleaving with the spin (accepted, §9).

## 11. Stubs consumed and stubs left

Consumed by 4a-ii: the `flow.c` title fake, and the actor-pool/pset/interpreter
gap sub-project 1 left unported. Two sub-project 1 open items close as a side
effect: the actor pool layout, and the per-sprite sub-palette rule (the latter
already closed by 4a-i). The `0x10EE4`/`0x29D60` "transition helpers" comment is
corrected: `0x29D60` is a 1-byte no-op and `0x10EE4` is a state-0 reset, both
belonging to 4b.

Left for later, each named at its site: `0x2BF08` and its `0x1C500`/`0x474E4`
chain (4d, conditionally), `0x10DB0`/`0x10E18` (4b), `0x11F28` (4b), the
`0x11000` attract sub-machine (4b), `CMOS`/EEPROM and high scores (4c),
fonts/text/`ENGLISH.TXT` (4d), and every fight-only actor op (5).

Moved into 4a-i rather than here: the pset/layer structs, the display-list sort
`0x1C3FC`/`0x1C3A0`'s sort pass, the node builder `0x14268`, the blitter
`0x51E5C` and all six renderers, the projection and clipping of `0x14328`, and
the palette-bank offset.
