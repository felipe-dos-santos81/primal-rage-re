# Proof Completion — design

Status: Implemented and gated, 2026-10-24. Follows the endgame truth phase; the
differential frontier, the span-writer consolidation, the gate's self-report
and the capture oracles' claim width are its subject. The measured outcome is
the derivation record `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md`
§P1–§P13; §7's predicted figures are superseded by those measurements.

## 1. Intent and why

The port is complete against its declared population: 799 of Ghidra's 1203
indexed functions (66%) and 759 of 759 portable ones (100%). What is not
complete is the proof. The gate is green, but four proof surfaces are narrower
than a reader would take them to be:

- **The differential frontier.** `make diff-verify` reported 31 open rows, 8
  portable functions with no row and 12 weaker entries at the phase's start; it
  now reports 20 open rows (19 distinct addresses), 8 `UNROWED` and 15 `WEAKER`
  entries — each with a declared reason — and 0 named gaps. The open rows read
  as unfinished work even where no row can exist.
- **The span-writer consolidation.** The original reaches 385 unrolled copy
  routines through four tables. The port replaces them with four C loops, but
  the differential row exercises only 11 of 27 code-shape classes; the comment
  at `tools/diff_verify.py:26774` says so. The bank-carry defect that the
  gameplay truth II phase fixed lived in exactly this surface.
- **The capture oracles.** Every gameplay oracle claims "no content-bearing
  capture frame in the window is unexplained and the window reaches its end".
  They explicitly do not claim the order of the port's frames or that every
  port frame is exhibited, and `MIN_REACH` only pins the last, not the
  composition.
- **The gate.** 35 capture directories are expected; plain `make verify`
  enforces 3 (the two Smacker captures and the title capture), compares
  `title2`/the front-end only when present, and skips 30 with exit 0. The docs
  disagree with the Makefile about which family is enforced, and a stray
  `PR_*_DUMP` variable can replace the entire unit suite with one driver.

The endgame truth design already names this phase: NG1, "The differential
frontier ... a separate Proof Completion phase". This design takes that seed
and completes the other three surfaces, because they share the same failure
mode: the claim is narrower than the evidence, and the gap is invisible in a
green run.

The phase adds no shipped-path player-visible behaviour (the one port edit is
a fault guard on a path the shipped data does not reach) and takes no new
capture. It is offline, reversible, and every change is either a stronger
proof or a truthful declaration.

## 2. Current state (measured 2026-10-24)

- `python3 tools/port_progress.py` → `799 1203 66` / `759 759 100`.
- `make diff-verify` → `1031/1031 functions VERIFIED; 14983/14983 mutants
  detected; 0 named gaps; 813/844 rows with callees closed (187 have none);
  806/844 closed on full rows; 8 ported functions with no row` (45 `FN_`
  ported-unrowed: 8 portable + 37 runtime).
- Span tables: `0x80C8C` (32 slots, 7 distinct), `0x80D0C` (64), `0x80E0C`
  (129), `0x81010` (64), `0x81110` (128): 417 slots, 392 distinct targets,
  six renderers, one bare `ret` (`0x51E58`), 385 writers. 27 code-shape
  classes, 26 reachable, one dead (`0x80E0C[128] = 0x57E3C`);
  `0x81110[128]` reads `0x81310[0] = 0x5D110`, a reachable 128-byte writer.
  The port's four C loops exercise 11 classes today.
- Capture inventory: 35 expected directories; 34 present; `gp-u14-win-live`
  absent by design (R1). Plain `make verify` enforces 3 capture directories
  (smk/twi5, smk/twg, title); `title2` and the front-end are compared only
  when present; 30 skip. K11 has no capture identity pin; the front-end's two
  allowed frames (832, 833) have no byte pin.
- Gameplay oracle dumps: 24 scenarios replay; all 24 reproduce their pinned
  `MIN_FIRST`/`MIN_REACH`. An occurrence-aware, existential order check
  measured 0 failures on all 24; the canonical unexhibited-class counts are
  in the 0..64 range, and each pin is measured by Task 9 (the recon's
  preliminary 24/11 figures are not pins).
- Known defects found by the phase's recon: `0x65546` (`sprintf`) is already
  runnable through `D3A_SPRINTF`, so `text_number_core`'s stub is stale;
  `0x65431`'s 11 declarations are reached by no case; `0x25FAC` and `0x26194`
  are ported with `PR_SEAM0` and have no row; `0x1B084`'s short-write block
  `0x1B0F7` is unhit; `0x51ED8` carries a declared byte difference but is
  counted among the full rows; `0x42203` is the only unproven divide-fault
  site; `0x24B08` is dead and `0x24B54` is live, ported, and capture-exercised.

## 3. Goals

**G1 — Differential.** Row the two ported functions, convert the sprintf
stub to an allow, move the unreached longjmp from the callee list to a
scan-only stop (so the static scan still stops at the tail jump), add the
short-write case, correct the `0x51ED8`, `0x249F0` and `0x1D0BC`
classifications, and declare the remaining 20 open rows terminal with a
per-row reason. The short-write case reopens `config_file_save` on the
runtime flush `0x6C4CF`; that is a named terminal, not a regression. Add the
one honest divide guard (`0x42203`/`0x4221C`).

**G2 — Span writers.** Prove every reachable writer class with a minimal
fixture, then every table entry with an exhaustive fixture if the class proof
passes; cover the clipped renderers' both-edges paths; document the one dead
writer and the two domain exclusions.

**G3 — Capture oracles.** Claim content order (existential, occurrence-aware,
meaningful-contribution) and a canonical coverage count for every enforced
gameplay scenario; add K11 identity pins and order; make every named
allowance byte-bound and stale-name-failing; optionally extend the front-end
and attract claims.

**G4 — Gate.** Add a strict preflight that names every expected capture and
fails on an undeclared absence; sanitize the driver environment; require
`frames/fn_miss.txt`; remove the one tautology; put `test_tick_vector` in the
gate; make lower-bound pins exact under `PR_PINS_EXACT`.

**G5 — Docs and accounting.** Remove the contradictions between README,
AGENTS, PROGRESS, RE_GUIDE, the specs and the newest records; qualify the
66% figure as Ghidra-indexed; add a reproducible span-writer census as the
second metric.

## 4. Non-goals

- **NG1 — No new portable game code.** This is a proof phase. The only
  functional port edit is the `0x42203` divide guard and its test; the
  `actors.c` sprintf-comment correction is documentation. No new function, no
  new `fn_register`, no new capture.
- **NG2 — No new capture, no re-capture.** The order/coverage pins are
  measured on the existing dumps and captures.
- **NG3 — No gameplay or audio truth work.** U14 R1, the U16 live finisher,
  the death streams, the 5120 cause and rendered audibility stay where the
  endgame phase left them. Audio audibility remains a product chore.
- **NG4 — No host-owned ports.** The AIL wrappers, `0x1BDF4`, the PIT/
  game-port cluster and the aperture blits stay host-owned. A row there would
  verify a harness-authored device, not the game.
- **NG5 — No E2 universe change.** The 158 span-table targets that E2's
  `ret` rule does not triage are reported by a separate census; the
  `--expect 579 --expect-u0 575` gate stays byte-identical.
- **NG6 — No rewrite of the differential harness.** Only the rows, allow
  entries, one file-model field and the pins the tasks need.

## 5. Approaches considered

**A — Differential-only proof (the endgame NG1 literal).** Span writers, the
two rows, the sprintf/longjmp dispositions, the short write, the terminal
census. Six tasks, purely in `tools/diff_verify.py` and the port's test seam.
It closes the named frontier but leaves the gate's skips and the oracles'
order/coverage caveats exactly as they are.

**B — Full proof completion (recommended).** A plus the capture-oracle
claims, the gate preflight and the docs/accounting pass. Twelve tasks, still
offline and reversible. It closes the four surfaces that share the same
defect, and it makes `make verify`'s name mean the same thing on every host.
The cost is one Makefile mode, one oracle-rule change and a docs sweep.

**C — Proof by porting the host-owned code.** Port `0x1BDF4`, the aperture
blits and the PIT/game-port cluster so their rows can close. It moves the
counters, but the rows would compare a harness-authored timing/device model,
and the classification file deliberately declares those routines host-owned.
It is a fidelity phase disguised as proof; reject for this phase.

**Recommendation: B.** The four surfaces are one problem: a green gate whose
claims are narrower than its evidence. B fixes the problem without adding a
capture, a policy decision, or a player-visible behaviour. C's rows would be
the metric inflation the closure rule was written to prevent.

## 6. What gets built

### 6.1 Differential proof

**Span writers.** The 385 writer targets divide into 27 code-shape classes by
table index and direction: fill/literal, forward/backward, run length 1..3,
4 mod 4, 1 mod 4, 2 mod 4, 3 mod 4, the 14/15 word-tail classes, the 128-byte
chunk, and the bare `ret`. One of the 385 slots is dead (`0x57E3C`, index 128 of
`0x80E0C`), so the reachable set is **385 targets** — 384 reachable slots plus
the out-of-table `0x81110[128] = 0x81310[0] = 0x5D110` (a reachable 128-byte
writer). The class-complete
fixture is one 56-byte RLE row (literals 0..7, fills 0..7/14/15) drawn three
ways — normal at `(10,2)`, hflip at `(84,2)`, and a 128-byte raw row — with
the palette bank byte `0xFE` so the dword carries make the word-tail quirk
observable. It reaches all 26 live classes. The exhaustive fixture is the
37-row, 8,434-byte deterministic stream (literal 0..127, fill 0..63) plus 128
raw psets at `vis = 128..1`, which reaches all 385 reachable writer targets
and names the dead one. The clipped renderers' both-edges paths (`0x5D580`,
`0x582F4`) need a width-330, two-row sprite at `x = -1..-9`; the shear
128/top-clip path is optional. A unittest runs `E.run_original` with the
writers recorded as `real` calls and asserts the arrived target set, so a
later payload edit cannot silently shrink the claim. The fixture generator is
committed (or its exact output pinned by length and hash) so 385/386 is
reproducible.

**Rows.** `flow_card_ramp_step` (`0x25FAC`, a leaf) and
`flow_bonus_card_b_step` (`0x26194`, callees `0x2BC30` and `0x2B150`, both
VERIFIED) get their own rows with the cases and mutants in the plan. The
sprintf stub in `text_number_core` becomes `allow_calls += D3A_SPRINTF`; the
`0x65546` stub-clobber pin and the `C3F` kinds are re-measured.
`0x65431` is removed from the 11 specs' **callee** list but kept in a new
scan-only `Spec.stop` set: `spec.calls` is also the static scan's stop set,
so deleting it outright would follow the tail jumps and turn 9 runtime
leaders into unhit blocks. With `stop`, the scan stays at 161/18 leaders,
the rows close, and a future case that enters the arm still fails closed as
an unmodeled call. `0x1B084` gets an `s5` short-write case through a new
`Machine.short_write` field, with `exits=True`, `SC_EXIT` in `allow_calls`
and `SC_FLUSH` in `calls`; the case hits the last block but reopens the row
on the runtime flush `0x6C4CF`, so the census gains `config_file_save` as a
20th terminal. `0x51ED8` joins `WEAKER` (one declared byte difference),
`0x249F0` joins it too (its `unhit_named` `0x24A9C` restart arm is a
reachable block left out), and `0x1D0BC` joins it (its `0x1D10E`/`0x1D183`
buffer-failure arms are unhit). The full-row count moves down by **one**
(`0x51ED8` was counted full; `0x1D0BC` was already neither full nor closed, so
it moves no full-row figure), to the measured 807.

**Terminal census.** The 20 rows still open after the short-write case are
15 AIL thunks, `sound_buffers_alloc` (`0x1D0BC`, blocked by `0x1C308`),
`input_sampler` (the four host-owned PIT routines), `config_file_load` and
`menu_fatal_error` (both blocked by `0x6C4CF`) and `config_file_save`
(reopened by the short-write case on the same `0x6C4CF`). Each gets a record
line: blocker, class, why no row can exist, the record that pins it. The 8
portable unrowed get the same treatment; the 15 weaker entries get a one-line
reason why they cannot be full. The census is the phase's answer to "31
open": 20 named terminals, no unclassified residue.

**Divide faults.** The recon proves the fighter divisors (`0x39ADF`,
`0x39C6D`) and the render/scroller divisors safe from their call sites and
tables; `0x2E910` is safe because the only caller passes the one-byte label
at `0x32640` (`03 00`, so `len = 1` and `unit >= 4`); the plan records that
caller and adds a unit test, rather than relying on the prose. `0x42203`/
`0x4221C` is the one site whose `step = 0` is not excluded by the EXE: state 6
divides before the scroll animation starts, and the animation stream is
external INDEX data. Add `host_cpu_fault(0x00u, 0x42203u, ..., 1)` before
both divisions (the literal status avoids the file-local `SVC_FAULT_EXIT`)
and a test that seeds `word[bg+0x2C] = 0x180`; the original faults, so the
port must fault at the same site.

**Dead and live.** `0x24B08` is a dead duplicate of the inline block at
`0x24F09`: preceded by a `ret`, no rel8/rel32, no dword, no fixup, not in the
index. Record it, add a `fn_resolve(0x24B08) == NULL` negative test, and do
not port it. The same gap holds the live `0x24B54`, which is ported,
registered and exercised by six captures; the record names both so the
census is not read as "only one body in `0x24B08..0x24C5C`".

### 6.2 Capture-oracle proof

The order rule is existential. At each capture frame, choose any model-valid
explanation — a clean occurrence, a valid adjacent-pair splice, a transition,
or a named allowance — whose highest exhibited port index is at least the
last, taking the smallest such index. A splice exhibits its high frame only
if some byte **after** the boundary differs from the **low** frame (the high
frame has a byte the low frame does not); symmetrically, it exhibits its low
frame only if some byte **before** the boundary differs from the **high**
frame. Without that test a clean rewind can be laundered through a degenerate
suffix. Band, scan3 and overlay frames are mixed or overlaid presentations
and are order-neutral. If the local window has no feasible explanation,
enumerate the whole dump. This rule measured 0 failures on all 24 scenarios;
the naive first-index rule fails 19/24 and a `p`-centred rule fails 16/24, so
the occurrence map and the minimum-feasible choice are load-bearing.

The coverage rule is canonical: count non-black port content classes up to
`reach`, collapse byte-identical frames, and pin the unexhibited class count
with `--max-missing`. The measured range is 0..64 per scenario; each pin is
measured by Task 9 after the flag exists (the recon's figures, e.g. 24/11,
are preliminary, not pins; the idle value re-measured at 29). An unpinned
scenario fails, `+1` fails, and an
improvement prints "improved: lower the pin". This is a **bounded-missing**
claim, not full coverage: a port that under-renders a state the capture never
sampled still passes, and that residual stays a named gap. K11 already has a
stronger index-based coverage claim; only order and the capture identity are
new there, and the identity pins `poll.log` + frame count, not every pixel.
The front-end's canonical missing count measured 30, not 11, and its window
start is port-derived; it is an optional extension only with
`--frontend-start 560` and the measured `--max-missing 30`, or it stays a
named gap. `gp-u17` (no frame replay) and `gp-pads` (smoke) are excluded by
name.

Allowance honesty: `FRONTEND_ALLOWED_UNEXPLAINED` (832, 833) and
`K11_ALLOWED_UNEXPLAINED` (walk 138, 151) gain sha256 pins and fail when a
named frame becomes explained. The empty `K11_ALLOWED_MISSING` and
`ATTRACT2_ALLOWED_UNEXPLAINED` get a synthetic test that the stale-name check
fires if a name is added; an empty set alone proves nothing. `ATTRACT2_SPLICE3`
and the gp recipes already bind bytes; the "retire the name" note becomes a
failure under exact mode.

### 6.3 Gate trust

`GATE_STRICT ?= 1` and a `gate-check` prerequisite in `verify` count the 35
expected directories, print the inventory, and fail on an undeclared absence.
The 35 are enumerated from the oracle targets, not globbed: 30 K11/gp
scenarios (`walk`, `menuesc`, `save-reload`, `gp-pads`, the 26 gp-oracle
scenarios including `gp-u14`) plus the 2 Smacker and 3 title captures.
Plain verify enforces 3 of them (smk/twi5, smk/twg, title); `title2` and the
front-end are compared only when present; the other 30 skip. `GATE_STRICT`
and `PR_PINS_EXACT` are **exported** (or passed explicitly) so the Python
tools see them, not only the Makefile. `GATE_NAMED_GAPS` defaults to
`data/k11-captures/gp-u14-win-live`; a declared gap that becomes present
fails as stale. `GATE_STRICT=0` reproduces today's partial behavior for
capture-less checkouts. `PR_PINS_EXACT` turns every "improved: raise/lower
the pin" branch into a failure, so a pin cannot silently lag the measured
value; it starts off for the implementation, runs explicitly on the full
gate, re-pins any drift, and only then defaults to `$(GATE_STRICT)`.
`k11_compare` and `attract_compare` gain the missing "improved" branches
before they are put under exact mode.

The unit run is sanitized with `env -u` for all seven driver selectors
(`PR_ATTRACT_DUMP`, `PR_TITLE_DUMP`, `PR_FRONTEND_DUMP`, `PR_FRONTEND_DET`,
`PR_K11_DUMP`, `PR_GP_DUMP`, `PR_RESTART`). `frames/fn_miss.txt` is required
by a separate `PR_FN_MISS_REQUIRED` flag set only by `verify`, so
`make test`/`run_tests` alone keep their current behavior. The tautology at
`test_fight.c:21084` is removed. `test_tick_vector` joins the tool list with
`PR_GAME_DIR=$(GAME_DIR)`. The stale `frames/*.idx`, `title_screen_ref.ppm`
and entry-triage comments are corrected. `test_video.c` already skips an
absent Smacker capture unless `PR_ORACLE_REQUIRED` is set; no change there.

### 6.4 Docs and accounting

The plan's appendix carries the C/E checklist as a table (C1–C15, E1–E21),
with file:line, current claim and correction. The actionable set is C1–C15
and E1–E18; E19 is a clarification, E20 is a plan-number citation, and E21
is a dated snapshot, so those three are checked and marked deferred rather
than rewritten. C2 (the "8 named gaps" mislabel) is explicit, not folded
into C1. The docs pass applies the list after the counts are final.

The 66% figure stays the README title, qualified as "of Ghidra's 1203
indexed functions" (README:31 and the title). A second metric reports the
span census: 417 slots, 392 distinct targets, six renderers, one `ret`, 385
writers (384 reachable + 1 dead), 227 E2-triaged and 158 untriaged (231 E2
span rows minus the four renderers = 227), plus `0x24B54` live and `0x24B08`
dead. The E2 table stays unchanged so the `--expect 579 --expect-u0 575`
gate remains byte-identical; the census is a separate report, not a new
universe.

## 7. Acceptance

- **Differential:** `make diff-verify` reads approximately `1033/1033
  functions VERIFIED; 14993/14993 mutants detected; 0 named gaps; 825/845
  rows with callees closed (188 have none); 807/845 closed on full rows; 8
  ported functions with no row` (the exact line is re-measured and pinned;
  the arithmetic is a check, not a target). The 12 open rows that close are
  `text_number_core`, `game_quit_prompt` (weak: its `0x24A9C` restart arm is
  unhit) and the ten `game_frame_*` rows; the new `flow_bonus_card_b_step` is
  the thirteenth closed row. The short-write case reopens `config_file_save`
  on the `0x6C4CF` terminal, so the 20 open rows are the declared terminals
  and the weaker entries number 15.
- **Span:** 26/26 reachable classes reached by the minimal fixture; 385/386
  distinct writers by the exhaustive fixture (the dead `0x57E3C` named);
  both clipped renderer both-edges paths hit; every mutant pin re-measured;
  the coverage unittest fails if a payload edit drops a class.
- **Oracles:** 0 order failures on all 24 scenarios; every enforced scenario
  with a frame replay (excluding `gp-u17` and `gp-pads` by name) has a
  measured `GP_*_MAX_MISSING` pin; the synthetic rewind fixture fails; K11
  prints its identity line and passes; a stale or byte-mutated allowance
  fails; no existing pixel pin moves.
- **Gate:** `make gate-check` prints 34/35 present and the declared U14 gap;
  a simulated missing directory fails under `GATE_STRICT=1` and passes under
  `GATE_STRICT=0`; a stray `PR_TITLE_DUMP` no longer replaces the unit
  suite; removing `frames/fn_miss.txt` fails the verify unit run via
  `PR_FN_MISS_REQUIRED` (while `make test` still works); `PR_PINS_EXACT=1` is
  green with no "improved:" line, with the variables exported.
- **Docs:** no contradiction from the C/E checklist remains (E19–E21 marked
  deferred); `symbols.h` regenerates byte-identically; `make entry-triage` is
  unchanged; `port_progress.py` stays `799 1203 66` / `759 759 100`; README's
  title stays 66% with the Ghidra qualifier.
- **Final gate:** `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make
  verify` (or the exported defaults) exits 0 on the phase tip.

## 8. Risks

- **Counter arithmetic.** The closures interact (a row can be blocked by two
  callees; a new row can close another). The plan requires the counter to be
  re-measured from the driver's own line, not predicted.
- **Moving the longjmp to a scan-only stop.** It closes 11 rows but changes
  the failure mode: a future case that enters the restart arm now fails
  closed instead of being stubbed. The `Spec.stop` set keeps the static scan
  at 161/18 leaders; without it the scan follows nine runtime leaders and the
  rows turn `PARTIAL`. The plan adds the guard test and records the
  measurement that no current case arrives.
- **The sprintf allow.** `D3A_SPRINTF` runs the original's tree; the port's
  formatting must match on the row's cases. The `C3F` mutant kinds move from
  a call to a byte, so the pins are re-measured.
- **The short-write reopen.** `config_file_save` is closed today; the
  short-write case reaches `0x1B0F7` but reopens the row on the `0x6C4CF`
  flush. The phase accepts the trade (the block is covered, the row becomes a
  named terminal), and the census/README quote the measured 20-open line.
- **The span exhaustive fixture.** It may expose a real port divergence in a
  run-length/tail class; that is the point. If it does, fix the port in a
  separate, recorded task and keep the class proof.
- **The order rule.** The existential/minimum-feasible rule is measured, not
  fitted; the three anomalies (u8-right-training 585, u9-win 1896,
  u22-diablo 5034) are forward mid-update splices or a scan3 mixed scan, not
  rewinds. The named allowances stay order-neutral. If a future capture
  exposes a new ambiguity, it gets a byte-bound allowance or a named gap,
  never a threshold.
- **Strict default.** A capture-less checkout now fails unless
  `GATE_STRICT=0`; the docs make that the documented partial command.
- **K11 identity.** The current `walk`/`menuesc` captures must match the new
  pins; if they do not, the phase records the measured values and re-pins
  only the identity, not the content pins.
- **Docs sweep.** Large but mechanical; it lands after the counter, oracle
  and gate values are final so it quotes measured numbers.

## 9. Named gaps after the phase

- The 20 terminal differential rows (15 AIL thunks, `sound_buffers_alloc`
  `0x1D0BC`, the input sampler's four host probes, and the three
  runtime-flush rows `config_file_load`, `menu_fatal_error` and
  `config_file_save`) with their reasons.
- The 8 portable functions with no row, as host replacements.
- The 15 weaker entries (`0x249F0`, `0x1D0BC` and `0x51ED8` join the 12) with
  their reasons.
- The 158 span-table targets outside E2's `ret` rule; the span sweep proves
  the port's behavior, not their E2 classification.
- The bounded-missing coverage claim: a state the capture never sampled, and
  a same-count class substitution, remain unclaimed.
- The dead `0x80E0C[128]` writer and the type ≥ 0x20 domain exclusion.
- U14 R1, the U16 live finisher, the death streams, the 5120 cause and
  rendered audibility — unchanged from the endgame phase.

## 10. Task map

| # | Task | Workstream |
|---|---|---|
| 1 | Evidence baseline, span census, record §P1–§P2 | record |
| 2 | Minimal class-complete span matrix + coverage unittest | differential |
| 3 | Exhaustive span matrix + both-edges clipped renderers | differential |
| 4 | Rows `0x25FAC` / `0x26194` | differential |
| 5 | `0x65546` allow + `0x65431` scan-only stop (12 rows close) | differential |
| 6 | `0x1B084` short write + `0x51ED8`/`0x249F0`/`0x1D0BC` weaker fixes (20 open) | differential |
| 7 | Terminal census + `0x42203` divide guard + `0x24B08`/`0x24B54` | differential |
| 8 | Order-preservation claim in `gp_compare` | oracles |
| 9 | Measured `--max-missing` pins + K11 identity/order + allowance honesty | oracles |
| 10 | Gate strict preflight (3 enforced, 35 enumerated) + unit-suite trust | gate |
| 11 | Docs C/E checklist + span accounting | docs |
| 12 | Full gate, re-pin, record, merge | closure |

The plan in `docs/superpowers/plans/2026-10-24-proof-completion.md` carries
the steps, files, tests and acceptance for each task. The derivation record
`docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` is
written during implementation and is the authoritative raw-byte source for
the tasks that implement from it.
