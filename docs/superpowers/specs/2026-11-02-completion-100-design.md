# Completion truth — 100% of the game's RE

**Status:** design, 2026-11-02. Goal: the terminal claim register has **no
`declared` row that is a RE gap**; all 1,203 indexed functions are accounted
for by evidence; the two real remaining exhibits (the U14 two-human win and the
service-menu language) are published; the rest of the declared rows are
reconciled to `terminal` with their measured reasons.

## 1. What "100%" means, measured

`python3 tools/port_progress.py` reads `798 1203 66` / `759 759 100`. The
identity is exact and measured:

```
1,203 = 798 ported (759 portable + 39 runtime)
      +  41 host-owned + 12 deferred
      +  37 replaced (clean-room runtime)
      + 315 runtime (WATCOM libc / DOS4GW / AIL)
```

`0 unclassified`; each `FN_` address has exactly one class
(`tools/port_classification.txt`, `tools/port_progress.py:32,61-65,123`). The
game's own code is **759/759 portable (100%)** and the differential harness
verifies **1033/1033** rows. The 66% is the **port share of the index**, not
RE completeness: a literal 100% raw would mean porting WATCOM/AIL runtime, which
is a declared non-target (R20) and the AIL set is named unreproducible.

The honest figure the docs will state: **66% ported / 100% analysed**, with the
identity above. The index boundary is declared (388 span-writer entries,
4 unindexed call targets triaged outside it).

## 2. The remaining declared rows and their disposition

The register has 7 declared rows after Volleyball truth. Measured position
(2026-11-02, six read-only analysts):

| row | disposition | measured reason |
|---|---|---|
| R03 U14 two-human win | **close** | the `.failed` artifact is a complete real-input 2-0 win (`gp_twop.py check --win` passes: KO `f=0xB1B`, match `f=0x13A8`, X `f=0x1693`, `b1f=3`, zero `W`); the only failing check is the port-script model's `S(f-1)` contiguity pin after a 3-snapshot poller gap (`0xB18..0xB1A`). Fixing the model to derive `bits` from the `I` records leaves all other captures byte-identical. |
| R27 inventory | **re-pin** | promoting R03's artifact adds its path; the final inventory is 47 explicit, 46/47 present, one declared skip (`gp-u14-win-live`, R04 terminal). |
| R30 poked 1P `0x21`/`0x25` | **terminal** | every `DS_00104ABC` store recomputes `(DS_00104B1F == 3) + 1` — the seven sites `0x28DE7`, `0x25EC5`, `0x2955A`, `0x27DE6`, `0x28932`, `0x25B27`, `0x26B94` — and the in-match join only ORs `b1f` then stores ABC, so the 1P gate state (`b1f != 3` with `DS_00104ABC = 2`) is inconsistent at every store; its consequences are `fighter.c:6501`'s 1P arm and the `config.c:703/743` run clock. The gate acceptance is already unit-proven (`test_fight.c:3435`, `:29354` case 9) and R05/R06 exhibit the real input. No distinct claim. |
| R11 two unattributable audio banks | **terminal** | the requests are real (`grave+0x0A8C` f5323/5363/5403, `grave+0x05E4` f5311..5433 in the ending S rows) but each is replaced before its first key-on: `86,371..90,424 ms` holds zero register writes; the "tick-0 resets" are the port's `seq_start` pair. A section needs a key-on; no capture or model change can attribute an empty interval. |
| R16 U16 finisher reaction | **terminal** | measured on `gp-u24-u16-state-t9-r5`: mode 9 `f=0x1485`, the gate's 99 `s1_42=0x8830` rows `f=0x149A..0x14FC`, the close `0x8030` at `f=0x14FD`, the first `r0=0x33` at `f=0x155A` (93 frames after the close) and the `s1_42=0x8030→0x8014` phase change at `f=0x15A1`; the three finisher entries' presses are `0x14E8`/`0x154C`/`0x15B0` (`f=0x1485` is the prior round's `p1.b2`/`p1.b3` press). The old 110/133-frame latency figures are withdrawn (not traceable to the S rows). The poked `r0=0x33` is a table reaction whose callback returned early. The port's `0x27`-vs-`0x33` divergence is already closed (IVT fix, `GP_U23_MOVES_MIN_FIRST 6050`). |
| R12 audio never-requested | **split** | `sound+0x7714` (`0xE4`) is requested only inside R16's 99-frame gate → **terminal** by the same measurement. `konsd+0x1CCB0` (`0xEB`): the `0x24508` call is the `D100` word + `0x24508` dword at `0xE50E0`, inside BLIZZARD's reaction-0x34 stream (`0xC92D8[1] = 0xE50D2`, callback `0x37898`); the reaction-0x33 stream `0xC92B0[1] = 0xE5038` does not contain it, and no capture carries `r0/r1 = 0x32`/`0x34` (only `0x33`), so the focused search finds no shipped route: **R39 declared** with that measured reason. |
| R28 music-truth optional | **split** | *language*: real functional gap — the raw reloads ENGLISH/FRENCH/GERMAN/ITALIAN/SPANISH/PORTUGAL/BRAZIL (table `0xC9418`, `0x47370`; boot `0x20C72..0x20C7F`, reload `0x336AE`), the port is English-only and `STRING_CAP` 0x2000 < FRENCH 8,564. Implement `game_string_table_load` (+ `STRING_CAP` 0x2200) and the boot/config reload; then one `k11-lang` capture exhibits the screen. *coin countdown*: already gated (R07/R08) and the drawing is inside the byte-exact title/front-end windows → **terminal, not a distinct claim**. *housekeeping*: the `closed_rows` figure is asserted (`825/845`, `810/845`) and the allowance-before-window-start guard is enforced in `gp_compare.frame_claim`; the remaining Task 6 items are enumerated in R38 as not a game claim → **terminal**. |

## 3. Claim boundaries

- R03's closure is the game's own 2-0 two-human win, **not** an arcade win; R04
  stays terminal. The harness inputs are real; zero `W`; R30's no-poke rule holds.
- The port-script model fix changes only how `bits` are derived (from `I`
  records); every existing capture's generated script must stay byte-identical,
  and the new artifact's `S` cross-check must pass 5,820/5,820.
- R28's language implemention covers the functional loader and the menu reload;
  the screen claim is the `k11-lang` capture. The absent `BRAZIL.TXT` (index 6)
  exits in the raw and is declared, not implemented.
- R30/R11/R16/R12 are status corrections with the measured evidence; no capture,
  no poke and no pin moves.
- R09 stays `gated` (12 whole-section banks, 86 prefixes / 44,489 ticks); it is
  a coverage claim, not a universal-equality claim. R10 stays terminal with its
  three comparison-model and seven driver/boundary rows.

## 4. Acceptance

1. `python3 tools/tests/test_final_claims.py` passes with **zero declared
   rows** except R39 (the `0xEB` route, which the search found no shipped input
   for; the row states the measured no-route reason).
2. `gp-u14-twop-win` is a clean capture in `CAPTURE_DIRS` with its oracle/pins;
   the register R03/R27 updated; 47 explicit / 46/47 present.
3. The language loader is implemented and tested; `k11-lang` is a clean capture
   with its oracle/pins; R28 closed for the screen.
4. The accounting identity is in the README/AGENTS/register with
   `66% ported / 100% analysed`; the `AGENTS.md:576` stale `799` is corrected.
5. `make verify` exit 0 and `make audio-render` twice byte-identical; every
   existing pin intact; the coverage baseline re-measured only if a replay
   exercises new rows.
