# Reverse completion C2b: the nine deferred callee rows (record)

**Scope.** Track P's third verification-only batch (roadmap row C2b of record
`2026-10-04-reverse-c2-derivations.md` §C2.5, which C2 deferred for session budget): add the
differential-verification row for `0x2B2A0 0x33754 0x13C70 0x2C3FC 0x2AE14 0x36870 0x39834 0x3B298
0x3B714`, so the dependent rows close. No ported function, no `fn_register`, no E2 move. Plan:
`2026-10-05-reverse-c2b-deferred-rows.md`. Recipe: E3 record §E3.10; checklist:
`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**Status of the numbers.** Measured by the planner on 2026-10-05 on `reverse-c2b` at the base `main`
`e43712f` (C1+C2+P1-P8 merged), in this worktree: the baseline gates and a full prototype of the
nine rows (every gate run, then reverted). The image is
`build/diffrun --exe data/game/C/PRAGE.EXE --image-out FILE`, sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's, E3's, P1-P8's). Every address below is capstone
5.0.7 over that image (fixups applied); `unicorn` 2.1.4 runs the original side. Ghidra was not
consulted.

---

## §C2b.1 The nine rows, re-derived from the raw

C2's §C2.5 table (its measured seam lists) was checked against capstone over the image and against
the port's call sites. It is exact, except where noted in §C2b.2. The rows and their direct callees
(the raw's scan):

| callee | blocks (insns) | the raw's direct callees | row design |
|---|---|---|---|
| `0x2B2A0` | 85 (682) | `2B8F8` (operand), `2EA64` (ret), `2B150` (set_dead), `5D7DC` (rng), `2AE14` (spawn), `29DB8` (write_var), three indirect (`0x2B56D/0x2B594/0x2B5EA`) and `2C3FC` (voice) | `2B8F8`/`29F34` allowed (so the 0x1F prefix selects ops >= 0x20 and the 0x0D table entry `0x2B52F`; see §C2b.5), `2EA64`/`29D60` allowed, the rest stubbed |
| `0x33754` | 15 (94) | `1B544` (host resolve), `62003` (fatal) | `1B544` allowed against a preloaded fixture; the fatal block named |
| `0x13C70` | 12 (73) | `249D0`, `1B544`, `249B0` | all allowed: the port's effects.c keeps its own copies of the list primitives and its `res_resolve` writes the same bytes |
| `0x2C3FC` | 100 (395) | `1CA14` `1CA6C` `1CC28` `1CD9C` `1CE04` `1CE70` `1D238` `1D244` | all stubbed |
| `0x2AE14` | 33 (260) | `2AC80` `2B2A0` `2A408` `33754` `2A820` `2A620`, the indirect type callback at `0x2B0E9`, `1C390`+`1C3A0` | the indirect callback allowed (`0x5D812` and `0x127C0`); `1C390`/`1C3A0` allowed |
| `0x36870` | 31 (238) | `385B0` `39280` `164E8` `39040` `37D18` `365C8` `36BC8` `36638` `2BC30` (x3) `3C520` `379C4` | all eleven stubbed (the rows they own prove them) |
| `0x39834` | 16 (113) | `33A10` `3AFC4` `39738` `392A0` `468D8` `36D98` `36CE4` `2C3FC` `4F434` | `39738`/`392A0`/`36CE4`/`4F434`/`2C3FC` stubbed, `468D8`/`36D98` real, `33A10`/`3AFC4` allowed |
| `0x3B298` | 28 (138) | `33A10` `3AFC4` `3B134` `1AB5C` `46460` `1A734` | `3B134`/`1AB5C`/`46460`/`1A734` stubbed, `33A10`/`3AFC4` allowed |
| `0x3B714` | 23 (132) | `33950` `3C59C` `62003` `3AFC4` `39EFC` `3B298` `3B080` `3AE9C` `2BD44` `3AAFC` `3AD98` `3B6C4` | `3C59C`/`3B298`/`3B080`/`3AE9C`/`2BD44`/`3AAFC`/`3AD98`/`3B6C4` stubbed, `39EFC` real, `33950`/`33A10`/`3AFC4` allowed; the `local_24 == 0xFF` fatal block named |

`0x33754`'s raw does **not** call `0x33734`: it writes the dirty-list record inline
(`0x337C3..0x337D8`); the port's `palette_record` is the same four stores plus the head bump. The
two host calls (`1B544`) are the only ones the raw's non-fatal path makes.

## §C2b.2 The rows: cases, seams, fixtures and the corrections

Each row is a `Spec` in `tools/diff_verify.py` (`C2B_SPECS`), a binding `b_*` and mutants `m_*` in
`port/tests/diff_runner.c` (`k_bindings`), and the exact-set extensions in
`tools/tests/test_diff_verify.py` (`C2B_MASKS`, `C2B_KINDS`, the case-set table, the clobber table,
the counter line). The measured rows:

| row | entry | cases | blocks | mutants | callees (each by its own check) |
|---|---|---|---|---|---|
| `palette_acquire` | 0x33754 | 7 | 14/15 (0x3384E named) | 6/6 | `1B544` allow |
| `effects_spawn` | 0x13C70 | 5 | 12/12 | 5/5 | `1B544`, `249B0`, `249D0` allow |
| `fighter_command_dispatch` | 0x3B298 | 15 | 28/28 | 7/7 | `3B134`, `1AB5C`, `46460`, `1A734` stub; `33A10`, `3AFC4` allow |
| `fighter_reaction` | 0x3B714 | 9 | 22/23 (0x3B75D named) | 6/6 | `3C59C`, `3B298`, `3B080`, `3AE9C`, `2BD44`, `3AAFC`, `3AD98`, `3B6C4` stub; `39EFC` real; `33950`, `33A10`, `3AFC4` allow |
| `fighter_39834` | 0x39834 | 7 | 16/16 | 5/5 | `39738`, `392A0`, `36CE4`, `4F434`, `2C3FC` stub; `468D8`, `36D98` real; `33A10`, `3AFC4` allow |
| `fighter_36870` | 0x36870 | 14 | 31/31 | 5/5 | all eleven stub |
| `actor_spawn` | 0x2AE14 | 11 | 32/33 (0x2B071 named dead) | 4/4 | `2AC80`, `2B2A0`, `2A408`, `33754`, `2A820`, `2A620` stub; `1C390`, `1C3A0`, `127C0`, `5D812` allow |
| `sound_voice` | 0x2C3FC | 62 | 100/100 | 4/4 | eight audio stubs |
| `spawn_anim_opcode` | 0x2B2A0 | 60 | 85/85 | 4/4 | `2B150`, `5D7DC`, `2AE14`, `29DB8`, `2C3FC` stub; `2EA64`, `29D60`, `2B8F8`, `29F34` allow |

**New seams added to `port/src`** (each is a first statement; `E.callee_clobbers` re-derives every
declared clobber, with the three §C2b.2 corrections):

| callee | port function | seam | clobbers (image, corrected) |
|---|---|---|---|
| `0x29DB8` | `anim_write_var` | `PR_SEAM(0x29DB8u, rec, (u32)op, value)` | ebx, edx |
| `0x2B8F8` | `anim_operand` | `PR_SEAM_RET(0x2B8F8u, rec)` | () |
| `0x2AC80` | `actor_alloc` | `PR_SEAM_RET(0x2AC80u, flag)` | () |
| `0x2A820` | `pset_write` | `PR_SEAM(0x2A820u, rec, pset)` | edx |
| `0x2A620` | `mode1_cursor` | `PR_SEAM(0x2A620u, rec, pset)` | edx |
| `0x1A734` | `fighter_block_hit` | `PR_SEAM(0x1A734u, side)` | () |
| `0x1AB5C` | `fighter_input_mask` | `PR_SEAM_RET(0x1AB5Cu, side)` | ebp |
| `0x46460` | `fighter_input_read` | `PR_SEAM_RET(0x46460u, side, (u32)index)` | edx |
| `0x365C8` | `fighter_state_365c8` | `PR_SEAM_RET(0x365C8u, slot, rec, side)` | ebx, edx |
| `0x36BC8` | `fighter_state_36bc8` | `PR_SEAM_RET(0x36BC8u, slot, rec)` | edx |
| `0x164E8` | `fighter_164e8` | `PR_SEAM(0x164E8u, side)` | () |
| `0x385B0` | `fighter_385b0` | `PR_SEAM(0x385B0u, rec)` | () |
| `0x379C4` | `fighter_379c4` | `PR_SEAM(0x379C4u, slot)` | ecx, esi, edi, ebp |
| `0x39040` | `fighter_39040` | `PR_SEAM(0x39040u, side)` | () |
| `0x3C520` | `hit_anim_start_c` | `PR_SEAM(0x3C520u, rec, stream, frame_bits)` | edx |
| `0x39738` | `fighter_39738` | `PR_SEAM_RET(0x39738u, side, (u32)b)` | edx |
| `0x392A0` | `fighter_392a0` | `PR_SEAM(0x392A0u, slot, (u32)v, (u32)w)` | ebx, edx |
| `0x36CE4` | `fighter_36ce4` | `PR_SEAM(0x36CE4u, slot)` | () |
| `0x4F434` | `fighter_4f434` | `PR_SEAM0(0x4F434u)` | () |
| `0x3B134` | `fight_command_map` | `PR_SEAM(0x3B134u, side, edx_arg, override)` | ebx, edx, edi, ebp |
| `0x3C59C` | `fighter_pass_flag` | `PR_SEAM_RET(0x3C59Cu, bit, side)` | edx |
| `0x3B080` | `fighter_3b080` | `PR_SEAM(0x3B080u, side, param_2, param_3, param_4)` | ebx, ecx, edx |
| `0x3AE9C` | `fighter_3ae9c` | `PR_SEAM(0x3AE9Cu, side, (u32)param_2)` | edx |
| `0x2BD44` | `fighter_2bd44` | `PR_SEAM(0x2BD44u, param_1, param_2)` | edx (corrected: see below) |
| `0x3AAFC` | `fighter_reaction_apply` | `PR_SEAM(0x3AAFCu, slot, reaction)` | ebx, ecx, edx, edi, ebp |
| `0x3AD98` | `fighter_3ad98` | `PR_SEAM(0x3AD98u, side, anim[0], anim[1], anim[2])` | edx |
| `0x3B6C4` | `fighter_3b6c4` | `PR_SEAM_RET(0x3B6C4u, side)` | () |
| `0x1CA14` | `snd_music_request` | `PR_SEAM_RET(0x1CA14u, song, b)` | edx |
| `0x1CA6C` | `snd_music_stop` | `PR_SEAM_RET0(0x1CA6Cu)` | () |
| `0x1CC28` | `snd_sample_queue` | `PR_SEAM_RET(0x1CC28u, h, loop)` | edx |
| `0x1CD9C` | `snd_samples_stop_all` | `PR_SEAM_RET0(0x1CD9Cu)` | () |
| `0x1CE04` | `snd_sample_stop` | `PR_SEAM_RET(0x1CE04u, h)` | () |
| `0x1CE70` | `snd_sample_playing` | `PR_SEAM_RET(0x1CE70u, h)` | () |
| `0x1D238` | `snd_music_unpause` | `PR_SEAM0(0x1D238u)` | () |
| `0x1D244` | `snd_sample_unpause` | `PR_SEAM0(0x1D244u)` | () |

`fighter_input_read`, `fighter_input_mask`, `fighter_state_365c8`, `fighter_state_36bc8`,
`hit_anim_start_c`, `fighter_379c4`, `fighter_164e8`, `fighter_36ce4`, `fighter_39738`,
`anim_operand`, `pset_write`, `mode1_cursor`, `fighter_2bd44`, `fighter_4f434` and the eight
`snd_*` lost `static` (22 functions; the diff has 25 `-static` lines, three of them forward
declarations), declared in `fighter.h` / `actors.h` / `flow.h`, so the mutant cores can call them. All are
source-only changes: no new `FN_` address, no `fn_register`, so the E2 table is byte-identical
(§C2b.4) and `port_progress.py` stays `771 1203 64` / `731 731 100`.

**Raw-over-port corrections (each with its address):**

1. **`0x2B2A0` opcode 0x2D's arms were swapped.** The raw at `0x2B8AB` is `cmp edx,eax; jl
   0x2B8C2`: `pv = (s16)word[pset+0x0C]` below the operand takes the `rec+8 += 4` arm, and
   `pv >= ax` takes the stream jump (`rec+8 += 2; rec+8 = *(rec+8) - 2`). The port had the two arms
   inverted; the row's `o2da`/`o2db` cases exposed it. Fixed in `actors.c` (raw wins, record §C2b.3
   of the plan's review focus).
2. **`E.callee_clobbers` over-approximates the Watcom callee-saved registers through the call
   tree** for three stubs, and their callers keep the registers live across the call:
   `0x2B150` (its caller `0x2B30D` reads ESI after the call; the bytes derive esi/edi/ebp),
   `0x2BD44` (the caller `0x3B877`/`0x3B8C8` reads ESI; the bytes derive edx/esi/edi/ebp) and
   `0x3B298` (the caller reads EDI at `0x3B82E` — `mov eax,edi` — feeding the `0x3B834` call into
   `0x3AE9C`; the bytes derive edx/edi/ebp). A poisoned register a
   caller relies on made the original side fault/split, so the declarations are the caller-observed
   sets and `diff_emu.callee_clobbers` applies `_CALLEE_CLOBBER_FIXES` (`0x2B150: ()`,
   `0x2BD44: ("edx",)`, `0x3B298: ("edx",)`). The P3 `DISPATCH` and P7 `P7_2B150` declarations were
   updated to match; every previously passing row re-measured.
3. **The `0x2AE14` fixture's descriptor field order was corrected during the sweep**: the port reads
   the extent at `dp+0x0A` and the `rec+0x28` bits at `dp+0x08`; the first prototype had them
   swapped.
4. **The `c2b_res` fixture's payload base**: `0x1B544` (and `res_resolve`) add the handle's low 23
   bits to the entry's `+0x10`, so the entry stores the payload base and the per-handle payload sits
   at base + offset. The first prototype stored the offset payload in `+0x10`, so the copy/len
   reads landed on zeros; corrected.

**Named deviations the rows do not exercise (see §C2b.5):** `0x33754`'s fatal `0x3384E`; `0x3B714`'s
`local_24 == 0xFF` fatal `0x3B75D`; `0x2AE14`'s pset+2 zero arm `0x2B071` (dead: `rec+0x5F` is
always 1 at `0x2AF31`). `0x2B2A0`'s 0x0D jump-table entry `0x2B52F` is **not** dead: the `o0dp`
prefix case (word `0x1F0D`) reaches it — the direct 0x0D test at `0x2B2CA` runs before
`anim_operand`, and the prefix store `mov word [0x105BE4],cx` at `0x2B932` then dispatches
`table[0x0D]` at `0x2B2FC`.

## §C2b.3 The mutants

46 mutants, all detected (`make diff-verify --self-check`: `598/598`); what alone catches each is
pinned by `test_each_c2b_mutant_is_caught_by_what_it_breaks` (`C2B_KINDS` plus the measured case-set
table; the exact dicts are the ones committed in `tools/tests/test_diff_verify.py`). The rows with
callees have mutants caught only by the call list or the memory at a call (e.g.
`fighter_command_dispatch@early` by `call #1`/`call #2`; `spawn_anim_opcode@child` by `call #0`;
`sound_voice@queue` by `call #1`).

The mutant kinds (measured; the record's copy of the test's `C2B_KINDS`):

KINDS = {
    "actor_spawn@minus": {'call #3', 'call #1', 'byte', 'call #1 memory', 'call #2 memory', 'call #3 memory', 'call #4', 'call #2'},
    "actor_spawn@mutant": {'call #3', 'byte', 'call #2 memory', 'call #3 memory', 'call #4', 'call #2'},
    "actor_spawn@pal": {'call #3', 'byte', 'call #2 memory', 'call #3 memory', 'call #4', 'call #2'},
    "actor_spawn@type": {'byte'},
    "effects_spawn@copy": {'byte'},
    "effects_spawn@count": {'byte'},
    "effects_spawn@free": {'byte'},
    "effects_spawn@lock": {'byte'},
    "effects_spawn@mutant": {'byte'},
    "fighter_36870@arm": {'call #5', 'call #4', 'call #3', 'byte'},
    "fighter_36870@case": {'call #4'},
    "fighter_36870@mask": {'call #4 memory', 'byte', 'call #5 memory', 'call #1 memory', 'call #2 memory', 'call #3 memory'},
    "fighter_36870@mutant": {'call #3', 'call #1', 'byte', 'call #5', 'call #0', 'call #0 memory', 'call #4', 'call #2'},
    "fighter_36870@so": {'call #4 memory', 'byte', 'call #5 memory', 'call #2 memory', 'call #3 memory'},
    "fighter_39834@ai": {'call #4', 'call #3', 'byte', 'call #3 memory'},
    "fighter_39834@arm": {'call #4', 'call #3', 'byte', 'call #3 memory'},
    "fighter_39834@mutant": {'call #1'},
    "fighter_39834@tail": {'call #4'},
    "fighter_39834@thr": {'byte'},
    "fighter_command_dispatch@arm": {'call #2 memory', 'byte', 'call #3 memory'},
    "fighter_command_dispatch@b2": {'call #3', 'byte', 'eax', 'call #3 memory'},
    "fighter_command_dispatch@copy": {'call #1 memory', 'byte', 'call #2 memory', 'call #3 memory'},
    "fighter_command_dispatch@early": {'call #1', 'call #2'},
    "fighter_command_dispatch@force": {'byte', 'eax', 'call #2'},
    "fighter_command_dispatch@mutant": {'call #1 memory', 'byte', 'call #2 memory', 'call #3 memory'},
    "fighter_command_dispatch@scan": {'call #3', 'byte', 'eax'},
    "fighter_reaction@branch": {'call #3', 'call #6', 'call #4 memory', 'byte', 'call #5 memory', 'call #5', 'call #4'},
    "fighter_reaction@early": {'call #3', 'call #1', 'byte', 'call #5', 'call #4', 'call #2'},
    "fighter_reaction@efc": {'call #3', 'call #6', 'byte', 'call #5', 'call #4', 'call #2'},
    "fighter_reaction@hold": {'call #3', 'call #6', 'call #5', 'call #3 memory', 'call #4'},
    "fighter_reaction@mutant": {'byte'},
    "fighter_reaction@swap": {'call #6 memory', 'call #3', 'call #6', 'call #4 memory', 'call #1', 'byte', 'call #5 memory', 'call #5', 'call #0', 'call #2 memory', 'call #3 memory', 'call #4', 'call #2'},
    "palette_acquire@count": {'byte'},
    "palette_acquire@inc": {'byte'},
    "palette_acquire@mutant": {'byte', 'eax'},
    "palette_acquire@new": {'byte', 'eax'},
    "palette_acquire@reflow": {'byte'},
    "palette_acquire@start": {'byte'},
    "sound_voice@case5": {'call #0'},
    "sound_voice@mutant": {'call #1', 'call #0', 'byte', 'eax'},
    "sound_voice@queue": {'call #1'},
    "sound_voice@play": {'call #1', 'eax'},
    "spawn_anim_opcode@child": {'call #0'},
    "spawn_anim_opcode@indirect": {'eax'},
    "spawn_anim_opcode@mutant": {'byte'},
    "spawn_anim_opcode@skip": {'byte'},
}

## §C2b.4 Counters, measured

| state | diff-verify counter | E2 |
|---|---|---|
| base `e43712f` | `192/192 functions VERIFIED; 552/552 mutants detected; 1 named gaps; 69/158 rows with callees closed (34 have none)` | `233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted 30`; voice `0 / 115 / 19` |
| C2b prototype (nine rows) | `201/201 functions VERIFIED; 598/598 mutants detected; 1 named gaps; 148/167 rows with callees closed (34 have none)` | byte-identical |

The arithmetic: +9 functions, +46 mutants, rows with callees 158 -> 167 (the nine new rows all have
callees), no-callee 34 unchanged, closed 69 -> 148. The 79 rows that close are the base rows whose
callee set is now a subset of the verified rows. The nine new rows themselves stay open: every one
has an allowed callee without its own row (`0x1B544`, `0x249B0`, `0x249D0`, `0x5D812`, `0x127C0`,
`0x1C390`, `0x1C3A0`, `0x29D60`, `0x2EA64`, `0x2B8F8`, `0x29F34`, `0x33A10`, `0x3AFC4`), which the
one-level-deep closure rule counts unverified by construction (the same shape as C1/C2).

`make entry-triage` is byte-identical (no ported function, no `fn_register`);
`PR_ORACLE_REQUIRED=1 ./build/run_tests` prints `all checks passed`; the Python suite
(`python3 -m unittest tools.tests.test_diff_verify`) runs 104 tests OK and
`make diff-verify` (which chains `test_diff_emu` too) is green;
`python3 tools/port_progress.py` stays `771 1203 64` / `731 731 100`.

## §C2b.5 Decisions, named gaps and limits

**No decision is left to the user.** The one raw-over-port correction (the op-0x2D arm order) is
recorded with its address; the three clobber corrections are pinned by the callers' live-register
use. The host/runtime callees follow C1/C2's treatment: `0x1B544` is allowed against a preloaded
resource entry (flags bit `0x01000000`, payload base at entry `+0x10`; both sides add the handle's
low 23 bits), never stubbed with a made-up behaviour; the `0x62003` fatal paths are named.

Named gaps and limits:

- **The nine rows' unhit blocks** are named with their evidence: `0x3384E` (the palette table-full
  fatal; the port returns 0), `0x3B75D` (the `0x3B714` `0xFF` fatal; the port returns), `0x2B071`
  (dead: `rec+0x5F` is set to 1). `0x2B2A0` is 85/85: `0x2B52F` is **not** dead — the `o0dp` prefix
  case (word `0x1F0D`) reaches it, because the direct 0x0D test at `0x2B2CA` runs before
  `anim_operand`, whose prefix arm stores the word's low byte back (`mov word [0x105BE4],cx` at
  `0x2B932`) before the table dispatch at `0x2B2FC` (`table[0x0D] = 0x2B52F` at `0x2B218`).
- **The `0x2B2A0` row allows `0x2B8F8` and `0x29F34`** (the real operand fetch) so the 0x1F prefix
  can select opcodes >= 0x20 and the 0x0D table entry (`o0dp`, above); `0x2EA64`/`0x29D60` are
  allowed `ret` targets the port does not implement. The row's claim is equivalence on the exercised
  opcodes with those callees allowed.
- **The `0x1B544` fixture claims only the preloaded fast path**; the lazy-load presentation path is
  not exercised (it would enter the loader).
- **`0x13C70`'s raw returns EAX = 0xFCCE0 at `0x13D5A`** (the active-list sentinel), not the record;
  the port returns the record. Every caller discards the return, so the row's mask is 0 and the
  difference is a named limit, not a claim.
- **`0x2AE14`'s `a5` high word**: the raw zeroes `[esp+0x2A]` (a5's high word) at `0x2AE3C`
  (`xor edx,edx` at `0x2AE35`, so DX = 0; the store is not a2); the port keeps a5. The only a5
  high-word reads (`a5 >> 0x10` at `0x2AE27` and `a5 >> 8` for the 0x40/0x44 bits) happen before the
  overwrite or on unaffected bytes, and the cases keep a5's high word 0 so both sides agree. The row
  does not claim the zeroed overwrite's effects on a5 bits 16+.
- **The `0x29DB8` seam narrows the selector to u8**: the port's `anim_write_var` takes `op` as `u8`,
  so the original's 16-bit DX selector is truncated at the seam; the raw itself clears DH (`xor
  dh,dh` at `0x29DBE`) and masks `dl,0x7f` (`0x29DC2`), so the narrowing is behavior-equivalent. No
  case seeds EDX > 0xFF because bits 8+ never reach either side's selector; the u8 interface is a
  named limit, not a claim on the discarded bits.
- **`0x2C3FC`'s case-5 binary search**: the 0x100 remap reaches the `edx == 0` arm with record 0's
  type 5 (`v100b`); the unmatched sub-ids of each range are covered by `t5_tail_*`.
- **`0x2AE14`'s type callback**: the visible arm uses the `0x5D812` allow (the port's identity test
  replaces the call); the invisible arm uses the `0x127C0` allow, whose empty-list path is
  self-contained. A non-stub type callback is not exercised by this row.
- **`0x36870`'s render list**: `0x1C390`/`0x1C3A0` are allowed (the port's `render_list_insert`
  performs both and guards a null free head where the raw would read mem[0]); the row's cases seed
  a valid free node.
- **The `0x39834` k counter**: `DSW(0x107D2C + ctx0*2)` is the same dword's high word the `>= 0x14`
  test reads at `0x39973`, so the counter bumps it before the test; the `f5` case starts at 0x13.
- E3's, P1-P8's limits stand: seeds are hand pokes; the memory at a call is mem[] only; the callee
  column is one level deep.
- The `title_pin` unittest failure on this tree is pre-existing and outside `make verify`.

## §C2b.6 What the planner ran

- The baseline on the clean `e43712f` worktree: `make diff-verify` -> the §C2b.4 base counter and
  table; `PR_ORACLE_REQUIRED=1 ./build/run_tests` -> all checks passed; `make entry-triage` ->
  `233/262`; `python3 tools/port_progress.py` -> `771 1203 64` / `731 731 100`.
- The prototype, in this worktree, one family at a time (the host pair; the type family; the opcode
  family; the voice dispatcher), each row measured with
  `python3 tools/diff_verify.py --function NAME --self-check` until VERIFIED with every mutant
  detected, then the full `python3 tools/diff_verify.py --self-check` (the §C2b.4 counter), the
  `python3 -m unittest tools.tests.test_diff_verify` suite (104 tests OK, including the new
  `test_each_c2b_mutant_is_caught_by_what_it_breaks` and the extended clobber exact-set), `make
  entry-triage` (byte-identical) and `PR_ORACLE_REQUIRED=1 ./build/run_tests` (all checks passed).
  Then `git checkout -- port tools`, leaving only this record and the plan.
- The prototype's diff (2709 lines, 2158 insertions, the exact files the plan's Task 2 applies) is
  embedded in the plan.
- The full `make verify` (~25 min) was not run by the planner: the brief's task-scoped gates are
  the ones above. The `port/src` changes are seams plus one game-logic correction that no oracle
  capture exercises (the op-0x2D arm is reached only through crafted command streams; the executor
  runs the full gate in Task 4).

## §C2b.7 The new frontier

C2's §C2.8 named 11 row-less stubs (`0x18540 0x18350 0x18788 0x1881C 0x1A5AC 0x249B0 0x249D0
0x2B150 0x29F34 0x38154`, `0x2EA30` allow). C2b leaves them open **and adds its own row-less
stubs**, the next callee-row batch's (C3) scope. Measured from the final table, the ported callees
the C2b rows stub or allow that still have no row:

- the previously open eleven: `0x18540 0x18350 0x18788 0x1881C 0x1A5AC 0x249B0 0x249D0 0x2B150
  0x29F34 0x38154` (+`0x2EA30` allow);
- the type family's new stubs: `0x164E8 0x1A734 0x1AB5C 0x2A620 0x2A820 0x2AC80 0x2BD44 0x365C8
  0x36BC8 0x36CE4 0x379C4 0x385B0 0x39040 0x392A0 0x39738 0x3AAFC 0x3AD98 0x3AE9C 0x3B080
  0x3B134 0x3B6C4 0x3C520 0x3C59C 0x46460 0x4F434`;
- the voice dispatcher's eight audio stubs: `0x1CA14 0x1CA6C 0x1CC28 0x1CD9C 0x1CE04 0x1CE70
  0x1D238 0x1D244`;
- the allowed tree members without rows: `0x29DB8` (its operands), `0x2B8F8` (allowed), `0x41310`,
  `0x49444` (P7's row-less), `0x127C0` (a type callback), `0x1C390`/`0x1C3A0` (the port combines
  them), `0x1B544` (host-owned), `0x5D812`/`0x29D60`/`0x2EA64` (bare stubs/rets).

Their rows are the next callee-row batch (C3) with the same recipe; every one is ported and its
direct callees are the port's own helpers (the C1/C2/C2b seams are in place). The host-owned
`0x1B544` and the runtime fatal `0x62003` are not rows; they stay named limits.
