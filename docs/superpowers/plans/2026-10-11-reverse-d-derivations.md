# Track D: differential rows for the ported functions the callee scan never reached — derivation record

Waves 1 and 2 (2026-10-11/12). Integration branch `reverse-d1` (off `main` `e53cc8a`); wave 2 integrated on
`reverse-d2int`. The per-batch records hold the raw-byte evidence for every row and correction:
`2026-10-11-reverse-d1{a..i}-derivations.md` (wave 1) and `2026-10-11-reverse-d2{a..e}-derivations.md`
(wave 2). This record is the track's ledger: why, how the batches were merged, the counters, the
corrections, the named gaps and the gate evidence.

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

