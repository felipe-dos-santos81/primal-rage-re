# Track J derivations (2026-10-15)

New track-J work records here as §J1, §J2, … (plan `2026-10-15-reverse-j-closeout.md`); track G's
contracted sections stay in `2026-10-14-reverse-g-derivations.md`, and the capture-wave tasks
(Part II) record here as they land.

## §J1 — the closure rule: declared non-goals stop blocking a closed row (lane j1, task 2: done; fix round 1)

**The rule, and where it lives.** `tools/diff_verify.py`'s `closed_rows` (the closure test the counter
line prints) now counts a row closed when it is VERIFIED and every callee is VERIFIED by its own row
*or* is an **allow-mode** callee outside the portable set. The callee's bytes already run on both sides
inside the row (record §E3.6's "the callee already runs on both sides inside the row — gives no
credit"), its arrival is compared through the caller's state, and no row of its own can exist because
the address falls in one of the three declared non-goal classes **and** the port tree does not
implement it:

1. `runtime` — at or above `RUNTIME_BASE` (0x5D000): the WATCOM libc / DOS-4GW region the port serves
   from the host libc and does not port (`tools/port_progress.py`'s cut, AGENTS.md);
2. `host-owned` / `deferred` — the class `tools/port_classification.txt` gives the address (every line
   there carries the record section that proves it);
3. `e2-field` — an address `port/decomp/prage.functions.csv` never lists as a function (E2's triage
   field; the census's "call targets never listed as functions in the decomp index", record G §G3).

The classes come from those two repo tables (`NonPortable` / `non_portable()`), never a hardcoded
address list. A row blocked on a **ported** function with no row of its own keeps the row open whether
it is one of the 8 named gaps (`0x1B120 0x1B3AC 0x1BEC4 0x1C930 0x1CF40 0x20C10 0x47370 0x4FBA2`,
measured ported + unrowed, and callees of no row) or a port implementation the decomp index never
listed.

**The port-code check (fix round 1; the finding it closes).** The classes alone were not enough: an
address can be outside the decomp index (`e2-field`) and still have a C body in `port/src`, in which
case a row of its own *can* exist and excusing it would be a false closure. `NonPortable.ported` is
the port tree's own registry — the addresses `port/src`'s `.c`/`.h` files (symbols.h excluded) give to
`fn_register` or head with a column-0 `/* 0xADDR` comment, the strict form `tools/entry_triage.py`'s
`load_ported` uses — and `excused()` refuses any address in it. It is load-bearing for exactly two
addresses today: **`0x29D04`** and **`0x3E484`** are E2-class handler entries the port implements
(`fighter_29d04`, `fighter_3e484`; `fn_register`ed by `port/src/game/actors.c:457`/`:362`) and that
no row covers, so the five rows **`fighter_19020`** (blocked on `0x3E484` alone),
**`fighter_35050`** and **`fighter_think_side`** (on `0x29D04` alone), **`fighter_pass_a`** and
**`fighter_pass_a_al`** (on both) stay open instead of closing on them. `0x28D68` and `0x3E4C4` are
the same shape (`fn_register`ed, unrowed, in allow use) but their rows block on a stub as well, so the
count does not move for them. Measured variants of the check: `fn_register` ∪ column-0 headers
**774/832** (adopted; the two sets are `fn_register` 425 addresses ⊆ the 1250 column-0 headers, 1201
of the union below the runtime base and 49 at or above), the loose "anywhere in a comment" header form
748/832 — not adopted, because it reads prose mentions as implementations (it would also take memset
`0x61A70` and the exit chain `0x62003` and re-open 25 further rows).

**Why the allow mode, and not every mode.** The plan's Step 1 sentence and recon-01 §6 item 4 both
name the *allow-mode* callee, and §G3 words the amendment "runtime/gap callees that are run from bytes
in allow mode". A recorded *stub* outside the portable set still keeps its row open, and that is the
honest reading: on a stub the original's bytes do not run on either side — the row applies the stub's
declared `eax`/`writes`/`clobbers`, and that declaration is exactly what a row of the callee's own
would verify. Counting such a row closed would be the metric inflation the design's anti-goal names.
The recorded `real` mode is the recorded variant of allow (it runs the bytes *and* compares the
arrival), but `E.Call`'s contract says a real callee "is proven by its own check"; a real-mode callee
with no row is an unmet declaration, so it keeps the row open too. Measured alternatives, for the
record (the same replay, `closed_rows` over `SPECS`, with the port-code check in force): allow-mode
only **774/832** (the rule as adopted), allow + real 776/832, every mode 782/832 (the port-code check
prunes that reading too: a stubbed or real implementation of a ported address keeps blocking), stubs
only 719/832 (a control, not a proposal). The every-mode reading would also drain the grunt batch:
design goal 5's rows for the 13 handler-table entries close **19** of the 58 rows still open under the
adopted rule (measured, the four port-code entries among the 13 included).

**The tables the classes are read from (measured).** `tools/port_classification.txt` holds 67 entries,
all below the runtime base and none of them ported (no `/* 0xADDR` header, no `fn_register`), so no
line is stale against the port tree; 65 are `symbols.h` `FN_` addresses (the ones `port_progress.py`
drops from the portable denominator) and two (`0x10604`, `0x1BDF4`) are outside the decomp index
altogether. `port/decomp/prage.functions.csv` holds 1207 function entries (rows
without `::`), of which 4 (`0x6FE85 0x7068E 0x70D1B 0xE4F45`) are not in `port/src/symbols.h`'s 1203
`FN_` addresses — all four are at or above 0x5D000, so below the runtime base the two function sets
are equal and the class-3 test does not depend on which table is read (measured).

**Baseline (lane head `9599e8be`, before the change).** Full self-check:
`python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE --image <tmp>
--self-check --table <tmp>` →
`1007/1007 functions VERIFIED; 14900/14900 mutants detected; 1 named gaps; 716/832 rows with callees
closed (175 have none)`. The static closed-rows replay (import `tools.diff_verify`, `closed_rows` over
the `SPECS` call sets with every spec's own entry VERIFIED) reproduces it exactly: 716/832, 116 open,
175 without callees — the replay and the run agree, so the classification below is the run's table.

**After the rule (with the port-code check).** The same command re-run →
`1007/1007 functions VERIFIED; 14900/14900 mutants detected; 1 named gaps; 774/832 rows with callees
closed (175 have none). The closure rule (record J §J1): an allow-mode callee outside the portable set
- runtime >= 0x5D000, host-owned/deferred in tools/port_classification.txt, or an address the decomp
index never lists as a function and the port tree does not implement (no fn_register, no `/* 0xADDR`
header) - does not keep a row open, while a recorded stub or real callee and a ported function without
a row do.` The function and mutant figures and the per-row table are byte-identical to the baseline's
(`diff` of the two `--table` files is empty): the change moves only the closure test, nothing any row
compares. Closed 716 → 774: **58 rows**, by the only-open-callee class they had — 49 rows on runtime
allow-mode callees alone, 4 on E2-field + runtime, 3 on host-owned + runtime, 1 on E2-field alone and
1 on host-owned alone (the 63-row reading before the port-code check minus the five rows `0x29D04`/
`0x3E484` keep open). 43 distinct callee addresses were excused of those 58 rows: 20 runtime
(`0x5D218 0x5D28F 0x5D812 0x61A4C 0x61A70 0x653A1 0x653ED 0x65490 0x654C7 0x65533 0x65546 0x655FF
0x65624 0x6C8EB 0x6CB6C 0x6CC9F 0x6CCFA 0x6CE84 0x72CD6 0x72D20`), 19 E2-field (the renderer family
`0x51E58 0x5215C 0x521F0 0x5222C 0x529A0 0x529C4 0x52A18 0x57F80 0x57FFB 0x58564 0x58578 0x5859C
0x58CBD 0x58D20 0x58D2C 0x58D40 0x58D4C 0x58D90` and the hook `0x412EC`), 4 host-owned (`0x10D0C
0x10D34 0x4FB98 0x501A3`).

**The 58 rows that stay open (32 distinct blockers, measured).** 17 runtime stubs (`0x65431` 11 rows,
`0x5DD03` 10, `0x5DEED` 7, `0x5DCC5` 6, `0x5DECA` 5, `0x5DC0F`/`0x5DC8B` 4 each, `0x5DEAF` 3,
`0x6C4CF` 2, and one each for `0x5D86A 0x5DC2A 0x5DC4D 0x5DC70 0x5DCA6 0x5DCE4 0x61A70 0x65546`),
9 E2-field stubs (`0x40CB0` 8, `0x24568` 7, `0x24804` 6, `0x29D04`/`0x2CB74` 2 each, and one each for
`0x25FAC 0x26194 0x3D0BC 0x4F5C8`), 1 host-owned stub (`0x1C308`, `sound_buffers_alloc`), the recorded
`real` mode `0x5D812` (8 rows: the `frontend_mode_15..1b_step` / `attract_scene_tick` dispatch rows
whose `fn_resolve` hook records the arrival) and the four **port-code** allow entries (`0x28D68` 6
rows, `0x3E484` 3, `0x29D04` 2, `0x3E4C4` 1). No row blocks on a ported function in the portable set:
the 8 named-gap functions are callees of none (measured), and a ported address blocks only where a row
of its own can exist — the port-code entries above and the runtime-region ported functions that are
stubbed (`0x65431 0x5DC0F 0x61A70 0x62003` family). §G3's 116-row census was the pre-rule reading (96
with a ≥0x5D000 blocker, 17 E2-class, none on the six); the rule closes 58 of the 116 and the other 58
are the blockers above.

**The mutation evidence (both directions).** The unit test
`CallParseTests.test_an_allow_mode_non_goal_does_not_keep_a_row_open_a_stub_or_a_named_gap_does` pins
the derivation against the repo's tables (`0x501A3` host-owned, `0x24568` e2-field, `0x29D04`
e2-field *and* ported, `0x1B120` a named gap → None, `0x255CC` a porting target → None, the
`RUNTIME_BASE - 1` / `RUNTIME_BASE` boundary) and the closure test in both directions: an allow-mode
blocker in an excluded class **closes** the row, the same blocker in `stub` mode, a named-gap `allow`
blocker and a port-code `allow` blocker all **keep it open**; and the three flips move it back —
dropping the class entry, dropping `0x29D04`/`0x3E484` from the port registry, or adding a named-gap
callee. Five mutations of `tools/diff_verify.py` each fail that test: the rule switched off, the mode
test dropped, the decomp index ignored, the classification table ignored, and the port-code check
ignored. At the real-tree level the replay moves the same way: with `0x501A3`'s line deleted from
`tools/port_classification.txt` the counter is 772/832 and the rows that open are
`config_screen_wait` and `config_screen_wait_zero` (their other blocker `0x655FF` is runtime allow, so
`0x501A3` was load-bearing for them); with all six port-tree declarations of `0x29D04`/`0x3E484`
removed (their four column-0 headers marked, their two `fn_register` calls deleted from
`port/src/game/actors.c`) the counter is **779/832** and `fighter_19020`, `fighter_35050`,
`fighter_pass_a`, `fighter_pass_a_al` and `fighter_think_side` close — the port-tree evidence is
exactly what keeps them open (removing only the two `fn_register` calls moves nothing: the headers
carry the same evidence, measured); a `0x1B120` allow callee injected into `config_screen_wait` gives
773/832 with that row open again. All three files were restored to their committed bytes (sha256
checked against `HEAD`).

**The weaker list, re-measured (the plan's secondary number).** 12 of the 739 rowed functions are
weaker, 727 full rows: the mechanically measured components are the rows that poke an instruction —
`0x2CADC` (`0x2CAF3/0x2CB5F/0x2CB69`, the omitted calls), `0x1BE30` (`0x4FB9E`, the `int 0x10`) and
`0x1C740` (`0x1C752/0x1C76C`) — and the rows that declare a terminal difference — `0x1AFE8` (case l6)
and `0x2EA68` (all cases). The other seven are the blocks-left-out rows the records name: `game_loop`
(18/20), `0x1B084` (7/8), `0x24C5C` (ten rows; its widest, `game_frame_keys`, 62/161), `0x2EA78`
(20/23), `0x2EA74` (20/23), `0x1B544` (2/11) and `0x2EA30` (7/8) — `0x1C740` is in both lists, so
the union is the 12. The one correction is track G g4's: `game_loop` no longer runs under pokes (they
became recorded stubs on `0x5D7DC`'s VERIFIED row), so its weaker reason is now its two named unhits
`0x25680`/`0x256D4` (18/20 blocks); the count is unchanged at 12 of 739 (727 full), and `0x51ED8`'s
one declared difference (`+0x14`/`+0x38`, exact on both sides, §H.3) is still carried by a row counted
full.

**What the rule does not claim.** It adds no coverage: an excused callee still has no cases, no
mutants, no block scan and no claim of its own; the row's claim stays "equivalence on the exercised
blocks and inputs only", now with the callee running real bytes inside it rather than needing a row it
can never have. The 58 still-open rows keep the stub-declaration and port-code gaps visible, and the
weaker list above is what the counter still does not distinguish. No oracle, `.idx` line or gp pin can
move (nothing in `port/src` changed; the diff-verify table is byte-identical).

**Commands (all on lane `reverse-j1`, base `9599e8be`, fix round on `60e0f4f6`).**
`python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE --image <tmp>
--self-check --table <tmp>` before (716/832) and after (774/832), exit 0, the two tables byte-identical.
`python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify` for the harness suite and
the new test; `PR_ORACLE_REQUIRED=1 ./build/run_tests` for the port suite. The per-function self-check
on the affected rows reads the port-code callee unverified in the callee column, e.g.
`| fighter_19020 | 0x19020 | 5 | 5/5 | VERIFIED | 18BD4 allow unverified, 18C14 stub unverified,
33950 allow unverified, 3E484 allow unverified |` and
`| fighter_35050 | 0x35050 | 4 | 4/4 | VERIFIED | 29D04 allow unverified, 2A17C stub unverified |`
(each 1/1 functions VERIFIED with all its mutants detected). The class-flip, registry-flip and
named-gap-injection measurements are scratch scripts over the same static replay (not committed).

**The per-row classification table (before → after).** All 116 pre-rule open rows, their open callees
with mode, class and whether the port implements the address, and the outcome under the rule.
| row | entry | its open callees (mode, class; ported = the port implements it) | after the rule |
|---|---|---|---|
| `actor_spawn` | 0x2AE14 | 0x5D812 allow (runtime) | closed |
| `snd_music_stop` | 0x1CA6C | 0x5DEAF stub (runtime, ported) | open on 0x5DEAF/stub |
| `snd_sample_playing` | 0x1CE70 | 0x5DD03 stub (runtime, ported) | open on 0x5DD03/stub |
| `snd_samples_stop_all` | 0x1CD9C | 0x5DC0F stub (runtime, ported); 0x5DC8B stub (runtime, ported); 0x5DD03 stub (runtime, ported) | open on 0x5DC0F/stub, 0x5DC8B/stub, 0x5DD03/stub |
| `snd_sample_stop` | 0x1CE04 | 0x5DC0F stub (runtime, ported); 0x5DC8B stub (runtime, ported); 0x5DD03 stub (runtime, ported) | open on 0x5DC0F/stub, 0x5DC8B/stub, 0x5DD03/stub |
| `snd_sample_queue` | 0x1CC28 | 0x5DC0F stub (runtime, ported); 0x5DC8B stub (runtime, ported); 0x5DD03 stub (runtime, ported) | open on 0x5DC0F/stub, 0x5DC8B/stub, 0x5DD03/stub |
| `set_dead` | 0x2B150 | 0x5D812 allow (runtime) | closed |
| `snd_music_playing` | 0x1CA40 | 0x5DEED stub (runtime, ported) | open on 0x5DEED/stub |
| `fighter_379c4` | 0x379C4 | 0x5D812 allow (runtime) | closed |
| `text_number_core` | 0x2EF24 | 0x65546 stub (runtime) | open on 0x65546/stub |
| `text_number_format` | 0x2EFD4 | 0x61A70 stub (runtime) | open on 0x61A70/stub |
| `text_glyph_emit` | 0x2F5A0 | 0x5D812 allow (runtime) | closed |
| `fighter_19020` | 0x19020 | 0x3E484 allow (e2-field, ported) | open on 0x3E484/allow |
| `fighter_19164` | 0x19164 | 0x61A4C allow (runtime) | closed |
| `fighter_slots_reset` | 0x34978 | 0x654C7 allow (runtime) | closed |
| `fighter_35050` | 0x35050 | 0x29D04 allow (e2-field, ported) | open on 0x29D04/allow |
| `frontend_mode12_advance` | 0x4160C | 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `frontend_mode_18_step` | 0x4F6E8 | 0x28D68 allow (e2-field, ported); 0x5D812 real (runtime) | open on 0x28D68/allow, 0x5D812/real |
| `frontend_mode_19_step` | 0x4F704 | 0x28D68 allow (e2-field, ported); 0x5D812 real (runtime) | open on 0x28D68/allow, 0x5D812/real |
| `actor_cursor_reset` | 0x38B70 | 0x654C7 allow (runtime) | closed |
| `camera_bitplane_pixel` | 0x15F48 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `nameentry_key` | 0x20860 | 0x653ED allow (runtime) | closed |
| `palette_list_init` | 0x336C0 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `sound_music_pause_toggle` | 0x1D1B0 | 0x5DEAF stub (runtime, ported) | open on 0x5DEAF/stub |
| `sound_music_volume` | 0x1CAB8 | 0x5DECA stub (runtime, ported); 0x5DEED stub (runtime, ported) | open on 0x5DECA/stub, 0x5DEED/stub |
| `sound_sfx_volume` | 0x1CED4 | 0x5DCC5 stub (runtime, ported); 0x5DD03 stub (runtime, ported) | open on 0x5DCC5/stub, 0x5DD03/stub |
| `sound_buffers_alloc` | 0x1D0BC | 0x1C308 stub (host-owned); 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | open on 0x1C308/stub |
| `fight_stage_marks_clear` | 0x4246C | 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `fight_char_team_tag_add` | 0x4408C | 0x5D812 allow (runtime) | closed |
| `fight_scene_props` | 0x412A0 | 0x412EC allow (e2-field); 0x5D812 allow (runtime) | closed |
| `attract_config_volumes_unscaled` | 0x2C8F0 | 0x5DCC5 stub (runtime, ported); 0x5DD03 stub (runtime, ported); 0x5DECA stub (runtime, ported); 0x5DEED stub (runtime, ported) | open on 0x5DCC5/stub, 0x5DD03/stub, 0x5DECA/stub, 0x5DEED/stub |
| `attract_config_volumes` | 0x2C8F0 | 0x5DCC5 stub (runtime, ported); 0x5DD03 stub (runtime, ported); 0x5DECA stub (runtime, ported); 0x5DEED stub (runtime, ported) | open on 0x5DCC5/stub, 0x5DD03/stub, 0x5DECA/stub, 0x5DEED/stub |
| `game_mode_25_reveal` | 0x4EF8C | 0x65624 allow (runtime) | closed |
| `flow_26d4c` | 0x26D4C | 0x65533 allow (runtime); 0x65546 allow (runtime); 0x6C8EB allow (runtime); 0x6CB6C allow (runtime); 0x6CC9F allow (runtime); 0x6CCFA allow (runtime); 0x6CE84 allow (runtime); 0x72CD6 allow (runtime); 0x72D20 allow (runtime) | closed |
| `frontend_mode_15_step` | 0x4F24C | 0x5D812 real (runtime) | open on 0x5D812/real |
| `frontend_mode_16_step` | 0x4F2B0 | 0x28D68 allow (e2-field, ported); 0x5D812 real (runtime) | open on 0x28D68/allow, 0x5D812/real |
| `frontend_mode_17_step` | 0x4F318 | 0x28D68 allow (e2-field, ported); 0x5D812 real (runtime) | open on 0x28D68/allow, 0x5D812/real |
| `frontend_mode_1b_step` | 0x4F9C8 | 0x28D68 allow (e2-field, ported); 0x5D812 real (runtime) | open on 0x28D68/allow, 0x5D812/real |
| `fight_char_text_clear` | 0x432EC | 0x65624 allow (runtime) | closed |
| `fight_char_text_clear_rr` | 0x432EC | 0x65624 allow (runtime) | closed |
| `fight_char_text_blink` | 0x43464 | 0x65624 allow (runtime) | closed |
| `fight_char_text_blink_rr` | 0x43464 | 0x65624 allow (runtime) | closed |
| `fighter_winner_body` | 0x193B0 | 0x29D04 stub (e2-field, ported); 0x3E4C4 allow (e2-field, ported) | open on 0x29D04/stub, 0x3E4C4/allow |
| `fighter_state_350d0` | 0x350D0 | 0x29D04 stub (e2-field, ported) | open on 0x29D04/stub |
| `fighter_state_3531c` | 0x3531C | 0x3D0BC stub (e2-field) | open on 0x3D0BC/stub |
| `camera_y_clamp` | 0x1317C | 0x61A4C allow (runtime) | closed |
| `fighter_think_side` | 0x3B464 | 0x29D04 allow (e2-field, ported) | open on 0x29D04/allow |
| `game_mode_05_step` | 0x25C88 | 0x24568 stub (e2-field, ported); 0x24804 stub (e2-field, ported); 0x40CB0 stub (e2-field, ported) | open on 0x24568/stub, 0x24804/stub, 0x40CB0/stub |
| `game_mode_05_step_reread` | 0x25C88 | 0x24568 stub (e2-field, ported); 0x24804 stub (e2-field, ported); 0x40CB0 stub (e2-field, ported) | open on 0x24568/stub, 0x24804/stub, 0x40CB0/stub |
| `sound_sample_start` | 0x1CB18 | 0x5DC0F stub (runtime, ported); 0x5DC2A stub (runtime, ported); 0x5DC4D stub (runtime, ported); 0x5DC70 stub (runtime, ported); 0x5DCA6 stub (runtime, ported); 0x5DCC5 stub (runtime, ported); 0x5DCE4 stub (runtime, ported) | open on 0x5DC0F/stub, 0x5DC2A/stub, 0x5DC4D/stub, 0x5DC70/stub, 0x5DCA6/stub, 0x5DCC5/stub, 0x5DCE4/stub |
| `camera_bitplane_accum` | 0x41030 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `fighter_pass_a` | 0x1958C | 0x3E484 allow (e2-field, ported) | open on 0x3E484/allow |
| `fighter_pass_a_al` | 0x1958C | 0x3E484 allow (e2-field, ported) | open on 0x3E484/allow |
| `fight_char_team_text_blink` | 0x44638 | 0x65624 allow (runtime) | closed |
| `fight_char_team_text_blink_rr` | 0x44638 | 0x65624 allow (runtime) | closed |
| `actors_reset_al` | 0x2BAF4 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `config_key_name` | 0x3157C | 0x65533 allow (runtime); 0x65546 allow (runtime); 0x6C8EB allow (runtime); 0x6CB6C allow (runtime); 0x6CC9F allow (runtime); 0x6CCFA allow (runtime); 0x6CE84 allow (runtime); 0x72CD6 allow (runtime); 0x72D20 allow (runtime) | closed |
| `game_coin_divert` | 0x257A4 | 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `camera_point_side` | 0x1790C | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `camera_point_side_real` | 0x1790C | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `camera_point_side_sync` | 0x1790C | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `camera_unfreeze` | 0x170A0 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `camera_unfreeze_real` | 0x170A0 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `camera_unfreeze_sync` | 0x170A0 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `camera_projectile_step` | 0x17CB0 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `camera_projectile_step_rr` | 0x17CB0 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `camera_point_hit` | 0x17D30 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `render_list_blit` | 0x14328 | 0x51E58 allow (e2-field); 0x5215C allow (e2-field); 0x521F0 allow (e2-field); 0x5222C allow (e2-field); 0x529A0 allow (e2-field); 0x529C4 allow (e2-field); 0x52A18 allow (e2-field); 0x57F80 allow (e2-field); 0x57FFB allow (e2-field); 0x58564 allow (e2-field); 0x58578 allow (e2-field); 0x5859C allow (e2-field); 0x58CBD allow (e2-field); 0x58D20 allow (e2-field); 0x58D2C allow (e2-field); 0x58D40 allow (e2-field); 0x58D4C allow (e2-field); 0x58D90 allow (e2-field); 0x5D218 allow (runtime); 0x5D28F allow (runtime) | closed |
| `sprite_blit` | 0x51E5C | 0x51E58 allow (e2-field) | closed |
| `fight_char_screen_open` | 0x43738 | 0x5DCC5 stub (runtime, ported); 0x5DD03 stub (runtime, ported); 0x5DECA stub (runtime, ported); 0x5DEED stub (runtime, ported) | open on 0x5DCC5/stub, 0x5DD03/stub, 0x5DECA/stub, 0x5DEED/stub |
| `fight_char_screen_open_both` | 0x444C8 | 0x5DCC5 stub (runtime, ported); 0x5DD03 stub (runtime, ported); 0x5DECA stub (runtime, ported); 0x5DEED stub (runtime, ported) | open on 0x5DCC5/stub, 0x5DD03/stub, 0x5DECA/stub, 0x5DEED/stub |
| `attract_scene_tick` | 0x292AC | 0x5D812 real (runtime) | open on 0x5D812/real |
| `frontend_mode_1a_step` | 0x4F9A0 | 0x28D68 allow (e2-field, ported); 0x5D812 real (runtime) | open on 0x28D68/allow, 0x5D812/real |
| `hiscore_insert` | 0x2DCA0 | 0x653A1 allow (runtime) | closed |
| `menu_step` | 0x2FFC4 | 0x2CB74 stub (e2-field, ported) | open on 0x2CB74/stub |
| `menu_run` | 0x2FA40 | 0x2CB74 stub (e2-field, ported) | open on 0x2CB74/stub |
| `game_frame_keys` | 0x24C5C | 0x65431 stub (runtime, ported) | open on 0x65431/stub |
| `game_frame_keys2` | 0x24C5C | 0x65431 stub (runtime, ported) | open on 0x65431/stub |
| `game_frame_head` | 0x24C5C | 0x25FAC stub (e2-field, ported); 0x26194 stub (e2-field, ported); 0x5D812 allow (runtime); 0x65431 stub (runtime, ported) | open on 0x25FAC/stub, 0x26194/stub, 0x65431/stub |
| `game_frame_fight` | 0x24C5C | 0x65431 stub (runtime, ported) | open on 0x65431/stub |
| `game_frame_tails` | 0x24C5C | 0x65431 stub (runtime, ported) | open on 0x65431/stub |
| `game_frame_m25` | 0x24C5C | 0x65431 stub (runtime, ported) | open on 0x65431/stub |
| `game_frame_match` | 0x24C5C | 0x65431 stub (runtime, ported) | open on 0x65431/stub |
| `game_frame_front` | 0x24C5C | 0x65431 stub (runtime, ported) | open on 0x65431/stub |
| `game_frame_start` | 0x24C5C | 0x65431 stub (runtime, ported) | open on 0x65431/stub |
| `game_frame_rr` | 0x24C5C | 0x65431 stub (runtime, ported) | open on 0x65431/stub |
| `game_loop` | 0x255CC | 0x4F5C8 stub (e2-field, ported); 0x501A3 allow (host-owned); 0x5D812 allow (runtime) | open on 0x4F5C8/stub |
| `game_mode_32_step` | 0x296B8 | 0x24568 stub (e2-field, ported); 0x24804 stub (e2-field, ported); 0x40CB0 stub (e2-field, ported) | open on 0x24568/stub, 0x24804/stub, 0x40CB0/stub |
| `game_mode_32_step_end` | 0x296B8 | 0x24568 stub (e2-field, ported); 0x40CB0 stub (e2-field, ported) | open on 0x24568/stub, 0x40CB0/stub |
| `game_mode_32_step_rr` | 0x296B8 | 0x24568 stub (e2-field, ported); 0x24804 stub (e2-field, ported); 0x40CB0 stub (e2-field, ported) | open on 0x24568/stub, 0x24804/stub, 0x40CB0/stub |
| `game_mode_0d_step` | 0x274FC | 0x24568 stub (e2-field, ported); 0x24804 stub (e2-field, ported); 0x40CB0 stub (e2-field, ported) | open on 0x24568/stub, 0x24804/stub, 0x40CB0/stub |
| `game_mode_0d_step_end` | 0x274FC | 0x40CB0 stub (e2-field, ported) | open on 0x40CB0/stub |
| `game_mode_0d_step_rr` | 0x274FC | 0x24568 stub (e2-field, ported); 0x24804 stub (e2-field, ported); 0x40CB0 stub (e2-field, ported) | open on 0x24568/stub, 0x24804/stub, 0x40CB0/stub |
| `game_mode_13_step` | 0x424E8 | 0x412EC allow (e2-field); 0x5D812 allow (runtime) | closed |
| `svc_buttons_draw` | 0x31A78 | 0x65533 allow (runtime); 0x65546 allow (runtime); 0x6C8EB allow (runtime); 0x6CB6C allow (runtime); 0x6CC9F allow (runtime); 0x6CCFA allow (runtime); 0x6CE84 allow (runtime); 0x72CD6 allow (runtime); 0x72D20 allow (runtime) | closed |
| `config_screen_wait` | 0x2EA78 | 0x501A3 allow (host-owned); 0x655FF allow (runtime) | closed |
| `config_screen_wait_zero` | 0x2EA74 | 0x501A3 allow (host-owned); 0x655FF allow (runtime) | closed |
| `game_quit_prompt` | 0x249F0 | 0x653ED allow (runtime); 0x65431 stub (runtime, ported) | open on 0x65431/stub |
| `config_storage_read` | 0x2E990 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `config_storage_touch` | 0x2D4EC | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `config_storage_load` | 0x2D638 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `hiscore_storage_load` | 0x2DE98 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `hiscore_storage_load_rr` | 0x2DE98 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `config_storage_validate` | 0x2DF8C | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `config_storage_validate_rr` | 0x2DF8C | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `config_validate` | 0x2D6F8 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `config_validate_rr` | 0x2D6F8 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `config_validate_audit` | 0x2D6F8 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `config_validate_chain` | 0x2D6F8 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `config_file_load` | 0x1AFE8 | 0x10D0C allow (host-owned); 0x10D34 allow (host-owned); 0x61A70 allow (runtime); 0x62002 allow (runtime); 0x62003 allow (runtime); 0x6201B allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime); 0x6C4CF stub (runtime) | open on 0x6C4CF/stub |
| `config_file_save` | 0x1B084 | 0x10D0C allow (host-owned); 0x10D34 allow (host-owned); 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `menu_fatal_error` | 0x2EA68 | 0x62002 allow (runtime); 0x62003 allow (runtime); 0x6201B allow (runtime); 0x6C4CF stub (runtime) | open on 0x6C4CF/stub |
| `actors_table_clear` | 0x2F920 | 0x61A70 allow (runtime); 0x65490 allow (runtime); 0x654C7 allow (runtime) | closed |
| `sound_shutdown` | 0x1D018 | 0x5D86A stub (runtime, ported); 0x5DC8B stub (runtime, ported); 0x5DD03 stub (runtime, ported); 0x5DEAF stub (runtime, ported); 0x5DEED stub (runtime, ported) | open on 0x5D86A/stub, 0x5DC8B/stub, 0x5DD03/stub, 0x5DEAF/stub, 0x5DEED/stub |
| `sprite_blit_aperture` | 0x51ED8 | 0x51E58 allow (e2-field); 0x521F0 allow (e2-field); 0x5222C allow (e2-field); 0x529A0 allow (e2-field); 0x529C4 allow (e2-field); 0x52A18 allow (e2-field); 0x57F80 allow (e2-field); 0x58564 allow (e2-field); 0x5859C allow (e2-field); 0x58CBD allow (e2-field); 0x58D2C allow (e2-field); 0x58D4C allow (e2-field); 0x58D90 allow (e2-field); 0x5D218 allow (runtime) | closed |
| `game_shutdown` | 0x1BE30 | 0x10D34 allow (host-owned); 0x4FB98 allow (host-owned) | closed |
