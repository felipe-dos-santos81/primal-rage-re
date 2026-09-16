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
  and Smacker video (`twi5.smk`, `twg.smk`).

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
  0x134C0
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

## Title state (Task 14)

* **Title/attract state is index 1.** `FUN_000121a0` — case 1 of
  `switch(DAT_000F0A64)` — is the animated title screen: on entry it spawns a
  scrolling/zooming logo animation through two `FUN_0002ae14` tasks, holds it
  for `DAT_000F0A66 = 0x600` ticks, then transitions to state 2
  (`DAT_000F0A64 = 2`). The attract sub-machine `FUN_00011000` case 0xb assigns
  state 1 (`DAT_000F0A64 = 1`) when its cycle counter `DAT_000F0A5C == 0`.
* **Runtime confirmation unavailable.** Task 14 could not read `DAT_000F0A64`
  live: this macOS DOSBox-X build refuses the internal debugger ("Debugger in
  Mac OS X not available unless you start DOSBox-X from Terminal"), so no
  breakpoint or memory dump could be scripted. The index above is static
  evidence, not a runtime reading. Confidence: **likely**.
* Port choice: the port enters state 1 directly (the 0x11000 attract
  sub-machine is deferred) and renders the full-screen `S16TITLE.GRA` frames
  10/12/13/18; the logo/menu sprite composite is deferred to the menus
  sub-project.

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
   **Still open:** which interrupt vector carries the tick `0x2D62C` (rate is
   60 Hz, inferred; no install site names the handler — see "Tick"). This is
   the one item Task 13 could not settle.
6. Why `main` gates on `int 10h` mode `0x13` (320x200, matching the `0x51F45`
   surface) while the installed set is `S16` (640x480).
