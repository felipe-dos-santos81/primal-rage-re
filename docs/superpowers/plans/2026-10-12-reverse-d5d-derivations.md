# Track D, wave 5, batch d5d — derivation record

Differential rows for the queue `.superpowers/sdd/2026-10-11-reverse-d1/d5d-queue.txt` (22 functions of
`game/flow.c`, `attract.c`, `nameentry.c`, `menu.c`, `config.c` and `fight.c`). Branch `reverse-d5d` off
`3790f323` (waves 1 to 4 merged up to d4d). Ground truth: the fixed-up image `build/diffrun --image-out`
dumps (linear addresses), disassembled with capstone 5.0.7. Every figure in §2 is measured with `python3
tools/diff_verify.py --image … --function <name> --self-check`; the pins in `tools/tests/test_diff_verify.py`
(`D5D_MASKS`/`D5D_KINDS`/`D5D_CASES`/`D5D_OUTSIDE`) come from `verify_all` on the batch's own specs. The
claim of each row is the track's: equivalence on the exercised blocks and inputs only, with the callees
stubbed or run as stated.

Result: 19 of the 22 entries are rowed, in 26 rows (seven functions have a second, "reread" row whose stub
rewrites a value the raw loads again): 162 cases, 423 mutants. Three entries are named gaps (§4, §6). **Six
address-proved port corrections** (§3): five re-reads of a value across a call and one call the port left
out. None changes a state the game can reach as the port stands (§3 says why for each); all are unproven
against the oracles until the integrator's full gate.

## §1 The members

EAX masks. The liveness of EAX after each call site was walked mechanically (a scratch script, not
committed: from the instruction after the call, following both arms of every conditional jump and
descending into direct callees, until every part of EAX — AL, AH, the high word — is overwritten; a `ret`
out of the calling function continues after each of that function's rel32 callers, three levels up; an
indirect call or jump, a stored pointer or the depth limit is reported, never assumed dead; `push eax`
counts as a read; `and eax,imm` kills the parts its mask clears). "dead" below means that walk found no read.
Its limits: stored-pointer callers of an *enclosing* function beyond the first are reported, not followed.

| addr | port function | size | callers and what they read of EAX |
|---|---|---|---|
| `0x4F200` | `flow_screen_reset` (flow.c) | 38 | One caller, `0x430F9`, then `call 0x25848`, which loads AL (`0x2584E`) and clears or reloads EAX on every path (`xor eax,eax` `0x2586B`, `0x258E7`) or returns at `0x259C3`; dead. The raw returns `0x2BAF4`'s EAX. Mask 0. |
| `0x292AC` | `attract_scene_tick` (attract.c) | 39 | One caller, `0x255F3`, then `cmp byte [0x107a54],0` and `0x389C4` / `0x24C5C`, which load EAX first; dead. Mask 0. |
| `0x10EE4` | `attract_state_reset` (attract.c) | 66 | One caller, `0x11EF6`, then `call 0x10db0; call 0x10e18; call 0x2bf08`; dead. Mask 0. |
| `0x10E80` | `game_state_init` (flow.c) | 98 | One direct caller, `0x20CE6` (then `call 0x5d808`, `mov eax,esp`), and a hook value (`mov ebx,0x10e80` `0x25B5F`, stored to `0x104AE4` at `0x25B6E`; the dword at `0x24E0F` is the immediate of `cmp dword [0x104ae4],0x10e80` `0x24E09`). The six `call [0x104ae4]` sites (`0x4F302 0x4F373 0x4F6F1 0x4F70D 0x4F9AA 0x4F9D1`) were walked from their return addresses: dead at all six. Mask 0. |
| `0x4F9E4` | `frontend_wipe_in` (flow.c) | 161 | One caller, `0x4F9A1`: `test eax,eax` (`0x4F9A6`). Mask `0xFFFFFFFF`. |
| `0x4F9A0` | `frontend_mode_1a_step` (flow.c) | 38 | One caller, `0x25403`, then `jmp 0x2540f` (the mode tail, `call 0x2a31c`); dead. Mask 0. |
| `0x416D4` | `frontend_mode12_darken_or_tally` (flow.c) | 137 | One caller, `0x420FE` (state 5 of `0x41C28`); dead. Mask 0. |
| `0x25AE8` | `game_hook_25ae8` (flow.c) | 212 | One direct caller, `0x253D9`; dead. Also a hook value (`mov edx,0x25ae8` `0x296A3`, stored at `0x296AF`; `mov eax,0x25ae8` `0x42ECF`, stored at `0x42ED9`): the six hook sites above, dead. Mask 0. |
| `0x1ED2C` | `nameentry_reset` (nameentry.c) | 320 | Five callers (`0x1EEEA 0x1EFB4 0x1F0A5 0x1F1E2 0x1F316`); dead at each. Mask 0. |
| `0x2FE84` | `menu_title_draw` (menu.c) | 320 | Four callers (`0x2FB03 0x2FD8A 0x300BD 0x30495`); dead at each. Mask 0. |
| `0x2DA0C` | `config_field_set` (config.c) | 213 | 20 call sites; two reach a read of AL through `0x1E988`'s return (`and eax,0xff` `0x1E884`). The raw returns -1 (`mov eax,0xffffffff` `0x2DA16`) or 0 (`xor eax,eax` `0x2DAD9`) and the port returns the same: mask `0xFFFFFFFF`, no caller analysis needed. |
| `0x2DCA0` | `hiscore_insert` (config.c) | 321 | Six callers, dead at each. The raw returns EBX = 1 or 0 (`mov eax,ebx` `0x2DDD7`) and so does the port: mask `0xFFFFFFFF`. |
| `0x1E918` | `hiscore_fill_defaults` (flow.c) | 61 | Two callers (`0x1E8AB`, `0x1E8F0`); dead. Mask 0. |
| `0x1EC38` | `hiscore_rank_single` (flow.c) | 141 | Six callers; two read AL (`test al,al` `0x1F1B0`, `0x1F2CE`), the others are dead. The function sets AL alone (`xor al,al` `0x1EC9F`, `mov al,1` `0x1ECB8`). Mask `0xFF`. |
| `0x20710` | `nameentry_finish` (nameentry.c) | 336 | One caller, `0x1F4C6`; dead. Mask 0. |
| `0x4E5A4` | `fight_4e5a4` (fight.c) | 214 | One caller, `0x4E84E`; dead. Mask 0. |
| `0x4B69C` | `fight_4b69c` (fight.c) | 233 | One caller, `0x49CFE`; dead. Mask 0. |
| `0x4D7A4` | `fight_4d7a4` (fight.c) | 219 | One caller, `0x4D323`; dead. Mask 0. |
| `0x4C60C` | `fight_4c60c` (fight.c) | 374 | One caller, `0x4C21B`; dead. Mask 0. |

**Exports (linkage only).** Three members were file-`static` and a binding cannot call a static function.
Following the precedent of the earlier waves (d1c, d1e, d1h, d2a…), each loses `static` and gains a
prototype with a comment naming this record: `frontend_mode12_darken_or_tally` (flow.c / flow.h),
`fight_4b69c` and `fight_4e5a4` (fight.c / fight.h). No body changes with the linkage.

## §2 The rows

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `flow_screen_reset` | `0x4F200` | 3 | 1/1 | 12/12 | 0 | EAX = v. The byte `0x107A55` = AL and the byte `0x107A54` = DH = 0 (`and eax,0xff; mov edx,eax; xor dh,ah`: `s1` has AH `0x56`, `s2` `0xFF`), both before `0x4F1D0` (the memory at the call), then `0x2BAF4` with EAX = 1. Both callees are stubs. |
| `attract_scene_tick` | `0x292AC` | 6 | 6/6 | 8/8 | 0 | The mask dword `0x104AD0` is read once (`0x292AE`: the `0x4F7F4` stub zeroes it and the walk goes on, as the real one clears its bit 0) and walked from bit 0 by `shr edx,1` (`t2`: bit 31 alone); each set bit calls the dword at `0xA8744` + 4 × the bit. `t3` walks all 32 bits over a poked table of three targets with no period. `0x4F7F4` and `0x4F1D0` are stubs; `0x5D812` (`xor eax,eax; ret`, unregistered in the port: `fn_resolve`'s miss reports it to the seam) runs and is recorded. The raw hands the callee EAX = the byte offset (`mov eax,ebx` `0x292BF`); the port's `fn_resolve` form has no argument (its `PORT:` note), so that EAX is not compared. |
| `attract_state_reset` | `0x10EE4` | 2 | 1/1 | 14/14 | 0 | `0x32970(0)` (the run clock: host-owned, the port omits it; it runs in the original from its no-elapsed-time state, `d1i_pt_case`), `0x4F1E4`, `0x2BAF4` with EAX = 1, then the word `0x104B00` = DX = 3 (loaded at `0x10EEF`; both callees save EDX), the word `0xF0A64` = BX = 0, the bytes `0xF0A71` = AH = 0 and `0xF0A6F` = DL = 0. `0x4F1E4` and `0x2BAF4` are stubs: the memory at their calls shows the stores come after. |
| `game_state_init` | `0x10E80` | 2 | 1/1 | 23/23 | 0 | The same head, then the dword `0xF0A5C` = 4, `0x2C304` (a stub: the five stores before it are in the memory at its call, the two after are not), the dword `0x104AB8` = EBX = 0 and the byte `0x104B1F` = DH = 0. `@twin` runs the other function's shape in each of the two rows. |
| `frontend_wipe_in` | `0x4F9E4` | 6 | 5/5 | 23/23 | `0xFFFFFFFF` | With no actor in `0xC98F0`: the spawn `0xC98F4` at (0, `0xFF`, 0, 0) into it and `0x10150C` = `0x101508`. Then on the signed counter `0x1088F5` (`sar eax,0x18` of the dword `0x1088F2`; `jle` against `0x10`: `w3` is `0x80`, `w5` `0xFF`): above it `0x2B150`, `0xC98F0` = 0, `0x2BAF4` with EAX = 1, the gate byte `0x1088F4` = 1 after the call, EAX = 1; else `0x10D70(the actor, the word 0xC991C[n])` after the counter is incremented, EAX = 0. Four stubs. The cases are the twin `0x4FA88`'s (`d2c_wipe_case`). |
| `frontend_mode_1a_step` | `0x4F9A0` | 4 | 3/3 | 10/10 | 0 | When `0x4F9E4` returns non-zero (whole: `w2`'s stub EAX is `0x100`), `call [0x104ae4]`, then the counter byte `0x1088F5` = 0 (AH after `xor ah,ah`) and the mode word `0x104B00` = `0x1B`. The hook is `0x5D812` (recorded: the stores come after it) or the registered `0x28D68` run on both sides, which itself leaves `0x104B00` = `0x1A` (`w2`). |
| `frontend_mode12_darken_or_tally` | `0x416D4` | 8 | 4/4 | 11/11 | 0 | With the result dword `0x104AD4` == 2 (whole: `d5` has `0x102`) only `0x41578`. Else `0x4660C(the column byte 0x108104[r], zero-extended)`, the dead `0x32BAC(char[r], char[r ^ 1])` (a bare `ret`: it runs in the original, the port drops it and the two reads before it), then `0x4160C` when the column byte is 7 and `0x41578` when not. Three stubs. `d5` reads two character bytes above the image for the dead call (`D5D_OUTSIDE`). |
| `…_rr` | `0x416D4` | 3 | 4/4 | 1/1 | 0 | A `0x4660C` stub that rewrites the result dword: the raw reads it again for the column test (`0x41738`). Found correction C1. |
| `game_hook_25ae8` | `0x25AE8` | 5 | 3/3 | 31/31 | 0 | The voice `0x100`, the byte `0x104B14` = AH = 0 (after the voice: `h3`'s stub returns `0xFFFFFFFF`), `0x4F1E4`, `0x2BAF4` with EAX = 1, the dword `0x104ABC` = (the byte `0x104B1F` == 3) + 1, string `0x52` held by `0x2F510(-1, 0xE, it, 0x4000)` (`0x1C500` real over the stubbed `0x474E4`; the string's first dword by value), `0x4246C`, the voice `0x3D`, then by the byte `0x104B1D` the hook dword `0x104AE4` = `0x10E80` (zero) or `0x24B54`, the words `0x1088EE` = `0x104AFE` = `0xB4` and the mode word `0x104B00` = `0x17`. |
| `nameentry_reset` | `0x1ED2C` | 2 | 3/3 | 29/29 | 0 | The cursor words `0x1044D0` = `0xB`, `0x1044D2` = 6; `0x4F1E4`; `0x2BAF4`; `0x38B18(0xA7B80; 0,0 / 0x2A,0 / 0,0x1E / 0x2A,0x1E)`; three spawns and two `0x2BCF4` (§5); the +0x12 byte of the 18 cells `0x104114 + 0x14k` = 0; the dwords `0x1044A8`, `0x1044C8`, `0x1044BC` and the byte `0x1044D8` = 0. Every callee is a stub; one stub EAX serves a case's three spawns, so the three record dwords are told apart by the memory at each later call. |
| `nameentry_reset_rr` | `0x1ED2C` | 1 | 3/3 | 4/4 | 0 | A `0x38B18` stub that rewrites the cursor words (column `0x8001`, row `0x0123`): the first spawn's x and y are read back signed (`mov eax,[0x1044ce]; sar eax,0x10` `0x1EDB5`), not taken from the `0xB`/6 just stored (`@cached`, `@zext`). Its `0x2BCF4` stub rewrites `0x1044B4`: each `0x2BCF4` gets the spawn's EAX, not the stored dword (`@seekrec`). The port agrees on both. |
| `menu_title_draw` | `0x2FE84` | 19 | 25/25 | 38/38 | 0 | EAX = entry, EDX = mode_a, EBX = mode_b, ECX = flags (their low byte: `test byte [esp+0x2c],1` / `,4`). `0x4F1E4`, `0x2BAF4`, `0x4F1D0`, `0x38B18(0x9AD84, 0, 0)`, with flag bit 0 `0x2F940(0x1B)`; the entry's first string without one leading `?`, then one `0x0B` and the byte after it, then one `0x0A` (in that order: `m2..m5`, `m8`, `mc`) is copied into the frame up to `0x2A` characters, then, with a second id, a space and the second string up to `0x2A` in all. The width in mode_a \| 2 (`0x2F0F0`, run on both sides: the title is in the raw's frame) decides: above `0x28` every `_` becomes a space and the title is drawn in mode_a, else in mode_a \| 2. The six `t` cases put the `0x2A` limit on the 40/41 width boundary with zero-width and one-wide glyphs, so a limit one off flips the mode. `0x1C500`'s tree runs on both sides. |
| `config_field_set` | `0x2DA0C` | 15 | 12/12 | 24/24 | `0xFFFFFFFF` | A field above `0x3E`, unsigned (`jbe` `0x2DA14`: `f1` -1, `f2` `0x80000000`), returns -1. Else the descriptor `0x2D300[field]` (poked per case): a trail byte, the flag byte `0x105DD8` (read before the trail store: `c9`'s trail is the flag byte itself), `0x2D4EC(0)`; flag \|= 6; width nibbles at an even or odd nibble position of `0x105DE1`; `0x2D4EC(1)`, `0x2D4EC(2)`, 0. `0x2D4EC` is a stub. |
| `hiscore_insert` | `0x2DCA0` | 12 | 29/29 | 22/22 | `0xFFFFFFFF` | `0x2DB58` real on both sides, the runtime memmove `0x653A1` in the original (under the flat segments, §7). Flag bit, the overlapping move, the big-endian value over the descriptor's byte count, the name words (a space consumed, the NUL not), then `0x2D4EC(table + 6)`. Found correction C2. |
| `hiscore_fill_defaults` | `0x1E918` | 6 | 6/6 | 12/12 | 0 | For i = 0..9: `0x2DBC4(i, 0)` (real on both sides over the case's table) and, when its value dword is 0 (whole) or force is non-zero (whole: `d2` `0x100`, `d3` `0x80000000`), `0x2DCA0(i, 0xA7BBC + 0x2C i, 0)` (a stub). `d5` has five records: `0x2DBC4` returns 0 and the raw reads the dword at address 0 (`D5D_OUTSIDE`). |
| `hiscore_rank_single` | `0x1EC38` | 6 | 5/7 | 17/17 | `0xFF` | `0x2DDE4(score, 0)`; the rank is its low word, zero-extended (`k3` -1, `k4` `0x00010003`, `k5` `0x8009`); below 10: `0x204F4(rank word, score)`, `0x2DA0C(0x26, 0)`, the word `0x1044D6` = the rank, AL = 1; else AL = 0. Two blocks named, not hit (§4). |
| `nameentry_finish` | `0x20710` | 7 | 14/14 | 23/23 | 0 | See the spec comment: the filter, the copy loop under the signed limit byte, the character's letter, the table-1 insert above a limit of 3, the table-0 insert at the rank word, two text calls and three `0x2B150`. Every callee is a stub. |
| `nameentry_finish_rr` | `0x20710` | 2 | 14/14 | 1/1 | 0 | A `0x2DCA0` stub that rewrites side 1's character byte: the raw reads it again at `0x207B8`. Found correction C3. |
| `fight_4b69c` | `0x4B69C` | 8 | 9/9 | 28/28 | 0 | Only with +0x1C bit 7: `0x17D30(the pset words +4, +8 sign-extended, 0)`; 0 returns; above 2, signed, counts as 1 (`b5` 3, `b6` -1, `b7` `0x80000002`); `0x4B788(it, entry, si)` tested whole (`b4`: `0x100`); +0x20, +0x1F, the release of a held actor (+0x4A zero-extended: `b5` `0x80`), `0x4B470`. Three stubs. |
| `fight_4b69c_rr` | `0x4B69C` | 3 | 9/9 | 1/1 | 0 | A `0x4B788` stub that points +8 at another record. Found correction C4. |
| `fight_4d7a4` | `0x4D7A4` | 8 | 9/9 | 28/28 | 0 | The mode-0x22 twin: gate `0x4D898`, no `0x1088B2` store (`@b2` adds it here and drops it in `fight_4b69c`'s row; `@gate` calls the other mode's gate). |
| `fight_4d7a4_rr` | `0x4D7A4` | 3 | 9/9 | 1/1 | 0 | The same reread; the port already re-reads (`@cached` is caught). |
| `fight_4c60c` | `0x4C60C` | 16 | 18/18 | 36/36 | 0 | The hit as above; the side's character byte +0x7A above 6, unsigned (`c9` 7, `ca` `0x80`), does nothing; the table `0x4C5F0`: 0, 3, 5 with the slot's +0x5F == 0 and 2 with +0x5F == 1 are eaten (`0x4C784(side)`), the rest struck: `0x108898` = 0, +0x20, +0x1C bit 3 cleared, +0x1F, the release, `0x4CB18(entry, si, flag, side)` with flag 1 and +0x1C bit 3 set after it when +0x5F is `0xC..0xF`. `cb`'s hit -1 selects the slot two below slot 0 (inside the image). |
| `fight_4c60c_rr` | `0x4C60C` | 5 | 18/18 | 1/1 | 0 | A `0x17D30` stub that points +8 at another record; the port already re-reads. |
| `fight_4e5a4` | `0x4E5A4` | 8 | 8/8 | 23/23 | 0 | Only with +0x1C bit 7: the hit; `0x1088BA` = 1; +0x20, +0x1F; `0x2BC30(the actor, 0xC9604[idx], 3.0f)`; with no partner the spawn `0xBB920[idx]` at (the actor's +0x18, its +0x30 `sar 16`, 0, 0) into +0x10; +0x34 = `0xFFA0`, +0x36 = `0x240`, +0x1C bit 7 cleared, the actor's +0x28 bit 7 set, +0x1E = 2. idx is ECX whole (`f7`: `0x10002`, its two table dwords poked). |
| `fight_4e5a4_rr` | `0x4E5A4` | 2 | 8/8 | 2/2 | 0 | A `0x17D30` stub and a `0x2BC30` stub that each point +8 at another record. Found correction C5. |

Callee stubs this batch adds and their clobber sets (`E.callee_clobbers` on the image; a script re-derived
each and checked that every `E.Call` of the same address in `SPECS` declares the same set): `0x2BAF4` `()`,
`0x4F1E4` `()`, `0x4F7F4` `()`, `0x2C304` `()`, `0x4F9E4` `(edi ebp)`, `0x4246C` `()`, `0x38B18` `(ebx ecx
edx)`, `0x2F940` `()`, `0x2D4EC` `()`, `0x2DCA0` `(ebx edx)`, `0x2DA0C` `(edx)`, `0x204F4` `(edx)`, `0x13EF0`
`()`, `0x17D30` `(ebx edx)`, `0x4B788` `(ebx edx)`, `0x4D898` `(ebx edx)`, `0x4B470` `(edx)`, `0x4C784` `()`.
No caller in these rows reads a poisoned register after a stub: each reloads what it uses (`0x1ED2C` reloads
EBX/ECX/EDX before every `0x38B18`; `0x4B69C` and its twins `mov ebx,esi; mov edx,ecx` after `0x17D30`;
`0x10EE4`/`0x10E80` keep DX = 3 across `0x4F1E4` and `0x2BAF4`, which save EDX).

## §3 Corrections

Each is in its own commit; the row that found it has a mutant with the old shape.

| | function | raw address | old | new | the instruction | found by |
|---|---|---|---|---|---|---|
| C1 | `frontend_mode12_darken_or_tally` (flow.c) | `0x41738` | r (the dword `0x104AD4`) read once, before `0x4660C` | read again before the column test | `mov eax,[0x104ad4]` `0x41738`; `mov al,[eax+0x108104]` `0x4173D` | `…_rr` `r0: call #1: original 0x4160C(), port 0x41578()`; `@cached` |
| C2 | `hiscore_insert` (config.c) | `0x2DDCD` | no call (a `PORT:` note) | `config_storage_touch(table + 6)` when the descriptor's +2 word is above rec | `mov ax,[edi+2]` `0x2DDBE`; `cmp eax,esi; jbe` `0x2DDC2`; `mov eax,[esp+0xc]; add eax,6; call 0x2d4ec` `0x2DDC6..0x2DDCD` | `hiscore_insert` `i1: call #0: original 0x2D4EC(0x6), port none`; `@notouch` |
| C3 | `nameentry_finish` (nameentry.c) | `0x207B8` | the side's character byte read once, before `0x2DCA0` | read again for the `0x104397` letter | `xor eax,eax; mov al,[ecx+0x10782a]` `0x207B6/0x207B8` | `…_rr` `r0: byte 0x104397: original 0x57, port 0x56`; `@cached` |
| C4 | `fight_4b69c` (fight.c) | `0x4B719` | the entry's actor (+8) read once, before `0x17D30` | read again after the gate `0x4B788` | `mov eax,[ecx+8]` `0x4B719` (and `0x4B740`, `0x4B747`, `0x4B74E`) | `…_rr` `r0: byte 0x10A34A: original unchanged, port 0x00`; `@cached` |
| C5 | `fight_4e5a4` (fight.c) | `0x4E619`, `0x4E633` | the node's actor (+8) read before `0x17D30`, used for the `0x2BC30` call and the spawn's arguments | read again after `0x17D30` and again after `0x2BC30` | `mov eax,[esi+8]` `0x4E619`; `mov eax,[esi+8]` `0x4E633` (`mov ecx,[eax+0x30]`, `mov edx,[eax+0x18]`) | `…_rr` `r0: call #1: original 0x2BC30(0x10A400, ..), port 0x2BC30(0x10A300, ..)`, `call #2: original 0x2AE14(0xBB574, 0x123422, 0xFFFF8BCD, ..), port 0x2AE14(0xBB574, 0x123400, 0xFFFF80FF, ..)`; `@cached1`, `@cached2` |

C5 is two re-reads in one commit (adjacent lines of one function); six corrections by address.

**What they change in the game: nothing reachable, as the port stands.** C1, C3, C4 and C5 differ from the
old code only when the callee between the two reads writes the value read (`0x4660C` the result dword,
`0x2DCA0` a fighter slot's character, `0x4B788` / `0x17D30` / `0x2BC30` an effect entry's actor pointer); the
port's functions for those addresses do not. C2 adds a call to `config_storage_touch`, the port's declared
no-op (the EEPROM image is not kept, spec §7), where `config_field_set` already keeps its three. So no frame,
trace field or RNG draw can move; `./build/prageport --check 820` and `run_tests` were re-run with each in
the tree before its commit (C4 and C5 together). The oracles were not run by this batch.

Seen, not changed:

- `0x2CADC` builds field `0x2A`'s new value as `and al,0xfc; or al,3; mov edx,eax` (`0x2CB36..0x2CB3F`): the
  upper 24 bits of `0x2D974`'s result are kept. The port passes `(v & 0xFC) | 3`. Field `0x2A` is one nibble
  wide (descriptor `0x1B80`), so `0x2D974` cannot return more than `0xF` and `0x2DA0C` stores the low nibble
  alone: no difference can be observed. `0x2CADC` is not a row (§4).
- `0x1ED2C`: the raw passes each `0x2BCF4` the spawn's EAX where the port re-reads the dword it just stored
  (`0x1044B4`, `0x1044B0`); nothing sits between the store and the call, and `nameentry_reset_rr` covers it.

## §4 Named gaps and unhit blocks

**`hiscore_rank_single`: two blocks named, not hit (`unhit_named`).** `0x1EC61` (`mov edx,0x7d0; mov
eax,0x27; call 0x2da0c`) and `0x1EC90` (`mov edx,0xc8; mov eax,0x26; call 0x2da0c`) are the clamps behind the
audit adds `0x2DAE4(0x27, 1)` (always) and `0x2DAE4(0x26, 1)` (the losing arm), reached when the running
total passes 2000 (`and eax,0xffff; cmp eax,0x7d0; jle` `0x1EC55..0x1EC5F`) or 200 (`cmp eax,0xc8; jle`
`0x1EC89`). The port omits the adds (`tools/port_classification.txt`: `2DAE4` deferred) and the clamps with
them, under its `PORT:` note. In the row the adds run in the original from a state in which each returns at
once (both descriptors read a negative value, `test eax,eax; jl 0x2db54` `0x2DAEE`: `D5D_AUDIT_OFF`, as d2e,
d3a and d4a do for other fields). So the row says nothing about the audit totals of fields `0x26`/`0x27`.

What the rows do not separate:

- **The pset index's extension** (`0x4B69C`, `0x4D7A4`, `0x4C60C`, `0x4E5A4`): `and edx,0xffff` (`0x4B6C2`)
  zero-extends the actor's +0x56 word. An index of `0x8000` or more puts the pset entry `0x100000` bytes
  above the pool, outside the image for any pool base inside it, where both a zero- and a sign-extending
  port read the load's zeros. Not cased.
- **`menu_title_draw`: the title past its first 16 bytes.** The `0x2F198` seam passes the string's first 16
  bytes by value (the title is in the raw's frame and in a C array in the port). What the copy loops and the
  `_` replacement do beyond byte 15 shows only through the width (`0x2F0F0`) and the mode it selects; a `_`
  past byte 15 is not seen replaced. Every built title has at least 15 characters, because the bytes after a
  shorter title's NUL are frame residue on one side and uninitialised C stack on the other.
- **`attract_scene_tick`: the EAX the callee gets** (the table offset; §2).
- **`nameentry_reset`: which spawn's record each `0x2BCF4` gets** in the main row (one stub EAX per case);
  the `_rr` row separates the second call's.
- **`frontend_wipe_in` / `frontend_mode_1a_step`**: nothing beyond the twin rows' limits (d2c, d3a).

Functions that are not rows, with the evidence (measured with `E.run_original`, `/tmp/pr_d5d/gaps.py`):

- **`0x249F0` (`game_quit_prompt`): `int 0x16`.** With its callees stubbed the original stops `unmodeled:
  int at 0x24A64` in both arms (AL = 0 and 1), after `0x1D250()`, `0x1C500(0x1EE` or `0x1EF)`, `0x2F198(-1,
  0xA, the string, 0x1000)`, `0x2EA78(-1)`, `0x1C500(0x1F0)`, `0x1C500(0x1F1)`. The BIOS key read is inline
  (`xor ah,ah; mov dh,ah; int 0x16` `0x24A60..0x24A64`) at the head of the loop every path enters; the port
  reads through `input_get_key` (host). A `gap=` spec was not added: the suite's
  `test_every_ported_function_agrees…` excuses only `host_1b890` by name, an existing line.
- **`0x2CADC` (`config_set_defaults`): the port omits three of the raw's ten calls by design.** Every callee
  stubbed, the original returns after `0x2F198(-1, 0xE, 0x809C8, 0xB000)`, `0x2CCD0(0xA2EB4)`,
  `0x2DA0C(0x29, its EAX)`, `0x2DA0C(0x35, 0xA0)`, `0x2DA0C(0x37, 0xA0)`, `0x2D974(0x2A)`, `0x2DA0C(0x2A,
  …)`, `0x1AE20()`, `0x2EA78(0xB4, 0xE)`, `0x2F280(-1, 0xE, 0x809F0, 0xB000)`. The port makes the middle
  seven and leaves out the message draw, the 180-tick wait and the cell release under its `PORT:` note (the
  wait would present frames inside `game_init` and move the frame-indexed oracles). A row would compare
  those three calls with none; and `0x2EA78(0xB4)` cannot run from its bytes instead (it stops `unmodeled:
  int at 0x2EB15`, the same key loop as d4d's `0x2EA74`). The seven calls the port does make agree with the
  list above by reading, not by a row.
- **`0x305FC` (`config_code_row`): both of its data paths live in the raw's frame.** With the flag byte
  `0x107453` zero, each of the eight characters is drawn from a two-byte frame buffer (`mov [esp+0x30],al`
  `0x306DA`/`0x306F5`, `lea ebx,[esp+0x30]` `0x306F9`, `call 0x2f198` `0x30704`): the `0x2F198` seam passes 16
  bytes by value, of which the raw's last 14 are its frame (the saved EBP, EDI, ESI above `[esp+0x34]`) and
  the port's are whatever follows a `u8 one[2]` on the C stack. With the flag byte non-zero and bit 1 clear,
  the record handed to `0x2DCA0` is the frame itself (`mov edx,esp` `0x306AB`) where the port uses the
  scratch `CFG_HISCORE_TMP` (`0x03900000`, outside the image, `PORT:`): the pointer argument differs and the
  scratch's non-zero bytes are writes only the port makes. What is left comparable is the flag-bit-1 arm
  (one `0x2F198` of the text at `0x10745D` in mode `0x1000`, then the return at `0x30712`): the entry, the
  draw and the bit test. Not rowed; the same class as d4d's `0x31A78`.

## §5 Store-sweep findings

Every field a row's function writes is seeded with a value that differs from what is written and sits
between seeded neighbours; each store has a dropping mutant (`@no…`) and, where the width is a choice, a
width mutant. By row:

- `flow_screen_reset`: `0x107A53..0x107A56` = `A3 5A 5B A6`; `@no55`, `@no54`, `@w55` (a word store over
  `0x107A56`), `@ah`/`@hi`/`@swap` (which byte gets what); `@late` is the memory at the first call.
- `attract_state_reset` / `game_state_init`: the mode word, the dword `0xF0A5C`, the words
  `0xF0A60..0xF0A66`, the bytes `0xF0A6E..0xF0A72`, the dword `0x104AB8` and the byte `0x104B1F`, with
  case-varied seeds; `@modew` and `@a64w` (dword stores over the next word), `@a5cb` and `@ab8b` (byte
  stores), `@off` (the neighbours `0xF0A70`/`0xF0A6E`), `@early`/`@a5clate`/`@tailearly` (the memory at a
  call).
- `frontend_wipe_in`: the twin's fixture; `@nof4`, `@f4early`, `@f4spawn` (the twin's store), `@keep`,
  `@nostore`, `@copy`/`@copysrc`, `@inc`/`@order`.
- `frontend_mode_1a_step`: `0x1088F4..F6` = `66 7F 67`, the mode word between seeded words; `@f5w` (a word
  store over `0x1088F6`), `@b00b` (a byte store: the high byte `0x44` survives), `@nof5`, `@nob00`, `@early`.
- `game_hook_25ae8`: `@nob14`, `@abcb` (a byte store into the dword `0x104ABC`), `@noabc`, `@nohook`, `@noee`,
  `@noafe`, `@nob00`, `@b00b`; the order of the `0x104B14` and `0x104ABC` stores around their calls by
  `@b14early`/`@b14late`/`@abclate`.
- `nameentry_reset`: the cell table is seeded whole from 0x14 bytes before the first cell to the cell after
  the last (`@cell0`, `@cells17`, `@cells19`, `@celloff`); `@roww` (a dword store over `0x1044D4`), `@d8w`.
  A first attempt, `@colw` (a dword store of `0xB` at `0x1044D0`), was **not detected**: the row word store
  that follows overwrites its upper half. It was replaced by `@roww`.
- `menu_title_draw`: no store of its own in `mem[]` (the title is frame-local).
- `config_field_set`: the storage `0x105DB0..0x105E6F` is seeded whole with distinct bytes per case, so a
  nibble or byte one off, a wrong half kept (`@oddkeep`), a whole byte where a nibble goes (`@w1`) and each
  flag bit (`@flag1`, `@no6`, `@six`) show; `@flagorder` is caught only by `c9` (trail `0x29`).
- `hiscore_insert`: each block is seeded whole and for one record past its end (`@movelen` writes there,
  `@movefwd` smears the overlap); the two flag bytes sit between seeded bytes (`@flagidx`, `@flagbit`).
- `hiscore_rank_single`: the rank word between seeded words; `@d6w`, `@nod6`, `@d6early`.
- `nameentry_finish`: the typed name, both copies and the record's value dword are seeded; `@no94`, `@no67`,
  `@no97`, `@letteridx`; `e4`'s limit -1 puts the letter in the value dword's top byte (`0x104393`).
- The four fight rows: the entry's `+0x1B..+0x22`, the record's `+0x28..+0x2B`, `+0x49..+0x4B`, the pool byte
  `+0x4B` of records 2, `0x80` and -`0x80`, and the counters/flags `0x1088AC..0x1088B7` are seeded;
  `@no20 @no1f @nopool @no2a @no29 @no4a @no1c @noae` drop one store each, `@pooloff`/`@aeidx` move one,
  `@ksext` sign-extends the held byte (caught by the `0x80` cases only), `@b2` is the one store the twins
  differ by.

Mutants that were not detected at first, and what was done: `nameentry_reset@colw` (above: unobservable,
replaced); `nameentry_reset@seekrec` (one stub EAX serves three spawns: moved to the `_rr` row, whose
`0x2BCF4` stub rewrites `0x1044B4`); `menu_title_draw@aearly` (no title began `0x0A 0x0B`: case `mc` added).

## §6 Queue entries not done

| entry | port function | why |
|---|---|---|
| `0x305FC` (L3, 287) | `config_code_row` (config.c) | Named gap: a two-byte frame buffer under a 16-byte by-value seam, and a record in the raw's frame against a port scratch outside the image (§4). |
| `0x2CADC` (L3, 150) | `config_set_defaults` (config.c) | Named gap: the port omits three of the raw's ten calls by design; `0x2EA78(0xB4)` stops on `int 0x16` (§4). |
| `0x249F0` (L3, 280) | `game_quit_prompt` (flow.c) | Named gap: `int 0x16` inline at `0x24A64` (§4). |

Done: the other 19 entries, 26 rows, 162 cases, 423 mutants.

## §7 Counters and gates (measured on the branch head)

- Seams added (`port/src`, the first statement of each): `0x2BAF4` `actors_reset_al` (`PR_SEAM(0x2BAF4u,
  al)`), `0x4F1E4` `frontend_input_reset` (`PR_SEAM0`), `0x4F7F4` `attract_palette_advance` (`PR_SEAM0`),
  `0x2C304` `config_credits_init` (`PR_SEAM0`), `0x4F9E4` `frontend_wipe_in` (`PR_SEAM_RET0`), `0x4246C`
  `fight_stage_marks_clear` (`PR_SEAM0`), `0x38B18` `frontend_spawn_row` (`PR_SEAM(0x38B18u, (u32)((const u8
  *)desc - mem), a2, a3)`), `0x2F940` `menu_debug_lines` (`PR_SEAM(0x2F940u, row)`), `0x2D4EC`
  `config_storage_touch` (`PR_SEAM(0x2D4ECu, kind)`), `0x2DCA0` `hiscore_insert` (`PR_SEAM_RET(0x2DCA0u, rec,
  src, table)`), `0x2DA0C` `config_field_set` (`PR_SEAM_RET(0x2DA0Cu, field, value)`), `0x204F4`
  `nameentry_arm` (`PR_SEAM(0x204F4u, rank, score)`), `0x13EF0` `name_filter` (`PR_SEAM_RET(0x13EF0u, str)`),
  `0x17D30` `camera_point_hit` (`PR_SEAM_RET(0x17D30u, (u32)x, (u32)y, tall)`; d5b owns its row), `0x4B788`
  `fight_4b788` and `0x4D898` `fight_4d898` (`PR_SEAM_RET(…, hit, entry, index)`), `0x4B470` `fight_4b470`
  (`PR_SEAM(0x4B470u, entry, index)`), `0x4C784` `fight_4c784` (`PR_SEAM(0x4C784u, side)`). 18 seams. The 14
  existing rows that name one of these addresses, or one of the three exported functions, as their entry or a
  callee were re-run after the last seam (all VERIFIED, every mutant detected): `frontend_spawn_row
  config_credits_init fight_stage_marks_clear fight_4b788 fight_4d898 name_filter menu_debug_lines
  game_mode_12_step_s5 nameentry_arm frontend_input_reset fight_4b470 fight_4c784 attract_palette_advance
  actors_reset_al`.
- `tools/diff_emu.py`, two additions: `FLAT_SEGMENT_CALLEES |= {0x653A1}` (the runtime memmove loads ES
  from DS on both copy arms, `0x653BB..0x653BE` and `0x653D5..0x653DA`; without the flat descriptors
  `hiscore_insert`'s moving cases stopped `unmodeled: int3 at 0x10000`; no earlier row names `0x653A1`) and
  `RESOLVED_JUMPS[0x4C687] = (0x4C67B, 0x4C5F0, 7)` (`0x4C60C`'s character table: `cmp al,6; ja; xor eax,eax;
  mov al,[esp]; shl eax,2; jmp cs:[eax+0x4c5f0]`, the byte saved at `0x4C678`; targets `0x4C68E 0x4C6B0
  0x4C69E 0x4C68E 0x4C6B0 0x4C68E 0x4C6B0`; `callee_clobbers(0x4C60C)` is `(edx)` with and without it).
- Insertions in `tools/tests/test_diff_verify.py` outside the batch's pin block: two lines at the top of
  the expected-clobber dictionary of `test_each_stub_declares_the_registers_its_callee_clobbers` (a `# D5
  d5d` comment and the 18 entries) and one line at the end of the `WINDOWS` dictionary (`0x4C687`'s window).
  No existing line of the three harness files is modified or deleted.
- Each row and its mutants were run ten times in a row with `--function … --self-check` (identical every
  time).
- The one full self-check, `PR_DIFF_JOBS=4 python3 tools/diff_verify.py --image /tmp/pr_d5d_img.bin
  --self-check`, on the tree of the last row commit (`894914d7`): `diff-verify: 810/810 functions VERIFIED;
  9643/9643 mutants detected; 1 named gaps; 508/643 rows with callees closed (167 have none)`, `rc 0`. The
  branch point's own counter was not measured by this batch; the batch adds 26 rows (all with callees) and
  423 mutants.
- Gates before each commit: `cmake --build build`, `./build/prageport --game-dir data/game/C --check 820`
  (with each correction in the tree), `PR_ORACLE_REQUIRED=1 ./build/run_tests` (`all checks passed`), `make entry-triage`
  (unchanged: 233 unported, 262 ported) and `python3 tools/port_progress.py` (`771 1203 64` / `731 731 100`).
- Not run by this batch: `make verify`, `make diff-verify`, any `*-oracle` target, the Python unittest.
