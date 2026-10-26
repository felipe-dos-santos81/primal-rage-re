# Fought-Match Truth — derivations

Status: in progress, 2026-10-26. Branch `fought-match-truth`, base `a5552639`
(`docs: plan the fought-match truth phase`). Plan
`docs/superpowers/plans/2026-10-26-fought-match-truth.md`; spec
`docs/superpowers/specs/2026-10-25-fought-match-truth-design.md`.

The record is the authoritative raw-byte source for the tasks that implement from it.
§F1 is Task 1 (the harness: the fighter-slot state fields, stop-on-mode and
skip-on-mode), which changes no game code and no oracle pin. Later tasks append their
own sections.

Conventions: an address is a Ghidra linear address (code `0x10000..0x73B14`, data
`0x80000..0x10B0CF`, `DSB/DSW/DSD(addr)` in the port's flat `mem[]`); a raw-file byte
offset is not a runtime address (the LE fixups rewrite the data references — AGENTS.md's
address model). **On any plan-vs-raw conflict the raw wins**; the corrections are
recorded in the section that measures them.

## §F1 The fighter-slot S fields and stop-on-mode (Task 1)

### §F1.1 The four appended fields

`tools/gp_session.py`'s `SNAP_FIELDS` gains, appended after `dev1`/`dev2` so every older
`poll.log`'s `S` lines simply lack them:

```python
('s0_30', 0x1077E0, 4), ('s0_42', 0x1077F2, 2),
('s1_30', 0x107874, 4), ('s1_42', 0x107886, 2),
```

The addressing rule is slot base `0x1077B0` + `side * 0x94` + the offset:

| slot | `+0x30` (dword) | `+0x42` (word) |
|---|---|---|
| 0 | `0x1077E0` | `0x1077F2` |
| 1 | `0x107874` | `0x107886` |

**`+0x30` is four bytes.** `0x12DA8` (`camera_y_commit`) reads both slots as dwords
(raw bytes at `0x12DD2`/`0x12DD7`; the immediates are pre-fixup raw-file values
`0x000877E0`/`0x00087874`, and the data object's fixup base `0x80000` makes them
`0x1077E0`/`0x107874`):

```text
0x12DD2  a1 e0 77 08 00            mov eax,dword ptr [0x877E0]     -> 0x1077E0 (slot 0 +0x30)
0x12DD7  8b 15 74 78 08 00         mov edx,dword ptr [0x87874]     -> 0x107874 (slot 1 +0x30)
0x12DDD  39 d0                     cmp eax,edx
```

The same function's mode-0 arm establishes the `0x94` stride raw:

```text
0x12DBA  8d 04 d5 00 00 00 00      lea eax,[edx*8]
0x12DC1  01 d0                     add eax,edx
0x12DC3  c1 e0 02                  shl eax,2
0x12DC6  01 d0                     add eax,edx                    ; eax = 0x25 * slot index
0x12DC8  66 8b 04 85 e0 77 08 00   mov ax,word ptr [eax*4 + 0x877E0]   ; 0x25 * 4 = 0x94 bytes
```

`0x12DA8`'s field meaning is the demo-fight record's
(`2026-09-20-demo-fight-derivations.md` §1.1/§2.7): `+0x30` is the **latched y**, a copy
of the fighter record's `+0x1C` (`0x186B0`/`0x18ACA`), and the second slot begins at
`0x107844` (`= 0x1077B0 + 0x94`). The port already reads it as a dword
(`port/src/game/camera.c` `camera_y_commit`, `DSD(DS_001077E0)` and
`DSD(DS_001077E0 + 0x94u * idx)`), so the field width is not a new claim.

**`+0x42` is two bytes, a word.** `FUN_00036f10`'s arm A, the one write that sets the
U16 gate's `+0x42` bit 5 and `+0x43` bit 3 (`2026-10-23-endgame-truth-derivations.md`
§E1.2, re-measured here from the image):

```text
0x36FC0  66 8b 56 42               mov dx,word ptr [esi+0x42]
0x36FC8  81 ca 20 08 00 00         or  edx,0x820
0x36FD4  66 89 56 42               mov word ptr [esi+0x42],dx
```

`ESI` is the slot pointer (`0x36F10`'s `param_1`; the caller passes the actor slot
record, `__regparm3`). The word's low byte `0x20` lands in `+0x42` and its high byte
`0x08` in `+0x43`, which is why the capture needs the **word** at `+0x42` and not a byte:
a byte field would hold `0x20` alone. The decompiler's byte read
`*(byte *)((int)&DAT_001077f0 + param * 0x94 + 2)` (`port/decomp/prage.c:5432`, slot
`+0x40`'s dword with `+2`) is a sub-view of the same word at `0x1077F2`/`0x107886`; the
same dword view is where the e-open scan names the indexed form
`[idx*4 + 0x1077F0/0x1077F2]` (`2026-09-29-e-open-derivations.md`, the `+0x42` writer
scan).

### §F1.2 The old `S` line still parses

The new names are appended, so a pre-phase line has no `s0_30`/`s0_42`/`s1_30`/`s1_42`
keys and `gp_session.parse` leaves them absent rather than defaulting them to zero. The
literal old line of the test (`test_an_old_s_line_without_the_state_fields_still_parses`,
the format before this phase) parses to:

```text
kind=S f=1234 mode=0006 b1f=03 dev1=0000 dev2=0000
s0_30 in r: False | s0_42 in r: False | s1_30 in r: False | s1_42 in r: False
```

(The absence is load-bearing: a predicate that opts in tests membership, as
`gp_compare --dev-words` and `gp_win.py` do, so a later phase can tell an old capture
from a new one without a version byte.) The `dev1`/`dev2` fields are no longer the last
two: `SNAP_FIELDS[-6:-4]` is `dev1`/`dev2` and `[-11:-6]` is the five U6 fields, which
the two existing append tests now assert.

### §F1.3 `stop_mode` and `skip_on_mode`

`Schedule(steps, stop_mode=None, stop_delay=0, skip_on_mode=None)`; each mode argument
is an int or a tuple/list of ints (U14 passes `(0x08, 0x09)`). `stop_delay` is in
frames, a **harness value** a scenario supplies (`stop_delay` in the scenario dict), not
a raw constant.

- `on_mode(f, mode)` with `mode in stop_modes` (first such entry only:
  `end_frame is None` guard) sets `stopped_on_mode` and `end_frame = f + stop_delay`.
  The capture then writes its `X` record at that frame and stops
  (`STOP_TAIL` frames later), exactly as an `('end',)` step's end does.
- `on_mode` with `mode in skip_modes` walks `self.i` to the next `after_entry` step
  (or past the end) and counts the steps it passed as `skipped`, except an
  `until_mode` and an `('end',)` action. It runs on every entry into that mode; after
  one skip `self.i` sits at an anchor (or past the end), so a second entry in the same
  frame skips nothing.
- `Schedule.steps_ok` is the completion rule: `fired + skipped == total or
  stopped_on_mode`. It replaces `fired == total` in `gp_capture.run_checks`, whose label
  is now `steps fired %d/%d (%d skipped)`.
- `Poller.__init__` takes the three values and passes them to `Schedule`;
  `gp_capture.main` reads `scn.get('stop_mode')`, `scn.get('stop_delay', 0)` and
  `scn.get('skip_on_mode')`. `Poller.run` guards the `fire(...)` call with
  `if self.sched.end_frame is None:` so a stop-on-mode capture injects none of the
  remaining queued input (a skip-on-mode capture already advanced `i` past the dropped
  steps, so its own build of the list is correct without the guard).

The two `tools/tests/test_gp_capture.py` clauses from the brief's two "Step 7" blocks are
both implemented: the poller-construction test and the `test_opt_in_scenarios` clause
(`... or gs.SCENARIOS[name].get('stop_mode') is not None`, the positive cases being the
U14 and two-human scenarios Tasks 2 and 4 add).

RED, before the implementation (`python3 -m unittest tools.tests.test_gp_session -k
state_fields -k old_s_line -k stop_on_mode -k skip_on_mode -v`):

```text
ERROR: test_skip_on_mode_drops_the_round_and_resumes_at_the_next_anchor
TypeError: Schedule.__init__() got an unexpected keyword argument 'skip_on_mode'
ERROR: test_stop_on_mode_arms_the_end_frame_and_counts_as_done
TypeError: Schedule.__init__() got an unexpected keyword argument 'stop_mode'
FAIL: test_the_state_fields_are_appended_with_their_addresses_and_widths
AssertionError: Tuples differ: (('c1', 1079486, 1), ('s0_43', 1079283, 1), ...) != (('s0_30', 1079264, 4), ...)
Ran 4 tests in 0.001s

FAILED (failures=1, errors=2)
```

(`test_an_old_s_line_without_the_state_fields_still_parses` passes before and after: it
is a characterization test of the parser's append-only contract, not of the new code.)

GREEN, the six new/updated cases (`python3 -m unittest -v` with their full names):

```text
test_the_state_fields_are_appended_with_their_addresses_and_widths ... ok
test_an_old_s_line_without_the_state_fields_still_parses ... ok
test_stop_on_mode_arms_the_end_frame_and_counts_as_done ... ok
test_skip_on_mode_drops_the_round_and_resumes_at_the_next_anchor ... ok
test_the_poller_passes_the_stop_modes_to_the_schedule ... ok
test_steps_ok_accepts_a_skipped_round ... ok
Ran 6 tests in 0.001s

OK
```

The two suites (`tools.tests.test_gp_session tools.tests.test_gp_capture`) ran 113
tests with `OK (skipped=1)` (base was 107); the wider tool set
(`test_gp_session test_gp_capture test_gp_compare test_gp_moves test_gp_keys
test_gp_u17 test_gp_twop test_gp_modes test_gp_win`) ran 349 with `OK (skipped=4)`;
`make build` exited 0.

Mutation proofs (each assertion fails when its code is mutated, then reverted):

| mutation | failing assertion |
|---|---|
| `Poller.__init__`: `gs.Schedule(steps, stop_mode, stop_delay, skip_on_mode)` → `gs.Schedule(steps)` | `Tuples differ: ((), 0, ()) != ((8, 9), 240, (8, 9))` |
| `steps_ok`: `fired + skipped == total or stopped_on_mode` → `fired == total or stopped_on_mode` | `False is not true` |
| `gp_trace_line`: `s0_42=%04X` → `s0_42=%08X` | width lists differ at the `s0_42` slot |

### §F1.4 The port trace's field order

`port/tests/test_game.c`'s `gp_trace_line` appends the four names after `dev2`, in
`SNAP_FIELDS` order:

```text
... r0 r1 c0 c1 s0_43 dev1 dev2 s0_30 s0_42 s1_30 s1_42 lat spz mpz ...
```

with `s0_30=%08X s0_42=%04X s1_30=%08X s1_42=%04X` (8, 4, 8, 4 hex digits for the
4/2/4/2-byte fields). Measured with the test's own extraction: the `T` line's 54 field
names equal `gs.SNAP_FIELDS + gs.KEYS_EXTRA + gs.WIN_EXTRA` in order. The arguments read
the raw addresses:

```c
(unsigned)DSD(DS_001077B0 + 0x30u),
(unsigned)DSW(DS_001077B0 + 0x42u),
(unsigned)DSD(DS_001077B0 + 0x94u + 0x30u),
(unsigned)DSW(DS_001077B0 + 0x94u + 0x42u),
```

The DSW/DSD accessor is the width rule: `DSD` for the `+0x30` dword, `DSW` for the
`+0x42` word.

### §F1.5 Plan-vs-raw correction

The brief's argument block writes the symbol with a trailing `u` (`DS_001077B0u` for
every one of the four arguments). `port/src/symbols.h` defines
`#define DS_001077B0 0x001077B0u` — the `u` is already inside the macro, so
`DS_001077B0u` is an undefined identifier and does not compile. The port uses
`DS_001077B0 + 0x30u` etc.; the address is unchanged (`0x1077B0`, `+0x30` = `0x1077E0`,
`+0x94 + 0x30` = `0x107874`). The correction is the brief's only raw conflict; every
address and width in §F1.1 is the raw's.

**Named gap.** The port-trace unit test checks these fields' names, placeholder widths
and accessor letter, but its per-address assertion (`test_the_port_t_line_writes_every_snap_field_in_order`)
only sums a single `DS_ + 0xoffset` form and covers `MOVE_FIELDS` and
`KEYS_EXTRA + WIN_EXTRA`; a two-term `+ 0x94u + 0x30u` argument does not fit that
regex, and this task does not extend it. The slot addresses are therefore asserted
here and in `SNAP_FIELDS` (whose entries the Python test pins), not by the C-side test;
the raw evidence above is the source. Task 5's diagnostic replay, which consumes
`s0_30`/`s0_42`, is the place to add a two-term address assertion if it wants one.

## §F2 U14's measured script and the one authorized run (Task 2)

Outcome: the scenario is the measured 160-attempt form; the phase's one authorized capture ran
on 2026-10-26 and **failed the game's win**. P1 (character 0, SAURON) was KO'd in both rounds
(`s0_5a = 0x78` at both results, `b0c = 0` in every S record), P2 (c1 = 3, VERTIGO; afc = 3) won
the match 2-0 (`w2 = 0, w3 = 2, ad4 = 1` at mode 9), and mode 0x12 (the conquered-lands stop)
never opened. The capture is preserved as `data/k11-captures/gp-u14-win-live.failed`; the target
directory stays absent, all nine `GP_U14_*` pins stay empty, and `GATE_NAMED_GAPS` keeps it
declared (`gate: captures 34/35 present, 1 missing`, exit 0). R1 is still a named gap (spec §10;
record `2026-10-23-endgame-truth` §E8 and `2026-10-20-gameplay-truth-ii` §J6). No second run was
made.

### §F2.1 The scenario change

`_u16_round` and its constants (`U16_GAP = 16`, `U16_WALK = 12`, `U16_GROUP = 8`) moved above the
U14 block, beside `U14_PUNCH`/`U14_KICK`/`U14_GAP`; the U16 block below keeps its comment and its
callers and points at the builder. `U14_ATTEMPTS`/`U14_PRESSES` had no other reader and are gone;
`U14_GAP = 16` stays the documented 1A/1B gap. The scenario is:

```python
SCENARIOS['gp-u14-win-live'] = dict(
    time_limit=260, extra=WIN_EXTRA,
    stop_mode=0x12, stop_delay=0, skip_on_mode=(0x08, 0x09),
    steps=WIN_MENU
    + _u16_round(160, 10, anchor=(0x06, 1))
    + _u16_round(160, 10, anchor=(0x06, 2)),
)
```

The builder's own output (measured, not counted by hand): per round 1 `after_entry` anchor + 159
`after` attempts + 19 `p1.right` walks (at k = 8, 16, ..., 152) = 179 steps; the scenario is
`4 + 179 + 179 = 362` steps, each round 160 attempts and 19 walks of 12 frames. Each walk fires
16 frames after the previous attempt (the 1A/1B gap) and holds 12; the attempt after it fires 20
frames later, so the two attempts a walk separates are 36 frames apart. Nominal frames from
round 1's anchor through round 2's last attempt: 5,868 (~98 s at 60.05 Hz), inside
`time_limit=260`. The old `('until_mode', 0x12, 0)` step is replaced by
`stop_mode=0x12, stop_delay=0` (a whole-capture stop on the conquered-lands screen), and
`skip_on_mode=(0x08, 0x09)` drops a round's queued presses at its round/match result (Task 1's
`Schedule` change).

Provenance of the 160/8/260 values: the measured ~3 landed hits per 32 attempts (spec §7.1), the
U23 160-attempt run's KO at attempt 88 (record `2026-10-23-endgame-truth` §E2), and the round
timer's ~3,280 frames; the script's source comment in `tools/gp_session.py` states each.

### §F2.2 RED then GREEN

Before the scenario change (`time_limit` 200, the two 32-attempt rounds):

```text
$ python3 -m unittest tools.tests.test_gp_session.TestLiveWin -v
FAIL: test_each_round_has_160_attempts_and_a_walk_every_eight
  AssertionError: 32 != 160
FAIL: test_the_live_win_has_no_pokes
  AssertionError: 200 != 260
Ran 2 tests ... FAILED (failures=2)
```

After it:

```text
$ python3 -m unittest tools.tests.test_gp_session -v
Ran 71 tests in 0.128s
OK
$ python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_capture -v
Ran 114 tests in 0.842s
OK                                  (113 after Task 1; +1 smoke shape test)
$ python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_capture \
    tools.tests.test_gp_compare tools.tests.test_gp_moves tools.tests.test_gp_keys \
    tools.tests.test_gp_u17 tools.tests.test_gp_twop tools.tests.test_gp_modes \
    tools.tests.test_gp_win
Ran 350 tests in 9.010s
OK                                  (349 after Task 1)
$ python3 -m unittest tools.tests.test_gp_win
Ran 42 tests in 3.029s
OK
```

The smoke shape test is `TestHarnessSmoke.test_the_smoke_is_a_throwaway_stop_on_mode_preflight`:
the scenario exists, is in `STOP_AT_END`, has no pokes, and its last step is a pad step.

### §F2.3 The disposable harness smoke

`gp-harness-smoke` (30 s, `stop_mode=0x27`, `stop_delay=10`, one `after_mode` press 30 frames into
the mode):

```text
$ DISPLAY=:1 timeout 300 make gp-capture scenario=gp-harness-smoke
gp_capture: snapshots 322, f 5..154, 14 frames missed (spec §3.7)
gp_capture: CHECK base: ok
gp_capture: CHECK steps fired 1/2 (0 skipped): ok
gp_capture: CHECK end frame reached: ok
gp_capture: CHECK mode 0x27 after the Enter: ok
gp_capture: CHECK snapshots kb == raw (0 differ): ok
gp_capture: CHECK frames written 81/81: ok
gp_capture: CHECK port script v2: ok
gp_capture: CHECK no unscripted input: ok
gp_capture: CHECK pokes written 0/0, 0 raced: ok
gp_capture: CHECK stopped at the end (SIGTERM at f=0154, rc=0): ok
gp_capture: wrote 81 frames to .../data/k11-captures/gp-harness-smoke (raw 1405..1722), wall 26.9s
```

`steps fired 1/2 (0 skipped)` is the stop path proven: the `after_mode` press never fired (the
`end_frame is None` guard in `Poller.run`), the end frame was reached, and DOSBox-X was stopped by
SIGTERM at f=0x154. The capture directory is throwaway: `gp-harness-smoke` is not in
`CAPTURE_DIRS`, not in the audio traces, and no oracle reads it.

### §F2.4 The authorized capture's harness checks

```text
$ DISPLAY=:1 timeout 600 make gp-capture scenario=gp-u14-win-live
gp_capture: snapshots 9471, f 5..302F, 2860 frames missed (spec §3.7)
gp_capture: CHECK base: ok
gp_capture: CHECK steps fired 137/362 (225 skipped): ok
gp_capture: CHECK end frame reached: FAIL
gp_capture: CHECK mode 0x27 after the Enter: ok
gp_capture: CHECK snapshots kb == raw (0 differ): ok
gp_capture: CHECK frames written 8928/8928: ok
gp_capture: CHECK port script v2 (key 70 (p1.b3) at f=69D was not queued: the BIOS ring was full): FAIL
gp_capture: CHECK unscripted input: 0 key words, 2 pad presses, 6 pad releases, first at f=71A: FAIL
gp_capture: CHECK pokes written 0/0, 0 raced: ok
gp_capture: CHECK stopped at the end (SIGTERM at f=-, rc=0): FAIL
gp_capture: wrote 8928 frames to .../data/k11-captures/gp-u14-win-live.failed (raw 1393..18218),
  wall 265.0s; a CHECK failed, .../data/k11-captures/gp-u14-win-live left as it was
```

The first failing check in the print order is `end frame reached`: mode 0x12 never opened, so no
`X` record was written and the run ended on the 260 s time limit (`E ms=264587 reason=time-limit
rc=0`), which is also `stopped at the end`'s failure. `steps fired 137/362 (225 skipped)` passes
and is the first real-capture evidence that Task 1's skip path works (`fired + skipped = 362`,
225 deliberately dropped at the two results). `port script v2` fails, so this capture has no port
replay, dump, compare or pins — the failure branch, not a measurement.

### §F2.5 The BIOS-ring burst (harness, not game)

The poller logged 9,471 S records over the run's f range 5..0x302F, with the tool's
`2860 frames missed` (spec §3.7). At f=0x69D it accepted its first snapshot for 0x80 frames
(previous S f=0x61D) and fired 16 queued I records (8 attempts) in one spin. The BIOS ring
(`0x1E..0x3E`, 32 words, empty before the burst: head = tail = 0x2C at S f=0x69D) took 15 of
them and refused the 16th — key 70, `p1.b3`, `ring=0`. The game's key loop then drained the 15
words in one iteration (S f=0x69E head = tail = 0x2A, `kb` = 0x0F00), but the victim press's
release was still injected at its original 4-frame hold, and a second burst (12 I records = 6
attempts at f=0x719) collapsed the same way; those collapsed hold windows are what
`unscripted_input` reports from f=0x71A. The scheduled input is otherwise complete: 512 I
records, exactly one not queued, and 0 W records. This is a capture-model failure (the
frame-keyed schedule's fire-on-due burst after a poller gap; spec §3.7's missed frames), not a
game fact; the game's verdict is §F2.6's bytes.

### §F2.6 The game's verdict: the mode-8/9 rows

`gp_win.py check` on the preserved capture (the first failing evidence row):

```text
$ python3 tools/gp_win.py check --scenario gp-u14-win-live \
    --capture data/k11-captures/gp-u14-win-live.failed
gp_win: gp-u14-win-live: evidence: character select (mode 0x10) at f=26A ok
gp_win: gp-u14-win-live: evidence: round 1: P1 is character 0 (cursor 0 confirmed, 0x43CAD) at f=45D ok
gp_win: gp-u14-win-live: evidence: FAIL: round 1 KO: 0x27C48 counts P1, 0x27BA4 leaves the match
  open, 0x27FA8 -> mode 8 (mode 0x8 entry 1)
exit 1
```

That milestone's predicate is P1's win (`s1_5a = 0x78, w2 = 1, w3 = 0, ad4 = 0xFFFFFFFF`); the
capture's mode-8 row is its mirror. The measured rows:

| row | frame | `s0_5a` | `s1_5a` | `w2` | `w3` | `ad4` | `b0c` |
|---|---|---|---|---|---|---|---|
| round 1 result (mode 0x08, entry 1) | 0x898 | 0x78 | 0x19 | 0 | 1 | 0xFFFFFFFF | 0 |
| match result (mode 0x09) | 0xF9F | 0x78 | 0x5C | 0 | 2 | 1 | 0 |

`b0c` is 0 in all 9,471 S records. P1 (`c0 = 0`, SAURON) is the KO'd side in both rounds; P2
(`c1 = 3`, VERTIGO; `afc = 3`) wins the match 2-0. The character is a surprise relative to the
plan's expectation (the same path's gp-u9-win and gp-u10-ending have `c1 = 6`, CHAOS, `afc = 5`);
this capture's own S fields are the measurement. Round 1's mode-6 entry is f=0x45D and round 2's
f=0xAAF; no press was queued between mode 8 (0x898) and the round-2 anchor, and none after mode 9
(0xF9F) — the skip/resume. After mode 9 the game ran 0x17, 0x15, 0x13 and 0x1E and returned to
mode 3 at f=0x19A8, so mode 0x12 never opened.

### §F2.7 The failure branch

The run produced a capture, so it is the phase's one authorized attempt; its harness checks failed
and the game did not win. Per the Task 2 brief: all nine `GP_U14_*` pins stay empty (the
`GP_U14_MAX_MISSING` line exists but is blank), `GATE_NAMED_GAPS` keeps
`$(K11_CAPTURES)/gp-u14-win-live`, `make gate-check` reads 34/35 present with the gap declared
(exit 0), and no second capture was made. `make gp-u14-oracle` therefore skips; it was not run
(Task 6 owns the phase gate).

The committed changes are the scenario/builder move, `TestLiveWin`, the smoke scenario and test,
the Makefile provenance comment plus the `--max-missing` coverage pin, and this section; the nine
`GP_U14_*` definitions stay empty. `tools/gp_session.py`'s U14 comment now records the measured
attempt-88 KO from §E2 and the skip's reason; the Makefile comment records this failure.

## §F3 The hand-dealt KO predicate (Task 3)

`tools/gp_twop.py` gains `ko(lines, winner=0)` and `check --ko [--winner N]`, a sibling of
`gp_win.live_death` for the fought win (plan 2026-10-26-fought-match-truth Task 3; Task 4's
two-human capture is what it will judge). It returns `dict(ko=, round=, result=, death=,
fail=[])`: the frames of the KO, the round result and the match result, the first death-done
record, and every reason the log is not a hand-dealt KO of side `winner`.

### §F3.1 What the predicate requires

Every condition is required, each with its own `fail` entry:

1. **The WIN fields.** Every S record carries all 13 `gp_session.WIN_EXTRA` names, else
   `no WIN fields: ...`: without `ad4`/`w2`/`w3`/`b0c` and the `+0x5A` damage bytes no KO
   and no result can be shown.
2. **No poke wrote the KO path.** No W record's bytes (`[addr, addr + len)`) cover a
   `KO_POKES` address: `0x10780A`/`0x10789E` (`s0_5a`/`s1_5a`), `0x104AF2`/`0x104AF3`
   (`w2`/`w3`), `0x104AD4` (`ad4`), `0x104B0C` (`b0c`), and `0x108106`/`0x10810A`/`0x10810C`
   (`m106`/`m10a`; `gp_session.lands_marked` writes seven bytes at `0x108106`).
3. **A two-human match.** `two_human(lines)`'s `fail` is empty.
4. **The KO value.** The first S record whose *loser* `+0x5A` reads `0x78` (`0x27FA8`'s
   `cmp 0x78`, `gp_session.KO_P2`'s comment; record 2026-10-02-gameplay-u9-u10 §W.3) is the
   KO. It must be in the fight (mode 6) or in a result mode (7/8/9) that follows a mode-6
   entry (§F3.2).
5. **A live value before it.** The mode-6 entry the KO ends has a strictly earlier record with
   `0 < s*_5a < 0x78`: the damage path adds to a live value, so a `0 → 0x78` jump fails even
   without a W record.
6. **The winner was not the KO'd side.** The winner's `s*_5a < 0x78` at the KO record.
7. **No time-out.** The round's result is not mode 7: the *time-up* result names no KO
   (§W.3: "round 2 timeout → mode 7 → 9"; record 2026-09-30-gameplay-ground-truth §G.23 lists
   mode 7 on U4's round path).
8. **The result names the winner.** The first result record after the KO is the round result
   (mode 8) with `ad4 = NO_RESULT = 0xFFFFFFFF` and `w[winner] >= 1, w[loser] = 0`, or the
   match result (mode 9) with `ad4 = winner` and the same `w` (`0x27C48`'s counters
   `DS_00104AF2`/`AF3` and `0x27BA4`'s `DS_00104AD4`; §W.3). The draw `ad4 = 2` fails.
9. **The match result, when reached.** A mode-9 entry at or after the KO has `ad4 = winner`
   and `w[winner] >= 1, w[loser] = 0` (scoped at/after the KO: an earlier match's result is
   not this match's).
10. **Reported, not required.** `death` = the first S at/after the KO with `b0c = 1`
    (`0x37EA0` → `0x37FF6`, record §W.5): a hand-dealt KO leaves the death animation to run,
    so an early capture legitimately has it still 0.

`no result after the KO` and `the record after the KO ... is mode X` are the remaining
rejections: the damage byte alone is not the round's result, and only modes 8/9 name a winner.

### §F3.2 Plan-vs-raw correction: where the KO value is observed

The brief says "find the first mode-6 entry whose S records include the loser's
`s*_5a == 0x78`". **The raw contradicts that**: no mode-6 S record in any of the three
captures that carry a KO reads `0x78`. Measured:

| capture | last sampled mode-6 record | first record with the value at `0x78` |
|---|---|---|
| `gp-u14-win-live.failed` (fought, side 0 KO'd) | `f=0x894` `s0_5a=0x71` | `f=0x898`, **mode 8**, `0x78` (`w3=1`: side 1's round) |
| `gp-u9-win` (poked KO of side 1) | `f=0x489` `s1_5a=0` | `f=0x48D`, **mode 8**, `0x78` (`w2=1`) |
| `gp-u23-u16-finisher` (labelled `0x77` poke + one real hit) | `f=0x1482` `s1_5a=0x77` | `f=0x1486`, **mode 9**, `0x78` |

The cause is the raw's own granularity, stated in §W.3's KO-poke note: "`DS_0010789E = 0x78`
written in the spin before an iteration of mode 6 makes that iteration's `0x27FA8` end the
round". The KO and the mode change share the iteration, and an S record is the
end-of-iteration snapshot (`gp_session`'s poll.log v2 header), so a hand-dealt KO's `0x78`
*is* the mode-8/9 record. A predicate that required a mode-6 record would reject every
hand-dealt KO measured here, the fought one included — and Task 4's capture is the same class
— so the **raw wins**: the predicate searches every record for the first `0x78`, and takes the
round's mode-6 entry as the run containing that record when it is mode 6 or the run
immediately before it when it is a result record. Conditions 5-10 are unchanged.

This is not a weakening. The live-value rule still rejects a poke-shaped jump on its own
(`test_a_zero_to_ko_jump_on_the_result_record_fails`, the `gp-u9-win` shape with its W record
removed), and `KO_POKES` catches the shape the live-value rule cannot: `gp-u23`'s `0x77 → 0x78`
reads as a live value, and only its W record at `f=0x13B8` (`0x10789E`) marks it poked.

Real-capture acceptance: `gp-u14-win-live.failed` under `--winner 1` reports
`ko f=898; round result f=898; match result f=F9F` — §F2.6's measured rows — and fails only
the two-human claim (`b1f` stays 1: P1 against the CPU). The KO arm reads a fought KO; the
capture is not a two-human match, which is why it is not a pass.

### §F3.3 RED then GREEN

RED, before `ko` existed (`python3 -m unittest tools.tests.test_gp_twop -k ko -v`):
7 tests, `FAILED (failures=1, errors=6)` — `AttributeError: module 'gp_twop' has no attribute
'ko'` on the four predicate cases `-k ko` matched, and `SystemExit` from `unrecognized
arguments: --ko` on two of its three CLI cases; the third (the exclusive group) fails with
`'not allowed with argument' not found` instead. The whole class (`TestKO`, 9 tests at that
point) ran `FAILED (failures=1, errors=8)`.

GREEN (`python3 -m unittest tools.tests.test_gp_twop -v`): `Ran 46 tests ... OK` — the module's
30 before the task plus `TestKO`'s 16 (the brief's 6 cases; the raw result-record shape; the
poke-shaped jump with no W record; the WIN-fields gate; the two-human wiring; the round-result
and match-result `w` rules; the no-result branch; and the three CLI cases: a pass, the winner
side reaching the predicate, and the exclusive group).

The wider tool set (Task 2's nine suites: `test_gp_session test_gp_capture test_gp_compare
test_gp_moves test_gp_keys test_gp_u17 test_gp_twop test_gp_modes test_gp_win`) ran **366**
tests, `OK` (350 at Task 2's close; +16, all in `test_gp_twop`).

### §F3.4 The four existing captures

Each exits 1. The first failure line is the brief's expected reason; the three two-human-family
captures' scenarios pass no `extra=` (so they log `SNAP_FIELDS` but not `WIN_EXTRA`), and
`gp-u9-win` has the fields and is poked:

```text
$ python3 tools/gp_twop.py check --ko --capture data/k11-captures/gp-u16-twop2
gp_twop: gp-u16-twop2: hand-dealt KO of side 0: ko f=-; round result f=-; match result f=-; death-done f=-
gp_twop: gp-u16-twop2: FAIL: no WIN fields: afc, ad4, w2, w3, b1e, b21, b14, b0c, t104, m106, m10a, sc0, c82 absent from some S records (the capture must log gp_session.WIN_EXTRA; without the damage/result bytes a KO cannot be shown)
gp_twop: gp-u16-twop2: hand-dealt KO: FAIL
exit=1

$ python3 tools/gp_twop.py check --ko --capture data/k11-captures/gp-twop
gp_twop: gp-twop: FAIL: no WIN fields: afc, ad4, w2, w3, b1e, b21, b14, b0c, t104, m106, m10a, sc0, c82 absent from some S records (the capture must log gp_session.WIN_EXTRA; without the damage/result bytes a KO cannot be shown)
exit=1

$ python3 tools/gp_twop.py check --ko --capture data/k11-captures/gp-u23-u16-finisher
gp_twop: gp-u23-u16-finisher: FAIL: no WIN fields: afc, ad4, w2, w3, b1e, b21, b14, b0c, t104, m106, m10a, sc0, c82 absent from some S records (the capture must log gp_session.WIN_EXTRA; without the damage/result bytes a KO cannot be shown)
exit=1

$ python3 tools/gp_twop.py check --ko --capture data/k11-captures/gp-u9-win
gp_twop: gp-u9-win: hand-dealt KO of side 0: ko f=48D; round result f=48D; match result f=85A; death-done f=-
gp_twop: gp-u9-win: FAIL: poke: the W record at f=489 wrote 0010789E, a KO/death field: the round must be the game's own write, not a poked one
gp_twop: gp-u9-win: FAIL: b1f never reaches 3 (no S record has both sides human)
gp_twop: gp-u9-win: FAIL: side 1 s1_5a jumps to 78 at f=48D with no earlier 0 < value < 78 in the mode-6 entry the KO ends: the damage path adds to a live value
gp_twop: gp-u9-win: hand-dealt KO: FAIL
exit=1
```

(`gp-u9-win`'s record shows the anchors found *and* three independent rejections: the poke, the
one-human log (it is LEFT PLAYER ARCADE against the CPU) and the poke-shaped `0 → 0x78` jump.
`gp-twop`'s other lines are omitted here for length; they are the same as `gp-u16-twop2`'s.)

The existing checks are unchanged: `check --capture gp-twop`, `check --join --capture
gp-u15-join`, `check --finisher --allow-missing-reaction --capture gp-u16-twop2` and `check
--finisher --capture gp-u23-u16-finisher` all still exit 0, and `--winner` without `--ko` is an
argparse error (exit 2), as is `--winner 2`.

### §F3.5 Mutation proofs

Each mutation was applied to `tools/gp_twop.py`, `TestKO` run, and the file restored
(`cmp` clean). Every one fails at least one assertion:

| mutation | caught by |
|---|---|
| `KO_POKES = ()` | `test_a_poked_ko_fails` |
| the live-value rule (`0 < v < 0x78`) disabled | `test_a_jump_from_zero_to_ko_fails`, `test_a_zero_to_ko_jump_on_the_result_record_fails` |
| the no-KO time-out arm disabled | `test_a_time_out_fails` |
| the winner-not-KO'd rule disabled | `test_the_winner_must_not_be_ko_d` |
| the result `ad4` rule disabled | `test_a_draw_or_wrong_side_result_fails` |
| the result-record KO (mode 8/9) rejected (the brief's literal rule) | `test_a_ko_seen_on_the_round_result_passes`, `test_a_zero_to_ko_jump_on_the_result_record_fails` |
| `death`'s `b0c == 1` → `== 2` | `test_a_hand_dealt_ko_passes_and_reports_death_done` |
| the WIN-fields gate disabled | `test_a_capture_without_the_win_fields_fails` |
| the `two_human` failures dropped | `test_a_one_human_log_fails_on_the_two_human_claim` |
| `--winner` ignored (`w = 0`) | `test_cli_ko_passes_the_winner_side` |
| the round result's `w` rule disabled | `test_the_round_result_must_name_the_winner` |
| the match result's `w` rule disabled | `test_the_match_result_must_name_the_winner` |
| the no-result arm disabled | `test_a_ko_with_no_result_after_it_fails` |

The exclusive-group test also failed before the flag existed (`--ko` was an unrecognized
argument), and the `--ko --join`/`--ko --finisher` pairs exit 2.

### §F3.6 Named gaps

1. **The predicate does not claim which hits landed.** It proves the loser's `+0x5A` reached
   `0x78` from a live value in a round with no poke and no time-out, and that the result names
   the side; the number and identity of the hits are not in the WIN fields.
2. **`KO_POKES` is nine field addresses, matched by byte-range overlap.** A one-byte poke at an
   unlisted interior land-mark byte (`0x108107..0x10810B`) is not caught. Every poked win in the
   tree writes `0x10789E` (len 1), `0x104B0C` (len 1) or `0x108106` (len 7, `lands_marked`), all
   caught; no measured poke writes the interior bytes alone.
3. **The `w`/`ad4` claim is judged on the result record, not the KO's mode-6 record.** The round
   counters are written by the iteration that deals the KO, and `ad4` is only decided at the
   result, so the result record is where the raw states the claim.
4. **Two rejection branches are hand-exercised, not unit-pinned:** a first `0x78` read in a
   non-fight, non-result mode, and a result record with no mode-6 run before it. They exist for
   capture shapes the tree has not measured (a poller gap that skips a result window); no
   capture in `data/` produces them, and Task 4's script runs to mode 0x12 through both results.

## §F4 The two-human KO capture and oracle (Task 4)

Outcome: the scenario and its test are committed; the phase's one authorized capture ran on
2026-10-26 and **the game's verdict was the hand-dealt two-human KO** — `gp_twop.py check --ko`
passed on it (`ko f=B9A`, `round result f=B9A`, no match result inside the 240-frame window,
`death-done f=-`: `b0c` stayed 0, reported not required) — **but the capture failed one harness
check and was not published.** The poller's `port script v2` check could not pin the consumption
frame of two BIOS words (key 86, `p1.b0`, `f=7C7`), so the poll log cannot produce a port script,
and without a script there is no replay, dump, compare, pins or oracle. The capture is preserved
as `data/k11-captures/gp-twop-ko.failed`; `data/k11-captures/gp-twop-ko` does not exist, the
target is not in `CAPTURE_DIRS`, all eight `GP_TWOP_KO_*` pins stay unwritten, no miss set and no
`gp-twop-ko-oracle` exist, and `make gate-check` still reads `34/35 present, 1 missing` with
`gp-u14-win-live` the declared gap. No second run was made under the task's single-use rule; the
controller then authorized one recovery run on a CPU-confined scope, and it failed the same harness
check on a different frame (§F4.7). No third run was made.

The cause is host starvation of the poller, not the game and not the scenario: 127 S frames were
missed over the run, the largest gap `f=0x7C4 → f=0x804` (S `ms=55605 → ms=56670`, 1.07 s) under a
host load average of 26 on 20 cores (twenty CPU-bound processes from another workload). The two
words step 49 queued at `f=0x7C4` were consumed inside that gap; the H records name `f=0x7C7`,
which is the poller's detection frame, so the true consumption frame is bounded only to
`0x7C5..0x7C7` and the harness refuses to guess. §F4.5 has the rows and why relaxing the pin was
rejected.

### §F4.1 The scenario change

```python
SCENARIOS['gp-twop-ko'] = dict(
    time_limit=240, extra=WIN_EXTRA, stop_mode=(0x08, 0x09), stop_delay=240,
    steps=steps_before_end(SCENARIOS['gp-twop']['steps'])[:9]
    + _u16_round(140, 30),
)
```

`gp-twop-ko` joins `STOP_AT_END`. The kept prefix is gp-twop's own steps through the walk-in (the
boot, the two menu Enters, P2's join in the character select, both cursors, both confirms and the
mode-6 walk-in); `[:9]` is computed from `steps_before_end(SCENARIOS['gp-twop']['steps'])` so a
future change to gp-twop before its end shows up as a test failure, not a silent divergence. The
walk-in step (`('after_mode', 0x06, 60, ('pad', ('p1.right', 'p2.left'), 30))`) is load-bearing:
its `p2.left` is the second side's mode-6 pad press, which `gp_twop.two_human`'s fight-press rule
needs (P1's 140 attempts alone would be a one-human fight). The round is one `_u16_round(140, 30)`
(140 1A/1B attempts, a 12-frame `p1.right` walk every 8 attempts, 17 walks, the builder's 16-frame
attempt gap), about 2,600 nominal frames against the measured ~88-attempt KO (record
2026-10-23-endgame-truth §E2), so the round timer (~3,280 frames) is only the backstop.
`stop_mode=(0x08, 0x09)` with `stop_delay=240` records the KO's result window: the round result
(0x27FA8 → mode 8) or the match result (mode 9), then 240 frames. That window value is sized from
U10's observed death stream (record 2026-10-02-gameplay-u9-u10 §W.5), **not** a two-human
measurement. No pokes: `WIN_EXTRA` is logged so `gp_twop.ko` can read the damage bytes, `ad4`,
the round counters and `b0c`; the round's own writes are the claim.

### §F4.2 RED then GREEN

Before the scenario existed:

```text
$ python3 -m unittest tools.tests.test_gp_session -k two_human_ko -v
ERROR: test_the_two_human_ko_scenario_is_one_round_with_the_walk_in_press
  KeyError: 'gp-twop-ko'
Ran 1 test ... FAILED (errors=1)
```

After it (and after `gp-twop-ko` joined `STOP_AT_END`):

```text
$ python3 -m unittest tools.tests.test_gp_session -k two_human_ko -v
Ran 1 test ... OK
$ python3 -m unittest tools.tests.test_gp_session
Ran 73 tests ... OK                    (72 before; +1)
$ python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_capture \
    tools.tests.test_gp_compare tools.tests.test_gp_twop
Ran 244 tests ... OK
```

The class is `TestTwopKo`. The brief's test is the minimum; the spacing is asserted as well (Task
2's review Minor): every attempt is 16 frames after the previous step (30 for the first), every
`p1.right` walk is 16 frames on, holds 12 and sits after exactly attempt 8, 16, ..., 136 (never
before the first or after the last), and the attempt after a walk is 20 frames on. The literals
are the builder's own values and one assertion pins `(U16_GAP, U16_WALK, U16_GROUP) == (16, 12,
8)`, so a constant mutation fails the shape test rather than travelling with it. Each mutation was
applied to `tools/gp_session.py`, the test run, and the file restored (`git diff` clean; the
`__pycache__` removed between runs — the first attempt at this proof silently reused a stale
bytecode file because the mutations and the restore kept the same size and mtime second):

| mutation | caught by |
|---|---|
| `stop_delay=240` → `241` | the scenario tuple assertion |
| `U16_GAP` 16 → 15 | the constant assertion |
| `U16_WALK` 12 → 11 | the constant assertion |
| `U16_GROUP` 8 → 7 | the walk count (19 != 17) and the constant assertion |
| `_u16_round(140, 30)` → `(140, 29)` | `block[0]` (the first gap) |
| `_u16_round(140, 30)` → `(139, 30)` | the attempt count (139 != 140) |
| the prefix `[:9]` → `[:8]` | the prefix equality and the first-block step |
| an extra non-attempt step appended | the walk/attempt partition |

### §F4.3 The authorized capture's harness checks

```text
$ DISPLAY=:1 timeout 600 make gp-capture scenario=gp-twop-ko
gp_capture: snapshots 3135, f 5..CC2, 127 frames missed (spec §3.7)
gp_capture: CHECK base: ok
gp_capture: CHECK steps fired 110/166 (0 skipped): ok
gp_capture: CHECK end frame reached: ok
gp_capture: CHECK mode 0x27 after the Enter: ok
gp_capture: CHECK snapshots kb == raw (0 differ): ok
gp_capture: CHECK frames written 2608/2608: ok
gp_capture: CHECK port script v2 (key 86 (p1.b0) consumption at f=7C7 unpinned): FAIL
gp_capture: CHECK no unscripted input: ok
gp_capture: CHECK pokes written 0/0, 0 raced: ok
gp_capture: CHECK stopped at the end (SIGTERM at f=0CC3, rc=0): ok
gp_capture: wrote 2608 frames to .../data/k11-captures/gp-twop-ko.failed (raw 1424..5316),
  wall 77.9s; a CHECK failed, .../data/k11-captures/gp-twop-ko left as it was
```

Nine of ten checks pass: the stop-on-mode path fires (the X record at `f=0x0C87`, step 110, 240
frames after the round result's mode-8 P record at `f=0xB97`), the run ends by SIGTERM at
`f=0x0CC3` (`reason=end rc=0`), no input was unscripted and no poke was written. The one failure
is the script pin; §F4.5 measures it.

### §F4.4 The game's verdict: a hand-dealt two-human KO

```text
$ python3 tools/gp_twop.py check --ko --capture data/k11-captures/gp-twop-ko.failed
gp_twop: gp-twop-ko.failed: hand-dealt KO of side 0: ko f=B9A; round result f=B9A; match result f=-; death-done f=-
gp_twop: gp-twop-ko.failed: hand-dealt KO: ok
exit=0

$ python3 tools/gp_twop.py check --capture data/k11-captures/gp-twop-ko.failed
gp_twop: gp-twop-ko.failed: join f=285 (cred 4 -> 4); 2448 S records from the join to X f=C87;
  side 0 [('pad', 2448)], side 1 [('pad', 2448)]; fight presses 489/30
gp_twop: gp-twop-ko.failed: two-human match: ok
exit=0
```

Both sides are human from the character select's join at `f=0x285` (`b1f` 1 → 3, no credit
debit), every S record's command word for both sides is a pad word, and both sides pressed in
mode 6 (P1's scripted attempts; P2's 30-frame walk-in `left`). P1 is TALON (`c0 = 2`, cursor 0
confirmed) and P2 is CHAOS (`c1 = 6`, `afc = 4`; the P2 cursor's `left` 5 → 4). The damage
progression in the last mode-6 entry (`f=0x4D1`, the first fight frame) is a live path:

| row | frame | `s0_5a` | `s1_5a` | `r0`/`r1` |
|---|---|---|---|---|
| round entry | `0x4D1` | 0 | 0 | `FF`/`FF` |
| | `0x53B` | 0 | `0x12` | `12`/`FF` |
| | `0x587` | 0 | `0x22` | `11`/`FF` |
| | `0x681` | 0 | `0x29` | `00`/`FF` |
| | `0x6C7` | 0 | `0x30` | `00`/`FF` |
| | `0x7A7` | 0 | `0x37` | `00`/`FF` |
| | `0x7C4` | 0 | `0x40` | `11`/`FF` |
| | `0x8CF` | 0 | `0x47` | `00`/`FF` |
| | `0x8FF` | 0 | `0x57` | `11`/`FF` |
| | `0x9F9` | 0 | `0x5E` | `00`/`FF` |
| | `0xA8B` | 0 | `0x65` | `00`/`FF` |
| | `0xABB` | 0 | `0x75` | `11`/`FF` |
| | `0xB96` | 0 | `0x75` | `10`/`FF` |
| **round result (mode 8)** | **`0xB9A`** | 0 | **`0x78`** | `10`/`FF` |

The result rows:

| row | frame | `s0_5a` | `s1_5a` | `w2` | `w3` | `ad4` | `b0c` |
|---|---|---|---|---|---|---|---|
| round result (mode 8) | `0xB9A` | 0 | `0x78` | 1 | 0 | `0xFFFFFFFF` | 0 |
| result window's last S (mode 0x16) | `0xCC2` | 0 | `0x78` | 1 | 0 | `0xFFFFFFFF` | 0 |

`w2 = 1, w3 = 0` is 0x27C48's round counter naming P1, and `ad4 = 0xFFFFFFFF` is 0x27BA4's
undecided match (the round result, not the match result: the KO ended round 1). The mode path:
`0x224` mode 0x1A (`b1f = 1`) → `0x248` mode 0x10 (the character select) → `0x285` join (`b1f =
3`, `e2` first shows P2's start mask) → `0x31C`/`0x330`/`0x341` → `0x432`/`0x444` → `0x456` mode 5
(the entrances) → **`0x4D1` mode 6** (the fight) → **`0xB9A` mode 8** (the round result) → `0xC7C`
mode 0x16. `b0c` is 0 in all 3135 S records: the 240-frame window ends before the death animation
does, which the predicate reports and does not require (§F3.1 item 10).

The KO landed on the round's **90th attempt** (`k = 89`, step index 109; the press fired at
`f=0xB96`) and the round result appeared four frames later, close to the measured attempt-88 KO.
90 attempts of the scripted 140 fired before the stop, so the attempt count has ~1.5× headroom
for the RNG's 85-88 range.

### §F4.5 The failure branch: no port script, no publication

The poll log cannot produce the port script:

```text
$ python3 tools/gp_session.py port-script --scenario gp-twop-ko \
    --capture data/k11-captures/gp-twop-ko.failed --out /tmp/gp-twop-ko.script
gp_session: port-script: gp-twop-ko: key 86 (p1.b0) consumption at f=7C7 unpinned
exit=1
```

The pin the script needs is `S(f-1)` for every consumed BIOS word (spec §4.1's "keys at their
consumption frame"), and the poller was starved across that frame. The rows:

```text
S ms=55605 f=07C4 ... head=002A tail=002A
I ms=55605 f=07C4 step=49 press=p1.b0 ... bios=1675 ring=1 late=1
I ms=55605 f=07C4 step=49 press=p1.b1 ... bios=1769 ring=1 late=1
H ms=55650 f=07C7 head=002C
H ms=55650 f=07C7 head=002E
I ms=55671 f=07C8 step=49 release=p1.b0 ...
S ms=56670 f=0804 ... head=002E tail=002E
```

`S(0x7C6)` does not exist: the poller's S records jump `0x7C4 → 0x804`, 64 frames and 1.07 s of
wall time, and the H records' `f` is the frame the poller *detected* the head change (45 ms after
the press), not a logged iteration. The two words were queued after `S(0x7C4)` and consumed before
`S(0x804)`, so the consumption frame is bounded only to `0x7C5..0x7C7`; the replay would inject
at `0x7C7` and could be up to two frames late. The rule refuses that, and relaxing it for this
capture would be fitting the model: the harness's input truth is exactly that a consumed word's
frame is pinned by a neighbouring snapshot. The run's other gaps (26 gaps, 127 missed frames)
matter less because a missing capture frame is simply not compared (`gp_compare`'s missing-frame
rule); only the key pin is load-bearing.

The host was heavily loaded throughout (load average 26.39 on 20 cores; twenty CPU-bound
`difftest.py` processes from another project at ~97 % each), and the poller is a Python thread
racing a 60 Hz guest: the starvation is an OS/host condition, not a game fact. It arrived *after*
the game started, so it is the task's one attempt, not a preflight failure: no second run was
made, per the brief.

Consequences, all deliberate:

- `data/k11-captures/gp-twop-ko` does not exist; the 2608-frame, 131 MB capture stays at
  `data/k11-captures/gp-twop-ko.failed` (`poll.log` lines 3758, S records 3135,
  sha256 `67aa1c403abdb1f45a33ef4864d0a4e79e1aa6c54ea210bd5429902945e9f6a9`).
- `gp-twop-ko` is not in `CAPTURE_DIRS`; the gate count stays 35 and `make gate-check` reads
  `captures 34/35 present, 1 missing`, the declared `gp-u14-win-live` gap.
- No `GP_TWOP_KO_*` block, no `gp-twop-ko-oracle`, no `verify` line, and no `k_miss_gp_twop_ko`
  entry: a miss set without a replay would pin nothing, and a row in `k_gp_sets` for a scenario
  no driver can run is dead weight.
- The scenario and `TestTwopKo` stay (the brief: the scenario and predicate code stay; the claim
  is not made). The claim that is *not* made is the ratcheted oracle; the game's KO itself is
  measured above, on a capture that cannot be replayed.

### §F4.6 What a second authorization would need

Nothing in the scenario's numbers is implicated: the KO came on attempt 90 of 140, the result
window held mode 8 from `f=0xB97` to `f=0xC79` (227 of its 240 frames; mode 0x16 entered at
`f=0xC7A`, the X at `f=0xC87`), and the stop, skip-free step count, unscripted-input and poke
checks all passed. A re-run would need a host with the poller uncontended (the missed-frame
count of the four published captures measured here is 8-17: gp-twop 8, gp-u16-twop2 15,
gp-keys-fight 9, gp-u23-u16-finisher 17; this run's 127 with a 64-frame gap is the outlier), the same
scenario, and a second authorized attempt; the measured values here are the expectations for it.
Until then the phase's two-human KO claim is a measured capture-side result, not an oracle.

**The second authorization was spent on 2026-10-26 and failed the same harness check (§F4.7), so
this section is now closed: no third run is authorized.**





### §F4.7 The recovery capture (2026-10-26, CPU-confined)

The controller authorized one recovery run after §F4.5's poller starvation. It ran in a systemd user
scope pinned to CPU 19 (host load 28.45 on 20 cores at the start):

```text
$ systemd-run --user --scope -p AllowedCPUs=19 --quiet bash -lc 'cd <worktree> && DISPLAY=:1 \
    timeout 600 make gp-capture scenario=gp-twop-ko'
gp_capture: snapshots 3087, f 5..CB9, 166 frames missed (spec §3.7)
gp_capture: CHECK base: ok
gp_capture: CHECK steps fired 107/166 (0 skipped): ok
gp_capture: CHECK end frame reached: ok
gp_capture: CHECK mode 0x27 after the Enter: ok
gp_capture: CHECK snapshots kb == raw (0 differ): ok
gp_capture: CHECK frames written 2565/2565: ok
gp_capture: CHECK port script v2 (key 16 (p1.b2) consumption at f=596 unpinned): FAIL
gp_capture: CHECK no unscripted input: ok
gp_capture: CHECK pokes written 0/0, 0 raced: ok
gp_capture: CHECK stopped at the end (SIGTERM at f=0CB9, rc=0): ok
gp_capture: wrote 2565 frames to .../data/k11-captures/gp-twop-ko.failed (raw 1373..5251),
  wall 77.0s; a CHECK failed, .../data/k11-captures/gp-twop-ko left as it was
make: *** [Makefile:650: gp-capture] Error 1
```

The game's verdict was again the KO — a complete two-human hand-dealt KO — and again the publication
was refused by the script pin:

```text
$ python3 tools/gp_twop.py check --ko --capture data/k11-captures/gp-twop-ko.failed
gp_twop: gp-twop-ko.failed: hand-dealt KO of side 0: ko f=B91; round result f=B91; match result f=-; death-done f=-
gp_twop: gp-twop-ko.failed: hand-dealt KO: ok
exit=0

$ python3 tools/gp_twop.py check --capture data/k11-captures/gp-twop-ko.failed
gp_twop: gp-twop-ko.failed: join f=2AB (cred 4 -> 4); 2378 S records from the join to X f=C7D;
  side 0 [('pad', 2378)], side 1 [('pad', 2378)]; fight presses 467/30
gp_twop: gp-twop-ko.failed: two-human match: ok
exit=0
```

| row | frame | `s0_5a` | `s1_5a` | `w2` | `w3` | `ad4` | `b0c` |
|---|---|---|---|---|---|---|---|
| round result (mode 8) | `0xB91` | 0 | `0x78` | 1 | 0 | `0xFFFFFFFF` | 0 |

`s1_5a`'s live path in the last mode-6 entry (`f=0x4F7`): `0x12` (`0x561`), `0x22` (`0x5D0`),
`0x29` (`0x6A7`), `0x30` (`0x6ED`), `0x37` (`0x7CD`), `0x40` (`0x801`), `0x47` (`0x8F7`), `0x57`
(`0x925`), `0x5E` (`0xA1F`), `0x6E` (`0xA4F`), `0x75` (`0xAF9`), then `0x78` on the mode-8 record;
`s0_5a = 0` and `b0c = 0` in every S record. The stop: mode-8 P record `f=0xB8D`, X `f=0xC7D`
(exactly +240), mode 0x16 P record `f=0xC6A`. The KO came on the round's **88th attempt**
(`k = 87`, X step 107) — the measured attempt-88 shape again, from a different RNG draw.

The pin's rows:

```text
S ms=45498 f=0593 ... head=001E tail=001E
I ms=45498 f=0593 step=12 press=p1.b2 ... bios=316E ring=1 late=1
I ms=45498 f=0593 step=12 press=p1.b3 ... bios=326D ring=1 late=1
I ms=45498 f=0593 step=13 press=p1.b0 ... bios=1675 ring=1 late=1
I ms=45498 f=0593 step=13 press=p1.b1 ... bios=1769 ring=1 late=1
H ms=45536 f=0596 head=0020
H ms=45536 f=0596 head=0022
H ms=45536 f=0596 head=0024
H ms=45536 f=0596 head=0026
S ms=45551 f=0596 ... head=0026 tail=0026
```

```text
$ python3 tools/gp_session.py port-script --scenario gp-twop-ko \
    --capture data/k11-captures/gp-twop-ko.failed --out /tmp/gp-twop-ko-recovery.script
gp_session: port-script: gp-twop-ko: key 16 (p1.b2) consumption at f=596 unpinned
exit=1
```

`S(0x595)` does not exist: the S records jump `0x593 → 0x596` (frames `0x594` and `0x595` missing),
so `prev = snap.get(c - 1)` is `None`. Two attempts (steps 12 and 13) had burst-fired together at
`f=0x593` after a 23-frame poller gap (`0x57B → 0x593`), queuing four BIOS words in one pass; the
game consumed all four at `f=0x596`. This is the same mechanism as §F4.5 one step earlier in the
log, and it shows the pin rule needs exactly one thing the starved poller cannot guarantee: an S
record at `c - 1`.

The CPU pinning made the poller worse, not better: the missed-frame count rose 127 → 166 (two
23-frame gaps instead of one 64-frame gap), because the scope put DOSBox-X and the poller thread on
the same core while the host's other ~20 CPU-bound processes stayed unconstrained across all 20
cores. Every other check passed again (stop path, end frame, kb/raw, frames written, no unscripted
input, no pokes).

The recovery's artifact, now at `data/k11-captures/gp-twop-ko.failed`: 2565 frames, 128 MB,
`poll.log` 3695 lines / 3087 S records,
sha256 `1513b481a0d877d26ad3aa1d9070696a94f4812c7d81f49a87ed461f8f8b8ae1`; `session.txt` reads
`wall_s=77.0`, `fps=70.0866`, `cmos=zero pad_bios=1`, `stop_at_end=1 tail=60 signal_f=0CB9`.

**The first capture's artifact did not survive the recovery.** `gp_capture.publish` renames an
existing `<out>.failed` to `<out>.failed.old` and deletes it after moving the new failed run in, so
the run §F4.5 measured (sha256 `67aa1c403abdb1f45a33ef4864d0a4e79e1aa6c54ea210bd5429902945e9f6a9`)
was replaced; its `poll.log` and frames are gone. Its measured rows, check lines and sha256 remain in
§F4.3-§F4.5, and nothing consumed the artifact (it was in no gate, oracle, miss set or audio trace),
so no claim depended on it. A future recovery must copy a failed artifact out first: the tool has no
separate failed-name option.

Consequences, unchanged and final: `data/k11-captures/gp-twop-ko` does not exist; `gp-twop-ko` is not
in `CAPTURE_DIRS`; all eight `GP_TWOP_KO_*` pins stay unwritten; there is no `gp-twop-ko-oracle`, no
`k_miss_gp_twop_ko` and no `verify` line; `make gate-check` still reads `34/35 present, 1 missing`
with `gp-u14-win-live` the declared gap. The two-human KO remains a **game-evidence-only named
gap** (measured on two captures, neither replayable), and no third run was made.
