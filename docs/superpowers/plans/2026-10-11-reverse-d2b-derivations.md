# Track D, wave 2, batch d2b — derivations (game/fight.c)

Differential rows for the functions of `.superpowers/sdd/2026-10-11-reverse-d1/d2b-queue.txt`, worked top
to bottom. Ground truth: the fixed-up image `build/diffrun --image-out` writes (linear addresses),
disassembled with capstone. Every figure below was measured with
`python3 tools/diff_verify.py --image <image> --function <name> --self-check`, one row at a time; each
mutant's port side was run ten times and gave the same output each time. The full `--self-check`, the
unittest suite, `make verify`, `make diff-verify` and the oracles were NOT run on this branch (the
integrator runs them on the merged tree), so no branch counter line is recorded here.

## §1 Members and the callers that fix each EAX mask

Call sites are every `E8`/`E9 rel32` in 0x10000..0x5D000 that targets the entry, plus a dword scan of
the whole image for a stored pointer to it (none found for any member below).

| addr | port function | size | call sites | what the callers read of EAX | mask |
|---|---|---|---|---|---|
| 0x1A978 | `fight_stance_pass` | 408 | 0x34C75 | 0x34C7A..0x34C7F pops and returns (in 0x34B6C); 0x34B6C's one call site 0x357FC is followed by 0x35801 `mov eax,esi` | 0 |
| 0x1D2F0 | `fight_hud_bar_set` | 371 | 0x1D5A8, 0x1D797, 0x1D941, 0x1DD2C | 0x1D5AD `mov al,[ecx+0x10780d]`, 0x1D5B3 `mov ah,[ebx+0x10290e]`, then 0x1D5C1 / 0x1D5D7 `xor eax,eax` or (equal) 0x1D5EC `mov eax,[esp+4]`; 0x1D7A1 `mov eax,[ebx*4+0x1028f8]`; 0x1D948 `xor eax,eax`; 0x1DD33 `xor eax,eax` | 0 |
| 0x1D464 | `fight_hud_bar2_set` | 106 | 0x1D5E1, 0x1D94A, 0x1DD35 | 0x1D5EC `mov eax,[esp+4]`; 0x1D953 `xor eax,eax` / 0x1D957 `mov eax,0x4000`; 0x1DD3E / 0x1DD42 the same | 0 |
| 0x1DA84 | `fight_hud_bar_set_inv` | 100 | 0x1DB70, 0x1DC5C | 0x1DB7E `xor eax,eax` / 0x1DB82 `mov eax,0x4000`. 0x1DC5C returns from 0x1DC0C (0x1DC61..0x1DC6B), whose address is the 0xA86C4 table's dword at 0xA86D0; the table's one reference is 0x25623 `call [eax+0xa86c4]`, and from its return site 0x25629 EAX is reloaded by 0x25621 `mov eax,ebx` (the loop) or, out of the loop, by 0x2563E `mov eax,[0x10150c]` after 0x25639 `call 0x134c0` (the walk of record d1f §1, the 0x4DBB4 row) | 0 |
| 0x20EF8 | `fight_round_reset` | 167 | 0x25C70, 0x26AAF | its EAX is 0x46670's with AH cleared (0x20F91 `xor ah,ah`), the value record d1f §1 walked for the 0x46670 row: 0x25C75 `pop edx; ret`, 0x26AB4 `mov ah,4`, dead on every path | 0 |
| 0x412A0 | `fight_scene_props` | 77 | 0x20E86 | 0x20E8B..0x20E8E pops and returns (in 0x20DF4); 0x20DF4's six call sites load EAX next: 0x11ACB `xor eax,eax`, 0x25A9A `mov eax,ebx`, 0x25BEB `xor eax,eax`, 0x269CE `xor eax,eax` / 0x269DA `mov eax,1`, 0x270E5 and 0x295E9 `mov eax,[0x10810a]` | 0 |
| 0x41350 | `fight_char_select` | 120 | 0x11ACD, 0x11AFE, 0x41794, 0x418C4, 0x423A7, 0x43161 | 0x11AD2 `xor eax,eax`; 0x11B03 `mov eax,1`; 0x423B1 `mov eax,0x17`; 0x4316D `mov eax,0xc8364`; 0x41794 returns through 0x41760 (0x417BE..0x417C1) and its caller 0x4213B: walk, dead on every path; 0x418C4 lies in the body 0x41878, which record E2 names dead code (no reference anywhere) | 0 |
| 0x433DC | `fight_char_opp_text_clear` | 133 | 0x43670 | returns through 0x435A8 into 0x43B24: walk, dead on every path (0x43C32 / 0x43C49 / 0x43C75 `mov eax,[edx+0x108163]`, 0x43C9F `mov eax,edx`, 0x43CB8 `and eax,0xffff` after the low word is reloaded) | 0 |
| 0x43A08 | `fight_char_select_actor` | 162 | 0x4379D, 0x43B60, 0x44521 | 0x4379D and 0x43B60: walk, dead on every path. 0x44521 loops (0x4450F `mov eax,edx`) or returns through 0x444C8 into 0x4367C, a function no instruction calls: it is the DS_00104AE4 frame hook (0x25806 `mov edx,0x4367c`, 0x25810 `mov [0x104ae4],edx`); from the return site of each of the six `call [0x104ae4]` (0x4F302, 0x4F373, 0x4F6F1, 0x4F70D, 0x4F9AA, 0x4F9D1) the walk is dead on every path | 0 |
| 0x43FBC | `fight_char_fighter` | 150 | 0x43CA8 | walk, dead on every path | 0 |
| 0x442A0 | `fight_char_team_fighter` | 170 | 0x448EF | walk, dead on every path | 0 |
| 0x493F0 | `fight_midpoint` | 50 | 0x49CAA, 0x4C3C9 | both store all of EAX: 0x49CB7 and 0x4C3DD `mov [0x108874],eax` | 0xFFFFFFFF |
| 0x496DC | `fight_496dc` | 416 | 0x4A32B | walk, dead on every path | 0 |
| 0x4A708 | `fight_effects_hold_all` | 122 | 0x28594, 0x288C2 | walk, dead on every path | 0 |
| 0x4A868 | `fight_4a868` | 63 | 0x4A361, 0x4BFDE, 0x4DF8E, 0x4DFFA, 0x4E066, 0x4E0D0 | each `test eax,eax` (0x4A366, 0x4BFE3, 0x4DF93, 0x4DFFF, 0x4E06B, 0x4E0D5) | 0xFFFFFFFF |
| 0x4A8A8 | `fight_4a8a8` | 96 | 0x4E871 | 0x4E876 `test eax,eax` | 0xFFFFFFFF |
| 0x4AA6C | `fight_4aa6c` | 98 | 0x4A97A, 0x4A9C2 | 0x4A97F `mov [0x10885c],eax` / 0x4A984 `test eax,eax`; 0x4A9C7 `mov [0x108858],eax` / 0x4A9CC `test eax,eax` | 0xFFFFFFFF |
| 0x4B144 | `fight_4b144` | 359 | 0x49C05, 0x4AC0B | walk, dead on every path | 0 |
| 0x4B2AC | `fight_4b2ac` | 322 | 0x4A163 | walk, dead on every path | 0 |
| 0x4B5A8 | `fight_4b5a8` | 241 | 0x4AB45 | 0x4AB4A `test al,al`; walk on AH and the high word: dead | 0xFF |
| 0x4B788 | `fight_4b788` | 547 | 0x4B705 | 0x4B70A `test eax,eax` | 0xFFFFFFFF |
| 0x4BD98 | `fight_4bd98` | 346 | 0x4B590 | walk, dead on every path | 0 |
| 0x4B9AC | `fight_challenge_crowd` | 925 | 0x42574 | walk, dead on every path | 0 |
| 0x4CC0C | `fight_4cc0c` | 393 | 0x4C356, 0x4C429, 0x4C90D | walk, dead on every path | 0 |
| 0x4CD98 | `fight_4cd98` | 187 | 0x4C3BA | walk, dead on every path | 0 |
| 0x4CF20 | `fight_4cf20` | 487 | 0x494C6 | walk, dead on every path | 0 |
| 0x4D150 | `fight_4d150` | 209 | 0x4D254 | 0x4D259 `test eax,eax` | 0xFFFFFFFF |
| 0x4D898 | `fight_4d898` | 796 | 0x4D80B | 0x4D810 `test eax,eax` | 0xFFFFFFFF |
| 0x4DBEC | `fight_4dbec` | 755 | 0x2766D, 0x29838 | walk, dead on every path | 0 |
| 0x4E350 | `fight_mode25_spawn` | 481 | 0x4E271, 0x4E987 | walk, dead on every path (0x4E987: 0x4E98C `mov [0x1088b8],ch`, the pops, `ret` from 0x4E67C, then its callers) | 0 |

"walk" is the rule record d1f §1 states: from the return site the code is followed (both arms of every
conditional, into direct callees to depth 4, out through `ret` into the callers of the containing
function to depth 7) tracking which parts of EAX (AL, AH, the high word) still hold the function's
value, until every part is overwritten; any read of a live part (a copy included) would set the mask.
It sees direct calls only: a function reached through a pointer is reported and followed by hand, as
0x1DC0C and 0x4367C above.

How the walk finds call sites matters, and the first version got it wrong: it took every `E8 rel32`
byte pattern in the code object as a call, and the `E8` at 0x4E6DF is the ModRM byte of 0x4E6DE `sub
eax,0xe0` (`81 E8 E0 00 00 00`), which made a false function 0x4E7C4 and a false caller; on 0x4E350's
site 0x4E987 it reported a read of AX at 0x4E6E8 that no path reaches. The walk now takes call sites
from a recursive-descent scan of every known entry (the Ghidra export, the E2 live list and triage
entries, closed over call targets), adds a byte-pattern site only where it lies outside every scanned
instruction (switch-case code the descent does not reach), and takes "the containing function" from
the Ghidra bodies. Every mask of this record was re-walked with it: all unchanged, including the six
`call [0x104ae4]` return sites and the 0xA86C4 table return site 0x25629.

## §2 The measured rows

`blocks` is hit/total; `mutants` is detected/total.

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fight_stance_pass` | 0x1A978 | 16 | 25/25 | 24/24 | 0 | 0x33A10 runs on both sides (allowed); 0x3B134 stub (it rewrites both command words), 0x1A6AC and 0x1A8F4 stubs; 0x1A640 runs on both sides, recorded |
| `fight_hud_bar_set` | 0x1D2F0 | 11 | 20/20 | 11/11 | 0 | 0x10D70 stub (`ret 4`) |
| `fight_hud_bar2_set` | 0x1D464 | 9 | 8/8 | 8/8 | 0 | 0x10D70 stub |
| `fight_hud_bar_set_inv` | 0x1DA84 | 10 | 8/8 | 8/8 | 0 | 0x10D70 stub |
| `fight_round_reset` | 0x20EF8 | 3 | 3/3 | 18/18 | 0 | 0x38BEC and 0x46670 stubs, EAX varied per case |
| `fight_scene_props` | 0x412A0 | 6 | 4/4 | 10/10 | 0 | 0x2AE14 and 0x2C320 stubs; the tail call 0x412DD `call [edi*4+0xc7f58]` reaches 0x412EC (the function's own `ret`) or 0x5D812 (`xor eax,eax; ret`), both allowed; the port issues neither |
| `fight_char_select` | 0x41350 | 8 | 6/6 | 13/13 | 0 | 0x33C18 stub (it preserves EDX, which the raw reads after it) |
| `fight_char_opp_text_clear` | 0x433DC | 6 | 5/5 | 15/15 | 0 | 0x2B150, 0x2C088 and 0x2F280 stubs; the string path 0x1C500 / 0x474E4 / 0x1E75C / 0x1E808 / 0x500BB runs on both sides (allowed), the decoded buffer 0x102760 in the memory at each release call |
| `fight_char_select_actor` | 0x43A08 | 5 | 4/4 | 16/16 | 0 | 0x2AE14 (EAX = the new record, varied per case), 0x1D7B8 and 0x2BCF4 stubs |
| `fight_char_fighter` | 0x43FBC | 5 | 1/1 | 16/16 | 0 | 0x2BC30, 0x2A17C and 0x1D7B8 stubs |
| `fight_char_team_fighter` | 0x442A0 | 7 | 3/3 | 17/17 | 0 | 0x2BC30, 0x2A17C and 0x1D7B8 stubs |
| `fight_midpoint` | 0x493F0 | 8 | 3/3 | 7/7 | 0xFFFFFFFF | 0x2BE00 runs on both sides, recorded (each record its own value) |
| `fight_4a868` | 0x4A868 | 11 | 6/6 | 8/8 | 0xFFFFFFFF | 0x2BE00 runs on both sides, recorded |
| `fight_4a8a8` | 0x4A8A8 | 10 | 12/12 | 7/7 | 0xFFFFFFFF | 0x2BE00 runs on both sides, recorded |
| `fight_4aa6c` | 0x4AA6C | 8 | 8/8 | 8/8 | 0xFFFFFFFF | 0x2BE00 runs on both sides, recorded: the call list is the matching entries, in order |
| `fight_effects_hold_all` | 0x4A708 | 5 | 7/7 | 15/15 | 0 | 0x4B3F0 and 0x4B430 stubs, each rewriting the entry's next pointer to the list head (a harness write: the raw read the next entry before the call, 0x4A71D) |
| `fight_496dc` | 0x496DC | 8 | 12/12 | 33/33 | 0 | 0x49388, 0x29CDC, 0x5D7DC, 0x2AE14 and 0x2BC30 stubs; 0x249D0, 0x249B0 and 0x496AC run on both sides (allowed) |
| `fight_4b5a8` | 0x4B5A8 | 8 | 10/10 | 21/21 | 0xFF | 0x2BC30, 0x2BE00 and 0x2BE4C stubs |
| `fight_4b144` | 0x4B144 | 16 | 17/17 | 20/20 | 0 | 0x2BE00, 0x5D7DC, 0x2BE4C and 0x2BC30 stubs; one rng value per case for its one to three draws |
| `fight_4b2ac` | 0x4B2AC | 12 | 23/23 | 25/25 | 0 | 0x5D7DC, 0x2BE4C and 0x2BC30 stubs |
| `fight_4b788` | 0x4B788 | 13 | 21/21 | 34/34 | 0xFFFFFFFF | 0x2B150, 0x2BC30, 0x2BD20 and 0x2C3FC stubs; 0x496AC runs on both sides (allowed) |
| `fight_4bd98` | 0x4BD98 | 10 | 8/8 | 33/33 | 0 | 0x13134, 0x2AE14 and 0x2BC30 stubs; the 0x2BC30 stub rewrites DS_00108868 to a second record (a harness write), so the two +0x36 stores land on different records |
| `fight_challenge_crowd` | 0x4B9AC | 10 | 48/49 | 38/38 | 0 | 0x5D7DC runs on both sides, recorded (seeded draws); 0x29CDC, 0x2AE14 and 0x2BC30 stubs; 0x249D0, 0x249B0 and 0x496AC run on both sides (allowed); one block named unhit (0x4BB7E) |
| `fight_4cc0c` | 0x4CC0C | 5 | 7/7 | 16/16 | 0 | 0x2F510 and 0x2F4BC stubs; the string path runs on both sides (allowed), the decoded buffer in the memory at each text call |
| `fight_4cd98` | 0x4CD98 | 4 | 4/4 | 18/18 | 0 | 0x2F510 and 0x2B150 stubs; the string path runs on both sides (allowed) |
| `fight_4cf20` | 0x4CF20 | 8 | 17/18 | 32/32 | 0 | 0x49388, 0x29CDC, 0x5D7DC and 0x2AE14 stubs; 0x249D0, 0x249B0 and 0x496AC run on both sides (allowed); one block named unhit (0x4CF97) |
| `fight_4d150` | 0x4D150 | 10 | 9/9 | 18/18 | 0xFFFFFFFF | 0x5D7DC runs on both sides, recorded (seeded draws); 0x2BE00, 0x2BE4C and 0x2BC30 stubs |
| `fight_4d898` | 0x4D898 | 27 | 41/41 | 50/50 | 0xFFFFFFFF | 0x2B150, 0x2BC30, 0x2BD20 and 0x2C3FC stubs; 0x496AC runs on both sides (allowed); the six targets of the jump table 0x4D880 each hit |
| `fight_mode25_spawn` | 0x4E350 | 6 | 37/37 | 39/39 | 0 | 0x4E27C, 0x5D7DC, 0x29CDC, 0x2AE14 and 0x2BC30 stubs; 0x249D0 and 0x249B0 run on both sides (allowed); the two jump tables 0x4E300 / 0x4E328 resolved by hand (E.RESOLVED_JUMPS), all ten entries of each walked |
| `fight_4dbec` | 0x4DBEC | 4 | 28/28 | 39/39 | 0 | 0x5D7DC runs on both sides, recorded (seeded draws); 0x2BE00, 0x49388, 0x29CDC, 0x2BE4C, 0x2AE14 and 0x2BC30 stubs; 0x249D0, 0x249B0 and 0x496AC run on both sides (allowed) |

30 rows, 617 mutants.

The `D2B_MASKS` / `D2B_KINDS` / `D2B_CASES` pins of `tools/tests/test_diff_verify.py` are generated from
these measurements. Every stub the batch declares carries the clobbers `E.callee_clobbers` derives from
the bytes (checked per row). Stubs the pinned dictionary of
`test_each_stub_declares_the_registers_its_callee_clobbers` did not have are added to it as new lines
(no existing line changed): 0x10D70 (ebx, ecx, edx), 0x1A8F4 (edx), 0x38BEC (), 0x46670 (), 0x2C320 (), 0x33C18 (),
0x2C088 (ebx, edx, edi, ebp), 0x1D7B8 (ebx, edx), 0x4B3F0 and 0x4B430 (ebx, edx), 0x49388 (),
0x29CDC (edx), 0x2BE00 (), 0x2BE4C (), 0x2BD20 (edx), 0x13134 (), 0x2F510 (ebx, ecx, edx), 0x4E27C ().

Each mutant's port side was run ten times with identical output before its pin was written
(`fight_mode25_spawn`: twice at the commit that adds it, then the ten-run pass, whose pins equal the
committed ones; `fight_4dbec`: the same, twice then ten times, the pins equal). On the final tree
every row was measured once more, all 30 together: 30 VERIFIED, 617 mutants detected, each with the
kinds and the case set the test file pins. That
rule earned its keep once: `fight_mode25_spawn@kept` and `@ne8` first followed a seeded node byte as an
actor pointer and crashed `diffrun` (signal 11); both were rewritten to stay inside mem[] (the mutant
keeps the spawned record in a local) before any pin was taken.

## §3 Corrections (port/src)

None so far.

The `port/src` edits of the batch that are not corrections:

- seams (first statement of a callee that had none): `0x1A640` `fighter_1a640` (`PR_SEAM_RET`, side),
  `0x1A8F4` `fighter_block_end` (`PR_SEAM`, rec), `0x10D70` `actor_pset_word_set` (`PR_SEAM`, rec, word),
  `0x38BEC` `fight_side_scratch_clear` (`PR_SEAM`, side), `0x46670` `fight_1081f0_clear` (`PR_SEAM0`),
  `0x2C320` `fight_scene_crowd` (`PR_SEAM`, scene), `0x33C18` `fight_char_reset` (`PR_SEAM`, side),
  `0x2C088` `prompt_press_start_clear` (`PR_SEAM`, col, row, side), `0x1D7B8`
  `fight_select_marker_spawn` (`PR_SEAM`, side, cls, y), `0x2BE00` `fight_2be00` (`PR_SEAM_RET`, rec),
  `0x2BE4C` `fight_2be4c` (`PR_SEAM_RET`, rec, value), `0x4B3F0` `fight_4b3f0` and `0x4B430`
  `fight_4b430` (`PR_SEAM_RET`, entry, index, flag), `0x49388` `fight_dust_pick` (`PR_SEAM_RET`, side),
  `0x29CDC` `fight_dust_value` (`PR_SEAM_RET`, side, ch), `0x2BD20` `actors_link_held` (`PR_SEAM_RET`,
  rec, v), `0x13134` `fight_13134` (`PR_SEAM_RET0`), `0x2F510` `text_cursor_hold_font2` (`PR_SEAM`, col,
  row, the string's mem[] offset, mode), `0x4E27C` `fight_mode25_face` (`PR_SEAM0`).
  Each callee's own row was re-run after its seam and is unchanged (VERIFIED, every mutant detected).
- exports for a binding (each loses `static` and is declared in `fight.h`): `fight_hud_bar_set_inv`
  (0x1DA84), `fight_midpoint` (0x493F0), `fight_4aa6c` (0x4AA6C), `fight_4a8a8` (0x4A8A8),
  `fight_4b5a8` (0x4B5A8), `fight_4b144` (0x4B144), `fight_4b2ac` (0x4B2AC), `fight_4b788` (0x4B788),
  `fight_4bd98` (0x4BD98), `fight_4cd98` (0x4CD98), `fight_4d150` (0x4D150).
- `tools/diff_emu.py`: two `RESOLVED_JUMPS` entries for 0x4E350's jump tables, 0x4E426 (guard 0x4E409,
  table 0x4E300, 10 entries) and 0x4E4B9 (guard 0x4E4B2, table 0x4E328, 10 entries), with their
  `WINDOWS` lines in `tools/tests/test_diff_verify.py` (new lines inside the dictionary). Both index by
  EDI, which the guard does not compare: EDI is 4 * the counter [esp+8] throughout the loop (0x4E36A
  `xor edi,edi` with 0x4E37D `mov [esp+8],ebx` = 0; 0x4E587 `inc edx` / 0x4E588 `add edi,4` / 0x4E58B
  `mov [esp+8],edx`; no other write of EDI in the function, and every callee preserves it), and the
  guards bound the counter at 9 (0x4E409 `cmp eax,9` with EAX = [esp+8], 0x4E4B2 `cmp dword [esp+8],9`).
  `E.callee_clobbers` is unchanged by them for 0x4E350, its callers 0x4E11C and 0x4E67C, and 0x4E27C.

A note on record d1g §1: it says the 0x10D70 call at 0x1DAE0 "returns through 0x1DA84: ... 0x1DC5C
(returns through 0x1DAE8 to 0x26AA1)". 0x1DC5C is not in 0x1DAE8 (which ends at 0x1DC09 `ret`): it is in
0x1DC0C, a function reached only through the 0xA86C4 table (§1 above). The mask 0 of that row stands on
the table call site's walk instead.

## §4 Named gaps and unhit blocks

Two rows name one unhit block each (`unhit_named`), both unreachable from the bytes:
- `fight_challenge_crowd` 0x4BB7E `jmp 0x4bcd1`: the lane word [esp+0x2c] is 0 at each side's start
  (0x4B9D1/0x4B9D7) and returns to 0 whenever its increment passes 2 (0x4BCEF..0x4BD04), so CX at
  0x4BB64 is 0, 1 or 2 and the fall-through after `cmp cx,2; je` cannot execute.
- `fight_4cf20` 0x4CF97 `mov eax,0x300`: 0x4CF45 stores 6 in the slot's +0x81 and nothing writes it
  before 0x4CF7C reads it back, so BL is 6 at 0x4CF82 `test bl,bl`.

**Named gap: 0x432EC `fight_char_text_clear` (no row).** Every path of 0x432EC calls the runtime strlen
0x65624 (0x43325, and again at 0x433A6), and the harness can run neither side of that call:
- the original's 0x65624 loads a segment register (0x65628 `push es`, 0x65629 `mov eax,ds`, 0x6562B
  `mov es,eax`, 0x65632 `repne scasb`, 0x65637 `pop es`). Measured with `diff_emu.run_original(image,
  0x65624, eax = a string)`: EAX holds the right length at the `scasb`, then the run ends `unmodeled:
  int3 at 0x10000` with accesses at 0xFFF0..0xFFFC outside the image (the stack has dropped to 16 bits
  after the segment load), for "", "A" and "HELLO" alike. So 0x65624 cannot be an allowed callee;
- as a stub it would need the port's call to be recorded, and the port calls the host `strlen`
  directly (fight.c, the two `strlen` of `fight_char_text_clear`): there is no seam to add to an
  existing callee. Record C3f §C3f.2 solved the same problem for sprintf/memset with the seamed host
  wrappers `host_sprintf`/`host_memset`; a `host_strlen` of that form would make 0x432EC rowable (and
  fight.c's other strlen sites, the code at 0x434C8..0x4355A and 0x4469F..0x4473E). That is a new
  port function, outside this batch's remit, so the gap is named here and no `gap=` spec is added.
Its twin without the strlen, 0x433DC, has its row.

Limits worth knowing:
- `fight_stance_pass`: 0x1A640 runs its own bytes, so the row sees its three results (0, 0x1000, 0x2000)
  and no other EAX. The port reads the side's command word before the 0x1A640 call and the raw after
  it (0x1AAA2); 0x1A640 writes nothing, so the two orders cannot differ.
- the three bar rows compare 0x10D70's word argument as all of EAX; every caller zero-extends it.
- `fight_scene_props`: the port does not issue the tail call 0x412DD. The row proves that harmless for
  the two targets the image's table 0xC7F58 holds (0x412EC for scenes 0, 1, 2, 5; 0x5D812 for 3, 4, 6,
  7; both exercised) and for nothing else.
- `fight_char_select_actor`: the raw stores the spawn's EAX and writes its +0x4D without a test for 0
  (0x43A65..0x43A7D); every case's stub returns a scratch record, a 0 is not exercised.
- `fight_char_team_fighter@null` runs the body with a fighter pointer of 0 and so writes mem[0x18],
  mem[0x1C] and mem[0x2C]: below the image, inside mem[], reported and cleared by the runner.
- the string table of the text rows is a fixture (ids 0x235 and 0x236 in group 8), not ENGLISH.TXT.
- the four 0x2BE00 readers run 0x2BE00's own bytes; the port's signed C arithmetic agrees with the raw's
  wrapping `neg`/`sar` on the extreme inputs the rows include (a difference of 0x80000000 in
  `fight_midpoint` d6, a wrapping subtraction in d7), on this compiler.
- `fight_4aa6c`: the count is tested and divided as 16 bits (0x4AAAD `test cx,cx`, 0x4AAB8 `mov si,cx`); a
  list of 65536 matching entries is not exercised. The raw reads the next link before the 0x2BE00
  call (0x4AA92); 0x2BE00 writes nothing, so the order cannot show.
- `fight_496dc`: the spawn stub returns one actor per case, so in the two-iteration cases both
  iterations write the same actor (the memory at each call separates them). The +0x34 word the raw
  tests after the 0x2BC30 call (0x4980B) is the one it copied before it; a stub does not change it,
  so "read after the call" is not separated from "read before". fight.c's `effects_list_unlink` /
  `effects_list_insert_after` carry no seam (the seamed 0x249D0/0x249B0 are actors.c's copies), so
  the splice runs on both sides and shows in the memory, not in the call list.
- `fight_4b5a8`: the entry's +0x1A is the actor's +0x32 read after the 0x2BC30 call (0x4B5EE); the stub
  does not change it, so the read's position is not separated.
- `fight_4b144`, `fight_4b2ac`: the rng stub has one EAX per case, used for rng(2) and for every
  rng(0x1200) / rng(0xC00) of that case; the ranges are in the call list.
- `fight_4b2ac`: a divisor DS_000C9520 of 0 (a divide fault in the raw) is not exercised.
- `fight_4b788`: `hit` is 1 or 2 in every case (0x4B7C0 `xor si,1` on a 16-bit register is not
  separated from a 32-bit one). The index stays below 7.
- `fight_4bd98`: the spawn stub returns one record for both spawns, so which spawn's record 0x2BC30
  takes (0x4BED0, EAX = the second) is not separated; the two midpoint arms are equal unless the
  difference wraps (cases e8, e9).
- `fight_challenge_crowd`, `fight_4d150`: 0x5D7DC runs its own bytes from a seed, so rng(2) is 0 or 1 and
  rng(0x3C) is below 0x3C: a `test al,al` is not separated from the raw's `test eax,eax` (0x4BC4E,
  0x4D161, 0x4D175). The seeds of `fight_challenge_crowd` c6..c9 were searched for a second draw
  rng(0x1D00) of 0x9A9, 0x9AA, 0x1353 and 0x1354 (each side of both lane bounds); `fight_4d150`'s for
  each (rng(2), rng(4)) pair after an rng(0x3C) of 0. In `fight_challenge_crowd` the spawn stub returns
  one actor for every entry of a case.
- `fight_4cc0c`, `fight_4cd98`: the string table is a fixture; which string each text call draws is
  carried by the decoded buffer 0x102760 in the memory at that call.
- `fight_4cf20`: the function stores 6 in +0x81 itself, so six entries (or the pool's end) is the only
  count; the rng stub's one value serves both draws of every iteration.
- `fight_4d898`: the cap `sum > 0x78` and `sum >= 0x78` give the same byte at a sum of exactly 0x78
  (0x4DB53 / 0x4DB90 `jle`), so no mutant separates them. `hit` is 1 or 2; the index stays below 7.
- `fight_mode25_spawn`: the spawn stub returns one actor for all ten entries of a case; a kept entry's
  actor is another record. A fresh call with a table entry already set replaces it (case f2).
- `fight_4dbec`: 0x5D7DC runs its own bytes from a seed (three seeds), so the draws are not chosen one
  by one: an exactly-zero left target (0x4DD35 `jge`) and a target of exactly 0x5400 (0x4DD79 `jle`)
  are not exercised, and rng(2)'s `test eax,eax` (0x4DCF9, 0x4DD9B) is not separated from an AL test.
  The spawn stub returns one actor for every entry of a case. One case runs the full 0x1C entries.

## §5 Store sweep

| row | stores | pre-state / neighbours | store mutants caught |
|---|---|---|---|
| `fight_stance_pass` | slot +0x61 (dec, then 0), +0x54, +0x60 (dec, then 0x28), +0x62, +0x53, the other slot's +0x8A | both slots' +0x52..+0x55, +0x5C..+0x65 and +0x82..+0x8B hold different non-zero bytes per side | `@c61`, `@s54`, `@c60`, `@c62`, `@c53`, `@w8a` (each dropped), `@order` (the +0x60 decrement moved after the 0x1A640 call: call memory alone) |
| `fight_hud_bar_set`, `fight_hud_bar2_set`, `fight_hud_bar_set_inv` | none (the word goes to 0x10D70) | the record pointers and the mode byte sit between seeded neighbours | — |
| `fight_round_reset` | per side: slot +0x74, +0x76 (twice), +0x78, +0x84, +0x8C words; bytes 0x1078F2[s], 0x100B5E[s], 0xFD158..0xFD15B + 4s; dword 0xFD148 + 4s; word 0x100B50[s] = 0xFFFF; byte 0x100C1D | every stored byte starts non-zero (0x100B50's words below 0xFFFF) inside non-zero neighbours | `@w74`, `@w76`, `@w78`, `@w84`, `@w8c`, `@b8f2`, `@b5e`, `@fd158`, `@fd148`, `@w50`, `@c1d` (each dropped), `@width` (a dword at +0x8C), `@ffff`, `@stride` |
| `fight_char_select` | 0x10816A[side] (unless 0x104B1D == 1), slot +0x63 = 1, 0x10816E[side] = 0xFF, 0x105B34[side] = 0 then 1, 0x10810D | each inside seeded neighbours; the other side's bytes differ | `@s63`, `@b6e`, `@b34` (dropped), `@d0d`, `@tab`, `@idx` (wrong value) |
| `fight_char_opp_text_clear` | none of its own (DS_00108144[side] is NOT zeroed) | the slot pair sits between seeded dwords | `@zero` (a store the raw does not make) |
| `fight_char_select_actor` | DS_0010813C[side] = the spawn, its +0x4D = 0x1E, the marker's +0x29 \|= 8 | +0x4C/+0x4E and +0x28/+0x2A seeded; +0x29 starts with bit 3 clear in four cases | `@store`, `@b4d`, `@or8` (dropped), `@order` (the OR before the 0x1D7B8 call), `@rec14c` |
| `fight_char_fighter`, `fight_char_team_fighter` | the fighter's +0x18 and +0x1C dwords, +0x2C word | +0x18/+0x1C start 0xFFFFFFFF/0xEEEEEEEE (a 16-bit store shows), +0x2A/+0x2E seeded | `@w18`, `@w1c` (16-bit), `@w2c` (dropped), `@order` (the +0x2C store before the calls), `@x`, `@y`, `@tab2c` |
| `fight_effects_hold_all` | each non-type-6 entry's actor +0x38, +0x34, +0x36 words = 0 | the three words start non-zero, +0x32 and +0x3A seeded | `@w38`, `@w34`, `@w36` (dropped), `@width` (a dword at +0x38), `@order` (after the call) |
| `fight_496dc` | per node: the list links, desc +0x10, node +8, +0x1F, +0x0C, +0x21, +0x1E, +0x14; actor +0x14, +0x2C, +0x28, +0x34, +0x2E (+= 4), +0x4E | the free nodes' +8..+0x23 and the actor's fields hold slot-distinct bytes | `@desc10`, `@ne8`, `@a14`, `@f1f`, `@fc`, `@fcrec`, `@f21`, `@clamp`, `@w28`, `@w34`, `@type`, `@n14`, `@w2e`, `@b4e`, `@lorder`, `@order` |
| `fight_4b5a8` | entry +0x1A, +0x14, +0x1E; actor +0x38, +0x34; the counter DS_001088B6[ch] (capped, then decremented) | entry +0x18/+0x1C, actor +0x32/+0x3A, the counter pair's neighbours | `@w1a`, `@y`, `@w38`, `@e14`, `@type`, `@cap`, `@dec`, `@signed`, `@order` |
| `fight_4b144`, `fight_4b2ac` | entry +0x14 (and +0x1E = 1 in 0x4B144); actor +0x34, +0x29; DS_00108878 (twice) in 0x4B2AC | as above; DS_0010886C..0x108883 seeded | `@e14`, `@type`, `@dir`, `@b29`, `@b29pre`, `@stop`, `@w78`, `@order` |
| `fight_4b788` | entry +0x1C \|= 0x40, +0x10 = 0, +0x20, +0x1E; actor +0x29, +0x18, +0x1C, +0x32, +0x2C, +0x38/+0x36/+0x34, +0x4A | every field inside seeded neighbours, both slots and records slot-distinct | `@set40`, `@z10`, `@flip`, `@ef`, `@e20`, `@mirror`, `@y1c`, `@w32`, `@clamp`, `@stop`, `@type`, `@b4a`, `@order` |
| `fight_4bd98` | DS_00108864, entry +0x1E, +0x1C; 0x1088C1, 0x104B00, 0x1088C5, 0x1088AA, 0x1088A0, 0x108898, 0x10889D, 0x10889C, 0x104AEC, 0x1088AC, 0x108884 (twice), 0x108868, 0x10886C, both records' +0x36 | every stored byte starts non-zero inside seeded neighbours | `@w864`, `@type`, `@c1`, `@b00`, `@c5`, `@waa`, `@wa0`, `@w98`, `@b9d`, `@b9c`, `@aec`, `@wac`, `@bit5`, `@sub854`, `@w868`, `@late`, `@w86c`, `@r36a`, `@r36b`, `@v36` |
| `fight_challenge_crowd` | per entry: the list links, desc +0x10, entry +8, +0x1E, +0x1F, +0x0C, +0x21; actor +0x2C, +0x14, +0x29 (\|= 0x40, twice), +0x55, +0x2E, +0x4E | free nodes' +8..+0x23 and the actor's fields seeded | `@d10`, `@ne8`, `@clamp`, `@a14`, `@f1e`, `@f1f`, `@fc`, `@f21`, `@arm3`, `@arm3early`, `@k1f`, `@f55`, `@k2order`, `@w2e`, `@b4e` |
| `fight_4cc0c` | both records' +0x36 = 0xFE5C, DS_001088A0 = 0x3C | +0x34/+0x38 and the words around 0x1088A0 seeded | `@r1`, `@r2`, `@v36`, `@wa0`, `@order` |
| `fight_4cd98` | DS_00104B00 = 6, DS_00104AEC \|= 1, 0x1088C5 / 0x10889D / 0x10889C = 0; the ball entry's +0x18 word, +0x1C bit 5, +0x1E = 4; DS_00108864 = 0 | each inside seeded neighbours | `@b00`, `@aec`, `@c5`, `@b9d`, `@b9c`, `@w18`, `@width`, `@bit5`, `@type`, `@z864` |
| `fight_4cf20` | slot +0x81 = 6; 0x1088AE/A2/A4/B2/9E[side] = 0; per entry: the list links, desc +0x10, entry +8, +0x1E, +0x1F, +0x21, +0x0C, +0x1C word, +0x10; actor +0x14, +0x2C, +0x2E, +0x4E | the five byte pairs non-zero inside seeded bytes | `@b81`, `@bae`, `@ba2`, `@ba4`, `@bb2`, `@b9e`, `@d10`, `@ne8`, `@a14`, `@f1e`, `@f1f`, `@f21`, `@fc`, `@clamp`, `@w1c`, `@e10`, `@w2e`, `@b4e` |
| `fight_4d150` | entry +0x14, +0x1E; actor +0x34, +0x29 | as `fight_4b144` | `@e14`, `@type`, `@dir`, `@b29`, `@order` |
| `fight_4d898` | 0x4B788's, and the DS_00104B1A slot's +0x5B | both slots' +0x5A..+0x5C seeded with different bytes | 0x4B788's, and `@b1a`, `@capv`, `@add1`, `@t0`..`@t5`, `@tdef`, `@t120` |
| `fight_mode25_spawn` | 0x1088B9 / 0x1088B8 = 0; per entry: the list links, desc +0x10, entry +0x1A, +8, +0x14, +0x1F, +0x21, +0x0C, +0x1C (word 0, then \|= 2), +0x10, +0x1E; actor +0x14, +0x2C, +0x29 (\|= 0x10, &= 0xBF), +0x2E, +0x4E, +0x34; DS_0010839C[i] | the table sits between seeded dwords; free and kept nodes' +8..+0x23 seeded | `@b9`, `@b8`, `@d10`, `@w1a`, `@ne8`, `@a14`, `@e14`, `@f1f`, `@f21`, `@fc`, `@w2c`, `@or10`, `@w1c`, `@e10`, `@w2e`, `@b4e`, `@and40`, `@or2`, `@type0`, `@type1`, `@w34`, `@store`, `@order` |
| `fight_4dbec` | 0x1088BB = 0, the word 0x1088B0; per entry: the list links, desc +0x10, entry +0x14, +0x1E, +8, +0x0C, +0x21, +0x1F, +0x1C word, +0x10; actor +0x14, +0x2C, +0x29 (\|= 0x10, \|= 0x40), +0x2E, +0x4E, +0x34 | 0x1088AE..0x1088BC seeded; the packed free nodes' +8..+0x23 seeded | `@bbb`, `@wb0`, `@d10`, `@e14`, `@typel`, `@ne8`, `@a14`, `@fc`, `@f21`, `@f1f`, `@clamp`, `@or10`, `@w1c`, `@e10`, `@w2e`, `@b4e`, `@w34`, `@or40` |

## §6 Queue entries not done

- 0x432EC `fight_char_text_clear`: a named gap (§4), no row.
- 0x4EBB8 (979 bytes, the mode-0x25 round card): not reached, no row. It was not read. Its EAX mask
  walks to 0 (call sites 0x4E928 and 0x4F06E); its queue line names the callees 0x1C500, 0x2F174,
  0x2F4D0 and 0x2F510, of which 0x2F174 (`text_glyph_at`) has no seam yet.
