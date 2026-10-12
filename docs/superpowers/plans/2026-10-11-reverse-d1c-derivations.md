# Track D1 batch d1c: differential rows for game/flow.c's first half (record)

Batch `d1c` of track D1 (brief `.superpowers/sdd/2026-10-11-reverse-d1/brief.md`, queue
`d1c-queue.txt`, 25 entries). 23 rows landed, 2 queue entries are named non-rows (§6), one port
correction (§3). Branch `reverse-d1c`, based on `reverse-d1` at `b4d0ad4` (not rebased: `reverse-d1`
has since taken the a, b, d, g, h and i merges).

The rows were written by a first agent that was stopped before this record; every number below was
re-measured by the agent that wrote it, on the committed tree (`d3075f2`), from the image
`./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_d1c_img.bin` (fixups applied) with
`python3 tools/diff_verify.py --image /tmp/pr_d1c_img.bin --function <name> --self-check`, one row at
a time. All addresses are linear (Ghidra) addresses; every instruction quoted is capstone's
disassembly of that image.

What was NOT run here: the full `--self-check` (so no branch counter line and no callee-closure
figure), the unittest suite, `make verify`, any oracle, `--check 820`. See §7.

## §1 The members, and the callers that fix each EAX mask

Call sites are every `E8 rel32` in the code object `0x10000..0x73B14` whose target is the entry; none
of the 23 entries appears as a dword anywhere in the image (no pointer-table reference).

For the mask-0 rows, "EAX dead" means: starting at the instruction after the call, on every path,
each of the three parts of EAX (AL, AH, bits 16-31) is overwritten before it is read. Where the
overwrite is in the caller's straight line the instruction is quoted. Where the caller returns or
calls on with EAX untouched, the claim comes from an ad-hoc liveness trace (§1.1), and the table says
"traced".

| addr | port function | size | callers and what they do with EAX | mask |
|---|---|---|---|---|
| 0x134C0 | `effects_step` | 1564 | `0x25639` (in `0x255CC`): `0x2563E mov eax,[0x10150c]` | 0 |
| 0x1C6D4 | `frontend_resource_known` | 94 | `0x12038`, `0x126EC`, `0x42594`: `test al,al` at `0x1203D`, `0x126F1`, `0x42599` | 0xFF |
| 0x1CAB8 | `sound_music_volume` | 95 | `0x2C912`: `0x2C917 mov eax,0x37`; `0x2C97C`: `0x2C981 mov eax,0x37`; `0x30DF9`: `0x30DFE mov eax,edx`; `0x30DB1`: `jmp 0x30E07`, then `0x30E19 mov eax,0x22`, `0x30E3D mov eax,3` or `0x30B45 call 0x2ea74` (`0x2EA74 xor eax,eax`) | 0 |
| 0x1CED4 | `sound_sfx_volume` | 76 | `0x2C92F` (`jmp 0x2C9B1`) and `0x2C9AC`: `0x2C9B1 mov eax,edx`; `0x30E02`: the same three exits as `0x30DB1` above | 0 |
| 0x1D0BC | `sound_buffers_alloc` | 243 | `0x1C0B1` (in `0x1BEC4`): `0x1C0B6 xor eax,eax`. No caller reads EAX; the row still compares AL, which the function sets on every exit (`mov al,dl` / `xor al,al`): stricter than the caller needs | 0xFF |
| 0x1D1B0 | `sound_music_pause_toggle` | 111 | `0x1D25C` (in `0x1D250`): `0x1D261 mov [0x1028d8],dl`, `call 0x1cd9c`, `pop edx`, `ret`: traced; `0x1D27C` (in `0x1D270`): `0x1D281 xor ah,ah`, a byte store, `ret`: traced; `0x24DBF` (in `0x24C5C`): `jmp 0x24D08` (`mov ah,1`, `int 16h`): traced | 0 |
| 0x1D220 | `sound_sample_pause_toggle` | 24 | `0x24DC9` (in `0x24C5C`): `jmp 0x24D08`: traced. The function ends in the tail jump `0x1D231 je 0x1cd9c` | 0 |
| 0x1E988 | `hiscore_audit_reset_due` | 40 | `0x1E87F`: `0x1E884 and eax,0xff`, `cmp eax,1` | 0xFF |
| 0x256F4 | `flow_win_markers_spawn` | 173 | `0x25DA6`: `0x25DAB mov ah,4` ... `0x25DB5 mov eax,0x3c`; `0x27D02`, `0x27D80`, `0x27DA5`, `0x27DBA` (in `0x27C48`): `call 0x27ba4`, pops, `ret`: traced; `0x29446` (in `0x29328`): `mov al,4`, `mov ah,2` ...: traced | 0 |
| 0x25848 | `flow_stage_pick` | 386 | `0x4177D`: `0x41784 mov eax,[0x104ad4]`; `0x418A8`: `0x418B4` same; `0x42390`: `0x42397` same; `0x430FE`: `0x4312F` / `0x43149 xor eax,eax` | 0 |
| 0x25FDC | `flow_round_bonus_a` | 111 | `0x27CC7`: `0x27CCC mov eax,[0x1088ef]`; `0x27D67`: `0x27D6C` same | 0 |
| 0x2604C | `flow_round_bonus_b` | 111 | `0x2613E` (in the non-Ghidra entry `0x260BC`): `0x2614F mov eax,[0x104ab4]`; `0x27CFD`, `0x27DA0` (in `0x27C48`): `call 0x256f4`, `call 0x27ba4`, pops, `ret`: traced | 0 |
| 0x27BA4 | `flow_match_result_set` | 163 | `0x27D07`, `0x27D85`, `0x27DAA`, `0x27DBF` (in `0x27C48`), each followed by `pop edx; pop ecx; pop ebx; ret`: traced | 0 |
| 0x28C38 | `flow_winner_pose_step` | 93 | `0x2528C` (in `0x24C5C`): `jmp 0x2540F` (`call 0x2a31c` ...): traced | 0 |
| 0x29B70 | `game_null_step` | 1 | `0x20E0B`, `0x2521A`, `0x25224`, `0x2522E`. The function is `c3`: it writes no EAX | 0 |
| 0x29CBC | `flow_challenge_handle` | 30 | `0x42979`, `0x42A13`, `0x42AAD`, `0x42AE3` (in `0x428B8`): `mov [0xc83c4],eax` at `0x42984`, `0x42A1E`, `0x42AB8`, `0x42AEE` | 0xFFFFFFFF |
| 0x2BF08 | `game_overlay_step` | 341 | twelve sites in `0x11D04` (`0x11D7F`, `0x11D97`, `0x11DAF`, `0x11DC7`, `0x11DDF`, `0x11E46`, `0x11E5E`, `0x11E86`, `0x11EA3`, `0x11EC7`, `0x11F05`, `0x11F1D`), each followed by `pop edx; pop ecx; pop ebx; ret`: traced | 0 |
| 0x2C06C | `config_set_credit_row` | 6 | `0x110CE`, `0x115C1`, `0x11A51`, `0x11AA3`, `0x11E03`, `0x11FB4`. The function is `mov [0x105c05],al; ret`: it writes no EAX | 0 |
| 0x2C074 | `flow_105bf0_clear` | 17 | `0x20E47`, `0x20EEE`. `push edx; xor edx,edx;` two dword stores; `pop edx; ret`: it writes no EAX | 0 |
| 0x2C088 | `prompt_press_start_clear` | 106 | `0x27960`: `0x2796F mov eax,0xffffffff`; `0x2C2CA`: `0x2C2D7 xor eax,eax`; `0x43316`: `0x4331B mov eax,0x39`; `0x43400`: `0x43405 mov eax,0x235`; `0x2C16F` (in `0x2C0F4`): `pop edi; pop esi; ret`: traced | 0 |
| 0x2C1D4 | `prompt_insert_coin_blink` | 219 | `0x27B92` (in `0x27A2C`): `0x27B97 or byte [0x104aec],2`, pops, `ret`: traced. The block `0x2C1AD..0x2C1C7` before the entry belongs to this function (reached from `0x2C1F1`) | 0 |
| 0x2C390 | `flow_list_105c0c_init` | 77 | `0x20E2E`, `0x20ED5`: `call 0x12750` (`0x12782 mov eax,0xf0a78`), `call 0x49300` (`0x4933D mov eax,0x1083c4`, `0x49354 xor eax,eax`): traced | 0 |
| 0x32968 | `game_init_null` | 1 | `0x20CC7`. The function is `c3` | 0 |

### §1.1 The liveness trace, and what it does not prove

An ad-hoc script (scratch, not committed) walked the image from the instruction after each of the 53
call sites of sixteen rows (the fifteen mask-0 rows whose function can leave a value in EAX, and
`0x1D0BC`), tracking the three parts of EAX separately. It
follows direct jumps and both arms of conditional jumps, enters direct callees (runtime ones
included), and at a `ret` continues after every direct call site of the Ghidra function that contains
the returning code, to a caller depth of six, iterated to a fixpoint. It stops a path as READ at the
first instruction capstone reports as reading a live part, and as UNKNOWN on an indirect call or
jump, an `int` other than `16h`, a function with no direct caller, or a function that also appears as
a dword in the image.

Result: all 53 sites DEAD, no READ, no UNKNOWN, no depth or budget stop. As a control the same
script reports READ at the three `test al,al` of `0x1C6D4`, the four `mov [0xc83c4],eax` of
`0x29CBC`, `0x1E884` of `0x1E988` and the two `test al,al` (`0x1CA8F`, `0x1D200`) of `0x1CA40`.

Limits, stated because the mask-0 claims of the "traced" sites rest on them:

- `and eax,imm` is modelled as killing the parts whose mask bytes are zero (such as `0x12D51` and
  `0x13528` `and eax,0xff`, `0x24EFC` `and eax,0xffff`); `xor r,r` and `sub r,r` as pure writes; `int 16h` as
  reading AH and writing AX. Nothing else is special-cased.
- A `ret` is attributed to the Ghidra function whose entry precedes it. Four sites lie outside the
  listed body of that function (`0x20ED5`, `0x30DB1`, `0x30E02`, `0x430FE`); in all four EAX dies
  before any `ret` (the kills are quoted in the table), so the attribution is not used.
- Indirect callers of the intermediate functions are not followed (none of them is pointer-referenced
  in the image, which is all the script checks).
- It is a script, not a proof: it was not reviewed and is not in the repository.

## §2 The rows, measured

`tools/diff_verify.py` block `D1C_SPECS`; bindings `b_d1c_*` / `m_d1c_*` in
`port/tests/diff_runner.c`; pins `D1C_MASKS`, `D1C_KINDS`, `D1C_CASES` in
`tools/tests/test_diff_verify.py`. Every row is VERIFIED.

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `effects_step` | 0x134C0 | 29 | 90/90 | 35/35 | 0x0 | 4b7e5a0; callees `0x249B0` allow, `0x249D0` allow, `0x33734` allow; jump table `0x134A8` (six targets, all hit) |
| `frontend_resource_known` | 0x1C6D4 | 21 | 19/19 | 13/13 | 0xFF | 4b7e5a0; no callee |
| `sound_sample_pause_toggle` | 0x1D220 | 4 | 2/2 | 5/5 | 0x0 | 4b7e5a0; callee `0x1CD9C` stub |
| `sound_music_pause_toggle` | 0x1D1B0 | 9 | 10/10 | 11/11 | 0x0 | 4b7e5a0; callees `0x1CA40` stub, `0x5DEAF` stub; port correction §3 |
| `hiscore_audit_reset_due` | 0x1E988 | 6 | 4/4 | 6/6 | 0xFF | 4b7e5a0; callee `0x2D974` real |
| `game_null_step` | 0x29B70 | 1 | 1/1 | 1/1 | 0x0 | 4b7e5a0; no callee |
| `game_init_null` | 0x32968 | 1 | 1/1 | 1/1 | 0x0 | 4b7e5a0; no callee |
| `flow_challenge_handle` | 0x29CBC | 5 | 3/3 | 5/5 | 0xFFFFFFFF | 4b7e5a0; no callee; exported (§3) |
| `config_set_credit_row` | 0x2C06C | 3 | 1/1 | 2/2 | 0x0 | 4b7e5a0; no callee |
| `flow_105bf0_clear` | 0x2C074 | 1 | 1/1 | 3/3 | 0x0 | 4b7e5a0; no callee; exported (§3) |
| `flow_list_105c0c_init` | 0x2C390 | 2 | 3/3 | 5/5 | 0x0 | 4b7e5a0; callee `0x249C0` allow; exported (§3) |
| `flow_win_markers_spawn` | 0x256F4 | 6 | 13/13 | 12/12 | 0x0 | f72c9a8; callee `0x2AE14` stub |
| `flow_match_result_set` | 0x27BA4 | 16 | 16/16 | 13/13 | 0x0 | f72c9a8; no callee |
| `flow_stage_pick` | 0x25848 | 20 | 32/32 | 16/16 | 0x0 | f72c9a8; callee `0x5D7DC` stub; limit §4.5 |
| `flow_round_bonus_a` | 0x25FDC | 3 | 4/4 | 12/12 | 0x0 | f72c9a8; callees `0x2AE14` stub, `0x41310` stub |
| `flow_round_bonus_b` | 0x2604C | 3 | 4/4 | 12/12 | 0x0 | f72c9a8; callees `0x2AE14` stub, `0x41310` stub |
| `flow_winner_pose_step` | 0x28C38 | 7 | 6/6 | 9/9 | 0x0 | f72c9a8; callee `0x392A0` stub; limit §4.3 |
| `sound_music_volume` | 0x1CAB8 | 6 | 7/7 | 8/8 | 0x0 | f72c9a8; callees `0x5DECA` stub (new seam), `0x5DEED` stub |
| `sound_sfx_volume` | 0x1CED4 | 5 | 6/6 | 6/6 | 0x0 | f72c9a8; callees `0x5DCC5` stub (new seam), `0x5DD03` stub |
| `prompt_press_start_clear` | 0x2C088 | 5 | 6/6 | 10/10 | 0x0 | f72c9a8; callees `0x1C500` allow, `0x2B150` stub, `0x2F280` stub, `0x474E4` stub |
| `prompt_insert_coin_blink` | 0x2C1D4 | 5 | 14/14 | 11/11 | 0x0 | f72c9a8; callees `0x1C500` allow, `0x2F198` stub, `0x2F388` stub (new seam), `0x474E4` stub; limit §4.4 |
| `game_overlay_step` | 0x2BF08 | 9 | 14/15 | 13/13 | 0x0 | d3075f2; callees `0x1C500` allow, `0x2CAA8` allow, `0x2F280` stub, `0x2F4BC` stub, `0x474E4` stub; unhit `0x2BF87` named (§4.1) |
| `sound_buffers_alloc` | 0x1D0BC | 6 | 17/19 | 15/15 | 0xFF | d3075f2; callees `0x1C308` stub (new seam), `0x61A70`, `0x65490`, `0x654C7` allow; unhit `0x1D10E`, `0x1D183` named (§4.2) |

Totals: 23 rows, 173 cases, 274/277 blocks (the three unhit are named), 224/224 mutants detected.

The per-row line of a `--function` run prints every callee as "unverified": that is the
single-function run, not a finding. Whether each callee is VERIFIED by its own row, and the branch's
closed-row figure, come from the full `--self-check`, which was not run (§7).

### §2.1 The mutants

Binding name `<row>@<suffix>`. What catches each one (kind, and the exact case set) is pinned in
`D1C_KINDS` and `D1C_CASES`; those 224 + 224 pins were compared against a fresh measurement of every
mutant and are equal (0 differences), and `D1C_MASKS` equals the specs' masks. The record does not
repeat the case sets, so there is one copy to keep in sync.

- `effects_step`: `@lock` `@type0` `@dec` `@reload` `@default` `@t1mask` `@t1delta` `@t1first` `@t1sext` `@rotdir` `@rotsub` `@offcache` `@ncache` `@t3flag` `@t6flag` `@ptr` `@palcount` `@nopal` `@eq` `@step` `@clamp` `@top` `@floor` `@count` `@t4zero` `@t6to` `@t5flag` `@t5dec` `@t5base` `@t5tgt` `@t5zext` `@t5remove` `@remove` `@free` `@stop`
- `frontend_resource_known`: `@deref` `@low16` `@range` `@next` `@c0` .. `@c8`
- `sound_sample_pause_toggle`: `@set` `@nz` `@nocall` `@order` `@byte`
- `sound_music_pause_toggle`: `@clear` `@d9` `@song` `@seq` `@cc` `@eq` `@gate` `@al` `@nostop` `@set` `@order`
- `hiscore_audit_reset_due`: `@t27` `@t26` `@u27` `@u26` `@swap` `@one`
- `game_null_step`, `game_init_null`: `@mutant`
- `flow_challenge_handle`: `@side` `@tbl` `@scale` `@char` `@one`
- `config_set_credit_row`: `@width` `@off`
- `flow_105bf0_clear`: `@f0` `@f4` `@width`
- `flow_list_105c0c_init`: `@count` `@head` `@stride` `@sent` `@first`
- `flow_win_markers_spawn`: `@swap` `@bound` `@sext` `@cap` `@skip` `@step` `@base` `@a3` `@a4` `@a5` `@store` `@desc`
- `flow_match_result_set`: `@eqfirst` `@two` `@bsign` `@ugt` `@d` `@ge0` `@ge1` `@inc` `@cap` `@wsext` `@widx` `@r2` `@store`
- `flow_stage_pick`: `@rng7` `@rng6` `@csext` `@cidx` `@plus` `@mod` `@ge` `@mode2` `@or40` `@mask` `@n` `@pick` `@wrap` `@k` `@step` `@word`
- `flow_round_bonus_a`, `flow_round_bonus_b`: `@bit` `@desc` `@a2` `@a3` `@a4` `@a5` `@slot` `@clr` `@or` `@side` `@val` `@order`
- `flow_winner_pose_step`: `@inc` `@pre` `@unsigned` `@le` `@sext` `@slot` `@args` `@byte2` `@early`
- `sound_music_volume`: `@same` `@store` `@seq` `@status` `@ms` `@vol` `@order` `@sfx`
- `sound_sfx_volume`: `@same` `@store` `@status` `@count` `@vol` `@mus`
- `prompt_press_start_clear`: `@bit` `@dead` `@zero` `@side` `@pos` `@sext` `@swap` `@id` `@mode` `@order`
- `prompt_insert_coin_blink`: `@phase` `@p18` `@order` `@mid` `@mode` `@c06` `@c07` `@swap` `@len` `@cnt` `@relpos`
- `game_overlay_step`: `@attract` `@free` `@id45` `@id47` `@p20` `@hold` `@col` `@colf` `@gate` `@c04` `@clr` `@sext` `@order`
- `sound_buffers_alloc`: `@once` `@c4` `@c0` `@d0` `@size` `@zero` `@len` `@c8` `@first` `@rest` `@stride` `@four` `@stop` `@set` `@ret`

Each mutant's port side was measured twice here (the self-check and the pin comparison) and once by
the first agent, with the same result each time. The brief's "ten runs before pinning" was not
repeated. The entry registers of every case are small (sides 0/1, characters 0..6, the record
`0x10A000`); the memory fields each binding and mutant follows were not audited for the mem[] bound.

### §2.2 Hand resolutions and pins added outside the D1C block

- `tools/diff_emu.py` `RESOLVED_JUMPS[0x1353A] = (0x13520, 0x134A8, 6)`: `cmp al,5; ja 0x13996; and
  eax,0xff; lea esi,[edi+0x14]; lea edx,[eax*4]; lea eax,[edi+0x10]; jmp cs:[edx+0x134a8]`. The table
  holds `0x13541 0x135A8 0x1362E 0x1374F 0x137EE 0x13996` and ends at the entry `0x134C0`. The
  window's pin (`WINDOWS[0x1353A]` in the test) equals the image's disassembly.
- The stub-clobber pin of `test_each_stub_declares_the_registers_its_callee_clobbers` gained
  `0x5DECA`, `0x5DCC5`, `0x2F388`, `0x1C308`. Checked without running the suite: the pinned dictionary
  equals the stubs of `V.SPECS`, and `E.callee_clobbers` on the image returns the declared set for
  each of the eighteen stub addresses the D1C rows use.

## §3 Port changes

### §3.1 Correction: `sound_music_pause_toggle` tests AL of `0x1CA40` (commit `d55a7e4`)

`port/src/game/flow.c`, 0x1D1B0. Old: `snd_music_playing() != 0u` (the whole u32). New:
`(u8)snd_music_playing() != 0u`. The proving instruction:

```
1D1FB  e8 40 f8 ff ff   call 0x1ca40
1D200  84 c0            test al,al
1D202  74 0f            je   0x1d213
```

Found by case `r7` (stub EAX `0x100`): the old port made a `0x5DEAF` call the original does not. The
mutant `sound_music_pause_toggle@al` reproduces the old behaviour and is detected.

It cannot change the game. `0x1D1FB` is reached only with `[0x1028c0] != 0` (`0x1D1F2 cmp dword
[0x1028c0],0` / `0x1D1F9 je 0x1d213`), and on that path `0x1CA40` returns through `0x1CA5F sete al` /
`0x1CA62 and eax,0xff`, so EAX is 0 or 1. The correction is a width fidelity fix that only a stub can
observe; no oracle was run against it, and `frames/` (generated before the commit) was not
regenerated.

Not changed, same class: `snd_music_stop` (0x1CA6C) tests the whole u32 (`snd_music_playing() ==
0u`) where the raw has `0x1CA8F test al,al`. Its existing row stubs `0x1CA40` with 0 and 1 only. It
is outside this batch's queue and the brief forbids editing existing rows; it is as unobservable in
the game as the one above.

### §3.2 Seams and exports (no behaviour change)

| address | function | change |
|---|---|---|
| 0x2F388 | `text_cells_release_count` (actors.c) | `PR_SEAM(0x2F388u, col, row, count)`; `E.Call` `("eax","edx","ebx")`, clobbers `ebx edx` |
| 0x5DCC5 | `AIL_set_sample_volume` (ail.c) | `PR_SEAM(0x5DCC5u, (u32)volume)`; `E.Call` `("s1",)` |
| 0x5DECA | `AIL_set_sequence_volume` (ail.c) | `PR_SEAM(0x5DECAu, (u32)volume, fade_ms)`; `E.Call` `("s1","s2")` |
| 0x1C308 | `res_block_alloc` (res.c) | `PR_SEAM_RET(0x1C308u, size)`; `E.Call` `("edx",)`, clobbers `edx` |
| 0x29CBC | `flow_challenge_handle` | `static` dropped, declared in flow.h |
| 0x2C074 | `flow_105bf0_clear` | `static` dropped, declared in flow.h |
| 0x2C390 | `flow_list_105c0c_init` | `static` dropped, declared in flow.h |

Three seams pass less than the callee's signature, by design: the AIL handle (the first stack
argument) is a host pointer in the port and is not compared, and `0x1C308`'s EAX (the tag `0x41`) has
no C counterpart. So those rows do not check which handle or which tag is passed.

No existing row changes: the hook (`seam_hook`, diff_runner.c) returns 0 for an address that is not
in the running case's call set, and no spec outside the D1C block names any of the four addresses.
That is an argument from the code; the existing rows were not re-run.

## §4 Named gaps and unhit blocks

1. **`game_overlay_step` `0x2BF87`, the credits arm (1 block unhit).** `0x2BF87..0x2BFA7` formats
   string `0x46` and the credit count through the runtime's sprintf (`0x2BFA7 call 0x65546`), which
   leaves the flat model under the harness (the spec's note: its `0x6C8EB` core writes `0xFF30..` and
   stops on an `int3` at `0x10000`); the port formats with `snprintf` and has no seam there. No case
   enters the arm, so the port's C for it is not covered by this row.
2. **`sound_buffers_alloc` `0x1D10E` and `0x1D183` (2 blocks unhit).** Both failure arms end in the
   runtime's printf: `0x1D10E push 0x5100` .. `0x1D12A call 0x62734` (the MIDI buffer: `0x1028C4`,
   `0x1028C0`, `0x1028CC` dropped) and `0x1D183 push 0x8c00` / `0x1D18D call 0x62734` / `0x1D195 mov
   [0x1028c8],ecx` (no slot-0 buffer). The harness cannot run `0x62734` and the port does not call it
   (`/* PORT: */` in flow.c), so a recorded call would differ by design. The port's stores for both
   arms exist (flow.c `0x1D113`, `0x1D11E`, `0x1D124`, `0x1D195`) and are not covered by this row.
3. **`flow_winner_pose_step`, the `idiv` at `0x28C52`.** A zero `[0x104aa8]` and `INT_MIN / -1` raise
   #DE in the raw; the port's `%` is undefined there. No case, by domain.
4. **`prompt_insert_coin_blink`, the 0x14-byte stack buffer.** The raw's three copies are unbounded;
   the port bounds them with `snprintf`. The `0x2F198` seam takes the string's first 16 bytes by
   value, so the two drawing cases build 15 and 19 characters; a shorter string leaves stack bytes
   after its NUL in that window and a longer one overruns the raw's buffer into its saved col.
   Neither is a case: equivalence is claimed for strings of 15..19 characters only.
5. **`flow_stage_pick`, the k = -2 arm.** With `[0x104ad4] = -1` the raw reads `0x259A8 mov
   al,[esp+ecx]` below its 4-byte frame; the port returns (`/* PORT: */` in flow.c). Case `f10` runs
   it with a list no stack byte matches, so both exit unchanged; a matching leftover byte is not
   modelled.
6. **`sound_buffers_alloc` mask.** 0xFF although no caller reads EAX (§1): not a gap, the stricter
   comparison holds.

## §5 Store sweep (checklist items 1-2)

What was checked, mechanically, on the original side only: an ad-hoc script (scratch, not committed)
hooked every memory write of the original for every case of the 23 rows (private stack excluded) and
compared the written bytes with the bytes that differ from the case's start.

- **Item 1, every store observable.** In 22 rows every distinct store (address + size) changes at
  least one byte in some case: `effects_step` 33 stores, `flow_list_105c0c_init` 36,
  `flow_win_markers_spawn` 9, `flow_match_result_set` 3, `flow_round_bonus_a`/`_b` 3 each,
  `prompt_press_start_clear` 3, `prompt_insert_coin_blink` 3, `sound_music_pause_toggle` 2,
  `flow_105bf0_clear` 2, one each for `sound_sample_pause_toggle`, `config_set_credit_row`,
  `flow_stage_pick`, `flow_winner_pose_step`, `sound_music_volume`, `sound_sfx_volume`,
  `game_overlay_step`; five rows write nothing. The exception is `sound_buffers_alloc`: the stub puts
  the block at `0xC0000`, where only 1960 of the 0x5100 bytes are non-zero in the image (first at
  `+0x1D`, last at `+0x50FD`), so 3426 of the memset's dword stores land on bytes that are already
  zero. The memset as a whole is observed (`@zero` and `@len` are detected, the last non-zero byte is
  three from the end); a memset with a hole over a zero run would not be.
- **Dropped stores.** The rows carry a store-dropping mutant for the fields read in the bindings
  (`sound_music_pause_toggle@clear`, `@cc`, `@set`; `flow_win_markers_spawn@store`), and all are
  detected. Not every store of every row has its own dropping mutant; that was not enumerated.
- **Bytes written that never change** (so a narrower store would agree there): the upper bytes of the
  list links in `effects_step` (`0xF0B02..3`, `0xF0B05..7` and the like: every node is in
  `0x000Fxxxx`), byte 3 of the handles in `flow_win_markers_spawn`, the high byte of the two words
  `0x108860[r]` in `flow_match_result_set` (the word is stored only when it is below 0xC8, so its
  high byte is 0 before and after), and bytes 0 and 3 of the slot dword in
  `prompt_press_start_clear`. In each the store is still observed through its other bytes; the first
  three cannot differ by construction.
- **Item 2, neighbours.** For every byte or word store the next byte is non-zero in some case, with
  two exceptions, both in `effects_step`: `0xF1322` (record 2's +0xE, whose +0xF is 0 in the two
  cases with a second record, `l2` and `m0`; the same field is observed on record 1 with a non-zero
  +0xF, whether by the same instruction was not checked) and
  **`0x9AF3D`, the active counter**. `0x9AF3E` is never seeded and the counter is 0x40 in every case,
  so a 16-bit decrement at `0x13744`, `0x137E3`, `0x13885..0x13891`, `0x13969..0x13975` or `0x13AAC`
  (all `dec byte` / byte moves in the raw) would agree with the byte one. This is an open hole in the
  row, found here and not closed: closing it needs a case with the counter at 0 and a seeded
  `0x9AF3E`, and a re-measure of the row's 35 pins.
- **Not checked:** the bytes below each field, the port side's writes (the row's byte comparison
  covers them only where the original changes a byte), and checklist items 3-12 beyond what §2 states.

## §6 Queue entries not rowed

| addr | size | port | why |
|---|---|---|---|
| 0x1CF40 | 215 | `game_audio_init` (flow.c) | Not comparable. The raw takes AL/DL gates and stores what its ten AIL thunks return: `0x1CF8E mov [0x1028c8],eax` (the DIG driver), `0x1CFAC mov [esi+0x102848],eax` (four sample handles), `0x1CFD6 mov [0x1028c4],eax`, `0x1CFE8 mov [0x1028c0],eax`. The port takes no argument, keeps the handles as host objects (`s_samples`, `s_sequence`), stores 1 at `0x1028C8` and nothing at the others, and adds work the raw does not have (`DS_000A2CB4 = 0x7f`, the `FAT.OPL` load, `mixer_reset`), each under a `/* PORT: */` comment. A row would differ on memory by design. |
| 0x1D018 | 161 | no C function: a comment inside `game_shutdown` (0x1BE30) | There is nothing to bind. The inline body is also a simplification of the raw: it stops the sequence unconditionally where the raw tests `0x1D031 cmp dword [0x1028c4],0` and the status (`0x1D041 call 0x5deed` / `0x1D049 cmp eax,4`), it has no counterpart of the four-slot loop (`0x1D07D call 0x5dd03` / `0x1D085 cmp eax,2` / `0x1D091 call 0x5dc8b` / `0x1D09E mov [esi+0x102848],ebx`), and it does not clear `0x1028C4` / `0x1028C0` (`0x1D05F`, `0x1D065`), which the port never sets. It runs once, at teardown. Rowing it needs a function of its own and a seam on `0x5D86A` (its other four AIL callees have one); not attempted. |

Neither was rowed "cheaply": both are host-object code, and neither carries a `/* PORT: */` marker
that says so for the differences listed for `0x1D018`.

## §7 What was run, and what was not

Run on `d3075f2`: the build; the 23 per-function self-checks (§2); the pin comparison (§2.1); the
static checks of §2.2; and

```
$ PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -3
test_frontend: PR_FRONTEND_DUMP unset, state-2 driver skipped
test_title: PR_TITLE_DUMP unset, title dump driver skipped
all checks passed
$ make entry-triage E2_IMAGE=/tmp/pr_d1c_e2.bin 2>&1 | tail -2      (exit 0)
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
$ python3 tools/port_progress.py
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

Not run (the integrator's): the full `--self-check` (no counter line for this branch), the unittest
suite, `make verify`, every `*-oracle`, `--check 820`. The counter test is expected to fail on this
branch until re-pinned. The branch is not merged with the current `reverse-d1`: a trial
`git merge-tree` reports textual conflicts in `diff_runner.c`, `diff_emu.py`, `diff_verify.py` and
`test_diff_verify.py` (adjacent batch blocks), and `reverse-d1` has since gained a seam on `0x249C0`
(effects.c), which `flow_list_105c0c_init` runs in allow mode: that row should be re-measured after
the merge.
