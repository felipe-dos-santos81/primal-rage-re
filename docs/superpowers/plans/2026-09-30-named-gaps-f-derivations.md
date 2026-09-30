# Named gaps unit F: movie exits, movie skip tests, sequence-volume fade (raw-byte derivation)

**Scope.** Ledger `2026-09-29-all-gaps-ledger.md` §H.3 named gaps #8
(`0x5DECA`'s fade), #9 (the movie player's entry skip tests) and #10
(`movie_play`'s decode-failure exit). Bounded brief of the named-gaps effort
(`.superpowers/sdd/2026-09-30-named-gaps/`), branch `named-gaps-f`, tag `nf`.

**Tooling.** The Ghidra MCP was unavailable. Every address was read from the
fixed-up LE mirror (`k11_img.bin`, the image of the K10/K11 records, fixups
applied) with capstone in 32-bit mode; function extents from
`port/decomp/prage.functions.csv`. Rel32 call scans cover the code object
`0x10000..0x73B14`; absolute-address scans cover the whole image
`0x10000..0x10B0D0`.

**Verdict.** All three gaps close. #10 and #9 are ported with the raw's
behaviour; #8 is ported from the raw's arithmetic (every constant has an
address). Corrections found on the way are in §F.5.

---

## §F.1 `0x1C740`'s exits (gap #10)

The listing (capstone, `0x1C740..0x1C880`) matches K10 §0.1. The exits:

| from | to | blank at `0x1C873`? |
|---|---|---|
| `0x1C759` (`0x62756` non-zero), `0x1C766` (`[0xA81A8]` non-zero), `0x1C77F` (open failed) | `0x1C878` epilogue | no |
| `0x1C7AA` (`edi > [ebp+0xC]`), `0x1C846` (`0x62756`), `0x1C854` (`0x50161`), `0x1C865` fall-through | `0x1C86B` close, `0x1C871 xor eax,eax`, `0x1C873 call 0x52106` | yes |

`0x1C7E6 push esi; 0x1C7E7 call 0x64130` (the frame decode) is followed
directly by `0x1C7EC push esi` (the rectangle loop): **the decode's return
value is never read.** The raw has no decode-failure exit at all, so the only
way out of the loop is through `0x1C86B`, which blanks.

**Port.** `movie_play` now leaves the loop on a decode failure (`ok = 0;
break;`) and falls into the `0x1C873` blank like every other loop exit; it
still returns 0 (the port's "playback failed" signal; `0x11050`/`0x1105A`
ignore the raw's EAX, and `attract_step` only logs it). `PORT:` note: the port
cannot draw past a frame it cannot decode, so it takes the raw's loop exit
instead of continuing. The pre-loop port exits (file not found, `smk_open`
rejected, unsupported size) stay without the exit blank: they are the raw's
`0x1C77F` open-failure path.

## §F.2 The skip tests (gap #9)

### `0x62756` is WATCOM's `kbhit`

```
62756: cmp dword [0xef910],0 ; je 62765
6275f: mov eax,1 ; ret
62765: mov ah,0x0b ; int 0x21 ; cbw ; cwde ; ret
```

A pending `ungetch` byte `[0xEF910]`, else DOS "check standard input status"
(`AH=0Bh`: `AL=0xFF` when a key is waiting), sign-extended. `[0xEF910]` is 0
in the image (`00000000`) and its only other references are `0x72BA0` (read)
and `0x72BA6` (`mov [0xef910],edx` with `edx = 0`, `0x72B9D xor edx,edx`), both
in `0x72B9C` (`getche`: read and clear). No instruction stores a non-zero
value, so `0x62756` reduces to "the console has a key": the BIOS keyboard
buffer, which the game reads with `int 16h` itself (`0x24D0E`, `0x24A64`). The
port models that buffer as the input queue (`input.h`), so `0x62756` =
`input_has_key()`. It does **not** read the key: the key stays queued.

### `[0xA81A8]` is the quit flag

Every absolute reference to `0xA81A8` in the image: `0x1C75F` (the movie test),
`0x24A93 mov byte [0xa81a8],1` (the quit prompt `0x249F0`'s yes) and
`0x256DD` (the master loop's exit test). The only writer stores 1. It is
already `mem[]` state in the port (`DS_000A81A8`).

It is reachable at `0x1C75F`: in `0x24C5C` the key loop (`0x24D08..0x24EEC`,
`int 16h`, `0x24DDF call 0x249F0`) runs before the mode switch
(`0x24EEC..0x24F01`, jump table `0x24B8C`), and the switch reaches the attract
step through `0x25238 call 0x11D04` (`0x11D04` calls `0x11000`, whose phase 0
calls `0x1C740` at `0x11050`/`0x1105A`). So a quit confirmed on the iteration
whose attract is at phase 0 skips both logos, and `0x256DD` then ends the
loop.

### The loop's key tests

`0x1C83F call 0x62756; test; jne 0x1C86B` and `0x1C848 mov eax,0xff00ff00;
call 0x50161; test; jne 0x1C86B`, polled around the frame wait `0x65240`. The
port had these as a draining ESC poll (`input_drain_esc`). The raw leaves on
**any** key and leaves it queued, so the next movie's entry test (§ above)
skips it and `0x24C5C` then reads it. `0x50161` is the ported
`input_select_bits` (`mem[]` state `DS_000E1C34`/`DS_000E1C38`); with the level
word only ever in `0xFF00FF00` (`0x500C4`), it returns the pad bits pressed and
not yet latched.

### The harness conflict, and its fix

`main.c`'s `--check` loop and the five test drivers used to preset
`DS_000A81A8 = 1` so that one `game_loop()` call runs one iteration. With the
entry test modelled, that preset would skip every logo in every headless run.
`game_loop_step()` (`flow.c`, `PORT:`) now runs one iteration with a host-side
brake and leaves the quit flag alone; the loop still stops on the flag
(`0x256DD`). `game_main()` is unchanged.

**Port.** `movie_play`: `if (input_has_key()) return 1;` (`0x1C752`),
`if (DSB(DS_000A81A8) != 0) return 1;` (`0x1C75F`), both after the entry blank
and before the open; in the loop `input_has_key()` (`0x1C83F`) and
`input_select_bits(0xFF00FF00)` (`0x1C848`), then the `PORT:` window-close
test. `input_drain_esc` has no caller left and is removed with its 5 assertion
sites in `test_input`.

## §F.3 The sequence-volume fade (gap #8)

### `0x5DECA` → `0x6A8D0`

`0x5DECA` pushes its three arguments and calls `0x6A8D0` (`0x5DEE1`). All
four game call sites push `0x1F4` = 500 ms (`0x1C8FD`, `0x1C992`, `0x1C9DD`,
`0x1CAF6`), volume `[0xA2CB8]`, sequence `[0x1028C0]`.

`0x6A8D0(seq, volume, ms)` (`esi` = the AIL sequence record):

| addr | effect |
|---|---|
| `0x6A8DA` | `seq == 0` → return |
| `0x6A8E6` | `[seq+0x38] = volume` (target) |
| `0x6A8EF..0x6A8FA` | `[seq+0x34] == target` → return |
| `0x6A8FB..0x6A902` | `ms == 0` → `[seq+0x34] = target` |
| `0x6A904..0x6A937` | else `ebx = ms*1000` (`shl 5; sub; lea *4; add; lea *8`), `[seq+0x40] = ebx / abs([seq+0x34] - target)` (signed `idiv`), `[seq+0x3C] = 0` |
| `0x6A93B` | `call 0x69320` (re-send) |

`0x69320(seq)`: for `ch = 0..15`, when `[seq+0x350+ch*4] != -1`, dispatch
`(0xB0|ch, 7, [seq+0x350+ch*4])` through `0x68AB0`.

`[seq+0x350+ch*4]` is the CC7 log: `0x68AB0` calls `0x688E0(seq+0xD0, status,
ctrl, value)` for every `B0`/`C0`/`E0` event (`0x68B07 lea eax,[esi+0xd0]`,
`0x68B0E`), and `0x688E0`'s controller-7 arm stores the unscaled value at
`base + ch*4 + 0x280` (`0x68A48`) = `seq + 0x350 + ch*4`. `0x69250` fills
`seq+0xD0..+0x510` with `-1` (`0x69267..0x69277`, `0x440` bytes).

`0x68AB0`'s CC7 arm (`0x68C8B..0x68CAD`) then scales the value by
`[seq+0x34]` (`imul; idiv 0x7F`), clamps to `0..0x7F`, and hands the result to
the driver, which stores it as the channel volume. So the driver holds the
volume **scaled at receipt**, and a change of `[seq+0x34]` reaches a channel
only by a new CC7 or a re-send.

### The service step (`0x69372`)

Per service call, for each sequence with status 4 (`0x693B7`):
`inc [seq+0x30]` (`0x693C1`), the tempo accumulator and the event ticks
(`0x693C4..0x69949`), then at `0x69952`, unless the sequence ended on this call
(`[0x108E24]`, set at `0x69548` on `FF 2F`):

```
6995F: if [seq+0x34] == [seq+0x38] goto 699C3       ; no fade
69969: [seq+0x3C] += [[seq]+0x10]                    ; one service period
69979: while [seq+0x3C] >= [seq+0x40]:               ; jl 699AF (signed)
6998D:     [seq+0x3C] -= [seq+0x40]
69990:     [seq+0x34] += target > vol ? 1 : -1
699AA:     if [seq+0x34] == target: break
699B4: if ([seq+0x30] & 7) == 0: call 0x69320        ; re-send, every 8th call
```

The re-send runs only on calls that started with the volume off its target:
the call that reaches the target re-sends only when its count is a multiple
of 8, and the next call skips the block. So the target itself is usually never
re-sent; the channel keeps the last re-sent value until a CC7 arrives.

### The constants

- **Service period** `[[seq]+0x10]` (the driver record's `+0x10`):
  `0x6A016 mov eax,0xf4240; 0x6A027 mov esi,[0x108d8c]; 0x6A030 idiv esi;
  0x6A03E mov [edx+0x10],eax`: `1000000 / preference 10`. AIL_startup sets
  preference 10 to `0x78` (`0x660CF push 0x78; push 0xa; call 0x5d87e`); the
  game's own `0x5D87E` calls set preferences 7, 4, 1, 3 and 11 only
  (`0x10056`, `0x1CF66`, `0x1CF75`, `0x1CF81`, `0x1CFC9`). So the period is
  `1000000 / 120 = 8333` us, the same 120 Hz the sequencer already ticks at
  (todo-verify record §15).
- **Seed.** `0x6A410` (AIL_init_sequence) on success: `0x6A578 call 0x69250`
  (log to -1, `0x692E5 [seq+0x30] = 0`), `0x6A57D/0x6A584` step and
  accumulator 0, `0x6A5B7 mov eax,[0x108d94]; 0x6A5BC [seq+0x34] = eax;
  0x6A5C5 [seq+0x38] = eax`. `[0x108D94]` is preference 12, set to `0x7F` at
  `0x660E7`. The failure exits (`0x6A457`, `0x6A553`) come before.
  AIL_start_sequence (`0x6A780`) calls `0x69250` again (`0x6A79B`) and does not
  touch `+0x34..+0x40`.

### Port

`sequencer.c` gains the record's fields (volume now, target, accumulator, step,
call count, CC7 log). `seq_fade_sequence_volume` is `0x6A8D0`,
`seq_resend_volume` is `0x69320`, `seq_fade_step` is `0x69952..0x699C0` (run
at the end of `seq_tick` while playing), and a CC7 is logged and stored scaled
(`0x68A48`, `0x68C8B`), with the TL law reading the stored value.
`AIL_set_sequence_volume` forwards to `seq_fade_sequence_volume`, and
`AIL_init_sequence` reseeds `0x7F` on success. `seq_set_sequence_volume`
(the sequencer's engine input, used by the C-vs-Python oracle and the WAV
render) sets the volume and the target at once. With a constant volume the
scaled-at-receipt value equals the old scaled-at-apply value, so the C stream
is unchanged (`oracle C-vs-Python: 9866 writes byte-exact`, the WAV
byte-identical).

In the game the fade runs when `DS_000A2CB8` differs from `0x7F` at a music
start (`0x1C930`: init, then `0x5DECA(…, 500)`, then start) or when the
volume changes while the music plays (`0x1CAB8`). `DS_000A2CB8`'s image value
is `0x7F`.

## §F.4 Tests

`test_movie_exits` (`test_video.c`, new, registered after `test_movie_blit`)
and block 4e of `test_ail` (`test_audio.c`).

Measured RED before each implementation: 13 FAIL lines for
`test_movie_exits`, 9 for block 4e (its first version).

Mutation proofs (each applied alone, built, suite run; FAIL lines as measured,
the closing `FAILURES: N` excluded):

| mutant | FAIL lines |
|---|---|
| M1 no entry kbhit test | `test_video.c:482: 1 != 0`, `:483: 3 != 1` |
| M2 no entry quit-flag test | `:492: 41 != 0`, `:493: 43 != 1` |
| M3 no loop kbhit test | `:503: 41 != 2`, `:504: 43 != 4` |
| M4 no loop `0x50161` test | `:515: 41 != 1`, `:516: 43 != 3`, `:518: 0 != 16777216` |
| M5 decode failure returns before the blank (the old code) | `:556: 4 != 5`, `:557: 1 != 0` |
| M7 loop test consumes the key | `:506: the loop test leaves the key queued` |
| M8 entry test consumes the key | `:485: the entry test leaves the key queued` |
| M9 no exit blank | `:316: 1 != 0`, `:319: 44 != 0`, `:320: 10 != 0`, `:413: 15600 != 0`, `:505: 1 != 0`, `:517: 1 != 0`, `:557: 1 != 0` |
| M11 decode failure returns 1 | `:554: 1 != 0` |
| F1 fade ignored (`ms` forced to 0) | `test_audio.c:1442: 63 != 127`, `:1443: 49 != 100`, `:1445: 63 != 121`, `:1446: 49 != 100`, `:1448: 63 != 120`, `:1449: 49 != 94`, `:1451: 63 != 69`, `:1452: 49 != 54`, `:1454: 63 != 65`, `:1459: 49 != 54` |
| F2 re-send on every call | `:1446: 95 != 100`, `:1459: 49 != 54` |
| F3 service period `1000000/60` | `:1445: 115 != 121`, `:1448: 113 != 120`, `:1449: 88 != 94`, `:1451: 63 != 69`, `:1452: 61 != 54`, `:1454: 63 != 65`, `:1459: 61 != 54` |
| F4 re-send also at the target | `:956: 14234 != 9866` (the C-vs-Python oracle), `:1459: 49 != 54` |
| F5 init does not reseed | 13 lines, `:1437: 32 != 127` … `:1459: 47 != 54` |
| F6 CC7 stored unscaled | `:964: C register stream equals the Python oracle byte-for-byte`, `:1449: 100 != 94`, `:1452: 100 != 54`, `:1459: 100 != 54` |
| F7 no stop at the target (`0x699AA`) | `:1465: 61 != 62` |
| F8 no re-send on the call (`0x6A93B`) | `:1469: 54 != 62` |
| F9 zero time treated as a fade | `:1468: 62 != 80`, `:1469: 48 != 62` |
| H1 front-end driver presets the quit flag again | `test_game.c:6237: front-end determinism run completes` (twice); the fe dump loses 328 files (164 movie screens per run) |

F1..F6 were measured before the 4e tail (stop-at-target and zero-time
assertions) was added; lines up to `:1459` are unchanged by that addition.
F7..F9 are measured on the final test. M10 (a decode failure `continue`s
instead of `break`s) is an equivalent mutant: the failing frame is retried and
fails every time, so the same three frames and the exit blank are observed.
H1 on `main.c`'s `--check` loop is not visible to the `--check` frames
(the frame after the logos is black either way): 0 of its frames change.

Assertion sites: 13545 → 13587 (+47 new: 27 in `test_video.c`, 20 in
`test_audio.c`; −5 with `input_drain_esc`).

**Not tested.**
- The pre-loop exits without the blank (not found, rejected, unsupported size)
  are unchanged; only "not found" has an existing assertion (`test_movie`).
- `0x62756`'s `[0xEF910]` arm (never non-zero, §F.2) and the Ctrl-C handling
  inside DOS `AH=0Bh`.
- The `PORT:` window-close test in the movie loop (no window in the suite).
- The quit flag set by the quit prompt inside a real iteration at attract
  phase 0 (the unit test seeds the flag directly).
- A fade toward a volume outside `0..0x7F` (the game's `DS_000A2CB8` stays in
  range; the scale clamps, as `0x68C9F..0x68CAD` does).
- The "sequence ended on this call" skip (`[0x108E24]`): covered by the port's
  `S.playing` test after `process()`, not asserted.
- `0x68AB0`'s channel map `[seq+0x90+ch*4]` and the lock test at `0x68C53`,
  which the port does not model (identity map, one sequence).

## §F.5 Corrections (raw wins)

- **`seq+0x350` has a second writer.** `2026-09-20-midi-controllers-tl-derivation.md`
  (the lines starting "The **only** writers of the per-channel volume array")
  and `port/spec/audio.md`'s residual note say the only writer is controller
  83. `0x688E0`'s controller-7 arm writes it on every CC7 (`0x68A48`, base
  `seq+0xD0` from `0x68B07`), and `0x69320` re-sends it during a fade. The
  residual exclusion itself is unchanged here (the capture oracle line does
  not move); the fade is a per-time volume that would give channels scaled at
  different times different effective volumes, which is the shape of the ch1/ch4
  residual, but that capture was not re-analysed in this unit.
- **The movie loop's key test is `kbhit`, not an ESC poll.** K10 §0.1 lists
  "`0x62756` / `0x50161(0xFF00FF00)` → key exit"; the port had read it as ESC
  only and drained the queue (`input_drain_esc`). §F.2.
- **The ledger's #10 wording.** "every exit of the frame loop reaches
  `0x1C873`" holds, but the raw has no decode-failure exit: `0x64130`'s result
  is unused (§F.1).
