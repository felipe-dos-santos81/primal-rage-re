# Track D, wave 2, batch d2e: `0x4EF8C` and `0x41C28` (game/flow.c), the two defects d2c reported (record)

Batch `d2e` of track D wave 2 (brief `.superpowers/sdd/2026-10-11-reverse-d1/brief-wave2.md`). It has
no queue file: its members are the two functions batch d2c reported as defective and left unchanged
(record `2026-10-11-reverse-d2c-derivations.md` §3.8), `0x4EF8C` and `0x41C28`. Branch `reverse-d2e`,
based on `reverse-d1` at `ad0c70d` (wave 1 merged; batch d2c is NOT merged here).

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

The mem[] bound: every pointer a binding or mutant follows is a node or record the case seeds in
the image's BSS (`0x10A300..0x10A54F`), the list head `0x10884C`, or a table entry
`0xC95D4`/`0xC9544` + index * 4 with the index below 0x10000 (`@idxs` indexes one dword below
`0xC95D4` in u32 arithmetic). `@rec` follows the node's +4, which every case seeds with the previous
node or the head.

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
  `test_each_stub_declares_the_registers_its_callee_clobbers`: ONE ADDED LINE (no existing line
  edited), immediately above the dictionary's last line:
  `0x4EBB8: (), 0x2F510: ("ebx", "ecx", "edx"),     # track D2 batch d2e`.
  Batch d2c adds its own lines at the same place and also names `0x2F510` (the same value): the
  merge keeps both lines or drops the duplicate key. Checked without the suite: `E.callee_clobbers`
  on the image returns the declared set for every stub of `D2E_SPECS`.

## §3 Port changes

Each correction is its own commit. `frames/` was regenerated (`--check 820`) and `run_tests` passed
after each; NO oracle was run against any of them, so all are unproven against the oracles.

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

### §3.3 Seams (no behaviour change)

| address | function | change |
|---|---|---|
| 0x2F510 | `text_cursor_hold_font2` (actors.c) | `PR_SEAM(0x2F510u, col, row, text_str_dw0(s), mode)` and the static helper `text_str_dw0`: the lines batch d2c adds, byte for byte, so the two branches merge as one change. `E.Call` `("eax","edx","[ebx]","ecx")`, clobbers `ebx ecx edx` |
| 0x4EBB8 | `fight_mode25_scorecard` (fight.c) | `PR_SEAM0(0x4EBB8u)`; `E.Call(0x4EBB8, ())`, clobbers none (it pushes EBX, ECX, EDX, ESI, EDI, EBP and pops them before its one `ret`, `0x4EF84..0x4EF8A`) |

`text_cursor_hold_font2`'s own row was re-run after the seam and is unchanged (5/5 mutants). No
other spec of this branch names `0x2F510` or `0x4EBB8`.

## §4 Named gaps and limits

1. **`game_mode_25_reveal` is open on `0x4EBB8`.** The scorecard (980 bytes, two loops, twelve
   `0x2F174`/`0x2F4D0` call sites) has no row; this row stubs it. The stub writes 0x40 into
   `0x10888C` (the real function stores both tallies, `0x4EF6B mov [eax+0x10888c],dl`), which is
   what lets `@early` be seen; the second tally `0x108891` is the case's.
2. **`game_mode_25_reveal`: one string per id, real lengths.** The two row-5 counts are the real
   `strlen` of the decoded strings 0x3B (7) and 0x3A (5); a count taken from another source that
   happened to give 7 and 5 would agree.

## §5 Store sweep (checklist items 1-2)

A scratch script hooks every memory write of the original for every case (private stack excluded)
and reports, per store instruction of the function's own code, how often it ran, how often it
changed a byte, and whether the byte after and the byte before were non-zero in some case.

- `game_mode_25_reveal`: 6 store instructions (`0x4EF9A`, `0x4EFCF`, `0x4EFD5`, `0x4EFDB`, `0x4EFEF`,
  `0x4EFF5`); each changes a byte in some case and has both neighbours non-zero in some case. Each
  has a dropping mutant (`@no9a` `@n14` `@r34` `@n1c` `@r28` `@n1e`), all detected.

## §6 Not done

(see the end of the batch)

## §7 What was run, and what was not

(see the end of the batch)
