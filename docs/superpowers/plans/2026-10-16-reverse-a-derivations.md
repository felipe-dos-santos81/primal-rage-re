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

**The pin.** `tools/tests/test_diff_verify.py:32024` (in `RealFunctionTests`,
`test_the_self_check_counts_functions_mutants_gaps_and_closed_rows`) now asserts the sentence
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
