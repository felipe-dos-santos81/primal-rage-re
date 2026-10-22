# Host Input Truth Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status:** closed 2026-10-22 — Tasks 1–7 landed on `host-input-truth`; the measured counter, the pins and the named gaps are record `docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md` §H7.

**Goal:** Make the host-owned input layer faithful — port the 12 pure sampler functions, map an SDL gamepad to the original's device arms, model presence/calibration/the boot probe, and record the movie-audio zero-payload finding.

**Architecture:** The host fills the BIOS key-state table and the four axis words; the ported sampler `0x1BBAC` is the only writer of `+0x2D8/+0x2D9`; `game_loop` calls it where it writes the bytes directly today. The six PIT/game-port routines and the 60 Hz AIL callback stay host-owned. The SDL pad feeds the device arms only when the config selects device 2/4/6.

**Tech Stack:** C (C11), SDL3, the existing `mem[]` model, the differential harness (`tools/diff_verify.py`, unicorn/capstone), DOSBox-X capture tooling, Ghidra/Rea for the raw.

**Spec:** `docs/superpowers/specs/2026-10-21-host-input-truth-design.md`

## Global Constraints

- **Prerequisite:** Gameplay Truth II is closed (the five captures taken or formally declined, record §J4–§J8, the AGENTS Commands lines, the full gate, merged to `main`). Branch `host-input-truth` is based on `main` after that merge.
- The 12 functions listed in Task 2–4 are the only new ported functions. The six PIT/game-port routines (`0x4FDF5`, `0x4FC05`, `0x4FCFD`, `0x4FBBB`, `0x5004A`, `0x50021`) and the 60 Hz AIL callback `0x1BDF4` stay host-owned.
- The default device 0/0 path must not move any capture pin; every existing oracle stays green.
- `0x2D2F0` and `0x4F644` keep their current bytes and differential rows; the game logic gains no joystick path.
- The SDL axis -> normalized unit scale is **host policy**, declared in the record; never a fitted constant.
- No game-port axis/button capture. The 12 differential rows and the unit tests are the arms' evidence; `gp-u17-device2` pins the device-word clear.
- The movie audio has zero payload on the shipped data; no implementation, no oracle.
- Every new pin is measured with provenance and fails at pin + 1; every new assertion is mutation-proven able to fail.
- `data/` is read-only; no capture without a user-opened session; nobody may type in the DOSBox-X window.

## Review Focus

- A pad connected while the config says keyboard (device 0) must not feed bits.
- A pad disconnected while the config says joystick must clear the device word.
- A rebind to a scancode outside the 26 letters + 10 pad keys must be sampled.
- The SDL axis at the dead-band edge (±0x1D/±0x1E) must not toggle.
- The F1/F2 keys must stay bit 0 in every device arm.

---

### Task 1: The deferred audio zero-payload record

**Files:**
- Create: `docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md`
- Modify: `tools/port_classification.txt:6-15,56-58`
- Modify: `port/spec/audio.md` (the movie-audio section)

**Interfaces:**
- Consumes: the raw movie chain `0x1C740` -> `0x6345C(..., 0xFE00)` -> `0x63180` -> `0x65240`; `tools/smk_info.py`.
- Produces: record §H1; the 13 classification rows' evidence lines.

- [ ] **Step 1: Run the payload check**

Run: `python3 tools/smk_info.py data/game/C/TWI5.SMK` and `python3 tools/smk_info.py data/game/C/TWG.SMK`
Expected for both: `audio_descriptors=[(0, 0), (0, 0), (0, 0), (0, 0), (0, 0), (0, 0), (0, 0)]` and `audio_frame_flags=[]`.

- [ ] **Step 2: Write record §H1**

In `docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md`, write §H1: the seven-track request at `0x1C76C-0x1C774`; `0x63180`'s bit-`0x40` gate at `0x6319D`; the zero descriptors; the no-stream result; `0x65240`'s no-audio time path at `0x65246`/`0x652BC..0x6532D`; the conclusion that no movie-audio oracle is possible on the shipped data. Correct `0x10678`'s label: it returns `[0x81E10]*4` milliseconds, not a buffer size.

- [ ] **Step 3: Update the 13 deferred rows and the audio spec**

In `tools/port_classification.txt`, change the 13 movie-audio rows' evidence from "no oracle" to "zero payload on the shipped data; no stream opens; §H1". In `port/spec/audio.md`, add the same finding to the movie-audio section.

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md tools/port_classification.txt port/spec/audio.md
git commit -m "docs: host input truth the movie-audio zero payload"
```

---

### Task 2: The keyboard readers and the host key-state table

**Files:**
- Modify: `port/src/platform/input.c`, `port/src/platform/input.h`
- Modify: `port/src/host.c:93-159`
- Modify: `port/tests/test_platform.c`
- Modify: `tools/diff_verify.py`
- Modify: `docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md`

**Interfaces:**
- Consumes: the BIOS key-state table at `GAME_BIOS_BASE+0x254`; the configured 16 key slots `+0x2DE..+0x2ED`; the default record.
- Produces: `u8 input_dirs_p1(void)`, `u8 input_dirs_p2(void)`, `u8 input_buttons_p1(void)`, `u8 input_buttons_p2(void)`, `u8 input_f1(void)`, `u8 input_f2(void)`; `void host_input_fill_keyboard(void)`.

- [ ] **Step 1: Add the six differential rows**

In `tools/diff_verify.py`, add rows for `0x1B610` (`input_dirs_p1`), `0x1B6A0` (`input_dirs_p2`), `0x1B730` (`input_buttons_p1`), `0x1B7C0` (`input_buttons_p2`), `0x1B850` (`input_f1`), `0x1B870` (`input_f2`), in the style of the existing `input_joystick_device`/`input_state_update` rows. Each binding seeds `GAME_BIOS_BASE+0x254` with the case's key-state bytes; the cases are:

| row | case input | expected |
|---|---|---|
| `0x1B610` | up / down / left / right key-state bits | `0x80` / `0x40` / `0x20` / `0x10` |
| `0x1B6A0` | the same P2 bits | `0x80` / `0x40` / `0x20` / `0x10` |
| `0x1B730` | buttons 1 / 2 / 3 / 4 | `1` / `2` / `4` / `8` |
| `0x1B7C0` | the same P2 buttons | `1` / `2` / `4` / `8` |
| `0x1B850` | F1 down / up | `1` / `0` |
| `0x1B870` | F2 down / up | `1` / `0` |

- [ ] **Step 2: Run the rows to verify they fail**

Run by spec name (the CLI's `--function` takes the spec name, not the address; record §H2):
`python3 tools/diff_verify.py --function input_dirs_p1` (then `input_dirs_p2`, `input_buttons_p1`,
`input_buttons_p2`, `input_f1`, `input_f2`)
Expected: FAIL — the row is unbound (no port function at the address).

- [ ] **Step 3: Implement the six readers and the host table**

In `port/src/platform/input.c`, implement the six functions with the raw's signatures and the bit rules in the table. In `port/src/host.c`, add `host_input_fill_keyboard()`: fill the BIOS table from the configured 16 slots (`+0x2DE..+0x2ED`, any scancode) plus the default record; change `host_key_bits` so a rebind outside the 26 letters + 10 pad scancodes is sampled. Keep `host_set_key_bits_override` working.

- [ ] **Step 4: Add the unit test**

In `port/tests/test_platform.c`, add `test_input_keyboard_readers`: seed the table, assert each reader's nibble/button; seed a configured slot with a scancode outside the 26+10 and assert it reaches the table; prove each assertion fails under a mutation of the reader or the table fill.

- [ ] **Step 5: Run the rows and the suite**

Run by spec name (record §H2): `python3 tools/diff_verify.py --function input_dirs_p1` (then the
other five); `cmake --build build && PR_ORACLE_REQUIRED=1 ./build/run_tests`
Expected: six rows VERIFIED; all checks passed.

- [ ] **Step 6: Append record §H2**

Record the six addresses, the row cases and the host table's scancode rule in `docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md`.

- [ ] **Step 7: Commit**

```bash
git add port/src/platform/input.c port/src/platform/input.h port/src/host.c port/tests/test_platform.c tools/diff_verify.py docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md
git commit -m "port: host input truth the keyboard readers"
```

---

### Task 3: The game-port button readers and the `0x1B890` row

**Files:**
- Modify: `port/src/platform/input.c`, `port/src/platform/input.h`
- Modify: `port/src/host.c`
- Modify: `port/tests/test_platform.c`
- Modify: `tools/diff_verify.py`
- Modify: `docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md`

**Interfaces:**
- Consumes: the game-port byte at `0x201`; the host pad state (Task 5).
- Produces: `u8 input_port_buttons_both(void)`, `u8 input_port_buttons_p1(void)`, `u8 input_port_buttons_p2(void)`; `u8 host_port_buttons(void)`.

- [ ] **Step 1: Extend the three rows**

Add rows for `0x1B890` (`input_port_buttons_both`), `0x1B8DC` (`input_port_buttons_p1`), `0x1B908` (`input_port_buttons_p2`). Use `E.Machine.in` for the `in al,dx` at `0x1B899`; the cases are:

| row | game-port byte | expected |
|---|---|---|
| `0x1B890` | bits 4..7 set | `1` / `2` / `4` / `8` |
| `0x1B8DC` | bits 4 / 5 | `1` / `4` |
| `0x1B908` | bits 6 / 7 | `1` / `4` |

`0x1B890` is the current `1 named gaps` row; its 9 blocks must all be hit.

- [ ] **Step 2: Run the rows to verify they fail**

Run by spec name (record §H3): `python3 tools/diff_verify.py --function input_port_buttons_both`
(then `input_port_buttons_p1`, `input_port_buttons_p2`)
Expected: FAIL — unbound.

- [ ] **Step 3: Implement the three readers and the host byte**

Implement the three functions with the raw's bit maps. `host_port_buttons()` returns the game-port byte from the host pad state (Task 5); until then, return 0 for no pad.

- [ ] **Step 4: Add the unit test**

In `port/tests/test_platform.c`, add `test_input_port_buttons`: seed the game-port byte, assert each reader; prove each assertion fails under a mutation of the bit map.

- [ ] **Step 5: Run the rows and the suite**

Run by spec name (record §H3): `python3 tools/diff_verify.py --function input_port_buttons_both`
(then the other two); `PR_ORACLE_REQUIRED=1 ./build/run_tests`
Expected: three rows VERIFIED; `1 named gaps` becomes `0`; all checks passed.

- [ ] **Step 6: Append record §H3**

Record the three addresses, the `in` model and the bit maps in the record.

- [ ] **Step 7: Commit**

```bash
git add port/src/platform/input.c port/src/platform/input.h port/src/host.c port/tests/test_platform.c tools/diff_verify.py docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md
git commit -m "port: host input truth the game-port buttons"
```

---

### Task 4: The axis readers, the sampler and the `game_loop` call

**Files:**
- Modify: `port/src/platform/input.c`, `port/src/platform/input.h`
- Modify: `port/src/game/flow.c:7117-7121`
- Modify: `port/src/host.c`
- Modify: `port/tests/test_platform.c`
- Modify: `tools/diff_verify.py`
- Modify: `docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md`

**Interfaces:**
- Consumes: the four axis words `0xE1C1E/0xE1C20/0xE1C26/0xE1C28`, the centers `0xE1C22/0xE1C24/0xE1C2A/0xE1C2C`; the device words `+0x2D4/+0x2D6`; the six keyboard readers (Task 2) and three port readers (Task 3).
- Produces: `u8 input_axes_a(void)`, `u8 input_axes_b(void)`, `void input_sampler(void)` (the `0x1BBAC`), `void host_input_fill_axes(void)`, the `game_loop` call.

- [ ] **Step 1: Add the three rows**

Add rows for `0x4FF8F` (`input_axes_a`), `0x4FFD8` (`input_axes_b`), `0x1BBAC` (`input_sampler`). The axis cases seed the axis words and centers and assert the `0x10/0x20/0x40/0x80` bits at `>= +0x1E` / `<= -0x1E` and the no-bit dead band. The sampler cases seed the device words and the six/three readers' inputs and assert `+0x2D8`/`+0x2D9` for devices 0, 2, 4 and 6, including F1/F2 bit 0.

- [ ] **Step 2: Run the rows to verify they fail**

Run by spec name (record §H4): `python3 tools/diff_verify.py --function input_axes_a` (then
`input_axes_b`, `input_sampler`)
Expected: FAIL — unbound.

- [ ] **Step 3: Implement the axis readers and the sampler**

Implement `input_axes_a`/`input_axes_b` with the raw's `(axis - center) >= 0x1E -> 0x10`, `<= -0x1E -> 0x20` (X) and `0x40/0x80` (Y). Implement `input_sampler` with the four arms in the design's table; it writes `+0x2D8`/`+0x2D9` and ORs F1/F2 into bit 0.

- [ ] **Step 4: Replace the direct write with the sampler**

In `port/src/game/flow.c:7117-7121`, replace the direct `+0x2D8`/`+0x2D9` write with `input_sampler()`; keep the `0x4F644` update. Add `host_input_fill_axes()` in `host.c` (the no-pad state writes the four axis words; Task 5 supplies the pad state), and call `host_input_fill_keyboard()` + `host_input_fill_axes()` before `input_sampler()`. The default device 0/0 output must equal the old direct write byte-for-byte.

- [ ] **Step 5: Add the unit test**

In `port/tests/test_platform.c`, add `test_input_sampler_device0` (the old direct write's bytes), `test_input_sampler_arms` (devices 2/4/6), `test_input_axes_dead_band` (the ±0x1D/±0x1E edges); prove each fails under a mutation of the sampler or the axis compare.

- [ ] **Step 6: Run the rows, the suite and the existing captures**

Run by spec name (record §H4): `python3 tools/diff_verify.py --function input_sampler` (then the
other two); `PR_ORACLE_REQUIRED=1 ./build/run_tests`; `make gp-oracle gp-twop-oracle gp-keys-oracle`
(the captures present)
Expected: three rows VERIFIED; all checks passed; every existing capture pin unchanged.

- [ ] **Step 7: Append record §H4**

Record the three addresses, the axis bits, the sampler's arms and the default device-0 proof in the record.

- [ ] **Step 8: Commit**

```bash
git add port/src/platform/input.c port/src/platform/input.h port/src/game/flow.c port/src/host.c port/tests/test_platform.c tools/diff_verify.py docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md
git commit -m "port: host input truth the sampler and the game loop"
```

---

### Task 5: The SDL gamepad model

**Files:**
- Modify: `port/src/host.c`
- Modify: `port/src/platform/input.c` (only if the host fill needs a hook)
- Modify: `port/tests/test_platform.c`
- Modify: `port/src/main.c` (only if the pad open needs the host init)
- Modify: `docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md`

**Interfaces:**
- Consumes: the device words `+0x2D4/+0x2D6`; the keyboard/port readers (Tasks 2–3); the axis words and centers (Task 4).
- Produces: `void host_pad_open(void)`, `void host_pad_close(void)`, `void host_pad_poll(void)`, `void host_pad_fill(void)`, `void host_set_pad_override(const u8 *axes, const u8 *buttons, int present)` (test seam).

- [ ] **Step 1: Add the failing pad tests**

In `port/tests/test_platform.c`, add:
`test_host_pad_device0_ignores_pad` (device 0, pad present, no bits);
`test_host_pad_arm2` (device 2: axes A + game-port bits 4..7 -> `1,2,4,8`);
`test_host_pad_arm4` (device 4: P1 bits 4/5 -> `1,4`, P2 axes B + bits 6/7 -> `1,4`);
`test_host_pad_arm6` (device 6: axes A/B + keyboard P1/P2 buttons);
`test_host_pad_dead_band` (axis at center ±0x1D no bit, ±0x1E the bit);
`test_host_pad_f1f2` (F1/F2 bit 0 in every arm).
Run them red; each must fail before the model exists.

- [ ] **Step 2: Implement the pad model**

In `port/src/host.c`, open the first SDL gamepad in `host_init` (close in `host_shutdown`), poll its axes/buttons each frame, and fill the BIOS table/axis words/game-port byte for the device arms. The device words gate the arms; device 0 ignores the pad; a joystick arm replaces that player's keyboard nibble (device 6 keeps the keyboard button scans). The SDL axis -> normalized unit scale is a named host policy constant, not a raw value.

- [ ] **Step 3: Run the pad tests and the suite**

Run: `cmake --build build && PR_ORACLE_REQUIRED=1 ./build/run_tests`
Expected: the six tests pass; all checks passed.

- [ ] **Step 4: Append record §H5**

Record the SDL pad mapping, the device-arm table and the axis-scale policy in the record.

- [ ] **Step 5: Commit**

```bash
git add port/src/host.c port/src/platform/input.c port/tests/test_platform.c port/src/main.c docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md
git commit -m "port: host input truth the SDL gamepad model"
```

---

### Task 6: Presence, calibration, the boot probe and `gp-u17-device2`

**Files:**
- Modify: `port/src/host.c`
- Modify: `port/src/game/flow.c` (the `0x20D0F..0x20DE3` and `0x5004A` paths)
- Modify: `port/src/platform/input.c` (the clear)
- Modify: `tools/gp_capture.py`, `tools/gp_compare.py`, `tools/gp_session.py`
- Modify: `Makefile`
- Modify: `port/tests/test_platform.c`
- Modify: `docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md`

**Interfaces:**
- Consumes: the pad model (Task 5); the `0x4FBBB` probe; `0x5004A`/`0x50021`; the device words.
- Produces: `int host_port_present(int probe)` (the `0x4FBBB`), `void host_pad_center(void)` (the `0x5004A`), the `dev1`/`dev2` poll fields, the `gp-u17-device2` scenario and oracle.

- [ ] **Step 1: Write the failing host tests**

Add `test_host_port_present` (a pad answers, no pad does not), `test_boot_probe_clears` (no pad -> the device word cleared), `test_host_pad_center` (the rest values become the centers). Run red.

- [ ] **Step 2: Implement presence, calibration and the boot probe**

Model `0x4FBBB` as `host_port_present`; wire `0x20D0F..0x20DE3` to it; clear the device word when no pad answers (`0x1BC6F`/`0x1BCB4`/`0x1BCE6`); implement `0x5004A`/`0x50021`'s center capture at pad connect and Alt-J. Keep the six routines host-owned.

- [ ] **Step 3: Add `dev1`/`dev2` and the scenario**

Add the two device words to the capture poll log and the port trace; the existing captures' poll logs lack the fields and their pins stay (the parser treats absent as absent). Add `gp-u17-device2` to `tools/gp_session.py`: the K11 service-menu walk to the device selection, select device 2, end 60 frames later. Add the empty `GP_U17_*` block and `gp-u17-oracle` to the Makefile, in `verify`.

- [ ] **Step 4: STOP — ask the user for a capture session**

The capture is user-gated. Do not run `make gp-capture` without a go-ahead. The user must not type in the DOSBox-X window.

- [ ] **Step 5: Capture and check the input**

Run: `make gp-capture scenario=gp-u17-device2`; `python3 tools/gp_capture.py check-input data/k11-captures/gp-u17-device2`
Expected: the capture completes; `check-input` is clean.

- [ ] **Step 6: Replay, measure and pin**

Run: `make gp-replay scenario=gp-u17-device2`; then fill `GP_U17_*` from the measured output (the device-word clear frame, the frame/trace ratchets, the reach, the sha256, the frame count), with the provenance in the Makefile comment. Run `make gp-u17-oracle`; expected: green, and pin + 1 fails.

- [ ] **Step 7: Append record §H6**

Record the presence model, the boot-probe clears, the calibration and the `gp-u17-device2` pins in the record.

- [ ] **Step 8: Commit**

```bash
git add port/src/host.c port/src/game/flow.c port/src/platform/input.c tools/gp_capture.py tools/gp_compare.py tools/gp_session.py Makefile port/tests/test_platform.c docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md
git commit -m "port: host input truth presence, calibration and the device-2 capture"
```

---

### Task 7: Closeout

**Files:**
- Modify: `README.md`, `AGENTS.md`, `docs/PROGRESS.md`
- Modify: `tools/port_classification.txt` (the 12 rows)
- Modify: `docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md`
- Modify: the plan/design status

**Interfaces:**
- Consumes: all previous tasks.
- Produces: the closeout commit; the re-measured counters.

- [ ] **Step 1: Re-measure every pin**

Re-run `make verify`; every existing oracle line and pin must be unchanged; the new `gp-u17-device2` pin must pass and fail at pin + 1. Re-run `python3 tools/port_progress.py`; expected `799 1203 66` and `759 759 100`. Re-run `make diff-verify`; expected +12 functions, + mutants, `1 named gaps` -> `0`, and the rows with callees re-measured.

- [ ] **Step 2: Update the durable docs**

Move the 12 rows in `tools/port_classification.txt` from host-owned to ported (the six PIT/game-port + `0x1BDF4` stay host-owned). Update README's title to 66% and its "N% of the original's D real functions" line. Add the `gp-u17-oracle` line to AGENTS' Commands list and the host-input paragraph (the 12 functions, the pad, the presence, the `0x1B890` row, the movie-audio record). Append §H7 to the record (the closeout, the counter, the named limits) and the PROGRESS paragraph with the measured pins or the named decline.

- [ ] **Step 3: Commit the closeout**

```bash
git add README.md AGENTS.md docs/PROGRESS.md tools/port_classification.txt docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md docs/superpowers/plans/2026-10-21-host-input-truth.md docs/superpowers/specs/2026-10-21-host-input-truth-design.md
git commit -m "docs: host input truth closeout"
```

---

## Self-Review

**Spec coverage:** Goals 1–5 map to Tasks 2–4 (the 12 functions and the sampler), Task 5 (the pad), Task 6 (presence/calibration/capture) and Task 7 (the closeout); the movie-audio record is Task 1. The six PIT/game-port routines and `0x1BDF4` stay host-owned in Task 6. The default device 0/0 constraint is Task 4's step 5 and Task 7's gate.

**Step scan:** Each step is one action with a checkable result; the row tables give the cases and expected bits; the C signatures and the test names are the decisions; the bodies are the raw's.

**Type consistency:** `input_dirs_p1`/`input_dirs_p2`/`input_buttons_p1`/`input_buttons_p2`/`input_f1`/`input_f2` (Task 2) are consumed by `input_sampler` (Task 4); `input_port_buttons_both`/`_p1`/`_p2` (Task 3) likewise; `host_port_buttons` (Task 3) is filled by `host_pad_fill` (Task 5); `host_input_fill_keyboard` (Task 2) and `host_input_fill_axes` (Task 4) are the `game_loop` fills, and `host_pad_fill` (Task 5) supplies the pad state to the axis fill; `host_port_present`/`host_pad_center` (Task 6) are the presence/calibration interfaces.

**Review Focus:** each of the five lines has a test: Task 5's `test_host_pad_device0_ignores_pad`, Task 6's `test_boot_probe_clears`, Task 2's rebind test, Task 5's `test_host_pad_dead_band`, Task 5's `test_host_pad_f1f2`.

**Proportion:** the plan is longer than the design because the 12 rows' cases and the arms' table are the decisions; the code bodies are the raw's, not transcribed.
