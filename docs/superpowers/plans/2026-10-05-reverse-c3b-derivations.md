# Reverse completion C3b: the frontier rows, part 2 (record)

**Scope.** Track P's fifth verification-only batch (roadmap row C3b of record
`2026-10-05-reverse-c3-derivations.md` §C3.7, the frontier remainder): add the
differential-verification row for the seventeen addresses this session measured — `0x29F34 0x18350
0x18540 0x18788 0x38154 0x2B150 0x3AFC4 0x1CA6C 0x1CE70 0x1CD9C 0x1CE04 0x1CC28 0x13420 0x249C0
0x29DB8 0x2B8F8 0x49444` — so their dependents close. No ported function, no `fn_register`, no E2
move. The remaining 21 candidate addresses and the 9 new frontier items these rows create are
**C3c** (§C3b.7). Plan: `2026-10-05-reverse-c3b-frontier-rows-2.md`. Recipe: E3 record §E3.10;
checklist: `.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**Status of the numbers.** Measured by the planner on 2026-10-06 on `reverse-c3b` at the base `main`
`2cf874b` (= C3a merged), in this worktree: the baseline gates and a full prototype of the seventeen
rows (every gate run, then reverted). The image is `build/diffrun --exe data/game/C/PRAGE.EXE
--image-out FILE`, sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's, E3's, P1-P8's, C1-C3's).
Every address below is capstone 5.0.7 over that image (fixups applied); `unicorn` 2.1.4 runs the
original side. Ghidra was not consulted. The eleven rows of the previous session's uncommitted
prototype (`/tmp/c3b-partial-prototype.patch`) were re-measured in this tree and two of its fixture
choices were corrected (§C3b.2); the six rows added this session are `0x13420`, `0x49444`,
`0x2B150`, `0x29DB8`, `0x2B8F8`, `0x3AFC4`.

---

## §C3b.1 The members, re-derived from the raw

The C3 record §C3.7's 36 candidates + its two new items (`0x13420`/`0x249C0`) + the named non-row
`0x2EA30` were checked against the final C3 table measure: the table has **43 distinct unverified
callees** = the 36 candidates + `0x2EA30` + the two new items + the four non-rows `0x1B544`
(host-owned `res_resolve`), `0x5D812`/`0x29D60`/`0x2EA64` (bare stubs/rets). The membership is exact
(no correction). The 36 split as 7 previously-open (6 rows + `0x2EA30` named), 18 type family,
5 voice, 6 allowed tree (4 rows + the `0x1C390`/`0x1C3A0` pair). This batch measures 17 of them;
the rest is §C3b.7.

The batch's rows and the raw's scan (sizes/`n_callers` from `port/decomp/prage.functions.csv`,
re-checked against capstone):

| row | entry | insns | the raw's direct callees | row design |
|---|---|---|---|---|
| `anim_read_var` | `0x29F34` | 122 | none | the three 6-entry jump tables (RESOLVED_JUMPS `0x29F78`/`0x29FE5`/`0x2A056`); ring, own fields, parent/child fields, above-0x51 zero |
| `fighter_18350` | `0x18350` | 39 | `1A570` (real) | per-character anchor table, the bit-15 x negation, the two `<<6` latching stores |
| `fighter_18540` | `0x18540` | 46 | `18460`/`18428` (allow) | the camera word, the sprite/limit clamp into `0x100AF0` |
| `hit_record_y` | `0x18788` | 20 | `18540`/`18350`/`1A570` (real), `18428`/`18460` (allow) | the +0x42 bit-3 shortcut, the re-latch, the -`0x100AB4` difference |
| `fighter_38154` | `0x38154` | 47 | `2BC30` (stub), `36638`/`35838` (real), `367DC` (allow) | the +0x3C/+0x18 range alternatives, the bit-14 far/near arms, the flag arm |
| `set_dead` | `0x2B150` | 57 | `33864` (stub), `249B0`/`249D0` (real via the callback), `5D812`/`12800`/`1C458`/`1C3D0` (allow) | the dead bit, the type-callback gate, the pset palette release, the render unlist |
| `fighter_anim_triple` | `0x3AFC4` | 39 | `62003` (the named fatal) | the per-character byte `<<6` + frame, the three table triples; out-of-range is the named unhit `0x3AFCE` |
| `snd_music_stop` | `0x1CA6C` | 8 | `1CA40` (stub), `5DEAF` (stub) | the sequence gate, the playing predicate, the stop call |
| `snd_sample_playing` | `0x1CE70` | 18 | `5DD03` (stub) | the four-slot handle scan with the status-4 predicate and the +0x0C clear |
| `snd_samples_stop_all` | `0x1CD9C` | 20 | `5DD03`/`5DC8B`/`5DC0F` (stub) | the driver gate, the four-slot clear/stop/re-init walk |
| `snd_sample_stop` | `0x1CE04` | 21 | `5DD03`/`5DC8B`/`5DC0F` (stub) | the first matching slot, its +0x0C clear, the tail stop/re-init |
| `snd_sample_queue` | `0x1CC28` | 92 | `1B544`/`500BB` (allow), `5DD03`/`5DC8B`/`5DC0F` (stub) | the driver/pause/handle gates, the >0x6000 single-arm path, the 3..0 scan, the smallest-time candidate tail |
| `anim_write_var` | `0x29DB8` | 122 | none | the ring, the six own fields, the parent/child fields (the three resolved jump tables) |
| `anim_operand` | `0x2B8F8` | 55 | `29F34` (stub) | the 0x1F previous-op arm, the mode-0 byte return, the 0x2000/0x4000 modes, the 0x1000..0x6000 selector scalings and the byte path |
| `effect_teardown` | `0x13420` | 42 | `249D0`/`249B0`/`33714`/`33734` (allow) | the lock, the unlink, the type dispatch (jump table `0x13408`), the free-list insert |
| `actor_type_49444` | `0x49444` | 34 | `2B150` (stub), `249D0`/`249B0` (real) | the +0x1C bit-1 entry clear, the +0x10 child retire, the `0x1083C4` return |
| `list_insert_before` | `0x249C0` | 16 | none | straight-line; the effects.c copy carries it |

`0x2EA30` (the interrupt-lock nesting counter) still has **no port C function** — the port's
single-threaded loop treats it as inert — so it cannot be rowed without porting it; it stays a
**named allow**, the C3 §C3.1 treatment. `0x1B544`/`0x5D812`/`0x29D60`/`0x2EA64` stay the four
non-rows (the first host-owned, the other three bare stubs/rets); §C3b.7 adds the new host names
this batch surfaces.

## §C3b.2 The rows: cases, seams, fixtures and the corrections

Each row is a `Spec` in `tools/diff_verify.py` (`C3B_SPECS`), a binding `b_*` and mutants `m_*` in
`port/tests/diff_runner.c` (`k_bindings`), and the exact-set extensions in
`tools/tests/test_diff_verify.py` (`C3B_MASKS`, `C3B_KINDS`, the case-set table, the clobber table,
the counter line). The measured rows:

| row | entry | cases | blocks | mutants | callees (each by its own check) |
|---|---|---|---|---|---|
| `anim_read_var` | 0x29F34 | 20 | 24/24 | 8/8 | - |
| `fighter_18350` | 0x18350 | 10 | 11/11 | 5/5 | `1A570` real VERIFIED |
| `fighter_18540` | 0x18540 | 13 | 14/14 | 6/6 | `18428`/`18460` allow unverified |
| `hit_record_y` | 0x18788 | 4 | 6/6 | 5/5 | `18350`/`18540`/`1A570` real VERIFIED; `18428`/`18460` allow unverified |
| `fighter_38154` | 0x38154 | 17 | 36/36 | 8/8 | `2BC30` stub VERIFIED, `35838`/`36638` real VERIFIED; `367DC` allow unverified |
| `set_dead` | 0x2B150 | 8 | 7/7 | 7/7 | `33864` stub VERIFIED, `249B0`/`249D0` real VERIFIED; `1C458`/`1C3D0`/`5D812`/`12800` allow unverified |
| `fighter_anim_triple` | 0x3AFC4 | 5 | 3/4 (0x3AFCE named) | 6/6 | - |
| `snd_music_stop` | 0x1CA6C | 3 | 4/4 | 6/6 | `1CA40` stub unverified, `5DEAF` stub unverified |
| `snd_sample_playing` | 0x1CE70 | 7 | 9/9 | 6/6 | `5DD03` stub unverified |
| `snd_samples_stop_all` | 0x1CD9C | 3 | 8/8 | 6/6 | `5DC0F`/`5DC8B`/`5DD03` stub unverified |
| `snd_sample_stop` | 0x1CE04 | 7 | 8/8 | 6/6 | `5DC0F`/`5DC8B`/`5DD03` stub unverified |
| `snd_sample_queue` | 0x1CC28 | 10 | 19/19 | 13/13 | `1B544`/`500BB` allow unverified; `5DC0F`/`5DC8B`/`5DD03` stub unverified |
| `anim_write_var` | 0x29DB8 | 22 | 24/24 | 8/8 | - |
| `anim_operand` | 0x2B8F8 | 16 | 28/28 | 8/8 | `29F34` stub VERIFIED |
| `effect_teardown` | 0x13420 | 8 | 6/6 | 6/6 | `249B0`/`249D0` allow VERIFIED; `33714`/`33734` allow unverified |
| `actor_type_49444` | 0x49444 | 8 | 7/7 | 6/6 | `2B150` stub VERIFIED; `249B0`/`249D0` real VERIFIED |
| `list_insert_before` | 0x249C0 | 3 | 1/1 | 5/5 | - |

**Seams and exports** (`port/src`, each seam a first statement; `E.callee_clobbers` re-derives every
declared clobber):

| function | address | change | conformance |
|---|---|---|---|
| `snd_music_playing` | `0x1CA40` | loses `static`, gains `PR_SEAM_RET0(0x1CA40u)`, declared in `flow.h` | clobbers `()` (image) |
| `AIL_init_sample` | `0x5DC0F` | gains `PR_SEAM0(0x5DC0Fu)` | clobbers `("ebx","ecx","edx")` (image) |
| `AIL_stop_sample` | `0x5DC8B` | gains `PR_SEAM0(0x5DC8Bu)` | clobbers `("ebx","ecx","edx")` |
| `AIL_sample_status` | `0x5DD03` | gains `PR_SEAM_RET0(0x5DD03u)` | clobbers `()` |
| `AIL_stop_sequence` | `0x5DEAF` | gains `PR_SEAM0(0x5DEAFu)` | clobbers `("ebx","ecx","edx")` |
| `fighter_state_367dc` | `0x367DC` | loses `static`, declared in `fighter.h` (the `0x38154` mutant cores call it) | no seam (allow) |
| `set_dead` | `0x2B150` | loses `static` (and its two forward declarations, `actors.c:1319`/`:2669`), declared in `actors.h` | its own row's binding |

**Corrections during prototyping (each recorded here with its evidence):**

1. **The draft's AIL stub clobbers were wrong.** The previous prototype declared `()` for all five
   AIL stub calls; `E.callee_clobbers` over the image derives `("ebx","ecx","edx")` for `0x5DC0F`,
   `0x5DC8B` and `0x5DEAF` (`0x1CA40`/`0x5DD03` are `()`), and
   `test_each_stub_declares_the_registers_its_callee_clobbers` fails on the draft. The specs were
   updated to the bytes' sets; the four voice rows re-measured VERIFIED with the corrected clobbers
   (the poisoned registers are dead at every raw call site, so the correction is observationally
   free but byte-derived).
2. **The effects.c list copies must stay seam-less (a harness constraint, not a port deviation).**
   The first `effect_teardown` draft added `PR_SEAM(0x249B0u, ...)`/`PR_SEAM(0x249D0u, rec)` to
   `effects_list_insert_after`/`effects_list_unlink` and declared the row's two list calls `real`.
   That broke C3's `actor_alloc@empty` pin: C3's `m_2ac80_*` mutants call those same seam-less
   copies, and it is their *missing recorded calls* that catch them (C3 record §C3.5); with the
   seams the mutant calls got recorded and `test_each_c3_mutant_is_caught_by_what_it_breaks` failed
   (`'call #0'` missing from `actor_alloc@empty`). The seams were reverted and the row's four
   callees (`0x249B0`, `0x249D0`, `0x33714`, `0x33734`) run as **allows** compared by final bytes.
   The `@lock` mutant was dropped with that (the lock's set/restore ordering is only observable at a
   recorded call: with the seed `0x5A` the final byte agrees); the fixture interlinks rec, the
   sentinel and two neighbour nodes so `@unlink` and `@tail` move bytes.
3. **No raw-over-port corrections.** Every row matched on its first measurement of this tree (after
   the two fixture/draft corrections above). `set_dead`'s export required removing two `static`
   forward declarations the draft had missed (`actors.c:1319`, `:2669`); that is a compile-level
   fix, not a behavior change.

**Fixtures.** The rows reuse the zero-BSS scratch records (`0x10A800`/`0x10A900`/`0x10AC00`/
`0x10B000`), the seeded globals (`0x105B4C`, `0x105BE4/BE6/BE8/BD4`, `0x1014EC`/`0x1014F4`,
`0x107798`/`0x107498`, `0x10782A`+slot_char*0x94, `0x10839C`/`0x1083C4`, `0x10275C`/`0x105B44`,
`0x10153C`, `0x1077A8`/`0x1077B0`, `0x100AB0/AB4`, `0x102860`, `0x1028C0/C8/CC/D4/D9/DA/DB`,
`0x9AF3C/D`, `0xFCCE8`, `0xFCCF0` (read only), `0xF0A78`, `0x108884/0x1088BD`,
`0x1078F0`) and the existing `ANIM_BEGIN` declaration for `0x2BC30`. `set_dead`'s record must be
in-pool (`DS_001014F4` = `0x10A000`, `rec = 0x10A068`): the port's `actor_pset` has the spec §7
pool guard, the raw does not.

## §C3b.3 The mutants

115 mutants, all detected (`make diff-verify --self-check`: `779/779`); what alone catches each is
pinned by `test_each_c3b_mutant_is_caught_by_what_it_breaks` (`C3B_KINDS` plus the measured
case-set table; the exact dicts are the ones committed in `tools/tests/test_diff_verify.py`). The
rows with callees have mutants caught only by the call list or the memory at a call
(`set_dead@bit/@clear/@gate/@pal/@pset/@table`, `actor_type_49444@bit/@child/@idx/@link/@sign`,
`anim_operand@adv/@op`, `fighter_18350@anchor/@neg/@side/@tab`, `hit_record_y@always/@bit/@call`,
`snd_*` at the AIL stubs, `fighter_38154` at the real animation calls).

The mutant kinds (measured; the record's copy of the test's `C3B_KINDS`):

```
C3B_KINDS = {
    "actor_type_49444@bit": {"byte", "call #0", "call #0 memory", "call #1", "call #2"},
    "actor_type_49444@child": {"byte", "call #0", "call #1", "call #2"},
    "actor_type_49444@idx": {"byte", "call #0", "call #0 memory", "call #1", "call #2"},
    "actor_type_49444@link": {"call #0", "call #1", "call #2"},
    "actor_type_49444@reclr": {"byte", "call #0", "call #1", "call #2"},
    "actor_type_49444@sign": {"byte", "call #0", "call #1", "call #2"},
    "anim_operand@adv": {"byte", "call #0 memory"},
    "anim_operand@be8": {"byte"},
    "anim_operand@byte": {"eax"},
    "anim_operand@deref": {"byte"},
    "anim_operand@mask": {"eax"},
    "anim_operand@op": {"byte", "call #0", "call #0 memory", "eax"},
    "anim_operand@scale": {"eax"},
    "anim_operand@sel": {"byte", "eax"},
    "anim_read_var@a45": {"eax"},
    "anim_read_var@cswap": {"eax"},
    "anim_read_var@opff": {"eax"},
    "anim_read_var@pswap": {"eax"},
    "anim_read_var@ret": {"eax"},
    "anim_read_var@ring": {"eax"},
    "anim_read_var@sx": {"eax"},
    "anim_read_var@w58": {"eax"},
    "anim_write_var@bep": {"byte"},
    "anim_write_var@child": {"byte"},
    "anim_write_var@high": {"byte"},
    "anim_write_var@mask": {"byte"},
    "anim_write_var@radio": {"byte"},
    "anim_write_var@ringm": {"byte"},
    "anim_write_var@swap": {"byte"},
    "anim_write_var@w44": {"byte"},
    "effect_teardown@buf": {"byte"},
    "effect_teardown@flag": {"byte"},
    "effect_teardown@restore": {"byte"},
    "effect_teardown@tail": {"byte"},
    "effect_teardown@type1": {"byte"},
    "effect_teardown@unlink": {"byte"},
    "fighter_18350@anchor": {"byte", "call #0 memory"},
    "fighter_18350@bit": {"byte"},
    "fighter_18350@neg": {"byte", "call #0"},
    "fighter_18350@side": {"byte", "call #0 memory"},
    "fighter_18350@tab": {"byte", "call #0 memory"},
    "fighter_18540@default": {"byte"},
    "fighter_18540@gt": {"byte"},
    "fighter_18540@idx": {"byte"},
    "fighter_18540@mask": {"byte"},
    "fighter_18540@noclamp": {"byte"},
    "fighter_18540@side": {"byte"},
    "fighter_38154@abs": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_38154@bd": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_38154@bit": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_38154@call": {"byte", "call #0", "call #1"},
    "fighter_38154@dir": {"byte", "call #0", "call #1"},
    "fighter_38154@flag": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_38154@mode8": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_38154@range": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_anim_triple@add": {"byte"},
    "fighter_anim_triple@idx": {"byte"},
    "fighter_anim_triple@o0": {"byte"},
    "fighter_anim_triple@o1": {"byte"},
    "fighter_anim_triple@o2": {"byte"},
    "fighter_anim_triple@shift": {"byte"},
    "hit_record_y@add": {"eax"},
    "hit_record_y@always": {"byte", "call #1", "call #2", "eax"},
    "hit_record_y@bit": {"call #0", "eax"},
    "hit_record_y@call": {"byte", "call #0", "call #1", "call #1 memory", "call #2", "eax"},
    "hit_record_y@rec": {"eax"},
    "list_insert_before@back": {"byte"},
    "list_insert_before@forward": {"byte"},
    "list_insert_before@head": {"byte"},
    "list_insert_before@link": {"byte"},
    "list_insert_before@prev": {"byte"},
    "set_dead@bit": {"byte", "call #0 memory", "call #1 memory", "call #2 memory"},
    "set_dead@clear": {"byte", "call #2 memory"},
    "set_dead@gate": {"byte", "call #0", "call #1", "call #2"},
    "set_dead@pal": {"call #0", "call #2"},
    "set_dead@pset": {"byte", "call #0"},
    "set_dead@render": {"byte"},
    "set_dead@table": {"byte", "call #0", "call #0 memory", "call #1", "call #2"},
    "snd_music_stop@al": {"eax"},
    "snd_music_stop@cc": {"byte", "call #1 memory"},
    "snd_music_stop@gate": {"call #0"},
    "snd_music_stop@play": {"byte", "call #1", "eax"},
    "snd_music_stop@stop": {"call #1"},
    "snd_music_stop@zero": {"byte", "call #0 memory", "call #1 memory"},
    "snd_sample_playing@al": {"byte", "call #1", "eax"},
    "snd_sample_playing@clear": {"byte", "call #1 memory"},
    "snd_sample_playing@driver": {"call #0", "eax"},
    "snd_sample_playing@scan": {"byte", "call #1"},
    "snd_sample_playing@status": {"byte", "eax"},
    "snd_sample_playing@wide": {"call #0", "eax"},
    "snd_sample_queue@armbuf": {"byte", "call #0", "call #1"},
    "snd_sample_queue@armq": {"byte", "call #0", "call #1", "call #2", "call #3", "call #4"},
    "snd_sample_queue@armst": {"byte", "call #1", "call #2"},
    "snd_sample_queue@candcmp": {"byte"},
    "snd_sample_queue@candmin": {"byte"},
    "snd_sample_queue@driver": {"byte", "call #0", "call #1", "eax"},
    "snd_sample_queue@loopbyte": {"byte"},
    "snd_sample_queue@order": {"byte"},
    "snd_sample_queue@pause": {"byte", "call #0", "call #1", "eax"},
    "snd_sample_queue@size": {"byte", "call #0", "call #1"},
    "snd_sample_queue@tailorder": {"call #0", "call #1", "call #2"},
    "snd_sample_queue@tailret": {"eax"},
    "snd_sample_queue@tailstop": {"call #0", "call #1", "call #2"},
    "snd_sample_stop@al": {"eax"},
    "snd_sample_stop@calls": {"call #1", "call #2"},
    "snd_sample_stop@clear": {"byte"},
    "snd_sample_stop@driver": {"byte", "call #0", "call #1", "call #2", "eax"},
    "snd_sample_stop@eq2": {"byte", "call #1", "call #2", "eax"},
    "snd_sample_stop@nolimit": {"byte", "call #3", "call #4", "call #5", "eax"},
    "snd_samples_stop_all@al": {"eax"},
    "snd_samples_stop_all@clr4": {"byte", "call #0 memory", "call #1 memory", "call #10 memory", "call #11 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory", "call #6 memory", "call #7 memory", "call #8 memory", "call #9 memory"},
    "snd_samples_stop_all@clrc": {"byte", "call #0 memory", "call #1 memory", "call #10 memory", "call #11 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory", "call #6 memory", "call #7 memory", "call #8 memory", "call #9 memory"},
    "snd_samples_stop_all@driver": {"byte", "call #0", "call #1", "call #2", "call #3", "eax"},
    "snd_samples_stop_all@order": {"call #1", "call #10", "call #11", "call #2", "call #4", "call #5", "call #7", "call #8"},
    "snd_samples_stop_all@skip2": {"call #1", "call #1 memory", "call #10", "call #11", "call #2", "call #2 memory", "call #3 memory", "call #4", "call #5", "call #6", "call #7", "call #8", "call #9"},
}
```

The exact case set that alone catches each mutant (measured on the prototype; the record's copy of
the test's table):

```
        ("actor_type_49444@bit", ['a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("actor_type_49444@child", ['a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("actor_type_49444@idx", ['a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("actor_type_49444@link", ['a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("actor_type_49444@reclr", ['a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("actor_type_49444@sign", ['a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
        ("anim_operand@adv", ['o0', 'o1', 'o14']),
        ("anim_operand@be8", ['o0', 'o14']),
        ("anim_operand@byte", ['o11']),
        ("anim_operand@deref", ['o5']),
        ("anim_operand@mask", ['o3']),
        ("anim_operand@op", ['o0', 'o1', 'o14']),
        ("anim_operand@scale", ['o7']),
        ("anim_operand@sel", ['o10', 'o6', 'o7', 'o8', 'o9']),
        ("anim_read_var@a45", ['a7']),
        ("anim_read_var@cswap", ['a10']),
        ("anim_read_var@opff", ['a14']),
        ("anim_read_var@pswap", ['a16', 'a17', 'a18', 'a19', 'a8']),
        ("anim_read_var@ret", ['a12']),
        ("anim_read_var@ring", ['a1']),
        ("anim_read_var@sx", ['a13', 'a2']),
        ("anim_read_var@w58", ['a6']),
        ("anim_write_var@bep", ['w12', 'w19', 'w20', 'w21', 'w9']),
        ("anim_write_var@child", ['w12', 'w13', 'w14']),
        ("anim_write_var@high", ['w15', 'w16']),
        ("anim_write_var@mask", ['w17', 'w18']),
        ("anim_write_var@radio", ['w0', 'w1', 'w17', 'w2']),
        ("anim_write_var@ringm", ['w1', 'w2']),
        ("anim_write_var@swap", ['w10', 'w11', 'w19', 'w20', 'w21', 'w9']),
        ("anim_write_var@w44", ['w10', 'w13', 'w7']),
        ("effect_teardown@buf", ['e4']),
        ("effect_teardown@flag", ['e0', 'e2', 'e3', 'e5']),
        ("effect_teardown@restore", ['e0', 'e1', 'e2', 'e3', 'e4', 'e5', 'e6', 'e7']),
        ("effect_teardown@tail", ['e0', 'e1', 'e2', 'e3', 'e4', 'e5', 'e6', 'e7']),
        ("effect_teardown@type1", ['e1']),
        ("effect_teardown@unlink", ['e0', 'e1', 'e2', 'e3', 'e4', 'e5', 'e6', 'e7']),
        ("fighter_18350@anchor", ['b3']),
        ("fighter_18350@bit", ['b1']),
        ("fighter_18350@neg", ['b0', 'b1', 'b2', 'b3', 'b4', 'b5', 'b6', 'b7', 'b8', 'b9']),
        ("fighter_18350@side", ['b1']),
        ("fighter_18350@tab", ['b1', 'b2', 'b3', 'b4', 'b6']),
        ("fighter_18540@default", ['c1', 'c2', 'c3', 'c4', 'c5', 'c6']),
        ("fighter_18540@gt", ['c9']),
        ("fighter_18540@idx", ['c12']),
        ("fighter_18540@mask", ['c12']),
        ("fighter_18540@noclamp", ['c0', 'c8', 'c9']),
        ("fighter_18540@side", ['c1']),
        ("fighter_38154@abs", ['d0', 'd11', 'd14', 'd7']),
        ("fighter_38154@bd", ['d0', 'd11', 'd15', 'd16', 'd7']),
        ("fighter_38154@bit", ['d0', 'd1', 'd11', 'd2', 'd4', 'd5', 'd7', 'd8']),
        ("fighter_38154@call", ['d0', 'd1', 'd11', 'd14', 'd7', 'd8']),
        ("fighter_38154@dir", ['d1', 'd14', 'd8']),
        ("fighter_38154@flag", ['d0', 'd11', 'd12', 'd13', 'd5', 'd7']),
        ("fighter_38154@mode8", ['d0', 'd11', 'd7']),
        ("fighter_38154@range", ['d0', 'd11', 'd12', 'd7', 'd8']),
        ("fighter_anim_triple@add", ['t1', 't2', 't4']),
        ("fighter_anim_triple@idx", ['t2', 't3', 't4']),
        ("fighter_anim_triple@o0", ['t0', 't1', 't2', 't3', 't4']),
        ("fighter_anim_triple@o1", ['t0', 't1', 't2', 't3', 't4']),
        ("fighter_anim_triple@o2", ['t0', 't1', 't2', 't3', 't4']),
        ("fighter_anim_triple@shift", ['t0', 't1', 't2', 't3', 't4']),
        ("hit_record_y@add", ['y1', 'y2', 'y3']),
        ("hit_record_y@always", ['y1', 'y3']),
        ("hit_record_y@bit", ['y0']),
        ("hit_record_y@call", ['y1', 'y2', 'y3']),
        ("hit_record_y@rec", ['y0']),
        ("list_insert_before@back", ['v0', 'v2']),
        ("list_insert_before@forward", ['v0']),
        ("list_insert_before@head", ['v0', 'v1']),
        ("list_insert_before@link", ['v0', 'v1', 'v2']),
        ("list_insert_before@prev", ['v0', 'v1', 'v2']),
        ("set_dead@bit", ['s0', 's1', 's2', 's3', 's4', 's5', 's6', 's7']),
        ("set_dead@clear", ['s3', 's4', 's5', 's7']),
        ("set_dead@gate", ['s3', 's4', 's5', 's7']),
        ("set_dead@pal", ['s1', 's6', 's7']),
        ("set_dead@pset", ['s6']),
        ("set_dead@render", ['s2']),
        ("set_dead@table", ['s4', 's7']),
        ("snd_music_stop@al", ['m2']),
        ("snd_music_stop@cc", ['m2']),
        ("snd_music_stop@gate", ['m0']),
        ("snd_music_stop@play", ['m1']),
        ("snd_music_stop@stop", ['m2']),
        ("snd_music_stop@zero", ['m0', 'm1', 'm2']),
        ("snd_sample_playing@al", ['q3', 'q5']),
        ("snd_sample_playing@clear", ['q3', 'q5']),
        ("snd_sample_playing@driver", ['q0']),
        ("snd_sample_playing@scan", ['q5']),
        ("snd_sample_playing@status", ['q2', 'q4']),
        ("snd_sample_playing@wide", ['q6']),
        ("snd_sample_queue@armbuf", ['z3', 'z9']),
        ("snd_sample_queue@armq", ['z4', 'z6', 'z8', 'z9']),
        ("snd_sample_queue@armst", ['z5', 'z6', 'z8']),
        ("snd_sample_queue@candcmp", ['z8']),
        ("snd_sample_queue@candmin", ['z6', 'z8']),
        ("snd_sample_queue@driver", ['z0']),
        ("snd_sample_queue@loopbyte", ['z2', 'z3', 'z4', 'z5', 'z6', 'z7', 'z8', 'z9']),
        ("snd_sample_queue@order", ['z8']),
        ("snd_sample_queue@pause", ['z1']),
        ("snd_sample_queue@size", ['z3', 'z4', 'z5', 'z6', 'z7', 'z8']),
        ("snd_sample_queue@tailorder", ['z3', 'z4', 'z5', 'z6', 'z8']),
        ("snd_sample_queue@tailret", ['z3', 'z4', 'z5', 'z6', 'z8']),
        ("snd_sample_queue@tailstop", ['z3', 'z4', 'z5', 'z6', 'z8']),
        ("snd_sample_stop@al", ['t3', 't4', 't5']),
        ("snd_sample_stop@calls", ['t3', 't4', 't5']),
        ("snd_sample_stop@clear", ['t3', 't4', 't5']),
        ("snd_sample_stop@driver", ['t0']),
        ("snd_sample_stop@eq2", ['t2', 't3', 't4', 't5']),
        ("snd_sample_stop@nolimit", ['t3', 't4', 't5']),
        ("snd_samples_stop_all@al", ['r1', 'r2']),
        ("snd_samples_stop_all@clr4", ['r1', 'r2']),
        ("snd_samples_stop_all@clrc", ['r1', 'r2']),
        ("snd_samples_stop_all@driver", ['r0']),
        ("snd_samples_stop_all@order", ['r2']),
        ("snd_samples_stop_all@skip2", ['r1']),
```

## §C3b.4 Counters, measured

| state | diff-verify counter | E2 |
|---|---|---|
| base `2cf874b` | `217/217 functions VERIFIED; 664/664 mutants detected; 1 named gaps; 154/172 rows with callees closed (45 have none)` | `233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted 30`; voice `0 / 115 / 19` |
| C3b prototype (seventeen rows) | `234/234 functions VERIFIED; 779/779 mutants detected; 1 named gaps; 165/185 rows with callees closed (49 have none)` | byte-identical |

The arithmetic: +17 functions, +115 mutants (the partial prototype's 74 for its eleven rows — 8+5+6
+5+8+6+6+6+6+5+13 — plus this session's 41 for its six: `effect_teardown` 6, `actor_type_49444` 6,
`set_dead` 7, `anim_write_var` 8, `anim_operand` 8, `fighter_anim_triple` 6), rows with callees
172 -> 185 (the thirteen new rows with callees are `fighter_18350`, `fighter_18540`, `hit_record_y`,
`fighter_38154`, `snd_music_stop`, `snd_sample_playing`, `snd_samples_stop_all`, `snd_sample_stop`,
`snd_sample_queue`, `effect_teardown`, `actor_type_49444`, `set_dead`, `anim_operand`; the other
four have none), no-callee 45 -> 49 (+4: `anim_read_var`, `list_insert_before`, `anim_write_var`,
`fighter_anim_triple`). Closed 154 -> 165 (+11): eight dependents the batch closes
(`fighter_slot_latch`, `hit_record_x` by `0x18350`+`0x18540`; `fighter_1883c` by `0x18788`;
`anim_next_sprite_id` by `0x29F34`; `fighter_state_36638` by `0x38154`; `effects_clear` by
`0x13420`; `fighter_4b03c` by `0x2B150`+`0x49444`; `sound_voice` by the five voice rows — all eight
of its audio stubs now verified) plus three of the batch's own rows (`fighter_18350`,
`actor_type_49444`, `anim_operand`); no regression.

`make entry-triage` is byte-identical (no ported function, no `fn_register`); `make diff-verify`
(the suite and the self-check) passes; `PR_ORACLE_REQUIRED=1 ./build/run_tests` prints `all checks
passed`; `python3 tools/port_progress.py` stays `771 1203 64` / `731 731 100`; `symbols.h`
regeneration is byte-identical.

## §C3b.5 Decisions, named gaps and limits

**No decision is left to the user.** The one shape question the C3.7 list raises is decided here for
C3c:

- **`0x1C390`/`0x1C3A0` (the row shape): two rows, reached by splitting the port function.** The raw
  has two entries: `0x1C390` (16 bytes, 1 caller) pops the free-list head `[0x10275C]` and returns
  it; `0x1C3A0` (47 bytes, 2 callers) is the stable layer splice. The port's `render_list_insert`
  runs both, plus a PORT-only `render_count++`, in one function, so it cannot bind either entry: a
  row run from `0x1C390` would pop without splicing (the port would splice), and one from `0x1C3A0`
  would splice without popping. C3c should split `render_list_insert` along the raw's seam into the
  pop and the splice (a coherent change: the porting rule asks one C function per original
  function), export `render_splice`, and add one row per entry; `render_list_insert` keeps its
  behavior as pop + count + splice. Neither row has callees (the pop reads `[0x10275C]`; the splice
  reads each pset's layer word).

Named gaps and limits:

- **`fighter_anim_triple`'s `0x62003` fatal**: EDX outside `[0,0x40)` runs the raw's fatal; the
  port's documented PORT deviation leaves the triple zeroed. The block `0x3AFCE` is `unhit_named`
  and no case can stop the original on a fatal. The row claims the in-range path only.
- **`effect_teardown`'s four callees are allows**: the effects.c list copies carry no seam (the C3
  constraint, §C3b.2 correction 2) and the two palette writers have no row; the comparison is the
  final bytes. The lock's set/restore ordering is not observed (no recorded call): the `@lock`
  mutant class is unobservable and dropped; `@restore` (lock left at 1) is the byte-observable
  variant.
- **`set_dead`'s callback is an allow**: the raw calls `DSD(0xBB9E0 + type*0xC)`; the port runs its
  registered C body when `fn_resolve` finds it and skips the unregistered `0x5D812` stub (whose
  `xor eax,eax; ret` has no state effect). The callback's own `0x249D0`/`0x249B0` calls are in the
  call set (`real`), so their arguments and order are checked. The `0x1C458`+`0x1C3D0` render pair
  is allowed (the port's `render_list_remove` is the same body).
- **The AIL wrapper stubs pass no arguments**: the original's `mem[]` DIG handle and the port's host
  pointer cannot be compared, so the `E.Call` entries record none (`ail.c`'s wrappers take a host
  pointer, not a `mem[]` offset); the stub's effect is the return value and the poisoned clobbers.
- **`snd_sample_queue`'s `0x500BB` and `0x1B544` are allows**: the DPMI clock read and the host
  resource resolve; the row compares the port's `DS_00101500` seed against the original's read (the
  fixture pins it) and the resolver's preloaded result. **The raw's clock reads are not
  one-for-one**: the raw calls `0x500BB` at four sites — `0x1CC51` (the entry `now`, the candidate
  scan's base) and the store arms `0x1CCA7`, `0x1CD06`, `0x1CD79` (two calls in any one run: the
  entry read plus one arm), so the stored `+0x14` can hold a tick later than the `now` it compares
  against; the port reads the per-frame `DS_00101500` word at the same four sites, and the allow's
  body is the raw `mov eax,[0x81500]; ret` at `0x500BB` over the fixture's word, which cannot
  advance mid-run — both sides store the value they scanned, so the shape is unobservable in the
  harness.
- **Masks** (`C3B_MASKS` in the test): `fighter_18350`, `fighter_18540`, `fighter_38154`,
  `list_insert_before`, `effect_teardown`, `actor_type_49444`, `set_dead`, `anim_write_var` and
  `fighter_anim_triple` 0 (their callers ignore EAX); the five `snd_*` rows and `snd_sample_queue`
  0xFF (AL); `anim_operand` 0xFFFF (its own returns mask `and eax,0xffff`, 0x2B9BC/0x2BAE1);
  `anim_read_var` 0xFFFF (the callers read 16 bits — `and eax,0xffff` at 0x2A49C, `add ecx,eax` at
  0x2A480 with only ECX's low word surviving, the `mov ax,cx` returns 0x2A4E4/0x2A4F1 and `test
  ax,ax` at 0x2AAD1 — while its pool arms write only AX on the 32-bit base of 0x29FBC..0x29FD8,
  keeping the base's high word in EAX);
  `hit_record_y` full (the only caller reads the dword).
- **`0x2EA30` and the host/bare names**: never rowed (the interrupt-lock counter has no C function;
  `0x1B544`, `0x5D812`, `0x29D60`, `0x2EA64` are host/bare; `0x500BB` and
`0x5DC0F`/`0x5DC8B`/`0x5DD03`/`0x5DEAF` are host DPMI/AIL). They stay named allows.
- E3's, P1-P8's and C1-C3's limits stand: seeds are hand pokes; the memory at a call is `mem[]`
  only; the callee column is one level deep.

## §C3b.6 What the planner ran

- The baseline on the clean `2cf874b` worktree: `make diff-verify` -> the §C3b.4 base counter and
  table; `make entry-triage` -> `233/262`; `python3 tools/port_progress.py` -> `771 1203 64` /
  `731 731 100`.
- The prototype, in this worktree: the previous prototype's patch was applied, built, and its
  eleven rows re-measured (`--function NAME --self-check` each), then six rows were derived from
  the raw and added family by family (the teardown, the type-0x20..0x25 teardown, the hub, the two
  anim-variable functions, the anim triple). Each row was measured with
  `python3 tools/diff_verify.py --function NAME --self-check` until VERIFIED with every mutant
  detected, then the full `python3 tools/diff_verify.py --self-check` (§C3b.4 counter), the
  `python3 -m unittest tools.tests.test_diff_verify` suite (106 tests OK, including the new
  `test_each_c3b_mutant_is_caught_by_what_it_breaks` and the extended clobber exact-set),
  `make entry-triage` (byte-identical), `PR_ORACLE_REQUIRED=1 ./build/run_tests` (all checks
  passed) and a `symbols.h` regeneration (byte-identical). Then `git checkout -- port tools`,
  leaving only this record and the plan.
- The prototype's own corrections (none a port correction; two fixture/draft fixes and one
  compile fix): §C3b.2's corrections 1-3.
- The full `make verify` (~25 min) was not run by the planner: the brief's task-scoped gates are
  the ones above. The `port/src` changes are seams and exports, inert outside `build/diffrun`; the
  executor runs the full gate in Task 4.
- The prototype's diff (2344 lines, 2089 insertions over 10 files, the exact files the plan's
  Task 2 applies) is embedded in the plan.

## §C3b.7 The C3c deferral (the rest of the frontier, with evidence)

This session measured seventeen of the 38 candidate addresses (36 + the two C3 items). The remaining
**21 candidate addresses**, plus the **9 new frontier items these rows create**, are **the next
batch (C3c)**, same recipe. The deferral is a session-budget split, not a correctness judgement:
every address below is ported and its port function is named (measured from the final prototype
table; sizes are `port/decomp/prage.functions.csv` bytes, callers are the raw's `n_callers`):

| family | addresses (size bytes / raw callers / port function) |
|---|---|
| the type family | `0x1A734` (151/1, `fighter_block_hit`), `0x1AB5C` (302/1, `fighter_input_mask`), `0x2A820` (589/3, `pset_write`), `0x2BD44` (89/4, `fighter_2bd44`), `0x36BC8` (282/3, `fighter_state_36bc8`), `0x379C4` (147/1, `fighter_379c4`), `0x385B0` (384/1, `fighter_385b0`), `0x39040` (566/2, `fighter_39040`), `0x392A0` (907/4, `fighter_392a0`), `0x39738` (250/1, `fighter_39738`), `0x3AAFC` (667/1, `fighter_reaction_apply`), `0x3AD98` (260/3, `fighter_3ad98`), `0x3AE9C` (294/1, `fighter_3ae9c`), `0x3B080` (177/4, `fighter_3b080`), `0x3B134` (355/2, `fight_command_map`), `0x3B6C4` (77/1, `fighter_3b6c4`), `0x3C520` (77/7, `hit_anim_start_c`), `0x4F434` (178/1, `fighter_4f434`) |
| the allowed tree remainder | `0x127C0` (not in the function index, the type callback's allow target), `0x1C390`/`0x1C3A0` (16/1 and 47/2, the row-shape decision above) |
| new items this batch creates | `0x367DC` (128/4, `fighter_state_367dc`; allow of `fighter_38154`), `0x1CA40` (43/2, `snd_music_playing`; stub of `snd_music_stop`), `0x33714` (31/1, `palette_record_flagged`; allow of `effect_teardown`), `0x33734` (31/6, `palette_record`; allow of `effect_teardown`), `0x1C458` (21/1, `render_list_remove`'s find half; allow of `set_dead`), `0x1C3D0` (42/1, `render_list_remove`'s unlink half; allow of `set_dead`), `0x12800` (not in the function index: the `0xBB9E0` table target, 40 bytes; allow of `set_dead`), `0x18428` (27/1, the bare-RET dispatch; allow of `fighter_18540`/`hit_record_y`), `0x18460` (193/1, `fighter_18460`; allow of `fighter_18540`/`hit_record_y`) |

The named non-rows that stay unrowed (never a C function or a host/library body): `0x2EA30` (the
inert lock), `0x1B544` (host `res_resolve`), `0x5D812`/`0x29D60`/`0x2EA64` (bare stubs/rets),
`0x500BB` (the DPMI clock), `0x5DC0F`/`0x5DC8B`/`0x5DD03`/`0x5DEAF` (the AIL host wrappers,
runtime-region addresses the port serves from the host mixer). Their presence keeps the rows that
name them open under the one-level rule (below); that is by design, not a backlog.

Closure value still on the table (prototype table, the VERIFIED rows with unverified callees):
`fighter_36870` needs the type family `0x36BC8`/`0x379C4`/`0x385B0`/`0x39040`/`0x3C520`;
`fighter_39834` needs `0x392A0`/`0x39738`/`0x4F434`; `fighter_command_dispatch` needs
`0x1A734`/`0x1AB5C`/`0x3B134`; `fighter_reaction` needs
`0x2BD44`/`0x3AAFC`/`0x3AD98`/`0x3AE9C`/`0x3B080`/`0x3B6C4`; `actor_spawn` needs
`0x127C0`/`0x2A820`/`0x1C390`/`0x1C3A0` (plus the named `0x5D812`); `effect_teardown` needs
`0x33714`/`0x33734`; `fighter_18540`/`hit_record_y` need `0x18428`/`0x18460`; `fighter_38154` needs
`0x367DC`; `snd_music_stop` needs `0x1CA40` (and the named `0x5DEAF`); `set_dead` needs
`0x12800`/`0x1C458`/`0x1C3D0` (and the named `0x5D812`); the rows blocked by the permanent names
(`actor_alloc`/`release_record` by `0x2EA30`, `effects_spawn`/`palette_acquire` by `0x1B544`,
`spawn_anim_opcode` by `0x29D60`/`0x2EA64`, the `snd_*` rows by the AIL names) cannot close under
the rule and stay open deliberately.

The recommended C3c order (dependency-first):
1. the split of `render_list_insert` and the `0x1C390`/`0x1C3A0` rows (unblocks `actor_spawn`);
2. the type-family rows (`0x1A734` `0x1AB5C` `0x2A820` `0x2BD44` `0x36BC8` `0x379C4` `0x385B0`
   `0x39040` `0x392A0` `0x39738` `0x3AAFC` `0x3AD98` `0x3AE9C` `0x3B080` `0x3B134` `0x3B6C4`
   `0x3C520` `0x4F434`), which close the four fighter rows;
3. the new items (`0x367DC` `0x1CA40` `0x33714` `0x33734` `0x1C458` `0x1C3D0` `0x12800`
   `0x18428`/`0x18460`) and `0x127C0`.

`make k11-oracle`-style gates do not apply: this batch touches no gameplay path (only `port/src`
seam/export changes), so the gp miss sets are untouched; Task 1's `PR_GP_DUMP` pinned sets are the
executor's check that they stay so.

## §C3b.8 Results (the executed tree)

The plan's Tasks 2-4 were executed on `reverse-c3b` at the base `main` `2cf874b`: the plan+record
commit `e7ce2da`, Task 2 `2ec8522` (the seventeen rows, the exports, the four AIL seams and 115
mutants), Task 3 `d5d2016` (the review sweep: the `fighter_38154` `+0x43` sentinel and the
`fighter_38154@bit` re-pin) and the fix wave `c55c758` (the `anim_read_var` mask prose), then this
closure commit. Every row was re-measured in the tree; the planner's prototype values held.

The counters on the final tree equal §C3b.4's prototype row:

| state | diff-verify counter | E2 |
|---|---|---|
| base `2cf874b` | `217/217 functions VERIFIED; 664/664 mutants detected; 1 named gaps; 154/172 rows with callees closed (45 have none)` | `targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30`; voice `0 / 115 / 19` |
| final (`c55c758` + this closure commit) | `234/234 functions VERIFIED; 779/779 mutants detected; 1 named gaps; 165/185 rows with callees closed (49 have none)` | byte-identical |

The batch's task gates, measured on this tree (Task 1's baseline in `verify-base.log`; Task 2's and
the fix wave's re-measures in the ledger directory):

```
diff-verify: 234/234 functions VERIFIED; 779/779 mutants detected; 1 named gaps; 165/185 rows with callees closed (49 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
```

`make entry-triage` is byte-identical (no ported function, no `fn_register`); `python3 -m unittest
tools.tests.test_diff_verify` 106 tests OK (the clobber re-derivation, the C3b case-set test, the
counter line); `PR_ORACLE_REQUIRED=1 ./build/run_tests` `all checks passed`; `python3
tools/port_progress.py` stays `771 1203 64` / `731 731 100`; `symbols.h` regeneration is
byte-identical; README untouched.

Task 3's measured change after §C3b.3's table: the fixed fixture seeds `slot+0x40..0x43 = 40 41 42
00`, so the `0x38221 mov byte [esi+0x43],bl` store is observed in all six cases that reach it and
`fighter_38154@bit` is alone caught by `['d0','d1','d11','d15','d16','d2','d3','d4','d5','d7','d8']`
(§C3b.3's copy shows the pre-fix set; the committed test carries the re-pin). The fix wave's
`anim_read_var` correction is in §C3b.5's masks bullet.

**The full gate** on this closure commit (the plan's parallel-safe overrides, log
`/tmp/pr_c3b_final.log`): the run's lines are recorded in the batch report
`.superpowers/sdd/2026-10-05-reverse-c3b-frontier-rows-2/task-4-report.md`; the plan's expected
values are `EXIT=0`, the 45 oracle lines equal to the k7-k12 baseline (`ORACLES-EQUAL`), `make
audio-render` `cmp`-equal to `before-t2.wav` (`WAV-SAME`; both sha256
`df74acfb65d345fb72cb214102089f2a0ab8d4b271ddc17e2a5f5c4f1a380844`) and every gp ratchet at its pin
(Task 1's list verbatim).

**The C3c deferral is the next batch**: §C3b.7's 21 remaining candidates, this batch's nine new
frontier items (`0x367DC` `0x1CA40` `0x33714` `0x33734` `0x1C458` `0x1C3D0` `0x12800`
`0x18428`/`0x18460`) and the named non-rows (`0x2EA30`, `0x1B544`, `0x5D812`, `0x29D60`, `0x2EA64`,
`0x500BB` and the four AIL wrappers), in §C3b.7's dependency-first order; nothing else is left open
by C3b.
