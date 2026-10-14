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

**The second caller, `0x20C42`.** It is in the post-`setjmp` tail of `0x20C10`, the port's
`game_init_resume`: `0x20C37 mov [0x104b1d],dl; 0x20C3D call 0x38b70; 0x20C42 call 0x2f920; 0x20C47 xor
eax,eax; 0x20C49 call 0x4f228; 0x20C4E mov eax,1; 0x20C53 mov ebx,0xabcd; 0x20C58 call 0x2baf4`. The
port left out `0x38B70`, `0x2F920` and `0x2BAF4(1)` there as unobservable (record named-gaps-b §B.11:
`0x10E80` runs `0x2BAF4(1)` again at `0x10E9C`, and nothing between the two reads what they reset).
Task 1 wires `actors_table_clear()` at `0x20C42`'s place, between the `0x20C37` store and the `0x4F228`
call; this supersedes §B.11's row for `0x2F920` alone. `0x38B70` and `0x2BAF4(1)` stay out, under a
`PORT:` note at the call. The call is unobservable for the reason §B.11 gives: the grid's readers are
the text-cell functions of `actors.c` (`text_glyph_emit`, the `text_cells_release*` family), none of
which runs between `0x20C42` and `game_state_init`'s `actors_reset`, which clears the grid again. So no
assertion can tell the tail with the call from the tail without it. Measured: the `PR_RESTART` driver's
output is byte-identical before and after (it compares the frames and state words of the boot with
those after a soft restart); `--check` and the oracles were not run by this task.

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
