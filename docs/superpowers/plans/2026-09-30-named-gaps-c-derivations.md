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
