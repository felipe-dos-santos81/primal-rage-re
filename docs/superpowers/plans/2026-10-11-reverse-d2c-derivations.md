# Track D, wave 2, batch d2c: differential rows for game/flow.c, nameentry.c, attract.c, menu.c (record)

Batch `d2c` of track D wave 2 (brief `.superpowers/sdd/2026-10-11-reverse-d1/brief-wave2.md`, queue
`d2c-queue.txt`, 30 entries). 24 rows landed over 23 queue entries (`0x2C8F0` is two port functions
and two rows), 7 queue entries are named non-rows (§6), five port corrections (§3), two of them
real defects (§3.2, §3.5). Branch `reverse-d2c`, based on `reverse-d1` at `ad0c70d` (wave 1 merged).

Every number below was measured on this branch from the image
`./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_d2c_img.bin` (fixups applied) with
`python3 tools/diff_verify.py --image /tmp/pr_d2c_img.bin --function <name> --self-check`, one row at
a time. All addresses are linear (Ghidra) addresses; every instruction quoted is capstone's
disassembly of that image.

What was NOT run here: the full `--self-check` (so no branch counter line and no callee-closure
figure), the unittest suite, `make verify`, any oracle. See §7.

## §1 The members, and the callers that fix each EAX mask

Call sites are every `E8`/`E9 rel32` in the code object `0x10000..0x73B14` whose target is the entry;
none of the entries appears as a dword anywhere in the image (no pointer-table reference).

"Traced" means the wave-1 liveness script (record `2026-10-11-reverse-d1c-derivations.md` §1.1, the
same scratch script pointed at this branch's image, caller depth 6): from the instruction after the
call site, on every path, each of AL, AH and bits 16-31 of EAX is overwritten before it is read. Its
limits are those of §1.1 of that record (a script, not a proof; indirect callers of the intermediate
functions are not followed). The first kill it reports is quoted.

| addr | port function | size | callers and what they do with EAX | mask |
|---|---|---|---|---|
| 0x12658 | `game_state_3_handoff` (flow.c) | 198 | `0x12592` (in `0x12484`): `inc byte [0xf0a6f]`, pops, `ret`. Not analysed further: EAX is 0 at every return (both ways into `0x12710` come from a `test eax,eax` that found 0: `0x126DE`/`0x126E0`, `0x1270C`/`0x1270E`), so the row compares the whole register | 0xFFFFFFFF |
| 0x13EF0 | `name_filter` (nameentry.c) | 118 | `0x2071A`: `0x2071F xor ecx,ecx`, `jmp 0x20758`, `0x2075E xor eax,eax`. No caller reads EAX; the row still compares all of it (the function computes it: `0x13F58 mov eax,[esp-3]` / `0x13F5C sar eax,0x18`): stricter than the caller needs | 0xFFFFFFFF |
| 0x1D250 | `sound_pause` (flow.c) | 30 | `0x24A07`: `0x24A0C test dl,dl`, `0x24A10 mov eax,0x1ee` / `0x24A17 mov eax,0x1ef`; `0x24E26`: `0x24E2B mov eax,0x1e8` | 0 |
| 0x1D270 | `sound_resume` (flow.c) | 26 | `0x24A9C`: `call 0x1b084`, whose `0x1B092 mov eax,0xa2c5c` is the first EAX access: traced; `0x24AF9` (pops, `ret`) and `0x24E8E` (`mov [0x104b22],dh`, `jmp 0x24d08`): traced | 0 |
| 0x1FFD0 | `nameentry_cells_step` (nameentry.c) | 1314 | `0x1F464` (in `0x1F458`): `0x1F469 mov dx,[0x10438c]` ...: traced (kills `0x1F489 mov eax,edx`, `0x1F4C0 xor eax,eax`, `0x1F4F8 xor eax,eax`, `0x1F515 mov eax,[0x1044e0]`, `0x1F526 mov eax,0xe6` and eleven more) | 0 |
| 0x2716C | `flow_side_char_random` (flow.c) | 115 | `0x2705E`: `0x27068 mov eax,0x17`; `0x27700`: `0x27705 xor eax,eax`; `0x2722E`: traced to `0x42403 mov eax,[0x104abc]` | 0 |
| 0x292D4 | `flow_side_char_set` (flow.c) | 67 | `0x29871`: `0x29876 xor eax,eax` | 0 |
| 0x27254 | `flow_match_snapshot` (flow.c) | 84 | `0x2730F`: `call 0x278b0`, whose `0x278B5 xor ah,ah` and `0x278CE mov eax,0x41` write EAX before any read: traced | 0 |
| 0x278B0 | `flow_continue_open` (flow.c) | 106 | `0x27314` (pops, `ret` into `0x27380`'s caller chain): traced | 0 |
| 0x27C48 | `flow_round_winner` (flow.c) | 384 | `0x27FC8`: `0x27FCD xor eax,eax`; `0x299A6`: `0x299AB xor eax,eax`; `0x280C4` (kills `0x27DCA xor eax,eax`, `0x27EE6 mov eax,edx`), `0x280DC` and `0x299DF`: traced | 0 |
| 0x28130 | `flow_match_result_text` (flow.c) | 404 | `0x275A3`: `0x275A8 mov al,[0x10810d]`, `0x275B4 movsx eax,al`; `0x28599`, `0x288C7` (kills `0x286CA xor eax,eax`, `0x286E0 lea eax,[edx*8]`) and `0x297C9` (`0x297F4 mov eax,0x65`, `0x29810 mov eax,0x66`): traced | 0 |
| 0x286BC | `flow_match_streak_update` (flow.c) | 203 | `0x288CC`: traced (kills `0x2891D xor eax,eax`, `0x2894B xor eax,eax`, `0x289E5 lea eax,[ebx*8]`) | 0 |
| 0x2C0F4 | `prompt_press_start_blink` (flow.c) | 131 | `0x27B7C` and `0x2C1A4` (in `0x2C178`): traced | 0 |
| 0x2C1C8 | `prompt_insert_coin` (flow.c) | 12 + 0x2C1D4's body | `0x28D4B`: `0x28D50 xor eax,eax`; `0x42644`, `0x426A4`, `0x4270A`, `0x432E0`: traced | 0 |
| 0x2C2B0 | `prompt_side_erase` (flow.c) | 82 | seven sites, each followed by a load of EAX: `0x26DC9 mov eax,1`, `0x26DDB lea eax,[edx*8]`, `0x275D6`/`0x275E2`, `0x27E5C mov eax,1`, `0x27E61` -> `0x4651C mov eax,[0x1082c0]`, `0x28D0E mov eax,1`, `0x4309C mov eax,ebx` | 0 |
| 0x2C8F0 | `attract_config_volumes_unscaled` (EAX = -1) and `attract_config_volumes` (EAX = -2) (attract.c) | 199 | -1 is passed at `0x20C2B`, `0x2FA2D`, `0x308C1`, `0x43750`, `0x444E0`; `0x308C6 mov edx,eax` / `0x308C8 test eax,eax` reads all of it. -2 is passed at `0x110B0` (`0x110B5 mov eax,4`) and `0x2FA37` (traced to `0x20C1A mov eax,0x1044f4`) | 0xFFFFFFFF / 0 |
| 0x2F940 | `menu_debug_lines` (menu.c) | 89 | `0x2FEC8`: `0x2FECD mov eax,[ebp]` | 0 |
| 0x38910 | `title_origin_reset` (flow.c) | 125 | `0x121F9`: `0x12218 mov eax,0x9ae3c` / `0x12224 mov eax,0x15` | 0 |
| 0x4F790 | `frontend_skip_check` (flow.c) | 97 | six sites read AL only: `test al,al` at `0x438DE`, `0x43904`, `0x44957`; `and eax,0xff` at `0x4F25F`, `0x4F2C3`, `0x4F32B` | 0xFF |
| 0x4FA88 | `frontend_wipe_out` (flow.c) | 152 | `0x4F9C8`: `0x4F9CD test eax,eax` | 0xFFFFFFFF |
| 0x4F37C | `flow_round_timer_draw` (flow.c) | 182 | `0x25E9F`: `0x25EB2 xor eax,eax`; `0x29534`: `0x29547 xor eax,eax`; `0x25AC7`: traced | 0 |
| 0x4F0FC | `game_mode_25_exit` (flow.c) | 210 | `0x25303`: traced | 0 |
| 0x319B0 | `svc_buttons_clear` (svcmenu.c) | 91 | `0x32207`: `0x32215 xor eax,eax` / `0x3222B mov eax,1`; `0x32248`: `0x32252 mov eax,0xa` | 0 |

## §2 The rows, measured

`tools/diff_verify.py` block `D2C_SPECS` (appended to `D1_SPECS`); bindings `b_d2c_*` / `m_d2c_*` in
`port/tests/diff_runner.c`; pins `D2C_MASKS`, `D2C_KINDS`, `D2C_CASES` in
`tools/tests/test_diff_verify.py`. Every row is VERIFIED.

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `game_state_3_handoff` | 0x12658 | 4 | 6/6 | 24/24 | 0xFFFFFFFF | `e278088`; callees `0x2AE14` stub, `0x13C70` stub, `0x33904` allow, `0x1C6D4` allow; port correction §3.1; limit §4.1 |
| `name_filter` | 0x13EF0 | 9 | 11/11 | 11/11 | 0xFFFFFFFF | `9e81e53`; callee `0x13F68` allow (it calls itself); limit §4.2 |
| `sound_pause` | 0x1D250 | 4 | 3/3 | 8/8 | 0x0 | `9e81e53`; callees `0x1D1B0` stub (new seam), `0x1CD9C` stub |
| `sound_resume` | 0x1D270 | 5 | 3/3 | 7/7 | 0x0 | `9e81e53`; callee `0x1D1B0` stub |
| `nameentry_cells_step` | 0x1FFD0 | 9 | 34/34 | 56/56 | 0x0 | `6745e8e`; callees `0x2AE14`, `0x2BCF4`, `0x2C3FC`, `0x2B150`, `0x2F510` (new seam) stubs; jump table `0x1FFAC` (nine targets, all hit); limits §4.3 |
| `flow_side_char_random` | 0x2716C | 5 | 6/6 | 17/17 | 0x0 | `8312297`; callees `0x33C18` allow, `0x5D7DC` real; limit §4.4 |
| `flow_side_char_set` | 0x292D4 | 5 | 4/4 | 12/12 | 0x0 | `8312297`; callee `0x33C18` allow |
| `flow_match_snapshot` | 0x27254 | 2 | 3/3 | 12/12 | 0x0 | `8312297`; callees `0x33ACC` allow, `0x3C16C` stub, `0x3C148` stub |
| `flow_continue_open` | 0x278B0 | 2 | 1/1 | 16/16 | 0x0 | `8312297`; callees `0x1C500` allow, `0x474E4` stub, `0x2F198` stub, `0x2F434` stub (new seam) |
| `flow_round_winner` | 0x27C48 | 15 | 22/22 | 26/26 | 0x0 | `5b3fadb`; callees `0x25FDC`, `0x2604C`, `0x256F4`, `0x27BA4` allow over `0x2AE14` and `0x41310` stubs |
| `flow_match_result_text` | 0x28130 | 17 | 27/27 | 20/20 | 0x0 | `5b3fadb`; callees `0x1C500` allow, `0x474E4`, `0x2F510`, `0x2C3FC` stubs; note §4.5 |
| `flow_match_streak_update` | 0x286BC | 10 | 7/7 | 18/18 | 0x0 | `0a0b712`; callees `0x4660C` allow, `0x46534` allow; port correction §3.2 |
| `prompt_press_start_blink` | 0x2C0F4 | 8 | 8/8 | 16/16 | 0x0 | `56d1394`; callees `0x1C500`, `0x2C088` allow; `0x2AE14`, `0x474E4`, `0x2F198`, `0x2B150`, `0x2F280` stubs |
| `prompt_insert_coin` | 0x2C1C8 | 5 | 14/14 | 5/5 | 0x0 | `56d1394`; falls through into `0x2C1D4`; callees `0x1C500` allow, `0x474E4`, `0x2F198`, `0x2F388` stubs; limit §4.6 |
| `prompt_side_erase` | 0x2C2B0 | 4 | 3/3 | 12/12 | 0x0 | `56d1394`; callees `0x1C500`, `0x2C088` allow; `0x474E4`, `0x2B150`, `0x2F280`, `0x2F388` stubs |
| `attract_config_volumes_unscaled` | 0x2C8F0 | 5 | 10/18 | 10/10 | 0xFFFFFFFF | `47c4dff`; EAX = -1; callees `0x1CAB8`, `0x1CED4` allow, `0x2D974` real, the four AIL stubs; the eight blocks of the -2 arm named (§4.7) |
| `attract_config_volumes` | 0x2C8F0 | 6 | 11/18 | 14/14 | 0x0 | `47c4dff`; EAX = -2; the same callees; the seven blocks of the -1 arm named (§4.7); port correction §3.3 |
| `title_origin_reset` | 0x38910 | 3 | 1/1 | 13/13 | 0x0 | `cc9c8ba`; callee `0x4F1D0` stub (new seam) |
| `menu_debug_lines` | 0x2F940 | 2 | 1/1 | 9/9 | 0x0 | `cc9c8ba`; callees `0x2F41C` allow, `0x2F198` stub |
| `frontend_skip_check` | 0x4F790 | 8 | 9/9 | 9/9 | 0xFF | `8d6ff4e`; callee `0x4F778` stub (new seam); port correction §3.4; limit §4.8 |
| `frontend_wipe_out` | 0x4FA88 | 6 | 5/5 | 19/19 | 0xFFFFFFFF | `8d6ff4e`; callees `0x2AE14`, `0x2B150`, `0x10D70` (new seam) stubs |
| `flow_round_timer_draw` | 0x4F37C | 6 | 7/7 | 17/17 | 0x0 | `8d6ff4e`; callees `0x2F198`, `0x2F528` (new seam) stubs |
| `game_mode_25_exit` | 0x4F0FC | 2 | 1/1 | 14/14 | 0x0 | `66f0166`; callees `0x1C500` allow, `0x474E4`, `0x2F510`, `0x2F388` stubs; port correction §3.5 |
| `svc_buttons_clear` | 0x319B0 | 3 | 11/11 | 11/11 | 0x0 | `040d448`; wave-1 d1i leftover; callees `0x1C500` allow, `0x474E4`, `0x2F280` stubs; jump table `0x319A0` (four targets, all hit) |

Totals: 24 rows, 145 cases, 208/223 blocks (the fifteen unhit are the other arm of each `0x2C8F0`
row, named), 376/376 mutants detected. The table was re-measured in one pass on `040d448` (a scratch
script runs every base row and reads the mutant counts from the pin measurements).

The per-row line of a `--function` run prints every callee as "unverified": that is the
single-function run, not a finding. Whether each callee is VERIFIED by its own row, and the branch's
closed-row figure, come from the full `--self-check`, which was not run (§7).

### §2.1 The mutants

Binding name `<row>@<suffix>`. What catches each one (kind, and the exact case set) is pinned in
`D2C_KINDS` and `D2C_CASES`, generated from the measurement (a scratch script runs the row's and every
mutant's port side ten times, requires the ten outputs to be equal, then measures kinds and case sets
once against the original). The record does not repeat the case sets, so there is one copy to keep
in sync.

- `game_state_3_handoff`: `@d1` `@d2` `@d3` (each spawn's descriptor) `@a2` `@a3` `@a4` `@a5` `@a5w`
  (the spawn arguments) `@local` (the old port: the first record kept in a local, §3.1) `@w36`
  `@off36` `@cam` `@order` `@b1` `@b2` `@src` `@third` `@skip` `@known` `@al` `@six` `@handle` `@a44`
  `@a44w`
- `name_filter`: `@bit` `@tol` `@tolw` `@acc` `@first` `@stride` `@str` `@nolead` `@sp` `@nul` `@ret`
- `sound_pause`: `@gate` `@nostore` `@val` `@order` `@nostop` `@notoggle` `@early` `@byte`
- `sound_resume`: `@nz` `@bit` `@noclear` `@order` `@nocall` `@byte` `@ah`
- `nameentry_cells_step`: `@ac0` `@ac1` `@n`; state 1 `@s1tgt` `@s1x` `@s1acc` `@s1args` `@s1store`
  `@s1anim` `@s1voice` `@s1vel` `@s1st`; state 2 `@s2pre` `@s2cmp` `@s2div` `@s2zext` `@s2y` `@s2neg`
  `@s2voice`; state 3 `@s3par` `@s3tgt` `@s3acc` `@s3cmp` `@s3post`; state 4 `@s4cmp` `@s4tgt`
  `@s4off` `@s4pre`; state 5 `@s5udiv` `@s5tgt` `@s5acc` `@s5voice` `@s5x`; state 6 `@s6cmp` `@s6vel`
  `@s6st`; state 7 `@s7order` `@s7st`; state 8 `@s8sign` `@s8lt` `@s8row` `@s8ch` `@s8tmp` `@s8sp`
  `@s8seek` `@s8mode` `@s8blank` `@s8x` `@s8xy` `@s8clr` `@s8nul` `@s8order`; state 9 `@s9args`
  `@s9dead` `@s9order` `@s9st`
- `flow_side_char_random`: `@bit` `@noretry` `@noset` `@set` `@range` `@char` `@zero` `@other` `@b34`
  `@dup` `@one` `@b12` `@b12v` `@b10d` `@reset` `@rside` `@wide`
- `flow_side_char_set`: `@char` `@zero` `@other` `@b34` `@dup` `@one` `@b12` `@b12v` `@b10d` `@reset`
  `@rside` `@wide`
- `flow_match_snapshot`: `@s94` `@s68` `@dst` `@side` `@c16c` `@c148` `@order` `@zero` `@off` `@early`
  `@one` `@rec`
- `flow_continue_open`: `@b1f` `@b110` `@c04` `@id` `@row` `@mode` `@col` `@nrow` `@val` `@width`
  `@pad` `@nmode` `@b00` `@b00w` `@order` `@late`
- `flow_round_winner`: `@b13` `@b1c` `@signed` `@al78` `@bit` `@f41side` `@b5b` `@b16` `@af` `@s7f`
  `@inc2` `@b1d3` `@b1d2` `@zero` `@thr` `@tsign` `@toff` `@b1cw` `@b1cv` `@bonusb` `@side` `@tie`
  `@nomark` `@nores` `@order` `@early`
- `flow_match_result_text`: `@b16` `@t9c4` `@id43` `@row` `@novoice` `@voiceid` `@above` `@tsign`
  `@thr` `@f63` `@f63side` `@cside` `@rows` `@tbl` `@mode` `@id42` `@r2voice` `@r3` `@order` `@col`
- `flow_match_streak_update`: `@draw` `@stagew` `@f63` `@inc` `@incorder` `@arg` `@zero2` `@b11z`
  `@s80` `@s80side` `@flag80` `@flag6` `@flagch` `@spur` (the old port: the spurious store, §3.2)
  `@lf63` `@lside` `@delta` `@nocall`
- `prompt_press_start_blink`: `@phase` `@p18` `@sprite` `@desc` `@a2a4` `@a3` `@a5` `@slot`
  `@nostore` `@id` `@mode` `@len` `@nolen` `@lenid` `@clrargs` `@clrside`
- `prompt_insert_coin`: `@side` `@sext` `@col` `@word` `@row`
- `prompt_side_erase`: `@bit` `@noclear` `@clrside` `@norel1` `@swap` `@sext` `@cnt` `@col2` `@row2`
  `@order` `@one` `@colsext`
- `attract_config_volumes_unscaled`: `@m8` `@s10` `@shr` `@noshift` `@f35` `@f37` `@swap` `@ret`
  `@rethalf` `@order`
- `attract_config_volumes`: `@m8` `@s10` `@shr` `@noshift` `@f35` `@f37` `@swap` `@order` `@mask`
  `@f2a` `@udiv` `@div` `@sraw` `@early` (the old port: field 0x37 read before the music call, §3.3)
- `title_origin_reset`: `@nocall` `@a4a` `@a4c` `@src` `@tbl` `@idx` `@a50` `@a40` `@sh5` `@sh6`
  `@sext` `@a48` `@order`
- `menu_debug_lines`: `@col` `@row2` `@os` `@mode` `@buf` `@off` `@deref` `@order` `@mode2`
- `frontend_skip_check`: `@any` `@one` `@ret2` `@inl` `@n` `@nocall` `@al` (the old port: the whole
  EAX tested, §3.4) `@r1` `@held`
- `frontend_wipe_out`: `@gate` `@desc` `@a3` `@nostore` `@copy` `@copysrc` `@f4` `@f4all` `@gt`
  `@unsigned` `@nodead` `@keep` `@ret1` `@tbl` `@zext` `@inc` `@args` `@order` `@ret0`
- `flow_round_timer_draw`: `@b1d2` `@f2a` `@f2b` `@xxf2` `@b14` `@col` `@row` `@mode` `@val` `@width`
  `@pad` `@nmode` `@f2c` `@aec` `@aecv` `@order` `@forder`
- `game_mode_25_exit`: `@id67` `@rowb` `@mode` `@col` `@rel` (the old port: col -1, count 0xE, §3.5)
  `@relrow` `@rel4` `@b00` `@b00w` `@c0` `@b15` `@aec` `@aecv` `@order`
- `svc_buttons_clear`: `@c0col` `@c1col` `@c2row` `@c3row` `@id` `@mode` `@n3` `@order` `@off8`
  `@row8` `@regs`

The mem[] bound: every index-like value a binding or mutant follows is a side (0/1), a cell index
below 18, a character below 7 or 0x80 + a table inside the data object, or a record pointer the case
seeds inside the image's BSS (`0x10A2xx..0x10A6xx`); `@stride`/`@first` of `name_filter` walk the
image's own pointer table, whose empty word (index 24, even) both reach. Two cases of
`flow_side_char_random` were changed while measuring because a mutant did not return on them
(`@bit` on a taken-byte set with 0x10 everywhere, `@range` with only character 6 free): a mutant
that loops for ever is a port error, not a detection.

### §2.2 Hand resolutions and pins added outside the D2C block

- `tools/diff_emu.py` `RESOLVED_JUMPS[0x2002E] = (0x20017, 0x1FFAC, 9)`: `cmp al,8; ja 0x204cd; mov
  bx,[0x1044c4]; and eax,0xff; shl ebx,9; jmp cs:[eax*4+0x1ffac]` (`switch_cases` refuses the `shl`
  of another register between the guard and the jump). The table holds `0x20036 0x200C9 0x20146
  0x201EB 0x2025B 0x202E8 0x20352 0x2036A 0x20487` and ends at the entry `0x1FFD0`.
- `tools/diff_emu.py` `RESOLVED_JUMPS[0x319E0] = (0x319D5, 0x319A0, 4)`: `cmp dword [esp+8],3; ja
  0x31a44; mov eax,[esp+4]; jmp cs:[eax+0x319a0]`. The jump's index is not the compared counter but
  the pre-scaled local `[esp+4]`; it is four times the counter because both start 0 (`0x319BE mov
  [esp+8],edx`, `0x319CB mov [esp+4],edx`, EDX = 0 from `0x319B9`) and step together (`0x31A4C add
  edx,4` / `0x31A4F inc ebx`, stored at `0x31A50`/`0x31A54`), and nothing else writes either. The
  table holds `0x319E7 0x319FF 0x31A13 0x31A2A` and ends at the entry `0x319B0`. This is the gap the
  wave-1 record `2026-10-11-reverse-d1i-derivations.md` §6 names for `0x319B0`; the existing guard
  test accepts the memory operand as it is (it reads the compare's immediate).
- Both windows are pinned in the test's `WINDOWS` dictionary (two ADDED entries, no existing line
  edited) and equal the image's disassembly; checked by running the test's own assertions for the
  two entries in a scratch script (the guard's immediate + 1 = the count, the table address in the
  jump, every target in the image, the jump still `indirect` for `switch_cases`).
- The stub-clobber pin of `test_each_stub_declares_the_registers_its_callee_clobbers` gained TWO
  ADDED LINES (no existing line edited), immediately above the dictionary's last line:
  `0x1D1B0: (), 0x2F510: ("ebx", "ecx", "edx"), 0x2F434: ("ebx", "ecx", "edx"), 0x4F1D0: (),     # track D2 batch d2c`
  and
  `0x4F778: (), 0x10D70: ("ebx", "ecx", "edx"), 0x2F528: ("ebx", "ecx", "edx"),     # track D2 batch d2c`.
  Checked without running the suite, at every commit: the pinned dictionary equals the stubs of
  `V.SPECS`, and `E.callee_clobbers` on the image returns the declared set for every stub address
  the D2C rows use.
- No `_CALLEE_CLOBBER_FIXES` entry was added. One would be needed to STUB `0x1CAB8`: the scan
  returns `ebx ecx edx esi edi` for it because it takes the `add esp,0xc` at `0x1CB0E` (the cleanup
  of the `0x5DECA` call) into the restore run, so the `je 0x1cb11` targets look like jumps into the
  run; the bytes push the five at `0x1CAB8..0x1CABC` and pop them at `0x1CB11..0x1CB15` on every
  path. The `0x2C8F0` rows run `0x1CAB8` real instead, so the question does not arise there.

## §3 Port changes

Five corrections, each in its own commit. §3.2 and §3.5 change what the game does; §3.1, §3.3 and
§3.4 are fidelity fixes that only a stub can observe. `frames/` was regenerated (`--check 820`) and
`run_tests` passed after each; NO oracle was run against any of them (the brief leaves that to the
integrator), so all five are unproven against the oracles.

### §3.1 `game_state_3_handoff` re-reads the camera dword (commit `130ad07`, fidelity)

`port/src/game/flow.c`, 0x12658. Old: the first spawned record stayed in the C local `first` for the
`+0x4B` store and the third spawn's a5. New: `u32 cam = DSD(DS_000F0A58)` after the second spawn, and
both use `cam`. The proving instructions:

```
1267D  mov dword ptr [0xf0a58], eax      ; first
1269D  call 0x2ae14                      ; second
126A2  mov esi, eax
126A4  mov eax, dword ptr [0xf0a58]      ; the reload
126A9  mov bl, byte ptr [esi + 0x56]
126AC  mov byte ptr [eax + 0x4b], bl
126AF  mov ax, word ptr [eax + 0x56]
```

Found by the row's spawn stub, which leaves another record in `0xF0A58` at each call so that the one
EAX a stub has per case does not make the three spawned records one: the old port stored `+0x4B` in,
and took a5 from, the stub's EAX record (`h0: call #2: original 0x2AE14(0x9AC6C, 0x0, 0xE2, 0x0,
0x7F45), port ...(..., 0x8523)`, `byte 0x10A44B: original 0x23, port unchanged`). The mutant
`game_state_3_handoff@local` reproduces the old behaviour and is detected on all four cases. It
cannot change the game: `0x2AE14` does not write `0xF0A58`, so the reload returns the record just
stored.

### §3.2 `flow_match_streak_update` stores nothing in the loser's +0x80 (commit `cd41ba6`, DEFECT)

`port/src/game/flow.c`, 0x286BC. Old: after the stage byte, `DSB(DS_00107830 + loser * 0x94u) =
(u8)(flag ^ (u8)loser)`, commented `0x2875B..0x2876F`. New: the line is gone (and the header comment
corrected). The raw has no such store:

```
28755  mov byte ptr [edx + 0x108106], al     ; the stage byte
2875B  lea eax, [ebx*8]                      ; ebx = loser
28762  add eax, ebx
28764  shl eax, 2
28767  add eax, ebx                          ; loser * 37
28769  mov al, byte ptr [eax*4 + 0x107813]   ; the loser's +0x63 (37 * 4 = 0x94)
28770  and eax, 0xff
28775  cmp eax, 1
28778  jne 0x28783
```

The bytes `0x28755..0x28785` were also read as hex (`88 82 06 81 10 00 8d 04 dd 00 00 00 00 01 d8 c1
e0 02 01 d8 8a 04 85 13 78 10 00 25 ff 00 00 00 83 f8 01 75 09 ...`): no store between `0x28755` and
the `call 0x46534`. Ghidra's decompilation (`port/decomp/prage.c`, `FUN_000286bc`) has none either.
Found by the row on six cases (`e0: byte 0x1078C4: original unchanged, port 0x82`, `e1: byte
0x107830: original unchanged, port 0xC2`, ...). The mutant `flow_match_streak_update@spur`
reproduces the old store and is detected on six cases. In the game the old port left `flag ^ loser`
(0x80 or more) in the loser's slot `+0x80` after every match that did not extend a streak; the same
byte is what `0x33C18` clears and `0x28788`'s tail counts in.

### §3.3 `attract_config_volumes` reads field 0x37 after the music call (commit `449897d`, fidelity)

`port/src/game/attract.c`, 0x2C8F0 with EAX = -2. Old: fields 0x2A, 0x35 and 0x37 read up front,
then the two volume calls. New: field 0x37 is read between them. The proving instructions:
`0x2C97C call 0x1cab8`, `0x2C981 mov eax,0x37`, `0x2C986 call 0x2d974`. Found by the row's recorded
call list (`v0: call #2: original 0x5DEED(), port 0x2D974(0x37)`). The mutant
`attract_config_volumes@early` reproduces the old order. It cannot change the game: `0x2D974` only
reads, and `0x1CAB8` does not write the configuration bytes. (The -1 function already had the raw's
order.)

### §3.4 `frontend_skip_check` tests AL of `0x4F778` (commit `fac61d5`, fidelity)

`port/src/game/flow.c`, 0x4F790. Old: `frontend_buttons_pressed(1u) != 0u` (the whole u32). New:
`(u8)frontend_buttons_pressed(1u) != 0u`. The proving instructions: `0x4F7D5 call 0x4f778`,
`0x4F7DA test al,al`, `0x4F7DC je 0x4f7e0`. Found by case `k5` (stub EAX `0x100`): `eax (mask 0xFF):
original 0x0, port 0x1`. The mutant `frontend_skip_check@al` reproduces the old test. It cannot
change the game (`0x4F778` returns 0 or 1); same class as the d1c record's §3.1.

### §3.5 `game_mode_25_exit` releases 0xB cells from col 0xE (commit `243dfc8`, DEFECT)

`port/src/game/flow.c`, 0x4F0FC. Old: five `text_cells_release_count(-1, row, 0xE)`. New: five
`text_cells_release_count(0xE, row, 0xB)`, rows 6..0xA. `0x2F388` takes EAX = col, EDX = row, EBX =
count (its seam, d1c record §3.2); the raw loads:

```
4F13F  mov ebx, 0xb
4F144  mov edx, 6
4F149  mov eax, 0xe
4F14E  call 0x2f388          ; and four more, edx = 7, 8, 9, 0xa, to 0x4F19E
```

Found by the row (`x0: call #4: original 0x2F388(0xE, 0x6, 0xB), port 0x2F388(0xFFFFFFFF, 0x6,
0xE)`). The mutant `game_mode_25_exit@rel` reproduces the old arguments. In the game the old port
released the same first column (a centred run of 0xE cells starts at `(0x2b - 0xe) >> 1` = 0xE) but
three more cells per row (cols 25..27 of rows 6..10).

The other 17 raw call sites of `0x2F388` were then compared with the port's calls by their register
loads (a scratch scan of the instructions before each `call`): the port's arguments agree at the
thirteen flow.c sites it has; the three svcmenu.c sites (`0x30986`, `0x30CD3`, `0x31511`) were
compared only as far as those loads show (the constants agree; the computed row and col were not
traced). One raw site has NO counterpart in the port and is outside this batch's queue, so it
was NOT changed: `0x4F01A mov ebx,2` / `0x4F01F mov edx,0xb` / `0x4F024 mov eax,0xffffffff` /
`0x4F029 call 0x2f388`, in `0x4EF8C` (`game_mode_25_reveal`, flow.c), between the node walk's exit
(`0x4F018 jne 0x4efa8`) and the two `(-1, 5, strlen)` releases the port does have. `0x4EF8C` has no
differential row and is in no wave-2 queue. Reported to the integrator; unverified beyond the bytes
quoted.

### §3.6 Seams and exports (no behaviour change)

| address | function | change |
|---|---|---|
| 0x12658 | `game_state_3_handoff` (flow.c) | `static` dropped, declared in flow.h (for the row's binding) |
| 0x286BC | `flow_match_streak_update` (flow.c) | `static` dropped, declared in flow.h |
| 0x38910 | `title_origin_reset` (flow.c) | `static` dropped, declared in flow.h |
| 0x1D1B0 | `sound_music_pause_toggle` (flow.c) | `PR_SEAM0(0x1D1B0u)`; `E.Call(0x1D1B0, ())`, clobbers none |
| 0x4F1D0 | `frontend_origin_zero` (flow.c) | `PR_SEAM0(0x4F1D0u)`; `E.Call(0x4F1D0, ())`, clobbers none (it pushes and pops EDX) |
| 0x4F778 | `frontend_buttons_pressed` (flow.c) | `PR_SEAM_RET(0x4F778u, n)`; `E.Call` `("eax",)`, clobbers none |
| 0x2F510 | `text_cursor_hold_font2` (actors.c) | `PR_SEAM(0x2F510u, col, row, text_str_dw0(s), mode)`; `E.Call` `("eax","edx","[ebx]","ecx")`, clobbers `ebx ecx edx`. `text_str_dw0` (new static helper, `/* PORT: */`) is the string's first four bytes: the bytes at its mem[] offset when it lies in mem[], else the bytes up to its NUL, zero after it |
| 0x2F434 | `text_number_set` (actors.c) | `PR_SEAM(0x2F434u, col, row, value, width, pad, mode)`; `E.Call` `("eax","edx","ebx","ecx","s0","s1")`, `pop=8` (`0x2F460 ret 8`), clobbers `ebx ecx edx` |
| 0x2F528 | `text_number_draw_font2` (actors.c) | `PR_SEAM(0x2F528u, col, row, value, width, pad, mode)`; the same `E.Call` shape, `pop=8`, clobbers `ebx ecx edx` |
| 0x10D70 | `actor_pset_word_set` (actors.c) | `PR_SEAM(0x10D70u, rec, word)`; `E.Call` `("s0","eax")` (the C order: rec is the stack argument), `pop=4`, clobbers `ebx ecx edx` |

Each seamed function's own row was re-run after its seam and is unchanged: `sound_music_pause_toggle`
(11/11 mutants), `frontend_origin_zero` (4/4), `frontend_buttons_pressed` (5/5),
`text_cursor_hold_font2` (5/5), `text_number_set` (7/7), `text_number_draw_font2` (7/7),
`actor_pset_word_set` (8/8). No other existing spec names any of the seven addresses (`grep` over
`tools/diff_verify.py`), and the hook returns 0 for an address outside the running case's call set.

The `0x2F510` seam compares only the first four bytes of the string. The rest of a string that lies
in mem[] is compared as memory (the bytes at each call and at return); which pointer was passed is
seen only through those four bytes.

## §4 Named gaps and limits

1. **`game_state_3_handoff`: the second and third records are one.** A stub has one EAX per case, so
   the three spawns return the same record; the camera-dword trick (§3.1) separates the first from
   the other two, not the second from the third (`0x126CF mov al,[eax+0x56]` / `0x126D2 mov
   [esi+0x4b],al`: source and destination are the same record in every case).
2. **`name_filter`: the sign extension.** With the real `0x13F68` (which returns 0 or 1: `0x13F91 xor
   eax,eax`, `0x14043 mov eax,1`) the accumulated byte is 0 or 1, so `0x13F5C sar eax,0x18` is not
   separated from a zero extension. No call argument is recorded (allow mode): the walk of the table
   is seen through its effects (the `*` runs, `0xFD0F2..0xFD0FB`).
3. **`nameentry_cells_step`.** (a) The letter arm's string is the two-byte stack buffer at
   `[esp+4]` (`0x203A5 lea ebx,[esp+4]`); the `0x2F510` seam's dword is its two bytes and the two
   above them, which are the emulator's zero stack on the raw side and the port's zero padding after
   the NUL (the byte at `0x1E821`, 0 in the image). A raw whose `[esp+6]` held anything else would
   differ there with no meaning. (b) `0x2023D cmp edx,0x12` (the top byte of the dword `0x10431A`)
   sets flags nothing reads; the port has no counterpart and the row cannot see one. (c) One spawn
   EAX per case: every spawned handle of a case is the same record.
4. **`flow_side_char_random`: AL and EAX of the draw.** The raw indexes with `mov dl,al` over a zeroed
   EDX (`0x2717B`, `0x27182`) and stores AL (`0x2719C`); the port indexes with the whole return of
   `rng_next(7)`. The row runs `0x5D7DC` real (so the retry loop is exercised: seed 0x23 draws 3, 3,
   5), and a real draw is below 7, so the two are not separated. A stubbed draw of 0x105 would
   separate them and would also make any retry loop for ever (one EAX per case). Not changed.
5. **`flow_match_result_text`: `0x2815E jne 0x282c0` is never taken** (EDX is 0 there: `0x28144 cmp
   edx,1` / `0x28147 jb`). Its target block is hit as the fall-through of `0x282BB`, so the block
   count is whole; the edge has no case and cannot have one.
6. **`prompt_insert_coin`** inherits the 0x14-byte stack buffer limit of `0x2C1D4` (d1c record §4.4):
   the two drawing cases build 15 and 19 characters.
7. **`0x2C8F0` is split.** The port has one function per value of EAX the seven call sites pass, so
   each row names the other arm's blocks (`0x2C939 0x2C942 0x2C960 0x2C967 0x2C97C 0x2C990 0x2C997
   0x2C9AC` for the -1 row; `0x2C8F8 0x2C909 0x2C910 0x2C912 0x2C926 0x2C92D 0x2C92F` for the -2
   row); together the two rows hit 18/18. An EAX that is neither returns at once (`0x2C93C jne
   0x2c9b3`): no port function, no case. In the -2 arm the raw leaves EDX (the last scaled value, or
   the caller's EDX when both fields read -1) in EAX; no -2 caller reads it (§1) and the port
   function is void.
8. **`frontend_skip_check`: the two stores to `0x1088D8`** (`0x4F7C1`, `0x4F7E6`) write back the value
   read at `0x4F794`; they change no byte in any case and cannot (the sweep reports both).

## §5 Store sweep (checklist items 1-2)

A scratch script hooks every memory write of the original for every case of a row (private stack
excluded; the writes of callees that run real are included) and reports, per store instruction, how
often it ran, how often it changed a byte, and for byte and word stores whether the byte after and
the byte before were non-zero in some case.

- **Item 1.** Every store instruction of the 24 rows' own code changes a byte in at least one case,
  with the one exception of §4.8 (`frontend_skip_check`'s two write-backs). Store instructions per
  row (own code): `game_state_3_handoff` 5, `name_filter` 2, `sound_pause` 1, `sound_resume` 1,
  `nameentry_cells_step` 46, `flow_side_char_random` 6, `flow_side_char_set` 5,
  `flow_match_snapshot` 1, `flow_continue_open` 4, `flow_round_winner` 13, `flow_match_streak_update`
  6, `prompt_press_start_blink` 2, `prompt_insert_coin` 3 (in `0x2C1D4`), `title_origin_reset` 6,
  `frontend_wipe_out` 5, `flow_round_timer_draw` 4, `game_mode_25_exit` 4; the other rows store
  nothing themselves.
- **Dropped stores.** Each storing row carries dropping mutants (`@cam` `@b1` `@b2` `@a44`; `@nul`;
  `@nostore`; `@noclear`; `@s1acc` `@s1vel` `@s1st` `@s1store` `@s5tgt` `@s5acc` `@s5x` `@s6vel`
  `@s6st` `@s8ch` `@s8nul` `@s8clr` `@s9st`; `@noset` `@char` `@zero` `@b12`; `@zero`; `@b1f` `@c04`;
  `@b13` `@b1c` `@b5b` `@s7f` `@b1cw`; `@inc` `@zero2` `@b11z` `@s80`; `@nostore` `@nolen`; `@a4a`
  `@a4c` `@a50` `@a40`; `@nostore` `@copy` `@f4` `@keep` `@inc`; `@f2b` `@f2c` `@aec`; `@c0` `@b15`
  `@aec`), all detected. Not every store has its own dropping mutant (`nameentry_cells_step`: 46
  stores, 13 dropping mutants); the others are covered by value or order mutants or by the row's
  byte comparison alone.
- **Item 2, neighbours.** The byte after each byte or word store is non-zero in some case, except
  in `nameentry_cells_step` where the neighbour is a field the function itself has just written
  (`0x2005C`, `0x201B1`, `0x202BE`: the acceleration word, whose next byte is the low byte of the
  target word stored a few instructions earlier, 0 by value). The byte BEFORE is zero in every case
  for `0xFD0F0` (`name_filter`), `0x1028D8` (`sound_pause`, `sound_resume`) and for the velocity and
  target words of `nameentry_cells_step` (the high byte of the y dword below them, 0 for every y a
  case uses): a store one byte too low would still be caught by the byte it leaves unwritten above.
  `flow_round_winner`'s first measurement had three such zeros (`0x104AF3` after `0x104AF2`,
  `0x104B1D` after `0x104B1C`, `0x10789E` before `0x10789F`); four cases were changed to seed them
  and the row re-measured.
- **Not checked:** the port side's writes beyond the row's byte comparison.

## §6 Queue entries not rowed

| addr | size | port | why |
|---|---|---|---|
| 0x28DA4 | 244 | `flow_player_join` (flow.c) | Not comparable. It is one block, and on it the raw calls two functions the port leaves out under `/* PORT: */` comments: `0x28DAC call 0x32970` (the run clock, host-owned) and `0x28E53 call 0x2dae4` (the audit add, deferred). Neither has a C function to seam, so the recorded call lists would differ by design on every case; allowing them instead runs their bytes on the raw side only. Rowing it needs a seamed host-side function for each (the pattern AGENTS.md names for `0x1BBAC`), which is a port change beyond a seam. The rest of the function was read against the bytes and agrees (the stores, the `xor si,1` other-side store at `0x28E29`, the hook `0x28D80`); that is a reading, not a measurement. |
| 0x418F4 | 783 | `flow.c` (state 5's scoreboard build) | Not attempted: the batch stopped before it. Its callees are all rowed or seamed now (`0x1C500`, `0x2A17C`, `0x2AE14`, `0x2F174`, `0x2F198`, `0x2F434`, the last seamed by this batch). |
| 0x1CF40 | 215 | `game_audio_init` | LEFTOVER d1c: stays the gap that record's §6 names (host-object handles, port-only work). Not re-examined beyond reading that entry. |
| 0x1D018 | 161 | a comment inside `game_shutdown` | LEFTOVER d1c: stays the gap that record's §6 names (no C function to bind). |
| 0x2EA68 | 10 | `menu_fatal_error` (menu.c) | LEFTOVER d1i: stays the gap that record names (`mov eax,1; jmp 0x62003`, the runtime's fatal exit; the port's function is a declared no-op). |
| 0x47370 | 369 | the localisation loader | LEFTOVER d1d: stays the gap that record names (DOS file I/O and the host-owned pager; `run_original` stops `unmodeled` at `call 0x61C60`). |
| 0x4FBA2 | 25 | `int10h_query` | LEFTOVER d1d: stays the gap that record names (`int 0x10`, `unmodeled`). |

The sixth leftover, `0x319B0`, IS rowed (§2.2): the d1i record's reason was the jump-table form, and
the bytes bound it.

## §7 What was run, and what was not

Run on `040d448` (the last row commit; this record is the only later change): the build; every
row's `--function <name> --self-check` at its own commit, and all 24 base rows again in one pass on
`040d448` (all VERIFIED); the pin measurement of every row (ten port-side runs each, equal); the
static checks of §2.2; and

```
$ cmake --build build 2>&1 | tail -1
[100%] Built target diffrun
$ PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -3
test_frontend: PR_FRONTEND_DUMP unset, state-2 driver skipped
test_title: PR_TITLE_DUMP unset, title dump driver skipped
all checks passed
$ make entry-triage E2_IMAGE=/tmp/pr_d2c_e2.bin 2>&1 | tail -2
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
$ python3 tools/port_progress.py
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

`frames/` was regenerated with `./build/prageport --game-dir data/game/C --check 820` after each of
the five corrections (last line each time: `fn-miss check distinct=2 dropped=0`), before `run_tests`.

Not run (the integrator's): the full `--self-check` (no counter line for this branch), the unittest
suite (so the `D2C_*` pins, the two `WINDOWS` entries and the two clobber-dictionary lines were
checked only by the scratch scripts described above, not by the tests that own them), `make
verify`, `make diff-verify`, every `*-oracle`. The counter test is expected to fail on this branch
until re-pinned. The mask-0 claims marked "traced" rest on the liveness script and its limits (§1).
