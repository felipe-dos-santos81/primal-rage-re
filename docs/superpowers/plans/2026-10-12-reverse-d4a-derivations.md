# Track D wave 4, batch d4a: game/flow.c — derivation record

Branch `reverse-d4a` (off `reverse-d3int` `94898999`). Queue: `.superpowers/sdd/2026-10-11-reverse-d1/d4a-queue.txt`
(15 functions, level 0 and 1). Ground truth: the fixed-up image dumped by `./build/diffrun --image-out`
(`/tmp/pr_d4a_img.bin`), disassembled with capstone; every address below is linear.
Claim of every row: equivalence on the exercised blocks and inputs only, with its callees stubbed or run as stated.

## §1 The members

| addr | port function | size | EAX mask and the instructions that fix it |
|---|---|---|---|
| `0x41578` | `frontend_darken_marked` | 146 | 0: `0x41755` (in `0x416D4`) is followed by `pop edx; pop ebx; ret`, and `0x416D4`'s one site `0x420FE` by `add esp,8`, six pops and `ret`; `0x41DE6`, `0x42337`, `0x42352` (in `0x41C28`) by the same `add esp,8`, six pops, `ret`. `0x41C28`'s EAX is held dead by the d2e record (§1), not re-traced here. The raw returns `0x2C3FC`'s EAX with AH cleared (`xor ah,ah` `0x415F6`) |
| `0x27FA8` | `flow_round_end_check` | 392 | 0: `0x263AF` goes on with `mov ah,[0x104aec]` (`0x263B4`) and reads no EAX up to its pops and `ret` (or `0x263EA call 0x4e11c`, which writes AH at `0x4E121`); `0x2669B` with `or byte [0x104aec],2`, three pops, `ret`. Onward the d3a record (§1, the `0x27DC8` line) traces both returns dead; not re-traced here |
| `0x2791C` | `flow_continue_take` | 269 | 0: the one caller `0x27A64` goes on with `mov eax,[0x10810a]` (`0x27A69`) |
| `0x28DA4` | `flow_player_join` | 244 | 0: both sites (`0x25269`, `0x25353`) are followed by `jmp 0x2540f`, the mode tail the wave-1 records trace dead (d1c §1, d1d §1); not re-traced here |
| `0x25C88` | `game_mode_05_step` | 804 | 0: the one site `0x2524C` is followed by `jmp 0x2540f` |
| `0x29328` | `game_mode_30_step` | 662 | 0: the one site `0x2519E` is followed by `jmp 0x2540f` |
| `0x1CB18` | `sound_sample_start` | 271 | 0: the one caller `0x1CF26` rewrites EAX in its loop (`mov eax,edx` `0x1CF23`) and leaves through `0x1C930` (`mov eax,[0x1028cc]` `0x1C936`) or its `ret` (next line) |
| `0x1CF20` | `game_audio_service` | 32 | 0: `0x256B6` goes on with `mov eax,[0x10150c]` (`0x256BB`); `0x2EB05` with `jmp 0x2eaf9`, `xor eax,eax` |

## §2 The row table (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `frontend_darken_marked` | `0x41578` | 5 | 6/6 | 36 | 0 | `0x33904` real on both sides; `0x13D4C` (seamed here, no row), `0x32A3C`, `0x2C3FC` stubbed. `k2` holds the near-miss handles (one bit off, the upper byte dropped or set: `@hw`); `k1`/`k3` carry a mode dword with upper bytes and a flag byte of `0x80`/`0xFF` (`@mode`, `@flagsext`); the voice stub returns AH set in `k1`/`k2` (the byte `0x104B25` is AH after `xor ah,ah`) |
| `flow_round_end_check` | `0x27FA8` | 21 | 25/25 | 73 | 0 | every callee a stub (`0x39FF4`, `0x27ED8`, `0x27DC8` seamed here). Boundaries: the two `+0x5A` bytes at `0x77`, `0x78`, `0x80`, `0xFF`; the timer byte `0x1088F2` at 0, 1, `0x7F`, `0x80`, `0xFF`; the dword `0x104AD4` at -1, 0, 1, 2, 3, `0x102`; the mode word `0x010B` (`@b00b`) and `0x000B` under a seeded upper word (`@b00d`); `0x104ADC` = `0x105` against the byte 5 (`@adcb`); |A - B| at 3 and 4 both ways. `b1`'s winner byte `0x80` writes `0x10C1F2`, above the image, on both sides (`D4A_OUTSIDE`) |
| `flow_round_end_check_reread` | `0x27FA8` | 6 | 23/25 | 9 | 0 | the same entry with stubs that write what the raw reads after each call: `0x27C48` the dword `0x104AD4` = 2, `0x39FF4` the bytes `0x10780A` = `0x20` and `0x104B1E` = `0x77`, `0x27DC8` the dword `0x104AD4` = 3 and the byte `0x104B16` = 2. Found correction §3.1 (`@pre` is the port before it). `0x2802D` and `0x280F3` named: this row's `0x27DC8` stub excludes them |
| `flow_continue_take` | `0x2791C` | 5 | 3/3 | 51 | 0 | `0x1C500` real over a stubbed `0x474E4` that writes four characters with no terminator: the case's seeded tail fixes the length (9, 4, 0 through a stub EAX of 0, `0x20`, 5: `@lenfix`). `0x2F388`, `0x2C088`, `0x33B00` (seamed here, no row), `0x1D764`, `0x46534` stubbed. Sides 0, 1, -1 (`t2`), -128 (`t3`: the zero-extension mutants) |
| `flow_continue_take_reread` | `0x2791C` | 3 | 3/3 | 8 | 0 | the same entry with stubs that write the side byte `0x10810D` (`0x33B00` -> `0xFF`, `0x1D764` -> 1, `0x46534` -> 0) and the bytes `0x104B0B`, `0x104B12` (`0x1D764`): each of the four side reads and the two byte reads is on its side of its call |
| `flow_player_join` | `0x28DA4` | 5 | 1/1 | 55 | 0 | `0x1C500` real over the stubbed `0x474E4`; `0x2C3FC`, `0x2F510`, `0x4651C`, `0x33C18`, `0x32B00` (seamed here) stubbed; the run clock `0x32970(0)` and the audit add `0x2DAE4(0xD, 1)` real in the original only (§4). Sides 0, 1, 2, -1, `0x101` (`@c5blow`); `j4` writes `0x110C0B`, above the image, on both sides |
| `game_mode_05_step` | `0x25C88` | 26 | 33/33 | 111 | 0 | one row over the four states of the table `0x25C78` (a bounded switch the scan follows) and the default. Every callee a stub (`0x3CB68`, `0x25C1C`, `0x256F4`, `0x4F37C`, the entrances `0x40CB0`, `0x24568`, `0x24804` seamed here); the run clock real in the original only. State 1 reaches seven of the eight descriptor dwords `0xA8898..0xA88B4` (all distinct in the image; `0xA889C`... see §4); state 3's entrance cases reach characters 0, 1 and 6 and side `0x80` (`u3`: `@ssext`), with the badge word poked to `0x8123` (`@badgey`, `@badgeysext`); state 4's countdown at 2, 1, 0, `0x8001`, `0x0100`, `0x8000` |
| `game_mode_05_step_reread` | `0x25C88` | 3 | 18/33 | 9 | 0 | the same entry with stubs that write: `0x25C1C` and `0x2AE14` the byte `0x104B14` = 1, `0x32B00` the dword `0x104ABC` = `0x77`, `0x32B4C` the byte `0x104B14` = 1, the entrance `0x40CB0` the byte `0x104B12` = 1, `0x1D838` the byte `0x10810D` = `0x66`. 15 blocks named (the plain row hits each) |
| `game_mode_30_step` | `0x29328` | 24 | 25/25 | 92 | 0 | mode 5's twin on the table `0x29318`; states 1 and 4 are the same code. State 2's voice is `0xD7` whatever `0x104B14` holds (`@s2voice` is mode 5's pick); state 3 advances only with the byte `0x104B21` == 0 (`v1` 1, `v2` `0x80`). `0x32B00` is in the call set so that a snapshot copied from mode 5 (`@snapa`) is a call difference |
| `game_mode_30_step_reread` | `0x29328` | 1 | 6/25 | 3 | 0 | the same entry with a `0x25C1C` stub that stores 1 in the byte `0x104B14`, read at `0x29361`. 19 blocks named (the plain row hits each) |
| `sound_sample_start` | `0x1CB18` | 9 | 5/5 | 34 | 0 | `0x1B544` real on both sides against the preloaded resource entry (`c2b_res`); the seven AIL thunks stubbed (`0x5DC2A`, `0x5DC4D`, `0x5DC70`, `0x5DCA6`, `0x5DCE4` seamed here). Lengths 0, 1, 3, 4, 5, 7, `0x23`, `0x105` (`@sizeb`: `s8` only); loop bytes 0, 1, 2, `0x81`, `0xFF`; slots 0..3; `s6` has no queued handle but a resolvable handle 0 (`@nogate`) |
| `game_audio_service` | `0x1CF20` | 2 | 4/5 | 6 | 0 | the four `0x1CB18` calls only (`0x1CB18` seamed here). `0x1CF39` named: the pending-music arm has no port counterpart (§4). A thin row: say so wherever it is counted |

**Totals (measured):** 12 rows on 8 distinct entries (`0x27FA8`, `0x2791C`, `0x25C88` and `0x29328` are each
rowed twice, once per stub set), 110 cases, 487 mutants. The one full self-check on the final tree
(`PR_DIFF_JOBS=4 python3 tools/diff_verify.py --image /tmp/pr_d4a_img.bin --self-check | tail -1`, rc 0):
`diff-verify: 745/745 functions VERIFIED; 8337/8337 mutants detected; 1 named gaps; 484/578 rows with callees
closed (167 have none).` (before the batch: 733/733, 7850/7850, 479/566 (167)). Each row's pins were measured
with every binding's port side run ten times (equal).

### §2.1 Callee stubs added by this batch

| callee | args | pop | clobbers | evidence |
|---|---|---|---|---|
| `0x13D4C` | EAX (`mov esi,eax` `0x13D51`), EDX (its DL: `mov [ecx+0xd],dl` `0x13D98`) | 0 | `edx` | pushes EBX ECX ESI EDI EBP; one plain `ret` (`0x13DEF`). `0x41578` reloads EDX after it |
| `0x39FF4` | none | 0 | none | six pushes; one plain `ret` (`0x3A0E8`) |
| `0x27ED8` | none | 0 | none | pushes EBX ECX EDX ESI; one plain `ret` (`0x27FA5`) |
| `0x27DC8` | none | 0 | `esi edi ebp` (the scan's set) | pushes EBX EDX; one plain `ret` (`0x27EA7`). `0x27FA8` writes EBX and EDI after it (`0x28012`, `0x28017`) and keeps EDX, which the callee saves (`xor edx,edx` `0x280E1`, `mov dl,` `0x280E8`) |
| `0x33B00` | EAX (`mov ebp,eax` `0x33B07`), EDX (`mov esi,edx` `0x33B09`), EBX (`mov esi,ebx` `0x33B60`) | 0 | `ebx edx` | pushes ECX ESI EDI EBP; one plain `ret` (`0x33C16`). `0x2791C` reloads EBX and EDX per iteration (`0x27994`, `0x27996`) |
| `0x32B00` | EAX (`shl eax,2` `0x32B08`), EDX (`test edx,edx` `0x32B0B`) | 0 | `edx` | pushes EBX ECX; one plain `ret` (`0x32B48`) |
| `0x3CB68` | none | 0 | none | pushes EBX ECX EDX; one plain `ret` (`0x3CBC2`) |
| `0x25C1C` | none | 0 | `esi edi ebp` (the scan's set) | pushes EDX; one plain `ret` (`0x25C76`). `0x25C88` zeroes ESI after it (`0x25D74`) and reads neither EDI nor EBP |
| `0x256F4` | none | 0 | none | six pushes; one plain `ret` (`0x257A0`) |
| `0x4F37C` | none (its first EAX use is `mov ah,[0x104b1d]` `0x4F37F`) | 0 | none | pushes EBX ECX EDX; four plain `ret` |
| `0x40CB0`, `0x24568`, `0x24804` | EAX (`mov esi,eax` at entry + 4 in each) | 0 | `ebp` | pushes EBX ECX EDX ESI; one plain `ret` each (`0x40E12`, `0x246D2`, `0x24960`). Reached through `call [edx*4+0xa8628]` (`0x25F27`); the raw reloads EAX, EBX and EDX after it |
| `0x1CB18` | EAX (`mov [esp+4],eax` `0x1CB21`) | 0 | none | six pushes; one plain `ret` (`0x1CC26`). `0x1CF20` keeps EDX, its counter, across it |
| `0x5DC2A` | s1, s2 (buffer, length) | 0 | `edx` | **args from the caller's pushes** (`0x1CB78 push ecx`, `0x1CB7F push esi`, under the handle `0x1CB86 push edi`; `add esp,0xc` `0x1CB8C`), not from the thunk's bytes, which were not read; clobbers from `E.callee_clobbers` |
| `0x5DCA6` | s1 (the rate) | 0 | `edx` | the same way (`0x1CBA4 push 0x2b11`, `add esp,8`) |
| `0x5DC4D` | s1, s2 (format, flag) | 0 | `ebx ecx edx` | (`0x1CBB8`/`0x1CBBA push 0`, `add esp,0xc` `0x1CBD0`) |
| `0x5DCE4` | s1 (the count) | 0 | `edx` | (`0x1CBD8 push 0`, `add esp,8`) |
| `0x5DC70` | none | 0 | `ebx ecx edx` | (`0x1CBFD push edi`, the handle; `add esp,4` `0x1CC13`) |

`0x5DC0F`, `0x5DCC5`: the constants earlier rows use (no handle argument). The AIL handle (s0, the slot's `+0x00`
dword) is a host object in the port and is compared in no row.

### §2.2 Every change outside the three row files

`port/src` (19 seams, each the first statement of its function, arguments in C-signature order; one correction,
§3.1):

| file | line added |
|---|---|
| effects.c | `PR_SEAM_RET(0x13D4Cu, source_rec, byte_arg);` (`effects_spawn_darken`) |
| fighter.c | `PR_SEAM0(0x39FF4u);` (`fighter_39ff4`) · `PR_SEAM(0x33B00u, side, src, dst2);` (`fighter_state_33b00`) · `PR_SEAM(0x24568u, side);` (`fighter_24568`) · `PR_SEAM(0x40CB0u, side);` (`fighter_40cb0`) · `PR_SEAM(0x24804u, side);` (`fighter_24804`) |
| flow.c | `PR_SEAM0(0x27DC8u);` (`flow_match_end`) · `PR_SEAM0(0x27ED8u);` (`flow_round_over`) · `PR_SEAM0(0x25C1Cu);` (`flow_round_hud_init`) · `PR_SEAM0(0x256F4u);` (`flow_win_markers_spawn`) · `PR_SEAM0(0x4F37Cu);` (`flow_round_timer_draw`) · `PR_SEAM(0x1CB18u, slot);` (`sound_sample_start`) |
| config.c | `PR_SEAM(0x32B00u, idx, arm);` (`config_play_time_snap`) |
| fight.c | `PR_SEAM0(0x3CB68u);` (`fight_slot_pass`) |
| platform/audio/ail.c | `PR_SEAM(0x5DC2Au, (u32)((const u8 *)buf - mem), len);` (`AIL_set_sample_address`) · `PR_SEAM(0x5DC4Du, (u32)format, flag);` (`AIL_set_sample_type`) · `PR_SEAM0(0x5DC70u);` (`AIL_start_sample`) · `PR_SEAM(0x5DCA6u, rate);` (`AIL_set_sample_rate`) · `PR_SEAM(0x5DCE4u, count);` (`AIL_set_sample_loop_count`) |

The existing rows of the seamed functions were re-run with `--function <row> --self-check` after their seam
(VERIFIED, every mutant detected): `fighter_39ff4`, `flow_match_end`, `flow_round_over`,
`config_play_time_snap`, `fight_slot_pass`, `fight_slot_pass_rr`, `flow_round_hud_init`,
`flow_win_markers_spawn`, `flow_round_timer_draw`, `sound_sample_start`; and the full self-check.

`tools/tests/test_diff_verify.py` outside the d4a pin block: two lines inserted at the top of the expected
dictionary of `test_each_stub_declares_the_registers_its_callee_clobbers` (`# D4 d4a ...: the stubs its rows
add` and 19 addresses with their sets); inside the block, `P45_OUTSIDE.update(D4A_OUTSIDE)`. No existing line of
the three harness files is changed or removed (`git diff 94898999`: 519, 1281 and 1005 added lines, 0 removed).
`tools/diff_emu.py` is untouched. The pinned counter line is untouched.

A static replica of the pin tests on the final tree (no emulation): every d4a spec name has its mask, every
mutant one `D4A_KINDS` and one `D4A_CASES` entry with case ids of its own spec, the stub dictionary equals the
stubs of `SPECS`, every declared set of a d4a stub equals `E.callee_clobbers` over the image, every binding
name is in `k_bindings` once with its spec's mask.

## §3 Corrections

### §3.1 `flow_round_end_check` kept the two `+0x5A` bytes across the freeze (`0x2808B`/`0x28090`)

`0x2808B mov al,[0x10780a]` and `0x28090 mov dl,[0x10789e]` load A and B again for the `|A - B| <= 3` test,
after the `0xD3` voice (`0x2805F`) and `0x39FF4` (`0x28064`). The port used the two bytes it read at entry
(`0x27FAE`/`0x27FBA`). Measured before the fix, with a `0x39FF4` stub that stores `0x20` in `0x10780A`:
`flow_round_end_check_reread` MISMATCH, `r2: call #2: original 0x27C48(), port 0x27ED8()`, `r3: call #2: original
0x27ED8(), port 0x27C48()`. Now re-read. Mutant `@pre` is the old port (caught by `r2`, `r3`, `r5`). Commit
`eac3b03b`.

**What this does not claim.** No instruction in the direct-call trees of `0x39FF4` and `0x2C3FC` (202 functions
scanned; indirect transfers in the runtime, `0x2B150`'s hook `0x2B185` and `0x51ED8` not followed) has a memory
operand with displacement `0x5A`, `0x10780A` or `0x10789E`, so no ordinary frame is known to differ: the fix
removes a dependence on that, it is not shown to change play. Unproven against the oracles until the
integrator's gate.

## §4 Named gaps and unhit blocks

- `frontend_darken_marked`: nothing unhit. The list walk is `0x33904`'s own bytes on both sides, so "live" is
  not this row's claim.
- `flow_round_end_check`: nothing unhit in the plain row. Not separated: in the first arm the mode word
  `0x104B00`, the byte `0x104B1E` and the dword `0x104ADC` are read after `0x27C48` (`0x27FCF..0x27FE2`) and no
  stub writes them there; in the second arm the mode word and the bytes `0x104AF2`/`0x104AF3` after the freeze
  likewise. The port reads each after the call, as the raw. An unsigned test of the dword `0x104AD4` for mode 9
  agrees with the signed one on every value (no mutant).
- `flow_continue_take`: the side read for `0x2C088` (`0x2794D`) is not pinned to its side of the first
  `0x2F388` (no stub writes it there). ECX holds the length at the first `0x2F388` call too; `0x2F388` takes
  three registers, so it is not compared.
- `flow_player_join`: the port omits the run clock `0x32970(0)` (`0x28DAC`, host-owned) and the audit add
  `0x2DAE4(0xD, 1)` (`0x28E53`, deferred), both under `/* PORT: */`. The row runs them in the original from
  states where they change no byte: the clock with no elapsed time (`d1i_pt_case`), the add with field `0xD`
  negative (`0x2D334` re-pointed at `0x105DE1 = 0x80000000`; `0x2DAEE test eax,eax; jl 0x2db54`), d2e's and
  d3a's technique. Neither is compared.
- `game_mode_05_step`: (a) state 3 hands the run clock EAX = the dword `0x104ABC` (`0x25EC5`/`0x25ECA`), which
  `0x32970` keeps as its mask byte `0x107494`; the cases seed that byte with the value so that the clock
  changes no byte, as d3a did for `0x26B99`. **The mask is not compared.** (b) The character byte is exercised
  at 0, 1 and 6 only: three of the seven entrances are seamed, and a negative byte would index below the table
  `0xA8628`. The port's `if (fn)` guard for an unregistered entry is not exercised. (c) State 1 reaches the
  descriptor dwords `0xA88B4`, `0xA88A4`, `0xA88B0`, `0xA8898`, `0xA88AC`, `0xA88A0`; `0xA889C` (i = 1 without
  the option bit) and `0xA88A8` (i = 0 with it) are not selected by a case, though `@i01`, `@tblswap` and
  `@stride` are each caught. (d) Not separated by the reread row: the reads of `0x104529`, `0x104B1E` and
  `0x104ADC` after `0x25C1C`, of `0x104B1E` after the clock, of `0x104B14` at `0x25EEA` (the one at `0x25F02`
  is: `@preb14m`), of the character byte after the entrance.
- `game_mode_30_step`: the clock as (a) above. The byte `0x104B21` is read after the clock (`0x29564`); no stub
  can write it there. The reread row has one case.
- `sound_sample_start`: (a) the AIL handle argument of every thunk is not compared. (b) The raw reads the
  buffer pointer and the length again after `0x5DC0F` (`0x1CB73..0x1CB79`); the port passes the values it read
  for the copy. No stub writes them; the thunk's driver code was not scanned. Seen, not changed. (c) The port's
  `/* PORT: */` guard (no buffer, or an unresolved handle: return) is not exercised: the raw would copy to
  linear 0. (d) The thunk arguments are named from the caller's pushes (§2.1).
- `game_audio_service`: `0x1CF39` unhit and named. The raw starts pending music when the dword `0x1028CC` is
  non-zero; the port keeps its request in a C flag (`s_music_request`) and its `0x1C930` is host code. What the
  port does after the four calls (sequencer ticks, the mixer) is host state: the row shows only that it changes
  no `mem[]` byte on these two cases.
- Rows that read or write past the image's end by design (`D4A_OUTSIDE`, added to `P45_OUTSIDE`):
  `flow_round_end_check` `0x10C1F2`, `flow_player_join` `0x110C0B`.

## §5 Store-sweep findings

- `frontend_darken_marked`: five stores after the voice (words `0x104AFE` = `0x78`, `0x1088EE` = 0, `0x104AFA`
  = `0x13`, `0x104B00` = `0x15`, byte `0x104B25` = 0), each seeded `0x5A..` between neighbours, each with a
  value, a drop and a width mutant; `@early` and `@mid` are caught only by the memory at a call.
- `flow_round_end_check`: the word `0x104AF8` = `0x258` after `0x27DC8` (`@af8early`: `call #1 memory`), the
  mode word as a word (`@mb`, `@m7b`: caught where its high byte is seeded, `a8`, `b0`); the slot `+0x42 |=
  0x80` after `0x27DC8` (`@orearly`), with a seed that already has other bits (`@orset`) and the neighbouring
  bytes seeded (`@oroff`); the three tail bytes each with a width mutant against a seeded neighbour (`@fcw`,
  `@few`, `@afew`).
- `flow_continue_take`: the slot's `+0x5B` byte after `0x1D764` and before `0x46534` (`@b5bearly`: `call #7
  memory`); the mode word and the `+0x41` mask after `0x46534` (`@b00early`, `@f41early`: `call #8 memory`);
  `@f41clr` is caught where other bits survive the mask.
- `flow_player_join`: the three zeroed dwords before the voice (`@late3`: `call #0 memory`), each with a
  narrow-store mutant; the dword `0x104ABC` before the string decode (`@abclate`); the other slot's `+0x5B`
  byte between `0x33C18` and `0x32B00` (`@c5bearly`, `@c5bw` against the seeded `+0x5C`); the seven tail stores
  after `0x32B00` (`@tailearly`), the hook dword whole (`@hookw`).
- `game_mode_05_step` / `game_mode_30_step`: state 1's two bytes before `0x25C1C` (`@s1early`), the dword
  `0x104AC0` whole (`@ac0w`: `p1`'s record `0x18000`, `p2`'s 0), the eight zeroed dwords between seeded dwords
  (`@z7`, `@z9`) and before `0x256F4` (`@zlate`); state 2's `0x104AE8 |= 0x40` before the voice (`@ae8late`)
  with seeds `0x00`, `0x20`, `0x40`, `0xBF`; state 3's `0x104AC0` zeroed before the second `0x2B150`
  (`@z0late`), the mode word `0xC` before the entrance (`@mclate`), the two `0xF0AFE`/`0xF0AFF` bytes after the
  badge (`@tailearly`); state 4's word decremented as a word (`@decb`: `0x0100`, 0, `0x8000`) and the new value
  tested (`@s4old`). Mode `0x30`'s state 3: the two bytes `0x104AF0`/`0x104AF1` (`@af1w` against the seeded
  `0x104AF2`) and the mode word `0x31`.
- `sound_sample_start`: the copy lands before `0x5DC0F` (`@copylate`: `call #0 memory`), in a buffer seeded
  `0x5A + k` with four bytes before it and the bytes after the length; `+0x0C` takes the queued handle before
  `+0x04` is zeroed (`@clrfirst`), both after `0x5DC70` (`@clrearly`: only the memory at a call).

## §6 Queue entries not done

Measured stops are `E.run_original(img, entry)` with no allow-list.

- `0x11BCC` (41 bytes): **no port function to bind**, as the d3a record found: the port inlines it in
  `game_state_step`'s state 7 (`/* 0x11BCC the timer exit. */`). Read against the bytes: the two byte stores
  (`0x11BCE`, `0x11BD4`), the word `0xF0A64 = the word 0xF0A6C` (`0x11BDA`/`0x11BE0`) and the voice `0x100`
  (`0x11BEB`, `jmp 0x2c3fc`) are there in the raw's order; the bare-`ret` `0x29D60` is omitted.
- `0x4FBA2` (25 bytes, `int10h_query`): `unmodeled`, `int at 0x4FBAD` (`mov eax,0xf00; int 0x10; and eax,0xff`
  between six pushes and pops). No gap row added: the named-gap count is pinned in a test this batch does not
  edit.
- `0x1C930` (228 bytes): **a host replacement.** The raw compares the dwords `0x1028CC`/`0x1028DC`, resolves
  the pending handle, copies its bytes into the buffer `0x1028D0` and drives the sequence handle `0x1028C0`
  through `0x5DEAF`, `0x5DE48`, `0x5DECA`, `0x5DE79`. The port's `title_music_start` (static) loads a bound
  resource into its own C sequence object and names none of those dwords. Stops `unmodeled`, `call 0x5DEAF from
  0x1C9BD`.
- `0x1CF40` (215 bytes, `game_audio_init`): the gap the d1c record names (§6): AL/DL gates and ten AIL thunk
  results stored in `mem[]` against a port that takes no argument and keeps host handles. Not re-examined
  beyond the stop: `unmodeled`, `call 0x5D851 from 0x1CF4D`.
- `0x1D018` (161 bytes): **no port function to bind** (a comment inside `game_shutdown`). Seen, not changed,
  as d1c listed: the raw tests the dword `0x1028C4` and the sequence status (`0x1D031`, `0x1D041`/`0x1D049`)
  before stopping it, zeroes `0x1028C4`, `0x1028C0` and the four slot handle dwords (`0x1D05F`, `0x1D065`,
  `0x1D09E`) and stops each sample whose status is not 2; the inline body stops the sequence unconditionally
  and zeroes only `0xA2CB1` and `0x1028C8`. Teardown only.
- `0x1BE30` (147 bytes, `game_shutdown`): every callee is host-owned or deferred (`0x1ADE4` three times,
  `0x4FB98`, `0x1B084`, `0x10D34`) or has no port function (`0x1D018`); the port's body is the inline AIL
  teardown above and nothing else. Nothing to compare. Stops `unmodeled`, `call 0x1D018 from 0x1BE33`.
- `0x47370` (369 bytes, the localisation loader): the gap the d1d record names (DOS file I/O and the
  host-owned pager; the port is a host replacement). Stops `unmodeled`, `call 0x61C60 from 0x473A6`.

The other eight entries of the queue are rowed.

## §7 What is not verified

- The correction §3.1 and the 19 seams are unproven against the capture oracles: `make verify`, `make
  diff-verify` and the `*-oracle` targets were not run (the integrator's gate). `run_tests` passed with `frames/`
  regenerated after the correction (`./build/prageport --game-dir data/game/C --check 820`).
- `python3 -m unittest tools.tests.test_diff_verify` was not run (forbidden to a batch); the pins rest on the
  per-row measurements, the one full self-check and the static replica of §2.2.
- The host-owned or deferred callees are compared nowhere: the run clock `0x32970` and the mask it is handed at
  `0x25ECA`/`0x2955F`, the audit add `0x2DAE4`, the AIL handles.
- Rows that stay open on callees without a row: `frontend_darken_marked` (`0x13D4C`), `flow_continue_take`
  (`0x33B00`, `0x1D764`), `game_mode_05_step` (the three entrances), `sound_sample_start` and every row that
  stubs an AIL thunk or allows a runtime or host-owned address.
- Mask 0 rests on the caller instructions quoted in §1; the mode tail `0x2540F`, `0x41C28`'s return and the
  returns of `0x27FA8`'s callers are taken from the earlier records named there.
