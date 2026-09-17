# Design: Primal Rage (DOS) → SDL3 port — sub-project 4a: the actor system

Sub-project 4a of the decomposition (see `2026-09-16-engine-core-port-design.md`,
"Sub-project split"; and `2026-09-17-smacker-video-design.md` §1 for how 2b was
re-sliced). Sub-projects 1 (engine core), 2a (audio/AIL) and 2b-i (Smacker
video) are complete and merged on `main` (`2761e55`). This spec covers **4a
only**: the engine's actor/sprite task system, proved end-to-end by rendering
the real title screen.

## 1. Context, and the decomposition that shaped this scope

The engine-core design listed sub-project 4 as "Menus / EEPROM: EEPROM read
`0x47370`, menu-input poll `0x11F28`, transitions `0x10EE4`/`0x29D60`,
character-select states 2–4, `0x11000` attract sub-machine". Exploration during
brainstorming found that this is not one subsystem but four, and that the
largest of them is shared with sub-project 5 and entirely unported:

| Slice | What | Depends on | Size |
|---|---|---|---|
| **4a** | Actor/sprite task system | `gra`, `gfx` | Large; **also unblocks 5** |
| 4b | Attract sub-machine `0x11000`, menu states 2/3/4, menu input poll `0x11F28` | 4a, fonts/text/localization | Medium |
| 4c | EEPROM/options (the `CMOS` file), high-score table | nothing | Small |
| 4d | Fonts, `0x2BF08` text render, `ENGLISH.TXT` deobfuscation + string table | `gra`, `gfx` | Small |

The deciding fact: `0x2AE14` (the actor spawn) has **168 callers** and drives
the menus, the attract cycle *and* the fight. The title screen only works in the
port today because sub-project 1 **faked** it — it decodes four full-screen
`S16TITLE.GRA` descriptors and cycles them, instead of running the original's
task-system composite. Any faithful menu, and the fight itself, needs the actor
system first. Brainstorming therefore took 4a ahead of the rest of sub-project 4.

Corrections found during exploration, each load-bearing:

* **`0x29D60` is a red herring.** Decompiled it is `void FUN_00029d60(void) {
  return; }`, size **1 byte**, with 13 callers. Nothing to port. `0x10EE4`
  (66 B) is a reset-to-state-0 helper.
* **EEPROM is a file, not hardware.** `data/game/C/CMOS` is 2040 bytes, all
  zero on a fresh install, and the writer's strings are "attempt to write
  outside eeprom" / "Write extends past end of EEPROM". So 4c needs no device
  emulation.
* **Localization is a real, unported dependency.** `ENGLISH.TXT` …
  `PORTUGAL.TXT` are XOR-obfuscated string tables read through `0x474E4` (a
  rolling XOR keyed by the leading length byte). The current `port/spec/
  game_flow.md` claim that menu strings live in the data object is wrong; every
  menu label comes from these files. This is 4d.
* **`0x47370` is almost certainly mislabelled** at `flow.c:490` as "EEPROM
  read". It takes a magic `0x33340001` and drives the `0x61xxx` runtime plus the
  `0x1E6D8`/`0x1E75C`/`0x1E808` string-table trio. Its real role is deferred to
  4c, not solved here.
* **`0x2BAF4` is the state-begin reset, not the actor draw.** It zeroes both
  process-table masks (`DAT_00104AE8`/`DAT_00104AEC`), rebuilds the actor pool's
  free-list heads (`DAT_00105B3C`/`DAT_00105B40`) while freeing every record via
  `0x249C0`, then seeds the back buffer from `DAT_000E87A0`. The port calls none
  of this today.
* **`0x33904` is not the actor walk.** It is a generic 16-byte-stride free-slot
  finder used by the *palette* dirty-list (bounded by `&UNK_00107797`), 15
  callers, and has nothing to do with the 0x68-byte actor pool.

Decisions taken during brainstorming:

| # | Decision |
|---|---|
| 1 | **Slice = 4a**, the actor system, ahead of 4b/4c/4d. It is the shared prerequisite for menus and fight, and retires the biggest unknown early. |
| 2 | **DoD = the actor system + the real title composite, pixel-exact** against a capture of the original. Smallest slice that yields a capturable, player-visible, verifiable result. |
| 3 | **The RNG is pinned at the title boundary.** The original's own title is run-to-run random (§4), so an unpatched capture is not a well-posed comparison target. |
| 4 | **Scope = the full actor core plus a faithful sprite blit**, deferring only ops the title provably never reaches, each named in a `/* PORT: */` comment. |
| 5 | **Seam = three new units**: `platform/sprite.{c,h}`, `game/actors.{c,h}`, `game/rng.{c,h}`. |

## 2. Scope

### In scope

* **The RNG**: `0x5D7DC` LCG, seed `0xABCD` from `0x20C10`, consumed once per
  master-loop iteration where the original consumes it unconditionally.
* **The actor pool**: `DAT_001014F4`, 0x68-byte records, `0xEBA0` bytes,
  free-list at `DAT_00105B3C`/`DAT_00105B40`; alloc `0x2AC80`, free `0x249C0`.
* **Spawn**: `0x2AE14` (825 B) and its support (`0x2AD40`, `0x2A820`,
  `0x29DB8`, `0x2EA30`).
* **State-begin reset**: `0x2BAF4` — clear both process masks, rebuild the pool
  free-list, seed the back buffer.
* **The draw path**: per-actor frame selection and the scaled / clipped /
  transparent blit with per-sprite sub-palette remap.
* **The pset (pattern-set) layer the pool shares**: the descriptor table at
  `DAT_00105F38` and its select/release path `0x2F0F0`, `0x2F198`, `0x2F280`,
  `0x2F4BC`, `0x2F830`, plus the destroy path `0x2AD40`. Pset entries reference
  actor-pool records through `+0x4B`/`+0x2A`/`+0x4F`, which is why the pool's
  0x68-byte layout carries pset fields.
* **The helpers the title calls at state boundaries**: `0x4F1E4` and `0x4F1D0`
  on entry, `0x13C70` on exit.
* **Replacing the faked title**: `TITLE_FRAMES = {10,12,13,18}` full-screen
  cycling is deleted; `0x121A0`'s two `0x2AE14` tasks and their real composite
  become the title.

### Non-goals

* States 2/3/4, the `0x11000` attract sub-machine, and the menu input poll
  `0x11F28`. State 1 stays entered directly; 4a does not build the attract
  skeleton.
* EEPROM/`CMOS` and the high-score table (4c).
* Fonts, text rendering, `ENGLISH.TXT` and the string table (4d).
* **`0x2BF08` is deferred on a hypothesis, not on inertness.** It *is* reached
  every frame of state 1 (`0x11D04` case 1 calls it after `0x121A0`). It
  early-returns on frames where `DAT_000EF6DC & 0x1F != 0`; on the remaining
  frames — multiples of 32, so 2–3 times inside the ~96-frame title window — it
  runs a string-cursor tick (`0x1C500` → the `0x474E4` deobfuscator) and pset
  housekeeping (`0x2F198`/`0x2F280`/`0x2F4BC`).
  The hypothesis: none of that reaches `DAT_000E87A4` unless a message is
  active, and `DAT_00105C00` (the message counter) is only ever set from
  `0x11F28` on menu input, so a no-input capture never displays one.
  The detectable signature: the hypothesis is wrong iff the oracle drifts
  specifically at frames 32, 64, 96 and not before or between them.
  The fallback: absorb `0x2BF08` and the minimal `0x1C500`/`0x474E4`/
  `0x1E75C` chain into 4a. That drags the string table in from 4d, which is the
  cost this hypothesis avoids.
* `0x10DB0`/`0x10E18` are deferred **with evidence**: both gate on
  `DAT_000F0A71 == 0` and two bits of the input state `DAT_001088D8`, then latch
  `DAT_000F0A71` before transitioning. With no input those bits stay zero, so
  the branches are never taken. This is a stronger claim than the `0x2BF08` one
  because it needs no assumption about what the deferred code draws.
* Fight-only actor ops. Each is named in a `/* PORT: */` comment at the site it
  would be reached from, so 4b/5 find them rather than hitting a silent stub.
* The tick interrupt vector (carried from sub-project 1, untouched here).

## 3. Definition of done

1. The port reaches state 1 and renders the real `0x121A0` composite — no faked
   frames.
2. **Pixel-exact** against a captured original title, frame for frame, over the
   RNG-pinned comparison window, at **zero tolerance**. The comparison window is
   every frame from state-1 entry until the title's own transition at
   `DAT_000F0A66` reaching `0x11`, not a sample.
3. A **reproducibility gate**: two captures under the same pin must reproduce
   byte-identically, else the oracle is void and the pin is revisited.
4. `make verify` green, including a unit test of the LCG against a table
   computed from `state = state*-0x46F2ED47 + 0x38CE051F` seeded `0xABCD`.
5. The capture-only `PRAGE.EXE` patch is confined to `tools/`, its purpose and
   its rung (§4) documented in the cycle report, and `data/` untouched.

## 4. Evidence the plan builds on

### The RNG is deterministic in source but not in time

`0x20C10` seeds it with a hardcoded `0xABCD` (`prage.c:11428`,
`DAT_000ef6d8 = 0xabcd`). `0x5D7DC` is a plain LCG, size 42:

```c
DAT_000ef6d8 = DAT_000ef6d8 * -0x46F2ED47 + 0x38CE051F;
return (DAT_000ef6d8 >> 0x10) * (param_1 & 0xffff) >> 0x10;   /* rand() % range */
```

But the master loop `0x255CC` consumes it **once per frame plus an unbounded
spin**:

```c
FUN_0005d7dc();                                  /* one unconditional call */
DAT_0010150c = DAT_0010150c + 1;
while (DAT_0010150c + -1 == DAT_00101508) {      /* wait for the 60 Hz tick */
  FUN_0005d7dc();                                /* timing-dependent count */
}
```

`0x121A0` draws three RNG values on entry to set the title logo's start X,
speed and direction (`iVar2 = rand()*0x40+0x280`, sign from a third draw). So
the original's own title logo trajectory varies run-to-run, and the RNG state at
title entry depends on how many spin iterations have elapsed since boot. This is
why §3.2 needs a pin; it is not a defect in the port.

### `0x121A0` — the title state (state 1)

* Case 0: two `0x2C3FC` resource requests, `0x4F1E4`, then `0x2BAF4`
  (state-begin reset), `0x38910`, four `0x38B18`, three `0x5D7DC` draws, then two
  `0x2AE14` spawns. `DAT_000F0A66 = 0x600`; per-frame decrement by `0x10`, and
  when it drops below `0x11` the exit path runs.
* Spawn A takes scale at `+0x2C`, velocities at `+0x34`/`+0x36`, position
  `DAT_00107A50 = iVar2/2 + 0x1500`, `DAT_00107A48`.
* The field offsets the port must transcribe for the title: `+0x08` (resolved
  sprite pointer), `+0x20`/`+0x24` (float decrement counters), `+0x2C` (4.12
  scale, `0x1000` == 1.0), `+0x34`/`+0x36` (velocities), `+0x40`, `+0x48`,
  `+0x4A`/`+0x4B` (slot indices), `+0x56` (pool index).

### The actor at 4.12 scale

`0x11000` case 4 walks `+0x2C` down by `0x100` per frame and floors it at
`0x1000` (`if (uVar3 < 0x1001)`), i.e. a zoom that converges on 1.0. The
resampler's rounding direction at non-integer scales must match the original's
or pixels differ; the oracle is the arbiter.

### The pool and the free-list

`0x2BAF4` walks `while (uVar1 < DAT_001014f4 + 0xeba0)` freeing each 0x68-byte
record through `0x249C0`, then resets `DAT_00105B3C`/`DAT_00105B40` to empty
intrusive list heads. `0x2AC80` pops from that list; on success `0x2EA30` is
consulted. `DAT_001014F4` is itself the result of the `0x1C308` bump allocator,
so the pool's location is a runtime value, not a link-time address.

### Pointer-valued globals store `mem[]` offsets

`surface_setup` stores `DSD(DS_001014E4)` into `DSD(DS_000E87A0)` and every
consumer writes `mem + DSD(DS_000E87A0)`. The actor pool base in
`DS_001014F4` follows the same convention.

### Existing port surface this builds on

* `platform/gra.h`: `gra_open` (chunk walk), `gra_decode_palette`,
  `gra_decode_frame` (frame → indices, transparency writes index 0, bounded
  against the blob). 158 lines.
* `platform/gfx.h`: `gfx_flush_palette` (the 4×u32 dirty-list → `gfx_dac[][]`),
  `gfx_present` (indices → RGB24 → host), 85 lines.
* `platform/res.h`: `res_load_index`, `res_load_file`, `res_handle`,
  `res_resolve`. **No exported allocator**: `res_alloc` is static in `res.c`
  and is already documented as the port's replacement for `0x1C308`.
* The back buffer the actor system composites into is `mem + DSD(DS_000E87A4)`,
  the same buffer `gfx_present` already reads.

## 5. Architecture

### `port/src/game/rng.{c,h}`

Owns the LCG and nothing else. State lives in `mem[]` at `DS_000EF6D8`,
preserving the port's rule that original engine state stays in `mem[]`.

```c
void rng_seed(u32 s);
u32  rng_next(u32 range);   /* (state>>16)*(range&0xffff)>>16 */
```

### `port/src/game/actors.{c,h}`

Owns the pool, its free-list, the spawn, the state-begin reset, the field
accessors, and the pset layer — the pset descriptor table manipulates pool
records directly (`+0x4B`/`+0x2A`/`+0x4F`), so it belongs with the pool rather
than in a unit of its own.

```c
void actors_init(void);             /* res_alloc(0xEBA0); store offset in DS_001014F4 */
void actors_reset(void);            /* 0x2BAF4: masks, free-list, seed buffer */
u32  actor_alloc(void);             /* 0x2AC80 */
void actor_free(u32 off);           /* 0x249C0 */
u32  actor_spawn(const u32 *desc);  /* 0x2AE14; 0 on pool exhaustion */
int  pset_select(...);              /* 0x2F198 / 0x2F280 / 0x2F4BC / 0x2F0F0 */
void pset_destroy(...);             /* 0x2AD40 */
```

`actors.c` never sees a GRA container. It holds the same resolved sprite pointer
the original stores at record `+0x08`, exactly as `0x2AE14` copies it from the
descriptor's first dword.

### `port/src/platform/sprite.{c,h}`

Owns the scaled / clipped / transparent blit and the per-sprite palette remap.
It never touches the pool.

```c
void sprite_blit(u8 *dst, int dw, int dh,
                 const u8 *sprite,          /* the resolved sprite */
                 int x, int y, int scale,   /* 4.12; 0x1000 == 1.0 */
                 const u8 *pal_map);        /* NULL == identity; else 256 entries */
```

The blit is its own unit rather than an addition to `gra.c` because `gra.c` is
by its own header comment the `.GRA` container decoder, and a raster scaler is
not container work. `gra` stays a pure decoder; `sprite` stays a pure raster op.

### Changed: `game/flow.c`

* `game_state_title` becomes a transcription of `0x121A0`, keyed on the phase
  counter `DS_000F0A6F` (replacing the port's `s_title_ready`), calling
  `actors_reset()` first.
* `TITLE_FRAMES`, `s_title_chunks`, `s_title_idx`, `s_title_hold` are deleted.
* `game_loop` gains the one-per-iteration `rng_next()` and the title-entry
  `rng_seed`.
* `game_init` gains `actors_init()` and the `0x20C10` `rng_seed(0xABCD)`.

### Changed: `platform/res.{c,h}`

`res.h` gains `u32 res_alloc(u32 size)`, exposing the existing static bump
allocator. This is one added public function, justified because it mirrors
`0x1C308` — the allocator the port already documents as replaced — and because
the alternative (a fixed pool region in `mem[]`) would invent an address the
original does not have.

## 6. Data flow

1. `game_init`: `actors_init()` allocates the pool; `rng_seed(0xABCD)` mirrors
   `0x20C10`.
2. State 1 first frame (`0x121A0` case 0): `actors_reset()` → three pinned
   `rng_next` draws → two `actor_spawn` → sprite handles resolved into the actor
   records.
3. Every frame: `game_loop` → `game_frame` → `game_state_step` →
   `game_state_title`; the draw path composites actors into
   `mem + DSD(DS_000E87A4)`.
4. `gfx_flush_palette()` then `gfx_present()` — unchanged.
5. `rng_next()` — **after present and swap, before `game_audio_service()`**,
   matching the original's position relative to the present and `0x1CF20`. Per
   iteration exactly one, because the capture binary's spin is NOP-ed (§7).

The original's ordering is what the port must mirror: on the entry frame the
three spawn draws happen in `0x24C5C` (before the present), and that frame's
unconditional draw happens after the present. The port keeps that order.

## 7. Error handling and invariants

**Trust boundary.** The only untrusted input is the asset set (`S16TITLE.GRA`
and anything the title's actors resolve). `gra.c` already bounds every RLE run
against the blob and returns −1 rather than over-reading; `sprite.c` inherits
that by consuming a sprite `gra` already validated. Sprite dimensions reaching
`sprite_blit` are checked (`w`/`h` in `(0, 320]`/`(0, 200]`) so a corrupt header
cannot make the blit claim a huge rectangle; a bad sprite is skipped, never
fatal — the posture `movie_play` already takes.

**No dynamic allocation after init.** The pool is one `res_alloc(0xEBA0)` at
`game_init`. `res_alloc` returning 0 is a fatal init error through the existing
`game_fatal`, matching the original's call to `0x1D290`; nothing downstream can
recover.

**Pool invariants.** `actor_spawn` returns 0 on exhaustion and every caller
tolerates 0, matching `0x2AC80`'s failure path. `actor_free` validates that the
offset is inside the pool and 0x68-aligned before touching the free-list; an
invalid offset is ignored rather than linked, so a transcription bug degrades to
a leak instead of corrupting the heap. `actors_reset` frees every record before
rebuilding the list, so state cannot leak across a state change.

**Blit safety.** `sprite_blit` clips on all four edges against `dw`/`dh` and
never writes outside `dst`. `scale <= 0` is rejected. A zero-area destination is
a no-op, not an error. Transparency uses the original's index, so no
uninitialised `dst` byte is ever read.

**RNG safety.** `rng_next(0)` returns 0 without an undefined shift. A `range`
wider than 16 bits is masked like the original, so no caller can provoke UB.

**Oracle safety.** The patch script operates on a *copy* under a temporary
directory, never on `data/`. It verifies the original bytes at every patch site
before writing and **fails closed** on a mismatch, so a wrong or already-patched
binary aborts instead of silently producing a bogus oracle.

**The pin, as a two-rung ladder settled by evidence in the first plan task.**
Rung 1 is not assumed to be certain, because it depends on the LE image's load
base being addressable from patched code:

1. **Preferred** — insert a re-seed at `0x121A0` entry using the runtime address
   of `DAT_000EF6D8`, and NOP the spin's `call 0x5D7DC`. Both sides then hold
   `0xABCD` at the three draws. The 2b work showed the base is reproducible
   (`0x266000`), so this is likely fine.
2. **Fallback** — if the base proves unstable, patch `0x5D7DC` in the capture
   copy to a fixed-return stub and mirror that same constant in the port's
   oracle build. Still pixel-exact and identically pinned on both sides; the
   LCG's real behaviour is then carried by the unit test instead of the oracle.

Either rung satisfies §3. Which one was used is recorded in the cycle report.
Re-seeding at `0x121A0` makes the three draws independent of everything before
it; NOP-ing the spin plus one draw per iteration makes them independent of
everything after. Either alone is probably sufficient; both together survive the
case where an actor op quietly draws RNG inside the comparison window, which
cannot be ruled out until the ops are transcribed.

**Invariants the implementation must preserve**

1. Exactly one `rng_next()` per master-loop iteration; `rng_seed` at title entry
   only.
2. The back buffer is fully seeded each state-begin by `actors_reset()`; a state
   that skips it presents stale pixels.
3. Every reachable op is either transcribed or a `/* PORT: */`-named stub; no
   silent stub.
4. `make verify` stays green and the oracle gate keeps the skip-when-absent /
   fail-when-required contract 2b established.

## 8. Open RE items (carried, not solved here)

* **Per-sprite sub-palette selection — rule unknown.** This is the sub-project 1
  open item. The port currently flattens the type-5 bank to a single 256-entry
  palette, which is known-wrong. The capture arbitrates. If the rule cannot be
  recovered, that is a blocker to declare, not to paper over.
* **`0x2AE14` field layout.** ~30 fields with inferred offsets. Only the title's
  fields are transcribed here; the rest are named, not implemented.
* **The string table / pset entanglement.** `0x2BF08` (reached every frame in
  state 1) pulls `0x1C500` → `0x474E4` → `0x1E75C` for its string-cursor tick,
  and the pset select path `0x2F198` → `0x2F830` does string work too. 4a needs
  the pset table regardless; it does not need the string table *unless* the
  §2 hypothesis fails at the 32-frame boundary.
* **Where the string table is loaded from.** `0x474E4`'s source comes from
  `0x1E75C`, which reads a structure set up by the `0x1E6D8`/`0x1E808` trio —
  the same trio `0x47370` drives. Whether that is `ENGLISH.TXT` or a data-object
  table is unsolved and only matters if the fallback is taken.
* The title's real sprite set and frame indices, pinned by the capture.
* `0x47370`'s real role (see §1); resolved in 4c.
* The tick interrupt vector (sub-project 1 carry-over).
* The blit's full op set — flip, masked and alternate-clipping variants the
  title may not exercise.

## 9. Risks

1. **The pin mechanism.** Depends on the load base (rung 1). Mitigated by the
   ladder, the reproducibility gate, and settling it in the first plan task
   before any actor code is written.
2. **Sub-palette selection unknown.** Gates pixel-exactness; see §8.
3. **`0x2AE14` inferred field offsets.** A wrong offset shows up as an oracle
   mismatch, not a crash.
4. **The `0x2BF08` hypothesis** (§2) could be wrong, which would pull the
   string table into 4a mid-cycle. Mitigated by the specific frame-32/64/96
   drift signature and the named fallback.
5. **Scale rounding direction.** The oracle arbitrates; no tolerance will be
   widened to hide it.
6. **Blit op-set discovery.** Deferred variants get PORT comments at their call
   sites.
7. **Patch perturbs timing.** NOP-ing the spin tightens the wait loop and could
   change the frame count. Mitigated by comparing patched and unpatched frame
   counts; under rung 1 the re-seed makes any pre-title offset difference
   irrelevant.
8. **`res_alloc` becomes public.** One added function, justified in §5.

**Explicit acceptance.** The RNG's unpinned interleaving with the spin is not
verified and will not be. The two proofs together cover everything that matters:
the LCG algorithm and call order by unit test, the composite by oracle, the spin
by design exclusion.

## 10. Testing and verification

**Capture side.** A title capture reusing `smk_capture.py`'s DOSBox-X invocation
(same `-defaultconf -fastlaunch` recipe, `DX-CAPTURE /V`, RGB24) and the same
frame-alignment bootstrap 2b used. A separate `tools/` patch script copies
`PRAGE.EXE` into a temp dir, verifies the original bytes at each patch site,
applies the pin, and fails closed on mismatch.

**Port side.** `make title-oracle`, a thin sibling of 2b's `smk-oracle` stage:
skip when the capture is absent, fail when `PR_ORACLE_REQUIRED=1` and absent.
Comparison is frame-indexed, pixel-exact, zero tolerance — there is no tolerance
parameter, as in 2b.

**Unit tests** (`port/tests/`, existing `run_tests.c` pattern):

| Test | Proves |
|---|---|
| LCG | N outputs from seed `0xABCD` match the recurrence exactly |
| pool | alloc/free/reuse ordering; exhaustion returns 0; free-list intact after `actors_reset` |
| spawn | field initialisation for a representative descriptor |
| blit | identity copy + transparency; scale 0.5/1.5 rounding; clip on all four edges; `pal_map` remap; zero-area no-op; out-of-range `w`/`h` rejected |
| reset | both masks zeroed, every record freed, back buffer seeded from `A0` |

**What each proof covers, and what it does not**
* Unit tests: the LCG algorithm and call order; blit geometry, rounding,
  transparency and palette; pool bookkeeping.
* The oracle: the title composite end to end across the pinned window.
* **Neither:** the RNG's real interleaving with the timing-dependent spin
  (accepted, §9).

## 11. Stubs consumed and stubs left

Consumed by 4a: the `flow.c` title fake (`TITLE_FRAMES` and its state), and the
`0x2BAF4`/`0x2AE14`/actor-pool gap that sub-project 1 left unported. Two
sub-project 1 open items are closed as a side effect: the actor pool's layout
and the per-sprite sub-palette rule (the latter only if the capture yields it).
The `0x10EE4`/`0x29D60` "transition helpers" comment is corrected: `0x29D60` is
a 1-byte no-op and `0x10EE4` is a state-0 reset, both belonging to 4b's attract
skeleton, not to 4a.

Left for later, each named at its site: `0x2BF08` and its `0x1C500`/`0x474E4`
chain (4d, **conditionally** — see §2), `0x10DB0`/`0x10E18` (4b), `0x11F28`
(4b), the `0x11000` attract sub-machine (4b), `CMOS`/EEPROM and high scores
(4c), fonts/text/`ENGLISH.TXT` proper (4d), and every fight-only actor op
(sub-project 5).

Moved into 4a from what the engine-core report listed under sub-project 4: the
pset layer (`0x2F0F0`/`0x2F198`/`0x2F280`/`0x2F4BC`/`0x2F830`/`0x2AD40`), which
sub-project 1 had left entirely unlisted.
