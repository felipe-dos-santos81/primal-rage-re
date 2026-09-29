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
| 8 | `attract.c:133` | `10E06` | 100 | 5 | state | attract | A |
| 9 | `attract.c:152` | `10E6E` | 100 | 5 | state | attract | A |
| 10 | `attract.c:212` | `11024` | 100 | 5 | state | check, fe, attract, title | A |
| 11 | `attract.c:260` | `11160` | 40 | 2 | sample | check, fe, attract, title | T3 (loop byte 1) |
| 12 | `attract.c:260` | `1116C` | 42 | 2 | sample | check, fe, attract, title | T3 (loop byte 1) |
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
| 27 | `fight.c:1137` | `44557` | 100 | 5 | state | real play only | A |
| 28 | `fight.c:1164` | `44626` | 2E | 1 | state | real play only | B2 |
| 29 | `fight.c:1181` | `43696` | 100 | 5 | state | real play only | A |
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
| 44 | `fight.c:4751` | `49FF1` | DE | 6 | state | real play only | A |
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
| 139 | `flow.c:1433` | `257AD` | 100 | 5 | state | fe | A |
| 140 | `flow.c:1447` | `25820` | 53 | 0 | state | fe | A |
| 141 | `flow.c:1534` | `28DCA` | 100 | 5 | state | real play only | A |
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
| 166 | `flow.c:3941` | `25AF1` | 100 | 5 | state | real play only | A |
| 167 | `flow.c:3949` | `25B51` | 3D | 1 | state | real play only | B2 |
| 168 | `flow.c:4044` | `28C2A` | D8 | 2 | sample | real play only | D4 |
| 169 | `flow.c:4193` | `27821` | 2B | 5 | state | real play only | B2 |
| 170 | `flow.c:4392` | `121CE` | 41 | 5 | state | check, fe, attract, title | T3 (the stand-in replaces it) |
| 171 | `flow.c:4392` | `121D8` | 43 | 5 | state | check, fe, attract, title | T3 (the stand-in replaces it) |
| 172 | `flow.c:4576` | `1159F` | 100 | 5 | state | fe | A |
| 173 | `flow.c:4595` | `116C4` | 100 | 5 | state | real play only | A |
| 174 | `flow.c:4617` | `11844` | 100 | 5 | state | real play only | A |
| 175 | `flow.c:4878` | `11A94` | 100 | 5 | state | check, fe | A |
| 176 | `flow.c:5112` | `1EEF4` | 100 | 5 | state | real play only | A (split phrase) |
| 177 | `flow.c:5112` | `1EEFE` | E1 | 1 | state | real play only | B2 (split phrase) |
| 178 | `flow.c:5158` | `1F1EC` | 100 | 5 | state | real play only | A (split phrase) |
| 179 | `flow.c:5158` | `1F1F6` | E1 | 1 | state | real play only | B2 (split phrase) |
| 180 | `flow.c:5173` | `1F249` | E3 | 1 | state | real play only | B2 |
| 181 | `flow.c:5173` | `1F253` | E2 | 5 | state | real play only | B2 |
| 182 | `flow.c:5198` | `1F307` | 100 | 5 | state | real play only | A (split phrase) |
| 183 | `flow.c:5198` | `1F311` | E1 | 1 | state | real play only | B2 (split phrase) |
| 184 | `flow.c:5214` | `1F36C` | E3 | 1 | state | real play only | B2 |
| 185 | `flow.c:5214` | `1F376` | E2 | 5 | state | real play only | B2 |
| 186 | `flow.c:5271` | `1EF8D/1F07F/1F109/1F01F` | E3 | 1 | state | real play only | B2 (split phrase; 4 raw calls, 1 port path) |
| 187 | `flow.c:5271` | `1EF97/1F089/1F113/1F029` | E2 | 5 | state | real play only | B2 (split phrase; 4 raw calls, 1 port path) |
| 188 | `flow.c:5281` | `1EFA9` | E1 | 1 | state | real play only | B2 |
| 189 | `flow.c:5290` | `1F099` | E1 | 1 | state | real play only | B2 |
| 190 | `flow.c:5345` | `20955` | 3B | 1 | state | real play only | B2 |
| 191 | `flow.c:5476` | `26DA9` | 29 | 0 | state | real play only | A |
| 192 | `flow.c:5476` | `26DB8` | 22 | 5 | state | real play only | B2 |
| 193 | `flow.c:5587` | `26B32` | 60 | 2 | sample | real play only | D4 |
| 194 | `flow.c:7355` | `11DF2` | 100 | 5 | state | check, fe | A |
| 195 | `flow.c:7380` | `11BF0` | 100 | 5 | state | check, fe | A (tail jmp) |
| 196 | `menu.c:173` | `2FA6D` | 100 | 5 | state | real play only | A |
| 197 | `menu.c:295` | `2FFF6` | 100 | 5 | state | real play only | A |
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
todo-verify §1 records.

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
