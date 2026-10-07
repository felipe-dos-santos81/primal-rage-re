# Reverse completion C3d: the frontier rows, part 4 (record)

**Scope.** Track P's seventh verification-only batch (roadmap row C3d of record
`2026-10-05-reverse-c3c-derivations.md` §C3c.7): ten of the thirteen remaining type-family rows —
`0x1AB5C` (`fighter_input_mask`), `0x2A820` (`pset_write`), `0x36BC8` (`fighter_state_36bc8`),
`0x379C4` (`fighter_379c4`), `0x385B0` (`fighter_385b0`), `0x3AD98` (`fighter_3ad98`), `0x3AE9C`
(`fighter_3ae9c`), `0x3B080` (`fighter_3b080`), `0x3B134` (`fight_command_map`) and `0x4F434`
(`fighter_4f434`) — **ten rows, 71 mutants, all detected.** The other three type rows (`0x39040`,
`0x392A0`, `0x3AAFC`) and the frontier tail the ten rows' callees name are **C3e** (§C3d.7), with
the measured evidence: the tail's 23 addresses surface 11 further unrowed callees and 12 more behind
those, so it cannot close in this batch. One raw-over-port correction (`0x3AD98`, §C3d.2). Plan:
`2026-10-05-reverse-c3d-frontier-rows-4.md`. Recipe: E3 record §E3.10; checklist:
`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**Status of the numbers.** Measured by the planner on 2026-10-07 on `reverse-c3d` at the base
`main` `0bda4fa` (= C3c merged), in this worktree: the baseline gates and a full prototype of the
ten rows (every gate run, then reverted). The image is `build/diffrun --exe
data/game/C/PRAGE.EXE --image-out FILE`, sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's,
E3's, P1-P8's, C1-C3c's). Every address below is capstone 5.0.7 over that image (fixups applied);
`unicorn` 2.1.4 runs the original side. Ghidra was not consulted. The prototype diff (1447 lines,
1184 insertions over eight files) is embedded in the plan; the prototype was reverted
(`git checkout -- port tools`), leaving only this record and the plan.

---

## §C3d.1 The members, re-derived from the raw

The C3c record §C3c.7 defers thirteen type-family rows. This session measured **ten**; the other
three are C3e (§C3d.7). The ten were re-checked against the final base table and the raw's call
scans (capstone over the image):

| row | entry | size | the raw's direct callees (mode in this batch) |
|---|---|---|---|
| `fighter_input_mask` | `0x1AB5C` | 302 | `0x33A10` allow, `0x46460` stub, `0x1AB10` allow, `0x18B04` stub, `0x1A7CC` stub |
| `pset_write` | `0x2A820` | 589 | `0x2A690` stub, `0x2B150` stub |
| `fighter_state_36bc8` | `0x36BC8` | 282 | `0x38BC8` stub, `0x38BB0` allow, `0x2BC30` stub, `0x188AC` stub |
| `fighter_379c4` | `0x379C4` | 147 | `0x37178` stub, `0x2BC30` stub, the `0x1078E8` callback allow |
| `fighter_385b0` | `0x385B0` | 384 | `0x164E8` stub, `0x2BC30` stub, `0x38154` stub |
| `fighter_3ad98` | `0x3AD98` | 260 | `0x2C3FC` stub, `0x2AE14` stub, `0x2BC30` stub, `0x392A0` stub, `0x33A10` allow |
| `fighter_3ae9c` | `0x3AE9C` | 294 | `0x33950` allow, `0x1A570` real |
| `fighter_3b080` | `0x3B080` | 177 | `0x1A570` real, `0x3C148` stub, `0x3B038` allow |
| `fight_command_map` | `0x3B134` | 355 | `0x33A10` allow, `0x3AFC4` allow, `0x1AB10` allow, `0x5D7DC` stub, `0x3BDB0` stub, `0x3BDDC` stub |
| `fighter_4f434` | `0x4F434` | 178 | `0x46534` allow |

Every one of the ten is ported (a `/* 0xADDR` header in `port/src`) **and already carries a
`PR_SEAM`** (an earlier row stubs it; the declared-clobber table in the tests lists all thirteen),
so the rows needed no change to the entries themselves. Their **missing** callees were re-derived
from the raw (capstone) and the base table, matching the C3c §C3c.7 table: the 23 addresses
`0x1AB10` `0x1A7CC` `0x2A690` `0x38BC8` `0x38BB0` `0x37178` `0x38D90` `0x38FEC` `0x4F944` `0x33A68`
`0x46190` `0x36E78` `0x3A280` `0x3A0FC` `0x36D20` `0x3A504` `0x3A650` `0x3A79C` `0x3A8E8` `0x3B038`
`0x3BDB0` `0x3BDDC` `0x46534` (plus `0x1A6AC`, reached through `0x1A7CC`, and `0x2A620`, reached
through `0x2A690`). Every call target of every row is one of: a rowed function (stub or real), one
of the six leaf allows (`0x1AB10`, `0x33A68`, `0x38BB0`, `0x3A280`, `0x3B038`, `0x46534`), or a
stub with a C3d seam.

The measured rows (cases/blocks/mutants measured; `EAX mask` from the Spec):

| row | entry | cases | blocks | mutants | EAX mask | named unhit |
|---|---|---|---|---|---|---|
| `fighter_4f434` | 0x4F434 | 9 | 10/10 | 6/6 | 0 | — |
| `fighter_385b0` | 0x385B0 | 5 | 8/8 | 6/6 | 0 | — |
| `fighter_3ae9c` | 0x3AE9C | 9 | 23/23 | 7/7 | 0 | — |
| `fighter_3b080` | 0x3B080 | 5 | 7/7 | 6/6 | 0 | — |
| `fighter_state_36bc8` | 0x36BC8 | 6 | 9/9 | 6/6 | 0 | — |
| `fighter_379c4` | 0x379C4 | 6 | 10/10 | 7/7 | 0 | — |
| `fighter_3ad98` | 0x3AD98 | 8 | 15/15 | 8/8 | 0 | — |
| `fighter_input_mask` | 0x1AB5C | 9 | 24/24 | 9/9 | 0xFFFFFFFF | — |
| `fight_command_map` | 0x3B134 | 12 | 19/19 | 9/9 | 0 | — |
| `pset_write` | 0x2A820 | 11 | 27/28 | 7/7 | 0 | `0x2A9CE` (the child layer is an 8-bit add) |

No `reads outside the image` in any row.

## §C3d.2 The rows: cases, seams, fixtures and the corrections

Each row is a `Spec` in `tools/diff_verify.py` (`C3D_SPECS`), a binding `b_*` and mutants `m_*` in
`port/tests/diff_runner.c` (`k_bindings`), and the exact-set extensions in
`tools/tests/test_diff_verify.py` (`C3D_MASKS`, `C3D_KINDS`, the case-set table, the clobber table,
the counter line). The measured rows:

| row | entry | cases | blocks | mutants | callees (each by its own check) | EAX mask |
|---|---|---|---|---|---|---|
| `fighter_4f434` | 0x4F434 | 9 | 10/10 | 6/6 | `46534` allow unverified | 0x0 |
| `fighter_385b0` | 0x385B0 | 5 | 8/8 | 6/6 | `164E8` stub VERIFIED, `2BC30` stub VERIFIED, `38154` stub VERIFIED | 0x0 |
| `fighter_3ae9c` | 0x3AE9C | 9 | 23/23 | 7/7 | `1A570` real VERIFIED, `33950` allow VERIFIED | 0x0 |
| `fighter_3b080` | 0x3B080 | 5 | 7/7 | 6/6 | `1A570` real VERIFIED, `3C148` stub VERIFIED, `3B038` allow unverified | 0x0 |
| `fighter_state_36bc8` | 0x36BC8 | 6 | 9/9 | 6/6 | `38BC8` stub unverified, `38BB0` allow unverified, `2BC30` stub VERIFIED, `188AC` stub VERIFIED | 0x0 |
| `fighter_379c4` | 0x379C4 | 6 | 10/10 | 7/7 | `37178` stub unverified, `2BC30` stub VERIFIED, `45D14`/`5D812` allow unverified | 0x0 |
| `fighter_3ad98` | 0x3AD98 | 8 | 15/15 | 8/8 | `2C3FC` stub VERIFIED, `2AE14` stub VERIFIED, `2BC30` stub VERIFIED, `392A0` stub unverified, `33A10` allow VERIFIED | 0x0 |
| `fighter_input_mask` | 0x1AB5C | 9 | 24/24 | 9/9 | `46460` stub VERIFIED, `18B04` stub VERIFIED, `1A7CC` stub unverified, `1AB10`/`33A10` allow unverified | 0xFFFFFFFF |
| `fight_command_map` | 0x3B134 | 12 | 19/19 | 9/9 | `5D7DC` stub VERIFIED, `3BDB0` stub unverified, `3BDDC` stub unverified, `3AFC4`/`1AB10`/`33A10` allow unverified | 0x0 |
| `pset_write` | 0x2A820 | 11 | 27/28 | 7/7 | `2A690` stub unverified, `2B150` stub VERIFIED | 0x0 |

(The callee column is the full-run table's; the per-row run reports `unverified` until the callees'
own rows are in the same run. `fighter_command_dispatch`, `fighter_385b0` and `fighter_3ae9c` are
the three rows this batch closes, §C3d.4.)

**Seams and exports** (each seam a first statement; `E.callee_clobbers` re-derives every declared
clobber; a stub's call is declared with those clobbers in `C3D_SPECS`):

| function | address | change | conformance |
|---|---|---|---|
| `fighter_block_start` | `0x1A7CC` | gains `PR_SEAM`; declared in `fighter.h`? (already exported) | clobbers `esi`,`edi`,`ebp` |
| `actor_pset_point` | `0x2A690` | gains `PR_SEAM` | no clobbers |
| `fighter_38bc8` | `0x38BC8` | gains `PR_SEAM`; loses `static` (mutant cores call it) | no clobbers |
| `fighter_38bb0` | `0x38BB0` | loses `static`; declared in `fighter.h` | no clobbers |
| `fighter_37178` | `0x37178` | gains `PR_SEAM` | clobbers `esi`,`edi`,`ebp` |
| `fighter_38d90` | `0x38D90` | gains `PR_SEAM` | no clobbers |
| `fighter_38fec` | `0x38FEC` | gains `PR_SEAM` | no clobbers |
| `fighter_46190` | `0x46190` | gains `PR_SEAM_RET0` | no clobbers |
| `fighter_36e78` | `0x36E78` | gains `PR_SEAM` | no clobbers |
| `fighter_3a0fc` | `0x3A0FC` | gains `PR_SEAM` | clobbers `ebp` |
| `fighter_36d20` | `0x36D20` | gains `PR_SEAM_RET` | no clobbers |
| `fighter_pose_3a504` / `3a650` / `3a79c` / `3a8e8` | `0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8` | each gains `PR_SEAM(side, edx)` (the shared `fighter_pose_commit` body is untouched) | clobbers `edx` each |
| `fight_attack_ready` | `0x3BDB0` | gains `PR_SEAM_RET`; loses `static`; declared in `fight.h` | no clobbers |
| `fighter_attack_consume` | `0x3BDDC` | gains `PR_SEAM_RET` | clobbers `ebp` |

Sixteen seams, two `static` removals (`fighter_38bc8`; `fight_attack_ready`) plus `fighter_38bb0`'s
export, and the `0x3AD98` correction (below). No seam is added to the six leaf allows
(`0x1AB10`, `0x33A68`, `0x38BB0` keeps only its export, `0x3A280`, `0x3B038`, `0x46534`) — they run
on both sides unrecorded, the C3b/C3c treatment. Adding the seams left `--self-check` at the base
counter (`251/251; 874/874; 178/195 (56)`), so no existing row moved.

**Corrections during prototyping (each recorded here with its evidence):**

1. **`fighter_3ad98` was a raw-over-port correction (the batch's one).** The port passed the sum
   `DSB(slot+0x5A) + DSB(anim[0]+4)` as `0x392A0`'s EDX. The raw's non-clamp path reaches `0x3AE83`
   with EDX still the `anim[0]+4` byte (`0x3AE53 xor edx,edx; 0x3AE55 mov dl,[ebx+4]`; the sum is
   `lea ecx,[eax+edx]` at `0x3AE6D` and only the `0x3AE75..0x3AE81` clamp replaces EDX). Evidence:
   case `a0`'s original call record is `0x392A0(0x1077B0, 0x20, 0x5)` (EDX = the +4 byte, 0x20) while
   the port replayed `0x392A0(0x1077B0, 0x30, 0x5)` (EDX = the sum 0x10+0x20); with the fix every
   case and all 8 mutants VERIFIED. The port now computes `v = DSB(anim[0]+4)`, and only
   `sum >= 0x78` replaces it with the clamped `0x77 - slot+0x5A` (or 0).
2. **`fight_command_map`'s third argument is EBX, not ECX.** The raw stores `bl` to `[esp+0x24]` at
   entry (`0x3B13D`) and tests that byte at `0x3B1B1`; the existing rows' call set already names
   `E.Call(0x3B134, ("eax", "edx", "ebx"))`. The row's binding/core read `R_EBX`; the `m7`/`m11`
   cases set `ebx`. Evidence: before the fix `m11` (EBX=1, draw 100 > thr 50) mismatched (original
   proceeded, port returned at the gate) and `@ov` was undetected; after, 19/19 blocks and 9/9
   mutants.
3. **`pset_write`'s `0x2A9CE` is a dead clamp.** The child arm's layer is an 8-bit add
   (`0x2A9B5 mov al,[esi+0xe]; 0x2A9B8 add al,[ebx+0x59]`), so it cannot exceed 0xFF and the
   `> 0xff` clamp cannot fire; the block is named unhit with that reason.
4. **Fixture/draft corrections (no port change):**
   - `0x4F434`: the first draft seeded the `0x1082C8[opp]` counter nonzero, so the `dx` test and the
     `±1` nudge used the same word and the cap (`DSB(0xC9408 + DSB(0x10452C))` = 5 at index 3)
     hid the sign; the final fixture seeds the counter 3 and `0x1082C0[opp] = 3 - dx`, so dx is exact
     and the clamped nudge is visible (`@scale`, `@thr`, `@delta` then detected).
   - `0x3AE9C`: the k word is the word at `rec+0x44` (`0x3AF4C mov edx,[edx+0x42]; sar edx,0x10`),
     not a dword at `+0x42`; the first draft poked `0x40 << 16` at `+0x42`, leaving the
     `k < 0x1A`/`k > 0x1E` blocks unhit (21/23); with `+0x44` poked, 23/23.
   - `0x1AB5C`: the `@word` mutant (the command-word index drops `*2`) needs the two words
     separated; the fixture now pokes the 4-byte `0x1088E0` pair (side 1's word at `+2` with `+1` a
     zero byte), and i8 seeds ring 0x8000 + word 0x1000 so only the real index sets bit 0x1000.
   - `0x3B134`: the `anim[2]+2` byte is at `0xA672A` only when the `0x3AFC4` allow computes
     `char = 0`; the image's own `0x10782A`/`0x1078BE` bytes are zero, so no slot poke is needed,
     but the first draft left them unpoked and every anim case read another address (the six
     anim blocks unhit until the char bytes were confirmed zero).
   - The 16-poke-per-case cap: every `c3d_*` helper seeds contiguous regions as one poke (the slot
     block, the record block, the parent pool/pset buffers); `0x385B0`'s 30-field fixture collapsed to
     11 pokes, `pset_write`'s to 12.
   - `0x3AD98`'s first draft poked the anim structs at `0x10B100+`, **outside the image** (the data
     object ends at `0x10B0CF`); they moved to `0x10A000`-`0x10A480`.

**Fixtures.** The rows reuse the C3c scratch records (`0x10AF00`/`0x10AF40`), the slot pointer pair
and both slots (`c3d_slot_pokes` seeds `0x1077A8` and slot 1's record pointer), the actor pool
`0x1014EC -> 0x10B000`, and per-row scratch (`0x10A000`-`0x10AA00` for `0x3AD98`/`pset_write`,
`0x10B100`-`0x10B400` never used). The leaf allows need no seam.

## §C3d.3 The mutants

71 mutants, all detected (`python3 tools/diff_verify.py --self-check`: `945/945`); what alone
catches each is pinned by `test_each_c3d_mutant_is_caught_by_what_it_breaks` (`C3D_KINDS` plus the
measured case-set table; the exact dicts are the ones committed in `tools/tests/test_diff_verify.py`).
The kinds (measured; the record's copy of the test's `C3D_KINDS`):

```
C3D_KINDS = {
    "fighter_4f434@sel": {'byte'},
    "fighter_4f434@scale": {'byte'},
    "fighter_4f434@thr": {'byte'},
    "fighter_4f434@dx": {'byte'},
    "fighter_4f434@delta": {'byte'},
    "fighter_4f434@call": {'byte'},
    "fighter_385b0@af8": {'byte', 'call #0 memory', 'call #1 memory'},
    "fighter_385b0@arm": {'byte', 'call #1 memory'},
    "fighter_385b0@five": {'byte', 'call #1', 'call #1 memory'},
    "fighter_385b0@anim": {'call #1'},
    "fighter_385b0@tail": {'byte'},
    "fighter_385b0@call": {'call #1'},
    "fighter_3ae9c@s53": {'byte', 'call #0'},
    "fighter_3ae9c@g": {'byte', 'call #0 memory'},
    "fighter_3ae9c@sign": {'byte', 'call #0'},
    "fighter_3ae9c@tab": {'byte', 'call #0 memory'},
    "fighter_3ae9c@u44": {'byte', 'call #0 memory'},
    "fighter_3ae9c@k": {'byte', 'call #0 memory'},
    "fighter_3ae9c@bit": {'byte', 'call #0 memory'},
    "fighter_3b080@gate": {'byte', 'call #0', 'call #1'},
    "fighter_3b080@neg": {'byte', 'call #2 memory', 'call #3 memory'},
    "fighter_3b080@mirror": {'byte', 'call #2', 'call #3'},
    "fighter_3b080@p4": {'byte', 'call #2', 'call #3'},
    "fighter_3b080@c148": {'call #1', 'call #2', 'call #3'},
    "fighter_3b080@o43": {'byte', 'call #2 memory', 'call #3 memory'},
    "fighter_state_36bc8@other": {'call #0', 'call #1', 'call #2', 'call #2 memory', 'call #3'},
    "fighter_state_36bc8@cond": {'call #0', 'call #1', 'call #2', 'call #2 memory', 'call #3'},
    "fighter_state_36bc8@anim": {'call #0', 'call #1', 'call #1 memory', 'call #2'},
    "fighter_state_36bc8@s5d": {'byte', 'call #2 memory'},
    "fighter_state_36bc8@bit": {'byte'},
    "fighter_state_36bc8@tail": {'call #2'},
    "fighter_379c4@fe": {'byte', 'call #0'},
    "fighter_379c4@s57": {'call #0'},
    "fighter_379c4@b41": {'call #0'},
    "fighter_379c4@e8": {'byte', 'call #0', 'call #0 memory'},
    "fighter_379c4@cb": {'byte', 'call #1'},
    "fighter_379c4@fc": {'byte', 'call #0 memory'},
    "fighter_379c4@tab": {'call #0'},
    "fighter_3ad98@voice": {'byte', 'call #1', 'call #2', 'call #3'},
    "fighter_3ad98@off": {'byte', 'call #1', 'call #2', 'call #3'},
    "fighter_3ad98@tab": {'byte', 'call #1', 'call #2', 'call #3'},
    "fighter_3ad98@zero": {'byte', 'call #1', 'call #2', 'call #3'},
    "fighter_3ad98@call": {'byte', 'call #1', 'call #2', 'call #3'},
    "fighter_3ad98@clamp": {'byte', 'call #1', 'call #2', 'call #3'},
    "fighter_3ad98@d": {'byte', 'call #1', 'call #2', 'call #3'},
    "fighter_3ad98@bit": {'byte', 'call #1', 'call #2', 'call #3'},
    "fighter_input_mask@loop": {'call #6', 'call #6 memory', 'call #7', 'call #7 memory', 'call #8'},
    "fighter_input_mask@word": {'call #7', 'call #8'},
    "fighter_input_mask@ok": {'byte', 'call #7', 'call #8', 'eax'},
    "fighter_input_mask@face": {'byte', 'call #7', 'call #8', 'eax'},
    "fighter_input_mask@eq": {'eax'},
    "fighter_input_mask@bl": {'call #7'},
    "fighter_input_mask@arm": {'call #7'},
    "fighter_input_mask@s54": {'byte', 'call #7 memory', 'call #8 memory'},
    "fighter_input_mask@call": {'byte', 'call #7', 'call #8'},
    "fight_command_map@a": {'call #0'},
    "fight_command_map@b": {'byte', 'call #0', 'call #1'},
    "fight_command_map@r": {'byte'},
    "fight_command_map@ov": {'byte'},
    "fight_command_map@bs": {'byte'},
    "fight_command_map@os": {'byte'},
    "fight_command_map@b1": {'byte'},
    "fight_command_map@rd": {'call #1', 'call #2'},
    "fight_command_map@c": {'call #2'},
    "pset_write@pool": {'byte'},
    "pset_write@x": {'byte'},
    "pset_write@lay": {'byte'},
    "pset_write@vis": {'byte'},
    "pset_write@ext": {'byte'},
    "pset_write@dead": {'call #1'},
    "pset_write@call": {'call #0', 'call #1'},
}
```

The exact case set that alone catches each mutant (measured on the prototype; the record's copy of
the test's table):

```
        ("fighter_4f434@sel", ['f0', 'f3', 'f5', 'f7']),
        ("fighter_4f434@scale", ['f0', 'f7']),
        ("fighter_4f434@thr", ['f8']),
        ("fighter_4f434@dx", ['f1', 'f4', 'f6']),
        ("fighter_4f434@delta", ['f0', 'f3', 'f5', 'f7']),
        ("fighter_4f434@call", ['f0', 'f3', 'f5', 'f7']),
        ("fighter_385b0@af8", ['b0', 'b1', 'b2', 'b3', 'b4']),
        ("fighter_385b0@arm", ['b2', 'b3', 'b4']),
        ("fighter_385b0@five", ['b0', 'b1', 'b2', 'b3', 'b4']),
        ("fighter_385b0@anim", ['b0', 'b1', 'b2', 'b4']),
        ("fighter_385b0@tail", ['b0', 'b1', 'b2', 'b4']),
        ("fighter_385b0@call", ['b3']),
        ("fighter_3ae9c@s53", ['e0', 'e1', 'e2', 'e3', 'e4', 'e5', 'e6', 'e7', 'e8']),
        ("fighter_3ae9c@g", ['e1', 'e2', 'e3', 'e4', 'e5', 'e6', 'e7', 'e8']),
        ("fighter_3ae9c@sign", ['e1', 'e2', 'e3', 'e5', 'e6', 'e7', 'e8']),
        ("fighter_3ae9c@tab", ['e2', 'e3', 'e4', 'e5', 'e6', 'e7', 'e8']),
        ("fighter_3ae9c@u44", ['e2', 'e3', 'e8']),
        ("fighter_3ae9c@k", ['e4', 'e5', 'e6', 'e7']),
        ("fighter_3ae9c@bit", ['e2', 'e3', 'e8']),
        ("fighter_3b080@gate", ['b0']),
        ("fighter_3b080@neg", ['b1', 'b2', 'b3', 'b4']),
        ("fighter_3b080@mirror", ['b3', 'b4']),
        ("fighter_3b080@p4", ['b2']),
        ("fighter_3b080@c148", ['b1', 'b2', 'b3', 'b4']),
        ("fighter_3b080@o43", ['b1', 'b2', 'b3', 'b4']),
        ("fighter_state_36bc8@other", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5']),
        ("fighter_state_36bc8@cond", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5']),
        ("fighter_state_36bc8@anim", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5']),
        ("fighter_state_36bc8@s5d", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5']),
        ("fighter_state_36bc8@bit", ['c0', 'c1', 'c5']),
        ("fighter_state_36bc8@tail", ['c2']),
        ("fighter_379c4@fe", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
        ("fighter_379c4@s57", ['k0', 'k1']),
        ("fighter_379c4@b41", ['k2']),
        ("fighter_379c4@e8", ['k3', 'k4', 'k5']),
        ("fighter_379c4@cb", ['k4']),
        ("fighter_379c4@fc", ['k5']),
        ("fighter_379c4@tab", ['k5']),
        ("fighter_3ad98@voice", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("fighter_3ad98@off", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("fighter_3ad98@tab", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("fighter_3ad98@zero", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("fighter_3ad98@call", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("fighter_3ad98@clamp", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("fighter_3ad98@d", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("fighter_3ad98@bit", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("fighter_input_mask@loop", ['i0', 'i1', 'i2', 'i3', 'i4', 'i5', 'i6', 'i7', 'i8']),
        ("fighter_input_mask@word", ['i8']),
        ("fighter_input_mask@ok", ['i1']),
        ("fighter_input_mask@face", ['i0', 'i2', 'i6', 'i7', 'i8']),
        ("fighter_input_mask@eq", ['i3', 'i4', 'i5']),
        ("fighter_input_mask@bl", ['i4', 'i5']),
        ("fighter_input_mask@arm", ['i7']),
        ("fighter_input_mask@s54", ['i2', 'i3', 'i4', 'i6']),
        ("fighter_input_mask@call", ['i0', 'i2', 'i3', 'i4', 'i5', 'i6', 'i8']),
        ("fight_command_map@a", ['m0']),
        ("fight_command_map@b", ['m1', 'm8']),
        ("fight_command_map@r", ['m2']),
        ("fight_command_map@ov", ['m11']),
        ("fight_command_map@bs", ['m5', 'm6']),
        ("fight_command_map@os", ['m10', 'm11', 'm3', 'm4', 'm9']),
        ("fight_command_map@b1", ['m6']),
        ("fight_command_map@rd", ['m7']),
        ("fight_command_map@c", ['m7']),
        ("pset_write@pool", ['p1', 'p10', 'p7', 'p9']),
        ("pset_write@x", ['p1', 'p10', 'p7', 'p9']),
        ("pset_write@lay", ['p8']),
        ("pset_write@vis", ['p4']),
        ("pset_write@ext", ['p5']),
        ("pset_write@dead", ['p6']),
        ("pset_write@call", ['p0', 'p4', 'p5', 'p6']),
```

The mutants with calls have their catch pinned to the call list or the memory at a call:
`pset_write` at the `0x2A690`/`0x2B150` stubs, `fighter_input_mask` at the seven `0x46460` reads and
the `0x18B04`/`0x1A7CC` stubs, `fighter_state_36bc8` at the `0x38BC8`/`0x2BC30`/`0x188AC` stubs,
`fight_command_map` at the `0x5D7DC`/`0x3BDB0`/`0x3BDDC` stubs, `fighter_3ad98` at the
`0x2AE14`/`0x2BC30`/`0x392A0` stubs.

## §C3d.4 Counters, measured

| state | diff-verify counter | E2 |
|---|---|---|
| base `0bda4fa` | `251/251 functions VERIFIED; 874/874 mutants detected; 1 named gaps; 178/195 rows with callees closed (56 have none)` | `233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted 30`; voice `0 / 115 / 19` |
| C3d prototype (ten rows) | `261/261 functions VERIFIED; 945/945 mutants detected; 1 named gaps; 181/205 rows with callees closed (56 have none)` | byte-identical |

The arithmetic: +10 functions, +71 mutants (6+6+7+6+6+7+8+9+9+7), rows with callees 195 -> 205
(all ten have callees), no-callee rows 56 (unchanged), closed 178 -> 181 (+3). The three new closed
rows: `fighter_385b0` and `fighter_3ae9c` (no missing callees) and the dependent
`fighter_command_dispatch` (0x3B298: `0x1A734`/`0x1AB5C`/`0x3B134` all VERIFIED). The batch's own
heavy rows stay open on the tail: `fighter_3ad98` on `0x392A0`, `fighter_4f434` on `0x46534`,
`fighter_3b080` on `0x3B038`, `fighter_state_36bc8` on `0x38BC8`/`0x38BB0`, `fighter_input_mask` on
`0x1A7CC`/`0x1AB10`, `fight_command_map` on `0x3BDB0`/`0x3BDDC`/the allows, `fighter_379c4` on
`0x37178`/the callback allows, `pset_write` on `0x2A690`.

`make entry-triage` is byte-identical (`targets 233 unported, 262 ported; supplement 131 (3
unported, 0 stale); untrusted entries 30`; voice `0 / 115 / 19`); `python3
tools/port_progress.py` stays `771 1203 64` / `731 731 100`; `PR_ORACLE_REQUIRED=1 ./build/run_tests`
prints `all checks passed`; `python3 -m unittest tools.tests.test_diff_verify` is `107 tests OK`
(the extended exact sets).

## §C3d.5 Decisions, named gaps and limits

**No decision is left to the user.** The `0x3AD98` correction (§C3d.2) is the batch's one
raw-over-port change; the three unrrowed type rows and the tail are C3e with measured evidence
(§C3d.7).

Named gaps and limits:

- **`pset_write`'s `0x2A9CE` is a named unhit block**: the child arm's layer is an 8-bit add
  (`0x2A9B5`/`0x2A9B8`), so the `> 0xff` clamp there cannot fire; the port's u16 arithmetic agrees
  on every reachable input.
- **The `0x3AD98` correction's neutrality.** The function's only in-port callers (`fighter_reaction`
  0x3B714 through the `0x3AD98` branch) are reached only when the command dispatch returns non-zero;
  no driver path reaches it at this base, and Task 4's full `make verify` is the oracle/gp evidence.
- **The allows stay open by design**: `0x1AB10`, `0x33A68`, `0x38BB0`, `0x3A280`, `0x3B038`,
  `0x46534` (and from earlier batches `0x33950`/`0x339AC`/`0x33A10`) run on both sides but have no
  own row, so the rows that call them cannot close until C3e rows them; the same for the stubbed
  missing callees.
- **`fighter_379c4`'s callback path** uses the named non-row `0x5D812` (`xor eax,eax; ret`) through
  the port's `fn_resolve` NULL guard and the registered `0x45D14` (returns 1) as the two arms: both
  sides take the same branch (allow, unrecorded). `0x5D812` is already a named non-row.
- **The four dependents' post-C3d status**: `fighter_command_dispatch` (0x3B298) closes;
  `fighter_36870` is open only on `0x39040`; `fighter_39834` only on `0x392A0`; `fighter_reaction`
  only on `0x3AAFC`.
- **A stub call can be recorded even when the stub is in an allowed callee's body** (`fighter_379c4`
  k4: the allowed `0x45D14` calls `0x2BC30`, which is in the row's call set): the original and port
  records agree because the seam intercepts on both sides.
- E3's, P1-P8's and C1-C3c's limits stand: seeds are hand pokes; the memory at a call is `mem[]`
  only; the callee column is one level deep.

## §C3d.6 What the planner ran

- The baseline on the clean `0bda4fa` worktree: `make diff-verify` -> the §C3d.4 base counter and
  table; `make entry-triage` -> `233/262`; `python3 tools/port_progress.py` -> `771 1203 64` /
  `731 731 100`.
- The prototype, in this worktree: the sixteen seams and three exports first (`--self-check` still
  at the base counter), then the ten rows family by family. Each row was measured with
  `python3 tools/diff_verify.py --function NAME --self-check` until VERIFIED with every mutant
  detected, then the full `python3 tools/diff_verify.py --self-check` (§C3d.4 counter), the
  `python3 -m unittest tools.tests.test_diff_verify` suite (107 tests OK, including the new
  `test_each_c3d_mutant_is_caught_by_what_it_breaks` and the extended exact-set, clobber and counter
  assertions), `make entry-triage` (byte-identical), `PR_ORACLE_REQUIRED=1 ./build/run_tests` (all
  checks passed) and a `symbols.h` regeneration (byte-identical). Then `git checkout -- port tools`,
  leaving only this record and the plan.
- The prototype's own corrections (one raw-over-port, §C3d.2 correction 1; the rest fixture/draft
  fixes): §C3d.2 corrections 2-4.
- The full `make verify` (~25 min) was not run by the planner: the brief's task-scoped gates are
  the ones above. The `port/src` changes are the seams/exports and the `0x3AD98` correction; the
  executor runs the full gate in Task 4.
- The prototype's diff (1564 lines, 1270 insertions over eight files) is embedded in the plan.

## §C3d.7 The C3e deferral (the three remaining type rows and the tail, with evidence)

This session measured 10 of the 13 type rows §C3c.7 defers. The three left are deferred on session
budget (not a correctness judgement): every one is ported (a `/* 0xADDR` header in `port/src`), its
original is disassembled, and **its seam already exists** (an earlier row stubs it), so C3e needs
only Specs, bindings and mutants — no `port/src` change for the entries. Sizes are
`port/decomp/prage.functions.csv` bytes; insn counts are capstone scans; the callee columns are the
same scans, with a callee "rowed" when the base table (plus this batch's ten) has a row for it:

| row | entry | size / insns | rowed callees | missing callees |
|---|---|---|---|---|
| `fighter_39040` | `0x39040` | 566 / 151 | `0x41310`, `0x5D7DC`, `0x2C3FC` | `0x38D90` `0x38FEC` `0x4F944` |
| `fighter_392a0` | `0x392A0` | 907 / 281 | `0x33A68` (as allow), `0x41310` | `0x46190` `0x36E78` `0x33A68` (no row) |
| `fighter_reaction_apply` | `0x3AAFC` | 667 / 182 | `0x33A10`, `0x18B04`, `0x3AFC4`, `0x39834`, `0x2C3FC`, `0x39F40`, `0x3AA54` | `0x3A280` `0x3A0FC` `0x36D20` `0x3A650` `0x3A79C` `0x3A504` `0x3A8E8` |

Their dependencies on this batch: `0x3AD98` (row VERIFIED here) stays open only on `0x392A0`;
`fighter_36870` only on `0x39040`; `fighter_reaction` only on `0x3AAFC`; `fighter_39834` only on
`0x392A0`. The 16 seams this batch added cover all but four of the three rows' missing callees:
`0x38D90`, `0x38FEC`, `0x46190`, `0x36E78`, `0x3A0FC`, `0x36D20`, the four pose setters, `0x3BDB0`,
`0x3BDDC`, `0x1A7CC`, `0x2A690`, `0x38BC8`, `0x37178` now have seams; `0x4F944`, `0x3A280`,
`0x33A68`, `0x46534`, `0x3B038`, `0x1AB10`, `0x38BB0` are leaf/allow candidates with no seam. So
rowing the three in C3e is the same shape as this batch's work.

**The tail the ten rows' callees name.** The 23 frontier addresses §C3c.7 lists (all ported, none
rowed; `0x36BC8` and the others in the ten rows are rowed by this batch), with sizes/insns:

| address | size / insns | | address | size / insns |
|---|---|---|---|---|
| `0x1AB10` | 74 / 33 | | `0x36E78` | 152 / 51 |
| `0x1A7CC` | 296 / 84 | | `0x3A280` | 32 / 12 |
| `0x2A690` | 397 / 125 | | `0x3A0FC` | 355 / 112 |
| `0x2A620` | (through `0x2A690`) | | `0x36D20` | 120 / 42 |
| `0x38BC8` | 34 / 14 | | `0x3A504` | 116 / 30 |
| `0x38BB0` | 24 / 12 | | `0x3A650` | 116 / 30 |
| `0x37178` | 746 / 243 | | `0x3A79C` | 116 / 30 |
| `0x38D90` | 318 / 87 | | `0x3A8E8` | 116 / 30 |
| `0x38FEC` | 81 / 32 | | `0x3B038` | 72 / 34 |
| `0x4F944` | 56 / 16 | | `0x3BDB0` | 43 / 18 |
| `0x33A68` | 97 / 32 | | `0x3BDDC` | 401 / 127 |
| `0x46190` | 33 / 13 | | `0x46534` | 93 / 29 |

**Their second wave** (the unrowed callees those 23 name; capstone scans against the base table
plus this batch): 11 distinct — `0x1A6AC` (134/46, through `0x1A7CC`), `0x1C500` (37/13),
`0x2CAA8` (16/4), `0x2D974` (149/62), `0x2F20C` (115/42), `0x2F4BC` (20/6), `0x2F4D0` (61/23),
`0x38C5C` (125/41), `0x38ED0` (284/92), `0x3CF38` (203/67), `0x4649C` (101/45). **Their third wave**
(the unrowed callees of those 11): 12 more — `0x2EFD4`, `0x2F0F0`, `0x2F198`, `0x2F280`, `0x2F314`,
`0x2F830`, `0x32BAC`, `0x3C6A8`, `0x3CD44`, `0x3CE58`, `0x474E4` and `0x33A68` (already in the first
list). So the tail is a **chain**, not a flat set: 23 addresses surface 11, which surface 12, and
the scan stops only because the last list was not expanded further (a fourth wave is not excluded).

**The C3e verdict.** C3e is required, with this exact list: **the three type rows** (`0x39040`,
`0x392A0`, `0x3AAFC`) **plus the 23 tail addresses**, to be rowed dependency-first (`0x392A0` before
`0x3AD98` can close; `0x39040` before `fighter_36870`; `0x3AAFC` before `fighter_reaction`), and
then the second and third waves re-measured as those land. Closing the frontier in C3d was
considered and rejected on measurement: the 23 alone are 4,244 bytes / ~1,216 instructions across
23 functions, the three rows add 2,140 bytes / 614 instructions, and the chain's second/third waves
are already 23 more addresses — one session cannot measure that under this batch's standard.

**What C3e must not redo.** The seams for the 23 are largely in place (this batch added 16; the
allows need none), and the three rows' entries need none; C3e's Task 1 can reuse this record's
per-address modes (the "mode in this batch" column, §C3d.1) for the callees this batch stubbed.
