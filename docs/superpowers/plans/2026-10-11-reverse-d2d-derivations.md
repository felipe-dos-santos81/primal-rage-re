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

## §2 The rows (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `camera_dust_burst` | 0x12BB8 | 3 | 4/4 | 18 | 0 | stubs `0x2AE14` SPAWN, `0x2BC30` ANIM_BEGIN, `0x2B150`, `0x2C3FC` VOICE; `0x4DBB4` allow (its own row, d1f) |
| `camera_mode_track_pair` | 0x12E3C | 15 | 24/24 | 19 | 0 | stub `0x18714`; `0x12C7C` allow (its own row, d1h); correction §3.1 |
| `fighter_14b90` | 0x14B90 | 6 | 10/10 | 33 | 0 | stubs VOICE, `0x18B44`, `0x3C520`, `0x3C148`, `0x3C16C`; `0x339AC`, `0x396AC` (with `0x33950`, `0x3AFC4`) and `0x14590` allow |
| `camera_box_palette` | 0x15B90 | 6 | 6/6 | 12 | 0 | `0x17EEC`, `0x1A570` allow (camera.c reads both through its own unseamed helpers) |
| `camera_page_tail_b` | 0x164F4 | 14 | 23/23 | 29 | 0xFF | `0x3C570`, `0x164C0`, `0x1A570` allow; correction §3.2 |
| `camera_page_tail_a` | 0x16AFC | 15 | 27/27 | 29 | 0xFF | `0x16734` (with `0x33A10`), `0x3C570`, `0x164C0`, `0x1A570` allow |

Every binding and mutant of each row was run ten times on the port side with identical output and no
`diffrun` failure: no index-like sentinel leaves `mem[]` ("other side" mutants use `side ^ 1` on sides
0 and 1; the side-2 cases of the two tails keep `0x1077B0 + 2 * 0x94` inside the image).

**Exports for the bindings** (not corrections): `camera_mode_track_pair` and `camera_box_palette` lose
`static` (declared in `camera.h`). No seam was added.

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

## §4 Named gaps, limits and unhit blocks

- **No unhit block** in any row so far.
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

## §6 Queue entries not done

(filled in at the end of the batch)
