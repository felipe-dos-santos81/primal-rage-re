# Host Input Truth derivations

Plan `2026-10-21-host-input-truth.md`, spec `../specs/2026-10-21-host-input-truth-design.md`.

Sources: capstone over the fixed-up image (`./build/diffrun --exe data/game/C/PRAGE.EXE
--image-out /tmp/hit.img`; image offset = address − `0x10000`), the Ghidra listing
(`port/decomp/prage.c`, project `primal-rage`, `/PRAGE.EXE`), the decomp indexes, and
`tools/smk_info.py` over the shipped movie files. Every address below is a linear
code-object address (`base 0x10000`), as elsewhere in this repo.

§H1 is Task 1: the movie audio is a **zero payload** on the shipped data. It ports
nothing; it records the chain that never opens a stream, corrects one label, and gives the
13 deferred rows in `tools/port_classification.txt` and `port/spec/audio.md` their
evidence.

## §H1 The movie audio is a zero payload on the shipped data

### §H1.1 The request: all seven tracks, `0x1C740` -> `0x6345C`

`0x1C740` (the boot-logo player, ported as `movie_play`) opens the movie at
`0x1C76C-0x1C774`:

```
01C76C  push esi            ; the movie name (esi = 0x1C740's parameter)
01C76D  push 0xFE00         ; the open flags: bits 9..15 = one per audio track
01C772  push -1             ; the "extra" buffer size: -1 = compute it (0x6383C)
01C774  call 0x6345C        ; the game's SmackOpen
```

`0x6345C` reads the flags at `[esp+0x20]` and the name at `[esp+0x24]` (Watcom pushes
left to right, so the first source argument sits highest; the `-1` at `[esp+0x1c]` is the
third argument and is read at `0x6383C` as "compute the buffer size"). `0xFE00`'s second
byte is `0xFE` = seven bits: `0x63987` tests it before the loop (`test byte [esp+0x21],0xFE;
je 0x63A37`), the loop at `0x63992..0x63A2C` consumes one bit per iteration — starting at
`0x200` (track 0) and doubling to `0x8000` (track 6), `0x6399D..0x639BA` — and its tail
re-tests the same byte (`0x63A2C test byte [esp+0x21],0xFE; jne 0x63992`). So the open asks
for **all seven** tracks; whether a track carries audio is the file's own decision, read
per track below.

Before the loop `0x63970` writes `[movie+0x728] = 0xFFFFFFFF` — "no stream open". That is
the field `0x65240` tests (§H1.5) and the field `0x63180` sets on its first success
(§H1.2).

### §H1.2 The gate: `0x63180` opens a stream only for a track with bit `0x40`

For track `i`, `0x63180` indexes the movie's seven track dwords at `movie+0x48+4*i` (the
offsets `tools/smk_info.py` reads) and tests the dword's **high byte**:

```
063197  mov edx,[eax+0x48]
06319A  shr edx,0x18
06319D  test dl,0x40
0631A0  je 0x631AC           ; bit clear -> local rate = 0
0631A2  mov eax,[eax+0x48]
0631A5  and eax,0xFFFFFF      ; bit set -> the rate is the low 24 bits
...
0631AE  mov [esp],eax
0631B1  jne 0x631BA           ; ZF from the 0x6319D test: bit set -> open
0631B3  xor al,al
0631B5  jmp 0x63418           ; bit clear -> return 0, no stream
```

So a track dword is `rate:24 | flags:8`, and bit `0x40` of the flags byte is the only
"this track has audio" bit. The `jne` at `0x631B1` re-uses the `test`'s ZF across the
`mov` at `0x631AE`, so the gate's result survives the assignment; the closed-gate return
is `al = 0` at `0x631B3` into the epilogue at `0x63418`.

The open path is the rest of the function: it allocates the track's stream buffer
(`0x63305 call 0x62770`), stores the rate at `[track+0x27c]` (`0x6335A`), calls the stream
opener `0x1013C` (`0x63387`), and on success records the first opened track
(`0x633F4 cmp [movie+0x728],-1; 0x63403 mov [movie+0x728],eax`) and returns 1
(`0x63410 mov al,1`). `0x1013C` is what allocates the AIL sample handle and links the
track into `DAT_00081E00`, the list `0x102B8` walks (`0x1013C`'s only caller is `0x63180`,
callers column in `port/decomp/prage.calls.csv`). On a 0 return the caller frees the
0x283-byte buffer (§H1.4).

### §H1.3 The descriptors are zero in both shipped movies

Run from the repo root:

```
$ python3 tools/smk_info.py data/game/C/TWI5.SMK
data/game/C/TWI5.SMK: 320x200 frames=121 pts_inc=4294960196
  treesize=40763 tree_sizes(mmap,mclr,full,type)=(31472, 2760, 109136, 1984)
  audio_descriptors=[(0, 0), (0, 0), (0, 0), (0, 0), (0, 0), (0, 0), (0, 0)]  audio_frame_flags=[]
  keyframes(frame_size bit0)=[]
  palette_change_frames=3 first=[0, 101, 102]
  layout: tbl=0x68 trees=0x2c5 data=0xa200 end=1208576

$ python3 tools/smk_info.py data/game/C/TWG.SMK
data/game/C/TWG.SMK: 320x200 frames=41 pts_inc=4294953096
  treesize=5835 tree_sizes(mmap,mclr,full,type)=(1088, 1824, 17328, 1488)
  audio_descriptors=[(0, 0), (0, 0), (0, 0), (0, 0), (0, 0), (0, 0), (0, 0)]  audio_frame_flags=[]
  keyframes(frame_size bit0)=[]
  palette_change_frames=1 first=[0]
  layout: tbl=0x68 trees=0x135 data=0x1800 end=31048
```

Each descriptor is `(rate = low 24 bits, flags = high byte)`. Both files carry seven
`(0, 0)` descriptors and no frame carries any of the low seven audio flag bits
(`audio_frame_flags=[]`). The raw track table at `0x48..0x63` is all zero in both files:

```
TWI5.SMK 0x48..0x63: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
TWG.SMK  0x48..0x63: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

`0x6319D` therefore takes the `je` on every one of the seven tracks.

### §H1.4 The result: no stream opens

`0x63180` returns 0 for all seven tracks. `0x6345C`'s loop frees each 0x283-byte track
buffer through `0x1C8CC` (`0x63A0C..0x63A13`), subtracts `0x283` from `[movie+0x748]`
(`0x63A18`) and zeroes the slot `[movie+0x70c+4*i]` (`0x63A22`). Nothing calls `0x1013C`,
so `DAT_00081E00` stays 0, `[movie+0x728]` stays `-1` (set at `0x63970`, never
overwritten because `0x63403` is on the open path only), and the open-stream count
`DAT_000EF954` stays 0 (`0x6340F..0x63412` is on the open path too).

The stream pump `0x102B8` is therefore never entered during the movie: every movie caller
is guarded — `0x6279C` only when `[0xEF954] != 0` (`0x627FB`/`0x628B9`), `0x63E30` only
when `[movie+0x728] != -1` (`0x63E3D`), and `0x65240` only on its stream arm (`0x6524F`,
§H1.5). Were it called it would walk an empty list (`DAT_00081E00 == 0`, `0x102B8`'s head
test at `port/decomp/prage.c` line 160).

### §H1.5 `0x65240`'s no-audio path is a pure time wait

`0x1C740`'s frame wait calls `0x65240` at `0x1C857` in the loop `0x1C83F..0x1C85F`
(`0x1C85C test ax,ax; 0x1C85F jne 0x1C83F`): it spins while the wait reports "keep
waiting". With no stream open, `0x65240` takes the time path:

```
065240  push esi / push ebp
065242  mov esi,[esp+0xc]              ; the movie
065246  cmp dword [esi+0x728],-1
06524D  je 0x652AC                     ; no stream -> the time path
06524F  call 0x102B8                   ; (stream arm only)
...
0652AC  cmp dword [esi+0x700],0
0652B3  je 0x652BC                     ; not late -> the time path
0652B5  xor eax,eax / ret 4            ; late -> return 0
0652BC  call 0x10678                   ; t = milliseconds
0652C1..0652D3                         ; eax = 100*t  (the deadline unit)
0652CD  mov ecx,[esi+0x704]            ; the next-frame deadline
0652D6  test ecx,ecx
0652DA  [esi+0x704] = 100*t + [esi+0x708]   ; first frame
0652EA  cmp eax,ecx
0652EC  jbe 0x6532D                    ; not yet due -> return 1
0652EE..06530D                         ; advance the deadline by [esi+0x708]; if the
                                       ;   frame is more than one duration late, put the
                                       ;   deadline one duration past now
065313  cmp dword [esi+0x708],0
06531A  je 0x65326
06531C  mov dword [esi+0x700],1        ; late flag
065326  xor eax,eax / ret 4            ; done -> return 0
06532D  mov eax,1 / ret 4              ; keep waiting -> return 1
```

`[movie+0x708]` is the per-frame duration in 1/100 ms units: `0x635F8..0x6360B` stores
`[movie+0x708] = -delay` and `[movie+0x10] = -delay/100` for a negative header delay, and
`0x63610..0x63621` stores `[movie+0x708] = delay*100` for a positive one (both arms put
the duration in the same unit as `100*t`). TWI5's header delay `-7100` gives `0x708 =
7100` = 71 ms, which is what the port's `smk_frame_delay_us` returns as 71000 µs
(`*10`). `[movie+0x704]` is the deadline, `[movie+0x700]` the "we are late" flag. No audio
call is made on this path.

### §H1.6 `0x10678` is the millisecond reader, not a buffer size

```
010678  mov eax,[0x81E10]
01067D  shl eax,2
010680  ret
```

The 250 Hz callback `0x10604` increments `[0x81E10]` once per 4 ms tick
(`010604 mov eax,[0x81E10]; 010609 inc dword [0x81E10]; 01060F ret`), so `[0x81E10]`
counts 4 ms units and `0x10678` returns **milliseconds** (`[0x81E10]*4`). The §50-E table
(`2026-09-24-demo-pose-derivations.md`, the `0x10678` row) labels it
"`eax = [0x81E10] << 2`, the stream buffer size": the shift is right, the label is wrong.
The same `[0x81E10]*4` is the time base of `0x102B8` (`0x10360`/`0x10371`) and `0x100DC`
(`0x100E8`/`0x100EE`), never a size. This record supersedes that label; the `10678`
classification row carries the correction.

### §H1.7 The conclusion: no movie-audio oracle is possible on the shipped data

Both shipped movies declare zero audio tracks, so the original opens no stream, the pump
is never entered (and would walk an empty list), and `0x65240` runs the time path. There
is nothing to record, capture or compare: an oracle over the shipped data would compare
two empty streams. A synthetic movie with an audio track would exercise a movie the game
never plays and would still not say what the game does with the shipped files. Therefore:

- The 13 deferred rows in `tools/port_classification.txt` stay deferred, their evidence
  now "zero payload on the shipped data; no stream opens; §H1" — a positive finding, not
  a missing oracle: `10024 10034 100C4 100DC 10510 10570 10604 10610 10678 10684` and
  `102B8 1013C 10A68`.
- `port/spec/audio.md`'s movie-audio section states the same finding.
- The port adds no movie-audio code. `movie.c` decodes and presents, and paces by the
  header's frame delay (`movie_pace`); that is what the original's time path does on these
  files. The video oracles (`smk-oracle`, the front-end/attract windows) remain the
  movie-path proof and are unaffected.

Named gap: nothing here covers a hypothetical movie with a nonzero track dword. The gate,
the stream opener and the pump are recorded above but unexercised by the shipped data; and
`0x102B8`/`0x1013C` remain the §49-W.2 named gap for their host-pointer reason (an AIL
sample handle cannot be stored in a 32-bit `mem[]` word), independent of this finding.
