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

"walk" is `eaxwalk.py`'s rule, the one record d1f §1 states: from the return site the code is followed
(both arms of every conditional, into direct callees to depth 4, out through `ret` into the callers of
the containing function to depth 7) tracking which parts of EAX (AL, AH, the high word) still hold the
function's value, until every part is overwritten; any read of a live part (a copy included) would set
the mask. It sees direct calls only: a function reached through a pointer is reported and followed by
hand, as 0x1DC0C and 0x4367C above.

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

11 rows, 156 mutants.

The `D2B_MASKS` / `D2B_KINDS` / `D2B_CASES` pins of `tools/tests/test_diff_verify.py` are generated from
these measurements. Every stub the batch declares carries the clobbers `E.callee_clobbers` derives from
the bytes (checked per row). Stubs the pinned dictionary of
`test_each_stub_declares_the_registers_its_callee_clobbers` did not have are added to it as new lines
(no existing line changed): 0x10D70 (ebx, ecx, edx), 0x1A8F4 (edx), 0x38BEC (), 0x46670 (), 0x2C320 (), 0x33C18 (),
0x2C088 (ebx, edx, edi, ebp), 0x1D7B8 (ebx, edx).

## §3 Corrections (port/src)

None so far.

The `port/src` edits of the batch that are not corrections:

- seams (first statement of a callee that had none): `0x1A640` `fighter_1a640` (`PR_SEAM_RET`, side),
  `0x1A8F4` `fighter_block_end` (`PR_SEAM`, rec), `0x10D70` `actor_pset_word_set` (`PR_SEAM`, rec, word),
  `0x38BEC` `fight_side_scratch_clear` (`PR_SEAM`, side), `0x46670` `fight_1081f0_clear` (`PR_SEAM0`),
  `0x2C320` `fight_scene_crowd` (`PR_SEAM`, scene), `0x33C18` `fight_char_reset` (`PR_SEAM`, side),
  `0x2C088` `prompt_press_start_clear` (`PR_SEAM`, col, row, side), `0x1D7B8`
  `fight_select_marker_spawn` (`PR_SEAM`, side, cls, y).
  Each callee's own row was re-run after its seam and is unchanged (VERIFIED, every mutant detected).
- exports for a binding: `fight_hud_bar_set_inv` (0x1DA84) loses `static` and is declared in `fight.h`.

A note on record d1g §1: it says the 0x10D70 call at 0x1DAE0 "returns through 0x1DA84: ... 0x1DC5C
(returns through 0x1DAE8 to 0x26AA1)". 0x1DC5C is not in 0x1DAE8 (which ends at 0x1DC09 `ret`): it is in
0x1DC0C, a function reached only through the 0xA86C4 table (§1 above). The mask 0 of that row stands on
the table call site's walk instead.

## §4 Named gaps and unhit blocks

No row has an `unhit_named` entry.

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

## §6 Queue entries not done

- 0x432EC `fight_char_text_clear`: a named gap (§4), no row.

Not yet reached (the queue is worked top to bottom): 0x493F0, 0x496DC, 0x4A708, 0x4A868, 0x4A8A8,
0x4AA6C, 0x4B144, 0x4B2AC, 0x4B5A8, 0x4B788, 0x4B9AC, 0x4BD98, 0x4CC0C, 0x4CD98, 0x4CF20, 0x4D150,
0x4D898, 0x4DBEC, 0x4E350, 0x4EBB8.
