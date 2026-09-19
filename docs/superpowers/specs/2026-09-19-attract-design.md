# Attract Subsystem and State Tails (4d) — Design

**Status:** design approved, spec for review.
**Cycle:** sub-project 4d. Predecessors: 4a (compositor/effects), 4b-A (config core),
4b-B (front-end input, credits, select state) — all merged on `main`.

## 1. Goal and scope

Port the boot/attract sub-machine `0x11000` and the front-end state tails so the port
boots the way the original does — through state 0 (attract), not by substituting the
logo playback and jumping to state 1 — and so the real producer of the title's credit
row (`0x2C06C(1)` at `0x110CE`) replaces the pinned stand-in.

In scope:

| raw | role |
|---|---|
| `0x11000` | attract/boot sub-machine, 13 phases, 1380 B; run from state 0 and state >9 |
| `0x10DB0` / `0x10E18` | per-state pause (→state 4) and continue (→state 5) tails, called after every state |
| `0x10EE4` | 66 B state reset |
| `0x10F28` | 128 B voice/rng scheduler, called from `0x11000`'s tail |
| `0x389C4` | 113 B scroll/zoom projection (edge) |
| `0x38A38` | 224 B scroll/zoom projection (per-row fill) |
| `0x292AC` | 39 B per-bit scene tick, gated by `DS_00104AD0` |
| `0x2C8F0(-2)` | config-derived music/SFX volume apply |

Out of scope (declared gaps, §7): the voice subsystem `0x2C3FC`, fight states 5–9, the
`0x13xxx` effect render path, the front-end states 3 (`0x12484`) and 4 (`0x11578`).

## 2. Decisions

1. **Scope** = the attract subsystem (README's four: `0x11000`, `0x292AC`, `0x389C4`,
   `0x38A38`) plus the per-state tails and reset/scheduler. States 3/4 stay stubs.
2. **Ownership**: a new `port/src/game/attract.{c,h}` owns the machine, the tails, the
   reset/scheduler and `0x292AC`; the scroll projection goes to
   `port/src/platform/render.c` because it writes compositor projection state.
   `flow.c` keeps only the state dispatch.
3. **Boot is one continuous run**: `game_state_init` no longer plays the logos by hand
   or forces state 1; it enters state 0 and lets `0x11000` phase 0 play the logos
   (through the existing `movie_play`) and hand off to title state 1. The attract and
   title oracles share one headless run.
4. **Proof**: (a) pixel oracle against the existing post-logo captures, comparing the
   attract prefix and the title window in one continuous alignment; (b) an env-gated
   deterministic state-0 driver; (c) raw-derived unit tests for the scroll projection,
   `0x292AC` and the two tails.
5. **Voice stays a stub.** `0x2C3FC` is a no-op; `0x10F28` keeps its `0x5D7DC` (rng)
   calls and drops its `0x2C3FC` calls.
6. **`tools/` is read-only**, so the attract comparison is a new
   `tools/attract_compare.py`; `tools/title_compare.py` is not modified.

## 3. Raw evidence (verified during design)

### 3.1 The state machine `0x11D04`

`switch(DS_000F0A64)`, table at `0x11CDC` (obj-0 link offsets +`0x10000`):

| state | target | works |
|---|---|---|
| 0 | `0x11D70` | `0x11000` (attract) + tails |
| 1 | `0x11D88` | `0x121A0` (title) + tails |
| 2 | `0x11DA0` | `0x11F6C` (select) + tails |
| 3 | `0x11DB8` | `0x12484` + tails |
| 4 | `0x11DD0` | `0x11578` + tails |
| 5 | `0x11DE8` | `0x2C3FC(0x100,0x12C)` + `0x1EA08` |
| 6 | `0x11E4F` | `0x11A8C` |
| 7 | `0x11E67` | `0x11BCC` |
| 8 | `0x11EAC` | `0x32970(0)` + `0x257A4(3)` |
| 9 | `0x11ED0` | `0x11BCC` |
| >9 | `0x11D70` | same as state 0 |

Every case's tail is `0x10DB0; 0x10E18; 0x2BF08` (`0x11D75`/`0x11D7A`/`0x11D7F`).
(`0x11D88` title case: `0x121A0; 0x10DB0; 0x10E18; 0x2BF08`.)

### 3.2 `0x11000` phases (from `prage.c:775` plus disassembly)

- **Phase 0** (case 0): `0x2C3FC(); DS_0009AD58 = 1; 0x4F1E4(); 0x2BAF4(); 0x4F1D0();
  0x1C740(); 0x1C740(); DS_000F0A6F = 1`.
- **Phase 1**: `0x2BAF4(); 0x4F1D0(); DS_000F0A6F = <register arg, decompiler dropped>`.
- **Phase 2** (`0x11098`): `DS_000F0A5C += 1`; wrap to 0 when `>= 4`; `DS_0009AD58 = 0`;
  `0x2C8F0(-2)` (`0x110AB` `mov eax,0xFFFFFFFE`); `0x4F1E4(4)` (`0x110B5` `mov eax,4`);
  `0x2BAF4(1)`; **`0x2C06C(1)`** (`0x110C9` `mov eax,1`, `0x110CE` call — the row-1
  producer); `0x33754()` ×8 (palette acquire); `0x38B18()`; `0x13C70()`; `0x2C3FC()` ×2;
  `DS_000F0A62 = 0x3C; DS_000F0A68 = 0xB4; DS_000F0A70 = 3; DS_000F0A6F = 0xC`.
- **Phase 3**: `DS_000F0A50 = 0x2AE14(0); DS_000F0A6F = 4`.
- **Phase 4**: `DS_000F0A50+0x2C -= 0x100`; when `< 0x1001`: `0x2C3FC()`,
  `DS_000F0A6F = 5`, `DS_000F0A50+0x2C = 0x1000`.
- **Phase 5/6**: when `(DS_000F0A50+0x24 & 0x7FFFFFFF) == 0`, spawn `0x2AE14(0)` and
  advance (5→6, 6→7).
- **Phase 7**: same test on `DS_000F0A50`; then `0x2B150()`, `DS_000F0A4C = 0x2AE14(0)`,
  `DS_000F0A6F = 8`.
- **Phase 8**: when `(DS_000F0A4C+0x24 & 0x7FFFFFFF) == 0`, `DS_000F0A68 = 0x40;
  DS_000F0A70 = 9; DS_000F0A6F = 0xC`.
- **Phase 9**: a run of `0x1C500`/`0x2F198`/`0x2F0F0` text calls, gated on
  `DS_00104528` byte 1 bit 1; `if (DS_000F0A5C == 0) { DS_000F0A68 = 0xF0; DS_000F0A70 = 0xB }`
  `else { DS_000F0A68 = 0x78; DS_000F0A70 = 10 }`; `DS_000F0A6F = 0xC`.
- **Phase 10**: `0x2AE14(0); DS_000F0A70 = 0xB; DS_000F0A68 = 0xF0; DS_000F0A6F = 0xC`.
- **Phase 0xB**: `DS_000F0A48 = 0; DS_000F0A6F = 0`; then if `DS_00108173 == 0`:
  `DS_000F0A5C == 0` → `DS_000F0A64 = 1` (title); `DS_000F0A5C == 1` → `DS_000F0A72 = 5`;
  `== 2` → `DS_000F0A72 = 0`; `== 3` → `DS_000F0A72 = 4`; and `DS_000F0A64 = 6`.
  Else `DS_000F0A64 = 8`.
- **Phase 0xC**: `DS_000F0A68 -= 1`; when it was `< 1`, `DS_000F0A6F = DS_000F0A70`.
- **Tail**: `if (DS_0009AD58 == 0) 0x10F28(<args>)`.

`DS_0009AD58` is the attract-active flag; `0x2BF08`'s overlay early-returns when it is
nonzero.

### 3.3 Tails

`0x10DB0`: if `DS_000F0A71 == 0 && (DS_001088D8 byte 3 & 0x20) && (byte 1 & 0x10)`:
latch `DS_000F0A71 = 1`; if `DS_00104B19 byte 2 != 0` then
`DS_00104B19 byte 2 = 0; DS_00104B15 = 0; DS_000F0A6C = 4; DS_000F0A64 = 4; 0x29D60();
0x2C3FC(); return`; else `DS_000F0A64 = 4`.

`0x10E18`: same, with `(byte 3 & 0x10) && (byte 1 & 0x20)`, `DS_000F0A6C = 5`,
`DS_000F0A64 = 5`.

### 3.4 Scroll projection

`0x389C4`: `DS_00107A3C < 1` → `DS_00107A38 = DS_00107A48 >> 6`, `uVar1 = DS_00107A4E`;
else compute `DS_00107A38` from `DS_00107A48`/`DS_00107A3A`, `uVar1 = DS_00107A4A`, and
return early when `DS_00107A4C < DS_00107A4A`; finally `DS_00107A4C = uVar1`.

`0x38A38`: fills the `short` table at `DS_00107900` downward from
`DS_000F0AF0`, `DS_00107A40`, `DS_00107A42`, `DS_00107A52`, `DS_00107A4C` and writes
`DS_00107A3E`, `DS_00107A44+2`, `DS_00107A3A`. The rounding uses the raw's
`(x + (x>>31)*-N - ((x>>31)<<k < 0)) >> k` bias form and must be transcribed exactly.

### 3.5 Support

- `0x33754` = `palette_acquire`, already ported in `actors.c:218` (static) — export it.
- `0x2C8F0(-2)` reads two config fields and applies the volumes through `0x1CAB8`
  (`DS_000A2CB8`, music) and `0x1CED4` (`DS_000A2CB4`, SFX). The port already consumes
  both globals at `flow.c:675`/`698`, so the port shape is two `config_field_get` reads
  into those globals via the existing audio setters.
- `0x292AC`: iterates the bits of `DS_00104AD0`, calling two callees per set bit.
- **`0x4F1D0` and `0x4F1E4` are distinct raw functions**, and the merged 4b-B code
  mislabels them: `0x4F1D0` (`0x4F1D0` `xor edx,edx; mov [0x87A3A],dx; mov [0x87A38],dx`)
  zeroes `DS_00107A3A`/`DS_00107A38`; `0x4F1E4` writes `DS_00104B15 = 0`.
  `flow.c`'s `title_input_reset` implements `0x4F1E4` but is tagged and called as
  `0x4F1D0` at `flow.c:335`/`344` (`game_state_select` phase 0) — a real, latent
  register-level error from 4b-B. 4d introduces the true `0x4F1D0` port
  (`frontend_origin_zero()`) and fixes both the attract phases and the select phase-0
  call site.

## 4. Components and ownership

| file | owns |
|---|---|
| `port/src/game/attract.h` / `.c` (new) | `attract_step()` (`0x11000`), `attract_state_reset()` (`0x10EE4`), `attract_voice_tick()` (`0x10F28`), `attract_scene_tick()` (`0x292AC`), `frontend_pause_tail()` (`0x10DB0`), `frontend_continue_tail()` (`0x10E18`), `attract_config_volumes()` (`0x2C8F0(-2)`) |
| `port/src/platform/render.h` / `.c` | `render_scroll_edge()` (`0x389C4`), `render_scroll_fill()` (`0x38A38`). `0x38A38` writes both `DS_00107A3E/3A/38` (read by `render.c`) and the `DS_00107900` shear table (read by `sprite.c`); this cross-module write is deliberate — both are platform render state |
| `port/src/game/flow.c` | state dispatch only: state 0/>9 → `attract_step()`; per-case tails; `game_state_init` enters state 0; master-loop `DS_00107A54 != 0` → the two scroll calls and `attract_scene_tick()` |
| `port/src/game/actors.h` / `.c` | export `palette_acquire` (`0x33754`) |
| `port/tests/test_attract.c` (new) | unit tests + the env-gated `PR_ATTRACT_DUMP` state-0 driver |
| `port/tests/test_title.c` | join the continuous run: the title window begins after the attract |
| `tools/attract_compare.py` (new) | compares one continuous port run against the post-logo capture |

## 5. Integration and pins

- `game_state_init` (`0x10E80`): set `DS_00104B00 = 3` (the port's chosen mode),
  `DS_000F0A64 = 0`, `DS_000F0A71 = 0`, `DS_000F0A5C = 4`, `DS_000F0A6F = 0`, and stop
  calling `movie_play` directly — `0x11000` phase 0 plays the logos.
- The master loop's `0x292AC` / `0x389C4` / `0x38A38` sites (currently a `PORT:`
  marker at `flow.c:890`) are gated by `DS_00107A54 != 0`.
- The `DS_00105C05 = 1` stand-in at `flow.c:835` is deleted; the value now comes from the
  attract's `0x2C06C(1)`.
- **4b-B correction**: `game_state_select` phase 0's `title_input_reset(); /* 0x4F1D0 */`
  becomes a call to the new `frontend_origin_zero()` (`0x4F1D0`), and the
  `title_input_reset` tag is corrected to `0x4F1E4` everywhere. This changes the select
  state's phase-0 behaviour to the raw's (zero `DS_00107A3A`/`DS_00107A38` instead of
  `DS_00104B15`).
- The title driver and the attract driver share one `game_init()` + `game_loop()` run,
  because `game_init()` may run once per process.

## 6. Definition of done

1. Build 0 warnings; `port/src/symbols.h` regenerates byte-identically.
2. Unit tests prove, against the raw: `0x389C4` and `0x38A38` projection arithmetic with
   exact values; `0x292AC`'s per-bit dispatch; the two tails' input gates and state
   writes; `0x2C8F0(-2)`'s two config reads; and `0x4F1D0` zeroing `DS_00107A3A`/`38`
   (distinct from `0x4F1E4`).
3. The attract pixel oracle runs: one continuous headless run is aligned to the post-logo
   capture. The oracle reports the **first diverging frame**. Acceptance is either zero
   divergence over the attract prefix (capture frames 0..215) or a divergence that is
   individually explained from the raw as a declared unported producer — and the
   existing title window must stay 0 unexplained on both captures. A silently skipped
   frame is a failure.
4. The `PR_ATTRACT_DUMP` driver proves the phase progression and the handoff to state 1,
   with a run-to-run frame-hash diff.
5. `DS_00105C05` has no init-time stand-in; the title row 1 is produced by the attract.
6. The full ladder passes: `make verify` exit 0, 0 warnings, `all checks passed`, the
   capture comparison unchanged, `symbols.h` byte-identical.

## 7. Declared gaps and non-goals

- Voice `0x2C3FC` stays a no-op stub; the attract is silent through it.
- Fight states 5–9, the `0x13xxx` effect render path, and front-end states 3 (`0x12484`)
  and 4 (`0x11578`) remain stubs.
- The `DS_00104B19` byte-2 branch of the tails is ported only if its writer is in scope;
  otherwise the branch is a declared no-op with the raw site recorded.
- The DOSBox-X debugger is unavailable for scripting; the attract driver's assertions are
  static/structural, not runtime values read from the original.

## 8. Risks and plan derivation hand-offs

- **Boot now runs ~216 attract frames before the title.** The title oracle's window
  shifts and `test_title` must run after the attract in the same process. This is the
  highest-risk integration point.
- **`0x11000` phases 1/2 (and phase 9's text calls) drop register arguments** in the
  decompiler (`extraout_CH`, `extraout_CX`, and the `0x1C500`/`0x2F198` pairs). The plan's
  derivation step must recover them from the bytes, exactly as 4b-B did for `0x11F6C`.
- **Fixed-point rounding.** `0x389C4`/`0x38A38` use 16-bit signed arithmetic with the
  raw's hand-rolled bias. Transcription must be exact; a tolerance-based test is not
  acceptable.
- **Pixel oracle scope.** Only frames whose content is fully ported can match. Any
  attract frame whose pixels depend on an unported producer (voice aside) must be
  identified and declared, not silently skipped.
- **A 4b-B error is in the blast radius.** The `0x4F1D0`/`0x4F1E4` conflation (§3.5) means
  the select state's phase 0 has been running the wrong raw function since 4b-B. Fixing
  it changes select behaviour, so the select determinism log may legitimately shift; the
  plan must re-baseline that log rather than treat the change as a regression.
