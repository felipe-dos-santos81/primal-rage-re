# Report: Primal Rage (DOS) → SDL3 port — sub-project 4a-iii, title-path residuals

Cycle: `title-residuals`, from `main` at `bd9579d`, merged as the commits below.
Spec: `docs/superpowers/specs/2026-09-18-title-residuals-design.md`.
Ledger: `.superpowers/sdd/2026-09-18-title-residuals/progress.md`.

This is the durable record of the falsifiability outcome and the residuals. It
states the coverage limits plainly: the port is claimed to match the original
only where this cycle's oracle and unit tests actually reach it.

## 1. What this cycle did

The 4a-ii title oracle proved the port against a capture of the **pinned**
original: `tools/title_pin.py` rewrote `0x2BF08`'s first byte (`53`→`c3`) so the
overlay never ran. That is a scope decision, not a behaviour pin, so the oracle
proved the ported subset, not the original. This cycle removed that inert site
(Task 1), re-captured the true original, diagnosed the resulting divergence
(Task 2), ported the overlay `0x2BF08` (Task 3), and ported the `0x13xxx`
effect-list slice the title's call needs — spawn `0x13C70`, clear `0x13DF0`,
free-list build `0x13ADC` (Tasks 4–6), and the per-frame step/age `0x134C0`
(Task 7) — then recorded the outcome and ran the ladder (Task 8).

Commits (`git log --oneline bd9579d..HEAD`): `3a7c28d` … `b22313d`; the cycle
report commit is the one carrying this file.

## 2. The un-pinned capture and Task 1's drift measurement

Task 1 removed only the fifth `tools/title_pin.py` site; `test_title_pin.py` is
now 4 sites (10 passed) and `make title-pin` writes nothing under `data/`. Two
headless captures were taken with DOSBox-X (no human interaction):

```
$ python3 tools/title_capture.py --out data/title-captures/title  --time-limit 45
title_capture: wrote 587 frames to data/title-captures/title (raw 1374..3152)
$ python3 tools/title_capture.py --out data/title-captures/title2 --time-limit 45
title_capture: wrote 590 frames to data/title-captures/title2 (raw 1369..3152)
```

The drift measurement is **wider than the spec's frames-32/64/96 prediction and
is total**:

```
data/title-captures/title : 587 frames  {clean 0, splice 0, transition 0, unexplained 587}
data/title-captures/title2: 590 frames  {clean 0, splice 0, transition 0, unexplained 590}
  NO frame exhibits any port frame in either capture; 0/96 port frames exhibited.
  Unexplained port frames: all of 0..95 in both captures. No window can be formed.
```

The two captures agree exactly at every sampled frame (identical near-miss
distances), so the drift is deterministic, not capture noise. Near-miss margins
are 0.26%–4.7% of frame bytes and vary per frame:

| capture frame | title | title2 |
|---|---|---|
| 226 | 498 | 498 |
| 242 | 3236 | 3236 |
| 258 | 498 | 498 |
| 274 | 498 | 498 |
| 290 | 1776 | 1776 |
| 306 | 9055 | 9055 |
| 322 | 497 | 497 |

Gate decision: **WIDER → STOP and re-scope** (spec Decision 4). The plan's
original Task 5 ("port `0x2BF08` for the 32/64/96 residuals") did not match the
measurement; the human re-scoped to port the overlay and seed its captured
inputs (Option A), keeping `0x13C70` in the cycle. The oracle also crashed on
total non-alignment (`title_compare.py:241` `IndexError`); that was fixed in
Task 1b (empty-window guard returns `(1, None)`; `make title-oracle` now exits 2
with `0/96` and no traceback instead of crashing). A pinned-backup control still
reproduced the known-green window (110 frames, 0 unexplained, 95/96), validating
the method.

## 3. Falsifiability verdicts (spec §6, Decision 5)

Every ported item either moves the oracle and is proven by it, or is declared an
explicit coverage gap with a unit-level proof.

### `0x2BF08` — proven by the oracle; it moves the composite

`0x2BF08` is the `0x11D04` tail's per-frame message/text tick. The un-pinned
original draws `CREDITS:5` (`sprintf("%s:%d", game_string_get(0x46),
DS_00105C00)`) at screen rows 7–12 on **every** frame, from the
`DS_00105D60 == 0` / `DS_00105C00 != 0` branch, which has no `&0x1F` gate — the
port comment at `flow.c:863-875` was wrong about that. Task 3 ported the
four-branch control flow at the existing marker, `FUN_0002CAA8`
(`DS_00105D60 == 0`), and `snprintf`, and seeded the captured runtime inputs
`DS_00105C00 = 5`, `DS_00105C05 = 1` (the row seed was corrected from the
diagnosis's `7` to `1`; one text row is `20/3` px, putting the glyph at screen
rows 7–12).

```
Task 3: make title-oracle GREEN against the un-pinned captures:
  capture 1: 0 unexplained, 94/96 exhibited;  capture 2: 0 unexplained, 94/96 exhibited
Negative control (0x2BF08 stubbed back to a no-op): RED, 0/96 exhibited; restored → green.
```

So the oracle proves `0x2BF08`: with it ported and seeded, the un-pinned captures
are explained; stubbing it out turns the oracle red. Port dump frames
`frame_{0010,0037,0051}` are byte-identical to un-pinned capture frames
`{0226,0258,0274}` (SHA-256 prefixes `ce8f4ddc…`, `2a5ecae3…`, `b16bd2c8…`).

Fidelity limit, stated: the seed reproduces the **no-input** captured window
only. `DS_00105C00` is a live credit counter whose writers `FUN_0002C304`
(initial), `FUN_0002CA48` (`dec`) and `FUN_0002CA7C` (`sub`, from the title input
handler `0x11F28`) are unported; credit countdown on input is carried to 4b.

### `0x13C70` — declared coverage gap; carried by Task 5's unit tests, not the oracle

`0x13C70` is the effect spawn called at the title's `0x123EA` site:
`effects_spawn(node, 3u, 0x419786Cu)` (EAX/DL/EBX per the register binding doc).
Free-list answer from Task 4: **the free list IS populated before the title's
`0x123EA` call** — `0x121A0` state 0 → `0x2BAF4` @`0x121EB` → `0x13ADC`
@`0x2BBB8` builds 24 records (stride `0x814`, `0xF0B00`..`0xFC4CC`), and nothing
empties it before the spawn. Task 6 wired `effects_init`/`effects_clear` into
`actors_reset` and the spawn into the title.

**`0x13C70` does not move the composite in-window, and the oracle does not prove
it.** A probe (since reverted) showed the spawn fires exactly once, on dump frame
95 (`src=0x107628`, `count=31`), which the oracle reports as `missing [0,95]`
and does not compare. There is no ported renderer that draws the spawned effect
record, so the spawn alone produces no composite change. Stubbing the spawn out
leaves the oracle green (a missing frame is disclosed, not failed), so the oracle
cannot distinguish present from absent — this is precisely spec Decision 5's
coverage gap. It is carried by Task 5's unit tests: spawn/teardown, the link
direction (`[+0]`=next, `[+4]`=prev; insert-after vs insert-before proved with
two records), the full 24-record pool, count bounds (signed; one-past-count left
untouched) and the NULL-resolve guard — all mutation-proven (delete zero-loop →
3 fails; swap insert direction → 9 fails; delete NULL guard → SIGSEGV).

The `0x13xxx` render path beyond the spawned record remains unported and stays a
declared gap (spec §2 non-goal, §6).

### `0x134C0` — the count drain; a regression introduced by Task 6 and fixed by Task 7

Task 6's wiring introduced a stall. `effects_spawn` is the only `DS_0009AF3D`
writer in the port (`+1`) besides `effects_clear` zeroing it, and the original's
decrement lives in the `0x13xxx` step/age family. The title's state-2 exit
requires `DSB(DS_0009AF3D) == 0`, so beyond frame 95 the title stalled where it
previously progressed. The reviewer ruled it real, introduced by Task 6, but
outside the 96-frame window; the human elected to port the drain in-cycle rather
than park it (plan errata `77db055`).

Task 7 ported `0x134C0` (`void effects_step(void)`, 1564 B, no arguments)
instruction-by-instruction from the raw at file `0x66314`, called from the
master loop at `flow.c:779` between `DS_00104AF4++` and `gfx_flush_palette()`.

Observation (temporary `PR_TITLE_PROBE`, since reverted):

```
with the step (fixed):
  PROBE f=96  outer=1 phase=2 count=1 type=3 state=3 flag=0 c0=00000000 t0=00f9f9f9 cnt=31
  PROBE f=195 outer=1 phase=2 count=0 rec=empty          <- last PROBE line
drain disabled (effects_step stubbed to return;):
  probe lines 260 for --check 260; count stays 1, phase stays 2, game_state_title every frame, nothing animates
```

The type-3 fade moves `+0x10` toward `+0x410` by 8/channel per age wrap (every 3
frames); it reaches `0x00F9F9F9` at `f=195`, where the count is 0 and the
`flow.c:432` guard fires (`DS_000F0A6F = 2`), so the title leaves state 2 rather
than stalling. Unit proof: `effects_active()` 1→0 after 4 steps, record re-linked
to the free list exactly once, 24 free (negative control: stubbing the step made
4 assertions fail). Oracle consistency: frame 95 moved from `missing` to
exhibited (window 110→111 frames, exhibited 94→95/96, `missing [0]`); the type-3
first-wrap palette pass legitimately alters the presented palette, and the hard
requirement — oracle green, untouched — holds at 0 unexplained.

## 4. The full ladder

`rm -rf build && make verify` (re-run after the docs edits): **exit 0**, no
compiler warnings (0 clang `warning:`/`error:` lines; 240 pre-existing Python
`ResourceWarning`s from the `gra_extract` tests, unchanged by this cycle).

| stage | result |
|---|---|
| `cmake -S port -B build && cmake --build build` | built clean, 0 warnings |
| `./build/prageport --check 60` | ran headless |
| `PR_ORACLE_REQUIRED=1 ./build/run_tests` | `all checks passed`; oracle 9340 writes byte-exact; the pre-existing C-vs-capture first difference at C write 2 is unchanged |
| `smk_compare` | **120/120** (twi5) and **41/41** (twg) |
| `title_compare` | capture 1: 111 in window — 54 clean, 55 splice, 2 transition, **0 unexplained**, exhibited **95/96**, `missing [0]`; capture 2: 111 — 54 clean, 57 splice, 0 transition, **0 unexplained**, exhibited **95/96**, `missing [0]`; determinism 54 agree / 0 disagree |
| `re-extract-test` | 32 tests OK |
| `symbols.h` | regenerates **byte-identical** |

## 5. Residuals carried out (spec §8, updated for this cycle)

Ported this cycle, so removed from the carried list: `0x2BF08`, `0x13C70`,
`0x13DF0`, `0x13ADC`, `0x134C0` (and the private `0x13420`).

Carried:

* `0x11000` attract sub-machine, `0x38A38` (`DS_00107900`'s producer), `0x389C4`,
  `0x292AC`, `0x4F644` — 4b/4d.
* `0x10DB0`/`0x10E18` — input-gated; deferred to 4b with the existing evidence.
* The `0x13xxx` **render path** beyond the spawned record — declared gap (§3).
* `DS_00105C00`'s live credit countdown (`FUN_0002C304` initial,
  `FUN_0002CA48`/`FUN_0002CA7C` decrements via `0x11F28`) — seeded for the
  no-input window only; 4b with the input model.
* `DS_00105C05`'s writers `FUN_0002BF00`/`FUN_0002C06C` — row seeded; 4b.
* `0x1B544`'s on-demand loader branch (`0x1E774`/`0x1D290`). The title handle
  `0x419786C` (index 8, `s16attrc.gra`) has entry flag `0x02`, not `0x1000000`,
  so the original takes this branch; the port's `res_resolve` fast-path
  substitution is self-consistent on the shipped path (load-time block offset,
  `mem + data + offset`) but the original branch itself is unported.
* `0x134C0`'s types 1/2/4/5/6 are transcribed but unreachable in the port today
  (`effects_spawn` hardcodes type 3); add body-specific tests when a caller
  spawns them.
* Master-loop ordering divergence: the port runs `render_list_sort`/`render_list`
  before `DS_00104AF4++`/`effects_step` where the raw runs them after
  (pre-existing, carried).
* 4a-i's bank byte 0 and the `DS_00107A3E`/`3A`/`38` projection load width.
* The 2a OPL register-stream divergence (`make verify` reports first difference at
  C write 2: `reg=0x20 val=0x00` vs the capture's `reg=0xb0 val=0x2b`, a key-on)
  and the `0x2D974` save/config pin — 4b/2a-ii.
* Streamed Smacker audio (2b-ii) — dropped from this cycle (both movies silent,
  no ported caller); until a caller exists.

### Deferred minors (ledger)

* Task 1: the plan errata's backup command
  (`cp -R data/title-captures data/title-captures.pinned-backup`) writes under
  `data/`, against the read-only-data constraint; the implementation correctly
  used scratch outside the repo, the plan text still needs correcting.
* Task 1: `tools/tests/test_title_pin.py` relies on `len(PATCH_SITES)` rather
  than a literal `== 4` assert.
* Task 2/Task 4: `task-4-report.md:74` still says "next (state-1) frame",
  inconsistent with the corrected args doc §4.
* Task 5: the fix report's mutation-proof line numbers are stale
  (`:97/98/99` vs the committed `:110-112`); values match, proof genuine.
* Task 7 review minors: three explanatory comments are not prefixed
  `PORT:`/`TODO(verify):`; a latent type-5 drain-without-removal path is
  unreachable while the spawn hardcodes type 3.

Resolved in-cycle, not carried: the `title_compare.py:241` total-non-alignment
crash (Task 1b); the Task 6 count-drain stall (Task 7).

## 6. Files changed by this cycle (code and docs)

Code: `port/src/game/effects.{c,h}` (new), `port/tests/test_effects.c` (new),
`port/tests/test.h`, `port/tests/run_tests.c`, `port/CMakeLists.txt`,
`port/src/game/actors.c`, `port/src/game/flow.{c,h}`, `port/tests/test_flow.c`.
Tools: `tools/title_pin.py`, `tools/tests/test_title_pin.py`,
`tools/title_compare.py`, `tools/tests/test_title_compare.py`.
Docs: this report, `docs/superpowers/plans/2026-09-18-bf08-overlay-diagnosis.md`,
`docs/superpowers/plans/2026-09-18-title-residuals-args.md`, the spec, the plan
and the ledger, `port/RE_GUIDE.md`, `port/spec/game_flow.md`, `README.md`.
`data/` was never modified; explicit paths only (no `git add -A`).

## 7. Verification claim and its limits

Proof for this cycle is the re-pinned title oracle plus the falsifiability record
above. It is **not** a claim that the port matches the original for any path the
window does not reach: `0x11000`, the `0x13xxx` render path beyond the spawned
record, the input-gated tail, the attract sub-machine, the mode-1 sprite branch,
and the credit-countdown data path are each named above rather than implied
green. `0x2BF08` is oracle-proven; `0x13C70` is a declared coverage gap with its
unit proof; the `0x134C0` drain is unit-proven with the oracle green.
