# Final Truth — derivation record

**Plan:** `docs/superpowers/plans/2026-10-30-final-truth.md`.
**Design:** `docs/superpowers/specs/2026-10-30-final-truth-design.md`.
**Base:** `9a149100` (the Live Truth merge); worktree/branch `final-truth`.
**Status:** in progress (this record grows per task; §F1 is Task 1).

Each section is the authoritative raw-byte source for its task. Addresses are
linear (object base + offset); a data-object VA `0x8xxxx` is file offset
`VA + 0x46E54` in `data/game/C/PRAGE.EXE`, a code-object VA `0x1xxxx` is file
offset `VA + 0x52E54` (pre-fixup; the loader's fixup records rewrite data
immediates).

## §F1 — `0x2CADC` boot-defaults fidelity (Task 1)

**Outcome: implemented.** The three calls the port used to omit are wired, the
`0x2CADC` differential row is the full ten-call row, and no frame-indexed
oracle moved: the title and attract dumps are byte-identical before and after,
and the title/attract/front-end/K11/gp/save-reload/audio oracles are green with
every existing pin unchanged. `WEAKER` reads 12 and the differential counter
reads `810/845` closed on full rows (only the full-row count moved).

### F1.1 The raw call trace

`0x2CADC` is 150 bytes (`0x2CADC..0x2CB71`). Its disassembly (raw file
`0x7F930`) is:

```
0002cadc  53              push ebx
0002cadd  51              push ecx
0002cade  52              push edx
0002cadf  b9 00b00000     mov ecx, 0xb000
0002cae4  bb c8090000     mov ebx, 0x9c8          ; fixup -> 0x809C8
0002cae9  ba 0e000000     mov edx, 0xe
0002caee  b8 ffffffff     mov eax, -1
0002caf3  e8 a0260000     call 0x2f198
0002caf8  b8 b42e0200     mov eax, 0x22eb4        ; fixup -> 0xA2EB4
0002cafd  e8 ce010000     call 0x2ccd0
0002cb02  89 c2           mov edx, eax
0002cb04  b8 29000000     mov eax, 0x29
0002cb09  e8 fe0e0000     call 0x2da0c
0002cb0e  ba a0000000     mov edx, 0xa0
0002cb13  b8 35000000     mov eax, 0x35
0002cb18  e8 ef0e0000     call 0x2da0c
0002cb1d  ba a0000000     mov edx, 0xa0
0002cb22  b8 37000000     mov eax, 0x37
0002cb27  e8 e00e0000     call 0x2da0c
0002cb2c  b8 2a000000     mov eax, 0x2a
0002cb31  e8 3e0e0000     call 0x2d974
0002cb36  24 fc           and al, 0xfc
0002cb38  0c 03           or  al, 3
0002cb3a  b9 00b00000     mov ecx, 0xb000
0002cb3f  89 c2           mov edx, eax
0002cb41  b8 2a000000     mov eax, 0x2a
0002cb46  bb f0090000     mov ebx, 0x9f0          ; fixup -> 0x809F0
0002cb4b  e8 bc0e0000     call 0x2da0c
0002cb50  e8 cbe2feff     call 0x1ae20
0002cb55  b8 b4000000     mov eax, 0xb4
0002cb5a  ba 0e000000     mov edx, 0xe
0002cb5f  e8 141f0000     call 0x2ea78
0002cb64  b8 ffffffff     mov eax, -1
0002cb69  e8 12270000     call 0x2f280
0002cb6e  5a              pop edx
0002cb6f  59              pop ecx
0002cb70  5b              pop ebx
0002cb71  c3              ret
```

The three calls the port used to omit, in raw order:

| site | call | arguments |
|---|---|---|
| `0x2CAF3` | `0x2F198` | EAX `-1` (col), EDX `0xE` (row), EBX `0x809C8` (string), ECX `0xB000` (mode) |
| `0x2CB5F` | `0x2EA78` | EAX `0xB4` (n); EDX `0xE` is loaded at `0x2CB5A` and only carried to the next call |
| `0x2CB69` | `0x2F280` | EAX `-1` (col), EDX `0xE` (row), EBX `0x809F0` (string), ECX `0xB000` (mode) |

Why `0x2F280`'s EBX/ECX are live without a reload after the wait:

* EBX `0x809F0` is set at `0x2CB46` for the `0x2DA0C(0x2A, ..)` call and ECX
  `0xB000` at `0x2CB3A`; `0x2DA0C` pushes EBX/ECX at `0x2DA0C`/`0x2DA0D` and
  pops them at `0x2DADF`/`0x2DADE`; `0x1AE20` (entry `0x1AE20` falls into
  `0x1AE28`) pushes EBX/ECX/EDX/ESI/EDI at `0x1AE28..0x1AE2C` and pops them at
  `0x1AED8..0x1AEDC`.
* EDX `0xE` is loaded at `0x2CB5A`; `0x2EA78` pushes EBX/ECX/EDX/ESI at
  `0x2EA78..0x2EA7C` and pops them at `0x2EB7A..0x2EB7D`, so the value arrives
  at `0x2F280`. The same EBX/ECX/EDX/EBP preservation is the one
  `tools/diff_emu.py:1217-1221` records for the `0x1AE20` callee fix.

The call chain: `0x2D6F8` (the storage validate, port `config_validate`,
`config.c`) calls `0x2CADC` at its defaults arm `0x2D886` when the four magic
bytes at `0x105E30` are not `0x9C94D2C4`; `0x2F9CC` (master init, port
`game_master_init`, `flow.c:6975`) calls `0x2D6F8` at `0x2FA0B`; `0x20C10`
(port `game_init`, `flow.c:7002`) calls `0x2F9CC` at `0x20C15`; the master loop
`0x255CC` (port `game_loop`) starts afterwards. On the shipped all-zero `CMOS`
the magic is 0, so the defaults arm and these three calls run on every boot.

### F1.2 The message string is the data object's own, not a literal

The two immediates are fixed up by the LE loader: `0x9C8` is an object-1
(data) offset, so the runtime EBX values are `0x80000 + 0x9C8 = 0x809C8` and
`0x80000 + 0x9F0 = 0x809F0` (the same per-site base rule as the menu table
`0x22EB4 -> 0xA2EB4`; record `2026-09-19-eeprom-config-derivations.md` and
`2026-10-12-reverse-d5d-derivations.md` §5). The bytes at those VAs (file
offsets `0xC781C` and `0xC7844`) are:

```
0x809C8: 53 45 54 54 49 4e 47 20 43 4f 4e 46 49 47 55 52   SETTING CONFIGUR
0x809D8: 41 54 49 4f 4e 20 44 45 46 41 55 4c 54 20 56 41   ATION DEFAULT VA
0x809E8: 4c 55 45 53 00 0a 2a 09 53 45 54 54 49 4e 47 20   LUES..*.SETTING
0x809F8: 43 4f 4e 46 49 47 55 52 41 54 49 4f 4e 20 44 45   CONFIGURATION DE
0x80A08: 46 41 55 4c 54 20 56 41 4c 55 45 53 00            FAULT VALUES.
```

`0x809C8` is `"SETTING CONFIGURATION DEFAULT VALUES"` (36 characters, NUL at
`0x809EC`); `0x809ED..0x809EF` are `0A 2A 09`; the second copy starts at
`0x809F0` and ends with its NUL at `0x80A14`. The message is **not** in the
loaded localisation table: `ENGLISH.TXT` has no `SETTING` string and
`game_string_get` reads the external table, so the faithful source is the
data-object VA. The port therefore passes `mem + 0x809C8u` and
`mem + 0x809F0u`; it does not transcribe the text. The mode `0xB000` has
`mode & 3 == 0`, so `0x2F0F0` returns `strlen` = 36 (`text_width`, `actors.c`).
`0x2F198` centres a col `-1` as `(0x2B - 36) >> 1 = 3` and stores
`{row, col + extent} = {0xE, 0x27}` at `0x105F34/36`.

`0x2F280` takes the same register shape (`actors.h:194`): its `text_width` is
the same 36, so it walks the same 36 cells from the centred col 3, row `0xE`,
through the grid at `0x105F38` (`row * 0xAC + col * 4`), releases each record
through `0x2AD40` and zeroes the cell.

### F1.3 The wait: one frame, 182 tick passes

`0x2EA78` (port `config_screen_wait`, `config.c:901`) reads EAX into ECX
(`0x2EA7C mov ecx,eax`), clears the latch `0x105F30`, runs `0x2A31C`,
`0x1C3FC`, `0x14328`, copies/blits the back buffer to the aperture and swaps
(`0x2EAD6`; one presented frame per call, before any waiting), and returns
immediately for `n == -1` (`0x2EAE0 cmp ecx,-1; je 0x2EB7A`). For any other n
the body runs for `ECX = n, n-1, .., -1` — the `0x2EB6E mov eax,ecx; dec ecx;
cmp eax,-1; jg` test reads the pre-decrement value — so **n + 2 passes**, each
waiting for `0xEF6DE` to change, pumping `0x500C4`, and draining the BIOS key
queue (record §49-Y). `n = 0xB4` is therefore **182 passes and one presented
frame**. The port's model already implements this (record §49-Y; the
`0x2EA78` row runs it with the key model).

The present happens inside `game_init`, before the master loop's first
iteration, and it is outside every compared frame window: the gameplay
captures select their frames from the post-logo AVI start (`window.txt`
`00000 1371` for `gp-idle-loss`), frame 0 is all black and frame 1 is the
166-pixel `- LOADING -` strip; the message present is not in the capture's
`frame_*` set, and the title/front-end/attract captures are post-logo too.
This is why the wait adds no captured frame.

The wait's tick writes are also outside the loop's state:

* `0x2EA78`'s ISR model advances `DS_00101508`, `DS_00101500`,
  `DS_000EF6DE` and `DS_00105D88` once per pass (182 times) [Port: matched by
  the `0x1BDF4` model in `config_screen_wait`]. The raw's ISR is
  `0x1BDF4`; the same 182 increments are what a capture made by the original
  would have.
* `game_init_resume` then calls `actors_reset()` (`0x2BAF4(1)`, `flow.c:7093`
  and `game_state_init` at `6065`), whose non-zero arm calls `0x52106`
  (`actors.c:1157`), and `gfx_screen_reset(0)` stores 0 into **both**
  `DS_00101508` and `DS_0010150C` (`gfx.c:225-226`; raw `0x52106` at
  `0x52108`/`0x5210D`). `game_isr_word_reset` (`0x5D808`, `flow.c:7134`)
  zeroes `DS_000EF6DE`. So the loop starts with the pair `0/0` and the frame
  word 0 exactly as it did before; the wait cannot move the gate.
* `DS_00101500` and `DS_00105D88` keep the 182 (they are not reset): a
  temporary stderr probe (removed) measured `1508=194, 150c=12, 1500=194,
  5d88=194, ef6de=00B6` at the end of `config_set_defaults` (the 12 is the
  pre-init resource-read model) and `1508=0, 150c=0, 1500=194, 5d88=194,
  ef6de=0` at the end of `game_init`. `config_run_clock(0)` in the init tail
  consumes the elapsed ticks (194 there: the 12 pre-init read ticks plus the
  wait's 182) into the run-clock total; no compared frame depends on it (the
  measured dumps below are byte-identical).

### F1.4 The frame-index measurement

The derivation experiment: baseline dumps were taken before the change; the
three calls were wired; the same drivers ran again. The dumps are compared
frame-by-frame by md5:

| dump | baseline frames | new frames | differing |
|---|---|---|---|
| `PR_TITLE_DUMP` (`title/`) | 96 | 96 | **0** |
| `PR_ATTRACT_DUMP` (`attract/`) | 690 | 690 | **0** |

The final (default-flags) build's title and attract dumps are also compared
against those baseline files: 96/96 and 690/690, 0 differing. The actor-spawn
miss counters rise by exactly 33 (the message's 36 characters minus its three
spaces): title `actor_spawn` 7199 -> 7232, `set_dead` 7053 -> 7086; attract
8132/7987 from 8099/7954. That is the draw and release running, not a frame
change.

The focused oracles on the final build, all green, with no re-pin:

* `make title-oracle`: 96-frame window, capture 1 95/96 exhibited (missing
  frame 0), capture 2 111 frames (54 clean, 57 splice), determinism 54/54.
* `make attract-oracle`: 173/690 port attract frames exhibited, 74 missing
  classes (pin 74), first divergence at capture 215/216 (raw 2175/2193).
* `make frontend-oracle`: 1325 frames in window (517 clean, 801 splice, 3
  transition, 2 allowed by name), 30 missing classes (pin 30), END 1884.
* `make k11-oracle`: walk and menuesc, 0 unexplained; menuesc END 388,
  184/184 non-black port frames exhibited.
* `make gp-oracle` (`gp-idle-loss`): first unexplained capture 7559 (ratchet
  7559), trace 0 differing through 8319 (ratchet 8320), 29 missing classes
  (pin 29); the trace's `tick` field is reported-only and still differs first
  at f=141 (not a claim).
* `make save-reload-oracle`: boot 1's replay writes 2040 bytes identical to
  the capture's `CMOS` (sha256 `dad3b1f6…`); boot 2 holds the five ranges at
  `f=0x1EB` and every later dumped frame.
* `make audio-oracle`: 29-trace summary ok, 11m16s.

No oracle was re-pinned; the front-end/menuesc/gp start and END pins are
unchanged. (One `make frontend-oracle` invocation failed its determinism
child check when run in parallel with `make attract-oracle`; the same target
alone passed. That is a harness concurrency effect, not a dump difference.)

### F1.5 The differential row and the WEAKER count

`tools/diff_verify.py`'s `0x2CADC` spec had three five-byte nop pokes
(`0x2CAF3`, `0x2CB5F`, `0x2CB69`). It is now the full ten-call row: the pokes
are gone and the three callees are stubbed/recorded exactly as the other seven:

```
D6A_MSG_DRAW    = E.Call(0x2F198, ("eax","edx","[ebx]","[ebx+4]","[ebx+8]","[ebx+12]","ecx"),
                         clobbers=("ebx","ecx","edx"))
D6A_MSG_RELEASE = E.Call(0x2F280, ("eax","edx","ebx","ecx"), clobbers=("ebx","ecx","edx"))
D6A_MSG_WAIT    = E.Call(0x2EA78, ("eax",), clobbers=("edi","ebp"))
```

The draw's `[ebx]`..`[ebx+12]` is the by-value string shape the `0x2F198` row
already uses (the seam passes the four dwords of `mem + 0x809C8`); the release
passes the pointer (`0x809F0`); the wait passes `0xB4`. The three callees'
own rows are VERIFIED (`text_cursor_set`, `text_cells_release`,
`config_screen_wait`). The row's 12 existing mutants are unchanged and all
detected. The wiring has a RED/GREEN: with the three calls commented out (the
row still un-poked) the row reads MISMATCH, and with them restored it reads
VERIFIED. Two further one-step probes confirmed the arguments are live:
changing the release string to `0x809C8` and changing the wait to `0xB3` each
turned the row MISMATCH (all three probes reverted).

The full `--self-check` run now prints:

```
1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps;
825/845 rows with callees closed (188 have none); 810/845 closed on full rows;
8 ported functions with no row (45 FN_ ported-unrowed: 8 portable
[5 registry-visible + 3 header-shape exceptions] + 37 runtime; the scan-visible
subset is 42 = 5 + 37)
```

Only `809/845 -> 810/845` moved (the row became full); functions, mutants,
named gaps, rows with callees and unrowed are unchanged. `WEAKER` drops
`0x2CADC` (13 -> 12) and `WEAKER_CENSUS` loses its entry; the test's pinned
counts and the D1 mutant core/indices are updated (`@nokeys` now differs at
calls #7/#8/#9 because the missing keys call shifts the wait and release;
`@keysfirst` at #0..#7; the other indices +1). The `config.h` notes now
describe the calls instead of the omission.

### F1.6 The unit test

`check_pset_layer` (`port/tests/test_game.c`, `test_actors`) gains the derived
facts: the two data-object strings are byte-equal to
`"SETTING CONFIGURATION DEFAULT VALUES"`; `text_width(0x809C8, 0xB000) == 36`;
`text_cursor_set(-1, 0xE, 0x809C8, 0xB000)` writes `{0xE, 0x27}` and the first
and last message cells hold records; `text_cells_release(-1, 0xE, 0x809F0,
0xB000)` zeroes both and sets the released record's dead bit and clears its
pset `+0x0E`. The existing `test_cfg_helpers` suite passes. The assertion was
mutation-proved by flipping the expected cursor high word `0x27 -> 0x28`:
`run_tests` failed `39 != 40` at `test_game.c:2195`, then passed after the
revert.

### F1.7 Limits of the claim

* The claim is the three calls and their derived arguments, proven by the
  differential row's recorded call arguments and the callees' own rows; the
  message's rendered pixels are not gated (the wait's present is outside every
  capture's frame set). The visible result is the port window's boot message.
* `DS_00101500`/`DS_00105D88` keep the wait's 182 ticks past the loop entry
  (the raw does too), but no compared frame reads them; the tick trace field
  remains reported-only (`gp-oracle`'s first tick difference f=141).
* The oracle windows and their pins are unchanged; the claim stays the
  documented narrow one, and this task adds no frame pin.

## §F2 — the 5120 band (Task 2)

**Outcome: Part A done (no holder); Step 2's one capture ran and the decision
is PASS-A (§F2.5; the analysis is §F2.5–§F2.6).** The
per-pixel holder search over the whole U13 dump finds no port frame that holds
the capture-5120 band (F2.1); the fade source and the DAC-injection write list
are derived (F2.2/F2.3), the `gp-u13-band-dac` scenario and its S fields are
prepared and unit-tested (F2.4), and the one authorised capture
(`data/k11-captures/gp-u13-band-dac`) was analysed in §F2.5. No pin or
allowance changed; the band allowance `(5120, 1, 5, 56b900b1…)` stays, and the
capture is the evidence for the PASS-A decision.

### F2.1 The holder search (Step 1, no capture)

The dump is a fresh current-tree replay of `gp-u13-ending2` with
`--end-to-last-s` (`GP_DUMP=/tmp/pr_gp_dump`, copied to
`/tmp/opencode/t2-gp-dump/gp-u13-ending2`): **4,953** `.ipx` frames,
`frames.txt` maps `04314 f=1824` … `04318 f=1828` (the §D1.5.2 correction, not
the design-era one-frame-later numbering). The capture is the existing
`data/k11-captures/gp-u13-ending2/frame_05120.raw.gz` (RGB 320×200×3). The
comparison is **rendered RGB** — each port frame's index byte at an offset
mapped through that frame's own 768-byte DAC — because the capture stores RGB
and a "holder" means the port can produce those values. The tool is
`tools/gp_band.py --holder-search` (`holder_search`), tested in
`tools/tests/test_gp_band.py`.

The capture-5120 values at the 14 offsets (`BAND_APERTURE` order) are
`00001c` ×8 (offsets 0..6 and 8), `000000` at (3,74) and at the four (4,·)
offsets, control (1,100) `000000`. The port frame at f=0x1826 (`04316.ipx`)
holds indices `0x32`×8 / `0x2E` / `0x4D`×4 / control `0x65`, rendering
`000000`/`100000`/`00000c`/`000000`; its DAC has exactly five entries that
render `00001c` — `0x15, 0x40, 0x42, 0x43, 0x4F`.

Over all 4,953 frames:

| group | holders |
|---|---|
| each of the 8 `00001c` pixels (offsets 0..6, 8) | **0** |
| (3,74) `000000` | 716 |
| the four (4,·) `000000` | 658 / 657 / 572 / 572 |
| row 1 segment (4 offsets) | **0** |
| row 2 segment (2 offsets) | **0** |
| row 3 segment (3 offsets) | **0** |
| row 4 segment (4 offsets, all black) | 569 |
| control (1,100) `000000` | 711 |
| whole band (13) | **0** |
| whole band + control (14) | **0** |

So no port present renders `00001c` at any of the eight blue offsets (a holder
would need one of the five `00001c` DAC entries' indices at all eight, under
that frame's own DAC). The five black offsets are trivially held (black is
common); the blue ones are not, and the row-1/2/3 segments and the whole band
have no holder. The band is **not** a splice/order artifact of a dumped port
present: Step 1 decides "no holder", Step 2's capture is needed. (A capture-
side check agrees the band is not a stale capture present either: no earlier
capture frame 0..5119 holds the band's rows 1..5; frame 5120 is the only one.)

### F2.2 The fade palette source and the 24 records (raw)

`0x29B74` (`frontend_darken_all`, `port/src/game/flow.c:403-425`; code-object
file offset `0x7C9C8`) disassembles to:

```
00029b77  e874a2feff   call 0x13df0        ; effects_clear (0x13DF0)
00029b7c  31c0         xor eax, eax
00029b7e  e8819d0000   call 0x33904        ; the palette table walk
00029b85  85c0         test eax, eax
00029b87  7419         je 0x29ba2
00029b89  ba03000000   mov edx, 3          ; the 0x13D4C byte_arg
00029b8e  89d8         mov eax, ebx        ; the live table entry
00029b90  e8b7a1feff   call 0x13d4c        ; effects_spawn_darken(entry, 3)
00029b95  89d8         mov eax, ebx
00029b97  e8689d0000   call 0x33904
...
00029ba2  ba78000000   mov edx, 0x78
00029ba7  b915000000   mov ecx, 0x15
00029bac  668915ee880800 mov word [0x888ee], dx   ; DS_001088EE = 0x78
00029bb3  668915fe4a0800 mov word [0x84afe], dx   ; DS_00104AFE = 0x78
00029bba  66890d004b0800 mov word [0x84b00], cx   ; DS_00104B00 = 0x15
```

`0x33904` (`frontend_list_next`, `flow.c:371-383`) walks the fixed 0x10-stride
table `DS_00107608..DS_00107798`, returning each entry whose `+4` dword
(refcount) is non-zero: the **first entry is 0x107618** and there are **24
slots** (`(0x107798-0x107618)/0x10`). `0x33754` (`palette_acquire`,
`actors.c:1249-1280`) fills each slot `{handle@+0, refcount@+4, first@+8,
len@+0xC}` and enqueues the range through 0x33734.

`0x13D4C` (`effects_spawn_darken`, `effects.c:171-205`; raw at 0x13D4C:
`c6 41 0c 04` = `mov byte [ecx+0xC],4`, `88 51 0d` = `mov [ecx+0xD],dl`)
pops the free-list head, sets type 4, `+0xD = 3`, `+0xE = 1`, copies the
resolved block's `len` colours into `+0x10` and head-inserts the record on the
active list (`effn++`). `0x134C0` (`effects_step`, `effects.c:337-423`) ages
each record: `+0xE--`; at 0 it reloads `+0xD` and runs the case-4 body, which
subtracts 8 per channel from every `+0x10` dword (floor 0) and calls
`palette_record(rec+0x10, DSD(src+8), DSD(src+0xC), 0)`. `0x33734`
(`gfx.c:59-93`) appends `{ptr, first, count, flag}`; `0x1C470`
(`gfx_flush_palette`, `gfx.c:95-185`) drains it and writes the DAC with the
raw's 6-bit truncation and the VGA's `(v<<2)|(v>>4)` expansion. `0x50188`
(`swap_buffers`, `flow.c:183-190`; decomp `prage.c:36769-36772`) exchanges
`DS_000E87A0`/`DS_000E87A4` after the present copy (`flow.c:7395-7411`), so
`DSD(DS_000E87A0)` at the S record names the just-presented 0xFA00 buffer.

**The measured state at the band's frame** (gdb on the `build/run_tests` U13
replay; `game_frame` entry with the counter at 0x1825 = the spin the W writes
land in, and `gfx_flush_palette` of iterations 0x1825..0x1828):

* The table has **17 live entries** (slots 0..16; slots 17..23 are zero).
  Entry 2 is `{handle 0x0105FF3C, rc 13, first 0x20, len 0x30}`; entry 5 is
  `{handle 0x0C039CA0, rc 16, first 0x52, len 0x3C}`.
* The effect pool holds **12 active records** at the spin, all type 4, all
  `+0xD = 3`, `+0xE = 3`; the active sentinel is
  `{next 0x0F842C, prev 0x0F1B28}`, the free head 0x0F0B00, `effn = 12`. (At
  the earlier spin before 0x1821 it is **14** — entry 0's and entry 16's
  records have not retired yet; 24 is the pool/table size, not the active
  count.) The free-list pop order is the table order (entry 0's record is
  0xF0B00, the pristine head), so entry 2's record is `0xF0B00 + 2*0x814 =
  0x0F1B28` and entry 5's `0x0F3364`.
* The four band indices are covered by exactly those two records: entry 2's
  range `0x20..0x4F` holds `0x32`, `0x2E` and `0x4D`; entry 5's `0x52..0x8D`
  holds the control `0x65`.
* The dirty-list flush at iteration 0x1825 holds the 12 fade records; at
  **0x1826 and 0x1827 it is empty** (no case-4 body runs: `+0xE` 3→2→1), and at
  0x1828 it holds them again (the 1→0 body + its darken). A colour-only write
  at the 0x1825 spin would therefore never be presented; the injection writes
  each record's `+0x0E = 1` too, which makes iteration 0x1826's body run once.

**The plan-vs-raw correction (the design's 56 writes).** The design/plan say
"56 derived W writes (24 records × 4 sentinel groups)". The raw measurement
gives a different shape: 24 is the pool/table size (17 live, 12 active at the
frame), the four indices are covered by exactly two records, and 24×4 is 96,
not 56. The derived injection is **6 W writes** (below); the "56" is not used
and no two-record record set is widened. This is the record's correction; the
task's brief inherits it.

**The injection (6 writes at the spin entry+81).** For each of the two records, write
`+0x0E = 1`, and for each of the four indices write the record's `+0x10` dword
at `4*(index - first)` with a sentinel word. The case-4 body darkens the block
before the upload, so each word is pre-darkened (channel 0xFF -> 0xF7); after
`0x1C470`'s conversion the presented colour is 0xF7 in each written channel:

| index | address | word | presented colour |
|---|---|---|---|
| state (entry 2) | `0x0F1B28+0x0E = 0x0F1B36` | `01` | — |
| state (entry 5) | `0x0F3364+0x0E = 0x0F3372` | `01` | — |
| `0x32` | `0x0F1B28+0x10+4*(0x32-0x20) = 0x0F1B80` | `0x00FF00FF` | `f700f7` magenta |
| `0x2E` | `0x0F1B28+0x10+4*(0x2E-0x20) = 0x0F1B70` | `0x00FFFF00` | `00f7f7` cyan |
| `0x4D` | `0x0F1B28+0x10+4*(0x4D-0x20) = 0x0F1BEC` | `0x00FFFFFF` | `f7f7f7` white |
| `0x65` | `0x0F3364+0x10+4*(0x65-0x52) = 0x0F33C0` | `0x0000FFFF` | `f7f700` yellow |

These are `gp_session.BAND_DAC_RECORDS`/`BAND_DAC_SENTINEL_WORDS`/
`BAND_DAC_WRITES`; the tests pin the six addresses and the four colours. The
port dry run (gp-u13-ending2's script, whose entry is 0x17D4, with
`poke 6182 …` = 0x1826 decimal — the target F = entry+82, the port driver's
`F = W.f+1` convention) lands them correctly: frame 04316 (f=0x1826) and 04317
(f=0x1827) present the four sentinels at all 14 offsets, and frame 04319
(f=0x1829) shows the natural darken 0xEF (0xF7-8), as derived.

The S fields read at the same spin in the dry run are
`bbp0=02A6A550 bbp1=02A5AB50 eff0=000F842C eff1=000F1B28 efffree=000F0B00
effn=12`, and the just-presented buffer's 14 bytes are
`32 32 32 32 32 32 32 2E 32 4D 4D 4D 4D 65`.

### F2.3 The scenario and the S fields

`gp_session.BAND_DAC_EXTRA` appends `bbp0`/`bbp1` (DS_000E87A0/DS_000E87A4),
the 13 band bytes `bb00..bb12`, the control byte `bbc`, and
`eff0/eff1/efffree/effn` (DS_000FCCE0/4/E8, DS_0009AF3D). `read_snap`
(`tools/gp_capture.py`) follows the `bbp0` pointer for the 14 bytes (a stored
linear pointer is the memory file's offset; the same convention
`SLOT_RECORD_FIELDS` uses) and reads the other fields base-relative; its test
seeds a decoy at the base-relative address.

`SCENARIOS['gp-u13-band-dac']` is `gp-u13-ending2`'s script with its last
`until_mode 3` replaced by `('after_entry', 0x15, 1, 82, ('poke',
BAND_DAC_WRITES))` and `('after_entry', 0x15, 1, 84, ('end',))`; time_limit 240
and memsize 64 are U13's, and the scenario joins `STOP_AT_END`.

**The mode-0x15 entry is run-dependent by one frame.** The published captures
of this route disagree: `gp-u13-ending2`'s poll.log has its first mode-0x15 S
at **f=0x17D4** (its S at 0x17D3 is mode 0x17), while `gp-u13-band`'s has it at
**f=0x17D5** (its S at 0x17D4 is mode 0x17) — the two runs' boot S sequences
are offset by one frame (first S 0x5 vs 0x4; every later mode entry shifts the
same way). `after_entry` is relative, so the schedule must be read relative to
the entry, which is also the fade's own clock:

| step | target F | W rows' spin (F-1) | presented / stopped |
|---|---|---|---|
| `after_entry 0x15 1 82` | entry+82 | entry+81 | entry+82 (the band's phase frame) |
| `after_entry 0x15 1 84` | entry+84 | — | entry+84 (two frames later) |

for the two measured entries that is (0x1825/0x1826/0x1828) under 0x17D4 and
(0x1826/0x1827/0x1829) under 0x17D5. The new capture is expected to be the
latter (gp-u13-band's fresh run of the same route): W rows at f=0x1826, the
sentinel's S record at 0x1827, the stop at 0x1829. `gp_band.py --dac` therefore
defaults its S frame to the **diagnostic W rows' f + 1** (the iteration that
runs the forced body and presents it), so neither run's absolute frame is baked
into the analysis; `--f` overrides it. The poller logs the S record before
applying the poke, so the S at the W rows' f is the pre-poke snapshot and the
S at W.f+1 is the post-poke one; the sentinel is presented at the latter.
No pin, oracle, manifest entry, `CAPTURE_DIRS` member or audio trace hangs off
it (a diagnostic, like `gp-u13-band`); a failed run is preserved as `.failed`.

**The capture (it ran as the one authorised run; the analysis and decision are
§F2.5).**

```
$ make gp-capture scenario=gp-u13-band-dac
```

Expected: a new `data/k11-captures/gp-u13-band-dac` whose poll.log's six
diagnostic W rows are at f=0x1826 (the entry-0x17D5 run; all `race=0` and
`late=0`; the log also holds the route's 17 win-path W rows, which the analysis
filters out by address), whose S record at f=0x1827 carries the fields above
with `effn=12` and `bbc=0x65`, and whose frames around the band's phase frame
show the four sentinel colours. The controller runs it after Gate-0 and resumes
this task to apply `tools/gp_band.py --dac` (the tool derives f=0x1827 from the
W rows):

```
$ python3 tools/gp_band.py --dac --poll-log data/k11-captures/gp-u13-band-dac/poll.log \
    --capture data/k11-captures/gp-u13-band-dac \
    --port-frame /tmp/opencode/t2-gp-dump/gp-u13-ending2/frame_04316.ipx
```

The outcomes are the design's (§3.4): sentinel at the eight `0x32` pixels with
the back buffer `0x32` -> **PASS-A** (capture/pipeline present model; the band
allowance stays and the class is declared); no sentinel with the back buffer
`0x32` -> the **present-layer** class (undumped present/aperture residue,
terminal with the S record); a back-buffer `Q` at a band offset -> **PASS-B**
(content divergence; the record names the renderer from Q and re-pins only if a
fix is measured); no S at the W frame+1, a race or late write, or a control
that does not turn yellow -> **INCONCLUSIVE** (keep `(5120,1,5,56b900b1…)`,
no second capture). The tool's tests cover the four verdicts and the whole
`--dac` path (the six-address filter with the route's W rows present, the S
frame derived from them, raced/late/missing writes) on synthetic sides.

### F2.4 Tests

* `tools/tests/test_gp_band.py`: the holder search (whole band, a one-pixel
  drop, a one-channel difference, the frame's own DAC, the control apart), the
  four `decide_dac` outcomes, the six derived writes, the scenario/field shape
  and the entry-relative schedule (both measured entries 0x17D4/0x17D5, and
  the published poll.logs' first-mode-0x15 frame pinned), and the whole
  `--dac` path on a synthetic poll.log that carries a prior U13 win-path W row
  (the round-1 Critical: the six-address filter and `race`/`late` rejection).
  Mutation-checked while writing: swapping the band comparison for a single
  channel or dropping the control changes the expected counts; a whole-log W
  count would fail the `--dac` test.
* `tools/tests/test_gp_capture.py`: `read_snap` follows the `bbp0` pointer,
  with a decoy at the base-relative address and the next buffer as a second
  pointer; the S record round-trips the fields. `TestMemsize` adds the new
  scenario to the `memsize=64` set.
* `tools/tests/test_gp_session.py` (unchanged) still passes; the new module is
  added to `make verify`'s k11/gp tool unit-test line (the Makefile edit), so
  the derivation's six addresses and four colours stay gated.
* No C code, no differential row, no oracle and no pin changed.

### F2.5 The capture and the decision (PASS-A)

The one authorised diagnostic capture is `data/k11-captures/gp-u13-band-dac`
(scenario `gp-u13-band-dac`; exe sha `8120f1bd1df389ed94cb329c030f95d9e38caad193bbd9840717af557161a68d`,
`memsize=64`): **5,177** collapsed frames (raw AVI 1408..8946, fps 70.0866),
session checks 24/24 steps, 23/23 pokes (0 raced), end reached, no unscripted
input, stopped at f=0x1864 (SIGTERM, rc 0), wall 128.6 s. No second capture.

**The frames.** This run's mode-0x15 entry is **0x17D4** (the older-style
entry, not gp-u13-band's 0x17D5), so the diagnostic W rows are at f=0x1825 and
the forced case-4 body presents at f=0x1826 — the analysis's derived
`W.f+1` frame. The sentinel presentation is **capture frame 05130**
(`window.txt` raw 8859; AVI 126.401 s against the S record's
`f=0x1826 ms=126694`, the ~0.29 s poller/AVI offset every earlier comparison
has; it is the capture's ONLY frame with the exact sentinel set, so the
identification is by the pixels, not the clock). Frame 05129 (raw 8858)
precedes it with `000000`×8 / `100000` / `00000c`×4 at the 13 offsets (the
port's f=0x1825/0x1826 band colours); 05132/05133 show `ef00ef`/`00efef`/
`efefef` and 05135 `e700e7`/`00e7e7`/`e7e7e7` — the sentinel darkening by 8 per
natural body in the sequence `0xF7 -> 0xEF -> 0xE7`, matching the port dry
run's values (the exact game-frame phase of those bodies is not claimed).

**The S record at f=0x1826** (`bbp0=00084410`, `bbp1=002FB038`, `effn=11`):

```
bb00..bb12 = 32 32 32 32 32 32 32 2E 32 4D 4D 4D 4D      (the port's indices)
bbc        = 03                                         (the port's 0x65)
eff0=002DE42C eff1=002D7B28 efffree=002D6B00
```

The effect links are the original's *relocated* data-object addresses: with the
data object at base 0x266000, subtract 0x266000 and add `DATA_BASE_VA` 0x80000
gives **0xF842C** (active head), **0xF1B28** (active tail = the entry-2 record
the W rows target) and **0xF0B00** (free head) — the port's own measured values
(F2.2). The capture therefore confirms the **entry-2** record's mapping in the
original (the active tail whose body uploaded the band sentinels), not only in
the port's model; the entry-5 record and the 0x65 colour were **not observed**
— the control's own index is 0x03, so the 0x65 sentinel has no pixel to show.
`effn=11` (one fewer than the port's pre-injection 12; one record had retired by
this frame — the entry-2 record is still the active tail and its body uploaded
the band sentinels, as the pixels show).

**The corrected control index.** The original's back buffer at (1,100) holds
**index 0x03**, not the port's 0x65. The design's control premise was the
port's index (and the old capture's control *colour* did agree: both indices
render black — port `DAC[0x03] = DAC[0x65] = 000000`, and the measured control
pixel is `000000`; at frame 05128 it is `040000`, the fade of index 0x03). The
0x65 sentinel therefore cannot appear at the control; the band sentinels are
the injection proof. The checker's control rule is corrected accordingly
(F2.6): a control whose own index was not injected is reported, not gated; an
injected control that does not turn is still INCONCLUSIVE.

**The displayed band.** Frame 05130 at the 13 offsets:

```
(1,23) f700f7  (1,24) f700f7  (1,295) f700f7  (1,296) f700f7
(2,22) f700f7  (2,297) f700f7
(3,21) f700f7  (3,74) 00f7f7  (3,298) f700f7
(4,27) f7f7f7  (4,28) f7f7f7  (4,291) f7f7f7  (4,292) f7f7f7
control (1,100) 000000
```

i.e. every one of the 13 pixels shows the sentinel for its own back-buffer
index (0x32 magenta, 0x2E cyan, 0x4D white) — not the old capture's blue
`00001c` and not a fade colour. Corroboration: the frame holds **101** magenta
pixels, exactly the port frame 4316's 101 index-0x32 pixels — the **0x32 set
matches 101/101**; the frame-wide 0x2E/0x4D counts (74/27 vs the port's 96/54)
differ at some lower-row pixels outside the 13, which are not part of the
claim. No frame of this capture holds `00001c` at the eight band
offsets (the old capture's blue band is absent in this run — a run-specific
scan-out state, consistent with the class below).

**Decision: PASS-A.** The band's index state is the port's (the
pre-injection back buffer at f=0x1826 is `0x32`×8 / `0x2E` / `0x4D`×4, the
port frame 4316's indices), and the displayed band pixels follow the game's own
DAC (they are the injected sentinels). The old capture 5120's `00001c` band is
therefore **not a content divergence**: it is a capture/present-side effect of
the scan-out's palette state, the class the U13 allowance comment already names
(a present state the driver never dumped; the scan-out catching a palette the
dumped presents do not hold). The specific palette/scan-out state that produced
`00001c` in that one 70.09 Hz frame is still not named — the diagnostic proves
the content and that the present follows the DAC, not the exact transient. The
band allowance `(5120, 1, 5, 56b900b1…)` is **unchanged**; no pin, oracle,
capture-manifest entry or `CAPTURE_DIRS` membership was added or moved, and the
diagnostic capture stays outside the gate inventory (a diagnostic, like
`gp-u13-band`).

### F2.6 The checker correction and its tests

The measured 0x03 control forced a minimal, test-covered change (no pin moved):

* `gp_band.decide_dac`: the control is a gate only when **its own** back-buffer
  index is one of the four injected indices; a `None` sentinel there (index
  0x03) is reported, not gated. An injected control that does not turn is still
  INCONCLUSIVE. The back-buffer / port-index divergence is checked **first**
  (PASS-B), before the sentinel requirement: a band `Q` outside the four has no
  sentinel, so the old order would have hidden PASS-B behind INCONCLUSIVE.
* `gp_band.dac_main`: the expected sentinel at each offset comes from the
  **back buffer's** index (the game's), not the port's, and the sentinel frames
  are located by any injected offset (the band offsets included) — a
  control-only locator would have found zero frames for this capture.
  `bbc == 0x65` is no longer part of the injection gate; the six-address W-row
  filter and the `race`/`late` checks stay.
* Tests: `TestDecideDac.test_a_non_injected_control_is_not_a_gate` (the 0x03
  case, PASS-A) replaces the old None-control INCONCLUSIVE test (the injected
  control that does not turn stays INCONCLUSIVE);
  `TestDacMain.test_dac_main_accepts_a_non_injected_control` runs the whole
  `--dac` path with `bbc=0x03` and a black control over the synthetic
  sentinel-band frame; `test_dac_main_pass_b_for_a_band_backbuffer_outside_the_four`
  runs the whole path with a band `Q = 0x15` (no sentinel at that offset) and
  pins PASS-B; `test_dac_main_absent_band_sentinels_are_inconclusive` pins the
  no-frame INCONCLUSIVE. `python3 -m unittest tools.tests.test_gp_band` (26
  tests) and `tools.tests.test_gp_session` pass; the other `gp_band` tests are
  unchanged.

## §F3 U14: the rule split, the two-human fought win, and R1 terminal (Task 3)

**Decision: the `b0c` clause is the final's, not the match's; the two-human
fought match win is captured as game evidence but failed its session check and
publishes nothing; U14's fought arcade win (R1) is terminal.** No port code
changed. The one authorized `gp-u14-twop-win` run reached the game's own 2-0
two-human win (`b1f=3`, both rounds live-to-`0x78`, no `W`, mode `0x12`) but
failed `port script v2` at `f=0xB1B` (a missed `S(0xB1A)`); it is preserved as
`data/k11-captures/gp-u14-twop-win.failed`, with no oracle, pin, miss set,
manifest entry or `CAPTURE_DIRS` path.

### F3.1 The raw correction: `b0c` is the final's `0x0C -> 0x0D` chain

`DS_00104B0C` has one non-zero writer: `0x37FF6` in `0x37EA0`
(`fighter_37ea0`, the `0xD100` opcode `0x11` target of the seven characters'
streams; `port/src/game/fighter.c:14391-14439`). Its only consumer is
`game_mode_0d_step` (`0x274FC`, `port/src/game/flow.c:2026-2067`), which returns
while the byte is 0 and otherwise clears it and increments `DS_00104B21`; the
seventh increment enters mode `0x0F` (`0x27590`). Mode `0x0D` is entered from
the **final's** seven-opponent arena (`flow_arena_ko_check` `0x272DC` via
`game_mode_0c_step` `0x27380`, `port/src/game/flow.c:1802-1886`;
`port/spec/game_flow.md:601-604`), never from match 1's win, which runs
`0x09 -> 0x17 -> 0x12`. The earlier wording "the ending path" is therefore a
plan-vs-raw correction: the byte's chain is the final's `0x0C -> 0x0D` one, not
an ending path (the game-set `b0c` is U10's, in mode `0x0F`, and its seven KOs
are poked). A match-1 win never *consumes* `b0c`: mode `0x17`'s setup clears it
(`0x271E0`'s `0x27200`), the match result never enters `0x0D`, and the measured
artifact holds `b0c=0` throughout. The old `live_death` rule required `b0c=1`
at/after the match-1 mode-9 row, i.e. the final-path signal on a path that
cannot produce it; it was unsatisfiable as a match-1 claim. Measured
corroboration: `gp-u14-win-live.failed` has `b0c=0` in all 9,471 S
rows and no mode `0x0C/0x0D/0x0F` (record `2026-10-26-fought-match-truth` §F2.6:
the match result is mode 9 at `f=0xF9F`, then `0x17/0x15/0x13/0x1E/0x03`).

### F3.2 The rule split (`tools/gp_win.py`)

`LIVE_WIN`/`live_death` are gone. Two rules replace them:

* **The match rule** (`MATCH_WIN`, `live_match(name, frames, lines, snaps)`):
  no `W` record may cover a KO/death field (`gp_session.KO_POKES`, the one
  tuple shared with `gp_twop.KO_POKES`: the two `+0x5A` damage bytes,
  `w2`/`w3`, `ad4`, `b0c` and the seven land marks); the match result row
  (mode 9, k 1) must exist and be a **2-0 win** for `MATCH_WINNER[name]`
  (`ad4 = winner`, `w[winner] = 2`, `w[loser] = 0`, the loser's
  `+0x5A = 0x78`, the winner's `< 0x78`; a 2-1 match is rejected); no mode-7
  time-up record may precede it; and the mode-6 entry immediately before it
  must hold a live `0 < loser +0x5A < 0x78` value (the damage path adds to a
  live value, so a `0 -> 0x78` jump is a poke shape). **No `b0c` clause.**
  `gp-u14-win-live` is its only member, with winner side 0. The rule reads the
  result/counter/damage rows, not the input's `b1f`: the **two-human** gate is
  `gp_twop.match_win`'s (it runs `gp_twop.ko`'s `b1f = 3` check first).
* **The final-only rule** (`FINAL_WIN`, `live_final(name, frames, lines,
  snaps)`): the same no-poke check, and a `b0c = 1` S record at or after the
  mode-`0x0F` entry (the final path). `FINAL_WIN` is empty: no fought final is
  captured; the rule is pinned synthetically until one is. The U10 game-set
  `b0c` (`f=0x1602`, 190 frames after the `0x0F` entry `f=0x1544`) is a poked-
  final write (all seven final KOs are poked, record §E1.3) and is **not** a
  fought-final exhibit.

`evidence()` dispatches on the set membership. The match rule also checks the
round result row (mode 8, k 1): `ad4 = NO_RESULT`, the winner's counter `>= 1`
and above the loser's (the milestone row's `_MATCH1` predicate carries the same
`s1_5a=0x78, w2=1, w3=0`), and then anchors the win on the decisive match
result.

### F3.3 The rule tests (RED then GREEN)

`tools/tests/test_gp_win.py`'s `TestLiveWin` became `TestLiveMatch` (twelve
tests) and `TestLiveFinal` (three). The synthetic match fixture is a fought 2-0
path: mode-6 entries with `s1_5a=0x10`, the mode-8 result `s1_5a=0x78`,
`w2=1`, `ad4=0xFFFFFFFF`, the second mode-6 entry with `s1_5a=0x10`, and the
mode-9 result `s1_5a=0x78`, `w2=2`, `w3=0`, `ad4=0`; **b0c is 0 throughout**.
The tests pin: the match rule passes a fought `w2/w3` win with no `b0c` (the
two-human gate is `gp_twop.match_win`); the U9 fixture (all milestones, poked
KO shape) fails the live-value rule; a match result naming side 1 (`ad4=1`,
`w2=0`, `w3=2`, the artifact's shape) fails; a 1-0 result fails; a 2-1 match
fails the 2-0 condition; a round result naming side 1 fails even when the
match result is a 2-0; a mode-7 record before the result fails; a KO poke and
a `DEATH_DONE` poke fail; the final rule requires `b0c=1` at/after the
mode-`0x0F` entry, a real pre-entry segment's `b0c=1` does not count (seeded at
`f=0xF0..0xFF` before the `0x100` entry), and a `DEATH_DONE` poke fails. The
artifact test loads the git-ignored `.failed` artifact: `gp_twop.ko(arcade=
True, winner=1)` passes (its KO/result clauses), while `live_match` for side 0
fails at the match result (`f=0xF9F` names side 1, `w3 = 2 > w2 = 0`).

RED (before the split): `python3 -m unittest tools.tests.test_gp_win -v` ran
59 tests, `FAILED (errors=12)` — every `live_match`/`live_final` call was an
`AttributeError` (the two old `live_death` calls and the absent final rule
included). GREEN after the split and the fix round (round-result, 2-1, pre-entry and
shared-tuple tests added): `Ran 62 tests ... OK`.

### F3.4 The checker (`tools/gp_twop.py check --win`)

`match_win(lines, winner=0)` extends `ko()` with the match claims:

1. the `ko` predicate (two-human `b1f=3`, no KO/death poke, the first live KO,
   no time-out at its result, the round result naming the winner);
2. an **X record** (the scenario end was reached; the capture-time session
   checks, not this checker, decide whether it is a published capture — the
   two-human `.failed` artifact is X-terminated and its game rows pass);
3. no mode-7 record anywhere before the X (a time-up round is not a fought
   win);
4. the round result (mode 8) at `1-0` with `ad4=NO_RESULT` and the loser at
   `0x78`;
5. the match result (mode 9) at **2-0** for the winner (`ad4=winner`,
   `w[winner]=2`, `w[loser]=0`), the loser at `0x78`, the winner below it;
6. **both rounds'** loser `+0x5A` transitions to `0x78` from a live
   `0 < value < 0x78` in a mode-6 entry (two transitions, one per round), and
   the winner's `+0x5A` never reaches `0x78`.

A one-round win has no match result and fails; the arcade `.failed` artifact
fails the two-human clause (`b1f never reaches 3`). The CLI has `--win` in the
same mutually exclusive group as `--join`/`--finisher`/`--ko`; `--winner`
applies to `--ko` or `--win`, `--arcade` stays `--ko`-only.

Tests (`tools/tests/test_gp_twop.py`, `TestMatchWin`, 13 tests): a two-round
2-0 pass; a one-round win fails; a mode-7 fails; a KO poke and a `DEATH_DONE`
poke fail; a 1-0/0-2/2-1 match result fails; a winner-side KO fails; a missing
X fails; a poked second KO fails the live rule; the `.failed` artifact fails
the two-human clause; the CLI passes/rejects and `--win --ko` is an argparse
conflict. RED before `match_win`: `Ran 67 tests`, `FAILED (errors=12,
failures=1)`; GREEN: `Ran 67 tests ... OK`. The wider focused set
(`test_gp_twop test_gp_win test_gp_session test_gp_capture test_gp_compare`)
ran 366 tests, `OK`.

### F3.5 The one capture: `gp-u14-twop-win`

The scenario is `gp_session.SCENARIOS['gp-u14-twop-win']` (added; also in
`STOP_AT_END`): gp-twop-ko's start and 140-attempt 1A/1B round, then the same
140-attempt round anchored to the second mode-6 entry, `extra=WIN_EXTRA`,
`skip_on_mode=(0x07,0x08,0x09)`, `stop_mode=0x12`, `stop_delay=0`,
`time_limit=240`. No poke; the route is the two-human one (P2 idle). The one
authorized run (`make gp-capture scenario=gp-u14-twop-win`, 2026-10-30) ran
118.8 s and logged 5,820 S records, f 5..0x16CF, 15 snapshots missed by the
capture model, 5,074 frames. Ten of the eleven session checks passed; one
failed:

```
CHECK steps fired 194/323 (129 skipped); stopped on mode 0x12; last skip on mode 0x09: ok
CHECK end frame reached: ok
CHECK port script v2 (pad change at f=B1B unpinned (no S record at f=B1A)): FAIL
CHECK no unscripted input: ok     CHECK pokes written 0/0, 0 raced: ok
CHECK capture pins: 0 BIOS words skipped (backlog over the 14-entry cap): ok
CHECK stopped at the end (SIGTERM at f=16CF, rc=0): ok
```

The game's own verdict (S rows, measured):

| row | frame | mode | `s0_5a` | `s1_5a` | `w2` | `w3` | `ad4` | `b0c` |
|---|---|---|---|---|---|---|---|---|
| round 1, mode-6 last sample | `0xAD2` | 6 | 0 | `0x73` | 0 | 0 | `FFFFFFFF` | 0 |
| round 1 KO (round result) | `0x0B1B` | 8 | 0 | **`0x78`** | **1** | 0 | `FFFFFFFF` | 0 |
| round 2, mode-6 last sample | `0x1375` | 6 | 0 | `0x73` | 1 | 0 | `FFFFFFFF` | 0 |
| round 2 KO (match result) | `0x13A8` | 9 | 0 | **`0x78`** | **2** | **0** | **0** | 0 |
| conquered lands | `0x1693` | `0x12` | 0 | `0x78` | 2 | 0 | 0 | 0 |

`b1f=3` from the join (`f=0x2CA`) to the X; zero `W` records; P1's `s0_5a=0`
throughout. The KO and the result share the iteration (record §F3.2 of the
2026-10-26 record): the mode-6 samples hold the live `0x73`, the result records
the `0x78`. `gp_twop.py check --win --capture
data/k11-captures/gp-u14-twop-win.failed` prints `ko f=B1B; round result
f=B1B; match result f=13A8; death-done f=-; X f=1693 ... match win: ok`
(exit 0) — **game evidence only**, not a capture pass (the session check
failed). The `.failed` artifact's `poll.log` sha256 is
`a96ca6f5de662159f43d61ae939c65be64d01dbed5441b324a12e8bfcfe728d9`, 5,074
frames.

### F3.6 The failure branch (no publication)

Per the brief and the capture discipline, the run failed a session check, so:
`data/k11-captures/gp-u14-twop-win.failed` is preserved; the requested
`gp-u14-twop-win` path is absent; there is no `gp-u14-twop-win-oracle`, no
`GP_U14_TWOP_WIN_*` pin, no `k_miss_gp_u14_twop_win`, no manifest entry and no
`CAPTURE_DIRS` line. The scenario stays in `gp_session.py` (the route and its
session check are the measured evidence; a future phase may retry it), but
this phase's one capture is spent. `make gate-check` reads **41/42 present, 1
declared gap** (`data/k11-captures/gp-u14-win-live`), and
`capture_manifest.py --check-pins` reads the unchanged baseline
`cadd4ec68740a7839847bc4ed1375af4a8f970564bc06181789fb49249308f88`. No pin
moved.

### F3.7 R1 terminal: the arcade fought win

**The arcade fought win is terminal.** The damage race in
`gp-u14-win-live.failed` (a one-human arcade match, P1 = SAURON `c0=0`,
P2 = VERTIGO `c1=3`, `afc=3`) is measured from the S records' damage
increments:

| round | side | increments | damage at the result | source |
|---|---|---|---|---|
| 1 | P1 (SAURON, script) | 2 | `0x19` = 25 | `f=0x73F` `0x12`, `f=0x77C` `0x19` |
| 1 | P2 AI | 10 | `0x71` = 113 + the result's KO 7 = **120** | `f=0x470..0x86C` |
| 2 | P1 (SAURON, script) | 8 | `0x5C` = 92 | `f=0xBC1..0xE85` |
| 2 | P2 AI | 8 | `0x63` = 99 + the match result's KO 21 = **120** | `f=0xB85..0xEE5` |

The plan's summary (round 1 AI 10 hits/120 vs script 2/25; round 2 AI 9/120 vs
script 8/92) is the same measurement with the KO increments counted in the
damage totals (round 2's 8 mode-6 increments plus the result's KO = 9). The
scripted P1 attacks are the U16 builder's 1A/1B (`U14_PUNCH`/`U14_KICK`,
reactions `0x10`/`0x11`); the `r0`/`r1` latch changes are recorded in the S
rows, but the landed damage is the increments above. P1 is SAURON (`c0=0`), so
its own `r0` latch is the 1A/1B reaction id it applied (0x10 at `f=0x468`,
`0x544`, `0x598`, `0x5D0`, `0x844`; 0x11 at `f=0x728`, `0x781`, `0x888` in
round 1) and `r1` is the VERTIGO AI's applied reaction; only the damage
increments say which applications landed. The Sauron reaction data therefore
agrees with the plan's reading: the script's input is delivered (the latch
changes), but the arcade AI's damage race is what decides the round. Of the
three historical attempts, no won arcade round is measured: gameplay truth
II's first left no artifact (a command timeout), its second ran 9,761
snapshots and never reached its X or the win (record
`2026-10-20-gameplay-truth-ii` §J6), and the fought-match-truth phase's one
authorized attempt lost 0-2 (P1 KO'd in both rounds, `b0c=0`, mode `0x12`
never opened; artifact `.failed`, four session failures: end frame, port
script, unscripted input, stopped at the end; record `2026-10-26-fought-match-
truth` §F2/§L8). The AI's damage path is the measured blocker: the script
landed 2/25 and 8/92 while the AI landed 120 in each round, and the arcade AI's
attack rate is not something the port's fixed script controls.

The `b0c` final chain does not rescue R1: `0x37FF6` writes it from a death
animation, but its only consumer is the final's `0x0D` (the seventh arena KO),
the U10 game-set byte is on the poked final (all seven final KOs poked, record
§E1.3), and a match-1 arcade win never enters `0x0D` (mode `0x17`'s setup
clears the byte). The two-human fought win above is **not** an arcade win: it is a
second human's idle slot against the script, it does not exercise the arcade
AI, and its capture failed its session check. **R1 stays declared**; the
match rule is correct and closed, but no arcade fought-win capture is claimed.

### F3.8 The commands and the tree

* `python3 -m unittest tools.tests.test_gp_win` — 62 tests, OK.
* `python3 -m unittest tools.tests.test_gp_twop` — 68 tests, OK.
* `python3 -m unittest tools.tests.test_gp_twop tools.tests.test_gp_win
  tools.tests.test_gp_session tools.tests.test_gp_capture
  tools.tests.test_gp_compare` — 366 tests, OK.
* `python3 tools/gp_twop.py check --win --capture
  data/k11-captures/gp-u14-twop-win.failed` — game evidence, ok (not a
  capture pass).
* `python3 tools/gp_twop.py check --ko --arcade --winner 1 --capture
  data/k11-captures/gp-u14-win-live.failed` — ok (record §L8's game evidence).
* `make gate-check` — 41/42 present, the declared U14 gap; exit 0.
* `python3 tools/capture_manifest.py --check-pins` — baseline `cadd4ec6…`
  unchanged.
* No `make verify` (the task's scope); no port code, no game byte, no
  existing pin, no manifest entry and no `CAPTURE_DIRS` line changed.
