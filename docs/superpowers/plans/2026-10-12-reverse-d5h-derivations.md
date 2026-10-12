# Track D, wave 5, batch d5h — the top of the call graph (derivation record)

Branch `reverse-d5h`, off `reverse-d4int` (`3790f323`). Queue: `0x11D04`, `0x255CC`, `0x20C10`, `0x1B3AC`,
`0x1B120`, `0x24C5C`, `0x1BEC4`. Ground truth: the image dumped by `build/diffrun` (`/tmp/pr_d5h_img.bin`,
fixups applied, linear addresses) disassembled with capstone. Every figure below is measured on this
branch (`python3 tools/diff_verify.py --image /tmp/pr_d5h_img.bin --function <row> --self-check`, and the
scratch scripts named where they were used); nothing is carried over from a plan.

Result: **12 rows over three functions, 269 mutants, 5 port corrections proved by an address and by a
mutant that reproduces the old port**, and four named non-rows with the instruction that stops each.

## §1 The members

| addr | name (port) | size | callers, and what they read of EAX | mask |
|---|---|---|---|---|
| `0x11D04` | `game_state_step` (flow.c) | 546 | one call site, `0x25238` in `0x24C5C`, then `jmp 0x2540f`: `0x2A31C` returns EAX untouched on its `0x2A325 jne 0x2a398` path, but `0x2545C` reloads AX (`mov ax,[0x104b00]`) and compares AX only, and each tail arm loads EAX before its first use (`0x25499 lea eax,[edx*8]`, `0x2550E mov eax,1`, `0x25550 xor eax,eax`, `0x25591 xor eax,eax`) | 0 |
| `0x24C5C` | `game_frame` (flow.c) | 2415 | one call site, `0x2560B` in `0x255CC`, then `0x25610 mov edx,[0x104aec]`; EAX is next written at `0x25621 mov eax,ebx` (a render entry) or `0x2563E mov eax,[0x10150c]`; `0x134C0` between them is a no-argument function (`cmp byte [0x9af3c],0` first, its row has no register) | 0 |
| `0x255CC` | `game_loop_begin` + `game_loop` (flow.c) | 293 | one call site, `0x20DE8` in `0x20C10`, which returns through `0x20DED add esp,0x28 … ret` to `0x1C0C2`: `0x10D34` loads EAX at `0x10D43` or returns at `0x10D5E`, `0x1BE30` and its first callee `0x1D018` take no argument, `0x1C0CC xor eax,eax` | 0 |
| `0x20C10` | `game_init` + `game_init_resume` (flow.c) | 483 | `0x1C0BD` in `0x1BEC4` | not a row (§4) |
| `0x1B3AC` | `res_load_present` (static, res.c) | 277 | `0x1B252`, `0x1B5E9`, `0x1BF51` | not a row (§4) |
| `0x1B120` | `res_load_index` (res.c) | 651 | `0x1C09D` in `0x1BEC4` | not a row (§4) |
| `0x1BEC4` | `game_main`/`game_init` (flow.c) | 542 | `0x6C475` (the runtime's startup) | not a row (§4) |

Callers are every `E8`/`E9` rel32 that lands on the entry and every dword equal to it in the code object
(`/tmp/pr_d5h/xr.py`).

## §2 The rows

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `game_state_step_low` | `0x11D04` | 12 | 15/25 | 21 | 0 | the coin arm with the poll stubbed; states 0..4, 6 and above 9 |
| `game_state_step_high` | `0x11D04` | 15 | 16/25 | 35 | 0 | states 5, 7, 8, 9; `0x11BCC`, `0x29D60`, `0x32970` real in the original |
| `game_state_step_coin` | `0x11D04` | 5 | 10/25 | 4 | 0 | `0x11F28` real over its stubs: each accepted mask |
| `game_frame_head` | `0x24C5C` | 10 | 33/161 | 39 | 0 | the head, the update table, the `0x25414` tail, in mode 0 |
| `game_frame_fight` | `0x24C5C` | 17 | 35/161 | 21 | 0 | modes 1..0xB and 0x20 |
| `game_frame_tails` | `0x24C5C` | 11 | 36/161 | 31 | 0 | modes 0xC, 0x21..0x23 and their mode tails |
| `game_frame_m25` | `0x24C5C` | 11 | 36/161 | 23 | 0 | modes 0x24, 0x25 and the 0x25 tail |
| `game_frame_match` | `0x24C5C` | 12 | 33/161 | 12 | 0 | modes 0xD..0x10, 0x12..0x14, 0x1E, 0x1F, 0x30..0x32 |
| `game_frame_front` | `0x24C5C` | 14 | 29/161 | 10 | 0 | modes 0x15..0x1B, 0x33 and the mode words with no handler |
| `game_frame_start` | `0x24C5C` | 12 | 34/161 | 36 | 0 | modes 0x11 and 0x27..0x2F, all inline in the raw |
| `game_frame_rr` | `0x24C5C` | 3 | 36/161 | 5 | 0 | what the raw reads again after a call |
| `game_loop` | `0x255CC` | 4 | 18/20 | 32 | 0 | under the project's two master-loop pins |

Together the three `0x11D04` rows hit 25/25, the eight `0x24C5C` rows 116/161 (the other 45 are the two
named gaps of §4: `/tmp/pr_d5h/gen_unhit.py` prints the union and the never-hit list), `game_loop` 18/20.
Every callee of every row is a stub unless stated; a stub's `clobbers` is `E.callee_clobbers` (checked for
all 88 stub addresses of the batch by `/tmp/pr_d5h/gen_pins.py`).

### §2.1 `0x11D04`

`mov ah,[0x104b1d]`: with the byte clear, `0x11F28(0)` and `0x11F28(1)` (`test eax,eax`, the whole
register: `l1`'s stub EAX `0x100` is accepted, `@al`), the accepted mask in EDX (`mov edx,1`, `or dl,2`); a
non-zero mask runs `0x32970(0)` and `0x257A4(mask)` and returns at `0x11D46` with no tail. Otherwise the
state word `0xF0A64` (`cmp ax,9; ja 0x11d70`) indexes the table `0x11CDC` (ten dwords: `0x11D70 0x11D88
0x11DA0 0x11DB8 0x11DD0 0x11DE8 0x11E4F 0x11E67 0x11EAC 0x11ED0`) through `jmp cs:[edx+0x11cdc]`, EDX =
4 * the state, EAX reloaded with the timer word `0xF0A6A` and decremented between the guard and the jump.
`switch_cases` does not bound that form, so it is resolved by hand: `diff_emu.RESOLVED_JUMPS[0x11D69] =
(0x11D50, 0x11CDC, 10)`, with its window in the test's `WINDOWS`. Every target ends `0x10DB0`, `0x10E18`,
`0x2BF08`.

One stub EAX serves both poll calls of a case, so `_low` can only produce the masks 0 and 3; `_coin` runs
`0x11F28` real (its own row, `frontend_coin_poll`) over its stubs `0x2C060`/`0x2CA7C` with the pressed dword
`0x1088E4` against the image's masks `0x9ACBC` (`0x01000000`, `0x00000100`), which gives 1, 2 and 3
(`@bits`, `@mask`). `0x32970` (the run clock, host-owned, omitted by the port) runs real in the original from
its zero state, where it changes no byte (no elapsed tick, mode mask 0). `0x11BCC` has no C function (it is
inlined in `game_state_step`) and runs real with its tail jump to `0x2C3FC` recorded.

State 7 and 9 compare the decremented timer zero-extended (`and eax,0xffff; cmp eax,1; jge`): only 0 exits.
`h4`/`hd` (timer 0, so 0xFFFF) and `h5`/`hc` (0x8001, so 0x8000) separate that from a signed 16-bit compare
(`@t7s`, `@t9s`); `h2` (timer 2) is the boundary (`@t7ge`). The limit: a byte store of the state word in
state 5 cannot be told from the word store, because the state word is 5 (high byte 0) on entry.

### §2.2 `0x24C5C`: the BIOS call and the patch

`0x24CFE..0x24EE7` is the keyboard loop: `mov ah,1 … int 0x16` at `0x24D0E` on every path to the mode
switch. The emulator does not model `int`, so unpatched the original stops there on every case
(`unmodeled: int at 0x24D0E`, measured by `/tmp/pr_d5h/try_frame.py` after the calls `0x4F644`, `0x38990`).
Every case of the eight rows therefore pokes the two bytes at `0x24D0E` (`CD 16`) to `39 C0`, `cmp eax,eax`:
ZF = 1, which is the BIOS's "no keystroke" answer to AH = 1. The raw's own code then does the rest
(`0x24D10 jne` not taken, `and dh,0xf; dec dh` gives ZF, `sub eax,eax`, `mov bx,ax; test ebx,ebx; je
0x24eec`). The port side sees the same thing through `game_key_loop`: `input_check_key()` returns 0 on the
empty queue of `diffrun`. The claim of these rows is `0x24C5C` on a frame with no key; the key loop itself
is a named gap (§4). The poke is applied to both sides (the port does not execute code bytes) and the
static scan reads the unpatched image, so the block list is the original's.

### §2.3 `0x24C5C`: the rows

The mode switch `mov ax,[0x104b00]; cmp ax,0x33; ja 0x2540f; and eax,0xffff; jmp cs:[eax*4+0x24b8c]` is a
bounded switch (52 entries, followed by the scan). `jmp 0x65431` at `0x2520B` is the runtime's `longjmp`:
each row lists `0x65431` in its call set so the scan stops there (a jump to a call-set address is not
followed), which leaves 161 leaders; no case reaches it. `diffrun` takes sixteen stubs a case and the
function has 69 direct callees (the queue's list), so the modes are split by what they share:

- **head** (mode 0, table entry `0x2540F`): `0x4F644` unless the mode word is 0x27 (`xor eax,eax; mov
  ax,[0x104b00]; cmp eax,0x27`: the word, `front`'s `m127` is 0x0127); the command block (`0x104B26` clear and
  `0x104B1B` set: per side `test byte [ecx+0x1077f1],0x10`, the word `0x1088E0[side] = 0` or `0x47208(side)`,
  then `0x461DC`); `0x38990` twice with `0x104B26` set, else once; `mov di,[0xef6dc]; inc edi; mov
  [0xef6dc],di` (a word: `a4` wraps 0xFFFF, `@framed`); the update table `call [eax+0xa8644]` over the mask
  dword `0x104AE8` (`a5`: entries 0, 6, 16 = `0x1324C`, `0x25FAC`, `0x26194`; `a6`: the ret-only `0x5D812`
  at 3 and 31, which the port's `fn_resolve` skips, and 6); `0x2A31C`; with the byte `0x104B15` set
  `0x3BB90`, `0x12D48`, per side with a non-zero dword `0x1077A8[side]` `0x186D0(side)` and
  `0x2A690([0x1077B0 + side * 0x94])`, then `0x33F08`.
- **fight**: `0x29B70` (1, 2, 0x20), `0x11D04` (3), `0x26254` (4), `0x25C88` (5), `0x282C4` (7), `0x28468`
  (8), `0x28788` (9), `0x28BD4` (0xA), `0x26254` then `0x28C38` (0xB). Mode 6: `cmp byte [0x104b1d],0; jne`,
  `0x28CC8`, `test eax,eax` (`j5`: 0x100), `dec eax; call 0x28da4`, else `0x26254`. `x0`: the word 0x0103 is
  above 0x33.
- **tails**: 0xC (`0x28CC8` with no `0x104B1D` test, `c4`; `0x28DA4` or `0x27380`) and its tail `0x25487`
  (the `0x104B12` slot's `+0x41` bit 0, the new frame word's bit 1, the pset word at `[0x1014EC] + (the
  record's +0x56 word, zero-extended) * 0x20` saved to `0x104AF6` and replaced by `(old & 0x8000) | 0x1E1`);
  0x21 (`0x26540`; tail `0x3BB90`, `0x12FD8(1)`, per side `0x186D0` and `0x2A690` on a non-zero record,
  `0x33F08`); 0x22/0x23 (`0x26C8C`/`0x26A50`; tail `0x12D48`, `0x186D0(byte 0x104B1A)`, `0x2A690`, `0x33F08`).
- **m25**: 0x24 (`0x26F58`, no tail: `cmp ax,0x25; je`, `@tail24`); 0x25, inline on the byte `0x104B25`
  (`test al,al; jbe` = 0: `0x266AC`, then with `0x1088BD == 8` (`xor eax,eax; mov al`) and both bytes
  `0x1078F0`/`0x1078F1` set `0x4EF8C` and `inc byte [0x104b25]`; 1: the word `0x10889A` decremented, `test
  ax,ax; ja` (unsigned: `s7`'s 0x8000) `0x49C78`, else `0x4F0FC`); tail `0x12FD8(0)`, the two sides,
  `0x33F08`.
- **match**, **front**: one handler a mode; `front` also holds the words with no handler (0x1C, 0x1D,
  0x26: table entries `0x2540F`; 0x34, 0xFFFF, 0x0127: `ja`).
- **start**: 0x11 (`0x104AFE = 0xF0`, `0x1088EE = 0`, both words, the hook `0x104AE4 = 0x259CC`, mode
  0x17); 0x27 (`0x50146(0xC000C000, 0x1E, 0xF)`, `0x2FFC4(0xBCBDC, 0x10, 4)` — its arguments are EAX, EDX and
  ECX: `0x2FFC7 mov edi,eax`, `0x2FFC9 mov esi,edx`, `0x30089 mov [0x107418],ecx`, EBX is overwritten at
  `0x2FFE7` — then `0x4F644` on the results 0, -5, -10); 0x28..0x2F, which the raw has inline and the port
  as `game_mode_28_step`..`game_mode_2f_step`: `v = 0x2D974(0x29)` (not 0x2B) to the dword `0x104528`, the
  byte `0x105B3A = (v & 0x100) >> 4`, the dword `0x1088D0 = (v & 0xF) * 5 + 0x1E`, the byte `0x10452C = (v
  & 0xF0) >> 4`; the dword `0x104AB8` = 1, 2, 3, 3 in 0x28..0x2B and untouched in 0x2C..0x2F; `0x2CA7C(1)` in
  0x2D..0x2F; `0x257A4` with 3, 3, 3, 3, 3, 1, 2, 2.

Each row's unhit blocks are the measured list `D5H_GF_UNHIT[row]` (`/tmp/pr_d5h/gen_unhit.py`, from the
original's executed set); `d5h_gf_unhit` names each with the first row that hits it, or with its named gap.

### §2.4 `0x24C5C`: what the raw reads again (`game_frame_rr`)

Five values are read again after a call; the row's stubs rewrite each, and a port that kept the first
read differs:

| read again at | value | after | stub write | mutant |
|---|---|---|---|---|
| `0x24EEC` | the mode word | the update table | entry 0 (`0x1324C`) stores mode 0x21 | `@modecache` |
| `0x2545C` | the mode word | the mode handler | `0x26540` stores mode 0x22 | `@tailcache` |
| `0x2555E` | the byte `0x104B1A` | `0x186D0` | stores 1 | `@b1acache` (the port before §3) |
| `0x252B9` | the byte `0x1088BD` | `0x266AC` | stores 8 | `@bdearly` |
| `0x252E6` | the byte `0x104B25` | `0x4EF8C` | stores 5 (the `inc` then gives 6) | `@inccache` |

### §2.5 `0x255CC`: the pins and the model

The raw draws `rng(0x7FFF)` once per iteration (`0x256B1`) and once per spin pass (`0x256D6`); the spin
count depends on real time. The project already fixes that for its captures: `tools/title_pin.py` replaces
both calls by `mov eax,0` (PATCHES `0x78505` and `0x7852A`, bytes `e826810300`/`e801810300` to
`b800000000`), and `game_loop` is written against that pinned original (its `PORT:` note at the body draw).
Every case of the `game_loop` row applies the same two patches as pokes. Unpinned and with `0x5D7DC` outside
the call set the original stops `unmodeled: call 0x5D7DC from 0x256B1` (`/tmp/pr_d5h/try_loop.py`).

The loop waits on three things the emulator does not have, and the stubs model them (their `writes`):
the quit flag `0xA81A8` (render entry 0, `0x4F4E8`, stores 1), a second iteration with other inputs (the
effects step `0x134C0` stores the render mask `0x104AEC = 0x11`), and the timer ISR `0x1BDF4` stepping the
tick `0x101508` (the audio service `0x1CF20` stores `0x10001`, so `0x256CC` never equals and the spin never
runs; the high word also separates a word compare of the pair, `@gatew`). `k0`, `k2`, `k3` run two
iterations — the first presents (`0x10150C == 0x101508` after the prologue's zeroing of both, which `@no508`
/`@no50c` break), the second has the gate false and the render entries armed — and `k1` one.

The present itself is host-owned: the cases keep the copy flag `0x1014FC` clear, so the raw runs the dirty
blit `0x501A3` (allowed, real). It compares the buffers `[0xE87A0]` and `[0xE87A4]` dword by dword and
copies the differing dwords of the second to `0xA0000 + offset` (`0x506B5 mov [ebx],eax`). The cases store
`0xE87A4 = 0xA0000`, the aperture itself, so every such copy rewrites the bytes it read and the blit changes
nothing: it is executed, its output is not claimed. `0xE87A0 = 0xB0000` differs, so the swap `0x50188`
(allowed, real, its own row) is visible (`@noswap`, `@swapearly`).

Not claimed, because the port adds them: the host key fill at the top of `game_loop` (it writes the two
bitmap bytes `[0x101514] + 0x2D8/0x2D9`, zeros over zeros here), `gfx_present` (it reads the back buffer and
returns in `host_present_rgb` with no window), the two dump hooks (unset environment) and
`host_quit_requested`. The stubs `0x1C3FC` and `0x14328` take no argument in the port (`render_list_sort
(void)`, `render_list(void)`), so the raw's EAX = `0x105B44`, EDX = `0xA87CC`, EBX = `0xA87D4` at those calls
are not compared. A render or update table entry receives EAX = 4 * its index in the raw (`mov eax,ebx`
/`mov eax,edx`); the C entries take no argument, so that is not compared either.

## §3 Corrections (raw over port)

Each is its own commit, before the row that proves it; the row's named mutant is the old port.

| # | address | old | new | the instruction | row, cases, mutant |
|---|---|---|---|---|---|
| 1 | `0x11EF6` | state 9 stored the state and left the call out (`PORT:` note) | `if (DSB(0xF0A71) != 0) attract_state_reset()` after the store | `0x11EE6 mov dl,[0xf0a71]`, `0x11EEC mov [0xf0a64],ax`, `0x11EF2 test dl,dl`, `0x11EF4 je 0x11f0e`, `0x11EF6 call 0x10ee4` | `game_state_step_high`, `ha` `he`, `@n9reset` |
| 2 | `0x2555E` | the 0x22/0x23 tail read `0x104B1A` once | read again after `0x186D0` | `0x25552 mov al,[0x104b1a]`, `0x25559 call 0x186d0`, `0x2555E mov dl,[0x104b1a]` | `game_frame_rr`, `r0`, `@b1acache` |
| 3 | `0x25632` | `DSD(0x104AF4)++` | a word increment | `inc word ptr [0x104af4]` (`66 FF 05 F4 4A 10 00`) | `game_loop`, `k0` `k1` `k3`, `@af4d` |
| 4 | `0x2569D` | `DSD(0x1014FC) = 0` | a byte store | `mov byte ptr [0x1014fc],bl` (`88 1D FC 14 10 00`) | `game_loop`, every case, `@fcd` |
| 5 | `0x256C0` | `0x10150C` stepped before the audio service | after it | `0x256B6 call 0x1cf20`, `0x256C0 inc dword ptr [0x10150c]` | `game_loop`, every case, `@incearly` |

Measured before each fix: (1) `ha: call #0: original 0x10EE4(), port 0x10DB0()`; (2) `r0: call #7: original
0x2A690(0x10A400), port 0x2A690(0x10A300)`; (3)-(5) `k0: … first difference at 0x104AF6: original
unchanged, port 0x5B` and `k0: byte 0x1014FD: original unchanged, port 0x00` (the order difference of (5)
sat behind those two in the call-memory lines and is what `@incearly` reproduces).

What each changes in the game:
- (1) is behaviour. A state-9 hold entered with `0xF0A71` set (the two per-state tails `0x10DB0`/`0x10E18`
  set it on their input combinations) ends in the attract reset (`0x10EE4`: mode 3, state 0, actors reset)
  where the port handed the state `0xF0A6C` names. `test_game.c`'s state-9 countdown had seeded `0xF0A71 = 1`
  only to quiet the two tails; it now runs the hold with the byte clear and the held dword `0x1088D8`
  zeroed, with the same assertions. The set arm is covered by the row, not by a unit check.
- (2) cannot be seen unless `0x186D0` stores `0x104B1A`; no such store is known.
- (3) the dword increment carried into `0x104AF6` (the pset word the mode-0xC tail saves at `0x254EE` and
  `0x27380`/`0x299E8` restore) when the word wrapped: once in 65536 frames.
- (4) the dword store zeroed `0x1014FD..0x1014FF`; no instruction of the image references those bytes (the
  flag's sites are `0x1B3F8`, `0x25677`, `0x2569D`, `0x2EAA9`, `0x2EAC9`, all byte operands). The raw stores
  only in the full-copy branch; the port stores on every presented frame, which is the same thing when the
  byte is already clear.
- (5) neither `0x1CF20`'s tree (`0x1CB18`, `0x1C930`) nor `game_audio_service` reads the count.

**None of the five is proved against the oracles**: this batch does not run them.

## §4 Named gaps and unhit blocks

- **`0x24C5C`, the keyboard loop** (44 blocks, `0x24D26..0x24EE0`, `D5H_GF_KEYS`): reached only on a
  keystroke from the BIOS (`int 0x16` at `0x24D0E`, `0x24D2E`, `0x24E54`). A static patch cannot give a
  finite queue: every path returns to the peek at `0x24D08`, so a patch that answers "a key" loops for ever.
  The port's `game_key_loop` is covered by its unit tests and the gp-keys oracle, not by a row.
- **`0x24C5C`, the restart arm** (`0x25201`): `mov edx,1; mov eax,0x1044f4; jmp 0x65431`. The runtime's
  `longjmp` does not return into `0x24C5C` (it resumes `0x20C24` on `0x20C10`'s frame); a stub cannot return
  from it either, because the jump is taken with five saved registers on top of the stack and no return
  address. The port side is `_Noreturn` and cannot carry a seam.
- **`0x255CC`, the full copy** (`0x25680`): `rep movs` of 0xFA00 bytes to `0xA0000` through EDI. The port
  never writes `mem[0xA0000]` (the aperture rule).
- **`0x255CC`, the spin** (`0x256D4`): waits on the ISR; §2.5. The loop's back edge is taken (two
  iterations); a third iteration is not exercised.
- **`0x20C10` is not a row.** `call 0x653fc` at `0x20C1F` is the runtime's `setjmp(0x1044F4)`: without it in
  the call set the original stops `unmodeled: call 0x653FC from 0x20C1F` after the one call `0x2F9CC`; with
  it allowed it writes the caller's frame into the buffer (`0x10450C..0x10450E = 24 0C 02`, the return
  address `0x20C24`; `0x104510..0x104513 = CC FF FE 7F`, the emulator's private ESP), bytes the port cannot
  produce. Past it the function keeps a 0x28-byte record on its own stack (`0x20CF0 mov eax,esp` for
  `0x1AEE0`/`0x1AF64`, `0x20D00 mov ax,[esp+0x24]`), probes the game port (`0x4FBBB`, host-owned) and ends by
  calling the master loop (`0x20DE8`), which the port calls from `game_main`. The port's
  `game_init_resume` is the tail `0x20C24..0x20DE3` with the calls record named-gaps-b §B.11 lists left out
  by design, and the harness cannot enter at `0x20C24` (the frame below the return address would have to be
  seeded: a case sets four stack dwords, the tail pops twelve). With every direct callee allowed in turn the
  run faults (`UC_ERR_READ_UNMAPPED`) inside `0x2BAF4`'s tree, on the unset pool pointers.
- **`0x1B120` is not a row.** Its first call, six pushes and two `mov`s in, is the runtime's open of the
  INDEX file: `0x1B130 call 0x61c60`, which reaches DOS at `int 0x21` (`unmodeled: int at 0x6F53A`, through
  `0x61C45 0x6BDBC 0x61423 0x6B36F 0x6B4B3 0x6B19F 0x6F506`). No byte is changed before it, so there is no
  part to row. The port's `res_load_index` reads the file through the host (`host.c`).
- **`0x1B3AC` is not a row.** The first call is the runtime's `0x62386` at `0x1B3C3` (the entry's 12-byte
  name copied to the frame); allowed, the run reaches `0x1B403 call 0x61c60` and stops at the same `int at
  0x6F53A`, having changed two bytes: `[0x101514] + 0x253 = 1` (`0x1B3D3`) and the copy flag `0x1014FC = 1`
  (`0x1B3F8`). The port has no function with this contract: `res_load_present(draw, index)` is `static`,
  takes the entry's index instead of its record, and models the read's duration by stepping the ISR tick
  (`PORT:`, record §45-A), which the raw does not do.
- **`0x1BEC4` is not a row.** `cmp eax,2; jne 0x1bee3` (the `-f` probe through the runtime's `0x623B0`),
  then the first callee every path reaches: `0x1BEE3 call 0x4fba2`, the video mode query, which stops at
  `int 0x10` (`unmodeled: int at 0x4FBAD`). Of its 24 callees three have rows (`0x1D0BC`, `0x336C0`,
  `0x51F45`); the others are host-owned (`0x109A0`, `0x10C30`, `0x10D0C`, `0x10D34`, `0x1ACA8`, `0x1AD64`,
  `0x1ADAC`, `0x1ADE4`, `0x1D290`, `0x1E2A0`, `0x4FB98`, `0x5004A`), runtime (`0x61A70`, `0x623B0`), earlier
  named gaps (`0x1BE30`, `0x1CF40`, `0x47370`, `0x4FBA2`) or this batch's other non-rows (`0x1B120`,
  `0x1B3AC`, `0x20C10`). The port's `game_init` is that chain with the host-owned calls replaced under
  `PORT:` notes, so a row would compare calls the port does not make.

The four non-row lines are measured by `/tmp/pr_d5h/d5gaps.py` (`E.run_original`, each `call X from Y` stop
allowed in turn until the run stops on something else). No harness `gap=` spec was added, for d4d's reason:
the suite's `test_every_ported_function_agrees…` excuses only `host_1b890` by name, an existing line.

## §5 Store-sweep findings

- Every field the three functions write is seeded with a value that differs from what is written, between
  seeded neighbours: the state block `0xF0A62..0xF0A73` and the bytes `0x104B15`/`0x104B1B` (`d5h_ss_case`);
  the command words `0x1088E0`, the frame word `0xEF6DC`, the pset word and `0x104AF6`, the mode-0x25 bytes
  and word, the start modes' `0x104528`/`0x10452C`/`0x105B3A`/`0x1088D0`/`0x104AB8` and mode 0x11's four
  stores (`d5h_gf_case` and its helpers); the tick pair, `0x104AF4`/`0x104AF6`, `0x1014FC..0x1014FF` and the
  two buffer pointers (`d5h_loop_case`). A dropped store is a mutant of each row (`@f6f`, `@b1b`, `@n7state`,
  `@cmdzero`, `@csave`, `@nov`, `@noab8`, `@noaf4`, `@noinc`, …) and each is detected.
- Width, found by the neighbours: corrections 3 and 4. Width mutants that are caught: `@tw`, `@c6w`,
  `@t7w`, `@cmdzerob`, `@framed`, `@cbyte`, `@decb`, `@b3aw`, `@b2cw`, `@d0w`, `@afew`, `@eew`, `@gatew`.
- **Seen, not corrected** (no row can carry it, §4): `res_load_present` stores `DSD(0x1014FC) = 1` where the
  raw has `mov byte [0x1014fc],dl` (`0x1B3F8`). The three bytes above the flag are referenced nowhere, so
  nothing reads the difference. The same function does not store the byte `[0x101514] + 0x253` (1 at
  `0x1B3D3` and `0x1B42E`, 0 at `0x1B451`), which ends 0 when the read succeeds.
- Not separable, named: a byte store of the state word in `0x11D04`'s state 5 (§2.1); a word increment of
  `0x10150C` (the prologue zeroes it, a carry needs 65536 iterations); a word read of the render mask
  `0x104AEC` (the entries above bit 4 are all the ret-only `0x5D812`); the results of `0x2FFC4` other than
  0, -5 and -10 (the restart arm).
- No mutant or binding uses an index-like sentinel: every address a mutant derives is a constant or a
  side 0/1 slot, so nothing leaves `mem[]`.

## §6 Queue entries

| entry | result |
|---|---|
| `0x11D04` | three rows, 25/25 between them; correction 1 |
| `0x255CC` | one row, 18/20, under the two pins; corrections 3, 4, 5 |
| `0x20C10` | not a row, §4 (`call 0x653FC from 0x20C1F`) |
| `0x1B3AC` | not a row, §4 (`int at 0x6F53A` through `0x61C60`; no comparable port function) |
| `0x1B120` | not a row, §4 (the first call is the runtime's open) |
| `0x24C5C` | eight rows, 116/161 between them, under the no-key patch; correction 2 |
| `0x1BEC4` | not a row, §4 (`int at 0x4FBAD` in its first callee) |

Seams added (79, each the callee's first statement, `PR_SEAM0` unless shown):
`0x10DB0 0x10E18 0x10EE4 0x11000 0x11578 0x11A8C 0x11D04 0x11F6C 0x121A0 0x12484 0x12D48 0x12FD8(track)
0x1324C 0x134C0 0x14328 0x1C3FC 0x1C470 0x1CF20 0x1EA08 0x1EEB0 0x208F8 0x24C5C 0x257A4(players) 0x25AE8
0x25C88 0x25FAC 0x26194 0x26254 0x263F4 0x26540 0x266AC 0x26A50 0x26C8C 0x26F58 0x27380 0x274FC 0x277C0
0x27A2C 0x282C4 0x28468 0x28788 0x28BD4 0x28C38 0x28CC8(RET0) 0x28DA4(side) 0x29328 0x29638 0x292AC 0x296B8
0x299E8 0x29B70 0x2A31C 0x2BF08 0x2C06C((u32)row) 0x2FFC4(RET: table, stride, flags) 0x33F08 0x38990 0x389C4
0x3BB90(RET0) 0x41C28 0x424E8 0x438B4 0x461DC 0x47208(side) 0x49C78 0x4EF8C 0x4F0FC 0x4F24C 0x4F2B0 0x4F318
0x4F4E8 0x4F5C8 0x4F644 0x4F6E8 0x4F704 0x4F9A0 0x4F9C8 0x500C4(RET0) 0x50146(mask, first, next)`.
The 47 existing rows that enter or call one of them were re-run and stay VERIFIED.

Harness lines that are not additive by position, as the brief allows: `diff_emu.RESOLVED_JUMPS[0x11D69]`
(appended to `tools/diff_emu.py`), its entry at the top of `RealFunctionTests.WINDOWS`, and the batch's
stub addresses at the top of the expected dictionary of
`test_each_stub_declares_the_registers_its_callee_clobbers` (both between `# D5 d5h` marker comments in
`tools/tests/test_diff_verify.py`).
