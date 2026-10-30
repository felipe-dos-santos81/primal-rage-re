# Reverse-engineering guide — Primal Rage (DOS)

Conventions, the DOS/4GW memory model, and the toolchain. Read together with
`../README.md` and `../docs/FORMATS.md`.

## Target

`PRAGE.EXE` is a **32-bit protected-mode** program:

* Compiler/runtime: **WATCOM C/C++32** (the `WATCOM C/C++32 Run-Time system`
  copyright string is in the code object).
* Container: Microsoft **DOS/4GW *bound* Linear Executable (LE)** — an MZ stub
  followed by an embedded LE image (`file(1)`: *LE for MS-DOS, DOS4GW DOS
  extender (embedded)*).
* No real-mode game code. The MZ stub only loads DOS/4GW, which maps the two
  LE objects and jumps to the protected-mode entry.

### Verified LE facts (`tools/le_info.py data/game/C/PRAGE.EXE`)

| Item | Value |
|---|---|
| LE header file offset | `0x290A4` |
| CPU / OS | 2 (i286+) / 1 (DOS) |
| Pages | 213 × `0x1000` |
| Entry (`eip`, obj 1) | `0x5245C` |
| Stack (`esp`, obj 2) | `0x8B0D0` |
| Object 0 (code) | base `0x10000`, virtual size `0x63B15`, 100 pages, flags `read|exec|preload|32-bit` |
| Object 1 (data) | base `0x80000`, virtual size `0x8B0D0`, 113 pages, flags `read|write|preload|32-bit` |
| Data-pages offset | `0x3C800` (relative to the embedded MZ image base at file `0x26654`, not to the LE header; page data begins at `0x62E54`) |

## Address conventions

* **Ghidra address = linear address = LE object base + object offset.** There
  is no segment:offset split; WATCOM uses flat 32-bit addressing.
* A function Ghidra calls `FUN_0001c500` is at **code-object offset `0x1C500`**
  and runs at linear `0x1C500`. Code object spans `0x10000`–`0x73B14`.
* A global `DAT_0008xxxx` is at **DS offset `0x0xxxx`** in the data object:
  subtract `0x80000`. E.g. `DAT_00080004` = `DS:0004` (start of the string
  table). Data object spans `0x80000`–`0x10B0CF`.
* The `.image` block is the unmodified file mapped as an overlay; its
  `FUN_.image__*` symbols are analysis noise — **ignore them** (1207 of 1352
  functions are real code).

### Entry / startup

* LE entry `0x6245C` is `jmp 0x624D4` (2 bytes, immediately followed by the
  `WATCOM C/C++32 Run-Time system` copyright string in rodata).
* `0x624D4` is the WATCOM/DOS4GW startup (`_start`): it saves the register
  state, uses `int 21h`/`int 31h` (DOS/DPMI reflection) to read the PSP
  command tail and environment, builds `argc/argv`, then calls the C `main`.
* Interrupts seen in the decompilation: `int 21h` (DOS/DPMI), `int 31h` (DPMI),
  `int 10h` (BIOS video), `int 33h` (mouse), `int 16h` (keyboard), plus
  direct port I/O for the sound drivers.

## What the game reads at runtime

* `INDEX` — resource index (`name → size|flags`); see `../docs/FORMATS.md`.
* `S16*.GRA` — graphics sets, loaded by name from the current directory.
* `\RAGE.S16` — CD/relative path string (data object).
* `english.txt`, `french.txt`, `german.txt`, `italian.txt`, `spanish.txt`,
  `portugal.txt` — localisation text.
* `DIG.INI`, `MDI.INI`, `RM.DRV`, `SB16.DIG` / `SBPRO.DIG` / `SBLASTER.DIG`,
  `FAT.OPL`, `FAT.AD` — Miles/AIL sound-driver set. `FUN_00010034` inits the
  driver and falls back through `SB16.DIG → SBPRO.DIG → SBLASTER.DIG`.
* `twi5.smk`, `twg.smk` — Smacker video (logos/intro), now decoded by the port
  (sub-project 2b-i; SMK2, 320×200, silent). See `../docs/FORMATS.md` and
  `../../docs/superpowers/plans/2026-09-17-smacker-video-report.md`.

## Toolchain

* Ghidra 12.2 + [`ghidra-lx-loader`](https://github.com/yetmorecode/ghidra-lx-loader).
  The loader maps the objects at their LE rel bases and applies the LE fixups,
  so the decompilation already has correct absolute data references.
* `dosbox-x` for ground truth. Because DOS4GW relocates the image, the fastest
  way to learn a runtime address is to run the game with a `memory file`
  (`[dosbox] memory file = /tmp/prage.mem`) and search the dump, or to set a
  breakpoint in the debugger. The file↔memory mapping has a constant delta per
  LE object (measured in this session: code object `file + 0x19E1AC`,
  data object `file + 0x19F1AC`, with the image loaded around `0x200000`).

### Useful commands

```bash
G=/Users/felipe.dos.santos/ghidra_12.2_DEV/support/analyzeHeadless
export JAVA_HOME=/Library/Java/JavaVirtualMachines/temurin-25.jdk/Contents/Home

# import + full analysis
$G _tools/ghidra_proj prage -import data/game/C/PRAGE.EXE -overwrite -scriptPath _tools/ghidra_scripts

# fix perms/entry and re-export
$G _tools/ghidra_proj prage -process PRAGE.EXE -scriptPath _tools/ghidra_scripts \
   -postScript FixupProgram.java \
   -postScript ExportDecomp.java port/decomp/prage.c 90 \
   -postScript ExportMeta.java port/decomp/

python3 tools/le_info.py data/game/C/PRAGE.EXE
python3 tools/le_info.py --index data/game/C/INDEX
```

## Landmarks found so far

| Address | Meaning |
|---|---|
| `0x6245C` | LE entry → `jmp 0x624D4` |
| `0x624D4` | WATCOM/DOS4GW `_start` (PSP/env parsing, builds `argc/argv`) |
| `0x6C435` | CRT `main` wrapper → `0x1BEC4` |
| `0x1BEC4` | **game main** (init chain + dispatch) |
| `0x20C10` | startup orchestrator → master loop |
| `0x1BE30` | teardown |
| `0x10034` | sound-driver init (`SB16.DIG`/`SBPRO.DIG`/`SBLASTER.DIG`) |
| `0x1B120` | `INDEX` loader / resource-table builder |
| `0x1B544` | resource-handle → pointer (30 callers) |
| `0x1C0F0` | extended-memory probe + memory-block list (EMS; unused by the flat port) |
| `0x2C3FC` | most-called function (206 callers) — core engine helper |
| `0x255CC` | master frame loop |
| `0x24C5C` | per-frame update (frame counter, player records, update process table) |
| `0x11D04` | state machine `switch(DAT_000F0A64)` |
| `0x11000` | per-state render/play dispatch (13 cases) |
| `0x1C740` | VBlank-gated animation/FLIC-style blit of `DAT_000E87A4` (**not** the screen write) |
| `0x1C470` | palette dirty-list flush → VGA DAC |
| `0x2D62C` | run-clock **counter** `DAT_00105D88++` (9 bytes), called by the 60 Hz AIL timer callback `0x1BDF4` at `0x1BE28` — **not** the ISR; the INT 8 vector is set at `0x66519` and points at `0x6627E` (§E1.6) |
| `0x51F45` | 320x200 double-buffer + scanline-table setup (`&DAT_001088F8`) |
| `0x50188` | swap back/fore buffers `DAT_000E87A0` ↔ `DAT_000E87A4` |
| `0x501A3` | dirty-dword blit `DAT_000E87A4` → literal aperture `0xA0000` |
| `0xA0000` | screen-write target, a **literal immediate** (no LE fixup) — literal-target and not-a-data-object are verified; the "VGA mode-13h aperture" reading is inferred (see `spec/game_flow.md`, verified-with-caveat). Hardware, never `mem[0xA0000]` |
| `PTR_FUN_000A8644` / `_DAT_00104AE8` | update process table / bitmask |
| `PTR_FUN_000A86C4` / `_DAT_00104AEC` | render process table / bitmask |
| `0x1C500` | `game_string_get` — the string decode's tail (`0x474E4`/`0x1C500`), ported; called 181× |
| `0x2BC30`, `0x2AE14`, `0x2F198` | very hot code (150–190 callers) |
| `0x5D7DC` | **not audio**: the Watcom C runtime `rand()` — a 32-bit LCG `seed = seed*0xB90D12B9 + 0x38CE051F`. 33 game callers, 0 AIL callers (spec `audio.md` "AIL surface"). Earlier "allocator/memory helper" was wrong |
| `0x121A0` | title state (switch case 1): phases on `DS_000F0A6F`, spawns the logo/second objects, counts down `DS_000F0A66` |
| `0x2AE14` | actor spawn; 5 args, EAX=descriptor, EDX/ECX/EBX, stack word (pinned in `docs/superpowers/plans/2026-09-17-actor-system-args.md`) |
| `DS_001014F4` / `DS_001014EC` | actor-record pool (`0xEBA0`, 580×`0x68`) / pset pool (`0x4880`, 580×`0x20`), both allocated by `res_load_index` |
| `0x2B2A0` | animation-stream opcode dispatcher, 47 entries at linear `0x2B1E4` |
| `0x5D87E`, `0x5D973`, `0x5DB9E` | sound driver API used by `0x10034`; `0x5D973` is AIL's driver dispatcher (`swi 0x31`), not game-called |
| `0x1CF40` | AIL init (sub-project 2a): `AIL_startup`, prefs, 4 sample handles, sequence handle, 60 Hz timer |
| `0x1CF20` | master-loop audio service: play queued samples (`0x1CB18`), start pending song (`0x1C930`), advance/sequence + render |
| `0x1C930` | load and start the pending song (`AIL_init_sequence`/`set_volume`/`start_sequence`) |
| `0x5D7DC`–`0x5DFFF` | sound-library block (36 game→audio callees: 33 AIL thunks `0x5D851`–`0x5DFEB` + 3 non-AIL below; the game→audio boundary) |
| `0x5DE48` | AIL XMIDI loader (`FORM`/`CAT`/`XMID` container parse) |
| `0x6FB28` | largest function (4979 bytes) |
| `0x14268` | sprite node build (id → display node, `sprite_node_build`) |
| `0x14328` | composite driver `render_list` (project + clip the display list) |
| `0x51E5C` | span blitter `sprite_blit`; dispatches via `PTR_LAB_00080C8C` |
| `0x5D218`/`0x5D28F` | RLE / clipped RLE span renderers |
| `0x57F80`/`0x57FFB` | mirrored RLE / mirrored clipped RLE renderers |
| `0x58CBD`/`0x5215C` | raw + clip / mode-1 shear copy renderers |
| `0x1C350`/`0x1C390`/`0x1C3A0`/`0x1C3D0`/`0x1C458`/`0x1C3FC` | display-list pool: reset, alloc, sorted insert/splice, remove/unlink, find-by-pset, sort |
| `DS_000A8B30` | static 18,443-entry sprite-handle table (`id & 0x7FFF`; `id & 0x8000` = hflip) |
| `DS_00105B44` | display-list head; nodes `{next, &pset}`, pool 580 at `DS_0010153C` |
| `DS_001088F8` | 200-dword row-offset table (`y * 0x140`), built by `0x51F45` |
| `DS_00107900` | mode-1 shear table (signed 16-bit ramp, produced by `0x38A38`) |
| `DS_00081310`/`DS_00081314` | bank table (`(n-1)` replicated) / fill-colour table (`n` replicated) |

### Sprite compositor (sub-project 4a-i)

On-screen sprites go through a **three-stage pipeline**:

1. **Actor update + pset sync** — `0x2A31C` walks the actor list and `0x2A820`
   syncs each 0x68-byte actor record into a 0x20-byte **pset** (`DAT_001014EC`),
   computing screen position and layer. **Ported in sub-project 4a-ii**
   (`port/src/game/actors.c`, `actors_update`).
2. **Ordering** — `0x1C3FC` insertion-sorts the singly-linked display list at
   `DS_00105B44` ascending by the pset layer, stable. Ported in
   `port/src/platform/render.c` (`render_list_sort`, `render_list_insert`).
3. **Composite** — `0x14328` walks the list, builds a 0x40-byte display node per
   entry via `0x14268`, projects and clips it, and calls the span blitter
   `0x51E5C`, which dispatches to a renderer that writes
   `mem + DS_000E87A4`. Ported in `render.c`/`sprite.c`.

The compositor is self-contained: it consumes psets and a display list and needs
no actor, no animation interpreter and no emulator. Its oracle is a three-way RLE
agreement against sub-project 1's already-verified decoder; the clipped,
mirrored and shear renderers are proven by hand-computed tests, not by a pixel
oracle. Full record: `../docs/superpowers/plans/2026-09-17-sprite-compositor-report.md`.

The code is dense from roughly `0x10000`–`0x39000` (engine/utilities) and
`0x3A000`–`0x6A000` (game logic), with libraries at the high end
(`0x5C000`+ looks like the WATCOM runtime / DOS4GW glue). The subsystem split is
in `port/spec/`; `game_flow.md` covers the loop, state machine and frame path.

### Actor system and title (sub-project 4a-ii)

The actor system is ported (`port/src/game/actors.{c,h}`): the `0x68`-byte pool
and its free/active lists, spawn `0x2AE14`, the pset sync `0x2A31C` →
`0x2A1FC`/`0x2A820`, the motion step `0x2A4FC` (16.16 fixed point), the
animation-stream readers `0x2A408`/`0x29F34` and writer `0x29DB8`, the 47-opcode
dispatcher `0x2B2A0`, the text grid `0x2F0F0`/`0x2F198`/`0x2F280`/`0x2F4BC` and
the glyph renderer `0x2F830`/`0x2F5A0`. The `0x5D7DC` LCG is
`port/src/game/rng.c`, seeded `0xABCD` in `game_init` (`0x20C62`).
`game/flow.c` transcribes the real title `0x121A0` (phases on `DS_000F0A6F`) and
the `0x1C500`/`0x474E4` caption chain; `0x47370`'s paged-memory loader is
replaced by a direct `ENGLISH.TXT` read.

The title is proven **pixel-exact** against the original by
`tools/title_compare.py`: `make title-pin` builds a capture-only `PRAGE.EXE`
copy with the four behaviour sites patched in place (the three entry RNG draws
plus the anim opcode-8 draw), `tools/title_capture.py` captures the
**overlay-live** original in DOSBox-X (four behaviour pins retained; one file per
distinct game frame — capture index ≠
game-frame index at 60 Hz logic / 70.09 Hz mode 13h), and `make title-oracle`
aligns the port dump into **two independent captures** and explains every
captured frame as a byte-offset splice of two adjacent port frames (a tear
model: the original updates the aperture at `0x255CC` with no retrace wait),
with zero pixel tolerance and zero unexplained frames. The determinism proof is
that both captures agree on the clean samples; with `PR_ORACLE_REQUIRED=1` and
fewer than two captures the oracle fails rather than reporting incomplete. Full
record: `../docs/superpowers/plans/2026-09-17-actor-system-report.md`.

### Title-path residuals (sub-project 4a-iii)

4a-ii's oracle proved a **pinned** original: `tools/title_pin.py` also ret'd
`0x2BF08`, hiding the title overlay. 4a-iii removed that site, re-captured the
true original, and ported the divergence. Two pieces:

* **`0x2BF08`**, the `0x11D04` tail's per-frame message tick, ported in
  `game/flow.c` (`game_overlay_step`): a centred `sprintf("%s:%d",
  game_string_get(0x46), DS_00105C00)` (`CREDITS:5`) from the
  `DS_00105D60 == 0` / `DS_00105C00 != 0` branch. Proven by the oracle (green on
  the overlay-live captures, red when stubbed). Seeded inputs `DS_00105C00 = 5`,
  `DS_00105C05 = 1`; the credit countdown is ported (4b-B, `game_state_step`'s
  coin poll) and the only 4b gap left is its coin capture (none was taken to pin
  the live countdown). Diagnosis:
  `../docs/superpowers/plans/2026-09-18-bf08-overlay-diagnosis.md`.
* **The `0x13xxx` effect list**, ported in the new `port/src/game/effects.{c,h}`:
  list primitives `0x249B0`/`0x249C0`/`0x249D0`, free-list build `0x13ADC`,
  spawn `0x13C70`, per-entry teardown `0x13420`, clear `0x13DF0` and step/age
  `0x134C0`. Wired in `actors_reset` (`0x2BBB8`/`0x2BB2B`) and the title's
  `0x123EA` (`effects_spawn(node, 3u, 0x419786C)`). The effect render path —
  `0x134C0`'s per-frame step/age (`effects_step`, animating each record's
  `+0x10` block through `0x33734`'s palette append) — is ported and
  unit-proven, as is `0x13DF0`'s drain. Full record:
  `../docs/superpowers/plans/2026-09-18-title-residuals-report.md`.

## Next steps

1. Subsystem split by call graph. ~~Identify the main loop~~ — done:
   `0x1BEC4` → `0x20C10` → `0x255CC` (loop) → `0x24C5C` → `0x11D04` (state
   machine) → `0x11000`; the frame write/present path is the literal `0xA0000`
   copy (`0x255CC` full copy / `0x501A3` dirty blit) from `DAT_000E87A4`. See
   `spec/game_flow.md`.
2. ~~Decode `S16*.GRA` fully~~ — done: chunk types 2/5/6 decoded (Task 8),
   implemented in `port/src/platform/gra.c`. The port matches
   `tools/gra_render.py` by **exact consumption** (18,201/18,202 descriptors
   across all 69 files) and byte-for-byte only for the four full-screen
   `S16TITLE` frames `{10,12,13,18}`; the per-sprite sub-palette/DAC base and
   the descriptor `x`/`y` anchor stay `likely`, not proven. See `../docs/FORMATS.md`.
3. ~~Name the hot core functions~~ (`0x2C3FC`, `0x2BC30`, `0x1C500`, `0x2AE14`) — done:
   all four have `FN_` symbols in `symbols.h`, and the port calls them (`actors.c`, `menu.c`).
4. ~~Pin the tick **interrupt vector**~~ — done: `0x66519..0x6652D` sets INT 8
   (`mov eax,8`; `mov edx,0x0005627E`; `mov bx,cs`; `mov ah,0x25`; `push ds`;
   `mov ds,bx`; `int 0x21`), so the vector's target is `0x6627E` (`0x5627E` +
   the object-0 base `0x10000`; the loaded image holds the immediate
   `0x0006627E`). The AIL PIT writer `0x663A4` sends mode `0x36` to port `0x43`
   (`0x663AC`) and the divisor's low and high bytes to port `0x40`; `0x663D4`
   computes `period*10000/0x20BC` (8,380), and the measured 16,666 µs period
   (`0x66BD0`, `1,000,000/60`) gives the 60 Hz timer's divisor 19,887 (59.998 Hz
   with the standard 1,193,182 Hz PIT clock; `0x66407` programs the PIT from
   AIL's minimum period over its 16 timer slots, so this is that timer's rate,
   not one global PIT rate). `0x2D62C` is **not** the ISR: it is the
   9-byte run-clock counter the AIL callback `0x1BDF4` calls at `0x1BE28`.
   Record `../docs/superpowers/plans/2026-10-23-endgame-truth-derivations.md`
   §E1.6/§E10.
5. The SDL3 port lives in `port/`: engine core (sub-project 1), audio/AIL
   (sub-project 2a, report at
   `../docs/superpowers/plans/2026-09-16-audio-ail-port-report.md`), Smacker
   video (sub-project 2b-i, report at
   `../docs/superpowers/plans/2026-09-17-smacker-video-report.md`; spec at
   `../docs/superpowers/specs/2026-09-17-smacker-video-design.md`), the
   sprite compositor (sub-project 4a-i, report at
   `../docs/superpowers/plans/2026-09-17-sprite-compositor-report.md`) and the
   actor system + title oracle (sub-project 4a-ii, report at
   `../docs/superpowers/plans/2026-09-17-actor-system-report.md`) and the
   title-path residuals (sub-project 4a-iii, report at
   `../docs/superpowers/plans/2026-09-18-title-residuals-report.md`). Streamed
   Smacker audio (2b-ii), menus/EEPROM (4), the fight engine (5), the
   deferred attract subsystem (4d: `0x11000`, `0x38A38`, `0x389C4`, `0x292AC`,
   `0x4F644`) were the open list; the
   open work is the "Remaining work" list below. The AIL sound-id
   table `DAT_000BBDC8` is **static in the EXE** (244 records, ids `0x00..0xF3`;
   stride 12; byte 0 = case, dword +4 = handle); its id → resource mapping was
   extracted (record `2026-09-29-k7-k12-derivations.md` §0.1): case counts
   0 ×5, 1 ×29, 2 ×154, 3 ×3 (`0x46 0x4D 0x5D`), 4 ×1, 5 ×18, 6 ×34, and the
   only case-1 records naming S16TITLE.GRA are ids `0x54`/`0x56`, both handle
   `0x03836102`.

### Remaining work (2026-10-24; counter re-measured at the endgame truth phase gate)

- The captures `gp-u12-endurance`, `gp-u13-ending2`, `gp-u15-join` and
  `gp-u16-twop2` are captured and pinned; their `gp-u1x-oracle` targets skip
  only on a checkout without the git-ignored captures. `gp-u17-device2` (host
  input truth) pins the no-pad clear and the device-2 selection's screen proof,
  with empty frame/trace ratchets (the wall-paced walk has no replay, record
  §H6.9). **All seven character
  endings are now captured**: SAURON's by U10 (poked), BLIZZARD's by U13, and
  TALON's, VERTIGO's, CHAOS's, ARMADON's and DIABLO's by the endgame truth
  phase (`gp-u18-oracle`…`gp-u22-oracle`, each in `make verify` and each
  skipping only without its capture; record
  `2026-10-23-endgame-truth-derivations.md` §E3–§E7). The U16 finisher reaction
  is `gp-u23-oracle` (§E2), captured under the **labelled poked-damage
  fallback** (P2's `0x10789E` poked to `0x77`, then a real hit to the KO
  `0x78`): `gp_twop.py check --finisher` runs without
  `--allow-missing-reaction` and the capture reaches `r0=0x33`; its moves
  ratchet records a measured port divergence, the replay leaving `r0=0x27`
  where the capture advances to `0x33`. The poked captures claim the ending
  content and the finisher reaction, not the KO by hits or the six earlier won
  matches. **U14's fought live win (R1, §J6) is a named gap**: the
  fought-match-truth phase's one authorized attempt (2026-10-26) lost (P1 KO'd
  in both rounds; `b0c` never 1; mode 0x12 never opened) and failed its harness
  checks, so its artifact is preserved as `gp-u14-win-live.failed`, the published
  target is absent, all nine `GP_U14_*` pins (the eight plus `MAX_MISSING`) are
  empty, `GATE_NAMED_GAPS` keeps it declared, and `gp-u14-oracle` keeps its
  absent-capture skip. (Endgame Task 8 had declined a further attempt after
  gameplay truth II's two; this phase ran one under its own go-ahead.) U13's
  pixel claim now reaches the capture's last frame (5903): frames 4248, 4320 and
  5120 are explained by named, byte-pinned and mutation-tested per-frame
  recipes, the
  tail 5891..5903 is byte-identical to the replay's own frames (the script now
  ends at the last `S`, f=10068, via `--end-to-last-s`), and the enforced reach
  is port 4951 — `GP_U13_MIN_FIRST` = 5904, `GP_U13_MIN_REACH` = 4951,
  trace/WIN = 10069 (R3, §J5; record `2026-10-23-endgame-truth-derivations.md`
  §E9). The game-set `b0c` is U10's (`GP_ENDING_WIN_MIN_FIRST=9954`), not a
  gap; the fought no-poke win is terminal R1 (final truth Task 3, record
  `2026-10-30-final-truth-derivations.md` §F3): `gp_win.live_match` has no
  `b0c` clause (the setter is the final's `0x0C→0x0D` chain, not match 1),
  `gp_win.live_final` keeps `b0c=1` at/after mode `0x0F`, and the one
  authorized two-human `gp-u14-twop-win` run reached the game's 2-0 win but
  failed its port-script check (`f=0xB1B`, a missed `S(0xB1A)`) and stays
  `.failed` with no oracle or pin.
   The two-human KO itself is now published by `gp-twop-ko-oracle` (live truth
   Task 3, record §L3): the no-`W`, no-poke capture reaches side 0's hand-dealt
   KO at `f=0x0B63`, with P2's `s1_5a` moving from live `0x73` to `0x78`, and
   the replay covers the KO and round-result window through the trace/X record
   `f=0x0C50` (frame pin 2525, trace/moves 3153). The oracle asserts the KO and
   round result; it does not claim a fought match win or that the result naming
   is a fought win. U14's fought live win is the declared terminal gap (final
   truth Task 3, record §F3; the rule split and the failed two-human capture
   are below). The current
   live-truth inventory is **42 explicit capture paths, 41/42 present**, with
   only that U14 path absent. The failed U14 artifact is separately game
   evidence for a fought arcade KO, not a capture, oracle or pin (live truth
   Task 8, record §L8). Task 12a is docs-only; Task 12b re-pinned the census;
   Task 13 runs the full gate.
- The differential audit (proof-completion phase, record
  `2026-10-24-proof-completion-derivations.md` §P5–§P8; the state-coverage
  proof-debt declaration §D7): `make diff-verify`
  reads `1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named
  gaps; 825/845 rows with callees closed (188 have none); 809/845 closed on
  full rows; 8 ported functions with no row`. The **20 open rows** are declared
  terminals (15 AIL thunks, `sound_buffers_alloc`, `input_sampler`, and the
  three runtime-flush rows `config_file_load`/`config_file_save`/
  `menu_fatal_error`), **13 rows are `WEAKER`**, and **8 portable functions are
  `UNROWED`** (45 `FN_` ported-unrowed: 8 portable + 37 runtime); the three
  lists are the harness's `TERMINAL_CENSUS`/`WEAKER_CENSUS`/`UNROWED_CENSUS`
  tables (`tools/tests/test_diff_verify.py`). The span census is 417 slots / 392
  distinct / 385 writer slots (384 reachable + dead `0x57E3C`) plus the
  out-of-table `0x5D110` (`0x81110[128] == 0x81310[0]`, the 386th target,
  reachable), and the E2 triage has 388 `span-writer` rows plus the supplement
  `0x5D004` (`0x81110[127]`). The
  counter and its clauses are in `AGENTS.md`, the phase record in
  `docs/PROGRESS.md`; `make diff-verify` prints it.
- Host and product work: audio audibility (no device-recording oracle), CI and
  packaging, SDL pad hot-plug, and the save-path documentation.
- The tick install is pinned (item 4: INT 8 → `0x6627E`, PIT divisor 19,887 at
  `0x663D4`; `0x2D62C` is the counter, not the ISR). AIL's own dispatch from
  `0x6627E` to the registered callback `0x1BDF4` is not traced byte by byte
  (record `2026-10-23-endgame-truth-derivations.md` §E1.6).
- The out-of-scope content modes `0x21` (the volleyball easter egg, entered at
  `0x4BD98` after the eighth worshipper trample `+0x1F > 7`) and `0x25` (the
  bowling easter egg, entered at `0x4E11C`; the game's decoder returns string
  `0x61` = "BOWLING!") stay declared (state-coverage record
  `../docs/superpowers/plans/2026-10-28-state-coverage-derivations.md` §D2).
  Live truth Task 6 made two real-input attempts with no pokes: the one-attempt
  route reached `DS_00108892 = 2`, and the review-corrected three-attempt route's
  last sample was 5 (+1/+2/+2); the trace cannot distinguish stopping at 5 from
  reaching 6 and taking the same-iteration rejection decrement. Mode `0x25` never
  opened, and the precise remaining blocker is unresolved. The reaction-`0x22`
  callback installs the increment hook and sets slot `+0x53=7` (`0x455C3`), while
  the caller requires slot 0's `+0x53==0` (`0x263DE`) and states 9/10 are no-ops;
  however, the `+0x53` dispatcher's case 7 can call `fighter_state_367dc` for
  character 4, reaction `0x21`/`0x22` and a passing `+0x86` threshold
  (`fighter.c:3680-3693`), and that callee clears `+0x52`/`+0x53`
  (`fighter.c:2690-2691`). Whether that clear can pass on every relevant increment
  frame is not derived, so structural impossibility is not claimed. The artifacts are
  `gp-bowling.failed` and `gp-bowling-fix.failed`, with no replay, oracle or pin.
  No fourth execution of the current route is authorized and no more captures are
  to be run; a staggered route would be a new, unmeasured task (record
  `../docs/superpowers/plans/2026-10-29-live-truth-derivations.md` §L6).
  Both read `DS_00104ABC >= 2`, and
  `DS_00104ABC = (DS_00104B1F == 3) + 1` (`flow.c:1632`/`2416`), so a poked 1P
  scenario cannot open them. The continue screen `0x0E` is now published by
  `gp-u28-continue-oracle` (live truth Task 5, record §L5): mode `0x0C` opens it
  only when the incumbent `DS_0010810D` side is KO'd. The labelled-poke capture
  verifies `w10d=0`, unchanged `cred=4`, `b1f=0`, the complete 15→0 countdown,
  PRESS START at 13 (`cnt < 0x0E`) and the mode-`0x15`/`b25=0` expiry; it does
  not claim a fought KO or the taken-continue `0x2791C` path. The pre-L5 census
  rows (`0x4BD98`/`0x4E11C`/`0x27A2C`) are `NOT-HIT`; Task 12b re-measured them.
  The KO-branch `0x0B` drawn winner pose is exhibited and pinned by `gp-draw`
  (live truth Task 2, record §L2), a distinct claim from the timeout branch's
  `gp-draw-timeout`.
- The proof-debt declaration (state-coverage §D7) declares what stays open:
  **U17** (the `gp-u17-device2` wall-paced walk has no tick-keyed replay — its
  screens are already covered by the K11 captures, record §H6.9) and **U14**
  (the fought live win) are declared, neither run; U14's `live_death` rule
  anchors on the match-1 round-2 KO but `b0c`'s only setter (`0x37FF6` in
  `0x37EA0`) runs on the ending path, so the two re-scope options are (a) drop
  the `b0c` clause ("fought KOs, no pokes") or (b) move to the final — with the
  measured correction that `b0c`'s ending write is **not universal** (BLIZZARD
  U13 and ARMADON U21 keep `b0c = 0`). The claim-coverage design's undefined
  **B2–B8 labels** are replaced by the census's measured NOT-HIT set (98
  addresses, §D7.5, §D8.3 and live-truth §L12) and the declared modes; the census baseline is a separate
  pin (its own `tools/coverage_baseline.txt`, not `tools/pin_baseline.sha256`).
- The state-coverage phase is implemented and gated (spec
  `../docs/superpowers/specs/2026-10-28-state-coverage-design.md`, record
  `../docs/superpowers/plans/2026-10-28-state-coverage-derivations.md` §D1–§D8;
  two full gates plus `make audio-render`, green 2026-10-29). The census
  (`make coverage`, outside `verify`) now reads **HIT 657 / NOT-HIT 98 /
  UNINSTRUMENTED 4** (the 4 are `0x29D60`, `0x2EA30`, `0x2EA64`, `0x500BB`) —
  Task 14 re-pinned it from the Task 3 baseline's `HIT 645 / NOT-HIT 110` to
  `HIT 653 / NOT-HIT 102` (R17/§D8.3): the eight `improved:` addresses are
  `gp-u25-endurance-end`'s `0x292D4`/`0x29638`/`0x296B8` and `gp-draw-timeout`'s
  `0x19820`/`0x19958`/`0x27ED8`/`0x28BD4`/`0x28C38`. Live truth Task 12b then
  re-pinned it with `gp-u27-name-entry` improving `0x20860` and
  `gp-u28-continue` improving `0x27254`/`0x278B0`/`0x27A2C` (record §L12).
  The exhibits: ENDURANCE's team end (`gp-u25-endurance-end`, `af1` `0 → 4`,
  stop mode `0x17`, 1,360 frames), the side-1 tour (`gp-u26-side1-ending`,
  `ad4=1`/`b1f=2`/`c1=4`, **268 mode-`0x1F` frames byte-identical to
  `gp-u21`**), the KO-branch drawn match (`gp-draw`, `ad4=2`/`b1e=adc=3`/
  `w2=w3=1`, three labelled poked KOs; live truth Task 2, record §L2), the
  timeout draw (`gp-draw-timeout`, `clock=0` from the `W` record, `b16` not
  asserted), the continue screen (`gp-u28-continue`, 15→0, PRESS START at 13,
  mode-`0x15` expiry; live truth Task 5, record §L5) and the name-entry path
  (`gp-u27-name-entry`, sub-states `b25 00 → 04 → 05 → 06 → 07 → 0A`, three letters, "ABD" — now the
  gated `gp-u27-oracle` in `make verify`; live truth Task 7, record §L7). The
  U16 fix (§D3): the reaction's record-motion
  gap is a null table base (the stream reads the IVT at linear 0), fixed by
  `bios_ivt_stand_in()` seeding `mem[1] = 0xCA`/`mem[3] = 0xF0`; only
  `GP_U23_TRACE_MIN_FIRST`/`GP_U23_MOVES_MIN_FIRST` re-pinned (`5449`/`5467 →
  6050`). Live truth Task 4 measured U16 R2's deciding gate input as
  `y(0x1531)=0x0EAE < 0x1400` (a strict clear; record §L4), without deciding
  the wider finisher-gap question. The 5120 band is **INCONCLUSIVE** (§D4):
  the DOSBox-X memory file excludes the VGA aperture, so the diagnostic's 14
  bytes read `0xFF` and the
  band stays a declared gap. Live-truth Task 10 was not taken because the fade
  palette source was not funded; no DAC injection ran, no cause is fitted, and
  the bounds and every pin are unchanged (record §L10). The OPL pitch test
  (§D6) renders 439.9915 Hz (measured 439.0000 Hz) within the 1.7586 Hz
  derived tolerance. Live truth Task 11 pins **74 diff-allowed audio section
  prefixes / 29,656 leading matched ticks**; unmatched remainders and rendered
  audibility stay named (record §L11).
