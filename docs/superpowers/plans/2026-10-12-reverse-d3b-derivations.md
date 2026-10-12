# Track D, wave 3, batch d3b — derivations (game/fight.c, fight.h)

Differential rows for the functions of `.superpowers/sdd/2026-10-11-reverse-d1/d3b-queue.txt`, worked top
to bottom. Ground truth: the fixed-up image `build/diffrun --image-out` writes (linear addresses),
disassembled with capstone. Every figure below was measured with
`python3 tools/diff_verify.py --image <image> --function <name> --self-check`, one row at a time; each
row's and each mutant's port side was run ten times and gave the same output each time before its pin
was written. The unittest suite, `make verify`, `make diff-verify` and the oracles were NOT run on this
branch (the integrator runs them on the merged tree).

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

The `D3B_MASKS` / `D3B_KINDS` / `D3B_CASES` pins of `tools/tests/test_diff_verify.py` are generated from
these measurements. Every stub the batch declares carries the clobbers `E.callee_clobbers` derives from
the bytes (checked per row by a scratch script). Stubs the pinned dictionary of
`test_each_stub_declares_the_registers_its_callee_clobbers` did not have are added to it as new lines
(no existing line changed): 0x1D2F0 (edx), 0x1D464 (edx), 0x1D838 (ebx, edx), 0x1DA84 (edx),
0x4CF20 ().

Two entries are rowed as two specs on one binding (the form d2e used for 0x41C28's states): a `Call`'s
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

### The `port/src` edits that are not corrections

Seams (first statement of a callee that had none; each callee's own row re-run after it, VERIFIED with
every mutant detected, unchanged):

- `0x1D838` `fight_hud_badge_spawn`: `PR_SEAM(0x1D838u, side, ch, y);`
- `0x1D2F0` `fight_hud_bar_set`: `PR_SEAM(0x1D2F0u, (u32)v, side);`
- `0x1D464` `fight_hud_bar2_set`: `PR_SEAM(0x1D464u, (u32)v, side);`
- `0x1DA84` `fight_hud_bar_set_inv`: `PR_SEAM(0x1DA84u, (u32)v, side);`
- `0x4CF20` `fight_4cf20`: `PR_SEAM(0x4CF20u, side);`

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

## §5 Store sweep

| row | stores | pre-state / neighbours | store mutants caught |
|---|---|---|---|
| `fight_hud_side_reset` | DS_0010290C[side] = 0, slot +0x5A = 0, slot +0x42 &= 0xEF | 0x10290B..0x10290E and both slots' +0x41..+0x43, +0x59..+0x5B non-zero, slot-distinct; +0x42 takes 0xFF, 0x10 and 0xEF | `@b90c`, `@b5a`, `@b42` (dropped), `@bit`, `@width` (a word at +0x5A), `@side`, `@idx`, `@order` (after the 0x1D2F0 call: call memory alone) |
| `fight_hud_spawn`, `fight_hud_spawn_b` | per side: slot +0x5E, DS_0010290C[side], slot +0x5A, DS_0010290E[side] = 0; DS_001028F0 / E8 / F8 / 102900 [side] = the spawns; DS_001028E0[side] = 0; DS_00104AEC \|= 2 | both slots' +0x59..+0x5F, 0x10290B..0x102910 and the eleven dwords 0x1028DC..0x102907 non-zero; 0x104AEB..ED seeded, bit 1 clear in two cases | `@b80e`, `@b90c`, `@b80a`, `@b90e`, `@s1`..`@s4`, `@e0`, `@aec` (dropped), `@e0late`, `@aecearly` (after / before the badge call: call memory alone), `@aecbit` |
| `fight_hud_spawn_side` | DS_0010290C[side], slot +0x5B = 0; DS_001028F0 / F8 [side] = the spawns; DS_001028E0[side] = 0; DS_00104AEC \|= 8 | as above; bit 3 clear in every case but d5 | `@b90c`, `@b80b`, `@s1`, `@s3`, `@e0`, `@aec` (dropped), `@b80a` (the +0x5A byte instead), `@e0late`, `@aecearly`, `@aecbit` |
| `fight_char_text_clear` | none of its own (DS_00108144[side] is NOT zeroed) | the slot pair sits between seeded dwords | `@zero` (a store the raw does not make) |
| `fight_dust_build` | DS_001088AE / A2 / A4 / B2 / 9E / B6 [side] = 0, DS_001088A8[side] = 0xFF, DS_001088C4 / BF = 0, the word DS_00108892 = 0; per entry: the list links, desc +0x10, entry +8, +0x1E, +0x1F, +0x21, +0x0C, +0x1C word, +0x10, +0x1A word; actor +0x14, +0x2C, +0x2E (+= 4), +0x4E; DS_001088C3 / C1 = 0 | 0x108890..0x1088C5 hold 0x11.. ascending (non-zero, none 0xFF, the two sides' bytes different); the free nodes' +8..+0x23 and the actor's fields seeded | `@bae`, `@bc4`, `@ba2`, `@ba4`, `@bb2`, `@bbf`, `@ba8`, `@b9e`, `@bb6`, `@w892`, `@bc3`, `@bc1`, `@d10`, `@ne8`, `@a14`, `@f1e`, `@f1f`, `@f21`, `@fc`, `@e10`, `@w1a`, `@b4e` (dropped), `@ba8v`, `@w892b`, `@w1c` (a byte), `@w1ay`, `@clamp`, `@w2e`, `@sideidx`, `@early` (the last two stores made on the empty-pool return) |

## §6 Queue entries not done

(updated as the batch proceeds)
