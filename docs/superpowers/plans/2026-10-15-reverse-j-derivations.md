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
`0x61A70` and the exit chain `0x62003` and re-open 26 further rows — measured: all 26 are the rows
the adopted form closes via the runtime allow-mode callee `0x61A70`).

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
only 719/832 (a control, not a proposal). The every-mode reading would not drain the grunt batch:
only **1** of its 8 rows beyond the adopted rule (`fighter_state_3531c`, blocked on the handler entry
`0x3D0BC`) is among the **19** that design goal 5's rows for the 13 handler-table entries close of the
58 rows open under the adopted rule — the batch keeps 18 closes under that reading too (measured, the
four port-code entries among the 13 included).

**Review fix wave (whole-branch review, 2026-10-15).** The loose form's figure above is 26 (was 25)
and the every-mode drain claim is corrected to the 1-of-19 overlap: re-measured on this tree by the
static replay (`closed_rows` over `SPECS`, the 13 = recon-01 §3's handler/hook table entries;
every-mode 782/832 = +8, `allow+real` 776, stubs-only 719). The counter sentence reads the port-code
guard as all-class (it always was, in `NonPortable.excused` and AGENTS.md) and its exact string is
re-pinned in `tools/tests/test_diff_verify.py`; the counts do not move — every guard-blocked
allow-mode callee today is `e2-field` (`0x127C0 0x12800 0x23130 0x28D68 0x29D04 0x3E484 0x3E4C4
0x45D14 0x47E9C`), so the reword is cosmetic to the counter (774/832).

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
index never lists as a function - does not keep a row open, provided the port tree does not implement
the address in any class (no fn_register, no column-0 `/* 0xADDR` header), while a recorded stub or
real callee and a ported function without a row do.` (The counter sentence is the fix wave's all-class
reword; its numbers are the fix-round-1 run's.) The function and mutant figures and the per-row table are byte-identical to the baseline's
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

## §J2 — Host rebind: `host_key_bits` reads the configured binding scans

Task 4 of `docs/superpowers/plans/2026-10-15-reverse-j-closeout.md`. Files:
`port/src/host.c` (`host_kb_bit`, the default table kept as the fallback),
`port/src/host.h` (the two comments), `port/tests/test_platform.c` (`test_host`),
`port/spec/game_flow.md` (the one stale "default binding" sentence). The replay
seam `host_set_key_bits_override` (host.c:101-108 pre-change) is untouched and
still answers first: no driver's pad word can move.

### J2.1 The raw: where the binding scans live

The port's sampler stand-in reads the *same bytes the raw's sampler reads*: the
device-0 keyboard arms of `0x1BBAC` take their scans out of the key-config
block at `[DS_00101514]`, by position, and OR the position's bit into
`+0x2D8`/`+0x2D9` (record gameplay-ground-truth §G.1.2):

| routine | block bytes | bits (of the player's byte) |
|---|---|---|
| `0x1B610` | `+0x2DE`, `+0x2DF`, `+0x2E0`, `+0x2E1` | `0x80 0x40 0x20 0x10` |
| `0x1B730` | `+0x2E2`..`+0x2E5` | `1 2 4 8` |
| `0x1B6A0` | `+0x2E6`..`+0x2E9` | `0x80 0x40 0x20 0x10` |
| `0x1B7C0` | `+0x2EA`..`+0x2ED` | `1 2 4 8` |
| `0x1B850` | `+0x28F` (fixed) | bit 0 |
| `0x1B870` | `+0x290` (fixed) | bit 0 |

(Decompiled from the Ghidra project `primal-rage` `/PRAGE.EXE`: each routine
tests `(*(byte *)(DAT_00101514 + 0x254 + scan) & 0x80) == 0` with
`scan = *(byte *)(DAT_00101514 + <offset>)` — the block byte is the scan, and
`0x1BBAC`'s case 0 combines `0x1B610|0x1B730|0x1B850` into `+0x2D8` and
`0x1B6A0|0x1B7C0|0x1B870` into `+0x2D9`.) The kb word the port's `game_loop`
writes is `(+0x2D8 << 8) | +0x2D9` (host.h, flow.c), so the position table in
`port/src/host.c` is `k_cfg_bit[16]` = `0x8000 0x4000 0x2000 0x1000 0x0100
0x0200 0x0400 0x0800 0x0080 0x0040 0x0020 0x0010 0x0001 0x0002 0x0004 0x0008`.

The block is filled by `config_keys_apply` (`0x1AE28`, bytes `+0x2D4..+0x2ED`
from a 0x28-byte record) from either the default record at `0xA2C62`
(`0x1AE20`, called by `config_set_defaults` `0x2CADC`, the no-magic arm of
`config_validate` `0x2D6F8`) or the service menu's edited record
(record §50-C). At boot the tail of the restart point re-loads it from the
**config image's mirror**: `0x20CF0 mov eax,esp`, `0x20CF2 call 0x1AEE0`
(`config_keys_pack`, mirror `0x1014AC..` → the record), `0x20CF7 mov eax,esp`,
`0x20CF9 call 0x1AF64` (`config_keys_load`, the record → `+0x2DE..+0x2ED`),
disassembled from `/PRAGE.EXE`; the mirror sits inside the image
`0x100CE4..0x1014DB` that `config_file_load` (`0x1AFE8`) reads, at
`0x1014AE + 2i` (P1) and `0x1014C0 + 2i` (P2) — not contiguous, which is why
the binding scans are named by their *block* offsets `+0x2DE..+0x2ED`
(the port follows the block, the bytes the sampler reads; reading the mirror
directly would read a different copy the raw never samples live).

Default record bytes (obj-1 file offset = VA + 0x46E54; `0xA2C62` →
`0xE9AB6`): `00 00 73 1f 78 2d 7a 2c 63 2e 75 16 69 17 6e 31 6d 32 | 00 00
| 00 48 00 50 00 4b 00 4d 00 47 00 49 00 4f 00 51 | 64 00 64 00` — P1 scans
`1F 2D 2C 2E 16 17 31 32`, P2 scans `48 50 4B 4D 47 49 4F 51`, i.e. exactly
the default binding `host_kb_bit`'s switch encoded (record §G.1.2, §K.10
item 8).

### J2.2 The port change

`host_kb_bit(scan)` now reads the 16 scans at
`mem + DSD(DS_00101514) + 0x2DE .. +0x2ED` (whole table inside `mem[]`;
`base == 0` or `base > MEM_SIZE - 0x2EE` is treated as absent) and ORs
`k_cfg_bit[i]` for every position whose scan equals `scan` — the sampler tests
each position independently, so a scan twice in the table gets both bits —
then ORs the fixed `+0x28F` bit for `0x3B` (`0x0100`) and `+0x290` for `0x3C`
(`0x0001`). `host_key_bits()` is unchanged: it still walks the pressed SDL keys
through `k_bios_letter`/`k_bios_pad` and calls `host_kb_bit` for each.

**PORT deviation (named).** With no block (no `game_init`, or all 16 scans
zero) the function falls back to the default binding — the switch that was the
whole body before. The raw's sampler would report no pad key for an empty
block; the fallback exists because the port's headless callers and any run
whose image records no binding would otherwise have no keyboard at all. In the
windowed run the block is never empty: the no-magic path writes the default
record (`config_set_defaults`), and an image with the magic carries its own
mirror through the `0x20CF2`/`0x20CF9` pair, so the fallback is the
absent-block case only.

`host_set_key_bits_override` (host.c) still short-circuits `host_key_bits()`
before any SDL or block read.

### J2.3 Tests (`test_host`, `test_platform.c`)

Seeded, both directions, all able to fail (planting uses the suite's free
scratch `0x03F00000`; `DS_00101514` is saved and restored):

- the default pins (18 scans + three zero pins) now run with an **absent**
  block (`DS_00101514 = 0`) so they cannot depend on whatever a previous test
  left in a block (test_platform.c:2961-2967);
- a non-default table (scans `0x11..0x14, 0x21..0x24, 0x31..0x34, 0x41..0x44`
  — no default scan in it): the 16 position checks (:2990), the three
  not-in-table checks (`0x1F`, `0x48`, `0x50` → 0, :2991-2993), the fixed F1/F2
  bits (:2995-2996), the duplicate-position OR (:3000), F1 in the up slot
  (:3003) and the slot-gone re-read (:3004);
- a **zeroed** table falls back to the default (two pins, :3008-3009);
- the override seam with a non-default table planted (:3110-3121): the sentinel
  `0x1234` still wins, and is not returned with the seam off.

Evidence (all on this worktree):

| run | result |
|---|---|
| `./build/run_tests` before the change | `FAILURES: 22` — the 16 positions and 6 of the configured checks fail against the old default-only body (`0 != 32768` …) |
| `./build/run_tests` after | exit 0, `all checks passed` |
| `PR_ORACLE_REQUIRED=1 ./build/run_tests` before vs after | **byte-identical output**, `all checks passed`, `oracle C-vs-Python: 9866 writes byte-exact` (12 lines, no pinned line moved) |

Mutation proofs (each restored; `M1` = the fallback removed, `M2` = bits 0/1 of
`k_cfg_bit` swapped, `M3` = the table cached after the first call, `M4` = the
`+0x28F`/`+0x290` lines dropped, `M5` = no `base == 0` guard and the empty
table authoritative):

| mutant | failing checks |
|---|---|
| M1 | :3008, :3009 (the zeroed-table fallback) |
| M2 | :2990 ×2 (the swapped positions), :3000, :3003 |
| M3 | :3000, :3003, :3004 (stale slot), :3008, :3009 |
| M4 | :2995, :2996, :3003 |
| M5 | :2963 ×18 (the absent-block default pins) |

### J2.4 The seam and the three controlled cases

Per the controller's resolution, no gp replay/oracle ran (lane `j2` holds
`/tmp/pr_gp_dump`); the seam is proven by unit case instead:

- **(a) override set, a non-default table planted** — `host_key_bits()` returns
  the override `0x1234` (test_platform.c:3116-3117): the replays' pad words
  cannot read the configured scans.
- **(b) override unset, configured scans win** — proven through `host_kb_bit`,
  the mapping `host_key_bits()` applies to every pressed key (the planner's
  `host_kb_bit` is the sampler's per-key combine; `host_key_bits` is the SDL
  walk). The SDL side itself cannot be driven headless: `SDL_GetKeyboardState`
  returns SDL's internal all-zero array and no public SDL3 call seeds it (the
  standing named gap: no test seeds the SDL keyboard state, §K.13 item 1 /
  §K.10 item 1).
- **(c) zeroed/absent scans → defaults** — `DS_00101514 = 0` (:2963) and a
  zeroed block (:3008-3009), including `M1`/`M3` proving both can fail.

No pinned path moved, and none can: every oracle driver is headless (SDL
reports no key pressed, so the mapping is never consulted) and the two replay
drivers hold the bitmap through `host_set_key_bits_override`; the
oracle-required run's byte-identical output is the measured form of that
argument. The gp replays themselves were left to the lane that holds the
dump; `make verify` will re-measure them at integration.

### J2.5 SDL failure-branch reachability (report, no forced tests)

Exercised with a scratch probe (`host_init`/`host_audio_open` linked against
`build/libprage_core.a`, run in the OS temp dir; none of it is repo content):

| branch | reachable here? | evidence |
|---|---|---|
| `host_init` w/h guard | yes, already tested headless | `host_init("pr-test",0,0) == 0` |
| `host_init` `SDL_Init(VIDEO)` fails | **yes, deterministically** | `SDL_VIDEODRIVER=nonexistent` → `host_init -> 0  has_window=0  sdl_error="nonexistent not available"`; the windowed binary prints `prageport: no window/display available`, exit 1 |
| `host_init` success (window + surface) | yes, headless with the dummy driver | `SDL_VIDEODRIVER=dummy` → `host_init -> 1 has_window=1`, `present+pump ok` |
| `SDL_CreateWindow`/`SDL_CreateSurface` NULL | no deterministic trigger here (resource exhaustion only; the dummy driver creates a window) | not reached |
| `host_audio_open` profile guard | yes, already tested | `host_audio_open(0,2)/(44100,0)` → 0 |
| `SDL_InitSubSystem(AUDIO)` fails | **yes, deterministically** | `SDL_AUDIODRIVER=nonexistent` → `err="SDL_InitSubSystem(SDL_INIT_AUDIO): Audio target 'nonexistent' not available"` |
| `SDL_OpenAudioDeviceStream`/`SDL_ResumeAudioStreamDevice` success | yes, headless with `SDL_AUDIODRIVER=dummy`, and on this host's real driver | `host_audio_open(49716,2) -> 1 rate=49716`, submit accepted |
| `SDL_OpenAudioDeviceStream` NULL (working driver) | no deterministic trigger here | not reached |
| `SDL_ResumeAudioStreamDevice` fails | **not reproducible here today** — the standing `-66681` (`AudioQueueStart`) observation (2026-09-16 audio report) does not reproduce: this host now reports `current audio driver: coreaudio`, 3 playback devices (LC49G95T, Realtek USB2.0 Audio, `Alto-falantes (MacBook Pro)`), and open+resume succeed; the windowed binary (dummy video, scratch `--cmos`) prints `prageport: audio device open at 49716 Hz` | measured |

That last row is a corrected fact, not a change: the "SDL audio cannot open
here (-66681) / the windowed run is silent" sentence in `AGENTS.md`, PROGRESS
and the track-G/J designs is stale on this host as of 2026-10-15 (the machine
gained a working playback route since 2026-09-16). Audio bring-up stays queued
per track J's ruling — a device-capable host is now available.

### J2.6 Live-play notes (notes only)

**Keys and behaviour of the windowed run.** In a match the default binding
gives player 1 up/down/left/right on `S`/`X`/`Z`/`C`, the four buttons on
`U`/`I`/`N`/`M` and start on `F1`; player 2 takes the arrows, `Home`/`PgUp`/
`End`/`PgDn` and `F2` — and the service menu's key config now changes those
(the port follows the block the game writes, defaulting to the same list).
Menus and prompts take `Enter`, `Esc`, `Space`, `Backspace` and the letters as
BIOS words. `Esc` in the front-end asks `QUIT TO DOS? Y/N` (yes ends the run),
in a match asks `ABANDON CONQUEST? Y/N`, and does nothing in the service menu;
`Space` shows `- PAUSED -` outside modes 3 and `0x27` (another space resumes);
`Alt-Q` asks `QUIT TO DOS?` in every mode, `Alt-S` toggles the samples,
`Alt-M` the music; closing the window is the host's own quit signal and is
distinct from any key. Left alone in a menu the game reboots itself: the
blocking waits' 20-second idle timeout (0x4B0 ticks at 60 Hz, `0x2EB80`)
soft-restarts through the game's own longjmp back to the attract/title. The
title's `CREDITS:<n>` count comes from the config image at boot (field 0x29's
nibble + 1; 5 on the shipped zero image); starting a game takes one credit,
there is no coin key and no free-play key in this build (record §K.4), and the
count is per-run (the debit is RAM, not written back by the debit itself).

### J2.7 Named gaps that stay

- The SDL scancode → BIOS scan table (36 keys) is still untested headless and
  is still the only way a pressed key reaches the map: a rebind to a key
  outside `k_bios_letter`/`k_bios_pad` (Enter, digits, F3..) is not reachable
  in the windowed port (§K.10 items 1 and 7's family).
- `+0x28F`/`+0x290` (F1/F2) are not part of the 16-scan config record — the
  record's 0x28 bytes carry the two devices and the 16 key words only
  (`config_keys_apply` `0x1AE28`) — so their bits stay fixed in the port as
  the raw's fixed +0x28F/+0x290 reads are; no writer of those two block bytes
  is established here.
- Typematic repeat and the joystick device words `+0x2D4`/`+0x2D6` (a
  non-zero device switches `0x1BBAC` to the game-port arms) stay out, as
  before (§K.10 items 9 and 10).

## §J3 — the save-reload capture design (Part II, Task 7; awaiting the capture go-ahead)

Task 7 of `docs/superpowers/plans/2026-10-15-reverse-j-closeout.md:285-299`; the standing contract is
the storage plan's Task 8 (`docs/superpowers/plans/2026-10-13-reverse-s-storage-layer.md:495-511`),
whose steps 1-2 end here and whose steps 3-5 are not started. This section is the design Step 2 asks
to show to the user before the capture; it was written from reading only — no DOSBox-X run, no tooling
change. **The stale reference, one line:** the storage plan's Step 2 says "write the design in the
record (§S.9)"; §S.9 is now the run clock (storage record:1122) — the design lands here, as the
track-J plan's Task 7 directs. It would close the named gap of
`2026-10-13-reverse-s-derivations.md:1064` ("Loading or writing a real save has no recording of the
original") and its F7 (there:1107); nothing else in track S waits on it.

The capture produces `data/k11-captures/save-reload/` and later `make save-reload-oracle`. The two
claims it is built for: **write** — the port's replay of boot 1, with a scratch save path, writes a
file byte-identical to the captured `CMOS`; **load** — the port booted from the captured `CMOS` holds,
at the pinned frame, the same three polled ranges as boot 2's `poll.log`.

### J3.1 The scenario: two boots, their keys and their limits

A `save-reload` entry in `tools/k11_session.py`'s `SCENARIOS` (the scenario format: `keys`, `pokes`,
`time_limit`, `k11_session.py:71-79`) plus two designed additions: a `boot2` sub-entry and a writable
flag (J3.2). No pokes.

**Boot 1** (the run that saves). Keys in order, with what each does and its evidence:

| # | key | screen / effect | evidence |
|---|---|---|---|
| 1 | `enter` | the mode-3 Enter; mode 3 → `0x27` (MAIN MENU) | `0x24ECF..0x24EE0`; `k11_session.py:5` |
| 2 | `enter` | MAIN item 0 → START MENU (`menu_step(0xBCCCC, 0x10, 0)`) | `0x2CB74`; §K11.2:507-510 |
| 3-4 | `down down` | START MENU rows 1, 2 (`0xBCCFC`, callback `0x2CBF4`) | §K11.2:529-531 |
| 5 | `esc` | START MENU Esc → `-5`; MAIN MENU re-initialised | §K11.2:540-546 |
| 6 | `down` | MAIN item 1 = "GAME OPTIONS" highlighted | named-gaps-a:320 |
| 7 | `enter` | MAIN item 1 → OPTIONS MENU (table `0xBCC1C`); its `menu_run` return is what saves | `0x2CB94`; `svcmenu.c:86-94` |
| 8 | `enter` | OPTIONS MENU item 0 → CONFIG OPTIONS editor (table `0xA2EB4`, field `0x29`) | `0x2CACC` → `0x33578`; §K11.3:708-710, 603-616 |
| 9 | `right` | CONFIG OPTIONS record 0 CREDITS, one step (drawn index v+1) | §K11.3:603-618, 674-706 |
| 10 | `esc` | leave CONFIG OPTIONS → OPTIONS MENU; `0x33578` writes field `0x29` via `0x2DA0C` | §K11.3:705-706, 717-718 |
| 11 | `esc` | leave OPTIONS MENU (`0x2FA40` flags 0 → `-1`) → **`config_file_save()` at `0x2CBAF`** | §K11.2:547-550; `svcmenu.c:90-92` |

so `keys = 'enter enter down down esc down enter enter right esc esc'` (11 keys): its first eight are
the walk's own first eight (`k11_session.py:65`, whose comment names the same screens, and whose
screen list is named-gaps-a:318-327), plus the `right` tap, plus the two Escs — the first unwinds the
editor, the second exits the OPTIONS MENU whose return saves. CREDITS is chosen because it is the
editor's first record (one tap, no Down first), it is a stored field (record 0 of `0xA2EB4`, shift
`0x10`, §K11.3:607), and the saved value is read back at boot as `CREDITS:<n>` (field 0x29's nibble
+ 1; record §J2.6, this record:483). Every key is already in the harness vocabulary: `right` is
`(0x4D, 0x00)` in `KEYS` (`k11_session.py:60-61`) with BIOS word `0x4DE0` (`k11_capture.py:59-60`,
"left/right follow the same grey-key E0 form"). The walk's own final `esc esc` left the OPTIONS MENU
under a read-only mount (screens 165/166 HANDICAP, 168 OPTIONS MENU, 170 the final MAIN MENU,
named-gaps-a:326-327), so the save path was exercised — and dropped — by an existing capture; nothing
wrote `data/`.

Timing: boot 1 `time_limit=55` s (DIGS/DE's value, `k11_session.py:75-78`). `ENTER_WAIT` 25 s puts the
Enter inside the boot attract (logos end ~19 s, title ~31 s; `k11_session.py:41`), the last key lands
at ~35 s, and the port script's `end` adds `END_TAIL_TICKS` 180 ticks (`k11_session.py:43`). The E
record's `reason=time-limit` is expected (the emulator's limit ends the run, as the walk's E record
at named-gaps-a:304-305).

**Boot 2** (the run that loads). `('enter',)`, `time_limit=45` s: one mode-0x27 Enter and stop. It must
not leave the OPTIONS MENU (that would re-save and move the target) and must not change any option;
nothing in boot 2 is pixel-claimed.

### J3.2 The staging copy and the two mounts

`k11_capture.py` stages `root/C` (temp) through `title_capture.stage`: **every game-dir entry is
symlinked except `PRAGE.EXE`, which is copied** (`title_capture.py:52-68`), and mounts it
`MOUNT C "<game>" -ro` (`k11_capture.py:102`) so the original's save cannot write; the tool's docstring
says the staging "mounts read-only, so the original's CMOS save (0x1B084) cannot write"
(`k11_capture.py:11-13`), and `check_cmos` requires the game dir's `CMOS` absent or 2040 zero bytes
(`CMOS_BYTES`, `k11_capture.py:41`, `:82-91`), which `make verify` runs first (Makefile:1062-1063).

For `save-reload` boot 1:
- `check_cmos` runs unchanged (an operator's non-zero `CMOS` still aborts) — the rule stays enforced
  for **every** scenario, with the new test of J3.7(b) saying so.
- after `stage`, the staging `CMOS` must be a **real file, never a symlink into `data/`**: if
  `root/C/CMOS` is a symlink (the game dir's zero file), remove the link and write a real copy of its
  bytes; if the game dir has none, leave it absent. Either way boot 1's starting bytes are exactly the
  game dir's — which is also what the port's fallback reads (J3.5) — and the condition is recorded as
  `cmos=zero|absent` in `session.txt` (`k11_capture.py:330-331`; all 22 existing captures say
  `cmos=zero`, storage record:109-110; the present file is 2040 zero bytes, `data/game/C/CMOS`).
- boot 1 mounts **without** `-ro` (the one scenario that does), so `fopen("cmos","wb")` succeeds and
  the file lands in `root/C`. `guard_out` (`k11_capture.py:63-68`) still confines `--out` to
  `data/k11-captures`; nothing writes `data/`.
- after boot 1 exits: copy `root/C/CMOS` → `out/CMOS`; CHECK it is 2040 bytes, that its last byte is
  the XOR of the first `0x7F7` (the save file's invariant, `config.c:1353-1356`, `:1402-1405`), and
  that it differs from the staged copy (the change reached the file; a save that wrote the untouched
  defaults would abort the capture as a failed scenario).
- boot 1's `poll.log` is kept as `out/boot1/poll.log` (the write replay's script source); its frames
  are not converted (no pixel claim exists for boot 1).

Boot 2 reuses the same staging `root/C` — the file the save wrote is the file the load reads — but
mounts it `-ro` again: boot 2 has no save to make, and read-only makes a stray write attempt
impossible, the condition every existing capture ran under. Boot 2 gets its own AVI dir, memory file
and `poll.log` (the existing `dosbox_cmd` names these under its `root`, `k11_capture.py:94-107`) and
its frames are converted into `out/` as usual. The capture dir ends:

| file | what |
|---|---|
| `CMOS` | boot 1's save (2040 bytes): the write claim's target and the load claim's input |
| `poll.log` | boot 2's poll log |
| `frame_%04d.raw`, `window.txt` | boot 2's collapsed frames (`title_capture.write_window`, `k11_capture.py:310-312`) |
| `boot1/poll.log` | boot 1's poll log (the write replay's script source) |
| `session.txt`, `dosbox.log`, `last_frame.png`, `prage_*.dro` | the existing artefacts; `session.txt` records both boots |

The no-stray-input rule (AGENTS.md: nobody may type in the DOSBox-X window while a capture runs) holds
for both boots. The K11 equivalent of `gp_capture check-input` is the existing script check: each poll
log must validate through `k11_session.port_script` against the scenario's keys
(`k11_session.py:126-142`, `ScriptError` on any unscripted K record), which the capture tool runs
before writing `out/`. Its blind spots are `gp_capture`'s documented ones (a stray press inside a
scripted hold window, a key-up of a non-injected key, a tap between records): named in J3.8.

### J3.3 The three polled ranges and the poll.log format

The three ranges are the storage families the boot chain loads, located from the same raw tables the
game's own functions read (nothing fitted). All file values below were read at obj-0 file offset
`VA + 0x52E54` (`k11_fields.py:23`; the descriptor dwords are fixup targets, so the raw value is the
object-relative offset — `mem.c:290-297`, `:332-334`: the loader writes object base + target offset):

| id | DS range | bytes | what | raw evidence |
|---|---|---|---|---|
| `cfg` | `0x100CE4..0x1014DB` | `0x7F8` | the config image `config_file_load`/`config_file_save` read/write | `[0x2D48C] = 0x100CE4` (file value `0x80CE4`); len `0x7F8` (`0x1B006`/`0x1B0CA`); §S.1:12-14; `config.c:1348-1351` |
| `k0` | `0x105DB0..0x105DD5` | `0x26` | the 38 bookkeeping counters' low bytes (kind 0) | kind-table entry 0 at `0x2D444` = `{cell 0, len 0x26, source 0x85DB0}`; §S.1:27; §S.10:1196-1198 |
| `hs0` | `0x105E34..0x105EAB` | `0x78` | high-score table 0: 10 records × (4 value + 8 name) | `0x2D3FC[0] = {10, 0x0A, 4, 8}`; `0x2D478[0] = 0x85E34`; kind entry 6 at `0x2D474` = `{cell 0xF6, len 0x78, source 0x85E34}` |
| `hs1` | `0x105EAC..0x105EC7` | `0x1C` | high-score table 1: 1 × (4 + 24) | `0x2D3FC[1] = {1, 1, 4, 0x18}`; `0x2D478[1] = 0x85EAC`; kind entry 7 (`0x2D47C`) |
| `hs2` | `0x105EC8..0x105ECC` | `5` | high-score table 2: 1 × (3 + 2) | `0x2D3FC[2] = {1, 1, 3, 2}`; `0x2D478[2] = 0x85EC8`; kind entry 8 (`0x2D484`) |

The `hs*` layout and the descriptor semantics are `0x2DB58`'s own: §S.1:42-43 ("0x2D45C is the
table's entry 3 … and 0x2D474 its entry 6 … data, not code"), port `config.c:467-480`
(`hiscore_locate` reads `DSD(0x2D478 + 8*table)` and the `0x2D3FC` descriptor; §S.5.3:283), and the
harness's independent statement agrees byte-for-byte (`tools/diff_verify.py:11429-11441`:
`D1I_HS_DESC 0x2D3FC`, `D1I_HS_BASE 0x2D478`, default bases `0x105E34/0x105EAC/0x105EC8`, descs
`(10,4,8) (1,4,0x18) (1,3,2)`).

**How the poller reads them** (a change to `k11_capture.Poller` and to `poll.log`'s format,
`k11_session.py:9-18`):
- when `find_base` has the data base (`k11_capture.py:71-79`), each range's runtime address is
  `base + <object-relative offset>` with the file values just quoted (`0x80CE4`, `0x85DB0`, `0x85E34`,
  `0x85EAC`, `0x85EC8`). No code-object base is needed — the `kf.load_descriptors` pattern
  (`k11_fields.py:26-29`, code VA `0x2D300 + 4*field` from the EXE) applied to the storage tables.
- new record kinds (the docstring's format list gains them): `L ms=<int> base=<hex8>
  id=<cfg|k0|hs0|hs1|hs2> at=<hex8> len=<dec>`, once per range when the base is found (a
  self-describing locate), and `R ms=<int> tick=<hex8> id=<...> img=<hex>` whenever the range's bytes
  change — the F record's shape for the field window (`k11_capture.py:244-248`; F covers
  `[0x105DAF,0x105E30)`, `k11_fields.py:16-17`).
- a capture-side CHECK joins the existing four (`k11_capture.py:342-347`): every range is logged at
  least once, and for boot 2 no range changes after the Enter (a move would invalidate the pin,
  J3.8(b)).

### J3.4 The pinned frame of boot 2

The K11 driver dumps only from the capture's Enter on (`k11_dump_if_new`/`k11_armed`,
`test_game.c:13032-13043`, `:13173-13187`), and the port script's `enter_frame` is the first P record
with `mode=0x27` after the first K (`k11_session.py:143-146`). **The pin is that frame**: boot 2's
`enter_frame`, a measured number like the walk's 324 (named-gaps-a:299-301), recorded in the Makefile
as `SAVE_RELOAD_ENTER_FRAME`. The port dump's frame 0 is the settled screen at exactly that counter —
the driver asserts `frame_after == enter_frame` on the Enter iteration (`test_game.c:13173-13186`,
`:13198-13202`).

So the load claim is judged **at the port dump frame whose master counter `f` equals boot 2's
`enter_frame`** (dump frame 0 by construction), and the oracle additionally compares every later
dumped frame's ranges and fails at the first that differs — the gp trace's "first difference" ratchet
form (`gp_compare`). The capture side of the comparison is boot 2's settled snapshot: the ranges load
in `game_init`, before the frame counter runs, so the L/R records must already hold the final values
at the Enter, and the oracle reads the R record current at the pin. This design cannot know the
number — nothing ran; it comes from the capture.

### J3.5 The port driver's save path

`test_k11_oracle` (`test_game.c:13140-13207`) gains one env var, **`PR_K11_SAVE`**: when set and
non-empty, the driver calls `host_save_set_path(<path>)` before `game_init()` (and can drop it at
teardown as `test_restart_drive` does, `test_game.c:13876`, `:13887-13888`). The plumbing exists:
`host_save_read` prefers the path and falls back to the game dir's `cmos`/`CMOS` only when the path is
absent or unopenable (`host.c:435-451`); `host_save_writable` requires the path set
(`host.c:453-465`); the fallback is the game dir via `game_set_game_dir` (`flow.c:6806`).

- **Write replay (boot 1):** `PR_K11_SAVE=<dump>/save-reload/boot1-scratch/cmos`; the Makefile creates
  the scratch dir and `rm -f`s the file first (a stale file would be *loaded* and change the starting
  condition; the fallback reads the game dir only while the path does not exist, `host.c:440-443`).
  The replay's OPTIONS MENU exit runs `config_file_save` (`svcmenu.c:86-94`) → `config_storage_touch(1)`,
  `(2)`, the XOR and the `0x7F8` write (`config.c:1396-1407`).
- **Load replay (boot 2):** `PR_K11_SAVE=<dump>/save-reload/boot2-cmos`, a *copy* of the capture that
  the oracle makes (`cp` + a byte check against `save-reload/CMOS`): `config_file_load` reads it at
  `game_init` (`config.c:1368-1380`). The copy is the point — a later save would overwrite the copy,
  and `data/` is never written.

No driver change is needed for the ranges: the driver locates them from its own `mem[]` through the
same raw structures (image `DSD(0x2D48C)` + `0x7F8`; kind 0 `DSW(0x2D444+2)`/`DSD(0x2D444+4)`; tables
`DSD(0x2D478 + 8t)` with `count * (vb + nb)` from `DSD(0x2D3FC + 8t)`) and writes `<dir>/ranges.txt`,
one line per dumped frame in the `<idx> f=<hex4> tick=<hex8> …` form the gp driver's `frames.txt`
uses (`test_game.c:13340-13341`), each range as its full hex bytes.

### J3.6 The two claims, exactly

**Write.** `tools/save_reload.py` requires the boot-1 replay's scratch file to be byte-identical (2040
bytes) to `data/k11-captures/save-reload/CMOS`. The replay starts from the same condition as the
capture (both read the game dir's zero `CMOS`; the oracle checks `session.txt`'s `cmos=` against
`check_cmos(GAME_DIR)` and fails on a mismatch — the data changed since the capture), runs the same
key/pad script boot 1's `poll.log` yields, and writes the scratch file where the original wrote
`C:\cmos` (`0x2CBAF` → `0x1B084`). **It does not cover:** the other save sites (`0x1BEB5`
`game_shutdown`, `0x24AA1` `game_quit_prompt`; §S.7.5:1010-1011); DOS's filename/current-directory
resolution (the port writes a host path); the failure paths (short write, long file; §S.7.3:951-956);
whether the image bytes are semantically right (only that they equal the capture's); any CMOS other
than this one.

**Load.** The oracle requires the port dump's `ranges.txt` at the pinned frame to hold, for all five
ranges, the bytes of boot 2's `poll.log` at the pin, and every later dumped frame to hold them too
(the first difference fails and is reported with its frame). **It does not cover:** the display of the
loaded values (no pixel/frame claim for save-reload — this is not a walk-style oracle); changes the
capture itself never recorded; the kinds outside the three ranges (1..5); the port's
`host_save_read` fallback/length semantics versus DOS's fopen/fread (their own rows and named gaps,
§S.7.3); any CMOS but the captured one; and anything beyond the pin's claim — boot 2 stops at the
MAIN MENU.

### J3.7 What the implementation will touch (designed here, not done)

(a) `tools/k11_session.py`: the `save-reload` scenario (`keys`, `pokes=()`, `time_limit`, `boot2`,
`writable`); `port-script`/`autotype` gain `--boot {1,2}`; the `poll.log` docstring gains `L`/`R`.
(b) `tools/k11_capture.py`: the two-boot flow (staging CMOS de-symlink, writable boot-1 mount, the
CMOS copy-out and checks, the `-ro` boot-2 reuse), the five ranges in the poller, the new CHECKs. The
`-ro` default for every other scenario and `check_cmos` for every scenario stay — a test in
`tools/tests/test_k11_capture.py` next to `test_dosbox_cmd_has_the_verified_flags` (which asserts
`MOUNT C "/r/C" -ro`) and `test_check_cmos` will say so.
(c) `tools/save_reload.py` + `tools/tests/test_save_reload.py`: the two claims and the scenario
parsing (test-first per storage plan Step 3); each new assertion mutation-proved (AGENTS.md's
discipline).
(d) `port/tests/test_game.c` `test_k11_oracle`: `PR_K11_SAVE` and `ranges.txt` only.
(e) `Makefile`: `save-reload-oracle` (target, `.PHONY` next to `k11-*` at Makefile:66, and `verify`
next to `k11-oracle` at Makefile:1082), plus the pins `SAVE_RELOAD_CMOS_SHA256`,
`SAVE_RELOAD_POLL_SHA256`, `SAVE_RELOAD_FRAMES`, `SAVE_RELOAD_ENTER_FRAME` with provenance comments as
the other blocks have. `AGENTS.md`: the Commands line and the capture paragraph. The oracle skips
without the capture even under `PR_ORACLE_REQUIRED` (the `k11_compare.py:203-207` rule).

### J3.8 Named risks and open questions (unresolvable by reading)

(a) **One `right` tap = one CREDITS step?** The editor's Left/Right are level reads through `0x2EDE0`
(§K11.3:696-706) and the walk's 11 Down taps each moved one row (named-gaps-a:309-316), so one tap
should step once — but the tap's hold is the harness's `HOLD_S` (`k11_capture.py:58`), not a game
value. *Resolved by*: the capture; and it cannot change the claims — the port script is generated from
the capture's own K/pad records, and the capture's CHECK that `CMOS` differs from the staged copy
proves the change landed. If the editor moves more than one step, the scenario is recorded as-is.
(b) **Do the ranges stay settled through boot 2?** The R records answer it. If a range moves after
the Enter, the single pinned frame is insufficient and the claim must be re-anchored (most likely a
second pin at the change's frame); the per-frame comparison fails rather than passing silently.
(c) **Is a writable mount safe for `data/`?** The staging's only real files are `PRAGE.EXE` and
`CMOS` (`title_capture.py:52-68`); the game's only writer is `fopen("cmos","wb")` (`0x1B084`,
§S.7.2), and `-f` (`0x101520 = 1`) skips the CD switch (`config.c:1358-1359`). *Mitigation*: before
boot 1 record the staging's entries and the game dir's mtimes; after boot 1 assert no entry appeared
besides `CMOS` and no symlink target changed. A DOSBox-X file the check catches aborts the scenario;
`data/` cannot be written through the mount root because it holds only links.
(d) **The starting condition.** Boot 1's staged `CMOS` is a copy of the game dir's zero file and the
port's boot-1 fallback is the same file; an operator changing `data/game/C/CMOS` between capture and
oracle would start the two sides differently — the oracle's `session.txt cmos=` check (J3.6) turns
that into a named failure, not a silent pass.
(e) **The E-record reason.** With `-time-limit` both boots expect `reason=time-limit`
(`k11_capture.py:296-298`); a clean `exit` would mean the game left early and the capture is suspect.
(f) **`memsize`.** Every K11 capture used the DOSBox-X default and the walk already saved (dropped) at
the same settings; nothing here needs `memsize=64` (the U10 ending's harness value, AGENTS.md).

### J3.9 What remains gated

Storage plan Steps 3-5 (implement test-first, capture, oracle) and the track-J plan's step 4 run only
after the user's go-ahead. If the user declines or the capture cannot be made, the contract's step 6
applies: the named gap stays the track's main one (already at storage record:1064/1107 and AGENTS.md)
and this section becomes its design record.

### J3.10 The capture and the two claims (2026-10-15, resolved)

The go-ahead came; §J3.9's gate is lifted. Storage plan Steps 3-7 and track-J plan step 4 ran on
`reverse-jc-save-reload` (base `9d65b30d`). One capture, one run, nobody typing: the host's
`/usr/bin/dosbox-x` was first tried and **cannot run this harness at all** — DOSBox-X 2024.03.01
predates the `[dosbox] memory file` option (added in the 2025.01.01 release; absent from that
binary's strings, reference conf and `-set` handling), so no K11/GP poller gets guest RAM and the
run ends `CHECK poll.log: FAIL` with no `guest.mem`; the controller then built the version every
existing capture used, DOSBox-X 2026.08.31 for aarch64, at `/usr/local/bin/dosbox-x`, and the
capture succeeded on its first run. A capture is a host artefact of that build; this is the only
save-reload capture and the only Linux-host capture in `data/k11-captures/`.

**The capture** (`data/k11-captures/save-reload`, `session.txt`): `cmos=zero pokes=[] input=inject
enter_wait=25 pace=1`; boot 1 `time_limit=55 wall_s=55.7 rc=0 reason=time-limit` (J3.8(e) answered:
both boots end by the harness limit, not an early exit), boot 2 `time_limit=45 wall_s=45.7 rc=0`;
162 `frame_*.raw` for boot 2, raw window 1169..3148, `fps=70.0866`. `sha256`:
`CMOS` `dad3b1f6f84bd851eee675281a7a3310dcf967d42a375b1b9919fb1a41f104f9` (2040 bytes, its last
the XOR of the first 0x7F7, and `CMOS[0:0x7F8]` is byte-identical to the cfg range's R record at
the save — the file is the original's own image post-save); boot 2's `poll.log`
`fb9ab5aa5f833e4decd680c55358e3df25b4dda887e1837d1033e25cb19a05f5`; boot 1's
`boot1/poll.log` `cfc05e51af655c0ab59f7e15b0253eef75e660cc92f83d9844c03a7eb3875bfc`. Every
capture-side CHECK passed for both boots (`base`, `keys 11/11` then `1/1`, `mode 0x27 after the
Enter`, `pokes`, `script` — `k11_session.port_script` revalidated both poll logs, the K11 equivalent
of `check-input` (`gp_capture.py check-input` answers `no S records: not a gp poll.log`, rc 2) —
`ranges`, and boot 1's `save`/`staging`).

**The poller's five ranges** (the `L` records, boot 1 ms 814; base `0x266000`):
`at` `0x2E6CE4` (cfg), `0x2EBDB0` (k0), `0x2EBE34` (hs0), `0x2EBEAC` (hs1), `0x2EBEC8` (hs2) —
exactly `base + (va - 0x80000)` for the VAs of §J3.3, confirming the poller's formula and the raw
table values.

**J3.8(a) — one `right` tap is one step. Resolved yes.** Boot 1's cfg R records: all-zero at tick 0
(ms 814); the attract's writes at tick 0x18 (ms 2406/2408); the high-score build at tick 0xCE
(ms 5441/5449, when hs0/hs1 are filled); then **tick 0x77C (ms 34021, the `right` at 33 s and the
editor's Esc at 34 s)** moves the twelve stored copies of the CREDITS value by exactly one step
(bytes 0x0, 0x8, 0x10, 0x20, 0x40, 0x78 and their +0xB6 duplicates: `0x83/0xD9/0x78/0xBC/0xA2/0x14`
→ `0x82/0xD8/0x79/0xBD/0xA3/0x15`, mixed directions, i.e. one encoded step), and **tick 0x7B7
(ms 35006, the OPTIONS MENU Esc at 35 s → `config_file_save` `0x2CBAF`)** changes only the image's
last byte `0x7F7` `0x00` → `0x31` — the XOR the save writes (`0x1B0D4`). Field 0x29's window value
(`tools/k11_fields.get` over the `F` record) goes `0x142095` → `0x152095` at the same tick 0x77C,
and boot 2 loads `0x152095` (§J2.6's `CREDITS` readback). So the scenario is recorded as-is: no
multi-step editor, no re-anchor.

**J3.8(b) — the ranges stay settled through boot 2. Resolved yes.** Boot 2's R records: all-zero at
tick 0 (ms 811); the loaded image at tick 9 (ms 2095; its head `82007000EE…`, byte-identical to
boot 1's post-save cfg R, and `post_save image == loaded image` is true over all 0x7F8); the two
high-score tables at tick 0x16 (ms 2315); **nothing after the Enter's K (ms 25001)**. The single
pinned frame stands; the capture-side `ranges` CHECK and the oracle's re-check both pass.

**The two claims, both green on the first oracle run** (`make save-reload-oracle`, pins in the
Makefile):

```
save_reload: write: the replay wrote 2040 bytes identical to the capture's CMOS (sha256 dad3b1f6…)
save_reload: load: capture identity: poll.log sha256 fb9ab5aa…, 162 frames
save_reload: load: the port holds the capture's five ranges at f=01EB and every later dumped frame (3 compared)
```

The write replay's driver run and the load replay's each missed only the base pair
(`0x5D812 actor_spawn`/`set_dead`, `distinct=2 dropped=0`), so no `test_platform.c` miss-set change
is needed. Boot 2's port dump holds 3 frames, all at `f=0x1EB` (the settled MAIN MENU lets the
master counter hold), idx 0 the pin; the oracle compares all three. **No difference arose**, so
there is no finding to fix: the port's `config_field_set` → `config_storage_touch` → `config_file_save`
path reproduces the original's save byte for byte, and `config_file_load` plus the RAM tables
reproduce the loaded state at the pin.

**§J3.4's pin, amended by the capture.** Boot 2's `enter_frame` is **491** (`f=0x1EB`; the
Makefile's `SAVE_RELOAD_ENTER_FRAME`), a measured number like the walk's 324. Boot 1's is **304**
(`f=0x130`): the two boots of the *same* capture enter at different master counters although their
Enter keys land at the same harness wall-clock (ms 25001) and the same ISR tick (0x562 vs 0x566), so
the master counter's phase/rate relative to the ISR clock differed between the two runs (the counter
is not proportional to the tick in either). The design's per-boot measured pin is what this
requires; what the port's driver proves at each boot is narrower than "it would reach the frame on
its own": it arms the Enter at that boot's `enter_frame` - 1 and asserts `mode_before == 3`,
`mode_after == 0x27`, `frame_after == enter_frame` — i.e. the port is still in the mode-3
Enter-accepting state at the scripted iteration and makes the transition there, for both configs.
Nothing in either claim rests on explaining the 304/491 difference; the load claim's evidence is the
byte comparison at the pin, which passed.

**J3.8(c) — the writable mount is safe. Resolved yes.** The capture's `CHECK staging: ok` (no
staging entry appeared or changed besides `CMOS`, no symlink target moved, the game dir's
mtimes/sizes unchanged), and after the whole capture `data/game/C/CMOS` is still 2040 zero bytes
with its original mtime. **J3.8(d) — the starting condition.** `session.txt` says `cmos=zero`, and
the oracle's `write` re-checks that against `check_cmos(GAME_DIR)` on every run; the game dir is
untouched. **J3.8(f) — `memsize`:** the default (16 MB), no memsize in either boot's argv.

**The pins** (`Makefile`, provenance in its comment): `SAVE_RELOAD_CMOS_SHA256`,
`SAVE_RELOAD_POLL_SHA256`, `SAVE_RELOAD_ENTER_FRAME = 491`, `SAVE_RELOAD_FRAMES = 162`; the target
is in `make verify` (skips without the capture, ignoring `PR_ORACLE_REQUIRED`, like `k11-oracle`),
and `tools.tests.test_save_reload` is in verify's tool-test line. The claims' limits stand as
§J3.6 wrote them: the write claim covers this one save site and this one CMOS; the load claim says
nothing about the display, the kinds outside the five ranges, or anything past the pin.
