# Spec: game_flow (top level)

Confidence: **verified** unless marked. Addresses are linear (LE code object
base `0x10000`); globals `DAT_0008xxxx` are `DS:xxxx-0x80000`.

## Entry chain (verified)

```
DOS4GW  -> LE entry 0x6245C   (jmp 0x624D4)
0x624D4 -> WATCOM _start      (PSP/env parse via int 21h/int 31h, builds argc/argv)
           call 0x6C484       ; CRT init (FUN_0006c484)
           call 0x6C435       ; -> C main, return value = process exit code
           call 0x6C4CF       ; CRT exit (FUN_0006c4cf)
0x6C435 -> FUN_00072C54, FUN_00072CA6, call FUN_0001BEC4 (game main), FUN_00062003
```

* `FUN_0001BEC4` (542 bytes, 24 callees) is the **game main**. `0x6C435` is the
  thin CRT `main` wrapper.
* `FUN_0004FBA2` = BIOS `int 10h` video-mode query (returns AL); `main` gates on
  it being `0x13` (VGA), else calls `FUN_0001D290` (fatal).
* `FUN_000109A0(ptr, size)` wraps `FUN_00010830(ptr, ptr+size)` and is called
  from `main` for several code/global ranges plus many other places (36
  callers) — it is a **DPMI region lock**; `FUN_000109CA`/`FUN_000108E8` is the
  matching unlock (29 callers).

## Engine model (verified / likely)

The game uses an in-house engine, not a third-party framework:

* **Resource manager** — `INDEX` is a 20-byte-record table; `FUN_0001B120`
  loads it, `count = size / 0x14`; `FUN_0001B544` resolves a handle
  `(index << 23) | byte_offset` to `entry.data + offset`. See `FORMATS.md`.
* **Extended-memory check** — `FUN_0001C0F0` probes extended memory
  (`< 1100000` bytes → error `"%s needs more extended memory.\nPlease consult
  the manual.\n"`) and builds a linked list of memory blocks beginning at
  `DAT_00101524`.
* **Screens/menus** — string table in the data object contains `"MENU"`,
  `"DIAGS"`, `"Null Menu"`, `"ADDRESS RAW DATA"`, `"SETTING CONFIGURATION
  DEFAULT VALUES"`, `"Round seconds)"`, `"Match seconds)"`,
  `"Selects SAURON BLIZZARD TALON VERTIGO ARMADON DIABLO CHAOS "`,
  `"Write EEPROM"`, `"Record bounds"`, `"High-score-table"`, `"OS: "`, `"MAIN: "`.
* **Character roster** — SAURON, BLIZZARD, TALON, VERTIGO, ARMADON, DIABLO,
  CHAOS (7 fighters).
* **Localisation** — `english.txt` / `french.txt` / `german.txt` /
  `italian.txt` / `spanish.txt` / `portugal.txt`; loaded via data-object
  string pointers (`0xC9418`+).
* **Video/audio** — `\RAGE.S16` path string; `RM.DRV` (DOS4GW RMI/DPMI), Miles
  AIL driver set (`DIG.INI`, `MDI.INI`, `*.DIG`, `*.MDI`, `FAT.OPL`, `FAT.AD`)
  and Smacker video (`twi5.smk`, `twg.smk`; SMK2, 320×200, silent — decoded by
  the port, sub-project 2b-i, report at
  `../../docs/superpowers/plans/2026-09-17-smacker-video-report.md`).

## Frame loop and process scheduler (verified)

`main` does not loop: it runs the init chain and returns after teardown. The
frame loop is reached through `0x20C10`:

```
0x1BEC4 main -> 0x20C10 -> 0x255CC  MASTER FRAME LOOP
  0x500C4   input / OS pump
  0x292AC
  0x389C4, 0x38A38   when DAT_00107A54 != 0
  0x24C5C   per-frame update
  process table 2: PTR_FUN_000A86C4 + bitmask _DAT_00104AEC (render)
   DAT_00104AF4++   frame counter
   0x134C0           (ported: `effects_step`, the effect list step/age)
   pacing compare DAT_0010150C vs DAT_00101508

0x24C5C   per-frame update
  DAT_000EF6DC++                          frame counter
  2 player records at DAT_001077E0, stride 0x94
  process table 1: PTR_FUN_000A8644 + bitmask _DAT_00104AE8 (update)
  0x11D04   STATE MACHINE: switch(DAT_000F0A64) cases 1..9, transitions via
            DAT_000F0A64 / DAT_000F0A6C / DAT_000F0A6A / DAT_000F0A6F / DAT_000F0A72
  0x11000   per-state render/play dispatch (13-case switch)
  0x1C740   VBlank-gated animation/FLIC blit (NOT the screen write; screen is 0xA0000)
```

* **Process tables** — two 32-entry `code *` tables `0x80` bytes apart, each
  gated by a `u32` bitmask: `PTR_FUN_000A8644`/`_DAT_00104AE8` (update, walked
  by `0x24C5C`) and `PTR_FUN_000A86C4`/`_DAT_00104AEC` (render, walked by
  `0x255CC`). This is the engine's extension seam.
* **Tick** — `DAT_00105D88` is incremented by the 9-byte handler `0x2D62C`
  (`DAT_00105D88++`); `main` locks that code page and the `DAT_00105D88` data
  page. **Tick rate = 60 Hz (measured + static).** Static: `0x32B00` converts a
  tick delta to seconds by dividing by `0x3c` = 60
  (`(… - DAT_00107478) / 0x3c`), and the per-player timer pair updated in
  `0x32970` wraps at `0xe10` = 3600 ticks (= 60 s = 1 minute), outer reset
  `0x383f` ≈ 14400 (= 4 minutes). Measured: in a live dosbox-x run the dword at
  physical `0x2EBD88` advanced 729 counts over a 12.139 s window (BIOS 18.2 Hz
  tick: 221) = **60.05 Hz**. That address's 4 KB-page offset (`0xd88`) equals
  `DAT_00105D88`'s (data offset `0x85d88`), so it is very likely
  `DAT_00105D88`; the runtime page map is not the LE page order, so the
  identification is probable, not certain. The counter kept running while the
  master loop was idle (all per-frame counters stayed 0) — consistent with an
  interrupt-driven tick.
  **Interrupt vector: unresolved.** `0x2D62C` has exactly one reference in the
  whole binary — `main`'s region lock; the game code (`0x10000..0x5FFFF`)
  contains **no** `int 21h`/`int 31h` instruction (all DOS/DPMI calls live in
  the `0x6xxxx+` runtime), so no install site names the handler. A dosbox-x
  `-log-int21` run showed only vector `0x15` get/set (DOS4GW's own hook) and
  no `int 21h AX=25` for a timer vector. What would settle it: break at
  `0x2D62C` in the dosbox-x debugger and read the vector/IDT entry, or trace
  the DPMI `int 31h AX=0205` call that installs it.
* **Pacing** — counter pair `DAT_00101508` (advanced asynchronously, presumably
  by the same tick source) and `DAT_0010150C` (loop-local), compared in
  `0x255CC`. `0x1C740` additionally busy-polls VBlank around the present work.
* **Screen surface** — `0x51F45` (called once from `main`) sets
  `DAT_000E87A0 = DAT_001014E4`, `DAT_000E87A4 = DAT_001014E8` (the two
  offscreen buffers) and builds a 200-entry dword row-offset table at
  `DAT_001088F8` (`0, 0x140, 0x280, …` = 320-byte scanlines).
* **Present / framebuffer write path (verified-with-caveat).** The screen-write
  target is the **literal address `0xA0000`** — the VGA mode-13h aperture — not a
  pointer and not a data-object address:
  * `0x255CC` (master loop): when `DAT_001014FC != 0`, copies 16000 dwords
    (64000 bytes = 320×200) from `DAT_000E87A4` to the literal `0xA0000`
    (`mov edi, 0xa0000`), then clears the flag.
  * `0x501A3`: dirty-dword blit — writes only dwords that differ between
    `DAT_000E87A4` and the flipped buffer `DAT_000E87A0` to the literal
    `0xA0000` (`mov ebx, 0xa0000`).
  * `0x50188` swaps `DAT_000E87A0` ↔ `DAT_000E87A4` (double buffer).
  * `main` gates on `int 10h` mode `0x13` (320×200×8); no `4F00`/`4F01`/`4F02`/
    `4F05` VBE call exists anywhere in the decompilation.
  * `0x1C740` VBlank-gates (`in(0x3DA) & 8`) a blit of `DAT_000E87A4` through
    `FUN_00065340`; this is an animation/FLIC-style blit, **not** the screen
    write.
  The *literal-target* and *not-a-data-object-address* facts are verified; the
  "VGA aperture" reading is inferred from the literal being non-relocated plus
  mode `0x13`. See the conflict below.

* **Aperture conflict with the port's memory model (verified) — the rule.**
  `0x501A3`/`0x255CC` write `0xA0000` as a **literal immediate**, and no LE
  fixup covers it (the nearby fixup at `0x501AC` relocates only the
  `DAT_000E87A4` operand). Non-relocated means **not** a data-object address:
  it is the VGA aperture. Runtime proof: the image is relocated at load (data
  pages live around `0x266000`+ — the bit-expansion LUT `00 80 c0 e0 f0 f8 fc
  fe`, stored in the file at data offset `0x21420`, is at physical `0x287420`),
  and a dosbox-x memory-file dump shows **`0xA0000..0xAFA00` all `0xFF`** — the
  data object is not mapped there.
  In the port's flat model the data object *is* mapped at `DATA_BASE 0x80000`,
  so `mem[0xA0000]` **is** data-object offset `0x20000`, which holds **live
  engine data, not a screen**: the 8-dword pointer table `PTR_DAT_000A1290`
  (file `0xA1290` = `90 f0 01 00 …`), bit-expansion/step LUTs at `0xA1420`
  (`00 80 c0 e0 f0 f8 fc fe …`), `0xA163C`, `0xA173C`, `0xA769C`, `0xA7BC0`,
  read via `FUN_00018950`/`FUN_00015F48` and callers at `prage.c:5608`, `4522`,
  `4386`, `3657`, `3710`, `9034`, `11200`, `25199`.
  **Rule: screen output must never target `mem[0xA0000]`.** The port keeps its
  frame buffer outside the mapped data object; `gfx_present()` renders the
  320×200 index buffer to the SDL window. Task 14 must not "port" the `0xA0000`
  copy as a `mem[]` write.
* **Palette** — `0x1C470` flushes a 4x`u32`-record dirty-list at
  `DAT_00107498` (head `DAT_00107798`) to the VGA DAC, VBlank-gated. Records:
  `[0]` colour ptr or resource handle, `[1]` first DAC index, `[2]` count,
  `[3]` non-zero -> `[0]` is a resource handle resolved by `0x1B544` **+4**.
  Colours are 8-bit guns at R=bits[2..9], G=bits[10..17], B=bits[18..25].

## Sprite compositor — `0x14328` (sub-project 4a-i, ported)

On-screen sprites are a **three-stage pipeline** (verified by disassembly):

1. **Actor update + pset sync** — `0x2A31C` walks the actor list and `0x2A1FC` →
   `0x2A820` writes each actor's position, layer and frame id into a 0x20-byte
   **pset** entry in the pool at `DAT_001014EC`. **Ported in sub-project 4a-ii**
   (`port/src/game/actors.c`, `actors_update`).
2. **Ordering** — `0x255CC` calls `0x1C3FC`, an insertion sort of the
   singly-linked display list at `DS_00105B44` ascending by `pset->layer`
   (`word` at `+0x0E`), stable; the sorted insert `0x1C3A0` places a node and the
   580-node pool lives at `DS_0010153C`. Ported in
   `port/src/platform/render.c` (`render_list_init/insert/remove/sort`).
3. **Composite** — `0x14328` walks the list, builds a 0x40-byte display node per
   entry via `0x14268`, and projects the pset position with
   `p = v*3901 + 0x800; if (p < 0) p += 0xFFF; proj_x = p >> 12` (same shape with
   3414 for `proj_y`). This is **not** round-half-away-from-zero: at negative
   exact values it rounds toward +infinity, so `proj_x(-4096) == -3900`, not
   -3901. Only positions are scaled — sprites blit 1:1. The three mode-offset
   projections (layer-1 x `DS_00107A3E`, layer-2 x `DS_00107A3A`, layer-2 y
   fallback `DS_00107A38`) instead use the **uncorrected**
   `(v*num + 0x800) >> 12`. It then applies the clip rectangle and the
   layer-1/layer-2 mode rules, and calls the span blitter `0x51E5C`. The blitter
   resolves the pixel handle and palette bank and dispatches through
   `PTR_LAB_00080C8C` to a renderer that writes `mem + DSD(DS_000E87A4)`.
   Ported in `render.c` and `port/src/platform/sprite.c`.

The clip rectangle is the literal `{0, 0, 320, 200}` of the master loop's camera
struct `DS_000A87CC + 8`, not a runtime camera. The port wires `render_list_init`
into `game_init` and `render_list_sort()` then `render_list()` into `0x255CC`'s
order (after the render process table, before `0x1C470`'s palette flush); with an
empty list the wiring is a no-op. Ownership: `render.c` owns the list and the
projection/clip driver; `sprite.c` is a pure raster unit that consumes a node.

**Proven vs unproven:** the RLE span renderer is byte-identical to
sub-project 1's `gra_decode_frame` on 32 real sprites (row-wise at each sprite's
own width); the clipped, mirrored and mode-1 shear renderers are proven by
hand-computed tests and negative controls, **not** by an emulator. The clipped
renderers' window-intersection formulation is a `PORT` deviation from the
original's six straddle branches. `DAT_00081310[0] == 0x0005D110` (a stale code
pointer; the port maps bank byte 0 to no offset) and `RAW+HFLIP` (type `0x0A`, a
no-op) are pinned. Full record:
`../../docs/superpowers/plans/2026-09-17-sprite-compositor-report.md`; format
notes in `../../FORMATS.md` ("Sprite compositor").

## Boot logos and the attract subsystem — `0x11000` (sub-project 4d, ported)

`FUN_00011000` case 0 (the attract sub-machine's entry) plays the two boot
Smacker movies through two `0x1C740` calls before assigning title state 1.
`0x1C740` is the VBlank-gated FLIC-style blit of `DAT_000E87A4` listed above;
its movie path is a licensed Smacker-library open/decode loop (`0x6345C`
open/decode, `0x63180` stream setup). The port ports the player (video only)
and wires `movie_play("twi5.smk")` then `movie_play("twg.smk")` at that case-0
site (`port/src/game/attract.c`, `attract_step` phase 0), decoding into
`DAT_000E87A4` and presenting through `gfx_present`. The original presents TWI5
120 of 121 frames and TWG 41 of 41; the rule that decides presentation is the
player's and is content-based (`TODO(verify)` on the original loop's exact
semantics). Streamed Smacker audio is sub-project 2b-ii and not ported. See
`../../docs/superpowers/plans/2026-09-17-smacker-video-report.md` and
`../../docs/superpowers/plans/2026-09-19-attract-report.md`.

Since 4d the port **boots through state 0**: `game_state_init` (`0x10E80`)
enters state 0 and no longer plays the logos by hand or forces state 1. The
attract subsystem is owned by `port/src/game/attract.{c,h}`:

* `attract_step` (`0x11000`), the 13-phase machine. The boot cycle runs phases
  0, 1, 2, `0xC` (its countdown), 3–9, `0xC` and `0xB`; phase 0xA is the
  `DS_000F0A5C != 0` arm of phase 9 and is skipped until a later cycle because
  phase 2 wraps `DS_000F0A5C` 4 → 0.
* `attract_state_reset` (`0x10EE4`), `attract_voice_tick` (`0x10F28`, the two
  signed countdowns that reload as `rng_next(N)+N`; its `0x2C3FC` voice calls
  are declared no-op stubs), `attract_scene_tick` (`0x292AC`) and
  `attract_config_volumes` (`0x2C8F0(-2)`), plus the per-state tails
  `frontend_pause_tail` (`0x10DB0`) and `frontend_continue_tail` (`0x10E18`).
* `port/src/platform/render.c` owns the attract scroll/zoom projection
  `render_scroll_edge` (`0x389C4`) and `render_scroll_fill` (`0x38A38`) because
  it writes compositor projection state; `flow.c` keeps only the state dispatch.
* **The row-1 producer.** Attract phase 2 calls `config_set_credit_row(1u)`
  (`0x2C06C(1)` at `0x110CE`), the captured title's credit row. The init-time
  `DS_00105C05 = 1` stand-in is deleted; `game_init` keeps only
  `config_set_credit_row_init()` (`0x20CCC` → `0x1D`).
* **Handoff.** Phase 0xB with `DS_00108173 == 0` and `DS_000F0A5C == 0` sets
  `DS_000F0A64 = 1` (title); the boot cycle therefore reaches the title.
* **The attract palette drivers — ported (small-fidelity-gaps cycle).**
  `attract_scene_tick`'s only shipped callee `0x4F7F4` (the `DS_000A8744[0]`
  effect-palette driver: it advances `DS_001088F1` through the 10-entry table at
  `DS_000C98A0`, enqueues each entry via `0x33874`, and clears `DS_00104AD0` bit
  0 after 10 frames) is ported as `attract_palette_advance` (registered into the
  dispatch), as is the starter `0x4F83C` (`attract_palette_start`) and the
  `0x33874` reflow (`palette_reflow`). Neither driver runs at runtime: the port
  never sets `DS_00104AD0` bit 0 (its image value is 0 and only `0x4F83C` sets
  it, whose `0xE8916`-table callers are unported), so the attract's palette
  animation does not render and the attract oracle's first divergence
  (`tools/attract_compare.py`, capture frame 215 raw 2180/2175) is unmoved. Its
  owner is the loader-frame DAC state / read-gate model (record §3.5), not these
  drivers.

The attract and the title share one `game_init()` run: `PR_ATTRACT_DUMP`
dumps every presented state-0 frame to `<dir>/attract/` and the post-attract
title window to `<dir>/title/`, and `test_title_window` drives the state
machine from boot to the title. Because the attract consumes the shared RNG
stream, the driver re-seeds `0xABCD` immediately before the title entry to
reproduce `tools/title_pin.py`'s hardcoded draws (12, 111, 0) — the capture's
pin decouples the title draws from the attract's RNG state. `make verify` runs
`--check 820` so the headless smoke test crosses the attract into the title.

## Title state — `0x121A0` (sub-project 4a-ii, ported)

* **Title/attract state is index 1.** `FUN_000121A0` is case 1 of
  `switch(DAT_000F0A64)`. The attract sub-machine `FUN_00011000` case 0xb
  assigns state 1 when its cycle counter `DAT_000F0A5C == 0`; since 4d the port
  boots through state 0 and reaches state 1 from the attract, as the original
  does. That the shipped title *is* state 1 is confirmed at runtime by the
  oracle driver, which asserts `DS_000F0A64 == 1` and `DS_000F0A66 == 0x600` on
  the entry frame (now driven post-attract by `test_title_window`).
* **Phases on byte `DS_000F0A6F`:**
  * **0**: `0x4F1E4`, `0x2BAF4` (`actors_reset`), `0x38910`; the branch on
    `DS_00104528 & 0x200` — clear takes `0x1C500` + `0x2F198` (the caption/text
    path, the shipped one because `DS_00104528 = 0x2D974(0x29) = 0x142095` and
    `& 0x200` is clear), set takes
    `0x2AE14(0x9AE3C)` (mode-1 sprite, unreachable on the shipped profile); four
    `0x38B18(0x9AC1C)` rows; three `0x5D7DC` draws (ranges `0x5A`/`0x7E`/`2`)
    that fix the logo's start X, speed and gravity sign and `DS_00107A50`;
    spawns the logo `0x9AC30` and the second object `0x9AC94`; sets
    `DS_000F0A66 = 0x600`.
  * **1**: `DS_000F0A66 -= 0x10` per frame; at `<= 0x10` it releases the caption
    cells and the two records, spawns `0x9ACA8`, and runs the `0x33904` retire
    walk; then `DS_00107A50 += (logo+0x32 >> 16)/2` and
    `logo+0x2C = 0x40000 / DS_000F0A66`.
  * **Exit**: `DS_000F0A6F = 0`, `DS_000F0A64 = 2`.
  * **Always**: `DS_00107A3A = DS_00107A50 >> 5`.
* **The pin.** `0x121A0` has no store to `DS_000EF6D8`; the only `0xABCD` seed
  store is `0x20C62` in `0x20C10`, transcribed as `game_init`'s `rng_seed`. The
  three draws are therefore the first three from `0xABCD` and land on
  `12`/`111`/`0`, giving `iVar1 = 12`, `iVar2 = 0x1E40`, `DS_00107A50 = 0x2420`,
  `DS_00107A3A = 0x121`, `logo+0x34 = -81`, `logo+0x36 = 8`, `logo+0x2C = 0xAA`.
  The non-zero `iVar1` is what makes the logo move. The capture-only pinned
  `PRAGE.EXE` patches only the four behaviour sites — those three draws plus the
  anim opcode-8 site — in place so the original produces the same values; there
  is no stub or instrument in the port (`tools/title_pin.py`). 4a-iii removed a
  former fifth site that `ret`'d `0x2BF08`, so the oracle now compares the true
  original.
* **The caption.** `0x1C500(0x15)` → `0x474E4` decodes string id `0x15` from
  `ENGLISH.TXT` (`THE FUTURE...`) into `DS_00102760` via the `0x1E75C`/`0x1E808`
  lock pair. `0x47370`'s paged-memory loader is replaced by a direct
  `ENGLISH.TXT` read into the buffer at `0x3800000`.
* **The message overlay `0x2BF08` (4a-iii, ported).** The `0x11D04` tail's tick
  (`flow.c`, `game_overlay_step`) draws a centred `sprintf("%s:%d",
  game_string_get(0x46), DS_00105C00)` → `CREDITS:5` on every frame from the
  `DS_00105D60 == 0` / `DS_00105C00 != 0` branch (no `&0x1F` gate; the raw
  `0x2BF7D`–`0x2BFCD` arm). `FUN_0002CAA8` is `DS_00105D60 == 0`; the
  `FUN_0002F198`/`FUN_0002F4BC`/`FUN_0002F280` text trio is already ported.
  `DS_00105C00` is now derived by `0x2C304` from the config bundle's field `0x29`
  (see the EEPROM/config section), so it renders the captured `5` without a seed.
   `DS_00105C05` (text row; screen rows 7–12) is written by the init chain at
   `0x20CCC` (`0x2BF00` → `0x1D`); the captured row 1 comes from `0x2C06C(1)`
   inside the ported attract machine `0x11000` (phase 2), so the former
   init-time `DS_00105C05 = 1` stand-in is gone. The credit countdown
   (`FUN_0002CA48`/`FUN_0002CA7C` via `0x11F28`) is now ported and wired through
   `game_state_step`'s coin poll (4b-B). Diagnosis:
   `../../docs/superpowers/plans/2026-09-18-bf08-overlay-diagnosis.md`.
* **The effect list `0x13xxx` (4a-iii/4a-iv, ported).** `port/src/game/effects.{c,h}`
  owns the two sentinels `DS_000FCCE0`/`DS_000FCCE8`, the lock `DS_0009AF3C`,
  the count `DS_0009AF3D`, the intrusive link primitives (`0x249B0` insert-after,
  `0x249C0` insert-before, `0x249D0` unlink), the free-list build `0x13ADC`, the
  per-entry teardown `0x13420`, the clear `0x13DF0`, and the step/age `0x134C0`
  (`effects_step`, called from the master loop at `flow.c:779`).
  `actors_reset` calls `0x13ADC`/`0x13DF0` at `0x2BBB8`/`0x2BB2B`; the title's
  `0x123EA` calls `effects_spawn(node, 3u, 0x419786C)`. The producer set is
  complete: **`0x13C70` type 3**, **`0x13D4C` type 4** (`effects_spawn_darken`),
  **`0x13E28` type 6** (`effects_spawn_pulse`) and **`0x13B3C` types 0/2**
  (`effects_spawn_scroll`) are the only functions that take a free record and
  write its type byte, so **types 1 and 5 have no producer and are dead**. The
  `0x13B3C` producer is itself **dead**: it has zero callers and zero
  cross-references anywhere in the image (`ghidra_get_xrefs_to 0x13B3C` = 0;
  `prage.functions.csv` `FUN_00013b3c` `n_callers = 0`; the bytes `3c b3 01 00`
  occur nowhere — the earlier "live via the jump table at `0x23AC4`" misread that
  table's `0x23B3C` dwords as `0x13B3C`). The port's `effects_spawn_scroll` stays
  faithful and test-driven, like the types-1/5 step bodies. An
  end-to-end unit test proves spawn → `effects_step` → palette dirty list →
  `gfx_dac` (the palette actually presented through `gfx_present`).
* **The camera/scene layer — `0x1324C` ported, the camera chain now ported too
  (small-fidelity-gaps cycle).** `0x1324C` (screen-shake decay, update-table
  entry 0) lives in `effects.c` and maintains `DS_000F0AF4`/`DS_000F0AF6`. It
  draws nothing: the plan's assumed "missing draw" half of the effect render path
  **does not exist**, and none of the camera/scene functions draws — the state is
  consumed by the existing render pass (`0x14328`) and the actor-pset sync.
  `0x1324C` is **dormant**: no shipped store sets `DS_00104AE8` bit 0, so it never
  runs; it is registered only so the existing update-table dispatch reaches it if
  bit 0 is ever set.
  **Ported:** `0x12CD4` (the camera-y stepper, inlined into `camera_y_clamp`),
  `0x1317C` (the camera-y clamp, with its `0x131F8`/`0x13224` tails), `0x13290`
  (mode-2 two-player centering), `0x1333C` (mode-3 one-player centering), the
  `0x12D48` dispatcher (modes `0x12DF0`/`0x12E3C`) and `0x12DA8`
  (`camera_y_commit`) now live in `port/src/game/camera.c`; `flow.c:1346` calls
  `camera_dispatch()` at the raw's `0x25422` site. The raw gives `0x12CD4`
  exactly one caller (`0x1317C` at `0x131CB`), so the chain is self-contained.
  The mode-1 `0x18714` updates and `0x17580`'s `0x140E4`/`0x170A0` tails remain
  named gaps inside the ported functions (`camera.c`'s `/* PORT: */` notes).
  **The effect call sites are deferred too (front-end-chain Task 8).** The plan
  said `0x29B74` and `0x41578` register into the process tables; the raw refutes it.
  `0x29B74` is the mode-`0x17` handler stored at `DS_00104AE4`. The raw constant
  `74 9b 02 00` occurs **five** times, each storing `0x29B74` into `DS_00104AE4`:
  `0x2788B`→`0x278A4` in `FUN_000277C0` (mode `0xF`); `0x2861E`→`0x2862A` in the
  un-emitted region starting at `0x2861C` (after `FUN_00028468`'s `ret` at
  `0x2861B`), which has **no callers** anywhere in the image and whose address
  appears nowhere as a pointer; and `0x28978`→`0x2898F`, `0x28A9D`→`0x28AB9`
  (`mov edi`), `0x28B3C`→`0x28B57` (`mov esi`), all three in `FUN_00028788`
  (mode 9). It is dispatched by six
  `call dword [0x104AE4]` sites (`0x4F302`, `0x4F373`, `0x4F6F1`, `0x4F70D`,
  `0x4F9AA`, `0x4F9D1`) plus one direct `call 0x29B74` at `0x27B17`; `0x41578` is
  direct-called from four sites (`0x41755`, `0x41DE6`, `0x42337`, `0x42352` in
  `0x416D4`/`0x41C28`). Every one of the live sites is reached only through
  `0x24C5C`'s **unported mode cases** (`0x12`, `0x13`, `0x16..0x1B`, `0xE`, `0xF`, `9`) and
  the unported match/fight chain; the `0x2861C` region is dead outright. The port's
  `DS_00104B00` is fixed at 3 by `0x10E80`, so none
  is reachable from the ported states 3/4/5. They are **deferred and unowned by
  this plan** — no dispatch path is shipped, and `port/tests/test_game.c` pins
  that no handler is registered and that state 5 neither arms `DS_00104AE4` nor
  leaves mode 3. `0x41578`'s register-level comparison against `0x88874B0` is
  **dead in the port's flat model**: `0x88874B0` is above `MEM_SIZE`
  (`0x4000000`) and outside both LE objects, so that half of the predicate can
  never match. Because the four `0x41578` sites are themselves unreachable, no
  port code holds the comparison; the raw fact is recorded here rather than
  deleted or substituted. **A later cycle that makes `0x41578` reachable must port
  the comparison with it, including the never-true `0x88874B0` half** (register-
  level fidelity — not deleted and not substituted). No shipped path spawns types
  0/2/4 yet (`0x13D4C`'s callers `0x29B74`/`0x41578` are unported; `0x13B3C` is
  dead — see the producer-set bullet above); type 6 (`0x13E28`) **is** spawned
  from the ported select state (`flow.c:367`). The remaining producers
  remain a coverage gap carried by unit tests. Details:
  `../../docs/superpowers/plans/2026-09-20-frontend-chain-derivations.md` §5,
  `../../docs/superpowers/plans/2026-09-19-effects-producers-report.md`.
* **`--check N`** (Task 15) runs exactly N master-loop iterations headless and
  writes `frame_NNNN.ppm`/`.pal`/`.idx`; exit code is the assertion-failure
  count. It is no longer used as a title oracle — `make title-oracle` is.
* **The presented buffer is redrawn every frame** (`0x255CC` swaps every
  presented tick, so a hold frame that skipped the redraw would present a blank
  buffer).

Full record: `../../docs/superpowers/plans/2026-09-17-actor-system-report.md`.

## EEPROM/config core — `0x2Dxxx` (4b-A, ported)

`port/src/game/config.{c,h}` owns the packed config field layer: `0x2D974`
(`config_field_get`), `0x2DA0C` (`config_field_set`), `0x2CCD0`
(`config_menu_default_bits`), `0x2CADC` (`config_set_defaults`) and `0x2D6F8`
(`config_validate`). The 63-entry descriptor table (obj-0 `0x2D300`) and the
config byte region (`DS_00105D88`) are read out of the loaded image; no table or
value is transcribed. With the storage layer (`0x2E990`, `0x2D638`, the `0x80CE4`
image, `0x2D4EC`) declared a no-op, validate always takes the defaults path, as on
a fresh machine, writing field `0x29 = 0x142095`, fields `0x35`/`0x37 = 0xA0`,
field `0x2A`'s low two bits `= 3`, and the `0x9C94D2C4` magic.

**Consumers (`flow.c` `game_init`).** `config_validate()` is called at the
`0x20C5D` block; the master init `0x2F9CC` (whose `0x13ADC` effects_init already
ran inside `actors_init`) invokes it before that block in the raw. The block then
reads `v = 0x2D974(0x29)` and derives `DS_00104528 = v`,
`DS_00105B3A = (v & 0x100) >> 4`, `DS_001088D0 = (v & 0xF)*5 + 0x1E`,
`DS_0010452C = (v & 0xF0) >> 4` (raw `0x20C5D`–`0x20CC2`), plus `0x2C304`'s
`DS_00105C00 = ((0x2D974(0x29) & 0xF0000) >> 16) + 1 = 5` (called from `0x10E80`
at `0x10ECC`). The outer wrapper `0x20C10` is not transcribed as one function;
its `0x2F9CC` and `0x10E80` calls fold into `game_init`/`game_state_init`.

**Declared gaps.** No storage I/O (the save/load path and the `0x80CE4` image)
and the deferred module taps `0x1AE20`/`0x2EA78` (screen setup, storage write).
The credit layer (`0x2CA48`/`0x2CA7C`/`0x2C060`/`0x2C06C`/`0x2BF00`) and its
caller `0x11F28` are ported in 4b-B (see "Front-end input, credits and the
select state"). Full record:
`../../docs/superpowers/plans/2026-09-19-eeprom-config-report.md`.

## Front-end input, credits and the select state (4b-B, ported)

Three layers complete the front-end path from the host keys to the state-2
selector.

* **Input bitfield** — `port/src/platform/input.c` owns the ported sampler.
  `0x500C4` (`input_pump`) combines the BIOS key bytes at `DS_00101514`+`0x2d8`
  (scan) and `+0x2d9`, keeps the previous level for every bit that changed that
  frame (the raw's one-frame debounce), then runs the `0x50130`/`0x5010F` repeat
  timer. `0x50161` (`input_select_bits`) is the level/edge selector sharing the
  `DS_000E1C38` latch. `0x4F644` (`input_state_update`) builds the newly-pressed
  mask `DS_001088E4` (the `0xFF00FF00` family) and the held mask `DS_001088D8`
  (the `0x00FF00FF` family) plus the two packed cursors `DS_001088E0/2`.
  **Host binding:** `game_loop` fills the key bitmap from `host_key_bits()`
  (host.c's `k_input_bind` table; bit 0 = coin) immediately before `input_pump`;
  `game_frame` calls `input_state_update()` at the `0x24C6E` site as its first
  action — the raw's `0x4F644` precedes the frame counter and the process
  tables — so the masks are fresh for `game_state_step`.
  `0x50146`'s repeat-timer setter has no port caller and stays inert.
* **Credit layer** — `port/src/game/config.c` ports `0x2CAA8`
  (`config_not_free_play`), `0x2CA2C` (`config_has_credit`), `0x2C060`
  (`config_credit_ready`), `0x2CA48` (`config_credit_take`), `0x2CA7C`
  (`config_credit_spend`), `0x2C06C` (`config_set_credit_row`) and the raw init
  writer `0x2BF00` (`config_set_credit_row_init`, `DS_00105C05 = 0x1D`).
  `DS_00105C00` is the credit count the overlay renders.
* **The coin poll** — `0x11F28`, ported as `frontend_coin_poll` in `flow.c`.
  It requires a credit (`0x2C060`), tests the event code's dword in the
  `DS_0009ACBC` table against the newly-pressed mask `DS_001088E4`, and debits
  one through `0x2CA7C`. `game_state_step` calls it with codes 0 (`0x11D15`,
  `accepted |= 1`) and 1 (`0x11D28`, `accepted |= 2`). **The raw returns from
  `0x11D04` when either is accepted** — it calls `0x32970(0)` then
  `0x257a4(accepted)` and skips the state dispatch for that frame; both divert
  handlers are unported (4b carve-out) and the port returns there.
* **Select state** — `0x11F6C` (`game_state_select`, case 2), the six-entry
  carousel, with `0x33904` (`frontend_list_next`) and `0x1C6D4`
  (`frontend_resource_known`). Phase 0 writes config row 1
  (`config_set_credit_row(1u)`) and falls through into phase 1 (no jump between
  `0x11FD4` and `0x11FDA`); phase 4 tests the **original** count value
  (`test ax,ax; jg`), so it advances one frame later than a prefix decrement.
  **Exit:** phase 3 sets `DSW(DS_000F0A64) = 3` (literal 3) and
  `DSB(DS_000F0A6F) = 0`; state 3 is `0x12484` (ported, see below). Derivation
  and frame arithmetic:
  `../../docs/superpowers/plans/2026-09-19-frontend-input-derivations.md`.

## Front-end states 3/4/5 (front-end-chain Tasks 3/4/6, ported)

`game_state_step`'s cases 3/4/5 are ported; the port now advances
carousel → 3 → 4 → 5 → 9 → 6 unaided. State 5's `0x1EA08` site is wired; state 6
is the attract demo fight's entry, ported by the demo-fight cycle 1 (see the
states 6/7 section below). Each state is a phase machine driven by its own
`mem[]` counter, transcribed phase-for-phase from the raw.

* **State 3 — `0x12484` (`game_state_3`).** Phase 0 (`DS_000F0A6F == 0`):
  `actors_reset` (`0x2BAF4`), four `frontend_spawn_row(0x9AC1C)` corner rows, a
  type-3 `effects_spawn` for the live list entry whose handle is `0x3E688`, the
  `DS_00104528` bit-1 branch (the `0x1C500`/`0x2F198` text pair, or a `0x2AE14`
  spawn into `DS_000F0A40`), then the `0x12658` handoff (`game_state_3_handoff`):
  three `0x2AE14` spawns, `DS_000F0A58` = the first, the two `+0x4B`/`+0x56`
  field copies, the list walk spawning a type-3 effect per
  `*rec != 0x3E688 && !frontend_resource_known(rec)`, and `DS_00107A44` zeroed.
  Phase 1 tracks the handoff actor's `+0x1C` offset and terminates into state 9
  when it reaches `0x1E00`.
* **State 4 — `0x11578` (`game_state_4`).** Phase counter `DS_0009AD98`.
  Cases 0/1/2 are the unrolled credit-roll pages of **8, 12 and 13** `0x2F4BC`
  (`text_cursor_hold`) calls — the counts are literal in the raw, not a loop
  bound — each spawning the `0x9AD84` descriptor, arming the 180-frame timer
  `DS_000F0A76 = 0xB4` and the continuation phase `DS_000F0A74` (1/2/3), then
  entering phase 4. `0x2C06C` is called in case 0 only, as the raw does. Phase 4
  counts the timer down and continues on the frame the pre-decrement value is
  zero; phase 3 hands to state 9 with `DS_000F0A6C = 0`.
* **State 5 — inline in `0x11D04` case 5.** `frontend_match_start` (`0x1EA08`),
  `config_set_credit_row(0x1D)` (`0x2C06C`), then the raw's five stores in raw
  order: `DS_000F0A6F` (`0x11E11`), `DS_000F0A72` (`0x11E17`), `DS_000F0A6A`
  (`0x11E1D`), `DS_000F0A6C` (`0x11E2E`), `DS_000F0A64` (`0x11E35`). The `0x2C3FC`
  voice cancel and the `0x32970` run clock are out of scope and skipped.

**Gaps this section leaves.** `frontend_match_start` is ported only through its
three pinnable pre-resource calls (`0x4F1E4`, `0x2BAF4`, `0x38B18(0xA7B6C)`); the
`0x2DBC4`/`0x2DB58` paged-resource blob, the `local` character index, the
`0x2F4D0`/`0x2F4BC` formatted draws and the `0x2AE14`/`0x2A17C` spawn are a named
gap (derivations §7.1/§7.2) — the port does not model the resource reader, so
spawning a descriptor would be a fitted constant. `0x1EA08`'s other four call
sites are the **match cycle's** and are unwired: `0x11A42` in `FUN_00011A30` (a
dead copy — `0x11A30` is referenced nowhere in either LE object) and `0x1F140`/
`0x1F278`/`0x1F39B` in `FUN_0001EEB0` (the match sub-state machine, cases 2/6/9).
No task in this plan owns the match cycle. States 6/7 (the attract demo
fight: `0x11A8C`, `0x263F4`) were ported by the demo-fight cycle 1 (see the
next section); state 8 (`0x33F08`'s run clock is not it — 8 is the run clock
plus the `0x257A4` coin divert) and state 9's semantics beyond the countdown
handoff (`0x10EE4`'s `DS_000F0A71` arm) remain the **next cycle**. The countdown
itself is ported and faithful; the state-9 hold's render divergence is in the
demo-fight section below.

**States 3/4 pixel oracle: enforced gate (front-end-chain Task 5, closed).** The
120 s pinned capture (`make frontend-capture`: `data/title-captures/frontend`,
3617 distinct post-logo frames, raw 1367..8409 after the demo-fight closure
cycle's re-capture) reaches the front-end. With the state-3 render ported
(`0x12484`) the `PR_FRONTEND_DUMP` driver emits the real zoom-out, and
`tools/title_compare.py --frontend` aligns it: window distinct **[560..865]**
(raw 3108..3772), **306 frames: 137 clean, 165 splice, 0 transition, 2
unexplained** (the four classes sum to 297: the window's other two frames are the
all-black captures 561 and 831, excluded as artifacts — the oracle's own list) — captures **832** and **833**, allowed by name in
`FRONTEND_ALLOWED_UNEXPLAINED` with their reason (`title_compare.py:352-374`;
the arena-backdrop cycle's absorbed claim move: the window is derived from the
port's own dump, so a correct arena render necessarily extends it — the fix
explains captures 834..842). Any other unexplained frame still fails. (The demo
section below records why the window indices moved from the pre-Task-9
`[557..813]` / 257 frames; the arena-backdrop outcome section below records the
`[560..830]`/271 → `[560..842]`/283 move; the demo-pose cycle's `0x3A43C`
stack-offset fix and `0x35829 0x186C4` re-latch then explained captures
843..850: `[560..842]`/283 → `[560..850]`/291; the roar-timing fix (the
`0x3AD27` pose-setter operand) then explained 851..857: → `[560..857]`/298;
the frame-858 fix (`0x2A690`'s mode-1 x operands) then explained 858: →
`[560..858]`/299; the frame-859 fix (`0x49C78`'s case-1 walk arrival) then
explained 859: → `[560..859]`/300; the frame-860 fix (`0x49C78`'s tail reset
`0x4A634`) then explained 860..863: → `[560..863]`/304; the frame-864 fix
(state 6's `0x12750` node list and the driver's frame-counter seed) then
explained 864/865: → `[560..865]`/306.) The claim that result supports is precise and narrow: **no
content-bearing capture frame inside the window the port's own dump exhibits is
unexplained** (the two named exceptions aside). The window is derived from the port's dump (`check_capture`'s
`idx`→`a,b` mapping) and the branch discards `check_capture`'s `rc` (coverage and
`endpoints BAD`), so `unexplained` is only ever evaluated inside that
self-derived window. The oracle therefore **cannot detect a port that
under-renders the front-end**: a dump of 300 identical copies of port frame 0
(nothing rendered past the state-3 entry) exits 0 (window `[557..559]`, 2 clean,
0 unexplained, exhibited 1/300), and a dump of only the first 12 real frames
exits 0 (window `[557..569]`, 0 unexplained, exhibited 9/12). `frontend-oracle`
keeps enforcing the gate (the Python compare exits non-zero on any unexplained
frame outside the two named) and remains a `verify` ladder step; it was proven
to fail on an induced regression (exit 1 — the unexplained count is
corruption-dependent, not a fixed 2).

Task 5 found and fixed a real port bug: the raw's `actors_reset` (`0x2BAF4`)
calls `0x38B70` at `0x2BBDA`, which zeroes the 7-entry front-end row table
`DS_00107A1C` that `0x38B18` (`frontend_spawn_row`) fills; the port skipped it
(actors.c's old comment misread `DS_00107A00/1C` as input state). Without the
clear the table only accumulated, so in state-3 phase 0 all four corner-row
spawns found the table full (`slot == 7`), `0x3E688` was never palette-acquired,
and `0x12484`'s `effects_spawn(0x3E688, 2, …)` — the palette-driven zoom
background — never fired. `actors.c` now ports `0x38B70` as
`actor_cursor_reset()` and calls it from `actors_reset`; the front-end window
grew from the 3-frame boundary island to the full zoom (257 frames at the time;
254 in the Task 9 re-capture), port frames exhibited 1/300 -> 234/300, and the
title, attract, smacker and C-vs-Python oracles are unmoved. No `title_pin.py`
pin was added by that fix.

**State 5 (`0x11D04` case 5) store order is a declared, unassertable fidelity
property — verified by the raw, asserted by nothing.** State 5 transcribes the
five stores in the raw's order: `DS_000F0A6F` (`0x11E11`),
`DS_000F0A72` (`0x11E17`), `DS_000F0A6A` (`0x11E1D`), `DS_000F0A6C` (`0x11E2E`),
`DS_000F0A64` (`0x11E35`), after the calls at `0x11DF2`/`0x11DF7`/`0x11E03`/
`0x11E0C`. The port has no write-order observation mechanism — `DSD`/`DSB`/`DSW`
are plain memory stores and there is no write trace — and no call in the case
reads those five globals, so the relative order is unobservable to any test.
`port/tests/test_game.c` asserts the five values and the two observable
calls (`0x1EA08`'s `0x4F1E4` latch clear and `0x2C06C`'s row store), which
proves the stores and calls happened, **not** their order. The order rests on
the raw citation above and on transcription; a reordering of the stores, or
moving the calls after them, passes the suite and must not be read as tested.
(Front-end chain Task 6.)

**The all-black capture frames are excluded as artifacts — an explicit
oracle-level choice, not a proven fact.** `tools/title_compare.py --frontend`
classifies a capture frame that is entirely black as an artifact: it requires no
match and is not counted as unexplained; only all-zero frames are dropped, so a
content-bearing frame that disagrees with the port still fails (a deliberately
corrupted port frame is reported; the unexplained count is corruption-dependent
and the tool returns 1, not a fixed 2). Sixteen all-black capture
frames are excluded: distinct 0, 217, 357, 390, 424, 458, 491, 525, 561, 831,
1885, 2095, 2133, 2383, 3406, 3543 (raw 1367, 2173, 2420, 2529, 2637, 2746,
2855, 2963, 3109, 3670, 4793, 5401, 5807, 6758, 7846, 8247, in the capture
current at the arena-backdrop cycle; the indices are host-timed and shift on a
re-capture — the Task-9 list was 0, 213, 353, …, 3552). One of them,
capture 561 (raw 3109), is the frame the earlier report named: a fully black
frame between two identical state-3 entry frames (caps 560/562, raw 3108/3110,
both byte-equal to port frame 0). It is produced by `0x52106`, which `0x2BAF4`
calls at `0x2BBEA` (the `xor eax,eax` at `0x2BBE8` precedes it) when
`param_1 != 0`: it clears both offscreen buffers, zeroes the VGA DAC, and blits
the visible `0xA0000` aperture. The port performs the buffer and tick-counter
arms but not the VGA DAC/aperture arms ("the aperture rule — never write
mem[0xA0000]").

Modelling that blank in the port was investigated and found **not feasible**, for
two concrete reasons. First, the port presents only once at end-of-frame
(`port/src/game/flow.c:1126-1127`) and the oracle dumps one post-swap buffer per
`game_loop()`, so a local blank placed at the `0x52106` model is byte-inert.
Second, even a structural present/dump seam would not converge: an all-black
*port* frame content-matches sixteen all-black *capture* frames (including
pre-logo capture 0), which would explode the window to `[0..3569]` with ~3296
unexplained. **What the investigation could not settle** is whether capture 561's
black frame is a distinct logic frame of the original or a scanout/sampling
artifact of its 70.09 Hz timing. The exclusion is therefore a choice made in the
absence of that certainty; it is documented here rather than presented as
proven-correct.

**No unpinned RNG.** States 3 and 4 contain no `call 0x5D7DC` (`rng_next`) site
of their own, and if the state-3 actor dispatch runs the animation opcode-8
handler at `0x2B435` it DOES consume RNG — but that site is already the fourth
title pin (`tools/title_pin.py` replaces the call with `mov eax,0`), so no new
pin is possible or needed. The only other actor RNG helper, `0x2BDA0`, has no
`call` site anywhere in the code object; a single relocated code pointer to it
exists at data VA `0xE8B90` (object-0 offset `0x1BDA0`), and no code reads that
pointer. The mismatch was therefore a rendering/timing gap, never an unpinned
draw, so no behaviour pin was written (a pin here would be a fitted constant).

**Known limitation of the enforced gate.** `frontend-oracle` fails only on
`unexplained` capture frames, and `unexplained` is evaluated only inside the
window the port's own dump exhibits (`check_capture` derives that window from its
`idx`→`a,b` mapping, then the branch discards `rc`). `check_capture`'s `rc`
additionally counts port frames that no capture frame exhibits (coverage) and
`endpoints BAD`; the front-end branch ignores both. The demo-fight cycle raised
the dump to 1381 frames, so the 120 s capture (3617 distinct post-logo frames)
now runs past the dump instead of ending before it; but that is not the whole
story: the ignored coverage is exactly what lets a port that **under-renders**
the front-end pass (counterexamples above). Passing the gate therefore means "no
content-bearing capture frame inside the port-exhibited window is unexplained",
NOT "every dumped port frame was exhibited" and NOT "the front-end was
rendered".

## Demo fight states 6/7 (demo-fight cycles 1–2, report-only window)

The attract demo's CPU-vs-CPU fight is states 6 (`0x11A8C`) and 7 (`0x11E8F`,
the arena frame `0x263F4`, ended by `0x11BCC`'s timer exit). It is **not** the
interactive match: that is the `DS_00104B00` mode graph (the `0x257A4` coin
divert, `0x1EEB0`), which no task in either demo-fight cycle owns. The port advances the
front-end into the demo unaided. State 3's phase-1 terminator (`0x12658`) hands
to state 9 with a 240-frame timer and target 6; state 9 counts down; state 6
picks the two characters from the shared RNG, seeds `DS_001082C8`, spawns both
fighters and arms the 900-frame timer; state 7 runs the arena loop until
`0x11BCC` restores `DS_000F0A64 = DS_000F0A6C` and the state machine moves on.

**The loop-back.** For the demo's chain `DS_000F0A6C` is 0: state 3's phase 1
sets `DS_000F0A72 = 0` (`0x1264C`), and state 6 copies it into `DS_000F0A6C`
(`0x11BB9`). So the 900-frame exit hands `DS_000F0A64 = 0` — state 0, the
attract sub-machine `0x11000` (record §4.2) — and the demo loops back to the
attract. The nonzero continuations (`DS_000F0A72 ∈ {3,4,5}` from the attract
phase at `0x1150E`/`0x11517`/`0x1151F`, `port/src/game/attract.c:304-307`)
belong to the *next* attract loop, not this demo run.

**Dump length.** `make demo-oracle` runs the same `PR_FRONTEND_DUMP` run as
`frontend-oracle`. The driver loops 2000 frames from the state-2 entry and caps
the RGB dump at 1400 frames (`PR_FRONTEND_DUMP_FRAMES`, default 1400). The
measurement that sizes it: state 3 enters at loop 589 (dumped frame 0), state 6
at loop 1070 (dumped 481), state 7 runs loop 1071..1969 (dumped 482..1380), and
`0x11BCC`'s exit is loop 1970, where `DS_000F0A64` drops to 0 so the `state >= 3`
dump stops. The dump therefore holds **1381 frames** (dumped 0..1380); the 1400
cap covers it with no truncation, and the 2000-frame loop clears the 1970 exit.

**The demo window is report-only; its first unexplained frame is capture 866 —
the f = 92/93 tear, where both fighters and the ground scroll differ, after the
demo-pose cycle explained captures 843..850, the roar-timing fix 851..857, the
frame-858 fix 858, the frame-859 fix 859, the frame-860 fix 860..863 and the
frame-864 fix 864/865.** `tools/title_compare.py --demo` locates the front-end window
with the same content alignment, then classifies the capture region after it
against the port dump frames after the last frame that window exhibits — the
same clean/splice/transition/unexplained model, no second one. It reports and
exits 0.

* Front-end window (still enforced in `verify`): distinct **[560..865]** (raw
  3108..3772), **306 frames: 137 clean, 165 splice, 0 transition, 2
  unexplained** — captures **832** (the loader's `- LOADING -` screen) and
  **833** (the dark arena with the `LOADING` text overlaid), allowed by name in
  `FRONTEND_ALLOWED_UNEXPLAINED` (the arena-backdrop cycle's absorbed claim
  move; their owners are the state-9 hold's animation and the presented
  DAC/palette state). Any other unexplained frame fails. The indices moved
  again from cycle 1's `[557..810]` / `254` (`100 clean, 150 splice, 3
  transition`) when Task 2's master-loop pin forced the cycle's re-capture; the
  move is the **capture's, not the port's** (the capture is host-timed and not
  reproducible — a `title_capture.py --verify-reproducible` run of the pinned
  original gave 587 vs 588 distinct frames and a first divergence at distinct
  index 30), and the oracle's claim is unchanged.
* Demo window: distinct **[866..3616]** (raw **3773..8409**), **2751 frames:
  0 clean, 0 splice, 0 transition, 2745 unexplained** (6 all-black capture
  frames excluded as artifacts). Demo port frames **[510..1380]** (871),
  **0/871 exhibited**. (Measured on the frame-864 fix; before it: `[864..3616]`,
  2753 frames, port `[508..1380]`, first unexplained 864; before the frame-860
  fix: `[860..3616]`,
  2757 frames, port `[505..1380]`, first unexplained 860; before the frame-859
  fix: `[859..3616]`,
  2758 frames, port `[504..1380]`, first unexplained 859; before the frame-858
  fix: `[858..3616]`,
  2759 frames, port `[503..1380]`, first unexplained 858; before the roar-timing
  fix: `[851..3616]`, 2766 frames, port `[497..1380]`, first unexplained 851; before
  the demo-pose cycle: `[843..3616]`, 2774 frames, port `[490..1380]`, first
  unexplained 843.)
* **First unexplained captured frame 866 (raw 3773)** (demo-pose record §15.5).
  Capture 866 is the f = 92/93 tear: the best 509/510 splice (row 137) leaves
  27 858 B / 9 891 px over both fighters and the ground, whose rows 185–199
  match no port frame 508..511 at any shift in [−8, 8]. At f = 93 the port's
  camera steps −500 → −256, the T-rex switches `0x907E` → `0x96B5` and its
  shadow `0x9E04` leaves the list. The owner is not yet derived. (Before the
  frame-864 fix this was capture 864: the grey flier `0x1282C` spawns from
  `0xBB254` was refused because state 6's `0x12750` node list was unported, and
  the driver's frame counter was unseeded, so the gate opened at f = 81 instead
  of f = 91; record §15. Before the frame-860 fix this was capture
  860: the port kept the roar's side-0 slot `+0x42` bit 0 set, so `0x4AB7F`
  retargeted the worshipper to type 8 at f = 88, and the best 504/505 splice
  left 476 B / 164 px; porting the `0x4A634` tail reset, record §14, removed
  that. Before the frame-859 fix this was capture 859: the worshipper walked past its target
  because `0x49C78`'s type-1 case was unported, and the best 503/504 splice left
  449 B / 159 px; the `0x49D2F`/`0x4AC38` port, record §13, removed that.
  Before the frame-858 fix this was capture 858:
  the port scrolled the arena's mode-1 props wrongly from f = 86, and the
  best 502/503 splice left 13 617 B / 5 253 px; the `0x2A733`/`0x2A6E0`
  operand fix, record §12, removed that. Before the roar-timing fix this was capture 851: port 497 (f = 80)
  differed by 6 544 B / 2 456 px because the roar advanced early, and the
  `0x3AD27` operand fix, record §11, removed that. At the arena-backdrop cycle this was capture 843 /
  raw 3750, the T-rex's pose — port 490 differed by 16 064 bytes; the
  mountains, temple, sky, sea, HUD and text matched.) The front-end
  window's boundary frames 832/833 remain its named, out-of-scope unexplained:
  831 is all-black (the state-6 `0x2BAF4(1)` DAC blackout; the oracle drops it
  as an artifact); 832 is black except rows 192..197 — the lazy loader's
  `- LOADING -` string (489) drawn at (0,192) through
  `0x1B3AC`/`0x1C500`/`0x1C65C`; 833 is the arena dark (a mid-fade presented
  DAC) with the `LOADING` text overlaid; 834..842 are the fight's opening arena
  frames, which the arena-backdrop cycle explains (834..839 byte-exact at 0 B
  each).
* **Cycle 2 advanced the boundary 811 → 816 → 832, then stopped there.** Task 3
  ported the state-9 globe's missing indirect-call target `0x12720` (the globe's
  fourth layer), so the state-9 hold now matches (captures 816..830 == port
  264..339) and the first-unexplained moved to the loader frame. Tasks 4/5/5a/5c
  then landed the props, the loader's overlay, the `rle_row` mirror-window fix,
  the camera seed, the master-loop tick gate + load-stall model and the real VGA
  aperture; Task 5b the fighters' idle-animation tick; Tasks 6/6b the hitbox
  machine and the `0x3CF38` hit chain (which now **fires** and lands hits). None
  moved the first-unexplained past 832: 832 is the loader's presentation, and its
  read-stall is the one piece proven un-derivable (closure outcome below). (The
  arena-backdrop cycle later moved it past 832 to **843** — the T-rex pose, the
  cycle-5 outcome below — the demo-pose cycle to **851**, cycle 6 below, and the
roar-timing fix to **858**, the frame-858 fix to **859**, the frame-859
fix to **860**, the frame-860 fix to **864**, and the frame-864 fix to
**866**.)
* **The window is no longer non-discriminating.** Cycle 1's `--demo` window
  opened on the state-9 hold's first frame, so it read identically for correct or
  broken code. Task 3 made the state-9 hold match — the hold's frames
  (816..830) are clean inside the **front-end** window (`0 unexplained`) — so the
  demo window's boundary (the loader's presentation at 832/833, then the T-rex
  pose at 843, since moved to 851 by cycle 6 below, to 858 by the roar-timing
fix, to 859 by the frame-858 fix, to 860 by the frame-859 fix, to 864 by
the frame-860 fix and to 866 by the frame-864 fix) is a real content gap, not a window-definition artifact; the demo
  window itself still reports **0 clean** (above). Window re-anchoring was **removed from this cycle**
  (design spec, "Removed from this cycle"): no new reference and no re-anchoring
  task, because porting the state-9 render made the **front-end** window's
  state-9 hold frames clean instead.

Cycle 1's declared bound expected the first unexplained frame to be the
original's first landing hit (the cycle split gives cycle 2 collision and
damage). Task 9's verification **retired that expectation**: cycle 1's frame was
in the **state-9 hold**, before state 6. Cycle 2 then explained it and moved the
boundary to the loader frame (above).

* Cycle 1's capture 811 was the "WHO WILL RULE THE NEW URTH?" globe screen; its
  closest port frame was **264**, 205 differing bytes in rows 98..144. Task 3
  found the cause: the globe's fourth layer is spawned by the indirect-call
  target `0x12720` (opcode `0x11` at `0xE89A8`, mode `0x4000`, loaded into
  `DS_00105BD4` and reached by `0x2B57F`'s `call dword[0x105BD4]`), which was
  unregistered, so `anim_indirect` skipped it. Registering it made the state-9
  hold match (captures 816..830 == port 264..339). Port frame 264 is inside the
  state-9 hold: state 3 hands to state 9 at dumped frame 240 (`0x12636` sets
  `DS_000F0A64 = 9`; `0x12645` sets the `0xF0` timer), state 9 runs dumped frames
  240..480 and state 6 runs 481. `0x11D04`'s case-9 arm only decrements
  `DS_000F0A6A` and, at zero, restores `DS_000F0A64 = DS_000F0A6C`; it draws
  nothing.
* The capture's demo fight does not begin until capture **833/834** (833 is a
  capture-time tear; 834..836 are full arena frames). Amendment 5 of the cycle-1
  plan said 839/28 and Task 1 corrected it to **836/25** against the
  then-current capture; the Task 2 re-capture moved the divergence to 832 and the
  loader text to 832, so the current divergence→fight gap is 832 → 833/834, not
  25 frames. (Record §9.1 carries the 836/25 correction; §9.3, measured against
  the final capture, is the raw-wins refinement.)
* The port's state-7 output moves: cycle 1 measured 64 distinct images over loop
  frames 1071..1969. Cycle 2's `+0x52` state machine (Task 6b) drives the chain
  so it **fires** and hits land (`+0x7C` 0/0 → 1/1); the fight's last state
  change moved **1262 → 1400**. The residual stall tail is a named gap (closure
  outcome below), not a determinism site.

**Two pins were added, for the demo's state-6 character picks; the state-9 hold
itself has no pinnable site.** A pin is a determinism fix — a site where the
original reads uninitialised or timing-dependent state — never a value chosen to
make a frame match. The first unexplained frame's cause is a render gap, not such
a site:

* State 9 draws no RNG (`0x11D04` case 9, above; the port's case 9 is faithful,
  and `port/tests/test_game.c`'s `check_state9_countdown` asserts the LCG state
  is unchanged across it). The only RNG consumer reachable in the hold is the
  actor-animation opcode-8 handler, which is already the fourth `title_pin.py`
  pin (`0x7E289`). So the hold has no unpinned draw to pin.
* The demo window's own RNG sites are state 6's two character picks
  (`0x11AAD` → `rng(7)`, `0x11AE9` → `rng(6)`) and the state-7 CPU-AI
  generator's `rng(0x64)` at `0x47063` (one draw per committed move, §11.2).
  Cycle 1 pinned the picks to the port's own LCG values (`0x64901`/`0x6493D`,
  `call` → `mov eax,4`). That was a determinism fix of the wrong shape — the
  `call`→`mov` pin does **not** advance the reference's LCG while the port's
  draws do, so the streams were offset by two draws from state 6 onward — and
  **cycle 2 removed both pins** and pinned the divergence's source instead.
* The generator's `rng(0x64)` is **not** pinned: it executes once per committed
  move with a different value each time, so the `mov eax, imm32`
  constant-replacement shape cannot align it. That is a cycle-2 question (how the
    demo's stream is kept in step), recorded here rather than fitted.

**Cycle 2 replaced the two state-6 pins with the master-loop pin, then closed
the two deterministic offsets the pin alone did not cover — the streams now
meet.** The master loop `0x255CC` draws `rng(0x7FFF)` twice: the body draw at
`0x256B1` (file `0x78505`, `e826810300` → `b800000000`) and the spin draw at
`0x256D6` (file `0x7852A`, `e801810300` → `b800000000`), both with
`EBP = 0x7FFF` (`0x255E4`). The spin is
`while (DS_0010150C - 1 == DS_00101508) rng_step()` (`0x256C6`..`0x256DB`) —
host-timed and unbounded, which is why the reference's stream position was not
deterministic. `title_pin.py` now pins both sites to a non-advancing
`mov eax,0`, and the port's `game_loop` stopped drawing at the body site (the
`rng_step()` there removed; `port/src/game/flow.c`). The port never modelled the
spin: its master loop waits one 60 Hz host retrace (`host_wait_vblank()`,
`host.c:203`), so it has no spin draw to stop.

The oracle claims held after the re-capture (`make frontend-capture`): title
`54 clean, 55 splice, 2 transition, 0 unexplained` / `54 clean, 57 splice, 0
unexplained`, attract `FIRST DIVERGENCE at capture frame 215`, front-end `0
unexplained`. The window indices moved (host-timed, derived): the front-end
window distinct `[557..810]` → `[560..815]` (raw `3113..3414` → `3108..3409`;
counts `254: 100 clean, 150 splice` → `256: 103 clean, 152 splice`), the demo
window distinct `[811..3759]` → `[816..3616]`, first unexplained `811` → `816`,
and the post-logo distinct count `3760` → `3617`. The claim is the gate; the
indices are derived.

**The pin alone did not align the streams; two deterministic draw sources were
missing, and both are now ported (the human's ruling, Option 1).** Measured and
derived:

* The reference's state-6 entry is the seed **+ 26**: the attract's voice tick
  `0x10F28` (called from `0x11559` while `DS_0009AD58 == 0`, set at `0x11092`)
  draws `rng(0x2D)`/`rng(2)`/`rng(0x3C)` at `0x10F4D`/`0x10F76`/`0x10F95`
  before the title; the title's three draws are pinned to constants (no
  advance); and the only draw site reachable from states 2..5 is the
  already-pinned opcode-8 handler (`0x2B2A0`). The port's own attract reaches
  the `attract_step` case 0xB handoff with `DS_000EF6D8 == 0x4308698B` = seed
  `0xABCD` advanced exactly 26 steps (the attract oracle shows the capture's
  attract is that same run). The front-end driver entered at state 2 and sat at
  the seed; it now re-seeds to that post-state (`FRONTEND_RNG_AFTER_ATTRACT`,
  `test_game.c`) — the same pattern the title driver uses for the pinned
  title, and the dumped window is unchanged.
* State 6 itself draws six more than the port's two. `fighter_spawn(0)`
  (`0x33EB4` → `0x33C78`) calls the dust builder `0x494A8`, whose loop
  (`0x49540`/`0x4967F`, bound `slot+0x81`) draws three values per iteration —
  `0x49388`'s unconditional draw (`0x493AB`), `rng(0x1800)` (`0x495DF`) and
  `rng(step)` (`0x495FC`) — between `0x11AAD` and `0x11AE9`; the demo's
  `slot+0x81` is 2, so six draws. The port's `fighter_spawn_slot` skipped the
  builder; `fight_dust_build` (`port/src/game/fight.c`) now ports it — the
  entry traffic, the `0x49388`/`0x29CDC`/`0x496AC` helpers and the `0x2AE14`
  actor spawn. (§10.5's "loops `n` times … two RNG values per iteration" was
  wrong against the raw: the bound is `slot+0x81` and `0x49388` draws once per
  iteration; corrected in the cycle-1 record.)
* The capture's picks confirm both: at seed+26, `rng(7)` = 0, and after the
  dust's six draws `rng(6)` = 3, giving characters `0xC835A[0] = 0` and
  `0xC835A[3] = 3` — exactly the two fighters the re-captured demo shows, and
  now the port's too (verified frame-for-frame against the capture). The driver
  asserts the entry LCG state and the two characters
  (`test_game.c`), and `check_state6` asserts the full 14-draw model and the
  dust entries' fields.
* **Residual.** The dust entries' type-0 processing (`0x49C78`'s default arm →
  `0x4AAD0` and its callees) is still a named gap, so the dust's motion and
  despawn are not faithful; its actor is spawned and rendered. The aligned
  stream's picks are `0xC9524` indices 0, 1, 3, 4 → the descriptors
  `0xBB470`/`0xBB484`/`0xBB4AC`/`0xBB4C0`, types `0x20`/`0x21`/`0x23`/`0x24`,
  whose per-type callbacks at `0xBB9DC + type*0xC` are all the `0x5D812` stub.
  The demo window's first unexplained frame is 832 (the loader frame), so the
  dust's pixels are measurable only after it is explained.
  `tools/title_pin.py`; `host.c:203`; `flow.c`; `fighter.c`; `fight.c`;
  `actors.c`; `effects.c`.

Cycle 2 landed the collision/damage half (`0x3BB90`, `0x4FB20`, `0x3BAEC`,
`0x3B9D8`), the `0x3CF38` hit chain, the `0x3C88C` hitbox machine (§7.7), the
state-9 globe's fourth layer (`0x12720`), and the `- LOADING -` screen (string
489 at (0,192) through `res_load_present`/`text_blit_string`
(`0x1B3AC`/`0x1C500`/`0x1C65C`), now drawn into the real VGA aperture and proven
pixel-exact at 166 px against capture 832). The Gate is nevertheless **UNMET** —
see the closure outcome below.

The `0x3C88C` draw helper and the skipped `0x20DF4` fight reset at `0x11AC4`
(below) were named as the gaps that might own the missing state-7 composition;
the machine is now ported and the `0x20DF4` liveness fix landed, so the residual
state-7 divergence is the fighter animation/palette subsystem (closure outcome
below).

**The skipped `0x20DF4` is a liveness gap, not only a fidelity gap.** The raw
calls `0x20DF4` at `0x11AC4`; its first callee `0x49300` self-links the
effect-list sentinels `DS_001083C4`/`DS_0010884C` and seeds `DS_001088CC`/`CB`.
The port skipped `0x20DF4` as a named gap, but `fight_effects_pass` (`0x49C78`)
reads `DS_0010884C` as a circular-list head and loops until it returns to the
sentinel; with the global left at 0 the walk never terminates and the demo hangs
on its first state-7 frame. `fight_list_init` ports `0x49300` verbatim (from the
raw disassembly), so the walk is a no-op on the empty list exactly as the
original's is. `0x20DF4`'s other resets remain a named gap.

## Demo fight cycle 2 — closure outcome (Task 8)

`make demo-oracle` (report-only, **not** in `verify`) measures the demo window
end-to-end. **The cycle's Gate — the demo window reports 0 unexplained frames —
is UNMET.** The measurement, reproduced by Task 8:

* Demo window distinct **[831..3616]** (raw **3670..8409**), **2786 frames:
  0 clean, 0 splice, 0 transition, 2779 unexplained** (7 all-black frames
  excluded). **First unexplained captured frame 832 (raw 3671).** The port
  exhibits **0/1068** demo port frames ([313..1380]).
* Front-end window (enforced in `verify`): **[560..830]** (raw 3108..3472),
  **271 frames: 117 clean, 153 splice, 0 transition, 0 unexplained** — its
  claim holds (cycle 5 supersedes: `[560..842]`/283/2; cycle 6 moves it on to `[560..850]`/291/2).

**The residual frames are named gaps, each with its evidence and the task that
left it. No pin or value was fitted to force the Gate.** (Cycle 3's section below
updates items 2–4: the "palette order" premise is refuted — the acquisition
sequences are identical and the T-rex's colour difference was the front-end dump
driver's `DS_0010816A` seed; the pose residual and the stall tail are one
subsystem, the unported `0x19020` chain.)

1. **Captures 831/832 — the loader read-stall / the presented DAC-palette state
   at the state-6 entry** (Tasks 5a/5c). Proven **un-derivable**. The port
   produces both states: `mem + DS_000E87A4` at the state-6 hold is
   byte-identical to capture 832 (498 non-zero bytes) and `mem + DS_000E87A0` to
   capture 831 (all zero). Task 5c landed the real VGA aperture (`0x51ED8` vs
   the back buffer's `0x51E5C`; `0x52106` clears it at `0x5214C`..`0x52151`), so
   the loader's text now lands in the aperture at 166 px — exactly capture 832 —
   with `E87A4` untouched. But the master loop's tick gate (`0x25643`,
   `if (150C == 1508)`) passes on the loader frame (`0x1B3AC`'s tail re-syncs
   `150C = 1508` at `0x1B45F`/`0x1B464`), so the copy overwrites the aperture in
   the same iteration. Exhibiting 832 needs the read's stall to hold frames; the
   stall's post-read ISR ticks are **not derivable** — `0x1BDF4` is a PIT timer
   interrupt (`0x1BE0E`/`0x1BE10`) whose count is post-read CPU work ÷
   cycles-per-tick, a host/emulator property, and the two live-RAM polls
   disagree (Δ=2 vs Δ=3). **No constant was shipped.**
2. **The palette order** (Task 5b). `palette_acquire` assigns DAC ranges in
   acquisition order; the character palette (`0x1BB9FD58`, DAC range
   `start=142`) differs while the backdrop palette matches, so the T-rex's
   residual is a palette/bank difference, not a frame — the same palette-order
   subsystem Task 5c deferred.
3. **The fighter animation poses** (Task 5b). The raptor silhouette's IoU is
   0.522 with no port frame matching over 900 (0.293 pre-fix). The residual is
   the fighters' animation/think state across `actors.c`/`fighter.c` plus the
   actor frame-timer path — a derivation.
4. **The fight's stall tail** (Task 6b). At loop 1400, side 1 lands on
   `+0x52 = 9` with `+0x53 = 8` where the original is at `+0x52 = 4`; closing it
   needs the unported `0x34B14` handlers (§7.10: `0x35F84`, `0x36870`,
   `0x235C4`, `0x370F0`, …) plus the `0x3C88C` re-arm. `0x34038`/`0x38D24`/
   `0x354F0`/`0x186C4` remain named gaps.
5. **The interactive match is UNOWNED.** The mode graph (`DS_00104B00`), the
   `0x257A4` coin divert, `0x1EEB0`, `0x1F458`, the player screens and human
   input are not implemented by any task in this cycle. The design spec's Out
   section stands; this is not a gap inside the demo window.

**What the cycle did land** (oracle-neutral or boundary-advancing): the state-9
globe layer (`0x12720`); the hitbox machine (`0x3C88C`) and the `0x3CF38` hit
chain, which now **fires** and lands hits (`+0x7C` 0/0 → 1/1, last state change
1262 → 1400); the master-loop tick gate + load-stall model and the loader
re-sync; the fighters' idle-animation tick (`0x37A58`); the `rle_row`
mirror-window fix; the props and the loader overlay; the `fight_hud_pass`
driver; the state-6 camera seed (`0x12C70`); and the real VGA aperture. Every
enforced oracle claim is at its original value (`README.md`, "Verify").

**Pins.** All six `title_pin.py` sites are behaviour pins: the three `0x121A0`
title-entry draws (file `0x650E9`/`0x650F5`/`0x6510B` = VAs
`0x12295`/`0x122A1`/`0x122B7`, values 12/111/0), the anim opcode-8 handler (file
`0x7E289` = VA `0x2B435` → 0), and the two master-loop draws (`0x256B1`, file
`0x78505`, and `0x256D6`, file `0x7852A`, both `rng(0x7FFF)` with `EBP = 0x7FFF`
at `0x255E4`, both → 0). The spin pin (`0x256D6`) is the one that made the
reference deterministic: the spin `while (DS_0010150C - 1 == DS_00101508)
rng_step()` (`0x256C6`..`0x256DB`) is host-timed and unbounded, so the
reference's stream position was not deterministic; pinning it forced the cycle's
one re-capture and moved the derived window indices (front-end `[557..810]`/254
→ `[560..830]`/271, demo `[811..3759]` → `[831..3616]`), not the oracle claims.
The state-6 character picks (`0x11AAD` `rng(7)`, `0x11AE9` `rng(6)`) are **not**
pinned — they are drawn from the aligned stream and the oracle validates them.
The title window splits `TITLE_WINDOW_ITERS 96` (ticks) from
`TITLE_PRESENTED_FRAMES 96` (gate-passed ticks the dump holds); the split is
derived from `0x25643` and the re-sync (`0x1230B`/`0x1235x`), and both constants
are 96 after the re-sync restored the loader frames.

## Demo fight cycle 3 — combat/render fidelity outcome (Task 5)

Cycle 3 (branch `combat-fidelity`, spec
`../../docs/superpowers/specs/2026-09-22-combat-fidelity-design.md`, record
`../../docs/superpowers/plans/2026-09-22-combat-fidelity-derivations.md`) closed
three of cycle 2's named residuals. **The Gate is UNMET overall**: its second
claim (the palette/arena) is MET; its first (the fight runs to the timer exit
without stalling) is not.

**The Gate's two claims, measured.**

1. **The fight reaches the timer exit — but stalls.** The state-7 900-frame timer
   exit is reached: `s7_last == 1969` (`port/tests/test_game.c`; state 7 is
   entered at loop 1070 and left at 1970, so its last frame is 1969). It does
   **not** run to it without stalling: the slot state machine's last `+0x52`
   change is loop frame **1072**, and the original's pose state `0x10`/`0x0A` is
   never entered (`s7_saw10`/`s7_saw0a` are 0). **UNMET.**
2. **The arena's character palette matches the raw's DAC range.** The T-rex's
   character palette handle `0x1BB9FCD8` (variant 1) is acquired at
   **`start=142 len=31`** — the port's ownership table (`DS_00107618`) entry 6 at
   HEAD, identical to the original's (record §1.2); the unit assertion is
   `test_game.c:694-695` (`DS_00105B34 == 1`, `0xA8A28[1] == 0x1BB9FCD8`).
   The arena byte-diff (`frame_0482.raw` vs capture
   `frontend/frame_0834.raw`) fell **42 667 B (22.2 %, 15 067 px) → 18 294 B
   (9.5 %, 6 194 px)**; the T-rex region 6 363 → 1 773 px, the raptor region
   8 704 → 4 421 px. The raptor's teal-mask IoU (region x 185..320, y 85..200)
   rose **0.522 → 1.000** (frame 482; 0.974 at 483). **MET.** The residual is the
   pose state (claim 1's gap), named not fitted.

**The freeze is the pose subsystem, not a single state (cycle 4's).** The
original's T-rex leaves the 9/8 hold for the pose state `0x10`/`0x0A` on the 6th
frame; the port stays at 9/8. The `+0x52 = 0x10` writers are reached only through
`0x1958C` (`fighter_pass_a`, ported) → `0x193B0` → `0x3B714` → `0x3AAFC` → the
`0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8` pose family, gated on
`DS_00100AF8 != 0`. `0x19020` (which sets `AF8[side] = (result == 0)`) is
unported, so `AF8`/`AFC` stay 0 and the tail never runs. The closure is **68 new
functions / 10 467 B (true new ≥ 11 012 B)** — ~2.4× the handler tail and ~3.3×
cycle 2's `+0x53` machine — so Task 4 landed no port and the subsystem is handed
to **cycle 4**. (The brief's `0x3531C` case-8 re-arm model is refuted: its gate
`word[0xBDBE8] = 3 ≤ slot+0x88` at `0x35498` stops it writing after 3 frames, and
the port's animation cursor already matches `0xD2316`.) **Measured at HEAD the
port's hit counter `+0x7C` is 0 for the whole state-7 window (`s7_hit == 0`):
cycle 2's `+0x7C 0/0 → 1/1` was a by-product of the old diverged trajectory
(s0's `+0x52=3`/`+0x53=4`/`+0x54=2` retry loop re-armed a hitbox). With the
trajectory now matching the original's early path, the hit requires the pose
state the port cannot reach.**

**What the cycle landed.** The front-end dump driver's palette-variant seed
(`DS_0010816A[1]` `0xFF` → `0`; no engine change — `0x41350` is faithful, and
the acquisition sequences are identical); seven faithful `+0x52` handlers
(`0x35F84`/`0x36430`/`0x399CC`/`0x361C8`/`0x36300`/`0x36710`/`0x364FC`) and the
`0x3C148`/`0x3C16C` clears; the state-7 entry's four missing RNG draws (the
type-0 effect handler `0x4AAD0`/`0x4B144` and its `fight_effects_pass` wiring —
the entry LCG now reaches `0x10F7DB07`, matching); and the demo-AI block state
(`fighter_18540`/`fighter_18350` wired into `fighter_slot_latch` — the command
words now match: `cmd0@1072 = 0x4848`, `cmd1@1071 = 0x0002`). The loader flush
scope got **no engine change**: record §4 shows no faithful standalone change.

**Named gaps carried out of the cycle.** 831/832's held-frame presentation (the
post-read ISR ticks, a host property — un-derivable, so excluded, not fitted);
the unported `0x36870` 9→4 closer and `0x37178`/`0x37D18`; the five unported
`+0x52` handlers (`0x359E0`, `0x35C1C`/`0x35D20`, `0x37464`, `0x33B00`,
`0x35E6C`); and the `0x19020`/`0x3Fxxx` freeze subsystem (cycle 4).

**The interactive match remains UNOWNED** — the mode graph (`DS_00104B00`), the
`0x257A4` coin divert, `0x1EEB0`, `0x1F458`, the player screens and human input
are implemented by no task in any demo-fight cycle.

**Every enforced oracle claim is unmoved.** `make verify` exits 0 with 0
warnings: title `54 clean, 55 splice, 2 transition, 0 unexplained` and
`54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at capture frame
215` (raw 2180/2175); front-end `[560..830]` / 271 frames: 117 clean, 153 splice,
0 transition, `0 unexplained`; smk `120/120` + `41/41`; C-vs-Python `9866 writes
byte-exact`; `symbols.h` regenerates byte-identically (1304 globals, 1206
functions).

## Small fidelity gaps — outcome (Task 6)

The `fidelity-gaps` cycle (spec
`../../docs/superpowers/specs/2026-09-22-fidelity-gaps-design.md`, record
`../../docs/superpowers/plans/2026-09-22-fidelity-gaps-derivations.md`) closed
**all four** of cycle 3's small named gaps and moved **no enforced oracle
claim**. No single group triggered the size gate, but the **branch total crosses
it**: 20 functions / 4 675 B (the four groups 10/2 223, 2/418, 0, 3/293 plus the
human-ratified caller chain 5/1 741).

* **The five `+0x52` handlers** — `0x359E0` (state 1), `0x35C1C` + `0x35D20` (2),
  `0x37464` (8), `0x33B00` (19), `0x35E6C` (20) ported and dispatched; 31
  `CHECK_EQ_INT` with seven mutation proofs.
* **`0x349C8`'s bit-6/7 deep callees** — `0x385B0` wired at `0x36884` (under
  `DSW(0x104B00) == 0x25`) and `0x39A10` at `0x37D57`, with the minimal caller
  chain (5 functions / ~1 741 B) ported and **ratified by the human** so the
  callees stay reachable; 15 `CHECK_EQ_INT`, five mutation proofs.
* **The loader flush scope** — the record's "scoped flush" premise was **refuted
  by the raw**: `0x1C470` is a whole-list drain (`0x1C6C6`/`0x25672`/`0x2EADB`,
  no scope parameter) and the port's flush was already faithful; the real gap,
  the missing initial record `{0xBD470, 0, 1, 0}` (`0x336C0`'s `0x33734`
  enqueue), is now enqueued and asserted (`test_game.c:485-489`).
* **The attract/scene palette drivers** — `0x4F7F4`/`0x4F83C`/`0x33874` ported
  and registered through `attract_scene_tick`; `0x4F83C` ships **test-only** (its
  `0xE8916`-table callers are unported) as an **explicit accepted exception**
  (spec representation rule, Q10); the attract claim is unmoved (215).

**The 169-tick hold's verdict.** A **real divergence**, not the load/stall
model: the port holds capture-830's frame byte-static for loop 902..1069 while
the capture's state-9 screen animates to the loader at 831. Its owner is the
state-9 screen's actor-animation advance; oracle-neutral (neither oracle covers
port frames 259..480) and carried forward.

**Carried with owners (record §7).** The pose/freeze subsystem (`0x19020` chain)
and the demo oracle's `res is None` → **cycle 4**; the `0x13xxx` call sites
(`0x29B74`/`0x41578`) → **the interactive match**; 831/832's held-frame
presentation (un-derivable); the interactive match (unowned); the audio gaps (the
audio sub-project, 2b-ii); the state-9 hold; `0x38154`; and the
flush-scope-vs-gate concern (cycle-2's read/gate model).

**Every enforced oracle claim is unmoved.** `make verify` exits 0 with 0
warnings: title `54 clean, 55 splice, 2 transition, 0 unexplained` and
`54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at capture frame
215`; front-end `[560..830]` / 271 frames `0 unexplained`; smk `120/120` +
`41/41`; C-vs-Python `9866 writes byte-exact`; `symbols.h` regenerates
byte-identically.

## Demo fight cycle 4 — pose/freeze outcome (Task 5)

Cycle 4 (branch `pose-freeze`, spec
`../../docs/superpowers/specs/2026-09-24-pose-freeze-design.md`, record
`../../docs/superpowers/plans/2026-09-24-pose-freeze-derivations.md`) ported the
freeze's two halves faithfully. **The demo observable did not move** — the
cycle's Gate (leave the `res is None` fallback) is **UNMET**, and the residual
divergence is named with its owner.

**The two halves and the gate.**

* **The unfreeze half** (`0x140E4` + `0x170A0` + 11 callees, one C function per
  original, in `camera.c`), wired into `camera_decay` at the `0x17698`..`0x176BF`
  tail (`camera.c:983-987`). Assertion `check_unfreeze` (the overlap gate, the
  `B60`/`B61` gates, the `0x170C5`/`0x170E2` guard, `AF8 == B54`); 5 mutation
  proofs.
* **The pose-entry half** (`0x193B0` → `0x3B714` → `0x3AAFC` → the
  `0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8` pose family + new callees, in
  `fighter.c`), wired into `fighter_pass_a`'s tail (`fighter.c:383-388`).
  Assertions `check_reaction_predicates`/`check_reaction`/`check_winner_body`;
  6+ mutation proofs.
* **The winner gate (`0x18950`)** — a **faithfulness fix**. The record's first
  draft misread the pair table's odd record as `0x0000FF00`; the raw bytes at
  `0x9F290`/`0x9FE90` are little-endian **`0x00FF0000`** (bits 16..23). With the
  correction both demo queries return 0, the raw reaches the position compares at
  `0x196BC`, and `0x196CE` zeroes `AFC` → side 0 (the T-rex) — matching cycle 3
  §10.2. The §7.1 "conflict" was a transcription artifact, now resolved;
  `0x18950` does **not** gate the demo.

**The measured observable.** `make demo-oracle` still takes the `res is None`
fallback (`tools/title_compare.py:484`): front-end window `[560..830]`, demo port
frames `[313..1380]` (1068), **`0/1068` exhibited**; demo window `[831..3616]`
(raw `3670..8409`), **2786 frames: 0 clean, 0 splice, 0 transition, 2779
unexplained** (7 all-black excluded); **first unexplained captured frame 832
(raw 3671)**. The 482/834 witness is **unchanged**: `frame_0482.raw` vs
`frontend/frame_0834.raw` = **18 294 B (9.5 %) / 6 194 px** (left 1 549 + right
4 645); capture 834 is still the best match over 830..840.

**The `0x17FA0` tail (Task 6, `bfd22cb`).** Task 5 measured `AF8`/`AFC` at 0 at
every state-7 frame because `camera_project` (`0x17FA0`) hardcoded the page flag
`DS_00100B60`/`B61` to 0 and never populated the visibility boxes
`0x100AC0`/`0x100AC8` — both outputs of the tail `0x16AFC` (601 B) / `0x164F4`
(547 B) (`0x180C9`/`0x18108`; demo-fight record §7.4 item 2). With
`0x100AC8[side] == 0`, `camera_unfreeze` returned at its visibility gate
(`0x1715e`/`0x17182`), so `B18`/`B10`/`B30` stayed 0, `B54` stayed 0, `AF8`
stayed 0, and `fighter_pass_a`'s tail never ran `0x193B0`. **Task 6 ported the
tail** (`0x16AFC`/`0x164F4` plus `0x16734`/`0x164C0`; record §11), wired at the
raw's `0x180C9`/`0x18108` site. It now fires (`camera_page_tail_b(1)`, ~18
state-7 frames), `B61 = 1`, `camera_unfreeze(1)` writes `AF8[1] = 5/3/2`,
`0x193B0` runs, and **892 port frames change from frame 489** (the winner's
blood/reaction effect). The fighters now byte-match at 482/834.

**The residual (owner: demo-fight-closure).** The 482/834 witness is unchanged
because the residual 18 294 B is **not** the fighters: it is the **arena
backdrop's missing dark mountain silhouette** in `platform/render.c` (`0x38730` →
`0x387F4`/`0x38890`/`0x38A38`, the scene actors `DS_000BDFBC`/`DS_000BDFC0`).
That is the demo-fight-closure cycle's subsystem, not the tail. **Cycle 5
(below) corrected this attribution:** the port did draw the scene actors, and
the missing layer was the crowd actor 0's mountain children, killed by
`actor_spawn`'s tail (`0x2B0D4`) in `port/src/game/actors.c` — not `render.c`.

**The size gate.** The union (68 f / 13 131 B) is over the gate and was already
ratified as this cycle's scope; the winner gate (1 f / 152 B) and the Task-6 tail
(4 f / 2 154 B) are under it; no group grew past the record's measurement.

**Named gaps carried (record §7).** `0x19020` (confirmed no-op; `slot+0x18`
never set), `0x38154`, the `0x3Fxxx` closer script (unreachable), 831/832's
held-frame presentation (un-derivable), the `0x2C3FC` voice stub, and the
interactive match (unowned). The `0x16AFC`/`0x164F4` page-flag tail (§7.11) is
**ported** (Task 6); the deferred minors are listed at record §7.0.

**Every enforced oracle claim is unmoved.** `make verify` exits 0 with 0
warnings: title `54 clean, 55 splice, 2 transition, 0 unexplained` and
`54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at capture frame
215`; front-end `[560..830]` / 271 frames `0 unexplained`; smk `120/120` +
`41/41`; C-vs-Python `9866 writes byte-exact`; `symbols.h` regenerates
byte-identically (`1304 globals, 1206 functions`). (Cycle 5 below moves the
front-end claim to `[560..842]` / 283 / 2 unexplained (832, 833), and cycle 6
to `[560..850]` / 291 / 2; every other claim stays at these values.)

## Demo fight cycle 5 — arena backdrop outcome (Task 3)

Cycle 5 (branch `arena-backdrop`, spec
`../../docs/superpowers/specs/2026-09-24-arena-backdrop-design.md`, record
`../../docs/superpowers/plans/2026-09-24-arena-backdrop-derivations.md`) closed
the state-7 arena's missing horizon layer. **The cycle's gate is MET** and the
front-end claim moved once, absorbed by policy.

**The layer and its owner.** Cycle 4's attribution was **wrong twice over**:
the port *did* draw the arena's scene actors (layer 2 = sky, id 11232; layer 1
= sea, id 11233), and the missing dark mountain silhouette was not
`platform/render.c`'s scroll path. The missing layer was the crowd actor 0's
**mountain children** (sprite ids 756/757/758, layer 94, `px` 152/320/440),
which `actor_spawn` spawned and then killed because its tail (`0x2B0D4`) tested
only for the stub `0x5D812` instead of calling the actor type's callback
(`DSD(0xBB9DC + type*0xC)`) and testing `AL`. Type `0x1B`'s callback (`0x412FC`)
returns 0 (visible), so the faithful dispatch revives the layer; the same
predicate killed five type-0x01 demo spawns (measured: 0 of 1381 dumped frames
differ). The port is `port/src/game/actors.c`'s 16 non-stub callbacks plus
`0x2BE5C` and the `0x2B0D4`/`0x2B185` dispatches; the size gate did **not**
trigger (closure 25 f / 1 544 B; 17 f / 1 148 B new; the `0x2C3FC` sensitivity
stands as recorded).

**The measurement.** Port 482 ↔ capture 834 = **0 / 192 000 B (0 px)**, from
18 294 B / 6 194 px; port 483–487 ↔ captures 835–839 are also 0 B each. The
report-only demo oracle still takes the `res is None` fallback: demo port frames
`[490..1380]` (891), `0/891` exhibited; window `[843..3616]` (raw
`3750..8409`), 2774 frames: 0 clean / 0 splice / 0 transition / 2768
unexplained (6 all-black excluded); first unexplained **843 (raw 3750)** — the
T-rex pose, out of scope.

**The claim move.** Front-end `[560..830]` / 271 / `0 unexplained` →
**`[560..842]` / 283 / `2 unexplained (832, 833)`** — absorbed (human-decided)
in Task 2 with the claim updated in the same commit: the window is derived from
the port's own dump, so a correct arena render necessarily extends it (the fix
explains captures 834..842), and 832/833 are pre-existing, out-of-scope gaps
with named owners (the state-9 hold's animation / the loader's presented DAC
state), allowed by name in `tools/title_compare.py`'s
`FRONTEND_ALLOWED_UNEXPLAINED`; any other unexplained frame still fails.

**Every other enforced oracle claim is unmoved.** `make verify` exits 0 with 0
warnings: title `54 clean, 55 splice, 2 transition, 0 unexplained` and
`54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at capture frame
215`; smk `120/120` + `41/41`; C-vs-Python `9866 writes byte-exact`;
`symbols.h` regenerates byte-identically (`1304 globals, 1206 functions`).

### Demo pose — cycle 6 (branch `demo-pose`), the T-rex pose handler

**Outcome: the goal (the whole fight window `[843..1884]` at 0 unexplained; first
unexplained 843 → 1886) was partly reached; the residual is named.**

* **What landed.** The pose handler `0x3A43C` (the derivation record corrected the
  design's `0x39CC8`) and the frame-hold scaler `0x39A34`; then, from the 43 px
  differential (record §9), `0x3A43C`'s context reads at the raw `RET 4` stack
  offsets (`0x2BC30`/`0x2BCEF`: `side = ctx[1]`, `rec_self = ctx[5]`) and the
  `0x186C4` two-slot re-latch at `0x35829`.
* **Size gate.** Did not trigger: genuinely-new closure 2 f / 245 B (record §4).
* **Measured.** Direct pairs port 490 ↔ capture 843, 491 ↔ 844, 496 ↔ 850 are
  **0 B** (port 490 ↔ 843 was 16 101 B / 5 793 px before, 22 919 B / 7 879 px
  with the handler alone). The demo oracle's first unexplained moved **843 → 851
  (raw 3758)**; the fight window `[851..1884]` (raw `3758..4791`) holds 1034
  frames, **0 explained**.
* **Moved claim.** Front-end `[560..842]`/283 → **`[560..850]`/291: 130 clean,
  157 splice, 0 transition, 2 unexplained (832, 833)**, ruled and absorbed in the
  same commit as the fix (raw `0x2BC30` `RET 4` + `0x35829` `CALL 0x186C4`).
* **New enforced line.** `make demo-fight-oracle` (in `make verify`) is a
  **ratchet** on the first unexplained fight-window frame: it fails when that
  frame is earlier than N = 851 (pinned in the `Makefile` with its provenance) or
  the window collapses; a later frame passes as an improvement and N is raised.
  It outputs `first unexplained captured frame 851 (raw 3758); 1034 unexplained
  in the fight window; ratchet N 851`. Near-vacuous today (0/1034 explained); it
  guards the front-end window's end and the fight window's start.
* **Residual (owner named).** Capture 851: the port advances the T-rex's roar one
  frame early at f = 80; likely owner the roar stream's frame-hold timing
  (`0x39A34`'s `rec+0x24`, read by `frame_timer` `0x2AA70`; record §9.5).
  **Superseded by the roar-timing fix below**: the owner was the pose setter's
  operand, not the timer.
* **Open named gaps.** `0x354F0` arena-wall clamp; `hit_record_x/y` omit the
  raw's `0x18540`/`0x18350` calls (first differs at f = 90); the camera split-arm
  `0x18714` write; the sibling pose handlers `0x3A588`/`0x3A6D4`/`0x3A820` and the
  `0x39F40`/`0x39CC8` family (unreached in the first fight); the `0x2C3FC` voice
  stub (RNG- and fight-state-neutral, record §3.4). Carried items: record §6.3.

### Roar timing (branch `roar-timing`), capture 851

* **Cause (raw).** The timer path is faithful: `0x2AA70`, `0x39A34`, `0x2BC30`
  and `0x2A1FC`'s gate re-diff clean. The early advance came from the value
  `0x39A34` scales. `0x3AAFC` loads the pose setter's EDX at `0x3AD23..0x3AD2E`
  as `mov esi,[esp+0x18]` / `mov esi,[esi+3]` (`8b 76 03`) / `sar esi,0x18`
  (`c1 fe 18`), which is the **signed byte at anim3 row +6**. The port read +3.
  For the demo's row `0xDE95F` (`08 18 08 73 02 0e fd 11`) the operand is −3,
  not 115. So `0x3A504`'s `slot+0x7E = byte[0xBECF8] + CL` is `0x14 − 3 = 0x11`
  (the port had `0x87`), and the roar's `0x39A34` hold is **17 / 10 = 1.7**,
  not −12.1. With −12.1 the timer stepped one sprite per frame. With 1.7 each
  roar sprite holds about two frames.
* **Fix.** One line in `fighter_reaction_apply` (`0x3AD23..0x3AD2E`). The seeded
  assertion `check_pose_entry` → `slot+0x7E == 0xC4` is mutation-proven: +3, +5
  and +7 each fail it.
* **Measured.** Port 497 ↔ capture 851: 6 544 B → **0 B**. Captures 851..857
  are explained. The demo's first unexplained is now **858 (raw 3765)**, and
  the fight window `[858..1884]` has 1027 frames, 0 explained. The ratchet N is
  raised 851 → 858.
* **Moved claim (allowed by the brief).** Front-end `[560..850]`/291 →
  **`[560..857]`/298: 134 clean, 160 splice, 0 transition, 2 unexplained
  (832, 833)**. Nothing else moved.

### Mode-1 x (`b2cb490`), capture 858

* **Cause (raw).** The camera starts moving at f = 85 (`cx = −86`, the T-rex's
  `slot+0x34` passing the `0x1800` dead zone). From f = 86 the scroll fill
  `0x38A38` writes a non-zero ramp, and the pset x writer `0x2A690` scrolls
  the mode-1 props from it. Its two mode-1 arms read their operand as the high
  word of a dword load: `0x2A733 mov eax,[ebx+0x44]` / `0x2A739 sar eax,0x10`
  (`8b 43 44` / `c1 f8 10`) is the ramp entry just stored at `rec+0x46`
  (`0x2A72F`), and `0x2A6E0 mov eax,[ebx+0x32]` / `0x2A6E9 sar eax,0x10`
  (`8b 43 32` / `c1 f8 10`) is the velocity word at `rec+0x34`, the one
  `0x2A6D9` gates on. The port read the low words `+0x44` and `+0x32`. So the
  temple, props and crowd (ramp arm) did not scroll, and the `0x02F4` mountain
  (velocity arm, `+0x32 = 9362`, `+0x34 = 320`, `0x107A46 = −10`) moved
  365/64 = 5.7 px instead of 12/64 = 0.2 px at f = 86, with its `0x02F5`/`0x02F6`
  children.
* **Fix.** Two operand reads in `actor_pset_point`. The seeded assertions in
  `check_type_callbacks` (`test_fight.c`) use the demo's f = 86 values: temple
  `0x02F2` → x = 536, mountain `0x02F4` → x = 6316. Each is mutation-proven:
  the pre-fix `+0x44` read gives −3310, the pre-fix `+0x32` read gives 6669,
  an unsigned ramp word gives −130 536, and a flooring `>> 8` gives 6317.
* **Measured.** Capture 858 ↔ port 502/503: 13 617 B / 5 253 px → a **0-byte
  splice** at byte 64 254 (row 66). The demo's first unexplained is now
  **859 (raw 3766)**, and the fight window `[859..1884]` has 1026 frames, 0
  explained. The ratchet N is raised 858 → 859.
* **Moved claim (allowed by the brief).** Front-end `[560..857]`/298 →
  **`[560..858]`/299: 134 clean, 161 splice, 0 transition, 2 unexplained
  (832, 833)**. Nothing else moved.

### Walk arrival (`afa47b3`), capture 859

* **Cause (raw).** The scene/effects pass `0x49C78` dispatches a type-1
  (walking) fight-effect entry to `0x49D2F`. When `|actor+0x18 − entry+0x14|`
  is at most the step `[actor+0x32] >> 16` (the word `+0x34`, negated when
  negative; signed `jg` at `0x49D79`), it calls `0x4AC38`. That clears the
  actor's hflip, sets `+0x29` bit 0x10, zeroes `+0x34/+0x36/+0x38`, returns
  the entry to type 0 and begins the `0xC9544[index]` stream with the hold 5.0
  (`push 0x40a00000`). The port left type 1 as a named gap, so the demo's
  type-0x20 worshipper (entry `0x1083CC`, target −4303) walked past its target
  at f = 87 instead of switching to its arrival sprite `0x0836`.
* **Fix.** `fight_4ac38` and the `case 1` body in `fight_effects_pass`. The
  seeded assertions in `check_effects_arrival` (`test_fight.c`) use the demo's
  f = 87 values and are mutation-proven: no case 1, a word read of `+0x32`, no
  negation, `<` for `<=`, the hold 3.0, and each dropped store in `0x4AC38`.
  Type 2 (`0x49D90`) stays a named gap.
* **Measured.** Capture 859 ↔ port 503/504: 449 B / 159 px → a **0-byte
  splice** at byte 90 582 (row 94). The demo's first unexplained is now
  **860 (raw 3767)**, and the fight window `[860..1884]` has 1025 frames, 0
  explained. The ratchet N is raised 859 → 860.
* **Moved claim (allowed by the brief).** Front-end `[560..858]`/299 →
  **`[560..859]`/300: 134 clean, 162 splice, 0 transition, 2 unexplained
  (832, 833)**. Nothing else moved.

### Slot `+0x42` reset (`b915712`), capture 860

* **Cause (raw).** The roar reaction sets the side-0 slot's `+0x42` bit 0
  (`0x3ABDA`) at f = 72. The raw clears it in the same frame: `0x49C78` ends
  with an unconditional `call 0x4A634` (`0x4A591`), which for each fighter slot
  does `and al,0xfc` on `+0x42` (`0x4A6D7..0x4A6E0`) and zeroes the side's
  `0x10889E`/`0x1088B2` bytes. Before the clear, when `+0x42` bit 1 is set, the
  side's `0x1088A8` reaction byte draws the crowd-voice RNG: rng(3) in
  `0x20..0x3F`, or rng(2) and, when that is non-zero, rng(2) again in
  `0x10..0x17`. The voices (`0x2C3FC`) are out of scope. The port skipped the
  call, so at f = 88 `0x4AB7F` still saw bit 0 and retargeted the worshipper to
  type 8.
* **Fix.** `fight_4a634` and its call before `DS_001088C2 = 0` (`0x4A5A0`). In
  the demo only f = 72 has set bits, and the reaction bytes (`0x0B`, `0x01`)
  draw nothing. `check_effects_tail` (`test_fight.c`) proves the clears, the
  two ranges and their bounds, the second-draw gate and the side index by RNG
  state, and every assertion is mutation-proven. `check_effects_arrival` gains
  the `DS_001088C2` → `0x4BD4C` pre-gate cases.
* **Measured.** Capture 860 ↔ port 504/505: 476 B / 164 px → a **0-byte
  splice** at byte 117 108 (row 121). Captures 861..863 are explained. The
  demo's first unexplained is now **864 (raw 3771)**, and the fight window
  `[864..1884]` has 1021 frames, 0 explained. The ratchet N is raised 860 → 864.
* **Moved claim (allowed by the brief).** Front-end `[560..859]`/300 →
  **`[560..863]`/304: 136 clean, 164 splice, 0 transition, 2 unexplained
  (832, 833)**. Nothing else moved.

### Grey flier (`7147288`), captures 864/865

* **Cause (raw).** The grey figure at the left edge of captures 864/865 is the
  flier `0x1282C` spawns from `0xBB254` (type `0x01`, stream `0xE8A94`, sprite
  `0x0281` hflipped at (−7, 99), effects palette `0x105FF3C`). Type `0x01`'s
  cb1 `0x127C0` pops the `0xF0A78` node list. State 6's reset `0x20DF4` builds
  it at `0x20E33` (`call 0x12750`), and the port had left that call in the
  `0x20DF4` gap, so every spawn was refused. The gate
  `(DS_000EF6DC & 0x3F) == 0` reads the loop counter `0x24C5C` increments
  (`0x24CDB`, a word since boot). The front-end driver skipped the attract and
  title with it at 0, so the gate would have opened at f = 81 rather than
  f = 91.
* **Fix.** `camera_dust_list_init` (`0x12750`), called before
  `fight_list_init` as in the raw. The driver seeds the counter to 886, the
  port's own boot-run count before state 2. The capture bounds that count to
  886 + [−4, +17], and the flier pins it mod 64. `check_dust_list`,
  `check_state6` and the driver's first-flier frame (loop 1097, capture 864)
  are mutation-proven.
* **Measured.** Captures 864 (port 508) and 865 (508/509 splice) are now
  0 B, from 495 and 498 B. The demo's first unexplained is now **866 (raw
  3773)**, and the fight window `[866..1884]` has 1019 frames, 0 explained. The
  ratchet N is raised 864 → 866.
* **Moved claim (allowed by the brief).** Front-end `[560..863]`/304 →
  **`[560..865]`/306: 137 clean, 165 splice, 0 transition, 2 unexplained
  (832, 833)**. Nothing else moved.

## Landmarks (verified)

| Address | Meaning |
|---|---|
| `0x624D4` | `_start` |
| `0x6C435` | CRT `main` wrapper → `0x1BEC4` |
| `0x1BEC4` | **game main** (init + dispatch) |
| `0x4FBA2` | `int 10h` video-mode query |
| `0x1B120` | `INDEX` loader / resource table builder |
| `0x1B544` | resource-handle → pointer (30 callers) |
| `0x1C0F0` | extended-memory probe + memory-block list |
| `0x1C308`/`0x1E458` | 20-caller allocator wrapper |
| `0x6C484` / `0x6C4CF` | CRT init / exit |
| `0x72C54` / `0x72CA6` | CRT argv helpers |
| `0x255CC` | master frame loop |
| `0x24C5C` | per-frame update (frame counter, 2 player records, update process table) |
| `0x11D04` | state machine `switch(DAT_000F0A64)` |
| `0x11000` | per-state render/play dispatch (13 cases) |
| `0x1C740` | VBlank-gated animation/FLIC blit of `DAT_000E87A4` (not the screen write) |
| `0x1C470` | palette dirty-list flush → VGA DAC |
| `0x2D62C` | tick handler `DAT_00105D88++` (interrupt handler; vector unresolved) |
| `0x51F45` | 320x200 double-buffer + scanline table setup (`&DAT_001088F8`) |
| `0x50188` | swap back/fore buffers `DAT_000E87A0` ↔ `DAT_000E87A4` |
| `0x501A3` | dirty-dword blit `DAT_000E87A4` → literal aperture `0xA0000` |
| `0x255CC` frame-loop full copy | 64000-byte `DAT_000E87A4` → literal `0xA0000` when `DAT_001014FC != 0` |
| `PTR_FUN_000A8644` / `_DAT_00104AE8` | update process table / bitmask |
| `PTR_FUN_000A86C4` / `_DAT_00104AEC` | render process table / bitmask |

## Open questions

1. ~~Where the per-frame main loop / state machine is~~ — **resolved**: see
   "Frame loop and process scheduler" above.
2. Meaning of the `INDEX` flags byte (`0x01` vs `0x02`).
3. ~~`FUN_000109A0` / `FUN_00010830` semantics~~ — **resolved**: DPMI region
   **lock/unlock**. `0x109A0(ptr,size)` → `0x10830(ptr, ptr+size)`; the unlock
   twin is `0x109CA` → `0x108E8`. Both thunk `0x61564`. 36 vs 29 callers.
   `main` locks 8 regions (ISR code `0x2D62C`, `0x1B610`+`0x4000`,
   `0x62451`+`0x1000`; data `0x101508`, `0x105D88`, `0xEF6DE`).
4. `DAT_00101524` memory-block list layout and the `0x600` / `0x200` block sizes.
5. ~~Which code the framebuffer write path uses~~ — **resolved**: the write
   target is the literal (non-relocated) address `0xA0000`, the VGA mode-`0x13`
   aperture, reached by a plain dword copy / dirty-dword blit from
   `DAT_000E87A4`; no VBE LFB, no `4F05` banking. **The earlier claim that
   `0xA0000` had zero references was a false negative**: it is written as a
   *literal immediate* (`mov edi/ebx, 0xa0000`), which the decompiler renders as
   `DAT_000A0000`, and the fixup right beside it (`0x501AC`) covers only the
   `DAT_000E87A4` operand — so it is not a relocated data pointer. See "Present
   / framebuffer write path" and "Aperture conflict".
   **Port rule (verified):** `mem[0xA0000]` is data-object offset `0x20000`
   (live pointer table / LUTs), so screen output must never target it; the port
   keeps its frame buffer outside the mapped data object and renders the
   320×200 index buffer to the window.
   **Still open:** which interrupt vector carries the tick `0x2D62C` (the rate
   is settled — 60.05 Hz measured, agreeing with the static ÷`0x3c` conversion;
   see "Tick" — but no install site names the handler). This is the one item
   Task 13 could not settle.
6. Why `main` gates on `int 10h` mode `0x13` (320x200, matching the `0x51F45`
   surface) while the installed set is `S16` (640x480).
