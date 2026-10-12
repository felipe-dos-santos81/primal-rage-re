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

## §2 The measured rows

`blocks` is hit/total; `mutants` is detected/total.

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fight_stance_pass` | 0x1A978 | 16 | 25/25 | 24/24 | 0 | 0x33A10 runs on both sides (allowed); 0x3B134 stub (it rewrites both command words), 0x1A6AC and 0x1A8F4 stubs; 0x1A640 runs on both sides, recorded |
| `fight_hud_bar_set` | 0x1D2F0 | 11 | 20/20 | 11/11 | 0 | 0x10D70 stub (`ret 4`) |
| `fight_hud_bar2_set` | 0x1D464 | 9 | 8/8 | 8/8 | 0 | 0x10D70 stub |
| `fight_hud_bar_set_inv` | 0x1DA84 | 10 | 8/8 | 8/8 | 0 | 0x10D70 stub |
| `fight_round_reset` | 0x20EF8 | 3 | 3/3 | 18/18 | 0 | 0x38BEC and 0x46670 stubs, EAX varied per case |

5 rows, 69 mutants.

The `D2B_MASKS` / `D2B_KINDS` / `D2B_CASES` pins of `tools/tests/test_diff_verify.py` are generated from
these measurements. Every stub the batch declares carries the clobbers `E.callee_clobbers` derives from
the bytes (checked per row). Stubs the pinned dictionary of
`test_each_stub_declares_the_registers_its_callee_clobbers` did not have are added to it as new lines
(no existing line changed): 0x10D70 (ebx, ecx, edx), 0x1A8F4 (edx), 0x38BEC (), 0x46670 ().

## §3 Corrections (port/src)

None so far.

The `port/src` edits of the batch that are not corrections:

- seams (first statement of a callee that had none): `0x1A640` `fighter_1a640` (`PR_SEAM_RET`, side),
  `0x1A8F4` `fighter_block_end` (`PR_SEAM`, rec), `0x10D70` `actor_pset_word_set` (`PR_SEAM`, rec, word),
  `0x38BEC` `fight_side_scratch_clear` (`PR_SEAM`, side), `0x46670` `fight_1081f0_clear` (`PR_SEAM0`).
  The five callees' own rows were re-run after the seams and are unchanged (VERIFIED, every mutant
  detected).
- exports for a binding: `fight_hud_bar_set_inv` (0x1DA84) loses `static` and is declared in `fight.h`.

A note on record d1g §1: it says the 0x10D70 call at 0x1DAE0 "returns through 0x1DA84: ... 0x1DC5C
(returns through 0x1DAE8 to 0x26AA1)". 0x1DC5C is not in 0x1DAE8 (which ends at 0x1DC09 `ret`): it is in
0x1DC0C, a function reached only through the 0xA86C4 table (§1 above). The mask 0 of that row stands on
the table call site's walk instead.

## §4 Named gaps and unhit blocks

None so far: no row has an `unhit_named` entry and no row is a named gap.

Limits worth knowing:
- `fight_stance_pass`: 0x1A640 runs its own bytes, so the row sees its three results (0, 0x1000, 0x2000)
  and no other EAX. The port reads the side's command word before the 0x1A640 call and the raw after
  it (0x1AAA2); 0x1A640 writes nothing, so the two orders cannot differ.
- the three bar rows compare 0x10D70's word argument as all of EAX; every caller zero-extends it.

## §5 Store sweep

| row | stores | pre-state / neighbours | store mutants caught |
|---|---|---|---|
| `fight_stance_pass` | slot +0x61 (dec, then 0), +0x54, +0x60 (dec, then 0x28), +0x62, +0x53, the other slot's +0x8A | both slots' +0x52..+0x55, +0x5C..+0x65 and +0x82..+0x8B hold different non-zero bytes per side | `@c61`, `@s54`, `@c60`, `@c62`, `@c53`, `@w8a` (each dropped), `@order` (the +0x60 decrement moved after the 0x1A640 call: call memory alone) |
| `fight_hud_bar_set`, `fight_hud_bar2_set`, `fight_hud_bar_set_inv` | none (the word goes to 0x10D70) | the record pointers and the mode byte sit between seeded neighbours | — |
| `fight_round_reset` | per side: slot +0x74, +0x76 (twice), +0x78, +0x84, +0x8C words; bytes 0x1078F2[s], 0x100B5E[s], 0xFD158..0xFD15B + 4s; dword 0xFD148 + 4s; word 0x100B50[s] = 0xFFFF; byte 0x100C1D | every stored byte starts non-zero (0x100B50's words below 0xFFFF) inside non-zero neighbours | `@w74`, `@w76`, `@w78`, `@w84`, `@w8c`, `@b8f2`, `@b5e`, `@fd158`, `@fd148`, `@w50`, `@c1d` (each dropped), `@width` (a dword at +0x8C), `@ffff`, `@stride` |

## §6 Queue entries not done

Not yet reached (the queue is worked top to bottom): 0x412A0, 0x41350, 0x432EC, 0x433DC, 0x43A08,
0x43FBC, 0x442A0, 0x493F0, 0x496DC, 0x4A708, 0x4A868, 0x4A8A8, 0x4AA6C, 0x4B144, 0x4B2AC, 0x4B5A8,
0x4B788, 0x4B9AC, 0x4BD98, 0x4CC0C, 0x4CD98, 0x4CF20, 0x4D150, 0x4D898, 0x4DBEC, 0x4E350, 0x4EBB8.
