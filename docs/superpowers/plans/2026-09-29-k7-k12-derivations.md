# K7 + K12 derivation record: the sample-start path and the omitted voice sites

Plan: `docs/superpowers/plans/2026-09-29-k7-k12-audio-voice.md`. Ledger rows:
§B.1 `0x1CB18`, §E rows 5/6, §F rows 10 (K7) and 19 (K12), and the
"about 180 'not wired'" paragraph under §E. Earlier record for K7:
`2026-09-29-k4-k6-k7-derivations.md` §K7.1-§K7.5 (Task 3d). §0 below is the
planner's starting record, measured on `all-gaps` at `e57d344`. Task 1 verifies
it, and Tasks 2-11 each append their own section (§1-§11).

**Tooling note.** The planner session had no Ghidra MCP bridge. Code was read
with capstone from `PRAGE.EXE` (object 0 file offset = VA + `0x52E54`). Data
was read at VA + `0x46E54`. Raw-file data displacements are pre-fixup: add
`0x80000` to a data displacement, so `0x475AA` is `0xC75AA`. The voice table's
dwords are resource handles, not addresses, so they carry no fixup. Its values
reproduce the records in K6 (§K6.1/§K6.2) exactly. **Task 1 re-reads every
address below through Ghidra (fixups applied). Where Ghidra and this record
disagree, Ghidra wins.**

---

## §0 Starting record (planner)

### §0.1 The dispatcher `0x2C3FC` and what each case costs in the port

The record is `DS_000BBDC8[id]`: 12 bytes, with `+0` the case, `+4` a handle and
`+8` a byte. The table has 244 entries (ids `0x00..0xF3`). Id `0x100` names
record 0. The case counts over the table are: case 0 ×5, 1 ×29, 2 ×154, 3 ×3
(`0x46`, `0x4D`, `0x5D`), 4 ×1 (id 3), 5 ×18 and 6 ×34. A handle is
`resource << 23 | offset`.

| case | raw effect | resource read (`0x1B544`) | rng | render | port today |
|---|---|---|---|---|---|
| 0 | none (AL = 1) | no | no | no | ported |
| 1 | `DS_00105D5C` = handle, `0x1CA14` song words `DS_001028D4/D9` (`DS_001028CC` only with a sequence handle, which the port keeps 0) | no | no | no | ported |
| 2 | `0x1CE70` playing test, then `0x1CC28` queue | **yes**: `0x1CC5D`, the bank's first read draws the loader and stalls ticks | no | only through the loader | queue stubbed (resolve only) |
| 3 | two handles queued (ids `0x46`/`0x4D`/`0x5D`) | yes, both | no | only through the loader | queue stubbed |
| 4 | unpause both, music `0x21`, stop all, queue `0x180122FD` | yes | no | only through the loader | queue stubbed |
| 5 | stops: `0x100` stops the music words and every slot (`0x1CA6C`, `0x1CD9C`); the others stop one song or one sample handle (`0x1CE04`) | no | no | no | ported |
| ≥6 | none (AL = 0) | no | no | no | ported |

No case draws from the rng or writes the frame, except case 2/3/4's first read
of an unread bank. **"Pure state"** in this record means a site whose ids are
all case 0/1/5/6. **"Sample"** means a site with a case-2/3/4 id: it resolves a
bank, and after K7 it writes a slot record and starts an AIL voice.

### §0.2 How the sites were found and classified

- **Raw universe.** A scan of object 0 finds 303 `call`/`jmp rel32` to `0x2C3FC`.
  Five are wired today: `0x2C9BE`, `0x4F721`, `0x4F766`, `0x4F770` and `0x1546E`
  (the `0x4D` pair in `actors.c`).
- **Port sites.** Every `port/src` comment that names a `0x2C3FC` call and does
  not make it was collected. That covers 188 lines matching `not wired`, the
  comments where the phrase is split across two lines (`not\n * wired`), the
  header-only mentions, `fight.c:4007` ("out of scope") and the title stand-in
  at `flow.c:4392`. The result is **218 wiring points (a C call to add) over 213
  raw call sites**. `0x4EB72` is one raw call reached by 8 C paths, `0x45F8C` by
  2, and the four `0x1F0xx` pairs are one port path. The ids come from the
  immediate before each call or from the word table it indexes. The per-character
  tables, read at VA + `0x46E54`, are: `0xC888A` = C0 C2 C4 C1 C5 C6 C3,
  `0xBDFFA` = 63 63 63 63 AC 63 63 90, `0xBDAA8` = 49 49 7B 49 49 49 49,
  `0xE9308` (26 words: 63, 76..7F, 49, 81, 82, 87, 88, 8E, 8F, 94, 99, 9D, 9E,
  A4, A8, 7E, AC, 7F, 45, 89, A3), `0xE933C` (63..6A, 46, 75, AC, 6F, 6B, B7, 63,
  70), `0xBE008` = 90 95 A5 9F 83 89 9A, `0xE9358` = 63 70 71 75 63 74 AC,
  `0xC75AA` = 91 96 A6 A0 84 8A 9B and `0xBDAD4` = 92 97 A7 A1 85 8B 4A.
- **Oracle reach (measured, not inferred).** An instrumented build
  (`build-cov`, `-fprofile-instr-generate`) ran `--check 8000`, the
  `PR_FRONTEND_DET` driver and the `PR_ATTRACT_DUMP` driver. The region count
  that follows each comment gives the sites those runs execute.
- **Oracle effect (measured).** A scratch copy wired every executed site
  through a logging `probe_voice(site, id)` that then called `sound_voice`. It
  logged each case-2/3/4 resource's loader flags at the moment of the call. The
  results against an unmodified build of the same tree:
  - `--check 8000` gave 24000 frame files, and the fe det dump gave 1384 frames
    ×2 runs, byte-identical in each run. The attract dump and the title dump were
    also byte-identical. The unit suite still printed `all checks passed`.
  - All 303 + 154 + 31 + 22 logged voices resolve a bank that is already read.
    The `fl` values were `22`/`21`: s16sound ×202, s16title ×106, s16statu
    (preloaded) ×6, s16cobsd ×10, s16konsd ×6, s16diasd ×3, s16rexsd ×2 and
    s16spisd ×1. So no wiring draws a new loader screen on an oracle path. The
    `fl=00` rows in the attract log were the driver's pre-`game_init` unit calls
    (`f=0`, no INDEX), not frames.
  - The same probe with the announcer stand-in removed and `0x41`/`0x43` wired
    at the title (§0.7.5) was also byte-identical in all four dumps. The only
    failure was main.c's `--check` announcer assertion, as expected.

### §0.3 Per-file classification (218 wiring points)

| file | points | pure state | sample | oracle path | of which sample | real play only |
|---|---:|---:|---:|---:|---:|---:|
| `game/actors.c` | 5 | 2 | 3 | 1 | 1 | 4 |
| `game/attract.c` | 8 | 4 | 4 | 8 | 4 | 0 |
| `game/camera.c` | 4 | 0 | 4 | 0 | 0 | 4 |
| `game/fight.c` | 40 | 11 | 29 | 4 | 4 | 36 |
| `game/fighter.c` | 72 | 2 | 70 | 15 | 15 | 57 |
| `game/flow.c` | 66 | 54 | 12 | 8 | 0 | 58 |
| `game/menu.c` | 2 | 2 | 0 | 0 | 0 | 2 |
| `game/nameentry.c` | 21 | 0 | 21 | 0 | 0 | 21 |
| **total** | **218** | **75** | **143** | **36** | **24** | **182** |

"Oracle path" means executed by at least one of `--check 8000`, the fe det
driver, the attract dump or the title dump. The site table (§0.4) names which
runs. `fight.c`'s count includes the three `fight.c:4007` calls marked "out of
scope (spec §7)". The raw makes them, so this record brings them into scope.

### §0.4 The site table

The columns are: row, the file:line of the comment at `e57d344`, the raw call
(or calls), the id(s), their case(s), the class, the oracle runs that reach it,
and the batch (plan task). Split-phrase and header-only rows are marked. They
are omitted calls that `rg 'not wired'` does not count.

| # | port comment | raw call | id(s) | case | class | oracle runs | batch |
|---:|---|---|---|---|---|---|---|
| 1 | `actors.c:2068` | `2B8D7` | opcode operand | 2 | sample | check, fe | C (data-driven; observed 0xD3 only) |
| 2 | `actors.c:2798` | `48CAE` | EF | 2 | sample | real play only | D1 |
| 3 | `actors.c:2954` | `49060` | F0 | 2 | sample | real play only | D1 |
| 4 | `actors.c:2967` | `490E1` | F1 | 5 | state | real play only | B2 |
| 5 | `actors.c:3049` | `3D789` | 4F | 5 | state | real play only | B2 (tail jmp) |
| 6 | `attract.c:105` | `10F43` | BD | 2 | sample | check, fe, attract, title | C |
| 7 | `attract.c:115` | `10F8B` | BE/BF | 2 | sample | check, fe, attract, title | C |
| 8 | `attract.c:133` | `10E06` | 100 | 5 | state | attract | A; **wired, §4** |
| 9 | `attract.c:152` | `10E6E` | 100 | 5 | state | attract | A; **wired, §4** |
| 10 | `attract.c:212` | `11024` | 100 | 5 | state | check, fe, attract, title | A; **wired, §4** |
| 11 | `attract.c:260` | `11160` | 40 | 2 | sample | check, fe, attract, title | T3 (loop byte 1); **wired, §3** |
| 12 | `attract.c:260` | `1116C` | 42 | 2 | sample | check, fe, attract, title | T3 (loop byte 1); **wired, §3** |
| 13 | `attract.c:282` | `111FF` | 54/56 | 1 | state | check, fe, attract, title | B2 |
| 14 | `camera.c:1379` | `17C88` | 64 | 2 | sample | real play only | D1 |
| 15 | `camera.c:1571` | `12C29` | BF | 2 | sample | real play only | D1 (split phrase) |
| 16 | `camera.c:1571` | `12C33` | D6 | 2 | sample | real play only | D1 (split phrase) |
| 17 | `camera.c:1571` | `12C3D` | CE | 2 | sample | real play only | D1 (split phrase) |
| 18 | `fight.c:329` | `43741` | 30 | 1 | state | real play only | B2 |
| 19 | `fight.c:361` | `444D1` | 30 | 1 | state | real play only | B2 |
| 20 | `fight.c:595` | `43FB1` | C888A[ch] | 2 | sample | real play only | D1 |
| 21 | `fight.c:699` | `43E96` | 6C | 2 | sample | real play only | D1 |
| 22 | `fight.c:868` | `44295` | C888A[ch] | 2 | sample | real play only | D1 |
| 23 | `fight.c:985` | `444B6` | 6C | 2 | sample | real play only | D1 |
| 24 | `fight.c:1089` | `430F2` | 31 | 1 | state | real play only | B2 |
| 25 | `fight.c:1122` | `4328A` | 2D | 5 | state | real play only | B2 |
| 26 | `fight.c:1122` | `43294` | 2F | 5 | state | real play only | B2 |
| 27 | `fight.c:1137` | `44557` | 100 | 5 | state | real play only | A; **wired, §4** |
| 28 | `fight.c:1164` | `44626` | 2E | 1 | state | real play only | B2 |
| 29 | `fight.c:1181` | `43696` | 100 | 5 | state | real play only | A; **wired, §4** |
| 30 | `fight.c:1204` | `43729` | 2E | 1 | state | real play only | B2 |
| 31 | `fight.c:1263` | `41483` | 32 | 1 | state | real play only | B2 |
| 32 | `fight.c:2492` | `4AD81` | C8 | 2 | sample | real play only | D1 |
| 33 | `fight.c:2647` | `4A6D2` | CD/CE/CF | 2 | sample | check, fe | C |
| 34 | `fight.c:2653` | `4A6B7/4A6C8` | C9/CA | 2 | sample | check, fe | C |
| 35 | `fight.c:2653` | `4A6D2` | DA/DB | 2 | sample | check, fe | C |
| 36 | `fight.c:2751` | `4B98F` | D4/D5 | 2 | sample | real play only | D1 |
| 37 | `fight.c:2825` | `4CB3E` | D1/D0 | 2 | sample | real play only | D1 |
| 38 | `fight.c:2880` | `4B497` | D1/D0 | 2 | sample | fe | C |
| 39 | `fight.c:3227` | `4DABE` | D4/D5 | 2 | sample | real play only | D1 |
| 40 | `fight.c:4007` | `4C85C` | D4/D5 | 2 | sample | real play only | D1 ("out of scope (spec 7)") |
| 41 | `fight.c:4007` | `4C866` | D6 | 2 | sample | real play only | D1 ("out of scope (spec 7)") |
| 42 | `fight.c:4007` | `4C875` | CE | 2 | sample | real play only | D1 ("out of scope (spec 7)") |
| 43 | `fight.c:4319` | `4BFF0` | C8 | 2 | sample | real play only | D1 |
| 44 | `fight.c:4751` | `49FF1` | DE | 6 | state | real play only | A; **wired, §4** |
| 45 | `fight.c:4988` | `4A4B5` | CB | 2 | sample | real play only | D1 |
| 46 | `fight.c:4994` | `4A4ED` | CB | 2 | sample | real play only | D1 |
| 47 | `fight.c:5001` | `4A52C` | DC | 2 | sample | real play only | D1 |
| 48 | `fight.c:5119` | `4DF0C` | CB | 2 | sample | real play only | D1 |
| 49 | `fight.c:5126` | `4DF4B` | DC | 2 | sample | real play only | D1 |
| 50 | `fight.c:5520` | `4EB72` | 5D | 2 | sample | real play only | D1 (one raw call, 8 paths) |
| 51 | `fight.c:5524` | `4EB72` | 5E | 2 | sample | real play only | D1 (one raw call, 8 paths) |
| 52 | `fight.c:5530` | `4EB72` | 5D | 2 | sample | real play only | D1 (one raw call, 8 paths) |
| 53 | `fight.c:5535` | `4EB72` | 5E | 2 | sample | real play only | D1 (one raw call, 8 paths) |
| 54 | `fight.c:5542` | `4EB72` | 5D | 2 | sample | real play only | D1 (one raw call, 8 paths) |
| 55 | `fight.c:5546` | `4EB72` | 5E | 2 | sample | real play only | D1 (one raw call, 8 paths) |
| 56 | `fight.c:5552` | `4EB72` | 5D | 2 | sample | real play only | D1 (one raw call, 8 paths) |
| 57 | `fight.c:5557` | `4EB72` | 5E | 2 | sample | real play only | D1 (one raw call, 8 paths) |
| 58 | `fighter.c:996` | `3B9B8` | BDFFA[ch] | 0/2 | sample | check, fe | C |
| 59 | `fighter.c:1854` | `362E5` | 6F | 2 | sample | real play only | D2 |
| 60 | `fighter.c:1923` | `367CF` | 6E | 2 | sample | check | C |
| 61 | `fighter.c:2406` | `35ED9` | 6D | 2 | sample | check, fe | C |
| 62 | `fighter.c:2617` | `37D73` | BDAD4[ch] | 2 | sample | real play only | D2 |
| 63 | `fighter.c:3139` | `3923D` | CD/CE/CF/DA/DB | 2 | sample | real play only | D2 |
| 64 | `fighter.c:3340` | `353C0` | EC | 1 | state | real play only | B2 |
| 65 | `fighter.c:3340` | `353CA` | E0 | 5 | state | real play only | B2 |
| 66 | `fighter.c:3492` | `35E38` | BDAA8[ch] | 2 | sample | check, fe | C |
| 67 | `fighter.c:4024` | `34FA4` | E9308[anim+7] | 0/2 | sample | check, fe | C |
| 68 | `fighter.c:4041` | `35032` | E9308[anim+7] | 0/2 | sample | check, fe | C |
| 69 | `fighter.c:4169` | `3D19D` | 91 | 2 | sample | check, fe | C |
| 70 | `fighter.c:4496` | `14807` | B1 | 2 | sample | fe | C |
| 71 | `fighter.c:4519` | `14829` | B0 | 2 | sample | fe | C |
| 72 | `fighter.c:4679` | `14E38` | B3 | 2 | sample | check | C |
| 73 | `fighter.c:4743` | `14B84` | B1 | 2 | sample | real play only | D2 |
| 74 | `fighter.c:4825` | `14BA5` | B0 | 2 | sample | real play only | D2 |
| 75 | `fighter.c:5383` | `39EDA` | 6C | 2 | sample | check, fe | C |
| 76 | `fighter.c:5831` | `3996E` | E933C[anim+8] | 0/2/3 | sample | check, fe | C (0x46 case 3 seen in fe) |
| 77 | `fighter.c:5875` | `22BB0` | B5 | 2 | sample | real play only | D2 |
| 78 | `fighter.c:6076` | `22F00` | B6 | 2 | sample | real play only | D2 |
| 79 | `fighter.c:6196` | `236CB` | B4 | 2 | sample | real play only | D2 |
| 80 | `fighter.c:6241` | `23166` | 7C | 2 | sample | real play only | D2 |
| 81 | `fighter.c:6256` | `231AE` | 7C | 2 | sample | real play only | D2 |
| 82 | `fighter.c:6432` | `24684` | B4 | 2 | sample | real play only | D2 |
| 83 | `fighter.c:6537` | `40E09` | 8C | 2 | sample | real play only | D2 |
| 84 | `fighter.c:6571` | `48957` | 7B | 2 | sample | real play only | D2 |
| 85 | `fighter.c:6634` | `4929C` | A2 | 2 | sample | real play only | D2 |
| 86 | `fighter.c:6664` | `15A22` | B1 | 2 | sample | real play only | D2 |
| 87 | `fighter.c:6707` | `15B80` | 9C | 2 | sample | real play only | D2 |
| 88 | `fighter.c:6769` | `4612C` | 80 | 2 | sample | real play only | D2 |
| 89 | `fighter.c:6802` | `3E057` | B9 | 2 | sample | real play only | D2 |
| 90 | `fighter.c:6868` | `40FB0` | 86 | 2 | sample | real play only | D2 |
| 91 | `fighter.c:6951` | `247F5` | A8 | 2 | sample | real play only | D2 |
| 92 | `fighter.c:7029` | `3ABAF` | BE008[ch] | 2 | sample | check, fe | C |
| 93 | `fighter.c:7257` | `3ADC1` | E9358[anim+9] | 0/2 | sample | check, fe | C |
| 94 | `fighter.c:7740` | `153BF` | AF | 2 | sample | fe | C |
| 95 | `fighter.c:8114` | `3E30C` | C75AA[ch] | 2 | sample | real play only | D2 |
| 96 | `fighter.c:8297` | `48B1E` | C75AA[ch] | 2 | sample | real play only | D2 |
| 97 | `fighter.c:8302` | `48B50` | 59 | 2 | sample | real play only | D2 |
| 98 | `fighter.c:8433` | `48C40` | 4B | 2 | sample | real play only | D2 |
| 99 | `fighter.c:8603` | `3A32D` | BE008[ch] | 2 | sample | real play only | D2 (split phrase) |
| 100 | `fighter.c:8717` | `44A53` | AB | 2 | sample | real play only | D2 |
| 101 | `fighter.c:8794` | `44C6B` | C75AA[ch] | 2 | sample | real play only | D2 |
| 102 | `fighter.c:9043` | `4514D` | 47 | 2 | sample | real play only | D2 |
| 103 | `fighter.c:9105` | `452C6` | 7B | 2 | sample | real play only | D3 |
| 104 | `fighter.c:9340` | `459B9` | 46 | 3 | sample | real play only | D3 |
| 105 | `fighter.c:9342` | `459AF` | 6C | 2 | sample | real play only | D3 (split phrase) |
| 106 | `fighter.c:9342` | `459B9` | 72 | 2 | sample | real play only | D3 (split phrase) |
| 107 | `fighter.c:9847` | `3F312` | 6C | 2 | sample | real play only | D3 |
| 108 | `fighter.c:9854` | `3F356` | 72 | 2 | sample | real play only | D3 |
| 109 | `fighter.c:9879` | `21F3A` | 6C | 2 | sample | real play only | D3 |
| 110 | `fighter.c:9886` | `21F7E` | 72 | 2 | sample | real play only | D3 |
| 111 | `fighter.c:10418` | `23328` | A9 | 2 | sample | real play only | D3 |
| 112 | `fighter.c:10520` | `3E0E3` | B9 | 2 | sample | real play only | D3 |
| 113 | `fighter.c:10635` | `3DE47` | B7 | 2 | sample | real play only | D3 |
| 114 | `fighter.c:10892` | `3EED3` | BB | 2 | sample | real play only | D3 |
| 115 | `fighter.c:11029` | `45BAE` | 50 | 2 | sample | real play only | D3 (header only) |
| 116 | `fighter.c:11029` | `45BF4` | D2 | 2 | sample | real play only | D3 (header only) |
| 117 | `fighter.c:11070` | `47A7B` | 46 | 3 | sample | real play only | D3 (header only) |
| 118 | `fighter.c:11216` | `408F2` | 6C | 2 | sample | real play only | D3 |
| 119 | `fighter.c:11223` | `40936` | 72 | 2 | sample | real play only | D3 |
| 120 | `fighter.c:11430` | `3EFD2` | 47 | 2 | sample | real play only | D3 |
| 121 | `fighter.c:11554` | `40B2B` | EE | 2 | sample | real play only | D3 |
| 122 | `fighter.c:11616` | `407D3` | 73 | 2 | sample | real play only | D3 |
| 123 | `fighter.c:11678` | `3705E` | BDAD4[ch] | 2 | sample | real play only | D3 (split phrase) |
| 124 | `fighter.c:12351` | `3FD23` | 48 | 2 | sample | real play only | D3 |
| 125 | `fighter.c:12417` | `40546` | B8 | 2 | sample | real play only | D3 |
| 126 | `fighter.c:12617` | `45F8C` | AB | 2 | sample | real play only | D3 (one raw call, 2 paths) |
| 127 | `fighter.c:12656` | `45F8C` | AC | 2 | sample | real play only | D3 (one raw call, 2 paths) |
| 128 | `fighter.c:12739` | `240B8` | 6B | 2 | sample | real play only | D3 (header only) |
| 129 | `fighter.c:12739` | `240CF` | BE008[ch] | 2 | sample | real play only | D3 (header only) |
| 130 | `flow.c:377` | `415DC` | 33 | 5 | state | real play only | B1 |
| 131 | `flow.c:701` | `41D2B` | 34+n | 2 | sample | real play only | D4 (n = the byte DS_00108112 before its increment) |
| 132 | `flow.c:787` | `4207F` | 3A | 2 | sample | real play only | D4 |
| 133 | `flow.c:793` | `420B7` | C7 | 2 | sample | real play only | D4 |
| 134 | `flow.c:813` | `4210D` | 33 | 5 | state | real play only | B1 |
| 135 | `flow.c:837` | `42229` | BC | 2 | sample | real play only | D4 |
| 136 | `flow.c:930` | `28D8B` | 2E | 1 | state | real play only | B1 |
| 137 | `flow.c:1364` | `26A2C` | 28 | 1 | state | real play only | B1 |
| 138 | `flow.c:1384` | `270F9` | 25 | 1 | state | real play only | B1 |
| 139 | `flow.c:1433` | `257AD` | 100 | 5 | state | fe | A; **wired, §4** |
| 140 | `flow.c:1447` | `25820` | 53 | 0 | state | fe | A; **wired, §4** |
| 141 | `flow.c:1534` | `28DCA` | 100 | 5 | state | real play only | A; **wired, §4** |
| 142 | `flow.c:1635` | `282BB` | 24 | 1 | state | real play only | B1 |
| 143 | `flow.c:1641` | `2817F/281F5` | 24 | 1 | state | real play only | B1 |
| 144 | `flow.c:1653` | `282BB` | 24 | 1 | state | real play only | B1 |
| 145 | `flow.c:1714` | `2730A` | D3 | 2 | sample | real play only | D4 |
| 146 | `flow.c:1721` | `27347` | 27 | 1 | state | real play only | B1 |
| 147 | `flow.c:1721` | `27351` | 22 | 5 | state | real play only | B1 |
| 148 | `flow.c:1900` | `27B01` | 27 | 1 | state | real play only | B1 |
| 149 | `flow.c:1900` | `27B0D` | 22 | 5 | state | real play only | B1 |
| 150 | `flow.c:1963` | `2759E` | 2A | 1 | state | real play only | B1 |
| 151 | `flow.c:2020` | `277B0` | 25/26 | 1 | state | real play only | B1 |
| 152 | `flow.c:2073` | `297C4` | 2A | 1 | state | real play only | B1 |
| 153 | `flow.c:2120` | `29960` | 25/26 | 1 | state | real play only | B1 |
| 154 | `flow.c:2284` | `25E39` | D7/D9 | 2 | sample | real play only | D4 (split phrase) |
| 155 | `flow.c:2425` | `294CE` | D7 | 2 | sample | real play only | D4 |
| 156 | `flow.c:2477` | `29983` | 27 | 1 | state | real play only | B1 (split phrase) |
| 157 | `flow.c:2477` | `29992` | 22 | 5 | state | real play only | B1 (split phrase) |
| 158 | `flow.c:2484` | `299C1` | 27 | 1 | state | real play only | B1 (split phrase) |
| 159 | `flow.c:2484` | `299CD` | 22 | 5 | state | real play only | B1 (split phrase) |
| 160 | `flow.c:2569` | `2968C` | 2B | 5 | state | real play only | B1 |
| 161 | `flow.c:2602` | `4F577` | 52 | 2 | sample | real play only | D4 (header only) |
| 162 | `flow.c:2952` | `27FF9` | D3 | 2 | sample | real play only | D4 |
| 163 | `flow.c:2964` | `2805F` | D3 | 2 | sample | real play only | D4 |
| 164 | `flow.c:3523` | `42E7F` | 2D | 5 | state | real play only | B2 |
| 165 | `flow.c:3602` | `4250D` | 2C | 1 | state | real play only | B2 |
| 166 | `flow.c:3941` | `25AF1` | 100 | 5 | state | real play only | A; **wired, §4** |
| 167 | `flow.c:3949` | `25B51` | 3D | 1 | state | real play only | B2 |
| 168 | `flow.c:4044` | `28C2A` | D8 | 2 | sample | real play only | D4 |
| 169 | `flow.c:4193` | `27821` | 2B | 5 | state | real play only | B2 |
| 170 | `flow.c:4392` | `121CE` | 41 | 5 | state | check, fe, attract, title | T3 (the stand-in replaces it); **wired, §3** |
| 171 | `flow.c:4392` | `121D8` | 43 | 5 | state | check, fe, attract, title | T3 (the stand-in replaces it); **wired, §3** |
| 172 | `flow.c:4576` | `1159F` | 100 | 5 | state | fe | A; **wired, §4** |
| 173 | `flow.c:4595` | `116C4` | 100 | 5 | state | real play only | A; **wired, §4** |
| 174 | `flow.c:4617` | `11844` | 100 | 5 | state | real play only | A; **wired, §4** |
| 175 | `flow.c:4878` | `11A94` | 100 | 5 | state | check, fe | A; **wired, §4** |
| 176 | `flow.c:5112` | `1EEF4` | 100 | 5 | state | real play only | A (split phrase); **wired, §4** |
| 177 | `flow.c:5112` | `1EEFE` | E1 | 1 | state | real play only | B2 (split phrase) |
| 178 | `flow.c:5158` | `1F1EC` | 100 | 5 | state | real play only | A (split phrase); **wired, §4** |
| 179 | `flow.c:5158` | `1F1F6` | E1 | 1 | state | real play only | B2 (split phrase) |
| 180 | `flow.c:5173` | `1F249` | E3 | 1 | state | real play only | B2 |
| 181 | `flow.c:5173` | `1F253` | E2 | 5 | state | real play only | B2 |
| 182 | `flow.c:5198` | `1F307` | 100 | 5 | state | real play only | A (split phrase); **wired, §4** |
| 183 | `flow.c:5198` | `1F311` | E1 | 1 | state | real play only | B2 (split phrase) |
| 184 | `flow.c:5214` | `1F36C` | E3 | 1 | state | real play only | B2 |
| 185 | `flow.c:5214` | `1F376` | E2 | 5 | state | real play only | B2 |
| 186 | `flow.c:5271` | `1EF8D/1F07F/1F109/1F01F` | E3 | 1 | state | real play only | B2 (split phrase; 4 raw calls, 1 port path) |
| 187 | `flow.c:5271` | `1EF97/1F089/1F113/1F029` | E2 | 5 | state | real play only | B2 (split phrase; 4 raw calls, 1 port path) |
| 188 | `flow.c:5281` | `1EFA9` | E1 | 1 | state | real play only | B2 |
| 189 | `flow.c:5290` | `1F099` | E1 | 1 | state | real play only | B2 |
| 190 | `flow.c:5345` | `20955` | 3B | 1 | state | real play only | B2 |
| 191 | `flow.c:5476` | `26DA9` | 29 | 0 | state | real play only | A; **wired, §4** |
| 192 | `flow.c:5476` | `26DB8` | 22 | 5 | state | real play only | B2 |
| 193 | `flow.c:5587` | `26B32` | 60 | 2 | sample | real play only | D4 |
| 194 | `flow.c:7355` | `11DF2` | 100 | 5 | state | check, fe | A; **wired, §4** |
| 195 | `flow.c:7380` | `11BF0` | 100 | 5 | state | check, fe | A (tail jmp); **wired, §4** |
| 196 | `menu.c:173` | `2FA6D` | 100 | 5 | state | real play only | A; **wired, §4** |
| 197 | `menu.c:295` | `2FFF6` | 100 | 5 | state | real play only | A; **wired, §4** |
| 198 | `nameentry.c:203` | `200B5` | B0 | 2 | sample | real play only | D4 (split phrase) |
| 199 | `nameentry.c:203` | `200BF` | 7B | 2 | sample | real play only | D4 (split phrase) |
| 200 | `nameentry.c:218` | `20128` | 71 | 2 | sample | real play only | D4 |
| 201 | `nameentry.c:238` | `201C6/201CD` | E7/E8 | 2 | sample | real play only | D4 |
| 202 | `nameentry.c:268` | `202C5` | 70 | 2 | sample | real play only | D4 |
| 203 | `nameentry.c:268` | `202CF` | 4D | 3 | sample | real play only | D4 |
| 204 | `nameentry.c:310` | `2043F` | E9 | 2 | sample | real play only | D4 |
| 205 | `nameentry.c:322` | `204B5` | E9 | 2 | sample | real play only | D4 |
| 206 | `nameentry.c:489` | `1F526` | E6 | 2 | sample | real play only | D4 |
| 207 | `nameentry.c:496` | `1F580` | 39 | 2 | sample | real play only | D4 |
| 208 | `nameentry.c:498` | `1F58E` | 34 | 2 | sample | real play only | D4 |
| 209 | `nameentry.c:503` | `1F5F8` | E6 | 2 | sample | real play only | D4 |
| 210 | `nameentry.c:507` | `1F638` | 35 | 2 | sample | real play only | D4 |
| 211 | `nameentry.c:509` | `1F63F` | 38 | 2 | sample | real play only | D4 |
| 212 | `nameentry.c:514` | `1F6A9` | E6 | 2 | sample | real play only | D4 |
| 213 | `nameentry.c:519` | `1F6DE` | 39 | 2 | sample | real play only | D4 |
| 214 | `nameentry.c:523` | `1F705` | 37 | 2 | sample | real play only | D4 |
| 215 | `nameentry.c:528` | `1F75A` | E6 | 2 | sample | real play only | D4 |
| 216 | `nameentry.c:533` | `1F78F` | 38 | 2 | sample | real play only | D4 |
| 217 | `nameentry.c:537` | `1F7B0` | 36 | 2 | sample | real play only | D4 |
| 218 | `nameentry.c:636` | `1FF31` | E9 | 2 | sample | real play only | D4 |

Batch sizes: T3 4, A 22, B1 21, B2 30, C 22, D1 31, D2 28, D3 27, D4 33 (sum
218). Rows 206-218 and several others cite the `mov eax,imm` address, not the
`call`. For example, `0x1F526` is the `mov` and `0x1F52B` the call. Task 1
normalises every row to its call address. In `nameentry_step`, two `mov`s can
share one call (`0x1F580`/`0x1F58E` -> `0x1F593`), so each such port path is
one wiring point.

Record-table notes that the batches need:
- A `0x63` entry (case 0) in `0xBDFFA`, `0xE9308`, `0xE933C` or `0xE9358` is a
  no-op voice: AL = 1 and no write. The port still makes the call, because the
  raw does.
- `0xE933C[8]` is `0x46`, case 3, which queues `0x28847C9` and `0x2886158`.
  The fe run reaches it.
- `0x2B8D7` (animation opcode `0x2E`) takes its id from the stream operand word
  (`0x2B8D2 and eax,0xffff`). The only value on an oracle path is `0xD3` (case
  2, s16statu, preloaded). Other values are data and can be any id.

### §0.5 Raw voice sites outside K12 (85)

These 85 raw calls to `0x2C3FC` have no port comment. The preliminary reading
is that they are in code the port does not have: non-Ghidra fighter callbacks
and move scripts, and the K11 service-menu callbacks `0x2C9CC`, `0x2C9E8` and
`0x30B34..0x30F47`:

`11A3D 11C38 14F46 14F9E 154DD 1550B 155EA 156CA 15780 15802 158CD 15956 1599B
212AF 2236D 223EF 224E2 2260E 228EF 2292B 22AA7 231FB 23243 23810 2385C 23BD8
23E9D 23F05 24001 2406D 24214 242DA 24317 243CF 243ED 2455E 295FD 2C9D4 2C9E0
2C9F0 2C9FC 30B34 30E20 30E44 30E59 30F47 342EF 3440D 3448B 34517 345A3 3462F
37687 37691 377B9 377C3 378DE 378E8 3D12D 3D395 3D3DB 3D643 3D730 3D9D7 3DAC5
3DB2A 3DB82 3F0C1 4012D 40137 402B1 402E0 41880 44AFF 45C8C 45D0A 475D9 4779D
478C7 47DF7 47E1B 48518 48548 4B0B4 4B0BE`

Several sit near a ported header: `11A3D` (state 5's `0x11A30`, "still
unwired" per `flow.c`), `11C38`, `295FD`, `41880` (the `0x41878` hook),
`3D12D`, `377B9`/`377C3`, `23243`, `2455E`, `3462F`, `3D3DB` and `44AFF`.
Task 1 places every one through Ghidra's containing function and the port's
headers. A site whose containing code is ported, but whose call is silently
dropped, joins batch D4, or a Task 11b if more than 10 are found.

### §0.6 Oracle risk (measured)

- Every oracle-path wiring point was wired in a scratch copy: the 36 in §0.3,
  plus the stand-in's removal. `--check 8000` (24000 files), the fe det dump
  (1384 frames ×2), the attract dump and the title dump are byte-identical to
  an unmodified build.
- Giving `0x1D0BC` its four buffers (`0x8C00 + 3×0x6000 = 0x1AC00` bytes, from
  the bump allocator at the raw's init point) moves every later heap address.
  The same four dumps stay byte-identical.
- No case reads the rng, and none renders except through the loader. The K7
  code adds only slot writes in `mem[]`, heap copies and AIL/mixer calls.
- **What the probe did not cover:** slot-record state in the fe driver after
  K7, which feeds no frame, and the rewritten `--check` probe (§0.7.6).

### §0.7 K7 design (replaces §K7.5 of the k4-k6-k7 record)

**§0.7.1 The slot buffers: port `0x1D0BC`. No stand-in.** `0x1D0BC`'s only
allocator is `0x1C308`, which is already replaced by `res.c`'s bump allocator
(`res_alloc`, "PORT: replaces FUN_0001C308"). So `0x1D0BC` itself ports
faithfully as `sound_buffers_alloc()`, called at the raw's point in the init
chain (`0x1C0B1`, after `0x1CF40`). `flow.c`'s `game_init` has a `PORT:` note
there. It works as follows:
- It runs once. A set `DS_000A2CB0` returns AL = 0 (`0x1D0BF`/`0x1D1A9`), and
  it sets the byte at `0x1D19D`.
- MIDI buffer: with `DS_001028C4` and `DS_001028C0` both set and `DS_001028D0`
  clear, it takes `0x5100` bytes into `DS_001028D0` and zeroes them (`0x61A70`).
  If that allocation fails, it zeroes `C4`, `C0` and `CC`. This arm is dead in
  the port, because C0 and C4 are 0, but it is ported as is.
- Sample buffers: with `DS_001028C8` set, and only if slot 0's `+0x10` is 0 at
  entry (`0x1D13B..0x1D147`), slot `i` gets `0x8C00` (i = 0) or `0x6000`
  (i = 1..3) at `DS_00102870 + i*0x18`. The loop stops at the first failure,
  or at a slot whose buffer is already set.
- If slot 0 got nothing (`ecx == 0`, `0x1D17F`), `DS_001028C8 = 0`. The raw also
  takes this arm when slot 0's buffer was set at entry. That is kept.
- The two `0x62734` messages are the runtime's printf. `PORT:` not printed.

The row `1D0BC host-owned record-§50-D` leaves `tools/port_classification.txt`.
This is a one-way scope change on a user-approved row (§0.9). `0x1C308` stays
host-owned. Its replacement is exported as `res_block_alloc(size)`. Every sample
fits its slot: the largest case-2 payload is `0x8BC6` (id `0xD3`), under
`0x8C00`, and the 13 payloads above `0x6000` go only to slot 0 (§K7.2).

**§0.7.2 The clock `0x500BB` (`mov eax,[0x101500]`).** The timer ISR advances
`DS_00101508` and `DS_00101500` together (`0x1BE0E..0x1BE16`). The port models
the ISR's `DS_00101508` increments in two places: the master loop's spin
(`flow.c`, `0x256C5`) and the read stall (`res.c`). `config.c`'s key-wait loop
already models both counters. So `DS_00101500` gets the same increments,
through one helper `game_isr_ticks(n)` (`PORT:`: the ISR's counter pair). The
readers of `0x500BB` are `0x1CC28`, the resource LRU stamps
`0x1B58E`/`0x1B5EE` (not modelled by the port), the lock `0x1E814` (a port
no-op), the config/menu key timers `0x2EB52..0x2EEF6`/`0x2FFDA` (they use
differences, inside their own loops) and `0x31F9F..0x320DB` (unported K11).
None is on an oracle path. The ISR's `DS_00104B22` gate stays unmodelled, as
todo-verify §1 records. **Corrected in §2.6 (raw wins):** the key timers are
not all "inside their own loops"; `0x2EB80` (`0x2EB8F`) is called from the
master loop's menu `0x2FFC4` (`0x303D9`) every frame.

**§0.7.3 `0x1CC28`'s choice, ported into `snd_sample_queue`** (raw
`0x1CC28..0x1CD99`, re-read here; it confirms §K7.2 and adds the forced arm's
details):
- No DIG driver (`DS_001028C8`) or paused samples (`DS_001028DB`): AL = 0, no
  read.
- Otherwise it reads `now = DSD(DS_00101500)` (`0x1CC51`), resolves the handle
  (`0x1CC5D`) and takes `size = [p]`.
- A slot is free when `+0x10 != 0`, `+0x04 == 0` and its `0x5DD03` status is not
  4.
- `size > 0x6000` (`jbe` unsigned, `0x1CC68`): slot 0 if it is free. Otherwise
  the candidate is 0 (`0x1CCBA`).
- `size <= 0x6000`: scan slots 3, 2, 1, 0 and take the first free one. A slot
  that is not free (a bufferless one included, `0x1CCD4 je 0x1CD1C`) becomes
  the candidate when `min > +0x14` (`0x1CD22 cmp ebp,eax; jbe`), where `min`
  starts at `now` and the candidate at 0.
- Queue: `+0x04 = h`, `+0x08 = loop`, `+0x14 = 0x500BB()`, AL = 1.
- Forced: `0x5DC8B` (the port's `AIL_stop_sample`), then `0x5DC0F`, then queue
  the candidate, AL = 1.
- `PORT:` a NULL resolve (no INDEX, in unit fixtures) queues nothing and
  returns 1.

**§0.7.4 `0x1CB18` as `sound_sample_start(slot)`** (§K7.1, confirmed):
- `+0x04 == 0` returns.
- Otherwise it resolves `+0x04` and copies `size` bytes from `p+4` to
  `+0x10`'s buffer in `mem[]`.
- AIL calls: `AIL_init_sample`, then `AIL_set_sample_address(mem + buf,
  size)`, volume `DS_000A2CB4`, rate `0x2B11` (`AIL_set_sample_rate`) and type
  (0,0). Loop count 0 only when `+0x08 == 1`. Then `AIL_start_sample`.
- Finally `+0x0C = +0x04` and `+0x04 = 0`.
- `PORT:` a bufferless slot is not started. The raw would copy to linear 0.

`0x1CF20` (`game_audio_service`) calls it for slots 0..3 (`0x1CF21..0x1CF2E`)
before the music. All case-2 payloads are raw 8-bit PCM except `0xCD`'s, which
is a RIFF file (header included). The raw plays the 44 header bytes as PCM, and
the port does the same.

**§0.7.5 The announcer stand-in is retired (raw wins).** The stand-in
(`game_sample_request`/`game_sample_play`, `s_pending_sample`,
`SND_ANNOUNCER_ID`) plays voice `0xCD` at the title's first entry. The raw
does not:
- `0x121C9`/`0x121D3` call `0x2C3FC(0x41)`/`(0x43)`, case-5 stops of
  `0x0383B6F4`/`0x03837440`.
- Those two handles are the attract's own looping s16title samples: `0x40` and
  `0x42`, loop byte 1, queued at phase 2 (`0x11160`/`0x1116C`).
- `0xCD` is a fight-effect voice. `0x4A6D2` (`fight_4a634`) and `0x3923D` pick
  it by `rng_next(3)` from `0xCD/0xCE/0xCF`.
- The stand-in also made the port's first read of s16sound happen at the title
  (f = 690). `res.c`'s own read-rate derivation puts that read at the state-6
  entry. Measured with the stand-in removed: s16sound's first read moves to
  f = 1956 (state 6), and every dump is byte-identical.

Todo-verify §23's premise ("the announcer the port plays is voice id `0xCD`")
is therefore a port choice, not the raw's. §23's loop-count fix stays correct.
The music stand-in (`s_music_request`, `title_music_bank`) is **not** in this
cluster and stays. The raw's music request is `0x54`/`0x56` at `0x111FF`
(batch B2, state only), whose `DS_001028CC` arm needs the sequence handle
`DS_001028C0`, which the port keeps 0 (todo-verify §22).

**§0.7.6 The AIL end status (port fix, needed by §0.7.3).** `ail.c`'s
`AIL_sample_status` returns the stored state. A one-shot that the mixer has
finished stays 4 ("playing") forever. The raw's DIG service sets it done at the
buffer end (`0x6F28F`). Fix: `AIL_sample_status` reports 2 once the mixer has no
active voice owned by the handle (`mixer_sample_active(owner)`). With no audio
device (`--check`, the suite), the mixer is not rendered, so a started sample
stays 4. That is the named gap, and it changes only which slot `0x1CC28` evicts.

The `--check` announcer probe (`main.c` `probe_announcer_audio`) and the
Task-12 announcer checks in `test_game.c` assert the stand-in. Task 3 rewrites
them to the raw's facts:
- On the last attract frame before the title, a slot's `+0x0C` holds
  `0x0383B6F4` with status 4.
- At title entry + 2 frames, no slot does, because `0x121C9` stopped it.
- The unit path is `0x40` queued, then started and looping, then stopped by
  `0x41`.

### §0.8 Corrections to earlier records (raw wins)

| earlier claim | correction | evidence |
|---|---|---|
| §K7.3.1 / §K7.5(a): model `+0x10` with a non-zero `PORT:` marker and point the handle at the resolved bytes instead of copying | Port `0x1D0BC`. Its only non-port callee `0x1C308` already has the port replacement `res_alloc`. Copy as the raw does. | `0x1D0BC..0x1D1AE`; `res.c:94` |
| §K7.3.3: the stand-in is "the deferred attract trigger `0x11000`" | The raw title plays no sample on entry. It stops `0x40`/`0x42` (`0x121C9`/`0x121D3`). `0xCD` is a fight-effect voice. | `0x121CE`/`0x121D8`, `0x11160`/`0x1116C`, `0x4A675`, `0x3920E` |
| todo-verify §23: "the announcer the port plays is voice id `0xCD`" | True of the port. It is not the raw's title behaviour (above). The loop-count fix stands. | as above |
| ledger §E note: "about 180 'not wired' sites; 124 of form `PORT: 0xADDR 0x2C3FC`" | 218 wiring points over 213 raw calls in ported code. 188 lines match `not wired`, and the rest are split-phrase, header-only, `fight.c:4007` and the title stand-in. 85 more raw calls are in code outside the port (§0.5). | §0.2 |
| `ail.c`: `AIL_sample_status` is the stored state | The DIG service ends a one-shot (`0x6F28F`). The port never did. | §0.7.6 |

### §0.9 Decisions for the user (the controller relays them)

1. **Reclassify `0x1D0BC` from host-owned to ported.** Removing the
   `record-§50-D` row is a one-way change to an approved classification. It
   adds `0x1D0BC` and `0x1CB18` to the counter.
2. **Retire the title announcer stand-in.** The audible behaviour changes. The
   title no longer plays `0xCD`. The attract plays its two s16title loops and
   the `0xBD`/`0xBE`/`0xBF` one-shots (batch C), and the title stops the loops.
   The `--check` announcer probe and four `test_game.c` assertions are
   rewritten to the raw's facts.
3. **`fight.c:4007`'s three calls, marked "out of scope (spec §7)", are wired**
   (batch D1). The raw makes them.

---

## §1 Task 1: verification, placement and the drive table

Measured on branch `k7-k12` at `cd58f02` (main after the K11 cycle-5 merge),
not on `all-gaps` at `e57d344`. The source drift since `e57d344` is 16 lines
in `attract.c`, `flow.c` and `menu.c` (K11's service-menu wiring); every §0.4
row was re-located through `git diff -U0 e57d344 HEAD` and its line re-read.
The line numbers below are the current tree's.

**Tooling.** The Ghidra MCP bridge was unavailable in this session too. Code
and data were read from a fixup-applied image of `PRAGE.EXE`: a Python mirror
of `mem_load_le` + `mem_load_le_fixups` (`$K/le.py`, `$K/img.bin`) with
capstone (`$K/dx.py`). That is the Ghidra address space with fixups applied.
Function extents come from Ghidra's own export, `port/decomp/prage.functions.csv`
(`$K/fn.py`). `$K` is `.superpowers/sdd/2026-09-29-k7-k12/scratch/`.

### §1.0 Baseline

- `make build && make verify` exits 0 (`$K/verify-base.txt`). Every output
  line of ledger §A's block appears verbatim in the log. §A's three shorthand
  lines (the two `unittest` summaries and `all checks passed (symbols.h
  byte-identical)`) appear as their literal output: `Ran 10 tests`, `Ran 33
  tests`, `OK` and `all checks passed`.
- The brief's grep (`$K/oracle-lines-base.txt`) extracts 45 lines. 26 are
  §A's lines. The other 19 are detail lines §A never lists: the title,
  front-end and attract2 `window distinct`/`splice bytes`/`transition
  rows`/`exhibited`/`all-black` lines, the attract `FIRST DIVERGENCE`/`best
  byte splice`/`attract window` lines, and the Makefile's N-less `== demo-fight
  oracle` echo. None of them is a claim §A records, so no claim moved. The
  45-line extract is byte-identical to the K11 cycle's last gate run on the
  same main. **This `oracle-lines-base.txt` is the file later tasks diff
  against.**
- `CHECK`/`CHECK_EQ_INT` sites: **13254** (`$K/checks-base.txt`). Ledger §A's
  12532 predates the K1-K11 merges.
- `python3 tools/port_progress.py`: `765 1203 64` / `729 730 100 (portable:
  excludes 82 host-owned/deferred and runtime >= 5D000)` (`$K/progress-base.txt`).
- `rg -c 'not wired' port/src`: 188 lines over 11 files
  (`$K/notwired-base.txt`), the same total as §0.2.
- `sh $K/dumps.sh base && sh $K/dumpcmp.sh base base` gives `CMP=0`.
  `check/frames` has **24000** files and `fe/run1` and `fe/run2` have **1384**
  each: the `e57d344` counts hold. The attract and title drivers write one
  sub-directory each (`attract/attract`, `title/title`). The dump is 3.5 GB;
  `$K/base.sha256` (32367 files) fingerprints it.

### §1.1 §0 verified against the raw

Every check below matched §0. There is no raw-wins correction to §0's facts.
The items marked *addition* are raw facts §0 leaves out that the K7 tasks need.

| § | checked | raw evidence | result |
|---|---|---|---|
| 0.7.1 | `0x1D0BC` | `0x1D0BF cmp [0xA2CB0],0; jne 0x1D1A9` (AL = 0); MIDI arm `0x1D0CC..0x1D12F` (`0x5100` from `0x1C308(0x41, 0x5100)` into `DS_001028D0`, zeroed by `0x61A70`; on failure `C4`, `C0`, `CC` = 0 and the `0x62734` message); sample arm `0x1D132..0x1D17D` (`DS_001028C8` set, slot 0 `+0x10` = `DS_00102870` zero at entry, `0x8C00` for slot 0 and `0x6000` after, stride `0x18`, stop at a failure or a set buffer); `0x1D17F test ecx,ecx` then `DS_001028C8 = 0`; `0x1D19D mov [0xA2CB0],dl` | matches |
| 0.7.1 | init point | `0x1BEC4` (Ghidra `FUN_0001bec4`) calls `0x1CF40` at `0x1BFDB` and `0x1D0BC` at `0x1C0B1`; they are the only callers of each | matches |
| 0.7.3 | `0x1CC28` | `0x1CC37`/`0x1CC44` gates (AL = 0 at `0x1CD8F`); `0x1CC51 call 0x500BB`; `0x1CC5D call 0x1B544`; `0x1CC68 cmp ecx,0x6000` / `0x1CC6E jbe`; slot 0 free test `0x1CC70..0x1CC94`; candidate 0 at `0x1CCB8..0x1CCBA`; scan `edi = 3..0`, `esi = 0x48..0` at `0x1CCC3..0x1CD32`, free test `+0x10 != 0`, `+0x04 == 0`, `0x5DD03 != 4`, `0x1CCD4 je 0x1CD1C`; `0x1CD22 cmp ebp,eax; jbe` (strict, unsigned); forced arm `0x1CD34..0x1CD84` (`0x5DC8B`, `0x5DC0F`, queue) | matches. Citation: `0x1CC68` is the `cmp`, the `jbe` is `0x1CC6E` |
| 0.7.4 | `0x1CB18` | `0x1CB39 je` on `+0x04 = 0`; `0x1CB41 call 0x1B544`; copy `size` bytes from `p+4` to `+0x10` (`0x1CB49..0x1CB61`); `0x5DC0F`, `0x5DC2A(h, buf, size)`, `0x5DCC5(h, [0xA2CB4])`, `0x5DCA6(h, 0x2B11)`, `0x5DC4D(h, 0, 0)`, `0x5DCE4(h, 0)` only when `+0x08 == 1` (`0x1CBD3`), `0x5DC70`; `+0x0C = +0x04`, `+0x04 = 0` (`0x1CC03..0x1CC16`). The `0x5DCxx` names are `ail.c`'s headers | matches |
| 0.7.4 | `0x1CF20` | `0x1CF21..0x1CF2E` calls `0x1CB18` for slots 0..3, then `0x1C930` when `DS_001028CC != 0` (`0x1CF30`); callers `0x256B6`, `0x2EB05` | matches |
| 0.7.2 | `0x500BB` | `mov eax,[0x101500]; ret`. Its 18 callers are `0x1B58E`, `0x1B5EE`, `0x1CC51`, `0x1CCA7`, `0x1CD06`, `0x1CD79`, `0x1E3FB`, `0x1E4BA`, `0x1E814`, `0x2EB52`, `0x2EB8F`, `0x2EE06`, `0x2EEF6`, `0x2FFDA`, `0x31F9F`, `0x31FDE`, `0x3205D`, `0x320DB` | matches; *addition*: `0x1E3FB` (`0x1E30C`) and `0x1E4BA` (`0x1E458`) also read it; both functions are host-owned heap code (`port_classification.txt` rows `1E30C`/`1E458`), so §0.7.2's "none on an oracle path" stands |
| 0.7.2 | ISR `0x1BDF4..0x1BE2F` | `0x1BDF8` gate `DS_00104B22 == 1` skips; `0x1BE0E..0x1BE16` increment `DS_00101508` and `DS_00101500` together; then `0x1BBAC`, `inc word [0xEF6DE]`, `0x2D62C` | matches |
| 0.1 | dispatcher `0x2C3FC` | `0x2C403 test eax,eax; je 0x2C8EA` (id 0: AL = 0, no table read); `0x2C409` id `0x100` becomes 0; case byte `[id*12 + 0xBBDC8]`, `ja 0x2C8E8` above 6; jump table `0x2C3E0` = `2C8DD 2C437 2C473 2C4C6 2C89B 2C57F 2C8E8` for cases 0..6 | matches; *addition*: id 0 returns AL = 0 before the table |
| 0.1 | case 2 | `0x2C483 call 0x1CE70`; when playing, AL = 0 (`0x2C48A jne 0x2C8E8`); else `0x1CC28(handle, loop byte)` at `0x2C4B6` | matches; *addition*: AL = 0 on the playing arm |
| 0.1 | case 3 | `0x46`: `0x1CE70(0x2886158)` gate, queue `0x28847C9` then `0x2886158` (loop 0); `0x4D`: gate `0x1201D606`, queue `0x1201D606`, `0x2001513C`; `0x5D`: gate `0x281A726`, queue `0x281A726`, `0x2819183` | matches |
| 0.1 | case 4 | `0x2C89B` `0x1D238`, `0x1D244`, `DS_00105D5C = 0x21`, `0x1CA14(0x2803E64, 0)`; `0x1CE70(0x180122FD)` and, only when not playing, `0x1CD9C` then `0x1CC28(0x180122FD, 1)` (`0x2C8C0..0x2C8D8`) | matches; *addition*: the playing test and loop byte 1 |
| 0.1 | case 5 | `0x100` (and 0): `0x1CA6C`, `0x1CD9C` (`0x2C69C`); `0x41`: `0x1CE04(0x383B6F4)` (`0x2C7A5`); `0x43`: `0x1CE04(0x3837440)` (`0x2C7BA`). The handles are immediates in the code; the table records of `0x41`/`0x43` hold handle 0 | matches |
| 0.1 | table | 244 records at `0xBBDC8`; raw file bytes equal the image (no fixups). Case counts 0 ×5, 1 ×29, 2 ×154, 3 ×3 (`46 4D 5D`), 4 ×1 (`03`), 5 ×18, 6 ×34. Spot checks: `40` case 2 `0383B6F4` loop 1; `41` case 5; `42` case 2 `03837440` loop 1; `43` case 5; `3A` case 2 `03022554`; `CD` case 2 `02824B0F`; `00` (so `100`) case 5; `53` case 0; `D3` case 2 `01000008`; `54`/`56` case 1 `03836102` | matches |
| 0.2 | word tables | `C888A` = C0 C2 C4 C1 C5 C6 C3; `BDFFA` = 63 63 63 63 AC 63 63 90; `BDAA8` = 49 49 7B 49 49 49 49; `E9308` (26) = 63 76 77 7D 78 79 7C 49 81 82 87 88 8E 8F 94 99 9D 9E A4 A8 7E AC 7F 45 89 A3; `E933C` (16) = 63 64 65 66 67 68 69 6A 46 75 AC 6F 6B B7 63 70; `BE008` = 90 95 A5 9F 83 89 9A; `E9358` = 63 70 71 75 63 74 AC; `C75AA` = 91 96 A6 A0 84 8A 9B; `BDAD4` = 92 97 A7 A1 85 8B 4A. Image equals raw file bytes | matches. Note: `BDFFA[7]` (`90`) is `BE008[0]`, and `E933C[14..15]` (`63 70`) are `E9358[0..1]`: the tables overlap |
| 0.7.5 | title and attract calls | `0x121C9 mov eax,0x41; 0x121CE call`; `0x121D3 mov eax,0x43; 0x121D8 call` (`FUN_000121a0`); `0x11156 mov eax,0x40; 0x11160 call`; `0x11165 mov eax,0x42; 0x1116C call` (`FUN_00011000`) | matches |
| 0.2 | the universe | a scan of the image finds 303 `call`/`jmp rel32` to `0x2C3FC` | matches |
| 0.4 | every row's ids | the immediate or table read before each normalised call (`$K/ids.txt`), including `0x4A6D2` (`CD`/`CE`/`CF` by `jmp` from `0x4A675..0x4A688`, `DA`/`DB` from `0x4A6BC`/`0x4A6CD`), `0x459B9` (`46` by `jmp` from `0x459A3`, `72` from `0x459B4`), `0x45F8C` (`AB` from `0x45E59`, `AC` from `0x45F81`), `0x48B50` (`59` from `0x48B37`), `0x44A53` (`AB` from `0x44A4E`), `0x41D2B` (`lea eax,[edx+0x34]`, `edx` = the byte before its increment) and `0x2B8D7` (`and eax,0xffff` of the operand) | matches all 218 |
| 0.3 | counts | per file 5/8/4/40/72/66/2/21; 75 state, 143 sample; batches T3 4, A 22, B1 21, B2 30, C 22, D1 31, D2 28, D3 27, D4 33 | matches |

### §1.2 The call addresses and the placement of the 85 outside sites

**Normalised calls.** Each row's call address is in §1.3's second column.
The 218 rows name 213 distinct `call`/`jmp` addresses, and 213 + 85 + 5 wired
= 303. Rows that share one raw call:
- `0x4EB72`: rows 50-57.
- `0x1F593`: rows 207/208 (`mov` `0x1F580` jumps to it).
- `0x1F644`: rows 210/211.
- `0x201D2`: row 201's two `mov`s.
- `0x282BB`: rows 142/144.
- `0x459B9`: rows 104/106.
- `0x45F8C`: rows 126/127.
- `0x4A6D2`: rows 33/35.

Two rows are tail `jmp`s: `0x3D789` (row 5) and `0x11BF0` (row 195).

**Placement.** For each site, the containing code is Ghidra's function
(`prage.functions.csv`) when there is one. Otherwise it is the nearest entry
candidate that reaches the site by intra-procedural flow: a data-object dword,
a call target, a code immediate, or a block start after `ret`/`jmp`. Ported
means a `/* 0xADDR` header or an `fn_register` in `port/src` whose flow
reaches the site (`$K/place2.py`, `$K/place4.py`, `$K/place5.py`). Of the 85
sites, only `0x44AFF` and the nine service-menu sites are reached from a
ported header. Only `0x3D3AC` has direct callers (`0x3D664`, `0x3D6D6`), and
both are in unported code.

Verdict counts: **75 outside**, **1 silent**, and **9 wired since `e57d344`**.
The last group is a drift correction, not a raw-wins one: K11 ported the
service-menu callbacks and wired their calls.

- **Silent (1): `0x44AFF`** (`AB`). `0x44A64` is ported
  (`anim_code_44A64` -> `fighter_44a64`), and its body is the second copy of
  `0x449B8`'s. The port runs both through one C body, `fighter_c4_spawn`, whose
  comment names only `0x449B8`'s call `0x44A53` (row 100). Wiring row 100's
  call in the shared body also makes `0x44AFF`'s. This site becomes
  **row 219**, batch D4 (Task 11): add `0x44AFF` to the row-100 comment and
  drive it through `fighter_44a64`. It adds no second call.
- **Wired (9):**
  - `0x2C9D4`/`0x2C9E0` (`svc_play_tune`, `svcmenu.c:369/370`)
  - `0x2C9F0`/`0x2C9FC` (`svc_play_sample`, `:376/377`)
  - `0x30B34`, `0x30E20`, `0x30E44`, `0x30E59` (ADJUST VOLUME `0x30864`, `:452/518/521/525`)
  - `0x30F47` (SAMPLE TEST `0x30EB4`, `:344`, repeated by MUSIC TEST at `:363`)
- **Outside (75):** listed below with their containing entries. Each is wired
  when that code is ported.


| site | containing code | verdict |
|---|---|---|
| `11A3D` | `0x11A30` (no reference found; `flow.c` names it "still unwired", a caller of `0x1EA08`) | outside |
| `11C38` | `0x11C00` (a dword in the data object) | outside |
| `14F46` | `0x14F00` (a dword in the data object) | outside |
| `14F9E` | `0x14F50` (a dword in the data object) | outside |
| `154DD` | `0x1549C` (a dword in the data object) | outside |
| `1550B` | `0x15500` (a dword in the data object) | outside |
| `155EA` | `0x155AB` (code immediate at `0x15570`) | outside |
| `156CA` | `0x1567C` (a dword in the data object) | outside |
| `15780` | `0x15700` (a dword in the data object) | outside |
| `15802` | `0x15800` (a dword in the data object) | outside |
| `158CD` | `0x1587E` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `15956` | `0x15908` (a dword in the data object) | outside |
| `1599B` | `0x15960` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `212AF` | `0x21200` (a dword in the data object) | outside |
| `2236D` | `0x22338` (a dword in the data object) | outside |
| `223EF` | `0x22338` (a dword in the data object) | outside |
| `224E2` | `0x22494` (a dword in the data object) | outside |
| `2260E` | `0x22588` (code immediate at `0x22979`) | outside |
| `228EF` | `0x228BB` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `2292B` | `0x228F9` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `22AA7` | `0x22A40` (a dword in the data object) | outside |
| `231FB` | `0x231C0` (a dword in the data object) | outside |
| `23243` | `0x23208` (a dword in the data object) | outside |
| `23810` | `0x23800` (a dword in the data object) | outside |
| `2385C` | `0x2381C` (a dword in the data object) | outside |
| `23BD8` | `0x23B68` (code immediate at `0x23C75`) | outside |
| `23E9D` | `0x23E6D` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `23F05` | `0x23EC0` (a dword in the data object) | outside |
| `24001` | `0x23F10` (a dword in the data object) | outside |
| `2406D` | `0x2400C` (a dword in the data object) | outside |
| `24214` | `0x241F4` (a dword in the data object) | outside |
| `242DA` | `0x242C1` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `24317` | `0x242C1` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `243CF` | `0x24338` (a dword in the data object) | outside |
| `243ED` | `0x24338` (a dword in the data object) | outside |
| `2455E` | `0x24508` (a dword in the data object) | outside |
| `295FD` | `0x295C0` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `342EF` | `0x3427C` (a dword in the data object) | outside |
| `3440D` | `0x3438C` (a dword in the data object) | outside |
| `3448B` | `0x34418` (a dword in the data object) | outside |
| `34517` | `0x344A4` (a dword in the data object) | outside |
| `345A3` | `0x34530` (a dword in the data object) | outside |
| `3462F` | `0x34608` (a dword in the data object) | outside |
| `37687` | `0x37640` (a dword in the data object) | outside |
| `37691` | `0x37640` (a dword in the data object) | outside |
| `377B9` | `0x37774` (a dword in the data object; `fighter.c` names it the unported reaction-0x33 callback) | outside |
| `377C3` | `0x37774` (a dword in the data object) | outside |
| `378DE` | `0x37898` (a dword in the data object; the reaction-0x34 callback) | outside |
| `378E8` | `0x37898` (a dword in the data object) | outside |
| `3D12D` | `0x3D10C` (a dword in the data object) | outside |
| `3D395` | `0x3D328` (a dword in the data object) | outside |
| `3D3DB` | `0x3D3AC` (called at `0x3D664`, `0x3D6D6`) | outside |
| `3D643` | `0x3D57D` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `3D730` | `0x3D6E0` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `3D9D7` | `0x3D94D` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `3DAC5` | `0x3DA50` (a dword in the data object) | outside |
| `3DB2A` | `0x3DADC` (a dword in the data object) | outside |
| `3DB82` | `0x3DB34` (a dword in the data object) | outside |
| `3F0C1` | `0x3F0A8` (a dword in the data object) | outside |
| `4012D` | `0x400FF` (a dword in the data object) | outside |
| `40137` | `0x400FF` (a dword in the data object) | outside |
| `402B1` | `0x402A0` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `402E0` | `0x402A0` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `41880` | `0x41878` (no Ghidra function; `flow.c` names it as a `0x259CC` hook storer) | outside |
| `45C8C` | `0x45C54` (a dword in the data object) | outside |
| `45D0A` | `0x45C98` (a dword in the data object) | outside |
| `475D9` | `0x475C0` (after `ret` and zero padding; no call, table or code-immediate reference) | outside |
| `4779D` | `0x47798` (code immediate at `0x478AF`) | outside |
| `478C7` | `0x47874` (a dword in the data object) | outside |
| `47DF7` | `0x47D24` (code immediate at `0x4803A`) | outside |
| `47E1B` | `0x47E04` (a dword in the data object) | outside |
| `48518` | `0x484F0` (code immediate at `0x48440`) | outside |
| `48548` | `0x4852A` (block start after a `ret`/`jmp`; no call or table reference) | outside |
| `4B0B4` | `0x4B03C` (a dword in the data object) | outside |
| `4B0BE` | `0x4B03C` (a dword in the data object) | outside |

### §1.3 The drive table

The table has one row per §0.4 row, plus row 219 (§1.2's silent site). Its
columns:

- **call:** the normalised `call`/`jmp`. The row's own `mov` is in brackets
  when the comment cites it.
- **C function:** the port function that holds the comment, in the current tree.
- **public entry:** the exported function a test calls.
  - A static function's nearest public caller.
  - A function the port registers is also reachable as
    `fn_resolve(0xADDR)` after `actors_init()`.
- **seed:** the port's own branch conditions to the call, with their raw
  addresses.
- **ids:** the ids the path posts, in raw call order. This includes the ids
  of other §0.4 rows on the same path. A table id names the byte that indexes
  it.
- **existing test:** a test that calls the enclosing function (or the named
  entry). "Path unconfirmed" means the test calls it, but this task did not
  confirm that it seeds the row's arm.

Rows on one path share a driver:
- `game_coin_divert` covers rows 139/140.
- `fight_hook_4367c` covers 29/30/18, or 27/28/19 with `DS_00104B1D = 3`.
- `fighter_40954` case 2 covers 118/119/121.
- `nameentry_step` runs `nameentry_cells_step` first (rows 198-205) and
  `game_mode_1e_step` runs `nameentry_step` (rows 180/184/186-189).

The path to `fighter_39834` (row 76) posts its `E933C` id before the caller's
own id in rows 72, 77, 92, 107-110 and 117. A driver for those rows sees two
ids.

The fight-context fixtures are:
- `tf_hit_fixture(ch)`: two slots, slot 0 live with `ch`, the mode, command
  and reaction globals.
- `tf_demo_fixture()`: the arena frame's two slots, an empty effect list, every
  gate closed.
- `tf_anim_alloc_record()`/`tf_anim_spawn_stream()`: an actor record and a
  stream at `ANIM_SCRATCH`.

A row that reads a fighter slot (`DS_001077B0 + side * 0x94`) or its record
starts from `tf_hit_fixture`. A row that walks the effect list `DS_0010884C`
starts from `tf_demo_fixture` and links its entry in. Row 1 starts from
`tf_anim_alloc_record`.

| row | call | C function | public entry | seed (branch, raw address) | ids, raw order | existing test that reaches it |
|---:|---|---|---|---|---|---|
| 1 | `2B8D7` | `actors.c` `spawn_anim_opcode` (static), case `0x2E` | `actors_anim_begin(rec, stream, bits)` (its pre-walk runs every command whose high byte has bit 7) | stream words `{0x9F2E, id}`: the `0x1F` prefix (`0x2B2A0` op `& 0x1F`), op byte `0x2E`, mode 0, so the operand is the next word (`anim_operand`, `0x2B932`); `0x2B8D2 and eax,0xffff`. The one-byte form sign-extends to `0xFFxx` and is not a valid id | the operand (oracle runs: `D3`) | none drives op `0x2E` |
| 2 | `48CAE` | `actors.c` `actor_type_2d_list_init` | the function itself (public), or `fighter_48d94(slot, rec, side)` with slot `+0x57 = 1` (`0x48DBF`) | none: unconditional tail | `EF` | `test_fight.c` `check_finisher_48aac` (calls it) |
| 3 | `49060` | `actors.c` `actor_type_2d_update` (static; `fn_register(0x48F98)`) | `fn_resolve(0x48F98)()` after `actors_init()` | one node on the in-use list `DS_00108368` with `+0x0C = 0` (`0x48FB5`), `abs(node rec +0x18 - them +0x18) <= 0x1000` (`0x48FED`), `DS_00108396` bit 7 clear (`0x49037`); slot `DS_00104AD4` record valid for `0x37B54` | `F0` | none |
| 4 | `490E1` | same | same | node `+0x0C = 1` (`0x48FB7`), distance `> 0x1000` (`0x4909C`), `DS_00108397 = 1` so `n = 0` is not `> 0` (`0x490D8`) | `F1` | none |
| 5 | `3D789` (tail `jmp`) | `actors.c` `actor_type_3D784` (static; `fn_register(0x3D784)`) | `fn_resolve(0x3D784)(rec)` after `actors_init()` | none | `4F` | none |
| 6 | `10F43` | `attract.c` `attract_voice_tick` | the function (public) | `DSW(DS_000F0A60) = 1` (signed `<= 0` after the decrement, `0x10F3C jg`) | `BD` | `test_game.c` `test_attract` (the "both expire" cases) |
| 7 | `10F8B` | same | same | `DSW(DS_000F0A62) = 1` (`0x10F6F jg`); `rng_next(2)` != 0 gives `BE`, 0 gives `BF` (`0x10F7D je`) | `BE` or `BF` (the rng pick) | `test_game.c` `test_attract` (seed `0xABCD`: pick 1, so `BE`) |
| 8 | `10E06` | `attract.c` `frontend_pause_tail` | the function (public) | `DS_000F0A71 = 0`, `DS_001088D8` byte 3 bit `0x20` and byte 1 bit `0x10`, `DS_00104B19+2 != 0` (`0x10DD4`) | `100` | `test_game.c` `test_attract` ("Gate open, DS_00104B19 byte 2 != 0") |
| 9 | `10E6E` | `attract.c` `frontend_continue_tail` | the function (public) | `DS_000F0A71 = 0`, `DS_001088D8` byte 3 bit `0x10` and byte 1 bit `0x20`, `DS_00104B19+2 != 0` | `100` | `test_game.c` `test_attract` ("Byte-2 branch") |
| 10 | `11024` | `attract.c` `attract_step` case 0 | `attract_step()` | `DSB(DS_000F0A6F) = 0` (`0x11000` switch). Case 0 also plays the two boot movies; a NULL media dir fails them harmlessly | `100` | none (the attract dump driver only) |
| 11 | `11160` | same, case 2 | same | `DSB(DS_000F0A6F) = 2` | `40`, then row 12's `42` | none (the attract dump driver only) |
| 12 | `1116C` | same, case 2 | same | as row 11 | `40`, `42` | as row 11 |
| 13 | `111FF` | same, case 4 | same | `DSB(DS_000F0A6F) = 4`, `DSD(DS_000F0A50)` a record with `+0x2C` in `0x100..0x1100` so the result `< 0x1001` (`0x111E4`); `DS_000F0A5C == 0` gives `54`, else `56` (`0x111F3`/`0x111FA`) | `54` or `56` | none |
| 14 | `17C88` | `camera.c` `camera_projectile_clash` (static) | `camera_projectile_step()` | both `DS_001077B8` and `DS_0010784C` non-zero (`0x17CEA`/`0x17CF3`), boxes overlapping and `DS_00100B1C`/`DS_00100B18 > 0` (`0x17C78`/`0x17C81`) | `64`, then row 58's `BDFFA[ch]` (`0x3B938` at `0x17C92`) | `test_fight.c` `check_projectile_step` (its clash case asserts `B1C = B18 = 0x20`) |
| 15 | `12C29` | `camera.c` `camera_dust_burst` | the function (public), or `camera_dust_hit(node)` | none after the spawn | `BF`, `D6`, `CE` | `test_fight.c` `check_dust_consume` (calls `camera_dust_burst` directly) |
| 16 | `12C33` | same | same | same | as row 15 | as row 15 |
| 17 | `12C3D` | same | same | same | as row 15 | as row 15 |
| 18 | `43741` | `fight.c` `fight_char_screen_open` | the function (public; `fn_register(0x43738)`), or `fight_hook_4367c()` with `DS_00104B1D != 3` (`0x4372E`) | none: first statement | `30` | `test_fight.c` `check_char_screen_open` (calls it) |
| 19 | `444D1` | `fight.c` `fight_char_screen_open_both` | the function (public), or `fight_hook_4367c()` with `DS_00104B1D = 3` (via `fight_4454c`, `0x4462B`) | none: first statement | `30` | `test_fight.c` `check_char_screen_open` (calls it) |
| 20 | `43FB1` | `fight.c` `fight_char_portrait(side)` | the function (public), or `fight_char_select_pass` | the side's `DS_00108154`/`DS_0010815C` records valid; `DS_00108166[side] = ch` in 0..6 (`0x43F08`); no early return | `C888A[ch]` (`C0 C2 C4 C1 C5 C6 C3`) | `test_fight.c` `check_char_select_pass` (calls it) |
| 21 | `43E96` | `fight.c` `fight_char_confirm(side)` | the function (public) | the side's `DS_00108154` record valid; no early return (tail) | `6C` | `test_fight.c` `check_char_select_pass` (calls it) |
| 22 | `44295` | `fight.c` `fight_char_team_portrait(side)` | the function (public) | `DSD(DS_00108154 + side*4) != 0` (`0x44196`); `DS_00108166[side] = ch` (`0x441F5`) | `C888A[ch]` | `test_fight.c` `check_char_team_pass` (calls it) |
| 23 | `444B6` | `fight.c` `fight_char_team_pick(side)` | the function (public) | none: tail | `6C` | `test_fight.c` `check_char_team_pass` (calls it) |
| 24 | `430F2` | `fight.c` `fight_hook_430e8` | the function (public; the `DS_00104AE4` hook) | none: first statement | `31`, then rows 25/26 | `test_fight.c` `check_mode_1a_hooks` (calls it) |
| 25 | `4328A` | same | same | none: tail | `31`, `2D`, `2F` | as row 24 |
| 26 | `43294` | same | same | as row 25 | as row 25 | as row 24 |
| 27 | `44557` | `fight.c` `fight_4454c` (static) | `fight_hook_4367c()` | `DSB(DS_00104B1D) = 3` (`0x4367F`) | `100`, `2E` (row 28), `30` (row 19) | `test_fight.c` `check_mode_1a_hooks` (calls `fight_hook_4367c`; path unconfirmed) |
| 28 | `44626` | same | same | as row 27 | as row 27 | as row 27 |
| 29 | `43696` | `fight.c` `fight_hook_4367c` | the function (public) | `DSB(DS_00104B1D) != 3` (`0x4367F`) | `100`, `2E` (row 30), `30` (row 18) | `test_fight.c` `check_mode_1a_hooks` (calls it) |
| 30 | `43729` | same | same | as row 29 | as row 29 | as row 29 |
| 31 | `41483` | `fight.c` `fight_hook_4142c` | the function (public) | none | `32` | `test_fight.c` `check_mode_17_hooks` (calls it) |
| 32 | `4AD81` | `fight.c` `fight_4ac80(rec)` | the function (public; anim code `0x4AC80`) | `rec+0x14 = entry != 0` (`0x4AC90`), `DS_001088C5 != 0` and entry `+0x1C` bit 5 (`0x4ACB6`/`0x4ACC9`), and `target == 0` (`0x4AD3B`): e.g. the entry actor's `fight_2be00` equal to `DS_00108868`'s | `C8` | `test_fight.c` `check_worshipper_landing` (calls it; path unconfirmed) |
| 33 | `4A6D2` | `fight.c` `fight_4a634` (static) | `fight_effects_pass()` (it calls `fight_4a634` at `0x4A591` unconditionally) | slot `side` `+0x42` bit 1 (`0x4A640`), `DS_001088A8[side]` in `0x20..0x3F` (`0x4A655`/`0x4A65A`); `rng_next(3)` 0/1/2 picks `CD`/`CE`/`CF` (`0x4A664..0x4A688`) | `CD`/`CE`/`CF` by the draw, per side 0 then 1 | `test_fight.c` `check_effects_rng` (calls `fight_effects_pass`; path unconfirmed) |
| 34 | `4A6B7`/`4A6C8` | same | same | slot `+0x42` bit 1, `DS_001088A8[side]` in `0x10..0x17` (`0x4A692`/`0x4A697`), `rng_next(2) != 0` (`0x4A69E`); the second `rng_next(2)` != 0 gives `C9` (`0x4A6B0`), else `CA` | `C9` then `DA`, or `CA` then `DB` | as row 33 |
| 35 | `4A6D2` | same | same | as row 34 | as row 34 (the second id) | as row 33 |
| 36 | `4B98F` | `fight.c` `fight_4b788` (static) | `fight_effects_pass()` via the per-entry prelude `fight_4b69c` (`0x49CFE`) | a list entry with `+0x1C` bit 7 (`0x4B6B3`), `camera_point_hit` != 0 (`0x4B6EC`), and `fight_4b788`'s gates passed (`0x4B7A0`..`0x4B821`) to its tail | `D4` when `index` (`(u8)rec+0x48 - 0x20`) < 3, else `D5` | none |
| 37 | `4CB3E` | `fight.c` `fight_4cb18(entry, index, flag, side)` | the function (public) | none: first statement | `D1` when `index < 3`, else `D0` | `test_fight.c` `check_grab_arms` (calls it) |
| 38 | `4B497` | `fight.c` `fight_4b470` (static) | `fight_4d7a4(entry, index)` (public): entry `+0x1C` bit 7 (`0x4D7BB`), `camera_point_hit` != 0 (`0x4D7F2`), `fight_4d898` != 0 (`0x4D80B`); or `fight_effects_pass()` case 8 (`0x4A10B`) | as the entry | `D1` when `index < 3`, else `D0`; then row 37's id only on the eighth hit (`0x4B547..0x4B59E`) | none |
| 39 | `4DABE` | `fight.c` `fight_4d898(hit, entry, index)` | the function (public) | `fight_4d898`'s gates to its tail (`0x4D8DA`..`0x4D95D`) | `D4` when `index < 3`, else `D5` | `test_fight.c` `check_grab_arms` (calls it; path unconfirmed) |
| 40 | `4C85C` | `fight.c` `fight_4c784(side)` | the function (public), or `fight_4c60c` (`0x4C697`/`0x4C6A9`) | `DSD(DS_00108864)` a valid ball entry; no early return | `D4` when the ball's `(u8)+0x48 - 0x20 < 3`, else `D5`; then `D6`, `CE` | `test_fight.c` `check_volleyball` (calls it) |
| 41 | `4C866` | same | same | same | as row 40 | as row 40 |
| 42 | `4C875` | same | same | same | as row 40 | as row 40 |
| 43 | `4BFF0` | `fight.c` `fight_4bf18` | the function (public), or `game_mode_21_step` | an entry on `DS_0010884C` with `+0x1E = 1` (`0x4BF6F` table `0x4BEF4`), `+0x1C` bit 5 (`0x4BFCD`) and `fight_4a868(entry) != 0` (`0x4BFDC`) | `C8` | `test_fight.c` `check_fx_gate` (calls it; path unconfirmed) |
| 44 | `49FF1` | `fight.c` `fight_effects_pass` case 7 | the function (public) | an entry with `+0x1E = 7`, its actor `+0x3C` in `1..0x53FF` and `+0x1C` bit 2 clear (`0x49FCA..0x49FE1`) | `DE` | `test_fight.c` `check_effects_*` (call it; case 7 unconfirmed) |
| 45 | `4A4B5` | same, the mode-9 tail | same | `DSW(DS_00104B00) = 9` (`0x4A476`), the type-14 count `>=` side count - 2 (`0x4A499`), `DS_001088C3 = 0` (`0x4A4A5`) | `CB` (then row 47 when the drawn countdown is `0x3C`, never: `rng(0x14)+0x78 >= 0x78`) | as row 44 |
| 46 | `4A4ED` | same | same | mode 9, `DS_001088C3 != 0` and `DSW(DS_001088B0) = 0` (`0x4A4D5..0x4A4E6`) | `CB` | as row 44 |
| 47 | `4A52C` | same | same | mode 9, `DS_001088C3 != 0`, `DSW(DS_001088B0) = 0x3C` before the decrement (`0x4A522`) | `DC` | as row 44 |
| 48 | `4DF11` (`mov` `4DF0C`) | `fight.c` `fight_effects_idle_pass` | the function (public) | `DSW(DS_001088B0) = 0` and `DS_001088BB != 0` (`0x4DEF9..0x4DF0A`) | `CB` | `test_fight.c` `check_fx_gate` (calls it; path unconfirmed) |
| 49 | `4DF50` (`mov` `4DF4B`) | same | same | `DSW(DS_001088B0) = 0x3C` and `DS_001088BB != 0` (`0x4DF3D..0x4DF49`) | `DC` | as row 48 |
| 50 | `4EB72` (`mov` `4EA01`, a `jmp` to the shared call) | `fight.c` `fight_4e99c` (static) | `fight_4e67c()` (public) with `DS_001088B9 != 0` (`0x4E91A`); `count` is its `processed` (0 with an empty `DS_0010884C` list) | `r = DS_001088BD + 1` odd and `< 5`, `count = 0` (`0x4E9CC..0x4E9DC`) | `5D` | none |
| 51 | `4EB72` (`mov` `4EA43`) | same | same | `r < 5`, `DS_001088BD` even after the store, `count = 0` (`0x4EA1A`) | `5E` | none |
| 52 | `4EB72` (`mov` `4EA7B`) | same | same | `r < 5`, `count != 0`, `DS_001088BD` odd (`0x4EA62`) | `5D` | none |
| 53 | `4EB72` (`4EAAA` -> `4EB6D`) | same | same | `r < 5`, `count != 0`, even, `v != 0` (`0x4EB6B je`) | `5E` | none |
| 54 | `4EB72` (`mov` `4EAD6`) | same | same | `r >= 5` odd, `count = 0` (`0x4EAAF..0x4EAB6`) | `5D` | none |
| 55 | `4EB72` (`4EB12` -> `4EB6D`) | same | same | `r >= 5`, even, `count = 0` (`0x4EAEF`) | `5E` | none |
| 56 | `4EB72` (`mov` `4EB47`) | same | same | `r >= 5`, `count != 0`, odd (`0x4EB2E`) | `5D` | none |
| 57 | `4EB72` (`4EB6B` -> `4EB6D`) | same | same | `r >= 5`, `count != 0`, even, `v != 0` (`0x4EB6B je`) | `5E` | none |
| 58 | `3B9B8` | `fighter.c` `fighter_3b938(slot)` | the function (public), or `camera_projectile_step()` (row 14) | none: tail; slot `+0x08` a valid record | `BDFFA[ch]`, `ch` = slot `+0x7A` (`63 63 63 63 AC 63 63`; `63` is case 0, a no-op the raw still calls) | `test_fight.c` `check_projectile_step` (calls it) |
| 59 | `362E5` | `fighter.c` `fighter_36280(rec)` | the function (public; anim code `0x36280`) | `rec+0x14` slot != 0 (`0x3628B`) | `6F` | `test_fight.c` `check_land_36280` (calls it) |
| 60 | `367CF` | `fighter.c` `fighter_state_36710(slot, rec)` | the function (public) | the side's `FIGHT_36710_FLAG` byte = 2, `rec+0x36 = 0` and `rec+0x1C = 0` (the third arm) | `6E` | `test_fight.c` `check_state_handlers` (calls it; path unconfirmed) |
| 61 | `35ED9` | `fighter.c` `fighter_state_35e6c(slot, rec)` | the function (public) | none: after `0x35ED0` | `6D` | `test_fight.c` `check_gap_handlers` (calls it) |
| 62 | `37D73` | `fighter.c` `fighter_37d18(slot, rec)` | the function (public) | none: tail | `BDAD4[ch]`, `ch` = slot `+0x7A` (`92 97 A7 A1 85 8B 4A`) | `test_fight.c` `check_p52_40554` (calls it) |
| 63 | `3923D` | `fighter.c` `fighter_39040(side)` | the function (public) | `FIGHT_D2C_BASE[side] > 1` (`0x39056`), slot `+0x63 = 0` (`0x39117`), and not (`a >= 0x23` and `r2 >= 4`) with `a <= 0x23` (`0x39190`/`0x391DE`); `r3 = DS_001088A8[side]` in `0x20..0x3F` takes `rng_next(3)` (`0x391FE`), else `rng_next(2)` (`0x39228`) | `CD`/`CE`/`CF` by `rng_next(3)` = 0/1/2 (`0x3920E..0x3921C`); else `DA` when `rng_next(2) != 0`, `DB` when 0 | `test_fight.c` `check_combo_text` (calls it; path unconfirmed) |
| 64 | `353C0` | `fighter.c` `fighter_state_3531c(side)` | the function (public) | `DSD(DS_001077A8 + side*4)` a slot with `DSD(slot) != 0` (`0x3539A`), `DSW(DS_001078F6) = 1` so it reaches `< 1` after the decrement (`0x353A7`) | `EC`, `E0` | none found (no `fighter_state_3531c` caller in the tests seeds `DS_001078F6` non-zero) |
| 65 | `353CA` | same | same | as row 64 | as row 64 | as row 64 |
| 66 | `35E38` | `fighter.c` `fighter_35e04(rec)` | the function (public; anim code `0x35E04`) | `rec+0x14` slot != 0 (`0x35E0B`) | `BDAA8[ch]`, `ch` = slot `+0x7A` (`49 49 7B 49 49 49 49`) | `test_fight.c` `check_deep_callees` (calls it) |
| 67 | `34FA4` | `fighter.c` `hit_reaction_apply(side, reaction)` | the function (public) | `reaction != 0xFF` (`0x34E36`) and the triple's stream != 0 (`0x34F85`) | `E9308[b]`, `b` = byte `anim[0]+7` (`[esp+0x18]+7`, `0x34F8F`); then row 68's `E9308[b]` when the callback != 0 | `test_fight.c` `check_char1_reactions_b` and the other `check_char*` reaction tests (call it) |
| 68 | `35032` | same | same | `reaction != 0xFF` and the triple's callback `anim[1]` dword != 0 (`0x35017`) | `E9308[b]` (the same byte, `0x3501D`) | as row 67 |
| 69 | `3D19D` | `fighter.c` `fighter_3d17c(slot, rec, side)` | the function (public; `fn_register(0x3D17C)`) | slot `+0x08 = 0` (`0x3D187`) | `91` | `test_fight.c` `check_trex_breath` (calls it) |
| 70 | `14807` | `fighter.c` `fighter_146f0` (static) | `fighter_1461c(slot, rec, side)` (`fn_register(0x1461C)`) | `ctx[2]+0x57 = 2` (`0x14632` switch, case `0x146D7`), then `fighter_146f0`'s gates (`0x14728`, `0x1473A`, `0x147A5`) to `0x14807` | `B1` | none |
| 71 | `14829` | `fighter.c` `fighter_14814(slot, rec, side)` | the function (public; `fn_register(0x14814)`) | none | `B0` | none |
| 72 | `14E38` | `fighter.c` `fighter_14d7c(side)` | the function (public; `fn_register(0x14D7C)`) | none after the `0x14D95` arm | `E933C[..]` (row 76, `0x14DE4`), then `B3` | `test_fight.c` `check_throw_3c208` (calls it) |
| 73 | `14B84` | `fighter.c` `fighter_14a5c` (static) | `fighter_14988(slot, rec, side)` (`fn_register(0x14988)`) | the case-2 arm (`0x14A43`), then `fighter_14a5c`'s gates (`0x14A94`, `0x14AA6`, `0x14B22`) | `B1` | none |
| 74 | `14BA5` | `fighter.c` `fighter_14b90(slot, rec, side)` | the function (public; `fn_register(0x14B90)`) | none | `B0` | `test_fight.c` `check_char3_2628` (calls it) |
| 75 | `39EDA` | `fighter.c` `fighter_39cc8(slot, side)` | the function (public; `fn_register(0x39CC8)`) | slot `+0x58 = 3` (`0x39CE2` table `0x39CB4`), `land` (`0x39E13`: `BD882[ch] >> 16 >= slot+0x30`) | `6C` | `test_fight.c` `check_knockback_pose` (calls it; path unconfirmed) |
| 76 | `3996E` | `fighter.c` `fighter_39834` (static) | `fighter_14d7c(side)` (public), or any caller below | none before the call | `E933C[b]`, `b` = byte `anim[0]+8` (`0x39959`; `63..6A 46 75 AC 6F 6B B7`; `46` is case 3, two handles) | via `check_throw_3c208` (`fighter_14d7c`) |
| 77 | `22BB0` | `fighter.c` `fighter_22b28` (static) | `fighter_235c4(side)` or `fighter_22ce4(side)` (public) | none | `E933C[..]` (row 76, `0x2360F` or `0x22D32`), then `B5` | `test_fight.c` `check_freeze_235c4` (calls `fighter_235c4`) |
| 78 | `22F00` | `fighter.c` `fighter_22e44(side)` | the function (public; `fn_register(0x22E44)`) | none | `B6` | none |
| 79 | `236CB` | `fighter.c` `fighter_2365c(slot, rec, side)` | the function (public; reaction callback `0x2365C`) | slot `+0x08 = 0` (`0x23662`), the other slot's `+0x10 != 0x22BEC` (`0x2368A`) | `B4` | `test_fight.c` `check_char1_reactions` (calls it; path unconfirmed) |
| 80 | `2316B` (`mov` `23166`) | `fighter.c` `fighter_23130(slot, rec, side)` | the function (public) | none | `7C` | `test_fight.c` `check_char1_reactions_b` (calls it) |
| 81 | `231B3` (`mov` `231AE`) | `fighter.c` `fighter_23178(slot, rec, side)` | the function (public) | none | `7C` | `test_fight.c` `check_char1_reactions_b` (calls it) |
| 82 | `24684` | `fighter.c` `fighter_24568(side)` | the function (public; `fn_register(0x24568)`) | none: the placement loop always breaks | `B4` | none |
| 83 | `40E09` | `fighter.c` `fighter_40cb0(side)` | the function (public) | as row 82 | `8C` | none |
| 84 | `48957` | `fighter.c` `fighter_488b8(slot, rec, side)` | the function (public) | none | `7B` | none |
| 85 | `4929C` | `fighter.c` `fighter_49150(side)` | the function (public) | as row 82 | `A2` | none |
| 86 | `15A22` | `fighter.c` `fighter_159a8(rec)` | the function (public; anim code `0x159A8`) | `rec+0x14` slot != 0 (`0x159B2`) | `B1` | none |
| 87 | `15B80` | `fighter.c` `fighter_15a34(side)` | the function (public) | as row 82 | `9C` | none |
| 88 | `4612C` | `fighter.c` `fighter_45fe8(side)` | the function (public) | as row 82 | `80` | none |
| 89 | `3E057` | `fighter.c` `fighter_3dfc0(slot, rec)` | the function (public) | the other slot record `o` != 0 (`0x3DFD9`) | `B9` | `test_fight.c` `check_char5_entrance` (calls it) |
| 90 | `40FB0` | `fighter.c` `fighter_40e64(side)` | the function (public) | as row 82 | `86` | none |
| 91 | `247F5` | `fighter.c` `fighter_24754(side)` | the function (public), or `fighter_24804` | none | `A8` | `test_fight.c` `check_char6_entrance` (calls it) |
| 92 | `3ABAF` | `fighter.c` `fighter_reaction_apply(slot, reaction)` | the function (public) | `fighter_3a280(other+0x5F) != 0` (`0x3AB81/0x3AB8D`) | `E933C[..]` (row 76, `0x3AB7C`), then `BE008[ch]`, `ch` = self `+0x7A` (`90 95 A5 9F 83 89 9A`) | `test_fight.c` `check_pose_entry` (calls it; path unconfirmed) |
| 93 | `3ADC1` | `fighter.c` `fighter_3ad98(side, anim)` | the function (public) | none | `E9358[b]`, `b` = byte `anim[0]+9` (`63 70 71 75 63 74 AC`) | `test_fight.c` `check_pose_entry` (calls it) |
| 94 | `153BF` | `fighter.c` `fighter_15350(slot, rec, side)` | the function (public; `fn_register(0x15350)`) | `ai_pred_468d8(other) == 0` (`0x15360..0x15375`) | `AF` | `test_fight.c` `check_char3_2425` (calls it; path unconfirmed) |
| 95 | `3E30C` | `fighter.c` `fighter_3e244(side)` | the function (public) | none | `C75AA[ch]`, `ch` = `ctx[3]`'s char (`91 96 A6 A0 84 8A 9B`) | `test_fight.c` `check_hold_3e244` (calls it) |
| 96 | `48B1E` | `fighter.c` `fighter_48aac(slot, rec, side)` | the function (public) | state `st = 0` (`0x48AC3`), distance `<= 0x100` (`0x48AFA`) | `C75AA[ch]`, then `59` | `test_fight.c` `check_finisher_48aac` (calls it; path unconfirmed) |
| 97 | `48B50` | same | same | as row 96 | as row 96 | as row 96 |
| 98 | `48C40` | `fighter.c` `fighter_48be0(slot, rec)` | the function (public) | none | `4B` | none |
| 99 | `3A32D` | `fighter.c` `fighter_3a2a0(side, ...)` | the function (public) | `(u8)r = 0` (`0x3A2F7`), `fighter_3a280(ctx[2]+0x5F) != 0` (`0x3A2FF..0x3A312`) | `BE008[ch]`, `ch` = `ctx[3]`'s char | `test_fight.c` `check_char4_react_b` (calls it; path unconfirmed) |
| 100 | `44A53` | `fighter.c` `fighter_c4_spawn` (static; the shared body of `0x449B8` and `0x44A64`) | `fighter_449b8(rec)` (public) | `rec+0x14` slot != 0 (`0x449C6`) | `AB` | none |
| 101 | `44C6B` | `fighter.c` `fighter_44bac(side)` | the function (public) | none | `C75AA[ch]` | `test_fight.c` `check_char4_react_a` (calls it) |
| 102 | `4514D` | `fighter.c` `fighter_450e8(slot, rec, side)` | the function (public) | none | `47` | `test_fight.c` `check_char4_react_b` (calls it) |
| 103 | `452C6` | `fighter.c` `fighter_45238(rec)` | the function (public; anim code `0x45238`) | `rec+0x14` slot != 0 (`0x4523F`); either arm of `DSW(DS_00104B00) != 0x25` (`0x45252`) | `7B` | `test_fight.c` `check_char4_react_c` (calls it) |
| 104 | `459B9` (`mov` `459A3`, `jmp`) | `fighter.c` `fighter_45908(rec)` | the function (public), or `fighter_4579c` | slot and `other` != 0 (`0x4591D`/`0x45935`), `other+0x52 = 0x0F` (`0x4597A`) | `46` (case 3: two handles `0x28847C9`, `0x2886158`) | `test_fight.c` `check_char4_react_c` (calls it; path unconfirmed) |
| 105 | `459AF` | same | same | slot and `other` != 0, `other+0x52 != 0x0F` | `6C`, `72` | as row 104 |
| 106 | `459B9` | same | same | as row 105 | as row 105 | as row 104 |
| 107 | `3F312` (`mov` `3F30D`) | `fighter.c` `fighter_3f308(slot)` | the function (public), or `fighter_3e6a8` with `+0x57 = 0` (`0x3E6DA`) | none | `6C`, `72` (via `fighter_3e6a8`: row 76's `E933C[..]` first, `0x3E702`) | none |
| 108 | `3F356` (`mov` `3F351`) | same | same | none | as row 107 | none |
| 109 | `21F3A` (`mov` `21F35`) | `fighter.c` `fighter_21f30` (static) | `fighter_21458(slot, rec, side)` (public; `fn_register(0x21458)` wraps it) | slot `+0x58 = 0` (`0x21470` table `0x21448`), `thr > slot+0x30` (`0x214A1`) | `E933C[..]` (row 76, `0x214B2`), `6C`, `72` | none |
| 110 | `21F7E` (`mov` `21F79`) | same | same | as row 109 | as row 109 | none |
| 111 | `23328` (`mov` `23323`) | `fighter.c` `fighter_232b4(side)` | the function (public) | `fighter_command_dispatch(ctx[1], ctx[2]+0x5F)` returns 0 (`0x232CA..0x232E2`) | `A9` | `test_fight.c` `check_sc_char6` (calls it; path unconfirmed) |
| 112 | `3E0E3` | `fighter.c` `fighter_3e064(slot, rec, side)` | the function (public) | none | `B9` | `test_fight.c` `check_sc_char5` (calls it) |
| 113 | `3DE47` | `fighter.c` `fighter_3dd84(side)` | the function (public) | none | `B7` | `test_fight.c` `check_sc_char5` (calls it) |
| 114 | `3EED3` | `fighter.c` `fighter_3ee00(slot, rec, side)` | the function (public) | the other side's `DS_001077A8` entry != 0 (`0x3EE0C`), slot `+0x57 = 1` (case 1, `0x3EEC1`) | `BB` | `test_fight.c` `check_posecb_3ee00` (calls it; path unconfirmed) |
| 115 | `45BAE` | `fighter.c` `fighter_45b50(slot, rec, side)` | the function (public) | slot `+0x57 = 1` (`0x45B65..0x45B6D`) | `50` | `test_fight.c` `check_49z_45b50` (calls it; path unconfirmed) |
| 116 | `45BF4` | same | same | slot `+0x57 = 2` (`0x45B71`), `DSW(DS_001081EC) = 1` so the decrement reaches 0 (`0x45BD8`) | `D2` | as row 115 |
| 117 | `47A7B` | `fighter.c` `fighter_47a00(side)` | the function (public), or `fighter_47b04` | none: after `0x47A71` | `E933C[..]` (row 76, `0x47A71`), then `46` | `test_fight.c` `check_49z_47a00` (calls it) |
| 118 | `408F2` (`mov` `408ED`) | `fighter.c` `fighter_408e8` (static) | `fighter_40954(slot, rec, side)` (public; `fn_register(0x40954)`) | slot `+0x57 = 2` (`0x40979` table `0x40940`, case `0x40A20`) | `6C`, `72`, then row 121's `EE` | `test_fight.c` `check_fs_40954` (calls `fighter_40954`; case 2 unconfirmed) |
| 119 | `40936` (`mov` `40931`) | same | same | as row 118 | as row 118 | as row 118 |
| 120 | `3EFD2` (`mov` `3EFC9`) | `fighter.c` `fighter_3ef44(slot, rec, side)` | the function (public) | the other slot `os` != 0 (`0x3EF5C`) | `47` | `test_fight.c` `check_fs_3ef44` (calls it) |
| 121 | `40B2B` (`mov` `40B26`) | `fighter.c` `fighter_40954(slot, rec, side)` | the function (public) | slot `+0x57 = 2` (`0x40969`/`0x40979`) | `6C`, `72` (rows 118/119, `0x40A20`), then `EE` | as row 118 |
| 122 | `407D3` (`mov` `407CE`) | `fighter.c` `fighter_406b4()` | the function (public) | none: after the spawn loop | `73` | `test_fight.c` `check_fs_406b4` (calls it) |
| 123 | `3705E` | `fighter.c` `fighter_36f10(slot)` | the function (public) | `DS_00104B13 != 0` (`0x36F17`), `os` != 0 (`0x36F36`), and `armb`: `DS_00104B14 != 0`, or `os`'s record side != `DSD(DS_00104AD4)`, or `os+0x63 = 1` (`0x36F71..0x36F9A`) | `BDAD4[ch]`, `ch` = slot `+0x7A` | `test_fight.c` `check_fs_36f10` (calls it; path unconfirmed) |
| 124 | `3FD23` (`mov` `3FD1E`) | `fighter.c` `fighter_3fcb0(rec)` | the function (public; anim code `0x3FCB0`) | `DS_00105B3A <= 1` (`0x3FCB6`), the other side's slot `o` != 0 (`0x3FCD3`), `o+0x52 = 0x10` (`0x3FCD7`) | `48` | `test_fight.c` `check_p54_3fcb0` (calls it; path unconfirmed) |
| 125 | `40546` (`mov` `40541`) | `fighter.c` `fighter_40434(rec)` | the function (public; anim code `0x40434`) | `rec+0x14` slot != 0 (`0x4043F`), the other slot `o` != 0 (`0x404DC`) | `B8` | `test_fight.c` `check_p54_40434` (calls it; path unconfirmed) |
| 126 | `45F8C` (`mov` `45E59`, `jmp`) | `fighter.c` `fighter_45d98()` | the function (public; update entry 17) | an entry `e` with state byte `UPD17_STATE+e = 0` (`0x45D9F`), `rng_next(0x0A) = 0` (`0x45DBF`) | `AB` per such entry, in `e` order | `test_game.c` `check_update_k8c` (calls it; path unconfirmed) |
| 127 | `45F8C` (`mov` `45F81`) | same | same | an entry with state 3 and `(s16)rec+0x36 + rec+0x1C <= 0` (`0x45F3A..0x45F47`) | `AC` | as row 126 |
| 128 | `240B8` | `fighter.c` `fighter_24078(rec)` | the function (public; anim code `0x24078`) | the other side's `DS_001077A8` entry `o` != 0 (`0x24093`) | `6B`, then `BE008[o+0x7A]` (`0x240BF`) | `test_game.c` `check_anim_setters` (calls it) |
| 129 | `240CF` | same | same | as row 128 | as row 128 | as row 128 |
| 130 | `415DC` | `flow.c` `frontend_darken_marked` | the function (public), or `game_mode_12_step` | none: after the list walk | `33` | `test_game.c` `test_effects` (calls it) |
| 131 | `41D2B` | `flow.c` `game_mode_12_step` case 1 | `game_mode_12_step()` (public) | `DSB(DS_00104B25) = 1` (`0x41C61`), a stage `i` from `DS_0010810F` below 7, `i != DSW(DS_00104AFC)` (`0x41CF6`) and `DS_00108106[i]` bit 7 set (`0x41CFE`) | `34 + n`, `n` = `DS_00108112` before its increment (`0x41D1A..0x41D28`), one per marked stage in the walk | `test_fight.c` `check_mode_12_*` (call it; case 1 unconfirmed) |
| 132 | `4207F` | same, case 3 | same | `DSB(DS_00104B25) = 3`, `(s16)DSW(DS_001080F4 rec +0x36) + rec+0x1C >= 0x2300` (`0x41E7D`) | `3A` | as row 131 |
| 133 | `420B7` | same, case 4 | same | `DSB(DS_00104B25) = 4`, `DS_0010810E = 7` so the increment gives 8 (`0x420AD cmp eax,8`) | `C7` | as row 131 |
| 134 | `42112` (`mov` `4210D`) | same, case 5 | same | `DSB(DS_00104B25) = 5`, `DS_0010810E >= 8` (`0x420E8`), `DS_00104B1F != 3` (`0x420F9`) | `33` | as row 131 |
| 135 | `42229` | same, case 6 | same | `DSB(DS_00104B25) = 6` (`0x4217D`), `DS_001080F4` a valid record | `BC` (EDX = the remainder) | as row 131 |
| 136 | `28D8B` | `flow.c` `frontend_char_screen_hook_voice` | the function (public; `fn_register(0x28D80)`) | none: first statement | `2E` | `test_fight.c` `check_char_screen_modes` (calls it) |
| 137 | `26A2C` | `flow.c` `game_hook_26998` | the function (public) | none after `fighter_spawn` (`0x269E5`) | `28` | `test_fight.c` `check_mode_1a_hooks` (calls it) |
| 138 | `270F9` | `flow.c` `game_hook_270bc` | the function (public) | none | `25` | `test_fight.c` `check_mode_1a_hooks` (calls it) |
| 139 | `257AD` | `flow.c` `game_coin_divert(players)` | the function (public), or `game_state_step()` with `DS_00104B1D = 0` and a coin accepted (`0x11D34`) | none: first statement | `100`, then `53` (row 140) | `test_fight.c` `check_coin_divert` (calls it) |
| 140 | `25820` | same | same | none: tail | `100`, `53` | as row 139 |
| 141 | `28DCA` | `flow.c` `flow_player_join(side)` | the function (public) | none | `100` | `test_fight.c` `check_33c18_callers_a` (calls it) |
| 142 | `282BB` | `flow.c` `flow_match_result_text` | the function (public) | `DSD(DS_00104AD4) = 0xFFFFFFFF` (`0x28139`) | `24` | `test_fight.c` `check_33c18_callers_b` (calls it; path unconfirmed) |
| 143 | `2817F`/`281F5` | same | same | `DSD(DS_00104AD4) = 0` (or 1), and `(s32)DSD(DS_001088EF) >> 24 < 1` or the side's slot `+0x63 != 0` (`0x28164..0x28178`, `0x281DA..0x281EE`) | `24` | as row 142 |
| 144 | `282BB` | same | same | `DSD(DS_00104AD4) = 2` (`0x2814F`) | `24` | as row 142 |
| 145 | `2730A` | `flow.c` `flow_arena_ko_check` | the function (public), or `game_mode_0c_step` | `DS_0010780A[DS_0010810D slot] >= 0x78` (`0x272F4..0x27303`) | `D3` | none |
| 146 | `27347` | same | same | the first test fails, `DS_0010780A[DS_00104B12 slot] >= 0x78` (`0x2731B..0x27340`) | `27`, `22` | none |
| 147 | `27351` | same | same | as row 146 | as row 146 | none |
| 148 | `27B01` | `flow.c` `game_mode_0e_step` | the function (public) | `flow_continue_poll` returns 0 (`0x27A2F`), the tick arm (`0x27ACD..0x27AE3`), `DS_00108110 = 0` so the decrement goes negative (`0x27AF3`) | `27`, `22` | `test_fight.c` `check_mode_0e` (calls it; path unconfirmed) |
| 149 | `27B0D` | same | same | as row 148 | as row 148 | as row 148 |
| 150 | `2759E` | `flow.c` `game_mode_0d_step` | the function (public) | `DS_00104B0C != 0` (`0x2756E`), `DS_00104B21 = 6` so `n = 7` (`0x27590`) | `2A`, then `flow_match_result_text`'s `24` (rows 142-144) | `test_fight.c` `check_2c2b0` (calls it; path unconfirmed) |
| 151 | `277B0` | same | same | `DS_00104B0C != 0`, `n != 7` (the arm after `0x276C0`) | `25` when `DS_00104B0A ^ 1 = 0`, else `26` (`0x277A8 add eax,0x25`) | as row 150 |
| 152 | `297C4` | `flow.c` `game_mode_32_step` | the function (public) | the round-won arm; `DS_00104AF0 = 4` or `DS_00104AF1 = 4` after the increment (`0x2979D..0x297B9`) | `2A`, then row 142-144's `24` | `test_fight.c` `check_33c18_callers_b` (calls it; path unconfirmed) |
| 153 | `29960` | same | same | the round-won arm, neither count at 4 | `25` or `26` (as row 151) | as row 152 |
| 154 | `25E39` | `flow.c` `game_mode_05_step` case 2 | the function (public) | `DSB(DS_00104B25) = 2` (`0x25C92` switch) | `D9` when `DS_00104B14 != 0`, else `D7` | `test_fight.c` `check_mode5_a`/`_b` (call it; case 2 unconfirmed) |
| 155 | `294D5` (`mov` `294CE`) | `flow.c` `game_mode_30_step` case 2 | the function (public) | `DSB(DS_00104B25) = 2` (`0x29332` switch) | `D7` | `test_fight.c` `check_mode_30` (calls it; case 2 unconfirmed) |
| 156 | `29983` | `flow.c` `flow_round_over_check` | the function (public), or `game_mode_31_step` | `DS_0010780A >= 0x78` (`0x29974`) | `27`, `22`; then rows 158/159 when `DS_001078BE >= 0x78` too | `test_fight.c` `check_flow_round_over_check` (calls it) |
| 157 | `29992` | same | same | as row 156 | as row 156 | as row 156 |
| 158 | `299C1` | same | same | `DS_001078BE >= 0x78` (`0x299AD`) | `27`, `22` | as row 156 |
| 159 | `299CD` | same | same | as row 158 | as row 158 | as row 156 |
| 160 | `29698` (`mov` `2968C`) | `flow.c` `game_mode_33_step` | the function (public) | `DSW(DS_00104AFE) = 1` so the decrement is `<= 0` (`0x29687`) | `2B` (and the idle pass's rows 48/49 when their gates hold, `0x2965F`) | `test_fight.c` `check_mode_33` (calls it; path unconfirmed) |
| 161 | `4F581` (`mov` `4F577`) | `flow.c` `flow_round_timer_step` | the function (public; update entry) | `DS_00105B3B = 0` (`0x4F52C`), `DSD(DS_001088D0)` in `1..0x62` (`0x4F539`), `DSW(DS_00104AF4) % DS_001088D0 = 0` (`0x4F548..0x4F55E`), `DS_001088F2 != 0` (`0x4F560`) and `<= 10` signed (`0x4F569..0x4F575`) | `52` | `test_fight.c` `check_49z_round_timer` (calls it; path unconfirmed) |
| 162 | `27FF9` | `flow.c` `flow_round_end_check` | the function (public) | a side at `>= 0x78` (`0x27FB3`/`0x27FBF`), `DSW(DS_00104B00) != 0x0B`, `DS_00104B1E = DSD(DS_00104ADC)`, `DSD(DS_00104AD4) = 2` (`0x27FCF..0x27FF2`) | `D3` | `test_fight.c` `check_fight_frame_b` (calls it; path unconfirmed) |
| 163 | `2805F` | same | same | both sides `< 0x78`, `(s32)DSD(DS_001088EF) >> 24 < 1` (`0x28049..0x28054`) | `D3` | as row 162 |
| 164 | `42E7F` | `flow.c` `flow_challenge_poll` | the function (public), or `game_mode_13_step` | `DS_00108110 = 0` so the decrement goes negative (`0x42E35`), `DSD(DS_00104AD4) != 2` and that slot's `+0x63 != 1` (`0x42E43..0x42E65`) | `2D` | `test_fight.c` `check_mode_13_b` (calls it; path unconfirmed) |
| 165 | `4250D` | `flow.c` `game_mode_13_step` case 0 | the function (public) | `DSB(DS_00104B25) = 0` (`0x424EE`) | `2C` | `test_fight.c` `m13_step` (calls it; case 0 unconfirmed) |
| 166 | `25AF1` | `flow.c` `game_hook_25ae8` | the function (public) | none: first statement | `100`, then `3D` | `test_fight.c` `check_mode_17_hooks` (calls it) |
| 167 | `25B51` | same | same | none | as row 166 | as row 166 |
| 168 | `28C2A` | `flow.c` `game_mode_0a_step` | the function (public) | `DS_00107804 = 0` and `DS_00107898 = 0` (`0x28C0E..0x28C1E`) | `D8` | `test_fight.c` `check_mode_0a` (calls it; path unconfirmed) |
| 169 | `27821` | `flow.c` `game_mode_0f_step` | the function (public) | `DSW(DS_00104AFE) = 1` so `hold <= 0` (`0x27811`) | `2B` | `test_fight.c` `check_mode_0f` (calls it; path unconfirmed) |
| 170 | `121CE` | `flow.c` `game_state_title` (static) | `game_state_step()` with `DSW(DS_000F0A64) = 1` | `DSB(DS_000F0A6F) = 0` (the title's first entry) | `41`, `43` | `test_game.c` `test_frontend` (the title entry); Task 3 owns these two rows |
| 171 | `121D8` | same | same | as row 170 | as row 170 | as row 170 |
| 172 | `1159F` | `flow.c` `game_state_4` (static) | `game_state_step()` with `DSW(DS_000F0A64) = 4` | `DSW(DS_0009AD98) = 0` | `100` | none |
| 173 | `116C4` | same | same | `DSW(DS_0009AD98) = 1` | `100` | none |
| 174 | `11844` | same | same | `DSW(DS_0009AD98) = 2` | `100` | none |
| 175 | `11A94` | `flow.c` `game_state_6` (static) | `game_state_step()` with `DSW(DS_000F0A64) = 6` | none: first statement | `100` | `test_fight.c` `check_state6` (drives state 6) |
| 176 | `1EEF4` | `flow.c` `game_mode_1e_step` case 0 | the function (public) | `DSB(DS_00104B25) = 0`, `DSD(DS_00104AD4) = 2`, `DS_00104B1F = 0`, `hiscore_rank_pair() != 0` (`0x1EECF..0x1EEE8`) | `100`, `E1` | `test_fight.c` `check_mode_1e_*` (call it; path unconfirmed) |
| 177 | `1EEFE` | same | same | as row 176 | as row 176 | as row 176 |
| 178 | `1F1EC` | same, case 4 | same | `DSB(DS_00104B25) = 4`, `DS_00107813 = 0` (`0x1F199`), `hiscore_rank_single(DS_001077EC) != 0` (`0x1F1A6`), `advance` (`0x1F1B8`) | `100`, `E1` | as row 176 |
| 179 | `1F1F6` | same | same | as row 178 | as row 178 | as row 176 |
| 180 | `1F249` | same, case 5 | same | `DSB(DS_00104B25) = 5`, `nameentry_step(0) = 0` (`0x1F203`) | `nameentry_step(0)`'s ids, then `E3`, `E2` | as row 176 |
| 181 | `1F253` | same | same | as row 180 | as row 180 | as row 176 |
| 182 | `1F307` | same, case 7 | same | `DSB(DS_00104B25) = 7`, `DS_001078A7 = 0` (`0x1F2B7`), `hiscore_rank_single(DS_00107880) != 0` (`0x1F2C4`), `advance` | `100`, `E1` | as row 176 |
| 183 | `1F311` | same | same | as row 182 | as row 182 | as row 176 |
| 184 | `1F36C` | same, case 8 | same | `DSB(DS_00104B25) = 8`, `nameentry_step(1) = 0` (`0x1F326`) | `nameentry_step(1)`'s ids, then `E3`, `E2` | as row 176 |
| 185 | `1F376` | same | same | as row 184 | as row 184 | as row 176 |
| 186 | `1EF8D`/`1F07F`/`1F109`/`1F01F` | same, cases `0x0B..0x0E` | same | `DSB(DS_00104B25)` in `0x0B..0x0E`, `nameentry_step(side_of[k]) = 0` (`0x1EF46`/`0x1F039`/`0x1F0C3`/`0x1EFD8`) | `nameentry_step`'s ids, then `E3`, `E2` | as row 176 |
| 187 | `1EF97`/`1F089`/`1F113`/`1F029` | same | same | as row 186 | as row 186 | as row 176 |
| 188 | `1EFA9` | same, case `0x0F` | same | `DSB(DS_00104B25) = 0x0F` | `E1`, then `nameentry_step(1)`'s ids (`0x1EFC3`) | as row 176 |
| 189 | `1F099` | same, case `0x10` | same | `DSB(DS_00104B25) = 0x10` | `E1`, then `nameentry_step(0)`'s ids (`0x1F0B4`) | as row 176 |
| 190 | `20955` | `flow.c` `game_mode_1f_step` case 0 | the function (public) | `DSB(DS_00104B25) = 0` (`0x2091B`) | `3B` | `test_fight.c` `check_mode_1f_state0` (calls it) |
| 191 | `26DA9` | `flow.c` `flow_26d4c` (static) | `game_mode_22_step()` (public) | `(s32)DSD(DS_00104AC4) <= 0` (`0x26D54`) | `29`, `22` | none |
| 192 | `26DB8` | same | same | as row 191 | as row 191 | none |
| 193 | `26B32` | `flow.c` `game_mode_23_step` case 2 | the function (public) | `DSB(DS_00104B25) = 2` (`0x26A55` switch) | `60` | `test_fight.c` `check_mode_23` (calls it; case 2 unconfirmed) |
| 194 | `11DF2` | `flow.c` `game_state_step` case 5 | `game_state_step()` | `DSW(DS_000F0A64) = 5` | `100` | none |
| 195 | `11BF0` (tail `jmp`) | same, case 7's timer exit | same | `DSW(DS_000F0A64) = 7`, `DSW(DS_000F0A6A) = 1` so the new value is 0 (`0x11E72`) | `100` | `test_game.c` `check_timer_exit` (drives the exit) |
| 196 | `2FA6D` | `menu.c` `menu_run(table, ...)` | the function (public) | none: after `config_screen_wait_zero` | `100` | `test_platform.c` `check_menu_run` (calls it) |
| 197 | `2FFFD` (`mov` `2FFF6`) | `menu.c` `menu_step(table, stride, flags)` | the function (public) | `DSB(MENU_ACTIVE) = 0` (`0x2FFCD`) | `100` | `test_platform.c` `check_menu_step` (calls it; first-call path) |
| 198 | `200B5` | `nameentry.c` `nameentry_cells_step` case 1 | the function (public), or `nameentry_step(side)` (it runs first, `0x1F464`) | a cell `i` (below `NE_CELL_CT`) with state byte `+0x12 = 1` (`0x1FFFA..0x20036`) | `B0`, `7B` per such cell, in cell order | `test_fight.c` `check_nameentry_cells` (calls it; case 1 unconfirmed) |
| 199 | `200BF` | same | same | as row 198 | as row 198 | as row 198 |
| 200 | `2012D` (`mov` `20128`) | same, case 2 | same | a cell with `+0x12 = 2` and `tgt < y` (`0x200F7`) | `71` | as row 198 |
| 201 | `201D2` (`mov` `201C6`/`201CD`) | same, case 3 | same | a cell with `+0x12 = 3` and `tgt < +0x08` (`0x20183`) | `E7` for an odd cell index `i`, else `E8` | as row 198 |
| 202 | `202C5` | same, case 5 | same | a cell with `+0x12 = 5` and `tgt > x` (`0x2027D`) | `70`, `4D` (case 3: two handles) | as row 198 |
| 203 | `202CF` | same | same | as row 202 | as row 202 | as row 198 |
| 204 | `2043F` | same, case 8 | same | a cell with `+0x12 = 8`, letter code `+0x13 = 0x1B` (`0x203E0`) | `E9` | as row 198 |
| 205 | `204B5` | same, case 9 | same | a cell with `+0x12 = 9` (`0x20487`) | `E9` | as row 198 |
| 206 | `1F52B` (`mov` `1F526`) | `nameentry.c` `nameentry_step(side)` | the function (public), or `game_mode_1e_step` | not finished (`DSW(DS_0010438C) != 0` or `DS_001044AC != 0`, `0x1F4AD`), `DS_001044D8 = 0` (`0x1F4D2`), the side's pad byte bit `0x10` (`0x1F4DF..0x1F501`); the cells step runs first | `E6`, then `39` (row 207) or `34` (row 208) | `test_fight.c` `check_nameentry_step` (calls it; path unconfirmed) |
| 207 | `1F593` (`mov` `1F580`) | same | same | as row 206, and the column after `+3` past the row's limit (`0x20` on row `0xF`, else `0x1D`; `0x1F549..0x1F579`) | `E6`, `39` | as row 206 |
| 208 | `1F593` (`mov` `1F58E`) | same | same | as row 206, the column within the limit | `E6`, `34` | as row 206 |
| 209 | `1F5FD` (`mov` `1F5F8`) | same | same | not finished, `DS_001044D8 = 0`, no bit `0x10`, pad bit `0x20` (`0x1F5B6..0x1F5F6`) | `E6`, then `35` (row 210) or `38` (row 211) | as row 206 |
| 210 | `1F644` (`mov` `1F638`) | same | same | as row 209, the column after `-3` below `0xB` (`0x1F60A..0x1F615`) | `E6`, `35` | as row 206 |
| 211 | `1F644` (`mov` `1F63F`) | same | same | as row 209, the column at or above `0xB` | `E6`, `38` | as row 206 |
| 212 | `1F6AE` (`mov` `1F6A9`) | same | same | no bit `0x10`/`0x20`, pad bit `0x80` (`0x1F667..0x1F6A7`) | `E6`, then `39` (row 213) or `37` (row 214) | as row 206 |
| 213 | `1F6E8` (`mov` `1F6DE`) | same | same | as row 212, the column `= 0x20` (`0x1F6D1..0x1F6DC`) | `E6`, `39` | as row 206 |
| 214 | `1F70A` (`mov` `1F705`) | same | same | as row 212, the column `!= 0x20` | `E6`, `37` | as row 206 |
| 215 | `1F75F` (`mov` `1F75A`) | same | same | no bit `0x10`/`0x20`/`0x80`, pad bit `0x40` (`0x1F714..0x1F754`) | `E6`, then `38` (row 216) or `36` (row 217) | as row 206 |
| 216 | `1F794` (`mov` `1F78F`) | same | same | as row 215, the column `= 0x20` (`0x1F782..0x1F78D`) | `E6`, `38` | as row 206 |
| 217 | `1F7B5` (`mov` `1F7B0`) | same | same | as row 215, the column `!= 0x20` | `E6`, `36` | as row 206 |
| 218 | `1FF31` | same, the letter pick | same | not finished, a face-button press (pad bits `0x0F`) or a queued letter (`0x1FCF9..0x1FD28`), the letter not DEL `0x1B`/END `0x1C` (`0x1FE0A`/`0x1FE77`), `NE_NAME_COUNT < NE_NAME_LIMIT` (`0x1FEA3..0x1FEAE`) | `E9` (after any of rows 206-217 in the same call) | as row 206 |
| 219 | `44AFF` (`mov` `44AFA`) | `fighter.c` `fighter_c4_spawn` (static; `0x44A64`'s copy of row 100's body) | `fighter_44a64(rec)` (public; anim code `0x44A64`) | `rec+0x14` slot != 0 (`0x44A72`, the copy of `0x449C6`) | `AB` | none |

### §1.4 Open at the checkpoint

- The three §0.9 decisions are still unanswered: reclassifying `0x1D0BC`,
  retiring the title announcer stand-in, and wiring `fight.c:4007`'s three
  calls.
- No raw-wins correction to §0 was found. §1.1 adds four raw facts. Id 0
  returns AL = 0 before the table. Case 2 returns AL = 0 on its playing arm.
  Case 4 tests `0x180122FD` and queues it with loop byte 1. `0x1E30C` and
  `0x1E458` also read `0x500BB`. `flow.c`'s `sound_voice` already models the
  first three (`0x2C401`, `0x2C48A`, `0x2C8C0..0x2C8D8`). Only §0.1's table
  text lacks them.
- §0.5's 85 are now 75 outside, 1 silent (row 219, batch D4) and 9 wired by
  K11.

---

## §2 Task 2: `0x1D0BC`, the ISR clock pair, the AIL end status

Implemented on branch `k7-k12` at `4cda564`. The raw was re-read from the
fixup-applied image (`$K/dx.py 1D0BC 1D1B0`, `$K/dx.py 1BDF4 1BE30`); both
bodies match §0.7.1/§0.7.2 and §1.1 instruction for instruction. No raw-wins
correction.

### §2.1 What was ported

- **`0x1D0BC` = `sound_buffers_alloc()`** (`flow.c`, in the sound module after
  `sound_music_volume`). The body follows the raw arm by arm:
  - `0x1D0BF`/`0x1D1A9`: a set `DS_000A2CB0` returns AL = 0.
  - `0x1D0CC..0x1D0E6`: the MIDI arm needs `DS_001028C4`, `DS_001028C0` and a
    clear `DS_001028D0`. `0x1D0E8..0x1D0F2` calls `0x1C308(0x41, 0x5100)` and
    `0x1D0F7` stores the result, 0 included. On success `0x61A70` zeroes the
    `0x5100` bytes; on failure `0x1D113`/`0x1D11E`/`0x1D124` zero C4, C0 and CC
    (with `ecx`, which is `[0x1028D0]` = 0 on this path) and `0x62734` prints.
  - `0x1D132..0x1D17D`: with `DS_001028C8` set and slot 0's `+0x10`
    (`DS_00102870`) clear at entry, slot `i` gets `0x8C00` (`i` = 0,
    `0x1D14D`) or `0x6000` (`0x1D154`). `0x1D163` stores the result, 0
    included; the loop stops at a failure (`0x1D16B`), at `i` = 4 (`0x1D174`)
    or at a slot whose buffer is set (`0x1D176..0x1D17D`).
  - `0x1D17F..0x1D195`: `ecx` = 0 (slot 0 got nothing, or its buffer was set
    at entry) prints and zeroes `DS_001028C8`.
  - `0x1D19B`/`0x1D19D`: `DS_000A2CB0` = 1, AL = 1.
  - `PORT:` the two `0x62734` messages (the runtime's printf) are not printed.
    The `0x41` tag in EAX is not passed: `res_alloc` takes the size only.
- **`res_block_alloc(size)`** (`res.c`/`res.h`) exports the port's `0x1C308`
  (the bump allocator `res_alloc`). `0x1C308` stays host-owned.
- **`game_init`** calls `sound_buffers_alloc()` where the old `PORT:` comment
  sat, after `game_audio_init()` (`0x1CF40`), as the raw's `0x1BEC4` does at
  `0x1C0B1` (after `0x1BFDB`). In every run `DS_001028C8` is 1 there
  (`AIL_install_DIG_INI` always returns the port's driver), so the four slots
  get their `0x1AC00` bytes. Every later heap block (the movies'
  `res_load_file`) moves up by `0x1AC00`. The localisation scratch at
  `0x3800000` stays above it (the heap tops out near `0x2BC0000`).
- **`game_isr_ticks(n)`** (`flow.c`/`flow.h`, `PORT:`) adds `n` to
  `DS_00101508` and `DS_00101500` (`0x1BE0E..0x1BE16`). The master loop's spin
  (`0x256C5`) and `res.c`'s read stall call it. `config.c`'s key-wait loop
  already models both counters itself and is unchanged. The ISR's
  `DS_00104B22` gate (`0x1BDF8`) stays unmodelled (todo-verify §1).
- **`AIL_sample_status`** (`ail.c`) reports 2 for a state-4 handle with no
  active mixer voice (`mixer_sample_active(owner)`, new in `mixer.c`/`.h`),
  the port's form of the DIG service's end at `0x6F28F`. Named gap
  (§0.7.6): with no device the mixer is not rendered, so a started sample stays
  4 in `--check` and in the suite.
- **Named gap, newly reachable (§2.6):** the idle timeout of `0x2EB80`. Its
  difference `0x500BB - DS_00105F2C` (`0x2EB8F..0x2EB9F`) now grows with the
  master loop's spin, and `menu_step` (`0x2FFC4`) calls it every frame at
  `0x303D9`: in the service menu (mode `0x27`, `flow.c`'s `0x251DF`) and the
  START MENU (`svc_start_menu`, `0x2CB74`). After `0x4B0` idle ticks (about
  20 s at 60.05 Hz), the raw stores `DS_00107414 = 0` (`0x2EBA8`) and
  longjmps (`0x2EBAE..0x2EBB3`, `jmp 0x65431` with EAX = `0x1044F4`,
  EDX = 1). The port keeps only the store (`config.c`, "PORT: the longjmp
  quit path is not modelled"). `DS_00107414` is `menu_step`'s active byte, so
  the next step re-initialises the menu: it re-stamps `DS_00105F2C`, redraws,
  and resets the selection to 0. This repeats every `0x4B0` idle ticks.
  Before this task `DS_00101500` advanced only inside
  `config_screen_wait`'s loop (and 2 ticks per `menu_step` init), so the
  master-loop arm was unreachable. No oracle driver enters mode `0x27`, and
  the dumps are unchanged. The longjmp is out of scope by ruling;
  `check_idle_timeout_clock` pins the store over the ISR clock.
- **Heap headroom (pre-existing):** `movie_play` loads each movie through
  `res_load_file` on every play, and the bump allocator never frees, so the
  heap creeps toward the localisation scratch at `0x3800000` (`flow.c`'s
  `STRING_HANDLE`). The headroom is about `0xC40000` (12 MB, from about
  `0x2BC0000`). This task's `0x1AC00` shift takes about 0.1 MB of it.
- **Classification:** the row `1D0BC host-owned record-§50-D` is removed from
  `tools/port_classification.txt` (user decision §0.9.1, approved).
- **Comments beyond the brief** (wording only, no code): `flow.c`'s
  sound-module header no longer says the port does not allocate the `0x1D0BC`
  buffers, and names where §0.7 settles §K7.3's three decisions. `res.c`'s
  stall comment now says the ISR advances `DS_00101508` with its clock
  `DS_00101500` but not `DS_0010150C` (it said "`DS_00101508` alone").

### §2.2 Tests

`test_game.c` `check_sound_buffers()`, called in `test_flow` right after
`game_audio_init()`'s `DS_001028C8` check. It saves and restores every global
it seeds except the ISR pair. Each asserted post-value differs from its seed,
or is a seeded value that the mutated code would overwrite.

| vector | seeds | asserted |
|---|---|---|
| already run | `A2CB0` = 1, `C8` = 1, slots = 0 | AL = 0; slot 0 stays 0 |
| first run, DIG, no sequence | `A2CB0` = 0, `C0` = `C4` = 0, `D0` = `0xD0D0D0D0` | AL = 1; `A2CB0` = 1; `C8` = 1; `D0` unchanged; slot 0 != 0; the slot spacings `0x8C00`, `0x6000`, `0x6000` |
| MIDI arm | `A2CB0` = 0, `C0` = `0x1234`, `C4` = `0x5678`, `D0` = 0, `C8` = 0, slots = `0x0BAD` | AL = 1; `D0` != 0; `C0` = `0x1234`; slot 0 = `0x0BAD` |
| slot 0 set at entry | `A2CB0` = 0, `C0` = `C4` = 0, `C8` = 1, slot 0 = `0x0BAD`, slot 1 = 0 | AL = 1; `C8` = 0 (`0x1D195`); slot 1 = 0 |
| ISR pair | `1508` = `0x10`, `1500` = `0x2000`, `game_isr_ticks(3)` | `0x13`, `0x2003` |

Review 1 added four vectors and two peeks. `res_block_alloc(0)` returns the
bump allocator's next block: it aligns the heap and does not advance it.

| vector | seeds | asserted |
|---|---|---|
| first run (added) | as above; peek before | slot 0 = the peek; the next peek − slot 3 = `0x6000` (slot 3's size) |
| C4 half (`0x1D0CC`) | `A2CB0` = 0, `C0` = `0x1234`, `C4` = 0, `D0` = 0, `C8` = 1, slots = 0 | AL = 1; `D0` = 0; slot 3 != 0. Slot 4's `+0x10` is `DS_001028D0`, so this also pins the loop bound `i < 4` (`0x1D171`) |
| D0 half (`0x1D0E6`) | `A2CB0` = 0, `C0` = `0x1234`, `C4` = `0x5678`, `D0` = `0xD0D0D0D0`, `C8` = 0 | AL = 1; `D0` unchanged |
| stop at a set slot (`0x1D176..0x1D17D`) | `A2CB0` = 0, `C0` = `C4` = 0, `C8` = 1, slots 0/1/3 = 0, slot 2 = `0x0BAD` | AL = 1; slot 1 != 0; slot 2 = `0x0BAD`; slot 3 = 0; `C8` = 1 |
| MIDI arm (added) | as above; the peeked block filled with `0xA5` | `D0` = the peek; all `0x5100` bytes 0 (`0x61A70`); the next peek − `D0` = `0x5100` |

`check_idle_timeout_clock()` (`test_game.c`, in `test_flow` after
`check_sound_buffers`) seeds no latched key, `DS_00101500` = `DS_00105F2C` =
`0x7000` and `DS_00107414` = `0x5A`. After `game_isr_ticks(0x4B0)`,
`config_key_latched()` returns 0 and the byte stays `0x5A`. After one more
tick it returns 0 and the byte is 0. It restores all five globals.

`test_title_window` runs in the `PR_TITLE_DUMP` and `PR_ATTRACT_DUMP` drivers,
both in `make verify`. At entry it asserts that `game_init`'s `0x1C0B1` call
ran: `DSB(DS_000A2CB0)` = 1 and slot 0 != 0. The image that `game_init` maps
holds 0 at both, and nothing else writes them. Over its 96 `game_loop`
iterations it counts those that left `DS_00101500` unchanged (expected 0) and
sums the clock's advance (expected >= 96). `DS_00101508` is not compared,
because `0x52106` (`gfx_screen_reset`) stores it, and not `DS_00101500`,
inside the window. A first draft that compared the two deltas measured 1
split iteration.

`test_platform.c`'s res stall seeds `DS_00101500` = `0x9ABC` and asserts it
advances by the same `ceil(res_size(0) / 132674)` as `DS_00101508`.
`test_audio.c`'s `test_ail` asserts status 2 after the mixer finishes a
count-1 sample and status 4 while a count-0 sample loops.

Assertion sites: 13254 → 13276 (+22: 19 in `check_sound_buffers`, 1 in
`test_platform.c`, 2 in `test_audio.c`), then **13299** after review 1 (+23:
15 in `check_sound_buffers`, 4 in `check_idle_timeout_clock`, 4 in
`test_title_window`). The count comes from `rg -o '\bCHECK(_EQ_INT)?\('
port/tests -g '!test.h' | wc -l`.

Before the implementation the build fails: `sound_buffers_alloc` and
`game_isr_ticks` are undeclared (5 errors).

### §2.3 Mutations (measured; FAIL lines exclude the closing `FAILURES: N`)

Re-measured on the review-1 tree. The line numbers are that tree's.

| | mutation | FAIL lines | failing checks |
|---|---|---|---|
| a | slot 0's `0x8C00` → `0x6000` | 1 | `test_game.c:1105` (the `0x8C00` spacing, 24576 != 35840) |
| b | drop `if (i == 0u) DSD(DS_001028C8) = 0;` | 1 | `test_game.c:1180` (slot 0 set at entry, 1 != 0) |
| c | drop `DSD(DS_00101500) += n;` | 3 | `test_platform.c:272` (the `0x9ABC` stall pin), `test_game.c:1188` (`0x2003`), `test_game.c:1219` (the idle timeout) |
| d | drop the `mixer_sample_active` test in `AIL_sample_status` | 1 | `test_audio.c:1502` (4 != 2) |
| e | drop `DSB(DS_000A2CB0) = 1u` | 1 | `test_game.c:1101` |
| f | skip the slot-0 entry gate (`0x1D147`) | 2 | `test_game.c:1180`, `:1181` |
| g | `AIL_sample_status` reports 2 for every state-4 handle | 57 | `test_audio.c:1507` (count 0 still loops) plus 56 existing sound checks |
| h | drop the `DS_001028D0` store (`0x1D0F7`) | 2 | `test_game.c:1160`, `:1163` |
| i | drop the once-gate (`0x1D0BF`) | 4 | `test_game.c:1089`, `:1090`, `:1102`, `:1108` |
| j | drop the `DS_001028C4` test (`0x1D0CC`) | 1 | `test_game.c:1121` (`D0` = 0) |
| k | drop the `DS_001028D0 == 0` test (`0x1D0E6`) | 1 | `test_game.c:1132` (`D0` unchanged) |
| l | loop condition `i < 4u` only (no stop at a set slot) | 2 | `test_game.c:1144`, `:1145` (slots 2 and 3) |
| m | drop the MIDI `memset` (`0x61A70`) | 1 | `test_game.c:1167` (20736 non-zero bytes) |
| n | loop bound `i < 5u` | 1 | `test_game.c:1121` (the loop allocates into `DS_001028D0`) |
| o | the spin back to `DSD(DS_00101508)++` (`PR_TITLE_DUMP` run) | 2 | `test_game.c:6964` (95 iterations with a still clock), `:6965` |
| p | delete `game_init`'s `0x1C0B1` call (`PR_TITLE_DUMP` run) | 2 | `test_game.c:6922`, `:6923` |
| q | drop `config.c`'s idle store (`0x2EBA8`) | 13 | `test_game.c:1219` (the new clock pin), plus 12 existing (`test_game.c:7197`, `:7201`; `test_platform.c:3032..3058`) |

Every mutation was applied alone, rebuilt, run under `PR_ORACLE_REQUIRED=1`
(o and p also with `PR_TITLE_DUMP`) and restored (`$K/t2mut.py`, outputs
`$K/t2-mut-*.txt`). Before review 1, j..m and n survived with 0 FAIL lines.

### §2.4 Not tested

Review 1 closed j..m and n (the loop bound), the `0x1C0B1` call (p) and the
spin (o). What is left:
- The MIDI allocation-failure arm (`0x1D10E..0x1D12F`: C4/C0/CC zeroed) and the
  slot allocation-failure break (`0x1D16B`), slot 0's failure included. The
  bump allocator cannot be made to fail in-process without exhausting `mem[]`.
- Slot 1's and slot 2's `0x6000` sizes are pinned only through the spacings;
  slot 3's is pinned by the peek.
- The idle timeout's re-initialisation, driven through `menu_step` over the
  master-loop clock (§2.6): the store is pinned on `0x2EB80` directly, and
  `test_platform.c`'s `check_menu_step` already pins the store and the
  re-init with a seeded clock. No test runs `0x4B0` `game_loop` iterations
  in a menu mode.
- The ISR gate `DS_00104B22` is not modelled (todo-verify §1).

### §2.5 Gate

- `make verify`: `EXIT=0` (`$K/verify-t2.txt`, 567 lines). The brief's grep
  of the oracle lines diffs empty against `$K/oracle-lines-base.txt`
  (`ORACLES-EQUAL`). No ledger §A line moved.
- Dumps: `dumps.sh before-t2` at `4cda564` matched `$K/base.sha256`, and so
  did `dumps.sh after-t2` (`dumpsha.sh`, `DUMPS-IDENTICAL`). Both dumps are
  deleted.
- `make audio-render`: `before-t2.wav` and `after-t2.wav` are byte-identical
  (`cmp`; sha256 `df74acfb…a380844`, 2386412 bytes).
- `python3 tools/port_progress.py`: `766 1203 64` / `730 731 100` (portable:
  excludes 81). The Task-1 values were `765 1203 64` / `729 730 100`
  (excludes 82): ported +1, portable denominator +1. `README.md`'s title stays
  at 64%, and its portable line now reads 730 of 731.
- `grep -c '/\* 0x1D0BC' port/src/game/flow.c` = 1.
- **Review 1** (tests and docs only, no `port/src` change): `make verify`
  gives `EXIT=0` (`$K/verify-t2r1.txt`) with `ORACLES-EQUAL`.
  `dumps.sh after-t2r1` matches `$K/base.sha256` (`DUMPS-IDENTICAL`), and
  `after-t2r1.wav` is byte-identical to `before-t2.wav`. The counter is
  unchanged.

### §2.6 Correction to §0.7.2 (raw wins): the idle timeout is a master-loop reader

§0.7.2 says the config/menu key timers `0x2EB52..0x2EEF6`/`0x2FFDA` "use
differences, inside their own loops". The raw says otherwise (`$K/dx.py`,
function extents from `prage.functions.csv`):
- `0x2EB52` (`0x2EA78`), `0x2EE06` (`0x2EDE0`), `0x2EEF6` (`0x2EEC8`) and
  `0x2FFDA` (`0x2FFC4`) take no difference. Each is `call 0x500BB; mov
  [0x105F2C],eax`, a stamp of the last key or menu entry. `0x2EDE0` and
  `0x2EEC8` stamp only when a key bit is set (`0x2EE02`/`0x2EEF2 test
  edx,edx; je`).
- `0x2EB8F` (`0x2EB80`) is the only difference: `sub eax,[0x105F2C]; cmp
  eax,0x4B0; jbe 0x2EBB8` (`0x2EB94..0x2EB9F`). Over `0x4B0`, it does
  `mov [0x107414],ah` (0) at `0x2EBA8` and then `mov eax,0x1044F4; jmp 0x65431`
  (`0x2EBAE..0x2EBB3`, EDX = 1), a longjmp.
- `0x2EB80` has no loop of its own. `0x2FFC4` calls it at `0x303D9` on every
  step, and `0x2FFC4` runs once per master-loop frame from mode `0x27`
  (`0x251DF`) and from `0x2CB74` (`svc_start_menu`). So the timeout depends
  on the ISR clock across frames, which this task models (`game_isr_ticks` in
  the spin).
- Also: `0x31F9F..0x320DB` is no longer "unported K11". `svcmenu.c`
  (`0x31F24`'s loop, MODIFY CONTROLS) reads it, inside its own loop over `config_screen_wait`,
  which models the ISR itself.

The consequence is the named gap in §2.1. The port stores `DS_00107414 = 0`
without the longjmp, so a master-loop menu idle for `0x4B0` ticks
re-initialises instead of leaving. By ruling, the longjmp stays out of scope
in this task.

---

## §3 Task 3: `0x1CC28`'s slot choice, `0x1CB18`, `0x1CF20`, the stand-in retired

Implemented on branch `k7-k12` at `811b362`. The raw was re-read from the
fixup-applied image (`$K/dx.py 1CB18 1CDA0`, `1CF20 1CF40`, `11151 11172`,
`121C0 121DA`, `1D0BC 1D0F8`, `500BB 500C1`). Both bodies match §0.7.3/§0.7.4
instruction for instruction. The payload sizes were read from the raw
resource files (a handle is `resource << 23 | offset`, and the GRA files are
the resource bytes as loaded): `0x03837440` (s16title) = `0x1D85`,
`0x0383B6F4` (s16title) = `0x5FAE`, `0x03022554` (s16snd2) = `0x8320`,
`0x180122FD` (s16havsd) = `0x79C0`. The voice records (`DS_000BBDC8`, 12 bytes
each) are `0x3A` = {2, `0x03022554`, 0}, `0x40` = {2, `0x0383B6F4`, 1},
`0x42` = {2, `0x03837440`, 1}, and `0x41`/`0x43` = case 5.

### §3.1 What was ported

- **`0x1CC28` = `snd_sample_queue(h, loop)`** (`flow.c`), arm by arm:
  - `0x1CC37`/`0x1CC44`: no DIG driver, or paused samples, give AL = 0 with no read.
  - `0x1CC51`: `now = 0x500BB()` (`mov eax,[0x101500]`). `0x1CC5D`: the resolve.
    `0x1CC62`: `size = [p]`. `0x1CC5B`/`0x1CC64`: the candidate starts at 0.
  - `0x1CC68 cmp ecx,0x6000; jbe`: above `0x6000`, slot 0 is queued if it is
    free (`0x1CC70` buffer, `0x1CC79` `+0x04`, `0x1CC89`/`0x1CC94` status
    != 4). Otherwise the candidate stays 0 (`0x1CCB8`/`0x1CCBA`).
  - `0x1CCC3..0x1CD32`: at or below `0x6000`, slots 3..0 (`edi` = 3, `esi` =
    `0x48`, `dec edi; sub esi,0x18; jge`). The first free one is queued. A
    slot that is not free becomes the candidate when `min > +0x14`
    (`0x1CD22 cmp ebp,eax; jbe`, unsigned and strict), with `min` starting at
    `now`.
  - `0x1CD34..0x1CD84`, the forced arm: `0x5DC8B` (`AIL_stop_sample`), then
    `0x5DC0F` (`AIL_init_sample`), then the queue. Its `+0x0C` is left as it
    is. `0x1CB18` overwrites it on the start.
  - A queue stores `+0x04` = `h`, `+0x08` = the loop byte, and `+0x14` =
    `0x500BB()` read again at the store (`0x1CCA7`/`0x1CD06`/`0x1CD79`), so a
    loader stall inside the resolve is included. AL = 1.
  - `PORT:` a NULL resolve (a handle past the loaded INDEX, in unit fixtures)
    queues nothing and returns 1. The raw dereferences it.
- **`0x1CB18` = `sound_sample_start(slot)`** (`flow.c`, declared in `flow.h`),
  `0x1CB25..0x1CC16`:
  - `+0x04 == 0` returns.
  - Otherwise it resolves, and copies `[p]` bytes from `p+4` into the slot's
    `+0x10` buffer (`rep movsd`/`movsb`).
  - AIL calls: `AIL_init_sample`, then the address and size, the volume
    `DS_000A2CB4`, the rate `0x2B11` and the type (0, 0).
    `AIL_set_sample_loop_count(0)` only when `+0x08 == 1`
    (`0x1CBD3 cmp eax,1`). Then `AIL_start_sample`.
  - Finally `+0x0C = +0x04` and `+0x04 = 0`.
  - `PORT:` a bufferless slot or a NULL resolve is not started. The raw
    would copy to linear 0.
- **`0x1CF20`** (`game_audio_service`): `for i in 0..3: 0x1CB18(i)`
  (`0x1CF21..0x1CF2E`) runs before the music, and it replaces the stand-in's
  one-slot `game_sample_play`.
- **The stand-in is retired** (§0.7.5, user decision §0.9.2). The following
  are deleted: `game_sample_request`, `game_sample_play`, `s_pending_sample`,
  `s_sample_request`, `SND_ANNOUNCER_ID`, `SOUND_RES` and the `samples.h`
  include (`flow.c` has no other use of any of them).
  - `game_state_title`'s first entry now calls `sound_voice(0x41)` and
    `sound_voice(0x43)` (`0x121C9`/`0x121CE`, `0x121D3`/`0x121D8`), and keeps
    `s_music_request` (a `PORT:` comment, todo-verify §22).
  - `attract_step` phase 2 calls `sound_voice(0x40)` and `sound_voice(0x42)`
    (`0x11156`/`0x11160`, `0x11165`/`0x1116C`).
  - **Correction to the brief (raw wins):** the brief's comments named
    `0x1115B`/`0x11160` and `0x11167`/`0x1116C`. The raw has
    `0x11156 mov eax,0x40`, `0x1115B mov ecx,0x2D`, `0x11160 call`, and then
    `0x11165 mov eax,0x42`, `0x1116A mov bh,3`, `0x1116C call`. The comments
    carry the raw addresses.
- **`main.c`'s `--check` probe** (`attract_loop_playing(h)`) asserts both
  loops. On the last state-0 frame before the title, slots hold `0x0383B6F4`
  and `0x03837440` as playing (`+0x0C`, status 4). At title entry + 2,
  neither does.
  - **Beyond the brief:** the brief probed `0x40` only. With that probe, the
    `0x42` queue and the `0x43` stop were both unproved (mutations q and r
    survived). Review Focus asks for "the attract's two s16title loops play,
    and the title's first entry stops them", so the probe checks both
    handles.
- Sites §0.4 rows 11, 12, 170 and 171 are closed.

### §3.2 Tests

`check_sample_slots()` (`test_game.c`) is called at the end of the rewritten
0x40 block in `test_flow`, on the live handles and on the buffers from that
block's `sound_buffers_alloc()`. It restores the aperture, the DAC, the low
`0x2000` bytes of `mem[]` (vector F) and slot 3's buffer. `ss_seed` sets
`+0x04 = 0`, `+0x08 = 0x77`, `+0x0C = 0` and `+0x14 = 0x10 + i` on each slot,
with `now = 0x100`.

| vector | seeds | asserted |
|---|---|---|
| A | `ss_seed`; `0x42`, then `0x40` | slot 3: `+0x04` = `0x03837440`, `+0x08` = 1, `+0x14` = `0x100`; slot 2 gets `0x0383B6F4`; slot 1 stays 0 |
| A2 | slot 3 playing (`sv_status(3,4)`); then slot 3 bufferless | `0x42` goes to slot 2 both times; slot 3's `+0x04` stays 0 |
| B | A's queue, then `game_audio_service()` | slots 3/2: `+0x0C` = handle, `+0x04` = 0; status 4 (slot 0: 2); the buffer equals `p+4` for `0x1D85` bytes; 2 voices, still 2 after 24×4096 frames (count 0) |
| B2 | slot 1 queued with `0x03837440` and loop byte **2** | 3 voices; after 24 renders 2 again, and slot 1's status is 2 (count 1, `== 1` is exact) |
| C | `0x41`, then `0x43` | slot 2's `+0x0C` = 0, status 2, 1 voice; then 0 voices |
| D | `ss_seed`; `0x3A` twice (`now` `0x100`, then `0x180`) | slot 0: `0x03022554`, loop 0; slot 3 untouched; the forced re-queue stamps `+0x14` = `0x180` |
| D2 | slot 0 playing; `0x3A` | 1 voice before, 0 after (`0x1CD4F` ends it); slot 0 queued |
| E | none free, `+0x14` = {`0x90`, `0x50`, `0x70`, `0x60`}; then all `0x100` (= now) | slot 1 evicted (`+0x14` = `0x100`), slots 3/0 keep their sentinels; all at now evicts slot 0 |
| E2 | ties {`0x90`, `0x50`, `0x70`, `0x50`}; slot 0 at `0x200` and 1..3 at now | slot 3, then slot 0; slot 1 keeps its sentinel in both (a `>=` takes slot 1) |
| F | slot 3 bufferless with `0x03837440` queued, `mem[0]` = `0x5A5A5A5A` | `+0x04` kept, `+0x0C` = 0, `mem[0]` unchanged |

`check_sound_buffers` gains the MIDI gate's C0 half (`0x1D0D5`, Task 2 review
minor): `C4` = `0x5678`, `C0` = 0, `D0` = 0, `C8` = 0 gives AL = 1 and `D0`
still 0.

**Rewritten assertions** (their premise was the stubbed queue or the stand-in):

| where | old | new | raw |
|---|---|---|---|
| `check_sound_voice` E | after `sound_voice(3)` every slot's `+0x04` = 0 | slot 0's `+0x04` = `0x180122FD`; slots 1..3 `+0x04` = 0; all `+0x0C` = 0 | `0x2C8D8` → `0x1CC28`: `0x79C0 > 0x6000`, slot 0 free after `0x1CD9C` → `0x1CC99` |
| `test_flow` (Task-12 block) | the title queued the announcer: a voice is active, the render is non-silent | `sound_buffers_alloc()` = 1, `sound_voice(0x40)` = 1, then the same two facts on `0x40` | `0x11160`, `0x1CC28`, `0x1CF21` → `0x1CB18` |
| `test_flow` (the `0xCD` block) | `0xCD`'s handle `0x02824B0F`; 0 voices after 24 renders (count 1) | `0x40`'s handle `0x0383B6F4`; 1 voice after 32 renders (24 until §4.6: a one-shot `0x40` outlives 24, so 24 did not prove count 0); `sound_voice(0x41)` = 1; 0 voices | `0x1CBE1` (loop byte 1 → count 0), `0x2C7A5` → `0x1CE04` |
| `main.c` `--check` | at title + 2 the announcer voice is active and non-silent | on the last attract frame, `0x0383B6F4` and `0x03837440` play; at title + 2, neither does | `0x11160`/`0x1116C`, `0x121CE`/`0x121D8` |

Assertion sites: 13299 → **13364** (+65 = 69 added − 4 removed): 58 in
`check_sample_slots`, 2 for the C0 half, +2 and +2 in the two `test_flow`
blocks, and +1 in section E (`rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g
'!test.h' | wc -l`).

### §3.3 Mutations (measured; FAIL lines exclude the closing `FAILURES: N`)

`$K/t3mut.py` applies each mutation alone, rebuilds and runs the suite under
`PR_ORACLE_REQUIRED=1`. For e, f, q and r it also runs `--check 820`.
Outputs: `$K/t3-mut-*.txt`, summary `$K/t3mut-summary.txt`. d and d2 were
re-measured through a pty (`$K/ptyrun.py`, `$K/t3-mut-d*-pty.txt`) after
vector F was hardened (below). The line numbers are the final tree's.

| | mutation | FAIL lines | failing checks |
|---|---|---:|---|
| s | the queue stubbed (resolve only, the pre-implementation state) | 29 | A, B, C, D, E, the `test_flow` block (`:1566`, `:1572`, `:1590`) |
| a | `min > t` → `min >= t` | 4 | E2 (`:1049`, `:1050`, both ties) |
| b | the scan runs 0..3 | 16 | A (`:928`..`:934`), A2, B |
| c | the loop-byte test and the count-0 call deleted | 5 | B `:969` (1 voice left), B2, C |
| c2 | the loop count 0 set unconditionally | 4 | B2 `:979`/`:980`, C |
| c3 | the loop-byte test `== 1` → `!= 0` | 4 | as c2 |
| d | the `buf == 0 \|\| p == NULL` guard deleted | 3 | F `:1067`, `:1068`, `:1069` |
| d2 | only the `buf == 0` half dropped | 3 | as d |
| e | `game_state_title`'s `sound_voice(0x41u)` deleted | suite 0; `--check` exit 1 | "the title did not stop the attract's loop 0x0383B6F4" |
| f | `attract.c`'s `sound_voice(0x40u)` deleted | suite 0; `--check` exit 1 | "the attract's loop 0x0383B6F4 was not playing before the title" |
| q | `attract.c`'s `sound_voice(0x42u)` deleted | suite 0; `--check` exit 1 | "... loop 0x03837440 was not playing before the title" |
| r | `game_state_title`'s `sound_voice(0x43u)` deleted | suite 0; `--check` exit 1 | "the title did not stop the attract's loop 0x03837440" |
| g | the C0 half of the MIDI gate dropped (`0x1D0D5`) | 1 | `:1317` (`D0` allocated) |
| h | the slot-0 arm's status test dropped (`0x1CC94`) | 1 | D2 `:1006` |
| i | the forced arm's `AIL_stop_sample` dropped (`0x1CD4F`) | 1 | D2 `:1006` |
| j | the scan's status test dropped (`0x1CCF1`) | 2 | A2 `:942`, `:943` |
| k | the scan's buffer test dropped (`0x1CCD4`) | 2 | A2 `:947`, `:948` |
| l | the forced arm's `+0x14` store dropped (`0x1CD7E`) | 2 | D `:999`, E `:1021` |
| m | the size threshold `0x6000` → `0x9000` | 8 | D, D2, and `check_sound_voice` E (`:771`, `:773`) |
| n | `0x1CB18`'s `+0x04 = 0` dropped | 1 | B `:958` |
| o | the copy dropped | 1 | B `:965` |
| p | `0x1CF20`'s per-slot loop dropped | 13 | the `test_flow` block and B..C |

**Corrections to the brief's mutation expectations (measured):**
- **(a)** The brief said `>=` fails E's "all at now" case. It does not. With
  every `+0x14` equal and the scan ending at slot 0, `>=` also leaves the
  candidate at 0. The first run measured 0 FAIL lines. Vector E2 was added,
  which separates the two by a tie and by slot 0 above now.
- **(d)** The first run measured 0 FAIL lines and a SIGBUS. F's three
  failures were printed but lost in the crashed process's pipe buffer. The
  crash came from the mutated copy of `0x1D85` bytes over `mem[0..]`, which
  outlived the vector. F now snapshots and restores the low `0x2000` bytes
  and stops slot 3's voice. The re-measure gives 3 FAIL lines and `rc` = 1.

### §3.4 Not tested

- The size boundary itself: no shipped case-2 payload is exactly `0x6000`
  bytes (§0.7.1, the 13 above `0x6000` go to slot 0 only). A mutation from
  `>` to `>=` at `0x1CC68` has no vector.
- The slot-0 arm's buffer test (`0x1CC70`): slot 0 without a buffer and a
  large sample. The forced arm also lands on slot 0 in that case, so the only
  difference is the forced `0x5DC8B`/`0x5DC0F` on an idle handle, which is not
  observable.
- The forced arm on a status-4 slot in the small-sample scan: the forced stop
  is pinned only through the large arm (D2), where the candidate is always 0.
- `0x1CB18`'s volume and rate calls (`DS_000A2CB4`, `0x2B11`) and the type
  (0, 0): no assertion reads them back. The rate is also AIL_init's default.
- ~~The loader stall inside `0x1CC28`'s resolve making `+0x14` later than
  `now` (the second `0x500BB` read).~~ Closed by §4.6 for the large arm
  (`0x1CCA7`), in the suite's order only; the scan's and the forced arm's
  second reads (`0x1CD06`, `0x1CD79`) stay untested.
- The mixer's end status in a windowed run (§0.7.6 named gap). `--check` and
  the suite render no device, so a started one-shot stays 4.

### §3.5 Gate

- `make verify`: `EXIT=0` (`$K/verify-t3.txt`, 567 lines). The brief's grep
  of the oracle lines diffs empty against `$K/oracle-lines-base.txt`
  (`ORACLES-EQUAL`). No ledger §A line moved, and the rewritten `--check 820`
  probe passes inside it.
- Dumps: `dumps.sh after-t3` matches `$K/base.sha256` (`dumpsha.sh`,
  `DUMPS-IDENTICAL`). `--check 8000`, the fe det driver, the attract dump and
  the title dump all report no failure. The dump is deleted. No before-dump
  was taken: `811b362` was already proven equal to `base.sha256` (§2.5).
- `make audio-render`: `after-t3.wav` is byte-identical to `before-t2.wav`
  (`cmp`; sha256 `df74acfb…a380844`, 2386412 bytes).
- `python3 tools/port_progress.py`: `767 1203 64` / `731 731 100` (portable:
  excludes 81). After Task 2 it was `766 1203 64` / `730 731 100`, so ported
  +1 (`0x1CB18`; `0x1CC28` already had its header). `--unported | grep 1CB18`
  prints nothing. The README title stays at 64%, and its portable line now
  reads 731 of 731.
- `grep -c '/\* 0x1CB18' port/src/game/flow.c` = 1, and `rg
  'game_sample_play|game_sample_request|s_pending_sample|SND_ANNOUNCER_ID'
  port/src` is empty.

---

## §4 Task 4: K12 batch A, the voice log and the site runner

Implemented on branch `k7-k12` at `8d68a12`. Every batch-A call was re-read
from the fixup-applied image (`$K/dx.py`), `mov` and `call` both; each C call
carries the pair `/* 0xMOV/0xCALL 0x2C3FC */`.

### §4.1 The seam (the API Tasks 5-11 consume)

`flow.c`/`flow.h`, marked `PORT:` (a test seam, not original state; the
pattern of `res.c`'s `res_set_screen_hook`):

```c
void sound_voice_log_reset(void);   /* count = 0 */
u32  sound_voice_log_count(void);   /* entries since the reset; counts past 16 */
u32  sound_voice_log_at(u32 i);     /* the i-th id, or 0xFFFFFFFF past the count or 16 */
```

`sound_voice` records its argument as its first statement, before the id-0
return and the `0x100` → 0 fold, so the log holds the id the caller passed
(`0x100`, not 0). The first 16 are kept (`SOUND_VOICE_LOG_CAP`). Nothing in
the port reads the log except the tests, so it moves no oracle.

### §4.2 The runner

`test_fixtures.{h,c}`:

```c
typedef struct { u32 row; void (*drive)(void); u32 n; u32 ids[4]; } TfVoiceSite;
void tf_voice_sites(const TfVoiceSite *t, u32 count);
```

For each row it snapshots the data object (`0x80000`, `0x8B0D0` bytes), the
two actor pools that `DS_001014EC`/`DS_001014F4` name at the runner's entry
(`0x4880`/`0xEBA0` bytes; skipped when 0), the aperture and the DAC. It sets
`DS_001028C8` = 0 (no DIG driver: `0x1CE78` and `0x1CC37` return before any
bank read, so a case-2/3/4 id is logged without a resolve), resets the log,
takes the bump heap's next block (`res_block_alloc(0)`, which does not
advance it) and runs `drive()`. It then checks three things (review 1 added
the first two):
- the heap is where it was (`test_fixtures.c:217`; a miss prints `voice site
  row R: the bump heap grew by 0xN`);
- the row logged at most `SOUND_VOICE_LOG_CAP` (16, now in `flow.h`) voices
  (`:225`; `voice site row R: N voices logged, over the cap 16`), because
  past the cap the log drops ids;
- `ids[0..n)` is an **in-order subsequence** of the log (`:232`
  `CHECK_EQ_INT(j, n)`; `voice site row R: j of n ids in order`).

Then everything snapshotted is put back.

**The subsequence match (verified against the runner code).** The loop walks
the log once, `for (i = 0; i < count && j < n; i++) if (log[i] == ids[j])
j++;`: a log entry that is not the next wanted id is skipped, never counted
against the row. So voices a later batch wires on the same path, before,
between or after a row's ids, do not break the row, as long as the row still
logs at most 16 voices (the cap check makes an overflow a clear failure, not
a silent miss). One limit: the greedy match cannot tell two calls with the
same id apart. If a later batch wires a second call with a row's id on the
same path, before the row's own call, deleting the row's call would still
pass. Batch A's paths have no such pair among the ids §0.4 lists for them
(`4367C`: `100`, `2E`, `30`; `1EEC0`: `100`, `E1`; `26D4C`: `29`, `22`;
`25AE8`: `100`, `3D`), but a batch that adds one must say so in its record.

**Consumer rules for Tasks 5-10** (each batch works in its own worktree):
- `row` is the §0.4 row. There is one entry per wiring point. Two rows on
  one path get one entry each, with the same driver (139/140).
- `ids` lists only **wired** ids on the path, in raw order.
- **A later batch does not edit an earlier batch's rows.** A voice it wires
  on an earlier row's path is logged but needs no change there (the
  subsequence match above), which keeps the parallel batches' edits apart.
- Each batch adds its own `vs_*` drivers, its own `static const TfVoiceSite
  k12_<b>[]`, a local `#define K12_<B>_ROWS` with a `CHECK_EQ_INT` on the
  table's size, and one `tf_voice_sites` call inside `test_voice_sites()`
  (`test_game.c`, registered after `test_key_loop`).
- `drive()` reaches the site from a public entry (§1.3's column) with seeded
  `mem[]`. **Every scratch byte a driver reads is seeded by that driver**
  (`FIGHT_*`, `ANIM_*`, `RA_POOL`/`RA_PSET` through `vs_pools()`, `VS_MENU`,
  `MT_LAYOUT`; batch A's drivers leave these changed, and no driver may rely
  on what an earlier row left there).
- **No bump-heap (`g_heap`) growth across a row** (the runner checks it). A
  loader that allocates (a movie, `res_load_file`) must be kept off.
- **Restore any C static you change.** `vs_attract0` clears the attract
  media dir and sets it back to `test_flow`'s `"data/game/C"`.
- `test_voice_sites()` (`test_game.c`, registered after `test_key_loop`)
  holds one table per batch: `static const TfVoiceSite k12_<b>[]`, a local
  `#define K12_<B>_ROWS` with a `CHECK_EQ_INT` on the table's size, and one
  `tf_voice_sites` call. Shared helpers: `vs_pools()` (the private pool
  `RA_POOL`/`RA_PSET`, then `actors_reset`) and `vs_state(s)`
  (`game_state_step`'s other gates closed: `DS_00104B1D` = 1, `DS_000F0A71`
  = 1, `DS_0009AD58` = 1, then `DS_000F0A64` = `s`).

### §4.3 The batch-A rows

22 entries, `K12_A_ROWS` = 22. All ids are case 0, 5 or 6: pure state.

| row | call (mov/call) | C site | driver | seed | ids |
|---:|---|---|---|---|---|
| 8 | `10E01/10E06` | `attract.c` `frontend_pause_tail` | `vs_pause_tail` | `F0A71` = 0, `1088D8` byte 3 = `0x20`, byte 1 = `0x10`, `104B19+2` = 1 | `100` |
| 9 | `10E69/10E6E` | `attract.c` `frontend_continue_tail` | `vs_continue_tail` | as row 8 with the bits swapped | `100` |
| 10 | `1101F/11024` | `attract.c` `attract_step` case 0 | `vs_attract0` | `vs_pools`, `F0A6F` = 0, media dir NULL | `100` |
| 27 | `44552/44557` | `fight.c` `fight_4454c` | `vs_hook_4367c_3` | `vs_pools`, `104B1D` = 3 | `100` |
| 29 | `43691/43696` | `fight.c` `fight_hook_4367c` | `vs_hook_4367c` | `vs_pools`, `104B1D` = 0 | `100` |
| 44 | `49FE9/49FF1` | `fight.c` `fight_effects_pass` case 7 | `vs_effects_7` | `tf_demo_fixture`; one entry of type 7, `+0x1C` = 0, its actor's `+0x3C` = `0x1000` | `DE` |
| 139 | `257A8/257AD` | `flow.c` `game_coin_divert` | `vs_coin_divert` | `vs_pools`, players 1 | `100`, `53` |
| 140 | `2581B/25820` | same | same | same | `100`, `53` |
| 141 | `28DB3/28DCA` | `flow.c` `flow_player_join` | `vs_player_join` | `vs_pools`, the strings, side 0 | `100` |
| 166 | `25AEC/25AF1` | `flow.c` `game_hook_25ae8` | `vs_hook_25ae8` | `vs_pools`, the strings | `100` |
| 172 | `1159A/1159F` | `flow.c` `game_state_4` phase 0 | `vs_state4_0` | `vs_pools`, `vs_state(4)`, `9AD98` = 0 | `100` |
| 173 | `116BF/116C4` | same, phase 1 | `vs_state4_1` | `9AD98` = 1 | `100` |
| 174 | `1183F/11844` | same, phase 2 | `vs_state4_2` | `9AD98` = 2 | `100` |
| 175 | `11A8F/11A94` | `flow.c` `game_state_6` | `vs_state6` | `tf_demo_fixture`, `vs_pools`, `vs_state(6)` | `100` |
| 176 | `1EEEF/1EEF4` | `flow.c` `game_mode_1e_step` case 0 | `vs_mode1e_0` | `ra_env` (the fresh-CMOS table), `104B25` = 0, `104AD4` = 2, `104B1F` = 0, scores 999999/450000 | `100` |
| 178 | `1F1E7/1F1EC` | same, case 4 | `vs_mode1e_4` | `ra_env`, `104B25` = 4, `107813` = 0, `1077EC` = 999999, `104AD4` = 1 | `100` |
| 182 | `1F2FC/1F307` | same, case 7 | `vs_mode1e_7` | `ra_env`, `104B25` = 7, `1078A7` = 0, `107880` = 999999, `104AD4` = 0 | `100` |
| 191 | `26DA4/26DA9` | `flow.c` `flow_26d4c` | `vs_mode22` | `tf_demo_fixture`, `vs_pools`, `104B1A` = 0, `104AC4` = 0, `104529` = 2 (`check_mode_22`'s arm) | `29` |
| 194 | `11DE8/11DF2` | `flow.c` `game_state_step` case 5 | `vs_state5` | `ra_env`, `vs_state(5)` | `100` |
| 195 | `11BEB/11BF0` (tail `jmp`) | same, case 7's timer exit | `vs_state7_exit` | `vs_state(7)`, `F0A6A` = 1, `F0A6C` = 0 | `100` |
| 196 | `2FA66/2FA6D` | `menu.c` `menu_run` | `vs_menu_run` | `vs_pools`, a zeroed table at `VS_MENU`, `101514` = `MT_LAYOUT`, ESC pressed, flags 0 | `100` |
| 197 | `2FFF6/2FFFD` | `menu.c` `menu_step` | `vs_menu_step` | `vs_pools`, the zeroed table, `107414` = 0 | `100` |

The call sites keep the rest of their comments. The split-phrase rows keep
their unwired half as a split phrase: 176/178/182 now read `PORT: 0x1EEF9/
0x1EEFE 0x2C3FC voice (0xE1), not\n * wired` (and `0x1F1F1/0x1F1F6`,
`0x1F30C/0x1F311`), and row 191's comment keeps `0x26DB8 0x2C3FC(0x22,
edx=0x1D) voice, not wired` for batch B2. Row 44's `0xDE` is case 6: the
dispatcher returns AL = 0 and writes nothing, and the call is still made.
Row 194's `0x11DED mov ecx,0x12C` is kept as a note: `0x2C3FC`'s own body
(`0x2C3FC..0x2C8F0`) never names ECX.

**Corrections to the brief (raw wins):**
- Row 196: the brief's example cites `0x2FA68/0x2FA6D`. The raw is `0x2FA66
  mov eax,0x100; 0x2FA6B xor esi,esi; 0x2FA6D call`. The comment carries
  `0x2FA66/0x2FA6D`.
- Rows 176/178/182: the old comments cited `0x1EEEF/0x1EEF9` (two `mov`s)
  and `0x1F1EC/0x1F1F6`, `0x1F307/0x1F311` (two calls). Each now names its
  `mov`/`call` pair: `0x1EEEF/0x1EEF4` + `0x1EEF9/0x1EEFE`, `0x1F1E7/0x1F1EC`
  + `0x1F1F1/0x1F1F6`, `0x1F2FC/0x1F307` + `0x1F30C/0x1F311`. In row 182 the
  raw's `0x1F301` store of `DS_00104B25` sits between the `mov` and the
  `call`; the port stores first, which is the same state at the call.
- The brief's `K12_A_ROWS` example let rows 139/140 share one entry. The
  table has one entry per wiring point (22), so rows 139 and 140 each have
  their own entry by that rule alone. The two entries run the same path and
  list the same ids, so deleting either call fails both (§4.5).

**`not wired` lines** (`rg -c 'not wired' port/src`), before → after:
`attract.c` 7 → 4, `fight.c` 36 → 33, `flow.c` 46 → 36, `menu.c` 2 → 0 (the
file drops out of the list); the other files are unchanged. Total 187 → 169.
`attract.c`'s 7 (not §1.0's 8) is Task 3's wiring of rows 11/12.

### §4.4 Tests

`test_voice_sites` (`test_game.c`): the size check and the 22-row table.
Before the wiring the suite printed 22 `voice site row` lines and
`FAILURES: 22`. After it: `all checks passed`.

Assertion sites: 13364 → **13370** (+6: 1 in `tf_voice_sites`, 1 in
`test_voice_sites`, and 4 from the §4.6 fixes).

### §4.5 Mutations (measured; FAIL lines exclude the closing `FAILURES: N`)

The plan asks for the first, middle and last rows. Every one of the 22 was
measured instead (`$K/t4mut.py`: replace the one `sound_voice` call with
`(void)0;`, rebuild, run under `PR_ORACLE_REQUIRED=1`, restore; outputs
`$K/t4-mut-<row>.txt`, summary `$K/t4mut-summary.txt`).

The line `test_fixtures.c:215` below is the subsequence check's line at
`a011cb1`; after review 1 it is `:232`.

| deleted call | FAIL lines | printed |
|---|---:|---|
| row 8 (`0x10E06`), first | 1 | `test_fixtures.c:215: 0 != 1`; `voice site row 8: 0 of 1` |
| rows 9, 10, 27, 29, 44, 141, 166, 172, 173, 174, 176, 178, 182, 191, 194, 195, 196 | 1 each | the named row only, `0 of 1` |
| row 139 (`0x257AD`) | 2 | rows 139 and 140, `0 of 2` |
| row 140 (`0x25820`) | 2 | rows 139 and 140, `1 of 2` |
| row 175 (`0x11A94`), middle | 1 | `test_fixtures.c:215: 0 != 1`; `voice site row 175: 0 of 1` |
| row 197 (`0x2FFFD`), last | 1 | `test_fixtures.c:215: 0 != 1`; `voice site row 197: 0 of 1` |

No mutation failed a row other than its own (139/140 share a path and both
list both ids).

### §4.6 Task 3 review minors

1. **The `test_flow` `0x40` loop pin did not prove loop count 0.** The block
   rendered 4096 + 24 × 4096 = 102,400 frames, but `0x5FAE` frames at
   `0x2B11` Hz last about 110,450 frames at 49716 Hz, so a one-shot was still
   live. Measured with mutation c (the loop-byte test and the count-0 call
   deleted): with 24 or 25 renders the line passes, with 26 it fails. The
   loop is now 32 renders, and c fails the line (`test_game.c:1626: 0 != 1`,
   6 FAIL lines in all, 5 before). §3.2's row is corrected.
2. **Two surviving Task 3 mutations, now tested:**
   - Dropping the large arm's free-slot `+0x14` store (`0x1CCA7/0x1CCAC`):
     vector D asserts slot 0's `+0x14` (seeded `0x10`) equals the clock
     after the call. Measured: 1 FAIL line (`test_game.c:1001: 16 != 258`).
     In the suite's order this first `0x3A` resolve reads s16snd2 for the
     first time and stalls 2 ticks, so the stored value is `0x102`, the
     second `0x500BB` read. Storing the entry's `now` instead also fails
     (`256 != 258`).
   - A signed compare in the scan (`0x1CD22 jbe`): vector E3 has now =
     `0x90000000` and `+0x14` = {`0x88000000`, `0x40`, `0x50`, `0x60`}.
     Unsigned takes slot 1; signed would take slot 0. Measured: 2 FAIL lines
     (`test_game.c:1074`, `:1075`).
3. **Stale references:** `flow.c`'s `DS_000A2CB4` comment ("playing the
   announcer at the zeroed mem[] value") now says "the samples"; the
   Makefile's `verify_frames` comment names the "attract/title sample and
   music assertions". `main.c`'s `#include "platform/audio/mixer.h"` is
   **not** unused (`MIXER_OPL_RATE`, `main.c:30`) and stays. Follow-up only:
   `samples_load` (`platform/audio/samples.c`) has no production caller; the
   suite alone calls it.
4. `check_sample_slots` now snapshots the four slot records
   (`DS_00102860`, `0x60` bytes), `DS_00101500`, `DS_001028C8` and
   `DS_001028DB` at entry and puts them back after its closing `ss_seed()`,
   so later tests no longer inherit loop bytes `0x77`, queue times
   `0x10 + i` and now = `0x100`.
5. `docs/PROGRESS.md`'s K7 task 3 paragraph: the evicted slot is the last
   one, scanning from 3 down, whose queue time is strictly below now and
   below every slot scanned before it (it said "below every other slot's",
   which contradicted "a tie keeps the first slot met").

### §4.7 Not tested

- The dispatcher's effect at these sites. The runner runs with
  `DS_001028C8` = 0, so a `0x100` stop-all clears only the music words
  (`DS_001028D4`/`D9`); the sample slots stay as they were. The case-5 and
  case-0/6 bodies themselves are pinned by `check_sound_voice` (§45-A).
- Each call's position relative to its neighbours, except 139 before 140
  (the in-order match). A call moved later in its function still passes.
- Row 139/140's other entry, `game_state_step`'s coin arm (`0x11D41`), and
  state 8 (`0x11EB8`). The test calls `game_coin_divert` directly.
- The unwired voices on the same paths (rows 19, 28, 30, 167, 177, 179,
  183, 192): their batches (B2) add them to these rows.

### §4.8 Gate

- `make verify`: `EXIT=0` (`$K/verify-t4.txt`, 571 lines). The brief's grep
  of the oracle lines diffs empty against `$K/oracle-lines-base.txt`
  (`ORACLES-EQUAL`). No ledger §A line moved. Batch A's nine oracle-path
  points (rows 8-10, 139, 140, 172, 175, 194, 195) are all state-only.
- Dumps: `dumps.sh after-t4` matches `$K/base.sha256` (`dumpsha.sh`,
  `DUMPS-IDENTICAL`; `check/frames` 24000, `fe/run1` and `fe/run2` 1384
  each, all three drivers `all checks passed`). No before-dump was taken:
  `8d68a12` was already proven equal to `base.sha256` (§3.5). The dump is
  deleted.
- `make audio-render`: `after-t4.wav` is byte-identical to `before-t2.wav`
  (`cmp`; sha256 `df74acfb…a380844`, 2386412 bytes).
- `python3 tools/port_progress.py`: `767 1203 64` / `731 731 100`,
  unchanged (no function is ported; the README stays as it is).
- The `not wired` count: §4.3.

### §4.9 Review 1

- `docs/PROGRESS.md`: 19 `0x100` stop-alls, not 20 (19 + `0x53` + `0x29` +
  `0xDE` = 22).
- §4.2 rewritten: the consumer rules for Tasks 5-10 (every scratch byte a
  driver reads is seeded by it, no bump-heap growth across a row, C statics
  restored, and later batches do not edit earlier rows), with the
  subsequence match verified against the runner loop and its one limit
  (same-id aliasing).
- `tf_voice_sites` gains two checks: the log count is at most
  `SOUND_VOICE_LOG_CAP` (moved from `flow.c` to `flow.h` so the fixture can
  name it), and the bump heap (`res_block_alloc(0)`) does not move across a
  row. The heap check is beyond the review's list: it makes the "no heap
  growth" rule enforced rather than stated. Measured
  (`$K/t4r1mut.py`, `$K/t4r1-mut-*.txt`; FAIL lines exclude `FAILURES: N`):

  | mutation | FAIL lines | failing checks |
  |---|---:|---|
  | cap `16u` → `1u` | 4 | `test_fixtures.c:225` and `:232` (`1 != 2`) for rows 139 and 140, the only batch-A rows that log two voices (`2 voices logged, over the cap 1`) |
  | `vs_attract0` keeps the media dir (the movies load) | 1 | `test_fixtures.c:217` (`voice site row 10: the bump heap grew by 0x12EA48`) |

- §4.3: rows 139/140 each have an entry by the one-entry-per-wiring-point
  rule alone; deleting either call fails both.
- Comments: `test_fixtures.h` (ids are the wired voices only),
  `test_fixtures.c`'s header (`tf_voice_sites` is new, not a moved body),
  `test_audio.c`'s AIL header (the sample path is record k7-k12 §3's, not
  "the announcer sample is Task 12's").
- Assertion sites: 13370 → **13372** (the two runner checks).
- Gate (ruling: no full `make verify` for this round): `make build` with 0
  warnings, `PR_ORACLE_REQUIRED=1 ./build/run_tests` gives `all checks
  passed`, and `./build/prageport --game-dir data/game/C --check 820` exits 0.

---

## §6 Task 6: K12 batch B2, the remaining pure-state voices

Implemented in the worktree `k12-t6` (branch `k12-t6`, from `9f15a6d`). Every
batch-B2 call was re-read from the fixup-applied image (`$K/dx.py`), `mov`
and `call` both; each C call carries the pair `/* 0xMOV/0xCALL 0x2C3FC */`.
All 30 ids are case 1 (the song words `DS_00105D5C`/`DS_001028D4`/`D9`) or
case 5 (a stop that tests `DS_00105D5C`), read from `0xBBDC8` at the table
offsets below: pure state, no bank read, no rng, no render.

### §6.1 The 30 wiring points

| row | call (mov/call) | C site | driver | seed | ids (case) |
|---:|---|---|---|---|---|
| 4 | `490DC/490E1` | `actors.c` `actor_type_2d_update` phase 1, far | `vf_2d_far` | `vf_pools`; one node on `DS_00108368` at phase 1 (`0x48FB7`), its record `0x2000` from slot `DS_001078FD` = 0's (`> 0x1000`, `0x4909C`), `DS_00108397` = 1 so `n` = 0 (`0x490D8`); `fn_resolve(0x48F98)` | `F1` (5) |
| 5 | `3D784/3D789` (tail `jmp`) | `actors.c` `actor_type_3D784` | `vf_3d784` | `fn_resolve(0x3D784)(rec)`, none | `4F` (5) |
| 13 | `111F3`/`111FA` `/111FF` | `attract.c` `attract_step` case 4 | `vs_attract_4` | `vs_pools`, `DS_0009AD58` = 1 (the `0x10F28` tail off); case 3 spawns the actor into `DS_000F0A50` (`0x111AF`); then twice case 4 with `+0x2C` = `0x1100`: `DS_000F0A5C` = 0, then 1 | `54` (1), `56` (1) |
| 18 | `4373C/43741` | `fight.c` `fight_char_screen_open` | `vf_hook_4367c` | `vf_pools`, `DS_00104B1F` = 0, `DS_00108173` = 0, `DS_00104B1D` = 0 | `100`, `2E`, `30` (1) |
| 19 | `444CC/444D1` | `fight.c` `fight_char_screen_open_both` | `vf_hook_4367c_3` | as row 18 with `DS_00104B1D` = 3 | `100`, `2E`, `30` (1) |
| 24 | `430ED/430F2` | `fight.c` `fight_hook_430e8` | `vf_hook_430e8` | `vf_pools`, `check_mode_1a_hooks` (k2): `DS_00104B17` = 2, `104B1D` = 0, `104B1F` = 3, characters 0/6, `DS_00108173` = 1 | `31` (1), `2D`, `2F` |
| 25 | `43272/4328A` | same, tail | same | same | `31`, `2D` (5), `2F` |
| 26 | `4328F/43294` | same, tail | same | same | `31`, `2D`, `2F` (5) |
| 28 | `44621/44626` | `fight.c` `fight_4454c` | `vf_hook_4367c_3` | as row 19 | `100`, `2E` (1), `30` |
| 30 | `43724/43729` | `fight.c` `fight_hook_4367c` | `vf_hook_4367c` | as row 18 | `100`, `2E` (1), `30` |
| 31 | `4147C/41483` | `fight.c` `fight_hook_4142c` | `vf_hook_4142c` | `vf_pools` | `32` (1) |
| 64 | `353BB/353C0` | `fighter.c` `fighter_state_3531c` | `vf_3531c` | `DS_001077A8[0]` = slot 0, its record a zeroed scratch record (`0x3539A`), `DS_001078F6` = 1 (`0x353B9 jg`), `DS_00104529` = 0 (no `0x2B150`), `+0x53` = 4 | `EC` (1), `E0` |
| 65 | `353C5/353CA` | same | same | same | `EC`, `E0` (5) |
| 164 | `42E7A/42E7F` | `flow.c` `flow_challenge_poll` | `vs_challenge_poll` | `vs_pools`, `DS_00104AD4` = 0, `DS_001088E4` = 0 (`0x42F60` returns 0), `DS_00105C04` = 0, `DS_00104B1F` = 0, `DS_000EF6DC` = `0x40`, `DS_00108110` = 0 (`0x42E37`), `DS_00107813` = 0 (`0x42E65`), `DS_00104B1D` = 1 | `2D` (5) |
| 165 | `42508/4250D` | `flow.c` `game_mode_13_step` case 0 | `vs_mode13_0` | `vs_pools`, `DS_00104AD4` = 3 with `DS_001080F8` a zeroed scratch record, `DS_00104B1D` = `DS_00104B1F` = 3 (`check_mode_13_c` (j)) | `2C` (1) |
| 167 | `25B4C/25B51` | `flow.c` `game_hook_25ae8` | `vs_hook_25ae8` (batch A's) | batch A's | `100`, `3D` (1) |
| 169 | `2781A/27821` | `flow.c` `game_mode_0f_step` | `vs_mode0f` | `tf_demo_fixture`, `DS_001077A8[0]` a zeroed scratch record, `DS_0010810D` = 0, `DS_00104AD4` = 0, `DS_00104AFE` = 1 (`0x27811`) | `2B` (5) |
| 177 | `1EEF9/1EEFE` | `flow.c` `game_mode_1e_step` case 0 | `vs_mode1e_0` (batch A's) | batch A's | `100`, `E1` (1) |
| 179 | `1F1F1/1F1F6` | same, case 4 | `vs_mode1e_4` (batch A's) | batch A's | `100`, `E1` (1) |
| 180 | `1F23D/1F249` | same, case 5 | `vs_mode1e_5` | `vs_mode1e_done(5)`: `ra_env`, `nameentry_reset` (the cursor actors, every cell idle, so `0x1FFD0` leaves `DS_001044AC` = 0), `DS_0010438C` = 0 (`0x1F4AD`), rank `DS_001044D6` = 2, 3 letters, characters 0 | `E3` (1), `E2` |
| 181 | `1F24E/1F253` | same | same | same | `E3`, `E2` (5) |
| 183 | `1F30C/1F311` | same, case 7 | `vs_mode1e_7` (batch A's) | batch A's | `100`, `E1` (1) |
| 184 | `1F360/1F36C` | same, case 8 | `vs_mode1e_8` | `vs_mode1e_done(8)` | `E3` (1), `E2` |
| 185 | `1F371/1F376` | same | same | same | `E3`, `E2` (5) |
| 186 | `1EF5F/1EF8D`, `1F074/1F07F`, `1F0FE/1F109`, `1EFF4/1F01F` | same, cases `0xB..0xE` (one port path) | `vs_mode1e_bc` | `vs_mode1e_done(0xB)` then `(0xC)` | `E3`, `E2`, `E3`, `E2` |
| 187 | `1EF92/1EF97`, `1F084/1F089`, `1F10E/1F113`, `1F024/1F029` | same | `vs_mode1e_de` | `vs_mode1e_done(0xD)` then `(0xE)` | `E3`, `E2`, `E3`, `E2` |
| 188 | `1EFA2/1EFA9` | same, case `0xF` | `vs_mode1e_0f` | `ra_env`, scores 999999/450000 (`ra_check_rearm`) | `E1` (1) |
| 189 | `1F094/1F099` | same, case `0x10` | `vs_mode1e_10` | `ra_env`, scores 150000/999999 | `E1` (1) |
| 190 | `2094E/20955` | `flow.c` `game_mode_1f_step` case 0 | `vs_mode1f_0` | `vs_pools`, side 0, character 0, the palette-acquire table `DS_00107608` zeroed (`m1f_seed`) | `3B` (1) |
| 192 | `26DAE/26DB8` | `flow.c` `flow_26d4c` | `vs_mode22` (batch A's) | batch A's | `29`, `22` (5) |

`K12_B2_GAME_ROWS` = 18 (`test_game.c`, run from `test_voice_sites`) and
`K12_B2_FIGHT_ROWS` = 12 (`test_fight.c`, `test_fight_voice_sites`): 30.
Rows 167/177/179/183/192 reuse batch A's drivers unchanged; their batch-A
rows (166/176/178/182/191) are not edited (§4.9) and still list only their
own ids. Same-id aliasing (§4.2): no B2 path has two calls with one id, and
rows 186/187's repeated ids come from two separate driver calls, each of
which makes both calls once.

Row 186/187: the port's one path (`side_of`/`next_of`) serves four raw
pairs, so it is one `0xE3` and one `0xE2` call; the comment names all eight
`mov`s and all eight calls. The two drivers cover all four states (`0xB`,
`0xC` and `0xD`, `0xE`).

### §6.2 Corrections (raw wins)

- **Row 25:** the brief's example cites `0x43285/0x4328A`. The raw's `mov
  eax,0x2d` is at `0x43272`, before the `+0x2E` store (`0x43277`), the
  `+0x4E` store (`0x43280`) and the hook store (`0x43284`); nothing between
  touches EAX. The comment carries `0x43272/0x4328A`. The port stores the
  hook first, the same state at the call.
- **Rows 186/187:** the old comment cited `0x1F10E`/`0x1F024` as two of the
  `0xE2` calls. Those are the `mov`s; the calls are `0x1F113`/`0x1F029`, as
  §0.4 has them.
- **Row 13, `jne 0x111FA`:** confirmed. `0x111EA cmp dword [0xF0A5C],0; 0x111F1
  jne 0x111FA` (`0x56`), else `0x111F3 mov eax,0x54; jmp 0x111FF`. The test is
  a dword compare, so the port reads `DSD(DS_000F0A5C)`, although case 1
  stores the value as a byte (`0x11098`). The brief's row lists `0x56` only
  and reads `DS_000F0A50` as found; the runner's rule (§4.2, every byte a
  driver reads is seeded) makes the driver spawn the actor itself (case 3,
  `0x111AF`), and it runs both arms, so the row lists `0x54`, `0x56` in that
  order and a swapped selection fails it (§6.5).
- **EDX at rows 165, 190, 192:** the old comments named `edx=0`, `edx=0x1D`
  and "EDX is game_frame's" as if they were arguments. `0x2C3FC` pushes EDX
  (`0x2C3FD`) and overwrites it at `0x2C3FF` (`mov edx,eax`) before any read,
  so EDX is never an input and the one-argument `sound_voice` is exact. The
  comments now say so. Rows 190/192 were also marked "spec §7" (out of
  scope); they are in scope under §0.3 and now wired.

### §6.3 `test_fight_voice_sites` (the structure Tasks 7-10 extend)

`test_fight.c` ends with a block headed `record k7-k12 §6..§10`:

- **Registration:** `X(test_fight_voice_sites)` follows `X(test_voice_sites)`
  in `TEST_CASES`, so it runs last, after every `actors_init()` caller.
- **Helpers (file-local, reusable by later batches):** `vf_pools()` points
  `DS_001014F4`/`DS_001014EC` at the private pool `VF_POOL`/`VF_PSET` (the
  same scratch as `test_game.c`'s `RA_POOL`/`RA_PSET`) and runs
  `actors_reset()`. Scratch records: `VF_NODE`, `VF_REC`, `VF_THEM`
  (`FIGHT_RECS + 0x3000/0x3100/0x3200`). Local `#define`s name the
  addresses `symbols.h` does not (`VF_00108396`, `VF_00108397`,
  `VF_00104529`). A registered static is reached with `fn_resolve(0xADDR)`
  and a NULL check (a NULL leaves the row's ids unlogged, so it fails).
- **Adding a batch:** after `k12_b2_fight[]`, add the batch's `vf_*` drivers
  and `static const TfVoiceSite k12_c_fight[]` (Task 7), `k12_d1[]` (8),
  `k12_d2[]` (9) or `k12_d3[]` (10), a local `#define K12_<B>_ROWS n`, and in
  `test_fight_voice_sites` one `CHECK_EQ_INT` on the table size plus one
  `tf_voice_sites` call, after the B2 pair. Do not edit an earlier table.
- **Per-row rules** are §4.2's: one entry per wiring point, `ids` the wired
  voices on the driver's path in raw order (at most 4; a path over 16 voices
  fails the cap check), every scratch byte seeded by the driver, no bump-heap
  growth, C statics restored. A case-2/3/4 id is logged without a bank read
  (the runner sets `DS_001028C8` = 0).
- **Seams available:** the voice log (`sound_voice_log_*`, §4.1), the
  fixtures `tf_demo_fixture`, `tf_hit_fixture(ch)`, `tf_anim_alloc_record`,
  `tf_anim_spawn_stream`, and the snapshots the runner takes (data object,
  both pools named at entry, aperture, DAC). Outside those, a driver's
  scratch writes (`FIGHT_*`, `ANIM_*`, `VF_*`) persist, so each driver seeds
  what it reads.

### §6.4 Tests

The two tables and their size checks. With the tables in and the source
unwired (`git stash push -- port/src`), the suite printed 30 `voice site
row` lines (one per row: rows 167/177/179/183/192/18/19/28/30 `1 of n`, the
rest `0 of n`) and `FAILURES: 30`. Wired: `all checks passed`.

Assertion sites: 13372 → **13374** (the two size checks).

`not wired` lines (`rg -c 'not wired' port/src`), before → after:
`actors.c` 6 → 3, `attract.c` 4 → 3, `fight.c` 33 → 26, `fighter.c` 68 → 67,
`flow.c` 36 → 26; the others unchanged. Total 169 → 144.

### §6.5 Mutations (measured; FAIL lines exclude the closing `FAILURES: N`)

Every one of the 30 calls was deleted in turn (`$K/t6mut.py`: the call
becomes `(void)0;`, rebuild, `PR_ORACLE_REQUIRED=1 ./build/run_tests`,
restore; outputs `$K/t6-mut-<row>.txt`, summary `$K/t6mut-summary.txt`).
Every FAIL line is `test_fixtures.c:232` (the in-order check).

| deleted call | FAIL lines | failing rows |
|---|---:|---|
| row 4 (`0x490E1`), first of the fight table | 1 | 4 (`0 of 1`) |
| rows 5, 31, 164, 165, 169, 188, 189, 190 | 1 each | their own (`0 of 1`) |
| row 13 (`0x111FF`), first of the game table | 1 | 13 (`0 of 2`) |
| rows 167, 177, 179, 183, 192 | 1 each | their own (`1 of 2`: batch A's id still logged) |
| row 18 (`0x43741`) | 2 | 18, 30 (`2 of 3`) |
| row 19 (`0x444D1`) | 2 | 19, 28 (`2 of 3`) |
| row 24 / 25 / 26, middle of the fight table (25) | 3 each | 24, 25, 26 (`0` / `1` / `2 of 3`) |
| row 28 (`0x44626`) | 2 | 19, 28 (`1 of 3`) |
| row 30 (`0x43729`) | 2 | 18, 30 (`1 of 3`) |
| row 64 / 65, last of the fight table (65) | 2 each | 64, 65 (`0` / `1 of 2`) |
| row 180 / 181 | 2 each | 180, 181 (`0` / `1 of 2`) |
| row 184 / 185, middle of the game table (184) | 2 each | 184, 185 (`0` / `1 of 2`) |
| row 186 / 187 | 2 each | 186, 187 (`0` / `1 of 4`) |
| row 192 (`0x26DB8`), last of the game table | 1 | 192 (`1 of 2`) |

No deletion failed a row off its own path. Rows that share a path (18/30,
19/28, 24-26, 64/65, 180/181, 184/185, 186/187) list each other's ids, so
deleting either call fails both. One more mutation: row 13's selection
swapped (`!= 0u` → `== 0u`, so `0x56` then `0x54`) gives 1 FAIL line,
`voice site row 13: 1 of 2` (`$K/t6-mut-13sel.txt`).

### §6.6 Not tested

- The dispatcher's effect at these sites (case 1's song words, case 5's
  stop tests); `check_sound_voice` (§45-A) pins those bodies.
- Each call's position relative to its non-voice neighbours (e.g. row 25
  after the hook store, row 188 before the `DS_00104B25` store), except the
  in-order ids on each path.
- Row 4's other arms (phase 0's `0xF0`, row 3, is batch D1; phase 1 with
  `n > 0` makes no call) and the phase-0/near paths.
- Row 13's arm with `+0x2C` above `0x1100` (no call) and the phase-3 actor
  from the attract's real flow (the driver spawns it directly).
- Row 164's other routes to the countdown (`r` = 1/2, the forced tick
  `DS_00105C04`) and its `r == 2` / think-gate-1 arm (no call).
- Rows 18/19 reached directly (`fight_char_screen_open(_both)` has other
  callers: `0x28D68`/`0x28D80`'s hook, `0x4372E`); only the `0x4367C` path
  is driven.
- Rows 64/65 with `+0x53` other than 4 (the voice precedes the switch).
- Rows 186/187's states reach the one C call pair; which raw address each
  state's call is cannot be told apart in the port (one path).

### §6.7 Gate

- `make verify` (with every fixed `/tmp` path overridden, below): `EXIT=0`
  (`$K/verify-t6.txt`, 571 lines, `all checks passed`). The brief's grep of
  the oracle lines diffs empty against `$K/oracle-lines-base.txt`
  (`ORACLES-EQUAL`). Row 13 is the only B2 point on an oracle path; its case 1
  writes `DS_00105D5C`, `DS_001028D4` and `DS_001028D9` only, and no oracle
  line moved.
- Dumps: `dumps.sh t6-after` matches `$K/base.sha256` (`dumpsha.sh`, exit 0;
  `check/frames` 24000, `fe/run1` and `fe/run2` 1384 each, the three drivers
  `all checks passed`). No before-dump was taken: the after-dump equals the
  base manifest itself, which `a011cb1` was already proven against (§4.8),
  and `9f15a6d` changed only tests and `flow.h`'s cap macro. The dump is
  deleted.
- `make audio-render`: `$K/t6-after.wav` is byte-identical to
  `before-t2.wav` (`cmp`; sha256 `df74acfb…a380844`, 2386412 bytes).
- `python3 tools/port_progress.py`: `767 1203 64` / `731 731 100`,
  unchanged (no function is ported; the README stays as it is).
- Build: 0 warnings.
- **Parallel safety.** The worktree shares `/tmp` with sibling worktrees.
  The fixed paths a verify run touches are the Makefile's `SMK_DUMP`,
  `TITLE_DUMP`, `ATTRACT_DUMP`, `FRONTEND_DUMP`, `TITLE_PIN_DIR` and
  `AUDIO_WAV` (all overridden with `/tmp/pr_t6_*` on the command line; the
  sub-`make`s inherit them). `test_game.c`'s `/tmp/pr_frontend_det` is only
  the fallback when `PR_FRONTEND_DET` is empty, and every Makefile caller
  sets it to `$(FRONTEND_DUMP)`. `test_platform.c`'s `/tmp/pr_resbig_XXXXXX`
  is a `mkstemp` template (unique). The `re-render` and `re-oracle`
  targets write `/tmp/*.ppm` and `/tmp/prage_dataobj.txt` but are not in
  `make verify`. The `--check` step writes `frames/` in the worktree.

---

## §7 Task 7: K12 batch C, the sample voices on oracle paths

Implemented in the worktree `k12-t7` (branch `k12-t7`, from `b7b7c74`).
Every batch-C call was re-read from the fixup-applied image (`$K/dx.py`), and
every table id from the same image. Each C call carries its raw addresses:
a `mov`/`call` pair, or the table read and the call.

These are the first sample voices wired on the enforced oracle paths.
Rows 1, 6, 7, 33-35, 38, 58, 60, 61, 66-72, 75, 76 and 92-94 now queue and
start real samples in `--check`, the fe driver, the attract dump and the
title dump. The frame dumps are unchanged (§7.6).

### §7.1 The 22 wiring points

The ids are case 2, except `0x46` (case 3) in rows 72/76. The table ids come
from the raw tables in §1.1 row 0.2. The triple is `0x3AFC4`'s:
`anim[0] = 0xDE114 + c*11`, with `c = char << 6 | reaction`.

| row | call | C site | driver | seed | ids |
|---:|---|---|---|---|---|
| 1 | `2B8D2/2B8D7` | `actors.c` `spawn_anim_opcode` case `0x2E` | `vc_anim_2e` | `vf_pools`; `tf_anim_alloc_record`; the stream `9F2E 00D3 9F2E 00CD 0000` at `ANIM_SCRATCH` through `actors_anim_begin` | `D3`, `CD` |
| 6 | `10F3E/10F43` | `attract.c` `attract_voice_tick` | `vc_voice_tick_be` | `F0A60` = `F0A62` = 1, seed `0xABCD` (draws 6, then 1) | `BD`, `BE` |
| 7 | `10F7B..10F86`, `10F8B` | same | `vc_voice_tick_bf` | as row 6, seed 3 (draws 17, then 0) | `BD`, `BF` |
| 33 | `4A669..4A688`, `4A6D2` | `fight.c` `fight_4a634` | `vc_crowd_cd` | `tf_demo_fixture` (empty list), mode 3; both slots' `+0x42` = 2; `1088A8` = `20`/`3F`, seed 18; then a second pass with side 0 at `2A` only; `rng_next(3)` = 0, 1, 2 | `CD`, `CE`, `CF` |
| 34 | `4A6B2/4A6B7`, `4A6C3/4A6C8` | same | `vc_crowd_c9` | as row 33, `1088A8` = `10`/`17`, seed 12; `rng_next(2)` = 1, 1 (side 0), then 1, 0 (side 1) | `C9`, `DA`, `CA`, `DB` |
| 35 | `4A6BC`/`4A6CD`, `4A6D2` | same | same | same | `C9`, `DA`, `CA`, `DB` |
| 38 | `4B478..4B492`, `4B497` | `fight.c` `fight_4b470` | `vc_trample` | `vf_pools`, `tf_demo_fixture`, mode 3, `104B1D` = 3 (ends at `0x4B547`); two type-8 entries: `+0x1C` = `0x40`, holder side 0 (char 0, record `+8` = 0, so `0x4AF04` = 0), `+0x4A` links 1/2, `+0x10` a shadow; records at `+0x48` = `0x20` and `0x23` | `D1`, `D0` |
| 58 | `3B9A1..3B9B3`, `3B9B8` | `fighter.c` `fighter_3b938` | `vc_3b938` | `tf_hit_fixture(4)`; slot 0 `+0x08` = a zeroed record with `+0x48` = 2 | `BDFFA[4]` = `AC` |
| 60 | `367C6/367CF` | `fighter.c` `fighter_state_36710` | `vc_36710` | `tf_hit_fixture(0)`, the slot pair; `+0x58` = 2, `rec+0x36` = `rec+0x1C` = 0 | `6E` |
| 61 | `35ED4/35ED9` | `fighter.c` `fighter_state_35e6c` | `vc_35e6c` | `tf_hit_fixture(0)`, the slot pair, command 0 | `6D` |
| 66 | `35E26..35E33`, `35E38` | `fighter.c` `fighter_35e04` | `vc_35e04` | side 1's record, `+0x14` = slot 1, slot 1's char 2, the `DS_00107D40[1]` row | `BDAA8[2]` = `7B` |
| 67 | `34F8B..34F9F`, `34FA4` | `fighter.c` `hit_reaction_apply` | `vc_react_stream` | `tf_hit_fixture(0)`; char 0, reaction 0: stream `0xE6F94`, no callback; byte `+7` = 12 | `E9308[12]` = `8E` |
| 68 | `35019..3502D`, `35032` | same | `vc_react_cb` | char 0, reaction `0x2A`: no stream, callback `0x3E3A8`; byte `+7` = 10 | `E9308[10]` = `87` |
| 69 | `3D193/3D19D` | `fighter.c` `fighter_3d17c` | `vc_3d17c` | `tf_hit_fixture(0)`; slot 0 `+0x08` = 0 | `91` |
| 70 | `14802/14807` | `fighter.c` `fighter_146f0` | `vc_1461c` | `c3_seed`; slot 0 `+0x57` = 2 (`0x146D7`), `FD11C` = 0 | `B1` |
| 71 | `14824/14829` | `fighter.c` `fighter_14814` | `vc_14814` | `c3_seed` | `B0` |
| 72 | `14E33/14E38` | `fighter.c` `fighter_14d7c` | `vc_14d7c` | `c3_seed`, test I's positions; slot 1 char 4; slot 0 char 4, `+0x5F` = `0x25` (`0x39834` swaps to `ctx[0]` = 0); byte `+8` = 8 | `E933C[8]` = `46`, `B3` |
| 75 | `39ED5/39EDA` | `fighter.c` `fighter_39cc8` | `vc_39cc8` | `vf_pools`, `kb_seed`; `+0x58` = 3, slot 1 `+0x30` = 5632, equal to the ground (it lands) | `6C` |
| 76 | `39959..39969`, `3996E` | `fighter.c` `fighter_39834` | `vc_14d7c` | as row 72 | `46`, `B3` |
| 92 | `3AB96..3ABAA`, `3ABAF` | `fighter.c` `fighter_reaction_apply` | `vc_reaction_apply` | `pose_chain_setup`; slot 0 `+0x5F` = `FF`, char 3; slot 1 `+0x5F` = `0x20` (`0x3A280` holds), char 0; reaction 0 (`0x39834` byte `+8` = 2) | `E933C[2]` = `65`, `BE008[3]` = `9F` |
| 93 | `3ADAA..3ADBC`, `3ADC1` | `fighter.c` `fighter_3ad98` | `vc_3ad98` | `pose_chain_setup`; slot 0 char 4; the triple of side 0, reaction `0x24` (byte `+9` = 5, `anim[2]` word 0: no spawn) | `E9358[5]` = `74` |
| 94 | `153B6/153BF` | `fighter.c` `fighter_15350` | `vc_15350` | `c3_seed`; slot 1, the other side's `0x468D8` false | `AF` |

`K12_C_GAME_ROWS` = 2 (`test_game.c`, `k12_c_game[]`, run from
`test_voice_sites`). `K12_C_FIGHT_ROWS` = 20 (`test_fight.c`,
`k12_c_fight[]`, run from `test_fight_voice_sites`). The total is 22. New
table addresses get local `#define`s in `fighter.c`: `FIGHTER_BDFFA`,
`FIGHTER_BDAA8`, `FIGHTER_E9308` and `FIGHTER_BE008`. `FIGHTER_E933C` and
`FIGHTER_E9358` already existed.

**Same-id aliasing (§4.2).** No batch-C path has two calls with one id.
The one row-33 call runs three times in its driver, and each run posts a
different id. Rows 72/76 share a driver, and rows 34/35 share one. Deleting
either call on a shared path fails both rows (§7.4). Row 92's path also
posts row 76's id (`0x65`, through `0x39834` at `0x3AB7C`), so deleting row
76's call fails row 92 as well.

**Every id matches the raw.** A temporary print in the runner (not
committed) showed each row's whole log equal to its `ids`. No row posted
any other voice.

### §7.2 Corrections (raw wins)

- **Row 38's id.** The old comment and §1.3 say `0xD1` "for si < 3",
  where si is EDX. The raw reads the record, not EDX:
  - `0x4B478 mov eax,[eax+8]; 0x4B47B mov al,[eax+0x48]; and eax,0xff;
    sub eax,0x20; 0x4B486 cmp eax,3; 0x4B489 jge 0x4B492` (`0xD0`),
    else `0xD1`.
  - The compare is signed, on a 32-bit value.
  - The caller's si is `(u16)(rec+0x48 - 0x20)`. For every `+0x48` of
    `0x20` or more, both forms give the same id.
  - Below `0x20`, the raw gives `0xD1` and the si form gives `0xD0`.
  - The port reads the record, as the raw does. Mutation `38sel` (`<= 3`)
    fails the row.
- **The `+0x42` test in rows 33-35.** The brief says "bit 2". The raw is
  `0x4A640 test byte [ebx+0x1077F2],2`: mask 2 (bit 1), as §1.3 has it and
  as the port already tested.
- **Row 1's operand.** §1.3 notes that "the one-byte form sign-extends to
  `0xFFxx`". That form cannot reach opcode `0x2E`:
  - A command's own opcode is `(word >> 8) & 0x1F` (`0x2B2B1`), at most
    `0x1F`, so `0x2E` needs the `0x1F` prefix.
  - Under the prefix, the operand is the next word (mode 0, `0x2B932`) or a
    variable read (modes `0x2000`-`0x6000`).
  - EAX still holds that operand at `0x2B8D2` (it is kept from `0x2B2DB`),
    and `and eax,0xffff` keeps its low word.
- **Row 70's gates.** §1.3 lists `0x146F0`'s gates `0x14728`, `0x1473A` and
  `0x147A5` "to `0x14807`". They are branch selectors, and every arm rejoins
  before `0x147ED`. The call at `0x14807` is unconditional in `0x146F0`.
- **Rows 6/7.** The brief's worked row gives both rows one driver and the
  ids `BD`, `BE`. Row 7's entry uses seed 3 instead (pick 0, so `BF`).
  The two entries then cover both arms of the pick, and swapping the
  selection (`7sel`) fails both rows.

### §7.3 Tests

The two tables and their size checks. With the tables in and the source
unwired, the suite printed 22 `voice site row` lines (every row `0 of n`)
and `FAILURES: 22`. Wired, it prints `all checks passed`.

Assertion sites (`rg -c 'CHECK\(|CHECK_EQ_INT\(' port/tests`, summed):
13371 → **13373** (the two size checks).

Comments that described these calls as unwired are updated:
- the `fighter_state_36710`, `fighter_3ad98` and `hit_reaction_apply`
  headers;
- `fighter.h`'s `0x3AD98` comment and `attract.h`'s `0x10F28` comment;
- `test_game.c`'s two `0x10F28` comments (the pick is no longer "stubbed"
  or "discarded"; no assertion changed).

`not wired` lines (`rg -c 'not wired' port/src`), before → after:
- `.c` files: `actors.c` 3 → 2, `attract.c` 3 → 0, `fight.c` 26 → 23 and
  `fighter.c` 67 → 49; the others are unchanged. The total is 144 → 119.
- Headers: `attract.h` 1 → 0 and `fighter.h` 1 → 0.

### §7.4 Mutations (measured; FAIL lines exclude the closing `FAILURES: N`)

`$K/t7mut.py` deletes every wired call in turn: the call becomes
`(void)0;`, the build is redone, `PR_ORACLE_REQUIRED=1 ./build/run_tests`
runs, and the source is restored. It then applies the selection and index
mutations. The outputs are `$K/t7-mut-<tag>.txt`, and the summary is
`$K/t7mut-summary.txt`. Every FAIL line is `test_fixtures.c:232`, the
in-order check.

| mutation | FAIL lines | failing rows |
|---|---:|---|
| delete row 6 (`0x10F43`), first of the game table | 2 | 6, 7 (`0 of 2`) |
| delete row 7 (`0x10F8B`) | 2 | 6, 7 (`1 of 2`) |
| delete row 1 (`0x2B8D7`), first of the fight table | 1 | 1 (`0 of 2`) |
| delete row 33 (`0x4A6D2`, the draw's call) | 1 | 33 (`0 of 3`) |
| delete `0x4A6B7` / `0x4A6D2` (`DA`) / `0x4A6C8` / `0x4A6D2` (`DB`) | 2 each | 34, 35 (`0` / `1` / `2` / `3 of 4`) |
| delete row 38 (`0x4B497`) | 1 | 38 (`0 of 2`) |
| delete rows 58, 60, 61, 66, 67, 68, 69, 70 (middle), 71, 75, 93, 94 (last) | 1 each | their own (`0 of 1`) |
| delete row 72 (`0x14E38`) | 2 | 72, 76 (`1 of 2`) |
| delete row 76 (`0x3996E`) | 3 | 72, 76 (`0 of 2`), 92 (`0 of 2`: its path posts `0x65` through `0x39834`) |
| delete row 92 (`0x3ABAF`) | 1 | 92 (`1 of 2`) |
| `7sel`: `pick == 0u` | 2 | 6, 7 (`1 of 2`) |
| `33sel`: `d == 2u ? 0xCE` | 1 | 33 (`2 of 3`) |
| `34sel`: the second `rng_next(2) == 0u` | 2 | 34, 35 (`2 of 4`) |
| `38sel`: `- 0x20 <= 3` | 1 | 38 (`1 of 2`) |
| `67idx` (`anim[0]+8`), `68idx` (`+9`), `93idx` (`+8`) | 1 each | their own (`0 of 1`) |
| `76idx` (`anim[0]+7`) | 3 | 72, 76, 92 (`0 of 2`) |
| `92idx` (the other slot's char) | 1 | 92 (`1 of 2`) |
| `58tab` (`BDFFA` → `BDAA8`), `66tab` (`BDAA8` → `BDFFA`) | 1 each | their own (`0 of 1`) |

The brief's minimum was rows 6, 33 and 94. All of them fail, as do the
other 21 deletions and the 11 selection and index mutations. No mutation
failed a row off its own path.

### §7.5 Not tested

- **What the dispatcher does at these sites.** The runner sets
  `DS_001028C8` = 0, so a call is logged but queues nothing. Task 3's
  vectors pin the case-2/3 queue and start. The oracle runs' real queues
  are covered only by frame identity (§7.6).
- **Rows 33-35's no-call arms:** the first `rng_next(2) == 0` arm, a
  `1088A8` byte outside both ranges, and `+0x42` bit 1 clear. The `0x4A675`
  fall-through for a draw above 2 is unreachable (`rng_next(3) < 3`).
- **Row 38's other callers.** `0x4B69C` (`0x4B779`) and `0x4D7A4`
  (`0x4D873`) are not driven; only case 8 (`0x4A10B`) is. A `+0x48` below
  `0x20` (§7.2) is not driven either: si would be `0xFFxx`, which indexes
  past `0xC9604`.
- **Row 1's variable-read operand modes** (`0x2000`/`0x4000`/`0x6000`).
  Only the literal word is driven.
- **Row 58's `+0x48 == 4` arm.** It makes the same call and is not driven.
  The `0x63` chars (case 0) are not driven.
- **Rows 67/68 on one path.** A reaction with both a stream and a callback,
  such as char 2 reaction `0x0B`, posts `0x7C` twice. That is same-id
  aliasing, so it is not used.
- **Row 70's other `0x146F0` arms** (`FD11C` set, `0x14590` true). The
  voice is on every arm.
- **`0x39834`'s other callers:** rows 77, 107-110 and 117. Only row 72's
  and row 92's paths are driven.
- **The no-call exits:** row 60's first two arms, row 66's `+0x14 == 0`,
  row 69's `+0x08 != 0`, row 75's non-landing phase 3, row 92's `0x3A280`
  false and row 94's `0x468D8` true.
- **Each call's position relative to its non-voice neighbours.** Only the
  in-order ids on each path are checked.

### §7.6 Gate

- **`make verify`.** It was run with every fixed `/tmp` path overridden
  (`/tmp/pr_t7_*`). `EXIT=0` (`$K/verify-t7.txt`, 571 lines,
  `all checks passed`). The brief's grep of the oracle lines diffs empty
  against `$K/oracle-lines-base.txt` (`ORACLES-EQUAL`).
- **Dumps.** A before-dump was taken on the unmodified `b7b7c74` build
  (`dumps.sh t7-before`), and it equals `$K/base.sha256`. The after-dump
  (`t7-after`) is identical to it (`dumpcmp.sh`, `DUMPS-IDENTICAL`) and
  also equals `base.sha256`. Counts: `check/frames` 24000, `fe/run1` and
  `fe/run2` 1384 each, and the three drivers `all checks passed`. Both
  dumps are deleted.
- **`./build/prageport --game-dir data/game/C --check 8000`:** `CHECK=0`.
- **`make audio-render`:** `$K/t7-after.wav` is byte-identical to
  `before-t2.wav` (`cmp`; sha256 `df74acfb…a380844`, 2386412 bytes). The FM
  path renders no samples.
- **Where the voices fire (measured).** A scratch copy of the port printed
  every `sound_voice` id; the copy is deleted and nothing was committed.
  The counts below are ids, not rows. The fe, attract and title drivers
  run unit checks before `game_init`, so their counts include those calls.
  - `--check 8000`: `BD` ×41, `BE` ×14, `BF` ×18, `C9` ×2, `CA` ×1, `CE`
    ×2, `CF` ×1, `DA` ×2, `DB` ×1, `D3` ×4, `6D` ×14, `6E` ×1, `6C` ×5,
    `91` ×1, `B3` ×1, `9F` ×1, and the table ids.
  - fe dump: `D0` ×2, `46` ×1 (case 3), `AF`, `B0`, `B1` and `87` ×1
    each, `CD` ×3, `8E` ×1, and the table ids.
  - attract dump: `BD` ×14, `BE` ×7, `BF` ×4.
  - title dump: `BD` ×10, `BE` ×4, `BF` ×4.

  So batch C's samples are queued and started on all four oracle paths,
  and the frames do not move. This matches §0.2's probe: every one of
  these voices resolves a bank that has already been read.
- **`python3 tools/port_progress.py`:** `767 1203 64` / `731 731 100`,
  unchanged. No function is ported, and the README stays as it is.
- **Build:** 0 warnings.
