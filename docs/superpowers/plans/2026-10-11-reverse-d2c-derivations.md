# Track D, wave 2, batch d2c: differential rows for game/flow.c, nameentry.c, attract.c, menu.c (record)

Batch `d2c` of track D wave 2 (brief `.superpowers/sdd/2026-10-11-reverse-d1/brief-wave2.md`, queue
`d2c-queue.txt`, 30 entries). Branch `reverse-d2c`, based on `reverse-d1` at `ad0c70d` (wave 1
merged).

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
  0x201EB 0x2025B 0x202E8 0x20352 0x2036A 0x20487` and ends at the entry `0x1FFD0`. The window's pin
  (`WINDOWS[0x2002E]` in the test, one added entry) equals the image's disassembly.
- The stub-clobber pin of `test_each_stub_declares_the_registers_its_callee_clobbers` gained ONE
  ADDED LINE (no existing line edited), immediately above the dictionary's last line:
  `0x1D1B0: (), 0x2F510: ("ebx", "ecx", "edx"), 0x2F434: ("ebx", "ecx", "edx"),     # track D2 batch d2c`.
  Checked without running the suite: the pinned dictionary equals the stubs of `V.SPECS`, and
  `E.callee_clobbers` on the image returns the declared set for every stub address the D2C rows use.

## §3 Port changes

### §3.1 Correction: `game_state_3_handoff` re-reads the camera dword (commit `130ad07`)

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
`game_state_3_handoff@local` reproduces the old behaviour and is detected on all four cases.

It cannot change the game: `0x2AE14` does not write `0xF0A58`, so the reload returns the record just
stored. It is a fidelity fix that only this stub can observe. `frames/` was regenerated
(`--check 820`) and `run_tests` passed after it; no oracle was run against it.

### §3.2 Seams and exports (no behaviour change)

| address | function | change |
|---|---|---|
| 0x12658 | `game_state_3_handoff` (flow.c) | `static` dropped, declared in flow.h (for the row's binding) |
| 0x1D1B0 | `sound_music_pause_toggle` (flow.c) | `PR_SEAM0(0x1D1B0u)`; `E.Call(0x1D1B0, ())`, clobbers none |
| 0x2F510 | `text_cursor_hold_font2` (actors.c) | `PR_SEAM(0x2F510u, col, row, text_str_dw0(s), mode)`; `E.Call` `("eax","edx","[ebx]","ecx")`, clobbers `ebx ecx edx`. `text_str_dw0` (new static helper, `/* PORT: */`) is the string's first four bytes: the bytes at its mem[] offset when it lies in mem[], else the bytes up to its NUL, zero after it |
| 0x2F434 | `text_number_set` (actors.c) | `PR_SEAM(0x2F434u, col, row, value, width, pad, mode)`; `E.Call` `("eax","edx","ebx","ecx","s0","s1")`, `pop=8` (`0x2F460 ret 8`), clobbers `ebx ecx edx` |

Each seamed function's own row was re-run after its seam and is unchanged: `sound_music_pause_toggle`
(11/11 mutants), `text_cursor_hold_font2` (5/5), `text_number_set` (7/7). No other existing spec
names `0x1D1B0`, `0x2F510` or `0x2F434` (`grep` over `tools/diff_verify.py`), and the hook returns 0
for an address outside the running case's call set.

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
   separate them and would also make any retry loop for ever (one EAX per case).

## §5 Store sweep (checklist items 1-2)

A scratch script hooks every memory write of the original for every case of a row (private stack
excluded) and reports, per store instruction, how often it ran, how often it changed a byte, and
for byte and word stores whether the byte after and the byte before were non-zero in some case.

- **Item 1.** Every store instruction of the nine rows changes a byte in at least one case
  (`game_state_3_handoff` 5 stores, `name_filter` 2 of its own, `sound_pause` 1, `sound_resume` 1,
  `nameentry_cells_step` 46, `flow_side_char_random` 6, `flow_side_char_set` 5, `flow_match_snapshot`
  1, `flow_continue_open` 4).
- **Dropped stores.** Each row carries dropping mutants for its stores (`@cam` `@b1` `@b2` `@a44`;
  `@nul`; `@nostore`; `@noclear`; `@s1acc` `@s1vel` `@s1st` `@s1store` `@s5tgt` `@s5acc` `@s5x`
  `@s6vel` `@s6st` `@s8ch` `@s8nul` `@s8clr` `@s9st`; `@noset` `@char` `@zero` `@b12`; `@zero`;
  `@b1f` `@c04`), all detected. Not every store of `nameentry_cells_step` has its own dropping
  mutant (46 stores, 13 dropping mutants); the others are covered by value or order mutants or by
  the row's byte comparison alone.
- **Item 2, neighbours.** The byte after each byte or word store is non-zero in some case, except
  in `nameentry_cells_step` where the neighbour is a field the function itself has just written
  (`0x2005C`, `0x201B1`, `0x202BE`: the acceleration word, whose next byte is the low byte of the
  target word stored a few instructions earlier, 0 by value). The byte BEFORE is zero in every case
  for `0xFD0F0` (`name_filter`), `0x1028D8` (`sound_pause`, `sound_resume`) and for the velocity and
  target words of `nameentry_cells_step` (the high byte of the y dword below them, 0 for every y a
  case uses): a store one byte too low would still be caught by the byte it leaves unwritten above.
- **Not checked:** the port side's writes beyond the row's byte comparison.

## §6 Queue entries not rowed

(Filled in at the end of the batch.)

## §7 What was run, and what was not

(Filled in at the end of the batch.)
