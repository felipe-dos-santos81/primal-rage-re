# Track D wave 3, batch d3a: game/flow.c — derivation record

Branch `reverse-d3a` (off `reverse-d2int` `7a9dfe7`). Queue: `.superpowers/sdd/2026-10-11-reverse-d1/d3a-queue.txt`
(24 functions of `game/flow.c`, level 0 first). Ground truth: the fixed-up image dumped by
`./build/diffrun --image-out`, disassembled with capstone; every address below is linear.
Claim of every row: equivalence on the exercised blocks and inputs only, with its callees stubbed or run as stated.

## §1 The members

| addr | port function | size | EAX mask and the instructions that fix it |
|---|---|---|---|
| `0x11F28` | `frontend_coin_poll` | 47 | full: `xor eax,eax` `0x11F53`, `mov eax,1` `0x11F4C`, or the zero EAX of `0x2C060` that `test eax,eax; je 0x11f55` (`0x11F30`) just tested |
| `0x1ECC8` | `hiscore_rank_pair` | 98 | `0xFF`: the one caller is `0x1EEE1`, `test al,al` at `0x1EEE6`; the raw leaves the rank's second byte in EAX (`xor al,al` `0x1ECE5`/`0x1ED22`, `xor al,dl` `0x1ED11`, `mov al,1` `0x1ED26`) |
| `0x26D4C` | `flow_26d4c` | 522 | 0: the one caller `0x26D3B` goes on with `or byte [0x104aec],0x18` (`0x26D40`), four pops and `ret` into `0x25326` `jmp 0x2540f`, the mode tail the wave-1 records trace dead (d1c §1, d1d §1) |
| `0x271E0` | `flow_join_prompt_draw` | 115 | 0: the one caller `0x423FC` (`0x41C28` state 7) goes on with `xor edx,edx; mov eax,[0x104abc]` (`0x42401`/`0x42403`) |

## §2 The row table (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `frontend_coin_poll` | `0x11F28` | 9 | 5/5 | 12 | `0xFFFFFFFF` | `0x2C060` and `0x2CA7C` stubbed (seamed here). `p4`'s stub EAX `0x100` separates `test eax,eax` from an AL test; `p7` pokes the masks, `p8` indexes the third dword |
| `hiscore_rank_pair` | `0x1ECC8` | 8 | 8/8 | 11 | `0xFF` | `0x2DDE4` and its `0x2DB58` real (their own rows): ten records give two different ranks 0..10 |
| `hiscore_rank_pair_stub` | `0x1ECC8` | 6 | 7/8 | 14 | `0xFF` | the same entry with `0x2DDE4` stubbed (seamed here): the rank's upper word (`s3` `0xFFFF0000`, `s4` `0x0000FFFF`) and the call list. `0x1ED22` named: one stub EAX serves both calls |
| `flow_26d4c` | `0x26D4C` | 10 | 8/8 | 54 | 0 | the string tree (`0x1C500`..) real, the run clock `0x32970` real in the original from its no-elapsed-time state (the port omits it), the runtime `sprintf` `0x65546` real in the original (§2.2); every other callee a stub. Found correction §3.1 |
| `flow_join_prompt_draw` | `0x271E0` | 4 | 1/1 | 23 | 0 | `0x4660C` and `0x2716C` stubbed (the latter seamed here); the memory at each call fixes the store order (`@b11late`, `@afcearly`, `@early`). `j2`/`j3` carry upper bytes in the dword `0x104AD4` (`xor al,1` on the whole EAX: `@xorb`) |

### §2.1 Callee stubs added by this batch

| callee | args | pop | clobbers | evidence |
|---|---|---|---|---|
| `0x2C060` | none | 0 | none (`_CALLEE_CLOBBER_FIXES`) | `call 0x2caa8; jmp 0x2ca2c`: `0x2CAA8` writes EAX only (`sete al` `0x2CAAF`, `and eax,0xff` `0x2CAB2`); `0x2CA2C` is `push edx` ... `pop edx` (`0x2CA44`), `ret`. The scan credits a save only to a function's own entry pushes, so it reported `edx`; the raw caller `0x11F28` reads EDX across the call (`0x11F29`, `0x11F39`) |
| `0x2CA7C` | EAX (`cmp eax,[0x105c00]` `0x2CA8B`) | 0 | none | every `ret` is plain (`0x2CA8A`, `0x2CAA7`, and `0x2CA78`'s) |
| `0x2DDE4` | EAX, EDX (`mov [esp+0xc],eax` `0x2DDEC`, `mov edi,edx` `0x2DDF0`) | 0 | `edx` | pushes EBX ECX ESI EDI EBP (`0x2DDE4..0x2DDE8`), both `ret` plain (`0x2DE89`, `0x2DE94`) |
| `0x2C2B0` | EAX, EDX (`mov esi,eax` `0x2C2B3`, `mov ecx,edx` `0x2C2B5`) | 0 | `edx edi ebp` (the scan's set, through `0x2C088`'s; the callers here reload EDX and read neither EDI nor EBP across it) | one plain `ret` (`0x2C301`) |
| `0x2716C` | EAX (`mov ecx,eax` `0x2716F`) | 0 | none | pushes EBX ECX EDX; the raw caller `0x271E0` keeps EDX (`0x27229`, read at `0x27233`), EBX, ECX and ESI across it |

### §2.2 The runtime `sprintf` runs from the original's bytes

`0x26D4C`'s text arm (`0x26EE2`) formats through `0x65546`. Wave 1 named such arms unhit (d1c §4: the core
"leaves the flat model"). The cause is the one d2e found for `strlen`: the formatter core `0x6C8EB` saves and
loads ES (`0x6C8ED push es`, `0x6C94E les edx,[ecx-8]`, `0x6CA21 mov es,edx`, `0x6CB68 pop es`), and unicorn's
empty descriptor tables turn the stack 16-bit after a segment load. `diff_emu.FLAT_SEGMENT_CALLEES |= {0x6C8EB}`
gives a row that allows it the flat descriptors; the tree that runs is `0x65546 0x6C8EB 0x6CB6C 0x6CC9F
0x6CE84 0x6CCFA 0x65533 0x72D20 0x72CD6` (`D3A_SPRINTF`, found by running the arm and adding each refused
target; `0x65533` is reached by the indirect call at `0x6CB02`). The port side has no seam there: it formats
with `snprintf`, and the two texts meet at `0x2F198`'s by-value string window (16 bytes).

## §3 Corrections

### §3.1 `flow_26d4c` drew a percent sign the original does not (`0x809AC`, `0x26EF3`/`0x26F01`)

`0x26EF3 push 0x809ac` passes the format `25 73 25 2E 30 33 64 25 00` = `"%s%.03d%"` to `0x65546` (`0x26F01`).
Run from the original's bytes (§2.2) the formatter emits nothing for the bare trailing `%`: with string `0x3D`
= `"DIFFICULTY "`, pct 100 and string `0x3E` = `"!ab"` the text handed to `0x2F198` is `DIFFICULTY 100!ab`.
The port formatted `"%s%03d%%"` (its comment called that "the equivalent") and produced `DIFFICULTY 100%!ab`.
Now `"%s%.03d"`. Mutant `@pct` is the old format (caught by `s0`, `s2`, `s3`: `call #8`). Commit `2281a5f`.
Reached in ordinary play: the end of mode `0x22` with `0x104AC4 <= 0` and `0x104529` bit 1 clear. No unit test
asserted the text. Unproven against the oracles until the integrator's gate.

## §4 Named gaps and unhit blocks

- `hiscore_rank_pair`: a 32-bit `cmp ax,dx` (`0x1ED02`) is separated by neither row: the stub's two ranks are
  one value and the real ranks have no upper word. The other widths are separated by the stub row (`@w16`,
  `@sext`, `@r1w`).
- `flow_26d4c`: nothing unhit. Limits: `0x2F198`'s seam compares the text's first 16 bytes, so `s1`'s 19
  characters hide the old `%` at index 17 (`s0`, `s2`, `s3` show it at 14 and 15); the text cases compose 15
  characters or more because the port's C stack bytes after a shorter string's NUL are not comparable; the
  raw's unbounded `repne scasb` append (`0x26F1D..0x26F3B`) is not driven past the 0x40-byte frame (the port
  bounds it). The run clock `0x32970` is not compared (host-owned): the original runs it from the state where
  it changes no byte.

## §5 Store-sweep findings

- `frontend_coin_poll` stores nothing itself; its one effect is the `0x2CA7C(1)` call (`@nospend`, `@spendn`,
  `@order` are caught by the call list alone).
- `hiscore_rank_pair`: `0x1044C0` and `0x1044C2` are seeded `0x5A5A`/`0x5B5B` between `0x6161` and `0x6262`.
  `a1` (the early bail) leaves `0x1044C2` at its seed, which catches a dword store of the first rank (`@c0w`);
  `@c2w` writes `0x1044C4`; `@noc0`, `@noc2`, `@swap` are byte differences. The stub row adds the order:
  `0x1044C0` is stored before the second call (`@order`: `call #1 memory`).
- `flow_26d4c`: the four stores before the `0x2F510` hold (slot `+0x41 |= 0x10`, word `0x104AFE`, byte
  `0x104AEC &= 0xEF`, word `0x104B00`) are seeded with neighbours and each has a drop/width/value mutant
  (`@nof41 @f41set @f41bit @f41off @afe @afeb @aec @noaec @b00 @b00b`); `@late` (the stores after the hold) is
  caught only by `call #7 memory`. The side byte is zero-extended: `t2`'s `0x80` writes `0x10C1F1` on both
  sides (past the image, inside `mem[]`), `@sidesext` lands on `0x102DF1`.
- `flow_join_prompt_draw`: eleven stores, each seeded `0x5A..` between neighbours, each with a drop or a
  width mutant (`@b21 @b14 @b0c @b11 @afc @afcb @hook @hookw @afe @afeb @ee @eew @b00 @b00b`).

## §6 Queue entries not done

(filled at the end)
