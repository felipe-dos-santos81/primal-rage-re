# Track D1, batch d1e: derivations (game/fight.c, first half)

Batch `d1e` of track D1 (brief `.superpowers/sdd/2026-10-11-reverse-d1/brief.md`): differential rows
for 21 already-ported functions of `port/src/game/fight.c` that the callee scan never reached. Ground
truth: the fixed-up image dumped by `build/diffrun --image-out` (linear addresses), disassembled with
capstone through `tools/diff_emu.py`. Every expectation in a case comes from running the original's
bytes; every value in a case is either read from the image or a sentinel chosen here. The claim of a
`VERIFIED` row is the harness's narrow one: equivalence on the exercised blocks and inputs, with the
callees stubbed or run as stated.

All 21 queue entries are rowed. One raw-over-port difference was found and corrected (§3).

## §1 The members

`EAX read by the callers` is what fixes the row's `eax_mask`. For a function whose C is `void` the mask
is 0 and rests on the call sites listed (the first access of EAX after each return, walked straight-line
from the return address; a direct `call` is followed into its callee's entry). A site marked *open* does
not rewrite the whole of EAX before its own function returns or branches, so part of the original's EAX
flows on to code this batch does not row: the port's C cannot pass it on, and it is settled only when
that caller gets its row (E3 §E3.8's rule).

| addr | port function | size | registers read at entry | EAX at return and the callers that read it |
|---|---|---|---|---|
| `0x13134` | `fight_13134` | 71 | none | 1 / 0, whole (`mov eax,1` 0x13150, 0x13172; `xor eax,eax` 0x13178). Caller 0x4BDBA: `test eax,eax` 0x4BDBF. Mask `0xFFFFFFFF`. |
| `0x1D4D0` | `fight_hud_rate` | 112 | EAX (side) | the table byte, `and eax,0xff` (0x1D511, 0x1D526, 0x1D536). Caller 0x1D563: `mov edi,eax` 0x1D568. Mask `0xFFFFFFFF`. |
| `0x1D74C` | `fight_cmd_bits_30` | 21 | EAX (side) | `and eax,0xffff` 0x1D758 then `setne al` 0x1D75D: EAX = 0 / 1. Caller 0x1D5FF: `test al,al` 0x1D604. Mask `0xFFFFFFFF` (conservative). |
| `0x1D7B8` | `fight_select_marker_spawn` | 87 | EAX (side), EDX (class), EBX (y: never written, it is 0x2AE14's a4) | void in C; the original returns 0x2AE14's EAX. 0x43A84: `mov eax,[edi+0x10814c]` 0x43A89 rewrites. 0x44035 and 0x4432D: `mov ax,[edi*2+0xc8924]` rewrites AX only, then the function returns (*open*: the upper word reaches the callers of 0x43EA0 / 0x4418C). Mask 0. |
| `0x1D810` | `fight_select_marker_release` | 39 | EAX (side) | void; EAX = the side (no record) or 0x2B150's. 0x43767, 0x444F7: `mov eax,1` (0x4376C, 0x444FC). 0x25C64: `call 0x20EF8`, which loads EAX at 0x20F05 (`mov eax,ecx`) before any read. 0x43773, 0x44503: `call 0x43818`, `xor eax,eax` at 0x4381B. Mask 0. |
| `0x1D838` | `fight_hud_badge_spawn` | 87 | EAX (side), EDX (character), EBX (y) | void; 0x2AE14's EAX. 0x27770: `lea eax,[edx*8]` 0x2777E rewrites. 0x25F47: `mov al,[0x10810d]` 0x25F4E (AL only), 0x298F3: `mov al,[0x104b09]` 0x298F8 (AL only) (*open*). 0x1D9D5, 0x1DDBF: no access before the loop branch (0x1D9F7, 0x1DDE3); 0x1DBFA: none before `ret` 0x1DC09 (*open*). Mask 0. |
| `0x1DA08` | `fight_hud_pulse` | 122 | none | void; EAX is scratch (the last reload computation or the stub's). 0x263AA, 0x26696: `call 0x27FA8`, `xor eax,eax` 0x27FAC. 0x274E7: `call 0x272DC`, `lea eax,[edx*8]` 0x272E6. 0x29B4F: `call 0x29970`, `xor eax,eax` 0x29972. 0x26864: `mov ax,[0xef6dc]` 0x2686F (AX only; *open*). Mask 0. |
| `0x29CDC` | `fight_dust_value` | 30 | EAX (side), EDX (character) | the table dword, whole; every caller stores it at a descriptor's +0x10 (0x495CC, 0x49778, 0x49987, 0x4BAF1, 0x4C996, 0x4D00C, 0x4DCEC, 0x4E3EE). Mask `0xFFFFFFFF`. |
| `0x2BE00` | `fight_2be00` | 27 | EAX (record) | the pset entry's +4 dword, whole (31 direct callers; e.g. `mov ebx,eax` 0x493FC, `cmp eax,edx` 0x49A50). Mask `0xFFFFFFFF`. |
| `0x2BE1C` | `fight_2be1c` | 45 | EAX, EDX (records) | the difference, whole (`test eax,eax` 0x49DEE, 0x4D47B; `mov edx,eax` 0x4ABB3). Mask `0xFFFFFFFF`. |
| `0x2BE4C` | `fight_2be4c` | 16 | EAX (record), EDX (value) | the sum, whole (`mov edx,eax` 0x499CF; `mov [ebx+0x14],eax` 0x4B268). Mask `0xFFFFFFFF`. |
| `0x2C320` | `fight_scene_crowd` | 102 | EAX (scene) | void; EAX = the scene (count 0) or the last spawn's. Its one caller 0x412D8 goes on to `call [edi*4+0xc7f58]` 0x412DD and returns (*open*). Mask 0. |
| `0x33C18` | `fight_char_reset` | 67 | EAX (side) | void; EAX = the slot address. 0x257EB, 0x27176 (`mov eax,imm`), 0x25AA1 (`mov eax,ebx`), 0x27722, 0x29893, 0x43E4E, 0x44496 (`xor eax,eax`), 0x28E17 (`lea eax,[esi*8]`) rewrite. 0x292DC: `xor ah,ah` (AH only); 0x257F7: `call 0x46594`, which branches (0x4659D) before touching EAX; 0x41359: branches at 0x41360 (*open*, three sites). Mask 0. |
| `0x33F08` | `fight_health_bars` | 302 | none (EDX's upper bits never reach a result: only DX is tested and extended, 0x33F98/0x33F9D) | void; EAX is scratch. 0x25457: `mov ax,[0x104b00]` 0x2545C (AX only). 0x11C44, 0x25540, 0x25586, 0x255C0: pops, then `ret`. 0x11E94: `call 0x10DB0`, which loads AH only (0x10DB1). 0x425EA: no access before the branch at 0x425F8 (*open*, all seven). Mask 0. |
| `0x36E2C` | `fight_position_gate` | 73 | EAX (slot) | AL = 1 / 0 (`mov al,1` 0x36E6D, `xor al,al` 0x36E71; the upper three bytes are the slot's). Its one caller 0x34B8E: `test al,al` 0x34B93. Mask `0xFF`. |
| `0x38BEC` | `fight_side_scratch_clear` | 70 | EAX (side) | void; EAX = side * 0x40 + 0x40. Its one caller 0x20F07: `xor ah,ah` 0x20F22 (AH only; *open* — the port's 0x20EF8 was written against that value, record §48-U). Mask 0. |
| `0x3C5CC` | `fight_slot_clear` | 23 | none | void; EAX untouched. All 12 callers rewrite EAX first (`xor eax,eax` at 0x2625E, 0x263FE, 0x2654A, 0x266EE, 0x273ED, 0x27506, 0x28474, 0x28795, 0x296C2, 0x29A55; `lea eax,[esi*8]` at 0x26C9E, 0x26F6B). Mask 0. |
| `0x413C8` | `fight_413c8` | 99 | none | void; EAX = the last spawn's. Its one caller 0x41456: `xor ah,ah` 0x4145D (AH only; *open*). Mask 0. |
| `0x4246C` | `fight_stage_marks_clear` | 30 | none | void; EAX = 0x65490's return with AH cleared. 0x25B47: `mov eax,0x3d` 0x25B4C. 0x28B37: `xor al,al` 0x28B46 (AL only; *open*). Mask 0. |
| `0x4248C` | `fight_stage_mark_drop` | 67 | AL (class), DL (side) | void; EAX = 7 (the loop counter). 0x43E58: `mov al,[ebx+0x10816a]` 0x43E5D, stored (0x43E63), then `mov eax,2` 0x43E6F; 0x444A0: `mov al,[ecx+0x10816a]` 0x444A5, stored (0x444AB), then `mov eax,0x6c` 0x444B1. Straight-line at both sites: only the rewritten AL is read before the whole register is rewritten. Mask 0. |
| `0x43964` | `fight_char_entry_spawn` | 164 | EAX (side) | void; EAX = 0x2A17C's. 0x43796, 0x43B59, 0x44517: `mov eax,edx` (0x4379B, 0x43B5E, 0x4451C). Mask 0. |

**Exports.** Ten of the 21 were file-`static` in `fight.c` and could not be bound from `diffrun`. Following the
precedent of E3 (`hit_anim_start_b`), C3c and C3g, they lose `static` and gain a declaration in `fight.h`
(one block after `fight_slot_clear`): `fight_13134`, `fight_hud_rate`, `fight_cmd_bits_30`,
`fight_dust_value`, `fight_2be00`, `fight_2be1c`, `fight_2be4c`, `fight_position_gate`,
`fight_side_scratch_clear`, `fight_413c8`. Linkage only: no body, comment or call site changes, no
`fn_register`, no new `/* 0xADDR` header. No seam was added: every stubbed callee already carried one.

**Callees** (each constant reused from an earlier batch; `clobbers` is `E.callee_clobbers` over the
image, re-derived by `test_each_stub_declares_the_registers_its_callee_clobbers`):

| callee | how | declaration | used by |
|---|---|---|---|
| `0x33A10` | allow | runs on both sides (verified row `fighter_ctx_swap`) | `0x1D4D0` |
| `0x2B150` | stub | `P7_2B150`: `(eax)`, `ret`, clobbers none | `0x1D810`, `0x1D7B8`, `0x1D838` |
| `0x2AE14` | stub | `SPAWN`: `(eax, edx, ecx, ebx, s0)`, `ret 4`, clobbers `ebx ecx edx` | `0x1D7B8`, `0x1D838`, `0x2C320`; with a stub write `0x413C8` (`D1E_SPAWN_W`) and `0x43964` (`D1E_SPAWN_A`) |
| `0x2BC30` | stub | `ANIM_BEGIN`: `(eax, edx, s0)`, `ret 4`, clobbers `edx` | `0x1DA08` |
| `0x2A408` | stub | `C1_SPRITE_ID`: `(eax, edx)`, `ret`, clobbers `edx` | `0x33F08` |
| `0x2BCF4` | stub | `P6_2BCF4`: `(eax, edx)`, `ret`, clobbers `edx` | `0x43964` |
| `0x2A17C` | stub | `PALETTE`: `(eax, edx, ebx)`, `ret`, clobbers `edx` | `0x43964` |
| `0x65490`, `0x654C7` | allow | the runtime byte fill and its dword helper (`call 0x654C7` at 0x654A7): their bytes run in the original; the port fills the seven bytes itself. Runtime (>= 0x5D000), no row | `0x4246C` |

No original caller here reads a register a stub clobbers: after `0x2AE14` `0x1D7B8`/`0x1D838` use ESI
(0x1D803, 0x1D883), `0x2C320` reloads everything from ESI/EDI, `0x413C8` uses ESI/EDI and reloads the
rest, `0x43964` reloads EBX/ECX/EDX and uses EDI; after `0x2BC30` `0x1DA08` reloads EDX (0x1DA3D) and
uses EBX/ECX/ESI; after `0x2A408` `0x33F08` uses ECX (0x3401B, preserved) and reloads EDI; after
`0x2B150` (clobbers none) `0x1D7B8`/`0x1D810`/`0x1D838` use EDX.

## §2 The rows (measured)

Each row is a `Spec` in `tools/diff_verify.py` (`D1E_SPECS`, fixtures `d1e_*`), a binding `b_d1e_*` and
mutants `m_d1e_*` in `port/tests/diff_runner.c` (helpers `d1e_*_core`), and pins in
`tools/tests/test_diff_verify.py` (`D1E_MASKS`, `D1E_KINDS`, `D1E_CASES`). Measured with
`python3 tools/diff_verify.py --image /tmp/pr_d1e_img.bin --function <row> --self-check`. Every row:
verdict `VERIFIED`, no problem, no unhit block, no read or write outside the image on the original
side, every mutant a `MISMATCH` with a counted difference and no port error. What was run, and by
whom, is in §7.

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fight_13134` | `0x13134` | 12 | 7/7 | 7 | `0xFFFFFFFF` | no callee. Both bounds on each side at the boundary (x1/x2: -0x3300; x4/x5: 0x3300), the extreme values (x8/x9), -1 and 0. Each record's +0x14 and +0x1C hold a value with the other verdict. |
| `fight_hud_rate` | `0x1D4D0` | 10 | 8/8 | 10 | `0xFFFFFFFF` | `0x33A10` allow. The image's seven 0xA769C rows are all `02 04 08 0C`; the cases poke distinct rows (a harness value). +0x5D at each column boundary (0x3E/0x3D, 0x30/0x2F, 0x23/0x22), 0x80 and 0xFF (zero-extended, `and edx,0xff` 0x1D4F3); the character 0x80 (zero-extended, `and eax,0xff` 0x1D4E4). |
| `fight_cmd_bits_30` | `0x1D74C` | 7 | 1/1 | 6 | `0xFFFFFFFF` | no callee. Bit 4 alone, bit 5 alone, both, every other bit of the low byte (0xFFCF), the two bits in the high byte only (0x3000); the other side's word holds the opposite verdict. |
| `fight_select_marker_release` | `0x1D810` | 4 | 3/3 | 8 | `0x0` | `0x2B150` stub. Each side with and without a record. The zero store follows the call (`call #0 memory` alone catches `@order`). |
| `fight_select_marker_spawn` | `0x1D7B8` | 6 | 6/6 | 13 | `0x0` | `0x2B150`, `0x2AE14` stubs. Sides 0, 1, 2 and 0x100 (m5: `test esi,esi` is the whole register); the release's zero store is visible only in the memory at the spawn (`call #1 memory` alone catches `@nozero`). The image's 0xA78B0 row is distinct per class. |
| `fight_hud_badge_spawn` | `0x1D838` | 6 | 6/6 | 13 | `0x0` | as `0x1D7B8`, table 0xA7760 (distinct per character). `@table` swaps the two tables in each row. |
| `fight_hud_pulse` | `0x1DA08` | 7 | 6/6 | 18 | `0x0` | `0x2BC30` stub. Countdown words 2, 1, 0, 0x8001, 0x8000 (wraps to 0x7FFF: no pulse), 0x7FFF, 0x0100; the +0x5A byte at the clamp (0x60 -> 0xC, 0x62 -> 0xB raised, 0x5E -> 0xD), 0x78, 0x79, 0x80, 0xF8 (negative reloads, raised). |
| `fight_dust_value` | `0x29CDC` | 6 | 3/3 | 5 | `0xFFFFFFFF` | no callee. The flag 0, 1, 2 and 0x80 (`test bl,bl`); the image's two tables differ in every row. |
| `fight_2be00` | `0x2BE00` | 4 | 1/1 | 6 | `0xFFFFFFFF` | no callee. Index words 0, 1, 0x7F, 0x0102 (a byte read gives 2); +0x58 non-zero (a dword read leaves the entry). Limit: §4. |
| `fight_2be1c` | `0x2BE1C` | 4 | 1/1 | 9 | `0xFFFFFFFF` | no callee. Both orders, 0x80000000 - 0x7FFFFFFF (wraps to 1), the same record twice. |
| `fight_2be4c` | `0x2BE4C` | 5 | 1/1 | 6 | `0xFFFFFFFF` | no callee. +0x44 positive, negative (`sar`), with a low word (0x00018000: `>> 15` differs), 0x8000FFFF, and a wrapping sum. |
| `fight_scene_crowd` | `0x2C320` | 4 | 5/5 | 15 | `0x0` | `0x2AE14` stub. Count 0, 1, 2, 3; a3/a4 negative, the +0xB byte 0x80 and 0xFF, the descriptor index 0x80. The count and table rows are poked (even scenes; decoys in the others). Limit: §4. |
| `fight_char_reset` | `0x33C18` | 2 | 1/1 | 13 | `0x0` | no callee. Both sides; every written field and its neighbours seeded non-zero in both slots. |
| `fight_health_bars` | `0x33F08` | 8 | 22/22 | 21 | `0x0` | `0x2A408` stub. Characters 0..7 and 0xFF (all seven jump-table targets and the `ja` default); the difference at -1, 0, 0x4AF, 0x4B0, 0x4B1; +0x41 = 0x20 and 0xDF; both arms of the +0x29 bit from both pre-states; a side with no slot (each, and both). Case a7 pokes two constants: the correction of §3. |
| `fight_position_gate` | `0x36E2C` | 15 | 12/12 | 14 | `0xFF` | no callee. Each of the three opening conditions alone, +0x52 = 0..8, +0x54 = 0, 1, 2, 0x80 (unsigned `ja`), +0x42 with only the wrong bit (0x20, 0xEF). |
| `fight_side_scratch_clear` | `0x38BEC` | 2 | 3/3 | 12 | `0x0` | no callee. Both sides; the 14 words 0x107D16..0x107D31 and the 130 bytes 0x107A7F..0x107B00 seeded non-zero. |
| `fight_slot_clear` | `0x3C5CC` | 1 | 1/1 | 4 | `0x0` | no callee. The three dwords and the dwords around them seeded. |
| `fight_413c8` | `0x413C8` | 3 | 3/3 | 11 | `0x0` | `0x2AE14` stub with a stub write (`D1E_SPAWN_W`). The first handle's +0x56 word 0x1234, 0xFBFF, 0x0400; in q0/q2 the i = 3 spawn's stub write changes it, so the per-pass reload shows (`@once`). |
| `fight_stage_marks_clear` | `0x4246C` | 2 | 1/1 | 5 | `0x0` | `0x65490`, `0x654C7` allow (runtime). The seven bytes and the count byte with their four neighbours seeded. |
| `fight_stage_mark_drop` | `0x4248C` | 8 | 5/5 | 12 | `0x0` | no callee. Bit 7 set in a matching byte, key 0, the arguments above a byte (s3: EAX 0x105, EDX 0x101), side 2 and class 0x85 (key bit 7: no match), side 4 (`shl dl,6` drops it), a match in the last byte only, the count wrapping below 0. |
| `fight_char_entry_spawn` | `0x43964` | 5 | 1/1 | 15 | `0x0` | `0x2AE14` (with a stub write, `D1E_SPAWN_A`), `0x2BCF4`, `0x2A17C` stubs. Both sides; characters 0, 1, 4, 6 and 0xFF (-1: `sar esi,0x18`); words >= 0x8000 poked into the three word tables (zero-extended). The 0xC88A6 words are poked distinct (the image's are equal within characters 0..3 and 4..6). |

The counter line of the full `--self-check` was not measured on this branch: §7.

### The mutants (measured: what detects each, and on which cases)

The tags are the comments of each `d1e_*_core` in `diff_runner.c`. `eax`, `byte`, `call #i` and
`call #i memory` are the four differences `--self-check` counts.

`fight_13134`:

| mutant | detected by | cases |
|---|---|---|
| `@lt` | eax | x1, x2 |
| `@gt` | eax | x4, x5 |
| `@unsigned` | eax | x1, x10, x11, x2, x4, x5, x6, x7 |
| `@field` | eax | x1, x10, x11, x2, x3, x4, x5, x6, x7, x9 |
| `@rec1` | eax | x1, x4, x6, x7 |
| `@rec0` | eax | x2, x5, x6, x7 |
| `@or` | eax | x1, x2, x6, x7 |

`fight_hud_rate`:

| mutant | detected by | cases |
|---|---|---|
| `@side` | eax | r0, r1, r2, r3, r4, r5, r6, r7, r8, r9 |
| `@ch` | eax | r0, r1, r2, r3, r4, r5, r6, r7, r8, r9 |
| `@chsext` | eax | r9 |
| `@scale` | eax | r0, r1, r2, r3, r4, r5, r7, r8, r9 |
| `@field` | eax | r1, r2, r3, r4, r5, r8, r9 |
| `@sext` | eax | r7, r8 |
| `@b0` | eax | r1 |
| `@b1` | eax | r3 |
| `@b2` | eax | r5 |
| `@col` | eax | r1, r2, r3, r4, r9 |

`fight_cmd_bits_30`:

| mutant | detected by | cases |
|---|---|---|
| `@side` | eax | b0, b1, b2, b3, b4, b5 |
| `@scale` | eax | b3, b4 |
| `@b4` | eax | b2 |
| `@b5` | eax | b1, b6 |
| `@word` | eax | b4, b5 |
| `@ret` | eax | b1, b2, b3, b6 |

`fight_select_marker_release`:

| mutant | detected by | cases |
|---|---|---|
| `@side` | byte, call #0 | k0, k1, k2, k3 |
| `@scale` | byte, call #0 | k1, k3 |
| `@nozero` | byte | k0, k1 |
| `@width` | byte | k0, k1 |
| `@order` | call #0 memory | k0, k1 |
| `@nocall` | call #0 | k0, k1 |
| `@arg` | call #0 | k0, k1 |
| `@always` | call #0 | k2, k3 |

`fight_select_marker_spawn`:

| mutant | detected by | cases |
|---|---|---|
| `@slot` | byte | m0, m1, m2, m3, m4, m5 |
| `@nocall` | call #0, call #0 memory, call #1 | m1, m3, m5 |
| `@nozero` | call #1 memory | m1, m3, m5 |
| `@scale` | call #0, call #1 | m1, m2, m3, m4, m5 |
| `@a2` | call #0, call #1 | m0, m1, m2, m3, m4, m5 |
| `@a3` | call #0, call #1 | m0, m1, m2, m3, m4, m5 |
| `@y` | call #0, call #1 | m0, m1, m2, m3, m4, m5 |
| `@a5` | call #0, call #1 | m0, m1, m2, m3, m4, m5 |
| `@store` | byte | m0, m1, m2, m4, m5 |
| `@width` | byte | m0, m1, m2, m4, m5 |
| `@eq1` | call #0, call #1 | m4, m5 |
| `@side8` | call #1 | m5 |
| `@table` | call #0, call #1 | m0, m1, m2, m3, m4, m5 |

`fight_hud_badge_spawn`:

| mutant | detected by | cases |
|---|---|---|
| `@slot` | byte | h0, h1, h2, h3, h4, h5 |
| `@nocall` | call #0, call #0 memory, call #1 | h1, h3, h5 |
| `@nozero` | call #1 memory | h1, h3, h5 |
| `@scale` | call #0, call #1 | h1, h2, h3, h4, h5 |
| `@a2` | call #0, call #1 | h0, h1, h2, h3, h4, h5 |
| `@a3` | call #0, call #1 | h0, h1, h2, h3, h4, h5 |
| `@y` | call #0, call #1 | h0, h1, h2, h3, h4, h5 |
| `@a5` | call #0, call #1 | h0, h1, h2, h3, h4, h5 |
| `@store` | byte | h0, h1, h2, h4, h5 |
| `@width` | byte | h0, h1, h2, h4, h5 |
| `@eq1` | call #0, call #1 | h4, h5 |
| `@side8` | call #1 | h5 |
| `@table` | call #0, call #1 | h0, h1, h2, h3, h4, h5 |

`fight_hud_pulse`:

| mutant | detected by | cases |
|---|---|---|
| `@gt` | byte, call #0, call #0 memory, call #1 | p0, p1, p2, p6 |
| `@u` | byte, call #0, call #1 | p3, p6 |
| `@rec` | call #0, call #1 | p0, p1, p2, p3, p6 |
| `@stream` | call #0, call #1 | p0, p1, p2, p3, p6 |
| `@frame` | call #0, call #1 | p0, p1, p2, p3, p6 |
| `@sext` | byte, call #1 memory | p3, p6 |
| `@w16` | byte, call #1 memory | p3, p6 |
| `@clamp` | byte, call #1 memory | p2 |
| `@order` | call #0 memory, call #1 memory | p0, p1, p2, p3, p6 |
| `@byte` | byte, call #1 memory | p0, p1, p2, p3, p6 |
| `@side` | byte, call #1 memory | p0, p1, p2 |
| `@nostore` | byte, call #0 memory, call #1 memory | p0, p1, p2, p3, p4, p5, p6 |
| `@decb` | byte, call #0 memory, call #1 memory | p3, p4, p5, p6 |
| `@relb` | byte, call #1 memory | p3, p6 |
| `@one` | byte, call #0, call #1 | p0, p1, p2, p3, p4, p5, p6 |
| `@k78` | byte, call #1 memory | p0, p1, p2 |
| `@norel` | byte, call #1 memory | p0, p1, p2, p3, p6 |
| `@noclamp` | byte, call #1 memory | p2, p3, p6 |

`fight_dust_value`:

| mutant | detected by | cases |
|---|---|---|
| `@side` | eax | d0, d1, d2, d3, d5 |
| `@scale` | eax | d1, d2, d3, d4, d5 |
| `@one` | eax | d4, d5 |
| `@inv` | eax | d0, d1, d2, d3, d4, d5 |
| `@always0` | eax | d1, d3, d4, d5 |

`fight_2be00`:

| mutant | detected by | cases |
|---|---|---|
| `@byte` | eax | t2 |
| `@dword` | eax | t0, t1, t2, t3 |
| `@field` | eax | t0, t1, t2, t3 |
| `@off` | eax | t0, t1, t2, t3 |
| `@scale` | eax | t1, t2, t3 |
| `@base` | eax | t0, t1, t2, t3 |

`fight_2be1c`:

| mutant | detected by | cases |
|---|---|---|
| `@same` | eax | e0, e1, e2 |
| `@swap` | eax | e0, e1, e2 |
| `@add` | eax | e0, e1, e2, e3 |
| `@abyte` | eax | e2 |
| `@bbyte` | eax | e2 |
| `@adword` | eax | e0, e1, e2, e3 |
| `@bdword` | eax | e0, e1, e2, e3 |
| `@aoff` | eax | e0, e1, e2, e3 |
| `@boff` | eax | e0, e1, e2, e3 |

`fight_2be4c`:

| mutant | detected by | cases |
|---|---|---|
| `@shr` | eax | v1, v3 |
| `@sh15` | eax | v2, v3 |
| `@const` | eax | v0, v1, v2, v3, v4 |
| `@sub` | eax | v0, v1, v2, v4 |
| `@field` | eax | v0, v1, v2, v3, v4 |
| `@nodbl` | eax | v0, v1, v2, v3, v4 |

`fight_scene_crowd`:

| mutant | detected by | cases |
|---|---|---|
| `@nscale` | byte, call #0, call #10, call #11, call #2, call #3, call #4, call #5, call #6, call #7, call #8, call #9 | c1, c2, c3 |
| `@tscale` | byte, call #0, call #0 memory, call #1, call #1 memory, call #2, call #2 memory | c1, c2, c3 |
| `@store` | byte, call #0 memory, call #1 memory, call #2 memory | c1, c2, c3 |
| `@stride` | call #1, call #2 | c2, c3 |
| `@late` | call #0 memory, call #1 memory, call #2 memory | c1, c2, c3 |
| `@one` | call #0, call #1, call #2 | c1, c2, c3 |
| `@hisext` | call #0 | c1, c3 |
| `@a5hi` | call #0, call #1, call #2 | c1, c2, c3 |
| `@a5w` | call #0, call #1, call #2 | c1, c2 |
| `@idxsext` | call #1 | c2 |
| `@idx` | call #0, call #1, call #2 | c2, c3 |
| `@a3zext` | call #0, call #1 | c1, c2 |
| `@a4zext` | call #0, call #2 | c1, c2 |
| `@swap` | call #0, call #1, call #2 | c1, c2, c3 |
| `@a2` | call #0, call #1, call #2 | c1, c2, c3 |

`fight_char_reset`:

| mutant | detected by | cases |
|---|---|---|
| `@side` | byte | z0, z1 |
| `@stride` | byte | z1 |
| `@s7f` | byte | z0, z1 |
| `@s80` | byte | z0, z1 |
| `@s3c` | byte | z0, z1 |
| `@s82` | byte | z0, z1 |
| `@s5b` | byte | z0, z1 |
| `@w` | byte | z0, z1 |
| `@wscale` | byte | z1 |
| `@val` | byte | z0, z1 |
| `@wbyte` | byte | z0, z1 |
| `@s3cw` | byte | z0, z1 |
| `@s81` | byte | z0, z1 |

`fight_health_bars`:

| mutant | detected by | cases |
|---|---|---|
| `@one` | byte, call #0, call #1 | a0, a1, a2, a3, a5, a7 |
| `@idxf` | byte, call #0 memory, call #1 memory | a0, a1, a2, a3, a4, a5, a7 |
| `@ch` | byte, call #0 memory, call #1 memory | a0, a2, a3, a4, a5, a7 |
| `@mask` | byte, call #0 memory, call #1 memory | a0, a4 |
| `@flag` | byte, call #0 memory, call #1 memory | a2 |
| `@lo` | byte, call #0 memory, call #1 memory | a0 |
| `@hi` | byte, call #0 memory, call #1 memory | a0, a7 |
| `@sext` | byte, call #0 memory, call #1 memory | a0, a4 |
| `@e1` | byte, call #0 memory, call #1 memory | a1, a2, a7 |
| `@pset8` | byte, call #0 memory, call #1 memory | a0, a1, a2, a3, a4, a5, a7 |
| `@inv` | byte, call #0 memory, call #1 memory | a0, a1, a2, a3, a4, a5, a7 |
| `@bit` | byte, call #0 memory, call #1 memory | a0, a1, a2, a4 |
| `@nobit` | byte, call #0 memory, call #1 memory | a0, a1, a2, a4 |
| `@spset` | byte, call #0, call #1, call #1 memory | a0, a1, a2, a3, a4, a5, a7 |
| `@word` | byte, call #1 memory | a0, a1, a2, a3, a4, a5, a7 |
| `@nocall` | byte, call #0, call #1 | a0, a1, a2, a3, a4, a5, a7 |
| `@args` | call #0, call #1 | a0, a1, a2, a3, a4, a5, a7 |
| `@bar` | byte, call #0 memory, call #1 memory | a0, a2, a3, a4, a5, a7 |
| `@no8` | byte, call #0 memory, call #1 memory | a0, a1, a2, a3, a4, a5, a7 |
| `@nospr` | byte, call #1 memory | a0, a1, a2, a3, a4, a5, a7 |
| `@wide` | byte, call #0 memory, call #1 memory | a7 |

`fight_position_gate`:

| mutant | detected by | cases |
|---|---|---|
| `@b1d` | eax | g0, g2 |
| `@b14` | eax | g1, g2 |
| `@s63` | eax | g13, g2, g3 |
| `@bit` | eax | g0, g1, g14, g3, g4, g6 |
| `@field` | eax | g0, g1, g10, g11, g12, g13, g3, g5, g6, g7 |
| `@f54` | eax | g0, g1, g13, g3, g5, g6, g8, g9 |
| `@st0` | eax | g0 |
| `@st5` | eax | g1 |
| `@st7` | eax | g13, g3 |
| `@st6` | eax | g5 |
| `@st1` | eax | g6 |
| `@st2` | eax | g7 |
| `@lt` | eax | g1, g13, g5 |
| `@signed` | eax | g9 |

`fight_side_scratch_clear`:

| mutant | detected by | cases |
|---|---|---|
| `@d2c` | byte | y0, y1 |
| `@d20` | byte | y0, y1 |
| `@d18` | byte | y0, y1 |
| `@d1c` | byte | y0, y1 |
| `@d24` | byte | y0, y1 |
| `@side` | byte | y0, y1 |
| `@wscale` | byte | y1 |
| `@n3f` | byte | y0, y1 |
| `@n41` | byte | y0, y1 |
| `@base` | byte | y0, y1 |
| `@bscale` | byte | y1 |
| `@wbyte` | byte | y0, y1 |

`fight_slot_clear`:

| mutant | detected by | cases |
|---|---|---|
| `@ee0` | byte | f0 |
| `@d50` | byte | f0 |
| `@d54` | byte | f0 |
| `@word` | byte | f0 |

`fight_413c8`:

| mutant | detected by | cases |
|---|---|---|
| `@xy` | call #0 | q0, q1, q2 |
| `@a3` | call #0 | q0, q1, q2 |
| `@late` | call #1 memory, call #2 memory, call #3 memory, call #4 memory, call #5 memory, call #6 memory, call #7 memory | q0, q1, q2 |
| `@count` | byte, call #7 | q0, q1, q2 |
| `@once` | call #5, call #6, call #7 | q0, q2 |
| `@field` | call #1, call #2, call #3, call #4, call #5, call #6, call #7 | q0, q1, q2 |
| `@or` | call #1, call #2, call #3, call #4, call #5, call #6, call #7 | q0, q1, q2 |
| `@a3b` | call #1, call #2, call #3, call #4, call #5, call #6, call #7 | q0, q1, q2 |
| `@slot` | byte, call #2 memory, call #3 memory, call #4 memory, call #5 memory, call #6 memory, call #7 memory | q0, q1, q2 |
| `@nostore` | byte, call #2 memory, call #3 memory, call #4 memory, call #5 memory, call #6 memory, call #7 memory | q0, q1, q2 |
| `@nof4` | byte, call #1 memory, call #2 memory, call #3 memory, call #4 memory, call #5 memory, call #6 memory, call #7 memory | q0, q1, q2 |

`fight_stage_marks_clear`:

| mutant | detected by | cases |
|---|---|---|
| `@n6` | byte | m0, m1 |
| `@n8` | byte | m0, m1 |
| `@base` | byte | m0, m1 |
| `@nocount` | byte | m0, m1 |
| `@cnt` | byte | m0, m1 |

`fight_stage_mark_drop`:

| mutant | detected by | cases |
|---|---|---|
| `@mask` | byte | s0, s1, s2, s3, s4, s5, s6 |
| `@shift` | byte | s0, s3, s4, s7 |
| `@n6` | byte | s1, s2, s5, s7 |
| `@n8` | byte | s0, s1, s2, s3, s5, s7 |
| `@nodec` | byte | s0, s1, s2, s3, s5, s7 |
| `@inc` | byte | s0, s1, s2, s3, s5, s7 |
| `@keep7` | byte | s0, s1, s2, s3, s5 |
| `@nowrite` | byte | s0, s1, s2, s3, s5, s7 |
| `@key7` | byte | s4, s6 |
| `@from1` | byte | s0, s1, s2, s5 |
| `@cnt` | byte | s0, s1, s2, s3, s4, s5, s6, s7 |
| `@argw` | byte | s3, s5 |

`fight_char_entry_spawn`:

| mutant | detected by | cases |
|---|---|---|
| `@zext` | call #0, call #2, call #3 | n2 |
| `@chside` | call #0, call #2, call #3 | n0, n1, n2, n3 |
| `@swap` | call #0 | n0, n1, n2, n3, n4 |
| `@wsext` | call #0 | n3 |
| `@side` | call #0 | n0, n1, n2, n3, n4 |
| `@a3` | call #0, call #1 | n0, n1, n2, n3, n4 |
| `@slota` | byte, call #1 memory, call #2 memory, call #3 memory | n0, n1, n2, n3, n4 |
| `@desc` | call #1 | n0, n1, n2, n3, n4 |
| `@actor` | call #2, call #3 | n0, n2, n4 |
| `@order` | call #2, call #3 | n0, n1, n2, n3, n4 |
| `@id` | call #2 | n0, n1, n2, n3 |
| `@psext` | call #3 | n3 |
| `@pal` | call #3 | n0, n1, n2, n3, n4 |
| `@noa` | byte, call #1 memory, call #2 memory, call #3 memory | n0, n1, n2, n3, n4 |
| `@nob` | byte, call #2, call #2 memory, call #3, call #3 memory | n0, n1, n2, n3, n4 |

## §3 Corrections

One, in its own commit.

**`0x33F08` `fight_health_bars`: the bar index is a 16-bit difference.**

- Raw: `sub ecx,edx` at 0x33F8E, `mov edx,ecx` at 0x33F90, then `test dx,dx` / `jl` at 0x33F98/0x33F9B,
  `movsx ecx,dx` at 0x33F9D, `cmp ecx,0x4b0` / `jle` at 0x33FA0/0x33FA6 and the table read
  `mov dx,[edx+ecx*2]` at 0x33FB7. Only DX of the difference is tested, and it is sign-extended before
  the bound and the index.
- Port before: `s32 s = (s32)(actorword & 0x7FFFu) - (s32)camera_char_const(ch);` (a 32-bit difference
  of two zero-extended words).
- Port after: `s32 s = (s32)(s16)(u16)((actorword & 0x7FFFu) - camera_char_const(ch));`.
- The two differ when the 32-bit difference leaves [-0x8000, 0x7FFF]. Case a7, side 0: the constant
  word 0xE061C poked to 0xFB60, the pset word 0x0010; the raw's DX is 0x04B0, so the bar's +8 becomes
  the table's word 0x4B0 (0x0B60); the old port took the difference as negative and stored 0x1E1
  (`a7: byte 0x10A608: original 0x60, port 0xE1`, measured on the uncorrected tree).
- Reach: with the image's seven constants (0x0EE4, 0x12A2, 0x0BD4, 0x16B5, 0x1F9E, 0x2F19, 0x32D7) and
  a 15-bit pset word the difference stays in [-0x32D7, 0x7FFF], where the two forms are equal. So the
  correction changes nothing unless those seven words are rewritten at run time; no search for a
  writer of them was made. No oracle was run on this branch (the brief leaves that to the integrator):
  the correction is unproven against the oracles until then.
- The mutant `fight_health_bars@wide` is the old form; a7 alone detects it.
- No unit test was added: the regression is the row's case a7 with that pin.

## §4 Named gaps, limits and unhit blocks

No named gap and no unhit block: every block of the 21 functions is hit. The limits of the rows:

- **`0x2BE00`, `0x2BE1C`: an index word >= 0x8000 is not exercised.** `and edx,0xffff` (0x2BE07; 0x2BE27,
  0x2BE35) zero-extends it, and the port reads a `u16`; but index * 0x20 >= 0x100000 leaves the image
  from any base inside it, so no case can poke the entry. A port that sign-extended the word would
  agree with every case here.
- **`0x2C320`: a count word >= 0x8000 is not exercised.** `dec edi; cmp di,-1` (0x2C340) counts the
  word down unsigned; a case would need 32768 recorded spawns.
- **`0x33F08`: the stub models `0x2A408` as leaving DS_001014EC alone.** The raw stores EDI there
  before the call (0x3400A) and reloads it after (0x34015); the port keeps the value in a local.
- **`0x413C8`, `0x43964`: the re-reads no stub disturbs.** The raw reloads DS_001080F4 on every pass
  (`mov eax,[0x1080f4]` 0x413F1), and the port does too; no stub write changes that dword, so a port
  that kept the first handle in a local would agree with every case. The raw re-reads the character
  byte before `0x2BCF4` and before `0x2A17C` (`mov eax,[edi+0x108163]` / `sar eax,0x18` at
  0x439C0/0x439C6 and 0x439DC/0x439E2); the port reads it once (0x43972). No stub writes 0x108166 or
  0x108167, so the row does not tell the two apart: they differ only if `0x2AE14` or `0x2BCF4` rewrites
  the character byte, which this row does not examine.
- **`0x413C8`, `0x43964`: one stub has one EAX per case**, so the eight (two) spawn handles of a case
  are equal. The stub writes of `D1E_SPAWN_W` and `D1E_SPAWN_A` are harness devices that make a re-read
  observable (the per-pass reload of the +0x56 word; which of the two handles `0x2BCF4`/`0x2A17C` take);
  `D1E_SPAWN_A` writes side 0's slot, so `@actor` is detected by the side-0 cases only.
- **`0x4246C`: `0x65490` runs its own bytes on the original side only** (allow-mode, with its helper
  `0x654C7`); the port fills the seven bytes with a loop. The row compares that one call (fill byte 0,
  count 7, destination 0x108106) and says nothing else about `0x65490`. Both callees are runtime
  (>= 0x5D000) and have no row, so this row is not counted as closed.
- **Poked image data** (harness values, not the game's): the 0xA769C rows (`0x1D4D0`), the 0xBBD98 /
  0xBBDA8 scene rows (`0x2C320`), the 0xC88A6 words and three words >= 0x8000 (`0x43964`), two
  character constants (`0x33F08` a7).
- **Mask-0 rows** rest on the call sites of §1; the sites marked *open* there are not settled.
- **Sides.** Every per-side row is cased for sides 0 and 1 only, except `0x1D7B8`/`0x1D838` (also 2
  and 0x100) and `0x4248C` (also 2 and 4).

## §5 The store sweep

For every field a function writes, some case starts from a different value in every written byte, the
bytes around it are seeded, and a mutant that drops or narrows the store is detected:

| row | stores | dropped / narrowed store mutant (detected) |
|---|---|---|
| `0x1D810` | DS_001028E0[side] = 0 | `@nozero`, `@width` (word only) |
| `0x1D7B8`, `0x1D838` | the slot = 0 (before the spawn), the slot = the handle | `@nozero` (only `call #1 memory`), `@store`, `@width` |
| `0x1DA08` | the decremented word, the reload, the clamp | `@nostore`, `@decb`; `@norel`, `@relb`; `@noclamp`, `@clamp` |
| `0x2C320` | DS_00105C08 = the table | `@store`, `@late` (after the loop: only the memory at the calls) |
| `0x33C18` | +0x7F, +0x80, +0x3C (dword), +0x82, +0x5B, the 0x108860 word | `@s7f`, `@s80`, `@s3c`, `@s3cw`, `@s82`, `@s5b`, `@w`, `@wbyte` |
| `0x33F08` | the bar's +8, its +0x29 bit, the bar's pset word | `@no8`, `@nobit`, `@nospr`, `@word` (dword: the next word seeded) |
| `0x38BEC` | five words, 0x40 bytes | `@d2c`, `@d20`, `@d18`, `@d1c`, `@d24`, `@wbyte`; `@n3f`, `@n41`, `@base` |
| `0x3C5CC` | three dwords | `@ee0`, `@d50`, `@d54`, `@word` |
| `0x413C8` | DS_001080F4, seven DS_001080C0 dwords | `@nof4`, `@late`; `@nostore`, `@slot`, `@count` |
| `0x4246C` | seven bytes, the count byte | `@n6`, `@n8`, `@base`; `@nocount`, `@cnt` |
| `0x4248C` | each matching byte, the count byte | `@nowrite`, `@keep7`; `@nodec`, `@cnt` |
| `0x43964` | DS_00108154[side], DS_0010815C[side] | `@noa`, `@slota`; `@nob` |

Findings of the sweep (each closed in the row before it was pinned):

1. The first marker fixtures used record addresses in the image's BSS (`0x0010A5xx`), whose bytes 0 and
   3 are zero: a zero store of only a word, or a handle store of only a word, changed the same bytes as
   the dword. The marker records and spawn handles are now values with four non-zero bytes below
   `MEM_SIZE` (never dereferenced: their consumers are stubs), and `@width` pins it.
2. `0x33F08`, the `and byte [ecx+0x29],0xbf` arm: with the byte 0x00 before, the store changes nothing
   (a0 side 0, a3, a5); the arm is observed from 0xFF in a1 and a2, the `or` arm from 0x00 in a0 and a4.
3. `0x4248C`: the count byte is written back even with no match (0x424C6), which changes no byte (s4,
   s6); the store is observed wherever a byte matches.
4. `0x1DA08`: a reload of exactly 0xC is the same with or without the clamp, so the boundary case is
   0xB (p2, +0x5A = 0x62), which `@clamp` (bound 0xB) and `@noclamp` need.
5. `0x2C320`: a first fixture with odd scenes let the `@tscale` mutant (the table pointer taken from
   scene * 2) read an unaligned dword whose value left mem[]; the scenes are now even and every decoy
   pointer is an in-image address with a small low word (it is also the count a `@nscale` mutant reads).
   No pinned mutant reads or writes outside mem[] (the first session's ten identical runs each; §7).

## §6 Queue entries not done

None: all 21 entries of `d1e-queue.txt` have a row.

## §7 What was measured, and what was not

The batch was written in two sessions; the second took over a stopped first one.

- **Rows 1-14** (`0x13134` .. `0x33F08`, commits `897b955` .. `55bd5a2`) were written, measured and
  committed by the first session. The second session re-measured each once with `--function
  <row> --self-check` semantics (the real row and every mutant, through `verify_all`): all 14
  `VERIFIED`, 150 mutants detected, and each mutant's difference kinds and catching case set equal
  to its `D1E_KINDS` / `D1E_CASES` pin. The first session's ten runs per binding are its own claim.
- **Rows 15-21** (`0x36E2C`, `0x38BEC`, `0x3C5CC`, `0x413C8`, `0x4246C`, `0x4248C`, `0x43964`) were
  written by the first session and left uncommitted; the second session measured them ten times
  each (73 mutants): every run `VERIFIED`, every mutant detected, kinds and case sets equal to the
  pins on every run, no `diffrun` failure. Their bytes were re-read from the image (the listings
  the §1 rows cite) and the call sites of §1 re-derived by a scan of the code object for `call
  rel32` to each entry; no dword in the image holds any of the seven entries.
- The §2 tables were compared with the specs and the pins by script: 21 rows (entry, cases,
  mutants, mask) and 223 mutant lines (kinds, case sets) agree.
- **Not run on this branch** (left to the integrator): the full `--self-check` (so no counter
  line was measured; by arithmetic from the closed tree's `322/322 … 1353/1353 … 229/251 … (71 have
  none)` this batch adds 21 functions, 223 mutants, 10 rows with callees of which 9 can close —
  `0x4246C` cannot, its callees are runtime — and 11 with none, which would read `343/343`,
  `1576/1576`, `238/261`, `82`: an expectation, not a measurement, and it ignores the other D1
  batches), the unittest suite, `make verify`, every `*-oracle` target and the `--check` frame run.
  The correction of §3 is therefore unproven against the oracles.
- Run on the final tree: the build, `PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests`
  (`all checks passed`), `make entry-triage` (233 unported, 262 ported; the table unchanged),
  `python3 tools/port_progress.py` (`771 1203 64` / `731 731 100`).
