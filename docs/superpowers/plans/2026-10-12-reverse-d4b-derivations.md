# Track D, wave 4, batch d4b — derivation record

Branch `reverse-d4b` (off `reverse-d3int`). Queue: 22 functions of `port/src/game/camera.c`, `camera.h`,
`effects.c`, `platform/sprite.c`, `render.c`, `gfx.c`. Result: 17 rows over 16 entries, 285 mutants, four
address-proved port corrections, four named gaps, two entries not done.

Everything below was measured on this branch with `python3 tools/diff_verify.py --image /tmp/pr_d4b_img.bin
--function <row> --self-check`, one row at a time; the pins (`D4B_MASKS` / `D4B_KINDS` / `D4B_CASES` /
`D4B_OUTSIDE` in `tools/tests/test_diff_verify.py`) were written from `verify_all` on the rows' own specs.
The unittest, `make verify`, `make diff-verify` and the oracle targets were not run.

**How the EAX masks were derived.** For each entry, every `call rel32` / `jmp rel32` to it and every dword of
the image equal to it; from each return address a walk over the image's bytes until the whole of EAX is
rewritten or some part of it still holding the callee's value is read (a callee met on the way is walked
from its entry; a zeroing `xor r,r` is not a read). Mask 0 means every walk ended in a rewrite.

## §1 Members

| entry | port function | size | EAX mask and the callers that fix it |
|---|---|---|---|
| `0x12DA8` | `camera_y_commit` (camera.c) | 72 | 0. EAX is `0x1317C`'s, passed through `pop edx; ret` (`0x12DEE`); d3e's table of the 15 call sites (record 2026-10-12-reverse-d3e §1) closes each. |
| `0x16308` | `camera_resolve_sprite` (*static* before this batch) | 19 | full. One site `0x18186`; the caller dereferences EAX (`mov ecx,[eax+2]` `0x1818D`). |
| `0x1631C` | `camera_sprite_height` (*static* before) | 109 | full. Two sites, both store EAX (`mov [esp+8],eax` `0x16DE1`, `mov [esp+0xc],eax` `0x16E04`). |
| `0x41030` | `camera_bitplane_accum` (*static* before) | 361 | full. One site `0x16380`, in `0x1631C`, which returns it. |
| `0x13D4C` | `effects_spawn_darken` | 164 | 0. Five sites; each reloads EAX next (`mov eax,ebx` `0x29B95`, `0x415A7`; `mov eax,0xb7` `0x3D639`, `0x3D9D2`, `0x3DE42`). |
| `0x13E28` | `effects_spawn_pulse` | 200 | 0. One site `0x12048`; `mov eax,ebx` `0x1204D`. |
| `0x13B3C` | `effects_spawn_scroll` | 308 | full. No call site and no dword of the image holds `0x13B3C` (dead in the shipped binary); it returns the record (`mov eax,esi` `0x13C68`), which the port returns too. |
| `0x140E4` | `camera_box_overlap` | 388 | `0xFF`. Three sites, each `test al,al` (`0x1769D`, `0x1774D`, `0x17BEF`). |
| `0x17BC8` | `camera_projectile_clash` (*static* before) | 231 | `0xFF`. One site `0x17CF5`; `mov [esp],al` `0x17CFA`. |
| `0x17FA0` | `camera_project` | 557 | 0. 40 sites, every walk ends in a rewrite; five (`0x26338`, `0x264C7`, `0x26624`, `0x274C2`, `0x29B2A`) return into `call 0x1975c`, whose `0x17CB0` runs `xor ah,ah` (`0x17CC0`) and `mov eax,0x128` (`0x17CCA`). |
| `0x15C30` | `camera_box_clip` (*static* before) | 654 | 0. Four sites; `xor eax,eax` at `0x171D1`, `0x1721F`, `0x177C9`, `0x179AD`. |
| `0x14268` | `sprite_node_build` (sprite.c) | 190 | 0. One site `0x14358`; `mov eax,[esi+0x18]` `0x1435D`. |
| `0x1C528` | `sprite_node_build` (the same C body) | 190 | 0. One site `0x1C60B`; `mov eax,0x80997c` `0x1C610`. |
| `0x387F4` | `render_scroll_scene_a` (*static* before) | 155 | 0. One site `0x387D1`; `mov eax,ebx` `0x387D6`. |
| `0x38890` | `render_scroll_scene_b` (*static* before) | 127 | 0. One site `0x387D8`; `call 0x38a38` follows (no argument; it loads EAX before reading it). |
| `0x38730` | `render_scroll_setup` | 196 | 0. Site `0x20E7F`: `mov eax,ebx` `0x20E84`. Site `0x20EAF` returns through `pop edx; ret` (`0x20EB4`) of `0x20E90`, whose one call site `0x4256A` rewrites EAX before any read of the old value. |

**Exports (linkage only; no body, comment or call-site change, no `fn_register`, no new `/* 0xADDR` header).**
`camera.c`/`camera.h`: `camera_resolve_sprite`, `camera_bitplane_accum`, `camera_sprite_height`,
`camera_projectile_clash`, `camera_box_clip`. `render.c`/`render.h`: `render_scroll_scene_a`,
`render_scroll_scene_b`.

**Seams added** (first statement of each; arguments in the C order):

| address | function | file | line |
|---|---|---|---|
| `0x1317C` | `camera_y_clamp` | camera.c | `PR_SEAM0(0x1317Cu);` |
| `0x41030` | `camera_bitplane_accum` | camera.c | `PR_SEAM_RET(0x41030u, handle, frame, count, table);` |
| `0x140E4` | `camera_box_overlap` | camera.c | `PR_SEAM_RET(0x140E4u, actor0, actor1);` |
| `0x181D0` | `camera_sync_visible` | camera.c | `PR_SEAM_RET(0x181D0u, p1, p2, (u32)p3, p4, p5, p6, p7, p8);` |
| `0x16AFC` | `camera_page_tail_a` | camera.c | `PR_SEAM_RET(0x16AFCu, side, out);` |
| `0x164F4` | `camera_page_tail_b` | camera.c | `PR_SEAM_RET(0x164F4u, side, out);` |
| `0x1A570` | `camera_actor_bit15_clear` (camera.c's static copy) | camera.c | `PR_SEAM_RET(0x1A570u, side);` |
| `0x3B938` | `fighter_3b938` | fighter.c | `PR_SEAM(0x3B938u, slot);` |
| `0x249B0` | `effects_list_insert_after` (effects.c's copy) | effects.c | `PR_SEAM(0x249B0u, at, rec);` |
| `0x249D0` | `effects_list_unlink` (effects.c's copy) | effects.c | `PR_SEAM(0x249D0u, rec);` |
| `0x387F4` | `render_scroll_scene_a` | render.c | `PR_SEAM(0x387F4u, i);` |
| `0x38890` | `render_scroll_scene_b` | render.c | `PR_SEAM(0x38890u, i);` |
| `0x38A38` | `render_scroll_fill` | render.c | `PR_SEAM0(0x38A38u);` |

The existing rows that reach a newly seamed function were re-run one by one after each seam
(`camera_y_clamp`, `list_insert_after`, `list_unlink`, `effects_spawn`, `effect_teardown`, `effects_step`,
`effects_clear`, `effects_init`, `camera_sync_visible`, `fighter_3b938`, `camera_rect_clip`,
`camera_page_tail_a`, `camera_page_tail_b`, `camera_box_palette`, `render_scroll_fill`,
`frontend_origin_zero`, `render_scroll_edge`): each VERIFIED with every mutant detected, unchanged.

**Callees.** `0x1B544` is allowed against C2b's preloaded resource entry (`d4b_res` adds a second entry so a
handle's index is not a constant); `0x17EEC`, `0x14080`, `0x15B90`, `0x16308`, `0x2EA30` and the runtime
memset `0x61A70`/`0x65490`/`0x654C7` are allowed where named in §2. New stubs and their clobbers
(`E.callee_clobbers`, re-derived for the expected-clobber dictionary line): `0x1317C` (), `0x41030` (ebx,
ecx, edx), `0x140E4` (edx), `0x181D0` (ebx, ecx, edx; `ret 0x10` on its five returns), `0x16AFC` (edx),
`0x164F4` (edx), `0x3B938` (), `0x387F4` (edi, ebp), `0x38890` (edi, ebp), `0x38A38` (). Reused constants:
`VOICE`, `SPAWN`, `BIT15`, `P7_2B150`, `C1_PAL_ACQ`. No `tools/diff_emu.py` change.

## §2 Row table (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `camera_y_commit` | `0x12DA8` | 8 | 5/5 | 11/11 | 0 | Mode byte `0xF0AFE` (`test al,al` `0x12DAE`): 0 takes the word at `0x1077E0 + byte[0xF0AFF] * 0x94` (index zero-extended, `xor edx,edx; mov dl,..` `0x12DB2`: `m2` is index `0x80`, which reads `0x10C1E0` above the image, pinned in `D4B_OUTSIDE`); else the signed max of the dwords `0x1077E0`/`0x107874` (`cmp eax,edx; jg` `0x12DDD`: `x0` signed only, `x2` differs as words, `x3` equal). `mov [0x1078f4],ax`, then `0x1317C` stubbed: `@order` is caught by the memory at the call alone. |
| `camera_resolve_sprite` | `0x16308` | 5 | 3/12 (9 named) | 5/5 | full | `call 0x17eec; add eax,edx; mov eax,[eax*4+0xa8b30]; jmp 0x1b544`. The tail jump makes `0x1B544`'s blocks this entry's: its nine non-resident arms are named (§4). `r2` character 7 (the default arm), `r3` index -1. |
| `camera_sprite_height` | `0x1631C` | 9 | 8/8 | 12/12 | full | height = first dword `sar 16`; `jl` `0x16353` (signed: `h5` negative frame, `h6` negative height), `jle` `0x16368` (`h3` the sum equal to the height); `test esi,esi` `0x1636E` (`h7` side 2); `h8` frame past the height with count 0. `0x41030` stubbed, its EAX returned and varied per case. |
| `camera_bitplane_accum` | `0x41030` | 9 | 28/28 | 17/17 | full | The RLE row decode into `table + k * 0x26`. Every arm of the skip loop and of the three draw loops, the bit's carry into the next byte in each, empty runs, a width of -7, counts 0 and -1. The runtime memset runs on the original side. |
| `effects_spawn_darken` | `0x13D4C` | 5 | 9/9 | 16/16 | 0 | The pop under the lock byte, the type-4 fill, the insert. `0x249D0`/`0x249B0` are recorded real calls: `@nolock1`, `@nolock2`, `@elate` are caught by the memory at a call alone. |
| `effects_spawn_pulse` | `0x13E28` | 6 | 11/11 | 19/19 | 0 | As above with type 6 and two loops; `p5` is count 257, where the white fill's last dword is the copy's first (`@merged`, a one-loop port, is caught there alone). |
| `effects_spawn_scroll` | `0x13B3C` | 9 | 20/20 | 22/22 | full | EAX = source, DL = offset, BL = flag, CL = count (`0x13B43`..`0x13B4B`). Forward arm, the descending arm with and without the `-0x80` special case, count 0 on each, the empty list. |
| `camera_box_overlap` | `0x140E4` | 18 | 5/5 | 20/20 | `0xFF` | Writes no memory: every case is an edge pair (touching = 1, one pixel apart = 0) on each of the four sides, with one actor's product negative on each axis, plus a negative width and a negative height. |
| `camera_projectile_clash` | `0x17BC8` | 10 | 8/8 | 25/25 | `0xFF` | Every callee a stub; stub EAX `0x100`, `0xFFFFFF01`, `0xFFFFFF00` on both predicates. Found the three AL tests (§3.1). |
| `camera_projectile_clash_sync` | `0x17BC8` | 5 | 8/8 | 5/5 | `0xFF` | The same entry with `0x181D0` run on both sides: the second call non-zero after the first returned zero (`q2`), which one stub EAX per case cannot produce. |
| `camera_project` | `0x17FA0` | 6 | 17/17 | 28/28 | 0 | The two anchors (null skip, the `+0x34` sign test), facing, page flag, index, the two page tails (stubs that store a box at their out dword), the two outs, the origin subtraction. Found the AL tests (§3.2, §3.3) and the sign extension of the origins (§3.4). |
| `camera_box_clip` | `0x15C30` | 11 | 21/21 | 30/30 | 0 | The box's four bytes clipped to the side's sprite rect. Cases place the rect's corner on each side of each compare, with bytes that wrap when scaled, a corner offset below 0 and above 255, an empty rect. |
| `sprite_node_build` | `0x14268` | 7 | 8/8 | 15/15 | 0 | The six fields the C `SpriteNode` shares with the raw node (`+0x08`, `+0x0C`, `+0x10`, `+0x14`, `+0x18`, `+0x20`). What it does not cover: §4. |
| `sprite_node_build_1c528` | `0x1C528` | 7 | 8/8 | 15/15 | 0 | The loader's copy: the bytes differ from `0x14268` only at `+0x23..+0x26`, the rel32 of the `call 0x1b544` (measured). Same C function, same cases. |
| `render_scroll_scene_a` | `0x387F4` | 5 | 5/5 | 17/17 | 0 | EBX is the caller's scene index (`mov ebx,eax` `0x38733`): every case has EBX = EAX. A height of 0, `0x7FFF`, negative, `0x8000`; the mode word `0x13`, `0x113`, `0x1300`. |
| `render_scroll_scene_b` | `0x38890` | 4 | 1/1 | 11/11 | 0 | `b1`'s `0xFFFF` gives the spawn argument `0x100FF` (the word zero-extended, then `+0x100`). |
| `render_scroll_setup` | `0x38730` | 4 | 1/1 | 17/17 | 0 | The `0x4F1D0` and `0x387F4` stubs store `0x107A4E` and `0x107A48`, so the read of each on the right side of its call is observed (`@a4astale`, `@a38late`). `0xBCD60` = 0 makes the allowed `0x2EA30` a no-op, as C2's rows have it. |

Totals: 17 rows, 285 mutants. Every mutant's port side was run ten times with identical output.

## §3 Corrections (each in its own commit, address-proved)

### §3.1 `camera_projectile_clash` tests AL of `0x140E4` and `0x181D0`

- **Raw.** `0x17BEA` `call 0x140e4`, `0x17BEF` `test al,al`, `0x17BF1` `je 0x17caa`; `0x17C35` `call 0x181d0`,
  `0x17C3A` `and eax,0xff`, `0x17C3F` `jne 0x17ca8`; `0x17C65` `call 0x181d0`, `0x17C6A` `and eax,0xff`,
  `0x17C6F` `jne 0x17ca8`.
- **Port before.** `camera_box_overlap(..) == 0`, `camera_sync_visible(..) != 0` (twice): the whole `int`.
- **Port after.** `(u8)camera_box_overlap(..) == 0u`, `(u8)camera_sync_visible(..) != 0u`.
- **Measured before the fix** (stub EAX `0x100`): `c2: eax (mask 0xFF): original 0x0, port 0x1`; `c4` and
  `c9`: `eax (mask 0xFF): original 0x1, port 0x0`. Mutants `@al140`, `@alsync`, `@alsync2` reproduce it.
- Not reached in play: the ported callees return a clean 0..3.

### §3.2 `camera_project` tests AL of `0x16AFC` and `0x164F4`

- **Raw.** `0x180C9` `call 0x16afc`, `0x180CE` `test al,al`, `0x180D0` `je 0x180e0`; `0x18108` `call 0x164f4`,
  `0x1810D` `test al,al`, `0x1810F` `je 0x18126`.
- **Port before / after.** `camera_page_tail_a(..) != 0` → `(u8)camera_page_tail_a(..) != 0u`; the same for `_b`.
- **Measured before the fix** (`j2`, stub EAX `0x100`): `byte 0x100AC0: original 0x00, port 0xA1`, `byte
  0x100AC8: original 0x00, port 0xB1`, `byte 0x10A5A9: original 0x00, port 0x01`. Mutants `@ala`, `@alb`.

### §3.3 `camera_project` tests AL of `0x1A570`

- **Raw.** `0x1819B` `call 0x1a570`, `0x181A0` `test al,al`, `0x181A2` `jne 0x181ac`.
- **Port before / after.** `!camera_actor_bit15_clear(side)` → `(u8)camera_actor_bit15_clear(side) == 0u`.
- **Measured before the fix** (`j2`, stub EAX `0x100`): `byte 0x10A584: original 0xE3, port 0xF7`. Mutant `@al15`.

### §3.4 `camera_project` sign-extends the sprite origins

- **Raw.** `0x1818D` `mov ecx,[eax+2]`, `0x18193` `sar ecx,0x10`; `0x18190` `mov edx,[eax+4]`, `0x18198` `sar
  edx,0x10`: the header's `+4` and `+6` words, sign-extended, are what `0x181BB` / `0x181C2` subtract from
  `[out_b]` / `[out_a]`.
- **Port before.** `(s32)((u32)DSD(sprite + 2u) >> 16)` and the same at `+4`: a logical shift, so a negative
  origin was subtracted as `0x0000FFxx`. The low word of each out dword agreed, the high word did not.
- **Port after.** `(s32)DSD(sprite + 2u) >> 16`, `(s32)DSD(sprite + 4u) >> 16` (as `camera_sprite_rect`, the
  helper of `0x140E4` and `0x15C30`, already had it).
- **Measured before the fix** (`j0`, origin y `0xFFFB`): `byte 0x10A596: original 0x00, port 0xFF`, `byte
  0x10A597: original 0x00, port 0xFF`. Mutants `@originzx` (`j1`, `j5`), `@originzy` (`j0`, `j2`, `j3`, `j4`).
- This one is a behaviour change for a sprite whose origin word is negative. `frames/` regenerated after it
  (`--check 820`) is byte-identical on the two frames compared (`frame_0400.idx`, `frame_0819.idx`) and
  `run_tests` passes; the oracles were not run.

## §4 Named gaps, unhit blocks, what a row does not cover

- **`0x12CD4`** (93 bytes): no C function to bind. Its body is the block commented `0x12CD4` inside
  `camera_y_clamp`; a function of its own would be a new `/* 0x12CD4` header, which the track's limits
  exclude. It is exercised through d3e's `camera_y_clamp` row, which runs it from the original's bytes:
  re-measured here, the row's cases execute 9 of its 9 blocks.
- **`0x52106`** (`gfx_screen_reset`, gfx.c): `in al,dx` at `0x5212E` (the retrace wait on port `0x3DA`);
  the original stops there (`unmodeled in at 0x5212E`, measured). The DAC `out`s follow at `0x5213F`..`0x52147`,
  and the third `0x51F72` call fills `0xA0000`, the aperture.
- **`0x1C470`** (the palette flush, gfx.c): `in al,dx` at `0x1C486` (`unmodeled in at 0x1C486`, measured), then
  `out dx,al` at `0x1C4AA`, `0x1C4CA`, `0x1C4CE`, `0x1C4D5`. As d3d found.
- **`0x51ED8`** (`sprite_blit_aperture`): `add edi,0xa0000` at `0x51F1B`: the destination is the VGA aperture,
  which in the flat image is game data and in the port is the host buffer `gfx_aperture()`, outside `mem[]`
  (AGENTS.md, aperture rule). Its renderer is reached by `call [edx*4+0x80c8c]` (`0x51F35`). The one part
  comparable in `mem[]` is the node's `+0x14` store (`0x51F2F`); a row on that store alone was not written.
- **`camera_resolve_sprite`'s nine unhit blocks** (`0x1B57C`, `0x1B585`, `0x1B58E`, `0x1B5A6`, `0x1B5AF`,
  `0x1B5C2`, `0x1B5CB`, `0x1B5D4`, `0x1B5E0`): `0x1B544`'s non-resident arms, counted as `0x16308`'s because it
  tail-jumps (`jmp 0x1b544` `0x16316`). The fixture's entries are preloaded (`test [ecx+0xc],0x1000000`
  `0x1B569`); the arms read a block descriptor and call the EMS/file loader, which the port replaces.
- **`sprite_node_build` (both entries) does not cover:** the raw's `+0x24` store of the descriptor handle
  (`mov [ebx+0x24],eax` `0x14287`; the C struct keeps none, so the fixture seeds `+0x24` with that value); the
  C function's zeroing of `pal_ptr`, the clip fields, `stride` and `row` (PORT, sprite.c: the binding stores
  back only the six shared fields); the port's guard on a zero table entry or an unresolvable descriptor.
- **`effects_spawn_*`:** the lock's restore after the unlink (`0x13D7A`, `0x13E56`, `0x13B75`) is not
  observable (no recorded call lies between it and the next `mov [0x9af3c],1`). The count is re-read from the
  source on every iteration in the raw (`0x13DB2`); no case aliases the source and the record. The port's
  guard on a zero free head has no case (the raw would unlink record 0).
- **`camera_bitplane_accum`:** a width word below -7 (a negative byte width: a 4 GB memset on both sides) and a
  count of `0x6BCA1B` or more (`count * 0x26` overflows) have no case.
- **`camera_box_clip`:** the first two compares of an axis are not observable at equality (either arm stores
  the same bytes); `@lt`/`@part` were equivalent and were replaced by `@acc8`/`@lensext`.
- **`render_scroll_scene_a`:** EBX at entry is the caller's; the C function takes the index once. A case with
  EBX different from EAX would differ and is not a state the one caller produces.
- **`render_scroll_scene_b`:** the raw's two dropped `0x1B544` resolves (`0x388CA`, `0x388E5`) are not made by
  the port. The row cannot see them (preloaded entries, `0x1B544` allowed). See §7.
- **Equivalent mutants dropped:** `camera_sprite_height@sumge` (`frame + count == height` stores the same
  count), `camera_projectile_clash@nosecond` in the stubbed row (kept in the `_sync` row).

## §5 Store sweep

Every field a row's function writes is seeded with a value different from the one written, between seeded
neighbours; one dropped or widened store per row is a registered mutant: `camera_y_commit@wide`/`@lo`,
`camera_bitplane_accum@nofill`/`@litclear`, `effects_spawn_darken@noe`/`@nocount`/`@countw`/`@lockend`,
`effects_spawn_pulse@nocopy`, `effects_spawn_scroll@no14`/`@no414`/`@off10w`, `camera_project@facingw`/
`@noclear`/`@ac0keep`/`@ac8keep`/`@nosuba`/`@nosubb`, `camera_box_clip@b2shl`/`@b3mul`/`@nozero`/`@rzero`,
`sprite_node_build@zero`/`@hflip8`, `render_scroll_scene_a@a55w`/`@a48d`/`@bdfbc`,
`render_scroll_scene_b@a52d`/`@bdfc0`, `render_scroll_setup@noa4a`/`@noa4c`/`@noa54`. `camera_box_overlap`
and `camera_resolve_sprite` write nothing; their comparison is EAX. No store was found missing or too wide
in the port beyond §3.

## §6 Queue entries not done

| entry | size | why |
|---|---|---|
| `0x12CD4` | 93 | named gap, §4 (no C function) |
| `0x52106` | 83 | named gap, §4 (`in` at `0x5212E`) |
| `0x1C470` | 143 | named gap, §4 (`in` at `0x1C486`) |
| `0x51ED8` | 109 | named gap, §4 (the aperture) |
| `0x51E5C` | 121 | not done (time). What is known: it guards on `+0x18` and `+0x14` non-zero (`0x51E5C`, `0x51E65`), saves the node's `+0x14` and `+0x30` (`0x51E74`, `0x51E77`), resolves `+0x20`, reads the bank dword `0x81310[byte [[+0x1C]+8]]`, forms the destination `[0x1088F8 + y*4] + [0xE87A4] + x`, stores `+0x14 - +0x34` and calls `[(+0x10 & 0x7F) * 4 + 0x80C8C]` (`0x51EBF`). The table's live targets are `0x5D218`, `0x58CBD`, `0x5215C`, `0x57F80`, `0x5D28F`, `0x57FFB` (the rest `0x51E58`). A row needs those assembly renderers allowed, a back buffer inside the image, and a binding that maps the raw node (clip fields from `+0x28`, since `+0x24` is the descriptor handle) to the C `SpriteNode`. |
| `0x14328` | 610 | not done (time); it walks the display list into C `SpriteNode`s and calls `0x14268` and `0x51E5C`, so it waits on the row above. |

## §7 Doubts

- `render_scroll_scene_b` skips the raw's two `0x1B544` calls with the comment "0x1B544 has no side effect in
  the port". `res_resolve` does have one on an entry that is neither preloaded nor loaded: it sets the loaded
  flag and runs the loader presentation (`res.c`, the `RES_FLAG_PRELOAD | RES_FLAG_LOADED` test). If a scene's
  `0xBDF9C` sprite lives in such an entry, the raw presents the loader inside the setup and the port at the
  sprite's first draw. Unverified: the harness cannot run `0x1B544`'s non-resident arms. Not changed.
- `camera_actor_bit15_clear` is also tested as a whole `int` in `camera_page_tail_a`/`_b` and
  `camera_box_palette` (d2d's rows, which allow `0x1A570`). The helper carries a seam now, so those rows could
  stub it; not done here (another batch's rows).
- §3.4 is unproven against the oracles.
