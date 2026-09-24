# Primal Rage (DOS, 1995) — reverse engineering

Reverse engineering of the PC/DOS release of **Primal Rage** (Time Warner
Interactive / Probe Software / Teeny Weeny Games, 1995), targeting
`PRAGE.EXE` and the `S16*.GRA` graphics set.

It is a **32-bit protected-mode WATCOM C/C++32 program** shipped as a
**Microsoft DOS/4GW *bound* Linear Executable (LE)**. There is no 16-bit real
mode game code: everything interesting lives in two LE objects (code + data).

## Layout

| Path | What |
|---|---|
| `data/game/C/` | Installed game (`PRAGE.EXE`, `INDEX`, `S16*.GRA`, sound drivers) |
| `data/game/CD/RAGECD.ISO` | Original CD (`/Volumes/RAGECD` when mounted: `RAGE.S04`, `RAGE.S08`, `RAGE.S16`, `RAGE.SND`) |
| `port/` | **SDL3 port** (engine core, sub-project 1) + **audio/AIL** (sub-project 2a) + **Smacker video** (sub-project 2b-i) + **sprite compositor** (sub-project 4a-i) + **actor system and title** (sub-project 4a-ii) + **title-path residuals** (sub-project 4a-iii) + **EEPROM/config core** (sub-project 4b-A) + **front-end input/credits/select** (sub-project 4b-B) — `cmake -S port -B build` |
| `port/src/platform/audio/` | AIL surface, XMIDI sequencer, FAT.OPL, samples, mixer, vendored OPL core |
| `port/RE_GUIDE.md` | Address conventions, DOS/4GW layout, toolchain, landmarks |
| `port/spec/game_flow.md` | Entry, frame loop, state machine, tick, pixel path |
| `port/PORTING.md` | Porting rules (memory model, `mem[]` discipline, `fn_resolve`) |
| `port/DECOMPILATION.md` | Exact dependencies and steps used to produce `port/decomp/` |
| `port/decomp/prage.c` | Ghidra decompilation of every function (via the lx-loader) |
| `port/decomp/prage.functions.csv` | Function index: entry, size, callers, callees |
| `port/decomp/prage.strings.csv` | Defined strings with cross-references |
| `port/decomp/prage.symbols.csv` | User-defined / imported symbols |
| `FORMATS.md` | Decoded on-disk formats (LE layout, `INDEX`, `GRA`) |
| `docs/superpowers/` | Sub-project specs, plans and the engine-core report |
| `tools/le_info.py` | Dump the LE header/objects and decode `INDEX` |
| `tools/gra_render.py` | Independent GRA decoder + frame oracle (`--indices`, `--frame`) |
| `tools/gra_extract.py` | Extract every S16 sprite to RGBA PNG + `manifest.json` (`make re-extract`) |
| `tools/gen_symbols.py` | Generate `port/src/symbols.h` from the decompilation |
| `_tools/ghidra_scripts/` | Ghidra headless scripts used to produce the artefacts |
| `Makefile` | `make help` — PORT (`build`/`test`/`check`/`run`/`verify`) and RE targets |

## Toolchain

* **Ghidra 12.2** with the [`ghidra-lx-loader`](https://github.com/yetmorecode/ghidra-lx-loader)
  extension (LE/LX loader, DOS/4GW aware). The loader maps object 0 to
  `0x10000` (code) and object 1 to `0x80000` (data) and applies the LE fixups,
  so no manual image extraction is needed. It is installed at
  `~/ghidra_12.2_DEV/Extensions/Ghidra/ghidra-lx-loader` and
  `~/Library/ghidra/ghidra_12.2_DEV/Extensions/ghidra-lx-loader`.
* **JDK 25** (`temurin-25`) for Ghidra 12.2.
* **dosbox-x** for ground truth (memory layout, runtime behaviour).
* Python 3 with `capstone` for ad-hoc disassembly checks and `Pillow` for `tools/gra_extract.py`.

### Regenerate the decompilation

```bash
export JAVA_HOME=/Library/Java/JavaVirtualMachines/temurin-25.jdk/Contents/Home
G=/Users/felipe.dos.santos/ghidra_12.2_DEV/support/analyzeHeadless

# one-shot import + analysis (the loader auto-detects the LE)
$G _tools/ghidra_proj prage -import data/game/C/PRAGE.EXE -overwrite \
   -scriptPath _tools/ghidra_scripts

# make .object1 executable, create the entry function, export everything
$G _tools/ghidra_proj prage -process PRAGE.EXE \
   -scriptPath _tools/ghidra_scripts \
   -postScript FixupProgram.java \
   -postScript ExportDecomp.java port/decomp/prage.c 90 \
   -postScript ExportMeta.java port/decomp/
```

## Status

**Reverse engineering.** `PRAGE.EXE` loads and analyses cleanly: **~1350
functions** decompiled, 0 failures. LE layout and the `INDEX` container are
verified; the `S16*.GRA` chunk types 2/5/6 are decoded (`FORMATS.md`) — the
per-sprite sub-palette/DAC base and the descriptor `x`/`y` anchor remain
`likely`, not proven.
The entry/startup (`0x624D4`), the game main (`0x1BEC4`), the real frame loop
(`0x255CC` → `0x24C5C` → `0x11D04`), the tick (60.05 Hz measured) and the
literal-`0xA0000` framebuffer write path are pinned — see `port/spec/game_flow.md`.

**SDL3 port — sub-project 1, engine core: boot → title screen, running.** The
port reimplements the engine in C over a flat `mem[]` that holds the original
LE data image at its original addresses, reads the original `data/game/C/`
assets at runtime, and SDL3 appears only in `host.c`/`main.c`. Ported and
verified: the LE loader + fixups (byte-exact against Ghidra's image), the
`INDEX` resource manager, `fn_resolve` code-address mapping, GRA decode
(exact-consumption primary — 18,201/18,202 descriptors across all 69 files — and
byte-identical to `tools/gra_render.py` for the four full-screen `S16TITLE`
frames `{10,12,13,18}` only; the palette bank is flattened and the sub-palette
choice is `likely`), the palette flush (`0x1C470`), the process-table scheduler,
and the `0x255CC`/`0x24C5C`/`0x11D04` loop. Boot runs the original's state-0
attract first (sub-project 4d, below). The title state runs the real
`0x121A0` actor composite through the display list; `make title-oracle` drives
the continuous run through the attract to the pinned title window and explains
every captured frame as a byte-offset splice of two adjacent port frames (port
frames 1..95 exhibited by both captures, the start-of-window transition
disclosed, zero unexplained frames) — see the 4a-ii and 4a-iii paragraphs below.
The earlier full-screen four-frame `S16TITLE` stand-in was removed in
sub-project 4a-ii.
**Audio — sub-project 2a, AIL/Miles, running.** The port runs the game's own
audio path with no DOS driver: the `0x1CF40` AIL init completes, the title/
attract XMIDI bank is decoded and sequenced into OPL register writes through a
vendored FM core, one located announcer sample plays through the game's own
sample request/play call path, and mixed stereo frames reach SDL audio when a
device opens. The FM data path is proven byte-exact against an independent
Python decoder (9866 OPL register writes, zero differences), and the original's
OPL trace was captured and governs the comparison under a **symmetric oracle**
(both streams anchored at their first key-on). The port now also reproduces
`SBPRO2.MDI`'s MIDI-controller path — the channel handler (`0x3b54`), the
mask-driven family re-apply (`0x3184`), the driver's `0x35fa` frequency routine
(which retires the hand-fitted note table), and the total-level/pan/mod folds —
which closed both remaining named divergences: the mid-note frequency change (a
bend re-applies the frequency family) and the total-level term (the driver's TL
law, with the engine's sequence volume scaling CC7). The first difference
advanced from C write 302 at cycle start to **C write 430**. What remains there
is an **intra-tick write-order** difference — the same registers and values in
every tick, emitted in a different order; per-tick write multisets are identical
— plus carrier TL on MIDI channels 1 and 4, whose engine per-channel volume is
provably unpinnable (one sequence volume, one timer tick), and the `0xBD` rhythm
register. The 9866 (port) / 6903 (reduced capture) write counts are not a
like-for-like gap. Remaining differences are named or excluded with cause, never
tuned. **No audibility claim**: the oracle is the register write stream, not
rendered audio, and the render below is FM music only — the title announcer
sample is not in it. The windowed run is silent on hosts where SDL audio cannot
start — on this machine `-66681`. To listen anyway, `make audio-render` plays the
title bank through the sequencer + OPL core + mixer and writes a 16-bit stereo
WAV at the OPL rate (`AUDIO_WAV`, default `/tmp/pr_title_fm.wav`; `AUDIO_SECONDS`,
default 12) — the audio path is real, only the device is missing. See
`docs/superpowers/plans/2026-09-16-audio-ail-port-report.md` and
`docs/superpowers/plans/2026-09-18-opl-driver-report.md`.
**Video — sub-project 2b-i, Smacker logos, running.** The port decodes the two
boot movies (`twi5.smk`, `twg.smk`) with an in-repo, clean-room SMK2 decoder and
plays them on the boot path at the attract machine's phase 0 (`0x11000` case 0)
before the title screen. Both movies' presented
frames are proven **pixel-exact** against frames captured from the original in
DOSBox-X (RGB24, palette included): TWI5 120/120, TWG 41/41; the container
layout reconciles byte-exactly on both files. The decoder is allocation-free and
fixed-profile (rejects everything else by name); the player owns the
presentation rule, which is content-based and carries a `TODO(verify)` on the
original's `0x1C740` loop. See
`docs/superpowers/plans/2026-09-17-smacker-video-report.md`.
**Sprite compositor — sub-project 4a-i, ported.** The engine composites a
display list into the back buffer the way `0x14328` does: the display node and
builder (`0x14268`), the span blitter (`0x51E5C`), all six reachable renderers
(RLE, clipped, mirrored, raw, mode-1 shear), the 580-node list with sorted
insert and sort, and the projection/clip driver. The RLE renderer is proven
byte-identical to sub-project 1's already-verified decoder on 32 real sprites;
the clipped/mirrored/shear renderers are proven by hand-computed tests, not by
an emulator (the DOSBox title oracle is 4a-ii's). `game_loop` calls the
compositor in the original's order — a proven no-op until 4a-ii populates the
list. See `docs/superpowers/plans/2026-09-17-sprite-compositor-report.md`.

**Actor system and title — sub-project 4a-ii, ported.** The engine runs its own
actor pool and real title state: the `0x68`-byte records and their lists, spawn
`0x2AE14`, the pset sync, the motion step, the animation-stream interpreter
(`0x2A408`/`0x29F34`/`0x29DB8` and the 47-opcode dispatcher `0x2B2A0`), the text
grid and glyph renderer, the `0x5D7DC` LCG, and `0x121A0` with its `ENGLISH.TXT`
caption. The title composite is proven against two independent captures of the
overlay-live original (four behaviour pins retained) over its window
(`make title-oracle`):
the capture is modelled as a byte-offset splice of two adjacent port frames
because the original updates the aperture at `0x255CC` with no retrace wait
while DX-CAPTURE samples at 70.09 Hz, and every captured frame is explained with
zero pixel tolerance and zero unexplained frames. The oracle found and forced
the fix of three defects (the `0x33754` palette-table entry, the 6-bit VGA DAC,
the 16.16 actor velocity). One confirmed finding contradicts the earlier RE: the
in-window opcode-8 count is 0, so pin site 4 is inert and the `task-2-anchor-re.md`
account of the title's nondeterminism is wrong. See
`docs/superpowers/plans/2026-09-17-actor-system-report.md`.

**Title-path residuals — sub-project 4a-iii, ported.** 4a-ii's oracle proved a
**pinned** original: `tools/title_pin.py` also ret'd `0x2BF08`, hiding the title
overlay. 4a-iii removed that inert site, re-captured the true original, diagnosed
the resulting per-frame divergence (`CREDITS:5`, rows 7–12), and ported it:
`0x2BF08`'s four-branch message/text tick (`DS_00105C05` seeded then, now
produced by the attract's `0x2C06C(1)`; `DS_00105C00` is derived from the config
module) and the
`0x13xxx` effect-list slice —
spawn `0x13C70`, free-list build `0x13ADC`, clear `0x13DF0`, teardown `0x13420`
and step/age `0x134C0` — in the new `port/src/game/effects.{c,h}` module. The
oracle is now green against the overlay-live original (four behaviour pins
retained; 0 unexplained, port frames
95/96 exhibited) and turns red when `0x2BF08` is stubbed back out, so it proves
the overlay. `0x13C70`'s spawn is a **declared coverage gap** — it fills a
record, but the effect render path is unported, so the spawn alone draws nothing
and the oracle stays green either way; once `0x134C0` is ported the spawn is
jointly responsible for exhibiting frame 95 (removing it reverts frame 95 to
`missing`, which the oracle discloses rather than fails), so the spawn's own
gate is Task 5's unit tests.
The `0x134C0` step (ported when the wiring exposed a count-drain stall past the
window) is unit-proven to drain and is oracle-consistent. See
`docs/superpowers/plans/2026-09-18-title-residuals-report.md`.

**Effect producers — sub-project 4a-iv, ported.** The three missing `0x13xxx`
producers now sit beside `0x13C70`: `0x13D4C` (type 4, darken-to-zero,
`effects_spawn_darken`), `0x13E28` (type 6, darken toward a target,
`effects_spawn_pulse`) and `0x13B3C` (types 0/2, signed-offset scroll,
`effects_spawn_scroll`). The free-list head is taken only by these four, so
types 1 and 5 have no producer and are dead. An end-to-end unit test proves the
headline chain — spawn → `effects_step` → palette dirty list → `gfx_flush_palette`
→ `gfx_dac`. `0x13D4C` is called only from the unported effect call sites
(`0x29B74`/`0x41578`); `0x13E28` is called from the ported select state
`0x11F6C` (`flow.c:367`). **`0x13B3C` is dead, not merely unwired:** it has zero
callers and zero cross-references anywhere in the image (`ghidra_get_xrefs_to
0x13B3C` = 0; `prage.functions.csv` `FUN_00013b3c` `n_callers = 0`; the bytes
`3c b3 01 00` occur nowhere) — the earlier "live via the jump table at `0x23AC4`"
was a mis-transcription (that table's dwords are `0x23B3C`, `0x23B20`, …). The
`0x13xxx` effect **render** path is no longer a gap (no draw was missing); its
effect call sites are deferred as unreachable. See
`docs/superpowers/plans/2026-09-19-effects-producers-report.md`.

**EEPROM/config core — sub-project 4b-A, ported.** `port/src/game/config.{c,h}`
ports the packed field layer (`0x2D974`/`0x2DA0C`), the menu-default parser
(`0x2CCD0`), the defaults writer (`0x2CADC`) and the magic validate (`0x2D6F8`).
The descriptor table and config bytes are read from the loaded image; the storage
layer and save/load I/O stay declared no-ops, so validate takes the fresh-machine
defaults path. `game_init` now derives `DS_00104528` and the three `0x20C9F`–
`0x20CC2` globals from field `0x29`, and `0x2C304` derives the credit counter
`DS_00105C00 = 5` — the unseed the title oracle validates (0 unexplained). The
storage path remains a 4b gap; the credit countdown and the front-end
input/select path are ported in 4b-B. See
`docs/superpowers/plans/2026-09-19-eeprom-config-report.md`.

**Front-end input, credits and the select state — sub-project 4b-B, ported.**
`port/src/platform/input.c` samples the host key bitmap into the `0x500C4`
debounced level (`0x50161` selector), and `0x4F644` builds the newly-pressed and
held masks each frame before the state machine. `port/src/game/config.c`'s credit
layer (`0x2C060`/`0x2CA48`/`0x2CA7C`/`0x2C06C`/`0x2BF00`) is driven by `0x11F28`
(`frontend_coin_poll`), and the `0x11F6C` select state
(`0x33904`/`0x1C6D4`) is live: its exit advances `DS_000F0A64` to state 3 (ported
in the front-end-chain cycle below). See
`docs/superpowers/plans/2026-09-19-frontend-input-report.md`.

**Attract/boot subsystem — sub-project 4d, ported.** The port now boots the way
the original does — through state 0's attract — instead of playing the logos by
hand and jumping to the title. `port/src/game/attract.{c,h}` owns the 13-phase
`0x11000` machine (its phase 0 plays the two Smacker logos), the per-state
tails `0x10DB0`/`0x10E18`, the reset/scheduler `0x10EE4`/`0x10F28` and the
per-bit scene tick `0x292AC`; `render.c` owns the scroll/zoom projection
`0x389C4`/`0x38A38`; `flow.c` keeps only the state dispatch. `game_state_init`
enters state 0, and the deleted `DS_00105C05 = 1` stand-in is replaced by the
attract's own `0x2C06C(1)` (the captured title row 1). Proof is a continuous
headless run aligned to the existing post-logo captures: the attract prefix
(derived as capture frames `0..215`) plus the title window in one
`game_init()`, an env-gated `PR_ATTRACT_DUMP` driver with a run-to-run frame-hash
log, and raw-derived unit tests. `tools/attract_compare.py` matches capture
frames 0..214 and reports its first divergence at the last attract capture frame
215 (raw 2180/2175). The RAGE-logo actor's animation reaches opcode 0x11's inline
code pointer `0x10FA8`; the port now implements it (spawning the descriptor
`0x9AD08` hand-off actor at layer `0xE4`, exactly the raw's `0x2AE14` call) and
registers it in `actors_init`, so the logo's continuation runs and the matched
prefix grew from frame 100 to the whole window. The palette-animation starter
`0x4F83C` (and `0x10FC4`) and the scene-palette driver `0x4F7F4`, once declared
gaps here, are **ported** by the small-fidelity-gaps cycle (below) — as is the
`0x33874` reflow — and registered through `attract_scene_tick`. They do not run
at runtime (`DS_00104AD0` bit 0 is never set), so the frame-215 divergence is
unmoved; its owner is the loader-frame DAC state / read-gate model (record §3.5),
not these drivers. The title window stays 0
unexplained on both captures. (Before the `0x11000` spawn-slot fix `e29f849` the
boundary was frame 68, and before this hand-off it was frame 100.) The oracle also
surfaced and fixed a real `0x336C0` bug — `palette_list_init` omitted the raw's
palette ownership-table clear at `0x87618`, which the attract's earlier palette
acquires exposed. See
`docs/superpowers/plans/2026-09-19-attract-report.md`.

**Front-end chain — states 3/4/5, ported.** `port/src/game/flow.c` now ports the
states after the select carousel: state 3 (`0x12484`, the post-select presentation
and its `0x12658` handoff), state 4 (`0x11578`, the 8/12/13-line credit roll) and
state 5 (the inline `0x11D04` match-start block), so the port advances
carousel → 3 → 4 → 5 → 9 → 6 unaided. `port/src/game/effects.c` owns the `0x13xxx`
effect render path: `0x1324C` (screen-shake decay) is registered as update-table
entry 0 but **dormant** (no shipped store sets `DS_00104AE8` bit 0), and the
palette path spawn → `effects_step` → dirty list → `gfx_flush_palette` →
`gfx_dac` → `gfx_present` is proven end to end. **No camera/scene function
draws** — the plan's assumed missing draw does not exist; the camera state is
consumed by the existing render pass and actor-pset sync. The front-end pixel
oracle is **closed and enforced**: a 120 s pinned capture aligns the port's
state-3 zoom to window `[560..842]` (raw `3108..3749`), **283 frames: 125 clean,
154 splice, 0 transition, 2 unexplained (832, 833)** — the two are allowed by
name (the arena-backdrop cycle's absorbed claim move, below) and any other
unexplained frame fails. (The indices moved from `[557..813]` / 257 frames when
demo-fight cycle 1's pins forced a re-capture, to `[560..830]` / 271 when cycle
2's master-loop pin forced a second, and the window's end grew to 842 in the
arena-backdrop cycle, whose fix explains captures 834..842; the capture is
host-timed and not reproducible, so its distinct-frame indices shift while the
oracle's claim does not.) It proves exactly one thing: **no content-bearing
capture frame inside the window the port's own dump exhibits is unexplained**
(the two named exceptions aside) — the window is derived from that dump and the
coverage/`endpoints BAD` counts are ignored, so a port that **under-renders**
the front-end (only the state-3 entry frame, or only the first 12 frames) still
passes. Sixteen all-black capture frames are excluded as an explicit
oracle-level choice, **not** a proven fact (the investigation could not settle
whether capture 561's black frame is a distinct logic frame or a 70.09 Hz
scanout artifact). The effect call sites
`0x29B74`/`0x41578` are **deferred**: the raw reaches them only through
`0x24C5C`'s unported mode cases (`0x12`, `0x16..0x1B`) and the match/fight chain,
and the port's `DS_00104B00` is fixed at 3, so wiring them would be a dispatch
path nothing can reach. The match cycle's `0x1EA08` call sites and the unported
half of `0x1EA08` itself remain declared gaps. The camera chain
(`0x12CD4`/`0x1317C`/`0x13290`/`0x1333C`, their `0x12D48` dispatcher and the
`0x12DA8`/`0x12DF0`/`0x12E3C` modes), once unowned here, is **ported** in
`port/src/game/camera.c` and dispatched from `flow.c:1346` (`camera_dispatch()`).
See
`docs/superpowers/plans/2026-09-20-frontend-chain-derivations.md` and
`port/spec/game_flow.md`.

The attract demo fight (states 6/7 — the CPU-vs-CPU demo, **not** the interactive
match) is ported through **motion** (demo-fight cycle 1) and **closure**
(demo-fight cycle 2, the `demo-fight-closure` branch): state 3's `0x12658`
handoff enters the 240-frame state-9 hold, state 6 picks the two characters from
the shared RNG and spawns them, and state 7 runs the arena with the ported CPU-AI
command generator (`0x47208`), the `+0x52` state machine (`0x3531C`/`0x350D0`),
the `0x3C88C` hitbox machine and the `0x3CF38` hit chain — the chain now **fires**
and hits land (`+0x7C` 0/0 → 1/1), and the fight's last state change moved
**1262 → 1400**. Its report-only oracle (`make demo-oracle`) measures the demo
window `[843..3616]` (raw `3750..8409`), **2774 frames: 0 clean / 0 splice / 0
transition / 2768 unexplained** (6 all-black frames excluded). **The demo
window's first unexplained frame is capture 843 (raw 3750)** — the T-rex's
animation pose, not the state-9 hold. (Amendment 5 of cycle 1's plan said the fight
begins at 839/28; Task 1 corrected it to **836/25**, and the final re-capture
moved the loader text to 832 with the fight's first frames at 833/834 — record
§9.1/§9.3.) Cycle 2 advanced the boundary 811 → 816 → 832: the
state-9 globe's fourth layer (`0x12720`) made the hold match, the `rle_row`
mirror-window fix and the idle-animation tick (`0x37A58`) improved the arena, and
the chain drives the fight past its former stall. The arena-backdrop cycle
(cycle 5) then explained the fight's opening frames — captures 834..839
byte-exact at 0 B each — and pushed the front-end window's end to 842, so the
report-only demo window now opens on 843 (the loader frame 832 and the held
dark-arena frame 833 are the front-end window's two named unexplained, above).
**The cycle's Gate — 0
unexplained frames — is UNMET**, and it is recorded as such: capture 832's
read-stall is **proven un-derivable** (the port produces both held states but the
gate passes on the loader frame, and the post-read ISR tick count is a
host/emulator property with two live-RAM polls disagreeing, Δ=2 vs Δ=3), and the
residuals are the palette-order subsystem, the fighters' animation poses, and the
fight's stall tail (the unported `0x34B14` handlers). No pin or value was fitted
to force the Gate. The state-9 hold draws no RNG; the demo's two state-6
character picks are **not** pinned — cycle 2 replaced cycle 1's fitted picks with
a source-level pin on the master loop's host-timed draws (`0x256B1`/`0x256D6` →
non-advancing), so the oracle validates the picks instead of being forced to
them. The **interactive match** (the mode graph, the `0x257A4` coin divert,
`0x1EEB0`, `0x1F458`, the player screens and human input) remains **unowned**.
See `port/spec/game_flow.md`, the design spec
`docs/superpowers/specs/2026-09-21-demo-fight-closure-design.md`, and the
derivation record `docs/superpowers/plans/2026-09-21-demo-fight-closure-derivations.md`.

**Combat/render fidelity — cycle 3 (branch `combat-fidelity`), the demo fight's palette and combat tail.** Cycle 3 closed three of cycle 2's residuals. The T-rex's colour difference was the **front-end dump driver's `DS_0010816A` seed** (0xFF → 0), not the engine: the acquisition sequences are identical and `0x41350` is faithful, so the T-rex's handle is now the original's `0x1BB9FCD8` at DAC range `start=142 len=31`. Seven of the twelve `0x34B14` `+0x52` handlers landed (`0x35F84`/`0x36430`/`0x399CC`/`0x361C8`/`0x36300`/`0x36710`/`0x364FC`); the state-7 entry's four missing RNG draws landed (the type-0 effect handler `0x4AAD0`/`0x4B144`, so the entry LCG now reaches `0x10F7DB07`, matching); and the demo-AI block state landed (`0x18540`/`0x18350`, so `cmd0@1072 = 0x4848` and `cmd1@1071 = 0x0002`, matching). The arena's byte-diff against capture 834 fell **42 667 B (22.2 %) → 18 294 B (9.5 %)**, the T-rex region 6 363 → 1 773 px and the raptor region 8 704 → 4 421 px, and the raptor's silhouette IoU rose **0.522 → 1.000**. **The Gate is UNMET overall:** the fight reaches the state-7 900-frame timer exit (`s7_last == 1969`) but its state machine is frozen from loop 1072 — the original's pose state `0x10`/`0x0A` is reached only through the unported `0x19020` → `0x193B0` → `0x3B714` → `0x3AAFC` → pose-family chain (68 new funcs / 10 467 B; true new ≥ 11 012 B), so the pose subsystem is handed to **cycle 4**. The port's hit counter `+0x7C` is now 0 for the whole state-7 window: cycle 2's `+0x7C 0/0 → 1/1` was a by-product of the old diverged trajectory, and with the trajectory now matching the original's early path the hit requires the pose state the port cannot reach. Every enforced oracle claim is unmoved (`make verify` exit 0, 0 warnings). See `docs/superpowers/specs/2026-09-22-combat-fidelity-design.md` and the record `docs/superpowers/plans/2026-09-22-combat-fidelity-derivations.md`.

**Small fidelity gaps — cycle (branch `fidelity-gaps`), the four off-demo-path residuals.** This cycle closed all four of cycle 3's small named gaps and moved no enforced oracle claim. The five remaining `+0x52` handlers landed (`0x359E0`/`0x35C1C`/`0x35D20`/`0x37464`/`0x33B00`/`0x35E6C`), with `0x349C8`'s bit-6/7 deep callees (`0x385B0`/`0x39A10`) wired at their raw sites (`0x36884`/`0x37D57`) via the minimal caller chain (5 functions / ~1 741 B, ratified by the human). The loader flush scope's record premise was **refuted by the raw** — `0x1C470` is a whole-list drain and the port's flush was already faithful; the real gap, the missing initial record `{0xBD470, 0, 1, 0}`, is now enqueued. The attract/scene palette drivers (`0x4F7F4`/`0x4F83C`/`0x33874`) are ported and registered; `0x4F83C` ships test-only (its `0xE8916`-table callers are unported) as an **explicit accepted exception** (spec representation rule, Q10). The 169-tick state-9 hold is a **real divergence** (the state-9 screen's animation stops ~169 ticks early, holding capture 830's frame byte-static), oracle-neutral and carried forward with its owner. No single group triggered the size gate, but the **branch total crosses it**: 20 functions / 4 675 B (the four groups 10/2 223, 2/418, 0, 3/293 plus the human-ratified caller chain 5/1 741). Every enforced oracle claim is unmoved (`make verify` exit 0, 0 warnings). See `docs/superpowers/specs/2026-09-22-fidelity-gaps-design.md` and the record `docs/superpowers/plans/2026-09-22-fidelity-gaps-derivations.md`.

**Pose/freeze — cycle 4 (branch `pose-freeze`), the demo fight's 9/8 freeze.** This cycle ported the freeze's two halves faithfully and measured the demo observable. The **unfreeze half** (`0x140E4` + `0x170A0` + callees, wired into `camera_decay`) and the **pose-entry half** (`0x193B0` → `0x3B714` → `0x3AAFC` → the pose family, wired into `fighter_pass_a`'s tail) landed, plus `0x18950` (`fighter_connect_query`) as a **faithfulness fix**. The winner-gate record misread the pair table's odd record (`0x0000FF00` → `0x00FF0000`); corrected, both demo queries return 0 and the position compare picks side 0 (T-rex), matching cycle 3's measurement — the "conflict" was a transcription error, now resolved. **Task 5 measured the residual** as the `0x17FA0` page-flag/visibility tail (`0x16AFC` 601 B + `0x164F4` 547 B): `B60=B61=0`, `B54=0`, `B18=B10=B30=0`, `0x100AC0=0x100AC8=0` for the whole state-7 window, so `camera_unfreeze` returned at its visibility gate and `0x193B0` never ran. **Task 6 (`bfd22cb`) ported that tail** (`0x16AFC`/`0x164F4` plus `0x16734`/`0x164C0`), wired at the raw's `0x180C9`/`0x18108` site in `camera_project`; it now fires (`camera_page_tail_b(1)`, ~18 state-7 frames), `B61=1`, `camera_unfreeze(1)` writes `AF8[1] = 5/3/2`, `0x193B0` runs, and **892 port frames change from frame 489** (the winner's blood/reaction effect). The fighters now byte-match at 482/834; the oracle still takes the `res is None` fallback (`0/1068` exhibited, `0 clean / 0 splice / 0 transition / 2779 unexplained`, first 832) because the residual **18 294 B (9.5 %) / 6 194 px** is the **arena backdrop's missing dark mountain silhouette** in `platform/render.c` (`0x38730`/`0x387F4`/`0x38890`/`0x38A38`), owned by the demo-fight-closure subsystem. Every enforced oracle claim is unmoved (`make verify` exit 0, 0 warnings). See `docs/superpowers/specs/2026-09-24-pose-freeze-design.md` and the record `docs/superpowers/plans/2026-09-24-pose-freeze-derivations.md`. **Cycle 5 (below) corrected this residual's attribution:** the port did draw the arena's scene actors, and the missing layer was the crowd actor 0's mountain children, killed by `actor_spawn`'s tail (`0x2B0D4`) in `port/src/game/actors.c` — not `render.c`.

**Arena backdrop — cycle 5 (branch `arena-backdrop`), the demo fight's state-7 mountain layer.** Cycle 4's residual attribution was **wrong twice over**: the port *did* draw the arena's scene actors (layer 2 = sky, layer 1 = sea), and the missing dark mountain silhouette was not `platform/render.c`'s scroll path. The missing layer was the crowd actor 0's **mountain children** (sprite ids 756/757/758, layer 94), spawned and then killed by `actor_spawn`'s tail (`0x2B0D4`), which tested only for the stub `0x5D812` instead of calling the actor type's callback (`DSD(0xBB9DC + type*0xC)`) and testing `AL`. Type `0x1B`'s callback (`0x412FC`) returns 0 (visible), so the faithful dispatch revives the layer — and the five type-0x01 demo spawns killed by the same predicate (measured: 0 of 1381 dumped frames differ). The cycle ported the 16 non-stub callbacks plus `0x2BE5C` in `port/src/game/actors.c`; the size gate did not trigger (closure 25 f / 1 544 B; 17 f / 1 148 B new). **The cycle's gate is MET:** port 482 ↔ capture 834 = **0 / 192 000 B (0 px)**, from 18 294 B / 6 194 px, and port 483–487 ↔ captures 835–839 are also 0 B each. The front-end claim moved `[560..830]`/271/`0 unexplained` → **`[560..842]`/283/`2 unexplained (832, 833)`** — absorbed per the policy because the window is derived from the port's own dump (the fix explains captures 834..842; 832/833 are the loader/held-frame gaps, allowed by name in `tools/title_compare.py`); title/attract/smk/C-vs-Python/`symbols.h` unmoved, `make verify` exit 0, 0 warnings. The demo oracle's report-only fallback stands: its first unexplained is now capture **843 (raw 3750)** — the T-rex pose. See `docs/superpowers/specs/2026-09-24-arena-backdrop-design.md` and the record `docs/superpowers/plans/2026-09-24-arena-backdrop-derivations.md`.

Streamed Smacker audio (2b-ii), the remaining menus/EEPROM storage I/O (4), the
demo fight's remaining arena divergence (the T-rex's animation pose, first
unexplained at capture 843; owner: the `0x34B14`/pose-family selector) and the
interactive match cycle (the mode graph, `0x1EEB0`, the
`0x1EA08` sites) remain;
the attract's `0x2C3FC` voice calls remain declared gaps with `/* PORT: */`
markers. The `0x13xxx` effect render path is no longer a gap (no draw was
missing); its effect call sites are deferred as unreachable. The `0x4F7F4`/
`0x4F83C` scene-palette drivers and the `0x33874` reflow, once declared gaps, are
ported by the small-fidelity-gaps cycle (below).

### Build and run

```bash
cmake -S port -B build && cmake --build build     # or: make build
./build/prageport --game-dir data/game/C          # windowed; ESC to quit
./build/prageport --game-dir data/game/C --check 60
```

The windowed run opens the audio device with the window, at the FM core's
native 49716 Hz stereo rate; if no device can be started it prints SDL's reason
and continues silently (audio is otherwise unconditional — there is no sound
flag). A headless machine can run with `SDL_AUDIODRIVER=dummy` to exercise the
open path without a device.

`--check N` is a headless mode: it runs exactly N master-loop iterations with no
window and no audio device, and writes `frame_NNNN.ppm` (RGB), `frame_NNNN.pal`
(the DAC) and `frame_NNNN.idx` (raw indices) per frame, exiting non-zero on an
internal assertion failure. Because boot runs the state-0 attract first, its
title facts (announcer voice, music notes, non-blank drawn frames) are asserted
relative to the title entry, so a short attract-only run is still valid;
`make verify` uses `--check 820` to cross the attract and exercise them.

### Verify

```bash
PR_ORACLE_REQUIRED=1 ./build/run_tests            # or: make verify
```

`PR_ORACLE_REQUIRED=1` is required for a real verification run: the byte-exact
Ghidra/Smacker/title oracles are git-ignored (they are copies of the game's own
bytes), so without it the suite skips those comparisons. `make verify` runs the
full ladder in order — a `--check 820` headless smoke run (crossing the boot
attract into the title), then the
oracle-required test suite, then `make smk-oracle`, `make title-oracle`,
the GRA-extract oracle tests, and finally `symbols.h` idempotence.

The title oracle compares the port dump against captures of the
**overlay-live** original (four behaviour pins retained; the ported overlay runs):

```bash
make title-pin                                   # patch only the four behaviour sites (three entry RNG draws + the anim opcode-8 draw) into /tmp/pr_title_pin/PRAGE.EXE (writes /tmp only)
make title-oracle                                # align the port dump into data/title-captures/* and compare
```

With two independent captures under `data/title-captures/`, `make title-oracle`
proves both the pixel match (every captured frame is a byte-offset splice of two
adjacent port frames, zero tolerance) and determinism (the captures agree on the
clean samples). With `PR_ORACLE_REQUIRED=1` and fewer than two captures it fails
rather than reporting the proof incomplete.

The attract oracle compares the same continuous run's state-0 prefix against
those post-logo captures; the attract window is derived as the capture frames
before the title window (0..215):

```bash
PR_ATTRACT_DUMP=/tmp/pr_attract PR_GAME_DIR=data/game/C ./build/run_tests
python3 tools/attract_compare.py --capture data/title-captures/title \
    --capture data/title-captures/title2 --port /tmp/pr_attract
```

The comparator matches capture frames 0..214 and reports its first divergence at
the last attract capture frame 215 (raw 2180/2175) — the next unregistered
animation inline code pointer after `0x10FA8` (the hand-off spawn, now
implemented), an individually explained, declared divergence, not a silent skip.
The palette-animation starter `0x4F83C`/`0x10FC4` and the scene-palette driver
`0x4F7F4` are now ported (small-fidelity-gaps cycle); they do not run at runtime
(`DS_00104AD0` bit 0 is never set), so the 215 divergence is unmoved. The
`PR_ATTRACT_DUMP` run
also writes the title window to `/tmp/pr_attract/title`, so `title_compare` can be
run on the same dump.

### Third-party

The FM synthesiser is vendored: **opal 2.0.3**, MIT
(`https://github.com/RealBitdancer/opal`, commit `7e829f33`), whose synthesis
core is by Shayde/Reality (Reality Adlib Tracker 2), public domain. It lives at
`port/src/platform/audio/opl/` with its licence at
`opl/LICENSE.opal.txt`; the files are byte-identical to upstream apart from a
provenance banner. See `THIRD_PARTY_LICENSES.md`.
