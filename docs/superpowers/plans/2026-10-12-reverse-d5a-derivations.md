# Track D wave 5, batch d5a: game/flow.c's front-end and mode handlers — derivation record

Branch `reverse-d5a` (off `reverse-d4int` `3790f323`). Queue: `.superpowers/sdd/2026-10-11-reverse-d1/d5a-queue.txt`
(12 functions). Ground truth: the fixed-up image dumped by `./build/diffrun --image-out` (`/tmp/pr_d5a_img.bin`),
disassembled with capstone; every address below is linear. Claim of every row: equivalence on the exercised
blocks and inputs only, with its callees stubbed or run as stated.

Eight of the twelve queue entries are rowed (13 rows, 587 mutants); four are not done (§6). Seven port
corrections, each proved by an address and each in its own commit (§3).

## §1 The members

| addr | port function | size | EAX mask and the instructions that fix it |
|---|---|---|---|
| `0x20E90` | `flow_scroll_reset` | 38 | 0: the one caller `0x4256A` goes on with `call 0x42724`, which loads AL from memory (`0x4272C`, `0x42734`) and then the whole register (`mov eax,[0x104ad4]` `0x4273D`) before reading either |
| `0x20DF4` | `game_fight_reset` | 155 | 0: each of the six callers loads EAX before reading it: `0x11ACB` `xor eax,eax`; `0x25A9A` `mov eax,ebx`; `0x25BEB` `xor eax,eax`; `0x269CE` `xor eax,eax` / `0x269DA` `mov eax,1`; `0x270E5` and `0x295E9` `mov eax,[0x10810a]` |
| `0x257A4` | `game_coin_divert` | 132 | 0: the eight game-start sites (`0x24F5C` … `0x25194`) are followed by `jmp 0x2540f`, and `0x11D41` returns (`pop`×3, `ret`) through `0x11D04` to `0x2523D` `jmp 0x2540f`: the mode tail the wave-1 records trace dead (d1c §1, d1d §1; not re-traced here beyond `0x2A31C`'s entry and `0x2545C` `mov ax,[0x104b00]`). `0x11EB8` goes on with `call 0x10db0` (see the next row). `0x11CD4` is a `jmp` from the unentered stub `0x11CC8` (no caller in the image) |
| `0x12484` | `game_state_3` | 468 | 0: the one site `0x11DB8` goes on with `call 0x10db0` and `call 0x10e18`, each of which loads AH (`0x10DB1`, `0x10E19`) and reads EAX only after `mov eax,0x100` (`0x10E01`, `0x10E69`; `0x29D60` before it is a bare `ret`), then `call 0x2bf08` (`mov ah,[0x9ad58]` `0x2BF0E`; `call 0x2caa8` before `test eax,eax` `0x2BF23`; its early exit `0x2C056` is `add esp`, three pops, `ret`), then three pops and `ret` back through `0x11D04` to `0x2523D` `jmp 0x2540f` |
| `0x11F6C` | `game_state_select` | 562 | 0: the one site `0x11DA0`, followed as `0x11DB8` is |
| `0x121A0` | `game_state_title` | 738 | 0: the one site `0x11D88`, followed as `0x11DB8` is |
| `0x208F8` | `game_mode_1f_step` | 785 | 0: the one site `0x253D2` is followed by `jmp 0x2540f` |
| `0x11A8C` | `game_state_6` | 317 | 0: the one site `0x11E4F`, followed as `0x11DB8` is |

**Exports.** `game_state_title`, `game_state_select`, `game_state_3`, `game_state_4` and `game_state_6` were
file-`static` in `flow.c`; they lose `static` and are declared in `flow.h` (the precedent of d1c/d2c) so the
bindings can call them. `game_state_4` (`0x11578`) is exported but not rowed (§6).

**Seams added (19).** Each is the callee's first statement, its arguments the C signature's in order:

| callee | port function | seam |
|---|---|---|
| `0x38730` | `render_scroll_setup` (render.c) | `PR_SEAM(0x38730u, i);` |
| `0x2C390` | `flow_list_105c0c_init` (flow.c) | `PR_SEAM0(0x2C390u);` |
| `0x12750` | `camera_dust_list_init` (camera.c) | `PR_SEAM0(0x12750u);` |
| `0x49300` | `fight_list_init` (fight.c) | `PR_SEAM0(0x49300u);` |
| `0x28E98` | `actor_type_0a19_list_init` (actors.c) | `PR_SEAM0(0x28E98u);` |
| `0x34978` | `fighter_slots_reset` (fighter.c) | `PR_SEAM0(0x34978u);` |
| `0x2C074` | `flow_105bf0_clear` (flow.c) | `PR_SEAM0(0x2C074u);` |
| `0x12C70` | `camera_step_seed` (camera.c) | `PR_SEAM0(0x12C70u);` |
| `0x2BAF4` | `actors_reset_al` (actors.c) | `PR_SEAM(0x2BAF4u, al);` |
| `0x412A0` | `fight_scene_props` (fight.c) | `PR_SEAM(0x412A0u, scene);` |
| `0x46594` | `flow_1082c8_init` (flow.c) | `PR_SEAM0(0x46594u);` |
| `0x4F980` | `frontend_wipe_arm` (flow.c) | `PR_SEAM(0x4F980u, ret_mode);` |
| `0x38B18` | `frontend_spawn_row` (flow.c) | `PR_SEAM(0x38B18u, (u32)((const u8 *)desc - mem), a2, a3);` |
| `0x12658` | `game_state_3_handoff` (flow.c) | `PR_SEAM0(0x12658u);` |
| `0x13E28` | `effects_spawn_pulse` (effects.c) | `PR_SEAM_RET(0x13E28u, source_rec, byte_arg);` |
| `0x4F1E4` | `frontend_input_reset` (flow.c) | `PR_SEAM0(0x4F1E4u);` |
| `0x38910` | `title_origin_reset` (flow.c) | `PR_SEAM(0x38910u, idx);` |
| `0x20DF4` | `game_fight_reset` (flow.c) | `PR_SEAM(0x20DF4u, stage, full);` |
| `0x33EB4` | `fighter_spawn` (fighter.c) | `PR_SEAM(0x33EB4u, side);` |

The clobbers of every stub are `E.callee_clobbers(image, addr)` (checked by script against the image and
against every other spec's `E.Call` of the same address): `0x38730` (edi, ebp), `0x412A0` (ebx, ecx, edx, esi,
edi, ebp), `0x38B18` (ebx, ecx, edx), `0x13E28` (edx), `0x20DF4` (edx, edi, ebp), `0x33EB4` (ebp), the other
thirteen none. All 19 are new stub addresses in the expected dictionary of
`test_each_stub_declares_the_registers_its_callee_clobbers` (two lines at its top, `# D5 d5a`).

**`tools/diff_emu.py`.** `RESOLVED_JUMPS[0x20931] = (0x20924, 0x208E4, 5)`: `0x208F8`'s state switch is `cmp
dl,4; ja 0x20c00; xor eax,eax; mov al,dl; jmp cs:[eax*4+0x208e4]`; the guard compares DL and the jump indexes
by the EAX built from it, which `switch_cases` does not follow (the form of `0x4507A`). The five targets are
`0x20939`, `0x2098D`, `0x20A2C`, `0x20A96`, `0x20B04`. No existing stub's clobber set moves (368 stubs
re-derived with and without the entry). The `WINDOWS` dictionary of
`test_each_resolved_jump_table_matches_the_bytes` gains the line for `0x20931` (after `0x4507A`'s).

## §2 The row table (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `flow_scroll_reset` | `0x20E90` | 3 | 1/1 | 13 | 0 | `0x38730` a stub. The stage goes on whole: `s1` is `0x10008`, `s2` -1 (`@argw`, `@arg7`). The camera block `0xF0AE8..0xF0AFF` is seeded byte by byte |
| `game_fight_reset` | `0x20DF4` | 7 | 5/5 | 46 | 0 | ten callees stubbed, `0x29B70` (a bare `ret`) real on both sides. The signed clamp: 6, 7, 8, -1, `0x10003`; `full` 0, 1, -1, `0x100`, `0x80000000` (`test edx,edx`) |
| `game_coin_divert` | `0x257A4` | 3 | 1/1 | 36 | 0 | `0x2C3FC`, `0x2BAF4`, `0x33C18`, `0x46594`, `0x4F980` stubbed; the runtime fill `0x65490`/`0x654C7` (EDX = 0, ECX = 7 over `0x104B02..08`) runs from the original's bytes, the port fills by hand. `players` `0x12345680` (DL `0x80`) and `0xFFFFFF00` (DL 0) |
| `game_state_3` | `0x12484` | 14 | 21/21 | 65 | 0 | `0x33904` and `0x1C500` real on both sides (`0x1C500` over the stubbed `0x474E4`); `0x2F198` by value. Phase 1's bound on both signs and at `0x8000` / `0x80001E00` (`q0`..`q7`) |
| `game_state_3_rr` | `0x12484` | 1 | 5/21 | 2 | 0 | stubs that write what the raw reads after them: `0x38B18` the option byte `0x104529` = 2 (`0x12527`), `0x12658` the phase byte = 5 (`inc byte [0xf0a6f]` `0x12597`). The port already re-reads both |
| `game_state_select` | `0x11F6C` | 21 | 20/20 | 92 | 0 | a bounded switch (`0x11F58`). `0x29D60`, `0x2C06C`, `0x33904`, `0x1C6D4`, `0x1C500` real on both sides. The entry byte 0, 1, 3, 5 and `0x80` (the tables' entries at index `0x80` poked); the image's own tables `0x9AEC8`/`0x9AEE0` |
| `game_state_select_rr` | `0x11F6C` | 3 | 13/20 | 6 | 0 | stubs that rewrite the entry byte `0xF0A6E` (`0x38B18` -> 2, `0x2AE14` -> 3, `0x2F198` -> 4) and the option byte (`0x38B18` -> 2, `0x13E28` -> 0). MISMATCH on the port before §3.1 |
| `game_state_title` | `0x121A0` | 16 | 21/21 | 119 | 0 | the RNG `0x5D7DC` real (four seeds: both signs of the offset); `0x33904`, `0x1C500` real. Phase 1 at both sides of its unsigned bound and with a zero logo dword (`b6`: the read at `0x32`, the word at `0x2C`, named in `D5A_OUTSIDE`). MISMATCH on `b6` before §3.2 |
| `game_state_title_rr` | `0x121A0` | 2 | 15/21 | 8 | 0 | stubs that rewrite the option byte, the logo dword `0xF0A58`, the dword `0xF0A54`, the words `0xF0A66` and `0x107A50` and the phase byte. The port already re-reads each |
| `game_mode_1f_step` | `0x208F8` | 22 | 22/22 | 127 | 0 | the jump table resolved by hand (§1). Character byte 1..6 and `0x80` on both sides; state 2's signed bound, state 4's two word bounds; the x word `0xC8718` poked to `0x8123` (its image value `0x2A00` equals a constant a port could hard-code) |
| `game_mode_1f_step_rr` | `0x208F8` | 4 | 17/22 | 11 | 0 | stubs that rewrite the state byte, the three record dwords and the option byte. MISMATCH on the port before §3.3 |
| `game_state_6` | `0x11A8C` | 4 | 5/5 | 57 | 0 | the RNG real (seeds 1, 27, 41, 47: sums 10, 6, 7, 1); `0x29D60`, `0x2C06C`, `0x1C500` real. MISMATCH before §3.4 and §3.5 |
| `game_state_6_rr` | `0x11A8C` | 4 | 5/5 | 5 | 0 | the RNG a stub (`0x10003`, `0x40000000`, 9, -1: one EAX serves both draws of a case); `0x1D890` clears the option byte, `0x2F198` stores `0x77` in `0xF0A72`. MISMATCH before §3.6 and §3.7 |

13 rows, 587 mutants, every mutant detected; the exact kinds and case sets are `D5A_KINDS` / `D5A_CASES` in
`tools/tests/test_diff_verify.py` (generated from `verify_all` on `D5A_SPECS`). Each mutant binding's port
output is identical over ten runs (the mem[] bounds check of the brief).

## §3 Corrections (the raw wins; each in its own commit)

1. **`game_state_select` re-reads the entry byte** (`43d224a6`). `0x11F6C` loads `0xF0A6E` after the
   `0x2BAF4`/`0x4F1D0`/`0x38B18` calls (`0x11FFD mov al,[0xf0a6e]`) and again at `0x1206C`, `0x12090` and
   `0x120C3`. Old: one read at the top of phases 0/1. New: a read at each of the four sites. No callee
   writes the byte in the game, so no behaviour changes there.
2. **`game_state_title`'s phase 1 has no null test** (`a5577896`). `0x12402 mov ebx,[0xf0a58]`, `0x12408 mov
   edx,[ebx+0x32]` … `0x1243E mov [ebx+0x2c],ax`: no test of EBX. Old: `if (logo != 0) { … }`. New: both stores
   unconditionally (with a zero dword the port now reads `mem[0x32]` and writes `mem[0x2C]`, as the raw does).
3. **`game_mode_1f_step`'s state 1 re-reads the record dword** (`1ffbbc35`). `0x20A10 mov eax,[0x1044a0]`
   after the second `0x2AE14`, then `0x20A17 mov word [eax+0x36],0xffc0`. Old: `DSW(rec1 + 0x36)`. New:
   `DSW(DSD(DS_001044A0) + 0x36)`.
4. **`game_state_6` stores the dword `0x1082CC`** (`759bb02c`). `0x11B2F mov dword [0x1082cc],ebx` (EBX = 3).
   Old: `DSW`. New: `DSD`. A real difference whenever the dword's high word is non-zero.
5. **`game_state_6` copies the byte `0xF0A72`** (`7447b3e3`). `0x11B9F xor ah,ah`, `0x11BA6 mov
   al,[0xf0a72]`, `0x11BB9 mov word [0xf0a6c],ax`. Old: `DSW(DS_000F0A6C) = DSW(DS_000F0A72)` (the byte
   `0xF0A73` became the high byte). New: `= DSB(DS_000F0A72)`. A real difference whenever `0xF0A73` is non-zero.
6. **`game_state_6`'s wrap is signed** (`e5190876`). `0x11AF1 cmp edx,7`, `0x11AF4 jl`. Old: `if (p1 >= 7u)`.
   New: `if ((s32)p1 >= 7)`. Equal for every sum the real RNG gives (0..11); shown under a stubbed RNG only.
7. **`game_state_6` passes the stage word** (`1cf7c75b`). `0x11ABA xor eax,eax`, `0x11AC1 mov ax,bx`, `call
   0x20df4`. Old: `game_fight_reset(draw1, 1)`. New: `game_fight_reset((u32)(u16)draw1, 1)`. Equal for every
   draw the real RNG gives (0..6); shown under a stubbed RNG only.

Each mismatch is reproduced by a mutant of the corrected row: `@pren1`..`@pren4` (1), `@guard` (2), `@pre4a0`
(3), `@2ccw` (4), `@6cw` (5), `@p1uns` (6), `@stagefull` (7). None is proved against the oracles here: the
integrator's full gate is the first run of them.

## §4 Named gaps, limits and unhit blocks

- **Unhit blocks: none in the plain rows.** The four `_rr` rows with unhit blocks name them
  (`unhit_named`): each runs only the cases whose callees can rewrite something, and the plain row of the
  same entry hits every block.
- **`0x2F198` is compared by value**, on the string's first sixteen bytes (the existing seam). The strings
  here come from the stubbed `0x474E4` (`INSERT `), so the id is what the call list separates, in
  `0x474E4`'s own arguments.
- **`0x1C6D4`'s AL** (`test al,al` `0x1203D`): the callee runs real on both sides, so a stub EAX of `0x100`
  is not available; `game_state_select@al` ORs the upper bytes the raw's EAX carries and is caught.
- **The RNG in `0x121A0`** runs real: `rng(2)` is 0 or 1, so the `test eax,eax` at `0x122BC` is not
  separated from a test of AL.
- **`game_state_6_rr`'s RNG stub** returns one EAX for both draws of a case.
- **Equivalent mutants dropped, with the reason:** `game_state_select@6e0w` (a word store of 0 at `0xF0A6E`
  also zeroes the phase byte, which is 0 whenever phase 0 runs); `game_state_select@s2sext` (the compare is
  `==`); `game_mode_1f_step@36rec2` in the plain row (the stub returns one record for both spawns; the mutant
  is in the `_rr` row, where it is caught).
- **`0x121A0`'s division** `0x40000 / the word 0xF0A66` (`idiv ecx` `0x1243C`) is never run with a zero
  divisor: the raw faults there.

## §5 Store-sweep findings

Every field a row writes has a seed that differs from the value written, between seeded neighbours. Three
fixture holes were found by an undetected width mutant and closed before pinning:

- `game_state_title@58w` / `@54w`: the phase-0 seeds of the dwords `0xF0A58`/`0xF0A54` had the high word of
  the stub's record (`0x0010`); the phase-0 cases now seed `0x5D5D5D5D` / `0x5E5E5E5E`.
- `game_mode_1f_step@4a0w`: the same hole on the dword `0x1044A0`; state 1's cases seed `0x0020A300` /
  `0x0030A340` (inside mem[]: a mutant that drops the store then follows the seed).
- `game_state_6@6csext`: the byte `0xF0A72` was positive in every case; `s2` has `0x80`.

The runner holds 40 pokes per case: `d5a_roar_rec` writes a record's `+0x2A..+0x39` as one poke.

## §6 Queue entries not done

- **`0x1EA08` `frontend_match_start`** (559 bytes). Not rowed; out of time. Derived so far: the names are built
  in the raw's own stack frame (`mov ebx,esp` `0x1EA96`, `0x1EB96`, `0x1EBCC`) and handed to `0x2F4BC`, whose
  seam reports a mem[] offset, so `0x2F4BC` has to run real on both sides over a `0x2F198` stubbed by value;
  `0x2DBC4` returns the one buffer `0x105EFC`, so it has to run real over poked tables (d1i's fixtures) for
  the ten records to differ. The port's `if (rec != 0u)` before `0x2A17C` (`0x1EC28`) is a marked `PORT:`
  deviation the row would name.
- **`0x27A2C` `game_mode_0e_step`** (374 bytes), **`0x11578` `game_state_4`** (1207 bytes; exported in
  `flow.h` for a later binding) and **`0x11000`** (1380 bytes, attract.c): not started.
