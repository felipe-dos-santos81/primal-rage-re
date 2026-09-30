# Allocation-Failure Seam (named-gaps sub-project C) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Close G4. Both allocation-failure arms of `0x1D0BC`
(`sound_buffers_alloc`) get tests that can fail: the MIDI-buffer arm
(`0x1D10E..0x1D12F`, which zeroes `DS_001028C4`/`C0`/`CC`) and the sample-slot
arm (`0x1D16B`: the loop stops at the first failure; slot 0 empty turns the DIG
driver off at `0x1D195`), plus the slot-0-already-set entry case seen through the
seam. No oracle-visible behaviour changes.

**Architecture:** One `PORT:` test seam goes into `res.c`'s bump allocator
`res_alloc` (the port's `0x1C308`). It is a zero-initialised static countdown.
`res_fail_alloc_nth(n)` arms it so that the n-th request from then on returns 0
before the heap moves, which is exactly what the allocator's existing "does not
fit" arm does. The seam then disarms itself (it is one-shot).
`res_fail_alloc_left()` reads the countdown. Nothing in `port/src` arms it.
The seam follows the precedent of `res_set_screen_hook` (`res.c`) and
`sound_voice_log_*` (`flow.c`/`flow.h`): a `PORT:` C static used only by
tests, not original state, and inert by default.
- A self-test in `test_platform.c`'s `test_res` pins the seam's own contract.
- New vectors in `test_game.c`'s existing `check_sound_buffers` pin the two
  arms.

No new test function and no `TEST_CASES` change.

**Tech Stack:** C over flat `mem[]` (SDL3 port), CMake, the `CHECK`/`CHECK_EQ_INT`
suite. The raw comes from the fixup-applied LE image via capstone
(`$K/dx.py`), because the Ghidra MCP is unavailable (spec §5).

**Spec:** `docs/superpowers/specs/2026-09-30-named-gaps-design.md`, §4.C (gap
G4, decision §3.5). Raw source: `docs/superpowers/plans/2026-09-29-k7-k12-derivations.md`
§0.7.1 (lines ~388-406) and §2.1/§2.4 (the Not-tested list, line ~1168). Ledger:
`docs/superpowers/plans/2026-09-29-all-gaps-ledger.md` §H.3 row 4.

## Global Constraints

- **Branch prerequisites.** The ledger row this plan closes (§H.3 row 4,
  "`0x1D0BC`'s two allocation-failure arms are ported but untested") exists
  only from commit `0247a1a` (branch `all-gaps-final`). That commit is not an
  ancestor of the spec commit `a169296` (main / `named-gaps-spec`). Work in the
  worktree the controller names. It must contain both commits, and it must
  have `data` and `.superpowers` as symlinks to the main checkout's (as
  `.worktrees/all-gaps-final` does). Task 1 Step 1 checks this and halts if it
  does not hold.
- **Run everything from that worktree's root.** `data/` is read-only.
- **Variables:**
  - `K=.superpowers/sdd/2026-09-29-k7-k12/scratch` holds the baselines:
    `oracle-lines-base.txt`, `base.sha256`, `before-t2.wav`, `dumps.sh`,
    `dumpsha.sh`, `dx.py`.
  - `C=.superpowers/sdd/2026-09-30-named-gaps-c-alloc-seam/scratch` is this
    plan's scratch.
  - Both are git-ignored.
- **No oracle claim moves.** The gate is `make verify` exit 0, and the oracle
  lines must equal `$K/oracle-lines-base.txt`. `make verify` always runs with
  the parallel-safety overrides:
  `SMK_DUMP=/tmp/pr_cN_smk TITLE_DUMP=/tmp/pr_cN_title ATTRACT_DUMP=/tmp/pr_cN_attract FRONTEND_DUMP=/tmp/pr_cN_frontend TITLE_PIN_DIR=/tmp/pr_cN_pin AUDIO_WAV=/tmp/pr_cN_fm.wav`,
  where `N` is the task number. The frame dumps must match `$K/base.sha256`
  (`dumps.sh` + `dumpsha.sh`), and the `make audio-render` WAV must be
  byte-identical to `$K/before-t2.wav`. If anything moves, **halt** and report
  the old value, the new value and the diff.
- **Raw wins; never a fitted constant.** Every seed and expected value below
  traces to a listed raw address. On a plan-vs-raw conflict, record the
  correction with its address in the record and follow the raw.
- **Porting rules:**
  - Only `/* PORT: ... */` or plain comments in `port/src`; no
    `TODO(verify)` is needed here.
  - Do not reformat `res.c`, `res.h` or `flow.c`.
  - `symbols.h` is generated; do not touch it.
  - SDL and file I/O stay in `host.c`/`main.c`.
  - **No original state in a C global:** the seam's static is test state,
    marked `PORT:`, by the precedent above.
- **Tests:**
  - Use only `CHECK`/`CHECK_EQ_INT`.
  - Seed sentinels that differ from the post-conditions. Never assert an
    unseeded BSS zero.
  - Every new assertion must fail under a named mutation (the tables in Tasks
    1 and 2).
  - `game_init()` may run only once per process. `sound_buffers_alloc()` is
    exercised without it, from `test_flow` → `check_sound_buffers()`, on the
    shared unit-suite process whose resource heap `test_res`/`test_flow`
    already loaded.
  - A test is registered in `TEST_CASES` only if a new test function is
    added. This plan adds none.
- **Assertion-site count:** `rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l`.
  Expect +10 after Task 1 and +33 after Task 2.
- **Commits:**
  - Style: `<area>: <what changed>`.
  - Trailer: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
  - Stage named files only (never `git add -A`).
  - Commit only where a step says so and the controller has authorised
    commits. Otherwise stop at that step and ask.
- **0 compiler warnings** (`-Wall -Wextra`). No new dependency.
- **SDD ledger:** `.superpowers/sdd/2026-09-30-named-gaps-c-alloc-seam/progress.md`.
  Append one line per completed step group.

## Review Focus

1. **The seam changes behaviour when unarmed.** An unarmed request must run
   exactly today's `res_alloc` path. **Task 1** covers this:
   - The self-test's disarm checks (mutations S3/S4).
   - The `rg` proof that no `port/src` code arms it.
   - Dumps equal to `base.sha256` after the seam lands.

   **Task 3** re-proves it on the final tree: dumps, oracle lines and the WAV
   are all identical.
2. **An injected failure is not faithful to a real one.** A failed request that
   advances or aligns-and-stores the heap would differ from the real
   `mem_in_range` failure. **Task 1** catches this with the self-test's
   "heap unmoved" check (mutation S2). **Task 2** checks it again: V1 slot 0
   equals the pre-call peek, and V3's spacings (S2 fails 5 of them).
3. **The MIDI arm's zeroing and fall-through are unpinned.** `0x1D113`,
   `0x1D11E` and `0x1D124` store `ecx` (= 0) into C4, C0 and CC, and the arm
   falls into `0x1D132`. **Task 2** V1 covers this (mutations M1, M1a-c and
   M5).
4. **The slot loop does not stop at the first failure, or it counts the failed
   slot.** `0x1D16B je 0x1D17F` skips `inc ecx`, so a slot-0 failure leaves
   `ecx` = 0 and `0x1D195` zeroes C8, while a later failure keeps C8. **Task
   2** V2/V3 cover this (mutations M2, M3 and M4).
5. **A vacuous or leaked arm.** An armed vector whose failure never reached
   `0x1C308` would pass trivially. An arm left set would fail an unrelated
   later allocation. **Task 2** covers this:
   - Every armed vector asserts `res_fail_alloc_left()` after the call (0 when
     the failure fired, 1 in the entry vector, where no request may be made).
   - Mutations M6 and M7 make those checks fail.
   - The re-run of S3 shows a leaked arm failing `test_flow`'s sample path.

---

### Task 1: Baseline, raw re-read, the `res_alloc` failure seam and its self-test

**Files:**
- Modify: `port/src/platform/res.h`: declare the seam, after `res_block_alloc`.
- Modify: `port/src/platform/res.c`: the seam static, the two functions, and one line in `res_alloc`.
- Modify: `port/tests/test_platform.c`: add `check_res_fail_seam()` above `int test_res(void)` and call it at the end of `test_res`.
- Create: `docs/superpowers/plans/2026-09-30-named-gaps-c-derivations.md` (§C.0, §C.1).
- Create (git-ignored): `$C/`, `.superpowers/sdd/2026-09-30-named-gaps-c-alloc-seam/progress.md`.

**Interfaces:**
- Consumes: `res_alloc`/`res_block_alloc` (`res.c:97-107`); `$K` baselines.
- Produces: `void res_fail_alloc_nth(u32 n);` and `u32 res_fail_alloc_left(void);`
  (`platform/res.h`); `$C/verify-base.txt`, `$C/checks-base.txt`,
  `$C/progress-base.txt`, `$C/dx-1D0BC.txt`.

- [ ] **Step 1: Prerequisites.**

```bash
git merge-base --is-ancestor a169296 HEAD && git merge-base --is-ancestor 0247a1a HEAD && echo BRANCH-OK
rg -n "0x1D0BC.s two allocation-failure arms are ported but untested" docs/superpowers/plans/2026-09-29-all-gaps-ledger.md
test -L data && test -d .superpowers/sdd/2026-09-29-k7-k12/scratch && echo LINKS-OK
git status --short
```
Expected:
- `BRANCH-OK`.
- One ledger hit, at the `| 4 |` row of §H.3.
- `LINKS-OK`.
- A clean status.

If any of these fails, stop and report to the controller.

- [ ] **Step 2: Baseline gate (before any change).**

```bash
K=.superpowers/sdd/2026-09-29-k7-k12/scratch; C=.superpowers/sdd/2026-09-30-named-gaps-c-alloc-seam/scratch; mkdir -p $C
make build 2>&1 | grep -E 'warning|error'
make verify SMK_DUMP=/tmp/pr_c1_smk TITLE_DUMP=/tmp/pr_c1_title ATTRACT_DUMP=/tmp/pr_c1_attract FRONTEND_DUMP=/tmp/pr_c1_frontend TITLE_PIN_DIR=/tmp/pr_c1_pin AUDIO_WAV=/tmp/pr_c1_fm.wav 2>&1 | tee $C/verify-base.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $C/verify-base.txt | diff $K/oracle-lines-base.txt - && echo ORACLES-EQUAL
rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l | tee $C/checks-base.txt
python3 tools/port_progress.py | tee $C/progress-base.txt
sh $K/dumps.sh c-base && sh $K/dumpsha.sh c-base && echo DUMPS-IDENTICAL
make audio-render AUDIO_WAV=$C/before.wav >/dev/null 2>&1; cmp $K/before-t2.wav $C/before.wav && echo WAV-IDENTICAL
```
Expected:
- No warning or error lines.
- `EXIT=0`, `ORACLES-EQUAL`, `DUMPS-IDENTICAL` and `WAV-IDENTICAL`.
- `progress-base.txt` shows `767 1203 64` and a portable `731 731 100` (the
  §H.1 values).

If any baseline differs, halt: the tree is not the base this plan was written
against. Then `rm -rf $K/c-base` (the dump is large; the sha check is what
counts).

- [ ] **Step 3: Re-read the raw.**

```bash
python3 $K/dx.py 1D0BC 1D1B0 | tee $C/dx-1D0BC.txt
```
Confirm these lines (the evidence the tests encode):
- `1d0de: mov ecx, dword ptr [0x1028d0]` and `1d0e6: jne 0x1d132`. `ecx` = `D0`
  = 0 on the allocating path.
- `1d0e8: mov edx, 0x5100` / `1d0f2: call 0x1c308` / `1d0f7: mov dword ptr [0x1028d0], eax` / `1d0fe: je 0x1d10e`.
- `1d113`/`1d11e`/`1d124`: `mov dword ptr [0x1028c4]/[0x1028c0]/[0x1028cc], ecx`.
  Then `1d12a: call 0x62734` and `1d12f: add esp, 8`, which falls through to
  `1d132`.
- `1d132: cmp dword ptr [0x1028c8], 0`, `1d13b..1d147` (slot 0's `+0x10`, `ecx`
  = `ebx` = 0, `jne 0x1d17f`).
- `1d14d: mov edx, 0x8c00`, `1d154: mov edx, 0x6000`, `1d163: mov [ebx+0x102870], eax`,
  `1d16b: je 0x1d17f` (before `1d16d: inc ecx`), `1d171: cmp ecx, 4`,
  `1d176: cmp dword ptr [ebx+0x102870], 0`.
- `1d17f: test ecx, ecx`, `1d181: jne 0x1d19b`, `1d195: mov dword ptr [0x1028c8], ecx`,
  `1d19d: mov byte ptr [0xa2cb0], dl` (dl = 1).

If any line differs, the raw wins: record it in §C.0 and adjust the Task 2
expectations to match.

- [ ] **Step 4: Write the failing self-test.** In `port/tests/test_platform.c`,
  insert this block immediately above `int test_res(void)` (after `res_hook`'s
  closing brace):

```c
/* named-gaps C (record 2026-09-30-named-gaps-c-derivations.md §C.1): res.c's
 * PORT: allocation-failure seam. Armed with n, the n-th request from then on
 * returns 0 and leaves the heap where it was (the mem_in_range arm of
 * res_alloc), and the seam disarms itself; n = 0 disarms. The heap ends where
 * it started: every request that succeeds here is size 0 (res_block_alloc(0)
 * aligns the heap and does not advance it), and the one sized request fails. */
static void check_res_fail_seam(void)
{
    const u32 a = res_block_alloc(0u);
    CHECK(a != 0u, "the bump heap has room");

    res_fail_alloc_nth(3u);
    CHECK_EQ_INT((int)res_fail_alloc_left(), 3);
    CHECK_EQ_INT((int)res_block_alloc(0u), (int)a);       /* request 1 succeeds */
    CHECK_EQ_INT((int)res_fail_alloc_left(), 2);
    CHECK_EQ_INT((int)res_block_alloc(0u), (int)a);       /* request 2 succeeds */
    CHECK_EQ_INT((int)res_block_alloc(0x100u), 0);        /* request 3 fails */
    CHECK_EQ_INT((int)res_fail_alloc_left(), 0);          /* one-shot: disarmed */
    CHECK_EQ_INT((int)res_block_alloc(0u), (int)a);       /* the failure took nothing */

    res_fail_alloc_nth(1u);
    res_fail_alloc_nth(0u);                               /* n = 0 disarms */
    CHECK_EQ_INT((int)res_block_alloc(0u), (int)a);
    CHECK_EQ_INT((int)res_fail_alloc_left(), 0);          /* an unarmed request leaves it */
}
```

At the end of `test_res`, change

```c
        close(big_fd);
        unlink(big_path);
    }

    return g_failures - before;
}

/* ---- test_gra.c ---- */
```

to

```c
        close(big_fd);
        unlink(big_path);
    }

    check_res_fail_seam();

    return g_failures - before;
}

/* ---- test_gra.c ---- */
```

- [ ] **Step 5: Confirm the build fails.**

```bash
cmake --build build 2>&1 | grep -E 'error' | head
```
Expected: implicit-declaration/undeclared errors for `res_fail_alloc_nth` and
`res_fail_alloc_left`. Record the error count in §C.1.

- [ ] **Step 6: Declare the seam.** In `port/src/platform/res.h`, change

```c
/* The port's 0x1C308 (res.c's bump allocator): a mem[] offset, 0 if it does not fit. */
u32 res_block_alloc(u32 size);
```

to

```c
/* The port's 0x1C308 (res.c's bump allocator): a mem[] offset, 0 if it does not fit. */
u32 res_block_alloc(u32 size);

/* PORT: the allocation-failure test seam (record
 * 2026-09-30-named-gaps-c-derivations.md §C.1), not original state (the
 * pattern of res_set_screen_hook). res_fail_alloc_nth(n) makes the n-th
 * bump-allocator request from now on return 0 without moving the heap, as a
 * block that does not fit does, and then disarms; every request counts
 * (res_load_index's, res_load_file's and res_block_alloc's, size 0 included).
 * n = 0 disarms. res_fail_alloc_left() is the requests left until the failure
 * (0 = disarmed). Nothing in port/src arms it. */
void res_fail_alloc_nth(u32 n);
u32 res_fail_alloc_left(void);
```

- [ ] **Step 7: Implement the seam.** In `port/src/platform/res.c`, change

```c
/* PORT: replaces FUN_0001C308. Bump allocator; the original is a real block
 * allocator with free(). Substituted until a task needs to release memory. */
static u32 res_alloc(u32 size)
{
    u32 at = (g_heap + 3u) & ~3u;
```

to

```c
/* PORT: the allocation-failure test seam (res.h), not original state (the
 * pattern of res_set_screen_hook above). s_fail_left counts the requests left
 * until the one that fails; 0, its initial value, is disarmed, and no caller
 * in port/src arms it, so every run's heap offsets are unchanged. */
static u32 s_fail_left;

void res_fail_alloc_nth(u32 n) { s_fail_left = n; }
u32 res_fail_alloc_left(void) { return s_fail_left; }

/* PORT: replaces FUN_0001C308. Bump allocator; the original is a real block
 * allocator with free(). Substituted until a task needs to release memory. */
static u32 res_alloc(u32 size)
{
    /* PORT: the test seam above; no original instruction. The injected
     * failure returns before the heap moves, as the mem_in_range arm does. */
    if (s_fail_left != 0u && --s_fail_left == 0u) return 0;
    u32 at = (g_heap + 3u) & ~3u;
```

- [ ] **Step 8: Build and run.**

```bash
cmake --build build 2>&1 | grep -E 'warning|error'
PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -2
rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l
rg -n 'res_fail_alloc|s_fail_left' port/src
```
Expected:
- No warning or error output.
- `all checks passed`.
- The site count is `checks-base + 10`.
- `rg` lists exactly: the two declarations in `res.h`; in `res.c`, the
  `static u32 s_fail_left;` line, the two one-line definitions and the
  `res_alloc` test line. There is no call of `res_fail_alloc_nth` anywhere
  else in `port/src`. That is the unarmed proof: the only writer of
  `s_fail_left` is `res_fail_alloc_nth` (plus `res_alloc`'s decrement, which
  runs only when the value is non-zero).

- [ ] **Step 9: Mutation proofs of the seam.** Back up `cp port/src/platform/res.c $C/res.c.good`.
  Apply each mutation alone with the Edit tool, then run:

```bash
C=.superpowers/sdd/2026-09-30-named-gaps-c-alloc-seam/scratch; M=S1   # set M per row
cmake --build build 2>&1 | grep -E 'warning|error'; PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep '^FAIL ' | tee $C/mut-$M.txt | wc -l
cp $C/res.c.good port/src/platform/res.c
```

Here `a` is the heap offset the self-test peeks (about `0x2Cxxxxx`), printed in
decimal.

| id | mutation (in `res.c`) | expected FAIL lines (`test_platform.c`, `check_res_fail_seam`) |
|---|---|---|
| S0 | `u32 res_block_alloc(u32 size) { return res_alloc(size); }` → `u32 res_block_alloc(u32 size) { (void)res_alloc(size); return 0; }` | `the bump heap has room`, plus the existing `check_sound_buffers` slot/peek checks in `test_game.c` (record the count) |
| S1 | `if (s_fail_left != 0u && --s_fail_left == 0u) return 0;` → `if (s_fail_left != 0u) return 0;` (fails every request while armed, never counts down) | 5: request 1 `0 != a`; `3 != 2`; request 2 `0 != a`; `3 != 0`; "the failure took nothing" `0 != a` |
| S2 | move the seam line after `g_heap = at + size;` (the failure advances the heap) | 2: "the failure took nothing" `a+256 != a`; after-disarm `a+256 != a` |
| S3 | `void res_fail_alloc_nth(u32 n) { s_fail_left = n; }` → `void res_fail_alloc_nth(u32 n) { if (n != 0u) s_fail_left = n; }` | 1: after-disarm request `0 != a` |
| S4 | `if (s_fail_left != 0u && --s_fail_left == 0u) return 0;` → `if (--s_fail_left == 0u) return 0;` (the unarmed path mutates the count) | 1: "an unarmed request leaves it" `-1 != 0` |

After all five, `cmp $C/res.c.good port/src/platform/res.c` must be silent,
and a rebuild and run must print `all checks passed`. A mutation that fails
fewer lines than listed is a defect in the test: fix the test, not the
expectation, and record it.

- [ ] **Step 10: Gate.**

```bash
K=.superpowers/sdd/2026-09-29-k7-k12/scratch; C=.superpowers/sdd/2026-09-30-named-gaps-c-alloc-seam/scratch
make verify SMK_DUMP=/tmp/pr_c1_smk TITLE_DUMP=/tmp/pr_c1_title ATTRACT_DUMP=/tmp/pr_c1_attract FRONTEND_DUMP=/tmp/pr_c1_frontend TITLE_PIN_DIR=/tmp/pr_c1_pin AUDIO_WAV=/tmp/pr_c1_fm.wav 2>&1 | tee $C/verify-t1.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $C/verify-t1.txt | diff $K/oracle-lines-base.txt - && echo ORACLES-EQUAL
sh $K/dumps.sh c-t1 && sh $K/dumpsha.sh c-t1 && echo DUMPS-IDENTICAL; rm -rf $K/c-t1
```
Expected: `EXIT=0`, `ORACLES-EQUAL` and `DUMPS-IDENTICAL`. This is the
empirical half of Review Focus 1. The env-gated drivers (`--check`, fe det,
attract, title) run alone and never reach `test_res`, so the seam is never
armed in any oracle process. Identical dumps prove that the unarmed branch
leaves every heap offset unchanged.

- [ ] **Step 11: Write the record (§C.0, §C.1).** Create
  `docs/superpowers/plans/2026-09-30-named-gaps-c-derivations.md` with:

```markdown
# Named-gaps sub-project C: the allocation-failure seam (G4). Derivations

Plan: `docs/superpowers/plans/2026-09-30-named-gaps-c-alloc-seam.md`. Spec:
`docs/superpowers/specs/2026-09-30-named-gaps-design.md` §4.C. `$K` is
`.superpowers/sdd/2026-09-29-k7-k12/scratch/`, `$C` is
`.superpowers/sdd/2026-09-30-named-gaps-c-alloc-seam/scratch/`.

## §C.0 Baseline and the raw

- `make verify` (parallel overrides `/tmp/pr_c1_*`) exits 0 (`$C/verify-base.txt`);
  its oracle lines diff empty against `$K/oracle-lines-base.txt`
  (`ORACLES-EQUAL`); `dumps.sh c-base` matches `$K/base.sha256`; the
  `make audio-render` WAV equals `$K/before-t2.wav`.
- Assertion sites: <the number in $C/checks-base.txt>. `port_progress.py`:
  <the two lines of $C/progress-base.txt>.
- The Ghidra MCP is unavailable (spec §5). `0x1D0BC..0x1D1AE` was re-read from
  the fixup-applied image (`$K/dx.py 1D0BC 1D1B0` → `$C/dx-1D0BC.txt`) and
  matches k7-k12 §0.7.1/§2.1 instruction for instruction:
  - MIDI arm: `0x1D0DE` loads `ecx` = `DS_001028D0`, which the `0x1D0E6` gate
    requires to be 0; `0x1D0F2` calls `0x1C308(0x41, 0x5100)`; `0x1D0F7`
    stores EAX; `0x1D0FE je 0x1D10E`. The failure arm `0x1D113`/`0x1D11E`/
    `0x1D124` stores `ecx` (0) to C4, C0 and CC, prints through `0x62734`
    (format `0x8063C`), and falls through to `0x1D132`.
  - Slot arm: `0x1D16B je 0x1D17F` precedes `0x1D16D inc ecx`, so the failed
    slot is not counted; `0x1D17F test ecx,ecx` / `0x1D195 mov [0x1028C8],ecx`
    turn the DIG driver off only when `ecx` = 0 (slot 0 failed, or was set at
    entry, `0x1D147`).
- The raw sees only EAX = 0 from `0x1C308`; the port's `0x1C308` is `res.c`'s
  bump allocator (host-owned, k7-k12 §0.7.1), whose only failure is
  `!mem_in_range` (returns 0, heap unmoved).

## §C.1 The seam

- **Design.** `res.c` gains `static u32 s_fail_left` (`PORT:`, the
  `res_set_screen_hook` / `sound_voice_log` precedent: test state, not
  original state) and `res_fail_alloc_nth(n)` / `res_fail_alloc_left()`
  (`res.h`). `res_alloc`'s first line returns 0 when a non-zero count reaches
  0, before the heap is aligned or moved, the same observable result as its
  `mem_in_range` arm.
- **Why here, and why one-shot.** In `res_alloc` rather than a replacement
  hook on `res_block_alloc`, so the injected failure goes through the real
  allocator and its heap effect is testable (and a later task can reach
  `res_load_index`/`res_load_file`'s arms). One-shot rather than
  "fail from the n-th on": after the failure the heap serves again, so a slot
  loop that failed to stop at `0x1D16B` would visibly give the next slot a
  block. With a sticky failure it would store 0 over the 0 seeds the loop
  condition `0x1D176` requires, and the missing break would be invisible.
  Exhausting `mem[]` instead (k7-k12 §2.4) would move every later heap
  address in the suite.
- **Inert when unarmed.** `s_fail_left` starts at 0; `rg -n 'res_fail_alloc|s_fail_left' port/src`
  shows no call of `res_fail_alloc_nth` outside its definition, so in every
  production and oracle-driver process the `s_fail_left != 0u` test is false
  and `res_alloc` runs today's instructions. Empirically `make verify` exits 0
  with `ORACLES-EQUAL` and `dumps.sh c-t1` matches `$K/base.sha256`.
- **Test.** `test_platform.c` `check_res_fail_seam()`, called at the end of
  `test_res`: armed with 3, requests 1-2 succeed (size 0), request 3 (`0x100`)
  returns 0, the count is 0 and the heap has not moved; arming 1 then 0
  disarms, and an unarmed request neither fails nor touches the count.
  Before the seam the build fails (<N> errors: `res_fail_alloc_nth`,
  `res_fail_alloc_left` undeclared). Assertion sites +10.
- **Mutations** (each applied alone, rebuilt, run with `PR_ORACLE_REQUIRED=1`,
  restored; `$C/mut-S*.txt`): S0..S4 as in the plan's Task 1 Step 9 table,
  with the measured FAIL-line counts and lines.
```

When writing the file, replace each `<...>` with the measured literal: the
number, the two progress lines, and the error count from Step 5. Replace the
mutation sentence with a table of measured `file:line: value` lines.

- [ ] **Step 12: Commit.**

```bash
git add port/src/platform/res.c port/src/platform/res.h port/tests/test_platform.c docs/superpowers/plans/2026-09-30-named-gaps-c-derivations.md
git commit -m "$(cat <<'EOF'
platform: add the res_alloc failure-injection test seam (record named-gaps-c §C.1)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Test both allocation-failure arms of `0x1D0BC`

**Files:**
- Modify: `port/tests/test_game.c`: `check_sound_buffers()` (one save line,
  four new vectors before the ISR-pair block, one restore line).
- Modify: `docs/superpowers/plans/2026-09-30-named-gaps-c-derivations.md` (§C.2).

**Interfaces:**
- Consumes: `res_fail_alloc_nth`, `res_fail_alloc_left`, `res_block_alloc`
  (`platform/res.h`, already included by `test_game.c`), `sound_buffers_alloc`
  (`game/flow.h`), `DS_001028CC` (`symbols.h:710`).
- Produces: vectors V1 (MIDI failure), V2 (slot 0 failure), V3 (slot k = 1..3
  failure) and V4 (slot 0 set at entry, armed). `port/src` is unchanged.

`check_sound_buffers` runs inside `test_flow` on the unit-suite process after
`game_audio_init()`, without `game_init()`. The resource heap is the one
`test_res`/`test_flow` loaded. The new vectors take at most about `0x4F000`
bytes of heap: V1 takes `0x1AC00`, and V3 takes `0x8C00`, `0xEC00` and
`0x14C00`. That is well inside the ~12 MB headroom (k7-k12 §2.1).

- [ ] **Step 1: Save and restore CC.** In `check_sound_buffers`, change

```c
    const u8 s_b0 = DSB(DS_000A2CB0);
```

to

```c
    const u8 s_b0 = DSB(DS_000A2CB0);
    const u32 s_cc = DSD(DS_001028CC);
```

and change

```c
    DSB(DS_000A2CB0) = s_b0;
    for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = s[i];
}
```

to

```c
    DSB(DS_000A2CB0) = s_b0;
    DSD(DS_001028CC) = s_cc;
    for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = s[i];
}
```

- [ ] **Step 2: Add the four vectors.** Insert immediately above the line
  `    /* The ISR's counter pair (PORT, 0x1BE0E..0x1BE16). */`:

```c
    /* named-gaps C (record 2026-09-30-named-gaps-c-derivations.md §C.2): the
     * two allocation-failure arms, through res.c's PORT: failure seam. Every
     * armed vector asserts the count the seam has left after the call (0: the
     * injected failure reached 0x1C308), then disarms it. peek is taken while
     * the seam is disarmed. */

    /* V1, the MIDI arm's failure (0x1D0FE -> 0x1D10E..0x1D12F): request 1, the
     * 0x5100 block, fails, and C4, C0 and CC get ecx (DS_001028D0 = 0, loaded
     * at 0x1D0DE). The arm falls through to 0x1D132: slot 0 gets the block
     * the failed request did not take. */
    DSB(DS_000A2CB0) = 0;
    DSD(DS_001028C0) = 0x1234u;
    DSD(DS_001028C4) = 0x5678u;
    DSD(DS_001028CC) = 0xCC01u;
    DSD(DS_001028D0) = 0;
    DSD(DS_001028C8) = 1u;
    for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = 0;
    peek = res_block_alloc(0u);
    res_fail_alloc_nth(1u);
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK_EQ_INT((int)res_fail_alloc_left(), 0);
    res_fail_alloc_nth(0u);
    CHECK_EQ_INT((int)DSB(DS_000A2CB0), 1);
    CHECK_EQ_INT((int)DSD(DS_001028C4), 0);              /* 0x1D113 */
    CHECK_EQ_INT((int)DSD(DS_001028C0), 0);              /* 0x1D11E */
    CHECK_EQ_INT((int)DSD(DS_001028CC), 0);              /* 0x1D124 */
    CHECK_EQ_INT((int)DSD(DS_00102870), (int)peek);      /* 0x1D132 still runs */

    /* V2, the slot loop's failure at slot 0 (0x1D16B with ecx = 0): request 1,
     * the 0x8C00, fails; the loop stops before 0x1D16D, so slot 1 is not
     * taken, and ecx = 0 turns the DIG driver off (0x1D17F..0x1D195). Slots
     * 1..3 are seeded 0 because 0x1D176 reaches a slot only when it is 0; a
     * loop that went on would give slot 1 the next block. */
    DSB(DS_000A2CB0) = 0;
    DSD(DS_001028C0) = 0;
    DSD(DS_001028C4) = 0;
    DSD(DS_001028D0) = 0xD0D0D0D0u;
    DSD(DS_001028C8) = 1u;
    for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = 0;
    peek = res_block_alloc(0u);
    res_fail_alloc_nth(1u);
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK_EQ_INT((int)res_fail_alloc_left(), 0);
    res_fail_alloc_nth(0u);
    CHECK_EQ_INT((int)DSD(DS_001028C8), 0);              /* 0x1D195 */
    CHECK_EQ_INT((int)DSD(DS_00102870 + 0x18u), 0);      /* slot 1 not taken */
    CHECK_EQ_INT((int)(res_block_alloc(0u) - peek), 0);  /* no block taken */

    /* V3, the failure at slot k = 1..3 (request k + 1): slots 0..k-1 are
     * allocated, the loop stops at 0x1D16B so no later slot is taken, ecx = k
     * keeps the DIG driver (0x1D181), and the failed request took nothing: the
     * next block starts right after slot k-1 (0x8C00 for slot 0, 0x6000
     * after). */
    {
        u32 k, j;
        for (k = 1u; k < 4u; k++) {
            DSB(DS_000A2CB0) = 0;
            DSD(DS_001028C0) = 0;
            DSD(DS_001028C4) = 0;
            DSD(DS_001028D0) = 0xD0D0D0D0u;
            DSD(DS_001028C8) = 1u;
            for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = 0;
            peek = res_block_alloc(0u);
            res_fail_alloc_nth(k + 1u);
            CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
            CHECK_EQ_INT((int)res_fail_alloc_left(), 0);
            res_fail_alloc_nth(0u);
            CHECK_EQ_INT((int)DSD(DS_00102870), (int)peek);
            for (j = 1u; j < k; j++)
                CHECK(DSD(DS_00102870 + j * 0x18u) != 0u,
                      "a slot before the failed one is allocated");
            for (j = k + 1u; j < 4u; j++)
                CHECK_EQ_INT((int)DSD(DS_00102870 + j * 0x18u), 0);   /* 0x1D16B stops */
            CHECK_EQ_INT((int)DSD(DS_001028C8), 1);                  /* ecx = k */
            CHECK_EQ_INT((int)(res_block_alloc(0u) - DSD(DS_00102870 + (k - 1u) * 0x18u)),
                         k == 1u ? 0x8C00 : 0x6000);
        }
    }

    /* V4, slot 0's buffer set at entry with the seam armed: the 0x1D147 gate
     * makes no request at all (the count stays 1), slot 0 keeps its buffer,
     * and ecx = 0 still turns the DIG driver off (0x1D195). */
    DSB(DS_000A2CB0) = 0;
    DSD(DS_001028C0) = 0;
    DSD(DS_001028C4) = 0;
    DSD(DS_001028C8) = 1u;
    for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = 0;
    DSD(DS_00102870) = 0x0BADu;
    res_fail_alloc_nth(1u);
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK_EQ_INT((int)res_fail_alloc_left(), 1);
    res_fail_alloc_nth(0u);
    CHECK_EQ_INT((int)DSD(DS_00102870), 0x0BAD);
    CHECK_EQ_INT((int)DSD(DS_001028C8), 0);

```

Seeds against post-conditions:
- Every zeroed field is seeded non-zero: C4 `0x5678`, C0 `0x1234`, CC
  `0xCC01`, C8 `1`.
- `DS_000A2CB0` is seeded 0 and asserted 1.
- V4's slot 0 is seeded `0x0BAD`, and that seed would be overwritten if the
  gate ran.
- The zero seeds that are asserted zero (V2/V3 later slots) are explicit
  writes that the loop condition `0x1D176` requires. They fail under M2.

Two stores of 0 cannot be observed, because their destinations must already be
0 to reach them. Do not assert them. They go in Not tested:
- `0x1D0F7`'s store on the failure path: `D0` must be 0 for the `0x1D0E6` gate.
- `0x1D163`'s store for the failed slot: the slot must be 0 for `0x1D147` or
  `0x1D176`.

- [ ] **Step 3: Build and run.** These are characterisation tests of arms
  that are already ported, so they pass on the first run. Their ability to
  fail is proved in Step 4.

```bash
cmake --build build 2>&1 | grep -E 'warning|error'
PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -2
rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l
```
Expected:
- No warning or error output.
- `all checks passed`.
- The site count is `checks-base + 33`: 10 from Task 1, plus 7 (V1), 5 (V2),
  7 (V3) and 4 (V4).

- [ ] **Step 4: Mutation proofs of the arms.** `flow.c` is unchanged in this
  task, so restore with `git checkout -- port/src/game/flow.c`. Apply each
  mutation alone in `sound_buffers_alloc` (`port/src/game/flow.c`) with the
  Edit tool, then run:

```bash
C=.superpowers/sdd/2026-09-30-named-gaps-c-alloc-seam/scratch; M=M1   # set M per row
cmake --build build 2>&1 | grep -E 'warning|error'; PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep '^FAIL ' | tee $C/mut-$M.txt | wc -l
git checkout -- port/src/game/flow.c
```

The table uses these names. `peek` is the vector's pre-call heap offset, and
`addr` is a heap offset (both printed in decimal). The listed lines are all in
`test_game.c`.

| id | mutation | expected FAIL lines |
|---|---|---|
| M1 | **delete the MIDI arm's zeroing**: the three lines `DSD(DS_001028C4) = 0;`, `DSD(DS_001028C0) = 0;`, `DSD(DS_001028CC) = 0;` inside `else { ... }` | 3: V1 C4 `22136 != 0`, V1 C0 `4660 != 0`, V1 CC `52225 != 0` |
| M1a | delete only `DSD(DS_001028C4) = 0;` (`0x1D113`) | 1: V1 C4 `22136 != 0` |
| M1b | delete only `DSD(DS_001028C0) = 0;` (`0x1D11E`) | 1: V1 C0 `4660 != 0` |
| M1c | delete only `DSD(DS_001028CC) = 0;` (`0x1D124`) | 1: V1 CC `52225 != 0` |
| M2 | **change the loop stop**: delete `if (b == 0u) break;` (`0x1D16B`) | 8: V2 C8 `1 != 0`; V2 slot 1 `peek != 0`; V2 heap `73728 != 0`; V3 k=1 later-slot check twice (slots 2, 3: `addr != 0`); V3 k=1 spacing `84992 != 35840`; V3 k=2 later-slot check once (slot 3); V3 k=2 spacing `49152 != 24576`. k=3 is unaffected, because `i` = 4 ends the loop. |
| M3 | count the failed slot: `if (b == 0u) break;` → `if (b == 0u) { i++; break; }` | 1: V2 C8 `1 != 0` |
| M4 | any failure turns the DIG off: `if (b == 0u) break;` → `if (b == 0u) { DSD(DS_001028C8) = 0; break; }` | 3: V3 C8 `0 != 1` for k = 1, 2, 3 (same line) |
| M5 | the MIDI failure skips the sample arm: append `DSB(DS_000A2CB0) = 1u; return 1;` after `DSD(DS_001028CC) = 0;` in the `else` | 1: V1 slot 0 `0 != peek` |
| M6 | skip the slot-0 entry gate: `if (DSD(DS_00102870) == 0u) {` → `if (1) {` (`0x1D147`) | 4: V4 count `0 != 1`, V4 slot 0 `0 != 2989`, plus the existing entry-case vector's C8 `1 != 0` and slot 1 `addr != 0` (k7-k12 §2.3 mutation f) |
| M7 | the sample arm never runs: `if (DSD(DS_001028C8) != 0u) {` → `if (0) {` (`0x1D132`) | includes V2 count `1 != 0` and V3 count `2 != 0`, `3 != 0`, `4 != 0` (proves the `== 0` count checks can fail), V1 slot 0 `0 != peek`, V4 C8 `1 != 0`, plus existing first-run checks; record the measured total |
| S3′ | re-apply Task 1's S3 in `res.c` (`if (n != 0u) s_fail_left = n;`) and restore with `cp $C/res.c.good port/src/platform/res.c`, having refreshed the backup from the committed file first (`git show HEAD:port/src/platform/res.c > $C/res.c.good`) | Task 1's line (`test_platform.c` after-disarm `0 != a`), plus V4's leaked arm failing the next allocation: `test_flow`'s post-`check_idle_timeout_clock` `sound_buffers_alloc()` loses slot 0, so the `sound_voice(0x40u)` check and the mixer checks after it fail. Record them; this is Review Focus 5's leak detection. |

After the table, `git status --short port/src` must be empty and a rebuild and
run must print `all checks passed`. If a mutation fails fewer lines than
listed, or different ones, analyse why:
- If the test is weak, strengthen it. Consolidation rules do not apply to new
  assertions.
- If the expectation was wrong, record the measured lines with the reason.

- [ ] **Step 5: Gate.**

```bash
K=.superpowers/sdd/2026-09-29-k7-k12/scratch; C=.superpowers/sdd/2026-09-30-named-gaps-c-alloc-seam/scratch
make verify SMK_DUMP=/tmp/pr_c2_smk TITLE_DUMP=/tmp/pr_c2_title ATTRACT_DUMP=/tmp/pr_c2_attract FRONTEND_DUMP=/tmp/pr_c2_frontend TITLE_PIN_DIR=/tmp/pr_c2_pin AUDIO_WAV=/tmp/pr_c2_fm.wav 2>&1 | tee $C/verify-t2.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $C/verify-t2.txt | diff $K/oracle-lines-base.txt - && echo ORACLES-EQUAL
```
Expected: `EXIT=0` and `ORACLES-EQUAL`.

- [ ] **Step 6: Record §C.2.** Append to the record:

```markdown
## §C.2 The two arms

`test_game.c` `check_sound_buffers()` gains four vectors after the
slot-0-set-at-entry vector (it now also saves and restores `DS_001028CC`).
`sound_buffers_alloc()` runs on the unit-suite process from `test_flow`,
without `game_init()`.

| vector | seeds | armed | asserted |
|---|---|---|---|
| V1 MIDI failure (`0x1D10E..0x1D12F`) | `A2CB0` = 0, `C0` = `0x1234`, `C4` = `0x5678`, `CC` = `0xCC01`, `D0` = 0, `C8` = 1, slots = 0 | 1 | AL = 1; count 0; `A2CB0` = 1; C4 = C0 = CC = 0; slot 0 = the pre-call peek (the arm falls into `0x1D132`, the failed request took nothing) |
| V2 slot 0 fails (`0x1D16B`, ecx = 0) | `A2CB0` = 0, `C0` = `C4` = 0, `D0` = `0xD0D0D0D0`, `C8` = 1, slots = 0 | 1 | AL = 1; count 0; `C8` = 0 (`0x1D195`); slot 1 = 0; heap unmoved |
| V3 slot k = 1..3 fails | as V2 | k + 1 | AL = 1; count 0; slot 0 = peek; slots 1..k-1 ≠ 0; slots k+1..3 = 0; `C8` = 1; next block − slot k-1 = `0x8C00` (k = 1) / `0x6000` |
| V4 slot 0 set at entry (`0x1D147`) | `A2CB0` = 0, `C0` = `C4` = 0, `C8` = 1, slot 0 = `0x0BAD`, slots 1..3 = 0 | 1 | AL = 1; count 1 (no request); slot 0 = `0x0BAD`; `C8` = 0 |

Assertion sites: +23 (V1 7, V2 5, V3 7, V4 4); with §C.1's +10 the tree holds
<checks-base + 33>.

Mutations (each alone, rebuilt, `PR_ORACLE_REQUIRED=1`, restored;
`$C/mut-M*.txt`): <the plan's Task 2 Step 4 table with the measured FAIL
lines>. The two arms' named proofs: M1 (delete the MIDI arm's zeroing: 3 FAIL
lines, one per store) and M2 (delete the loop stop: 8 FAIL lines over V2 and
V3).

### §C.2.1 Not tested

- `0x1D0F7`'s store of 0 into `DS_001028D0` on the failure path and `0x1D163`'s
  store of 0 into the failed slot's `+0x10`: both destinations must already be
  0 to reach the store (`0x1D0E6`; `0x1D147`/`0x1D176`), so no state tells a
  store from none.
- The two `0x62734` messages (`0x8063C` MIDI, `0x80684` slots): `PORT:` not
  printed (k7-k12 §2.1), so there is nothing to observe.
- Both arms failing in one call (MIDI and slot 0): the seam is one-shot by
  design (§C.1); each arm is pinned alone.
- `0x1C308`'s own failure modes (a real paged allocator with free): host-owned;
  the port's allocator fails only by `mem_in_range`, which the seam mirrors.
```

Replace the two `<...>` with the measured number and the measured table.

- [ ] **Step 7: Commit.**

```bash
git add port/tests/test_game.c docs/superpowers/plans/2026-09-30-named-gaps-c-derivations.md
git commit -m "$(cat <<'EOF'
game: test 0x1D0BC's two allocation-failure arms through the res_alloc seam (record named-gaps-c §C.2)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Final gate, ledger, PROGRESS, closure

**Files:**
- Modify: `docs/superpowers/plans/2026-09-30-named-gaps-c-derivations.md` (§C.3).
- Modify: `docs/superpowers/plans/2026-09-29-all-gaps-ledger.md` (§H.3 row 4).
- Modify: `docs/PROGRESS.md` (append one paragraph).

**Interfaces:**
- Consumes: Tasks 1-2 commits and the `$K` baselines.
- Produces: the closure evidence. `README.md` is unchanged, because no
  function was added or removed.

- [ ] **Step 1: Clean final gate.**

```bash
K=.superpowers/sdd/2026-09-29-k7-k12/scratch; C=.superpowers/sdd/2026-09-30-named-gaps-c-alloc-seam/scratch
make clean && make build 2>&1 | grep -E 'warning|error'
make verify SMK_DUMP=/tmp/pr_c3_smk TITLE_DUMP=/tmp/pr_c3_title ATTRACT_DUMP=/tmp/pr_c3_attract FRONTEND_DUMP=/tmp/pr_c3_frontend TITLE_PIN_DIR=/tmp/pr_c3_pin AUDIO_WAV=/tmp/pr_c3_fm.wav 2>&1 | tee $C/verify-t3.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $C/verify-t3.txt | diff $K/oracle-lines-base.txt - && echo ORACLES-EQUAL
sh $K/dumps.sh c-after && sh $K/dumpsha.sh c-after && echo DUMPS-IDENTICAL; rm -rf $K/c-after
make audio-render AUDIO_WAV=$C/after.wav >/dev/null 2>&1; cmp $K/before-t2.wav $C/after.wav && echo WAV-IDENTICAL
python3 tools/port_progress.py | diff $C/progress-base.txt - && echo PROGRESS-UNCHANGED
rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l
rg -n 'res_fail_alloc_nth' port/src
```
Expected:
- No warning or error output.
- `EXIT=0`, `ORACLES-EQUAL`, `DUMPS-IDENTICAL`, `WAV-IDENTICAL` and
  `PROGRESS-UNCHANGED`.
- The site count is `checks-base + 33`.
- The `rg` shows only the `res.h` declaration and the `res.c` definition.

`make clean` keeps `.superpowers/` by design.

- [ ] **Step 2: Record §C.3.** Append:

```markdown
## §C.3 Gate and closure

- `make clean && make build && make verify` (overrides `/tmp/pr_c3_*`) exits 0
  (`$C/verify-t3.txt`); the oracle lines diff empty against
  `$K/oracle-lines-base.txt` (`ORACLES-EQUAL`); `dumps.sh c-after` matches
  `$K/base.sha256` (`DUMPS-IDENTICAL`, dump deleted); the `make audio-render`
  WAV is byte-identical to `$K/before-t2.wav`; `port_progress.py` is unchanged
  (no function added). 0 warnings.
- **G4 closed.** Both allocation-failure arms of `0x1D0BC` are tested with
  seeded sentinels and mutation proofs (§C.2: M1 for the MIDI arm, M2/M3/M4 for
  the slot break and the DIG-off test, M6 for the entry gate), through a
  `PORT:` seam that is inert unarmed (§C.1). Residue, not a gap in behaviour:
  the two unobservable stores of 0 and the unprinted messages (§C.2.1).
- Noticed, out of scope (other allocation-failure arms in `port/src`):
  - `res_load_index`'s `table == 0` (`res.c:163`) and per-entry `data == 0`
    (`res.c:187`) returns, and its four trailing `res_alloc` results stored
    unchecked (`res.c:224-227`, `0x1B2A0..0x1B3A9`): untested; `res_load_index`
    can run only once per process (a second load exhausts the heap), so a
    test needs its own env-gated driver. The seam can reach them.
  - `game_audio_init`'s `FAT.OPL` load (`flow.c`, `res_load_file(..., "FAT.OPL", ...)`):
    the no-bank arm is untested.
  - Already covered: `res_load_file`'s `off == 0` arm (`res.c:253`, the
    128 MiB sparse fixture in `test_res`) and `movie_play`'s missing-file arm
    (`movie.c:90`, `test_video.c`).
  - Host-only `malloc` failures (`mem.c` `slurp`, `ail.c` sample growth) and
    the NULL arms of `AIL_allocate_sample_handle`/`AIL_allocate_sequence_handle`
    (`flow.c` `game_audio_init`): host, no raw counterpart.
```

- [ ] **Step 3: Ledger.** In `docs/superpowers/plans/2026-09-29-all-gaps-ledger.md`
  §H.3, change the row

```markdown
| 4 | `0x1D0BC`'s two allocation-failure arms are ported but untested | The MIDI arm `0x1D10E..0x1D12F` (zeroes `DS_001028C4/C0/CC`) and the slot break `0x1D16B` (slot 0's failure included). The bump allocator cannot fail in-process without exhausting `mem[]` (record k7-k12 §0.7.1 and its Not-tested list). |
```

to

```markdown
| 4 | ~~`0x1D0BC`'s two allocation-failure arms are ported but untested~~ **closed** (named-gaps C, record `2026-09-30-named-gaps-c-derivations.md` §C.2/§C.3) | The MIDI arm `0x1D10E..0x1D12F` (zeroes `DS_001028C4/C0/CC`) and the slot break `0x1D16B` (slot 0's failure included). ~~The bump allocator cannot fail in-process without exhausting `mem[]`~~: `res.c`'s `PORT:` seam `res_fail_alloc_nth` fails the n-th request one-shot without moving the heap; `check_sound_buffers` V1-V4 pin both arms and the entry gate (mutations M1, M2). Residue: the two stores of 0 at `0x1D0F7`/`0x1D163` on the failure path are unobservable (§C.2.1). |
```

- [ ] **Step 4: PROGRESS.** Append this paragraph to `docs/PROGRESS.md`:

```markdown
**named-gaps C: the allocation-failure seam (G4, record `2026-09-30-named-gaps-c-derivations.md`).** `0x1D0BC`'s two allocation-failure arms are now tested. `res.c` gains a `PORT:` test seam, `res_fail_alloc_nth(n)`: the n-th bump-allocator request from then on returns 0 before the heap moves (as a block that does not fit does), then the seam disarms. It is 0 by default and nothing in `port/src` arms it. `check_sound_buffers` adds four vectors. The MIDI arm's failure (`0x1D10E..0x1D12F`) zeroes C4, C0 and CC and falls through to the slot arm. A slot-0 failure (`0x1D16B`) stops the loop and turns the DIG driver off (`0x1D195`), while a failure at slot 1..3 keeps it. Slot 0 set at entry makes no request. Deleting the MIDI zeroing fails 3 checks, and deleting the loop stop fails 8. The two stores of 0 on the failure path (`0x1D0F7`, `0x1D163`) cannot be observed and are recorded as residue. The suite gains 33 assertion sites (10 in `test_platform.c`, 23 in `test_game.c`). `make clean && make build && make verify` exits 0 with every oracle line equal to the k7-k12 baseline, the frame dumps match the base manifest, and the `make audio-render` WAV is byte-identical. The README's percentage is unchanged.
```

If a measured mutation count differs from 3 or 8, write the measured number.

- [ ] **Step 5: Commit.**

```bash
git add docs/superpowers/plans/2026-09-30-named-gaps-c-derivations.md docs/superpowers/plans/2026-09-29-all-gaps-ledger.md docs/PROGRESS.md
git commit -m "$(cat <<'EOF'
docs: G4 closed, 0x1D0BC's allocation-failure arms tested (record named-gaps-c §C.3)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

Then append the closing line to the SDD ledger `progress.md`.

---

## Self-Review

- **Spec coverage (§4.C):**
  - "A `PORT:` seam … fail the N-th request, off by default and inert in
    production": Task 1 Steps 6-8 and 10; unarmed proof by `rg` and by
    identical dumps.
  - "MIDI-buffer failure (C4/C0/CC zeroed)": V1 and M1/M1a-c.
  - "Sample-buffer failure (loop stops at the first failure; slot 0 empty
    sets C8 = 0)": V2/V3 and M2/M3/M4.
  - "The slot-0-already-set entry case": V4 and M6. The existing vector keeps
    its assertions.
  - "Seeded sentinels and a mutation proof per arm": the seed notes in Task 2
    Step 2 and the tables.
  - "Acceptance: both arms tested; the seam adds no behaviour when unarmed":
    Task 3 Step 1.
- **Raw anchoring:** every expected value traces to a `dx.py` line in Task 1
  Step 3:
  - `ecx` = D0 = 0 (`0x1D0DE`) explains why C4/C0/CC become 0.
  - The break before `inc ecx` (`0x1D16B`/`0x1D16D`) explains C8 = 0 only
    for slot 0.
  - The sizes come from `0x1D14D`/`0x1D154`.
  - The fall-through `0x1D12F → 0x1D132` explains V1's slot 0.
- **No unseeded BSS zero:**
  - The seam's default 0 is never asserted. Its inertness is shown
    behaviourally and by the gate.
  - Every asserted 0 is either written by the code under test over a non-zero
    seed, or is an explicit zero seed that a named mutation turns non-zero
    (M2).
- **Assertions that can fail:** every new site appears in at least one
  mutation row:
  - Seam checks: S0-S4.
  - V1: M1a-c, M5, S2.
  - V2: M2, M3, M7.
  - V3: M2, M4, M7, S1, S2.
  - V4: M6, M7, and the existing mutation b for C8.
- **Process constraints:**
  - No `game_init()`.
  - No new test function, so `test.h` is untouched.
  - `symbols.h` is untouched, since `DS_001028CC` already exists.
  - SDL and I/O are untouched.
  - No reformatting.
  - `port_progress` is unchanged, since the seam's comments do not start with
    `/* 0x`.
- **Risks:**
  - The heap grows by ≤ `0x4F000` in the unit suite, within the ~12 MB
    headroom.
  - M2/M7's exact line lists are predictions. The executor records the
    measured lines, and any shortfall is treated as a test defect.
  - The branch prerequisite (`0247a1a` for ledger §H) is checked first.
