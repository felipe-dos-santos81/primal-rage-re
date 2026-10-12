# Track D wave 5, batch d5e: the per-frame match-mode handlers — derivation record

Branch `reverse-d5e` (off `reverse-d4int` `3790f323`). Queue: `.superpowers/sdd/2026-10-11-reverse-d1/d5e-queue.txt`
(12 functions: eleven mode handlers of `game/flow.c` and the arena frame `0x263F4`). Ground truth: the fixed-up
image dumped by `./build/diffrun --image-out` (`/tmp/pr_d5e_img.bin`), disassembled with capstone; every address
below is linear. Claim of every row: equivalence on the exercised blocks and inputs only, with its callees stubbed
or run as stated.

## §1 The members

No member takes an argument. Each handler's port function returns `void`; the binding reports EAX 0 under mask 0.

| addr | port function | size | EAX mask and the instructions that fix it |
|---|---|---|---|
| `0x28BD4` | `game_mode_0a_step` | 100 | 0: the one site `0x2527D` (in `0x24C5C`) is followed by `jmp 0x2540f` (`0x25282`) |
| `0x282C4` | `game_mode_07_step` | 336 | 0: `0x25273`, then `jmp 0x2540f` (`0x25278`) |
| `0x26C8C` | `game_mode_22_step` | 192 | 0: `0x25321`, then `jmp 0x2540f` (`0x25326`) |
| `0x26F58` | `game_mode_24_step` | 353 | 0: `0x2532B`, then `jmp 0x2540f` (`0x25330`) |
| `0x26254` | `game_mode_04_step` | 415 | 0: `0x25242`, then `jmp 0x2540f` (`0x25247`); `0x25287`, then `call 0x28c38` (`0x2528C`), which loads EAX before any read (`mov eax,edx` `0x28C4D`; its first instructions push EBX/EDX and load EDX, EBX from memory) |
| `0x263F4` | `fight_arena_frame` | 329 | 0: both sites (`0x11C3F`, `0x11E8F`) are followed by `call 0x33f08`, whose first EAX use is a load (`mov eax,[ebx+0x1077a8]` `0x33F15`) |
| `0x26540` | `game_mode_21_step` | 363 | 0: `0x25296`, then `jmp 0x2540f` (`0x2529B`) |
| `0x27380` | `game_mode_0c_step` | 380 | 0: `0x2535D`, then `jmp 0x2540f` (`0x25362`) |
| `0x299E8` | `game_mode_31_step` | 380 | 0: `0x251A8`, then `jmp 0x2540f` (`0x251AD`) |
| `0x28468` | `game_mode_08_step` | 436 | 0: `0x25335`, then `jmp 0x2540f` (`0x2533A`) |
| `0x266AC` | `game_mode_25_step` | 716 | 0: the one site `0x252B2` is followed by `xor eax,eax` (`0x252B7`) |
| `0x28788` | `game_mode_09_step` | 1099 | 0: `0x2533F`, then `jmp 0x2540f` (`0x25344`) |

`0x2540F` is the mode tail (`call 0x2a31c`: `0x2A31C` writes AL at `0x2A347` or EAX at `0x2A353` before any read, or
returns at once when the byte `0x104B24` is set). Past that the earlier records hold the tail dead (d1c §1, the
`0x28C38` line; d1d §1, "the mode tail, dead (12 kill sites)"); it is not re-traced here.

## §2 The row table (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `game_mode_0a_step` | `0x28BD4` | 5 | 4/4 | 26 | 0 | every callee a stub (`0x35658`, `0x19068`, `0x49C78`, `0x12DA8` seamed here). The two +0x54 bytes at 0, 1, 3, `0x80`, `0xFF`, each between seeded bytes (`@g54w`, `@g54off`); the mode word seeded `0x5A5A` and `0x010A` (`@modeb`); `k1` gives the voice stub EAX `0xFFFFFFFF` (the mode is DX, kept by `0x2C3FC`) |
| `game_mode_0a_step_rr` | `0x28BD4` | 2 | 4/4 | 4 | 0 | a `0x12DA8` stub that zeroes both +0x54 bytes: read after the last call (`0x28C0E`, `0x28C17`) |
| `game_mode_07_step` | `0x282C4` | 16 | 16/16 | 52 | 0 | the winner byte at 0, 1, 2, 3 (`w5`: slot 3) and `0x80` (`w6`: `@wsext`); the result at -1, 0, 1, 2, 3, `0x102` (`@ad4b`), `0x80000000`; each arm's +0x52 / +0x54 byte with the other slot's seeded differently (`@wother`, `@dother`, `@dbotha`, `@dbothb`, `@dbeq`) |
| `game_mode_07_step_rr` | `0x282C4` | 2 | 6/16 | 5 | 0 | a `0x12DA8` stub that leaves the winner byte 0, the result 1 and slot 0's +0x52 byte 0 (`0x2830E`, `0x2832B`, `0x28324`). 10 blocks named (the plain row hits each) |
| `game_mode_07_step_rr2` | `0x282C4` | 2 | 5/16 | 5 | 0 | a `0x12DA8` stub that leaves the winner byte 2, the result 0 and slot 1's +0x54 byte 3 (`0x28361`, `0x28382`). 11 blocks named |
| `game_mode_22_step` | `0x26C8C` | 6 | 3/3 | 43 | 0 | sides 0, 1, 2 and `0x80` (`s5`: `@sidesext`; reads and moves the latch above the image); side 1's character byte `0x85` in `s1` (`@chsext`); the dword `0x104AC4` at -1, 0, 1, 2, `0x100` (`@ac4b`), `0x80000000` (`@ac4u`) |
| `game_mode_22_step_rr` | `0x26C8C` | 2 | 3/3 | 6 | 0 | stubs that write the side byte after `0x16D58` (1) and after `0x3CB68` (2) and the dword `0x104AC4` after `0x4D2D0`. Found correction §3.1 (`@preside` is the port before it) |
| `game_mode_24_step` | `0x26F58` | 6 | 3/3 | 83 | 0 | the audit add `0x2DAE4(0x10, 1)` real in the original only (§4). The countdown word at 0, 1, 2, 5, `0x0100` (`@tb`), `0x8001` (`@tu`); the result at 0, 1, `0x102` (`@ad4b`); the dword `0x104ABC` at `0x101` (`@abcb`) and the byte `0x104B19` at `0x80` (`@b19sext`) |
| `game_mode_24_step_rr` | `0x26F58` | 2 | 3/3 | 9 | 0 | the mode `0x22` stubs, and `0x4D2D0` leaving the countdown 1, `0x4660C` the result 1, `0x2716C` the dword `0x104ABC` = 2 and the byte `0x104B19` = 7. Found correction §3.2 |
| `game_mode_04_step` | `0x26254` | 7 | 8/8 | 72 | 0 | `0x4BF18` is in the call set so that `@fxswap` is a call difference. The gate byte at 1, 2, 3; the word `0x108892` at 5, 6, 7, `0x0100` (`@e25wb`), `0x8000` (`@e25wsext`); slot 0's +0x53 byte between seeded bytes (`@e25koff`, `@e25kw`) |
| `game_mode_04_step_rr` | `0x26254` | 2 | 7/8 | 9 | 0 | `0x16D58` leaves the gate byte 3, side 1's character byte 9 and the latch sources changed; `0x27FA8` leaves the gate byte 2, the word `0x108892` = 6, slot 0's +0x53 = 0 and the flag byte `0x40`. `0x262A6` named |
| `game_mode_21_step` | `0x26540` | 3 | 3/3 | 60 | 0 | mode 4 with `0x4BF18` for `0x49C78` and no `0x4E11C` tail (`@e25all` gives it one) |
| `game_mode_21_step_rr` | `0x26540` | 2 | 2/3 | 5 | 0 | the same two writing stubs. `0x26592` named |
| `fight_arena_frame` | `0x263F4` | 3 | 1/1 | 47 | 0 | one block. `a1`/`a2` hold a gate byte other than 2 (`@gated` is mode 4's gate) |
| `fight_arena_frame_rr` | `0x263F4` | 1 | 1/1 | 3 | 0 | the `0x16D58` stub that changes side 1's character byte and the latch sources |
| `game_mode_0c_step` | `0x27380` | 7 | 7/7 | 72 | 0 | `0x29970` is in the call set for `@chkswap`. The frozen-pose word with each test failing in turn (`h2`..`h5`), pose words `0x81E1` (`@nomask`), `0x41E1`, `0x01E0`; the slot byte at 0, 1 and `0x80` (`h6`: `@b12sext`, above the image) |
| `game_mode_0c_step_rr` | `0x27380` | 2 | 3/7 | 5 | 0 | `0x16D58` as above, `0x272DC` leaves the flag byte `0x40`. 4 blocks named |
| `game_mode_31_step` | `0x299E8` | 7 | 7/7 | 72 | 0 | `0x27380`'s cases and mutants with `0x29970` for `0x272DC` |
| `game_mode_31_step_rr` | `0x299E8` | 2 | 3/7 | 5 | 0 | `0x29970` leaves the flag byte `0x40`. 4 blocks named |
| `game_mode_08_step` | `0x28468` | 8 | 9/9 | 96 | 0 | the countdown at 0, 1, 2, `0x0100` (`@decb`), `0x0101` (`@hb`); both slots' +0x41 seeded `0x01`/`0x80` and `0x10`/`0xFF` (`@hf41set`); `e7` holds `0x1078FC` set and `0x1078FE` clear (`@fefc`) |
| `game_mode_08_step_rr` | `0x28468` | 2 | 8/9 | 6 | 0 | `0x12DA8` leaves the countdown 1, `0x28130` the byte `0x104B1D` = 3. `0x285EC` named |
| `game_mode_25_step` | `0x266AC` | 9 | 15/15 | 93 | 0 | `0x1C500` real over the stubbed `0x474E4` (four characters; the case's tail fixes the length: 0, 4 and 9 released, 16 bytes of each drawn string compared). The frame word at `0x01`, `0x10` (`@tmask0f`), `0x20`, `0x40`, `0x60`, `0x80`, `0xE0`, `0x0120`, `0xFF00`; the countdown at 0, 1, `0x3C`, `0x3D`, `0x0100`, `0x1234`, `0x8000` (`@a6s`, `@a6sgt`), `0xFFFF` (`@a6olds`) |
| `game_mode_25_step_rr` | `0x266AC` | 2 | 11/15 | 8 | 0 | `0x384F8` leaves the gate byte 2, `0x1DA08` the frame word `0x40`, the countdown `0x78` and the flag byte `0x40`. 4 blocks named |
| `game_mode_09_step` | `0x28788` | 23 | 30/30 | 174 | 0 | `0x32BAC` (a bare `ret`, four sites) allowed: real in the original, omitted by the port. 23 cases over the early returns (`n`), the drawn match (`q`), the streak arm (`s`) and the other arm (`u`); streak bytes 1, 2, `0x7F` (`@stsext`), `0xFF`; limit bytes 0, 5, 6, 9, `0x80` (`@lsext`); the stage word 3, 7, `0x0102` (`@fidxb`), `0x0107` (`@afc7b`); the result -1 (`u4`) and `0x10000` under the `0x108173` reset (`@ad42w`) |
| `game_mode_09_step_rr` | `0x28788` | 2 | 19/30 | 11 | 0 | `0x12DA8` leaves the countdown 1, `0x4A708` the bytes `0x104B1D` = 0 and `0x104B1F` = 3, `0x286BC` the byte `0x108173` = 1, `0x32B00` the result 0 and slot 0's +0x63 byte 1. 11 blocks named |

**Totals (measured):** 25 rows on 12 distinct entries (each handler is rowed again with stubs that write, `0x282C4` twice), 125 cases, 971 mutants. The one full self-check on the final tree (`PR_DIFF_JOBS=4 python3 tools/diff_verify.py --image /tmp/pr_d5e_img.bin --self-check | tail -1`, rc 0): `diff-verify: 809/809 functions VERIFIED; 10191/10191 mutants detected; 1 named gaps; 499/642 rows with callees closed (167 have none).` Before the batch, by subtraction (not measured): 784 functions, 9220 mutants, 499/617 (167); every row of this batch stays open on a callee without a row. Each row's pins were measured with every binding's port side run ten times (equal).

### §2.1 Callee stubs added by this batch

Arguments from each callee's entry bytes, `pop` from its `ret`, clobbers from `E.callee_clobbers` over the image
(every stub's declared set equals it, checked by script on the final tree).

| callee | port function | args | pop | clobbers | evidence |
|---|---|---|---|---|---|
| `0x3C5CC` | `fight_slot_clear` | none | 0 | none | `push edx; xor edx,edx`, three stores, `pop edx; ret` |
| `0x16D58` | `camera_screen_base` | EAX (`test ax,ax` `0x16D59`), EDX (`test dx,dx`) | 0 | `edx` | pushes EBX; `movsx edx,dx`; plain `ret` (`0x16DA0`); no call. The handlers reload EDX after it |
| `0x17FA0` | `camera_project` | EAX (`mov esi,eax`), EDX (`mov edi,edx`), EBX (`mov ebp,ebx`), ECX (`mov [esp+0x10],ecx`), s0, s1 | 8 | `ebx ecx edx` | pushes ESI EDI EBP; every `ret` of its scan is `ret 8`. The callers push s1 then s0 (`0x26CF2`/`0x26D01`: the index-out pointer first, the page-flag pointer second) and reload EBX/ECX/EDX before each call |
| `0x17580` | `camera_decay` | none | 0 | none | pushes EBX ECX EDX ESI EDI; plain `ret` |
| `0x1958C` | `fighter_pass_a` | none (`xor eax,eax` at `0x19595`) | 0 | none | six pushes; plain `ret` |
| `0x19068` | `fighter_pass_b` | EAX (its AL: `test al,al` `0x19088`) | 0 | `edi ebp` | pushes EBX ECX EDX; plain `ret`. No handler reads EDI or EBP after it before loading it |
| `0x1975C` | `fighter_think` | none | 0 | `edi ebp` | pushes EBX ECX EDX ESI; plain `ret` |
| `0x35658` | `fight_hud_pass` | EAX (`mov esi,eax` `0x3565F`) | 0 | `edi ebp` | pushes EBX ECX EDX ESI; plain `ret`. `0x282C4` loads EDI at `0x28390` after it; `0x26F58` loads ESI/EDI at `0x27028`/`0x27053` |
| `0x49C78` | `fight_effects_pass` | none (`xor eax,eax` after the pushes) | 0 | none | six pushes; plain `ret` |
| `0x4BF18` | `fight_4bf18` | none (`mov eax,[0x108868]`) | 0 | none | six pushes; plain `ret` |
| `0x4D2D0` | `fight_4d2d0` | none | 0 | none | six pushes; plain `ret` |
| `0x4E67C` | `fight_4e67c` | none | 0 | `ebp` | pushes EBX ECX EDX ESI EDI; plain `ret` |
| `0x1282C` | `camera_dust_spawn` | none (`mov ax,[0xef6dc]`) | 0 | none | pushes EBX ECX EDX ESI EDI; plain `ret` |
| `0x128D4` | `camera_impact_dust_spawn` | none (`mov ax,[0xef6dc]`) | 0 | `ebp` | pushes EBX ECX EDX ESI EDI; plain `ret` |
| `0x12DA8` | `camera_y_commit` | none (`mov al,[0xf0afe]`) | 0 | none | pushes EDX; plain `ret` |
| `0x1DA08` | `fight_hud_pulse` | none | 0 | none | pushes EBX ECX EDX ESI; plain `ret` |
| `0x26D4C` | `flow_26d4c` | none | 0 | `ebp` | pushes EBX ECX EDX ESI EDI; plain `ret` |
| `0x27FA8` | `flow_round_end_check` | none (`xor eax,eax`) | 0 | `esi ebp` (the scan's set) | pushes EBX ECX EDX EDI; plain `ret`. `0x26254` and `0x26540` read only memory and EDX (reloaded, `0x263BA`) after it |
| `0x4E11C` | `fight_mode25_enter` | none (`mov ah,[0x104b1d]`: AH written, then compared) | 0 | none | pushes EBX ECX EDX ESI EDI; plain `ret` |
| `0x272DC` | `flow_arena_ko_check` | none | 0 | none | pushes EDX; plain `ret` |
| `0x29970` | `flow_round_over_check` | none | 0 | none | pushes EBX EDX; plain `ret` |
| `0x28130` | `flow_match_result_text` | none | 0 | none | pushes EBX ECX EDX; plain `ret` |
| `0x4A708` | `fight_effects_hold_all` | none | 0 | none | pushes EBX ECX EDX ESI; plain `ret` |
| `0x286BC` | `flow_match_streak_update` | none | 0 | none | pushes EBX ECX EDX; plain `ret` |
| `0x4246C` | `fight_stage_marks_clear` | none | 0 | none | pushes ECX EDX; plain `ret` |
| `0x384F8` | `fight_384f8` | EAX (`mov ecx,eax` `0x384FC`) | 0 | `edi ebp` | pushes EBX ECX EDX ESI; plain `ret`. `0x266AC` loads EDI (`0x268F5`) before its `repne scasb` |

Reused constants: `VOICE` (`0x2C3FC`), `D4A_PASS` (`0x3CB68`), `D2E_M12_TIMER` (`0x4660C`), `D3A_SIDE` (`0x2716C`),
`D2E_M12_CLOSE` (`0x32A3C`), `D4A_SNAPA` (`0x32B00`), `D4A_STR4` (`0x474E4`), `D1D_CURSOR` (`0x2F198`), `D2C_RELCNT`
(`0x2F388`), `D4C_NUMDRAW` (`0x2F4D0`). Allowed (run from the original's bytes, the port has no call): the deferred
audit add `0x2DAE4` with its field read `0x2D974` under `D2E_M12_AUDIT_OFF` (field `0x10` negative: `0x2DAEE test
eax,eax; jl 0x2db54`, no byte changes), and the bare-`ret` `0x32BAC`. `0x1C500` runs real on both sides over the
stubbed `0x474E4` (d4a's technique).

No handler reads a callee's EAX: after each stubbed call the raw loads EAX, or writes AH and stores it (`0x26FF5`,
`0x263B4`), or calls again. `0x266AC` reads `0x1C500`'s EAX (`mov ebx,eax` `0x268BB`, `mov edi,eax` `0x268F5`),
which runs real. A stub EAX is varied in `game_mode_0a_step` `k1` (the voice) and `game_mode_25_step` `c4`
(`0x474E4` = 0, the empty string).

### §2.2 Every change outside the three row files

`port/src`: 26 seams, each the first statement of its function, arguments in C-signature order as `u32`, and two
corrections (§3).

| file | line added |
|---|---|
| camera.c | `PR_SEAM(0x17FA0u, side, out_a, out_b, facing, page_flag, index_out);` (`camera_project`) · `PR_SEAM0(0x17580u);` (`camera_decay`) · `PR_SEAM(0x16D58u, (u32)side, (u32)character);` (`camera_screen_base`) · `PR_SEAM0(0x1282Cu);` (`camera_dust_spawn`) · `PR_SEAM0(0x128D4u);` (`camera_impact_dust_spawn`) · `PR_SEAM0(0x12DA8u);` (`camera_y_commit`) |
| fight.c | `PR_SEAM0(0x3C5CCu);` (`fight_slot_clear`) · `PR_SEAM0(0x1DA08u);` (`fight_hud_pulse`) · `PR_SEAM0(0x4246Cu);` (`fight_stage_marks_clear`) · `PR_SEAM(0x35658u, side);` (`fight_hud_pass`) · `PR_SEAM0(0x4D2D0u);` (`fight_4d2d0`) · `PR_SEAM0(0x4BF18u);` (`fight_4bf18`) · `PR_SEAM0(0x49C78u);` (`fight_effects_pass`) · `PR_SEAM0(0x4A708u);` (`fight_effects_hold_all`) · `PR_SEAM0(0x4E11Cu);` (`fight_mode25_enter`) · `PR_SEAM0(0x4E67Cu);` (`fight_4e67c`) |
| fighter.c | `PR_SEAM0(0x1958Cu);` (`fighter_pass_a`) · `PR_SEAM(0x19068u, arg);` (`fighter_pass_b`) · `PR_SEAM0(0x1975Cu);` (`fighter_think`) · `PR_SEAM(0x384F8u, side);` (`fight_384f8`) |
| flow.c | `PR_SEAM0(0x28130u);` (`flow_match_result_text`) · `PR_SEAM0(0x272DCu);` (`flow_arena_ko_check`) · `PR_SEAM0(0x29970u);` (`flow_round_over_check`) · `PR_SEAM0(0x27FA8u);` (`flow_round_end_check`) · `PR_SEAM0(0x286BCu);` (`flow_match_streak_update`) · `PR_SEAM0(0x26D4Cu);` (`flow_26d4c`) |

The 22 existing rows of the seamed functions were re-run with `--function <row> --self-check` after the seams
(VERIFIED, every mutant detected): `camera_y_commit`, `fighter_pass_b`, `camera_screen_base`, `camera_project`,
`fighter_pass_a`, `fighter_pass_a_al`, `fight_slot_clear`, `flow_26d4c`, `camera_dust_spawn`, `fight_hud_pulse`,
`flow_round_end_check`, `flow_round_end_check_reread`, `fight_mode25_enter`, `flow_arena_ko_check`,
`flow_round_over_check`, `flow_round_over_check_reread`, `flow_match_result_text`, `fight_effects_hold_all`,
`fight_384f8`, `fight_384f8_chain`, `flow_match_streak_update`, `fight_stage_marks_clear`; and the full self-check
(§2, totals). `0x35658`, `0x17580`, `0x1975C`, `0x49C78`, `0x4BF18`, `0x4D2D0`, `0x4E67C` and `0x128D4` had no row on
this batch's base; another batch of the wave may add the identical seam line.

`tools/tests/test_diff_verify.py` outside the d5e pin block: two lines inserted at the top of the expected
dictionary of `test_each_stub_declares_the_registers_its_callee_clobbers` (the `# D5 d5e ...` comment and the 26
addresses of §2.1 with their sets); inside the block, `P45_OUTSIDE.update(D5E_OUTSIDE)`. No existing line of the
three harness files is changed or removed against `3790f323` (`git diff 3790f323 --numstat`: port/tests/diff_runner.c +1865 -0, tools/diff_verify.py +742 -0, tools/tests/test_diff_verify.py +1991 -0).
`tools/diff_emu.py` is untouched. The pinned counter line is untouched.

A static replica of the pin tests on the final tree (no emulation): every d5e spec name has its mask, every mutant
one `D5E_KINDS` and one `D5E_CASES` entry with case ids of its own spec, the stub dictionary equals the stubs of
`SPECS`, every declared set of a d5e stub equals `E.callee_clobbers` over the image, every binding name is in
`k_bindings` once with its spec's mask.

### §2.3 How the mutants are built

Every callee of a handler is a stub, so a mutant cannot be a patched callee: it is the handler written out again
in `diff_runner.c` with one choice changed (`d5e_*_core(mut)`), sharing the parts the handlers share (the latches,
the 0x35658 pair, the two-sided prelude, one 0x17FA0 pair, the gate, the projection block, the results head). Each
core was first bound as the real function and run against the original on every case of its rows (VERIFIED; a
development check, not committed): a core that differed from the raw would have made its mutants detected for the
wrong reason. One mutant enum serves the batch; a suffix names the same change wherever it is bound.

## §3 Corrections

### §3.1 `game_mode_22_step` kept the side byte across `0x16D58` (`0x26CBC`)

`0x26CBC movzx esi, byte [0x104b1a]` loads the side again after `0x26CB7 call 0x16d58`; the latch (`0x26CD1`/
`0x26CD8`) and `0x17FA0`'s six arguments (`0x26CDF..0x26D0D`) use that second load. The port used the byte it read
at `0x26C95` for both. Measured before the fix, with a `0x16D58` stub that stores 1 in `0x104B1A`:
`game_mode_22_step_rr` MISMATCH, `r0: call #2: original 0x17FA0(0x1, 0x100B0C, 0x100B04, 0x100B63, 0x100B61,
0x100AF4), port 0x17FA0(0x0, 0x100B08, 0x100B00, 0x100B62, 0x100B60, 0x100AF0)` and `byte 0x1077E8: original
unchanged, port 0x78` / `byte 0x10787C: original 0xF0, port unchanged` (the latch on the other slot). Now re-read.
Mutant `@preside` is the old port. Commit `2ffb608e`.

### §3.2 `game_mode_24_step`: the same, at `0x26F89`

`0x26F89 movzx esi, byte [0x104b1a]` after `0x26F84 call 0x16d58`, for the latch (`0x26F9E`/`0x26FA5`) and
`0x17FA0` (`0x26FAC..0x26FDA`). Measured before the fix: `game_mode_24_step_rr` MISMATCH with the same first
difference (`r0: call #2: original 0x17FA0(0x1, 0x100B0C, ...), port 0x17FA0(0x0, 0x100B08, ...)`). Mutant
`@preside`. Commit `fcfa43c8`.

**What neither claims.** `0x16D58` is a leaf: no call, and no instruction with the operand `0x104B1A` (its 6
blocks, `0x16D58..0x16DA0`, read in full). So the second load returns the first one's value in every frame, and no
frame is known to differ: the fixes remove a dependence on that. Unproven against the oracles until the
integrator's gate.

## §4 Named gaps and unhit blocks

- **Unhit blocks.** Every plain row hits every block of its function. The rows whose stubs write name the blocks
  those stubs exclude, each with the reason (the plain row hits each): `game_mode_07_step_rr` 10 (the drawn
  round and mode 8), `game_mode_07_step_rr2` 11 (the decided round and the undecided draw),
  `game_mode_04_step_rr` `0x262A6` and `game_mode_21_step_rr` `0x26592` (the projection block: the `0x16D58`
  stub leaves the gate byte 3), `game_mode_0c_step_rr` / `game_mode_31_step_rr` 4 each (the frozen-pose word's
  three blocks and the projection block), `game_mode_08_step_rr` `0x285EC`, `game_mode_25_step_rr` 4 (the
  countdown's reset and the drawn string), `game_mode_09_step_rr` 11 (the drawn match, the arm without a
  streak, the early return).
- **Calls the port omits, run in the original only.** `0x26F58`: the audit add `0x2DAE4(0x10, 1)` (`0x270A9`,
  deferred, under `/* PORT: */`), from a state where it changes no byte (`D2E_M12_AUDIT_OFF`: field `0x10`
  negative). `0x28788`: the four calls of the bare-`ret` `0x32BAC` (`0x289D9`, `0x28AFF`, `0x28B25`, `0x28BA2`)
  and the reads that only feed them (the character byte of the result's slot, `0x289C8`/`0x28AEE`/`0x28B91`;
  the loser's +0x63 byte `0x28B0D` and its character byte `0x28B18`). Neither is compared.
- **Reads not pinned to their side of a call** (no stub writes them there; the port reads each where the raw
  does, by reading): `0x282C4` the undecided draw's two +0x54 bytes (`0x283B9`, `0x283C4`); `0x26F58` the flag
  byte `0x104AEC` after `0x4D2D0` (`0x26FF5`); `0x28468` the bytes `0x1078FE` and `0xF0AFE` against `0x12DA8`
  (they are pinned after the countdown's stores: `@pre78fe`, `@prefafe`); `0x266AC` the byte `0x1088BC`
  (`0x2689E`); `0x28788` the two +0x41 bits tested for `0x3CB68` (`0x28815`, `0x2881E`, after `0x1958C`), the
  bytes `0x1078FC` and `0xF0AFE` against `0x12DA8` (`@pre78fc`, `@prefafe` likewise), slot 1's +0x63 byte in the
  drawn match, the loser's +0x80 byte, the stage word and the close's `0x104ABC` / `0x104B19`. In every
  two-sided prelude side 0's character byte is read before the first call (nothing to separate).
- **Tests whose signed and unsigned readings agree on every value** (no mutant): `0x282C4`'s result ranges
  (`test ebx,ebx; jl` + `cmp ebx,2; jg`: `0 <= r <= 2` is the unsigned `r <= 2`; `jl` + `cmp ebx,eax; jge` with
  EAX = 2 likewise); `0x266AC`'s `test ax,ax; ja` (`0x266D2`) and `test bx,bx; jbe` (`0x26940`), both "not
  zero" (the signed readings `@a6olds` and `@a6sgt` are caught); `0x28788`'s `cmp eax,3; jl` and `cmp eax,6; jge`
  on zero-extended bytes (the sign-extending `@stsext`, `@lsext` are caught).
- **Stores that are not observable.** `0x282C4` stores the result dword back at `0x283AE`, `0x283EE` and
  `0x28409`: the value just read, no call between. Its first load of that dword (`0x282C8`) is dead (reloaded at
  `0x2832B` / `0x28361`).
- **Index values.** Side bytes 0, 1, 2, `0x80` (`0x26C8C`, `0x26F58`); slot byte 0, 1, `0x80` (`0x27380`,
  `0x299E8`); winner byte 0, 1, 2, 3, `0x80` (`0x282C4`); result 0, 1, 2, -1, `0x10000` (`0x28788`). The record's
  +0x56 word of `0x27380` / `0x299E8` is exercised at `0x0103` and `0x0104` only: with bit 15 set the raw's pose
  entry lies above the image, and a sign-extending port would index below `mem[]`, so there is no sign-extension
  mutant for it (the `mem[]` rule); the byte-wide `@idxb` is caught.
- **`0x266AC`'s string lengths** are counted inline in the raw (`repne scasb` `0x26901`, `0x26928`) and by
  `strlen` in the port: equal on the lengths exercised (0, 4, 9). Both strings come from one stub, so a port that used the
  first string's length for both releases is not separated (the two ids are, in `0x474E4`'s call list).
- **Rows that read or write past the image's end by design** (`D5E_OUTSIDE`, added to `P45_OUTSIDE`; zero on
  both sides, inside `mem[]`): `game_mode_07_step` `0x10C202`+1; `game_mode_22_step` `0x10C1E4`+4, `0x10C1E8`+4, `0x10C22A`+1; `game_mode_24_step` `0x10C1E4`+4, `0x10C1E8`+4, `0x10C22A`+1; `game_mode_0c_step` `0x10C1F1`+1; `game_mode_31_step` `0x10C1F1`+1.
- **`fight_arena_frame`** is one block: three cases, differing in the character bytes, the latch sources and
  the (unread) gate byte. A thin row beside its neighbours; its 47 mutants are the call list's.

## §5 Store-sweep findings

Every stored field is seeded with a value its store changes, between seeded neighbours; each has a drop mutant
and, where a width is a choice, a width mutant.

- **The two latches** (slot +0x34 -> +0x38, every handler but the per-side ones): targets seeded `0x5A5A5A5A` /
  `0x5B5B5B5B`, sources `0x12345678` / `0x9ABCDEF0` and, in one case per row, `0x00010000` / `0xFFFF0000` (`@l0w`,
  `@l1w`: a 16-bit copy differs in either half); `@nol0`, `@nol1`, `@l1src` (slot 0's source for slot 1). Their
  place: `@latearly` (before the first `0x16D58`: `call #1 memory`, `call #2 memory`) and `@latchlate` (after the last call), both
  caught only by the memory at a call where no later store hides them. Modes `0x22`/`0x24`: `@nolat`, `@latw`,
  `@latsrc`, `@latside`, `@latearly`, `@latlate` on slots 0, 1, 2 and the slot the byte `0x80` selects.
- **`0x104AEC |= 2`** (`|= 0x18` mode `0x22`, `|= 8` mode `0x24`): seeds `0x00`, `0x02`/`0x08`/`0x10`, `0x41`,
  `0x80`, `0xBD`/`0xE7`/`0xF7` (`@aecset` is caught where other bits survive, `@noaec` where the bit is clear,
  `@aecbit`/`@aec08`/`@aec10` on the bit), and its place against the neighbouring calls (`@aecearly`, `@aeclate`:
  only the memory at a call).
- **`0x28BD4`**: the mode word `0xB` after the voice (`@modeearly`: `call #5 memory`), as a word (`@modeb`
  against `0x5A5A` and `0x010A`).
- **`0x282C4`**: the hold word `0x104AF8` = `0x258` at its three sites (`@noaf8w`, `@noaf8a`, `@noaf8b`; `@af8b`
  against the seed `0x5A5A`), the mode word at its three sites (`@nomodew`, `@nomodea`, `@nomodeb`; `@modeb`),
  the flag byte at its three sites (`@noaeca`, `@noaecb`, `@noaect`).
- **`0x26F58`**: the four bytes `0x104B21`, `0x104B14`, `0x104B0C`, `0x104B11` before `0x4660C` (`@storeslate`:
  `call #6 memory`), each inside a run of seeded bytes (`@b21w`, `@b14w`, `@b0cw`, `@b11w`); the stage word 7
  before `0x2716C` (`@afclate`); the countdown `0x78`, the hook and the mode word after it (`@hookearly`); the
  word `0x1088EE` before `0x32A3C` (`@eelate`); the byte `0x104B19` zeroed after it (`@b19early` changes the
  call's argument; `@b19w` reaches the side byte `0x104B1A`). The countdown's decrement is a word (`@decb`:
  `0x0100`) and precedes nothing observable but its own test (`@afeearly`: `call #5 memory`).
- **`0x27380` / `0x299E8`**: the frozen-pose word gets the whole word `0x104AF6` (`@restb` against `0x81E1`,
  `@restsrc`), before the first call (`@headlate`: the memory at every call from `call #0` on).
- **`0x28468` / `0x28788`**: the countdown word (`@nodec`, `@decb`), then, when it runs out, `0x1078FE` and
  `0x1078FC` = 1 (`@fe78w`, `@fc78w` against the seeded `0x1078FF` / `0x1078FD`), both +0x41 bytes `|= 0x10`
  (`@nohf41a`, `@nohf41b`, `@hf41set`, `@hf41bit`), `0xF0AFE` = 4 (`@afe4w` against `0xF0AFF`); all after
  `0x12DA8` (`@holdearly`: `call #12 memory` in mode 8). The tail's words `0x104AFE` = `0xF0`, `0x1088EE` = `0x3C` after the
  two (three) calls (`@tailearly`), as words (`@afeb`, `@eeb`); the hook dword whole (`@ae4w`), the byte
  `0x104B25` (`@b25w` against `0x104B26`), the mode word (`@b00b`), the word `0x104AFA` (`@afab`).
- **`0x28788`** besides: the result dword = 2 whole (`@ad42w`: `q4`'s `0x10000`), the two +0x63 bytes zeroed
  (`@no813a`, `@no813b`), the byte `0x104B1B` (`@b1bw` against `0x104B1C`), the dword `0x104ABC` whole before
  `0x32B00` (`@abcw`: `q0`'s `0x00030001`; `@abclate`), the drawn match's two result stores in their order
  (`@dswap`: `q3`), the streak byte's increment as a byte (`@stw`: `s4`'s `0xFF`), the stage byte, the byte
  `0x104B17` in each of its five arms (`@nob17d`, `@nob17s`, `@nob17e`, `@nob17m`, `@nob17u`), the streak
  byte's reset on the loser's slot (`@st0win`, `@st0w`), `0x104B17` = 0 after `0x4246C` (`@markslate`), every
  store of the close before `0x32A3C` (`@closeearly`).
- **`0x266AC`**: the countdown word decremented as a word (`@a6decb`: `0x0100`), the byte `0x104B18` = 1
  (`@b18w` against the seeded `0x104B19`), the word = `0xB4` whole (`@a6rb`: over the decremented `0xFFFF`),
  all before the first call (`@headlate`).

## §6 Queue entries not done

None: the twelve entries of the queue are rowed. `0x28788`, which the brief allowed one spec per state, is one
row over 23 cases (it has no jump table: the scan follows every branch) and one row with writing stubs.

## §7 What is not verified

- The two corrections (§3) and the 26 seams are unproven against the capture oracles: `make verify`, `make
  diff-verify` and the `*-oracle` targets were not run (the integrator's gate). `run_tests` passed with `frames/`
  regenerated after the corrections (`./build/prageport --game-dir data/game/C --check 820`).
- `python3 -m unittest tools.tests.test_diff_verify` was not run (forbidden to a batch); the pins rest on the
  per-row measurements, the one full self-check and the static replica of §2.2.
- The check that each mutant core, with no mutation, equals the original (§2.3) was run in development and is
  not in the tree.
- The deferred or omitted callees are compared nowhere: the audit add `0x2DAE4`, the bare-`ret` `0x32BAC`.
- Rows that stay open on callees without a row on this batch's base: every row here (`0x35658`; `0x49C78`,
  `0x17580`, `0x1975C`, `0x4BF18`, `0x4D2D0`, `0x4E67C`, `0x128D4` where called), and `game_mode_25_step` on the
  allowed `0x1C500`.
- Mask 0 rests on the caller instructions quoted in §1; the mode tail past `0x2A31C` is taken from the earlier
  records named there.
- The read positions listed in §4 as not pinned.
