# Named-gaps sub-project C: the allocation-failure seam (G4). Derivations

Plan: `docs/superpowers/plans/2026-09-30-named-gaps-c-alloc-seam.md`. Spec:
`docs/superpowers/specs/2026-09-30-named-gaps-design.md` §4.C. `$K` is
`.superpowers/sdd/2026-09-29-k7-k12/scratch/`, `$C` is
`.superpowers/sdd/2026-09-30-named-gaps-c-alloc-seam/scratch/`.

## §C.0 Baseline and the raw

- Branch `named-gaps-c` from main `af135ec`, which contains both `a169296`
  and `0247a1a` (`BRANCH-OK`); `data` and `.superpowers` are symlinks
  (`LINKS-OK`).
- **Correction (plan Task 1 Step 1).** The plan's check
  `rg -n "0x1D0BC.s two allocation-failure arms ..."` finds nothing: the
  ledger text is `` `0x1D0BC`'s ``, two characters (a backtick and an
  apostrophe) between `C` and `s`, and `.` matches one. The row exists:
  `docs/superpowers/plans/2026-09-29-all-gaps-ledger.md` line 617, §H.3 row 4.
  The tree wins; the check passes on the row, not on the plan's pattern.
- `make verify` (parallel overrides `/tmp/pr_c1_*`) exits 0 (`$C/verify-base.txt`);
  its oracle lines diff empty against `$K/oracle-lines-base.txt`
  (`ORACLES-EQUAL`); `dumps.sh c-base` matches `$K/base.sha256`; the
  `make audio-render` WAV equals `$K/before-t2.wav` (`$C/gate-base.log`).
- Assertion sites: 13545. `port_progress.py`:
  `767 1203 64` and
  `731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)`.
- The Ghidra MCP is unavailable (spec §5). `0x1D0BC..0x1D1AE` was re-read from
  the fixup-applied image (`$K/dx.py 1D0BC 1D1B0` → `$C/dx-1D0BC.txt`) and
  matches k7-k12 §0.7.1/§2.1 instruction for instruction:
  - MIDI arm: `0x1D0CC`/`0x1D0D5` require C4 and C0 non-zero (both `je 0x1D132`);
    `0x1D0DE` loads `ecx` = `DS_001028D0`, which the `0x1D0E6` gate
    requires to be 0; `0x1D0F2` calls `0x1C308(0x41, 0x5100)`; `0x1D0F7`
    stores EAX; `0x1D0FE je 0x1D10E`. The failure arm `0x1D113`/`0x1D11E`/
    `0x1D124` stores `ecx` (0) to C4, C0 and CC, prints through `0x62734`
    (format `0x8063C`), and falls through (`0x1D12F add esp,8`) to `0x1D132`.
  - Slot arm: `0x1D132 cmp [0x1028C8],0`; `0x1D13B..0x1D147` load slot 0's
    `+0x10`, clear `ecx`/`ebx`, `jne 0x1D17F`; sizes `0x1D14D mov edx,0x8C00`
    (ecx = 0) / `0x1D154 mov edx,0x6000`; `0x1D163 mov [ebx+0x102870],eax`;
    `0x1D16B je 0x1D17F` precedes `0x1D16D inc ecx`, so the failed slot is not
    counted; `0x1D171 cmp ecx,4` / `0x1D176 cmp [ebx+0x102870],0`;
    `0x1D17F test ecx,ecx` / `0x1D181 jne 0x1D19B` / `0x1D195 mov [0x1028C8],ecx`
    turn the DIG driver off only when `ecx` = 0 (slot 0 failed, or was set at
    entry, `0x1D147`); `0x1D19B mov dl,1` / `0x1D19D mov [0xA2CB0],dl`.
  - The plan's Step 3 list omits the `0x1D0CC`/`0x1D0D5` C4/C0 gates; not a
    conflict (V1 seeds both non-zero, V2-V4 seed both 0 so the MIDI arm is
    skipped), recorded for completeness.
- The raw sees only EAX = 0 from `0x1C308`; the port's `0x1C308` is `res.c`'s
  bump allocator (host-owned, k7-k12 §0.7.1), whose only failure is
  `!mem_in_range` (returns 0, heap unmoved).

## §C.1 The seam

- **Design.** `res.c` gains `static u32 s_fail_left` (`PORT:`, the
  `res_set_screen_hook` / `sound_voice_log` precedent: test state, not
  original state) and `res_fail_alloc_nth(n)` / `res_fail_alloc_left()`
  (`res.h`). `res_alloc`'s first line returns 0 when a non-zero count reaches
  0, before the heap is aligned or moved, the same observable result as its
  `mem_in_range` arm.
- **Why here, and why one-shot.** In `res_alloc` rather than a replacement
  hook on `res_block_alloc`, so the injected failure goes through the real
  allocator and its heap effect is testable (and a later task can reach
  `res_load_index`/`res_load_file`'s arms). One-shot rather than
  "fail from the n-th on": after the failure the heap serves again, so a slot
  loop that failed to stop at `0x1D16B` would visibly give the next slot a
  block. With a sticky failure it would store 0 over the 0 seeds the loop
  condition `0x1D176` requires, and the missing break would be invisible.
  Exhausting `mem[]` instead (k7-k12 §2.4) would move every later heap
  address in the suite.
- **Inert when unarmed.** `s_fail_left` starts at 0; `rg -n 'res_fail_alloc|s_fail_left' port/src`
  lists only the two `res.h` declarations (plus their comment), and in
  `res.c` the static, the two one-line definitions and the `res_alloc` test
  line: no call of `res_fail_alloc_nth` outside its definition, so in every
  production and oracle-driver process the `s_fail_left != 0u` test is false
  and `res_alloc` runs today's instructions. Empirically `make verify` exits 0
  with `ORACLES-EQUAL` and `dumps.sh c-t1` matches `$K/base.sha256`
  (`$C/gate-t1.log`; the first `dumps.sh c-t1` run had its `--check` and
  front-end driver processes killed by an external SIGTERM while other
  worktrees ran concurrently, so it was rerun alone and matched).
- **Test.** `test_platform.c` `check_res_fail_seam()`, called at the end of
  `test_res`: armed with 3, requests 1-2 succeed (size 0), request 3 (`0x100`)
  returns 0, the count is 0 and the heap has not moved; arming 1 then 0
  disarms, and an unarmed request neither fails nor touches the count.
  Before the seam the build fails (5 errors: `res_fail_alloc_nth` undeclared
  once at `test_platform.c:180`, `res_fail_alloc_left` at `:181`, `:183`,
  `:186`, `:192`; clang reports each function's later calls against the
  implicit declaration). Assertion sites +10 (13545 → 13555).
- **Mutations** (each applied alone, rebuilt, run with `PR_ORACLE_REQUIRED=1`,
  restored; `$C/mut-S*.txt`). `a` measured = 45854608 (`0x2BBB090`). Lines are
  `test_platform.c` unless named.

| id | mutation (in `res.c`) | measured FAIL lines |
|---|---|---|
| S0 | `res_block_alloc` returns 0 (`(void)res_alloc(size); return 0;`) | `:178: the bump heap has room`, then the existing `check_sound_buffers` checks (`test_game.c:1320`, `:1322`-`:1325`, `:1327`, `:1340`, `:1371`, `:1374`, `:1388`, `:1389`, `:1395`, `:1397`), `test_flow`'s slot/mixer checks (`test_game.c:1598`, `:1604`, `:1625`, `:926` ×4, `:931`-`:933`, `:936`, `:937`, `:946`, `:951`, `:960`, `:962`, `:963`): 31 complete FAIL lines, then the suite dies with SIGBUS (`Bus error: 10`) before its summary line (every allocation is 0, so later code writes through offset 0; not analysed further). The seam's own check fails first. |
| S1 | `if (s_fail_left != 0u) return 0;` (fails every armed request, never counts down) | 5: `:182: 0 != 45854608`; `:183: 3 != 2`; `:184: 0 != 45854608`; `:186: 3 != 0`; `:187: 0 != 45854608` |
| S2 | the seam line after `g_heap = at + size;` (the failure advances the heap) | 2: `:187: 45854864 != 45854608`; `:191: 45854864 != 45854608` |
| S3 | `if (n != 0u) s_fail_left = n;` (n = 0 does not disarm) | 1: `:191: 0 != 45854608` |
| S4 | `if (--s_fail_left == 0u) return 0;` (the unarmed path decrements) | 1: `:192: -1 != 0` |

After the five, `cmp $C/res.c.good port/src/platform/res.c` is silent and the
rebuilt suite prints `all checks passed`. S1-S4 match the plan's counts
exactly; S0's count was left open by the plan ("record the count").

## §C.2 The two arms

`test_game.c` `check_sound_buffers()` gains four vectors after the
slot-0-set-at-entry vector (it now also saves and restores `DS_001028CC`).
`sound_buffers_alloc()` runs on the unit-suite process from `test_flow`,
without `game_init()`.

| vector | seeds | armed | asserted |
|---|---|---|---|
| V1 MIDI failure (`0x1D10E..0x1D12F`) | `A2CB0` = 0, `C0` = `0x1234`, `C4` = `0x5678`, `CC` = `0xCC01`, `D0` = 0, `C8` = 1, slots = 0 | 1 | AL = 1; count 0; `A2CB0` = 1; C4 = C0 = CC = 0; slot 0 = the pre-call peek (the arm falls into `0x1D132`, the failed request took nothing) |
| V2 slot 0 fails (`0x1D16B`, ecx = 0) | `A2CB0` = 0, `C0` = `C4` = 0, `D0` = `0xD0D0D0D0`, `C8` = 1, slots = 0 | 1 | AL = 1; count 0; `C8` = 0 (`0x1D195`); slot 1 = 0; heap unmoved |
| V3 slot k = 1..3 fails | as V2 | k + 1 | AL = 1; count 0; slot 0 = peek; slots 1..k-1 ≠ 0; slots k+1..3 = 0; `C8` = 1; next block − slot k-1 = `0x8C00` (k = 1) / `0x6000` |
| V4 slot 0 set at entry (`0x1D147`) | `A2CB0` = 0, `C0` = `C4` = 0, `C8` = 1, slot 0 = `0x0BAD`, slots 1..3 = 0 | 1 | AL = 1; count 1 (no request); slot 0 = `0x0BAD`; `C8` = 0 |

Assertion sites: +23 (V1 7, V2 5, V3 7, V4 4); with §C.1's +10 the tree holds
13578 (13545 + 33). The suite passes on the first run (characterisation of
arms already ported); the mutations prove the assertions can fail.

Mutations (each alone in `sound_buffers_alloc`, `port/src/game/flow.c`,
rebuilt, `PR_ORACLE_REQUIRED=1`, restored with `git checkout`; `$C/mut-M*.txt`).
Lines are `test_game.c`; V1 is `:1431..:1438`, V2 `:1453..:1458`, V3
`:1476..:1487`, V4 `:1501..:1505`; the pre-existing entry-case vector is
`:1408..:1410`. Heap offsets are this run's (decimal).

| id | mutation | measured FAIL lines |
|---|---|---|
| M1 | delete the MIDI arm's three stores (`0x1D113`/`0x1D11E`/`0x1D124`) | 3: `:1435: 22136 != 0` (C4), `:1436: 4660 != 0` (C0), `:1437: 52225 != 0` (CC) |
| M1a | delete only the C4 store (`0x1D113`) | 1: `:1435: 22136 != 0` |
| M1b | delete only the C0 store (`0x1D11E`) | 1: `:1436: 4660 != 0` |
| M1c | delete only the CC store (`0x1D124`) | 1: `:1437: 52225 != 0` |
| M2 | delete the loop stop `if (b == 0u) break;` (`0x1D16B`) | 8: V2 `:1456: 1 != 0` (C8), `:1457: 46268088 != 0` (slot 1 = peek), `:1458: 73728 != 0` (heap); V3 k=1 `:1484: 46377656 != 0`, `:1484: 46402232 != 0` (slots 2, 3), `:1487: 84992 != 35840`; V3 k=2 `:1484: 46487224 != 0` (slot 3), `:1487: 49152 != 24576`. k = 3 is unaffected (i = 4 ends the loop). |
| M3 | count the failed slot: `{ i++; break; }` | 1: `:1456: 1 != 0` (V2 C8) |
| M4 | any failure turns the DIG off: `{ DSD(DS_001028C8) = 0; break; }` | 3: `:1485: 0 != 1` for k = 1, 2, 3 |
| M5 | the MIDI failure skips the sample arm (`DSB(DS_000A2CB0) = 1u; return 1;` after the CC store) | 1: `:1438: 0 != 46158520` (V1 slot 0) |
| M6 | skip the slot-0 entry gate: `if (1) {` (`0x1D147`) | 4: existing entry vector `:1409: 1 != 0` (C8), `:1410: 46194360 != 0` (slot 1); V4 `:1502: 0 != 1` (count), `:1504: 0 != 2989` (slot 0) |
| M7 | the sample arm never runs: `if (0) {` (`0x1D132`) | 48, among them the new: V1 `:1438: 0 != 45878968`; V2 `:1454: 1 != 0` (count) and `:1456: 1 != 0`; V3 `:1477: 2 != 0`, `3 != 0`, `4 != 0` (the `== 0` count checks can fail), `:1479` ×3, `:1482` ×3, `:1487` ×3; V4 `:1505: 1 != 0`. The rest are existing first-run checks (`:1323..:1328`, `:1341`, `:1372`, `:1409`), `check_sample_slots` (`:926` ×4, `:931..:990`) and `test_flow`'s sample/mixer checks (`:1695`, `:1701`, `:1722`). |
| S3′ | Task 1's S3 in `res.c` (n = 0 does not disarm), backup refreshed from `HEAD` | 24: `test_platform.c:191: 0 != 45854608` (Task 1's line) and V4's leaked arm (count 1 after the call, never disarmed) failing the next allocation: `test_flow`'s later `sound_buffers_alloc()` loses slot 0, so `:1695` (the 0x40 voice), `:1701` (mixer non-silence), `:1722: 0 != 1`, and `check_sample_slots` `:926` ×4, `:931..:937`, `:946`, `:951`, `:960..:963`, `:968..:990` fail. This is Review Focus 5's leak detection. |

After the table `git status --short port/src` is empty and the rebuilt suite
prints `all checks passed`. Every count matches the plan's table (M7 and S3′
were left to measurement: 48 and 24). The two arms' named proofs: M1 (delete
the MIDI arm's zeroing: 3 FAIL lines, one per store) and M2 (delete the loop
stop: 8 FAIL lines over V2 and V3).

### §C.2.1 Not tested

- `0x1D0F7`'s store of 0 into `DS_001028D0` on the failure path and `0x1D163`'s
  store of 0 into the failed slot's `+0x10`: both destinations must already be
  0 to reach the store (`0x1D0E6`; `0x1D147`/`0x1D176`), so no state tells a
  store from none.
- The two `0x62734` messages (`0x8063C` MIDI, `0x80684` slots): `PORT:` not
  printed (k7-k12 §2.1), so there is nothing to observe.
- Both arms failing in one call (MIDI and slot 0): the seam is one-shot by
  design (§C.1); each arm is pinned alone.
- `0x1C308`'s own failure modes (a real paged allocator with free): host-owned;
  the port's allocator fails only by `mem_in_range`, which the seam mirrors.

## §C.3 Gate and closure

- `make clean && make build && make verify` (overrides `/tmp/pr_c3_*`) exits 0
  (`$C/verify-t3.txt`); the oracle lines diff empty against
  `$K/oracle-lines-base.txt` (`ORACLES-EQUAL`); `dumps.sh c-after` matches
  `$K/base.sha256` (`DUMPS-IDENTICAL`, dump deleted); the `make audio-render`
  WAV is byte-identical to `$K/before-t2.wav`; `port_progress.py` is unchanged
  (no function added: `767 1203 64`, portable `731 731 100`). 0 warnings.
  Assertion sites 13578 (13545 + 33). The oracle fixtures `make clean`
  deletes were backed up and restored byte-identical (`cmp` against the main
  checkout's). `$C/gate-t3.log`.
- `rg -n 'res_fail_alloc_nth' port/src` lists the `res.h` declaration, the
  `res.c` definition and one `res.h` comment line that names it (the plan
  expected only the first two; the comment is the declaration's own). No call.
- **G4 closed.** Both allocation-failure arms of `0x1D0BC` are tested with
  seeded sentinels and mutation proofs (§C.2: M1 for the MIDI arm, M2/M3/M4 for
  the slot break and the DIG-off test, M6 for the entry gate), through a
  `PORT:` seam that is inert unarmed (§C.1). Residue, not a gap in behaviour:
  the two unobservable stores of 0 and the unprinted messages (§C.2.1).
- Noticed, out of scope (other allocation-failure arms in `port/src`; line
  numbers are the tree after §C.1, which shifted `res.c` by 12 lines against
  the plan's):
  - `res_load_index`'s `table == 0` (`res.c:177`) and per-entry `data == 0`
    (`res.c:201`) returns, and its four trailing `res_alloc` results stored
    unchecked (`res.c:238-241`, `0x1B2A0..0x1B3A9` per the plan, not re-read
    here): untested; `res_load_index` can run only once per process (a second
    load exhausts the heap), so a test needs its own env-gated driver. The
    seam can reach them.
  - `game_audio_init`'s `FAT.OPL` load (`flow.c:5907`,
    `res_load_file(s_game_dir, "FAT.OPL", ...)`): the no-bank arm is untested.
  - Already covered: `res_load_file`'s `off == 0` arm (`res.c:267`, the
    128 MiB sparse fixture in `test_res`) and `movie_play`'s missing-file arm
    (`game/movie.c:91`, `test_video.c:330`).
  - Host-only `malloc` failures (`mem.c` `slurp`, `ail.c` sample growth) and
    the NULL arms of `AIL_allocate_sample_handle`/`AIL_allocate_sequence_handle`
    (`flow.c` `game_audio_init`): host, no raw counterpart.
