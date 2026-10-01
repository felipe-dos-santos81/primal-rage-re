# E3: call stubs for the differential harness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the differential harness verify a function whose callees are stubbed or run identically on both sides, comparing the ordered list of calls with their arguments beside EAX and the changed bytes (spec §5.1(3), §5.2), and prove it on a worked batch of four E2 `stubs` rows and three of their callees, all VERIFIED with every block hit, plus the first named-gap row; this is the gate track P needs before it ports the 98 unported targets and verifies the six span dispatchers.

**Architecture:** The original side (`tools/diff_emu.py`) intercepts each arrival at a call-set address by a `call` or `jmp` in unicorn's code hook, records the declared registers and stack slots, and for a stub applies the declared writes, sets EAX and returns popping the callee's `ret N`; indirect call targets are resolved at run time. The port side is a one-line `PR_SEAM`/`PR_SEAM_RET` at the top of each seamed C callee (`mem.h`), inert unless `build/diffrun` installs its hook, which prints `c <addr> <args...>` lines and stubs or runs the body per the case's `stub` lines; `fn_resolve` reports an unregistered target to the same hook. `tools/diff_verify.py` declares the call set per spec (`E.Call`), compares the two call lists, reports each callee's own verdict, and accepts `NAMED_GAP` rows.

**Tech Stack:** Python 3.12 (`unittest`), `unicorn` 2.1.4 and `capstone` 5.0.7 (`tools/requirements-diff.txt`, already installed by E1), C11 (`port/src`, `port/tests/diff_runner.c`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §5.1 (compared: "(3) the ordered list of outgoing calls with arguments"), §5.2 ("A callee is stubbed identically on both sides and its call recorded: ported functions, unported functions, the DOS4GW runtime, host I/O. Each function is verified in isolation; each callee is proven by its own check. The port side uses the existing `fn_resolve` miss log for the call record. A function whose behaviour depends on port I/O, interrupts, or an instruction the stubs cannot model faithfully is **not exercisable**: it becomes a named gap naming the blocking instruction."), §5.3 (block coverage; unhit blocks named), §5.5 (the table and `make diff-verify`), §6 (P: "each ported function has a verification row; unhit blocks and non-emulable instructions named").

**Derivation record:** `docs/superpowers/plans/2026-10-01-reverse-e3-derivations.md` (§E3.1 what is open, §E3.2 the original side, §E3.3 the port seam and the rejected alternatives, §E3.4 the call set and its soundness, §E3.5 stack arguments and the `ret N` table, §E3.6 the worked batch and its results, §E3.7 indirect calls, jump tables and E2's blockers, §E3.8 named gaps and limits, §E3.9 cost, §E3.10 what P does next, §E3.11 what was run). Inputs: E1's record (§E.3-§E.7) and E2's (§E2.6, §E2.10), the committed `2026-10-01-reverse-e2-triage.md`.

**What the planner ran (scratch, 2026-10-01, `main` at `a935e91`, image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; scratch paths in place of `/tmp`):** a prototype with every change of this plan, on which `make verify` exited 0 (743 s) with the 45 oracle lines equal to the base, the `make audio-render` WAV identical, `entry-triage` unchanged and `771 1203 64` / `731 731 100`; then a replay: a fresh copy of `a935e91`, Tasks 2-8 applied in order exactly as written here, every red and green output quoted below taken from it, every mutation of `/tmp/e3_mutate.py` run on each task's committed state (the quoted results), the final self-check, and `make verify` plus `make audio-render` on the replay's final state (Task 8, Step 4); and a second replay from a fresh copy running this plan's own check commands step by step (everything but the three `make verify` gates), whose output equals every expected block below. The code blocks below are the files both replays applied, byte for byte.

## Decisions needed from the user

**Decided by the user on 2026-10-01:** all three recommendations accepted — D1 the NULL-by-default hook in `prage_core`; D2 the worked batch uses already-ported rows only; D3 indirect calls to unregistered targets are compared by address only (fails closed).

1. **D1: where the port-side seam lives.** **Recommendation: a NULL-by-default hook in `prage_core`** (`pr_seam` in `mem.c`, the `PR_SEAM`/`PR_SEAM_RET` macros in `mem.h`, one line at the top of each seamed callee), set only by `build/diffrun` and inside `test_call_seam`. It verifies the very object code the game ships, `run_tests` can `CHECK` it, and it follows the repo's inert test seams (`sound_voice`'s voice log in `flow.c`, `res_set_screen_hook`, the `fn_resolve` miss log). With the hook NULL a seam is one untaken branch; the prototype's `make verify` shows the oracle lines and the WAV unchanged. Alternative: compile the core a second time with `-DPR_DIFF_SEAMS` into a `prage_core_diff` library for `diffrun` only, the macros expanding to nothing in `prage_core` (a clean build costs 2.31 s more at `-j8`; but `diffrun` would then verify object code the game does not ship, and the seam test would move out of `run_tests`). Link-time wrapping is not available on this host (record §E3.3). Cost if wrong: switching later is one small task (an `#ifdef` around the macros, a second CMake library, `diffrun` relinked, the seam test moved); the seam lines in `port/src` are the same either way.
2. **D2: the worked batch is already-ported rows only.** **Recommendation: yes** — `0x23130`, `0x45878`, `0x10FA8`, `0x3E4E4` (E2 `stubs` rows) and the callees `0x33950`, `0x339AC`, `0x3C4CC`, with `0x1B890` as the named-gap row. Porting an unported row here would register a code pointer the port skips today (a move callback or an animation target), which changes what the game does when that move runs and can move the gameplay ratchets and the pinned `fn_resolve` miss sets: that belongs to P, under P's gates. Cost if wrong: if the user wants E3 to port one too, add a task after Task 7 that ports `0x3F0A8` (72 bytes; callees `0x2C3FC`, `0x3C4CC`, both seamed by this plan) with its spec, regenerates the E2 table (decision D3 of E2) and re-runs the gameplay oracles.
3. **D3: indirect calls to unregistered targets are compared by address only.** **Recommendation: yes.** The port skips such a call (`fn_resolve` misses) and cannot know its arguments, so `fn_resolve` reports the miss to the hook with no arguments and the spec declares `args=()` for that target. The alternative, refusing any indirect target that is not a registered, seamed C function, would make verifying `0x2B2A0` or `0x2AE14` wait until every code pointer their cases reach is ported. The recommended rule fails closed when a target is ported later: its seam then records arguments, the spec's `args=()` no longer matches, and the row turns `MISMATCH` until the spec names them. Cost if wrong: one `ValueError` in `Spec` refusing `args=()` for an address with a registered C function (a small change; no test of this plan depends on the looser rule).

## Global Constraints

Copied verbatim from their sources:

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Spec §8: "`make verify` is the gate after every task." Common brief: the 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the `make audio-render` WAV must be byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav`; `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100`. **E3 ports no function** (no new `/* 0xADDR` header, no new `fn_register`), so the counters and the E2 table stay as they are; `make entry-triage` must stay green.
- AGENTS.md: "Ghidra address == linear address" / "those bytes are pre-fixup ... **use Ghidra (fixups applied) for any data address**, or replicate `mem_load_le` + `mem_load_le_fixups`". Both sides read only the image `build/diffrun --image-out` writes.
- AGENTS.md: "SDL and file/asset I/O live **only** in `port/src/host.c` and `main.c`." The seam adds no I/O to `port/src`; only `port/tests/diff_runner.c` prints.
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`." / "Mark deliberate deviations `/* PORT: ... */`" / "**`port/src/symbols.h` is generated** ... Never hand-edit it". The seam's declaration carries a `PORT:` comment; each seam line is a macro call; `symbols.h` is not touched.
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Register a test once** — add `X(test_foo)` to `TEST_CASES` in `port/tests/test.h`" / "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero". Python tests are `unittest` in `tools/tests/`, imported with `sys.path.insert(0, ROOT/tools)`.
- AGENTS.md: "Consolidating must not change an assertion." E3 consolidates nothing; it **extends** three exact-set assertions of `RealFunctionTests` (the spec names, the mutant names, the masks) with the E3 entries, each still exact, and changes no other existing assertion.
- E1 Global Constraints: the harness "skips like the other oracles when `unicorn` or `PRAGE.EXE` is absent (spec §5.5)". Unchanged.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." Nothing here writes outside `/tmp`, `tempfile` paths and the repo's own files.
- Common brief: "record Ghidra evidence as data"; none is used: every fact is from the bytes (record header).
- AGENTS.md: "Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files." Trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Spec §5.3 / E1 §E.7: "VERIFIED" is equivalence on the exercised blocks and inputs only, now also "each function with its callees stubbed or run as stated"; a stubbed callee is an assumption its own row discharges (record §E3.4).

## Review Focus

1. **A seam that changes what the game does.** The hook must be NULL in every binary but `diffrun`, and a seam must stub only when asked. Pinned by `test_call_seam` (mutations C1-C4), the Task 2 and Task 8 `make verify` runs (45 oracle lines, WAV), and Task 8 Step 3's grep: only `port/tests/diff_runner.c` and `port/tests/test_platform.c` assign `pr_seam`.
2. **A call list compared on one side only, or a mutant counted without a real difference.** Pinned by `test_a_missing_call_is_a_mismatch_and_counts_as_a_detection`, `test_an_extra_call_is_a_mismatch` (V1, V2), `test_a_call_set_callee_without_a_port_seam_is_a_mismatch` (a callee with no seam fails closed) and `test_each_e3_mutant_is_caught_by_what_it_breaks` (five mutants only the call list catches).
3. **Wrong stub mechanics on the original side** (the return, the popped stack arguments, a fall-through taken for a call, an indirect target not resolved). Pinned by `test_a_stub_pops_its_stack_arguments_and_records_them` (P2), `test_falling_into_a_call_set_address_is_not_a_call` (P3), the two run-time indirect tests (P4) and `test_a_stubbed_call_is_recorded_and_its_bytes_do_not_run` (P5).
4. **Coverage over-claimed.** A switch bound too loose, an indirect jump forgiven, a tail call scanned into the callee. Pinned by the four `SwitchScanTests` (P8-P10), `test_an_indirect_call_that_resolved_does_not_keep_the_function_partial` (V4), `test_a_bounded_switch_counts_its_cases_as_blocks` (V8) and `test_a_jump_to_a_call_set_address_is_a_tail_call_not_scanned` (P6).
5. **E1 or E2 drifting.** E1's six rows and seven mutants must read as before (only the new last column), E2's committed table must stay equal to a fresh run (`static_scan`'s new keywords default to E1's behaviour). Pinned by E1's 71 tests (unchanged except the three extended registry assertions), `test_the_self_check_counts_functions_mutants_gaps_and_closed_rows` (the counter line), and `make entry-triage` after Tasks 3 and 4.

## Where to run

A worktree off `main`, branch `reverse-e3`:

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse
git worktree add .worktrees/reverse-e3 -b reverse-e3 main
cd .worktrees/reverse-e3
ln -s ../../data data && ln -s ../../.superpowers .superpowers
cp ../../port/tests/ghidra_data.bin ../../port/tests/title_screen_ref.ppm port/tests/
ls -d data .superpowers port/tests/ghidra_data.bin port/tests/title_screen_ref.ppm   # all four must exist
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1                           # [100%] Built target run_tests
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_e3_img.bin && shasum /tmp/pr_e3_img.bin
```

The last line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the image differs from the one the record measured: stop. If this plan and its record are not on `main` yet, bring them from the planning branch first: `git checkout reverse-e3-plan -- docs/superpowers/plans/2026-10-01-reverse-e3-call-stubs.md docs/superpowers/plans/2026-10-01-reverse-e3-derivations.md`. Every command runs from the worktree root. Ledger: `.superpowers/sdd/2026-10-01-reverse-e3-call-stubs/progress.md`. `make verify` always runs with the parallel-safe overrides written out in the gate steps (another worktree may run at the same time; E1 §E.6.9).

**How the code steps are written.** Each change is a `python3 - <<'PY'` script that replaces exact text and asserts the text occurs exactly once before it does (`sub`), so a script either applies cleanly or stops with the name of the file and the text it could not find. Run each from the worktree root, once.

## File Structure

| File | Responsibility |
|---|---|
| `docs/superpowers/plans/2026-10-01-reverse-e3-derivations.md`, `...-reverse-e3-call-stubs.md` | the record and this plan (Task 1) |
| `port/src/mem.h`, `port/src/mem.c` | `pr_seam_fn`, `pr_seam`, `PR_SEAM`, `PR_SEAM_RET`; `fn_resolve_from` reports a miss to the hook (Task 2) |
| `port/src/game/flow.c`, `port/src/game/actors.c` | seams on `sound_voice` (`0x2C3FC`), `actors_anim_begin` (`0x2BC30`) (Task 2) and `actor_spawn` (`0x2AE14`) (Task 6) |
| `port/src/game/fighter.c`, `port/src/game/fighter.h` | seams on `hit_anim_start_a` (`0x3C480`), `hit_anim_start_b` (`0x3C4CC`, made non-static and declared) (Task 6) |
| `port/tests/test.h`, `port/tests/test_platform.c` | `test_call_seam` (Task 2) |
| `tools/diff_emu.py` | `STACK_ARGS`, `Call`, `calls=` in `run_original`, run-time indirect calls, `static_scan(stop=, switches=)`, `switch_cases` (Tasks 3-4) |
| `tools/diff_verify.py` | `Spec.calls`/`gap`, the call list in `cases_text`/`parse_port_output`/`compare`, `verify_gap`, the callee column and counter line (Task 5); `E3_SPECS` (Task 7) |
| `port/tests/diff_runner.c` | `s0`-`s3`, `stub`/`swrite` lines, the seam hook, `register_code`, the seven bindings and eight mutants (Task 6) |
| `tools/tests/test_diff_emu.py`, `tools/tests/test_diff_verify.py` | 18 and 27 new tests; three registry assertions extended (Tasks 3-7) |
| `Makefile`, `AGENTS.md`, `docs/PROGRESS.md` | the `diff-verify` comment and banner; a porting rule and the counter line; one paragraph (Task 8) |

## Shared-file touch points

E3 may run beside track G (U5-U8, U11) and before P; every edit is additive or backward compatible:

| file | region | what changes |
|---|---|---|
| `tools/diff_emu.py` | `REGS`, `OrigResult`, `run_original`, `static_scan` | new names and keyword arguments with E1's defaults (`calls=()`, `stop=()`, `switches=False`); `entry_triage.py` calls `static_scan(img, e)` and sees no change |
| `tools/diff_verify.py` | `Spec`, `PortResult`, `cases_text`, `parse_port_output`, `compare`, `SpecResult`, `verify_spec`, `verify_all`, `mutant_detection`, `table_row`, `TABLE_HEAD`, `main`; `E3_SPECS` before `SPECS`, `SPECS` gains `+ E3_SPECS` | the table gains a last column; the counter line gains `; N named gaps; M/N with every callee VERIFIED` after the mutants count (E1's `"1/1 functions VERIFIED; 1/1 mutants detected"` substring is kept) |
| `port/tests/diff_runner.c` | includes, the register enum, bindings, mutants, the table, the case struct, `run_case`, `run_cases`, `main` | the case-file grammar gains `reg s0..s3`, `stub`, `swrite`; output gains `c` lines before `ret` |
| `port/src/mem.h`, `port/src/mem.c` | end of `mem.h`; before `fn_resolve_from` and its first line | new declarations; `fn_resolve_from` calls the hook only when it is set |
| `port/src/game/flow.c`, `actors.c`, `fighter.c`, `fighter.h` | the first line of five functions; `hit_anim_start_b` loses `static`; one prototype | one `PR_SEAM` line each |
| `port/tests/test.h`, `port/tests/test_platform.c` | `TEST_CASES` after `X(test_fn_misslog)`; a block before `/* ---- test_le.c ---- */` | one registry line; one test |
| `tools/tests/test_diff_emu.py`, `tools/tests/test_diff_verify.py` | before `if __name__ == "__main__":`; three assertions in `RealFunctionTests`; the end of `RealFunctionTests` | new classes and tests |
| `Makefile` | the comment above `diff-verify:` and its `@echo` line | text only; the recipe is unchanged |
| `AGENTS.md` | Porting rules, after the `fn_origin()` / `fn_resolve()` line; the `make diff-verify` tooling line | one bullet; one sentence |
| `docs/PROGRESS.md` | end | one paragraph |

---

### Task 1: The record and the plan

**Files:**
- Add (from the planning branch if needed): `docs/superpowers/plans/2026-10-01-reverse-e3-derivations.md`, `docs/superpowers/plans/2026-10-01-reverse-e3-call-stubs.md`

**Interfaces:**
- Produces: the record every later task cites (§E3.2-§E3.8).

- [ ] **Step 1: Check the record holds the sections the tasks cite**

```bash
grep -c '^## §E3\.' docs/superpowers/plans/2026-10-01-reverse-e3-derivations.md
grep -o 'diff-verify: 13/13 functions VERIFIED; 15/15 mutants detected; 1 named gaps; 8/13 with every callee VERIFIED' docs/superpowers/plans/2026-10-01-reverse-e3-derivations.md | head -1
```

Expected: `11`, then the counter line once (record §E3.6).

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/plans/2026-10-01-reverse-e3-derivations.md docs/superpowers/plans/2026-10-01-reverse-e3-call-stubs.md
git commit -m "docs: E3 record and plan (call stubs for the differential harness)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: The port-side seam (`PR_SEAM`, `pr_seam`, the `fn_resolve` report)

**Files:**
- Modify: `port/tests/test.h` (`TEST_CASES`, after `X(test_fn_misslog)`), `port/tests/test_platform.c` (a block before `/* ---- test_le.c ---- */`)
- Modify: `port/src/mem.h` (end), `port/src/mem.c` (before and at the top of `fn_resolve_from`), `port/src/game/flow.c` (`sound_voice`'s first line), `port/src/game/actors.c` (`actors_anim_begin`'s first line)
- Create (not committed): `/tmp/e3_mutate.py`

**Interfaces:**
- Produces: `typedef int (*pr_seam_fn)(u32 addr, u32 nargs, const u32 *args, u32 *eax); extern pr_seam_fn pr_seam;`, `PR_SEAM(addr, ...)`, `PR_SEAM_RET(addr, ...)` (record §E3.3); seams on `0x2C3FC` (`sound_voice(id)`) and `0x2BC30` (`actors_anim_begin(rec, stream, frame_bits)`).
- Consumes: `fn_resolve_from` (`mem.c`), test_mem's `fn_register(FN_0002D62C, fn_probe)`.

- [ ] **Step 1: Write the mutation helper (used by Tasks 2-7; restores each file with `git checkout`, so run it only on a committed tree)**

```bash
cat > /tmp/e3_mutate.py <<'PY'
"""E3 mutation proofs (plan 2026-10-01-reverse-e3-call-stubs): apply one mutation, run the tests that
must catch it, print the failing test names, restore the file with `git checkout`. Run it only on a
committed tree (it restores to HEAD)."""
import re, subprocess, sys

MUTS = {
 # Task 2: the port-side seam (port/src/mem.h, mem.c); caught by run_tests' test_call_seam
 "C1": ("port/src/mem.h", "                return;                                                     \\", "                ;                                                           \\"),
 "C2": ("port/src/mem.h", "return seam_e_; ", "return 0;       "),
 "C3": ("port/src/mem.c", "(void)pr_seam(orig_addr, 0, NULL, &eax);", "(void)eax;"),
 "C4": ("port/src/mem.c", "if (fn == NULL && orig_addr != 0 && pr_seam != NULL) {", "if (orig_addr != 0 && pr_seam != NULL) {"),
 # Tasks 3-4: tools/diff_emu.py
 "P1": ("tools/diff_emu.py", "        recorded.append((addr, vals))\n", ""),
 "P2": ("tools/diff_emu.py", "uc.reg_write(ux.UC_X86_REG_ESP, esp + 4 + c.pop)", "uc.reg_write(ux.UC_X86_REG_ESP, esp + 4)"),
 "P3": ("tools/diff_emu.py", "if prev is not None and addr in callset:", "if addr in callset:"),
 "P4": ("tools/diff_emu.py", "            if tgt not in allow and tgt not in callset:\n                kind", "            if True:\n                kind"),
 "P5": ("tools/diff_emu.py", "elif tgt not in allow and tgt not in callset:", "elif tgt not in allow:"),
 "P6": ("tools/diff_emu.py", "                if tgt not in stop:\n                    leaders.add(tgt)", "                if True:\n                    leaders.add(tgt)"),
 "P7": ("tools/diff_emu.py", "                before.setdefault(at + i, b)", "                pass"),
 "P8": ("tools/diff_emu.py", "and _FAMILY.get(p.reg_name(p.operands[0].reg)) == idx):", "):"),
 "P9": ("tools/diff_emu.py", 'elif p.mnemonic == "ja" and guard is not None:', 'elif guard is not None:'),
 "P10": ("tools/diff_emu.py", "            n = (guard.operands[1].imm & ((1 << (8 * width)) - 1)) + 1", "            n = guard.operands[1].imm & ((1 << (8 * width)) - 1)"),
 # Tasks 5 and 7: tools/diff_verify.py
 "V1": ("tools/diff_verify.py", "        if o != p:\n            out.append(\"call #", "        if False:\n            out.append(\"call #"),
 "V2": ("tools/diff_verify.py", 'if d.startswith(("eax ", "byte ", "call #")):', 'if d.startswith(("eax ", "byte ")):'),
 "V3": ("tools/diff_verify.py", "        for k in spec.calls:\n            out.append(\"stub", "        for k in ():\n            out.append(\"stub"),
 "V4": ("tools/diff_verify.py", 'jumps = [a for a in info.indirect if E.decode_at(image, a).mnemonic != "call"]', 'jumps = list(info.indirect)'),
 "V5": ("tools/diff_verify.py", 'if (orig.outcome, orig.detail) != ("unmodeled", spec.gap):', 'if orig.outcome != "unmodeled":'),
 "V6": ("tools/diff_verify.py", "        if both:\n            raise", "        if False:\n            raise"),
 "V7": ("tools/diff_verify.py", '            cur.calls.append((int(t[1], 16), tuple(int(x, 16) for x in t[2:])))', '            pass'),
 "V8": ("tools/diff_verify.py", "    info = E.static_scan(image, spec.entry, stop=[k.addr for k in spec.calls], switches=True)\n    executed, outside", "    info = E.static_scan(image, spec.entry, stop=[k.addr for k in spec.calls])\n    executed, outside"),
 "V9": ("tools/diff_verify.py", '              and all(verdicts.get(a) == "VERIFIED" for a, _ in r.callees)]', '              ]'),
 # Task 6: port/tests/diff_runner.c (diffrun is rebuilt)
 "R1": ("port/tests/diff_runner.c", "    if (s->real) return 0;\n", ""),
 "R2": ("port/tests/diff_runner.c", "        if (w->base >= 0) {", "        if (0) {"),
 "R3": ("port/tests/diff_runner.c", "    if (!register_code()) {", "    if (0) {"),
 "R4": ("port/tests/diff_runner.c", '    for (u32 i = 0; i < nargs; i++) printf(" 0x%X", args[i]);', '    for (u32 i = 1; i < nargs; i++) printf(" 0x%X", args[i]);'),
}

def build(target):
    subprocess.run(["cmake", "--build", "build", "--target", target], capture_output=True)

for tag in sys.argv[1:]:
    path, old, new = MUTS[tag]
    s = open(path).read()
    assert s.count(old) == 1, (tag, s.count(old))
    open(path, "w").write(s.replace(old, new))
    try:
        if tag.startswith("C"):
            build("run_tests")
            out = subprocess.run(["./build/run_tests"], capture_output=True, text=True).stdout
            got = [l.split("/")[-1] for l in out.splitlines() if l.startswith(("FAIL", "all checks"))]
        else:
            if tag.startswith("R"):
                build("diffrun")
            p = subprocess.run([sys.executable, "-m", "unittest", "tools.tests.test_diff_emu",
                                "tools.tests.test_diff_verify"], capture_output=True, text=True)
            got = sorted(set(re.findall(r"^(?:FAIL|ERROR): (\w+)", p.stderr, re.M))) or ["NOTHING FAILED"]
        print("%s -> %s" % (tag, ", ".join(got)))
    finally:
        subprocess.run(["git", "checkout", "-q", "--", path])
        if tag[0] in "CR":
            build("run_tests" if tag.startswith("C") else "diffrun")
PY
python3 -c "import ast; ast.parse(open('/tmp/e3_mutate.py').read()); print('ok')"
```

Expected: `ok`.

- [ ] **Step 2: Write the failing test**

```bash
python3 - <<'PY'
import pathlib
def sub(path, old, new):
    p = pathlib.Path(path); s = p.read_text()
    assert s.count(old) == 1, "%s: expected exactly one match for %r" % (path, old[:60])
    p.write_text(s.replace(old, new))

sub("port/tests/test.h", """    X(test_fn_misslog)  \\
""", """    X(test_fn_misslog)  \\
    X(test_call_seam)   \\
""")
sub("port/tests/test_platform.c", """/* ---- test_le.c ---- */
""", """/* ---- the call seam (mem.h PR_SEAM; record 2026-10-01-reverse-e3 §E3.3) ---- */

static u32 s_seam_n, s_seam_addr[4], s_seam_nargs[4], s_seam_args[4][6];
static int s_seam_stub;                 /* the probe's answer: 1 stubs the call, 0 runs the body */

static int seam_probe(u32 addr, u32 nargs, const u32 *args, u32 *eax)
{
    if (s_seam_n < 4) {
        s_seam_addr[s_seam_n] = addr;
        s_seam_nargs[s_seam_n] = nargs;
        for (u32 i = 0; i < nargs && i < 6; i++) s_seam_args[s_seam_n][i] = args[i];
    }
    s_seam_n++;
    *eax = 0x5Au;
    return s_seam_stub;
}

int test_call_seam(void)
{
    int before = g_failures;
    const u32 rec = 0x03F00000u;              /* above the image, inside mem[] */
    const u32 miss = 0x00FEDC30u;             /* above the code object, never registered */

    /* Stubbed: the hook gets the original address and the C arguments in order, and the body
     * does not run (the 0xA5 sentinels at +0x0C and +0x24, which the body zeroes and sets to the
     * frame, survive). */
    mem_fill(rec, 0xA5u, 0x68u);
    pr_seam = seam_probe;
    s_seam_n = 0u;
    s_seam_stub = 1;
    actors_anim_begin(rec, 0x000EB58Cu, 0x40400000u);
    CHECK_EQ_INT(s_seam_n, 1);
    CHECK_EQ_INT(s_seam_addr[0], 0x2BC30);
    CHECK_EQ_INT(s_seam_nargs[0], 3);
    CHECK(s_seam_args[0][0] == rec && s_seam_args[0][1] == 0x000EB58Cu
          && s_seam_args[0][2] == 0x40400000u, "the arguments in C order");
    CHECK_EQ_INT(DSD(rec + 0x0Cu), 0xA5A5A5A5u);
    CHECK_EQ_INT(DSD(rec + 0x24u), 0xA5A5A5A5u);

    /* PR_SEAM_RET returns the hook's EAX when it stubs, and the body's value when it runs
     * (sound_voice(0) returns 0 at 0x2C401). */
    s_seam_n = 0u;
    CHECK_EQ_INT(sound_voice(0u), 0x5A);
    s_seam_stub = 0;
    CHECK_EQ_INT(sound_voice(0u), 0);
    CHECK_EQ_INT(s_seam_n, 2);
    CHECK(s_seam_addr[0] == 0x2C3FCu && s_seam_addr[1] == 0x2C3FCu, "both calls are seen");
    CHECK(s_seam_nargs[1] == 1u && s_seam_args[1][0] == 0u, "with the voice id");

    /* fn_resolve: an unregistered address is seen with no arguments; a registered one (test_mem's
     * FN_0002D62C) and 0 are not. */
    s_seam_n = 0u;
    s_seam_stub = 1;
    CHECK(fn_resolve(miss) == NULL, "the miss still resolves to NULL");
    CHECK(fn_resolve(FN_0002D62C) == fn_probe, "registered by test_mem");
    CHECK(fn_resolve(0u) == NULL, "0 resolves to NULL");
    CHECK_EQ_INT(s_seam_n, 1);
    CHECK(s_seam_addr[0] == miss && s_seam_nargs[0] == 0u, "the miss, with no arguments");

    pr_seam = NULL;
    return g_failures - before;
}

/* ---- test_le.c ---- */
""")
PY
```

- [ ] **Step 3: Run it to see it fail**

```bash
cmake --build build 2>&1 | grep -E 'error' | sed 's|.*/port/|port/|'
```

Expected (the test cannot compile without the seam):
```
port/tests/test_platform.c:315:5: error: use of undeclared identifier 'pr_seam'
port/tests/test_platform.c:347:5: error: use of undeclared identifier 'pr_seam'
2 errors generated.
```

- [ ] **Step 4: The seam, its hook in `fn_resolve_from`, and the first two seams**

```bash
python3 - <<'PY'
import pathlib
def sub(path, old, new):
    p = pathlib.Path(path); s = p.read_text()
    assert s.count(old) == 1, "%s: expected exactly one match for %r" % (path, old[:60])
    p.write_text(s.replace(old, new))

sub("port/src/mem.h", """void        fn_misslog_report(const char *tag);
""", """void        fn_misslog_report(const char *tag);

/* PORT: the differential harness's call seam (record 2026-10-01-reverse-e3
 * §E3.3); no original instruction. A ported function another one calls opens
 * with PR_SEAM (void) or PR_SEAM_RET (a value), passing its own original
 * address and its arguments in the order of its C signature. pr_seam is NULL
 * in every binary but build/diffrun, which installs a hook: the hook records
 * the call (address and arguments, in order) when the address is in the
 * case's call set, and returns 1 to stub it (the function returns at once,
 * PR_SEAM_RET with *eax) or 0 to run the body. fn_resolve_from also calls the
 * hook, with no arguments, for an unregistered address while pr_seam is set:
 * an indirect call the port cannot make is still recorded. */
typedef int (*pr_seam_fn)(u32 addr, u32 nargs, const u32 *args, u32 *eax);
extern pr_seam_fn pr_seam;
#define PR_SEAM(addr, ...)                                                  \\
    do {                                                                    \\
        if (pr_seam) {                                                      \\
            const u32 seam_a_[] = { __VA_ARGS__ };                          \\
            u32 seam_e_;                                                    \\
            if (pr_seam((addr), (u32)(sizeof seam_a_ / sizeof seam_a_[0]),  \\
                        seam_a_, &seam_e_))                                 \\
                return;                                                     \\
        }                                                                   \\
    } while (0)
#define PR_SEAM_RET(addr, ...)                                              \\
    do {                                                                    \\
        if (pr_seam) {                                                      \\
            const u32 seam_a_[] = { __VA_ARGS__ };                          \\
            u32 seam_e_;                                                    \\
            if (pr_seam((addr), (u32)(sizeof seam_a_ / sizeof seam_a_[0]),  \\
                        seam_a_, &seam_e_))                                 \\
                return seam_e_;                                             \\
        }                                                                   \\
    } while (0)
""")
sub("port/src/mem.c", """void (*fn_resolve_from(u32 orig_addr, const char *ctx))(void)
{
    void (*fn)(void) = (fn_resolve)(orig_addr);
    if (fn != NULL || orig_addr == 0 || !fn_miss_armed) return fn;""", """/* PORT: the call seam's hook (mem.h, record 2026-10-01-reverse-e3 §E3.3);
 * NULL except in build/diffrun. */
pr_seam_fn pr_seam;

void (*fn_resolve_from(u32 orig_addr, const char *ctx))(void)
{
    void (*fn)(void) = (fn_resolve)(orig_addr);
    if (fn == NULL && orig_addr != 0 && pr_seam != NULL) {
        u32 eax;
        (void)pr_seam(orig_addr, 0, NULL, &eax);
    }
    if (fn != NULL || orig_addr == 0 || !fn_miss_armed) return fn;""")
sub("port/src/game/flow.c", """u32 sound_voice(u32 id)
{
    /* PORT: the test seam above; no original instruction. */""", """u32 sound_voice(u32 id)
{
    PR_SEAM_RET(0x2C3FCu, id);
    /* PORT: the test seam above; no original instruction. */""")
sub("port/src/game/actors.c", """void actors_anim_begin(u32 rec, u32 stream, u32 frame_bits)
{
""", """void actors_anim_begin(u32 rec, u32 stream, u32 frame_bits)
{
    PR_SEAM(0x2BC30u, rec, stream, frame_bits);
""")
PY
```

- [ ] **Step 5: Run the suite**

```bash
cmake --build build 2>&1 | grep -E 'warning|error'; ./build/run_tests 2>&1 | tail -1
```

Expected: no warning or error line, then `all checks passed`.

- [ ] **Step 6: Commit**

```bash
git add port/src/mem.h port/src/mem.c port/src/game/flow.c port/src/game/actors.c port/tests/test.h port/tests/test_platform.c
git commit -m "port: PR_SEAM call seam for the differential harness (inert unless diffrun sets it)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 7: Mutation proofs** (each mutation is applied, `run_tests` rebuilt and run, the file restored)

```bash
python3 /tmp/e3_mutate.py C1 C2 C3 C4; git status --short
```

Expected, exactly (`git status` prints nothing):
```
C1 -> test_platform.c:324: 0 != 2779096485, test_platform.c:325: 1077936128 != 2779096485, FAILURES: 2
C2 -> test_platform.c:330: 0 != 90, FAILURES: 1
C3 -> test_platform.c:344: 0 != 1, test_platform.c:345: the miss, with no arguments, FAILURES: 2
C4 -> test_platform.c:344: 2 != 1, FAILURES: 1
```
C1 drops the stub's `return` (the body overwrites the `0xA5` sentinels), C2 makes `PR_SEAM_RET` return 0 instead of the hook's EAX, C3 removes the miss report from `fn_resolve_from`, C4 reports hits too.

- [ ] **Step 8: The gate** (`port/src` changed)

```bash
make verify SMK_DUMP=/tmp/pr_e3_smk TITLE_DUMP=/tmp/pr_e3_title ATTRACT_DUMP=/tmp/pr_e3_att FRONTEND_DUMP=/tmp/pr_e3_fe TITLE_PIN_DIR=/tmp/pr_e3_pin AUDIO_WAV=/tmp/pr_e3.wav K11_DUMP=/tmp/pr_e3_k11 GP_DUMP=/tmp/pr_e3_gp DIFF_IMAGE=/tmp/pr_e3_diffimg DIFF_TABLE=/tmp/pr_e3_diff.md E2_IMAGE=/tmp/pr_e3_e2img > /tmp/pr_e3_verify2.log 2>&1; echo "exit $?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_e3_verify2.log | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo LINES-EQUAL
make audio-render AUDIO_WAV=/tmp/pr_e3.wav >/dev/null 2>&1; cmp /tmp/pr_e3.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-EQUAL
python3 tools/port_progress.py
```

Expected: `exit 0`, `LINES-EQUAL`, `WAV-EQUAL`, `771 1203 64` and `731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)`.

---

### Task 3: The original side: `Call`, stack arguments, interception at the entry, run-time indirect calls

**Files:**
- Modify: `tools/diff_emu.py` (`REGS`; a new `Call` before `OrigResult`; `OrigResult.calls`; `_UC_REG`; `_indirect_target` and `run_original`; `static_scan`'s `stop`)
- Modify: `tools/tests/test_diff_emu.py` (before `if __name__ == "__main__":`)

**Interfaces:**
- Produces: `E.STACK_ARGS = ("s0", "s1", "s2", "s3")`; `E.Call(addr, args=(), mode="stub", eax=0, writes=(), pop=0)` (frozen; `ValueError` on an unknown argument name, an unknown mode, or a `real` call with an effect); `E.run_original(image, entry, regs=None, pokes=None, allow_calls=(), max_insns=..., calls=())` with `OrigResult.calls = [(addr, (values...)), ...]`; `regs` may hold `s0`-`s3`; `E.static_scan(image, entry, max_insns=4000, stop=())`.
- Consumes: E1's `run_original` and `static_scan` (record §E3.2).

- [ ] **Step 1: Write the failing tests**

```bash
python3 - <<'PY'
import pathlib
def sub(path, old, new):
    p = pathlib.Path(path); s = p.read_text()
    assert s.count(old) == 1, "%s: expected exactly one match for %r" % (path, old[:60])
    p.write_text(s.replace(old, new))

sub("tools/tests/test_diff_emu.py", '''

if __name__ == "__main__":''', '''


# ---- E3: call stubs and stack arguments (record 2026-10-01-reverse-e3 §E3.2, §E3.5) ------------

def program(parts):
    """An image from {address: hex bytes}."""
    data = bytearray(0x80100)
    for at, h in parts.items():
        b = bytes.fromhex(h)
        data[at - 0x10000:at - 0x10000 + len(b)] = b
    return E.Image(bytes(data))


# 10000: mov eax,5; mov edx,7; call 0x10020; mov [0x80000],eax; ret
# 10020: mov dword [0x80004],0x11111111; mov eax,9; ret
CALLER = {0x10000: "B805000000" "BA07000000" "E811000000" "A300000800" "C3",
          0x10020: "C70504000800" "11111111" "B809000000" "C3"}
SEED = {0x80000: le32(0xFFFFFFFF), 0x80004: le32(0x22222222)}


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class CallStubTests(unittest.TestCase):
    def test_a_stubbed_call_is_recorded_and_its_bytes_do_not_run(self):
        r = E.run_original(program(CALLER), 0x10000, pokes=SEED,
                           calls=(E.Call(0x10020, ("eax", "edx"), eax=0x42),))
        self.assertEqual((r.outcome, r.calls), ("ok", [(0x10020, (5, 7))]))
        self.assertEqual(r.writes, {0x80000: 0x42, 0x80001: 0, 0x80002: 0, 0x80003: 0})   # 0x80004 untouched
        self.assertNotIn(0x1002A, r.executed)

    def test_a_real_call_runs_the_callee_and_is_recorded(self):
        r = E.run_original(program(CALLER), 0x10000, pokes=SEED,
                           calls=(E.Call(0x10020, ("eax",), mode="real"),))
        self.assertEqual((r.outcome, r.calls, r.regs["eax"]), ("ok", [(0x10020, (5,))], 9))
        self.assertEqual(r.writes[0x80004], 0x11)

    def test_stub_writes_land_at_an_address_or_at_an_argument_plus_offset(self):
        c = E.Call(0x10020, ("eax", "edx"), writes=((None, 0x80008, b"\\xab"), (0, 0x80010 - 5, b"\\xcd")))
        r = E.run_original(program(CALLER), 0x10000, pokes=SEED, calls=(c,))
        self.assertEqual((r.writes[0x80008], r.writes[0x80010]), (0xAB, 0xCD))

    def test_a_stub_write_outside_the_image_stops_the_run(self):
        c = E.Call(0x10020, ("eax",), writes=((None, 0x2000000, b"\\x01"),))
        r = E.run_original(program(CALLER), 0x10000, calls=(c,))
        self.assertEqual(r.outcome, "unmodeled")
        self.assertIn("outside the image", r.detail)

    # 10000: push 0x33; call 0x10020; ret      10020: mov eax,[esp+4]; ret 4
    PUSHER = {0x10000: "6A33" "E819000000" "C3", 0x10020: "8B442404" "C20400"}

    def test_a_stub_pops_its_stack_arguments_and_records_them(self):
        r = E.run_original(program(self.PUSHER), 0x10000, calls=(E.Call(0x10020, ("s0",), pop=4),))
        self.assertEqual((r.outcome, r.calls), ("ok", [(0x10020, (0x33,))]))
        r = E.run_original(program(self.PUSHER), 0x10000, calls=(E.Call(0x10020, ("s0",), pop=0),),
                           max_insns=1000)
        self.assertNotEqual(r.outcome, "ok")      # the caller's ret then pops 0x33 as its return address

    def test_a_case_sets_the_stack_arguments(self):
        r = E.run_original(program({0x10000: "8B442404" "C20400"}), 0x10000, regs={"s0": 0x1234})
        self.assertEqual((r.outcome, r.regs["eax"]), ("ok", 0x1234))

    def test_an_indirect_call_through_a_register_is_resolved_at_run_time(self):
        r = E.run_original(program({0x10000: "FFD0" "C3", 0x10020: "C3"}), 0x10000, regs={"eax": 0x10020},
                           calls=(E.Call(0x10020, ("eax",)),))
        self.assertEqual((r.outcome, r.calls), ("ok", [(0x10020, (0x10020,))]))

    def test_an_indirect_call_through_a_table_is_resolved_at_run_time(self):
        # 10000: call [ebx*4 + 0x80000]; ret
        r = E.run_original(program({0x10000: "FF149D00000800" "C3", 0x10020: "C3"}), 0x10000,
                           regs={"ebx": 1}, pokes={0x80004: le32(0x10020)}, calls=(E.Call(0x10020, ("ebx",)),))
        self.assertEqual((r.outcome, r.calls), ("ok", [(0x10020, (1,))]))

    def test_an_indirect_call_to_an_unlisted_target_names_the_target(self):
        r = E.run_original(program({0x10000: "FFD0" "C3"}), 0x10000, regs={"eax": 0x10030},
                           calls=(E.Call(0x10020),))
        self.assertEqual((r.outcome, r.detail), ("unmodeled", "indirect call to 0x10030 at 0x10000"))

    def test_an_indirect_call_to_an_allowed_target_runs(self):
        r = E.run_original(program({0x10000: "FFD0" "C3", 0x10020: "B809000000" "C3"}), 0x10000,
                           regs={"eax": 0x10020}, allow_calls={0x10020})
        self.assertEqual((r.outcome, r.regs["eax"], r.calls), ("ok", 9, []))

    def test_a_tail_jump_to_a_stubbed_entry_returns_to_the_caller(self):
        r = E.run_original(program({0x10000: "E91B000000", 0x10020: "CC"}), 0x10000, regs={"eax": 3},
                           calls=(E.Call(0x10020, ("eax",), eax=8),))
        self.assertEqual((r.outcome, r.calls, r.regs["eax"]), ("ok", [(0x10020, (3,))], 8))

    def test_falling_into_a_call_set_address_is_not_a_call(self):
        r = E.run_original(program({0x10000: "90" "C3"}), 0x10000, calls=(E.Call(0x10001),))
        self.assertEqual((r.outcome, r.calls), ("ok", []))

    def test_a_call_declaration_is_checked(self):
        for kw in ({"args": ("eax", "foo")}, {"mode": "skip"}, {"mode": "real", "eax": 1},
                   {"mode": "real", "pop": 4}):
            with self.subTest(kw=kw), self.assertRaises(ValueError):
                E.Call(0x10020, **kw)


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class CallScanTests(unittest.TestCase):
    def test_a_jump_to_a_call_set_address_is_a_tail_call_not_scanned(self):
        img = program({0x10000: "E91B000000", 0x10020: "40" "C3"})
        self.assertEqual(sorted(E.static_scan(img, 0x10000).insns), [0x10000, 0x10020, 0x10021])
        self.assertEqual(sorted(E.static_scan(img, 0x10000, stop=[0x10020]).insns), [0x10000])


if __name__ == "__main__":''')
PY
```

- [ ] **Step 2: Run them to see them fail**

```bash
python3 -m unittest tools.tests.test_diff_emu 2>&1 | grep -E 'Error:|^Ran|^FAILED' | sort | uniq -c
```

Expected:
```
   1 AttributeError: 'OrigResult' object has no attribute 'calls'
  14 AttributeError: module 'diff_emu' has no attribute 'Call'
   1 FAILED (errors=17)
   1 KeyError: 's0'
   1 Ran 41 tests in <t>s
   1 TypeError: static_scan() got an unexpected keyword argument 'stop'
```
(17 errors from the 14 new tests: the four subtests of `test_a_call_declaration_is_checked` count one each.)

- [ ] **Step 3: Implement**

```bash
python3 - <<'PY'
import pathlib
def sub(path, old, new):
    p = pathlib.Path(path); s = p.read_text()
    assert s.count(old) == 1, "%s: expected exactly one match for %r" % (path, old[:60])
    p.write_text(s.replace(old, new))

P = "tools/diff_emu.py"
sub(P, '''REGS = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")
''', '''REGS = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")
# The dwords above the return address at entry: the stack arguments a `ret N` function pops
# (record E3 §E3.5). A case sets them as regs["s0"]..regs["s3"]; a call record reads them the same way.
STACK_ARGS = ("s0", "s1", "s2", "s3")
''')
sub(P, '''@dataclass
class OrigResult:''', '''@dataclass(frozen=True)
class Call:
    """A callee in the call set (spec §5.2, record E3 §E3.4). Each arrival at `addr` by a `call` or
    a `jmp` is recorded as (addr, the values of `args`), `args` naming registers or stack slots in
    the order the port's C signature passes them. mode "stub": the callee's bytes do not run; its
    `writes` are applied ((base, offset, bytes): base None is absolute, an int is that argument's
    value), EAX becomes `eax`, and the callee returns, popping `pop` bytes of stack arguments (its
    own `ret N`). mode "real": the bytes run (the callee is proven by its own check)."""
    addr: int
    args: tuple = ()
    mode: str = "stub"
    eax: int = 0
    writes: tuple = ()
    pop: int = 0

    def __post_init__(self):
        if self.mode not in ("stub", "real"):
            raise ValueError("call 0x%X: mode %r is neither stub nor real" % (self.addr, self.mode))
        bad = [a for a in self.args if a not in REGS + STACK_ARGS]
        if bad:
            raise ValueError("call 0x%X: unknown argument %s" % (self.addr, ", ".join(bad)))
        if self.mode == "real" and (self.writes or self.eax or self.pop):
            raise ValueError("call 0x%X: a real call declares no effect" % self.addr)


@dataclass
class OrigResult:''')
sub(P, '''    outside: list = field(default_factory=list)    # (addr, size) read or written outside image and stack
''', '''    outside: list = field(default_factory=list)    # (addr, size) read or written outside image and stack
    calls: list = field(default_factory=list)      # (addr, args tuple) per arrival at a call-set address, in order
''')
sub(P, '''if capstone is not None:
    _md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    _md.detail = True
''', '''if capstone is not None:
    _md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    _md.detail = True

if unicorn is not None:   # capstone's register names -> unicorn's ids, for an indirect call's operand
    _UC_REG = {n: getattr(ux, "UC_X86_REG_" + n.upper())
               for n in ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp")}
''')
sub(P, '''def run_original(image, entry, regs=None, pokes=None, allow_calls=(), max_insns=DEFAULT_MAX_INSNS):''',
'''def _indirect_target(uc, ins):
    """The target of an indirect call, read from the registers and memory as they are now."""
    op = ins.operands[0]
    if op.type == cx86.X86_OP_REG:
        return uc.reg_read(_UC_REG[ins.reg_name(op.reg)])
    m = op.mem
    ea = m.disp
    if m.base:
        ea += uc.reg_read(_UC_REG[ins.reg_name(m.base)])
    if m.index:
        ea += uc.reg_read(_UC_REG[ins.reg_name(m.index)]) * m.scale
    return int.from_bytes(uc.mem_read(ea & 0xFFFFFFFF, 4), "little")


def run_original(image, entry, regs=None, pokes=None, allow_calls=(), max_insns=DEFAULT_MAX_INSNS,
                 calls=()):''')
sub(P, '''    allow_calls: direct call targets the emulator may enter; any other call stops the run as
    `unmodeled` (spec §5.2: the closure the port also runs).
    """''', '''    allow_calls: call targets the emulator may enter, unrecorded; any other call stops the run as
    `unmodeled` (spec §5.2: the closure the port also runs).
    calls: Call entries (record E3 §E3.4): their targets may be called, directly or indirectly,
    and every arrival is recorded in `OrigResult.calls`. With no calls and no allow-list entry for
    it, an indirect call stops the run as E1's "indirect call at" (E1 §E.3).
    """''')
sub(P, '''    for r in REGS:
        mu.reg_write(names[r], 0)
    for r, v in (regs or {}).items():
        mu.reg_write(names[r], v & 0xFFFFFFFF)
    esp = STACK_TOP - 4
    mu.mem_write(esp, SENTINEL.to_bytes(4, "little"))
    mu.reg_write(ux.UC_X86_REG_ESP, esp)
''', '''    for r in REGS:
        mu.reg_write(names[r], 0)
    esp = STACK_TOP - 4
    for r, v in (regs or {}).items():
        if r in STACK_ARGS:
            mu.mem_write(esp + 4 + 4 * STACK_ARGS.index(r), (v & 0xFFFFFFFF).to_bytes(4, "little"))
        else:
            mu.reg_write(names[r], v & 0xFFFFFFFF)
    mu.mem_write(esp, SENTINEL.to_bytes(4, "little"))
    mu.reg_write(ux.UC_X86_REG_ESP, esp)
''')
sub(P, '''    state = {"stop": None}
    executed = set()
    before = {}            # addr -> byte before the run's first write to it
    outside = set()
    seen = {}              # addr -> mnemonic class, so each address is decoded once
    allow = frozenset(allow_calls)
''', '''    state = {"stop": None, "prev": None}
    executed = set()
    before = {}            # addr -> byte before the run's first write to it
    outside = set()
    seen = {}              # addr -> (stop kind or None, transfer "call"/"jmp"/None, indirect call insn or None)
    allow = frozenset(allow_calls)
    callset = {c.addr: c for c in calls}
    recorded = []

    def arg(uc, esp, a):
        if a in STACK_ARGS:
            return int.from_bytes(uc.mem_read(esp + 4 + 4 * STACK_ARGS.index(a), 4), "little")
        return uc.reg_read(names[a])

    def arrive(uc, addr, c):
        """Record the arrival at a call-set address; for a stub, apply its effect and return."""
        esp = uc.reg_read(ux.UC_X86_REG_ESP)
        vals = tuple(arg(uc, esp, a) for a in c.args)
        recorded.append((addr, vals))
        if c.mode == "real":
            return
        for base, off, data in c.writes:
            at = (off if base is None else vals[base] + off) & 0xFFFFFFFF
            if not image.contains(at, len(data)):
                raise ValueError("stub 0x%X writes 0x%X+%d, outside the image" % (addr, at, len(data)))
            for i, b in enumerate(uc.mem_read(at, len(data))):
                before.setdefault(at + i, b)
            uc.mem_write(at, bytes(data))
        uc.reg_write(ux.UC_X86_REG_EAX, c.eax & 0xFFFFFFFF)
        ret = int.from_bytes(uc.mem_read(esp, 4), "little")
        uc.reg_write(ux.UC_X86_REG_ESP, esp + 4 + c.pop)
        uc.reg_write(ux.UC_X86_REG_EIP, ret)
''')
sub(P, '''    def on_code(uc, addr, size, _):
        executed.add(addr)
        if addr in seen:
            kind = seen[addr]
        else:
            ins = _decode(uc.mem_read(addr, 15), addr)
            kind = None
            if ins is None:
                kind = ("unmodeled", "undecodable bytes at 0x%X" % addr)
            elif ins.mnemonic in UNMODELED_MNEMONICS:
                kind = ("unmodeled", "%s at 0x%X" % (ins.mnemonic, addr))
            elif ins.mnemonic == "call":
                tgt = _direct_target(ins)
                if tgt is None:
                    kind = ("unmodeled", "indirect call at 0x%X" % addr)
                elif tgt not in allow:
                    kind = ("unmodeled", "call 0x%X from 0x%X" % (tgt, addr))
            seen[addr] = kind
        if kind is not None and state["stop"] is None:
            state["stop"] = kind
            uc.emu_stop()
''', '''    def on_code(uc, addr, size, _):
        executed.add(addr)
        prev, state["prev"] = state["prev"], None
        if prev is not None and addr in callset:      # arrived by a call or a jmp (record E3 §E3.2)
            try:
                arrive(uc, addr, callset[addr])
            except ValueError as e:
                state["stop"] = ("unmodeled", str(e))
                uc.emu_stop()
                return
            if callset[addr].mode == "stub":
                return
        if addr in seen:
            kind, transfer, ind = seen[addr]
        else:
            ins = _decode(uc.mem_read(addr, 15), addr)
            kind, transfer, ind = None, None, None
            if ins is None:
                kind = ("unmodeled", "undecodable bytes at 0x%X" % addr)
            elif ins.mnemonic in UNMODELED_MNEMONICS:
                kind = ("unmodeled", "%s at 0x%X" % (ins.mnemonic, addr))
            elif ins.mnemonic == "call":
                transfer = "call"
                tgt = _direct_target(ins)
                if tgt is None:
                    ind = ins
                elif tgt not in allow and tgt not in callset:
                    kind = ("unmodeled", "call 0x%X from 0x%X" % (tgt, addr))
            elif ins.mnemonic == "jmp":
                transfer = "jmp"
            seen[addr] = (kind, transfer, ind)
        if ind is not None and kind is None:          # an indirect call: its target, as of now
            tgt = _indirect_target(uc, ind)
            if tgt not in allow and tgt not in callset:
                kind = ("unmodeled", "indirect call at 0x%X" % addr if not callset and not allow
                        else "indirect call to 0x%X at 0x%X" % (tgt, addr))
        if kind is not None and state["stop"] is None:
            state["stop"] = kind
            uc.emu_stop()
            return
        state["prev"] = transfer
''')
sub(P, '''    return OrigResult(outcome, detail, final_regs, writes, executed, sorted(outside))''',
'''    return OrigResult(outcome, detail, final_regs, writes, executed, sorted(outside), recorded)''')
sub(P, '''def static_scan(image, entry, max_insns=4000):
    insns, leaders, indirect, unresolved = {}, {entry}, [], []''', '''def static_scan(image, entry, max_insns=4000, stop=()):
    """`stop`: call-set addresses (record E3 §E3.4); a jump to one is a tail call, not followed."""
    stop = frozenset(stop)
    insns, leaders, indirect, unresolved = {}, {entry}, [], []''')
sub(P, '''                tgt = _direct_target(ins)
                if tgt is None:
                    indirect.append(addr)
                    break
                leaders.add(tgt)
                work.append(tgt)''', '''                tgt = _direct_target(ins)
                if tgt is None:
                    indirect.append(addr)
                    break
                if tgt not in stop:
                    leaders.add(tgt)
                    work.append(tgt)''')
PY
```

- [ ] **Step 4: Run both suites, and E2's**

```bash
python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify 2>&1 | tail -1
python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify 2>&1 | grep '^Ran'
make entry-triage E2_IMAGE=/tmp/pr_e3_e2img 2>&1 | tail -3
```

Expected: `OK`, `Ran 85 tests` (E1's 71 and 14 new), and the three `entry-triage:` lines of E2's record §E2.9 (`579 candidates (575 by U0's rule); ...`, `targets 329 unported, 166 ported; ...`, `voice sites outside Ghidra 134: 48 in unported code, 67 in ported code, 19 nowhere`) with exit 0.

- [ ] **Step 5: Commit**

```bash
git add tools/diff_emu.py tools/tests/test_diff_emu.py
git commit -m "tools: diff_emu call stubs, stack arguments, run-time indirect calls

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 6: Mutation proofs**

```bash
python3 /tmp/e3_mutate.py P1 P2 P3 P4 P5 P6 P7; git status --short
```

Expected, exactly:
```
P1 -> test_a_real_call_runs_the_callee_and_is_recorded, test_a_stub_pops_its_stack_arguments_and_records_them, test_a_stubbed_call_is_recorded_and_its_bytes_do_not_run, test_a_tail_jump_to_a_stubbed_entry_returns_to_the_caller, test_an_indirect_call_through_a_register_is_resolved_at_run_time, test_an_indirect_call_through_a_table_is_resolved_at_run_time
P2 -> test_a_stub_pops_its_stack_arguments_and_records_them
P3 -> test_falling_into_a_call_set_address_is_not_a_call
P4 -> test_an_indirect_call_through_a_register_is_resolved_at_run_time, test_an_indirect_call_through_a_table_is_resolved_at_run_time, test_an_indirect_call_to_an_allowed_target_runs
P5 -> test_a_real_call_runs_the_callee_and_is_recorded, test_a_stub_pops_its_stack_arguments_and_records_them, test_a_stub_write_outside_the_image_stops_the_run, test_a_stubbed_call_is_recorded_and_its_bytes_do_not_run, test_stub_writes_land_at_an_address_or_at_an_argument_plus_offset
P6 -> test_a_jump_to_a_call_set_address_is_a_tail_call_not_scanned
P7 -> test_stub_writes_land_at_an_address_or_at_an_argument_plus_offset
```
P1 stops recording, P2 forgets the callee's `ret N` (`esp + 4`), P3 intercepts a fall-through, P4 refuses every indirect target, P5 refuses a direct call to a call-set target, P6 scans into a tail-called callee, P7 forgets a stub write's old byte.

---

### Task 4: Bounded switches in the static scan

**Files:**
- Modify: `tools/diff_emu.py` (`_FAMILY`, `switch_cases` before `static_scan`; `static_scan`'s `switches` and its per-run instruction path)
- Modify: `tools/tests/test_diff_emu.py` (before `if __name__ == "__main__":`)

**Interfaces:**
- Produces: `E.switch_cases(image, ins, prior)` -> list of case targets or `None` (record §E3.7: `cmp r, imm` on the index register's family, then `ja`, then the `jmp [R*4+T]` with no base); `E.static_scan(..., switches=False)`; with `switches=True` a bounded switch's targets become leaders and it is not reported as indirect.
- Consumes: Task 3's `static_scan(stop=)`.

- [ ] **Step 1: Write the failing tests**

```bash
python3 - <<'PY'
import pathlib
def sub(path, old, new):
    p = pathlib.Path(path); s = p.read_text()
    assert s.count(old) == 1, "%s: expected exactly one match for %r" % (path, old[:60])
    p.write_text(s.replace(old, new))

sub("tools/tests/test_diff_emu.py", '''

if __name__ == "__main__":''', '''


# 10000: cmp al,2; ja 10010; and eax,0xff; jmp [eax*4+0x10100]; (10010) ret
# table 10100: 10011 10012 10013, each a ret (record E3 §E3.7)
SWITCH = {0x10000: "3C02" "770C" "25FF000000" "FF248500010100" "C3" "C3C3C3",
          0x10100: "11000100" "12000100" "13000100"}


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class SwitchScanTests(unittest.TestCase):
    def test_a_bounded_switch_is_followed_when_asked(self):
        info = E.static_scan(program(SWITCH), 0x10000, switches=True)
        self.assertEqual(info.indirect, [])
        self.assertEqual(info.leaders, [0x10000, 0x10004, 0x10010, 0x10011, 0x10012, 0x10013])

    def test_without_switches_the_scan_is_e1s(self):
        self.assertEqual(E.static_scan(program(SWITCH), 0x10000).indirect, [0x10009])

    def test_a_guard_on_another_register_does_not_bound_it(self):
        other = {**SWITCH, 0x10000: "80FB02" "770B" "25FF000000" "FF248500010100" "C3"}   # cmp bl,2
        self.assertEqual(E.static_scan(program(other), 0x10000, switches=True).indirect, [0x1000A])

    def test_a_guard_without_ja_does_not_bound_it(self):
        below = {**SWITCH, 0x10000: "3C02" "720C" "25FF000000" "FF248500010100" "C3"}     # jb, not ja
        self.assertEqual(E.static_scan(program(below), 0x10000, switches=True).indirect, [0x10009])


if __name__ == "__main__":''')
PY
```

- [ ] **Step 2: Run them to see them fail**

```bash
python3 -m unittest tools.tests.test_diff_emu 2>&1 | grep -E 'Error:|^Ran|^FAILED' | sort | uniq -c
```

Expected:
```
   1 FAILED (errors=3)
   1 Ran 45 tests in <t>s
   3 TypeError: static_scan() got an unexpected keyword argument 'switches'
```
(`test_without_switches_the_scan_is_e1s` already passes: it pins E1's behaviour.)

- [ ] **Step 3: Implement**

```bash
python3 - <<'PY'
import pathlib
def sub(path, old, new):
    p = pathlib.Path(path); s = p.read_text()
    assert s.count(old) == 1, "%s: expected exactly one match for %r" % (path, old[:60])
    p.write_text(s.replace(old, new))

P = "tools/diff_emu.py"
sub(P, '''def static_scan(image, entry, max_insns=4000, stop=()):
    """`stop`: call-set addresses (record E3 §E3.4); a jump to one is a tail call, not followed."""
    stop = frozenset(stop)
    insns, leaders, indirect, unresolved = {}, {entry}, [], []
    work, truncated = [entry], False''', '''# The registers whose low part a guard may compare (record E3 §E3.7): cmp al/ax/eax all bound eax.
_FAMILY = {r: fam for fam, rs in {
    "eax": ("al", "ax", "eax"), "ebx": ("bl", "bx", "ebx"), "ecx": ("cl", "cx", "ecx"),
    "edx": ("dl", "dx", "edx"), "esi": ("si", "esi"), "edi": ("di", "edi"), "ebp": ("bp", "ebp")}.items()
    for r in rs}


def switch_cases(image, ins, prior):
    """The case targets of a bounded switch `jmp dword ptr [R*4 + T]` (record E3 §E3.7), else None.
    Bounded means: among the (up to five) instructions `prior` that precede it on its own straight
    path, a `cmp r, imm` with r in R's family, followed by a `ja` before the jmp; the table then
    holds imm + 1 dwords (imm masked to the compared width). Stricter than E2's rule (E2 §E2.2),
    which does not check the register or the `ja`."""
    mem = [op for op in ins.operands if op.type == cx86.X86_OP_MEM]
    if ins.mnemonic != "jmp" or not mem or not mem[0].mem.index or mem[0].mem.scale != 4 or mem[0].mem.base:
        return None
    idx = _FAMILY.get(ins.reg_name(mem[0].mem.index))
    guard = None
    for p in prior:
        if (p.mnemonic == "cmp" and len(p.operands) == 2 and p.operands[0].type == cx86.X86_OP_REG
                and p.operands[1].type == cx86.X86_OP_IMM and _FAMILY.get(p.reg_name(p.operands[0].reg)) == idx):
            guard = p
        elif p.mnemonic == "ja" and guard is not None:
            width = guard.operands[0].size
            n = (guard.operands[1].imm & ((1 << (8 * width)) - 1)) + 1
            table = mem[0].mem.disp & 0xFFFFFFFF
            if not image.contains(table, 4 * n):
                return None
            return [int.from_bytes(image.bytes_at(table + 4 * k, 4), "little") for k in range(n)]
    return None


def static_scan(image, entry, max_insns=4000, stop=(), switches=False):
    """`stop`: call-set addresses (record E3 §E3.4); a jump to one is a tail call, not followed.
    `switches`: follow a bounded switch's case targets (switch_cases) instead of flagging it."""
    stop = frozenset(stop)
    insns, leaders, indirect, unresolved = {}, {entry}, [], []
    work, truncated = [entry], False''')
sub(P, '''        addr = work.pop()
        while True:
            if addr in insns:
                break''', '''        addr = work.pop()
        path = []                     # the instructions of this straight run, for a switch's guard
        while True:
            if addr in insns:
                break''')
sub(P, '''            insns[addr] = ins.size
            nxt = addr + ins.size
            m = ins.mnemonic''', '''            insns[addr] = ins.size
            nxt = addr + ins.size
            m = ins.mnemonic
            path = (path + [ins])[-6:]''')
sub(P, '''                tgt = _direct_target(ins)
                if tgt is None:
                    indirect.append(addr)
                    break
                if tgt not in stop:''', '''                tgt = _direct_target(ins)
                if tgt is None:
                    cases = switch_cases(image, ins, path[:-1]) if switches else None
                    if cases is None:
                        indirect.append(addr)
                    else:
                        for t in cases:
                            if t not in stop:
                                leaders.add(t)
                                work.append(t)
                    break
                if tgt not in stop:''')
PY
```

- [ ] **Step 4: Run the suites, E2's gate, and the real switches**

```bash
python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify 2>&1 | grep -E '^Ran|^OK'
make entry-triage E2_IMAGE=/tmp/pr_e3_e2img 2>&1 | tail -3
python3 - <<'PY'
import sys; sys.path.insert(0, "tools")
import diff_emu as E
img = E.Image.load("/tmp/pr_e3_img.bin")
for a in (0x2C3FC, 0x2B2A0, 0x1BBAC, 0x18350):
    p = E.static_scan(img, a); s = E.static_scan(img, a, switches=True)
    print("%05X plain %d leaders, indirect %s | switches %d leaders, indirect %s" % (
        a, len(p.leaders), " ".join("%X" % x for x in p.indirect) or "-",
        len(s.leaders), " ".join("%X" % x for x in s.indirect) or "-"))
PY
```

Expected: `Ran 89 tests`, `OK`; the same three `entry-triage:` lines as Task 3; then (record §E3.7):
```
2C3FC plain 7 leaders, indirect 2C42F | switches 100 leaders, indirect -
2B2A0 plain 6 leaders, indirect 2B2FC | switches 85 leaders, indirect 2B56D 2B594 2B5EA
1BBAC plain 35 leaders, indirect 1BD06 1BD7C | switches 42 leaders, indirect 1BD7C
18350 plain 6 leaders, indirect 18384 | switches 6 leaders, indirect 18384
```

- [ ] **Step 5: Commit**

```bash
git add tools/diff_emu.py tools/tests/test_diff_emu.py
git commit -m "tools: diff_emu follows bounded switches (cmp on the index family, then ja)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 6: Mutation proofs**

```bash
python3 /tmp/e3_mutate.py P8 P9 P10; git status --short
```

Expected, exactly:
```
P8 -> test_a_guard_on_another_register_does_not_bound_it
P9 -> test_a_guard_without_ja_does_not_bound_it
P10 -> test_a_bounded_switch_is_followed_when_asked
```
P8 drops the register-family check, P9 drops the `ja` requirement, P10 takes `imm` cases instead of `imm + 1`.

---

### Task 5: The driver: call lists, named gaps, the callee column, the counter line

**Files:**
- Modify: `tools/diff_verify.py` (`Spec`, `PortResult`, `cases_text`, `parse_port_output`, `_call_text` and `compare`, `SpecResult`, `verify_gap` and `verify_spec`, `verify_all`, `mutant_detection`, `table_row`/`TABLE_HEAD`, `main`)
- Modify: `tools/tests/test_diff_verify.py` (before `if __name__ == "__main__":`)

**Interfaces:**
- Produces: `Spec(..., calls=(), gap="")` (`ValueError` when an address is both allowed and in `calls`); `PortResult.calls`; the case file's `stub 0x<addr> stub|real 0x<eax>` and `swrite abs|arg<k> 0x<off> <hex>` lines; the output's `c 0x<addr> 0x<arg>...` lines (before `ret`); problems `call #<i>: original 0x<a>(<args>), port ...`; `verify_gap(spec, image)` -> `NAMED_GAP` or `MISMATCH`; `SpecResult.callees`, `.gap`; `table_row(r, verdicts=None)`; the counter line `diff-verify: F/F functions VERIFIED; M/M mutants detected; G named gaps; C/F with every callee VERIFIED. Claim: ...`.
- Consumes: Tasks 3-4 (`E.Call`, `run_original(calls=)`, `static_scan(stop=, switches=)`).

- [ ] **Step 1: Write the failing tests**

```bash
python3 - <<'PY'
import pathlib
def sub(path, old, new):
    p = pathlib.Path(path); s = p.read_text()
    assert s.count(old) == 1, "%s: expected exactly one match for %r" % (path, old[:60])
    p.write_text(s.replace(old, new))

sub("tools/tests/test_diff_verify.py", '''

if __name__ == "__main__":''', '''

# ---- E3: the call list, named gaps, the callee column (record 2026-10-01-reverse-e3 §E3.4, §E3.8) --

def program(parts):
    data = bytearray(0x80100)
    for at, h in parts.items():
        b = bytes.fromhex(h)
        data[at - 0x10000:at - 0x10000 + len(b)] = b
    return E.Image(bytes(data))


# 10000: mov eax,5; mov edx,7; call 0x10020; mov [0x80000],eax; ret    10020: (not run when stubbed) ret
CALLER = program({0x10000: "B805000000" "BA07000000" "E811000000" "A300000800" "C3", 0x10020: "C3"})
STUB = E.Call(0x10020, ("eax", "edx"), eax=0x42)
WROTE = {0x80000: 0x42, 0x80001: 0, 0x80002: 0, 0x80003: 0}
SEEDED = {0x80000: le32(0xFFFFFFFF)}


class CallParseTests(unittest.TestCase):
    def test_cases_text_emits_the_call_set(self):
        spec = V.Spec("f", 0x10000, [V.Case("a", {"eax": 5, "s0": 0x40400000})], calls=(
            E.Call(0x10020, ("eax",), eax=1, writes=((None, 0x80008, b"\\xab"), (0, 4, b"\\x01\\x02"))),
            E.Call(0x10030, mode="real")))
        self.assertEqual(V.cases_text(spec, "f"),
                         "case a\\nfn f\\nreg eax 0x5\\nreg s0 0x40400000\\n"
                         "stub 0x10020 stub 0x1\\nswrite abs 0x80008 ab\\nswrite arg0 0x4 0102\\n"
                         "stub 0x10030 real 0x0\\nend\\n")

    def test_call_lines_are_parsed_in_order(self):
        out = V.parse_port_output("case a\\nc 0x10020 0x5 0x7\\nc 0x10030\\nret eax 0x0 mask 0x0\\nend\\n")
        self.assertEqual(out["a"].calls, [(0x10020, (5, 7)), (0x10030, ())])

    def test_a_call_line_after_the_result_is_rejected(self):
        with self.assertRaises(ValueError):
            V.parse_port_output("case a\\nret eax 0x0 mask 0x0\\nc 0x10020\\nend\\n")

    def test_an_address_both_allowed_and_in_the_call_set_is_refused(self):
        with self.assertRaises(ValueError):
            V.Spec("f", 0x10000, [], allow_calls=(0x10020,), calls=(E.Call(0x10020),))

    def test_the_table_marks_each_callee_with_its_own_check(self):
        r = V.SpecResult("f", 0x10000, "VERIFIED", 1, 1, 1, callees=[(0x10020, "stub"), (0x10030, "allow")])
        self.assertIn("| 10020 stub VERIFIED, 10030 allow unverified |", V.table_row(r, {0x10020: "VERIFIED"}))


@needs_unicorn
class CallVerifyTests(unittest.TestCase):
    def spec(self, **kw):
        return V.Spec("c", 0x10000, [V.Case("x", {}, SEEDED)], calls=(STUB,), **kw)

    def test_agreeing_calls_verify(self):
        r = V.verify_spec(self.spec(), CALLER, {"x": V.PortResult(0x42, writes=WROTE, calls=[(0x10020, (5, 7))])})
        self.assertEqual((r.verdict, r.hit, r.total, r.problems), ("VERIFIED", 1, 1, []))

    def test_a_missing_call_is_a_mismatch_and_counts_as_a_detection(self):
        r = V.verify_spec(self.spec(), CALLER, {"x": V.PortResult(0x42, writes=WROTE)})
        self.assertEqual((r.verdict, r.problems), ("MISMATCH", ["x: call #0: original 0x10020(0x5, 0x7), port none"]))
        self.assertEqual(V.mutant_detection(r), (True, ""))

    def test_a_call_with_another_argument_is_a_mismatch(self):
        r = V.verify_spec(self.spec(), CALLER, {"x": V.PortResult(0x42, writes=WROTE, calls=[(0x10020, (5, 8))])})
        self.assertEqual(r.problems, ["x: call #0: original 0x10020(0x5, 0x7), port 0x10020(0x5, 0x8)"])

    def test_an_extra_call_is_a_mismatch(self):
        r = V.verify_spec(self.spec(), CALLER, {"x": V.PortResult(
            0x42, writes=WROTE, calls=[(0x10020, (5, 7)), (0x10020, (5, 7))])})
        self.assertEqual(r.problems, ["x: call #1: original none, port 0x10020(0x5, 0x7)"])

    def test_an_indirect_call_that_resolved_does_not_keep_the_function_partial(self):
        img = program({0x10000: "FFD0" "C3", 0x10020: "C3"})
        spec = V.Spec("i", 0x10000, [V.Case("x", {"eax": 0x10020})], calls=(E.Call(0x10020, ("eax",)),))
        r = V.verify_spec(spec, img, {"x": V.PortResult(0, calls=[(0x10020, (0x10020,))])})
        self.assertEqual((r.verdict, r.problems), ("VERIFIED", []))

    # 10000: cmp al,2; ja 10010; and eax,0xff; jmp [eax*4+0x10100]; (10010) ret; 10011..10013 ret
    SWITCH = program({0x10000: "3C02" "770C" "25FF000000" "FF248500010100" "C3" "C3C3C3",
                      0x10100: "11000100" "12000100" "13000100"})

    def test_a_bounded_switch_counts_its_cases_as_blocks(self):
        cases = [V.Case("s%d" % n, {"eax": n}) for n in range(4)]
        port = {"s%d" % n: V.PortResult(n) for n in range(4)}
        r = V.verify_spec(V.Spec("s", 0x10000, cases), self.SWITCH, port)
        self.assertEqual((r.verdict, r.hit, r.total), ("VERIFIED", 6, 6))
        r = V.verify_spec(V.Spec("s", 0x10000, cases[:3]), self.SWITCH, {k: port[k] for k in ("s0", "s1", "s2")})
        self.assertEqual((r.verdict, r.unhit), ("PARTIAL", [0x10010]))


@needs_unicorn
class GapTests(unittest.TestCase):
    IN = program({0x10000: "EC" "C3"})           # in al,dx; ret

    def test_a_named_gap_holds_when_every_case_stops_on_it(self):
        r = V.verify_gap(V.Spec("g", 0x10000, [V.Case("x", {})], mutants=(), gap="in at 0x10000"), self.IN)
        self.assertEqual((r.verdict, r.problems, r.gap), ("NAMED_GAP", [], "in at 0x10000"))

    def test_a_misnamed_gap_is_a_mismatch(self):
        r = V.verify_gap(V.Spec("g", 0x10000, [V.Case("x", {})], mutants=(), gap="in at 0x10001"), self.IN)
        self.assertEqual(r.verdict, "MISMATCH")

    def test_a_gap_the_original_runs_through_is_stale(self):
        r = V.verify_gap(V.Spec("g", 0x10000, [V.Case("x", {})], mutants=(), gap="in at 0x10000"),
                         program({0x10000: "C3"}))
        self.assertEqual(r.verdict, "MISMATCH")
        self.assertIn("the original ok (returned)", r.problems[0])


if __name__ == "__main__":''')
PY
```

- [ ] **Step 2: Run them to see them fail**

```bash
python3 -m unittest tools.tests.test_diff_verify 2>&1 | grep -E 'Error:|^Ran|^FAILED|^FAIL:' | sed 's/ (tools.*//' | sort | uniq -c
```

Expected:
```
   1 AssertionError: Tuples differ: ('PARTIAL', 3, 3) != ('VERIFIED', 6, 6)
   3 AttributeError: module 'diff_verify' has no attribute 'verify_gap'. Did you mean: 'verify_all'?
   1 FAIL: test_a_bounded_switch_counts_its_cases_as_blocks
   1 FAILED (failures=1, errors=12)
   1 Ran 58 tests in <t>s
   7 TypeError: Spec.__init__() got an unexpected keyword argument 'calls'
   1 TypeError: SpecResult.__init__() got an unexpected keyword argument 'callees'
   1 ValueError: diffrun output: unrecognised line 'c 0x10020 0x5 0x7'
```

- [ ] **Step 3: Implement**

```bash
python3 - <<'PY'
import pathlib
def sub(path, old, new):
    p = pathlib.Path(path); s = p.read_text()
    assert s.count(old) == 1, "%s: expected exactly one match for %r" % (path, old[:60])
    p.write_text(s.replace(old, new))

P = "tools/diff_verify.py"
sub(P, '''    mutants: tuple = ("@mutant",)    # diffrun binding suffixes that must be reported as MISMATCH
    eax_mask: int = 0xFFFFFFFF       # the part of the original's EAX the callers read (full = conservative)
''', '''    mutants: tuple = ("@mutant",)    # diffrun binding suffixes that must be reported as MISMATCH
    eax_mask: int = 0xFFFFFFFF       # the part of the original's EAX the callers read (full = conservative)
    calls: tuple = ()                # E.Call entries: the callees stubbed or run on both sides, recorded (record E3 §E3.4)
    gap: str = ""                    # a named gap (spec §5.2): the blocking instruction every case must stop on
''')
sub(P, '''        if dup:
            raise ValueError("spec %s: duplicate case id %s" % (self.name, ", ".join(dup)))
''', '''        if dup:
            raise ValueError("spec %s: duplicate case id %s" % (self.name, ", ".join(dup)))
        both = sorted(set(self.allow_calls) & {c.addr for c in self.calls})
        if both:
            raise ValueError("spec %s: 0x%X is both allowed and in the call set" % (self.name, both[0]))
''')
sub(P, '''    writes: dict = field(default_factory=dict)
    error: str = ""


def cases_text(spec, port_name):''', '''    writes: dict = field(default_factory=dict)
    error: str = ""
    calls: list = field(default_factory=list)      # (addr, args tuple), in the order the seam saw them


def cases_text(spec, port_name):''')
sub(P, '''        for a, b in c.pokes.items():
            out.append("poke 0x%X %s" % (a, bytes(b).hex()))
        out.append("end")''', '''        for a, b in c.pokes.items():
            out.append("poke 0x%X %s" % (a, bytes(b).hex()))
        for k in spec.calls:
            out.append("stub 0x%X %s 0x%X" % (k.addr, k.mode, k.eax & 0xFFFFFFFF))
            for base, off, data in k.writes:
                out.append("swrite %s 0x%X %s" % ("abs" if base is None else "arg%d" % base, off, bytes(data).hex()))
        out.append("end")''')
sub(P, '''        elif t[0] == "w" and len(t) == 3:''', '''        elif t[0] == "c" and len(t) >= 2:
            if got:
                raise ValueError("diffrun output: a call line after the result line: %r" % line)
            cur.calls.append((int(t[1], 16), tuple(int(x, 16) for x in t[2:])))
        elif t[0] == "w" and len(t) == 3:''')
sub(P, '''def compare(orig, port, mask=0xFFFFFFFF):''', '''def _call_text(c):
    if c is None:
        return "none"
    return "0x%X(%s)" % (c[0], ", ".join("0x%X" % v for v in c[1]))


def compare(orig, port, mask=0xFFFFFFFF):''')
sub(P, '''    for a in sorted(set(orig.writes) | set(port.writes)):''', '''    oc, pc = list(orig.calls), list(port.calls)
    for i in range(max(len(oc), len(pc))):
        o = oc[i] if i < len(oc) else None
        p = pc[i] if i < len(pc) else None
        if o != p:
            out.append("call #%d: original %s, port %s" % (i, _call_text(o), _call_text(p)))
    for a in sorted(set(orig.writes) | set(port.writes)):''')
sub(P, '''    diffs: int = 0                                   # eax and byte differences (not port errors)
''', '''    diffs: int = 0                                   # eax, call and byte differences (not port errors)
    callees: list = field(default_factory=list)      # (addr, how) for every allowed or call-set callee
    gap: str = ""                                    # the named gap's blocking instruction, when the spec is one
''')
sub(P, '''def verify_spec(spec, image, port_results, port_name=None):''', '''def verify_gap(spec, image):
    """A named gap (spec §5.2): every case must stop on exactly the instruction the spec names. The
    port is not run. A case that runs through, or stops elsewhere, is a MISMATCH: the gap is stale
    or misnamed, and the function becomes a verification target again."""
    info = E.static_scan(image, spec.entry, stop=[k.addr for k in spec.calls], switches=True)
    res = SpecResult(spec.name, spec.entry, "NAMED_GAP", len(spec.cases), 0, len(info.leaders), gap=spec.gap)
    executed = set()
    for c in spec.cases:
        orig = E.run_original(image, spec.entry, c.regs, c.pokes, spec.allow_calls, calls=spec.calls)
        executed |= orig.executed
        if (orig.outcome, orig.detail) != ("unmodeled", spec.gap):
            res.verdict = "MISMATCH"
            res.problems.append("%s: the gap names '%s', the original %s (%s)" % (
                c.id, spec.gap, orig.outcome, orig.detail or "returned"))
    res.hit = len(E.coverage(info, executed)[0])
    return res


def verify_spec(spec, image, port_results, port_name=None):''')
sub(P, '''    info = E.static_scan(image, spec.entry)
    executed, outside, problems, blocked = set(), set(), [], []
    port_errors, diffs = [], 0
    for c in spec.cases:
        orig = E.run_original(image, spec.entry, c.regs, c.pokes, spec.allow_calls)''', '''    info = E.static_scan(image, spec.entry, stop=[k.addr for k in spec.calls], switches=True)
    executed, outside, problems, blocked = set(), set(), [], []
    port_errors, diffs = [], 0
    for c in spec.cases:
        orig = E.run_original(image, spec.entry, c.regs, c.pokes, spec.allow_calls, calls=spec.calls)''')
sub(P, '''            if d.startswith(("eax ", "byte ")):     # the strings compare() emits for a real difference
                diffs += 1''', '''            if d.startswith(("eax ", "byte ", "call #")):   # the strings compare() emits for a real difference
                diffs += 1''')
sub(P, '''    if problems:
        res.verdict = "MISMATCH"
    elif blocked:
        res.verdict, res.problems = "NOT_EXERCISABLE", blocked
    elif res.unhit or info.indirect or info.truncated:
        res.verdict = "PARTIAL"
        if info.indirect:
            res.problems.append("indirect jmp/call at %s: targets unknown" % ",".join(hex(a) for a in info.indirect))''',
'''    res.callees = sorted([(a, "allow") for a in spec.allow_calls] + [(k.addr, k.mode) for k in spec.calls])
    # An indirect call hides no block of the function (it returns to the next instruction, which the
    # scan follows), and a run that reached one resolved its target into the allow-list or the call
    # set, or stopped (NOT_EXERCISABLE): only an indirect jump that is not a bounded switch keeps a
    # function PARTIAL (record E3 §E3.7).
    jumps = [a for a in info.indirect if E.decode_at(image, a).mnemonic != "call"]
    if problems:
        res.verdict = "MISMATCH"
    elif blocked:
        res.verdict, res.problems = "NOT_EXERCISABLE", blocked
    elif res.unhit or jumps or info.truncated:
        res.verdict = "PARTIAL"
        if jumps:
            res.problems.append("indirect jmp/call at %s: targets unknown" % ",".join(hex(a) for a in jumps))''')
sub(P, '''        names = [spec.name + s for s in spec.mutants] if mutants else [spec.name]
        for name in names:''', '''        if spec.gap:
            if not mutants:
                results.append(verify_gap(spec, E.Image.load(image_path)))
            continue
        names = [spec.name + s for s in spec.mutants] if mutants else [spec.name]
        for name in names:''')
sub(P, '''    if not r.diffs:
        return False, "no eax or byte difference on any case (verdict %s)" % r.verdict''', '''    if not r.diffs:
        return False, "no eax or byte difference, and no call difference, on any case (verdict %s)" % r.verdict''')
sub(P, '''def table_row(r):
    note = "; reads outside the image: " + ", ".join("0x%X+%d" % o for o in r.outside) if r.outside else ""
    return "| %s | 0x%05X | %d | %d/%d | %s%s |" % (r.name, r.entry, r.ncases, r.hit, r.total, r.verdict, note)


TABLE_HEAD = ("| function | original | cases | blocks hit/total | verdict |\\n"
              "|---|---|---|---|---|")''', '''def table_row(r, verdicts=None):
    """One row; `verdicts` (entry -> verdict of the real rows) marks each callee with its own check."""
    note = "; reads outside the image: " + ", ".join("0x%X+%d" % o for o in r.outside) if r.outside else ""
    if r.gap:
        note += " (%s)" % r.gap
    callees = ", ".join("%05X %s %s" % (a, how, (verdicts or {}).get(a, "unverified"))
                        for a, how in r.callees) or "-"
    return "| %s | 0x%05X | %d | %d/%d | %s%s | %s |" % (r.name, r.entry, r.ncases, r.hit, r.total, r.verdict,
                                                       note, callees)


TABLE_HEAD = ("| function | original | cases | blocks hit/total | verdict | callees (each by its own check) |\\n"
              "|---|---|---|---|---|---|")''')
sub(P, '''    bad = [r for r in real if r.verdict != "VERIFIED"]
    missed = [(r, why) for r in mutants for ok, why in [mutant_detection(r)] if not ok]

    rows = [TABLE_HEAD] + [table_row(r) for r in real + mutants]''', '''    gaps = [r for r in real if r.gap]
    funcs = [r for r in real if not r.gap]
    bad = [r for r in funcs if r.verdict != "VERIFIED"] + [r for r in gaps if r.verdict != "NAMED_GAP"]
    missed = [(r, why) for r in mutants for ok, why in [mutant_detection(r)] if not ok]
    verdicts = {r.entry: r.verdict for r in real}
    closed = [r for r in funcs if r.verdict == "VERIFIED"
              and all(verdicts.get(a) == "VERIFIED" for a, _ in r.callees)]

    rows = [TABLE_HEAD] + [table_row(r, verdicts) for r in real + mutants]''')
sub(P, '''    print("diff-verify: %d/%d functions VERIFIED%s. Claim: equivalence on the exercised blocks and "
          "inputs only." % (len(real) - len(bad), len(real),
                            "; %d/%d mutants detected" % (len(mutants) - len(missed), len(mutants))
                            if args.self_check else ""))''', '''    print("diff-verify: %d/%d functions VERIFIED%s; %d named gaps; %d/%d with every callee VERIFIED. Claim: "
          "equivalence on the exercised blocks and inputs only, each function with its callees stubbed or "
          "run as stated." % (len([r for r in funcs if r.verdict == "VERIFIED"]), len(funcs),
                              "; %d/%d mutants detected" % (len(mutants) - len(missed), len(mutants))
                              if args.self_check else "", len(gaps), len(closed), len(funcs)))''')
PY
```

- [ ] **Step 4: Run the suites and the E1 self-check**

```bash
python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify 2>&1 | grep -E '^Ran|^OK'
python3 tools/diff_verify.py --image /tmp/pr_e3_diffimg --self-check | tail -1
```

Expected: `Ran 103 tests`, `OK`; then `diff-verify: 6/6 functions VERIFIED; 7/7 mutants detected; 0 named gaps; 6/6 with every callee VERIFIED. Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.` (E1's specs under the new driver: same verdicts, same blocks).

- [ ] **Step 5: Commit**

```bash
git add tools/diff_verify.py tools/tests/test_diff_verify.py
git commit -m "tools: diff_verify compares the call list; named gaps; callee column and counter line

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 6: Mutation proofs**

```bash
python3 /tmp/e3_mutate.py V1 V2 V3 V4 V5 V6 V7 V8; git status --short
```

Expected, exactly:
```
V1 -> test_a_call_with_another_argument_is_a_mismatch, test_a_missing_call_is_a_mismatch_and_counts_as_a_detection, test_an_extra_call_is_a_mismatch
V2 -> test_a_missing_call_is_a_mismatch_and_counts_as_a_detection
V3 -> test_cases_text_emits_the_call_set
V4 -> test_an_indirect_call_that_resolved_does_not_keep_the_function_partial
V5 -> test_a_misnamed_gap_is_a_mismatch
V6 -> test_an_address_both_allowed_and_in_the_call_set_is_refused
V7 -> test_call_lines_are_parsed_in_order
V8 -> test_a_bounded_switch_counts_its_cases_as_blocks
```
V1 stops comparing calls, V2 stops counting a call difference as a detection, V3 sends no `stub` lines, V4 lets an indirect call keep a function `PARTIAL`, V5 accepts a gap that stops on another instruction, V6 accepts an address both allowed and in the call set, V7 drops parsed `c` lines, V8 scans without switches. (V9, the closure count, has no test until Task 7.)

---

### Task 6: `diffrun` speaks stubs: stack slots, `stub`/`swrite` lines, the seam hook, code-pointer registration, three more seams

**Files:**
- Modify: `port/src/game/actors.c` (`actor_spawn`'s first line), `port/src/game/fighter.c` (`hit_anim_start_a`'s first lines; `hit_anim_start_b` loses `static`), `port/src/game/fighter.h` (one prototype after `hit_anim_ctx`)
- Modify: `port/tests/diff_runner.c` (includes; the register enum; bindings after `b_37dcc`; mutants before `k_bindings`; 15 table entries; `register_code`, `stub_t`, `swrite_t`, the case struct, `seam_hook`; `run_case`; `run_cases`; `main`)
- Modify: `tools/tests/test_diff_verify.py` (a `DiffrunStubTests` class before `if __name__ == "__main__":`)

**Interfaces:**
- Produces: seams on `0x2AE14` (`actor_spawn`: arguments `desc` as a `mem[]` offset, `a2`, `a3`, `a4`, `a5`), `0x3C480`, `0x3C4CC` (`rec, stream, frame_bits`); `void hit_anim_start_b(u32 rec, u32 stream, u32 frame_bits);` in `fighter.h`; `diffrun` bindings `fighter_23130`, `fighter_45878`, `anim_10fa8`, `anim_3e4e4`, `fighter_ctx_same`, `hit_anim_ctx`, `hit_anim_start_b` and the mutants `fighter_23130@voice`, `@novoice`, `fighter_45878@mutant`, `anim_10fa8@mutant`, `anim_3e4e4@mutant`, `fighter_ctx_same@mutant`, `hit_anim_ctx@mutant`, `hit_anim_start_b@mutant` (record §E3.6); `error a stub write outside the image`.
- Consumes: Task 2's `pr_seam`; `actors_init()` (registers the port's code pointers once the pools validate).

- [ ] **Step 1: Write the failing tests**

```bash
python3 - <<'PY'
import pathlib
def sub(path, old, new):
    p = pathlib.Path(path); s = p.read_text()
    assert s.count(old) == 1, "%s: expected exactly one match for %r" % (path, old[:60])
    p.write_text(s.replace(old, new))

sub("tools/tests/test_diff_verify.py", '''

if __name__ == "__main__":''', '''

@unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
                     "build/diffrun or PRAGE.EXE absent")
class DiffrunStubTests(unittest.TestCase):
    """The port side of the call stubs (record E3 §E3.3): diffrun's stub lines and the PR_SEAM hook."""

    def run_text(self, text):
        with tempfile.TemporaryDirectory() as d:
            return V.run_port(DIFFRUN, EXE, os.path.join(d, "img"), text)

    def test_stubbed_callees_are_recorded_with_their_c_arguments_in_order(self):
        # 0x23130: slot 0x10A200 (+0x52..+0x54 seeded 7F), rec 0x10A300, side 1
        out = self.run_text("case a\\nfn fighter_23130\\nreg eax 0x10A200\\nreg edx 0x10A300\\nreg ebx 0x1\\n"
                            "poke 0x10A252 7f7f7f\\nstub 0x3C4CC stub 0x0\\nstub 0x2C3FC stub 0x1\\nend\\n")["a"]
        self.assertEqual(out.calls, [(0x3C4CC, (0x10A300, 0xE4872, 0x40400000)), (0x2C3FC, (0x7C,))])
        self.assertEqual((out.eax, out.error), (1, ""))
        self.assertEqual([out.writes.get(a) for a in (0x10A252, 0x10A253, 0x10A254)], [9, 7, 0])

    def test_a_real_callee_runs_and_its_own_calls_are_recorded(self):
        # slot 0 is the fighter slot and the record's +0x51 names side 0, so 0x3C4CC reads the +0x52
        # that 0x23130 has just set to 9 (seeded 0): its 0x3C480 arm, with the slot's record 0x10A400
        out = self.run_text("case r\\nfn fighter_23130\\nreg eax 0x1077B0\\nreg edx 0x10A300\\nreg ebx 0x0\\n"
                            "poke 0x1077B0 00a41000\\npoke 0x107802 00\\npoke 0x10A351 00\\n"
                            "stub 0x3C4CC real 0x0\\nstub 0x2BC30 stub 0x0\\nstub 0x3C480 stub 0x0\\n"
                            "stub 0x2C3FC stub 0x1\\nend\\n")["r"]
        self.assertEqual(out.calls, [(0x3C4CC, (0x10A300, 0xE4872, 0x40400000)),
                                     (0x3C480, (0x10A400, 0xE4872, 0x40400000)), (0x2C3FC, (0x7C,))])

    def test_stack_slots_reach_the_binding(self):
        # 0x3C4CC alone: side 1 (+0x51), slot 1's record 0x10A400 and state 5: the 0x2BC30 arm
        out = self.run_text("case s\\nfn hit_anim_start_b\\nreg eax 0x10A300\\nreg edx 0xE4872\\nreg s0 0x40400000\\n"
                            "poke 0x10A351 01\\npoke 0x107844 00a41000\\npoke 0x107896 05\\n"
                            "stub 0x2BC30 stub 0x0\\nstub 0x3C480 stub 0x0\\nend\\n")["s"]
        self.assertEqual(out.calls, [(0x2BC30, (0x10A400, 0xE4872, 0x40400000))])

    def test_stub_writes_land_at_an_address_or_an_argument_plus_offset(self):
        out = self.run_text("case w\\nfn fighter_45878\\nreg eax 0x10A200\\nreg edx 0x10A300\\n"
                            "stub 0x2BC30 stub 0x0\\nswrite abs 0x10A500 5a\\nswrite arg0 0x52 33\\nend\\n")["w"]
        self.assertEqual((out.writes.get(0x10A500), out.writes.get(0x10A352)), (0x5A, 0x33))

    def test_a_stub_write_outside_the_image_is_an_error(self):
        out = self.run_text("case x\\nfn fighter_45878\\nreg eax 0x10A200\\nreg edx 0x10A300\\n"
                            "stub 0x2BC30 stub 0x0\\nswrite abs 0x2000000 01\\nend\\n")["x"]
        self.assertEqual(out.error, "a stub write outside the image")

    def test_the_animation_targets_are_registered(self):
        out = self.run_text("case t\\nfn anim_10fa8\\nstub 0x2AE14 stub 0x0\\nend\\n")["t"]
        self.assertEqual(out.calls, [(0x2AE14, (0x9AD08, 0, 0xE4, 0, 0))])

    def test_a_malformed_stub_line_is_refused(self):
        for line in ("stub 0x2BC30 skip 0x0", "stub 0x2BC30 stub", "swrite abs 0x10A500 5a"):
            with self.subTest(line=line), self.assertRaises(RuntimeError):
                self.run_text("case m\\nfn fighter_45878\\n%s\\nend\\n" % line)


if __name__ == "__main__":''')
PY
```

- [ ] **Step 2: Run them to see them fail**

```bash
python3 -m unittest tools.tests.test_diff_verify.DiffrunStubTests 2>&1 | grep -E 'Error:|^Ran|^FAILED' | sort | uniq -c
```

Expected:
```
   1 FAILED (errors=6)
   1 Ran 7 tests in <t>s
   1 RuntimeError: diffrun failed (1): diffrun: bad reg line
   5 RuntimeError: diffrun failed (1): diffrun: cannot parse 'stub'
```
(`test_a_malformed_stub_line_is_refused` already passes: today's `diffrun` refuses every `stub` line.)

- [ ] **Step 3: Implement**

```bash
python3 - <<'PY'
import pathlib
def sub(path, old, new):
    p = pathlib.Path(path); s = p.read_text()
    assert s.count(old) == 1, "%s: expected exactly one match for %r" % (path, old[:60])
    p.write_text(s.replace(old, new))

# -- the three remaining callee seams (record E3 §E3.6); 0x3C4CC becomes reachable from diffrun --
sub("port/src/game/actors.c", r'''u32 actor_spawn(const u32 *desc, u32 a2, u32 a3, u32 a4, u32 a5)
{
''', r'''u32 actor_spawn(const u32 *desc, u32 a2, u32 a3, u32 a4, u32 a5)
{
    PR_SEAM_RET(0x2AE14u, (u32)((const u8 *)desc - mem), a2, a3, a4, a5);
''')
sub("port/src/game/fighter.c", r'''static void hit_anim_start_a(u32 rec, u32 stream, u32 frame_bits)
{
    u32 ctx[6];
''', r'''static void hit_anim_start_a(u32 rec, u32 stream, u32 frame_bits)
{
    u32 ctx[6];
    PR_SEAM(0x3C480u, rec, stream, frame_bits);
''')
sub("port/src/game/fighter.c", r'''static void hit_anim_start_b(u32 rec, u32 stream, u32 frame_bits)
{
    u32 ctx[6];
''', r'''void hit_anim_start_b(u32 rec, u32 stream, u32 frame_bits)
{
    u32 ctx[6];
    PR_SEAM(0x3C4CCu, rec, stream, frame_bits);
''')
sub("port/src/game/fighter.h", r'''void hit_anim_ctx(u32 out[6], u32 rec);               /* 0x339AC */
''', r'''void hit_anim_ctx(u32 out[6], u32 rec);               /* 0x339AC */
void hit_anim_start_b(u32 rec, u32 stream, u32 frame_bits); /* 0x3C4CC */
''')

# -- diffrun: stack slots, stub lines, the seam hook, the code-pointer registration ----------------
D = "port/tests/diff_runner.c"
sub(D, r'''#include "game/config.h"
#include "game/fighter.h"
#include "game/rng.h"''', r'''#include "game/actors.h"
#include "game/config.h"
#include "game/fighter.h"
#include "game/flow.h"
#include "game/rng.h"''')
sub(D, r'''enum { R_EAX, R_EBX, R_ECX, R_EDX, R_ESI, R_EDI, R_EBP, R_N };
static const char *const k_reg[R_N] = { "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp" };''',
r'''/* s0..s3 are the dwords above the return address: a callee's stack arguments (record E3 §E3.5). */
enum { R_EAX, R_EBX, R_ECX, R_EDX, R_ESI, R_EDI, R_EBP, R_S0, R_S1, R_S2, R_S3, R_N };
static const char *const k_reg[R_N] = { "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp",
                                        "s0", "s1", "s2", "s3" };''')
sub(D, r'''static void b_37dcc(const u32 *r, u32 *eax)            { (void)r; fighter_37dcc(); *eax = 0u; }
''', r'''static void b_37dcc(const u32 *r, u32 *eax)            { (void)r; fighter_37dcc(); *eax = 0u; }
/* E3 (record 2026-10-01-reverse-e3 §E3.6). The reaction callbacks take EAX = slot, EDX = rec,
 * EBX = side (0x34E2C's call at 0x35045) and return AL = 1; the animation-opcode targets are
 * reached through fn_resolve as (rec, arg) = (EAX, EDX), as anim_indirect calls them. The two
 * context builders write six dwords at EAX (a stack buffer in every original caller); the binding
 * hands the C function a local array and copies it to mem[EAX], the original's own store. */
typedef void (*anim_code_fn)(u32 rec, u32 arg);
static void b_23130(const u32 *r, u32 *eax)            { *eax = (u32)fighter_23130(r[R_EAX], r[R_EDX], r[R_EBX]); }
static void b_45878(const u32 *r, u32 *eax)            { fighter_45878(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_anim(u32 addr, const u32 *r, u32 *eax)
{
    anim_code_fn fn = (anim_code_fn)(void *)fn_resolve(addr);
    if (fn) fn(r[R_EAX], r[R_EDX]);
    *eax = 0u;
}
static void b_10fa8(const u32 *r, u32 *eax)            { b_anim(0x10FA8u, r, eax); }
static void b_3e4e4(const u32 *r, u32 *eax)            { b_anim(0x3E4E4u, r, eax); }
static void b_ctx_same(const u32 *r, u32 *eax)
{
    u32 out[6];
    fighter_ctx_same(out, r[R_EDX]);
    memcpy(mem + r[R_EAX], out, sizeof out);
    *eax = 0u;
}
static void b_anim_ctx(const u32 *r, u32 *eax)
{
    u32 out[6];
    hit_anim_ctx(out, r[R_EDX]);
    memcpy(mem + r[R_EAX], out, sizeof out);
    *eax = 0u;
}
static void b_3c4cc(const u32 *r, u32 *eax)            { hit_anim_start_b(r[R_EAX], r[R_EDX], r[R_S0]); *eax = 0u; }
''')
sub(D, r'''static const binding_t k_bindings[] = {''', r'''static void m_23130_voice(const u32 *r, u32 *eax)     /* the wrong voice id */
{
    u32 ctx[6];
    fighter_ctx_same(ctx, r[R_EBX]);
    DSB(r[R_EAX] + 0x52u) = 9u;
    DSB(r[R_EAX] + 0x53u) = 7u;
    DSB(r[R_EAX] + 0x54u) = 0u;
    DSD(r[R_EAX] + 0x0Cu) = 0u;
    hit_anim_start_b(r[R_EDX], 0x000E4872u, 0x40400000u);
    (void)sound_voice(0x7Du);
    *eax = 1u;
}
static void m_23130_novoice(const u32 *r, u32 *eax)   /* forgets the voice call */
{
    u32 ctx[6];
    fighter_ctx_same(ctx, r[R_EBX]);
    DSB(r[R_EAX] + 0x52u) = 9u;
    DSB(r[R_EAX] + 0x53u) = 7u;
    DSB(r[R_EAX] + 0x54u) = 0u;
    DSD(r[R_EAX] + 0x0Cu) = 0u;
    hit_anim_start_b(r[R_EDX], 0x000E4872u, 0x40400000u);
    *eax = 1u;
}
static void m_45878(const u32 *r, u32 *eax)            /* the 0x2BC30 frame at 2.0, not 3.0 */
{
    actors_anim_begin(r[R_EDX], 0x000EB58Cu, 0x40000000u);
    DSB(r[R_EDX] + 0x53u) = 1u;
    DSB(r[R_EDX] + 0x59u) = 1u;
    DSB(r[R_EAX] + 0x52u) = 9u;
    DSB(r[R_EAX] + 0x53u) = 7u;
    DSB(r[R_EAX] + 0x54u) = 1u;
    DSB(r[R_EAX] + 0x57u) = 0u;
    DSD(r[R_EAX] + 0x0Cu) = 0x0004579Cu;
    DSD(r[R_EAX] + 0x18u) = 0x00045640u;
    DSD(r[R_EAX] + 0x1Cu) = 0x000456A8u;
    DSW(r[R_EAX] + 0x88u) = 0u;
    DSB(r[R_EAX] + 0x42u) |= 4u;
    *eax = 0u;
}
static void m_10fa8(const u32 *r, u32 *eax)            /* layer 0xE5 */
{
    (void)r;
    actor_spawn((const u32 *)(mem + 0x9AD08u), 0, 0xE5, 0, 0);
    *eax = 0u;
}
static void m_3e4e4(const u32 *r, u32 *eax)            /* skips the +0x14 test */
{
    u32 slot = DSD(r[R_EAX] + 0x14u);
    actors_anim_begin(r[R_EAX], 0x000E7BFAu, 0x40400000u);
    DSW(r[R_EAX] + 0x36u) = 0x320u;
    DSW(r[R_EAX] + 0x44u) = 0x23u;
    if (slot) DSB(slot + 0x57u) = 0u;
    *eax = 0u;
}
static void m_ctx_same(const u32 *r, u32 *eax)         /* out[1] = side */
{
    u32 out[6];
    fighter_ctx_same(out, r[R_EDX]);
    out[1] = r[R_EDX];
    memcpy(mem + r[R_EAX], out, sizeof out);
    *eax = 0u;
}
static void m_anim_ctx(const u32 *r, u32 *eax)         /* the side from +0x50 */
{
    u32 out[6];
    u32 s = DSB(r[R_EDX] + 0x50u);
    out[0] = s;
    out[1] = 1u - s;
    out[2] = DS_001077B0 + s * 0x94u;
    out[3] = DS_001077B0 + (1u - s) * 0x94u;
    out[4] = DSD(out[2]);
    out[5] = DSD(out[3]);
    memcpy(mem + r[R_EAX], out, sizeof out);
    *eax = 0u;
}
static void m_3c4cc(const u32 *r, u32 *eax)            /* forgets the 0x3C480 arm */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], r[R_EDX], r[R_S0]);
    *eax = 0u;
}

static const binding_t k_bindings[] = {''')
sub(D, r'''    { "fighter_37dcc@mutant",     m_37dcc,        0x00000000u },
};''', r'''    { "fighter_37dcc@mutant",     m_37dcc,        0x00000000u },
    { "fighter_23130",            b_23130,        0x000000FFu },
    { "fighter_23130@voice",      m_23130_voice,  0x000000FFu },
    { "fighter_23130@novoice",    m_23130_novoice, 0x000000FFu },
    { "fighter_45878",            b_45878,        0x00000000u },
    { "anim_10fa8",               b_10fa8,        0x00000000u },
    { "anim_3e4e4",               b_3e4e4,        0x00000000u },
    { "fighter_ctx_same",         b_ctx_same,     0x00000000u },
    { "hit_anim_ctx",             b_anim_ctx,     0x00000000u },
    { "hit_anim_start_b",         b_3c4cc,        0x00000000u },
    { "hit_anim_start_b@mutant",  m_3c4cc,        0x00000000u },
    { "fighter_45878@mutant",     m_45878,        0x00000000u },
    { "anim_10fa8@mutant",        m_10fa8,        0x00000000u },
    { "anim_3e4e4@mutant",        m_3e4e4,        0x00000000u },
    { "fighter_ctx_same@mutant",  m_ctx_same,     0x00000000u },
    { "hit_anim_ctx@mutant",      m_anim_ctx,     0x00000000u },
};''')
sub(D, r'''typedef struct { u32 addr; u32 len; u8 b[64]; } poke_t;
typedef struct {
    char id[64], fn[64];
    u32 reg[R_N];
    poke_t poke[16];
    int npoke;
} case_t;
''', r'''/* actors_init registers the port's code pointers (fn_register) once it has validated the two
 * pool pointers, which only game_init sets. Point them at a scratch range above the image for
 * the call, then restore them: mem[] is the loader's image again (checked). */
static int register_code(void)
{
    u32 pool = DSD(DS_001014F4), pset = DSD(DS_001014EC);
    DSD(DS_001014F4) = 0x2000000u;
    DSD(DS_001014EC) = 0x2100000u;
    int ok = actors_init();
    DSD(DS_001014F4) = pool;
    DSD(DS_001014EC) = pset;
    return ok && memcmp(mem + CODE_BASE, g_pristine, g_len) == 0;
}

typedef struct { u32 addr; u32 len; u8 b[64]; } poke_t;
/* A callee in the case's call set (record E3 §E3.4): `stub` returns at once with `eax` after its
 * writes, `real` runs the C body; both are recorded. A write lands at `off` (base -1) or at
 * argument `base` plus `off`. */
typedef struct { int base; u32 off; poke_t p; } swrite_t;
typedef struct {
    u32 addr, eax;
    int real;
    swrite_t w[4];
    int nw;
} stub_t;
typedef struct {
    char id[64], fn[64];
    u32 reg[R_N];
    poke_t poke[16];
    int npoke;
    stub_t stub[16];
    int nstub;
} case_t;

static const case_t *g_case;          /* the case running, for the seam hook */
static int g_seam_error;              /* a stub write the hook refused */

static int seam_hook(u32 addr, u32 nargs, const u32 *args, u32 *eax)
{
    const stub_t *s = NULL;
    for (int i = 0; g_case && i < g_case->nstub; i++)
        if (g_case->stub[i].addr == addr) s = &g_case->stub[i];
    if (!s) return 0;
    printf("c 0x%X", addr);
    for (u32 i = 0; i < nargs; i++) printf(" 0x%X", args[i]);
    printf("\n");
    if (s->real) return 0;
    for (int i = 0; i < s->nw; i++) {
        const swrite_t *w = &s->w[i];
        u32 at = w->off;
        if (w->base >= 0) {
            if ((u32)w->base >= nargs) { g_seam_error = 1; continue; }
            at = args[w->base] + w->off;
        }
        if (!(at >= CODE_BASE && w->p.len <= g_len && at - CODE_BASE <= g_len - w->p.len)) {
            g_seam_error = 1;
            continue;
        }
        memcpy(mem + at, w->p.b, w->p.len);
    }
    *eax = s->eax;
    return 1;
}
''')
sub(D, r'''    u32 eax = 0;
    b->call(c->reg, &eax);
    printf("ret eax 0x%X mask 0x%X\n", eax & b->eax_mask, b->eax_mask);''', r'''    u32 eax = 0;
    g_case = c;
    g_seam_error = 0;
    b->call(c->reg, &eax);
    g_case = NULL;
    if (g_seam_error) printf("error a stub write outside the image\n");
    else printf("ret eax 0x%X mask 0x%X\n", eax & b->eax_mask, b->eax_mask);''')
sub(D, r'''        char *t[4] = { 0 };
        int n = 0;
        for (char *s = strtok(line, " \t\r\n"); s && n < 4; s = strtok(NULL, " \t\r\n")) t[n++] = s;''',
r'''        char *t[6] = { 0 };
        int n = 0;
        for (char *s = strtok(line, " \t\r\n"); s && n < 6; s = strtok(NULL, " \t\r\n")) t[n++] = s;''')
sub(D, r'''        } else if (strcmp(t[0], "end") == 0) {''', r'''        } else if (strcmp(t[0], "stub") == 0 && n == 4 && c.nstub < 16) {
            /* stub <addr> stub|real <eax> */
            stub_t *s = &c.stub[c.nstub++];
            s->real = strcmp(t[2], "real") == 0;
            if (!parse_hex(t[1], &s->addr) || !parse_hex(t[3], &s->eax)
                    || (!s->real && strcmp(t[2], "stub") != 0)) {
                fprintf(stderr, "diffrun: bad stub line\n"); fclose(f); return 0;
            }
        } else if (strcmp(t[0], "swrite") == 0 && n == 4 && c.nstub > 0
                   && c.stub[c.nstub - 1].nw < 4) {
            /* swrite <abs|argN> <off> <bytes>, for the last stub line */
            stub_t *s = &c.stub[c.nstub - 1];
            swrite_t *w = &s->w[s->nw++];
            u32 k = 0;
            if (strcmp(t[1], "abs") == 0) w->base = -1;
            else if (strncmp(t[1], "arg", 3) == 0 && parse_hex(t[1] + 3, &k) && k < 8) w->base = (int)k;
            else { fprintf(stderr, "diffrun: bad swrite base\n"); fclose(f); return 0; }
            if (!parse_hex(t[2], &w->off) || !parse_bytes(t[3], &w->p)) {
                fprintf(stderr, "diffrun: bad swrite line\n"); fclose(f); return 0;
            }
        } else if (strcmp(t[0], "end") == 0) {''')
sub(D, r'''    if (!load_image(exe, img)) { fprintf(stderr, "diffrun: cannot load %s\n", exe); return 1; }''',
r'''    if (!load_image(exe, img)) { fprintf(stderr, "diffrun: cannot load %s\n", exe); return 1; }
    if (!register_code()) { fprintf(stderr, "diffrun: cannot register the port's code pointers\n"); return 1; }
    pr_seam = seam_hook;''')
PY
```

- [ ] **Step 4: Build and run everything that can see it**

```bash
cmake --build build 2>&1 | grep -E 'warning|error'
python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify 2>&1 | grep -E '^Ran|^OK'
./build/run_tests 2>&1 | tail -1
```

Expected: no warning or error line; `Ran 110 tests`, `OK`; `all checks passed`.

- [ ] **Step 5: Commit**

```bash
git add port/src/game/actors.c port/src/game/fighter.c port/src/game/fighter.h port/tests/diff_runner.c tools/tests/test_diff_verify.py
git commit -m "diffrun: stub lines, the seam hook, code-pointer registration; seams on 0x2AE14 0x3C480 0x3C4CC

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 6: Mutation proofs** (`diffrun` is rebuilt for each)

```bash
python3 /tmp/e3_mutate.py R1 R2 R3 R4; git status --short
```

Expected, exactly:
```
R1 -> test_a_real_callee_runs_and_its_own_calls_are_recorded
R2 -> test_stub_writes_land_at_an_address_or_an_argument_plus_offset
R3 -> test_the_animation_targets_are_registered
R4 -> test_a_real_callee_runs_and_its_own_calls_are_recorded, test_stack_slots_reach_the_binding, test_stubbed_callees_are_recorded_with_their_c_arguments_in_order, test_the_animation_targets_are_registered
```
R1 stubs `real` callees too, R2 ignores an argument-relative write's base, R3 skips the registration, R4 drops a call's first argument.

- [ ] **Step 7: The gate** (`port/src` changed): Task 2 Step 8's five commands with `/tmp/pr_e3_verify6.log` in place of `/tmp/pr_e3_verify2.log`. Expected: the same results (`exit 0`, `LINES-EQUAL`, `WAV-EQUAL`, the two counter lines).

---

### Task 7: The worked batch: `E3_SPECS`, the registry assertions, the real-image tests

**Files:**
- Modify: `tools/diff_verify.py` (`E3_SPECS` and its constants before `SPECS`; `SPECS` ends `] + E3_SPECS`)
- Modify: `tools/tests/test_diff_verify.py` (three assertions in `RealFunctionTests`; six tests at its end)

**Interfaces:**
- Produces: `V.E3_SLOT, V.E3_REC, V.E3_REC2, V.E3_OUT` (`0x10A200`.. `0x10A500`, zero BSS of the image, as E1's `U6_REC`), `V.DS_SLOTS` (`0x1077B0`), the calls `V.VOICE`, `V.ANIM_BEGIN`, `V.HIT_B`, `V.HIT_A`, `V.SPAWN` (record §E3.5), `V.E3_SPECS` (seven functions and the gap `host_1b890`, record §E3.6).
- Consumes: Tasks 5-6.

- [ ] **Step 1: Write the failing tests** (the three registry assertions now list the E3 entries; six new tests)

```bash
python3 - <<'PY'
import pathlib
def sub(path, old, new):
    p = pathlib.Path(path); s = p.read_text()
    assert s.count(old) == 1, "%s: expected exactly one match for %r" % (path, old[:60])
    p.write_text(s.replace(old, new))

T = "tools/tests/test_diff_verify.py"
# the three registry assertions list E3's specs, mutants and masks beside E1's (each stays exact)
sub(T, """        self.assertEqual(sorted(self.real), ["config_codeword_len", "config_credit_spend",
                                             "fighter_3640c", "fighter_37dcc", "fighter_slot_flag", "rng_next"])
        for name, r in self.real.items():""", """        self.assertEqual(sorted(self.real), ["anim_10fa8", "anim_3e4e4", "config_codeword_len",
                                             "config_credit_spend", "fighter_23130", "fighter_3640c",
                                             "fighter_37dcc", "fighter_45878", "fighter_ctx_same",
                                             "fighter_slot_flag", "hit_anim_ctx", "hit_anim_start_b",
                                             "host_1b890", "rng_next"])
        for name, r in self.real.items():
            if name == "host_1b890":       # the named gap (record E3 §E3.8), tested on its own below
                continue""")
sub(T, """        self.assertEqual(sorted(self.mut), [
            "config_codeword_len@mutant", "config_credit_spend@mutant", "config_credit_spend@signed",
            "fighter_3640c@mutant", "fighter_37dcc@mutant", "fighter_slot_flag@mutant", "rng_next@mutant"])""",
"""        self.assertEqual(sorted(self.mut), [
            "anim_10fa8@mutant", "anim_3e4e4@mutant",
            "config_codeword_len@mutant", "config_credit_spend@mutant", "config_credit_spend@signed",
            "fighter_23130@novoice", "fighter_23130@voice", "fighter_3640c@mutant", "fighter_37dcc@mutant",
            "fighter_45878@mutant", "fighter_ctx_same@mutant", "fighter_slot_flag@mutant",
            "hit_anim_ctx@mutant", "hit_anim_start_b@mutant", "rng_next@mutant"])""")
sub(T, """            "fighter_3640c": 0, "fighter_37dcc": 0})""", """            "fighter_3640c": 0, "fighter_37dcc": 0,
            "fighter_23130": 0xFF, "fighter_45878": 0, "anim_10fa8": 0, "anim_3e4e4": 0,
            "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF})""")
# the worked batch's own tests, at the end of RealFunctionTests
sub(T, """        self.assertEqual((both["ok"].eax, both["ok"].writes), (alone["ok"].eax, alone["ok"].writes))
""", """        self.assertEqual((both["ok"].eax, both["ok"].writes), (alone["ok"].eax, alone["ok"].writes))

    # ---- E3's worked batch (record 2026-10-01-reverse-e3 §E3.6) ----

    def test_each_e3_mutant_is_caught_by_what_it_breaks(self):
        kinds = {"fighter_23130@voice": {"call #1"}, "fighter_23130@novoice": {"call #1"},
                 "fighter_45878@mutant": {"call #0"}, "anim_10fa8@mutant": {"call #0"},
                 "hit_anim_start_b@mutant": {"call #0"}, "anim_3e4e4@mutant": {"call #0", "byte"},
                 "fighter_ctx_same@mutant": {"byte"}, "hit_anim_ctx@mutant": {"byte"}}
        for name, want in kinds.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)
        # the 0x3C480 arm only: states 3, 7 and 0x10 (cases h3, h7, h10) take it
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["hit_anim_start_b@mutant"].problems}),
                         ["h10", "h3", "h7"])

    def test_the_named_gap_is_0x1b890s_in(self):
        r = self.real["host_1b890"]
        self.assertEqual((r.verdict, r.gap, r.problems), ("NAMED_GAP", "in at 0x1B899", []))

    def spec(self, name, **kw):
        return dataclasses.replace([s for s in V.SPECS if s.name == name][0], **kw)

    def test_a_call_set_callee_without_a_port_seam_is_a_mismatch(self):
        # 0x33950 (fighter_ctx_same) has no PR_SEAM: recorded on the original side only
        spec = self.spec("fighter_23130", allow_calls=(),
                         calls=(E.Call(0x33950, (), mode="real"), V.HIT_B, V.VOICE))
        r = V.verify_all(DIFFRUN, EXE, os.path.join(self.tmp.name, "n.bin"), [spec])[0]
        self.assertEqual(r.verdict, "MISMATCH")
        self.assertTrue(r.problems[0].startswith("v0: call #0: original 0x33950(), port 0x3C4CC("), r.problems)

    def test_a_real_callee_runs_on_both_sides_and_its_own_calls_are_recorded(self):
        # 0x3C4CC run, not stubbed, inside 0x23130: its 0x2BC30 or 0x3C480 call joins the list
        spec = self.spec("fighter_23130", allow_calls=(0x33950, 0x339AC),
                         calls=(E.Call(0x3C4CC, ("eax", "edx", "s0"), mode="real"), V.ANIM_BEGIN, V.HIT_A, V.VOICE),
                         cases=[V.Case("r0", {"eax": V.DS_SLOTS, "edx": V.E3_REC, "ebx": 0},
                                       {V.E3_REC + 0x51: b"\\x00", V.DS_SLOTS: le32(V.E3_REC2),
                                        V.DS_SLOTS + 0x52: b"\\x00"})])
        img = os.path.join(self.tmp.name, "r.bin")
        r = V.verify_all(DIFFRUN, EXE, img, [spec])[0]
        self.assertEqual((r.verdict, r.problems), ("VERIFIED", []))
        o = E.run_original(E.Image.load(img), spec.entry, spec.cases[0].regs, spec.cases[0].pokes,
                           spec.allow_calls, calls=spec.calls)
        # EAX is fighter slot 0 and the record's +0x51 names side 0, so 0x3C4CC reads the +0x52 that
        # 0x23130 has just set to 9 (seeded 0, an arm-0x2BC30 state): the 0x3C480 arm (fighter.c's
        # record §43-C note on 0x23130)
        self.assertEqual([a for a, _ in o.calls], [0x3C4CC, 0x3C480, 0x2C3FC])

    def test_a_stub_write_relative_to_an_argument_lands_on_both_sides(self):
        anim = E.Call(0x2BC30, ("eax", "edx", "s0"), pop=4, writes=((0, 0x52, b"\\x33"),))
        spec = self.spec("fighter_45878", calls=(anim,))
        img = os.path.join(self.tmp.name, "w.bin")
        port = V.run_port(DIFFRUN, EXE, img, V.cases_text(spec, spec.name))
        self.assertEqual(port["b0"].writes.get(V.E3_REC + 0x52), 0x33)
        r = V.verify_spec(spec, E.Image.load(img), port)
        self.assertEqual((r.verdict, r.problems), ("VERIFIED", []))

    def test_the_self_check_counts_functions_mutants_gaps_and_closed_rows(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = V.main(["--diffrun", DIFFRUN, "--exe", EXE, "--image", os.path.join(self.tmp.name, "a.bin"),
                         "--self-check"])
        self.assertEqual(rc, 0)
        self.assertIn("diff-verify: 13/13 functions VERIFIED; 15/15 mutants detected; 1 named gaps; "
                      "8/13 with every callee VERIFIED.", out.getvalue())
""")
PY
```

- [ ] **Step 2: Run them to see them fail**

```bash
python3 -m unittest tools.tests.test_diff_verify.RealFunctionTests 2>&1 | grep -E 'Error|^Ran|^FAILED|^FAIL:' | sed 's/ (tools.*//' | cut -c1-110 | sort | uniq -c
```

Expected:
```
   1 AssertionError: 'diff-verify: 13/13 functions VERIFIED; 15/15 mutants detected; 1 named gaps; 8/13 with every 
   1 AssertionError: Lists differ: ['config_codeword_len', 'config_credit_spen[65 chars]ext'] != ['anim_10fa8', 'an
   1 AssertionError: Lists differ: ['config_codeword_len@mutant', 'config_cred[137 chars]ant'] != ['anim_10fa8@muta
   1 AssertionError: {'rng[149 chars]c': 0} != {'rng[149 chars]c': 0, 'fighter_23130': 255, 'fighter_45878': [122 c
   1 AttributeError: module 'diff_verify' has no attribute 'ANIM_BEGIN'
   1 AttributeError: module 'diff_verify' has no attribute 'HIT_B'
   1 FAIL: test_every_mutant_is_reported_as_a_mismatch
   1 FAIL: test_every_ported_function_agrees_with_the_original_on_every_block
   1 FAIL: test_the_eax_mask_is_stated_by_the_spec_per_function
   1 FAIL: test_the_self_check_counts_functions_mutants_gaps_and_closed_rows
   1 FAILED (failures=4, errors=5)
   1 IndexError: list index out of range
   1 KeyError: 'fighter_23130@voice'
   1 KeyError: 'host_1b890'
   1 Ran 17 tests in <t>s
```

- [ ] **Step 3: Implement**

```bash
python3 - <<'PY'
import pathlib
def sub(path, old, new):
    p = pathlib.Path(path); s = p.read_text()
    assert s.count(old) == 1, "%s: expected exactly one match for %r" % (path, old[:60])
    p.write_text(s.replace(old, new))

P = "tools/diff_verify.py"
sub(P, r'''SPECS = [
    Spec("rng_next", 0x5D7DC, [''', r'''# ---- E3: the call stubs' worked batch (record 2026-10-01-reverse-e3 §E3.6) ----------------------
E3_SLOT, E3_REC, E3_REC2, E3_OUT = 0x10A200, 0x10A300, 0x10A400, 0x10A500   # zero BSS of the image
DS_SLOTS = 0x1077B0            # DS_001077B0: the two 0x94-byte fighter slots; +0 holds the slot's record
VOICE = E.Call(0x2C3FC, ("eax",), eax=1)
ANIM_BEGIN = E.Call(0x2BC30, ("eax", "edx", "s0"), pop=4)
HIT_B = E.Call(0x3C4CC, ("eax", "edx", "s0"), pop=4)
HIT_A = E.Call(0x3C480, ("eax", "edx", "s0"), pop=4)
SPAWN = E.Call(0x2AE14, ("eax", "edx", "ecx", "ebx", "s0"), pop=4)
SLOT_PTRS = {DS_SLOTS: le32(E3_REC), DS_SLOTS + 0x94: le32(E3_REC2)}
OUT_SEED = {E3_OUT: b"\xaa" * 24}

E3_SPECS = [
    Spec("fighter_23130", 0x23130, [
        Case("v0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0},
             {E3_SLOT + 0x52: b"\x7f\x7f\x7f", E3_SLOT + 0x0C: le32(0xFFFFFFFF)}),
        Case("v1", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 1},
             {E3_SLOT + 0x52: b"\x7f\x7f\x7f", E3_SLOT + 0x0C: le32(0xFFFFFFFF)}),
    ], allow_calls=(0x33950,), calls=(HIT_B, VOICE), eax_mask=0xFF,
       mutants=("@voice", "@novoice")),
    Spec("fighter_45878", 0x45878, [
        Case("b0", {"eax": E3_SLOT, "edx": E3_REC},
             {E3_REC + 0x53: b"\x00", E3_REC + 0x59: b"\x00", E3_SLOT + 0x52: b"\x7f\x7f\x7f",
              E3_SLOT + 0x57: b"\x7f", E3_SLOT + 0x0C: le32(0xFFFFFFFF), E3_SLOT + 0x18: le32(0xFFFFFFFF),
              E3_SLOT + 0x1C: le32(0xFFFFFFFF), E3_SLOT + 0x88: b"\xff\xff", E3_SLOT + 0x42: b"\x00"}),
        Case("b1", {"eax": E3_SLOT, "edx": E3_REC},
             {E3_SLOT + 0x42: b"\xfb", E3_SLOT + 0x88: b"\x01\x00"}),
    ], calls=(ANIM_BEGIN,), eax_mask=0),
    Spec("anim_10fa8", 0x10FA8, [
        Case("s0", {}),
        Case("s1", {"eax": E3_REC, "edx": 0x55}),
    ], calls=(SPAWN,), eax_mask=0),
    Spec("anim_3e4e4", 0x3E4E4, [
        Case("e0", {"eax": E3_REC}, {E3_REC + 0x14: le32(0), E3_REC + 0x36: b"\xff\xff", E3_REC + 0x44: b"\xff\xff"}),
        Case("e1", {"eax": E3_REC}, {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x36: b"\xff\xff",
                                     E3_REC + 0x44: b"\xff\xff", E3_SLOT + 0x57: b"\x7f"}),
    ], calls=(ANIM_BEGIN,), eax_mask=0),
    Spec("fighter_ctx_same", 0x33950, [
        Case("x0", {"eax": E3_OUT, "edx": 0}, {**OUT_SEED, **SLOT_PTRS}),
        Case("x1", {"eax": E3_OUT, "edx": 1}, {**OUT_SEED, **SLOT_PTRS}),
    ], eax_mask=0),
    Spec("hit_anim_ctx", 0x339AC, [
        Case("y0", {"eax": E3_OUT, "edx": E3_REC}, {**OUT_SEED, **SLOT_PTRS, E3_REC + 0x50: b"\x01\x00"}),
        Case("y1", {"eax": E3_OUT, "edx": E3_REC}, {**OUT_SEED, **SLOT_PTRS, E3_REC + 0x50: b"\x00\x01"}),
    ], eax_mask=0),
    Spec("hit_anim_start_b", 0x3C4CC, [
        Case("h%X" % st, {"eax": E3_REC, "edx": 0xE4872, "s0": 0x40400000},
             {E3_REC + 0x51: bytes([side]), DS_SLOTS + side * 0x94: le32(E3_REC2),
              DS_SLOTS + side * 0x94 + 0x52: bytes([st])})
        for st, side in ((1, 0), (3, 1), (5, 0), (7, 1), (0xE, 0), (0x10, 1), (0x15, 0))
    ], allow_calls=(0x339AC,), calls=(ANIM_BEGIN, HIT_A), eax_mask=0),
    Spec("host_1b890", 0x1B890, [Case("g0", {})], mutants=(), gap="in at 0x1B899"),
]

SPECS = [
    Spec("rng_next", 0x5D7DC, [''')
sub(P, r'''        Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
    ], eax_mask=0),
]''', r'''        Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
    ], eax_mask=0),
] + E3_SPECS''')
PY
```

- [ ] **Step 4: Run the suites and the self-check**

```bash
python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify 2>&1 | grep -E '^Ran|^OK'
python3 tools/diff_verify.py --image /tmp/pr_e3_diffimg --table /tmp/pr_e3_diff.md --self-check > /tmp/pr_e3_self.txt; echo "exit $?"
head -16 /tmp/pr_e3_self.txt; tail -1 /tmp/pr_e3_self.txt
```

Expected: `Ran 116 tests`, `OK`, `exit 0`, then the 16 lines of record §E3.6's table (the header, the separator and the fourteen real rows, E1's six first with `| - |` as their last cell), then `diff-verify: 13/13 functions VERIFIED; 15/15 mutants detected; 1 named gaps; 8/13 with every callee VERIFIED. Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.` If any real row is not `VERIFIED` (or the gap row not `NAMED_GAP`), stop: the raw wins; record what the bytes did and report, do not edit the cases to fit.

- [ ] **Step 5: Commit**

```bash
git add tools/diff_verify.py tools/tests/test_diff_verify.py
git commit -m "tools: E3 worked batch (0x23130 0x45878 0x10FA8 0x3E4E4 and three callees VERIFIED; 0x1B890 named gap)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 6: Mutation proofs**

```bash
python3 /tmp/e3_mutate.py V9 P1; git status --short
```

Expected, exactly:
```
V9 -> test_the_self_check_counts_functions_mutants_gaps_and_closed_rows
P1 -> test_a_call_set_callee_without_a_port_seam_is_a_mismatch, test_a_call_with_another_argument_is_a_mismatch, test_a_missing_call_is_a_mismatch_and_counts_as_a_detection, test_a_real_call_runs_the_callee_and_is_recorded, test_a_real_callee_runs_on_both_sides_and_its_own_calls_are_recorded, test_a_stub_pops_its_stack_arguments_and_records_them, test_a_stub_write_relative_to_an_argument_lands_on_both_sides, test_a_stubbed_call_is_recorded_and_its_bytes_do_not_run, test_a_tail_jump_to_a_stubbed_entry_returns_to_the_caller, test_agreeing_calls_verify, test_an_extra_call_is_a_mismatch, test_an_indirect_call_that_resolved_does_not_keep_the_function_partial, test_an_indirect_call_through_a_register_is_resolved_at_run_time, test_an_indirect_call_through_a_table_is_resolved_at_run_time, test_each_e3_mutant_is_caught_by_what_it_breaks, test_every_ported_function_agrees_with_the_original_on_every_block, test_the_self_check_counts_functions_mutants_gaps_and_closed_rows
```
V9 counts every VERIFIED row as closed; P1 (Task 3's) now also fails the real-image tests.

---

### Task 8: `make diff-verify`, AGENTS, PROGRESS, and the gate

**Files:**
- Modify: `Makefile` (the comment above `diff-verify:` and its `@echo`), `AGENTS.md` (a porting rule; the diff-harness tooling line), `docs/PROGRESS.md` (one paragraph at the end)

**Interfaces:**
- Consumes: Tasks 2-7. Produces: the documented rule for P (record §E3.10).

- [ ] **Step 1: Edit the three files**

```bash
python3 - <<'PY'
import pathlib
def sub(path, old, new):
    p = pathlib.Path(path); s = p.read_text()
    assert s.count(old) == 1, "%s: expected exactly one match for %r" % (path, old[:60])
    p.write_text(s.replace(old, new))

sub("Makefile", '''# Differential verification (spec 2026-09-30-reverse-completion-design §5): the original's own
# bytes run in an emulator against the port's C functions from the same image; compares every
# changed byte, the return register and the block coverage. Skips cleanly without unicorn or
# capstone (spec §5.5); tools/diff_verify.py skips without PRAGE.EXE. The claim is narrow: equivalence on
# the exercised blocks and inputs only.''', '''# Differential verification (spec 2026-09-30-reverse-completion-design §5): the original's own
# bytes run in an emulator against the port's C functions from the same image; compares every
# changed byte, the return register, the ordered list of calls into the call set (record E3: callees
# stubbed or run on both sides, the port through its PR_SEAM lines) and the block coverage. Skips
# cleanly without unicorn or capstone (spec §5.5); tools/diff_verify.py skips without PRAGE.EXE. The
# claim is narrow: equivalence on the exercised blocks and inputs only.''')
sub("Makefile", '''	@echo "== differential verification (original bytes vs the port's C; record E1) =="''',
'''	@echo "== differential verification (original bytes vs the port's C; records E1, E3) =="''')
sub("AGENTS.md", '''- Code addresses stored in data go through `fn_origin()` / `fn_resolve()`.
''', '''- Code addresses stored in data go through `fn_origin()` / `fn_resolve()`.
- A ported function that a differentially verified function calls opens with
  `PR_SEAM(0xADDR, args...)` (void) or `PR_SEAM_RET(0xADDR, args...)` (`mem.h`): the harness's
  call seam, inert outside `build/diffrun`. The arguments are the C signature's, in order; the
  `E.Call` in `tools/diff_verify.py` names the original's registers and stack slots in the same
  order (record `2026-10-01-reverse-e3-derivations.md` §E3.3-§E3.5).
''')
sub("AGENTS.md", '''pins `unicorn` 2.1.4 and `capstone` 5.0.7). Without either the step prints its skip line and `make verify` stays green.''',
'''pins `unicorn` 2.1.4 and `capstone` 5.0.7). Without either the step prints its skip line and `make verify` stays green. Its last line counts the functions VERIFIED, the mutants detected, the named gaps (`NAMED_GAP` rows: the original stops on the instruction the spec names) and the rows whose every callee is VERIFIED by its own check; a stubbed callee that is not is "unverified" in the table's callee column.''')
p = pathlib.Path("docs/PROGRESS.md")
s = p.read_text()
p.write_text(s.rstrip("\n") + "\n\n" + """**Reverse completion E3: call stubs for the differential harness (plan `docs/superpowers/plans/2026-10-01-reverse-e3-call-stubs.md`, record `2026-10-01-reverse-e3-derivations.md`).** The harness now verifies a function with its callees stubbed or run identically on both sides and compares the ordered list of calls with their arguments (spec §5.1(3), closing E1 §E.6.1, §E.6.3 and §E.6.8). The original side intercepts every arrival at a call-set address by a `call` or a `jmp` (direct, indirect through a register or memory, or a tail jump), records the declared registers and stack slots, and for a stub applies its declared writes, sets EAX and returns popping the callee's own `ret N`; indirect call targets are resolved at run time, which lifts E2's `0x2B56D`/`0x2B0E9` blockers. The port side is a one-line `PR_SEAM`/`PR_SEAM_RET` at the top of each seamed callee (`mem.h`), inert unless `build/diffrun` installs its hook; `fn_resolve` reports an unregistered target to the same hook. Seams: `0x2C3FC`, `0x2BC30`, `0x2AE14`, `0x3C4CC`, `0x3C480` (`hit_anim_start_b` loses `static`). Bounded switches (`cmp` on the index register's family, then `ja`) are followed by the static scan, so `0x2C3FC` scans 100 blocks with no unknown jump. Worked batch, all VERIFIED with every block hit: the E2 `stubs` rows `0x23130` (voice and `0x3C4CC` stubbed), `0x45878` (`0x2BC30`), `0x10FA8` (`0x2AE14`), `0x3E4E4` (`0x2BC30`), and three callees by their own checks: `0x33950`, `0x339AC` (out-parameter bindings) and `0x3C4CC` (10/10 blocks); `0x1B890` is the first `NAMED_GAP` row (`in at 0x1B899`). `make diff-verify` reads `13/13 functions VERIFIED; 15/15 mutants detected; 1 named gaps; 8/13 with every callee VERIFIED`; E1's six rows and seven mutants are unchanged. The stubbed `0x2C3FC`, `0x2BC30`, `0x2AE14`, `0x3C480` are "unverified" until P verifies them. Named limits (record §E3.8): an indirect call to an unregistered target is compared by address only; EAX of a void port is not compared; a switch whose index is moved (`0x1BD7C`) or pre-scaled into a base (`0x18384`) stays an unknown jump. `make verify` exit 0, the 45 oracle lines and the WAV unchanged; counters `771 1203 64` / `731 731 100` (E3 ports nothing).
""")
PY
git diff --stat
```

Expected: `AGENTS.md | 7 ++++++-`, `Makefile | 9 +++++----`, `docs/PROGRESS.md | 2 ++`, `3 files changed, 13 insertions(+), 5 deletions(-)`.

- [ ] **Step 2: `make diff-verify`, and its skip**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_e3_diffimg DIFF_TABLE=/tmp/pr_e3_diff.md 2>&1 | grep -E '^==|^Ran|^OK|^diff-verify:'
make diff-verify PYTHON="python3 -S" 2>&1 | tail -1
```

Expected: `== differential verification (original bytes vs the port's C; records E1, E3) ==`, `Ran 116 tests in <t>s`, `OK`, then the counter line of Task 7 Step 4; and for the second command `diff-verify: skipped: unicorn or capstone is not installed (pip install -r tools/requirements-diff.txt)`.

- [ ] **Step 3: Only the harness sets the hook**

```bash
grep -rn 'pr_seam = ' port/src port/tests | sed 's/:.*pr_seam/: pr_seam/' | sort
```

Expected:
```
port/tests/diff_runner.c: pr_seam = seam_hook;
port/tests/test_platform.c: pr_seam = NULL;
port/tests/test_platform.c: pr_seam = seam_probe;
```

- [ ] **Step 4: The gate**

```bash
make verify SMK_DUMP=/tmp/pr_e3_smk TITLE_DUMP=/tmp/pr_e3_title ATTRACT_DUMP=/tmp/pr_e3_att FRONTEND_DUMP=/tmp/pr_e3_fe TITLE_PIN_DIR=/tmp/pr_e3_pin AUDIO_WAV=/tmp/pr_e3.wav K11_DUMP=/tmp/pr_e3_k11 GP_DUMP=/tmp/pr_e3_gp DIFF_IMAGE=/tmp/pr_e3_diffimg DIFF_TABLE=/tmp/pr_e3_diff.md E2_IMAGE=/tmp/pr_e3_e2img > /tmp/pr_e3_verify8.log 2>&1; echo "exit $?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_e3_verify8.log | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo LINES-EQUAL
grep -E '^diff-verify:|^entry-triage: targets' /tmp/pr_e3_verify8.log
make audio-render AUDIO_WAV=/tmp/pr_e3.wav >/dev/null 2>&1; cmp /tmp/pr_e3.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-EQUAL
python3 tools/port_progress.py
```

Expected (the replay's run):
```
exit 0
LINES-EQUAL
diff-verify: 13/13 functions VERIFIED; 15/15 mutants detected; 1 named gaps; 8/13 with every callee VERIFIED. Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 329 unported, 166 ported; supplement 131 (35 unported, 0 stale); untrusted entries 30
WAV-EQUAL
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

- [ ] **Step 5: Commit**

```bash
git add Makefile AGENTS.md docs/PROGRESS.md
git commit -m "build: diff-verify notes the call list; AGENTS rule for PR_SEAM; PROGRESS for E3

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

## Execution notes

- **Order:** 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8, strictly: each task's scripts replace text the previous tasks left (a script stops with "expected exactly one match" if a task was skipped or applied twice). Task 2 can run before or after Tasks 3-5 in principle, but keep the order: the quoted line numbers and counts assume it.
- **Model tiers:** Task 1: haiku. Tasks 2, 3, 4, 5: sonnet (paste the scripts exactly; the risk is a hand edit that drifts from the quoted outputs). Task 6: sonnet, with an opus reviewer (C in `port/src`, the first task where the two sides meet). Task 7: sonnet with an opus reviewer: it is the only task whose verdicts come from the real bytes; a row that is not `VERIFIED` is a finding to report, never a case to adjust. Task 8: sonnet; the reviewer re-runs Step 3 and Step 4.
- **Reviewer checks per task:** the red output matches the quoted failure; the green count matches (85, 89, 103, 110, 116 Python tests; `all checks passed` for `run_tests`); the mutation output matches the quoted lines exactly and `git status --short` is empty afterwards; no file outside the task's list changed (`git show --stat HEAD`).
- **The gate:** `make verify` after Tasks 2, 6 and 8 (the tasks that change `port/src` or `diffrun`); Tasks 3 and 4 change `static_scan`, which `entry_triage.py` imports, so they run `make entry-triage`; Tasks 5 and 7 change only `diff_verify.py` and its tests, whose gate is the test run plus the self-check line. Every gate: exit 0, the 45 oracle lines equal, the WAV identical, `771 1203 64` / `731 731 100`, `entry-triage` unchanged.
- **After E3:** track P ports a batch with the recipe of record §E3.10 (seam each direct callee, a spec with `calls`, a mutant only the call list catches, the E2 table regenerated in the same commit). The first P batch can reuse the five seamed callees, which cover the four most frequent callees of the 85 rows; the eight unported direct callees (record §E3.1) come first.
