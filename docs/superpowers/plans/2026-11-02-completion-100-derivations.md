# Completion truth — derivations

**Phase:** 2026-11-02. Design
`docs/superpowers/specs/2026-11-02-completion-100-design.md`; plan
`docs/superpowers/plans/2026-11-02-completion-100.md`; base `6ebfcff5`
(Volleyball truth merged). Task reports:
`.superpowers/sdd/2026-11-02-completion-100/task-{1,2,3,4,5}-report.md`.

## 1. The 100% figure (measured)

```
python3 tools/port_progress.py -> 798 1203 66 / 759 759 100
ported 798 (759 portable + 39 runtime) host-owned 41 deferred 12 replaced 37 runtime 315
```

`1,203 = 798 + 41 + 12 + 37 + 315` exactly; `class_map()` reports 0
unclassified and `ported ∩ classification = ∅` (each `FN_` address has exactly
one class; `tools/port_classification.txt`, `tools/port_progress.py:32,61-65,123`).
The index itself is 1,207 CSV rows: 1,203 numeric `FN_` + 3 mnemonic
(`FN_CASED_0/40/3`, all inside `FUN_0006FB28`) + 1 data-object artifact
(`0xE4F45`, dropped by `gen_symbols.py:44-49`); all 1,207 decompile `ok`; the
145 `.image` rows are analysis noise.

The game's own code is 759/759 portable (100%) and the differential harness
verifies 1,033/1,033 rows. The 66% is the **port share of the whole index**,
not RE completeness: the unported 405 are 315 WATCOM/libc/DoS4GW/AIL runtime,
37 clean-room-replaced runtime, and 53 host-owned/deferred. A literal 100% raw
port would mean porting WATCOM/AIL, a declared non-target (R20) whose AIL set
is named unreproducible. The honest figure, now in the README/register/AGENTS,
is **66% ported / 100% analysed**.

## 2. R03 — the U14 two-human win (closed)

The 2026-10-30 artifact `gp-u14-twop-win.failed` is the game's own real-input
2-0 two-human win: `gp_twop.py check --win` passes (KO `f=0xB1B`, match result
`f=0x13A8`, mode `0x12` at `f=0x1693`, `b1f=3`, zero `W`; a real P2 join at
`f=0x2CA`; the KOs from the game's damage path; the P2's only in-fight input is the
scenario's 30-frame left hold, `I p2.left` at `f=0x551` to the release at `f=0x56F`,
with 30 mode-6 S rows carrying `e2=0x2000`).
Its only failing check was the port-script model's `S(f-1)` bitmap pin after the
poller's 3-snapshot gap `S(0xB18..0xB1A)` (the step-97 pad release was injected
in spin `0xB17`; the bitmap changes at `0xB18`; the first accepted snapshot is
`0xB1B`).

**Fix** (`tools/gp_session.py:1866-1919`): `port_script` now derives `bits` from
the `I` press/release records (a write in spin `f` changes `S(f+1)`; the pad
scan→bit from `PAD`), with a fail-closed cross-check of every accepted `S`
against `raw_to_kb` (0/5,820 mismatches on the artifact). Every one of the 42
existing captures' generated scripts is byte-identical; the two `.failed`
diagnostics newly generate; `gp-u6-moves` is correctly rejected (the
contaminated first U6 capture; not in `CAPTURE_DIRS`).

The unchanged bytes were promoted to `data/k11-captures/gp-u14-twop-win` and
its session checks re-issued: all 11 ok. `gp-u14-twop-win-oracle` (in
`make verify`): N 5,072, trace/moves 5,780, start 100, reach 4,269, missing 10,
sha `a96ca6f5…`, 5,074 frames; `gp_win.py` 6/6 milestones and path, WIN fields
0 differing to 5,779. The artifact is preserved as `.failed`. **The claim is
the game's own 2-0 two-human win, not an arcade win** (R04 terminal, the AI's
120-damage race). R27 is terminal: 47 explicit capture paths, 46/47 present,
one declared skip (`gp-u14-win-live`, R04).

## 3. R28 — the language reload (closed)

The raw selects one of eight dword pointers at `0xC9418`
(english/french/german/italian/spanish/portugal/brazil; index 7 = index 0)
through `0x47370(index)`, loads the file, checks magic `0x33340001`, and stores
the handle at `0x1E75C`, the table at `DS_001082DC` and the count at
`DS_001082D8`. Boot calls it with index 0 (`0x1C0B8`) and reloads the config's
language nibble `(field 0x29 & 0x7000000) >> 24` at `0x20C72..0x20C7F`;
`0x336AE` reloads on a change.

**Port** (`port/src/game/flow.c`, `svcmenu.c`, `flow.h`): `game_string_table_load`
over the same table via `host_read_file`, the magic check, the three stores;
`STRING_CAP` 0x2000 → 0x2200 (FRENCH 8,564 = 0x2174 is the largest shipped
file); the boot load and the config-nibble reload; `svc_config_options` reloads
on a change. Index 0 stays byte-identical (no oracle moved); index 6
(`BRAZIL.TXT`, absent) is declared: the raw exits `0x1D290`, the port keeps the
last good table. Unit tests: ENGLISH/FRENCH sentinels (seeded to differ), the
editor reload case, two loader mutations that must fail.

`data/k11-captures/k11-lang` (460 frames, sha `5aeb8266…`; its checks were green
at capture time and `k11_capture.py` now persists them to `session.txt`) records the config field `0x29` `0x00142095 → 0x01142095` at ms 39,012 and
the OPTIONS/MAIN menus redrawn from `FRENCH.TXT`; `tools/k11_lang.py` is in
`k11-oracle` (0 unexplained, 17/17 settled, 201/201 non-black, open END 408),
with 6 checker tests and the two loader mutations failing.

R28 is **closed** for the language and screen. The two other Task 6 items are
separate rows: **R37 terminal** (the coin countdown is already gated by R07/R08;
`CREDITS:<n>` (`DS_00105C00`, `0x2BF08`) is drawn inside the byte-exact
title/front-end windows; not a distinct claim) and **R38 terminal** (the
music-truth housekeeping: the `closed_rows` figure is asserted and the
test asserts the allowance-before-window-start guard; the ResourceWarnings stay
named and the remaining Task 6 items are enumerated in R38; not a claim).

## 4. The status corrections (measured limits)

**R30 terminal.** Every `DS_00104ABC` store recomputes `(DS_00104B1F == 3) + 1`
— the seven sites `0x28DE7`/`0x25EC5`/`0x2955A`/`0x27DE6`/`0x28932`/`0x25B27`/
`0x26B94` (`flow.c:1676`/`:2460`/`:2603`/`:3070`/`:4016`/`:4168`/`:5833`) — and
the in-match join only ORs `b1f` (`0x28D02..0x28D13`) then stores ABC; the only
`b1f` clear is the between-matches `0x24B7C` reset. The poked 1P state
(`b1f != 3` with `DS_00104ABC = 2`) is therefore inconsistent at every store.
Its consequences are `fighter.c:6501`'s 1P arm and the `config.c:703`/`743`
run clock. The gate acceptance is unit-proven (`test_fight.c:3435`, `:29354`
case 9) and R05/R06 are the real-input exhibits. No distinct claim.

**R11 terminal.** The two unattributable starts are real
(`grave+0x0A8C` f5323/5363/5403; `grave+0x05E4` f5311..5433 in the ending
S rows) but each is replaced before its first key-on: `86,371..90,424 ms` holds
zero register writes; the "tick-0 resets" are the port's `seq_start` pair
(`sequencer.c:807-808`). A section needs a key-on; no capture or model change
can attribute an empty interval.

**R16 terminal.** The measurement is `data/k11-captures/gp-u24-u16-state-t9-r5`
(not the poked `gp-u23-u16-finisher`). Its S rows: mode 9 begins at
`f=0x1485` (`s1_42=0x8010`, `r0=0x00`); `s1_42=0x8830` on exactly **99 rows**
`f=0x149A..0x14FC` (`r0=0x11` on the first); `s1_42=0x8030` from `f=0x14FD`
(`r0=0x27`). The first `r0=0x33` is `f=0x155A`, **93 frames after the close**;
`f=0x15A1` is the `s1_42=0x8030→0x8014` phase change (85 frames after the
entry-0x16 press `0x154C`, 15 before the entry-0x17 press `0x15B0`), not a
reaction. The three finisher entries' presses are `f=0x14E8` (entry 0x15),
`f=0x154C` (entry 0x16) and `f=0x15B0` (entry 0x17); `f=0x1485` is the prior
round's `p1.b2`/`p1.b3` press (poll.log 5471-5472), not a finisher entry. The
old 110/133-frame latency figures and the "pressed by +10" reading are
**withdrawn** (not traceable to the t9-r5 S rows). The poked
`r0=0x33` is a table reaction whose callback returned early; the old port
`r0=0x27` divergence is closed by the IVT fix (`GP_U23_MOVES_MIN_FIRST` 6,050).

**R12 terminal for `0xE4`, R39 declared for `0xEB`.** `sound+0x7714` (`0xE4`)
is requested only post-gate inside R16's window (the `0x37640`/`0x37774`/
`0x37898` callbacks after `+0x42` b5 / `+0x43` b3), which no shipped route
reaches. For `konsd+0x1CCB0` (`0xEB`) the bounded link check from the shipped
bytes and the port's decoder: `0x24508`'s only reference is the `D100` opcode
word + the `0x24508` dword at `0xE50E0` (the `0x11` indirect call: `anim_operand`
mode 0x4000 loads `0x24508` from `0xE50E2` into `DS_00105BD4`, the case-0x11 arm
calls it with the word at `0xE50E6` = 0, `anim_indirect` → `anim_code_24508` →
`sound_voice(0xEB)` at `0x24559`). The instruction is inside **BLIZZARD's
reaction-0x34 stream** `0xC92D8[1] = 0xE50D2` (callback `0x37898`): the
stream's first word `40 ED` is opcode `0x0D`, and `spawn_anim_opcode` returns
1 there (`actors.c:2361`), so `actors_anim_begin` stops before the `0x18` hold
at `0xE50D8` — the call site is in the stream's bytes but the walk does not
demonstrate reaching it. The reaction-0x33 stream
`0xC92B0[1] = 0xE5038` does **not** contain the instruction (the two streams are
distinct table entries), so the old "`0xE5038`→`0xE50E0`" wording was wrong.
No capture carries `r0/r1 = 0x32`/`0x34` (only `0x33`, 640-641 S rows), so no
shipped input exhibits the 0x34 reaction: `0xEB` is **declared** (R39) with
that measured no-route reason. No audio section is claimed.

## 5. The gate

`PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify` exit 0 (third
run; the first two found a tooling and a comment issue, below): gate-check
46/47 + pin `c6d56fa3…` (re-stamped to `e66d8e24…` by the round-2 fix); diff-verify `1033/1033`, `14993/14993`, 0 named gaps,
`825/845` with callees, `810/845` full, 8 unrowed; entry-triage 736/388; the
C/tool/harness suites green; both new oracles green; `1304 globals, 1206
functions -> symbols.h`; `all checks passed`. `make audio-render` twice:
title `515b7c94…`/2,386,412 B, selmo `530a9430…`/peak 2790, unchanged. Coverage
re-measured 684/71/4 (the `gp-u14-twop-win` replay adds the second round and
match result, rows already HIT; only the baseline reason line moved).

Two gate fixes, neither weakening a pin:
- The audio oracle's `audio-one` replay of the contaminated `gp-u6-moves`
  capture needs the capture's recorded bitmap; `port_script --trust-bitmap` is
  that one documented caller, while the default stays injector-derived and
  fail-closed (the two derivations agree 0/5,820 S on every clean capture).
- A new inline `/* 0x61CF6` credit in the runtime `fread` family was reworded to
  the repo's non-credit form; `port_progress` stays `798/759`.

## 6. The final review fix wave (2026-11-02)

The whole-phase review (spec compliance PASS, quality CHANGES REQUESTED) found
two Critical and five Important items; every one is addressed here or declared.

**R03's P2 input (Critical 1).** The promoted capture's P2 is not idle: the
capture has `I f=0x551 press=p2.left` / `f=0x56F release`, and 30 mode-6 S rows
carry `e2=0x2000` (`gp_twop.two_human()` reports `pressed=[921, 30]`). Every
"P2 idle" sentence now reads "the P2's only in-fight input is the scenario's
30-frame left hold at `f=0x551..0x56F`".

**R16's evidence (Critical 2).** The measured capture is
`data/k11-captures/gp-u24-u16-state-t9-r5`, not `gp-u23-u16-finisher`; §4 above
now carries its exact press/reaction frames (mode 9 `f=0x1485`, gate
`f=0x149A..0x14FC` with `s1_42=0x8830` on 99 rows, close `0x8030` at `0x14FD`,
first `r0=0x33` at `f=0x155A` (93 frames after the close), presses
`0x14E8`/`0x154C`/`0x15B0`, and the `s1_42=0x8030→0x8014` phase change at
`0x15A1`). The 110/133-frame latencies and "pressed by +10"
are withdrawn.

**R12's `0xEB` (Important 2).** The bounded check in §4 proves the call site
but not the old stream link: `0xE50E0` is in the reaction-0x34 stream
`0xC92D8[1] = 0xE50D2`, not in the reaction-0x33 stream
`0xC92B0[1] = 0xE5038`. R12 is therefore `terminal` for `0xE4` only, and the
new **R39** is `declared` for `0xEB` with the measured no-route reason (no
capture carries `r0/r1 = 0x32`/`0x34`). `tools/audio_compare.py`'s
`TRACE_NEVER_REQUESTED` text and the `UNROWED_CENSUS` `0x47370` reason are
corrected.

**The loader (Important 1).** `game_string_table_load` reads into `STRING_STAGE`
(immediately above the table scratch) and copies to `STRING_DATA` only after the
`0x33340001` magic and the length pass; a bad file now leaves the last good
table intact. A `language >= 8` bound guards the eight-dword table. The new
test (`test_game.c`, `check_localised_string`) writes an 8,600-byte bad-magic
`FRENCH.TXT` into `/tmp/pr_lang_bad_test`, then asserts the previous length
(8,564), the sentinel at +8,564 and the French id survive; it fails under the
reverted staging (`0 != 204`, `a bad-magic file leaves the previous table`).

**R30 (Important 3).** The evidence is now the seven ABC store sites
(`0x28DE7`/`0x25EC5`/`0x2955A`/`0x27DE6`/`0x28932`/`0x25B27`/`0x26B94`), the
`(b1f == 3) + 1` invariant, the join's OR-then-store and the `0x24B7C` reset;
`fighter.c:6501` and `config.c:703`/`743` are the consequences.

**The docs test (Important 4).** `check_register` now rejects any `declared`
row other than R39, with a mutation case appending a declared R99.

**R38 (Important 5).** The `closed_rows` figure is asserted by the counter
test (`825/845`, `810/845`) and the allowance-before-window-start guard is now
enforced in `gp_compare.frame_claim` with a mutation test. The remaining Task 6
items (`waitpid` retry/assert, duplicate-key rejection, the 32
`ResourceWarning` contexts, `SpanCensusTests`' docstring, the proof-completion
design §7 sentence) are enumerated in R38 as **not done; not a game claim**;
the ResourceWarnings stay named.

**The small items.** `port_script` recomputes the shared bit from the scans
still held (p1.b0/p1.start share 0x0100; a fixture proves the bit survives one
release); `k11_capture.py` now writes the `check=` lines into `session.txt`
(the existing `k11-lang` file predates it, so the docs say the checks were green
at capture time rather than that they are stored); `k11_lang.py` honours
`PR_GAME_DIR` and the Makefile passes `--exe $(GAME_DIR)/PRAGE.EXE`;
`gp-u14-twop-win`'s `.dro` joins the "outside the 29-trace inventory" audio
list; the `test_final_claims` R03 token is `gp-u14-twop-win-oracle` (not a
prefix of the `.failed` path); the `test_check_docs_claims` guard list includes
the U23-ratchet phrase; the suite labels are corrected (the 234-test
`test_diff_verify` run is the diff harness, the 45-test `test_entry_triage` run
is the entry-triage suite); design §4.2 reads 47/46; the README title's "100%
analysed" states its scope (every indexed function accounted for; the 315
runtime non-targets, the unreproducible AIL set, the 8 portable `UNROWED`
functions and the index boundary).
