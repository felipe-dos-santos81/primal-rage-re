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

## §2 The row table (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `frontend_coin_poll` | `0x11F28` | 9 | 5/5 | 12 | `0xFFFFFFFF` | `0x2C060` and `0x2CA7C` stubbed (seamed here). `p4`'s stub EAX `0x100` separates `test eax,eax` from an AL test; `p7` pokes the masks, `p8` indexes the third dword |
| `hiscore_rank_pair` | `0x1ECC8` | 8 | 8/8 | 11 | `0xFF` | `0x2DDE4` and its `0x2DB58` real (their own rows): ten records give two different ranks 0..10 |
| `hiscore_rank_pair_stub` | `0x1ECC8` | 6 | 7/8 | 14 | `0xFF` | the same entry with `0x2DDE4` stubbed (seamed here): the rank's upper word (`s3` `0xFFFF0000`, `s4` `0x0000FFFF`) and the call list. `0x1ED22` named: one stub EAX serves both calls |

### §2.1 Callee stubs added by this batch

| callee | args | pop | clobbers | evidence |
|---|---|---|---|---|
| `0x2C060` | none | 0 | none (`_CALLEE_CLOBBER_FIXES`) | `call 0x2caa8; jmp 0x2ca2c`: `0x2CAA8` writes EAX only (`sete al` `0x2CAAF`, `and eax,0xff` `0x2CAB2`); `0x2CA2C` is `push edx` ... `pop edx` (`0x2CA44`), `ret`. The scan credits a save only to a function's own entry pushes, so it reported `edx`; the raw caller `0x11F28` reads EDX across the call (`0x11F29`, `0x11F39`) |
| `0x2CA7C` | EAX (`cmp eax,[0x105c00]` `0x2CA8B`) | 0 | none | every `ret` is plain (`0x2CA8A`, `0x2CAA7`, and `0x2CA78`'s) |
| `0x2DDE4` | EAX, EDX (`mov [esp+0xc],eax` `0x2DDEC`, `mov edi,edx` `0x2DDF0`) | 0 | `edx` | pushes EBX ECX ESI EDI EBP (`0x2DDE4..0x2DDE8`), both `ret` plain (`0x2DE89`, `0x2DE94`) |

## §3 Corrections

None so far.

## §4 Named gaps and unhit blocks

- `hiscore_rank_pair`: a 32-bit `cmp ax,dx` (`0x1ED02`) is separated by neither row: the stub's two ranks are
  one value and the real ranks have no upper word. The other widths are separated by the stub row (`@w16`,
  `@sext`, `@r1w`).

## §5 Store-sweep findings

- `frontend_coin_poll` stores nothing itself; its one effect is the `0x2CA7C(1)` call (`@nospend`, `@spendn`,
  `@order` are caught by the call list alone).
- `hiscore_rank_pair`: `0x1044C0` and `0x1044C2` are seeded `0x5A5A`/`0x5B5B` between `0x6161` and `0x6262`.
  `a1` (the early bail) leaves `0x1044C2` at its seed, which catches a dword store of the first rank (`@c0w`);
  `@c2w` writes `0x1044C4`; `@noc0`, `@noc2`, `@swap` are byte differences. The stub row adds the order:
  `0x1044C0` is stored before the second call (`@order`: `call #1 memory`).

## §6 Queue entries not done

(filled at the end)
