# Track D, wave 6, batch d6a — derivation record

Rows for the queue `.superpowers/sdd/2026-10-11-reverse-d1/d6a-queue.txt`: eleven of the 28 functions that
waves 1 to 5 left as named gaps (track record `2026-10-11-reverse-d-derivations.md` §D.8, §D.9, §D.11). Branch
`reverse-d6a` off `main` (`5fee3522`). Ground truth: the fixed-up image `build/diffrun --image-out` dumps
(linear addresses), disassembled with capstone 5.0.7. Every figure in §2 is measured with `python3
tools/diff_verify.py --image … --function <name> --self-check`; the pins in `tools/tests/test_diff_verify.py`
(`D6A_MASKS`/`D6A_KINDS`/`D6A_CASES`) come from `verify_all` on the batch's own specs, each mutant run ten
times. The claim of each row is the track's: equivalence on the exercised blocks and inputs only, with the
callees stubbed or run as stated — and, for the rows under a code poke, with the poke and what it blinds the
row to as §2 and §4 state them.

Result: **10 of the 11 entries have a row** (12 specs, 57 cases, 143 mutants) and 1 stays a gap. **Three
address-proved port corrections** (§3). Three rows are full rows, seven run the original under code pokes
(four of those are also partial: the port does less than the raw by a documented `PORT:` deviation).

| entry | port function | outcome | what decides it |
|---|---|---|---|
| `0x1C5E8` | `text_blit_glyph` (actors.c) | **full row** (2 specs) | The obstacle was the harness shape: the function was `static` and `0x51ED8` had no seam. `static` removed; the node, the first `0x38` bytes of the raw's frame (`mov eax,esp` `0x1C63B`), is compared by value at the `0x51ED8` stub. |
| `0x31A78` | `svc_buttons_draw` (svcmenu.c) | **full row** | The name is built in the raw's frame (`mov ebx,esp` `0x31AB0`, `0x31AF1`, `0x31B14`, `0x31B4B`). `0x3157C`, `0x2F198`, `0x2F830`, `0x2F0F0` run on both sides and the name is compared character by character at the `0x2F5A0` stub; the binding hides the port's scratch (§6). |
| `0x305FC` | `config_code_row` (config.c) | **full row** (2 specs) | The one-character strings (`lea ebx,[esp+0x30]` `0x306F9`) and the record (`mov edx,esp` `0x306AB`) are the raw's frame. The same text closure; `0x2DCA0` runs on both sides and the record is compared by what it stores in table 2. |
| `0x2CADC` | `config_set_defaults` (config.c) | **poked row, partial** | Three pokes (`0x2CAF3`, `0x2CB5F`, `0x2CB69`): the three calls the port leaves out by its `PORT:` note are nops. The seven calls the port makes are compared. |
| `0x2D6F8` | `config_validate` (config.c) | **poked row, partial** (24/43 blocks) | Two pokes (`0x2D912`, `0x2D919`: the deferred validators). The storage load and reads run from their bytes. The version arms and the stored-image arm are named unhit: the port's storage-absent model (`PORT:`) cannot reach them. |
| `0x2EA74` | `config_screen_wait_zero` (config.c) | **poked row** | One poke: `int 0x16` at `0x2EB15` as `cmp eax,eax` (it falls into `0x2EA78`). |
| `0x2EA78` | `config_screen_wait` (config.c) | **poked row** (15/23 blocks) | The same poke, `0x24C5C`'s: exact for the no-key path. The key loop's eight blocks are named unhit. |
| `0x249F0` | `game_quit_prompt` (flow.c) | **poked row** (16/18 blocks) | One poke: `int 0x16` (AH = 0) at `0x24A64` as `mov al,K`; the code keeps AL alone (`0x24A71/0x24A76`). The soft-restart arm is named unhit. |
| `0x4FBA2` | `int10h_query` (flow.c, static) | **stays a gap** | `int 0x10` at `0x4FBAD`: the function returns the BIOS's AL and nothing else (§4). |
| `0x1C470` | `gfx_flush_palette` (gfx.c) | **poked row, partial** | Five pokes: the retrace read as `mov al,8`, the four `out dx,al` as nops. Only the dirty list's bookkeeping is compared; blind to every DAC byte. |
| `0x52106` | `gfx_screen_reset` (gfx.c) | **poked row, partial** | Six pokes: the retrace read, the four `out dx,al`, and the fill of the aperture (`call 0x51f72` `0x52151`). The two tick stores and the two offscreen fills are compared. |

## §1 The members

Callers are a rel32 scan of the code object for `call`/`jmp` to each entry. "Mask" is the row's `eax_mask`.

| addr | port function | size | callers and what they read of EAX |
|---|---|---|---|
| `0x1C5E8` | `text_blit_glyph` | 113 | One, `0x1C6BD` in `0x1C65C`: `mov esi,eax` (`0x1C6C2`), which becomes `0x1C65C`'s own return (`mov eax,esi` `0x1C6CB`), which its one caller overwrites (`mov eax,esp` `0x1B3F6`, d5b record). The port's function is void. Mask 0. |
| `0x31A78` | `svc_buttons_draw` | 137 + the four arms to `0x31B80` | Four: `0x321F2` (then `0x3220C..0x3222B`: `xor eax,eax` or `mov eax,1`), `0x32230` (`mov eax,0xa` `0x32252`), `0x324A9` (`mov eax,[0x101514]` `0x324AE`), `0x324DC` (`call 0x2ea74`, `xor eax,eax`). Mask 0. |
| `0x305FC` | `config_code_row` | 287 | Two: `0x2FE2A` (`call 0x2ea74` `0x2FE2F`) and `0x305E2` (`xor eax,eax` `0x305E7`). Mask 0. |
| `0x2CADC` | `config_set_defaults` | 150 | One, `0x2D886` (`xor eax,eax` `0x2D88B`). Mask 0. |
| `0x2D6F8` | `config_validate` | 633 | One, `0x2FA0B` (`mov eax,0x2a` `0x2FA10`). Mask 0. |
| `0x2EA78` | `config_screen_wait` | 263 | Five: `0x24A3C` (`mov eax,0x1f0`), `0x2CB5F` (`mov eax,0xffffffff`), `0x31FBF` (`xor eax,eax`), `0x1A38C` (`call 0x2ebbc`, which loads EAX from `0x105F28`), `0x24E4B` (`xor ah,ah; mov dh,ah; int 0x16`: the BIOS's AX; `mov al,bl` `0x24E65` is the next use). Mask 0. |
| `0x2EA74` | `config_screen_wait_zero` | 4 (falls into `0x2EA78`) | Eighteen. Thirteen load EAX first (`0x2D016`, `0x2FA66`, `0x2FACA`/`0x2FD20` after `0x2FE2F`, `0x2FFF6`, `0x30755`, `0x30B25`, `0x30B4F`, `0x30E63`, `0x31284`, `0x3129A`, `0x324FB`/`0x3252B`, `0x32539`, `0x33282`); three call `0x2EB80`, which loads it (`0x32E36`, `0x3306B`, `0x33247`); **two hand it on**: `0x313F5` returns it (`0x313FA..0x31403`, `0x31138`'s return) and `0x32F2D` reaches a `ret` on one path (`0x32F52`). Both are menu callbacks; the port restates the value there (`return 0xFFFFFFFFu` in `svc_handicap`). The raw's EAX is -1 after the loop (`mov eax,ecx; dec ecx; cmp eax,-1; jg` `0x2EB6E..0x2EB74`; measured: -1 for n = 0 and 2, -2 for n = -2, the `0x1C470` stub's EAX for n = -1). The port's functions are void: **mask 0 and EAX is not compared** by these two rows. |
| `0x249F0` | `game_quit_prompt` | 280 | Three, all in `0x24C5C`: `0x24DDF`, `0x24EAD`, `0x24EC5`, each back to the dispatcher's `mov ah,1; mov dh,ah; int 0x16` (`0x24D08..0x24D0E`), after which AX is the BIOS's and the upper half is masked (the debris_spawn row's argument, d1g). Mask 0. |
| `0x4FBA2` | `int10h_query` | 25 | Two, `0x1BEE3` and `0x1BFEF` (in `0x1BEC4`, itself a named gap on this callee). Not a row. |
| `0x1C470` | `gfx_flush_palette` | 143 | Three: `0x1C6C6` (`mov eax,esi` `0x1C6CB`), `0x25672` (`0x2568E` loads EAX, or the dirty blit `0x501A3`), `0x2EADB` (`xor eax,eax` `0x2EAF9`, or `0x2EA78`'s return for n = -1, above). Mask 0. |
| `0x52106` | `gfx_screen_reset` | 83 | Seven: `0x1C74D` (`call 0x62756`), `0x1C873` (**returned**, `0x1C881`: the movie player's own return, a host replacement), `0x2BBEA` (`call 0x336c0`), `0x32BF5`, `0x32E93`, `0x330B1`, `0x332E4` (`call 0x2f99c`). The raw's EAX is `0xAFA00` (`0x51F72` leaves it advanced past the aperture). The port's function is void and the third fill is poked out: **mask 0 and EAX is not compared**. |

## §2 The rows

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `text_blit_glyph` | `0x1C5E8` | 5 | 4/4 | 11/11 | 0 | AL = the character (`and eax,0xff` `0x1C5EF`; `g1` has `0x123456C1`), EDX and EBX = the addresses of the column and the row. The font entry `0xBCD7C + 4·ch`: the id word, zero-extended with bit 15 kept, goes to `0x1C528(frame node, id)` (run on both sides); the width byte is kept in ESI (`0x1C5FF`). `0x33754(0x80997C)`'s EAX goes to the node's `+0x1C`, the clips `+0x28..+0x34` are zeroed, `+0`/`+4` = the column and the row (read after the acquire, `0x1C630`/`0x1C635`), `0x51ED8(node)` by value (thirteen fields); the column advances `0xF` when the width byte is `0x10` (`g2` `0x11`, `g3` `0x0F`, `g4` `0x90`), else 8. Stubs: `0x33754`, `0x51ED8`. |
| `text_blit_glyph_rr` | `0x1C5E8` | 2 | 4/4 | 4/4 | 0 | The `0x33754` stub rewrites the column, the row and the font entry's width byte; the `0x51ED8` stub rewrites the column. `rr0` separates the width byte the raw loaded before the calls from the one a re-read finds (§3.1). |
| `svc_buttons_draw` | `0x31A78` | 4 | 14/14 | 15/15 | 0 | EAX = side (whole, `test eax,eax` `0x31A7E`: `b2` has `0x100`), EDX = the key-config record, EBX = col, ECX = row. Four key words from `+0xA` or `+0x1C`, each named wrapped by `0x3157C(key, 0, frame)`; a key with no name leaves the buffer as the key before left it (`b0`, `b1`). half = strlen `shr` 1 (names of 3 to 7 characters). Key 0 at (col - 5 - half, row + 8), key 1 at (col + 5 - half, row + 8), keys 2 and 3 at row + `0xC` (jump table `0x31A68`, resolved by hand, §6), each `0x2F198(.., name, 0x4000)`. `b3`'s first column is -1, which `0x2F198` centres. Stub: `0x2F5A0` (by value). |
| `config_code_row` | `0x305FC` | 11 | 14/14 | 23/23 | 0 | EAX = col, EDX = row. Flags byte `0x107453` non-zero: the second text at mode `0x1000`; the flags byte is read again (`0x3063D`) and bit 1 returns (`a0`, `a1`). Else the first three characters and the decimal number from the fourth on, each in '0'..'9' signed (`b2` stops on ':', `b3` on '/', `b1` runs to the eighth column), go to `0x2DCA0(0, record, 2)` and bit 1 is set after it. Flags byte 0: eight `0x2F198(col + i, row, one character, mode)`, the first text at `0x4000` while i is below the count byte (zero-extended; `c3` has `0xFF`), else the second at `0x2000`. Stubs: `0x2F5A0` (by value), `0x2D4EC`. |
| `config_code_row_rr` | `0x305FC` | 2 | 8/14 (6 named) | 2/2 | 0 | The `0x2F5A0` stub stores the flags byte `0x03` and the count byte 6: `r0` returns on the flags byte it reads after the draw, `r1` reads the count every column. The insert arm's six blocks are the first row's. |
| `config_set_defaults` | `0x2CADC` | 3 | 1/1 | 12/12 | 0 | **Under three pokes** (§4). `0x2CCD0(0xA2EB4)`, whose EAX goes whole to `0x2DA0C(0x29, ..)`; `0x2DA0C(0x35, 0xA0)`; `0x2DA0C(0x37, 0xA0)`; `0x2D974(0x2A)`, whose EAX with AL `& 0xFC | 3` goes to `0x2DA0C(0x2A, ..)` (§3.2: `t1` `0x12345678`, `t2` `0x100`); `0x1AE20()`. All five are stubs. |
| `config_validate` | `0x2D6F8` | 5 | 24/43 (19 named) | 20/20 | 0 | **Under two pokes** (§4). The bytes `0x105DA5`/`0x105DA4` = 0; a failed read with the byte `0x2D490` zero returns (`v3`); the defaults arm: `0x105DD8 |= 6`, the fill of `0x105DE1` (0x53), `|= 1`, the fill of `0x105DB0` (0x26), `0x105E2F` = 0, `0x2CADC()`, the magic `0x9C94D2C4` stored after it, `0x2D4EC(1)`, `(2)`, `(0)`; the tail: `0x105DA7` = 1 and `0x2D974(0x24)`. `v0`/`v1`: the image's empty storage (both reads return 1); `v2`/`v4`: two unreadable copies (both return -1, the port's model). Run from the original's bytes: `0x2D638`, `0x2E990`, `0x61A70`, `0x65490`, `0x654C7`, `0x2D4B4`. Stubs: `0x2CADC`, `0x2D4EC`, `0x2D974`. |
| `config_screen_wait` | `0x2EA78` | 5 | 15/23 (8 named) | 16/16 | 0 (EAX not compared) | **Under one poke** (§4). EAX = n. The latch dword `0x105F30` = 0; `0x2A31C`, `0x1C3FC`, `0x14328`; the copy flag byte `0x1014FC` set: the full copy and the flag byte = 0, else the dirty blit; the swap `0x50188`; `0x1C470`. n = -1 returns (`w0`, `w1`). Else n + 2 passes for n >= 0 (`w2`: 2, `w3`: 4) and one for n = -2 (`w4`): the wait on the frame word `0xEF6DE` (`0x1CF20` while unchanged), `0x500C4`, the peek. Stubs: `0x2A31C`, `0x1C3FC`, `0x14328`, `0x1C470`, `0x1CF20` (stores the frame word `0x2222`), `0x500C4` (stores `0x1111`). Run: `0x655FF`, `0x501A3`, `0x50188`. |
| `config_screen_wait_zero` | `0x2EA74` | 2 | 15/23 (8 named) | 3/3 | 0 (EAX not compared) | The same poke and stubs. `xor eax,eax; mov eax,eax` and the fall into `0x2EA78`: n = 0 whatever EAX held (`z1`: `0xFFFFFFFF`). |
| `game_quit_prompt` | `0x249F0` | 11 | 16/18 (2 named) | 20/20 | 0 | **Under one poke** (§4). AL = hard_quit (`q4` `0x100`, `q0` `0x12345600`, `qa` `0xFFFFFF80`). The prompt flag `0x104B22` = 1, `0x1D250()`, `0x2F198(-1, 0xA, string 0x1EE or 0x1EF, 0x1000)`, `0x2EA78(-1)`; the first bytes of the strings `0x1F0`/`0x1F1` sign-extended; the key through the runtime's toupper `0x653ED` (run: `q5` '{', `q6` '`', `q7` 'z', `q8` 'a'). Yes (tested first: `q9`) clears the flag and, with AL zero, sets the quit flag `0xA81A8`; no clears the flag and calls `0x2F280(-1, 0xA, the string again, 0x1000)`; then `0x1D270()`. Stubs: `0x1D250`, `0x2F198` (by value), `0x2EA78`, `0x2F280`, `0x1D270`. Run: `0x1C500` and its tree, `0x653ED`. |
| `gfx_flush_palette` | `0x1C470` | 4 | 11/11 | 10/10 | 0 | **Under five pokes** (§4). For each 16-byte record from `0x107498` to the head `[0x107798]`: the dword `+4` plus the dword `+0xC` above `0x100`, signed (`f3`: a sum of `0x80000000`; `f2`: exactly `0x100` and `0x101`), takes the excess off the dword `+0xC`; `+4` = -1; then the head = `0x107498`. `f0` is the empty list. Run: `0x1B544`. **Blind to every byte sent to the DAC.** |
| `gfx_screen_reset` | `0x52106` | 3 | 5/5 | 7/7 | 0 (EAX not compared) | **Under six pokes** (§4). EAX = ticks. The dwords `0x101508` and `0x10150C` = EAX; `0x51F72([0x1014E8], EAX)` and `0x51F72([0x1014E4], EAX)` (run on both sides, 0xFA00 bytes of the image's own data each). **Blind to the DAC and to the aperture fill.** |

Totals: 12 specs, 57 cases, 143 mutants; 131 of the twelve specs' 174 blocks hit, the 43 others named in the specs (`config_code_row_rr`'s six are hit by its first row; the two wait rows name the same eight).

## §3 Corrections

Each is its own commit `port: D6 d6a: …` with a row that was MISMATCH before it and a mutant that is the old
code.

| # | function | raw | old | new | evidence |
|---|---|---|---|---|---|
| 1 | `text_blit_glyph` (actors.c) | `0x1C5FF movzx esi,byte [eax+2]` (before `0x1C528`, `0x33754`, `0x51ED8`); `0x1C642 cmp esi,0x10` | `*col += (DSB(e + 2u) == 0x10u) ? 0xfu : 8;` after the three calls | `const u32 width = DSB(e + 2u);` before them, tested after | `text_blit_glyph_rr` `rr0`: `byte 0x10A204: original 0x08, port 0x0F`. Mutant `@wreread`. |
| 2 | `config_set_defaults` (config.c) | `0x2CB31 call 0x2d974; 0x2CB36 and al,0xfc; 0x2CB38 or al,3; 0x2CB3F mov edx,eax; 0x2CB4B call 0x2da0c` | `config_field_set(0x2Au, (v2a & 0xFCu) | 3u)` | `(v2a & 0xFFFFFFFCu) | 3u` | `config_set_defaults` `t1`, `t2`: `call #5: original 0x2DA0C(0x2A, 0x1234567B), port 0x2DA0C(0x2A, 0x7B)`. Mutant `@al`. |
| 3 | `config_validate` (config.c) | `0x2D83A mov ebx,0x53; 0x2D845 mov eax,0x105de1; 0x2D84D xor edx,edx; 0x2D855 call 0x61a70` and `0x2D85A mov ebx,0x26; 0x2D865 mov eax,0x105db0; 0x2D86D xor edx,edx; 0x2D875 call 0x61a70` | neither fill (the comment took them for "the unported pset pool") | `mem_fill(0x105DE1, 0, 0x53)` between the two ORs of `0x105DD8`, `mem_fill(0x105DB0, 0, 0x26)` after the second | `config_validate` `v1`, `v2`, `v4`: the 121 seeded bytes `0x105DB0..0x105DD5` and `0x105DE1..0x105E33`, `original 0x00, port unchanged`. Mutants `@nofill` (the old code), `@nofill1`, `@nofill2`. |

What they change in play, as far as measured. (1) Nothing a callee of the port does writes the font table, so
the byte read is the same. (2) Field `0x2A`'s descriptor (`0x2D300 + 4·0x2A` = `0x00001B80`) is a bit field;
`0x2D974` returns its value, whose bits 8..31 I did not show to be zero from the bytes: **unverified**. (3)
Both ranges are zero when `game_init` reaches `config_validate` on a first run: the 820 headless frames
(`prageport --check 820`, every `.idx` and `.pal`) are byte-identical before and after corrections 2 and 3
(sha256 of their concatenation `6e2fc369d76d4a28…` both times). Correction 1 was not compared that way. d5g's
record §3 had seen (3) and left it for the batch that rows `0x2D6F8`.

**Not corrections.** `config_code_row`, `svc_buttons_draw`, `game_quit_prompt`, `config_screen_wait`,
`gfx_flush_palette` and `gfx_screen_reset` agree with the original as they stand, on the claims of §2.

## §4 Named gaps, pokes and unhit blocks

### The pokes

A code poke is an ordinary `Case.pokes` entry at a code address (the mechanism of d5h's `0x24C5C` and
`game_loop` rows; the port side stores the same bytes in `mem[]`, where nothing executes them). For each: what
it replaces, why the row is exact where it claims to be, and what the row is blind to.

| row | poke | exact for | blind to |
|---|---|---|---|
| `config_screen_wait`, `config_screen_wait_zero` | `0x2EB15`: `int 0x16` (AH = 1) -> `cmp eax,eax` (`39 C0`) | The no-key path: ZF = 1 is the BIOS's "no keystroke"; the code then zeroes EAX itself (`0x2EB17..0x2EB20`) and leaves (`je 0x2eb6e` `0x2EB27`). ZF is all it takes from the BIOS there. Unpoked: `unmodeled: int at 0x2EB15` (measured, `w2`). The n = -1 cases never reach it (`w0` returns unpoked too, measured). | The key loop `0x2EB29..0x2EB6C` (eight blocks, named): a static poke cannot drain a queue. |
| `game_quit_prompt` | `0x24A64`: `int 0x16` (AH = 0) -> `mov al,K` (`B0 K`) | A BIOS answer whose ASCII byte is K, any scan code: `and eax,0xffff; and eax,0xff` (`0x24A71/0x24A76`) keeps AL alone, and both flag outcomes of the wrapper reach `0x24A71` with AX kept (for AH = 0, `and dh,0xf; dec dh` is `0xFF` and `jne` jumps). Unpoked: `unmodeled: int at 0x24A64` (measured). The port reads its BIOS-style queue (`input.c`), which the binding loads with the same K, read from the poked byte `0x24A65`. | A key that is neither answer (it reads another key; a static poke would answer the same for ever), and so a second key. |
| `config_set_defaults` | `0x2CAF3`, `0x2CB5F`, `0x2CB69`: `call 0x2f198`, `call 0x2ea78`, `call 0x2f280` -> five nops each | The seven remaining calls: nothing the remaining instructions read comes from the three (`0x2CAF8` loads EAX; `pop edx; pop ecx; pop ebx; ret` follow `0x2CB69`). The form of `game_loop`'s two pins (calls the port leaves out by a `PORT:` note). | The three calls and their arguments. Measured unpoked with every callee a stub: `0x2F198(-1, 0xE, 0x809C8, 0xB000)`, `0x2CCD0(0xA2EB4)`, `0x2DA0C(0x29, ..)`, `0x2DA0C(0x35, 0xA0)`, `0x2DA0C(0x37, 0xA0)`, `0x2D974(0x2A)`, `0x2DA0C(0x2A, ..)`, `0x1AE20()`, `0x2EA78(0xB4, 0xE)`, `0x2F280(-1, 0xE, 0x809F0, 0xB000)`. |
| `config_validate` | `0x2D912`, `0x2D919`: `call 0x2de98`, `call 0x2df8c` -> five nops each | The rest of the function: EAX is loaded before each (`mov eax,ebp`) and after (`0x2D91E test esi,esi`). Both are deferred in `tools/port_classification.txt` and the port calls neither. | Those two calls. Measured unpoked with both run from their bytes: six more `0x2D4EC` calls, `(6)`, `(7)`, `(8)`, `(3)`, `(4)`, `(5)`, after `(1)`, `(2)`, `(0)`. Also blind to what the reads store in `0x105DE1..0x105E33` (zeros over the image's zeros, or bytes the defaults arm's fill then clears). |
| `gfx_flush_palette` | `0x1C481`: `mov edx,0x3da; in al,dx` -> `mov al,8` and four nops; `0x1C4AA`, `0x1C4CA`, `0x1C4CE`, `0x1C4D5`: `out dx,al` -> nop | The list bookkeeping. Bit 3 of the status byte is all the code takes from the port (`test al,8; je` `0x1C487`; AL is loaded again at `0x1C4A7`). Unpoked: `unmodeled: in at 0x1C486` (measured; the empty list returns unpoked). | Every byte sent to the DAC: the index, the count, the colour dwords, their three shifts; and so whether `0x1B544` resolved the handle and whether the count dword behind it is skipped. |
| `gfx_screen_reset` | `0x52129`: the retrace read as above; `0x5213F`, `0x52145..0x52147`: `out dx,al` -> nop; `0x52151`: `call 0x51f72` (EAX = `0xA0000`) -> five nops | The two stores and the two offscreen fills. Unpoked: `unmodeled: in at 0x5212E` (measured). | The 256 black DAC entries and the fill of the aperture (the aperture rule: the port fills its host aperture, `gfx.c` `PORT:`); EAX (§1). |

### Unhit blocks, named in the specs

- `config_screen_wait(_zero)`: `0x2EB29`, `0x2EB31`, `0x2EB38`, `0x2EB3A`, `0x2EB4D`, `0x2EB52`, `0x2EB61`,
  `0x2EB64` — the key loop (above).
- `game_quit_prompt`: `0x24A6F` (`sub eax,eax`: dead for AH = 0, the wrapper's `(AH & 0xF) - 1` is `0xFF`) and
  `0x24A9C` (the soft-restart arm: `0x1D270`, the config writer `0x1B084`, which the port defers by a `PORT:`
  note, and a tail jump into the runtime's `longjmp` `0x65431`).
- `config_validate` (19): `0x2D761` and the two version arms `0x2D773..0x2D7C5`, `0x2D7CD..0x2D7FD` — the two
  reads disagree; the port models both as -1 (`PORT:`), and the raw then repairs a copy through `0x2D4EC`
  first (measured: one extra `0x2D4EC(1)` or `(2)` before `0x2CADC` with one copy unreadable). `0x2D828`,
  `0x2D8BB`, `0x2D8EB`, `0x2D8FC`, `0x2D900` — the stored-image arm, which needs a stored copy holding the
  magic; the port has no stored image. `0x2D92B` — unreachable after `0x2D754`. `0x2D95D` — `0x2DAE4(0x24)`,
  a deferred no-op the port never calls.
- `config_code_row_rr`: the insert arm's six blocks (the first row's).

### What the partial rows do not claim about the port's model

- `config_validate`: the port tests the RAM bytes `0x105E30` for the magic; the raw tests them after its
  first read has stored the stored copy over `0x105DE1..0x105E33` (`0x2D729`). A RAM magic over an empty
  stored image is therefore a state where the two differ by the port's model: not cased. The byte `0x2D490`
  is 4 in the image and no instruction stores it (its eight references, all in `0x2D6F8`, are `cmp`/`mov`
  reads): with it zero and a read that succeeds (1 on the empty image), the raw runs the defaults arm and
  the port, whose reads are -1, returns: not cased either.
- `config_screen_wait`: the ISR gate byte `0x104B22` is 1 in every case. With it clear the port steps the
  tick dwords and the frame word itself in the wait (its model of the timer ISR `0x1BDF4`, `PORT:`), which
  the raw's bytes do not. The camera and clip records handed to `0x1C3FC` and `0x14328` are not compared
  (the port's functions take none). The present is not compared: the back buffer is `0xA0000`, so the full
  copy `0x655FF` and the dirty blit `0x501A3` rewrite the bytes they read (`game_loop`'s fixture).

### Stays a gap

- **`0x4FBA2` (`int10h_query`): `int 0x10` at `0x4FBAD`.** `mov eax,0xf00; int 0x10; and eax,0xff; ret`
  between six pushes and pops. Run, the original stops `unmodeled: int at 0x4FBAD` (measured). Everything
  the function returns is the BIOS's AL. A poke (`mov al,0x13`) would state the value that the port's
  `static u32 int10h_query(void) { return 0x13u; }` states (`PORT:`: the host owns the window): the row
  would compare a constant of mine with a constant of the port's and say nothing about either. Not rowed.

### Not cased, by row

- `text_blit_glyph`: a font entry whose id is 0. `0x1C528` then leaves the frame node's `+0x10` and `+0x20`
  as the stack held them and `0x51ED8` returns on the zero width (`0x51EDF`); the by-value comparison would
  be of uninitialised bytes on both sides. No entry of the image's font table has a zero id (0 of 256,
  measured).
- `svc_buttons_draw`: a first key with no name (the raw draws its uninitialised frame; the port zeroes its
  scratch, `PORT:`), and a name longer than five characters (wrapped it overruns the frame's eight bytes into
  the locals `[esp+8]`, `[esp+0xC]`; the port's scratch has no such neighbours).
- `config_code_row`: high-score descriptors other than the image's for table 2 (three value bytes, one name
  word). With a wider name field `0x2DCA0` would read past the record into the raw's frame.

## §5 Store-sweep findings

Every field a row's function writes is seeded with a value that differs from what is written and sits
between seeded neighbours; each store has a dropping mutant.

- `text_blit_glyph`: the column dword (`@adv8`, `@adv10`, `@ge`, `@bit`, `@addcache`), between `0xC0C0C0C0`
  and `0xC1C1C1C1`; the node's fields at the `0x51ED8` stub (`@nopal`, `@xy`, `@clip`, `@nohflip`).
- `svc_buttons_draw`, `config_code_row`: the cursor words `0x105F34`/`0x105F36` (seeded `0x7171`/`0x7272`)
  are stored by `0x2F198`, run on both sides. `config_code_row`: the flags byte's bit 1 (`@noset`, `@set1`,
  `@setearly`: the memory at the `0x2D4EC` call) and the table-2 record (`@name2`, `@base`, `@from4`, …).
  The record's fourth name byte (`mov [esp+7],dh` `0x3066F`) has no mutant: table 2 packs three characters
  and never reads it.
- `config_validate`: `0x105DA4`, `0x105DA5`, `0x105DA7` seeded `0x44`, `0x55`, `0x77` around a kept `0x66`;
  the flags byte `0x50` -> `0x57` (`@or6`, `@or1`); both fills against every seeded byte and their
  neighbours `0x105DD6..0x105DDF`, `0x105DE0`, `0x105E34..0x105E37` (`@fill1len`, `@fill2len`); the magic
  (`@magicbe`, `@magicearly`: the memory at the `0x2CADC` call). The store `0x105E2F` = 0 (`0x2D881`) has no
  mutant: the first fill has already cleared it.
- `config_screen_wait`: the latch dword (`@nolatch`, `@latchw`), the copy flag byte before `0xFD 0xFE 0xFF`
  (`@flagkeep`, `@flagw`), the buffer pair (`@noswap`; `@swapearly` and `@flushfirst` by the memory at the
  stubs).
- `game_quit_prompt`: the prompt flag (`@noflag` by the memory at the `0x1D250` stub, `@flagkeep`), the
  quit flag before a seeded `0x5A` (`@noquit`, `@quitw`).
- `gfx_flush_palette`: the dword `+0xC` (`@noclamp`, `@clampby`, `@clampcount`, `@unsigned`), the mark
  (`@nomark`, `@mark0`, `@markw`), the head (`@nohead`, `@headfirst`), with one seeded record past the head.
- `gfx_screen_reset`: the two tick dwords between seeded dwords (`@no508`, `@no50c`, `@w50c`), the two
  buffers (`@one`, `@e4twice`, `@short`, `@byte`).

Two mutants were dropped as equivalent, measured undetected and then reasoned: `config_set_defaults@noand`
(`(v & ~3) | 3` is `v | 3`) and `gfx_flush_palette@clamp1` (a sum of exactly `0x100` takes off zero either
way). One was dropped because it cannot terminate (`config_screen_wait@nopump`: the wait never ends without
the pump stub's store).

## §6 The queue, and the harness and port changes outside the rows

All eleven entries are decided (the table at the top): ten have a row, `0x4FBA2` stays a gap (§4). Nothing
is left open.

**Mechanism changes** (the runner's hook, with the one binding that first uses it, and each `diff_emu.py`
addition are commits of their own; none alters an existing row, which the full self-check of §7 shows):

- `port/tests/diff_runner.c`, `d6a_scratch_hook` and `D6A_SCRATCH_RUN` (commit `6e184509`). svcmenu.c and
  config.c keep in `mem[]` above the image (`0x03900000..0x039000FF`, four `PORT:` buffers) what the raw
  keeps in its own stack frame. The emulator leaves its private stack out of every comparison; the runner
  reported the port's scratch as changed memory at every recorded call and as writes at the end. The hook
  wraps `seam_hook` (forward-declared in the d6a block), zeroes the scratch around the report and restores
  it; the macro installs it for one binding call and zeroes the scratch afterwards. Only `b_d6a_31a78`,
  `b_d6a_305fc` and their mutants install it. The contents are compared through the callees that read them.
- `tools/diff_emu.py`, appended: `_CALLEE_CLOBBER_FIXES[0x51ED8] = ()` (its six pushes `0x51EEA..0x51EEF` are
  not at the entry; `0x1C5E8` keeps ECX and ESI live across it), `[0x1AE20] = ()` (it falls into `0x1AE28`,
  whose five pushes the scan cannot credit; `0x2CADC` keeps EBX and ECX live across it for `0x2CB69`),
  `[0x2CADC] = ("edi",)` (`0x2D6F8` keeps ESI and EBP live across `0x2D886`); `RESOLVED_JUMPS[0x31AD6] =
  (0x31AC9, 0x31A68, 4)` (`cmp edi,3; ja; mov eax,[esp+0x10]; jmp cs:[eax+0x31a68]`, `0x319B0`'s form);
  `FLAT_SEGMENT_CALLEES |= {0x655FF}` (the runtime memcpy loads ES from DS, `0x65608..0x6560B`; without the
  flat descriptors the run stopped `int3 at 0x10000`, measured).
- `tools/tests/test_diff_verify.py`, existing structures extended at their top: the expected clobber
  dictionary gains `0x51ED8: ()`, `0x2CCD0: ()`, `0x1AE20: ()`, `0x2CADC: ("edi",)`, `0x1D250: ()`, `0x1D270:
  ()`, `0x2EA78: ("edi", "ebp")`; `WINDOWS` gains `0x31AD6`.

**Seams and shape changes in `port/src`** (no behaviour change):

| file | line |
|---|---|
| actors.c / actors.h | `text_blit_glyph` loses `static`; declared in actors.h |
| sprite.c `sprite_blit_aperture` | `PR_SEAM(0x51ED8u, (u32)n->x, (u32)n->y, (u32)n->xorg, (u32)n->yorg, n->type, (u32)n->rows, (u32)n->width, n->pal_ptr, n->pixel_handle, (u32)n->clip_l, (u32)n->clip_r, (u32)n->clip_t, (u32)n->clip_b);` |
| config.c `config_menu_default_bits` | `PR_SEAM_RET(0x2CCD0u, table);` |
| config.c `config_keys_apply_defaults` | `PR_SEAM0(0x1AE20u);` |
| config.c `config_set_defaults` | `PR_SEAM0(0x2CADCu);` |
| config.c `config_screen_wait` | `PR_SEAM(0x2EA78u, (u32)n);` |
| flow.c `sound_pause`, `sound_resume` | `PR_SEAM0(0x1D250u);`, `PR_SEAM0(0x1D270u);` |

The rows of the functions that gained a seam (`config_menu_default_bits`, `config_keys_apply_defaults`,
`config_keys_apply`, `sound_pause`, `sound_resume`) verify unchanged (measured one by one, and in §7's run).

## §7 Counters and gates

Measured on the branch head, one run: `PR_DIFF_JOBS=4 python3 tools/diff_verify.py --image
/tmp/pr_d6a_img.bin --self-check | tail -1`:

`diff-verify: 963/963 functions VERIFIED; 14349/14349 mutants detected; 1 named gaps; 604/796 rows with callees
closed (167 have none)`, `rc 0`. Before the batch (`main` `5fee3522`): `951/951`, `14206/14206`, `601/784`. The
twelve new rows all have callees; three more rows are closed. Two comment-only edits (a call-site count in
`diff_verify.py`, section numbers in `diff_emu.py`) were committed after that run.

By script, not by the suite (`tools/tests/test_diff_verify.py` was not run): the stub clobber dictionary
equals the specs' (594 addresses, each equal to `E.callee_clobbers`, no address declared two ways); the 22
resolved-jump windows match the bytes; the three values of the unresolved-clobber test hold; 964 spec names,
none repeated; `SpecResult.outside` is empty for all twelve rows.

Not run: `make verify`, `make diff-verify`, the harness unittest, any oracle or capture target. The three
corrections are unproven against the oracles until the integrator's gate.
