# Sub-project report — Primal Rage (DOS) → SDL3, sub-project 4a-i: the sprite compositor

Date: 2026-09-17
Branch: `sprite-compositor`
Base: `d514f9a` ("docs: 4a-i plan errata: out-of-bounds scratch, render order,
bank helper signature, verified sprite ids")
Cycle commits: 25, from `d514f9a` (pre-flight errata) through `b63e6e4`; the
final commit on the branch adds this report.
Spec: `docs/superpowers/specs/2026-09-17-sprite-compositor-design.md`
Plan: `docs/superpowers/plans/2026-09-17-sprite-compositor.md`
Ledger: `.superpowers/sdd/2026-09-17-sprite-compositor/progress.md`
Supersedes: sub-project 4a's single scaled-blit model in
`docs/superpowers/specs/2026-09-17-actor-system-design.md` §1.

Scope note: the branch is shared with a concurrent, unrelated plan
(`2026-09-17-gra-extract`). Its `tools:`/`docs:` commits interleave with this
cycle's commits in `git log`; none of them is part of this report, and its
workspace was neither read nor written. The cycle's own commits are the 16
`sprite:`/`render:`/`flow:` feature commits plus 9 `docs: 4a-i plan errata`
commits.

## 1. Summary

The engine can now composite a display list of sprites into the back buffer the
way `0x14328` does. Three units landed:

* `gra.{c,h}` gained a handle-addressed descriptor opener so the 12-byte sprite
  record lives in one place (`gra_sprite_lookup` / `gra_sprite_open` /
  `gra_sprite_pixels`), plus `gra_decode_frame_at` — the shared RLE core exposed
  so the compositor can be cross-checked against it.
* `sprite.{c,h}` (new) is a pure raster unit: `sprite_node_build` (`0x14268`),
  `sprite_blit` (`0x51E5C`) and the RLE / clipped-RLE / mirrored-RLE / raw /
  shear renderers, dispatching through the ten live entries of
  `PTR_LAB_00080C8C`. It consumes a node and writes only
  `mem + DSD(DS_000E87A4)`.
* `render.{c,h}` (new) owns the display list — the 580-node pool
  (`DS_0010153C`), sorted insert (`0x1C3A0`), remove (`0x1C3D0`/`0x1C458`),
  insertion sort (`0x1C3FC`) — and the projection/clip driver `render_list`
  (`0x14328`).
* `game/flow.c` (`+4`, no deletions) calls `render_list_init` in `game_init` and
  `render_list_sort()` + `render_list()` in the master loop, after the render
  process table and before the palette flush — the original `0x255CC` order.
  Until 4a-ii populates the list this is provably a no-op: with an empty list
  the captured title frames are byte-identical before and after the wiring
  (600/600, `diff -rq`), so the wiring is live and harmless.

Files created: `port/src/platform/render.c`, `port/src/platform/render.h`,
`port/tests/test_render.c`. Files extended: `port/src/platform/sprite.{c,h}`,
`port/src/platform/gra.{c,h}`, `port/tests/test_sprite.c`,
`port/tests/test_gra.c`, `port/src/game/flow.c`, `port/tests/test.h`,
`port/tests/run_tests.c`, `port/CMakeLists.txt`.

`make verify` is green, zero compiler warnings (§7). No SDL outside
`host.c`/`main.c`, no new dependency, `data/` untouched, `mem[0xA0000]` never
written.

## 2. The oracle: a three-way RLE agreement

The compositor's primary evidence is that two independent implementations of the
same RLE agree on real asset data, and that the first of them was already
proven against a third:

* `sprite_render_rle` (this cycle) is asserted **byte-identical** to
  `gra_decode_frame_at` (sub-project 1's decoder, exposed through the same
  `decode_frame_core`/`rle_decode`). `check_rle_cross` in `test_sprite.c`
  compares the two on **32 real sprites** — the first 32 positive-dimension RLE
  descriptors at most 320×200, from the shipped set (negative-dimension raw
  markers and zero-size records are excluded).
* The comparison is **row-wise over each sprite's own width**, not a flat
  `memcmp`: the GRA decoder packs rows at `width`, while the renderer is called
  with the composite path's `stride = 320`. A whole-buffer compare would fail
  for every `width != 320` for a stride reason, not a pixel mismatch. Comparing
  `expect + r*width` against `got + r*320` pins every decoder-written byte
  (including row advance) and is exactly the "confirm they agree on row advance"
  property. Row-clipping of the renderer is not pinned by this compare (an
  overflow would land only in the uncompared `[width, 320)` padding) — a
  deferred minor.
* Control-byte class coverage is asserted from the sample (instrumentation
  recorded in `task-3-report.md`): **literal runs 1,220, fill runs 1,523,
  transparent runs 3,834**, so the sample is not vacuous for any of the three
  classes (`0x00..0x7F`, `0x80..0xBF`, `0xC0..0xFF`).
* **Negative control:** with the run's `bank` argument stubbed at identity the
  cross-check fails all 32 sprites once the test calls it at offset 1 (`FAILURES:
  32`), and passes at offset 0 — so the comparison is sensitive to the bank
  offset and is not merely detecting "the copy ignores bank".

**Transitivity, stated honestly.** `gra_decode_frame` was proven against
`tools/gra_render.py` in sub-project 1, but only two ways: by the
**exact-consumption** property on 18,201 of 18,202 descriptors, and by
**byte-identical index buffers and RGB PPMs for the four full-screen,
fully-opaque `S16TITLE` frames `{10,12,13,18}`** (`FORMATS.md`, "Renderer
scope"). So the three-way agreement is byte-exact for that `S16TITLE` subset and
consumption-equivalent in general — the port's `gra_render.py` agreement is not a
general byte-for-byte palette oracle (the Python decoder flattens the palette
bank and maps index 0 to black; the game selects a sub-palette per sprite).
This cross-check needs no emulator and no skip variable: it needs only the
shipped assets, so it cannot be absent from `make verify`.

## 3. Proven by hand-computed tests, not by an emulator

There is **no emulator pixel oracle for the compositor** in 4a-i. The DOSBox
title oracle is 4a-ii's. What is proven here is proven four ways:

**Hand-computed renderer tests.** `test_sprite.c` carries exact expected
buffers, computed by hand from the original's semantics:

* **Clipped RLE (`0x5D28F`)** — a 6-wide, 3-row fixture with bank 0 and a
  distinct-payload two-row stream. The table asserts every case: no clip, left
  cutting a literal, left cutting a transparent run, right only, both sides, a
  window inside a fill run, the top skip (`clip_t = 1`), and `vis <= 0`
  (whole stream consumed, nothing drawn). The distinct-payload stream is the
  source-consumption assertion; the repeated-row fixture cannot hide a per-row
  desync behind its zero padding. Negative control: substituting the unclipped
  row walker fails 6 of these assertions.
* **Mirrored RLE (`0x57F80`/`0x57FFB`)** — rendered plain and mirrored, then
  `mir[i] == plain[5-i]` for all six columns on a **non-palindromic** row (a
  transparent gap plus two distinct runs swap ends). Disabling the mirror fails
  exactly the six comparisons.
* **Mode-1 shear (`0x5215C`)** — a zero table degenerates to a plain copy; a
  non-zero signed ramp gives hand-computed shifted rows. The control uses a
  truncating division instead of the arithmetic `>> 5`: because
  `-33 / 32 == -1` toward zero where `-33 >> 5 == -2`, exactly the negative row
  fails by one byte. This is why the `(s16)` read of `DS_00107900` is
  load-bearing.
* **Raw copy (`0x58CBD`)** — a 4×2 fixture plus a `0xFE`/`0xFF` byte-overflow
  case. The control swaps the byte-wise `+ bank` for the original's replicated
  dword add; only the overflow assertion fails (`0x03` becomes `0x04`),
  pinning byte-wise wrap semantics.

**Dispatch pinned by output.** `sprite_blit`'s switch is checked against the
spec's `PTR_LAB_00080C8C`, and every distinct call target is pinned by a
whole-buffer `memcmp` against the specific renderer the original's table selects
at that node's `(x, y)` — so a cross-class swap (RLE vs raw, plain vs mirrored,
clip arguments into the shared RLE/shear renderer) changes the bytes. Changing
`case 0x01` to the raw renderer fails the 0x01 assertion.

**Projection rounding.** `render_proj_x/y` are unit-tested for exact values and
an exhaustive `-8192..8192` loop. `proj_x(-2048) == -1950` is the discriminator:
the corrected idiom (below) gives -1950, the brief's original `+0x1000` form
gives -1949, and a plain `>>12` gives -1951.

**End-to-end.** A hand-built two-entry display list composites to a buffer the
test builds directly through the renderers (`memcmp`, no tolerance); swapping
the two layers swaps the winner. The list pool is exercised to 580-node
exhaustion, the sort's stability is asserted with equal layers, and a
disorder case (a layer mutated after insertion) forces a real reorder — the
brief's 5,1,3 case is a no-op because the sorted insert already keeps the order.

### The windowed formulation is a deliberate `PORT` deviation

The original's clipped RLE renderers are a **six-way nest of straddle
branches** (left/right/both × a span crossing the edge). The port instead
decodes **one row at a time and intersects each run with the row's visible
window** `[clip_l, clip_l + vis)`, writing window-relative at `dst[0]`; the
mirror becomes an in-row index (`vis - 1 - (col - clip_l)`) rather than the
original's backward-walking destination pointer. This is recorded as a
`/* PORT: ... */` note in `sprite.c` naming `0x5D28F`, `0x57F80` and `0x57FFB`.
**Its evidence is the hand-computed straddle tests plus the cross-check and the
negative controls — not a pixel oracle.** The equivalence argument is that every
original branch draws exactly the window-covered part of the straddling run and
clips-off portions are still walked, so source consumption matches the
original's `esi` rewind for the literal case (verified by the distinct-payload
source-consumption assertion).

The projection idiom itself is a transcription, not a deviation: `0x14328` uses
`imul` / `sar 31` / `shl 12` / **`sbb`** / `sar 12`, whose net effect is
`p + 0xFFF` for a negative `p` — i.e. round half toward **+infinity**, not half
away from zero. `proj_x(-4096) == -3900` (the symmetric answer would be -3901).
This is implemented exactly and carries its own `PORT:` note. Task 11 stopped
because the plan's transcribed formula and its explicit assertions were
internally inconsistent; the controller re-derived the idiom from
`port/decomp/prage.c:3149-3163` and the plan was corrected (`834cf23`).

## 4. Pinned original quirks

* **`DAT_00081310[0] == 0x0005D110` — a stale code pointer, not replicated for
  bank byte 0.** For `n = 1..255`, `DAT_00081310[n] == (n-1) * 0x01010101`, so
  the effect is a per-pixel index offset `+ (n - 1)`. Entry 0 holds a code
  pointer whose four bytes differ, so the original's dword add gives each pixel
  *within a 4-pixel group* a different offset — position- and
  alignment-dependent garbage that a byte-wise port cannot reproduce. The port
  **pins the table entry** (the test asserts `0x0005D110`) and maps bank byte 0
  to **no offset**, recording the divergence in `sprite.h` and the plan. It is
  neither hidden nor reintroduced as an offset. Whether any shipped asset ever
  reaches bank byte 0 is unknown (a 4a-ii `TODO(verify)`, §6).
* **`RAW+HFLIP` (type `0x0A`) is a no-op in the original** — the dispatch table
  slot 10 is the bare-`ret` stub `0x51E58`, because hflip is valid only on the
  RLE base. The port implements no `case 0x0A`; letting it reach the raw
  renderer fails the type-10 no-op assertion.
* **The bank and colour tables are pinned exhaustively.** The test loops all 255
  bank entries and all 256 colour entries (`DAT_00081314[n] == n * 0x01010101`),
  and cross-checks `sprite_bank(desc_handle)` against the descriptor's own byte
  8 for a real sprite.

## 5. Corrections this cycle made to earlier notes

These correct wrong statements in the sub-project 2a notes and in `flow.c`; the
corrected `flow.c` comments are comment-only (§8).

* **`DAT_000BBDC8` is static in the EXE, not runtime-populated-and-zero.** It is
  a static table in the data object, **stride 12 bytes, byte 0 = case selector,
  dword at +4 = resource handle**. This makes the sound-table id → resource
  mapping *extractable*; the port has not extracted it yet, so the `TODO(verify)`
  stays and the port binds the title state to the bank directly.
* **The title's `FUN_0002C3FC(0x41)` / `(0x43)` are case-5 voice cancels, not a
  case-1 music request.** `FUN_0002c3fc` dispatches on the record's case byte;
  case 1 reads `DAT_00105d5c` from the record's handle and starts the sequence,
  while case 5 for `param_1` `0x41`/`0x43` calls `FUN_0001ce04`, which walks the
  voice table and stops the voice whose id matches (a cancel). The two records'
  handles (`0x383B6F4` / `0x3837440`) point into `s16title.gra`. This
  **corrects 2a's rationale without changing its oracle result**: 2a's 9,340-write
  C-vs-Python comparison still passes, because it tested the opcode stream, not
  this rationale.
* **The title's sprites come from `S16ATTRC.GRA` (resource index 8), not
  `S16TITLE.GRA` (index 7).** Ids `0x2C11…` resolve through the static table
  `DS_000A8B30` to resource 8. The port's `TITLE_RES 7` was a choice made for the
  faked title and applies only to the bank that fake read.
* **The pset "scale" is not a display-path scale.** Only **positions** are
  projected (`proj_x`/`proj_y`, the fixed 320/336 and 200/240 ratios); sprite
  pixels blit **1:1**. Pset `+0x0C` is copied from the actor and never read by
  the display path, and the `+0x2C` field the first draft read as a 4.12 scale
  is an actor-side value whose display role is nil.
* **The clip rectangle is a literal `{0, 0, 320, 200}`**, the master loop's
  camera struct at `DS_000A87CC + 8` (and `0xBCD64 + 8` for the other caller),
  not a runtime camera viewport.

## 6. What 4a-ii still needs

4a-ii is `docs/superpowers/specs/2026-09-17-actor-system-design.md`: the actor
pool, the pset sync (`0x2A31C` → `0x2A1FC` → `0x2A820`), the animation-stream
interpreter (`0x2A408`/`0x29F34`), the real title state `0x121A0`, the RNG pin,
the DOSBox title pixel oracle, `0x2BF08`, and the `DS_00107900` shear-table
producer `0x38A38` (4a-i only consumes the table). 4a-ii calls 4a-i's
`render_list_insert` from spawn's emit path.

Two `TODO(verify)` items are left open by this cycle for 4a-ii's pixel oracle:

1. **The mode-offset projection's form and load width.** `0x14328`'s three
   offset projections (layer-1 x `DS_00107A3E` at `prage.c:3166`, layer-2 x
   `DS_00107A3A` at `:3173`, layer-2 y fallback `DS_00107A38` at `:3175`) use an
   uncorrected `+0x800` then `>>12` (`proj_scale_u`), unlike the main projection.
   The decompiler renders those operands `(uint)` while the port sign-extends
   with `(s16)`; with all five globals zero in a compositor-only run the two
   forms coincide, so only 4a-ii's oracle can settle both.
2. **Whether any shipped asset ever reaches bank byte 0** (§4).

## 7. Verification (`make verify`, run on this cycle)

```
$ make verify > /tmp/pr_verify.log 2>&1; echo EXIT=$?
EXIT=0

== headless frames (must precede the tests that read frame_*.idx) ==
./build/prageport --game-dir data/game/C --check 60
== tests (oracles required; consume the captured frames) ==
PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests
oracle C-vs-Python: 9340 writes byte-exact
all checks passed
== smacker frame oracle (pixel-exact) ==
all checks passed
smk_compare: 120/120 frames match
smk_compare: 41/41 frames match
== gra_extract oracle tests (real assets required) ==
Ran 32 tests in 1.157s
OK
== symbols.h must regenerate byte-identically ==
1304 globals, 1206 functions -> port/src/symbols.h
  dropped 11 globals, 1 functions outside the LE objects
all checks passed
```

Zero compiler warnings in the full build (the log's only diagnostics are
python `ResourceWarning`s from the concurrent `gra-extract` tooling, not the C
build). The compositor's cross-check oracle runs inside `all checks passed` and
cannot be skipped: it needs only the shipped assets.

## 8. Deferred minors — disposition

Recorded in the ledger; none blocks this cycle. The compositor is
test-complete for its scope; the deferred items are coverage gaps, not defects.

* **Task 1 (2):** `gra_sprite_pixels` leaves `*out` untouched on failure;
  `gra_sprite_lookup` writes `*desc_handle` before `gra_sprite_open` can fail
  (both harmless if the caller gates on the return).
* **Task 2 (4):** the resolvable-path zeroing of `pal_ptr`/clip/stride/row is
  untested; `SpriteNode` is a named-field struct, not a 0x40-byte image of the
  original node (safe while all access is by name); `u32 dh` is written but
  never read; the `id == 0`/unresolvable paths are untested.
* **Task 3 (5):** the cross-check's row compare does not pin renderer row
  clipping; `sprite.c` includes `../symbols.h` for `DS_00107900` only; a stale
  comment is not `PORT:`-prefixed; the test's first-absent-descriptor `break` is
  fragile; `sprite_render_rle` takes no source length (faithful to `0x5D218`).
* **Task 4 (2):** a garbled half-sentence comment; the descriptor-byte-8 check
  is vacuous if that byte is 0.
* **Task 5 (1):** `dst[32]` over-allocates (20 would fit).
* **Task 6 (3):** the `clip_t` skip path reuses the zero-padded fixture; the
  16-byte stride tail is never asserted untouched; negative `clip_l`/`clip_r`
  are unguarded (out of contract; `render_list` clamps inward).
* **Task 7 (1):** mirror is tested only with `clip_l = 0`/`vis == width`, so a
  bug ignoring `clip_l` would pass; a mirror+clip case is the gap.
* **Task 8 (2):** `clip_t < 0` is not clamped; the shear test does not exercise
  non-zero `clip_l`/`clip_r`/`clip_t` or the no-draw paths.
* **Task 9 (2):** `type & 0x1F` vs the original's `& 0x7F` (no producer sets
  bits 5-6); a negative hand-built `x` would wrap `(u32)n->x` (caller contract).
* **Task 10 (2):** the sort's stability path is untested in isolation; the
  report cites negative-control lines 50/51 (checks are 51/52).
* **Task 11 (2):** the exhaustive loop mirrors the implementation (the literal
  assertions are the discriminators); the `DS_00107A3E/3A/38` load width is
  carried to 4a-ii (§6.1).
* **Task 12 (2):** the non-vacuity guard asserts the two bank bytes differ, not
  that the banks render differently; the `0x7FFF` scan bound is unexplained.

## 9. Unresolved risks

1. **The window-intersection formulation is equivalence-argued, not
   oracle-proven** (§3). The hand-computed straddle table and the cross-check
   cover every branch the design names; a DOSBox pixel oracle for a clipped,
   projected composite is 4a-ii's and does not exist yet.
2. **The bank-0 divergence is unfalsified** (§4/§6.2). If a shipped asset
   reaches bank byte 0, the port's output will differ from the original in
   4-pixel groups; the `TODO(verify)` names this as the first suspect.
3. **The mode-offset projection form and load width are unproven** (§6.1); the
   globals are zero in every compositor-only run, so the two forms coincide
   today.
4. **`RAW+HFLIP` is a no-op by transcription.** The port relies on the
   dispatch-table read, not on running the original; it is asserted by test.

## 10. Honesty statement

The project may claim:

* the compositor's RLE renderer is **byte-identical** to sub-project 1's
  `gra_decode_frame` core on 32 real sprites, row-wise at each sprite's own
  width, covering all three control-byte classes (1,220 literal / 1,523 fill /
  3,834 transparent runs);
* that agreement is **byte-exact against `tools/gra_render.py`** only for the
  same four full-screen `S16TITLE` frames sub-project 1 pinned, and
  consumption-equivalent in general;
* the clipped, mirrored, shear and raw renderers are proven by **hand-computed
  exact-buffer tests with failing negative controls**, and the dispatch table by
  output per class;
* the projection rounding is the original's `sbb` idiom, with
  `proj_x(-2048) == -1950` the discriminator against both wrong forms;
* the original's bank-0 defect and `RAW+HFLIP` no-op are pinned and tested;
* the wiring into the master loop is a proven no-op on an empty list (600/600
  captured title frames byte-identical);
* `make verify` is green with zero warnings.

The project may **not** claim:

* that the compositor has been compared pixel-for-pixel against the original in
  an emulator — it has not; that oracle is 4a-ii's;
* that the window-intersection clipped renderer is a transcription of the
  original's six straddle branches — it is a `PORT` formulation whose evidence
  is the hand-computed tests and the cross-check;
* that the three-way RLE agreement is byte-exact for all sprites against
  `tools/gra_render.py` — that holds only for the pinned `S16TITLE` subset;
* that bank byte 0 or the mode-offset projections are settled.

## 11. Final status

Sub-project 4a-i's scope is complete: the display node, all six renderers, the
blitter, the display list and the projection/clip driver are ported and tested,
and the master loop composites the list in the original's order. The cycle's
DoD — cross-check oracle under `make verify`, unit coverage of clipping, hflip,
shear, bank and clip edges, green build with no new warnings, no `data/` writes
and no new dependency — is met. The actor system (4a-ii) is the named next plan;
it consumes `render_list_insert` and owns the title oracle that will close the
two open `TODO(verify)` items.
