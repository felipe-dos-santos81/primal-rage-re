# Host Input Truth — Design

**Status:** closed 2026-10-22 — implemented as Tasks 1–6 and closed by Task 7; the measured
counters, the pins and the named limits are record `2026-10-21-host-input-truth-derivations.md`
§H7. Draft for review, 2026-10-21; the two capture-era statements are corrected below.
**Phase:** the next phase after Gameplay Truth II. Branch `host-input-truth`, based on `main` after the Gameplay Truth II closeout and merge.

## Why this phase

The portable game logic is 100% ported (747/747 at writing; **759/759** and **53** host-owned after this phase, record §H7). The remaining RE is the 65 host-owned/deferred functions and the 351 runtime functions. The last host-owned subsystem whose observable interface is modelled only approximately is **input**:

- The original's joystick is the host-owned ISR sampler `0x1BBAC` (ported by this phase, record §H4). The game logic has no joystick path: `0x2D2F0` is `xor eax,eax; ret`, and `0x4F644`'s `0x4F68B` gate never opens (`AGENTS.md:353-368`, record §J9). The sampler reads the keyboard or a game port through the device words `+0x2D4/+0x2D6` and writes the same key bitmap `+0x2D8/+0x2D9` the game reads.
- The port's host models only the sampler's **device-0 (keyboard)** arms (`host_kb_bit`, `host.c:93-136`; `host_key_bits`, `host.c:147-159`). There is no SDL gamepad code. The service menu stores the joystick device choice with no host effect. A rebind to a scan outside the 26 letters + 10 pad scancodes is stored in the config but never sampled.
- The movie-audio question is still open in the records. The 13 deferred movie-audio functions and the four AIL streaming stubs are inert, and `movie_play` never opens audio. The raw shows why: both shipped movies declare zero audio tracks, so no stream opens and the no-audio time path runs. That is a zero-payload finding, not an implementation gap.

This phase makes the host-owned input layer faithful to the sampler, adds the SDL gamepad map, presence/calibration/boot probe, and records the movie-audio zero-payload finding. It uses the 12 pure sampler functions as ported RE (with differential rows) and leaves the six PIT/game-port routines and the 60 Hz AIL callback host-owned.

## Goals

1. **Port the 12 pure input functions** with differential rows and call the sampler from `game_loop`:
   `0x1B610`, `0x1B6A0`, `0x1B730`, `0x1B7C0`, `0x1B850`, `0x1B870`, `0x1B890`, `0x1B8DC`, `0x1B908`, `0x4FF8F`, `0x4FFD8`, `0x1BBAC`.
   The host fills the BIOS key-state table (`GAME_BIOS_BASE+0x254`) and the four axis words; the sampler writes `+0x2D8/+0x2D9`. The default device 0/0 path must be byte-identical to today's direct write.
2. **The SDL gamepad map**: device words 2/4/6 select the original's arms; the arms' button positions, the ±0x1E dead band, F1/F2 bit 0, and the device-word gating are faithful, not a naive additive OR.
3. **Presence, calibration and the boot probe**: model `0x4FBBB`; wire `0x20D0F..0x20DE3`; implement `0x5004A`/`0x50021`'s center capture; clear a joystick device word when no pad answers; add `dev1`/`dev2` to the capture poll log and a K11-style `gp-u17-device2` capture that pins the clear.
4. **Record the movie-audio zero-payload finding** and correct `0x10678`'s label. No movie-audio implementation.
5. **No existing pin moves**: the `0x2D2F0`/`0x4F644` differential rows stay unchanged; the game logic gains no joystick path; every existing capture oracle stays green.

## Non-goals

- No game-logic joystick path; `0x2D2F0` and `0x4F644` keep their current bytes and rows.
- No movie-audio implementation; no rendered-audio audibility oracle; no OPL/DAC model comparison.
- No DPMI, allocator, CD, video, fatal-exit or runtime port. The 351 runtime functions stay served by the host libc.
- No differential grunt batch (the 58 open rows / 8 unrowed / 12 weaker rows are a separate audit, mostly bookkeeping).
- No packaging, CI, release build, save-path documentation or performance work; those are a later host/product phase.
- No game-port axis/button capture; the capture harness cannot inject game-port axes/buttons per frame.

## Anti-goals

- A pad that feeds bits while the config says keyboard (device 0).
- A keyboard that can steer a player configured for a joystick.
- A dead joystick configuration that keeps working with no pad attached.
- A fitted SDL axis scale presented as a raw constant.
- A capture pin moved by the default device-0 path, or by a `POLL_FIELDS` addition that changes the existing captures' meaning.

## The raw evidence

### The sampler and the device arms

`0x1BBAC` switches on the two device words (`0x1BBB6`/`0x1BBDC`; jump tables `0x1BB74`/`0x1BB90`) and builds P1's `+0x2D8` and P2's `+0x2D9`. The arms are not symmetric:

| dev | P1 (`+0x2D4`) | P2 (`+0x2D6`) |
|---|---|---|
| 0 | keyboard directions `0x1B610` -> `0x80/0x40/0x20/0x10`; buttons `0x1B730` -> `1/2/4/8` | keyboard directions `0x1B6A0`; buttons `0x1B7C0` |
| 2 | axes A `0x4FF8F`; game-port bits 4..7 `0x1B890` -> `1,2,4,8` | axes A; the same buttons `0x1B890` |
| 4 | axes A; game-port bits 4/5 `0x1B8DC` -> `1,4` | axes B `0x4FFD8`; game-port bits 6/7 `0x1B908` -> `1,4` |
| 6 | axes A; **keyboard** P1 buttons `0x1B730` | axes B; **keyboard** P2 buttons `0x1B7C0` |

F1/F2 are OR'd into bit 0 of the respective byte in every arm (`0x1B850`/`0x1B870`; `0x1BD53`/`0x1BDE0`). The axis bits are `(axis - center) >= 0x1E -> 0x10`, `<= -0x1E -> 0x20` for X, and `0x40/0x80` for Y (`0x4FF92..0x4FFD4`, `0x4FFDB..0x5001D`): a ±0x1D dead band around the calibrated center, both axes independent (8-way). `0x4FDF5` normalizes each timed axis to `((raw - previous) mod PIT_max) >> 4` before storing `0xE1C1E/20/26/28`; `0x5004A` stores the rest values as centers `0xE1C22/24/2A/2C`.

### Presence and calibration

The sampler checks the game port every tick: `0x4FDF5`/`0x4FC05`/`0x4FCFD` return 0 when no port answers and the device word is cleared (`0x1BC6F`, `0x1BCB4`, `0x1BCE6`). At boot, `0x20DAC` calls `0x4FBBB(3)` and `0x20DD1` calls `0x4FBBB(0xC)` (a `cli` write-0/read loop on `0x201`) and clears the device word on failure; both device 2 clears P1's word (`0x20D6F`). `0x5004A` waits for each axis to read `>= 0x1E` and copies the rest values into the centers (`0x50055-0x500B8`). Alt-J's calibration path is `0x5004A`; the port currently makes it a no-op (`flow.c:7385-7390`).

### The movie audio

`0x1C740` asks `SmackOpen` for all seven tracks (`0x1C76C-0x1C774`: flags `0xFE00`). `0x6345C` starts the 250 Hz timer (`0x63468` -> `0x10610`), reads the ms clock (`0x6346D` -> `0x10678`, which returns `[0x81E10]*4`), loops the requested tracks (`0x63992..0x63A2C`) calling `0x63180`; `0x63180` opens a stream only when bit `0x40` of the track's flag dword is set (`0x6319D`). Both shipped movies declare zero audio tracks: `tools/smk_info.py` prints `audio_descriptors=[(0,0) x7]` and `audio_frame_flags=[]` for TWI5.SMK (121 frames) and TWG.SMK (41). No stream opens, `0x102B8` walks an empty list, and `0x65240` takes the no-audio time path (`0x65246`, `0x652BC..0x6532D`). The port's `movie.c` plays silent through the time path. A movie-audio oracle is impossible on the shipped data; a synthetic movie would test a movie the game never plays.

### The port today

- `host_kb_bit` (`host.c:93-136`) maps one set-1 scan to the configured 16-slot key table (`+0x2DE..+0x2ED`) or the default record; `host_key_bits` (`host.c:147-159`) ORs the SDL keyboard over the 26 letters + 10 pad keys only.
- `game_loop` writes both bytes unconditionally each frame (`flow.c:7117-7121`). Nothing in the input path reads `+0x2D4/+0x2D6`; the service menu reads them only to draw the selection (`svcmenu.c:978-985`).
- `input_joystick_device` (`input.c:64-69`) returns 0; `input_state_update` (`input.c:110-130`) stores 0 and its merge gate never opens.
- `0x20D0F..0x20DE3` is omitted by name (`flow.c:6980-6985`, named gap B.11); `0x5004A` is a no-op on Alt-J (`flow.c:7385-7390`); `0x4FBBB` has no port counterpart.
- No SDL gamepad code exists. A rebind to a scan outside the 26 letters + 10 pad scancodes is stored in the config but never sampled.

## Decisions

1. **Port the 12 pure functions; keep the six PIT/game-port routines and `0x1BDF4` host-owned.** The 12 are pure logic over the BIOS key-state table, the game-port byte and the axis words; they are in the decomp index and row-able. The six (`0x4FDF5`, `0x4FC05`, `0x4FCFD`, `0x4FBBB`, `0x5004A`, `0x50021`) and the 60 Hz AIL callback `0x1BDF4` stay host-owned because their observable interface is the host's answer (timing, presence, calibration), not a byte sequence to port.
2. **The host fills the inputs; the sampler is the only writer of `+0x2D8/+0x2D9`.** `game_loop` calls the sampler where it currently writes the bytes directly. The sampler's device-0 arms read the BIOS key-state table and the keyboard's buttons; the axis arms read the four axis words. The host fills the table and the axis words from SDL.
3. **The SDL pad model is faithful to the arms, not additive.** A pad feeds the device arms only when the config selects device 2/4/6; device 0 ignores it. A joystick arm replaces that player's keyboard direction/button nibble (device 6 keeps only the keyboard *button* scans). The button positions are the arms' (device 4/6 put the second button on nibble bit 2; P2 device 4 uses game-port bits 6/7). F1/F2 stay bit 0 in every arm. The SDL axis -> normalized unit scale is **host policy**, declared, not a fitted constant.
4. **Presence, calibration and the boot probe are in scope.** A pad that answers the probe keeps its device word; a pad that does not is cleared on the next tick and at boot. The center capture is the rest values at pad connect and on Alt-J. The K11-style capture pins the clear — no pad answers the capture's run (the harness cannot inject game-port axes or buttons), so selecting device 2 must clear the word; the capture confirms it (record §H6.6).
5. **The movie-audio finding is a record, not an implementation.** Both shipped movies have zero audio payload; the 13 deferred functions stay inert; `0x10678` is relabelled as the ms reader (`[0x81E10]*4`), not a buffer size.
6. **One new capture, `gp-u17-device2`, not a `gp-*` game-port capture.** The harness can inject keyboard but not game-port axes/buttons. The 12 rows + the unit tests are the arms' evidence; the capture pins the device-word clear. `dev1`/`dev2` are added to the capture poll log and the port trace; the existing captures' poll logs lack the fields and their pins stay (the parser treats absent fields as absent, and the new scenario is the only one that asserts them).
7. **The `0x1B890` named gap becomes a row.** Its `in al,dx` at `0x1B899` is answered by track H's `E.Machine.in` model; the row's 9 blocks become hit, and the row's claim is the game-port byte -> `1/2/4/8`.
8. **The default device 0/0 path must not move any pin.** The shipped config and the all-zero CMOS select device 0/0. The sampler's device-0 output is byte-identical to the current direct write; the full gate and every existing capture oracle prove it.

## What's left to the builder

- The exact SDL button/axis mapping (which SDL button is the game's button 1..4; the axis -> normalized-unit scale policy).
- The `host.c` function names and signatures for the pad model and the presence/calibration seam.
- The poll-field compatibility mechanism (absent fields in the existing captures; `dev1`/`dev2` only asserted by `gp-u17-device2`).
- The `gp-u17-device2` scenario's service-menu path and the exact keys to select device 2.
- The 12 C signatures from the raw (the differential rows' `E.Call` names the registers; the port functions take the raw's arguments).
- The tests' mutation proofs for each new assertion.

## Acceptance

1. The 12 functions are ported with differential rows; the sampler is called from `game_loop`; the device 0/0 output is byte-identical to the old direct write; every existing capture oracle and pin is unchanged.
2. The SDL pad model maps the device arms; unit tests cover the gating, the arms, the ±0x1E dead band, F1/F2 and the button positions; the device-0 path ignores the pad.
3. The `gp-u17-device2` capture pins the device-word clear; the existing captures are unchanged; the new pins are measured with provenance and fail at pin + 1.
4. The movie-audio zero-payload finding is recorded; `0x10678` is relabelled; no movie-audio code is added.
5. The closeout runs `make verify` green; `port_progress.py` reads 799/1203 (66%) raw and 759/759 (100%) portable; README, AGENTS and PROGRESS are updated; the 12 classification rows move from host-owned to ported.

## Risks and named gaps

- **The SDL axis scale is host policy.** The raw gives the normalized unit and the ±0x1E threshold, not the SDL axis range. Declare the scale; do not present it as a raw constant.
- **No game-port capture.** The arms' evidence is the rows and unit tests; the capture only pins the clear. The presence model is a host answer; the K11 capture pins the clear, not the pad's answer.
- **The `0x1B890` `in` model.** Track H models `in` from a per-case byte list; the row's game-port byte is the case's input. The row's claim is the byte-to-bit map, not a real game port.
- **The poll fields.** Adding `dev1`/`dev2` to the poll log must not change the existing captures' pins; the parser treats absent fields as absent and only the new scenario asserts them.
- **The six PIT/game-port routines and `0x1BDF4` stay host-owned.** They are not ported; the host interface is modelled. The classification rows for them stay host-owned.
- **The default device 0/0 path.** A refactor of `host_key_bits` or the sampler call can move the default capture pins; the full gate is the proof.

## My calls

- The phase is input + the movie-audio record. Packaging/CI/audio audibility are a later host phase; the differential grunt batch is a separate audit.
- The 12 pure functions are ported; the six PIT/game-port + `0x1BDF4` stay host-owned.
- The SDL pad map is faithful to the sampler's arms; the axis scale is host policy.
- Presence, calibration and the boot probe are in scope.
- The new capture is `gp-u17-device2`; there is no game-port capture.
- The default device 0/0 path must not move any pin.
- The phase starts after the current Gameplay Truth II closeout and merge.
