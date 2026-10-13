# Track D: differential rows for the ported functions the callee scan never reached — derivation record

Waves 1 and 2 (2026-10-11/12). Integration branch `reverse-d1` (off `main` `e53cc8a`); wave 2 integrated on
`reverse-d2int`. The per-batch records hold the raw-byte evidence for every row and correction:
`2026-10-11-reverse-d1{a..i}-derivations.md` (wave 1) and `2026-10-11-reverse-d2{a..e}-derivations.md`
(wave 2). This record is the track's ledger: why, how the batches were merged, the counters, the
corrections, the named gaps and the gate evidence.
Wave 3 (2026-10-12, integrated on `reverse-d3int`, per-batch records
`2026-10-12-reverse-d3{a..e}-derivations.md`) is §D.7, wave 4 (`reverse-d4int`) is §D.8 the first eight batches of wave 5 (`reverse-d5int`) are §D.9 and its ninth with the audits' follow-ups §D.10; §D.1-§D.6 describe waves 1 and 2 as they were gated.

## §D.1 Why

After C3g the port was `731 731 100` and the callee frontier was closed (322 rows), but the callee scan had
only ever reached 181 of the 731 Ghidra-listed ported functions below `0x5D000`. The other **550** rested on
hand-written unit tests and the narrow capture oracles. Every C3 batch that rowed such functions found
raw-over-port defects. Track D rows them leaves first: a wave is the unrowed functions whose callees are all
rowed, runtime (`>= 0x5D000`) or absent.

Measured by the same script on each tree (ported `FN_` addresses below `0x5D000` with no `Spec` entry):
550 before, **335** after wave 1, **230** after wave 2 (by call depth over unrowed callees: 67 at depth 0,
33 at 1, 30 at 2, 12 at 3, 88 deeper).

## §D.2 The rows

| wave | batch | scope | rows | cases | mutants |
|---|---|---|---|---|---|
| 1 | d1a | fighter.c | 25 | 157 | 203 |
| 1 | d1b | fighter.c | 29 | 228 | 341 |
| 1 | d1c | flow.c | 23 | 173 | 224 |
| 1 | d1d | flow.c | 23 | 111 | 185 |
| 1 | d1e | fight.c | 21 | 121 | 223 |
| 1 | d1f | fight.c | 22 | 185 | 221 |
| 1 | d1g | actors.c | 23 | 104 | 251 |
| 1 | d1h | camera, effects, attract | 23 | 300 | 171 |
| 1 | d1i | config, menus, platform | 27 | 144 | 166 |
| 1 | (integrator) | `fighter_47a00` | 1 | 4 | 21 |
| 2 | d2a | fighter.c | 24 | 249 | 478 |
| 2 | d2b | fight.c | 30 | 269 | 617 |
| 2 | d2c | flow.c, menus, attract | 25 | 151 | 410 |
| 2 | d2d | actors, camera, config, fighter | 24 | 266 | 578 |
| 2 | d2e | flow.c (`0x4EF8C`, `0x41C28` as nine per-state rows) | 10 | 50 | 208 |
| | **total** | | **330** | **2512** | **4297** |

`make diff-verify` goes from `322/322 functions VERIFIED; 1353/1353 mutants detected; 1 named gaps; 229/251
rows with callees closed (71 have none)` to **`652/652 functions VERIFIED; 5650/5650 mutants detected; 1 named
gaps; 428/485 rows with callees closed (167 have none)`** (measured on the integration tree; after wave 1
alone: `538/538; 3338/3338; 1; 329/372 (166)`). `SPECS` has 653 entries on 643 distinct addresses: the
`0x468D8` alias, `0x2C8F0` rowed as its two port functions (d2c) and `0x41C28` as nine per-state specs (d2e:
one spec with its 45 cases x 163 mutants would be 7380 original runs; a state's mutant cannot be seen by
another state's cases, so one spec per jump-table target, each naming the other targets' blocks, together
67/67). **The frontier is open again by construction:** the 57 rows with callees that are not closed are open
on the 230 unrowed ported functions and on the named non-rows of C3g §C3g.7.

No row adds a `/* 0xADDR` header or an `fn_register`: `port_progress.py` stays `771 1203 64` / `731 731 100`
and the E2 table is byte-identical.

## §D.3 The corrections (54; every one address-proved and independently audited)

Each is its own commit `port: D1|D2 <batch>: ...` with a mutant that reproduces the old behaviour — except
`0x28A8D`, whose function `0x28788` has no row yet: its regression is the unit test (`test_fight.c` arm B,
which fails `133 != 0` with the fix reverted). Three
read-only audits (capstone over the fixed-up image, a different agent from the implementer) re-derived them
from the bytes: 20 + 18 + 11 CONFIRMED, none wrong or unsure; the d1c/d1e/d1f four were re-read by the agent
that finished those batches, and `0x47A89` by the integrator.

| batch | correction | raw |
|---|---|---|
| d1b | fighter_state_36710 takes the 0x3C480 record and character from the side's slot | `0x3676E/0x36793` |
| d1a | fighter_19164 keeps an unordered rec+0x20 and stores the fistp indefinite | `0x19188/0x191A1` |
| d1i | config_keys_apply re-reads the BIOS record pointer before the second key loop | `0x1AE8D` |
| d1i | config_menu_default_bits bounds its entry index signed | `0x2CD0B/0x2CD11` |
| d1g | anim_id_path takes the pset index from DX alone | `0x2A3A2` |
| d1b | fighter_3962c and fighter_396ac test DL, not EDX | `0x3963B/0x396BC` |
| d1b | fighter_state_399cc hands 0x367DC the 0x1077A8[side] slot and its record | `0x399EE/0x399FC` |
| d1a | fighter_24754 tests AL of 0x1A570 | `0x2476C` |
| d1b | fighter_3b9d8 sign-extends CX and tests AL of 0x3B8D8 | `0x3BA39/0x3BA6E and 0x3BA53/0x3BA8A` |
| d1a | fighter_state_default tests AL of 0x365C8, 0x36638 and 0x3BDDC | `0x34A43/0x34A5A/0x34AA7` |
| d1f | fight_4cb18 tests 0x1A570's AL | `0x4CB92` |
| d1d | frontend_spawn_row spawns and stores at index -1 when the table is full | `0x38B62` |
| d1d | frontend_mode12_advance spawns the 0xC8704 prompt | `0x41626` |
| d1d | game_mode_12_step state 0 spawns the 0xC876C prompt | `0x41C7F` |
| d1a | fighter_state_35d7c computes its slot from the side and tests AL | `0x35D8E/0x35DED` |
| d1b | fighter_3f720 tests AL of 0x189FC | `0x3F766` |
| d1c | sound_music_pause_toggle tests AL of 0x1CA40 | `0x1D200` |
| d1g | render_list_init leaves the list head alone | `0x1C350` |
| d1b | ai_weight reads the other character from slot 1 - side | `0x46BE4/0x46BE9` |
| d1b | ai_band clamps and compares the band signed | `0x46E9C/0x46EA8/0x46EB1` |
| d1f | fight_dust_pick compares the draw signed | `0x493B3` |
| d1e | fight_health_bars the bar index in 16 bits | `0x33F98/0x33F9D` |
| d1h | camera_frame_last keeps the x87 sum and returns 1 when unordered | `0x164D7/0x164E3` |
| d1h | camera_bitrev skips the swap loop for a non-positive half length | `0x15EDD` |
| d2c | game_state_3_handoff re-reads the camera dword after the second spawn | `0x126A4` |
| d2d | camera_mode_track_pair dead zone at INT_MIN | `0x12F56` |
| d2a | fighter_state_35b7c divides the 16-bit difference | `0x35BBC` |
| d2a | fighter_state_36430/364fc test AL after 0x365C8 and 0x36638 | `0x3643E` |
| d2a | fighter_state_36430/364fc read bit 2 of the side's own slot | `0x36499` |
| int | fighter_47a00 restarts the other side's record | `0x47A89` |
| d2d | camera_page_tail_b cached +0x63 word is a signed word | `0x166F3` |
| d2c | flow_match_streak_update stores nothing in the loser's +0x80 | `0x2875B` |
| d2d | frame_timer's pset index is DX, not EDX | `0x2ABC8` |
| d2d | frame_timer takes the hold arm on an unordered timer | `0x2AB44` |
| d2a | hit_connect/hit_displace sign-extend the phase word | `0x3C774` |
| d2a | hit_connect/hit_displace write their stack dwords whole | `0x3C7D4` |
| d2d | hiscore_rank_probe checks the byte budget on a win too | `0x2DE75` |
| d2c | attract_config_volumes reads field 0x37 after the music volume call | `0x2C981` |
| d2a | fighter_input_ring_update sign-extends the ring index | `0x46425` |
| d2c | frontend_skip_check tests AL of 0x4F778 | `0x4F7DA` |
| d2c | game_mode_25_exit releases 0xB cells from col 0xE | `0x4F13F` |
| d2a | fighter_wall_clamp calls 0x187FC twice on the push path | `0x3558E` |
| d2a | fighter_state_35e6c tests AL after 0x3BDDC | `0x35F1A` |
| d2a | fighter_state_35e6c works on the side's own slot and record | `0x35F2B` |
| d2c | frontend_scoreboard_build passes word 0x74 and no handle to 0x2A17C | `0x4196C` |
| d2c | frontend_scoreboard_build zero-extends the divider's x word | `0x4197B` |
| d2a | fighter_state_35f84 gates on the EAX slot's record and stores to the side's slot | `0x36007` |
| d2d | fighter_3fdd8 tests AL of 0x1A570 | `0x3FDF0` |
| d2e | game_mode_25_reveal releases two cells at row 0xB | `0x4F029` |
| d2e | game_mode_25_reveal takes the 0xC95D4 stream table | `0x4EFFF` |
| d2a | hit_reaction_pick tests AL after 0x1DDF4 and the picked word's low half | `0x1E119` |
| d2e | game_mode_12_step passes word 0x74 and no handle to 0x2A17C | `0x41F06` |
| d2e | game_mode_12_step compares state 7's word zero-extended | `0x42279` |
| d2e | game_mode_09_step resets the loser's +0x80 to 0 | `0x28A8D` |

**The ones that change ordinary play** (the rest differ only on out-of-range inputs, NaNs, harness stubs, or a
non-default configuration bit):

- **`fighter_47a00` (`0x47A00`, the throw setup).** `0x47A89` is `mov eax,[esp+0x14]` with ESP as
  `sub esp,0x18` left it (`0x2AE14`'s `ret 4` popped the one push): the sixth dword `0x33950` wrote, the OTHER
  side's record, goes to `0x3C480` with `0xC90F8[the other's char]` at 3.0. The port passed `ctx[4]`, the
  side's own: the throw restarted the thrower on the victim's stream. Found by the d1b row (d1b record §3.8),
  landed by the integrator: `check_49z_47a00` and the `0x47B04` gate case asserted the old behaviour and now
  assert the other record's `+8`/`+0x24`, the side's seeds kept, and the other pset word — the restart rewrites
  it from the stream word and the record's flip bit (`+0x28` bit `0x4000`, `0x2A408`), which therefore steers
  `0x3C208`'s facing; the expected distance is unchanged. Reverting the fix fails 76 checks.
- **`flow_match_streak_update` (`0x286BC`) and `game_mode_09_step` (`0x28788`).** The loser's slot `+0x80`:
  the raw stores nothing at `0x2875B..0x28767` (it only forms the offset for the read at `0x28769`) and stores
  0 at `0x28A8D` (`mov edx,ebx` at `0x28A75` reloads EDX before `xor dl,bl`). The port wrote `flag ^ loser`
  in both, so the three-loss counter restarted at `0x80` or more and the streak arm re-fired on the next loss.
- **`frontend_scoreboard_build` (`0x418F4`) and `game_mode_12_step` state 3 (`0x41F06`).** `0x2A17C` takes
  EDX = the pset word and EBX = the palette handle; the raw passes `(rec, 0x74, 0)`, the port passed
  `(rec, 0, 0x74)`: pset word 0 and a palette acquired for "handle" `0x74`.
- **`game_mode_25_exit` (`0x4F0FC`) and `game_mode_25_reveal` (`0x4EF8C`).** The five exit releases are
  `0x2F388(0xE, row, 0xB)` (`0x4F13F..0x4F19E`), not `(-1, row, 0xE)`; the reveal lacked
  `0x2F388(-1, 0xB, 2)` (`0x4F01A..0x4F029`) and read its stream from `0xC9544` where the raw has `0xC95D4`
  (`0x4EFFF`).

After the review fixes (PROGRESS, "Track D review fixes") the counter reads `652/652; 5651/5651; 1; 428/485
(167)`: `snd_music_stop` gained the mutant `@eax` with the `0x1CA8F` correction.

## §D.4 Named gaps and what is not claimed

- **Not rowed, by cause** (the batch record's §6 has each): no port function to bind (`0x12CD4`, `0x2F920`,
  `0x1D018`, inlined in their callers); a deliberate host replacement (`0x1CF40` audio init, `0x47370` string
  table load, `0x28DA4` whose callees the port omits under `PORT:`); instructions the emulator does not model
  (`0x4FBA2` `int 0x10`, `0x52106` `in`/`out`, `0x2EA74` falling into `int 0x16`); the runtime fatal exit
  (`0x2EA68`); the runtime `strlen` `0x65624`, which loads ES (`0x432EC`; d2e's opt-in flat-segment setup
  `FLAT_SEGMENT_CALLEES` makes such a row possible, not yet used for it); not reached in the session
  (`0x3157C`, `0x216EC`, `0x3EA24`, `0x4EBB8`, `0x28788` — corrected at `0x28A8D` but not rowed —
  `0x41578`, `0x416D4`, `0x41760`, `0x271E0`).
- **Open holes named by the batches:** `effects_step`'s cases cannot tell an 8-bit from a 16-bit decrement of
  the active counter `0x9AF3D` (d1c §4); the arms that end in the runtime `sprintf`/`printf` (`0x2BF87`,
  `0x1D10E`, `0x1D183`) and in the fatal `0x62003` are unhit and named; where a callee runs real instead of
  stubbed, an AL-versus-EAX test after it is not separated (named per row).
- **A case set that cannot separate two readings** (found in review): `config_play_time_close`'s cases all
  have `mode & 3 == 0`, so an index of `[mode & 3]` and of `[0]` agree; the row does not prove the index.
- **Seen, not changed** (audit #3): `camera_mode_track_pair` keeps `diff < 0 ? -diff : diff` for the `neg` at
  `0x12ED5`/`0x12F3E` (C undefined at INT_MIN; the raw leaves INT_MIN) and caches `[0xF0AF0]` where the raw
  re-reads it at `0x12F34` after two `0x18714` calls (equivalence unproven); `snd_music_stop` (`0x1CA6C`)
  tested the whole dword where the raw has `test al,al` at `0x1CA8F` (its row stubbed only 0 and 1). The
  review fixes corrected the two negations and `snd_music_stop`; the cached `[0xF0AF0]` stands.
- **No capture reaches a changed path** (audit #2/#3 by reading; the gate by measurement, §D.6): the
  corrections' evidence is the rows and the unit tests, not a capture. The claim of every row is spec §5.3's:
  equivalence on the exercised blocks and inputs only, with its callees stubbed or run as stated.

## §D.5 Harness and test changes outside the rows

- **Two pre-existing host-dependent defects, found because macOS 27 turns an out-of-`mem[]` read into an
  intermittent SIGBUS** (both on untouched `main`): `test_frontend`'s "no localisation table" cases
  (`DS_001082DC = 0`) let `string_decode` (`0x474E4`) walk null-record residue that `test_fight`/
  `test_pose_pool` leave at `mem[4..0x18)` (`0xFFFFBA00` at +4) to `mem + 0xFFFFBA08` — `run_tests` died in 2
  of 3 runs; the cases now zero `mem[0..0x50)` first. And `fighter_pose_3a820`'s cases passed EDX = `0xDEAD`,
  so the `@side` mutant's other-slot address `(1 - side) * 0x94` left `mem[]` — `diffrun` died in 3 of 4
  runs; EDX is now the other side, row and pins unchanged. **Rule for every row:** an index-like sentinel
  must keep `base + index * size` inside `mem[]` for the binding and every mutant.
- **`verify_all` runs on a process pool and `run_port` spawns `diffrun` without a fork.** The (spec, binding)
  items are independent; 16 or more of them run on a spawn pool (`jobs`, else `PR_DIFF_JOBS`, else the CPU
  count), each worker with its own image file, results in the specs' order — identical to the sequential
  loop on a sample of 85 rows + 673 mutants (`dataclasses.asdict` equal). `subprocess.run(...,
  close_fds=False)` lets Python use `posix_spawn`: a fork of the emulating process (its address space is
  large) cost about half a second per run (`fight_health_bars`' 21 mutants: 12.0 s to 1.7 s). The full
  `--self-check` went from 25+ minutes (and growing with every row) to 2 min 9 s at 652 rows.
- **The D scaffold:** `D1_SPECS` in `tools/diff_verify.py`, `D1_MASKS`/`D1_KINDS`/`D1_CASES` and one
  `test_each_d1_mutant_is_caught_by_what_it_breaks` in `tools/tests/test_diff_verify.py`; each batch appends
  its own `Dnx_*` block. `tools/diff_emu.py` gains ten `RESOLVED_JUMPS` entries (with their `WINDOWS` pins)
  and `FLAT_SEGMENT_CALLEES`.
- **How parallel batches were merged.** `merge=union` is WRONG for these files (it interleaved one batch's
  block inside another's). The harness files were rebuilt per merge as an additive N-way composition keyed
  on base lines (every branch's insertions contiguous, in branch order; a changed base line reported), script
  kept in the ledger. One real conflict: d2b and d2c seamed `0x2F510` differently; d2c's by-value form
  (the string's first dword — one caller's string is a two-byte stack buffer) stands, d2b's `E.Call`
  follows, and seven of its mutant pins gained the call-argument difference (re-measured, catching case sets
  unchanged).

## §D.6 Gate evidence

- Wave 1 alone (reverse-d1 at the nine-batch merge + the counter): every oracle stage of `make verify`
  passed and the 942 lines before "== differential verification" equal the pre-track baseline's once the
  `(N.Ns)` / `in N.Ns` timings are normalised (0 differing lines).
- Wave 1 + the `0x47A00` correction: the same — 942 lines, 0 differing. No capture distinguishes the throw fix.
- Both waves (this tree): self-check `652/652 functions VERIFIED; 5650/5650 mutants detected; 1 named gaps;
  428/485 rows with callees closed (167 have none)`, rc 0; `PR_ORACLE_REQUIRED=1 ./build/run_tests` all checks
  passed. The first `make verify` on this tree passed every oracle stage (its 942 pre-differential lines equal
  the baseline's, 0 differing) and stopped in the harness tests on one pin: `fighter_45908`'s case z8
  deliberately reads the slot above the image (`0x10C1B0`, and its `+0x8A` byte) and was not named in
  `P45_OUTSIDE`. With the read named, an uninterrupted `make verify` exits 0 (177 harness tests OK, the
  counter above, entry-triage byte-identical, `symbols.h` idempotent).
- Audio: `make audio-render` on the pre-track tree and on this tree are `cmp`-equal, sha1
  `4194254de1155a2dda0ee0dd69f638d92cf9eea0` (the C3g closure's).

## §D.7 Wave 3 (81 rows, 29 corrections)

Five batches in worktrees off `7a9dfe7` (the wave-2 integration tree before its last docs commits), merged on
`reverse-d3int` (off `review-d-fixes`) by the same additive composition (§D.5) with base `7a9dfe7` and the
branch list review-d-fixes, d3a..d3e: no conflict in `port/src` (the seams two batches added to the same callee
are the same lines: `0x2C060`, `0x2C178`, `0x2C1C8`, `0x3C88C`, `0x2A4FC`), no conflicting base-line change.
d3d changed two existing harness lines and one pin, all for one fact: `0x36870` preserves EDI (its own body
never names ESI, EDI or EBP; the scan's set came from the indirect `call [0x1078E8]` at `0x379E8`, whose ten
table targets clobber EDX only — audit #6), and `0x47B04` stores through EDI after calling it.

| batch | scope | rows | cases | mutants |
|---|---|---|---|---|
| d3a | flow.c, config.c, fight.c | 25 (23 addresses) | 187 | 601 |
| d3b | fight.c, flow.c (seven `_rr` second specs) | 26 (19) | 214 | 639 |
| d3c | fighter.c | 13 | 147 | 431 |
| d3d | actors.c, config.c, fight.c, fighter.c | 12 (11) | 188 | 393 |
| d3e | camera.c, fighter.c (`0x384F8` as two specs) | 5 (4) | 98 | 135 |
| **total** | | **81** | **834** | **2199** |

`make diff-verify` goes from `652/652; 5651/5651; 1; 428/485 (167)` to **`733/733 functions VERIFIED;
7850/7850 mutants detected; 1 named gaps; 479/566 rows with callees closed (167 have none)`**. `SPECS` has 734
entries on 713 distinct addresses; **571 of the 731** ported functions below `0x5D000` have a row and **160** do
not (the same measure as §D.1; by call depth over unrowed callees: 43 at depth 0, 22 at 1, 29 at 2, 20 at 3,
46 deeper; six of the 160 are named non-rows). `port_progress.py` and the E2 table are unchanged.

**The corrections.** Each is its own commit `port: D3 <batch>: ...` with a mutant that reproduces the old
behaviour. Three read-only audits (#4 d3c, #5 d3b, #6 d3a+d3d+d3e; capstone over the fixed-up image, an agent
other than the implementer) re-derived all 29: 9 + 8 + 12 CONFIRMED, none wrong or unsure, and every seam the
five batches added (102 lines) is the entry's first statement with the C signature's arguments in order.

| batch | correction | raw |
|---|---|---|
| d3a | flow_26d4c formats `"%s%.03d%"`: the runtime prints nothing for the bare trailing `%` (the port drew a percent sign) | `0x26EF3`, format at `0x809AC`, formatter `0x6C929` |
| d3b | fight_hud_spawn_side re-reads the side byte `0x104B1A` after each call | `0x1DB61/0x1DB75/0x1DBC0` |
| d3b | fight_char_text_clear reads the column word per string, after `0x1C500` | `0x43349/0x4336B/0x4338C/0x433C4` |
| d3b | fight_char_opp_text_blink the same | `0x43621/0x43647` |
| d3b | fight_char_text_blink the same | `0x434F5/0x43517/0x43538/0x43569` |
| d3b | fight_4aad0 tests AL of `0x4B5A8` | `0x4AB4A` |
| d3b | fight_4b470 tests AL of `0x1A570` | `0x4B4FB` |
| d3b | fight_4d224 tests AL of `0x4D108` | `0x4D24C` |
| d3b | fight_slot_pass stores `0x107ED8` before each `0x1A570` call and reloads both counters from memory | `0x3CB7E/0x3CB9B/0x3CBA7` |
| d3c | fighter_382c4 runs `0x35B7C` in phase 2 only | `0x382EB/0x382F1` |
| d3c | fighter_winner_body tests AL of `0x3962C` and `0x396AC` | `0x193E9/0x1941E` |
| d3c | fighter_state_350d0 tests AL of `0x3BDDC`, `0x34DDC`, `0x3CF38` and takes `0x1DE64`'s AL | `0x35213/0x35293/0x352AB/0x352C1` |
| d3c | fighter_state_359e0 tests AL of `0x365C8` and `0x1A570` | `0x359EF/0x35ABF/0x35AFC` |
| d3c | the AL tests in `0x37464`, `0x469A8` and fighter_state_3531c | d3c record §3 |
| d3c | fighter_36f10 re-reads after its call | `0x370AA` |
| d3c | fighter_state_3531c state 8 takes the side's own slot | `0x35483` |
| d3d | fighter_1461c and fighter_14988 test AL of `0x14590` | `0x14669`, `0x149D5` |
| d3d | fight_hud_meter_step tests AL of `0x468D8` | `0x1D5F5/0x1D6BB` |
| d3d | fighter_216ec and fighter_3ea24 test AL of `0x1A570` | `0x217D6`, `0x3EB01` |
| d3d | fighter_47b04 reads the side's slot `+0x5F` from the slot table and passes `1 - side` | `0x47BB0`, `0x47B1C/0x47B28` |
| d3e | camera_y_clamp multiplies at the x87's 53 bits, not in `float` | `0x131AC..0x131B8` |
| d3e | fight_384f8 calls `0x2A4FC` (motion_step) on the side's record after `0x38434` | `0x38570/0x38572` |
| d3e | fight_384f8 tests AL of `0x3C59C` | `0x3850A` |
| d3e | fighter_think_side tests AL of `0x3C59C` and `0x3B298` | `0x3B484/0x3B4CC` |

**Which change ordinary play** (audits, by reading; the gate, by measurement below). `camera_y_clamp`: the
`float` product differs from the original at exactly four of the 65536 input words (12320, 19520, 22483,
27181: original 1875/7500/10903/17602, old port one off; audit #6 ran the original's bytes over all 65536
words against the new expression, all equal), one unit of camera y for a frame. `flow_26d4c`: visible text in
mode 0x22. `fight_384f8`'s missing `motion_step` and `fighter_382c4`'s phase gate: mode 0x25 only. The
AL tests and the re-reads are equivalent while the C callees return 0 or 1 and do not write the
re-read global (the audits scanned the direct stores; a store through a pointer is not excluded); their
evidence is the stub-EAX `0x100` case and the `@eax`/`@al` mutant of each row.

**Seen by the audits, not changed in wave 3** (open): whole-`int` tests where the raw has `test al,al` at
`0x14707`, `0x14A73`, `0x14872`, `0x14BEC` (callee `0x14590`) and `0x1D604` (after `0x1D74C`);
`fight_char_opp_text_clear` (`0x433DC`) reads the column word once where the raw re-reads it at
`0x43422/0x4343C`; pointers cached across calls in `fight_4aad0`, `fight_4b5a8`, `fight_4b470` and
`fighter_think_side` (`0x3B4B9`); `0x3C59C` has two C bodies whose shift masks differ (`& 0xFF` against the
raw's modulo 32; equal for the bits passed); three stale comments (`flow_26d4c`'s and `fighter_47b04`'s
headers, `0x36870`'s clobber note in `tools/diff_verify.py`).

**Not rowed in wave 3** (the batch records' §6): `0x1C470` (`in`/`out`), `0x51ED8` (the aperture), `0x1C528`
(a C `SpriteNode`), and not reached: `0x3C88C`, `0x1958C`, `0x33B00`, `0x2BAF4`, `0x31A78`, `0x3157C`,
`0x4EBB8`, `0x43D60`, `0x4434C`, `0x44638`, `0x1BE30`, `0x1C740`, `0x2D4EC`.

**Gate evidence.** Self-check on the merged tree `733/733; 7850/7850; 1; 479/566 (167)`, rc 0. The first
`make verify` passed every oracle stage and stopped in the harness tests on two pins that only the whole suite
sees (no batch ran it): d3a had derived that `0x2C060` clobbers nothing (`call 0x2caa8; jmp 0x2ca2c`, the
tail-jump target saves EDX) while d3b's stub still declared EDX and the pinned dictionary held both keys; and
`fight_hud_spawn_side`'s case d5 (the side byte `0x80`, zero-extended) reads slot `0x80`'s `+0x5B`/`+0x7A`
above the image (`0x10C20B`, `0x10C22A`), now named in `P45_OUTSIDE`. A pass over all 734 specs found no other
(303 stub clobber sets against the image, every row's outside reads against the pins). With the two fixed, an
uninterrupted `make verify` exits 0: 178 harness tests OK, the counter above, entry-triage byte-identical,
`symbols.h` idempotent, every oracle pin unchanged. Its 782 normalised lines before "== differential
verification" equal the pre-wave gate's (the review-fix tree) except one, which is not a pin: the
`gp-u10-ending` replay's `fn-miss PR_GP_DUMP 0x5D812 actor_spawn hits=65951` reads `65950`. Replay probes on
the merged tree: with the `flow_26d4c` correction alone reverted the count is 65951 again; with the
`camera_y_clamp` correction alone reverted it stays 65950. So that replay reaches mode 0x22's `flow_26d4c` and
the one actor less is the percent sign's glyph — the only correction of the wave a capture run distinguishes
(the frame ratchet stops before it, so no pixel is compared there). No trace or frame pin moved for the camera
correction.

## §D.8 Wave 4 (66 rows, 30 corrections)

Four batches in worktrees off `94898999` (the wave-3 integration tree), merged on `reverse-d4int` (off `main`
`4a7f5e5f`) by the additive composition with base `94898999` and the list main, d4a..d4d: no conflict. Per-batch
records `2026-10-12-reverse-d4{a..e}-derivations.md`. The wave is the 65 functions at call depths 0 and 1.

| batch | scope | rows | cases | mutants |
|---|---|---|---|---|
| d4a | flow.c (four `_reread` second specs) | 12 (8 addresses) | 110 | 487 |
| d4b | camera, effects, sprite, render (`0x17BC8` twice; `0x14268` and `0x1C528` are one C function) | 17 (16) | 128 | 285 |
| d4c | fighter.c, fight.c (`0x1958C` and `0x44638` twice) | 14 (12) | 220 | 449 |
| d4d | config, actors, attract | 8 | 167 | 149 |
| **total** | | **51** | **625** | **1370** |

A fifth batch, d4e, took the audits' open findings (§D.7 "seen, not changed", audit #7's notes) on functions
that already had rows: 15 more specs (65 mutants) — every one a second spec on an existing entry (`_al`: the
predicate the first row runs for real is stubbed; `_rr`: the stubs rewrite a value the raw reloads) except
`fighter_14814` (`0x14814`), a new row. It changed no existing spec or pin. Merged last; the one `port/src`
conflict was `camera_project`, where d4b's arithmetic shift stands.

`make diff-verify`: **`799/799 functions VERIFIED; 9285/9285 mutants detected; 1 named gaps; 513/632 rows
with callees closed (167 have none)`** (`784/784; 9220/9220; 499/617` before d4e). 615 of the 731 have a row
and 116 do not: 18 named gaps below, the six named non-rows, and 92 for waves 5 and 6.

**The corrections** (each its own commit `port: D4 <batch>: ...`; audits #7, #8 and #9 re-derived all 30
from the bytes: 4 + 9 + 17 CONFIRMED, and every seam). The 13 of d4a..d4d, each with a mutant that is the old
code:

| batch | correction | raw |
|---|---|---|
| d4a | flow_round_end_check re-reads both `+0x5A` bytes after the `0xD3` voice and `0x39FF4` | `0x2808B/0x28090` |
| d4b | camera_projectile_clash takes AL of `0x140E4` and of `0x181D0` | `0x17BEF`, `0x17C3A/0x17C6A` |
| d4b | camera_project tests AL of `0x16AFC`/`0x164F4` | `0x180CE/0x1810D` |
| d4b | camera_project tests AL of `0x1A570` | `0x181A0` |
| d4b | camera_project shifts the sprite origins arithmetically | `0x18193/0x18198` `sar` |
| d4c | fighter_pass_a compares the `+0x88` words signed | `0x196C9/0x196CC`, `0x196D7/0x196DA` |
| d4c | fighter_spawn_slot narrows its stack argument to 16 bits | `0x33CBB..0x33CC2` |
| d4c | fighter_spawn_slot clamps the quotient signed | `0x33DCC/0x33DCE` |
| d4c | fighter_command_generate lacked the block `0x4721B..0x47283` | `0x47225 test ah,8`, `0x47233`, `0x4725C`, `0x47283` |
| d4c | fighter_command_generate passes `0x1A570`'s byte to `0x3C6E8` | `0x472F2 and eax,0xff` |
| d4c | fight_char_team_text_blink reads the column word before each string | `0x446C9/0x446F2/0x4471B/0x4474E` |
| d4c | fight_mode25_scorecard lacked the title draw | `0x4EBC1..0x4EBDC` |
| d4c | fight_mode25_scorecard draws the digits on the side's row, not `tally + 1` | `0x4EE8C/0x4EE94`, `0x4EF05/0x4EF0F` |

d4e's 17 (record `2026-10-12-reverse-d4e-derivations.md`), none of which changes ordinary play by audit #9's
reading (the C predicates return 0 or 1; no callee was found writing a re-read value):

| correction | raw |
|---|---|
| fighter_146f0, fighter_14a5c test AL of `0x14590` | `0x14707`, `0x14A73` |
| fighter_14b90, fighter_14814 test AL of `0x396AC` and `0x14590` | `0x14BB4/0x14BEC`, `0x14838/0x14872` |
| fight_hud_meter_step tests AL of `0x1D74C` | `0x1D604` |
| camera_box_palette, camera_page_tail_b, camera_page_tail_a test AL of `0x1A570` | `0x15C1C`, `0x1661A`, `0x16B4B/0x16C85` |
| fighter_reaction tests AL of `0x3C59C`, `0x39EFC`, `0x3B6C4` | `0x3B748/0x3B79C/0x3B8B4` |
| fighter_attack_consume tests AL of `0x3CF38` and takes `0x4649C`'s AL | `0x3BE66`, `0x3BE84/0x3BE9E` |
| fighter_think tests AL of `0x3962C` and `0x396AC` (no row: ESI is live across both calls and d3c's declared clobbers include it; the bytes only) | `0x197A0/0x197C4` |
| fight_char_opp_text_clear reads the column word before each release | `0x43422/0x4343C` |
| fight_4aad0 reloads the entry's pointers and the midpoint `0x108874` after each call | `0x4AB0B..0x4ABC7`, `0x4ABBF/0x4ABD1` |
| fight_4b5a8, fight_4b470 reload the entry's actor and slot pointers | `0x4B5EB/0x4B5FF/0x4B648/0x4B653`, `0x4B4A3..0x4B52B` |
| camera_mode_track_pair reloads the camera x after the split and folds the stack copies (the latch where it was written), not the slots' `+0x34` | `0x12F34`, `0x12F4D` |
| fighter_frame_flag (the static second body of `0x3C59C`) shifts modulo 32 | `0x3C5AC` |

and, without a correction count: the wrapping negations at `0x3837B`, `0x12CE7`, `0x38595` and `fight_4aad0`'s
are unsigned, and three stale comments are corrected. `camera_project`'s `0x181A0` (d4b) and the unsigned
negations have no case behind them; `fighter_think_side`'s `+0x64` re-read (`0x3B4B9`) cannot be separated:
only `0x3AFC4` runs between the reads and it stores through the caller's frame alone.

**Which change ordinary play** (audits, by reading). `camera_project`'s shift: a negative origin word was
subtracted as `0x0000FFxx`, the output dword off by `0x10000`, so `0x181D0`'s signed compare saw no overlap on
that frame. Of the 5893 fighter body sprites none has a negative y origin and 25 have a negative x origin
(Rex 4, Kon 2, Cob 14, Dia 4, Hav 1; 19 of them named in the character's own stream block); whether a capture
plays one is not established by reading — the gate below is the measurement. The scorecard corrections are the
hidden bowling minigame (mode 0x25): string `0x62` (nine spaces) on row 6 and the digits on rows 7 and 9
instead of wandering with the score. `fighter_command_generate`'s block (the `DEMO ONLY` text and the clear of
`0x1082C8[side]`) needs bit 11 of config field `0x29`, which no option row of this image sets (defaults
`0x00142095`): not reached without a CMOS file that carries it — d4c's record calls it reachable, the audit's
reading stands. `fighter_pass_a`'s signed compare differs only for a `+0x88` word with bit 15 set. The rest are
equivalent while the C callees return 0 or 1 and do not write the re-read value.

**Named gaps** (the batch records' §4/§6; measured stops): no C function to bind (`0x11BCC`, `0x12CD4`,
`0x2F920`, `0x1D018`: inlined in their callers); `in`/`out`/`int` (`0x1C470` at `0x1C486`, `0x52106` at
`0x5212E`, `0x4FBA2` at `0x4FBAD`, `0x2EA74` falling into `int 0x16` at `0x2EB15`); the VGA aperture
(`0x51ED8`, `add edi,0xa0000` at `0x51F1B`); host replacements and runtime I/O (`0x1C930`, `0x1CF40`,
`0x47370`, `0x1C740`, `0x1BE30`); declared no-ops in the port (`0x2EA68`, `0x2D4EC`); a static function over
a C `SpriteNode` (`0x1C5E8`); the raw's own stack frame as the name buffer (`0x31A78`). Not reached:
`0x51E5C`, `0x14328`, `0x34B6C` (wave 5).

**Named by the batches and the audits, open:** `camera_mode_track_pair` reads `[slot]` unsequenced against
`0x18714` where the raw loads it after the call (`0x12F07/0x12F27`); `camera_x_commit`'s `-diff`
(`0x12C90`) and four signed adds near ±2^31 are C-undefined where the raw wraps; `camera_box_palette`'s header
says "bit 15 clear" for the AL = 0 arm; `0x28788` still has no row; `frontend_darken_all` and `actors_reset_al` report a constant
under the full mask (the mask-0 caller analysis is not done: nothing about the port's return is compared);
`game_audio_service`'s row compares only its four `0x1CB18` calls; the five AIL seams omit the host `sample`
pointer (the handle the raw pushes is not compared); `fighter_spawn_slot` caches the record pointer and the
character byte the raw re-reads (`0x33D1E..`, `0x33D31..`) and tests `0x1CEBC`'s whole return (`0x33E4D`);
`fighter_command_generate` caches `entry` (`0x47307..0x47312`); `sound_sample_start` keeps the buffer pointer
and length the raw re-reads after `0x5DC0F` (`0x1CB73..0x1CB79`); modes 5 and 0x30 do not reach two of the
eight descriptor dwords (`0xA889C`, `0xA88A8`) or entrances other than 0, 1 and 6.

**Gate evidence.** Self-check on the merged tree `799/799; 9285/9285; 1; 513/632 (167)`, rc 0; all 352
stub clobber sets equal the image's and the pinned dictionary, no row reads outside the image unnamed. The first
`make verify` passed every oracle stage and stopped in the harness tests on pins of two C3 rows: the mutants of
`actor_alloc` (`0x2AC80`) and `actor_type_49444` (`0x49444`) are C bodies in `diff_runner.c` that call effects.c's
exported list helpers, and d4b seamed those copies (`0x249D0`, `0x249B0`) for its effects rows. Their list calls
are recorded now; before, every case with a list call "caught" them on `original 0x249D0(..), port none`,
whatever the mutant broke. A run of the real-function tests with a soft assertion listed every moved pin: 16
(ten mutants' kinds, six catching-case sets), all in those two rows, every mutant still detected; re-pinned with
the cause in a comment. The second `make verify` exits 0: 178 harness tests OK, the counter above, entry-triage
byte-identical, `symbols.h` idempotent, every oracle pin unchanged, and its 782 normalised lines before
"== differential verification" equal the wave-3 gate's — 0 differing. So no capture distinguishes any wave-4
correction, `camera_project`'s shift included: its evidence is the row and the audit's sprite count, not a replay.

## §D.9 Wave 5, batches d5a..d5h (126 rows, 38 corrections)

Eight batches in worktrees off `3790f323` (the wave-4 tree before d4e), merged on `reverse-d5int` (off `main`
`3886d8ab`) by the additive composition with base `3790f323` and the list main, d5a..d5h. Per-batch records
`2026-10-12-reverse-d5{a..h}-derivations.md`. Three `port/src` overlaps, each resolved by keeping both sides'
intent: `fight_4b470` (main's reloads of the entry's actor, d4e, under d5d's seam), `fight_4e67c` (d5f's body
under d5e's seam) and `fighter_think` (d4e and d5f made the same two AL tests). The wave's last batch (d5i, seven
flow.c functions) and the functions `0x4BF18`, `0x49C78` are not in this section.

| batch | scope | rows | cases | mutants |
|---|---|---|---|---|
| d5a | flow.c front end (`0x20E90`, `0x20DF4`, `0x257A4`, `0x12484`, `0x11F6C`, `0x121A0`, `0x208F8`, `0x11A8C`) | 13 (8 addresses) | 104 | 587 |
| d5b | the camera overlap family, `render_list`, `sprite_blit`, `text_blit_string` | 19 (10) | 129 | 301 |
| d5c | character select, `fight_health_sync`, `fight_hud_pass`, three flow handlers | 14 (11) | 137 | 452 |
| d5d | small flow, attract, name entry, menu, config, four fight.c functions | 26 (19) | 162 | 423 |
| d5e | the twelve per-frame match-mode handlers (`0x28788` included) | 25 (12) | 125 | 971 |
| d5f | `fighter_think`, two camera dust functions, `fight_4e67c`, `fight_4d2d0` | 11 (5) | 197 | 293 |
| d5g | `hiscore_init`, `menu_step`, `menu_run`, `game_mode_1e_step`, `nameentry_step` | 6 (5) | 204 | 268 |
| d5h | the top of the call graph: `game_state_step`, the mode dispatcher `0x24C5C` (eight specs), `game_loop` | 12 (3) | 126 | 269 |
| **total** | | **126** | **1184** | **3564** |

`make diff-verify`: **`925/925 functions VERIFIED; 12849/12849 mutants detected; 1 named gaps; 572/758 rows
with callees closed (167 have none)`**. 688 of the 731 have a row; of the 43 that do not, 6 are named non-rows,
28 are named gaps (§D.8's 18 and, from this wave, `0x2EA78` — the VGA aperture and `int 0x16`; `0x249F0` —
`int 0x16` inline at `0x24A64`; `0x2CADC` — the port omits three of the raw's ten calls by design; `0x305FC` —
the raw's stack frame as the buffer; `0x2F9CC` — no C function, inlined in `game_init`; `0x2D6F8` — the only
arm the port runs makes 3 of the raw's 9 `0x2D4EC` calls; `0x20C10` — the runtime `setjmp` `0x653FC`;
`0x1B120`, `0x1B3AC` — the runtime open, `int` at `0x6F53A`; `0x1BEC4` — `int` at `0x4FBAD` in its first
callee) and 9 are open: d5i's seven and `0x4BF18`, `0x49C78`.

Two of d5h's rows run the original under a code poke, which the rows' claims name: `0x24C5C` with the two
bytes of `int 0x16` at `0x24D0E` as `cmp eax,eax` — exact for the no-key path, the only thing the code takes
from the BIOS there being ZF (audit #10); the key loop `0x24D26..0x24EE0` and the `longjmp` arm are named
unhit — and `game_loop` under `tools/title_pin.py`'s two `mov eax,0` over the RNG draws at `0x256B1` and
`0x256D6`, so those rows are blind to the port drawing nothing there (a documented `PORT:` deviation).

Second specs are the rule in this wave: `_rr` (the stubs write what the raw reloads after a call), `_real`
and `_sync` (a callee run on both sides instead of stubbed), `_al`, and a split of `fight_health_sync` in two
because `diffrun` takes 16 stubs a case. `render_list_blit` runs `0x51E5C` and the six renderers of table
`0x80C8C` on both sides and compares the back buffer: no pixel differs from `sprite.c`. `game_mode_09_step`
(`0x28788`), corrected at `0x28A8D` in wave 1, verifies as the port stands (30/30 blocks, 174 mutants).

**The corrections.** Each is its own commit `port: D5 <batch>: ...` with a mutant that is the old code and a
row that was MISMATCH before it. Two read-only audits (#10: d5a, d5e, d5g, d5h; #11: d5b, d5c, d5d, d5f)
re-derived all of them from the bytes: 18 + 21 CONFIRMED (39 with `fighter_think`'s two AL tests, which d5f
made again and §D.8 already counts), none refuted or unclear.

| batch | correction | raw |
|---|---|---|
| d5a | game_state_select reads the entry byte `0xF0A6E` at each site, not once | `0x11FFD`, `0x1206C`, `0x12090`, `0x120C3` |
| d5a | game_state_title phase 1 stores without testing the logo dword | `0x12402/0x12408/0x1243E` |
| d5a | game_mode_1f_step state 1 reads `0x1044A0` again after the second spawn | `0x20A10/0x20A17` |
| d5a | game_state_6 stores `0x1082CC` as a dword, reads `0xF0A72` as a byte, compares the draw signed, passes the stage as a zero-extended word | `0x11B2F`, `0x11BA6`, `0x11AF1/0x11AF4`, `0x11ABA/0x11AC1` |
| d5b | camera_projectile_hit, camera_point_side, camera_unfreeze, camera_decay, camera_point_hit take AL of their predicates | `0x1774D/0x17849/0x17883`, `0x179ED/0x17AB9/0x17B10`, `0x17367/0x17441/0x174EA`, `0x1769D`, `0x17E13/0x17E8B` |
| d5c | fight_health_sync tests AL of `0x36E2C` and of `0x3BDDC` | `0x34B93`, `0x34D11` |
| d5c | fight_health_sync reads the record once before the gate and again only for `+0x18` | `0x34B82`, `0x34BA7/0x34BB4` |
| d5c | fight_hud_pass tests AL of `0x3C59C` | `0x356C3` |
| d5d | frontend_mode12_darken_or_tally reads `0x104AD4` again before the column test | `0x41738` |
| d5d | hiscore_insert calls `0x2D4EC(table + 6)` when the descriptor's `+2` word is above the record | `0x2DDBE..0x2DDCD` |
| d5d | nameentry_finish reads the side's character byte again after `0x2DCA0` | `0x207B8` |
| d5d | fight_4b69c and fight_4e5a4 read the entry's actor again after their calls | `0x4B719`, `0x4E619/0x4E633` |
| d5e | game_mode_22_step and game_mode_24_step read the side byte `0x104B1A` again after `0x16D58` | `0x26CBC`, `0x26F89` |
| d5f | camera_dust_hit reads the node's actor again for the burst | `0x12A99`, `0x12AF5`, `0x12B47`, `0x12B98` |
| d5f | camera_impact_dust_spawn passes the draw's low word, sign-extended | `0x12941/0x12948/0x12956` |
| d5f | fight_4e67c's last gate: on an odd round the call needs the processed count zero (the port had it inverted) | `0x4E97E test si,si`, `0x4E981 jne` |
| d5f | fight_4e67c indexes `0x108894` by a 16-bit difference | `0x4E6DE/0x4E6E8/0x4E6ED` |
| d5f | fight_4e67c reads the fighter record and the node's actor again after its calls | `0x4E6B8..`, `0x4E799..`, `0x4E758`, `0x4E8E4` |
| d5g | hiscore_init takes the byte of `0x1E988` | `0x1E884 and eax,0xff` |
| d5g | game_mode_1e_step states 0, 4, 7 test AL of `0x1ECC8` / `0x1EC38` | `0x1EEE6`, `0x1F1B0`, `0x1F2CE` |
| d5h | game_state_step state 9 calls the attract reset `0x10EE4` when `0xF0A71` is set | `0x11EE6..0x11EF6` |
| d5h | the modes 0x22/0x23 tail reads `0x104B1A` again after `0x186D0` | `0x2555E` |
| d5h | game_loop increments `0x104AF4` as a word, clears `0x1014FC` as a byte, steps `0x10150C` after the audio service | `0x25632`, `0x2569D`, `0x256B6/0x256C0` |

**Which change ordinary play** (the audits, by reading):
- `fight_4e67c`'s gate (mode 0x25, the bowling minigame; no capture reaches it). The raw calls `0x4E350(0)`
  and clears `0x1088B8` iff `[0x1088B8] != 0 && [0x1078F0] != 0 && [0x1078F1] != 0 && (([0x1088BD] & 1) == 0
  || SI == 0)`, SI counting the nodes still in state 0 or 1 this pass. The port had the odd-round sense
  reversed: it re-stood the set in the middle of the attempt.
- `game_state_step` state 9. `0xF0A71` is set only behind held-input tests (`0x10DCD`, `0x10E35`); the idle
  attract (3 -> 9 -> 6) is unchanged. After that chord the hold now ends in state 0; the old port went on to
  the demo with the byte stuck at 1. `check_state9_countdown` seeded that byte only to quiet the two tails and
  now seeds it clear (its six assertions unchanged); the set arm is covered by the row alone — a third unit
  step is owed.
- `game_loop`'s word increment: the dword carried into `0x104AF6` every 65536 iterations (about 18 minutes),
  which the mode-0xC tail saves and reads back. And its increment order: the audio service's tree writes the
  frame count (`0x1B464`, through `0x1B544` on a first read), so on such an iteration the raw waits one tick
  the old order did not; d5h's record says nothing reads the count there, the audit's reading stands.
- Not reached, where the batch records say otherwise: `game_state_6`'s dword store and byte read (every writer
  leaves 0..255; `0xF0A73` is never referenced), `game_state_title` with a zero logo dword (phase 0 already
  dereferences the same pointer). `fight_4e67c`'s 16-bit index differs only for a word in `0x8000..0x80DF`.
The rest are equivalent while the C callees return 0 or 1 and do not write the re-read value.

**Open, seen by the audits:** `res.c` stores `DSD(0x1014FC) = 1` where the raw has a byte store (`0x1B3F8`);
`attract.c` tests the byte of `0xF0A5C` where the raw tests the dword (`0x114E4`); `game_loop`'s row never
executes `0x2569D` (no mutant drops the store); the C passes `0x4E99C` the 32-bit count where the raw passes
the word (`0x4E91E`); `config_validate`'s defaults arm omits two fills (`0x2D83A..0x2D875`).

**Harness changes outside the rows.** `_CALLEE_CLOBBER_FIXES`: `0x51E5C` clobbers nothing (d5b); `0x3962C`
and `0x396AC` clobber EDX only and `0x49444` nothing (d5f: the scan's ESI/EDI/EBP come from the fatal exit
under `0x3AFC4` and from `0x2B150`'s uncorrected set) — d3c's and P7's declarations and pins follow, their
rows unchanged. `RESOLVED_JUMPS` gains `0x20931` and `0x4C687` with their `WINDOWS` pins;
`FLAT_SEGMENT_CALLEES` gains the runtime memmove `0x653A1`.

**Gate evidence.** Self-check on the merged tree `925/925; 12849/12849; 1; 572/758 (167)`, rc 0; all
559 stub clobber sets equal the image's and the pinned dictionary; no row reads outside the image unnamed (926
specs); the real-function tests under a soft assertion report no moved pin (run before the gate, as §D.8's
lesson asks). `make verify` exits 0 on the first run: 178 harness tests OK, the counter above, entry-triage
byte-identical, `symbols.h` idempotent, every oracle pin unchanged, and its 782 normalised lines before
"== differential verification" equal the wave-4 gate's — 0 differing. So no capture distinguishes any of the 38
corrections, `game_loop`'s increment order and `game_state_step`'s state 9 included: their evidence is the rows
and the audits. One thing a merge must know: the composition rebuilds the harness files from the base and the
listed branches, so an integration edit made on the integration branch itself (here the three stub clobbers
and the counter pin) is dropped by the next composed merge and has to be applied again.

## §D.10 Wave 5, batch d5i and the audits' follow-ups (19 rows, 5 corrections)

Batch d5i (record `2026-10-12-reverse-d5i-derivations.md`), merged on `reverse-d5int` after §D.9's tree went
to `main`; the composition's list is the pre-merge `main` (`3886d8ab`) and d5a..d5i, base `3790f323`.

| scope | rows | cases | mutants |
|---|---|---|---|
| `game_mode_32_step` `0x296B8` and `game_mode_0d_step` `0x274FC` (four specs each), `game_mode_13_step` `0x424E8` (three), `frontend_match_start` `0x1EA08` (two), `game_mode_0e_step` `0x27A2C` (three), `game_state_4` `0x11578`, `attract_step` `0x11000` (two) | 19 (7 addresses) | 162 | 946 |

`make diff-verify`: **`944/944 functions VERIFIED; 13795/13795 mutants detected; 1 named gaps; 582/777 rows
with callees closed (167 have none)`**. 695 of the 731 have a row. Of the 36 that do not, 6 are named non-rows,
28 are the named gaps of §D.8 and §D.9, and **two are open: `0x4BF18` and `0x49C78`** (batch d5j, not merged).

**The corrections** (audits #12 and #13, read-only, from the bytes: 5 of 5 CONFIRMED; d5i's seven functions
were also read end to end against the raw with nothing else found):

| source | correction | raw |
|---|---|---|
| d5i | game_mode_13_step tests AL of `0x1C6D4` | `0x42599 test al,al` |
| d5i | frontend_match_start reads the row byte `0xA7B94[i*4]` at each of its four calls | `0x1EB4C`, `0x1EB98`, `0x1EBB9`, `0x1EBD4` |
| d5i | attract_step uses the cycle counter `0xF0A5C` as a dword | `0x11089..0x110A5`, `0x1143A`, `0x114E4` |
| audit #12 | game_state_select and game_state_3_handoff test AL of `0x1C6D4` | `0x1203D`, `0x126F1` |

None changes ordinary play: `0x1C6D4` leaves the entry's handle in EAX's upper bytes and writes AL only, and
the port's function returns 0 or 1 (its three callers are these); the row table is constant; the counter's
seven sites in the image are all dword operands and its only writers leave 0..4. The two `0x1C6D4` casts of
audit #12 have no case that separates them (the rows of `0x11F6C` and `0x12658` run the callee for real): the
bytes are their evidence.

**Also in this round.** `check_state9_countdown` gained the set arm of `0x11EF2` that §D.9 owed: with
`0xF0A71` set at the expiry the hold ends in state 0 with `0xF0A71`, `0xF0A6F` and `0x104B00` rewritten by
`0x10EE4`; four of its five new checks fail with the call removed. `attract_step`'s increment is written in
the wrapping form (`0x11091 inc ecx`; audit #13).

**Open, seen by audit #13:** in `attract_step` the C stores the phase byte before a reload the raw makes
first (`0x11206..0x11211`) and reads `0x108173` after the phase store (`0x114C6`) — no call or aliasing
between, not observable; `0x110DD`: one stub EAX per case cannot show that the first of the eight `0x33754`
results is the one stored in `0xF0A48`.

**Gate evidence.** Self-check on the merged tree `944/944; 13795/13795; 1; 582/777 (167)`, rc 0; all 575 stub clobber
sets equal the image's and the pinned dictionary; no row reads outside the image unnamed (945 specs). The
soft-assertion pre-check was not run this round; the gate's own harness suite stands in for it. `make verify`
exits 0 on the first run: 178 harness tests OK, the counter above, entry-triage byte-identical, `symbols.h`
idempotent, every oracle pin unchanged, and its 782 normalised lines before "== differential verification"
equal the §D.9 gate's — 0 differing. So no capture distinguishes any of the 5 corrections either.

## §D.11 Wave 5, batch d5j and the audits' open notes (7 rows, 13 corrections): the last two functions

Batch d5j (record `2026-10-12-reverse-d5j-derivations.md`), merged on `reverse-d5int` after §D.10's tree went
to `main` (`d5663cf2`). The three harness files are the integration head plus d5j's insertions (composition
from the merge base `3790f323` over `HEAD` and `reverse-d5j`: 740, 770 and 844 lines added, none removed);
`fight_4b470` keeps the integration side, and the three stub-clobber keys d5j repeats (`0x4987C`, `0x4AC38`,
`0x4B470`) stay once.

| scope | rows | cases | mutants |
|---|---|---|---|
| `fight_4bf18` `0x4BF18` (three specs: the list walk, the empty list's preamble and post-loop, `_rr`; 84 of 84 blocks between them) and `fight_effects_pass` `0x49C78` (four: types 0..8 and the tail, `_m9`, `_rr`, `_pre`; 139 of 139) | 7 (2 addresses) | 131 | 411 |

`make diff-verify`: **`951/951 functions VERIFIED; 14206/14206 mutants detected; 1 named gaps; 601/784 rows
with callees closed (167 have none)`**. 697 of the 731 have a row and **no function is open**: the 34 without
one are the 28 named gaps of §D.8 and §D.9 and the 6 named non-rows below `0x5D000` (`0x2EA30`, `0x1B544`,
`0x29D60`, `0x2EA64`, `0x32BAC`, `0x500BB`). d5j adds 15 seams in `fight.c` and no named gap.

**The corrections** (audit #14, read-only, from the bytes: 13 of 13 CONFIRMED, the reloads of both fight
functions checked both ways — every reload the port makes is one the raw makes, every cached-register site
still uses the local —, the 15 seams and the merged `fight_4b470` CONFIRMED):

| source | correction | raw |
|---|---|---|
| d5j | fight_4bf18 tests AL of `0x1A570` | `0x4C29C`, `0x4C504` `test al,al` |
| d5j | fight_4bf18's frame gate is a signed remainder | `0x4C3E7 sar edx,0x1f; idiv ecx` |
| d5j | fight_4bf18 loads the entry's actor `[ecx+8]` again after its calls | `0x4BFF5..0x4C027`, `0x4C0AF/0x4C0C2`, `0x4C0F2/0x4C113`, `0x4C1A9/0x4C1C2`, `0x4C231/0x4C23D`, `0x4C30D` |
| d5j | fight_4bf18 loads `0x108898` again for the score test | `0x4C321` |
| d5j | fight_effects_pass tests AL of `0x4AF04` | `0x4A0A7 test al,al` |
| d5j | fight_effects_pass loads the entry's actor again after the prelude and its arms' calls | `0x49D4C` and the sites of the batch record's §3.6 |
| audit #10 | res_load_present stores the full-copy flag `0x1014FC` as a byte | `0x1B3F4/0x1B3F8 mov byte` |
| audit #11 | fight_4e67c hands `0x4E99C` the zero-extended word count | `0x4E91C..0x4E923` |
| audit #9 | camera_mode_track_pair loads `[slot]` after `0x18714` | `0x12F02/0x12F07`, `0x12F22/0x12F27` |
| d4c's row, this round | fighter_spawn_slot tests AL of `0x1CEBC` | `0x33E4D test al,al` |
| d4c's row, this round | fighter_spawn_slot loads the record and the character again | `0x33D1E`, `0x33D31`, `0x33D53`, `0x33D60..0x33D7B`, `0x33E12/0x33E14` |
| d4c's row, this round | fighter_command_generate reads the command dword before `0x1A570` and recomputes the entry after | `0x472E1..0x472E7`, `0x47307..0x47312` |
| d5a's row, this round | sound_sample_start loads the length and the buffer again after `0x5DC0F` | `0x1CB73..0x1CB79` |

The six of d5j each have a mutant that is the old code. The seven others are the open notes of audits #9,
#10 and #11 and of the wave-4 and wave-5 rows, fixed from the bytes; none has a case that separates it (the
rows of those functions stub nothing that rewrites the reloaded value, and `0x1B3AC` is a named gap), so the
bytes and the audit are their evidence. `res_load_present`'s is also a unit check: `test_platform.c` seeds
the upper three bytes of `0x1014FC` and fails with the dword store.

**Also in this round.** `camera_x_commit` is written in the wrapping form (`sub` `0x12C88`, `neg` `0x12C90`,
`add`/`sub` `0x12CB7/0x12CC4`: the signed C forms were undefined where the raw wraps); `camera_box_palette`'s
header says what the code does (it mirrors when `0x1A570`'s AL is 0, the actor's bit 15 set, `0x15C1C`).

**What the audit adds.** None of the 13 changes ordinary play as far as the callees it read show: the port's
predicates return 0 or 1; `0x4E99C` only ever tests `bx`; no instruction addresses `0x1014FD..FF`; and the
signed remainder differs only for a negative `0x1088D0`, which the image never stores (its nine writers,
`0x20CB0`, `0x24B48`, `0x24F4C`, `0x24FB4`, `0x25007`, `0x25061`, `0x250B4`, `0x25111`, `0x25163`, all write
`(x & 0xF) * 5 + 0x1E`, 30..105 — this closes the batch record's §3.2 caveat). It did not read the callees
for writes to the reloaded cells (`[entry+8]`, `[slot]`, `[slot+0x7a]`), so "no change in play" is not claimed
for the reload corrections beyond that. One stub clobber set is wider than the bytes: `0x4CD98` pushes and
pops EBX/ECX/EDX and writes none of ESI/EDI/EBP, but the scan (`callee_clobbers`) reports those three because
its callee `0x2B150` has an indirect call (`0x2B185`), and the hand fix for `0x2B150` applies only where it is
the stub. The set is the scan's, it is pinned, it errs towards poisoning more, and the only caller pops the
three right after (`0x4C3BF`); left as measured.

**Open, named:** the port sets `0x1014FC` after the read's stall and re-sync where the raw sets it before the
open (`0x1B3F8` precedes `0x1B403`; no reader between); `fight_4bf18`'s `%` has no zero guard where the raw's
`idiv` faults (`0x4C3E7`; only before any writer of `0x1088D0` ran); the held-actor cell `[ecx+0x10]` is kept in
a local where the raw loads it twice with no call between (`0x4C168`, `0x4C240`, `0x49F2F`); 23 other callers
of `0x1A570` (`fighter.c`, `actors.c:1613`) test the whole int where the raw tests AL — value-identical with
the 0/1 return, not rowed as such; `fight_effects_pass` type 1 and `fight_4bf18`'s bounce negate an `s32` (`d = -d`; `neg` at
`0x49D5B`/`0x4C043`, `0x4C378`), undefined in C at 0x80000000 where the rows' cases agreed on this build;
`config_validate`'s two omitted fills (`0x2D83A..0x2D875`); no mutant for `game_loop`'s `0x2569D` store; no
stub-EAX case separates the two `0x1C6D4` casts of §D.10; the limits of the batch record's §4.

**Gate evidence.** Self-check on the merged tree `951/951; 14206/14206; 1; 601/784 (167)`, rc 0; all 587 stub clobber
sets equal the scan's and the pinned dictionary; no row reads outside the image unnamed (952 specs); the
real-function tests under a soft assertion report no moved pin. `make verify` exits 0 on the first run: 178
harness tests OK, the counter above, entry-triage byte-identical, `symbols.h` idempotent, every oracle pin
unchanged, and its 782 normalised lines before "== differential verification" equal the §D.10 gate's — 0
differing. So no capture distinguishes any of the 13 corrections either: their evidence is the rows, the
bytes and audit #14.
