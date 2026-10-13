# Track I: the master init and the inlined functions — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the port's master init run the original's twelve calls in the original's order, and turn the five functions the port inlines (`0x2F9CC`, `0x2F920`, `0x11BCC`, `0x12CD4`, `0x1D018`) into real C functions with differential rows.

**Architecture:** Each inlined block becomes one C function with a `/* 0xADDR — … */` header in the file that holds the inlined copy today, called from where the copy was. Each gets a binding, a `Spec` and mutants in the differential harness. `0x2F9CC` also gains the eight calls the port omits today, all of whose callees are already ported.

**Tech Stack:** C11 over the flat `mem[]`, Python 3 with `unicorn` 2.1.4 and `capstone` 5.0.7, the differential harness (`tools/diff_verify.py`, `port/tests/diff_runner.c`, `tools/tests/test_diff_verify.py`), Ghidra MCP read-only.

**Spec:** the design approved in chat on 2026-10-13 (bounded path; no spec file). Its content is this plan's Goal, the table in Task 2 and the Risk note in Task 2. Rulings made against it are provisional and are listed for the user at the end.

## Global Constraints

- Ghidra address == linear address. The fixed-up image is `/tmp/pr_i_img.bin` (`./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_i_img.bin`); image offset = address − `0x10000`. Never read data addresses from a raw-file disassembly.
- Every value and branch sense comes from an instruction you can cite. No fitted constant. Where this plan and the bytes disagree, the bytes win: record the correction with its address in the record and the report.
- One C function per original function, header comment `/* 0xADDR — record 2026-10-13-reverse-i §I.n */`. Comment styles in `port/src`: `/* 0xADDR — … */`, `/* PORT: … */`, `/* TODO(verify): … */` only.
- All original state lives in `mem[]` at its original address (`DSB/DSW/DSD`). No long-lived C global shadows it.
- A ported function that another row stubs or records opens with `PR_SEAM`/`PR_SEAM_RET`/`PR_SEAM0`/`PR_SEAM_RET0` (`mem.h`).
- Never hand-edit `port/src/symbols.h`. Never write under `data/`. Never write `mem[0xA0000]`.
- Every binding and mutant stays inside the 64 MB `mem[]`.
- Tests use only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`; a new test is registered once in `port/tests/test.h`; every new assertion is proved to fail under a mutation of the code it tests (name the mutation in the record); no assertion on an unseeded zero.
- Harness files: insertions in one contiguous block per file, prefix `i_` (bindings) and `I_` (specs, pins), immediately above `static const binding_t k_bindings[] = {` / above the table's closing `};` / after the last `D1_SPECS += …` line / after the last `D1_CASES.extend(…)` line. A change to an existing row is made in place and listed in the report.
- Method for a row: `.superpowers/sdd/2026-10-11-reverse-d1/brief-wave5.md` and `docs/superpowers/plans/2026-10-01-reverse-e3-derivations.md` §E3.10. The closest worked examples: the `SI_SPECS` block (`config_run_clock`, `config_isr_tick`) and the `SA_SPECS` block in `tools/diff_verify.py`, their bindings in `port/tests/diff_runner.c`, their pins in `tools/tests/test_diff_verify.py`.
- Yours to run: `cmake --build build`, `PR_ORACLE_REQUIRED=1 ./build/run_tests`, `python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE --image /tmp/pr_i_img.bin --function <name> --self-check`, one full `--self-check` (about half a minute), and `python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify` (about two minutes). Not yours: `make verify`, any oracle target, any capture target, DOSBox-X.
- Ghidra MCP is read-only for agents: no rename, retype, comment or save.
- Commit on the task's branch as you go: `<area>: <what changed>`, stage named files, never `git add -A` (the `data` and `.superpowers` symlinks show as untracked: never add them), end each message with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Never push; never touch `main`.
- Record: `docs/superpowers/plans/2026-10-13-reverse-i-derivations.md`, one section per function (§I.1 `0x2F920`, §I.2 `0x11BCC`, §I.3 `0x12CD4`, §I.4 `0x2F9CC`, §I.5 `0x1D018`): the blocks with addresses, the row table, the mutants, the corrections, the mutation proofs.

## Review Focus

Conditions the design implies that no task's tests would otherwise exercise, most likely first. Each has its test in the task named.

1. **Boot state after the eight added calls.** A driver that boots the port must end in the same state the oracles pin. Expected: the gate's oracle lines do not move, or move towards the capture. Test: Task 2 step 5 (the `PR_RESTART` driver and `make check frames=60` byte-compared before and after) and the controller's gate.
2. **A soft restart must not run the master init again.** `0x2F9CC` is called before the `setjmp` at `0x20C1F`; a `longjmp` re-runs only the tail. Expected: `game_master_init` runs once per process. Test: Task 2, the `PR_RESTART` driver assertion.
3. **The camera stepper at the extremes.** `0x12CD4` negates a signed difference (`neg ecx`, `0x12CE7`): the difference `0x80000000` stays negative. Expected: the port follows the raw there with no C undefined behaviour. Test: Task 1, the row's case and the unit case `intmin`.
4. **The timer exit's tail call.** `0x11BCC` ends in `jmp 0x2C3FC` with `EAX = 0x100`, so its caller's next instruction runs after the voice call returns. Expected: the caller's order is unchanged. Test: Task 1, the `game_state_step_high` row stays VERIFIED with its pins unchanged.
5. **The sound shutdown with nothing open.** `0x1D018` returns at once when the byte `0xA2CB1` is 0. Expected: a second shutdown is a no-op. Test: Task 3, the unit case `twice`.

---

### Task 1: `0x2F920`, `0x11BCC` and `0x12CD4` become functions

**Files:**
- Modify: `port/src/game/actors.c` and `actors.h` (`0x2F920`), `port/src/game/flow.c` and `flow.h` (`0x11BCC`), `port/src/game/camera.c` and `camera.h` (`0x12CD4`)
- Test: `port/tests/test_game.c`, `port/tests/test.h`; the three harness files
- Create: `docs/superpowers/plans/2026-10-13-reverse-i-derivations.md` (§I.1-§I.3)

**Interfaces:**
- Produces: `void actors_table_clear(void)` (`0x2F920`), `void game_state_timer_exit(void)` (`0x11BCC`), `void camera_y_step(s32 target)` (`0x12CD4`). If the bytes show another signature, the bytes win and the report says so.

The derivations to confirm from the image (integrator's reading):

- **`0x2F920`** (`0x2F920..0x2F935`): `push ebx; push edx; mov ebx,0x14d4; mov eax,0x105f38; xor edx,edx; call 0x61a70; pop edx; pop ebx; ret`. It is `memset(0x105F38, 0, 0x14D4)`. Callers: `0x2BBDF` (the port's inlined copy is `mem_fill(DS_00105F38, 0, 0x14D4u)` in `actors.c`, near the comment `/* 0x2F920 */`) and `0x20C42` (in `0x20C10`'s tail: find what the port does there and report it; wire the call if the port omits it and say so).
- **`0x11BCC`** (`0x11BCC..0x11BF4`): `xor ah,ah; mov [0x104b1b],ah; mov [0x104b15],ah; mov ax,[0xf0a6c]; mov [0xf0a64],ax; call 0x29d60; mov eax,0x100; jmp 0x2c3fc`. One caller, `0x11E77`. The port's inlined copy is in `game_state_step`, `case 7`, under `/* 0x11BCC the timer exit. */`. `0x29D60` is a `ret`-only function the port does not call (a named non-row): keep that as it is, under its `PORT:` note.
- **`0x12CD4`** (`0x12CD4..0x12D30`), `EAX = target`: `edx = [0xF0AEC]`; `diff = target - edx`; `mag = diff < 0 ? -diff : diff` (`neg ecx`); `mag <= 0x100` (`jg` not taken) -> `edx = target`; else `diff > 0` (`jle` not taken) -> `edx += 0x100`, else `edx -= 0x100`; then `edx += (s32)[0xF0AF2] >> 16` (`sar eax,0x10`); `[0xF0AEC] = edx`. One caller, `0x131CB`. The port's inlined copy is in `camera.c` under `/* 0x12CD4: step or snap DS_000F0AEC … */`.

- [ ] **Step 1: Set up.** `./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_i_img.bin`. Read the three functions' bytes with capstone and confirm or correct the readings above. Read the three inlined copies and their callers in the port.
- [ ] **Step 2: Unit test first.** Add `int test_inlined_functions(void)` to `port/tests/test_game.c` and register `X(test_inlined_functions)`. Cases, each from seeded sentinels that differ from the post-condition:
  - `table`: `0x105F37` and `0x10740C` (the bytes on either side of the cleared range) keep their seeds, every byte of `0x105F38..0x10740B` seeded `0xA5` becomes 0;
  - `timer`: with `0x104B1B = 0x5A`, `0x104B15 = 0x5B`, the word `0xF0A6C = 0x1234`, the word `0xF0A64 = 0x4321` and their neighbours seeded: the two bytes are 0, `0xF0A64 == 0x1234`, the neighbours keep their seeds;
  - `step`: from `0xF0AEC = 0x1000` with the dword `0xF0AF2 = 0x00030000`: a target `0x1100` snaps (`0x1100 + 3`), `0x1101` steps (`0x1100 + 3`), `0xF00` snaps (`0xF00 + 3`), `0xEFF` steps (`0xF00 + 3`); with the dword `0xF0AF2 = 0xFFFE0000` the offset is `-2`;
  - `intmin` (Review Focus 3): `0xF0AEC = 0` and the target `0x80000000`: the result the raw gives (derive it from `neg ecx; cmp ecx,0x100; jg` and `test eax,eax; jle`, and cite the instructions).
  Run it and see it fail to link.
- [ ] **Step 3: The three functions.** Write each with its header, replace the inlined copy with a call, and keep the surrounding order. Build; the unit test passes.
- [ ] **Step 4: Rows.** A `Spec` and mutants for each (`actors_table_clear`, `game_state_timer_exit` with `0x29D60` in `allow_calls` and `0x2C3FC` a recorded stub, `camera_y_step`), covering every block. Mutants are the plausible wrong ports: the length or the base off by one; a byte not cleared, the word copied the wrong way, the voice id; `>=` for `>` at `0x100`, the step sign, the offset taken from the low word, an arithmetic shift made logical. Per-function `--self-check` passes.
- [ ] **Step 5: The rows of the callers.** The rows of the functions that held the copies (`game_state_step_high` for `0x11BCC`; the camera row that covers `0x131CB`; the `actors_reset` row) must stay VERIFIED with their pins unchanged: the new functions have no seam unless a row needs to stub them. Run those rows' `--self-check`, then the harness suite.
- [ ] **Step 6: Mutation-prove** each assertion of `test_inlined_functions` (list the mutation per case in the record).
- [ ] **Step 7: Record and commit** (one commit per function, then the record): `port: <name> (0xADDR) is a function, with its row`.

### Task 2: `0x2F9CC` becomes `game_master_init`, with the calls the port omits

**Files:**
- Modify: `port/src/game/flow.c` and `flow.h`
- Test: `port/tests/test_game.c`; the three harness files; the record §I.4

**Interfaces:**
- Consumes: `config_file_load` (`0x1AFE8`), `effects_init` (`0x13ADC`), `frontend_input_reset` (`0x4F1E4`), `actors_reset` (`0x2BAF4`), the port's function for `0x4F1D0` (`flow.c`, near the comment `/* 0x4F1D0. …`), `frontend_spawn_row` (`0x38B18`), `config_validate` (`0x2D6F8`), `config_field_get` (`0x2D974`), `config_voice_gate` (`0x2C9B8`), `attract_config_volumes_unscaled` (`0x2C8F0` with -1), `attract_config_volumes` (`0x2C8F0` with -2).
- Produces: `void game_master_init(void)` (`0x2F9CC`).

The original (`0x2F9CC..0x2FA3F`), in order, to confirm from the image:

| # | Address | Call or store | In the port's `game_init` today |
|---|---|---|---|
| 1 | `0x2F9CF` | `0x1AFE8()` | yes |
| 2 | `0x2F9D4` | `0x13ADC()` | no (it runs only earlier, inside `actors_init`) |
| 3 | `0x2F9D9..0x2F9DB` | `0x4F1E4(EAX = 0)` | no |
| 4 | `0x2F9E0..0x2F9E9` | `0x2BAF4(EAX = 1)` with `ECX = 0`, `EBX = 0` | no |
| 5 | `0x2F9EE..0x2F9F2` | `0x4F1D0(EAX = 0, EDX = 0)` | no |
| 6 | `0x2F9F7..0x2F9FC` | `0x38B18(EAX = 0x9AD84)` | no |
| 7 | `0x2FA01` | `[0x10740C] = 0x1D2D0` | yes |
| 8 | `0x2FA0B` | `0x2D6F8()` | yes |
| 9 | `0x2FA10..0x2FA1C` | `[0x107410] = 0x2D974(0x2A) & ~3` (`and al,0xfc`) | yes |
| 10 | `0x2FA21..0x2FA23` | `0x2C9B8(EAX = 0)` | no |
| 11 | `0x2FA28..0x2FA2D` | `0x2C8F0(EAX = -1)` | no |
| 12 | `0x2FA32..0x2FA37` | `0x2C8F0(EAX = -2)` | no |

The port's block is in `game_init` under the comment `/* 0x20C15 0x2F9CC, before the setjmp (0x20C1F) …`. `svcmenu_register()` follows it there as a `PORT:` line: it stays in `game_init`, after the call of `game_master_init`.

**Risk, binding.** The eight added calls change what runs at boot (an actor spawn, an actors reset, three sound calls). If a driver or `--check` output moves, do not adjust anything to hide it: stop, report the call, the address and what moved. The controller decides with the gate.

- [ ] **Step 1: Derive.** Confirm the table from the image: each argument register at each call, and what the callees do with registers `0x2F9CC` leaves set (`ECX = 0`, `EBX = 0` at call 4; `EDX = 0` at call 5). Read each callee's port function and check its signature takes what the raw passes. Record §I.4.
- [ ] **Step 2: Baseline.** Before any change: `make check frames=60` and keep a copy of `frames/` (`/tmp/pr_i_frames_before`); run `PR_RESTART=1 PR_GAME_DIR=data/game/C ./build/run_tests` and keep its output.
- [ ] **Step 3: Unit test first.** In `test_inlined_functions`, a case `master` that runs under the harness's call seam the way `test_call_seam` does (read it in `port/tests/test_platform.c`): record the ordered seam calls of `game_master_init()` and check the twelve steps of the table in order with their arguments, and the two stores (`0x10740C == 0x1D2D0`; `0x107410 == field 0x2A & ~3`, with the field seeded so that its low two bits are set). If a callee has no seam, add `PR_SEAM`/`PR_SEAM0` to it (allowed: it is inert outside the harness) and say so in the report. See it fail.
- [ ] **Step 4: The function.** `game_master_init()` in `flow.c` with the twelve steps in the raw's order; `game_init` calls it where the block was. Build; the unit case passes.
- [ ] **Step 5: Boot comparison (Review Focus 1 and 2).** `make check frames=60` again and compare `frames/` byte for byte with the copy (`diff -rq`); run the `PR_RESTART` driver again and compare. Report exactly what differs, or that nothing does. Add an assertion to the `PR_RESTART` driver that the master init ran once (count its seam, or a byte it alone writes) across the restart.
- [ ] **Step 6: Row.** `Spec("game_master_init", 0x2F9CC, …)` with every callee a recorded stub (`E.Call` with the argument registers of the table and the clobbers `E.callee_clobbers` derives), cases that seed field `0x2A` with its low bits set and clear, and stub EAX values that show which results the raw reads (the `0x2D974` result) and which it does not. Mutants: each call dropped, two adjacent calls swapped, each argument wrong, the `& ~3` dropped, the store to `0x10740C` dropped or with another value. `--self-check` passes; the harness suite passes.
- [ ] **Step 7: Mutation-prove** the unit case (drop each of the eight added calls in turn: the case fails each time; list them).
- [ ] **Step 8: Record and commit**: `port: game_master_init (0x2F9CC) is a function and makes the original's twelve calls, with its row`.

### Task 3: `0x1D018`, the sound shutdown

**Files:**
- Modify: `port/src/game/flow.c` and `flow.h` (the inlined copy is in `game_shutdown`), possibly `port/src/platform/audio/ail.c`/`ail.h` for seams
- Test: `port/tests/test_game.c` or `test_audio.c`; the three harness files; the record §I.5

**Interfaces:**
- Produces: `void sound_shutdown(void)` (`0x1D018`), or a named gap with its evidence.

The original (`0x1D018..0x1D0B8`), to confirm: returns at once when the byte `0xA2CB1` is 0; else clears it; when the dword `0x1028C4` is not 0: `0x5DEED([0x1028C0])` (the sequence's status), and when that is 4, `0x5DEAF([0x1028C0])` (stop); then `0x1028C4 = 0` and `0x1028C0 = 0`; when the dword `0x1028C8` is not 0: for the four sample slots at `0x102860 + 0x18 * i`, `0x5DD03(handle)` and, when that is not 2, `0x5DC8B(handle)`, then the handle dword is zeroed (`mov [esi+0x102848],ebx` after `esi += 0x18`); then `0x1028C8 = 0`; then `0x5D86A()` (the library's shutdown). The arguments of the five library calls are pushed and popped by the caller (`add esp,4`). One caller, `0x1BE33`.

The port's copy in `game_shutdown` is shorter: it clears the two flags, calls `AIL_stop_sequence(s_sequence)` and `AIL_shutdown()`, with no status test and no sample loop, and the sequence handle is a C static. The audio layer keeps host objects, so the port's handles may not be the dwords the raw reads.

- [ ] **Step 1: Derive** the function from the image and read the port's audio layer: where the sequence and sample handles live (`mem[]` dwords or C statics), which of `0x5DEED`, `0x5DEAF`, `0x5DD03`, `0x5DC8B`, `0x5D86A` have a port function and a seam, and how the existing rows of the sound functions (`sound_sample_start`, the `0x1D0BC` family) stub them.
- [ ] **Step 2: Decide, from what step 1 shows, the first of these that is honest:**
  1. *A faithful function and a full row*: the port's handles are the raw's dwords, or a read of them gives the same tests. `sound_shutdown()` follows the raw block by block (the status tests, the four-slot loop, the stores in the raw's order), `game_shutdown` calls it at `0x1BE33`'s place, the library calls are recorded stubs in the row.
  2. *A function with named `PORT:` deviations and a partial row*: the function exists and follows the raw where it can; each place it cannot (a host handle) is one `PORT:` note with the address, and the row names every block it does not reach.
  3. *Stays inlined, a named gap*: say exactly which instruction makes a faithful function impossible.
  Do not change what the audio layer does to make a row pass.
- [ ] **Step 3: Unit test first** for whichever form step 2 chose: `twice` (Review Focus 5: with `0xA2CB1 = 0` nothing is touched and no library call is made; a second call after a shutdown is that case), and, for forms 1 and 2, `open` (the flags and handles seeded: the stores of the raw happen and the calls happen in the raw's order). See it fail, implement, see it pass, mutation-prove it.
- [ ] **Step 4: Row** for forms 1 and 2, with mutants (the status tests inverted, the loop bound, the handle not zeroed, the shutdown call dropped, the early return dropped).
- [ ] **Step 5: Record §I.5 and commit**: `port: sound_shutdown (0x1D018) …` or `docs: 0x1D018 stays inlined: …`.

---

## After the tasks (controller)

- The final whole-branch review is a read-only audit: re-derive the five functions and every changed call site from the bytes, independently of the record, and report every instruction the port does not reproduce, every `PORT:` note that hides a difference, and every row that passes only through a stub or a pin.
- The gate: `make verify`, the oracle sections compared with the last gate's. The pins and the counter are re-measured (`unrowed` goes from 23 towards 18).
- Docs: `AGENTS.md` (the counter, the unrowed list), `README.md`, `docs/PROGRESS.md`.
- Merge to `main` and push only when the user asks.
