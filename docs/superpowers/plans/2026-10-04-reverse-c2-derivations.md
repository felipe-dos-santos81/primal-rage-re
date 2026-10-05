# Reverse completion C2: the verification-only callee rows (record)

**Scope.** Track P's second verification-only batch (roadmap row C2 of record
`2026-10-02-reverse-p1-derivations.md` §P1.3, as C1's record §C1.5 and the P4+P5/P6 records left
it): add a differential-verification row for each ported callee the P/C1 rows stub that still has
none, so the dependent rows close. No ported function, no `fn_register`, no E2 move. Plan:
`2026-10-04-reverse-c2-callee-rows.md`. Recipe: E3 record §E3.10; checklist:
`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**Status of the numbers.** Measured by the planner on 2026-10-04 on `reverse-c2` at the base `main`
`e88eb44` (C1, P4+P5 and P6 merged), in this worktree: the baseline gates and a prototype of 27 of
the 36 rows (every gate run, then reverted). The image is
`build/diffrun --exe data/game/C/PRAGE.EXE --image-out FILE`, sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's, E3's, P1-P6's). Every address below is capstone
5.0.7 over that image (fixups applied); `unicorn` 2.1.4 runs the original side. Ghidra was not
consulted.

---

## §C2.1 The unverified set, enumerated from the final table

The brief's evidence list was checked against the baseline table (`make diff-verify`, the callee
column). The distinct callees marked `unverified` are exactly **36** addresses: the brief's
"known members" (C1b's six, P6's eleven minus the verified `0x5D7DC`, P4+P5's `0x2AD40` and the
shared `0x29C08`) **plus two groups the brief's list does not name**:

- **C1's own stubbed callees (17).** C1's 20 rows stub these; C1 did not give them rows, so they
  keep C1's rows and many P rows open: `0x186D0 0x18714 0x187FC 0x1883C 0x189FC 0x18A4C 0x18B04
  0x18B44 0x1DDF4 0x2A408 0x2B2A0 0x33754 0x33864 0x36638 0x39EFC 0x3B8D8 0x3B90C` (record
  §C1.2's seam table).
- **The E3/P1-P3 allow callee without a row (1):** `0x33A10` (`fighter_ctx_swap`) — the allow
  callee of P1-P3/C1/P6 rows; `make diff-verify` marks `33A10 allow unverified` (the brief's list
  missed it because it is not a `stub`).

| # | addr | family | the rows that stub it (base rows) | outcome |
|---|---|---|---|---|
| 1 | `0x13244` | P6 | `22a40` (1) | **row added** |
| 2 | `0x13C70` | P6 | `45c98` (1) | deferred (C2b) |
| 3 | `0x186D0` | C1 | `188ac`, `3c208` (2) | **row added** |
| 4 | `0x18714` | C1 | `188dc`, `18af8`, `3c208` (3) | **row added** |
| 5 | `0x187FC` | C1 | `3c208` (1) | **row added** |
| 6 | `0x1883C` | C1 | `3c208` (1) | **row added** |
| 7 | `0x1890C` | P6 | `24338` (1) | **row added** |
| 8 | `0x189FC` | C1 | `18c14` (1) | **row added** |
| 9 | `0x18A4C` | C1 | `18c14` (1) | **row added** |
| 10 | `0x18B04` | C1 | `18af8`, `39fb0`, `3c208` (3) | **row added** |
| 11 | `0x18B44` | C1 | `18c14` (1) | **row added** |
| 12 | `0x1DDF4` | C1 | `18c14` (1) | **row added** |
| 13 | `0x29BC8` | P6 | `37dd4` (1) | **row added** |
| 14 | `0x29C08` | P6/P45 | `15510`, `3e160` (2) | **row added** |
| 15 | `0x2A148` | P6 | `23f10` (1) | **row added** |
| 16 | `0x2A408` | C1 | `2bc30` (1) | **row added** |
| 17 | `0x2AD40` | P45 | `24454` (1) | **row added** |
| 18 | `0x2AE14` | E3/P1-P3 | `anim_10fa8`, `38034`, `23b68`, `401d4`, `14fa8`, `14ff8`, `150ac`, … (26) | deferred (C2b) |
| 19 | `0x2B2A0` | C1 | `2bc30` (1) | deferred (C2b) |
| 20 | `0x2BCF4` | P6 | `2400c` (1) | **row added** |
| 21 | `0x2C3FC` | E3/P1-P3 | nearly every P/C1 row (47) | deferred (C2b) |
| 22 | `0x33754` | C1 | `2a17c` (1) | deferred (C2b) |
| 23 | `0x33864` | C1 | `2a17c` (1) | **row added** |
| 24 | `0x33A10` | E3/P1-P3 (allow) | `39fb0`, `3a95c`, `468d8` (3) | **row added** |
| 25 | `0x36638` | C1 | `35838` (1) | **row added** |
| 26 | `0x36870` | P2 | `22638`, `4811c`, `4844c` (3) | deferred (C2b) |
| 27 | `0x37D18` | P6 | `40148` (1) | **row added** |
| 28 | `0x39280` | P6 | `22338` (1) | **row added** |
| 29 | `0x39834` | P2 | `211f0`, `22338`, `22494`, `47688`, `47d24`, `482e4`, `48374` (7) | deferred (C2b) |
| 30 | `0x39EFC` | C1 | `18c14` (1) | **row added** |
| 31 | `0x39F40` | P6 | `22338`, `39fb0`, `48374` (3) | **row added** |
| 32 | `0x3AA54` | P6 | `47e30` (1) | **row added** |
| 33 | `0x3B298` | P3 | `18c14`, `47688`, `480b4` (3) | deferred (C2b) |
| 34 | `0x3B714` | P3 | `477e8` (1) | deferred (C2b) |
| 35 | `0x3B8D8` | C1 | `3c208` (1) | **row added** |
| 36 | `0x3B90C` | C1 | `3c208` (1) | **row added** |

**27 rows were prototyped, measured and reverted; 9 are deferred to C2b** (§C2.5). The deferred
nine keep **84 of the 145 rows with callees open** (measured: 61 closed after the 27). The 27 rows
also introduce **11 new row-less stubs** (§C2.8) — the next callee-row batch's scope — so the
closed figure cannot reach 145/145 in this batch even if the nine were done.

**Raw over plan, two corrections (each found by the prototype):**

1. **`0x2A408`'s `0x29F34` call passes the full stream word, not the masked op.** The raw at
   `0x2A471`/`0x2A493` loads `EDX` = the word (`xor edx,edx; mov dx,ax` / `and eax,0xffff; mov
   edx,eax`) and `0x29F34` masks `DL & 0x7F` itself (`0x29F38`). The port's `anim_read_var` took
   `u8 op` and every caller pre-masked, so the row's recorded call arguments read
   `0x29F34(rec, 0x45)` against the original's `0x29F34(rec, 0x8D45)` (the row was MISMATCH at
   `call #0`). Corrected: `anim_read_var(u32 rec, u32 op)` masks internally and all five callers
   pass the full word (`actors.c` 1389/1444/1448/2869 and 2A408's own body). A raw-fidelity fix
   with no behaviour change.
2. **`0x1DDF4` calls each distance twice on every path.** The raw calls `0x187FC` once for the sign
   test and once more for the value (`0x1DE1D` then `0x1DE29`/`0x1DE32`), and `0x1881C` likewise
   (`0x1DE3F` then `0x1DE48`/`0x1DE51`). The port had collapsed each pair into one call
   (`PORT: the raw calls the pure, side-effect-free ai_distance … the port calls it once`), so the
   row was MISMATCH at `call #1` and block `0x1DE48` was unhit. Corrected: `hit_geometry` makes the
   raw's two calls per distance (`ai_distance(); if (d1 < 0) d1 = -ai_distance(); else d1 =
   ai_distance();` and the same for `hit_vert_distance`), which is idempotent for the game and
   exact for the row.

## §C2.2 The rows added (27): cases, mutants, seams

Each row is a `Spec` in `tools/diff_verify.py` (`C2_SPECS`), a binding `b_*` and mutants `m_*` in
`port/tests/diff_runner.c` (`k_bindings`), and the exact-set extensions in
`tools/tests/test_diff_verify.py` (`C2_MASKS`, `C2_KINDS`, the case-set table, the clobber table,
the counter line). Cases carry sentinels on every field a row writes and on every neighbour byte
(checklist 1-2); every row with a callee has at least one mutant caught only by the call list or the
memory at a call (checklist 7). The measured rows (the prototype's final table):

| row | entry | cases | blocks | mutants | callees (each by its own check) |
|---|---|---|---|---|---|
| `fighter_state_39280` | 0x39280 | 2 | 1/1 | 3/3 | - |
| `palette_release` | 0x33864 | 3 | 3/3 | 3/3 | - |
| `fighter_13244` | 0x13244 | 2 | 1/1 | 3/3 | - |
| `fighter_29c08` | 0x29C08 | 4 | 1/1 | 3/3 | - |
| `actor_pset_flag_5f` | 0x2A148 | 3 | 4/4 | 3/3 | - |
| `fighter_29bc8` | 0x29BC8 | 2 | 1/1 | 2/2 | `2A17C` stub VERIFIED |
| `hit_anchor_y` | 0x1890C | 3 | 1/1 | 3/3 | `186D0` stub VERIFIED |
| `ai_distance` | 0x187FC | 2 | 1/1 | 2/2 | `186D0` stub VERIFIED |
| `fighter_slot_latch` | 0x186D0 | 4 | 8/8 | 3/3 | `18350`, `18540` stub unverified |
| `hit_record_x` | 0x18714 | 4 | 6/6 | 3/3 | `18350`, `18540` stub unverified |
| `fighter_189fc` | 0x189FC | 7 | 6/6 | 4/4 | `1A570` stub VERIFIED, `33A10` allow VERIFIED |
| `fighter_18a4c` | 0x18A4C | 6 | 8/8 | 3/3 | `189FC` stub VERIFIED, `1A570` real VERIFIED, `33950` allow VERIFIED |
| `fighter_39efc` | 0x39EFC | 5 | 7/7 | 3/3 | `33A10` allow VERIFIED |
| `fighter_3b8d8` | 0x3B8D8 | 6 | 5/5 | 3/3 | - |
| `fighter_3b90c` | 0x3B90C | 6 | 4/4 | 3/3 | - |
| `fighter_ctx_swap` | 0x33A10 | 2 | 1/1 | 3/3 | - |
| `fighter_1883c` | 0x1883C | 3 | 1/1 | 4/4 | `186D0`, `18714` stub VERIFIED, `18788` stub unverified |
| `hit_facing_flag` | 0x18B04 | 4 | 6/6 | 4/4 | `18714` stub VERIFIED, `33950` allow VERIFIED |
| `fighter_18b44` | 0x18B44 | 4 | 7/7 | 3/3 | `2AE14` stub unverified |
| `hit_geometry` | 0x1DDF4 | 7 | 11/11 | 4/4 | `187FC` stub VERIFIED, `1881C` stub unverified |
| `fighter_pose_start` | 0x39F40 | 2 | 1/1 | 4/4 | `33A10` allow VERIFIED |
| `actors_anim_seek` | 0x2BCF4 | 2 | 1/1 | 3/3 | `2A408` stub VERIFIED |
| `fighter_37d18` | 0x37D18 | 2 | 3/3 | 3/3 | `2BC30`, `39A10` stub VERIFIED, `2C3FC` stub unverified |
| `fighter_3aa54` | 0x3AA54 | 3 | 3/3 | 3/3 | `1A5AC` stub unverified |
| `release_record` | 0x2AD40 | 4 | 8/8 | 3/3 | `249B0`, `249D0`, `2B150` stub unverified, `2EA30` allow unverified |
| `anim_next_sprite_id` | 0x2A408 | 6 | 12/12 | 5/5 | `29F34` stub unverified |
| `fighter_state_36638` | 0x36638 | 7 | 13/13 | 4/4 | `2BC30` stub VERIFIED, `38154` stub unverified |

**New seams added to `port/src`** (the E3 recipe; each is a first statement and `E.callee_clobbers`
re-derives every declared clobber):

| callee | port function | seam | clobbers (image) |
|---|---|---|---|
| `0x18540` | `fighter_18540` | `PR_SEAM(0x18540u, side)` | () |
| `0x18350` | `fighter_18350` | `PR_SEAM(0x18350u, side, anchor)` | edx |
| `0x18788` | `hit_record_y` | `PR_SEAM_RET(0x18788u, side)` | () |
| `0x1881C` | `hit_vert_distance` | `PR_SEAM_RET0(0x1881Cu)` | () |
| `0x1A5AC` | `fighter_1a5ac` | `PR_SEAM_RET(0x1A5ACu, side)` | () |
| `0x38154` | `fighter_38154` | `PR_SEAM(0x38154u, side)` | () |
| `0x249B0` | `list_insert_after` | `PR_SEAM(0x249B0u, at, rec)` | () |
| `0x249D0` | `list_unlink` | `PR_SEAM(0x249D0u, rec)` | () |
| `0x2B150` | `set_dead` | `PR_SEAM(0x2B150u, rec)` | esi, edi, ebp |
| `0x29F34` | `anim_read_var` | `PR_SEAM_RET(0x29F34u, rec, op)` | edx |

`fighter_18540`, `fighter_18350`, `hit_record_y`, `hit_vert_distance`, `fighter_1a5ac` and
`release_record` lost `static` (declared in `fighter.h` / `actors.h`) so the bindings and mutants
can call them. All are source-only changes: no new `FN_` address, no `fn_register`, so the E2 table
is byte-identical (§C2.4) and `port_progress.py` stays `771 1203 64` / `731 731 100`.

**Fixture decisions.** `0x2A408`'s `s0`/`s5` keep `rec+8` a valid in-image pointer
(`0x00109234`) whose low word carries bit 15, so the `@bit8` mutant dereferences valid memory
instead of faulting (the first prototype crashed `diffrun` on it). `release_record`'s list head
`0x105B3C` is poked to itself (`le32(0x105B3C)`) so the mutants' inline list ops land inside the
image (the first prototype seeded it `0x11111111` and the mutants faulted at `next+4`).
`release_record`'s `0x2EA30` is allowed (not stubbed): the port drops the original's interrupt-lock
counter (`PORT: inert in the port's single-threaded loop`, record §47-C), and with `0xBCD60 = 0`
both calls are memory no-ops, so the call list and the bytes agree. `0x18A4C` runs `0x1A570` real
(its own row) so its two calls can disagree — a constant stub EAX would make the two
`mov al,1` blocks unreachable.

## §C2.3 The mutants

87 mutants, all detected (`make diff-verify --self-check`); what alone catches each is pinned by
`test_each_c2_mutant_is_caught_by_what_it_breaks` (`C2_KINDS` + the measured case-set table). The
kinds (measured):

```
fighter_state_39280@mutant byte      fighter_1883c@mutant byte call#2 memory call#3 memory
fighter_state_39280@side   byte      fighter_1883c@side   byte call#2 call#2 memory call#3 call#3 memory
fighter_state_39280@width  byte      fighter_1883c@latch  call#1 call#1 memory call#2 call#2 memory call#3
palette_release@mutant     byte      fighter_1883c@arg    call#2
palette_release@width      byte      hit_facing_flag@mutant byte call#0 memory
palette_release@off        byte      hit_facing_flag@mode   byte call#0
fighter_13244@mutant       byte      hit_facing_flag@side   byte call#0 call#0 memory
fighter_13244@addr         byte      hit_facing_flag@store  byte call#0 memory
fighter_13244@val          byte      fighter_18b44@mutant call#0
fighter_29c08@side         eax       fighter_18b44@latch  byte call#0 memory call#1 memory
fighter_29c08@char         eax       fighter_18b44@layer  call#1
fighter_29c08@sext         eax       hit_geometry@mutant  call#1..3 eax
actor_pset_flag_5f@mutant  byte      hit_geometry@side    call#2 call#3 eax
actor_pset_flag_5f@word    byte      hit_geometry@abs     call#1..3 eax
actor_pset_flag_5f@pset    byte      hit_geometry@table   call#1..3 eax
fighter_29bc8@side         call#0    fighter_pose_start@mutant byte
fighter_29bc8@rec          call#0    fighter_pose_start@side   byte
hit_anchor_y@mutant        byte call#1 memory   fighter_pose_start@arg byte
hit_anchor_y@side          byte call#0 call#1 call#1 memory   fighter_pose_start@field byte
hit_anchor_y@field         byte call#1 memory   actors_anim_seek@mutant byte call#0 memory
ai_distance@mutant         eax       actors_anim_seek@char byte call#0
ai_distance@order          call#0 call#1   actors_anim_seek@width byte
fighter_slot_latch@mutant  byte call#0 call#1   fighter_37d18@mutant byte
fighter_slot_latch@side    byte call#0 call#1   fighter_37d18@state byte call#0..2 memory
fighter_slot_latch@anchor  call#1    fighter_37d18@order call#2 memory
hit_record_x@mutant        byte call#1 memory eax   fighter_3aa54@mutant byte
hit_record_x@side          call#0 call#1 eax   fighter_3aa54@char byte call#0 memory
hit_record_x@anchor        byte call#1 call#1 memory   fighter_3aa54@field byte call#0 memory
fighter_189fc@mutant       eax       release_record@mutant byte call#0..3 call#0 memory
fighter_189fc@side         call#0 eax   release_record@field byte call#0..3 call#0/1 memory
fighter_189fc@bit          call#0 eax   release_record@latch byte call#0..3
fighter_189fc@al           eax       anim_next_sprite_id@mutant byte call#0 eax
fighter_18a4c@mutant       eax       anim_next_sprite_id@bit8 call#0 eax
fighter_18a4c@side         call#0..2   anim_next_sprite_id@op byte call#0 eax
fighter_18a4c@arg          call#1    anim_next_sprite_id@var call#0
fighter_39efc@mutant       eax       anim_next_sprite_id@clear call#0 eax
fighter_39efc@val          eax       fighter_state_36638@mutant byte call#0 eax
fighter_39efc@side         eax       fighter_state_36638@anim byte call#0 eax
fighter_3b8d8@mutant       eax       fighter_state_36638@mode byte call#0 memory
fighter_3b8d8@side         eax       fighter_state_36638@side call#0
fighter_3b8d8@bound        eax
fighter_3b90c@mutant       eax
fighter_3b90c@side         eax
fighter_3b90c@bound        eax
fighter_ctx_swap@mutant    byte
fighter_ctx_swap@slot      byte
fighter_ctx_swap@rec       byte
```

**Named limits (mutations no case can observe).** `0x29C08`'s `@side`/`@char`/`@sext` differ only in
EAX (the row's callee-free claim). `0x3B8D8`/`0x3B90C` have no callee, so their mutants are
`eax`-only by construction. `0x186D0`/`0x18714`'s `18540`/`18350` are stubs without rows (C3); the
rows' own call lists and memory-at-call are pinned. `release_record`'s `2EA30` allow is a memory
no-op only for `0xBCD60 = 0`; the case pokes it. `0x2A408`'s bit-8 arm reads a value that must be a
valid pointer in the mutants too (§C2.2's fixture note). `0x18B44`'s `2AE14` spawn is stubbed (its
own row is C2b).

## §C2.4 Counters, measured

| state | diff-verify counter | E2 |
|---|---|---|
| base `e88eb44` | `149/149 functions VERIFIED; 391/391 mutants detected; 1 named gaps; 41/126 rows with callees closed (23 have none)` | `240 unported, 255 ported; supplement 131 (7 unported, 0 stale); untrusted 30`; voice `0 / 115 / 19` |
| C2 prototype (27 rows) | `176/176 functions VERIFIED; 478/478 mutants detected; 1 named gaps; 61/145 rows with callees closed (31 have none)` | byte-identical |

The arithmetic: +27 functions, +87 mutants, rows with callees 126 -> 145 (the 27 new rows minus the
8 with no callees), no-callee 23 -> 31, closed 41 -> 61. The base rows that close are those whose
callee set is a subset of the 27 with rows plus `33950`, `339AC`, `3C4CC`, `5D7DC` and the other
already-verified callees; the nine deferred rows keep 84 open.

`make entry-triage` is byte-identical (no ported function, no `fn_register`);
`PR_ORACLE_REQUIRED=1 ./build/run_tests` prints `all checks passed` (the `port/tests/ghidra_data.bin`
fixture must exist; it is git-ignored); `make diff-verify` is green (the Python suite 165 tests OK
and the counter above); `python3 tools/port_progress.py` stays `771 1203 64` / `731 731 100`.

## §C2.5 The nine deferred rows (C2b) and their evidence

All nine are ported and verifiable in principle; the planner's session budget stopped after 27
rows. The six C1b rows carry C1's seam lists (record §C1.5) unchanged; the three others carry the
planner's measured direct-callee lists (capstone over the image, same scan as §C2.1):

| callee | blocks (insns) | direct callees needing a seam (measured) | notes |
|---|---|---|---|
| `0x2B2A0` | 85 (682) | `29DB8`, `2B8F8`, `2EA64` (new); `2AE14`, `2B150`, `2C3FC`, `5D7DC` already seamed/row-less | the animation-opcode dispatcher; 3 indirect calls (`0x2B56D/0x2B594/0x2B5EA`) already `fn_resolve`-routed in the port |
| `0x33754` | 15 (94) | `1B544` (host-owned `res_resolve`), `62003` (runtime fatal) | the full-table path is a documented port deviation (the port returns 0; the original calls the fatal 0x62003); the row needs the host call allowed/limited and the fatal block named |
| `0x13C70` | 12 (73) | `1B544` (host), `249B0`, `249D0` (seamed now) | the port's `effects_spawn` skips the copy when `res_resolve` fails; the row needs the host call handled |
| `0x2C3FC` | 100 (395) | `1CA14 1CA6C 1CC28 1CD9C 1CE04 1CE70 1D238 1D244` (flow.c; two no-arg returning: `PR_SEAM_RET0`) | C1 §C1.5: the 0x2C3E0 jump table is the bounded form `switch_cases` follows; a voice-id sweep 0..0x100 is the case menu |
| `0x2AE14` | 33 (260) | `2AC80 2A820 2A620 1C390 1C3A0` | C1 §C1.5; one indirect call at 0x2B0E9 (the animation-opcode target, resolved per case) |
| `0x36870` | 31 (238) | `385B0 39280 164E8 39040 379C4 3C520 365C8 37D18 36BC8` (`39280`, `37D18` now have rows) | C1 §C1.5; `36638` now has a row |
| `0x39834` | 16 (113) | `3AFC4` allow (4-function tree, no seamed member), `39738 392A0 36CE4 4F434`; `33A10` allow, `468D8`/`36D98` have rows | C1 §C1.5 |
| `0x3B298` | 28 (138) | `3B134 1AB5C 46460 1A734`; `33A10`, `3AFC4` allow | C1 §C1.5 |
| `0x3B714` | 23 (132) | `3C59C 62003 39EFC 3B080 3AE9C 2BD44 3AAFC 3B6C4 3AD98`; `33A10`, `3AFC4` allow | C1 §C1.5; `39EFC` now has a row; `62003` is the runtime fatal |

Their dependent rows stay open with this reason: "the callee has no row yet; C2b owns it".

## §C2.6 Decisions, named gaps and limits

**No decision is left to the user.** The scope is the raw's: the 36 unverified callees the final
table names (§C2.1). The nine deferrals are a session-budget call, recorded with their evidence
(§C2.5) exactly as C1 deferred its six. The two corrections of §C2.2 are raw-over-plan, recorded
with their addresses. The 27 prototyped rows are complete and measured.

Named gaps and limits:

- **The nine C2b rows** (§C2.5): deferred; they keep 84 rows open.
- **The 11 new row-less stubs C2 introduces** (§C2.8): the next callee-row batch's scope.
- **`release_record`'s `2EA30`** is allowed and only a memory no-op for `0xBCD60 = 0` (the port
  drops the interrupt-lock counter as inert, record §47-C); the row pokes it.
- **`0x2A408`'s `s0`/`s5` pointer fixture**: `rec+8` must be a valid pointer for the mutants too; a
  non-pointer bit-15 word makes the `@bit8` mutant fault rather than mismatch.
- **`0x29C08`'s `@side`/`@char`/`@sext`** are EAX-only; the row has no callee.
- **`hit_facing_flag`'s `0x18AC7` store is a raw self-assignment**: `mov [eax*4+0x1077dc], ebx` after
  `0x18B29` loaded `slot[side]+0x2C` into EBX, in the shared tail block C1 §C1.6 already names for
  `0x18AF8`. Task 3's store sweep finds it the 27 rows' only survivor (81 store sites; every other
  store is observable) because the value written is read from the address written; no seed or case
  can observe it. A dead store in the original, reproduced faithfully; the row keeps it.
- **The 27 rows' Task-3 store sweep has no committed regression pin**: the sweep that shows every
  other store observable is scratch
  (`.superpowers/sdd/2026-10-04-reverse-c2-callee-rows/task-3-scratch/sweep.py`); pinning it in
  `tools/tests/test_diff_verify.py` would add cases to the recorded suite in a docs-only closure
  task, so the pin is a named deferral, not silently absent (the same deferral as C1's four seed
  fields, §C1.6).
- E3's, P1's and P2's limits stand: seeds are hand pokes; the memory at a call is mem[] only; the
  callee column is one level deep.
- The `title_pin` unittest failure on this tree is pre-existing and outside `make verify`.

## §C2.7 What the planner ran

- The baseline on the clean `e88eb44` worktree: `make diff-verify` -> the §C2.4 counter and table;
  `PR_ORACLE_REQUIRED=1 ./build/run_tests` -> all checks passed; `make entry-triage` ->
  `240/255`; `python3 tools/port_progress.py` -> `771 1203 64` / `731 731 100`.
- The prototype, in this worktree, in four stages (the C1 stubs' leaf rows; the C1 stubs'
  context/flag rows; the 1883C/18B04/18B44/1DDF4 family; the P6/P45 medium rows including 2A408),
  each measured with `python3 tools/diff_verify.py --function NAME --self-check`, the fix rounds of
  §C2.2, then `make diff-verify` (the §C2.4 counter), `make entry-triage` (byte-identical) and
  `PR_ORACLE_REQUIRED=1 ./build/run_tests` (all checks passed). Then `git checkout -- port tools`,
  leaving only this record and the plan.
- The prototype's diff (2250 lines, the exact files the plan's Task 2 applies) is embedded in the
  plan.
- The full `make verify` (~25 min) was not run by the planner: the brief's task-scoped gates are
  the ones above, and no rendering/timing/RNG path changed (the `port/src` changes are seams, two
  raw-fidelity corrections and `static` removals). Task 5 runs it on the final state.
- Task 5, the final gate on this worktree (base `e5e67a2` plus the review-nits `tests:` commit; its
  docs commit follows): the four task gates
  above (`make diff-verify` -> the §C2.4 counter, unchanged after the three review nits; `make
  entry-triage` byte-identical; `PR_ORACLE_REQUIRED=1 ./build/run_tests` -> `all checks passed`;
  `port_progress.py` -> `771 1203 64` / `731 731 100`) and the full `make verify` with the
  parallel-safe dump overrides: `EXIT=0`; the 45 oracle lines extracted by the K7-K12 gate's
  `grep -E` are byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`
  (`ORACLES-EQUAL`); `make audio-render` is byte-identical to `before-t2.wav` (sha256
  `4194254de1155a2dda0ee0dd69f638d92cf9eea0`). The three review nits folded in before the run
  (duplicate `fighter_3b90c` binding dropped, `m_1ddf4`'s unused `idx` dropped, `m_18714`'s comment
  now names both deviations) change no counter or oracle line.

## §C2.8 The new frontier

The 27 rows stub 11 callees that still have no row, so they keep their own rows open:

`0x18540 0x18350` (186D0/18714), `0x18788` (1883C), `0x1881C` (1DDF4), `0x1A5AC` (3AA54),
`0x249B0 0x249D0 0x2B150` (release_record), `0x2EA30` allow (release_record), `0x29F34` (2A408),
`0x38154` (36638).

Their rows are the next callee-row batch (C3) with the same recipe; every one is ported and its
direct callees are the port's own helpers (the C1/C2 seams are in place).
