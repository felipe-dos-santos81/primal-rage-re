# Track D1, batch d1d — derivation record (game/flow.c second half, flow.h)

Date 2026-10-11. Branch `reverse-d1d` (off `reverse-d1` `b4d0ad4`). Brief:
`.superpowers/sdd/2026-10-11-reverse-d1/brief.md`; queue `d1d-queue.txt` (25 entries).

Ground truth: the fixed-up image `./build/diffrun --exe data/game/C/PRAGE.EXE --image-out
/tmp/pr_d1d_img.bin` (1028304 bytes, linear addresses) disassembled with capstone through
`tools/diff_emu.py`, cross-checked against the live Ghidra project for the two address findings of §3.
Every number below is measured on this branch; nothing is fitted. Claim, as for every differential row:
equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as
stated.

Result: **23 rows VERIFIED, 185 mutants detected**, 3 `port/src` corrections (§3), 2 queue entries
that the harness cannot run (§4), 0 queue entries left undone for lack of time.

## §1 The members

`size` is the queue's. "EAX" is what fixes the row's mask.

**How the EAX column was derived.** For each function, every direct call site in the code object
(`E8`/`E9 rel32` scan of `0x10000..0x73B14`; none of the 23 is referenced by a pointer: no dword of the
image equals an entry address) was followed forward instruction by instruction with capstone's
`regs_access`: a read of a still-live part of EAX is a READ, a write kills the bits it writes (`al`/`ax`
kill only their bits; `and eax,imm` keeps only `imm`), a direct callee is walked from its entry and the
walk resumes after the call with what it left live, and a `ret` resumes after every direct call site of
the containing function (function starts: the `FN_` addresses of `symbols.h`). Limits of that walk,
stated: depth 6 down and 6 up (no path reached either limit), an indirect call or jump stops the walk and
is reported (none was reached with EAX live), a recursive callee is cut by a memo (treated as leaving
nothing live), and function starts are Ghidra's. "dead" below means no READ on any path; the kill sites
are the instructions where the last live bit died.

| addr | port function | size | EAX: evidence | mask |
|---|---|---|---|---|
| `0x33904` | `frontend_list_next` | 29 | all 15 call sites read EAX whole: `mov ebx,eax`/`mov ecx,eax` then `test eax,eax` (0x12030, 0x12054, 0x123D0, 0x123F6, 0x124FC, 0x12521, 0x126DC, 0x1270A, 0x29B83, 0x29B9C, 0x41583, 0x415AE, 0x42580, 0x425B2), `test eax,eax` at 0x2096B | full |
| `0x389C4` | `render_scroll_edge` | 113 | one caller, 0x25601; the next instruction calls `0x38A38`, which loads EAX at 0x38A54 `mov eax,ebx` before any read | 0 |
| `0x38A38` | `render_scroll_fill` | 224 | 0x25606 then `call 0x24c5c`, whose 0x24C61 is `xor eax,eax`; 0x387DD then 0x387E9 `mov eax,ecx` | 0 |
| `0x38B18` | `frontend_spawn_row` | 85 | 23 call sites, dead on every path. The walk's kill sites: 0x11147, 0x11FAF, 0x11FFB, 0x12259, 0x1226C, 0x12284, 0x12290, 0x124BE, 0x124D1, 0x124E9, 0x124F5, 0x1EA3A, 0x1ED6D, 0x1ED80, 0x1ED98, 0x1EDA4, 0x20964, 0x2FEC3, 0x2FECD, 0x43272 (a `mov eax,..`/`xor eax,eax` after the call), and inside the callees entered first 0x2D64C `xor eax,eax`, 0x413DE `mov eax,0xc86dc`, 0x428EB `mov eax,0xc8364`; the site 0x2F9C2 returns with EAX live (0x2F9CA) and dies in its callers | 0 |
| `0x414B4` | `frontend_portrait_flash` | 114 | 0x41D13 then 0x41D28 `lea eax,[edx+0x34]` (the `mov al`/`inc al` before it write AL only and read none of the rest); 0x42096 then 0x420A1 `xor eax,eax` | 0 |
| `0x41528` | `frontend_portrait_reveal` | 80 | one caller 0x42159 in `0x41C28`, which returns with EAX live into `0x24C5C`'s mode tail; dead on every path (12 kill sites, e.g. 0x25499 `lea eax,[edx*8]`, 0x25550/0x25591 `xor eax,eax`, 0x2A353 `mov eax,[0x105bcc]`, and 0x12D51/0x13528 `and eax,0xff` where only bits 16..31 were still live) | 0 |
| `0x4160C` | `frontend_mode12_advance` | 199 | callers 0x4174D, 0x4189A, 0x4212C: the same tail as `0x41528`, dead (12 kill sites) | 0 |
| `0x417C4` | `flow_no_continue_screen` | 180 | caller 0x42434: the same tail, dead (12 kill sites) | 0 |
| `0x42724` | `flow_challenge_fighters` | 403 | caller 0x4256F then `call 0x4b9ac`, which loads EAX at 0x4B9CD `mov eax,[esp+0x1c]` before any read | 0 |
| `0x42BCC` | `flow_challenge_drop` | 195 | caller 0x42553 then `jmp 0x425ef`; EAX is rewritten at 0x4260C `lea eax,[ecx+1]`, 0x42665 `xor eax,eax` or 0x426C0 before any read | 0 |
| `0x46504` | `flow_1082c8_latch` | 21 | callers 0x25C0E and 0x26A31 (hooks) return with EAX live into the mode tail; dead (13 kill sites, the `0x41528` set plus 0x252B7 `xor eax,eax`) | 0 |
| `0x4651C` | `flow_1082c8_restore` | 21 | 0x27E74 then 0x27E79 `xor eax,eax`; 0x28E07 then 0x28E0C `mov eax,esi` | 0 |
| `0x46594` | `flow_1082c8_init` | 119 | 0x257F7 then 0x257FC `mov eax,0x104b02` | 0 |
| `0x4660C` | `flow_1082d0_from_column` | 100 | 7 call sites: 0x2704E/0x2721B/0x41784/0x418B4/0x42397 `mov eax,[0x104ad4]`, 0x2871C `xor eax,eax`, 0x416FA `lea eax,[edx*8]`; the three that call `0x25848` first die at its 0x2586B/0x258E7 `xor eax,eax` | 0 |
| `0x4F1D0` | `frontend_origin_zero` | 19 | the function writes only EDX (`push edx; xor edx,edx; ...; pop edx`): EAX at return is the caller's. Measured: the binding reports its argument and the row compares all 32 bits | full, pass-through |
| `0x4F6E8` | `frontend_mode_18_step` | 28 | caller 0x253F5 (`0x24C5C` case 0x18) then `jmp 0x2540f`: the mode tail, dead (12 kill sites) | 0 |
| `0x4F704` | `frontend_mode_19_step` | 16 | caller 0x253FC then the same tail, dead | 0 |
| `0x4F714` | `sound_voice_stage` | 18 | a tail `jmp 0x2c3fc`: EAX at return is the dispatcher's. Its one caller 0x25C09 then calls `0x46504`, whose first instruction `mov eax,[0x1082c8]` kills it; the port returns the dispatcher's value, so the row compares all 32 bits of the stub's EAX (conservative) | full |
| `0x4F728` | `sound_voice_match_end` | 79 | 0x27E4B then 0x27E50 `xor eax,eax` | 0 |
| `0x4F778` | `frontend_buttons_pressed` | 24 | `setne al` leaves bits 8..31 = `[0x1088e4]`'s; the three callers read AL: 0x27AC8 and 0x42E06 `and eax,0xff`, 0x4F7DA `test al,al` | 0xFF |
| `0x4F980` | `frontend_wipe_arm` | 29 | the function writes only DL/EDX: EAX at return is its argument, and a caller chain does read it (0x25264, 0x2534E `test eax,eax` in the walk). Measured as for `0x4F1D0` | full, pass-through |
| `0x50188` | `swap_buffers` | 27 | `push eax` .. `pop eax` around the body: EAX preserved (0x24E61 `mov ebx,eax` reads it in the walk). Measured the same way | full, pass-through |
| `0x51F45` | `surface_setup` | 45 | `pushal` .. `popal`: every register preserved. Measured the same way | full, pass-through |

**The pass-through rows.** `0x4F1D0`, `0x4F980`, `0x50188` and `0x51F45` have no return value in the port
(`void`), and the raw never changes EAX. A mask of 0 would need the whole caller analysis; instead each
binding sets its result to its EAX argument and the row keeps the full mask, so every case measures "the
raw returns EAX unchanged" (the `@eax` mutant of each, which returns something else, is detected). This is
a statement about the raw's bytes; the C function cannot alter its caller's value.

## §2 The row table

Measured by `python3 tools/diff_verify.py --image /tmp/pr_d1d_img.bin --self-check` on the branch's last
commit; each mutant's port side was run ten times (identical parsed results every time, no `diffrun`
failure) before its pin was written. Pins: `D1D_MASKS`/`D1D_KINDS`/`D1D_CASES` in
`tools/tests/test_diff_verify.py`.

| row | entry | cases | blocks | mutants | EAX mask | callees (stub / allow / real) |
|---|---|---|---|---|---|---|
| `frontend_list_next` | 0x33904 | 7 | 6/6 | 7 | 0xFFFFFFFF | - |
| `render_scroll_edge` | 0x389C4 | 6 | 6/6 | 7 | 0x0 | - |
| `render_scroll_fill` | 0x38A38 | 8 | 6/6 | 14 | 0x0 | - |
| `frontend_spawn_row` | 0x38B18 | 6 | 6/6 | 9 | 0x0 | `2AE14` stub |
| `frontend_portrait_flash` | 0x414B4 | 5 | 3/3 | 12 | 0x0 | `2A17C` stub, `2BCF4` stub |
| `frontend_portrait_reveal` | 0x41528 | 3 | 1/1 | 8 | 0x0 | `2A17C` stub, `2BCF4` stub |
| `frontend_mode12_advance` | 0x4160C | 4 | 4/4 | 16 | 0x0 | `1C500` allow, `1E75C` allow, `1E808` allow, `2AE14` stub, `2F198` stub, `2F280` stub, `474E4` allow, `500BB` allow, `65490` allow, `654C7` allow |
| `flow_no_continue_screen` | 0x417C4 | 4 | 4/4 | 12 | 0x0 | `1C500` allow, `1E75C` allow, `1E808` allow, `2AE14` stub, `2F198` stub, `474E4` allow, `500BB` allow |
| `flow_challenge_fighters` | 0x42724 | 6 | 13/13 | 12 | 0x0 | `29BC8` stub, `2AE14` stub |
| `flow_challenge_drop` | 0x42BCC | 8 | 13/14 | 14 | 0x0 | - |
| `flow_1082c8_latch` | 0x46504 | 2 | 1/1 | 4 | 0x0 | - |
| `flow_1082c8_restore` | 0x4651C | 2 | 1/1 | 4 | 0x0 | - |
| `flow_1082c8_init` | 0x46594 | 5 | 4/4 | 7 | 0x0 | - |
| `flow_1082d0_from_column` | 0x4660C | 7 | 6/6 | 7 | 0x0 | - |
| `frontend_origin_zero` | 0x4F1D0 | 2 | 1/1 | 4 | 0xFFFFFFFF | - |
| `frontend_mode_18_step` | 0x4F6E8 | 4 | 3/3 | 7 | 0x0 | `28D68` allow, `4F980` allow, `5D812` real |
| `frontend_mode_19_step` | 0x4F704 | 4 | 3/3 | 5 | 0x0 | `28D68` allow, `4F980` allow, `5D812` real |
| `sound_voice_stage` | 0x4F714 | 5 | 1/1 | 5 | 0xFFFFFFFF | `2C3FC` stub |
| `sound_voice_match_end` | 0x4F728 | 9 | 7/7 | 10 | 0x0 | `2C3FC` stub |
| `frontend_buttons_pressed` | 0x4F778 | 8 | 1/1 | 5 | 0xFF | - |
| `frontend_wipe_arm` | 0x4F980 | 2 | 1/1 | 6 | 0xFFFFFFFF | - |
| `swap_buffers` | 0x50188 | 2 | 1/1 | 4 | 0xFFFFFFFF | - |
| `surface_setup` | 0x51F45 | 2 | 3/3 | 6 | 0xFFFFFFFF | - |

Notes per row (what the cases separate, checklist items 1-3 and 7):

- `frontend_list_next`: every table entry's +0 and +0xC are non-zero and +8 zero, so a read at the wrong
  offset (`@off`), stride (`@stride`) or width (`@width`: live values `0x00010000`, `0x80000000`) differs; the
  entry AT the bound (0x107798) is live, so only `jae` keeps it out (`@gt`); the head 0x107608 is live in
  `n1` and must be skipped (`@head`). The `jae` is unsigned; a signed bound is not separable inside mem[]
  (it needs a node >= 0x80000000), named here rather than cased.
- `render_scroll_edge`: the gate word takes 0, 1, 0x8000 (`@sign`); the numerator takes 0xFFC0/0xFFFF
  (`@zext`); `e3` has x = -193 so truncation and flooring differ (`@floor`); `e4`/`e5` put 0x8000 on either
  side of the unsigned `ja` (`@cmp`). The equal case (0x107A4A == 0x107A4C) stores the value already
  there and is not observable; not cased.
- `render_scroll_fill`: `f4`/`f7` run a negative stride (0xFFFF8765 << 8) with a non-multiple step, which is
  what separates the three truncating divides from shifts and the signed `idiv` from an unsigned one;
  `f2`/`f3` sit on the 0xEF edge bound; `f5` wraps the edge word past 0xFFFF (`@edge16`); `f6` has the
  divisor 0x8001 and the tail words 0xFFFF (zero-extension). EDI's upper half is seeded in `f1`/`f5`: the
  raw's `mov di,[0x107a4c]` keeps it and only DI is read. Limit, named: a zero divisor traps in the raw
  (`idiv ecx`, 0x38A59) and is undefined in C; the 32-bit products and differences are kept far from
  overflow.
- `frontend_spawn_row`: `p3` is the full table (§3.1); `p2` (six taken) pins the bound; `p4` has a hole at
  slot 2 with slots after it free.
- `frontend_portrait_flash` / `frontend_portrait_reveal`: each table the function indexes is poked with a
  sentinel no image dword holds, at the raw's index only, so every wrong index, table or class split reads
  an image value instead; the character byte takes 0x80 (zero-extension), the stream word 0x8123/0xFFFF
  (`@sext`); slot index and character always differ.
- `frontend_mode12_advance` / `flow_no_continue_screen`: the text arm runs `0x1C500` real on a three- or
  four-group localisation table, so the decoded string is in the memory at each text call: `@id`/`@ids`
  (the wrong string id) are caught only by that memory or the by-value string dwords. The flag byte takes
  0x02, 0xFD, 0xFF, 0x01 (`@bit`); the prompt word 0x8123 (`@x`).
- `flow_challenge_fighters`: the `0x2AE14` stub returns one record per case, so in `t0`..`t2`/`t5` both slots
  hold the same record; `t3`/`t4` (result 3 and -1: no fighter spawn) keep two pre-seeded slot records with
  different +0x56 words, which is what separates slot 0 from slot 1 in the `0x29BC8` arguments and the a5
  words. `@c1a3` is the "obvious fix" the raw does not make (0x427BA reads `[esi+0xc834a]`, ESI = c0 * 2);
  `@childc` likewise (0x4286E reads c0 for both children).
- `flow_challenge_drop`: a record is (+0x5B, the +0x36 word, the +0x1C dword). `d1` has the sum exactly 0
  (the `jl` boundary), `d2`/`d3` put the magnitude at 64 and 65 (the `jg 0x40` boundary), `d4` has v = -191
  (truncating `idiv`), `d5` wraps the word past 0x7FFF, `d7` has the sum's sign from the +0x1C dword.
- `flow_1082c8_init`: the gate takes 1, 0x80, 0; the table bytes take values >= 0x80 (`@sext`); 0xC9388[8b]
  and [b] hold other bytes (`@stride`, `@tbl`).
- `flow_1082d0_from_column`: the count takes 0, 1, 2, 3, 5, 0xFF (the `jle 1` boundary on both sides), the
  result goes negative in `k3`/`k6` (clamp) and stays 0x80/0x90-based in `k1`/`k5` (zero-extension).
- `frontend_mode_18_step` / `frontend_mode_19_step`: the gate takes 1, 0x80, 0 between two 0xFF neighbours
  (`@sign`, `@width`). `h2`/`j2` use the hook value `0x5D812` (`xor eax,eax; ret`, a value the game stores
  and the port does not register): the raw's arrival and the port's `fn_resolve` report are compared with
  the memory at that point. `h3`/`j3` use the registered hook `0x28D68`, run real on both sides with its
  `0x4F980`: it stores 0x104AFA = 0x10, which `0x4F6E8` then copies to 0x104B00, so reading the word
  before the call (`@early`) is a byte difference.
- `sound_voice_stage`: stages 0, 1, 7 read the image's ids; `s3`/`s4` poke 0x8123 and 0x0100 (`@sext`,
  `@byte`); the stub's EAX takes 1, 0, 0x100, 0xABCD0001 (`@ret`).
- `sound_voice_match_end`: the result takes 0, 1, 2, 3, -1; the signed byte takes 1, 0x7F, 0, 0x80, 2; the
  slot byte takes 0, 1, 0x80, with the other side's and the +0x62 neighbour holding the opposite.
- `frontend_buttons_pressed`: `b1`/`b5` return AL = 0 with non-zero upper bytes of EAX (the mask is
  load-bearing); `b6`/`b7` poke the mask table.

## §3 Corrections (raw over port)

Each is its own commit. None is proven against the capture oracles (the brief forbids `make verify` and
the `*-oracle` targets on a batch branch); the measured evidence on this branch is: the differential row,
`PR_ORACLE_REQUIRED=1 ./build/run_tests` (all checks passed), and the 820-frame headless run, whose 2461
frame files are byte-identical before and after all three (`cat frames/* | shasum -a 256` =
`a53064ba3444295e5282625f00f2ba4d61169ad315857c9df6ce8692f63f4b81` both times).

### §3.1 `frontend_spawn_row` (0x38B18): a full table still spawns, and stores at index -1

Raw: `0x38B1F mov edi,0xffffffff`; the scan leaves through `0x38B3A cmp eax,0x1c` / `0x38B3D jge 0x38b4a`
with EDI untouched when none of the seven dwords at 0x107A1C is zero; then `0x38B5D call 0x2ae14` and
`0x38B62 mov [edi*4+0x107a1c],eax`, i.e. the record goes to 0x107A18 (the dword before the table: the
seventh dword of the 0x107A00 array that `actors.c` clears beside it).

Old port: returned without spawning when the table was full (a `/* PORT: */` deviation: "the port skips
instead of touching the word before the table"). New port: spawns and stores at `DS_00107A1C - 4`.

Measured before the fix (case `p3`): `call #0: original 0x2AE14(0x9AC1C, 0x150, 0x2, 0xF0, 0x0), port
none` and `byte 0x107A18..0x107A1B: original 00 A5 10 00, port unchanged`. The mutant
`frontend_spawn_row@full` reproduces the old behaviour and is caught by `p3` alone (`byte`, `call #0`).
The store stays inside mem[].

### §3.2 `frontend_mode12_advance` (0x4160C): the sprite prompt is 0xC8704, not 0xC876C

Raw: `0x41626 mov eax,0xc8704` (image and Ghidra, fixups applied; 0xC8704 occurs in the code object at
0x41627 only). Old port: `actor_spawn(mem + DS_000C876C, ...)`. The two descriptors differ in their first
dword (0x19C at 0xC8704, 0x18C at 0xC876C).

Measured before the fix (cases `m0`, `m2`, the sprite arm): `call #0: original 0x2AE14(0xC8704, 0x2A00,
0xFF, 0x1200, 0x0), port 0x2AE14(0xC876C, ...)`. The mutant `frontend_mode12_advance@desc` reproduces it
(`m0`, `m2`; `call #0`).

### §3.3 `game_mode_12_step` state 0 (0x41C28): the other half of the same swap — NO ROW

Raw: `0x41C7F mov eax,0xc876c` (0xC876C occurs in the code object at 0x41C80 only). Old port:
`actor_spawn(mem + DS_000C8704, DSW(DS_000C8718), 0xFF, 0x3600, 0)`. The port had the two descriptors of
§3.2 exchanged between the two functions (and the two `#define` comments with them).

`0x41C28` is not in a D1 queue and has no differential row: this change rests on the two instructions
above and is **not measured by any row**. It is a separate commit so it can be dropped alone.

Both §3.2 and §3.3 are on the sprite-prompt arm (`DS_00104529` bit 1 set); the 820-frame run and the unit
suite never take it with either descriptor (their outputs did not move).

## §4 Named gaps and unhit blocks

- **`0x4FBA2` (`int10h_query`, the BIOS mode query): no row.** The function is `mov eax,0xf00; int 0x10;
  and eax,0xff` between six pushes and pops; `E.run_original(img, 0x4FBA2)` stops `unmodeled`, `int at
  0x4FBAD` (the harness models no interrupt). The port's function is a host stand-in (`return 0x13`, a
  `/* PORT: */`). A `Spec(..., gap="int at 0x4FBAD")` row in the form of `host_1b890` would be the faithful
  statement; it is not added because `test_every_ported_function_agrees_with_the_original_on_every_block`
  exempts only `host_1b890` by name, so a second gap row needs an edit outside this batch's block. For the
  integrator to decide.
- **`0x47370` (the localisation loader): no row.** 16 blocks; direct callees `0x1C308`, `0x1D290`,
  `0x1E6D8` (host-owned), `0x1E75C`, `0x1E808`, and the DOS file runtime `0x61C60`, `0x61CF6`, `0x61EC0`,
  `0x62003`, `0x6225A`, `0x62356`. `E.run_original(img, 0x47370)` stops `unmodeled`, `call 0x61C60 from
  0x473A6` (the file open). The port's `game_string_table_load(const char *dir)` replaces the paged-memory
  manager and the DOS reads with one stdio read (`/* PORT: */`): there is no register-level binding to
  compare, and stubbing six runtime file calls would compare a stub script with a different host
  implementation. Named gap; the functions that consume the table (`0x1C500`, `0x474E4`, `0x1E75C`,
  `0x1E808`) have rows (C3f).
- **`flow_challenge_drop` block `0x42C5D`: dead, named.** `0x42C51 jge 0x42c5d` follows `0x42C4F jge
  0x42c7e` with no flag-writing instruction between (`0x42C4C test si,si`), so it is reached only with the
  condition false. 13/14 blocks hit.
- **`frontend_mode_18_step` / `frontend_mode_19_step`:** the indirect `call [0x104ae4]` is exercised with
  two targets only (`0x5D812`, `0x28D68`); every other hook value is outside the row.
- **Stubbed callees' results that the functions do not read** (so not varied per case): `0x2BCF4`,
  `0x2A17C`, `0x29BC8`, `0x2F198`, `0x2F280` return nothing the raw reads; `0x2C3FC`'s EAX is overwritten in
  `0x4F728` (0x4F76B `mov eax,0x22`, then `pop edx; ret` with the second call's EAX, which no caller
  reads) and is the row's result in `0x4F714` (varied). `0x2AE14`'s EAX is stored by `0x38B18` and
  `0x42724` and varied per case there; `0x4160C`/`0x417C4` drop it.

## §5 The store sweep

For every field a function writes, the case's pre-state differs from the written value and the bytes
beside it are seeded and must survive (a too-wide store is a byte difference). Dropped-store mutants,
each detected: `frontend_spawn_row@store`, `render_scroll_edge@store`, `flow_1082c8_latch@one`,
`flow_1082c8_restore@one`, `flow_1082c8_init@d0`/`@latch`, `frontend_mode12_advance@b111`,
`flow_no_continue_screen@ee`, `flow_challenge_drop@b25`/`@clr1c`/`@zero36`/`@done`,
`frontend_mode_18_step@nostore`, `frontend_origin_zero@a`/`@b`, `frontend_wipe_arm@f5`, `surface_setup@a4`;
too-wide or too-narrow stores: `frontend_origin_zero@width`, `frontend_wipe_arm@width`/`@byte`/`@b00`,
`frontend_mode12_advance@afe`, `flow_1082d0_from_column@width`, `swap_buffers@width`,
`flow_1082c8_latch@width`.

Findings:

1. The sweep's one real finding is §3.1 (a store the port did not make).
2. `render_scroll_edge`: the store of 0x107A4C when 0x107A4A == 0x107A4C writes the value already there;
   no case can observe it (named in §2).
3. `frontend_portrait_flash`/`reveal`: the `or byte [rec+0x29],8` is unobservable when bit 3 is already set
   (`a3`, `b2`); the other cases start with it clear.
4. `frontend_mode12_advance`: the side's +0x82 byte wraps 0xFF -> 0 in `m1` (a byte increment, not a wider
   one: the +0x83 neighbour survives).
5. Two fixture defects found and fixed while measuring (no pin ever rested on them): `frontend_list_next`'s
   `n5` first named a node off the 0x10 grid (its live entry was never built; the fixture now asserts
   every live address is on the grid), and the first form of the gate mutant of the two hook rows inverted
   the wrong way round (caught at once: the unmutated core disagreed).

## §6 Queue entries not done

| addr | why |
|---|---|
| `0x4FBA2` | named gap, §4: `int 0x10` (`unmodeled`, `int at 0x4FBAD`); no gap row added (a test outside the block would need an edit) |
| `0x47370` | named gap, §4: DOS file I/O and the host-owned pager; the port is a host replacement |

The other 23 entries have rows. Not verified here, by the brief: `make verify` and every `*-oracle` target
(so §3's corrections are unproven against the captures), and the pinned counter line (left for the
integrator).

## §7 `port/src` changes other than §3

No seam was added (every stubbed callee already had one: `0x2AE14`, `0x2BCF4`, `0x2A17C`, `0x29BC8`,
`0x2F198`, `0x2F280`, `0x2C3FC`; no new stub address, so the pinned clobber table of
`test_each_stub_declares_the_registers_its_callee_clobbers` is unchanged). Eight `static` functions of
`flow.c` are made external so `build/diffrun` can bind them (declarations in `flow.h`, one block; the
precedent is C3g's `hit_stance_timer`): `frontend_portrait_flash`, `frontend_portrait_reveal`,
`frontend_mode12_advance`, `flow_no_continue_screen`, `flow_1082c8_latch`, `flow_1082d0_from_column`,
`swap_buffers`, `surface_setup`. No behaviour change; `python3 tools/port_progress.py` and `make
entry-triage` are unchanged.
