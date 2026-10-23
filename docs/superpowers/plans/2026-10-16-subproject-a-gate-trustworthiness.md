# Sub-project A — make the gate mean what it says — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps
> use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a green `make verify` fail an under-rendering port (not only an
over-rendering one), retire the ASLR-dependent `setarch -R` workaround, resolve the two
orphan harness tests, make the differential counter honest about weaker rows and unrowed
portable functions, and correct the stale docs — with no byte-exact oracle verdict line
moved and no existing pin moved without recorded provenance.

**Architecture:** Five near-independent workstreams (A1 the comparison model; A2 the
test re-seed; A3 the orphan tests; A4 the counter; A5 the docs). A1 pins each oracle's
window **reach** (the highest port frame the capture exhibits), because the leak is that
a port which renders fewer screens shrinks its own comparison window; A2 fixes a
test-fixture base mismatch; A3/A4/A5 are small and self-contained. A1/A2/A3/A4 are
`tools/`- and `test/`-only (no `port/src`); A5 runs last and quotes A4's measured
figures.

**Tech Stack:** Python 3 stdlib `unittest` oracle tooling (`tools/*_compare.py`,
`tools/diff_verify.py`), C11 test suite (`port/tests/test_fight.c`), GNU make.

**Spec:** `docs/superpowers/specs/2026-10-16-subproject-a-gate-trustworthiness-design.md`
— read it first; the plan argues from it.

**Repo guidance (binding):** `AGENTS.md` (address model, oracle paragraphs, differential
harness, "Assertions must be able to fail") and `port/RE_GUIDE.md` / `port/PORTING.md`
if any engine code is touched (this plan touches none).

## Global Constraints

- **The byte-exact oracle verdict lines never move:** the pre-A `make verify` oracle
  output (each oracle's pass/fail and its N/F/ratchet numbers) equals the post-A output.
  Any new coverage/reach line A1 prints is additive and named.
- Every new pin is **measured from the existing capture**, with its provenance in the
  Makefile comment and the record `docs/superpowers/plans/2026-10-16-reverse-a-derivations.md`.
  A pin never moves toward the port.
- Every new assertion is **mutation-proven able to fail** (AGENTS "Assertions must be
  able to fail").
- **No new capture sessions.** Use only captures already in `data/k11-captures/`.
- `port/src` is untouched by this plan (A2 is a test-fixture edit). `port/src/symbols.h`
  is never hand-edited. Never write under `data/`; the game-dir `CMOS` stays 2040 zero
  bytes or absent (`make verify` enforces this).
- Every new test case/mutant/binding stays inside the 64 MB `mem[]`.
- Commits `<area>: <what changed>`, stage named files, **never `git add -A`**.
- A fresh worktree needs the git-ignored fixtures copied in
  (`port/tests/ghidra_data.bin`, `port/tests/title_screen_ref.ppm`) before any
  `PR_ORACLE_REQUIRED=1` run. Run binaries from the repo root.
- Workers run per-task: `make build`, `PR_ORACLE_REQUIRED=1 ./build/run_tests`, the
  owning oracle target, `python3 -m unittest <module>`. The full `make verify` is the
  controller's (Task 8).

---

## File Structure

| File | Responsibility | Task |
|---|---|---|
| `tools/diff_verify.py` | the counter + `SpecResult` gains `weaker`; `WEAKER`/`UNROWED` sets | 1 |
| `tools/tests/test_diff_verify.py` | re-pin the counter sentence; the full-vs-weaker unit test | 1 |
| `Makefile` | append `test_title_pin` to the tools-unittest list; new `GP_*_MIN_REACH` pins + `--min-reach` args | 2, 4, 6 |
| `tools/mdi_disasm.py`, `tools/tests/test_mdi_disasm.py` | deleted | 2 |
| `port/tests/test_fight.c` | the `p45_check_p5_spawns` re-seed + in-pool assertion | 3 |
| `AGENTS.md`, `docs/PROGRESS.md` | the ASLR bullet/paragraph; doc truth | 3, 5, 7 |
| `tools/gp_compare.py` | `reach_check`; `frame_claim` returns the reach; `--min-reach` | 4 |
| `tools/tests/test_gp_compare.py` | the reach_check unit test | 4 |
| `tools/title_compare.py` | the frontend window-end pin; stop discarding `rc` | 5 |
| `tools/tests/test_title_compare.py` | the frontend end-pin test | 5 |
| `tools/attract_compare.py`, `tools/k11_compare.py` | the port-exhibited / settled-coverage reach pins | 6 |
| `tools/tests/test_gp_compare.py` / `test_k11_compare.py` | the attract/k11 reach tests | 6 |
| `docs/PROGRESS.md`, `README.md`, `AGENTS.md` | A5 doc truth | 7 |

---

## Task 1: A4 — the counter's full-vs-weaker figure and the unrowed list

**Files:**
- Modify: `tools/diff_verify.py` (`SpecResult` ~388-402; `verify_gap` ~405-422;
  `verify_spec` ~439-504; the counter print ~34405-34414; add the two module frozensets)
- Modify: `tools/tests/test_diff_verify.py` (the pin in
  `RealFunctionTests.test_the_self_check_counts_functions_mutants_gaps_and_closed_rows`;
  add the unit test beside `test_only_rows_with_callees_count_toward_the_closed_figure`)
- Modify: `AGENTS.md` (the counter sentence ~384-385)

**Interfaces:**
- Consumes: nothing new.
- Produces: `SpecResult.weaker: bool`; module constants `WEAKER` (frozenset of 12 entry
  ints) and `UNROWED` (frozenset of 8 entry ints); the counter sentence's two new
  clauses `; N/M closed on full rows` and `; L ported functions with no row`.

- [ ] **Step 1: Add the frozensets and the field.**

In `tools/diff_verify.py`, immediately above `@dataclass class SpecResult` (~388):

```python
# The rowed functions whose row is not full (record 2026-10-15-reverse-j-derivations.md
# §J1; AGENTS.md:398-412): a row that runs the original under code pokes, leaves
# reachable blocks out, or declares a terminal difference. Function entries, not spec
# names; keep in step with AGENTS.md. The counter could not tell these from a full row.
WEAKER = frozenset({0x2CADC, 0x255CC, 0x1AFE8, 0x1B084, 0x1BE30, 0x1C740,
                    0x24C5C, 0x2EA78, 0x2EA74, 0x2EA68, 0x1B544, 0x2EA30})
# The 8 ported functions with no differential row at all (record §J1:26;
# AGENTS.md:385-386): "1 named gaps" is the spec NAMED_GAP count and never named these.
UNROWED = frozenset({0x1B120, 0x1B3AC, 0x1BEC4, 0x1C930, 0x1CF40, 0x20C10,
                     0x47370, 0x4FBA2})
```

Add to `SpecResult` after `gap` (~402):

```python
    weaker: bool = False                             # a poked/partial/terminal row (WEAKER)
```

- [ ] **Step 2: Set the flag in both spec paths.**

In `verify_gap` (~411), after the `SpecResult(...)` construction, add:

```python
    res.weaker = spec.entry in WEAKER
```

In `verify_spec` (~485), after `res = SpecResult(...)`, add:

```python
    res.weaker = spec.entry in WEAKER
```

- [ ] **Step 3: Print the two new clauses.**

In `main`, immediately before the final `print("diff-verify: ...")` (~34405), add:

```python
    full = [r for r in closed if not r.weaker]
```

Then extend the format string: after `(%d have none)` add
`; %d/%d closed on full rows; %d ported functions with no row`, and append to the
`% (...)` tuple, in order:

```python
              len(full), len(with_callees), len(UNROWED),
```

(The clause order must match the `%d`s: the existing `(%d have none)` is
`len(funcs) - len(with_callees)`; the three new values go last.)

- [ ] **Step 4: Update the pin with the fixture's measured values.**

Run the self-check once to read the new sentence:

Run: `make build && python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE --image /tmp/a4img --table /tmp/a4table --self-check | tail -1`
Expected: the sentence now ends `... (175 have none); F/832 closed on full rows; 8 ported functions with no row.` Record `F`.

In `RealFunctionTests.test_the_self_check_counts_functions_mutants_gaps_and_closed_rows`,
extend the `assertIn` string with the two new
clauses at their measured values (`F` from the run above), keeping the existing text
verbatim before them.

- [ ] **Step 5: Add the mutation-proven unit test.**

Beside `test_only_rows_with_callees_count_toward_the_closed_figure` (~32301), add:

```python
    def test_the_full_figure_excludes_weaker_rows(self):
        # A row whose entry is in WEAKER is not counted as closed-on-full, even when
        # VERIFIED and closed; a plain row is. Mutation: drop the `not r.weaker`
        # filter (or the WEAKER membership) and the kept set changes -> this fails.
        import tools.diff_verify as dv
        plain = dv.SpecResult("p", 0x1000, "VERIFIED", callees=[(0x1100, "allow")])
        weak = dv.SpecResult("w", next(iter(dv.WEAKER)), "VERIFIED", callees=[(0x1100, "allow")])
        weak.weaker = weak.entry in dv.WEAKER
        kept = [r for r in (plain, weak) if not r.weaker]
        self.assertEqual([r.name for r in kept], ["p"])
```

Prove it can fail: temporarily change `WEAKER` membership (or the filter) and confirm the
test fails, then revert.

- [ ] **Step 6: Update the AGENTS counter sentence.**

In `AGENTS.md` (~384), replace the quoted counter sentence with the new one (verbatim from
Step 4) and add one sentence naming `WEAKER` (12) and `UNROWED` (8) as the functions the
two clauses count.

- [ ] **Step 7: Run and commit.**

Run: `PR_ORACLE_REQUIRED=1 ./build/run_tests` (all checks pass) and
`python3 -m unittest tools.tests.test_diff_verify` (green).

```bash
git add tools/diff_verify.py tools/tests/test_diff_verify.py AGENTS.md
git commit -m "tools: sub-project A a4: the counter's full-vs-weaker and unrowed figures"
```

Record §A4 in `docs/superpowers/plans/2026-10-16-reverse-a-derivations.md`: the measured
`F`, the `WEAKER`/`UNROWED` lists, and the mutation proof.

---

## Task 2: A3 — gate `test_title_pin`, delete `mdi_disasm`

**Files:**
- Modify: `Makefile` (append `tools.tests.test_title_pin` to the tools-unittest list at
  ~1148)
- Delete: `tools/mdi_disasm.py`, `tools/tests/test_mdi_disasm.py`
- Modify: `docs/PROGRESS.md` (the stale "ungated test" notes ~981, ~985, ~987, ~990, ~994)

**Interfaces:**
- Consumes: nothing.
- Produces: `test_title_pin` runs in `make verify`; `mdi_disasm` gone.

- [ ] **Step 1: Confirm the tool/test facts.**

Run: `python3 -m unittest tools.tests.test_title_pin -v` → expect 10 tests OK.
Run: `python3 -m unittest tools.tests.test_mdi_disasm -v` → expect "Ran 0 tests".
Run: `grep -rn mdi_disasm Makefile tools docs` → expect only the two files and the
design-prose mention in `docs/superpowers/specs/2026-09-18-opl-driver-design.md`.

- [ ] **Step 2: Append the module to the gate.**

In the `verify` target's tools-unittest line (`Makefile:1148`), append
` tools.tests.test_title_pin` to the module list (the line already runs
`test_k11_fields ... test_gp_keys`).

- [ ] **Step 3: Delete the dead tool and its test.**

```bash
git rm tools/mdi_disasm.py tools/tests/test_mdi_disasm.py
```

- [ ] **Step 4: Confirm no live reference remains.**

Run: `grep -rn mdi_disasm Makefile tools` → expect no hits.

- [ ] **Step 5: Fix the stale notes.**

In `docs/PROGRESS.md` (~981, ~985, ~987, ~990, ~994), remove or reword the notes that
call `test_title_pin`/`test_mdi_disasm` ungated debt, since both are now resolved.

- [ ] **Step 6: Run and commit.**

Run: `python3 -m unittest tools.tests.test_title_pin tools.tests.test_title_compare` (green).

```bash
git add Makefile docs/PROGRESS.md
git commit -m "tools: sub-project A a3: gate test_title_pin, drop the dead mdi_disasm"
```

Record §A3.

---

## Task 3: A2 — re-seed `p45_check_p5_spawns`, retire the ASLR OOB write

**Files:**
- Modify: `port/tests/test_fight.c` (`p45_check_p5_spawns`, the `0x48A20` arm ~47053-47079)
- Modify: `AGENTS.md` (the Linux-host bullet ~64-68)
- Modify: `docs/PROGRESS.md` (the ASLR paragraph ~1044)

**Interfaces:**
- Consumes: `actor_index` (or the equivalent in-pool accessor) from `port/src/game/actors.h`.
- Produces: a consistent pool seed; a mutation-proofed in-pool assertion. `port/src`
  unchanged.

- [ ] **Step 1: Establish the current failure and the fix shape.**

Read `port/tests/test_fight.c:47047-47079` and `port/src/game/actors.c:3779-3784`. The fix:
inside the `addr[k] == 0x48A20u` arm, move `DSD(0x001014F4u) = Z_R1;` **above** the
`c4r_pool()` call at `:47054` so `actors_reset` builds the free list under `Z_R1`.

Replace (at `:47049-47068`):

```c
        for (k = 0; k < 7u; k++) {
            f = (p45_anim_fn)(void *)fn_resolve(addr[k]);
            CHECK(f != NULL, "the spawn target is registered");
            if (f == NULL) return;
            z_fseed();
            if (addr[k] == 0x48A20u) {
                DSD(0x001014F4u) = Z_R1;     /* build the pool under the base actor_spawn() reads */
            }
            c4r_pool();
```

with the arm's later `DSD(0x001014F4u) = Z_R1;` removed from the pre-run block
(`:47065-47068` becomes `if (addr[k] == 0x48A20u) { DSB(Z_R0 + 0x4Bu) = 0u; }`).

- [ ] **Step 2: Add the mutation-proofed in-pool assertion.**

In the `if (addr[k] == 0x48A20u)` post-run arm (`:47078-47079`), after the existing
`CHECK_EQ_INT`, add a check that the returned child is a real in-pool record:

```c
                {
                    u32 idx = (u32)((child - DSD(0x001014F4u)) / ACTOR_REC_SIZE);
                    CHECK(idx < 0x100u, "the child is an in-pool record");
                    CHECK_EQ_INT((int)DSB(Z_R0 + 0x4Bu), (int)(idx & 0xFFFFu) & 0xFF);
                }
```

Use the repo's existing record-size constant (grep `ACTOR_REC_SIZE` in `actors.h`); if
none is exported, use the literal the port uses at `actors.c:3782`. This makes the
`Z_R0+0x4B == child+0x56` check a real index check instead of a verbatim copy.

- [ ] **Step 3: Run the suite with and without the fix.**

Run: `setarch -R env PR_ORACLE_REQUIRED=1 ./build/run_tests` → all pass.
Run: `PR_ORACLE_REQUIRED=1 ./build/run_tests` (no `setarch -R`, ASLR on) → all pass
(this is the point: it no longer SIGSEGVs).
Mutation proof: revert the re-seed (put the base rewrite back after `c4r_pool`) and confirm
the new in-pool assertion fails, then re-apply.

- [ ] **Step 4: Retire the workaround in the docs.**

`AGENTS.md:64-68`: delete the "run the suite under `setarch -R`" instruction and the
out-of-bounds-write sentence; state that the `p45` pool seed is now consistent and the
suite runs under ASLR. `docs/PROGRESS.md:1044`: replace the "pre-existing … out-of-bounds
write stays named" paragraph with a note that it was fixed in sub-project A (test re-seed).

- [ ] **Step 5: Commit.**

```bash
git add port/tests/test_fight.c AGENTS.md docs/PROGRESS.md
git commit -m "port: sub-project A a2: re-seed p45's pool, retiring the ASLR out-of-bounds write"
```

Record §A2 with the measured evidence (the pre-fix wild index and address, the post-fix
index 3).

---

## Task 4: A1a — pin the gp window reach

**Files:**
- Modify: `tools/gp_compare.py` (`start_check` stays; add `reach_check` ~446; `frame_claim`
  return ~449-573; `main` ~742-803; new `--min-reach` arg)
- Modify: `tools/tests/test_gp_compare.py` (add a `reach_check` unit test)
- Modify: `Makefile` (add a `GP_*_MIN_REACH` pin + `--min-reach` to every gp target's
  `gp_compare.py` invocation)

**Interfaces:**
- Consumes: `frame_claim`'s existing `p` (the highest exhibited port frame).
- Produces: `reach_check(name, reach, min_reach, out=print) -> int`; `frame_claim` returns a
  4-tuple `(rc, first, j, reach)`; the `--min-reach` CLI option.

- [ ] **Step 1: Add `reach_check`.**

After `start_check` (`tools/gp_compare.py:446`):

```python
def reach_check(name, reach, min_reach, out=print):
    """The window REACH is ratcheted (sub-project A §A1): `reach` is the highest port frame
    any classified capture frame exhibits. Both the window and the coverage count are
    computed up to it, so a port that renders fewer screens stops at a lower reach and
    passes; it must be at least the pinned reach. A larger reach is an improvement (raise
    the pin). `min_reach is None` fails (a gp scenario without the pin is unguarded)."""
    rc = 0
    if min_reach is None:
        out('gp_compare: %s: frames: FAIL: the window-reach pin is not set (GP_*_MIN_REACH)' % name)
        rc = 1
    elif reach < min_reach:
        out('gp_compare: %s: frames: FAIL: window reach port %d < pinned reach %d '
            '(the port exhibited fewer port frames)' % (name, reach, min_reach))
        rc = 1
    elif reach > min_reach:
        out('gp_compare: %s: frames: window reach port %d > pinned reach %d '
            '(improved: raise the pin)' % (name, reach, min_reach))
    return rc
```

- [ ] **Step 2: Make `frame_claim` return the reach.**

In `frame_claim`, at both return points (`:472` and `:573`) return `p` as a fourth element:
the early `return 1, None, 0, 0` and the final `return rc, first, j, p`. Update the
docstring's Returns line.

**Update every `frame_claim` caller for the new arity.** `tools/tests/test_gp_compare.py`
unpacks a 3-tuple at five call sites — lines 48, 427, 520, 592, 689 — change each
`rc, first, _ = gc.frame_claim(...)` to `rc, first, _, _ = gc.frame_claim(...)`. (The only
production caller is `main` at `gp_compare.py:785`, updated in Step 3.) Confirm with
`grep -n 'frame_claim' tools/tests/test_gp_compare.py` that all five are updated.

- [ ] **Step 3: Wire `--min-reach` in `main`.**

Add `ap.add_argument('--min-reach', default=None)` beside `--max-start` (`:749`). Change
the `frame_claim` call (`:785-788`) to unpack four values:

```python
    rc1, _, _, reach = frame_claim(name, cap, raws, port, rows, n_frames, a.report, print,
                                   None if a.report else _int_or_none(a.max_start),
                                   BANDS_ALLOWED.get(name, ()), SCAN3_ALLOWED.get(name, ()),
                                   OVERLAY_ALLOWED.get(name, ()))
    rc4 = 0 if a.report else reach_check(name, reach, _int_or_none(a.min_reach), print)
```

and add `rc4` to the final `return 0 if a.report else (1 if rc1 or rc2 or rc3 else 0)`
(`:803`) → `... or rc4 ...`.

- [ ] **Step 4: Add the unit test.**

In `tools/tests/test_gp_compare.py`, add a test that `reach_check` fails when
`reach < min_reach`, passes at equality and above (the above case printing "improved"),
and fails when `min_reach is None`:

```python
    def test_the_reach_pin_catches_a_shrunken_window(self):
        import io, tools.gp_compare as gc
        out = io.StringIO()
        self.assertEqual(gc.reach_check('s', 40, 50, out=out), 1)   # shrank -> FAIL
        self.assertEqual(gc.reach_check('s', 50, 50, out=out), 0)   # exact -> ok
        self.assertEqual(gc.reach_check('s', 60, 50, out=out), 0)   # grew -> ok (raise)
        self.assertEqual(gc.reach_check('s', 40, None, out=out), 1) # unpinned -> FAIL
```

Mutation proof: change the `<` at the first branch to `<=` and confirm the exact-equality
case fails.

- [ ] **Step 5: Add the pins per gp scenario and measure.**

For each gp target, add a `GP_<ID>_MIN_REACH` variable and pass `--min-reach
"$(GP_<ID>_MIN_REACH)"`. Scenarios and their existing pin blocks: `gp-idle-loss`
(~526-545), `gp-u5-charsel` (~564-568), `gp-u6-moves-b` (~609-614), `gp-keys-fight`
(~641-666), `gp-twop` (~694-715), the seven U8 `gp-u8-*` (~745-826), `gp-u9-win`
(~948-952), `gp-u10-ending` (~1032-1036). Measure each value by running the target once in
report mode and reading the coverage line's "up to port %d" (`p`):

Run: `make gp-replay scenario=<id> GP_OPTIONAL=1 && python3 tools/gp_compare.py --scenario <id> --capture data/k11-captures/<id> --port /tmp/pr_gp_dump/<id> --report | grep 'coverage\\|window from'`

Pin `MIN_REACH` = the measured `p`; add a provenance comment (the capture and the date).
An unpinned scenario must fail, so do not leave any `--min-reach` off.

- [ ] **Step 6: Run the gp oracles and commit.**

Run: `make gp-oracle gp-charsel-oracle gp-moves-oracle gp-keys-oracle gp-twop-oracle gp-modes-oracle gp-win-oracle gp-ending-oracle`
Expected: every oracle green with its existing N/F unchanged and the new reach line
"ok"; the oracle verdict lines equal to the pre-A baseline.

```bash
git add tools/gp_compare.py tools/tests/test_gp_compare.py Makefile
git commit -m "tools: sub-project A a1a: pin the gp window reach"
```

Record §A1a with each measured `p` and its provenance.

---

## Task 5: A1b — the front-end window end pin

**Files:**
- Modify: `tools/title_compare.py` (the `--frontend` branch ~639-679; `check_capture`'s
  return already carries `window`)
- Modify: `tools/tests/test_title_compare.py` (a frontend end-pin test)
- Modify: `Makefile` (the `frontend-oracle` target ~190-199 and a `FRONTEND_END` pin)

**Interfaces:**
- Consumes: `res['window']` (the `(a, b)` pair `check_capture` returns, ~331-333).
- Produces: a `--frontend-end` pin; the frontend branch FAILS when the measured window end
  `b` is below it.

- [ ] **Step 1: Understand the branch.**

The `if a.frontend:` branch (`title_compare.py:639`) calls `check_capture(..., detail=False,
skip_black=True)` and keeps only `res['unexpl']`; `rc` (which counts coverage and bad
endpoints) is discarded (`:653`). Raw coverage false-fails for the frontend because the
dump legitimately runs into state 9 (`:624-638`). The fix keeps the discard for `rc`'s
coverage meaning but adds a **measured window-end** ratchet: the window end `b` is where
the port's last exhibited frame lands; a port that renders less stops earlier.

- [ ] **Step 2: Add the pin argument and check.**

In `title_compare.py`'s argparse (near the `--frames` arg ~553), add:

```python
    ap.add_argument('--frontend-end', type=int, default=None,
                    help='the pinned window end of the front-end comparison; a port that '
                         'stops earlier fails (sub-project A A1)')
```

In the `if a.frontend:` branch, after `raws = raw_map(...)` (~659), add:

```python
        a_win, b_win = res['window']
        if a.frontend_end is None:
            print("title_compare: frontend: FAIL: the window-end pin is not set "
                  "(--frontend-end)")
            return 1
        if b_win < a.frontend_end:
            print("title_compare: frontend: FAIL: window ends at capture %d < pinned end %d "
                  "(the port exhibited fewer frames)" % (b_win, a.frontend_end))
            return 1
        if b_win > a.frontend_end:
            print("title_compare: frontend: window ends at capture %d > pinned end %d "
                  "(improved: raise the pin)" % (b_win, a.frontend_end))
```

Place this after `res is None` is handled (`:655-658`) and before the `unexpl` handling, so
a shrunken window fails regardless of the unexplained set.

- [ ] **Step 3: Add the mutation-proven test.**

In `tools/tests/test_title_compare.py`, add a test that drives the frontend comparison over
a synthetic port dump and a synthetic capture where the port's last exhibited frame moves
one earlier, asserting the end pin fails. (Follow the existing synthetic-fixture helpers in
that module — grep `def test_` for the frame-dict builders.) Mutation proof: move the pin up
by one and confirm the unmutated case still passes, then a one-frame-shorter dump fails.

- [ ] **Step 4: Pin and measure.**

In the `Makefile`'s `frontend-oracle` target (~190-199), add `FRONTEND_END ?= <measured>`
and pass `--frontend-end $(FRONTEND_END)`. Measure `b` from a report run:

Run: `make frontend-capture` (or the target that writes the dump) then
`python3 tools/title_compare.py --capture data/k11-captures/<the captures> --port /tmp/<dump> --frontend`
Read the `window distinct [a..b]` line; pin `b` with a provenance comment.

- [ ] **Step 5: Run and commit.**

Run: `make frontend-oracle` (green; the enforced oracle's verdict line unchanged).

```bash
git add tools/title_compare.py tools/tests/test_title_compare.py Makefile
git commit -m "tools: sub-project A a1b: pin the front-end window end"
```

Record §A1b.

---

## Task 6: A1c — the attract port-exhibited and k11 settled-coverage reaches

**Files:**
- Modify: `tools/attract_compare.py` (`check_capture` ~69-133; `main` ~136-186;
  `--expect-port-exh` or a `--min-port-exh` arg)
- Modify: `tools/k11_compare.py` (`compare` ~120-190; `--min-settled`)
- Modify: `Makefile` (the `attract-oracle` ~167-177 and `k11-oracle` ~371-389 targets +
  pins)
- Modify: `tools/tests/test_gp_compare.py` and/or `tools/tests/test_k11_compare.py` (reach
  tests)

**Interfaces:**
- Consumes: `attract_compare`'s `port_exh` set (~91); `k11_compare`'s `wanted`/`exh`
  (~158-160).
- Produces: `--min-port-exh` (attract) and `--min-settled` (k11) lower-bound pins.

- [ ] **Step 1: Attract — pin the port-exhibited count.**

In `attract_compare.check_capture`, after the `port_exh` print (`:104-106`), the count is
`len(port_exh)`. Add a parameter `min_port_exh=None`; before the divergence logic add:

```python
    if min_port_exh is not None and len(port_exh) < min_port_exh:
        print("attract_compare: %s: FAIL: port attract frames exhibited %d < pinned %d "
              "(the port rendered fewer frames)" % (capture, len(port_exh), min_port_exh))
        return 1, (None, None)
```

Thread `min_port_exh` from `main`'s `--min-port-exh` arg into the `check_capture` call
(~179).

- [ ] **Step 2: k11 — pin the settled coverage.**

In `k11_compare.compare`, `missing` is the settled screens not exhibited (`:158-160`). The
gate already fails on `missing` (`:190`). Extend the coverage to all non-black port frames
up to the window end with the iteration's `exh`, and pin the count with a named allowance
(the walk's mid-draw wipe frames join `K11_ALLOWED_UNEXPLAINED` if needed):

```python
    port_exh = {canon[m] for m in exh}
    full_missing = [m for m in range(min(end_settled, n)) if canon[m] not in port_exh]
    allowed_missing = K11_ALLOWED_MISSING.get(name, ())
    bad_missing = [m for m in full_missing if m not in allowed_missing]
```

Add `K11_ALLOWED_MISSING = {}` beside `K11_ALLOWED_UNEXPLAINED` (~42), fold `bad_missing`
into the `return 1 if ...` at `:190`, and print the new count. A start-of-task measurement
names which screens legitimately remain unexhibited; if none, the dict stays empty.

- [ ] **Step 3: Tests.**

Add reach tests to `tools/tests/test_gp_compare.py` (attract) and
`tools/tests/test_k11_compare.py` (k11) asserting each new pin fails when the measured count
drops below it, and passes at/above. Mutation-prove each.

- [ ] **Step 4: Pin and measure, run, commit.**

Add `ATTRACT_MIN_PORT_EXH` and `K11_<SCENARIO>_MIN_EXH` pins (measured from the existing
captures' report runs) and pass them in the targets. Run:
`make attract-oracle k11-oracle` → green with unchanged verdict lines.

```bash
git add tools/attract_compare.py tools/k11_compare.py tools/tests/test_gp_compare.py tools/tests/test_k11_compare.py Makefile
git commit -m "tools: sub-project A a1c: pin the attract and k11 port-frame coverage"
```

Record §A1c.

---

## Task 7: A5 — doc truth

**Files:**
- Modify: `docs/PROGRESS.md` (add the save-reload paragraph; fix `:1028`, `:1042`)
- Modify: `README.md` (`:53` the corrections figure wording)
- Modify: `AGENTS.md` (already partly done in Tasks 1/3; fold in any remaining A5 edits)
- (Record) `docs/superpowers/plans/2026-10-16-reverse-a-derivations.md`

**Interfaces:**
- Consumes: Task 1's measured counter clauses; Task 3's AGENTS/PROGRESS edits.
- Produces: docs matching the live tree.

- [ ] **Step 1: Add the save-reload PROGRESS paragraph.**

Append a paragraph after the Track-J host-findings paragraph
(`docs/PROGRESS.md:1044`) recording the `save-reload` capture and oracle (commit
`bffb779e`; record `2026-10-15-reverse-j-derivations.md` §J3.10): the boot-1 write is
byte-identical to the capture's `CMOS`; boot 2 holds the five storage ranges at the pin
(`f=0x1EB`); `make save-reload-oracle` in `make verify`; it closes §S.8 F7.

- [ ] **Step 2: Fix the two stale sentences.**

`docs/PROGRESS.md:1028` — remove "no recording of the original writing or loading a real
save exists yet (the `save-reload` capture, spec §4.3)" and state it is now closed.
`docs/PROGRESS.md:1042` — replace "its capture wave (…) is held" with "save-reload has
landed; the remaining captures (ENDURANCE, ending2, win-live, join, twop2, joystick) are
held".

- [ ] **Step 3: Disambiguate the corrections figure.**

`README.md:53` — keep `172` but scope it: "its rows have found 172 raw-over-port
corrections (track D; 174 program-wide including track N's two, `PROGRESS.md:1034`)."

- [ ] **Step 4: Re-measure and commit.**

Run: `python3 tools/port_progress.py` → confirm `787 1203 65` / `747 747 100`, so README's
title and counts need no change; `python3 tools/diff_verify.py ... --self-check | tail -1`
→ confirm the counter sentence matches Task 1's.

```bash
git add docs/PROGRESS.md README.md
git commit -m "docs: sub-project A a5: doc truth (save-reload, the corrections scope)"
```

Record §A5.

---

## Task 8: Controller close (not a dispatched implementer task)

- [ ] Read-only audit of Tasks 1-7 against base: evidence discipline, claim wording, the
  "drop a port frame → the oracle fails" proof for A1, the mutation proofs, the doc figures
  re-measured from source.
- [ ] Integrate; run **`PR_DIFF_FULLSCAN=1 make verify`** (the pre-push form, per AGENTS'
  fullscan note) **without `setarch -R`** and confirm the oracle verdict lines equal the
  pre-A baseline's (timings normalised), the differential counter reads the new sentence at
  its measured values, `symbols.h` is idempotent, and the E2 table is byte-identical.
- [ ] `AGENTS.md` final read (the oracle paragraphs now state the reach pins; the ASLR
  bullet is gone); README title re-check (`python3 tools/port_progress.py` — unchanged).
- [ ] PROGRESS.md sub-project-A paragraph; record/ledger close.

---

## Self-Review (spec coverage)

- **A1** — Task 4 (gp reach), Task 5 (title frontend end), Task 6 (attract/k11). The
  "drop a port frame → the owning oracle fails" proof is Task 4 Step 3 / Task 8.
- **A2** — Task 3.
- **A3** — Task 2.
- **A4** — Task 1.
- **A5** — Task 7.
- **Constraints** — the byte-exact verdict-line freeze is verified in every task's run step
  and gated in Task 8; "no port/src change" holds (Task 3 is test-only).
- **Non-goals** — no capture sessions, no porting, no N-frame model: honored (A1 uses
  measured per-oracle reach pins + named allowances only).
- Type consistency: `SpecResult.weaker`, `WEAKER`, `UNROWED` (Task 1) are used only in
  Task 1; `reach_check`/`--min-reach` (Task 4) are the names used in the Makefile steps;
  `--frontend-end` (Task 5), `--min-port-exh` (Task 6), `--min-settled` (Task 6) are
  consistent between their code and target steps.
