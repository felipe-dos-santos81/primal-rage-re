# Track D1 batch d1i — derivation record (2026-10-11)

Differential rows (original x86 bytes against the port's C function, `tools/diff_verify.py`) for the
unrowed ported functions of queue `d1i` (`game/config.c`, `menu.c`, `nameentry.c`, `svcmenu.c`,
`platform/gfx.c`, `input.c`, `render.c`). Method: the brief `.superpowers/sdd/2026-10-11-reverse-d1/brief.md`,
record `2026-10-01-reverse-e3-derivations.md` §E3.10/§E3.12, the P-track review checklist. Bytes: the image
`./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_d1i_img.bin` (fixups applied) disassembled
with capstone 5.0.7. Every claim below is narrow in the harness's own sense: equivalence on the exercised
blocks and inputs only.

## §1 The members

`callers` = every `call`/`jmp rel32` to the entry found by scanning the code object (no pointer to any of
them is stored in the image except where said). The EAX column is what those callers read.

| addr | port function | size | callers (what they read of EAX) | EAX mask |
|---|---|---|---|---|
| `0x13F68` | `name_filter_word` | 236 | `0x13F27` (`or ah,al` at `0x13F2F`: AL); the recursive `0x1403E` (`mov eax,1` at `0x14043`) | `0xFF` |
| `0x1AE28` | `config_keys_apply` | 182 | `0x1A551`, `0x32347` (`xor eax,eax`), `0x313F0` (calls `0x2EA74`: `xor eax,eax`) | 0 |
| `0x1AE20` | `config_keys_apply_defaults` | 190 | `0x2CB50` (`mov eax,0xb4`) | 0 |
| `0x1AEE0` | `config_keys_pack` | 132 | `0x19E02` (`mov eax,0x17`), `0x20CF2` (`mov eax,esp`), `0x31153` (`xor eax,eax`), `0x31F4F`/`0x323F9` (load AX at `0x31F5D`/`0x32407`, `xor eax,eax` at `0x31F8A`/`0x32436` before any 32-bit read) | 0 |
| `0x1AF64` | `config_keys_load` | 103 | `0x20CF9` (`xor eax,eax`) | 0 |
| `0x1C3FC` | `render_list_sort` | 89 | `0x25650`, `0x2EA9A`; the raw leaves only on EAX == 0 (`0x1C420`, `0x1C449`) | full (0) |
| `0x20860` | `nameentry_key` | 132 | `0x24D67` then `jmp 0x24D08`: `mov ah,1; int 0x16`, after which only AX is read (`0x24D1B`) or EAX cleared (`0x24D19`, `0x24D26`) | 0 |
| `0x2BF00` | `config_set_credit_row_init` | 8 | `0x20CCC` (`xor eax,eax`) | 0 |
| `0x2C304` | `config_credits_init` | 25 | `0x2CBB4` (`mov eax,edx`); `0x10ECC`, whose function `0x10E80` returns it (see §4) | full (binding, §4) |
| `0x2CA2C` | `config_has_credit` | 26 | `0x2C065` (tail jump of `0x2C060`; its eight callers `test eax,eax`) | full |
| `0x2CA48` | `config_credit_take` | 45 | `0x42F8B` | full (`mov eax,1` / `xor eax,eax`) |
| `0x2CA78` | `config_credit_zero` | 3 | `0x32F56` (`mov ebx,eax; test eax,eax`), `0x42F7A` (`test eax,eax`) | full |
| `0x2CCD0` | `config_menu_default_bits` | 93 | `0x2CAFD` (`mov edx,eax`), `0x335C5` (`mov edi,eax`), `0x33687` (`mov ebx,eax`) | full |
| `0x2D2F0` | `input_joystick_device` | 3 | `0x4F66D` (`mov [0x1088dc],eax`), `0x4F67C` (`and eax,0xff`) | full |
| `0x2DB58` | `hiscore_locate` | 105 | `0x2DBD1`, `0x2DCBB`, `0x2DDFC`, `0x2DEC1` (`mov r,eax; test eax,eax`) | full |
| `0x2F464` | `text_number_cont` | 38 | `0x30A12` (`mov eax,[0xbd44c]`), `0x30D59` (`mov eax,[esp]`), `0x328E5` (`mov eax,edx` at `0x328F9`), `0x32913` (returns from `0x328B8` to `0x33052`, `ret` of `0x32F98`, to `0x331C2` `mov eax,0x209`), `0x33020` (`je 0x33052` or `mov eax,0x8b`) | 0 |
| `0x2FE40` | `menu_entry_find` | 66 | `0x30552`, `0x305B9` (`mov [0x107434],eax`), `0x2FDDD`, `0x2FE12` | full |
| `0x32A3C` | `config_play_time_close` | 194 | eight sites; see §4 | full (binding, §4) |
| `0x32B00` | `config_play_time_snap` | 73 | `0x25EE5`, `0x28937`, `0x28E3F`; see §4 | full (binding, §4) |
| `0x32B4C` | `config_play_time_snap_b` | 70 | `0x25EFD`, `0x27DEB`; see §4 | full (binding, §4) |
| `0x336C0` | `palette_list_init` | 83 | `0x2BBEF` (`mov eax,ebp` at `0x2BC19`); `0x1C0A7`, whose next call `0x5004A` can reach `0x4FC05` (`in al,dx`) and `0x50065` `test eax,eax` with the upper bits still those of this return | full (binding, §4) |
| `0x38990` | `render_scroll_track` | 52 | `0x24CC3` (the next call re-enters `0x38990`: AX loaded at `0x38991`, EAX at `0x3899F`), `0x24CC8` (then `0x24CED` `mov eax,edx` or `0x24D08` as for `0x20860`) | 0 |
| `0x4F228` | `render_projection_reset` | 36 | `0x20C49`, `0x43822` (`mov eax,1`), `0x2BBC4` (calls `0x1C350`: `mov eax,1` at `0x1C357` first) | 0 |
| `0x500C4` | `input_pump` | 130 | `0x255EE`, `0x2EB0C`; the raw returns `[0xE1C34]` (`0x5013E`) | full |
| `0x50146` | `input_repeat_set` | 27 | `0x251DA` (`mov eax,0xbcbdc`), `0x2CFF3` (`lea eax,[edx*4]`), `0x30B40`/`0x3128B` (call `0x2EA74`) | 0 |
| `0x50161` | `input_select_bits` | 38 | `0x1C84D` (`test eax,eax`), `0x2EDEA`/`0x2EED2` (`mov ecx,eax`), `0x4F64A`/`0x4F659` (stored) | full |
| `0x51F72` | `gfx_fill_screen` | 404 | the three sites of `0x52106`; see §4 | full (binding, §4) |

## §2 The rows (measured)

`python3 tools/diff_verify.py --image /tmp/pr_d1i_img.bin --function <row> --self-check`, every row
`VERIFIED`, every mutant `MISMATCH` with a real difference (no port error). `blocks` is hit/total of the
original's static scan; an unhit block is named in §4.

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `name_filter_word` | `0x13F68` | 14 | 28/28 | 15 | `0xFF` | `13F68` allowed (recursion) |
| `config_keys_apply` | `0x1AE28` | 3 | 5/5 | 11 | `0x0` | correction 1 (§3) |
| `config_keys_apply_defaults` | `0x1AE20` | 2 | 5/5 | 2 | `0x0` |  |
| `config_keys_pack` | `0x1AEE0` | 2 | 5/5 | 7 | `0x0` |  |
| `config_keys_load` | `0x1AF64` | 3 | 5/5 | 6 | `0x0` |  |
| `render_list_sort` | `0x1C3FC` | 8 | 5/5 | 6 | `0xFFFFFFFF` | `1C3A0` allow (own row) |
| `nameentry_key` | `0x20860` | 16 | 12/12 | 14 | `0x0` | `653ED` allow (runtime toupper) |
| `config_set_credit_row_init` | `0x2BF00` | 1 | 1/1 | 3 | `0x0` |  |
| `config_credits_init` | `0x2C304` | 5 | 1/1 | 5 | `0xFFFFFFFF` | `2D974` real, recorded |
| `config_has_credit` | `0x2CA2C` | 5 | 1/1 | 4 | `0xFFFFFFFF` |  |
| `config_credit_take` | `0x2CA48` | 7 | 7/7 | 6 | `0xFFFFFFFF` |  |
| `config_credit_zero` | `0x2CA78` | 2 | 1/1 | 2 | `0xFFFFFFFF` |  |
| `input_joystick_device` | `0x2D2F0` | 2 | 1/1 | 2 | `0xFFFFFFFF` |  |
| `config_menu_default_bits` | `0x2CCD0` | 8 | 9/9 | 8 | `0xFFFFFFFF` | correction 2 (§3) |
| `hiscore_locate` | `0x2DB58` | 11 | 10/10 | 8 | `0xFFFFFFFF` |  |
| `text_number_cont` | `0x2F464` | 3 | 1/1 | 6 | `0x0` | `2EFD4`/`2F198` stubs, buffer by value |
| `menu_entry_find` | `0x2FE40` | 11 | 11/11 | 8 | `0xFFFFFFFF` | `1C500` tree allowed |
| `render_projection_reset` | `0x4F228` | 2 | 1/1 | 5 | `0x0` |  |
| `render_scroll_track` | `0x38990` | 8 | 1/1 | 6 | `0x0` |  |
| `palette_list_init` | `0x336C0` | 2 | 3/3 | 8 | `0xFFFFFFFF` | `61A70` tree and `33734` allowed |
| `input_pump` | `0x500C4` | 9 | 6/6 | 10 | `0xFFFFFFFF` |  |
| `input_repeat_set` | `0x50146` | 2 | 1/1 | 5 | `0x0` |  |
| `input_select_bits` | `0x50161` | 5 | 3/3 | 4 | `0xFFFFFFFF` |  |
| `gfx_fill_screen` | `0x51F72` | 2 | 3/3 | 4 | `0xFFFFFFFF` |  |
| `config_play_time_snap` | `0x32B00` | 4 | 3/4 | 4 | `0xFFFFFFFF` | 1 block named (§4) |
| `config_play_time_snap_b` | `0x32B4C` | 4 | 3/4 | 4 | `0xFFFFFFFF` | 1 block named (§4) |
| `config_play_time_close` | `0x32A3C` | 3 | 2/12 | 3 | `0xFFFFFFFF` | `32970` allow; 10 blocks named (§4) |

27 rows, 166 mutants, all detected.

Fixtures (`d1i_*` in `tools/diff_verify.py`) live in the image: the scratch strings, records, lists and
tables in its zero BSS `0x10A000..0x10B0CF`, the globals at their own addresses. The pins are `D1I_MASKS`,
`D1I_KINDS` and `D1I_CASES` in `tools/tests/test_diff_verify.py` (what alone catches each mutant, and the
exact case set), regenerated from a measurement of the committed tree.

Callee treatment, by row:

- `name_filter_word`: its own recursion (`0x1403E`) is allowed; the port recurses in C.
- `render_list_sort`: `0x1C3A0` (`render_splice`, its own C3c row) runs real on both sides.
- `nameentry_key`: `0x653ED` (the runtime's toupper: `cmp eax,0x61; jl; cmp eax,0x7a; jg; sub eax,0x20`) runs
  real in the original; the port inlines it. Cases q10/q11 poke the class table `0x81C84` so the two bounds
  are reached with the neighbouring codes `0x7B` and `0x60`.
- `config_credits_init`: `0x2D974` (`config_field_get`, C3e row) is a `real` call, recorded on both sides
  (its argument `0x29` is compared); field `0x29` is the dword `0x105E14` (descriptor `0x2D3A4` = `0x0001D980`).
- `text_number_cont`: `0x2EFD4` and `0x2F198` are stubs with the declared clobbers `ebx ecx edx`, the buffer by
  value (as `text_number_draw`'s row).
- `menu_entry_find`: `0x1C500` and its tree (`0x474E4`, `0x1E75C`, `0x1E808`, `0x500BB`) run real on both sides
  over a one-group string table (`d1i_str_case`).
- `palette_list_init`: the runtime memset `0x61A70` (`0x65490`, `0x654C7`) runs real in the original (the port
  calls `mem_fill`); `0x33734` (`palette_record`, C3c row) runs real on both sides.
- `config_play_time_close`: the host-owned run clock `0x32970` runs real in the original (§4).

No seam was added: no row stubs a callee that lacked one, and no stub address is new to
`test_each_stub_declares_the_registers_its_callee_clobbers`.

## §3 Corrections (raw over port)

Two, each in its own commit, each with a mutant that keeps the old behaviour. Neither was run against the
capture oracles on this branch (`make verify` is the integrator's).

1. **`config_keys_apply` (`0x1AE28`) re-reads the BIOS record pointer.** The raw loads `[0x101514]` at
   `0x1AE33` and again at `0x1AE8D` (`mov esi,[0x101514]`) after the first loop's eight scan-byte stores at
   `BIOS+0x2DE..`; the port kept the first value for the second loop's stores at `BIOS+0x2E6..`. Old → new:
   `bios` cached → `bios = DSD(DS_00101514)` before the second loop. The two agree unless the first loop's
   stores land on the pointer (a BIOS record at `0x101514 - 0x2DE`), which case `k2` builds; measured before the
   change: `k2: byte 0x10151C..0x101523: original unchanged, port 0x8C..0xD2` and `byte 0x10AAE6..0x10AAED:
   original 0x8C..0xD2, port unchanged`. Mutant `config_keys_apply@reread`. `config_keys_load` (`0x1AF64`,
   `0x1AFA1`) already re-read; its case `l2` and mutant `@reread` pin that.
2. **`config_menu_default_bits` (`0x2CCD0`) bounds its entry index signed.** The index is the counter's low
   word sign-extended (`0x2CD0B` `movsx ebx,dx`), compared signed with the record's count (`0x2CD0E`
   `cmp ebx,[esi+8]`, `0x2CD11` `jge`), and it is that sign-extended index that is shifted in (`0x2CD00`);
   the entry pointer advances by 8 on its own (`0x2CD08`). The port compared an unsigned `i < count`, so a
   record whose count is negative was walked. Measured before the change: `b6: eax: original 0x2, port 0x12`,
   `b7: original 0x400, port 0x410`. Mutant `config_menu_default_bits@unsigned`. The game's own table
   `0xA2EB4` (`0x2CAF8` `mov eax,0xa2eb4`) was measured once with no pokes: original `0x142095`, port `0x142095`
   after the change. It is not kept as a case: one of its entries has a null string pointer, so the walk
   reads byte `[0]`, which the harness reports as a read outside the image. Limit: the wrap of the 16-bit index at `0x8000` entries is
   transliterated but not exercised (it needs 256 KB of entries).

## §4 Named gaps, limits and unhit blocks

**Rows whose EAX comparison is not a test of the port.** Five port functions are `void` while the raw leaves
a defined value in EAX that some caller chain was not shown to discard. Their bindings report the value the
raw leaves, with the full mask, so the comparison documents the raw and adds nothing about the port:

- `config_credits_init`: the dword it stores (`0x2C317` `mov [0x105c00],eax; ret`); the binding reads the
  stored dword back. `0x10E80` returns it, and `0x10E80` is also the hook stored at `0x104AE4` (`0x25B5F`),
  called from six sites (`0x4F302`, `0x4F373`, `0x4F6F1`, `0x4F70D`, `0x4F9AA`, `0x4F9D1`) that return in turn;
  that chain was not followed.
- `palette_list_init`: the new list head (`0x3374C` in `0x33734`); the binding reads `0x107798` back.
- `gfx_fill_screen`: `addr + 0xFA00` (`0x520F7` over `0xC8` passes); `0x5211E` reloads EAX, `0x5213D` clears
  it, `0x52156` returns `0xAFA00` from `0x52106`, whose seven callers were not followed.
- `config_play_time_snap`/`_b` on the arm != 0 path: `idx << 2` (`0x32B08`/`0x32B54`); `0x25EF8`, `0x28E44`,
  `0x27E02`/`0x27E10`/`0x27E1B` reload or clear EAX, `0x25F6B` returns it from `0x25C88` (not followed).
- `config_play_time_close` with mode & 3 == 0: `0x107488` (`0x32A2D` in `0x32970`).

**The deferred audit layer (queue mark `h`).** `tools/port_classification.txt`: `2DAE4 deferred record-§48-V
(audit no-op)`, `2E934 deferred record-§48-V (audit no-op)`, `32970 host-owned record-§48-V (host owns wall
time)`. `0x2E934` always ends in `0x2E180(counter, bucket, 1)` and `0x2DAE4` adds its EDX to an audit
counter, so in the raw those arms always change audit bytes the port, by declaration, does not write: no
state makes the two agree, and the blocks stay unexercised and named:

- `config_play_time_snap` `0x32B17` and `config_play_time_snap_b` `0x32B63` (the arm == 0 block: the
  `(acc[idx] - snapshot) / 0x3C` store and the `0x2E934` post). The port's arithmetic on that arm is
  **unverified** by this row.
- `config_play_time_close` `0x32A65`, `0x32A6A`, `0x32A7B`, `0x32A91`, `0x32AB4`, `0x32AC4`, `0x32AC8`,
  `0x32AD4`, `0x32ADE`, `0x32AF5` (every block of mode & 3 != 0, each reaching a `0x2DAE4` add). Only
  `acc[0] = 0` is verified. The run clock `0x32970` (the port omits it) runs real in the original from the one
  state where it changes no byte: last tick `0x10747C` == ISR tick `0x105D88`, mode mask `0x107494` = 0,
  `0x107484` < `0x3840` (`d1i_pt_case`); what it does with elapsed time is not compared.

**Limits of rows that are VERIFIED.**

- `render_list_sort`: the raw takes the list head pointer in EAX; both call sites pass `0x105B44`
  (`0x2564B`, `0x2EA90`) and the port hard-wires it, so every case passes that constant. The raw restarts its
  scan from the saved head after each move (`0x1C441`); the port continues from the next node. They agree on
  the eight cases; the restart itself has no separate mutant (a mutant `<=` for `<` is equivalent: an equal
  node re-splices to the same place).
- `name_filter_word`: a mutant restarting at the match start itself would not terminate and is not bound;
  `@restart` restarts two past it.
- `text_number_cont`: the two calls' buffers are compared by value (zero on both sides), so that they are
  the same buffer is not shown; the `0x2EFD4` stub cannot write a stack buffer.
- `nameentry_key`: `movsx` against `movzx` of the class byte cannot differ under `test al,0xc0`.
- `render_scroll_track`: the added word `0x107A4E` is zero-extended (`0x389B1`/`0x389B3`); a sign extension
  gives the same stored low word, and EAX is not read, so no case separates them.
- `gfx_fill_screen`: the fill target must lie in the image for the harness (`0xF1000`, `0xF0003`), not the
  aperture the game passes.

**Queue entries with no row** are in §6.

## §5 Store sweep

For each row every written field starts from a sentinel that differs from the written value and has seeded
neighbours; the mutants that drop or narrow/widen a store, and what catches them:

| mutant | caught by | on cases |
|---|---|---|
| `name_filter_word@word` | byte | n0 n1 n10 n11 n12 n13 n2 n3 n4 n5 n6 n7 n8 n9 |
| `name_filter_word@save` | byte | n10 n8 n9 |
| `name_filter_word@bxb` | byte | n10 n8 n9 |
| `name_filter_word@star` | byte | n11 n12 n13 n2 n3 n4 n5 n6 n7 |
| `config_keys_apply@mirror` | byte | k0 k1 k2 |
| `config_keys_apply@w24` | byte | k0 k1 k2 |
| `config_keys_apply@w26` | byte | k0 k1 k2 |
| `config_keys_apply@devw` | byte | k0 k1 k2 |
| `config_keys_pack@w26b` | byte | p0 p1 |
| `config_keys_pack@devw` | byte | p0 p1 |
| `config_keys_load@devw` | byte | l0 l1 l2 |
| `render_list_sort@unlink` | byte | s3 s4 s5 s6 s7 |
| `nameentry_key@wstore` | byte | q13 q7 |
| `nameentry_key@flagw` | byte | q0 q1 q10 q11 q12 q13 q14 q15 q2 q3 q4 q5 q7 q9 |
| `nameentry_key@fullflag` | byte | q0 q14 |
| `config_set_credit_row_init@width` | byte | r0 |
| `config_set_credit_row_init@addr` | byte | r0 |
| `config_credits_init@width` | byte, eax | i0 i1 i2 i3 i4 |
| `config_credit_take@decb` | byte | t4 t5 |
| `hiscore_locate@swap` | byte | l0 |
| `render_projection_reset@a3a` | byte | v0 v1 |
| `render_projection_reset@a38` | byte | v0 v1 |
| `render_projection_reset@w55` | byte | v0 v1 |
| `render_scroll_track@wide` | byte | k0 k1 k2 k3 k4 k5 k6 k7 |
| `palette_list_init@fill` | byte | p0 p1 |
| `palette_list_init@head` | byte, eax | p0 p1 |
| `palette_list_init@bd` | byte | p0 p1 |
| `palette_list_init@enq` | byte, eax | p0 p1 |
| `input_pump@prev` | byte | u2 u4 |
| `input_pump@latch` | byte | u0 u1 u2 u3 u4 u5 u6 u7 u8 |
| `input_pump@decb` | byte | u5 u6 u8 |
| `input_pump@clr` | byte | u1 u7 |
| `input_repeat_set@mask` | byte | w0 w1 |
| `input_repeat_set@count` | byte | w0 w1 |
| `input_repeat_set@wide` | byte | w0 w1 |
| `input_select_bits@latch` | byte | x1 x2 x4 |
| `gfx_fill_screen@count` | byte | g0 g1 |
| `gfx_fill_screen@row` | byte | g0 g1 |
| `config_play_time_snap@wide` | byte | a0 a1 a2 a3 |
| `config_play_time_snap_b@wide` | byte | b0 b1 b2 b3 |
| `config_play_time_close@drop` | byte | c0 c1 c2 |
| `config_play_time_close@wide` | byte | c0 c1 c2 |

No dropped store went undetected. Mutants were each run ten times through `diffrun` (identical output, exit
0): none reads outside `mem[]`. Two fixtures were changed to keep a mutant inside `mem[]`: the aliasing case
of `config_keys_apply`/`_load` carries a second valid pointer in the ASCII bytes (the `@bioslo`/`@lo` mutants
store those into the pointer; first run: `diffrun failed (-11)`), and `name_filter_word@strrep` consumes one
letter (as first written it recursed without advancing).

## §6 Queue entries not done

| addr | function | why |
|---|---|---|
| `0x2EA68` | `menu_fatal_error` | named gap: `mov eax,1; jmp 0x62003` is the runtime's fatal exit; the port's function is a declared no-op (`PORT:` comment, spec §7). A row would need `0x62003` stubbed (a stub address new to the pinned clobber table) and a port that calls it. |
| `0x2EA74` | `config_screen_wait_zero` | named gap: `xor eax,eax; mov eax,eax` with no `ret`, falling into `0x2EA78` (not a call, so it cannot be stubbed: a fall-through is not an arrival). `0x2EA78` waits on the timer ISR word (`0x2EAF2..0x2EB0A`) and drains the BIOS queue with `int 0x16`. The port is the one-line delegation `config_screen_wait(0)`. |
| `0x3157C` | `config_key_name` | not done (time). 1057 bytes: a compare tree over about 25 scan codes, each `0x1C500(id)` + an inlined copy; the raw == 0 arm formats into a stack buffer through `0x65546`, which a stub cannot write. |
| `0x319B0` | `svc_buttons_clear` | named gap: the dispatch `0x319E0` `jmp cs:[eax+0x319a0]` indexes by a pre-scaled stack local bounded by `cmp dword [esp+8],3` (`0x319D5`), a form `switch_cases` does not follow, so the row would stay `PARTIAL`; resolving it means a new `RESOLVED_JUMPS` entry in `tools/diff_emu.py` (shared, with a guard test that expects a register compare). |
