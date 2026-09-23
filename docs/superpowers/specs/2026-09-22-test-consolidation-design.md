# Test consolidation (design)

**Context.** Cycle 3 (`combat-fidelity`) is complete through Task 5; its final
review and merge are pending. This is a maintenance change to `port/tests/`,
independent of the cycle.

## Goal

Remove four redundancies from the unit-test suite **without changing a single
assertion**. The gate is the suite itself:

- `make verify` exits 0 with every oracle claim unmoved, and
- the `CHECK`/`CHECK_EQ_INT` **count per file is unchanged** — that count is the
  proof that no assertion was lost, weakened, or reordered.

## Starting state

30 test files, 10396 lines. One `int test_X(void)` per file calls `check_*`
statics. Measured redundancy:

1. **Triple registration.** Each test is listed three times: declared in
   `port/tests/test.h` (30 declarations), called in `port/tests/run_tests.c`
   (28 calls), and named in `port/CMakeLists.txt`'s explicit source list.
   Adding a test means editing all three; a test declared but never called is
   silently dropped from the run.
2. **Runner boilerplate.** `run_tests.c` has four near-identical env-gated
   blocks (each: read the env var, call the driver, print the result, return).
   `test_scaffold.c` is an 8-line stub (`1 + 1 == 2`, `sizeof(void*) >= 4`).
3. **Per-file fixtures.** `test_fight.c`'s `hit_fixture`/`demo_fixture`,
   `test_effects.c`'s `fixture_begin`, `test_anim.c`'s `anim_alloc_record`/
   `anim_spawn_stream`, `test_frontend.c`'s `seed_frontend_list`/
   `restore_frontend_list`, and `test_fight.c`'s `snap`/`put` each re-seed
   overlapping globals with no shared home.
4. **In-file setups.** `test_fight.c` (359 assertions across ~40 checks) and
   `test_effects.c` (210) repeat the same `mem_fill(...)` + `DS_...` seeding
   inside each check.

## Scope

**In:** `port/tests/test.h`, `port/tests/run_tests.c`,
`port/CMakeLists.txt`'s test target, a new `port/tests/test_fixtures.{h,c}`,
and the bodies of `port/tests/test_*.c` where fixtures and setups move; and
`AGENTS.md`'s Tests section.

**Out:** every `CHECK`/`CHECK_EQ_INT` assertion — its condition, its message and
its order are unchanged. No production code. No test's coverage is added or
removed.

## Design

**1. One registry.** `test.h` defines an X-macro list:

```c
#define TEST_CASES(X)      \
    X(test_mem)            \
    X(test_le)             \
    /* … one line per unit test … */

#define TEST_DRIVERS(X)                       \
    X(test_frontend_determinism, "PR_FRONTEND_DET") \
    X(test_attract,              "PR_ATTRACT_DUMP")  \
    X(test_title,                "PR_TITLE_DUMP")    \
    X(test_frontend,             "PR_FRONTEND_DUMP")
```

`run_tests.c` expands `TEST_DRIVERS` into a table `{ env, fn }` and loops it,
then expands `TEST_CASES` into the unit calls; `test.h` expands the same lists
into the declarations. Adding a test is one line in one place. `CMakeLists.txt`
switches the test target's sources to
`file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/test_*.c)` plus
`tests/run_tests.c` — `CONFIGURE_DEPENDS` re-globs at build time, so a new file
needs no CMake edit. (This is the one deliberate departure from the repo's
"no globbing" rule, and the registry plus `CONFIGURE_DEPENDS` makes it safe.)

**2. Shared fixtures.** A new `port/tests/test_fixtures.{h,c}` holds the
fixtures above, each with its **current body byte-for-byte** — only the home
changes, so no fixture's behaviour can drift. The files that used them call the
shared names.

**3. Collapsed in-file setups.** The repeated seeding in `test_fight.c` and
`test_effects.c` becomes named reset helpers (`fight_reset_slots()`,
`fight_reset_recs()`, `effects_reset_pool()`), so each check reads
`reset; seed; act; assert`. A helper's body is the exact sequence the checks
repeat today.

**4. The stub and the boilerplate.** `test_scaffold.c` is deleted; its
`test_scaffold()` entry leaves the registry. `run_tests.c`'s four gated blocks
become the driver table.

**5. AGENTS.md.** The Tests section records: the registry (one place to add a
test), the fixtures' home, the **no-assertion-change rule**, and replaces the
stale "declared in `test.h`, called from `run_tests.c`, added to
`CMakeLists.txt`" triple with the single-source rule.

## Verification

- Before the change, record the `CHECK`/`CHECK_EQ_INT` count per file
  (`rg -c`); after, require it identical.
- `make verify` exits 0 with every claim unmoved: title `54 clean, 55 splice,
  2 transition, 0 unexplained` and `54 clean, 57 splice, 0 unexplained`;
  attract `FIRST DIVERGENCE at capture frame 215`; front-end `[560..830]`/271
  `0 unexplained`; smk `120/120` + `41/41`; the C-vs-Python byte-exact claim.
- Each env-gated driver still runs alone: `PR_TITLE_DUMP`, `PR_ATTRACT_DUMP`,
  `PR_FRONTEND_DUMP`, `PR_FRONTEND_DET` each select only their driver.
- 0 warnings; no new dependency.

## Risks

- **A fixture's subtle difference.** Moving a fixture to a shared home is only
  safe if its body is copied exactly; the per-file assertion counts and the
  oracle claims are the check.
- **The CMake glob.** `CONFIGURE_DEPENDS` is CMake 3.12+; the repo's CMake is
  newer. If a stale build is suspected, `cmake --build build` remains the
  fallback (and `make build` re-configures).
- **The in-file collapse touches the largest files** (`test_fight.c` 2379
  lines). It is the riskiest part; it lands last, on its own, with the count
  gate after each file.
