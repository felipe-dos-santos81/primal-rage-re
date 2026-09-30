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

## §K11.1 The test seams (executor, Task 2)

No raw counterpart; both are test infrastructure.

* **`host_set_pump_hook`** (`host.h`/`host.c`, a `PORT:` test seam). When a
  hook is set, `host_pump()` calls it once at the end of every call. NULL is
  the default and nothing outside `run_tests` sets it, so the game and the
  oracle runs are unaffected (the frame proof of §K11.2 covers this). The
  blocking menu loops reach the host only through `config_screen_wait`'s
  `host_wait_vblank()`.
* **`tf_menu_press`** is `test_platform.c`'s `mt_press`, moved byte for byte to
  `test_fixtures.c` with `MT_LAYOUT` (`0x3E2D000`, into `test_fixtures.h`). Its
  34 call sites were renamed. `grep -c CHECK port/tests/test_platform.c` is 819
  before and after, so no assertion changed.
* **The harness** (`test_game.c`, `sm_*`). `sm_hook` feeds one scripted step
  (a BIOS key `(scan << 8) | ascii` through `input_push`, and a pad word through
  `tf_menu_press`) on the first pump after `DS_000E87A0` changes. That is once
  per presented frame, because `config_screen_wait` swaps the buffers at
  `0x2EAD6` before its tick waits. Past the script's end it feeds Esc, then
  Esc+Enter alternately, and counts each one as an extra frame. After 600
  extra frames it exits the process. `sm_end(n)` asserts exactly `n` frames
  and no extra ones.
* **Hardening (correction to the plan's mutation 8).** With the hook call
  removed from `host_pump`, the seam checks do fail, but the next scripted
  blocking run (`0x2CB94`'s `menu_run`) then never sees its Esc. The harness's
  own exit guard lives in the hook, so the suite hung: the first mutation-8 run
  was killed, and its failures were lost in the unflushed pipe. `sm_check_seam`
  now sets `sm_seam_ok` only when both latches and the frame count are right.
  `sm_begin` stops the process with a `FAIL` line when the flag is clear, and
  the seam check itself uses the unguarded `sm_begin_raw`. After this change,
  mutation 8 fails with 3 check failures and the guard's exit.
* **Frame budget.** This task scripts 3 hooked frames: 2 for the seam and 1 for
  GAME OPTIONS. Five more frames are presented, one by each `menu_step`
  initialisation in the START checks (`0x2FFF1`). `run_tests` takes 5.9 s in all.

## §K11.2 Cycle 1: the shell (executor, Task 2)

Ten functions, all re-read from the fixed-up image (`k11_dx.py`):

| addr | raw | port |
|---|---|---|
| `0x2CB74` | `mov ebx,0xF000; mov edx,0x10; mov eax,0xBCCCC; xor ecx,ecx; call 0x2FFC4` | `svc_start_menu`: `menu_step(0xBCCCC, 0x10, 0)` |
| `0x2CB94` | `... mov eax,0xBCC1C; xor ecx,ecx; call 0x2FA40; mov edx,eax; call 0x1B084; call 0x2C304; mov eax,edx` | `svc_options_menu`: `menu_run(0xBCC1C, 0x10, 0)`, `PORT:` for `0x1B084` (deferred row `1B084`), `config_credits_init`, returns `menu_run`'s result |
| `0x2CBC4`..`0x2CC54` | `push edx; mov edx,M; xor ah,ah` or `mov ah,N`; `mov [0x104B00],dx` (word); `mov [0x104B1D],ah`; `pop edx; ret` | seven setters, M/N = `2D/0 2E/0 28/1 29/1 2A/2 2B/3 2C/4`; EAX returns the entry with AH replaced |
| `0x2F99C` | `0x4F1E4` (EAX 0, which it overwrites with 0x2700 at `0x4F1E5`), `0x2BAF4` (EAX 1, EBX = ECX = 0), `0x4F1D0` (EAX = EDX = 0), `0x38B18(0x9AD84)` with EDX = EBX = 0 (both preserved across the calls) | `svc_screen_reset`: the same four calls as `menu_title_draw` opens with (`frontend_input_reset`, `actors_reset`, `frontend_origin_zero`, `frontend_spawn_row(0x9AD84, 0, 0)`) |

All nine table callbacks are registered by `svcmenu_register()`, which is
idempotent (a static guard, because `fn_register` appends unconditionally).
`game_init` calls it once. `0x2CACC` and `0x2CAC0` are left for their own cycles.

**The two `0x2F9CC` stores** (`game_init`, next to `config_validate`): the store
`0x2FA01 mov dword [0x10740C], 0x1D2D0` comes before `0x2FA0B call 0x2D6F8`.
Then `0x2FA10 mov eax,0x2A; call 0x2D974; and al,0xFC; mov [0x107410],eax`. The
image has `[0x1D2D0..] = 0, 0xA2EB4, 0x1D2C9, 0x1D2C0`, so `[0x1D2D4]` is the
CONFIG OPTIONS table `0xA2EB4` (and `[+0xC] = 0x1D2C0` is the string
`menu_debug_lines` reads). **Correction:** §0.5 cites the `DS_00107410` reader
as `0x32363`, but the instruction is at `0x32361`
(`mov ebp,[0x107410]; and ebp,0x10`); `0x32363` is its displacement.

**Values the tests pin, checked against the raw:**

* The MAIN MENU table ends at `0xBCC0C`, so the walk counts 2 items. In the
  START MENU (`0xBCCDC..0xBCD3C`, 7 items, ending at `0xBCD4C`) no string
  starts with `?`, so two Downs (`0x3055E..0x305BE`: `cur+1`, compared against
  `DS_00107424 = 7`, then `0x2FE40`) select item 2, `0xBCCFC`, whose `+8` is
  `0x2CBF4`.
* Enter (`0x3046F`): `0x30480` clears `DS_00107414`. `0x304A2 call [edx+8]`
  runs with EAX = the selected entry. `0x2CB74`'s inner `0x2FFC4` sees the
  cleared byte and re-initialises the one shared state onto `0xBCCCC`:
  `DS_0010741C = 0xBCCDC`, `DS_00107418 = ECX = 0`. Its result 0 passes
  `0x304AA` (not -10) and `0x304B3` (not -5), which gives `0x305E7`, result 0.
* `0x2CBF4` returns `0x000BCCFC` with AH = 1, which is `0x000B01FC`. That is
  neither -10 nor -5, so `0x2FFC4` returns 0 with `DS_00107414 = 0` (from
  `0x30480`).
* **The Esc from START MENU returns -5, not -1** (this corrects the plan's
  Review Focus, which says -1; the plan's Task 2 test text already says -5).
  The nested state has flags 0, so `0x30430 test [0x107418],4` falls to
  `0x30449`. `DS_0010742C = 0` is not `DS_00107424 = 7`, so the result is
  `0x30466 mov eax,-5`, with `0x30456` clearing `DS_00107414`. The next
  `menu_step(0xBCBDC, 0x10, 4)` re-initialises MAIN MENU (`DS_0010741C =
  0xBCBEC`, flags 4). Case `0x27` treats -5 like 0 (`0x251F5..0x251FC`).
* GAME OPTIONS with an immediate Esc: `0x2FA40` with flags 0 returns
  `(cur == count) - 1 = -1` (`0x2FD53..0x2FD60`) after one presented frame
  (`0x2FA61`). Then `0x2C304` sets `DS_00105C00 = ((field 0x29 & 0xF0000) >> 16)
  + 1`.

**Tests:** `test_svcmenu` (registered after `test_cfg_helpers`, whose one
`actors_init()` it needs) runs `sm_check_seam` and `sm_check_shell`. It saves
and restores the tick model, the key state, `DS_00107414..0x10744F` and
`DS_00105C00`.

**Mutations** (each one rebuilt, run and reverted; log `<scratchpad>/k11_t2_mutations.txt`):

| # | mutation | result |
|---|---|---|
| 1 | `0x2CBF4` stores mode 0x29 | caught (2 checks) |
| 2 | `0x2CBF4` stores AH = 0 and returns AH = 0 | caught (3: `DS_00104B1D` twice and EAX) |
| 3 | the `0x2CBC4` store becomes a dword | caught (the `+2` sentinel) |
| 4 | the `0x2CB74` registration is dropped | caught (7) |
| 5 | `svc_start_menu` passes flags 4 | caught (`DS_00107418`, and the Esc result becomes -1) |
| 6 | `config_credits_init` is dropped | caught (the credits check) |
| 7 | `frontend_spawn_row` is dropped | caught (the backdrop check) |
| 8 | the hook call is removed from `host_pump` | caught (3 seam checks, then the `sm_seam_ok` guard exits; see §K11.1) |

`game_init`'s stores and `svcmenu_register()` are not reachable from `run_tests`
(`game_init` runs only in the env-gated drivers). The frame proof below shows
they move nothing.

**Unobservable** (added by all-gaps Task 7 from the Task 2 review):
`svc_screen_reset`'s `frontend_origin_zero()` (`0x4F1D0`, zeroing
`DS_00107A3A`/`DS_00107A38`) runs right after `actors_reset` (`0x2BAF4`), whose
`0x4F228` call at `0x2BBC4` has already zeroed both words, so dropping it fails
no check. The raw makes both calls, and so does the port.

**Gate (Task 2).** `make verify` exited 0. Its oracle lines equal §K11.0's,
apart from the two unittest wall-clock lines (`Ran 10 tests in 0.097s`,
`Ran 33 tests in 1.082s`), which are not oracle claims. `make check
frames=8000` was run after the change and compared with `cmp` against the
§K11.0 baseline: 8000 `.idx` and 8000 `.pal`, 16000 files, 0 differ. So the
two `0x2F9CC` stores and `svcmenu_register()` in `game_init` move no frame.
The header grep (`/* 0xADDR`) counts 1 for each of the ten addresses. The
`svcmenu.h` declaration comments put the address last (`... — 0x2CB74.`) so
that the grep counts only the definition. `tools/port_progress.py` is
unchanged at `762 1203 63` / `726 730 99`: none of the ten is a Ghidra
function (§0.3).

## §K11.3 Cycle 2: the option editor and three screens (executor, Task 3)

Nine functions, re-read from the fixed-up image (`k11_dx.py`, dump
`<scratchpad>/k11_t3_dis.txt`); data from `<scratchpad>/k11_t3_data.py`.

**The option record** (tables `0xA2EB4`, `0xA3500`, `0xA3060`; stride 0x14,
ended by a zero `+0`): `+0` the label string id, `+4` the shift (only CL is
used: `0x2CD62 mov cl,[esi+4]`, `0x2D208`), `+8` the value count, `+0xC` a
byte (draw the 1-based index), `+0x10` the value list of 8-byte `{char *text,
u32 string id}` entries.

The CONFIG OPTIONS table `0xA2EB4` (10 records, terminator `0xA2F7C`):

| rec | id | shift | count | byte | list | non-zero entries |
|---|---|---|---:|---|---|---|
| 0 `A2EB4` | `0x46` CREDITS | `0x10` | 10 | 1 | `A2CEC` | 4 `{0x8088C "*", 0}` |
| 1 `A2EC8` | `0x1F3` | `0x14` | 4 | 0 | `A2D3C` | "1", "*3", "5", "7" |
| 2 `A2EDC` | `0x1F4` | 0 | 11 | 0 | `A2D5C` | "30".."80" step 5, each `0x1F5`; default 5 "*55" |
| 3 `A2EF0` | `0x1F6` Difficulty | 4 | 16 | 1 | `A2DB4` | 0 `{0, 0x1F7}`, 9 `{"*", 0x1F8}`, 15 `{0, 0x1F9}` |
| 4 `A2F04` | `0x1FA` | 8 | 2 | 0 | `A2E34` | `{"*", 0x1FB}`, `{0, 0x1FC}` |
| 5 `A2F18` | `0x1FD` | `0x18` | 6 | 0 | `A2E44` | "*English" .. "portugu\x88s" (the language, bits 24..26) |
| 6 `A2F2C` | `0x1FE` | `0xA` | 2 | 0 | `A2E74` | `{"*", 0x1FF}`, `{0, 0x200}` |
| 7 `A2F40` | `0x201` | `0xD` | 2 | 0 | `A2E84` | `{0, 0x203}`, `{"*", 0x202}` |
| 8 `A2F54` | `0x204` | `0xE` | 2 | 0 | `A2E94` | `{"*", 0x203}`, `{0, 0x202}` |
| 9 `A2F68` | `0x205` "Restore Factory Default?" | `0xF` | 2 | 0 | `A2EA4` | `{"*", 0x203}`, `{0, 0x202 "Yes"}` |

Their `'*'` entries give `0x2CCD0(0xA2EB4)` = `4<<16 | 1<<20 | 5 | 9<<4 |
1<<13` = **`0x142095`**. SOUND TEST `0xA3500` is one record `{0x207
"Samples", 0, 143, 1, 0xA3088}` and MUSIC TEST `0xA3060` one record `{0x206
"Music Tunes", 0, 26, 1, 0xA2F90}`; both lists are all zero, so only the
index is drawn.

**Corrections (raw wins).**

* **Field `0x29` is a 32-bit field**, not "bits 0..26": its descriptor
  `[0x2D300 + 0x29*4]` = `0x1D980` gives width `((d >> 14) & 7) + 1` = 8
  nibbles, bit position 102 and no trailing byte. The table above uses bits
  0..8, 10, 13..21 and 24..26.
* **Bit 15 is itself an option**: record 9, "Restore Factory Default?"
  (shift `0xF`, count 2). `0x33578` honours it twice: before the edit
  (`0x335BE..0x335D3`) and **after** it (`0x33674..0x33695`, which §0.5 does
  not mention), so choosing "Yes" and Enter writes `0x142095`.
* **The `mask` key is a pad button, not a BIOS key.** `0x2CF00` polls
  `0x2EDE0(0xF300F000, 1)`; `0x50161` returns the unmasked level for every bit
  outside the mask, so `0x4000000` (byte `+0x2D8` bit 2 of the key record) is
  seen while held. Its branch `0x2D038..0x2D065` releases the window with the
  result bits, puts the working bits back to the entry bits (`[esp+0x1C]`),
  sets the index to 0 and forces the redraw. Strings `0x6E`/`0x71` on the
  CONFIG screen name it: "Press LEFT PL LOWER LEFT button / to RESTORE old
  Setting".
* `0x2CC74`'s rows are 3, 6, .. **21** (`0x2CCC2 cmp ebx,0x16; jle`, so 24 ends
  the walk): seven rows, one more than the window of 6 below the current top
  (§0.5 said "rows 3, 6, .. 0x16").
* SOUND/MUSIC TEST pass the **whole** result of `0x2CF00` to `0x2C9E8`/`0x2C9CC`
  (`0x30F32 mov edx,eax`); it equals the value only because both records have
  shift 0.

**`0x2CC74`** (EAX table, EDX bits, EBX first, ECX mode, `[esp+4]` release,
`ret 4`): `jge` at `0x2CC7D` returns 0 for a negative first; `0x2CC8F` returns
0 when the first record is empty; otherwise ESI = 3 and each pass calls
`0x2CD30(rec, bits, (u16)si, mode, release)`, stops with 0 when it returns 0
(`0x2CCB8`), and adds 3. It returns the last `0x2CD30` result. The `0x2CC8F`
test repeats `0x2CD30`'s own `0x2CD6A` test; the redundancy is unobservable.

**`0x2CD30`** (EAX record ESI, EDX bits, EBX row `[esp+8]`, ECX mode EBP,
`[esp+0x28]` = the stack release flag, `ret 4`). Mask: EAX = 2, ECX = 1; while
`count > EAX` (signed, `0x2CD4B jle` / `0x2CD55 jl`) EAX doubles and ECX
counts; mask = `(1 << cl) - 1` (`0x2CD65 dec eax`). CREDITS' 10 gives 4 bits
(15). `v = (bits >> shift) & mask`. `0x2CD6D`: an empty record returns 0;
`0x2CD72..0x2CD75 jbe`: `v + 1 > count` (unsigned) returns 0 and draws nothing.
The label is `0x1C500(+0)` at (4, row) in the entry mode, through `0x2F280`
(release) or `0x2F198`. Entry = `[+0x10] + 8v`; col = 5. A text whose first
byte (`movsx`) is `'*'` sets mode = `(mode & 0x8000) | 0x1000` and skips the
star (`0x2CDEC..0x2CDFD`). With byte `+0xC` set, the index is
`itoa(v + 1, buf, 10)` (`0x2CE11 0x655E4`, runtime) drawn at (col, row + 1)
(`0x2CE1E inc edx`), then col += strlen + 1. A non-empty text is drawn next at
(col, row + 1), col += strlen + 1; the `+4` string last. All three use the
possibly switched mode. It returns `rec + 0x14`. So record 0 with bits `3<<16`
draws "CREDITS" at (4, 6) and "4" at (5, 7), both `0x2000`; with `4<<16` the
text "*" switches to `0x1000` before "5" is drawn at (5, 7), and the label keeps
`0x2000`.

**`0x2CF00`** (EAX table EBP, EDX bits, EBX mask, CL Esc cancels; `sub esp,
0x44`, plain `ret`). Locals: `[esp]` the copy of string `0x72` "+ MORE +",
`[esp+0x1C]` entry bits, `[esp+0x20]` current record, `[esp+0x24]` window 6,
`[esp+0x28]` mask, `[esp+0x2C]` = 6*3+5 = 23 (the lower marker row), `[esp+0x30]`
result, `[esp+0x34]` working, `[esp+0x38]` top, `[esp+0x3C]` last index,
`[esp+0x40]` CL. Each `push` before a `ret 4` callee shifts `[esp+N]` by 4: the
store at `0x2CF6E [esp+0x24]` is `[esp+0x20]` = the table, `0x2CF8E
[esp+0x40]` is `[esp+0x3C]` = N - 1. Opening: `0x2CC74(table, bits, 0,
0xF000, 0)`, then record 0 on row 3 in `0x2000`; when `(u16)last > 6`
(`0x2CFAA jle`) the buffer's first and last bytes become `0x3B` and it is drawn
at (9, 23) in `0x1000`. `0x50146(0xF000F000, 0x1E, 0xF)`. Each pass: `0x2EA74`,
`keys = 0x2EDE0(0xF300F000, 1)`; `0x2000000` or `0x1000000` exits. The mask
branch (above). `[esp+0x30] = [esp+0x34]` (`0x2D06E`). Down (`0x40004000`)
with `(u16)idx < (u16)last` increments, else Up (`0x80008000`) with a non-zero
index decrements. **Scroll** (`0xC000C000` held or the mask branch): release the
window at the old top with the result bits; `idx < top` (16-bit `jae`) sets
top = idx, else `top + 6 < idx` (signed `jge` at `0x2D0D4`) sets top = idx - 6;
draw the window at the new top with the working bits; `[esp+0x20]` = table +
idx*0x14; the buffer's ends become `0x19` and it (or `[0x2CC70]` = `0x80A18`,
eight spaces, which `0x2F830` turns into a release) goes to (9, 2) when top is
non-zero; the ends become `0x3B` and it (or the blank) goes to (9, 23) when
`(u16)last - (u16)top > 6` (`0x2D174 jle`); the current record on row
`(idx - top + 1) * 3` in `0x2000`. **Left/Right** (no scroll): a second poll
`0x2EDE0(0, 1)` with Up/Down held skips the pass (`0x2D1CF`); none of
`0x30003000` skips it. The field mask is `((1 << n) - 1) << shift` with n from
`0x2D1E1..0x2D1FC` (ECX = 2 doubling while `(u16)cx < count`). Left
(`0x2D20D`): a zero field ORs `(count - 1) << shift` (the wrap), else
subtracts `1 << shift`. Right (`0x2D23A`): a field equal to `count - 1`
(`0x2D250 cmp ecx,ebx; jne`) is cleared (the wrap), else `1 << shift` is added.
The row is released with the result bits in `0xF000` and redrawn with the
working bits in `0x2000`, then result = working (`0x2D2B7`). **Exits**
(`0x2D2C0..0x2D2DA`): Esc with a non-zero CL returns -1, anything else the
result bits.

**`0x2CACC`**: `mov eax,[0x10740C]; mov eax,[eax+4]; jmp 0x33578`. With the
§K11.2 store `DS_0010740C = 0x1D2D0`, `[0x1D2D4]` = `0xA2EB4`.

**`0x33578`** (EAX table; pushes EBX/ECX/EDX/ESI/EDI): `0x2F99C`; string
`0x76` "GAME OPTIONS" centred on row 0 in `0x5002`. `0x335A1..0x335AC`: a zero
table becomes the code-object `0x32700` and the `je 0x336B3` after it is never
taken (no raw caller passes 0). `old = 0x2D974(0x29)`; bit 15 (`test ah,0x80`)
writes `0x2CCD0(table)` first. Strings `0x6E`, `0x71`, `0x209`, `0x70` centred
on rows `0x19..0x1C` in `0x1000`. `0x2CF00(table, old, 0x4000000, CL = 0)`,
`0x2DA0C(0x29, r)`; `now = 0x2D974(0x29)` and bit 15 writes `0x2CCD0(table)`
again. The language test `0x3369C..0x336A9` compares `old & 0x7000000` with
`now & 0x7000000`; when they differ `0x336AE` calls `0x47370(now >> 24)`, the
language reload (the port's `PORT:` note and **named gap**: the new language
is stored in field `0x29` but not shown). EAX is `now & 0x7000000`, or
`0x47370`'s result; the only path to it, `0x2CACC` through `menu_run`
(`0x2FD98`), discards it, and the port returns `now & 0x7000000` in both cases.

**`0x30EB4` / `0x30F54`**: `0x2F99C`; the title (`0x20F` / `0x214`) centred on
row 0 in `0x5002`; `0x209`, `0x20A` on rows `0x1B`/`0x1C` in `0x1000`. ESI = 0,
then `r = 0x2CF00(0xA3500 / 0xA3060, esi, 0x4000000, CL = 1)` until r = -1,
each r played by `0x2C9E8` / `0x2C9CC` and kept as the next entry bits
(`0x30F3E` / `0x30FE2`). The tail `0x30F42..0x30F50` is `sound_voice(0x100)`,
and its EAX is the result; `0x30FD7 je 0x30F42` jumps into `0x30EB4`'s copy,
and the port repeats it.

**`0x2C9CC` / `0x2C9E8`**: `sound_voice(0x100)`, then `sound_voice([0xBC938 +
4i])` / `[0xBC9A0 + 4i]` (EDX kept). The tunes table has 26 ids ending at
`0xBC9A0` and the samples table 143 ending at `0xBCBDC`. Tune 0 is `0x1B`,
case 1 with handle `0x0D000008` (so `DS_00105D5C` = `0x0D000008`); tune 2 is
`0x1D`, `0x0B800008`. Every tune is case 1. The samples are case 2 except 21,
27 and 34 (`0x46`, `0x4D`, `0x5D`, case 3); sample 0 is `0xD7` (`0x02807ACC`)
and sample 3 is `0xEA` (`0x1201A05D`). Voice id `0x100` is record 0, case 5
id 0: the music stop, which zeroes `DS_001028D4`.

**Values the tests pin** (`sm_check_options`), all as the brief gives them
except the two corrections above: (a) and (b) as above; `0x2CD30` on record 3
with value 9 draws "10" at (5, 10), (6, 10) and string `0x1F8` at (8, 10), all
`0x1000` (the "*" switch from `0x4000`); `0x2CC74` from record 0 returns
`0xA2F40` and draws record 6 on row 21, from record 4 it returns 0 (the
terminator) with record 9 on row 18, and a negative first returns 0. (c) Left
on CREDITS 0 wraps to 9 (`9 << 16`, 2 frames), with "10" at (5, 4)/(6, 4) in
`0x2000` and the lower marker's `0x3B` at (9, 23) and (16, 23); Esc returns -1
with CL = 1 and `5 << 16` with CL = 0 (1 frame each); Right on 9 wraps to 0 (2
frames); Right, the restore pad bit, Enter returns the entry `5 << 16` (3
frames). (d) `0x2C9CC(0)` sets `DS_00105D5C` and `DS_001028D4` to
`0x0D000008` (the stop comes first). Samples leave no mark without a DIG
driver, so the test retypes `0xEA` and `0xD7` to case 1 and restores them:
`0x2C9E8(3)` gives `0x1201A05D`; SOUND TEST Enter, Esc gives `0x02807ACC`
with `DS_001028D4 = 0` from the tail (2 frames); MUSIC TEST Right, Enter,
Right, Enter, Esc gives tune 2 `0x0B800008` (5 frames; tune 1 if the next
edit did not start from the last result). (e) `0x2CACC` with `DS_0010740C =
0x1D2D0` and field `0x29` = `v0` (bit 15 clear, CREDITS 2): Right, Enter gives
`v0 + (1 << 16)` (2 frames). Field `0x29` = `v0 | 0x8000`, nine Downs, Right,
Enter (11 frames): the pre-edit default writes `0x142095`, the Downs scroll to
record 9 (top 3), Right sets bit 15 and the post-edit default writes
`0x142095`. Without the pre-edit default the edit would start from the seeded
bit 15 and wrap it to 0, leaving `v0`; without the post-edit one the field
keeps bit 15. The screen then shows the upper marker `0x19` at (9, 2), a blank
lower marker (9 - 3 = 6), record 3 "Difficulty" on row 3 in `0xF000`, record 9
highlighted on row 21 and "Yes" (`0x202`) at (5, 22) in `0x2000`.

**Not tested:** the titles and the four CONFIG help lines (drawn, not
asserted); the `0x32700` fallback (no raw caller passes 0); the `0x47370`
reload (the named gap). **Unobservable:** the second `0x2CC8F` empty test,
which repeats `0x2CD6A`'s. (Correction, §K11.4: this line first called the
Up/Down hold check `0x2D1BE..0x2D1CF` unobservable. It was only not tested;
§K11.4 tests it through the harness's `sm_held`.)

**Frame budget.** 29 scripted frames (2 + 1 + 1 + 2 + 3 in (c), 2 + 5 for
SOUND/MUSIC, 2 + 11 in (e)), 37 across K11 so far with cycle 1's 8.

**Mutations** (each applied, rebuilt, run and reverted by
`<scratchpad>/k11_t3_mut.py`; log `<scratchpad>/k11_t3_mutations.txt`). All 21
fail the suite:

| # | mutation | result |
|---|---|---|
| 1 | `0x2CD65 dec eax` dropped (the mask is one bit too wide) | 14 checks |
| 2 | the index drawn without `+1` | 7 |
| 3 | `0x2D250` Right wrap compared with `>` | 1 |
| 4 | Esc with CL = 1 returns the bits | 1, then the harness exit (SOUND TEST never leaves) |
| 5 | `0x2C9CC` indexes `0xBC9A0` | 3 |
| 6 | `0x33578` skips the bit-15 default before the edit | 3 |
| 7 | `0x2CACC` reads `+0` | SIGSEGV (exit -11): `[0x1D2D0]` = 0 selects the `0x32700` fallback (since §K11.4 the test's scratch record makes it fail 2 checks instead) |
| 8 | `0x33578` skips the bit-15 default after the edit | 1 |
| 9 | the restore-key branch dropped | 1 |
| 10 | `0x2CC74`'s row bound `0x13` | 3 |
| 11 | the scroll top off by one | 3 |
| 12 | Left wraps to `count` | 3 |
| 13 | `0x2C9E8` indexes `0xBC938` | 2 |
| 14 | `0x30EB4`'s tail `sound_voice(0x100)` dropped | 1 |
| 15 | `0x30F54` drops `prev = r` | 1 |
| 16 | the `'*'` mode switch dropped | 4 |
| 17 | the opening lower marker dropped | 2 |
| 18 | the upper marker's condition inverted | 1 |
| 19 | the lower marker compared with `>=` | 1 |
| 20 | the string after the text dropped | 2 |
| 21 | `input_repeat_set` dropped | 2 |

**Gate (Task 3).** `make verify` exited 0. Its oracle lines equal §K11.0's
except the two unittest wall-clock lines (`Ran 10 tests in 0.116s`, `Ran 33
tests in 1.209s`). The three new `fn_register` calls run in `game_init`, so
the 8000-frame dump was compared: `--check 8000` without the three calls and
with them gives the same `shasum` list over 8000 `.idx` and 8000 `.pal` files.
The header grep counts 1 for each of the nine addresses. `tools/port_progress.py`
is unchanged at `762 1203 63` / `726 730 99`: none of the nine is a Ghidra
function (§0.3).

## §K11.4 Cycle 3: ADJUST VOLUME and 2 PLAYER HANDICAP (executor, Task 4)

Five functions, re-read from the fixed-up image (`k11_dx.py`, dump
`<scratchpad>/k11_t4_dis.txt` and `k11_t4_dis_a.txt`); data from
`<scratchpad>/k11_t4_data.py`.

**(a) `0x2F464`** (`text_number_cont`): `push esi; sub esp,0x14; mov esi,ecx;
mov ecx,ebx; mov ebx,edx; mov edx,esp; call 0x2EFD4` then `mov ebx,esp; mov
edx,-1; mov ecx,esi; xor eax,eax; call 0x2F198`. So EAX = value, EDX = width,
EBX = pad, ECX = mode, as the plan says; the number is drawn at col 0, row -1,
which `0x2F198` turns into the cursor `DS_00105F34` (both words reloaded), and
the cursor moves past it. A pad-1 "  123" draws no cell for the two blanks.

**(b) The layout bytes.** The image holds, from `0xBD440`:
`64 65 66 00 | 05 0D 13 | 03 0B 11 | 00 | 78 | 77 00 00 00 | 78 00 00 00 |
79 00 00 00 | 00 | 04 0C | 02 0A`. **Correction (raw wins):** the plan names
`0xBD441..0xBD459` as the bytes, but every read is a dword three bytes below
the byte followed by `sar 0x18` (the WATCOM signed-char load): `mov
edx,[0xBD441]; sar edx,0x18` (`0x30932`) is the byte at `0xBD444`. The port
keeps the raw form (`SVC_HI8(a)` = `(s32)DSD(a) >> 24`). The bytes are:

| byte | value | read as | meaning |
|---|---|---|---|
| `0xBD444..0xBD446` | 5, 0xD, 0x13 | `[0xBD441..0xBD443]` | the music, effects and voice bar rows (`0x30788`) |
| `0xBD447..0xBD449` | 3, 0xB, 0x11 | `[0xBD444..0xBD446]`, `[esi+0xBD444]` | their label rows; the voice text is on `0xBD449` + 5 = 0x16 |
| `0xBD44C/50/54` | 0x77, 0x78, 0x79 | dwords | "GAME MUSIC", "GAME SAMPLES", "ATTRACT RATIO" |
| `0xBD458` | 0 | byte | the mute flag: `mov [0xBD458],cl` (CL = 1, `0x30E25`), `mov [0xBD458],bh` (BH = 0, `0x30E49`), `cmp byte [0xBD458],0` (`0x30E30`) |
| `0xBD459/0xBD45A` | 4, 0xC | `[0xBD456]`, `[0xBD457]` | the handicap bar rows |
| `0xBD45B/0xBD45C` | 2, 0xA | `[0xBD458]`, `[edi/esi+0xBD458]` | the handicap label rows |

The handicap labels are the code-object dwords `0x30720` = `0x17` "LEFT
PLAYER" and `0x30724` = `0x16` "RIGHT PLAYER", copied to `[esp+0x30]` by two
`movsd` (`0x31141..0x3114B`). The stack bytes `[esp+0x1C..0x1E]` = `0xFF`
(`0x30872..0x30881`) are read the same way (`[esp+0x19..0x1B]`, `sar 0x18`):
the three bars' label row is -1, so `0x30788` draws no number.

**(c) `0x308B7..0x308CC`.** `0x2C8F0(-1)` is `attract_config_volumes_unscaled`.
Its EAX is EDX = field `0x35` as read at `0x2C902` (`0x2C9B1 mov eax,edx`;
`0x1CAB8`, `0x2D974` and `0x1CED4` preserve EDX), so the port's function now
returns `u32` (the fight callers still discard it). **Correction (raw wins):**
the plan says field `0x35` reads -1 when unset and asks for a test of the
negative arm. Field `0x35`'s descriptor `[0x2D300 + 0x35*4]` = `0xE0C0` is 4
nibbles (16 bits) at bit 131 with no trailing byte, and `0x2D974` returns -1
only for a field above `0x3E` (`0x2D978..0x2D97D`). So field `0x35` reads
0..0xFFFF, the `jge` at `0x308CA` is always taken, and the arm `0x308CC`
(`0x30728`, then EAX = -1 at `0x308D1`) is **not reachable in the raw**. It
is ported as written and left untested; `0x30728` itself is tested directly.
The same holds for the `== -1` tests inside `0x2C8F0` and for field `0x37`
(`0x62C0`: 2 nibbles, 0..0xFF), whose `jl`/`> 0xFF` clamp at `0x308E7..0x308F2`
never fires.

**(d) `0x30864`.** Locals: `[esp]` music (field `0x35`), `[esp+4]` effects
(field `0x37`), `[esp+8]` voice (field `0x2A` & 3, read first at `0x308A0`),
ESI the selected row (0), EDI the redraw flag (**1 from `0x308BC`**, so the
first loop pass redraws even with no key), EBP the voice bar `max(music,
effects) * voice / 3` (`imul`/`idiv`, recomputed every pass at
`0x30C18..0x30C48`). Before the loop: the scale string `0x7B` at (4, 9) in
`0xF000`; the three bars; `0x2F388(5, 0x11, 6)` (the row is `0xBD449`
without the `+ 5` the loop uses, as the raw has it); voice 0 or 3 draws
"FULL" (`0x7D`) when the bar is non-zero, else "MUTE" (`0x7E`), at (5, 0x16),
otherwise `0x2F434(5, 0x16, voice, 2, pad 3, 0xF000)`, "/" (`0x80B60`) through
`0x2F41C`, and `0x2F464(3, 2, pad 3, 0xF000)`, which makes "1/3" or "2/3";
the three labels centred on rows 3, 0xB, 0x11 in `0xF000`; `0x6F`, `0x7C`,
`0x209`, `0x70` on rows 0x18, 0x19, 0x1B, 0x1C in `0x1000`; the title `0x20C`
on row 0 in `0xF002`; one `0x2EA74`; `sound_voice(3)` (case 4: the music
restart, `DS_00105D5C = 0x21`); `0x50146(0xF000F000, 0x1E, 0xF)`. Each pass:
`0x2EA74`, `keys = 0x2EDE0(0xF300F000, 1)`; Esc leaves. Down (`0x40004000`)
is `sel < 2 ? sel + 1 : 0`, Up (`0x80008000`) `sel > 0 ? sel - 1 : 2`, both
tested in the same pass. Left: the voice is decremented and floored at 0; a
volume is `v >= 8 ? v - 8 : 0` (`jl`). Right: the voice is incremented and
capped at 3; a volume is `v <= 0xF7 ? v + 8 : 0xFF` (`jg`). Each sets EDI.
With EDI set: the labels, the selected one in `0x3000` and the others in
`0x4000`; for the voice row, `0x2F388(5, 0x16, 0x14)` and the voice text
again (width ECX = ESI = 2); the three bars; then the volumes: row 0
`0x1CAB8(music >> 1)`, row 1 `0x1CED4(effects >> 1)`, row 2 both scaled
`(v * voice / 3) >> 1`. Then **mute** (`0x30E07..0x30E4F`): music 0, or row 2
with voice 0, runs `sound_voice(0x22)` and sets `0xBD458`; otherwise a set
`0xBD458` runs `sound_voice(3)` and clears it. **Exit** (`0x30E54..0x30EA8`):
`sound_voice(0x100)`, `0x2EA74`, `config_voice_gate(-1)` (its `sound_voice(0)`
does nothing), `0x2DA0C(0x35, music)`, `0x2DA0C(0x37, effects)`,
`0x2DA0C(0x2A, (0x2D974(0x2A) & ~3) | voice)` (`and al,0xFC`), EAX = 0.

**`0x30FE8`** (EAX value `[esp+0xC]`, EDX row `[esp+8]`): the value is clamped
signed to 0x32..0x96 (`jge`/`jle`). From 100 up (`0x31029 jl`), "    "
(`0x80B64`) is released at column 0x10 on rows + 4 and + 5 and the number is
`0x2F434(0x12, row + 4, v, 3, pad 2, 0xC002)`; below 100 the same at columns
0x12 and 0x14. Then glyph 0x13 on rows row..row + 2 for i = 0x32..0x96 step 5
from column 0xB: `0x3000` while i < v, `0xF000` at i = v, `0x1000` above.
**Correction:** §0.5's "four `0x2F280` releases clear the row first" is two
releases on each path, and the number comes between them and the bar.

**(e) `0x31138`.** `0x1AEE0` packs the key-config record into `[esp]`; the
handicap values are its words `+0x24`/`+0x26` (the mirror bytes
`DS_001014D0`/`DS_001014D2`), copied to `[esp+0x28]`/`[esp+0x2C]`. The rows
are drawn through `0x30FE8` on rows 4 and 0xC, the title `0x1F2` on row 0 in
`0xF002`, the labels on rows 2 and 0xA in `0xF000`, and `0x6C`, `0x71`,
`0x209`, `0x70` on rows 0x17, 0x18, 0x1A, 0x1B in `0x1000`. Then one
`0x2EA74`, EDI = 0 (the side) and `0x50146`. **EBP is not initialised**: it
is the caller's. The only caller is `menu_run` (`0x2FA40`), whose EBP is its
EDX, the stride 0x10 (`0x2FA48 mov ebp,edx`; nothing else writes EBP before
`0x2FD9E call [edx+8]`). So the first pass redraws, and the port starts the
flag at 1. Each pass: Esc leaves; Enter reloads both values from the packed
record (`0x312B1..0x312CF`); Up or Down toggles the side (`xor di,1`); Left
is `v >= 0x37 ? v - 5 : 0x32`, Right `v <= 0x91 ? v + 5 : 0x96` (`jg`).
The redraw draws the labels (`0x3000` selected, `0x4000` not) and both rows.
**Exit:** `DS_00107468` = left (`0x313CE`), `DS_0010746C` = right (`0x313D7`),
both stored back into the record's `+0x24`/`+0x26` words, `0x1AE28(record)`
(which copies their low bytes to `DS_001014D0`/`DS_001014D2`), `0x2EA74`. EAX
is `0x2EA78`'s -1 (`0x2EB6E..0x2EB74` leaves at EAX = -1); `menu_run`
discards it (`0x2FDA1 mov eax,1`). The raw's stack record is a `PORT:` scratch
at `0x03900040` (`SVC_KEYREC_TMP`, beside `config.c`'s `CFG_HISCORE_TMP`).

**Values the tests pin** (`sm_check_volume`; the two sound modules are
stubbed as in §K11.3 by zeroing `DS_001028C0`, `DS_001028C8`, `DS_001028DA`,
`DS_001028DB`):

* `0x2F464`: "  123" (width 5, pad 1) with the cursor at (3, 10) leaves
  columns 10 and 11 empty, '1', '2', '3' at 12..14 in `0x1000`, and the
  cursor at column 15, row 3. The row equals its seed because `0x2F198`
  reloads it; the check still fails under mutation 1 (row 0 is written).
* `0x30728`: a pass without a key, then Esc (2 frames); the 26-character
  string centred at column 8 on row 6 is released (columns 8 and 33 empty).
* `0x2C8F0(-1)` with field `0x35` = 0x40 returns 0x40 and sets the music
  volume to 0x20.
* ADJUST VOLUME A (music 0x40, effects 0x80, field `0x2A` = 9): Up, Right,
  Down, Right, Esc (7 frames). Up wraps row 0 to 2, Right makes the voice 2
  (the effects volume becomes `(0x80 * 2 / 3) >> 1` = 0x2A), Down wraps 2 to
  0, and Right steps music 0x40 to 0x48 unclamped (volume `0x48 >> 1` =
  0x24; the music bar covers cells 0..9 of row 5). Fields: `0x2A` = 0xA (bit
  3 kept), `0x35` = 0x48. The mute byte (0x5A) is cleared by the first pass,
  `DS_00105D5C` = 0x21, `DS_001028D4` = 0 (the exit's `0x100`), the repeat
  mask is `0xF000F000`. The screen: "GAME MUSIC" at (3, 16) in `0x3000`,
  "ATTRACT RATIO" at (0x11, 15) in `0x4000`, "2/3" at (0x16, 5..7) in
  `0xF000`, and the voice bar 0x55 covers cells 0..10 of row 0x13 (column 15
  `0x1000`, column 16 `0xF000`).
* B (music 0x40, effects 0xFC): Left, Down, Right, Esc (6 frames): music
  0x38 (the unclamped Left step), effects clamped to 0xFF, volumes 0x1C and 0x7F, "GAME SAMPLES"
  selected, "1/3" from the pre-loop draw.
* C (music 5): Left, Esc (4 frames): music clamped to 0, which mutes (the
  byte becomes 1) and sets the music volume to 0.
* D: a pass with no key, then Esc (4 frames): the first pass redraws ("GAME
  MUSIC" in `0x3000`) and clears the mute byte 0x5A.
* `0x30FE8(100, 4)`: (8, 0x10) and (8, 0x11) released, the number from
  (8, 0x12), bar cells 0..9 in `0x3000`, cell 10 (column 0x15) in `0xF000`,
  cells 11..20 in `0x1000`, column 0x20 empty. `0x30FE8(0x10, 4)` clamps to
  0x32: released from 0x12, the number from 0x14, cell 0 `0xF000`.
  `0x30FE8(0x200, 4)` clamps to 0x96: cell 20 `0xF000`, cell 19 `0x3000`.
* 2 PLAYER HANDICAP A (0x93, 0x36): Right, Down, Left, Esc (6 frames):
  `DS_00107468` = 0x96, `DS_0010746C` = 0x32 (both clamped), the mirror
  bytes the same, EAX = -1, "RIGHT PLAYER" selected. F (0x64, 0x50): Left,
  Down, Right, Esc (6 frames): the unclamped steps give 0x5F and 0x55, whose
  bars put `0xF000` on cell 9 (column 0x14, row 4) and cell 7 (column 0x12,
  row 0xC). B (0x64, 0x50): Left,
  Enter, Esc (5 frames): Enter restores 0x64. E: a pass with no key, then Esc
  (4 frames): "LEFT PLAYER" turns `0x3000`. The suite's own key record at
  `[DS_00101514]+0x2D4..+0x2EF`, seeded with 0xA5, is unchanged afterwards:
  `0x1AE28` writes the harness's `MT_LAYOUT` record instead.

**What the arrows pin.** ADJUST VOLUME: Down (the step and the 2 -> 0 wrap),
Up (the 0 -> 2 wrap), Left (the unclamped music step and the music clamp at
0), Right (the unclamped music step, the effects clamp at 0xFF, the voice
step). 2 PLAYER HANDICAP: Down (the side toggle), Left and Right (the
unclamped steps and both clamps), Enter (the restore).

**Not tested:** the unreachable `0x308CC` arm (above); the voice clamps
(`v2 < 0`, `v2 > 3`); "FULL"/"MUTE" (voice 0 or 3); the title and help lines
of both screens; the `0x2F388` releases of the voice row; Up on the handicap
screen (Down is tested, and both run the same `xor`); Up without a wrap on
ADJUST VOLUME (the `sel - 1` step); the unclamped Left step on the effects
row (the music row's is tested; both run the same code). (Added from the
cycle-3 review, §K11.5:) the voice Left step `0x30BA9..0x30BAD` (a `v[2] -=
2` mutation survives the suite, since no script presses Left on the voice
row) and the unclamped Right step on the effects row (script B's Right on
effects is the clamp to 0xFF). A test of either costs a new ADJUST VOLUME
script of at least 5 frames, over the 2-frame limit set for these minors.
**Unobservable:**
`config_voice_gate(-1)` at the exit (its `sound_voice(0)` returns at once).

**Cycle-2 review items folded in.** (1) `0x2CACC`'s test now points
`DS_0010740C` at a scratch record `{0xA3060, 0xA2EB4}` and asserts CREDITS on
row 3, and a separate check pins `[0x1D2D4]` = `0xA2EB4`; the old mutation 7
(`+0` read) now fails two checks instead of crashing. (2) The four repeat
words `DS_000E1C3C/40/42/44` are saved and restored around all of
`test_svcmenu`'s screens. (3) The Up/Down hold check is tested: the harness's
`sm_held` puts a held pad bit back into the latch `DS_000E1C38` after
`tf_menu_press` clears it (as `0x500C4`'s `latch &= level` keeps a held bit),
so a pad Up held from an earlier frame has no edge in `0x2EDE0(0xF300F000)`
and the second poll `0x2EDE0(0, 1)` sees its level: a latched Left is
skipped (2 frames). (4) The `0x32700` fallback's comment is no longer a
`PORT:` note (it is the raw's behaviour). (5) `0x2CF00`'s unbounded copy of
string `0x72` has a `PORT:` note: it mirrors the raw's 0x1C-byte stack buffer.

**Frame budget.** 44 scripted frames in `sm_check_volume` (2 + 7 + 6 + 4 + 4
+ 6 + 6 + 5 + 4) and 2 more in `sm_check_options` (the hold check), 46 in this
task; the scratch-record change reuses cycle 2's 2 frames. K11 total: 37 + 46
= 83 of 160. (Review round 1 added 7: A's unclamped Right and script F.)

**Mutations** (each applied, rebuilt, run and reverted by
`<scratchpad>/k11_t4_mut.py`; log `<scratchpad>/k11_t4_mutations.txt`). All 42
fail the suite with exit 1 and no crash:

| # | mutation | result |
|---|---|---|
| 1 | 0x2F464 row -1 as 0 | 6 checks |
| 2 | 0x2F464 width off by one | 4 checks |
| 3 | 0x30728 exits on Enter | 2 checks |
| 4 | 0x30728 release dropped | 2 checks |
| 5 | 0x30864 Left and Right swapped | 13 checks |
| 6 | 0x30864 field 0x35 save dropped | 2 checks |
| 7 | 0x30864 field 0x37 save dropped | 1 checks |
| 8 | 0x30864 field 0x2A save dropped | 1 checks |
| 9 | 0x30864 field 0x2A keeps bits 0..1 | 1 checks |
| 10 | 0x31138 exit stores swapped | 5 checks |
| 11 | 0x2C8F0(-1) returns 0 | 7 checks |
| 12 | 0x30864 Down wraps to 2 | 3 checks |
| 13 | 0x30864 Up wraps to 0 | 7 checks |
| 14 | 0x30864 mute store dropped | 1 checks |
| 15 | 0x30864 unmute store dropped | 2 checks |
| 16 | 0x30864 redraw flag starts 0 | 2 checks |
| 17 | 0x30864 sel-2 effects unscaled | 1 checks |
| 18 | 0x30864 Right clamp dropped | 2 checks |
| 19 | 0x30864 Left clamp dropped | 3 checks |
| 20 | 0x30864 sel-0 music apply dropped | 3 checks |
| 21 | 0x30864 voice 0x2F464 dropped (loop) | 1 checks |
| 22 | 0x30864 bar is max * voice without /3 | 1 checks |
| 23 | 0x30FE8 value cell compare <= | 5 checks |
| 24 | 0x30FE8 threshold 0x65 | 3 checks |
| 25 | 0x30FE8 low clamp dropped | 1 checks |
| 26 | 0x30FE8 high clamp dropped | 1 checks |
| 27 | 0x30FE8 releases dropped | 3 checks |
| 28 | 0x31138 Enter restore dropped | 2 checks |
| 29 | 0x31138 Right clamp dropped | 2 checks |
| 30 | 0x31138 Left clamp dropped | 2 checks |
| 31 | 0x31138 config_keys_apply dropped | 2 checks |
| 32 | 0x31138 redraw flag starts 0 | 1 checks |
| 33 | 0x31138 input_repeat_set dropped | 1 checks |
| 34 | 0x30864 registration dropped | 1 checks |
| 35 | 0x2CF00 Up/Down hold check dropped (review item 3) | 1 checks |
| 36 | 0x2CACC reads +0 (§K11.3 mutation 7, review item 1) | 2 checks |
| 37 | 0x31138 handicap label mode swapped | 3 checks |
| 38 | `0x30864` volume Right step +7 (review round 1) | 3 checks (`0x35` = 0x47, volume 0x23, the music bar) |
| 39 | `0x31138` Right step +4 (review round 1) | 2 checks (0x54, the right bar) |
| 40 | `0x31138` Left step -4 (review round 1) | 2 checks (0x60, the left bar) |
| 41 | `0x30864` volume Left step -7 (review round 1) | 1 check (`0x35` = 0x39) |
| 42 | the harness leaves `DS_00101514` on the suite's record (test file) | 43 checks, among them the new key-record check (corrected in §K11.8: 44 counted the closing `FAILURES` line) |

**Gate (Task 4).** `make verify` exited 0 (`<scratchpad>/k11_t4_verify.txt`).
Its oracle lines equal §K11.0's except the two unittest wall-clock lines
(`Ran 10 tests in 0.102s`, `Ran 33 tests in 1.078s`). The signature change
and the two new `fn_register` calls run on oracle paths (the fight setups
and `game_init`), so the 8000-frame dump was compared: `--check 8000` on
`a3d17a6` and on this tree give the same `shasum` list over 8000 `.idx` and
8000 `.pal` files (`k11_t4_frames_{before,after}.sha`, FRAMES-IDENTICAL).
The header grep counts 1 for each of `2F464 30728 30864 30FE8 31138`.
`tools/port_progress.py` goes from `762 1203 63` / `726 730 99` to `763 1203
63` / `727 730 100`: `0x2F464` is a Ghidra function (`FN_0002F464`), the
other four are not.

## §K11.5 Cycle 4: MODIFY CONTROLS and TEST CONTROLS (executor, Task 5)

Nine functions, re-read from the fixed-up image (`k11_dx.py`, dumps
`<scratchpad>/k11_t5_dis_a.txt` for `0x31F24` and `k11_t5_dis_b.txt` for
`0x32358`).

**(a) `0x2EF48`** (`text_hex_format`; EAX value in ESI, EDX buf, EBX width in
`[esp+4]`, ECX the pad flag in `[esp]`). `0x2EF56 mov ebx,[esp+2]; sar ebx,0x10`
reads the width's low word (the dword at `[esp+2]` has the flag's high word
below it), so the NUL goes at `buf[(s16)width]`. The pad is `0x20` when all of
ECX is non-zero and `0x30` when it is zero (`0x2EF65 test ecx,ecx`, stored at
`0x2EF75`). The digits come from the code-object table `0x2EF10` =
"0123456789ABCDEF": AX counts down from the width, `buf[(s16)ax] =
digits[v & 0xF]`, `v >>= 4`, until `(s16)ax <= 0` (`0x2EF92 jle`) or `v == 0`
(`0x2EF96`), so at least one digit is written and the high digits that do not
fit are dropped. The rest is padded down to index 0. **Correction:** it returns
`width - (s16)ax` (`0x2EF98..0x2EFA1`), the **digit count**, not `width -
digits` as §0.5 and the brief say (the brief's `0x2A` case gives 2 either way;
`0xBEEF` in 6 gives 4).

**`0x2F48C`** (`text_hex_set`): EAX col, EDX row, EBX value, ECX width, stack
`[esp+4]` pad and `[esp+8]` mode (`ret 8`), as the brief says. `0x2EF48` fills
the 0x14-byte stack buffer, then `0x2F198(col, row, buf, mode)`. For a width of
1..0x13 every byte up to the NUL is written; both callers pass 8.

**(b) `0x314A0`** (`svc_stick_draw`; EAX col, EDX row, EBX bits; ECX kept).
EDI = `0x10` (the centre cell, bit 4); Left `0x20002000` makes it 8, else
Right `0x10001000` makes it `0x20` (`0x314AB..0x314C7`); then Up `0x80008000`
shifts it right by 3 (`sar`), else Down `0x40004000` left by 3
(`0x314CC..0x314E1`). For each row r = row - 2, row, row + 2 it releases 5
cells from col - 2 (`0x2F388`, `0x31511`), then for c = col - 2, col, col + 2
draws `'+'` (`0x2B`) when EDI's bit 0 is set, else `'.'` (`0x2E`), in `0x4000`
(`0x31528..0x31544`, `0x2F174`), and shifts EDI right by one. So the bits are
the cells row-major from the top left.

**(c) The key-name helpers.** All four jump tables hold the four case bodies
in order, one per loop index 0..3:

| table | entries | case bodies |
|---|---|---|
| `0x319A0` | `319E7 319FF 31A13 31A2A` | `0x2F280` of string `0x22C` (nine blanks) at (col - 8, row + 8), (col + 2, row + 8), (col - 8, row + 0xC), (col + 2, row + 0xC), mode `0x4000` |
| `0x31A68` | `31ADD 31B00 31B23 31B37` | `0x2F198(x - len/2, y, name, 0x4000)` with x = col - 5, col + 5, col - 5, col + 5 and y = row + 8, row + 8, row + 0xC, row + 0xC |
| `0x31B84` | `31BDE 31BFA 31C17 31C2E` | string `0x22C`: `0x2F280` at (c - 3, 8) and (c - 3, 0xE), then `0x2F314` (down a column) at (c - 3, 8) and (c + 3, 8) |
| `0x31C68` | `31CF9 31D38 31D74 31DAE` | `0x2F198(c - len/2, 8)` and `(c - len/2, 0xE)` for "<name>" (EDX = 0), then `0x2F20C(c - 3, 0xB - len/2)` and `(c + 3, 0xB - len/2)` for the bare name (EDX = 1) |

The offsets: `0x319B6 lea eax,[ebx-5]`, `0x319C5 add ebx,5`, then `0x319CF lea
esi,[eax-3]` = col - 8 and `0x319D2 lea ebp,[ebx-3]` = col + 2; rows `0x319BB
lea esi,[ecx+8]` and `0x319C8 lea edi,[ecx+0xC]`. In `0x31A78`: `0x31A8E lea
eax,[ebx-5]` and `0x31A9E add ebx,5` (the name centres), `0x31A93 lea
ebp,[ecx+8]` and `0x31AA1 lea esi,[ecx+0xC]`. `len/2` is `repne scasb` then
`shr eax,1`. **So the layout is a 2x2 block below (col, row), not a diamond**
(§0.5's word), and the directions are a cross around (c, 0xB): up on row 8,
down on row 0xE, left and right down columns c - 3 and c + 3.

**Corrections to the brief's signatures (raw wins):**

* `0x319B0` takes **EBX = col, ECX = row** only. Its callers load EAX (the
  side) and EDX (the record), but `0x319B6` overwrites EAX and `0x319B9 xor
  edx,edx` EDX before either is read. The port is `svc_buttons_clear(col,
  row)`.
* `0x31A78` is `(EAX side, EDX record, EBX col, ECX row)`: the brief's `keys`
  is the key-config record, whose words `+0xA..+0x11` (side 0) or
  `+0x1C..+0x23` (side 1) are read (`0x31A7E..0x31A87`, `0x31ABB mov
  ax,[edx]`).
* `0x31B94` reads only EAX (the side: c = `0xA` or `0x1E`, `0x31B9C..0x31BA7`);
  EDX (the record) is loaded by its callers and not read.
* `0x31C78` is `(EAX side, EDX record)`; the words are `+2..+9` or
  `+0x14..+0x1B` (`0x31C96 lea esi,[edx+2]`, `0x31CA5 lea esi,[edx+0x14]`).

So the 0x28-byte record is: `+0` player 1's device, `+2` its four direction
keys, `+0xA` its four button keys, `+0x12` player 2's device, `+0x14` and
`+0x1C` its keys, `+0x24`/`+0x26` the handicaps (§K11.4).

**The name buffer.** `0x31A78` passes `EBX = esp` to `0x3157C`; the buffer is
the 8 bytes `[esp..esp+7]`, and its locals start at `[esp+8]` (col + 5,
re-read by cases 1 and 3). `0x31C78` has the same shape with its locals from
`[esp+8]` (c - 3). The English names fit: the longest is "<HOME>"/"<PGUP>"/
"<PGDN>"/"<LEFT>"/"<DOWN>", 7 bytes with the NUL. `0x3157C` writes nothing for
a key with no name (§49-Y), so such a key draws the name left by the key
before it (tested). The first key's buffer is uninitialised stack; **PORT:** the
port's buffer is the scratch `SVC_NAME_TMP` (`0x03900080`), zeroed on entry,
so a first key with no name draws nothing.

**(d) `0x31F24`** (MODIFY CONTROLS). There is no `0x2F99C` reset and no title:
the screen is drawn over the menu. "PRESS ESCAPE KEY" (`0x6B`) is centred on
row `0x1B` in `0x1000`. `0x1AEE0` packs the record into `[esp]`; then each
device word is replaced by the BIOS record's `+0x2D4`/`+0x2D6` when they differ
(`0x31F54..0x31F85`). Locals: EDI the selected player (0), EBP player 1's
device (updated each pass while player 1 is selected, `0x31FF7`), `[esp+0x2C]`
player 2's (while player 2 is, `0x32002`), ESI the selected player's, byte
`[esp+0x38]` the redraw flag (1), `[esp+0x34]` = tick - 1, `[esp+0x28]` =
`0x31410 + 0x24` = `0x31434`. Each pass: `0x2EA78(1)` (one frame, three tick
waits), `keys = 0x2EDE0(0, 1)` (EDX = 1 survives `0x2EA78`); Esc
(`0x2000000`) leaves.

**Correction (raw wins): Left and Right pick the player; Up and Down cycle the
device** (§0.5 and the brief have Left/Right cycling it). The chain is
exclusive: Left (`0x32006`) selects player 1 when player 2 is selected;
Right (`0x32029`) selects player 2; Up (`0x32051`) is `dev == 0 ? 6 : dev -
2`; Down (`0x320CF`) is `dev == 6 ? 0 : dev + 2`. The devices are the
`0x31E28` texts: 0 "KEYBOARD" (`0x22D`), 2 "4 BUTTON JOYSTICK" (`0x22F`), 4 "2
BUTTON JOYSTICK" (`0x230`), 6 "KEYBOARD/JOYSTICK" (`0x22E`). Then the other
player's device limits the new one:

| key | player 1 selected (other = player 2) | player 2 selected (other = player 1) |
|---|---|---|
| Up | other 2: 0; other 4 and new 2: 0 (`0x32081..0x3209C`) | other 2 and new 2 or 4: 0; other 4 or 6 and new 2: 0 (`0x320A3..0x320C8`) |
| Down | other 2: 0; other 4 and new 2: 4 (`0x320FD..0x32114`) | other 2 and new 2 or 4: 6; other 4 or 6 and new 2: 4 (`0x32118..0x3213C`) |

**Correction: there is no blink.** The two `0x500BB` reads after the first
are the Up/Down wait: Up and Down store `[esp+0x34]` = tick + `0xC`
(`0x3205D..0x32067`, `0x320DB..0x320E5`), and while the tick is below it
(`0x31FDE..0x31FE5`, unsigned `jae`) the pass masks the keys with
`0x3FFF3FFF`, dropping Up and Down (Left, Right and Esc still act). The first
read (`0x31F9F`) starts the wait at tick - 1, so the first pass is free. At
three ticks a frame, an Up or Down is dropped on the next three frames and
acts on the fourth.

With the redraw flag clear the pass ends (`0x32141..0x32146`). Otherwise the
flag is cleared and ESI is stored into the selected device word, then:
`0x31E28(0, rec, sel == 0)` and `0x31E28(1, rec, sel == 1)` (the selected
player's device text in `0x3000`); for each side, `0x31C78` when its device is
0, else `0x31B94`; `0x31A78(side, rec, 0xA or 0x1E, 0xB)` when the device is 0
or 6, else `0x319B0(0xA or 0x1E, 0xB)`; `0x314A0(0xA, 0xB, 0)` and
`0x314A0(0x1E, 0xB, 0)` (the centres lit); then the marker table `0x31434`
(12-byte entries `{u32 bit, u8 col, u8 row, u16 0, u32 string id}`, ended by
bit and string both 0):

| entry | bit | (col, row) | string |
|---|---|---|---|
| `31434` | `0x1000000` | (5, 0x12) | `0x21C` "HI QUICK" |
| `31440` | `0x2000000` | (15, 0x12) | `0x21D` "HI FIERCE" |
| `3144C` | `0x4000000` | (5, 0x16) | `0x21E` "LO QUICK" |
| `31458` | `0x8000000` | (15, 0x16) | `0x21F` "LO FIERCE" |
| `31464`..`31488` | `0x100`, `0x200`, `0x400`, `0x800` | (25/35, 0x12/0x16) | the same four |

Each entry draws its string at (col - 3, row - 1) in `0x4000`, then a glyph at
(col, row) in `0x4000`: `' '` (which releases the cell) for the FIERCE entries
of a player whose device is 4 (`0x322A8..0x322F4`), else `'X'`. **Exit**
(`0x32336`): `0x2EDE0(0xF300F000, 1)`, `0x1AE28(rec)`, EAX = 0. **PORT:** the
stack record is the scratch `SVC_KEYREC_TMP`, as in `0x31138`.

**(e) `0x32358`** (TEST CONTROLS). EBP = `DS_00107410 & 0x10` (the
instruction is `0x32361`, §K11.2). With it set: "ADDRESS    RAW DATA"
(`0x80BAC`) at (3, 4), nineteen `'^'` (`0x80BC0`) at (3, 5), `0x2F48C(3, 6,
0xFFE80000, 8, pad 0, 0x4000)` and "DIAGS" (`0x80BD4`) at (3, 7). Then
"PRESS ESCAPE KEY", the packed record with the device override, `0x31E28(0,
rec, 0)` and `(1, rec, 0)` (no selection), `0x31C78` for a BIOS device word of
0 and `0x31A78` for 0 or 6 (both read `[DS_00101514]+0x2D4/+0x2D6` again, not
the record; no clears), and one `0x2EA74`. The labels are drawn once, from
`0x31410` with the flag and from `0x31434` without. The three entries at
`0x31410` are `{8, (0x20, 0xB)}`, `{0x10, (0x22, 0xB)}`, `{4, (0x24, 0xB)}`
and hold the **pointers** `0x80B6C`/`0x80B70`/`0x80B74` (" D", " S", " F")
where a string id belongs. `0x1C500` decodes them as ids: the group walk
`0x80B6C / 0x40 = 0x202D` steps runs around the 9-group ring of ENGLISH.TXT
(the last group's link is 0), so they are strings `0xAC` "Computer Character
% Wins  Matches", `0xB0` "Character         spms/round" and `0xB4` "Vertigo",
drawn from (0x1D, 0xA), (0x1F, 0xA) and (0x21, 0xA). The port calls
`game_string_get` with the same values (tested).

Each pass: `keys = 0x2EDE0(0, 0)` (the pad level; no latched key), `k =
0x2EB80()`; `k != 0 && k == 0x1B` leaves (`0x32547..0x3254E`). With the flag:
`0x2F48C(0xE, 6, keys, 8, 0, 0x3000)` and `0x2F48C(0xE, 7, [byte
0xFFE80003], 8, 0, 0x3000)`. Then `now = 0x2EDE0(0, 0)`, `0x314A0(0xA, 0xB,
now & 0xF0000000)`, `0x314A0(0x1E, 0xB, now & 0xF000)`, each table entry's
glyph `'O'` (`0x4F`) when `now & bit`, else `'X'`, in `0x3000`, and one
`0x2EA74`. **Exit** (`0x32623`): `0x2EDE0(0xF300F000, 1)`, EAX = 0; no
`0x1AE28`. The diagnostic rows 4 and 6 are drawn before the option rows,
which overwrite them from column 2 ("LEFT PLAYER" on row 4, the device text on
row 6); the pad word under RAW DATA (column 0xE) is drawn each pass and stays.

**Named gap (PORT): `0x32573 mov ebx,0xFFE80003; mov bl,[ebx]`.** The
diagnostic arm reads a byte at linear `0xFFE80003`, an arcade address with no
memory behind it in the DOS build and outside the port's `mem[]`. What the DOS
build reads there (a page fault under DOS/4GW, or whatever the host returns)
is not pinned. The port draws 0 and the test asserts only that the row is
drawn. The arm is reached only with `DS_00107410` bit 4, config field `0x2A`
bit 4.

**Values the tests pin** (`sm_check_controls`, 23 scripted frames):

* `0x2EF48`: `0x2A`/4/1 gives "  2A" and 2; `0xBEEF`/6/0 "00BEEF" and 4;
  `0x12345`/3/0 "345" and 3; `0`/3/`0x100` "  0" and 1 (the flag is all of
  ECX). `0x2F48C` draws "00BEEF" from (3, 10) in `0x3000` and "  2A" with the
  two blanks undrawn.
* `0x314A0` around (10, 11): no bit lights the centre and releases the 5-cell
  rows; Up + Left lights (8, 9); Down + Right (`0x5000`) lights (12, 13).
* `0x319B0(0xA, 0xB)` releases columns 2..10 and 12..20 of rows 0x13 and 0x17
  and keeps 1 and 11. `0x31A78`: "<A>" from (4, 0x13), "<S>" from (14, 0x13),
  "<UP>" from (3, 0x17), and a key with no name draws "<UP>" again from (13,
  0x17); side 1 reads `+0x1C`. `0x31B94(0)` releases rows 8 and 0xE from
  column 7 and columns 7 and 13 from row 8 through row 16; side 1 is column
  0x1B. `0x31C78`: "<UP>" from (8, 8), "<DOWN>" from (7, 0xE), "LEFT" down
  column 7 from row 9, "RGT" down column 13 from row 0xA, and no "<" above
  either; side 1 reads `+0x14`.
* MODIFY A (BIOS devices 6 and 0, the mirror's 4 and 2): Down, Right, a frame
  with no key, Down, Up, Esc (6 frames). Down wraps player 1 from 6 to 0 (from
  the BIOS word, not the mirror's 4); Right selects player 2; frame 4's Down is
  inside the wait and dropped; frame 5's Up is past it (tick + 12) and wraps
  player 2 from 0 to 6. The applied devices are 0 and 6 (BIOS words and the
  mirror bytes). This pins the wait to 10..12 ticks: at 4..9 frame 4's Down
  acts and frame 5's Up is dropped (player 2 ends at 2), below 4 both act (0),
  and at 13 or more frame 5's Up is dropped too (0).
* MODIFY B (4, 0): Right, Down, Esc: player 2 goes 0 -> 2 -> 4 (player 1's
  device 4); player 1's names are released; the HI FIERCE and LO FIERCE
  markers of both players (`0x2000000`/`0x8000000`, `0x200`/`0x800`) are
  blank, their cells seeded beforehand; LO QUICK keeps its "X".
* MODIFY C (2, 6): Right, Up, Left, Esc: player 2 goes 6 -> 4 -> 0 (player 1's
  device 2); Left selects player 1 again ("4 BUTTON JOYSTICK" in `0x3000`).
* MODIFY D (4, 4): Up, Esc: player 1 goes 4 -> 2 -> 0 (player 2's device 4).
* The unlimited steps (fix round 1). With player 2's device 0, neither of
  player 1's limit rows matches (Up `0x32081..0x3208D`: other 2, or other 4
  and new 2; Down `0x320FD..0x3210D`: the same), so the step stands. MODIFY
  E (6, 0): Up, Esc: player 1 goes 6 -> 4 (`0x3207A sub esi,2`), "2 BUTTON
  JOYSTICK" selected and HI FIERCE blank. MODIFY F (0, 0): Down, Esc: player
  1 goes 0 -> 2 (`0x320F6 add esi,2`), "4 BUTTON JOYSTICK" selected and the
  first pass's "<UP>" released. A step of 4 either way fails (mutations 43,
  44); before this, every non-wrapping step in A-D ended on a limit arm or
  gave the same result with a step of 4.
* TEST A (`DS_00107410` = `0xEF`, bit 4 clear; devices 0 and 4): a frame with
  the pad at `0xA1004000`, then Esc (2 frames). The pass shows player 1's
  stick Up + Left, player 2's Down, "O" for HI QUICK and "X" for HI FIERCE in
  `0x3000`; there is no DIAGS row, and the mirror is not written.
* TEST B (`0x10`; devices 8 and 4, so row 6 keeps the address): the pad at
  `0x10002000`, then Esc (2 frames). "FFE80000" from (3, 6), "DIAGS", RAW DATA
  at (0xE, 4), "10002000" from (0xE, 6), the raw-data row drawn, the labels
  of strings `0xAC` and `0xB4` at (0x1D, 0xA) and (0x21, 0xA), the bit-8 "X"
  over player 2's stick at (0x20, 0xB), both sticks, and no button names for
  device 8 (a sentinel at (4, 0x13) is kept).

**Limit arms tested:** player 1 Up with player 2's device 4 and new 2 (D);
player 2 Up with player 1's device 2 and new 4 (C); player 2 Down with player
1's device 4 and new 2 (B). **Not tested:** player 1 Up with player 2's device
2; **both player 1 Down arms** (other 2 gives 0, other 4 and new 2 gives 4);
player 2 Up with player 1's device 2 and new 2 (the `dev == dev1` sub-arm,
`0x320A8`; reaching it needs a Right frame on top of a script) and with
player 1's device 4 or 6 and new 2; player 2 Down with player 1's device 2
(new 2 or 4 gives 6), and with player 1's device 6 (the same arm as 4); the buttons for device 6; the "PRESS ESCAPE
KEY" line; the `0x2EDE0(0xF300F000, 1)` at both exits (its only effect is the
key-time stamp); the width limits of `0x2EF48` (no caller passes a width
outside 1..0x13).

**Frame budget.** 23 scripted frames in `sm_check_controls` (MODIFY 6 + 3 +
4 + 2 + 2 + 2, TEST 2 + 2); the helper checks present no frame. K11 total: 83
+ 23 = 106 of 160 (fix round 1 added MODIFY E and F, 4 frames).

**Mutations** (each applied, rebuilt, run and reverted by
`<scratchpad>/k11_t5_mut.py`; log `<scratchpad>/k11_t5_mutations.txt`). All 46
fail the suite with exit 1 and no crash. They include the brief's five: a
dropped pad flag (1), swapped jump-table cases (10, 14), `0x314A0`'s Up and
Down swapped (6), the device wraps (17, 18) and the Esc test on `0x0D` (32).
**Count correction (Task 6 fix round 1):** the counts first shipped here
included the suite's closing `FAILURES: N` line, one too many for every row.
The rows below are the `test_game.c` FAIL lines re-measured from the same log
(43..46 are 2, 3, 1 and 1, not 3, 4, 2 and 2), and the labels of 45 and 46
name the marker bit each one drops. **The counts are a snapshot** of the suite
as it stood at Task 5's fix round; later tasks add checks that the same
mutation can also fail, so a re-run at a later HEAD measures more (at
`13a99e8`, row 22 fails 18 checks, not 14). The pass/fail verdicts do not
move.

| # | mutation | result |
|---|---|---|
| 1 | 0x2EF48 pad flag dropped | 3 checks |
| 2 | 0x2EF48 pad flag low byte only | 1 check |
| 3 | 0x2EF48 returns width - digits | 3 checks |
| 4 | 0x2EF48 no truncation stop | 7 checks |
| 5 | 0x2F48C pad and mode swapped | 2 checks |
| 6 | 0x314A0 Up and Down swapped | 6 checks |
| 7 | 0x314A0 Left and Right swapped | 5 checks |
| 8 | 0x314A0 release dropped | 1 check |
| 9 | 0x319B0 case 1 column + 3 | 1 check |
| 10 | 0x31A78 cases 0 and 1 swapped | 3 checks |
| 11 | 0x31A78 side 1 at +0x14 | 1 check |
| 12 | 0x31B94 case 1 row r + 2 | 1 check |
| 13 | 0x31B94 case 3 column c + 2 | 2 checks |
| 14 | 0x31C78 cases 2 and 3 swapped | 4 checks |
| 15 | 0x31C78 left/right wrapped | 4 checks |
| 16 | 0x31C78 side 1 at +0x12 | 1 check |
| 17 | 0x31F24 Down wrap dropped | 5 checks |
| 18 | 0x31F24 Up wrap to 4 | 3 checks |
| 19 | 0x31F24 Up/Down wait dropped | 3 checks |
| 20 | 0x31F24 Down wait 9 ticks | 3 checks |
| 21 | 0x31F24 Down wait 13 ticks | 3 checks |
| 22 | 0x31F24 BIOS device override dropped | 14 checks |
| 23 | 0x31F24 p2 Down dev1 4/6 arm dropped | 2 checks |
| 24 | 0x31F24 p2 Up dev1 2 arm dropped | 2 checks |
| 25 | 0x31F24 p1 Up dev2 4 arm dropped | 1 check |
| 26 | 0x31F24 Left ignored | 2 checks |
| 27 | 0x31F24 config_keys_apply dropped | 7 checks (the first run's pattern broke the build; rerun) |
| 28 | 0x31F24 device-4 blank markers dropped | 3 checks |
| 29 | 0x31F24 option-row flags inverted | 4 checks |
| 30 | 0x31F24 dirs draw/clear swapped (p1) | 2 checks |
| 31 | 0x31F24 buttons clear dropped | 1 check |
| 32 | 0x32358 Esc test on 0x0D | the harness exit (the loop never leaves) |
| 33 | 0x32358 diag flag bit 0x20 | 15 checks |
| 34 | 0x32358 diag table start ignored | 4 checks |
| 35 | 0x32358 O/X inverted | 3 checks |
| 36 | 0x32358 stick masks swapped | 4 checks |
| 37 | 0x32358 keys hex row dropped | 4 checks |
| 38 | 0x32358 applies the record | 1 check |
| 39 | 0x32358 buttons drawn for every device | 2 checks (it survived the first run; TEST B now keeps a sentinel where player 1's button names would go) |
| 40 | 0x31F24 registration dropped | 1 check |
| 41 | 0x32358 registration dropped | 1 check |
| 42 | 0x2EF48 pad digit off (0x31) | 2 checks |
| 43 | 0x31F24 Down step +4 (fix round 1) | 2 checks (MODIFY F: 4, not 2) |
| 44 | 0x31F24 Up step -4 (fix round 1) | 3 checks (MODIFY E: 2, not 4) |
| 45 | 0x31F24 LO FIERCE (0x8000000) dropped from player 1's device-4 blanks (fix round 1) | 1 check |
| 46 | 0x31F24 LO FIERCE (0x800) dropped from player 2's device-4 blanks (fix round 1) | 1 check |

**Gate (Task 5).** `make verify` exited 0 (`<scratchpad>/k11_t5_verify.txt`).
Its oracle lines equal §K11.0's and ledger §A's, except the two unittest
wall-clock lines (`Ran 10 tests in 0.118s`, `Ran 33 tests in 1.168s`). The two
new `fn_register` calls run in `game_init`, so the 8000-frame dump was
compared: `--check 8000` on this tree gives the same `shasum` list over 8000
`.idx` and 8000 `.pal` files as `k11_t4_frames_after.sha` (the source under
`port/src` is unchanged from `4862074` to `7cb83e6`; FRAMES-IDENTICAL). The
header grep counts 1 for each of `2EF48 2F48C 314A0 319B0 31A78 31B94 31C78
31F24 32358`. `tools/port_progress.py` goes from `763 1203 63` / `727 730 100`
to `765 1203 64` / `729 730 100`: `0x319B0` and `0x31A78` are Ghidra functions
(`FN_000319B0`, `FN_00031A78`), the other seven are not.

## §K11.6 Cycle 5: CONFIGURE KEYBOARD (executor, Task 6)

Four functions, re-read from the fixed-up image (`k11_dx.py`; dumps
`<scratchpad>/k11_t6_dis_a.txt` for the tables and `k11_t6_dis_b.txt` for
`0x19DF0`).

**(a) `0x19C60` / `0x19D34`** (`svc_key_slot_set` / `svc_key_slot_get`). Both
open with `cmp eax,0xf` and an unsigned `ja` (`0x19C63 ja 0x19CF0`, the last
case's `ret`; `0x19D37 ja 0x19DD5 xor eax,eax; ret`), so a slot above 15,
-1 included, stores nothing and reads 0. The jump tables `0x19C20` and
`0x19CF4` hold the sixteen case bodies in order (`0x19C71 + 8i` and
`0x19D45 + 9i`); each setter case is `mov word [addr],dx; ret`, each getter
case `xor eax,eax; mov ax,[addr]; ret` (zero-extended). The slot map is §0.5's,
confirmed entry by entry:

| slot | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| addr | `CAE` | `CB4` | `CB0` | `CB2` | `CB6` | `CB8` | `CBA` | `CBC` | `CC0` | `CC6` | `CC2` | `CC4` | `CC8` | `CCA` | `CCC` | `CCE` |
| record | +2 | +8 | +4 | +6 | +0xA | +0xC | +0xE | +0x10 | +0x14 | +0x1A | +0x16 | +0x18 | +0x1C | +0x1E | +0x20 | +0x22 |

(`0x100xxx`; record = offset in `0x100CAC`). By §K11.5's record layout the
slots run up, right, down, left, then the four buttons, for each player, which
is the order of the on-screen labels below. `+0` and `+0x12` (the devices,
`0x100CAC`/`0x100CBE`) are no slot.

**(b) `0x2EBBC`** (`svc_raw_key_take`): `push edx; xor edx,edx; mov
eax,[0x105F28]; mov [0x105F28],edx; pop edx; ret`. `DS_00105F28` is the key
word `0x2EA78` stores for each drained key (`0x2EB3F`). Callers: `0x1A331` and
`0x1A391` only.

**(c) `0x19DF0`** (`svc_configure_keyboard`). There is no `0x2F99C` reset;
the screen is drawn over the menu's. It packs the mirror into the **global**
record `0x100CAC` (`0x19DF8 mov eax,0x100cac`, `0x1AEE0`), so the port needs
no scratch record; `0x3157C`'s names go to the global `0x100CD4`. ECX =
`0x2000` is set before `0x1AEE0` and survives it (`0x1AEE1 push ecx` /
`0x1AF61 pop ecx`) and `0x1C500` (`0x474E4 push ecx`), so the first title is
drawn in `0x2000` like the second; `0x3157C` saves ECX (`0x3157C push ecx`) but
not EDX. The screen:

* "LEFT PLAYER" (`0x17`) at (2, 4) and "RIGHT PLAYER" (`0x16`) at (0x16, 4) in
  `0x2000`.
* The eight label ids at `0xA2C2C..0xA2C48` = `0x231` "UP", `0x234` "RIGHT",
  `0x232` "DOWN", `0x233` "LEFT", `0x21C` "HI QUICK", `0x21D` "HI FIERCE",
  `0x21E` "LO QUICK", `0x21F` "LO FIERCE", at column 2 and rows 7, 9, ..
  0x15 in `0x1000` (`0x19E42..0x19F3D`), and again at column 0x16
  (`0x1A0B7..0x1A1B2`).
* Slots 0..7 as "<name>" (`0x19D34`, then `0x3157C` with EDX = 0) at column
  0xC, rows 7 + 2s, in `0xF000` (`0x19F42..0x1A0B2`); slots 8..15 at column
  0x20 (`0x1A1B7..0x1A32C`). All sixteen draws are unrolled in the raw; the
  port loops.

Then `0x1A331` calls `0x2EBBC` once, dropping a stale key word, and the slot
loop runs ESI = 0..15 (`0x1A32A`, `0x1A542..0x1A546`). Per slot: EDI =
(slot > 7 ? 0x16 : 2) + 0xA (`0x1A336..0x1A35A`, a signed `jg`), the row
`(slot % 8) * 2 + 7` into `[esp]` (`idiv` by 8), and the slot's current name
in `0x3000` at (EDI, row) (`0x1A355` ECX = `0x3000`). The wait loop
`0x1A387`: `0x2EA78(1)`, ECX = `0x2EBBC()`; zero waits again. Then on AL:

* **Esc** (`0x1A39C..0x1A3A4`, AL = `0x1B`): jumps to `0x1A556`, EAX = 0.
  **It skips the `0x1AE28` at `0x1A551`**: the record is not applied. The
  slots already taken stay in `0x100CAC` (which the next entry re-packs) and
  a code flag already set stays set.
* **Enter** (`0x1A3AA..0x1A3B6`, AL = `0x0D`): ECX = `0x19D34(slot)`, the
  slot's current word, which then goes through the same checks and store:
  Enter keeps the slot's key and moves on.
* Every key (Enter's reload included): for j = 0..slot - 1 (`0x1A3B8..
  0x1A3DD`, none for slot 0), `(0x19D34(j) & 0xFF00) == (ECX & 0xFF00)` zeroes
  ECX, and a zero ECX waits again (`0x1A3DF`): **a key whose scan code an
  earlier slot holds is refused**. Then `0x3157C(ECX, 0, 0x100CD4)` returning
  AL = 0 (no name, `0x1A3F1 test al,al`) waits again.
* Accepted: `0x19C60(slot, ECX)` (`0x1A3FE`; its EBX = `0x80590` is for the
  next call), seven blanks `0x80590` at (EDI, row) in `0xF000` (an all-space
  string: `0x2F830` releases the cells, `0x2F882`), then the new name in
  `0xF000`.
* **Two key codes** (not in §0.5). After slot 5 (`0x1A426`): when the low
  bytes of slots 0..5 are `'s' 'p' 'a' 't' 'e' 'n'` (`0x1A42F..0x1A49F`),
  `DSB(DS_00105D60) = 1` (`0x1A4A1`), the FREE PLAY flag `0x2CA1A..0x2CAAA`
  read (config.c). After slot 6 (`0x1A4A8`): `'m' 'o' 'r' 'l' 'a' 'n' 'd'`
  (`0x1A4B1..0x1A539`) sets `DSB(DS_00108113) = 1` (`0x1A53B`), whose only
  reader in the image is `0x4236F` (flow.c's state-3 `0x42374` test).

After slot 15: `0x1AE28(0x100CAC)` (`0x1A551`), EAX = 0 (`0x1A556`).

**Corrections (raw wins):**

* §0.5 and the brief call Esc and Enter "handled" and have `0x1AE28` apply the
  record: **Esc cancels without applying** (`0x1A3A4 je 0x1A556` passes the
  `0x1A551` call), and **Enter is not an exit**: it re-takes the slot's
  current key (`0x1A3B1`). Only completing slot 15 applies.
* The brief asks for "its cursor movement": **there is none**. No arrow key is
  special; the slot only advances (`0x1A542 inc esi`) when a key is accepted,
  and an arrow key is a key to bind (`0x4800` is "<UP>").
* The refusals (earlier slot's scan code, no name) and the two key codes are
  not in §0.5.
* `0x19C60`'s only rel32 caller is `0x1A3FE` (§0.3's "12+" is `0x19D34`'s, 33
  sites). The one absolute dword `0x00019C60` in the image, at `0x111B0`, is
  the rel32 of `0x111AF call 0x2AE14`, not a reference.

**`0x19DD8` is dead** (`push ebx; push edx; mov ebx,0x100cd4; xor edx,edx;
call 0x19d34; call 0x3157c; pop edx; pop ebx; ret`): the image has no rel32
call or jump to it and no absolute dword `0x00019DD8`. No port, no row
(§K9.13 precedent). `0x19DF0` is reached only through its table dword
`0xBCC84` (entry `BCC7C` + 8).

**Values the tests pin** (`sm_check_keyboard`, 25 scripted frames):

* The slot map through both functions (the values `0x9100 + s`, bit 15 set
  so the getter's zero extension is checked on every slot; fix round 1, the
  brief had `0x1100 + s`) over a `0x5A` fill, `+0` and `0x100CBE` untouched
  after the out-of-range stores (16 and -1), the getter's 0 for 16 and -1,
  and `0x2EBBC` returning `0x1C0D` and zeroing the word.
* KEYS A (18 frames): "morland" into slots 0..6 (`DS_00108113` = 1). At slot
  7, 'D' (`0x2044`) is refused: a shifted letter keeps its scan code, so its
  high byte `0x20` is that of slot 6's 'd' (`0x2064`), the last slot the
  `0x1A3B8..0x1A3DD` loop compares; F1 (`0x3B00`, no name) is refused; then
  'x' replaces slot 7's "<HOME>" in column 0xC (the `0x1A339 jg` switches
  after 7). UP goes into slot 8, whose own old word has scan `0x48` (only the
  lower slots are compared); RGT, DOWN, LEFT, j into 9..12; Enter keeps slot
  13's 'k'; i, u into 14, 15. All sixteen words, the applied mirror (slot 0's
  scan and ascii) and BIOS record (slot 0's scan at `+0x2DE`, slot 9's at
  `+0x2E6 + 3`), the titles in `0x2000`, three labels, the names in `0xF000`
  at both columns (slot 13 redrawn after Enter), and slots 7's "<x>" and 12's
  "<j>" with their old "<HOME>" tails released by the blanks.
* KEYS B (7 frames): "spaten" into slots 0..5 (FREE PLAY = 1), then Esc on
  slot 6: EAX = 0, the six words stored, the mirror and BIOS record keep the
  seed (not applied), slot 6's old "<HOME>" left highlighted in `0x3000`,
  and slot 9's (`0x100CC6`) initial "<DOWN>" at (0x20, 9).

**Not tested** (the first version of KEYS A refused an 'M' against slot 0's
'm'; fix round 1 removed that slot-0 duplicate case and traded it for the
slot-6 clash below, so the lower-bound mutation 36 now survives and is listed
here): the `0x1A331` drop of a stale key word (it shows only when the
first frame brings no key: one more frame over the brief's 25); the wait on a
zero key word (`0x1A39A`; the checks after it refuse a zero word anyway, so it
is not separable); **the duplicate loop's lower bound**: the one refused
duplicate clashes with slot 6 (j = slot - 1), so a loop starting at j = 1
survives (mutation 36); a clash with slot 0 costs one more frame; **the slot
at which "morland" is tested**: KEYS A goes on past slot 7, so testing it
after slot 7 instead of 6 also sets the flag (mutation 33 survives; pinning it
needs an Esc run of its own, 8 frames); an Enter refused by the duplicate or
name check; a key whose ascii is `0x0D` or `0x1B` from another scan code
(keypad Enter `0xE00D` is Enter); the partial matches of either code word
(only the full words are run: a first mismatch at each letter is not);
"spaten" in a run that also completes slot 15; the labels other than UP,
RIGHT and player 2's LO FIERCE; the unrolled initial draws of slots other
than 9 (B) as drawn before any edit.

**Frame budget.** 25 scripted frames in `sm_check_keyboard` (KEYS A 18, KEYS
B 7); the slot and take checks present no frame. K11 total: 106 + 25 = 131 of
160.

**Red, then green.** With the tests and the header in place and no port, the
build failed at the link on the four new symbols
(`<scratchpad>/k11_t6_red.txt`). With the port, `PR_ORACLE_REQUIRED=1
./build/run_tests` printed "all checks passed" on the first run
(`k11_t6_green.txt`).

**Mutations** (each applied, rebuilt, run and reverted by
`<scratchpad>/k11_t6r1_mut.py`; log `k11_t6r1_mut_run.txt`, table
`k11_t6r1_mut_table.txt`). The first run (`k11_t6_mut.py`, 30 mutations, all
failing) was repeated on the fix-round-1 tests with six more. The count is the
`test_game.c` FAIL lines, as measured (the closing `FAILURES: N` line is not
counted). 34 of 36 fail the suite with exit 1 and no crash; 33 and 36 survive
and are the two untested arms named above. The brief's four are 1, 2 (slots 9
and 10 swapped), 3 (the getter's -1 above 15), 8 (`0x2EBBC` without its clear)
and 9, 10, 11 (wrong Enter/Esc arms).

| # | mutation | result |
|---|---|---|
| 1 | 0x19C60 slots 9 and 10 swapped | 7 checks |
| 2 | 0x19D34 slots 9 and 10 swapped | 3 checks |
| 3 | 0x19D34 returns -1 above 15 | 2 checks |
| 4 | 0x19D34 slot 3 sign-extended | 1 check |
| 5 | 0x19C60 above 15 writes +0 | 1 check |
| 6 | 0x19C60 signed bound (-1 stores slot 0) | 1 check |
| 7 | 0x19D34 signed bound (-1 reads slot 0) | 1 check |
| 8 | 0x2EBBC without its clear | 1 check |
| 9 | 0x19DF0 Esc applies the record | 2 checks |
| 10 | 0x19DF0 Esc and Enter compares swapped | 9 checks |
| 11 | 0x19DF0 Enter reload dropped | 9 checks |
| 12 | 0x19DF0 duplicate compare on the whole word | 15 checks |
| 13 | 0x19DF0 duplicate scan includes the slot itself | 16 checks |
| 14 | 0x19DF0 duplicate refusal dropped | 15 checks |
| 15 | 0x19DF0 name refusal dropped | 17 checks |
| 16 | 0x19DF0 seven blanks dropped | 2 checks |
| 17 | 0x19DF0 highlight drawn in 0xF000 | 1 check |
| 18 | 0x19DF0 accepted name not redrawn | 7 checks |
| 19 | 0x19DF0 "spaten" store dropped | 1 check |
| 20 | 0x19DF0 "morland" store dropped | 1 check |
| 21 | 0x19DF0 "spaten" tested after slot 4 | 1 check |
| 22 | 0x19DF0 final 0x1AE28 dropped | 4 checks |
| 23 | 0x19DF0 column switch after slot 8 | 2 checks |
| 24 | 0x19DF0 key rows from 6 | 7 checks |
| 25 | 0x19DF0 labels from the second id | 2 checks |
| 26 | 0x19DF0 LEFT PLAYER in 0x1000 | 1 check |
| 27 | 0x19DF0 player 2 initial names in 0x1000 | 1 check |
| 28 | 0x19DF0 registration dropped | 1 check |
| 29 | 0x19DF0 player 2 labels at column 0x14 | 1 check |
| 30 | 0x19DF0 "morland" last letter 'e' | 1 check |
| 31 | 0x19DF0 duplicate loop bound j + 1 < slot (fix round 1, R1) | 15 checks |
| 32 | 0x19DF0 duplicate loop compares slot 0 only (fix round 1, R2) | 15 checks |
| 33 | 0x19DF0 "morland" tested after slot 7 (fix round 1, R3) | **survives** (listed under Not tested) |
| 34 | 0x19D34 slot 12 sign-extended (fix round 1, R7) | 1 check |
| 35 | 0x19DF0 column switch after slot 6 (fix round 1, R10) | 2 checks |
| 36 | 0x19DF0 duplicate loop starts at j = 1 (fix round 1) | **survives** (listed under Not tested) |

**Gate (Task 6).** `make verify` exited 0 (`<scratchpad>/k11_t6_verify.txt`).
Its oracle lines (`k11_t6_or.txt`) equal §K11.5's and ledger §A's, except the
two unittest wall-clock lines (`Ran 10 tests in 0.107s`, `Ran 33 tests in
1.219s`). The new `fn_register` runs in `game_init`, so the 8000-frame dump
was compared: `--check 8000` gives the same `shasum` list over 8000 `.idx` and
8000 `.pal` files as `k11_t5_frames_after.sha` (FRAMES-IDENTICAL). The header
grep counts 1 for each of `19C60 19D34 2EBBC 19DF0`. `tools/port_progress.py`
stays at `765 1203 64` / `729 730 100`: none of the four is a Ghidra
function. **Fix round 1** (committed as `13a99e8`): `make verify` exited 0
again on that tree (`<scratchpad>/k11_t6r1_verify.txt`); its oracle lines
(`k11_t6r1_or.txt`) equal the first run's and ledger §A's, except the two
unittest wall-clock lines (`Ran 10 tests in 0.104s`, `Ran 33 tests in
1.184s`). The fix round changed `port/tests/test_game.c`, the docs and
comments in `svcmenu.h` only (no code on a frame path), so no frame dump was
repeated.

## §K11.7 Cycle 6: STATISTICS pages 1 and 2 (executor, Task 7)

Six functions, re-read from the fixed-up image (`k11_dx.py`; the tables and
strings with `k11_str.py`). Each has exactly one rel32 caller and no absolute
reference: `0x328B8` from `0x3304D`, `0x32F54` from `0x33048`, `0x32F98` from
`0x331BD`, `0x33458` from `0x331A8`, `0x33058` from `0x33563` and `0x33230`
from `0x3356A` (both in `0x33560`, reached by `0x2CAC5 jmp` from `0x2CAC0`,
which sets EAX = 1). None is a table callback, so none is registered.

**(a) The code-object tables.** `0x32644` holds five 8-byte rows `{u32 string
id, u8 field, 3 bytes}`, ended by `{0, 0xFF}` at `0x3266C`; `0x33058` walks
exactly five (EBP 0..0x28, `0x3319D`), so the end row is not read:

| row | id | field |
|---|---|---|
| `32644` | `0x8F` "Idle Mins           :" | 3 |
| `3264C` | `0x90` "1 Player Mins       :" | `0x12` |
| `32654` | `0x91` "2 Player Mins       :" | `0x13` |
| `3265C` | `0x92` "New Game Mins       :" | `0xA` |
| `32664` | `0x93` "Cont Game Mins      :" | `0xC` |

`0x32674` holds nine such rows (ended by `{0, 0xFF}` at `0x326BC`; EDI
0..0x48, `0x33378`): `0xA1` "1 player games" field 8, `0xA2` "2 player
games" 9, `0xA3` "1 pl continues" 6, `0xA4` "2 pl continues" 7, `0xA5` "1 pl
finishes" `0xE`, `0xA6` "Challenge games" `0xD`, `0xA7` "Sudden deaths"
`0xF`, `0xA8` "Final battles" `0x10`, `0xA9` "Final continues" `0x11`.
`0x326C4` holds four 12-byte rows `{u32 id, u8 numerator field, u8 0, u16
denominator field 1, u16 denominator field 2 (0 = none), u16 0}` (ended at
`0x326F4`; EDI 0..0x30, `0x33548`): `0x94` "Ave New 1 pl time" `0xA / 8`,
`0x95` "Ave Cont 1 pl time" `0xC / 0xB`, `0x96` "Ave 1 pl game time" `0x12 /
(8 + 6)`, `0x97` "Ave 2 pl game time" `0x13 / (9 + 7)`. The field widths
(descriptors at `0x2D300`): 3, 4, 5 are 20 bits; 6..9, `0xB`, `0xD..0x11`
16 bits; `0xA`, `0xC` 24 bits; `0x12`, `0x13` 32 bits.

**(b) `0x328B8`** (EAX secs, EDX w): minutes `m = secs / 60` (`div`),
`0x2F464(m & 0xFFFF, (w & 0xFFFF) - 3, pad 1, 0xF000)` (`0x328CB..0x328E5`;
`xor edx,edx; mov dx,bx`), `":"` (`0x80BDC`) through `0x2F198(-1, -1, ..,
0xF000)` (row -1: at the cursor), then `0x2F464((secs - m * 60) & 0xFFFF, 2,
pad 0, 0xF000)`. Against `0x2EFD4`'s pad table (actors.c): pad 1
right-justifies with `' '`, pad 0 with `'0'`, so 125 s with w = 5 is " 2",
":", "05", as the brief says; a `' '` releases its cell. Its only caller
passes w = 6.

**(c) `0x32F54`** re-read from `0x32F56..0x32F90`: `c = 0x2CA78()`; zero
returns it (`0x32F5F je 0x32F92`); else `edx = 2 * field 5 + field 4`, `eax =
((edx << 4) - edx) << 2` = 60 * edx, `div (c & 0xFFFF)`. The formula is the
brief's. **Correction (raw wins): `0x2CA78` is `xor eax,eax; ret`**
(`config_credit_zero`, record §48-E), so `0x32F54` always returns 0 and its
division is dead. The brief's "seed c = 7, expect 60" cannot be run: there is
nothing for `0x2CA78` to read. The test asserts 0 with fields 4 and 5 seeded.

**(d) `0x32F98`** (EAX col, EDX row). **Correction (raw wins): it takes no
mode**: EBX is zeroed (`0x32FA6`) and ECX overwritten (`0x32F9F mov
ecx,edx`) before either is read (the caller's ECX = `0x1000` at `0x331B8` is
dead); the port is `svc_stats_play(col, row)`. `sum = field 4 + field 5`
(EDX 4..5, `0x32FA1..0x32FB5`), `all = sum + field 3`, `num = 100 * (sum &
0xFFFF)` (`0x32FC4..0x32FD7`), `pct = all ? num / all : 0` (`idiv`). Then
"Percentage Play     : " (`0x8A`) at **(col, row + 2)** and `0x2F464((s16)pct,
0xB, pad 3, 0xF000)` after it; "AVG TIME/COIN       : " (`0x8B`) at **(col +
1, row + 1)** (`0x33036 lea edx,[ebp+1]`, `0x33039 inc esi`), then
`0x328B8(0x32F54(), 6)`. So AVG TIME/COIN is drawn one row above Percentage
Play and one column right. The `je 0x33052` at `0x33025` tests the flags
left by `0x2F464`, whose last flag-setting instruction is `0x2F485 add
esp,0x14` (never zero): it always falls through.

**(e) `0x33458`** (EAX row, returns row + 4): per row, the label at (4, row)
in `0xF000`; `d = field(f1) + field(f2)` when f2 != 0, else `field(f1)`
(`0x334A6..0x334C7`); when d != 0, `v = (s32)field(num) / (s32)(d & 0xFFFF)`
(`idiv`, `0x334CD..0x334E2`), else v = d = 0. `t = v & 0xFFFF`, then
`0x2F434(0x24, row, t / 60, 2, pad 1, 0xF000)`, `":"` at (0x26, row) and
`0x2F434(0x27, row, t % 60, 2, pad 0, 0xF000)`. **Named gap (PORT):** a
non-zero d with a zero low word (reachable with f2 set: e.g. fields 8 + 6 =
0x10000) makes the raw's `idiv` fault (#DE, DOS/4GW aborts); the port draws 0.

**(f) `0x33058`** (STATISTICS page 1). **Correction: EAX is not read**
(`0x33066 call 0x2EA74` is first); the port is `svc_stats_page1(void)`. EBX =
1 (the redraw flag). The exits, `0x3306B..0x330A7`: per pass `0x2EA74`, then
the latched key `0x2EB80`: non-zero and `0x1B` or `0x0D` leaves; then
`0x2EDE0(0x2000000, 0)`: with the Esc bit (a **new** pad Esc, as `0x50161`
reports a masked bit only on its edge), `0x2EDE0(0, 0)` (the level) without
`0x1000000` leaves; with Enter held it stays. §0.5's "Esc or Enter (the
latched key) leaves" is right but incomplete. With EBX set: `0x52106(EAX)`,
`0x2F99C`, title `0x81` centred on row 0 in `0x5002`, the five rows from row 3
(label at column 4 in `0xF000`; the value, divided by 60 with `idiv` for
fields `0xA`, `0xC`, `0x12`, `0x13`, is drawn `& 0xFFFF` by `0x2F434(0x24,
row, .., 0xB, pad 3, 0xF000)`), `0x33458(8)`, `0x32F98(4, (s16)(12 + 1))`,
"PRESS ESCAPE KEY" (`0x209`) and "for more stats" (`0x82`) centred on rows
`0x1B`/`0x1C` in `0x1000`; EBX = 0. Exit (`0x33204`): "EEPROM ERROR"
(`0x9F`) released from (0x1B, 0xC) in `0x3000`. The `field == 0x24 && (value
& 0xFFFF) > 0x4B` arm (`0x33169..0x33194`, which draws `0x9F` there) is dead:
no row of `0x32644` has field `0x24`. It is ported as written.

**`0x52106`'s argument** (`0x330B1`, and `0x332E4` in page 2) is pinned: EAX
is the last `0x2EDE0` result, the Esc-masked poll's when it had no Esc bit,
else the level poll's. **It has no visible effect:** `0x2F99C`, called next,
runs `0x2BAF4` with EAX = 1, whose non-zero arm calls `0x52106(0)`
(`0x2BBEA`, actors.c): the ticks, both offscreen buffers, the DAC and the
aperture are written again with 0. The port passes the value and tests
nothing of it (a first test asserting `DS_0010150C` = the key word failed
for exactly this reason and was dropped).

**(g) `0x33230`** (page 2 "MORE STATISTICS", EAX = clear_ok in `[esp+4]`).
The exits are page 1's (`0x33247..0x3325C`, `0x332B5..0x332D6`). Between
them, with clear_ok (`0x33262`): when the level `0x2EDE0(0, 0)` has both
`0x3000000` bits (Esc and Enter held, `and`/`cmp` at `0x33272..0x3327C`), it
loops `0x2EDE0(0, 0)` + `0x2EA74` while Esc is held (`0x33280..0x33295`),
then **zeroes config fields 0..0x27** (`0x2DA0C(i, 0)`, ESI 0..0x27 with
`inc` before the call and `cmp esi,0x28; jl`, `0x33297..0x332A8`), clears
clear_ok and sets the redraw flag. The draw: `0x52106(EAX)`, `0x2F99C`, title
`0xAA` in `0x5002`, the nine rows from row 3 (label at column 4, value
`field & 0xFFFF` by `0x2F434(0x20, row, .., 0xB, pad 3, mode)`), the mode
EBP starting `0xF000` and alternating with `0x4000` (`0x3335E..0x33372`), the
two footer lines, and with clear_ok the three hints `0x69` "HOLD LEFT PL
UPPER LEFT", `0x6A` "AND PRESS LEFT PL UPPER RIGHT", `0xA0` "to clear ALL
statistics" centred on rows `0x18..0x1A` in `0x4000`. Exit as page 1.
The fields it clears include the play-time and audit fields pages 1 and 2
show, and also every other field 0..0x27 (the brief's "the §K11.7 fields").

**The modes of page 2 are invisible.** `0x2F5A0` (`text_glyph_emit`) picks
the font palette from the mode's high nibble, and `0x4000` and `0xF000` both
select `0x8099A4` (actors.c, the class-0 switch), so the alternating rows
are drawn identically; the test cannot tell them apart (mutation 46).

**Values the tests pin** (`sm_check_stats`, 13 scripted frames). The fields
seeded for page 1: 0 = 7, 3 = 0x10005, 4 = 0x10000, 5 = 0xFFFF, 6 = 3, 7 = 4,
8 = 2, 9 = 0xFFFF, 0xA = 250, 0xB = 0, 0xC = 999, 0x12 = 3932580, 0x13 = 600
(each read back).

* `0x328B8(125, 5)` at the cursor (4, 5): a seeded glyph at column 5
  released by the pad `' '`, then '2', ':', '0', '5' in columns 6..9.
* `0x2CA78` = 0 and `0x32F54` = 0.
* P1A (3 frames: a new Esc with Enter held, nothing, a new Esc alone): the
  title; rows 3..7 with field 3 & 0xFFFF = 5 (not divided), `0x12` / 60 =
  65543 & 0xFFFF = 7 (column 0x25 empty: pad 3), `0x13` 10, `0xA` 4, `0xC`
  16; `0x33458` from row 8: " 2:05" (250 / 2), " 0:00" (a zero sum), " 1:24"
  (3932580 / 5 = 786516, & 0xFFFF = 84), " 3:20" (600 / ((0xFFFF + 4) &
  0xFFFF)); `0x32F98(4, 13)`: AVG TIME/COIN at (5, 14) with "  0:00" from
  column 27, Percentage Play at (4, 15) with "33" (100 * 0xFFFF / 0x30004) at
  columns 26..27 (field 3 dropped gives 49, the `& 0xFFFF` dropped 66); the
  two footers. A glyph planted by the harness probe after the first draw is
  still there on the next frame: the pass with the flag clear draws nothing.
* P1B (latched Enter) and P1C (latched Esc), 1 frame each: no draw (a seeded
  glyph and the tick `DS_0010150C` kept), and cells 0x1B..0x26 of row 0xC
  released, 0x27 kept.
* P2A (clear_ok, Esc held throughout via `sm_held`, so never new; 5 frames):
  Esc alone, Enter alone, Esc + Enter, Enter alone (Esc released), then the
  latched Enter. A per-frame probe (`sm_probe`, new in the harness: called
  with the frame index before that frame's step is applied) sees the hints of
  the first draw, no tenth row (a tenth would read the end row `{0, 0xFF}`
and draw field `0xFF`'s -1 as 65535 at (0x20, 12); row 12 = `0xC` is looked
at here because the exit's release of "EEPROM ERROR" covers its columns
0x1B..0x26), a planted glyph kept (no redraw on the second pass), fields
  0 and 0x27 still seeded at probes 1, 2 and 3 (frames numbered from 0: the
  probe of frame f runs before step f; neither key alone clears, and the
  clear waits for Esc's release), and field 0 cleared at probe 4. After:
  fields 0..0x27 are 0 and field 0x28 keeps its 5; the redraw shows the
  labels (rows 3, 4, 5, 11), the values 0 (pad 3) and no hints.
* P2B (clear_ok = 0; 2 frames: Esc + Enter, then Esc): no clear and no exit
  on the first frame, the values 2 and 65535, no hints; the new Esc leaves.
* P2C (clear_ok = 1; the latched Esc, 1 frame): no draw, the release, the
  fields kept.

So each page's three exits run once: page 1 the latched Enter (P1B), the
latched Esc (P1C) and a new pad Esc (P1A); page 2 the latched Enter (P2A),
the latched Esc (P2C) and a new pad Esc (P2B). Loop bounds: page 1's five
rows (first and last drawn), `0x33458`'s four (first and last, and the play
rows' position), `0x32F98`'s fields 4..5 (both ends), page 2's nine rows
(first, last and none after) and the clear's 0..0x27 (field 0, field 0x27,
field 0x28 kept).

**Not tested:** `0x32F54`'s division arm (dead: `0x2CA78` is constant 0),
and so its `c == 0` test (mutation 6 survives: with it removed the port
divides by zero, which AArch64's `udiv` answers with 0, the same result; on
x86 it would trap); `0x32F98`'s zero-total **test** (all three fields 0, where the
quotient is 0 either way; mutation 57 survives for the same reason; the
arm's value 0 is tested since §K11.8, mutation 58); the
signedness of each `idiv` (every seeded value is positive); `0x328B8`'s `w &
0xFFFF` (its only caller passes 6); the `#DE` gap of `0x33458` (PORT above);
the dead `field == 0x24` arm of `0x33058`; the argument of `0x52106` (no
visible effect, above); page 2's alternating modes (invisible, above); the
titles beyond one non-empty cell each; the labels other than those named.

**Frame budget.** 13 scripted frames in `sm_check_stats` (P1A 3, P1B 1, P1C
1, P2A 5, P2B 2, P2C 1); `0x328B8` and `0x32F54` are called directly and
present no frame. K11 total: 131 + 13 = 144 of 160, which leaves **16** for
Task 8.

**Red, then green.** With the tests and the header in place and no port, the
build failed at the link on the six new symbols
(`<scratchpad>/k11_t7_red.txt`). With the port, the first run failed three
checks, each on `DS_0010150C` = `0x52106`'s argument: `0x2F99C` runs
`0x52106(0)` right after (above), so those checks were dropped as untestable.
Then `PR_ORACLE_REQUIRED=1 ./build/run_tests` printed "all checks passed"
(`k11_t7_green.txt`).

**Mutations** (each applied, rebuilt, run and reverted by
`<scratchpad>/k11_t7_mut.py`; the final log is `k11_t7_mut_run.txt`). The
count is the `test_game.c` FAIL lines as measured; the suite's closing
`FAILURES: N` line is not counted. Three runs: the first (55 mutations,
`k11_t7_mut_run1.txt`) left six survivors, 6 and 46 (untestable, below) and
four weak tests. 10: field 3 = 1 gave 49 with or without it in `0x32F98`'s
total (it is now `0x10005`: 33 against 49). 35: page 1's no-redraw pass was
not observed (the harness probe now plants a glyph after the first draw).
42: P2A's Esc-alone frame came just before the combination, so the
Esc-alone clear waited for the release into the same pass (the order is now
Esc alone, then Enter alone). 48: see the second run. The second run
(`k11_t7_mut_run2.txt`) added 56 (page 2's no-redraw pass) and 57 and left
48 as well: a tenth page-2 row draws on row `0xC`, which the exit's release
of "EEPROM ERROR" clears, so its check moved into the probe. The third run
(`k11_t7_mut_run.txt`, the table below) is on the final suite. Of 57, 54
fail the suite with exit 1 and no crash; 6, 46 and 57 survive (Not tested,
above). The table is a snapshot of this suite: later tasks can add checks that
raise a count, but not change a verdict. The brief's four: `/ 60` as `/ 64`
(1), the seconds pad as `' '` (2), `0x32F54`'s `c == 0` test removed (6: it
survives on this AArch64 host, where the division by zero returns 0 and
does not trap), and the page-1 exit on Esc only (23).

| # | mutation | result |
|---|---|---|
| 1 | 0x328B8 / 60 as / 64 | 2 checks |
| 2 | 0x328B8 seconds pad as space (1) | 2 checks |
| 3 | 0x328B8 minutes pad as zero (0) | 2 checks |
| 4 | 0x328B8 minutes width w - 2 | 7 checks |
| 5 | 0x328B8 colon dropped | 5 checks |
| 6 | 0x32F54 c == 0 test removed | **survives** (listed under Not tested) |
| 7 | 0x32F54 returns 1 for c == 0 | 2 checks |
| 8 | 0x32F98 sum fields 4..6 (upper bound + 1) | 2 checks |
| 9 | 0x32F98 sum fields 5..5 (lower bound + 1) | 2 checks |
| 10 | 0x32F98 field 3 dropped from the total | 2 checks |
| 11 | 0x32F98 numerator without (u16) | 2 checks |
| 12 | 0x32F98 AVG row at (col, row + 1) | 5 checks |
| 13 | 0x32F98 Percentage row at row + 1 | 3 checks |
| 14 | 0x32F98 AVG passes width 5 | 4 checks |
| 15 | 0x33458 second field always added | 5 checks |
| 16 | 0x33458 second field never added | 5 checks |
| 17 | 0x33458 denominator without & 0xFFFF | 2 checks |
| 18 | 0x33458 quotient without & 0xFFFF | 3 checks |
| 19 | 0x33458 zero sum shows the numerator | 2 checks |
| 20 | 0x33458 five rows (upper bound + 1) | 8 checks |
| 21 | 0x33458 from the second row (lower bound + 1) | 18 checks |
| 22 | 0x33458 seconds pad as space | 1 check |
| 23 | 0x33058 exit on Esc only (Enter compare dropped) | 5 checks |
| 24 | 0x33058 exit on Enter only (Esc compare dropped) | 5 checks |
| 25 | 0x33058 pad Esc leaves with Enter held | 40 checks |
| 26 | 0x33058 pad Esc exit dropped | 2 checks |
| 27 | 0x33058 field 3 divided by 60 | 2 checks |
| 28 | 0x33058 field 0xC not divided | 2 checks |
| 29 | 0x33058 value without & 0xFFFF | 4 checks |
| 30 | 0x33058 value pad 1 (right-justified) | 7 checks |
| 31 | 0x33058 exit release dropped | 4 checks |
| 32 | 0x33058 play rows at next (no + 1) | 8 checks |
| 33 | 0x33058 four rows (upper bound - 1) | 21 checks |
| 34 | 0x33058 from the second row (lower bound + 1) | 29 checks |
| 35 | 0x33058 redraws every pass | 1 check |
| 36 | 0x33058 draw before the exit tests | 6 checks |
| 37 | 0x33230 clear up to 0x26 (upper bound - 1) | 1 check |
| 38 | 0x33230 clear up to 0x28 (upper bound + 1) | 1 check |
| 39 | 0x33230 clear from 1 (lower bound + 1) | 2 checks |
| 40 | 0x33230 Esc-release wait dropped | 1 check |
| 41 | 0x33230 clear on Enter alone | 1 check |
| 42 | 0x33230 clear on Esc alone | 4 checks |
| 43 | 0x33230 clear offered without clear_ok | 8 checks |
| 44 | 0x33230 clear_ok kept after the clear | 1 check |
| 45 | 0x33230 hints drawn without clear_ok | 2 checks |
| 46 | 0x33230 modes do not alternate | **survives** (listed under Not tested) |
| 47 | 0x33230 eight rows (upper bound - 1) | 2 checks |
| 48 | 0x33230 ten rows (upper bound + 1) | 1 check |
| 49 | 0x33230 from the second row (lower bound + 1) | 8 checks |
| 50 | 0x33230 exit on Esc only | 2 checks |
| 51 | 0x33230 exit on Enter only | 4 checks |
| 52 | 0x33230 pad Esc exit dropped | 2 checks |
| 53 | 0x33230 exit release dropped | 1 check |
| 54 | 0x33230 value pad 1 (right-justified) | 6 checks |
| 55 | 0x33230 no redraw after the clear | 4 checks |
| 56 | 0x33230 redraws every pass | 1 check |
| 57 | 0x32F98 zero-total test removed | **survives** (listed under Not tested) |
| 58 | 0x32F98 zero-total value 1 (added in §K11.8, measured on its final suite) | 1 check |

**Gate (Task 7).** `make verify` exited 0 on the final tree
(`<scratchpad>/k11_t7_verify.txt`). Its oracle lines (`k11_t7_or.txt`) equal
§K11.6's fix-round run and contain every oracle line of ledger §A, except the
two unittest wall-clock lines (`Ran 10 tests in 0.097s`, `Ran 33 tests in
1.114s`). No 8000-frame dump was compared: `game_init` and
`svcmenu_register` are unchanged (none of the six is a table callback), and
the six are reached only from `0x33560`, which is not ported yet, so no frame
path runs them. The header grep counts 1 for each of `328B8 32F54 32F98
33458 33058 33230`. `tools/port_progress.py` stays at `765 1203 64` / `729
730 100`: none of the six is a Ghidra function.

## §K11.8 Cycle 7: the histograms and the STATISTICS entry (executor, Task 8)

Seven functions, re-read from the fixed-up image (`k11_dx.py`; listings
`<scratchpad>/k11_t8_dis_2e248.txt`, `k11_t8_dis_2e5e4.txt`,
`k11_t8_dis_32bdc.txt`). Callers (rel32 scan and absolute dwords): `0x2E218`
from `0x2E66E`; `0x2E11C` from `0x32F02`; `0x2E248` from `0x32C18`; `0x2E5E4`
from `0x32C6A` and `0x32CAE`; `0x32BDC` from `0x33571`; `0x33560` from the
tail `jmp` at `0x2CAC5`; `0x2CAC0` only through its table dword `0xBCC44`.
So `0x2CAC0` is registered (`fn_register`) and the other six are reached
from it. `config_storage_touch` (`0x2D4EC`, the declared no-op) is exported
from `config.c` for `0x2E11C`; its body is unchanged.

**(a) The descriptors.** `0x2D414` holds three 0x10-byte histogram
descriptors: `+0` the template, `+4` the first bucket's width (d4), `+8` the
other buckets' width (d8), and a dword whose byte `+0xE` is the bucket count
(the brief's "flags" `0x140001`/`0x140002`/`0x70004`: count 0x14, 0x14, 7;
the low byte 1, 2, 4 is not read by any of the seven):

| desc | template | d4 | d8 | count |
|---|---|---|---|---|
| `2D414` | `0x80A24` "Round Time (in seconds)" | 10 | 10 | 20 |
| `2D424` | `0x80A3C` "Match Time (in seconds)" | 15 | 15 | 20 |
| `2D434` | `0x80A54` "Selects Per Character:\tSAURON \tBLIZZARD \tTALON \tVERTIGO \tARMADON \tDIABLO \tCHAOS " | 1 | 1 | 7 |

`0x2D444` holds 8-byte storage descriptors `{u16, u16 size, u32 address}`;
histogram g uses entry g + 3: `2D45C` {`0xB6`, 0x14, `0x105ECD`}, `2D464`
{`0xD0`, 0x14, `0x105EE1`}, `2D46C` {`0xEA`, 7, `0x105EF5`}. The first word
is not read here. The screen titles are the strings at `0xBD460` (`0x238`
"Round Time <in seconds>", `0x239`, `0x23A` "Selects Per Character: "), not
the templates. The bar string `0x32BFF` pushes is the code-object `0x32640`
= `"\x03"` (raw bytes `03 00`, the same before and after fixups), one cell
wide. Fixed strings: `0x80B20` "#:" (the default bar string), `0x80B24` "-",
`0x80B28` "& UP", `0x80B30` ": " (each copied by length, so the junk bytes
between them are never read).

**(b) `0x2E218`** (EAX v): 1 for v < 10 (`0x2E227 jl`, signed, so a negative
v is 1); else it multiplies by 10 and counts while v >= the power (`jge`)
and the count is below 10 (`0x2E237`). 999999999 is 9, 10^9 and `0x7FFFFFFF`
are 10.

**(c) `0x2E11C`** (EAX g): -1 for g >= 3 (`0x2E123 jb`, unsigned) with
nothing written. Else `DSB(0x105DD8 + (g+3) >> 3) |= 1 << ((g+3) & 7)`
(bits 3, 4, 5), `memset(ptr, 0, size)` of storage descriptor g + 3 (the
runtime `0x61A70`, `mem_fill` with a `PORT:` note), `0x2D4EC(g + 3)`, and 0.

**(d) `0x2E248`** (EAX g, EDX buf, EBX size, ECX max_out; stack: median_out
at `[esp+0x34]`, the bar string at `[esp+0x38]`; `ret 8`). The state block
`0x105D64` (no `symbols.h` name, a local define): `+0` g + 1 (zeroed first at
`0x2E26A`, set last at `0x2E5B2`), `+4` the largest count, `+8` the sum,
`+0xC` the label column's width, `+0x10`/`+0x14` the digits of a range's low
and high bound, `+0x18` the template's first tab (0 = none), `+0x1C` the bar
string (0 selects "#:"), `+0x20` its length. -1 when g >= 3, buf is 0 or
size < 1 (signed). The title is copied up to a NUL or a tab
(`0x2E2DC..0x2E2F8`, `jb` on buf + size); a title that fills `size` returns
-1 without a NUL. With a tab (histogram 2): each of the count columns runs
from its tab to the next tab or NUL, and `+0xC` is the widest including the
tab (10, "\tBLIZZARD "); -1 when the template ends before the last column or
goes on after it. `+0x10`/`+0x14` are not written on this path. Without a
tab: -1 for a count below 2; `last = d8 * (count - 2) + d4 - 1`, `+0x10 =
digits(last + 1)`, and when d8 = 1 `last = d4 - 1`; `+0x14 = digits(last)`
(0 when last = 0) and `+0xC = +0x10 + 1 + +0x14` (`+0x10` when last = 0).
When `+0x10 + 4 > +0xC` (unsigned), `+0xC` becomes `+0x10 + 4` if `+0xC -
+0x10 > 2`, else at least `+0x10 + 1`. Then `+0xC += 2`. Histograms 0 and 1:
`+0x10 = +0x14 = 3`, `+0xC = 9`. The counters: bucket i of the count
(`0x2E4ED..0x2E510`: -1 past the storage size, which returns -1) are
summed into `+8`, the largest (unsigned) into `+4` and `[max_out]`. The
median: `half = (sum + 1) >> 1` (`sar`), then the first bucket with `half <=
count` (signed `jle`), subtracting each count passed; stored in
`[median_out]`. The result is the template's length, or with a tab the
title's length - 1 (`0x2E5B8 lea edi,[edx-1]; sub edi,[ecx]`): 23, 23, 21.

**(e) `0x2E5E4`** (EAX i, EDX buf, EBX width; returns the count or -1): -1
when the state block names no histogram (`+0 - 1 >= 3`), bucket i is past
the storage size, buf is 0, or the bar `width - +0xC - digits(+4) - 7` is
below 1. **Bucket i is bounded by the storage size, not the count:** with the
stock descriptors the two are equal. The line: with a tab, column i's name
right-aligned in `+0xC` cells; else the low bound `lo` (0 for i = 0, else
`(i - 1) * d8 + d4`) in `+0x10` cells (`0x2EFD4` pad 1), `+0xC - +0x10`
spaces, and over them: when `lo != hi` (`hi` = d4 - 1 for i = 0, else lo +
d8 - 1) and i is not the last, "-" and hi in `+0x14` cells and a space; for
the last, "& UP" at `(e + 1) >> 1` with `e = +0xC - +0x10 - 6`, or '+' when
e < 0; then ": " over the last two cells. Then the count in
`digits(+4)` cells, a space, `100 * count / sum` (`idiv`; 0 for a zero sum) in
3 cells, "% ", and the bar: `units = bar * L * count + unit / 2` with `unit =
max(+4, 4 * L)` (unsigned), one bar[0] per `L * unit`, then bar[q] for `q =
rest / unit` when q > 0 (a half cell: only with L >= 2, so never with the
game's `"\x03"`), and a NUL. So histogram 0's bucket 0 with the counts
below is `"  0-  9:  3   4% \x03\x03"`, the last bucket `"190& UP:  7   9% "`
and six bars, and histogram 2's column 1 `" BLIZZARD 255  94% "` and 22 bars.
The model these lines were checked against is `<scratchpad>/k11_t8_model.py`,
written from the listings, not from the port.

**(f) `0x32BDC`** (EAX a). The frame: a 0x2A-byte line buffer at `[esp]`,
the largest count at `[esp+0x30]`, the median at `[esp+0x34]`, the
histogram index as the word `[esp+0x3C]`; the port keeps it at the scratch
`0x039000C0` with the same offsets (`PORT:`). For h = 0, 1, 2: `0x52106(EAX)`,
`0x2F99C`, `0x2E248(h, buf, 0x2A, &max, &median, "\x03")`; the title at
column `(0x28 - strlen) >> 1` (`shr`), row 0, mode `0x1000` (**not** the
-1 centring: column 8 for all three); the lines from row 2 at column 2
until `0x2E5E4` returns -1, the median bucket's in mode `0x3000` and the
others in `0x2000`, their counts summed; then the median's line again,
**cut at its first ':'**, and only when it has one: "MEDIAN:" (`0x83`) at
(0xF, n + 3) in `0x3000` and the range at (0x16, n + 3) in `0x1000`. A
tabbed line has no ':', so **histogram 2 shows no MEDIAN** (the plan's §0.5
lists MEDIAN for all three). "TOTAL:" (`0x84`) at (3, n + 3) and the sum
through `0x2F434(0xB, n + 3, total, 5, pad 3, 0x1000)`. Footers: h < 2:
`0x209` and `0x87` "for next histogram" on rows `0x1B`/`0x1C`; h = 2:
`0x209` and `0x20A` "TO EXIT MENU", and with a the hints `0x69`, `0x6A`,
`0x86` "to clear ALL histograms" on rows `0x18..0x1A` in `0x4000` (§0.5
did not list `0x20A` or `0x86`). The key loop, per pass `0x2EA74`: the
latched Esc or Enter goes to the next histogram (after the last, the
function returns); a new pad Esc (`0x2EDE0(0x2000000, 0)`) goes on for h <
2; on h = 2 it returns when a = 0, or when `0x2EDE0(0, 1)` has no Enter;
else the clear: `0x52106`, `0x2F99C`, "CLEARING ALL HISTOGRAMS" (`0x88`) on
row `0xA` in `0x4000`, `0x209` and `0x20A`, `0x2E11C(0..2)`, then up to 89
passes (`0x5A`, `dec` before the test) that leave at a new pad Esc. A
drawn line of 41 cells from column 2 is cut to 39 by `0x2F830` (the NUL at
`0x2A - col - 1`), so the longest bars lose two cells on screen.
**Interface (raw wins over the brief's `void`):** the port returns the
raw's EAX at the `ret`: the latched key after the last histogram, h = 2
for a = 0, the `0x2EDE0(0, 1)` word without Enter, the release poll's word,
or -1 (`0x2EA78` returns -1 at `0x2EB6E..0x2EB74`) when the wait runs out.
`menu_run` tests a callback's result only for -5 and -10 (`0x304AA`,
`0x304B3`), which no path gives (the poll words sit in `0xFF00FF00` and
bits 24..25). `menu_run` also stores the result in `DS_00107448` (`0x304A5`),
and sets that word to 1 at `0x304EB` before it is read again, so the `u32`
return carries the whole state (`menu.c:392`, `:401`).

**(g) `0x33560`** (EAX a): `0x33058` (EAX not read, §K11.7),
`0x33230(a)`, `0x32BDC(a)`; returns `0x32BDC`'s EAX. **`0x2CAC0`**: `mov
eax,1; jmp 0x33560`.

**§K9.9 revisited.** `0x2E11C` is a second writer of the audit counters
`0x105ECD..0x105EFB` that §K9.9 found inert on the port's path. It writes
only zeros, and only from STATISTICS in the options menu (mode `0x27`),
which no oracle reaches (§0.7), so the counters still stay 0 on every
oracle path. The ledger's `2DF8C` row points here.

**Values the tests pin** (`sm_check_hist`, 16 scripted frames). Direct
calls (no frame): the digit table (brief); `0x2E11C` on groups 0 and 2
(sizes, the untouched neighbours and sentinel `0x105EFC`, the dirty bits
`0x08` then `0x28`) and the refusal of group 3. Counters: histogram 0 {0:
3, 2: 30, 5: 12, 9: 20, 19: 7} (sum 72, largest 30, median 5), 1 {0: 1, 3:
2} (an odd sum 3: the median is bucket 3, and `sum >> 1` would give 0), 2
{1: 255, 2..6: 1..5} (sum 270, median 1). The state block for histograms 0
and 2 field by field (histogram 2's `+0x10`/`+0x14` keep their `0xA5`
seed); five lines of histogram 0, one of 1 (the unit floor `4 * L`: a count
of 1 over a largest of 2 is 6 bar cells, not 13), three of 2; the "#:"
default with its half cells; a bar of 1 (width 19) and of 0 (width 18,
refused); the refusals (g = 3 zeroing `+0`, buf 0, size 0, a title that
fills 23 bytes, no histogram prepared, a bucket past the size) and, with
the code-object descriptors patched and restored, a count of 6 or 8 for
the tabbed template (8 with 8 counters, so only the column parse can refuse
it), 6 counters for 7 columns, a median bucket holding exactly half
(histogram 1 as {0: 2, 3: 1}: bucket 0, by the `jle`), one bucket, one-value
buckets (d4 = d8 = 1: `+0xC` 5, `+0x14` 0, '+' for the last), d4 = 50 (the
`+0x10 + 4` width), d4 = d8 = 100 ("& UP" one cell right) and eleven
10-wide buckets (a 3-digit low and a 2-digit high bound, and bucket 11
formatted past the count); a zero sum's "0%". Scripted:

* A, `0x2CAC0` (7 frames: page 1's latched Enter; page 2 drawn, probed for
  its hints, then its latched Esc; a new Esc with Enter held on histogram
  0; the latched Enter on 1; a new Esc with Enter held on 2; nothing; a new
  Esc). The probes see each histogram screen (titles at column 8, column 7 empty
  on the first;
  whole lines cell by cell, the median line in `0x3000`, the 39-cell cut;
  MEDIAN and its range cut at the ':' for histograms 0 and 1, none for 2;
  the totals 72, 3 and 270; the footers; the hints only on the last) and
  the clear screen with all 47 counters 0, the sentinel kept and
  `DS_00105DD8` = `0x38` before the wait ends. Result `0x2000000`.
* B, `0x32BDC(0)` (3 frames: the latched Esc, a new Esc, a new Esc with
  Enter held on 2): no clear, no hints, "TO EXIT MENU", result 2.
* C, `0x32BDC(1)` (3 frames: the latched Enter, the latched Esc, a new Esc
  alone on 2): no clear, the hints, result `0x2000000` (the `0x2EDE0(0, 1)`
  word).
* D, `0x32BDC(1)` (3 frames: a new Esc, the latched Enter, the latched Esc
  on 2): the three end, no clear, result `0x1B`.

**Not tested:** the `0x2D4EC` call in `0x2E11C` (a no-op); the null
`max_out`/`median_out` arms (a write to address 0 is invisible); the
bucket read's `g >= 3` inside `0x2E248` (dead: tested at entry); the `>`
compares of the largest count and the column width (equal values give the
same result); the separator length 0 (`+0x14 = 0` only when every range is
one value, where no range is written); the half-cell arm with the game's
own bar string (L = 1, never taken; it is tested with "#:"); an empty bar
string (L = 0 would loop or divide by zero in the raw; no caller passes
one); a title longer than 0x28 (the unsigned column); the release wait's 89
passes and its -1 result (89 frames); `0x52106`'s argument (no visible
effect, §K11.7). Three more, from the Task 8 review: the signedness of
the `size < 1` refusal in `0x2E248` (`0x2E27C` `jge`; only size 0 is
tested, so a negative size is not distinguished); the tabbed template's
no-pad arm when the length is at least `+0xC` (`0x2E6CA` `jge`); and the
median scan's past-size -1 bucket (`0x2E582` `jb`). Both are unreachable, as
derived by fix round 1 of Task 9 from `k11_t8_dis_2e248.txt` and
`k11_t8_dis_2e5e4.txt`. `0x2E6CA`: `edi` is the column-name length taken after
the tab and `edx` is `+0xC`; on the tab path `0x2E248` sets `+0xC` (at
`0x2E3BA`) to the widest column length including its tab, then jumps to
`0x2E4CC`, skipping the `+ 2` at `0x2E4C8`, so a name's length is at most `+0xC
- 1` and `edi >= edx` never holds (it could only if the state block or the
template were patched between the two calls). `0x2E582`: the sum loop at
`0x2E4ED..0x2E517` runs first over the same bucket indices (`ebx` below the
count) and the same storage-size word `[desc + 2]`, and returns -1 on any
past-size bucket (`0x2E503`, then `0x2E519`), so the median scan never sees
`eax >= size`.

**Frame budget.** 16 scripted frames (A 7, B 3, C 3, D 3). K11 total: 144 +
16 = **160 of 160**.

**Red, then green.** With the tests and the header in place and
`svcmenu.c` at `e9e2054`, the build failed at the link on `audit_digits`,
`audit_hist_clear`, `audit_hist_format`, `audit_hist_line`,
`svc_stats_hist` and `svc_statistics_entry` (`<scratchpad>/k11_t8_red.txt`).
With the port, the first run failed four checks, all in the test: the
character at index 22 of the title is ')' (the check said 's'), and three
descriptor-case lines were checked against the "\x03" bar while the calls
passed label 0 ("#:"). Both were fixed in the test; then
`PR_ORACLE_REQUIRED=1 ./build/run_tests` printed "all checks passed"
(`k11_t8_green.txt`).

**Mutations** (each applied, rebuilt, run and reverted by
`<scratchpad>/k11_t8_mut.py`; the final log is `k11_t8_mutations.txt`).
The count is the `test_game.c` FAIL lines as measured; the closing
`FAILURES: N` line is not counted. Two runs: the first
(`k11_t8_mutations_run1.txt`) left two weak tests beside the expected
survivor 60. 10: with a count of 8 the tabbed template also failed the
counter sum (7 counters), so the column parse's refusal was not needed (the
case now has 8 counters). 18: no seeded median sat exactly on half (the
`{0: 2, 3: 1}` case was added). The second run, on the final suite, is the
table: of 60, 57 fail the suite with exit 1, 8 and 32 crash it (exit -10,
the suite fails), and 60 survives. The brief's five mutations, by table row and the checks that
fail: row 1 (`>` for `>=` in `0x2E218`, 1 check), row 3 (the dirty bit
without `+ 3`, 4 checks), row 4 (group 3 accepted, 2 checks), row 6 (the tab
stop removed, 96 checks), row 55 (`0x33560` skipping page 2, 165 checks). Rows 59 and 60 are
the Task 7 minor: `0x32F98`'s zero-total value as 1 now fails, and the
test's removal still survives (AArch64 division by zero gives 0).

| # | mutation | result |
|---|---|---|
| 1 | 0x2E218 > for >= (10 gives 1) | 1 check |
| 2 | 0x2E218 at most 9 digits | 2 checks |
| 3 | 0x2E11C dirty bit without + 3 | 4 checks |
| 4 | 0x2E11C group 3 accepted | 2 checks |
| 5 | 0x2E11C one byte too many | 3 checks |
| 6 | 0x2E248 tab stop removed | 96 checks |
| 7 | 0x2E248 title fills the buffer accepted (> for >=) | 2 checks |
| 8 | 0x2E248 default label dropped | crash (SIGBUS, exit -10): label 0 reads the bar string at address 0, and the bar loop writes without end |
| 9 | 0x2E248 too many columns accepted | 1 check |
| 10 | 0x2E248 too few columns accepted | 1 check |
| 11 | 0x2E248 one bucket accepted (< 1) | 1 check |
| 12 | 0x2E248 d8 == 1 arm dropped | 4 checks |
| 13 | 0x2E248 last == 0 arm dropped | 4 checks |
| 14 | 0x2E248 lo + 4 width dropped | 7 checks |
| 15 | 0x2E248 width + 1 for + 2 | 101 checks |
| 16 | 0x2E248 lo + 1 width arm dropped | 3 checks |
| 17 | 0x2E248 a short table accepted | 1 check |
| 18 | 0x2E248 median < for <= | 1 check |
| 19 | 0x2E248 median from sum >> 1 | 43 checks |
| 20 | 0x2E248 median not stored | 92 checks |
| 21 | 0x2E248 tabbed return is the length | 1 check |
| 22 | 0x2E248 state +0 not cleared first | 1 check |
| 23 | 0x2E5E4 bar width - 6 | 11 checks |
| 24 | 0x2E5E4 name right-align dropped | 48 checks |
| 25 | 0x2E5E4 i == 0 high bound d4 | 8 checks |
| 26 | 0x2E5E4 range without the lo != hi test | 1 check |
| 27 | 0x2E5E4 high bound in the low width | 2 checks |
| 28 | 0x2E5E4 '+' arm dropped | 1 check |
| 29 | 0x2E5E4 & UP at e >> 1 | 1 check |
| 30 | 0x2E5E4 ": " dropped | 39 checks |
| 31 | 0x2E5E4 zero-sum percentage 1 | 1 check |
| 32 | 0x2E5E4 unit floor 4 * L dropped | crash (SIGBUS, exit -10): with a largest count of 0 the unit is 0, so `units >= full` never ends |
| 33 | 0x2E5E4 rounding half unit dropped | 9 checks |
| 34 | 0x2E5E4 half cell dropped | 2 checks |
| 35 | 0x2E5E4 count width + 1 | 37 checks |
| 36 | 0x32BDC title centred by 0x2F198 (-1) | 4 checks |
| 37 | 0x32BDC median line mode 0x2000 | 78 checks |
| 38 | 0x32BDC lines from row 3 | 187 checks |
| 39 | 0x32BDC MEDIAN row i + 2 | 19 checks |
| 40 | 0x32BDC MEDIAN drawn without the colon | 1 check |
| 41 | 0x32BDC total not summed | 6 checks |
| 42 | 0x32BDC last-footer on h == 1 | 6 checks |
| 43 | 0x32BDC hints without a | 1 check |
| 44 | 0x32BDC latched Enter ignored | 94 checks |
| 45 | 0x32BDC latched Esc ignored | 7 checks |
| 46 | 0x32BDC pad Esc goes on only on h < 1 | 4 checks |
| 47 | 0x32BDC a == 0 test dropped | 7 checks |
| 48 | 0x32BDC Enter test dropped | 5 checks |
| 49 | 0x32BDC clears two histograms | 2 checks |
| 50 | 0x32BDC release wait dropped | 3 checks |
| 51 | 0x32BDC wait leaves on Enter | 3 checks |
| 52 | 0x32BDC four histograms | 2 checks |
| 53 | 0x32BDC two histograms | 13 checks |
| 54 | 0x32BDC returns 0 | 4 checks |
| 55 | 0x33560 skips page 2 | 165 checks |
| 56 | 0x33560 page 2 with 0 | 1 check |
| 57 | 0x2CAC0 passes 0 | 8 checks |
| 58 | 0x2CAC0 registration dropped | 1 check |
| 59 | 0x32F98 zero-total gives 1 (Task 7 minor 1) | 1 check |
| 60 | 0x32F98 zero-total test removed (Task 7 mutation 57, re-run) | **survives** (Task 7 mutation 57, listed under §K11.7 Not tested) |

**Task 7 review minors (folded in).** (1) `0x32F98`'s zero-total arm is
tested by a direct `svc_stats_play(4, 13)` with fields 3..5 = 0 (no frame):
"0" at (15, 26); §K11.7's table gains row 58. (2) `sm_check_stats`'s P2A
messages no longer claim the modes: the rows are drawn, the alternation is
invisible. (3) P1A's three seeds under the page-1 draw are removed: `0x2F99C`
empties every cell before the draw, so they were no sentinels; the comment
says the empty-cell checks catch a glyph the draw puts there. (4) The probe
comments and §K11.7's P2A bullet number frames from 0. (5) `docs/PROGRESS.md`
now says page 2 draws again after a clear. (6) The §K11.3 and §K11.4 tables
were re-measured at their snapshots with the closing line excluded
(`<scratchpad>/k11_t8_remeasure.py`: §K11.3 at `a3d17a6`, §K11.4 rows 1..37
at `4862074` and 38..42 at `7cb83e6`; logs `k11_t8_re_t3.txt`,
`k11_t8_re_t4a.txt`, `k11_t8_re_t4b.txt`; each unmutated tree gives 0 FAIL
lines). Every §K11.3 row matches (mutation 4: one check, then the harness
exit, which prints no closing line), and §K11.4 rows 1..41 match. §K11.4
row 42 read 44 and measures 43: it counted the closing line. It is corrected.

**Gate (Task 8).** `make verify` exited 0 on the final tree
(`<scratchpad>/k11_t8_verify.txt`). Its oracle lines (`k11_t8_or.txt`) equal
§K11.7's run at `e9e2054` and contain every oracle line of ledger §A, except
the two unittest wall-clock lines (`Ran 10 tests in 0.097s`, `Ran 33 tests
in 1.069s`). `svcmenu_register` gains `0x2CAC0`, which runs in `game_init`,
so the 8000-frame dump was compared: `--check 8000` without that
registration and with it gives the same `shasum` list over 8000 `.idx` and
8000 `.pal` files (`k11_t8_frames_before.sha`, `k11_t8_frames_after.sha`).
The header grep (`/* 0xADDR`) counts 1 for each of `2E218 2E11C 2E248 2E5E4
32BDC 33560` and 2 for `2CAC0`, which matches the header at `svcmenu.c:1597`
and the inline comment at `:1601` (`/* 0x2CAC0 mov eax,1; ...`). `tools/port_progress.py` stays at `765 1203 64` / `729 730 100`:
none of the seven is a Ghidra function, so README is unchanged. The frame
dumps were deleted.

## §K11.9 Closure (executor, Task 9)

**Closure re-check.** `k11_closure.py` (`<scratchpad>/k11_closure_final.txt`)
prints `ALREADY P` for every function of the closure that has a header. Its
only two `U` lines are `0x1AE28` (`config_keys_apply`, header
`0x1AE20`; "U(nogh)") and `0x500BB` (the inline tick read; "U(gh 6)"), which
§0.3 showed are already ported. The walker prints no line at all for `0x38B18`
(`frontend_spawn_row`, also already ported), neither `U` nor `P`. So the
brief's `grep "^[0-9a-f]* U" | grep -v "^38b18\|^500bb\|^1ae28"` prints nothing
(exit 1), and §0.3's 50 functions are all ported.

**Totals.** 7 cycles: §K11.2 10 functions, §K11.3 9, §K11.4 5, §K11.5 9,
§K11.6 4, §K11.7 6 and §K11.8 7, which is **50 functions / 15 304 B**
(§0.3, re-checked in §K11.0). Three are Ghidra functions: `0x2F464`
(`text_number_cont`, §K11.4), `0x319B0` (`svc_buttons_clear`) and `0x31A78`
(`svc_buttons_draw`) (both §K11.5). `tools/port_progress.py` prints
`765 1203 64` / `729 730 100`: 762 at Task 1, 763 after §K11.4 and 765 after
§K11.5; §K11.6, §K11.7 and §K11.8 hold no Ghidra function. The README title
(64%) and its "729 of 730" line already read these values, so the README is
unchanged. `svcmenu_register` makes 18 `fn_register` calls (11 callbacks and 7
setters, §0.2/§0.7; `svcmenu.c:1612-1629`), inside `FN_TABLE_MAX`.

**The test seam.** `host_set_pump_hook` (§K11.1, a `PORT:` seam, NULL in the
game) plus the `sm_*` harness in `test_game.c`. The frame budget is used up:
**160 of 160** scripted frames (§K11.8).

**Named gaps** (ledger §E rows 28..33): six. From §0.6: the language reload
`0x47370` (a language change is stored in field `0x29` but not shown); the
joystick device choice (`+0x2D4`/`+0x2D6`; stored, no host effect); the
play-time fields `3, 0xA, 0xC, 0x12, 0x13` (page 1 shows 0); and the audit
counters `0x105ECD..0x105EFB` (the histograms show 0). From the code (`PORT:`
notes): the TEST CONTROLS RAW DATA row that reads the byte at linear
`0xFFE80003`, outside `mem[]` and drawn as 0 (`svcmenu.c:992`, §K11.5), and
`0x33458`'s `idiv` fault on a zero low word, drawn as 0 (`svcmenu.c:1192`,
§K11.7). Each stays as recorded in §0.6.
The deferred CMOS save `0x1B084` (§50-C) is unchanged. `grep -c
'TODO(verify)' port/src/game/svcmenu.c` prints 0.

**Task 8 review minors folded in.** (1) The `CHECK(memcmp(...), "the
descriptors are back")` in `sm_check_hist`, directly after the `memcpy` that
restores them, could not fail; it is now a plain comment. The suite's `CHECK`
total drops by one and every other assertion is unchanged. (2) The header
grep claim of §K11.8 (and the task-8 report) said "1 for each"; `0x2CAC0`
counts 2 (`svcmenu.c:1597` and the inline comment at `:1601`). (3) §K11.8's
five mutations are now listed by table row with their check counts. (4)
§K11.8 "Not tested" gains the signedness of `size < 1`, the tabbed no-pad arm
(`0x2E6CA`) and the median's past-size bucket (`0x2E582`). (5) PROGRESS.md's
cycle 7 paragraph says the clear on the last histogram needs `a` not 0
(with `a` = 0 the function returns 2). (6) §K11.8 (g) and `svcmenu.h` note that
`menu_run` also stores the result in `DS_00107448` (`0x304A5`, reset at
`0x304EB` before it is read), so the `u32` return is the whole state, and the
header now lists the 2 and -1 results next to the key word.
