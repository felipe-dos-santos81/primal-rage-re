# Track D wave 3, batch d3a: game/flow.c — derivation record

Branch `reverse-d3a` (off `reverse-d2int` `7a9dfe7`). Queue: `.superpowers/sdd/2026-10-11-reverse-d1/d3a-queue.txt`
(24 functions of `game/flow.c`, level 0 first). Ground truth: the fixed-up image dumped by
`./build/diffrun --image-out`, disassembled with capstone; every address below is linear.
Claim of every row: equivalence on the exercised blocks and inputs only, with its callees stubbed or run as stated.

## §1 The members

| addr | port function | size | EAX mask and the instructions that fix it |
|---|---|---|---|
| `0x11F28` | `frontend_coin_poll` | 47 | full: `xor eax,eax` `0x11F53`, `mov eax,1` `0x11F4C`, or the zero EAX of `0x2C060` that `test eax,eax; je 0x11f55` (`0x11F30`) just tested |
| `0x1ECC8` | `hiscore_rank_pair` | 98 | `0xFF`: the one caller is `0x1EEE1`, `test al,al` at `0x1EEE6`; the raw leaves the rank's second byte in EAX (`xor al,al` `0x1ECE5`/`0x1ED22`, `xor al,dl` `0x1ED11`, `mov al,1` `0x1ED26`) |
| `0x26D4C` | `flow_26d4c` | 522 | 0: the one caller `0x26D3B` goes on with `or byte [0x104aec],0x18` (`0x26D40`), four pops and `ret` into `0x25326` `jmp 0x2540f`, the mode tail the wave-1 records trace dead (d1c §1, d1d §1) |
| `0x271E0` | `flow_join_prompt_draw` | 115 | 0: the one caller `0x423FC` (`0x41C28` state 7) goes on with `xor edx,edx; mov eax,[0x104abc]` (`0x42401`/`0x42403`) |
| `0x272DC` | `flow_arena_ko_check` | 164 | 0: the one caller `0x274EC` goes on with `or byte [0x104aec],2` (`0x274F1`), three pops and `ret` into `0x25362` `jmp 0x2540f` (the mode tail) |
| `0x27DC8` | `flow_match_end` | 224 | 0: neither call site in `0x27FA8` reads EAX before its `ret` (`0x28012..0x2803A`/`0x28048`; `0x280E8..0x2812F`, where `0x280F3 lea eax` is the only use); `0x27FA8`'s sites `0x263AF` and `0x2669B` write AH or nothing up to their `ret` (`0x263B4 mov ah,[0x104aec]`; `0x4E11C` on one path writes AH at `0x4E121` and AL at `0x4E19D` before any read; `0x266A0..0x266AA`); then `0x25247`/`0x2529B` `jmp 0x2540f` (the mode tail) or `0x2528C call 0x28c38`, whose first EAX use is `mov eax,edx` (`0x28C4D`) |
| `0x29970` | `flow_round_over_check` | 119 | 0: the one caller `0x29B54` goes on with `or byte [0x104aec],2` (`0x29B59`), three pops and `ret` into `0x251AD` `jmp 0x2540f` (the mode tail) |
| `0x2C178` | `prompt_press_start` | 53 | 0: it returns `0x2C0F4`'s EAX untouched (`pop esi; pop ecx; pop ebx; ret` after the call at `0x2C1A4`), which the d2c record (§1, `0x2C0F4`) traces dead through this function's nine call sites. Seen here: `0x28D40 jmp 0x28d60` (`xor eax,eax`); the three loops at `0x42606`/`0x42665`/`0x426BE` rewrite EAX at their tops (`lea eax,[ecx+1]`, `xor eax,eax`); their exit `0x4271B` and `0x432C3`/`0x432D3` return it upward (not re-traced here) |
| `0x41760` | `frontend_mode12_challenge_continue` | 98 | 0: the one caller `0x4213B` (`0x41C28` state 5) goes on with `add esp,8` (`0x42140`), six pops and `ret`: the mode step's own return, whose EAX the d2e rows hold dead (mask 0, d2e §1) |
| `0x42F60` | `flow_continue_poll` | 78 | full: `xor eax,eax` `0x42FAA`, `mov eax,1` `0x42FA3`, or the zero EAX of `0x2C060` just tested (`0x42F68`); all four callers `test eax,eax` (`0x27A3C`, `0x42CE6`, `0x42D2A`, `0x42D6F`) |
| `0x42FE0` | `flow_challenge_join` | 221 | 0: the four call sites in `0x42CB4` drop EAX — `0x42D04..`, `0x42D8F..` and `0x42D3E mov ah,5` run to the pops and `ret` into `0x425E3 jmp 0x425ea; call 0x33f08` (first EAX use `mov eax,[ebx+0x1077a8]` `0x33F15`); after `0x42DB2` the loop reloads it (`mov eax,edx` `0x42D68`) or the tail rewrites it (`xor al,al` `0x42DC8` .. `mov eax,1`; `mov al,[0x104b1f]; and al,1; and eax,0xff` `0x42DDC..`). The fifth site `0x42FB1` is in the unreferenced stub `0x42FB0` (`mov ah,5`, `ret`) |
| `0x4F24C` | `frontend_mode_15_step` | 97 | 0: `0x253E0 call` then `0x253E5 jmp 0x2540f` (the mode tail the wave-1 records trace dead) |
| `0x4F2B0` | `frontend_mode_16_step` | 103 | 0: `0x253E7 call` then `0x253EC jmp 0x2540f` |
| `0x4F318` | `frontend_mode_17_step` | 100 | 0: `0x253EE call` then `0x253F3 jmp 0x2540f`; `0x425E5 call` then `0x425EA call 0x33f08` (first EAX use a load, `0x33F15`) |
| `0x4F9C8` | `frontend_mode_1b_step` | 28 | 0: `0x2540A call` falls into the mode tail `0x2540F` |
| `0x10DB0` | `frontend_pause_tail` (attract.c) | 104 | 0: every site is `call 0x10db0; call 0x10e18` (`0x11D75`.. `0x11E99`); `0x10E18`'s first EAX use is `mov ah,[0xf0a71]` (`0x10E19`), it reads AH only until `mov eax,0x100` (`0x10E69`) |
| `0x10E18` | `frontend_continue_tail` (attract.c) | 104 | 0: every site is followed by `call 0x2bf08`, which writes AH first (`0x2BF0E`) and passes AL through on its attract arm; that row's trace (d1c §1: twelve sites, each `pop edx; pop ecx; pop ebx; ret`, traced) holds EAX dead after it |
| `0x25C1C` | `flow_round_hud_init` | 91 | 0: both call sites go on to a load of EAX on every path (`0x25CBC`: `mov eax,[0xa88b4]` `0x25CE4`, `mov eax,[0xa88a4]` `0x25CFF`, `xor eax,eax` `0x25D06`; `0x2935C`: `0x29384`, `0x2939F`, `0x293A6`) |
| `0x26A50` | `game_mode_23_step` | 572 | 0: `0x25317 call` then `0x2531C jmp 0x2540f` (the mode tail) |
| `0x27ED8` | `flow_round_over` | 206 | 0: its three call sites in `0x27FA8` (`0x28003`, `0x280BA`, `0x280D2`) are followed by four pops and `ret`; `0x27FA8`'s EAX is dead by the trace of the `0x27DC8` line above |
| `0x28CC8` | `flow_join_poll` | 158 | full: `mov eax,ebx` `0x28D1E` or `xor eax,eax` `0x28D50`/`0x28D60`; both callers `test eax,eax` and pass EAX - 1 on (`0x25264`/`0x25268`, `0x2534E`/`0x25352`) |

## §2 The row table (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `frontend_coin_poll` | `0x11F28` | 9 | 5/5 | 12 | `0xFFFFFFFF` | `0x2C060` and `0x2CA7C` stubbed (seamed here). `p4`'s stub EAX `0x100` separates `test eax,eax` from an AL test; `p7` pokes the masks, `p8` indexes the third dword |
| `hiscore_rank_pair` | `0x1ECC8` | 8 | 8/8 | 11 | `0xFF` | `0x2DDE4` and its `0x2DB58` real (their own rows): ten records give two different ranks 0..10 |
| `hiscore_rank_pair_stub` | `0x1ECC8` | 6 | 7/8 | 14 | `0xFF` | the same entry with `0x2DDE4` stubbed (seamed here): the rank's upper word (`s3` `0xFFFF0000`, `s4` `0x0000FFFF`) and the call list. `0x1ED22` named: one stub EAX serves both calls |
| `flow_26d4c` | `0x26D4C` | 10 | 8/8 | 54 | 0 | the string tree (`0x1C500`..) real, the run clock `0x32970` real in the original from its no-elapsed-time state (the port omits it), the runtime `sprintf` `0x65546` real in the original (§2.2); every other callee a stub. Found correction §3.1 |
| `flow_join_prompt_draw` | `0x271E0` | 4 | 1/1 | 23 | 0 | `0x4660C` and `0x2716C` stubbed (the latter seamed here); the memory at each call fixes the store order (`@b11late`, `@afcearly`, `@early`). `j2`/`j3` carry upper bytes in the dword `0x104AD4` (`xor al,1` on the whole EAX: `@xorb`) |
| `flow_arena_ko_check` | `0x272DC` | 11 | 5/5 | 25 | 0 | every callee a stub (`0x27254`, `0x278B0` seamed here). `a2`/`b2` run w = -1, `a3` w = -128, `n1` the side `0x80`; `a1`/`b1`/`b3` carry a byte of `0x80` or more (`@bsext`, `@bsext2`) |
| `flow_match_end` | `0x27DC8` | 5 | 11/11 | 31 | 0 | six stubs (`0x32B4C`, `0x4F728`, `0x4651C`, `0x46534` seamed here); `0x32970` and `0x32BB0` real in the original only (the port omits both, §4). `e3`: the byte `0x104B1F` = `0x83` (`@abcbit`), the handle `0x100` (`@deadlow`), the winner byte `0xFF` (`@b16sext`) |
| `flow_round_over_check` | `0x29970` | 6 | 5/5 | 25 | 0 | `0x2C3FC` and `0x27C48` stubbed (the latter seamed here). `c0` takes both arms; `c2`/`c3` carry `0xFF` (`@sext0`, `@sext1`); the voice stub's EAX has AH set in `c0`, `c2`, `c3` (the raw stores AH after `xor ah,ah`) |
| `flow_round_over_check_reread` | `0x29970` | 3 | 5/5 | 3 | 0 | the same entry with a `0x27C48` stub that stores `0x10` in `0x10789E`: side 1's byte is read at `0x299AD`, after side 0's call (`@pre`) |
| `prompt_press_start` | `0x2C178` | 6 | 4/4 | 13 | 0 | `0x2C0F4` stubbed (seamed here). `k0`: x word `0x8123` (`@xzext`); `k2`: col byte `0x83` (`@colsext`); `k4`/`k5`: side 2 |
| `frontend_mode12_challenge_continue` | `0x41760` | 4 | 1/1 | 20 | 0 | `0x4660C`, `0x25848`, `0x41350` stubbed with d2e's constants (the `0x25848` stub stores a new stage word: `@stagepre`). `h3`: a result of `0x100` (`@xorb`); `h0`: a col byte `0x83` (`@colsext`) |
| `flow_continue_poll` | `0x42F60` | 7 | 8/8 | 20 | `0xFFFFFFFF` | five stubs (`0x2CA48`, `0x2CA78` seamed here). The stubbed `0x2CA78` returns non-zero in `q1` and `0x100` in `q4`, so the arm its real body (`xor eax,eax; ret`) never selects is compared too |
| `flow_challenge_join` | `0x42FE0` | 7 | 8/8 | 32 | 0 | four stubs, none new. The stubs return AH set where the raw stores AH after `xor ah,ah` (`0x43078`). `j4`: side 2, handle `0x100` (`@dclow`); `j5`: side `0x100` (`@xorb`); `j6`: handle `0x10000` (`@e0low`) |
| `frontend_mode_15_step` | `0x4F24C` | 10 | 9/9 | 18 | 0 | `0x4F790` stubbed (seamed here): its EAX carries upper bytes (`0xFFFFFF02`, `0x101`, `0x100`: `@al`). Boundaries: old countdown 1 (`c5`), 0 (`c7`), `0x8000` (`c6`), `0x7FFF` (`c8`) |
| `frontend_mode_16_step` | `0x4F2B0` | 10 | 9/9 | 20 | 0 | the same cases; on expiry the hook `[0x104AE4]`: `0x5D812` recorded on both sides (`c2`, `c6`), the registered `0x28D68` real with its `0x4F980` (`c4`, `c7`), whose store to `0x104AFA` the raw reads back (`@pre`) |
| `frontend_mode_17_step` | `0x4F318` | 10 | 9/9 | 19 | 0 | the same cases; the word `0x1088EE = 0xFFFF` is stored before the hook call (`@late`: `call #0/#1 memory`) |
| `frontend_mode_1b_step` | `0x4F9C8` | 4 | 3/3 | 8 | 0 | `0x4FA88` stubbed (seamed here); `w2`'s stub EAX `0x100` separates `test eax,eax` from an AL test and runs the registered hook |
| `frontend_pause_tail` | `0x10DB0` | 8 | 7/7 | 20 | 0 | `0x2C3FC` stubbed, `0x29D60` (a bare `ret`) allowed. `t2`/`t3` hold every bit but one with the neighbouring held bytes all set (`@dboff`, `@d9off`, `@either`); `t7` holds the other tail's two bits |
| `frontend_continue_tail` | `0x10E18` | 8 | 7/7 | 20 | 0 | the same cases with the two bits swapped and the value 5 |
| `flow_round_hud_init` | `0x25C1C` | 6 | 7/7 | 16 | 0 | four stubs, all seamed here; `0x29D60` allowed. Open on `0x1D890` and `0x1DC6C` (no row). `r4`: bytes `0x82`, `0x80`, `0x83` (`@b12sext`) |
| `game_mode_23_step` | `0x26A50` | 16 | 12/12 | 65 | 0 | one row over the four states of the jump table `0x26A40` (a bounded switch the scan follows). The string tree real; `0x29D60` allowed; `0x32970` real in the original only (§4); seven stubs (`0x1DAE8` seamed here, no row). `c2`: character `0x80`; `c3`: side `0x80` |
| `flow_round_over` | `0x27ED8` | 8 | 9/9 | 30 | 0 | three stubs (`0x1D764`, `0x19820` seamed here; `0x1D764` has no row); the deferred audit add `0x2DAE4(0xF, 1)` real in the original, turned off by its field (§4). With `0x1D764` stubbed the `+0x5A` bytes survive, so m and every divisor vary: `o5` a negative product, `o6` a product that overflows 32 bits, `o3` a byte of `0xFF` (`@maxs`) |
| `flow_join_poll` | `0x28CC8` | 10 | 12/12 | 27 | `0xFFFFFFFF` | five stubs (`0x2C1C8`, `0x2C178` seamed here). `j6`: the byte `0x104B1F` = `0xFD` and a `0x2C060` EAX of `0x100` (`@al`); `j9`: poked masks (`@eq`); `j3`/`j4`/`j7`: the row by the option bit |

### §2.1 Callee stubs added by this batch

| callee | args | pop | clobbers | evidence |
|---|---|---|---|---|
| `0x2C060` | none | 0 | none (`_CALLEE_CLOBBER_FIXES`) | `call 0x2caa8; jmp 0x2ca2c`: `0x2CAA8` writes EAX only (`sete al` `0x2CAAF`, `and eax,0xff` `0x2CAB2`); `0x2CA2C` is `push edx` ... `pop edx` (`0x2CA44`), `ret`. The scan credits a save only to a function's own entry pushes, so it reported `edx`; the raw caller `0x11F28` reads EDX across the call (`0x11F29`, `0x11F39`) |
| `0x2CA7C` | EAX (`cmp eax,[0x105c00]` `0x2CA8B`) | 0 | none | every `ret` is plain (`0x2CA8A`, `0x2CAA7`, and `0x2CA78`'s) |
| `0x2DDE4` | EAX, EDX (`mov [esp+0xc],eax` `0x2DDEC`, `mov edi,edx` `0x2DDF0`) | 0 | `edx` | pushes EBX ECX ESI EDI EBP (`0x2DDE4..0x2DDE8`), both `ret` plain (`0x2DE89`, `0x2DE94`) |
| `0x2C2B0` | EAX, EDX (`mov esi,eax` `0x2C2B3`, `mov ecx,edx` `0x2C2B5`) | 0 | `edx edi ebp` (the scan's set, through `0x2C088`'s; the callers here reload EDX and read neither EDI nor EBP across it) | one plain `ret` (`0x2C301`) |
| `0x2716C` | EAX (`mov ecx,eax` `0x2716F`) | 0 | none | pushes EBX ECX EDX; the raw caller `0x271E0` keeps EDX (`0x27229`, read at `0x27233`), EBX, ECX and ESI across it |
| `0x27254` | none | 0 | none | pushes EBX ECX EDX ESI EDI EBP (`0x27254..0x27259`), one plain `ret` (`0x272A7`) |
| `0x278B0` | none | 0 | none | pushes EBX ECX EDX, one plain `ret` (`0x27919`) |
| `0x32B4C` | EAX, EDX (`shl eax,2` `0x32B54`, `test edx,edx` `0x32B57`) | 0 | `edx` | pushes EBX ECX; one plain `ret` (`0x32B91`) |
| `0x4F728` | none: its first EAX use is a write (`lea eax,[edx*8]` `0x4F739`, or `mov eax,0x23` `0x4F761`) | 0 | none | pushes EDX; one plain `ret` (`0x4F776`). The raw caller keeps EDX = `0x1D` across it (`0x27E46`, used at `0x27E52`) |
| `0x4651C` | none | 0 | none | one plain `ret` (`0x46530`) |
| `0x46534` | EAX, EDX | 0 | `edx` | pushes EBX ECX EBP; one plain `ret` (`0x46590`) |
| `0x27C48` | none | 0 | none | pushes EBX ECX EDX; four plain `ret` (`0x27D0F`, `0x27D8D`, `0x27DB2`, `0x27DC7`) |
| `0x2C0F4` | EAX, EDX, EBX, ECX (`mov esi,eax` `0x2C0F6`, `mov edi,ebx` `0x2C0F8`, `mov eax,ecx` `0x2C0FA`; EDX is the row, the d2c row's registers) | 0 | `ebx ecx edx ebp` | pushes ESI EDI; two plain `ret` (`0x2C167`, `0x2C176`); `0x2C178` reads none of the clobbered registers after the call |
| `0x2CA78` | none | 0 | none | `xor eax,eax; ret` |
| `0x2CA48` | none (its body reads memory only) | 0 | none | three plain `ret` (`0x2CA56`, `0x2CA74`, `0x2CA7A`) |
| `0x2CAA8` (as a stub; seamed before) | none | 0 | none | `cmp; sete al; and eax,0xff; ret` |
| `0x4F790` | none | 0 | none | pushes EBX ECX EDX EDI; one plain `ret` (`0x4F7F0`). The raw callers store DX after it (`0x4F269`) |
| `0x4FA88` | none | 0 | `edi ebp` | pushes EBX ECX EDX ESI; two plain `ret` (`0x4FAE7`, `0x4FB1F`) |
| `0x1D890` | EAX (`mov [esp+8],al` `0x1D899`) | 0 | none | pushes EBX ECX EDX ESI EDI EBP; one plain `ret` (`0x1DA06`) |
| `0x1DC6C` | EAX (`mov [esp+8],al` `0x1DC75`) | 0 | none | pushes EBX ECX EDX ESI EDI EBP; one plain `ret` (`0x1DDF2`) |
| `0x1D810` | EAX (`lea edx,[eax*4]` `0x1D813`) | 0 | `esi edi ebp` | pushes EBX ECX EDX; one plain `ret` (`0x1D836`) |
| `0x20EF8` | none (`mov eax,ecx` `0x20F05` is its first EAX use) | 0 | none | pushes EBX ECX EDX ESI EDI; one plain `ret` (`0x20F9E`) |
| `0x1DAE8` | EAX (`mov dl,al` `0x1DAEB`) | 0 | none | pushes EBX ECX EDX; one plain `ret` (`0x1DC09`) |
| `0x1D764` | EAX (`mov ebx,eax` `0x1D767`) | 0 | none | pushes EBX ECX EDX; one plain `ret` (`0x1D7B5`) |
| `0x19820` | none (its first EAX use is `mov eax,0x100c20` `0x19852`) | 0 | none | pushes EBX ECX EDX; one plain `ret` (`0x198C7`) |
| `0x2C1C8` | EAX, EDX (`mov al,[eax+0xbab58]` `0x2C1C8`; it falls into `0x2C1D4`, which takes EDX as the row) | 0 | `ebx ecx edx esi edi` | the scan's set: `0x2C1C8` has no push of its own (the saves are `0x2C1D4`'s), so it is over-approximated; `0x28CC8` reads no register after the call (`0x28D50 xor eax,eax`, three pops). `0x28D49 xor ebx,ebx` before the call is not an argument |
| `0x2C178` | EAX, EDX (the row's registers) | 0 | `edx ebp` | pushes EBX ECX ESI; one plain `ret` (`0x2C1AC`) |

### §2.2 The runtime `sprintf` runs from the original's bytes

`0x26D4C`'s text arm (`0x26EE2`) formats through `0x65546`. Wave 1 named such arms unhit (d1c §4: the core
"leaves the flat model"). The cause is the one d2e found for `strlen`: the formatter core `0x6C8EB` saves and
loads ES (`0x6C8ED push es`, `0x6C94E les edx,[ecx-8]`, `0x6CA21 mov es,edx`, `0x6CB68 pop es`), and unicorn's
empty descriptor tables turn the stack 16-bit after a segment load. `diff_emu.FLAT_SEGMENT_CALLEES |= {0x6C8EB}`
gives a row that allows it the flat descriptors; the tree that runs is `0x65546 0x6C8EB 0x6CB6C 0x6CC9F
0x6CE84 0x6CCFA 0x65533 0x72D20 0x72CD6` (`D3A_SPRINTF`, found by running the arm and adding each refused
target; `0x65533` is reached by the indirect call at `0x6CB02`). The port side has no seam there: it formats
with `snprintf`, and the two texts meet at `0x2F198`'s by-value string window (16 bytes).

## §3 Corrections

### §3.1 `flow_26d4c` drew a percent sign the original does not (`0x809AC`, `0x26EF3`/`0x26F01`)

`0x26EF3 push 0x809ac` passes the format `25 73 25 2E 30 33 64 25 00` = `"%s%.03d%"` to `0x65546` (`0x26F01`).
Run from the original's bytes (§2.2) the formatter emits nothing for the bare trailing `%`: with string `0x3D`
= `"DIFFICULTY "`, pct 100 and string `0x3E` = `"!ab"` the text handed to `0x2F198` is `DIFFICULTY 100!ab`.
The port formatted `"%s%03d%%"` (its comment called that "the equivalent") and produced `DIFFICULTY 100%!ab`.
Now `"%s%.03d"`. Mutant `@pct` is the old format (caught by `s0`, `s2`, `s3`: `call #8`). Commit `2281a5f`.
Reached in ordinary play: the end of mode `0x22` with `0x104AC4 <= 0` and `0x104529` bit 1 clear. No unit test
asserted the text. Unproven against the oracles until the integrator's gate.

## §4 Named gaps and unhit blocks

- `hiscore_rank_pair`: a 32-bit `cmp ax,dx` (`0x1ED02`) is separated by neither row: the stub's two ranks are
  one value and the real ranks have no upper word. The other widths are separated by the stub row (`@w16`,
  `@sext`, `@r1w`).
- `flow_26d4c`: nothing unhit. Limits: `0x2F198`'s seam compares the text's first 16 bytes, so `s1`'s 19
  characters hide the old `%` at index 17 (`s0`, `s2`, `s3` show it at 14 and 15); the text cases compose 15
  characters or more because the port's C stack bytes after a shorter string's NUL are not comparable; the
  raw's unbounded `repne scasb` append (`0x26F1D..0x26F3B`) is not driven past the 0x40-byte frame (the port
  bounds it). The run clock `0x32970` is not compared (host-owned): the original runs it from the state where
  it changes no byte.
- `flow_match_end`: the port omits `0x32970` (the run clock) and `0x32BB0` (two audit adds through the deferred
  `0x2DAE4` for characters below 7), both under `/* PORT: */`. The row runs them in the original from states
  where they change no byte: the clock with no elapsed time, `0x32BB0` with a first character above 6
  (`cmp eax,6; ja 0x32bd7` at `0x32BB3` returns before the second is looked at). So the argument choice at
  `0x27DF0..0x27E28` (which character goes to EAX and EDX, by the dword `0x104ABC` and slot 0's `+0x63`) is
  executed on every arm but not observable, and the audit adds for characters 0..6 — the ordinary case —
  are not compared at all. `0x4F728` takes no argument, so the stage word loaded at `0x27E40` is not
  observable either.
- `frontend_mode12_challenge_continue`, seen, not changed: the port keeps the dword `0x104AD4` it read at entry
  for `0x41350`'s first argument, where the raw re-reads it at `0x41784` after `0x4660C` and `0x25848`.
  Neither callee writes it: their direct-call trees (`0x4660C`, `0x25848`, `0x5D7DC`: three functions, no
  indirect transfer) name `0x104AD4` only in two reads of `0x25848` (`0x2595D cmp`, `0x2598F mov ecx,`). No
  stub of the row writes it, so the row does not separate the two.
- `flow_continue_poll`: `0x42F88 inc eax; sar eax,1` on `0x2CAA8`'s result feeds `0x2CA48`, which reads no
  register: the value is not observable and the port does not compute it.
- The mode-step family (`0x4F24C`, `0x4F2B0`, `0x4F318`, `0x4F9C8`): the hook is exercised with two values of the
  dword `0x104AE4` only (the unregistered `0x5D812` and the registered `0x28D68`); the registered hooks take no
  argument, so the EAX/EDX the raw leaves for them (the old countdown and its decrement) are not compared.
- `flow_round_hud_init`: the byte `0x104B1D` is read twice (`0x25C24`, and again at `0x25C4B` after the HUD
  call); no stub writes it, so a port that reused the first read is not separated (the port re-reads).
- `game_mode_23_step` state 3, the run clock's argument: `0x26B94 mov [0x104abc],eax` is followed at `0x26B99`
  by `call 0x32970` with that EAX (1, or 2 when the byte `0x104B1F` is 3), and `0x32970` keeps AL as its
  accumulator mask (`0x32A25 mov al,[esp]; 0x32A28 mov [0x107494],al`; the byte is read only by `0x32970`
  itself, `0x32994` and `0x329B2`). Every other clock call of this batch passes 0. The port omits the clock
  (`/* PORT: */`, host-owned) and so never writes `0x107494`: measured first as `c0..c3: byte 0x107494:
  original 0x01, port unchanged` with the byte seeded 0. The cases now seed the byte with the value the raw
  stores, so the clock again changes no byte; **the mask the game hands the clock here is not compared, and a
  host clock that wants per-mode play time has to take it from the dword `0x104ABC`.**
- `game_mode_23_step`: the three string ids are compared through the real string tree (the decoded text, 16
  bytes by value at `0x2F198`), with the rows a wrong stride or a sign-extended character would reach poked to
  other valid ids (`0xA8D08`, `0xA8308`), so that no mutant decodes an arbitrary id.
- `flow_round_over`: the port omits the audit add `0x2DAE4(0xF, 1)` (`0x27F0B..0x27F1F`, deferred under
  `/* PORT: */`). The row runs it in the original with field `0xF` negative (`0x2D33C` re-pointed at
  `0x105DE1 = 0x80000000`; `0x2DAEE test eax,eax; jl 0x2db54`), d2e's technique for field `0x10`: the add
  itself is not compared. The upper bound of the threshold (`cmp ebx,0x78; jge`) cannot have a mutant on its
  far side — m = `0x78` would divide by zero — so `@thr` moves it down (`o1`, m = `0x77`). `test ebx,ebx; jl`
  (`0x27F5C`) is never taken (EBX is a zero-extended byte) and is not a block leader. In the game `0x1D764`
  zeroes both `+0x5A` bytes first, so m is 0 there; the row's stub leaves them, which is what exercises the
  division.

## §5 Store-sweep findings

- `frontend_coin_poll` stores nothing itself; its one effect is the `0x2CA7C(1)` call (`@nospend`, `@spendn`,
  `@order` are caught by the call list alone).
- `hiscore_rank_pair`: `0x1044C0` and `0x1044C2` are seeded `0x5A5A`/`0x5B5B` between `0x6161` and `0x6262`.
  `a1` (the early bail) leaves `0x1044C2` at its seed, which catches a dword store of the first rank (`@c0w`);
  `@c2w` writes `0x1044C4`; `@noc0`, `@noc2`, `@swap` are byte differences. The stub row adds the order:
  `0x1044C0` is stored before the second call (`@order`: `call #1 memory`).
- `flow_26d4c`: the four stores before the `0x2F510` hold (slot `+0x41 |= 0x10`, word `0x104AFE`, byte
  `0x104AEC &= 0xEF`, word `0x104B00`) are seeded with neighbours and each has a drop/width/value mutant
  (`@nof41 @f41set @f41bit @f41off @afe @afeb @aec @noaec @b00 @b00b`); `@late` (the stores after the hold) is
  caught only by `call #7 memory`. The side byte is zero-extended: `t2`'s `0x80` writes `0x10C1F1` on both
  sides (past the image, inside `mem[]`), `@sidesext` lands on `0x102DF1`.
- `flow_join_prompt_draw`: eleven stores, each seeded `0x5A..` between neighbours, each with a drop or a
  width mutant (`@b21 @b14 @b0c @b11 @afc @afcb @hook @hookw @afe @afeb @ee @eew @b00 @b00b`).
- `flow_arena_ko_check`: two stores (w's slot `+0x41 |= 0x10`, the word `0x104B00 = 0xD`), both after the two
  voices (`@early`: `call #0 memory`). `b3` has the bit already set (`@f41set`, `@nof41` are caught where it is
  clear); `@f41side` stores to the `0x104B12` side's slot, `@f41zext` re-reads w zero-extended (`b2`).
- `flow_match_end`: three stores. The dword `0x104ABC` is stored before `0x32B4C` (`@snapearly`: `call #0
  memory`) as a dword (`@abcb`); the two flag bytes are cleared after the second `0x2C2B0` and before `0x4651C`
  (`@early`, `@late`). `e2` has both bits already clear, `e4` only the two bits set.
- `flow_round_over_check`: per arm the word `0x104B00 = 0x32` and the byte `0x104B09 = the side`, stored after
  the two voices and before `0x27C48` (`@early0/1`, `@late0/1` are caught only by the memory at a call). The
  `+0x5B` bytes next to the two tested bytes are above the threshold (`@off0`, `@off1`).
- `frontend_mode12_challenge_continue`: four stores after `0x41350` (`@early`: `call #2 memory`), each seeded
  between neighbours with a value and a width mutant.
- `flow_continue_poll`: one store, the byte `0x105C04 = 1`, after the spend (`@early`: the memory at calls
  1..3); `q4` has the byte already 1 (`@c04` is caught where it is not), `@c04w` writes `0x105C05`.
- `flow_challenge_join`: per arm the handle dword zeroed after its two `0x2B150` calls (`@noz0`, `@nozdc`,
  `@z8`), the slot's `+0x7F` byte = 0 (`j6` has it already 0) and `0x104B1F |= bit` (`@b1fset`), all before
  `0x2C2B0`; the byte `0x104B19 = 1` between `0x2C2B0` and the first `0x41310` (`@b19early`, `@b19late`: only
  the memory at a call).
- The countdown family: the delay word `0x1088EE` and the countdown `0x104AFE` are stored as words (`@eeb`,
  `@afeb`, caught where the high byte changes: `0x8000`, `0x0100`, 0); the mode word `0x104B00` takes the word
  `0x104AFA` (`@b00b`, `@src`, `@nostore`). `0x4F318` stores `0x1088EE = 0xFFFF` (`@ffff`, `@ffb`).
- The two tails: the latch byte `0xF0A71 = 1` is stored before the voice (`@a71late`: `call #0 memory`), as
  are the bytes `0x104B1B`, `0x104B15` and the words `0xF0A6C`, `0xF0A64` (`@early`); each has a drop mutant
  and the words a byte-store mutant (`@a6cb`, `@a64b`).
- `flow_round_hud_init`: the dword `0x104AD8 = 0` before the HUD call (`@ad8late`, `@ad8w`) and the byte
  `0x104B20 = 1` between the marker release and `0x20EF8` (`@b20early`, `@b20late`: only the memory at a call).
- `game_mode_23_step`: state 1's eight stores, state 2's five, state 3's four and state 4's two are each
  seeded and each has a drop, value or width mutant; the stores a callee could observe are pinned to their
  side of the call by the memory at it (`@s1ad8late`, `@s1b20late`, `@s1early`, `@s2acclate`, `@s2ae8late`,
  `@s2early`, `@s3late`, `@s3deadz`). State 4 tests the decremented value (`@s4old`: `e1`, `e4`).
- `flow_round_over`: each slot's `+0x40` dword is masked before `0x1D764(side)` (`@resetearly`), as a dword
  (`@maskw`; `o1`'s slot 1 already has both bits clear); the dword `0x104AC8` before `0x19820` (`@ac8late`);
  the byte `0x1088F2 = 0x1E` after `0x19820` (`@f2early`) and before it is read back (`@f2late`: the seed
  `0x80` is negative); then the word `0x104B00` and the dword `0x104AA8` (`@aa8w`).
- `flow_join_poll`: one store, the byte `0x104B1F |= side + 1`, between `0x2C2B0` and `0x2CA7C` (`@early`,
  `@late`: only the memory at a call); `@set` and `@orbit` are caught where other bits are set or the side is 0.

## §6 Queue entries not done

(filled at the end)
