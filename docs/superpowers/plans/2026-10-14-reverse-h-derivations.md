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
