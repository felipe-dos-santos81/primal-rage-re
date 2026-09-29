# K11 MENU-CB — the service menu: derivation record

Plan: `docs/superpowers/plans/2026-09-29-k11-service-menu.md`. Ledger rows:
`2026-09-29-all-gaps-ledger.md` §B.1 (`0x2F464`, `0x319B0`, `0x31A78`), §B.2
"Service menu", §F row 18. This record is the authoritative raw-byte source for
the plan's tasks. §0 is the planner's walk; the executor re-checks every row it
ports against the raw and appends §K11.1..§K11.9 (one section per task). On any
conflict between §0 and the raw, **the raw wins**; record the correction and its
address in the task's section.

## §0 The walk (planner, 2026-09-29)

### §0.1 Tooling and method

The Ghidra MCP bridge was not reachable. The image is the Python mirror of
`mem_load_le` + `mem_load_le_fixups` (the ledger's `le.py` in the session
scratchpad). The fixups are applied, and the image is byte-identical to the
ledger's `img_rev.bin`. Disassembly used capstone. The closure is a
recursive-descent walk from each root. It follows `jcc`/`jmp` inside a function.
It follows `jmp [reg*4 + table]` and `jmp cs:[reg + table]` through the tables'
entries. It records every `call rel32`. It treats a `jmp rel32` to a known
function, or to a target more than 0x800 bytes away, as a tail call. Each
unported callee (a function with no `/* 0xADDR` header and no `fn_register`
in `port/src`) was walked the same way. The sizes below are **reachable
instruction bytes** and exclude jump tables. The six tables are `0x19C20` and
`0x19CF4` (16 dwords each), and `0x319A0`, `0x31A68`, `0x31B84` and `0x31C68` (4
dwords each).

Strings: `ENGLISH.TXT` decoded as `0x474E4` does (group walk via `+4`, entries
XOR'd with their length). Scratch scripts: `k11_walk.py`, `k11_closure.py`,
`k11_tab.py`, `k11_str.py` in the session scratchpad.

### §0.2 The tables (fixed-up image)

`0xBCBDC` MAIN MENU (the table case `0x27` passes to `0x2FFC4`, flags 4):

| entry | +0 string | +4 | +8 callback | +0xC |
|---|---|---|---|---|
| `BCBDC` | `0x210` "MAIN MENU" | 0 | 0 | 0 |
| `BCBEC` | `0x211` "Start" | 0 | `0x2CB74` | 0 |
| `BCBFC` | `0x76` "GAME OPTIONS" | 0 | `0x2CB94` | 1 |

`0xBCC1C` OPTIONS MENU (opened by `0x2CB94` through the blocking `0x2FA40`, flags 0):

| entry | +0 string | +8 callback |
|---|---|---|
| `BCC1C` | `0x212` "OPTIONS MENU" | 0 |
| `BCC2C` | `0x213` "CONFIG OPTIONS" | `0x2CACC` |
| `BCC3C` | `0x81` "STATISTICS" | `0x2CAC0` |
| `BCC4C` | `0x20F` "SOUND TEST" | `0x30EB4` |
| `BCC5C` | `0x214` "MUSIC TEST" | `0x30F54` |
| `BCC6C` | `0x20E` "MODIFY CONTROLS" | `0x31F24` |
| `BCC7C` | `0x237` "CONFIGURE KEYBOARD" | `0x19DF0` |
| `BCC8C` | `0x20D` "TEST CONTROLS" | `0x32358` |
| `BCC9C` | `0x20C` "ADJUST VOLUME" | `0x30864` |
| `BCCAC` | `0x21A` "2 PLAYER HANDICAP" | `0x31138` |

All items from `BCC3C` on have `+0xC = 1`.

`0xBCCCC` START MENU (opened by `0x2CB74` through `0x2FFC4`, flags 0). The walk
found this third table and its seven callbacks. The ledger does not list them.

| entry | +0 / +4 strings | +8 | stores `DSW(DS_00104B00)` / `DSB(DS_00104B1D)` |
|---|---|---|---|
| `BCCDC` | `0x17` "LEFT PLAYER" / `0x216` "ARCADE" | `0x2CBC4` | `0x2D` / 0 |
| `BCCEC` | `0x16` "RIGHT PLAYER" / `0x216` | `0x2CBDC` | `0x2E` / 0 |
| `BCCFC` | `0x17` / `0x217` "TRAINING" | `0x2CBF4` | `0x28` / 1 |
| `BCD0C` | `0x16` / `0x217` | `0x2CC0C` | `0x29` / 1 |
| `BCD1C` | `0x218` "TUG OF WAR" / 0 | `0x2CC24` | `0x2A` / 2 |
| `BCD2C` | `0x219` "ENDURANCE" / 0 | `0x2CC3C` | `0x2B` / 3 |
| `BCD3C` | `0x211` "Start" / `0x21A` "2 PLAYER HANDICAP" | `0x2CC54` | `0x2C` / 4 |

**Correction to the task statement.** The task calls this the "service menu" and
expects "audit display, coin/switch tests, volume/sound test, high-score reset".
The raw is the PC build's **options menu**. It has no coin test, switch test,
DIP-switch page or high-score reset. It does contain the arcade-derived operator
pages: STATISTICS ("Idle Mins", "AVG TIME/COIN", three histograms and a clear),
the key/joystick test page (TEST CONTROLS, whose "ADDRESS RAW DATA" and "DIAGS"
strings sit at `0x80BAC`/`0x80BD4`) and "EEPROM ERROR" (`0x9F`).

### §0.3 Closure

**50 unported functions, 15 304 reachable instruction bytes.** Three are
Ghidra functions in `symbols.h`: `0x2F464`, `0x319B0` and `0x31A78`. Only
these three move `tools/port_progress.py`, so the count goes from 762 to
765 of 1203. The other 47 have no Ghidra function. Two more callees look
unported by header, and both are already ported. `0x38B18` is
`frontend_spawn_row` (`flow.c`, a `PORT: 0x38B18` header). `0x500BB` is the
tick read `DSD(DS_00101500)`, inline at every ported site. `0x1AE28` is
`config_keys_apply` (`config.c`, the "0x1AE20 (entry 0x1AE28)" header).

Legend for the callee column: `P` ported, `U` unported (in this closure), `D`
deferred (classification row), `R` WATCOM runtime (`>= 0x5D000`, not a target).

| addr | walked extent | bytes | callees |
|---|---|---:|---|
| `0x2CB74` | `2CB74..2CB91` | 29 | 2FFC4P |
| `0x2CB94` | `2CB94..2CBC1` | 45 | 1B084D 2C304P 2FA40P |
| `0x2CBC4` | `2CBC4..2CBDB` | 23 | — |
| `0x2CBDC` | `2CBDC..2CBF3` | 23 | — |
| `0x2CBF4` | `2CBF4..2CC0B` | 23 | — |
| `0x2CC0C` | `2CC0C..2CC23` | 23 | — |
| `0x2CC24` | `2CC24..2CC3B` | 23 | — |
| `0x2CC3C` | `2CC3C..2CC53` | 23 | — |
| `0x2CC54` | `2CC54..2CC6B` | 23 | — |
| `0x2F99C` | `2F99C..2F9CB` | 47 | 2BAF4P 38B18P 4F1D0P 4F1E4P |
| `0x2CC74` | `2CC74..2CCCD` | 89 | 2CD30U |
| `0x2CD30` | `2CD30..2CEFF` | 463 | 1C500P 2F198P 2F280P 655E4R |
| `0x2CF00` | `2CF00..2D2E3` | 995 | 1C500P 2CC74U 2CD30U 2EA74P 2EDE0P 2F198P 50146P |
| `0x2CACC` | `2CACC..2CAD9` | 13 | 33578U (tail `jmp`) |
| `0x33578` | `33578..336B9` | 321 | 1C500P 2CCD0P 2CF00U 2D974P 2DA0CP 2F198P 2F99CU 47370P |
| `0x30EB4` | `30EB4..30F51` | 157 | 1C500P 2C3FCP 2C9E8U 2CF00U 2F198P 2F99CU |
| `0x30F54` | `30F54..30FE6` + the shared tail `30F42..30F51` | 161 | 1C500P 2C3FCP 2C9CCU 2CF00U 2F198P 2F99CU |
| `0x2C9CC` | `2C9CC..2C9E7` | 27 | 2C3FCP |
| `0x2C9E8` | `2C9E8..2CA03` | 27 | 2C3FCP |
| `0x2F464` | `2F464..2F48A` | 38 | 2EFD4P 2F198P |
| `0x30728` | `30728..30787` | 95 | 1C500P 2EA74P 2EDE0P 2F198P 2F280P |
| `0x30864` | `30864..30EB4` | 1616 | 1C500P 1CAB8P 1CED4P 2C3FCP 2C8F0P 2C9B8P 2D974P 2DA0CP 2EA74P 2EDE0P 2F198P 2F388P 2F41CP 2F434P 2F464U 2F99CU 30728U 30788P 50146P |
| `0x30FE8` | `30FE8..31135` | 333 | 2F174P 2F280P 2F434P |
| `0x31138` | `31138..31404` | 716 | 1AE28P 1AEE0P 1C500P 2EA74P 2EDE0P 2F198P 2F99CU 30FE8U 50146P |
| `0x2EF48` | `2EF48..2EFC3` | 123 | — (hex digits table `0x2EF10` "0123456789ABCDEF") |
| `0x2F48C` | `2F48C..2F4BB` | 47 | 2EF48U 2F198P |
| `0x314A0` | `314A0..3157B` | 219 | 2F174P 2F388P |
| `0x319B0` | `319B0..31A68` | 184 | 1C500P 2F280P |
| `0x31A78` | `31A78..31B81` | 265 | 2F198P 3157CP |
| `0x31B94` | `31B94..31C68` | 212 | 1C500P 2F280P 2F314P |
| `0x31C78` | `31C78..31E09` | 401 | 2F198P 2F20CP 3157CP |
| `0x31F24` | `31F24..32358` | 1076 | 1AE28P 1AEE0P 1C500P 2EA78P 2EDE0P 2F174P 2F198P 314A0U 319B0U 31A78U 31B94U 31C78U 31E28P 500BBP |
| `0x32358` | `32358..3263E` | 742 | 1AEE0P 1C500P 2EA74P 2EB80P 2EDE0P 2F174P 2F198P 2F48CU 314A0U 31A78U 31C78U 31E28P |
| `0x19C60` | `19C60..19CF1` | 145 | — (jump table `0x19C20`) |
| `0x19D34` | `19D34..19DD8` | 164 | — (jump table `0x19CF4`) |
| `0x2EBBC` | `2EBBC..2EBCC` | 16 | — |
| `0x19DF0` | `19DF0..1A561` | 1905 | 19C60U 19D34U 1AE28P 1AEE0P 1C500P 2EA78P 2EBBCU 2F198P 3157CP |
| `0x328B8` | `328B8..3291D` | 101 | 2F198P 2F464U |
| `0x32F54` | `32F54..32F95` | 65 | 2CA78P 2D974P |
| `0x32F98` | `32F98..33058` | 192 | 1C500P 2D974P 2F198P 2F464U 328B8U 32F54U |
| `0x33458` | `33458..3355D` | 261 | 1C500P 2D974P 2F198P 2F434P |
| `0x33058` | `33058..3322E` | 470 | 1C500P 2D974P 2EA74P 2EB80P 2EDE0P 2F198P 2F280P 2F434P 2F99CU 32F98U 33458U 52106P |
| `0x33230` | `33230..33455` | 549 | 1C500P 2D974P 2DA0CP 2EA74P 2EB80P 2EDE0P 2F198P 2F280P 2F434P 2F99CU 52106P |
| `0x2E218` | `2E218..2E246` | 46 | — |
| `0x2E11C` | `2E11C..2E17F` | 99 | 2D4ECP 61A70R |
| `0x2E248` | `2E248..2E5E1` | 921 | — |
| `0x2E5E4` | `2E5E4..2E933` | 847 | 2E218U 2EFD4P 61A70R |
| `0x32BDC` | `32BDC..32F53` | 887 | 1C500P 2E11CU 2E248U 2E5E4U 2EA74P 2EB80P 2EDE0P 2F198P 2F434P 2F99CU 52106P |
| `0x33560` | `33560..33578` | 24 | 32BDCU 33058U 33230U |
| `0x2CAC0` | `2CAC0..2CACA` | 10 | 33560U (tail `jmp`) |

**Callers (raw `call rel32` scan and absolute dwords).** The 18 table callbacks
have no rel32 caller. Their only references are the table dwords, at `+8` of the
entries above. `0x2CAC0` and `0x2CACC` are reached only through their table
dwords `BCC44`/`BCC34`. `0x33560` and `0x33578` are reached only by those two
tail jumps. Every other function is called only from inside this closure:
`0x2F99C` from 9 sites, `0x2F464` from 5, `0x2CC74` from 4, `0x2CD30` from 5,
`0x2CF00` from 3, `0x19D34` from 12+, `0x30FE8`, `0x314A0`, `0x31A78` and
`0x31C78` from 4 each, and the rest from 1 or 2. `0x19DD8` (`bb 0x100CD4; call
0x19D34; call 0x3157C; ret`) has no reference at all: no rel32 and no absolute
dword. It is dead code and gets no port and no row, as in §K9.13.

### §0.4 Call graph

```
case 0x27 (flow.c) -> menu_step(0xBCBDC,0x10,4)
  Start        0x2CB74 -> menu_step(0xBCCCC,0x10,0)   [re-initialises the 0x2FFC4 state onto START MENU]
                 START MENU items -> 0x2CBC4 0x2CBDC 0x2CBF4 0x2CC0C 0x2CC24 0x2CC3C 0x2CC54 (mode setters)
  GAME OPTIONS 0x2CB94 -> menu_run(0xBCC1C,0x10,0), then 0x1B084 (D, config save), 0x2C304 (P)
    CONFIG OPTIONS   0x2CACC -> 0x33578([[DS_0010740C]+4]) -> 0x2F99C, 0x2CCD0P, 0x2CF00, 0x2DA0CP, 0x47370P
    STATISTICS       0x2CAC0 -> 0x33560(1) -> 0x33058 (page 1) -> 0x33458, 0x32F98 -> 0x32F54, 0x328B8 -> 0x2F464
                                           -> 0x33230 (page 2, "clear ALL statistics")
                                           -> 0x32BDC (histograms) -> 0x2E248, 0x2E5E4 -> 0x2E218; 0x2E11C (clear)
    SOUND TEST       0x30EB4 -> 0x2F99C, 0x2CF00(0xA3500) -> 0x2CC74 -> 0x2CD30; 0x2C9E8 -> sound_voice
    MUSIC TEST       0x30F54 -> 0x2F99C, 0x2CF00(0xA3060), 0x2C9CC -> sound_voice
    MODIFY CONTROLS  0x31F24 -> 0x31E28P, 0x31C78, 0x31B94, 0x31A78, 0x319B0, 0x314A0, 0x2EA78P, 0x1AE28P
    CONFIGURE KEYBD  0x19DF0 -> 0x19D34, 0x19C60, 0x3157CP, 0x2EBBC, 0x2EA78P, 0x1AE28P
    TEST CONTROLS    0x32358 -> 0x31E28P, 0x31C78, 0x31A78, 0x314A0, 0x2F48C -> 0x2EF48
    ADJUST VOLUME    0x30864 -> 0x2F99C, 0x30728, 0x30788P, 0x2F464, sound_music/sfx_volume (P), 0x2C9B8P
    2 PLAYER HANDICAP 0x31138 -> 0x2F99C, 0x30FE8, 0x1AEE0P, 0x1AE28P
```

### §0.5 What each routine does (planner's reading; each task re-derives its own)

* **`0x2CB74`**: `menu_step(0xBCCCC, 0x10, ECX = 0)`. EBX = `0xF000`, which
  `0x2FFC4` ignores. Returns EAX. It runs inside the outer `0x2FFC4`'s Enter
  arm, which cleared `DS_00107414` at `0x30480`. So the inner call initialises
  the one shared state (`DS_00107414..0x10744C`) for START MENU, and the
  following frames' `menu_step(0xBCBDC)` keep driving START MENU. The case-0x27
  table argument is only read at initialisation.
* **`0x2CB94`**: `menu_run(0xBCC1C, 0x10, 0)`, then `0x1B084` (the CMOS config
  writer, **deferred**, classification row `1B084`, record §50-C), then
  `0x2C304` (`config_credits_init`). It returns `menu_run`'s result.
* **`0x2CBC4`..`0x2CC54`**: `push edx; mov edx, M; mov ah, N` (the first two
  use `xor ah, ah`); `mov word [0x104B00], dx; mov byte [0x104B1D], ah; pop
  edx; ret`. EAX comes back as the entry address with AH replaced. It is never
  -5 or -10, so `0x2FFC4` returns 0.
* **`0x2F99C`**: the menus' screen reset: `0x4F1E4(0)`, `0x2BAF4(1)`,
  `0x4F1D0`, `0x38B18(0x9AD84)`. These are the same four calls that open
  `0x2FE84`.
* **`0x2CC74`** (EAX table, EDX value bits, EBX first index, ECX mode,
  `[esp+4]` release flag, `ret 4`): a negative index returns 0. Otherwise it
  draws the option rows from `table + 0x14*index` through `0x2CD30` on rows
  3, 6, .. `0x16`, and stops early when `0x2CD30` returns 0.
* **`0x2CD30`** (EAX option record, EDX bits, EBX row, ECX mode, `[esp+4]`
  release flag, `ret 4`): one option row. The record is `+0` the string id,
  `+4` the shift, `+8` the value count, `+0xC` a byte (draw the value's index),
  and `+0x10` the value list of 8-byte `{char *text, u32 string id}` entries.
  The value is `(bits >> shift) & (2^ceil(log2 count) - 1)`, and an
  out-of-range value returns 0. The label goes at column 4. A value text
  starting `*` switches the mode to `0x1000`. The optional index uses runtime
  `itoa(value + 1, buf, 10)` (`0x655E4`). Then come the text and the string. It
  returns `record + 0x14`.
* **`0x2CF00`** (EAX table, EDX bits, EBX mask, CL Esc-cancels; blocking): a
  scrolling editor for bit-packed option values. It shows "+ MORE +" (`0x72`)
  when there are more than 6 rows. Up and down move, left and right cycle the
  value, and `input_repeat_set(0xF000F000, 0x1E, 0xF)` sets the key repeat. Enter
  returns the bits. Esc returns -1 with CL set, and the bits otherwise. Each
  loop pass calls `0x2EA74`. It reads the code-object dword `[0x2CC70]` =
  `0x80A18` (8 spaces).
* **`0x2CACC`**: `0x33578([[DS_0010740C] + 4])`. In the raw, `0x2F9CC`
  (`0x2FA01`) stores `DS_0010740C = 0x1D2D0`, and `[0x1D2D4] = 0xA2EB4` is the
  CONFIG OPTIONS option table (10 records: `0x46`, `0x1F3`, `0x1F4`, `0x1F6`,
  `0x1FA`, `0x1FD`, `0x1FE`, `0x201`, `0x204`, `0x205`). **The port does not
  make that store** (record §49-X.6 names it). Task 2 adds it with the
  `0x2FA10..0x2FA1C` store `DS_00107410 = 0x2D974(0x2A) & ~3`, which
  `0x32358` reads (`0x32363`).
* **`0x33578`** (EAX table; 0 selects the code-object `0x32700`, which never
  happens in the raw): draws the screen, reads field `0x29`, and defaults it
  through `0x2CCD0` when bit 15 is set. It runs `0x2CF00(table, bits,
  0x4000000, 0)` and writes back field `0x29`. When bits 24..26 (the language)
  changed, it calls `0x47370(new >> 24)`. That is the language reload. In the
  port `game_string_table_load` is idempotent and reads English only, so that
  part is a `PORT:` note and a named gap.
* **`0x30EB4` / `0x30F54`**: the SOUND TEST (`0x20F`, option table `0xA3500`,
  "Samples" `0x207`, 143 values) and MUSIC TEST (`0x214`, table `0xA3060`,
  "Music Tunes" `0x206`, 26 values) screens. Each loops `r = 0x2CF00(table,
  prev, 0x4000000, 1)` until r is -1, playing each r through `0x2C9E8` or
  `0x2C9CC`, then ends with `sound_voice(0x100)`. The value lists `0xA3088`
  and `0xA2F90` are all zero in the image, and their only reference is their
  own option record. So only the index number is drawn. `0x30F54` jumps into
  `0x30EB4`'s tail `0x30F42..0x30F50`, and the C port repeats the tail in each
  function.
* **`0x2C9E8` / `0x2C9CC`**: `sound_voice(0x100)` then
  `sound_voice(DSD(0xBC9A0 + 4*i))` (samples) or `DSD(0xBC938 + 4*i)` (tunes).
  The tune ids are `0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x23,..`. Record
  `0xBBDC8 + 12*0x1B` is case 1 (music).
* **`0x2F464`** (EAX value, EDX width, EBX pad, ECX mode): `0x2EFD4` into a
  `0x14`-byte stack buffer, then `0x2F198(0, -1, buf, mode)`. This is a number
  drawn at the cursor.
* **`0x30728`**: draws "ERROR SETTING VOLUME LEVEL" (`0x7A`) on row 6 in mode
  `0xB000`. It loops `0x2EA74` until `0x2EDE0(0, 1)` has bit `0x2000000` (Esc),
  then releases the cells.
* **`0x30864`**: ADJUST VOLUME. Bars through `0x30788`, the "0 1 2 .. 11"
  scale (`0x7B`) and MUTE (`0x7E`). The row layout bytes are the packed record
  `0xBD441..0xBD458` (unaligned dword reads). The byte `0xBD458` is written at
  `0x30E25`/`0x30E49`. Up/down pick music, effects or voice; left/right adjust.
  It uses `sound_music_volume`, `sound_sfx_volume` and `sound_voice`.
  `0x2C8F0(-1)` returns EDX, which is field `0x35` as read at `0x2C902`
  (`0x2C9B1 mov eax,edx`). When that is negative (unset, -1), `0x308CC` runs
  the `0x30728` error and the function returns -1. The port's
  `attract_config_volumes_unscaled` is `void` today (see plan Task 4). It
  saves through `0x2DA0C` and `0x2C9B8`.
* **`0x30FE8`** (value, row): one handicap bar: a number (`0x2F434`) and
  glyphs (`0x2F174`). Four `0x2F280` releases clear the row first.
* **`0x31138`**: 2 PLAYER HANDICAP (`0x1F2`). `0x1AEE0` packs the key-config
  record, then the loop edits the two handicap values (layout `0xBD456`/`0xBD457`).
  On exit it stores `DS_00107468`/`DS_0010746C` (`0x313CE`/`0x313D7`) and
  `0x1AE28` applies the record.
* **`0x2EF48`** (value, buf, width, pad flag): hex formatting. It uses the digits
  at `0x2EF10`, pads with `0x20` when the flag is set and `0x30` when it is
  clear, and returns `width - digits`. **`0x2F48C`** draws it through `0x2F198`
  (`ret 8`).
* **`0x314A0`** (col, row, pad bits): a 3x3 stick indicator. The direction bits
  `0x20002000/0x10001000/0x80008000/0x40004000` select a glyph cell.
* **`0x319B0` / `0x31A78`**: clear or draw the four button-key names (the
  `0x22C` blanks, then `0x3157C` names) at the diamond positions around (col,
  row). They dispatch through jump tables `0x319A0`/`0x31A68`, and EAX picks
  the player side (`+0xA` / `+0x1C`).
* **`0x31B94` / `0x31C78`**: the same for the four direction keys, jump tables
  `0x31B84`/`0x31C68`. Side 0 is column `0xA`, side 1 is column `0x1E`.
* **`0x31F24`**: MODIFY CONTROLS. `0x1AEE0` packs the record. Each player's
  device word `[DS_00101514]+0x2D4`/`+0x2D6` (keyboard, 4-button or 2-button
  joystick, `0x31E28`) cycles with left/right. Key names are drawn per device.
  A blink is timed from the tick `0x500BB`. It uses `0x2EA78(n)` and exits on
  Esc, then `0x1AE28` applies the record.
* **`0x32358`**: TEST CONTROLS. It is the same device/key layout without the
  editing. It shows the live stick and buttons (`0x314A0`, `0x31C78`,
  `0x31A78`) and draws hex values through `0x2F48C`. It reads `DS_00107410`
  (`0x32363`). It exits on the latched Esc (`0x2EB80`, `0x3254B cmp 0x1B`).
* **`0x19C60` / `0x19D34`** (slot 0..15): set or get one key word of the
  record at `0x100CAC`. A slot above 15 does nothing, and the getter returns 0.
  The slot-to-address map is the same for both: 0 `0x100CAE`, 1 `0x100CB4`,
  2 `0x100CB0`, 3 `0x100CB2`, 4 `0x100CB6`, 5 `0x100CB8`, 6 `0x100CBA`,
  7 `0x100CBC`, 8 `0x100CC0`, 9 `0x100CC6`, 10 `0x100CC2`, 11 `0x100CC4`,
  12 `0x100CC8`, 13 `0x100CCA`, 14 `0x100CCC`, 15 `0x100CCE`.
* **`0x2EBBC`**: returns `DS_00105F28` (the raw key word `config_screen_wait`
  stores at `0x2EB3F`) and zeroes it.
* **`0x19DF0`**: CONFIGURE KEYBOARD. It packs into `0x100CAC`, draws 16 slots
  (`0x19D34` + `0x3157C` into `0x100CD4`), and waits for keys through
  `0x2EA78` + `0x2EBBC`. Esc (`0x1A3A1`) and Enter (`0x1A3AA`) are handled.
  Each other key is stored with `0x19C60`. `0x1AE28` applies the record.
* **`0x328B8`** (secs, w): `0x2F464(secs / 60, (w & 0xFFFF) - 3, 1, 0xF000)`,
  `":"` (`0x80BDC`) at the cursor, then `0x2F464(secs % 60, 2, 0, 0xF000)`.
* **`0x32F54`**: 0 when `0x2CA78()` is 0. Otherwise `(60 * (2*field5 + field4))
  / (c & 0xFFFF)`, unsigned.
* **`0x32F98`**: "Percentage Play" (`0x8A`) and "AVG TIME/COIN" (`0x8B`) rows,
  through `0x2F464`, `0x32F54` and `0x328B8`.
* **`0x33458`** (row): draws the fields of the code-object table `0x326C4`.
* **`0x33058`**: STATISTICS page 1 (`0x81`). Its rows come from the code-object
  table `0x32644`: `{0x8F "Idle Mins", 3}`, `{0x90 "1 Player Mins", 0x12}`,
  `{0x91, 0x13}`, `{0x92, 0xA}`, `{0x93, 0xC}`. Fields `0xA`, `0xC`, `0x12` and
  `0x13` are divided by 60. The `field == 0x24 && value > 0x4B` "EEPROM ERROR"
  arm is dead with this table. Esc or Enter (the latched key) leaves.
* **`0x33230`**: page 2 "MORE STATISTICS" (`0xAA`, table `0x32674`). The
  held-Esc + Enter combination ("HOLD LEFT PL UPPER LEFT AND PRESS .. UPPER
  RIGHT", `0x69`/`0x6A`) clears the statistics fields through `0x2DA0C`.
* **`0x32BDC`**: page 3, the histograms (MEDIAN `0x83`, TOTAL `0x84`, "for next
  histogram" `0x87`, "CLEARING ALL HISTOGRAMS" `0x88`) over the three audit
  descriptors `0x2D414` (stride 0x10: "Round Time", "Match Time", "Selects Per
  Character"). It uses `0x2E248`, `0x2E5E4` and the clear `0x2E11C`.
* **`0x2E218`**: the decimal digit count of a signed value, 1..10.
* **`0x2E11C`** (group): -1 when group is 3 or more. Otherwise it sets the dirty
  bit `DSB(0x105DD8 + (g+3)/8) |= 1 << ((g+3) & 7)` and zeroes the counter table
  at descriptor `0x2D444 + 8*(g+3)`: `+2` size word, `+4` pointer (`0x105ECD`/20,
  `0x105EE1`/20, `0x105EF5`/7), through the runtime memset `0x61A70`. It then
  calls `0x2D4EC(g + 3)` (`config_storage_touch`, the declared no-op) and
  returns 0.
* **`0x2E248` / `0x2E5E4`**: format a histogram and a histogram line into the
  state block `DS_00105D64` from a descriptor's template text (tabs separate the
  columns, `"#:"` `0x80B20` is the default label) and counters. `0x2E5E4` uses
  `0x2E218`, `0x2EFD4` and the runtime memset.
* **`0x33560`** (EAX): `0x33058(a)`, `0x33230(a)`, `0x32BDC(a)`.
  **`0x2CAC0`**: `mov eax, 1; jmp 0x33560`.

### §0.6 Host state, storage and hardware arms

No arm reads a DIP switch, a coin switch or the EEPROM hardware. The PC build
reads its settings from the config fields (`0x2D974`/`0x2DA0C`, ported) and the
key-config record. The dependencies on host-modelled state are these:

| dependency | where | port status | consequence |
|---|---|---|---|
| CMOS config save `0x1B084` | `0x2CBAF` | deferred (row `1B084`, §50-C) | `PORT:` note at the call; the settings live in `mem[]` for the session only |
| Language reload `0x47370` | `0x336AE` | host file I/O; the port's `game_string_table_load` is idempotent and English-only | `PORT:` note; named gap: a language change is stored in field `0x29` but not shown |
| Joystick devices | `0x31F24`/`0x32358` device words `+0x2D4/+0x2D6` | the port's input is keyboard-only (host-owned `0x4FF8F`/`0x4FFD8`/`0x5004A`, §K9.3/§K9.4) | ported faithfully; picking a joystick has no effect on the host (named gap) |
| Play-time fields `3,0xA,0xC,0x12,0x13` | `0x33058` | their writer, the run clock `0x32970`, is host-owned (`config_play_time_close` `PORT:`) | they stay 0 in the port, so page 1 shows 0 (named gap, existing) |
| Audit counters `0x105ECD..0x105EFB` | `0x2E248`/`0x2E5E4`/`0x2E11C` | writers `0x2E180`/`0x2E0A4`/`0x2E034` deferred (§K9.6–§K9.8) | they stay 0, so the histograms show 0. `0x2E11C`'s clear is ported: it writes `mem[]` and calls the ported no-op `0x2D4EC` |
| Runtime `itoa` `0x655E4`, `memset` `0x61A70` | `0x2CD30`, `0x2E11C`, `0x2E5E4` | WATCOM runtime (not a target) | `PORT:` notes; decimal formatting in C, `mem_fill` |

**Classification verdict: all 50 functions are `port`.** None is host-owned or
deferred. None does I/O. Each reads and writes `mem[]` and calls ported
functions. No `tools/port_classification.txt` row is added. The rows name only
Ghidra functions, and the three here are ported. `0x19DD8` is dead code and
gets no row (§K9.13 precedent).

### §0.7 Reachability and oracle exposure

Mode `0x27` is entered only by a real Enter key in mode 3 (`game_key_loop`,
record §55-A). No oracle driver queues a key. So none of the 50 functions is
reached by the front-end, demo-fight, attract, title or smk runs. Only two
changes in this plan touch a path the oracles run, and both are in
`game_init`. The first is the two `0x2F9CC` stores, `DS_0010740C` and
`DS_00107410`. Their only readers are `0x2CACD`, `0x2F983` (gated on menu flags
bit 0, which no stock call sets) and `0x32363`. The second is `svcmenu_register`,
which appends 18 entries to `fn_table` (limit `FN_TABLE_MAX` 1300, `mem.c:18`).
Task 2 proves neither moves a frame. It byte-compares a `make check frames=8000`
dump before and after, and checks that every `make verify` oracle line is
unchanged.

## §K11.0 Baseline (executor, Task 1)

The baseline is green on `8466111` (branch `all-gaps`). `cmake --build build`
printed no warning or error. `make verify` exited 0 (`verify-exit=0`; log
`<scratchpad>/k11_t1_verify.txt`). These are its oracle lines as `orlines.sh`
extracts them (`<scratchpad>/k11_t1_or.txt`, sha256 `5852a1225ab8f3bf...`),
the comparison base for every later task. They agree with ledger §A. Four
lines longer than 400 characters (the title splice-byte lists and the
front-end splice-byte and missing lists) are shortened here to their prefix
and the sha256 of the full line; the full text is in the file.

```
oracle C-vs-Python: 9866 writes byte-exact
capture oracle first difference at C write 430: C tick=1010 reg=0xa0 val=0xd3 vs capture tick=1010 reg=0x1a8 val=0x68 (C 9866 writes, capture 6903 normalised)
all checks passed
oracle C-vs-Python: 9866 writes byte-exact
capture oracle first difference at C write 430: C tick=1010 reg=0xa0 val=0xd3 vs capture tick=1010 reg=0x1a8 val=0x68 (C 9866 writes, capture 6903 normalised)
all checks passed
smk_compare: 120/120 frames match
smk_compare: 41/41 frames match
all checks passed
title_compare: capture 1: window distinct [216..326] (raw 2198..2308)
title_compare: capture 1: 111 frames in window: 54 clean, 55 splice, 2 transition, 0 unexplained
title_compare: capture 1: splice bytes [55 frames, 55 distinct]:  ... (459 chars, sha256 614f56c8c579f4d8)
title_compare: capture 1: transition rows (N, row, from_N, from_N+1) [2 frames]: ['port87@row7(28/73)', 'port90@row76(322/273)']
title_compare: capture 1: port frames exhibited 95/96; missing [0]; endpoints OK
title_compare: capture 2: window distinct [216..326] (raw 2193..2303)
title_compare: capture 2: 111 frames in window: 54 clean, 57 splice, 0 transition, 0 unexplained
title_compare: capture 2: splice bytes [57 frames, 57 distinct]:  ... (472 chars, sha256 2d9ed45f9104492f)
title_compare: capture 2: transition rows (N, row, from_N, from_N+1) [0 frames]: none
title_compare: capture 2: port frames exhibited 95/96; missing [0]; endpoints OK
title_compare: determinism: clean samples of 54 port frame(s) agree, 0 disagree
all checks passed
title_compare: frontend: window distinct [560..1884] (raw 3108..4791)
title_compare: frontend: 1325 frames in window: 517 clean, 801 splice, 3 transition, 2 unexplained
title_compare: frontend: splice bytes [801 frames, 801 distinct]:  ... (6118 chars, sha256 a94212acd7d58794)
title_compare: frontend: transition rows (N, row, from_N, from_N+1) [3 frames]: ['port832@row177(235/33)', 'port862@row189(148/478)', 'port906@row31(101/343)']
title_compare: frontend: port frames exhibited 1145/1382; missing [1, 2, 6, 12, 17, 23, 29, 35, 41, 47, 53, 59, 65, 127, ... (1249 chars, sha256 57cde3464bb284fa)
title_compare: frontend: 16 all-black capture frame(s) excluded as artifacts: [(0, 1367), (217, 2173), (357, 2420), (390, 2529), (424, 2637), (458, 2746), (491, 2855), (525, 2963), (561, 3109), (831, 3670), (1885, 4793), (2095, 5401), (2133, 5807), (2383, 6758), (3406, 7846), (3543, 8247)]
title_compare: frontend: 2 unexplained captured frame(s) allowed by name: [(832, 3671), (833, 3740)]; no other unexplained frame in the window.
all checks passed
title_compare: demo-fight: no port frames after the front-end window
title_compare: demo-fight: front-end window distinct [560..1884]; fight window empty: the front-end window reaches the first all-black capture frame 1885 (raw 4793)
title_compare: demo-fight: 0 unexplained in the fight window
title_compare: demo-fight: fully explained; the window claim is now exact
title_compare: attract2: front-end window distinct [560..1884]; cycle-2 region [1885..3616] (raw 4793..8409); cycle-2 dump 2308 frames
title_compare: attract2: exhibited window distinct [1886..3616] (raw 4795..8409)
title_compare: attract2: 1732 frames in region: 1078 clean, 630 splice, 17 transition, 1 unexplained, 6 all-black
title_compare: attract2: captured frame 3545 (raw 8338) allowed by name as a three-frame splice: cycle-2 2192/2193/2194 at bytes 120000/172800 (rows 125/180)
title_compare: attract2: 0 unexplained in the region
all checks passed
attract_compare: data/title-captures/title: attract window [0..215]; title window starts at capture 216 (raw 2198)
attract_compare: data/title-captures/title: 215/216 capture frames explained; port attract frames exhibited 173/690
attract_compare: data/title-captures/title: FIRST DIVERGENCE at capture frame 215 (raw 2180)
attract_compare: data/title-captures/title:   best byte splice port0[0..0) ++ port1[0..192000) still differs at 498 byte(s) (first row 192 byte 184320)
attract_compare: data/title-captures/title: expected divergence at capture frame 215
attract_compare: data/title-captures/title2: attract window [0..215]; title window starts at capture 216 (raw 2193)
attract_compare: data/title-captures/title2: 215/216 capture frames explained; port attract frames exhibited 173/690
attract_compare: data/title-captures/title2: FIRST DIVERGENCE at capture frame 215 (raw 2175)
attract_compare: data/title-captures/title2:   best byte splice port0[0..0) ++ port1[0..192000) still differs at 498 byte(s) (first row 192 byte 184320)
attract_compare: data/title-captures/title2: expected divergence at capture frame 215
Ran 10 tests in 0.099s
OK
Ran 33 tests in 1.102s
OK
== symbols.h must regenerate byte-identically ==
python3 tools/gen_symbols.py port/decomp port/src/symbols.h
1304 globals, 1206 functions -> port/src/symbols.h
  dropped 11 globals, 1 functions outside the LE objects
all checks passed
```

`python3 tools/port_progress.py` prints:

```
762 1203 63
726 730 99 (portable: excludes 82 host-owned/deferred and runtime >= 5D000)
```

(the plan's `726 731 99` is the older tree; the tree's value is `726 730`.)

The frame baseline is `make check frames=8000` on the same build
(`check-exit=0`); its 8000 `frames/frame_*.idx` and 8000 `frame_*.pal` files
are copied to `<scratchpad>/k11_frames_before/` (the `.ppm` is the `.idx`
through the `.pal`, and is not kept, for disk space).

**§0 re-checked.** `le.py` rebuilt `k11_img.bin` byte-identical to
`img_rev.bin`. `k11_closure.py` prints `TOTAL 53 functions 15577 bytes`, which
includes the three already-ported `0x38B18`, `0x500BB` and `0x1AE28`, and its
output is identical to the planner's `k11_closure.txt`. `k11_tab.py` prints 50
rows summing to 15 304 bytes. **§0 re-checked: 50 functions / 15 304 B**, no
correction.

One addition to §0.2 from the fixed-up image: the title entry `0xBCCCC` of the
START MENU holds string `0x215` "START MENU" (its `+4`/`+8`/`+0xC` are 0), and
the three tables end at the zero entries `0xBCC0C`, `0xBCCBC` and `0xBCD4C`.
The START MENU items all have `+0xC = 1`; the title entries `0xBCBDC`,
`0xBCC1C` and `0xBCCCC` have no menu-level callback (`+8 = 0`).
