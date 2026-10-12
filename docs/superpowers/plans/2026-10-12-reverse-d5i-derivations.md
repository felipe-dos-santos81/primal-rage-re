# Track D, wave 5, batch d5i — derivation record (2026-10-12)

Differential rows for six `port/src/game/flow.c` functions the callee scan never reached: the original's
bytes (the image `diffrun` dumps, fixups applied, run by `tools/diff_emu.py`) against the port's C. Every
number below is measured on the branch `reverse-d5i` with
`python3 tools/diff_verify.py --image /tmp/pr_d5i_img.bin --function <row> --self-check`.
Narrow claim, as every row: equivalence on the exercised blocks and inputs only, each function with its
callees stubbed or run as stated.

## §1 The members

| addr | name | size | callers, and what they read of EAX | mask |
|---|---|---|---|---|
| `0x296B8` | `game_mode_32_step` | 696 | one site `0x251B2`, then `0x251B7 jmp 0x2540f`, the mode tail the wave-1 records trace dead (d1c §1, d1d §1); not re-traced here | 0 |
| `0x274FC` | `game_mode_0d_step` | 708 | one site `0x25367`, then `0x2536C jmp 0x2540f` | 0 |
| `0x424E8` | `game_mode_13_step` | 570 | one site `0x253C4`, then `0x253C9 jmp 0x2540f` | 0 |
| `0x1EA08` | `frontend_match_start` | 559 | five sites: `0x11A42` (`0x11A47 mov eax,0x1d`), `0x11DF7` (`0x11DFC mov eax,0x1d`), `0x1F140` (`0x1F15A xor eax,eax`; the caller reads DI, DX, BX and CH first, all saved by `0x1EA08`), `0x1F39B` (`0x1F3A0 mov eax,0x12c`), `0x1F278` (no load: `0x1EEB0` returns at `0x1F2B6` to `0x253D0 jmp 0x2540f`) | 0 |
| `0x27A2C` | `game_mode_0e_step` | 374 | one site `0x25371`, then `0x25376 jmp 0x2540f` | 0 |
| `0x11578` | `game_state_4` | 1207 | one site `0x11DD0`, then `0x11DD5 call 0x10db0` and `0x11DDA call 0x10e18`, which load AH alone before any read (`0x10DB1`, `0x10E19 mov ah,[0xf0a71]`) and then `mov eax,0x100` or return, and `0x11DDF call 0x2bf08`, whose twelve sites in `0x11D04` are traced (d1c §1; d3a §1 for `0x10DB0`) | 0 |

`game_state_4` was `static` in `flow.c`; this batch exports it (prototype in `flow.h`) for its row, as
earlier batches did for theirs.

## §2 The rows

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `game_mode_32_step` | `0x296B8` | 8 | 7/11 | 89 | 0 | the prelude, the idle return, the next-character arm; every callee a stub; `n4` side `0x80`, `n5` character -1, `n2` winner byte `0x80` |
| `game_mode_32_step_end` | `0x296B8` | 7 | 11/11 | 38 | 0 | the match-end arm; `0x3C5CC`/`0x16D58` real so that the other arm's callees fit the 16 stubs; `x0` takes the other arm |
| `game_mode_32_step_rr` | `0x296B8` | 1 | 7/11 | 19 | 0 | stubs write what the raw reads after each call (prelude and next-character arm) |
| `game_mode_32_step_end_rr` | `0x296B8` | 1 | 6/11 | 2 | 0 | the same for the match-end arm |
| `game_mode_0d_step` | `0x274FC` | 7 | 4/13 | 79 | 0 | the prelude, the idle return, the next-character arm; `n3` side `0x80`, `n4` character -1 |
| `game_mode_0d_step_end` | `0x274FC` | 8 | 13/13 | 52 | 0 | the match-end arm; `0x3C5CC`, `0x16D58`, `0x46534` real, `0x19068` real and stopped at `0x1906E`; `x0`, `x1` take the other arm |
| `game_mode_0d_step_rr` | `0x274FC` | 1 | 4/13 | 20 | 0 | writing stubs, prelude and next-character arm |
| `game_mode_0d_step_end_rr` | `0x274FC` | 1 | 8/13 | 4 | 0 | writing stubs, match-end arm (the text arm) |
| `game_mode_13_step` | `0x424E8` | 16 | 22/46 | 49 | 0 | the six state arms and the two states above 5; `0x33904` real over poked list entries; the prompt tail silent |
| `game_mode_13_step_prompt` | `0x424E8` | 14 | 31/46 | 23 | 0 | the three prompt loops; `0x2C1C8` real over the stubbed `0x2F388` |
| `game_mode_13_step_rr` | `0x424E8` | 4 | 37/46 | 8 | 0 | writing stubs: states 0, 1, 2, 4 and each prompt loop |
| `frontend_match_start` | `0x1EA08` | 6 | 17/17 | 72 | 0 | `0x2DBC4` real and recorded over poked tables; `0x2F4BC` real over the by-value `0x2F198` stub |
| `frontend_match_start_rr` | `0x1EA08` | 1 | 16/17 | 6 | 0 | the text stubs rewrite the result buffer and the row byte `0xA7B98` |
| `game_mode_0e_step` | `0x27A2C` | 15 | 19/19 | 68 | 0 | every callee a stub; the audit add `0x2DAE4` real in the original only, from its no-op state |
| `game_mode_0e_step_rr` | `0x27A2C` | 2 | 11/19 | 7 | 0 | writing stubs: the continue and the tick |
| `game_mode_0e_step_rr2` | `0x27A2C` | 2 | 11/19 | 2 | 0 | writing stubs: the prompt |
| `game_state_4` | `0x11578` | 12 | 9/9 | 205 | 0 | the five phases and three phase words above 4; `0x2C06C` real; one mutant per field and per line of the 33 text lines |

17 rows on 6 entries, 743 mutants, all detected. The plain rows of one entry cover all its blocks between
them (`0x296B8` 11, `0x274FC` 13, `0x424E8` 46). Each mutant core was also run with no mutation against
every row of its entry (a temporary `@same` binding, not committed): VERIFIED on all 17, so a mutant is
caught by its mutation and not by a difference between the core and the port. Every row and mutant binding
was run ten times on the port side with identical output.

### §2.1 Why several rows on one entry

`diffrun` takes 16 stubs per case. `0x296B8` has 19 callee addresses with the three entrances used
(`0x40CB0`, `0x24568`, `0x24804`, the entries 0, 1 and 6 of the table `0xA8628`), `0x274FC` has 22,
`0x424E8` 19. Each plain row stubs one arm's callees and names the other arm's blocks in `unhit_named`. A
mutant that misjudges the arm (`@cnt5`, `@cntand`, `@cntonly0`, `@cntonly1`, `@cntwins`, `@winsadd2`,
`@nowins`, `@winsother`; `@cnt8`, `@cntge`, `@b21add2`) would run the other arm's callees unstubbed on the
port side, so those mutants live in the `_end` rows, which stub both arms: they make room by running the
leaves `0x3C5CC` and `0x16D58` (and for `0x274FC` `0x46534`, and `0x19068` held at its first compare by the
byte `0x107802` = `0x13`) from their own bytes on both sides. The prelude mutants are in the rows that stub
all five prelude callees.

### §2.2 Stubs, clobbers and seams

`E.callee_clobbers` over the image, for the stubs this batch adds (the expected dictionary of
`test_each_stub_declares_the_registers_its_callee_clobbers` gains these three lines at its top, under
a `# D5 d5i` comment):

```
0x3C5CC: (), 0x16D58: ("edx",), 0x35658: ("edi", "ebp"), 0x19068: ("edi", "ebp"), 0x12DA8: (), 0x28130: (), 0x292D4: (), 0x4DBEC: (),
0x2BAF4: (), 0x38B18: ("ebx", "ecx", "edx"), 0x428B8: (), 0x42BCC: (), 0x29CFC: ("edx",), 0x20E90: ("edi", "ebp"), 0x42724: (), 0x4B9AC: (), 0x1C6D4: (), 0x42CB4: (), 0x33F08: (), 0x4F318: ("ecx", "esi", "edi", "ebp"),
0x4F1E4: (), 0x42F60: (), 0x2791C: ("ebp",), 0x29B74: (), 0x2C1D4: ("ebx", "edx"),
```

Seams added (each the first statement of the function, the C signature in order):

| addr | function | line |
|---|---|---|
| `0x3C5CC` | `fight_slot_clear` | `PR_SEAM0(0x3C5CCu);` |
| `0x16D58` | `camera_screen_base` | `PR_SEAM(0x16D58u, (u32)side, (u32)character);` |
| `0x35658` | `fight_hud_pass` | `PR_SEAM(0x35658u, side);` |
| `0x19068` | `fighter_pass_b` | `PR_SEAM(0x19068u, arg);` |
| `0x12DA8` | `camera_y_commit` | `PR_SEAM0(0x12DA8u);` |
| `0x28130` | `flow_match_result_text` | `PR_SEAM0(0x28130u);` |
| `0x292D4` | `flow_side_char_set` | `PR_SEAM(0x292D4u, side, ch);` |
| `0x4DBEC` | `fight_4dbec` | `PR_SEAM0(0x4DBECu);` |
| `0x2BAF4` | `actors_reset_al` | `PR_SEAM(0x2BAF4u, al);` |
| `0x38B18` | `frontend_spawn_row` | `PR_SEAM(0x38B18u, (u32)((const u8 *)desc - mem), a2, a3);` |
| `0x428B8` | `flow_challenge_open` | `PR_SEAM0(0x428B8u);` |
| `0x42BCC` | `flow_challenge_drop` | `PR_SEAM0(0x42BCCu);` |
| `0x29CFC` | `effects_29cfc` | `PR_SEAM0(0x29CFCu);` |
| `0x20E90` | `flow_scroll_reset` | `PR_SEAM(0x20E90u, stage);` |
| `0x42724` | `flow_challenge_fighters` | `PR_SEAM0(0x42724u);` |
| `0x4B9AC` | `fight_challenge_crowd` | `PR_SEAM0(0x4B9ACu);` |
| `0x1C6D4` | `frontend_resource_known` | `PR_SEAM_RET(0x1C6D4u, rec);` |
| `0x42CB4` | `flow_challenge_poll` | `PR_SEAM0(0x42CB4u);` |
| `0x33F08` | `fight_health_bars` | `PR_SEAM0(0x33F08u);` |
| `0x4F318` | `frontend_mode_17_step` | `PR_SEAM0(0x4F318u);` |
| `0x4F1E4` | `frontend_input_reset` | `PR_SEAM0(0x4F1E4u);` |
| `0x42F60` | `flow_continue_poll` | `PR_SEAM_RET(0x42F60u, side);` |
| `0x2791C` | `flow_continue_take` | `PR_SEAM0(0x2791Cu);` |
| `0x29B74` | `frontend_darken_all` | `PR_SEAM0(0x29B74u);` |
| `0x2C1D4` | `prompt_insert_coin_blink` | `PR_SEAM(0x2C1D4u, (u32)col, (u32)row);` |

A seam is inert unless a case names its address as a stub, and no row before this batch names any of the
25; the one full self-check (§7) is what proves no existing row changed.

Argument notes from the callees' bytes: `0x16D58` reads AX and DX (`0x16D59 test ax,ax`, `0x16D66 test
dx,dx`); `0x19068` AL (`0x19088 test al,al`); `0x292D4` EAX and DL (`0x292D5 mov ebx,eax`, `0x292E8 mov
[ebx+0x10816a],dl`); `0x2BAF4` AL (`0x2BAFD mov [esp],al`); `0x38B18` EAX, EDX, EBX (`0x38B1B mov esi,eax`,
`0x38B1D mov ebp,edx`, `0x38B51 shl ebx,3`); `0x20E90` hands EAX to `0x38730` untouched; `0x42F60` EAX
(`0x42F61 mov edx,eax`); `0x2C1D4` EAX, EDX. The others read no register before writing it.

## §3 Corrections (the raw wins; each in its own commit)

1. **`game_mode_13_step` tested the whole return of `0x1C6D4`** — `0x42594 call 0x1c6d4`, `0x42599 test
   al,al`, `0x4259B jne 0x425ab`. Old: `if (frontend_resource_known(e) != 0u) continue;`. New: `if
   ((frontend_resource_known(e) & 0xFFu) != 0u) continue;`. The raw's EAX at `0x42599` is the entry's handle
   with AL replaced (`0x1C6D4 mov eax,[eax]`, then `xor al,al` or `mov al,1`). The port's own
   `frontend_resource_known` returns 0 or 1, so no game path changes. Reproduced by the case `w2` (stub EAX
   `0x100`: before the correction `call #5: original 0x13C70(0x107628, 0x3, 0xAB1111), port
   0x1C6D4(0x107788)`) and the mutant `@knowneax`.
2. **`frontend_match_start` kept the row byte across the row's four calls** — the raw loads `0xA7B94[i]`
   before each: `0x1EB4C mov dl,[edi*4+0xa7b94]` (`0x2F4D0`, the rank), `0x1EB98` (`0x2F4BC`), `0x1EBB9`
   (`0x2F4D0`, the value), `0x1EBD4` (`0x2F4BC`). Old: `s32 row = DSB(0xA7B94u + i * 4u);` read once and
   passed four times. New: each call reads the byte. The table is constant data in the image, so no game path
   changes. Reproduced by the `_rr` row (before the correction `call #10: original 0x2F4D0(0xBE, 0x66,
   0xBADC0DE, 0x7, 0x1, 0x3000), port 0x2F4D0(0xBE, 0x77, ...)`) and the mutant `@prerow`.

Both are unproven against the oracles until the integrator's full gate (this batch runs none of them).
`0x296B8`, `0x274FC`, `0x27A2C` and `0x11578` verified with no port change.

Seen, not changed (outside the queue): `flow.c` has two more whole-return tests of `frontend_resource_known`
(`if (!frontend_resource_known(node))` and `if (frontend_resource_known(rec) != 0u) continue; /* 0x126F3 */`).
Their raw instructions were not read here: unverified.

## §4 Named gaps, limits and unhit blocks

- **`0x1EC28`, the palette call after the spawn.** The raw calls `0x2A17C(EAX of 0x2AE14, 0, 0x105FD30)`
  unconditionally; the port skips it when the spawn returns 0 (`/* PORT: */` in `frontend_match_start`: the
  pool was reset at `0x1EA26`). Every case's spawn stub returns a non-zero record; a 0 would be a `call`
  difference. A declared deviation, not corrected here.
- **`0x425D1`, the scene call `[0xC7F58 + stage * 4]`.** The eight entries are `0x412EC` (a `ret`) and
  `0x5D812` (`xor eax,eax; ret`); the port does not issue it (`/* PORT: */`). Both run from the original's
  bytes only (allow-mode), stages 0, 3 and 7; a stage above 7 indexes past the table and is not cased.
- **`0x2DAE4(0x11, 1)` at `0x27A5F`** (the audit add, host-owned/deferred): real in the original only, with
  field `0x11`'s descriptor `0x2D344` poked so that `0x2D974` returns a negative value and `0x2DAEE jl`
  leaves at once (`D4A_AUDIT_OFF_D`'s form). Its effect is not compared. `0x29D60` (a bare `ret`) is allowed.
- **`0x2C1C8` cannot be a stub under `0x424E8`.** Its three loops keep ECX, ESI and EDI across the call;
  `0x2C1C8` is `mov al,[eax+0xbab58]; and eax,0xff; nop` falling into `0x2C1D4`, whose `push ecx/esi/edi`
  the scan cannot credit to it, so the declared clobber set (`ebx ecx edx esi edi`, shared with the d3a and
  d3b rows) would poison the loop counter. It runs real at the phase `0xEF6DC & 0x1F == 0x18`, where it
  only releases the side's cells through the stubbed `0x2F388`, which names the side by its col byte. A
  stub at `0x2C1D4` is not reached: a fall-through is not an arrival (`diff_emu` §E3.2).
- **`0x1EA98`, the champion name's terminator** (`mov [esp+0x12],ah`) is outside the sixteen bytes the
  `0x2F198` seam takes by value: not observed (no `@nonul` mutant for it). The three-character names'
  terminator at `+3` is (`@rnonul`).
- **Characters.** Only the entrances of characters 0, 1 and 6 are stubbed (and -1 through a poked
  `0xA8624`); the other four table entries are not called by any case.
- **Sides above the image.** The side byte `0x80` (zero-extended) puts the slot at `0x10C1B0`, above the
  image and inside mem[]: its record pointer reads 0, so the `+0x48` store is the byte `0x48`. Named in
  `D5I_OUTSIDE` (`game_mode_32_step`, `game_mode_32_step_end`, `game_mode_0d_step`).
- **Not separable, by the bytes:** `0x2764B test dx,dx` (the remainder is below 100, so its low word is the
  whole value); the drawn count at `0x27B49` is never negative (the branch at `0x27AF5`), so its sign
  extension is not shown; `0x42606 test eax,ebp` reads only bits 0 and 1 of the dword `0x104AB8`.
- **Unhit blocks** are named per row in `unhit_named` and are each hit by another row of the same entry;
  the lists are exact (no named block is hit).

## §5 Store sweep

Every field a function writes is seeded with a value different from what is written, between seeded
neighbours; each store has a dropped-store mutant that is detected:

- `0x296B8`/`0x274FC`: `0x1077E8`, `0x10787C` (`@nolatch0`, `@nolatch1`, `@latch0w`), `0x104AEC` (`@noaec`,
  `@aecset` on the `0xFF` case), `0x104B0C` (`@nob0c`, `@b0cw`), `0x104B21` (`@nob21`), the record's `+0x48`
  (`@norec48`, `@rec48w`, `@rec48off`), `0x104AF0[s]` (`@nowins`), `0x1078FA` (`@nofa`, `@faw` on the 0
  case), the slots' `+0x41` (`@noe41a`, `@noe41b`, `@noe41`, `@noother41`, `@now41`), `0x10810D` (`@now`),
  `0xF0AFE`/`0xF0AFF` (`@nofafe`, `@nonfafe`, `@nofaff`), `0x104B00` (`@nob00`, `@b00b`, `@nomode`,
  `@modeb`), `0x104AFE` (`@noafe`, `@afeb`), the slot's `+0x63` and `+0x7A` (`@no63`, `@no7a`, `@w7a`),
  `0x104B0A` (`@nob0a`).
- `0x424E8`: `0x104B15` (`@nob15`, `@b15w`), `0x104B25` (`@s0noinc`, `@s2noinc`, `@s3noinc`).
- `0x27A2C`: `0x104B1F` (`@nob1f`, `@b1fw`), `0x108110` (`@no8110`, `@nodec`), `0x105C04` (`@noc04`,
  `@c04w`), `0x104B25` (`@nob25`), `0x104AFA` (`@noafa`, `@afab`), `0x104AEC` (`@noaec`).
- `0x11578`: `0xF0A76` (`@notimer`, `@timerb`, `@nodec`), `0xF0A74` (`@nonext`), `0x9AD98` (`@nophase`,
  `@nophase3`, `@nocont`), `0xF0A6C`, `0xF0A64`, `0xF0A6A` (`@no6c`, `@no64`, `@no6a`).
- `0x1EA08` stores nothing in mem[] itself (its frame is private); its calls carry every value.

A store moved across a call is caught by the memory at the call alone (`@latchearly`, `@aecearly`,
`@wlate`, `@b15early`, `@s0incearly`, `@s2incearly`, `@s3inclate`, `@b25early`, `@rowsetlate`).

## §6 Queue entries not done

- `0x11000` (the attract sub-machine, 1380 bytes, 49 blocks, 13 phases on the byte `0xF0A6F`, 16 callee
  addresses, one of them the named gap `0x1C740`): not started beyond its block scan and its caller trace
  (`0x11D70`, then `0x10DB0`/`0x10E18`/`0x2BF08`, as `0x11578`). No finding either way.

## §7 Counter

One full `PR_DIFF_JOBS=4 python3 tools/diff_verify.py --image /tmp/pr_d5i_img.bin --self-check` on the
tree of the batch's last commit (base `3790f323`, the wave-4 merge of d4d, plus this batch): `rc 0`,

```
diff-verify: 801/801 functions VERIFIED; 9963/9963 mutants detected; 1 named gaps; 504/634 rows with callees closed (167 have none).
```

of which this batch's 17 rows and 743 mutants. The harness files are additive against the base (`git diff
--numstat 3790f323`: 0 deletions in `tools/diff_verify.py`, `port/tests/diff_runner.c` and
`tools/tests/test_diff_verify.py`). Not run here: the full unittest suite, `make verify`, any oracle.
