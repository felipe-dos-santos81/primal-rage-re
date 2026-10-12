# Track D, wave 4, batch d4c — derivation record

Differential rows for twelve of the thirteen queue entries of `d4c` (`fighter.c`, `fighter.h`, `fight.c`).
Branch `reverse-d4c` off `94898999` (`reverse-d3int`, waves 1 to 3 merged). Ground truth: the fixed-up
image `build/diffrun --image-out /tmp/pr_d4c_img.bin` dumps (linear addresses), disassembled with capstone.
Every figure in §2 is measured with `python3 tools/diff_verify.py --image /tmp/pr_d4c_img.bin --function
<row> --self-check`; the pins (`D4C_MASKS`/`D4C_KINDS`/`D4C_CASES` in `tools/tests/test_diff_verify.py`)
come from `verify_all` on the row's own spec. Each row and each of its mutant bindings was run ten times in
a row through `build/diffrun` (identical output, exit 0 every time). The claim of each row is the track's:
equivalence on the exercised blocks and inputs only, with the callees stubbed or run as stated. Every
correction of §3 is unproven against the capture oracles until the integrator's gate.

Each mutant set is a restatement of the function in `port/tests/diff_runner.c` (`d4c_<addr>_core`) with one
choice flipped per mutant number. While it was written each core was also bound unflipped and measured
`VERIFIED` on its row's cases (all but `fighter_spawn`'s ten-line core), so the restatement is the
function; that binding is not kept.

## §1 The members

The mask is the part of EAX a caller reads. "Void" rows are mask 0 with the instructions that write EAX
before any read on each path after each call site.

| addr | port function | size | callers and what they read of EAX |
|---|---|---|---|
| `0x3C88C` | `hit_slot_step` (fighter.c) | 730 | One site, `0x3CB96` (in `0x3CB68`): `0x3CB9B mov edx,[0x107ed8]`, then the loop's `mov eax,[0x107edc]` (`0x3CB79`) or the return of `0x3CB68`, whose row has mask 0. Mask 0. |
| `0x1958C` | `fighter_pass_a` (fighter.c) | 464 | Seven sites: `xor eax,eax` at `0x262F3`, `0x26482`, `0x265DF`, `0x2747D`, `0x29AE5`; `0x284F8 call 0x3cb68` (loads EAX at `0x3CB79`) then `0x284FD xor eax,eax`; `0x28815` reaches `0x2882C xor eax,eax`, through `0x28827 call 0x3cb68` or not. Mask 0. |
| `0x33B00` | `fighter_state_33b00` (fighter.c) | 279 | `0x22CB8 mov eax,[esp+4]`; `0x2799F` loops (`0x27998 mov eax,ecx`) to `0x279AE mov eax,[0x10810a]`; `0x34D58` loops (`0x34D4F mov eax,ecx`) and returns through `0x34B6C` to `0x35801 mov eax,esi`; the fourth site `0x272BF` is in `0x272A8`, which no `call`/`jmp rel32` and no dword of the image names (Ghidra lists no function there). Mask 0. |
| `0x33C78` | `fighter_spawn_slot` (fighter.c; *static* before this batch) | 571 | Nine sites: `mov eax,[..+0x1077b0]` at `0x15AE4`, `0x24618`, `0x248B4`, `0x40D63`, `0x40F14`, `0x46098`, `0x49200`; `0x33EE5` returns through `0x33EB4` (next row); `0x357DB` jumps to `0x3582E..0x35835`, the return of `0x35658`, no call site of which reads EAX first (record 2026-10-11-reverse-d1a §1, re-used, not re-walked). Mask 0. |
| `0x33EB4` | `fighter_spawn` (fighter.c) | 53 | Eight sites: `0x11ADE mov eax,6`; `0x11B0D mov ah,1` (stored to `0x104B15`) then `0x11B1A xor eax,eax`; `0x25AB1`/`0x25ABD mov eax,1`; `0x25BF2 mov eax,1`; `0x25BFC xor eax,eax`; `0x269F2 lea eax,[edx*8]`; `0x270F2`, `0x295F6 mov eax,0x25`. Mask 0. |
| `0x47208` | `fighter_command_generate` (fighter.c) | 345 | One site, `0x24CA1`: the loop's `0x24C9F mov eax,ebx` (or its `0x24C94` arm, no EAX), then `0x24CB5 call 0x461dc`, which loads EAX (`0x461F8 mov eax,2`, `0x46203`/`0x46211 mov eax,[0x101514]`) before reading it. Mask 0. |
| `0x43D60` | `fight_char_confirm` (fight.c) | 319 | `0x43AD8` loops to `0x43AC5 mov al,[edx+0x108170]` (AL alone is read) or reaches `0x43AE3 mov eax,0x11`; `0x43CC6` loops to `0x43B2B mov al,..` or reaches `0x43CE1 xor ah,ah` .. `0x43CF0 mov ax,[0xef6dc]` .. `0x43CFA and eax,0xffff`. Mask 0. |
| `0x43AAC` | `fight_char_countdown` (fight.c) | 119 | One site, `0x43D01`, which returns at once (`0x43D06..0x43D09`). The function sets AL (`mov al,1` `0x43AF3`, `xor al,al` `0x43B1D`); the row compares AL: mask 0xFF. |
| `0x4434C` | `fight_char_team_pick` (fight.c) | 377 | One site, `0x44908` (in `0x44798`): the loop head `0x4479F mov al,..` and every later read is of AL or AX, or follows `and eax,0xff` (`0x447C5`, `0x447DD`) or `mov eax,edx`; past the loop `0x44928 xor ah,ah` and the return of `0x44798` (below). Mask 0. |
| `0x44638` | `fight_char_team_text_blink` (fight.c) | 350 | One site, `0x447BE`, which jumps to `0x44833 mov ax,..; xor ah,ah; and al,0xf0; mov cx,ax` .. `cmp ax,..` (AX alone), then `0x448E6 mov eax,edx` or `0x448F4 mov ax,..` .. `0x448FF and eax,0xffff`. Mask 0. |
| `0x44798` | `fight_char_team_pass` (fight.c) | 419 | One site, `0x438CD`, returning through `0x438D2 pop edx; ret` to `0x2538A jmp 0x2540f`, where `0x2A31C` writes EAX before reading it (the note of the existing `0x25210` row in `tools/diff_verify.py`; re-used, not re-walked). Mask 0. |
| `0x4EBB8` | `fight_mode25_scorecard` (fight.c) | 979 | `0x4E92D mov dl,..; mov ebx,2; mov eax,edx` (`0x4E938`); `0x4F073 mov al,..; mov ah,..; cmp al,ah` then `0x4F082 mov eax,0x63` or `0x4F0A9 setbe al; test al,al` and `mov eax,0x65`/`0x66` (`0x4F0B0`, `0x4F0B7`). Mask 0. |
| `0x34B6C` | (fight.c) | 541 | not rowed (§6). |

## §2 The row table (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `hit_slot_step` | 0x3C88C | 29 | 33/33 | 43/43 | 0 | 0x3C600 a stub (the descriptor), 0x3C6A8 recorded and run (`mode="real"`: the raw reloads the phase it stores), 0x3C758/0x3C800/0x3C6E8 run on both sides (their accumulators are the caller's stack dwords). Reads 0xC+2 outside the image in case a0 (0x3C6A8 with a null descriptor), pinned in `P45_OUTSIDE`. |
| `fighter_pass_a` | 0x1958C | 36 | 41/41 | 43/43 | 0 | 0x19020 and 0x18950 recorded and run, 0x18C14 (under the registered hook 0x3E484), 0x5D7DC and 0x193B0 stubs; 0x33950, 0x3AFC4, 0x18BD4 run on both sides. |
| `fighter_pass_a_al` | 0x1958C | 35 | 41/41 | 5/5 | 0 | the same entry with 0x18950 a stub (AL of both queries). |
| `fighter_state_33b00` | 0x33B00 | 7 | 6/6 | 25/25 | 0 | 0x36E78 and 0x29BC8 stubs; 0x2EA30 runs on the original side (the lock byte 0xBCD60 is 0, it stores it back unchanged). |
| `fighter_spawn_slot` | 0x33C78 | 20 | 22/22 | 82/82 | 0 | 0x2AE14, 0x186D0, 0x29BC8, 0x494A8 stubs; 0x1CEBC and 0x1B544 run on both sides. |
| `fighter_spawn` | 0x33EB4 | 4 | 4/4 | 10/10 | 0 | 0x33C78 a stub. |
| `fighter_command_generate` | 0x47208 | 19 | 17/17 | 42/42 | 0 | 0x2D974, 0x2CAA8, 0x2F510, 0x469A8, 0x470F8 (which rewrites side 0's move index), 0x1A570 stubs; 0x33950, 0x3C6E8 and the string path 0x1C500 run on both sides. |
| `fight_char_confirm` | 0x43D60 | 14 | 16/16 | 37/37 | 0 | 0x43D0C, 0x33C18, 0x2B150, 0x432EC, 0x2C3FC stubs; 0x4248C runs on both sides; 0x2E934/0x2E180 run on the original side only (§4). |
| `fight_char_countdown` | 0x43AAC | 8 | 8/8 | 20/20 | 0xFF | 0x2F528 and 0x43D60 stubs; 0x4F980 runs on both sides. |
| `fight_char_team_pick` | 0x4434C | 12 | 23/23 | 34/34 | 0 | 0x44054, 0x4408C, 0x2B150, 0x432EC, 0x33C18, 0x2C3FC stubs; 0x4248C runs on both sides. |
| `fight_char_team_text_blink` | 0x44638 | 10 | 11/11 | 34/34 | 0 | 0x432EC, 0x2AE14, 0x2F198 stubs; the string path and the runtime strlen 0x65624 run on both sides. d3b's 0x43464 fixture (`d3b_blink_case`). |
| `fight_char_team_text_blink_rr` | 0x44638 | 6 | 9/11 | 1/1 | 0 | the same entry; the cursor stub rewrites both col words 0xC894C (a harness write). Two blocks named (§4). |
| `fight_char_team_pass` | 0x44798 | 15 | 35/36 | 40/40 | 0 | 0x44638, 0x4418C, 0x442A0, 0x4434C stubs; 0x4F980 runs on both sides. One dead block named (§4). |
| `fight_mode25_scorecard` | 0x4EBB8 | 5 | 24/24 | 33/33 | 0 | 0x2F510, 0x2F174, 0x2F4D0 stubs; the string path runs on both sides. Four cases poke the grid table down to three cells; r0 runs the image's 37. |

14 rows on 12 entries; 449 mutants.

## §3 Corrections (each address-proved, each in its own commit)

| # | function | raw | old | new |
|---|---|---|---|---|
| 1 | `fighter_pass_a` (0x1958C) | `0x196C9 cmp ax,dx` / `0x196CC jge 0x196d7`; `0x196D7 cmp dx,ax` / `0x196DA jge 0x196e6` | the two slots' +0x88 words compared unsigned (`DSW < DSW`) | compared signed (`(s16)`). 0x8000 against 1: the raw clears DS_00100AFC, the port cleared DS_00100AF8. Cases c4, c5; mutant `@w88uns`. |
| 2 | `fighter_spawn_slot` (0x33C78) | `0x33CBB xor ecx,ecx` / `0x33CBD mov cx,[esp+0x14]` / `0x33CC2 push ecx` | the stack argument passed whole to 0x2AE14 (each port caller masked it) | `a5 & 0xFFFF` in the function. No port caller passes a non-zero high half. Case w0; mutant `@a5w`. |
| 3 | `fighter_spawn_slot` (0x33C78) | `0x33DCC cmp eax,edx` / `0x33DCE jl 0x33dd2` | the quotient clamped unsigned (`q >= cap`) | signed. The two differ only for a quotient of 0x80000000 or more, which the image's divisor 0xC350 (`0xC9520`) cannot produce; case d0 pokes the divisor to 1. Fidelity to the raw, not a behaviour of the shipped game. Mutant `@capuns`. |
| 4 | `fighter_command_generate` (0x47208) | `0x4721B mov eax,0x29` / `0x47220 call 0x2d974` / `0x47225 test ah,8` / `0x4722E call 0x2caa8` / `0x47233 test eax,eax`; `0x47241..0x4725C` (0x1C500(0x51), 0x2F510(0xA, 7, the string, 0x4000)); `0x47261..0x47283` | the whole block absent (the header named it a gap, inert while FREE PLAY is off) | ported: bit 11 of the field 0x29 and a zero 0x2CAA8 draw the string 0x51 and, the two slots' +0x63 differing and the side's own non-zero, clear the dword 0x1082C8[side]. Every case (the 0x2D974 call), k2..k4, k18 (the block); mutants `@nodip`, `@notext`, `@noc8` and nine more on its parts. |
| 5 | `fighter_command_generate` (0x47208) | `0x472ED call 0x1a570` / `0x472F2 and eax,0xff` / `0x472F7 call 0x3c6e8` | the predicate's C int passed whole to `fighter_cmd_facing` (which tests its whole argument, `0x3C6F0 test ebx,ebx`) | `(u8)`. The port's `fighter_actor_bit15_clear` returns 0 or 1, so no call changes. Case k13 (stub EAX 0x100: the raw maps facing 0, 0x10; the port mapped 0x20); mutant `@facingeax`. |
| 6 | `fight_char_team_text_blink` (0x44638) | `0x446C9 mov bx,[esi*2+0xc894c]`, `0x446F2` the same, `0x4471B movzx ebp,word [esi*2+0xc894c]`, `0x4474E mov si,[esi*2+0xc894c]`, each after that string's 0x1C500 | the col word read once into a local before the first string | read before each `text_cursor_set`. d3b's finding for the twins 0x43464/0x435AC (record 2026-10-12-reverse-d3b §3.7/§3.8). Measured on the `_rr` row (`y2: call #1: original 0x2F198(0x77, 0x1A, ..), port 0x2F198(0x8003, 0x1A, ..)`); mutant `@cache`. |
| 7 | `fight_mode25_scorecard` (0x4EBB8) | `0x4EBC1 mov eax,0x62` / `0x4EBC6 mov ecx,0x4000` / `0x4EBCB mov edx,6` / `0x4EBD0 call 0x1c500` / `0x4EBD5 mov ebx,eax` / `0x4EBD7 mov eax,0xffffffff` / `0x4EBDC call 0x2f510` | absent: the port began at the glyph grid | `text_cursor_hold_font2(-1, 6, game_string_get(0x62), 0x4000)` first. Every case (`r0: call #0: original 0x2F510(0xFFFFFFFF, 0x6, 0x44524143, 0x4000), port 0x2F174(..)`); mutant `@notitle`. |
| 8 | `fight_mode25_scorecard` (0x4EBB8) | `0x4EE8C push eax` .. `0x4EE94 movzx ebp,word [esp+0xc]` / `0x4EE9E inc ebp`; `0x4EF05 push eax` .. `0x4EF0F movzx ebp,word [esp+0xc]` / `0x4EF19 inc ebp` | the row of the second pair's digits (cols 0x14/0x15) and of the tally's digits (cols 0x17/0x18) was the tally + 1 | the side's row (`row0 + 1`). After the one `push`, `[esp+0xc]` is the frame's `[esp+8]`: EDI saved at `0x4EC7C`, not the tally word. Measured: `r0: call #40: original 0x2F4D0(0x17, 0x7, 0x5, ..), port 0x2F4D0(0x17, 0x38, 0x5, ..)`. Cases r0, r2, r3; mutants `@row2acc`, `@row3acc`. The header comment that stated the old reading is corrected with the code. |

Corrections 1, 4, 7 and 8 change what the port does on reachable states (8 moves digits the mode-0x25 card
draws; 4 and 7 add draws; 1 changes the winner pick when a +0x88 word has bit 15). 2, 3, 5 and 6 are
fidelity to the raw with no change on the states the port's own callers and callees produce.

## §4 Named gaps and unhit blocks, and what the rows do not cover

- **`fighter_spawn_slot`, the sound-bank tail (0x33E48..0x33EA6).** AL of 0x1CEBC (`test al,al` 0x33E4D)
  opens it; the character's handle through the table 0x33C5C and the fixed 0x287B2F5 go to 0x1B544. Both
  run on both sides (0x1B544 against preloaded INDEX entries, the fast path at 0x1B569) and the resolve
  stores nothing, so the row hits the original's blocks there (c0..c7, n2) and compares **nothing** of
  what the port resolves: no mutant on the tail is possible. The port tests the C `int` of
  `fighter_spawn_audio_gate` whole where the raw tests AL; that function returns 0 or 1 and a stub's EAX
  could not be observed either, so it is left as it is (not a correction: no case can show it).
- **`fighter_spawn_slot`, the two records.** The two 0x2AE14 stubs return one EAX per case, so the record
  and the child record are one address: a store to the wrong one of the two (the child's +0x59, the
  record's +0x14/+0x51/+0x4C/+0x4D) is not separable. Running 0x2AE14 real needs the pool fixture of
  `d1f_tag_case`; not done.
- **`fighter_spawn_slot`, `q == cap`.** `jl` keeps q below the cap and stores the cap otherwise; at
  equality both give the cap, so `>=` against `>` is not observable (q2 is the equal case).
- **`hit_slot_step`.** The phase's sign extension shows only at the first test (n0); every reload follows
  a 0x3C6A8 store of 0..8. 0x3C800's AL (`test al,al` 0x3CA99) is not stubbed (the callee runs on both
  sides and the port's returns 0 or 1), so no AL-against-EAX case exists for it.
- **`fighter_pass_a`.** With 0x18950 run real the port's C returns 0 or 1, so AL against EAX is the
  `_al` row's; there one stub EAX serves both queries, so only a whole-EAX test of *one* of the two is a
  mutant (`@eaxbl`, `@eaxal`): testing both whole gives the same verdicts. In the tail at most one flag is
  non-zero (the compare section always clears one), so `else if` against two `if`s is not observable.
- **`fight_char_confirm`, the audit post.** `0x43E69..0x43E77` calls the deferred 0x2E934(2, the signed
  class) (`tools/port_classification.txt`: audit no-op), which the port omits under a `/* PORT: */`
  comment. It runs on the original side only, and its 0x2E180(2, bucket, 1) stores nothing because the
  cases poke the counter's bucket count, the word 0x2D46E, to 0 (`0x2E1A1 mov si,[eax+2]`, `0x2E1A5 cmp
  edx,esi; jb`; the image has 7). The row does not compare that call or its arguments.
- **`fight_char_countdown`.** The value drawn is always above 0 (the other arm takes the rest), so its
  sign extension (`sar ebx,0x10` 0x43B15) is not observable.
- **`fight_char_team_text_blink_rr`**: blocks `0x44774` and `0x44779` (the non-zero phases) are the
  `fight_char_team_text_blink` row's (b6..b9).
- **`fight_char_team_pass`**: block `0x44874` (`jmp 0x448e6` after `0x4486E cmp ax,0x10; je`) is dead: a
  non-zero value & 0xF0 below 0x20 is 0x10.
- **`fight_mode25_scorecard`.** Five cases: the three arms of each pair, the boundaries 10 on each
  (`@a10`, `@ab10`, `@c10`, `@cd10`), 0x1088BD at 6, 7 and 0x80. The tally is a word in the raw and the
  cases keep it below 0x10000.

No row is a named gap (`gap=`), and no function of the queue was found unrunnable.

## §5 Store-sweep findings

Every field each function writes is seeded with a value different from the one written, with seeded
neighbours; one dropped store per field is a mutant.

| row | stores | seeded with | dropped-store mutants |
|---|---|---|---|
| `hit_slot_step` | the countdown word (dec, displacement), the phase word (= 8), the accumulator word | the three 2 x 32 word arrays whole, each word distinct | `@p8nodec`, `@nodec`, `@p0noarm`, `@c0noarm`, `@nodisp`, `@c3none`; widths `@p8decd`, `@p0armd`, `@dispd`, `@c3d` |
| `fighter_pass_a` | the two flag dwords | a zero low word in each (a word test misses them), between two dwords | `@no53`, `@noanimclr`, `@norng`; `@53w`, `@flagw` |
| `fighter_state_33b00` | the slot (0x94), the record (0x68), +0x5A, +0x5D, +0x64, +0x08 | five byte patterns, every byte odd | `@nocopy1/2`, `@len1/2`, `@nof0/1`, `@no5a`, `@no5d`, `@no64`, `@nop8` |
| `fighter_spawn_slot` | 35 stores (slot, record, DS_001077A0/A8, 0x1078FA, 0xF0AFE/F) | both slots whole (every byte odd), the records' +0x10..+0x17 and +0x48..+0x5B | a `@no..` mutant per store, widths `@incw`, `@40w`, `@08w`, `@6ab`, `@88b` |
| `fighter_command_generate` | the command word, 0x1082C8[side], the block's +0x04, +0x0C, +0x30, +0x31 | both AI blocks whole, the words and dwords between neighbours | `@nozero`, `@noc8`, `@noterm04`, `@notermzero`, `@noadv30`, `@noadv31`, `@noinc31`, `@noinc0c`; `@zerod`, `@c8w`, `@inc0cw` |
| `fight_char_confirm` / `_team_pick` | the class, state, button, confirmed-class and pick bytes, the slot's +0x63, the entry dword | the select bytes 0x108164..0x108173 whole, the picks between four bytes on each end | `@no6a`, `@no70`, `@no63`, `@no34`, `@noclash`, `@no6e`; `@noff`, `@nostore`, `@no2`, `@no3`, `@nozero` |
| `fight_char_countdown` / `_team_pass` | the countdown word, the hook dword, the cursors, the class and button bytes, 0x105C04 | each between its neighbours; the countdown's high byte non-zero | `@nodec`, `@nohook`; `@no6a`, `@no34`, `@no6b`, `@no35`, `@nocd`, `@noc04clr`; `@decd`, `@cdb` |
| `fight_char_team_text_blink` | DS_00108144[side] | d3b's fixture | `@nostore`, `@storeside` |
| `fight_mode25_scorecard` | the side's fifth tally byte | 0xEE, between the other side's bytes | `@nostore`, `@storeside` |

The call memory at each recorded call caught the order mutants (`@order`, `@early`, `@latchearly`,
`@1clate`, `@dustearly`, `@clsearly`, `@zeroearly`).

## §6 Not done

- **`0x34B6C`** (541 bytes, level 1; 21 callees, the state dispatch over 0x33B00, 0x349C8, 0x359E0, ...):
  not started beyond its EAX trace (`0x35801 mov eax,esi` after its one call site `0x357FC`). No row, no
  claim. `0x33B00` has no seam yet (its row needed none); a row of `0x34B6C` that stubs it adds
  `PR_SEAM(0x33B00u, side, src, dst2)`.

## §7 Port changes other than corrections

Seams (each the callee's first statement):

| addr | function | line |
|---|---|---|
| `0x18950` | `fighter_connect_query` (fighter.c) | `PR_SEAM_RET(0x18950u, param_1, param_2);` |
| `0x19020` | `fighter_19020` (fighter.c) | `PR_SEAM(0x19020u, side);` |
| `0x193B0` | `fighter_winner_body` (fighter.c) | `PR_SEAM(0x193B0u, side);` |
| `0x33C78` | `fighter_spawn_slot` (fighter.c) | `PR_SEAM(0x33C78u, side, a2, a3, a4, a5);` |
| `0x469A8` | `ai_classify` (fighter.c) | `PR_SEAM(0x469A8u, param);` |
| `0x470F8` | `ai_manage` (fighter.c) | `PR_SEAM(0x470F8u, side);` |
| `0x494A8` | `fight_dust_build` (fight.c) | `PR_SEAM(0x494A8u, side);` |
| `0x43D0C` | `fight_char_button` (fight.c) | `PR_SEAM_RET(0x43D0Cu, side);` |
| `0x43D60` | `fight_char_confirm` (fight.c) | `PR_SEAM(0x43D60u, side);` |
| `0x44638` | `fight_char_team_text_blink` (fight.c) | `PR_SEAM(0x44638u, side);` |
| `0x4418C` | `fight_char_team_portrait` (fight.c) | `PR_SEAM(0x4418Cu, side);` |
| `0x442A0` | `fight_char_team_fighter` (fight.c) | `PR_SEAM(0x442A0u, side);` |
| `0x44054` | `fight_char_team_tag_drop` (fight.c) | `PR_SEAM(0x44054u, side, slot);` |
| `0x4408C` | `fight_char_team_tag_add` (fight.c) | `PR_SEAM(0x4408Cu, side, slot);` |
| `0x4434C` | `fight_char_team_pick` (fight.c) | `PR_SEAM(0x4434Cu, side);` |

The existing rows of the seamed callees were re-run after each seam and are unchanged
(`fighter_connect_query`, `fighter_19020`, `fighter_winner_body`, `fight_dust_build`, `ai_classify`,
`ai_manage`, `fight_char_button`, `fight_char_team_tag_drop`, `fight_char_team_tag_add`,
`fight_char_team_portrait`, `fight_char_team_fighter`).

Export (linkage only): `fighter_spawn_slot` loses `static` and is declared in a `/* PORT: */` block of
`port/src/game/fighter.h`.

Harness lines outside the batch's own blocks: one group of lines added inside the expected dictionary of
`test_each_stub_declares_the_registers_its_callee_clobbers` (`tools/tests/test_diff_verify.py`) for the
fifteen new stub addresses: `0x18950 ("edx",)`, `0x193B0 ("ebx","esi","edi","ebp")`, `0x494A8 ()`,
`0x33C78 ("ebx","ecx","edx","ebp")`, `0x2D974 ()`, `0x469A8 ()`, `0x470F8 ()`, `0x43D0C ()`, `0x43D60
("esi","edi","ebp")`, `0x44054 ("edx","edi","ebp")`, `0x4408C ("edx",)`, `0x44638 ()`, `0x4418C ()`,
`0x442A0 ()`, `0x4434C ()`, each `E.callee_clobbers` of the image. No `tools/diff_emu.py` change.
