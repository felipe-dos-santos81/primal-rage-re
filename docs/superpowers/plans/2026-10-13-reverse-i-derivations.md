# Track I derivations: the master init and the inlined functions

Plan `2026-10-13-reverse-i-master-init.md`. Sources: capstone over the fixed-up image
(`./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_i_img.bin`; image offset = address −
`0x10000`) and the harness's emulator (`tools/diff_emu.py` `run_original`). One section per function.
§I.1-§I.3 are task 1: three functions the port inlined in their callers become C functions, each with a
differential row. They already counted as ported (`tools/port_progress.py` prints `787 1203 65` and
`747 747 100` before and after) and the E2 table is byte-identical to a fresh run.

Callers were found by scanning the code object for `call rel32` and `jmp rel32` to each entry and the
whole image for the entry as a dword (a code pointer in data):

| Entry | `call rel32` | `jmp rel32` | dword in the image |
|---|---|---|---|
| `0x2F920` | `0x20C42`, `0x2BBDF` | none | none |
| `0x11BCC` | `0x11E77` | none | none |
| `0x12CD4` | `0x131CB` | none | none |

## §I.1 `0x2F920` — `actors_table_clear` (`actors.c`)

```
02F920  push ebx
02F921  push edx
02F922  mov ebx, 0x14d4
02F927  mov eax, 0x105f38
02F92C  xor edx, edx
02F92E  call 0x61a70
02F933  pop edx
02F934  pop ebx
02F935  ret
```

One block. `0x61A70` is the runtime memset (EAX = destination, EDX = the byte, EBX = the length): it
replicates DL into EDX (`0x61A72..0x61A7C`), moves the length to ECX and calls `0x65490` (the byte and
dword fill, which calls `0x654C7`); it keeps EAX and ECX (`push eax; push ecx` `0x61A70`/`0x61A71`,
`pop ecx; pop eax` `0x61A85`/`0x61A86`). So the function zeroes the `0x14D4` bytes
`0x105F38..0x10740B` and returns with EAX = `0x105F38`, EBX and EDX as they were. The brief's reading is
confirmed.

**EAX.** No caller reads it: after `0x2BBDF` the next EAX accesses are `0x2BBE8 xor eax,eax` (the
non-zero arm) and `0x2BC08 mov eax,ecx` (the copy arm); after `0x20C42`, `0x20C47 xor eax,eax`. The C
function is `void`; the row's mask is 0.

**Port.** `void actors_table_clear(void)` is `mem_fill(DS_00105F38, 0, 0x14D4u)`. `actors_reset_al`
calls it at `0x2BBDF`'s place (it held the same `mem_fill`).

**The second caller, `0x20C42`: not wired.** It is in the post-`setjmp` tail of `0x20C10`, the port's
`game_init_resume`: `0x20C37 mov [0x104b1d],dl; 0x20C3D call 0x38b70; 0x20C42 call 0x2f920; 0x20C47 xor
eax,eax; 0x20C49 call 0x4f228; 0x20C4E mov eax,1; 0x20C53 mov ebx,0xabcd; 0x20C58 call 0x2baf4`. The
port leaves out `0x38B70`, `0x2F920` and `0x2BAF4(1)` there as unobservable (record named-gaps-b §B.11:
`0x10E80` runs `0x2BAF4(1)` again at `0x10E9C`, and nothing between the two reads what they reset), and
that omission set stands: `game_init_resume` does not call `actors_table_clear()`, whose one caller in
the port is `actors_reset_al`. Task 1 first wired the call at `0x20C42`'s place (commit `82d93467`);
the controller reverted it (`3947c81a`) because the tail's omissions are one documented set, and the
call alone, with `0x38B70` before it and `0x2BAF4(1)` after it still out, would have split it. What
that commit showed is kept as evidence for §B.11: the grid's readers are the text-cell functions of
`actors.c` (`text_glyph_emit`, the `text_cells_release*` family), none of which runs between `0x20C42`
and `game_state_init`'s `actors_reset`, which clears the grid again, so no assertion can tell the tail
with the call from the tail without it; and the `PR_RESTART` driver's output was byte-identical with
the call and without it (it compares the frames and state words of the boot with those after a soft
restart). `--check` and the oracles were not run by this task.

**Row.**

| row | entry | cases | blocks | mutants | EAX mask | callees |
|---|---|---|---|---|---|---|
| `actors_table_clear` | `0x2F920` | 3 | 1/1 | 5 | 0 | `0x61A70`, `0x65490`, `0x654C7` run on the original side |

A case holds 40 pokes of at most `0x40` bytes (`diffrun`'s parser), and the range with its two
neighbours is `0x14D6` bytes, 84 pokes: the three cases take every third poke, so between them every
byte of the range is seeded non-zero (`t0` `0xA5`, `t1` `1 + i % 255` with every register dirty, `t2`
`0xFF`). `t0` holds the first byte and `0x105F37 = 0x5A`, `t2` the last byte and `0x10740C = 0x5B`. The
bytes a case does not poke are the image's, which are zero over the whole range (read from the image),
so within one case a clear of them cannot be seen; the unit case seeds all of them at once.

| mutant | the wrong port | caught by | cases |
|---|---|---|---|
| `@short` | length `0x14D3` (the last byte kept) | byte | `t2` |
| `@long` | length `0x14D5` (`0x10740C` cleared) | byte | `t2` |
| `@base` | from `0x105F39` (the first byte kept) | byte | `t0` |
| `@low` | from `0x105F37` (the byte below cleared) | byte | `t0` |
| `@none` | no fill | byte | `t0`, `t1`, `t2` |

**The caller's row.** `actors_reset_al` (`0x2BAF4`) runs `0x2F920` on the original side
(`allow_calls`); the port's function has no seam, so the row is unchanged: VERIFIED, 34/34 mutants, its
pins untouched. Its spec comment said "no port function: the port fills inline" and now names the
function.

**Unit case `table`** (`test_inlined_functions`, `test_game.c`): the `0x14D4` bytes seeded `0xA5`,
`0x105F37 = 0x5A`, `0x10740C = 0x5B`. Mutations of `actors_table_clear`
(`PR_ORACLE_REQUIRED=1 ./build/run_tests`, restored after each):

| # | mutation | measured |
|---|---|---|
| T1 | length `0x14D3` | `test_game.c:4959: 1 != 0` (one byte left) |
| T2 | from `0x105F37`, length `0x14D5` | `test_game.c:4960: 0 != 90`; also `test_fight.c:4872` four times |
| T3 | length `0x14D5` | `test_game.c:4961: 0 != 91`; also `test_game.c:13479` |

## §I.2 `0x11BCC` — `game_state_timer_exit` (`flow.c`)

```
011BCC  xor ah, ah
011BCE  mov byte ptr [0x104b1b], ah
011BD4  mov byte ptr [0x104b15], ah
011BDA  mov ax, word ptr [0xf0a6c]
011BE0  mov word ptr [0xf0a64], ax
011BE6  call 0x29d60
011BEB  mov eax, 0x100
011BF0  jmp 0x2c3fc
```

No argument (AH is cleared before its first use). The bytes `0x104B1B` and `0x104B15` become 0, the
state word `0xF0A64` takes the word `0xF0A6C`, `0x29D60` is called (`029D60 ret`: one byte), then the
voice `0x2C3FC` with EAX = `0x100` by a tail jump: it returns to `0x11BCC`'s caller (`0x11E7C`). The
brief's reading is confirmed.

**EAX.** It is the voice's. After the one call site `0x11E77` the caller runs its three tails, and each
loads AH before reading it and reads nothing else of EAX until it loads it whole: `0x10DB0` (`0x10DB1
mov ah,[0xf0a71]`; then `0x10E01 mov eax,0x100`), `0x10E18` (`0x10E19 mov ah,[0xf0a71]`; `0x10E69 mov
eax,0x100`), `0x2BF08` (`0x2BF0E mov ah,[0x9ad58]`, `0x2BF16 test ah,ah; jne 0x2c056`, a plain exit; else
`0x2BF1E call 0x2caa8`, whose `sete al; and eax,0xff` sets all of EAX). `0x11D04`'s own EAX is not read
(record 2026-10-12-reverse-d5h §1). The C function is `void`; the row's mask is 0.

**Port.** `void game_state_timer_exit(void)`: the two byte stores, the word copy, a `PORT:` note for
`0x29D60` (a named non-row, not called by the port, as before), `(void)sound_voice(0x100u)`.
`game_state_step`'s case 7 calls it where the copy was (`0x11E77`), and the tails follow the call as
they followed the copy, so the caller's order is unchanged (Review Focus 4). The function has no seam.

**Row.**

| row | entry | cases | blocks | mutants | EAX mask | callees |
|---|---|---|---|---|---|---|
| `game_state_timer_exit` | `0x11BCC` | 2 | 1/1 | 12 | 0 | `0x29D60` run on the original side; `0x2C3FC` a recorded stub (`VOICE`, args `eax`) |

Both cases seed `0x104B14..0x104B16` and `0x104B19..0x104B1D` and the seven words `0xF0A62..0xF0A6F`.
`e0`: state `0x4321`, next state `0x1234`. `e1`: every register dirty (AH = `0xFF` before the `xor`),
state `0x0007`, next state `0x8001`, the stub's EAX 0. The stub's EAX is not read by `0x11BCC` (the
`jmp` is its last instruction) and is outside the mask. The memory at the voice call is compared, so a
store after the call is seen.

| mutant | the wrong port | caught by | cases |
|---|---|---|---|
| `@nob1b` | `0x104B1B` not cleared | byte, call #0 memory | `e0`, `e1` |
| `@nob15` | `0x104B15` not cleared | byte, call #0 memory | `e0`, `e1` |
| `@b19` | `0x104B19` cleared for `0x104B1B` | byte, call #0 memory | `e0`, `e1` |
| `@b1bw` | a word store at `0x104B1B` | byte, call #0 memory | `e0`, `e1` |
| `@b15w` | a word store at `0x104B15` | byte, call #0 memory | `e0`, `e1` |
| `@swap` | the word copied the wrong way (`0xF0A6C = 0xF0A64`) | byte, call #0 memory | `e0`, `e1` |
| `@nostate` | no copy | byte, call #0 memory | `e0`, `e1` |
| `@stated` | a dword copy | byte, call #0 memory | `e0`, `e1` |
| `@stateb` | a byte copy | byte, call #0 memory | `e0`, `e1` |
| `@voice0` | voice 0 for `0x100` | call #0 | `e0`, `e1` |
| `@novoice` | no voice | call #0 | `e0`, `e1` |
| `@voicefirst` | the voice before the stores | call #0 memory | `e0`, `e1` |

**The caller's row.** `game_state_step_high` (`0x11D04`) runs `0x11BCC` and `0x29D60` on the original
side and records the voice: unchanged, VERIFIED, 35/35 mutants, pins untouched. Its spec comment said
"inlined in the port" and now names the function.

**Unit case `timer`**: the nine bytes `0x104B14..0x104B1C` seeded `0x14..0x1C` with `0x104B15 = 0x5B`
and `0x104B1B = 0x5A`; the seven words from `0xF0A62` seeded with `0xF0A64 = 0x4321`, `0xF0A6C =
0x1234`. Two assertions: the nine bytes equal the seeds with the two bytes zero; the seven words equal
the seeds with `0xF0A64 = 0x1234`. The voice id is not asserted here: voice-site row 195
(`test_voice_sites`) drives the same call through `game_state_step`. Mutations of
`game_state_timer_exit`:

| # | mutation | measured |
|---|---|---|
| M1 | `0x104B1B` not cleared | `test_game.c:4978` (the bytes); also `test_game.c:264`, `test_fight.c:4229` |
| M2 | `0x104B15` not cleared | `test_game.c:4978`; also `test_game.c:263`, `test_fight.c:4228` |
| M3 | the word copied the wrong way | `test_game.c:4980: 2 != 0`; also `test_game.c:262`, `test_fight.c:4227` |
| M4 | a dword copy | `test_game.c:4980: 1 != 0`, alone |
| M5 | a word clear at `0x104B1B` | `test_game.c:4978`, alone |

## §I.3 `0x12CD4` — `camera_y_step` (`camera.c`)

```
012CD4  push ebx
012CD5  push ecx
012CD6  push edx
012CD7  mov edx, dword ptr [0xf0aec]
012CDD  mov ebx, eax
012CDF  sub eax, edx
012CE1  test eax, eax
012CE3  jge 0x12ceb
012CE5  mov ecx, eax
012CE7  neg ecx
012CE9  jmp 0x12ced
012CEB  mov ecx, eax
012CED  cmp ecx, 0x100
012CF3  jg 0x12cf9
012CF5  mov edx, ebx
012CF7  jmp 0x12d1d
012CF9  test eax, eax
012CFB  jle 0x12d17
012CFD  add edx, 0x100
012D03  mov eax, dword ptr [0xf0af2]
012D08  sar eax, 0x10
012D0B  add edx, eax
012D0D  mov dword ptr [0xf0aec], edx
012D13  pop edx / pop ecx / pop ebx / ret
012D17  sub edx, 0x100
012D1D  mov eax, dword ptr [0xf0af2]
012D22  sar eax, 0x10
012D25  add edx, eax
012D27  mov dword ptr [0xf0aec], edx
012D2D  pop edx / pop ecx / pop ebx / ret
```

EAX = the target. Nine blocks: `0x12CD4`, `0x12CE5`, `0x12CEB`, `0x12CED`, `0x12CF5`, `0x12CF9`,
`0x12CFD`, `0x12D17`, `0x12D1D`. `diff = target − [0xF0AEC]` (a wrapping `sub`); `mag = diff`, negated
when negative; `mag <= 0x100` (signed: `jg` not taken) takes the target; else `diff > 0` (`jle` not
taken) adds `0x100` and the others subtract it; then the dword `0xF0AF2` shifted right 16 arithmetically
is added and the dword `0xF0AEC` stored. The brief's reading is confirmed.

**The difference `0x80000000`** (Review Focus 3). `0x12CE1 test eax,eax; 0x12CE3 jge` is not taken;
`0x12CE7 neg ecx` leaves `0x80000000`; `0x12CED cmp ecx,0x100; 0x12CF3 jg` compares signed, so
`0x80000000` is below `0x100` and the jump is not taken: `0x12CF5 mov edx,ebx`, the camera takes the
target. The `0x12CF9 test eax,eax; 0x12CFB jle` pair is not reached. From `[0xF0AEC] = 0` and the target
`0x80000000` with the offset 3 the result is `0x80000003`. The port computes the difference, the
negation, the step and the sum in `u32`, so no signed overflow occurs; it compares `mag` as `s32`.

**EAX.** At the `ret` it is the offset (`sar` result). The one caller clears it at once (`0x131D0 xor
eax,eax` after `0x131CB`). The C function is `void`; the row's mask is 0.

**Port.** `void camera_y_step(s32 target)`; `camera_y_clamp` calls it at `0x131CB`'s place. The offset
is read as the word `DS_000F0AF4` sign-extended, which is the dword `0xF0AF2` `sar 16` (`0xF0AF2` is
not a global of its own; a `PORT:` note says so). The function has no seam.

**Correction to the brief (the mutant list).** "`>=` for `>` at `0x100`", a port that snaps only below
`0x100`, is an equivalent mutant: at a magnitude of exactly `0x100` the step arm adds or subtracts
`0x100`, which lands on the target, the value the snap arm stores. Measured with a temporary mutant
`@ge` (`mag < 0x100`): `NOT DETECTED … (verdict VERIFIED)` on all 14 cases; it is not in the row. The
threshold is pinned from both sides by the nearest observable ones: `@snapwide` (`mag <= 0x101`, caught
at ±`0x101`) and `@snapnarrow` (`mag <= 0xFE`, caught at ±`0xFF`). The same holds for a `diff >= 0` step
test: it differs only at `diff == 0`, which snaps.

**Row.**

| row | entry | cases | blocks | mutants | EAX mask | callees |
|---|---|---|---|---|---|---|
| `camera_y_step` | `0x12CD4` | 14 | 9/9 | 15 | 0 | none |

Every case seeds the camera block `0xF0AEC..0xF0AFF` (`d1h_cam`: y, x = `0xC1C2C3C4`, whose high word
is the low word of the dword `0xF0AF2`; the shake word `0xF0AF4`) and the dword below it. `C` is
`0x0001FF80`, a camera y whose `0x100` step up carries into the high word.

| case | camera, target | offset word | arm |
|---|---|---|---|
| `n0` | `C`, `C` | 3 | snap, diff 0 |
| `n1`, `n2` | `C`, `C ± 0xFF` | 3 | snap |
| `n3`, `n4` | `C`, `C ± 0x100` | 3 | snap at the limit (`jg` not taken) |
| `u0`, `d0` | `C`, `C ± 0x101` | 3 | the first step up, down |
| `u1` | `C`, `C + 0x5000` | `0x0030` | step up |
| `d1` | `C`, `C − 0x5000` | `0xFFF0` (−16) | step down |
| `w0` | `C`, `C + 0x40` | `0x8000` (−32768) | snap |
| `m0` | 0, `0x80000000` | 3 | the difference `0x80000000`: snap |
| `m1` | `0x7FFFFF80`, `0x80000200` | 3 | a step up through `0x7FFFFFFF` |
| `m2` | `0x80000080`, `0x7FFFFE00` | 3 | a step down through `0x80000000` |
| `m3` | 0, `0x80000001` | 3 | magnitude `0x7FFFFFFF`, step down |

| mutant | the wrong port | cases that catch it (all by a byte) |
|---|---|---|
| `@diffrev` | `cam − target` | `u0 u1 d0 d1 m1 m2 m3` |
| `@noabs` | no negation | `d0 d1 m2 m3` |
| `@snapwide` | snap up to `0x101` | `u0 d0` |
| `@snapnarrow` | snap up to `0xFE` | `n1 n2` |
| `@unsigned` | the magnitude compared unsigned | `m0` |
| `@nosnap` | always step | `n0 n1 n2 w0 m0` |
| `@dir` | the step sign | `u0 u1 d0 d1 m1 m2 m3` |
| `@step` | a `0x80` step | `u0 u1 d0 d1 m1 m2 m3` |
| `@nooff` | no offset | all 14 |
| `@shr` | the offset zero-extended (a logical shift) | `d1 w0` |
| `@lo` | the offset from the low word of `0xF0AF2` | all 14 |
| `@storew` | a word store | `n1 n3 u0 u1 m0 m1 m2 m3` |
| `@nostore` | no store | all 14 |
| `@argw` | the target's low word | `n0 n1 n2 n3 u0 u1 w0 m0 m1 m3` |
| `@camw` | the camera read as a word | `n0 n1 n2 n3 n4 u0 u1 d0 d1 w0 m1 m2` |

**The caller's row.** `camera_y_clamp` (`0x1317C`) runs `0x12CD4` on the original side: unchanged,
VERIFIED, 25/25 mutants, pins untouched. Its spec comment and its binding's comment said "inlined in the
port" and now name the function.

**Unit case `step`**: one assertion over a table of seven rows (camera, the dword `0xF0AF2`, target,
result): `0x1000`/`0x00030000` with the targets `0x1100` (`0x1103`), `0x1101` (`0x1103`), `0xF00`
(`0xF03`), `0xEFF` (`0xF03`) and `0x1090` (`0x1093`); `0x1000`/`0xFFFE0000` with the target `0x1000`
(`0x0FFE`); and `intmin`, 0/`0x00030000` with the target `0x80000000` (`0x80000003`). The `0x1090` row is
not in the brief: without it a port that never snaps passes the brief's rows that use the offset 3 (S2
below fails only on `0x1090`, the −2 row and `intmin`). Mutations of `camera_y_step` (the assertion is
`test_game.c:5003`; the pairs are measured != expected, in decimal as printed):

| # | mutation | measured at `test_game.c:5003` | other tests |
|---|---|---|---|
| S1 | the snap arm keeps the camera | `4099 != 4355`, `4099 != 3843`, `4099 != 4243`, `3 != 2147483651` | `test_fight.c:401` |
| S2 | no snap | `4355 != 4243`, `3838 != 4094`, `4294967043 != 2147483651` | `test_fight.c:401` |
| S3 | snap up to `0x101` | `4356 != 4355`, `3842 != 3843` | none |
| S4 | the step sign | `3843 != 4355`, `4355 != 3843` | `test_fight.c:380`, `:386`, `:395` |
| S5 | the offset from the low word | all seven rows | `test_fight.c:386` |
| S6 | a logical shift | `69630 != 4094` | none |
| S7 | the magnitude compared unsigned | `4294967043 != 2147483651` | none |
| S8 | no negation | `3842 != 3843` | `test_fight.c:380`, `:386` |

## Task 1: the counter and the gates

`python3 tools/diff_verify.py … --self-check`, measured: `993/993 functions VERIFIED; 14757/14757
mutants detected; 1 named gaps; 635/824 rows with callees closed (169 have none)` (before:
`990/990`, `14725/14725`, `635/822`, `168`). The three rows add 5 + 12 + 15 mutants. Two of them have
callees and stay open (`0x2F920` on the runtime memset, `0x11BCC` on the ret-only `0x29D60`, a named
non-row); `0x12CD4` has none. No existing row's verdict or pin moved; the three callers' rows stay open
on their other callees. No seam was added.

## §I.4 `0x2F9CC` — `game_master_init` (`flow.c`)

The function, its four seams and the unit case came first (commit `7113e155`). The eight added calls
moved the boot output, and the plan's risk rule stopped the task there; the controller accepted the move
("The ruling" below), and the front-end driver's pins, the `PR_RESTART` assertion and the row followed.

```
02F9CC  push ebx
02F9CD  push ecx
02F9CE  push edx
02F9CF  call 0x1afe8
02F9D4  call 0x13adc
02F9D9  xor eax, eax
02F9DB  call 0x4f1e4
02F9E0  xor ecx, ecx
02F9E2  mov eax, 1
02F9E7  xor ebx, ebx
02F9E9  call 0x2baf4
02F9EE  xor eax, eax
02F9F0  xor edx, edx
02F9F2  call 0x4f1d0
02F9F7  mov eax, 0x9ad84
02F9FC  call 0x38b18
02FA01  mov dword ptr [0x10740c], 0x1d2d0
02FA0B  call 0x2d6f8
02FA10  mov eax, 0x2a
02FA15  call 0x2d974
02FA1A  and al, 0xfc
02FA1C  mov dword ptr [0x107410], eax
02FA21  xor eax, eax
02FA23  call 0x2c9b8
02FA28  mov eax, 0xffffffff
02FA2D  call 0x2c8f0
02FA32  mov eax, 0xfffffffe
02FA37  call 0x2c8f0
02FA3C  pop edx
02FA3D  pop ecx
02FA3E  pop ebx
02FA3F  ret
```

One block, no branch. Callers: `0x20C15` alone (`call rel32`; no `jmp rel32`, no dword in the image),
before `0x20C1A mov eax,0x1044f4; 0x20C1F call 0x653fc` (the `setjmp`). The brief's table of twelve
steps is confirmed, with one addition (step 6's EDX and EBX).

| # | Address | Raw | What the callee reads of it | Port |
|---|---|---|---|---|
| 1 | `0x2F9CF` | `0x1AFE8()` | no argument; its EAX is not read (`0x2F9D4` calls on, `0x2F9D9 xor eax,eax`) | `(void)config_file_load()` |
| 2 | `0x2F9D4` | `0x13ADC()` | no argument | `effects_init()` |
| 3 | `0x2F9DB` | `0x4F1E4`, EAX = 0 | nothing: `0x4F1E5 mov eax,0x2700` loads EAX before any read | `frontend_input_reset()` |
| 4 | `0x2F9E9` | `0x2BAF4`, EAX = 1, ECX = EBX = 0 | AL alone (`0x2BAFD mov [esp],al`); it pushes EBX, ECX, EDX, ESI, EDI, EBP (`0x2BAF4..0x2BAF9`) and loads EBX and ECX before reading them (`0x2BB05`, `0x2BB7E`) | `actors_reset()` (= `actors_reset_al(1)`) |
| 5 | `0x2F9F2` | `0x4F1D0`, EAX = EDX = 0 | nothing: `push edx; xor edx,edx; two word stores; pop edx` | `frontend_origin_zero()` |
| 6 | `0x2F9FC` | `0x38B18`, EAX = `0x9AD84`, EDX = 0, EBX = 0 | EAX (`0x38B1B mov esi,eax`), EDX (`0x38B1D mov ebp,edx`), EBX (`0x38B51 shl ebx,3`); ECX is loaded from `[0x107A1C]` (`0x38B24`) | `frontend_spawn_row(mem + 0x9AD84, 0, 0)` |
| 7 | `0x2FA01` | `[0x10740C] = 0x1D2D0` | | `DSD(DS_0010740C) = 0x1D2D0` |
| 8 | `0x2FA0B` | `0x2D6F8()` | no argument; its EAX is not read (`0x2FA10 mov eax,0x2a`) | `config_validate()` |
| 9 | `0x2FA15`..`0x2FA1C` | `[0x107410] = 0x2D974(0x2A)` with `and al,0xfc` | EAX = the field | `DSD(DS_00107410) = config_field_get(0x2A) & 0xFFFFFFFC` |
| 10 | `0x2FA23` | `0x2C9B8`, EAX = 0 | EAX, signed (`0x2C9B8 test eax,eax; 0x2C9BA jge`): 0 takes the `jge`, no voice; its EAX is not read (`0x2FA28`) | `(void)config_voice_gate(0)` |
| 11 | `0x2FA2D` | `0x2C8F0`, EAX = −1 | EAX (`0x2C8F3 cmp eax,-1`); its EAX is not read (`0x2FA32`) | `(void)attract_config_volumes_unscaled()` |
| 12 | `0x2FA37` | `0x2C8F0`, EAX = −2 | EAX (`0x2C939 cmp eax,-2`) | `attract_config_volumes()` |

**Step 6's EDX and EBX (not in the brief's table).** `0x38B18` reads three registers, and the brief's
row names EAX only. EDX is the 0 of `0x2F9F0`: `0x4F1D0` pushes and pops it. EBX is the 0 of `0x2F9E7`:
`0x2BAF4` pushes and pops it and `0x4F1D0` does not touch it. So the two `xor`s the brief lists under
calls 4 and 5 are call 6's arguments, not theirs; `0x2F9E0 xor ecx,ecx` is read by nothing (`0x2BAF4`
and `0x38B18` each load ECX first). The port's `frontend_spawn_row(desc, a2, a3)` takes all three.

**EAX at the return** is `0x2C8F0`'s (EDX, `0x2C9B1`), and the one caller loads EAX next (`0x20C1A`):
`void`, mask 0 for the row.

**The port before.** `game_init` held steps 1, 7, 8 and 9 inline. Step 2 ran only earlier, inside
`actors_init` (the raw's own `0x2BBB8` call, which stays). Steps 3 to 6 and 10 to 12 were missing.
`svcmenu_register()` (`PORT:`) stays in `game_init`, after the call.

**Seams added** (inert while `pr_seam` is NULL; the hook of `diffrun` answers 0 for an address the case
does not stub, so no existing row changed): `PR_SEAM_RET0(0x1AFE8u)` in `config_file_load`,
`PR_SEAM0(0x2D6F8u)` in `config_validate`, `PR_SEAM_RET(0x2C9B8u, (u32)v)` in `config_voice_gate`,
`PR_SEAM_RET(0x2C8F0u, 0xFFFFFFFFu)` in `attract_config_volumes_unscaled` (the −2 function already had
`PR_SEAM(0x2C8F0u, 0xFFFFFFFEu)`). Measured after them: the full `--self-check` prints the same counter
(`993/993`, `14757/14757`, 1, `635/824`, 169) and the harness suite passes (180 tests).

### The boot comparison

Baseline on `62a1f5a8`, before any source change: `make check frames=60`, the 60 frames copied to
`/tmp/pr_i_frames_before`; `./build/prageport --game-dir data/game/C --check 820`, `frames/` copied to
`/tmp/pr_i_frames_before_820`; `PR_RESTART=1 PR_GAME_DIR=data/game/C ./build/run_tests`. After the
change, the same three commands:

| Output | Result |
|---|---|
| `--check 60`, `diff -rq` | `frame_0003`..`frame_0060`: every `.idx` and `.pal` differs; every `.ppm` is identical |
| `--check 820`, `diff -rq` | `frame_0003`..`frame_0690`: 688 `.idx` and 688 `.pal` differ; all 820 `.ppm` are identical; frames 1, 2 and 691..820 are identical in all three files; `fn_miss.txt` identical |
| `--check` stdout | `fn-miss check 0x5D812 actor_spawn hits=523` -> `524` (60 frames), `7504` -> `7505` (820) |
| `PR_RESTART` driver | passes (`all checks passed`); `fn-miss PR_RESTART 0x5D812 actor_spawn hits=891` -> `892`; the line `restart after 1198 menu iterations (stamp 3E, clock 4EF)` is unchanged |
| `PR_ORACLE_REQUIRED=1 ./build/run_tests` | `all checks passed` on the old `frames/` and on the regenerated one |

**What the difference is.** In frames 3..690 the index of each pixel is a one-to-one renaming of the
baseline's and the DAC colour under it is the same (the renaming checked on frames 3, 60, 300 and 690,
the colour per pixel on frames 3 and 690; the `.ppm` equality covers all 820): index 1 becomes 219 and
the others are one lower (63 of them at frame 3, 124 at frame 690, where ten more keep their value).
The same colours sit in other palette slots.

**Which call.** Each of the eight calls removed alone, and each added alone to the four old steps
(`--check 820` and the `PR_RESTART` driver each time):

| Variant | `.idx`/`.pal`/`.ppm` differing | miss count |
|---|---|---|
| all eight (the change) | 688 / 688 / 0 | +1 |
| none of the eight | 0 / 0 / 0 | same |
| without `0x2BAF4` (`0x2F9E9`) | 0 / 0 / 0 | same |
| without `0x38B18` (`0x2F9FC`) | 0 / 0 / 0 | same |
| without any one of `0x13ADC`, `0x4F1E4`, `0x4F1D0`, `0x2C9B8`, `0x2C8F0(-1)`, `0x2C8F0(-2)` | 688 / 688 / 0 | +1 |
| any one of the eight alone | 0 / 0 / 0 | same |
| `0x2BAF4` and `0x38B18` alone | 688 / 688 / 0 | +1 |

So the two calls `0x2F9E9` (`0x2BAF4(1)`) and `0x2F9FC` (`0x38B18(0x9AD84, 0, 0)`) together are the
whole cause, and neither alone changes anything: without the reset the actor pool's free list is not
built yet at boot, `actor_alloc` returns 0 and the spawn does nothing. The other six calls change no
output.

**The mechanism** (a temporary print in `res_resolve`, removed): the descriptor `0x9AD84` holds the
handle `0x0396EB68` at `+0x10`, entry 7 of the INDEX (`0x0396EB68 >> 23`), `s16title.gra`, a lazy
entry. Its first resolve presents the loader screen and marks the entry read (`0x1B47A`). In the
original the screen is the `- LOADING -` string (`0x1B3DE mov eax,0x1e9; 0x1B3EA call 0x1C500; 0x1B3EF
call 0x1C65C`), whose glyphs acquire the font palette `0x80997C`, then the read's tick stall and the gate
re-sync `0x1B45F/0x1B464`. In the port (`res_load_present`) the boot draw had no text until the
final-review fix below: the string table was not loaded yet. Before the change that first
resolve was in master-loop iteration 3 (the frame word `0xEF6DC` = 3; its caller was not traced); with the spawn
at `0x2F9FC` it is at boot (the frame word 0), inside `game_master_init`. In iteration 3 no loader
screen is drawn any more, the font palette does not take its slot before the attract's palettes, and
those sit one slot lower until frame 690. The one new `0x5D812` miss is the spawned record's.

**The original does the same.** Entry 7's INDEX flags are `0x02`, lazy, and `0x2F9CC` runs before the
first loop iteration, so the raw's first entry-7 resolve is the spawn at `0x2F9FC`. Record
`2026-09-29-e-open-derivations.md` §6 had the original's poll read entry 7 at f 0 and the port at f 3 and
left that residue open; its list of entry-7 handles covers code immediates and misses this one, which is
in data (`0x9AD84 + 0x10`). The residue is settled from the bytes. No DOSBox-X run was made.

### The ruling, and the pins the move shifts

The controller ran the gate on `8c8917fe`: it stopped at `frontend-oracle`, on the `PR_FRONTEND_DUMP`
driver's own pins. That driver enters at state 2 and skips the boot attract, so before the change its
first entry-7 resolve was cycle 2's phase 2 (loop 1973), a loader screen its comment already called "this
driver's, not the port's" and whose removal it predicted to the number (§6's table). Ruling: the move is
accepted; re-measure the pins.

Re-measured from the driver's output (`/tmp/pr_frontend_dump/run1.log`, both determinism runs agree):

| Pin (`test_game.c`) | Before | After |
|---|---|---|
| `fe_cyc2_n` | 2308 | 2307 |
| `c2_s6c_f` | 2193 | 2192 |
| `fe_ld_n` | 13 | 12 |
| `ld_loop` | 1973, 2783 x4, 3557, 3684, 3985 x6 | 2783 x4, 3557, 3684, 3985 x6 |
| `ld_frame` | 168, 979..982, 1757, 1885, 2187..2192 | 978..981, 1756, 1884, 2186..2191 |
| capture 3257's screen (the frame pair read back) | 1756 / 1757 | 1755 / 1756 |

No other pin of the driver moved (`c2_start` 1971, the sound-bank counts, `c2_s6c_i` 3985 and every
loop number are as they were). The comment above the lists now says why the screen is gone.

`make frontend-oracle`, `make demo-fight-oracle` and `make attract2-oracle` against the last green gate
before track I (`/tmp/claude-501/fast_verify.log`, the sections from the front-end oracle to the attract
cycle-2 one): of the 18 `title_compare`/`all checks passed` lines, 16 are byte-identical, the splice-byte
and missing-frame lists included (window distinct `[560..1884]`; 1325 frames, 517 clean, 801 splice, 3
transition, 2 unexplained, the two named frames `(832, 3671)` and `(833, 3740)`; demo-fight 0 unexplained,
the window claim exact; attract2 1732 frames, 1078 clean, 630 splice, 17 transition, 1 unexplained, 6
all-black, 0 unexplained in the region). Two differ, both port-dump numbers:

```
- attract2: … cycle-2 dump 2308 frames
+ attract2: … cycle-2 dump 2307 frames
- attract2: captured frame 3545 (raw 8338) allowed by name as a three-frame splice: cycle-2 2192/2193/2194 at bytes 120000/172800 (rows 125/180)
+ attract2: captured frame 3545 (raw 8338) allowed by name as a three-frame splice: cycle-2 2191/2192/2193 at bytes 120000/172800 (rows 125/180)
```

No capture-side number moved and no capture frame became unexplained. `tools/title_compare.py` needed no
change: it reads the splice's frames from `cycle2/loader.txt`. The `Makefile` comment, `port/spec/game_flow.md`
and a dated note in the e-open record's §6 carry the new numbers.

**A side effect at the restart.** The `PR_RESTART` driver compares colours, not indices, because the
first boot's attract drew the loader and the restart's did not (record named-gaps-b §B.6b). With entry 7
read at boot neither does: the driver also passed with `rd_frame_hash` hashing the indices (tried, not
pinned; its comment says so).

**Not measured** (not the implementer's to run): `make verify`, the K11 and gameplay oracles, the title,
smk and attract-prefix oracles. Where a later gate could stop: the K11 and gameplay drivers install the
same dump hook (`res_set_screen_hook(k11_loader)`, `gp_loader`) after `game_init()`; if either dumped
entry 7's loader screen in its first iterations before, that screen is gone from its dump too. The tail's
`0x2BAF4(1)` (`0x20C58`), which the port leaves out (record named-gaps-b §B.11), is the raw's reset after
the spawn; in the port the spawned record lives until `game_state_init`'s `actors_reset` (`0x10E9C`),
across `hiscore_init`, `config_run_clock` and `game_string_table_load`.

### The `PR_RESTART` assertion

`test_restart_drive` installs a seam hook that runs every body and counts `0x1AFE8` (`config_file_load`),
whose one caller is the master init (`0x2F9CF`; no other `call rel32`, `jmp rel32` or dword in the image),
from before `game_init()` to the end of the fifty iterations after the restart, and asserts the count is 1
(`test_game.c:13826`). The driver's output is unchanged. Mutation: `game_master_init()` also at the head
of `game_init_resume` gives `test_game.c:13826: 3 != 1` (the boot's tail and the restart's) and no other
failure.

### The row

`Spec("game_master_init", 0x2F9CC)`: 4 cases, 1/1 blocks, 36 mutants, mask 0, every callee a recorded
stub. The call set, in the port's argument order:

| Callee | `E.Call` | Clobbers (`E.callee_clobbers`) | Stub |
|---|---|---|---|
| `0x1AFE8` | `()` | `edi`, `ebp` | new (`I_LOAD`) |
| `0x13ADC` | `()` | none | `D4D_EFF_INIT` |
| `0x4F1E4` | `()` | none | `D5A_INPUT` |
| `0x2BAF4` | `("eax",)` | none | `D5A_RESET` |
| `0x4F1D0` | `()` | none | `D5A_ORIGIN` |
| `0x38B18` | `("eax", "edx", "ebx")` | `ebx`, `ecx`, `edx` | `D5A_ROW` |
| `0x2D6F8` | `()` | none | new (`I_VALIDATE`) |
| `0x2D974` | `("eax",)` | none | `D4C_DIP` |
| `0x2C9B8` | `("eax",)` | none | new (`I_GATE`) |
| `0x2C8F0` | `("eax",)` | `esi`, `edi` | `D5I2_VOL` |

The three new addresses are in the pinned dictionary of
`test_each_stub_declares_the_registers_its_callee_clobbers`. Each case seeds the dwords `0x107408..0x107417`
(`0x08080808`, `0xC0C0C0C0`, `0xEDEDEDED`, `0x14141414`), and the memory at every recorded call places the
two stores between the calls.

| Case | Entry registers | `0x2D974` answers | Other stub answers |
|---|---|---|---|
| `m0` | none | `0x12345677` (both low bits) | 0 |
| `m1` | EAX, EBX, ECX, EDX, ESI, EDI, EBP = `0xFFFFFFFF` | `0xA5A5A5FC` (clear) | `0x1AFE8` 1, `0x2D6F8` `0x9C94D2C4`, `0x2C9B8` `0x10000`, `0x2C8F0` `0x12345678`, `0x38B18` `0x10A300`, `0x2BAF4` `0x5A5A5A5A` |
| `m2` | EBX = 5, EDX = 7 | `0x80010001` (bit 0) | `0x2C9B8` `0xFFFFFFFF`, `0x2C8F0` `0xA0` |
| `m3` | EAX = `0x2A` | `0x0000FF02` (bit 1) | `0x1AFE8` `0x100`, `0x13ADC` `0x33333333`, `0x4F1E4` `0x44444444`, `0x4F1D0` `0x55555555` |

`m1` and `m2` show that call 6's EDX and EBX are the function's own zeros. The only stub answer the raw
reads is `0x2D974`'s; every other is varied and changes nothing.

Mutants, with what catches each (measured four times, identical; pinned in `I_KINDS`/`I_CASES`):

| Mutants | Caught by |
|---|---|
| `@noload @noeff @noinput @noreset @noorigin @norow @novalidate @nogate @novolu @novols` (a call dropped) | the call list from the dropped call on (and the memory at the shifted calls) |
| `@swap12 @swap34 @swap56 @gatelate @volswap @fieldearly` (two adjacent calls swapped) | the two calls |
| `@reset0` (`0x2BAF4(0)`), `@desc` (`0x9AD98`), `@a2`, `@a3` (1 for 0), `@field29`, `@gatearg` (−1), `@volu2` (−2 twice), `@vols1` (−1 twice) | the one call's arguments, nothing else |
| `@ptrearly` (the pointer stored before `0x4F1D0`), `@ptrlate` (after `0x2D6F8`), `@storelate` (`0x107410` after `0x2C9B8`) | the memory at a call, nothing else |
| `@noptr @ptrval` (`0x1D2D4`) `@ptrw` (a word store) | the bytes and the memory at calls 6 to 10 |
| `@nofield` (no read, no store), `@nostore`, `@fieldw` (a word store), `@mask8` (`& 0xFC`, the high bytes lost) | the bytes and the memory at calls 8 to 10 |
| `@nomask` | the same, on `m0`, `m2`, `m3` (not `m1`, whose bits are clear) |
| `@mask1` (`& ~1`) | the same, on `m0` and `m3` (bit 1 set) |

The counter, measured: `994/994 functions VERIFIED; 14793/14793 mutants detected; 1 named gaps; 636/825
rows with callees closed (169 have none)` (before: `993/993`, `14757/14757`, `635/824`). The row is
closed: each of its ten callees has a VERIFIED row (`0x1AFE8`'s row runs the original with its file
calls poked out, record track S). No existing row, mask, case or pin moved.

### The unit case `master` (`test_inlined_functions`)

Every callee is stubbed through the call seam (`if_mi_probe`): it records the address, the arguments
and the dwords `0x10740C` and `0x107410` as they are when the call arrives, and answers `0x12345677`
for `0x2D974` and `0x5A5A5A5A` for the others. The two dwords are seeded `0x0C0C0C0C` and `0x10101010`.
Assertions: the count is 11 (`test_game.c:5056`); each of the eleven records equals the table's
(`:5064`: the pointer is still the seed at `0x38B18` and `0x1D2D0` at `0x2D6F8`; the field dword is
still the seed at `0x2D974` and `0x12345674` at `0x2C9B8`); `0x10740C == 0x1D2D0` (`:5066`);
`0x107410 == 0x12345674` (`:5067`). It failed first on the four-step function (`3 != 11` and ten
records). Mutations of `game_master_init`, each built, run with `PR_ORACLE_REQUIRED=1
PR_GAME_DIR=data/game/C ./build/run_tests` and restored:

| # | mutation | failing lines | first differing record |
|---|---|---|---|
| D1 | drop `0x13ADC` | `:5056`, `:5064` | call 1 is `0x4F1E4` |
| D2 | drop `0x4F1E4` | `:5056`, `:5064` | call 2 is `0x2BAF4(1)` |
| D3 | drop `0x2BAF4` | `:5056`, `:5064` | call 3 is `0x4F1D0` |
| D4 | drop `0x4F1D0` | `:5056`, `:5064` | call 4 is `0x38B18` |
| D5 | drop `0x38B18` | `:5056`, `:5064` | call 5 is `0x2D6F8` |
| D6 | drop `0x2C9B8` | `:5056`, `:5064` | call 8 is `0x2C8F0(-1)` |
| D7 | drop `0x2C8F0(-1)` | `:5056`, `:5064` | call 9 is `0x2C8F0(-2)` |
| D8 | drop `0x2C8F0(-2)` | `:5056`, `:5064` | call 10 is missing |
| P1 | drop the `0x10740C` store | `:5064`, `:5066` | call 6 sees the seed |
| P2 | the `0x10740C` store after `0x2D6F8` | `:5064` | call 6 sees the seed |
| P3 | `0x10740C = 0x1D2D4` | `:5064`, `:5066` | call 6 sees `0x1D2D4` |
| F1 | no `& ~3` | `:5064`, `:5067` | call 8 sees `0x12345677` |
| F2 | field `0x29` | `:5064` | call 7 is `0x2D974(0x29)` |
| F3 | drop the `0x107410` store | `:5064`, `:5067` | call 8 sees the seed |
| A1 | `actors_reset_al(0)` | `:5064` | call 3 is `0x2BAF4(0)` |
| A2 | the spawn's `a2 = 1` | `:5064` | call 5 is `0x38B18(0x9AD84, 1, 0)` |
| A3 | `config_voice_gate(-1)` | `:5064` | call 8 is `0x2C9B8(-1)` |
| A4 | the two `0x2C8F0` calls swapped | `:5064` | call 9 is `0x2C8F0(-2)` |

### The final-review fix: the string table before the master init (2026-10-14)

**The audit's finding** (final review A1). The original's boot chain `0x1BEC4` loads the string table
before it calls `0x20C10`: `0x1C0B1 call 0x1d0bc; 0x1C0B6 xor eax,eax; 0x1C0B8 call 0x47370; 0x1C0BD
call 0x20c10` (the tail loads it again at `0x20C7F`). The port loaded it only in `game_init_resume`,
after `game_master_init`. So from `7113e155` to this fix the master init's entry-7 resolve (`0x2F9FC`)
ran `res_load_present` with the string handle `DS_001082DC` still 0 (its image dword):
`game_string_get(0x1E9)` decoded through handle 0 and gave "", `text_blit_string` returned at once (no
glyph, no font-palette flush), and `string_unlock(0)` wrote `mem[0x10..0x13]` and `mem[0x15]`, which the
original never writes. The paragraph "The mechanism" above described the original's draw as the port's.
No dumped frame showed it: `game_state_init`'s `actors_reset` (`0x10E9C`) blanks the screen and resets
the palette list before the first present.

**The fix.** `game_init` calls `game_string_table_load(s_game_dir)` after `sound_buffers_alloc()`
(`0x1C0B1`) and before `game_master_init()` (`0x20C15`), where `0x1C0B8` is. The port's loader is
English-only (`0x47370`'s AL = 0 is index 0, `english.txt`) and returns at once on a second call, so the
tail's call (`0x20C7F`) stays. A probe in `res_load_present` (removed) shows the boot resolve at the frame
word 0 now decodes `- LOADING -`, and `mem[0x10]` and `mem[0x15]` are 0 after it.

**Assertion.** The `PR_RESTART` driver's seam hook keeps `DS_001082DC` as it is at the first `0x1AFE8`
call, the master init's first step, and the driver asserts that it equals the handle after the boot and
that the handle is not 0 (`test_game.c:13931`, `:13932`; the kept value is seeded `0xFEEDFACE`).
Mutation: the new call removed gives `test_game.c:13932: 0 != 58720256` and no other failure.

**Measured.** `--check 820`: the 820 frames (`.ppm`, `.idx`, `.pal`) and `fn_miss.txt` are
byte-identical to task 2's tree (the HEAD `5a30eac8` without the fix, `/tmp/pr_i_frames_t2_820`), and the
stdout too: the boot's draw and its palette flush are wiped by the `0x10E9C` reset before any dump.
Against the pre-track-I baseline the difference is task 2's, unchanged (frames 3..690, `.idx`/`.pal`
only). The `PR_RESTART` driver's output is unchanged. The front-end, demo-fight and attract cycle-2
oracles print what task 2's green gate printed.

## §I.5 `0x1D018` — `sound_shutdown` (`flow.c`)

Callers: `call rel32` at `0x1BE33` only (the first instruction of `0x1BE30`'s body after its three
pushes); no `jmp rel32`, no dword `0x1D018` in the image.

```
01D018  push ebx / push ecx / push edx / push esi
01D01C  cmp byte ptr [0xa2cb1], 0
01D023  je 0x1d0b4
01D029  xor dl, dl
01D02B  mov byte ptr [0xa2cb1], dl
01D031  cmp dword ptr [0x1028c4], 0
01D038  je 0x1d06b
01D03A  mov ebx, dword ptr [0x1028c0]
01D040  push ebx
01D041  call 0x5deed
01D046  add esp, 4
01D049  cmp eax, 4
01D04C  jne 0x1d05d
01D04E  mov ecx, dword ptr [0x1028c0]
01D054  push ecx
01D055  call 0x5deaf
01D05A  add esp, 4
01D05D  xor esi, esi
01D05F  mov dword ptr [0x1028c4], esi
01D065  mov dword ptr [0x1028c0], esi
01D06B  cmp dword ptr [0x1028c8], 0
01D072  je 0x1d0af
01D074  xor esi, esi
01D076  mov eax, dword ptr [esi + 0x102860]
01D07C  push eax
01D07D  call 0x5dd03
01D082  add esp, 4
01D085  cmp eax, 2
01D088  je 0x1d099
01D08A  mov edx, dword ptr [esi + 0x102860]
01D090  push edx
01D091  call 0x5dc8b
01D096  add esp, 4
01D099  add esi, 0x18
01D09C  xor ebx, ebx
01D09E  mov dword ptr [esi + 0x102848], ebx
01D0A4  cmp esi, 0x60
01D0A7  jne 0x1d076
01D0A9  mov dword ptr [0x1028c8], ebx
01D0AF  call 0x5d86a
01D0B4  pop esi / pop edx / pop ecx / pop ebx
01D0B8  ret
```

The brief's reading is confirmed block by block. Notes from the bytes:

- `0x1D09E` writes `[esi + 0x102848]` after `esi += 0x18`: that is `0x102860 + 0x18 * i`, the handle
  dword of the slot just served, zeroed after its status (and stop) and before the next slot's status.
- Both status tests compare the whole EAX (`cmp eax,4` `0x1D049`, `cmp eax,2` `0x1D085`), not AL.
- The handle is re-read for the second call of each arm (`0x1D04E`, `0x1D08A`).
- With `0x1028C4` zero, `0x1028C0` is not touched; `0x5D86A` runs whenever the flag was set, with or
  without either driver.
- The five library calls are cdecl wrappers (`push esi; push edi; push ebp; ... ret`, the caller pops:
  `add esp,4`). Their bodies: `0x5DEED -> 0x6A950` and `0x5DD03 -> 0x67FA0` return `[handle+4]` (0 for
  a NULL handle); `0x5DEAF -> 0x6A8A0` sets `[handle+4] = 2` unless it is 1; `0x5DC8B -> 0x680F0` sets
  `[handle+4] = 2` unless it is 1 or 2; `0x5D86A -> 0x6616A` releases the 16 driver slots at
  `0x108C24` and shuts the library down. `E.callee_clobbers` gives `()` for `0x5DEED`/`0x5DD03` and
  `ebx, ecx, edx` for `0x5DEAF`/`0x5DC8B`/`0x5D86A`.
- **EAX.** On the early exit it is the caller's, else `0x5D86A`'s. The one caller does not read it:
  `0x1BE38 mov edx,[0x101514]; test edx,edx; je 0x1BEAB`, then `0x1BE42 mov eax,edx` or `0x1BEAB mov
  eax,[0x101504]`. The C function is `void`; the row's mask is 0.

**What the port's audio layer holds, and where.** The original's handles are `mem[]` dwords: `0x1CF40`
stores the DIG driver at `0x1028C8` (`0x1CF8E`), each slot's sample handle at `0x102860 + 0x18 * i`
(`0x1CFAC`), the MDI driver at `0x1028C4` (`0x1CFD6`) and the sequence at `0x1028C0` (`0x1CFE8`). The
port's `game_audio_init` stores `1` at `0x1028C8` as a non-zero stand-in for its host DIG object, keeps
the four sample handles in `flow.c`'s `static HSAMPLE s_samples[4]` and the sequence in `static
HSEQUENCE s_sequence` (pointers into `ail.c`'s static pools `g_samples`/`g_sequences`), and writes
neither `0x1028C4` nor `0x1028C0` nor the slots' `+0x00` dwords (the sound-module comment in `flow.c`,
record §22 of `2026-09-29-todo-verify-derivations.md`). The port's `AIL_sequence_status` /
`AIL_sample_status` answer 0 for a NULL or released handle, 2 stopped, 4 playing, like the library's
`[handle+4]` read for the cases `0x1D018` tests (a released original handle is status 1, the port's 0:
either is "not 2", and both libraries' stop does nothing for it). Seams before this task: `0x5DEED`,
`0x5DEAF`, `0x5DD03`, `0x5DC8B` (`ail.c`, no argument: the C3b treatment, record C3b §C3b.5); `0x5D86A`
had none.

**The outcome: 2, a function with named `PORT:` deviations; its row reaches every block.**

1. *Outcome 1 is not honest.* Its condition is that the port's handles are the raw's dwords or that
   a read of them gives the same tests. The four handle arguments are host pointers, not the dwords
   `[0x1028C0]` and `[esi+0x102860]`; and the gate `[0x1028C4]` does not give the original's test in a
   run of the port: `game_audio_init` leaves it 0, so the sequence arm (`0x1D03A..0x1D065`) is never
   taken there, where the original with an MDI driver takes it.
2. *Outcome 2 is.* `sound_shutdown()` follows the raw block by block: the flag test and clear, the
   MDI gate, the status test on the whole EAX, the stop, the two stores in the raw's order, the DIG
   gate, the four-slot loop with the stop when the status is not 2 and the handle dword zeroed after
   each slot's calls, the `0x1028C8` store, then `0x5D86A`. The deviations are one `PORT:` note in the
   function's header: the handle arguments at `0x1D03A`/`0x1D04E` (`s_sequence`) and
   `0x1D076`/`0x1D08A` (`s_samples[i]`), and the dead sequence arm in a run of the port.
3. *Outcome 3 is not needed*: no instruction of `0x1D018` is impossible in C.

**Observable behaviour at the shutdown: unchanged.** The old inline copy called
`AIL_stop_sequence(s_sequence)` unconditionally and then `AIL_shutdown()`. The function, in a run of
the port, skips the sequence arm and polls each slot's status, ending the playing ones, then calls
`AIL_shutdown()`. `AIL_shutdown` releases every sample handle (`AIL_release_sample_handle`:
`mixer_stop_sample`, state 0), stops the sequencer (`seq_stop`) and clears the sequence (state 0): the
same end state as before, with nothing rendered between (the mixer runs only from
`game_audio_service`). `--check 820` after the change gives the same miss lines as task 2's tree
(`actor_spawn hits=7505`, `set_dead hits=7359`, distinct=2) and the same 688/688/0 idx/pal/ppm count
against the pre-task-2 reference `/tmp/pr_i_frames_before_820` that §I.4 records for the master init
alone. The function stops or releases nothing the port does not own (the four handles `game_audio_init`
allocated, and `AIL_shutdown`'s pool, which it already released).

**Port.** `void sound_shutdown(void)` (`flow.c`, declared in `flow.h`), header `/* 0x1D018 — record
2026-10-13-reverse-i §I.5 */`; `game_shutdown` calls it at `0x1BE33`'s place, before the
`config_file_save()` of `0x1BEB5` (order kept). **Seam added:** `PR_SEAM0(0x5D86Au)` as the first
statement of `AIL_shutdown` (`ail.c`); no existing binding calls `AIL_shutdown`, so no existing row
moves. `sound_shutdown` itself has no seam: its one caller `0x1BE30` has no row (a named gap of record
§D.12).

### The row

| row | entry | cases | blocks | mutants | EAX mask | callees |
|---|---|---|---|---|---|---|
| `sound_shutdown` | `0x1D018` | 6 | 13/13 | 32 | 0 | `0x5DEED`, `0x5DEAF`, `0x5DD03`, `0x5DC8B`, `0x5D86A`: recorded stubs, no argument |

Every case seeds the flag byte between `0xA2CB0 = 0xB0` and `0xA2CB2 = 0xB2`, the dword below the slots
(`0x10285C`), the four slot records whole (a handle `0x10C000 + 0x18 * i`, every other byte non-zero),
`0x1028C0 = 0x10B0C0`, `0x1028C4`, `0x1028C8` and `0x1028CC = 0xCCCCCCCC`. The stubs of `0x5DEED`,
`0x5DD03` and `0x5D86A` answer a value per case. The memory at each recorded call places every store.

| case | regs | flag | `0x1028C4` | `0x1028C8` | `0x5DEED` | `0x5DD03` | what it shows |
|---|---|---|---|---|---|---|---|
| `s0` | dirty | `0x00` | set | set | — | — | the early exit (a word test would see `0xB2`) |
| `s1` | — | `0x01` | `0x10B100` | `0x10B200` | 4 | 4 | every stop |
| `s2` | ESI `0x18` | `0x80` | `0x00010000` | `0x00010000` | `0x104` | 2 | whole-EAX `cmp eax,4`; no stop; word gates |
| `s3` | — | `0x01` | 0 | `0x01000000` | — | `0x102` | MDI gate (`0x1028C0` kept); whole-EAX `cmp eax,2` |
| `s4` | dirty | `0x01` | `0x01000000` | 0 | 3 | — | DIG gate (handles kept) |
| `s5` | — | `0xFF` | set | set | 2 | 0 | sequence not stopped; status 0 stops a slot |

| mutant | the wrong port | caught by (cases) |
|---|---|---|
| `@noret` | the early return dropped | byte, calls (`s0`) |
| `@flagw` | the flag tested as a word | byte, calls (`s0`) |
| `@noclear` / `@clearw` | the flag not cleared / cleared as a word | byte, call memory (`s1`-`s5`) |
| `@mdigate` | the MDI gate dropped | calls, memory (`s3`) |
| `@mdiw` | `0x1028C4` tested as a word | calls (`s2`, `s4`) |
| `@seqne` | the sequence status test inverted | calls (`s1`, `s2`, `s4`, `s5`) |
| `@seqal` | AL compared with 4 | calls (`s2`) |
| `@nostopseq` | the `0x5DEAF` stop dropped | calls (`s1`) |
| `@noc4` / `@noc0` | `0x1028C4` / `0x1028C0` not zeroed | byte, call memory (`s1`, `s2`, `s4`, `s5`) |
| `@c4w` / `@c0w` | either stored as a word | byte, call memory (the same) |
| `@c0always` | `0x1028C0` zeroed without the MDI gate | byte, call memory (`s3`) |
| `@diggate` | the DIG gate dropped | calls (`s4`) |
| `@digw` | `0x1028C8` tested as a word | calls (`s2`, `s3`) |
| `@smpeq` | the sample status test inverted | calls (`s1`, `s2`, `s3`, `s5`) |
| `@smpal` | AL compared with 2 | calls (`s3`) |
| `@smp4` | a stop only at status 4 | calls (`s3`, `s5`) |
| `@smpgt` | a stop only above 2 | calls (`s5`) |
| `@nostopsmp` | the `0x5DC8B` stop dropped | calls (`s1`, `s3`, `s5`) |
| `@three` / `@five` | the loop bound 3 / 5 slots | calls, byte (`s1`, `s2`, `s3`, `s5`) |
| `@nohandle` / `@handlew` / `@handle4` | the handle not zeroed / a word / `+0x04` instead | byte, call memory (the same) |
| `@early` | the handle zeroed before its status call | call memory (the same) |
| `@nodig` | `0x1028C8` not zeroed | byte, call memory (the same) |
| `@digfirst` | `0x1028C8` zeroed before the slots (the old inline copy's order) | call memory (the same) |
| `@noshut` | the `0x5D86A` call dropped | calls (`s1`-`s5`) |
| `@shutfirst` | `0x5D86A` before the stores and slots | calls, memory (`s1`-`s5`) |
| `@order` | the slot loop before the sequence arm | calls, memory (`s1`, `s2`, `s5`) |

**What the row does not compare.** The handle each library call takes (pushed by the caller at
`0x1D040`, `0x1D054`, `0x1D07C`, `0x1D090`): the port's seams pass no argument, as in the seven earlier
sound rows (C3b). A port that passed the wrong slot's handle would not be caught by the row; the unit
case does not see it either (its probe records no argument). No block is unreached. The row is open on
its five callees, which have no row (the AIL wrappers are named non-rows, record §D.12).

New stub `0x5D86A: ("ebx", "ecx", "edx")` in the pinned clobbers dictionary. The counter, measured:
`995/995 functions VERIFIED; 14825/14825 mutants detected; 1 named gaps; 636/826 rows with callees closed
(169 have none)` (before: `994/994`, `14793/14793`, `636/825`). No existing row, mask, case or pin moved.

### The unit case `shutdown` (`test_inlined_functions`)

Every library call is stubbed through the call seam (`if_sd_probe`): it records the address and which
of the eight locations `0x1D018` clears are zero when the call arrives (bit 0 the flag, bit 1
`0x1028C4`, bit 2 `0x1028C0`, bits 3..6 the slot handles, bit 7 `0x1028C8`); `0x5DEED` answers the
row's value, the k-th `0x5DD03` the k-th of four. The slot range `0x10285C..0x1028CF` is filled `0x6B`
before each row. Four rows, in order: `open` (flag 1, both drivers, the statuses 4 and `2, 4, 0, 0x102`:
ten calls), `twice` (the call after `open`, the flag as it left it, everything else re-seeded: no call,
nothing stored), `idle` (flag `0x80`, MDI only, status `0x104`: `0x5DEED`, `0x5D86A`, the handles kept),
`nomdi` (DIG `0x100` only, `0x1028C0` set: four status calls, `0x5D86A`, `0x1028C0` kept). Assertions
per row: the call count (`test_game.c:5147`), each call and its zero set (`:5154`), the zero set at the
end (`:5156`), the 88 bytes the function does not store kept (`:5164`), the bytes around the flag kept
(`:5165`). It failed first on the old inline copy moved into `sound_shutdown` (23 failures). Mutations
of `sound_shutdown`, each built, run with `PR_ORACLE_REQUIRED=1 ./build/run_tests` and restored:

| # | mutation | failing lines |
|---|---|---|
| M1 | the early return dropped | `:5147`, `:5156` (`twice`) |
| M2 | the flag not cleared | `:5147`, `:5154`, `:5156` (and `test_flow`'s `:1772`) |
| M3 | the sequence status test inverted | `:5147`, `:5154` |
| M4 | AL compared with 4 | `:5147`, `:5154` (`idle`) |
| M5 | `0x1028C0` zeroed without the MDI gate | `:5154`, `:5156` (`nomdi`) |
| M6 | `0x1028C4` not zeroed | `:5154`, `:5156` |
| M7 | the sample status test inverted | `:5147`, `:5154` |
| M8 | AL compared with 2 | `:5147`, `:5154` (`open`) |
| M9 | three slots | `:5147`, `:5154`, `:5156` |
| M10 | the handle not zeroed | `:5154`, `:5156` |
| M11 | the handle zeroed before its status | `:5154` |
| M12 | `0x1028C8` not zeroed | `:5154`, `:5156` (and `test_flow`'s `:1773`) |
| M13 | `0x5D86A` dropped | `:5147`, `:5154` (and later suites: the handles stay live) |
| M14 | the early exit zeroes `0x1028C8` | `:5156` alone |
| M15 | the slot's `+0x04` zeroed too | `:5164` alone |
| M16 | the flag cleared as a word | `:5165` alone |
| M17 | the DIG gate dropped | `:5147`, `:5154`, `:5156` (`idle`) |
| M18 | the MDI gate dropped | `:5147`, `:5154`, `:5156` (`nomdi`) |
| M19 | `0x1028C8` zeroed before the slots (the old copy's order) | `:5154` |

### An incident during the task (repaired)

A measurement script run from the shared scratchpad imported `bisect`, and the scratchpad held task 2's
`bisect.py`, which shadowed the standard module: it ran task 2's boot bisection (editing `flow.c`,
rebuilding, `rm -rf frames`, `--check 820`, the `PR_RESTART` driver) for ten minutes before it was
killed, which skipped its own restore. It left one line of `game_master_init` commented out
(`/* BISECT */ //actors_reset();`), a build of that tree and a `frames/` dump from it. Repaired: `flow.c`
restored from the task's own copy (the diff was that one line), rebuilt, `frames/` regenerated with
`./build/prageport --game-dir data/game/C --check 820` (the gate's `verify_frames`, rc 0, the miss lines
above), and every later measurement ran from a private directory. Task 2's scratch outputs
`b_check.txt`, `b_diff.txt` and `b_restart.txt` were overwritten by that run.
