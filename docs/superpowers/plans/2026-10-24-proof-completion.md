# Proof Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every proof claim in the repository match its evidence: close the honestly closable differential rows, prove every reachable span-writer class/entry, claim order and coverage in the capture oracles, make the gate report what it actually checked, and remove the docs contradictions — without a new capture, a new portable function, or a player-visible behaviour change.

**Architecture:** Three offline workstreams on existing infrastructure. The differential work extends `tools/diff_verify.py`'s `render_list_blit` payloads, adds two specs, converts one stub to an allow, moves eleven unreached longjmp declarations to a scan-only `Spec.stop` set, and adds one short-write case (which reopens `config_file_save` on the runtime flush). The oracle work extends `tools/gp_compare.py` with an existential occurrence-aware order rule and a canonical `--max-missing` ratchet, then adds K11 identity pins and allowance byte/stale checks. The gate work adds a `gate-check` preflight and a `PR_PINS_EXACT` mode to the Makefile, plus unit-suite hygiene. The docs pass is last, after the counters are measured.

**Tech Stack:** Python 3 (the diff harness, the capture comparators, the tests), the C/SDL3 port, GNU Make, the raw `PRAGE.EXE` bytes and the committed Ghidra decompilation, the existing git-ignored captures.

**Spec:** `docs/superpowers/specs/2026-10-24-proof-completion-design.md`

## Global Constraints

- Work from the repo root. `make`, `run_tests` and the tools resolve `data/game/C` and `data/k11-captures/` relative to it.
- **No new capture and no re-capture.** The order/coverage pins are measured on the existing dumps and captures. `data/game/` is read-only; the capture directories are read-only for this phase.
- **No new portable game code.** The only functional `port/src` edit is the `0x42203`/`0x4221C` divide guard and its test; Task 5's `actors.c` change is a comment correction. If a span case exposes a real port divergence, fix it with the raw instruction, a `PORT:` note, a row case or a unit test, and the full gate; a divergence that cannot be fixed stays a named gap.
- **Every new pin is measured from the tool's own output and proved to fail one step in the tested direction.** The arithmetic in this plan is a check, not a target. No fitted constant.
- **No existing pixel/state pin moves** except the phase's own allowances/pins and the `0x51ED8`, `0x249F0` and `0x1D0BC` weaker reclassifications. Any unexpected movement stops the task and is recorded.
- The differential counter is re-measured from `make diff-verify`'s line, never predicted. The expected values below are arithmetic checks; the record quotes the measured line.
- `python3 tools/port_progress.py` must stay `799 1203 66` / `759 759 100`; no task adds an `FN_` function.
- `make entry-triage` must stay byte-identical (`--expect 579 --expect-u0 575`); the E2 universe is not extended. The 158 untriaged span targets are a separate census.
- The full gate is `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify` (the Makefile exports the two mode variables so the Python tools see them); every capture except the declared U14 named gap must be present for the final run.
- Docs updated: `docs/PROGRESS.md`, `README.md`, `AGENTS.md`, `port/RE_GUIDE.md`, `port/spec/game_flow.md`, the affected specs/Makefile comments, and the derivation record. Commit style `<area>: <what>`; stage named files, never `git add -A`; commit only when the user authorizes the phase's commits.

## Review Focus

The spec implies these failure modes; each gets a test in the task named.

1. **A span class claimed but not reached.** The minimal fixture's coverage unittest runs the original with the writers as `real` calls and asserts the arrived target set. A payload edit that drops a class fails it (Task 2).
2. **A row closed by moving a declaration a case actually reaches.** Before moving `D5H_LONGJMP` to `Spec.stop`, measure that no case arrives at `0x65431`; a synthetic case that enters `0x25201` must fail closed as an unmodeled call, not be stubbed (Task 5).
3. **An order rule that launders a rewind.** A degenerate splice (no byte after the boundary differs from the **low** frame) must not rescue a clean rewind; the existing `[1,2,3]` / `[rgb(1),rgb(3),rgb(2),rgb(1)]` fixture must fail (Task 8).
4. **A coverage pin that hides a missing class.** Canonicalise byte-identical frames; `+1` fails, `-1` prints "lower", an unpinned scenario fails, and all-black frames stay excluded (Task 9).
5. **A strict gate that silently skips.** `gate-check` fails on an undeclared absence with `GATE_STRICT=1`; `GATE_STRICT=0` is the documented partial mode; the exported `PR_PINS_EXACT=1` has no "improved:" line (Task 10).
6. **Docs quoting a predicted counter.** The docs pass lands after the measured line; every number in README/AGENTS/PROGRESS is copied from the driver/tool output (Task 11).

---

### Task 1: Evidence baseline and span-writer census

**Files:**
- Create: `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md`
- Read: `tools/diff_verify.py`, `tools/diff_emu.py`, `tools/tests/test_diff_verify.py`, `port/src/platform/sprite.c`, `data/game/C/PRAGE.EXE`, `port/decomp/prage.c`, `port/decomp/prage.functions.csv`
- Modify: `tools/tests/test_diff_verify.py` (the span-table census test)

**Interfaces:**
- Produces: record §P1 (the measured baseline: `port_progress`, the diff counter, the capture inventory, the 24 oracle pins) and §P2 (the span-table census: 417 slots, 392 distinct targets, six renderers, one `ret`, 385 writers (384 reachable + 1 dead); 27 classes, 26 reachable, one dead; 227 E2-triaged, 158 untriaged; `0x80E0C[128] = 0x57E3C` dead; `0x81110[128] = 0x81310[0] = 0x5D110` reachable; `0x24B54` live; `0x24B08` dead). Later tasks cite §P1/§P2.

- [ ] **Step 1: Record the baseline.** Run and paste into §P1:
  ```bash
  python3 tools/port_progress.py
  make diff-verify
  ```
  Also record the current `tools/tests/test_diff_verify.py` counter pin, the `Makefile`'s 35 capture paths and which 3 are enforced (smk/twi5, smk/twg, title) versus compared-if-present (`title2`, front-end), the 24 gp scenario pins, and the K11/front-end/attract pins. This is the "before" table the phase's closure compares against.

- [ ] **Step 2: Decode the span tables.** From the fixup-applied image, dump `0x80C8C`, `0x80D0C`, `0x80E0C`, `0x81010`, `0x81110` and classify each entry by the renderer's control-byte/index formula. Confirm 417 slots / 392 distinct / six renderers / `0x51E58` / 385 writers (384 reachable + 1 dead), and the 27 class table (fill/literal × forward/backward × n<4, 4 mod 4, 1 mod 4, 2 mod 4, 3 mod 4, 14/15 word tail, 128 chunk, `ret`). Record the index formula, the 27-entry class key and the representative target per class.

- [ ] **Step 3: Confirm the dead/reachable edge cases.** Confirm `0x80E0C[128] = 0x57E3C` is unreachable (literal control bytes are 7-bit; straddle remainders ≤ 127) and `0x81110[128]` reads `0x81310[0] = 0x5D110` (a 128-byte writer). Record the type ≥ 0x20 domain exclusion and the bank-0 policy (`sprite.h` maps bank 0 to no offset).

- [ ] **Step 4: Confirm the live/dead pair in `0x24B08..0x24C5C`.** Disassemble `0x24B08` (dead: preceded by `ret` at `0x24B07`, no rel32/dword/fixup, not in the index) and `0x24B54` (live: installed by `0x25AE8` into `DS_00104AE4`, called by the ported hook dispatcher, `b1d` 01/02/03/04 in six gp captures). Record `0x24B8C`'s 52-entry table as the gap's third occupant. Correct the record's raw-vs-runtime notation (`0x888D0` → `DS_001088D0`, etc.).

- [ ] **Step 5: Add the census test.** In `tools/tests/test_diff_verify.py`, add a test that decodes the five tables from the **fixup-applied image** (`build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/e2img`, or the same `mem_load_le` + `mem_load_le_fixups` path) and asserts the counts/classes above; skip without the EXE/image. It must not read the EXE at import time. Run:
  ```bash
  python3 -m unittest tools.tests.test_diff_verify
  ```

- [ ] **Step 6: Write §P1/§P2 and commit.** No code, no pin. Commit (when authorized): `docs: baseline the proof completion phase`.

---

### Task 2: Minimal class-complete span-writer proof

**Files:**
- Modify: `tools/diff_verify.py` (`D5B_BLIT_ALLOW`, `render_list_blit` around line 26733)
- Modify: `tools/tests/test_diff_verify.py` (the coverage test and the `D1_CASES`/mutant pins)
- Modify: `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` (§P3)
- Read: `port/src/platform/sprite.c` (`copy_run` line 114, `rle_row` line 175, `sprite_render_*` lines 271–355)

**Interfaces:**
- Consumes: the `d5b_bl` helper/resource region and the existing `render_list_blit` b0–b9 cases.
- Produces: three cases in `render_list_blit` (normal 85×1 RLE, hflip 85×1 RLE, raw 128) that reach all 26 live writer classes; 37 new allow targets; a coverage unittest that proves the class set; re-measured `D1_CASES`/mutant pins.

- [ ] **Step 1: Write the failing coverage test.** Add a test that runs the original through `E.run_original` on the three new payloads with the writer families as `real` calls and asserts the arrived **target** set covers every live class. (`E.Call` records the target address, not the table slot; the 25 duplicate slots are a separate count.) Expected: FAIL (the cases do not exist).

- [ ] **Step 2: Add the minimal RLE stream.** Use the measured 56-byte stream (literal 0..7, fill 0..7/14/15):
  ```
  00
  01 13
  02 14 14
  03 15 15 15
  04 16 16 16 16
  05 17 17 17 17 17
  06 18 18 18 18 18 18
  07 19 19 19 19 19 19 19
  80 21
  81 22
  82 23
  83 24
  84 25
  85 26
  86 27
  87 28
  8E 2F
  8F 30
  ```
  Three psets: normal `0x0230` at `(10,2)` type 1; hflip `0x8230` at `(84,2)` type 9; raw `0x0231` at `(10,2)` with descriptor width `-128`/height `-1`, 128 source bytes, type 2. Use palette bank byte `0xFE` (bank offset `0xFD`) so the dword carries make the word-tail classes observable. The pokes fit `D4B_RES2` (`0x10AA00..0x10B0D0`).

- [ ] **Step 3: Add the allow addresses.** Add the 37 writer targets reached by the three cases to `D5B_BLIT_ALLOW` (the existing 13 overlap). Do not add `0x57E3C` (dead). The row remains allow-mode for the writers; the coverage test is the claim that each arrived.

- [ ] **Step 4: Re-measure the pins.** Run `make diff-verify` and the unittest; update `D1_CASES`/the mutant case lists at `tools/tests/test_diff_verify.py:21747-21757` from the failure output. Confirm `@bank`, `@rowtab` and `@nomirror` are still caught; if a tail/carry mutant is not, add a case rather than weakening a pin.

- [ ] **Step 5: Prove the test can fail.** Temporarily remove one control byte (e.g. the `8E 2F` fill 14), run the coverage test, confirm it reports the missing class, and restore. Record the mutation in §P3.

- [ ] **Step 6: Run and record.** `make build` is not needed (no C change). Run `make diff-verify`; the counter must be unchanged. Commit (when authorized): `diff: prove every span-writer class`.

---

### Task 3: Exhaustive span-writer proof and both-edges clipped renderers

**Files:**
- Modify: `tools/diff_verify.py` (`D5B_BLIT_ALLOW`, `render_list_blit` cases)
- Modify: `tools/tests/test_diff_verify.py` (coverage test, `D1_CASES`, mutant pins)
- Modify: `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` (§P4)

**Interfaces:**
- Consumes: Task 2's helper and coverage test.
- Produces: a 37-row / 8,434-byte deterministic RLE stream and a 128-pset raw sweep that reach all **384 reachable** writer targets (385 total, one dead); 18 both-edges clipped psets that reach `0x5D580`/`0x582F4`; optionally the shear 128/top-clip case. The class proof remains even if the exhaustive case is deferred; the phase gate treats 384/385 and both-clip as required.

- [ ] **Step 1: Generate the exhaustive RLE stream.** Pack literal runs 0..127 and fill runs 0..63 into 320-wide rows (greedy, transparent padding): 37 rows, 8,434 bytes. Commit the generator (or pin the stream's length and sha256) so the 384/385 claim is reproducible. Place the descriptor/stream at `0x100000`/`0x102000` through resource entry 3 (`0x10A63C`), list nodes/psets at `0x0F0000`/`0x0F1000`, avoiding `0xF0AEC`. Normal pset `0x0210` at `(0,0)`; hflip `0x8210` at `(319,100)`; bank `0xFE`; row table `y*320+0x40` so `@rowtab` remains discriminating.

- [ ] **Step 2: Generate the raw sweep.** Descriptor width `-128`/height `-1`, 128 source bytes, 128 psets at `x=-k, y=k`, `k=0..127`, so `vis=128-k` reaches `0x81110[1..128]` including `0x5D110`.

- [ ] **Step 3: Add the both-edges clipped cases.** A width-330, two-row RLE sprite at `x=-a` for `a=1..9` (normal: `clip_l=a`, `clip_r=10-a`; hflip at screen x `329-a`, `xorg=329`, node.x=-a), with leading/trailing 16-pixel runs so visible lengths 15/14 exercise the word tail through the clipped paths. Reaches `0x5D580` (normal both-clip) and `0x582F4` (hflip both-clip).

- [ ] **Step 4: Extend the allow list and coverage test.** Add all 384 reachable targets and the renderer both-clip entries to the allow mode; extend the coverage test to assert 384/385 (the dead `0x57E3C` named) and the two both-clip addresses. If a target is unreachable in the fixture, fix the payload or record a named exclusion; do not add an address that the fixture cannot reach.

- [ ] **Step 5: Re-measure and run.** Update `D1_CASES`/mutant pins; run `make diff-verify`; the counter must be unchanged. If a case exposes a real port divergence, stop and write a new task/record section; do not change the payload to hide it.

- [ ] **Step 6: Record and commit.** §P4 records the generator/hash, the psets, the 384/385 reached set, the mutations and the 1 dead writer. Commit (when authorized): `diff: prove every reachable span-writer target and clip path`.

---

### Task 4: Row `0x25FAC` and `0x26194`

**Files:**
- Modify: `tools/diff_verify.py` (two `Spec`s, the allow/stub lists)
- Modify: `port/tests/diff_runner.c` (the two bindings and their mutant branches)
- Modify: `tools/tests/test_diff_verify.py` (name/mask/kind/case pins)
- Modify: `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` (§P5)

**Interfaces:**
- Consumes: `flow_card_ramp_step` (`0x25FAC`, `PR_SEAM0`, leaf) and `flow_bonus_card_b_step` (`0x26194`, `PR_SEAM0`, callees `0x2BC30` and `0x2B150`).
- Produces: two VERIFIED rows; `flow_bonus_card_b_step` closed; `game_frame_head` loses its two e2-field blockers (it still needs Task 5's scan-only stop to close).

- [ ] **Step 1: Write the `flow_card_ramp_step` cases.** Cover below the clamp (`+0x2C = 0xFF0`), at `0x1000`, above `0x8010`, and the byte wrap `0xFF10`; mutants `@nocarry`, `@signed`, `@bit`; EAX mask 0. Confirm the row's blocks all execute.

- [ ] **Step 2: Write the `flow_bonus_card_b_step` cases.** State 0 below/at the clamp; state 1 timer > 0 and = 0; state 2 w≠0 and = 0; state `0x80`. Stub `0x2BC30` with EAX/EDX/s0, pop 4, clobbers EDX; stub `0x2B150` with EAX. Mutants for the clamp/timer/state/bit paths. The row should be 8/8 blocks and closed because both callees have VERIFIED rows.

- [ ] **Step 3: Add the rows and re-measure.** Add the two `Spec`s, the two `diff_runner.c` bindings and their mutant branches, update the exact-name/mask/kind/case pins, and run:
  ```bash
  make diff-verify
  python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE --function flow_card_ramp_step --self-check
  python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE --function flow_bonus_card_b_step --self-check
  ```
  Expected: both VERIFIED, the expected mutant counts, no other row moves.

- [ ] **Step 4: Record and commit.** §P5 records the cases, mutants, arrival set and the `game_frame_head` blocker change. Commit (when authorized): `diff: row the two ported card steps`.

---

### Task 5: `0x65546` allow and the `0x65431` scan-only stop

**Files:**
- Modify: `tools/diff_verify.py` (`text_number_core`'s spec, the `Spec` dataclass/`static_scan` `stop` field, `D5H_GF_COMMON`/`D5H_LONGJMP`, the 11 specs)
- Modify: `tools/tests/test_diff_verify.py` (counter string, EAX-mask/real-name/mutant/stub-clobber pins, `C3F_KINDS`/`C3F_CASES`)
- Modify: `port/src/game/actors.c` (the stale `host_sprintf` comment)
- Modify: `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` (§P6)

**Interfaces:**
- Consumes: `D3A_SPRINTF = (0x65546, 0x6C8EB, 0x6CB6C, 0x6CC9F, 0x6CE84, 0x6CCFA, 0x65533, 0x72D20, 0x72CD6)`; `D5H_LONGJMP = E.Call(0x65431, ("eax","edx"), clobbers=("ebx","ecx","edx","esi","edi","ebp"))`; the 11 specs that use it; `spec.calls` is also `static_scan`'s stop set.
- Produces: `text_number_core` closed with the real sprintf tree; 11 rows closed (all 11 weak: `game_quit_prompt` joins `WEAKER` for its `0x24A9C` unhit restart arm); the `0x65546` stub-clobber pin removed; `C3F` kinds re-measured.

- [ ] **Step 1: Prove the sprintf allow.** Run the `text_number_core` row with `0x65546` as `allow_calls=D3A_SPRINTF` (not a stub) and confirm the port's formatting matches on the existing cases. If a case's format string is not exercised, add one from the raw's `0x2EF24` call sites; do not weaken the row. Update the stale `host_sprintf` comment in `actors.c` (D3A runs it in the `flow_26d4c`/`config_key_name` rows). Record the block/mutant result.

- [ ] **Step 2: Prove no case arrives at `0x65431`.** Instrument a one-off run (a breakpoint or a recorded call) over the 11 specs' cases and confirm zero arrivals at `0x65431`. The existing `unhit_named` blocks (`0x25201`, `0x24A9C`) name the restart arms; the port's `game_restart_longjmp` would `exit(1)` with no restart point, so a future arrival must fail closed, not be stubbed.

- [ ] **Step 3: Move the declaration to `Spec.stop`.** Add a scan-only `stop` field to `Spec` and have `static_scan` honor it; keep `0x65431` in `stop` while removing `D5H_LONGJMP` from the 11 specs' **callee** list (not from the row bodies, not from `unhit_named`). If `D5H_GF_COMMON`/`D6A_PROMPT_CALLS` are shared with a row outside the 11, split the tuple so only those rows lose the callee. Without `stop`, the scan follows the `0x2520B`/`0x24AB0` tail jumps into nine runtime leaders (`0x65431`, `0x6544F`, `0x65450`, `0x65468`, `0x6546A`, `0x65475`, `0x65477`, `0x65482`, `0x65484`), making `game_frame_head` 170 leaders and `game_quit_prompt` 27, and the rows `PARTIAL`. Add a guard test that a synthetic case entering `0x25201` now fails as an unmodeled call (rather than being stubbed). Re-measure the counter; expected `826/845` closed, `19` open at this point.

- [ ] **Step 4: Re-measure the pins.** Update the counter string and the EAX-mask/real-name/mutant/stub-clobber dictionaries; remove `0x65546` from the stub-clobber dict; add `0x249F0` to `WEAKER`; re-measure `C3F_KINDS`/`C3F_CASES` (`@val` moves to a byte, `@call` loses the call). Run the full `tools/tests/test_diff_verify.py`.

- [ ] **Step 5: Record and commit.** §P6 records the measured zero arrivals, the scan-only stop, the sprintf comparison, the 12 closed rows (1 full, 11 weak) and the re-measured line. Commit (when authorized): `diff: close the sprintf and scan-stop rows`.

---

### Task 6: The short-write block and the `0x51ED8` classification

**Files:**
- Modify: `tools/diff_emu.py` (`Machine`/`_fwrite`)
- Modify: `tools/diff_verify.py` (`config_file_save`'s `s5` case, `WEAKER`)
- Modify: `tools/tests/test_diff_verify.py` (`SC_KINDS`/`SC_CASES`, the counter string, the weaker comment)
- Modify: `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` (§P7)

**Interfaces:**
- Consumes: `config_file_save` (`0x1B084`, `WEAKER`, 7/8 blocks) and `0x1B0F7` (the short-write exit arm, whose exit chain flushes through `0x6C4CF`); `0x51ED8` (aperture blit, one declared byte difference, currently counted full); `0x249F0` and `0x1D0BC` (unhit restart/buffer arms, not in `WEAKER`).
- Produces: `0x1B084` 8/8 with a terminal `s5` case, but **reopened** on the `0x6C4CF` flush (a named terminal); `0x51ED8` and `0x1D0BC` in `WEAKER` (`0x249F0` was added in Task 5); the counter's full/weaker figures re-measured at 825/845 closed, 807/845 full, 20 open, 15 weaker entries.

- [ ] **Step 1: Add the short-write model.** Extend `Machine` with `short_write = (name, bytes)`; in `_fwrite`, return `bytes` for the named file and mark the remaining bytes unwritten. Add the `s5` case: a save whose write is short, `exits=True`, `allow_calls=SC_EXIT`, `calls=(SC_FLUSH,)`, `terminal=("exit", 1)`. The port's side takes its declared path; the case hits `0x1B0F7` and proves the raw's exit arm, but the row reopens because `SC_FLUSH` (`0x6C4CF`) is a recorded stub. Record that trade; the census gains `config_file_save`.

- [ ] **Step 2: Run and confirm 8/8.** Run the `config_file_save` row; confirm the last block is hit and the row remains `WEAKER` (do not remove it). Re-measure `SC_KINDS`/`SC_CASES`; the mutant set must not weaken.

- [ ] **Step 3: Reclassify the weaker entries.** Add `0x51ED8` (declared byte difference) and `0x1D0BC` (the `0x1D10E`/`0x1D183` buffer-failure arms) to `WEAKER`; `0x249F0` was added in Task 5. Broaden the comment to "a poked/partial/terminal row **or a declared byte difference**". At this point the full-row figure moves 808 → 807 (806 → 805 on the baseline); the weaker-entrant count becomes 15. Re-measure the counter and the portable full figure.

- [ ] **Step 4: Run and commit.** Run `make diff-verify` and the unittest; confirm no other row moves. §P7 records the case, the block, the reopen, the classifications and the measured 825/845, 807/845, 20 open, 15 weaker. Commit (when authorized): `diff: cover the short write and count the declared differences`.

---

### Task 7: Terminal census, the divide guard, and the dead/live functions

**Files:**
- Modify: `port/src/game/flow.c` (the `0x42203`/`0x4221C` guard, near the mode-0x12 case 6)
- Modify: `port/tests/test_game.c` (the fault test)
- Modify: `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` (§P8)
- Modify: `AGENTS.md`, `docs/PROGRESS.md` only in Task 11 (the census text is drafted here)

**Interfaces:**
- Consumes: `host_cpu_fault`, `host_set_fault_hook`, the mode-0x12 case-6 test, the 20 open rows, the 8 `UNROWED`, the 15 `WEAKER` entries, `0x24B08`/`0x24B54`, the divide-site evidence.
- Produces: a complete 20-row terminal census; one guard and one test; a `fn_resolve(0x24B08) == NULL` negative test; the raw site corrections (`0x39ADF`, `0x39C6D`, `0x42203`/`0x4221C`, `0x2E910`).

- [ ] **Step 1: Write the census table.** For each of the 20 remaining open rows: address, blocker, class, why no row can exist, the record that pins it. The 15 AIL thunks, `sound_buffers_alloc` (`0x1D0BC`, blocked by `0x1C308`), `input_sampler` (four host probes), `config_file_load`/`menu_fatal_error` (both blocked by `0x6C4CF`) and `config_file_save` (reopened by the short-write case on the same `0x6C4CF`) are the set. For each of the 8 `UNROWED`: the replacement and why a row would mask it. For each of the 15 weaker entries: the one-line reason it cannot be full. Add a unittest that the census's row set equals `closed_rows`'s open set (20 after Task 6), so a new open row fails the census. This is the phase's "no unclassified residue" claim.

- [ ] **Step 2: Add the divide guard.** In the mode-0x12 case 6, before both `idiv` sites, add:
  ```c
  if (step == 0) host_cpu_fault(0x00u, 0x42203u, "divide by zero", 1);
  ```
  Use the raw's faulting instruction address for each site (`0x42203`, `0x4221C`) and the literal status `1` (the file-local `SVC_FAULT_EXIT` is not visible in `flow.c`). The two guarded `flow_*` sites (`0x4F4E8`/`0x4F5C8`) and the safe sites (`0x39ADF`, `0x39C6D`, `0x38A59`, `0x2E910`) are recorded with their proofs, not changed. For `0x2E910`, record the caller's `0x32640 = 03 00` label and add a unit test that `len = 1` and `unit >= 4`; the prose "only caller" is not enough.

- [ ] **Step 3: Add the fault test.** Seed the background record's `word[+0x2C] = 0x180`, run the case, and assert the fault hook sees `(0, 0x42203)` and the run exits with status `1`. Prove the test fails if the guard is removed (SIGFPE or no fault). Run `PR_ORACLE_REQUIRED=1 ./build/run_tests`.

- [ ] **Step 4: Record `0x24B08`/`0x24B54`.** §P8 has the disassembly, the no-reference proof, the `ret` at `0x24B07`, the `0x24B8C` table, the live `0x24B54` installation and the six captures. Add `CHECK(fn_resolve(0x24B08u) == NULL, ...)` with a seeded registry; confirm `0x24B54`'s row/registration status and that the unrowed guard test does not flag it (if it does, row it or add it to the census with evidence).

- [ ] **Step 5: Run and commit.** No counter change expected (no new spec); `port_progress` unchanged. Run the unit suite and `make verify`'s C steps. Commit (when authorized): `port: guard the one open divide and record the dead function`.

---

### Task 8: Order-preservation in the gameplay comparator

**Files:**
- Modify: `tools/gp_compare.py` (`frame_claim`, a new `order_claim`/occurrence map, the docstring)
- Modify: `tools/tests/test_gp_compare.py` (replace `test_named_gap_order_is_not_claimed`)
- Modify: `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` (§P9)

**Interfaces:**
- Consumes: the existing `frame_claim` classification (`clean`, `splice`, `transition`, `band`, `scan3`, `overlay`) and `exhibited`.
- Produces: an existential, occurrence-aware order check over `[start, first_unexplained)`: clean occurrences from an `md5 → [indices]` map; splice/transition candidates with the meaningful-contribution test; band/scan3/overlay order-neutral; full-dump fallback; the first non-alignable frame reported. No pin change; all 24 scenarios pass.

- [ ] **Step 1: Write the failing tests.** Replace the "order is not claimed" test with:
  - the existing rewind fixture `[rgb(1),rgb(2),rgb(3)]` / `[rgb(1),rgb(3),rgb(2),rgb(1)]` must fail at capture 2;
  - a held-screen repeat passes (occurrence map);
  - a mid-update splice passes (u8 585 / u9 1896 shape);
  - a degenerate splice with no later-frame-unique byte does not rescue a rewind;
  - a band/scan3 frame is order-neutral (u22 5033/5034 shape).
  Run `python3 -m unittest tools.tests.test_gp_compare`; expected FAIL for the new rules.

- [ ] **Step 2: Implement the map and the rule.** Add `md5(port[m]) → [m]`; for clean frames choose the smallest occurrence ≥ `last`; for splice/transition enumerate all model-valid pairs, filter by "some byte after the boundary differs from the **low** frame" (and before the boundary from the **high** frame), choose the smallest `hi ≥ last`; for band/scan3/overlay keep `last` unchanged; if no local candidate, enumerate the whole dump. Report `order failure` with capture/raw/kind/`last`/candidates. The predicate is the only defense against laundering a clean rewind; a literal "differs from the high frame after the boundary" is false for every valid splice and must not be implemented.

- [ ] **Step 3: Run all 24 scenarios.** Use the existing dumps (regenerate only into `/tmp`, never `data/`). Confirm 0 order failures on all 24; the measured anomalies (u8-right-training 585, u9-win 1896, u22-diablo 5034) pass as forward splices/mixed scans. If any scenario fails, stop and record the exact bytes; do not add a fitted allowance.

- [ ] **Step 4: Update the claim text.** Replace `tools/gp_compare.py`'s "the order ... not claimed" docstring with the new claim and its named limits (content order, not display order; repeating screens map to the earliest feasible occurrence; mixed allowances are neutral). No Makefile pin changes.

- [ ] **Step 5: Run and commit.** `python3 -m unittest tools.tests.test_gp_compare`; a report-only `gp_compare.py` run on the 24 scenarios prints 0 order failures. Commit (when authorized): `tools: claim gameplay frame order`.

---

### Task 9: Coverage pins, K11 identity/order, and allowance honesty

**Files:**
- Modify: `tools/gp_compare.py` (`--max-missing`, canonical coverage, the ratchet)
- Modify: `tools/k11_compare.py` (order, `--capture-sha256`/`--capture-frames`, allowance sha/stale)
- Modify: `tools/title_compare.py` (front-end/attract2 allowance sha/stale; optional `--frontend-start`/`--max-missing`)
- Modify: `Makefile` (all `GP_*_MAX_MISSING` pins, K11 identity pins, the commands; `PR_PINS_EXACT` in Task 10)
- Modify: `tools/tests/test_gp_compare.py`, `test_k11_compare.py`, `test_title_compare.py`
- Modify: `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` (§P10)

**Interfaces:**
- Consumes: Task 8's order map; `k11_compare.canonical`; the recon's **preliminary** missing-class estimates. These are not pins: the recon's idle value was re-measured at 29 and the front-end at 30, so Task 9 measures every scenario with the implemented flag and pins only the tool's own values.
- Produces: `--max-missing` on every enforced gp command with its measured pin; canonical missing-class counting (a bounded-missing claim, not full coverage); K11 order + identity (sha/frames); byte/stale checks for the named allowances. No existing pixel pin moves.

- [ ] **Step 1: Add `--max-missing`.** Compute canonical non-black classes up to `reach` (collapse byte-identical frames, exclude all-black); fail if the scenario is unpinned or the count exceeds the pin; print "improved: lower the pin" when below. Do not couple the coverage set to the order choices.

- [ ] **Step 2: Measure and pin.** After Step 1, run the flag on each of the 24 scenarios and add `GP_*_MAX_MISSING` from the tool's own values (the preliminary estimates are not authoritative); pass `--max-missing` in every enforced command (`gp-oracle`, `gp-charsel-oracle`, `gp-moves-oracle`, `gp-keys-oracle`, `gp-twop-oracle`, the `gp-modes-one` loop, `gp-u12-oracle`, `gp-win-one`, `gp-u13-oracle`, `gp-u15-oracle`, `gp-u16-oracle`, `gp-u18…u23-oracle`). No pin for `gp-u14` (absent), `gp-u17` (no frame replay) or `gp-pads` (smoke).

- [ ] **Step 3: K11 order + identity.** Add the order check to `k11_compare.compare` (the same existential rule, with 138/151 order-neutral). Add `--capture-sha256`/`--capture-frames` and the identity check; pin `walk` sha `aea00a7d…def391` / 171 frames and `menuesc` sha `aacd3242…0f161e` / 766 frames (re-measure before pinning). The pin covers `poll.log` + frame count, not every pixel (the same limit as the gp identity). Add the unit cases for wrong sha/count and unpinned-under-enforced.

- [ ] **Step 4: Allowance honesty.** Add sha256 pins and stale-name failures for `FRONTEND_ALLOWED_UNEXPLAINED` (832 `4918d7e5…`, 833 `c9436f54…`) and `K11_ALLOWED_UNEXPLAINED` (walk 138 `1ec090d0…`, 151 `e1fc034f…`); add stale-name failures for `K11_ALLOWED_MISSING`, `ATTRACT2_ALLOWED_UNEXPLAINED`, `ATTRACT2_SPLICE3_ALLOWED`, and the gp "retire the name" notes. For the two empty sets, add a synthetic test that the stale-name check fires when a name is added; an empty set alone proves nothing. A named frame that becomes explained or changes bytes fails.

- [ ] **Step 5: Optional front-end/attract extension.** If the shared helper fits, add `--frontend-start 560` and the measured `--max-missing 30` to the front-end, the measured attract pin, and the measured attract2 pin (with the splice3 allowance modelled and the beyond-reach classes excluded). If it does not fit, record the front-end's 30 classes as a named gap; do not ship a port-derived-start coverage pin without the start pin.

- [ ] **Step 6: Run and commit.** Run all 24 oracles and the K11/front-end/attract targets with the dumps retained; confirm every pin passes, `+1` fails, a stale allowance fails, and no existing pixel pin moves. Commit (when authorized): `tools: claim gameplay coverage and K11 identity`.

---

### Task 10: Gate strict preflight and unit-suite trust

**Files:**
- Modify: `Makefile` (the `GATE_STRICT`/`GATE_NAMED_GAPS`/`CAPTURE_DIRS` block, `gate-check`, `verify`, the conditional `PR_ORACLE_REQUIRED` prefixes, the `env -u` lines, the stale comments, `PR_PINS_EXACT`)
- Modify: `tools/gp_compare.py`, `tools/title_compare.py`, `tools/k11_compare.py`, `tools/attract_compare.py`, `tools/gp_keys.py` (the exact-mode failure branches)
- Modify: `port/tests/test_platform.c` (`fn_miss.txt` required; the tautology fix is `test_fight.c`)
- Modify: `port/tests/test_fight.c` (remove the tautology at 21084)
- Modify: `tools/tests/test_tick_vector.py` (the required guard)
- Modify: `tools/tests/test_gp_compare.py`, `test_title_compare.py`, `test_k11_compare.py`, `test_gp_keys.py` (exact-mode cases)
- Modify: `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` (§P11)

**Interfaces:**
- Consumes: the 35 enumerated capture paths, the 7 driver selectors, the three capture-dependent `PR_ORACLE_REQUIRED` oracles (smk, title, attract) plus the unit/tool/re-extract uses that stay, the lower-bound pin branches.
- Produces: `gate-check` with `GATE_STRICT=1` default and `GATE_STRICT=0` partial; exported `GATE_STRICT`/`PR_PINS_EXACT`; a sanitized unit run; a required `fn_miss.txt` via `PR_FN_MISS_REQUIRED`; the tautology removed; `test_tick_vector` in the tool list; the stale comments corrected. No oracle line moves.

- [ ] **Step 1: Add the preflight.** Define `CAPTURE_DIRS` as the explicit 35 paths (the 26 gp-oracle dirs + `walk`, `menuesc`, `save-reload`, `gp-pads` + `smk/twi5`, `smk/twg` + `title`, `title2`, `frontend`; do not glob `data/k11-captures/*`, which has 5 non-oracle dirs), `GATE_STRICT ?= 1`, `GATE_NAMED_GAPS ?= $(K11_CAPTURES)/gp-u14-win-live`, `PR_PINS_EXACT ?= 0` (off for the implementation; flipped after Step 5), and `gate-check`. Export both mode variables so the Python tools see them. It prints `gate: captures N/35 present, M missing (strict=…, pins_exact=…)`, lists declared gaps and undeclared absences, fails on an undeclared absence in strict mode, and fails on a declared gap that is present (stale). `verify: gate-check build`. Keep direct oracle invocation's per-target skip.

- [ ] **Step 2: Make partial mode real.** Replace the three **capture-oracle** `PR_ORACLE_REQUIRED=1` prefixes (smk, title, attract) with `$(GATE_ORACLE_ENV)` (set when strict, `env -u PR_ORACLE_REQUIRED` when partial); leave the unit/tool/re-extract uses alone; unset it on the front-end family in partial mode. A caller-exported `PR_ORACLE_REQUIRED=1` must not defeat partial mode. `test_video.c` already skips an absent Smacker capture unless `PR_ORACLE_REQUIRED` is set; no change there.

- [ ] **Step 3: Sanitize the unit run.** Prefix the verify unit line (and `make test`) with `env -u PR_ATTRACT_DUMP -u PR_TITLE_DUMP -u PR_FRONTEND_DUMP -u PR_FRONTEND_DET -u PR_K11_DUMP -u PR_GP_DUMP -u PR_RESTART`; keep `PR_ORACLE_REQUIRED=1` and `PR_GAME_DIR` explicitly set. Sanitize the restart line similarly, keeping `PR_RESTART=1` after `env`.

- [ ] **Step 4: Assertion honesty.** Make `frames/fn_miss.txt` absence fail under a new `PR_FN_MISS_REQUIRED` flag set only by `verify` (so `make test` and standalone `run_tests` keep working); `verify` runs `--check` immediately before the unit run, so the file is fresh in the gate, and `make test` does not claim it. Remove the `CHECK(1, ...)` tautology at `test_fight.c:21084`; add `tools.tests.test_tick_vector` to the tool list with `PR_GAME_DIR=$(GAME_DIR)` and make it fail (not skip) without `PRAGE.EXE` under `PR_ORACLE_REQUIRED`; correct the `frames/*.idx`, `title_screen_ref.ppm` and entry-triage comments.

- [ ] **Step 5: Exact pins.** Add the missing "improved" branches to `k11_compare` and `attract_compare` first, then add `PR_PINS_EXACT` reads to `gp_compare` (start/reach/ratchet), `title_compare` (front-end/demo/attract2), `k11_compare` (min-exh/open-end), `attract_compare` (min-port-exh) and `gp_keys` (effects). Under exact mode, an improvement fails with "re-pin"; add a unit case per branch. Run the full gate once with the explicit `PR_PINS_EXACT=1` while the default is still 0; re-pin any drift in a separate, recorded commit; only then flip the default to `$(GATE_STRICT)`.

- [ ] **Step 6: Run and commit.** `make gate-check` with the full checkout prints 34/35 + the U14 declared gap; a scratch missing dir fails strict and passes partial; a stray `PR_TITLE_DUMP` no longer replaces the suite; `PR_PINS_EXACT=1 make verify` (explicit) has no "improved:" line. Commit (when authorized): `build: make the gate check its captures and pins`.

---

### Task 11: Docs truth sweep and span accounting

**Files:**
- Modify: `README.md`, `AGENTS.md`, `docs/PROGRESS.md`, `port/RE_GUIDE.md`, `port/spec/game_flow.md`, `port/spec/audio.md`, `docs/superpowers/specs/2026-10-16-subproject-a-gate-trustworthiness-design.md`, `docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md`, `docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md`, `tools/port_classification.txt`, `port/src/game/fighter.c` (the `0x377DD` comment)
- Modify: `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` (§P12)

**Interfaces:**
- Consumes: Task 6's measured full/weaker counter; Task 5's 20-row census; Task 4/5's counter; Task 9's oracle text; Task 10's gate text; the C/E checklist in Appendix A.
- Produces: no contradiction from the checklist; the 66% title qualified as Ghidra-indexed; the second span metric; the 20/8/15 census; the corrected `0x51ED8`, `0x249F0`, `0x1D0BC`, `0x1BDF4`, `0x377DD`, AIL timer, boot-controller, Smacker and storage wording. `entry-triage` unchanged; `symbols.h` idempotent.

- [ ] **Step 1: Apply the counter/classification fixes.** Confirm the phase's `0x51ED8`/`0x249F0`/`0x1D0BC` `WEAKER` additions and update the current-state counts in README/AGENTS/PROGRESS after the measured line. Replace "8 named gaps" with "8 ported functions with no differential row (`UNROWED`); `diff-verify` prints 0 named gaps". Correct "1031 functions" to "1031 specs over 905 entries" (or the measured equivalent), and change README:31 to "Ghidra's 1203 indexed functions".

- [ ] **Step 2: Apply the stale-status fixes.** Work Appendix A's table: C1/C14 (GT2 design/plan), C2 (the `UNROWED` mislabel), C3 (audio timer), C4 (boot controllers/run clock), C5 (front-end END), C6 (Smacker audio), C7 (storage §S.10/PLAUSIBLE), C8 (sub-project A status), C9 (U0 history), C10 (K11 gaps), C11 (effect render path), C12 (`0x377DD` comment), C13 (sound-id table), C15 (`0x51ED8`), E1–E18 (functions label, enforced/skip split, `data/` wording, `.idx`/test_gfx, entry-triage comment, `title_screen_ref.ppm`, classification reasons, 53 wording, `0x1BDF4`). E19–E21 are checked and marked deferred: E19 is a clarification, E20 a plan-number citation, E21 a dated snapshot.

- [ ] **Step 3: Add the span accounting.** In README/AGENTS, add the span census: 417 slots, 392 distinct, six renderers, one `ret`, 385 writers (384 reachable + 1 dead), 227 E2-triaged and 158 untriaged (231 E2 span rows minus the four renderers = 227), `0x24B54` live, `0x24B08` dead. Keep the README title at 66% with the "of Ghidra's 1203 indexed functions" qualifier; do not change `port_progress.py`'s denominator or `symbols.h`.

- [ ] **Step 4: Append the phase paragraph.** `docs/PROGRESS.md` gets the measured differential line, the span proof, the oracle claims, the gate modes, the 20/8/15 census and the named gaps. The record §P12 has the same numbers and the task-by-task evidence.

- [ ] **Step 5: Verify docs consistency.** Grep for the old claims and confirm none remain in current-state text (dated snapshots may keep their numbers if marked). Run `python3 tools/port_progress.py` and `make entry-triage`; both unchanged. Commit (when authorized): `docs: state the proof completion results`.

---

### Task 12: Full gate, re-pin, and closure

**Files:**
- Modify: the phase's pins in `Makefile`, `tools/tests/test_diff_verify.py`, `tools/tests/test_gp_compare.py`, `tools/tests/test_k11_compare.py`, `tools/tests/test_title_compare.py`
- Modify: `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` (§P13)
- Modify: `README.md`, `AGENTS.md`, `docs/PROGRESS.md` only if the final run moves a number

**Interfaces:**
- Consumes: every task's measured values.
- Produces: the phase's authoritative counter, oracle lines and gate output; the final record; the merge-ready tree.

- [ ] **Step 1: Run the full gate.** With every capture present except the declared U14 gap:
  ```bash
  PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify
  ```
  Record every line. Expected: exit 0; `gate: captures 34/35 present, 1 missing` + the U14 gap; `make diff-verify` at the measured 1033/1033 line; every gp oracle at its old pixel pins plus the new order/coverage lines; K11 identity; `symbols.h` byte-identical; `entry-triage` unchanged.

- [ ] **Step 2: Prove the new pins fail.** For each new pin (`MAX_MISSING`, K11 identity, order rule, span coverage, counter), move it one step and confirm the failure, then restore. Record the failure text in §P13. A pin that cannot fail is removed, not kept.

- [ ] **Step 3: Correct the record and docs.** Replace every predicted counter with the measured line; correct the task arithmetic; update the README/AGENTS/PROGRESS numbers if they moved; note any pin that was re-measured.

- [ ] **Step 4: Run the regression suite.** `PR_ORACLE_REQUIRED=1 ./build/run_tests` after a `--check`, `python3 -m unittest` for the tool modules, `make entry-triage`, `python3 tools/port_progress.py`. All green; `port_progress` unchanged.

- [ ] **Step 5: Close the phase.** Mark the design "Implemented and gated", append the PROGRESS paragraph, and record the remaining named gaps (the 20 terminals, the 8 unrowed, the 15 weaker, the 158 untriaged span targets, the dead writer, U14/audio/etc. outside scope). Commit (when authorized): `docs: close the proof completion phase`. Merge/push only on the user's explicit instruction.

---

## Phase acceptance summary

- **Differential:** the measured line replaces the predicted one; 12 open rows close (1 full, 11 weak); the 20 remaining open rows are declared terminals with a per-row reason; the 8 `UNROWED` and 15 weaker entries have reasons; the span class/entry proof and the `0x42203` guard are in.
- **Oracles:** all 24 scenarios pass the order rule and their measured coverage pins; K11 has identity + order; stale/byte-mutated allowances fail; no existing pixel pin moves.
- **Gate:** strict/partial modes, exported exact pins, sanitized unit run, `PR_FN_MISS_REQUIRED` in verify, no tautology, tick vector in the gate.
- **Docs:** the Appendix A checklist resolved (E19–E21 deferred); the 66% title qualified; the span census added; `entry-triage`/`symbols.h`/`port_progress` unchanged.
- **Final:** `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify` exits 0.

---

## Appendix A: C/E docs checklist

The review's correction list, as the plan consumes it. Each row is a current
claim to verify and, if still present, correct; `E19–E21` are checked and
deferred (clarification / plan-number citation / dated snapshot).

| id | file:line | current claim | correction |
|---|---|---|---|
| C1 | `docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md:3`; `docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md:3` | U16 finisher is the R2 gap; U13 ends at 4248 | mark superseded by the endgame phase; U23/U18–U22/U13 5904 |
| C2 | `README.md:56`; `docs/PROGRESS.md:23-24` | the 8 unrowed are "named gaps" | `UNROWED`; `diff-verify` prints 0 named gaps |
| C3 | `port/spec/audio.md:252-254,300-301,313` | AIL timer `likely`/`TODO(verify)` | verified 60 Hz, 59.998 Hz; `0x66BB8` |
| C4 | `port/spec/game_flow.md:261-263`; `docs/PROGRESS.md:952` | boot controller checks are a named gap; `0x32970` host-owned | `game_boot_ports`; track S run clock |
| C5 | `port/spec/game_flow.md:1163-1170,1249-1261` | front-end limitation omits `FRONTEND_END` | add the END pin; interior still unclaimed |
| C6 | `port/spec/game_flow.md:342` | streamed Smacker audio not ported | zero-payload finding; no stream opens |
| C7 | `AGENTS.md:440-452`; `docs/PROGRESS.md:1037` | storage gap list omits §S.10/PLAUSIBLE | add the slot 13..35 and PLAUSIBLE items |
| C8 | `docs/superpowers/specs/2026-10-16-subproject-a-gate-trustworthiness-design.md:3` | Status DRAFT | implemented (2026-10-16) |
| C9 | `docs/PROGRESS.md:958` | 27/72 callbacks and 6/10 finishers unported | historical; E2 now 0/71 and 0/9 |
| C10 | `docs/PROGRESS.md:859` | K11 six named gaps (some closed) | mark host-input/track-S closures |
| C11 | `port/RE_GUIDE.md:227-229,272-275` | `0x13C70` render path unported | ported in `effects.c` |
| C12 | `port/src/game/fighter.c:14051`; `docs/PROGRESS.md:805` | "unported finisher code at `0x377DD`" | `0x37774` callback; store at `0x377DD` |
| C13 | `port/RE_GUIDE.md:276-279` | sound-id table not extracted | 244 records / ids 0x54/0x56 in the records |
| C14 | `docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md:5,20` | U13 5891/4939; U10 4113 | U13 5904/4952/10069; U10 5437 |
| C15 | `tools/diff_verify.py:407`; `tools/tests/test_diff_verify.py:32211`; `README.md:47,51-52`; `AGENTS.md:488,501,540`; `docs/PROGRESS.md:17,23` | `0x51ED8` counted full | add to `WEAKER`; re-pin the measured full line (807 after the phase) |
| E1 | `README.md:45`; `AGENTS.md:119,487`; `docs/PROGRESS.md:16` | "1031 functions" | 1031 specs over 905 entries |
| E2 | `AGENTS.md:59-61`; `README.md:179-182,193` | all oracles skip unless `PR_ORACLE_REQUIRED`; verify sets it | 3 enforced; `GATE_STRICT`; exact split |
| E3 | `README.md:19-20`; `AGENTS.md:433`; `.gitignore:16-18` | `data/` read-only | `data/game/` read-only; captures are writable outputs |
| E4 | `AGENTS.md:62-63`; `Makefile:1886-1888,1892`; `port/src/main.c:158` | tests read `frames/*.idx` | `test_fn_misslog` reads `frames/fn_miss.txt` |
| E5 | `Makefile:1865-1866` | entry-triage fails under `PR_ORACLE_REQUIRED` (verify sets it) | verify does not export it globally |
| E6 | `Makefile:1968`; `.gitignore:21` | `title_screen_ref.ppm` fixture | no producer/reader; remove or mark obsolete |
| E7 | `AGENTS.md:557` | "8 named-gap functions" | 8 `UNROWED` functions |
| E8 | `docs/PROGRESS.md:1001` | `0x1BDF4` callees host-owned | both callees ported |
| E9 | `port/spec/game_flow.md:875` | `0x2E934` named gap | ported by track S |
| E10 | `port/spec/game_flow.md:1102` | `0x32970` named gap | ported by track S |
| E11 | `port/spec/game_flow.md:507,560` | 10 game_frame cases named gaps | 52/52 dispatch; no case gap |
| E12 | `docs/PROGRESS.md:1037` | save rows run under code pokes | unpoked under track F |
| E13 | `tools/port_classification.txt:56-59` | PIT/game-port reason says caller ported | hardware read is host-owned; §H6 |
| E14 | `tools/port_classification.txt:39` | `0x1BDF4` reason | keep the callback host-owned; callees ported |
| E15 | `port/RE_GUIDE.md:128` | `0x1C500` likely getter | `game_string_get` |
| E16 | `port/RE_GUIDE.md:283-284` | capture list omits U17/U18–U23 | add them |
| E17 | `docs/PROGRESS.md:1059`; `AGENTS.md:606` | "53 host-owned" | 41 host-owned + 12 deferred |
| E18 | `AGENTS.md:385` | "Task 8 was declined" ambiguous | endgame-truth plan's Task 8 |
| E19 | `docs/superpowers/specs/2026-10-13-storage-layer-design.md:14,101,201,252` | 14 rows vs 16 functions | clarification only; deferred |
| E20 | GT2 plan/AGENTS U14 task number | Task 11 vs Task 8 | no contradiction; cite the plan; deferred |
| E21 | `docs/PROGRESS.md:1071` | dated 2026-10-22 snapshot | mark snapshot; deferred |
