# Track H — harden the weak rows with a machine model: derivation record

Brief: `.superpowers/sdd/2026-10-14-reverse-h/brief.md`. Base: `main` `0a4a30d5` (`998/998 functions VERIFIED;
14840/14840 mutants detected; 1 named gaps; 638/829 rows with callees closed (169 have none)`).

## §H.1 Batch h0: the mechanism, and 0x1C470 as its proof

### What the bytes issue

**`int 0x16`.** Ghidra's instruction search (project `rage`) and a raw scan of the code object for `CD 16` agree
on six sites below `0x5D000`. The raw scan's other two hits are not `int 0x16`: `0x2EAD2` lies inside `call
0x501a3` (`E8 CD 16 02 00` at `0x2EAD1`) and `0x6BAF0` is the runtime's `int86` dispatch table (`int 0x16; ret`,
above `0x5D000`). Every site is the inlined WATCOM `_bios_keybrd`: `mov dh,ah; int 0x16; jne +; and dh,0xf;
dec dh; jne +; sub eax,eax`, so for AH = 0 the result is AX whatever ZF is (`dec dh` of 0 is not zero), and for
AH = 1 a set ZF gives 0.

| site | function | AH | set at | what the code keeps |
|---|---|---|---|---|
| `0x24A64` | `0x249F0` | 0 | `0x24A60 xor ah,ah` | `and eax,0xffff` `0x24A71`, then AL (`and eax,0xff` `0x24A76`) |
| `0x24D0E` | `0x24C5C` | 1 | `0x24D08 mov ah,1` | ZF, then AX (`mov bx,ax` `0x24D1B`, EBX = 0 at `0x24D0A`) |
| `0x24D2E` | `0x24C5C` | 0 | `0x24D26 xor eax,eax; mov ah,al` | AX (`mov bx,ax` `0x24D3B`) |
| `0x24E54` | `0x24C5C` | 0 | `0x24E50 xor ah,ah` | AL (`mov ebx,eax; xor eax,eax; mov al,bl` `0x24E61`) |
| `0x2EB15` | `0x2EA78` (and `0x2EA74`) | 1 | `0x2EB11 mov ah,1` | ZF, then AX (`and eax,0xffff` `0x2EB22`) |
| `0x2EB2D` | `0x2EA78` (and `0x2EA74`) | BL | `0x2EB29 mov ah,bl`; BL = 0 from `0x2EAEE xor ebx,ebx` | AX (`xor edx,edx; mov dx,ax` `0x2EB3A`) |

No site issues AH = 0x10 or 0x11. No site reads EAX above AX, another register the BIOS could change, or a flag
other than ZF after AH = 1.

**`in` / `out`.** Ghidra's search for the mnemonics `IN`, `OUT` (no `INS`/`OUTS` exist), cross-checked by a
static scan (switches followed) from every entry of `port/decomp/prage.functions.csv` below `0x5D000`; the scan
also misses the two sites Ghidra lists outside a function (`0x1B9C4`, `0x1BAE4`), so the table is their union.
Every one is a byte form (`EC`, `E4`, `EE`, `E6`).

| port | instruction | sites | the port fixed by |
|---|---|---|---|
| `0x3DA` (VGA input status 1) | `in al,dx` | `0x1C486` (`0x1C470`), `0x1C7CA` (`0x1C740`), `0x5212E` (`0x52106`) | `mov edx,0x3da` at `0x1C481`, `0x1C7C3`, `0x52129` |
| `0x3C8` (DAC write index) | `out dx,al` | `0x1C4AA` (`0x1C470`), `0x5213F` (`0x52106`) | `mov edx,0x3c8` at `0x1C4A2`, `0x52138` |
| `0x3C9` (DAC data) | `out dx,al` | `0x1C4CA`, `0x1C4CE`, `0x1C4D5` (`0x1C470`); `0x52145`, `0x52146`, `0x52147` (`0x52106`) | `mov edx,0x3c9` at `0x1C4C0`, `0x52140` |
| `0x201` (game port) | `in al,dx` | `0x1B899` (`0x1B890`), `0x1B8E5` (`0x1B8DC`), `0x1B911` (`0x1B908`), `0x1B9C4`, `0x1BAE4`, `0x4FBCA`, `0x4FBE7` (`0x4FBBB`), `0x4FC16`, `0x4FC42` (`0x4FC05`), `0x4FD0E`, `0x4FD3A` (`0x4FCFD`), `0x4FE06`, `0x4FE32` (`0x4FDF5`) | `mov edx,0x201` at `0x1B892`, `0x1B8DE`, `0x1B90A`, `0x1B9BB`, `0x1BADB`, `0x4FBC0`, `0x4FC0C`, `0x4FD04`, `0x4FDFC` |
| `0x201` | `out dx,al` | `0x4FBDC`, `0x4FC3F`, `0x4FD37`, `0x4FE2F` | the same `mov edx,0x201` (no write to EDX in between) |
| `0x43` (PIT mode) | `out 0x43,al` | `0x4FC29`, `0x4FC51`, `0x4FC74`, `0x4FD21`, `0x4FD49`, `0x4FD6C`, `0x4FE19`, `0x4FE41`, `0x4FE64`, `0x4FE87`, `0x4FEAA`, `0x5002C` | immediate |
| `0x40` (PIT counter 0) | `in al,0x40` | two after each `out 0x43` above: `0x4FC2D`/`33`, `0x4FC55`/`5B`, `0x4FC78`/`7E`, `0x4FD25`/`2B`, `0x4FD4D`/`53`, `0x4FD70`/`76`, `0x4FE1D`/`23`, `0x4FE45`/`4B`, `0x4FE68`/`6E`, `0x4FE8B`/`91`, `0x4FEAE`/`B4`, `0x50030`/`36` | immediate |

The joystick and timer functions (`0x4FC05`, `0x4FCFD`, `0x4FDF5`, `0x50021`) also run `cli`/`sti`, which the
model does not take on (it stays an `unmodeled` stop). Of the rows in the brief's weaker list, only `0x1C470`,
`0x52106` and `0x1C740` (the Smacker loop, out of scope) reach a port.

### The model (`tools/diff_emu.py`, commits `489d8a41`, `10a5e887`)

`E.Machine(keys=None, ins=(), outs=(), aperture=False)`, given per case as `Case(..., machine=...)` and passed to
`run_original(..., machine=)`. A case that gives none runs exactly as before: no hook changes, the same stops
(the full self-check printed byte-identical output on `8deafa2f` and `10a5e887`, below).

| part | the original's side | the port's side (`diffrun`) | compared as |
|---|---|---|---|
| `keys` (AX words; `()` an empty queue) | `int 0x16`: AH = 0 removes the first word into AX, an empty queue stops the run (the BIOS would wait); AH = 1 sets ZF = 1 when empty, else ZF = 0 and AX = the first word, kept; any other AH stops. EAX bits 16+, the other registers and flags kept (the table above: no site reads them) | `kbd` and one `key` line a word: `input_clear()` then `input_push(word >> 8, word & 0xFF)` before the binding; after it the FIFO is drained and counted, `k <n>` | the words left (`keys left:`) |
| `ins` (`((port, (byte, ...)), ...)`) | `in al,dx` / `in al,imm8` from a listed port returns its next byte; past the last, or from an unlisted port, the run stops | nothing: the port has no `in` to answer | (an input only) |
| `outs` (ports) | each `out dx,al` / `out imm8,al` to a listed port is recorded as (port, AL), in order; another port stops the run | `outs` line: `pr_out` is installed for the case; each `PR_OUT(port, value)` the port makes is an `o` line | `E.dac_effects` of both streams, element by element (`out #i:`) |
| `aperture` | the bytes changed in `[0xA0000, 0xAFA00)` (the 320x200 the game writes at the literal `0xA0000`) leave `writes` and the call memory for `OrigResult.aperture` | `aperture` line: `gfx_aperture()` is seeded from `mem[0xA0000..0xAFA00)` after the pokes, and each byte of it changed by the run is an `a` line | byte by byte (`aperture 0x...:`) |

Only the byte forms of `in`/`out` are modelled (the word and dword forms, and `ins`/`outs`, stay unmodeled; the
game below `0x5D000` has none). The emulator advances EIP past a modelled instruction from its code hook, as a
stub returns.

**The DAC's view.** The original sets the DAC index once a record and lets the device advance it; the port
stores entry by entry. Comparing the raw `out` order would compare an implementation detail no screen shows, and
would not check the port's index arithmetic at all (`gfx_dac[(u8)(first + i)]`). `E.dac_effects` is the VGA DAC
(mode 13h): `out 0x3C8` sets the write index and restarts the channel count; each `out 0x3C9` latches the low six
bits of a channel (the DAC registers are six bits wide; the port's own comment at `0x1C470` and the
`(v << 2) | (v >> 4)` expansion rest on the same fact); the third writes `("dac", index, r, g, b)` and advances
the index modulo 256. The stream ends with `("dac-at", index, channels latched)`, the state a later write would
continue from. Every other port passes through as `("out", port, value)` in place. So the port reports each entry
with `PR_OUT(0x3C8, index)` and its three channels, and two streams agree when they write the same entries in the
same order and leave the DAC in the same state.

**`PR_OUT`** (`port/src/mem.h`, `mem.c`): `pr_out` is a function pointer, NULL in every binary but
`build/diffrun`, which sets it only for a case with `outs` and clears it after the case (the `PR_SEAM` pattern);
it adds no behaviour to the game.

**What the model does not claim.** The `in` bytes are the case's, not a timing model: a row claims the code's
answer to those bytes (for `0x3DA` the code keeps bit 3 alone, so any byte stands for the hardware's). Whether
the port waits for the retrace (`gfx_wait_vblank`, a `PORT:`) is not compared. The call memory (`m` lines and
`call_mem`) does not include the screen. With `aperture`, a genuine data write of the function into
`[0xA0000, 0xAFA00)` (the data object aliases that range in the flat image) would be counted as a screen byte:
it cannot pass silently, since the port writes such data to `mem[]` and the two sides then differ.

**Harness tests** (each fails without the mechanism, and under the mutations listed): `H0MachineTests` in
`tools/tests/test_diff_emu.py` (ZF inverted, EAX's upper half cleared, a peek that consumes, the aperture kept
in `writes`, the call memory with the screen, the six-bit mask, the index advance, the channel restart, the
final `dac-at`: each turned a test red); `H0MachineTextTests` and `H0DiffrunMachineTests` in
`tools/tests/test_diff_verify.py` (no `input_push`, the seed taken before the pokes, the screen not seeded from
the case, the first screen bytes not reported: each turned a test red).

### The proof row: `0x1C470` `gfx_flush_palette` (commit `4dfcd7e6`)

| | before (d6a) | after |
|---|---|---|
| code pokes | 5: `0x1C481` `mov al,8` + 4 nops (the retrace read), `0x1C4AA`, `0x1C4CA`, `0x1C4CE`, `0x1C4D5` nops (the DAC writes) | none |
| machine | none | `ins` `0x3DA`, `outs` `0x3C8`/`0x3C9` |
| cases | f0..f3 | f0..f3, same records; the retrace bytes f0 `(8,)` (not read: an empty list), f1 `(0xF7, 0x37, 0x0C)`, f2 `(0x08,)`, f3 `(0xFF,)` |
| blocks | 11/11 | 11/11 (the back edge `je 0x1c486` `0x1C489`, never taken under the poke, is taken twice in f1) |
| compared | the list's bookkeeping | the bookkeeping and every DAC entry: index, count, the colour dword's three shifts, the handle's resolution through `0x1B544` and its count dword's skip |
| mutants | 10 | 16 |
| verdict | VERIFIED | VERIFIED |

The retrace read is exact: the code keeps bit 3 of the byte alone (`test al,8; je 0x1c486` `0x1C487`; AL is
reloaded from `[ebx+4]` at `0x1C4A7`). The DAC writes are exact for mode 13h (above): index `AL = [ebx+4]` at
`0x1C4AA`; per colour `mov eax,[ecx]; shr eax,2` then `shr eax,8` twice, so the channels are the dword's bits
2..7, 10..15, 18..23 (`0x1C4C5..0x1C4D5`); `dec esi; jg 0x1c4c5` makes it a do-while (`0x1C4D6`).

**Port side.** `gfx_flush_palette` reports each entry it stores (`PR_OUT(0x3C8, index)`, then r, g, b) under a
`PORT:` note; no other change. **No correction**: before the `PR_OUT`s were in, the row was a MISMATCH only by
the missing stream (`f1: out #0: original dac[0x10] = 0x14 0x2A 0x3F, port none (6 and 0 effects)`, and the
same in f2 and f3); with them, VERIFIED at the first run.

**Mutants.** The binding core `d6a_1c470_core` now also restates the DAC writes (`d6a_1c470_dac`), so the old
mutants differ only in what they break. They are caught in the same cases as before; four of them now also by
`out`, each for a reason in the bytes: `@clampby` and `@clampcount` leave another low byte in `+0xC`, which the
handle test `0x1C4AD` reads after the clamp; `@headfirst` and `@one` send fewer records. The six new ones, each
the old code's possible error:

| mutant | what it breaks | caught by, in |
|---|---|---|
| `@shift` | the colour dword not shifted right by 2 | `out`: f1 f2 f3 |
| `@idx` | every entry at the record's first index | `out`: f1 f2 f3 |
| `@while` | `while` for the do-while (a count below 1 writes none) | `out`: f1 |
| `@noskip` | the handle's data read from its count dword | `out`: f2 f3 |
| `@noresolve` | the handle used as a colour pointer | `out`: f2 f3 |
| `@chan` | red and blue swapped | `out`: f1 f2 f3 |

**The row leaves the weaker-row list**: no poke, no stub standing in for hardware (`0x1B544` runs on both
sides, as before), no named unhit block. Its claim stays narrow: the four cases' inputs and the retrace bytes
given.

### Measurements

- Full `--self-check` before the conversion (on `8deafa2f` and on `10a5e887`): byte-identical to `main`'s
  (`cmp` of the whole output), `998/998 functions VERIFIED; 14840/14840 mutants detected; 1 named gaps; 638/829
  rows with callees closed (169 have none)`.
- After the conversion (`4dfcd7e6`): `998/998 functions VERIFIED; 14846/14846 mutants detected; 1 named gaps;
  638/829 rows with callees closed (169 have none)`; the only other difference in the output is the six new
  mutant rows of `gfx_flush_palette`.
- Harness suite (`python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify`): 192 tests OK. `PR_ORACLE_REQUIRED=1 ./build/run_tests`:
  all checks passed.

## §H.2 Batch h1: the keyboard rows

The rows that poked `int 0x16` out now take the key queue of the machine model (§H.1: AH = 0 removes the first
word into AX, an empty queue stops the run; AH = 1 sets ZF and, with a key, AX = the first word; flags and
the other registers kept). Each case gives its words (`Case.machine = E.Machine(keys=...)`, `()` an empty
queue); `h0_start` loads the same words into the port's FIFO (`input_push(word >> 8, word & 0xFF)`), so the
bindings no longer touch the queue (their `input_clear()`/`input_push()` calls are gone), and the words left
are compared. `game_loop` (`0x255CC`) is not converted: its two pokes replace `call 0x5d7dc` at `0x256B1` and
`0x256D6` (the project's master-loop pins, `tools/title_pin.py`), not a keyboard call; it has no `int`.

### What the sites take, from the bytes

| site | AH | flags before the `int` | what follows |
|---|---|---|---|
| `0x24D0E` | 1 (`0x24D08`) | from `xor ebx,ebx` `0x24D0A` | ZF = 1 (empty): `0x24D12`, `dec dh` of 1 is 0, `sub eax,eax` `0x24D19`; ZF = 0: `jne 0x24d1b`. Then `mov bx,ax; test ebx,ebx` (EBX zeroed at `0x24D0A`): a queued word 0 ends the loop as an empty queue does |
| `0x24D2E` | 0 (`xor eax,eax; mov ah,al` `0x24D26`) | ZF = 1 (`xor ebx,ebx` `0x24D2A`) | `0x24D32` runs (`dec dh` of 0 is 0xFF, `jne 0x24d3b`): `0x24D39` is dead |
| `0x24E54` | 0 (`xor ah,ah` `0x24E50`) | ZF = 1 (that `xor`) | the same: `0x24E58` runs, `0x24E5F` is dead |
| `0x24A64` | 0 (`xor ah,ah` `0x24A60`) | ZF = 1 | the same: `0x24A68` runs, `0x24A6F` is dead (named since d6a) |
| `0x2EB15` | 1 (`0x2EB11`) | (not read: ZF is the BIOS's) | `and eax,0xffff` `0x2EB22`; `je 0x2eb6e`: a word 0 ends the drain |
| `0x2EB2D` | BL = 0 (`xor ebx,ebx` `0x2EAEE`; nothing in the loop writes EBX: `0x500BB` is `mov eax,[0x101500]; ret`, the stubs `0x1CF20`/`0x500C4` clobber nothing) | ZF = 0 (`and eax,0xffff` on a non-zero word, `0x2EB22`; `mov ah,bl`/`mov dh,ah` keep flags) | `jne 0x2eb3a` taken: `0x2EB31` is not reached under a BIOS that returns AH = 0's flags unchanged (as the model does; on any BIOS it joins `0x2EB3A` with AX kept), and `0x2EB38` is dead |

The latch, both loops: `test al,0xff` / `test dl,0xff` picks the ASCII byte, else `shr 8` the scan code; `and
eax,esi` / `and edx,esi` with ESI = 0xFF (`0x24CFE`, `0x2EAE9`). `0x2EA78` also stores the key word
zero-extended (`xor edx,edx; mov dx,ax` `0x2EB3A`) and the key time `0x500BB()` = the dword `0x101500`.

### Per row

| row | pokes before -> after | cases | blocks before -> after | mutants before -> after | named unhit after |
|---|---|---|---|---|---|
| `config_screen_wait` `0x2EA78` | 1 (`0x2EB15` `cmp eax,eax`) -> 0 | w0..w4 (same, `keys=()`), k0..k3 | 15/23 -> 20/23 | 16 -> 23 | `0x2EB31` (above), `0x2EB38` (dead), `0x2EB4D` (key word 0x2400: `0x5004A`, the joystick calibration, which the port leaves out, config.c PORT) |
| `config_screen_wait_zero` `0x2EA74` | 1 -> 0 | z0, z1 (z1 now with the key 0x4800, which n = 0 drains) | 15/23 -> 20/23 | 3 -> 3 | the same three |
| `game_quit_prompt` `0x249F0` | 1 (`0x24A64` `mov al,K`) -> 0 | q0..qa (the same words, scan byte 0), qb, qc, qd | 16/18 -> 16/18 (the back edge `jne 0x24a60` `0x24AF3`, never taken under the poke, now taken in qb) | 20 -> 23 | `0x24A6F` (dead), `0x24A9C` (the soft-restart arm, the longjmp `0x65431`; as in d6a) |
| `game_frame_*` (8 d5h rows) `0x24C5C` | 1 each (`0x24D0E` `cmp eax,eax`) -> 0 | the same cases, `keys=()` | each row unchanged; the union 116/161 -> 157/161 with the two new rows | 177 -> 177 | `0x25201` (restart, as before); the key-loop blocks are now owned by the rows below |
| `game_frame_keys` `0x24C5C` (new) | 0 | k0..k6, ka, kb (modes 0, 3, 0x27) | 62/161 | 14 | `0x24D39`, `0x24E5F` (dead), `0x24DD3` (scan code 0x24, Alt-J: `0x5004A`, left out by the port, game_key_loop PORT), `0x25201` |
| `game_frame_keys2` `0x24C5C` (new) | 0 | k7, k8, k9 (modes 0x1E, 0x17) | 48/161 | 3 | the same |

The key loop is two rows because diffrun takes 16 stubs a case (`c.nstub < 16`, `run_cases`): modes 0, 3 and
0x27 need `0x11D04`, `0x50146`, `0x2FFC4`, the three key callees `0x249F0`, `0x1D1B0`, `0x1D220` and the five of
the pause (`0x1D250`, `0x2F198`, `0x2EA78`, `0x2F280`, `0x1D270`) besides the four every d5h row has (15);
modes 0x1E and 0x17 need `0x1EEB0`, `0x4F318` and `0x20860` instead (12). `0x1C500`'s tree runs on both sides
(`D1D_STR_ALLOW`); the pause cases seed string 0x1E8 (`d1d_str_pokes`). The other rows' unhit blocks are
named by owner as before (`d5h_gf_unhit`, `D5H_GF_ROWS` gains `keys`, `keys2`); `D5H_GF_KEYS` is gone.

**A constraint on the key cases**, from the stubs' declared clobbers (`E.callee_clobbers`): `0x249F0` and
`0x1D220` clobber every register (the longjmp tail `0x65431`; `0x1D220`'s tail jump `0x1CD9C`'s tree) and
`0x2EA78` clobbers EDI and EBP, and the harness poisons them; the loop keeps ESI (`and eax,esi`) and EDI (`mov
[0x104b00],di` `0x24EE0`) across those calls. So a key after one that reaches `0x249F0` or `0x1D220`, or an Enter
in mode 3 after a pause, is not a case: each such key is the last of its queue. Whether the real `0x2EA78`
keeps EDI (its callees' trees decide) is not claimed.

**Port side.** `PR_SEAM` added to `game_quit_prompt` (`0x249F0`), `nameentry_key` (`0x20860`) and
`sound_sample_pause_toggle` (`0x1D220`), each with a row of its own; inert unless a case stubs the address. No
other change to `port/src`. **No correction**: every converted row and both new rows were VERIFIED at their
first run (the port's `game_key_loop`, `config_screen_wait`'s drain and `game_quit_prompt`'s loop agree with the
bytes on every case).

### Mutants (each the old code's possible error; caught by, in)

`config_screen_wait`: `@nokeys` (the d6a restatement: no drain; keys, latch: k0 k1 k3), `@klatchw` (latch not
masked: k0 k3), `@kscan` (no scan-code fallback: k1), `@kwordw` (key word stored as a word: k0 k1 k3),
`@ktime` (key time from `0x101508`: k0 k1 k3), `@kone` (one key a pass: k1 k3), `@kany` (a queued word 0 read
as a key: k1). The old mutants restate the drain too and are caught in the old cases and now also in the k
cases; `@m1` gains `byte` and `keys` (k2's key is drained); `config_screen_wait_zero`'s `@m1` and `@eax` gain
`byte` and `keys` (z1's key).

`game_quit_prompt`: `@once` (the d6a restatement: one key: qb), `@ax` (AX compared, not AL: qb qc qd), `@more`
(reads on after an answer while keys remain: qc). The mutant core now loops and stops on an empty queue (where
the original stops), so a mutant that reads past the keys returns instead of waiting. `@noflag` gains `byte`,
`call #3 memory` and `keys` (qb), `@noupper` gains `keys` (qc).

`game_frame_keys`: `@knone` (no key loop: the d5h restatement), `@kmode1` (the mode read once: k3), `@kscan`
(k2 k6), `@klatchw`, `@kzero` (a word 0 read as a key: k1), `@kenter` (Enter in any mode: k1), `@kesc27` (Esc
in mode 0x27 prompts: k3 kb), `@kescarg` (Esc in mode 3 with AL = 1: k4), `@kspace` (space not ignored in
modes 3/0x27: k3 k4 kb), `@kpausew` (the pause compares AX with 0x20: ka), `@kpauserow` (row 0xA: ka),
`@kpauseflag` (the byte 0x104B22 left set: ka), `@kaltq` (Alt-Q with AL = 1: k6), `@kaltsm` (Alt-S and Alt-M
swapped: k2). `game_frame_keys2`: `@knone`, `@kname` (an extended key in mode 0x1E to `0x20860`: k7), `@khook`
(mode 0x17 ignores space whatever the hook: k9). They run `d5h_24c5c_core`, which now calls the restated key
loop (`h1_24c5c_keys`) at the raw's place; with no flip (every d5h mutant, whose cases queue no key) it is
flow.c's `game_key_loop`. Proof that the restatement is faithful: a temporary `@nomut` binding (no flip) was
NOT DETECTED on both rows; every d5h pin is unchanged.

### Measurements

- Full `--self-check` on the base `7930eada` (built with the same default build type) and on h1: the only
  differences are the rows above and their mutants. Before: `998/998 functions VERIFIED; 14846/14846 mutants
  detected; 1 named gaps; 638/829 rows with callees closed (169 have none)`. After: `1000/1000 functions
  VERIFIED; 14873/14873 mutants detected; 1 named gaps; 638/831 rows with callees closed (169 have none)` (the
  two new rows are open on `0x500BB` and `0x65431`, named non-rows).
- Harness suite: 192 tests OK. `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all checks passed.

## §H.3 Batch h2: the VGA and aperture rows

Branch `reverse-h2`, on `h0` (`7930eada`). Two rows converted, both leave the weaker-row list. One harness
addition (a declared difference, `H2Case`); no change to `tools/diff_emu.py`. No port correction.

| row | pokes before | pokes after | cases | blocks | mutants | leaves the weaker list |
|---|---|---|---|---|---|---|
| `0x52106` `gfx_screen_reset` | 4 (6 instructions: the retrace read `0x52129`, `out` `0x5213F`, `0x52145..0x52147`, the aperture fill `call 0x51f72` `0x52151`) | 0 | s0..s2 (same inputs, retrace bytes added, the screen's ends seeded) | 5/5 -> 5/5 | 7 -> 14 | yes |
| `0x51ED8` `sprite_blit_aperture` | 0 (partial: no draw compared) | 0 | a0..a3 (now on the screen) + d1, d2, d9, d3 | 4/4 -> 4/4 | 4 -> 8 | yes, with one declared difference (below) |

### §H.3.1 `0x52106`: the bytes and the row

`push ecx; push edx`; `[0x101508] = [0x10150C] = EAX` (`0x52108`, `0x5210D`); `mov edx,eax` (`0x52112`);
`0x51F72([0x1014E8], EDX)` and `0x51F72([0x1014E4], EDX)` (`0x52114..0x52123`; `0x51F72` keeps EDX: it pushes
ECX alone and stores EDX); `push edx` (`0x52128`); `mov edx,0x3da; in al,dx; test al,8; je 0x5212e`
(`0x52129..0x52131`); `mov ecx,0x100; mov edx,0x3c8; xor eax,eax; out dx,al` (`0x52133..0x5213F`); `mov
edx,0x3c9; out dx,al` three times (`0x52140..0x52147`); `dec ecx; jg 0x52145` (`0x52148`); `pop edx`
(`0x5214B`); `mov eax,0xa0000; call 0x51f72` (`0x5214C`, `0x52151`); `pop edx; pop ecx; ret`.

What the model answers, and why it is exact for these paths:

- **The retrace read.** The code keeps bit 3 of the byte alone (`test al,8` `0x5212F`; AL is set to 0 by `xor
  eax,eax` `0x5213D` before its next use), so the case's bytes are all the hardware's answer can matter for.
  s0 `(0x08,)`, s1 `(0xF7, 0x00, 0x08)` (two spins: the back edge `je 0x5212e`, never taken under the poke, is
  taken twice), s2 `(0xFF,)`.
- **The DAC.** AL = 0 at every `out` (`xor eax,eax` `0x5213D`, nothing writes EAX before `0x52148`): the index
  0, then 0x100 entries of three zero channels (`dec ecx; jg` from ECX = 0x100: a do-while of 256). Through
  `E.dac_effects`: 256 `("dac", i, 0, 0, 0)` for i = 0..255 and `("dac-at", 0, ())` (the index wraps).
- **The screen.** The third fill is `0x51F72(0xA0000, ticks)`: 0xFA00 bytes, exactly `E.APERTURE`. With
  `aperture`, those bytes leave `writes` for `OrigResult.aperture` and are compared with `gfx_aperture()`,
  which the runner seeds from the same image bytes after the pokes. The screen bytes equal to the image's
  bytes before the fill do not show on either side; so the first and last 0x40 screen bytes are poked to
  0x5A (`D6A_RESET_SEED`), which none of the three tick dwords holds (0x00000000, 0xA1B2C3D4, 0x00000100):
  the fill's two ends show in every case (measured: 38152, 63879 and 43873 screen bytes compared in s0, s1,
  s2). The offscreen buffers (0xC0000..0xD0802 + 0xFA00) do not overlap the screen (h0's caution).
- EAX: the raw now returns 0xAFA00 (the third fill's advanced EAX, measured; it was 0xA0000 under the poke).
  Mask 0 kept: the port's function is void and the callers are as before (`0x1C873` hands it to the movie
  player's host replacement, the other sites call on).

**Port side.** `gfx_screen_reset` (gfx.c) reports the DAC writes with `PR_OUT(0x3C8, 0)` and 768 `PR_OUT(0x3C9,
0)` under a `PORT:` note naming `0x5213D/0x5213F`, `0x52145..0x52147`, `0x52148`, `0x52133`. Nothing else in
`port/src` changed: the aperture fill was already the port's `g_aperture` fill (record §K2.4). Before the
`PR_OUT`s the unpoked row was a MISMATCH only by the missing stream (`s0: out #0: original dac[0x00] = 0x00
0x00 0x00, port none (257 and 0 effects)`, and the same in s1, s2); the screen and every byte already agreed.

**Mutants.** The binding core `d6a_52106_core` now also makes the DAC writes and the screen fill, so the seven
old mutants are caught exactly as before (same kinds, same cases). Seven new ones:

| mutant | the wrong code | caught by | cases |
|---|---|---|---|
| `@nodac` | no DAC write | out | s0 s1 s2 |
| `@dacidx` | the index 1 | out | s0 s1 s2 |
| `@dac255` | 255 entries | out | s0 s1 s2 |
| `@dacval` | the channels from the ticks (`xor eax,eax` dropped) | out | s1 |
| `@noap` | no screen fill | aperture | s0 s1 s2 |
| `@apbyte` | the screen filled with the ticks' low byte | aperture | s1 s2 |
| `@apshort` | the screen fill one dword short | aperture | s0 s1 s2 (the seeded end) |

**Mutation proofs on the port** (gfx.c changed, rebuilt, the row run, restored): the screen fill from `i = 4`
-> `s0: aperture 0xA0000: original 0x00, port unchanged`; `PR_OUT(0x3C8u, 2u)` -> `out #0: original dac[0x00]
..., port dac[0x02] ...`; the channels `ticks` -> `s1: out #0: ... port dac[0x00] = 0x14 0x14 0x14`.

### §H.3.2 A declared difference (`H2Case`, commit `9cfadba9`)

`tools/diff_verify.py` (the `h2_` block): `H2Case(Case)` adds `declared = {addr: (the original's final byte,
the port's)}`, None for a byte the run leaves unchanged; a pair with no difference is refused (a stale
declaration). `h2_compare(case, orig, port, mask)`, called by `verify_spec` in place of `compare` (one-line
hook), checks each declared byte against its declared pair, reports any other value as `byte 0x...: declared
original X, port Y; the run gives original X', port Y'` (counted as a difference: it detects a mutant), and
then leaves the declared bytes out of `compare`. A case that is not an `H2Case` is `compare` itself: the full
`--self-check` printed byte-identical output before and after the commit. The declaration covers the bytes at
return, not the memory at a recorded call (no `H2Case` row records a call). Tests: `H2DeclaredTests`
(`tools/tests/test_diff_verify.py`); each fails under a mutation: the hook reverted to `compare` (1 failure),
the values not checked (3), the stale-declaration check dropped (1), the declared bytes not left out (5).

### §H.3.3 `0x51ED8`: the drawing cases

The bytes are §N.1.1's. The aperture base needs no poke now: `add edi,0xa0000` (`0x51F1B`) points the
renderer at the machine's screen, and the port draws into `gfx_aperture()` seeded from the same bytes. The
three renderers the one caller reaches run on the original's side (allowed with the copy routines they call on
these run lengths, measured: `0x58564`, `0x5859C`, `0x58D2C`, `0x58D4C`, `0x58D90`, `0x521F0`, `0x5222C`,
`0x529A0`, `0x529C4`, `0x52A18`; another length reaches another routine and stops the run, it cannot pass
silently).

**The declared difference, and only it.** The renderers use the node as their scratch; the port's take its
fields by value (`sprite.c` `PORT:`, §N.1.1). From the bytes: `+0x14` ends at 0 (`dec dword [ebp+0x14]; jne`
`0x5D262/0x5D289`, `0x57FCE/0x57FF5`; `0x58CBD`'s loop measured) where the port keeps `0x51F2F`'s `rows -
clip_b`; `+0x38 = 0x140 - ECX` (`0x5D218..0x5D21F`, `0x58CD6..0x58CDD`) or `0x140 + ECX` (`0x57F80..0x57F8B`),
ECX the width (`0x51F26`), where the port leaves the node. `h2_scratch` derives each case's declaration from
those values and the seeds (`+0x14` = rows, `+0x38` = 0x38383838): the bytes of the two dwords where the sides
end apart, with both values. Measured with no declaration (the experiment before the mechanism), the only
differences were exactly those bytes: `0x10A914`, `0x10A939..0x10A93B` (and `0x10A938` for type 9: 0x48);
every screen byte agreed. The cases keep `clip_l`, `clip_r` and `clip_t` 0, the caller's (`0x1C620..0x1C62C`):
with `clip_t` set `0x58CBD` would also `sub [ebp+0x14],eax` (`0x58CC4`), and with `clip_l`/`clip_r` set its
`mov [ebp+0x18],ecx` (`0x58CD3`) would change `+0x18`, a byte outside the declaration; with them 0 it stores the
width back unchanged (measured: no `+0x18` difference).

| case | node | renderer | screen bytes compared | declared (original, port) |
|---|---|---|---|---|
| `d1` | type 1, 8x4, x 10, y 2 | `0x5D218` | 27 (a transparent run of 2 and of 3) | `+0x14` 0 / unchanged (4); `+0x38` 0x138 / unchanged |
| `d2` | type 2 | `0x58CBD` | 32 | the same |
| `d9` | type 9 | `0x57F80` | 27 | `+0x14` as d1; `+0x38` 0x148 / unchanged |
| `d3` | type 1, clip_b 1 | `0x5D218` | 19 (three rows) | `+0x14` 0 / 3; `+0x38` 0x138 / unchanged |

Each drawing case seeds the row table for rows 1..3 (0x3C0, 0x80, 0x500: a wrong row index draws elsewhere)
and the screen under the node's rows (x 0..31, four rows, `d5b_bytes`), so a transparent run must leave the
seed. `d3` is outside the caller's domain (it zeroes `clip_b` too) and inside the function's: the renderer
draws `rows - clip_b` rows. a0..a3 are unchanged but run on the screen too, so the `ret` entries' "nothing
drawn" is now compared (it catches `@nomirror` in a2).

**Mutants** (the binding draws through `sprite_blit_at` on a copy of the node, one choice flipped):

| mutant | the wrong code | caught by | cases |
|---|---|---|---|
| `@nodraw` | no draw (the store kept) | aperture | d1 d2 d3 d9 |
| `@noclipb` | all rows drawn (clip_b ignored) | aperture | d3 |
| `@nomirror` | the mirror bit dropped | aperture | a2 d9 |
| `@xoff` | one pixel right | aperture | d1 d2 d3 d9 |

The old four keep their kinds; `@nostore` and `@clipt` are now caught in d3 as well (its declared port value 3
is not what they leave). **Mutation proofs on the port** (sprite.c changed, rebuilt, the row run, restored):
`sprite_blit_at(n, gfx_aperture() + 320)` -> `d1: aperture 0xA008A: original 0x16, port unchanged`; the store
`n->rows -= 0` -> `a2: byte 0x10A914: original 0x03, port unchanged`.

**What the row claims, and not.** The pixels of the caller's three live types, unclipped (and type 1 with
`clip_b`), with the d5b sprites and bank 6; the guards and the store as before. Not claimed: the clipped types
and the shear types, which the one caller never builds (`render_list_blit` runs them through `0x51E5C` on the
same table); the renderers have no rows of their own (the self-check prints them `allow unverified`): they are
compared only through the screen bytes of this row and `render_list_blit`'s. The node's `+0x14`/`+0x38` after
a live renderer is the declared, documented difference: the one caller reads its frame again only at `+0x1C`
(`0x1C64F`).

### §H.3.4 The other weak rows

None other has a VGA poke. `game_loop`'s two pokes are `call 0x5d7dc` (`0x256B1`, `0x256D6`), the runtime's
`rand` (`0x5D7DC`: the LCG over `0xEF6D8`), not hardware. Its named unhit block `0x25680` is the full copy to
the screen (`mov edi,0xa0000` `0x255E9`, `rep movs` `0x25693`); the screen model could reach it, but the port
presents the whole back buffer on both paths where the original's flag-clear path is the dirty blit `0x501A3`
(a `PORT:`), so it is left to a later batch, unconverted. `0x1BE30`'s poke is `int 0x10` (the BIOS mode set at
`0x4FB9E`), which the machine does not model (h0's model has `int 0x16` alone, and this batch does not change
`tools/diff_emu.py`): it stays. `0x1C740`'s retrace read (`0x1C7CA`) belongs to the Smacker loop, out of scope.

### §H.3.5 Measurements

- Full `--self-check` after `0x52106` (`3e74939e`) against `h0`'s tools on the same build: the seven new mutant
  rows and the counter differ, nothing else. After the declaration (`9cfadba9`): byte-identical to `3e74939e`.
  After `0x51ED8` (`4e7ebf2c`): only `sprite_blit_aperture`'s rows (8 cases, the allowed renderers, four new
  mutants) and the counter differ.
- Final: `998/998 functions VERIFIED; 14857/14857 mutants detected; 1 named gaps; 638/829 rows with callees
  closed (169 have none)` (both rows' callee status unchanged: `0x52106`'s is closed, `0x51ED8`'s open on
  `0x1B544`, a named non-row).
- Harness suite: 196 tests OK. `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all checks passed.
  `tools/port_progress.py` unchanged (no new function).
