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

## Boot logos — `0x1C740` (sub-project 2b-i, video)

`FUN_00011000` case 0 (the attract sub-machine's entry) plays the two boot
Smacker movies through two `0x1C740` calls before assigning title state 1.
`0x1C740` is the VBlank-gated FLIC-style blit of `DAT_000E87A4` listed above;
its movie path is a licensed Smacker-library open/decode loop (`0x6345C`
open/decode, `0x63180` stream setup). The port ports the player (video only)
and wires `movie_play("twi5.smk")` then `movie_play("twg.smk")` at that case-0
site (`port/src/game/flow.c`), decoding into `DAT_000E87A4` and presenting
through `gfx_present`. The original presents TWI5 120 of 121 frames and TWG 41
of 41; the rule that decides presentation is the player's and is content-based
(`TODO(verify)` on the original loop's exact semantics). Streamed Smacker audio
is sub-project 2b-ii and not ported. See
`../../docs/superpowers/plans/2026-09-17-smacker-video-report.md`.

## Title state — `0x121A0` (sub-project 4a-ii, ported)

* **Title/attract state is index 1.** `FUN_000121A0` is case 1 of
  `switch(DAT_000F0A64)`. The attract sub-machine `FUN_00011000` case 0xb
  assigns state 1 when its cycle counter `DAT_000F0A5C == 0`; the port enters
  state 1 directly (the attract sub-machine is deferred to 4d). That the
  shipped title *is* state 1 is now confirmed at runtime by the oracle driver,
  which asserts `DS_000F0A64 == 1` and `DS_000F0A66 == 0x600` on the entry frame.
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
  `DS_00105C05 = 1` (text row; screen rows 7–12) is still seeded: its init writer
  `0x2BF00` writes `0x1D` and the captured row comes from `0x2C06C`'s
  attract/mode writer, both unported. The live credit countdown
  (`FUN_0002CA48`/`FUN_0002CA7C` via `0x11F28`) is a declared 4b gap. Diagnosis:
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
  `gfx_dac`. The effect **render** path is still unported, so no shipped path
  spawns types 0/2/4/6 yet; the producers are a declared coverage gap carried by
  unit tests, and `0x134C0`'s drain is unit-proven. Details:
  `../../docs/superpowers/plans/2026-09-18-title-residuals-report.md`,
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

**Declared gaps.** No storage I/O (the save/load path and the `0x80CE4` image);
the deferred module taps `0x1AE20`/`0x2EA78` (screen setup, storage write); the
credit countdown `0x2CA48`/`0x2CA7C` and its caller `0x11F28` (input, 4b); and
`DS_00105C05`'s init writer `0x2BF00` (`= 0x1D`) plus attract writer `0x2C06C`
(`= 1`, the captured value). Full record:
`../../docs/superpowers/plans/2026-09-19-eeprom-config-report.md`.

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
