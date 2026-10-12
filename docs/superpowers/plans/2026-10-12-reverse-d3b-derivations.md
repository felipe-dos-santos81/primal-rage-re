# Track D, wave 3, batch d3b — derivations (game/fight.c, fight.h)

Differential rows for the functions of `.superpowers/sdd/2026-10-11-reverse-d1/d3b-queue.txt`, worked top
to bottom. Ground truth: the fixed-up image `build/diffrun --image-out` writes (linear addresses),
disassembled with capstone. Every figure below was measured with
`python3 tools/diff_verify.py --image <image> --function <name> --self-check`, one row at a time; each
row's and each mutant's port side was run ten times and gave the same output each time before its pin
was written. The full `--self-check` was run once on the final tree (`PR_DIFF_JOBS=4`): `678/678 functions
VERIFIED; 6289/6289 mutants detected; 1 named gaps; 442/511 rows with callees closed (167 have none)`,
rc 0 (the base tree's 652 / 5650 plus this batch's 26 specs and 639 mutants). The unittest suite, `make
verify`, `make diff-verify` and the oracles were NOT run on this branch (the integrator runs them on the
merged tree), so every `port/src` correction below is unproven against the oracles.

## §1 Members and the callers that fix each EAX mask

Call sites: a recursive-descent scan of every known entry (the Ghidra export, the E2 live list, closed
over direct call targets), plus an `E8 rel32` byte pattern where it lies outside every scanned
instruction, plus a dword scan of the whole image for a stored pointer to the entry (none found for
any member below).

"walk" is the rule of record d1f §1 / d2b §1, re-implemented for this batch (scratch tool, not
committed): from the return site the code is followed (both arms of every conditional, into direct
callees to depth 4, out through `ret` into the callers of the containing function to depth 7) tracking
which parts of EAX (AL, AH, the high word) still hold a value derived from the function's. An
instruction that only transforms EAX in place (`dec eax`, `and eax,imm`, `movzx eax,al`...) or sets
the flags from it propagates the taint (carries upward only for add/sub/inc/dec/neg/shl/imul/and/or/
xor); any other use of a tainted part — a store, a copy, an address, a conditional jump on tainted
flags, an argument a callee reads — is a read and sets the mask. The tool was checked against nine
masks d2b derived by hand (0x1D2F0 0, 0x493F0 / 0x4A868 / 0x4AA6C / 0x4D150 / 0x4A8A8 0xFFFFFFFF,
0x4B5A8 0xFF): all equal.

| addr | port function | size | call sites | what the callers read of EAX | mask |
|---|---|---|---|---|---|
| 0x1D764 | `fight_hud_side_reset` | 82 | 0x27752, 0x279B6, 0x27F01, 0x298DE | 0x27757 `xor eax,eax`; 0x298E5 `xor eax,eax`; 0x279BB.. and 0x27F06..: walk, dead on every path | 0 |
| 0x1D890 | `fight_hud_spawn` | 375 | 0x11B1E, 0x25AC2, 0x25C46 | walk, dead on every path (0x25AC7 `call 0x4f37c` loads EAX before any read) | 0 |
| 0x1DAE8 | `fight_hud_spawn_side` | 290 | 0x26AA1 | 0x26AA6 `mov al,1`, then 0x26AAF `call 0x20ef8` (its EAX dead, d2b §1) and 0x26AB4 `mov ah,4`: walk, dead | 0 |
| 0x1DC6C | `fight_hud_spawn_b` | 391 | 0x25AB6, 0x25C3A | 0x25ABB `jmp 0x25acc` / 0x25C3F `jmp 0x25c4b`: walk, dead on every path | 0 |
| 0x432EC | `fight_char_text_clear` | 238 | 0x43493, 0x4359E, 0x435E8, 0x43E8C, 0x44455, 0x4466A, 0x44787 | 0x43E91 `mov eax,0x6c`; 0x4445A `lea eax,[ecx*8]`; the others: walk, dead on every path | 0 |
| 0x494A8 | `fight_dust_build` | 515 | 0x33E43 | 0x33E48 `call 0x1cebc`, which loads EAX before any read, then 0x33E4D `test al,al` on its result: walk, dead | 0 |
| 0x4987C | `fight_4987c` | 942 | 0x4A616, 0x4D792 | 0x4A61B.. and 0x4D797 `add esp,4` / the pops / `ret`: walk, dead on every path | 0 |
| 0x4A928 | `fight_4a928` | 323 | 0x4A562 | 0x4A567 `cmp byte [esp+0x18],0`..: walk, dead on every path | 0 |
| 0x4AAD0 | `fight_4aad0` | 326 | 0x49D25 | 0x49D2A `jmp 0x4a468`: walk, dead on every path | 0 |
| 0x4B470 | `fight_4b470` | 312 | 0x4A10B, 0x4B779, 0x4D873 | 0x4A110 `jmp 0x4a468`; 0x4B77E and 0x4D878 `add esp,4` / the pops / `ret`: walk, dead on every path | 0 |
| 0x4C784 | `fight_4c784` | 914 | 0x4C697, 0x4C6A9 | 0x4C69C / 0x4C6AE `jmp 0x4c6b5`: walk, dead on every path | 0 |
| 0x4D224 | `fight_4d224` | 133 | 0x4D3AB | 0x4D3B0 `jmp 0x4d747`: walk, dead on every path | 0 |
| 0x4DEF4 | `fight_effects_idle_pass` | 552 | 0x277E9, 0x2965F | 0x277EE / 0x29664 `call 0x12da8`, then `mov ah,[0x104aec]`: walk, dead on every path | 0 |
| 0x4E11C | `fight_mode25_enter` | 352 | 0x263EA | 0x263EF `pop edx`.. `ret`, then 0x26254's callers: walk, dead on every path | 0 |
| 0x3CB68 | `fight_slot_pass` | 91 | 0x25C8D, 0x26385, 0x26514, 0x26671, 0x26764, 0x26D12, 0x26FDF, 0x274CC, 0x282E2, 0x284F8, 0x28827, 0x2932D, 0x29B34 | walk, dead on every path at each of the thirteen | 0 |
| 0x432A0 | `fight_char_prompt` | 73 | 0x43B85 | walk, dead on every path | 0 |
| 0x43464 | `fight_char_text_blink` | 326 | 0x43BB0 | walk, dead on every path | 0 |
| 0x435AC | `fight_char_opp_text_blink` | 207 | 0x43BA7 | walk, dead on every path | 0 |
| 0x43928 | `fight_char_join` | 59 | 0x438D4, 0x438FA, 0x4494D | walk, dead on every path | 0 |

## §2 The measured rows

`blocks` is hit/total; `mutants` is detected/total.

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fight_hud_side_reset` | 0x1D764 | 4 | 1/1 | 14/14 | 0 | 0x1D2F0 and 0x2BC30 stubs |
| `fight_hud_spawn` | 0x1D890 | 5 | 11/11 | 42/42 | 0 | 0x2AE14 (EAX varied per case), 0x1D2F0, 0x1D464 and 0x1D838 stubs |
| `fight_hud_spawn_b` | 0x1DC6C | 5 | 14/14 | 42/42 | 0 | the same stubs |
| `fight_hud_spawn_side` | 0x1DAE8 | 7 | 9/9 | 29/29 | 0 | 0x2AE14, 0x1DA84 and 0x1D838 stubs; case d5's side byte 0x80 reads the slot byte 0x10C22A and writes 0 at 0x10C20B, both outside the image and inside mem[] |
| `fight_hud_spawn_side_rr` | 0x1DAE8 | 2 | 8/9 | 4/4 | 0 | the same entry; the spawn stub rewrites DS_00104B1A to 1 and the bar stub to 0 (harness writes); 0x1DB82 named unhit |
| `fight_char_text_clear` | 0x432EC | 7 | 9/9 | 27/27 | 0 | 0x2B150, 0x2C088 and 0x2F280 stubs; the string path 0x1C500 / 0x474E4 / 0x1E75C / 0x1E808 / 0x500BB and the runtime strlen 0x65624 run on both sides (allowed; the port's strlen is the host's) |
| `fight_char_text_clear_rr` | 0x432EC | 2 | 9/9 | 1/1 | 0 | the same entry; the release stub rewrites both col words 0xC894C (a harness write) |
| `fight_dust_build` | 0x494A8 | 9 | 16/16 | 48/48 | 0 | 0x4CF20, 0x49388, 0x29CDC, 0x5D7DC and 0x2AE14 stubs; 0x249D0, 0x249B0 and 0x496AC run on both sides (allowed) |
| `fight_4987c` | 0x4987C | 16 | 62/62 | 54/54 | 0 | 0x49388, 0x29CDC, 0x5D7DC, 0x2BE4C, 0x2AE14 and 0x4B144 stubs; 0x2BE00 runs on both sides, recorded (each record its own value); 0x249D0, 0x249B0 and 0x496AC run on both sides (allowed) |
| `fight_4a928` | 0x4A928 | 16 | 12/12 | 28/28 | 0 | 0x4AA6C and 0x2BE00 run on both sides, recorded |
| `fight_4aad0` | 0x4AAD0 | 32 | 27/27 | 28/28 | 0 | 0x4B430, 0x4B3F0, 0x4BD4C, 0x4B5A8, 0x2BE1C and 0x4B144 stubs; 0x2BE00 runs on both sides, recorded |
| `fight_4aad0_rr` | 0x4AAD0 | 4 | 27/27 | 3/3 | 0 | the same entry; the 0x4B430 stub rewrites the side-0 slot's +0x54 to 4, the 0x4B3F0 stub its +0x42 to 1 and the 0x4B5A8 stub its +0x42 to 2 (harness writes) |
| `fight_4b470` | 0x4B470 | 20 | 23/23 | 46/46 | 0 | 0x2C3FC, 0x2BC30, 0x2AE14, 0x1A570, 0x4BD98 and 0x4CB18 stubs |
| `fight_4c784` | 0x4C784 | 8 | 29/29 | 68/68 | 0 | 0x2AE14, 0x2BC30, 0x2B150, 0x2C3FC, 0x4CC0C, 0x2F510, 0x49388, 0x29CDC and 0x2BE4C stubs; 0x2BE00 runs on both sides, recorded; the string path, 0x249D0, 0x249B0 and 0x496AC run on both sides (allowed) |
| `fight_4d224` | 0x4D224 | 12 | 11/11 | 17/17 | 0 | 0x4BD4C, 0x4D108, 0x4D150, 0x4B3F0 and 0x4B430 stubs |
| `fight_4d224_rr` | 0x4D224 | 3 | 11/11 | 2/2 | 0 | the same entry; the 0x4D150 stub rewrites the side-0 slot's +0x42 to 2 and the 0x4B3F0 stub to 1 (harness writes) |
| `fight_effects_idle_pass` | 0x4DEF4 | 8 | 29/29 | 38/38 | 0 | 0x2C3FC, 0x5D7DC, 0x4A868 and 0x2BC30 stubs; the five targets of the jump table 0x4DEE0 each hit |
| `fight_mode25_enter` | 0x4E11C | 12 | 15/15 | 54/54 | 0 | 0x34D8C, 0x38154, 0x2F510 and 0x4E350 stubs; the string path runs on both sides (allowed) |
| `fight_slot_pass` | 0x3CB68 | 3 | 5/5 | 10/10 | 0 | 0x1A570 and 0x3C88C stubs: 2 + 64 recorded calls a case |
| `fight_slot_pass_rr` | 0x3CB68 | 1 | 5/5 | 3/3 | 0 | the same entry; the 0x3C88C stub rewrites DS_00107ED8 to 0x1F and DS_00107EDC to 1 (harness writes): 1 + 1 calls |
| `fight_char_prompt` | 0x432A0 | 6 | 5/5 | 9/9 | 0 | 0x2C060, 0x2C178 and 0x2C1C8 stubs |
| `fight_char_join` | 0x43928 | 6 | 6/6 | 10/10 | 0 | 0x11F28 stub (it preserves EBX and EDX, which the raw keeps live across it) |
| `fight_char_opp_text_blink` | 0x435AC | 8 | 7/7 | 24/24 | 0 | 0x432EC, 0x433DC, 0x2AE14 and 0x2F198 stubs; the string path runs on both sides (allowed) |
| `fight_char_opp_text_blink_rr` | 0x435AC | 4 | 7/7 | 1/1 | 0 | the same entry; the cursor stub rewrites both col words 0xC894C (a harness write) |
| `fight_char_text_blink` | 0x43464 | 10 | 10/10 | 36/36 | 0 | 0x432EC, 0x2AE14 and 0x2F198 stubs; the string path and the runtime strlen 0x65624 run on both sides (allowed) |
| `fight_char_text_blink_rr` | 0x43464 | 4 | 10/10 | 1/1 | 0 | the same entry; the cursor stub rewrites both col words |

26 specs on 19 queue entries, 639 mutants.

The `D3B_MASKS` / `D3B_KINDS` / `D3B_CASES` pins of `tools/tests/test_diff_verify.py` are generated from
these measurements. Every stub the batch declares carries the clobbers `E.callee_clobbers` derives from
the bytes (checked per row by a scratch script). Stubs the pinned dictionary of
`test_each_stub_declares_the_registers_its_callee_clobbers` did not have are added to it as new lines
(no existing line changed): 0x1D2F0 (edx), 0x1D464 (edx), 0x1D838 (ebx, edx), 0x1DA84 (edx),
0x4CF20 (), 0x4B144 (edx), 0x4BD4C (edx), 0x4B5A8 (edx), 0x2BE1C (edx), 0x4BD98 (), 0x4CB18 (ebx, ecx, edx),
0x4CC0C (), 0x4D108 (edx), 0x4D150 (edx), 0x4A868 (), 0x4E350 (), 0x3C88C (), 0x2C060 (edx), 0x2C178 (edx, ebp),
0x2C1C8 (ebx, ecx, edx, esi, edi), 0x11F28 (), 0x432EC (edx), 0x433DC (edx, ebp). The block is the lines
from `# D3 d3b (record 2026-10-12-reverse-d3b): the stubs its rows add` to `0x432EC: ("edx",), 0x433DC:
("edx", "ebp"),`, inserted directly after `self.assertEqual(stubs, {`.

Seven entries are rowed as two specs on one binding (the form d2e used for 0x41C28's states): a `Call`'s
`writes` are fixed per spec, so the cases whose stubs rewrite a global the function reads again
(`_rr`) cannot share a spec with the cases whose stubs write nothing.

## §3 Corrections (port/src)

### §3.1 `fight_hud_spawn_side` (0x1DAE8) reads the side byte again after each call

The raw loads DS_00104B1A six times: 0x1DAF1 `mov cl,[0x104b1a]` (the two zero stores and the first
spawn's a5, 0x1DB1C `test cl,cl`), 0x1DB31 `mov al,[0x104b1a]` (the first spawn's table indexes; no
call between it and 0x1DAF1), **0x1DB61** `mov dl,[0x104b1a]` after the first 0x2AE14 (the index of
the 0x1DB67 store into DS_001028F0 and the EDX of 0x1DA84), **0x1DB75** `cmp byte [0x104b1a],0` after
0x1DA84 (the second spawn's a5) with 0x1DB8E `mov dl,[0x104b1a]` (its position words), and
**0x1DBC0** `mov cl,[0x104b1a]` after the second 0x2AE14 (the indexes of the 0x1DBC8 / 0x1DBCF stores
into DS_001028F8 / DS_001028E0, the slot of the badge byte and 0x1D838's side).

Old: `u32 side = DSB(DS_00104B1A)` once at entry. New: read again at the three bold points.
Measured on the old port with the `_rr` spec: `q0: byte 0x1028F7..0x1028FF` (the DS_001028F8 store
landed on side 0, the raw's on side 1) and `q1: call #2: original 0x2AE14(0xBB664, 0x7203, 0xFF,
0x7405, 0x0), port 0x2AE14(0xBB664, 0x7304, 0xFF, 0x7506, 0x4000)`. The mutants `@r2`, `@r3`, `@r5`
each cache one of the three reads and `@cache` all three (the old port).

Ordinary play: no difference. 0x2AE14 (the spawn) and 0x1DA84 (the bar word) are not seen to write
DS_00104B1A; the two forms differ only under a callee that does (unverified for the whole 0x2AE14
tree, which runs descriptor callbacks).

### §3.2 `fight_char_text_clear` (0x432EC) reads the col word before each release

The raw loads the col word 0xC894C[side] four times, each after that release's 0x1C500 and so after
the preceding 0x2F280: 0x43349 `movzx ebp,word [edi+0xc894c]`, 0x4336B and 0x4338C the same, 0x433C4
`mov si,[edi+0xc894c]` (ESI zeroed at 0x433BD). Old: one read before the first release's string was
decoded. New: one read per release, after its `game_string_get`.
Measured on the old port with the `_rr` spec: `u0: call #2: original 0x2F280(0x77, 0x1B, 0x102760,
0x1000), port 0x2F280(0x8003, 0x1B, 0x102760, 0x1000)`. The mutant `@cache` is the old port.

Ordinary play: no difference (0xC894C is a table in the data object that neither 0x1C500 nor 0x2F280
writes).

### §3.3 `fight_4aad0` (0x4AAD0) tests AL of 0x4B5A8

0x4AB45 `call 0x4b5a8` is followed by 0x4AB4A `test al,al` / 0x4AB4C `jne 0x4ac10`: only AL decides
(0x4B5A8 sets AL alone, 0x4B689 `mov al,1` / 0x4B694 `xor al,al`; d2b rowed it with mask 0xFF). The
other three results of the function are tested whole (0x4AB03, 0x4AB1F, 0x4AB39, 0x4AB74, 0x4AB9E
`test eax,eax`). Old: `if (fight_4b5a8(entry, index) != 0) return;`. New: `(u8)fight_4b5a8(...) !=
0u`. Measured on the old port: `n6: call #2: original 0x4B3F0(0x10A000, 0x6, 0x0), port none` (the
stub returns 0x100). The mutant `@eax5a8` is the old port.

Ordinary play: no difference (the port's `fight_4b5a8` returns 0 or 1).

### §3.4 `fight_4b470` (0x4B470) tests AL of 0x1A570

0x4B4F6 `call 0x1a570` is followed by 0x4B4FB `test al,al` / 0x4B4FD `je 0x4b504`: AL != 0 throws
the first hit at +0x34 = 0xFF80 (0x4B525), AL = 0 at 0x0080 (0x4B507). Old:
`fighter_actor_bit15_clear(...) != 0` on the whole int. New: `(u8)... != 0u`. The real 0x1A570
leaves EAX = 0x8000 with AL = 0 when the bit is set (0x1A59A `xor al,al`, 0x1A59C `and ah,0x80`,
0x1A59F `and eax,0xffff`, 0x1A5A4 `sete al`) and 1 when it is clear, so a whole-register test of the
RAW callee would be wrong in play; the port's `fighter_actor_bit15_clear` returns 0 or 1, so the old
port agreed in play and differs only under a stub. Measured on the old port: `k1: byte 0x10A135:
original 0x00, port 0xFF` (stub 0x100) and `k2` the same (stub 0xFFFFFF00). The mutant `@eax` is the
old port. The same finding as d1a's `fighter_24754`, d1f's `fight_4cb18` and d2d's `fighter_3fdd8`.

Ordinary play: no difference.

### §3.5 `fight_4d224` (0x4D224) tests AL of 0x4D108

0x4D247 `call 0x4d108` is followed by 0x4D24C `test al,al` / 0x4D24E `jne 0x4d2a5`: only AL decides
(0x4D108 sets AL alone: 0x4D141 `mov al,1`, 0x4D149 `xor al,al`). 0x4BD4C's, 0x4D150's and 0x4B3F0's
results are tested whole (0x4D23B, 0x4D259, 0x4D27F `test eax,eax`). Old: `if (fight_4d108(entry,
index) != 0) return;`. New: `(u8)fight_4d108(...) != 0u`. Measured on the old port: `j1: call #2:
original 0x4D150(0x10A000, 0x3), port none` (the stub returns 0x100). The mutant `@eax108` is the old
port.

Ordinary play: no difference (the port's `fight_4d108` returns 0 or 1).

### §3.6 `fight_slot_pass` (0x3CB68) keeps its two counters in memory

Two differences, one commit:

1. 0x3CB6B `mov edx,[0x107ed8]` loads DS_00107ED8 at entry and 0x3CB7E `mov [0x107ed8],edx` stores EDX
   back before each side's 0x1A570 call: the value at entry for side 0, the 0x20 the inner loop ended
   on (0x3CBA1 `inc edx`) for side 1. The old port made no such store, so DS_00107ED8 held 0x1F during
   the second 0x1A570 call. Measured on the old port with plain stubs, every case: `p0: call #33
   memory: 3 bytes changed so far in the original, 3 in the port; first difference at 0x107ED8:
   original 0x20, port 0x1F`. The mutant `@pre` reproduces it.
2. Both counters are loaded from memory after each 0x3C88C: 0x3CB9B `mov edx,[0x107ed8]` (the inner
   one, then `inc edx` / `cmp edx,0x20` / `jl`), 0x3CBA7 `mov ecx,[0x107edc]` (the side, then `inc`,
   store, `cmp ecx,2` / `jl`) and 0x3CB79 `mov eax,[0x107edc]` (the side 0x1A570 gets). The old port
   counted in locals. Measured on the old port with the `_rr` spec (the step stub leaves 0x1F and
   1): the original makes 1 + 1 calls, the old port 2 + 64 (`q0: call #2 .. #65: original none, port
   0x3C88C()`). The mutants `@cn`, `@cs` cache one counter each and `@old` is the old port.

Ordinary play: no difference seen. 0x1A570 does not read DS_00107ED8 (its bytes, 0x1A570..0x1A5A8),
and 0x3C88C's own body only loads the two counters (forty references to 0x107ED8 / 0x107EDC in its scan, every one
a load, none a store); whether its callees 0x3C600, 0x3C6A8 and 0x3C758 store to them is unverified.

### §3.7 `fight_char_opp_text_blink` (0x435AC) and §3.8 `fight_char_text_blink` (0x43464) read the col word before each string

The same finding as §3.2, in the two functions that draw what 0x432EC / 0x433DC release. 0x435AC:
0x43621 `mov si,[edi+0xc894c]` after the first 0x1C500 and 0x43647 the same after the second (and so
after the first 0x2F198). 0x43464: 0x434F5 `movzx ebp,word [esi+0xc894c]`, 0x43517, 0x43538 the same
and 0x43569 `mov si,[esi+0xc894c]`, each after that string's 0x1C500. Old: one read into a local
before the first string. New: one read per `text_cursor_set`, after its `game_string_get`. Measured on
the old port with the `_rr` specs (the cursor stub rewrites the col words): `z0: call #1: original
0x2F198(0x77, 0x1C, ...), port 0x2F198(0x8003, 0x1C, ...)` and `y0: call #3: original 0x2F198(0x77,
0x1C, ...), port 0x2F198(0x8003, 0x1C, ...)`. Each `_rr` spec's `@cache` mutant is the old port.

Ordinary play: no difference (0xC894C is a table in the data object that neither 0x1C500 nor 0x2F198
writes).

### The `port/src` edits that are not corrections

Seams (first statement of a callee that had none; each callee's own row re-run after it, VERIFIED with
every mutant detected, unchanged):

- `0x1D838` `fight_hud_badge_spawn`: `PR_SEAM(0x1D838u, side, ch, y);`
- `0x1D2F0` `fight_hud_bar_set`: `PR_SEAM(0x1D2F0u, (u32)v, side);`
- `0x1D464` `fight_hud_bar2_set`: `PR_SEAM(0x1D464u, (u32)v, side);`
- `0x1DA84` `fight_hud_bar_set_inv`: `PR_SEAM(0x1DA84u, (u32)v, side);`
- `0x4CF20` `fight_4cf20`: `PR_SEAM(0x4CF20u, side);`
- `0x4B144` `fight_4b144`: `PR_SEAM(0x4B144u, entry, index);`
- `0x4AA6C` `fight_4aa6c`: `PR_SEAM_RET(0x4AA6Cu, (u32)side);`
- `0x4BD4C` `fight_4bd4c`: `PR_SEAM_RET(0x4BD4Cu, entry, index);`
- `0x4B5A8` `fight_4b5a8`: `PR_SEAM_RET(0x4B5A8u, entry, index);`
- `0x2BE1C` `fight_2be1c`: `PR_SEAM_RET(0x2BE1Cu, rec_a, rec_b);`
- `0x4BD98` `fight_4bd98`: `PR_SEAM(0x4BD98u, entry);`
- `0x4CB18` `fight_4cb18`: `PR_SEAM(0x4CB18u, entry, index, flag, side);`
- `0x4CC0C` `fight_4cc0c`: `PR_SEAM0(0x4CC0Cu);`
- `0x4D108` `fight_4d108`: `PR_SEAM_RET(0x4D108u, entry, index);`
- `0x4D150` `fight_4d150`: `PR_SEAM_RET(0x4D150u, entry, index);`
- `0x4A868` `fight_4a868`: `PR_SEAM_RET(0x4A868u, entry);`
- `0x4E350` `fight_mode25_spawn`: `PR_SEAM(0x4E350u, fresh);`
- `0x3C88C` `hit_slot_step` (fighter.c): `PR_SEAM0(0x3C88Cu);`
- `0x2C060` `config_credit_ready` (config.c): `PR_SEAM_RET0(0x2C060u);`
- `0x2C178` `prompt_press_start` (flow.c): `PR_SEAM(0x2C178u, side, (u32)row);`
- `0x2C1C8` `prompt_insert_coin` (flow.c): `PR_SEAM(0x2C1C8u, side, (u32)row);`
- `0x11F28` `frontend_coin_poll` (flow.c): `PR_SEAM_RET(0x11F28u, code);`
- `0x432EC` `fight_char_text_clear`: `PR_SEAM(0x432ECu, side, flag);`
- `0x433DC` `fight_char_opp_text_clear`: `PR_SEAM(0x433DCu, side, flag);`

Every seam is in `port/src/game/fight.c` unless marked. The rows of the seamed callees that have one
were re-run after the seam: `fight_hud_badge_spawn`, `fight_hud_bar_set`, `fight_hud_bar2_set`,
`fight_hud_bar_set_inv`, `fight_4cf20`, `fight_4b144`, `fight_4aa6c`, `fight_2be1c`, `fight_4bd4c`,
`fight_4b5a8`, `fight_4bd98`, `fight_4cb18`, `fight_4cc0c`, `fight_4d108`, `fight_4d150`, `fight_4a868`,
`fight_mode25_spawn`, `prompt_insert_coin`, `config_credit_ready`, `fight_char_text_clear` (both
specs), `fight_char_opp_text_clear`.

Exports for a binding (each loses `static` and is declared at the end of `fight.h`): `fight_4aad0`
(0x4AAD0), `fight_4b470` (0x4B470), `fight_4d224` (0x4D224).

Why 0x2BE1C is a stub and not an allowed callee: the raw 0x2BE1C computes the difference inline
(0x2BE1D..0x2BE44, no call), the port's `fight_2be1c` calls the seamed `fight_2be00` twice. Under a
row that records 0x2BE00 the port's call list would carry two calls the original never makes
(measured: `w11: call #3: original none, port 0x2BE00(0x10A100)`), so the 0x4AAD0 rows stub it.

## §4 Named gaps and unhit blocks

- `fight_hud_spawn_side_rr` 0x1DB82 `mov eax,0x4000`: the bar stub of that spec leaves DS_00104B1A = 0,
  so 0x1DB75 takes 0x1DB7E; `fight_hud_spawn_side` d1, d2 and d5 hit it.

0x432EC was d2b's named gap (the runtime strlen 0x65624 loads ES). It is rowed here: d2e's
`E.FLAT_SEGMENT_CALLEES` gives the emulator flat segments when 0x65624 is in the allow-list, so the
original runs its own strlen; the port calls the host `strlen` (no seam, no `host_strlen`).

Limits worth knowing:

- `fight_hud_side_reset`, the three HUD spawns: the side is 0 or 1 (0x1DAE8 also 0x80, which separates
  a sign-extended side byte); a side register above 1 is not exercised.
- The HUD spawns: the spawn stub returns one record per case, so all four (two in 0x1DAE8) spawns of a
  case store the same value; which store follows which spawn is carried by the memory at each call.
- `fight_hud_spawn` / `_b`: the image's tables hold equal values for several side pairs
  (0xA767C[0] = [1] = 0xA7638, four of the eight position-word pairs), so the index mutants
  `@d1`, `@d2`, `@y1`..`@y4` are caught by the poked-table cases only (h1, h4).
- `fight_char_text_clear`: both strlen calls take string 0x39, so "the second test uses the first
  result" is not separated; a string of 2^31 or more characters (the signed compare 0x4332D `jge`,
  0x433AE `jl`) is not exercised. The string table is a fixture (d2b's), not ENGLISH.TXT. In `_rr`
  the release stub writes the same two words at every call, so the third and fourth reads see what
  the second saw.
- `fight_dust_build`: the rng stub has one EAX per case for both draws of every iteration (the ranges
  0x1800 and the step are in the call list); the spawn stub returns one actor per case. The step for
  a +0x81 of 0 (0x49563 `mov eax,0x300`) is computed and never used (no iteration), so no mutant
  separates it. The port reads the loop bound slot +0x81 once where the raw reads it at every 0x49685
  after the iteration's calls, and takes the record `[edi]` once per iteration where the raw loads it
  at 0x495CF, 0x495E6 and 0x49651, and uses its local for the actor where the raw re-reads the
  entry's +8 (0x49635, 0x49659, 0x49661): every callee in between is a stub or pure here, so a read's
  position is not separated; in the game none of 0x49388, 0x29CDC, 0x5D7DC, 0x2AE14, 0x496AC is seen
  to write those fields (unverified for the 0x2AE14 tree). Not changed.
- `fight_4987c`: the rng stub's one EAX per case serves rng(2) and rng(0x300); the 0x2BE4C stub
  returns the x whatever its arguments (they are in the call list); the spawn stub returns one actor
  per case. As in `fight_dust_build` the port takes the side's record once per iteration where the
  raw loads `[slot]` again after each call (0x499C4, 0x499FD, 0x49A45, 0x49A5E, 0x49A94, 0x49AF1,
  0x49B64) and uses its local for the actor where the raw re-reads the entry's +8 (0x49B4B,
  0x49B52, 0x49B84..): not separated, not changed. The block 0x49B7E's `jne` (0x49B82) cannot be
  taken: it re-tests the +0x51 byte 0x49B6C just found zero. A count above 2 with kind 1 or 2
  reads 0xC953C beyond its two descriptors (cases f11, f15: the image's next dwords, stream
  addresses, which the spawn stub only records).
- `fight_4a928`: the two means come from one list, so both are `sum / count + 1` of real entries or
  a record's own position; the port's `b - a >= 0x1400` and `a - b` are signed C subtractions
  (undefined on overflow) where the raw wraps: cases s13 and s14 (0x80000000 against 0x7FFFFFFF)
  agree on this compiler. The port keeps the two means in locals where the raw reloads
  DS_0010885C / DS_00108858 at 0x4AA00 (it has just stored them).
- `fight_4aad0`: 0x2BE1C is a stub, so d is whatever the case says and is not tied to the two
  positions; `@dswap` (the arguments exchanged) is caught by the call list alone, the function using
  only |d|. The port reads the entry's slot pointer +0xC once where the raw loads it before each
  test (0x4AAEF, 0x4AB0B, 0x4AB52, 0x4AB7C, 0x4ABA6, 0x4ABC7): not separated. The slot bytes
  +0x54 and +0x42 are re-read on both sides, which `_rr` separates from a cached read (`@c54`,
  `@c42a`, `@c42b`).
- `fight_4b470`: the spawn stub's record is not dereferenced; the raw reads the actor's +0x18 /
  +0x30 after the 0x2BC30 call and so does the port, but the stub does not change them, so the
  read's position is not separated. An index above 11 is not exercised (0xC9604 / 0xBB920 have
  twelve entries by the 0x4D880 / record §42-C tables; unverified here).
- `fight_4c784`: the spawn stub returns one record per case for both spawns (the spit actor and the
  new ball), so the 0xEF65A stream and the new ball's fields land on one record; the memory at each
  call carries the order. The owner byte +0x21 is 0 or 1 (a sign-extended read is not separated).
  The 0x2BE4C stub returns the x whatever its arguments. A case holds at most 39 pokes (the
  runner's limit), so the globals 0x108860..0x1088AF and the new actor's +0x10..+0x4F go in as
  three blocks of non-zero filler with the fields patched in.
- `fight_4d224`: as `fight_4aad0`, the entry's slot pointer is read once by the port and before
  each test by the raw (0x4D25D, 0x4D283): not separated.
- `fight_effects_idle_pass`: the 0x4A868 stub has one EAX per case, so every entry of a case has
  the same gate; the port takes the actor `[entry+8]` and the index before the 0x4A868 call where
  the raw reloads the actor after it (0x4DFA2..): not separated. Case i6's rng value 0xFFC4 is a
  harness draw (rng(0x14) is below 0x14): it makes the re-armed word land on 0x3C through the
  16-bit store and reload (0x4DF25 / 0x4DF2B). The image's 0xC9664[1], 0xC964C[1] and 0xC9634[1]
  are equal, so the table mutants are caught on other indexes.
- `fight_mode25_enter`: a pure straight line after the gates; the spawn, flash and 0x38154 stubs
  return nothing the function reads. DH carries the zero for five byte stores across the 0x34D8C
  call (0x4E200, 0x4E206) and EDX the byte offset across 0x38154: both callees preserve EDX
  (`E.callee_clobbers` = () for each), so a stub leaves them alone.
- `fight_slot_pass`: the 0x1A570 stub has one EAX per case for both sides; DS_00107EE4 takes its
  AL (cases store 0x01, 0x00 and 0x7F). The 0x3C88C stub writes nothing in the plain spec, so the
  64 steps differ only by the counters the memory at each call carries.
- `fight_char_prompt`: three calls, no store. `fight_char_join`: the poll stub has one EAX per case
  for both sides, so "side 0 accepted, side 1 refused" is reached only through the joined bytes.
- `fight_char_opp_text_blink`, `fight_char_text_blink`: the 0x2F198 seam passes the string by value
  (its first sixteen bytes, d2c's form), so the strings are compared as bytes of the decoded buffer.
  The spawn stub's record is stored, not dereferenced. As in `fight_char_text_clear`, both strlen
  calls of 0x43464 take string 0x39. The dword DS_00104528's bit 9 is the byte DS_00104529's bit 1:
  one poke sets both.

## §5 Store sweep

| row | stores | pre-state / neighbours | store mutants caught |
|---|---|---|---|
| `fight_hud_side_reset` | DS_0010290C[side] = 0, slot +0x5A = 0, slot +0x42 &= 0xEF | 0x10290B..0x10290E and both slots' +0x41..+0x43, +0x59..+0x5B non-zero, slot-distinct; +0x42 takes 0xFF, 0x10 and 0xEF | `@b90c`, `@b5a`, `@b42` (dropped), `@bit`, `@width` (a word at +0x5A), `@side`, `@idx`, `@order` (after the 0x1D2F0 call: call memory alone) |
| `fight_hud_spawn`, `fight_hud_spawn_b` | per side: slot +0x5E, DS_0010290C[side], slot +0x5A, DS_0010290E[side] = 0; DS_001028F0 / E8 / F8 / 102900 [side] = the spawns; DS_001028E0[side] = 0; DS_00104AEC \|= 2 | both slots' +0x59..+0x5F, 0x10290B..0x102910 and the eleven dwords 0x1028DC..0x102907 non-zero; 0x104AEB..ED seeded, bit 1 clear in two cases | `@b80e`, `@b90c`, `@b80a`, `@b90e`, `@s1`..`@s4`, `@e0`, `@aec` (dropped), `@e0late`, `@aecearly` (after / before the badge call: call memory alone), `@aecbit` |
| `fight_hud_spawn_side` | DS_0010290C[side], slot +0x5B = 0; DS_001028F0 / F8 [side] = the spawns; DS_001028E0[side] = 0; DS_00104AEC \|= 8 | as above; bit 3 clear in every case but d5 | `@b90c`, `@b80b`, `@s1`, `@s3`, `@e0`, `@aec` (dropped), `@b80a` (the +0x5A byte instead), `@e0late`, `@aecearly`, `@aecbit` |
| `fight_char_text_clear` | none of its own (DS_00108144[side] is NOT zeroed) | the slot pair sits between seeded dwords | `@zero` (a store the raw does not make) |
| `fight_dust_build` | DS_001088AE / A2 / A4 / B2 / 9E / B6 [side] = 0, DS_001088A8[side] = 0xFF, DS_001088C4 / BF = 0, the word DS_00108892 = 0; per entry: the list links, desc +0x10, entry +8, +0x1E, +0x1F, +0x21, +0x0C, +0x1C word, +0x10, +0x1A word; actor +0x14, +0x2C, +0x2E (+= 4), +0x4E; DS_001088C3 / C1 = 0 | 0x108890..0x1088C5 hold 0x11.. ascending (non-zero, none 0xFF, the two sides' bytes different); the free nodes' +8..+0x23 and the actor's fields seeded | `@bae`, `@bc4`, `@ba2`, `@ba4`, `@bb2`, `@bbf`, `@ba8`, `@b9e`, `@bb6`, `@w892`, `@bc3`, `@bc1`, `@d10`, `@ne8`, `@a14`, `@f1e`, `@f1f`, `@f21`, `@fc`, `@e10`, `@w1a`, `@b4e` (dropped), `@ba8v`, `@w892b`, `@w1c` (a byte), `@w1ay`, `@clamp`, `@w2e`, `@sideidx`, `@early` (the last two stores made on the empty-pool return) |

| `fight_4987c` | per entry: the list links, entry +0x1C word = 0 then bit 0 / bit 4, desc +0x10, entry +8, +0x0C, +0x21, +0x1F, +0x10, +0x1E = 7; actor +0x14, +0x2C, +0x29 (\|= 0x10, \|= 0x40), +0x2E (+= 4), +0x4E, +0x34 | the free nodes' +8..+0x23 and the actor's +0x10..+0x1B, +0x28..+0x37, +0x47..+0x49, +0x4D..+0x4F seeded; +0x29 takes 0x00 and 0xAF | `@w1c0`, `@bit0`, `@d10`, `@ne8`, `@a14`, `@fc`, `@f21`, `@f1f`, `@or10`, `@e10`, `@b4e`, `@w34`, `@or40`, `@or10e` (dropped), `@bit0all`, `@clamp`, `@w2e`, `@type`, `@or40inv`, `@v34a`, `@v34sign` |
| `fight_4a928` | DS_001088C6 = 0 (then 1), DS_00108870 = DS_0010887C = 0 (then one of them), DS_001088C9, DS_001088C7, DS_0010885C, DS_00108858, DS_001088C8 = 1, DS_001088CA | 0x1088C5..CB = C5 66 77 88 99 AA CB; the four dwords non-zero between seeded dwords | `@c6`, `@z870`, `@z87c`, `@c7`, `@s5c`, `@fb1s`, `@c6one`, `@s58`, `@fb2s`, `@c8one`, `@cadrop` (dropped), `@c8c6`, `@ca`, `@t7c`, `@t70` |
| `fight_4aad0` | entry +0x1E = 9 (mode 9) | +0x1D / +0x1F seeded, the type starts 0x55 | `@t9` |
| `fight_4b470` | entry +0x10 = the shadow, actor +0x34 (0x80 / 0xFF80), +0x36 = 0x240, entry +0x1E = 6, +0x1C &= 0x7F | entry +0x0C / +0x14 and +0x1B..+0x22, actor +0x2E..+0x39 seeded; +0x1C takes 0xFF, 0x80, 0x7F | `@sstore`, `@w34`, `@w36`, `@type`, `@bit7` (dropped), `@v34`, `@v36`, `@bit7v` |

| `fight_4c784` | the ball entry's +0x10 = 0, +0x1F = 0; DS_0010889C[other] += 1; DS_001088AC = 0x69; DS_00108898; DS_00108864 (0 or the new entry); per new entry: the list links, desc +0x10, entry +8, +0x1F, +0x21, +0x0C, +0x1C word (0, then bit 5), +0x10, +0x1E = 1, +0x14; actor +0x14, +0x2C, +0x28 word, +0x2E (+= 4), +0x4E, +0x34, +0x29 bit 6 | 0x108860..0x1088AF and the actor's +0x10..+0x4F non-zero filler; +0x29 takes 0xFF and 0x00 | `@shzero`, `@scoredrop`, `@wac`, `@w898drop`, `@f1f`, `@z864`, `@d10`, `@ne8`, `@a14`, `@nf1f`, `@w28`, `@w1c`, `@e10`, `@s864`, `@e14`, `@or20`, `@b29` (dropped), `@score2`, `@w898`, `@f21`, `@fc`, `@clamp`, `@type`, `@v34`, `@shorder` (the zero before the 0x2B150 call: call memory alone) |
| `fight_4d224` | none | — | — |
| `fight_effects_idle_pass` | the word DS_001088B0 (re-armed, decremented), DS_001088BB = 1; per open entry: actor +0x34 = 0, +0x55 = 1 (on or above the layer), entry +0x1E = 0 | 0x1088AE..B3 and 0x1088BA..BC seeded; the actors' +0x30..+0x37 and +0x54..+0x56 seeded, +0x55 never 1 before | `@dec`, `@bb`, `@w34`, `@f55`, `@clr` (dropped), `@w34w` (a dword), `@f55lo`, `@r78`, `@order` (the flag after the 0x2BC30 call: call memory alone) |
| `fight_mode25_enter` | the word DS_00108892 (decremented, or 0); DS_00104B25 = 0, DS_001088C4 = 1, the words DS_001088A6 = 0xB4, DS_0010889A = 0, DS_00108896 = 0, DS_00104B15 = 0, DS_001088C0 = 1, DS_00104AEC &= 0xFE, DS_001088BC / BD / BE = 0, the mode word = 0x25, DS_00108884 = DS_000F0AF0, the ten bytes 0x108888..0x108891 = 0 | 0x108880..0x1088BF hold 0x41.. ascending, 0x1088C0..C5 and the other bytes seeded non-zero; DS_00104AEC takes 0xFF, 0x01, 0xFE | `@b25`, `@c4s`, `@wa6`, `@w9a`, `@w92`, `@b15`, `@c0`, `@aec`, `@bc`, `@mode`, `@w96`, `@bd`, `@be`, `@d884` (dropped), `@decb` (a byte), `@wa6v`, `@aecbit`, `@modev`, `@d884v`, `@b5`, `@stride`, `@base`, `@order`, `@order2`, `@lorder` (call memory alone) |

| `fight_slot_pass` | DS_00107EDC = 0, then + 1 per side; DS_00107ED8 (the entry value, 0..0x1F, 0x20); DS_00107EE4 = 0x1A570's AL | 0x107ED4..0x107EE5 seeded; the counters start 0x55 / 0x77, 0x20 / 2 and 0xFFFFFFFF / 1 | `@edc0`, `@ee4`, `@i`, `@edcinc`, `@final` (dropped), `@ee4w` (a word), `@pre` (the store before 0x1A570: the memory at call #33 alone) |
| `fight_char_prompt` | none | — | — |
| `fight_char_join` | DS_00104B1F \|= side + 1, DS_00108170[side] = 1 | 0x10816F..0x108172 and 0x104B1E..0x104B20 seeded; DS_00104B1F takes 0x10, 0x00, 0xFC, 0x01, 0x02 | `@f170` (dropped), `@f170v`, `@or`, `@set`, `@idx`, `@order` (the store before the poll: call memory) |
| `fight_char_opp_text_blink`, `fight_char_text_blink` | DS_00108144[side] = the sprite | the slot pair starts 0x0BAD8144 / 0x0BAD8148 between seeded dwords | `@store` (dropped), `@storeside` |

## §6 Queue entries not done

- 0x4EBB8 (979 bytes, the mode-0x25 round card): no row. Its first 0x1B0 bytes were read (the 0x62
  title through 0x2F510, the 0x37-cell loop over 0xC9850 / 0xC9BCE through 0x2F174, the start of the
  per-side score loop over 0x108888); the rest and the port's `fight_mode25_scorecard` were not compared.
  EAX mask 0 by the walk (call sites 0x4E928 and 0x4F06E). It already carries `PR_SEAM0(0x4EBB8u)`
  (d2e stubs it). Skipped for its size against the session's remaining budget, not for a blocker.
- 0x43D60 (319), 0x4434C (377), 0x44638 (350): not reached, not read. EAX masks 0 by the walk
  (0x43D60: 0x43AD3, 0x43CC1; 0x4434C: 0x44908; 0x44638: 0x447BE). 0x44638 is the third twin of
  0x43464 (`fight.c` says so in its header); by §3.2 / §3.7 / §3.8 its col word reads are worth
  checking first. 0x43D60 and 0x4434C call 0x432EC, which now has a seam.
