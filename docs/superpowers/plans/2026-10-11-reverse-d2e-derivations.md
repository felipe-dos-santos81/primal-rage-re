# Track D, wave 2, batch d2e: `0x4EF8C`, `0x41C28` and `0x28788` (game/flow.c), defects reported from outside their rows (record)

Batch `d2e` of track D wave 2 (brief `.superpowers/sdd/2026-10-11-reverse-d1/brief-wave2.md`). It has
no queue file: its members are the two functions batch d2c reported as defective and left unchanged
(record `2026-10-11-reverse-d2c-derivations.md` §3.8), `0x4EF8C` and `0x41C28`, and a third the
coordinator added mid-batch from an independent audit, the store at `0x28A8D` in `0x28788`. Branch
`reverse-d2e`, based on `reverse-d1` at `ad0c70d` (wave 1 merged; batch d2c is NOT merged here).

Result: 10 rows over 2 entries (1 for `0x4EF8C`, 9 for `0x41C28`, one per jump-table target, §2.3),
50 cases, 208/208 mutants; five port corrections (§3), four of them defects that change what the
game does; `0x28788` is corrected with a unit regression and not rowed (§4.6).

Every number below was measured on this branch from the image
`./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_d2e_img.bin` (fixups applied) with
`python3 tools/diff_verify.py --image /tmp/pr_d2e_img.bin --function <name> --self-check`, one row at
a time. All addresses are linear (Ghidra) addresses; every instruction quoted is capstone's
disassembly of that image.

What was NOT run here: the full `--self-check`, the unittest suite, `make verify`, any oracle (§7).

## §1 The members, and the callers that fix each EAX mask

Call sites are every `E8`/`E9 rel32` in the code object whose target is the entry; neither entry
appears as a dword in the image.

| addr | port function | size | callers and what they do with EAX | mask |
|---|---|---|---|---|
| 0x4EF8C | `game_mode_25_reveal` (flow.c) | 368 | `0x252E1` (in `0x24C5C`, case 0x25): `0x252E6 inc byte [0x104b25]`, `0x252EC jmp 0x2540f`. Neither reads EAX; `0x2540F` is the mode tail the wave-1 records trace dead for the sites `0x2528C` (d1c §1, `0x28C38`) and `0x253F5` (d1d §1, `0x4F6E8`), which reach it by the same `jmp 0x2540f` | 0 |
| 0x41C28 | `game_mode_12_step` (flow.c) | 2115 | `0x253BD` (in `0x24C5C`, case 0x12): `0x253C2 jmp 0x2540f`, the same tail | 0 |

The trace is the wave-1 liveness script's (d1c record §1.1), with its limits; it was not re-run here.
Read by hand as far as `0x2A31C` (the first call of the tail): `0x2A325 jne 0x2a398` returns with
EAX untouched, the other paths load it (`0x2A353 mov eax,[0x105bcc]`).

## §2 The rows, measured

`tools/diff_verify.py` block `D2E_SPECS` (appended to `D1_SPECS`); bindings `b_d2e_*` / `m_d2e_*` in
`port/tests/diff_runner.c`; pins `D2E_MASKS`, `D2E_KINDS`, `D2E_CASES` in
`tools/tests/test_diff_verify.py`.

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `game_mode_25_reveal` | 0x4EF8C | 5 | 11/11 | 45/45 | 0x0 | `cfb4dfa`; callees `0x2BC30`, `0x2F388`, `0x2F510` (new seam), `0x4EBB8` (new seam) stubs; `0x1C500` with its tree (`0x474E4`, `0x1E75C`, `0x1E808`, `0x500BB`) and the runtime strlen `0x65624` allow; port corrections §3.1, §3.2; open on `0x4EBB8` (§4.1) |
| `game_mode_12_step_s0` | 0x41C28 | 2 | 6/67 (61 named) | 13/13 | 0x0 | `b391201`; state 0; stubs `0x2AE14`, `0x2F198`; `0x1C500` tree allow |
| `game_mode_12_step_s1` | 0x41C28 | 7 | 16/67 (51 named) | 25/25 | 0x0 | state 1; stubs `0x414B4`, `0x2C3FC`, `0x41578` |
| `game_mode_12_step_s2` | 0x41C28 | 2 | 6/67 (61 named) | 10/10 | 0x0 | state 2; stubs `0x2B150`, `0x2F510`; `0x1C500` tree allow |
| `game_mode_12_step_s3` | 0x41C28 | 7 | 10/67 (57 named) | 31/31 | 0x0 | state 3; stubs `0x2AE14`, `0x2A17C`, `0x2BCF4`, `0x2F0F0`, `0x2F198`, `0x2C3FC`; `0x1C500` tree and `0x2F41C` allow; port correction §3.3 |
| `game_mode_12_step_s4` | 0x41C28 | 4 | 5/67 (62 named) | 8/8 | 0x0 | state 4; stubs `0x414B4`, `0x2C3FC` |
| `game_mode_12_step_s5` | 0x41C28 | 5 | 9/67 (58 named) | 13/13 | 0x0 | state 5; stubs `0x41528`, `0x418F4`, `0x416D4`, `0x2C3FC`, `0x4160C`, `0x41760` |
| `game_mode_12_step_s6` | 0x41C28 | 3 | 9/67 (58 named) | 21/21 | 0x0 | state 6; stubs `0x2C3FC`, `0x2BC30` |
| `game_mode_12_step_s7` | 0x41C28 | 9 | 20/67 (47 named) | 36/36 | 0x0 | state 7; stubs `0x2B150`, `0x2AE14`, `0x41310`, `0x2F434`, `0x41578`, `0x4660C`, `0x25848`, `0x41350`, `0x417C4`, `0x271E0`, `0x32A3C`; `0x2DAE4` with `0x2D974` allow (§4.3); port correction §3.4 |
| `game_mode_12_step_s8` | 0x41C28 | 6 | 5/67 (62 named) | 6/6 | 0x0 | state 8 and the guard; no callee |

Totals: 10 rows, 50 cases, 208/208 mutants. The nine `0x41C28` rows hit 67/67 blocks between them
(the two dispatch blocks in every row, the 64 state blocks once each, the shared epilogue `0x42461`
in `_s3`, `_s6`, `_s7`, `_s8`). Each row's official `--function <name> --self-check` was run on
`b391201` (the mutant counts above are its last lines).

### §2.1 The mutants

Binding name `<row>@<suffix>`. What catches each one (kind and exact case set) is pinned in
`D2E_KINDS` and `D2E_CASES`, generated from the measurement (a scratch script runs the row's and
every mutant's port side ten times, requires the ten outputs to be equal, then measures kinds and
case sets once against the original).

- `game_mode_25_reveal`: the countdown word `@w9a` (a byte store) `@v9a` `@no9a` `@late9a` (after the
  walk); the walk `@next` (the link read after the call) `@bit` `@rec`; the index `@idx8` `@idxs`
  `@off48` `@sub`; the five stores, dropped / wrong value / wrong width: `@n14` `@n14v` `@n14w`,
  `@r34` `@r34w`, `@n1c` `@n1cw` `@n1cv`, `@r28` `@r28v` `@r28b`, `@n1e` `@n1ev` `@n1ew`; the
  animation `@tbl` (the old port: table 0xC9544, §3.2) `@speed` `@animrec`; the releases `@norel`
  (the old port: no (-1, 0xB, 2), §3.1) `@relargs` `@relcnt` `@ids` `@row5` `@len`; the scorecard
  `@nocard` `@cardorder`; the compare `@early` (read before the scorecard) `@signed` `@swap` `@off91`;
  the strings `@eqids` `@neid` `@rows` `@mode` `@col`.

- `game_mode_12_step_sN` (binding suffix as listed; the C mutant is `m_d2e_m_sN<suffix>`):
  - `_s0`: `@bit` `@x` (the word sign-extended) `@desc` `@y` `@bc` (no handle store) `@id` `@row`
    `@mode` `@afe` `@afew` (a byte store) `@b25` `@b23` `@order` (the stores before the call)
  - `_s1`: `@start` `@skip` `@afcb` (the stage read as a byte) `@bit80` `@flash` `@nstore` `@post`
    `@r2` `@cls` `@tag` `@chside` `@tally` `@i1` `@b23` `@afe` `@b25` `@c111` `@order` `@end`
    `@dark2` `@f63` `@f63side` `@tafe` `@tb23` `@tb25`
  - `_s2`: `@bit` `@dead` `@id` `@row` `@mode` `@b25` `@w36` `@w36v` `@w36b` `@order`
  - `_s3`: `@zext` `@le` `@y1c` `@w36` `@bit` `@chside` `@afcb` `@half` `@desc` `@y` `@pal` (the old
    port: word 0, handle 0x74, §3.3) `@palgate` `@x50` `@halfw` `@seek` `@noseek` `@ylast` `@xlast`
    `@w2` `@wmode` `@div` (a floor divide) `@col` `@row` `@first` `@lines` `@c10e` `@tally` `@b25`
    `@c111` `@voice` `@order`
  - `_s4`: `@afcb` `@noinc` `@ge8` `@voice` `@afe` `@b23` `@b25` `@order`
  - `_s5`: `@le8` `@signed` `@afcb` `@b23` `@afe` `@b25` `@noboard` `@b1f` `@voice` `@seven` `@tside`
    `@swap` `@order`
  - `_s6`: `@sext` `@floor` `@sub` `@w100side` `@now100` `@cycside` `@bit` `@dec` `@now102` `@vx1c`
    `@vxs` `@vy18` `@vys` `@vswap` `@voice` `@b25` `@b25early` `@anim0` `@speed` `@n` `@tbl`
  - `_s7`: `@sext` (the old port: the word sign-extended, §3.4) `@nowrap` `@dec` `@nostore` `@ge`
    `@dead` `@spawnxy` `@desc` `@pts` `@ptside` `@bit` `@colside` `@valside` `@width` `@pad` `@mode`
    `@row` `@b1d` `@b1f` `@lt9` `@c113` `@col` `@pickorder` `@selside` `@selafc` `@hook` `@ee` `@afe`
    `@b00` `@b3a` `@a7` `@v3c` `@joinorder` `@close` `@b19` `@b19early`
  - `_s8`: `@gt` (the guard as `>= 8`) `@nodec` `@byte` `@unsigned` `@lt` `@copy`

Both mutant cores were checked unmutated against the original before any mutant was measured (a
temporary binding calling `d2e_4ef8c_core(D2E_RV_NONE)` / `d2e_41c28_core(D2E_M_NONE)`, not
committed): VERIFIED on all 50 cases. A core that disagreed with the raw would make every mutant
"detected" for the wrong reason.

Three case sets were changed while measuring because a mutant came out undetected: `_s1@chside`
(both sides had character 5: `b1` now has (5, 2) and a slot byte 0xC2, which also gives the row its
one side-1 tally), `_s6@vys` (no 0x108102 word at or above 0x8000: `g2` now has +0x82 = 0xF4, word
0x9000) and `_s7@selafc` (the 0x25848 stub stored 4, a byte-sized stage: it now stores 0x0104).

The mem[] bound: every pointer a binding or mutant follows is a node or record the case seeds in
the image's BSS (`0x10A300..0x10A54F`), the list head `0x10884C`, or a table entry
`0xC95D4`/`0xC9544` + index * 4 with the index below 0x10000 (`@idxs` indexes one dword below
`0xC95D4` in u32 arithmetic). `@rec` follows the node's +4, which every case seeds with the previous
node or the head. For `0x41C28`: the background record `0x10A300` and the handles the cases seed in
the BSS (`0x10A400..0x10A6FF`, passed to stubs only), a side 0, 1 or 2 (and `side ^ 1`, so slot
bases up to `0x1077B0 + 3 * 0x94`), a character below 7 and a stage word at most 0x0703 (table
entries below `0xC8EC0 + 8`). The division of state 6 is by a step no case or mutant makes zero
(`@sub` subtracts 0x80, not a value that meets a case's word).

`<=` against `<` in the unequal arm (`0x4F0A9 setbe al`) is not a choice a case can separate: the
arm is entered only when the two tallies differ.

### §2.2 Additions outside the D2E blocks

- `tools/diff_emu.py`: `FLAT_SEGMENT_CALLEES = frozenset({0x65624})`, `FLAT_GDT`, `flat_segments(mu)`
  and two lines in `run_original` (`if FLAT_SEGMENT_CALLEES & (set(allow_calls) | {real calls}):
  flat_segments(mu)`). The WATCOM strlen `0x65624` is `push ecx; push edi; mov edi,eax; push es; mov
  eax,ds; mov es,eax; sub ecx,ecx; dec ecx; xor eax,eax; repne scasb; not ecx; dec ecx; pop es; mov
  eax,ecx; pop edi; pop ecx; ret`. Unicorn starts with no descriptor tables; `mov es,eax` at
  `0x6562B` recomputes the stack-size flag from an empty SS cache, and from there every push and pop
  goes through a 16-bit SS:SP. Measured in isolation (ESP = 0x3000000, EAX = a 5-byte string):
  `0x6563B pop ecx` leaves ESP = 0x2FF0000 and the `ret` lands on address 0; in the harness the row
  stopped `unmodeled (int3 at 0x10000)` after running from 0. With a three-entry GDT (null, flat
  code 0x08, flat data 0x10: base 0, limit 0xFFFFF pages, 32-bit) and CS/SS/DS/ES loaded, the same
  run returns EAX = 5 with ESP, ECX and EDI restored. The setup is opt-in by callee address: no
  other spec of this branch or of `reverse-d2c` names `0x65624` (`grep`), so no other row's
  emulator state changes. The GDT sits at `STACK_LOW` (0x7FF00000), the bottom of the private stack
  mapping, 0xF0000 below the stack top. Seven more runtime routines open with the same `push es;
  mov eax,ds; mov es,eax` (`0x619DE 0x61D6F 0x62110 0x65608 0x6C56C 0x6CD59 0x6D3D6`, a byte scan);
  none is in the set: none was needed, and none was tried.
- `tools/tests/test_diff_verify.py`, the stub-clobber pin of
  `test_each_stub_declares_the_registers_its_callee_clobbers`: THREE ADDED LINES (no existing line
  edited), immediately above the dictionary's last line:
  `0x4EBB8: (), 0x2F510: ("ebx", "ecx", "edx"),     # track D2 batch d2e`,
  `0x414B4: (), 0x41528: (), 0x41578: (), 0x418F4: (), 0x416D4: (), 0x4160C: (), 0x41760: (), 0x417C4: (),     # track D2 batch d2e`
  and
  `0x4660C: (), 0x25848: (), 0x41350: ("edx",), 0x271E0: (), 0x32A3C: ("edx",), 0x2F434: ("ebx", "ecx", "edx"),     # track D2 batch d2e`.
  Batch d2c adds its own lines at the same place and also names `0x2F510` and `0x2F434` (the same
  values): the merge keeps all the lines (a repeated key with the same value is harmless) or drops
  the duplicates. Checked without the suite, by a scratch script: the pinned dictionary equals the
  stubs of `V.SPECS`, `E.callee_clobbers` on the image returns the declared set for every stub of
  `D2E_SPECS`, the `WINDOWS` keys equal `RESOLVED_JUMPS`' and every window passes the test's own
  assertions, and the `D2E_MASKS`/`D2E_KINDS`/`D2E_CASES` names equal the specs' rows and mutants
  (10 / 208 / 208).
- The same two files are where a merge with `reverse-d2c` will stop: both batches add at the end
  of the clobber dictionary, of `WINDOWS`, of `RESOLVED_JUMPS` (this batch adds after the closing
  brace, d2c before it) and above `SPECS = [` / `k_bindings` / the `RealFunctionTests` decorators.
  Every conflict is a union of the two sides.

### §2.3 `0x41C28` is nine rows

The harness runs every case of a spec, on both sides, once for the row and once more for each of
its mutants, and one original run costs about 0.23 s here (measured: `game_mode_25_reveal`'s
self-check, 46 bindings x 5 cases, 54 s; the time is the emulator's 64 MB mapping). One spec for
`0x41C28` with these 45 cases and 163 mutants would be 7380 original runs, about 28 minutes, in
every `make diff-verify` and again in the unittest's mutant pass. A state of this function is one
jump-table target that shares only the dispatch (`0x41C28..0x41C61`) with the others, and a mutant
of one state cannot be seen by another state's cases. So the function is nine specs over the one
entry and one binding (`b_d2e_41c28`, `game_mode_12_step()`), `game_mode_12_step_s0` ..
`game_mode_12_step_s8`, each with its own cases and mutants: 1003 original runs, about 4 minutes.

Each row names the blocks of the other eight targets in `unhit_named` (`d2e_m12_unhit`), each with
the row that hits it; the reason is the bytes' own: `0x41C61 jmp cs:[ebx+0x41c04]` takes one target
per call on the byte `0x104B25`. The block lists per target (`D2E_M12_BLOCKS`) were checked against
`static_scan`'s leaders for every row's stop set (67 leaders, equal for all nine). A second
constraint points the same way: `diffrun` takes 16 stubs a case and the function has 24 callees; the
call set is per state (`D2E_M12_CALLS`), at most 11.

This is a decision the integrator can reverse: one spec named `game_mode_12_step` with the
concatenated cases, the union of the call sets cut to 16 or the limit raised, and the mutant
bindings renamed, at the cost above. The counter consequence: nine "functions VERIFIED" for one
function (the tree already has two specs over `0x468D8`).

`RESOLVED_JUMPS[0x41C61] = (0x41C36, 0x41C04, 9)` (`tools/diff_emu.py`, one added statement after
the dictionary): `cmp al,8; ja 0x42461; mov edx,[0x104ad4]; lea ecx,[edx*8]; add ecx,edx; shl ecx,2;
and eax,0xff; add ecx,edx; lea ebx,[eax*4]; shl ecx,2; jmp cs:[ebx+0x41c04]` (`switch_cases` refuses
the slot-offset arithmetic between the guard and the jump). The table holds `0x41C68 0x41CDD
0x41E1B 0x41E6D 0x4208E 0x420E1 0x4217D 0x4226E 0x42443` and ends at the entry `0x41C28`. Pinned
in the test's `WINDOWS` dictionary (one ADDED entry, three lines, after the last entry).

## §3 Port changes

Five corrections, each its own commit: §3.1, §3.2, §3.3 and §3.5 change what the game does; §3.4
is a fidelity fix whose inputs play may not produce. `frames/` was regenerated (`--check 820`) and
`run_tests` passed after each; NO oracle was run against any of them, so all are unproven against
the oracles.

### §3.1 `game_mode_25_reveal` releases two cells at row 0xB (commit `7b35aaf`, DEFECT)

`port/src/game/flow.c`, 0x4EF8C. Old: after the node walk, the two `(-1, 5, strlen)` releases.
New: `text_cells_release_count(-1, 0xB, 2)` before them. `0x2F388` takes EAX = col, EDX = row, EBX =
count; the raw, between the walk's exit (`0x4F018 jne 0x4efa8`) and the first string:

```
4F01A  mov ebx, 2
4F01F  mov edx, 0xb
4F024  mov eax, 0xffffffff
4F029  call 0x2f388
4F02E  mov eax, 0x3b
```

d2c's claim (its record §3.8) holds. Measured by the row on every case before the fix (`e0: call #0:
original 0x2F388(0xFFFFFFFF, 0xB, 0x2), port 0x2F388(0xFFFFFFFF, 0x5, 0x7)`). The mutant
`game_mode_25_reveal@norel` reproduces the old behaviour and is detected on all five cases. In the
game the old port left the two centred cells of text row 0xB unreleased when the reveal starts.
Row 0xB is where mode 0x25's step draws its countdown digit (`0x26954..0x2696E`,
`text_number_draw(-1, 0xB, .., 1, ..)`); that the two released cells are that digit's was not
traced. Reached only in mode 0x25, at the frame `0x24C5C`'s case 0x25 finds `0x1088BD ==
8` with both round flags set.

### §3.2 `game_mode_25_reveal` takes the 0xC95D4 stream table (commit `e0d7b17`, DEFECT)

Same function, the flagged node's animation. Old: `DSD(0x000C9544u + idx * 4u)`. New:
`DSD(0x000C95D4u + idx * 4u)`. Not in d2c's report; found by this row. The raw:

```
4EFEC  sub ecx, 0x20
4EFF3  xor edx, edx
4EFF9  mov dx, cx
4EFFC  mov eax, dword ptr [eax + 8]
4EFFF  mov edx, dword ptr [edx*4 + 0xc95d4]
4F006  push 0x40400000
4F00B  call 0x2bc30
```

The bytes at `0x4EFFF` are `8b 14 95 d4 95 0c 00` in the fixup-applied image and in Ghidra
(`read_memory`). Measured before the fix: `w2: call #0: original 0x2BC30(0x10A400, 0xEE138,
0x40400000), port 0x2BC30(0x10A400, 0xEE02C, 0x40400000)` (index 0: `0xC95D4[0]` = 0xEE138,
`0xC9544[0]` = 0xEE02C). The mutant `game_mode_25_reveal@tbl` reproduces the old table and is
detected on the four cases with a flagged node. The sibling ports of the idiom in fight.c (`0x497FF`,
`0x4B2A1`, `0x4B3D8`, `0x4CB07`, `0x4D213`) already read 0xC95D4. In the game every flagged audience
actor took the stream 0x90 bytes (36 entries) lower in the table area at the reveal.

### §3.3 `game_mode_12_step` passes word 0x74 and no handle to `0x2A17C` (commit `85f0f88`, DEFECT)

`port/src/game/flow.c`, 0x41C28, state 3's sprite arm. Old: `actor_pset_palette(spawned, 0u, 0x74u)`.
New: `actor_pset_palette(spawned, 0x74u, 0u)`. `0x2A17C` takes EAX = rec, EDX = the pset word
(`0x2A192 mov eax,edx` before `0x2A1A4 or eax,edx` / `0x2A1A6 mov [esi+2],ax`) and EBX = the palette
handle (`0x2A1AA test ebx,ebx` / `0x2A1AC je 0x2a1f8`); the raw:

```
41EF1  call 0x2ae14
41EF6  cmp dword ptr [0x104ad4], 0
41EFD  je 0x41f0b
41EFF  mov edx, 0x74
41F04  xor ebx, ebx
41F06  call 0x2a17c
```

d2c's claim (its record §3.8) holds: the defect of its §3.6, a third time. The mutant
`game_mode_12_step_s3@pal` reproduces the old arguments and is detected on `d1` (`call #1:
original 0x2A17C(0x10A600, 0x74, 0x0), port 0x2A17C(0x10A600, 0x0, 0x74)`). The correction was
committed from the bytes before the row existed; the row was never measured against the unfixed
function itself, only against the mutant. In the game: in mode 0x12 (the next-opponent screen
after a won match) with the sprite layout (`0x104529` bit 1) and side 1 the winner, the old port
set the spawned character's pset word to 0 and acquired palette 0x74, where the raw sets the word
to 0x74 and acquires nothing.

### §3.4 `game_mode_12_step` compares state 7's word zero-extended (commit `a03f692`, fidelity)

Same function, state 7. Old: `s32 y = (s32)(s16)DSW(bg + 0x2C) - 0x40; ... if (y > 0x180) break;`.
New: `u32 y = (u32)(u16)(DSW(bg + 0x2C) - 0x40u); ... if (y > 0x180u) break;`. Not in d2c's report;
found by this row. The raw:

```
42273  mov cx, word ptr [eax + 0x2c]
42277  xor edx, edx
42279  sub ecx, 0x40
4227C  mov dx, cx
4227F  mov word ptr [eax + 0x2c], cx
42283  cmp edx, 0x180
42289  jg 0x42461
```

EDX is the 16-bit difference zero-extended. Measured before the fix: `h2` (word 0x0010) and `h3`
(word 0x9000), each `call #0: original none, port 0x2B150(0x10A300)` and four more port calls: the
old port went on to the kill and the spawn where the raw returns. The mutant
`game_mode_12_step_s7@sext` reproduces it (detected on `h2`, `h3`); `@nowrap` (the word
zero-extended but the difference not truncated) is detected on `h2` alone. The two sides differ
only for a word below 0x40 or at 0x8040 and above when state 7 runs; whether play produces one was
not traced.

### §3.5 `game_mode_09_step` resets the loser's +0x80 to 0 (commit `105eefc`, DEFECT)

`port/src/game/flow.c`, 0x28788 (added by the coordinator from an independent audit; verified here
from the bytes). Old: `DSB(DS_00107830 + loser * 0x94u) = (u8)(flag ^ (u8)loser)`. New: `= 0u`. The
raw, in the arm where the winner's slot +0x63 is 1, the loser's +0x80 has just been incremented to 3
or more (`0x28A2E cmp eax,3` / `jl`) and the loser's tally `0x108104[loser]` is below 6:

```
28A40  mov ebx, dword ptr [0x104ad4]         ; the winner
28A46  mov edx, ebx
28A54  shl dl, 6
28A59  or dl, 0x80
28A5C  or dl, byte ptr [eax*4 + 0x10782a]    ; the flag byte
28A65  mov ch, 1
28A67  mov ax, word ptr [0x104afc]
28A6D  xor bl, ch                            ; EBX = the loser
28A6F  mov byte ptr [eax + 0x108106], dl     ; the stage's slot byte = the flag
28A75  mov edx, ebx                          ; EDX reloaded from EBX
28A77  lea eax, [ebx*8]
28A7E  add eax, ebx
28A80  shl eax, 2
28A83  add eax, ebx
28A85  xor dl, bl                            ; DL = BL ^ BL = 0
28A87  mov byte ptr [0x104b17], ch
28A8D  mov byte ptr [eax*4 + 0x107830], dl   ; the loser's +0x80 = 0
```

The audit's reading holds: the flag is dead after `0x28A6F`, and the byte stored is 0. It is the
mis-reading d2c removed from `0x286BC` (its §3.2), which on this branch is unchanged.

The unit test that owns the function (`port/tests/test_fight.c`, the mode-9 block, "arm B") pinned
the old value (`CHECK_EQ_INT(DSB(DS_00107830 + 0x94), flag ^ 1)`); it now expects 0. The byte is
seeded 2 and is 3 after the increment, so a dropped store fails it too. With the fix reverted:
`FAIL .../port/tests/test_fight.c:30900: 133 != 0` (133 = 0x85 = (0x80 | 4) ^ 1), `FAILURES: 1`.

Which situation reaches the store: mode 9's results state (`0xF0AFE == 4` and `0x1078FC` set) with
a decided match (`0x104AD4` 0 or 1) whose winner's slot +0x63 is 1. That byte is set at `0x41385`
in `0x41350`, the function that gives a side the stage's character (the game's own opponent), and
`0x418F4` skips such a side on the scoreboard: read as "the computer's side won" (the writers at
`0x246C6`, `0x40DF0`, `0x4928F`, `0x15B73`, `0x4611F`, `0x40FA3`, `0x24954`, `0x459D0` were not
read). The loser's +0x80 counts such losses (`0x28A19..0x28A21`); at the third, with the loser's
tally below 6, the raw flags the stage's slot, sets `0x104B17 = 1` and clears the counter. The old
port left 0x80 or more in it, so every later loss of that kind met `>= 3` at once (until the byte
wraps or `0x33C18` clears it). What `0x104B17 = 1` then makes `0x25848` do was not traced.

`0x28788` is not rowed (§4.6). The rest of the function was read against the bytes, block by
block, and agrees with the port (the hold word, the five stores of `0x2886A..0x28898`, the draw
arm, the four `0x32BAC` no-op calls the port omits, the three closes, the `0x4142C` hook arm): a
reading, not a measurement.

### §3.6 Seams (no behaviour change)

| address | function | change |
|---|---|---|
| 0x2F510 | `text_cursor_hold_font2` (actors.c) | `PR_SEAM(0x2F510u, col, row, text_str_dw0(s), mode)` and the static helper `text_str_dw0`: the lines batch d2c adds, byte for byte, so the two branches merge as one change. `E.Call` `("eax","edx","[ebx]","ecx")`, clobbers `ebx ecx edx` |
| 0x4EBB8 | `fight_mode25_scorecard` (fight.c) | `PR_SEAM0(0x4EBB8u)`; `E.Call(0x4EBB8, ())`, clobbers none (it pushes EBX, ECX, EDX, ESI, EDI, EBP and pops them before its one `ret`, `0x4EF84..0x4EF8A`) |

| 0x41578 | `frontend_darken_marked` (flow.c) | `PR_SEAM0`; `E.Call(0x41578, ())`, clobbers none |
| 0x414B4 | `frontend_portrait_flash` (flow.c) | `PR_SEAM(0x414B4u, i)`; `("eax",)`, clobbers none |
| 0x41528 | `frontend_portrait_reveal` (flow.c) | `PR_SEAM(0x41528u, slot)`; `("eax",)`, clobbers none |
| 0x4660C | `flow_1082d0_from_column` (flow.c) | `PR_SEAM(0x4660Cu, col)`; `("eax",)` (`0x4660F mov edx,eax`), clobbers none |
| 0x4160C | `frontend_mode12_advance` (flow.c) | `PR_SEAM0`; clobbers none |
| 0x416D4 | `frontend_mode12_darken_or_tally` (flow.c, static) | `PR_SEAM0`; clobbers none |
| 0x41760 | `frontend_mode12_challenge_continue` (flow.c, static) | `PR_SEAM0`; clobbers none |
| 0x417C4 | `flow_no_continue_screen` (flow.c) | `PR_SEAM0`; clobbers none |
| 0x418F4 | `frontend_scoreboard_build` (flow.c, static) | `PR_SEAM0`; clobbers none. d2c drops this function's `static` two lines above and corrects lines below; the seam is the first statement |
| 0x271E0 | `flow_join_prompt_draw` (flow.c, static) | `PR_SEAM0`; clobbers none |
| 0x25848 | `flow_stage_pick` (flow.c) | `PR_SEAM0`; clobbers none |
| 0x41350 | `fight_char_select` (fight.c) | `PR_SEAM(0x41350u, side, char_index)`; `("eax","edx")` (`0x41352 mov ebx,eax`, `0x41364 mov ax,dx`), clobbers `edx` |
| 0x2F434 | `text_number_set` (actors.c) | `PR_SEAM(0x2F434u, col, row, value, width, pad, mode)`: batch d2c's line, byte for byte; `("eax","edx","ebx","ecx","s0","s1")`, `pop=8` (`ret 8`), clobbers `ebx ecx edx` |
| 0x32A3C | `config_play_time_close` (config.c) | `PR_SEAM(0x32A3Cu, mode, flag)`; `("eax","edx")` (`0x32A3F mov ecx,edx`, `0x32A41 mov edx,eax`), clobbers `edx` |

Every `ret` of the fourteen targets of this table's second part was listed from `static_scan`:
plain `ret`, except `0x2F434`'s `ret 8`. Commits `cd353e2` (the first two) and `011ccd0` (the rest).

The rows of the seamed functions that have one were re-run after their seam and are unchanged:
`text_cursor_hold_font2` 5/5 mutants, `frontend_portrait_flash` 12/12, `frontend_portrait_reveal`
8/8, `flow_1082d0_from_column` 7/7, `frontend_mode12_advance` 16/16, `flow_no_continue_screen`
12/12, `flow_stage_pick` 16/16, `text_number_set` 7/7, `config_play_time_close` 3/3. No other spec
of this branch names any of the sixteen addresses as a callee (`grep`), and the hook returns 0 for
an address outside the running case's call set.

The four static callees are not exported: the mutant core reaches them through `d2e_seam0(addr)`
(`PR_SEAM0` alone, in `diff_runner.c`), which records the call when a case stubs the address and
does nothing otherwise. Every case of the nine rows stubs them.

## §4 Named gaps and limits

1. **`game_mode_25_reveal` is open on `0x4EBB8`.** The scorecard (980 bytes, two loops, twelve
   `0x2F174`/`0x2F4D0` call sites) has no row; this row stubs it. The stub writes 0x40 into
   `0x10888C` (the real function stores both tallies, `0x4EF6B mov [eax+0x10888c],dl`), which is
   what lets `@early` be seen; the second tally `0x108891` is the case's.
2. **`game_mode_25_reveal`: one string per id, real lengths.** The two row-5 counts are the real
   `strlen` of the decoded strings 0x3B (7) and 0x3A (5); a count taken from another source that
   happened to give 7 and 5 would agree.
3. **`game_mode_12_step_s7`: the audit add is exercised only as a no-op.** The join arm ends in
   `0x42420 mov eax,0x10` / `0x42425 call 0x2dae4`, which the port defers under a `/* PORT: */`
   comment (no C function, so no seam). The two cases that reach it (`h7`, `h8`) run `0x2DAE4` and
   its field read `0x2D974` real on the original side and make them write nothing, by their own
   bytes: field 0x10's descriptor `0x2D340` is poked to a full dword (0x1C000) and the dword
   `0x105DE1` to 0x80000000, so `0x2D974` returns a negative value and `0x2DAF0 jl 0x2db54`
   returns. The row therefore claims the arm's other effects (the 0x3C store into `0x10780B` or
   `0x10789F`, `0x271E0`, `0x32A3C(0x104ABC, the byte 0x104B19)`, `0x104B19 = 0`) and nothing about
   the audit add; with a non-negative field the two sides differ by design.
4. **`0x41C28`: one EAX per stub per case.** Every spawn of a case returns the same record, so in
   state 3 which spawn `0x2A17C` and `0x2BCF4` are given is seen only by their place in the call
   list; the three `0x2F0F0` widths of a text case are equal (5 in `d3`, -1 in `d4`: the sum -3
   separates the truncating divide from a floor).
5. **`0x41C28`: the side is read once.** The port reads `0x104AD4` at entry (and again at
   `0x41D30`/`0x41DC2`/`0x41EF6`); the raw reloads it after calls (`0x42049`, `0x42117`, `0x422BC`,
   `0x422C6`, `0x422F3`/`0x4230D`, `0x42376`, `0x42397`). No stub writes it, so the rows cannot see
   the difference; no callee was checked for a write of it. Likewise `0x4239C` reads the stage word
   after `0x25848` (the stub writes it, and `_s7@pickorder` is detected), but the `0x4660C` argument
   and the four stores after `0x41350` are compared only at their values.
6. **`0x28788` is not rowed.** Nothing in its bytes blocks a row: no host-owned callee (the four
   `0x32BAC` calls land on a bare `ret` and can run real), and its sixteen other direct callees
   (`0x3C5CC 0x16D58 0x17FA0 0x17580 0x1958C 0x3CB68 0x35658 0x19068 0x49C78 0x12DA8 0x4A708
   0x28130 0x286BC 0x32B00 0x32A3C 0x4246C`) just fit `diffrun`'s sixteen stubs. It needs a seam on
   most of them, `0x286BC` is static here and is being exported and corrected on `reverse-d2c`, and
   the function is the size of `0x41C28`'s largest states together. It was not attempted in this
   batch: the correction of §3.5 rests on the bytes and the unit regression.

## §5 Store sweep (checklist items 1-2)

A scratch script hooks every memory write of the original for every case (private stack excluded)
and reports, per store instruction of the function's own code, how often it ran, how often it
changed a byte, and whether the byte after and the byte before were non-zero in some case.

- `game_mode_25_reveal`: 6 store instructions (`0x4EF9A`, `0x4EFCF`, `0x4EFD5`, `0x4EFDB`, `0x4EFEF`,
  `0x4EFF5`); each changes a byte in some case and has both neighbours non-zero in some case. Each
  has a dropping mutant (`@no9a` `@n14` `@r34` `@n1c` `@r28` `@n1e`), all detected.
- `0x41C28`: 44 store instructions over the nine rows (4, 10, 2, 6, 4, 3, 5, 8, 2); each changes a
  byte in some case. The first sweep found four byte or word stores with a zero neighbour in every
  case (`0x104AFD` before the countdown word in states 0, 1 and 8: the stage word's high byte;
  `0x108113` after `0x108112` in state 1; `0x104B02` after the mode word in state 7); cases `a0`,
  `b0`, `b5`, `i0` and the fixture were changed to seed them and all nine rows re-measured. One is
  left: the byte before `0x423CA mov [0x104b00],ax` is `0x104AFF`, which the function has just
  written 0 itself (`0x423B6`, the 0x78 countdown). Dropping mutants: `_s0@bc` `@b25` `@b23`,
  `_s1@nstore` `@b25` `@c111` `@tb25`, `_s2@w36`, `_s3@y1c` `@w36` `@c10e` `@c111`, `_s4@noinc`
  `@b25`, `_s5@b25`, `_s6@now100` `@now102`, `_s7@nostore` `@ee` `@b19`, `_s8@nodec`; the other
  stores are covered by value, width or order mutants.
- **Not checked:** the port side's writes beyond the rows' byte comparison.

## §6 Not done

| addr | size | port | why |
|---|---|---|---|
| 0x28788 | 1099 | `game_mode_09_step` (flow.c) | corrected (§3.5), not rowed (§4.6) |
| 0x4EBB8 | 980 | `fight_mode25_scorecard` (fight.c) | seamed and stubbed; no row (§4.1). Not small |
| 0x41350 | - | `fight_char_select` (fight.c) | item C of the batch, not started. Seamed and stubbed by `_s7` |
| 0x41578 | - | `frontend_darken_marked` (flow.c) | item C, not started. Seamed and stubbed by `_s1`, `_s7` |
| 0x416D4 | - | `frontend_mode12_darken_or_tally` (flow.c) | item C, not started. Seamed and stubbed by `_s5` |
| 0x41760 | - | `frontend_mode12_challenge_continue` (flow.c) | item C, not started. Seamed and stubbed by `_s5` |
| 0x271E0 | 115 | `flow_join_prompt_draw` (flow.c) | item C, not started. Seamed and stubbed by `_s7`. Its bytes were read once against the port and agree (four byte stores, `0x4660C(4)`, the stage word 7, `0x2716C(side ^ 1)`, the hook `0x27134`, the three words): a reading, not a measurement |

So the ten rows stay open on these callees and on `0x418F4` (rowed on `reverse-d2c`).

## §7 What was run, and what was not

Run on `b391201` (the last row commit) and again on `105eefc` (the last commit that changes
`port/src`): the build; each of the ten rows' `--function <name> --self-check`, one at a time; the
pin measurement of every row (ten port-side runs per binding, equal); the static checks of §2.2;
`frames/` regenerated with `./build/prageport --game-dir data/game/C --check 820` after each of
the five corrections (last line each time: `fn-miss check distinct=2 dropped=0`); and the gates
quoted in the final report (`run_tests`, `entry-triage`, `port_progress`).

Not run (the integrator's): the full `--self-check` (no counter line for this branch), the
unittest suite (so the `D2E_*` pins, the `WINDOWS` entry and the three clobber-dictionary lines
were checked only by the scratch scripts described above, not by the tests that own them), `make
verify`, `make diff-verify`, every `*-oracle`. The counter test is expected to fail on this branch
until re-pinned. None of the five corrections was run against a capture oracle. The two mask-0
claims rest on the wave-1 liveness trace of the shared mode tail `0x2540F` (§1), not on a re-run
of it.
