# Track D wave 2 batch d2d — derivation record (2026-10-11)

Differential rows for the already-ported functions of the d2d queue
(`.superpowers/sdd/2026-10-11-reverse-d1/d2d-queue.txt`, 31 entries: `game/camera.c`, `game/actors.c`
(the fighter callbacks live in `game/fighter.c`), `game/config.c`, `platform/gfx.c`, `platform/input.c`,
the last 9 wave-1 leftovers). Every value below is measured on this branch (`reverse-d2d`), image
`./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_d2d_img.bin`, fixups applied, one row
at a time with `--function <name> --self-check`. The full `--self-check` and the full unittest were
not run (the integrator's).

Narrow claim, as for every row: equivalence on the exercised blocks and inputs only, each function
with its callees stubbed or run as stated.

**A constraint that shaped the call sets.** A callee may be stubbed only when its address is already in
the expected-clobber table of `test_each_stub_declares_the_registers_its_callee_clobbers`
(`tools/tests/test_diff_verify.py`): adding a stub address means editing that existing line, which this
batch's limits exclude. A callee that is not in the table therefore runs on both sides
(`allow_calls`), its own row proving it; the cases then drive its result through its real inputs, and
"a stub's AL = 0 with the upper bits set" cannot be cased for it. Each such callee is named in §2.

## §1 The members and the callers that fix each EAX mask

Masks: 0 = no caller reads EAX; 0xFF = AL; 0xFFFFFFFF = the whole register.

| addr | port function (file) | size | mask | the caller instructions |
|---|---|---|---|---|
| `0x12BB8` | `camera_dust_burst` (camera.c) | 181 | 0 | four call sites, all in `0x129FC`: `0x12AA3`, `0x12AFF`, `0x12B51`, `0x12B9D`, each followed by `mov eax,1` (`0x12AA8`, `0x12B04`, `0x12B56`, `0x12BA2`) |
| `0x12E3C` | `camera_mode_track_pair` (camera.c) | 409 | 0 | one call site `0x12D65` in `0x12D48`, which returns to `0x25427` (`xor ebx,ebx` .. `mov eax,ebx` `0x25436`; the `je` arm reaches `call 0x33f08`, `mov eax,[ebx+0x1077a8]` `0x33F15`) and to `0x25550` (`xor eax,eax`): record d1h §1, re-read |
| `0x14B90` | `fighter_14b90` (fighter.c) | 261 | 0 | every path ends `mov al,1` (`0x14C8D`) and the port function is void. `0x14CAC` (in `0x14C98`): `mov eax,[esp]` (`0x14CB1`). The reaction dispatcher `call [esp+0x24]` (`0x35045`, the dword at `0xA4720`) returns the callback's AL from `0x34E2C`; `0x3CE58` overwrites it (`0x3CF33`), as for the sibling callbacks (record P1 §P1.4). The constant AL is not compared |
| `0x15B90` | `camera_box_palette` (camera.c) | 159 | 0 | one call site `0x15D23`, followed by `mov eax,[edi+4]` (`0x15D28`) |
| `0x164F4` | `camera_page_tail_b` (camera.c) | 547 | 0xFF | `mov al,1` (`0x1669E`, `0x16701`) / `xor al,al` (`0x1670C`); one call site `0x18108`, `test al,al` (`0x1810D`) |
| `0x16AFC` | `camera_page_tail_a` (camera.c) | 601 | 0xFF | `mov al,1` (`0x16B5B`, `0x16CF4`, `0x16D3F`) / `xor al,al` (`0x16D4A`); one call site `0x180C9`, `test al,al` (`0x180CE`) |
| `0x21458` | `fighter_21458` (fighter.c) | 344 | 0 | the slot `+0x10` callback (the dword at `0x21A24`): `call [ecx+0x10]` (`0x354E2`) ends `0x3531C`, whose one caller `0x35803` overwrites EAX (`mov eax,ebx` `0x3580E`) |
| `0x2AA70` | `frame_timer` (actors.c) | 525 | 0 | `0x2A251`: `mov ax,[ebx+0x28]` .. `xor ah,ah; and al,4; and eax,0xffff` (`0x2A256..0x2A267`); the recursion `0x2AB9B`: `mov ax,[ebx]; xor al,al; and ah,0x80; and eax,0xffff` (`0x2ABB3..0x2ABBB`) |
| `0x2C060` | `config_credit_ready` (config.c) | 10 | 0xFFFFFFFF | the tail's `setne al; and eax,0xff` (`0x2CA3C/0x2CA3F`); all eight callers `test eax,eax` (`0x11F30`, `0x27B63`, `0x28CE4`, `0x42618`, `0x42678`, `0x426DB`, `0x42F68`, `0x432AA`) |
| `0x2DBC4` | `hiscore_read` (config.c) | 219 | 0xFFFFFFFF | `mov eax,0x105efc` (`0x2DC8F`) or the 0 of `0x2DB58` (`test eax,eax; je 0x2dc97`): the pointer is the result |
| `0x2DDE4` | `hiscore_rank_probe` (config.c) | 177 | 0xFFFFFFFF | `mov eax,0xffffffff` (`0x2DE07`, `0x2DE7C`) / `mov eax,edi` (`0x2DE8A`): the rank is the result |
| `0x30788` | `config_bar_draw` (config.c) | 220 | 0 | six call sites, each loads EAX before reading it: `mov eax,[esp+0x10]` (`0x30947`), `mov eax,ecx` (`0x30966`), `mov eax,5` (`0x3097E`), `mov eax,[esp+4]` (`0x30D7A`), `mov eax,ebp` (`0x30D99`); after `0x30DA3`: `mov eax,[esp]` (`0x30DAC`), `mov eax,[esp+4]` (`0x30DBD`), `mov eax,edx` (`0x30DD7`), `mov eax,0x22` (`0x30E19`), `mov eax,3` (`0x30E3D`), or `0x30B45`'s `call 0x2ea74`, which opens with `xor eax,eax` |
| `0x31E28` | `config_option_row` (config.c) | 251 | 0 | `0x3216C`: `mov eax,1` (`0x32173`); `0x32182`: `mov eax,ebx` (`0x3218E`); `0x32190`: `xor eax,eax` (`0x3219E` / `0x321A9`); `0x32438`: `mov eax,1` (`0x3243F`); `0x32446`: `mov eax,[0x101514]` (`0x3244B`) |
| `0x3E6A8` | `fighter_3e6a8` (fighter.c) | 344 | 0 | the slot `+0x10` callback (the dword at `0x3ECBF`), as `0x21458` |
| `0x3F9C8` | `fighter_3f9c8` (fighter.c) | 446 | 0 | the slot `+0x0C` callback (the dword at `0x3FBC1`): `xor eax,eax` (`0x35434`) after `0x35431`; `0x384D9` returns through `0x38434` to `mov eax,esi` (`0x38570`) (record d1b, the `0x3F720` row) |
| `0x3FDD8` | `fighter_3fdd8` (fighter.c) | 206 | 0 | the slot `+0x1C` callback (the dword at `0x3FF81`): the one `call [edx+0x1c]` (`0x19505`) is followed by `mov eax,[esp+8]` (`0x19508`) |
| `0x4F644` | `input_state_update` (input.c) | 162 | 0 | followed by hand, two calls deep. `0x24C6E`: `mov eax,ebx` (`0x24C9F`) or, past the loop, `0x461DC` (`mov eax,2` `0x461F8` / `mov eax,[0x101514]` `0x46203` before any read) or `0x38990` (`mov ax,[0xf0aec]` `0x38991`, `mov eax,[0xf0aec]` `0x3899F`). `0x25210`: `jmp 0x2540f`, `0x2A31C` (`mov eax,[0x105bcc]` `0x2A353`, or its early `ret`), then `mov ax,[0x104b00]` (`0x2545C`, only AX compared) or `0x3BB90` (`xor ah,ah; mov [0x107d30],ah`, `xor eax,eax` `0x3BB9B`). **Weaker evidence than the other rows': not every branch below those calls was read** |

## §2 The rows (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `camera_dust_burst` | 0x12BB8 | 3 | 4/4 | 18 | 0 | stubs `0x2AE14` SPAWN, `0x2BC30` ANIM_BEGIN, `0x2B150`, `0x2C3FC` VOICE; `0x4DBB4` allow (its own row, d1f) |
| `camera_mode_track_pair` | 0x12E3C | 15 | 24/24 | 19 | 0 | stub `0x18714`; `0x12C7C` allow (its own row, d1h); correction §3.1 |
| `fighter_14b90` | 0x14B90 | 6 | 10/10 | 33 | 0 | stubs VOICE, `0x18B44`, `0x3C520`, `0x3C148`, `0x3C16C`; `0x339AC`, `0x396AC` (with `0x33950`, `0x3AFC4`) and `0x14590` allow |
| `camera_box_palette` | 0x15B90 | 6 | 6/6 | 12 | 0 | `0x17EEC`, `0x1A570` allow (camera.c reads both through its own unseamed helpers) |
| `camera_page_tail_b` | 0x164F4 | 14 | 23/23 | 29 | 0xFF | `0x3C570`, `0x164C0`, `0x1A570` allow; correction §3.2 |
| `camera_page_tail_a` | 0x16AFC | 15 | 27/27 | 29 | 0xFF | `0x16734` (with `0x33A10`), `0x3C570`, `0x164C0`, `0x1A570` allow |
| `fighter_21458` | 0x21458 | 13 | 10/10 | 35 | 0 | stubs `0x39834`, VOICE, SPAWN, `0x13244`, ANIM_BEGIN, `0x3C16C`, `0x188AC`; `0x33A10`, `0x21F30` allow |
| `frame_timer` | 0x2AA70 | 27 | 32/32 | 33 | 0 | stubs `0x29F34`, `0x2B2A0`, `0x2A408`; `0x2A39C` and the raw's own recursive call (`0x2AA70`) allow; corrections §3.3, §3.4 |
| `config_credit_ready` | 0x2C060 | 5 | 2/2 | 6 | 0xFFFFFFFF | `0x2CAA8` real (recorded); the tail jump's target `0x2CA2C` is the row's second block |
| `hiscore_read` | 0x2DBC4 | 7 | 17/17 | 13 | 0xFFFFFFFF | `0x2DB58` allow |
| `hiscore_rank_probe` | 0x2DDE4 | 9 | 16/16 | 9 | 0xFFFFFFFF | `0x2DB58` allow; correction §3.5 |
| `config_bar_draw` | 0x30788 | 7 | 15/15 | 16 | 0 | `0x2F434`, `0x2F174` allow; their `0x2EFD4`, `0x2F198`, `0x2F5A0` are the recorded stubs |
| `config_option_row` | 0x31E28 | 12 | 14/14 | 16 | 0 | stubs `0x2F198`, `0x2F5A0`; the string path `0x1C500`/`0x474E4`/`0x1E75C`/`0x1E808`/`0x500BB` and `0x2F174` allow; `RESOLVED_JUMPS` `0x31EBC` |
| `fighter_3e6a8` | 0x3E6A8 | 13 | 10/10 | 34 | 0 | as `fighter_21458` with `0x3F308` allow; EDX = the slot's record in every case (§4) |
| `fighter_3f9c8` | 0x3F9C8 | 14 | 18/20 | 36 | 0 | stubs `0x187FC`, `0x189FC`, `0x3C190`, `0x3C148`, `0x2B150`, `0x3C16C`, `0x188AC`, `0x2BC30`, `0x188DC`; `0x33950`, `0x3F6F4`, `0x3F720` allow; two blocks named (§4) |
| `fighter_3fdd8` | 0x3FDD8 | 4 | 4/4 | 23 | 0 | stubs `0x1A570`, `0x188DC`, `0x2BC30`, `0x39834`, `0x39A10`; `0x33950`, `0x3D0C0` allow; correction §3.6 |
| `input_state_update` | 0x4F644 | 5 | 2/3 | 10 | 0 | `0x50161`, `0x2D2F0` allow; one block named (§4) |

Every binding and mutant of each row was run ten times on the port side with identical output and no
`diffrun` failure: no index-like sentinel leaves `mem[]` ("other side" mutants use `side ^ 1` on sides
0 and 1; the side-2 cases of the two tails keep `0x1077B0 + 2 * 0x94` inside the image).

**Exports for the bindings** (not corrections): `camera_mode_track_pair` and `camera_box_palette` lose
`static` (declared in `camera.h`), `frame_timer` loses `static` (declared in `actors.h`). No seam was
added.

**Harness lines outside the batch's three blocks** (additive, none edited): `tools/diff_emu.py`
`RESOLVED_JUMPS` gains `0x31EBC: (0x31EA7, 0x31E0C, 7)` with its comment, and the `WINDOWS` dictionary
of `test_each_resolved_jump_table_matches_the_bytes` (`tools/tests/test_diff_verify.py`) gains the
matching `0x31EBC` window (`cmp edi, 6` .. `jmp dword ptr cs:[eax + 0x31e0c]`), re-checked from the
image by a scratch run of the test's own loop.

## §3 Corrections (raw over port, address-proved)

Each is its own commit. Neither was run against the oracles; for each the 820-frame `--check` dump is
byte-identical before and after (sha256 of the concatenated `frames/*` `a53064ba3444295e...`) and
`PR_ORACLE_REQUIRED=1 ./build/run_tests` is green.

1. **`camera_mode_track_pair` (`0x12E3C`): the dead zone at a distance of INT_MIN.** Raw: per slot
   `edx = L[i] - cam` (`0x12F4D..0x12F53`), `cmp ecx,edx; jle; neg edx` (`0x12F56..0x12F5A`, ECX = 0),
   `cmp edx,0x1800; jle 0x12f7e` (`0x12F5C/0x12F62`): the local is zeroed when the *magnitude* is at
   most 0x1800, and `neg` leaves INT_MIN negative, so a distance of exactly INT_MIN gives 0. The port
   tested the signed distance (`d < -0x1800 ? d + 0x1800 : ...`) and returned `INT_MIN + 0x1800`.
   Found by case `p11` (camera 0, slot 0 at `0x80000000`, slot 1 at `0x80004000`): `byte 0xF0AF0:
   original 0x01, port unchanged` .. `byte 0xF0AFD: original unchanged, port 0x04` (the original commits
   `cam - step`, the port a target `0x3800` right). New: the magnitude by an unsigned negation, then
   the raw's three arms. No game state reaches it: the camera x is clamped to ±0x5D00 after each mode
   body (`0x12D78..0x12D9A`). Mutant `camera_mode_track_pair@zone` is the port before the fix (caught
   by `p11` alone).
2. **`camera_page_tail_b` (`0x164F4`): the cached +0x63 word is compared as a signed word.** Raw, the
   cached-box path: `mov dl,[edx+0x63]` (`0x166E7`), `mov si,[ecx+0x100b48]` (`0x166EA`), `xor dh,dh`
   (`0x166F1`), `cmp dx,si; jl 0x1670c` (`0x166F3/0x166F6`). The port compared
   `(s32)r63 < (s32)(u32)DSW(0x100B48 + side*2)`, the cached word zero-extended, so a cached word of
   0x8000 or more failed where the raw passes. Found by case `t9` (cached word `0x8000`): `eax (mask
   0xFF): original 0x1, port 0x0` and the four out bytes. New: `(s32)(s16)DSW(..)`. The function's own
   store there is a zero-extended byte (`xor ah,ah; mov [ecx*2+0x100b48],ax` `0x16694/0x16696`), so no
   state it produces differs. Mutant `camera_page_tail_b@uns` is the port before the fix (caught by
   `t9` alone).

3. **`frame_timer` (`0x2AA70`): the pset index is DX, not EDX.** Raw: the argument stays in ESI and
   every use narrows it: `xor edx,edx; mov dx,si` before `0x2B2A0` (`0x2ABC4/0x2ABC8`) and before
   `0x2A39C` (`0x2ABDF/0x2ABE3`), `xor eax,eax; mov ax,si; .. shl eax,5` for the 0x1E1 pset store
   (`0x2AC60..0x2AC6E`). The port used the whole 32-bit argument for the opcode call and the pset
   store. Found by `m1` (EDX = `0x00030001`: `byte 0x10A620: original 0xE1, port unchanged`, the port's
   word at `0x70A620`) and `l1` (`call #0: original 0x2B2A0(0x10A300, 0x3, 0x0), port 0x2B2A0(0x10A300,
   0x20003, 0x0)`). New: `slot &= 0xffffu` on entry. Both callers pass a zero-extended value (`0x2A24D`
   `mov edx,esi`, ESI from `mov si,[eax+0x56]` `0x2A204`; the recursion's `xor edx,edx; mov
   dl,[ecx+0x4b]`), so no reachable state changes. Mutant `frame_timer@slotw` is the port before the
   fix. The same class as d1g's `anim_id_path` correction.
4. **`frame_timer` (`0x2AA70`): an unordered timer takes the hold arm.** Raw: `fldz; fcomp dword
   [esp]; fnstsw ax; sahf; jb 0x2ac47` (`0x2AB3C..0x2AB44`): `jb` is taken for 0 < old and for an
   unordered compare (C0 set), so a NaN timer goes to the hold arm (the 0x1E1 pset word under `+0x2A`
   bit 8). The port's `old > 0.0f` fell into the opcode walk. Found by `m3` (timer `0x7FC00000`):
   `call #0: original none, port 0x2A408(0x10A300, 0x10A660)`. New: `!(old <= 0.0f)`. The two later
   compares need no change: `jae` (`0x2AC35`) is not taken on a NaN step, but the sum just stored is
   then a NaN too and `ja` (`0x2AC3F`) is not taken either, so the raw returns as the port's
   `!(step > 0)` does, with the same bytes. Mutant `frame_timer@nan` is the port before the fix.
5. **`hiscore_rank_probe` (`0x2DDE4`): the win exit checks the byte budget.** Raw: the strict-win `ja
   0x2de75` (`0x2DE5E`) lands on `cmp dword [esp+8],0; jge 0x2de8a` (`0x2DE75/0x2DE7A`), the same test
   the exhausted loop falls into: -1 when the remaining budget is negative, else the rank. The port
   returned the rank on a win without the test. The budget is negative on a win only when `0x2DB58`'s
   `count * size` is at least `0x80000000` on entry; the descriptors are constants of the code object
   (`0x2D3FC`: counts 10/1/1, sizes 12/28/5), so no reachable state changes. Found by `k7` (count
   `0x8000`, size `0x10000`): `eax: original 0xFFFFFFFF, port 0x0`. Mutant `hiscore_rank_probe@left` is
   the port before the fix.
6. **`fighter_3fdd8` (`0x3FDD8`): AL of `0x1A570`.** Raw: `call 0x1a570; test al,al; je` (`0x3FDEB..
   0x3FDF2`). The port compared the callee's whole return value with 0. Found by `e2` (stub EAX =
   `0x100`): `call #1: original 0x188DC(0x0, 0x7FFFF400), port 0x188DC(0x0, 0x80000400)`. New: `(u8)..
   != 0u`. The real `0x1A570` returns 0 or 1 in the whole register (`and eax,0xffff; sete al`
   `0x1A59F/0x1A5A4`), so no reachable state changes. Mutant `fighter_3fdd8@al` is the port before the
   fix.

## §4 Named gaps, limits and unhit blocks

- **Unhit blocks, each named in its spec's `unhit_named`:**
  - `fighter_3f9c8` `0x3FB76`: the `+0x57` above 6 arm, `mov eax,1; call 0x62003` (the runtime's
    fatal exit, >= 0x5D000), cannot run; the port returns there (a `PORT:` comment).
  - `fighter_3f9c8` `0x3FA7A` (`xor edx,eax`, the clamp of a negative count to 0): dead with
    `0x3F6F4` real. `0x3F6F4(side, 0x28)` has just returned `0x28 - (+0x86 >> 16)` clamped at 0 and
    the raw went on only for a result of at least 1 (`cmp eax,1; jge` `0x3FA42/0x3FA45`); the second
    `0x33950` context is the same side's, so `0x3FA60..0x3FA72` recomputes the same value and the
    `jge` at `0x3FA78` is always taken. A stubbed `0x3F6F4` would reach it; `0x3F6F4` is not in the
    stub clobber table.
  - `input_state_update` `0x4F694` (`or [0x1088e4],[0x1088d8]`): dead with `0x2D2F0` real (`xor
    eax,eax; ret`): the byte `0x1088D4` just stored is 0, so `test byte [0x1088d4],2; je` (`0x4F68B`)
    is always taken.
- **`0x52106` (`gfx_screen_reset`, gfx.c) is a named gap, not a row.** Its body reads the VGA status
  port and programs the DAC: `mov edx,0x3da; in al,dx; test al,8; je` (`0x52129..0x52131`, the wait for
  vertical retrace), `out dx,al` at `0x5213F` and `0x52145..0x52147` (256 black DAC entries). `in`/`out`
  are in the harness's `UNMODELED_MNEMONICS`: `diff_emu.run_original(image, 0x52106, {"eax": 5},
  allow_calls=(0x51F72,))` stops with `unmodeled`, `in at 0x5212E` (run once, by hand). The port keeps
  the DAC in `gfx_dac` and the aperture in `g_aperture` (its `PORT:` comment), which are not in
  `mem[]`, so the row would not see them either. No spec was written: a `gap=` spec is a named-gap row,
  which changes the pinned counter's gap figure and the list of
  `test_every_ported_function_agrees_with_the_original_on_every_block`.
- **`fighter_3e6a8`: the record argument.** The port's function is `(slot, side)` and reads the record
  from the slot; the raw reads EDX for the dust's `+0x30` (`mov eax,[esi+0x30]` `0x3E760`). Every case
  passes EDX = the EAX slot's record, as the one call site does (`mov edx,[ecx]` `0x35396`, `call
  [ecx+0x10]` `0x354E2`): an EDX that is another record is **not cased**, and the twin's `@spawnrec`
  mutant has no counterpart here.
- **`frame_timer`: NaN only where it separates.** `m3` cases a quiet NaN timer. A NaN step is not
  cased (equivalent, §3.4); a signalling NaN is not cased (the x87 quiets it on load, the port's `float`
  arithmetic is the host's).
- **`hiscore_rank_probe`: a value byte count above 4 is not cased.** The raw packs the value into its
  frame's first four bytes (`mov [esp+eax-1],dl` `0x2DE27`, with the size dword at `[esp+4]` and the
  budget at `[esp+8]`): five or more bytes run into the size. The port keeps a separate 8-byte buffer
  (its comment calls 8 "headroom"; the raw's frame gives 4). The descriptors' counts are the code
  object's constants 4/4/3 (`0x2D400 + table*8`). `@ge` was dropped as equivalent (the compare runs on
  the first differing byte, so `>=` is `>`).
- **`hiscore_read`: an odd name byte count is not cased** (the raw's `sub esi,2; jne` `0x2DC8A` then
  wraps; the port's loop is the same `n -= 2`).
- **`config_credit_ready`, `config_bar_draw`, `config_option_row`: the stubs below the allowed
  wrappers.** `0x2F434`/`0x2F174`/`0x1C500` run on both sides, so the rows prove the arguments those
  wrappers hand on (`0x2EFD4`, `0x2F198`, `0x2F5A0` recorded) and rest on the wrappers' own rows (d1g).
- **`input_state_update`: the selectors' order is not observable** (the two masks `0xFF00FF00` and
  `0x00FF00FF` are disjoint, so the two turns at the latch commute; `@order` was dropped as equivalent).
- **PORT guards not cased.** `camera_dust_burst` skips the stream start when the spawn returns 0 (the
  raw calls `0x2BC30` with EAX = 0 unconditionally, `0x12C0E..0x12C18`); no case returns 0 from the
  SPAWN stub.
- **`camera_dust_burst`: the slot byte's extension.** `movzx esi, byte [0x104b1a]` (`0x12C42`) is a
  zero extension; an index of 0x63 or more puts `0x1077B0 + idx*0x94` past the image's end
  (`0x10B0D0`), so no case can tell it from a sign extension. Indexes 0, 1 and 2 are cased.
- **`camera_mode_track_pair`: signed wrap is not cased** beyond `p11`: `L[a] - L[b]`, `L[i] - cam` and
  `l0 + l1` wrap in the raw and are `s32` arithmetic in the port (undefined on overflow in C, the build
  has no `-fwrapv`); `p11` is chosen so that neither side overflows (both slots negative, camera 0).
  `a = (slot0 >= slot1)` versus `>`: equal positions give a zero difference, no split and equal locals
  on either reading, so that edge is not separable (no mutant).
- **`fighter_14b90`: the callees' AL.** `0x396AC` and `0x14590` run on both sides; `test al,al`
  (`0x14BB4`, `0x14BEC`) is therefore fed real 0/1 results only. The constant `mov al,1` return is not
  compared (void port function, §1).
- **The two tails: the countdown's signed wrap.** `dec` of a countdown of `0x80000000` wraps to
  `0x7FFFFFFF` in the raw (`0x1651A`, `0x16543`, `0x16B81`, `0x16BAA`); the port's `(s32)x - 1` is
  undefined there. Not cased; `t4` (`0xFFFFFFF0`) and `t2`/`t13` (0) take the clamp on each side arm.
- **The two tails: `0x16734`/`0x164C0`/`0x1A570`/`0x3C570` run real**, so the tails' rows rest on
  those rows (d1h, E1) for the callees' own arithmetic. `camera_page_tail_a`'s side-2 case (`t12`)
  gives slot 2 a record so that `0x16734` (which reads `[0x1077B0 + 2*0x94]`) stays inside the image.

## §5 Store sweep

For every field a row's function writes, the pre-state differs from the written value and the
neighbouring bytes are seeded (the slots of `fighter_14b90` and the two tails are seeded whole, as
are `0xFD120..0xFD15F` and `0x100B38..0x100B4F` for the tails); a dropped or misplaced store is caught
in every row that has one (`camera_mode_track_pair@no34a`/`@no2ca`/`@no34b`/`@no2cb`/`@rec`/`@field`,
`fighter_14b90@s57`/`@cb0c`/`@r55`/`@nodec`/`@fd11a`, `camera_page_tail_*@nobox`/`@noe1`/`@nodec`/
`@cww`/`@c63w`). `camera_dust_burst` and `camera_box_palette` write only through their callees and the
two out dwords. Findings:

- The tails' first fixture put the pset array on top of fighter record 0 (`0x10A200`), so the record's
  `+0x20` float was the pset word and `0x164C0` never reported a last frame: `@bound`, `@s5f` and
  `@side` went undetected until the pset moved to `0x10A600` (`D2D_PSET`), the other slot's state byte
  stopped being `st ^ 0xF` (8 ^ 0xF = 7, also a scanning state) and `t0` took `s5f` = 4 (a 3-byte
  shift of the `* 16` mutant lands on the next triple and matches the same code).
- `camera_dust_burst@early` and `camera_mode_track_pair@late` are caught by the memory at a recorded
  call alone.
- `fighter_3f9c8`: case `s2` first named a slot that was not the side's, whose seeded `+0x57` (0x97)
  took the fatal arm (`unmodeled: call 0x62003 from 0x3FB7B`); it now passes the side's own slot.
- `input_state_update@e0w` (a dword store at `0x1088E0`) was first written before the `0x1088E2` word
  store, which overwrote its damage; it is now the last store.
- `frame_timer`: the four `+0x20`/`+0x24` float stores, the `+0x63` counter, the child chain's three
  byte stores and the pset word each have a mutant that drops or misplaces them (`@add`, `@no63`,
  `@late`, `@child2a`, `@child20`, `@chainclr`, `@chain2a`, `@e1w`).

## §6 Queue entries not done

(filled in at the end of the batch)
