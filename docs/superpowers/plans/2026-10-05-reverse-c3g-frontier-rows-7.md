# C3g: the frontier rows, part 7 (track P, batch C3g — the closing batch) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** End the track-C chain. Add the differential-verification row for each of the **31
addresses** the C3f record's §C3f.7 measures as the remaining frontier (the nineteen deferred C3f
addresses and the twelve the `0x3CF38` row exposes), in four dependency waves — **nineteen rows
were prototyped and measured by this planner** (the nine wave-1 rows the inherited scratch carried,
re-audited and re-measured here; ten of wave 2), **plus the batch's raw-over-port correction**
(`hit_reaction_b`'s table base `0xA7ACC`, found by its own `@table` mutant) — with their bindings,
mutants and exact-set pins. The remaining twelve addresses (`0x38ED0`, `0x34E2C`; the eight wave-3
text/`0x1922C`/`0x3CD94`; `0x2EF24`, `0x2F5A0`) are Tasks 3-4 from the C3f record's measured
sizes/callers. **The terminal verdict** (§C3g.7, required): after the measured rows the frontier is
exactly those twelve ported addresses and the named non-rows; **no C3h is provably needed** — the
plan closes them, leaving only the non-rows and the four rows open solely on them.

**Architecture:** Each row is a `Spec` in `tools/diff_verify.py` (`C3G_SPECS` with its `c3g_*`
fixtures), a `b_*` binding and `m_*` mutants in `port/tests/diff_runner.c` (`k_bindings`), and
exact-set assertions in `tools/tests/test_diff_verify.py` (`C3G_MASKS`, `C3G_KINDS`, the case-set
test, the clobber and counter lines). `port/src` changes: the `hit_reaction_b` table correction,
the AL normalizations (`hit_gate`'s return; wave 1's `hit_scan`/`hit_reaction_drive`/
`hit_frame_desc` seam and the `movsx` at `0x3CF14`), the seams (`0x3C600`, `0x3CCEC`, `0x3CE24`,
`0x4CE70`, `0x3CBC4`, `0x3CC58`, `0x34E2C`, `0x4649C`, `0x3CD94`, `0x2F0F0`) and the
`_CALLEE_CLOBBER_FIXES[0x3CF38]` entry.

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and
`capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track P, §5.1-§5.3,
§6 ("each ported function has a verification row; unhit blocks and non-emulable instructions named;
counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-05-reverse-c3g-derivations.md` (§C3g.1 the
members, §C3g.2 the rows, fixtures and the corrections, §C3g.3 the mutants and their measured
kinds/catching sets, §C3g.4 the counters, §C3g.5 the named gaps and limits, §C3g.6 what the planner
ran, §C3g.7 **the terminal verdict**). Recipe: `2026-10-01-reverse-e3-derivations.md` §E3.10;
checklist: `.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**What the planner ran (scratch, 2026-10-08, on `reverse-c3g` at `main` `87443cf` = C3f merged;
image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`):** the baseline in a detached worktree of the
same commit (the batch worktree carried the inherited scratch), then the inherited wave-1 prototype
re-audited (every self-check re-run, every pin re-measured, every semantic edit re-checked against
the raw) and ten wave-2 rows built and measured. Every row was measured with `--function NAME
--self-check` until VERIFIED with every mutant detected, then the full `python3
tools/diff_verify.py --self-check` counter (§C3g.4), `make entry-triage` (byte-identical) and
`python3 tools/port_progress.py` (`771 1203 64` / `731 731 100`). Then the prototype was reverted
(`git checkout -- port tools`); the patch below is the exact diff it applied. The **twelve
unmeasured addresses** carry C3f §C3f.7's evidence — a session split, not a correctness
judgement.

**Re-baseline note.** The counters below are `87443cf`'s (measured by this planner: `291/291
functions VERIFIED; 1087/1087 mutants detected; 1 named gaps; 205/225 rows with callees closed
(66 have none)`). If `main` moves before this plan executes, Task 1 records the measured base and
every later expected counter adds this plan's increments: functions +31, mutants +247 (without the
two hand-measured Tasks 3-4 rows this is partial; the executor measures each row as it lands), rows
with callees +24, no-callee rows +7, and the closed count from the rows the waves close. The E2
table must not move (no new ported target; no `fn_register`): if a task regenerates it, the line
must be byte-identical.

## Decisions needed from the user

**None.** The `hit_reaction_b` correction is derived from the raw with its address (`0x3CCBD`);
the AL normalizations and the `0x3CF38` clobber entry are byte-backed; the twelve unrowed
addresses are the C3f record's measured list, split by session budget.

## The C3g roadmap

| task | what | commit |
|---|---|---|
| 1 | baseline on `87443cf` (no commit) | - |
| 2 | wave 1: the nine inherited rows, re-audited and measured, with their seams and the clobber fix | `tools:` |
| 3 | wave 2: the ten measured rows (`hit_stance_ok`, `hit_gate`, `hit_frame_desc`, `hit_reaction_a`/`b`, `hit_reaction_allow`, `fighter_38c5c`, `text_cursor_hold`, `text_number_draw`, `text_vertical_set`), the `hit_reaction_b` correction and the two remaining rows (`0x38ED0`, `0x34E2C`) | `tools:` |
| 4 | waves 3+4: the ten remaining rows (`0x2EFD4`, `0x2F0F0`, `0x2F198`, `0x2F280`, `0x2F314`, `0x2F830`, `0x1922C`, `0x3CD94`, `0x2EF24`, `0x2F5A0`) | `tools:` |
| 5 | the review sweep (stores, clobbers, mutant case sets) | `tests:` (only if a fix) |
| 6 | closure: PROGRESS, the record's §C3g.8, the final gates and the terminal-verdict re-measure | `docs:` |

**This plan's own commit (planner) also folds the three C3f final-review prose nits** into the C3f
docs: (a) the §C3f.7 accounting sentence now says seven rows stay open (two named + the five the
deferred first wave blocks) and three closed; (b) §C3f.8's "265 `Spec` entries" now reads 292 (291
addresses plus the `0x468d8` alias); (c) the C3f plan's "The patch" section carries a pointer that
its frozen prototype text is pre-fix-wave (record §C3f.2/§C3f.8).

## Global Constraints

Copied verbatim from their sources:

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a
  capture, with the address that proves it. A value that cannot be pinned is a **named gap with its
  evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.**
  Record the correction and the address, in the derivation record and the report."
- Spec §8: "`make verify` is the gate after every task." User ruling (2026-10-01, speed-up): the
  per-task gate is the task's tests, `make diff-verify`, `make entry-triage` and the gp oracles
  whose miss sets the task touches; the full `make verify` runs at the baseline, the final task and
  before the merge. The task-scoped gates (the brief): `make diff-verify`, `make entry-triage`,
  `PR_ORACLE_REQUIRED=1 ./build/run_tests`, and `python3 -m unittest tools.tests.test_diff_verify`.
- AGENTS.md: "**The E2 target list is a gate** ... A commit that ports a target (a new `/* 0xADDR`
  header or `fn_register`) regenerates the table in the same commit (decision D3)". This plan ports
  no target: the table must be byte-identical.
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`." /
  "Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`." / "Code
  addresses stored in data go through `fn_origin()` / `fn_resolve()`." / "**`port/src/symbols.h` is
  generated** ... Never hand-edit it."
- AGENTS.md: "A ported function that a differentially verified function stubs or records ... opens
  with `PR_SEAM(0xADDR, args...)` (void) or `PR_SEAM_RET(0xADDR, args...)` ... The arguments are the
  C signature's, in order." Record E3 §E3.8: "A seam must be the callee's first statement".
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Assertions must be able to
  fail.**" / "Consolidating must not change an assertion": this plan extends the exact-set
  assertions of `RealFunctionTests` (rows, masks, mutant names, stub clobbers, the counter line)
  and the C3g case-set test; no other assertion changes.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style:
  `<area>: <what changed>`. **Never `git add -A`** — stage named files." No `Co-Authored-By`
  trailer. Never run `make gp-capture`.
- AGENTS.md: "Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and must record
  exactly the pinned known-set (`k_miss_known`) ... register the target or pin the miss with its
  evidence." The wave-1/2 rows add no `fn_register` and reach no new code pointer: the gp miss sets
  must stay as Task 1 measures them.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each
  function with its callees stubbed or run as stated.

## Where to run

The worktree `.worktrees/reverse-c3g` (branch `reverse-c3g`). Every command runs from the worktree
root; `data/` and `.superpowers/` are symlinks; `build/` exists. Before Task 1:

```bash
git rev-parse HEAD   # 87443cf
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_c3g_img.bin && shasum /tmp/pr_c3g_img.bin
ls port/tests/ghidra_data.bin   # the PR_ORACLE_REQUIRED fixture (git-ignored); copy it from the main repo if absent
ls data/k11-captures/gp-idle-loss   # the gp-oracle capture (git-ignored)
```

The image line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the image
differs from the one the record measured: stop. Ledger:
`.superpowers/sdd/2026-10-05-reverse-c3g/progress.md`.

## The 31 rows (the C3f record's four waves, executed as families)

The patch is one `git diff` (Task 2 applies it at once; its wave-2 part lands with Task 3's
remaining work); the families below are the gate order:

| family | rows | shared seams/fixtures |
|---|---|---|
| wave 1 — the deferred C3f first wave + the `0x3CF38` leaves | `0x1A7CC` `0x2A690` `0x37178` `0x38D90` `0x38FEC` `0x3BDDC` `0x3C6A8` `0x3CD44` `0x3CE58` | the existing allows (`0x33950`, `0x33A10`, `0x3AFC4`, `0x1A570`, `0x34D8C`, `0x1C500`+string tree, `0x2C3FC`…); `c3d_slot_pokes`; the `c3g_*` wave-1 fixtures |
| wave 2 — the `0x3CF38` tree's predicates and the first text rows | `0x3C600` `0x3CBC4` `0x3CC58` `0x3CCEC` `0x3CE24` `0x4CE70` `0x34E2C` `0x2F20C` `0x2F4BC` `0x2F4D0` `0x38C5C` `0x38ED0` | `C1_GEOM` (`0x1DDF4`, rowed in C1); the by-value `0x2F198`/`0x2EFD4` seams; the cursor at `0x105F34`; `c3g_desc_case`, `c3g_stance_case`, `c3g_gate_case`, `c3g_react_*_case`, `c3g_allow_case`, the text fixtures |
| wave 3 — the text tree's second wave and the `0x3CD94` leaf | `0x2EFD4` `0x2F0F0` `0x2F198` `0x2F280` `0x2F314` `0x2F830` `0x1922C` `0x3CD94` | `host_sprintf`/`host_memset` (C3f), the by-value seams, `0x2AD40` via `release_record` (rowed), `0x33950` |
| wave 4 — the text tree's leaves | `0x2EF24` `0x2F5A0` | the wave-3 rows' stubs; `0x2F198`'s caller stack buffer (P2.7) |

The measured rows (cases/blocks/mutants measured; `EAX mask` from the Spec):

| row | entry | cases | blocks | mutants | EAX mask | named notes |
|---|---|---|---|---|---|---|
| `hit_slot_seed` | 0x3C6A8 | 4 | 1/1 | 6/6 | 0 | `0x3C600` stub |
| `hit_scan` | 0x3CD44 | 8 | 6/6 | 6/6 | 0xFFFFFFFF | `0x3CCEC` stub (AL tested) |
| `fighter_38fec` | 0x38FEC | 6 | 4/4 | 5/5 | 0 | `0x38ED0` stub (Task 3) |
| `hit_reaction_drive` | 0x3CE58 | 12 | 16/16 | 11/11 | 0xFF | five stubs, `0x34D8C` allow |
| `fighter_38d90` | 0x38D90 | 3 | 8/8 | 7/7 | 0 | four text stubs, string path allows |
| `fighter_block_start` | 0x1A7CC | 7 | 8/10 | 11/11 | 0 | two dead arms named |
| `actor_pset_point` | 0x2A690 | 8 | 20/20 | 11/11 | 0 | `0x2A620` stub |
| `fighter_attack_consume` | 0x3BDDC | 8 | 16/16 | 13/13 | 0xFF | three stubs |
| `fighter_37178` | 0x37178 | 14 | 31/31 | 12/12 | 0 | three stubs |
| `hit_stance_ok` | 0x3CCEC | 9 | 8/8 | 7/7 | 0xFFFFFFFF | — |
| `hit_gate` | 0x3CE24 | 10 | 5/5 | 6/6 | 0xFF | `0x3CD94` stub (Task 4) |
| `hit_frame_desc` | 0x3C600 | 8 | 11/11 | 8/8 | 0xFFFFFFFF | the jump table's 11 blocks |
| `hit_reaction_a` | 0x3CBC4 | 8 | 8/8 | 7/7 | 0xFFFFFFFF | `C1_GEOM` stub |
| `hit_reaction_b` | 0x3CC58 | 7 | 8/8 | 7/7 | 0xFFFFFFFF | the table correction's row |
| `hit_reaction_allow` | 0x4CE70 | 17 | 29/29 | 8/8 | 0xFFFFFFFF | the 0x4CE54 jump table |
| `fighter_38c5c` | 0x38C5C | 2 | 1/1 | 10/10 | 0 | four text stubs (call-list row) |
| `text_cursor_hold` | 0x2F4BC | 2 | 1/1 | 4/4 | 0 | `0x2F198` stub with a write |
| `text_number_draw` | 0x2F4D0 | 2 | 1/1 | 7/7 | 0 | `0x2EFD4`/`0x2F198` stubs |
| `text_vertical_set` | 0x2F20C | 6 | 6/6 | 9/9 | 0 | `0x2F0F0`/`0x2F830` stubs |

The twelve Task 3-4 rows carry the C3f record §C3f.7's sizes and callers; the executor measures
each into this table's form as it lands.

No row reads outside the image (the tables, the cursor, the string buffers and the slots live in
the data object).

## How the code steps are written

Task 2 applies the patch (the prototype's exact `git diff`, embedded at the end of this plan) with
`git apply`. It touches `port/src/game/actors.c`, `port/src/game/fighter.c`, `port/tests/diff_runner.c`,
`tools/diff_emu.py`, `tools/diff_verify.py` and `tools/tests/test_diff_verify.py`. If `main` moved,
`git apply --3way` and resolve by keeping the patch's additions at the same anchors; never merge the
E2 table by hand. The patch carries the wave-1 rows and the ten measured wave-2 rows; Tasks 3-4 add
the remaining twelve to the same files.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/fighter.c` | the `hit_reaction_b` correction, the AL normalizations, the `0x3C600`/`0x3CCEC`/`0x3CE24`/`0x4CE70`/`0x3CBC4`/`0x3CC58`/`0x34E2C`/`0x4649C`/`0x3CD94` seams |
| `port/src/game/actors.c` | the `0x2F0F0` seam |
| `port/tests/diff_runner.c` | the nineteen bindings and their mutants, the `c3g_*` cores |
| `tools/diff_verify.py` | `C3G_SPECS` (the nineteen rows and their fixtures) |
| `tools/diff_emu.py` | the `_CALLEE_CLOBBER_FIXES[0x3CF38]` entry |
| `tools/tests/test_diff_verify.py` | `C3G_MASKS`, `C3G_KINDS`, the case-set test, the clobber and counter lines |
| `docs/superpowers/plans/2026-10-05-reverse-c3g-derivations.md` | the record (this plan's source of values) |
| `docs/PROGRESS.md` | Task 6's paragraph |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes the worktree at `87443cf`; produces the baseline counters
and the gp miss-set pin.

- [ ] **Step 1: the task gates on the untouched tree.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3g_base.bin DIFF_TABLE=/tmp/pr_c3g_base_table.md 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3g_base_e2.bin 2>&1 | tail -2
python3 tools/port_progress.py
```

Expected (measured at `87443cf` by the planner, in a detached worktree of the same commit):

```
diff-verify: 291/291 functions VERIFIED; 1087/1087 mutants detected; 1 named gaps; 205/225 rows with callees closed (66 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

- [ ] **Step 2: pin the gp miss sets and the oracle WAV.** The `PR_GP_DUMP` scenarios' pinned sets
  in `test_platform.c` must be the ones Tasks 2-4 leave untouched; no row here adds a `fn_register`
  or a new code pointer, so the miss sets and the oracle lines must not move (`make gp-oracle` and
  Task 6's full `make verify` are the evidence).

### Task 2: wave 1 — the nine inherited rows (one commit)

**Files:** the patch's wave-1 part. **Interfaces:** produces nine rows with their seams, the
`fighter.c` AL/`movsx` edits and the `_CALLEE_CLOBBER_FIXES` entry; consumes the C3d/C3f fixtures.

- [ ] **Step 1: apply the patch.** `git apply - <<'PATCH' ... PATCH` with the diff embedded at the
  end of this plan (the exact prototype diff; it also carries Task 3's ten measured rows).

- [ ] **Step 2: build and run the wave-1 self-checks.**

```bash
cmake --build build 2>&1 | tail -1
for f in hit_slot_seed hit_scan fighter_38fec hit_reaction_drive fighter_38d90 \
         fighter_block_start actor_pset_point fighter_attack_consume fighter_37178; do
  python3 tools/diff_verify.py --function "$f" --self-check
done
```

Expected rows (measured): `hit_slot_seed` 4/1-1/1/6, `hit_scan` 8/6-6/6/6, `fighter_38fec` 6/4-4/4/5,
`hit_reaction_drive` 12/16-16/16/11, `fighter_38d90` 3/8-8/8/7, `fighter_block_start` 7/8-8/10/11
(two dead arms named), `actor_pset_point` 8/20-20/20/11, `fighter_attack_consume` 8/16-16/16/13,
`fighter_37178` 14/31-31/31/12 (cases/blocks/mutants).

- [ ] **Step 3: the task gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3g_w1.bin 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3g_w1_e2.bin 2>&1 | tail -2
python3 tools/port_progress.py
```

Expected (measured by the planner on the wave-1 prototype): `300/300 functions VERIFIED; 1169/1169
mutants detected; 1 named gaps; 213/234 rows with callees closed (66 have none)`; E2
byte-identical; `771 1203 64` / `731 731 100`.

- [ ] **Step 4: commit.**

```bash
git add port/src/game/fighter.c port/tests/diff_runner.c tools/diff_emu.py tools/diff_verify.py \
        tools/tests/test_diff_verify.py
git commit -m "tools: C3g wave 1: the nine frontier rows and the 0x3CF38 clobber fix"
```

### Task 3: wave 2 — the ten measured rows, the correction and the two remaining (one commit)

**Files:** the patch's wave-2 part plus `0x38ED0`/`0x34E2C`. **Interfaces:** consumes Task 2's tree;
produces twelve rows and the `hit_reaction_b` correction.

- [ ] **Step 1: the ten measured rows' self-checks.**

```bash
for f in hit_stance_ok hit_gate hit_frame_desc hit_reaction_a hit_reaction_b hit_reaction_allow \
         fighter_38c5c text_cursor_hold text_number_draw text_vertical_set; do
  python3 tools/diff_verify.py --function "$f" --self-check
done
```

Expected rows (measured): the plan's table above (`hit_stance_ok` 9/8-8/8/7, `hit_gate` 10/5-5/5/6,
`hit_frame_desc` 8/11-11/11/8, `hit_reaction_a` 8/8-8/8/7, `hit_reaction_b` 7/8-8/8/7,
`hit_reaction_allow` 17/29-29/29/8, `fighter_38c5c` 2/1-1/1/10, `text_cursor_hold` 2/1-1/1/4,
`text_number_draw` 2/1-1/1/7, `text_vertical_set` 6/6-6/6/9).

- [ ] **Step 2: the two remaining rows.** `0x38ED0` (`fighter_38ed0`, EAX = side, EDX = the 0x44-byte
  combo record; walks +0x1B ids against `0x107A80 + side*0x40`, the +0x2F needs, then the three
  +0x18 thresholds against `(s16)DSW(0x107D2C + side*2)`; the met one redraws string 0xE5 and the
  pair `dword rec+4j`/`dword rec+0xC+4j` on rows 6/7 via `0x2F198` when slot+0x63 is clear; returns
  1/0) — its own row's details are record §C3f.7's caller column plus the raw at `0x38ED0`;
  `0x34E2C` (`hit_reaction_apply`, the 548-byte reaction applier: the 0xFF early return, the
  +0x88/+0x8A/+0x84/+0x5F stores, `0x18B04`, `0x1922C` (Task 4), the 0x1078FA==2 pair, the
  `fighter_anim_triple` stream/callback resolution through `fn_resolve`, `0x2C3FC`). Both follow
  the checklist and the §E3.10 recipe; `0x38ED0`'s `0x2F198` calls are by-value (P2.7) and
  `0x34E2C`'s `0x1922C`/`0x18B04`/`0x3AFC4`/`0x3C4CC`/`0x3C520`/`0x2C3FC` calls are the existing
  seams.

- [ ] **Step 3: the task gates.** As Task 2 Step 3 but with `DIFF_IMAGE=/tmp/pr_c3g_w2.bin`; the
  counter is the record §C3g.4's final value plus the two new rows (the executor records its own
  measured line). E2 byte-identical; `port_progress.py` unchanged.

- [ ] **Step 4: commit.**

```bash
git add port/src/game/fighter.c port/src/game/actors.c port/tests/diff_runner.c tools/diff_verify.py \
        tools/tests/test_diff_verify.py
git commit -m "tools: C3g wave 2: the 0x3CF38 tree, the text cinq and the hit_reaction_b correction"
```

### Task 4: waves 3+4 — the ten remaining rows (one commit)

**Files:** the text/tree rows' seams and rows. **Interfaces:** consumes Tasks 2-3; produces the
last ten rows.

- [ ] **Step 1: wave 3, bottom-up.** `0x2F0F0` (`text_width`, seam added this batch), `0x2F198`
  (`text_cursor_set`), `0x2F280` (`text_cells_release`), `0x2F314`
  (`text_cells_release_vertical`), `0x2F830` (`text_render`), `0x2EFD4` (`text_number_format`),
  `0x1922C` (`hit_stance_timer`, called by `0x34E2C`), `0x3CD94` (`hit_immunity`, seam added this
  batch). `text_render`'s string is truncated in place (0x2A/0x1E limits), `0x2F198`'s by-value
  dwords are the P2.7 form, `0x2F280`'s `release_record` (`0x2AD40`) and `0x33950` are rowed.
- [ ] **Step 2: wave 4.** `0x2EF24` (`text_number_core`, the `host_sprintf` seam and the `%i` at
  `0x80B40`), `0x2F5A0` (`text_glyph_emit`, the descriptor/`0x2AE14`/`0x2A408`/`0x2B2A0` path; the
  col/row pointers are the P2.7 by-value form of the C3f seam).
- [ ] **Step 3: the task gates.** As Task 3 Step 3; the expected counter is the fully-closed state:
  the record §C3g.4's final value plus these ten rows' increments, with every wave-1/2 row that
  was open on these addresses closed (the executor records the measured line).
- [ ] **Step 4: commit.**

```bash
git add port/src/game/actors.c port/src/game/fighter.c port/tests/diff_runner.c tools/diff_verify.py \
        tools/tests/test_diff_verify.py
git commit -m "tools: C3g waves 3+4: the text tree, 0x3CD94 and 0x1922C"
```

### Task 5: the review sweep (one commit, only if a fix)

**Files:** the rows. **Interfaces:** consumes Tasks 2-4; produces the store sweep's findings.

- [ ] **Step 1: the store sweep.** For every C3g row, poke each field the row writes to a value
  that differs from what it writes and re-run `--function NAME --self-check`; a store with no
  sentinel fails some mutant. The batch's new stores: the slot phase/displacement/stun words of
  the `0x3C6A8` family, the pset dwords of `0x2A690`, the slots' +0x43/+0x52..+0x57/+0x5F/+0x7C
  of the `0x3BDDC`/`0x37178`/`0x3CE58` rows, the `0x105F34` cursor (the text rows), the
  `0x107D40`/`0x1078F8`/`0x1078FE`/`0x107D18` globals and the `0x107A80` tables.
- [ ] **Step 2: the clobber and case-set re-check.** `python3 -m unittest
  tools.tests.test_diff_verify.RealFunctionTests` (the clobber re-derivation with the new stub
  entries and the C3g case-set test). Expected: OK.
- [ ] **Step 3: commit any fix.**

```bash
git add <the fixed files>
git commit -m "tests: C3g review sweep: the store sentinels and the case-set pins"
```

### Task 6: Closure (one commit)

**Files:** `docs/PROGRESS.md`, the record. **Interfaces:** consumes Tasks 2-4's measured state.

- [ ] **Step 1: the final gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3g_close.bin 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3g_close_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/port_progress.py
make verify
```

Expected: the Task 4 counter (the fully-closed state); E2 byte-identical; `all checks passed`;
`771 1203 64` / `731 731 100`; and the full `make verify` exit 0 with the oracle lines equal to
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`, the `make audio-render` WAV
equal to the pre-change WAV, every gp ratchet line `ok` with measured == pin, and `symbols.h`
byte-identical. The `hit_reaction_b` correction and the AL normalizations are the batch's
behavioral changes; the full ladder is what proves no oracle-visible path moved.

- [ ] **Step 2: re-measure the terminal verdict** (the record's §C3g.7 list) against the final tree:
  scan the final `tools/diff_verify.py`'s `Spec` entries against the 31 addresses and the named
  non-rows. Every one must now have a row except the named non-rows (`0x2EA30`, `0x1B544`,
  `0x5D812`, `0x29D60`, `0x2EA64`, `0x32BAC`, `0x500BB`, `0x5DEED`, the AIL wrappers); if any
  listed address got rowed or any new frontier appears, update §C3g.7 with the evidence.
- [ ] **Step 3: append the PROGRESS paragraph** (the 31 rows, the counter, the `hit_reaction_b`
  correction, the terminal verdict).
- [ ] **Step 4: append §C3g.8 Results to the record** (the executed tree's counters, the commit
  shas, the gate log lines), mirroring C3f's §C3f.8.
- [ ] **Step 5: commit.**

```bash
git add docs/PROGRESS.md docs/superpowers/plans/2026-10-05-reverse-c3g-derivations.md
git commit -m "docs: C3g closure: the frontier closed and the terminal verdict"
```

### The patch

```diff
diff --git a/port/src/game/actors.c b/port/src/game/actors.c
index 2c4b5fc..07b038e 100644
--- a/port/src/game/actors.c
+++ b/port/src/game/actors.c
@@ -3916,6 +3916,7 @@ u32 actor_spawn(const u32 *desc, u32 a2, u32 a3, u32 a4, u32 a5)
  * (DS 0x3D048 / 0x3D1EC). */
 int text_width(const u8 *s, u32 mode)
 {
+    PR_SEAM_RET(0x2F0F0u, (u32)(s - mem), mode);
     mode &= 3u;
     const u32 table = (mode == 2u) ? 0xbd048u : 0xbd1ecu;
     if (mode != 2u && mode != 3u)
diff --git a/port/src/game/fighter.c b/port/src/game/fighter.c
index b7d45d0..aeaa054 100644
--- a/port/src/game/fighter.c
+++ b/port/src/game/fighter.c
@@ -608,6 +608,7 @@ u32 fighter_input_read(u32 side, s32 index)
  * 16 bits overlap `mask`; 1 on the first hit, 0 otherwise. */
 int fighter_input_scan(u32 side, s32 n1, s32 n2, u32 mask)
 {
+    PR_SEAM_RET(0x4649Cu, side, n1, n2, mask);
     s32 pos = (s32)DSD(DS_001082D2) >> 16;      /* 0x464A5/0x464AC */
     if (n1 > 0) {
         s32 n = n1;
@@ -3739,11 +3740,13 @@ void fighter_35e04(u32 rec)
 #define HIT_IMMUNE_MASK  0x000A182Cu  /* 0xA182C: 64 rows x 8 bytes per character */
 #define HIT_GEOM_TABLE   0x000A7A70u  /* 0xA7A70: 0x1DDF4's per-character threshold */
 #define HIT_GEOM_TABLE2  0x000A7B44u  /* 0xA7B44: 0x1DDF4's per-character table pointer */
+#define HIT_GEOM_TABLE_B 0x000A7ACCu  /* 0xA7ACC: 0x3CC58's per-character table pointer */
 
 /* 0x3C600. The per-attack-frame descriptor for (side, i). `sel` is 2 whenever
  * slot+0x63 != 0 (always in the demo), so the player table 0xC619C is read. */
 u32 hit_frame_desc(u32 side, u32 i)
 {
+    PR_SEAM_RET(0x3C600u, side, i);
     u32 slot = DS_001077B0 + side * 0x94u;
     u32 sel;
     if (DSB(slot + 0x63u) != 0u) {
@@ -3900,6 +3903,7 @@ void hit_slot_step(void)
  * descriptor's `e` flag (byte +7) for stance 2, `d` (byte +6) for stance 0/1. */
 int hit_stance_ok(u32 side, u32 i)
 {
+    PR_SEAM_RET(0x3CCECu, side, i);
     u32 slot = DS_001077B0 + side * 0x94u;
     u32 entry = (u32)DSB(slot + 0x7Au) * 0x20u + i;
     u8 e = DSB(HIT_TABLE_PLAYER + entry * 8u + 7u);
@@ -3917,7 +3921,7 @@ s32 hit_scan(u32 side)
     PR_SEAM_RET(0x3CD44u, side);
     for (u32 i = 0; i < 0x20u; i++) {
         if ((s16)DSW(DS_00107D58 + side * 0x40u + i * 2u) == 8
-                && hit_stance_ok(side, i))
+                && (u8)hit_stance_ok(side, i))          /* 0x3CD68 `test al,al` */
             return (s32)i;
     }
     return -1;
@@ -3928,6 +3932,7 @@ s32 hit_scan(u32 side)
  * candidate reaction is set. */
 int hit_immunity(u32 side, u32 i)
 {
+    PR_SEAM_RET(0x3CD94u, side, i);
     u32 slot = DS_001077B0 + side * 0x94u;
     u8 r = DSB(slot + 0x5Fu);
     if (r == 0xFFu) return 1;
@@ -3946,15 +3951,17 @@ int hit_immunity(u32 side, u32 i)
  * of the dword at slot+0x53) <= 5 and 0x3CD94 != 0. */
 int hit_gate(u32 side, u32 i)
 {
+    PR_SEAM_RET(0x3CE24u, side, i);
     u32 slot = DS_001077B0 + side * 0x94u;
     if ((s8)DSB(slot + 0x56u) >= 6) return 0;
-    return hit_immunity(side, i) != 0;
+    return (u8)hit_immunity(side, i) != 0;              /* 0x3CE4F `test al,al` */
 }
 
 /* 0x4CE70. The per-character reaction allow-list: 1 unless `reaction` is one of
  * the character's blocked ids. */
 int hit_reaction_allow(u32 side, u32 reaction)
 {
+    PR_SEAM_RET(0x4CE70u, side, reaction);
     u32 ch = (u32)DSB(DS_001077B0 + side * 0x94u + 0x7Au);
     if (ch < 7u) {
         switch (ch) {
@@ -4028,6 +4035,7 @@ int hit_geometry(u32 side, u32 table, u32 idx)
  * leftovers 0x1DDF4 reads (ctx[2] = &slot[side] for the character byte). */
 u16 hit_reaction_a(u32 side)
 {
+    PR_SEAM_RET(0x3CBC4u, side);
     u32 slot = DS_001077B0 + side * 0x94u;
     if (DSB(slot + 0x54u) == 2u) return 0x16u;
     if ((DSW(DS_001088E0 + side * 2u) & 0x4000u) != 0u) return 0x14u;
@@ -4041,11 +4049,12 @@ u16 hit_reaction_a(u32 side)
 /* 0x3CC58. Reaction variant B: the same shape with 0x17/0x15/0x13/0x11. */
 u16 hit_reaction_b(u32 side)
 {
+    PR_SEAM_RET(0x3CC58u, side);
     u32 slot = DS_001077B0 + side * 0x94u;
     if (DSB(slot + 0x54u) == 2u) return 0x17u;
     if ((DSW(DS_001088E0 + side * 2u) & 0x4000u) != 0u) return 0x15u;
     if (hit_geometry(1u - side,
-                     DSD(HIT_GEOM_TABLE2 + (u32)DSB(slot + 0x7Au) * 4u),
+                     DSD(HIT_GEOM_TABLE_B + (u32)DSB(slot + 0x7Au) * 4u),  /* 0x3CCBD */
                      HIT_GEOM_TABLE))
         return 0x13u;
     return 0x11u;
@@ -4253,6 +4262,7 @@ void hit_sound(u32 ch)
  * word 0xE9308[byte anim[0]+7] (record k7-k12 §7). */
 void hit_reaction_apply(u32 side, u32 reaction)
 {
+    PR_SEAM(0x34E2Cu, side, reaction);
     u32 slot = DS_001077B0 + side * 0x94u;
     u32 other = DS_001077B0 + (1u - side) * 0x94u;
     u32 rec = DSD(slot);
@@ -5302,16 +5312,17 @@ int hit_reaction_drive(u32 side, u32 i)
     PR_SEAM_RET(0x3CE58u, side, i);
     u32 slot = DS_001077B0 + side * 0x94u;
     u32 reaction;
-    if (!hit_gate(side, i)) return 0;                   /* 0x3CE5E */
+    if (!(u8)hit_gate(side, i)) return 0;               /* 0x3CE63 `test al,al` */
     if (i >= 0x20u) return 1;                           /* 0x3CE73 */
     reaction = (u32)(s16)DSW(HIT_TABLE_PLAYER
                              + ((u32)DSB(slot + 0x7Au) * 0x20u + i) * 8u + 4u);
     if ((DSW(DS_00104B00) == 0x21u || DSW(DS_00104B00) == 0x22u)
-            && !hit_reaction_allow(side, reaction))
+            && !(u8)hit_reaction_allow(side, reaction))  /* 0x3CEBE `test al,al` */
         return 0;                                       /* 0x3CEC0 */
     hit_flash_pair(side);                               /* 0x3CEC8 */
     if (reaction == 0x10u)      reaction = hit_reaction_a(side);   /* 0x3CF04 */
     else if (reaction == 0x11u) reaction = hit_reaction_b(side);   /* 0x3CF0D */
+    reaction = (u32)(s32)(s16)reaction;                 /* 0x3CF14 `movsx edx,ax` */
     DSB(slot + 0x5Fu) = (u8)reaction;                   /* 0x3CF25 */
     hit_reaction_apply(side, reaction);                 /* 0x3CF2E */
     return 1;
diff --git a/port/tests/diff_runner.c b/port/tests/diff_runner.c
index ea56741..efcec55 100644
--- a/port/tests/diff_runner.c
+++ b/port/tests/diff_runner.c
@@ -12349,6 +12349,674 @@ static void m_c3f_unlock_and(const u32 *r, u32 *eax){ c3f_1e808_core(r, 4u); *ea
 static void m_c3f_unlock_store(const u32 *r, u32 *eax){ c3f_1e808_core(r, 8u); *eax = 0u; }
 static void m_c3f_unlock_clock(const u32 *r, u32 *eax){ c3f_1e808_core(r, 16u); *eax = 0u; }
 
+/* ---- C3g (record 2026-10-05-reverse-c3g): wave 1 -------------------------------------------- */
+
+/* 0x3C6A8's mutants. */
+static u32 c3g_3c6a8_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX], value = r[R_EDX], i = r[R_EBX];
+    u32 desc = hit_frame_desc(side, i);
+    u32 off = side * ((mut & 1u) ? 0x94u : 0x40u) + i * ((mut & 2u) ? 1u : 2u);
+    if (mut & 4u) {                                     /* @phase: the two arrays swapped */
+        DSW(0x00107E58u + off) = (u16)value;
+        DSW(0x00107D58u + off) = 0;
+    } else {
+        DSW(0x00107D58u + off) = (u16)value;
+        DSW(0x00107E58u + off) = 0;
+    }
+    if (!(mut & 8u))                                    /* @clear */
+        DSW(0x00107DD8u + off) =
+            DSW(desc + ((mut & 16u) ? 0x08u : 0x0Cu) + value * ((mut & 32u) ? 4u : 0x14u));
+    return 0;
+}
+static void b_c3g_3c6a8(const u32 *r, u32 *eax) { hit_slot_seed(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
+static void m_c3g_seed_phase(const u32 *r, u32 *eax) { *eax = c3g_3c6a8_core(r, 4u); }
+static void m_c3g_seed_clear(const u32 *r, u32 *eax) { *eax = c3g_3c6a8_core(r, 8u); }
+static void m_c3g_seed_off(const u32 *r, u32 *eax)   { *eax = c3g_3c6a8_core(r, 1u); }
+static void m_c3g_seed_field(const u32 *r, u32 *eax) { *eax = c3g_3c6a8_core(r, 16u); }
+static void m_c3g_seed_scale(const u32 *r, u32 *eax) { *eax = c3g_3c6a8_core(r, 32u); }
+static void m_c3g_seed_bytes(const u32 *r, u32 *eax) { *eax = c3g_3c6a8_core(r, 2u); }
+
+/* 0x3CD44's mutants. */
+static u32 c3g_3cd44_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    for (u32 i = 0; i < ((mut & 1u) ? 0x1Fu : 0x20u); i++) {
+        u32 off = side * ((mut & 2u) ? 0x94u : 0x40u) + i * ((mut & 4u) ? 4u : 2u);
+        if ((s16)DSW(0x00107D58u + off) == ((mut & 8u) ? 9 : 8)
+                && ((mut & 16u) ? hit_stance_ok(side, i) : (u8)hit_stance_ok(side, i)))
+            return (s32)i;
+    }
+    return (mut & 32u) ? 0u : 0xFFFFFFFFu;
+}
+static void b_c3g_3cd44(const u32 *r, u32 *eax)      { *eax = (u32)hit_scan(r[R_EAX]); }
+static void m_c3g_scan_bound(const u32 *r, u32 *eax) { *eax = c3g_3cd44_core(r, 1u); }
+static void m_c3g_scan_side(const u32 *r, u32 *eax)  { *eax = c3g_3cd44_core(r, 2u); }
+static void m_c3g_scan_stride(const u32 *r, u32 *eax){ *eax = c3g_3cd44_core(r, 4u); }
+static void m_c3g_scan_phase(const u32 *r, u32 *eax) { *eax = c3g_3cd44_core(r, 8u); }
+static void m_c3g_scan_al(const u32 *r, u32 *eax)    { *eax = c3g_3cd44_core(r, 16u); }
+static void m_c3g_scan_miss(const u32 *r, u32 *eax)  { *eax = c3g_3cd44_core(r, 32u); }
+
+/* 0x38FEC's mutants. */
+static u32 c3g_38fec_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 ctx[6];
+    fighter_ctx_same(ctx, side);
+    u32 ch = (u32)DSB(ctx[2] + ((mut & 1u) ? 0x7Bu : 0x7Au));
+    s32 n = (s32)(s16)DSW(((mut & 2u) ? 0x000BEBB6u : 0x000BEBB8u) + ch * 2u);
+    u32 rec = DSD(((mut & 4u) ? 0x000BEBB8u : 0x000BEB90u) + ch * 4u);
+    for (s32 i = 0; i < n; i++) {
+        if (!(mut & 16u) && fighter_38ed0(ctx[0], rec) != 0u) return 0;
+        rec += (mut & 8u) ? 0x40u : 0x44u;
+    }
+    return 0;
+}
+static void b_c3g_38fec(const u32 *r, u32 *eax)   { fighter_38fec(r[R_EAX]); *eax = 0u; }
+static void m_c3g_fec_ch(const u32 *r, u32 *eax)  { *eax = c3g_38fec_core(r, 1u); }
+static void m_c3g_fec_n(const u32 *r, u32 *eax)   { *eax = c3g_38fec_core(r, 2u); }
+static void m_c3g_fec_table(const u32 *r, u32 *eax) { *eax = c3g_38fec_core(r, 4u); }
+static void m_c3g_fec_stride(const u32 *r, u32 *eax){ *eax = c3g_38fec_core(r, 8u); }
+static void m_c3g_fec_call(const u32 *r, u32 *eax) { *eax = c3g_38fec_core(r, 16u); }
+
+/* 0x3CE58's mutants. */
+static u32 c3g_3ce58_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX], i = r[R_EDX];
+    u32 slot = 0x001077B0u + side * 0x94u;
+    u32 mode, reaction, gate;
+    gate = (mut & 1024u) ? (u32)hit_gate(side, i) : (u32)(u8)hit_gate(side, i);
+    if ((mut & 1u) ? (gate != 0u) : (gate == 0u)) return 0;     /* @gate / @gateal */
+    if ((mut & 2u) ? (i > 0x20u) : (i >= 0x20u)) return 1u;
+    reaction = (u32)(s16)DSW(0x000C619Cu + ((u32)DSB(slot + 0x7Au) * 0x20u + i) * 8u + 4u);
+    mode = DSW(0x00104B00u);
+    if ((mode == 0x21u || ((mut & 4u) ? 0u : (mode == 0x22u)))
+            && !((mut & 8u) ? (u32)hit_reaction_allow(side, reaction)
+                            : (u32)(u8)hit_reaction_allow(side, reaction)))
+        return 0;
+    if (!(mut & 16u)) hit_flash_pair(side);             /* @flash */
+    if ((mut & 32u) ? (reaction == 0x11u) : (reaction == 0x10u))
+        reaction = hit_reaction_a(side);
+    else if ((mut & 32u) ? (reaction == 0x10u) : (reaction == 0x11u))
+        reaction = hit_reaction_b(side);
+    reaction = (u32)(s32)(s16)reaction;
+    DSB(slot + ((mut & 64u) ? 0x5Eu : 0x5Fu)) = (u8)((mut & 128u) ? (reaction >> 8) : reaction);
+    if (!(mut & 256u)) hit_reaction_apply(side, reaction);
+    return (mut & 512u) ? 0u : 1u;
+}
+static void b_c3g_3ce58(const u32 *r, u32 *eax)
+{ *eax = (u32)hit_reaction_drive(r[R_EAX], r[R_EDX]); }
+static void m_c3g_drv_gate(const u32 *r, u32 *eax)  { *eax = c3g_3ce58_core(r, 1u); }
+static void m_c3g_drv_gateal(const u32 *r, u32 *eax){ *eax = c3g_3ce58_core(r, 1024u); }
+static void m_c3g_drv_bound(const u32 *r, u32 *eax) { *eax = c3g_3ce58_core(r, 2u); }
+static void m_c3g_drv_mode(const u32 *r, u32 *eax)  { *eax = c3g_3ce58_core(r, 4u); }
+static void m_c3g_drv_allow(const u32 *r, u32 *eax) { *eax = c3g_3ce58_core(r, 8u); }
+static void m_c3g_drv_flash(const u32 *r, u32 *eax) { *eax = c3g_3ce58_core(r, 16u); }
+static void m_c3g_drv_sel(const u32 *r, u32 *eax)   { *eax = c3g_3ce58_core(r, 32u); }
+static void m_c3g_drv_store(const u32 *r, u32 *eax) { *eax = c3g_3ce58_core(r, 64u); }
+static void m_c3g_drv_hi(const u32 *r, u32 *eax)    { *eax = c3g_3ce58_core(r, 128u); }
+static void m_c3g_drv_apply(const u32 *r, u32 *eax) { *eax = c3g_3ce58_core(r, 256u); }
+static void m_c3g_drv_ret(const u32 *r, u32 *eax)   { *eax = c3g_3ce58_core(r, 512u); }
+
+/* 0x38D90's mutants. */
+static u32 c3g_38d90_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    s32 c = (s32)side * 0x25;
+    if (!(mut & 1u)) fighter_38c5c(side);
+    text_cursor_hold(-1, 6, game_string_get(0xE5u), 0x3000u);
+    text_cursor_hold(-1, (mut & 2u) ? 6 : 7, game_string_get((mut & 4u) ? 0xE4u : 0xE5u), 0x3000u);
+    text_number_draw(c + 2, 8, (s32)(s16)DSW(0x00107D2Cu + side * 2u), 2, 3u, 0x1000u);
+    text_vertical_set(c + 2, 9, mem + 0x000BE01Cu, (mut & 8u) ? 0x2000u : 0x1000u);
+    if (DSB(0x001077B0u + side * 0x94u + 0x63u) != 0u) return 0;
+    text_number_draw((mut & 16u) ? c + 1 : c, (mut & 32u) ? 0x0Fu : 0x10u,
+                     (s32)(s16)DSW(0x00107D20u + side * 2u), 3, 1u, 0x2000u);
+    if (!(mut & 64u))
+        text_cursor_hold(c + 3, 0x10, mem + 0x00080BF0u, 0x2000u);
+    return 0;
+}
+static void b_c3g_38d90(const u32 *r, u32 *eax)   { fighter_38d90(r[R_EAX]); *eax = 0u; }
+static void m_c3g_90_c5c(const u32 *r, u32 *eax)  { *eax = c3g_38d90_core(r, 1u); }
+static void m_c3g_90_row(const u32 *r, u32 *eax)  { *eax = c3g_38d90_core(r, 2u); }
+static void m_c3g_90_id(const u32 *r, u32 *eax)   { *eax = c3g_38d90_core(r, 4u); }
+static void m_c3g_90_mode(const u32 *r, u32 *eax) { *eax = c3g_38d90_core(r, 8u); }
+static void m_c3g_90_col(const u32 *r, u32 *eax)  { *eax = c3g_38d90_core(r, 16u); }
+static void m_c3g_90_pctrow(const u32 *r, u32 *eax) { *eax = c3g_38d90_core(r, 32u); }
+static void m_c3g_90_pct(const u32 *r, u32 *eax)  { *eax = c3g_38d90_core(r, 64u); }
+
+/* 0x1A7CC's mutants. */
+static u32 c3g_1a7cc_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 ctx[6], tri[3];
+    fighter_ctx_swap(ctx, side);
+    DSB(ctx[3] + 0x43u) &= (mut & 1u) ? 0xFEu : 0xFDu;
+    hit_facing_flag(ctx[1]);
+    DSB(ctx[3] + 0x61u) = (mut & 2u) ? 1u : 0u;
+    DSB(ctx[3] + 0x60u) = (mut & 4u) ? 0x1Fu : 0x1Eu;
+    DSB(ctx[3] + 0x62u) = (mut & 8u) ? 0u : 1u;
+    if (DSB(ctx[2] + 0x5Fu) <= ((mut & 16u) ? 0x3Eu : 0x3Fu)) {
+        if (!(mut & 32u)) fighter_anim_triple(tri, ctx[0], (s32)DSB(ctx[2] + 0x5Fu));
+        else tri[0] = ctx[0];
+        if (DSB(ctx[2] + 0x5Fu) < 0x40u)
+            DSB(ctx[3] + 0x60u) = DSB(tri[0] + 0x0Au);
+    }
+    fighter_block_anim(ctx[3], ctx[5]);
+    {
+        u32 o = ctx[0] * 2u;
+        DSW(0x00100CE0u + o) = (u16)(DSW(0x00100CE0u + o) + 1u);
+        s32 k = (s32)DSD(0x00100CDEu + o) >> 16;
+        if ((u32)k >= ((mut & 64u) ? 8u : 7u)
+                || (!(mut & 128u) && (s16)DSW(0x00100CE0u + o) < 0)) {
+            DSB(ctx[3] + 0x60u) = 2u;
+        } else {
+            s32 pct = (s32)DSD(0x000A2C4Au + (u32)k * ((mut & 256u) ? 1u : 2u)) >> 16;
+            s32 t = (s32)DSD(ctx[3] + 0x5Du) >> 24;
+            DSB(ctx[3] + 0x60u) = (mut & 512u)
+                ? (u8)((u32)(pct * t) / 100u) : (u8)((pct * t) / 100);
+        }
+    }
+    DSB(ctx[3] + 0x52u) = (mut & 1024u) ? 5u : 6u;
+    DSB(ctx[3] + 0x53u) = (mut & 2048u) ? 0u : 1u;
+    return 0;
+}
+static void b_c3g_1a7cc(const u32 *r, u32 *eax)   { fighter_block_start(r[R_EAX]); *eax = 0u; }
+static void m_c3g_bs_and(const u32 *r, u32 *eax)  { *eax = c3g_1a7cc_core(r, 1u); }
+static void m_c3g_bs_s61(const u32 *r, u32 *eax)  { *eax = c3g_1a7cc_core(r, 2u); }
+static void m_c3g_bs_s60(const u32 *r, u32 *eax)  { *eax = c3g_1a7cc_core(r, 4u); }
+static void m_c3g_bs_s62(const u32 *r, u32 *eax)  { *eax = c3g_1a7cc_core(r, 8u); }
+static void m_c3g_bs_bound(const u32 *r, u32 *eax){ *eax = c3g_1a7cc_core(r, 16u); }
+static void m_c3g_bs_tri(const u32 *r, u32 *eax)  { *eax = c3g_1a7cc_core(r, 32u); }
+static void m_c3g_bs_k(const u32 *r, u32 *eax)    { *eax = c3g_1a7cc_core(r, 64u); }
+static void m_c3g_bs_div(const u32 *r, u32 *eax)  { *eax = c3g_1a7cc_core(r, 512u); }
+static void m_c3g_bs_tbl(const u32 *r, u32 *eax)  { *eax = c3g_1a7cc_core(r, 256u); }
+static void m_c3g_bs_s52(const u32 *r, u32 *eax)  { *eax = c3g_1a7cc_core(r, 1024u); }
+static void m_c3g_bs_s53(const u32 *r, u32 *eax)  { *eax = c3g_1a7cc_core(r, 2048u); }
+
+/* 0x2A690's mutants. */
+static u32 c3g_2a690_core(const u32 *r, u32 mut)
+{
+    u32 rec = r[R_EAX];
+    u32 pset = actor_pset(rec);
+    u32 x;
+    if ((DSW(rec + 0x28) >> 8 & ((mut & 1u) ? 0x20u : 0x10u)) == 0) {
+        x = DSD(rec + 0x18) - ((mut & 2u) ? 0u : DSD(0x000F0AF0))
+            + ((mut & 4u) ? 0xFFFFD600u : 0x2A00u);
+    } else {
+        if ((mut & 4096u) ? 0 : (DSW(rec + 0x38) != 0)) mode1_cursor(rec, pset);
+        if ((s8)DSB(rec + 0x64) >= ((mut & 8u) ? 1 : 0)) {
+            s32 idx = (s32)DSD(rec + 0x61) >> 24;
+            DSW(rec + 0x46) = DSW(((mut & 16u) ? 0x00107902u : 0x00107900u) + (u32)idx * 2u);
+            x = DSD(rec + 0x18) + 0x2A00u
+                - (u32)(((s32)DSD(rec + 0x44) >> 16) * ((mut & 32u) ? 1 : 2));
+        } else if (DSW(rec + 0x34) == 0) {
+            x = DSD(rec + 0x18) + 0x2A00u - (u32)((s32)DSD(0x00107A44) >> 16);
+        } else {
+            s32 p = ((s32)DSD(0x00107A44) >> 16) * ((s32)DSD(rec + 0x32) >> 16);
+            x = DSD(rec + 0x18) + 0x2A00u - ((mut & 64u) ? (u32)(p >> 8) : (u32)(p / 256));
+        }
+    }
+    DSD(pset + 0x10) = DSD(pset + 4);
+    DSD(pset + 0x14) = DSD(pset + 8);
+    s32 ysrc;
+    if ((DSW(rec + 0x28) & ((mut & 128u) ? 0x80u : 0x40u)) == 0)
+        ysrc = ((s32)DSD(rec + 0x30) >> 16) + (s32)DSD(rec + 0x1c);
+    else
+        ysrc = (s32)DSD(rec + 0x30) >> 16;
+    s32 y = (s32)DSD(0x000F0AEC) + 0x3bc0 - ysrc;
+    DSD(0x00105BE0) = (u32)y;
+    DSD(pset + 4) = x;
+    DSD(pset + 8) = (u32)y;
+    if ((s16)DSW(rec + 0x32) > ((mut & 256u) ? -1 : 0)) {
+        s32 layer = 0xf0 - ((s32)DSD(rec + 0x30) >> 22) + ((s32)DSD(rec + 0x56) >> 24);
+        u16 u = (u16)layer;
+        if ((s32)u > 0xff) u = 0xff;
+        DSW(pset + 0x0e) = u;
+    } else {
+        s32 layer = (s32)(s8)DSB(rec + 0x59) + 0xf0;
+        if ((mut & 512u) && layer < 0) layer = 0;
+        if (layer > 0xff) layer = 0xff;
+        DSW(pset + 0x0e) = (u16)layer;
+    }
+    DSW(pset + 0x0c) = DSW(rec + 0x2c);
+    DSD(rec + 0x3c) = (mut & 1024u) ? 0x12345678u : DSD(pset + 4);
+    DSD(0x00105BDC) = ((mut & 2048u) ? DSD(pset + 8) : x);
+    return 0;
+}
+static void b_c3g_2a690(const u32 *r, u32 *eax)   { actor_pset_point(r[R_EAX]); *eax = 0u; }
+static void m_c3g_pp_bit(const u32 *r, u32 *eax)  { *eax = c3g_2a690_core(r, 1u); }
+static void m_c3g_pp_x0(const u32 *r, u32 *eax)   { *eax = c3g_2a690_core(r, 2u); }
+static void m_c3g_pp_x2(const u32 *r, u32 *eax)   { *eax = c3g_2a690_core(r, 4u); }
+static void m_c3g_pp_gte(const u32 *r, u32 *eax)  { *eax = c3g_2a690_core(r, 8u); }
+static void m_c3g_pp_ramp(const u32 *r, u32 *eax) { *eax = c3g_2a690_core(r, 16u); }
+static void m_c3g_pp_x44(const u32 *r, u32 *eax)  { *eax = c3g_2a690_core(r, 32u); }
+static void m_c3g_pp_div(const u32 *r, u32 *eax)  { *eax = c3g_2a690_core(r, 64u); }
+static void m_c3g_pp_ymask(const u32 *r, u32 *eax){ *eax = c3g_2a690_core(r, 128u); }
+static void m_c3g_pp_rec3c(const u32 *r, u32 *eax) { *eax = c3g_2a690_core(r, 1024u); }
+static void m_c3g_pp_bdc(const u32 *r, u32 *eax)  { *eax = c3g_2a690_core(r, 2048u); }
+static void m_c3g_pp_cursor(const u32 *r, u32 *eax) { *eax = c3g_2a690_core(r, 4096u); }
+
+/* 0x3BDDC's mutants. */
+static u32 c3g_3bddc_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 ctx[6];
+    fighter_ctx_same(ctx, side);
+    u32 self = ctx[2];
+    if ((DSB(self + 0x40u) & ((mut & 1u) ? 0x40u : 0x80u)) != 0) return 0;
+    u16 cmd = DSW(0x001088E0u + side * 2u);
+    if ((cmd & ((mut & 2u) ? 0x4000u : 0x8000u)) == 0) return 0;
+    u32 rec = DSD(0x001077B0u + side * 0x94u);
+    DSW(rec + 0x34u) = (mut & 4u) ? 0x1234u : 0u;
+    DSB(rec + 0x43u) = 0;
+    DSB(rec + 0x42u) = 0;
+    DSB(self + 0x5Fu) = (mut & 8u) ? 0u : 0xFFu;
+    if ((mut & 2048u) ? 1 : (DSB(0x00107803u + side * 0x94u) == 0)) {
+        if ((mut & 16u) ? (hit_chain_resolve(side) == 0) : (hit_chain_resolve(side) != 0))
+            return 1;
+    }
+    u32 sc = fighter_input_scan(side, 0, 5, (mut & 32u) ? 0x8000u : 0x4000u);
+    u32 base = ((mut & 4096u) ? !sc : (sc != 0u)) ? 0xBEF64u : 0xBEF28u;
+    u32 ch = DSB(self + 0x7Au);
+    DSD(0x00107D40 + side * 4u) = base + ch * ((mut & 64u) ? 4u : 6u);
+    hit_anim_start_a(DSD(self), DSD(((mut & 1024u) ? 0x000C8B34u : 0x000C8B30u) + ch * 4u),
+                     0x3F800000u);
+    DSB(self + 0x52u) = 3u;
+    DSB(self + 0x54u) = (mut & 128u) ? 3u : 2u;
+    DSB(self + 0x53u) = 4u;
+    DSB(0x001078F8 + side) = 1u;
+    if (cmd & 0x1000u) DSW(0x001077FEu + side * 0x94u) = (mut & 256u) ? 2u : 1u;
+    else if (cmd & 0x2000u) DSW(0x001077FEu + side * 0x94u) = 0xFFFFu;
+    else DSW(0x001077FEu + side * 0x94u) = 0u;
+    return (mut & 512u) ? 0u : 1u;
+}
+static void b_c3g_3bddc(const u32 *r, u32 *eax)
+{ *eax = (u32)fighter_attack_consume(r[R_EAX]); }
+static void m_c3g_att_o40(const u32 *r, u32 *eax) { *eax = c3g_3bddc_core(r, 1u); }
+static void m_c3g_att_cmd(const u32 *r, u32 *eax) { *eax = c3g_3bddc_core(r, 2u); }
+static void m_c3g_att_clr(const u32 *r, u32 *eax) { *eax = c3g_3bddc_core(r, 4u); }
+static void m_c3g_att_s5f(const u32 *r, u32 *eax) { *eax = c3g_3bddc_core(r, 8u); }
+static void m_c3g_att_gate(const u32 *r, u32 *eax){ *eax = c3g_3bddc_core(r, 2048u); }
+static void m_c3g_att_base(const u32 *r, u32 *eax){ *eax = c3g_3bddc_core(r, 4096u); }
+static void m_c3g_att_chain(const u32 *r, u32 *eax) { *eax = c3g_3bddc_core(r, 16u); }
+static void m_c3g_att_scan(const u32 *r, u32 *eax) { *eax = c3g_3bddc_core(r, 32u); }
+static void m_c3g_att_row(const u32 *r, u32 *eax) { *eax = c3g_3bddc_core(r, 64u); }
+static void m_c3g_att_anim(const u32 *r, u32 *eax) { *eax = c3g_3bddc_core(r, 1024u); }
+static void m_c3g_att_state(const u32 *r, u32 *eax) { *eax = c3g_3bddc_core(r, 128u); }
+static void m_c3g_att_dir(const u32 *r, u32 *eax) { *eax = c3g_3bddc_core(r, 256u); }
+static void m_c3g_att_ret(const u32 *r, u32 *eax) { *eax = c3g_3bddc_core(r, 512u); }
+
+/* 0x37178's mutants. */
+static u32 c3g_37178_core(const u32 *r, u32 mut)
+{
+    u32 slot = r[R_EAX];
+    u32 rec = DSD(slot);
+    u32 side = (u32)DSB(rec + 0x51u);
+    u32 other = 1u - side;
+    u32 s = 0x001077B0u + side * 0x94u;
+    u32 so = 0x001077B0u + other * 0x94u;
+    u32 rec_s = DSD(s);
+    u32 rec_o = DSD(so);
+    s32 base, x, diff;
+    int facing_s, facing_o;
+    DSB(0x001078FE) = 0;
+    DSD(s + 0x40u) &= 0xFFBFFFBFu;
+    DSB(s + 0x54u) = (mut & 2048u) ? 3u : 4u;
+    DSB(s + 0x40u) |= 0x40u;
+    DSB(so + 0x42u) |= 4u;
+    base = (s16)DSW(DSD(0x001078DC)
+                    + (u32)DSB(s + 0x7Au) * ((mut & 1u) ? 2u : 14u)
+                    + (u32)DSB(so + 0x7Au) * 2u);
+    if (base == -1) {
+        DSB(s + 0x52u) = 9u;
+        DSD(rec_s + 0x18u) = DSD(rec_s + 0x18u);
+        if (!(mut & 32u)) DSB(0x001078FE) = 1u;
+        fighter_36870(rec_s);
+        return 0;
+    }
+    if (fighter_actor_bit15_clear(other) != 0)
+        x = (mut & 2u) ? (s32)DSD(rec_o + 0x18u) + base : (s32)DSD(rec_o + 0x18u) - base;
+    else
+        x = (mut & 2u) ? (s32)DSD(rec_o + 0x18u) - base : (s32)DSD(rec_o + 0x18u) + base;
+    diff = (s32)DSD(rec_s + 0x18u) - x;
+    facing_s = (int)((DSW(rec_s + 0x28u) >> 8) & ((mut & 4u) ? 0x20u : 0x40u));
+    facing_o = (int)((DSW(rec_o + 0x28u) >> 8) & ((mut & 4u) ? 0x20u : 0x40u));
+    if (facing_s != facing_o) {
+        if (diff >= -0x400 && diff <= ((mut & 8u) ? 0x3FF : 0x400)) {
+            DSB(s + 0x52u) = (mut & 16u) ? 8u : 9u;
+            DSD(((mut & 64u) ? rec_o : rec_s) + 0x18u) = (u32)x;
+            if (!(mut & 32u)) DSB(0x001078FE) = 1u;
+            if (mut & 128u) fighter_state_36638(s, rec_s); else fighter_36870(rec_s);
+            return 0;
+        }
+        if (fighter_actor_bit15_clear(side) != 0) {
+            if (diff > 0) {
+                fighter_state_35838(s, rec_s, (mut & 256u) ? 0x1000u : 0x2000u);
+                DSB(s + 0x57u) = (mut & 512u) ? 1u : 0u;
+                return 0;
+            }
+            if (!(mut & 1024u)) DSB(s + 0x43u) |= 0x40u;
+            (void)fighter_state_36638(s, rec_s);
+            return 0;
+        }
+        if (diff < 0) {
+            fighter_state_35838(s, rec_s, (mut & 256u) ? 0x2000u : 0x1000u);
+            DSB(s + 0x57u) = (mut & 512u) ? 1u : 0u;
+            return 0;
+        }
+        if (!(mut & 1024u)) DSB(s + 0x43u) |= 0x40u;
+        (void)fighter_state_36638(s, rec_s);
+        return 0;
+    }
+    if (diff >= -0x400 && diff <= ((mut & 8u) ? 0x3FF : 0x400)) {
+        if (!(mut & 1024u)) DSB(s + 0x43u) |= 0x40u;
+        (void)fighter_state_36638(s, rec_s);
+        return 0;
+    }
+    if (fighter_actor_bit15_clear(side) != 0) {
+        if (diff > 0) {
+            fighter_state_35838(s, rec_s, (mut & 256u) ? 0x1000u : 0x2000u);
+            DSB(s + 0x57u) = (mut & 512u) ? 0u : 1u;
+            return 0;
+        }
+        if (!(mut & 1024u)) DSB(s + 0x43u) |= 0x40u;
+        (void)fighter_state_36638(s, rec_s);
+        return 0;
+    }
+    if (diff < 0) {
+        fighter_state_35838(s, rec_s, (mut & 256u) ? 0x2000u : 0x1000u);
+        DSB(s + 0x57u) = (mut & 512u) ? 0u : 1u;
+        return 0;
+    }
+    if (!(mut & 1024u)) DSB(s + 0x43u) |= 0x40u;
+    (void)fighter_state_36638(s, rec_s);
+    return 0;
+}
+static void b_c3g_37178(const u32 *r, u32 *eax)   { fighter_37178(r[R_EAX]); *eax = 0u; }
+static void m_c3g_178_base(const u32 *r, u32 *eax) { *eax = c3g_37178_core(r, 1u); }
+static void m_c3g_178_x(const u32 *r, u32 *eax)   { *eax = c3g_37178_core(r, 2u); }
+static void m_c3g_178_facing(const u32 *r, u32 *eax) { *eax = c3g_37178_core(r, 4u); }
+static void m_c3g_178_bound(const u32 *r, u32 *eax){ *eax = c3g_37178_core(r, 8u); }
+static void m_c3g_178_s52(const u32 *r, u32 *eax) { *eax = c3g_37178_core(r, 16u); }
+static void m_c3g_178_fe(const u32 *r, u32 *eax)  { *eax = c3g_37178_core(r, 32u); }
+static void m_c3g_178_s18(const u32 *r, u32 *eax) { *eax = c3g_37178_core(r, 64u); }
+static void m_c3g_178_call(const u32 *r, u32 *eax) { *eax = c3g_37178_core(r, 128u); }
+static void m_c3g_178_dirs(const u32 *r, u32 *eax) { *eax = c3g_37178_core(r, 256u); }
+static void m_c3g_178_s57(const u32 *r, u32 *eax) { *eax = c3g_37178_core(r, 512u); }
+static void m_c3g_178_s43(const u32 *r, u32 *eax) { *eax = c3g_37178_core(r, 1024u); }
+static void m_c3g_178_s54(const u32 *r, u32 *eax) { *eax = c3g_37178_core(r, 2048u); }
+
+/* 0x3CCEC's mutants. */
+static u32 c3g_3ccec_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX], i = r[R_EDX];
+    u32 slot = 0x001077B0u + side * 0x94u;
+    u32 ch = (u32)DSB(slot + ((mut & 32u) ? 0x7Bu : 0x7Au));
+    u32 entry = (mut & 16u) ? (ch + i * 0x20u) : (ch * 0x20u + i);
+    u32 st = (u32)DSB(slot + 0x54u);
+    u8 e = DSB(0x000C619Cu + entry * 8u + 7u);
+    u8 d = DSB(0x000C619Cu + entry * 8u + 6u);
+    u8 first = (mut & 1u) ? d : e;
+    u8 second = (mut & 4u) ? e : d;
+    if (first != 0u && st == ((mut & 2u) ? 3u : 2u)) return 1u;
+    if (second != 0u && ((mut & 8u) ? (st == 1u) : (st == 1u || st == 0u))) return 1u;
+    return (mut & 64u) ? 1u : 0u;
+}
+static void b_c3g_3ccec(const u32 *r, u32 *eax) { *eax = (u32)hit_stance_ok(r[R_EAX], r[R_EDX]); }
+static void m_c3g_st_e(const u32 *r, u32 *eax)     { *eax = c3g_3ccec_core(r, 1u); }
+static void m_c3g_st_st2(const u32 *r, u32 *eax)   { *eax = c3g_3ccec_core(r, 2u); }
+static void m_c3g_st_d(const u32 *r, u32 *eax)     { *eax = c3g_3ccec_core(r, 4u); }
+static void m_c3g_st_lo(const u32 *r, u32 *eax)    { *eax = c3g_3ccec_core(r, 8u); }
+static void m_c3g_st_entry(const u32 *r, u32 *eax) { *eax = c3g_3ccec_core(r, 16u); }
+static void m_c3g_st_char(const u32 *r, u32 *eax)  { *eax = c3g_3ccec_core(r, 32u); }
+static void m_c3g_st_ret(const u32 *r, u32 *eax)   { *eax = c3g_3ccec_core(r, 64u); }
+
+/* 0x3CE24's mutants. */
+static u32 c3g_3ce24_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX], i = r[R_EDX];
+    u32 slot = 0x001077B0u + side * 0x94u;
+    s32 b = (mut & 2u) ? (s32)(u8)DSB(slot + 0x56u) : (s8)DSB(slot + 0x56u);
+    if (mut & 4u) b = (s8)DSB(slot + 0x57u);
+    if (b > ((mut & 1u) ? 6 : 5)) return (mut & 16u) ? 1u : 0u;
+    if (mut & 8u) return 1u;
+    {
+        u32 got = (u32)hit_immunity(side, i);
+        return (mut & 32u) ? got : (((u8)got != 0u) ? 1u : 0u);
+    }
+}
+static void b_c3g_3ce24(const u32 *r, u32 *eax) { *eax = (u32)hit_gate(r[R_EAX], r[R_EDX]); }
+static void m_c3g_gate_bound(const u32 *r, u32 *eax)  { *eax = c3g_3ce24_core(r, 1u); }
+static void m_c3g_gate_signed(const u32 *r, u32 *eax) { *eax = c3g_3ce24_core(r, 2u); }
+static void m_c3g_gate_off(const u32 *r, u32 *eax)    { *eax = c3g_3ce24_core(r, 4u); }
+static void m_c3g_gate_ret(const u32 *r, u32 *eax)    { *eax = c3g_3ce24_core(r, 16u); }
+static void m_c3g_gate_call(const u32 *r, u32 *eax)   { *eax = c3g_3ce24_core(r, 8u); }
+static void m_c3g_gate_al(const u32 *r, u32 *eax)     { *eax = c3g_3ce24_core(r, 32u); }
+
+/* 0x3C600's mutants. */
+static u32 c3g_3c600_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX], i = r[R_EDX];
+    u32 slot = 0x001077B0u + side * 0x94u;
+    u32 sel;
+    if (!(mut & 1u) && DSB(slot + 0x63u) != 0u) {           /* @s63: ignore the +0x63 force */
+        sel = 2u;
+    } else {
+        u32 w = (u32)DSW(DSD(0x00101514u) + 0x2D4u + ((mut & 2u) ? 0u : side * 2u));
+        sel = (mut & 256u) ? (w | 0x10000u) : w;            /* @mask: the raw's `and 0xffff` */
+    }
+    if (sel > ((mut & 4u) ? 5u : 6u))                       /* @gt */
+        return (mut & 64u) ? 0x1234u : r[R_ECX];            /* @def */
+    {
+        u32 ch = (u32)DSB(slot + ((mut & 32u) ? 0x7Bu : 0x7Au));
+        u32 entry = (mut & 16u) ? (ch + i * 0x20u) : (ch * 0x20u + i);
+        switch (sel) {
+        case 0u: case 4u: case 6u:
+            return DSD(((mut & 8u) ? 0x000C619Cu : 0x000C6B9Cu) + entry * 8u);
+        case 2u:
+            return DSD(((mut & 8u) ? 0x000C6B9Cu : 0x000C619Cu) + entry * 8u);
+        default:
+            return r[R_ECX];
+        }
+    }
+}
+static void b_c3g_3c600(const u32 *r, u32 *eax) { *eax = hit_frame_desc(r[R_EAX], r[R_EDX]); }
+static void m_c3g_desc_s63(const u32 *r, u32 *eax)   { *eax = c3g_3c600_core(r, 1u); }
+static void m_c3g_desc_side(const u32 *r, u32 *eax)  { *eax = c3g_3c600_core(r, 2u); }
+static void m_c3g_desc_gt(const u32 *r, u32 *eax)    { *eax = c3g_3c600_core(r, 4u); }
+static void m_c3g_desc_table(const u32 *r, u32 *eax) { *eax = c3g_3c600_core(r, 8u); }
+static void m_c3g_desc_entry(const u32 *r, u32 *eax) { *eax = c3g_3c600_core(r, 16u); }
+static void m_c3g_desc_char(const u32 *r, u32 *eax)  { *eax = c3g_3c600_core(r, 32u); }
+static void m_c3g_desc_def(const u32 *r, u32 *eax)   { *eax = c3g_3c600_core(r, 64u); }
+static void m_c3g_desc_mask(const u32 *r, u32 *eax)  { *eax = c3g_3c600_core(r, 256u); }
+
+/* 0x3CBC4/0x3CC58's mutants. */
+static u32 c3g_react_core(const u32 *r, u32 mut, u32 v_crouch, u32 v_cmd, u32 v_hit, u32 v_miss,
+                          u32 tbl2, u32 tbl_wrong)
+{
+    u32 side = r[R_EAX];
+    u32 slot = 0x001077B0u + side * 0x94u;
+    u32 st = (u32)DSB(slot + 0x54u);
+    if ((mut & 1u) ? (st == 3u) : (st == 2u)) return v_crouch;              /* @st */
+    if ((DSW(0x001088E0u + side * 2u) & ((mut & 2u) ? 0x8000u : 0x4000u)) != 0u)
+        return v_cmd;                                                       /* @bit */
+    {
+        u32 a = (mut & 4u) ? side : (1u - side);                            /* @side */
+        u32 tbl = DSD(((mut & 8u) ? tbl_wrong : tbl2)
+                      + (u32)DSB(slot + 0x7Au) * 4u);
+        u32 idx = (mut & 16u) ? 0x000A7ACCu : 0x000A7A70u;                  /* @idx */
+        if (mut & 64u) return (mut & 32u) ? v_miss : v_hit;                 /* @geom */
+        return (hit_geometry(a, tbl, idx) != 0u)
+             ? ((mut & 32u) ? v_miss : v_hit)                               /* @ret */
+             : ((mut & 32u) ? v_hit : v_miss);
+    }
+}
+static void b_c3g_3cbc4(const u32 *r, u32 *eax) { *eax = (u32)hit_reaction_a(r[R_EAX]); }
+static void b_c3g_3cc58(const u32 *r, u32 *eax) { *eax = (u32)hit_reaction_b(r[R_EAX]); }
+static void m_c3g_ra_st(const u32 *r, u32 *eax)   { *eax = c3g_react_core(r, 1u, 0x16u, 0x14u, 0x12u, 0x10u, 0x000A7B44u, 0x000A7ACCu); }
+static void m_c3g_ra_bit(const u32 *r, u32 *eax)  { *eax = c3g_react_core(r, 2u, 0x16u, 0x14u, 0x12u, 0x10u, 0x000A7B44u, 0x000A7ACCu); }
+static void m_c3g_ra_side(const u32 *r, u32 *eax) { *eax = c3g_react_core(r, 4u, 0x16u, 0x14u, 0x12u, 0x10u, 0x000A7B44u, 0x000A7ACCu); }
+static void m_c3g_ra_table(const u32 *r, u32 *eax){ *eax = c3g_react_core(r, 8u, 0x16u, 0x14u, 0x12u, 0x10u, 0x000A7B44u, 0x000A7ACCu); }
+static void m_c3g_ra_idx(const u32 *r, u32 *eax)  { *eax = c3g_react_core(r, 16u, 0x16u, 0x14u, 0x12u, 0x10u, 0x000A7B44u, 0x000A7ACCu); }
+static void m_c3g_ra_ret(const u32 *r, u32 *eax)  { *eax = c3g_react_core(r, 32u, 0x16u, 0x14u, 0x12u, 0x10u, 0x000A7B44u, 0x000A7ACCu); }
+static void m_c3g_ra_geom(const u32 *r, u32 *eax) { *eax = c3g_react_core(r, 64u, 0x16u, 0x14u, 0x12u, 0x10u, 0x000A7B44u, 0x000A7ACCu); }
+static void m_c3g_rb_st(const u32 *r, u32 *eax)   { *eax = c3g_react_core(r, 1u, 0x17u, 0x15u, 0x13u, 0x11u, 0x000A7ACCu, 0x000A7B44u); }
+static void m_c3g_rb_bit(const u32 *r, u32 *eax)  { *eax = c3g_react_core(r, 2u, 0x17u, 0x15u, 0x13u, 0x11u, 0x000A7ACCu, 0x000A7B44u); }
+static void m_c3g_rb_side(const u32 *r, u32 *eax) { *eax = c3g_react_core(r, 4u, 0x17u, 0x15u, 0x13u, 0x11u, 0x000A7ACCu, 0x000A7B44u); }
+static void m_c3g_rb_table(const u32 *r, u32 *eax){ *eax = c3g_react_core(r, 8u, 0x17u, 0x15u, 0x13u, 0x11u, 0x000A7ACCu, 0x000A7B44u); }
+static void m_c3g_rb_idx(const u32 *r, u32 *eax)  { *eax = c3g_react_core(r, 16u, 0x17u, 0x15u, 0x13u, 0x11u, 0x000A7ACCu, 0x000A7B44u); }
+static void m_c3g_rb_ret(const u32 *r, u32 *eax)  { *eax = c3g_react_core(r, 32u, 0x17u, 0x15u, 0x13u, 0x11u, 0x000A7ACCu, 0x000A7B44u); }
+static void m_c3g_rb_geom(const u32 *r, u32 *eax) { *eax = c3g_react_core(r, 64u, 0x17u, 0x15u, 0x13u, 0x11u, 0x000A7ACCu, 0x000A7B44u); }
+
+/* 0x4CE70's mutants. */
+static u32 c3g_4ce70_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX], reaction = r[R_EDX];
+    u32 slot = 0x001077B0u + ((mut & 1u) ? 0u : side * 0x94u);
+    u32 ch = (u32)DSB(slot + ((mut & 2u) ? 0x7Bu : 0x7Au));
+    if (mut & 4u) ch &= 0xFEu;
+    if (mut & 8u) reaction += 1u;
+    if (ch < 7u) {
+        int blocked;
+        switch (ch) {
+        case 0u: blocked = (reaction == 0x28u || reaction == 0x29u || reaction == 0x2Au
+                            || ((mut & 16u) ? 0 : (reaction == 0x2Cu))); break;
+        case 1u: blocked = (reaction == 0x22u || reaction == 0x23u || reaction == 0x24u); break;
+        case 2u: blocked = (reaction == 0x22u || reaction == 0x24u
+                            || ((mut & 32u) ? 0 : (reaction == 0x26u)) || reaction == 0x27u); break;
+        case 3u: blocked = (reaction == 0x22u || reaction == 0x23u); break;
+        case 4u: blocked = (reaction == 0x24u || reaction == 0x26u); break;
+        case 5u: blocked = (reaction == 0x21u || reaction == 0x22u); break;
+        default: blocked = (reaction == 0x21u || reaction == 0x24u); break;
+        }
+        if (mut & 64u) blocked = !blocked;
+        return blocked ? 0u : 1u;
+    }
+    return (mut & 128u) ? 0u : 1u;
+}
+static void b_c3g_4ce70(const u32 *r, u32 *eax) { *eax = (u32)hit_reaction_allow(r[R_EAX], r[R_EDX]); }
+static void m_c3g_allow_side(const u32 *r, u32 *eax)  { *eax = c3g_4ce70_core(r, 1u); }
+static void m_c3g_allow_char(const u32 *r, u32 *eax)  { *eax = c3g_4ce70_core(r, 2u); }
+static void m_c3g_allow_off(const u32 *r, u32 *eax)   { *eax = c3g_4ce70_core(r, 4u); }
+static void m_c3g_allow_shift(const u32 *r, u32 *eax) { *eax = c3g_4ce70_core(r, 8u); }
+static void m_c3g_allow_d0(const u32 *r, u32 *eax)    { *eax = c3g_4ce70_core(r, 16u); }
+static void m_c3g_allow_d2(const u32 *r, u32 *eax)    { *eax = c3g_4ce70_core(r, 32u); }
+static void m_c3g_allow_inv(const u32 *r, u32 *eax)   { *eax = c3g_4ce70_core(r, 64u); }
+static void m_c3g_allow_z(const u32 *r, u32 *eax)     { *eax = c3g_4ce70_core(r, 128u); }
+
+/* 0x38C5C's mutants. */
+static u32 c3g_38c5c_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    s32 c = (mut & 1u) ? (s32)side * 0x94 : (s32)side * 0x25;   /* @c */
+    text_cells_release((mut & 2u) ? c : c + 2, 8,
+                       mem + ((mut & 4u) ? 0x00080BE4u : 0x00080BE0u),
+                       (mut & 512u) ? 0x1000u : 0x2000u);
+    text_cells_release_vertical((mut & 16u) ? c : c + 2, (mut & 8u) ? 8 : 9,
+                                mem + ((mut & 16u) ? 0x00080BE0u : 0x00080BE4u));
+    text_cells_release(c, (mut & 32u) ? 0x0Fu : 0x10u,
+                       mem + ((mut & 64u) ? 0x00080BE0u : 0x00080BECu),
+                       (mut & 512u) ? 0x1000u : 0x2000u);
+    if (!(mut & 256u))
+        text_cells_release((mut & 128u) ? c + 2 : c + 3, 0x10u,
+                           mem + 0x00080BECu, (mut & 512u) ? 0x1000u : 0x2000u);
+    return 0;
+}
+static void b_c3g_38c5c(const u32 *r, u32 *eax) { fighter_38c5c(r[R_EAX]); *eax = 0u; }
+static void m_c3g_c5c_c(const u32 *r, u32 *eax)   { *eax = c3g_38c5c_core(r, 1u); }
+static void m_c3g_c5c_two(const u32 *r, u32 *eax) { *eax = c3g_38c5c_core(r, 2u); }
+static void m_c3g_c5c_s2(const u32 *r, u32 *eax)  { *eax = c3g_38c5c_core(r, 4u); }
+static void m_c3g_c5c_r9(const u32 *r, u32 *eax)  { *eax = c3g_38c5c_core(r, 8u); }
+static void m_c3g_c5c_s6(const u32 *r, u32 *eax)  { *eax = c3g_38c5c_core(r, 16u); }
+static void m_c3g_c5c_r10(const u32 *r, u32 *eax) { *eax = c3g_38c5c_core(r, 32u); }
+static void m_c3g_c5c_s3(const u32 *r, u32 *eax)  { *eax = c3g_38c5c_core(r, 64u); }
+static void m_c3g_c5c_col4(const u32 *r, u32 *eax) { *eax = c3g_38c5c_core(r, 128u); }
+static void m_c3g_c5c_n4(const u32 *r, u32 *eax)  { *eax = c3g_38c5c_core(r, 256u); }
+static void m_c3g_c5c_mode(const u32 *r, u32 *eax) { *eax = c3g_38c5c_core(r, 512u); }
+
+/* 0x2F4BC's mutants. */
+static u32 c3g_2f4bc_core(const u32 *r, u32 mut)
+{
+    u32 save = DSD(0x00105F34u);
+    text_cursor_set((s32)r[R_EAX],
+                    (mut & 4u) ? (s32)r[R_EAX] : (s32)r[R_EDX],
+                    (mut & 2u) ? mem + 0x0010A000u : mem + (r[R_EBX] & 0x3FFFFFFFu),
+                    (mut & 8u) ? 0x2000u : r[R_ECX]);
+    if (!(mut & 1u)) DSD(0x00105F34u) = save;
+    return 0;
+}
+static void b_c3g_2f4bc(const u32 *r, u32 *eax) { text_cursor_hold(r[R_EAX], r[R_EDX], mem + (r[R_EBX] & 0x3FFFFFFFu), r[R_ECX]); *eax = 0u; }
+static void m_c3g_hold_save(const u32 *r, u32 *eax) { *eax = c3g_2f4bc_core(r, 1u); }
+static void m_c3g_hold_str(const u32 *r, u32 *eax)  { *eax = c3g_2f4bc_core(r, 2u); }
+static void m_c3g_hold_args(const u32 *r, u32 *eax) { *eax = c3g_2f4bc_core(r, 4u); }
+static void m_c3g_hold_mode(const u32 *r, u32 *eax) { *eax = c3g_2f4bc_core(r, 8u); }
+
+/* 0x2F4D0's mutants. */
+static u32 c3g_2f4d0_core(const u32 *r, u32 mut)
+{
+    s32 col = (mut & 64u) ? (s32)r[R_EDX] : (s32)r[R_EAX];
+    s32 row = (mut & 128u) ? (s32)r[R_EAX] : (s32)r[R_EDX];
+    u32 save = DSD(0x00105F34u);
+    u8 buf[0x14] = {0};
+    u32 value = (mut & 1u) ? r[R_ECX] : r[R_EBX];
+    u32 pad = (mut & 2u) ? r[R_S1] : r[R_S0];
+    if (!(mut & 32u))
+        (void)text_number_format((s32)value, buf, (s32)r[R_ECX], pad);
+    text_cursor_set(col, row, buf, (mut & 4u) ? 0x1000u : r[R_S1]);
+    if (!(mut & 8u)) DSD(0x00105F34u) = save;
+    return 0;
+}
+static void b_c3g_2f4d0(const u32 *r, u32 *eax)
+{ text_number_draw((s32)r[R_EAX], (s32)r[R_EDX], (s32)r[R_EBX], (s32)r[R_ECX], r[R_S0], r[R_S1]); *eax = 0u; }
+static void m_c3g_draw_val(const u32 *r, u32 *eax)  { *eax = c3g_2f4d0_core(r, 1u); }
+static void m_c3g_draw_pad(const u32 *r, u32 *eax)  { *eax = c3g_2f4d0_core(r, 2u); }
+static void m_c3g_draw_mode(const u32 *r, u32 *eax) { *eax = c3g_2f4d0_core(r, 4u); }
+static void m_c3g_draw_save(const u32 *r, u32 *eax) { *eax = c3g_2f4d0_core(r, 8u); }
+static void m_c3g_draw_fmt(const u32 *r, u32 *eax)  { *eax = c3g_2f4d0_core(r, 32u); }
+static void m_c3g_draw_col(const u32 *r, u32 *eax)  { *eax = c3g_2f4d0_core(r, 64u); }
+static void m_c3g_draw_row(const u32 *r, u32 *eax)  { *eax = c3g_2f4d0_core(r, 128u); }
+
+/* 0x2F20C's mutants. */
+static u32 c3g_2f20c_core(const u32 *r, u32 mut)
+{
+    s32 col = (s32)r[R_EAX], row = (s32)r[R_EDX];
+    if (row == -1) {
+        if (mut & 256u) {                                   /* @sext: no sign extension */
+            col = (s32)(u16)DSW(0x00105F36u);
+            row = (s32)(u16)DSW(0x00105F34u);
+        } else if (mut & 1u) {                              /* @reload: the words swapped */
+            col = (s16)DSW(0x00105F34u);
+            row = (s16)DSW(0x00105F36u);
+        } else {
+            col = (s16)DSW(0x00105F36u);
+            row = (s16)DSW(0x00105F34u);
+        }
+    } else if (col == -1) {
+        s32 w = (mut & 8u) ? 0 : text_width(mem + r[R_EBX],
+                                            (mut & 4u) ? r[R_EAX] : r[R_ECX]);
+        col = (0x2b - w) >> 1;
+        if (col < 0 && !(mut & 2u)) col = 0;
+    }
+    {
+        s32 ext = (s32)text_render(mem + r[R_EBX], (mut & 32u) ? 0x1000u : r[R_ECX],
+                                   row, col, (mut & 16u) ? 0u : 1u);
+        DSW(0x00105F34u) = (u16)((mut & 64u) ? col : row);
+        DSW(0x00105F36u) = (u16)((mut & 128u) ? col : col + ext);
+    }
+    return 0;
+}
+static void b_c3g_2f20c(const u32 *r, u32 *eax)
+{ text_vertical_set((s32)r[R_EAX], (s32)r[R_EDX], mem + (r[R_EBX] & 0x3FFFFFFFu), r[R_ECX]); *eax = 0u; }
+static void m_c3g_vert_reload(const u32 *r, u32 *eax) { *eax = c3g_2f20c_core(r, 1u); }
+static void m_c3g_vert_neg(const u32 *r, u32 *eax)    { *eax = c3g_2f20c_core(r, 2u); }
+static void m_c3g_vert_wargs(const u32 *r, u32 *eax)  { *eax = c3g_2f20c_core(r, 4u); }
+static void m_c3g_vert_skip(const u32 *r, u32 *eax)   { *eax = c3g_2f20c_core(r, 8u); }
+static void m_c3g_vert_v(const u32 *r, u32 *eax)      { *eax = c3g_2f20c_core(r, 16u); }
+static void m_c3g_vert_mode(const u32 *r, u32 *eax)   { *eax = c3g_2f20c_core(r, 32u); }
+static void m_c3g_vert_row(const u32 *r, u32 *eax)    { *eax = c3g_2f20c_core(r, 64u); }
+static void m_c3g_vert_ext(const u32 *r, u32 *eax)    { *eax = c3g_2f20c_core(r, 128u); }
+static void m_c3g_vert_sext(const u32 *r, u32 *eax)   { *eax = c3g_2f20c_core(r, 256u); }
+
 static const binding_t k_bindings[] = {
     { "rng_next",                 b_rng_next,     0xFFFFFFFFu },
     { "fighter_slot_flag",        b_slot_flag,    0x000000FFu },
@@ -13733,6 +14401,180 @@ static const binding_t k_bindings[] = {
     { "hit_chain_resolve@s55",             m_c3f_cf_s55,   0x000000FFu },
     { "hit_chain_resolve@inc",             m_c3f_cf_inc,   0x000000FFu },
     { "hit_chain_resolve@ret",             m_c3f_cf_ret,   0x000000FFu },
+    { "hit_slot_seed",                     b_c3g_3c6a8,    0x00000000u },
+    { "hit_slot_seed@phase",               m_c3g_seed_phase, 0x00000000u },
+    { "hit_slot_seed@clear",               m_c3g_seed_clear, 0x00000000u },
+    { "hit_slot_seed@off",                 m_c3g_seed_off,  0x00000000u },
+    { "hit_slot_seed@field",               m_c3g_seed_field, 0x00000000u },
+    { "hit_slot_seed@scale",               m_c3g_seed_scale, 0x00000000u },
+    { "hit_slot_seed@bytes",               m_c3g_seed_bytes, 0x00000000u },
+    { "hit_scan",                          b_c3g_3cd44,     0xFFFFFFFFu },
+    { "hit_scan@bound",                    m_c3g_scan_bound, 0xFFFFFFFFu },
+    { "hit_scan@side",                     m_c3g_scan_side, 0xFFFFFFFFu },
+    { "hit_scan@stride",                   m_c3g_scan_stride, 0xFFFFFFFFu },
+    { "hit_scan@phase",                    m_c3g_scan_phase, 0xFFFFFFFFu },
+    { "hit_scan@al",                       m_c3g_scan_al,   0xFFFFFFFFu },
+    { "hit_scan@miss",                     m_c3g_scan_miss, 0xFFFFFFFFu },
+    { "fighter_38fec",                     b_c3g_38fec,     0x00000000u },
+    { "fighter_38fec@ch",                  m_c3g_fec_ch,    0x00000000u },
+    { "fighter_38fec@n",                   m_c3g_fec_n,     0x00000000u },
+    { "fighter_38fec@table",               m_c3g_fec_table, 0x00000000u },
+    { "fighter_38fec@stride",              m_c3g_fec_stride, 0x00000000u },
+    { "fighter_38fec@call",                m_c3g_fec_call,  0x00000000u },
+    { "hit_reaction_drive",                b_c3g_3ce58,     0x000000FFu },
+    { "hit_reaction_drive@gate",           m_c3g_drv_gate,  0x000000FFu },
+    { "hit_reaction_drive@gateal",         m_c3g_drv_gateal, 0x000000FFu },
+    { "hit_reaction_drive@bound",          m_c3g_drv_bound, 0x000000FFu },
+    { "hit_reaction_drive@mode",           m_c3g_drv_mode,  0x000000FFu },
+    { "hit_reaction_drive@allow",          m_c3g_drv_allow, 0x000000FFu },
+    { "hit_reaction_drive@flash",          m_c3g_drv_flash, 0x000000FFu },
+    { "hit_reaction_drive@sel",            m_c3g_drv_sel,   0x000000FFu },
+    { "hit_reaction_drive@hi",             m_c3g_drv_hi,    0x000000FFu },
+    { "hit_reaction_drive@store",          m_c3g_drv_store, 0x000000FFu },
+    { "hit_reaction_drive@apply",          m_c3g_drv_apply, 0x000000FFu },
+    { "hit_reaction_drive@ret",            m_c3g_drv_ret,   0x000000FFu },
+    { "fighter_38d90",                     b_c3g_38d90,     0x00000000u },
+    { "fighter_38d90@c5c",                 m_c3g_90_c5c,    0x00000000u },
+    { "fighter_38d90@row",                 m_c3g_90_row,    0x00000000u },
+    { "fighter_38d90@id",                  m_c3g_90_id,     0x00000000u },
+    { "fighter_38d90@mode",                m_c3g_90_mode,   0x00000000u },
+    { "fighter_38d90@col",                 m_c3g_90_col,    0x00000000u },
+    { "fighter_38d90@pctrow",              m_c3g_90_pctrow, 0x00000000u },
+    { "fighter_38d90@pct",                 m_c3g_90_pct,    0x00000000u },
+    { "fighter_block_start",               b_c3g_1a7cc,     0x00000000u },
+    { "fighter_block_start@and",           m_c3g_bs_and,    0x00000000u },
+    { "fighter_block_start@s61",           m_c3g_bs_s61,    0x00000000u },
+    { "fighter_block_start@s60",           m_c3g_bs_s60,    0x00000000u },
+    { "fighter_block_start@s62",           m_c3g_bs_s62,    0x00000000u },
+    { "fighter_block_start@bound",         m_c3g_bs_bound,  0x00000000u },
+    { "fighter_block_start@tri",           m_c3g_bs_tri,    0x00000000u },
+    { "fighter_block_start@k",             m_c3g_bs_k,      0x00000000u },
+    { "fighter_block_start@div",           m_c3g_bs_div,    0x00000000u },
+    { "fighter_block_start@tbl",           m_c3g_bs_tbl,    0x00000000u },
+    { "fighter_block_start@s52",           m_c3g_bs_s52,    0x00000000u },
+    { "fighter_block_start@s53",           m_c3g_bs_s53,    0x00000000u },
+    { "actor_pset_point",                  b_c3g_2a690,     0x00000000u },
+    { "actor_pset_point@bit",              m_c3g_pp_bit,    0x00000000u },
+    { "actor_pset_point@x0",               m_c3g_pp_x0,     0x00000000u },
+    { "actor_pset_point@x2",               m_c3g_pp_x2,     0x00000000u },
+    { "actor_pset_point@gte",              m_c3g_pp_gte,    0x00000000u },
+    { "actor_pset_point@ramp",             m_c3g_pp_ramp,   0x00000000u },
+    { "actor_pset_point@x44",              m_c3g_pp_x44,    0x00000000u },
+    { "actor_pset_point@div",              m_c3g_pp_div,    0x00000000u },
+    { "actor_pset_point@ymask",            m_c3g_pp_ymask,  0x00000000u },
+    { "actor_pset_point@rec3c",            m_c3g_pp_rec3c,  0x00000000u },
+    { "actor_pset_point@bdc",              m_c3g_pp_bdc,    0x00000000u },
+    { "actor_pset_point@cursor",           m_c3g_pp_cursor, 0x00000000u },
+    { "fighter_attack_consume",            b_c3g_3bddc,     0x000000FFu },
+    { "fighter_attack_consume@o40",        m_c3g_att_o40,   0x000000FFu },
+    { "fighter_attack_consume@cmd",        m_c3g_att_cmd,   0x000000FFu },
+    { "fighter_attack_consume@clr",        m_c3g_att_clr,   0x000000FFu },
+    { "fighter_attack_consume@s5f",        m_c3g_att_s5f,   0x000000FFu },
+    { "fighter_attack_consume@gate",       m_c3g_att_gate,  0x000000FFu },
+    { "fighter_attack_consume@chain",      m_c3g_att_chain, 0x000000FFu },
+    { "fighter_attack_consume@scan",       m_c3g_att_scan,  0x000000FFu },
+    { "fighter_attack_consume@base",       m_c3g_att_base,  0x000000FFu },
+    { "fighter_attack_consume@row",        m_c3g_att_row,   0x000000FFu },
+    { "fighter_attack_consume@anim",       m_c3g_att_anim,  0x000000FFu },
+    { "fighter_attack_consume@state",      m_c3g_att_state, 0x000000FFu },
+    { "fighter_attack_consume@dir",        m_c3g_att_dir,   0x000000FFu },
+    { "fighter_attack_consume@ret",        m_c3g_att_ret,   0x000000FFu },
+    { "fighter_37178",                     b_c3g_37178,     0x00000000u },
+    { "fighter_37178@base",                m_c3g_178_base,  0x00000000u },
+    { "fighter_37178@x",                   m_c3g_178_x,     0x00000000u },
+    { "fighter_37178@facing",              m_c3g_178_facing, 0x00000000u },
+    { "fighter_37178@bound",               m_c3g_178_bound, 0x00000000u },
+    { "fighter_37178@s52",                 m_c3g_178_s52,   0x00000000u },
+    { "fighter_37178@fe",                  m_c3g_178_fe,    0x00000000u },
+    { "fighter_37178@s18",                 m_c3g_178_s18,   0x00000000u },
+    { "fighter_37178@call",                m_c3g_178_call,  0x00000000u },
+    { "fighter_37178@dirs",                m_c3g_178_dirs,  0x00000000u },
+    { "fighter_37178@s57",                 m_c3g_178_s57,   0x00000000u },
+    { "fighter_37178@s43",                 m_c3g_178_s43,   0x00000000u },
+    { "fighter_37178@s54",                 m_c3g_178_s54,   0x00000000u },
+    { "hit_stance_ok",                     b_c3g_3ccec,     0xFFFFFFFFu },
+    { "hit_stance_ok@e",                   m_c3g_st_e,      0xFFFFFFFFu },
+    { "hit_stance_ok@st2",                 m_c3g_st_st2,    0xFFFFFFFFu },
+    { "hit_stance_ok@d",                   m_c3g_st_d,      0xFFFFFFFFu },
+    { "hit_stance_ok@lo",                  m_c3g_st_lo,     0xFFFFFFFFu },
+    { "hit_stance_ok@entry",               m_c3g_st_entry,  0xFFFFFFFFu },
+    { "hit_stance_ok@char",                m_c3g_st_char,   0xFFFFFFFFu },
+    { "hit_stance_ok@ret",                 m_c3g_st_ret,    0xFFFFFFFFu },
+    { "hit_gate",                          b_c3g_3ce24,     0x000000FFu },
+    { "hit_gate@bound",                    m_c3g_gate_bound, 0x000000FFu },
+    { "hit_gate@signed",                   m_c3g_gate_signed, 0x000000FFu },
+    { "hit_gate@off",                      m_c3g_gate_off,  0x000000FFu },
+    { "hit_gate@ret",                      m_c3g_gate_ret,  0x000000FFu },
+    { "hit_gate@call",                     m_c3g_gate_call, 0x000000FFu },
+    { "hit_gate@al",                       m_c3g_gate_al,   0x000000FFu },
+    { "hit_frame_desc",                    b_c3g_3c600,     0xFFFFFFFFu },
+    { "hit_frame_desc@s63",                m_c3g_desc_s63,  0xFFFFFFFFu },
+    { "hit_frame_desc@side",               m_c3g_desc_side, 0xFFFFFFFFu },
+    { "hit_frame_desc@gt",                 m_c3g_desc_gt,   0xFFFFFFFFu },
+    { "hit_frame_desc@table",              m_c3g_desc_table, 0xFFFFFFFFu },
+    { "hit_frame_desc@entry",              m_c3g_desc_entry, 0xFFFFFFFFu },
+    { "hit_frame_desc@char",               m_c3g_desc_char, 0xFFFFFFFFu },
+    { "hit_frame_desc@def",                m_c3g_desc_def,  0xFFFFFFFFu },
+    { "hit_frame_desc@mask",               m_c3g_desc_mask, 0xFFFFFFFFu },
+    { "hit_reaction_a",                    b_c3g_3cbc4,     0xFFFFFFFFu },
+    { "hit_reaction_a@st",                 m_c3g_ra_st,     0xFFFFFFFFu },
+    { "hit_reaction_a@bit",                m_c3g_ra_bit,    0xFFFFFFFFu },
+    { "hit_reaction_a@side",               m_c3g_ra_side,   0xFFFFFFFFu },
+    { "hit_reaction_a@table",              m_c3g_ra_table,  0xFFFFFFFFu },
+    { "hit_reaction_a@idx",                m_c3g_ra_idx,    0xFFFFFFFFu },
+    { "hit_reaction_a@ret",                m_c3g_ra_ret,    0xFFFFFFFFu },
+    { "hit_reaction_a@geom",               m_c3g_ra_geom,   0xFFFFFFFFu },
+    { "hit_reaction_b",                    b_c3g_3cc58,     0xFFFFFFFFu },
+    { "hit_reaction_b@st",                 m_c3g_rb_st,     0xFFFFFFFFu },
+    { "hit_reaction_b@bit",                m_c3g_rb_bit,    0xFFFFFFFFu },
+    { "hit_reaction_b@side",               m_c3g_rb_side,   0xFFFFFFFFu },
+    { "hit_reaction_b@table",              m_c3g_rb_table,  0xFFFFFFFFu },
+    { "hit_reaction_b@idx",                m_c3g_rb_idx,    0xFFFFFFFFu },
+    { "hit_reaction_b@ret",                m_c3g_rb_ret,    0xFFFFFFFFu },
+    { "hit_reaction_b@geom",               m_c3g_rb_geom,   0xFFFFFFFFu },
+    { "hit_reaction_allow",                b_c3g_4ce70,     0xFFFFFFFFu },
+    { "hit_reaction_allow@side",           m_c3g_allow_side, 0xFFFFFFFFu },
+    { "hit_reaction_allow@char",           m_c3g_allow_char, 0xFFFFFFFFu },
+    { "hit_reaction_allow@off",            m_c3g_allow_off,  0xFFFFFFFFu },
+    { "hit_reaction_allow@shift",          m_c3g_allow_shift, 0xFFFFFFFFu },
+    { "hit_reaction_allow@d0",             m_c3g_allow_d0,   0xFFFFFFFFu },
+    { "hit_reaction_allow@d2",             m_c3g_allow_d2,   0xFFFFFFFFu },
+    { "hit_reaction_allow@inv",            m_c3g_allow_inv,  0xFFFFFFFFu },
+    { "hit_reaction_allow@z",              m_c3g_allow_z,    0xFFFFFFFFu },
+    { "fighter_38c5c",                     b_c3g_38c5c,     0x00000000u },
+    { "fighter_38c5c@c",                   m_c3g_c5c_c,     0x00000000u },
+    { "fighter_38c5c@two",                 m_c3g_c5c_two,   0x00000000u },
+    { "fighter_38c5c@s2",                  m_c3g_c5c_s2,    0x00000000u },
+    { "fighter_38c5c@r9",                  m_c3g_c5c_r9,    0x00000000u },
+    { "fighter_38c5c@s6",                  m_c3g_c5c_s6,    0x00000000u },
+    { "fighter_38c5c@r10",                 m_c3g_c5c_r10,   0x00000000u },
+    { "fighter_38c5c@s3",                  m_c3g_c5c_s3,    0x00000000u },
+    { "fighter_38c5c@col4",                m_c3g_c5c_col4,  0x00000000u },
+    { "fighter_38c5c@n4",                  m_c3g_c5c_n4,    0x00000000u },
+    { "fighter_38c5c@mode",                m_c3g_c5c_mode,  0x00000000u },
+    { "text_cursor_hold",                  b_c3g_2f4bc,     0x00000000u },
+    { "text_cursor_hold@save",             m_c3g_hold_save, 0x00000000u },
+    { "text_cursor_hold@str",              m_c3g_hold_str,  0x00000000u },
+    { "text_cursor_hold@args",             m_c3g_hold_args, 0x00000000u },
+    { "text_cursor_hold@mode",             m_c3g_hold_mode, 0x00000000u },
+    { "text_number_draw",                  b_c3g_2f4d0,     0x00000000u },
+    { "text_number_draw@val",              m_c3g_draw_val,  0x00000000u },
+    { "text_number_draw@pad",              m_c3g_draw_pad,  0x00000000u },
+    { "text_number_draw@mode",             m_c3g_draw_mode, 0x00000000u },
+    { "text_number_draw@save",             m_c3g_draw_save, 0x00000000u },
+    { "text_number_draw@fmt",              m_c3g_draw_fmt,  0x00000000u },
+    { "text_number_draw@col",              m_c3g_draw_col,  0x00000000u },
+    { "text_number_draw@row",              m_c3g_draw_row,  0x00000000u },
+    { "text_vertical_set",                 b_c3g_2f20c,     0x00000000u },
+    { "text_vertical_set@reload",          m_c3g_vert_reload, 0x00000000u },
+    { "text_vertical_set@neg",             m_c3g_vert_neg,   0x00000000u },
+    { "text_vertical_set@wargs",           m_c3g_vert_wargs, 0x00000000u },
+    { "text_vertical_set@skip",            m_c3g_vert_skip,  0x00000000u },
+    { "text_vertical_set@vert",            m_c3g_vert_v,     0x00000000u },
+    { "text_vertical_set@mode",            m_c3g_vert_mode,  0x00000000u },
+    { "text_vertical_set@row",             m_c3g_vert_row,   0x00000000u },
+    { "text_vertical_set@ext",             m_c3g_vert_ext,   0x00000000u },
+    { "text_vertical_set@sext",            m_c3g_vert_sext,  0x00000000u },
 };
 
 static const binding_t *find_binding(const char *name)
@@ -13806,7 +14648,7 @@ typedef struct {
 typedef struct {
     char id[64], fn[64];
     u32 reg[R_N];
-    poke_t poke[16];
+    poke_t poke[40];
     int npoke;
     stub_t stub[16];
     int nstub;
@@ -13979,7 +14821,7 @@ static int run_cases(const char *path)
             int r = 0;
             while (r < R_N && strcmp(t[1], k_reg[r]) != 0) r++;
             if (r == R_N || !parse_hex(t[2], &c.reg[r])) { fprintf(stderr, "diffrun: bad reg line\n"); fclose(f); return 0; }
-        } else if (strcmp(t[0], "poke") == 0 && n == 3 && c.npoke < 16) {
+        } else if (strcmp(t[0], "poke") == 0 && n == 3 && c.npoke < 40) {
             poke_t *p = &c.poke[c.npoke++];
             if (!parse_hex(t[1], &p->addr) || !parse_bytes(t[2], p)) { fprintf(stderr, "diffrun: bad poke line\n"); fclose(f); return 0; }
         } else if (strcmp(t[0], "stub") == 0 && n == 4 && c.nstub < 16) {
diff --git a/tools/diff_emu.py b/tools/diff_emu.py
index 2b0e3f6..4d5623f 100644
--- a/tools/diff_emu.py
+++ b/tools/diff_emu.py
@@ -635,7 +635,11 @@ def callee_clobbers(image, addr, resolved=None):
     # so those callees preserve them.
     return _CALLEE_CLOBBER_FIXES.get(addr, result)
 
-_CALLEE_CLOBBER_FIXES = {0x2B150: (), 0x2BD44: ("edx",), 0x3B298: ("edx",)}
+# C3g: 0x3CF38's body pushes and pops ESI only (0x3CF3B/0x3CFD1, 0x3CFFE) and never writes
+# EDI or EBP, so it preserves them; the scan above over-approximates through the indirect
+# reaction callback. The raw caller 0x3BDDC depends on DI across the call (set at 0x3BDFD,
+# read at 0x3BF14), which the poison would break.
+_CALLEE_CLOBBER_FIXES = {0x2B150: (), 0x2BD44: ("edx",), 0x3B298: ("edx",), 0x3CF38: ()}
 
 
 # ---- decoding helpers for the static tools (E2 tools/entry_triage.py); additive, used by nothing above --
diff --git a/tools/diff_verify.py b/tools/diff_verify.py
index e6625db..254ddbb 100644
--- a/tools/diff_verify.py
+++ b/tools/diff_verify.py
@@ -6780,6 +6780,650 @@ C3F_SPECS = [
 ]
 
 
+# ---- track P batch C3g (record 2026-10-05-reverse-c3g): the closing frontier ---------------------
+#
+# Wave 1: 0x1A7CC, 0x2A690, 0x37178, 0x38D90, 0x38FEC, 0x3BDDC, 0x3C6A8, 0x3CD44, 0x3CE58. The C3f
+# fixtures (C3D_REC0/1, c3d_slot_pokes, c3f_str_case) and seams are reused; every poke stays inside
+# the image.
+
+C3G_POOL, C3G_REC, C3G_PSET = 0x0010A000, 0x0010A068, 0x0010A200
+
+
+def c3g_seed_case(side, i, value, desc=0x000C619C):
+    """0x3C6A8: the three (side, i) words and the frame's stun entry at desc+0xC+value*0x14."""
+    off = side * 0x40 + i * 2
+    return {0x00107D58 + off: le16(0xFFFF), 0x00107E58 + off: le16(0xAAAA),
+            0x00107DD8 + off: le16(0xBBBB),
+            desc + 0x0C + value * 0x14: le16(0x1234)}
+
+
+def c3g_scan_case(side, armed):
+    """0x3CD44: the side's 0x20 phase words, 8 at the indices in `armed` (0 elsewhere)."""
+    p = {0x00107D58 + side * 0x40: b"\x00" * 0x40}
+    for i in armed:
+        p[0x00107D58 + side * 0x40 + i * 2] = le16(8)
+    return p
+
+
+def c3g_fec_case(side, ch, n, rec=0x000C0000):
+    """0x38FEC: the slot's +0x7A char, the 0xBEBB8+ch*2 count word, 0xBEB90+ch*4 record base."""
+    p = c3d_slot_pokes()
+    p[0x001077B0 + side * 0x94 + 0x7A] = bytes([ch])
+    p[0x000BEBB8 + ch * 2] = le16(n)
+    p[0x000BEB90 + ch * 4] = le32(rec)
+    return p
+
+
+def c3g_drive_case(side, i, ch=0, reaction=0x10, mode=0x20, fa=0, s5f=0, s53=0, s56=0):
+    """0x3CE58: the slot's +0x7A/+0x5F/+0x53/+0x56, the mode word 0x104B00, the flash gate
+    0x1078FA and the (ch, i) reaction word at 0xC619C+(ch*0x20+i)*8+4."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    p[so + 0x7A] = bytes([ch])
+    p[so + 0x5F] = bytes([s5f])
+    p[so + 0x53] = bytes([s53])
+    p[so + 0x56] = bytes([s56])
+    p[0x00104B00] = le16(mode)
+    p[0x001078FA] = bytes([fa])
+    p[0x000C619C + (ch * 0x20 + i) * 8 + 4] = le16(reaction)
+    return p
+
+
+def c3g_combo_str_case():
+    """0x38D90: a localisation table with id 0xE5 = group 3, entry 0x25 (0xE4 = entry 0x24),
+    reachable through the chain base+4 -> group1 -> group2 -> group3, and the 0x102760 out buffer
+    seeded 0x5A. The entries between are zero-length so the table fits in <= 0x40-byte pokes."""
+    base = 0x0010B000
+    g3 = [b""] * 36 + [b"XX", b"BBBB"]                 # entry 36 (0xE4) and entry 37 (0xE5)
+    groups = [[b"g0"], [b"g1"], [b"g2"], g3]
+    buf, offs = bytearray(8), []
+    for gi, g in enumerate(groups):
+        if gi > 0:
+            offs.append(len(buf))
+            buf.extend(b"\x00" * 8)
+        for e in g:
+            buf.append(len(e) & 0xFF)
+            buf.extend(bytes(c ^ (len(e) & 0xFF) for c in e))
+    buf[4:8] = le32(offs[0])
+    for i in range(len(offs) - 1):
+        buf[offs[i] + 4:offs[i] + 8] = le32(offs[i + 1])
+    p = {0x001082DC: le32(0x0010AA00),
+         0x0010AA00: b"\x00" * 8 + le32(base) + le32(0x400) + b"\x00" * 15,
+         0x00102760: bytes([0x5A]) * 0x40}
+    for off in range(0, len(buf), 0x40):
+        p[base + off] = bytes(buf[off:off + 0x40])
+    return p
+
+
+def c3g_90_case(side, s63):
+    """0x38D90: the combo string table, the side slot's +0x63 gate and the two number sources
+    (0x107D2C/0x107D20)."""
+    p = c3g_combo_str_case()
+    p[0x001077B0 + side * 0x94 + 0x63] = bytes([s63])
+    p[0x00107D2C + side * 2] = le16(0x1234)
+    p[0x00107D20 + side * 2] = le16(0x0056)
+    return p
+
+
+def c3g_start_case(side, s5f=3, count=3, tribyte=5, pct=50, s43=0xFF, s61=0x11, s62=0, s60=0x55):
+    """0x1A7CC: the side's slot +0x43/+0x60..+0x62, the other slot's +0x5F/+0x64/+0x7A, the
+    0x100CE0 + other*2 count word, the 0xA2C4A pct table (broad 0x0001, then the count+1 entry) and
+    the tri[0]+0xA byte of the (other char = 2, s5f) triple."""
+    other = 1 - side
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    oo = 0x001077B0 + other * 0x94
+    p[so + 0x43] = bytes([s43])
+    p[so + 0x60] = bytes([s60, s61, s62])
+    p[oo + 0x5F] = bytes([s5f & 0xFF])
+    p[oo + 0x64] = bytes([0])
+    p[oo + 0x7A] = bytes([2])
+    p[0x00100CE0 + other * 2] = le16(count)
+    p[0x000A2C4A] = le32(0x00640000) + le32(0x00010001) * 15   # 0xA2C4A..0xA2C89: k=0 100, else 1
+    k = (count + 1) & 0xFFFF
+    p[0x000A2C4C + k * 2] = le16(pct)
+    if s5f < 0x40:
+        c = ((2 << 6) + s5f) * 11
+        p[0x000DE114 + c + 0x0A] = bytes([tribyte & 0xFF])
+    return p
+
+
+def c3g_pset_case(rec, s28, x18=0x12345678, y30=0x00010000, y1c=7, s34=0, s38=0, s64=0,
+                  s61=0x02000000, s44=0, s32=0, s59=0, idx=0, d44=0, daf0=0x00020000,
+                  daec=0x00010000):
+    """0x2A690: the pool bases, the record's fields (one composed buffer, so overlaps cannot drop a
+    field), the two globals and the pset (C3G_PSET + idx*0x20) with sentinels."""
+    buf = bytearray(0x68)
+    def w32(o, v): buf[o:o + 4] = le32(v)
+    def w16(o, v): buf[o:o + 2] = le16(v)
+    w16(0x28, s28)
+    w32(0x18, x18)
+    w32(0x30, y30)          # +0x30..+0x33
+    w32(0x1C, y1c)
+    w16(0x32, s32)          # inside the y30 dword: written after it
+    w16(0x34, s34)
+    w16(0x38, s38)
+    w32(0x3C, 0x5A5A5A5A)
+    w32(0x44, s44)
+    buf[0x59] = s59
+    w16(0x56, idx)
+    w16(0x2C, 0x00AB)
+    w32(0x61, s61)          # +0x61..+0x64
+    buf[0x64] = s64
+    p = {0x001014F4: le32(C3G_POOL), 0x001014EC: le32(C3G_PSET),
+         0x00107A44: le32(d44), 0x000F0AF0: le32(daf0), 0x000F0AEC: le32(daec),
+         rec: bytes(buf[:0x40]), rec + 0x40: bytes(buf[0x40:])}
+    ps = bytearray(0x20)
+    ps[4:8] = le32(0x11111111)
+    ps[8:12] = le32(0x22222222)
+    ps[0x0C:0x0E] = le16(0x7777)
+    ps[0x0E:0x10] = le16(0x8888)
+    ps[0x10:0x14] = le32(0x33333333)
+    ps[0x14:0x18] = le32(0x44444444)
+    p[C3G_PSET + idx * 0x20] = bytes(ps)
+    p[0x00105BE0] = le32(0x66666666)
+    p[0x00105BDC] = le32(0x55555555)
+    p[0x00107900] = le32(0x00001234) + le32(0x00005678)
+    return p
+
+
+def c3g_attack_case(side, s40=0, cmd=0x8000, ch=2, s54=0, chain=0, scan=0):
+    """0x3BDDC: the slot's +0x40/+0x53/+0x54/+0x7A, the command word 0x1088E0+side*2, the
+    0x107803+side*0x94 chain gate byte, the record pointers, the animation table at 0xC8B30 and the
+    two attack rows at 0xBEF28/0xBEF64."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    p[so + 0x40] = bytes([s40])
+    p[so + 0x54] = bytes([s54])
+    p[so + 0x7A] = bytes([ch])
+    p[so + 0x52] = bytes([0x11])
+    p[so + 0x4E] = le16(0x2222)
+    p[0x001088E0 + side * 2] = le16(cmd)
+    p[0x00107803 + side * 0x94] = bytes([0 if chain is None else chain])
+    rec = C3D_REC0 + (side & 1) * 0x40
+    p[rec + 0x34] = le16(0x3333)
+    p[rec + 0x43] = bytes([0x44])
+    p[rec + 0x42] = bytes([0x55])
+    p[0x00107D40 + side * 4] = le32(0x66666666)
+    p[0x001078F8 + side] = bytes([0x77])
+    p[0x000C8B30 + ch * 4] = le32(0x0000ABCD)
+    p[0x000BEF28 + ch * 6] = le16(0x0101) + le16(0x0202) + le16(0x0303)
+    p[0x000BEF64 + ch * 6] = le16(0x0404) + le16(0x0505) + le16(0x0606)
+    p[0x00107D2C + side * 2] = le16(0)
+    return p
+
+
+def c3g_37178_case(rec_s, rec_o, side=None, s51=None, x18_s=0x1000, x18_o=0x1200, s28_s=0,
+                   s28_o=0, ch_s=1, ch_o=1, word=0x0100, tbl=0x0010A300, actor_base=0x0010A400,
+                   act_s=0x8000, act_o=0x8000, idx_s=0, idx_o=1):
+    """0x37178: the two records (X = the slot's [slot], poked to rec_s), +0x51 (the side byte),
+    +0x18, +0x28, +0x7A, +0x56 (the actor index), the 0x1078DC base table and the 0x1014EC actor
+    words (bit 15 selects)."""
+    if side is not None:
+        s51 = side
+    p = c3d_slot_pokes()
+    p[0x001077B0] = le32(rec_s)
+    p[0x001077B0 + 0x94] = le32(rec_o)
+    p[rec_s + 0x51] = bytes([0 if s51 is None else s51])
+    p[rec_o + 0x51] = bytes([1 - (0 if s51 is None else s51)])
+    p[rec_s + 0x18] = le32(x18_s)
+    p[rec_o + 0x18] = le32(x18_o)
+    p[rec_s + 0x28] = le16(s28_s)
+    p[rec_o + 0x28] = le16(s28_o)
+    sd = 0 if s51 is None else (s51 & 1)
+    p[0x001077B0 + sd * 0x94 + 0x7A] = bytes([ch_s])          # the slot's char, not the record's
+    p[0x001077B0 + (1 - sd) * 0x94 + 0x7A] = bytes([ch_o])
+    p[rec_s + 0x56] = le16(idx_s)
+    p[rec_o + 0x56] = le16(idx_o)
+    p[rec_s + 0x40] = bytes([0x11, 0x22, 0x33, 0x44])
+    p[rec_s + 0x52] = bytes([0x11])
+    p[rec_s + 0x54] = bytes([0x22])
+    p[rec_s + 0x43] = bytes([0x55])
+    p[rec_o + 0x42] = bytes([0x77])
+    p[0x001078FE] = bytes([0x88])
+    p[0x001078DC] = le32(tbl)
+    p[tbl + ch_s * 14 + ch_o * 2] = le16(word)
+    p[tbl + 2 + ch_o * 2] = le16(0x0080)        # the @base mutant's wrong-stride read
+    p[0x001014EC] = le32(actor_base)
+    p[actor_base + idx_s * 0x20] = le16(act_s)
+    p[actor_base + idx_o * 0x20] = le16(act_o)
+    return p
+
+
+def c3g_stance_case(side, i, ch, st, e, d, s7b=0):
+    """0x3CCEC: the side slot's +0x54 (stance) and +0x7A (char; +0x7B is the @char mutant's
+    wrong byte, seeded with `s7b`, whose own entry gets zero flags), and the 0xC619C entry
+    (char*0x20+i)*8: six sentinel bytes then d (+6) and e (+7). The swapped-index entry the
+    @entry mutant reads is seeded 0xF1/0xF2 so a wrong index changes the verdict."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    p[so + 0x54] = bytes([st])
+    p[so + 0x7A] = bytes([ch, s7b])
+    entry = ch * 0x20 + i
+    p[0x000C619C + entry * 8] = b"\x41\x42\x43\x44\x45\x46"
+    p[0x000C619C + entry * 8 + 6] = bytes([d])
+    p[0x000C619C + entry * 8 + 7] = bytes([e])
+    entry2 = ch + i * 0x20
+    if entry2 != entry:
+        p[0x000C619C + entry2 * 8 + 6] = b"\xF1"
+        p[0x000C619C + entry2 * 8 + 7] = b"\xF2"
+    entry3 = s7b * 0x20 + i
+    if entry3 != entry:
+        p[0x000C619C + entry3 * 8 + 6] = b"\x00"
+        p[0x000C619C + entry3 * 8 + 7] = b"\x00"
+    return p
+
+
+def c3g_desc_case(side, i, s63, sel=None, ch=0, ptr=0x0010A000, cpu=0x11111111, pl=0x22222222):
+    """0x3C600: the side slot's +0x63 (nonzero forces sel 2) and +0x7A (char; +0x7B feeds the
+    @char mutant), the 0x101514 pointer's mode word (+0x2D4/+0x2D6; the unused one is 0xAA so a
+    wrong-side read hits the default), and the 0xC6B9C/0xC619C entries at (char*0x20+i)*8: the real
+    one, the swapped-index one (@entry), the char+1 one (@char) and the next entry dword."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    p[so + 0x63] = bytes([s63])
+    p[so + 0x7A] = bytes([ch, ch + 1])
+    p[0x00101514] = le32(ptr)
+    p[ptr + 0x2D4] = le16(0x00AA)
+    p[ptr + 0x2D6] = le16(0x00AA)
+    if sel is not None:
+        p[ptr + 0x2D4 + side * 2] = le16(sel)
+    entry = ch * 0x20 + i
+    p[0x000C6B9C + entry * 8] = le32(cpu)
+    p[0x000C619C + entry * 8] = le32(pl)
+    entry2 = ch + i * 0x20
+    if entry2 != entry:
+        p[0x000C6B9C + entry2 * 8] = le32(0x33333333)
+        p[0x000C619C + entry2 * 8] = le32(0x44444444)
+    entry3 = (ch + 1) * 0x20 + i
+    if entry3 != entry:
+        p[0x000C6B9C + entry3 * 8] = le32(0x55555555)
+        p[0x000C619C + entry3 * 8] = le32(0x66666666)
+    p[0x000C6B9C + (entry + 1) * 8] = le32(0x77777777)
+    p[0x000C619C + (entry + 1) * 8] = le32(0x88888888)
+    return p
+
+
+def c3g_react_case(side, st, cmd, ch=0, tbl2=0x000A7B44, tbl2w=0x000A7ACC, idx_tbl=0x000A7A70):
+    """0x3CBC4/0x3CC58: the side slot's +0x54 (stance) and +0x7A (char), the command word
+    0x1088E0+side*2, the per-char table pointer DSD(`tbl2`+ch*4) (0xA7B44 for A, 0xA7ACC for B;
+    the other table is seeded differently for the @table mutant) and the 0xA7A70 idx byte."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    p[so + 0x54] = bytes([st])
+    p[so + 0x7A] = bytes([ch])
+    p[0x001088E0 + side * 2] = le16(cmd)
+    p[tbl2 + ch * 4] = le32(0x0010A300)
+    other = tbl2w if tbl2 != tbl2w else 0x000A7B44
+    if other != tbl2:
+        p[other + ch * 4] = le32(0x0010A400)
+    p[idx_tbl + ch] = bytes([0x44])
+    return p
+
+
+def c3g_react_b_case(side, st, cmd, ch=0):
+    return c3g_react_case(side, st, cmd, ch=ch, tbl2=0x000A7ACC, tbl2w=0x000A7B44)
+
+
+def c3g_allow_case(side, ch):
+    """0x4CE70: both slots' +0x7A chars (the unread side's is 0x0B > 6 so the @side mutant is
+    visible) and +0x7B (ch^1, so the @char mutant reads a different list)."""
+    p = c3d_slot_pokes()
+    for s in (0, 1):
+        so = 0x001077B0 + s * 0x94
+        c = ch if s == side else 0x0B
+        p[so + 0x7A] = bytes([c, c ^ 1])
+    return p
+
+
+def c3g_gate_case(side, s56):
+    """0x3CE24: the side slot's +0x56 (the signed byte the raw's `sar 0x18` of the +0x53 dword
+    reads), with +0x55 and +0x57 seeded different from it."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    p[so + 0x55] = b"\x7E"
+    p[so + 0x56] = bytes([s56])
+    p[so + 0x57] = b"\x7F"
+    return p
+
+
+C3G_SPECS = [
+    # 0x3C6A8 hit_slot_seed: EAX = side, EDX = value, EBX = i. 0x3C600 (desc) is a stub.
+    Spec("hit_slot_seed", 0x3C6A8, [
+        Case("g0", {"eax": 0, "edx": 2, "ebx": 3}, c3g_seed_case(0, 3, 2), {0x3C600: 0x000C619C}),
+        Case("g1", {"eax": 1, "edx": 0, "ebx": 0}, c3g_seed_case(1, 0, 0), {0x3C600: 0x000C619C}),
+        Case("g2", {"eax": 0, "edx": 4, "ebx": 0x1F}, c3g_seed_case(0, 0x1F, 4), {0x3C600: 0x000C619C}),
+        Case("g3", {"eax": 1, "edx": 1, "ebx": 5}, c3g_seed_case(1, 5, 1, desc=0x000C719C),
+             {0x3C600: 0x000C719C}),
+    ], calls=(E.Call(0x3C600, ("eax", "edx"), mode="stub"),),
+       eax_mask=0, mutants=("@phase", "@clear", "@off", "@field", "@scale", "@bytes")),
+    # 0x3CD44 hit_scan: EAX = side. 0x3CCEC (stance) is a stub (AL tested); -1 when none armed.
+    Spec("hit_scan", 0x3CD44, [
+        Case("t0", {"eax": 0}, c3g_scan_case(0, [0]), {0x3CCEC: 1}),
+        Case("t1", {"eax": 0}, c3g_scan_case(0, [2, 5]), {0x3CCEC: 1}),
+        Case("t2", {"eax": 0}, c3g_scan_case(0, [3]), {0x3CCEC: 0}),
+        Case("t3", {"eax": 0}, c3g_scan_case(0, []), {0x3CCEC: 1}),
+        Case("t4", {"eax": 0}, c3g_scan_case(0, [0x1F]), {0x3CCEC: 1}),
+        Case("t5", {"eax": 1}, c3g_scan_case(1, [0x1F]), {0x3CCEC: 1}),
+        Case("t6", {"eax": 1}, c3g_scan_case(1, []), {0x3CCEC: 1}),
+        Case("t7", {"eax": 0}, c3g_scan_case(0, [2, 5]), {0x3CCEC: 0x100}),
+    ], calls=(E.Call(0x3CCEC, ("eax", "edx"), mode="stub", clobbers=("edx",)),),
+       eax_mask=0xFFFFFFFF, mutants=("@bound", "@phase", "@side", "@stride", "@al", "@miss")),
+    # 0x38FEC fighter_38fec: EAX = side; walk the char's combo records until 0x38ED0 (stub) says 1.
+    Spec("fighter_38fec", 0x38FEC, [
+        Case("f0", {"eax": 0}, c3g_fec_case(0, 2, 0)),
+        Case("f1", {"eax": 0}, c3g_fec_case(0, 2, 2), {0x38ED0: 1}),
+        Case("f2", {"eax": 0}, c3g_fec_case(0, 3, 2), {0x38ED0: 0}),
+        Case("f3", {"eax": 1}, c3g_fec_case(1, 5, 1), {0x38ED0: 1}),
+        Case("f4", {"eax": 0}, c3g_fec_case(0, 2, 0xFFFF)),
+        Case("f5", {"eax": 0}, c3g_fec_case(0, 0, 2), {0x38ED0: 1}),
+    ], allow_calls=(0x33950,),
+       calls=(E.Call(0x38ED0, ("eax", "edx"), mode="stub", clobbers=("edx",)),),
+       eax_mask=0, mutants=("@ch", "@n", "@table", "@stride", "@call")),
+    # 0x3CE58 hit_reaction_drive: EAX = side, EDX = i. 0x3CE24 (gate) and 0x4CE70 (allow) are
+    # stubs whose AL the raw tests; 0x3CBC4/0x3CC58 return the reaction word (movsx, 0x3CF14);
+    # 0x34D8C runs real (the 0x1078FA gate); 0x34E2C (apply) is a stub.
+    Spec("hit_reaction_drive", 0x3CE58, [
+        Case("d0", {"eax": 0, "edx": 0}, c3g_drive_case(0, 0), {0x3CE24: 0}),
+        Case("d1", {"eax": 0, "edx": 0x20}, c3g_drive_case(0, 5), {0x3CE24: 1}),
+        Case("d2", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x10, mode=0x21),
+             {0x3CE24: 1, 0x4CE70: 0}),
+        Case("d3", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x10, mode=0x22),
+             {0x3CE24: 1, 0x4CE70: 1}),
+        Case("d4", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x10),
+             {0x3CE24: 1, 0x3CBC4: 0x1234}),
+        Case("d5", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x11),
+             {0x3CE24: 1, 0x3CC58: 0x5678}),
+        Case("d6", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x12),
+             {0x3CE24: 1}),
+        Case("d7", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x0F),
+             {0x3CE24: 1}),
+        Case("d8", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x10),
+             {0x3CE24: 1, 0x3CBC4: 0x8000}),
+        Case("d9", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x12, fa=2),
+             {0x3CE24: 1}),
+        Case("d10", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x12), {0x3CE24: 0x100}),
+        Case("d11", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x12, mode=0x21),
+             {0x3CE24: 1, 0x4CE70: 0x100}),
+    ], allow_calls=(0x34D8C,),
+       calls=(E.Call(0x3CE24, ("eax", "edx"), mode="stub", clobbers=("edx",)),
+              E.Call(0x4CE70, ("eax", "edx"), mode="stub"),
+              E.Call(0x3CBC4, ("eax",), mode="stub"),
+              E.Call(0x3CC58, ("eax",), mode="stub"),
+              E.Call(0x34E2C, ("eax", "edx"), mode="stub", clobbers=("edx", "edi", "ebp"))),
+       eax_mask=0xFF, mutants=("@gate", "@gateal", "@bound", "@mode", "@allow", "@flash",
+                               "@sel", "@hi", "@store", "@apply", "@ret")),
+    # 0x38D90 fighter_38d90: EAX = side. The whole string path runs real (0x1C500 and its tree);
+    # the four text calls are stubs, the 0x1C500 string feeds the two row-6/7 holds.
+    Spec("fighter_38d90", 0x38D90, [
+        Case("s0", {"eax": 0}, c3g_90_case(0, 0)),
+        Case("s1", {"eax": 1}, c3g_90_case(1, 0)),
+        Case("s2", {"eax": 0}, c3g_90_case(0, 1)),
+    ], allow_calls=(0x1C500, 0x474E4, 0x1E75C, 0x1E808, 0x500BB),
+       calls=(E.Call(0x38C5C, ("eax",), mode="stub"),
+              E.Call(0x2F4BC, ("eax", "edx", "ebx", "ecx"), mode="stub",
+                     clobbers=("ebx", "ecx", "edx")),
+              E.Call(0x2F4D0, ("eax", "edx", "ebx", "ecx", "s0", "s1"), mode="stub", pop=8,
+                     clobbers=("ebx", "ecx", "edx")),
+              E.Call(0x2F20C, ("eax", "edx", "ebx", "ecx"), mode="stub",
+                     clobbers=("ebx", "ecx", "edx"))),
+       eax_mask=0, mutants=("@c5c", "@row", "@id", "@mode", "@col", "@pctrow", "@pct")),
+    # 0x1A7CC fighter_block_start: EAX = side. 0x33A10/0x3AFC4 are allows; 0x18B04 and 0x1A6AC
+    # are stubs. The 0x1A842 arm is dead (the +0x5F <= 0x3F test at 0x1A810 precedes it).
+    Spec("fighter_block_start", 0x1A7CC, [
+        Case("b0", {"eax": 0}, c3g_start_case(0, s5f=3, count=3, tribyte=5, pct=50)),
+        Case("b1", {"eax": 0}, c3g_start_case(0, s5f=0x40, count=1, pct=40)),
+        Case("b2", {"eax": 0}, c3g_start_case(0, s5f=0x3F, count=0, tribyte=0x7F, pct=30)),
+        Case("b3", {"eax": 0}, c3g_start_case(0, s5f=1, count=6, pct=10)),
+        Case("b4", {"eax": 0}, c3g_start_case(0, s5f=1, count=0x7FFF, pct=10)),
+        Case("b5", {"eax": 1}, c3g_start_case(1, s5f=2, count=2, tribyte=9, pct=25)),
+        Case("b6", {"eax": 0}, c3g_start_case(0, s5f=4, count=2, tribyte=0x80, pct=50)),
+    ], allow_calls=(0x33A10, 0x3AFC4),
+       calls=(E.Call(0x18B04, ("eax",), mode="stub"),
+              E.Call(0x1A6AC, ("eax", "edx"), mode="stub", clobbers=("edx",))),
+       eax_mask=0,
+       unhit_named={0x1A842: "dead: the +0x5F <= 0x3F arm of 0x1A810 was just taken (re-tested at 0x1A82D), so the 0x1A830 jge cannot fall here",
+                    0x1A854: "dead: the same arm as 0x1A842 (its second call site)"},
+       mutants=("@and", "@s61", "@s60", "@s62", "@bound", "@tri", "@k",
+                "@div", "@tbl", "@s52", "@s53")),
+    # 0x2A690 actor_pset_point: EAX = rec. 0x2A620 (cursor) is a stub; the pset is
+    # 0x1014EC + word[rec+0x56]*0x20 and the record must be in the 0x1014F4 pool (the port's
+    # actor_pset guard).
+    Spec("actor_pset_point", 0x2A690, [
+        Case("q0", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x0001)),
+        Case("q1", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x1001, s38=1, s64=0x7F, s61=0x02000000,
+                                                   s44=0x00020000)),
+        Case("q2", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x1000, s64=0xFF, s34=0)),
+        Case("q3", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x1000, s64=0xFF, s34=5, s32=200,
+                                                   d44=0xFF9C0000)),
+        Case("q4", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x0000, s32=1, y30=0xFF000000, s59=0x7F)),
+        Case("q5", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x0000, s32=0xFFFF, s59=0x80)),
+        Case("q6", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x1040, s32=0xFFFF, s59=0x7F)),
+        Case("q7", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x1000, s38=1, s64=0x00, s61=0x03000000,
+                                                   s44=0x00040000, idx=1)),
+    ], calls=(E.Call(0x2A620, ("eax", "edx"), mode="stub", clobbers=("edx",)),),
+       # @cls and a lower layer-B clamp are equivalent mutations (record §C3g.3): layer B is
+       # s8(s59)+0xF0 >= 0x70, and at s32 == 0 the two layer arms compute the same value.
+       eax_mask=0, mutants=("@bit", "@x0", "@x2", "@gte", "@ramp", "@x44", "@div", "@ymask",
+                            "@rec3c", "@bdc", "@cursor")),
+    # 0x3BDDC fighter_attack_consume: EAX = side. 0x33950 runs real; 0x3CF38 and 0x4649C are
+    # stubs; 0x3C480 is the HIT_A stub (hold 1.0 pushed).
+    Spec("fighter_attack_consume", 0x3BDDC, [
+        Case("a0", {"eax": 0}, c3g_attack_case(0, s40=0x80)),
+        Case("a1", {"eax": 0}, c3g_attack_case(0, cmd=0x7FFF)),
+        Case("a2", {"eax": 0}, c3g_attack_case(0, cmd=0x8000, chain=None), {0x3CF38: 1}),
+        Case("a3", {"eax": 0}, c3g_attack_case(0, cmd=0x9000, chain=None),
+             {0x3CF38: 0, 0x4649C: 5}),
+        Case("a4", {"eax": 0}, c3g_attack_case(0, cmd=0x9000, chain=1), {0x4649C: 5}),
+        Case("a5", {"eax": 0}, c3g_attack_case(0, cmd=0xA000, chain=None),
+             {0x3CF38: 0, 0x4649C: 0}),
+        Case("a6", {"eax": 0}, c3g_attack_case(0, cmd=0x8000, chain=None),
+             {0x3CF38: 0, 0x4649C: 0}),
+        Case("a7", {"eax": 1}, c3g_attack_case(1, cmd=0x9000, chain=None, ch=5),
+             {0x3CF38: 0, 0x4649C: 3}),
+    ], allow_calls=(0x33950,),
+       calls=(E.Call(0x3CF38, ("eax",), mode="stub"),  # preserves EDI/EBP: §C3g.2
+              E.Call(0x4649C, ("eax", "edx", "ebx", "ecx"), mode="stub",
+                     clobbers=("ebx", "edx")),
+              E.Call(0x3C480, ("eax", "edx", "s0"), mode="stub", pop=4, clobbers=("edx",))),
+       eax_mask=0xFF, mutants=("@o40", "@cmd", "@clr", "@s5f", "@gate", "@chain", "@scan",
+                               "@base", "@row", "@anim", "@state", "@dir", "@ret")),
+    # 0x37178 fighter_37178: EAX = slot. 0x1A570 runs real; 0x36870 (ANIM54), 0x35838 (DIRS) and
+    # 0x36638 are stubs. The base word is at 0x1078DC + ch_s*14 + ch_o*2; the facing/bits come
+    # from the two records' +0x28.
+    Spec("fighter_37178", 0x37178, [
+        Case("r0", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0xFFFF)),
+        Case("r1", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
+                                                       x18_s=0x1000, x18_o=0x1200, act_o=0x8000,
+                                                       act_s=0)),
+        Case("r2", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
+                                                       x18_s=0x2000, x18_o=0x1000, s28_s=0x4000,
+                                                       s28_o=0, act_o=0, act_s=0x8000)),
+        Case("r3", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
+                                                       x18_s=0x0F00, x18_o=0x1000, s28_s=0x4000,
+                                                       s28_o=0, act_o=0, act_s=0x8000)),
+        Case("r4", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
+                                                       x18_s=0x2000, x18_o=0x0F00, s28_s=0x4000,
+                                                       s28_o=0, act_o=0x8000, act_s=0)),
+        Case("r5", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
+                                                       x18_s=0x0800, x18_o=0x1000, s28_s=0x4000,
+                                                       s28_o=0, act_o=0x8000, act_s=0x8000)),
+        Case("r6", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
+                                                       x18_s=0x1000, x18_o=0x1050)),
+        Case("r7", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
+                                                       x18_s=0x2000, x18_o=0x1000, act_o=0x8000,
+                                                       act_s=0)),
+        Case("r8", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
+                                                       x18_s=0x0F00, x18_o=0x1000, act_o=0x8000,
+                                                       act_s=0)),
+        Case("r9", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
+                                                       x18_s=0x2000, x18_o=0x0F00, act_o=0,
+                                                       act_s=0x8000)),
+        Case("r10", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
+                                                        x18_s=0x0F00, x18_o=0x2000, act_o=0,
+                                                        act_s=0x8000)),
+        Case("r11", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
+                                                        x18_s=0x3000, x18_o=0x1000, act_o=0,
+                                                        act_s=0x8000)),
+        Case("r12", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0500,
+                                                        x18_s=0x1000, x18_o=0x1000, act_o=0x8000,
+                                                        act_s=0)),
+        Case("r13", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
+                                                        x18_s=0x1500, x18_o=0x1200, act_o=0,
+                                                        act_s=0)),
+    ], allow_calls=(0x1A570,),
+       calls=(E.Call(0x36870, ("eax",), mode="stub", clobbers=("esi", "edi", "ebp")),
+              E.Call(0x35838, ("eax", "edx", "ebx"), mode="stub", clobbers=("ebx", "edx")),
+              E.Call(0x36638, ("eax", "edx"), mode="stub", clobbers=("edx",))),
+        eax_mask=0, mutants=("@base", "@x", "@facing", "@bound", "@s52", "@fe", "@s18", "@call",
+                             "@dirs", "@s57", "@s43", "@s54")),
+    # ---- wave 2: the 0x3CF38 tree's predicates -----------------------------------------------
+    # 0x3CCEC hit_stance_ok: EAX = side, EDX = i. The 0xC619C entry's +7 (e) selects stance 2,
+    # +6 (d) stances 0/1.
+    Spec("hit_stance_ok", 0x3CCEC, [
+        Case("k0", {"eax": 0, "edx": 3}, c3g_stance_case(0, 3, 0, 2, 1, 0)),
+        Case("k1", {"eax": 0, "edx": 3}, c3g_stance_case(0, 3, 0, 2, 0, 1)),
+        Case("k2", {"eax": 1, "edx": 0}, c3g_stance_case(1, 0, 5, 1, 0, 1)),
+        Case("k3", {"eax": 0, "edx": 0x1F}, c3g_stance_case(0, 0x1F, 6, 2, 1, 1, s7b=5)),
+        Case("k4", {"eax": 0, "edx": 5}, c3g_stance_case(0, 5, 2, 3, 1, 0)),
+        Case("k5", {"eax": 0, "edx": 7}, c3g_stance_case(0, 7, 3, 0, 0, 1)),
+        Case("k6", {"eax": 1, "edx": 1}, c3g_stance_case(1, 1, 1, 1, 1, 0)),
+        Case("k7", {"eax": 0, "edx": 0}, c3g_stance_case(0, 0, 0, 2, 0, 0)),
+        Case("k8", {"eax": 0, "edx": 3}, c3g_stance_case(0, 3, 0, 2, 0, 0)),
+    ], eax_mask=0xFFFFFFFF, mutants=("@e", "@st2", "@d", "@lo", "@entry", "@char", "@ret")),
+    # 0x3CE24 hit_gate: EAX = side, EDX = i. 0x3CD94 (immunity) is a stub whose AL the raw tests
+    # (0x3CE4F); the early reject's AL is 0 with the sign byte's high bits in EAX, so the mask is AL.
+    Spec("hit_gate", 0x3CE24, [
+        Case("g0", {"eax": 0, "edx": 0}, c3g_gate_case(0, 5), {0x3CD94: 1}),
+        Case("g1", {"eax": 0, "edx": 0}, c3g_gate_case(0, 6), {0x3CD94: 1}),
+        Case("g2", {"eax": 0, "edx": 0}, c3g_gate_case(0, 5), {0x3CD94: 0}),
+        Case("g3", {"eax": 0, "edx": 3}, c3g_gate_case(0, 0x80), {0x3CD94: 1}),
+        Case("g4", {"eax": 1, "edx": 7}, c3g_gate_case(1, 0xFF), {0x3CD94: 1}),
+        Case("g5", {"eax": 0, "edx": 0}, c3g_gate_case(0, 5), {0x3CD94: 0x100}),
+        Case("g6", {"eax": 0, "edx": 0x1F}, c3g_gate_case(0, 0), {0x3CD94: 0x100}),
+        Case("g7", {"eax": 1, "edx": 2}, c3g_gate_case(1, 6), {0x3CD94: 1}),
+        Case("g8", {"eax": 0, "edx": 4}, c3g_gate_case(0, 0x7F), {0x3CD94: 1}),
+        Case("g9", {"eax": 0, "edx": 5}, c3g_gate_case(0, 5), {0x3CD94: 0x37}),
+    ], calls=(E.Call(0x3CD94, ("eax", "edx"), mode="stub", clobbers=("edx",)),),
+       eax_mask=0xFF, mutants=("@bound", "@signed", "@off", "@ret", "@call", "@al")),
+    # 0x3C600 hit_frame_desc: EAX = side, EDX = i, ECX the caller's (the raw's default and its
+    # sel>6 arm return ECX; the port returns 0, so every default case holds ECX 0 — named limit).
+    Spec("hit_frame_desc", 0x3C600, [
+        Case("d0", {"eax": 0, "edx": 3}, c3g_desc_case(0, 3, 1, ch=0)),
+        Case("d1", {"eax": 0, "edx": 0}, c3g_desc_case(0, 0, 0, sel=0, ch=1)),
+        Case("d2", {"eax": 0, "edx": 0x1F}, c3g_desc_case(0, 0x1F, 0, sel=4, ch=2)),
+        Case("d3", {"eax": 0, "edx": 2}, c3g_desc_case(0, 2, 0, sel=6, ch=3)),
+        Case("d4", {"eax": 1, "edx": 5}, c3g_desc_case(1, 5, 0, sel=2, ch=4)),
+        Case("d5", {"eax": 0, "edx": 1}, c3g_desc_case(0, 1, 0, sel=7, ch=5)),
+        Case("d6", {"eax": 1, "edx": 1}, c3g_desc_case(1, 1, 0, sel=1, ch=5)),
+        Case("d7", {"eax": 0, "edx": 1}, c3g_desc_case(0, 1, 0, sel=0xFFFF, ch=6)),
+    ], eax_mask=0xFFFFFFFF,
+       mutants=("@s63", "@side", "@gt", "@table", "@entry", "@char", "@def", "@mask")),
+    # 0x3CBC4/0x3CC58 hit_reaction_a/b: EAX = side. 0x33950 is allowed (the port reads the char
+    # directly); 0x1DDF4 (C1_GEOM, its own row) is stubbed.
+    Spec("hit_reaction_a", 0x3CBC4, [
+        Case("a0", {"eax": 0}, c3g_react_case(0, 2, 0x0000)),
+        Case("a1", {"eax": 0}, c3g_react_case(0, 0, 0x4000)),
+        Case("a2", {"eax": 0}, c3g_react_case(0, 1, 0x0000), {0x1DDF4: 1}),
+        Case("a3", {"eax": 0}, c3g_react_case(0, 1, 0x0000), {0x1DDF4: 0}),
+        Case("a4", {"eax": 3}, c3g_react_case(0, 3, 0x0001), {0x1DDF4: 1}),
+        Case("a5", {"eax": 1}, c3g_react_case(1, 2, 0x0000)),
+        Case("a6", {"eax": 1}, c3g_react_case(1, 0, 0x4000)),
+        Case("a7", {"eax": 0}, c3g_react_case(0, 2, 0x4000)),
+    ], allow_calls=(0x33950,), calls=(C1_GEOM,),
+       eax_mask=0xFFFFFFFF, mutants=("@st", "@bit", "@side", "@table", "@idx", "@ret", "@geom")),
+    Spec("hit_reaction_b", 0x3CC58, [
+        Case("b0", {"eax": 0}, c3g_react_b_case(0, 2, 0x0000)),
+        Case("b1", {"eax": 0}, c3g_react_b_case(0, 0, 0x4000)),
+        Case("b2", {"eax": 0}, c3g_react_b_case(0, 1, 0x0000), {0x1DDF4: 1}),
+        Case("b3", {"eax": 0}, c3g_react_b_case(0, 1, 0x0000), {0x1DDF4: 0}),
+        Case("b4", {"eax": 3}, c3g_react_b_case(0, 3, 0x0001), {0x1DDF4: 1}),
+        Case("b5", {"eax": 1}, c3g_react_b_case(1, 2, 0x0000)),
+        Case("b6", {"eax": 1}, c3g_react_b_case(1, 0, 0x4000)),
+    ], allow_calls=(0x33950,), calls=(C1_GEOM,),
+       eax_mask=0xFFFFFFFF, mutants=("@st", "@bit", "@side", "@table", "@idx", "@ret", "@geom")),
+    # 0x4CE70 hit_reaction_allow: EAX = side, EDX = reaction; the per-char blocked-id list
+    # (0x4CE54's jump table over the +0x7A char), 1 unless `reaction` is in it.
+    Spec("hit_reaction_allow", 0x4CE70, [
+        Case("r0", {"eax": 0, "edx": 0x28}, c3g_allow_case(0, 0)),
+        Case("r1", {"eax": 0, "edx": 0x29}, c3g_allow_case(0, 0)),
+        Case("r2", {"eax": 0, "edx": 0x2A}, c3g_allow_case(0, 0)),
+        Case("r3", {"eax": 0, "edx": 0x2C}, c3g_allow_case(0, 0)),
+        Case("r4", {"eax": 0, "edx": 0x2B}, c3g_allow_case(0, 0)),
+        Case("r5", {"eax": 0, "edx": 0x24}, c3g_allow_case(0, 1)),
+        Case("r6", {"eax": 0, "edx": 0x23}, c3g_allow_case(0, 3)),
+        Case("r7", {"eax": 1, "edx": 0x26}, c3g_allow_case(1, 2)),
+        Case("r8", {"eax": 1, "edx": 0x27}, c3g_allow_case(1, 2)),
+        Case("r9", {"eax": 0, "edx": 0x24}, c3g_allow_case(0, 4)),
+        Case("r10", {"eax": 0, "edx": 0x21}, c3g_allow_case(0, 5)),
+        Case("r11", {"eax": 0, "edx": 0x24}, c3g_allow_case(0, 6)),
+        Case("r12", {"eax": 0, "edx": 0x28}, c3g_allow_case(0, 7)),
+        Case("r13", {"eax": 0, "edx": 0x28}, c3g_allow_case(0, 0xFF)),
+        Case("r14", {"eax": 1, "edx": 0x22}, c3g_allow_case(1, 1)),
+        Case("r15", {"eax": 0, "edx": 0x26}, c3g_allow_case(0, 4)),
+        Case("r16", {"eax": 0, "edx": 0x22}, c3g_allow_case(0, 5)),
+    ], eax_mask=0xFFFFFFFF,
+       mutants=("@side", "@char", "@off", "@shift", "@d0", "@d2", "@inv", "@z")),
+    # 0x38C5C fighter_38c5c: EAX = side. Four text calls, no reads: the row is the recorded call
+    # list (0x2F280 x3 stubs and 0x2F314 x1 stub; 0x2F314 ignores the raw's ECX).
+    Spec("fighter_38c5c", 0x38C5C, [
+        Case("s0", {"eax": 0}, {}),
+        Case("s1", {"eax": 1}, {}),
+    ], calls=(E.Call(0x2F280, ("eax", "edx", "ebx", "ecx"), mode="stub", clobbers=("ebx", "ecx", "edx")),
+              E.Call(0x2F314, ("eax", "edx", "ebx"), mode="stub", clobbers=("ebx", "ecx", "edx"))),
+       eax_mask=0,
+       mutants=("@c", "@two", "@r9", "@r10", "@s2", "@s6", "@s3", "@mode", "@col4", "@n4")),
+    # 0x2F4BC text_cursor_hold: EAX = col, EDX = row, EBX = string, ECX = mode. 0x2F198 is a stub
+    # (its by-value string dwords) that also writes 0x105F34, so the save/restore is observable.
+    Spec("text_cursor_hold", 0x2F4BC, [
+        Case("h0", {"eax": 3, "edx": 4, "ebx": 0x0010A200, "ecx": 0x1000},
+             {0x00105F34: le32(0x11223344), 0x0010A200: b"\x11\x22\x33\x44\x55\x66\x77\x88"}),
+        Case("h1", {"eax": 0xFFFFFFFF, "edx": 0xFFFFFFFF, "ebx": 0x0010A210, "ecx": 0x2000},
+             {0x00105F34: le32(0xAABBCCDD), 0x0010A210: b"\x91\x82\x73\x64\x55\x46\x37\x28"}),
+    ], calls=(E.Call(0x2F198, ("eax", "edx", "[ebx]", "[ebx+4]", "[ebx+8]", "[ebx+12]", "ecx"),
+                     mode="stub", clobbers=("ebx", "ecx", "edx"),
+                     writes=((None, 0x00105F34, b"\x5A\x5A\x5A\x5A"),)),),
+       eax_mask=0, mutants=("@save", "@args", "@str", "@mode")),
+    # 0x2F4D0 text_number_draw: EAX = col, EDX = row, EBX = value, ECX = width, [esp+4] = pad,
+    # [esp+8] = mode; `ret 8`. 0x2EFD4 and 0x2F198 are stubs (by-value buffers; the 0x2F198 stub
+    # writes 0x105F34 so the cursor save/restore is observable).
+    Spec("text_number_draw", 0x2F4D0, [
+        Case("n0", {"eax": 5, "edx": 6, "ebx": 0xFFFFFFD6, "ecx": 3, "s0": 0, "s1": 0x1000},
+             {0x00105F34: le32(0x11223344)}),
+        Case("n1", {"eax": 0, "edx": 0, "ebx": 12345, "ecx": 5, "s0": 2, "s1": 0x2000},
+             {0x00105F34: le32(0xAABBCCDD)}),
+    ], calls=(E.Call(0x2EFD4, ("eax", "[edx]", "[edx+4]", "[edx+8]", "[edx+12]", "ebx", "ecx"),
+                     mode="stub", clobbers=("ebx", "ecx", "edx")),
+              E.Call(0x2F198, ("eax", "edx", "[ebx]", "[ebx+4]", "[ebx+8]", "[ebx+12]", "ecx"),
+                     mode="stub", clobbers=("ebx", "ecx", "edx"),
+                     writes=((None, 0x00105F34, b"\x5A\x5A\x5A\x5A"),))),
+       eax_mask=0, mutants=("@val", "@pad", "@mode", "@save", "@fmt", "@col", "@row")),
+    # 0x2F20C text_vertical_set: EAX = col, EDX = row, EBX = string, ECX = mode. 0x2F0F0 (the
+    # centring width) and 0x2F830 (vertical = 1) are stubs; the cursor 0x105F34 takes {row,
+    # col + extent}, reloaded sign-extended when row == -1.
+    Spec("text_vertical_set", 0x2F20C, [
+        Case("v0", {"eax": 5, "edx": 6, "ebx": 0x0010A200, "ecx": 0x1000},
+             {0x00105F34: le32(0x00010002)}, {0x2F830: 7}),
+        Case("v1", {"eax": 0xFFFFFFFF, "edx": 4, "ebx": 0x0010A200, "ecx": 0x2000},
+             {0x00105F34: le32(0x00010002)}, {0x2F0F0: 0x21, 0x2F830: 3}),
+        Case("v2", {"eax": 0xFFFFFFFF, "edx": 4, "ebx": 0x0010A200, "ecx": 0x2000},
+             {0x00105F34: le32(0x00010002)}, {0x2F0F0: 0x40, 0x2F830: 2}),
+        Case("v3", {"eax": 7, "edx": 0xFFFFFFFF, "ebx": 0x0010A200, "ecx": 0x2000},
+             {0x00105F34: le32(0x00040003)}, {0x2F830: 1}),
+        Case("v4", {"eax": 0xFFFFFFFF, "edx": 0xFFFFFFFF, "ebx": 0x0010A200, "ecx": 0x2000},
+             {0x00105F34: le32(0x00060005)}, {0x2F830: 0}),
+        Case("v5", {"eax": 0, "edx": 0xFFFFFFFF, "ebx": 0x0010A200, "ecx": 0x2000},
+             {0x00105F34: le32(0x0000FFF6)}, {0x2F830: 1}),
+    ], calls=(E.Call(0x2F0F0, ("eax", "edx"), mode="stub", clobbers=("edx",)),
+              E.Call(0x2F830, ("eax", "edx", "ecx", "ebx", "s0"), mode="stub", pop=4,
+                     clobbers=("ebx", "ecx", "edx"))),
+       # The raw's EAX at the ret is 0x2F830's return (its `add esi,eax` leaves EAX); the callers
+       # read nothing, so the mask is 0.
+       eax_mask=0,
+       mutants=("@reload", "@neg", "@wargs", "@skip", "@vert", "@mode", "@ext", "@row", "@sext")),
+]
+
+
 SPECS = [
     Spec("rng_next", 0x5D7DC, [
         Case("r1", {"eax": 0x1234}, {DS_RNG: le32(0x12345678)}),
@@ -6822,7 +7466,7 @@ SPECS = [
         Case("d0", {}, {DS_1078FC: b"\x00"}),
         Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
     ], eax_mask=0),
-] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + C3B_SPECS + P7_SPECS + P8_SPECS + C3C_SPECS + C3D_SPECS + C3E_SPECS + C3F_SPECS
+] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + C3B_SPECS + P7_SPECS + P8_SPECS + C3C_SPECS + C3D_SPECS + C3E_SPECS + C3F_SPECS + C3G_SPECS
 
 
 # ---- driver -------------------------------------------------------------------------------------
diff --git a/tools/tests/test_diff_verify.py b/tools/tests/test_diff_verify.py
index 662f0a0..556e825 100644
--- a/tools/tests/test_diff_verify.py
+++ b/tools/tests/test_diff_verify.py
@@ -1560,6 +1560,343 @@ C3F_KINDS = {
     "string_unlock@store": {'byte'},
 }
 
+C3G_MASKS = {
+    "hit_slot_seed": 0x0,
+    "hit_scan": 0xffffffff,
+    "fighter_38fec": 0x0,
+    "hit_reaction_drive": 0xff,
+    "fighter_38d90": 0x0,
+    "fighter_block_start": 0x0,
+    "actor_pset_point": 0x0,
+    "fighter_attack_consume": 0xff,
+    "fighter_37178": 0x0,
+    "hit_stance_ok": 0xffffffff,
+    "hit_gate": 0xff,
+    "hit_frame_desc": 0xffffffff,
+    "hit_reaction_a": 0xffffffff,
+    "hit_reaction_b": 0xffffffff,
+    "hit_reaction_allow": 0xffffffff,
+    "fighter_38c5c": 0x0,
+    "text_cursor_hold": 0x0,
+    "text_number_draw": 0x0,
+    "text_vertical_set": 0x0,
+}
+C3G_KINDS = {
+    "actor_pset_point@bdc": {'byte'},
+    "actor_pset_point@bit": {'byte', 'call #0'},
+    "actor_pset_point@cursor": {'call #0'},
+    "actor_pset_point@div": {'byte'},
+    "actor_pset_point@gte": {'byte'},
+    "actor_pset_point@ramp": {'byte'},
+    "actor_pset_point@rec3c": {'byte'},
+    "actor_pset_point@x0": {'byte'},
+    "actor_pset_point@x2": {'byte'},
+    "actor_pset_point@x44": {'byte'},
+    "actor_pset_point@ymask": {'byte'},
+    "fighter_37178@base": {'byte', 'call #0', 'call #0 memory'},
+    "fighter_37178@bound": {'byte', 'call #0', 'call #0 memory'},
+    "fighter_37178@call": {'call #0'},
+    "fighter_37178@dirs": {'call #0'},
+    "fighter_37178@facing": {'byte', 'call #0', 'call #0 memory'},
+    "fighter_37178@fe": {'byte', 'call #0 memory'},
+    "fighter_37178@s18": {'byte', 'call #0 memory'},
+    "fighter_37178@s43": {'byte', 'call #0 memory'},
+    "fighter_37178@s52": {'byte', 'call #0 memory'},
+    "fighter_37178@s54": {'byte', 'call #0 memory'},
+    "fighter_37178@s57": {'byte'},
+    "fighter_37178@x": {'byte', 'call #0', 'call #0 memory'},
+    "fighter_38d90@c5c": {'call #0', 'call #0 memory', 'call #1', 'call #2', 'call #3', 'call #4', 'call #5', 'call #6'},
+    "fighter_38d90@col": {'call #5'},
+    "fighter_38d90@id": {'byte', 'call #2 memory', 'call #3 memory', 'call #4 memory', 'call #5 memory', 'call #6 memory'},
+    "fighter_38d90@mode": {'call #4'},
+    "fighter_38d90@pct": {'call #6'},
+    "fighter_38d90@pctrow": {'call #5'},
+    "fighter_38d90@row": {'call #2'},
+    "fighter_38fec@call": {'call #0', 'call #1'},
+    "fighter_38fec@ch": {'call #0', 'call #1', 'call #2', 'call #3', 'call #4', 'call #5', 'call #6'},
+    "fighter_38fec@n": {'call #0', 'call #1', 'call #2', 'call #3', 'call #4', 'call #5'},
+    "fighter_38fec@stride": {'call #1'},
+    "fighter_38fec@table": {'call #0', 'call #1'},
+    "fighter_attack_consume@anim": {'call #1', 'call #2'},
+    "fighter_attack_consume@base": {'byte', 'call #1 memory', 'call #2 memory'},
+    "fighter_attack_consume@chain": {'byte', 'call #1', 'call #2'},
+    "fighter_attack_consume@clr": {'byte', 'call #0 memory', 'call #1 memory', 'call #2 memory'},
+    "fighter_attack_consume@cmd": {'byte', 'call #0', 'call #1', 'call #2', 'eax'},
+    "fighter_attack_consume@dir": {'byte'},
+    "fighter_attack_consume@gate": {'call #0', 'call #1', 'call #1 memory', 'call #2'},
+    "fighter_attack_consume@o40": {'byte', 'call #0', 'call #1', 'call #2', 'eax'},
+    "fighter_attack_consume@ret": {'eax'},
+    "fighter_attack_consume@row": {'byte', 'call #1 memory', 'call #2 memory'},
+    "fighter_attack_consume@s5f": {'byte', 'call #0 memory', 'call #1 memory', 'call #2 memory'},
+    "fighter_attack_consume@scan": {'call #0', 'call #1'},
+    "fighter_attack_consume@state": {'byte'},
+    "fighter_block_start@and": {'byte', 'call #0 memory', 'call #1 memory'},
+    "fighter_block_start@bound": {'byte', 'call #1 memory'},
+    "fighter_block_start@div": {'byte'},
+    "fighter_block_start@k": {'byte'},
+    "fighter_block_start@s52": {'byte'},
+    "fighter_block_start@s53": {'byte'},
+    "fighter_block_start@s60": {'call #1 memory'},
+    "fighter_block_start@s61": {'byte', 'call #1 memory'},
+    "fighter_block_start@s62": {'byte', 'call #1 memory'},
+    "fighter_block_start@tbl": {'byte'},
+    "fighter_block_start@tri": {'byte', 'call #1 memory'},
+    "hit_reaction_drive@allow": {'byte', 'call #2', 'eax'},
+    "hit_reaction_drive@apply": {'call #1', 'call #2', 'call #3'},
+    "hit_reaction_drive@bound": {'byte', 'call #1'},
+    "hit_reaction_drive@flash": {'byte', 'call #1 memory'},
+    "hit_reaction_drive@gate": {'byte', 'call #1', 'call #2', 'call #3', 'eax'},
+    "hit_reaction_drive@gateal": {'byte', 'call #1', 'eax'},
+    "hit_reaction_drive@hi": {'byte', 'call #1 memory', 'call #2 memory'},
+    "hit_reaction_drive@mode": {'call #1', 'call #2', 'call #3'},
+    "hit_reaction_drive@ret": {'eax'},
+    "hit_reaction_drive@sel": {'byte', 'call #1', 'call #2', 'call #2 memory'},
+    "hit_reaction_drive@store": {'byte', 'call #1 memory', 'call #2 memory'},
+    "hit_scan@al": {'call #1', 'eax'},
+    "hit_scan@bound": {'call #0', 'eax'},
+    "hit_scan@miss": {'eax'},
+    "hit_scan@phase": {'call #0', 'call #1', 'eax'},
+    "hit_scan@side": {'call #0', 'eax'},
+    "hit_scan@stride": {'call #0', 'call #1', 'eax'},
+    "hit_slot_seed@bytes": {'byte'},
+    "hit_slot_seed@clear": {'byte'},
+    "hit_slot_seed@field": {'byte'},
+    "hit_slot_seed@off": {'byte'},
+    "hit_slot_seed@phase": {'byte'},
+    "hit_slot_seed@scale": {'byte'},
+    "hit_stance_ok@e": {'eax'},
+    "hit_stance_ok@st2": {'eax'},
+    "hit_stance_ok@d": {'eax'},
+    "hit_stance_ok@lo": {'eax'},
+    "hit_stance_ok@entry": {'eax'},
+    "hit_stance_ok@char": {'eax'},
+    "hit_stance_ok@ret": {'eax'},
+    "hit_gate@bound": {'call #0', 'eax'},
+    "hit_gate@signed": {'call #0', 'eax'},
+    "hit_gate@off": {'call #0', 'eax'},
+    "hit_gate@ret": {'eax'},
+    "hit_gate@call": {'call #0', 'eax'},
+    "hit_gate@al": {'eax'},
+    "hit_frame_desc@s63": {'eax'},
+    "hit_frame_desc@side": {'eax'},
+    "hit_frame_desc@gt": {'eax'},
+    "hit_frame_desc@table": {'eax'},
+    "hit_frame_desc@entry": {'eax'},
+    "hit_frame_desc@char": {'eax'},
+    "hit_frame_desc@def": {'eax'},
+    "hit_frame_desc@mask": {'eax'},
+    "hit_reaction_a@st": {'call #0', 'eax'},
+    "hit_reaction_a@bit": {'call #0', 'eax'},
+    "hit_reaction_a@side": {'call #0'},
+    "hit_reaction_a@table": {'call #0'},
+    "hit_reaction_a@idx": {'call #0'},
+    "hit_reaction_a@ret": {'eax'},
+    "hit_reaction_a@geom": {'call #0', 'eax'},
+    "hit_reaction_b@st": {'call #0', 'eax'},
+    "hit_reaction_b@bit": {'call #0', 'eax'},
+    "hit_reaction_b@side": {'call #0'},
+    "hit_reaction_b@table": {'call #0'},
+    "hit_reaction_b@idx": {'call #0'},
+    "hit_reaction_b@ret": {'eax'},
+    "hit_reaction_b@geom": {'call #0', 'eax'},
+    "hit_reaction_allow@side": {'eax'},
+    "hit_reaction_allow@char": {'eax'},
+    "hit_reaction_allow@off": {'eax'},
+    "hit_reaction_allow@shift": {'eax'},
+    "hit_reaction_allow@d0": {'eax'},
+    "hit_reaction_allow@d2": {'eax'},
+    "hit_reaction_allow@inv": {'eax'},
+    "hit_reaction_allow@z": {'eax'},
+    "fighter_38c5c@c": {'call #0', 'call #1', 'call #2', 'call #3'},
+    "fighter_38c5c@two": {'call #0'},
+    "fighter_38c5c@r9": {'call #1'},
+    "fighter_38c5c@r10": {'call #2'},
+    "fighter_38c5c@s2": {'call #0'},
+    "fighter_38c5c@s6": {'call #1'},
+    "fighter_38c5c@s3": {'call #2'},
+    "fighter_38c5c@mode": {'call #0', 'call #2', 'call #3'},
+    "fighter_38c5c@col4": {'call #3'},
+    "fighter_38c5c@n4": {'call #3'},
+    "text_cursor_hold@save": {'byte'},
+    "text_cursor_hold@str": {'call #0'},
+    "text_cursor_hold@args": {'call #0'},
+    "text_cursor_hold@mode": {'call #0'},
+    "text_number_draw@val": {'call #0'},
+    "text_number_draw@pad": {'call #0'},
+    "text_number_draw@mode": {'call #1'},
+    "text_number_draw@save": {'byte'},
+    "text_number_draw@fmt": {'call #0', 'call #1'},
+    "text_number_draw@col": {'call #1'},
+    "text_number_draw@row": {'call #1'},
+    "text_vertical_set@reload": {'byte', 'call #0'},
+    "text_vertical_set@neg": {'byte', 'call #1'},
+    "text_vertical_set@wargs": {'call #0'},
+    "text_vertical_set@skip": {'byte', 'call #0', 'call #1'},
+    "text_vertical_set@vert": {'call #0', 'call #1'},
+    "text_vertical_set@mode": {'call #0', 'call #1'},
+    "text_vertical_set@ext": {'byte'},
+    "text_vertical_set@row": {'byte'},
+    "text_vertical_set@sext": {'call #0'},
+}
+C3G_CASES = [
+        ("actor_pset_point@bdc", ['q0', 'q1', 'q2', 'q3', 'q4', 'q5', 'q6', 'q7']),
+        ("actor_pset_point@bit", ['q1', 'q2', 'q3', 'q6', 'q7']),
+        ("actor_pset_point@cursor", ['q1', 'q7']),
+        ("actor_pset_point@div", ['q3']),
+        ("actor_pset_point@gte", ['q6', 'q7']),
+        ("actor_pset_point@ramp", ['q6', 'q7']),
+        ("actor_pset_point@rec3c", ['q0', 'q1', 'q2', 'q3', 'q4', 'q5', 'q6', 'q7']),
+        ("actor_pset_point@x0", ['q0', 'q4', 'q5']),
+        ("actor_pset_point@x2", ['q0', 'q4', 'q5']),
+        ("actor_pset_point@x44", ['q6', 'q7']),
+        ("actor_pset_point@ymask", ['q6']),
+        ("fighter_37178@base", ['r0', 'r3']),
+        ("fighter_37178@bound", ['r13']),
+        ("fighter_37178@call", ['r3']),
+        ("fighter_37178@dirs", ['r10', 'r4', 'r5', 'r7']),
+        ("fighter_37178@facing", ['r3', 'r4', 'r5']),
+        ("fighter_37178@fe", ['r0', 'r3']),
+        ("fighter_37178@s18", ['r3']),
+        ("fighter_37178@s43", ['r1', 'r11', 'r12', 'r13', 'r2', 'r6', 'r8', 'r9']),
+        ("fighter_37178@s52", ['r3']),
+        ("fighter_37178@s54", ['r0', 'r1', 'r10', 'r11', 'r12', 'r13', 'r2', 'r3', 'r4', 'r5', 'r6', 'r7', 'r8', 'r9']),
+        ("fighter_37178@s57", ['r10', 'r4', 'r5', 'r7']),
+        ("fighter_37178@x", ['r12', 'r3']),
+        ("fighter_38d90@c5c", ['s0', 's1', 's2']),
+        ("fighter_38d90@col", ['s0', 's1']),
+        ("fighter_38d90@id", ['s0', 's1', 's2']),
+        ("fighter_38d90@mode", ['s0', 's1', 's2']),
+        ("fighter_38d90@pct", ['s0', 's1']),
+        ("fighter_38d90@pctrow", ['s0', 's1']),
+        ("fighter_38d90@row", ['s0', 's1', 's2']),
+        ("fighter_38fec@call", ['f1', 'f2', 'f3', 'f5']),
+        ("fighter_38fec@ch", ['f0', 'f1', 'f2', 'f3', 'f4']),
+        ("fighter_38fec@n", ['f0', 'f2', 'f4', 'f5']),
+        ("fighter_38fec@stride", ['f2']),
+        ("fighter_38fec@table", ['f1', 'f2', 'f3', 'f5']),
+        ("fighter_attack_consume@anim", ['a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_attack_consume@base", ['a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_attack_consume@chain", ['a2', 'a3', 'a5', 'a6', 'a7']),
+        ("fighter_attack_consume@clr", ['a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_attack_consume@cmd", ['a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_attack_consume@dir", ['a3', 'a4', 'a7']),
+        ("fighter_attack_consume@gate", ['a4']),
+        ("fighter_attack_consume@o40", ['a0']),
+        ("fighter_attack_consume@ret", ['a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_attack_consume@row", ['a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_attack_consume@s5f", ['a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_attack_consume@scan", ['a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_attack_consume@state", ['a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_block_start@and", ['b0', 'b1', 'b2', 'b3', 'b4', 'b5', 'b6']),
+        ("fighter_block_start@bound", ['b2']),
+        ("fighter_block_start@div", ['b6']),
+        ("fighter_block_start@k", ['b3']),
+        ("fighter_block_start@s52", ['b0', 'b1', 'b2', 'b3', 'b4', 'b5', 'b6']),
+        ("fighter_block_start@s53", ['b0', 'b1', 'b2', 'b3', 'b4', 'b5', 'b6']),
+        ("fighter_block_start@s60", ['b1']),
+        ("fighter_block_start@s61", ['b0', 'b1', 'b2', 'b3', 'b4', 'b5', 'b6']),
+        ("fighter_block_start@s62", ['b0', 'b1', 'b2', 'b3', 'b4', 'b5', 'b6']),
+        ("fighter_block_start@tbl", ['b0', 'b1', 'b2', 'b5', 'b6']),
+        ("fighter_block_start@tri", ['b0', 'b2', 'b3', 'b4', 'b5', 'b6']),
+        ("hit_reaction_drive@allow", ['d11']),
+        ("hit_reaction_drive@apply", ['d3', 'd4', 'd5', 'd6', 'd7', 'd8', 'd9']),
+        ("hit_reaction_drive@bound", ['d1']),
+        ("hit_reaction_drive@flash", ['d9']),
+        ("hit_reaction_drive@gate", ['d0', 'd1', 'd10', 'd11', 'd2', 'd3', 'd4', 'd5', 'd6', 'd7', 'd8', 'd9']),
+        ("hit_reaction_drive@gateal", ['d10']),
+        ("hit_reaction_drive@hi", ['d4', 'd5', 'd6', 'd7', 'd8', 'd9']),
+        ("hit_reaction_drive@mode", ['d3']),
+        ("hit_reaction_drive@ret", ['d3', 'd4', 'd5', 'd6', 'd7', 'd8', 'd9']),
+        ("hit_reaction_drive@sel", ['d3', 'd4', 'd5', 'd8']),
+        ("hit_reaction_drive@store", ['d4', 'd5', 'd6', 'd7', 'd9']),
+        ("hit_scan@al", ['t7']),
+        ("hit_scan@bound", ['t4', 't5']),
+        ("hit_scan@miss", ['t2', 't3', 't6', 't7']),
+        ("hit_scan@phase", ['t0', 't1', 't2', 't4', 't5', 't7']),
+        ("hit_scan@side", ['t5']),
+        ("hit_scan@stride", ['t1', 't2', 't4', 't5', 't7']),
+        ("hit_slot_seed@bytes", ['g0', 'g2', 'g3']),
+        ("hit_slot_seed@clear", ['g0', 'g1', 'g2', 'g3']),
+        ("hit_slot_seed@field", ['g0', 'g1', 'g2', 'g3']),
+        ("hit_slot_seed@off", ['g1', 'g3']),
+        ("hit_slot_seed@phase", ['g0', 'g2', 'g3']),
+        ("hit_slot_seed@scale", ['g0', 'g2', 'g3']),
+        ("hit_stance_ok@e", ['k0', 'k1']),
+        ("hit_stance_ok@st2", ['k0', 'k3', 'k4']),
+        ("hit_stance_ok@d", ['k2', 'k5', 'k6']),
+        ("hit_stance_ok@lo", ['k5']),
+        ("hit_stance_ok@entry", ['k1', 'k8']),
+        ("hit_stance_ok@char", ['k2', 'k3', 'k5']),
+        ("hit_stance_ok@ret", ['k1', 'k4', 'k6', 'k7', 'k8']),
+        ("hit_gate@bound", ['g1', 'g7']),
+        ("hit_gate@signed", ['g3', 'g4']),
+        ("hit_gate@off", ['g0', 'g2', 'g3', 'g4', 'g5', 'g6', 'g9']),
+        ("hit_gate@ret", ['g1', 'g7', 'g8']),
+        ("hit_gate@call", ['g0', 'g2', 'g3', 'g4', 'g5', 'g6', 'g9']),
+        ("hit_gate@al", ['g9']),
+        ("hit_frame_desc@s63", ['d0']),
+        ("hit_frame_desc@side", ['d4']),
+        ("hit_frame_desc@gt", ['d3']),
+        ("hit_frame_desc@table", ['d0', 'd1', 'd2', 'd3', 'd4']),
+        ("hit_frame_desc@entry", ['d0', 'd1', 'd2', 'd3', 'd4']),
+        ("hit_frame_desc@char", ['d0', 'd1', 'd2', 'd3', 'd4']),
+        ("hit_frame_desc@def", ['d5', 'd7']),
+        ("hit_frame_desc@mask", ['d1', 'd2', 'd3', 'd4']),
+        ("hit_reaction_a@st", ['a0', 'a5', 'a7']),
+        ("hit_reaction_a@bit", ['a1', 'a6']),
+        ("hit_reaction_a@side", ['a2', 'a3', 'a4']),
+        ("hit_reaction_a@table", ['a2', 'a3', 'a4']),
+        ("hit_reaction_a@idx", ['a2', 'a3', 'a4']),
+        ("hit_reaction_a@ret", ['a2', 'a3', 'a4']),
+        ("hit_reaction_a@geom", ['a2', 'a3', 'a4']),
+        ("hit_reaction_b@st", ['b0', 'b5']),
+        ("hit_reaction_b@bit", ['b1', 'b6']),
+        ("hit_reaction_b@side", ['b2', 'b3', 'b4']),
+        ("hit_reaction_b@table", ['b2', 'b3', 'b4']),
+        ("hit_reaction_b@idx", ['b2', 'b3', 'b4']),
+        ("hit_reaction_b@ret", ['b2', 'b3', 'b4']),
+        ("hit_reaction_b@geom", ['b2', 'b3', 'b4']),
+        ("hit_reaction_allow@side", ['r14', 'r7', 'r8']),
+        ("hit_reaction_allow@char", ['r0', 'r1', 'r10', 'r11', 'r14', 'r15', 'r16', 'r2', 'r3', 'r5', 'r6', 'r7', 'r8', 'r9']),
+        ("hit_reaction_allow@off", ['r10', 'r14', 'r16', 'r5', 'r6']),
+        ("hit_reaction_allow@shift", ['r11', 'r15', 'r16', 'r2', 'r3', 'r4', 'r5', 'r6', 'r8', 'r9']),
+        ("hit_reaction_allow@d0", ['r3']),
+        ("hit_reaction_allow@d2", ['r7']),
+        ("hit_reaction_allow@inv", ['r0', 'r1', 'r10', 'r11', 'r14', 'r15', 'r16', 'r2', 'r3', 'r4', 'r5', 'r6', 'r7', 'r8', 'r9']),
+        ("hit_reaction_allow@z", ['r12', 'r13']),
+        ("fighter_38c5c@c", ['s1']),
+        ("fighter_38c5c@two", ['s0', 's1']),
+        ("fighter_38c5c@r9", ['s0', 's1']),
+        ("fighter_38c5c@r10", ['s0', 's1']),
+        ("fighter_38c5c@s2", ['s0', 's1']),
+        ("fighter_38c5c@s6", ['s0', 's1']),
+        ("fighter_38c5c@s3", ['s0', 's1']),
+        ("fighter_38c5c@mode", ['s0', 's1']),
+        ("fighter_38c5c@col4", ['s0', 's1']),
+        ("fighter_38c5c@n4", ['s0', 's1']),
+        ("text_cursor_hold@save", ['h0', 'h1']),
+        ("text_cursor_hold@str", ['h0', 'h1']),
+        ("text_cursor_hold@args", ['h0']),
+        ("text_cursor_hold@mode", ['h0']),
+        ("text_number_draw@val", ['n0', 'n1']),
+        ("text_number_draw@pad", ['n0', 'n1']),
+        ("text_number_draw@mode", ['n1']),
+        ("text_number_draw@save", ['n0', 'n1']),
+        ("text_number_draw@fmt", ['n0', 'n1']),
+        ("text_number_draw@col", ['n0']),
+        ("text_number_draw@row", ['n0']),
+        ("text_vertical_set@reload", ['v3', 'v4', 'v5']),
+        ("text_vertical_set@neg", ['v2']),
+        ("text_vertical_set@wargs", ['v1', 'v2']),
+        ("text_vertical_set@skip", ['v1', 'v2']),
+        ("text_vertical_set@vert", ['v0', 'v1', 'v2', 'v3', 'v4', 'v5']),
+        ("text_vertical_set@mode", ['v1', 'v2', 'v3', 'v4', 'v5']),
+        ("text_vertical_set@ext", ['v0', 'v1', 'v2', 'v3', 'v5']),
+        ("text_vertical_set@row", ['v0', 'v1', 'v2', 'v3', 'v4', 'v5']),
+        ("text_vertical_set@sext", ['v5']),
+]
+
+
 
 @needs_unicorn
 @unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
@@ -1585,7 +1922,7 @@ class RealFunctionTests(unittest.TestCase):
                                              "fighter_slot_flag", "hit_anim_ctx", "hit_anim_start_b",
                                              "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)
                                             + list(P3_MASKS) + list(P45_MASKS) + list(C1_MASKS)
-                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(C3_MASKS) + list(C3B_MASKS) + list(C3C_MASKS) + list(C3D_MASKS) + list(C3E_MASKS) + list(C3F_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
+                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(C3_MASKS) + list(C3B_MASKS) + list(C3C_MASKS) + list(C3D_MASKS) + list(C3E_MASKS) + list(C3F_MASKS) + list(C3G_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
         for name, r in self.real.items():
             if name == "host_1b890":       # the named gap (record E3 §E3.8), tested on its own below
                 continue
@@ -1601,7 +1938,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_45878@mutant", "fighter_ctx_same@mutant", "fighter_slot_flag@mutant",
             "hit_anim_ctx@mutant", "hit_anim_start_b@mutant", "hit_anim_start_b@set", "rng_next@mutant"]
             + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS) + list(P45_KINDS) + list(C1_KINDS)
-            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(C3_KINDS) + list(C3B_KINDS) + list(C3C_KINDS) + list(C3D_KINDS) + list(C3E_KINDS) + list(C3F_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
+            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(C3_KINDS) + list(C3B_KINDS) + list(C3C_KINDS) + list(C3D_KINDS) + list(C3E_KINDS) + list(C3F_KINDS) + list(C3G_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
         for name, r in self.mut.items():
             self.assertEqual(r.verdict, "MISMATCH", name)
 
@@ -1666,7 +2003,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_3640c": 0, "fighter_37dcc": 0,
             "fighter_23130": 0xFF, "fighter_45878": 0, "anim_10fa8": 0, "anim_3e4e4": 0,
             "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
-            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS, **C3_MASKS, **C3B_MASKS, **C3C_MASKS, **C3D_MASKS, **C3E_MASKS, **C3F_MASKS,
+            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS, **C3_MASKS, **C3B_MASKS, **C3C_MASKS, **C3D_MASKS, **C3E_MASKS, **C3F_MASKS, **C3G_MASKS,
             **P7_MASKS, **P8_MASKS})
         # with the full mask the slot-flag original's scratch bits (case f9: EAX = 0x201) differ
         spec = dataclasses.replace([s for s in V.SPECS if s.name == "fighter_slot_flag"][0],
@@ -2581,6 +2918,16 @@ class RealFunctionTests(unittest.TestCase):
         ):
             self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
 
+    def test_each_c3g_mutant_is_caught_by_what_it_breaks(self):
+        # track P batch C3g (record 2026-10-05-reverse-c3g): what alone catches each mutant
+        for name, want in C3G_KINDS.items():
+            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
+                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
+            self.assertEqual(got, want, name)
+        # the exact case set that alone catches each mutant (measured on the prototype)
+        for name, ids in C3G_CASES:
+            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
+
     def test_each_stub_declares_the_registers_its_callee_clobbers(self):
         # Call.clobbers, re-derived from the bytes (record §E3.5's table, §E3.12)
         img = E.Image.load(os.path.join(self.tmp.name, "image.bin"))
@@ -2611,9 +2958,15 @@ class RealFunctionTests(unittest.TestCase):
         0x3B080: ("ebx", "ecx", "edx"), 0x3B134: ("ebx", "edx", "edi", "ebp"), 0x3B298: ("edx",), 0x3B6C4: (),
         0x3B714: ("edx",), 0x3B8D8: ("edx",), 0x3B90C: ("edx",), 0x3BDB0: (), 0x3BDDC: ("ebp",), 0x3C148: (),
         0x3C16C: (), 0x3C190: ("edx",), 0x3C208: ("edx",), 0x3C358: (), 0x3C480: ("edx",), 0x3C4CC: ("edx",),
-        0x3C520: ("edx",), 0x3C59C: ("edx",), 0x32BAC: (), 0x3C6A8: ("edx",), 0x3CD44: (), 0x3CE58: ("edx", "edi", "ebp"), 0x41310: (), 0x46190: (), 0x46460: ("edx",), 0x468D8: (), 0x474E4: ("ebx", "edx"), 0x48170: (),
+        0x3C520: ("edx",), 0x3C59C: ("edx",), 0x32BAC: (), 0x3C6A8: ("edx",), 0x3CD44: (), 0x3CE58: ("edx", "edi", "ebp"),
+        0x34E2C: ("edx", "edi", "ebp"), 0x38ED0: ("edx",), 0x38C5C: (), 0x2F4BC: ("ebx", "ecx", "edx"),
+        0x2F4D0: ("ebx", "ecx", "edx"), 0x2F20C: ("ebx", "ecx", "edx"), 0x1A6AC: ("edx",), 0x3C600: (),
+        0x3CCEC: ("edx",), 0x3CE24: ("edx",), 0x4CE70: (), 0x3CBC4: (), 0x3CC58: (), 0x3CF38: (), 0x4649C: ("ebx", "edx"),
+        0x41310: (), 0x46190: (), 0x46460: ("edx",), 0x468D8: (), 0x474E4: ("ebx", "edx"), 0x48170: (),
         0x49444: ("edi", "ebp"), 0x4F434: (), 0x5D7DC: (), 0x5DC0F: ("ebx", "ecx", "edx"),
-        0x5DC8B: ("ebx", "ecx", "edx"), 0x5DD03: (), 0x5DEAF: ("ebx", "ecx", "edx"), 0x5DEED: ()})
+        0x5DC8B: ("ebx", "ecx", "edx"), 0x5DD03: (), 0x5DEAF: ("ebx", "ecx", "edx"), 0x5DEED: (),
+        0x3CD94: ("edx",), 0x2F198: ("ebx", "ecx", "edx"), 0x2F280: ("ebx", "ecx", "edx"),
+        0x2F314: ("ebx", "ecx", "edx")})
         for addr, declared in stubs.items():
             self.assertEqual(E.callee_clobbers(img, addr), declared, hex(addr))
 
@@ -2708,8 +3061,8 @@ class RealFunctionTests(unittest.TestCase):
                          "--self-check"])
         self.assertEqual(rc, 0)
         # the closed-row count is over the rows that have callees (225), the 66 without are counted apart
-        self.assertIn("diff-verify: 291/291 functions VERIFIED; 1087/1087 mutants detected; 1 named gaps; "
-                      "205/225 rows with callees closed (66 have none).", out.getvalue())
+        self.assertIn("diff-verify: 310/310 functions VERIFIED; 1242/1242 mutants detected; 1 named gaps; "
+                      "217/241 rows with callees closed (69 have none).", out.getvalue())
 
 
     def test_each_c3c_mutant_is_caught_by_what_it_breaks(self):
```
