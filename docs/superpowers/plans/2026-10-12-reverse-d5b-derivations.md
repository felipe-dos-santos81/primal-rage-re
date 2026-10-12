# Track D, wave 5, batch d5b — derivation record

Branch `reverse-d5b` (off `reverse-d4int`). Queue: 11 functions — the overlap family of
`port/src/game/camera.c` (seven entries), `sprite_blit` and `render_list` in `port/src/platform/`, and two
small ones. Result: 19 rows over 10 entries, 301 mutants, five address-proved port corrections (one per
function, twelve `test al,al` / `and eax,0xff` sites), one named gap, no entry left undone.

Everything below was measured on this branch with `python3 tools/diff_verify.py --image /tmp/pr_d5b_img.bin
--function <row> --self-check`, one row at a time; the pins (`D5B_MASKS` / `D5B_KINDS` / `D5B_CASES` in
`tools/tests/test_diff_verify.py`) were written from `verify_all` on the rows' own specs, every mutant's port
side run ten times with identical output. One full `PR_DIFF_JOBS=4 … --self-check` was run at the end, on
the commit that holds the last rows: `rc 0`, `803/803 functions VERIFIED; 9521/9521 mutants detected; 1
named gaps; 503/636 rows with callees closed (167 have none)`. The unittest, `make verify`, `make
diff-verify` and the oracle targets were not run.

**How the EAX masks were derived.** For each entry, every `call rel32` / `jmp rel32` to it and every dword of
the image equal to it (none of these entries is stored in data); from each return address the instructions
up to the first one that rewrites EAX or reads the callee's value.

## §1 Members

| entry | port function | size | EAX mask and the callers that fix it |
|---|---|---|---|
| `0x16DA4` | `camera_winner_height` (camera.c; *static* before this batch) | 729 | 0. Three sites: `mov eax,[0x100b54]` `0x1756A`, `0x178EA`; `cmp [0x100b54],0` `0x17BB1`, then `mov al,1` / `xor al,al`, and `0x1790C`'s callers read AL alone. |
| `0x176CC` | `camera_projectile_hit` (*static* before) | 574 | 0. Two sites `0x17D0E`, `0x17D21`, in `0x17CB0`: `cmp [0x10784c],0; mov eax,1`, or the return through `0x17CB0`. |
| `0x17CB0` | `camera_projectile_step` | 125 | 0. One site `0x19763`; `mov eax,esp` `0x19774` (after `mov ecx,1; mov esi,2; xor ebx,ebx`). |
| `0x1790C` | `camera_point_side` (*static* before) | 699 | `0xFF`. Four sites, each `test al,al` (`0x17E00`, `0x17E13`, `0x17E75`, `0x17E8B`). |
| `0x17D30` | `camera_point_hit` | 413 | full. Five sites test the dword and compare it with 2 (`mov edx,eax; test eax,eax; cmp eax,2` `0x12A30`; `0x4B6E7`, `0x4C646`, `0x4D7ED`, `0x4E5EA`). |
| `0x170A0` | `camera_unfreeze` | 1248 | 0. Two sites `0x176AC`, `0x176BF`, in `0x17580`: `cmp byte [0x100b61],0; mov eax,1`, or the return through `0x17580`. |
| `0x17580` | `camera_decay` | 330 | 0. Seven sites (`0x262E9`, `0x26478`, `0x265D5`, `0x27473`, `0x284EE`, `0x2880B`, `0x29ADB`), each followed by `call 0x1958c`, which begins `xor eax,eax` (`0x19595`). |
| `0x51E5C` | `sprite_blit` (sprite.c) | 121 | 0. One site `0x1456B`; `mov eax,[esp+0x5c]` `0x14570`. |
| `0x14328` | `render_list` (render.c) | 610 | full, by value: the raw leaves EAX = the list's terminating link, 0 (`mov eax,[eax]` `0x14574`, `0x14338`), which the binding returns. |
| `0x1C65C` | `text_blit_string` (actors.c) | 119 | 0. One outside site `0x1B3EF`; `mov dl,1; mov eax,esp` `0x1B3F4`. |
| `0x2EA78` | `config_screen_wait` (config.c) | 263 | named gap, §4. |

**Exports (linkage only; no body, comment or call-site change, no `fn_register`, no new `/* 0xADDR` header).**
`camera.c`/`camera.h`: `camera_winner_height`, `camera_projectile_hit`, `camera_point_side`.

**Seams added** (first statement of each; arguments in the C order):

| address | function | file | line |
|---|---|---|---|
| `0x15EC0` | `camera_bitrev` | camera.c | `PR_SEAM(0x15EC0u, buf, len);` |
| `0x1638C` | `camera_sprite_row` | camera.c | `PR_SEAM(0x1638Cu, side, dst, count, row);` |
| `0x1631C` | `camera_sprite_height` | camera.c | `PR_SEAM_RET(0x1631Cu, side, index, frame, count);` |
| `0x1617C` | `camera_bitplane_merge` | camera.c | `PR_SEAM_RET(0x1617Cu, bit, src, rows, dst, arg5);` |
| `0x15C30` | `camera_box_clip` | camera.c | `PR_SEAM(0x15C30u, side, index, (u32)box[0] \| ((u32)box[1] << 8) \| ((u32)box[2] << 16) \| ((u32)box[3] << 24));` |
| `0x16DA4` | `camera_winner_height` | camera.c | `PR_SEAM(0x16DA4u, flag, af0_side, af0_other, b10, b30, (u32)arg2, side);` |
| `0x170A0` | `camera_unfreeze` | camera.c | `PR_SEAM(0x170A0u, side);` |
| `0x176CC` | `camera_projectile_hit` | camera.c | `PR_SEAM(0x176CCu, side);` |
| `0x17BC8` | `camera_projectile_clash` | camera.c | `PR_SEAM_RET0(0x17BC8u);` |
| `0x1790C` | `camera_point_side` | camera.c | `PR_SEAM_RET(0x1790Cu, side, (u32)x, (u32)y, tall);` |
| `0x51E5C` | `sprite_blit` | sprite.c | `PR_SEAM(0x51E5Cu, (u32)n->x, (u32)n->y, (u32)n->xorg, (u32)n->yorg, n->type, (u32)n->rows, (u32)n->width, n->pal_ptr, n->pixel_handle, (u32)n->clip_l, (u32)n->clip_r, (u32)n->clip_t, (u32)n->clip_b);` |
| `0x1C5E8` | `text_blit_glyph` (static) | actors.c | `PR_SEAM(0x1C5E8u, ch, (u32)*col, (u32)*row);` |
| `0x1C470` | `gfx_flush_palette` | gfx.c | `PR_SEAM0(0x1C470u);` |

Three of them pass a buffer of the caller's frame by value (record 2026-10-02-reverse-p2 §P2.7): `0x15C30`'s
box (`[ebx]`), `0x51E5C`'s node (`[eax]`, `[eax+4]` … `[eax+32]`, `[eax+40]` … `[eax+52]`: the raw offsets
of the C `SpriteNode`'s thirteen fields; `+0x24`, the descriptor handle, has no C field) and `0x1C5E8`'s
column and row (`[edx]`, `[ebx]`). A stub cannot store into the caller's frame, so what such a callee
leaves there is covered only by a row that runs it (`_real`, `render_list_blit`) or is named (§4).

The existing rows that reach a newly seamed function were re-run one by one after the seams
(`camera_bitrev`, `camera_sprite_row`, `camera_bitplane_merge`, `camera_sprite_height`, `camera_box_clip`,
`camera_projectile_clash`, `camera_projectile_clash_sync`): each VERIFIED with every mutant detected,
unchanged. No existing spec names `0x14328`, `0x51E5C`, `0x1C470`, `0x1C5E8` or `0x1C65C`.

**Callees.** New stubs and their clobbers (`E.callee_clobbers`; the expected-clobber dictionary of
`test_each_stub_declares_the_registers_its_callee_clobbers` gains them): `0x1631C` (ebx, ecx, edx), `0x15C30`
(ebx, edx), `0x16DA4` (ebx, ecx, edx; `ret 0xc`), `0x17BC8` (edi, ebp), `0x176CC` (), `0x1790C` (ebx, ecx,
edx), `0x170A0` (), `0x51E5C` (), `0x1C5E8` (ebx, edx, edi, ebp), `0x1C470` (edi, ebp). Reused constants:
`D4B_OVERLAP`, `D4B_SYNC`, `D4B_SYNC_REAL`. Recorded real calls: `0x1638C`, `0x15EC0`, `0x1617C` (under
`0x16DA4`), `0x181D0` (the `_real`/`_sync` rows). Allowed: `0x33950` (run from the original's bytes only:
`0x176CC` fills a context nothing reads, `0x176F3`), `0x17EEC`, `0x1B544`, `0x15F48`, `0x15FD4`, the runtime
memset `0x61A70`/`0x65490`/`0x654C7`, `0x15C30` with `0x15B90`/`0x1A570` (the `_real` rows), `0x14268`,
`0x51E5C` with its table's renderers (`render_list_blit`).

**`tools/diff_emu.py`.** One `_CALLEE_CLOBBER_FIXES` entry: `0x51E5C: ()`. Its six pushes
(`0x51E6E`..`0x51E73`) follow two early-out tests that write no register, so the scan's entry-push rule
credits none, and its indirect renderer call (`call [edx*4+0x80c8c]` `0x51EBF`) makes the scan assume every
register written; the six pops `0x51ECE`..`0x51ED3` restore them all, and the early-outs go to the `ret`
at `0x51ED4`. The raw caller `0x14328` keeps EBP (the camera) and EDI (the clip record) live across the
call (set `0x1432F`/`0x14331`, read `0x1439C`, `0x144E4`).

## §2 Row table (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `camera_winner_height` | `0x16DA4` | 13 | 32/32 | 31/31 | 0 | AL = flag (`mov [esp+0x10],al` `0x16DAE`: `w3` is EAX `0x100`); EDX, EBX the two sprite indices, ECX and `[esp+4]` the two first rows, `[esp+8]` the mode, `[esp+0xC]` the side; `ret 0xc`. `0x1631C` a stub whose EAX is the row width; `0x1638C`, `0x15EC0`, `0x1617C` recorded real calls, so every argument and the memory at each call are compared. Every mode (-1, 0, 1, 2, 3), both flags, both flip bytes, a row count of 0 and -1, a width of 0, the shift 0, `0x128` and `0x129`, side 2 (`w10`, `w12`: `1 - side`). |
| `camera_winner_height_rr` | `0x16DA4` | 3 | 7/32 (25 named) | 6/6 | 0 | The `0x1631C` stub stores `[0x100B54]` = `0x0007F000` and `[0x100B18]` = -1, which the raw reads after the call (`0x16E08`, `0x16DF5`, `0x16E10`). No row runs; `[0x100B54]` is the three divisions of `0x7F000000`, whose second dividend is negative: `@floor`, `@divuns`, `@kswap` are caught here alone. |
| `camera_projectile_hit` | `0x176CC` | 10 | 16/16 | 37/37 | 0 | Every callee a stub (`0x15C30` by value). The two countdown returns (`ja` `0x17700`; `jg` `0x17712` at 1, 2 and `0x8000`), the three AL tests with stub EAX `0x100`, `0xFFFFFF01`, `0xFFFFFF00`, the empty-box default, a negative width word and height. Found the AL tests (§3.1). |
| `camera_projectile_hit_real` | `0x176CC` | 5 | 16/16 | 7/7 | 0 | `0x15C30` and `0x181D0` run on both sides, the `0x16DA4` stub stores `[0x100B54]`. The clip changes the frame box (`@preclip`); the y sync fails after the x sync passed (`q3`: `@nosync2`); `@adcache`. |
| `camera_point_side` | `0x1790C` | 10 | 24/24 | 35/35 | `0xFF` | The two empty-box returns, the three AL tests, the byte width at 24, 32, 39 and -9, a box that reaches the fifth plane byte (`s6`, `s9`), `tall` = `0x100`, `[0x100B54]` positive, 0, `0x80000000`, -1. Found the AL tests (§3.2). |
| `camera_point_side_real` | `0x1790C` | 5 | 24/24 | 8/8 | `0xFF` | As above with `0x15C30`/`0x181D0` run: `@preclip`, `@retcache` (the stub's `[0x100B54]` over a negative seed), the third sync failing after two zeros (`t3`). |
| `camera_point_side_sync` | `0x1790C` | 4 | 24/24 | 2/2 | `0xFF` | `0x15C30` a stub, `0x181D0` run: the box need not lie inside the sprite, so the sprite sync fails alone (`v0`: `jne` `0x17ABE` taken, `@nosync2`). |
| `camera_unfreeze` | `0x170A0` | 12 | 48/50 (2 named) | 38/38 | 0 | The two countdown returns, the side's empty box, the other's absent box (the sprite arms of the x and y syncs), the record's bit 14 at `0x4000`, `0x8000`, `0xFFFF`, `0xBFFF`, the three AL tests. Found the AL tests (§3.3). |
| `camera_unfreeze_real` | `0x170A0` | 7 | 48/50 (2 named) | 13/13 | 0 | `0x15C30`/`0x181D0` run: both clips change their frame boxes (`@preclip`, `@preclipb`), `@afcache`, the y sync failing after two zeros (`e3`). |
| `camera_unfreeze_sync` | `0x170A0` | 2 | 38/50 (12 named) | 3/3 | 0 | `0x15C30` a stub, `0x181D0` run: the sprite sync fails alone (`g0`: `jne` `0x17446` taken). |
| `camera_projectile_step` | `0x17CB0` | 7 | 9/9 | 13/13 | 0 | The plane fill (run), the two zero stores, the clash byte = AL of `0x17BC8` (`0x100`, `0xFFFFFF00`, `0xFFFFFF01`), each pointer live and not. |
| `camera_projectile_step_rr` | `0x17CB0` | 2 | 9/9 | 2/2 | 0 | The stubs store the two pointers the raw reads again (`0x17D03`, `0x17D13`): `@cache0`, `@cache1`. |
| `camera_point_hit` | `0x17D30` | 8 | 18/18 | 26/26 | full | AX, DX, BX alone are read (upper halves set in `h0`, `h1`, `h7`); the two truncating divisions by 64 at -1, -63, -32767; mode `0x22` with the selector 0, 1, 2 and mode `0x122`; the stub stores the four dwords the function restores. Found the AL tests (§3.5). |
| `camera_decay` | `0x17580` | 6 | 14/14 | 20/20 | 0 | The four countdowns at 0, 1, `0x8000`, `0xFFFF`; the four scalings over positive, negative and wrapping products; the gate's AL (`0x100`, `0xFFFFFF01`); each gate byte alone. Found the AL test (§3.4). |
| `camera_decay_rr` | `0x17580` | 2 | 14/14 | 1/1 | 0 | The `0x170A0` stub zeroes the byte `0x100B61`, read after the first call (`0x176B1`): `@b61cache`. |
| `render_list` | `0x14328` | 11 | 30/30 | 30/30 | full | `0x51E5C` a stub that records the frame node by value (thirteen fields). Layers 0, 1, 2 (before and after a layer-1 node), 3, 4, `0x8000`, `0x0101`, `0x0102`; each clip alone and in pairs; each skip at the extent and one short of it; the four offset words at `0xFFFF`/`0xFFF0` (zero-extended); a negative `[0xF0AEC]`; both callers' camera records. |
| `render_list_blit` | `0x14328` | 8 | 30/30 | 11/11 | full | The same entry with `0x51E5C` and its table's renderers run on both sides: the back buffer is the comparison. Types 1, 2, 9, 6 and `0x11`, `0x12`, `0x19`, `0x16`, clipped on each edge and on two; the `ret` types 5, `0xA`, `0xE`; bank bytes 6 and 1. No difference was found between the original's renderers and `sprite.c`'s. |
| `sprite_blit` | `0x51E5C` | 9 | 4/4 | 7/7 | 0 | The entry's own four blocks on the paths that leave the node as it was: the two zero-extent returns and the table's `ret` entries (types 0, 3, 5, `0xA`, `0x15`, `0x1A`, `0x1F`). The live renderers are `render_list_blit`'s (§4). |
| `text_blit_string` | `0x1C65C` | 5 | 6/6 | 11/11 | 0 | The two truncating scalings (0, `0xE6`, -1, -`0x100`, `0x12345`), the empty string, bytes `0x80` and `0xFF`, eleven characters; `0x1C5E8` (by value) and `0x1C470` stubs. |

Totals: 19 rows, 301 mutants. Every mutant's port side was run ten times with identical output. No row
reads or writes outside the image (`SpecResult.outside` empty for all 19: no `D5B_OUTSIDE` entry).

## §3 Corrections (each in its own commit, address-proved)

All five are the same defect class: the port tested a callee's whole `int` where the raw tests AL. None is
reached in play (the ported callees return 0 or 1, and `0x181D0` 0..3), so no frame can move; the oracles
were not run.

### §3.1 `camera_projectile_hit` tests AL of `0x140E4` and `0x181D0`

- **Raw.** `0x17748` `call 0x140e4`, `0x1774D` `test al,al`, `0x1774F` `je 0x17900`; `0x17844` `call 0x181d0`,
  `0x17849` `and eax,0xff`, `0x1784E` `jne 0x17900`; `0x1787E` `call 0x181d0`, `0x17883` `and eax,0xff`,
  `0x17888` `jne 0x17900`.
- **Port before / after.** `camera_box_overlap(..) == 0` → `(u8)camera_box_overlap(..) == 0u`;
  `camera_sync_visible(..) != 0` (twice) → `(u8)camera_sync_visible(..) != 0u`.
- **Measured before the fix.** `p5` (stub EAX `0x100` of `0x140E4`): `call #1: original none, port
  0x15C30(0x0, 0x21, 0x6050403)`; `p1` (stub EAX `0x100` of `0x181D0`): `call #2: original 0x181D0(0x20,
  0x31, 0xFFFFFF10, 0x100B18, 0x100B10, 0x100B2C, 0x100B30, 0x100B20), port none`; `p8` (`0xFFFFFF00`) the
  same. Mutants `@al140`, `@alsync`, `@alsync2`.

### §3.2 `camera_point_side` tests AL of `0x181D0`

- **Raw.** `0x179E8` `call 0x181d0`, `0x179ED` `and eax,0xff`, `0x179F2` `jne 0x17bbe`; `0x17AB4`,
  `0x17AB9`, `0x17ABE`; `0x17B0B`, `0x17B10`, `0x17B15`: the same three instructions.
- **Port before / after.** `camera_sync_visible(..) != 0` (three times) → `(u8)camera_sync_visible(..) != 0u`.
- **Measured before the fix.** `s1` (stub EAX `0x100`): `call #2: original 0x181D0(0x20, 0x30, 0x198,
  0x100B1C, 0x100B14, 0x100B28, 0x100B34, 0x100B24), port none`; `s5` and `s8` the same. Mutants `@al1`,
  `@al2`, `@al3`.

### §3.3 `camera_unfreeze` tests AL of `0x181D0`

- **Raw.** `0x17362` `call 0x181d0`, `0x17367` `and eax,0xff`, `0x1736C` `jne 0x17576`; `0x1743C`,
  `0x17441`, `0x17446`; `0x174E5`, `0x174EA`, `0x174EF`.
- **Port before / after.** `if (r != 0) return;` (three times) → `if ((u8)r != 0u) return;`.
- **Measured before the fix.** `u1` (stub EAX `0x100`): `call #2: original 0x181D0(0x15, 0x27, 0x298,
  0x100B1C, 0x100B14, 0x100B28, 0x100B34, 0x100B24), port none`; `u7` and `u10` the same. Mutants `@al1`,
  `@al2`, `@al3`.

### §3.4 `camera_decay` tests AL of `0x140E4`

- **Raw.** `0x17698` `call 0x140e4`, `0x1769D` `test al,al`, `0x1769F` `je 0x176c4`.
- **Port before / after.** `camera_box_overlap(..) != 0` → `(u8)camera_box_overlap(..) != 0u`.
- **Measured before the fix.** `d1` (stub EAX `0x100`): `call #1: original none, port 0x170A0(0x0)`, `call
  #2: original none, port 0x170A0(0x1)`. Mutant `@al`.

### §3.5 `camera_point_hit` tests AL of `0x1790C`

- **Raw.** `0x17DFB` / `0x17E0E` `call 0x1790c`, `0x17E00` / `0x17E13` `test al,al`, `je 0x17e1b`; `0x17E70`
  / `0x17E86` `call 0x1790c`, `0x17E75` / `0x17E8B` `test al,al`, `je 0x17e93`.
- **Port before / after.** `camera_point_side(..) != 0` (twice) → `(u8)camera_point_side(..) != 0u`.
- **Measured before the fix.** `h2` (stub EAX `0x100`): `eax (mask 0xFFFFFFFF): original 0x0, port 0x3`;
  `h7` (`0xFFFFFF00`) the same. Mutants `@al0`, `@al1`.

## §4 Named gaps, unhit blocks, what a row does not cover

- **`0x2EA78`** (`config_screen_wait`, 263 bytes): a named gap, for two reasons in its bytes. (1) Its
  present writes the VGA aperture, which in the flat image is game data and in the port is the host buffer
  (AGENTS.md, the aperture rule; d4b's `0x51ED8` is the same class): with the byte `0x1014FC` set, `mov
  eax,0xa0000; mov edx,[0xe87a4]; call 0x655ff` (`0x2EAB7`..`0x2EAC2`), else `call 0x501a3` (`0x2EAD1`), the
  dirty-dword blit whose destination is `mov ebx,0xa0000` (`0x501B0`). Measured with `0x2A31C`, `0x1C3FC`,
  `0x14328`, `0x1C470` stubbed and EAX = -1: the original returns with 102 of its 104 changed bytes in
  `0xA0000`..`0xAF9FF` (the other two are the buffer swap `0x50188`'s); with the byte `0x1014FC` set the
  copy changes 38216 bytes there. `0x501A3` is host-owned (no C function to seam; the port presents through
  `gfx_present`). (2) For EAX other than -1 the wait loop spins on the word `0xEF6DE`, which only the timer
  interrupt advances (`0x2EAF2`..`0x2EB0A`: measured, no return within 400000 instructions), then reads the
  BIOS keyboard (`int 0x16` `0x2EB15`, `0x2EB2D`). The port also orders the palette flush before the
  present (a PORT comment in config.c); nothing here can check that.
- **`camera_winner_height`.** A negative row width is a 4 GB `rep movs` in `0x1638C` (`0x163BC`) on both
  sides: no case, so the signed `jle` at `0x16F62` is not separated from the port's unsigned `i < width_a`.
  A mode outside -1..3 reads the raw's uninitialised `[esp+0xC]` as the other's width: no caller passes one.
  In mode -1 both `0x1631C` calls return the one stub EAX; which call's result is which width is fixed by
  modes 0 and 2 (one call each).
- **`camera_winner_height_rr`'s 25 named blocks** (`0x16E22`..`0x17027`): the row loop, which the stub's
  stored row count of -1 skips; the main row hits every one.
- **`camera_unfreeze`'s two named blocks** (`0x17198`, `0x171E5`): the raw tests the side's box twice
  (`0x1713C`..`0x1715E`, then `0x17184`..`0x171A0`); the second test's empty arm cannot run, the first having
  returned. Also not taken: the `je` at `0x173B6` (the frame box's width zero after the clip): `0x181D0`
  returns 2 for a zero extent, and a stub cannot store into the frame box.
- **`camera_unfreeze_sync`'s twelve named blocks**: the ten arms that are not that row's subject (hit by
  `camera_unfreeze` and `camera_unfreeze_real`) and the two dead ones above.
- **The by-value stubs.** Under a `0x15C30` stub the frame box the caller reads back is the one it passed, so
  a port reading the box before the clip is caught only by the `_real` rows (`@preclip`, `@preclipb`). Under
  the `0x1C5E8` stub the column is not advanced (`0x1C647`/`0x1C64C` add `0xF` or 8 in `0x1C65C`'s frame):
  every character is recorded at the first column; the advance is `0x1C5E8`'s own, a named gap (the brief).
- **One stub EAX per case.** `0x181D0` is called two or three times by `0x176CC`, `0x1790C`, `0x170A0`; a
  stub answers them alike, so a later sync failing after an earlier one passed is the `_real`/`_sync` rows'
  (`q3`, `t3`, `v0`, `e3`, `g0`). With the box inside the sprite (the clip's result) the sprite sync of
  `0x1790C`/`0x170A0` cannot fail alone, which is why the `_sync` rows stub the clip.
- **`render_list`.** The port walks the global head with the fixed camera {0, 0} and clip {0, 0, 320, 200}
  (a PORT comment in render.c); both call sites pass `0x105B44` and records holding those bytes in the image
  (`0xA87CC`/`0xA87D4`; `0xBCD64`/`0xBCD6C`), so the row's cases do, and another camera is not a state it
  can run. Whether anything writes those records at run time was not searched. The two early skips
  (`0x144D0`, `0x144DE`) are implied by the clip test and are not separated. A zero sprite id leaves the
  frame node's type as the previous node left it (the raw's frame, the port's uninitialised local): no case.
- **`sprite_blit` and `render_list_blit`.** With a live renderer the raw node is written: measured with the
  node in the image, `+0x38` in every renderer, `+0x3C` in the shear renderer, and in the clipped ones
  `+0x18` (the width becomes the visible width in `0x58CBD` and `0x5215C`), beside `+0x14`/`+0x30`, which
  `0x51E5C` restores (`0x51EC8`, `0x51ECB`). The port's renderers take the clips by value and keep the node
  (PORT comments in sprite.c), so a row on `0x51E5C` with the node in the image and a live type is a
  MISMATCH on those node bytes. In the game the node is a record of `0x14328`'s frame, rebuilt per pset,
  so `render_list_blit` runs the renderers there and compares the pixels; the `sprite_blit` row holds the
  entry's own blocks on the paths that leave the node alone. Not covered by either: a bank byte of 0 (the
  dword `0x81310` is the stale pointer `0x0005D110`; sprite.h), types 4 and `0x14` (not built by `0x14328`),
  a type above `0x1F` (`and edx,0x7f` `0x51EBC` indexes past the table's 32 entries), run lengths that reach
  other copy routines than the thirteen allowed (`0x521F0`, `0x5222C`, `0x529A0`, `0x529C4`, `0x52A18`,
  `0x58564`, `0x58578`, `0x5859C`, `0x58D20`, `0x58D2C`, `0x58D40`, `0x58D4C`, `0x58D90`), sprites other than
  the two 8 x 4 fixtures, and the buffer outside the seeded rows (a store of 0 into unseeded zero BSS is
  not seen). The two zero-extent returns of `0x51E5C` are not separated from a blit of nothing.
- **`text_blit_string`.** A product that overflows 32 bits wraps in the raw's `imul` and is undefined in the
  port's `int` multiply: no case (the one caller passes 0 and `0xE6`).
- **Mutants that were equivalent and are not registered:** `camera_projectile_hit`'s default box bytes 2
  and 3 (only bytes 0 and 1 are read back, `0x177CD`/`0x177D0`); `camera_point_side`'s unconditional
  `0x15F48` against a conditional one and its `0x15FD4` at a zero shift (no effect on the plane);
  `render_list`'s two early skips.

## §5 Store sweep

Every field a row's function writes is seeded with a value different from the one written, between seeded
neighbours (`d5b_region` seeds every dword of `0x100AA0`..`0x100B63` with its own byte); one dropped,
moved or widened store per row is a registered mutant: `camera_winner_height@nozero`/`@nofinal`/`@noand`,
`camera_projectile_hit@nob62` (caught by the memory at the call alone) /`@b62keep`/`@adside`,
`camera_point_side@nofill`/`@nob10`, `camera_unfreeze@noae0`/`@nob10`/`@nob30`/`@afside`,
`camera_projectile_step@noad0`/`@noad4`/`@ad4w`/`@late`, `camera_point_hit@nob63`/`@nob62`/`@keep08`/
`@keep0c`/`@keep62`/`@keep00`/`@keep04`, `camera_decay@cd0`..`@cd3`/`@nob54`/`@noaf8`/`@noafc`/`@late`.
`render_list` and `text_blit_string` store nothing outside their frames (the comparison is the call
list); `render_list_blit` and `sprite_blit` are compared on the back buffer. No store was found missing or
too wide in the port.

## §6 Queue entries not done

| entry | size | why |
|---|---|---|
| `0x2EA78` | 263 | named gap, §4 (the aperture; `int 0x16`) |

The other ten entries have rows.

## §7 Doubts

- §3's five corrections are unproven against the oracles (not run here); none changes a value the ported
  callees can return.
- `sprite_blit`'s own row is thin by construction (§4): it never draws. The row that verifies the blit's
  pixels is `render_list_blit`. In the full self-check neither `render_list` nor `sprite_blit` counts as
  closed (both stay open on `0x1B544`, a named non-row); of this batch's 19 rows the four closed ones are
  `camera_winner_height`, `camera_winner_height_rr`, `camera_decay` and `camera_decay_rr`.
- `camera_actor_bit15_clear` is still tested as a whole `int` in `camera_box_palette` (d4b §7); not this
  batch's rows.
- `camera_sprite_row` and `camera_winner_height` loop on unsigned counts where the raw's `0x16F62` is
  signed; the difference needs a negative width, which the raw's own `0x1638C` cannot survive (§4).
