# Track D, wave 4, batch d4d — derivation record

Differential rows for the queue `.superpowers/sdd/2026-10-11-reverse-d1/d4d-queue.txt` (15 functions of
`game/config.c`, `menu.c`, `actors.c`, `movie.c`, `svcmenu.c`, `attract.c`; two of them live in `flow.c` and
`platform/`). Branch `reverse-d4d` off `reverse-d3int` (`94898999`). Ground truth: the fixed-up image
`build/diffrun --image-out` dumps (linear addresses), disassembled with capstone 5.0.7. Every figure in §2 is
measured with `python3 tools/diff_verify.py --image … --function <name> --self-check`; the pins in
`tools/tests/test_diff_verify.py` (`D4D_MASKS`/`D4D_KINDS`/`D4D_CASES`) come from `verify_all` on the batch's
own specs. The claim of each row is the track's: equivalence on the exercised blocks and inputs only, with
the callees stubbed or run as stated.

Result: 8 of the 15 entries are rows (167 cases, 149 mutants), 7 are named gaps (§4, §6). **No row found a
port defect: there is no `port/src` correction in this batch**, only five seams.

## §1 The members

| addr | port function | size | callers and what they read of EAX |
|---|---|---|---|
| `0x2EBF0` | `config_key_flags` (config.c) | 493 | Two callers (a rel32 scan): `0x2EDFB` and `0x2EEE3`, each returning to `or edx,eax` (`0x2EE00`, `0x2EEE8`). No stored pointer. Mask `0xFFFFFFFF`. |
| `0x2EDE0` | `config_input_poll` (config.c) | 56 | 20 call sites; each keeps or tests the whole dword: `mov edi,eax` `0x2D020`, `test eax,0xc000c000` `0x2D1CA`, `mov edx,eax` `0x2FD2E`, `test eax,0x2000000` `0x3075C`, `mov esi,eax` `0x312A4`, `and eax,0x3000000` `0x33272` … (two discard it: `mov eax,esp` `0x32345`, `xor eax,eax` `0x32632`). The function ends `mov eax,edx` (`0x2EE10`). Mask `0xFFFFFFFF`. |
| `0x2EEC8` | `config_input_poll_clear` (config.c) | 64 | One caller, `0x303D2`, returning to `mov edx,eax` (`0x303D7`). Mask `0xFFFFFFFF`. |
| `0x33874` | `palette_reflow` (actors.c) | 143 | Two callers, `0x4F81F` and `0x4F872`, both returning to `xor eax,eax` (`0x4F824`, `0x4F877`). No stored pointer. Mask 0. |
| `0x4F7F4` | `attract_palette_advance` (attract.c) | 70 | No direct caller; one stored pointer, the dword `0xA8744` (entry 0 of that table), called at `0x292C1` (`mov eax,ebx; call [eax+0xa8744]`, the only code reference to `0xA8744`). It returns to `shr edx,1` (`0x292C7`), then `mov eax,ebx` (`0x292BF`) before the next entry, or `0x292AC` returns to its one caller `0x255F3` at `0x255F8`: `cmp byte [0x107a54],0; je 0x2560b`, then `call 0x389c4`, which loads EAX on both of its paths before reading it (`mov eax,[0x107a3a]` `0x389D2`; `mov eax,edx` `0x38A13`), or `call 0x24c5c`, which opens `xor eax,eax` (`0x24C61`). Mask 0. |
| `0x29B74` | `frontend_darken_all` (flow.c; registered in actors.c) | 81 | One direct caller `0x27B17` and five register immediates (`0x2788B`, `0x2861E`, `0x28978`, `0x28A9D`, `0x28B3C`; `0x2861E`'s is stored at once, `mov [0x104ae4],edx`, the other four were not traced to their store) for the `call [0x104AE4]` sites `0x4F302 0x4F373 0x4F6F1 0x4F70D 0x4F9AA 0x4F9D1`, two of which return it to their own callers (`pop edx; pop ebx; ret` `0x4F379`; `ret` `0x4F713`). The caller analysis for mask 0 was **not** done. Instead: the raw's EAX is 0 on every path — the two edges into the tail `0x29BA2` are `test eax,eax; je` (`0x29B85/0x29B87`) and the fall-through of `test eax,eax; jne` (`0x29B9E/0x29BA0`), and the tail writes EDX, ECX and three words only. The port function is void; the binding reports the constant 0 under the **full mask**, so each case checks that the raw does return 0. |
| `0x2BAF4` | `actors_reset_al` (actors.c) | 316 | 35 rel32 callers; several do not overwrite EAX at once (`0x10E9C` and `0x24B66` continue with `xor ah,ah`, `0x4F21F` with `pop edx; ret`, `0x1F13B` with `call 0x1ea08`), so mask 0 was **not** established. The raw's EAX is `0x2EA30`'s at `0x2BC21`: the lock byte `0xBCD60` as the pair of calls leaves it (`0x2EA30(0x2700)` returns the old byte and clears a 1, `0x2EA30(that byte)` puts a 1 back and returns it; `0x2EA4B..0x2EA5C`). The port does not model the lock (`0x2EA30` is a named non-row) and its function is void; the binding reports the byte `0xBCD60` under the **full mask**. |
| `0x3157C` | `config_key_name` (config.c) | 1057 | 24 call sites. The function sets AL alone (`mov al,1` `0x316E1`; `xor al,al` `0x31645`, `0x3168D`, `0x31994`). One caller reads it, `test al,al` (`0x1A3F1`, after the call `0x1A3EC`). The others overwrite EAX: `mov eax,0xc` / `mov eax,0x20` after the sixteen sites `0x19F55..0x1A316`, `mov eax,edi` after `0x1A373`, `mov eax,[esp+0x10]` on both arms after `0x31AC4` (`0x31AD2`, `0x31B5D`), `xor eax,eax` after `0x31D08 0x31D47 0x31D86 0x31DC0`; `0x19DE6` is in `0x19DD8`, which nothing calls and no dword points to. Mask `0xFF`. |

The two full-mask-with-a-reported-value bindings (`0x29B74`, `0x2BAF4`) compare nothing the port computes:
they stand where a mask 0 would need a caller analysis this batch did not do, and they do check the raw's
value in every case. A reviewer who prefers mask 0 needs the analysis named above.

## §2 The rows

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `config_key_flags` | `0x2EBF0` | 62 | 46/46 | 19/19 | `0xFFFFFFFF` | EAX = the mask (EBX). `0x2EB80` (rowed, d3d) and `0x500BB` run on both sides: the latch dword `0x105F30` is the key, read whole (`test eax,eax` `0x2EC04`; `lea esi,[eax-0x48]; cmp esi,8; ja` `0x2EC31..0x2EC37`; `cmp edi,eax` against each `movzx` key byte: `j7` is `0x00010048`, `j8` `0x8000004D`, `e8` `0x10D`, `e9` `0x1001B`). The arrows are scanned for mask 0 or any bit of `0xF300F000` (`0x2EC19..0x2EC2B`; `g2`/`g3` have only bit 24 / 25, `g0`/`g1` none of them). Table `0x2EBCC` = `0x2EC45 0x2ED9F 0x2ED9F 0x2ECF7 0x2ED9F 0x2ED49 0x2ED9F 0x2ED9F 0x2EC9E` (`0x48` up, `0x4B` left, `0x4D` right, `0x50` down; `j0..j6` are `0x47`, `0x51` and the five default entries). Per arm nine cases (`u*`, `d*`, `l*`, `r*`): neither key byte (words 0), the first alone with `+0x2D4` = 0 / `0x0100`, the second alone with `+0x2D6` = 0 / `0x8000`, both with either word 0, both non-zero, and every OTHER key byte equal to the code with both words 0. Enter needs mask 0 or bit 24, Esc mask 0 or bit 25 (`e0..e7`). `z0`/`z1`: nothing latched. |
| `config_input_poll` | `0x2EDE0` | 9 | 5/5 | 11/11 | `0xFFFFFFFF` | EAX = the mask (EBX), DL = the flag (`mov [esp],dl` `0x2EDE7`; `cmp byte [esp],0` `0x2EDF3`: `p4` has EDX = `0x100`, `p5` DL = `0x80`). `0x50161` (rowed; it latches into `0xE1C38`) and `0x500BB` run on both sides; `0x2EBF0` is a stub (seamed by this row) that takes the mask again (`mov eax,ebx` `0x2EDF9`) and whose whole EAX is OR-ed in (`or edx,eax` `0x2EE00`). A non-zero result stamps `0x105F2C` with the tick dword (`test edx,edx` `0x2EE02`: `p3`, `p4` are zero; `p2`, `p7` have only the key bits). |
| `config_input_poll_clear` | `0x2EEC8` | 9 | 5/5 | 14/14 | `0xFFFFFFFF` | The twin, same cases: it also clears the latch dword `0x105F30` (`xor ebx,ebx; mov [0x105f30],ebx` `0x2EEEA/0x2EEEC`), after the `0x2EBF0` call (`@clrearly`: the memory at the call) and whole (`@clrw`). `@twin` runs the other function's shape in each row. |
| `palette_reflow` | `0x33874` | 13 | 7/7 | 21/21 | 0 | EAX = the table entry, EDX = the handle. `0x1B544` (the named non-row) runs on both sides against the preloaded resource entry (`c2b_res`); it saves EDX, so `mov [ebx],edx` (`0x3389B`) stores the handle. The entry's len against the resource's count, signed (`cmp ecx,[eax]; jl` `0x3388B/0x3388D`: `a3` has len `0x80000000`, `a4` count -1, `a2` len == count). Not short: a differing handle re-points the entry and queues `{handle; start; count; flag byte 1}` (`a0`, `a2`, `a4`; `a1`, `a5` have the same handle). Short: the entries after it are walked to the table end `0x107798` (`b3` is the last entry, `b4` the one before it), each start moved to the previous entry's end while that end is signed-above it (`jle` `0x338D1`: `b0` stops on an equal start, `b5` moves a start of `0x80000000`, `b7` stops with the end `0xFFFFFFF2`) and queued with the entry's own handle and len (`mov ebx,[eax]` `0x338DC`; `b1` walks three, one with a zero handle: no free-entry skip here, unlike `0x33754`). |
| `attract_palette_advance` | `0x4F7F4` | 9 | 4/4 | 15/15 | 0 | No argument. While the dword `0xF0A48` is non-zero (whole: `v7` has only its top byte): `0x33874(the entry, 0xC98A0[the counter byte, zero-extended])` with the counter stepped before the call (the memory at the call) and re-read after it, unsigned against 10 (`jb` `0x4F82E`: `v3` reaches 10, `v5` wraps `0xFF` to 0, `v6` is `0x80`); else, or at 10 and above, bit 0 of the byte `0x104AD0` is cleared. `0x33874` is a stub (seamed by this row). The image's table repeats its ten handles (entries 0 and 1 are both `0x0396F128`), so seven cases poke twelve distinct handles and a sentinel at entry `0x80`; `v5` and `v8` use the image's. |
| `frontend_darken_all` | `0x29B74` | 4 | 3/3 | 14/14 | `0xFFFFFFFF` (the constant 0, §1) | No argument (`f0` and `f2` enter with a dirty EAX: `call 0x13df0` then `xor eax,eax` `0x29B7C`). `0x13DF0` (stub), then for every live entry of `0x33904`'s table (rowed, run on both sides; `test eax,eax` `0x29B85/0x29B9E`) `0x13D4C(the entry, 3)` (a stub, seamed by this row), then the words `0x1088EE` = `0x104AFE` = `0x78` and `0x104B00` = `0x15`. `f0` no live entry, `f1` the first, `f2` three (live dwords `0x80000000`, `0x100`, `0x10000`; the last table entry among them), `f3` all 24; the entry AT the end bound is seeded live in every case. |
| `actors_reset_al` | `0x2BAF4` | 3 | 7/7 | 34/34 | `0xFFFFFFFF` (the lock byte, §1) | AL = the flag, saved at `[esp]` (`0x2BAFD`) and tested as DL (`0x2BBE4`): `t0` EAX = 1, `t1` `0xFFFFFF00` (the copy arm), `t2` `0x00010080`. Stubs: `0x13DF0`, `0x13ADC` and `0x52106` (`in`/`out`; both seamed by this row). On both sides: `0x4F228`, `0x1C350`, `0x38B70`, `0x336C0` (+ `0x33734`), all rowed; on the original side the runtime memset `0x61A70` (`0x65490`, `0x654C7`), the list insert `0x249C0` (actors.c keeps its own), `0x2F920` (no port function, §4) and the lock `0x2EA30`. The two pool pointers `0x1014EC`/`0x1014F4` point at zero runs of the image (`0x74000`, in the gap between the code and data objects; `0xF1000`, in the BSS, which the two effects stubs leave free), with sentinels at each pool's first, a middle and its last dword and in the dword past its end; the same for the `0x14D4` bytes at `0x105F38`. The copy arm copies `0xFA00` bytes `0x90000` -> `0xC0000` (image data). The lock byte is 0 or `0x80` (§4). |
| `config_key_name` | `0x3157C` | 58 | 108/108 | 21/21 | `0xFF` | EAX = the key word, DL = the raw flag (`mov cl,dl` `0x31584`; `test cl,cl` `0x316AC`: EDX = `0x100` and `0xFFFFFF00` are 0), EBX = the destination. An ascii byte 1..`0x7F` other than `0x0D` and `0x20` (`0x31591..0x315A0`: `c2` is `0x7F`, `c4` `0x80`, `c5` `0x0D`, `c6` `0x20`) is class 2, formatted `"%c"` (`0x80B78`) by the runtime's sprintf; else the class is the whole EAX `sar 8` (`nw` = `0x147`; `nn`, `nm` negative) and the unsigned compare tree `0x315A7..0x31687` picks a string id (`0x0E` `0x22A`, `0x0F` `0x22B`, `0x47` `0x220`, `0x48` `0x223`, `0x49` `0x221`, `0x4B` `0x224`, `0x4D` `0x225`, `0x4F` `0x227`, `0x50` `0x226`, `0x51` `0x222`, `0x52` `0x228`, `0x53` `0x229`: twelve distinct names in the fixture), a literal (`0x3D..0x43` three bytes at `0x80B7C + 4i`, `movsw; movsb`; `0x44` the dword `0x80B98`; `0x4A`/`0x4E` the words `0x80B9C`/`0x80BA0`) or returns AL = 0 with the destination untouched (classes 0, 1, 3, `0x0D`, `0x10`, `0x3C`, `0x45`, `0x46`, `0x4C`, `0x54`, `0xFF`). A scan code of 2 with no ascii byte is class 2 as well (`c8..ca`). With the raw flag 0 the name is re-formatted `"<%s>"` (`0x80BA4`) in the 12-byte frame and copied back. The string tree `0x1C500` runs on both sides; the sprintf tree `0x65546` runs from the original's bytes under the flat segments (`D3A_SPRINTF`), the port formats by hand. |

Callee stubs and their clobber sets (`E.callee_clobbers` on the image; a script re-derived every one):
`0x2EBF0` `(ebp)` — it saves EBX/ECX/EDX/ESI/EDI itself and inherits EBP from `0x2EB80`'s tail jump into the
runtime longjmp `0x65431`; `0x33874` `(edx)`; `0x13D4C` `(edx)`; `0x13ADC` `()`; `0x52106` `(ebx)`; `0x13DF0`
`()` (the existing `P8_CLEAR`). No caller in these rows reads a poisoned register: `0x2EDE0`/`0x2EEC8` use EDX
and EBX after `0x2EBF0`; `0x4F7F4` reloads EAX and pops EDX after `0x33874`; `0x29B74` reloads `mov edx,3`
each pass (`0x29B89`); `0x2BAF4` keeps the saved lock result in EBP across `0x52106` and `0x336C0` sets EBX
itself.

## §3 Corrections

None. The eight rows verify the port as it stands. Checked for the classes the earlier waves found: every
callee result in these functions is tested whole (`test eax,eax` at `0x2EC04`, `0x29B85`, `0x29B9E`; `or
edx,eax` at `0x2EE00`, `0x2EEE8`), the one AL test is of the function's own argument (`test dl,dl` `0x2BBE4`,
which the port has as `(al & 0xFF) != 0`), and the signed compares of `0x33874` are signed in the port.

Seen, not changed (not rows of this batch):

- `0x1C65C` returns the last glyph's `0x1C5E8` result: `mov esi,eax` (`0x1C6C2`) after each call and `mov
  eax,esi` (`0x1C6CB`) before the epilogue. The port's `text_blit_string` is void and the comment on
  `text_blit_glyph` says no caller reads `0x1C5E8`'s return. The one caller of `0x1C65C`, `0x1B3EF`, continues
  with `mov dl,1; mov eax,esp`, so nothing reads it there; the comment is imprecise, the behaviour agrees.
- `0x20C10`'s tail calls `0x2F920` at `0x20C42`; the port's `game_init_resume` leaves it out, as record
  `2026-09-30-named-gaps-b-derivations.md` §B.11 states with its reason. Not re-examined.

## §4 Named gaps and unhit blocks

No row of this batch names an unhit block. What the rows do not cover:

- **`config_key_flags`: three things the cases cannot separate.** (1) The record pointer `0x101514` is read
  before the `0x2EB80` call and again after it (`0x2EC08`, `0x2EC13`); the port reads it once. `0x2EB80`'s
  returning paths write nothing (its one store, `0x2EBA8`, is on the longjmp arm), so the two agree. (2) The
  store `mov [0x101514],ecx` (`0x2EDD1`) writes the value just read: unobservable. (3) The two `xor eax,eax`
  that drop the code (`0x2ED9D`, right arrow with neither key; `0x2EDB6`, after Enter) change no result: the
  code is then compared with `0x0D` and `0x1B` only. The idle-timeout arm of `0x2EB80` (the runtime longjmp)
  is not entered: every zero-latch case has a key time `0x100` ticks old.
- **`palette_reflow`: the port's two `PORT:` guards are not cased.** `res_resolve` returning NULL (the port
  takes count 0, the raw dereferences) and `palette_record`'s bound guard (a head outside
  `0x107498..0x107608`); every case resolves and queues at `0x107500`. The raw keeps the queue head in EDI
  across the walk and stores it once (`0x338F7`) where the port's `palette_record` reads and stores it per
  record: equal while no record lands on `0x107798` itself.
- **`frontend_darken_all`: the walk after a `0x13D4C` that edits the table.** The raw takes the next entry
  after each call (`mov eax,ebx; call 0x33904` `0x29B95`), as the port does; the stub writes nothing, so a
  port that fetched the next entry before the call is not told apart. `0x13D4C`'s bytes write effect records
  and the lock byte `0x9AF3C`, never the table `0x107608`.
- **`actors_reset_al`: the lock byte 1.** With `0xBCD60` = 1 (the image's value) `0x2EA30` holds it at 0 from
  `0x2BB0A` to `0x2BC21`, which the memory at every stub call shows (measured: `call #0 memory: … first
  difference at 0xBCD60: original 0x00, port unchanged`) and the port, which does not model the lock, cannot.
  The cases use 0 and `0x80`, which `0x2EA30` leaves alone (the same choice as `release_record`'s row). Also
  not cased: the port's `pool_base() == 0` guard (`PORT:`). The order of the four callees that run on both
  sides shows only where it changes a byte.
- **`config_key_name`: a name longer than 9 characters.** The raw formats `"<%s>"` into its 12-byte frame
  (`sub esp,0xc` `0x3157F`; `lea eax,[esp+8]` after two pushes, `0x316B6`): a 10-character name puts the NUL
  on the saved EDI (`[esp+0xc]`), 14 on the saved ESI, 22 on the return address (`[esp+0x18]`). The port wraps in a larger C buffer
  (`PORT:`). The fixture's longest name is 9 (`BACKSPACE`), which fills the frame exactly.

Functions that are not rows, with the evidence (the `E.run_original` lines are measured,
`/tmp/pr_d4d/d4gaps.py`):

- **`0x2EA74` (`config_screen_wait_zero`): `int 0x16`.** `xor eax,eax; mov eax,eax` and no `ret`: it falls
  into `0x2EA78` with EAX = 0, so `cmp ecx,-1; je 0x2eb7a` (`0x2EAE0`) is never taken and every run reaches
  the BIOS key loop. With the eight callees before it stubbed (the `0x1CF20` stub steps the frame word
  `0xEF6DE` so the wait ends) the original stops `unmodeled: int at 0x2EB15`, after the calls `0x2A31C 0x1C3FC
  0x14328 0x501A3 0x50188 0x1C470 0x1CF20 0x500C4`. 18 call sites. A harness `gap=` spec was not added: the
  suite's `test_every_ported_function_agrees…` excuses only `host_1b890` by name, an existing line.
- **`0x2EA68` (`menu_fatal_error`): the port omits the body by design.** `mov eax,1; jmp 0x62003`: the
  message in EAX is overwritten and the runtime's exit is entered (`unmodeled: indirect call at 0x62006` when
  it runs; stubbed as a tail call it records `0x62003(1)`). The port's function is `(void)msg;` under a
  `PORT:` (spec §7): a row would compare that call with none.
- **`0x2F920`: no port function.** `mov ebx,0x14d4; mov eax,0x105f38; xor edx,edx; call 0x61a70`. The port
  fills inline in `actors_reset_al` (`mem_fill(DS_00105F38, 0, 0x14D4u)`). Its bytes run on the original side
  of the `actors_reset_al` row (the call at `0x2BBDF`), where `@nofill` and `@filllen` are caught; run alone it
  writes exactly `0x105F38..0x10740B`. Its other call site is `0x20C42` (§3).
- **`0x1C5E8` (`text_blit_glyph`): nothing to bind.** The port function is `static`, takes C pointers to its
  caller's `col`/`row` locals (the raw: EDX/EBX = addresses in `0x1C65C`'s frame) and builds a C `SpriteNode`
  where the raw fills its own `0x58`-byte frame (`mov eax,esp; call 0x1c528` `0x1C603`; `mov eax,esp; call
  0x51ed8` `0x1C63B`). Its callees are d3d's two gaps (`0x1C528`: no `mem[]` record; `0x51ED8`: the literal
  aperture). What is comparable — the palette acquire of `0x80997C`, and the advance `cmp esi,0x10` -> `+0xF`
  else `+8` on the zero-extended width byte (`movzx esi,byte [eax+2]` `0x1C5FF`) — is reachable only through
  `text_blit_string` (`0x1C65C`, not in this queue).
- **`0x31A78` (`svc_buttons_draw`): the name lives in the raw's frame.** `sub esp,0x1c`; the destination
  handed to `0x3157C` is `mov ebx,esp` (`0x31AB0`) and the string handed to `0x2F198` is `mov ebx,esp`
  (`0x31AF1`, `0x31B14`, `0x31B4B`), with the locals `[esp+8]` = col + 5 (`0x31AA8`) and `[esp+0xc]` = col - 5
  (`0x31A9A`) right above the 8 name bytes. The port keeps the name in the scratch `SVC_NAME_TMP`
  (`0x03900080`, outside the image, `PORT:`). `0x2F198`'s seam passes the string's first 16 bytes by value:
  the raw's include those two locals, the port's are zeros; `0x2F830`'s and `0x2F0F0`'s seams pass `s - mem`
  (a frame address against `0x03900080`); and a `0x3157C` stub cannot write its result through a pointer the
  two sides disagree on. Running everything real leaves the port's scratch bytes as writes outside the image.
  No comparison is possible without changing those seams, which are other batches' lines.
- **`0x2D4EC` (`config_storage_touch`): a declared no-op.** `tools/port_classification.txt` (the `2D498`
  line, record §K9.10) defers the EEPROM-image store. Run with `0x2D498` stubbed, `0x2D4EC(1)` makes 91 calls
  `0x2D498(address, byte)` (the first `(0x100D98, 0)`) and no store of its own; the port's function is
  `(void)kind;`.
- **`0x1C740` (`movie_play`): a host replacement.** After `0x52106(0)` the first test is the runtime kbhit
  `0x62756` (`cmp dword [0xef910],0`; zero: `mov ah,0xb; int 0x21`): the original stops `unmodeled: int at
  0x62767`; with a kbhit stub returning 0 and the quit flag `0xA81A8` clear it stops at the runtime open,
  `call 0x6345C from 0x1C774`. The port's `movie_play(game_dir, name)` takes host strings and decodes through
  host.c and `smacker.c`. The two skip gates before the open (kbhit, tested whole, `test eax,eax` `0x1C757`;
  the flag byte) are comparable in principle: that needs a `PR_SEAM_RET0(0x62756)` in `input_has_key` and the
  EAX mask, whose analysis is open (`0x1105A` returns to `mov byte [0xf0a6f],1; jmp 0x11550`, and from there
  into `0x10F28` or back to `0x11D75`'s `call 0x10db0`, where only AH is written first). Three of 23 blocks;
  not done.

## §5 Store-sweep findings

- `config_key_flags`: no observable store (§4). The record's `+0x2D0..+0x2EF` is seeded whole with bytes that
  are no arrow code, `0x0D` or `0x1B`, so a key byte read one off (`@off1`, `@p2off`) or a word read as a byte
  (`@wbyte`: the words `0x0100` and `0x8000`) changes the result.
- `config_input_poll` / `_clear`: the key time `0x105F2C` (`0x5A5A0001`) sits between seeded dwords and
  differs from the tick (`0x00C0FFEE`): `@nostamp`, `@stampalways`, `@stampval`, `@stampsel`. The latch
  `0x105F30` is `0x11223344`: `@noclr` leaves four bytes, `@clrw` two; `@twin` clears it in the function that
  must not. `0x50161`'s latch store into `0xE1C38` is its own row's; here it orders the two calls (`@order`).
- `palette_reflow`: the 24 entries are seeded whole (handle, refcount, start, len all distinct per entry) and
  `0x60` bytes at the queue head are `0xA5`. Dropped stores: `@nohandle`, `@nostart`, `@norec`; the record's
  fields by `@startoff`, `@reclen`, `@rechandle`, `@reccount`, `@flag0`. The flag is a byte (`mov byte
  [edi-4],1`): the three `0xA5` after it survive on both sides (the port's `palette_record`). `0x1077A0` is
  poked 0 so `@bound` (an entry AT `0x107798`) moves it.
- `attract_palette_advance`: the counter byte and the flag dword (`0xFFFFFFFF`, or 1) sit between seeded
  neighbours: `@noinc`, `@noclr`, `@bit`, `@wide` (a dword `and`). The counter store precedes the call:
  `@late` is the memory at the call.
- `frontend_darken_all`: each of the three words sits between seeded words: `@no8ee`, `@noafe`, `@nob00`,
  `@vals`, and the widths `@wide8ee`/`@wideb00` (a dword store over the next word). `@early` (the stores before
  the walk) is the memory at the first stub call.
- `actors_reset_al`: every store has a dropping mutant (`@noae8 @noaec @nob4c @noad0 @nobea @nobed0 @noactive
  @nob44 @nob48 @nofill @nobed1`), the widths `@ad0w`/`@beab`, the lengths `@psetlen @reclen @ringlen @filllen
  @copylen` (each against a sentinel in the last dword and one past the end), the list `@freehead @bound
  @count`. The byte `0x105BED` is 0 during the body and 1 at the end: `@nobed0` shows only in the memory at
  the `0x13ADC` and `0x52106` calls. The first run of the case set used the image's lock byte and was a
  MISMATCH on it alone (§4).
- `config_key_name`: the destination's `0x20` bytes are `0x5A`, so a missing terminator (`@nonul`, `@f10`,
  `@minus`) or a byte too many (`@fwide`: the byte after each literal's NUL is not `0x5A`) shows; a key with
  no name leaves all of it. The first run left block `0x31893` unhit (string `0x229` was empty, so `0x53`'s
  copy loop never took its second byte); every id now has a non-empty name and `e0`/`e1` re-run two ids with
  an empty one.

## §6 Queue entries not done

| entry | port function | why |
|---|---|---|
| `0x2EA74` (L0, 4) | `config_screen_wait_zero` (config.c) | Named gap: falls into `0x2EA78` with EAX = 0 and always reaches `int 0x16` at `0x2EB15` (§4). |
| `0x2EA68` (L0, 10) | `menu_fatal_error` (menu.c) | Named gap: `mov eax,1; jmp 0x62003`, which the port omits under `PORT:` (§4). |
| `0x2F920` (L0, 22) | none (inline in `actors_reset_al`) | No port function to bind; its bytes run inside the `actors_reset_al` row (§4). |
| `0x1C5E8` (L1, 113) | `text_blit_glyph` (actors.c, `static`) | Named gap: static, C-pointer arguments, a C `SpriteNode`; callees `0x1C528`/`0x51ED8` are d3d's gaps (§4). |
| `0x2D4EC` (L1, 319) | `config_storage_touch` (config.c) | Named gap: a declared no-op against 91 image stores through the deferred `0x2D498` (§4). |
| `0x31A78` (L1, 137) | `svc_buttons_draw` (svcmenu.c) | Named gap: the name buffer is the raw's frame, the port's a scratch outside the image; the text seams cannot carry it (§4). |
| `0x1C740` (L1, 322) | `movie_play` (movie.c) | Named gap: runtime kbhit (`int 0x21`), runtime file and decoder routines, a host replacement in the port; the two skip gates were left (§4). |

Done: `0x2EBF0`, `0x33874`, `0x3157C` (L0) and `0x2EDE0`, `0x2EEC8`, `0x4F7F4`, `0x29B74`, `0x2BAF4` (L1): 8 of
the 15 entries, 8 rows, 167 cases, 149 mutants.

## §7 Counters and gates (measured on the branch head)

- The one full self-check, `PR_DIFF_JOBS=4 python3 tools/diff_verify.py --image /tmp/pr_d4d_img.bin
  --self-check` (5 min 23 s): `diff-verify: 741/741 functions VERIFIED; 7999/7999 mutants detected; 1 named
  gaps; 480/574 rows with callees closed (167 have none)`, `rc 0` (the branch point: `733/733; 7850/7850; 1;
  479/566 (167)`). `actors_reset_al` alone costs about 65 CPU-seconds of it (three cases over 80 KB of pool
  writes, 35 bindings).
- Each row and its mutants were also run ten times in a row with `--function … --self-check` (identical
  every time).
- Seams added (`port/src`, first statement of each): `0x2EBF0` (`config_key_flags`, config.c), `0x33874`
  (`palette_reflow`, actors.c), `0x13D4C` (`effects_spawn_darken`, effects.c), `0x13ADC` (`effects_init`,
  effects.c), `0x52106` (`gfx_screen_reset`, platform/gfx.c). None of the five was a callee of an existing
  row; `effects_init`'s own row was re-run (unchanged) and the full self-check covers the rest.
- One insertion outside the batch's blocks: two lines at the top of the expected-clobber dictionary of
  `test_each_stub_declares_the_registers_its_callee_clobbers` (`tools/tests/test_diff_verify.py`).
- Not run by this batch: `make verify`, `make diff-verify`, any `*-oracle` target, the Python unittest.
