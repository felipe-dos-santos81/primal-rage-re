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

## §H3 The game-port button readers and the `0x1B890` row

§H2.6's `1 named gap` was `host_1b890`: a one-case spec whose `in al,dx` at `0x1B899` the
emulator could not model. Track H's `E.Machine.in` answers it, so the gap became a row. This
section ports the three game-port button readers, gives the host the byte they read, extends
the differential harness's machine model with the port's side of the same `in`, and records
the bit maps and the plan-vs-raw correction.

### §H3.1 The three functions and the active-low button byte

The sampler `0x1BBAC`'s device 2/4/6 arms call these (disassembly of the fixed-up image; the
Ghidra listing agrees):

```
1B890  push ebx / push edx
1B892  mov edx,0x201            ; the PC game port
1B897  sub eax,eax
1B899  in al,dx                 ; the button byte
1B89A  mov ah,al                ; kept while the bits are tested
1B89C  xor edx,edx / and al,0x10 / mov dl,al / xor bl,bl
1B8A4  test edx,edx / jne 0x1b8aa / mov bl,1     ; bit 4 clear -> 1
1B8AA  mov al,ah / ... and al,0x20 ... / or bl,2 ; bit 5 clear -> 2
1B8B9  mov al,ah / ... and al,0x40 ... / or bl,4 ; bit 6 clear -> 4
1B8C8  mov al,ah / and al,0x80 / and eax,0xff / jne / or bl,8  ; bit 7 clear -> 8
1B8D6  mov al,bl / pop edx / pop ebx / ret
```

`0x1B8DC` (P1) is the same shape for bits 4/5: `mov edx,0x201` `0x1B8DE`, `in al,dx`
`0x1B8E5`, `mov dl,al` `0x1B8E8`, bit 4 clear -> `mov bl,1` `0x1B8F3`, bit 5 clear ->
`or bl,4` `0x1B900`, `mov al,bl` `0x1B903`. `0x1B908` (P2) does bits 6/7: `in al,dx`
`0x1B911`, `mov dl,al` `0x1B914`, bit 6 clear -> `mov bl,1` `0x1B91F`, bit 7 clear ->
`or bl,4` `0x1B92C`, `mov al,bl` `0x1B92F`.

The maps:

| function | game-port bits | game-bit values | raw return |
|---|---|---|---|
| `0x1B890` `input_port_buttons_both` | 4, 5, 6, 7 | 1, 2, 4, 8 | AL (EAX upper bits cleared by `and eax,0xff` at `0x1B8CC`) |
| `0x1B8DC` `input_port_buttons_p1` | 4, 5 | 1, 4 | AL (`and eax,0xff` at `0x1B8EC`/`0x1B8F9`) |
| `0x1B908` `input_port_buttons_p2` | 6, 7 | 1, 4 | AL (`and eax,0xff` at `0x1B918`/`0x1B925`) |

**A correction to the plan's table.** The plan's Task 3 table reads "game-port byte: bits
4..7 set -> `1`/`2`/`4`/`8`". The raw tests each bit with `test`/`and` and sets the game bit
when the bit is **clear** (`0x1B8A4 jne` skips `mov bl,1`; `0x1B8D1 jne` skips `or bl,8`),
i.e. the byte's bits are ACTIVE LOW, the PC game port's own electrical convention (a pressed
button grounds the line). The design's line 52/82 wording -- "game-port bits 4..7 -> 1,2,4,8"
-- is the bit map, not a polarity. The raw wins, and the row's cases are the raw's: a byte
with bit 4 clear returns 1, and `0xFF` (all four released) returns 0. Every case below is
derived from the bytes above.

### §H3.2 The `in` model: the original's machine, the port's host seam

The original's side is track H's `E.Machine.ins=((0x201, (byte,)),)`: `in al,dx` returns the
case's byte. The port has no `in`, so its three readers call `host_port_buttons()`, and the
differential harness now carries the same case byte to that host answer:

- `tools/diff_verify.py`'s `h0_machine_text` emits `ins` then `in 0x201 0xNN` for a case
  whose machine's `ins` lists the game port (`H0_GAME_PORT = 0x201`). It is the **only** port
  the port side answers; a case's other `in`s (the `0x3DA` retrace reads of the d6a rows)
  stay the emulator's alone, as before. A case with two different 0x201 bytes is refused
  (`ValueError`): the host holds one game-port byte, and the raw reads the same physical port
  once per reader.
- `port/tests/diff_runner.c` parses `ins`/`in <port> <byte>` (`case_t.ins/game_port_set/
  game_port`), refuses any port other than 0x201, a byte above 0xFF, a second `in` line, or
  an `in` without `ins`; `h0_start` arms `host_set_port_buttons_override(byte, 1)` before the
  binding and `h0_end` clears it, so no case's byte leaks into the next.
- The port's side of each reader is the binding: `input_port_buttons_both()` ->
  `host_port_buttons()` -> the seam's byte. Nothing else about the harness's `in` handling
  changed; `compare()` still does not compare `in`s (the port has none to compare).

### §H3.3 The rows (`tools/diff_verify.py`) and their mutants

The three rows replace `host_1b890` (`tools/diff_verify.py`, the E3 group; the new rows sit
with the §H2 rows in the D1I group). Fixture `h3_port_case(cid, byte)` = a case with no
registers, no pokes, and `machine=E.Machine(ins=((0x201, (byte,)),))`, `eax_mask=0xFF`:

| row | case (byte -> AL) |
|---|---|
| `input_port_buttons_both` | none `0xFF`->0, b1 `0xEF`->1, b2 `0xDF`->2, b3 `0xBF`->4, b4 `0x7F`->8, b1b4 `0x6F`->9, all `0x0F`->15, low `0x10`->14 |
| `input_port_buttons_p1` | none `0xFF`->0, b1 `0xEF`->1, b2 `0xDF`->4, both `0xCF`->5, p2 `0x3F`->0 |
| `input_port_buttons_p2` | none `0xFF`->0, b1 `0xBF`->1, b2 `0x7F`->4, both `0x3F`->5, p1 `0xCF`->0 |

`low` (`0x10`: bits 0..3 clear, bit 4 high) is the case that pins that the low nibble is never
read (it returns `2|4|8`, not `0x1F`); the P1/P2 cross cases (`0x3F`/`0xCF`) pin that each
reader ignores the other player's two bits. `all` and `b1b4` pin the OR of the arms.

The 9 blocks of `0x1B890` are all hit: the entry, the two arms of each of the four bit tests
(`0x1B8A8`/`0x1B8AA`, `0x1B8B6`/`0x1B8B9`, `0x1B8C5`/`0x1B8C8`, `0x1B8D3`/`0x1B8D6`). The
`0x1B8C8` block is both the bit-6 test's not-taken fall-through and the bit-7 test's entry;
the 9 are covered by b1..b4 (each arm taken once) plus `none` (every arm not taken).

Mutants (`port/tests/diff_runner.c`, `h3_both_core`/`h3_p1_core`/`h3_p2_core` with the
`H3_MUTANTS_FOR` wrappers; bindings after `input_f2@inv`): `@mask` tests the wrong input bit
(0x08 for 0x10, 0x40 for 0x10, 0x10 for 0x40), `@bits` swaps the output order (1/2/4/8 ->
8/4/2/1; 4 <-> 2; 4 <-> 8), `@inv` inverts the polarity (`al ^ 0x0F` / `al ^ 0x05`), `@all`
returns every bit (`0x0F` / `0x05`). All twelve are detected. The measured case sets (the
harness suite's `H3_CASES`, §H3.5):

| mutant | cases that catch it |
|---|---|
| `both@mask` | all, b1, b1b4, low |
| `both@bits` | b1, b2, b3, b4, low |
| `both@inv` | all, b1, b1b4, b2, b3, b4, low, none |
| `both@all` | b1, b1b4, b2, b3, b4, low, none |
| `p1@mask` | b1, both, p2 |
| `p1@bits` | b2, both |
| `p1@inv` | b1, b2, both, none, p2 |
| `p1@all` | b1, b2, none, p2 |
| `p2@mask` | b1, both, p1 |
| `p2@bits` | b2, both |
| `p2@inv` | b1, b2, both, none, p1 |
| `p2@all` | b1, b2, none, p1 |

RED before the port side existed: `python3 tools/diff_verify.py --function
input_port_buttons_both` on the pre-batch `diffrun` printed `diff-verify: error: diffrun
failed (1): diffrun: cannot parse 'ins'` -- neither the case line nor the binding existed.
GREEN: the three rows by name (`PR_DIFF_JOBS=1 ... --self-check`) print `VERIFIED` at
9/9 (`0x1B890`), 5/5 (`0x1B8DC`) and 5/5 (`0x1B908`) blocks with 4/4 mutants detected each.

### §H3.4 The host byte, the reader and the unit test

`port/src/host.c`: `host_port_buttons()` is the PC game port's button byte (0x201's bits
4..7), the host's answer to the three ported readers. A game port's inputs are active low, so
the raw's readers set a game bit when the byte's bit is CLEAR. The pad model (Task 5,
`host_pad_fill`) writes the pad's byte (`g_port_buttons`); **until then the answer is 0**,
the brief's placeholder. That 0 is "all four buttons low" as a game-port byte, and it is
sound only because a device word that selects a game-port arm is cleared when no pad answers
(`0x4FBBB`; Task 6), and because at this commit nothing in the windowed run calls the
readers yet (Task 4 wires `input_sampler` into `game_loop`). The seam
`host_set_port_buttons_override(u8 buttons, int on)` keeps its own byte
(`g_port_buttons_override`) apart from the pad's, so pinning one never loses the other:
`host_port_buttons()` returns the seam's byte while it is armed, else `g_port_buttons` (0
with no pad). Off by default.

`port/src/platform/input.c`: the three readers call `host_port_buttons()` **once** and map
the bits, mirroring the raw's single `in al,dx` + `mov ah,al`:
`0x1B890` tests 0x10/0x20/0x40/0x80 -> 1/2/4/8; `0x1B8DC` tests 0x10/0x20 -> 1/4;
`0x1B908` tests 0x40/0x80 -> 1/4. Each opens with `PR_SEAM_RET0` at its address so the
sampler's row (Task 4) can record the call. `port/src/platform/input.h` declares all four
with the bit maps and the active-low rule.

`port/tests/test_platform.c`, `test_input_port_buttons` (registered once in `test.h` after
`test_input_keyboard_readers`): it asserts, with the seam armed:

- each game-port bit alone of `0x1B890` in the raw's order (4 assertions), and that
  `host_port_buttons()` returns the seeded byte;
- released `0xFF` -> 0 for all three; all-low `0x00` -> `0x0F`, `0x05`, `0x05`;
- P1's bits 4/5 -> 1/4, both -> 5, and `0x3F` (only 6/7 low) -> 0;
- P2's bits 6/7 -> 1/4, both -> 5, and `0xCF` (only 4/5 low) -> 0;
- the low nibble is not read (`0xF0` -> 0; `0x10` -> 14);
- the seam disarmed with the byte 0 (no pad): `host_port_buttons()` == 0 and the readers
  return `0x0F`/`0x05`/`0x05`.

Each assertion was seen to fail under at least one mutation, applied alone and reverted (the
suite's only FAILs were these; the two `host_port_buttons()` checks `3379`/`3424` are the
seam's wiring, not a bit map, so the last row is a wiring mutation):

| mutation | failing checks |
|---|---|
| `0x1B890` bit-4 test `0x10` -> `0x20` | `test_platform.c:3380: 0 != 1`, `3380: 3 != 2`, `3419: 15 != 14` |
| `0x1B8DC` bit-5 output `4` -> `2` | `3390: 3 != 5`, `3398: 2 != 4`, `3400: 3 != 5`, `3426: 3 != 5` |
| `0x1B908` bit-6 test `0x40` -> `0x10` | `3406: 0 != 1`, `3410: 4 != 5`, `3412: 1 != 0` |
| `0x1B890` bit-7 polarity `== 0u` -> `!= 0u` | `3380` x4, `3385: 8 != 0`, `3389: 7 != 15`, `3417: 8 != 0`, `3419: 6 != 14`, `3425: 7 != 15` |
| `0x1B8DC` bit-4 polarity `== 0u` -> `!= 0u` | `3386: 1 != 0`, `3390: 4 != 5`, `3396: 0 != 1`, `3398: 5 != 4`, `3400: 4 != 5`, `3402: 1 != 0`, `3426: 4 != 5` |
| `0x1B8DC` bit-4 mask `0x10` -> `0x40` | `3396: 0 != 1`, `3400: 4 != 5`, `3402: 1 != 0` |
| `0x1B908` bit-7 polarity `== 0u` -> `!= 0u` | `3387: 4 != 0`, `3391: 1 != 5`, `3406: 5 != 1`, `3408: 0 != 4`, `3410: 1 != 5`, `3412: 4 != 0`, `3427: 1 != 5` |
| `host_port_buttons()` byte `^ 0x40` (the seam's wiring) | 21 checks: `3379` x4 (`175 != 239`, `159 != 223`, `255 != 191`, `63 != 127`), `3380` x4, `3385`, `3387`, `3389`, `3391`, `3406`, `3408`, `3410`, `3412`, `3417`, `3419`, `3424: 64 != 0`, `3425`, `3427` |

Coverage after these: every check line `3379..3427` appears in at least one row -- `3379`
and `3424` only in the wiring row, `3386`/`3387`/`3391`/`3396`/`3402`/`3408`/`3427` in the
three polarity/mask rows added for this review (they were green under the first four), and
the rest in the earlier rows. The three new bit-map mutations are what make a P1 or P2
polarity inversion fail the unit test, not only the differential row.

Named limit: the test cannot exercise the SDL pad (Task 5) and does not; it pins the byte ->
bit maps for the byte the host answers, which is the row's claim too.

### §H3.5 Counters and the harness suite's name tables

The differential step (`--self-check`) now reads **1016/1016 functions VERIFIED;
14938/14938 mutants detected; 0 named gaps; 774/832 rows with callees closed (184 have
none); 768/832 closed on full rows; 8 ported functions with no row.** Against §H2.6's 1013 /
14926 / 1 / 181: +3 rows and +12 mutants from this batch, the named gap count 1 -> 0, and
the callee-less count 181 -> 184; the closed pair is unchanged. `tools/port_progress.py`
reads `796 1203 66`, `756 756 100` (README stays at 65% until Task 7's closeout).

**A Task 2 omission repaired here.** Task 2's six rows were never added to
`tools/tests/test_diff_verify.py`'s expected-name tables, so at this task's start the harness
suite (`make diff-verify`'s unittest, AGENTS.md) failed with six unknown functions; the
controller's Task 2 review did not catch it. This batch adds `H2_MASKS`/`H2_KINDS`/
`H2_CASES` for those six rows and `H3_MASKS`/`H3_KINDS`/`H3_CASES` for the three here (all
values measured), removes `host_1b890` from the name list and the EAX-mask dict, turns its
named-gap test into a 9/9 full-row assertion, pins the new counter string, and adds two tests
for the `ins`/`in 0x201` case line (the machine text and the runner's answer/refusals).
`python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify`: 216 tests, OK.
`PR_ORACLE_REQUIRED=1 ./build/run_tests` (repo root): `all checks passed`.

## §H4 The axis readers, the ISR sampler and the `game_loop` call

Task 4 ports the two axis readers and the sampler `0x1BBAC` that consumes all eleven readers,
gives the host the no-pad axis state and the presence answer, and replaces `game_loop`'s direct
`+0x2D8`/`+0x2D9` write with the sampler. Sources: capstone over the fixed-up image (image offset
= address − `0x10000`) for every instruction below; the Ghidra listing
(`port/decomp/prage.c:7484-7603`, `36561-36625`) agrees. The rows were run by spec name
(`--function input_axes_a`, ...): the plan's `--function 0x4FF8F` form is not the CLI's, as §H2
already records.

### §H4.1 The three functions

The axis readers are argument-free leaves returning through AL:

```
4FF8F  push ecx / xor eax,eax
4FF92  mov cx,[0xe1c1e] / sub cx,[0xe1c22] / js 0x4ffac / cmp cx,0x1e / jl 0x4ffb4 / or al,0x10
4FFAC  cmp cx,0xffffffe2 (-0x1e) / jg 0x4ffb4 / or al,0x20
4FFB4  mov cx,[0xe1c20] / sub cx,[0xe1c24] / ... / or al,0x40 (>= +0x1e) / or al,0x80 (<= -0x1e)
4FFD6  pop ecx / ret
```

and `0x4FFD8` is the same shape on `0xE1C26/0xE1C28` against `0xE1C2A/0xE1C2C`. The subtraction
is **16-bit** (`sub cx,`), so a value below its center wraps; the compares are signed, so the
thresholds are `>= +0x1E` and `<= -0x1E` and `+0x1D/-0x1D` stay in the dead band. X's bits are
`0x10/0x20` and Y's `0x40/0x80`, independent of each other (8-way). The port's versions cast the
difference to `s16` and mirror the two compares (`port/src/platform/input.c`).

`0x1BBAC` (the ISR's sampler, called from `0x1BDF4` at `0x1BE1C`) is the whole of
`port/decomp/prage.c:7486-7603`:

- **The device masks.** P1's word `+0x2D4` (`0x1BBB6`) is a port when it is 2/4/6; P2's `+0x2D6`
  (`0x1BBDC`) likewise. A P2 device 2 arm reads the same both-player game-port bits as P1's, so
  when P2 selects 2 the raw's presence code treats P1 as a port too (`0x1BC35`): it sets its own
  flag, not the `+0x2D4` word (which stays 0 unless the config says otherwise). That is why
  `input_sampler`'s `p1`/`p2` are not the two independent "is a port" tests; the raw's `mov ah,1`/
  `mov al,1` pair is kept.
- **The presence block** (`0x1BC4F..0x1BCE6`): both ports call `0x4FDF5` (`0x1BC4F`); a 0 answer
  probes `0x4FBBB(3)` (`0x1BC61`, clear `+0x2D4` on 0, `0x1BC6F`) and `0x4FBBB(0xC)` (`0x1BC7D`,
  clear `+0x2D6` on 0, `0x1BCE1`). The two masks are the game port's four axis one-shot bits
  (port 0x201 bits 0..3): 3 is P1's axes 0/1 and 0xC P2's axes 2/3 (`0x4FBBB` waits for the mask to
  clear, writes `out 0x201,0` and waits again). P1 only calls `0x4FC05` (`0x1BC99`); a 0 answer
  clears `+0x2D4` when `+0x2D6 == 0` (`0x1BCBF`) else clears `+0x2D6` (`0x1BCB4`). P2 only calls
  `0x4FCFD` (`0x1BCD8`) and a 0 answer clears `+0x2D6` (`0x1BCE6`). Neither skips it. The block
  re-reads `+0x101514` before each clear (`0x1BC6A`/`0x1BCA2`/`0x1BCE1`), and so does the port.
- **The arms** (the switch tables `0x1BB74` for P1 and `0x1BB90` for P2, seven entries each; the
  entries for 1, 3 and 5 are the "past the write" target `0x1BD61`/`0x1BDEE`, so a non-0/2/4/6
  device leaves that player's byte untouched, as do values above 6 via the `ja` guards):

  | dev | P1 (+0x2D4) call order | P2 (+0x2D6) call order |
  |---|---|---|
  | 0 | `0x1B610` dirs, `0x1B730` buttons, `0x1B850` F1 | `0x1B6A0`, `0x1B7C0`, `0x1B870` F2 |
  | 2 | `0x4FF8F` axes A, `0x1B890` both, `0x1B850` F1 | `0x4FF8F` axes A, `0x1B890`, `0x1B870` F2 |
  | 4 | `0x4FF8F`, `0x1B8DC` P1, `0x1B850` | `0x4FFD8` axes B, `0x1B908` P2, `0x1B870` |
  | 6 | `0x4FF8F`, `0x1B730` keyboard buttons, `0x1B850` | `0x4FFD8`, `0x1B7C0`, `0x1B870` |

  The write ORs the three readers with F1/F2 (`0x1BD53` for `+0x2D8`, `0x1BDE0` for `+0x2D9`).
  **P2's device 2 arm reads axes A, not B** (`0x1BD8B` calls `0x4FF8F`; `0x1BDA4`/`0x1BDC9` call
  `0x4FFD8`): the design's table agrees, and the row's `b2` case pins it (the `@axesb` mutant is
  caught on three cases).

Two raw-vs-listing notes. The Ghidra listing shows `case 0: FUN_0001b610(); break;` (no write):
that is a decompiler artifact — the jump-table entry for 0 is `0x1BD0E`, whose `jmp 0x1BD41`
re-enters the case 6 block at `mov dl,al` and reaches the `+0x2D8` store through
`0x1BD48..0x1BD53`, so device 0 does write the byte. And the P2 switch's `jmp
cs:[edx*4+0x1bb90]` (`0x1BD7C`) is added to `E.RESOLVED_JUMPS` (`tools/diff_emu.py`) because the
`xor edx,edx; mov dx,ax` pair hides the guard from `switch_cases` (the P1 table is found
heuristically). Without the resolution the row's static scan is **42 leaders with one indirect
jump**; with it, **48 leaders, no indirect jump, not truncated**, and all 48 are hit.

A PORT guard: the sampler returns before touching `mem[]` when `DS_00101514` is 0 or outside
`MEM_SIZE - 0x2EE` — the drivers' diffrun cases and any headless run before `0x1BEC4` has built the
block. The raw's ISR is installed only after that block exists (`0x1BF4C/0x1BF6D`), and the port's
`game_loop` folds the ISR in, so without the guard the game_loop row would read `mem[0x254]`
through a zero base. The sampler row's cases all set the base, so the guard is outside the raw's
behaviour it compares; the game_loop row exercises it.

### §H4.2 The rows (`tools/diff_verify.py`) and their mutants

The axis rows (`input_axes_a` `0x4FF8F`, `input_axes_b` `0x4FFD8`) are added to the D1I group,
`eax_mask=0xFF` (AL only; the sampler reads AL). Fixture `h4_axis_case(cid, ax, ay, cx, cy,
player_a)`: the player's two axis words and their two centers (A `0xE1C1E/0xE1C20` against
`0xE1C22/0xE1C24`, B `+8` on both). The cases use **distinct centers per axis** (`cx=0x100,
cy=0x200`) so a wrong-center mutant is caught:

| case | words | expected |
|---|---|---|
| a0 | rest | 0 |
| a1/a2 | X +0x1E / -0x1E | 0x10 / 0x20 |
| a3/a4 | X +0x1D / -0x1D | 0 (dead band) |
| a5/a6 | Y +0x1E / -0x1E | 0x40 / 0x80 |
| a7/a8 | Y +0x1D / -0x1D | 0 |
| a9/a10/a11 | both extremes (X-,Y+ / X+,Y+ / X-,Y-) | 0x90 / 0x50 / 0xA0 |
| a12 | X word 0x0005 against center 0xFFF0 | 0 (+0x15; a 32-bit subtract would read +0x80015) |
| a13 | X word 0x8000 against 0 | 0x20 (-0x8000) |

`input_axes_b` has the same fourteen cases on its words. Six mutants each: `@thr` (0x1D threshold),
`@xbit`/`@ybit` (the sign bits swapped), `@or` (Y assigned instead of OR'd), `@wide` (the 16-bit
truncation dropped) and `@ctr` (X compared against the Y center). All 12 are detected; measured
case sets (`H4_CASES`): `@thr` the four ±0x1D cases per axis (a3/a4/a7/a8, b3/b4/b7/b8), `@wide`
only a12/a13 (b12/b13), `@ctr` ten cases, the rest the sign cases.

The sampler row (`input_sampler` `0x1BBAC`, `eax_mask=0`, void) has **11 `real` callees** (the six
keyboard readers, the three game-port readers and the two axis readers; each has its own row) and
**4 stubbed host-owned presence routines** (`0x4FDF5`/`0x4FBBB`/`0x4FC05`/`0x4FCFD`), whose
per-case EAX the `stub_eax` map chooses. Fixture `h4_sampler_case(cid, dev1, dev2, pressed, f1, f2,
a, b, port, poll, probe)`: the key-config block `H4_BIOS = 0x109000` with `[0x101514]` pointing at
it, the 16 H2 slot scans, the table (all break, `pressed` make, F1/F2 at `+0x28F/+0x290`), the
device words `+0x2D4/+0x2D6`, the four axis words (centers 0), the sentinels `+0x2D8=0xA5`,
`+0x2D9=0x5A`, and the game-port byte as the machine's `in 0x201 NN` (the §H3 channel; the byte is
listed twice because a 2/2 or 4/4 case reads the port once per player's arm). Sixteen cases:

| case | devices | what it pins |
|---|---|---|
| k1/k2 | 0/0 | the keyboard arms, F1/F2 on and off (the default path) |
| b2/b2b | 2/2 | axes A for **both** players, game-port bits 4..7 -> 1/2/4/8 |
| b4 | 4/4 | P1 axes A + bits 4/5, P2 axes B + bits 6/7 |
| b6 | 6/6 | axes A/B with the keyboard button scans (no game port) |
| o1/o2 | 4/1, 0/4 | the single-player port arm beside the other player's keyboard arm |
| o3 | 0/2 | P2 device 2 makes the presence code treat P1 as a port (no word write), so the P1-only poll runs; P1's arm stays the keyboard |
| c1 | 2/2 poll 0 probe 0 | both device words cleared (0x1BC6F + 0x1BCE1) |
| c2 | 4/0 poll 0 | P1-only, `+0x2D6 == 0` -> clear P1 (0x1BCBF) |
| c3 | 4/1 poll 0 | P1-only, `+0x2D6 != 0` -> clear P2 (0x1BCB4) |
| c4 | 0/4 poll 0 | P2-only -> clear P2 (0x1BCE6) |
| x1/x2/x3 | 1/1, 7/7, 1/7 | no presence call and no write (the sentinels) |

All **48/48 blocks** are hit and the row has no pokes, no unhit block and no terminal difference,
but it is **open** on the four presence stubs: the closure rule (record §J1) excuses only
*allow-mode* callees outside the portable set, and a recorded stub keeps the row open. That is the
honest statement — the presence block's calls and answers are compared, the routines themselves are
host-owned (record §49-V) and Task 6 models them. Six mutants: `@axesb` (P2 device 2 reads axes B),
`@p1f1`/`@p2f2` (F2 in P1's byte / F1 in P2's), `@arm6` (P1 device 6 reads the game-port buttons),
`@clear` (the P1-only arm always clears P1) and `@nonport` (a device that is not 0/2/4/6 writes the
keyboard byte). All six are detected, measured kinds and case sets in `H4_KINDS`/`H4_CASES`
(`@arm6` and `@clear` change which reader an arm calls as well as the byte).

RED before the port side existed: the three rows against `db8862f6`'s predecessor
`build/diffrun` (the Task 3 binding table) print `port: unknown binding input_axes_a` (and
`input_axes_b`/`input_sampler`) on every case, a MISMATCH. GREEN by name (`--self-check`):
`input_axes_a` 11/11 VERIFIED 6/6 mutants, `input_axes_b` 11/11 VERIFIED 6/6, `input_sampler`
48/48 VERIFIED 6/6.

### §H4.3 The host: the no-pad axes, the presence answer and the `game_loop` call

`port/src/host.c`:

- `host_input_fill_axes()` writes each of the four normalized axis words to its calibrated center
  (`0xE1C1E` <- `0xE1C22`, `0xE1C20` <- `0xE1C24`, `0xE1C26` <- `0xE1C2A`, `0xE1C28` <- `0xE1C2C`).
  With no pad every axis is at rest, so the ported readers read their dead band; Task 5's
  `host_pad_fill` writes the pad's displacement over them. In the captured runs the centers are 0
  (the all-zero CMOS image), so this is a 0 -> 0 write and cannot move a pin. The unit test seeds
  non-zero centers and asserts the copy (so a zeroing fill fails).
- The four presence answers `host_port_poll_both()` (`0x4FDF5`), `host_port_poll_p1()` (`0x4FC05`),
  `host_port_poll_p2()` (`0x4FCFD`) and `host_port_probe(u32)` (`0x4FBBB`) are the port's stand-ins
  for the host-owned routines. Each opens with its differential seam (`PR_SEAM_RET0`/`PR_SEAM_RET`)
  so the sampler's row records the call and takes the case's answer, and each returns
  `host_port_present()` otherwise. **Task 4 answers 0 (no port)** — the no-pad state — so a
  device 2/4/6 selection is cleared on the next sample exactly as the original does when nothing
  answers; Task 6 models the pad's answer and the boot probe.
  `host_set_port_present_override(int, int)` is the unit test's seam (like
  `host_set_port_buttons_override`); off by default. The production answer is asserted by
  `test_input_sampler_arms`'s last block (override off, a device-2 sample clears the word and the
  keyboard arms take over).
- The seam functions carry no column-0 `/* 0xADDR` header and no `fn_register`, so the port-code
  guard does not read them as ported: the four addresses stay host-owned in
  `tools/port_classification.txt` and the sampler row's open state is the stub, not a ported callee.

`port/src/game/flow.c` (`game_loop`'s body, the old lines 7117-7121) now reads

```
host_input_fill_keyboard();
host_input_fill_axes();
input_sampler();                 /* 0x1BBAC (0x1BE1C) */
```

where it used to write `k[0x2d8] = host_key_bits() >> 8` and `k[0x2d9] = host_key_bits()`. The
`0x4F644` update is untouched (`game_frame` calls `input_state_update` at 0x24C6E).

**The default device 0/0 path is byte-identical.** For every one of the 16 bits, the fill writes
the slot's make/break byte from the same kb word the direct write copied, and the sampler's device-0
arms read the same four positions per player back (the slot->bit map is `k_cfg_bit` in reverse);
F1/F2 are bits `0x0100`/`0x0001` of the word and the readers' `0x1B850/0x1B870` OR the same two
into `+0x2D8`/`+0x2D9` bit 0. The proof, all run this task: `PR_ORACLE_REQUIRED=1
./build/run_tests` all checks passed; `make gp-oracle` first unexplained 7559 and trace difference
8320 (pins unchanged), `make gp-twop-oracle` 612/1506/1506 moves and the two-human check
unchanged, `make gp-keys-oracle` 11/11 events, first unexplained 1136 and trace 2164 unchanged;
`make frontend-oracle` (1325 frames in window, 2 named unexplained), `make demo-fight-oracle`
(fight window empty, N 1886) and `make attract2-oracle` (N 3617) all exit 0 with their pins as
they were. The default path also leaves the axis words and the key-state table equal to what they
held (the captures' centers and table entries are 0/absent), so no capture byte moves.

### §H4.4 The unit test and its mutation proofs

`port/tests/test_platform.c`, three tests registered once in `test.h` after
`test_input_port_buttons`:

- `test_input_sampler_device0`: eight kb words (`0x0000`, `0xFFFF`, `0x1234`, `0x0100`, `0x0001`,
  `0x8000`, `0x0080`, `0x8040`) through `host_set_key_bits_override` + `host_input_fill_keyboard` +
  `input_sampler`, asserting `+0x2D8 == word >> 8` and `+0x2D9 == word` (the old direct write's
  bytes; one bit per byte per word, all 16 positions covered); F1/F2 alone with their slots
  released set bit 0 and releasing the fixed entries clears it; a device-0 sample ignores non-zero
  axis words; a non-port 3/3 device leaves the words and the byte sentinels alone.
- `test_input_sampler_arms`: with `host_set_port_present_override(1, 1)`, devices 2/2 (axes A for
  both, game-port `0xFF/0x0F`), 4/4 (`0x3F`/`0xCF` for bits 4/5 vs 6/7), 6/6 (axis A/B + keyboard
  buttons), 0/4, 4/0, 0/2, the non-port 1/1 and 7/7 (sentinels), the no-pad clears
  (`host_set_port_present_override(0, 1)`: 2/2 clears both, 4/0 clears P1, 4/1 clears P2, 0/4
  clears P2, 0/2 clears P2), and the production answer (the override off, a device 2/0 sample
  clears P1 and the keyboard arms take over).
- `test_input_axes_dead_band`: `host_input_fill_axes()` over non-zero centers (each word becomes its
  own center, so a zeroing fill fails), then each axis of both readers at ±0x1D (no bit) and ±0x1E
  (the bit), with distinct centers per axis (`0x2000/0x2400` for A, `0x1C00/0x2800` for B), both
  axes together, and the 16-bit wrap (`0x0005` vs `0xFFF0` -> 0; `0x8000` vs 0 -> 0x20).

Each assertion was seen to fail under a mutation, applied alone, rebuilt and reverted (the suite's
only FAILs were these):

| mutation | failing checks |
|---|---|
| device-0 P1 arm drops the F1 OR | `3495: 0 != 1` (F1-only p1) |
| device-0 P2 arm reads F1 for F2 | `3496: 1 != 0` x5 |
| device-0 P1 arm ORs `input_axes_a()` | `3495: 144 != 0`, `3495: 96 != 32`, `3495: 16 != 0`, `3495: 17 != 1` x4 |
| P2 device 2 reads axes B | `3496: 32 != 16`, `3496: 33 != 17`, `3496: 47 != 31` |
| P1 device 6 reads the game-port buttons | `3495: 69 != 79` |
| P1-only clear ignores `+0x2D6` | `3655: 0 != 4`, `3656: 1 != 0`, `3662: 2 != 0` (the 0/2 case), `3496: 16 != 0` |
| a non-port device writes the keyboard byte | `3495: 0 != 195` (the 3/3 sentinel), `3495: 0 != 165` x2 |
| `host_input_fill_axes` writes 0 for P1 X | `3699: 0 != 4660`, `3703: 32 != 0` |
| `host_input_fill_axes` P1 Y reads the P2 Y center | `3700: 17767 != 9029`, `3703: 64 != 0` |
| the production presence answer becomes 1 | `3671: 2 != 0` (the word not cleared), `3495: 17 != 1` |
| axes A threshold 0x1E -> 0x1D | `3711: 16 != 0` |
| axes A Y+ bit 0x40 -> 0x80 | `3495: 143 != 79`, `3495: 128 != 64`, `3722: 128 != 64` |
| axes B X center -> its Y center | `3704` (the fill's B dead-band check) and the eight B checks `3742..3759` |
| axes A X difference 32-bit | `3735: 32 != 0` (the wrap case) |

Line numbers are this commit's `test_platform.c`; `3495`/`3496` are `sampler_bytes`' p1/p2
assertions, so a mutation names the call site's expectation, not a distinct line per case.

### §H4.5 Counters

The differential step (`--self-check`) now reads **1019/1019 functions VERIFIED; 14956/14956
mutants detected; 0 named gaps; 774/833 rows with callees closed (186 have none); 768/833 closed on
full rows; 8 ported functions with no row.** Against §H3.5's 1016 / 14938 / 0 / 774/832 / 184: +3
functions (the two axis readers and the sampler), +18 mutants (12 axis + 6 sampler), +2 callee-less
rows (the axis readers) and +1 row with callees (the sampler), which is open on its four presence
stubs; the closed and full counts are unchanged. `tools/port_progress.py` reads `799 1203 66`,
`759 759 100` (README stays at 65% until Task 7's closeout). `python3 -m unittest
tools.tests.test_diff_verify`: 133 tests, OK; `tools.tests.test_diff_emu`: 83 tests, OK.
`PR_ORACLE_REQUIRED=1 ./build/run_tests`: `all checks passed`.

## §H5 The SDL gamepad model

Task 5 gives the host the SDL gamepad the raw never had (the platform's joystick is the
host-owned ISR sampler `0x1BBAC`; the game logic's `0x2D2F0` is `xor eax,eax; ret`). The raw-side
maps are §H3/§H4; this section records the host mapping the design left to the builder: the SDL
axes and buttons, the device-word gating, the axis-scale policy, and the tests. The plan's file
list omits `host.h` and `test.h`; both changed (the declarations and the one-line registrations).

### §H5.1 The pad state, the fill and the device-arm table

`port/src/host.c`:

- `host_pad_open()` initialises `SDL_INIT_GAMEPAD` and opens the first pad from
  `SDL_GetGamepads()`/`SDL_OpenGamepad()`; a host with no pad, no driver or no display leaves the
  pad absent and never aborts. `host_init` calls it after the window and surface exist;
  `host_shutdown` calls `host_pad_close()` before `SDL_Quit()` (`SDL_CloseGamepad`, then
  `SDL_QuitSubSystem(SDL_INIT_GAMEPAD)` only when the subsystem was ours).
- `host_pad_poll()` samples the left/right sticks and the four face buttons into
  `g_pad_axes[4]`/`g_pad_buttons[4]` (`SDL_UpdateGamepads` + `SDL_GetGamepadAxis`/`Button`). The
  seam short-circuits SDL; with no open pad it is a no-op, so every headless path is untouched.
- `host_pad_fill()` writes the two kinds of game input the arms read: the four normalized axis
  words (`0xE1C1E/0xE1C20/0xE1C26/0xE1C28` = their calibrated centers + the pad's displacement)
  and the active-low game-port byte (`g_port_buttons`). It is gated by the device words: it writes
  only the axis words the selected arms actually read, and leaves a keyboard player's words at the
  centers `host_input_fill_axes()` wrote. With no pad it writes no axes and the byte stays Task 4's
  0 placeholder.
- **The hook.** `input_sampler()` (Task 4's per-frame input entry, called from `game_loop`) calls
  `host_pad_poll(); host_pad_fill();` after its block guard. The sampler is the once-per-iteration
  input point, so `flow.c` needs no change (the brief's "input.c only if the host fill needs a
  hook"). With no pad both calls are no-ops: the sampler's differential row and the default
  device-0 path compare exactly the raw's bytes (§H5.4).

The fill's writes, by the two device words (the raw's arm readers are the §H4 table):

| dev1 (`+0x2D4`) | dev2 (`+0x2D6`) | axis words the fill writes | the sampler's arms read |
|---|---|---|---|
| 0 | 0 | none (centers) | keyboard dirs/buttons + F1/F2 (device 0) |
| 2, 4, 6 | 0, 1, 3, 5, 7+ | A: `0xE1C1E/20` = center + left stick | P1 device 2/4/6 arm (`0x4FF8F`) |
| any | 2 | A: `0xE1C1E/20` (the shared stick) | P2's device-2 arm also calls `0x4FF8F`, not `0x4FFD8` (`0x1BD8B`) |
| any | 4, 6 | B: `0xE1C26/28` = center + right stick | P2's device 4/6 arm (`0x4FFD8`) |

The port byte is composed from all four game buttons whenever the pad is present (bits 4..7
active low); device 0 and device 6 never read it (their arms read the keyboard), so its value is
irrelevant there, and a device 2/4 arm reads it exactly as `0x1B890`/`0x1B8DC`/`0x1B908` do.

**Two corrections the mutation pass forced (raw wins over the first draft).** The first cut wrote
axes A only when P1's word selected a joystick, and mirrored A into P2's B words on device 2:

1. P2's device-2 arm reads **axes A**, the same physical stick as P1's (`0x1BD8B` calls
   `0x4FF8F`), so axes A must be written for a `0/2` selection too — otherwise P2's both-player arm
   reads the centers. The fill's A condition is `dev1 in 2/4/6 || dev2 == 2`.
2. Device 2's arm never reads P2's B words, so writing them there was dead state that also masked
   the A-vs-B distinction in the sampler (the first mutated `@axesb` run passed because the fill had
   copied A into B). The fill writes B only for `dev2 in 4/6`.

### §H5.2 The SDL mapping and the axis-scale policy

- **Axes:** the left stick X/Y is axes A (`0xE1C1E`/`0xE1C20`), the right stick X/Y axes B
  (`0xE1C26`/`0xE1C28`), matching the raw's P1/P2 game-port axes.
- **Buttons:** the face buttons south/east/west/north are the game's buttons 1/2/4/8, grounding port
  bits 4/5/6/7 under the PC game port's active-low convention (a pressed button grounds its bit; the
  released byte is `0xF0`, all four pressed `0x00`). Device 2's both-player arm maps all four to
  1/2/4/8; P1's device-4 arm reads bits 4/5 as 1/4 and P2's reads 6/7 as 1/4. With one pad and
  `4/4`, P1 therefore gets the pad's first two face buttons and P2 the last two — the arm table's
  positions, which is the only mapping the raw's two-stick port leaves to the host.
- **The scale is host policy.** `HOST_PAD_AXIS_SCALE 64` (`host.c`) divides SDL's int16 range:
  full deflection is `32767 / 64 = 0x1FF`, so the raw's ±0x1E dead band (`0x4FF92..0x4FFD4`) is
  about 6% of travel. The raw fixes the normalized unit (a PIT-tick difference `>> 4`) and the dead
  band, not the SDL stick's range; there is no game-port capture to fit, and the design's
  "The SDL axis scale is host policy" is the authority. This is declared, not fitted.
- **The seam.** `host_set_pad_override(const u8 *axes, const u8 *buttons, int present)` supplies
  four signed 8-bit normalized displacements (X1 Y1 X2 Y2) and four button flags; `(NULL, NULL, 0)`
  disarms it. It lets the unit tests drive the whole model with no SDL and no physical pad; nothing
  outside `port/tests` sets it.

### §H5.3 The unit tests and the mutation proofs

`port/tests/test_platform.c`, six tests registered once in `test.h` after
`test_input_axes_dead_band` (each arms `host_set_port_present_override(1, 1)`, because Task 4's
production presence answer is still "no port" and would otherwise clear a joystick device word on
the same sample):

- `test_host_pad_device0_ignores_pad`: a present, fully deflected pad with all buttons pressed
  leaves a device-0 player's bytes 0 and its axis words at a seeded nonzero center; the host still
  samples the pad (the byte is `0x00` pressed / `0xF0` released).
- `test_host_pad_arm2`: 2/2 reads axes A for both players and maps the port bits 4..7 to 1/2/4/8
  one at a time and all at once; the `0/2` case pins that P2's device-2 arm reads the shared A stick
  while P1 stays on the keyboard.
- `test_host_pad_arm4`: 4/4 reads P1 axes A + bits 4/5 -> 1/4 and P2 axes B + bits 6/7 -> 1/4, each
  button alone and all together.
- `test_host_pad_arm6`: 6/6 reads axes A/B with the keyboard button scans: the pad's own buttons are
  all pressed and the bytes keep the keyboard's `0x01` (a port-reading arm would give `0x0F`).
- `test_host_pad_dead_band`: the fill composes center + displacement; an axis at the seeded center
  ±0x1D is in the dead band and ±0x1E sets the bit, on both axes, negative displacements included;
  a device-0 player's words are never written.
- `test_host_pad_f1f2`: F1/F2 are bit 0 of their player's byte in every arm (0/0, 2/2, 4/4, 6/6),
  with the pad's buttons pressed and released. Because the game-port reader's first bit is also
  byte bit 0 (game bit 1), the test releases keyboard button 1's table slot (`k_cfg_bit` 0x0100 /
  0x0001 is shared with F1/F2 in the seam's packed word) so bit 0 can only come from the fixed
  `0x1B850`/`0x1B870` entries; and one check calls the sampler with the seam armed but no explicit
  `host_pad_fill()`, pinning the per-frame hook.

Each assertion was seen to fail under a mutation, applied alone, rebuilt and reverted (the suite's
only FAILs were the listed ones; `3495`/`3496` are `sampler_bytes`' p1/p2 assertions, so a mutation
names the call site's expectation):

| mutation | first FAILs |
|---|---|
| the fill's axes-A device gate removed (always write) | `3814: 8319 != 8192` (the device-0 center) |
| P2's device-2 arm reads axes B (`input_sampler`) | `3496: 0 != 80`, `1 != 81`, `2 != 82`, ... (the `0/2` and 2/2 cases), 19 total |
| the port byte's game-bit order reversed | `3853: 8 != 1`, `4 != 2`, `2 != 4`, `1 != 8` (arm 2), 28 total |
| P2's two port bits swapped (3/4 -> bits 7/6) | `3853: 8 != 4`, `4 != 8`; `3918: 4 != 1`, `3924: 1 != 4` (arm 4), 10 total |
| the device-6 arm reads the game-port buttons | `3495: 85 != 81`, `85 != 80` (arm 6, `0x55` vs `0x51`), 4 total |
| the fill drops the axis center | `3994: 29 != 8221`, then the whole dead-band set, 9 total |
| the F1/F2 readers return 0 | 27 total, ending `3495/3496: 80 != 81` (the f1f2 arm cases) |
| the port byte active high instead of active low | `3813: 240 != 0`, `3819: 0 != 240` (device 0), 68 total |
| the seam's axes read unsigned (`(u8)`) | `3999: 16 != 0`, `4001: 16 != 32` (the negative dead-band cases), 7 total |
| P2's 4/6 arms write axes A instead of B | `3496: 16 != 64`, `17 != 65` (arm 4's P2 `0x40`), 7 total |
| the sampler's `host_pad_poll/fill` hook removed | `3495: 15 != 80`, `3496: 15 != 80` (the hook-only check), 2 total |

### §H5.4 The default path, the presence dependency and counters

- **No pad / device 0.** `host_pad_fill()` writes no axis words and `g_port_buttons = 0`; the
  `host_input_fill_axes()` call that precedes the sampler in `game_loop` still writes the centers.
  The sampler's device-0 output is byte-identical to §H4's, and the differential row is unchanged
  because the two new host calls are no-ops with no pad. `make diff-verify`: **1019/1019 functions
  VERIFIED; 14956/14956 mutants detected; 0 named gaps; 774/833 rows with callees closed (186 have
  none); 768/833 closed on full rows; 8 ported functions with no row** — the §H4.5 counter
  exactly. `PR_ORACLE_REQUIRED=1 ./build/run_tests`: `all checks passed`.
- **The presence dependency (Task 6's).** `host_port_present()` still answers 0 (the no-pad state),
  so the sampler's presence block clears a joystick device word on the first sample. A windowed run
  with a pad and a config that selects device 2/4/6 therefore still falls back to the keyboard until
  Task 6 models the pad's probe answer; the pad model is exercised through
  `host_set_port_present_override` in the tests. This is the plan's split, not a regression, and is
  the task's main named limit.
- **Untested headless: the SDL open.** The suite never calls `host_init`, so `host_pad_open`,
  `host_pad_poll`'s SDL path and `host_pad_close` are unexercised here; they are a thin host layer
  over SDL3's gamepad API, and the seam covers the model. A windowed run is the only path that
  opens a real pad, and `host_pad_open`'s failure is a silent no-pad, never an abort.
- No new ported function: the host pad functions carry no `/* 0xADDR` header and no `fn_register`,
  so `tools/port_progress.py` and the E2 triage table are unchanged.

## §H6 The presence, the boot probe, the calibration and the device-2 scenario

Task 6 (the code half) closes §H5.4's presence dependency: `host_port_present()` answered 0, so a
windowed run with a pad and a config selecting device 2/4/6 fell back to the keyboard on the first
sample. It models `0x4FBBB`, wires the boot block `0x20D0F..0x20DE3`, implements `0x5004A`/
`0x50021`'s centre capture at pad connect, in `game_init` and on both Alt-J sites, and adds the
`dev1`/`dev2` poll fields and the `gp-u17-device2` scenario. Sources: capstone over the fixed-up
image (image offset = address − `0x10000`) for every instruction below, and the Ghidra listing
(`port/decomp/prage.c:7521-7525`, `11404-11520`) which agrees. The capture half (the measured pins)
is user-gated and is not in this commit.

### §H6.1 The presence model: `0x4FBBB` -> `host_port_present(int probe)`

`0x4FBBB` takes the mask in EAX and returns through AL:

```
4FBBB  push ebx/ecx/edx / mov ebx,eax / mov edx,0x201 / mov ecx,0x2FFFF
4FBCA  in al,dx / test bl,al / je 0x4FBD5 / loop 0x4FBCA     ; wait for the masked bits to CLEAR
4FBD3  jmp 0x4FBF4                                            ; the wait expired -> 0
4FBD5  xor eax,eax / mov ecx,0x2FFFF / out dx,al              ; TRIGGER: write 0 to the port
4FBE7  in al,dx / test bl,al / je 0x4FBFA / loop 0x4FBE7      ; wait for the fan-out (bits clear)
4FBF4  xor eax,eax -> 0        (the second wait expired)
4FBFA  mov eax,1 -> 1          (the fan-out arrived)
```

So the answer is "the port's axis one-shot bits cleared after the trigger": 1 with a game port
attached, 0 with nothing there (the one-shots never fan out and the second wait expires). The masks
are the port's four axis bits: 3 is P1's axes 0/1 (`0x1BC61`, `0x20DAC`) and 0xC P2's 2/3
(`0x1BC7D`, `0x20DD1`); `0x4FDF5`/`0x4FC05`/`0x4FCFD` are the timed poll equivalents over all
four, P1's two and P2's two.

**Mask 0 is a raw quirk**: `test bl,al` with BL = 0 always sets ZF, so both `je` arms are taken and
the function answers 1 whatever the hardware (`0x4FBCD`/`0x4FBEE`). The host keeps that arm
(`if (probe == 0u) return 1;`) rather than pretending a port is attached.

The host owns the answer (`port/src/host.c`): `host_port_present(int probe)` returns 1 when a pad
is attached (`pad_present()`: the Task 5 seam's answer while armed, else the last poll's) and 0
when none is. One SDL gamepad supplies both sticks (left = axes A = P1's pair, right = axes B =
P2's), so every axis mask answers alike; a future two-pad host would split the masks here. The four
Task 4 routines now answer through it with their raw masks — `host_port_poll_both` 0xF,
`host_port_poll_p1` 3, `host_port_poll_p2` 0xC, `host_port_probe(mask)` the mask — and keep their
`PR_SEAM` lines, so the sampler's differential row is unchanged (its four presence stubs are still
stubs: the routines stay host-owned, `tools/port_classification.txt` rows 62-67).
`host_set_port_present_override` still wins while armed (the Task 4/5 unit tests).

**The no-pad game-port byte.** `host_port_buttons()` answers 0 with no pad, which the active-low
readers would read as all four buttons held (§H3.1). That state is **unreachable**: the sampler's
presence block runs before its arms (`port/src/platform/input.c`, `0x1BC4F..0x1BCE6` precedes the
switches at `0x1BCF4`/`0x1BD66`) and clears a joystick device word on the same tick when the answer
is 0, so a device 2/4 arm never reads the byte. §H3.4's "sound only because Task 6 answers the
probe" is now a property of the code, not a plan. The byte's *value* is unchanged (0), so the
existing §H3.4 test's assertion (`host_port_buttons() == 0` with no pad) stays as it was.

### §H6.2 The boot clears: `0x20D0F..0x20DE3` (`game_boot_ports`)

`0x20C10`'s tail runs the controller checks after the key-config record round trip (`0x1AEE0`/
`0x1AF64`, `0x20CF2`/`0x20CF9`) and the handicap copy (`0x20D05`/`0x20D0A`). The flags are DL
(P1's port flag) and DH (P2's): `0x20C29 xor dl,dl` and `0x20C30 xor dh,dh` leave both 0 at
`0x20D0F`, which is what makes the block well-defined. The reconstruction:

```
20D0F  eax=[0x101514]; ax=[eax+0x2d4]; if (ax==2||4||6) dl=1        ; 0x20D2F
20D31  eax=[0x101514]; ax=[eax+0x2d6]; if (ax!=2&&4&&6) goto 20D9E
20D51  if (dl==1) { if ([+0x2d6]==2) { [+0x2d4]=0; dh=0; goto 20D9E } }   ; 0x20D6F
20D7C  else if (dl==0 && [+0x2d6]==2) { dl=1; dh=0; goto 20D9E }          ; 0x20D96
20D9C  else dh=1
20D9E  if (dl==1) { if (0x4FBBB(3)==0)  [+0x2d4]=0 }                      ; 0x20DAC/0x20DB5
20DC3  if (dh==1) { if (0x4FBBB(0xC)==0) [+0x2d6]=0 }                     ; 0x20DD1/0x20DDA
```

Both clears re-read `[0x101514]` after the probe (`0x20DB5`, `0x20DDA`; `0x20D5A` before the
`0x20D6F` clear). The `dev2 == 2` arms are the reason a P2 device-2 selection probes P1's side: a
device-2 P2 arm reads the same both-player axis pair as P1's (`0x1BD8B` calls `0x4FF8F`), so the
block probes mask 3 and leaves DH 0 — P2's word survives this block (the sampler's presence block
clears it on the first tick; `test_input_sampler_arms`'s 2/2 case pins that). No other word is
touched, and a device that is not 0/2/4/6 is never probed.

The port's `game_boot_ports()` (`port/src/game/flow.c`, declared in `flow.h`, called at the end of
`game_init_resume`) is that block line for line. It replaces the named gap at the old
`flow.c:6980-6985` (record named-gaps-b §B.11), and `0x20C10` is an unrowed function
(`tools/diff_verify.py` `UNROWED`), so no differential row sees the new calls. With the shipped
all-zero CMOS both words are 0, so the block takes no branch and writes nothing: the default path
is unchanged (the front-end oracle below).

### §H6.3 The calibration: `0x5004A` and `0x50021` (`host_pad_center`)

```
5004A  test word [0xe1c1c],0xffff / jne 0x50060        ; the period word, stored once
50055  call 0x50021 / mov [0xe1c1c],ax                  ; the PIT channel-0 period
50060  call 0x4fc05 / test eax,eax / je 0x5008d         ; P1's poll: 0 -> skip P1's pair
50069  ax=[0xe1c1e]; [0xe1c22]=ax; cmp ax,0x1e; jl 0x50060   ; the reading -> the centre, then wait
5007B  ax=[0xe1c20]; [0xe1c24]=ax; cmp ax,0x1e; jl 0x50060
5008D  call 0x4fcfd ... 0x50096..0x500B8                ; P2's pair, the same shape
```

`0x50021` latches PIT channel 0 (`out 0x43,0`, two `in 0x40` reads) eight times and returns the
largest difference between successive 16-bit readings — the wrap period `0x4FDF5` adds when a new
normalized reading falls below the previous one. The centre store happens **before** the `cmp`,
so the centre ends up as the first reading that is >= +0x1E: the raw waits until the stick has been
pushed positive and then calls that reading the rest position (holding the stick at a position and
pressing Alt-J centres it there).

The host's model (`host_pad_center`, `port/src/host.c`; called at pad connect
(`host_pad_open`), from `game_init`'s `0x1C0AC` call, and on both Alt-J sites — `0x24DD3` in
`game_key_loop` and `0x2EB4D` in `config_screen_wait`):

- The axis word at rest **is** the centre: `host_pad_fill` writes `centre + displacement`, so a rest
  reading equals the centre and the copy is the plan's "the rest values become the centres". The
  deflected-stick case copies the deflected reading, exactly as the raw's first store does.
- A player's pair is captured only when its poll answers (`host_port_poll_p1`/`_p2`, masks 3/0xC):
  with no pad neither answers and nothing moves.
- `0x50021`'s period is **declared 0**, not fitted: the port's normalized axis words are host
  values (`centre + displacement`), not PIT counts, so there is no period to measure
  (`host_port_pit_period`, a static helper). `0x5004A` stores it once, only while `0xE1C1C` is 0;
  a non-zero word is left as found.
- What is not claimed: the PIT timing itself, the `>= +0x1E` wait loop (a host rest capture has
  nothing to wait for), and any measured stick offset at connect (the axis words are 0 there:
  `game_init`'s image load and `host_input_fill_axes` come after `host_init`'s pad open).

The `0x5004A`-caller evidence: `0x1C0AC` (the init chain, between `0x336C0` and `0x1D0BC`),
`0x24DD3` (the match key loop's Alt-J arm) and `0x2EB4D` (the BIOS key wait's Alt-J arm) — found by
scanning the image for `call` displacements to `0x5004A`, not from the callers column alone. The
two differential rows whose named-unhit blocks are those arms (`game_frame_keys`' `0x24DD3`,
`0x2EA78`'s `0x2EB4D`) now say the block is unhit because no case queues the Alt-J word, not
because the port leaves it out; no counter moves (§H6.7).

### §H6.4 The `dev1`/`dev2` fields and the existing captures

`gp_session.SNAP_FIELDS` gains `('dev1', 0x2D4, 2)` and `('dev2', 0x2D6, 2)` **appended** after the
u6 fields, so every older poll.log's S lines are still a prefix of the format and parse as they did
(the parser is key/value; an absent field is absent, and `test_gp_session`'s strip test asserts it).
The two words live in the key-config block behind `DS_00101514`, so they cannot be read at a fixed
DS address like every other S field:

- `gp_session.POINTER_FIELDS` names them, and `gp_capture.read_snap` reads such a field at
  `[DS_00101514] + its offset` (the poller's `ptr`, the same value it already uses for the key-state
  table and the bitmap). Without the pointer — the pre-block state read and the consistency reads —
  there is no address to read and the value is 0; an S record is only written with the pointer.
- The port trace's T line appends `dev1`/`dev2` at the same position (SNAP_FIELDS then the
  scenarios' extra fields), read as `DSW(DSD(DS_00101514) + 0x2D4/0x2D6)`; the
  `test_gp_session` order/width test pins the format against the C source.
- `gp_compare --dev-words` adds `gp_session.DEV_FIELDS` to the trace claim, and a capture whose S
  records lack them FAILs ("the capture records no device-word fields") rather than comparing
  nothing. Only `gp-u17-oracle` passes the flag: every existing capture's poll.log lacks the
  fields, and adding them to the default claim would change what those oracles mean (§H5's
  anti-goal). Their pins do not move: the pinned N, F, window start, reach and poll.log sha256 are
  computed from the same capture bytes and the same `TRACE_FIELDS`.

### §H6.5 The unit tests and their mutation proofs

`port/tests/test_platform.c`, three tests registered once in `test.h` after `test_host_pad_f1f2`:

- `test_host_port_present`: no pad -> 0 for the three axis masks, 1 for mask 0 (the raw's quirk);
  a pad -> 1 for all three and through the four routines; the pad gone -> the four answer 0; the
  Task 4 seam still wins while armed.
- `test_boot_probe_clears`: no pad -> device 2/0 clears P1 (`0x20DAC`); 4/4 clears both; 2/2 clears
  P1 at `0x20D6F` and leaves P2 at 2 (the sampler's tick clears it); 0/2 leaves P2 at 2; a pad ->
  2/0 and 4/6 are kept; 1/3 (not a port) is never written.
- `test_host_pad_center`: no pad -> no centre moves and a non-zero PIT period word is kept; a pad ->
  each axis's rest reading becomes its centre (distinct values, so a swapped pair fails); a second
  capture takes the new reading; the at-rest readers see their dead band; and the match key loop's
  Alt-J word (`input_push(0x24, 0)`, `game_key_loop`) reaches the capture.

Each assertion was seen to fail under at least one mutation, applied alone, rebuilt and reverted
(the suite's only FAILs were these):

| mutation | failing checks |
|---|---|
| `host_port_present` always answers 1 (the no-pad answer removed) | `4109/4110/4111: 1 != 0`, `4126..4129`, `4159: 2 != 0` (the boot clear), plus `3671: 2 != 0` and `3495: 17 != 1` (the sampler's production-answer block) |
| the mask-0 arm dropped | `4112: 0 != 1` |
| `game_boot_ports` never probes DH's side | `4166: 4 != 0` (the 4/4 case) |
| `0x20D6F`'s clear writes `+0x2D6` instead | `4173: 0 != 2` (the 2/2 case) |
| `host_pad_center` captures P1's pair with no poll gate | `4228: 4369 != 0`, `4229: 8738 != 0` |
| P1 Y's centre read from X's word | `4238: 4369 != 8738`, `4257: 21845 != 26214` |
| the Alt-J call dropped from `game_key_loop` | `4257: 8738 != 26214` |

Named limit: `host_pad_open`'s connect-time capture, `host_pad_poll`'s SDL path and the
`config_screen_wait` Alt-J site are not exercised headless (the suite never calls `host_init`, and
driving `config_screen_wait` presents a frame); they are thin wiring over the tested function, and
the flow.c site is tested. The SDL pad's own answer is the windowed run's (§H5.4).

### §H6.6 The `gp-u17-device2` scenario (capture pending)

`gp_session.SCENARIOS['gp-u17-device2']`: the K11 walk's first 24 keys
(`k11_session.WALK[2:24]`, after the boot Enter and the START MENU Enter) take the service menu
through GAME OPTIONS and the OPTIONS MENU's CONFIG OPTIONS editor, STATISTICS' five screens, SOUND,
MUSIC and into MODIFY CONTROLS (`0x31F24`); then one `p1.down` steps P1's device KEYBOARD (0) ->
JOYSTICK 2 (`0x3206F`/`0x320ED` add 2) and `esc` leaves, whose `0x32345 config_keys_apply` writes
the record's device into BIOS+0x2D4 (`0x1AE3B`). DOSBox-X has no game port, so the ISR sampler's
presence block probes `0x4FBBB(3)`, gets 0 and clears the word on its next tick (`0x1BC6F`). The
scenario ends 60 frames after the leave (before anything in the OPTIONS MENU could save). Harness
values: U4's 150-frame gap into the MAIN MENU, 30-frame gaps between the walk's keys, a 5-frame pad
hold (a press reaches the pad level one iteration late, record §G.7.2), a 90 s limit, and
`STOP_AT_END` (nothing past the clear is compared).

`tools/gp_u17.py check` is the capture-side evidence (record §H6.6's claim): the S records carry
dev1/dev2; the scenario's own last two injected presses are the device step and the leave; the mode
stays 0x27 from the menu Enter on; dev2 stays 0; the selection is seen as dev1 = 2; the clear is the
first S record after the leave with dev1 = 0 and no later record holds a word. The selection's
visibility is a timing race (the word is 2 for one game tick); if a capture's poll misses it, the
tool refuses to date the clear unless `--unseen-selection-ok` is passed, which records a named gap
and pins only the end state. The unit tests (`tools/tests/test_gp_u17.py`, 14) cover each row's
pass and fail on synthetic logs, the pin, the CLI exit codes and the scenario's steps.

The pins are **empty** in `Makefile`'s `GP_U17_*` block until the capture is taken (user-gated):
`GP_U17_CLEAR_FRAME` (the tool's measured clear frame), `MIN_FIRST`, `TRACE_MIN_FIRST` (over
`TRACE_FIELDS + DEV_FIELDS`), `MAX_START`, `MIN_REACH`, `GP_U17_CAPTURE_SHA256` and
`GP_U17_CAPTURE_FRAMES`. `gp-u17-oracle` runs the tool's unit tests always, then skips without the
capture directory; with the capture present, an empty pin fails first in `gp_compare` ("the ratchet
N is not pinned" / "the capture identity ... is not pinned"). The claim is narrow: no game-port
axis or button is injected (the harness cannot), so the arms' evidence is the unit tests and the
differential rows; the capture pins the **clear**, not the pad's answer.

### §H6.7 Counters and gates run (the code half)

- `PR_ORACLE_REQUIRED=1 ./build/run_tests` (repo root): `all checks passed`.
- `make diff-verify`: **1019/1019 functions VERIFIED; 14956/14956 mutants detected; 0 named gaps;
  774/833 rows with callees closed (186 have none); 768/833 closed on full rows; 8 ported functions
  with no row** — §H4.5's counter exactly (the two updated unhit reason strings move no row).
- `tools/port_progress.py`: `799 1203 66`, `759 759 100` — unchanged (the new host functions carry
  no `/* 0xADDR` header and no `fn_register`; two inline comments that would have read as such were
  reworded).
- `make frontend-oracle`: 1325 frames in window, 517 clean / 801 splice / 3 transition, the two
  named unexplained frames (832, 833), endpoints OK — the boot path (`game_boot_ports`,
  `host_pad_center` in `game_init`) moves no frame.
- `make gp-report scenario=gp-pads`: the replay's T line carries `dev1=0000 dev2=0000` and the
  trace compares `0 differing through 1041` (the new fields parse and the default claim ignores
  them).
- `make entry-triage`: the committed table equals a fresh run.
- `make gp-u17-oracle`: the tool tests run, the capture is absent -> `(skipped)`.
