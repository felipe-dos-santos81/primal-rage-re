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

