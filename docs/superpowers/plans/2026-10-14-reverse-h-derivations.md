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
