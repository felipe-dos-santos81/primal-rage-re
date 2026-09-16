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
  0x1C740   PRESENT: VBlank wait `in(0x3DA) & 8`, then 0x52106 / 0x50161 / 0x50D23
```

* **Process tables** — two 32-entry `code *` tables `0x80` bytes apart, each
  gated by a `u32` bitmask: `PTR_FUN_000A8644`/`_DAT_00104AE8` (update, walked
  by `0x24C5C`) and `PTR_FUN_000A86C4`/`_DAT_00104AEC` (render, walked by
  `0x255CC`). This is the engine's extension seam.
* **Tick** — `DAT_00105D88` is incremented by the 9-byte handler `0x2D62C`
  (`DAT_00105D88++`). `main` locks that code page and the `DAT_00105D88` data
  page, so it is an interrupt handler; the interrupt vector is not yet
  confirmed.
* **Pacing** — counter pair `DAT_00101508` (advanced asynchronously) and
  `DAT_0010150C` (loop-local), compared in `0x255CC`. `0x1C740` additionally
  busy-polls VBlank around the present work.
* **Screen surface** — `0x51F45` (called once from `main`) sets
  `(0, 200, &DAT_001088F8, 0x140, -1, 1, 0)` → 320x200 offscreen buffer at
  `0x1088F8`; globals `DAT_000E87A0` / `DAT_000E87A4`.
* **Palette** — `0x1C470` flushes a 4x`u32`-record dirty-list at
  `DAT_00107498` (head `DAT_00107798`) to the VGA DAC, VBlank-gated. Records:
  `[0]` colour ptr or resource handle, `[1]` first DAC index, `[2]` count,
  `[3]` non-zero -> `[0]` is a resource handle resolved by `0x1B544` **+4**.
  Colours are 8-bit guns at R=bits[2..9], G=bits[10..17], B=bits[18..25].

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
| `0x1C740` | present / flip (VBlank-gated) |
| `0x1C470` | palette dirty-list flush → VGA DAC |
| `0x2D62C` | tick handler `DAT_00105D88++` (interrupt handler, vector TBD) |
| `0x51F45` | 320x200 screen-surface setup (`&DAT_001088F8`) |
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
5. Which interrupt carries the tick (`0x2D62C`), and which code the framebuffer
   write path uses (`0xA0000` is never referenced directly).
6. Why `main` gates on `int 10h` mode `0x13` (320x200, matching the `0x51F45`
   surface) while the installed set is `S16` (640x480).
