# Design: Volleyball truth — the mode-`0x21` route search and exhibit

**Status:** implemented and gated (2026-11-01): the route, the guarded capture
`data/k11-captures/gp-u30-volleyball`, `tools/gp_volley.py`, the oracle
`gp-u30-volleyball-oracle` and the register R05 `closed`; implementation plan
`docs/superpowers/plans/2026-11-01-volleyball-truth.md`, record
`docs/superpowers/plans/2026-11-01-volleyball-truth-derivations.md` §2–§3.
**Authority:** register R05 (`docs/superpowers/specs/2026-10-30-final-claims.md`),
`port/spec/volleyball.md`, final-truth record §F6, and the repo's evidence rules
(`AGENTS.md`).
**Base commit:** `edbc35de`.

## 1. Goal

Move R05 from "the mechanism and four first hits are measured; no exhibit" to
one of two measured outcomes:

- **closed:** one guarded capture of a real-input two-human stage-0 match whose
  replay reaches `entry+0x1F == 8` and the mode-`0x21` entry, exhibits the
  court, a point and the return to mode 6, with a checker, oracle and pins; or
- **declared with a measured bound:** the search's parameter space, the best
  state it reached, and the reason no route was found. Both are evidence; a
  port-side dump alone is not a capture and publishes nothing.

No poke may write `DS_00104B1F`/`DS_00104ABC` (register R30), and no constant
is fitted: the stage, the lie, the count and the flight come from the raw bytes.

## 2. Preflight findings

Four read-only preflights (route, harness, Ghidra, capture) are recorded in
`.superpowers/sdd/2026-11-01-volleyball-truth/preflight/`. The findings that
change the route:

### 2.1 The trample is body contact, not a strike

`0x4B69C` is called from the effects prelude (`0x49CFE`, before the type
dispatch) for every entry every iteration. It tests only `entry+0x1C` bit 7
(`0x4B6A6`) and the entry actor's pset point against both fighters' boxes
(`0x17D30`; the box is `DS_00100A90[side]`, copied to `DS_00100AC8[side]`). A
fighter standing or jumping under the descending body tramples it; no button is
tested. `0x4B788` must return 1 (the fighter is not in its grab move
`+0x5F == 0xC97F2[ch]`, the other slot's `+0x54 != 3`, `DS_00105B3A <= 1`);
the return-0 arm is the grab and counts nothing. The count increments at
`0x4B716` (`+0x1F`), the `> 7` gate is `0x4B579`, and the mode entry is
`0x4BD98` from `0x4B5A8`'s tail.

### 2.2 The lie is a two-step crowd event

`0x4B5A8` is the crowd **walk-in spawner** (type 3, target fighter
`±0x1800`), not the lie. The lie is the case-3 landing (`0x49E47`/`0x49E52`):
type 4, `+0x1C |= 0x80`, a `rng(0x3C)+0x3C` (60..119-frame) lie timer. The
gate is `DS_001088B6[side] != 0` — the combo budget set by `0x3907B` when the
consecutive-hit word `0x107D2C[side] > 1` — or the dizzy state
`slot+0x52 == 7 && slot+0x53 == 2` (`0x36BC8`). Both are hit-driven: a 2-hit
combo lies the attacker's own crowd; a sustained beating lies the victim's.

### 2.3 A mid-round join keeps the 1P stage (route B)

`flow_player_join` sets `DS_00104B17 = 2` (`0x28E6D`) and `flow_stage_pick`
returns on `m == 2` (`0x2585D/0x2585F`); the join's char-select/vs tail calls
the pick (`0x430FE`) but it writes nothing, so the stage from the 1P match
survives. The 1P pick is `0xA87C4[c] + rng(6) + 1`, reduced by 7 when `>= 7`
(`0x25897..0x258C6`), with `c = DS_0010816A` (the 1P selected character). The
base table is `{0,4,2,3,6,1,5}` (data `0xA87C4`): character 0 can never give
stage 0; each of characters 1..6 can, for one `rng(6)` value. The character
cursor and the select timing are the knobs. This replaces the front-end
`rng(7)` 1-in-7 retry as the primary route.

### 2.4 The flight and the window

After a counted hit `0x4B470` stores `vy = 0x240` (`0x4B52E`), type 6
(`0x4B537`) and clears bit 7 (`0x4B544`); the same-pass case-6 body subtracts
`0x10` (`0x49FB8`), and `motion_step` integrates `height += vy`:
`h(n) = 568n - 8n²`, 71 motion steps, apex `0x2760` at `n = 35/36`, landing at
`n = 71`. The descent latch is set from `n = 37` (`0x49F18`), but the
fighter-box geometry (`0x1790C`/`0x179F8..0x17B10`, 56 px) reaches the body
only in the **last 7 frames** `T+65..T+71` (the preflight's derived practical
window; the raw latch window is 35 frames). A hit flips `vx` (`0x4B507..`), so
the body bounces between two fighters ~142 px apart (2 px/frame × 71). A miss
lands the body and zeroes `+0x1F` (`0x49FAF`).

### 2.5 The spec has errors to correct

`+0x10` is the **shadow actor**, not a held actor; `+0x34`/`+0x36` are the
**actor's** vx/vy integer words, not entry fields; `0x4B5A8` is the walk-in
spawner, not the lie; the spec omits the `0x4B788` grab gate; the `+0x1F` zero
list has `0x4E4E5` (not `0x4E4E8`) and omits `0x4C1E8`/`0x4C27F`/`0x4C349`/
`0x4C904`/`0x4CAA9`; the `0x1A570` polarity is unresolved in §7; record
§F6.7's "gp-idle-loss 2,565 frames" is wrong (the capture is 8,173 frames); and
the `f=2238` figure is decimal while the traces print hex (`0x8BE`). These
corrections are Task 1.

## 3. The route

**Primary (route B):**

1. Boot to the 1P arcade; in the 1P character select choose a character whose
   `0xA87C4` base plus the select's `rng(6)` gives stage 0 (try the seven
   cursor positions and a few select timings; read `DS_00104AFC` at the vs).
2. Start the 1P fight; at mode 6 press P2 start (`0x28CC8` join). The join
   re-enters the character select/vs (`m = 2`), both players confirm, and the
   fight resumes on the same stage 0 with `b1f = 3` and `DS_00104ABC = 2`.
3. Land a 2-hit combo (P1 on P2) to set `DS_001088B6[0]`: the side-0 crowd
   walks in; the case-3 landing lies it (type 4, bit 7, 60..119 frames).
4. Put a fighter over the lying body. The first trample launches it; the
   second fighter stands at the far end of the 142 px flight. The body's count
   advances each landing window until `+0x1F > 7`; the 8th hit enters
   `0x4BD98` (mode `0x21`).
5. From the entry, the port's deterministic replay is the capture's frame path;
   the capture must show the court actors' walk-on, a point (landing or eat),
   the scoreboard and the return to mode 6.

**Fallback (route A):** the front-end two-human `rng(7)` stage 0 (1-in-7),
same fight chain.

## 4. The search

Reuse the existing GP replay path rather than a new fork-per-variant binary:

- a `PR_GP_FAST=1` host seam skips the 60 Hz `host_wait_vblank` sleep (still
  pumps), making a replay CPU-bound;
- a `PR_GP_VOLLEY=1` observer emits a per-iteration `V` line walking the active
  effects list `DS_0010884C` (entry `+0x1E/+0x1C/+0x1F`, actor x/depth/height/
  vx/vy) and a `Vg` line with `DS_001088B6[0..1]`, `DS_00108864`,
  `DS_00104ABC` and the mode-`0x21` bytes;
- `tools/vb_search.py` generates port-script v2 variants, runs
  `run_tests`/`PR_GP_DUMP` with `PR_GP_FAST=1` in parallel over per-variant
  `/tmp` dirs, and ranks the traces by stage, `b1f`, lie frame, max `+0x1F`
  and mode `0x21`.

The search is bounded: the stage cursor/timing grid, the join frame, the combo
timing and the two fighter positions; then the passive rally. If no route is
found, the record keeps the grid, the best state and the reason.

The observer is **inert without its env var**: with `PR_GP_VOLLEY` unset the
driver writes no `V`/`Vg` line (the Task 4 T-line fields are separate and
additive; the preexisting T values and the frame dump are unchanged). The
search tool is not in `make verify`. The one deliberate in-gate use is
`gp-u30-volleyball-oracle`, which sets `PR_GP_VOLLEY=1` on its own
`gp-replay` so `gp_volley.py --trace` can read the port's entry `+0x1F`
count and the `T`/`Vg` entry/return stores; that replay's frame dump is the
same and no existing pin or default output path moves.

## 5. The capture

One guarded `gp-u30-volleyball` capture, cloned from `gp-bowling-2`/`gp-twop`:
scenario in `gp_session.SCENARIOS`, `stop_mode 0x21`, the new S/V fields, the
session checks, and the minimum publishable set: the court, a real point, the
scoreboard, the return to mode 6; checker `tools/gp_volley.py`, oracle
`gp-u30-volleyball-oracle`, miss set, manifest and `CAPTURE_DIRS`. A failed run
is preserved as `.failed` and publishes nothing.

## 6. Claims and gates

- R05 closes only with the capture and its oracle; otherwise it stays
  `declared` with the measured search bound (a new evidence string, no new
  claim).
- The spec corrections are documentation, not claims; the register test and
  `test_final_claims` must stay green.
- If a capture/oracle is added, the full gate is `make verify` and
  `make audio-render`; if not, `make verify` still runs (the new `V` observer
  is inert outside `PR_GP_VOLLEY`; the search tool is not in the gate; the
  `gp-u30-volleyball-oracle` deliberately sets `PR_GP_VOLLEY=1` for its own
  replay, so the checker can read the port's `+0x1F` count and `T`/`Vg` entry
  and return stores, and the default output paths are unchanged).

## 7. Out of scope

- No poke of `b1f`/`DS_00104ABC` (R30); no poked 1P exhibit.
- No new claim for the mode's unmeasured content beyond what the capture shows.
- No change to the audio, input or pixel models.
