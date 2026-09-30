# Gameplay ground truth (U1–U4): design

**Status:** design for units U1–U4 of the gameplay ground-truth effort
(scoping report `.superpowers/sdd/2026-09-30-gameplay-scope/report.md`, main
`1c2bb63`; this branch `gameplay-ground-truth` at `934992a`). Plans:
`docs/superpowers/plans/2026-09-30-gameplay-u1-capture-harness.md`,
`…-u2-port-replay.md`, `…-u3-frame-compare.md`, `…-u4-idle-loss-run.md`.
One derivation record serves all four units:
`docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md`
(created by U1 Task 0, one `§G.n` section per task). §3 holds the facts the
planner established from the raw and one DOSBox-X probe; §7 lists what is
still open. Where §3 and the scoping report differ, §3 wins (raw and capture
win) and §3.9 records each correction.

## 1. Purpose

No oracle covers play. Every oracle stays in mode 3 (title, attract, demo
fight) or mode `0x27` (the K11 service menu); the match modes are covered only
by seeded unit tests (scoping finding 1). U1–U4 build the ground truth for play:
drive the pinned original through a real match under DOSBox-X with
frame-exact input, replay the same input in the port keyed by the same frames,
and compare the two frame by frame and state by state, with a ratchet in
`make verify` that fails when the first unexplained frame moves earlier. U4 is
the first real run: START MENU → LEFT PLAYER ARCADE → character select → a
match with P1 idle → the CPU wins → the challenge/continue offer → game over.

Nothing here changes game code. U1 and U3 are tools; U2 is a test driver; U4 is
a capture plus pinned ratchet values. The existing oracle lines do not move.

## 2. Scope

| Unit | Deliverable | Depends on |
|---|---|---|
| U1 | `tools/gp_session.py`, `tools/gp_capture.py`: frame-keyed injection, P1/P2 pad names, chords and holds, a per-frame consistent snapshot log, the port-script v2 generator, a trace diff; `make gp-capture` | — |
| U2 | `test_gp_replay` (`PR_GP_DUMP`/`PR_GP_SCRIPT`): replays a v2 script keyed by the frame counter, dumps every displayed frame (indices + DAC) and one trace line per master-loop iteration | U1's script format |
| U3 | `tools/gp_compare.py`: frame claim (title_compare's clean/splice/transition model, windowed with a full fallback) and trace claim, each a first-unexplained ratchet; `make gp-oracle` in `make verify` | U1, U2 formats |
| U4 | scenario `gp-idle-loss`: two captures, the determinism diff, the port replay, triage, the pinned ratchet values, the record and `docs/PROGRESS.md` | U1–U3 |

Out of scope (later units of the scoping report): U0 missing-function audit,
U5 character-select walk, U6 moves, U7 two players, U8 other modes, U9 win
path, U10 endings, U11 in-match keys. Fixing any divergence U4 finds is also out
of scope: U4 pins it and names it; a follow-up unit fixes it.

## 3. Facts established while planning

All code addresses are linear (Ghidra) addresses; `DS_00xxxxxx` names the
global at that linear address. Raw listings were read with capstone on
`data/game/C/PRAGE.EXE` (obj-0 file offset = VA + `0x52E54`; data
displacements in these listings are pre-fixup, VA − `0x80000`, per AGENTS.md)
and from `port/decomp/prage.c` (Ghidra, fixups applied). U1 Task 1 re-derives
each fact into the record with the same commands.

### 3.1 The master loop, the frame counter and the pad sampler

The master loop `0x255CC` (raw): `0x255D4..0x255DF` zeroes `DS_00101508` and
copies it to `DS_0010150C`; then per iteration:

1. `0x255EE call 0x500C4` — the pad sampler: reads the ISR key bitmap
   `[DS_00101514]+0x2D8/+0x2D9` into `DS_000E1C30` (raw) and the debounced level
   `DS_000E1C34` (pad). A bit that changed this iteration keeps its previous
   level (`input.c` `input_pump`), so the level lags the raw word by one
   iteration.
2. `0x2560B call 0x24C5C` (`game_frame`): `0x24C6E call 0x4F644` (skipped in
   mode `0x27`, `0x24C69 cmp eax,0x27`) builds the newly-pressed/held masks
   `DS_001088E4`/`DS_001088D8` and the command words `DS_001088E0` (P1) and
   `DS_001088E2` (P2); the CPU-AI block `0x24C73..0x24CB5`; then
   **`0x24CCD..0x24CDB` the frame counter word `DS_000EF6DC` increments**;
   the update table; the int 16h key loop `0x24CFE..0x24EE7`; the mode switch
   `0x24EEC`.
3. The render table, `0x25632 inc word [0x104AF4]`, `0x25639 call 0x134C0`,
   the present gate `0x25643` (`DS_0010150C == DS_00101508`), the audio service
   `0x256B6`, `0x256C0 inc [DS_0010150C]`, and the spin `0x256C6..0x256DB`
   while `DS_0010150C − 1 == DS_00101508` (its `rng(0x7FFF)` at `0x256D6` is
   pinned to `mov eax,0` in the pinned copy, `tools/title_pin.py`).

The timer ISR body (`0x1BDF4`, skipped while `DS_00104B22 == 1`):
`0x1BE0E..0x1BE16` increments `DS_00101508` and `DS_00101500`, `0x1BE1C call
0x1BBAC` (the key sampler), `0x1BE21 inc word [DS_000EF6DE]`,
`0x1BE28 call 0x2D62C`.

**Consequence (the answer to "where the frame counter increments vs
`0x500C4`"):** `0x500C4` samples the bitmap *before* `0x24CDB` increments `f`
in the same iteration. The bitmap it reads is the one the ISR tick that
released the previous iteration's spin left. So a key-state change made while
the loop spins at the end of iteration `f` is sampled by iteration `f + 1`,
appears in `DS_000E1C30` in the iteration that raises the counter to `f + 1`,
and in the level `DS_000E1C34` one iteration later. A BIOS key queued at the
same moment is read by the key loop, after `0x24CDB`, of a later iteration
(§3.6). While the loop spins (`DS_0010150C − 1 == DS_00101508`) iteration `f`
is complete and the next has not begun: **every global the iteration writes is
consistent in that state.** U1 takes its per-frame snapshot there.

### 3.2 The pad bitmap: which key sets which bit

`[DS_00101514]` points at a block in conventional memory (probe: `0x0000FE20`).
The probe read `+0x2D4..+0x2ED` in mode 3 as
`0000 0000 0000 0000 0000 | 1f 2d 2c 2e 16 17 31 32 | 48 50 4b 4d 47 49 4f 51`:
both device words `+0x2D4`/`+0x2D6` are 0 (keyboard), `+0x2DE..+0x2E5` are
P1's eight scan codes and `+0x2E6..+0x2ED` P2's. They are the high bytes of the
config words at `0x122C62` (named-gaps-a record §A.1.5: `dev1 0x1f73 0x2d78
0x2c7a 0x2e63 0x1675 0x1769 0x316e 0x326d`, `dev2 0x4800 0x5000 0x4b00 0x4d00
0x4700 0x4900 0x4f00 0x5100`).

`0x1BBAC` with device 0 (Ghidra `FUN_0001bbac`, `prage.c:7486`): P1's byte
`+0x2D8 = 0x1B610() | 0x1B730() | 0x1B850()`, P2's `+0x2D9 = 0x1B6A0() |
0x1B7C0() | 0x1B870()`. A key is down when bit 7 of the IRQ1 key-state byte
`[DS_00101514]+0x254+scan` is clear. `0x1B610` gives `+0x2DE..+0x2E1` bits
`0x80 0x40 0x20 0x10`; `0x1B730` gives `+0x2E2..+0x2E5` bits `1 2 4 8`;
`0x1B850` is scan `0x3B` (F1, `+0x28F`) and ORs bit 0; `0x1B6A0`/`0x1B7C0`/
`0x1B870` are the same for P2 with `+0x2E6..+0x2ED` and scan `0x3C` (F2,
`+0x290`). The kb word the tools use is `(+0x2D8 << 8) | +0x2D9` (the K11 poll
log's `kb`, the port's `host_key_bits()` layout, `flow.c` `game_loop`):

| name | P1 key (scan) | P2 key (scan) | P1 kb bit | P2 kb bit |
|---|---|---|---|---|
| up | S (`0x1F`) | Up (`0x48`) | `0x8000` | `0x0080` |
| down | X (`0x2D`) | Down (`0x50`) | `0x4000` | `0x0040` |
| left | Z (`0x2C`) | Left (`0x4B`) | `0x2000` | `0x0020` |
| right | C (`0x2E`) | Right (`0x4D`) | `0x1000` | `0x0010` |
| b0 | U (`0x16`) | Home (`0x47`) | `0x0100` | `0x0001` |
| b1 | I (`0x17`) | PgUp (`0x49`) | `0x0200` | `0x0002` |
| b2 | N (`0x31`) | End (`0x4F`) | `0x0400` | `0x0004` |
| b3 | M (`0x32`) | PgDn (`0x51`) | `0x0800` | `0x0008` |
| start | F1 (`0x3B`) | F2 (`0x3C`) | `0x0100` | `0x0001` |

`0x500C4` places `+0x2D8` at bits 24..31 and `+0x2D9` at bits 8..15 of
`DS_000E1C30`. The start masks `0x9ACBC` are `0x01000000` (side 0) and `0x100`
(side 1) (demo-pose record §47-M), i.e. bit 0 of each byte: **start is F1/F2,
and b0 (U/Home) sets the same bit.** The capture evidence so far: the K11 walk's
Down gave `kb=0040` (`walk/poll.log:1533`, record §A.4), which the table
predicts. F1/F2 and the letter keys are raw-derived only: the probe's taps fell
in the boot movies, where the master loop (and so `0x500C4`) does not run. U1
Task 8 closes this with a capture (§7 Q2).

The menus read directions from the pad level (record §A.1.5); the service-menu
editors arm the repeat on `0xF000F000` (bits 4..7 of each byte), consistent
with the table.

### 3.3 START MENU row order

`0xBCCCC` (K11 record §0.2, fixed-up image): title "START MENU", then seven
items `0xBCCDC..0xBCD3C` in this order, each with its `+8` callback and the
`DS_00104B00`/`DS_00104B1D` it stores (`svcmenu.c` `0x2CBC4..0x2CC54`):

| row | label | callback | mode / sub |
|---|---|---|---|
| 0 | LEFT PLAYER ARCADE | `0x2CBC4` | `0x2D` / 0 |
| 1 | RIGHT PLAYER ARCADE | `0x2CBDC` | `0x2E` / 0 |
| 2 | LEFT PLAYER TRAINING | `0x2CBF4` | `0x28` / 1 |
| 3 | RIGHT PLAYER TRAINING | `0x2CC0C` | `0x29` / 1 |
| 4 | TUG OF WAR | `0x2CC24` | `0x2A` / 2 |
| 5 | ENDURANCE | `0x2CC3C` | `0x2B` / 3 |
| 6 | Start 2 PLAYER HANDICAP | `0x2CC54` | `0x2C` / 4 |

The START MENU opens with the cursor on row 0 (`0x2CB74`'s inner `0x2FFC4`
initialises `DS_0010741C = 0xBCCDC`, K11 record §K11.2). So from mode 3 the key
sequence Enter (mode `0x27`, MAIN MENU on "Start"), Enter (START MENU), Enter
(LEFT PLAYER ARCADE) stores mode `0x2D`. The probe confirms it (§3.5).

### 3.4 Credits and continue requirements

- The credit counter is `DS_00105C00`; `0x2C304` sets it to
  `((field 0x29 & 0xF0000) >> 16) + 1` at `game_state_init` (`0x10ECC`) and after
  GAME OPTIONS. With the zero CMOS (the defaults path the K11 captures use,
  `k11_capture.check_cmos`) the probe read **5 credits and FREE PLAY
  `DS_00105D60 = 0`** in mode 3.
- There is no coin input in this build's play path: credits come only from the
  config. Mode `0x2D` (`0x25071`) spends one through `0x2CA7C(1)` (probe: 5 → 4
  at `f = 0x248`) and diverts side 0 (`0x257A4(1)`).
- An attract start (`0x11F28`: a credit and the start mask newly pressed) is a
  second way into a match; U4 does not use it (U8's).
- After a loss the offer is mode `0x13`'s challenge screen (`0x424E8`, record
  §48-D): sub-state 4's `0x42CB4` takes "the loser's start with a credit"; the
  count `DS_00108110 = 9` ticks and below 0 the mode becomes `0x1E`. With P1
  idle nothing is taken. The continue screen of mode `0xE` (`0x27A2C`) is
  reached from mode `0xC`'s `0x272DC`, which the idle run never enters.
- After game over the credits are re-initialised: the probe reads 5 again at
  the next mode 3 (`f = 0x22BF`).

### 3.5 The idle-loss path, observed (planner's probe)

Probe: the pinned copy (`title_pin.py`, sha256 `8120f1bd…a68d`, the K11 walk's
exe), DOSBox-X 2026.08.31, no video, `-time-limit 300`, zero CMOS; a poller
thread on the memory file (base `0x266000`) logging one snapshot per `f` in the
spin state and every mode change; injected Enters at 25 s (mode 3), mode
`0x27` + 2.5 s, + 5.0 s; nothing else. Kept (ignored by git)
in the ledger directory `.superpowers/sdd/2026-09-30-gameplay-scope/planner-probe/`
(`gp_probe.py`, and `probe.log`: 14 661 lines, sha256 `37785064…e2c`); U4
re-measures everything with the committed harness. First `f` seen in
each mode (`f` hex; `S` = a spin snapshot, `M` = an asynchronous read):

| f | mode | what |
|---|---|---|
| `0x11D` | 3 | the Enter queued (spin snapshot `f = 0x11D`) |
| `0x120` (M) | `0x27` | MAIN MENU (`ent = 0x2A2BEC`) |
| `0x1B3` | `0x27` | Enter queued; START MENU from `0x1B6` (`ent = 0x2A2CDC`) |
| `0x246` | `0x27` | Enter queued |
| `0x247` (M) | `0x2D` | LEFT PLAYER ARCADE; credits 5 → 4 at `0x248` |
| `0x248`, `0x25C` | `0x1A`, `0x1B` | the wipe |
| `0x26C` | `0x10` | character select, P1 idle for 916 frames |
| `0x600..0x728` | `0x1A 0x1B 0x11 0x17 0x1A 0x1B` | the pick timed out; wipes and countdowns |
| `0x73A` | 5 | round start |
| `0x7B5` | 6 | round 1 (P2 command word `0x2020`, P1's `0`) |
| `0xEDC` | 8 | round end: P1 slot `+0x5A = 0x78` |
| `0xF8E`, `0x107F` | `0x16`, 5 | results countdown, round start |
| `0x10F9` | 6 | round 2 (3 249 frames) |
| `0x1DAA` | 7 | match end, P1 `+0x5A = 0x44`, P2 `+0x5A = 0` |
| `0x1E19`, `0x1E1C` | 9, `0x17` | results, darken |
| `0x1F0B` | `0x15` | countdown |
| `0x1F84` | `0x13` | the challenge screen (§3.4) |
| `0x2200`, `0x2204` (M), `0x2207` | `0x1E`, `0x14`, `0x17` | game over (no name entry at score 0) |
| `0x22BA` (M) | 3 | attract; the boot movies follow (`f` frozen, raw `0x22B9 → 0x22BF`) |

Mode 3 at `f = 0x22BA` came 169.9 s after the poller started (DOSBox-X's start); the match runs at
60.0 frames/s (`0x7B5 → 0xED9`: 1 828 frames in 30.45 s). From the first Enter
to mode 3 is 8 605 iterations.

### 3.6 BIOS key consumption latency

A key queued in the BIOS ring during the spin of `f` was consumed one to three
iterations later: the mode-3 Enter queued at `f = 0x11D` took effect in the
iteration that raised `f` to `0x120` (spin snapshots `0x11E` and `0x11F` still
read mode 3); the START MENU Enter queued at `0x246` took effect at `0x247`.
The cause is not established (§7 Q3). The design does not depend on it: the
port replays each key at the iteration where the capture shows it consumed
(the BIOS head `0x41A` advanced), not where it was queued.

### 3.7 Snapshot coverage

In the probe the spin state was observed for 14 602 distinct `f` values between
`0x4` and `0x3946`; 57 frames were missed in 25 gaps, every gap at a load or a
mode change (for example `0x11F → 0x121`, `0x246 → 0x248`, `0x2203 → 0x2207`),
when the loop catches up without spinning. So a spin snapshot exists for
99.6 % of the iterations, and the design treats a missing snapshot as "not
observed", never as "unchanged". `DS_00101508`/`DS_0010150C` restart at 1/2 at
some transitions (the probe's `f = 0x121`, `0x248`); the spin predicate is
relative and unaffected.

### 3.8 The pins

The pinned copy's patches (`tools/title_pin.py`) are behaviour pins: the three
title-entry draws, the anim opcode-8 handler `0x2B2A0` (pinned to 0; the port
mirrors it with `actors_pin_anim_tick_zero(1)`), and the master loop's body and
spin draws `0x256B1`/`0x256D6` (non-advancing). So the capture's RNG
`DS_000EF6D8` advances only at consumption sites, like the port's. The ground
truth of U1–U4 is **the pinned original**, as for every existing oracle.

### 3.9 Corrections to the scoping report (raw and capture win)

1. The fight runs in **mode 6** (`0x28CC8`'s join poll falling back to the fight
   frame), not mode 4, in the one-player arcade path (§3.5, rounds at `0x7B5`
   and `0x10F9`).
2. The post-loss offer is **mode `0x13`** (the challenge screen), not mode `0xE`;
   mode `0xE` needs mode `0xC`, which the idle path never enters (§3.4, §3.5).
3. The observed tail is `7 → 9 → 0x17 → 0x15 → 0x13 → 0x1E → 0x14 → 0x17 → 3`;
   no name entry (score 0) and no winner roar `0x1F`.
4. `K11_MAX_LOOPS 20000` would cover the idle run (8 605 iterations); the v2
   driver still drops the fixed caps (§4.2) because the scripts of later units
   are longer.
5. `host.c`'s `k_input_bind` labels (bit 0 "coin", 1 "player 1 start", …) do
   not match the raw layout of §3.2 (bit 8 is P1 start). It is the stale claim
   the scoping report lists; U1–U4 do not use it (the drivers use the override
   seam) and do not fix it.

## 4. Design

### 4.1 U1 — capture harness v2

New files, so the K11 tools and their oracle stay byte-unchanged:
`tools/gp_session.py` (constants, scenarios, the log formats, the scheduler,
the port-script generator, the trace diff; stdlib only) and
`tools/gp_capture.py` (the DOSBox-X run, the poller/injector thread, the frame
writer). They import `k11_capture` (`guard_out`, `find_base`, `check_cmos`,
`bios_insert`, `u16`), `title_capture` (`stage`, `post_logo_start`,
`collapse_from`) and `smk_capture` (`which`, `read_avi_frames`,
`ffprobe_fps`) read-only. Captures go to `data/k11-captures/gp-<scenario>/`
(the one writable place under `data/`; `guard_out` enforces it).

**Scenario language (frame-keyed).** A scenario is a tuple of steps:

- `('boot', seconds, action)` — at a wall time from DOSBox-X start (the boot
  movies freeze `f`); only for the mode-3 Enter, as K11's `ENTER_WAIT = 25.0`.
- `('after_mode', mode, n, action)` — at frame `F = f0 + n`, `f0` the first
  snapshot `f` in `mode`.
- `('after', n, action)` — `F` = the previous step's frame + `n`.
- `('until_mode', mode, n)` — the scenario's end: frame `f0(mode) + n`, the
  mode looked for only after every earlier step fired.
- actions: `('key', name)` — a BIOS key (`enter`, `esc`): the key-state byte
  held for `HOLD_FRAMES = 3` iterations and the BIOS word queued once (K11
  record §A.9: the 3-tick press AUTOTYPE's taps showed; a stimulus);
  `('pad', names, n)` — a chord of §3.2 names (`p1.up`, `p2.b3`, `p1.start`, …)
  held for `n` iterations, each key's BIOS word queued once at the press (the
  make code a keyboard would queue; typematic repeat is not modelled, §7 Q4).

**Frame-exact injection.** An action for frame `F` fires when the poller holds a
consistent spin snapshot with `f == F − 1` (or the first one past it, logged
`late=1`): the key-state bytes are written during the spin, the next ISR tick
samples them, and iteration `F` reads them in `0x500C4`. A hold of `n` releases
at the spin of `F − 1 + n`. The log records what the game saw, so a late or
racing write is visible and the port replays the observed frame, never the
intended one.

**Per-frame poll log (`poll.log`, v2).** One `S` record per `f` taken in the spin
state, accepted only when two reads of the fields around it agree on `f` and
`DS_00101508`:

```
B ms=<int> base=<hex8> ptr=<hex8>                    the data object's base, [DS_00101514]
S ms=<int> f=<hex4> <field>=<hex> … kb=<hex4> head=<hex4> tail=<hex4>
P ms=<int> f=<hex4> mode=<hex4> st=<hex4> tick=<hex8>  mode/st changed, read outside a spin state
H ms=<int> f=<hex4> head=<hex4>                       the BIOS head moved (a key consumed)
I ms=<int> f=<hex4> step=<n> press=<name> scan=<hex2> lin=<hex8> old=<hex2> bios=<hex4|-> ring=<0|1|-> late=<0|1>
I ms=<int> f=<hex4> step=<n> release=<name> lin=<hex8>
X ms=<int> f=<hex4> step=<n> end                      the scenario's end frame
E ms=<int> reason=<exit|time-limit> rc=<int>
```

The `S` fields: `f` `DS_000EF6DC`, `mode` `DS_00104B00`, `st` `DS_000F0A64`,
`tick` `DS_00101500`, `t508` `DS_00101508`, `t50c` `DS_0010150C`, `raw`
`DS_000E1C30`, `pad` `DS_000E1C34`, `new` `DS_001088E4`, `held` `DS_001088D8`,
`e0` `DS_001088E0`, `e2` `DS_001088E2`, `rng` `DS_000EF6D8`, `cred`
`DS_00105C00`, `fp` `DS_00105D60`, `b1d` `DS_00104B1D`, `b1f` `DS_00104B1F`,
`b25` `DS_00104B25`, `w10d` `DS_0010810D`, `cnt` `DS_00108110`, the two slots'
`+0x52`/`+0x54`/`+0x5A` (`DS_00107802`/`04`/`0A`, `DS_00107896`/`98`/`9E`;
stride `0x94`), `ent` `DS_0010741C`.

**Frames.** `DX-CAPTURE /V /O` as K11. After the run, pass 1 hashes every AVI
frame (streaming, md5 only), finds the post-logo start and the distinct run
(`title_capture`); pass 2 streams the distinct frames to
`frame_%05d.raw.gz` (RGB24 320×200, gzip level 1) with `window.txt`
(`<index> <raw>`). A 200 s run holds about 14 000 AVI frames; the distinct ones
from the Enter on are about 9 000 (1.7 GB raw), so nothing holds all frames in
memory and nothing is written uncompressed.

**Checks** (each a `CHECK … ok|FAIL` line; exit 1 on a failure): the base was
found; every step fired; every key's consumption frame and every pad change's
sample frame is pinned (the generator below succeeds); mode `0x27` follows the
first Enter; the end frame was reached before the time limit.

**Port script v2** (`gp_session.py port-script`), frame-keyed:

```
# gp port script v2: scenario <name>
enter_frame <dec>          first f in mode 0x27 (the K11 driver's check)
enter_state <hex4>         DS_000F0A64 then
key <f> <scan hex2> <ascii hex2>    queue before the iteration that raises the counter to <f>
bits <f> <kb hex4>                  the bitmap from the iteration that raises the counter to <f> on
end <f>                             stop after the iteration that raises the counter to <f>
```

A key's `<f>` is its consumption frame: the `H` record that pairs with the `I`
press (FIFO), accepted only when the `S` record of `f − 1` shows the old head
and `S(f)`, when present, the new one (else: "key N consumption unpinned" or
"S(f) disagrees", exit 1). A `bits` line is emitted at each
change of the `S` records' `raw` (`kb = (raw >> 24) << 8 | (raw >> 8) & 0xFF`),
accepted only when `S(f − 1)` exists. `enter_frame` must equal the first key's
frame. A key consumed while `f` is frozen (a blocking loop) is an error: no
spin snapshot exists inside a blocking loop, so its `H` record carries the last
master-loop `f`, whose `S` record still shows the old head, and the generator
rejects it; v2 is master-loop only (§7 Q5). `--end F` cuts the script at an
earlier frame (U4's fallback for a replay that stalls; the cut is pinned with
its evidence).

**Trace diff** (`gp_session.py trace-diff A B`): the first `f` at which two
captures' `S` records differ in `TRACE_FIELDS = (mode, st, raw, pad, e0, e2,
rng, cred, s0_5a, s1_5a)`, over the `f` both hold; `tick` is compared and
reported on its own line (host-timed, §7 Q6).

### 4.2 U2 — port replay v2

A new env-gated driver `test_gp_replay` in `port/tests/test_game.c` (the area
file of `flow.c`; K11's driver stays untouched), registered once in
`TEST_DRIVERS` as `X(test_gp_replay, "PR_GP_DUMP")`. It runs `game_init()` and
`actors_pin_anim_tick_zero(1)` as the K11 driver does, then steps
`game_loop_step()` (one iteration each):

- Before the iteration that raises `DS_000EF6DC` to `F`: queue every `key F`
  (`input_push`) and apply the last `bits F` (the K11 driver's `k11_key_bits`
  shape: `host_set_key_bits_override` plus the two bitmap bytes, so blocking
  0x500C4 pumps see it too).
- The steps come from the script's line count (`calloc`), not a fixed
  `K11_MAX_STEPS`; the loop bound is `end + GP_LOOP_SLACK` iterations from
  boot (`GP_LOOP_SLACK = 600`, a harness bound named in the record), with the
  K11 driver's pump-stall guard. A key the port leaves queued after its
  iteration is logged (`left-queued`), not failed: it is a divergence for the
  comparison to show.
- **Dump every displayed frame.** After each iteration and at every host pump
  and loader screen (`host_set_pump_hook`, `res_set_screen_hook`), hash the
  displayed indices (`gfx_display()`, else `DS_000E87A0`) plus `gfx_dac`; a new
  hash writes `frame_%05d.ipx` (64 000 index bytes then the 768-byte DAC) and a
  `frames.txt` line `<index> f=<hex4> tick=<hex8> mode=<hex4>`. Every present
  changes what `gfx_display()` returns, and a present happens at most once per
  iteration (the gate `0x25643`, then the spin's pump), so this sees every
  presented frame; a present identical to the previous one is the same image
  and the splice model (§4.3) cannot tell it apart. `.ipx` is a third of RGB24
  (about 580 MB for the idle run); the comparison expands it through the DAC,
  exactly as `fe_write_frame` does (`rgb = dac[idx]`).
- **Trace.** After each iteration, one `T` line with the `S` field names and the
  same globals as §4.1 (from `mem[]`), so U3 compares the two by `f`.
- End: the iteration that raises `f` to `end` (or the K11 driver's
  `longjmp` exit when the end falls in a blocking loop). The driver's checks:
  the Enter arm (mode 3 → `0x27` at `enter_frame`, `enter_state`), every key
  queued, the end reached, frames written; sentinels as the K11 driver's.

No production code changes: the seams exist (`host_set_key_bits_override`,
`host_set_pump_hook`, `res_set_screen_hook`, `host_set_fault_hook`).

### 4.3 U3 — frame-by-frame comparison and the ratchet

`tools/gp_compare.py` (stdlib; imports `title_compare` read-only). Two claims:

**Frame claim.** The capture's distinct frames (`frame_%05d.raw.gz`) against
the port's (`frame_%05d.ipx` → RGB). A capture frame is explained by
`title_compare.explain` exactly: clean, a byte-offset splice
`port[N][0..b) ++ port[N+1][b..)` of adjacent port frames, or one transition
row. This is the demo-fight model for the 70.09 Hz capture against the
60.05 Hz game (AGENTS.md): a DOSBox frame shows the scan-out of one game frame
switching to the next at byte `b`. All-black capture frames are the documented
capture artefact and are skipped. Speed: `explain` is first tried on the port
frames `[p − 2, p + 64)`, `p` the last explained port index; only when that
fails does it run over the whole dump, so the result equals the unwindowed
classification (a window is only a search order). Row hashes are computed
once per frame; full frames are loaded on demand. The window starts at the
first capture frame that exhibits a port frame at or before the port's first
dump (the Enter), as K11's. **Ratchet:** the first unexplained capture frame
`j` must satisfy `j >= N` (`GP_IDLE_LOSS_MIN_FIRST` for `gp-idle-loss`). When the port's script ends
before the capture, the capture frames after the port's last frame are
unexplained, so `j` also bounds how far the port got: a longer, faithful replay
raises it. `--report` continues past the first unexplained (up to 5, with the
K11 difference boxes).

**Trace claim.** The capture's `S` records against the port's `T` records by `f`,
from `enter_frame` to the port's end, over `TRACE_FIELDS`; an `f` the capture
did not snapshot is skipped and counted. **Ratchet:** the first differing `f`
must be `>= F` (`GP_IDLE_LOSS_TRACE_MIN_FIRST`). The first difference of `tick` is
reported, not ratcheted (§7 Q6).

**Make.** `make gp-oracle` (in `make verify` after `k11-oracle`): for each
scenario (today `gp-idle-loss`), when `data/k11-captures/<scenario>` exists,
build the script (`make gp-replay`), run `PR_GP_DUMP`, compare with both
ratchets; a missing capture skips
(exit 0), like the K11 and front-end oracles (enforced without
`PR_ORACLE_REQUIRED`; an unpinned N with a present capture fails). `make
gp-report` runs `--report`. The pinned values and their provenance live in the
Makefile, as `DEMO_FIGHT_MIN_FIRST`.

The claims are narrow in the same way as the front-end and K11 oracles: a
green frame claim says no content-bearing capture frame before `N` is
unexplained; it does not say the frames after `N` are right, nor that the port
renders everything (the trace claim is what catches a port that diverges in
state while the pictures still match).

### 4.4 U4 — the first scripted run

Scenario `gp-idle-loss` (`time_limit = 200`):

```
('boot', 25.0, ('key', 'enter'))              mode 3 -> 0x27 (MAIN MENU, cursor on Start)
('after_mode', 0x27, 150, ('key', 'enter'))   START MENU (cursor on row 0)
('after', 150, ('key', 'enter'))              LEFT PLAYER ARCADE: mode 0x2D
('until_mode', 0x03, 0)                        the end: back in mode 3 after game over
```

150 frames matches the probe's 2.5 s gaps (a stimulus, not a game value); 200 s
covers the probe's 169.9 s. U4 captures it twice (`gp-idle-loss` and
`gp-idle-loss-run2`), runs `trace-diff` (the determinism answer, §7 Q1),
replays it in the port, triages the first frame and trace divergences with
evidence, pins `GP_IDLE_LOSS_MIN_FIRST` and `GP_IDLE_LOSS_TRACE_MIN_FIRST`
at the measured values, proves each ratchet can fail, and writes the record and
a `docs/PROGRESS.md` paragraph. Its acceptance: the capture reproduces §3.5's
mode path (or the record says where and why it differs); both ratchets are in
`make verify`; the oracle lines of the baseline are unchanged.

## 5. Cross-cutting rules

- Raw wins; never a fitted constant; a value that cannot be pinned is a named
  gap with its evidence. Harness values (`HOLD_FRAMES`, the 150-frame gaps,
  `time_limit`, the `[p − 2, p + 64)` search order, `GP_LOOP_SLACK`) are named as
  harness values with their source, never presented as game values.
- `make verify` is the gate after every task; the oracle lines equal the U1 Task 0
  baseline; the enforced front-end oracle, the demo-fight and attract cycle-2
  ratchets and the K11 oracles stay green.
- Tests: Python `unittest` under `tools/tests` (stdlib), C only `CHECK`/
  `CHECK_EQ_INT`; every assertion can fail and each new test is shown failing
  under a named mutation; the driver runs alone (`game_init()` once).
- Writes under `data/` only to `data/k11-captures/gp-*`; scratch under `/tmp`.
  Never `pkill`; stage named files; commit trailer
  `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Risks

- **Nondeterminism of the original.** If two captures of `gp-idle-loss` differ
  (host-timed ISR ticks feeding play, §7 Q6), the ratchets can only claim the
  common prefix; U4 then pins the trace ratchet below the first run-to-run
  difference and names it.
- **Disk and time.** About 1.7 GB per capture before gzip and 580 MB per port
  dump; the comparison is linear in the number of frames with the window
  (quadratic only on unexplained frames, and the enforced run stops at the
  first). `make verify` grows by one port run and one comparison.
- **The port may diverge early** (the character select's 916 idle frames, the
  pick timeout, the loads). That is the expected outcome of U4; it is pinned,
  not fixed, here.
- **Snapshot gaps at loads** (§3.7): a key or pad change inside a gap cannot be
  pinned; the capture's check fails and the scenario moves the stimulus.

## 7. Open questions (evidence so far)

- **Q1 — Is the pinned original deterministic over a match under frame-keyed
  input?** Unknown. The RNG host-timed draws are pinned (§3.8), but play may
  read the ISR clock `DS_00101500` (Q6). Answered by U4 Task 3 (two captures,
  `trace-diff`). Not blocking for U1's code; blocking for U4's ratchet values.
- **Q2 — F1/F2 and the letter keys as pad bits.** Raw-derived (§3.2), not yet
  seen in a capture: the probe's taps fell in the boot movies. U1 Task 8
  (`gp-pads` on the MAIN MENU, where `0x500C4` runs every frame) closes it. Not
  blocking.
- **Q3 — Why a BIOS key is consumed 1–3 iterations after it is queued** (§3.6).
  Unknown; the design replays the observed consumption frame, so not blocking.
- **Q4 — BIOS words for pad presses.** A real keyboard queues the make code
  (and typematic repeats after the delay) for every key, including the pad
  letters, which the key loop latches (`DS_00105F30`) and name entry (mode
  `0x1E`) types. The design queues the config word once per press (`0x1F73` for
  S, …; arrows in the `E0` form the AUTOTYPE runs left, record §A.9) and does
  not model typematic repeat. **Needs the user's agreement before U1 Task 2**
  (it decides what `('pad', …)` writes); U4 presses no pad, so U4 does not
  depend on it.
- **Q5 — Keys inside blocking loops.** v2 keys by `f`, which does not advance in
  a blocking loop (OPTIONS MENU, movies, name entry's waits if any). Not needed
  by U4 (the MAIN and START menus are `menu_step` per frame: the probe's `f`
  advances in mode `0x27`); a later unit adds a tick-keyed step (K11's `dtick`).
- **Q6 — Does play read the host-timed clock?** `DS_00101500` advances per ISR
  tick, and loads take host time (the probe's `t508` restarts). If a play
  routine reads it, the port's tick model decides the outcome. The trace diff
  reports `tick` separately so U4 can tell.
- **Q7 — Storage policy.** `data/k11-captures/gp-*` will hold about 0.3–0.6 GB
  per capture after gzip (fight frames; not measured). The design assumes that
  is acceptable in the git-ignored `data/`. **Needs the user's agreement before
  U4 Task 2** (the first long capture; U1's `gp-pads` is short).
</content>
</invoke>
