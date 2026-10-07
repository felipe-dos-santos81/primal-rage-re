# Reverse completion C3e: the frontier rows, part 5 (record)

**Scope.** Track P's eighth verification-only batch (the C3d record §C3d.7's C3e deferral): the
**three remaining type rows** — `0x39040` (`fighter_39040`), `0x392A0` (`fighter_392a0`) and
`0x3AAFC` (`fighter_reaction_apply`) — plus **fourteen tail leaves** that close or advance the
chain: `0x3A280` (`fighter_3a280`), `0x4F944` (`fighter_4f944`), `0x33A68`
(`fighter_ctx_rec_swap`), `0x36E78` (`fighter_36e78`), `0x2CAA8` (`config_not_free_play`),
`0x2D974` (`config_field_get`), `0x468D8` (`ai_pred_468d8`), `0x46190` (`fighter_46190`),
`0x36D20` (`fighter_36d20`), the four pose setters `0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8` and
`0x3A0FC` (`fighter_3a0fc`) — **seventeen rows, 86 mutants, all detected.** Two raw-over-port
corrections (`0x3AAFC`'s second anim triple, `0x3A280`'s u8 cast). **The tail verdict** (§C3e.7):
after this batch the remaining frontier is exactly the twelve first-wave addresses the other
frontier rows wait on plus the `0x38D90`/`0x38FEC` trees (the C3f list with evidence). Plan:
`2026-10-05-reverse-c3e-frontier-rows-5.md`. Recipe: E3 record §E3.10; checklist:
`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**Status of the numbers.** Measured by the planner on 2026-10-07 on `reverse-c3e` at the base
`main` `c28e522` (= C3d merged), in this worktree: the baseline gates and a full prototype of the
seventeen rows (every gate run, then reverted). The image is `build/diffrun --exe
data/game/C/PRAGE.EXE --image-out FILE`, sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's,
E3's, P1-P8's, C1-C3d's). Every address below is capstone 5.0.7 over that image (fixups applied);
`unicorn` 2.1.4 runs the original side. Ghidra was not consulted. The prototype diff (1797 lines,
1542 insertions over six files) is embedded in the plan; the prototype was reverted
(`git checkout -- port tools`), leaving only this record and the plan.

---

## §C3e.1 The members, re-derived from the raw

The C3d record §C3d.7 defers the three type rows and the tail. This session measured **seventeen**
of that list: the three rows and the fourteen leaves whose rows the three rows' callee columns
need (or whose trees they start). The members were re-checked against the raw's call scans
(capstone over the image) and the final base table:

| row | entry | size / insns | the raw's direct callees (mode in this batch) |
|---|---|---|---|
| `fighter_39040` | `0x39040` | 566 / 151 | `0x38D90` stub, `0x38FEC` stub, `0x41310` stub (rowed), `0x4F944` allow, `0x5D7DC` stub (rowed), `0x2C3FC` stub (rowed) |
| `fighter_392a0` | `0x392A0` | 907 / 281 | `0x33A68` allow, `0x46190` stub, `0x41310` stub (rowed), `0x36E78` stub, `0x5D7DC` stub (rowed) |
| `fighter_reaction_apply` | `0x3AAFC` | 667 / 182 | `0x33A10` allow (rowed), `0x18B04` stub (rowed), `0x3AFC4` allow (rowed), `0x39834` stub (rowed), `0x3A280` allow, `0x2C3FC` stub (rowed), `0x3A0FC` stub, `0x3AA54` stub (rowed), `0x36D20` stub, `0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8` stubs, `0x39F40` stub (rowed) |
| `fighter_3a280` | `0x3A280` | 32 / 12 | — |
| `fighter_4f944` | `0x4F944` | 56 / 16 | — |
| `fighter_ctx_rec_swap` | `0x33A68` | 97 / 32 | — |
| `fighter_36e78` | `0x36E78` | 152 / 51 | `0x29BC8` stub (rowed) |
| `config_not_free_play` | `0x2CAA8` | 16 / 4 | — |
| `ai_pred_468d8` | `0x468D8` | 96 / 34 | `0x33A10` allow (rowed) |
| `config_field_get` | `0x2D974` | 149 / 62 | — |
| `fighter_46190` | `0x46190` | 33 / 13 | `0x2D974` real (rowed here), `0x2CAA8` real (rowed here) |
| `fighter_36d20` | `0x36D20` | 120 / 42 | `0x468D8` stub (rowed here), `0x36BC8` stub (rowed), `0x2BC30` stub (rowed), `0x36CE4` (inlined in the raw: the port calls it, unrecorded) |
| `fighter_pose_3a504` | `0x3A504` | 116 / 30 | `0x33A10` allow (rowed) |
| `fighter_pose_3a650` | `0x3A650` | 116 / 30 | `0x33A10` allow (rowed) |
| `fighter_pose_3a79c` | `0x3A79C` | 116 / 30 | `0x33A10` allow (rowed) |
| `fighter_pose_3a8e8` | `0x3A8E8` | 116 / 30 | `0x33A10` allow (rowed) |
| `fighter_3a0fc` | `0x3A0FC` | 355 / 112 | `0x33950` allow (rowed), `0x3AFC4` allow (rowed), `0x2AE14` stub (rowed), `0x2BC30` stub (rowed) |

The measured rows (cases/blocks/mutants measured; `EAX mask` from the Spec):

| row | entry | cases | blocks | mutants | EAX mask | named unhit |
|---|---|---|---|---|---|---|
| `fighter_39040` | 0x39040 | 17 | 33/33 | 11/11 | 0 | — |
| `fighter_392a0` | 0x392A0 | 18 | 42/42 | 11/11 | 0 | — |
| `fighter_reaction_apply` | 0x3AAFC | 13 | 28/28 | 12/12 | 0 | — |
| `fighter_3a280` | 0x3A280 | 10 | 6/6 | 2/2 | 0xFF | — |
| `fighter_4f944` | 0x4F944 | 7 | 3/3 | 4/4 | 0 | — |
| `fighter_ctx_rec_swap` | 0x33A68 | 4 | 1/1 | 3/3 | 0 | — |
| `fighter_36e78` | 0x36E78 | 5 | 5/5 | 4/4 | 0 | — |
| `config_not_free_play` | 0x2CAA8 | 3 | 1/1 | 3/3 | 0xFF | — |
| `ai_pred_468d8` | 0x468D8 | 7 | 8/8 | 4/4 | 0xFF | — |
| `config_field_get` | 0x2D974 | 9 | 12/12 | 5/5 | 0xFFFFFFFF | — |
| `fighter_46190` | 0x46190 | 5 | 5/5 | 3/3 | 0xFF | — |
| `fighter_36d20` | 0x36D20 | 6 | 8/8 | 5/5 | 0xFF | — |
| `fighter_pose_3a504` | 0x3A504 | 2 | 1/1 | 3/3 | 0 | — |
| `fighter_pose_3a650` | 0x3A650 | 2 | 1/1 | 3/3 | 0 | — |
| `fighter_pose_3a79c` | 0x3A79C | 2 | 1/1 | 3/3 | 0 | — |
| `fighter_pose_3a8e8` | 0x3A8E8 | 2 | 1/1 | 3/3 | 0 | — |
| `fighter_3a0fc` | 0x3A0FC | 9 | 28/28 | 7/7 | 0 | — |

No row reads outside the image (the `0x3A0FC` actor-spawn stubs use a scratch actor at `0x10A480`,
so their `+0x59` stores land inside the image).

## §C3e.2 The rows: cases, seams, fixtures and the corrections

Each row is a `Spec` in `tools/diff_verify.py` (`C3E_SPECS` and its `c3e_*` fixture helpers), a
binding `b_*` and mutants `m_*` in `port/tests/diff_runner.c` (`k_bindings`), and the exact-set
extensions in `tools/tests/test_diff_verify.py` (`C3E_MASKS`, `C3E_KINDS`, the case-set test, the
clobber table, the counter line). The callee column of the full run's table:

| row | entry | cases/blocks/mutants | callees (each by its own check) |
|---|---|---|---|
| `fighter_39040` | 0x39040 | 17 / 33/33 / 11 | `2C3FC` stub VERIFIED, `38D90` stub **unverified**, `38FEC` stub **unverified**, `41310` stub VERIFIED, `4F944` allow VERIFIED, `5D7DC` stub VERIFIED |
| `fighter_392a0` | 0x392A0 | 18 / 42/42 / 11 | `33A68` allow VERIFIED, `36E78` stub VERIFIED, `41310` stub VERIFIED, `46190` stub VERIFIED, `5D7DC` stub VERIFIED |
| `fighter_reaction_apply` | 0x3AAFC | 13 / 28/28 / 12 | `18B04`, `2C3FC`, `36D20`, `39834`, `39F40`, `3A0FC`, `3A504`/`3A650`/`3A79C`/`3A8E8`, `3AA54` stubs VERIFIED; `33A10`, `3A280`, `3AFC4` allows VERIFIED |
| `fighter_3a280` | 0x3A280 | 10 / 6/6 / 2 | — |
| `fighter_4f944` | 0x4F944 | 7 / 3/3 / 4 | — |
| `fighter_ctx_rec_swap` | 0x33A68 | 4 / 1/1 / 3 | — |
| `fighter_36e78` | 0x36E78 | 5 / 5/5 / 4 | `29BC8` stub VERIFIED |
| `config_not_free_play` | 0x2CAA8 | 3 / 1/1 / 3 | — |
| `ai_pred_468d8` | 0x468D8 | 7 / 8/8 / 4 | `33A10` allow VERIFIED |
| `config_field_get` | 0x2D974 | 9 / 12/12 / 5 | — |
| `fighter_46190` | 0x46190 | 5 / 5/5 / 3 | `2CAA8` real VERIFIED, `2D974` real VERIFIED |
| `fighter_36d20` | 0x36D20 | 6 / 8/8 / 5 | `2BC30` stub VERIFIED, `36BC8` stub VERIFIED, `468D8` stub VERIFIED |
| `fighter_pose_3a504`/`3a650`/`3a79c`/`3a8e8` | 0x3A504/650/79C/8E8 | 2 / 1/1 / 3 each | `33A10` allow VERIFIED each |
| `fighter_3a0fc` | 0x3A0FC | 9 / 28/28 / 7 | `2AE14` stub VERIFIED, `2BC30` stub VERIFIED, `33950` allow VERIFIED, `3AFC4` allow VERIFIED |

**Seams and exports** (each seam a first statement; `E.callee_clobbers` re-derives every declared
clobber; a stub's call is declared with those clobbers in `C3E_SPECS`):

| function | address | change | conformance |
|---|---|---|---|
| `fighter_ctx_rec_swap` | `0x33A68` | loses `static`; declared in `fighter.h` (its own row's binding writes into `mem[]` at EAX) | no clobbers |
| `fighter_46190` | `0x46190` | loses `static`; declared | no clobbers |
| `fighter_36e78` | `0x36E78` | loses `static`; declared | no clobbers |
| `fighter_36d20` | `0x36D20` | loses `static`; declared | no clobbers |
| `fighter_3a0fc` | `0x3A0FC` | loses `static`; declared | clobbers `ebp` |
| `fighter_pose_3a504`/`3a650`/`3a79c`/`3a8e8` | `0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8` | each loses `static`; declared | clobbers `edx` each |
| `config_field_get` | `0x2D974` | gains `PR_SEAM_RET` (so `fighter_46190`'s mode-real call is recorded on the port side) | no clobbers |
| `config_not_free_play` | `0x2CAA8` | gains `PR_SEAM_RET0` (the same) | no clobbers |

The nine exports the mutant cores call; no other `port/src` surface changes. The three rows' own
entries needed no new seams (C3d had added all of theirs).

**Corrections during prototyping (each recorded here with its evidence):**

1. **`fighter_reaction_apply` was a raw-over-port correction (the batch's heavy one).** The port's
   second anim triple passed the reaction byte as its frame; the raw reaches the 0x3AB58 call with
   **EDX = DSB(self+0x5F)** — the byte the 0x3AB3F `mov dl,[edx+0x5f]` loaded and the 0x3AB48
   `cmp edx,0xff` tested — and never reloads EDX before the call (`0x3AB50 lea ebx,[esp+0x24]`;
   `0x3AB54 mov eax,[esp+4]`; `0x3AB58 call 0x3afc4`). Evidence: case `a0` (reaction 1,
   `self+0x5F = 0`) read the port's anim2 word at `0xA6730` (c = 1, `0x44` -> the uvar2 `& 0x40`
   arm: `0x18B04` + `0x39F40` and `self+0x90 = 5`) while the original read c = 0 (`0xA672A` -> the
   `ecx & 4` arm: `0x3A650(0, 0)` and no `+0x90` store). With the correction the row's cases and
   all 12 mutants VERIFIED. The port now computes
   `fighter_anim_triple(anim2, ctx[1], (s32)DSB(self + 0x5Fu))`.
2. **`fighter_3a280` was a raw-over-port correction.** The port cast `code` to `u8` before its two
   range tests; the raw compares the full EAX (`0x3A280 cmp eax,0x20` / `0x3A3F` and the
   `sub eax,0x10; cmp eax,7; ja` after it). Evidence: case `r9` (EAX = `0x100020`) reads the
   original's AL = 0 while the port's was 1; with the cast removed every case (including `r8`,
   `0x100010`) and both mutants VERIFIED. The two callers mask with `and eax,0xff`, so no behavior
   change outside the row's inputs.
3. **Fixture/draft corrections (no further port change):**
   - `0x39040`'s fixture `q` is the pre-divisor numerator (`DSD(slot+0x3C)`), not the quotient:
     the first draft's `g2`/`g3` used numerators below the divisor, so the clamp block `0x390B4`
     stayed unhit (32/33); with `q=0x30`/`0x40` the row reads 33/33.
   - The `0x3AAFC` correction moved the second/third anim words: with the raw's frame
     (`DSB(self+0x5F)`) the second triple's c2 = `(char[side]<<6) + 0x5F-byte` and the uvar2 word
     sits at `0xA672A + 6*c2`; the cases now pin both the ecx word (`0xA672A + 6*c1`) and the uvar2
     word explicitly, and `a2`'s self char is `ch0` (the slot's own +0x7A).
   - The pose setters' cases set EBX = `DSB(slot+0x52)`: the raw stores BX (the caller's register)
     into the setter's glob_b, and every 0x3A5xx-family call site loads EBX as the self slot's
     +0x52 (the port's `PORT:` note; the port reads the slot byte, so the row's domain is that
     call convention — before the EBX seed the p1/p0 cases mismatched on the gb word).
   - `0x3A0FC`'s cases `f2`/`f3` set the `0x2AE14` stub EAX to `0x10A480`: with the default 0 the
     second spawn's `+0x59 = 3` store lands at address `0x59`, outside the image (the driver's
     `reads outside the image: 0x59+1` note); the scratch actor keeps it inside.
   - `0x36D20`'s spec leaves `0x36CE4` out of the call set: the raw inlines 0x36CE4's body
     (`0x36D42..0x36D7D`), the port factors it into a call; the stub seam intercepts it when it is
     in a call set, so the row keeps it unrecorded and only the `0x2BC30` animation call is
     compared (the port's call is recorded neither way).
   - `fighter_46190`'s first draft declared its two config callees `mode="stub"` with no seams on
     the port side; `q4` read `call #0: original 0x2D974(0x29), port none`. The seams added in
     correction 1's table make the port's calls recordable; the spec now runs them `mode="real"`.

**Fixtures.** The rows reuse the C3d scratch records (`0x10AF00`/`0x10AF40`), the slot pointer
pair and both slots (`c3d_slot_pokes`), scratch records at `0x10A100`-`0x10A480` and the image's
own tables (`0xA6728`, `0xA3528`, `0xDE114`, `0xBEBCA`, `0xBECF8`, `0x105DE1`, `0x105DAF`, the
`0x107A80` stun table). No fixture pokes more than 16 regions per case (the driver's per-case cap);
the contiguous slot/global ranges are single pokes.

## §C3e.3 The mutants

86 mutants, all detected (`python3 tools/diff_verify.py --self-check`: `1031/1031`); what alone
catches each is pinned by `test_each_c3e_mutant_is_caught_by_what_it_breaks` (`C3E_KINDS` plus the
measured case-set table; the exact dicts are the ones committed in `tools/tests/test_diff_verify.py`).
The kinds (measured; the record's copy of the test's `C3E_KINDS`):

```
C3E_KINDS = {
    "ai_pred_468d8@byte": {'eax'},
    "ai_pred_468d8@hand": {'eax'},
    "ai_pred_468d8@low": {'eax'},
    "ai_pred_468d8@side": {'eax'},
    "config_field_get@field": {'eax'},
    "config_field_get@hi": {'eax'},
    "config_field_get@odd": {'eax'},
    "config_field_get@trail": {'eax'},
    "config_field_get@width": {'eax'},
    "config_not_free_play@byte": {'eax'},
    "config_not_free_play@eq": {'eax'},
    "config_not_free_play@zero": {'eax'},
    "fighter_36d20@anim": {'call #1'},
    "fighter_36d20@pred": {'byte', 'call #1', 'eax'},
    "fighter_36d20@rec": {'byte', 'call #1 memory'},
    "fighter_36d20@ret": {'eax'},
    "fighter_36d20@s54": {'byte', 'call #1', 'call #1 memory'},
    "fighter_36e78@f40": {'byte', 'call #0 memory'},
    "fighter_36e78@f5b": {'byte', 'call #0'},
    "fighter_36e78@other": {'byte', 'call #0'},
    "fighter_36e78@pal": {'call #0'},
    "fighter_39040@a23": {'byte', 'call #3', 'call #4'},
    "fighter_39040@a41": {'byte', 'call #3'},
    "fighter_39040@b4": {'byte', 'call #0 memory', 'call #1 memory', 'call #2 memory', 'call #3 memory', 'call #4 memory'},
    "fighter_39040@clear": {'byte'},
    "fighter_39040@delta": {'call #2'},
    "fighter_39040@gate": {'byte', 'call #0', 'call #1'},
    "fighter_39040@id": {'call #4'},
    "fighter_39040@inc": {'byte', 'call #2 memory', 'call #3 memory', 'call #4 memory'},
    "fighter_39040@lim": {'byte', 'call #0 memory', 'call #1 memory', 'call #2 memory', 'call #3 memory', 'call #4 memory'},
    "fighter_39040@rng2": {'call #4'},
    "fighter_39040@voice": {'call #4'},
    "fighter_392a0@a": {'byte', 'call #1 memory'},
    "fighter_392a0@b": {'byte', 'call #2 memory'},
    "fighter_392a0@clamp": {'byte'},
    "fighter_392a0@clamp44": {'byte'},
    "fighter_392a0@half": {'byte', 'call #1 memory'},
    "fighter_392a0@k": {'byte'},
    "fighter_392a0@mode2": {'byte', 'call #1', 'call #1 memory'},
    "fighter_392a0@so": {'byte', 'call #1'},
    "fighter_392a0@t": {'byte', 'call #2 memory'},
    "fighter_392a0@tail": {'call #2'},
    "fighter_392a0@zero": {'byte', 'call #1', 'call #2 memory'},
    "fighter_3a0fc@face": {'call #0'},
    "fighter_3a0fc@frame": {'byte', 'call #0', 'call #1', 'call #2', 'call #3'},
    "fighter_3a0fc@key": {'call #3'},
    "fighter_3a0fc@layer": {'call #0', 'call #2'},
    "fighter_3a0fc@off": {'call #0', 'call #2'},
    "fighter_3a0fc@s59": {'byte', 'call #1 memory', 'call #3 memory'},
    "fighter_3a0fc@stream": {'call #0', 'call #1', 'call #1 memory', 'call #2', 'call #3'},
    "fighter_3a280@cast": {'eax'},
    "fighter_3a280@lo": {'eax'},
    "fighter_46190@bit": {'call #1', 'eax'},
    "fighter_46190@field": {'call #0', 'call #1', 'eax'},
    "fighter_46190@notfree": {'call #1', 'eax'},
    "fighter_4f944@ae9": {'byte'},
    "fighter_4f944@cmp": {'byte'},
    "fighter_4f944@e8": {'byte'},
    "fighter_4f944@f0": {'byte'},
    "fighter_ctx_rec_swap@rec": {'byte'},
    "fighter_ctx_rec_swap@side": {'byte'},
    "fighter_ctx_rec_swap@stride": {'byte'},
    "fighter_pose_3a504@cb": {'byte'},
    "fighter_pose_3a504@glob": {'byte'},
    "fighter_pose_3a504@s52": {'byte'},
    "fighter_pose_3a650@cb": {'byte'},
    "fighter_pose_3a650@glob": {'byte'},
    "fighter_pose_3a650@s52": {'byte'},
    "fighter_pose_3a79c@cb": {'byte'},
    "fighter_pose_3a79c@glob": {'byte'},
    "fighter_pose_3a79c@s52": {'byte'},
    "fighter_pose_3a8e8@cb": {'byte'},
    "fighter_pose_3a8e8@glob": {'byte'},
    "fighter_pose_3a8e8@s52": {'byte'},
    "fighter_reaction_apply@call0fc": {'call #2', 'call #2 memory', 'call #3', 'call #3 memory', 'call #4'},
    "fighter_reaction_apply@e100": {'byte', 'call #3'},
    "fighter_reaction_apply@e200": {'byte', 'call #3', 'call #4'},
    "fighter_reaction_apply@e2000": {'byte', 'call #3'},
    "fighter_reaction_apply@inc": {'byte', 'call #3 memory', 'call #4 memory'},
    "fighter_reaction_apply@s42": {'byte', 'call #3 memory', 'call #4 memory'},
    "fighter_reaction_apply@s54": {'byte', 'call #3', 'call #4'},
    "fighter_reaction_apply@s5f": {'byte', 'call #3', 'call #4'},
    "fighter_reaction_apply@s6": {'byte', 'call #3', 'call #4'},
    "fighter_reaction_apply@st": {'call #3'},
    "fighter_reaction_apply@u2": {'byte', 'call #3', 'call #4'},
    "fighter_reaction_apply@voice": {'call #2', 'call #3', 'call #3 memory', 'call #4'},
}
```

The exact case set that alone catches each mutant (measured on the prototype; the record's copy of
the test's table):

```
        ("ai_pred_468d8@byte", ['p3', 'p4']),
        ("ai_pred_468d8@hand", ['p1', 'p6']),
        ("ai_pred_468d8@low", ['p2']),
        ("ai_pred_468d8@side", ['p1', 'p3', 'p4', 'p6']),
        ("config_field_get@field", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5', 'c7', 'c8']),
        ("config_field_get@hi", ['c7']),
        ("config_field_get@odd", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5', 'c7', 'c8']),
        ("config_field_get@trail", ['c5']),
        ("config_field_get@width", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5', 'c7', 'c8']),
        ("config_not_free_play@byte", ['f1', 'f2']),
        ("config_not_free_play@eq", ['f0', 'f1', 'f2']),
        ("config_not_free_play@zero", ['f0']),
        ("fighter_36d20@anim", ['d2']),
        ("fighter_36d20@pred", ['d0', 'd1', 'd2', 'd3', 'd4', 'd5']),
        ("fighter_36d20@rec", ['d4', 'd5']),
        ("fighter_36d20@ret", ['d1', 'd2', 'd3', 'd4', 'd5']),
        ("fighter_36d20@s54", ['d4', 'd5']),
        ("fighter_36e78@f40", ['b1', 'b2', 'b4']),
        ("fighter_36e78@f5b", ['b0', 'b3']),
        ("fighter_36e78@other", ['b1']),
        ("fighter_36e78@pal", ['b1']),
        ("fighter_39040@a23", ['g16', 'g6']),
        ("fighter_39040@a41", ['g7', 'g8']),
        ("fighter_39040@b4", ['g1', 'g10', 'g11', 'g12', 'g13', 'g14', 'g15', 'g16', 'g2', 'g3', 'g4', 'g5', 'g6', 'g7', 'g8', 'g9']),
        ("fighter_39040@clear", ['g0', 'g1', 'g10', 'g11', 'g12', 'g13', 'g14', 'g15', 'g16', 'g2', 'g3', 'g4', 'g5', 'g6', 'g7', 'g8', 'g9']),
        ("fighter_39040@delta", ['g10', 'g11', 'g12', 'g13', 'g14', 'g15', 'g16', 'g5', 'g6', 'g7', 'g8']),
        ("fighter_39040@gate", ['g0']),
        ("fighter_39040@id", ['g5']),
        ("fighter_39040@inc", ['g1', 'g10', 'g11', 'g12', 'g13', 'g14', 'g15', 'g16', 'g2', 'g3', 'g4', 'g5', 'g6', 'g7', 'g8', 'g9']),
        ("fighter_39040@lim", ['g13', 'g2', 'g3']),
        ("fighter_39040@rng2", ['g10', 'g9']),
        ("fighter_39040@voice", ['g10', 'g11', 'g12', 'g13', 'g5', 'g9']),
        ("fighter_392a0@a", ['m0']),
        ("fighter_392a0@b", ['m0', 'm1', 'm11', 'm13', 'm14', 'm16', 'm17', 'm2', 'm3', 'm4', 'm6', 'm7', 'm8']),
        ("fighter_392a0@clamp", ['m10']),
        ("fighter_392a0@clamp44", ['m15']),
        ("fighter_392a0@half", ['m11']),
        ("fighter_392a0@k", ['m12']),
        ("fighter_392a0@mode2", ['m2', 'm3', 'm5']),
        ("fighter_392a0@so", ['m2', 'm3', 'm5']),
        ("fighter_392a0@t", ['m0', 'm1', 'm10', 'm11', 'm12', 'm16', 'm17', 'm2', 'm3', 'm4', 'm6', 'm7', 'm8']),
        ("fighter_392a0@tail", ['m1', 'm9']),
        ("fighter_392a0@zero", ['m9']),
        ("fighter_3a0fc@face", ['f4', 'f5']),
        ("fighter_3a0fc@frame", ['f0', 'f1', 'f2', 'f3', 'f4', 'f5', 'f6']),
        ("fighter_3a0fc@key", ['f0', 'f1']),
        ("fighter_3a0fc@layer", ['f0', 'f1']),
        ("fighter_3a0fc@off", ['f0', 'f1', 'f2', 'f3', 'f4', 'f5', 'f6']),
        ("fighter_3a0fc@s59", ['f0', 'f1', 'f2', 'f3', 'f5', 'f6']),
        ("fighter_3a0fc@stream", ['f0', 'f1', 'f4', 'f5', 'f6']),
        ("fighter_3a280@cast", ['r8', 'r9']),
        ("fighter_3a280@lo", ['r1', 'r3']),
        ("fighter_46190@bit", ['q0', 'q1', 'q2', 'q3']),
        ("fighter_46190@field", ['q0', 'q1', 'q2', 'q3', 'q4']),
        ("fighter_46190@notfree", ['q0', 'q2', 'q3']),
        ("fighter_4f944@ae9", ['v0', 'v1', 'v2', 'v3', 'v4', 'v5', 'v6']),
        ("fighter_4f944@cmp", ['v4', 'v5']),
        ("fighter_4f944@e8", ['v0', 'v1', 'v2', 'v3', 'v4', 'v5', 'v6']),
        ("fighter_4f944@f0", ['v0', 'v1', 'v2', 'v3', 'v4', 'v5', 'v6']),
        ("fighter_ctx_rec_swap@rec", ['s0', 's1', 's3']),
        ("fighter_ctx_rec_swap@side", ['s0', 's1', 's2', 's3']),
        ("fighter_ctx_rec_swap@stride", ['s0', 's1', 's2', 's3']),
        ("fighter_pose_3a504@cb", ['p0', 'p1']),
        ("fighter_pose_3a504@glob", ['p0', 'p1']),
        ("fighter_pose_3a504@s52", ['p0', 'p1']),
        ("fighter_pose_3a650@cb", ['p0', 'p1']),
        ("fighter_pose_3a650@glob", ['p0', 'p1']),
        ("fighter_pose_3a650@s52", ['p0', 'p1']),
        ("fighter_pose_3a79c@cb", ['p0', 'p1']),
        ("fighter_pose_3a79c@glob", ['p0', 'p1']),
        ("fighter_pose_3a79c@s52", ['p0', 'p1']),
        ("fighter_pose_3a8e8@cb", ['p0', 'p1']),
        ("fighter_pose_3a8e8@glob", ['p0', 'p1']),
        ("fighter_pose_3a8e8@s52", ['p0', 'p1']),
        ("fighter_reaction_apply@call0fc", ['a0', 'a1', 'a10', 'a11', 'a12', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7', 'a8', 'a9']),
        ("fighter_reaction_apply@e100", ['a4']),
        ("fighter_reaction_apply@e200", ['a12', 'a4']),
        ("fighter_reaction_apply@e2000", ['a5']),
        ("fighter_reaction_apply@inc", ['a0', 'a1', 'a10', 'a11', 'a12', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7', 'a8', 'a9']),
        ("fighter_reaction_apply@s42", ['a0', 'a1', 'a10', 'a11', 'a12', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7', 'a8', 'a9']),
        ("fighter_reaction_apply@s54", ['a3']),
        ("fighter_reaction_apply@s5f", ['a0']),
        ("fighter_reaction_apply@s6", ['a2']),
        ("fighter_reaction_apply@st", ['a6', 'a7']),
        ("fighter_reaction_apply@u2", ['a0']),
        ("fighter_reaction_apply@voice", ['a11']),
```

The mutants whose catch includes a call or call-memory kind: the three rows at their stubs and
allows (`39040` at `38D90`/`38FEC`/`41310`/`5D7DC`/`2C3FC`, `392a0` at `46190`/`41310`/`36E78`,
`reaction_apply` at `18B04`/`39834`/`39F40`/`3A0FC`/`3AA54`/`36D20`/the pose setters),
`36e78` at `29BC8`, `46190` at `2D974`/`2CAA8`, `36d20` at `468D8`/`36BC8`/`2BC30`, `3a0fc` at
`2AE14`/`2BC30`. The pure rows' mutants (`3a280`, `4f944`, `ctx_rec_swap`, `config_not_free_play`,
`config_field_get`, the pose setters) are caught on EAX or bytes alone.

## §C3e.4 Counters, measured

| state | diff-verify counter | E2 |
|---|---|---|
| base `c28e522` | `261/261 functions VERIFIED; 945/945 mutants detected; 1 named gaps; 181/205 rows with callees closed (56 have none)` | `233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted 30`; voice `0 / 115 / 19` |
| C3e prototype (seventeen rows) | `278/278 functions VERIFIED; 1031/1031 mutants detected; 1 named gaps; 196/217 rows with callees closed (61 have none)` | byte-identical |

The arithmetic: +17 functions, +86 mutants (11+11+12+2+4+3+4+3+4+5+3+5+3+3+3+3+7), rows with
callees 205 -> 217 (twelve of the seventeen have callees; `3a280`, `4f944`, `ctx_rec_swap`,
`config_not_free_play` and `config_field_get` have none), no-callee 56 -> 61, closed 181 -> 196
(+15): the eleven new rows with callees that close (`392a0`, `reaction_apply`, `36e78`, `468d8`,
`46190`, `36d20`, the four pose setters, `3a0fc`) and the four dependents the three rows now
verify — `fighter_3ad98` (only on `0x392A0`), `fighter_39834` (only on `0x392A0`),
`fighter_reaction` (only on `0x3AAFC`) and `fighter_36870` (only on `0x39040`). `fighter_39040`
stays open on `0x38D90`/`0x38FEC` alone.

`make entry-triage` is byte-identical (no ported function, no `fn_register`); `python3 -m unittest
tools.tests.test_diff_verify` 109 tests OK (the record's 109: the only added method is
`test_each_c3e_mutant_is_caught_by_what_it_breaks`, plus the extended exact-set, clobber and
counter assertions); `PR_ORACLE_REQUIRED=1 ./build/run_tests` `all checks passed`; `symbols.h`
regeneration is byte-identical; `python3 tools/port_progress.py` stays `771 1203 64` / `731 731
100`; README untouched.

## §C3e.5 Named gaps and limits

- **The pose setters' row domain.** The raw stores the caller's BX into the setter's `glob_b`; the
  port reads the self slot's `+0x52` byte instead (its `PORT:` note: every call site loads EBX as
  that byte). The rows' cases set EBX = `DSB(slot+0x52)`, the call-site convention; the raw's
  BH (bits 8-15) is not compared.
- **`0x36D20`'s inlined `0x36CE4`.** The raw inlines 0x36CE4's body; the port calls the ported
  `fighter_36ce4`. The row keeps the call unrecorded (allow) so the comparison sees the same call
  list; the two are equivalent on the exercised cases, but the row does not compare the port's
  internal call.
- **`0x39040`'s inlined `0x38BB0`.** The raw inlines `0x38BB0`'s clear at the function's tail
  (`0x3925C shl ebx,6`; `0x39264..0x3926F` the 0x40-byte store loop over `0x107A80 + side*0x40`),
  and the port calls the ported `fighter_38bb0` (`fighter.c:3377`, cited `0x3925C`); the two write
  the same bytes in the same order, so the row is behaviorally exact. The spec keeps the call out
  of the call set (the raw has none) — the same shape as `0x36D20`'s inlined `0x36CE4`. Task 3's
  sweep observed the clear's stores in 16 cases, so the C3d §C3d.5 note (the `0x38BB0` clear
  invisible to `fighter_state_36bc8`'s fixture) is covered by this row.
- **`0x3A0FC`'s 0x105B3A jump table.** Entries 0..2 all point at the spawn block and entry 3 at
  the skip block; the port's `DSB(0x105B3A) < 3` is equivalent, and the cases exercise 0..4.
- **`0x46190`'s config pair.** `0x2D974`/`0x2CAA8` run `mode="real"` in `fighter_46190`'s row (their
  bytes on the original side, the port's C on the port side); the calls are recorded through the
  new seams. Their own rows verify them separately.
- **The `0x3AAFC` correction's reach.** The function is reached from the winner's reaction path
  (`0x3B714`); the planner's `make gp-oracle` is at its pins (N 2064, trace 8320) and Task 4's
  full `make verify` is the batch's evidence that no oracle-visible path moved — not that no driver
  path reaches it.
- **`fighter_39040` stays open on `0x38D90`/`0x38FEC`** (their trees are C3f, §C3e.7); its row
  itself is VERIFIED and `fighter_36870` closes on the row's verdict.
- E3's, P1-P8's and C1-C3d's limits stand: seeds are hand pokes; the memory at a call is `mem[]`
  only; the callee column is one level deep.

## §C3e.6 What the planner ran

- The baseline on the clean `c28e522` worktree: `make diff-verify` -> the §C3e.4 base counter and
  table; `make entry-triage` -> `233/262`; `python3 tools/port_progress.py` -> `771 1203 64` /
  `731 731 100`.
- The prototype, in this worktree: the three rows family first (the `0x3AAFC` correction was found
  by the row's `a0` case; the `0x3A280` correction by `r9`), then the small leaves, then the pose
  family and `0x3A0FC`. Each row was measured with `python3 tools/diff_verify.py --function NAME
  --self-check` until VERIFIED with every mutant detected, then the full `python3
  tools/diff_verify.py --self-check` (§C3e.4 counter), the `python3 -m unittest
  tools.tests.test_diff_verify` suite (109 tests OK, including the new
  `test_each_c3e_mutant_is_caught_by_what_it_breaks` and the extended exact-set, clobber and
  counter assertions), `make entry-triage` (byte-identical), `PR_ORACLE_REQUIRED=1 ./build/run_tests`
  (all checks passed), `make gp-oracle` (every ratchet `ok`: N 2064, trace 8320) and a `symbols.h`
  regeneration (byte-identical). Then `git checkout -- port tools`, leaving only this record and
  the plan.
- The prototype's own corrections: §C3e.2 corrections 1-3.
- The full `make verify` (~25 min) was not run by the planner: the brief's task-scoped gates are
  the ones above. The `port/src` changes are the two corrections, the nine exports and the two
  seams; the executor runs the full gate in Task 4.

## §C3e.7 The tail verdict (required): the frontier after C3e and what a C3f would need

**Measured on the prototype** (capstone over the image; a "row" is any `Spec` entry; the scan is
the same call scan §C3c.7/§C3d.7 used, with runtime addresses `>= 0x5D000` excluded per AGENTS.md's
host-libc rule):

**From the three rows (`0x39040`, `0x392A0`, `0x3AAFC`), the unrowed callees are now exactly two
in the first wave:**

| address | port function | size / insns | callers (rowed) | wave |
|---|---|---|---|---|
| `0x38D90` | `fighter_38d90` | 318 / 87 | `0x39040` | 1 |
| `0x38FEC` | `fighter_38fec` | 81 / 32 | `0x39040` | 1 |

and their trees (wave 2+: the unrowed callees those name):

| wave | addresses |
|---|---|
| 2 | `0x1C500` (37), `0x2F20C` (115), `0x2F4BC` (20), `0x2F4D0` (61), `0x38C5C` (125), `0x38ED0` (284) |
| 3 | `0x2EFD4` (283), `0x2F0F0` (105), `0x2F198` (115), `0x2F280` (146), `0x2F314` (113), `0x2F830` (239), `0x474E4` (215) |
| 4 | `0x1E75C` (23), `0x1E808` (22), `0x2EF24` (34), `0x2F5A0` (615) |
| 5+ | runtime only (`0x61A70`, `0x500BB`, `0x65490`, …; not porting targets) |

So a C3f's **19-address list** to close the three rows and their dependents is: `0x38D90`,
`0x38FEC`; then `0x1C500`, `0x2F20C`, `0x2F4BC`, `0x2F4D0`, `0x38C5C`, `0x38ED0`; then `0x2EFD4`,
`0x2F0F0`, `0x2F198`, `0x2F280`, `0x2F314`, `0x2F830`, `0x474E4`; then `0x1E75C`, `0x1E808`,
`0x2EF24`, `0x2F5A0` — every one ported (a `/* 0xADDR` header), sized above, and blocked in exactly
that dependency order (`0x38D90` before `fighter_39040` can close; `0x38ED0` only through
`0x38FEC`).

**From every current row, the other unrowed first-wave addresses are the ten the C3d record's
§C3d.7 table lists**, each the sole remaining callee of a row that is now otherwise closed:

| address | port function | size / insns | closes the row | its own wave 2 |
|---|---|---|---|---|
| `0x1A7CC` | `fighter_block_start` | 296 / 84 | `fighter_input_mask` (with `0x1AB10`) | `0x1A6AC` (134) |
| `0x1AB10` | `fighter_state_ok` | 74 / 33 | `fighter_input_mask` (with `0x1A7CC`) | — |
| `0x2A690` | `actor_pset_point` | 397 / 125 | `pset_write` | — |
| `0x37178` | `fighter_37178` | 746 / 243 | `fighter_379c4` | — |
| `0x38BB0` | `fighter_38bb0` | 24 / 12 | `fighter_state_36bc8` (with `0x38BC8`) | — |
| `0x38BC8` | `fighter_38bc8` | 34 / 14 | `fighter_state_36bc8` (with `0x38BB0`) | — |
| `0x3B038` | `fighter_3b038` | 72 / 34 | `fighter_3b080` | — |
| `0x3BDB0` | `fight_attack_ready` | 43 / 18 | `fight_command_map` (with `0x3BDDC`) | — |
| `0x3BDDC` | `fighter_attack_consume` | 401 / 127 | `fight_command_map` (with `0x3BDB0`) | `0x3CF38` (203), `0x4649C` (101) |
| `0x46534` | `fighter_46534` | 93 / 29 | `fighter_4f434` | — |

**Verdict.** After C3e the frontier is **not** just the named non-rows: it is the 19-address
`0x38D90`/`0x38FEC` tree above (the price of closing `fighter_39040`) plus the ten-address first
wave for the other seven open rows, plus the three second-wave leaves those two lists name
(`0x1A6AC` through `0x1A7CC`; `0x3CF38` and `0x4649C` through `0x3BDDC`) — **32 addresses**, the
three lists disjoint (19 + 10 + 3). Every one is ported;
sizes are `port/decomp/prage.functions.csv` bytes; the dependency order is the order listed (a
row closes when its listed callees have rows). Everything else the prototype's scan reaches is
runtime (`>= 0x5D000`) or a named non-row (`0x5D812`, `0x62003`, the `0x2EA*` locks).

**What a C3f would need, precisely:** (a) the seventeen rows this batch did not take (`0x38D90`,
`0x38FEC`, their 17 tree addresses, the ten first-wave addresses and the three leaves) — each with
a `Spec`, binding, mutants and exact-set pins, exactly as this batch's rows; (b) one new seam per
callee it stubs that has none (the trees' callees: e.g. `0x38C5C`, `0x1C500`, `0x2F4BC`,
`0x2F4D0`, `0x2F20C`, `0x38ED0` have no seams yet); and (c) a re-measure of this section's table
after each wave. The three rows' own entries need no further port change.

## §C3e.8 Results (the executed tree)

The plan's Tasks 2-4 were executed on `reverse-c3e` at the base `main` `c28e522`: the plan+record
commit `fc378ad`, Task 2 `572697e` (the seventeen rows, the two corrections, the nine exports and the
two config seams), Task 3 `b7e28f6` (the review sweep: the `fighter_36e78` `0x104AE9` seed) and this
closure commit (its sha is in the batch report named below). Every row was re-measured in the tree; the
planner's prototype values held.

| state | diff-verify counter | E2 |
|---|---|---|
| base `c28e522` | `261/261 functions VERIFIED; 945/945 mutants detected; 1 named gaps; 181/205 rows with callees closed (56 have none)` | `targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30`; voice `0 / 115 / 19` |
| final (`b7e28f6` + this closure commit) | `278/278 functions VERIFIED; 1031/1031 mutants detected; 1 named gaps; 196/217 rows with callees closed (61 have none)` | byte-identical |

The batch's task gates, measured on this tree (Task 1's baseline in `verify-base.log`; Task 2's and
Task 3's re-measures in the ledger directory):

```
diff-verify: 278/278 functions VERIFIED; 1031/1031 mutants detected; 1 named gaps; 196/217 rows with callees closed (61 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

`make entry-triage` is byte-identical (no ported function, no `fn_register`); `python3 -m unittest
tools.tests.test_diff_verify` 109 tests OK (Task 2's and Task 3's standalone runs; the only added
method is `test_each_c3e_mutant_is_caught_by_what_it_breaks`); `PR_ORACLE_REQUIRED=1
./build/run_tests` `all checks passed`; `symbols.h` regeneration is byte-identical; `python3
tools/port_progress.py` stays `771 1203 64` / `731 731 100`; README untouched.

**The closure-value accounting.** +17 functions (the seventeen rows), +86 mutants
(11+11+12+2+4+3+4+3+4+5+3+5+3+3+3+3+7); rows with callees 205 -> 217 (twelve of the seventeen have
callees), no-callee 56 -> 61, closed 181 -> 196 (+15): the eleven new rows with callees that close
(`392a0`, `reaction_apply`, `36e78`, `468d8`, `46190`, `36d20`, the four pose setters, `3a0fc`) and
the four dependents the three rows now verify — `fighter_3ad98` (only on `0x392A0`),
`fighter_39834` (only on `0x392A0`), `fighter_reaction` (only on `0x3AAFC`) and `fighter_36870`
(only on `0x39040`). `fighter_39040` stays open on `0x38D90`/`0x38FEC` alone.

**Task 3's measured change.** The store sweep found one store the fixtures left unobserved —
`fighter_36e78`'s `0x36F05 and byte [0x104AE9], 0xFE` (the image pre is 0, so a dropped port store
was invisible) — and `c3e_6e_case` now seeds `0x104AE9 = 0xFF`; with the port's store temporarily
dropped the row reads `0/1` (`fighter_36e78: b1: byte 0x104AE9: original 0xFE, port unchanged`) and
the restored tree `1/1`. All 279 executed store sites of the seventeen rows are observed after the
fix; no mutant's pinned catch set moved (`gate-unittest.log`, `OK`).

**The review fixes folded into this closure commit** (each verified against the tree/raw): this
section and the `0x39040`/`0x38BB0` factoring note (§C3e.5); the plan's file table now says "nine
exports" (it read "eight"; the commit adds nine); and the baseline WAV's sha256 below. No behavior
change: `port/src` is the Task 2 tree, `fighter.c` sha1
`73923b25c4ca71f3bf0eb96a4f74821affaa7c3b` (the Task 2/3 reports said "sha256"; it is the SHA-1).

**The WAV.** The C3e baseline's `make audio-render` WAV is `before-t2.wav`, sha256
`df74acfb65d345fb72cb214102089f2a0ab8d4b271ddc17e2a5f5c4f1a380844` (the Task 1 report's
`4194254d…` is that file's SHA-1, misquoted as sha256; re-verified independently). The closure gate
re-renders it and `cmp`s it equal to the baseline (`WAV-SAME`).

**The two corrections' neutrality.** The batch's behavioral changes are the `0x3AAFC` second-triple
frame (§C3e.2 correction 1) and the `0x3A280` cast removal (correction 2); the full closure ladder —
the 45 oracle lines equal to the k7-k12 baseline, every gp ratchet at its pin (gp-idle-loss N 2064 /
trace 8320) and the WAV `cmp`-equal — is the evidence that no oracle/gp-visible path moved, not that
no driver path reaches them.

**The tail-verdict re-measure (Task 4 Step 2).** On the final tree none of §C3e.7's 32 frontier
addresses has a row (the final `tools/diff_verify.py`'s 251 `Spec` entries scanned against the
list): `0x38D90`/`0x38FEC` and their 17-address tree, the other seven rows' ten first-wave
addresses and the three wave-2 leaves all remain C3f. The list stands unchanged.

**The full gate** on this closure commit (the plan's parallel-safe overrides, log
`/tmp/pr_c3e_final.log`): the run's lines are recorded in the batch report
`.superpowers/sdd/2026-10-05-reverse-c3e-frontier-rows-5/task-4-report.md`; the plan's expected
values are `EXIT=0`, the 45 oracle lines equal to the k7-k12 baseline (`ORACLES-EQUAL`), the WAV
`cmp`-equal to `before-t2.wav` (`WAV-SAME`, the sha256 above) and every gp ratchet at its pin
(Task 1's list verbatim; the planner's gp-oracle N 2064 / trace 8320).

**The gp-u6-moves-b re-pin (the closure fix wave).** The closure kept Task 1's conservative
gp-u6-moves-b pins (1005/2262/2949) although the measured first unexplained frame was 2139 and the
trace and moves claims had 0 differing through 3247; the fix wave raises `GP_MOVES_MIN_FIRST`,
`GP_MOVES_TRACE_MIN_FIRST` and `GP_MOVES_MOVES_MIN_FIRST` to 2139/3248/3248; the measured lines are
`/tmp/pr_c3e_final.log` 335/338/340 and the fix wave's `make gp-moves-oracle` prints
`ratchet N 2139 ok`, `ratchet N 3248 ok` and `ratchet N 3248 ok` (log
`/tmp/c3e_fixwave_gp_moves.log`).

**The C3f tail is the next batch**: §C3e.7's 32 addresses (the `0x38D90`/`0x38FEC` trees
dependency-first, then the ten first-wave addresses and the three wave-2 leaves) with the measured
sizes, dependency order and seam notes; nothing else is left open by C3e beyond the standing named
non-rows and the earlier batches' gaps.
