# Track D, wave 4, batch d4e — derivations (follow-ups on rowed functions)

The work list is `.superpowers/sdd/2026-10-11-reverse-d1/d4e-tasks.md`: places where a read-only
audit saw the port differ from the raw, on functions that already have a differential row. Ground
truth: the fixed-up image `build/diffrun --image-out` writes (linear addresses), disassembled with
capstone. Every figure below was measured with `python3 tools/diff_verify.py --image <image>
--function <name> --self-check`, one row at a time. On the final tree every d4e spec and mutant was
re-measured against its pin, and each binding's port side was run ten times with the same output
each time. The full `--self-check` was run once on the final tree (`PR_DIFF_JOBS=4`): `748/748
functions VERIFIED; 7915/7915 mutants detected; 1 named gaps; 493/581 rows with callees closed (167
have none)`, rc 0 (the base tree's 733 / 7850 plus this batch's 15 specs and 65 mutants). The
unittest suite, `make verify`, `make diff-verify` and the oracles were NOT run on this branch, so
every `port/src` correction below is unproven against the oracles.

No existing spec, binding or pin line was changed: every proof is a second spec on the entry (the
form of d3b's `_rr` specs), added in the `D4E_SPECS` block. `_al` specs stub a predicate the first
row runs on both sides; `_rr` specs use stubs that rewrite a value the raw loads again.

## §1 Members and EAX masks

Each second spec keeps the mask of the row it repeats (the record that row cites). The one new row:

| addr | port function | size | call sites | what the callers read of EAX | mask |
|---|---|---|---|---|---|
| 0x14814 | `fighter_14814` | 247 | 0x14920 (in 0x1490C); the dword at 0xA46D0 (the only stored pointer in the image) | every path ends `mov al,1` (0x1485F, 0x14903) and the port function is void; 0x14925 `mov eax,[esp]`; the reaction dispatcher returns the callback's AL from 0x34E2C, as for its twin 0x14B90 (record d2d §1). The constant AL is not compared | 0 |

## §2 The measured rows

`blocks` is hit/total; `mutants` is detected/total.

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fighter_146f0_al` | 0x146F0 | 6 | 12/12 | 1/1 | 0 | d2a's cases; 0x14590 a stub (d3d's `D3D_14590`) returning 1, 0, 0x100, 0xFFFFFF00, 0xFFFFFF01, 0x80 |
| `fighter_14a5c_al` | 0x14A5C | 6 | 12/12 | 1/1 | 0 | the twin, the same stub values |
| `fighter_14b90_al` | 0x14B90 | 6 | 10/10 | 2/2 | 0 | d2d's cases; 0x396AC (`D3C_396AC`) and 0x14590 stubs |
| `fighter_14814` | 0x14814 | 6 | 10/10 | 29/29 | 0 | NEW row; the voice, 0x18B44, 0x3C4CC, 0x396AC and 0x14590 stubs; 0x339AC runs on both sides |
| `fight_hud_meter_step_al` | 0x1D540 | 12 | 35/35 | 1/1 | 0 | d3d's cases; 0x1D74C a stub (seamed here) |
| `camera_box_palette_al` | 0x15B90 | 6 | 6/6 | 1/1 | 0 | d2d's cases; 0x1A570 the `BIT15` stub |
| `camera_page_tail_b_al` | 0x164F4 | 16 | 23/23 | 1/1 | 0xFF | d2d's 14 cases and u0 / u1; 0x1A570 a stub |
| `camera_page_tail_a_al` | 0x16AFC | 17 | 27/27 | 1/1 | 0xFF | d2d's 15 cases and u0 / u1; 0x1A570 a stub |
| `fighter_reaction_al` | 0x3B714 | 11 | 22/23 | 3/3 | 0 | C2b's 9 cases and h0 / h1; 0x39EFC a stub too; the fatal block 0x3B75D stays named |
| `fighter_attack_consume_al` | 0x3BDDC | 8 | 16/16 | 2/2 | 0xFF | C3g's cases and stubs |
| `fight_char_opp_text_clear_rr` | 0x433DC | 2 | 5/5 | 1/1 | 0 | d3b's release stub that rewrites both col words 0xC894C |
| `fight_4aad0_rr2` | 0x4AAD0 | 8 | 27/27 | 9/9 | 0 | every stub rewrites the entry's +0xC (0x2BE1C also +8); 0x2BE00 a stub whose two writes are relative to its argument |
| `fight_4b5a8_rr` | 0x4B5A8 | 5 | 10/10 | 5/5 | 0xFF | 0x2BC30 rewrites +8 and +0xC, 0x2BE00 +8, 0x2BE4C +0xC |
| `fight_4b470_rr` | 0x4B470 | 6 | 23/23 | 5/5 | 0 | the voice, 0x2BC30, the spawn and 0x1A570 each leave another actor in +8 |
| `camera_mode_track_pair_rr` | 0x12E3C | 6 | 24/24 | 3/3 | 0 | the 0x18714 stub rewrites the camera x 0xF0AF0 and both slots' +0x34 |

15 specs (14 second specs and one new row), 65 mutants. The `D4E_MASKS` / `D4E_KINDS` /
`D4E_CASES` pins of `tools/tests/test_diff_verify.py` are these measurements.

Stub clobbers: every stub reuses an existing `E.Call` constant or a `dataclasses.replace` of one
(mode or `writes` alone), except `D4E_1D74C = E.Call(0x1D74C, ("eax",))`:
`E.callee_clobbers(image, 0x1D74C)` is `()` (its bytes 0x1D74C..0x1D760 write EAX only). The line
`0x1D74C: (),` is added to the pinned dictionary of
`test_each_stub_declares_the_registers_its_callee_clobbers`, with its comment line, directly after
`self.assertEqual(stubs, {`.

## §3 Corrections (port/src)

"Ordinary play: no difference" below means the two readings agree for every value the port's own
callee returns; each differs only under a stub.

### §3.1 AL tests the port made on the whole int (task item 1)

Each site is `call` / `test al,al` / a conditional jump on ZF. Old: `callee(...) != 0` (or `!callee`)
on the whole C int. New: `(u8)callee(...) != 0u`.

| function | call | test | old-port measurement (the `_al` spec on the port before the commit) | mutant = the old port | catching cases |
|---|---|---|---|---|---|
| `fighter_146f0` | 0x14702 `call 0x14590` | 0x14707 `test al,al` / 0x14709 `jbe` (CF clear: `je`) | not measured before the fix; the mutant is the measurement | `fighter_146f0_al@eax` | c2 (0x100), c3 (0xFFFFFF00) |
| `fighter_14a5c` | 0x14A6E `call 0x14590` | 0x14A73 / `je 0x14a7e` | as above | `fighter_14a5c_al@eax` | c2, c3 |
| `fighter_14b90` | 0x14BAF `call 0x396ac` | 0x14BB4 / `je 0x14be4` | as above | `fighter_14b90_al@eax396` | b0, c0 |
| `fighter_14b90` | 0x14BE7 `call 0x14590` | 0x14BEC / `jbe 0x14bfe` | as above | `fighter_14b90_al@eax590` | b1, c0 |
| `fighter_14814` | 0x14833 `call 0x396ac` | 0x14838 / `je 0x1486a` | as above | `fighter_14814@eax396` | b0, c0 |
| `fighter_14814` | 0x1486D `call 0x14590` | 0x14872 / `je 0x14884` | as above | `fighter_14814@eax590` | b1, c0 |
| `fight_hud_meter_step` | 0x1D5FF `call 0x1d74c` | 0x1D604 / `jne 0x1d615` | `p2: byte 0x10780D: original unchanged, port 0x08`; `p4: byte 0x1078A1: original unchanged, port 0x01` | `fight_hud_meter_step_al@eaxcmd` | p2, p4 |
| `camera_box_palette` | 0x15C17 `call 0x1a570` | 0x15C1C / `jne 0x15c2b` | `x2: byte 0x10A310: original 0x86, port 0x7A`; `x3: byte 0x10A311: original 0x01, port 0x00` | `camera_box_palette_al@eax` | x2, x3 |
| `camera_page_tail_b` | 0x16615 `call 0x1a570` | 0x1661A / `jne 0x1662a` | `t0: byte 0xFD158: original 0xA4, port 0x55`; u0 the same at 0xFD15C | `camera_page_tail_b_al@eax` | t0, u0 |
| `camera_page_tail_a` | 0x16B46 `call 0x1a570` (the sprite-code arm) | 0x16B4B / `jne 0x16b5b` | `k0: byte 0x10A310: original 0xA4, port 0xC1` | `camera_page_tail_a_al@eax` | k0 |
| `camera_page_tail_a` | 0x16C80 `call 0x1a570` (the scan) | 0x16C85 / `jne 0x16c95` | `t0: byte 0xFD138: original 0xA4, port 0x55` | the same mutant | t0, u0 |
| `camera_project` | 0x1819B `call 0x1a570` | 0x181A0 / `jne 0x181ac` | no row for 0x17FA0 | none: the instruction alone | — |
| `fighter_reaction` | 0x3B743 `call 0x3c59c` | 0x3B748 / `jne 0x3b8cc` | `r4: call #1: original 0x39EFC(0x1), port none` | `fighter_reaction_al@eaxflag` | r4, r5 |
| `fighter_reaction` | 0x3B797 `call 0x39efc` | 0x3B79C / `je 0x3b7b8` | (in the same run) | `fighter_reaction_al@eaxefc` | r5, r6 |
| `fighter_reaction` | 0x3B8AF `call 0x3b6c4` | 0x3B8B4 / `je 0x3b8c8` | (in the same run) | `fighter_reaction_al@eaxhold` | h0, h1 |
| `fighter_attack_consume` | 0x3BE61 `call 0x3cf38` | 0x3BE66 / `je 0x3be71` | `a3: call #1: original 0x4649C(0x0, 0x0, 0x5, 0x4000), port none` | `fighter_attack_consume_al@eaxchain` | a3, a5 |
| `fighter_attack_consume` | 0x3BE7F `call 0x4649c` | 0x3BE84 `mov dl,al`, 0x3BE9E `test dl,dl` / `je 0x3bebc` | (in the same run) | `fighter_attack_consume_al@eaxscan` | a4, a5 |
| `fighter_think` | 0x1979B `call 0x3962c` | 0x197A0 / `je 0x197ba` | no row for 0x1975C (§4) | none: the instruction alone | — |
| `fighter_think` | 0x197BF `call 0x396ac` | 0x197C4 / `je 0x197e4` | as above | none | — |

Notes:

- 0x39EFC is the one callee here whose raw really leaves a non-zero EAX with AL = 0: 0x39F09 `mov
  eax,[esp+0xc]` (the slot address) then `xor al,al` (0x39F13, 0x39F23, 0x39F30). A whole-register
  test of the RAW callee would be wrong in play; the port's `fighter_39efc` returns 0 or 1, so the
  old port agreed in play. 0x1D74C leaves EAX 0 or 1 whole (`and eax,0xffff` 0x1D758, `setne al`).
- Ordinary play: no difference at any site (every port predicate returns 0 or 1).
- The audit note's 0x14872 and 0x14BEC were in `fighter_14814` and `fighter_14b90`; both functions
  also had the 0x396AC site (0x14838, 0x14BB4), not in the note. `fighter_reaction` had three sites
  (the note named 0x3B748), `fighter_attack_consume` two (the note named 0x3BE66).
- `camera_actor_bit15_clear` (camera.c's static copy of 0x1A570) is called at five sites; all five
  are corrected. The fifth, in `camera_project` (0x17FA0), has no row.

### §3.2 `fight_char_opp_text_clear` (0x433DC) reads the col word before each release

0x43422 `mov di,[esi+0xc894c]` (EDI zeroed at 0x4341B) after the first 0x1C500, and 0x4343C `mov
si,[esi+0xc894c]` (`and esi,0xffff` 0x4344D) after the second 0x1C500 and so after the first
0x2F280. Old: one read into a local before the first string. New: one read per release, after its
`game_string_get`. Measured on the old port with `fight_char_opp_text_clear_rr`: `q0: call #3:
original 0x2F280(0x77, 0x1C, 0x102760, 0x1000), port 0x2F280(0x8003, 0x1C, 0x102760, 0x1000)`;
`@cache` (d2b's core at mutation set 0) is the old port. Ordinary play: no difference (0xC894C is a
table in the data object that neither 0x1C500 nor 0x2F280 writes).

### §3.3 `fight_4aad0` (0x4AAD0) loads the entry's pointers and the midpoint after each call

The raw loads `[ecx+0xc]` at 0x4AAEF, 0x4AB0B (after 0x4B430), 0x4AB52 (after 0x4B3F0 / 0x4BD4C /
0x4B5A8), 0x4AB7C (after 0x4B3F0), 0x4ABA6 (after 0x4B430) and 0x4ABC7 (after 0x2BE1C and the first
0x2BE00); `[ecx+8]` at 0x4ABA9 and again at 0x4ABB7 (after 0x2BE1C); the midpoint dword 0x108874 at
0x4ABBF and 0x4ABD1, each after a 0x2BE00. Old: `slot = DSD(entry + 0xC)` once, and `DSD(0x108874)
- fight_2be00(...)` with the load's position left to the compiler's operand order. New: each load
where the raw has it. Measured on the old port with `fight_4aad0_rr2`: `y0: call #1: original
0x4B3F0(0x10A000, 0x2, 0x0), port 0x4B5A8(0x10A000, 0x2)`. Mutants: `@c0c2` .. `@c0c6` keep the slot
pointer across the call before the second .. sixth load, `@c8` the actor pointer across 0x2BE1C,
`@c874a` / `@c874b` the midpoint across the first / second 0x2BE00, `@cache` is the old port (d3b's
core at mutation set 0). The `-d` is `(s32)(0u - (u32)d)` now (0x4ABDF / 0x4ABED `neg`; task item
4). Ordinary play: no difference seen (none of the callees is seen to write the entry's +8 / +0xC
or 0x108874; unverified for the 0x2BC30 tree under 0x4B3F0 / 0x4B430).

### §3.4 `fight_4b5a8` (0x4B5A8) loads the entry's actor and slot pointers after each call

`[ebx+8]`: 0x4B5DE (0x2BC30's EAX), then again 0x4B5EB, 0x4B5F6, 0x4B614 / 0x4B62F (after
0x2BC30) and 0x4B648 (after 0x2BE00). `[ebx+0xc]` and its record `[eax]`: 0x4B5AC / 0x4B5AF, then
0x4B5FF / 0x4B602 and 0x4B61D / 0x4B638 (after 0x2BC30), 0x4B653 / 0x4B65A and 0x4B67B / 0x4B67E
(after 0x2BE4C), with the record's +0x51 read each time (0x4B65C, 0x4B680). Old: slot, record,
character byte and actor read once at entry. New: each load where the raw has it. Measured on the
old port with `fight_4b5a8_rr`: `q0: call #1: original 0x2BE00(0x10A280), port 0x2BE00(0x10A200)`
and `q0: call #2: original 0x2BE4C(0x10A180, 0x6800), port 0x2BE4C(0x10A100, 0x3800)`. Mutants:
`@c8a` / `@cca` keep the actor / the slot across 0x2BC30, `@c8b` the actor across 0x2BE00, `@ccb`
the slot across 0x2BE4C, `@cache` is the old port (d2b's core at mutation set 0). The ±0x1800 is
unsigned now (0x4B627 / 0x4B642 `lea`). Ordinary play: no difference seen (unverified for the
0x2BC30 tree).

### §3.5 `fight_4b470` (0x4B470) loads the entry's actor pointer after each call

`[esi+8]`: 0x4B478 (the +0x48 id), 0x4B4A3 (after the voice: 0x2BC30's EAX), 0x4B4BD (after
0x2BC30: the spawn's x and y), 0x4B4FF / 0x4B504 (after the spawn and 0x1A570), 0x4B50F (after the
spawn) and 0x4B52B (the +0x36 store). Old: `rec = DSD(entry + 8)` once. New: each load where the
raw has it. Measured on the old port with `fight_4b470_rr`: `r0: call #1: original
0x2BC30(0x10A140, 0xEE160, 0x40400000), port 0x2BC30(0x10A100, 0xEE160, 0x40400000)`. Mutants:
`@cv`, `@ca`, `@cs`, `@cb` keep the actor across the voice, 0x2BC30, the spawn and 0x1A570; `@cache`
across all four (the old port). Ordinary play: no difference seen (unverified for the 0x2BC30 and
0x2AE14 trees).

### §3.6 `camera_mode_track_pair` (0x12E3C) loads the camera x again and folds the stack copies

Two differences, one commit:

1. 0x12F34 `mov ebx,[0xf0af0]` loads the camera x after the split's two 0x18714 calls (0x12EEC
   stored EBX there before them). Old: the entry value kept. New: loaded again at the end of the
   split block.
2. The dead-zone fold reads the two stack copies of +0x34 (0x12F4D `mov edx,[esp+eax+8]`; written
   at 0x12E9B / 0x12EB5 and with each latch at 0x12EFE / 0x12F1E). Old: the slots' +0x34 fields
   read again. New: the locals `la` / `lb` (index 0 is slot 0's).

Measured on the old port with `camera_mode_track_pair_rr`: `v0: byte 0xF0AF0: original 0x80, port
unchanged` (v1..v3 were re-valued after that measurement, see §4). Mutants: `@ccam` (1 alone), `@m34` (2
alone), `@cache` (both: the old port). Ordinary play: no difference seen (0x18714's head
0x18714..0x1873F writes neither; unverified for its callee 0x18540).

### §3.7 `fighter_frame_flag` (the static copy of 0x3C59C) shifts by the bit modulo 32 (task item 3)

0x3C5A3 `mov cl,al` / 0x3C5AC `shl ebx,cl`. `fighter_pass_flag` masks with 0x1F (its row's case f3
and `@shift`); the static copy masked with 0xFF. New: 0x1F. The static copy has no binding and its
callers pass 1 (0x3B47A `mov eax,1`) and 0 (0x469B9 `xor eax,eax`): the instruction alone.

### §3.8 Wrapping negations (task item 4)

`fighter_382c4` 0x3837B `neg eax`, `camera_y_clamp`'s inlined 0x12CD4 (0x12CDF `sub eax,edx`,
0x12CE7 `neg ecx`), `fight_384f8` 0x38595 `neg edx`, `fight_4aad0` 0x4ABDF / 0x4ABED: each is
`(s32)(0u - (u32)x)` now. The rows `fighter_382c4`, `camera_y_clamp`, `fight_384f8`,
`fight_384f8_chain`, `fight_4aad0` re-run unchanged; whether one of their cases reaches INT_MIN was
not checked case by case (d3b's x8 does for `fight_4aad0`).

### §3.9 Stale text (task item 5; no behaviour)

- `flow_26d4c`'s header: the format is the nine bytes at 0x809AC, `%s%.03d%` and a NUL (pushed at
  0x26EF3), not `%s%.03d%%`.
- `fighter_47b04`'s header: state 1 hands 0x39834 the +0x5F of the side's own fighter slot (0x47BB0
  `mov dl,[eax*4+0x10780f]`, EAX = side * 0x25), not "this slot's".
- `tools/diff_verify.py`, the note on the `ANIM54` stub: 0x36870's body saves EBX, ECX and EDX and
  writes no other register; `("esi", "ebp")` is the conservative set of `E._CALLEE_CLOBBER_FIXES`.

### The `port/src` edits that are not corrections

Seams (first statement; the rows that reach each were re-run, unchanged):

- `0x1D74C` `fight_cmd_bits_30` (fight.c): `PR_SEAM_RET(0x1D74Cu, side);` (rows `fight_cmd_bits_30`,
  `fight_hud_meter_step`).
- `0x1A570` `camera_actor_bit15_clear` (camera.c's static copy): `PR_SEAM_RET(0x1A570u, side);`
  (rows `camera_box_palette`, `camera_page_tail_a`, `camera_page_tail_b`; the others by the full
  self-check).

## §4 Named gaps, what could not be separated, unhit blocks

- **`fighter_think_side` (0x3B464), the +0x64 re-read at 0x3B4B1 / 0x3B4B9: cannot be separated,
  port unchanged.** Between the first read (0x3B48C / 0x3B490) and the second the only code run is
  0x3AFC4 (0x3B4AC). Its bytes 0x3AFC4..0x3B037 store through EBX alone (`mov [ebx],edx` 0x3B010,
  `[ebx+4]` 0x3B025, `[ebx+8]` 0x3B033), and EBX is `lea ebx,[esp+0x24]` (0x3B4A5), the caller's
  frame; its other arm is the fatal 0x62003 (stance byte 0x40 or more), which the row cannot run.
  It cannot be a stub either: the port hands `fighter_anim_triple` a C stack array (no seam,
  record E3 §E3.10 item 6). So no callee can change the byte between the two reads.
- **`fighter_think` (0x1975C): no row; the two AL corrections rest on the instructions alone.** A
  row needs 0x3962C / 0x396AC as stubs whose AL is 0 under a non-zero EAX, and then the loop goes
  on; the raw keeps ESI = 2 live across both calls (0x1976D; read at 0x19780 and 0x197FD `mov
  eax,esi`), while the declared clobbers of both stubs (d3c's `D3C_3962C` / `D3C_396AC`, from
  `E.callee_clobbers`) include ESI, so the original would run on the poison. The scan's `esi, edi,
  ebp` comes from 0x3AFC4's direct target 0x62003 (the runtime's fatal exit, taken to clobber
  everything): 0x3962C's own body pushes EBX only and 0x396AC's EBX and ECX. A
  `_CALLEE_CLOBBER_FIXES` entry for both (the form of d3c's 0x46C78 fix) would open the row; it
  changes d3c's constants and two lines of the pinned stub dictionary, which this batch did not
  touch. The row would also need seams on 0x17CB0, 0x3B464 and 0x3B938.
- **`camera_project` (0x17FA0): no row**; its AL correction (0x181A0) rests on the instruction.
- `fighter_reaction_al`: 0x3B75D (the `local_24 == 0xFF` fatal, the raw's 0x62003) stays named
  unhit, as in C2b's row.

Limits worth knowing:

- `_al` specs reuse the first row's pokes; what steered the real predicate there (the pose and
  state bytes for 0x14590, the command words for 0x1D74C, the pset bit for 0x1A570) is seeded and
  not read.
- `fight_hud_meter_step_al`: one stub EAX per case serves both sides. The stub's argument (the
  side) is in the call list; no mutant passes the other side (d3d's core has none).
- `camera_page_tail_*_al`: of d2d's thirteen shared cases only t0 and t1 reach the scan's 0x1A570;
  u0 / u1 are t0's state on side 1.
- `fight_4aad0_rr2`: the 0x2BE00 stub makes both its argument-relative writes at every call; the
  stray one lands at 0x1087B4 (called on D2B_ACT1) or 0x108934 (called on D2B_REC0), in no field
  the function reads; a mutant that hands 0x2BE00 another record moves both, which shows as `byte`
  differences. The loads at 0x4AAEF (the first) and the +0x21 byte (0x4AB5D, 0x4AB87) have no call
  before them that a stub could use.
- `fight_4b5a8_rr`: the loads between 0x2BC30 and 0x2BE00 (`[ebx+8]` three times, `[ebx+0xc]`
  twice) form one group, as do the two after 0x2BE4C: no call lies between the members.
- `fight_4b470_rr`: the load at 0x4B52B has no call between it and the preceding one.
- `camera_mode_track_pair_rr`: where both folded copies lie outside the 0x1800 zone the midpoint is
  `(L0 + L1) / 2` whatever the camera x, so v0..v3 each keep one copy inside the zone for one of
  the two camera values. The stub writes both slots' +0x34 at every call, so the second block's
  field read (0x12F0C) sees the stub's value on both sides.

## §5 Store sweep (the new row)

| row | stores | pre-state / neighbours | store mutants caught |
|---|---|---|---|
| `fighter_14814` | the hold arm: slot +0x52 = 9, +0x53 = 4, +0x54 = 0 (after the 0x3C4CC call); else: ctx[3]'s +0x68 decremented (0x14590 holding), slot +0x57 = 0, +0x52 = 9, +0x53 = 7, +0x54 = 0, +0x0C / +0x18 / +0x1C = 0x1461C / 0x145CC / 0x145E4, +0x40 \|= 0x48000, rec +0x55 = 0, the dword 0xFD108[side] = 0x2000 / 0x1000, the byte 0xFD11C[side] = 0 | both slots and the EAX slot seeded whole (d2d's fixture: no byte 0, none equal to what is written), rec +0x50..+0x56, 0xFD0FC..0xFD113 and 0xFD118..0xFD11D seeded | `@nodec`, `@s57`, `@cb0c`, `@r55`, `@noflag` (dropped); `@a53`, `@b53`, `@b54`, `@a54`, `@cbswap`, `@orbit`, `@dec`, `@fdidx`, `@flagside`, `@slotctx` (value, field or slot); `@aorder`, `@early` (a store on the other side of the 0x3C4CC call: call memory alone) |

The second specs add no store: every store of their functions is covered by the first row's sweep.

## §6 Task items not done

- Item 6, a row for `game_mode_09_step` (0x28788): not attempted. The entry has 30 block leaders
  and about twenty direct callees up to its first `ret` (0x3C5CC, 0x16D58, 0x17FA0 with stack
  arguments, 0x17580, 0x1958C, 0x3CB68, 0x35658, 0x19068, 0x49C78, 0x12DA8, 0x4A708, 0x28130,
  0x286BC, 0x32B00, 0x32A3C, 0x32BAC), most of which would need a seam. Skipped for its size
  against the session's remaining budget, not for a blocker.

## §7 Leads seen and not acted on (outside the task list; unverified)

- `camera_project` (0x17FA0): 0x1818D `mov ecx,[eax+2]` / 0x18193 `sar ecx,0x10` and 0x18190 `mov
  edx,[eax+4]` / 0x18198 `sar edx,0x10` are arithmetic shifts (the signed words at sprite +4 and
  +6); the port writes `(s32)((u32)DSD(sprite + 2u) >> 16)` and the same at +4, logical shifts. The
  two differ by 0x10000 in `out_a` / `out_b` when the word is negative, i.e. only above bit 15 of
  the result. No row, not changed, not measured.
- The pinned dictionary of `test_each_stub_declares_the_registers_its_callee_clobbers` has the key
  0x2C060 twice (d3a's `()` and d3b's `("edx",)`; the later wins in a dict literal), and
  `D3A_READY` / `D3B_CREDIT` declare those two different sets for the one callee.
