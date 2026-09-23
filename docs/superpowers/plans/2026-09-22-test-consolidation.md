# Test Consolidation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove four redundancies from `port/tests/` — the triple test registration, the runner boilerplate, the duplicated per-file fixtures, and the repeated in-file setups — **without changing a single assertion.**

**Architecture:** A single registry (an X-macro list in `test.h`) replaces the declarations, the calls and the CMake list; a new `test_fixtures.{h,c}` becomes the shared home for the per-file fixtures; named reset helpers replace the repeated seeding inside `test_fight.c` and `test_effects.c`; `test_scaffold.c` and the runner's four near-identical blocks go.

**Tech Stack:** C, CMake (`file(GLOB CONFIGURE_DEPENDS)`), the existing `CHECK`/`CHECK_EQ_INT` macros.

**Spec:** `docs/superpowers/specs/2026-09-22-test-consolidation-design.md`. Read it first.

## Global Constraints

- **No assertion changes.** Every `CHECK`/`CHECK_EQ_INT` keeps its condition, message and order. The `CHECK`/`CHECK_EQ_INT` count per file is **identical before and after** — this is the change's primary gate. Baseline: `test_fight.c` 359, `test_effects.c` 210, `test_sprite.c` 137, `test_attract.c` 101, `test_anim.c` 100, `test_frontend.c` 87, `test_flow.c` 86, `test_actors.c` 74, `test_sequencer.c` 71, `test_gra.c` 65, `test_render.c` 62, `test_res.c` 61, `test_config.c` 51, `test_ail.c` 48, `test_input.c` 33, `test_smacker.c` 32, `test_text.c` 30, `test_gfx.c` 27, `test_mixer.c` 24, `test_host.c` 22, `test_samples.c` 20, `test_pitch.c` 17, `test_mem.c` 16, `test_le.c` 11, `test_movie.c` 9, `test_rng.c` 9, `test_title.c` 8, `test_opl.c` 5, `test_scaffold.c` 2 (deleted — its two assertions leave with it). **Suite total 1777.**
- **A moved fixture keeps its body byte-for-byte.** Only its home and its callers change.
- **No production code changes.** `port/src` is untouched.
- **0 warnings**, no new dependency, `data/` read-only. Never `git add -A`. Commit style: `<area>: <what changed>`.
- **`make verify` is the ladder** and must exit 0 with every claim unmoved: title `54 clean, 55 splice, 2 transition, 0 unexplained` and `54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at capture frame 215`; front-end `[560..830]`/271 `0 unexplained`; smk `120/120` + `41/41`; the C-vs-Python byte-exact claim.
- Each env-gated driver still runs **alone**: `PR_TITLE_DUMP`, `PR_ATTRACT_DUMP`, `PR_FRONTEND_DUMP`, `PR_FRONTEND_DET`.
- **Commit incrementally** — one coherent step per commit as soon as it builds and the suite passes. Sessions die; a death must cost at most the current step.

---

### Task 1: The single registry

**Files:**
- Modify: `port/tests/test.h`
- Modify: `port/tests/run_tests.c`
- Modify: `port/CMakeLists.txt` (the `run_tests` target's sources)
- Delete: `port/tests/test_scaffold.c`

**Interfaces:**
- Consumes: the current 29 unit tests and 4 gated drivers.
- Produces: `TEST_CASES(X)` and `TEST_DRIVERS(X)` in `test.h`, expanded by both `test.h` and `run_tests.c`.

- [ ] **Step 1: Add the X-macro lists to `test.h`**

Replace the block of 30 `int test_X(void);` declarations with the two lists plus the expansions. The driver's second field is its env var, or `NULL` for a driver with no gate (`test_title` is a no-op without `PR_TITLE_DUMP`, so it stays a case too — keep the current behaviour):

```c
/* One line per unit test; adding a test is one line here. */
#define TEST_CASES(X)   \
    X(test_mem)         \
    X(test_le)          \
    X(test_res)         \
    X(test_gra)         \
    X(test_gfx)         \
    X(test_input)       \
    X(test_host)        \
    X(test_flow)        \
    X(test_opl)         \
    X(test_pitch)       \
    X(test_samples)     \
    X(test_mixer)       \
    X(test_sequencer)   \
    X(test_ail)         \
    X(test_smacker)     \
    X(test_movie)       \
    X(test_sprite)      \
    X(test_render)      \
    X(test_rng)         \
    X(test_actors)      \
    X(test_effects)     \
    X(test_fight)       \
    X(test_config)      \
    X(test_frontend)    \
    X(test_attract)     \
    X(test_anim)        \
    X(test_text)        \
    X(test_title)

/* One line per env-gated driver; it runs alone, before the unit cases. */
#define TEST_DRIVERS(X)                                    \
    X(test_frontend_determinism, "PR_FRONTEND_DET")        \
    X(test_attract,              "PR_ATTRACT_DUMP")        \
    X(test_title,                "PR_TITLE_DUMP")          \
    X(test_frontend,             "PR_FRONTEND_DUMP")

#define TEST_DECLARE(name) int name(void);
TEST_CASES(TEST_DECLARE)
TEST_DRIVERS(TEST_DECLARE)
#undef TEST_DECLARE
```

Keep `test_title_window` and any other non-registry declarations as they are.

- [ ] **Step 2: Rewrite `run_tests.c` to expand the lists**

Replace the four gated `if (getenv(...))` blocks with one table-driven loop, and the 28 explicit calls with the `TEST_CASES` expansion. Preserve the existing comments' meaning (the drivers run alone because `game_init()` may run once per process; `PR_FRONTEND_DET` must come first and run nothing else):

```c
static const struct { const char *env; int (*fn)(void); } k_drivers[] = {
#define DRIVER_ROW(name, env) { env, name },
    TEST_DRIVERS(DRIVER_ROW)
#undef DRIVER_ROW
};

int main(int argc, char **argv)
{
    (void)argc;
    for (unsigned i = 0; i < sizeof k_drivers / sizeof k_drivers[0]; i++) {
        const char *v = getenv(k_drivers[i].env);
        if (v != NULL && v[0] != '\0') {
            k_drivers[i].fn();
            printf(g_failures ? "FAILURES: %d\n" : "all checks passed\n", g_failures);
            return g_failures != 0;
        }
    }
#define CASE_RUN(name) name();
    TEST_CASES(CASE_RUN)
#undef CASE_RUN
    printf(g_failures ? "FAILURES: %d\n" : "all checks passed\n", g_failures);
    return g_failures != 0;
}
```

`test_frontend_determinism` needs `argv[0]`, so special-case it before the loop (or give the table an optional `argc/argv` field) — keep the current order: the determinism gate first, then the other three, then the cases.

- [ ] **Step 3: Update the CMake target**

In `port/CMakeLists.txt`, replace the explicit test source list with:

```cmake
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/test_*.c)
add_executable(run_tests tests/run_tests.c ${TEST_SOURCES})
```

`CONFIGURE_DEPENDS` re-globs at build time, so a new `tests/test_*.c` needs no CMake edit.

- [ ] **Step 4: Delete the stub**

`git rm port/tests/test_scaffold.c`. Its two assertions leave the suite total (1777 → 1775); no other file's count may move.

- [ ] **Step 5: Build, count, and run the ladder**

```bash
cmake --build build
rg -c "CHECK|CHECK_EQ_INT" port/tests/*.c   # compare to the Global Constraints baseline
./build/run_tests
make verify
```

Expected: every per-file count identical to the baseline (minus `test_scaffold.c`), `./build/run_tests` prints `all checks passed`, `make verify` exits 0 with every claim unmoved. Then confirm each gated driver still selects only itself: `PR_TITLE_DUMP=/tmp/t ./build/run_tests` runs the title driver alone.

- [ ] **Step 6: Commit**

```bash
git add port/tests/test.h port/tests/run_tests.c port/CMakeLists.txt
git rm port/tests/test_scaffold.c
git commit -m "tests: one registry for the suite"
```

**Gate for this task:** one place registers a test; the per-file assertion counts are unchanged; `make verify` is green.

---

### Task 2: The shared fixtures

**Files:**
- Create: `port/tests/test_fixtures.h`, `port/tests/test_fixtures.c`
- Modify: `port/tests/test_fight.c`, `port/tests/test_effects.c`, `port/tests/test_anim.c`, `port/tests/test_frontend.c`

**Interfaces:**
- Consumes: the per-file fixtures as they exist today.
- Produces: `test_fixtures.h` declaring them; `test_fixtures.c` defining them. `TEST_CASES`'s CMake glob already compiles `test_fixtures.c` (it matches `test_*.c`).

- [ ] **Step 1: Move the fixtures, bodies unchanged**

Create `test_fixtures.{h,c}` and move each fixture **with its body copied exactly**:

| today | new name |
|---|---|
| `test_fight.c`'s `snap`/`put` | `tf_snap`/`tf_put` |
| `test_fight.c`'s `demo_fixture` | `tf_demo_fixture` |
| `test_fight.c`'s `hit_fixture` | `tf_hit_fixture` |
| `test_effects.c`'s `fixture_begin` | `tf_effects_fixture_begin` |
| `test_anim.c`'s `anim_alloc_record` | `tf_anim_alloc_record` |
| `test_anim.c`'s `anim_spawn_stream` | `tf_anim_spawn_stream` |
| `test_frontend.c`'s `seed_frontend_list`/`restore_frontend_list` | `tf_frontend_seed_list`/`tf_frontend_restore_list` |

Each file includes `test_fixtures.h` and calls the new names; the old definitions are removed. **No body is edited** — a byte-for-byte copy.

- [ ] **Step 2: Build and verify**

```bash
cmake --build build
rg -c "CHECK|CHECK_EQ_INT" port/tests/*.c   # identical to Task 1's counts
./build/run_tests && make verify
```

Expected: counts identical; `all checks passed`; `make verify` exit 0, every claim unmoved.

- [ ] **Step 3: Commit**

```bash
git add port/tests/test_fixtures.h port/tests/test_fixtures.c port/tests/test_fight.c port/tests/test_effects.c port/tests/test_anim.c port/tests/test_frontend.c
git commit -m "tests: one home for the shared fixtures"
```

**Gate for this task:** the fixtures live in one place, their bodies are unchanged, and the counts and ladder hold.

---

### Task 3: The in-file setups in `test_fight.c`

**Files:**
- Modify: `port/tests/test_fight.c`

**Interfaces:**
- Consumes: Task 2's `tf_hit_fixture` etc.
- Produces: named reset helpers the checks call.

- [ ] **Step 1: Identify the repeated sequences**

Read `test_fight.c` and list each sequence of `mem_fill(...)` + `DS_...` seeding that appears in more than one check (the slot arrays at `DS_00107D58`, the record array at `FIGHT_RECS`, the actor array at `FIGHT_ACTORS`, the `DS_001077B0` bases). Record each sequence and the checks that repeat it.

- [ ] **Step 2: Extract them as helpers**

Add one helper per distinct sequence — e.g. `fight_reset_slots()`, `fight_reset_recs()`, `fight_reset_actors()`, `fight_reset_bases()` — each containing **exactly** the repeated lines, in the same order. Replace the repetitions at the call sites.

- [ ] **Step 3: Build, count, and verify**

```bash
cmake --build build
rg -c "CHECK|CHECK_EQ_INT" port/tests/test_fight.c   # must still be 359
./build/run_tests && make verify
```

Expected: 359; `all checks passed`; `make verify` exit 0, every claim unmoved. **If any assertion count moves, revert and re-read the sequence** — a lost line is a lost assertion.

- [ ] **Step 4: Commit**

```bash
git add port/tests/test_fight.c
git commit -m "tests: name the fight suite's repeated setups"
```

**Gate for this task:** `test_fight.c`'s assertion count is still 359 and the ladder holds.

---

### Task 4: The in-file setups in `test_effects.c`

**Files:**
- Modify: `port/tests/test_effects.c`

**Interfaces:**
- Consumes: Task 2's `tf_effects_fixture_begin`.
- Produces: the pool-reset helper.

- [ ] **Step 1: Identify and extract**

The file repeats `mem_fill(src, 0, 0x40u)` / `mem_fill(src, 0, 0x300u)` before most spawns and the pool seeding at `DS_000FCCE0`. Extract `effects_reset_pool()` and `effects_reset_source(u32 size)` containing exactly the repeated lines, and replace the repetitions.

- [ ] **Step 2: Build, count, and verify**

```bash
cmake --build build
rg -c "CHECK|CHECK_EQ_INT" port/tests/test_effects.c   # must still be 210
./build/run_tests && make verify
```

Expected: 210; `all checks passed`; `make verify` exit 0, every claim unmoved.

- [ ] **Step 3: Commit**

```bash
git add port/tests/test_effects.c
git commit -m "tests: name the effects suite's repeated setups"
```

**Gate for this task:** `test_effects.c`'s assertion count is still 210 and the ladder holds.

---

### Task 5: AGENTS.md and the close-out

**Files:**
- Modify: `AGENTS.md`

- [ ] **Step 1: Update the Tests section**

Replace the stale registration rule with the single-source rule, and record the fixtures' home and the no-assertion-change rule:

```markdown
- One `int test_X(void)` per file. **Register it once** — add `X(test_foo)` to
  `TEST_CASES` in `port/tests/test.h`; the declarations, the run order and the
  build all follow from that one line (CMake globs `tests/test_*.c` with
  `CONFIGURE_DEPENDS`). Env-gated drivers go in `TEST_DRIVERS` with their env
  var; each runs alone before the unit cases.
- Shared test fixtures live in `port/tests/test_fixtures.{h,c}`. A fixture moved
  there keeps its body byte-for-byte; only its home and its callers change.
- **Consolidating tests must not change an assertion.** The
  `CHECK`/`CHECK_EQ_INT` count per file is the gate: it is identical before and
  after, and `make verify` stays green.
```

Keep the existing rules on `CHECK`/`CHECK_EQ_INT` only, `game_init()` once per process and env-gating, and assertions that can fail.

- [ ] **Step 2: Full ladder and commit**

```bash
make verify
git add AGENTS.md
git commit -m "docs: record the test registry and fixture rules"
```

Expected: exit 0, 0 warnings, every claim unmoved.

**Gate for this task:** AGENTS.md records the single-source registry, the fixtures' home, and the no-assertion-change rule.

---

## Self-Review

**Spec coverage.** The spec's four redundancies map one-to-one: the triple registration → Task 1; the runner boilerplate and the stub → Task 1; the per-file fixtures → Task 2; the in-file setups → Tasks 3 and 4. Its AGENTS.md item is Task 5. Its Verification section (the per-file assertion counts, `make verify`, the four drivers running alone, 0 warnings) is each task's Step with the counts and the ladder. Its Risks map to the task order (the registry first, the fixtures second, the largest files last on their own) and to the CMake fallback note.

**Placeholder scan.** No `TBD` or "similar to Task N". The registry snippets are complete; the fixtures' bodies are not reproduced because they are moved byte-for-byte from named lines, and each step says so. Every step has its command.

**Type consistency.** `TEST_CASES`/`TEST_DRIVERS`/`TEST_DECLARE`/`DRIVER_ROW`/`CASE_RUN` are used as defined in Task 1. The fixture names (`tf_*`) are introduced in Task 2 and used unchanged in Tasks 3–4. `fight_reset_*`/`effects_reset_*` are introduced where first needed.

**Right-sizing.** Task 1 is one mechanical change with the count gate. Task 2 is one move with the same gate. Tasks 3 and 4 are one file each, largest last, each with its own count gate — a reviewer can reject one without touching the others. Task 5 is the doc rule.
