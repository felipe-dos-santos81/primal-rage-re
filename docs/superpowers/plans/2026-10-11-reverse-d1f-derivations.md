# Track D1 batch d1f — derivations (game/fight.c, second half)

Differential rows for the 22 functions of `.superpowers/sdd/2026-10-11-reverse-d1/d1f-queue.txt`.
Every queue entry has a row. Ground truth: the fixed-up image `build/diffrun --image-out` writes
(linear addresses), disassembled with capstone. All figures below were measured with
`python3 tools/diff_verify.py --image <image> --function <name> --self-check`, one row at a time.
The full `--self-check`, the unittest suite, `make verify` and the oracles were NOT run on this
branch (the integrator runs them on the merged tree), so no branch counter line is recorded here.

## §1 Members and the callers that fix each EAX mask

Call sites are every `E8`/`E9 rel32` in 0x10000..0x5D000 that targets the entry; a dword scan of the
whole image found no stored pointer to any of the 22 entries. "walk" means: from each return site
the code was followed (both arms of every conditional, into direct callees to depth 4, out through
`ret` into the callers of the containing function to depth 7) tracking which parts of EAX (AL, AH,
the high word) still held the function's value; the walk ended with every part overwritten before
any read. A masking `and eax,imm` counts as an overwrite of the parts its immediate clears.

| addr | port function | size | call sites | what the callers read of EAX | mask |
|---|---|---|---|---|---|
| 0x43D0C | `fight_char_button` | 83 | 0x43D9D | 0x43DA2 `mov edx,eax`, 0x43DA4 `mov [ebx+0x105b34],al` | 0xFFFFFFFF |
| 0x43EA0 | `fight_char_portrait` | 283 | 0x43CA1 | 0x43CA6 `mov eax,edx` | 0 |
| 0x44054 | `fight_char_team_tag_drop` | 56 | 0x443B4 | 0x443C1 `jmp 0x4445a`, 0x4445A `lea eax,[ecx*8]` | 0 |
| 0x4408C | `fight_char_team_tag_add` | 253 | 0x44403 | 0x44414 `xor eax,eax` | 0 |
| 0x4418C | `fight_char_team_portrait` | 274 | 0x448E8 | 0x448ED `mov eax,edx` | 0 |
| 0x46670 | `fight_1081f0_clear` | 37 | 0x20F8C | 0x20F91 `xor ah,ah`, then 0x20EF8 returns: its callers 0x25C70 (`pop edx; ret`) and 0x26AAF (0x26AB4 `mov ah,4`); walk: dead on every path | 0 |
| 0x49300 | `fight_list_init` | 135 | 0x20E38, 0x20EDF | the next instruction calls 0x28E98, whose first use of EAX is the load 0x28ECA `mov eax,0x104888`; walk: dead | 0 |
| 0x49388 | `fight_dust_pick` | 104 | 0x495B2, 0x49753, 0x49961, 0x4C973, 0x4CFEA, 0x4DCC5 | each indexes with all of EAX: `[eax*4+0xc9524]` (0x495B9, 0x49758, 0x49966, 0x4C97F, 0x4CFF1, 0x4DCCA) | 0xFFFFFFFF |
| 0x496AC | `fight_dust_clamp` | 47 | 17 (0x49630 … 0x4DE4B) | each stores AX (`mov word ptr [reg+0x2c],ax`); the row compares all of EAX, which is stricter than the callers need | 0xFFFFFFFF |
| 0x4A634 | `fight_4a634` | 211 | 0x4A591 | 0x4A596 `xor eax,eax` | 0 |
| 0x4A7D4 | `fight_4a7d4` | 49 | 0x4A196 | 0x4A19B `test eax,eax` | 0xFFFFFFFF |
| 0x4A808 | `fight_4a808` | 95 | 0x4E74B | 0x4E750 `test eax,eax` | 0xFFFFFFFF |
| 0x4AC38 | `fight_4ac38` | 70 | 0x49D86, 0x49DA9, 0x4AC2D, 0x4C074, 0x4C097, 0x4D382, 0x4D412, 0x4D435, 0x4D6E0 | walk: dead on every path (0x4A468 → 0x4A476 `xor eax,eax` or the loop head; 0x4C1F3 `mov ax,[ecx+0x1c]` … 0x4C1FB `and eax,0xffff`; 0x4D6E9 `xor eax,eax`) | 0 |
| 0x4AF04 | `fight_4af04` | 288 | 0x4A0A2 | 0x4A0A7 `test al,al`; walk on AH and the high word: dead | 0xFF |
| 0x4B3F0 | `fight_4b3f0` | 63 | 7 sites | 0x4AB1F, 0x4AB74, 0x4D27F `test eax,eax` | 0xFFFFFFFF |
| 0x4B430 | `fight_4b430` | 63 | 8 sites | 0x4A154, 0x4AB03, 0x4AB9E `test eax,eax` | 0xFFFFFFFF |
| 0x4BD4C | `fight_4bd4c` | 76 | 0x49D3F, 0x4AB34, 0x4D236, 0x4D3C5 | each `test eax,eax` (0x49D44, 0x4AB39, 0x4D23B, 0x4D3CA) | 0xFFFFFFFF |
| 0x4CB18 | `fight_4cb18` | 244 | 0x4B59E, 0x4C760, 0x4C774 | all three return through 0x4B470 / 0x4C60C; walk: dead on every path | 0 |
| 0x4D108 | `fight_4d108` | 69 | 0x4D247 | 0x4D24C `test al,al`; walk on AH and the high word: dead | 0xFF |
| 0x4DBB4 | `fight_slot_5b_add` | 53 | 0x12C64, 0x4F638 | 0x12C64 returns from 0x12BB8, whose four callers load `mov eax,1` (0x12AA8, 0x12B04, 0x12B56, 0x12BA2). 0x4F638 returns from code reached only through the 0xA86C4 table (entries 0x4F4E8, 0x4F5C8), called at 0x25623 `call [eax+0xa86c4]`; from its return site 0x25629 the walk is dead (0x25621 `mov eax,ebx`, 0x2563E `mov eax,[0x10150c]`) | 0 |
| 0x4E27C | `fight_mode25_face` | 131 | 0x4E361 | walk: dead on every path | 0 |
| 0x4E99C | `fight_4e99c` | 540 | 0x4E923 | the next instruction calls 0x4EBB8, which loads 0x4EBC1 `mov eax,0x62` before any read | 0 |

Limit of the walk: it sees direct calls only. The single indirect path met (0x25623) was followed
by hand as stated; a pointer call of any other function on those paths would not have been seen.

## §2 The measured rows

`blocks` is hit/total; no row names an unhit block.

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fight_char_button` | 0x43D0C | 8 | 7/7 | 6/6 | 0xFFFFFFFF | no callee |
| `fight_1081f0_clear` | 0x46670 | 2 | 3/3 | 6/6 | 0 | no callee |
| `fight_dust_clamp` | 0x496AC | 11 | 5/5 | 4/4 | 0xFFFFFFFF | no callee |
| `fight_4a7d4` | 0x4A7D4 | 9 | 5/5 | 6/6 | 0xFFFFFFFF | no callee |
| `fight_4a808` | 0x4A808 | 10 | 12/12 | 6/6 | 0xFFFFFFFF | no callee |
| `fight_4af04` | 0x4AF04 | 34 | 24/24 | 8/8 | 0xFF | no callee; the seven targets of the 0x4AEE8 jump table each hit on both arms |
| `fight_slot_5b_add` | 0x4DBB4 | 8 | 3/3 | 6/6 | 0 | no callee |
| `fight_4ac38` | 0x4AC38 | 3 | 1/1 | 11/11 | 0 | 0x2BC30 stub |
| `fight_4b3f0` | 0x4B3F0 | 6 | 6/6 | 9/9 | 0xFFFFFFFF | 0x2BC30 stub |
| `fight_4b430` | 0x4B430 | 6 | 6/6 | 9/9 | 0xFFFFFFFF | 0x2BC30 stub |
| `fight_4bd4c` | 0x4BD4C | 4 | 3/3 | 9/9 | 0xFFFFFFFF | 0x2BC30 stub |
| `fight_4d108` | 0x4D108 | 5 | 3/3 | 10/10 | 0xFF | 0x5D7DC, 0x2BC30 stubs |
| `fight_mode25_face` | 0x4E27C | 6 | 6/6 | 10/10 | 0 | 0x2BC30 stub (it rewrites the first entry's next pointer, a harness write) |
| `fight_4cb18` | 0x4CB18 | 7 | 19/19 | 18/18 | 0 | 0x2C3FC, 0x2BC30, 0x1A570 stubs |
| `fight_char_portrait` | 0x43EA0 | 8 | 5/5 | 16/16 | 0 | 0x2BCF4, 0x2A17C, 0x2C3FC stubs |
| `fight_char_team_portrait` | 0x4418C | 8 | 6/6 | 15/15 | 0 | 0x2BCF4, 0x2A17C, 0x2C3FC stubs |
| `fight_char_team_tag_drop` | 0x44054 | 5 | 3/3 | 8/8 | 0 | 0x2AD40 stub |
| `fight_char_team_tag_add` | 0x4408C | 6 | 10/10 | 13/13 | 0 | 0x33754, 0x2A820 stubs; 0x2AE14 and its exercised tree (0x2AC80, 0x249D0, 0x249B0, 0x2EA30, 0x2A408, 0x5D812, 0x1C390, 0x1C3A0) run on both sides: the descriptor is on the caller's stack |
| `fight_dust_pick` | 0x49388 | 12 | 14/14 | 10/10 | 0xFFFFFFFF | 0x5D7DC stub |
| `fight_list_init` | 0x49300 | 3 | 5/5 | 9/9 | 0 | 0x249C0 runs on both sides (allowed; no seam; its own row is `list_insert_before`) |
| `fight_4a634` | 0x4A634 | 9 | 19/19 | 15/15 | 0 | 0x5D7DC runs real and is recorded (`P6_RNG`; the draws come from the 0xEF6D8 seed, because the 0xCA/0xDB arm needs two rng(2) draws that differ and a stub has one EAX per case); 0x2C3FC stub |
| `fight_4e99c` | 0x4E99C | 15 | 30/30 | 17/17 | 0 | 0x2C3FC, 0x34D8C stubs |

22 rows, 221 mutants, every mutant detected. The `D1F_MASKS` / `D1F_KINDS` / `D1F_CASES` pins of
`tools/tests/test_diff_verify.py` are these measurements: all 22 masks, 221 kind sets and 221 case
sets were re-measured on the final tree and compared equal to the file. The last four rows (the
last commit) were measured ten times with identical results. Every stub the batch uses declares the
clobbers `E.callee_clobbers` derives from the bytes (0x1A570, 0x2C3FC, 0x33754, 0x34D8C, 0x5D7DC: none;
0x2A17C, 0x2A820, 0x2AD40, 0x2BC30, 0x2BCF4: EDX), and the stub dictionary equals the one pinned in
`test_each_stub_declares_the_registers_its_callee_clobbers` (checked without running the suite).

Mutants added on takeover, each proving an input the cases already separated:
`fight_4a634@gate` (the first rng(2) draw's test at 0x4A6A3/0x4A6A5 removed; caught by e3 alone, the
case whose first draw is 0) and `fight_4e99c@sext` (the 0x1088BC character byte sign-extended; the
raw zero-extends it, 0x4E9E3 `xor eax,eax` / 0x4E9E5 `mov al,[0x1088bc]`; caught by y14 alone,
the 0x80 case).

## §3 Corrections (port/src, each in its own commit)

Both align the port to the instruction; neither changes what the game can do, and both are
unproven against the oracles until the integrator's full gate.

1. **`fight_4cb18` tests 0x1A570's AL — raw 0x4CB92** (commit 0be5f7a). 0x4CB18 calls 0x1A570 at
   0x4CB8B and tests AL alone (0x4CB92 `test al,al`). Old: `if (fighter_actor_bit15_clear(side) != 0)`;
   new: `if ((u8)fighter_actor_bit15_clear(side) != 0u)`. 0x1A570 leaves EAX = 0x8000 with AL = 0
   when the bit is set (0x1A59C `and ah,0x80`, 0x1A59F `and eax,0xffff`, 0x1A5A4 `sete al`); the C
   callee returns 0 or 1, so the two forms differ only under the row's 0x1A570 stub (case t2, stub
   EAX 0x8000). The `@al` mutant is the old form.
2. **`fight_dust_pick` compares the draw signed — raw 0x493B3** (commit abe3029). The five
   threshold compares are `cmp eax,imm` / `jge` (0x493B0/0x493B3, 0x493BC/0x493BF, 0x493C8/0x493CB,
   0x493D4/0x493D7, 0x493E0/0x493E3). Old: `u32 v`, unsigned `<`; new: `s32 v`, signed `<`.
   0x5D7DC returns 0..0xFFFE, so the forms agree on every value the game can draw; they differ
   only on a negative draw, which the row's stub supplies (case d11, stub EAX 0x80000000:
   original 4, the old port 2). The `@unsigned` mutant is the old form.

The other `port/src` edits of the batch are exports only: thirteen functions of `fight.c` lose
`static` and are declared in `fight.h` for their bindings (`fight_1081f0_clear`, `fight_dust_clamp`,
`fight_4a7d4`, `fight_4a808`, `fight_4af04`, `fight_4ac38`, `fight_4b3f0`, `fight_4b430`,
`fight_4bd4c`, `fight_4d108`, `fight_dust_pick`, `fight_4a634`, `fight_4e99c`). No seam was added:
every stubbed or recorded callee already had one.

## §4 Named gaps and unhit blocks

None. No row has an `unhit_named` entry and no row is a named gap.

Dead edges in 0x4E99C (the blocks at both ends are hit, so nothing is unhit): the taken edge of
0x4E9DE/0x4E9E1 `cmp ecx,5; jge 0x4ea06` (ECX < 5 was just established at 0x4E9CF), and the taken
edge of the `jne 0x4eb77` at 0x4EA80 and at 0x4EB49 (each reached only by the `je` at 0x4EA64 /
0x4EB30, ZF = 1). The port omits them.

Limits worth knowing:
- `fight_4e99c` case y14 addresses the row at 0x108888 + 0x80 * 5 = 0x108B08, which the fixture
  does not seed (BSS zero on both sides); its neighbours are therefore unseeded in that one case.
- `fight_4a634` covers the rng paths the nine seeds produce; the draws are not chosen one by one.
- `fight_dust_clamp`'s full-EAX compare claims more than its callers read (AX).

## §5 Store sweep

For the four rows of the last commit, read against the fixtures and proven by a mutant each:

| row | stores | pre-state / neighbours | store mutants caught (dropped, moved or wrong value) |
|---|---|---|---|
| `fight_list_init` | 0x108880, 0x108850, 0x10884C, 0x1083C8, 0x1083C4, the links 0x249C0 writes, 0x1088CC, 0x1088CB | 0x108880 = 0x0BAD8880; 0x1083C4..0x108853 filled with address-derived nonzero bytes; 0x1088CA..0x1088CD = CA 77 88 CD | `@self` (0x108850 dropped), `@v1500`, `@swap` |
| `fight_4a634` | slot +0x42 (both sides), 0x10889E/9F, 0x1088B2/B3 | +0x41/+0x43 seeded; 0x10889D..A0 = 9D 11 22 A0; 0x1088B1..B4 = B1 33 44 B4 | `@b9e`, `@bb2` (dropped), `@off` (one byte low), `@clr` (mask 0xFE) — each caught by all nine cases |
| `fight_4e99c` | 0x1088BD (twice), the row byte +0 or +1 at 0x108888/0x10888A + ch*5, 0x1088BC, 0x1088B9 | 0x1088B8..BE = B8 b9 BA BB ch bd BE with b9 = 0x77; both 5-byte rows seeded with different bytes | `@inc`, `@save`, `@b9` (dropped), `@idx`, `@hi` (moved) |
| `fight_dust_pick` | none (a pure function of the mode word, the range word and the draw) | the word after the two range words is seeded for the `@width` read | — |

For the eighteen rows of the earlier commits the dropped-store mutants read from the binding block
are: `fight_1081f0_clear` `@byte`, `@d0`, `@d4`; `fight_4ac38` `@and`, `@or`, `@w34`, `@w36`, `@w38`,
`@type`; `fight_4bd4c` `@w38`, `@w34`, `@w36`; `fight_4d108` `@w1a`; `fight_mode25_face`
`@flip`; `fight_char_portrait` / `fight_char_team_portrait` `@bit`; `fight_char_team_tag_drop` `@keep`.
The other store mutants of those rows (`@field`, `@flag`, `@f55`, `@y`, `@w6` … `@wc`, `@slot`, …)
were not re-read one by one on takeover; that each is detected, and by which cases, is measured (§2).
Their fixtures seed the neighbours of every field named in the docstrings (`d1f_ent_case`,
`d1f_port_case`, `d1f_drop_case`, `d1f_tag_case`, `d1f_clr_case`, `d1f_5b_case`).

The sweep found no store the port makes that the raw does not, and none it misses: all 22 rows
are VERIFIED with a byte-exact memory compare.

## §6 Queue entries not done

None: all 22 entries of `d1f-queue.txt` have a row.
