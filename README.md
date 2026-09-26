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
`[560..1714]` (raw `3108..4621`), **1155 frames: 455 clean, 693 splice, 3
transition, 2 unexplained (832, 833)** — the two allowed by name (the
arena-backdrop cycle's absorbed claim move, below); any other unexplained frame
fails. (Indices moved `[557..813]`/257 → `[560..830]`/271 → `[560..842]`/283 →
`[560..850]`/291 → `[560..857]`/298 → `[560..858]`/299 → `[560..859]`/300 → `[560..863]`/304 → `[560..865]`/306 → `[560..866]`/307 → `[560..869]`/310 → `[560..879]`/320 → `[560..890]`/331 → `[560..891]`/332 → `[560..949]`/390 → `[560..991]`/432 → `[560..997]`/438 → `[560..1357]`/798 → `[560..1410]`/851 → `[560..1477]`/918 → `[560..1480]`/921 → `[560..1545]`/986 → `[560..1562]`/1003 → `[560..1658]`/1099 → `[560..1714]`/1155 as cycle 1's pins, cycle 2's master-loop pin and the
arena-backdrop fix forced re-captures, the demo-pose cycle's `0x3A43C`
stack-offset fix + `0x186C4` re-latch explained captures 843..850, and the
roar-timing fix (the `0x3AD27` pose-setter operand) explained 851..857, and the
frame-858 fix (`0x2A690`'s mode-1 x operands) explained 858, and the
frame-859 fix (`0x49C78`'s case-1 walk arrival) explained 859, and the
frame-860 fix (`0x49C78`'s tail reset `0x4A634`) explained 860..863, and the
frame-864 fix (state 6's `0x12750` node list and the driver's frame-counter
seed) explained 864/865, the frame-866 fix (`0x36870`'s case 0 restarting
its own record) explained 866, and the frame-867 fix (`0x3BDDC`'s `0x3C480`
animation start and the full `0x18714` anchor path) explained 867..869, the
frame-870 fix (the `0x35E04`/`0x3BC70` launch) explained 870..879, and the
frame-880 fix (`0x34E2C`'s reaction callback and the T-rex's `0x3E62C` leap)
explained 880..890, and the frame-891 fix (the T-rex's `+0x1C` callback
`0x3E4C4`) explained 891, and the frame-892 fix (the knockback pose's handler
`0x39CC8`) explained 892..949, and the frame-950 fix (the knockdown floor
`0x347B8`) explained 950..991, and the frame-992 fix (the T-rex's walk entry
`0x35938`) explained 992..997, and the frame-998 fix (the worshipper arrival
target `0x4AC18`) explained 998..1357, whose three transition frames all lie
in that new span, and the frame-1358 fix (the camera split arm's `0x18714`
record writes) explained 1358..1410, and the frame-1411 fix (the T-rex's
reaction-`0x20` callback `0x3D17C` and its `0x3D214`/`0x3D26C` targets)
explained 1411..1477, and the frame-1478 fix (the projectile collision step
`0x17CB0` and the hit it wakes in `0x1975C`/`0x3B464`) explained 1478..1480, and the
frame-1481 fix (`0x349C8`'s `0x34A8D` command gate) explained 1481..1545, and the
frame-1546 fix (the effects pass's worshipper fall/lie/climb, `0x49C78` cases
3..5) explained 1546..1562, and the frame-1563 fix (the effects pass's
per-entry prelude `0x4B69C`, the worshippers' trample) explained 1563..1658, and the
frame-1659 fix (game_frame's fighters' body push `0x3BB90`) explained 1659..1714; the host-timed capture is not reproducible, so indices shift while
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
demo window `[1881..3616]` (raw `4788..8409`), **1736 frames: 0 clean / 0 splice
/ 0 transition / 1730 unexplained** (6 all-black frames excluded); the first
unexplained frame is capture **1881 (raw 4788)** — 3 791 px in x 0–204, rows
137–192: below the splice row the capture's gold T-rex drops into a new
pose at f = 962 (port frame 1379, the dump's second-to-last), while the
port's stays upright; f = 962 is the run's `0x3E3A8` miss, a reaction
callback `hit_reaction_apply` skips; not derived)
(moved from 843
by the demo-pose cycle, then from 851 by the roar-timing fix, then from 858 by
the frame-858 fix, then from 859 by the frame-859 fix, then from 860 by the
frame-860 fix, then from 864 by the frame-864 fix, then from 866 by the
frame-866 fix, then from 867 by the frame-867 fix, then from 870 by the
frame-870 fix, then from 880 by the frame-880 fix, then from 891 by the
frame-891 fix, then from 892 by the frame-892 fix, then from 950 by the
frame-950 fix, then from 992 by the frame-992 fix, then from 998 by the
frame-998 fix, then from 1358 by the frame-1358 fix, then from 1411 by the
frame-1411 fix, then from 1478 by the frame-1478 fix, then from 1481 by the
frame-1481 fix, then from 1546 by the frame-1546 fix, then from 1563 by the
frame-1563 fix, then from 1659 by the frame-1659 fix, then from 1715 by the
frame-1715 fix, then from 1750 by the frame-1750 fix, then from 1763 by the
frame-1763 fix), not the state-9 hold. (Cycle 1's Amendment 5 said the fight begins at 839/28;
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
* **Named gaps still open.** The roar animation timing at f = 80; the `0x354F0` arena-wall clamp; `hit_record_x/y` (`0x18714`/`0x18788`) omitting the raw's `0x18540`/`0x18350` calls (first differs at f = 90; closed since, record §17); the camera split-arm `0x18714` write (closed since, record §24); the sibling pose handlers `0x3A588`/`0x3A6D4`/`0x3A820` and the `0x39F40`/`0x39CC8` pose family (unreached in the demo's first fight); the `0x2C3FC` voice stays a stub (RNG- and fight-state-neutral, record §3.4).
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

**Walk arrival (`afa47b3`), the demo fight's capture 859.** The type-0x20 worshipper behind the T-rex reaches its walk target at f = 87. The raw's scene/effects pass `0x49C78` handles a type-1 (walking) entry at `0x49D2F`: when `|actor+0x18 − entry+0x14|` is at most the step `[actor+0x32] >> 16` (negated for a leftward walker; signed `jg` at `0x49D79`), it calls `0x4AC38`. That zeroes `+0x34/+0x36/+0x38`, clears hflip, sets the entry back to type 0 and begins the `0xC9544[index]` stream (`0xEE02C`, sprite `0x0836`) with the hold 5.0. The port had left type 1 as a named gap, so the worshipper walked on. One function plus one case body (about 170 raw bytes).

* **Measured.** Capture 859 is now a 0-byte splice of port 503/504 at byte 90 582 (row 94), from 449 B / 159 px. The demo oracle's first unexplained is now **860 (raw 3767)**. The demo window `[860..3616]` has 2757 frames, 2751 unexplained, and the fight window `[860..1884]` has 1025 frames, 0 explained. The ratchet N is raised **859 → 860** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..858]` / 299 → **`[560..859]` / 300 / `134 clean, 162 splice, 0 transition, 2 unexplained (832, 833)`**. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 860 (f = 88) leaves 476 B / 164 px in x 86–106, rows 136–172: the same worshipper, which the capture still shows in its arrival sprite `0x0836`. In the port, `0x4AAD0`'s `0x4AB7F` gate reads bit 0 of the side-0 slot's `+0x42` and retargets the worshipper (type 8, `0xC958C[0]`). The roar reaction set that bit at f = 72, and the port never clears it. The raw clear is not derived. (Derived since: the effects pass's tail `0x4A634`; see the slot `+0x42` reset below.)

See §13 of the record `docs/superpowers/plans/2026-09-24-demo-pose-derivations.md`.

**Slot `+0x42` reset (`b915712`), the demo fight's capture 860.** The raw clears the roar's `+0x42` bit 0 in the frame that sets it. The scene/effects pass `0x49C78` ends with an unconditional `call 0x4A634` (`0x4A591`). For each fighter slot, `0x4A634` does `and al,0xfc` on `+0x42` (`0x4A6D7..0x4A6E0`: `8a 83 f2 77 10 00` / `24 fc` / `88 83 f2 77 10 00`) and zeroes the side's `0x10889E`/`0x1088B2` bytes. Before that, when bit 1 is set, the side's `0x1088A8` reaction byte draws the crowd-voice RNG: rng(3) for `0x20..0x3F`, or rng(2) plus a second rng(2) when the first is non-zero for `0x10..0x17`. The port had skipped the call as a named gap, so the bit set at f = 72 made `0x4AB7F` retarget the worshipper to type 8 at f = 88. One function (211 raw bytes); the demo's reaction bytes (`0x0B`, `0x01`) draw nothing here.

* **Measured.** Capture 860 is now a 0-byte splice of port 504/505 at byte 117 108 (row 121), from 476 B / 164 px. Captures 861..863 are explained. The demo oracle's first unexplained is now **864 (raw 3771)**. The demo window `[864..3616]` has 2753 frames, 2747 unexplained, and the fight window `[864..1884]` has 1021 frames, 0 explained. The ratchet N is raised **860 → 864** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..859]` / 300 → **`[560..863]` / 304 / `136 clean, 164 splice, 0 transition, 2 unexplained (832, 833)`**. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 864 (f = 91) matches port 508 except for 495 B / 165 px in x 0–23, rows 99–118. The capture shows a small grey figure at the left screen edge, which persists through 865/866. The port does not draw it. Its owner, an unported effect type or spawn, is not derived. (Derived since: the `0x12750` node list and the frame counter; see the grey flier below.)

See §14 of the record `docs/superpowers/plans/2026-09-24-demo-pose-derivations.md`.

**Grey flier (`7147288`), the demo fight's captures 864/865.** The grey figure is the flying creature that `0x1282C` spawns from descriptor `0xBB254` (type `0x01`, sprites `0x0281..`, effects palette `0x105FF3C`; a sprite search over every id matched `0x8281` at (−7, 99) to 1 px). Two things kept it off screen. First, state 6's reset `0x20DF4` calls `0x12750` at `0x20E33`, which self-links `0xF0AE0`/`0xF0A78` and tail-appends the eight 12-byte nodes `0xF0A80..0xF0AD4`. Type `0x01`'s cb1 `0x127C0` pops that list, and the port had skipped the call, so every spawn was refused. Second, the gate `(DS_000EF6DC & 0x3F) == 0` reads the raw's boot-relative loop counter (only writer `0x24CDB`), but the front-end driver entered state 2 with it at 0. The driver now seeds it to 886, the port's own boot-run count before state 2 (690 attract + 196 title iterations). The capture bounds that count to 886 + [−4, +17], and the flier pins it mod 64. The original's live counter is still unread, a `TODO(verify)`: the pin also depends on the modelled iteration count from state 2 to f = 91, including the un-derivable loader stall. `test_attract` cross-checks 690 and 886 in the continuous boot run. One function of 0x4D raw bytes, its call and the driver seed. Fix round 1 also made `game_frame`'s counter increment the raw's word (`0x24CDB`), no longer a dword that could carry into the separate global at `0xEF6DE`.

* **Measured.** Captures 864 and 865 are now exact (0 B; 865 is a 508/509 splice at row 69), from 495/498 B. The demo oracle's first unexplained is now **866 (raw 3773)**. The demo window `[866..3616]` has 2751 frames, 2745 unexplained, and the fight window `[866..1884]` has 1019 frames, 0 explained. The ratchet N is raised **864 → 866** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..863]` / 304 → **`[560..865]` / 306 / `137 clean, 165 splice, 0 transition, 2 unexplained (832, 833)`**. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 866 is the f = 92/93 tear. The best 509/510 splice (row 137) leaves 27 858 B / 9 891 px across both fighters and the ground, whose rows 185–199 match no port frame 508..511 at any shift in [−8, 8]. At f = 93 the port's camera steps −500 → −256, the T-rex switches `0x907E` → `0x96B5` and its shadow `0x9E04` leaves the list. The owner is not derived. (Derived since: `0x36870`'s case 0 restarted the wrong record; see below.)

See §15 of the record `docs/superpowers/plans/2026-09-24-demo-pose-derivations.md`.

**Wrong record in `0x36870` case 0 (`2137bce`), the demo fight's capture 866.** The f = 93 camera step and the T-rex's `0x96B5` were one port error. `0x36870`'s `+0x54 == 0` arm restarts the calling side's **own** record at `0xC8950[char]` and sets its `+0x4D = 0x1E`: `0x36A8C` and `0x36AA1` both read `[esp+0x10]`, which `0x368E5` loaded from `[S]`, and `[esp+0x14]` (the other record) is never read. The port restarted the other side's record. So when the raptor's stream reached its `0x36870` opcode at f = 93, the T-rex went onto the raptor's stance `0xD2136` (`0x16B5`, hflipped `0x96B5`). Its `0x18540` anchor then fell out of range to 0, which moved `AB0` −448 → 320 and the camera −500 → −256. Two operands in an already-ported function.

* **Measured.** Capture 866 is now a 0 B splice of port 509/510 (row 96), from 27 858 B / 9 891 px. The demo oracle's first unexplained is now **867 (raw 3774)**. The demo window `[867..3616]` has 2750 frames, 2744 unexplained, and the fight window `[867..1884]` has 1018 frames, 0 explained. The ratchet N is raised **866 → 867** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..865]` / 306 → **`[560..866]` / 307 / `137 clean, 166 splice, 0 transition, 2 unexplained (832, 833)`**. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 867 is a tear. The best 510/511 splice (row 124) leaves 11 184 B / 3 955 px, all in x 185–319, rows 125–194: the raptor's body and legs. The capture shows the raptor lowering out of its stance. The port holds the stance `0x16B5` from f = 93 to f = 96, while side 1's slot reads `+0x52/+0x53/+0x54` = 3/4/2 from f = 94. The owner is not derived. (Derived since: `0x3BDDC`'s unported `0x3C480` call and `0x18714`'s omitted anchor calls; see below.)

See §16 of the record `docs/superpowers/plans/2026-09-24-demo-pose-derivations.md`.

**The raptor's crouch and its anchor (`a76414d`), the demo fight's captures 867..869.** At f = 94 side 1's command `0xA0A0` reaches `0x3BDDC`, which moves the raptor to state 3/4/2. Before those stores the raw calls `0x3C480(rec, [0xC8B30 + char·4], 1.0f)` (`0x3BEF7..0x3BF05`), starting the crouch stream `0xD2274` (`0x1746`, `0x1747`, …); the port had that call as a gap, so the raptor held its stance `0x16B5`. `0x3C480`'s `0x188DC` tail then re-derives `rec+0x18` through `0x18714`, which runs `0x18540` and, on an anchor change, `0x18350` before subtracting the new `DS_00100AB0`; the port omitted both calls (a named gap since the demo-pose cycle), so the new sprite's anchor x (−11 against the stance's −4) never moved `rec+0x18`: the raptor stood 7 px left (6144 instead of 6592). One call added and one function's two calls ported (`0x18788` likewise); no size gate.

* **Measured.** Captures 867, 868 and 869 are now 0 B (510/511 and 511/512 splices, then port 512), from 11 184 / 14 505 / 14 034 B. The demo oracle's first unexplained is now **870 (raw 3777)**. The demo window `[870..3616]` has 2747 frames, 2741 unexplained, and the fight window `[870..1884]` has 1015 frames, 0 explained. The ratchet N is raised **867 → 870** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..866]` / 307 → **`[560..869]` / 310 / `138 clean, 168 splice, 0 transition, 2 unexplained (832, 833)`**. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 870 is a tear. The best 512/513 splice leaves 12 025 B / 4 306 px, 3 942 px of them in x 240–319, rows 28–197: the raptor, in a different pose from the port's `0x174E`. At f = 96 the port's raptor stream runs from `0x1747` through the opcodes `D000 5E04 0003`, `DA00 17D8 000D`, `DC00 12D8 000D`, `8E40`, `FF20`/`FF21` and `ED40 22B0 000D` to `0x174E`. The owner is not derived.

See §17 of the record `docs/superpowers/plans/2026-09-24-demo-pose-derivations.md`.
(Derived since: the raptor's launch opcode's target `0x35E04` was unregistered; see below.)

**The raptor's launch (`0x35E04`/`0x3BC70`, `0a8346b`), the demo fight's captures 870..879.** At f = 96 the raptor's attack stream `0xD2274` reaches `D000 5E04 0003` at `0xD2278`: opcode `0x10` with the inline dword `0x00035E04`. The port had nothing registered there, so the dispatch skipped the call. The raw `0x35E04` sets hold 3.0 and calls `0x3BC70`, which puts the slot in state 4/0/2 and loads the record's gravity, vertical speed and horizontal speed from the row `0x3BDDC` stored at `DS_00107D40` (the char-3 `0xBEF28` row: 23, 550, 150). The horizontal speed is negated because `slot+0x4E` = −1. So the raptor jumps, and the port kept it on the ground. Two small functions (60 B and 112 B) and one registration; no size gate. The `0x2C3FC` voice call stays a `PORT:` gap.

* **Measured.** Capture 870 is now port 513 at 0 B, and 871..879 are 0 B splices of 513/514 … 520/521 (876 is port 518), from 12 025 B at 870. The demo oracle's first unexplained is now **880 (raw 3787)**. The demo window `[880..3616]` has 2737 frames, 2731 unexplained, and the fight window `[880..1884]` has 1005 frames, 0 explained. The ratchet N is raised **870 → 880** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..869]` / 310 → **`[560..879]` / 320 / `140 clean, 176 splice, 0 transition, 2 unexplained (832, 833)`**. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 880 is a tear. The best 521/522 splice (row 101) leaves 6 073 px, all in x 0–149, rows 118–199: the T-rex, which the capture shows lowered while the port draws it upright (`0x8F35`). The owner is not derived.

See §18 of the record `docs/superpowers/plans/2026-09-24-demo-pose-derivations.md`.
(Derived since: `0x34E2C`'s reaction callback was never called; see below.)

**The T-rex's reaction-0x2B leap (`0x3E62C`/`0x3E524`/`0x3E4E4`, `cfff063`), the demo fight's captures 880..890.** At f = 105 the T-rex's `0x3CF38` chain drives reaction `0x2B` (hit-scan index 5, `word[0xC61C8]`). The `0x34E2C` entry for it (`0xA3884`) has no stream, only the callback `0x3E62C`, and the port skipped `0x34E2C`'s callback call at `0x35045`. So the T-rex stood up where the capture shows it lowering. The raw `0x3E62C` starts the `0xE7BDE` stream (`0x11B3`…) through `0x3C4CC` at hold 3.0, re-anchors x from the `slot+0x2C` it read first, and puts the slot in state 9/7/2 with `+0x57 = 2`. It also arms the `+0x0C` callback `0x3E524`, which `0x3531C` case 7 runs each frame (rise, fall, landing). The stream's `D500 E4E4 0003` reaches `0x3E4E4`, the leap: vertical speed `0x320`, gravity `0x23`. The fix is the callback call, four small functions and three registrations; no size gate.

* **Measured.** Captures 880..890 are now 0 px (splices of 521/522 … 529/530; 883 and 890 are ports 524 and 530), from 6 073 px at 880. The demo oracle's first unexplained is now **891 (raw 3798)**. The demo window `[891..3616]` has 2726 frames, 2720 unexplained, and the fight window `[891..1884]` has 994 frames, 0 explained. The ratchet N is raised **880 → 891** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..879]` / 320 → **`[560..890]` / 331 / `142 clean, 185 splice, 0 transition, 2 unexplained (832, 833)`**. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 891 is a tear. The best 530/531 splice (row 29) leaves 2 188 px in x 100–287: the capture shows the raptor struck in mid-air by the leaping T-rex (a hit spray, a different pose), and a worshipper; 892 onwards differ across the frame. (Derived since, below: `0x3E62C`'s `+0x1C` callback `0x3E4C4`, which `0x193B0` calls at `0x19505`. This bullet first named the `0x1975C` collision step and said the two callbacks were only called from the think chain. Both claims were wrong.)
* **Known later gaps (unregistered code targets, skipped).** The animation-opcode target **`0x35938`** is first hit at f = 173 on the raptor (50 hits in the run), and `0x3640C` at f = 712. The reaction callbacks `0x3D17C` (f = 350), `0x3ECF8` (f = 688) and `0x3C0A4` (f = 832) are also unregistered. None fires before capture 891.

See §19 of the record `docs/superpowers/plans/2026-09-24-demo-pose-derivations.md`.

**The T-rex's `+0x1C` callback (`0x3E4C4`, `6a48972`), the demo fight's capture 891.** `0x3E62C` stores two callbacks, `0x3E484` in `slot+0x18` and `0x3E4C4` in `slot+0x1C`. Their callers are `0x1958C`'s `0x19020` hook (`0x1903F`) and its winner body `0x193B0` (`0x19505`, EAX = side). At f = 114 the winner body runs for the T-rex, and the port's `fn_resolve` returned NULL for `0x3E4C4`, so it skipped the reaction and the raptor was not struck. `0x3E4C4` (31 B) applies `0x3B714(slot[1-side], slot[side])`, the call the `+0x1C == 0` arm makes at `0x19526`, which is already ported. `0x3E484` stays a `PORT:` gap: it needs the unported `0x18C14`, and its closure is 1 408 B in 6 functions. Evaluated on the port's state, it gives the port's `DS_00100AF8` zero-ness on every frame the hook is set (f = 106..114), so it is inert in this run.

* **Measured.** Capture 891 is now 0 px (the 530/531 splice). The demo oracle's first unexplained is now **892 (raw 3799)**. The demo window `[892..3616]` has 2725 frames, 2719 unexplained, and the fight window `[892..1884]` has 993 frames, 0 explained. The ratchet N is raised **891 → 892** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..890]` / 331 → **`[560..891]` / 332 / `142 clean, 186 splice, 0 transition, 2 unexplained (832, 833)`**. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 892 is a tear. The best 531/532 splice (row 56) leaves 35 921 px across rows 56–199. From it on, the capture's whole scene sits lower than the port's: the background matches at dy = +3, +5, +7 and +11 px at captures 892, 893, 894 and 896. The owner is not derived. The candidates are the camera's vertical follow after the hit and the struck raptor's reaction state.
* **Collision step, measured.** `DS_00100AD0` still stays 0 because `0x1975C`'s `0x17CB0` → `0x176CC` is unported. Its genuinely new closure is `0x17CB0`, `0x176CC`, `0x17BC8` and the §7.12 gap `0x3B938`: 1 070 B in 4 functions. Its other callees (`0x140E4`, `0x15C30`, `0x16DA4`, `0x17EEC`, `0x181D0`, …) are already ported.

See §19.6 of the record.
(Derived since: the reaction's knockback pose handler `0x39CC8` was unregistered; see below.)

**The knockback pose's handler (`0x39CC8`/`0x39B30`, `a51685d`), the demo fight's captures 892..949.** At f = 114 the reaction `0x3B714` → `0x3AAFC` finds the struck raptor airborne and calls the pose setter `0x39F40` (`0x3AC89`: −80, `0x46`, `0x0C`, `0x14`), which stores the per-frame handler `0x39CC8` in `slot+0x10` (`0x39F8F`). `0x3531C` case 10 calls it every frame (`0x354E2`), and the port resolved it to NULL. The raw handler (561 B, no Ghidra function) is a `+0x58` machine: it arms, launches the raptor through `0x39B30` (gravity `0x39AC8(0x46, 12)` = 62, vertical 62 · 12 = 744, horizontal −80 · 64 / 32 = −160, negated while unflipped), re-times the fall to the ground, lands it (the `0xBEDB0[char]` stream, the `0xBB1DC` dust) and clears `+0x54`. The camera follows the higher fighter's y (`0x12DA8`), so in the capture it climbs one 0x100 step a frame from f = 115; the port's raptor hung at the hit and the camera barely moved. That was the "scene drops" of capture 892. The closure is `0x39CC8`, `0x39B30`, `0x35050`, `0x39AC8` and `0x39B14`: 1 152 B in 5 functions, inside the size gate. The `0x2C3FC(0x6C)` voice stays a `PORT:` gap.

* **Measured.** Captures 892..949 are now explained (clean or splice). The demo oracle's first unexplained is now **950 (raw 3857)**. The demo window `[950..3616]` has 2667 frames, 2661 unexplained, and the fight window `[950..1884]` has 935 frames, 0 explained. The ratchet N is raised **892 → 950** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..891]` / 332 → **`[560..949]` / 390 / `150 clean, 236 splice, 0 transition, 2 unexplained (832, 833)`**. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 950 is a tear. The best 581/582 splice (row 122) leaves 1 731 px in x 132–264, rows 161–199: the capture's raptor stays lying where it landed, and the port's gets up. At f = 165 the landing stream `0xD2ADA` reaches `D500 47B8 0003`, an opcode-`0x15` target `0x347B8` that the port has not registered, so it was skipped. With `DS_00104B00` = 3 the raw `0x347B8` puts the slot in state 9/`0x0B`/0 and starts a per-character stream (table `0x34780`; corrected since: `0x34780` or `0x3479C`, chosen by `0x340BC` at `0x3485F`). That is the candidate owner; it is not derived here. (Derived since, below.)
* **Known later gaps (unregistered code targets, skipped).** `0x35938` (now first at f = 201, 53 hits), `0x4AC18` (f = 206) and `0x370F0` (f = 291). The reaction callbacks `0x3D17C`/`0x3ECF8`/`0x3C0A4` no longer miss in this run.

See §20 of the record.

**The knockdown floor (`0x347B8`/`0x346F8`, `3fee8d0`), the demo fight's captures 950..991.** At f = 165 the landed raptor's stream `0xD2ADA` reaches `D500 47B8 0003` at `0xD2B00`, and the port's `fn_resolve(0x347B8)` returned NULL, so the dispatcher walked on into the ids that follow and the raptor got up. The raw `0x347B8` (448 B, no Ghidra function; the dword occurs 53 times in the streams) runs `0x39A10(rec, 0x29A)`, `0x3C148`, `0x3C16C`, sets state 9/`0x0B`/0 and clears `+0x43` bits 0..1, then asks the stun gate `0x340BC` (`0x3485F`): on 1 it runs the stun start `0x34168` (the `0xBDBC8[char]` actor, `+0x8C` = `0x4B0`) and takes `0x34780[char]`, on 0 `0x3479C[char]`, at hold 3.0 (in game mode 7 it may freeze the side instead). The demo raptor fails the gate (`+0x63` = 1, `+0x5A` 22 against 9), so it lies on `0xD28FC`, a 20-pass loop, until that stream's second opcode-`0x15` target `0x346F8` (the get-up: `+0x76` = `word[0xBDBE6]` + 1, then `0x36870`) at f = 206. The closure is `0x347B8`, `0x340BC`, `0x34168` and `0x346F8`: 1 029 B in 4 functions, all callees already ported. The frame-892 review's minors are folded in: the slot `+0x14` callback now takes the slot at all three raw call sites (`0x19537`, `0x3514C`, `0x350B8`), and its `PORT:` note names the raw writers' target `0x29D04`.

* **Measured.** Captures 950..991 are now explained (clean or splice). The demo oracle's first unexplained is now **992 (raw 3899)**. The demo window `[992..3616]` has 2625 frames, 2619 unexplained, and the fight window `[992..1884]` has 893 frames, 0 explained. The ratchet N is raised **950 → 992** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..949]` / 390 → **`[560..991]` / 432 / `170 clean, 258 splice, 0 transition, 2 unexplained (832, 833)`**. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 991 is a clean splice; 992 is a tear. The best 617/618 splice (row 134) leaves 6 342 px in x 64–319, rows 134–192, 993 leaves 13 800 px and from 994 the residual is whole-frame. The capture's background sits 1 px right of the port's at 991/992 and 1–2 px left from 994: a horizontal camera and fighter-position divergence, not the raptor's pose. Port frame 617 is f = 201, the first miss after the fix: the T-rex's stream reaches the unregistered animation-opcode target `0x35938`. That is the candidate owner; it is not derived here.
* **Known later gaps (unregistered code targets, skipped).** `0x35938` (first at f = 201, 61 hits), `0x4AC18` (f = 206, 302, 362) and `0x3640C` (f = 598). `0x370F0` is still unregistered; it is not reached (no longer misses) in this run. `0x347B8`'s stun stream target `0x34530` is unregistered but not reached.

See §21 of the record.
(Derived since: the T-rex's walk entry `0x35938` was unregistered; see below.)

**The walk entry (`0x35938`, `ff38dcc`), the demo fight's captures 992..997.** At f = 201 the T-rex's stream (state `0x0E`) reaches `D500 5938 0003` at `0xE6EE8`, and the port's `fn_resolve(0x35938)` returned NULL, so the dispatcher walked on into the ids that follow and `D500 7068` (`0x36870`), a loop that kept the T-rex in place while the capture's walked forward and the camera followed. The raw `0x35938` (166 B, no Ghidra function; the dword occurs 14 times, two per character) sets state 1/0 (8 when `+0x54` is 4, keeping `+0x53`), sets `rec+0x29` bit 3 and seeks `rec+8` through `0x2BCF4` to the literal sprite id of `0x35C1C`'s `+0x43`-selected frame table (`0xC8AE0`/`0xC8A68`) at the record's signed frame `rec+0x52`, then clears the frame, speed and hold, sets the step `rec+0x58` = 1 and `rec+0x28 |= 0x804`. The state-1 handler `0x359E0` then walks the T-rex. One function, its callee already ported. The frame-950 review's minors are folded in: the `0x347B8` dword is at `0xD2B02`, the `0x347B8`/`0x346F8` wrappers "do not read EDX", and `check_knockdown_floor` asserts the stun spawn's ECX layer `0xFF`.

* **Measured.** Captures 992..997 are now explained (clean or splice). The demo oracle's first unexplained is now **998 (raw 3905)**. The demo window `[998..3616]` has 2619 frames, 2613 unexplained, and the fight window `[998..1884]` has 887 frames, 0 explained. The ratchet N is raised **992 → 998** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..991]` / 432 → **`[560..997]` / 438 / `172 clean, 262 splice, 0 transition, 2 unexplained (832, 833)`**. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 997 is a splice; 998's best 622/623 splice (row 107) leaves 259 px, all in the left-edge worshipper (x < 40), and 999..1006 leave 481–651 px there: the capture's worshipper turns and walks while the port's keeps cheering. Port frame 622 is f = 206, the first miss after the fix: a pool record's stream (`rec+8` `0xEE0A0`) reaches the unregistered animation-opcode target `0x4AC18` (24 data sites in `0xEE09E..0xEF62E`; it re-animates the `rec+0x14` effects-list entry through `0x4AC38`). That is the candidate owner; it is not derived here. From capture 1007 (f = 214) a second whole-frame divergence joins (called the raptor's leap here, corrected below to the T-rex's).
* **Known later gaps (unregistered code targets, skipped; a whole-run `fn_resolve` probe).** `0x4AC18` (first f = 206, 8 hits), `0x3640C` (f = 304), `0x3C0A4` (f = 333), `0x14EF8` (f = 400) and `0x3A820` (first f = 473, 491 hits). `0x370F0` is still unregistered; it is not reached (no longer misses) in this run. `0x34530` is unregistered and not reached.

See §22 of the record.
(Derived since: the worshipper arrival target `0x4AC18` was unregistered; see below.)

**The worshipper arrival target (`0x4AC18`, `b5a48a0`), the demo fight's captures 998..1357.** At f = 206 the left-edge worshipper's cheer stream (its fight-effect entry in type 8) reaches `D500 AC18 0004` at `0xEE09C`, and the port's `fn_resolve(0x4AC18)` returned NULL, so the dispatcher walked on into the next cheer stream and the worshipper kept cheering while the capture's turned and walked. The raw `0x4AC18` (29 B, no Ghidra function; the dword occurs 24 times in `0xEE09E..0xEF62E`) calls the already-ported arrival `0x4AC38` for the actor's `+0x14` entry with the index `(u32)(u8)rec+0x48 − 0x20`: the entry returns to type 0 and its actor begins the `0xC9544[index]` stream at the hold 5.0, after which the type-0 handler `0x4AAD0` walks it. One function, its callee already ported.

* **Measured.** Captures 998..1357 are now explained (clean, splice or transition), including the second divergence from 1007. That one is not the raptor's leap, as the frame-992 characterisation said, but the T-rex's (side 0). With the fix, the port makes 3 RNG draws at f = 207 instead of 1 (the worshipper's two `rng(0x1200)` in `0x4B144`). Both fighters' states stay identical until the next draw at f = 214, which is exactly the capture-1007 frame. There the T-rex leaves state 1 (→ 0 → 5 → `09/07/02`) in the fixed port and stays in 1 in the unfixed one, while side 1 is the same in both builds. The dumped port frames are identical through 622, and the first difference is frame 623. The demo oracle's first unexplained is now **1358 (raw 4265)**. The demo window `[1358..3616]` has 2259 frames, 2253 unexplained, and the fight window `[1358..1884]` has 527 frames, 0 explained. The ratchet N is raised **998 → 1358** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..997]` / 438 → **`[560..1357]` / 798 / `307 clean, 484 splice, 3 transition, 2 unexplained (832, 833)`**. The three transition frames all lie in the new span; port frames 0..622 are byte-identical to before. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 1357 is a splice; 1358's best 930/931 splice (row 39) leaves 2 743 px in the T-rex at the right edge, growing to the whole frame from 1362. The capture's T-rex comes down from its leap and stands by 1361, while the port's (side 0, state 4/8/2 from f = 508) stays in the air until f = 521. No `fn_resolve` miss other than the stub `0x5D812` falls before f = 559, so it is not an unregistered target; no candidate owner is named. (Derived since: the T-rex was misplaced in x, not late to land; the camera split arm's `0x18714` writes were skipped; see below.)
* **Known later gaps (unregistered code targets, skipped; a whole-run `fn_resolve` probe).** `0x3D17C` (first f = 559, 3 hits), `0x14EF8` (f = 582), `0x3640C` (f = 741) and `0x3A588` (first f = 784, 180 hits). `0x3C0A4` and `0x3A820` are no longer reached. `0x370F0` is still unregistered; it is not reached (no longer misses) in this run. `0x34530` is unregistered and not reached.

See §23 of the record.

**The camera split arm's `0x18714` writes (`c78dc97`), the demo fight's captures 1358..1410.** At the end of f = 514 and f = 515 the fighters' `+0x34` latches are 20 526 apart (T-rex 26 674, raptor 6 148), more than `word[0x9AF28]` = `0x5000`. The mode-1 camera `0x12E3C` then pulls the outward-moving T-rex back to its `+0x38` latch (the previous frame's `+0x34`), storing `+0x34`/`+0x2C`, and rewrites its record's `+0x18` through `0x18714` (`0x12F22`/`0x12F29`; slot a's twin at `0x12F02`/`0x12F09`). The port stored only the slot fields (a named gap since the demo-fight cycle), so the next latch rebuilt `+0x2C` from the unclamped record and the T-rex ran two frames' worth of x (436) further right. The capture draws the same sprite 4 px further left from 1358 on; it was never a late landing. `0x18714` was already ported (`hit_record_x`), so the fix is two calls; new `check_camera_split`.

* **Measured.** Captures 1358..1410 are now explained; port frames 0..930 are byte-identical to before, and 931 (f = 515) is the first that differs. The demo oracle's first unexplained is now **1411 (raw 4318)**. The demo window `[1411..3616]` has 2206 frames, 2200 unexplained, and the fight window `[1411..1884]` has 474 frames, 0 explained. The ratchet N is raised **1358 → 1411** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..1357]` / 798 → **`[560..1410]` / 851 / `337 clean, 507 splice, 3 transition, 2 unexplained (832, 833)`**; the three transition frames are the same as before. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 1410 is a splice; 1411's best 975/976 splice leaves 6 363 px in rows 90–192, in both fighters, growing to 13 407 px by 1416. Port frame 976 is f = 560. At f = 559 the T-rex (state 9/0/0) takes reaction `0x20`, whose `(char 0, 0x20)` entry at `0xA37A8` has no stream and the callback `0x3D17C`. That callback is still unregistered (the run's first `fn_resolve` miss after the stub), so the port only stores `+0x5F` and the T-rex is back in its stance at f = 560. `0x3D17C` is the candidate owner; not derived. (Derived since: it is the owner; see below.)
* **Known later gaps (unregistered code targets, skipped; a whole-run `fn_resolve` probe).** `0x3D17C` (first f = 559, 3 hits) and `0x3A588` (first f = 807, 157 hits). `0x14EF8` and `0x3640C` no longer miss in this run. `0x370F0` and `0x34530` are unregistered and not reached.

See §24 of the record.

**The T-rex's reaction-`0x20` breath (`4065c1d`), the demo fight's captures 1411..1477.** At f = 559 `0x34E2C` applies reaction `0x20` to the T-rex; its `(char 0, 0x20)` entry `0xA37A8` holds only the callback `0x3D17C`, which the port skipped as unregistered, so the T-rex fell back into its stance. The raw `0x3D17C` (unless the slot's `+0x08` already holds a projectile) starts the breath stream `0xE84C8` at hold 3.0 through `0x3C4CC` (state `0xB/6/0`), clears the slot's `+0x0C/+0x18/+0x1C` callbacks, moves `+0x5F` to `+0x64` and stores `0x100` in the word `0x1080AC[side]`. The stream's `D100` target `0x3D214` spawns the emitter `0xBB27C` as the record's child, and the emitter's `D100` target `0x3D26C` spawns the projectile `0xBB268` (the purple ring) beside the record into the slot's `+0x08`, at the speed `∓0x1080AC[side]`. Three functions (386 B) whose callees were already ported; new `check_trex_breath`. Two existing reaction-`0x20` checks now seed a live projectile so their assertions stay unchanged.

* **Measured.** Captures 1411..1477 are now explained; port frames 0..975 are byte-identical to before, and 976 (f = 560) is the first that differs. The demo oracle's first unexplained is now **1478 (raw 4385)**. The demo window `[1478..3616]` has 2139 frames, 2133 unexplained, and the fight window `[1478..1884]` has 407 frames, 0 explained. The ratchet N is raised **1411 → 1478** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..1410]` / 851 → **`[560..1477]` / 918 / `382 clean, 529 splice, 3 transition, 2 unexplained (832, 833)`**; the three transition frames are the same as before. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 1477 is a splice; 1478's best 1033/1034 splice (row 103) leaves 7 749 px in rows 103–195, growing to the whole frame from 1480. Port frame 1034 is f = 618. The capture's ring reaches the raptor at the left edge and bursts as the raptor recoils; the port's ring (slot 0's `+0x08`, −256 per frame, x 10 330 at f = 618 against the raptor's 10 052) flies on untested. No `fn_resolve` miss falls in f = 560..624. The candidate owner is the unported collision step `0x17CB0` (`0x1975C`'s first call, `0x19763`), whose `0x176CC`/`0x17BC8` test the slots' `+0x08` projectiles; not derived. The T-rex at the right edge also differs (reaction `0x08` at f = 619); not separated. (Derived since: `0x17CB0` is the owner; see below.)
* **Known later gaps (unregistered code targets, skipped; a whole-run `fn_resolve` probe).** `0x14814` (f = 625, the `(char 3, 0x22)` reaction callback) and `0x3ECF8` (f = 888, the `(char 0, 0x2C)` callback). `0x3D17C` no longer misses, and `0x3A588` is no longer reached. `0x370F0` and `0x34530` are unregistered and not reached.

See §25 of the record.

**The projectile collision step (`3d64c61`), the demo fight's captures 1478..1480.** `0x1975C`'s first call `0x17CB0` was unported, so `DS_00100AD0/AD4` (the per-thrower projectile overlap counts) stayed 0, the think step never ran and the T-rex's breath ring flew through the raptor. The raw `0x17CB0` fills the `0x100BD3` plane, zeroes `AD0/AD4`, and tests either two live projectiles against each other (`0x17BC8`: burst side 0's through `0x3B938`, kill side 1's) or each side's projectile against the other fighter (`0x176CC`: the `0x170A0` shape with the projectile as a `0x20` box and `0x16DA4`'s mode-0 row `0xA173C`), writing the overlap count into `AD0[side]`. At f = 617 it gives `AD0[0]` = 17, and `0x1975C` applies the hit to the raptor through `0x3B464` (the `0x39834` pose driver and the `0x3A95C` stagger) and bursts the ring (`0x3B938`). New: `0x17CB0`, `0x176CC`, `0x17BC8`, `0x3B938`, `0x3A95C` (1 192 B) and `0x16DA4`'s mode arms; `0x3B464`'s §7.12 gaps are wired except `0x235C4` (projectile `+0x48` = 8, not in the demo). `check_think_chain` now asserts the raw zeroing; new `check_projectile_step`.

* **Measured.** Captures 1478..1480 are now explained; port frames 0..1033 are byte-identical to before, and 1034 (f = 618) is the first that differs. The demo oracle's first unexplained is now **1481 (raw 4388)**. The demo window `[1481..3616]` has 2136 frames, 2130 unexplained, and the fight window `[1481..1884]` has 404 frames, 0 explained. The ratchet N is raised **1478 → 1481** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..1477]` / 918 → **`[560..1480]` / 921 / `383 clean, 531 splice, 3 transition, 2 unexplained (832, 833)`**; the three transition frames are the same as before. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 1481's best 1035/1036 splice leaves 3 626 px in rows 86–192, almost all in the two fighters; against port 1036 (f = 620) the capture's raptor sits 1 px left and its T-rex 1 px right, with a slightly different T-rex pose; 1482 leaves 3 615 px. Candidates: the struck raptor's knock-back or the T-rex's reaction `0x08` at f = 619; not derived.
* **Known later gaps (unregistered code targets, skipped; a whole-run `fn_resolve` probe).** `0x3C048` (f = 652, the reaction-`0x3D` callback of every character) and `0x3E3A8` (f = 920, the `(char 0, 0x2A)` callback). `0x14814` and `0x3ECF8` no longer miss. `0x370F0` and `0x34530` are unregistered and not reached.

See §26 of the record.

**The `0x349C8` command gate (`219691e`), the demo fight's captures 1481..1545.** `0x349C8` (the `+0x52` == 0 handler) returns at `0x34A8D` (`0F 85 78 00 00 00  jne 0x34B0B`) when the side's command word has a bit in both `(cmd>>8)&3` and `(cmd>>8)&0xC`; the `0x3BDDC` consume, the `0x4000` arm and `0x35838` all sit behind that gate. The port skipped only the consume. At f = 619 the T-rex's word `0x6F6F` took the port's `0x4000` arm, which restarted its record on `0xC8978[0]` (sprite `0x0F02` → `0x0F34`), and the reaction-`0x08` hit's `0x18B04` then re-derived its record x from the new sprite's anchor, 64 units (1 px) left of the raw's. A one-line gate, no new function; new case I in `check_deep_callees`.

* **Measured.** Captures 1481..1545 are now explained; port frames 0..1035 are byte-identical to before, and 1036 (the f = 619 state) is the first that differs. The demo oracle's first unexplained is now **1546 (raw 4453)**. The demo window `[1546..3616]` has 2071 frames, 2065 unexplained, and the fight window `[1546..1884]` has 339 frames, 0 explained. The ratchet N is raised **1481 → 1546** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..1480]` / 921 → **`[560..1545]` / 986 / `410 clean, 569 splice, 3 transition, 2 unexplained (832, 833)`**; the three transition frames are the same as before. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 1546's best 1091/1092 splice (row 67) leaves 148 px in x 221–234, rows 180–199: a small teal-clad worshipper crouches in the capture and not in the port; 1547 leaves 297 px and 1549 372 px. The fighters, the burst and the background match. Port 1091 is the f ≈ 674 state; not derived.
* **Known later gaps (unregistered code targets, skipped; a whole-run `fn_resolve` probe).** `0x3C0A4` (f = 850), `0x3ECF8` (f = 887) and `0x3E3A8` (f = 896). `0x3C048` no longer misses. `0x370F0` and `0x34530` are unregistered and not reached.

See §27 of the record.

**The worshippers' fall, lie and climb (`27c95c0`), the demo fight's captures 1546..1562.** The effects pass `0x49C78`'s type-3 handler `0x49DB3` was ported only as its `DS_000BD898` gate and its `rng(0x3C)` draw. The raw sets the actor's `+0x2C` from its y (`0x496AC`) every frame and, once the y is at or below the zero-extended word `0xBD898` (`0x400`), lands it: hflip OR-ed in when `0x2BE1C` > 0, the `0xC9634[si]` crouch stream at 2.0, the velocities zeroed, a `rng(0x3C) + 0x3C` lie timer, `+0x1C` bit 7 and type 4. The port kept the entry at type 3 on its upright stream and drew `rng(0x3C)` on every later frame, so the two side-1 worshippers `0x4B5A8` scared at f ≈ 650 (their slot in state 7/2) never crouched (f = 675/676). The fix ports case 3 whole and the cycle it hands off to, case 4 (`0x49E5A`, the signed lie timer and the `0xC95EC` rise) and case 5 (`0x49EC0`, the climb back to type 0), with no new callee; `0x496AC` compares signed and `fight_dust_clamp` now does too. New `check_effects_fall` in `test_fight.c`; 31 mutations each fail it.

* **Measured.** Captures 1546..1562 are now explained; port frames 0..1091 are byte-identical to before, and 1092 (the f = 675 state) is the first that differs. The demo oracle's first unexplained is now **1563 (raw 4470)**. The demo window `[1563..3616]` has 2054 frames, 2048 unexplained, and the fight window `[1563..1884]` has 322 frames, 0 explained. The ratchet N is raised **1546 → 1563** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..1545]` / 986 → **`[560..1562]` / 1003 / `413 clean, 583 splice, 3 transition, 2 unexplained (832, 833)`**; the three transition frames are the same as before. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 1563's best 1106/1107 splice (row 154) leaves 514 px in x 37–232, rows 158–191, in two places: x 200–232, a worshipper beside the right-hand fighter in a different pose, and x 37–70, rows ≈ 180–191, a dark blob under the left fighter. 1562 splices at 0 px. Port 1106 is the f ≈ 689 state; not derived. The landed worshippers now carry `+0x1C` bit 7, which gates the unported per-entry prelude `0x4B69C` (a `0x17D30` collision against the fighters); that is a candidate, not a derivation.
* **Known later gaps (unregistered code targets, skipped; a whole-run `fn_resolve` probe).** `0x14F50` (f = 865 and 871). `0x3C0A4`, `0x3ECF8` and `0x3E3A8` no longer miss (the run changed from f = 675). `0x370F0` and `0x34530` are unregistered and not reached.

See §28 of the record.

**The worshippers' trample (`cc38a38`), the demo fight's captures 1563..1658.** The effects pass `0x49C78` calls its per-entry prelude `0x4B69C(entry, si)` before the type dispatch (`0x49CFE`); the port did not. An entry with `+0x1C` bit 7 (a worshipper lying after case 3's landing) tests its actor's pset point against both fighters' `0x100AC8` boxes (`0x17D30`, which runs `0x1790C` per side: the `0x176CC` projectile-test shape with the point as a `0x30`-wide, `0x38`-tall box and the `0xA1740` overlap row). On a hit that `0x4B788` does not claim for a grab (the fighter's slot `+0x5F` is not its grab move `0xC97F2[ch]`), `0x4B470` tramples it: the `0xC9604[si]` tumble stream, a shadow actor from `0xBB920[si]`, `+0x34` = ±`0x80`, `+0x36` = `0x240`, type 6; case 6 (`0x49F11`) flies it and lands it on `0xC973C[si]` as type 8. At f = 689 the gold fighter's tail puts both landed side-1 worshippers inside side 0's box. The fix ports `0x17D30`/`0x1790C` in `camera.c` (every callee was already ported), `0x4B69C`, `0x4B788`'s gates, `0x4B470`, case 6 and case 8's gate in `fight.c`; the grab arm, the eighth-hit tail (`0x4BD98`/`0x4CB18`) and case 8's held body are named gaps that no probe reached. New `check_point_trample` in `test_fight.c`; 51 mutations each fail it.

* **Measured.** Captures 1563..1658 are now explained; port frames 0..1105 are byte-identical to before, and 1106 (the f = 689 state) is the first that differs. The demo oracle's first unexplained is now **1659 (raw 4566)**. The demo window `[1659..3616]` has 1958 frames, 1952 unexplained, and the fight window `[1659..1884]` has 226 frames, 0 explained. The ratchet N is raised **1563 → 1659** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..1562]` / 1003 → **`[560..1658]` / 1099 / `447 clean, 645 splice, 3 transition, 2 unexplained (832, 833)`**; the three transition frames are the same as before. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 1659's best 1188/1189 splice (row 135) leaves 57 px in x 111–124, rows 135–142, on the top-left edge of the dark ring; from 1660 the gold fighter's leap and the camera y diverge (5 279, 10 954, 42 868 px). Port 1188 is the f ≈ 771 state; not derived.
* **Known later gaps (unregistered code targets, skipped; a whole-run `fn_resolve` probe).** `0x4AC80` (f = 820 and 841; a worshipper stream callback with no code xref) and `0x3BF70` (f = 850). `0x14F50` no longer misses. `0x370F0` and `0x34530` are unregistered and not reached.

See §29 of the record.

**The fighters' body push (`1268371`), the demo fight's captures 1659..1714.** game_frame's `DS_00104B15` tail calls `0x3BB90` at `0x2541D`, before `0x12D48`; the port had only a `PORT:` note (`port/spec/game_flow.md`'s "cycle 2 landed `0x3BB90`" was stale). `0x3BB90` latches both slots and, when the two latched points are closer than the sum of the characters' `0xBEEF8` widths (halved for `+0x54` = 2), takes `0x4FB20`'s distance estimate (max + min/4 + min/8) and pushes the sides apart by the penetration: `0x3BAEC` runs `0x3B9D8` per side, which moves the side half of it away from the other through `0x1883C` (or the other side at the `0x7C00` wall, `0x3B8D8`) and zeroes a speed that does not point away when `+0x54` is 2. The fighters first overlap at f = 772, the T-rex's leap onto the raptor: capture 1659's residual is the T-rex's claw 1 px left in the port (and the raptor 1 px right), not the dark ring. The fix ports the five functions in `fighter.c` (every callee was already ported) and wires the call in `flow.c`. New `check_body_push` in `test_fight.c` and a wiring assertion in `check_game_frame_tail`; 46 of 47 mutations fail them, and the 47th (`0x4FB20`'s |dy| gate) is equivalent in the reachable domain.

* **Measured.** Captures 1659..1714 are now explained; port frames 0..1188 are byte-identical to before, and 1189 (the f = 772 state) is the first that differs. The demo oracle's first unexplained is now **1715 (raw 4622)**. The demo window `[1715..3616]` has 1902 frames, 1896 unexplained, and the fight window `[1715..1884]` has 170 frames, 0 explained. The ratchet N is raised **1659 → 1715** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..1658]` / 1099 → **`[560..1714]` / 1155 / `455 clean, 693 splice, 3 transition, 2 unexplained (832, 833)`**; the three transition frames are the same as before. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 1715's best 1236/1237 splice (row 144) leaves 383 px, almost all in x 40–99, rows 144–190: a standing worshipper at x ≈ 45–60 in the capture is at x ≈ 75–88 in port 1237 and gone from 1238. Port 1237 is the f = 820 state, the run's first unregistered `0x4AC80` call (a worshipper stream callback); not derived.
* **Known later gaps (unregistered code targets, skipped; a whole-run `fn_resolve` probe).** `0x4AC80` (f = 820 and 841) and `0x3C0A4` (f = 850, no code xref). `0x3BF70` no longer misses.

See §30 of the record.

**The worshipper landing target (`2287114`), the demo fight's captures 1715..1749.** The six worshipper landing streams end in `D500 AC80 0004` (opcode `0x15`, mode `0x4000`), and the port had not registered `0x4AC80`, so `anim_indirect` skipped it. At f = 820 and 841 it ends each trampled side-1 worshipper's type-8 landing with the climb: the `0xC95EC` rising stream at 3.0, the record's `+0x38` = `0x40` and `+0x34` = ±`0x40` by the fighter's `+0x28` bit `0x4000`, type 5 and `+0x1C &= 0x3F`; case 5 then stands it up. The port left both on the landing stream. `0x4AC80`'s other arms (the `DS_001088C5` walk or hold beside the `DS_00108868` record, the `DS_00108864` release, and the mode 8/9/`0x17` hold through `0x4B3F0`/`0x4B430` with EBX = 1) are ported too but not reached; `0x4B3F0`/`0x4B430` now take the raw's EBX flag for `+0x55` (the port had hard-coded 0). New `check_worshipper_landing` in `test_fight.c`; 58 of 59 mutations fail it, and the 59th (`0x4AD4F` at equality) is equivalent in the reachable domain. Task 20's parked minors are tidied in `71c61b2`.

* **Measured.** Captures 1715..1749 are now explained; port frames 0..1236 are byte-identical to before, and 1237 (the f = 820 state) is the first that differs. The demo oracle's first unexplained is now **1750 (raw 4657)**. The demo window `[1750..3616]` has 1867 frames, 1861 unexplained, and the fight window `[1750..1884]` has 135 frames, 0 explained. The ratchet N is raised **1715 → 1750** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..1714]` / 1155 → **`[560..1749]` / 1190 / `468 clean, 715 splice, 3 transition, 2 unexplained (832, 833)`**; the three transition frames are the same as before. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 1750's best 1266/1267 splice (row 153) leaves 5 544 px in x 0–286, rows 153–199 (1751 and 1752 leave 10 703 and 11 165): below the split the capture's gold T-rex keeps port 1266's place and upright pose, while port 1267 (f = 850) has it further left in another pose, and the camera matches. f = 850 is the run's `0x3C0A4` miss; not derived.
* **Known later gaps (unregistered code targets, skipped; a whole-run `fn_resolve` probe).** `0x3C0A4` (f = 850, no code xref), `0x14F50` (f = 929) and `0x3A820` (f = 962/963). `0x4AC80` no longer misses.

See §31 of the record.

**The reaction callbacks `0x3C0A4`/`0x3BF70` (`c7320b2`), the demo fight's captures 1750..1762.** `0x34E2C`'s `(char, reaction)` table holds `0x3C0A4` as reaction `0x3E`'s callback and `0x3BF70` as reaction `0x3F`'s (the T-rex's records `0xA3A00`/`0xA3A14`, one pair per character at stride `0x500`; no code reference to `0x3C0A4`). The port had not registered `0x3C0A4`, so the `0x35045` call skipped it. At f = 850 the T-rex takes reaction `0x3E`: `0x3BF70`, the forced attack, starts its `0xC8B30` attack stream at hold 2.0 through `0x3C4CC` with the `0xBEFA0` row in `DS_00107D40` and state 3/4/2, and `0x3C0A4` then turns the slot's `+0x4E` facing the other way. New `check_reaction_attack` in `test_fight.c`; 36 of 37 mutations fail it, and the 37th (the `0x3BF70` wrapper passing the slot's own record) is equivalent at the only call site. Task 21's parked test minors are tidied in `1bb5b9e`.

* **Measured.** Captures 1750..1762 are now explained; port frames 0..1266 are byte-identical to before, and 1267 (the f = 850 state) is the first that differs. The demo oracle's first unexplained is now **1763 (raw 4670)**. The demo window `[1763..3616]` has 1854 frames, 1848 unexplained, and the fight window `[1763..1884]` has 122 frames, 0 explained. The ratchet N is raised **1750 → 1763** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..1749]` / 1190 → **`[560..1762]` / 1203 / `473 clean, 723 splice, 3 transition, 2 unexplained (832, 833)`**; the three transition frames are the same as before. Nothing else moved.
* **Residual (characterised, not fixed).** Capture 1763's best 1277/1278 splice (row 130) leaves 153 px, all in x 15–22, rows 53–98, all green (0, 203, 0); 1764..1766 leave the same box. The capture draws a vertical "2 HIT COMBO" at the left edge. `0x39040` runs its gated body at f = 860 for side 0 with `DSW(0x107D2C)` = 2 (the T-rex's second hit) and there draws the text through `0x38D90` ("COMBO" at `0xBE01C`, referenced only from `0x38E1D`); the port skips that draw as a `PORT:` named gap (the `0x2F4D0`/`0x2EFD4` text-grid formatter). Not derived.
* **Known later gaps (unregistered code targets, skipped; a whole-run `fn_resolve` probe).** `0x3E3A8` (f = 962). `0x3C0A4`, `0x14F50` and `0x3A820` no longer miss.

See §32 of the record.

**The combo text `0x38D90`/`0x38FEC` (`bcce10b`), the demo fight's captures 1763..1880.** `0x39040` draws the combo text through `0x38D90` and checks the combo names through `0x38FEC`, and the port skipped both as a `PORT:` named gap. At f = 860 the T-rex's second hit draws "2", the HIT glyph (`0x1B`) and "COMBO" (`0xBE01C`) down the text grid's column 2 (rows 8..14). The glyph grid and the string table were already ported, so the gap was ten functions: `0x2F4D0`/`0x2EFD4`/`0x2EF24` (the `"%i"` number formatter), `0x2F20C`/`0x2F314` (vertical draw and clear), `0x38C5C`/`0x38D90`/`0x38D24` (the combo text, its clear and its 180-frame timer, now wired at `fight_hud_pass`'s `0x357F5`) and `0x38FEC`/`0x38ED0` (the per-character combo names on rows 6/7). New `check_text_vertical_number` in `test_platform.c` and `check_combo_text` in `test_fight.c`; all 55 mutations fail them.

* **Measured.** Captures 1763..1880 are now explained; port frames 0..1276 are byte-identical to before, and 1277 (f = 860) differs in exactly the capture's 153 px. The demo oracle's first unexplained is now **1881 (raw 4788)**. The demo window `[1881..3616]` has 1736 frames, 1730 unexplained, and the fight window `[1881..1884]` has 4 frames, 0 explained. The ratchet N is raised **1763 → 1881** in the same commit.
* **Claim move (the one the brief allowed).** Front-end `[560..1762]` / 1203 → **`[560..1880]` / 1321 / `516 clean, 798 splice, 3 transition, 2 unexplained (832, 833)`**; the three transition frames are the same as before, and port frames 0..1378 are exhibited (1142). Nothing else moved.
* **Residual (characterised, not fixed).** Capture 1881's best 1378/1379 splice (row 137) leaves 3 791 px in x 0–204, rows 137–192, and 1882..1884 leave 8 429..11 140 px. Below the split the capture's gold T-rex drops into a new pose, lower, legs apart and its tail flat on the sand, while port 1379 (f = 962) keeps it upright as in 1880; the raptor, the worshippers and the camera match. f = 962 is the run's `0x3E3A8` miss (a `hit_reaction_apply` reaction callback, unregistered). Not derived. The port's dump ends at frame 1380, so the fight window holds only 4 captures.
* **Known later gaps (unregistered code targets, skipped; a whole-run `fn_resolve` probe).** `0x3E3A8` (f = 962), unchanged.

See §33 of the record.

Streamed Smacker audio (2b-ii), the remaining menus/EEPROM storage I/O (4), the
demo fight's remaining arena divergence (first unexplained at capture 1881
after the frame-1763 fix: the T-rex's new pose at f = 962, where the
unregistered reaction callback `0x3E3A8` misses, not derived; the unported `0x19020`/`0x3E484` hook,
`0x3B464`'s `0x235C4` arm, the effects pass's types 2, 7 and 9..12, the
grab arm of `0x4B788`, `0x4B470`'s eighth-hit tail and case 8's held body are
named gaps; the unregistered code target `0x3E3A8` (f = 962) is the
candidate owner of 1881, and `0x370F0` is still unregistered; not reached in
this run) and
the
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
