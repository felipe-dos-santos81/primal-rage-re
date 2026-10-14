# Track N — the last twelve named gaps — derivation record

Three batches (`n1`, `n2`, `n3`) re-decide from the original's bytes whether the ported functions that have
no differential row can take one. Each batch writes its own section; the integrator merges them.

## §N.3 Batch `n3`: the resource loaders, the string table, the boot logos' player

Branch `reverse-n3` off `main` `95afed00`. Ground truth: the fixed-up image `/tmp/pr_n3_img.bin` (`build/diffrun
--image-out`), disassembled with capstone 5.0.7, and the Ghidra project `rage` (read-only). Every figure is
measured with `tools/diff_verify.py --self-check`; the pins (`N3_MASKS`/`N3_KINDS`/`N3_CASES`) come from
`verify_all` on `N3_SPECS`, run five times with identical results.

| function | port | outcome | what decides it |
|---|---|---|---|
| `0x1B120` | `res_load_index` (res.c) | **poked row, partial** (21/35 blocks) | The six file calls are pokeable (`mov eax,<result>`, track S's form); every block between them corresponds to the port's C once the port allocates through `0x1C308`'s seam and `0x1B3AC`'s head has one. The 14 unhit blocks are the fatal `0x1D290` and the allocator's retries. Two corrections (§N.3.3). |
| `0x1B3AC` | none whole: the head is `res_load_present` (res.c) | **stays a gap** | No port function is `0x1B3AC`. Its head `0x1B3B8..0x1B3F8` is `res_load_present`; its read `0x1B403..0x1B45A` (`0x61C60`, `0x61CF6`, `0x61EC0`, `int 0x21` at `0x6F53A` under them) is the host's, done by `res_load_index` for every entry at init (PORT); its mark `0x1B47A` is made by `res_load_index` after the read and by `res_resolve` before the head; `0x1E75C`/`0x1E808` (`0x1B423`, `0x1B475`) and the stores to `[[0x101514]+0x253]` (`0x1B3D3`, `0x1B42E`, `0x1B451`) have no port counterpart; and the port's head advances the ISR's counters (`game_isr_ticks`, the read-stall model, PORT) where the raw writes none. A row would compare a composition the binding assembles, not the port. |
| `0x47370` | `game_string_table_load` (flow.c) | **stays a gap** | The port's loader is a replacement that makes none of the raw's calls: `0x1E6D8` (the free of the old table, `0x47389`), `0x1C308(0x42, size)` (`0x47429`, retried with 2 at `0x47442`), `0x1E75C(handle, 1)` (`0x4746B`), `0x1E808` (`0x474A7`), and on failure `0x1D290` (`0x474D3`, `0x47496`) or the runtime's `exit` (`0x474BA`, a bad magic: `cmp dword [0x1082d8],0x33340001` `0x473E0`). Its two stores take values of its own making: `0x1082DC` is a fixed handle `0x3800000` (STRING_HANDLE), not the allocator's, and `0x1082D8` the bytes read capped at `0x2000`, not ftell's size. It ignores the language index (`[edi+0xC9418]`, `0x473A0`) and returns at once on a second call (a C static), where the raw frees and reloads (`0x20C7F`, `0x336AE`). The file calls could be poked as `0x1B120`'s are; the rest could not. |
| `0x1C740` | `movie_play` (movie.c) | **poked row, partial** (4/23 blocks) | The entry blank, the key test, the quit byte, the open and the epilogue are the port's prefix. Two pokes: the runtime's kbhit (`0x1C752`) and the Smacker open (`0x1C76C..0x1C778`). The play loop (19 blocks) is the Smacker library and the hardware, which the port replaces with its own decoder (movie.c, smacker.c, PORT). |

### §N.3.1 `0x1B120`: the bytes

No argument. `push` of six registers, `pop`s and `ret` at `0x1B3AA`. One caller, `0x1C09D` in `0x1BEC4`, whose
next call `0x51F45` begins `pushal; mov eax,[0x1014e4]`: **mask 0**.

| block | instructions | what it does |
|---|---|---|
| `0x1B120` | `mov edx,0x805a0; mov eax,0x805a4; call 0x61c60`; `mov ecx,eax; mov ebp,eax; test eax,eax; jz 0x1B1EB` | `fopen("index", "rb")` |
| `0x1B141` | `call 0x6225a` (EBX = 2), `call 0x62356`, `call 0x6225a` (EBX = 0); `mov edi,eax; mov esi,eax`; `test edi,edi; jle 0x1B1DF` | fseek end, ftell, fseek 0: the size in EDI and ESI |
| `0x1B16B`, `0x1B17A` | `div ecx` (0x14); `jz 0x1B189`; else `0x1D290(2, 0x805A4)` | a size that is not a multiple of 0x14 is fatal |
| `0x1B189`, `0x1B1AE`, `0x1B1BF` | `[0x1014F0] = size / 0x14` (`0x1B194`); `0x1C308(0x41, size)` (`0x1B1A0`) to `[0x1014E0]` (`0x1B1A5`), retried with EAX = 1; `fread([0x1014E0], 1, size, file)` (`0x1B1CD`), `fclose` (`0x1B1D6`, EDX kept); `cmp edx,esi; jz 0x1B1FA` | the count first, then the table |
| `0x1B1DF`, `0x1B1EB`, `0x1B1F5` | `0x1D290(2, ..)` / `0x1D290(1, ..)` | a size not above 0, a short read, no file: fatal |
| `0x1B1FA`, `0x1B210` | ECX = `[0x1014E0]`, ESI = 0; `test [ecx+0xc],0x1000000; jz 0x1B259` | the walk; a preloaded entry? |
| `0x1B219`..`0x1B257` | `0x1C308(0x41, [ecx+0xc] & 0xFFFFFF)` to `[ecx+0x10]`, retried with 1 (`0x1B233`); `0x1B3AC(ecx, EDX = 1, EBX = 0)` (`0x1B252`) | preloaded: allocate and read now |
| `0x1B259`..`0x1B289` | `0x1C308(0xE, size)` to `[ecx+0x10]`; `mov eax,[ecx+0xc]; and eax,0xffffff; mov edx,[0x1014f8]; cmp eax,edx; jle 0x1B28E`; `[0x1014F8] = size` | lazy: allocate; raise the stored maximum, signed |
| `0x1B28E` | `mov ebx,[0x1014f0]; inc esi; add ecx,0x14; cmp esi,ebx; jl 0x1B210` | the count read again each entry |
| `0x1B2A0`..`0x1B3A4` | `0x1C308(0x41, 0xFA00)` to `0x1014E8`, `(0xFA00)` to `0x1014E4`, `(0x4880)` to `0x1014EC`, `(0xEBA0)` to `0x1014F4`, each retried with 1 and then `0x1D290(4, 0)` | the four buffers |

The fopen leads to `int 0x21` at `0x6F53A` (measured: with each runtime callee allowed in turn the original
stops there, through `0x61C60`, `0x61C45`, `0x6BDBC`, `0x61423`, `0x6B36F`, `0x6B4B3`, `0x6B19F`, `0x6F506`).
The calls are the runtime's, the obstacle is not the function: the same pokes as track S's `0x1AFE8` row work.

### §N.3.2 The rows

| row | entry | cases | blocks | mutants | EAX mask | weaker |
|---|---|---|---|---|---|---|
| `res_load_index` | `0x1B120` | 5 (`x0`..`x4`) | 21/35 (14 named) | 11 | 0 | poked, partial |
| `movie_play` | `0x1C740` | 6 (`k0`, `k1`, `q0`, `q1`, `o0`, `o1`) | 4/23 (19 named) | 7 | 0 | poked, partial |

**`res_load_index` under six pokes.** The file calls at `0x1B130` (fopen, the case's handle), `0x1B148` and
`0x1B15E` (fseek, 0 or -1: their EAX is not read, `0x1B14D` and `0x1B170` load EAX), `0x1B14F` (ftell) and
`0x1B1CD` (fread), both the table's size, and `0x1B1D6` (fclose, its EAX not read) are `B8 imm32`. Exact for
the paths taken: the code after each call reads only EAX of it (ECX, EBP, ESI, EDI are reused and so kept by
the runtime; fclose keeps EDX, `0x1B1DB`). `0x1C308` is a stub answering `N3_TABLE` (`0x10AC00`, zero BSS of
the image) where the case seeds the INDEX bytes; `0x1B3AC` is a stub (clobbers `ebx edx ebp`, derived) recorded
as (EAX = the entry, EBX). The binding (`n3_*` in `diff_runner.c`) writes the poked ftell's count of bytes from
`N3_TABLE` to a scratch `INDEX` and passes a game directory that does not exist. The row sees: the allocation
sizes and their order (seven fixed calls plus one per entry), each `+0x10` store, the count, the table pointer,
the four buffers, the maximum, which entries get `0x1B3AC` and with which EBX, and the memory at every call.
**It is blind to:** the INDEX read (the raw's fread is poked and writes nothing; the port reads the bytes already
there), the file calls' arguments (`0x805A4`, `0x805A0`, the handle, the buffer), which block lands in which
field (one stub EAX per case: all are `N3_TABLE`), the allocation type in EAX (`0x41`, `0xE`, the retry's 1:
the port's allocator has none), `0x1B3AC`'s EDX (the fatal flag, 1), and everything `0x1B3AC` does: the payload
read and the mark are not in the row. The port reads every payload in `res_load_index` and marks the preloaded
entries it read (res.c, PORT); the binding's missing directory makes that read nothing, so the difference
between the port's in-line read and the raw's stubbed one does not appear. A case with the payload files
present would show the port's mark where the stubbed raw has none.

Cases. `x0`: a preloaded entry is the largest (`0x20000`); the maximum is the lazy `0x3000`. `x1`: the stored
maximum `0x800000` is above every size and stays; flag bytes `0x02` and `0x20` on lazy entries; dirty
registers; handle `0x100`. `x2`: the stored maximum `0x80000000` is negative and the first lazy size replaces
it (signed); flag byte `0x03`. `x3`: one preloaded entry; the maximum `0x777` stays. `x4`: six entries, a
12-character name with no NUL, the size `0xFFFFFF` on a lazy entry with flag byte `0xFE`, a preloaded entry
with `0x81`, the fseeks and fclose answer -1. The seven dwords `0x1014E0..0x1014F8` and the byte `0x1014FC`
are seeded.

Named unhit (14): `0x1B17A`, `0x1B1DF`, `0x1B1EB`, `0x1B1F5` and the buffers' `0x1B2D5`, `0x1B316`,
`0x1B357`, `0x1B398` reach `0x1D290`, the fatal error, which the port does not reach (`res_load_index` returns
0 and `game_init`'s `game_fatal` exits, flow.c PORT); `0x1B1AE`, `0x1B233`, `0x1B2B8`, `0x1B2F9`, `0x1B33A`,
`0x1B37B` are the retries after a `0x1C308` that answers 0, which one stub EAX per case cannot give after a
non-zero one (the port returns 0 there). The port's own guard of more than 256 entries (`RES_MAX`) has no raw
counterpart and is not reached (the cases have up to six).

**`movie_play` under two pokes.** EAX = the name (`0x80038` or `0x80044`, the two call sites `0x11050`,
`0x1105A`). The kbhit call at `0x1C752` is `mov eax,<result>`: the runtime's `0x62756` answers 1 when
`[0xEF910]` is set and else the BIOS's AL `0` or `0xFF` widened (`int 0x21` AH = 0Bh at `0x62767`, `cbw;
cwde`); the binding gives the port's key queue one key when the result is not 0. The open
`0x1C76C..0x1C778` (`push esi; push 0xfe00; push -1; call 0x6345C`, which pops its 12 bytes: `ret 0xc` at
`0x635EC`) is `mov byte [0x10AF00],1; xor eax,eax` and four nops: reached, it fails. The binding's game
directory holds both names as two-byte files that are not movies: the port's `res_load_file` reads one into
the bump heap and `smk_open` rejects it, and when the heap moved the binding clears what was read (mem[] above
the image is zero) and stores 1 at `0x10AF00`. `0x52106` is the existing stub (`D4D_SCREEN`). The row sees the
entry blank and its argument, the key and quit gates (the quit byte as a byte: `o0` sets the byte above), that
the open is reached exactly when neither gate leaves, and that none of these exits makes the exit blank. **It is
blind to** the open's arguments, any movie that opens (a non-zero open enters the loop, whose first call `0x649B0`
is the library's: not exercisable), and the 19 blocks of the play loop from `0x1C785` (named: the Smacker library
`0x649B0`, `0x65340`, `0x64130`, `0x64ED8`, `0x643CC`, `0x65240`, `0x63CE8`; the retrace `in al,dx` at `0x1C7CA`;
the host-owned blit `0x50D23`; the key and pad tests in the loop; the exit blank `0x1C873`). Mask 0: the first
call site loads EAX (`0x11055`) and the second's EAX is `attract_step`'s return, whose row's mask is 0.

Not compared, a port-only guard: `movie_play` returns 0 before the entry blank for a NULL directory or name or
an empty name; the raw has no such test and its two callers pass the fixed names.

### §N.3.3 The corrections

| correction | raw | caught by (old code = mutant) |
|---|---|---|
| `res_load_index` stores the count `0x1014F0` before the table's `0x1C308` and the table pointer `0x1014E0` right after it | `0x1B194` before `0x1B1A0`; `0x1B1A5` | `@countlate`: `call #0 memory`, all five cases |
| `res_load_index` raises `0x1014F8` only for an entry without `0x1000000`, by the size read again after the allocation, when greater **signed** than the stored dword (it was the largest size of every entry, from 0, stored after the walk) | `0x1B210 jz`, `0x1B26F..0x1B289` (`cmp eax,edx; jle`) | `@maxall`: `byte` and `call #2..#8 memory`, all five cases |

Before the corrections the row was MISMATCH on those differences alone (measured: every case's `call #0 memory`
at `0x1014F0`, and `0x1014F8` in each case). The shipped INDEX gives the same maximum both ways (its largest size
`0x3A2AF4` is a lazy entry's; the preloaded `s16fonts` `0xAC18` and `s16statu` `0x61C94` are smaller), and the
only readers of `0x1014F8` are `0x1C264`, `0x1C28C`, `0x1C2B4`, in `0x1C260`, a memory check (it asks the
allocator `0x1E458` for the maximum + `0x1003` bytes and else prints the shortfall and exits) that nothing calls:
no rel32 `call`/`jmp` to it and no dword holding it in the image. So no oracle can move. (The commit message of
`f7a40032` says these readers are `0x1C0F0`'s; they are `0x1C260`'s, which follows `0x1C0F0`.) `test_res` (test_platform.c) computes its expected maximum by the raw's rule now;
its assertion is unchanged.

The port change that makes the row possible (no behaviour change outside `build/diffrun`): `res_load_index`'s
seven allocations go through `res_block_alloc` (the seamed form of the same bump allocator; the raw's `call
0x1C308` at each), and `res_load_present` takes the entry (the raw's EAX, `0x1B3B2 mov esi,eax`), is exported,
and opens with `PR_SEAM(0x1B3AC, entry, draw)`.

### §N.3.4 Mutants

`res_load_index`: `@countlate` (the old store order), `@maxall` (the old maximum), `@maxuns` (unsigned compare:
`x2`), `@nopreload`, `@preloadall`, `@draw` (EBX = 1), `@noslot`, `@nomask` (the flag byte in the size), `@count`
(size / 0x10: `x4`), `@bufsize` (0x4880 and 0xEBA0 swapped), `@nobuf`. `movie_play`: `@noreset`, `@reset1`,
`@nokey` (`k0`), `@noquit` (`q0`, `q1`), `@quitw` (`o0`), `@exitblank`, `@noopen` (`o0`, `o1`). 18 of 18
detected; the per-mutant kinds and cases are pinned (`N3_KINDS`, `N3_CASES`).

### §N.3.5 The gaps, and what the bytes show beside them

- `0x1B3AC` (§ table): besides the split above, the raw re-syncs the frame counter (`0x1B45F/0x1B464`) after
  every successful read, the init's (BL = 0) included; the port's head does so only when drawing. At init the
  port's pair has not moved (only the drawing read stall, the master loop's spin and config.c's key wait advance
  it) and `0x255D4/0x255DA` zero both before the loop, so the difference is not observable. The three stores to
  `[[0x101514]+0x253]` have no reader in the code object (a capstone scan of `0x10000..0x73B14` finds only these
  three accesses at `+0x253`): the block is the real-mode driver's (`0x1BF51` loads `RM_DRV` with `0x1B3AC`), which
  the port does not have. Neither is changed.
- `0x47370` (§ table): no partial row either: every path makes a call the port does not make.
- The boot chain's own `0x1B3AC` call (`0x1BF51`, `RM_DRV`) belongs to `0x1BEC4` (batch `n1`).

### §N.3.6 Counter and tests

`--self-check`: **`997/997 functions VERIFIED; 14843/14843 mutants detected; 1 named gaps; 638/828 rows with
callees closed (169 have none)`** (from 995/995; 14825; 636/826). The two new rows have callees; the movie row is
closed (`0x52106` VERIFIED) and closes `attract_step`'s row (`0x11000` stubs `0x1C740`); the loader's row is open
on `0x1B3AC` and `0x1C308`, which have no row. Harness suite: 180 tests OK. `run_tests` (PR_ORACLE_REQUIRED=1):
all checks passed. `tools/port_progress.py`: `787 1203 65`, `747 747 100` (unchanged). Unrowed ported functions:
18 → 16; the named gaps of this batch: 4 → 2 (`0x1B3AC`, `0x47370`). Weaker rows: 10 → 12.
