# Reverse D1 batch d1h: rows for attract.c, camera.c, camera.h and effects.c (record)

**Scope.** Track D1's batch `d1h`: the 24 queue entries of
`.superpowers/sdd/2026-10-11-reverse-d1/d1h-queue.txt`. 23 are rowed here; one (`0x12CD4`) has no C
function of its own and is not rowed (§6). Recipe: E3 record §E3.10/§E3.12; checklist:
`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**Status of the numbers.** Measured on 2026-10-11 on `reverse-d1h` (base `reverse-d1` `b4d0ad4`). The
image is `build/diffrun --exe data/game/C/PRAGE.EXE --image-out FILE`, sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's, E3's and every later batch's). Every address is
capstone 5.0.7 over that image (fixups applied); `unicorn` 2.1.4 runs the original side at the game's
x87 control word `0x127F`. Ghidra was not consulted. The caller analysis of §1 is a path walk over the
image (a scratch script, not committed): from each direct call site's return address it follows every
path, tracking which parts of EAX (AL, AH, bits 16-31) still hold the callee's value; a write kills the
parts it covers, a read of a live part is reported, a `call` is entered, a `ret` goes up to every direct
caller of the enclosing function. "dead" below means no path reads a live part; the instructions named
are the last ones that kill it.

---

## §1 The members

| addr | port function | size | who reads its EAX (fixes the mask) |
|---|---|---|---|
| `0x10F28` | `attract_voice_tick` | 128 | nobody: `0x11559` then `pop`s and `ret` (`0x11563`); `0x11000`'s one caller `0x11D70` goes on to `0x10DB0`/`0x10E18`/`0x2BF08`, which load EAX before any read (`0x10E01 mov eax,0x100`, `0x25550 xor eax,eax`, ...). The bytes `00 10 01 00` at `0xBE1FF` are not a pointer to `0x11000`: they straddle the dwords `0`, `0x110` of the table at `0xBE200` (`0x110, 0x112, 0x114, 0x111`). Mask 0 |
| `0x12750` | `camera_dust_list_init` | 77 | nobody: `0x20E38`/`0x20EDF` call `0x49300`, which loads EAX (`0x4933D mov eax,0x1083c4`, `0x49354 xor eax,eax`). Mask 0 |
| `0x1282C` | `camera_dust_spawn` | 166 | nobody: `0x263A5`/`0x26534`/`0x26691` call `0x12DA8` (`0x12DBA lea eax,[edx*8]`, `0x12DD2 mov eax,[0x1077e0]`); `0x26864` calls `0x1DA08` (`0x1DA2B mov eax,[ecx+0x1028f8]`), then `0x26882 and eax,0xffff` after `mov ax,[0xef6dc]`. Mask 0 |
| `0x12C70` | `camera_step_seed` | 10 | nobody: `0x20E73 mov eax,1` on one arm, the callers of `0x20DF4` on the other (`0x11ACB`, `0x25A9A`, `0x25BEB`, `0x269CE`, `0x269DA`, `0x270E5`, `0x295E9` all load EAX). Mask 0 |
| `0x12C7C` | `camera_x_commit` | 85 | nobody: `0x12FBA` returns through `0x12E3C` and `0x12D48` to `0x25436 mov eax,ebx` / `0x25550 xor eax,eax` / `0x33F15 mov eax,[ebx+0x1077a8]`. Mask 0 |
| `0x12CD4` | (inlined in `camera_y_clamp`) | 93 | not rowed, §6 |
| `0x12DF0` | `camera_mode_track_player` | 74 | nobody: as `0x12C7C` (`0x12D5E`, then `0x12D48`'s callers). Mask 0 |
| `0x12FD8` | `camera_pair_hold` | 346 | nobody: `0x2551C` / `0x2559C mov eax,ebx`. Mask 0 |
| `0x1324C` | `camera_shake_decay` | 67 | nobody. No direct caller; the one reference is the update table's entry 0 (`0xA8644`), called at `0x24CEF call [eax+0xa8644]`. After it: `shr ebx,1; add edx,4; test ebx,ebx; jne 0x24ce8`, whose loop arm is `mov eax,edx` (`0x24CED`) and whose exit arm is `mov ah,1` (`0x24D08`) then `int 0x16` (the BIOS keyboard service, function AH = 1, which takes no AL input). Mask 0 |
| `0x13290` | `camera_mode_center_two` | 169 | nobody: as `0x12C7C` (`0x12D6C`). Mask 0 |
| `0x1333C` | `camera_mode_center_one` | 134 | nobody: as `0x12C7C` (`0x12D73`). Mask 0 |
| `0x13ADC` | `effects_init` | 96 | nobody: `0x20CE6` calls `0x10E80` (`0x10E82 xor eax,eax`); `0x2BBC0`, `0x2F9D9 xor eax,eax`. Mask 0 |
| `0x14080` | `camera_rect_clip` | 97 | AL, through `0x140E4`'s `ret`: `0x1769D`, `0x1774D`, `0x17BEF test al,al`. Both exits load the whole EAX (`mov eax,1` `0x140D0`, `xor eax,eax` `0x140DA`): full mask |
| `0x15EC0` | `camera_bitrev` | 104 | nobody: `0x16F2E mov eax,[esp]`; `0x16F94`/`0x16FD4 mov eax,[0x100b38]`. Mask 0 |
| `0x15F48` | `camera_bitplane_pixel` | 97 | nobody: `0x173DF`/`0x17A5B xor eax,eax`, `0x173F0`/`0x17A6A mov eax,[esp+..]`, `0x17D0C`, `0x17DF9`, `0x17E0C xor eax,eax` and the loads behind them. Mask 0 |
| `0x15FD4` | `camera_bitplane_shift` | 423 | nobody (`0x173F0`, `0x17A6A mov eax,[esp+..]`). Every exit sets AL (`xor al,al` `0x15FF9`/`0x1601A`, `mov al,1` `0x16170`); the row compares AL: mask 0xFF |
| `0x1617C` | `camera_bitplane_merge` | 396 | nobody (`0x17001 mov eax,0x100b89`). Every exit is `mov al,dl` (`0x161BC`, `0x162FD`): mask 0xFF |
| `0x1638C` | `camera_sprite_row` | 65 | nobody: `0x16E8B`/`0x16EB7`/`0x16EE5 mov eax,ecx`, `0x16F0B mov eax,[esp]`, `0x16F13 mov eax,[esp+0x2c]`. Mask 0 |
| `0x164C0` | `camera_frame_last` | 40 | AL: `0x165E6`, `0x16C53 test al,al`. `sete al` (`0x164E3`) sets AL only; the rest of EAX is the record pointer and the status word. Mask 0xFF |
| `0x16734` | `camera_sprite_code` | 966 | the whole EAX: `0x16B0D mov edi,eax`, `0x16B0F cmp eax,-1`. Every exit is `mov eax,imm`: full mask |
| `0x16D58` | `camera_screen_base` | 73 | nobody: each of its 22 call sites is followed by a load of EAX before any read (`mov eax,1`, `mov eax,[0x1077e4]`, `xor eax,eax`, `lea eax,[esi*8]`). Mask 0 |
| `0x17EEC` | `camera_char_const` | 107 | the whole EAX at all ten sites (`add eax,ecx` `0x15B9E`, `add eax,edx` `0x1630D`, ..., `mov [esp+0x14],eax` `0x17FBB`). Every exit is `xor eax,eax; mov ax,word`: full mask |
| `0x181D0` | `camera_sync_visible` | 219 | AL at all ten sites (`and eax,0xff` `0x17367`, `0x17441`, `0x174EA`, `0x17849`, `0x17883`, `0x179ED`, `0x17AB9`, `0x17B10`, `0x17C3A`, `0x17C6A`). The raw sets AL only (`mov al,N`, `xor al,al`). Mask 0xFF |
| `0x2C9B8` | `config_voice_gate` | 20 | nobody (`0x2FA28 mov eax,0xffffffff`, `0x30E74 mov eax,0x35`). Both exits load the whole EAX (`mov eax,0x10000`, `xor eax,eax`), which the C returns: full mask |

Register arguments, from each entry's first reads: `0x12C7C` EAX; `0x12FD8` AL (`test al,al` `0x12FE1`);
`0x14080` EAX/EDX/EBX; `0x15EC0` EAX = len, EDX = buf; `0x15F48` EAX = bit, EDX = rows, EBX = dst, CL = value;
`0x15FD4`/`0x1617C` EAX = bit, EDX = src, EBX = rows, ECX = dst, `[esp+0x34]` after the three pushes and
`sub esp,0x24` = the one stack dword (`ret 4`); `0x1638C` EAX = side, EDX = dst, EBX = count, ECX = row;
`0x164C0`, `0x16734`, `0x17EEC` EAX = side; `0x16D58` AX, DX; `0x181D0` EAX, EDX, EBX, ECX and four stack
dwords (`[esp+0x10]`..`[esp+0x1c]` after three pushes, `ret 0x10`); `0x2C9B8` EAX. The others read no
register.

## §2 The rows (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `attract_voice_tick` | 0x10F28 | 6 | 8/8 | 8/8 | 0 | `0x2C3FC` and `0x5D7DC` stubs; the rng stub's EAX per case 0, 1, 0x100, 0x12345 (the pick `test eax,eax` `0x10F7B` and both `mov word,ax` reloads) |
| `camera_dust_list_init` | 0x12750 | 2 | 3/3 | 6/6 | 0 | `0x249C0` real (recorded: eight `(0xF0A78, node)` calls and the memory at each) |
| `camera_dust_spawn` | 0x1282C | 9 | 7/7 | 11/11 | 0 | `0x5D7DC` real (three draws from the case's seed), `0x2AE14` stub returning the record (two addresses) |
| `effects_init` | 0x13ADC | 2 | 3/3 | 7/7 | 0 | `0x249C0` real: 24 `(0xFCCE8, rec)` calls; the lock byte `0x9AF3C` = 1 is seen in the memory at each call |
| `camera_step_seed` | 0x12C70 | 2 | 1/1 | 3/3 | 0 | — |
| `camera_x_commit` | 0x12C7C | 9 | 9/9 | 7/7 | 0 | — |
| `camera_mode_track_player` | 0x12DF0 | 7 | 9/9 | 6/6 | 0 | — |
| `camera_shake_decay` | 0x1324C | 8 | 4/4 | 8/8 | 0 | — |
| `camera_mode_center_two` | 0x13290 | 13 | 20/20 | 11/11 | 0 | — |
| `camera_mode_center_one` | 0x1333C | 10 | 16/16 | 10/10 | 0 | — |
| `camera_pair_hold` | 0x12FD8 | 13 | 21/21 | 12/12 | 0 | `0x18714` stub (`C1_RECORD_X`), its EAX per case |
| `config_voice_gate` | 0x2C9B8 | 5 | 3/3 | 6/6 | 0xFFFFFFFF | `0x2C3FC` stub (its EAX is overwritten, `0x2C9C3`) |
| `camera_char_const` | 0x17EEC | 12 | 9/9 | 5/5 | 0xFFFFFFFF | jump table `0x17ED0`, every target; the binding reads the slot's +0x7A (§4) |
| `camera_screen_base` | 0x16D58 | 10 | 6/6 | 8/8 | 0 | — |
| `camera_frame_last` | 0x164C0 | 11 | 1/1 | 6/6 | 0xFF | x87; corrected (§3) |
| `camera_sprite_code` | 0x16734 | 73 | 102/104 | 9/9 | 0xFFFFFFFF | `0x33A10` allow; jump table `0x16718`, every target; two dead blocks named (§4) |
| `camera_rect_clip` | 0x14080 | 10 | 13/13 | 6/6 | 0xFFFFFFFF | — |
| `camera_bitrev` | 0x15EC0 | 10 | 5/5 | 5/5 | 0 | corrected (§3) |
| `camera_sprite_row` | 0x1638C | 9 | 4/4 | 5/5 | 0 | — |
| `camera_bitplane_pixel` | 0x15F48 | 13 | 6/6 | 7/7 | 0 | `0x61A70`, `0x65490`, `0x654C7` allow (the libc memset wrapper and its body; the port fills through `mem_fill`) |
| `camera_sync_visible` | 0x181D0 | 18 | 21/21 | 9/9 | 0xFF | four stack arguments, `ret 0x10` |
| `camera_bitplane_shift` | 0x15FD4 | 27 | 32/32 | 8/8 | 0xFF | one stack argument, `ret 4`; seven cases in place (dst == src, the raw callers' shape); one input class named (§4) |
| `camera_bitplane_merge` | 0x1617C | 21 | 33/33 | 8/8 | 0xFF | one stack argument, `ret 4` |

23 rows, 171 mutants. The exact kinds and case sets that catch each mutant are `D1H_KINDS`/`D1H_CASES` in
`tools/tests/test_diff_verify.py`, written from a measurement of the final tree (not by hand).

**The seam.** `effects_list_insert_before` (`0x249C0`, effects.c) opens with
`PR_SEAM(0x249C0u, at, rec)`: the callee reads EAX (`mov ebx,[eax+4]` `0x249C1`) and EDX
(`mov [eax+4],edx` `0x249C4`), plain `ret`, saves and restores EBX, clobbers nothing. The rows that call it
run it `real` (`E.Call(0x249C0, ("eax", "edx"), mode="real")`), so no stub declaration is added to the
pinned stub table. Its own row (`list_insert_before`) and the rows that allow it do not record it and are
unchanged.

**Stubs reused.** Only callees already in the pinned stub table are stubbed, with the same clobbers:
`0x2C3FC` (`VOICE`), `0x5D7DC` (`("eax",)`, no clobber), `0x2AE14` (`SPAWN`), `0x18714` (`C1_RECORD_X`).

**Exports.** Twelve `static` functions of camera.c lose `static` and gain a prototype in camera.h so
their bindings can call them (the C3 batches' "exported for its own row's binding" form): `camera_x_commit`,
`camera_mode_track_player`, `camera_mode_center_two`, `camera_mode_center_one`, `camera_frame_last`,
`camera_rect_clip`, `camera_bitrev`, `camera_sprite_row`, `camera_bitplane_pixel`, `camera_sync_visible`,
`camera_bitplane_shift`, `camera_bitplane_merge`. No
header `/* 0xADDR` is new to the tree's set (`port_progress.py` unchanged) and nothing is registered.

## §3 Corrections (raw over port)

**C1. `camera_frame_last` (`0x164C0`): the sum's precision.** The raw is `fld dword [eax+0x24]`
(`0x164D4`), `fadd dword [0x80580]` (`0x164D7`), `fcomp dword [eax+0x20]` (`0x164DD`): the sum stays on
the x87 stack at the game's control word `0x127F` (53-bit precision) and is compared without being
rounded to 24 bits. The port computed `float a = f24 + k` and compared floats. Measured before the fix:
`camera_frame_last: f3: eax (mask 0xFF): original 0x0, port 0x1` (+0x24 = 1e-10f, +0x20 = -1.0f: the
x87 holds -0.9999999999, a float sum rounds to -1.0). The port now adds in `double`, which holds the
sum of two floats exactly as the 53-bit x87 does (no result of two floats is a double denormal).
Cases f3 (1e-10 - 1), f4 (2^24 + 2 - 1 against 2^24) and f5 (2^24 + 4 - 1 against 2^24 + 4) separate the
two; mutant `@float` is the old sum (caught on f3, f4, f5).

**C2. `camera_frame_last` (`0x164C0`): the unordered compare.** `fnstsw ax` (`0x164E0`), `sahf`
(`0x164E2`), `sete al` (`0x164E3`): `sete` reads ZF, which `fcomp` sets for "equal" and for "unordered"
(C3 = 1 in both). So a NaN on either side returns 1. The port's `a == b` returned 0. The port now
returns `!(a < b || a > b)`. Cases f9 (+0x24 NaN) and f10 (+0x20 NaN): the original returns AL = 1 on
both; mutant `@nan` is the old ordered `==` (caught on f9, f10). This was found by reading the bytes
while fixing C1; the old port was not run on f9/f10, the mutant stands for it.

**C3. `camera_bitrev` (`0x15EC0`): the swap loop's bound is signed.** The half length is
`(len - (len >> 31)) >> 1` (`0x15ECE`..`0x15ED5`), tested by `test eax,eax; jle 0x15f01`
(`0x15EDB`/`0x15EDD`): a negative or zero half skips the loop. The port held the half length in a `u32`
(`(u32)((s32)len >> 1)`) and looped `i < half` unsigned, so a negative len would have swapped about 2^31
byte pairs and left `mem[]`. That cannot be measured (the port side must stay inside `mem[]`), so there
is no MISMATCH line and no mutant reproduces the old behaviour; the fix is by reading. The port now uses
`s32 half = (s32)len / 2`. Cases v7, v8, v9 (len -1, -2, -3) verify the fixed function: no swap, and
the odd tail (`idiv`, `test edx,edx; je` `0x15F0D`..`0x15F11`) reverses the byte at `buf` for -1 and
-3. For len >= 0 the old and new code are the same.

None of the three can move a capture oracle on its face (frame counters are small integers, lengths
are positive), but **no oracle was run on this branch**: the corrections are unproven against the
oracles until the integrator's `make verify`. C1+C2 and C3 are separate commits.

## §4 Named gaps, unhit blocks and limits

- **`camera_sprite_code`, blocks `0x1695D` and `0x1696D` (dead).** `0x1694A cmp eax,0x1752` /
  `0x1694F jne 0x1695b` reaches `0x1695B jne 0x16967` with ZF clear, so it is always taken; `0x16967
  jne 0x16af0` likewise. The two `mov eax,0xd6` blocks cannot run. Named in `unhit_named`.
- **`camera_bitplane_shift`, bit == 0x128.** The raw's arm (`0x1603B`..`0x1604A`) sets the byte count
  to 0x25 and `[esp+0x10]` to `arg5 ^ rows` but leaves the bit index `[esp+0x14]` unset: it is read
  at `0x16096` from whatever the stack holds. The port uses 0 and the emulator's stack is zero. With
  rows = 0x25 (case j13) the byte shift then writes 0 to every one of the `rows` destination bytes,
  whatever the bit index was, so the case's expectation does not rest on the unset slot. rows > 0x25
  at bit 0x128 does depend on it and is **not cased** for this function. Neither raw caller can pass
  0x128: both load the bit count as a zero-extended byte (`xor eax,eax; mov al,dl` `0x173DF`/`0x173E2`,
  `xor eax,eax; mov al,[esi]` `0x17A5B`/`0x17A5F`). `0x1617C` clears the index explicitly
  (`0x161E2`/`0x161E8`), so `camera_bitplane_merge` cases rows = 0x26 at bit 0x128 (m20).
- **`camera_char_const`: the slot read is the binding's.** The raw takes EAX = side and reads
  `byte [side*0x94 + 0x10782a]` (`0x17EF9`); the port function takes the character, which each port
  caller reads. The binding does that read, so the row proves the table selection (`cmp al,6; ja`,
  unsigned; the jump table) and the zero-extended word, not the port callers' slot index. `@side`
  mutates the binding's own read and shows only that the cases would see a wrong side.
- **`camera_mode_center_one`: the index byte's extension.** `xor ebx,ebx; mov bl,[0x10810d]` is a
  zero extension; an index >= 0x63 puts `0x1077B0 + idx*0x94` past the image's end (`0x10B0D0`), so no
  case can tell it from a sign extension. Indexes 0, 1 and 2 are cased. (`camera_mode_track_player`'s
  stride is 4, so its index 0x80 is cased, k5.)
- **`camera_dust_spawn`: the first draw's width and `movsx ecx,dx`.** With the rng real, rng(7) is
  0..6 and rng(0x1300) < 0x1300, so the `test al,3` / `test al,4` reads and the 16-bit sign extension
  of the second draw (`0x128A8`) cannot differ from wider reads. A stubbed rng would return one value
  for all three draws, making the y argument `0x1300 - r1 + r2` a constant; the real rng was chosen
  because it separates the draws' roles (`@order`, `@r1`, `@a4`).
- **`camera_dust_spawn`: the PORT guard.** The port skips the `+0x34` store when the spawn returns 0;
  the raw stores at `[eax+0x34]` unconditionally (`0x128C5`). A stub EAX of 0 would make the original
  write outside the image, so it is not cased.
- **Signed wrap is not cased** in `camera_x_commit`, `camera_mode_track_player`,
  `camera_mode_center_two`/`_one`: a target and a camera x more than 2^31 apart make the port's `s32`
  subtraction and negation undefined behaviour in C (the build has no `-fwrapv`), while the raw wraps.
  The camera x is clamped to ±0x5D00 after each mode body (`0x12D78`..`0x12D9A`); the cases keep
  every difference far inside 2^31.
- **Overlapping buffers.** `camera_bitplane_shift` is cased in place (p0..p6, dst == src), the
  shape of both raw callers (`mov ecx,0x100bd3` ... `mov edx,ecx`, `0x173DA`/`0x173E4` and
  `0x17A53`/`0x17A5D`); partial overlaps are not cased. `camera_bitplane_merge` is cased with separate
  buffers only, as its callers pass them (src `0x100BAE`/`0x100B64`, dst `0x100BF8`). A destination
  overlapping the source is **not cased** in `camera_sprite_row`: the raw copies by dwords (`rep
  movsd` then `rep movsb`, the `F2` prefix at `0x163BF`/`0x163C6`), the port by bytes, and the callers
  pass the fixed buffers `0x100BAE`/`0x100B64` (`0x16E61`, `0x16F06`), which do not overlap the row
  tables `0xFD160`/`0xFEDE0`.
- **`camera_sync_visible`: the five out pointers are distinct in every case**, as in the callers.
- **`camera_bitplane_pixel`'s row is open** in the counter: its allowed callees `0x61A70`, `0x65490`,
  `0x654C7` are runtime (>= 0x5D000) and have no row.

## §5 The store sweep

Every field a row's function writes is seeded with a value different from what it writes, with seeded
neighbours; for each row some mutant drops or misplaces a store and is caught by a `byte` difference
(or the memory at a call): `attract_voice_tick@store`/`@early`, `camera_dust_list_init@inuse`,
`effects_init@active`/`@unlock`/`@flag` (`@flag`, the lock byte set to 1, is caught **only** by the
memory at the recorded `0x249C0` calls, and only on e0, where the byte starts 0: the image itself holds
1 at `0x9AF3C`), `camera_dust_spawn@field`/`@width`, `camera_step_seed@byte`/`@width`,
`camera_x_commit@noseed`/`@reseed`/`@width`, `camera_shake_decay@keepvel`/`@keepoff`/`@swap`,
`camera_mode_center_two@mode`/`@gate`, `camera_mode_center_one@mode`, `camera_pair_hold@x34`/`@x2c`/`@rec`,
`camera_screen_base@second`/`@stride`, `camera_rect_clip@top`, `camera_bitrev@mid`,
`camera_sprite_row@count`, `camera_bitplane_pixel@idx`/`@aligned`/`@fill`,
`camera_sync_visible@p57`/`@p68`/`@rest`/`@vis`, `camera_bitplane_shift@tail`/`@zero`,
`camera_bitplane_merge@tail`/`@zero`. `camera_mode_track_player` has one store (the camera x), which
every one of its mutants changes.

Findings of the sweep (cases that were too weak, found because a mutant went undetected, then fixed):

- `camera_mode_track_player@sext` was not detected: case k5 (index 0x80) had target 0, the same value a
  sign-extended index reads from the zero bytes at `0x1075A8`. k5's target is now `0x13000`.
- `camera_mode_center_two@slot` and `@mid` were not detected: no case put the midpoint and one record
  on different sides of the 0x100 threshold. Cases t11 and t12 (one record at the camera, the other
  0x2000 away) were added.
- Three planned mutants were dropped as equivalent, not hidden: the dead-zone edge of `0x12DF0`
  (`mag == 0x1800` adds 0 either way), the `p2 == end` edge of `0x181D0` (both arms write 0 and 0), and
  `vis <= 0` for `vis == 0` in `0x181D0` (the guards make the extent non-negative).

Every binding and mutant was run ten times through `diffrun` with identical output (the mem[]-bounds
check of the brief): no index sentinel leaves `mem[]`; "other side" mutants use `1 - (side & 1)`.

## §6 Queue entries not done

- **`0x12CD4`** (93 bytes, the camera-y stepper). The port has no function for it: its body is inlined
  in `camera_y_clamp` (`0x1317C`, camera.c, the block commented `0x12CD4`). A row needs a C function to
  bind, and splitting one out is a port refactor with a new `/* 0x12CD4` header, which this track's
  limits exclude. Read against the raw instead (not measured): `diff = target - [0xf0aec]`, snap when
  `|diff| <= 0x100` (`jg 0x12cf9`), else step by ±0x100 by the sign of diff (`jle 0x12d17`), then add
  `[0xf0af2] >> 16` (the sign-extended word at `0xF0AF4`) and store; the inlined block does the same.
  Its caller `0x131CB` kills EAX at `0x131D0`.
