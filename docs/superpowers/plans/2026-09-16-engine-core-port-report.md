# Sub-project report — Primal Rage (DOS) → SDL3, sub-project 1: engine core

Date: 2026-09-16
Branch: `engine-core`
Base commit: `bc551aa` (Initial commit: RE artifacts, specs and decompilation)
Engine-core commits: 30, `bc551aa..bb3096a` (`bb3096a` = Task 15), plus the
final documentation commit that adds this report.
Spec: `docs/superpowers/specs/2026-09-16-engine-core-port-design.md`
Plan: `docs/superpowers/plans/2026-09-16-engine-core-port.md`
Ledger: `.superpowers/sdd/2026-09-16-engine-core-port/progress.md`

## 1. Summary

The engine core boots, runs the original's frame loop on the original
`data/game/C/` assets, and shows an animating title screen in a 320×200 SDL3
window. Everything on the loop path that belongs to a later sub-project is
stubbed at its call site behind a `/* PORT: */` marker — nothing on the path is
silently missing.

What runs:

```
main --game-dir data/game/C
  -> game_main (0x1BEC4 init chain: int10h query, res_load_index, 0x51F45
     surface setup, 0x336C0 palette init, game_state_init)
  -> game_loop (0x255CC): input_pump (0x500C4) -> game_frame (0x24C5C) ->
     render process table (PTR_FUN_000A86C4 / _DAT_00104AEC) -> frame counter ->
     gfx_flush_palette (0x1C470) -> gfx_present -> swap (0x50188) ->
     host_wait_vblank -> ESC check
  -> game_frame: frame counter, update process table (PTR_FUN_000A8644 /
     _DAT_00104AE8), state machine 0x11D04 (case 3 = mode DSD(DS_00104B00))
  -> state 1 (0x121A0) title: full-screen S16TITLE.GRA frames {10,12,13,18}
```

Headless `--check N` (Task 15) runs the same loop one frame at a time with no
window (`host_init()` is never called) and writes `frame_NNNN.ppm`,
`frame_NNNN.pal`, `frame_NNNN.idx` per frame; the exit code is the accumulated
assertion-failure count.

## 2. Verification ladder (run 2026-09-16, `engine-core`)

```bash
cmake -S port -B build && cmake --build build
PR_ORACLE_REQUIRED=1 ./build/run_tests
PR_ORACLE_REQUIRED=1 ./build/prageport --game-dir data/game/C --check 60
python3 tools/gen_symbols.py port/decomp port/src/symbols.h
```

Observed:

| Step | Result |
|---|---|
| configure + build (cached) | exit 0, 0 warnings |
| clean out-of-tree build (`/tmp/prbuild-clean`) | exit 0, 0 warnings (`grep -ciE 'warning\|error'` = 0) |
| `PR_ORACLE_REQUIRED=1 ./build/run_tests` (no frame artifacts) | `all checks passed`, exit 0 |
| `PR_ORACLE_REQUIRED=1 ./build/prageport --game-dir data/game/C --check 60` | `prageport 0.0.1 game-dir=data/game/C --check 60 (headless)`, exit 0; 60 `.ppm` + 60 `.pal` + 60 `.idx` |
| `run_tests` again after `--check` (artifacts present) | `all checks passed`, exit 0 |
| `python3 tools/gen_symbols.py port/decomp port/src/symbols.h` | `1304 globals, 1206 functions`, `dropped 11 globals, 1 functions outside the LE objects`, exit 0 |
| `git diff --exit-code -- port/src/symbols.h` | no diff (header byte-identical) |

`PR_ORACLE_REQUIRED=1` is mandatory for a real verification run. The two oracles
(`port/tests/ghidra_data.bin`, `port/tests/s16title_frame10.idx`) are copies of
the game's own bytes and are git-ignored, so a fresh checkout skips the
byte-exact comparisons. Without the variable a run can report `all checks
passed` while never performing them.

Negative controls run this session (each proves an assertion is not vacuous):

* corrupting `frame_0017.idx` (image 13 ← image 10) → exactly 1 FAIL, exit 1;
* reverting `host_read_file`'s bound check to the old `(u32)sz > max` → the
  suite aborts (exit 134), demonstrating the sparse-file test discriminates the
  64-bit-truncation bug.

## 3. Modules

`/` = ported and verified; `(stub SPn)` = deferred to sub-project n.

### `src/mem.h/.c` — flat memory + LE image + code-address map

* `mem[]` flat 64 MB (`0x4000000`), original addresses preserved; accessors
  `DSB/DSW/DSD/DSP` (offset includes the `0x80000` data-object base),
  `mem_in_range`, `mem_fill`.
* `mem_load_le()`: MZ → inner LE header, object table, 1-based page map
  (index `pageidx - 1 + p`; physical page `entry >> 16`); page-data base is the
  bound image base `0x26654` + header `0x80`; reads the truncated final page
  (2585 B); zero-fills each object's virtual-size BSS tail.
* `mem_load_le_fixups()`: LE type-7 internal 32-bit offsets, strict whitelist
  (`src == 0x07`, `tf` bits limited to `0x50`); unhandled forms fail closed.
* `fn_register` / `fn_resolve` / `fn_origin`: code-address ↔ C-function map
  (1206 `FN_` addresses), 1300-entry ceiling with an unconditional abort before
  the store; unknown → NULL/0, callers must NULL-check.

Verified: `mem[]` is byte-exact against Ghidra's fixup-applied data object over
all `0x8B0D0` bytes (27,600 fixups); raw page data differs at `DS:0x00c8e`
before fixups, so the fixup pass is load-bearing; the `0x8B0D0` BSS tail matches
Ghidra (zero).

### `src/types.h`

`u8/s8/u16/s16/u32/s32`, cast at every truncating step.

### `src/host.h/.c` — SDL3 seam

Window + `SDL_PIXELFORMAT_RGB24` scratch surface, `SDL_UpdateWindowSurface`
present, event drain → `input_push`, 60 Hz tick (`CLOCK_MONOTONIC`, bounded
30-tick catch-up rebase), `host_wait_vblank`, `host_read_file` (64-bit bound
check), `host_write_file`. SDL is included **only** here and in `main.c`.

Verified: `nm build/run_tests` shows a single strong `_host_pump` (the Task 10
weak-seam trap is gone); `host_shutdown`/`host_pump`/`host_present_rgb` are safe
before `host_init` and with `SDL_VIDEODRIVER=dummy`; the tick advances; the file
round-trip and over-max rejection hold, including the new sparse-file
(4 GiB+2, `max=4`) discrimination test.

### `src/platform/res.h/.c` — INDEX resource manager

`res_load_index()` ports `0x1B120`: 20-byte records, table at `DS_001014E0`,
count at `DS_001014F0`, largest size at `DS_001014F8`, per-entry data block at
`entry+0x10`, and the four post-loop buffers (`0xFA00` = 64000 = a 320×200
surface, `0x4880`, `0xEBA0`). All **69** resources are read from disk
(41.31 MB) — this replaced `0x1B3AC`'s file mapping. `res_handle` /
`res_resolve` (matching `0x1B544`, `ptr = entry.data + (handle & 0x7FFFFF)`) /
`res_name` / `res_size` / `res_flags`.

Verified: 69 entries; table pointer in range; every entry has a data block;
`res_resolve` fails closed for out-of-range indices and missing data blocks.
Heap top `0x2A8C548` (44.6 MB) leaves 21.45 MB under the 64 MB `mem[]`, which is
why the spec's `MEM_SIZE` was raised from 16 MB (`/* PORT: */` + spec amendment).

### `src/platform/gra.h/.c` — GRA container

`gra_open()` walks the 8-byte chunk chain (self-loop, backward jump,
beyond-EOF and over-long chains all rejected/bounded). `gra_decode_palette()`
decodes chunk 5 (`{u32 count; u32 colour[]}` concatenated). `gra_decode_frame()`
decodes a chunk-6 descriptor `{u16 w,h; s16 x,y; u32 pixel_handle}` through
`res_resolve` into the chunk-2 RLE blob.

Verified: **exact-consumption 18,201/18,202 across all 69 files** (the one miss
is a `S16TITLE` 74×167 descriptor at `0x349d7`); 0 errors; 17 zero-dimension and
10 negative-dimension sentinels rejected before length math; an 804-byte
overflow in `gra_decode_palette` was found and fixed (`S16ATTRC.GRA` has a
1292-colour bank), ASan-confirmed. For `S16TITLE` frames `{10,12,13,18}` the
port's index buffers and RGB PPMs are byte-identical to `tools/gra_render.py`.

### `src/platform/gfx.h/.c` — palette + present

`gfx_flush_palette()` ports `0x1C470` (VBlank gate, 4×u32 dirty records, clamp
to `0x100`, consume marking, handle path `(rp - mem) + 4` to skip the bank's
colour count, unresolvable handle skipped before any DAC write).
`gfx_present()` converts an index buffer through `gfx_dac` to RGB24 and hands it
to `host_present_rgb`. `gfx.c` includes no SDL. `gfx_dac` holds 8-bit guns
(`word >> 2`), not the hardware's 6-bit values.

Verified: R/G/B field order, first-index placement, count bounds, clamp (no wrap
past `0xFF`), head reset, consumed marking, handle path, unresolvable-handle
skip. The original VGA DAC ports `0x3C8/0x3C9` become array writes
(`/* PORT: */`).

### `src/platform/input.h/.c` — BIOS keyboard queue

64-entry ring `input_push`/`has_key`/`get_key`/`check_key`/`clear`, BIOS packing
`(scan << 8) | ascii`. Verified: head/count arithmetic across empty, single,
full, overflow and clear; exactly one entry dropped per overfull push.

### `src/game/flow.h/.c` — init chain, loop, state machine, title

`game_main` (`0x1BEC4`), `game_loop` (`0x255CC`), `game_frame` (`0x24C5C`),
`game_state_step` (`0x11D04`, case 3), `title_load`/`game_state_title`
(`0x121A0`), `surface_setup` (`0x51F45`), `palette_list_init` / `palette_record`
(`0x336C0` / `0x33734`), `run_process_table`, `swap_buffers` (`0x50188`).

Verified: the update process table is walked and its registered function called;
the frame counter increments; the title animates; the presented buffer is a
drawn image, not blank (the Task 14 ordering bug — fixed after the reviewer
reproduced a 50 % blank-frame duty cycle); the title palette reaches `gfx_dac`;
a deferred state is a harmless no-op; a non-3 mode skips the state machine; ESC
exits; the windowed run shows the animating title. The aperture rule is honoured
— no `mem[0xA0000]` write.

### `src/main.c`

Arg parsing (`--game-dir`, `--check N`, `--help`), the windowed run, and the
Task 15 headless `run_check()` / `capture_frame()`.

## 4. Stubs by sub-project

| Sub-project | Stubbed calls (original addresses) |
|---|---|
| 2 Smacker — **superseded** | Sub-project 2b-i ported the Smacker video decoder and the boot-logo player; `twi5.smk` / `twg.smk` now play on the boot path, pixel-exact against the original (TWI5 120/120, TWG 41/41). See `2026-09-17-smacker-video-report.md`. Streamed Smacker audio (2b-ii) remains. |
| 3 Audio — **superseded** | Sub-project 2a ported the AIL surface, sequencer, samples and mixer; the stubs listed here are gone. See `2026-09-16-audio-ail-port-report.md`. The four `0x5dd*` movie/streaming stubs now belong to sub-project 2b (Smacker). |
| 4 Menus / EEPROM | EEPROM read `0x47370`, menu-input poll `0x11F28`, transitions `0x10EE4`/`0x29D60`, character-select states 2–4, `0x11000` attract sub-machine |
| 5 Fight engine | 2 player records at `DS_001077E0` (stride `0x94`), `0x4F644` and the second `0x38990` per-frame service, fight states 5–8, `0x292AC`, `0x134C0` (scene/narrative) |

## 5. `/* PORT: */` deviations (56 markers)

`mem.c`: `mem[]` alignment `aligned(0x4000)`; the "data pages offset" note.

`host.c`: stall catch-up clamp (30 ticks); partial BIOS scan-code map; window
close maps onto ESC. `host.h`: `g_tick`/window state are host infrastructure,
not `mem[]`.

`platform/res.c`: `res_alloc` is a bump allocator replacing `FUN_0001C308`
(no free/reuse); `res_resolve` ignores `0x1B544`'s `block+8` indirection and the
`0x1000000` lazy-load branch.

`platform/gra.c`: transparent runs write index 0 (conflating transparent with
entry 0); a zero-advance token returns −1; returns RLE bytes consumed instead of
writing into a caller pointer; resolves the pixel handle through `res_resolve`.

`platform/gfx.c`: the VBlank spin maps onto `gfx_wait_vblank` → `host_pump`;
extended-memory block-list walk replaced by `res_resolve`; unresolvable handle
skipped instead of trusted; DAC ports `0x3C8/0x3C9` → `gfx_dac[]`; the literal
`0xA0000` write redirected to `gfx_present` (aperture rule); VBlank wait → host
tick.

`platform/input.h/.c`: the key ring is C state, not a `mem[]` BIOS buffer;
overfull queue drops the oldest instead of refusing.

`game/flow.c`: BIOS/real-mode scratch base `0x3000000`; `0x4FBA2` int-10h query
returns a constant `0x13`; fatal init errors `exit(1)` instead of unwinding; no
VGA reset in `0x336C0`; `0x500C4` input via `host_pump`; the whole type-5 bank is
flattened (only the first 256 pushed); the title composite is a port choice; the
attract sub-machine is skipped (state 1 entered directly); the init chain's
`-f` argv probe, memory detect, DPMI locks, resource-file setup, audio, EMS block
list, int-10h set-mode, joystick init, MIDI memory and EEPROM read are stubbed;
the original tick-counter pacing is replaced by one 60 Hz host retrace; the
loop's deferred services (`0x292AC`, `0x389C4`/`0x38A38`, `0x134C0`, `0x38990`,
`0x4F644`, `0x11F28`, `0x10EE4`/`0x29D60`, `0x10DB0`/`0x10E18`/`0x2BF08`) are
no-ops; the state-machine cases for menus/attract/fight are empty.

`main.c`: the original has no headless mode — `run_check` presets `DS_000A81A8`
to run exactly one loop iteration per call.

## 6. `/* TODO(verify): */` doubts (4)

| Location | Doubt |
|---|---|
| `platform/gra.c:84` | The type-5 bank is returned flat; which record / DAC base index a sprite uses is unresolved. |
| `platform/gra.c:131` | The descriptor `x`/`y` anchor is not read here; its meaning (sprite origin vs bounding-box corner) is `likely`. |
| `platform/gra.c:136` | The negative-dimension sentinels are documented, not proven. |
| `host.c:26` | The interrupt vector that installs `0x2D62C` is still unknown (no static install site). |

## 7. Unresolved risks carried forward

1. **Tick interrupt vector.** The rate is settled at **60.05 Hz measured**
   (static ÷`0x3c` at `0x32B00` agrees; the `0x2EBD88` counter's page offset
   `0xd88` matches `DS_00105D88`, so it is *probable* but not proven to be that
   exact counter). The vector itself is unresolved: `0x2D62C` has one reference
   (the region lock) and no install site. Resolving needs an interactive DOSBox-X
   debugger breakpoint or a DPMI `int 31h AX=0205` trace.
2. **`fn_resolve` limitation.** It breaks if the original does arithmetic on
   code addresses. No ported path does; revisit for sub-project 5 jump tables.
3. **GRA constructs still `likely`.** Per-sprite sub-palette / DAC base index;
   the descriptor `x`/`y` anchor; the negative-dimension sentinel meaning; and
   the single exact-consumption miss (`S16TITLE` 74×167 at `0x349d7`).
4. **`int 10h` mode `0x13` (320×200) vs the installed `S16` (640×480) set**
   remains contradictory and unresolved.
5. **Emulator comparison is unusable, not merely approximate.** In ~4 minutes of
   the reachable attract, the game never presented the chosen `S16TITLE` frames:
   the capture's non-black colours overlap the 63-colour union of frames
   10/12/13/18 by **0 pixels (0.0000)**, and only 2 of 235 distinct colours appear
   anywhere in the 720-entry bank. Even unfiltered (`sdl output=surface`) the
   overlap is 0. The reachable attract is a text/credits composite, and synthetic
   input did not reach the game, so the title composite could not be driven in.
   **No emulator agreement is claimed anywhere.**
6. **Framebuffer reading is inferred-with-caveat.** The literal-target fact and
   the not-a-data-object fact are verified; the "VGA mode-13h aperture" reading
   is inferred from the non-relocated immediate plus the mode-`0x13` gate.
7. **Title state index 1 is `likely`, not runtime-confirmed** — the macOS
   DOSBox-X build refused the scripted debugger, so `DAT_000F0A64` could not be
   read live.
8. **`MEM_SIZE` was raised to 64 MB** to hold the 41.31 MB resource set; margin
   is 21.45 MB. A larger installed set would need this revisited.
9. **Test enforcement is partial.** See §8.

## 8. Reconciliation of the deferred minors

1. **`gfx_dac` width wording.** `gfx.h` and `main.c` said the DAC entries were
   "6-bit 0..63" while the code stores `(u8)(word >> 2)` — 8-bit 0..255. Both
   comments corrected to "8-bit guns"; the byte values were already right.
2. **Aperture wording.** `gfx.c` stated the literal is "the VGA mode-13h
   aperture" flatly; reworded to match the spec's verified-with-caveat reading
   (inferred from non-relocation plus the mode-`0x13` gate).
3. **Title frame-set wording.** `flow.c` and `game_flow.md` called
   `{10,12,13,18}` "a port choice, not derived from the binary" while also
   calling it "the four 320×200 descriptors". Corrected: the *set* is derived
   from the asset (its only 320×200 descriptors); *rendering them full-screen*
   is the port's choice in place of the original task-system composite.
4. **Tick rate wording.** `game_flow.md` line 83 said "measured" while open
   question 5 said "inferred". Reconciled: the rate is measured at 60.05 Hz with
   static agreement; only the *vector* is unresolved.
5. **Stale `--check` note.** `game_flow.md` said "`--check N` does nothing yet";
   replaced with the Task 15 outcome (headless, three artifacts, byte-exact
   against `tools/gra_render.py`, emulator unusable).
6. **Frame-10-only enforcement / stale artifact.** The suite now compares the
   four `--check` runtime captures (`frame_0001/0009/0017/0025.idx` = images
   10/12/13/18) against the decoder, not just frame 10; this was
   negative-controlled (corrupting `frame_0017.idx` → 1 FAIL). It cannot detect
   a *stale* artifact (it only runs when the file exists), and the Python-oracle
   comparison remains frame 10 only (there is no committed oracle for 12/13/18;
   those were verified manually in Task 15). Because the ladder runs `run_tests`
   before `--check`, a clean-tree ladder does not exercise the runtime
   comparison — it was exercised by running `run_tests` again after `--check`.
   The suite also does not assert the PPM/DAC conversion byte-exactness (that is
   manual), and the frame-10 Python oracle is not regenerated by any target.
7. **`host_read_file` ≥4 GiB regression.** The committed test used a 5-byte file
   with `max=4`, which the old narrowing bug also rejected, so it did not
   discriminate. Added a sparse-file assertion (`fseek` to 4 GiB+1, one byte) so
   `host_read_file(big, in, 4, &n)` must fail; with the old `(u32)sz` guard the
   suite aborts (exit 134), proving the test discriminates.

## 9. Honesty statement

The project may claim:

* the port's `INDEX`/LE/GRA/palette behaviour is byte-exact against the
  independent Ghidra image and the independent Python decoder, over the ranges
  stated above;
* the four chosen `S16TITLE` frames `{10,12,13,18}` are byte-identical
  (indices **and** RGB) between the port and `tools/gra_render.py`.

The project may **not** claim:

* that the port's title screen matches the original's title screen or palette —
  the original is a task-system composite the port does not reproduce, and the
  emulator comparison yielded zero overlap;
* a general byte-exact GRA/palette property beyond the stated full-screen,
  fully-opaque subset (the palette bank is flattened, sub-palettes are not
  selected);
* any emulator agreement at all.

The byte-exact comparison that exists is **port-rendered vs `tools/gra_render.py`**
for the same GRA frames — not port vs original.

## 10. Final status

Sub-project 1's definition of done is met except for the emulator leg of DoD 3
(pixels and DAC match the independent Python decoder; the emulator leg is
unusable and is not claimed) and DoD 6's open question of whether the original
handles ESC at the title (the port's ESC exit is a deliberate deviation).
Everything else on the path is either ported and verified or stubbed with a
sub-project marker. Sub-project 2a (audio/AIL) is implementation complete,
reported separately (`2026-09-16-audio-ail-port-report.md`) with its scope and
open items (audibility unverified, reconstruction unproven), and sub-project
2b-i (Smacker video) is implementation complete, reported separately
(`2026-09-17-smacker-video-report.md`); the remaining work is sub-project 2b-ii
(streamed audio), 4 (menus/EEPROM) and 5 (fight engine).
