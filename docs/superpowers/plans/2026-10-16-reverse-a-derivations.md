# Sub-project A derivations (2026-10-16)

New sub-project-A work records here as §A1, §A2, … (plan
`2026-10-16-subproject-a-gate-trustworthiness.md`; design
`specs/2026-10-16-subproject-a-gate-trustworthiness-design.md`). A1/A2/A3/A4 record as they
land; A5 runs last and quotes the final text.

## §A4 — the counter's full-vs-weaker figure and the unrowed list (lane a, task 1: done)

**What the counter could not say.** `tools/diff_verify.py`'s closing sentence printed the
functions VERIFIED, the mutants detected, the `NAMED_GAP` spec count, and, over the rows that
have callees, how many closed (`774/832 (175 have none)`). It did **not** distinguish a full row
from a *weaker* one — a row that runs the original under code pokes, leaves reachable blocks out,
or declares a terminal difference — and its `1 named gaps` is the spec `NAMED_GAP` count (the one
spec `host_1b890`), which never named the **8 ported functions with no differential row at all**.

**Change.** `SpecResult` gains `weaker: bool` (`tools/diff_verify.py:403`), set in both spec
paths (`verify_gap`, `verify_spec`) from an explicit module frozenset `WEAKER`; `UNROWED` holds
the 8 unrowed ported entries. The counter's `main` print computes
`full = [r for r in closed if not r.weaker]` and prints two additive clauses after
`(175 have none)`:

```
diff-verify: 1007/1007 functions VERIFIED; 14900/14900 mutants detected; 1 named gaps;
774/832 rows with callees closed (175 have none); 768/832 closed on full rows;
8 ported functions with no row. The closure rule …
```

**Measured (this lane's fixture, 2026-10-16; self-check).** `len(closed) = 774`,
`len(with_callees) = 832`, `len(full) = 768`, `len(UNROWED) = 8`. So `F = 768`: 6 of the 12
`WEAKER` entries are closed rows with callees (`774 - 6 = 768`). The counter's other figures are
unchanged (`1007/1007`, `14900/14900`, `1 named gaps`).

**The explicit sets** (hand-maintained; keep in step with `AGENTS.md`; the design's call is an
explicit set, not a `terminal | pokes | unhit_named` heuristic — that flags ~120 rows, most
`unhit_named` sites declared dead, source `2026-10-16-subproject-a-gate-trustworthiness-design.md`
§A4).

- `WEAKER` (12, record `2026-10-15-reverse-j-derivations.md` §J1; `AGENTS.md:398-412`):
  `0x2CADC 0x255CC 0x1AFE8 0x1B084 0x1BE30 0x1C740 0x24C5C 0x2EA78 0x2EA74 0x2EA68 0x1B544 0x2EA30`.
- `UNROWED` (8, record §J1: "measured ported + unrowed, and callees of no row"):
  `0x1B120 0x1B3AC 0x1BEC4 0x1C930 0x1CF40 0x20C10 0x47370 0x4FBA2`.

**The pin.** `tools/tests/test_diff_verify.py`'s
`RealFunctionTests.test_the_self_check_counts_functions_mutants_gaps_and_closed_rows` (line
32024 at the time; later a section name, not a line) now asserts the sentence
with `; 768/832 closed on full rows; 8 ported functions with no row` inserted after
`(175 have none)`.

**Mutation proof (both clauses bite).**

1. New unit test `test_the_full_figure_excludes_weaker_rows` (beside
   `test_only_rows_with_callees_count_toward_the_closed_figure`): a `WEAKER` entry is not
   counted as closed-on-full while a plain row is. Mutation: the `not r.weaker` filter dropped
   → the kept set becomes `['p', 'w']` and the test FAILS (observed). Reverted.
2. The design's A4 acceptance (the figure MOVES under a `WEAKER` change): dropping `0x2CADC`
   from `WEAKER` moves the printed figure `768/832` → `769/832`, so the pin fails. Observed via
   the self-check; reverted.

**Gate.** `setarch -R env PR_ORACLE_REQUIRED=1 ./build/run_tests` → `all checks passed`;
`python3 -m unittest tools.tests.test_diff_verify` → `Ran 130 tests … OK`.

**Wording note (builder's call; design leaves the exact wording to the builder).** The clauses
sit immediately after `(175 have none)`, before `. The closure rule …`, reproducing the plan's
Step-4 expected substring `… (175 have none); 768/832 closed on full rows; 8 ported functions
with no row.` The plan's "append the three new values last in the tuple" is positional in
Python's `%` formatting and would instead place the `%d`s after `>= 0x%X` (the runtime base) in
the string; the clauses were placed where the string prints them and the tuple values
(`len(full), len(with_callees), len(UNROWED)`) go in the matching positional slot, before
`RUNTIME_BASE`. The design's line 284 leaves this to the builder.

## §A3 — the orphan tool tests: gate `test_title_pin`, delete the dead `mdi_disasm` (lane a, task 2: done)

**The gap.** The tools unittest suite is not one target; each gate names its modules in its own
`-m unittest` line (`Makefile:1148` k11/gp, `:1150` title_compare, the diff list). Two modules ran
in no gate: `tools/tests/test_title_pin.py` (a live tool's test — `title_pin.py` backs the
`title-pin` target and the capture targets; a `unittest.TestCase`, green) and
`tools/tests/test_mdi_disasm.py` (its tool referenced by no gate or capture target; pytest-style
module-level `def test_*`, so `unittest` collected **0 tests**).

**Change.**
- `Makefile:1148` (the `verify` target's tools-unittest line, after `tools.tests.test_gp_keys`)
  gains `tools.tests.test_title_pin`, so it runs in `make verify`.
- `git rm tools/mdi_disasm.py tools/tests/test_mdi_disasm.py` (152 + 24 lines; the OPL path is C
  and no file referenced the tool).
- `docs/PROGRESS.md` loses the six stale "`title_pin` unittest failure … outside `make verify`"
  clauses that called the test ungated debt (the batch paragraphs at 981/985/987/989/990/994); the
  closeout paragraph (996) already records that the failure was fixed there.
- `tools/title_pin.py`'s under-data guard (`16a5620b`) becomes symlink/worktree-robust: the
  casefold check refuses on EITHER `os.path.realpath(DATA_DIR)` (follows a worktree's
  `data -> ../../data` symlink to the main checkout) or `os.path.abspath(DATA_DIR)` (the literal
  repo `data/` path). Fail-closed — the OR only adds refusals; the existing exact-case
  (`real_src` vs `real_out`) check and the `os.path.samefile` backstop are unchanged, so the main
  checkout's behavior is unchanged. This closes the worktree failure below (otherwise gating
  `test_title_pin` would fail `make verify` run from a worktree).

**Measured.** Before deletion, `python3 -m unittest tools.tests.test_mdi_disasm -v` printed
`Ran 0 tests` (pytest-only collection). After deletion, `grep -rn mdi_disasm Makefile tools` is
empty (no live reference). `python3 -m unittest tools.tests.test_title_pin` is 10 OK with the fixed
guard **in the worktree and in the main checkout** (the modified file temporarily installed there),
and `python3 -m unittest tools.tests.test_title_pin tools.tests.test_title_compare` runs 20 tests
OK.

**The worktree failure and its fix.** Before `16a5620b`, run from a `git worktree` whose `data` is
the cross-checkout symlink, `test_refuses_an_out_under_data_mixed_case` FAILED: `guard` folded only
`os.path.realpath(DATA_DIR)`, which escapes to the main checkout's real `data/`, so the literal
`Data/game/C/…` out path never casefolded onto it on a case-sensitive filesystem. With the
`os.path.abspath(DATA_DIR)` baseline added the worktree refuses it too (10/10). Mutation proof:
`git stash push -- tools/title_pin.py` restores the old guard and the worktree test fails again
(`test_refuses_an_out_under_data_mixed_case ... FAIL`, `test_title_pin.py:96`, `Ran 10 … FAILED
(failures=1)`); `git stash pop` re-applies it and the run is 10 OK. Residual fail-closed limit
(named in the guard's comment): a genuine case-variant sibling directory (`Data/`, distinct from
`data/` on a case-sensitive filesystem) is refused too.

**Gate.** No byte-exact oracle and no pin is involved (A3 touches no comparison and no `port/src`).

## §A2 — re-seed `p45_check_p5_spawns`, retire the ASLR out-of-bounds write (lane a, task 3: done)

**The gap.** `test_fight.c`'s `p45_check_p5_spawns` (the `addr[k] == 0x48A20u` arm) built the actor
free list under the process's real pool base — `c4r_pool()` → `actors_reset()` reads `pool_base() =
DSD(0x1014F4)` — and only *then* rewrote the base `DSD(0x1014F4) = Z_R1`. `actor_spawn` (`actors.c`)
computes `index = (rec - pool_base()) / ACTOR_REC_SIZE` against the new base, so a record the free
list returned under the old base indexes off the pool; the pset it derives
(`DSD(0x1014EC) + (index & 0xffff) * PSET_SIZE`) then writes past `mem[]`. There is no in-pool guard
on `actor_spawn`'s index, so the write is silent where the wild address lands in mapped memory (ASLR
off) and a SIGSEGV where it does not (ASLR on).

**Measured, pre-fix** (gdb, `set disable-randomization off`, SIGSEGV caught in
`actor_spawn` at `actors.c:3865` `DSW(pset+0x00)`): the free-list record `rec = 0x2A7DAE0`, the wild
index `index = 0x272F83F` (41,089,087), and the wild `pset = 0x41107E0` (68,224,992) = the process
base + `0x41107E0` — the address AGENTS/PROGRESS named, reproduced base-identically. The existing
`DSB(Z_R0+0x4B) == DSB(child+0x56)` check could not catch it: `fighter_48a20` stores the child's
`+0x56` byte into `Z_R0+0x4B` by construction, so the two sides are equal no matter what index ran.

**Change (`port/tests/test_fight.c` only; `port/src` untouched).**
- The `0x48A20` arm assigns `DSD(0x1014F4) = Z_R1` *before* `c4r_pool()`, so `actors_reset` builds the
  free list under `Z_R1`, the base `actor_spawn` reads; the later pre-run block keeps only
  `DSB(Z_R0+0x4B) = 0u`.
- The post-run arm gains an in-pool assertion before the kept `+0x4B`/`+0x56` check:
  `CHECK(actor_index(child) < 0x100u, "the child is an in-pool record")`. `actor_index` (`actors.h`)
  returns `-1u` for a non-pool offset, so an inconsistent seed fails loudly here.
- `AGENTS.md` (the Linux-host bullet) and `docs/PROGRESS.md:1044` drop the `setarch -R` instruction
  and the named pre-existing OOB write, and state the pool seed is consistent and the suite runs
  under ASLR.

**Measured, post-fix.** With the base re-seeded before `c4r_pool()`, `c4r_pool` takes records 0..2 of
the `Z_R1` pool, so `actor_spawn`'s next free record is `Z_R1 + 3*0x68` → **index 3** (in-pool).
`PR_ORACLE_REQUIRED=1 ./build/run_tests` (ASLR on, no `setarch`) → exit 0, `all checks passed`;
`setarch -R env PR_ORACLE_REQUIRED=1 ./build/run_tests` → exit 0, `all checks passed`.

**Mutation proof.** Reverting the re-seed (base rewrite moved back after `c4r_pool()`, the loop's
`if (addr[k] == 0x48A20u) { DSD(0x1014F4) = Z_R1; DSB(Z_R0+0x4B) = 0u; }`) and running under
`setarch -R` (so the wild write does not SIGSEGV and the assertion is reached) fails the new check
alone: `FAIL …/test_fight.c:47083: the child is an in-pool record`, `FAILURES: 1` (exit 1); the kept
`+0x4B`/`+0x56` check still passes, which is exactly why it is not the guard. The same reverted tree
under ASLR on SIGSEGVs again (exit 139). Re-applied → both runs green.

**Gate.** No byte-exact oracle and no pin is involved (A2 touches no comparison and no `port/src`);
`make verify`'s suite line is unchanged.

## §A1a — pin the gp window reach, retire the under-render blind spot (lane a, task 4: done)

**The gap.** `tools/gp_compare.py`'s frames claim computes both its window (`first`) and its
coverage count up to `p`, the highest port frame any classified capture frame exhibits
(`frame_claim`, the `p = max(p, max(ex))` line). A port that stops rendering early therefore
just *shrinks* `p`: the capture frames past the port's last one are never classified, no
`unexplained` first is reached below the ratchet N, and the oracle stays green. The window
start is pinned (`start_check`) but the reach was not, so under-rendering was invisible to
every gp oracle (the plan's A1 under-render blind spot).

**Change.**
- `tools/gp_compare.py` gains `reach_check(name, reach, min_reach, out=print) -> int` (beside
  `start_check`): `reach < min_reach` FAILs ("the port exhibited fewer port frames"),
  `reach > min_reach` prints "improved: raise the pin", `min_reach is None` FAILs ("the
  window-reach pin is not set (GP_*_MIN_REACH)") — an unpinned scenario is unguarded.
- `frame_claim` returns a 4-tuple `(rc, first, j, reach)` from both return points (the window-empty
  early `return 1, None, 0, 0` and the final `return rc/first/j, p`); `reach` is the `p` it already
  computed, unchanged.
- `main` gains `--min-reach` and `rc4 = 0 if a.report else reach_check(name, reach,
  _int_or_none(a.min_reach), print)`, and `rc4` joins the final return; report mode never applies it.
- `Makefile`: a `GP_*_MIN_REACH` pin per scenario and `--min-reach "$(…)"` on every enforced
  `gp_compare.py` invocation (gp-oracle, gp-charsel-oracle, gp-moves-oracle, gp-keys-oracle,
  gp-twop-oracle, gp-modes-one, gp-win-one). `gp-report` is report-only, no pin.

**Measured reach (`p`), from each scenario's `gp_compare.py … --report` coverage line
("… up to port %d"), 2026-10-16, on the git-ignored captures below.** Report-mode and enforced-mode
reach are equal for all fourteen (the oracle run printed no "window reach" line: every reach equals
its pin exactly).

| pin | capture | measured `p` |
|---|---|---|
| `GP_IDLE_LOSS_MIN_REACH` | `data/k11-captures/gp-idle-loss` | 6300 (f=0x207D) |
| `GP_CHARSEL_MIN_REACH` | `data/k11-captures/gp-u5-charsel` | 358 |
| `GP_MOVES_MIN_REACH` | `data/k11-captures/gp-u6-moves-b` | 1737 (f=0xCAF) |
| `GP_KEYS_MIN_REACH` | `data/k11-captures/gp-keys-fight` | 788 (f=0x873) |
| `GP_TWOP_MIN_REACH` | `data/k11-captures/gp-twop` | 463 (f=0x5E1) |
| `GP_MODES_RA_MIN_REACH` | `data/k11-captures/gp-u8-right-arcade` | 825 |
| `GP_MODES_LT_MIN_REACH` | `data/k11-captures/gp-u8-left-training` | 847 |
| `GP_MODES_RT_MIN_REACH` | `data/k11-captures/gp-u8-right-training` | 845 |
| `GP_MODES_TW_MIN_REACH` | `data/k11-captures/gp-u8-tug-of-war` | 826 |
| `GP_MODES_HC_MIN_REACH` | `data/k11-captures/gp-u8-handicap` | 789 |
| `GP_MODES_EN_MIN_REACH` | `data/k11-captures/gp-u8-endurance` | 163 (f=0x495) |
| `GP_MODES_AS_MIN_REACH` | `data/k11-captures/gp-u8-attract-start` | 823 |
| `GP_WIN_MIN_REACH` | `data/k11-captures/gp-u9-win` | 1976 (f=0xDAE) |
| `GP_ENDING_MIN_REACH` | `data/k11-captures/gp-u10-ending` | 3444 (f=0x1502) |

**Oracle runs (unchanged pins).** `make gp-oracle gp-charsel-oracle gp-moves-oracle gp-keys-oracle
gp-twop-oracle gp-modes-oracle gp-win-oracle gp-ending-oracle` → exit 0. Every existing N/F/start
ratchet line is identical to the pre-A1a baseline (e.g. idle-loss 7559/8320, chars-el 516/1513,
moves 2139/3248, keys 1136/2164, twop 612/1506, RA 1072/2274 … EN 278/1174, U9 2452/3503, U10
4113/9954), zero `FAIL`, zero `improved`; the only new lines are the reach comparisons, all
exact (equal → silent).

**Mutation proof.**
1. Unit: change `elif reach < min_reach:` to `<=` → `test_the_reach_pin_catches_a_shrunken_window`'s
   exact-equality case FAILS (`1 != 0`, observed); reverted → green.
2. Integration: run gp-u5-charsel's real command with `--min-reach 999` (a reach above the port's
   358) → exit 1 and `FAIL: window reach port 358 < pinned reach 999 (the port exhibited fewer port
   frames)` (observed); the same command with the pinned 358 exits 0.

**Correction (plan-vs-raw).** The brief's unit-test snippet passes `out=io.StringIO()` to
`reach_check`, but `out` is called as a function (as `start_check`'s is) — a `StringIO` is not
callable. The test uses `out = []` and `out=out.append`, the convention the rest of the file already
uses (`run_claim`). The two `TestCli` tests that invoke `main` in enforced mode
(`test_a_present_capture_needs_pinned_values`, `test_the_capture_identity_pin`) also had to pass
`--min-reach`, because `main` now fails an unpinned reach by design; the new
`FAIL: the window-reach pin is not set` line is asserted, and no existing assertion changed meaning.

**Gate.** `python3 -m unittest tools.tests.test_gp_compare` → 55 tests OK (one added);
`tools.tests.test_gp_win`/`test_gp_modes`/`test_gp_twop` → 47 tests OK.

## §A1b — pin the front-end window END, retire the front-end under-render blind spot (lane a, task 5: done)

**The gap.** `tools/title_compare.py`'s `--frontend` branch discards `check_capture`'s `rc`
(which counts coverage and bad endpoints, `title_compare.py:653`) and keeps only `res['unexpl']`
— a documented choice, because the port dump legitimately runs into state 9 and raw coverage
false-fails. But the window `res['window'] = (a, b)` is derived from content alignment alone
(`b = idx[-1]`, the last capture frame any port frame is exhibited by, `:274`). A port that
renders fewer front-end frames stops the window **earlier**: the capture frames it used to
explain become `unexplained`, but they fall *outside* the shrunken window `range(a, b+1)`, so
`unexpl_j` never reaches them and the branch still prints "0 unexplained" and exits 0. The
window END was the under-render leak; the START is likewise unpinned but is not this task's.

**Change.**
- `tools/title_compare.py` gains `--frontend-end` (`argparse`, default `None`). In the
  `--frontend` branch, immediately after `res is None` is handled and before the `unexpl`
  handling (`:662`): `b < pin` FAILs ("the port exhibited fewer frames"), `b > pin` warns
  ("improved: raise the pin"), `pin is None` FAILs ("the window-end pin is not set
  (--frontend-end)") — an unpinned front-end run is unguarded. The existing `rc` discard and
  the `FRONTEND_ALLOWED_UNEXPLAINED` (832, 833) allowance are untouched; the check runs before
  them, so a shrunken window fails regardless of the unexplained set.
- `Makefile`: `FRONTEND_END ?= 1884` with a provenance comment, and
  `--frontend-end $(FRONTEND_END)` on the `frontend-oracle` invocation.

**Measured `b`.** `title_compare.py --frontend` on `data/title-captures/frontend` against the
`PR_FRONTEND_DET` dump prints `window distinct [560..1884] (raw 3108..4791)`; the end `b = 1884`
and the pin is 1884. (Same window the demo-fight comment records: `[560..1884]`, the frame before
the capture's first all-black frame 1885.) The exhibited count is unchanged (`1145/1382`), the
allowed set is unchanged (`[(832, 3671), (833, 3740)]`), endpoints `OK`.

**Oracle runs (unchanged pins).** `make frontend-oracle` → exit 0. The verdict line is
byte-identical to the pre-A1b run: `window distinct [560..1884]`, `1325 frames in window: 517
clean, 801 splice, 3 transition, 2 unexplained`, and
`2 unexplained captured frame(s) allowed by name: [(832, 3671), (833, 3740)]; no other
unexplained frame in the window.` The new pin is silent when `b == pin` (equal → no line); the
only new output is on a violation. Live checks: `--frontend-end 1885` → exit 1,
`FAIL: window ends at capture 1884 < pinned end 1885 (the port exhibited fewer frames)`;
`--frontend-end 1883` → exit 0, `window ends at capture 1884 > pinned end 1883 (improved: raise
the pin)`.

**Mutation proof.** Unit, in `tools/tests/test_title_compare.py`'s new `FrontendEndPinTest`:
port frames `0x11, 0x22`, capture frames `0x11, 0x22`.
1. Pin 1, two-frame port dump → exit 0, `window distinct [0..1]`, `0 unexplained`.
2. Pin 1, **one-frame** port dump → capture frame 1 is no longer exhibited so it lies outside
   `[0..0]`; the unexplained set is still empty (`0 unexplained` printed) and only the end pin
   can fail → exit 1, `window ends at capture 0 < pinned end 1`. Removing the `b < pin` check
   (mutation) makes case 2 return 0 and the assertion FAIL (`0 != 1`, observed); reverted green.

**Gate.** `python3 -m unittest tools.tests.test_title_compare` → 12 tests OK (2 added);
`make frontend-oracle` → exit 0. `port/src` untouched; `data/` untouched.

## §A1c — pin the attract port-exhibited and k11 coverage reaches (lane a, task 6: done)

**The gap.** Two window oracles report a port-frame statistic but do not ratchet it, so an
under-rendering port passes:
- `tools/attract_compare.py` prints `port attract frames exhibited Y/Z` (`len(port_exh)`, the
  distinct port attract frames the capture's explanations name) and its only gate is the
  capture-divergence exact match (`--expect-first 215`). A port that renders fewer attract
  frames exhibits fewer, which only shrinks the statistic.
- `tools/k11_compare.py` ratchets coverage **only over the settled screens** (`wanted`,
  `keys` from the port's own `screens.txt`), plus the END ratchet. `screens.txt` is
  regenerated by `k11_session.py port-script` from the port's own run, so a port that renders
  fewer frames just gets a shorter settled list and window and passes.

**Change.**
- `attract_compare.py`: `check_capture` gains `min_port_exh=None`; after the exhibited-count
  print it fails with `FAIL: port attract frames exhibited %d < pinned %d (the port rendered
  fewer frames)` when the count is below the pin. `main` gains `--min-port-exh` and threads
  it through. The `--expect-first` gate is untouched.
- `k11_compare.py`: `compare` gains `min_exh=None`. It now computes the distinct exhibited
  set (`port_exh`) and extends coverage past the settled screens to **every non-black port
  frame up to the final settled screen** (`cover = [m for m in range(min(end_settled, n)) if
  any(port[m])]`; all-black is the documented capture artifact, port/spec/game_flow.md). A
  non-black port frame in that range no capture frame exhibits fails (`bad_missing`), with
  `K11_ALLOWED_MISSING` (empty today) naming any legitimate exception. The distinct exhibited
  count is pinned: below `min_exh` fails with `FAIL: port frames exhibited %d < pinned %d (the
  port rendered fewer frames)`. `main` gains `--min-exh`; report mode never applies it. The
  existing settled/END/OPEN_END claims are untouched; the tool's DOCSTRING claim list gains
  claim 5.
- `Makefile`: `ATTRACT_MIN_PORT_EXH ?= 173` + `--min-port-exh` on `attract-oracle`;
  `K11_WALK_MIN_EXH ?= 36` / `K11_MENUESC_MIN_EXH ?= 184` + `--min-exh` on the two
  `k11-oracle` invocations, each with a provenance comment.

**Measured pins, from the existing captures' runs, 2026-10-16.**

| pin | capture | measured |
|---|---|---|
| `ATTRACT_MIN_PORT_EXH` | `data/title-captures/title`, `.../title2` | 173 (`port attract frames exhibited 173/690`, both) |
| `K11_WALK_MIN_EXH` | `data/k11-captures/walk` | 36 (`port frames exhibited 36 < pinned 99999`) |
| `K11_MENUESC_MIN_EXH` | `data/k11-captures/menuesc` | 184 (`... exhibited 184`) |

`K11_ALLOWED_MISSING` stays empty: on the walk dump the only unexhibited port frames are the
single **all-black** screen (canonical rep 1, at 30 indices 1,3,5,…,77) and on menuesc
likewise (indices 1, 157) — excluded by the non-black filter, so `non-black port frames
exhibited 48/48` (walk) and `184/184` (menuesc), `missing []`.

**Oracle runs (unchanged pins).** `make attract-oracle k11-oracle` → exit 0. Diffing the
`attract_compare:`/`k11_compare:` lines against the pre-A1c baseline shows **only two additive
lines** (`walk`/`menuesc`: `non-black port frames exhibited …; missing []`); every existing
verdict line — the attract windows, the `173/690` counts, the `--expect-first 215` passes, the
walk/menuesc windows, the settled `38/38` and `2/2`, the `K11_OPEN_END` `END 388 must be >=
388: ok` — is byte-identical.

**Mutation proof (each reverted green).**
1. Attract check disabled (`if False and …`) → `AttractPortExh.test_the_pin_fails_when_the_port_exhibits_fewer` FAILS.
2. Attract boundary `<` → `<=` → `…test_the_pin_passes_at_the_measured_count` FAILS.
3. k11 pin disabled (`short_exh = False and …`) → `test_the_port_exh_pin_catches_a_port_that_exhibits_less` FAILS.
4. k11 coverage dropped from the return (`not bad_missing` removed) → `test_an_unexhibited_non_black_port_frame_fails_the_coverage` FAILS.
5. k11 black filter removed (`any(port[m])` dropped) → `test_an_all_black_port_frame_is_not_missing` FAILS (the black frame is "missing").

**Corrections (plan-vs-raw).**
- The brief's Step-2 snippet omits the non-black filter (`full_missing = … if canon[m] not in
  port_exh`). Its prose says "all **non-black** port frames", and the raw shows the only
  unexhibited port frames are the single all-black screen (walk: 30 indices; menuesc: 2), so
  the unfiltered count is a spurious 30/2 and would need 30 allowances. The code filters
  `any(port[m])`; the allowance dict stays empty.
- The plan's provisional k11 arg name `--min-settled` (`Task 6` interfaces) names a quantity
  that is always 100% by construction (`wanted` is port-derived). The pin actually guards the
  **distinct exhibited count** (`len(port_exh)`), which is what the brief's Step-4 pin
  `K11_<SCENARIO>_MIN_EXH` counts; the CLI arg is therefore `--min-exh`.
- Both pins are lower-bound only (no "improved" warning), matching the brief's attract
  snippet; a count above the pin is not reported. An unset pin is a no-op (the Makefile always
  passes it); unlike the gp/title pins, unattended triggering is not made fatal — following the
  brief's `if min_port_exh is not None` form.

**Gate.** `python3 -m unittest tools.tests.test_gp_compare tools.tests.test_k11_compare` → 77
tests OK (57 + 20; 2 attract + 3 k11 added); the full gate list (`test_k11_* test_save_reload
test_gp_* test_title_pin`, under `PR_ORACLE_REQUIRED=1`) → 243 OK; `test_title_compare` → 12
OK; `make attract-oracle k11-oracle` → exit 0; `make k11-report` → exit 0. `port/src` untouched;
`data/` untouched; `symbols.h` untouched.

**Fix round 1/5 (1 addressed, 0 open).** The first edit of the k11 tests replaced the
`def test_trailing_black_capture_frames_pass` line instead of inserting beside it, orphaning
its body into the new all-black test (the module still imported and ran, so the count read 19
with the trailing test silently gone). Restored the `def`; the module is 20 tests and the
trailing-black claim is asserted again.

## §A5 — doc truth: the save-reload paragraph, the corrections scope and the oracle paragraphs (lane a, task 7: done)

**Files.** `docs/PROGRESS.md`, `README.md`, `AGENTS.md` only (`port/src` untouched; `data/` untouched).

**The save-reload paragraph.** `docs/PROGRESS.md` gains a paragraph after the Track-J host-findings one
(record §J3.10; commit `bffb779e`): boot 1's replay (scratch `PR_K11_SAVE`) writes 2040 bytes
byte-identical to the capture's `CMOS` (sha256 `dad3b1f6…`); boot 2, from a copy of that `CMOS`, holds
the five storage ranges (`config`, k0, hs0..hs2, §J3.3) at the pin `f=0x1EB` and every later dumped frame
(3 compared); `make save-reload-oracle` is in `make verify`, skipping without the capture; `data/game/C`
is proven untouched (the capture's own `CHECK staging`). It closes §S.8 F7 ("No recording of the original
behind a written or a loaded save", `2026-10-13-reverse-s-derivations.md:1107`).

**The two stale sentences.** `PROGRESS.md`'s Track-S paragraph named "no recording of the original writing
or loading a real save exists yet" among its gaps; the clause now reads that the §S.8 F7 gap is **closed**
by the `save-reload` capture (record §J3.10; `make save-reload-oracle`, in `make verify`). The Track-J
Part-I paragraph said its capture wave "is held"; it now reads that save-reload has landed and the
remaining captures (ENDURANCE, ending2, win-live, join, twop2, joystick) are held.

**The corrections scope.** `README.md`'s differential bullet read "its rows have found 172 raw-over-port
corrections", which a reader could take for the program total. The figure is track-D-scoped and deliberate;
it now reads "172 raw-over-port corrections (track D; 174 program-wide including track N's two,
`PROGRESS.md:1034`)". `PROGRESS.md:1034` is Track N's paragraph, whose two corrections (`0x1B194`,
`0x1B26F`) are the only ones after track D's 172 (tracks S, I, H, F, G and J found none), so 172 + 2 = 174;
`PROGRESS.md:1042` already records the 174→172 rescope.

**The oracle paragraphs (carried from Tasks 4/5/6).** `AGENTS.md`'s gp-oracle, attract and k11 paragraphs
now state each oracle's new port-frame reach/coverage pin (the front-end one landed in Task 5):
- gp: "it also pins the port-frame window reach (`GP_*_MIN_REACH`, one per enforced gp scenario;
  sub-project A A1a), so a port that renders fewer frames fails", and the gp-charsel narrow claim's
  "cannot detect a port that under-renders" is scoped to "*inside* the window" so it stays true with the
  reach pin (A1a).
- attract: "The attract prefix oracle itself (`make attract-oracle`) likewise pins the count of port
  attract frames the capture exhibits (`ATTRACT_MIN_PORT_EXH` = 173, sub-project A A1c), so a port that
  renders fewer attract frames fails; the window's interior coverage is otherwise not claimed."
- k11: "It now also covers every non-black port frame up to the final settled screen and pins the distinct
  exhibited count (`K11_WALK_MIN_EXH` = 36, `K11_MENUESC_MIN_EXH` = 184, `--min-exh`; sub-project A A1c),
  so a port that renders fewer frames fails."

Also folded in (design A5): the blocker-count sentence's unit — "By blocker (citations, not rows: a row may
name more than one, so these sum above the 58)". The per-blocker citations sum above the 58 open rows
because a row may cite more than one blocker (§J1's per-row table).

**Re-measurement (not transcription).**
- `python3 tools/port_progress.py` → `787 1203 65` / `747 747 100`; README's title (`65%`), "787 of the
  original's 1203 real functions" and "100% (747 of 747)" all match, so the title/counts need no change.
- `python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE --image
  /tmp/pr_diff_image.bin --table /tmp/pr_diff_table.md --self-check | tail -1` → `1007/1007 functions
  VERIFIED; 14900/14900 mutants detected; 1 named gaps; 774/832 rows with callees closed (175 have none);
  768/832 closed on full rows; 8 ported functions with no row.` — equal to Task 1's AGENTS.md clause and
  README's figure.

**Gate.** Docs-only: no test, `symbols.h`, or E2 change; the two commands above are the brief's Step 4.
`port/src` untouched; `data/` never written.
