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

### §H1.2 The gate: `0x63180` opens a stream only for a track with bit `0x40` and a nonzero rate

For track `i`, `0x63180` indexes the movie's seven track dwords at `movie+0x48+4*i` (the
offsets `tools/smk_info.py` reads) and tests the dword's **high byte**:

```
063197  mov edx,[eax+0x48]
06319A  shr edx,0x18
06319D  test dl,0x40
0631A0  je 0x631AC           ; bit clear -> local rate = 0
0631A2  mov eax,[eax+0x48]
0631A5  and eax,0xFFFFFF      ; bit set -> the rate is the low 24 bits; ZF = (rate == 0)
0631AA  jmp 0x631AE
0631AC  xor eax,eax           ; bit clear -> rate 0, ZF = 1
0631AE  mov [esp],eax         ; stores the rate; does not touch flags
0631B1  jne 0x631BA           ; ZF from 0x631A5's and (or 0x631AC's xor): nonzero rate -> open
0631B3  xor al,al
0631B5  jmp 0x63418           ; zero rate -> return 0, no stream
```

So a track dword is `rate:24 | flags:8`, and bit `0x40` of the flags byte is the only
"this track has audio" bit — but it is **necessary, not sufficient**. The `jne` at
`0x631B1` reads the ZF the rate path left: the `and eax,0xFFFFFF` at `0x631A5`
(ZF = rate == 0) or, on the bit-clear arm, the `xor eax,eax` at `0x631AC` (ZF = 1). The
`mov [esp],eax` at `0x631AE` does not touch flags, so a set bit with a zero low-24-bit
rate also returns 0. The decomp agrees: `port/decomp/prage.c:45074-45084` sets
`bVar9 = true` on the bit-clear arm and `bVar9 = local_28 == 0` on the bit-set arm, and
returns 0 when `bVar9`. The closed-gate return is `al = 0` at `0x631B3` into the epilogue
at `0x63418`.

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

The stream pump `0x102B8` is therefore never entered during the movie: every movie-path
caller is guarded — `0x6279C` only when `[0xEF954] != 0` (`0x627FB`/`0x628B9`), `0x63E30`
only when `[movie+0x728] != -1` (`0x63E3D`), `0x65240` only on its stream arm (`0x6524F`,
§H1.5), `0x643CC` (called from `0x1C740` at `0x1C83A`) only when `[0xEF954] != 0`
(`0x643D2`), `0x64ED8` (called from `0x1C740` at `0x1C7EF`) only when `[0xEF954] != 0`
(`0x64EE2`), and `0x640B8` (called from `0x64130` at `0x64187`) only when
`[movie+0x728] != -1` (`0x640BE`). `0x640B8` reaches the pump a second time through
`0x100DC` (`0x640FC`; `0x100DC` calls `0x102B8` unguarded at `0x100E3`), inside that same
guard. Were it called it would walk an empty list (`DAT_00081E00 == 0`, `0x102B8`'s head
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

## §H2 The keyboard readers and the host key-state table

Task 2 ports the six keyboard readers `0x1BBAC`'s device-0 arms call, and gives the host the
table they read. Sources: capstone over `/tmp/hit.img` (the fixed-up image; image offset =
address − `0x10000`) for the 32-bit readers, and `data/game/C/RM.DRV` disassembled as 16-bit
code (`CS_MODE_16`) for the handler that writes the table. The plan's command
`python3 tools/diff_verify.py --function 0x1B610` names no spec (`--function` takes the spec
name): the rows were run as `--function input_dirs_p1` etc.

### §H2.1 The six readers

Each is argument-free and returns through AL. `ECX` is `[0x101514]`, the driver block's base;
a slot byte from `+0x2DE..+0x2ED` indexes the table at `+0x254`, and the reader sets its game
bit when the entry's **bit 7 is clear**:

```
1B610  push ebx / push ecx / push edx
1B613  mov ecx,[0x101514]
1B619  xor ebx,ebx
1B61B  mov edx,ecx
1B61D  mov bl,[ecx+0x2de]        ; the slot's scan
1B623  mov dl,[edx+ebx+0x254]    ; the key-state byte
1B62A  xor al,al
1B62C  and dl,0x80
1B62F  and edx,0xff
1B635  jne 0x1b639              ; bit 7 set -> no game bit
1B637  mov al,0x80              ; bit 7 clear -> the bit
1B639  ... the next slot (0x2df, 0x2e0, 0x2e1), then
1B693  mov [0x101514],ecx       ; re-stores the base it read (a no-op)
1B699  pop edx / pop ecx / pop ebx / ret
```

The slot groups and bits:

| function | slots | bits (in slot order) | store of the base |
|---|---|---|---|
| `0x1B610` `input_dirs_p1` | `+0x2DE..+0x2E1` | `0x80/0x40/0x20/0x10` | `0x1B693` |
| `0x1B6A0` `input_dirs_p2` | `+0x2E6..+0x2E9` | `0x80/0x40/0x20/0x10` | `0x1B723` |
| `0x1B730` `input_buttons_p1` | `+0x2E2..+0x2E5` | `1/2/4/8` | `0x1B7B3` |
| `0x1B7C0` `input_buttons_p2` | `+0x2EA..+0x2ED` | `1/2/4/8` | `0x1B843` |
| `0x1B850` `input_f1` | fixed `+0x28F` | `1` | none |
| `0x1B870` `input_f2` | fixed `+0x290` | `1` | none |

`0x1B850`/`0x1B870` are two instructions of substance: `mov ah,[edx+0x28f]` (`+0x290`),
`and ah,0x80`, then `test edx,edx; jne; mov al,1` (`0x1B859..0x1B86A`). They therefore also
write AH, which is why their row's EAX mask is `0xFF`. The port's versions take the base
write of the raw's first read (no separate store), and `PR_SEAM_RET0` at each address so the
sampler's row (Task 4) can record the call.

### §H2.2 The table is the driver's make/break bytes, not the BIOS's 0x417 table

`RM.DRV`'s keyboard handler writes the table:

```
030D  push ax / push bx / push ds
0310  mov ax,cs / mov ds,ax      ; DS = the driver's own segment (the block [0x101514])
0314  in al,0x60                 ; the raw set-1 scan byte: make, or break | 0x80
0316  mov bx,ax
0318  and bx,0x7f                ; the index is 7 bits
031B  mov [bx+0x254],al          ; the whole byte, break flag included
```

So an entry is the key's make byte (`0x1E` for A) while down and its break byte (`0x9E`)
while up, and **bit 7 clear means down**. That is the opposite of the standard BIOS key-state
table's "bit 7 = pressed", and it is why the readers set the game bit on a CLEAR bit 7. The
table is **128 entries**, `+0x254..+0x2D3`; `+0x2D4` is the config block's first device word
(`DSW(0x101514+0x2D4)` is P1's), so the 16 scan slots at `+0x2DE..+0x2ED` sit after it and the
table and the slots do not overlap. The readers index with the full slot byte, which is 0..0x7F
for every binding the config path can produce (the service menu's key queue and the default
record both carry set-1 make codes). The F1/F2 entries are the table's `0x3B`/`0x3C`
(`+0x254+0x3B = +0x28F`, `+0x254+0x3C = +0x290`), which is why those two reads are "fixed".

A correction from the first draft of the fixture: seeding 256 entries (`+0x254..+0x353`) also
overwrote the slots at `+0x2DE`, and the case's slot bytes became the table's own `0x8A..`
values; the row's original side then read `[0x2DE] = 0x8A` and returned 0. The 128-entry table
is the raw's shape, and the fixture seeds exactly it.

### §H2.3 The rows (`tools/diff_verify.py`) and their mutants

Fixture `h2_key_case`: `[0x101514] = 0x10A000` (in-image BSS, between seeded dwords at
`0x101510`), the 128 table entries all break (`scan|0x80`), the case's scan(s) make
(`scan`), the 16 slot scans at `+0x2DE..+0x2ED`, and `raw={scan: byte}` for the F mask cases.
All 16 slot scans are distinct, and each make byte has bit 6 set (`0x40..0x4F`), so a mutant
that masks 0x40 instead of 0x80 sees a pressed key as up. Six rows, `eax_mask=0xFF`, no calls:

| row | cases (expected AL) | mutants |
|---|---|---|
| `input_dirs_p1` | up `0x80`, down `0x40`, left `0x20`, right `0x10`, both `0x90` | `@mask @bits @slot @table @all` |
| `input_dirs_p2` | the same four plus both `0x90` | the same five |
| `input_buttons_p1` | b1 `1`, b2 `2`, b3 `4`, b4 `8`, b1+b4 `9` | the same five |
| `input_buttons_p2` | the same | the same five |
| `input_f1` | make `1`, break `0`, `0x7F` entry `1` | `@mask @off @inv` |
| `input_f2` | make `1`, break `0`, `0x7F` entry `1` | `@mask @off @inv` |

The "both" cases prove the four positions are OR'd (a last-wins read passes every
single-press case). `@bits` swaps the output bit order, `@slot` reads slot `i+1`, `@table`
indexes `+0x253`, `@all` returns `0xF0`/`0x0F`, `@off` reads the other F reader's entry, and
`@inv` inverts. `0x7F` (bit 7 clear, bit 6 set) is what catches a 0x40-mask mutant on the F
rows; the F1 down case also seeds EAX `0x5A5A5A5A`, so the raw's AH write is visible to the
0xFF mask as nothing.

Bindings (`port/tests/diff_runner.c`, `h2_nib_core`/`h2_f_core` and their wrappers, table
entries after `input_select_bits`): none of the six has a callee, so all six rows are full
(no pokes, no unhit blocks, no terminal difference). `--self-check` for each: VERIFIED,
9/9 blocks per nibble row (3/3 per F row), and 5/5 or 3/3 mutants detected (26 mutants in
all). RED before the port side existed: `--function input_dirs_p1` on the pre-batch `diffrun`
printed `port: unknown binding input_dirs_p1` on every case (MISMATCH).

### §H2.4 The host table and the scancode rule (`port/src/host.c`)

`host_input_fill_keyboard()` writes the table the readers read:

- Source of truth is `host_key_bits()`, which is the 16-bit kb word `game_loop` copies today
  (`0x2D8 << 8 | 0x2D9`) and the seam `host_set_key_bits_override` holds. With the seam on,
  the captured bitmap decides each slot; off, SDL's keyboard does. That keeps the drivers'
  replays byte-identical when the sampler replaces the direct store (Task 4).
- The 16 scans are the configured block's `+0x2DE..+0x2ED`, else the default record's
  (S X Z C / U I N M / arrows / Home PgUp End PgDn) when the block is absent or all zero:
  `host_key_scans()`.
- Each bound slot is written every frame: the make byte `scan&0x7F` when its bit is down, the
  break byte `(scan&0x7F)|0x80` when not (`host_key_state_store`, the RM.DRV handler's own
  index mask). The fixed F1/F2 entries `+0x28F/+0x290` are written from bits `0x0100`/`0x0001`
  in both paths; a slot bound to F1/F2 is the same table entry, so either source makes it
  (the write is the OR).
- A scan above 0x7F is not a set-1 make code and is skipped when sampling SDL (the table index
  is masked); the fill still writes it at the masked index, which is the raw handler's own
  behaviour.
- Guard: `DS_00101514` must be non-zero and `<= MEM_SIZE - 0x2EE` (the 16 slots' end); the
  table writes land in `+0x254..+0x2D3` only. Outside `build/diffrun`, nothing calls the fill
  yet (Task 4 wires `game_loop`).

`host_key_bits()` now samples **every configured slot's scan**, through `k_scan_sdl`
(set-1 0x01..0x58 -> `SDL_Scancode`, `0` = `SDL_SCANCODE_UNKNOWN`), instead of walking the 26
letters and 10 pad keys. The output bits are identical for every binding the old loop could
see (an unbound scan produced no bit then either); a rebind outside those scancodes -- a digit,
punctuation, F3..F10, Delete -- is now sampled. Where two keys share a set-1 code (the keypad's
`0x47..0x53` and the navigation keys, which int 16h cannot tell apart) the map keeps the key
the default record binds -- arrows, Home PgUp End PgDn -- exactly the first cut's choice.
`host_kb_bit()`'s default fallback is now the same 16-scan table (`k_default_scan` +
`k_cfg_bit`) plus the two fixed F1/F2 bits; its assertions in `test_host` are unchanged.

### §H2.5 The unit test and its mutation proofs

`port/tests/test_platform.c`, `test_input_keyboard_readers` (registered once in `test.h`).
It saves `DS_00101514`, points it at `TEST_KEY_BASE` (`0x3000000`, game_init's
`GAME_BIOS_BASE`), and asserts, on a seeded table:

- each of the four positions of all four slot readers, one bit at a time (16 assertions), and
  the OR of two positions (`0x90`);
- F1/F2 make/break, and that one is not the other;
- the fill: a configured slot naming `0x06` ('5', outside the 26 letters + 10 pad keys)
  reaches the table as the make byte while its bit is in the seam, the other slot as break,
  and `input_dirs_p1()` reads it back; with the seam off it becomes break (no key);
- with no block, the fill uses the default record's scans (`0x1F` make for S, `0x48` break for
  Up) and the fixed F1 entry.

Each assertion was seen to fail under a mutation, applied alone and reverted:

| mutation | first FAIL |
|---|---|
| `input_slot_bits`: `& 0x80` -> `& 0x40` | `test_platform.c:3285: 0 != 128` |
| `input_slot_bits`: `+0x254` -> `+0x253` | `3285: 64 != 128` |
| `input_buttons_p2`: slot base `0x2EA` -> `0x2EB` | `3300: 0 != 1` |
| `input_f1`: `+0x28F` -> `+0x290` | `3313: 0 != 1` |
| `input_f2`: mask `0x80` -> `0x01` | `3314: 1 != 0` |
| fill: `down` forced to 0 (always break) | `3332: 134 != 6` |
| fill: `host_key_scans()` -> the default list always | `3332: 0 != 6` |
| fill: `host_key_scans()` drops the default fallback (`return cfg`) | `3346: 0 != 31` |

Named limit: the SDL half of `host_key_bits()` (the `k_scan_sdl` lookup) cannot be asserted
headless -- the suite never initializes SDL's keyboard, and the seam short-circuits the scan.
The fill's rebind assertion proves the slot list is used and the reader reads it back; the map
itself is host policy (SDL scancodes are physical positions), and the default binding's 18
keys are covered by every capture oracle, which would move if the map were wrong for them.

### §H2.6 Counters

The differential step alone (`python3 tools/diff_verify.py --self-check`, this batch):
**1013/1013 functions VERIFIED; 14926/14926 mutants detected; 1 named gap; 774/832 rows
with callees closed (181 have none); 768/832 closed on full rows; 8 ported functions with no
row.** The six new rows have no callees, so they raise the total row count (832 + 6) and the
"have none" count (175 -> 181) without changing the closed pair; the six rows and 26 mutants
are all VERIFIED/detected, and every existing row and mutant is unchanged.
`tools/port_progress.py`: `793 1203 66`, `753 753 100`. The six classification rows in
`tools/port_classification.txt` still read `host-owned` and the README title still reads 65%:
Task 7 (closeout) moves them with the other six of the phase's 12 ported functions.

No existing pin is expected to move: the default device-0/0 path samples the same 18 scans and
produces the same kb word, and the build's `--check` frames, the oracle-required tests, the
restart driver and the smk/title/front-end oracles passed with the change before the
controller stopped the full gate (the whole gate is Task 7's closeout check).
