# Primal Rage (DOS, 1995) — reverse engineering

Reverse engineering of **Primal Rage**'s PC/DOS release (Time Warner
Interactive / Probe Software / Teeny Weeny Games, 1995), targeting `PRAGE.EXE`
and the `S16*.GRA` graphics set.

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
  extension (LE/LX loader, DOS/4GW aware), installed at
  `~/ghidra_12.2_DEV/Extensions/Ghidra/ghidra-lx-loader` and
  `~/Library/ghidra/ghidra_12.2_DEV/Extensions/ghidra-lx-loader`. It maps
  object 0 to `0x10000` (code) and object 1 to `0x80000` (data) and applies the
  LE fixups, so no manual image extraction is needed.
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
functions**, 0 failures. LE layout and `INDEX` verified; `S16*.GRA` chunk types
2/5/6 decoded (`FORMATS.md`); the per-sprite sub-palette/DAC base and descriptor
`x`/`y` anchor are `likely`, not proven. Pinned: `0x624D4` (entry/startup),
`0x1BEC4` (game main), the `0x255CC` → `0x24C5C` → `0x11D04` frame loop, the
60.05 Hz tick and the literal-`0xA0000` write path — see
`port/spec/game_flow.md`.

**SDL3 port — sub-project 1, engine core: boot → title screen, running.** The
engine is reimplemented in C over a flat `mem[]` holding the original LE data
image at its original addresses; assets come from `data/game/C/` at runtime;
SDL3 lives only in `host.c`/`main.c`. Ported and verified: the LE loader +
fixups (byte-exact against Ghidra's image), the `INDEX` resource manager,
`fn_resolve` code-address mapping, GRA decode (exact-consumption primary —
18,201/18,202 descriptors across all 69 files — and byte-identical to
`tools/gra_render.py` for the four full-screen `S16TITLE` frames `{10,12,13,18}`
only; palette bank flattened, sub-palette choice `likely`), the palette flush
(`0x1C470`), the process-table scheduler and the `0x255CC`/`0x24C5C`/`0x11D04`
loop. Boot runs the original's state-0 attract first (4d below). The title state
runs the real `0x121A0` actor composite through the display list; `make
title-oracle` drives the continuous run to the pinned title window (port frames
1..95 exhibited by both captures, start-of-window transition disclosed, zero
unexplained frames) — see 4a-ii/4a-iii below.

**Audio — sub-project 2a, AIL/Miles, running.** The game's own audio path runs
with no DOS driver: `0x1CF40` AIL init completes; the title/attract XMIDI bank
decodes and sequences into OPL register writes through a vendored FM core; one
located announcer sample plays through the game's sample request/play path;
mixed stereo frames reach SDL audio when a device opens. FM data is byte-exact
against an independent Python decoder (9866 OPL register writes, zero
differences) under a **symmetric oracle** (both streams anchored at their first
key-on), with the captured original trace as the reference. `SBPRO2.MDI`'s
MIDI-controller path is reproduced — channel handler `0x3b54`, mask-driven
family re-apply `0x3184`, the driver's `0x35fa` frequency routine (retiring the
hand-fitted note table), the total-level/pan/mod folds — closing both named
divergences: the mid-note frequency change (a bend re-applies the frequency
family) and the total-level term (the driver's TL law; the engine's sequence
volume scales CC7). The first difference advanced from C write 302 to **C write
430**; what remains is an **intra-tick write-order** difference (same registers
and values each tick, different order; per-tick write multisets identical),
carrier TL on MIDI channels 1 and 4 (engine per-channel volume provably
unpinnable — one sequence volume, one timer tick), and the `0xBD` rhythm
register. The 9866 (port) / 6903 (reduced capture) counts are not a like-for-like
gap. Remaining differences are named or excluded with cause, never tuned. **No
audibility claim**: the oracle is the register write stream, not rendered audio;
the render is FM music only, without the title announcer sample. The windowed
run is silent where SDL audio cannot start (here `-66681`); `make audio-render`
plays the title bank through sequencer + OPL core + mixer to a 16-bit stereo WAV
at the OPL rate (`AUDIO_WAV`, default `/tmp/pr_title_fm.wav`; `AUDIO_SECONDS`,
default 12) — the path is real, only the device missing. See
`docs/superpowers/plans/2026-09-16-audio-ail-port-report.md` and
`docs/superpowers/plans/2026-09-18-opl-driver-report.md`.

**Video — sub-project 2b-i, Smacker logos, running.** The two boot movies
(`twi5.smk`, `twg.smk`) decode with an in-repo, clean-room SMK2 decoder and play
on the boot path at the attract machine's phase 0 (`0x11000` case 0) before the
title. Both movies' presented frames are **pixel-exact** against DOSBox-X
captures of the original (RGB24, palette included): TWI5 120/120, TWG 41/41; the
container layout reconciles byte-exactly on both files. The decoder is
allocation-free and fixed-profile (rejects everything else by name); the player
owns the content-based presentation rule, which carries a `TODO(verify)` on the
original's `0x1C740` loop. See
`docs/superpowers/plans/2026-09-17-smacker-video-report.md`.

**Sprite compositor — sub-project 4a-i, ported.** The display list composites
into the back buffer as `0x14328` does: display node and builder (`0x14268`),
span blitter (`0x51E5C`), all six reachable renderers (RLE, clipped, mirrored,
raw, mode-1 shear), the 580-node sorted list, and the projection/clip driver. The
RLE renderer is byte-identical to sub-project 1's already-verified decoder on 32
real sprites; the clipped/mirrored/shear renderers are proven by hand-computed
tests, not an emulator (the DOSBox title oracle is 4a-ii's). `game_loop` calls
the compositor in the original's order — a proven no-op until 4a-ii populates the
list. See `docs/superpowers/plans/2026-09-17-sprite-compositor-report.md`.

**Actor system and title — sub-project 4a-ii, ported.** The engine runs its own
actor pool and real title state: the `0x68`-byte records and their lists, spawn
`0x2AE14`, the pset sync, the motion step, the animation-stream interpreter
(`0x2A408`/`0x29F34`/`0x29DB8` and the 47-opcode dispatcher `0x2B2A0`), the text
grid and glyph renderer, the `0x5D7DC` LCG, and `0x121A0` with its `ENGLISH.TXT`
caption. `make title-oracle` proves the composite against two independent
captures of the overlay-live original (four behaviour pins retained): the capture
is a byte-offset splice of two adjacent port frames because the original updates
the aperture at `0x255CC` with no retrace wait while DX-CAPTURE samples at
70.09 Hz, and every captured frame is explained with zero pixel tolerance and
zero unexplained frames. The oracle forced three defect fixes (the `0x33754`
palette-table entry, the 6-bit VGA DAC, the 16.16 actor velocity). One confirmed
finding contradicts the earlier RE: the in-window opcode-8 count is 0, so pin
site 4 is inert and the `task-2-anchor-re.md` account of the title's
nondeterminism is wrong. See
`docs/superpowers/plans/2026-09-17-actor-system-report.md`.

**Title-path residuals — sub-project 4a-iii, ported.** 4a-ii's oracle proved a
**pinned** original (`tools/title_pin.py` also ret'd `0x2BF08`, hiding the
overlay); 4a-iii removed that inert site, re-captured the true original,
diagnosed the per-frame divergence (`CREDITS:5`, rows 7–12), and ported
`0x2BF08`'s four-branch message/text tick (`DS_00105C05` seeded then, now
produced by the attract's `0x2C06C(1)`; `DS_00105C00` derives from the config
module) plus the `0x13xxx` effect-list slice — spawn `0x13C70`, free-list build
`0x13ADC`, clear `0x13DF0`, teardown `0x13420`, step/age `0x134C0` — in
`port/src/game/effects.{c,h}`. The oracle is green against the overlay-live
original (four behaviour pins retained; 0 unexplained, port frames 95/96
exhibited) and turns red when `0x2BF08` is stubbed out, so it proves the overlay.
`0x13C70`'s spawn is a **declared coverage gap**: it fills a record, but the
effect render path is unported, so the spawn alone draws nothing and the oracle
stays green either way; once `0x134C0` is ported the spawn jointly explains
frame 95 (removing it reverts frame 95 to `missing`, which the oracle discloses
rather than fails), so the spawn's own gate is Task 5's unit tests. The `0x134C0`
step (ported when the wiring exposed a count-drain stall past the window) is
unit-proven to drain and oracle-consistent. See
`docs/superpowers/plans/2026-09-18-title-residuals-report.md`.

**Effect producers — sub-project 4a-iv, ported.** The three missing `0x13xxx`
producers now sit beside `0x13C70`: `0x13D4C` (type 4, darken-to-zero,
`effects_spawn_darken`), `0x13E28` (type 6, darken toward a target,
`effects_spawn_pulse`) and `0x13B3C` (types 0/2, signed-offset scroll,
`effects_spawn_scroll`). The free-list head is taken only by these four, so types
1 and 5 have no producer and are dead. An end-to-end unit test proves the chain
spawn → `effects_step` → palette dirty list → `gfx_flush_palette` → `gfx_dac`.
`0x13D4C` is called only from the unported effect call sites
(`0x29B74`/`0x41578`); `0x13E28` from the ported select state `0x11F6C`
(`flow.c:367`). **`0x13B3C` is dead, not merely unwired:** zero callers and zero
cross-references anywhere in the image (`ghidra_get_xrefs_to 0x13B3C` = 0;
`prage.functions.csv` `FUN_00013b3c` `n_callers = 0`; the bytes `3c b3 01 00`
occur nowhere) — the earlier "live via the jump table at `0x23AC4`" was a
mis-transcription (that table's dwords are `0x23B3C`, `0x23B20`, …). The effect
**render** path is no longer a gap (no draw was missing); its call sites are
deferred as unreachable. See
`docs/superpowers/plans/2026-09-19-effects-producers-report.md`.

**EEPROM/config core — sub-project 4b-A, ported.** `port/src/game/config.{c,h}`
ports the packed field layer (`0x2D974`/`0x2DA0C`), the menu-default parser
(`0x2CCD0`), the defaults writer (`0x2CADC`) and the magic validate (`0x2D6F8`).
The descriptor table and config bytes come from the loaded image; the storage
layer and save/load I/O stay declared no-ops, so validate takes the
fresh-machine defaults path. `game_init` derives `DS_00104528` and the three
`0x20C9F`–`0x20CC2` globals from field `0x29`, and `0x2C304` derives the credit
counter `DS_00105C00 = 5` — the unseed the title oracle validates (0
unexplained). The storage path remains a 4b gap; 4b-B ports the credit countdown
and the front-end input/select path. See
`docs/superpowers/plans/2026-09-19-eeprom-config-report.md`.

**Front-end input, credits and the select state — sub-project 4b-B, ported.**
`port/src/platform/input.c` samples the host key bitmap into the `0x500C4`
debounced level (`0x50161` selector); `0x4F644` builds the newly-pressed and held
masks each frame before the state machine. `config.c`'s credit layer
(`0x2C060`/`0x2CA48`/`0x2CA7C`/`0x2C06C`/`0x2BF00`) is driven by `0x11F28`
(`frontend_coin_poll`); the `0x11F6C` select state (`0x33904`/`0x1C6D4`) is
live, its exit advancing `DS_000F0A64` to state 3 (ported in the front-end-chain
cycle below). See `docs/superpowers/plans/2026-09-19-frontend-input-report.md`.

**Attract/boot subsystem — sub-project 4d, ported.** The port boots through
state 0's attract, as the original does. `port/src/game/attract.{c,h}` owns the
13-phase `0x11000` machine (its phase 0 plays the two Smacker logos), the
per-state tails `0x10DB0`/`0x10E18`, the reset/scheduler `0x10EE4`/`0x10F28` and
the per-bit scene tick `0x292AC`; `render.c` owns the scroll/zoom projection
`0x389C4`/`0x38A38`; `flow.c` keeps only the state dispatch. `game_state_init`
enters state 0; the deleted `DS_00105C05 = 1` stand-in is replaced by the
attract's own `0x2C06C(1)` (the captured title row 1). Proof: one continuous
headless run aligned to the post-logo captures (attract prefix, capture frames
`0..215`, plus the title window in one `game_init()`), an env-gated
`PR_ATTRACT_DUMP` driver with a run-to-run frame-hash log, and raw-derived unit
tests; `tools/attract_compare.py` matches capture frames 0..214, first
divergence at the last attract frame 215 (raw 2180/2175). The RAGE-logo
actor's animation reaches opcode 0x11's inline code pointer `0x10FA8`; the port
implements it (spawning the descriptor `0x9AD08` hand-off actor at layer `0xE4`,
exactly the raw's `0x2AE14` call) and registers it in `actors_init`, so the
logo's continuation runs and the matched prefix grew from frame 100 to the whole
window. The palette-animation starter `0x4F83C` (and `0x10FC4`) and the
scene-palette driver `0x4F7F4` — once declared gaps here, ported by the
small-fidelity-gaps cycle below, as is the `0x33874` reflow — are registered
through `attract_scene_tick` but do not run (`DS_00104AD0` bit 0 is never set),
so the frame-215 divergence is unmoved; its owner is the loader-frame DAC state
/ read-gate model (record §3.5), not these drivers. The title window stays 0
unexplained on both captures. (The `0x11000` spawn-slot fix `e29f849` moved the
boundary from frame 68; the hand-off moved it from frame 100.) The oracle also
surfaced and fixed a real `0x336C0` bug: `palette_list_init` omitted the raw's
palette ownership-table clear at `0x87618`, which the attract's earlier palette
acquires exposed. See `docs/superpowers/plans/2026-09-19-attract-report.md`.

**Front-end chain — states 3/4/5, ported.** `port/src/game/flow.c` ports the
states after the select carousel: state 3 (`0x12484`, the post-select
presentation and its `0x12658` handoff), state 4 (`0x11578`, the 8/12/13-line
credit roll) and state 5 (the inline `0x11D04` match-start block), so the port
advances carousel → 3 → 4 → 5 → 9 → 6 unaided. `effects.c` owns the `0x13xxx`
effect render path: `0x1324C` (screen-shake decay) is update-table entry 0 but
**dormant** (no shipped store sets `DS_00104AE8` bit 0); the palette path
spawn → `effects_step` → dirty list → `gfx_flush_palette` → `gfx_dac` →
`gfx_present` is proven end to end. **No camera/scene function draws** — the
plan's assumed missing draw does not exist; the camera state feeds the existing
render pass and actor-pset sync. The front-end pixel oracle is **closed and
enforced**: a 120 s pinned capture aligns the port's state-3 zoom to window
`[560..858]` (raw `3108..3765`), **299 frames: 134 clean, 161 splice, 0
transition, 2 unexplained (832, 833)** — the two allowed by name (the
arena-backdrop cycle's absorbed claim move, below); any other unexplained frame
fails. (Indices moved `[557..813]`/257 → `[560..830]`/271 → `[560..842]`/283 →
`[560..850]`/291 → `[560..857]`/298 → `[560..858]`/299 as cycle 1's pins, cycle 2's master-loop pin and the
arena-backdrop fix forced re-captures, the demo-pose cycle's `0x3A43C`
stack-offset fix + `0x186C4` re-latch explained captures 843..850, and the
roar-timing fix (the `0x3AD27` pose-setter operand) explained 851..857, and the
frame-858 fix (`0x2A690`'s mode-1 x operands) explained 858; the host-timed capture is not reproducible, so indices shift while
the claim does not.) It proves exactly one thing: **no content-bearing capture
frame inside the window the port's own dump exhibits is unexplained** (the two
named exceptions aside) — the window is derived from that dump and the
coverage/`endpoints BAD` counts are ignored, so a port that **under-renders** the
front-end (only the state-3 entry frame, or only the first 12 frames) still
passes. Sixteen all-black capture frames are excluded as an explicit oracle-level
choice, **not** a proven fact (capture 561's black frame may be a distinct logic
frame or a 70.09 Hz scanout artifact). The effect call sites `0x29B74`/`0x41578`
are **deferred**: reachable only through `0x24C5C`'s unported mode cases
(`0x12`, `0x16..0x1B`) and the match/fight chain, and `DS_00104B00` is fixed at
3 — a dispatch path nothing can reach. The match cycle's `0x1EA08` call sites
and the unported half of `0x1EA08` remain declared gaps. The camera chain
(`0x12CD4`/`0x1317C`/`0x13290`/`0x1333C`, `0x12D48` dispatcher,
`0x12DA8`/`0x12DF0`/`0x12E3C` modes) is **ported** in `port/src/game/camera.c`,
dispatched from `flow.c:1346` (`camera_dispatch()`). See
`docs/superpowers/plans/2026-09-20-frontend-chain-derivations.md` and
`port/spec/game_flow.md`.

**Attract demo fight — cycles 1/2 (motion, closure), ported.** States 6/7 (the
CPU-vs-CPU demo, **not** the interactive match): state 3's `0x12658` handoff
enters the 240-frame state-9 hold; state 6 picks the two characters from the
shared RNG and spawns them; state 7 runs the arena with the ported CPU-AI
command generator `0x47208`, the `+0x52` state machine (`0x3531C`/`0x350D0`),
the `0x3C88C` hitbox machine and the `0x3CF38` hit chain — the chain now
**fires**, hits land (`+0x7C` 0/0 → 1/1), and the fight's last state change
moved **1262 → 1400**. Its report-only oracle (`make demo-oracle`) measures the
demo window `[859..3616]` (raw `3766..8409`), **2758 frames: 0 clean / 0 splice
/ 0 transition / 2752 unexplained** (6 all-black frames excluded); the first
unexplained frame is capture **859 (raw 3766)** — one worshipper's pose at port
504 (moved from 843 by the demo-pose cycle, then from 851 by the roar-timing
fix, then from 858 by the frame-858 fix), not the state-9 hold. (Cycle 1's Amendment 5 said the fight begins at 839/28;
Task 1 corrected it to **836/25**, and the final re-capture moved the loader text
to 832 with the fight's first frames at 833/834 — record §9.1/§9.3.) Cycle 2
advanced the boundary 811 → 816 → 832: the state-9 globe's fourth layer
(`0x12720`) made the hold match, the `rle_row` mirror-window fix and the
idle-animation tick (`0x37A58`) improved the arena, and the chain drives the
fight past its former stall. **The Gate — 0 unexplained frames — is UNMET**,
recorded as such: capture 832's read-stall is **proven un-derivable** (the port
produces both held states but the gate passes on the loader frame; the post-read
ISR tick count is a host/emulator property with two live-RAM polls disagreeing,
Δ=2 vs Δ=3), and the residuals are the palette-order subsystem, the fighters'
animation poses and the fight's stall tail (the unported `0x34B14` handlers). No
pin or value was fitted to force the Gate. The state-9 hold draws no RNG; the
demo's two state-6 character picks are **not** pinned — cycle 2 replaced cycle
1's fitted picks with a source-level pin on the master loop's host-timed draws
(`0x256B1`/`0x256D6` → non-advancing), so the oracle validates the picks. The
**interactive match** (the mode graph, the `0x257A4` coin divert, `0x1EEB0`,
`0x1F458`, the player screens and human input) remains **unowned**. See
`port/spec/game_flow.md`, the design spec
`docs/superpowers/specs/2026-09-21-demo-fight-closure-design.md` and the record
`docs/superpowers/plans/2026-09-21-demo-fight-closure-derivations.md`.

**Combat/render fidelity — cycle 3 (branch `combat-fidelity`), the demo fight's palette and combat tail.** Cycle 3 closed three of cycle 2's residuals. The T-rex's colour difference was the **front-end dump driver's `DS_0010816A` seed** (0xFF → 0), not the engine — the acquisition sequences are identical and `0x41350` is faithful — so the T-rex's handle is now the original's `0x1BB9FCD8` at DAC range `start=142 len=31`. Seven of the twelve `0x34B14` `+0x52` handlers landed (`0x35F84`/`0x36430`/`0x399CC`/`0x361C8`/`0x36300`/`0x36710`/`0x364FC`); the state-7 entry's four missing RNG draws landed (the type-0 effect handler `0x4AAD0`/`0x4B144`, so the entry LCG now reaches `0x10F7DB07`, matching); and the demo-AI block state landed (`0x18540`/`0x18350`, so `cmd0@1072 = 0x4848` and `cmd1@1071 = 0x0002`, matching). The arena's byte-diff against capture 834 fell **42 667 B (22.2 %) → 18 294 B (9.5 %)**, the T-rex region 6 363 → 1 773 px and the raptor region 8 704 → 4 421 px, and the raptor's silhouette IoU rose **0.522 → 1.000**. **The Gate is UNMET overall:** the fight reaches the state-7 900-frame timer exit (`s7_last == 1969`) but freezes from loop 1072 — the original's pose state `0x10`/`0x0A` is reachable only through the unported `0x19020` → `0x193B0` → `0x3B714` → `0x3AAFC` → pose-family chain (68 new funcs / 10 467 B; true new ≥ 11 012 B), so the pose subsystem passes to **cycle 4**. The port's hit counter `+0x7C` is 0 for the whole state-7 window: cycle 2's `+0x7C 0/0 → 1/1` was a by-product of the old diverged trajectory, and with the trajectory now matching the original's early path the hit needs the pose state the port cannot reach. Every enforced oracle claim is unmoved (`make verify` exit 0, 0 warnings). See `docs/superpowers/specs/2026-09-22-combat-fidelity-design.md` and the record `docs/superpowers/plans/2026-09-22-combat-fidelity-derivations.md`.

**Small fidelity gaps — cycle (branch `fidelity-gaps`), the four off-demo-path residuals.** Closed all four of cycle 3's small named gaps; no enforced oracle claim moved. The five remaining `+0x52` handlers landed (`0x359E0`/`0x35C1C`/`0x35D20`/`0x37464`/`0x33B00`/`0x35E6C`), with `0x349C8`'s bit-6/7 deep callees (`0x385B0`/`0x39A10`) wired at their raw sites (`0x36884`/`0x37D57`) via the minimal caller chain (5 functions / ~1 741 B, ratified by the human). The loader flush scope's record premise was **refuted by the raw** — `0x1C470` is a whole-list drain and the port's flush was already faithful; the real gap, the missing initial record `{0xBD470, 0, 1, 0}`, is now enqueued. The attract/scene palette drivers (`0x4F7F4`/`0x4F83C`/`0x33874`) are ported and registered; `0x4F83C` ships test-only (its `0xE8916`-table callers are unported) as an **explicit accepted exception** (spec representation rule, Q10). The 169-tick state-9 hold is a **real divergence** (the state-9 screen's animation stops ~169 ticks early, holding capture 830's frame byte-static), oracle-neutral and carried forward with its owner. No single group triggered the size gate, but the **branch total crosses it**: 20 functions / 4 675 B (the four groups 10/2 223, 2/418, 0, 3/293 plus the human-ratified caller chain 5/1 741). Every enforced oracle claim is unmoved (`make verify` exit 0, 0 warnings). See `docs/superpowers/specs/2026-09-22-fidelity-gaps-design.md` and the record `docs/superpowers/plans/2026-09-22-fidelity-gaps-derivations.md`.

**Pose/freeze — cycle 4 (branch `pose-freeze`), the demo fight's 9/8 freeze.** Ported the freeze's two halves faithfully and measured the demo observable. The **unfreeze half** (`0x140E4` + `0x170A0` + callees, wired into `camera_decay`) and the **pose-entry half** (`0x193B0` → `0x3B714` → `0x3AAFC` → the pose family, wired into `fighter_pass_a`'s tail) landed, plus `0x18950` (`fighter_connect_query`) as a **faithfulness fix**. The winner-gate record misread the pair table's odd record (`0x0000FF00` → `0x00FF0000`); corrected, both demo queries return 0 and the position compare picks side 0 (T-rex), matching cycle 3 — the "conflict" was a transcription error. **Task 5 measured the residual** as the `0x17FA0` page-flag/visibility tail (`0x16AFC` 601 B + `0x164F4` 547 B): `B60=B61=0`, `B54=0`, `B18=B10=B30=0`, `0x100AC0=0x100AC8=0` for the whole state-7 window, so `camera_unfreeze` returned at its visibility gate and `0x193B0` never ran. **Task 6 (`bfd22cb`) ported that tail** (`0x16AFC`/`0x164F4` plus `0x16734`/`0x164C0`), wired at the raw's `0x180C9`/`0x18108` site in `camera_project`; it now fires (`camera_page_tail_b(1)`, ~18 state-7 frames), `B61=1`, `camera_unfreeze(1)` writes `AF8[1] = 5/3/2`, `0x193B0` runs, and **892 port frames change from frame 489** (the winner's blood/reaction effect). The fighters now byte-match at 482/834; the oracle still takes the `res is None` fallback (`0/1068` exhibited, `0 clean / 0 splice / 0 transition / 2779 unexplained`, first 832), attributing the residual **18 294 B (9.5 %) / 6 194 px** to the **arena backdrop's missing dark mountain silhouette** in `platform/render.c` (`0x38730`/`0x387F4`/`0x38890`/`0x38A38`) — **cycle 5 (below) refuted that attribution**. Every enforced oracle claim is unmoved (`make verify` exit 0, 0 warnings). See `docs/superpowers/specs/2026-09-24-pose-freeze-design.md` and the record `docs/superpowers/plans/2026-09-24-pose-freeze-derivations.md`.

**Arena backdrop — cycle 5 (branch `arena-backdrop`), the demo fight's state-7 mountain layer.** Cycle 4's residual attribution was **wrong twice over**: the port *did* draw the arena's scene actors (layer 2 = sky, layer 1 = sea), and the missing dark mountain silhouette was not `platform/render.c`'s scroll path. The missing layer was the crowd actor 0's **mountain children** (sprite ids 756/757/758, layer 94), spawned then killed by `actor_spawn`'s tail (`0x2B0D4`), which tested only for the stub `0x5D812` instead of calling the actor type's callback (`DSD(0xBB9DC + type*0xC)`) and testing `AL`. Type `0x1B`'s callback (`0x412FC`) returns 0 (visible), so the faithful dispatch revives the layer — and the five type-0x01 demo spawns killed by the same predicate (measured: 0 of 1381 dumped frames differ). The cycle ported the 16 non-stub callbacks plus `0x2BE5C` in `port/src/game/actors.c`; the size gate did not trigger (closure 25 f / 1 544 B; 17 f / 1 148 B new). **The cycle's gate is MET:** port 482 ↔ capture 834 = **0 / 192 000 B (0 px)**, from 18 294 B / 6 194 px; captures 834..839 are byte-exact at 0 B each (port 483–487 ↔ 835–839). The front-end claim's window end grew to 842 (`[560..830]`/271/`0 unexplained` → **`[560..842]`/283/`2 unexplained (832, 833)`**) — absorbed per the policy because the window is derived from the port's own dump (the fix explains captures 834..842; the loader frame 832 and the held dark-arena frame 833 are the front-end window's two named unexplained, allowed by name in `tools/title_compare.py`); title/attract/smk/C-vs-Python/`symbols.h` unmoved, `make verify` exit 0, 0 warnings. The demo oracle's report-only fallback stands: its first unexplained is now capture **843 (raw 3750)** — the T-rex pose. See `docs/superpowers/specs/2026-09-24-arena-backdrop-design.md` and the record `docs/superpowers/plans/2026-09-24-arena-backdrop-derivations.md`.

**Pose handler — cycle 6 (branch `demo-pose`), the demo fight's T-rex pose. Goal partly reached; residual named.** The cycle's goal was the whole fight window `[843..1884]` at 0 unexplained (the demo oracle's first unexplained 843 → 1886). It was **not** reached.

* **What landed.** The pose handler `0x3A43C` (the record corrected the design's `0x39CC8`: the demo's pose setter calls `0x3A43C`) and the frame-hold scaler `0x39A34`; then, from the 43 px differential, two raw-derived fixes — `0x3A43C` reads its context at the raw `RET 4` stack offsets (`0x2BC30`, `0x2BCEF`: `side = ctx[1]`, `rec_self = ctx[5]`), and the `0x186C4` two-slot re-latch at `0x35829` is ported. The size gate did not trigger (Task 1: genuinely-new closure 2 f / 245 B).
* **Measured.** The direct pairs port 490 ↔ capture 843, 491 ↔ 844 and 496 ↔ 850 are now **0 B** (from 16 101 B / 5 793 px on the unmodified port), so captures 843..850 are explained. The demo oracle's first unexplained moved **843 → 851 (raw 3758)**: the demo window is `[851..3616]`, 2766 frames, 2760 unexplained; the fight window `[851..1884]` (raw `3758..4791`) has 1034 frames, **0 explained**.
* **Residual.** Capture 851 (port 497 = f 80): the port advances the T-rex's roar animation one frame early (`0x9076 → 0x9077`); likely owner the roar stream's frame-hold timing (`0x39A34`'s `rec+0x24`, read by `frame_timer` `0x2AA70`).
* **Claim move (ruled, absorbed in the same commit `dad2712`).** Front-end `[560..842]` / 283 → **`[560..850]` / 291 / `130 clean, 157 splice, 0 transition, 2 unexplained (832, 833)`**. The four classes sum to 289 of the 291 frames: the two all-black captures 561 and 831 are excluded as artifacts.
* **New enforced line — a ratchet.** `make demo-fight-oracle` (in `make verify`) fails if the first unexplained fight-window frame is earlier than N = **851** (pinned in the `Makefile` with its provenance) or the fight window collapses; a later first-unexplained passes as an improvement and N is then raised. It is honestly near-vacuous today (0/1034 explained: it guards the front-end window's end and the fight window's start) and becomes a real regression guard as fight frames are explained.
* **Named gaps still open.** The roar animation timing at f = 80; the `0x354F0` arena-wall clamp; `hit_record_x/y` (`0x18714`/`0x18788`) omitting the raw's `0x18540`/`0x18350` calls (first differs at f = 90); the camera split-arm `0x18714` write; the sibling pose handlers `0x3A588`/`0x3A6D4`/`0x3A820` and the `0x39F40`/`0x39CC8` pose family (unreached in the demo's first fight); the `0x2C3FC` voice stays a stub (RNG- and fight-state-neutral, record §3.4).
* **Unmoved.** Every other enforced claim (title, attract 215, smk, C-vs-Python 9866, `symbols.h`).

See `docs/superpowers/specs/2026-09-24-demo-pose-design.md` and the record `docs/superpowers/plans/2026-09-24-demo-pose-derivations.md`.

**Roar timing (branch `roar-timing`), the demo fight's capture 851.** The roar's early frame advance was not the frame timer: `0x2AA70`, `0x39A34` and `0x2BC30` re-diff clean against the raw. The cause was the port's read of the pose setter's operand at `0x3AD23..0x3AD2E`: the raw is `mov esi,[esi+3]` / `sar esi,0x18` (`8b 76 03` / `c1 fe 18`), so the operand is the **signed byte at the anim3 row's +6**, and the port read +3. The demo's row `0xDE95F` (`08 18 08 73 02 0e fd 11`) gives −3, not 115, so the setter's `slot+0x7E` (`0x3A549..0x3A554`: `byte[0xBECF8] + CL`) is `0x11`, not `0x87`, and `0x39A34` sets the roar's frame hold to **+1.7**, not −12.1: each roar sprite holds about two frames instead of one. One line of code; no size gate.

* **Measured.** Port 497 ↔ capture 851: 6 544 B / 2 456 px → **0 B**. Captures 851..857 are explained. The demo oracle's first unexplained is now **858 (raw 3765)**: the demo window `[858..3616]` has 2759 frames, 2753 unexplained, and the fight window `[858..1884]` has 1027 frames, 0 explained. The ratchet N is raised **851 → 858** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..850]` / 291 → **`[560..857]` / 298 / `134 clean, 160 splice, 0 transition, 2 unexplained (832, 833)`**. Nothing else moved.
* **Residual (characterised, not fixed).** In capture 858 (port 502/503, f ≈ 86), rows 175–199 match port 503 exactly. Rows 67–174, which hold the fighters and the backdrop band around them, match neither 502 nor 503. The best splice (502/503 at byte 131 205) leaves 13 617 B / 5 253 px, and the gap grows in the next captures.

See §11 of the record `docs/superpowers/plans/2026-09-24-demo-pose-derivations.md`.

**Mode-1 x (`b2cb490`), the demo fight's capture 858.** The camera starts moving at f = 85, and from f = 86 the port scrolled the arena's mode-1 props wrongly. The pset x writer `0x2A690` reads both mode-1 operands as the high word of a dword load. `0x2A733`/`0x2A739` (`8b 43 44` / `c1 f8 10`) read the ramp entry just stored at `rec+0x46`. `0x2A6E0`/`0x2A6E9` (`8b 43 32` / `c1 f8 10`) read the velocity word at `rec+0x34`, the one `0x2A6D9` gates on. The port read the low words, `+0x44` and `+0x32`. So the temple, props and crowd did not scroll at all, and the `0x02F4` mountain (with its `0x02F5`/`0x02F6` children) moved 5.7 px instead of 0.2 px at f = 86. Two operand reads in one function; no size gate.

* **Measured.** Capture 858 is now a 0-byte splice of port 502/503 at byte 64 254 (row 66), from 13 617 B / 5 253 px. The demo oracle's first unexplained is now **859 (raw 3766)**. The demo window `[859..3616]` has 2758 frames, 2752 unexplained, and the fight window `[859..1884]` has 1026 frames, 0 explained. The ratchet N is raised **858 → 859** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..857]` / 298 → **`[560..858]` / 299 / `134 clean, 161 splice, 0 transition, 2 unexplained (832, 833)`**. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 859's best splice (port 503/504 at byte 90 582, row 94) leaves 449 B / 159 px, all in x 86–111, rows 137–174: the worshipper behind the T-rex (sprite `0x07E1`, layer 208). It shows a different pose from port 504's, not a shift (the best offset is 0).

See §12 of the record `docs/superpowers/plans/2026-09-24-demo-pose-derivations.md`.

Streamed Smacker audio (2b-ii), the remaining menus/EEPROM storage I/O (4), the
demo fight's remaining arena divergence (first unexplained at capture 859
after the frame-858 fix: one worshipper's pose at port 504, owner not yet
derived) and the
interactive match cycle (the mode graph, `0x1EEB0`, the `0x1EA08` sites) remain;
the attract's `0x2C3FC` voice calls remain declared gaps with `/* PORT: */`
markers.

### Build and run

```bash
cmake -S port -B build && cmake --build build     # or: make build
./build/prageport --game-dir data/game/C          # windowed; ESC to quit
./build/prageport --game-dir data/game/C --check 60
```

The windowed run opens the audio device with the window at the FM core's native
49716 Hz stereo rate; if no device can be started it prints SDL's reason and
continues silently (audio is otherwise unconditional — there is no sound flag).
`SDL_AUDIODRIVER=dummy` exercises the open path on a headless machine.

`--check N` runs exactly N master-loop iterations with no window and no audio
device, writing `frame_NNNN.ppm` (RGB), `frame_NNNN.pal` (the DAC) and
`frame_NNNN.idx` (raw indices) per frame, and exits non-zero on an internal
assertion failure. Because boot runs the state-0 attract first, the title facts
(announcer voice, music notes, non-blank drawn frames) are asserted relative to
the title entry, so a short attract-only run is still valid; `make verify` uses
`--check 820` to cross the attract and exercise them.

### Verify

```bash
PR_ORACLE_REQUIRED=1 ./build/run_tests            # or: make verify
```

`PR_ORACLE_REQUIRED=1` is required for a real verification run: the byte-exact
Ghidra/Smacker/title oracles are git-ignored copies of the game's own bytes, so
without it the suite skips those comparisons. `make verify` runs the full ladder
in order — a `--check 820` headless smoke run (crossing the boot attract into
the title), the oracle-required test suite, `make smk-oracle`, `make
title-oracle`, the GRA-extract oracle tests, and `symbols.h` idempotence.

The title oracle compares the port dump against captures of the **overlay-live**
original (four behaviour pins retained; the ported overlay runs):

```bash
make title-pin                                   # patch only the four behaviour sites (three entry RNG draws + the anim opcode-8 draw) into /tmp/pr_title_pin/PRAGE.EXE (writes /tmp only)
make title-oracle                                # align the port dump into data/title-captures/* and compare
```

With two independent captures under `data/title-captures/`, it proves both the
pixel match (every captured frame is a byte-offset splice of two adjacent port
frames, zero tolerance) and determinism (the captures agree on the clean
samples). With `PR_ORACLE_REQUIRED=1` and fewer than two captures it fails
rather than reporting the proof incomplete.

The attract oracle compares the same continuous run's state-0 prefix against
those post-logo captures, with the attract window derived as the capture frames
before the title window (0..215):

```bash
PR_ATTRACT_DUMP=/tmp/pr_attract PR_GAME_DIR=data/game/C ./build/run_tests
python3 tools/attract_compare.py --capture data/title-captures/title \
    --capture data/title-captures/title2 --port /tmp/pr_attract
```

The comparator matches capture frames 0..214 and reports its first divergence at
the last attract frame 215 (raw 2180/2175) — the next unregistered animation
inline code pointer after `0x10FA8` (the hand-off spawn, now implemented), an
individually explained, declared divergence, not a silent skip. The
palette-animation starter `0x4F83C`/`0x10FC4` and the scene-palette driver
`0x4F7F4` are ported (small-fidelity-gaps cycle) but do not run
(`DS_00104AD0` bit 0 is never set), so the 215 divergence is unmoved. The
`PR_ATTRACT_DUMP` run also writes the title window to `/tmp/pr_attract/title`,
so `title_compare` can run on the same dump.

### Third-party

The FM synthesiser is vendored: **opal 2.0.3**, MIT
(`https://github.com/RealBitdancer/opal`, commit `7e829f33`); its synthesis core
is by Shayde/Reality (Reality Adlib Tracker 2), public domain. It lives at
`port/src/platform/audio/opl/` with its licence at `opl/LICENSE.opal.txt`; the
files are byte-identical to upstream apart from a provenance banner. See
`THIRD_PARTY_LICENSES.md`.
