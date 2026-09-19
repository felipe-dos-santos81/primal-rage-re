# Design: Primal Rage (DOS) → SDL3 port — sub-project 4a-iv: the `0x13xxx` effect producers

Sub-project 4a-iii ported the effect *list* — the 24-record pool, its
spawn (`0x13C70`), free-list build (`0x13ADC`), per-frame step (`0x134C0`),
teardown (`0x13420`) and clear (`0x13DF0`) — and shipped it wired into the title
path (`0x123EA`). Its report records a declared gap: `0x13C70` **does not move
the composite oracle**, because the port has no renderer for what an effect
actually changes. This cycle closes the producer side of that gap.

## 1. Context

* The effect pool: 24 records of stride `0x814` at `0xF0B00..0xFC4CC`, active
  and free sentinels `DS_000FCCE0`/`DS_000FCCE8`, interrupt lock
  `DS_0009AF3C`, active count `DS_0009AF3D`. Shipped as
  `port/src/game/effects.{c,h}`.
* An effect record's **type** byte (`rec+0x0C`) is written by its producer and
  later dispatched by `effects_step` (`0x134C0`) and `effect_teardown`
  (`0x13420`). `effects_step` already implements the bodies for types 0/2/3/4/6
  (types 1 and 5 exist as transcribed bodies).
* **Effects do not draw pixels.** They mutate the palette dirty list: every step
  or teardown body calls `palette_record` (`0x33714`/`0x33734`), which appends a
  `{ptr; first; count; flag}` record; `gfx_flush_palette` (`0x1C470`) drains it
  into `gfx_dac` (the displayed 8-bit RGB). Both are already ported
  (`port/src/platform/gfx.c`). This is *why* `0x13C70` cannot move the composite
  oracle: the title/composite oracles compare pixel **indices**, and a palette
  change is invisible to them.
* The producer set is complete and provable. The free-list head
  `DAT_000fcce8` is referenced by exactly four functions, so exactly four
  functions can take a record from the pool:
  | producer | type written | callers |
  |---|---|---|
  | `0x13C70` | **3** | 8 (incl. the title `0x123EA`) — ported |
  | `0x13D4C` | **4** | `0x29B74`, `0x41578` |
  | `0x13E28` | **6** | `0x11F6C` |
  | `0x13B3C` | **0** or **2** | none direct; referenced by the jump table at VA `0x23AC4` (`0x13b3c, 0x13b20, 0x13b27, 0x13b2e, 0x13b35, 0x13b3c, 0x13b20`) |
  Therefore **types 1 and 5 have no producer and are dead** in the shipped EXE.
* `0x11F6C` (the type-6 producer's caller) is reached from `0x11D04`, which
  **is** ported (`flow.c`'s title/attract state machine). The other two callers
  are not ported.
* The rest of the `0x13xxx` range is a different subsystem: `0x13134`,
  `0x1317C`, `0x13244`, `0x1324C`, `0x13290`, `0x1333C` are camera / fade /
  scroll integrators over `DS_000F0AEC` / `DS_000F0AF0` / `DS_000F0AF4`;
  `0x13EF0`/`0x13F68` are a text filter. None is an effect producer.

### 1.1 Decisions taken during brainstorming

| # | Decision |
|---|---|
| 1 | **Scope is the three missing producers**, not the camera/scene functions and not call-site wiring. The producer set above is the natural boundary: it makes every reachable effect type constructible. |
| 2 | **Proof is unit-level, reaching `gfx_dac`.** No new capture is built. The strongest available proof is port-side end-to-end: spawn → `effects_step` → `gfx_flush_palette` → assert the DAC colours. A palette capture oracle is explicitly *not* in this cycle. |
| 3 | **Both levels of test.** Record-field assertions (the contract each variant writes) **and** the end-to-end `gfx_dac` test; the end-to-end one is the headline because the palette chain has no end-to-end test today. |
| 4 | **Raw disassembly is the source**, not `prage.c`. The decompiler drops register arguments here and appears to omit `0x13B3C`'s active-count increment; all three bodies are re-derived with capstone, as the `2026-09-18-title-residuals-args.md` precedent requires. |
| 5 | **Types 1 and 5 are declared dead by proof, not ported.** The producer-set enumeration is the evidence. The transcribed step bodies for 1/5 stay (they are unreachable, not wrong). |
| 6 | **No palette-fidelity claim.** The port-side chain is proven; that the original's rendered colours match is not. |

## 2. Scope

### In scope

* `port/src/game/effects.{c,h}` — the three missing producers
  (`0x13D4C`, `0x13E28`, `0x13B3C`), plus one shared free-list pop helper if the
  raw code shows they share it exactly.
* `port/tests/test_effects.c` — record-field assertions and the end-to-end
  `gfx_dac` test.
* A derivation document recording the raw disassembly for each producer and the
  types-1/5 reachability proof.
* `README.md` / `port/spec/game_flow.md` if they state the effect coverage.

### Non-goals

* Call-site wiring. `0x29B74`, `0x41578` and `0x11F6C` are unported; the new
  producers ship callable and tested but not yet invoked from game flow.
* The `0x131xx`–`0x133xx` camera/fade/scroll functions, and `DS_000F0AEC`.
* A palette capture oracle (a new capture tool + dosbox session).
* Any change to `data/`, the captures, or `tools/opl_trace.py`.
* The `0x13C70` type-3 path, `effects_step`'s existing bodies and
  `effect_teardown` — unchanged except for the shared helper if extracted.

## 3. Architecture

One owner for the producer contract: `port/src/game/effects.c`, beside the
existing `effects_spawn` (`0x13C70`). Each variant is a faithful transcription of
one original function — they share the pop-free-record step but differ in the
type byte, the `+0x0F` flag, which block is filled, and whether the target block
is filled at all. The port keeps them as separate functions rather than fusing
them behind a parameterised helper, because the differences are the contract and
register-level fidelity is this project's rule; only the genuinely identical
free-list pop is factored.

The palette path (`palette_record`, `gfx_flush_palette`) is already owned by
`port/src/platform/gfx.c`; this cycle does not change it, it exercises it.

## 4. The three producers

Behaviour, to be **confirmed from raw bytes** before implementation (the shapes
below come from the decompiler and are the hypothesis, not the authority):

| producer | type (`+0x0C`) | `+0x0F` | `+0x0E` | `+0x10` block | `+0x410`/`+0x414` block | count |
|---|---|---|---|---|---|---|
| `0x13D4C` | 4 | `0` | 1 | resolved `[1+i]` | — | ++ |
| `0x13E28` | 6 | `0x80` | 1 | `0xFFFFFF` | resolved `[1+i]` in `+0x410` | ++ |
| `0x13B3C` | 0 (flag 0) / 2 (flag ≠ 0) | count | 0 / 1 | signed offset byte | resolved walked ±, both `+0x14` and `+0x414` | **to be pinned** |

Each takes a record from the free list under the lock, fills it, head-inserts it
into the active list, and increments the active count (except where the raw
bytes say otherwise for `0x13B3C`). Each returns the record's `mem[]` offset, or
`0` when the pool is empty — the same contract `effects_spawn` already has, and
the same unbuilt-pool guard (`DS_000FCCE8 == 0`).

Two specific raw-byte questions must be answered before coding:

1. Does `0x13B3C` increment `DS_0009AF3D`? The decompile shows `0x13C70`,
   `0x13D4C` and `0x13E28` incrementing it and does not show the increment in
   `0x13B3C`; that is either a decompiler omission or real.
2. Does `0x13B3C`'s zero-flag arm really set `+0x0E = 0`? If so, and if
   `effects_step`'s type-0 branch skips while the state byte is 0, such a record
   lingers forever. Reconcile against the raw step code before trusting it.

## 5. Reachability

* **Types 1 and 5 are dead.** Proven by the producer set in §1: the only four
  functions that can take a record from the pool write types 0/2, 3, 4 and 6.
  No other code path obtains a fresh record, so no path can write type 1 or 5.
  Recorded in the derivation doc; not encoded as a test (dead code is not
  testable, and asserting an absence would be the "test that asserts nothing"
  defect).
* **`0x13B3C` is live** despite having no call-graph caller: its address is a
  target in the jump table at VA `0x23AC4`, which is reached from obj-0 code.
  The table is the switch that selects the type-0/2 variant.

## 6. Oracle and Definition of Done

* `make verify` exit 0, 0 warnings, `port/src/symbols.h` byte-identical, no
  `data/` change; the title, smk and GRA oracles unchanged.
* For each of `0x13D4C`, `0x13E28`, `0x13B3C`: an assertion that **fails before
  the port** (RED), then passes (GREEN), covering the record fields in §4 and the
  emitted `palette_record` sequence.
* **End-to-end:** a test that spawns via each variant, runs `effects_step()`
  until the body fires, drains through `gfx_flush_palette()`, and asserts the
  resulting `gfx_dac` entries are the expected 6-bit-expanded RGB — proving
  spawn → step → dirty list → DAC.
* The derivation document records, per producer, the raw instructions and file
  offsets that establish the behaviour, plus the types-1/5 producer-set proof.
* `port/spec/game_flow.md`'s effect coverage is updated to name the four live
  producers and the two dead types.

## 7. Residuals and honest limits

* **No palette capture oracle.** The chain is proven port-side only; whether the
  port's colours equal the original's on screen is not proven. Named, not
  implied green.
* **No wiring.** The new producers are reachable in the port only from tests.
  `0x11F6C` (from the ported `0x11D04`) and `0x29B74`/`0x41578` remain unported,
  so no shipped path spawns types 0/2/4/6 yet.
* **Camera/scene functions untouched.** `0x1317C`, `0x1324C`, `0x13290`,
  `0x1333C` engage `DS_000F0AEC`/`DS_000F0AF0`/`DS_000F0AF4`; `DS_000F0AEC` is a
  carried unconfirmed value in this repo. It becomes the next-cycle subject if
  the scene path is ported.
* `0x13B3C`'s zero-flag arm may produce a record that never retires (§4.2). If
  the raw bytes confirm it, it is reproduced faithfully and named as an original
  behaviour, not "fixed".

## 8. Verification limits

Proof is the record contract plus the port-side DAC chain. It is **not** a claim
that the original's on-screen colours match, that any producer is wired into game
flow, or that types 1/5 would behave correctly if some path did construct them.
Each is named above rather than implied green.
