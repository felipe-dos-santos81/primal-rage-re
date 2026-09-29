# Cluster K10 MOVIE-BLIT (`0x50D23`): raw-byte derivation

**Scope.** Ledger `2026-09-29-all-gaps-ledger.md`, §B.1 row `0x50D23` and §F
row 17 (K10 MOVIE-BLIT, own plan). The plan that executes from this record is
`2026-09-29-k10-movie-blit.md`. Its sections are numbered `§K10.n`, and
`tools/port_classification.txt` and the port comments cite them by that name.
§0 is the starting record the planner derived. Task 1 of the plan re-runs it
and confirms each verdict section §K10.1..§K10.6.

**Tooling.** The Ghidra MCP bridge was not reachable, so no Ghidra tool was
exposed. Every address was read from the Python mirror of `mem_load_le` +
`mem_load_le_fixups` (`port/src/mem.c`), with LE fixups applied (Appendix A,
`le.py`), and disassembled with capstone in 32-bit mode. The mirror's image is
byte-identical to the one the K1/K9 and K13 records used, over
`0x10000..0x10B0D0`. Function extents come from
`port/decomp/prage.functions.csv`. Appendix B, `k10_check.py`, re-derives
every scripted fact in §0 and exits 0 only when all of them hold.

---

## §0 Starting record (derived while planning, 2026-09-29)

### §0.1 The caller: `0x1C740` (the boot-logo movie player, ported as `movie_play`)

`0x1C740` (EAX = file name) is the port's `movie_play` (`port/src/game/movie.c`,
header `/* 0x1C740`). Its raw sites are `0x11050` and `0x1105A` in `0x11000`,
the two phase-0 logos. Its body, read with capstone:

| addr | instruction | meaning |
|---|---|---|
| `0x1C74B/0x1C74D` | `xor eax,eax; call 0x52106` | entry blank, `0x52106(0)` |
| `0x1C752..0x1C766` | `call 0x62756; jne 0x1C878`; `cmp byte [0xA81A8],0; jne 0x1C878` | skip tests. `0x1C878` is the epilogue, reached **without** the exit blank. |
| `0x1C76C..0x1C77F` | `push esi; push 0xFE00; push -1; call 0x6345C`; `je 0x1C878` | Smacker open (runtime library). A failure skips everything. |
| `0x1C785..0x1C7A2` | `push eax; push 0; push 0; push 0x140; push 0xC8; push [0xE87A4]; push 0; call 0x649B0` | binds the movie to the buffer `[0xE87A4]` (pitch `0x140`, height `0xC8`). `[0xE87A4]` is read **once**, at `0x1C794`. |
| `0x1C79D`, `0x1C7A7..0x1C7AA` | `mov edi,1`; `cmp edi,[ebp+0xC]; ja 0x1C86B` | frame counter 1..`[smk+0xC]` |
| `0x1C7BD..0x1C7E3` | `[esi+0x68]` gate, VBlank spin on `0x3DA`, `call 0x65340` | palette (`[esi+0x6C]` picks the source) |
| `0x1C7E6..0x1C7E7` | `push esi; call 0x64130` | decode one frame into `[0xE87A4]`'s buffer |
| `0x1C7EC..0x1C7F7` | `push esi; push 0; call 0x64ED8; test ax,ax; je 0x1C834` | next dirty rectangle, or done |
| `0x1C7F9..0x1C82B` | `[esp+0] = [esi+0x684]`, `[esp+4] = [esi+0x688]`, `[esp+8] = [esi+0x684]+[esi+0x68C]`, `[esp+0xC] = [esi+0x688]+[esi+0x690]`, `mov eax,esp` | the rectangle `{x0, y0, x1 = x0+w, y1 = y0+h}` from the handle's `+0x684..+0x690` = `x, y, w, h` |
| `0x1C82D` | `call 0x50D23` | **the only call to `0x50D23`**, then `jmp 0x1C7EC` (next rectangle) |
| `0x1C834..0x1C83A` | `cmp edi,[esi+0xC]; je; call 0x643CC` | advance, except on the last frame |
| `0x1C83F..0x1C85F` | `0x62756` / `0x50161(0xFF00FF00)` → `jne 0x1C86B`; `0x65240` wait loop | key exit, frame wait |
| `0x1C861..0x1C865` | `inc edi; cmp edi,[esi+0xC]; jbe 0x1C7BD` | loop; falls through to `0x1C86B` |
| `0x1C86B..0x1C873` | `push esi; call 0x63CE8; xor eax,eax; call 0x52106` | close, then the **exit blank** `0x52106(0)` |

Every edge out of the loop (`0x1C7AA`, `0x1C846`, `0x1C854` and the
fall-through at `0x1C865`) goes to `0x1C86B`, which runs `0x52106(0)` at
`0x1C873`. The only paths that skip the exit blank, `0x1C759`, `0x1C766` and
`0x1C77F`, leave before the loop, so they never reach `0x1C82D`. **Every
execution of `0x50D23` is followed by `0x52106(0)` at `0x1C873`, before
`0x1C740` returns.**

The callees of `0x1C740` are `0x52106`, `0x62756`, `0x6345C`, `0x649B0`,
`0x65340`, `0x64130`, `0x64ED8`, `0x50D23`, `0x643CC`, `0x50161`, `0x65240`
and `0x63CE8` (`prage.calls.csv`, and the capstone listing above). Every one
except `0x52106`, `0x50D23` and `0x50161` is runtime-library Smacker/keyboard
code at `>= 0x5D000`.

### §0.2 The function: `0x50D23` (4405 B, `0x50D23..0x51E57`)

The prologue (`0x50D23..0x50D5E`) is `pushad`, then EAX = the rectangle
pointer:

```
0x50D24 mov edx,[eax+4]          ; y0
0x50D27 mov ebp,[eax+0xC]
0x50D2A sub ebp,edx              ; rows = y1 - y0
0x50D2C mov edi,[edx*4+0x1088F8] ; row offset (DS_001088F8: i*0x140, 0x51F45)
0x50D33 mov edx,[eax]
0x50D35 and dl,0xFC              ; x0 & ~3
0x50D38 add edi,edx
0x50D3A mov esi,edi
0x50D3C mov ebx,edi
0x50D3E add edi,[0xE87A0]        ; EDI -> the DS_000E87A0 buffer
0x50D44 add esi,[0xE87A4]        ; ESI -> the DS_000E87A4 buffer
0x50D4A add ebx,0xA0000          ; EBX -> the VGA aperture
0x50D50 mov ecx,[eax+8]
0x50D53 add ecx,3
0x50D56 and cl,0xFC
0x50D59 sub ecx,edx
0x50D5B shr ecx,2                ; n = (((x1+3)&~3) - (x0&~3)) >> 2 dwords
0x50D5E mov edx,ecx
```

The row body (`0x50D60..0x51443` plus the store chain `0x5146C..0x51E53`) is
fully unrolled over 80 dword slots `k = 0..79` (offset `4k`, `0..0x13C`).
Each slot does `mov eax,[esi+4k]; cmp [edi+4k],eax`. When the dwords are
equal, it goes on to the next slot. When they differ, it runs
`mov [ebx+4k],eax; mov [edi+4k],eax` (for example `0x5146C mov [ebx],eax;
0x5146E mov [edi],eax`). After each slot, `dec ecx; je 0x5144F`. Slot 79
reaches `0x5144F` whatever ECX holds (`0x51449 jne 0x51E47`, `0x51E41 je
0x5144F`, `0x51E53 jmp 0x5144F`). The row step is `0x5144F add esi,0x140;
add edi,0x140; add ebx,0x140; mov ecx,edx; dec ebp; jne 0x50D60`, then
`0x5146A popad; ret`.

Whole-body scans (`k10_check.py` §1):
- 13 mnemonics: `add and cmp dec je jmp jne mov popal pushal ret shr sub`.
  There is no `call`, `int`, `in`, `out` or string op.
- Memory stores go only through EBX (80 stores, one per offset `0..0x13C`)
  and EDI (80 stores, the same offsets). There is no other store.
- Absolute memory operands: `0xE87A0`, `0xE87A4` and the table `0x1088F8`.
  Nothing else.
- The linear decode ends exactly at `0x50D23 + 4405`, on `0x51E53 jmp 0x5144F`.

### §0.3 Closed-form model (verified by interpretation)

With `n = (((x1+3)&~3) - (x0&~3)) >> 2` as an unsigned 32-bit value, and
`cnt = n` when `1 <= n <= 80`, else `80`:

```
for r in 0 .. (y1 - y0) - 1:                    # unsigned; y1 == y0 is 2^32 rows (unreachable, §0.5)
    off0 = DS_001088F8[y0] + (x0 & ~3) + r*0x140
    for k in 0 .. cnt - 1:
        o = off0 + 4k
        v = dword(E87A4 + o)
        if dword(E87A0 + o) != v:
            dword(0xA0000 + o) = v              # EBX, the aperture
            dword(E87A0 + o)   = v              # EDI, the shadow
```

`k10_check.py` §4 interprets the raw bytes and compares them with this model
on 308 rectangles: the full screen, the last row, `n == 0`
(`(4,5,4,6)`, `(8,10,8,12)`: 80 dwords), `x1 < x0` (`(100,50,5,51)`: 80
dwords), sub-dword rectangles, and 300 random ones. The buffers are random,
about half their dwords equal, and the aperture starts equal to the E87A0
buffer. Result: **0 mismatches**, and after every call the aperture equals
the E87A0 buffer over `0..0xFA00`. Two model mutations each produce FAIL:
`cnt = min(n,80)` (the `n == 0` rows) and dropping the EDI store.

### §0.4 The EDI half is the aperture's shadow, not independent state

- `0x52106(0)` at entry (`0x1C74D`) stores 0 in both tick counters
  (`0x52108/0x5210D`). It fills `[0x1014E8]`'s buffer (`0x52114/0x52119`),
  `[0x1014E4]`'s buffer (`0x5211E/0x52123`) and the aperture
  (`0x5214C mov eax,0xA0000; 0x52151 call 0x51F72`), each with the dword 0
  over `0xFA00` bytes (`0x51F72`, record §K2.4). So the aperture and the E87A0
  buffer are equal when the loop starts.
- `0x50D23` writes the aperture and the E87A0 buffer together, with the same
  value at the same offset (§0.2). Nothing else in `0x1C740`'s call tree
  writes either one. Of the absolute references to `0xE87A0` in the code
  object (§0.6), none is in a callee of `0x1C740` other than `0x50D23`. The
  Smacker library received only `[0xE87A4]`'s value (`0x1C794`), and a scan
  of the image finds no absolute reference to `0xE87A0` at `>= 0x5D000`.
- So during the movie the E87A0 buffer equals the aperture at every
  instruction boundary of `0x1C740`. Its only reader in that window is
  `0x50D23`'s own `cmp [edi+4k],eax`. `0x50D23`'s net effect on the screen is
  `aperture[o] = E87A4[o]` for every dword `o` of the expanded rectangle,
  because the compare only skips a store when the aperture already holds `v`.
  The interpreter confirms this.

### §0.5 The EDI half is dead when `0x1C740` returns

- The pointer pair `{[0xE87A0], [0xE87A4]}` is written only by `0x51F45`
  (`0x51F4B mov [0xE87A0],eax` from `[0x1014E4]`, `0x51F55 mov [0xE87A4],eax`
  from `[0x1014E8]`) and by the swap `0x50188` (`0x5018A..0x5019B`). The swap
  has only two callers, `0x256AA` in `0x255CC` and `0x2EAD6` in `0x2EA78`,
  and neither is in `0x1C740`'s call tree. `[0x1014E4]` and `[0x1014E8]` are
  written only by the init allocations `0x1B2AF/0x1B2C7` (E8) and
  `0x1B2F0/0x1B308` (E4), which are two separate `0x1C308(…, 0xFA00)` calls.
  So `[0xE87A0]` always names one of the two buffers that `0x52106` fills.
- The exit blank `0x52106(0)` at `0x1C873` follows every execution of
  `0x50D23` (§0.1). It overwrites both buffers, and so every byte that
  `0x50D23`'s EDI stores wrote, with zero before `0x1C740` returns. The one
  exception would be a store outside `[E87A0, E87A0+0xFA00)`, which needs a
  rectangle that leaves the 320x200 frame (`y1 > 200`, or `n == 0` / `n > 80`
  on row 199). The rectangles are the runtime `0x64ED8`'s output, which no
  raw byte in the port's scope bounds. So this is the one residue, and it is
  named here rather than assumed away. Such a store would hit the heap past
  the buffer in the original, which is undefined behaviour no oracle observes.
  Both shipped movies are 320x200 (`movie.c` `MOVIE_W/MOVIE_H`,
  `smk_width/smk_height`).
- No reader of the E87A0 buffer runs between the stores and the exit blank
  (§0.4), and the ISRs `0x1B908..0x1BDF4` hold no reference to `0xE87A0`
  (§0.6). **The EDI half has no observable effect outside `0x50D23` itself.**

### §0.6 Every absolute reference to `0xE87A0` (code object `0x10000..0x73B14`)

| site | owner | use |
|---|---|---|
| `0x2BBFD` (`0x2BBFB mov esi,[0xE87A0]`) | `0x2BAF4` | source of the `0xFA00` copy into `[0xE87A4]` (record §47-C; `actors.c:739`), after that function's own `0x52106` at `0x2BBEA` |
| `0x5018B`, `0x50197` | `0x50188` | the swap |
| `0x501A6` | `0x501A3` | the master-loop dirty blit (host-owned, record §K9.1) |
| `0x50D40` | `0x50D23` | this function |
| `0x51F4C` | `0x51F45` | the binding (`flow.c` `surface_setup`) |

### §0.7 The port today, and what the oracles prove

- `movie_play` (`movie.c`) decodes each frame into `mem + DSD(DS_000E87A4)`
  (`smk_decode_frame`, `platform/smacker.h`, the port's own decoder, which
  replaces the runtime Smacker library). It presents the **whole** buffer
  through `movie_present` → `gfx_present(mem + DSD(DS_000E87A4), w, h)`,
  which copies 320x200 into `g_aperture` (`gfx.c` `gfx_present`) and converts
  through `gfx_dac`. The entry and exit blanks are `gfx_screen_reset(0u)`
  (`0x1C74D`/`0x1C873`). It never writes the E87A0 buffer between them.
- The smk oracle (`make smk-oracle`, and in `make verify`) dumps every
  decoded full frame (`test_video.c` `decode_movie`, `PR_SMK_DUMP`) and
  compares it pixel-exact against DOSBox captures of the original's screen.
  That screen is the aperture after `0x50D23`'s rectangle blits. Baseline
  (ledger §A): `smk_compare: 120/120 frames match` and
  `smk_compare: 41/41 frames match`. So at every captured frame, the raw's
  rectangle-blitted aperture equals the full decoded frame the port presents.
  The front-end oracle's movie frames (capture 1886 and 2094, through
  `movie_set_screen_hook`) agree.
- The port's decoder exposes no dirty rectangles (`smacker.h`: `smk_open`,
  `smk_width`, `smk_height`, `smk_frames`, `smk_frame_delay_us`,
  `smk_decode_frame`, `smk_palette_to`). The raw's rectangles come from the
  runtime library's `0x64ED8` (the handle's `+0x684..+0x690`), and code at
  `>= 0x5D000` is not a porting target (`AGENTS.md`).
- Audio: `0x50D23` has no `call`, `in` or `out` (§0.2), so it has no audio
  effect.

### §0.8 Verdict drawn from §0

**(b) host-owned, the whole function.** The EBX half writes the VGA aperture,
which the aperture rule assigns to the host. The EDI half is that aperture's
shadow: equal to it at every instruction of `0x1C740` (§0.4), read only by
`0x50D23`'s own compare, and zeroed by `0x52106(0)` at `0x1C873` on every
path, before any other reader (§0.5). Its only observable output is
`aperture = E87A4` over each dirty rectangle, and the port's full-frame
`gfx_present` reproduces that pixel-exact at every captured frame (§0.7).

Why not (a) or (c):
- Porting the EDI half over `mem[]` needs the rectangles. The only raw source
  is the runtime `0x64ED8`, and the port's decoder has none.
- Feeding a synthetic full-screen rectangle `{0,0,320,200}` would be a
  constant the raw never passes. The function would then reduce to "copy
  E87A4 into E87A0 and the screen", a `mem[]` write that `0x52106` erases
  before anyone reads it.
- Either route adds a live write path with no observable effect. It would
  also risk the enforced movie lines (the smk oracle and the front-end
  captures 1886/2094) for no fidelity gain.

This matches the precedent `0x501A3` (§K9.1): the same E87A4-vs-E87A0 dirty
compare, replaced by `gfx_present`.

**Correction to the ledger (raw wins).** Ledger §B.1 and §G say "port
(partial): the EDI `mem[]` half is port", on the grounds that "it stores the
`E87A0` front buffer in `mem[]` through EDI". The store is real (§0.2), but
the buffer it writes is the aperture's shadow and is dead at `0x1C873` (§0.5),
so it is not state the port must carry.

---

## §K10.1 `0x50D23`: what it is

Status: confirmed (Task 1, 2026-09-29): k10_check.py 9/9 ok, both
mutants FAIL, listings match §0.1/§0.4/§0.5.

It takes a rectangle pointer in EAX (`{x0, y0, x1, y1}` dwords) and walks
`y1 - y0` rows of `cnt` dwords from `DS_001088F8[y0] + (x0 & ~3)`. For each
dword where the E87A4 buffer differs from the E87A0 buffer, it stores the new
dword to the aperture (EBX = `0xA0000 + off`) and to the E87A0 buffer (EDI).
`cnt` is `n` for `1 <= n <= 80`, else 80. It has no calls, no port I/O, and
no other stores. Its one caller is `0x1C82D` in `0x1C740`, with no absolute
reference.

## §K10.2 The caller `0x1C740` and the rectangle source

Status: confirmed (Task 1, 2026-09-29): k10_check.py 9/9 ok, both
mutants FAIL, listings match §0.1/§0.4/§0.5.

`0x1C82D` passes `ESP` → `{[h+0x684], [h+0x688], [h+0x684]+[h+0x68C],
[h+0x688]+[h+0x690]}`, which is the next rectangle that the runtime
`0x64ED8(h, 0)` returns. The loop repeats until `0x64ED8` returns 0.
Every loop exit reaches `0x1C873 call 0x52106` with EAX = 0.

## §K10.3 The EDI half is a dead shadow of the aperture

Status: confirmed (Task 1, 2026-09-29): k10_check.py 9/9 ok, both
mutants FAIL, listings match §0.1/§0.4/§0.5.

At `0x1C74D` the aperture and both buffers are filled with 0, so the E87A0
buffer equals the aperture. From then on `0x50D23` writes both together, and
nothing else in `0x1C740`'s call tree writes either one. `[0xE87A0]` always
names one of the two buffers `[0x1014E4]`/`[0x1014E8]`, because only `0x51F45`
and the swap `0x50188` write the pointer pair, and the swap is not in the
call tree. `0x52106(0)` at `0x1C873` zeroes both buffers on every path that
executed `0x50D23`. The E87A0 buffer's only reader in between is `0x50D23`'s
own compare.

Task 1 re-check, beyond the script (2026-09-29, LE mirror + capstone; the
Ghidra MCP was unavailable again):
- The transitive call tree of `0x1C740` over `prage.calls.csv` holds 281
  functions (47 below `0x5D000`, including `0x64ED8`'s game-code callees
  `0x102B8` and `0x10678`). It contains none of the functions that hold an
  absolute reference to `0xE87A0`, `0x1014E4` or `0x1014E8` other than
  `0x50D23`, `0x52106` and its fill `0x51F72`: `0x2BAF4`, `0x50188`,
  `0x501A3`, `0x51F45` and `0x1B120` are all outside it.
- Every absolute dword `0x1014E4`/`0x1014E8` in the image sits in `0x1B120`
  (the stores `0x1B2AF/0x1B2C7/0x1B2F0/0x1B308` and the null tests),
  `0x51F45` (read) or `0x52106` (read). None is in the data object, and none
  is in the ISRs `0x1B908..0x1BDF4`. So the buffer addresses reach no other
  global than `[0xE87A0]`/`[0xE87A4]`.
- `0x51F72` stores EDX over 200 rows of 80 dwords (`0x51F73 mov cl,0xc8`,
  `0x520F7 add eax,0x140`, `0x520FC dec cl; jne 0x51F78`), and `0x52106`
  keeps EDX = its EAX argument = 0 for all three fills (`0x52112 mov edx,eax`,
  `0x52128 push edx` / `0x5214B pop edx` around the DAC loop).
- `0x64ED8` (554 B, runtime Smacker, one caller) stores only to the handle
  (`[esi+0x684/0x688/0x68C/0x690]`, `[esi+0x6EC]`, `[esi+0x750/0x754/0x770]`)
  and to its own stack locals; its calls are `0x102B8`, `0x10678` and
  `0x61243`, all inside the call tree checked above. No correction to §0.

## §K10.4 The port's replacement and its evidence

Status: confirmed (Task 1, 2026-09-29): k10_check.py 9/9 ok, both
mutants FAIL, listings match §0.1/§0.4/§0.5.

`movie_play` → `movie_present` → `gfx_present(mem + DSD(DS_000E87A4), 320,
200)`, a full-frame copy into `g_aperture`, which is the net effect of
§K10.1 when the rectangles cover every changed dword. Evidence:
- `smk_compare: 120/120` and `41/41` (ledger §A) against the original's
  screen.
- The front-end oracle's movie frames.
- `test_movie_blit` (Task 2) pins the port-side premises: aperture ==
  E87A4 buffer at every screen the player writes, and both offscreen buffers
  zero at exit.

## §K10.5 Verdict

Status: confirmed (Task 1, 2026-09-29): k10_check.py 9/9 ok, both
mutants FAIL, listings match §0.1/§0.4/§0.5.

**host-owned.** `tools/port_classification.txt` row:
`50D23 host-owned record-§K10 (k10 movie-blit derivations: movie dirty-rect blit, E87A4 vs E87A0 dwords into the aperture (0x50D4A add ebx,0xa0000) and the E87A0 shadow (EDI); the shadow is zeroed by 0x52106(0) at 0x1C873 before any reader; only caller 0x1C82D; movie_present/gfx_present replaces it, smk oracle 120/120 + 41/41)`.

## §K10.6 Correction recorded against the ledger

Status: confirmed (Task 1, 2026-09-29): k10_check.py 9/9 ok, both
mutants FAIL, listings match §0.1/§0.4/§0.5.

Ledger §B.1 row `0x50D23` ("port (partial) / host-owned (aperture half)"),
§F row 17 ("the EDI `mem[]` half is port") and §G ("Explicitly not
classified host-owned … `0x50D23`: it stores the `E87A0` front buffer in
`mem[]` through EDI") are superseded by §K10.3–§K10.5. The EDI store exists,
but it writes the aperture's shadow, and that shadow is dead at `0x1C873`.

---

## Appendix A: `le.py` (the LE loader mirror)

Save it next to `k10_check.py`. `EXE` must point at the repo's
`data/game/C/PRAGE.EXE`.

```python
"""Mirror of port/src/mem.c mem_load_le + mem_load_le_fixups: returns a
bytearray image indexed by linear address (fixups applied)."""
import struct, sys
EXE = '/Users/felipe.dos.santos/code/mine/primal-rage-reverse/data/game/C/PRAGE.EXE'
rd32 = lambda d, o: struct.unpack_from('<I', d, o)[0]
rd16 = lambda d, o: struct.unpack_from('<H', d, o)[0]

def find_le(d):
    i = 0
    while True:
        i = d.find(b'LE\0\0', i)
        if i < 0: return -1
        cpu, os_ = rd16(d, i+8), rd16(d, i+10)
        npages, psz = rd32(d, i+0x14), rd32(d, i+0x28)
        nobj, objtab = rd32(d, i+0x44), rd32(d, i+0x40)
        if 1 <= cpu <= 5 and 1 <= os_ <= 4 and psz in (0x200,0x400,0x1000,0x2000,0x4000) and 0 < nobj < 64 and objtab < 0x2000 and 0 < npages < 20000:
            return i
        i += 1

def load():
    d = open(EXE, 'rb').read()
    le = find_le(d)
    npages, psz, last = rd32(d, le+0x14), rd32(d, le+0x28), rd32(d, le+0x2c)
    nobj, objtab, pagemap, pageoff = rd32(d, le+0x44), rd32(d, le+0x40), rd32(d, le+0x48), rd32(d, le+0x80)
    fpt, frt = rd32(d, le+0x68), rd32(d, le+0x6c)
    bound = -1
    i = 0
    while True:
        i = d.find(b'MZ', i)
        if i < 0 or i + 0x40 > len(d): break
        r = rd32(d, i+0x3C)
        if r and i + r == le: bound = i; break
        i += 1
    pagedata = (bound if bound >= 0 else le) + pageoff
    mem = bytearray(0x120000)
    for obj in range(nobj):
        e = le + objtab + obj*24
        virt, rel, pageidx, npg = rd32(d, e), rd32(d, e+4), rd32(d, e+12), rd32(d, e+16)
        for p in range(npg):
            idx = pageidx - 1 + p
            entry = rd32(d, le+pagemap+idx*4) if idx < npages else 0
            dst = rel + p*psz
            if entry == 0: continue
            phys = entry >> 16 or (pageidx + p)
            src = pagedata + (phys-1)*psz
            avail = min(max(len(d)-src, 0), psz)
            mem[dst:dst+avail] = d[src:src+avail]
    for obj in range(nobj):
        e = le + objtab + obj*24
        rel, pageidx, npg = rd32(d, e+4), rd32(d, e+12), rd32(d, e+16)
        for p in range(npg):
            page = pageidx + p
            begin, end = rd32(d, le+fpt+(page-1)*4), rd32(d, le+fpt+page*4)
            pagelen = last if (obj+1 == nobj and p+1 == npg) else psz
            cur = 0
            while cur < end - begin:
                r = le + frt + begin + cur
                src, tf = d[r], d[r+1]
                assert src == 7 and (tf & ~0x50) == 0
                so = struct.unpack_from('<h', d, r+2)[0]
                q = r + 4
                if tf & 0x40: tobj = rd16(d, q); q += 2
                else: tobj = d[q]; q += 1
                if tf & 0x10: toff = rd32(d, q); q += 4
                else: toff = rd16(d, q); q += 2
                base = rd32(d, le+objtab+(tobj-1)*24+4) or tobj*0x100000
                v = (base + toff) & 0xFFFFFFFF
                for k in range(4):
                    o = so + k
                    if 0 <= o < pagelen:
                        mem[rel + p*psz + o] = (v >> (8*k)) & 0xFF
                cur += q - r
    return mem

if __name__ == '__main__':
    m = load()
    open(sys.argv[1], 'wb').write(m)
```

## Appendix B: `k10_check.py` (re-derives §0.2, §0.3, §0.6 and the caller scan)

Run it from the repo root: `python3 <dir>/k10_check.py <dir>`, where `<dir>`
holds `le.py`. Planning-time output (exit 0):

```
ok   the linear decode covers all 4405 bytes (last 51e53 jmp 0x5144f)
ok   mnemonics: ['add', 'and', 'cmp', 'dec', 'je', 'jmp', 'jne', 'mov', 'popal', 'pushal', 'ret', 'shr', 'sub']
ok   stores only through EBX and EDI: ['ebx', 'edi']
ok   ebx: 80 stores, one per dword 0..0x13C
ok   edi: 80 stores, one per dword 0..0x13C
ok   absolute refs: ['0x1088f8', '0xe87a0', '0xe87a4']
ok   callers: rel ['0x1c82d'] abs []
ok   E87A0 refs: [('0x2bbfd', '0x2baf4'), ('0x5018b', '0x50188'), ('0x50197', '0x50188'), ('0x501a6', '0x501a3'), ('0x50d40', '0x50d23'), ('0x51f4c', '0x51f45')]
ok   interpreter == model on 308 rects; aperture == E87A0 buffer after every call
```

```python
"""Record §K10 check: static scans of 0x50D23 plus a byte-level interpreter of
its 4405 bytes compared against the closed-form model of §0.3.
Usage (repo root): python3 k10_check.py [dir holding le.py] -> prints the facts;
exit 0 only when every check holds."""
import os, sys, struct, random, csv
sys.path.insert(0, sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__)))
import capstone
from capstone import x86_const as X
import le
m = bytes(le.load())
A, SIZE = 0x50D23, 4405
M32 = 0xFFFFFFFF
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True
code = {i.address: i for i in md.disasm(m[A:A + SIZE], A)}
fail = 0
def check(c, msg):
    global fail
    print(('ok   ' if c else 'FAIL ') + msg); fail += 0 if c else 1

# 1. static scans
last = max(code)
check(last + code[last].size - A == SIZE, 'the linear decode covers all 4405 bytes (last %x %s %s)' % (last, code[last].mnemonic, code[last].op_str))
mn = {i.mnemonic for i in code.values()}
check(mn == {'add','and','cmp','dec','je','jmp','jne','mov','popal','pushal','ret','shr','sub'}, 'mnemonics: %s' % sorted(mn))
st = {}
absref = set()
for a, i in code.items():
    if i.mnemonic == 'mov' and i.operands[0].type == X.X86_OP_MEM:
        st.setdefault(i.reg_name(i.operands[0].mem.base), []).append(i.operands[0].mem.disp)
    for o in i.operands:
        if o.type == X.X86_OP_MEM and o.mem.base == 0:
            absref.add(o.mem.disp)
check(sorted(st) == ['ebx', 'edi'], 'stores only through EBX and EDI: %s' % sorted(st))
for r in ('ebx', 'edi'):
    check(len(st[r]) == 80 and sorted(st[r]) == list(range(0, 0x140, 4)), '%s: 80 stores, one per dword 0..0x13C' % r)
check(absref == {0xE87A0, 0xE87A4, 0x1088F8}, 'absolute refs: %s' % sorted(hex(x) for x in absref))

# 2. callers: rel32 call/jmp/jcc into 0x50D23, and absolute dwords equal to it
rel, ab = [], []
for i in range(0x10000, 0x73B14 - 5):
    if m[i] in (0xE8, 0xE9) and (i + 5 + struct.unpack_from('<i', m, i + 1)[0]) & M32 == A: rel.append(i)
    if m[i] == 0x0F and 0x80 <= m[i + 1] <= 0x8F and (i + 6 + struct.unpack_from('<i', m, i + 2)[0]) & M32 == A: rel.append(i)
j = 0
while (j := m.find(struct.pack('<I', A), j)) >= 0: ab.append(j); j += 1
check(rel == [0x1C82D] and ab == [], 'callers: rel %s abs %s' % ([hex(x) for x in rel], [hex(x) for x in ab]))

# 3. every absolute reference to 0xE87A0 and its owner
fx = sorted((int(r['entry'], 16), int(r['size'])) for r in csv.DictReader(open('port/decomp/prage.functions.csv')) if '::' not in r['entry'])
own = lambda a: next((hex(e) for e, s in reversed(fx) if e <= a < e + s), '-')
refs = []; j = 0x10000
while (j := m.find(struct.pack('<I', 0xE87A0), j)) >= 0 and j < 0x73B14: refs.append((hex(j), own(j))); j += 1
check([r[1] for r in refs] == ['0x2baf4', '0x50188', '0x50188', '0x501a3', '0x50d23', '0x51f45'], 'E87A0 refs: %s' % refs)

# 4. interpreter vs model
def run(mem, rectp, e0, e4, rowtab):
    R = dict(eax=rectp, ebx=0, ecx=0, edx=0, esi=0, edi=0, ebp=0); zf = False
    def ea(i, o):
        x = o.mem.disp
        if o.mem.base: x += R[i.reg_name(o.mem.base)]
        if o.mem.index: x += R[i.reg_name(o.mem.index)] * o.mem.scale
        return x & M32
    def val(i, o):
        if o.type == X.X86_OP_REG:
            n = i.reg_name(o.reg); return R[n] if n in R else R['e' + n[0] + 'x'] & 0xFF
        if o.type == X.X86_OP_IMM: return o.imm & M32
        a = ea(i, o)
        if a == 0xE87A0: return e0
        if a == 0xE87A4: return e4
        if 0x1088F8 <= a < 0x1088F8 + 800: return rowtab[(a - 0x1088F8) // 4]
        return struct.unpack_from('<I', mem, a)[0]
    def setr(i, o, v):
        n = i.reg_name(o.reg)
        if n in R: R[n] = v & M32
        else: f = 'e' + n[0] + 'x'; R[f] = (R[f] & ~0xFF) | (v & 0xFF)
    pc = A
    while True:
        i = code[pc]; nx = pc + i.size; k = i.mnemonic; o = i.operands
        if k == 'ret': return
        if k == 'mov':
            v = val(i, o[1])
            if o[0].type == X.X86_OP_MEM: struct.pack_into('<I', mem, ea(i, o[0]), v)
            else: setr(i, o[0], v)
        elif k in ('add', 'sub', 'and', 'shr', 'dec'):
            a = val(i, o[0]); b = val(i, o[1]) if len(o) > 1 else 1
            w = 0xFF if i.reg_name(o[0].reg) in ('cl', 'dl') else M32
            r = {'add': a + b, 'sub': a - b, 'and': a & b, 'shr': a >> b, 'dec': a - 1}[k] & w
            setr(i, o[0], r); zf = r == 0
        elif k == 'cmp': zf = val(i, o[0]) == val(i, o[1])
        elif k == 'je' and zf or k == 'jne' and not zf or k == 'jmp': nx = o[0].imm
        pc = nx

def model(mem, rect, e0, e4, rowtab, ap):
    x0, y0, x1, y1 = rect
    n = ((((x1 + 3) & ~3) - (x0 & ~3)) & M32) >> 2
    cnt = n if 1 <= n <= 80 else 80
    base = rowtab[y0] + (x0 & ~3)
    for r in range((y1 - y0) & M32):
        for k in range(cnt):
            off = base + r * 0x140 + 4 * k
            v = struct.unpack_from('<I', mem, e4 + off)[0]
            if struct.unpack_from('<I', mem, e0 + off)[0] != v:
                struct.pack_into('<I', mem, e0 + off, v); struct.pack_into('<I', mem, ap + off, v)

random.seed(0x50D23)
B0, B1, RECT, AP = 0x10000, 0x20000, 0x3FF00, 0xA0000
rowtab = [i * 0x140 for i in range(200)]
cases = [(0,0,320,200), (0,199,320,200), (1,0,2,1), (3,5,4,6), (4,5,4,6), (8,10,8,12), (316,0,320,3), (100,50,5,51)]
for _ in range(300):
    x0 = random.randrange(320); x1 = random.randrange(x0, 321); y0 = random.randrange(199); y1 = random.randrange(y0 + 1, 200)
    cases.append((x0, y0, x1, y1))
bad = 0
for rect in cases:
    mem = bytearray(0xB0000)
    mem[B1:B1 + 0x10000] = random.randbytes(0x10000)
    mem[B0:B0 + 0x10000] = random.randbytes(0x10000)
    for o in range(0, 0x10000, 4):
        if random.random() < 0.5: mem[B0 + o:B0 + o + 4] = mem[B1 + o:B1 + o + 4]
    mem[AP:AP + 0x10000] = mem[B0:B0 + 0x10000]          # the entry invariant: aperture == E87A0 buffer
    struct.pack_into('<4I', mem, RECT, *rect)
    ref = bytearray(mem)
    run(mem, RECT, B0, B1, rowtab)
    model(ref, rect, B0, B1, rowtab, AP)
    if mem != ref or mem[AP:AP + 0xFA00] != mem[B0:B0 + 0xFA00]: bad += 1; print('MISMATCH', rect)
check(bad == 0, 'interpreter == model on %d rects; aperture == E87A0 buffer after every call' % len(cases))
sys.exit(1 if fail else 0)
```
