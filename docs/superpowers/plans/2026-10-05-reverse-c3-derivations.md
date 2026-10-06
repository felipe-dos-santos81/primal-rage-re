# Reverse completion C3: the sixteen frontier callee rows (record)

**Scope.** Track P's fourth verification-only batch (roadmap row C3 of record
`2026-10-05-reverse-c2b-derivations.md` §C2b.7, the 53-address frontier): add the
differential-verification row for the sixteen addresses this session measured —
`0x249B0 0x249D0 0x164E8 0x1D238 0x1D244 0x1CA14 0x2A620 0x3C59C 0x46460 0x41310 0x365C8 0x1A5AC
0x13DF0 0x1881C 0x36CE4 0x2AC80` — so their dependents close. No ported function, no
`fn_register`, no E2 move. Plan: `2026-10-05-reverse-c3-frontier-rows.md`. Recipe: E3 record §E3.10;
checklist: `.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**Status of the numbers.** Measured by the planner on 2026-10-06 on `reverse-c3` at the base `main`
`6da0128`, in this worktree: the baseline gates and a full prototype of the sixteen rows (every
gate run, then reverted). The image is `build/diffrun --exe data/game/C/PRAGE.EXE --image-out FILE`,
sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's, E3's, P1-P8's). Every address below is
capstone 5.0.7 over that image (fixups applied); `unicorn` 2.1.4 runs the original side. Ghidra was
not consulted.

---

## §C3.1 The sixteen rows, re-derived from the raw

The C2b record §C2b.7's frontier table was re-measured from the final table: 57 distinct unverified
callees, of which the four non-rows (`0x1B544` host-owned, `0x5D812`/`0x29D60`/`0x2EA64` bare
stubs/rets) leave **53 candidate addresses**. This batch measured sixteen of them; the membership
is exact as committed (no correction). The rows and the raw's scan:

| row | callee | insns | the raw's direct callees | row design |
|---|---|---|---|---|
| `list_insert_after` | `0x249B0` | 8 | none | straight-line; the effects.c copy carries it |
| `list_unlink` | `0x249D0` | 14 | none | straight-line |
| `fighter_164e8` | `0x164E8` | 5 | none | one dword store of 0 at `0xFD148 + side*4` |
| `snd_music_unpause` | `0x1D238` | 3 | none | one byte store |
| `snd_sample_unpause` | `0x1D244` | 3 | none | one byte store |
| `snd_music_request` | `0x1CA14` | 13 | none | two stores, the pause and sequence-handle gates |
| `mode1_cursor` | `0x2A620` | 27 | none | the pset/current-y alternatives, the threshold, the dead clamp |
| `fighter_pass_flag` | `0x3C59C` | 20 | none | test-and-set with the 5-bit shift mask |
| `fighter_input_read` | `0x46460` | 23 | none | the ring walk with the modulo-0x14 wrap |
| `fighter_41310` | `0x41310` | 18 | none | mode-3 gate, signed sum, the underflow clamp |
| `fighter_state_365c8` | `0x365C8` | 34 | none | six gates then the signed facing compare |
| `fighter_1a5ac` | `0x1A5AC` | 14 | `33950` (allow) | ctx[4]+0x28 bit 0x4000 clear |
| `effects_clear` | `0x13DF0` | 15 | `13420` (stub; its own row is C3b) | the active-list walk |
| `hit_vert_distance` | `0x1881C` | 7 | `186D0` x2 (stub; its own row exists) | two latches then the difference |
| `fighter_36ce4` | `0x36CE4` | 30 | `2BC30` (stub; its own row exists) | the bit, the two mode gates, the animation call |
| `actor_alloc` | `0x2AC80` | 43 | `249D0`/`249B0` (real), `249C0` (allow), `2EA30` x2 (allow) | pop the free head, head/tail insert |

`0x2EA30` (the interrupt-lock nesting counter) has **no port C function** — the port's
single-threaded loop treats it as inert (comments at `actors.c:1114`, `flow.c:197`,
`render.c:328`), so it cannot be rowed without porting it, which this verification-only batch must
not do. It stays a named allow, the same treatment as the host-owned `0x1B544`; the record §C3.7
carries it in the C3b list as "name it".

## §C3.2 The rows: cases, seams, fixtures and the corrections

Each row is a `Spec` in `tools/diff_verify.py` (`C3_SPECS`), a binding `b_*` and mutants `m_*` in
`port/tests/diff_runner.c` (`k_bindings`), and the exact-set extensions in
`tools/tests/test_diff_verify.py` (`C3_MASKS`, `C3_KINDS`, the case-set table, the clobber table,
the counter line). The measured rows:

| row | entry | cases | blocks | mutants | callees (each by its own check) |
|---|---|---|---|---|---|
| `list_insert_after` | 0x249B0 | 3 | 1/1 | 4/4 | - |
| `list_unlink` | 0x249D0 | 3 | 1/1 | 4/4 | - |
| `fighter_164e8` | 0x164E8 | 3 | 1/1 | 3/3 | - |
| `snd_music_unpause` | 0x1D238 | 3 | 1/1 | 3/3 | - |
| `snd_sample_unpause` | 0x1D244 | 3 | 1/1 | 2/2 | - |
| `snd_music_request` | 0x1CA14 | 4 | 4/4 | 5/5 | - |
| `mode1_cursor` | 0x2A620 | 7 | 7/8 (0x2A66D named dead) | 5/5 | - |
| `fighter_pass_flag` | 0x3C59C | 7 | 3/3 | 4/4 | - |
| `fighter_input_read` | 0x46460 | 6 | 5/5 | 4/4 | - |
| `fighter_41310` | 0x41310 | 7 | 6/6 | 5/5 | - |
| `fighter_state_365c8` | 0x365C8 | 13 | 11/11 | 4/4 | - |
| `fighter_1a5ac` | 0x1A5AC | 6 | 1/1 | 3/3 | `33950` allow VERIFIED |
| `effects_clear` | 0x13DF0 | 3 | 3/3 | 5/5 | `13420` stub unverified (C3b) |
| `hit_vert_distance` | 0x1881C | 4 | 1/1 | 4/4 | `186D0` stub VERIFIED |
| `fighter_36ce4` | 0x36CE4 | 4 | 4/4 | 6/6 | `2BC30` stub VERIFIED |
| `actor_alloc` | 0x2AC80 | 5 | 9/9 | 5/5 | `249B0`/`249D0` real VERIFIED; `249C0` allow unverified (C3b); `2EA30` allow unverified (named) |

**The one new seam** (`port/src`): `effect_teardown` (`0x13420`) loses `static`, gains
`PR_SEAM(0x13420u, rec)` as its first statement and an `effects.h` declaration, because the
`effects_clear` row stubs it (it has no row; its 133 bytes and 4 callees make it a C3b row). The
`E.Call(0x13420, ("eax",))` declares no clobbers, matching `E.callee_clobbers` over the image
(`()` — the raw loop relies on EDX surviving the call, and the byte-derived set agrees). No other
`port/src` change was needed: every other callee already carries its seam
(`0x186D0`, `0x249B0`, `0x249D0`, `0x2BC30`, `0x2EA30`'s allow needs none) or is allowed.

**Raw-over-port corrections: none.** All sixteen rows agreed on first measurement with the port as
committed, once the prototype's own mutant fixtures were made memory-safe (record §C3.6). The two
findings that constrain a row are recorded as limits, not corrections, in §C3.5.

**Fixtures.** The rows reuse the zero-BSS scratch records (`0x10A200`/`0x10A240`/`0x10A600`/
`0x10A680`/`0x10A700`), the seeded data words (`0x1077E0`, `0x107874`, `0x1082D2`, `0x107A4C`,
`0x104B00`, `0x1077A8`, `0x1077B0`, `0x105B3C`, `0x105BCC`, the four pause/song bytes at
`0x1028DA..DB`, `0x1028D4`, `0x1028C0`, `0x1028CC`, `0x1028D9`) and the existing `ANIM_BEGIN`
call declaration for `0x2BC30`. `actor_alloc`'s cases seed the lock byte `0xBCD60` to 0: the raw's
two `0x2EA30` calls bracket the list ops and their net write is zero only from zero (the memory at
a recorded call is compared).

## §C3.3 The mutants

66 mutants, all detected (`make diff-verify --self-check`: `664/664`); what alone catches each is
pinned by `test_each_c3_mutant_is_caught_by_what_it_breaks` (`C3_KINDS` plus the measured case-set
table; the exact dicts are the ones committed in `tools/tests/test_diff_verify.py`). The rows with
callees have mutants caught only by the call list or the memory at a call
(`effects_clear@walk/@one` by the missing calls, `effects_clear@set` by the lock byte written
before call #0, `hit_vert_distance@one/@order/@side` by the latch call list, `fighter_36ce4@mode/
@mode22/@rec/@stream/@side` by the animation call, `actor_alloc@flag/@tail` by the list calls).

The mutant kinds (measured; the record's copy of the test's `C3_KINDS`):

KINDS = {
    'actor_alloc@empty': ['call #0', 'call #1', 'eax'],
    'actor_alloc@flag': ['call #0', 'call #1'],
    'actor_alloc@ret': ['call #0', 'call #1', 'eax'],
    'actor_alloc@tail': ['call #0', 'call #1'],
    'actor_alloc@unlink': ['byte', 'call #0', 'call #1'],
    'effects_clear@count': ['byte'],
    'effects_clear@lock': ['byte'],
    'effects_clear@one': ['call #1', 'call #2'],
    'effects_clear@set': ['call #0 memory', 'call #1 memory', 'call #2 memory'],
    'effects_clear@walk': ['call #0', 'call #1', 'call #2'],
    'fighter_164e8@byte': ['byte'],
    'fighter_164e8@noside': ['byte'],
    'fighter_164e8@side': ['byte'],
    'fighter_1a5ac@bit': ['eax'],
    'fighter_1a5ac@side': ['eax'],
    'fighter_1a5ac@slot': ['eax'],
    'fighter_36ce4@bit': ['byte', 'call #0 memory'],
    'fighter_36ce4@mode': ['call #0'],
    'fighter_36ce4@mode22': ['call #0'],
    'fighter_36ce4@rec': ['call #0'],
    'fighter_36ce4@side': ['call #0'],
    'fighter_36ce4@stream': ['call #0'],
    'fighter_41310@add': ['byte'],
    'fighter_41310@clamp': ['byte'],
    'fighter_41310@eq': ['byte'],
    'fighter_41310@mode': ['byte'],
    'fighter_41310@side': ['byte'],
    'fighter_input_read@pos': ['eax'],
    'fighter_input_read@side': ['eax'],
    'fighter_input_read@sign': ['eax'],
    'fighter_input_read@wrap': ['eax'],
    'fighter_pass_flag@eq': ['eax'],
    'fighter_pass_flag@set': ['byte'],
    'fighter_pass_flag@shift': ['eax'],
    'fighter_pass_flag@side': ['byte', 'eax'],
    'fighter_state_365c8@bit': ['eax'],
    'fighter_state_365c8@f43': ['eax'],
    'fighter_state_365c8@other': ['eax'],
    'fighter_state_365c8@signed': ['eax'],
    'hit_vert_distance@diff': ['eax'],
    'hit_vert_distance@one': ['call #1'],
    'hit_vert_distance@order': ['call #0', 'call #1'],
    'hit_vert_distance@side': ['call #0'],
    'list_insert_after@back': ['byte'],
    'list_insert_after@head': ['byte'],
    'list_insert_after@next': ['byte'],
    'list_insert_after@skip': ['byte'],
    'list_unlink@link': ['byte'],
    'list_unlink@one': ['byte'],
    'list_unlink@prev': ['byte'],
    'list_unlink@swap': ['byte'],
    'mode1_cursor@cmp': ['byte'],
    'mode1_cursor@pset': ['byte'],
    'mode1_cursor@shl': ['byte'],
    'mode1_cursor@sub': ['byte'],
    'mode1_cursor@y': ['byte'],
    'snd_music_request@al': ['eax'],
    'snd_music_request@cc': ['byte'],
    'snd_music_request@d9': ['byte'],
    'snd_music_request@pause': ['byte', 'eax'],
    'snd_music_request@seq': ['byte', 'eax'],
    'snd_music_unpause@db': ['byte'],
    'snd_music_unpause@one': ['byte'],
    'snd_music_unpause@word': ['byte'],
    'snd_sample_unpause@da': ['byte'],
    'snd_sample_unpause@one': ['byte'],
}

The exact case set that alone catches each mutant (measured on the prototype; the record's copy of
the test's table):

```
CASES = {
    'actor_alloc@empty': ['a0', 'a1', 'a2', 'a3', 'a4'],
    'actor_alloc@flag': ['a0', 'a1', 'a3', 'a4'],
    'actor_alloc@ret': ['a0', 'a1', 'a3', 'a4'],
    'actor_alloc@tail': ['a0', 'a1', 'a3', 'a4'],
    'actor_alloc@unlink': ['a0', 'a1', 'a3', 'a4'],
    'effects_clear@count': ['e0', 'e1', 'e2'],
    'effects_clear@lock': ['e0', 'e1', 'e2'],
    'effects_clear@one': ['e2'],
    'effects_clear@set': ['e1', 'e2'],
    'effects_clear@walk': ['e1', 'e2'],
    'fighter_164e8@byte': ['s0', 's1', 's2'],
    'fighter_164e8@noside': ['s1', 's2'],
    'fighter_164e8@side': ['s1', 's2'],
    'fighter_1a5ac@bit': ['a1', 'a4', 'a5'],
    'fighter_1a5ac@side': ['a0', 'a1', 'a2', 'a3', 'a4', 'a5'],
    'fighter_1a5ac@slot': ['a0', 'a1', 'a2', 'a3', 'a4', 'a5'],
    'fighter_36ce4@bit': ['c0', 'c1', 'c2', 'c3'],
    'fighter_36ce4@mode': ['c0'],
    'fighter_36ce4@mode22': ['c1'],
    'fighter_36ce4@rec': ['c3'],
    'fighter_36ce4@side': ['c2', 'c3'],
    'fighter_36ce4@stream': ['c2', 'c3'],
    'fighter_41310@add': ['g1', 'g2', 'g3', 'g4', 'g5', 'g6'],
    'fighter_41310@clamp': ['g3'],
    'fighter_41310@eq': ['g5'],
    'fighter_41310@mode': ['g0'],
    'fighter_41310@side': ['g4'],
    'fighter_input_read@pos': ['i0', 'i1', 'i2', 'i3', 'i4'],
    'fighter_input_read@side': ['i4'],
    'fighter_input_read@sign': ['i0', 'i1', 'i2', 'i4', 'i5'],
    'fighter_input_read@wrap': ['i2', 'i5'],
    'fighter_pass_flag@eq': ['f5'],
    'fighter_pass_flag@set': ['f0', 'f2', 'f6'],
    'fighter_pass_flag@shift': ['f3'],
    'fighter_pass_flag@side': ['f1', 'f4'],
    'fighter_state_365c8@bit': ['s10', 's11', 's7', 's8'],
    'fighter_state_365c8@f43': ['s10', 's11', 's12', 's5', 's7', 's9'],
    'fighter_state_365c8@other': ['s10', 's11', 's5', 's7', 's9'],
    'fighter_state_365c8@signed': ['s10', 's9'],
    'hit_vert_distance@diff': ['v0', 'v1', 'v2'],
    'hit_vert_distance@one': ['v0', 'v1', 'v2', 'v3'],
    'hit_vert_distance@order': ['v0', 'v1', 'v2', 'v3'],
    'hit_vert_distance@side': ['v0', 'v1', 'v2', 'v3'],
    'list_insert_after@back': ['l0'],
    'list_insert_after@head': ['l0', 'l1'],
    'list_insert_after@next': ['l0', 'l1', 'l2'],
    'list_insert_after@skip': ['l0', 'l1', 'l2'],
    'list_unlink@link': ['u0', 'u2'],
    'list_unlink@one': ['u0', 'u1', 'u2'],
    'list_unlink@prev': ['u0'],
    'list_unlink@swap': ['u0'],
    'mode1_cursor@cmp': ['y1'],
    'mode1_cursor@pset': ['y3', 'y4', 'y5'],
    'mode1_cursor@shl': ['y3', 'y4'],
    'mode1_cursor@sub': ['y1', 'y2', 'y3', 'y5', 'y6'],
    'mode1_cursor@y': ['y0', 'y1', 'y2', 'y6'],
    'snd_music_request@al': ['r1', 'r2'],
    'snd_music_request@cc': ['r0', 'r3'],
    'snd_music_request@d9': ['r0', 'r1', 'r2', 'r3'],
    'snd_music_request@pause': ['r1'],
    'snd_music_request@seq': ['r2'],
    'snd_music_unpause@db': ['p1', 'p2'],
    'snd_music_unpause@one': ['p1', 'p2', 'p3'],
    'snd_music_unpause@word': ['p1', 'p3'],
    'snd_sample_unpause@da': ['p1', 'p2'],
    'snd_sample_unpause@one': ['p1', 'p2', 'p3'],
}
```

## §C3.4 Counters, measured

| state | diff-verify counter | E2 |
|---|---|---|
| base `6da0128` | `201/201 functions VERIFIED; 598/598 mutants detected; 1 named gaps; 148/167 rows with callees closed (34 have none)` | `233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted 30`; voice `0 / 115 / 19` |
| C3 prototype (sixteen rows) | `217/217 functions VERIFIED; 664/664 mutants detected; 1 named gaps; 154/172 rows with callees closed (45 have none)` | byte-identical |

The arithmetic: +16 functions, +66 mutants, rows with callees 167 -> 172 (the five new rows with
callees: `fighter_1a5ac`, `effects_clear`, `hit_vert_distance`, `fighter_36ce4`, `actor_alloc`; the
other eleven have none), no-callee 34 -> 45 (+11). Closed 148 -> 154 (+6): the three new
with-callee rows whose callees are all verified (`fighter_1a5ac`, `hit_vert_distance`,
`fighter_36ce4`), the three dependents the batch closes (`effects_29cfc` by `0x13DF0`,
`fighter_3aa54` by `0x1A5AC`, `hit_geometry` by `0x1881C`), and no regression. The batch's sixteen
rows account as 3 closed + 2 open + 11 no-callee: the three closed with-callee rows are named
above; the two still open are `effects_clear` (stubbed `0x13420` has no row) and `actor_alloc`
(the `0x249C0`/`0x2EA30` allows have none — both enter the C3b list); and the eleven no-callee
rows count in the 45.

`make entry-triage` is byte-identical (no ported function, no `fn_register`);
`PR_ORACLE_REQUIRED=1 ./build/run_tests` prints `all checks passed`; the Python suite
(`python3 -m unittest tools.tests.test_diff_verify`) runs 105 tests OK and `make diff-verify`
(which chains `test_diff_emu` too) is green; `python3 tools/port_progress.py` stays `771 1203 64` /
`731 731 100`.

## §C3.5 Decisions, named gaps and limits

**No decision is left to the user.** No raw-over-port correction was needed. The host/runtime
callees the batch does not row follow C1/C2's treatment: the `0x62003` fatals are outside these
sixteen rows' paths; `0x2EA30` is the port's inert lock (named, never stubbed with a made-up
behaviour); `0x249C0` is a ported sibling copy with no seam, allowed on both sides in
`actor_alloc`.

Named gaps and limits:

- **`mode1_cursor`'s `0x7F` clamp is dead in both**: `sar edx,0x18` (0x2A662) yields
  [-0x80,0x7F], so `cmp edx,0x80` / `jl` (0x2A665/0x2A66B) always takes the jump to 0x2A671 and
  the store at `0x2A66D` never runs; the port's `>= 0x80` is never true. The row names the block
  unhit (`unhit_named={0x2A66D: ...}`) and has no clamp mutant — one would be equivalent to the
  port.
- **`effects_clear`'s port guard**: the port returns early when the active sentinel
  `DS_000FCCE0` is zero (a PORT deviation for an unbuilt pool); the cases keep it non-zero (the
  empty list is represented by the sentinel pointing at itself, which both sides walk as zero
  iterations). The row claims the built-pool path only.
- **`hit_vert_distance`'s stub writes nothing**, so the row pins the two latch calls (order and
  side) and the difference of the seeded `0x1077E0`/`0x107874`, but a port that loaded them before
  the calls would pass; a load-vs-call reorder is not separated. Named, not claimed.
- **`actor_alloc`'s allows**: `0x249C0` runs on both sides (the port's `list_insert_before` vs the
  raw's bytes; its final memory is compared) and `0x2EA30`'s two calls run on the original side
  only, their net lock write zero because every case seeds `0xBCD60 = 0`. Neither is rowed; both
  are C3b items (§C3.7). The row's tail-insert path is reached by cases a1/a3 and both blocks are
  hit.
- **The list rows bind the effects.c copies** (`effects_list_insert_after`/`effects_list_unlink`),
  while `actor_alloc`'s real calls exercise the actors.c static copies of the same bodies (the
  seams are on the actors.c copies); both copies are compared (the real calls' memory and the final
  bytes), but the *row claim* is on the bound copy. The `m_2ac80_*` mutants call the same seam-less
  effects.c copies rather than the entry's seamed actors.c ones, so they emit no recorded call at
  `0x249D0`/`0x249B0`: on a0 (flag 0, the head insert) `@flag`'s byte outcome agrees with the
  original and the missing call is what catches it, and the pinned a0 case sets rest on that mutant
  route. The entry's own route is covered by the real calls' memory and the final bytes, not by the
  mutants.
- **`fighter_1a5ac`'s raw returns `0x4000` on the bit-set path** (`and eax,0xffff` leaves AH bit
  0x40, then `sete al` leaves it), while the port's C returns 0; the caller 0x3AADC tests
  `test al,al`, so the row's mask is 0xFF and the difference in bits 8+ is a named limit (it is
  scratch the caller does not read).
- **Masks**: `fighter_input_read` 0xFFFF (the raw's `mov ax` leaves the high half as the `side*5`
  scratch; the port's C return is zero-extended), `snd_music_request`/`fighter_pass_flag`/
  `fighter_1a5ac`/`fighter_state_365c8` 0xFF (AL), `hit_vert_distance`/`actor_alloc` full (the
  callers read the whole dword), the void rows 0.
- E3's, P1-P8's and C2/C2b's limits stand: seeds are hand pokes; the memory at a call is mem[]
  only; the callee column is one level deep.
- The earlier records' `title_pin` unittest failure is fixed on this base (the closeout, `6da0128`):
  `python3 -m unittest tools.tests.test_title_pin` runs 10 tests OK; the suite is outside
  `make verify`.

## §C3.6 What the planner ran

- The baseline on the clean `6da0128` worktree: `make diff-verify` -> the §C3.4 base counter and
  table; `make entry-triage` -> `233/262`; `python3 tools/port_progress.py` -> `771 1203 64` /
  `731 731 100`.
- The prototype, in this worktree, family by family (the splice lists; the per-side leaves; the
  fighter leaves; the walkers), each row measured with
  `python3 tools/diff_verify.py --function NAME --self-check` until VERIFIED with every mutant
  detected, then the full `python3 tools/diff_verify.py --self-check` (the §C3.4 counter), the
  `python3 -m unittest tools.tests.test_diff_verify` suite (105 tests OK, including the new
  `test_each_c3_mutant_is_caught_by_what_it_breaks` and the extended clobber exact-set), `make
  entry-triage` (byte-identical) and `PR_ORACLE_REQUIRED=1 ./build/run_tests` (all checks passed).
  Then `git checkout -- port tools`, leaving only this record and the plan.
- The prototype's own fixes during measurement (none a port correction; all fixture or mutant
  corrections): the `m_249b0_next`/`m_249d0_prev`/`m_41310_side` mutants were made memory-safe
  (they had read a sentinel as a pointer); `fighter_state_365c8`'s other-slot seeds were corrected
  from `0x10A682/3` to `0x10A6C2/3` (its `+0x42`/`+0x43` are at the record's own offsets); the
  `fighter_input_read` ring fixture was reduced from 41 pokes to 3 per case (the case struct caps
  at 16); `fighter_41310`'s negative deltas were expressed as two's complement (the cases text
  prints `0x%X`); `actor_alloc`'s sentinel back-link was made self-referential so the tail-insert
  mutants cannot walk a sentinel.
- The prototype's diff (1288 lines, 1170 insertions over 5 files, the exact files the plan's
  Task 2 applies) is embedded in the plan.
- The full `make verify` (~25 min) was not run by the planner: the brief's task-scoped gates are
  the ones above. The `port/src` changes are one seam and one `static` removal that no oracle
  capture can reach; the executor runs the full gate in Task 4.

## §C3.7 The C3b deferral (the rest of the frontier, with evidence)

This session measured sixteen of the 53 candidates. The remaining **36 candidate addresses** —
plus the two new frontier items this batch creates (`0x13420`, `0x249C0`) and the named non-row
`0x2EA30` — are **the next batch (C3b)**, same recipe. The deferral is a session-budget split, not a
correctness judgement: every address below is ported and its direct callees are the port's own
helpers, measured from the final table (the sizes are `port/decomp/prage.functions.csv` bytes;
the callers are the raw's n_callers):

| family | addresses (size bytes / raw callers) |
|---|---|
| the remaining previously-open | `0x18350` (185/3), `0x18540` (229/3), `0x18788` (116/1), `0x2B150` (148/63), `0x29F34` (303/4), `0x38154` (368/4), `0x2EA30` (52/12, **name it**: no port C function, inert by design) |
| the type family | `0x1A734` (151/1), `0x1AB5C` (302/1), `0x2A820` (589/3), `0x2BD44` (89/4), `0x36BC8` (282/3), `0x379C4` (147/1), `0x385B0` (384/1), `0x39040` (566/2), `0x392A0` (907/4), `0x39738` (250/1), `0x3AAFC` (667/1), `0x3AD98` (260/3), `0x3AE9C` (294/1), `0x3B080` (177/4), `0x3B134` (355/2), `0x3B6C4` (77/1), `0x3C520` (77/7), `0x4F434` (178/1) |
| the voice dispatcher | `0x1CA6C` (74/11), `0x1CC28` (370/8), `0x1CD9C` (102/3), `0x1CE04` (106/7), `0x1CE70` (76/5) |
| the allowed tree | `0x29DB8` (306/4), `0x2B8F8` (508/1), `0x3AFC4` (116/17), `0x49444` (97/2), `0x127C0` (not in the function index; the allow target of the type callback), `0x1C390`/`0x1C3A0` (16/1 and 47/2, **the port combines them** into `render_list_insert`/`render_splice`, so C3b must decide the row shape: one row on the combined function with one address named, or name both) |
| new items this batch creates | `0x13420` (133/1, the `effects_clear` stub), `0x249C0` (16/10, the `actor_alloc` allow) |

Closure value still on the table (base table, the rows with unverified callees that remain):
`fighter_slot_latch` and `hit_record_x` close with `0x18350`+`0x18540`; `fighter_1883c` with
`0x18788`; `anim_next_sprite_id` with `0x29F34`; `fighter_state_36638` with `0x38154`;
`fighter_4b03c` with `0x2B150`+`0x41310` (done)+`0x49444`; `sound_voice` with all eight voice
stubs (three are done: `0x1CA14`, `0x1D238`, `0x1D244` — five remain); `spawn_anim_opcode` needs
`0x2B150`, `0x29DB8`, `0x2C3FC` (done), `0x2B8F8`, `0x29F34` plus its bare allows; the
`fighter_command_dispatch`/`fighter_reaction`/`fighter_36870`/`fighter_39834` rows need their
type-family stubs; `effects_spawn` needs its `0x1B544` allow (never rowed) plus `0x249B0`/
`0x249D0` (done); `palette_acquire` needs `0x1B544` (never rowed); `release_record` needs
`0x2B150`, `0x2EA30` (named), `0x249B0`/`0x249D0` (done).

The recommended C3b order (dependency-first, all evidence measured this session):
1. the small leaves that close dependents: `0x18350`+`0x18540`, `0x18788`, `0x29F34`, `0x38154`.
2. `0x2B150` (the 63-caller hub; its indirect type callback and the `0x33864`/`0x1C458`/`0x1C3D0`
   callees need the call-set/allow treatment).
3. `0x3AFC4` (the anim triple; its `0x62003` out-of-range path is a named gap).
4. the voice remainder (`0x1CA6C`, `0x1CE70`, `0x1CD9C`, `0x1CE04`, `0x1CC28`; their AIL runtime
   callees need seams on the port's host wrappers).
5. the type-family tail and the allowed-tree remainder (`0x1C390`/`0x1C3A0` decision;
   `0x29DB8`, `0x2B8F8`, `0x49444`, `0x127C0`).
6. `0x13420` and `0x249C0` (this batch's new items).

`make k11-oracle`-style gates do not apply: this batch touches no gameplay path (only `port/src`
seam/export changes), so the gp miss sets are untouched; Task 1's `PR_GP_DUMP` pinned sets are the
executor's check that they stay so.

## §C3.8 Results (the executed tree)

The plan's Tasks 2-4 were executed on `reverse-c3` at the base `main` `6da0128`: the plan+record
commit `7392029`, Task 2 `17a2b61` (the sixteen rows, the seam and the mutants), Task 3 `4c6b239`
(the store sweep: every site already observable bar the one dead in both, and the two harness
nits) and the review-nits commit `ee57860` (the C3 cross-refs and the `snd_music_unpause` comment),
then this closure commit. Every row was re-measured in the tree; the planner's prototype values
held.

The counters on the final tree equal §C3.4's prototype row:

| state | diff-verify counter | E2 |
|---|---|---|
| base `6da0128` | `201/201 functions VERIFIED; 598/598 mutants detected; 1 named gaps; 148/167 rows with callees closed (34 have none)` | `targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30`; voice `0 / 115 / 19` |
| final (`ee57860`, the closure tree) | `217/217 functions VERIFIED; 664/664 mutants detected; 1 named gaps; 154/172 rows with callees closed (45 have none)` | byte-identical |

`make entry-triage` is byte-identical (no ported function, no `fn_register`); `PR_ORACLE_REQUIRED=1
./build/run_tests` `all checks passed`; `python3 tools/port_progress.py` stays `771 1203 64` / `731
731 100` (neither frontier item is a Ghidra `FN_`); README untouched.

**The full gate** on the closed tree (the plan's parallel-safe overrides, log `/tmp/pr_c3_final.log`):

```
EXIT=0
diff-verify: 217/217 functions VERIFIED; 664/664 mutants detected; 1 named gaps; 154/172 rows with callees closed (45 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
```

`EXIT=0` (the log's last line); the 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture
oracle|smk_compare|title_compare|attract_compare|== demo-fight)'`) diff clean against
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt` (`ORACLES-EQUAL`); `make
audio-render AUDIO_WAV=/tmp/pr_c3.wav` `cmp`-equal to `before-t2.wav` (`WAV-SAME`; both sha256
`df74acfb65d345fb72cb214102089f2a0ab8d4b271ddc17e2a5f5c4f1a380844`); all 33 gp ratchet lines are
`ok` with measured == pin (Task 1's list verbatim); `symbols.h` regenerated byte-identically. The
only `port/src` change of the batch (the `effect_teardown` seam and its `static` removal) is inert
outside `build/diffrun`, and the full ladder is the proof: no oracle line, WAV byte, gp ratchet or
goldens moved.

**The C3b deferral is the next batch**: §C3.7's 36 remaining candidates, this batch's two new
frontier items (`0x13420`, `0x249C0`) and the named non-row `0x2EA30`, in §C3.7's dependency-first
order; nothing else is left open by C3.
