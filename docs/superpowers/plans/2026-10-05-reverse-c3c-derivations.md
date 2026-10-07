# Reverse completion C3c: the frontier rows, part 3 (record)

**Scope.** Track P's sixth verification-only batch (roadmap row C3c of record
`2026-10-05-reverse-c3-derivations.md` §C3.7, the rest of the frontier per record
`2026-10-05-reverse-c3b-derivations.md` §C3b.7): the render-list seam split `0x1C390`/`0x1C3A0`,
the four render-list halves, the type-0x01 callbacks `0x127C0`/`0x12800`, and the new frontier
items the C3b rows created (`0x367DC`, `0x1CA40`, `0x33714`, `0x33734`, `0x1C458`, `0x1C3D0`,
`0x12800`, `0x18428`, `0x18460`) — plus this session's measured slice of the type family
(`0x2BD44`, `0x3B6C4`, `0x3C520`, `0x1A734`, `0x39738`). **Seventeen rows, 95 mutants, all
detected.** The remaining thirteen type-family rows are **C3d** (§C3c.7). One raw-over-port
correction (`0x39738`, §C3c.2). Plan: `2026-10-05-reverse-c3c-frontier-rows-3.md`. Recipe: E3
record §E3.10; checklist: `.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**Status of the numbers.** Measured by the planner on 2026-10-06 on `reverse-c3c` at the base
`main` `bb14036` (= C3b merged), in this worktree: the baseline gates and a full prototype of the
seventeen rows (every gate run, then reverted). The image is `build/diffrun --exe
data/game/C/PRAGE.EXE --image-out FILE`, sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's,
E3's, P1-P8's, C1-C3b's). Every address below is capstone 5.0.7 over that image (fixups applied);
`unicorn` 2.1.4 runs the original side. Ghidra was not consulted. The prototype diff (1600 lines,
1314 insertions over ten files) is embedded in the plan; the prototype was reverted
(`git checkout -- port tools`), leaving only this record and the plan.

---

## §C3c.1 The members, re-derived from the raw

The C3b record §C3b.7 defers 21 candidate addresses + the 9 new items its rows created. This
session measured **17** of them; the other **13** are C3d (§C3c.7):

| family | measured this session (17) | deferred to C3d (13) |
|---|---|---|
| the render seam | `0x1C390` `0x1C3A0` (the `render_list_insert` split), `0x1C458` `0x1C3D0` (the `render_list_remove` split) | — |
| the type-0x01 callbacks | `0x127C0` `0x12800` | — |
| the new items | `0x367DC` `0x1CA40` `0x33714` `0x33734` `0x12800` `0x18428` `0x18460` (and `0x1C458`/`0x1C3D0`, above) | — |
| the type family | `0x2BD44` `0x3B6C4` `0x3C520` `0x1A734` `0x39738` | `0x1AB5C` `0x2A820` `0x36BC8` `0x379C4` `0x385B0` `0x39040` `0x392A0` `0x3AAFC` `0x3AD98` `0x3AE9C` `0x3B080` `0x3B134` `0x4F434` |

Membership was re-checked against the final base table (the `unverified` callees of the C3b
prototype table) and the raw's call scans (capstone over the image; the original addresses of the
deferred rows in §C3c.7). No correction to the C3b membership.

The measured rows (sizes from `port/decomp/prage.functions.csv`; `n_callers` are the raw's):

| row | entry | size | the raw's direct callees | row design |
|---|---|---|---|---|
| `render_pop_free` | `0x1C390` | 16 | none | the free-list head pop `[0x10275C]`; EAX = the popped node; the raw dereferences the new head unconditionally (no empty case) |
| `render_splice` | `0x1C3A0` | 47 | none | EAX = headp, EDX = node; stable splice before the first greater pset+0xE layer |
| `render_find` | `0x1C458` | 21 | none | EAX = headp, EDX = pset; the node whose +4 matches, or 0 |
| `render_unlink` | `0x1C3D0` | 42 | none | EAX = headp, EDX = node; unlink + free-list push; absent node no-op |
| `actor_type_127C0` | `0x127C0` | 64* | `249D0` `249B0` (real) | the 0xF0A78 sentinel check, the pop, node+8 = rec, rec+0x14 = node, insert after 0xF0AE0; 0xFF when empty |
| `actor_type_12800` | `0x12800` | 44* | `249D0` `249B0` (real) | rec+0x14 non-zero: unlink, insert after 0xF0A78, clear rec+0x14 |
| `fighter_18428` | `0x18428` | 39 | none | slot+0x7A read + the all-RET 0x1840C dispatch; **effect-free, zero mutants** (§C3c.5) |
| `fighter_18460` | `0x18460` | 193 | `18428` (real) | both slot pointers live; ch 0..6 range pair 0xA1774/76 + ch*14; the sprite id & 0x7FFF; 0x1E1; else 0x18428(side, id, a0, a1) |
| `fighter_state_367dc` | `0x367DC` | 128 | `2BC30` ×2 (stub) | the +0x53 == 7 reset; stream 0xC8950[slot+0x7A] at 3.0; the mode != 3/0x22/0x24 second animation (0x102900[rec+0x51], 0xE906A, 1.0) |
| `snd_music_playing` | `0x1CA40` | 43 | `5DEED` (stub) | `[0x1028C0] == 0` -> 0; else the AIL status == 4, AL-masked |
| `palette_record_flagged` | `0x33714` | 31 | none | the append at `[0x107798]` with the flag byte 1 |
| `palette_record` | `0x33734` | 31 | none | the same append with flag 0 |
| `fighter_2bd44` | `0x2BD44` | 89 | `2A408` `2B150` (stub) | the +0x4B copy, the re-arm stores, +8 = 0x1E1, the actor word = 0x2A408's low word |
| `fighter_3b6c4` | `0x3B6C4` | 77 | `33950` (allow), `1A570` ×2 (real) | the +0x53 == 8 / +0x54 == 2 / other +0x54 != 2 gates and the two bit-15 predicates agreeing |
| `hit_anim_start_c` | `0x3C520` | 77 | `339AC` (allow), `2BC30` `188DC` `1890C` (stub) | the ctx from rec+0x51; 2BC30(rec, stream, s0); 0x188DC(side, slot+0x2C); 0x1890C(side, slot+0x30) |
| `fighter_block_hit` | `0x1A734` | 151 | `33A10` (allow), `18B04` `3C480` (stub) | the +0x61 = 0x0C clamp against +0x60 gated by +0x62; +0x43 bit 0x20/0x10 pick +0x54 0/1 and 0xC8F40/0xC8F90[ch]; 0x3C480(rec, stream, 3.0) |
| `fighter_39738` | `0x39738` | 147 | `33950` (allow) | the 0xBEC28/0xBEC58 tables (0xBEC54/0xBEC84 at k > 0xB) scaled by b/100; the slot+0x8C first path returns immediately (raw 0x39789 `jmp 0x3982C`, §C3c.2); else the other +0x8C 15% cut and the +0x63 difficulty multiplier |

\* `0x127C0`/`0x12800` are not in `prage.functions.csv` (§C3b.7's evidence); the sizes are their
E2 spans (capstone scan: 0x127C0..0x12800 = 64 and 0x12800..0x1282C = 44; the code itself ends at
0x127FE and 0x12829, the rest alignment).

The callees these rows surface are **all rowed or named**: every call target is a verified row
except `0x5DEED` (the AIL `AIL_sequence_status`, a host wrapper at 0x5DEED — a **named non-row**,
like the C3b AIL names) and the allows `0x33950`/`0x339AC`/`0x33A10` (all three have rows). So
this batch creates **no new frontier items**.

## §C3c.2 The rows: cases, seams, fixtures and the corrections

Each row is a `Spec` in `tools/diff_verify.py` (`C3C_SPECS`), a binding `b_*` and mutants `m_*` in
`port/tests/diff_runner.c` (`k_bindings`), and the exact-set extensions in
`tools/tests/test_diff_verify.py` (`C3C_MASKS`, `C3C_KINDS`, the case-set table, the clobber table,
the counter line). The measured rows:

| row | entry | cases | blocks | mutants | callees (each by its own check) | EAX mask |
|---|---|---|---|---|---|---|
| `render_pop_free` | 0x1C390 | 3 | 1/1 | 3/3 | - | 0xFFFFFFFF |
| `render_splice` | 0x1C3A0 | 6 | 5/5 | 5/5 | - | 0x0 |
| `render_find` | 0x1C458 | 4 | 5/5 | 4/4 | - | 0xFFFFFFFF |
| `render_unlink` | 0x1C3D0 | 5 | 5/5 | 5/5 | - | 0x0 |
| `actor_type_127C0` | 0x127C0 | 3 | 6/6 | 6/6 | `249B0` real VERIFIED, `249D0` real VERIFIED | 0xFF |
| `actor_type_12800` | 0x12800 | 3 | 3/3 | 5/5 | `249B0` real VERIFIED, `249D0` real VERIFIED | 0x0 |
| `fighter_18428` | 0x18428 | 4 | 3/3 | 0/0 | - | 0x0 |
| `fighter_18460` | 0x18460 | 11 | 10/10 | 8/8 | `18428` real VERIFIED | 0x0 |
| `fighter_state_367dc` | 0x367DC | 5 | 5/5 | 8/8 | `2BC30` stub VERIFIED | 0x0 |
| `snd_music_playing` | 0x1CA40 | 4 | 3/3 | 6/6 | `5DEED` stub unverified (named) | 0xFF |
| `palette_record_flagged` | 0x33714 | 2 | 1/1 | 5/5 | - | 0x0 |
| `palette_record` | 0x33734 | 2 | 1/1 | 5/5 | - | 0x0 |
| `fighter_2bd44` | 0x2BD44 | 2 | 1/1 | 7/7 | `2A408` stub VERIFIED, `2B150` stub VERIFIED | 0x0 |
| `fighter_3b6c4` | 0x3B6C4 | 6 | 6/6 | 6/6 | `1A570` real VERIFIED, `33950` allow VERIFIED | 0xFF |
| `hit_anim_start_c` | 0x3C520 | 3 | 1/1 | 6/6 | `188DC` stub VERIFIED, `1890C` stub VERIFIED, `2BC30` stub VERIFIED, `339AC` allow VERIFIED | 0x0 |
| `fighter_block_hit` | 0x1A734 | 6 | 9/9 | 8/8 | `18B04` stub VERIFIED, `33A10` allow VERIFIED, `3C480` stub VERIFIED | 0x0 |
| `fighter_39738` | 0x39738 | 8 | 17/17 | 8/8 | `33950` allow VERIFIED | 0xFFFFFFFF |

Total: 17 rows, 95 mutants. Every block of every row is exercised; no named unhit blocks and no
`reads outside the image` except `fighter_18428`'s h3 (a null slot reads the emulator's zero page
at 0x7A; the port reads `mem[0x7A]` the same way; pinned in `P45_OUTSIDE["fighter_18428"] =
[(122, 1)]`).

**The `0x1C390`/`0x1C3A0` and `0x1C458`/`0x1C3D0` splits (the C3b §C3b.5 decision executed).**
The port's `render_list_insert` ran the raw's two entries (pop + splice) and a PORT-only count; it
could bind neither. The split: a new `render_pop_free` (`0x1C390`, the `/* 0x1C390 — record §50-D`
header), `render_splice` (`0x1C3A0`) un-`static`ed and declared in `render.h`,
`render_list_insert` = `render_pop_free()` + its PORT pool guard + the splice + count; the
`render_list_remove` body split into `render_find` (`0x1C458`) and `render_unlink` (`0x1C3D0`),
with `render_list_remove` kept as the find + unlink + count wrapper the actor callers use. Both
rows are behavior-neutral refactors of listed port functions; no existing row moved. `port_progress`
stays `771 1203 64` **because `0x1C390` was already counted**: `actors.c:3877`'s call-site comment
`/* 0x1C390 + 0x1C3A0 */` matches the counter's `/\* 0xADDR` regex (evidence run: before and after
the prototype both print `771 1203 64`; `0x1C3A0` was already counted through `render.c`'s
`/* 0x1C3A0 — record §50-D` header). E2 is byte-identical (no E2 candidate: 0x1C390/0x1C3A0 are
absent from `2026-10-01-reverse-e2-live-functions.txt`).

**Seams and exports** (`port/src`, each seam a first statement; `E.callee_clobbers` re-derives every
declared clobber):

| function | address | change | conformance |
|---|---|---|---|
| `render_pop_free` | `0x1C390` | new; declared in `render.h` | no callees |
| `render_splice` | `0x1C3A0` | loses `static`; declared in `render.h` | no callees |
| `render_find` | `0x1C458` | new; declared in `render.h` | no callees |
| `render_unlink` | `0x1C3D0` | new; declared in `render.h` | no callees |
| `actor_type_127C0` | `0x127C0` | loses `static` (+ its forward declaration); declared in `actors.h` | `249D0`/`249B0` real |
| `actor_type_12800` | `0x12800` | loses `static` (+ its forward declaration); declared in `actors.h` | `249D0`/`249B0` real |
| `list_insert_after` / `list_unlink` | `0x249B0`/`0x249D0` | lose `static`; declared in `actors.h` **for the C3c mutant cores only** (the seams stay the only observers) | their own rows carry the bodies |
| `fighter_18428` | `0x18428` | loses `static`; gains `PR_SEAM(0x18428u, side, sprite, a0, a1)`; declared in `fighter.h` | no callees |
| `AIL_sequence_status` | `0x5DEED` | gains `PR_SEAM_RET0(0x5DEEDu)` | clobbers `()` (image); the handle is a host pointer, so the stub records no args (the C3b AIL treatment) |

No seam on `fighter_39738`'s `0x33950`, `hit_anim_start_c`'s `0x339AC`, `fighter_block_hit`'s
`0x33A10` (allows: run on both sides, not recorded, each with its own row), and none on the
`fighter_18460` → `fighter_18428` call (real, recorded through 18428's own seam).

**Corrections during prototyping (each recorded here with its evidence):**

1. **`fighter_39738` was a raw-over-port correction (the batch's one).** The port applied the
   other slot's 15% cut (`0x397DB..0x397FC`) and the `+0x63` difficulty multiplier
   (`0x397FE..0x39828`) to both paths. The raw's first path (slot+0x8C non-zero, taken at
   `0x3975B`) ends `jmp 0x3982C` at **0x39789** — straight to the epilogue, before both
   adjustments. Evidence: the original's executed blocks on case r6 end
   `0x39774, 0x3977A, 0x3977D, 0x39782, 0x39784, 0x39787, 0x39789, 0x3982C` (no 0x397DB/0x397FE
   blocks), and the original returns `0xFFFFDFF5` = the base `0xBEC84` value 0x200B negated and
   scaled, not the cut/multiplied value the port returned (`0xFFE8574C`). The port now returns the
   divided value from the first path (`return (s32)((u32)v * (u32)b) / 100;` at the `0x39789`
   comment). Every case and all 8 mutants VERIFIED after the fix.
2. **Fixture corrections (draft errors, no port change):**
   - `fighter_2bd44`'s first draft used `param_1 = 0x10AF00` / `param_2 = 0x10AF40`, 0x40 apart
     against 0x68-byte records: `param_2 + 8`'s dword store (0x10AF48..0x10AF4B) overwrote
     `param_1 + 0x4B` (0x10AF4B), so `@src` agreed and the row measured 6/7. `param_2` moved to
     0x10B000 (records disjoint) → 7/7.
   - `fighter_18460`'s first draft set the wrong slot's `+0x7A` (`C3C_R_N2` for the side-0 slot)
     and never set the slot's rec pointer, so the range test read the image's bytes and block
     `0x184F4` stayed unhit (9/10) with five mutants undetected. The `c3c_46_case` helper now
     seeds both slot pointers, both records (`+0x56` index words), the 0xA1774/0xA1776 pair for
     the case's ch and the actor words; 11 cases, 10/10, 8/8.
   - `palette_record_flagged`/`palette_record`'s first draft seeded the flag byte's neighbours
     (`+13..15`) at the next record's addresses, so `@wide` (a dword store of the flag) agreed;
     the seeds moved to `head+0xA5..0xA7` (v0/w0) and `head+0x545..0x547` (v1/w1) → 5/5 each.
   - `actor_type_127C0` t2 and `actor_type_12800` t1/t2's first drafts used the same address for a
     node and its list sentinel (self-unlink, an unobservable no-op); the sentinels moved to the
     other scratch nodes → 6/6 and 5/5.
   - `render_find`'s first draft reused the splice helper with 0x2222-style pset *values*, whose
     `pset + 0x0E` layer write fell outside the image (`poke 0x2230+2 is outside the image`); the
     find cases write node+0/+4 directly (the row reads no layer) → 4/4.
   - `fighter_39738`'s first draft poked both 12-entry tables in every case (31 pokes), over the
     harness's 16-pokes-per-case cap (`diffrun: cannot parse 'poke'` after the 16th); the helper
     now pokes only the entries the case (or a mutant) reads: its branch's table entry, the
     fallback entry 11, the cross entries `@K`/`@TAB` read, and the two k words. No harness change.
3. **No other raw-over-port correction.** Every other row matched on its first measurement of
   this tree (after the fixture corrections above).

**Fixtures.** The rows reuse the C3b scratch (`0x10AF00`/`0x10AF40`/`0x10AF80`/`0x10AFC0`/
`0x10B000`), the seeded globals (`0x1077B0`+side*0x94 slots, `0x10275C`, `0x105B44`,
`0x107798`/`0x107498`, `0x1028C0`, `0x1014EC`, `0x102900`, `0x104B00`, `0x107D2A`/`0x107D1E`,
`0x1082C8`, `0xBEC28`/`0xBEC58`/`0xBEBD8`, `0xF0A78`/`0xF0AE0`, `0x1078F0`), and a new
`c3c_46_case` helper for `0x18460`.

## §C3c.3 The mutants

95 mutants, all detected (`python3 tools/diff_verify.py --self-check`: `874/874`); what alone
catches each is pinned by `test_each_c3c_mutant_is_caught_by_what_it_breaks` (`C3C_KINDS` plus the
measured case-set table; the exact dicts are the ones committed in
`tools/tests/test_diff_verify.py`). The kinds (measured; the record's copy of the test's
`C3C_KINDS`):

```
C3C_KINDS = {
    "actor_type_127C0@at": {'call #1', 'byte'},
    "actor_type_127C0@empty": {'call #1', 'byte', 'call #0', 'eax'},
    "actor_type_127C0@link": {'byte', 'call #1 memory'},
    "actor_type_127C0@rec": {'byte', 'call #1 memory'},
    "actor_type_127C0@ret": {'eax'},
    "actor_type_127C0@unlink": {'call #1', 'call #0 memory', 'byte', 'call #0'},
    "actor_type_12800@at": {'call #1', 'byte'},
    "actor_type_12800@clear": {'byte'},
    "actor_type_12800@node": {'call #1', 'byte', 'call #0'},
    "actor_type_12800@unlink": {'call #1', 'byte', 'call #0'},
    "actor_type_12800@zero": {'call #1', 'call #0'},
    "fighter_18460@call": {'call #0'},
    "fighter_18460@e1": {'call #0'},
    "fighter_18460@hi": {'call #0'},
    "fighter_18460@id": {'call #0'},
    "fighter_18460@lo": {'call #0'},
    "fighter_18460@null": {'call #0'},
    "fighter_18460@null2": {'call #0'},
    "fighter_18460@range": {'call #0'},
    "fighter_2bd44@a28": {'call #0 memory', 'byte', 'call #1 memory'},
    "fighter_2bd44@a2a": {'call #0 memory', 'byte', 'call #1 memory'},
    "fighter_2bd44@clr24": {'call #0 memory', 'byte', 'call #1 memory'},
    "fighter_2bd44@id": {'call #0 memory', 'byte', 'call #1 memory'},
    "fighter_2bd44@idx": {'byte', 'call #1 memory', 'call #0'},
    "fighter_2bd44@o29": {'call #0 memory', 'byte', 'call #1 memory'},
    "fighter_2bd44@src": {'call #0 memory', 'byte', 'call #1 memory'},
    "fighter_39738@chan": {'eax'},
    "fighter_39738@cut": {'eax'},
    "fighter_39738@diff": {'eax'},
    "fighter_39738@div": {'eax'},
    "fighter_39738@idx": {'eax'},
    "fighter_39738@k": {'eax'},
    "fighter_39738@k2": {'eax'},
    "fighter_39738@tab": {'eax'},
    "fighter_3b6c4@cmp": {'eax'},
    "fighter_3b6c4@o54": {'call #1', 'call #0', 'eax'},
    "fighter_3b6c4@ret": {'eax'},
    "fighter_3b6c4@s53": {'call #1', 'call #0', 'eax'},
    "fighter_3b6c4@s54": {'call #1', 'call #0', 'eax'},
    "fighter_3b6c4@side": {'call #1', 'call #0', 'eax'},
    "fighter_block_hit@arm": {'call #1', 'call #0 memory', 'byte', 'call #0'},
    "fighter_block_hit@b62": {'call #1', 'call #0 memory', 'byte', 'call #0'},
    "fighter_block_hit@c61": {'call #1', 'call #0 memory', 'byte', 'call #0'},
    "fighter_block_hit@call": {'call #1', 'call #0'},
    "fighter_block_hit@clamp": {'call #1', 'call #0 memory', 'byte', 'call #0'},
    "fighter_block_hit@s54": {'call #1', 'call #0 memory', 'byte', 'call #0'},
    "fighter_block_hit@tab": {'call #1', 'call #0 memory', 'call #0'},
    "fighter_block_hit@u": {'call #1', 'call #0 memory', 'call #0'},
    "fighter_state_367dc@call": {'call #0'},
    "fighter_state_367dc@ch": {'call #0'},
    "fighter_state_367dc@clr4c": {'byte', 'call #1 memory'},
    "fighter_state_367dc@clr53": {'byte', 'call #1 memory'},
    "fighter_state_367dc@ff": {'byte', 'call #1 memory'},
    "fighter_state_367dc@mask": {'byte', 'call #1 memory'},
    "fighter_state_367dc@mode": {'call #1'},
    "fighter_state_367dc@stream": {'call #0'},
    "hit_anim_start_c@anchor": {'call #1', 'call #2'},
    "hit_anim_start_c@begin": {'call #1', 'call #2', 'call #0'},
    "hit_anim_start_c@bits": {'call #0'},
    "hit_anim_start_c@side": {'call #1', 'call #2'},
    "hit_anim_start_c@stream": {'call #0'},
    "hit_anim_start_c@xy": {'call #1', 'call #2'},
    "palette_record@adv": {'byte'},
    "palette_record@flag": {'byte'},
    "palette_record@order": {'byte'},
    "palette_record@swap": {'byte'},
    "palette_record@wide": {'byte'},
    "palette_record_flagged@adv": {'byte'},
    "palette_record_flagged@flag": {'byte'},
    "palette_record_flagged@order": {'byte'},
    "palette_record_flagged@swap": {'byte'},
    "palette_record_flagged@wide": {'byte'},
    "render_find@cmp": {'eax'},
    "render_find@last": {'eax'},
    "render_find@null": {'eax'},
    "render_find@skip": {'eax'},
    "render_pop_free@head": {'byte'},
    "render_pop_free@next": {'byte'},
    "render_pop_free@ret": {'eax'},
    "render_splice@first": {'byte'},
    "render_splice@head": {'byte'},
    "render_splice@lt": {'byte'},
    "render_splice@next": {'byte'},
    "render_splice@prev": {'byte'},
    "render_unlink@chain": {'byte'},
    "render_unlink@free": {'byte'},
    "render_unlink@head": {'byte'},
    "render_unlink@noop": {'byte'},
    "render_unlink@prev": {'byte'},
    "snd_music_playing@call": {'eax', 'call #0'},
    "snd_music_playing@eax": {'eax'},
    "snd_music_playing@eq3": {'eax'},
    "snd_music_playing@gate": {'eax', 'call #0'},
    "snd_music_playing@one": {'eax', 'call #0'},
    "snd_music_playing@zero": {'eax', 'call #0'},
}
```

The exact case set that alone catches each mutant (measured on the prototype; the record's copy of
the test's table):

```
        ("actor_type_127C0@at", ['t1', 't2']),
        ("actor_type_127C0@empty", ['t0']),
        ("actor_type_127C0@link", ['t1', 't2']),
        ("actor_type_127C0@rec", ['t1', 't2']),
        ("actor_type_127C0@ret", ['t0']),
        ("actor_type_127C0@unlink", ['t1', 't2']),
        ("actor_type_12800@at", ['t1', 't2']),
        ("actor_type_12800@clear", ['t1', 't2']),
        ("actor_type_12800@node", ['t1', 't2']),
        ("actor_type_12800@unlink", ['t1', 't2']),
        ("actor_type_12800@zero", ['t0']),
        ("fighter_18460@call", ['h3', 'h4', 'h6', 'h8', 'h9', 'ha']),
        ("fighter_18460@e1", ['h5']),
        ("fighter_18460@hi", ['h2', 'h7']),
        ("fighter_18460@id", ['ha']),
        ("fighter_18460@lo", ['h2', 'h7']),
        ("fighter_18460@null", ['h0']),
        ("fighter_18460@null2", ['h1']),
        ("fighter_18460@range", ['h4']),
        ("fighter_2bd44@a28", ['m0']),
        ("fighter_2bd44@a2a", ['m0', 'm1']),
        ("fighter_2bd44@clr24", ['m0']),
        ("fighter_2bd44@id", ['m0', 'm1']),
        ("fighter_2bd44@idx", ['m0']),
        ("fighter_2bd44@o29", ['m0']),
        ("fighter_2bd44@src", ['m0', 'm1']),
        ("fighter_39738@chan", ['r3', 'r4', 'r5', 'r7']),
        ("fighter_39738@cut", ['r7']),
        ("fighter_39738@diff", ['r7']),
        ("fighter_39738@div", ['r5']),
        ("fighter_39738@idx", ['r7']),
        ("fighter_39738@k", ['r0', 'r1', 'r2', 'r3', 'r7']),
        ("fighter_39738@k2", ['r4']),
        ("fighter_39738@tab", ['r2', 'r4', 'r6']),
        ("fighter_3b6c4@cmp", ['b0', 'b4', 'b5']),
        ("fighter_3b6c4@o54", ['b3']),
        ("fighter_3b6c4@ret", ['b0', 'b5']),
        ("fighter_3b6c4@s53", ['b0', 'b4', 'b5']),
        ("fighter_3b6c4@s54", ['b0', 'b2', 'b4', 'b5']),
        ("fighter_3b6c4@side", ['b0', 'b4', 'b5']),
        ("fighter_block_hit@arm", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
        ("fighter_block_hit@b62", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
        ("fighter_block_hit@c61", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
        ("fighter_block_hit@call", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
        ("fighter_block_hit@clamp", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
        ("fighter_block_hit@s54", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
        ("fighter_block_hit@tab", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
        ("fighter_block_hit@u", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
        ("fighter_state_367dc@call", ['d0', 'd1', 'd2', 'd3', 'd4']),
        ("fighter_state_367dc@ch", ['d0', 'd1', 'd2', 'd3', 'd4']),
        ("fighter_state_367dc@clr4c", ['d0', 'd1', 'd2', 'd3', 'd4']),
        ("fighter_state_367dc@clr53", ['d0', 'd1', 'd2', 'd3', 'd4']),
        ("fighter_state_367dc@ff", ['d0', 'd1', 'd2', 'd3', 'd4']),
        ("fighter_state_367dc@mask", ['d0', 'd1', 'd2', 'd3', 'd4']),
        ("fighter_state_367dc@mode", ['d0', 'd1', 'd2']),
        ("fighter_state_367dc@stream", ['d0', 'd1', 'd2', 'd3', 'd4']),
        ("hit_anim_start_c@anchor", ['c0', 'c1', 'c2']),
        ("hit_anim_start_c@begin", ['c0', 'c1', 'c2']),
        ("hit_anim_start_c@bits", ['c0', 'c2']),
        ("hit_anim_start_c@side", ['c1']),
        ("hit_anim_start_c@stream", ['c0', 'c1', 'c2']),
        ("hit_anim_start_c@xy", ['c0', 'c1', 'c2']),
        ("palette_record@adv", ['w0', 'w1']),
        ("palette_record@flag", ['w0', 'w1']),
        ("palette_record@order", ['w0', 'w1']),
        ("palette_record@swap", ['w0', 'w1']),
        ("palette_record@wide", ['w0', 'w1']),
        ("palette_record_flagged@adv", ['v0', 'v1']),
        ("palette_record_flagged@flag", ['v0', 'v1']),
        ("palette_record_flagged@order", ['v0', 'v1']),
        ("palette_record_flagged@swap", ['v0', 'v1']),
        ("palette_record_flagged@wide", ['v0', 'v1']),
        ("render_find@cmp", ['f0', 'f1']),
        ("render_find@last", ['f0', 'f1']),
        ("render_find@null", ['f0', 'f1']),
        ("render_find@skip", ['f1', 'f2']),
        ("render_pop_free@head", ['p0', 'p1', 'p2']),
        ("render_pop_free@next", ['p0', 'p2']),
        ("render_pop_free@ret", ['p0', 'p1', 'p2']),
        ("render_splice@first", ['s2', 's3', 's4', 's5']),
        ("render_splice@head", ['s0', 's1']),
        ("render_splice@lt", ['s4', 's5']),
        ("render_splice@next", ['s0', 's1', 's2', 's3', 's4', 's5']),
        ("render_splice@prev", ['s2', 's3', 's4', 's5']),
        ("render_unlink@chain", ['u0', 'u1', 'u2']),
        ("render_unlink@free", ['u0', 'u1', 'u2']),
        ("render_unlink@head", ['u0', 'u2']),
        ("render_unlink@noop", ['u0', 'u1', 'u2']),
        ("render_unlink@prev", ['u0', 'u2']),
        ("snd_music_playing@call", ['m1', 'm2', 'm3']),
        ("snd_music_playing@eax", ['m3']),
        ("snd_music_playing@eq3", ['m1', 'm2']),
        ("snd_music_playing@gate", ['m0']),
        ("snd_music_playing@one", ['m0', 'm1', 'm2', 'm3']),
        ("snd_music_playing@zero", ['m1', 'm2', 'm3']),
```

The mutants with calls have their catch pinned to the call list or the memory at a call:
`actor_type_127C0`/`actor_type_12800` at the `0x249D0`/`0x249B0` real calls (their cores call the
now-exported `list_unlink`/`list_insert_after`, so the seams record exactly as the port bodies do),
`fighter_2bd44` at the `0x2A408` stub and the memory at it, `fighter_state_367dc` at the `0x2BC30`
stub, `hit_anim_start_c`/`fighter_block_hit`/`fighter_3b6c4`/`fighter_18460` at their anchors and
calls, `snd_music_playing` at the `0x5DEED` stub.

## §C3c.4 Counters, measured

| state | diff-verify counter | E2 |
|---|---|---|
| base `bb14036` | `234/234 functions VERIFIED; 779/779 mutants detected; 1 named gaps; 165/185 rows with callees closed (49 have none)` | `233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted 30`; voice `0 / 115 / 19` |
| C3c prototype (seventeen rows) | `251/251 functions VERIFIED; 874/874 mutants detected; 1 named gaps; 178/195 rows with callees closed (56 have none)` | byte-identical |

The arithmetic: +17 functions, +95 mutants (3+5+4+5+6+5+0+8+8+6+5+5+7+6+6+8+8), rows with callees
185 -> 195 (+10: `actor_type_127C0`, `actor_type_12800`, `fighter_18460`, `fighter_state_367dc`,
`snd_music_playing`, `fighter_2bd44`, `fighter_3b6c4`, `hit_anim_start_c`, `fighter_block_hit`,
`fighter_39738`; the other seven rows have none), no-callee 49 -> 56 (+7: `render_pop_free`,
`render_splice`, `render_find`, `render_unlink`, `fighter_18428`, `palette_record_flagged`,
`palette_record`). Closed 165 -> 178 (+13): nine of the ten new rows with callees (all but
`snd_music_playing`, open on the named `0x5DEED`) plus four dependents the batch closes —
`fighter_18540` and `hit_record_y` by `0x18428`+`0x18460`, `fighter_38154` by `0x367DC`,
`effect_teardown` by `0x33714`+`0x33734`; no regression.

`make entry-triage` is byte-identical (`targets 233 unported, 262 ported; supplement 131 (3
unported, 0 stale); untrusted entries 30`; voice `0 / 115 / 19`); `python3
tools/port_progress.py` stays `771 1203 64` / `731 731 100` (the split adds no counted address,
§C3c.2); `symbols.h` regeneration is byte-identical; `PR_ORACLE_REQUIRED=1 ./build/run_tests`
prints `all checks passed`; `python3 -m unittest tools.tests.test_diff_verify` is `107 tests OK`.

## §C3c.5 Decisions, named gaps and limits

**No decision is left to the user.** The C3b §C3b.5 row-shape decision for `0x1C390`/`0x1C3A0`
was executed as recorded (two rows, the split; §C3c.2), and the same treatment was given to the
`0x1C458`/`0x1C3D0` pair the C3c brief named. The one correction is the `0x39738` raw-over-port
fix (§C3c.2 correction 1).

Named gaps and limits:

- **`fighter_18428` carries zero mutants.** The function only reads (the slot pointer and
  slot+0x7A, then a jump through the seven-entry table 0x1840C whose entries are all the RET at
  0x18408); it writes nothing and calls nothing, so no case can catch a mutation of a read. The
  row's claim is its coverage (3/3 blocks, four cases) and the fact that it is a row at all:
  `fighter_18460`'s call to it is in that row's call set, and `hit_record_y`/`fighter_18540` close
  on it. Its h3 null-slot case reads 0x7A (outside the image; the emulator's zero page), the same
  way the port reads `mem[0x7A]` — pinned in `P45_OUTSIDE`.
- **`snd_music_playing`'s `0x5DEED` is a named non-row.** `0x5DEED` is the AIL
  `AIL_sequence_status` wrapper (runtime region >= 0x5D000; its body calls the AIL dispatcher
  0x6A950), which the port serves from the host mixer. It joins the C3b names
  (`0x5DC0F`/`0x5DC8B`/`0x5DD03`/`0x5DEAF`); its seam (`PR_SEAM_RET0`) is added so the row can
  stub it per case. The row stays open by design, like the other AIL-blocked rows.
- **All four render rows and the type-0x01 callback rows have no callees**; their `render_count`
  bookkeeping is PORT-only and lives in `render_list_insert`/`render_list_remove`, not in the
  halves (the halves' rows compare only the raw's state).
- **`actor_type_127C0`/`actor_type_12800`'s empty-list treatment** is the port's `list_head`
  self-sentinel rule (the raw's `cmp edx,0xF0A78`): a head of 0 is unrepresentable (the raw would
  walk address 0), so no case seeds it.
- **The `0x39738` first path's raw return** (the §C3c.2 correction) is now the port's; its
  `0x33950` ctx is an allow (both sides run it, unrecorded).
- **`fighter_18428`'s and `fighter_18460`'s reads of `[slot+0x7A]` with a null slot** read the
  emulator's zero page on the original side and `mem[]`'s zeros in the port (0x7A /= 0x10000);
  both sides agree.
- E3's, P1-P8's and C1-C3b's limits stand: seeds are hand pokes; the memory at a call is `mem[]`
  only; the callee column is one level deep.

## §C3c.6 What the planner ran

- The baseline on the clean `bb14036` worktree: `make diff-verify` -> the §C3c.4 base counter and
  table; `make entry-triage` -> `233/262`; `python3 tools/port_progress.py` -> `771 1203 64` /
  `731 731 100`.
- The prototype, in this worktree: the port/src split and exports and the `0x5DEED` seam first,
  then the seventeen rows family by family (the render halves; the type-0x01 callbacks; the new
  items; the type-family slice). Each row was measured with
  `python3 tools/diff_verify.py --function NAME --self-check` until VERIFIED with every mutant
  detected, then the full `python3 tools/diff_verify.py --self-check` (§C3c.4 counter), the
  `python3 -m unittest tools.tests.test_diff_verify` suite (107 tests OK, including the new
  `test_each_c3c_mutant_is_caught_by_what_it_breaks` and the extended exact-set, clobber and
  counter assertions), `make entry-triage` (byte-identical), `PR_ORACLE_REQUIRED=1 ./build/run_tests`
  (all checks passed) and a `symbols.h` regeneration (byte-identical). Then `git checkout -- port
  tools`, leaving only this record and the plan.
- The prototype's own corrections (one raw-over-port, §C3c.2 correction 1; the rest fixture/draft
  fixes): §C3c.2 corrections 2-3.
- The full `make verify` (~25 min) was not run by the planner: the brief's task-scoped gates are
  the ones above. The `port/src` changes are the splits, exports and one seam, plus the `0x39738`
  correction; the executor runs the full gate in Task 4.
- The prototype's diff (1600 lines, 1314 insertions over ten files, the exact files the plan's
  Task 2 applies) is embedded in the plan.

## §C3c.7 The C3d deferral (the rest of the type family, with evidence)

This session measured 17 of the 30 addresses the C3b §C3b.7 deferral named (21 candidates + 9
new items); the split bundles `0x1C390`+`0x1C3A0` and `0x1C458`+`0x1C3D0` as four of those
addresses. The remaining **13 are the type family**, deferred on session budget (not a
correctness judgement): every one is ported (a `/* 0xADDR` header in `port/src`, so it is
counted by `port_progress.py`) and its original is disassembled below. Sizes are
`port/decomp/prage.functions.csv` bytes; the callee columns are capstone scans of the image; a
callee is "rowed" when the base `diff_table` has a row for its address and "missing" otherwise
(those missing ones are the type rows' own new frontier, to be rowed in C3d or named):

| family | addresses (insns / size / port function) and their callees: rowed | missing |
|---|---|---|
| the type family | `0x1AB5C` (105/302, `fighter_input_mask`): `0x33A10` `0x46460` `0x18B04` | `0x1AB10` `0x1A7CC` |
| | `0x2A820` (182/589, `pset_write`): `0x2B150` | `0x2A690` |
| | `0x36BC8` (87/282, `fighter_state_36bc8`): `0x2BC30` `0x188AC` | `0x38BC8` `0x38BB0` |
| | `0x379C4` (49/147, `fighter_379c4`): `0x2BC30` | `0x37178` |
| | `0x385B0` (106/384, `fighter_385b0`): `0x164E8` `0x2BC30` `0x38154` | - |
| | `0x39040` (151/566, `fighter_39040`): `0x41310` `0x5D7DC` `0x2C3FC` | `0x38D90` `0x38FEC` `0x4F944` |
| | `0x392A0` (281/907, `fighter_392a0`): `0x41310` | `0x33A68` `0x46190` `0x36E78` |
| | `0x3AAFC` (182/667, `fighter_reaction_apply`): `0x33A10` `0x18B04` `0x3AFC4` `0x39834` `0x2C3FC` `0x39F40` `0x3AA54` | `0x3A280` `0x3A0FC` `0x36D20` `0x3A650` `0x3A79C` `0x3A504` `0x3A8E8` |
| | `0x3AD98` (82/260, `fighter_3ad98`): `0x33A10` `0x2C3FC` `0x2AE14` `0x2BC30` | `0x392A0` |
| | `0x3AE9C` (86/294, `fighter_3ae9c`): `0x33950` `0x1A570` | - |
| | `0x3B080` (50/177, `fighter_3b080`): `0x1A570` `0x3C148` | `0x3B038` |
| | `0x3B134` (100/355, `fight_command_map`): `0x33A10` `0x3AFC4` `0x5D7DC` | `0x1AB10` `0x3BDB0` `0x3BDDC` |
| | `0x4F434` (58/178, `fighter_4f434`): - | `0x46534` |

Notes for C3d:

- `0x385B0` and `0x3AE9C` have **no missing callees**: their rows would close immediately
  (like this session's `fighter_2bd44`/`3b6c4`/`hit_anim_start_c`/`fighter_block_hit`/`39738`).
- `0x3AD98` misses only `0x392A0` (itself in this list): row `0x392A0` first, then `0x3AD98`.
- `0x4F434`'s only callee `0x46534` has no row; it is the *next* frontier item after `0x4F434`
  (the same chained-frontier shape C3b surfaced for its own rows). Its port function exists
  (`fighter_46534`, fighter.h) and is 58 bytes/?? insns — C3d should row it with or after
  `0x4F434`.
- The other missing callees (`0x1AB10`, `0x1A7CC`, `0x38BC8`, `0x38BB0`, `0x37178`, `0x38D90`,
  `0x38FEC`, `0x4F944`, `0x33A68`, `0x46190`, `0x36E78`, `0x3A280`, `0x3A0FC`, `0x36D20`,
  `0x3A650`, `0x3A79C`, `0x3A504`, `0x3A8E8`, `0x3B038`, `0x3BDB0`, `0x3BDDC`, `0x2A690`,
  `0x46534`) all have port functions (they are called by the ported type rows) and no rows;
  C3d's enumeration must measure each before deciding stub/real/allow.
- The four fighter rows and `fighter_command_dispatch` stay open until **all** of
  `0x36BC8`/`0x379C4`/`0x385B0`/`0x39040`/`0x3C520` (for `fighter_36870`), `0x392A0`/`0x39738`/
  `0x4F434` (for `fighter_39834`), `0x1A734`/`0x1AB5C`/`0x3B134` (for
  `fighter_command_dispatch`) and `0x2BD44`/`0x3AAFC`/`0x3AD98`/`0x3AE9C`/`0x3B080`/`0x3B6C4`
  (for `fighter_reaction`) are rowed; this session closed the `0x3C520`, `0x2BD44`, `0x3B6C4`,
  `0x1A734` and `0x39738` contributions.
- No row in this batch touches a gameplay path (the `port/src` changes are the splits/exports,
  one seam and the `0x39738` correction), so the gp miss sets are untouched; Task 1's `PR_GP_DUMP`
  pinned sets are the executor's check that they stay so. The `0x39738` correction does change a
  port function body: `PR_GP_DUMP` and the oracles are the evidence that no exercised path
  depended on the old behavior (the function is reached only through `fighter_39834`, itself
  unexercised until a scenario reaches it).

## §C3c.8 Results (the executed tree)

To be appended by the executor after the plan's Tasks 2-4: the commit shas, the measured counters
on the executed tree (expected equal to §C3c.4's prototype row), the task-gate log lines, the
sweep's findings, and the full `make verify` result with the plan's parallel-safe overrides.
