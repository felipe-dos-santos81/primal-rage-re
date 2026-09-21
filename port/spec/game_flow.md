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
* **Declared rendering gap.** `attract_scene_tick`'s only shipped callee
  `0x4F7F4` (the `DS_000A8744[0]` effect-palette driver: it advances
  `DS_001088F1` through the 10-entry table at `DS_000C98A0`, enqueues each entry
  via `0x33874`, and clears `DS_00104AD0` bit 0 after 10 frames) is unported, as
  is the starter `0x4F83C` that sets `DS_00104AD0 |= 1`. The port sets no mask
  bit, so the attract's palette animation never renders; the attract oracle's
  first divergence (`tools/attract_compare.py`, capture frame 68 raw 1626/1621)
  is exactly this producer.

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
  write its type byte, so **types 1 and 5 have no producer and are dead**. An
  end-to-end unit test proves spawn → `effects_step` → palette dirty list →
  `gfx_dac` (the palette actually presented through `gfx_present`).
* **The camera/scene layer — `0x1324C` ported, the rest deferred (unowned gap).**
  `0x1324C` (screen-shake decay, update-table entry 0) lives in `effects.c` and
  maintains `DS_000F0AF4`/`DS_000F0AF6`. It draws nothing: the plan's assumed "missing
  draw" half of the effect render path **does not exist**, and none of the camera/scene
  functions draws — the state is consumed by the existing render pass (`0x14328`) and the
  actor-pset sync. `0x1324C` is **dormant**: no shipped store sets `DS_00104AE8` bit 0, so
  it never runs; it is registered only so the existing update-table dispatch reaches it if
  bit 0 is ever set.
  **Deferred and unowned by this plan:** `0x12CD4` (the camera-y stepper), `0x1317C`
  (camera-y clamp), `0x13290` (mode-2 two-player centering) and `0x1333C` (mode-3
  one-player centering), plus the dispatcher chain that is their only caller — `0x12D48`
  with its modes `0x12DF0`/`0x12E3C`, and `0x12DA8`/`0x131F8`/`0x13224`. No task in this
  plan owns that chain, and the raw gives `0x12CD4` exactly one caller (`0x1317C` at
  `0x131cb`), so all four are unreachable in the port; shipping them would be dead
  production surface. A later cycle must port the dispatcher chain first.
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
  this plan** — no dispatch path is shipped, and `port/tests/test_frontend.c` pins
  that no handler is registered and that state 5 neither arms `DS_00104AE4` nor
  leaves mode 3. `0x41578`'s register-level comparison against `0x88874B0` is
  **dead in the port's flat model**: `0x88874B0` is above `MEM_SIZE`
  (`0x4000000`) and outside both LE objects, so that half of the predicate can
  never match. Because the four `0x41578` sites are themselves unreachable, no
  port code holds the comparison; the raw fact is recorded here rather than
  deleted or substituted. **A later cycle that makes `0x41578` reachable must port
  the comparison with it, including the never-true `0x88874B0` half** (register-
  level fidelity — not deleted and not substituted). No shipped path spawns types
  0/2/4/6 yet; the producers
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
3760 distinct post-logo frames, raw 1373..8409 after Task 9's re-capture) reaches
the front-end. With the state-3 render ported (`0x12484`) the `PR_FRONTEND_DUMP`
driver emits the real zoom-out, and `tools/title_compare.py --frontend` aligns
it: window distinct [557..810] (raw 3113..3414), **254 frames: 100 clean, 150
splice, 3 transition, 0 unexplained** (the demo section below records why the
window indices moved from the pre-Task-9 `[557..813]` / 257 frames). The claim that result supports is precise and narrow: **no
content-bearing capture frame inside the window the port's own dump exhibits is
unexplained.** The window is derived from the port's dump (`check_capture`'s
`idx`→`a,b` mapping) and the branch discards `check_capture`'s `rc` (coverage and
`endpoints BAD`), so `unexplained` is only ever evaluated inside that
self-derived window. The oracle therefore **cannot detect a port that
under-renders the front-end**: a dump of 300 identical copies of port frame 0
(nothing rendered past the state-3 entry) exits 0 (window `[557..559]`, 2 clean,
0 unexplained, exhibited 1/300), and a dump of only the first 12 real frames
exits 0 (window `[557..569]`, 0 unexplained, exhibited 9/12). `frontend-oracle`
keeps enforcing the gate (the Python compare exits non-zero on any unexplained
frame) and remains a `verify` ladder step; it was proven to fail on an induced
regression (exit 1 — the unexplained count is corruption-dependent, not a fixed
2).

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
`port/tests/test_frontend.c` asserts the five values and the two observable
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
frames are excluded: distinct 0, 213, 353, 386, 420, 454, 488, 522, 558, 833,
1876, 2088, 2128, 2392, 3426, 3552 (raw 1373, 2178, 2425, 2533, 2642, 2750,
2859, 2968, 3114, 3675, 4792, 5401, 5807, 6758, 7810, 8202, in the Task 9
re-capture). One of them,
capture 558 (raw 3114), is the frame the earlier report named: a fully black
frame between two identical state-3 entry frames (caps 557/559, raw 3113/3115,
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
unexplained. **What the investigation could not settle** is whether capture 558's
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
the dump to 1381 frames, so the 120 s capture (3712 distinct post-logo frames)
now runs past the dump instead of ending before it; but that is not the whole
story: the ignored coverage is exactly what lets a port that **under-renders**
the front-end pass (counterexamples above). Passing the gate therefore means "no
content-bearing capture frame inside the port-exhibited window is unexplained",
NOT "every dumped port frame was exhibited" and NOT "the front-end was
rendered".

## Demo fight states 6/7 (demo-fight cycle 1, report-only window)

The attract demo's CPU-vs-CPU fight is states 6 (`0x11A8C`) and 7 (`0x11E8F`,
the arena frame `0x263F4`, ended by `0x11BCC`'s timer exit). It is **not** the
interactive match: that is the `DS_00104B00` mode graph (the `0x257A4` coin
divert, `0x1EEB0`), which no task in cycle 1 owns. The port advances the
front-end into the demo unaided. State 3's phase-1 terminator (`0x12658`) hands
to state 9 with a 240-frame timer and target 6; state 9 counts down; state 6
picks the two characters from the shared RNG, seeds `DS_001082C8`, spawns both
fighters and arms the 900-frame timer; state 7 runs the arena loop until
`0x11BCC` restores `DS_000F0A64 = DS_000F0A6C` and the state machine moves on.

**Dump length.** `make demo-oracle` runs the same `PR_FRONTEND_DUMP` run as
`frontend-oracle`. The driver loops 2000 frames from the state-2 entry and caps
the RGB dump at 1400 frames (`PR_FRONTEND_DUMP_FRAMES`, default 1400). The
measurement that sizes it: state 3 enters at loop 589 (dumped frame 0), state 6
at loop 1070 (dumped 481), state 7 runs loop 1071..1969 (dumped 482..1380), and
`0x11BCC`'s exit is loop 1970, where `DS_000F0A64` drops to 0 so the `state >= 3`
dump stops. The dump therefore holds **1381 frames** (dumped 0..1380); the 1400
cap covers it with no truncation, and the 2000-frame loop clears the 1970 exit.

**The demo window is report-only, and its first unexplained frame is the state-9
hold — not the first landing hit.** `tools/title_compare.py --demo` locates the
front-end window with the same content alignment, then classifies the capture
region after it against the port dump frames after the last frame that window
exhibits — the same clean/splice/transition/unexplained model, no second one. It
reports and exits 0.

* Front-end window (still enforced in `verify`): distinct **[557..810]** (raw
  3113..3414), **254 frames: 100 clean, 150 splice, 3 transition, 0
  unexplained**. These numbers moved from `[557..813]` / `257 frames: 92 clean,
  162 splice, 2 transition` when Task 9 added the demo pins and re-captured
  `frontend`. The move is the **capture's, not the port's**: the capture is
  host-timed and not reproducible (a `title_capture.py --verify-reproducible`
  run of the pinned original gave 587 vs 588 distinct frames and a first
  divergence at distinct index 30), so the distinct-frame indices shift between
  captures while the oracle's claim stays the same (`0 unexplained`).
* Demo window: distinct **[811..3759]** (raw **3421..8409**), **2949 frames:
  0 clean, 0 splice, 0 transition, 2942 unexplained** (7 all-black capture
  frames excluded as artifacts). Demo port frames [259..1380], **0/1122
  exhibited**.
* **First unexplained captured frame 811 (raw 3421)** — the first frame after
  the front-end window.
* **The window is non-discriminating by construction (a plan flaw, not an
  implementation flaw).** `--demo` defines the window as the capture region after
  the front-end window; that window's last exhibited port frame is 258, inside
  this state-9 hold, so the demo window opens on state 9 and diverges at its
  **first** frame — before state 6. It would read identically for correct or
  entirely broken Tasks 2–8. Re-anchoring the port side to the state-6/7 entry
  (dumped 481) was measured and does **not** restore discrimination: no capture
  frame after 811 exhibits any port frame in [481..1380] (**0/900 exhibited**),
  because the port's state-7 arena render is itself a cycle-2 gap (the broken
  globe background, `0x3C88C` §7.7). Cycle 2 must re-anchor the window once the
  arena render lands.

Cycle 1's declared bound expected this frame to be the original's first landing
hit (the cycle split gives cycle 2 collision and damage). Task 9's verification
**retires that expectation**: the frame is in the **state-9 hold**, before
state 6, and neither a motion-layer pin nor an RNG pin can move it.

* Capture 811 is the "WHO WILL RULE THE NEW URTH?" globe screen. Its closest port
  frame is **264**, 205 differing bytes in rows 98..144 — a sprite/content band
  on the globe, not a whole-frame change. Port frame 264 is inside the state-9
  hold: state 3 hands to state 9 at dumped frame 240 (`0x12636` sets
  `DS_000F0A64 = 9`; `0x12645` sets the `0xF0` timer), state 9 runs dumped frames
  240..480 and state 6 runs 481, so dumped frame 264 is state 9. `0x11D04`'s
  case-9 arm (verified in the fixed-up image) only decrements `DS_000F0A6A` and,
  at zero, restores `DS_000F0A64 = DS_000F0A6C`; it draws nothing.
* The difference is the globe's island/landmass content: the capture draws it on
  the later rotation steps (captures 811..832), the port draws it on only some
  (`+264`/`+276`/`+282`/`+288` omit it; `+270` shows a smaller one), so the
  port's state-9 hold render is coarser than the original's. The capture's demo
  fight does not begin until capture **836**, 25 capture frames after the
  divergence, so the divergence cannot be a landing hit.
* The port's state-7 output now moves (Task 8): dumped frames 482..1380 hold
  **64 distinct images** over loop frames 1071..1969 (55/10/1 per third), versus
  one before. The motion then stalls at the `0x3CF38` hit chain (side 0 reaches
  `+0x52 == 0x0E`, a table no-op; side 1 reaches `+0x52 == 3`, whose `0x35D7C`
  handler needs `0x3CF38`). That is cycle 2's combat chain, not a determinism
  site.

**Two pins were added, for the demo's state-6 character picks; the state-9 hold
itself has no pinnable site.** A pin is a determinism fix — a site where the
original reads uninitialised or timing-dependent state — never a value chosen to
make a frame match. The first unexplained frame's cause is a render gap, not such
a site:

* State 9 draws no RNG (`0x11D04` case 9, above; the port's case 9 is faithful,
  and `port/tests/test_flow.c`'s `check_state9_countdown` asserts the LCG state
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
  `test_frontend.c`) — the same pattern the title driver uses for the pinned
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
  (`test_frontend.c`), and `check_state6` asserts the full 14-draw model and the
  dust entries' fields.
* **Residual.** The dust entries' type-0 processing (`0x49C78`'s default arm →
  `0x4AAD0` and its callees) is still a named gap, so the dust's motion and
  despawn are not faithful; its actor is spawned and rendered (the descriptor
  `0xBB4C0`, type `0x24`, whose per-type callback is the `0x5D812` stub). The
  demo window's first unexplained frame is still 816 (the state-9 hold render),
  so the oracle cannot measure the dust's pixels until Task 3 lands.
  `tools/title_pin.py`; `host.c:203`; `flow.c`; `fighter.c`; `fight.c`;
  `actors.c`; `effects.c`.

The demo window therefore cannot converge in cycle 1; cycle 2 owns it: collision
and damage (`0x3BB90`, `0x4FB20`, `0x3BAEC`, `0x3B9D8`), the `0x3CF38` hit chain,
the arena draw helper `0x3C88C` (§7.7), the state-9 hold's zoom-actor globe
render (the `0x3E688` palette-driven zoom background spawned by
`0x12484`/`0x12658` and advanced through the actor path `0x2A31C`), and the
`- LOADING -` screen the capture shows at capture 834, which the port does not
draw. The window's closure retires this bound rather than narrowing it.

The gaps that may own the missing state-7 composition are `fight_slot_pass`'s
`0x3C88C` draw helper (§7.7) and the skipped `0x20DF4` fight reset at `0x11AC4`
(below); the derivation record named both as fidelity gaps, and this measurement
shows at least one of them is load-bearing for whether the arena renders at all.

**The skipped `0x20DF4` is a liveness gap, not only a fidelity gap.** The raw
calls `0x20DF4` at `0x11AC4`; its first callee `0x49300` self-links the
effect-list sentinels `DS_001083C4`/`DS_0010884C` and seeds `DS_001088CC`/`CB`.
The port skipped `0x20DF4` as a named gap, but `fight_effects_pass` (`0x49C78`)
reads `DS_0010884C` as a circular-list head and loops until it returns to the
sentinel; with the global left at 0 the walk never terminates and the demo hangs
on its first state-7 frame. `fight_list_init` ports `0x49300` verbatim (from the
raw disassembly), so the walk is a no-op on the empty list exactly as the
original's is. `0x20DF4`'s other resets remain a named gap.

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
