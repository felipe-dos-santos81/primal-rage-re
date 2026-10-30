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
