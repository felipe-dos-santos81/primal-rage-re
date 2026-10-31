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

## §F4 The taken-continue branch: `0x0E` -> `0x2791C` (Task 4)

**Decision: `gp-u29-continue-taken` is published and
`gp-u29-continue-taken-oracle` is in `make verify`.** One guarded capture
passed every session check and the new `tools/gp_continue.py check --taken`.
The claim is that the credit-ready PRESS START prompt is **taken**: the game's
own `flow_continue_poll` spends one credit, `b1f` becomes 1, `flow_continue_take`
runs, and the round restarts in mode `0x0C`. The route's four harness writes
(the two match-1 KOs and their lands byte, and the incumbent KO that opens the
screen) are labelled: neither a fought KO nor a match win is claimed.

### F4.1 The route, the press and the raw take

`gp-u29-continue-taken` clones `gp-u28-continue`'s step tuple (U10's LEFT
PLAYER ARCADE through the two match-1 KOs, then
`('after_entry', 0x0C, 1, 10, KO_P1)`, the labelled `0x10780A = 0x78` poke that
makes `flow_arena_ko_check` (`0x272DC`) open the screen through `flow_continue_open`
(`0x278B0`)). It then adds one `p1.start` and a frame-keyed end
(`gp_session.py`):

```
CONTINUE_TAKEN_PRESS = ('after_entry', 0x0E, 1, 0xA0, ('pad', ('p1.start',), 6))
steps = gp-u28-continue's + (CONTINUE_TAKEN_PRESS, ('after_entry', 0x0E, 1, 0x100, ('end',)))
```

The press frame is derived, not tuned. `0x27B59`'s `cmp eax,0xE; jge` is the
prompt arm: the PRESS START blink (`0x2C0F4`) is drawn only while the signed
count `DS_00108110` is below `0x0E`. The screen opens at `f=0x14C0` with count
`0x0F` (raw `0x278C2`) and the count decrements on the frames whose
`DS_000EF6DC & 0x3F` is 0 (`0x27ACD..0x27AED`), so the prompt's first frame is
`cnt=0x0D` at `f=0x1540`. The `after_entry` target is `F = 0x14C0 + 0xA0 =
0x1560`: the injector writes the key-state byte in the spin of `f=0x155F`,
iteration `0x1560` samples the key down (the capture's `S(0x1560)` has
`raw=0x01000000`, `kb=0x0100`), and `0x500C4`'s one-frame debounce makes
`0x4F644` report the newly pressed bit at `0x1561`, where the take is taken.
`p1.start` is the F1 key slot (scan `0x3B`): its P1 byte bit 0 is the start mask
`0x9ACBC[0] = 0x01000000` (the data table's first dword; the second is P2's
`0x00000100`, read from `ghidra_data.bin` at DS+0x1ACBC). The 6-frame hold is
`gp-u5-charsel`'s and the 240 s limit/game memory are `gp-u28-continue`'s.

The take is `0x42F60` -> `0x2791C` (port `flow.c` headers `0x42F60`, `0x2791C`,
`0x27A2C`):

* `config_credit_ready` (`0x2C060`) is true while free play `0x105D60` or the
  credit dword `0x105C00` is non-zero; `config_credit_zero` (`0x2CA78`) is
  `xor eax,eax; ret`, so the spend is always `config_credit_spend(1)`
  (`0x2CA7C`). Its `0x2CA9C` debit runs only while `DS_00104B1F` is 0 - and
  `0x27A40` stores `b1f` **after** the poll, so the take debits `4 -> 3`.
* `0x27A40` stores `b1f = (DS_0010810D != 0) + 1`; the incumbent is side 0
  (`w10d=0`), so `b1f=1`.
* `0x2791C` releases string `0x41`'s cells at row `0xA`, the PRESS START prompt
  at row `0xC`, and `0x1C` cells at rows `0xE`/`0xF`; restores both
  `0x27254` snapshots through `0x33B00`; calls `0x1D764` on the incumbent
  (which clears its `+0x5A` to 0, `0x1D782`); `0x46534(DS_00104B12, -2)`;
  stores mode `0x0C` (`0x27A15`, DX) and clears the incumbent's `+0x41` bit 4.
* Mode `0x0C` persists: its handler `0x27380` (`game_mode_0c_step`) ends in
  `0x272DC` and has no `DS_00104B00` store (the decomp's `FUN_00027380`; the
  only mode-6 stores in the image are `0x25F6B`'s no-entrance round and the
  volleyball/`0x25` exits), so the restarted round runs in `0x0C` until a KO.

The frame-keyed end is `0x100` frames after the screen's entry (`f=0x15C0`), so
the take and 0x5F frames of the restarted round are inside the replayed window.
No new poke: the route's four writes are U28's and none lands at or after
`0x14C0`.

### F4.2 The one capture and acceptance

Command, run exactly once with the host idle (load 0.29 on 20 CPUs):

```
make gp-capture scenario=gp-u29-continue-taken
```

The published capture is `data/k11-captures/gp-u29-continue-taken`: **4,446
frames** (raw 1373..8052), 5,605 snapshots (`f=0x0005..0x15FC`, 19 game frames
missed by the documented poller), wall 115.7 s, and poll.log sha256
`cb0347c50cd257408b1c447c23b667991dedbf2c9ee4ce64fb9638dcc1136b9c`. All
session checks passed: base; 8/8 steps fired; end frame reached; mode `0x27`
after Enter; `kb == raw`; 4,446/4,446 frames written; port script v2; no
unscripted input; 4/4 pokes written with zero races; zero skipped BIOS words;
stopped at the end (`SIGTERM` at `f=0x15FC`, rc=0). The X record and generated
script end at `f=0x15C0` (5568); the later SIGTERM is after the harness's
`STOP_TAIL=60` poll. No `.failed` artifact and no second capture exists.

The checker and the capture's S/I/W records measure:

| Claim | Capture evidence |
|---|---|
| labelled incumbent KO | `W f=0x14BF addr=0010780A was=00 now=78`; mode `0x0E` opens at `f=0x14C0` with `w10d=0` and the incumbent `s0_5a=0x78` |
| countdown to the prompt | `cnt=0x0F` at `f=0x14C0`, `0x0E@0x1500`, `0x0D@0x1540`: the prompt arm (`cnt < 0x0E`, `0x27B59`) is on screen from `0x1540` |
| the press | `I f=0x155F step=7 press=p1.start scan=3B`; `S(0x1560) raw=01000000 kb=0100`, `S(0x1540..0x1560)` keep `cnt=0x0D` |
| the take | `S(0x1561) mode=0x0C cred=3 b1f=1 cnt=0x0D s0_5a=0` |
| the spend | `cred=4` through the screen (`0x14C0..0x1560`), `cred=3` from `0x1561` to the end: no `W` on `0x105C00` |
| the restart | 155 records of mode `0x0C` after `0x1561` (`cnt` frozen at `0x0D`, `b1f=1`, `cred=3`) to the last `S(0x15FC)` |
| the route's writes only | `W f=0x489 00108106`/`0010789E`, `W f=0x857 0010789E`, `W f=0x14BF 0010780A`, all `late=0 race=0`; nothing at or after `0x14C0` |

The route's absolute frames are `+10` against `gp-u28-continue` (`W 0x489` vs
`0x47F`, mode `0x27` at `0x13B` vs `0x131`): the U29 capture's raw window starts
14 presents earlier (1373 vs 1387), so the boot's Enter and every later frame
sit ten game frames later. The `after_entry` anchors are relative, so the route
is unchanged; the trace's zero differences through `0x15C0` are the measured
confirmation (below).

### F4.3 The checker (`tools/gp_continue.py check --taken`)

`check` (expiry) is unchanged; `--taken` selects the new branch and is accepted
only with `--scenario gp-u29-continue-taken`. It proves, from the capture's
records alone:

* the first mode-`0x0E` run has `w10d=0`, `cred=4`, `b1f=0` (the screen's
  `0x278BC` store) and the incumbent `+0x5A=0x78` in every record;
* the new countdown values are exactly `0x0F -> 0x0E -> 0x0D`; the last screen
  record is the prompt frame (`cnt=0x0D < 0x0E`);
* the take is the first record after the screen: mode `0x0C`, `cnt=0x0D`,
  `cred=3`, `b1f=1` and the incumbent `+0x5A=0` (`0x1D764`'s clear);
* at least `0x20` post-take mode-`0x0C` records with `cnt` frozen at the take's
  value and `cred=3`/`b1f=1` to the end: the `0x27A2C` countdown stopped and the
  round restarted;
* the W rule (plan ruling R-P10): exactly one `0x10780A=0x78` write **before**
  the screen opens; no write to the credit dword `0x105C00..+3`; no `W` at or
  after the screen's open (the take is not poke-induced); every `W` address is
  one of the route's three (`0x108106`, `0x10789E`, `0x10780A`) and each
  address's write count is the route's (`1`, `2`, `1`), so a new poke and a
  duplicate write both fail the check.

The prompt/countdown **cells' pixels** are not state fields: `0x2F388`/
`0x2C088` clear the text cells, and the capture's `cnt` freezing is the
checker's witness that the handler stopped. The cleared-screen pixels are the
frame oracle's claim (`gp_compare`), exactly as §L5 said of the prompt's pixels.

`path --taken` reproduces the capture's four milestones on the fields
`gp_trace_line` carries (`mode`, `w10d`, `cnt`, `cred`, `b1f`, `b25`):
`f=0x14C0/0x1500/0x1540` (mode `0x0E`, `cnt 0x0F/0x0E/0x0D`) and `f=0x1561`
(mode `0x0C`, `cnt=0x0D`), 4/4 on the port's replay.

The tests (`tools/tests/test_gp_continue.py`, 28 tests) reject the expiry-only
U28 shape and a synthetic no-take through `check --taken`, and each distinct
rule has its own failing case: a credit write, a poke at/after the screen, an
unknown route write, a missing incumbent poke, an uncleared `+0x5A`, a take
holding `cred=4` or `b1f=0`, a take at `cnt=0x0F` (no prompt frame), a short
post-take window and a reopened screen; the taken path fails on a no-take
capture and on a port `b1f` difference. The U28 evidence/path tests and their
messages are unchanged. The real expiry capture is rejected by the same check
(`gp_continue.py check --taken --scenario gp-u29-continue-taken --capture
data/k11-captures/gp-u28-continue`: `f=18C2 has mode 15 after the 0x0E screen,
not the 0x2791C take's mode 0x0C`).

### F4.4 Replay, frame measurement and pins

The replay reaches the script end and records its own miss set,
`k_miss_gp_u29_continue_taken` (`port/tests/test_platform.c`): `0x29D60`
(bare `ret`, hits=1) and `0x5D812` (runtime stub, hits=3), both from
`frontend_mode_1b_step`; the global `0x5D812 actor_spawn`/`set_dead` rows are
the fight's, as `gp-u28-continue`'s. The dump has **3,650 presented frames**
(0..3649); trace and moves agree through the end (`f=0x15C0 = 5568`), 5,236
frames compared with 19 capture frames lacking a snapshot, zero differences:
`GP_U29_TRACE_MIN_FIRST` and `GP_U29_MOVES_MIN_FIRST` are 5569.

Frame measurement:

* window start **100** (raw 1737);
* the first unexplained frame is capture **4380** (raw 7984), nearest port
  3649, rows 128..199 - inside the harness's `STOP_TAIL` past the X
  (`f=0x15C0`), so the whole prompt/countdown/take and the post-take round are
  below it;
* reach **3649** (the dump's last presented frame), **12** canonical non-black
  missing classes: `[9, 11, 92, 129, 132, 219, 220, 809, 1019, 1083, 1926,
  2008]`;
* capture **346** (raw 2823), the round-1 mode-8 entry's two-segment scan-out,
  is one `SCAN3_ALLOWED` name: rows 0..1 are port 218's (the only earlier
  holder of those bytes) and rows 2..199 port 221's, bound through the vacuous
  middle pair (220, 221); ports 219/220 are unexhibited. The top-2-row bytes
  are U28's, so the recipe's sha is the same
  `2d6fb6ad829baae61c65a844fb44f05cb51393322afdce4ab8117960e1306ddd`.

Measured pins:

| Pin | Value |
|---|---:|
| `GP_U29_MIN_FIRST` | 4380 |
| `GP_U29_TRACE_MIN_FIRST` / `GP_U29_MOVES_MIN_FIRST` | 5569 / 5569 |
| `GP_U29_MAX_START` | 100 |
| `GP_U29_MIN_REACH` | 3649 |
| `GP_U29_MAX_MISSING` | 12 |
| `GP_U29_CAPTURE_FRAMES` | 4446 |
| `GP_U29_CAPTURE_SHA256` | `cb0347c50cd257408b1c447c23b667991dedbf2c9ee4ce64fb9638dcc1136b9c` |

Every pin was moved one step in its failing direction and restored:

| Mutation | Observed failure |
|---|---|
| `MIN_FIRST 4380 -> 4381` | `first unexplained 4380 < ratchet N 4381` |
| `TRACE_MIN_FIRST 5569 -> 5570` | `N 5570 > end 5569: N is unreachable` |
| `MOVES_MIN_FIRST 5569 -> 5570` | the same unreachable-N failure on moves |
| `MAX_START 100 -> 99` | `window starts at capture 100 (raw 1737) > pinned start 99` |
| `MIN_REACH 3649 -> 3648` | `window reach port 3649 > pinned reach 3648; re-pin to 3649` |
| `MAX_MISSING 12 -> 13` with `PR_PINS_EXACT=1` | `12 missing class(es) < pinned 13; re-pin to 12` |
| capture SHA final digit `9 -> d` | capture identity mismatch (poll.log sha) |
| `CAPTURE_FRAMES 4446 -> 4445` | capture identity mismatch (4,446 actual frames) |
| scan-out top hash last digit `d -> e` | `allowed capture 346 (raw 2823) is not its named three-frame scan-out` and `FIRST UNEXPLAINED capture 346` |

`tools/tests/test_gp_compare.py` gains the two-segment recipe's fixture case
(`test_u29_two_segment_recipe_rejects_one_mutated_bound_byte`): the tuple is
pinned, the fixture explains the frame and one mutated bound byte of the bottom
holder returns `no unique adjacent port pair`. The 94 `test_gp_compare` tests
pass; the real dump's hash mutation above is the manual proof on the capture.

### F4.5 Coverage, manifest and verification

The capture is in `CAPTURE_DIRS`, so `make coverage`'s
`GP_COV_SCENARIOS = $(filter-out gp-pads gp-u17-device2,$(notdir ...))` includes
it automatically: the coverage driver is extended, and the census was re-pinned
from the measured hit dump. The run's split is **HIT 659 / NOT-HIT 96 /
UNINSTRUMENTED 4** (was 657/98/4). The task's expectation was `0x2791C` alone
(98 -> 97); the measured union improves **two** rows, and the second is the
take's own callee: `0x2791C` (flow_continue_take) and `0x33B00`
(fighter_state_33b00, the snapshot restore `0x2791C` calls for both sides -
`0x27994..0x2799A`; no earlier capture replayed it, because the expiry route
never takes). No other census row moved. The baseline reason is "final truth
Task 4: gp-u29-continue-taken improves 0x2791C and 0x33B00 (the 0x2791C take
and the snapshot restore it calls; HIT 659 / NOT-HIT 96 / UNINSTRUMENTED 4)",
and a re-run `--check` reads `no change`.

`tools/capture_manifest.py --generate` writes **43 entries** (42 present, 1
declared skip) and `make gate-check` reads **42/43 captures present, one
missing**, the declared U14 gap, with the manifest, the verify preflight and the
pin baseline (`b5cc953efeb550ca03404cee7b183f386f447865f0a914cb551e46c8fb1e243a`,
reason "final truth Task 4: measured gp-u29-continue-taken pins ... and the
capture-346 two-segment scan-out allowance") green.

`PR_PINS_EXACT=1 make gp-u29-continue-taken-oracle` is green, including the
evidence checker, the replay/miss set, the frame/order/coverage, trace/moves and
the 4-milestone path. The affected Python suites (`test_gp_continue`,
`test_gp_session`, `test_gp_compare`, `test_gp_capture`,
`test_capture_manifest`) pass; no game or port implementation file changed, no
other capture's oracle pin moved, and the full `make verify` remains Task 9.

`prage_000.dro` is deliberately outside `tools/audio_compare.py`'s 29-trace
inventory (the `GP_TRACES` list is fixed and has no gp-u29 entry): no audio
section was measured, so no section minimum or audio claim is fabricated.
The claim is the take and the restart, not a fought KO or a win: the route's
KOs are harness writes and the credit spend is the game's own `0x2CA9C` debit.

## §F5 The mode-`0x25` bowling trigger: the `+0x53` machine, the count window and the staggered route (Task 5)

**Decision:** pending the one capture (§F5.5). This section is the raw derivation
(Task 5 Step 1) and the field/scenario/checker design; the capture's measured
result and either the published oracle or the terminal declaration follow in
§F5.5.

### F5.1 The reaction, the hook, and the one-sided impossibility

`0x455A0` is character 4's **reaction-`0x22` callback** (the dword at `0xA4BD0`,
entry `(4 << 6) + 0x22` of the `0xA3528` reaction rows). The only path that
applies a reaction is `0x34E2C` (`hit_reaction_apply`), whose callers are
`0x350D0` (`0x352CD`, inside the `+0x53` dispatcher's default case),
`0x3CE58` (`0x3CF2E`, the hitbox/reaction drive) and none else
(`prage.calls.csv`). It

* zeroes the applying slot's `+0x88` counter — `0x34E9E` `mov word [eax+0x88],0`;
* stores the reaction byte in the slot's `+0x5F` — `0x34EE7` `mov [eax+0x5f],cl`
  — and mirrors it in `DS_001088A8 + side` (`0x34EF6`, the S fields `r0`/`r1`);
* then runs the per-(char, reaction) callback.

`0x455A0` writes the performing slot's **`+0x53 = 7`** (`0x455C3`
`mov byte [ecx+0x53],7`) and `+0x52 = 9` (`0x455C7`), plays the `0xEB516`
stream (`0x455B2..0x455BE`) and, outside mode `0x25` (`0x455DB` `cmp eax,0x25`,
`0x455DE` `jne`), installs `+0x0C = 0x452E4`, **`+0x18 = 0x45158`**
(`0x45602`) and `+0x1C = 0x451EC` (`0x45609`).

`0x45158` is the per-slot `+0x18` hook `0x19020` calls every fight frame
(`0x1903F` `call [ebx+0x877C8]`, EAX = side, EAX returned; `0x1904C`/`0x1905C`
store the result latch). It runs `0x18C14` with flag 0 = 1, flags 1 and 8 = 0
and every other flag 2 (`0x45169..0x4518B`) and increments the **word
`DS_00108892`** at `0x451C6` (`inc word [0x88892]`) only when all of:

| # | condition | raw |
|---|---|---|
| 1 | the `0x18C14` result is 0 | `0x45190` `test eax,eax` / `0x45192` `jne` |
| 2 | `DS_00100AF8[side] > 0` (the projected-overlap latch) | `0x18C49` `cmp [eax*4+0x80AF8],0` / `0x18C51` `setle`; the flag-1 arm returns 1 while it is `<= 0` (`0x18C62`/`0x18C6B`) |
| 3 | the other slot's word `+0x74 == 0` and `+0x76 <= 1` | `0x18CC0` `cmp word [eax+0x74],0` `ja`; `0x18CD4` `cmp eax,1` `jg` |
| 4 | the other slot's `+0x42` bit 3 clear | `0x18E4E` `test byte [eax+0x42],8` `jne` |
| 5 | the mode is not `0x25` | `0x4519D` `cmp edx,0x25` / `0x451A0` `je` |
| 6 | `ctx[3]` (the **other** slot) `+0x5F == 0x22` | `0x451AF` `cmp edx,0x22` |
| 7 | `ctx[3]` `+0x7A == 4` | `0x451C1` `cmp edx,4` |
| 8 | (the increment) | `0x451C6` `inc word [0x88892]` |

`ctx[3]` is the other side (`0x33950`'s context; `0x45158` reads
`[esp+0xc]`, its fourth dword). Condition 2 is the **pixel-overlap latch**:
`0x1756F` (inside `camera_unfreeze`, called from `camera_decay` `0x176AC`/
`0x176BF`) writes `DS_00100AF8[side] = DS_00100B54`, the shared count of set
bits of the two projected fighters' AND-ed bit-planes (`0x16FDE..0x17001`),
scaled at `0x1703E..0x1706F`; it is non-zero only when the two sprites overlap
on screen, and only when the `0x140E4` box-overlap gate passed and the other
slot's `+0x74 == 0`/`+0x76 <= 1` (`0x170C5`/`0x170E2`) — the same two words as
condition 3.

**The one-sided route is dead.** The hook installed by a slot's own reaction
(`0x455A0` `0x45602`) counts *the other* slot's reaction. If only P2 performs,
`fighter_19020(1)` calls P2's hook, which reads P1's `+0x5F` (not `0x22`) →
no increment; P1's `+0x18` is 0 from the spawn reset (`0x33C78`'s `0x33E07`
`mov dword [slot+0x18],0`, next to `0x33DC3`'s `+0x88 = 0`), so
`fighter_19020(0)` returns at `0x19032` and no second hook exists. Zero
increments. The prior symmetric route (both sides play character 4's move 0)
is therefore the raw's own shape, not a harness convenience: both slots must be
in `+0x5F == 0x22` for either hook to count, and each hook then counts the
other.

### F5.2 The `+0x53` machine and the 91-frame clear

The `+0x53` dispatcher `0x3531C` (called only from `0x35658` at `0x35803`, inside
`fight_hud_pass`) switches on the slot's `+0x53` (`0x353E9`, table `0x352DC`).
Case 7 (`0x3541B..0x3546B`) is the reaction's own arm:

```
03541B  or byte [slot+0x41],0x80
035422  and byte [slot+0x43],0xcf
03542B  test ebp,ebp / 0x35431 call [ecx+0xc]   ; the +0x0C callback (0x452E4)
035436  mov al,[ecx+0x7a] / 0x35439 cmp eax,4 / 0x3543C jne 0x354e5
035444  mov al,[ecx+0x5f] / 0x35447 cmp eax,0x21 / 0x3544A je
03544C  cmp eax,0x22 / 0x3544F jne 0x354e5
035455  mov eax,[ecx+0x86] / 0x3545B sar eax,0x10 / 0x3545E cmp eax,0x5a / 0x35461 jle 0x354e5
035467  mov eax,ecx / 0x35469 mov edx,[ecx] / 0x3546B call 0x367dc
```

`0x367DC` restarts the record's animation and clears the slot:

```
036806  mov byte [ebx+0x52],0
03680A  mov byte [ebx+0x53],0
03680E  mov byte [ebx+0x5f],0xff
```

The dword at `+0x86`'s high word is the slot's `+0x88` word, which `0x34E2C`
zeroes when the reaction is applied (`0x34E9E`) and `0x35658` increments once
per frame at `0x356D6` (`mov word [ebx+0x87838],dx`), **before** the same
function's `0x35803 call 0x3531C`. So the case-7 clear fires on the frame whose
increment makes `+0x88 = 0x5B = 91 > 0x5A`, i.e. **91 frames after the
application frame** (`F`): the apply frame leaves `+0x88 = 0` (the apply runs in
the same `fight_hud_pass`, after `0x356D6`, through `0x350D0` → `0x352CD`), and
frames `F+1 .. F+91` carry `+0x53 = 7`; at `F+91` the clear runs.

The other three `0x367DC` callers cannot fire during this window: `0x359E0`
(state 1's command gate, `0x35A21`) and `0x399CC` (`+0x52 == 7`, `0x399FC`)
need another `+0x52` state, and our measured `+0x52` series is
`9 → 0x10 → {4,0x14,9} → 0` (`gp-bowling-fix.failed`), never `7` or `1`;
`0x38154` is mode `0x25`'s own per-side spawn, which cannot run before the
entry. And because `0x3531C` dispatches on `+0x53`, no other state handler runs
while it is `7`, so nothing else can clear it. The capture's appended `s0_53`
field measures the window directly (91 samples of `7`, then `0`).

### F5.3 The count window: the gate and the clear share one frame

`game_mode_04_step` (`0x26254`) runs, in this order: `camera_decay` (`0x262E9`,
writes the overlap latch), `fighter_pass_a` (`0x262EE` → `0x19020` → `0x45158`,
the increments), `fighter_pass_b`, `fighter_think` (`0x2633D`),
`fight_slot_pass` (`0x26385`), **`fight_hud_pass(0)` then `(1)`** (`0x2638A`/
`0x26391` → `0x35658`: the `+0x88` increment at `0x356D6`, then `0x3531C` at
`0x35803`, hence the case-7 clear at `0x3546B`), effects/scene/HUD pulse, the
round-end check, and finally the entry gate (`0x263BA..0x263E8`):

```
0263CB  cmp edx,2 / 0x263CE jne 0x263ef        ; DS_001078FA == 2
0263D2  mov dx,[0x88892] / 0x263D9 cmp edx,6 / 0x263DC jl 0x263ef
0263DE  mov dl,[0x87803] / 0x263E4 test dl,dl / 0x263E6 jne 0x263ef   ; slot 0's +0x53
0263EA  call 0x4e11c
```

`0x4E11C` then rejects on `b1d ∈ {2,3}` (`0x4E127..0x4E131` `dec word
[0x88892]`) or on any of the camera/`0x104AD8`/`0x104ABC`/both-character-4/
`b8c4` gates (`0x4E13D..0x4E189`, the same decrement), and otherwise stores its
entry set including the count reset `0x4E1CB` `mov word [0x88892],0` and the
mode `0x4E1E9` (`mov word [0x84b00],0x25`).

So the count is only *visible to the gate* when slot 0's `+0x53 == 0`, and the
only frame inside a reaction where that holds is the clear frame `F+91`
(§F5.2) — the increments happen earlier, in `fighter_pass_a`, while `+0x53` is
still `7`. **The count must already be `>= 6` at `F+91`'s `fighter_pass_a`, and
the gate reads it later in that same iteration.** The increments and the clear
can therefore never be separated by a frame: a capture that samples `b892 = 5`
and no mode entry is genuinely one increment short, not a frame-boundary
artifact.

The gate's other inputs:

| input | derived value in this route | raw |
|---|---|---|
| `DS_001078FA == 2` | both slots live: `fighter_spawn_slot` increments it per spawn (`0x33CDA..0x33CEA`) | `0x263CB` |
| `b1d` not 2/3 | the capture's `b1d = 0` (the two-human non-demo match) | `0x4E127` |
| `camx` `in [-0x3300,0x3300]` (signed) | **unmeasured before this task** | `0x4E13D..0x4E151` |
| `DS_00104AD8 <= 0` (signed) | `0` in a fight: its only increment is mode `0xB`'s tick `0x28C47`, and the round reset `0x25C1C` stores `0` (`0x25C2A`) | `0x4E153` |
| `DS_00104ABC >= 2` | `(b1f == 3) + 1` at `0x28DCF..0x28DE7` (match start) and `0x25EB2..0x25EC5` (round-end case 3); `b1f = 3` gives 2 | `0x4E15C` |
| both characters 4 | `c0 = c1 = 4` in every capture snapshot | `0x4E165..0x4E17D` |
| `b8c4 == 0` before | `0x494FE` clears it at the round entrance; the entry sets it to 1 (`0x4E1AE`) | `0x4E17F` |

So four of the five gate inputs the live-truth phase left unsampled are now
derived (`0x104ABC`, `0x104AD8`, `0x107803`, and the latch via the clear
timing); only the camera remains, which is why the capture appends it.

### F5.4 The measured attempts and the exact residue

Both prior artifacts are real-input two-human runs of the same move-0 route
(both sides play character 4's move 0; the move's reaction is `0x22` by
`tools/gp_moves.py list --char 4`):

| capture | attempts | visible `b892` samples (mode 6) | state-9 edges/side | mode `0x25` |
|---|---|---:|---:|---|
| `gp-bowling.failed` | 1 | `0 -> 2` at `f=0x5C2` | 2 | no |
| `gp-bowling-fix.failed` | 3 | `1@0x5F9, 3@0x69C, 5@0x732` | 6 | no |

(The fix capture's +1/+2/+2 is the number of frames in each 91-frame window
where the §F5.1 conjunction held; the single-attempt run's +2 is the same
conjunction on both slots.) Its last sample is **5**, and the capture then runs
~2,300 frames of idle mode-6 fight through the `until_mode 0x25, 300` step with
`b892 = 5` and `s0_53` (as we now know) back to `0`, so the gate was evaluated
for thousands of frames with the count one short: the route was one attempt (or
one increment) short, not blocked.

**The exact residue this task's one capture settles:**

1. **The count.** With a fourth attempt's `+2` at `F4` the count is `7` at
   `F4+91`; the visible pre-entry series must then read `1, 3, 5, 7` and the
   entry frame must hold `b892 = 0`. A fifth attempt is the margin if an
   attempt yields `+1`. If no attempt adds an increment, the visible series
   stops at `5` and the count — not the camera — is the residue.
2. **The camera.** The entry snapshot's `camx` must lie in
   `[-0x3300, 0x3300]` (signed). If the count reaches `>= 6` at a clear frame
   and the mode still does not open, the camera is the failing input: the
   rejection arm then decrements, so the visible series shows `...,5,7,6` (the
   `7` sampled at `F4+90`, the `6` after `0x4E189`) and the measured `camx`
   names the reason. That is the terminal declaration instead of an unresolved
   blocker.
3. **The `+0x53` window.** The appended `s0_53` samples must show `7` for the
   91 frames `F..F+90` after each application and `0` at the clear, which
   turns §F5.2's raw timing into a measurement (or corrects it).

A one-sided route cannot produce (1) (§F5.1), and no poke can (§F5.3's gate is
the game's own), so the route stays the symmetric move-0 route with 4-5
attempts at the measured 150-frame spacing.


### F5.5 The one capture and the measured trigger

Command, run exactly once with the host idle (load 0.36 on 20 CPUs):

```
make gp-capture scenario=gp-bowling-2
```

The published capture is `data/k11-captures/gp-bowling-2`: **1,491 frames**
(raw 1381..4030), **2,207 snapshots** (`f=0x0005..0x08A9`; 6 game frames missed
by the documented poller), wall 58.4 s, and poll.log sha256
**`54d4c7d506c0835f8f94e1270029348cbb529d6313ba5759c0f69fe3134ad3b3`**. All ten
session checks pass: base; **28/32 steps fired, `stopped on mode 0x25`**; end
frame reached; mode `0x27` after Enter; `kb == raw` (0 differ); 1,491/1,491
frames written; port script v2; no unscripted input; 0/0 pokes with zero races;
zero skipped BIOS words; stopped at the end (`SIGTERM` at `f=0x8AA`, rc=0). The
X record is `f=0x86E` (the entry + the 60-frame stop delay). No `.failed`
artifact and no second capture.

The route played **four** attempts and the fourth opened the mode: 28 = the 12
base steps + 4 × 4 attempt steps, so the fifth attempt never fired. The
measured trigger:

| Claim | Capture evidence |
|---|---|
| both reactions | `r0=r1=0x22` from `f=0x5EA` |
| the counter | `b892` 0→**1** @`0x5EF`, 1→**3** @`0x692`, 3→**5** @`0x728`, 5→**7** @`0x7BE`; 0 at the entry |
| the `+0x53` lattice | attempt 1: `7` @`0x5EA..0x621` (56 samples), `0`, `4`, `0`; attempt 2: `7` @`0x680..0x691` (18), `0x0A` @`0x692..0x6E0` (79), `0x0B` @`0x6E1..0x705` (37), `0`; attempt 3: `7` @`0x716..0x75A` (69), `0`, `4`, `0`; attempt 4: `7` @`0x7AC..0x7BD` (18), `0x0A` @`0x7BE..0x80C` (79), `0x0B` @`0x80D..0x831` (37), **`0` at `0x832` = the entry** |
| the camera | `camx` **-1433..4887** over the fight's 713 mode-6 snapshots, **-606** at the entry (the gate is `±0x3300 = ±13056`) |
| the entry | `f=0x832` mode `0x25`, `b892=0`, `s0_53=0x0C`, `b8c4=1`, `b8a6=0xB4`, `b8bd=b415=0`, `b8c0=1`, `b1f=3`, `b1d=0`, `c0=c1=4`, `b25=0`; 120 mode-`0x25` snapshots to `f=0x8A9` |
| no poke | **0** `W` records |

**The residue is settled: the count, not the camera.** The three-attempt route
stopped at 5; the fourth attempt's +2 reached 7 and the mode opened on the
first frame whose `+0x53` was 0. The camera never approached its gate
(`-1433..4887` against `±13056`), and the fifth attempt was margin that was not
needed.

**Plan-vs-raw corrections (the measurement wins; §F5.2/§F5.3 are corrected
here):**

1. **`+0x53` is not held at `7` for 91 frames.** Its writers are not all behind
   the `0x3531C` dispatcher: the pose arming `0x39F7F`/`0x3A98A` stores `0x0A`,
   the knockdown floor `0x347B8` stores `0x0B` (`0x3484C`), the attack consumer
   `0x3BDDC` stores `4` (`0x3BF16`) and the launch `0x3BC70` stores `0`
   (`0x3BC97`). The measured `7` windows are 18, 56 and 69 samples long, then
   `0x0A`/`0x0B`; the §F5.2 derivation's "nothing else can clear it" was wrong.
   The 91-frame `0x367DC` clear (`+0x88 > 0x5A`) was never reached in this
   route.
2. **The gate's zero came from the get-up, not the `0x367DC` clear.** Attempt
   4's `+0x53` left `0x0B` for `0` at `f=0x832` (the get-up's `0x35D6C`:
   `if (+0x53 != 0x0D) +0x53 = 0`), 116 frames after the last increment. So
   §F5.3's "the count must reach 6 at the clear frame" holds in the weaker,
   measured form: the count must be `>= 6` at the **first frame after the
   window where slot 0's `+0x53` is 0**, whenever that frame is. The count had
   been `7` since `0x7BE`, and the entry is the first such zero.
3. **The entry snapshot's `s0_53` is `0x0C`, not 0.** `0x4E11C`'s own
   `0x38154(side)` writes it: the flag-clear arm stores `0x0C` and the
   flag-set arm runs `0x367DC` (0). The `0x263DE` gate's `0` is read earlier in
   the same iteration and is not in the end-of-frame snapshot. The checker
   accepts `{0, 0x0C}` and prints the measured value.

### F5.6 The checker, the allowance, the oracle and the pins

`tools/gp_bowling.py check --staggered` (its own branch; `check` for the old
route is unchanged) proves from the capture's records alone: no `W`; 3..10
state-9 rising edges per side (`>= 3` the live-truth minimum, `<= 10` = the
route's five attempts × the two per attempt both prior captures measured); the
counter's visible maximum `>= 6` (the fourth attempt's increments over the
three-attempt route's measured 5); every increase `<= 2` (the two `0x45158`
hooks, one `inc word [0x88892]` each per frame); every decrease exactly the
rejection arm's `-1` from `>= 6` at a frame whose `s0_53` is 0
(`0x4E131`/`0x4E189` only run behind the `0x263DE` gate); at least one
pre-entry `s0_53 = 7` (the reaction window); the `b8c4` 0 → 1 transition; the
entry's `s0_53 in {0, 0x0C}`, `camx` signed within `±0x3300`, and every
`fight_mode25_enter` store; real pad input on both sides and `r0=r1=0x22`.
The CLI pins the scenario name (`--scenario`, default `gp-bowling-2`) and the
poll.log sha (`--capture-sha256`). `tools/tests/test_gp_bowling.py` runs **25
tests** (10 new): the four-attempt crossing, the count one short, the camera
inside/outside at both ends, the rejection shapes (a valid `7→6` at
`s0_53=0`, a `7→6` at `s0_53=7`, a `5→4`, a `7→5`), a `+3` frame, the edge
bounds (2 and 12), the missing window, both entry writes (`0`, `0x0C`) and two
wrong ones (`7`, `0x0B`), the poke, the CLI and the prior artifacts. Both
preserved artifacts still exit 1 with the recorded verdict, now with the
residue appended:

```
gp_bowling: gp-bowling.failed: evidence: FAIL: mode 0x25 never observed (b892 max 2 in mode 6), camx not sampled (pre-Task-5 capture)
gp_bowling: gp-bowling-fix.failed: evidence: FAIL: mode 0x25 never observed (b892 max 5 in mode 6), camx not sampled (pre-Task-5 capture)
```

**The frame allowance.** Capture **443** (raw 2896), the second wipe-in's first
presented frame, is the only frame the two-frame model cannot answer in the
window: its rows 0..18 are black, rows 19..32 are port 301's (`f=0x4DD`
tick=`0x5CD` mode=`0x1B`) and rows 33..199 are port 302's (`f=0x4DE`
tick=`0x5D3` mode=`0x1B`) — a scan-out that spanned two presents, the SCAN3
class. Its byte layout repeats at ports 31 and 255 (the first transition's
identical wipe), so the measured occurrence 301 is the entry's pinned fifth
field:

```
SCAN3_ALLOWED['gp-bowling-2'] = ((443, 19, 33,
    '558052a232c34e9c3607aab24451e103c2c7062885013197af9270ef18924539', 301),)
```

Named limits (as §G6): the top rows are bound by hash and by the *existence* of
an earlier holder (many all-black frames hold them) and the row splits 19/33 are
capture-pinned bytes — the presents' timing is not claimed. Its neighbours are
a splice (442, the loader strip) and a two-frame `(302, 303)` splice at row 61
(444). A real-dump mutation proof flipped one byte of port 301's bound rows
(`frame_00301.ipx` offset `25*320+300`): the allowance reported
`pinned adjacent port pair 301/302 does not bind its rows [19,33) and [33,200)`,
the frame became `FIRST UNEXPLAINED capture 443` and the ratchet failed
(`first unexplained 443 < ratchet N 1422`); the original dump was not modified.
`tools/tests/test_gp_compare.py` pins the tuple and proves the pinned-occurrence
model on a fixture (two identical pairs: unpinned ambiguous, wrong pin refused,
measured pin accepted).

**The replay and the pins.** The replay reaches the X record (`f=0x86E`) and
records its own miss set, `k_miss_gp_bowling` (`port/tests/test_platform.c`):
`0x29D60` (bare `ret`, hits=1) and `0x5D812` (runtime stub, hits=1), both from
`frontend_mode_1b_step`; the global `0x5D812 actor_spawn`/`set_dead` rows are
the fight's. The dump has **1,138 presented frames** (0..1137); trace and moves
agree through `f=0x86E = 2158` with zero differences (1,845 frames compared from
`f=134` on, 6 capture frames without a snapshot). Frame
measurement: 1,318 classified (726 clean, 588 splice, 2 transition, 1
scan3-allowed, 8 all-black, 1 unexplained), 0 order failures.

| Pin | Measured value | Evidence |
|---|---:|---|
| `GP_BOWLING_MIN_FIRST` | **1422** | first unexplained capture 1422 (raw 3962), the port dump's post-script tail; the entry is capture **1352** (a splice of port 1077/1078, port 1077 `f=0x832` mode 0x25), so the frame claim covers it and 70 frames after it (capture 1370 is port 1093, `f=0x842`) |
| `GP_BOWLING_TRACE_MIN_FIRST` / `GP_BOWLING_MOVES_MIN_FIRST` | **2159** / **2159** | 0 differing through 2158 (`f=0x86E`, the X record) |
| `GP_BOWLING_MAX_START` | **97** | window starts at capture 97 (raw 1735) |
| `GP_BOWLING_MIN_REACH` | **1137** | highest exhibited port index (the dump's last presented frame) |
| `GP_BOWLING_MAX_MISSING` | **4** | canonical non-black missing classes `[9, 11, 68, 255]` through port 1137 |
| `GP_BOWLING_CAPTURE_FRAMES` | **1491** | stored capture frames |
| `GP_BOWLING_CAPTURE_SHA256` | **`54d4c7d5...4ad3b3`** | full value above |

Every pin was moved one step in its failing direction and restored:

| Mutation | Observed failure |
|---|---|
| `MIN_FIRST 1422 -> 1423` | `first unexplained 1422 < ratchet N 1423` |
| `TRACE_MIN_FIRST 2159 -> 2160` | `N 2160 > end 2159: N is unreachable` |
| `MOVES_MIN_FIRST 2159 -> 2160` | the same unreachable-N failure on moves |
| `MAX_START 97 -> 96` | `window starts at capture 97 (raw 1735) > pinned start 96` |
| `MIN_REACH 1137 -> 1136` with `PR_PINS_EXACT=1` | `window reach port 1137 > pinned reach 1136; re-pin to 1137` |
| `MIN_REACH 1137 -> 1138` | `window reach port 1137 < pinned reach 1138` |
| `MAX_MISSING 4 -> 5` with `PR_PINS_EXACT=1` | `4 missing class(es) < pinned 5; re-pin to 4` |
| capture SHA final digit `3 -> 4` | capture identity mismatch (poll.log sha) |
| `CAPTURE_FRAMES 1491 -> 1490` | capture identity mismatch (1,491 actual frames) |
| one bound byte of port 301 | `allowed capture 443 ... is not its named three-frame scan-out` + `FIRST UNEXPLAINED capture 443` |

`PR_PINS_EXACT=1 make gp-bowling-oracle` is green: the checker, the replay/miss
set, the frame/order/coverage and the trace/moves ratchets. The claim is narrow
like the other gp oracles: no under-render detection inside the window, order
and per-frame coverage not claimed; the `b892`/`s0_53`/`camx` series are the
checker's capture-side claim (they are not in the compare's trace/move field
sets), and the mode-0x25 entry is claimed by the trace's `mode` field at
`f=0x832` plus the checker's stores. `tools/capture_manifest.py --generate`
writes **44 entries** (43 present, 1 declared skip) and `make gate-check` reads
**43/44 captures present**, the declared U14 gap, with the manifest, the verify
preflight and the pin baseline re-pinned (reason "final truth Task 5: measured
gp-bowling-2 pins ...").

**Coverage.** The capture is in `CAPTURE_DIRS`, so `make coverage`'s
`GP_COV_SCENARIOS` includes it automatically. The run re-pinned the census from
**HIT 659 / NOT-HIT 96 / UNINSTRUMENTED 4** to **HIT 675 / NOT-HIT 80 /
UNINSTRUMENTED 4** (`tools/coverage_baseline.txt`, reason "final truth Task 5:
gp-bowling-2 reaches the mode-0x25 entry and its first 60 frames"), and the 16
rows that moved are exactly the mode-`0x25` chain:
`0x12FD8` (camera_mode_track_pair), `0x18950` (fighter_connect_query), `0x266AC`
(game_mode_25_step), `0x38154`/`0x382C4`/`0x38434`/`0x384F8`/`0x385B0` (the
mode-25 per-side approach/combo passes), `0x3C1C8`, `0x4A808`/`0x4A8A8` (the
audience effects' handlers), `0x4E11C` (fight_mode25_enter), `0x4E27C`,
`0x4E350` (the pin spawn), `0x4E5A4` and `0x4E67C` (the audience pass). No
other row moved, and a re-run `--check` reads `no change (HIT=675 NOT-HIT=80
UNINSTRUMENTED=4)`.

`prage_000.dro` stays outside `tools/audio_compare.py`'s 29-trace inventory (the
`GP_TRACES` list has no gp-bowling-2 entry): no audio section was measured, so
no audio claim is fabricated. The claim is the trigger and the entry, not the
bowling scoreboard: the capture holds 120 mode-`0x25` snapshots and the frame claim
reaches capture 1421 (the entry at capture 1352, +70), and nothing past it is claimed.

### F5.7 Fix round 1 (2026-10-30): the register text, the entry-store mutation proof, the entry's capture index

Three corrections; no capture, no pin moved.

1. **The authoritative docs no longer say the mode never opened** (Final truth
   Task 8 still owns the full register): `AGENTS.md`'s `0x21`/`0x25` block now
   states the measured outcome (capture, entry stores, oracle/pins, census
   `0x4E11C` `HIT`, the claim boundary) and marks the old "stays declared /
   never opened / no oracle or pin" register superseded; `docs/PROGRESS.md`
   gains the Final truth Task 5 paragraph and marks the Live truth Task 6, the
   state-coverage Task 4 and the Task 12a sentences superseded; `port/RE_GUIDE.md`'s `0x25` passage is
   updated the same way; the design's status line and §3.3 carry the outcome
   (`0x25` published, the count residue, the measured `+0x53` lattice
   correction) with the pre-capture derivation kept as the raw evidence; and
   the plan's Task 5 item 3 records the measured edge bound (`3..10`, two per
   attempt per side) and the entry's `s0_53` value.
2. **The staggered checker's entry-store predicates are mutation-proved.**
   `TestStaggered.test_each_entry_store_predicate_is_required` mutates each of
   `b1f`, `b1d`, `c0`, `c1`, `b25`, `b8c4`, `b8a6`, `b8bd`, `b415`, `b8c0` at
   the entry, seeds a pre-entry `b8c4 = 1`, and drops the `I` records,
   asserting the named failure (the old route's table test calls `evidence()`,
   so it could not cover this branch). Each of the four predicate deletions in
   `gp_bowling.py` was then applied and caught: the `ENTRY_VALUES` loop (nine
   sub-tests), the `b1d` gate, the `pad_sides` gate and the `b8c4` 0 → 1 gate
   (the log is task-5-report.md §12); `gp_bowling.py` was restored
   byte-identically.
3. **The entry's capture index is 1352, not 1370.** Capture 1352 is a splice
   of port 1077 (`f=0x832` tick=`0x927` mode `0x25`) and port 1078
   (`f=0x833`); capture 1370 is port 1093 (`f=0x842`, clean). The frame claim
   (`MIN_FIRST` 1422) therefore covers the entry and 70 frames after it; N, the
   start, the reach and every other pin are unchanged. The Makefile comment,
   this record and the report are corrected and §F5.6's table row carries the
   measured classification.

## §F6 The `0x21` volleyball trigger: the `+0x1C` re-hit question, the refuted blocker and the declared outcome (Task 6)

**Decision: `0x21` stays declared, and the design's candidate blocker is
refuted: the eight-trample count is not limited to 1 per spawn.** No capture,
no code change, no pin. The re-hit latch (`entry+0x1C` bit 7) returns during the
tumble's **fall**, because the engine's own motion integration lifts the actor
from the trample's `+0x36 = 0x240`; no animation stream is involved. The gate is
therefore *mechanically* reachable, and every link but the composition is already
pinned by a unit test or a differential row (§F6.4). What no byte sequence
yields is a *frame-exact eight-hit route*: the trigger needs a two-human match
whose stage word is one `rng(7)` draw away, a crowd-lie state, and eight strikes
each inside a ~7-pass window of a 71-motion-step flight, so the phase's one
capture was not spent on an underived script (§F6.7-§F6.8). The mode's own pass and the eighth-hit tail stay
`NOT-HIT` (§F6.6); no "structurally unreachable" register text is written
(Task 8 owns the final register).

### F6.1 The two `+0x1C` fields (they are different objects)

| field | object | use |
|---|---|---|
| `entry+0x1C` (byte) | the fight-effect list entry (type at `+0x1E`, count at `+0x1F`, side at `+0x20/+0x21`, actor at `+8`) | bit 7 = the **re-hit latch** (`0x4B69C` tests it at `0x4B6A6..0x4B6B3`); bit 6 = held; bit 5 = the mode-`0x21` ball; bit 2/1/0 = the walk/flight flags |
| `actor+0x1C` (dword) | the animation record (`DS_001014F4 + index*0x68`, pset at `0x1014EC + index*0x20`) | the **height** added to the depth y when the pset point is written (`0x2A690`'s `ysrc = (rec+0x30>>16) + rec+0x1C`); the vertical motion is the word at `actor+0x36` (`vy`), paired with `+0x34` (`vx`) in the dword at `+0x34` |

The brief's "actor's `+0x1C`" and the "bit 7 of `+0x1C`'s word" are the two
different fields above. `0x4B470` writes neither height nor latch-return: it
writes `actor+0x36` and clears `entry+0x1C` bit 7 (`0x4B52E`, `0x4B544`).

### F6.2 Every writer of the actor's `+0x1C` (height)

Enumerated from the raw (addresses are the store instructions; the port line is
in `port/src/game/fight.c`/`actors.c`):

| # | writer | raw address | store | reaches the dust/worshipper actor? |
|---|---|---|---|---|
| 1 | `actor_spawn` (`0x2AE14`) | `0x2AF44` `mov [ecx+0x1c],eax` | `h = a4` (the 4th argument) | **yes** — both dust builders pass 0 (`0x4960F`, `0x4D04E`), and `0x4B470`'s shadow spawn passes 0 too (`0x4B4D2`) |
| 2 | `motion_step` (`0x2A4FC`) | `0x2A524 mov edx,[eax+0x34]` / `0x2A52A sar edx,0x10` / `0x2A531 add ecx,edx` / `0x2A537 mov [eax+0x1c],ecx` | `h += (s32)DSD(+0x34)>>16` = the word at `+0x36` (`vy`), **every iteration** | **yes** — its gate `0x2A501..0x2A510` tests the byte `+0x2B` bit 0x80, which the spawn sets at `0x2AF66/0x2AF6C`, and the dust descriptor's flags word is `0x1000` (bit 0x200, the `actor_sync` skip, clear: `0xBB470..0xBB4E4`) |
| 3 | `motion_step`'s gravity landing | `0x2A5BC`/`0x2A60D` (`mov [eax+0x1c],edx` with edx = 0) | `h = 0`, in the `+0x44 != 0` arm | **no** — the spawn zeroes `+0x44` and no dust path writes it (the gravity arm is dead for this actor) |
| 4 | the grab arm `0x4B788` | `0x4B8E8` | the held actor's `h = oy + holder h` | only on a **grab** (returns 0; does not count) |
| 5 | the mode-`0x22` grab arm `0x4D898` | `0x4DA19` | the same | mode `0x22` only |
| 6 | `0x49C78` case 6 landing | `0x49F9A` `mov dword [ebx+0x1c],0` | `h = 0` on landing | **yes** (§F6.4) |
| 7 | `0x4D2D0` case 6 landing | `0x4D727` | `h = 0` | mode `0x22` only |
| 8 | `0x4BF18` case 6 landing | `0x4C1D3` | `h = 0` | mode `0x21` only (the ball) |
| 9 | the stream interpreter's **opcode 0x21** | `0x2B7B8` `cwde; mov edi,[esi+0x1c]; shl eax,6; add edi,eax; mov [esi+0x1c],edi` | `h += (s16)operand * 64` | only if a stream executes it |
| 10 | the child derivation in the sync | `0x2A898..0x2A8A0` | `h = parent_h + (the +0x34 word >> 16) * 64` | **no** — the dust actors are not children (`a5 = 0`; `0x2AE14`'s `0x400` arm) |
| 11 | the other actor families | `0x19BE0` (debris), `0x29137` (type `0x0A`/`0x19`) | `h = 0` | no (different records) |
| 12 | `actor_mode1_pset` (`0x2BE5C`) | `0x2BE5C` (`DSD(rec+0x1c) = 0x3BC0 + F0AEC - pset_y - (rec+0x30>>16)`) | the mode-1 projection's height, from the pset y | no — the type-`0x19`/`0x0A` cb1 tails (`0x28FB5`/`0x2906D`) are its only callers (`actors.c:2775`), a different actor family |

**The stream answer.** The interpreter *can* write the height (opcode `0x21`)
and the velocity (`0x25` `0x2B7E9` `vx +=`, `0x26` `0x2B7FF` `vy +=`, `0x28`
`0x2B825` `vx =`, `0x29` `0x2B850` `vy =`, `0x2A` `0x2B860` `+0x38 =`), but the
six **tumble** streams `0xC9604[0..5]` (`0xEE160`, `0xEE512`, `0xEE87C`,
`0xEEC44`, `0xEF00E`, `0xEF400`) contain **no motion opcode at all**: each is a
sprite-hold loop `[0xCD40 <id-base>][loop up var 0x40 for N][sprite 0x000E]
[0x8E40 var 0x40 = 0][0xC300 <own head>]`, N = 7/6/6/11/6/6, i.e. 6..11 sprites
then a one-hold filler, looped. The same is true of the walk (`0xC9544`) and the
lie (`0xC9634`, `0xC9724`) streams used around the trample. So **no stream
restores `+0x1C` or bit 7 after a counted trample** — and none is needed.

### F6.3 Every writer of `entry+0x1C` bit 7 (the latch)

| effect | raw address | context |
|---|---|---|
| **set** | `0x49F18` `or byte [ecx+0x1c],0x80` | `0x49C78` case 6, `actor+0x36 < 0` (`0x49F11 cmp word [ebx+0x36],0` / `0x49F16 jge`) |
| set | `0x4D650` | `0x4D2D0` case 6, same test |
| set | `0x49E52` (`or dl,0x80`; type 4 at `0x49E47`) | the main pass's case 3 landing: the **lie** (type 3 is set by `0x4B5A8` at `0x4B656`, the crowd reaction gate `0x4B5A8`'s `DS_001088B6[side]`/`+0x52==7,+0x53==2`) |
| set | `0x4D4FA` | mode `0x22` case 3 → type 4 |
| set | `0x4C14F`, `0x4C227` | the mode-`0x21` pass (`0x4BF18`) case 6 and shared tail |
| set | `0x4E7D2`, `0x4E830` | the mode-`0x25` audience pass `fight_4e67c` (`fight.c:5871-5887`): the audience worships while within `±0x100` of the performer (`0x4E7C1`/`0x4E7CE`) or on the `0xC9810`/`0x0C9830` bit draws |
| **clear** | `0x4B544` `and byte [esi+0x1c],0x7f` | **`0x4B470`** (every trample, counted or not) |
| clear | `0x4CC05` | `0x4CB18` (the launch) |
| clear | `0x4C0D5` (the mode-`0x21` cases 3/4), `0x4A16F`/`0x4A172` (case 10's get-up), `0x4D5ED` (mode-`0x22` case 4), `0x4E836` (the mode-`0x25` audience) | the other transitions |

Only `0x49F18` (main pass) and `0x4D650` (mode-`0x22`) restore the latch on a
*tumble*; both require `vy < 0`, i.e. the **fall**. `0x4B69C` is called per
entry per iteration *before* the type dispatch (`0x49CFE` → the switch at
`0x49D03`), so a latch set on iteration *n* is seen by the prelude on *n+1*.

### F6.4 The flight, the re-hit window, and why the count is not 1

**The flight.** `0x4B470` sets `actor+0x36 = 0x240` (`0x4B52E`) and leaves the
height alone (it is 0 for a lying entry: `0x2AF44` with `a4 = 0`). Case 6 then
runs every iteration:

1. `0x49F11..0x49F18`: if `vy < 0`, set the latch;
2. `0x49F3A..0x49F47`: `(s32)DSD(actor+0x34)>>16 + DSD(actor+0x1C) > 0` → still
   airborne: `0x49FB8 sub word [ebx+0x36],0x10`; else land;
3. after the mode step, `actors_update` (`0x2A31C`) runs `actor_sync`
   (`0x2A1FC`) → `motion_step` (`0x2A4FC`), which adds `vy` to the height
   (`0x2A537`). (The order is the raw's: the effects pass runs inside the mode step
   (`0x2639B`, `0x2652A`, `0x28847` are its fight-mode call sites) and
   `actors_update`/`0x2A31C` in the master-loop tail, `flow.c:7947`.)

**The arithmetic, with the same-pass decrement first.** The trample runs in the
prelude (`0x49CFE`), so the *same* effects pass dispatches case 6 and
decrements before `actors_update` ever runs: the **first motion step adds
`0x230`, not `0x240`** (`test_fight.c:2904` asserts the stored `+0x36 = 0x0230`
after the trample pass). Let `n` be the number of motion steps completed since
the trample (`n = 0` on the trample pass). The pass that follows them reads the
stored `vy = 0x240 - 0x10·n` (the latch test `0x49F11..0x49F18` runs first),
tests `h(n) + vy = -8(n² - 69n - 72) > 0` (`0x49F3A..0x49F47`), then decrements
to `0x230 - 0x10·n` (`0x49FB8`) and `motion_step` adds that (`0x2A537`), so
`h(n) = 568n - 8n²`. The test passes for `n ≤ 70`; the **72nd effects pass
lands** (`0x49F9A..0x49FAF`, zeroing `h`, `vy`, `vx` and **`+0x1F`**) after 71
motion steps. The apex is `h(35) = h(36) = 10,080 = 0x2760` (157.5 px), and the
last airborne height `h(71) = 0`.

**The window.** `vy < 0` for `n = 37..71` — **35 passes** per flight (the last is
the landing pass, whose latch set the landing then clears in the same pass); the
prelude sees a latch set on the previous pass. The *practical* window is
narrower: the entry's pset point
`(pset_x/64 - 0x18, pset_y/64 - 0x38)` (`0x17D64..0x17D97`) must fall inside a
fighter's `0x30 x 0x38` attack box (`0x1790C`'s `DS_00100AC8 + side*4` box, the
`0x15F48` bitplane). `0x38` = 56 px = 3,584 units; `h(64) = 3,584` and
`h(65) = 3,120`, so the point is in vertical reach on the **7 passes** whose
preceding motion step left `h < 3,584` (n = 65..71).

**Why the count is not limited to 1.** A hit in that window needs only
`entry+0x1C & 0x80` (`0x4B6A6..0x4B6B3`) and a fighter's box containing the
point (`0x4B6E2`), so the prelude counts it (`0x4B716 inc byte [ecx+0x1f]`) and
`0x4B470` re-launches: `vy = 0x240` again, latch cleared again, another 71-
motion-step flight. The count is zeroed **only** by the landing (`0x49FAF`) or a
spawn/rebuild (`0x49622`, `0x4D061`, `0x4E4E8`), so it accumulates across
consecutive flights: 8 hits needed for `+0x1F > 7` (`0x4B579`). The trample's
`vx` also reverses on hits 2+ (`0x4B50F..0x4B525`: `vx == -0x80` → `+0x80`, else
`-0x80`), which is what bounces the worshipper between two fighters. The
horizontal throw is **≈140-142 px** (70-71 motion steps x 2 px = 0x80/64), *not*
a fixed 0x90: `motion_step` also adds the record's `+0x42` to `|vx|` each step
and clamps by `+0x43` (`actors.c:2874-2879`); `actor_spawn` zeroes `+0x43`
(`actors.c:3811`) but never writes `+0x42`, so a non-zero pool residue
lengthens the throw. The four hit actors measured here (`gp-idle-loss` x2, the demo x2) all
read `+0x42 = 0`, so their throws are the 2 px/frame figure; the vertical flight
and its window do not depend on `vx` at all.

**Every link is already pinned.** `test_fight.c`'s effects-pass test:
case G (the demo's trample: `+0x1F = 1`, `vy = 0x230` after the same-pass
decrement, the shadow, `vx = -0x80`, type 6, bit 7 cleared), case H (`vy =
0x100` with `h = 0x1000` keeps bit 7 clear; `vy = -0x20` sets it: `0x85`), case
I (the landing zeroes `h`, `vy`, `vx` and `+0x1F`), case J (`+0x1F` 1 → 2 on a
seeded latch, `vx` reversal). `0x2AE14` and `0x2A4FC` each have a differential
row (`1033/1033 functions VERIFIED`). The *composition* (trample → flight →
fall → latch → second hit) is what this record derives; no test drives it
end-to-end, and no capture has measured it.

### F6.5 The gate chain (all addresses re-read from the raw)

`0x4B470`'s tail, after the increment (`0x4B716`) and the latch clear
(`0x4B544`):

| # | gate | raw | file |
|---|---|---|---|
| 1 | `b1d != 3 && != 2` | `0x4B547`/`0x4B54C` | `fight.c:3080` |
| 2 | the stage word `DS_00104AFC == 0` | `0x4B559` | `fight.c:3081` |
| 3 | `DS_001088C1 == 0` | `0x4B562` | `fight.c:3082` |
| 4 | `(s8)DS_001088F2 > 1` (the round clock's top byte, `[0x1088EF]>>24`) | `0x4B564`/`0x4B56F` | `fight.c:3083` |
| 5 | `entry+0x1F > 7` | `0x4B579` | `fight.c:3084` |
| 6 | `DS_001088C5 == 0` | `0x4B582` | `fight.c:3085` |
| 7 | `DS_00108864 == 0` | `0x4B58C` | `fight.c:3086` |

then `0x4BD98` (`0x4B590`) and `0x4CB18` (`0x4B59E`). `0x4BD98`'s own gates:
`DSD(DS_00104AD8) <= 0` (`0x4BDA7`), `DSD(DS_00104ABC) >= 2` (`0x4BDB4`, i.e.
`b1f == 3`), `fight_13134` false (`0x4BDC1`, both fighters' x outside the same
`±0x3300`); its stores: `DS_00108864 = entry`, `entry+0x1E = 6` (`0x4BDD9`),
`DS_001088C1 = 1` (`0x4BDE1`), **`DS_00104B00 = 0x21` (`0x4BDE7`)**,
`DS_001088C5 = 1` (`0x4BDEE`), the `0x1E` timer (`0x4BDF4`), the clears and
`entry+0x1C |= 0x20` (`0x4BE3C`), then the two actors from `0xBAB88`/`0xBAB9C`
with `+0x36 = 0x1A4`. `0x4CB18` runs whatever `0x4BD98` decided (it is called
unconditionally after the gate): the launch that resets the entry to type 6 and
clears bit 7 (`0x4CC05`) from the actor's height (`0x4CB84`).

**The mode's own pass** (what a published capture would have to show):
`game_mode_21_step` (`0x26540`) runs `0x4BF18` in place of `fight_effects_pass`
— a *volleyball* pass over the same `0x10884C` list, with its own type
dispatch (`0x4BEF4`), its own case 6 (the tumble, `0x4C135`, with the same
landing that zeroes `+0x1F` at `0x4C27F`) and the ball strike `0x4C60C` (the
point hit test, `+0x1F` count, `0x4CB18` launch with the flag 1 when the
fighter's `+0x5F` is `0x0C..0x0F`), and the timeout end `0x4CD98` (mode 6, the
score bytes, string `0x60` twice). So the eight tramples *enter* a game whose
own rules are a second, different count over the same entry — the mode does not
reuse `0x4B470`.

### F6.6 The stage word: 1-in-7 on the two-human pick (the design's figure, confirmed)

`0x4B470`'s gate 2 needs `DS_00104AFC == 0`. `flow_stage_pick` (`0x25848`,
called by the arcade stage-setup hook `fight_hook_430e8` at `0x430FE`,
`fight.c:1134`) branches on `DS_00104B17` (`0x2584E`) and `b1f`:

- `m == 0` and `b1f == 3` → `DSW(0x104AFC) = rng(7)` (`0x2587C/0x25881`): the
  **two-human pick**, uniform 0..6, so stage 0 with probability 1/7;
- `m == 0` and `b1f != 3` → `0xA87C4[c] + rng(6) + 1`, reduced once by 7
  (`0x2588C..0x258C6`, table `{0,4,2,3,6,1,5}`): the **1P/standby pick**, which
  can never give 0 for character 0 (base 0 → 1..6 only);
- `m == 2` → return (keeps the word); any other `m` → the bitmask pick
  (`0x258D2..`).

Measured with a throwaway instrumented build (`PR_FT6_LOG` in
`flow_stage_pick`, the port running each scenario's script; every run made
**exactly one** call):

| scenario | the call | arm | word |
|---|---|---|---|
| `gp-twop` (two-human) | f=876, `m=0`, `b1f=3` | `rng(7)` (`0x2587C`) | **5** |
| `gp-pads` (1P) | f=1618, `m=0`, `b1f=1` | base + `rng(6)` | 4 |
| `gp-u8-right-arcade` (1P) | f=1554, `m=0`, `b1f=2` | base + `rng(6)` | 3 |

The join's `DS_00104B17 = 2` (`0x28E58`, `flow.c:1643`) does not survive to the
pick: `game_coin_divert` (`0x257A4`) zeroes it at `0x257C0` and sets
`b1f = players` (`0x257E0`) at the game start, which is why every measured pick
reads `m = 0` and the two-human one reads `b1f = 3`. So **the design's 1-in-7 is
right for the two-human route**: the round-1 text "not a 1-in-7 gamble / one
`rng(6)` draw" is **withdrawn**, and the 0-base barrier belongs to the 1P pick
only. A 1/7 per match is a *retry* condition (a few matches, or a restart), not
a structural blocker.

The stage also does not depend on the capture's `poll.log`: `afc` is a
WIN_EXTRA trace field (present in every gp replay trace, in `poll.log` only for
the U9/U10 captures), so the measured words above are port-replay values. The
demo's own tramples (below) read `stage=0`, which is the same word the demo's
mode-3 fight runs with.

### F6.7 The measured evidence

**The trample is reached in a real capture — twice.** `gp-idle-loss` (the 1P
arcade capture where P1 idles and the AI fights; 2,565 frames) hits `0x4B470`
twice. The port replay (`PR_GP_DUMP`, `PR_FT6_LOG` on a throwaway instrumented
build, reverted) logs:

```
FT6 4b470 f=2238 via=1 entry=108438 idx=3 if=1 ic=80 type=4 mode=6 stage=4 act=2a7eeb8 h=0 vy=0 vx=0 g42=0
FT6 4b470 f=2238 via=1 entry=108414 idx=5 if=1 ic=80 type=4 mode=6 stage=4 act=2a7ee50 h=0 vy=0 vx=0 g42=0
```

Both are **first counted hits** (`via=1`, `+0x1F = 1`) in the same frame
(f=2238, one attack box touching two entries) on **different** lying entries
(`entry+0x1C = 0x80`, bit 6 clear, type 4), stage 4 — the crowd-lie path
(`0x4B5A8` → `0x49E52`) reached by the AI's combo. They are not the type-8
release (that handler returns at `0x4A097` without bit 6, and `via` would
read 2). The census's
per-driver hit file agrees: `fn-hit gp-idle-loss 0x4B470 n=2`
(`make coverage`'s `coverage/gp-idle-loss.hits`).

**The demo reaches it too — twice, both counted.** The `frontend` coverage tag
is the demo driver `PR_FRONTEND_DUMP` (Makefile:2797), *not* the unarmed parent
`PR_FRONTEND_DET`; round 1 measured the parent and wrongly reported zero. With
the demo driver instrumented to record the call site (`ft6_via`: 1 = the
`0x4B69C` prelude at `0x4B779`, 2 = the type-8 release at `0x4A10B`, 3 = the
mode-`0x22` prelude at `0x4D873`):

```
FT6 4b470 f=2582 via=1 entry=108438 idx=4 if=1 ic=80 type=4 mode=3 stage=0 act=2a7ef88 h=0 vy=0 vx=0 g42=0
FT6 4b470 f=2582 via=1 entry=108414 idx=3 if=1 ic=80 type=4 mode=3 stage=0 act=2a7ef20 h=0 vy=0 vx=0 g42=0
```

`via=1` both: **first counted hits on distinct lying entries, at stage 0**, not
releases. The census rows agree (`fn-hit frontend 0x4B470 n=2`,
`0x4B69C n=7648`, `0x4B788 n=2`). So the demo is a second *measured* trample
site (it is not a capture; `gp-idle-loss` remains the only capture) and it also
shows the crowd lie and the trample at stage 0.

The same instrumented replays of the other fights — `gp-twop`, `gp-twop-ko`,
`gp-draw-timeout`, `gp-keys-fight` — call `0x4B470` **zero** times and never
sample an entry with bit 7 set. So four measured calls exist across the runs
(two in `gp-idle-loss`, two in the demo), **all first hits on distinct
entries**; a count of two or more is still not measured anywhere.

**The census rows** (`tools/coverage_baseline.txt`, Task 12b's pin, not moved):
`0x4B470 HIT`, `0x4B69C HIT`, `0x4B5A8 HIT`, `0x4B144 HIT`, `0x4BD4C HIT`;
`0x4BD98 NOT-HIT`, `0x4BF18 NOT-HIT`, `0x4C60C NOT-HIT`, `0x4C784 NOT-HIT`,
`0x4CB18 NOT-HIT`, `0x4CD98 NOT-HIT`, `0x26540 NOT-HIT`, `0x13134 NOT-HIT`.
The `HIT` rows are *not* gameplay evidence on their own: `run_tests`' unit cases
call `0x4B470`/`0x4B69C` directly (G/H/I/J and the grab-arm rows), and the
coverage driver that reaches it in gameplay is `gp-idle-loss` (above). The
`NOT-HIT` rows are the tail and the mode, and they stay `NOT-HIT`: no measured
run has eight counted tramples.

### F6.8 Why the one capture was not taken

The brief allows one capture **only if the derivation survives**. It yields the
window (7 passes, `n = 65..71` of a 71-motion-step flight), the stream/phase
chain and the stage's 1-in-7 draw, but not a frame-exact route:

1. **The strike frames are not in the static bytes.** Whether a strike lands is
   `camera_point_hit` against the *fighter's* current attack box, whose 4-byte
   `DS_00100AC8` box is written per attack frame from that character's stream
   data; the derivation cannot pin "press at F".
2. **Eight windows, 71 motion steps apart.** Each hit needs one of ~7 passes; a
   miss lands the entry (`0x49FAF` zeroes the count) and restarts the chain. A
   scripted first attempt has no margin to discover the phase.
3. **The set-up is a chain of its own.** A stage-0 two-human match (a 1-in-7
   draw, §F6.6), the crowd lie (§F6.3: a combo or a fighter state 7/2 —
   measured in `gp-idle-loss` and the demo, but not in any two-human capture),
   and both fighters ~140 px apart.
4. **The budget is one.** `.failed` publishes nothing, so a blind attempt would
   spend the phase's only `gp-u21-volley` on a measured "no trample" — a weaker
   record than this derivation, with no second attempt possible.

The honest route to publishing `0x21` is a **port-side search** (the port is
deterministic and its fight is frame-verified against captures): script the
two-human join, a character with a non-zero stage base, a combo to lie the
crowd, and two spammers, then read `+0x1F`/`+0x1C` from the port until an
eight-count appears; that script would then be a *derived* capture route. That
is a new task (and a new capture authorization), not this brief.

### F6.9 The outcome and the doc changes

- `0x21` stays declared, with the corrected statement: the count is **not**
  limited to 1 (the design's §3.2 blocker is refuted), the gate is mechanically
  reachable, and no frame-exact route is derivable from the checked-in bytes —
  a port-side search is the next task; the tail `0x4BD98` and the pass
  `0x4BF18` stay `NOT-HIT`.
- No capture, no code change, no S field, no oracle, no pin, no miss set; the
  census baseline is untouched (all the rows above are cited, not re-pinned).
- Docs updated (fix round 1 included): the design's §3.2 gains the outcome
  paragraph; `AGENTS.md`'s `0x21` bullet states the corrected verdict and the
  measured boundary; `docs/PROGRESS.md` gains the Task 6 paragraph; the
  live-truth design §2.6 and record §L9.3 carry superseded notes (the
  counter/landing blocker is refuted, the stage is a 1-in-7 retry), and the
  plan's Task 8 step 3 lists both files in the register's reconciliation set so
  the final text cannot repeat the refuted blockers.
