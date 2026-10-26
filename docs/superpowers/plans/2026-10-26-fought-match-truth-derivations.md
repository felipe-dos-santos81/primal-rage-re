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
