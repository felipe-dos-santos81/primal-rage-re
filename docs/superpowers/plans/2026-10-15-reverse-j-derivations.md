# Track J derivations record — close the ledger, run the captures (2026-10-15)

Plan: `docs/superpowers/plans/2026-10-15-reverse-j-closeout.md`.
Spec: `docs/superpowers/specs/2026-10-15-track-j-design.md`.
Sections are per task (§J1..§J9); a lane appends only its own section, byte-evidence inline.
`§G4` and `§G7` stay in the track-G record (their contracted sections).

## §J1 — the closure amendment (lane j1 task 2; declared non-goals stop blocking a closed row)

**Step 1, the baseline (measured on this lane's base `4c47761a`, Task 1's post-§G4 tip).** The
closed-row replay over `SPECS` (every non-gap row VERIFIED; the one gap spec `host_1b890` at
`NAMED_GAP`) reproduces the plan's 832 with-callee rows, 716 closed, 116 open, 175 without callees,
and the full `--self-check` prints `1007/1007 functions VERIFIED; 14901/14901 mutants detected;
1 named gaps; 716/832 rows with callees closed (175 have none)` (§G4's reading). Every open row's
blocker set was classified from the repo's own tables: **runtime** `>= 0x5D000` (AGENTS.md's
runtime region — the WATCOM libc / DOS-4GW glue the host serves), **host-owned** a
`tools/port_classification.txt` line (host-owned or deferred), **non-function** an address
`port/decomp/prage.functions.csv` never lists as a real function (E2's field — §G3's census), and
**portable** anything else (a real function without a row).

**The measured census (116 rows).** 96 carried a `>= 0x5D000` runtime callee (79 blocked on those
alone). 19 blocked only on E2-class non-functions: the `0x24568`/`0x24804`/`0x40CB0` call-tables
(8 rows: the `game_mode_05/0d/32` rows), the `0x29D04`/`0x3D0BC`/`0x3E484`/`0x3E4C4` fighter hooks
(8 rows: `fighter_19020/35050/pass_a(_al)/state_350d0/state_3531c/think_side/winner_body`),
`0x51E58` (`sprite_blit`) and the `0x2CB74` pair (`menu_step`/`menu_run`) — this is §G3's
"17 E2-family" count plus that pair. 1 blocked only on the host-owned `0x10D34`/`0x4FB98` pair
(`game_shutdown`). **No row blocked on a `portable` address:** §G3's sentence "the open rows are
open on the 8 unrowed functions" is not what the table measures — no spec lists any of the 8 named
gaps (`0x4FBA2`, `0x1BEC4`, `0x20C10`, `0x1CF40`, `0x1C930`, `0x1B120`, `0x1B3AC`, `0x47370`) as a
callee; every blocker of every open row is one of the three non-goal classes. (This corrects that
AGENTS.md sentence; raw wins.) The per-row table is at the end of this section.

**The rule implemented** (`tools/diff_verify.py`: `RUNTIME_BASE`, `_non_goal_tables`, `non_goal`,
`closed_rows`): a row counts closed when it is VERIFIED and every callee is VERIFIED by its own row
or `non_goal(a)` — `runtime` for `a >= 0x5D000`, `host-owned/deferred` for a
`tools/port_classification.txt` address, `non-function` for an address
`port/decomp/prage.functions.csv` never lists. A real function in that index with no row (the 8
named gaps) is **not** a non-goal and keeps its callers open; only a row can close that hole. The
callee's mode (stub/allow/real) is deliberately not part of the test: a non-goal callee can never
gain a row in any mode, and the mode split was not the metric's finding (recon-01 §3: "the metric
cannot distinguish 'open on a callee that can never have a row' from 'open on a missing row'").

**Counter deltas, each with its cause.** functions 1007 -> 1007 and mutants 14901 -> 14901 (no row,
case, binding or mutant is added; the amendment changes only the closure test), named gaps 1,
with-callee rows 832 and no-callee rows 175 unchanged. Closed 716 -> 832 (+116: every open row
qualified — the census above is the per-delta cause; the largest single class is the 79 rows whose
only blocker is runtime). The counter's wording now states the rule and names the three classes
(`tools/diff_verify.py`'s last line; pinned in `tools/tests/test_diff_verify.py`, quoted in
AGENTS.md). The secondary number is unchanged: **12 of the 739 rowed functions stay weaker**
(727 full rows) — §G4 made `game_loop` weaker through its `0x25680` (the full-screen copy) and
`0x256D4` (the ISR-tick spin) named unhit, not through pokes any more; the other eleven are as
AGENTS.md lists them.

**Byte-identity against base.** The full `--self-check` output on this lane differs from the base's
in exactly one line, the counter sentence (output line 15912): `716/832` -> `832/832` plus the rule
wording. Every table row, every row verdict, every mutant verdict and all other prose is
byte-identical (`diff` of the two runs: one changed line).

**Step 3, the mutation proofs (each run as a scratch patch on the lane, then reverted).**
1. *Flip a class entry*: removing the `501A3 host-owned` line from `tools/port_classification.txt`
   re-opens the three rows that block on it (`config_screen_wait`, `config_screen_wait_zero`,
   `game_loop` — the address is in the index, so without its classification it is portable):
   closed 832 -> 829, open 0 -> 3, and `tools/tests/test_diff_verify.py` fails two ways
   (`non_goal(0x501A3)` reads None; the closed-rows regression closes only 3 of its 4 non-goal
   rows). Cause: the class comes from the table, not a literal.
2. *Add a named-gap blocker*: adding `0x4FBA2` (a named-gap function, in the index, no row) to
   `game_shutdown`'s allow list re-opens that row: closed 832 -> 831, open 0 -> 1, the row still
   VERIFIED (1007/1007 — the original never calls it, so only the callee list changed). Cause: a
   portable function without a row is not a non-goal.
3. *Drop the non-function class* (a directed mutant of `non_goal`: skip the index test): the new
   class test fails at `0x2CB74` and the closed-rows regression's named-gap row closes (the
   assertion that must fail, fails).

**Files.** `tools/diff_verify.py` (the rule and the counter line), `tools/tests/test_diff_verify.py`
(the pin sentence + the rule's regression cases: a row blocked only on excluded classes counts
closed, a row blocked on the portable named gap `0x4FBA2` stays open, and the class test pinning
each class to its table), `AGENTS.md` (the counter sentence, the weaker-list sentence — `game_loop`'s
stale §G4 poke clause — and the open-rows census paragraph), this record. Measured on this lane
(base `4c47761a` + this commit); the full gate's commands and results are in the task report.

### The 116 rows §G3 left open, each with its blockers and classes (the closure replay)

| row | entry | open callee(s): address, mode, class |
|---|---|---|
| `actor_spawn` | 0x2AE14 | `0x5D812` allow runtime |
| `snd_music_stop` | 0x1CA6C | `0x5DEAF` stub runtime |
| `snd_sample_playing` | 0x1CE70 | `0x5DD03` stub runtime |
| `snd_samples_stop_all` | 0x1CD9C | `0x5DC0F` stub runtime, `0x5DC8B` stub runtime, `0x5DD03` stub runtime |
| `snd_sample_stop` | 0x1CE04 | `0x5DC0F` stub runtime, `0x5DC8B` stub runtime, `0x5DD03` stub runtime |
| `snd_sample_queue` | 0x1CC28 | `0x5DC0F` stub runtime, `0x5DC8B` stub runtime, `0x5DD03` stub runtime |
| `set_dead` | 0x2B150 | `0x5D812` allow runtime |
| `snd_music_playing` | 0x1CA40 | `0x5DEED` stub runtime |
| `fighter_379c4` | 0x379C4 | `0x5D812` allow runtime |
| `text_number_core` | 0x2EF24 | `0x65546` stub runtime |
| `text_number_format` | 0x2EFD4 | `0x61A70` stub runtime |
| `text_glyph_emit` | 0x2F5A0 | `0x5D812` allow runtime |
| `fighter_19020` | 0x19020 | `0x3E484` allow non-function |
| `fighter_19164` | 0x19164 | `0x61A4C` allow runtime |
| `fighter_slots_reset` | 0x34978 | `0x654C7` allow runtime |
| `fighter_35050` | 0x35050 | `0x29D04` allow non-function |
| `frontend_mode12_advance` | 0x4160C | `0x65490` allow runtime, `0x654C7` allow runtime |
| `frontend_mode_18_step` | 0x4F6E8 | `0x28D68` allow non-function, `0x5D812` real runtime |
| `frontend_mode_19_step` | 0x4F704 | `0x28D68` allow non-function, `0x5D812` real runtime |
| `actor_cursor_reset` | 0x38B70 | `0x654C7` allow runtime |
| `camera_bitplane_pixel` | 0x15F48 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `nameentry_key` | 0x20860 | `0x653ED` allow runtime |
| `palette_list_init` | 0x336C0 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `sound_music_pause_toggle` | 0x1D1B0 | `0x5DEAF` stub runtime |
| `sound_music_volume` | 0x1CAB8 | `0x5DECA` stub runtime, `0x5DEED` stub runtime |
| `sound_sfx_volume` | 0x1CED4 | `0x5DCC5` stub runtime, `0x5DD03` stub runtime |
| `sound_buffers_alloc` | 0x1D0BC | `0x1C308` stub host-owned, `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `fight_stage_marks_clear` | 0x4246C | `0x65490` allow runtime, `0x654C7` allow runtime |
| `fight_char_team_tag_add` | 0x4408C | `0x5D812` allow runtime |
| `fight_scene_props` | 0x412A0 | `0x412EC` allow non-function, `0x5D812` allow runtime |
| `attract_config_volumes_unscaled` | 0x2C8F0 | `0x5DCC5` stub runtime, `0x5DD03` stub runtime, `0x5DECA` stub runtime, `0x5DEED` stub runtime |
| `attract_config_volumes` | 0x2C8F0 | `0x5DCC5` stub runtime, `0x5DD03` stub runtime, `0x5DECA` stub runtime, `0x5DEED` stub runtime |
| `game_mode_25_reveal` | 0x4EF8C | `0x65624` allow runtime |
| `flow_26d4c` | 0x26D4C | `0x65533` allow runtime, `0x65546` allow runtime, `0x6C8EB` allow runtime, `0x6CB6C` allow runtime, `0x6CC9F` allow runtime, `0x6CCFA` allow runtime, `0x6CE84` allow runtime, `0x72CD6` allow runtime, `0x72D20` allow runtime |
| `frontend_mode_15_step` | 0x4F24C | `0x5D812` real runtime |
| `frontend_mode_16_step` | 0x4F2B0 | `0x28D68` allow non-function, `0x5D812` real runtime |
| `frontend_mode_17_step` | 0x4F318 | `0x28D68` allow non-function, `0x5D812` real runtime |
| `frontend_mode_1b_step` | 0x4F9C8 | `0x28D68` allow non-function, `0x5D812` real runtime |
| `fight_char_text_clear` | 0x432EC | `0x65624` allow runtime |
| `fight_char_text_clear_rr` | 0x432EC | `0x65624` allow runtime |
| `fight_char_text_blink` | 0x43464 | `0x65624` allow runtime |
| `fight_char_text_blink_rr` | 0x43464 | `0x65624` allow runtime |
| `fighter_winner_body` | 0x193B0 | `0x29D04` stub non-function, `0x3E4C4` allow non-function |
| `fighter_state_350d0` | 0x350D0 | `0x29D04` stub non-function |
| `fighter_state_3531c` | 0x3531C | `0x3D0BC` stub non-function |
| `camera_y_clamp` | 0x1317C | `0x61A4C` allow runtime |
| `fighter_think_side` | 0x3B464 | `0x29D04` allow non-function |
| `game_mode_05_step` | 0x25C88 | `0x24568` stub non-function, `0x24804` stub non-function, `0x40CB0` stub non-function |
| `game_mode_05_step_reread` | 0x25C88 | `0x24568` stub non-function, `0x24804` stub non-function, `0x40CB0` stub non-function |
| `sound_sample_start` | 0x1CB18 | `0x5DC0F` stub runtime, `0x5DC2A` stub runtime, `0x5DC4D` stub runtime, `0x5DC70` stub runtime, `0x5DCA6` stub runtime, `0x5DCC5` stub runtime, `0x5DCE4` stub runtime |
| `camera_bitplane_accum` | 0x41030 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `fighter_pass_a` | 0x1958C | `0x3E484` allow non-function |
| `fighter_pass_a_al` | 0x1958C | `0x3E484` allow non-function |
| `fight_char_team_text_blink` | 0x44638 | `0x65624` allow runtime |
| `fight_char_team_text_blink_rr` | 0x44638 | `0x65624` allow runtime |
| `actors_reset_al` | 0x2BAF4 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `config_key_name` | 0x3157C | `0x65533` allow runtime, `0x65546` allow runtime, `0x6C8EB` allow runtime, `0x6CB6C` allow runtime, `0x6CC9F` allow runtime, `0x6CCFA` allow runtime, `0x6CE84` allow runtime, `0x72CD6` allow runtime, `0x72D20` allow runtime |
| `game_coin_divert` | 0x257A4 | `0x65490` allow runtime, `0x654C7` allow runtime |
| `camera_point_side` | 0x1790C | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `camera_point_side_real` | 0x1790C | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `camera_point_side_sync` | 0x1790C | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `camera_unfreeze` | 0x170A0 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `camera_unfreeze_real` | 0x170A0 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `camera_unfreeze_sync` | 0x170A0 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `camera_projectile_step` | 0x17CB0 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `camera_projectile_step_rr` | 0x17CB0 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `camera_point_hit` | 0x17D30 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `render_list_blit` | 0x14328 | `0x51E58` allow non-function, `0x5215C` allow non-function, `0x521F0` allow non-function, `0x5222C` allow non-function, `0x529A0` allow non-function, `0x529C4` allow non-function, `0x52A18` allow non-function, `0x57F80` allow non-function, `0x57FFB` allow non-function, `0x58564` allow non-function, `0x58578` allow non-function, `0x5859C` allow non-function, `0x58CBD` allow non-function, `0x58D20` allow non-function, `0x58D2C` allow non-function, `0x58D40` allow non-function, `0x58D4C` allow non-function, `0x58D90` allow non-function, `0x5D218` allow runtime, `0x5D28F` allow runtime |
| `sprite_blit` | 0x51E5C | `0x51E58` allow non-function |
| `fight_char_screen_open` | 0x43738 | `0x5DCC5` stub runtime, `0x5DD03` stub runtime, `0x5DECA` stub runtime, `0x5DEED` stub runtime |
| `fight_char_screen_open_both` | 0x444C8 | `0x5DCC5` stub runtime, `0x5DD03` stub runtime, `0x5DECA` stub runtime, `0x5DEED` stub runtime |
| `attract_scene_tick` | 0x292AC | `0x5D812` real runtime |
| `frontend_mode_1a_step` | 0x4F9A0 | `0x28D68` allow non-function, `0x5D812` real runtime |
| `hiscore_insert` | 0x2DCA0 | `0x653A1` allow runtime |
| `menu_step` | 0x2FFC4 | `0x2CB74` stub non-function |
| `menu_run` | 0x2FA40 | `0x2CB74` stub non-function |
| `game_frame_keys` | 0x24C5C | `0x65431` stub runtime |
| `game_frame_keys2` | 0x24C5C | `0x65431` stub runtime |
| `game_frame_head` | 0x24C5C | `0x25FAC` stub non-function, `0x26194` stub non-function, `0x5D812` allow runtime, `0x65431` stub runtime |
| `game_frame_fight` | 0x24C5C | `0x65431` stub runtime |
| `game_frame_tails` | 0x24C5C | `0x65431` stub runtime |
| `game_frame_m25` | 0x24C5C | `0x65431` stub runtime |
| `game_frame_match` | 0x24C5C | `0x65431` stub runtime |
| `game_frame_front` | 0x24C5C | `0x65431` stub runtime |
| `game_frame_start` | 0x24C5C | `0x65431` stub runtime |
| `game_frame_rr` | 0x24C5C | `0x65431` stub runtime |
| `game_loop` | 0x255CC | `0x4F5C8` stub non-function, `0x501A3` allow host-owned, `0x5D812` allow runtime |
| `game_mode_32_step` | 0x296B8 | `0x24568` stub non-function, `0x24804` stub non-function, `0x40CB0` stub non-function |
| `game_mode_32_step_end` | 0x296B8 | `0x24568` stub non-function, `0x40CB0` stub non-function |
| `game_mode_32_step_rr` | 0x296B8 | `0x24568` stub non-function, `0x24804` stub non-function, `0x40CB0` stub non-function |
| `game_mode_0d_step` | 0x274FC | `0x24568` stub non-function, `0x24804` stub non-function, `0x40CB0` stub non-function |
| `game_mode_0d_step_end` | 0x274FC | `0x40CB0` stub non-function |
| `game_mode_0d_step_rr` | 0x274FC | `0x24568` stub non-function, `0x24804` stub non-function, `0x40CB0` stub non-function |
| `game_mode_13_step` | 0x424E8 | `0x412EC` allow non-function, `0x5D812` allow runtime |
| `svc_buttons_draw` | 0x31A78 | `0x65533` allow runtime, `0x65546` allow runtime, `0x6C8EB` allow runtime, `0x6CB6C` allow runtime, `0x6CC9F` allow runtime, `0x6CCFA` allow runtime, `0x6CE84` allow runtime, `0x72CD6` allow runtime, `0x72D20` allow runtime |
| `config_screen_wait` | 0x2EA78 | `0x501A3` allow host-owned, `0x655FF` allow runtime |
| `config_screen_wait_zero` | 0x2EA74 | `0x501A3` allow host-owned, `0x655FF` allow runtime |
| `game_quit_prompt` | 0x249F0 | `0x653ED` allow runtime, `0x65431` stub runtime |
| `config_storage_read` | 0x2E990 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `config_storage_touch` | 0x2D4EC | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `config_storage_load` | 0x2D638 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `hiscore_storage_load` | 0x2DE98 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `hiscore_storage_load_rr` | 0x2DE98 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `config_storage_validate` | 0x2DF8C | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `config_storage_validate_rr` | 0x2DF8C | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `config_validate` | 0x2D6F8 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `config_validate_rr` | 0x2D6F8 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `config_validate_audit` | 0x2D6F8 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `config_validate_chain` | 0x2D6F8 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `config_file_load` | 0x1AFE8 | `0x10D0C` allow host-owned, `0x10D34` allow host-owned, `0x61A70` allow runtime, `0x62002` allow runtime, `0x62003` allow runtime, `0x6201B` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime, `0x6C4CF` stub runtime |
| `config_file_save` | 0x1B084 | `0x10D0C` allow host-owned, `0x10D34` allow host-owned, `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `menu_fatal_error` | 0x2EA68 | `0x62002` allow runtime, `0x62003` allow runtime, `0x6201B` allow runtime, `0x6C4CF` stub runtime |
| `actors_table_clear` | 0x2F920 | `0x61A70` allow runtime, `0x65490` allow runtime, `0x654C7` allow runtime |
| `sound_shutdown` | 0x1D018 | `0x5D86A` stub runtime, `0x5DC8B` stub runtime, `0x5DD03` stub runtime, `0x5DEAF` stub runtime, `0x5DEED` stub runtime |
| `sprite_blit_aperture` | 0x51ED8 | `0x51E58` allow non-function, `0x521F0` allow non-function, `0x5222C` allow non-function, `0x529A0` allow non-function, `0x529C4` allow non-function, `0x52A18` allow non-function, `0x57F80` allow non-function, `0x58564` allow non-function, `0x5859C` allow non-function, `0x58CBD` allow non-function, `0x58D2C` allow non-function, `0x58D4C` allow non-function, `0x58D90` allow non-function, `0x5D218` allow runtime |
| `game_shutdown` | 0x1BE30 | `0x10D34` allow host-owned, `0x4FB98` allow host-owned |

## §J2 — host rebind: `host_key_bits` reads the configured binding scans (lane j3, task 4)

**The address (plan correction).** The task brief and the plan's Task 4
Interfaces say "track S's config image at `0x100CE4` (binding scans
`+0x2DE..+0x2ED`)". The raw disagrees with that base, and per the standing
rule the raw wins:

- The sampler reads the scans through the BIOS key record pointer:
  `FUN_0001b610` (`prage.c:7312`) tests `[DAT_00101514 + 0x254 + *[DAT_00101514
  + 0x2de]]`, `FUN_0001b730` (`prage.c:7362`) `+0x2e2..+0x2e5`, and
  `FUN_0001b6a0`/`FUN_0001b7c0` (`prage.c:7337`, `7384`) `+0x2e6..+0x2ed`;
  `FUN_0001b850`/`FUN_0001b870` (`prage.c:7406`) test the fixed F1/F2 state
  bytes `+0x28F`/`+0x290`. `DAT_00101514` is the dword holding the DOS block
  base (`flow.c:6862` sets it to `GAME_BIOS_BASE`, the port's flat scratch),
  so the scans are at `[DS_00101514]+0x2DE..+0x2ED`, not at
  `0x100CE4+0x2DE` (= `0x100FC2`).
- The probe agrees: `probe.log` line 3 is
  `K ms=5785 ptr=0000FE20 2d4..2ed=000000000000000000001f2d2c2e1617313248504b4d47494f51`
  — the dump is `ptr+0x2d4..0x2ed` of the `[0x101514]` block, and the 16 scans
  (`1F 2D 2C 2E 16 17 31 32 48 50 4B 4D 47 49 4F 51`) sit exactly at `+0x2DE`.
- `config_keys_apply` (`config.c:1282`; 0x1AE85/0x1AEC0) writes the
  player's scans to `DSB(bios + 0x2DE + i)` / `+ 0x2E6 + i` from a 0x28-byte
  record; `config_keys_load` (0x1AF64; 0x1AF99/0x1AFBE) does the same from the
  packed record in the 0x20CF0/0x20CF9 round trip. The boot record is the
  constant at `0xA2C62` (0x1AE20, `config_keys_apply_defaults`): its bytes
  `00 00 73 1F 78 2D 7A 2C 63 2E 75 16 69 17 6E 31 6D 32 00 00 00 48 00 50 00
  4B 00 4D 00 47 00 49 00 4F 00 51 64 00 64 00` (file offset `0xA2C62 +
  0x46E54`, read for this closure) have the scans as their high bytes.
- `0x100CE4+0x2DE` is a different region: image offset 0x2DE lies inside kind
  6's codeword (image bytes `0x100ED0..0x100FCF`; §S.1's kind table), i.e. the
  encoded high-score table, not scans. The image *does* carry a key-config
  mirror in its unmanaged tail at `0x1014AC..` (scan at `+0x1014AE+2i` and
  `+0x1014C0+2i`; `config.c`'s §50-C comment), and a saved rebind round-trips
  through it (0x20CF2 packs it to a record, 0x20CF9 loads the BIOS scans back),
  which is likely what the plan's "the config image holds the scans" meant; the
  authoritative read for the sampler is still the BIOS record. A raw-file
  search finds the contiguous default-scan sequence nowhere in PRAGE.EXE.

**What changed.** `host_key_bits` is the host's stand-in for 0x1BBAC; it now
resolves each key's scan against the configured binding, with the boot record's
scans as the per-position fallback (`port/src/host.c`):

- `k_bios_default_scan[16]` (the 16 positions' boot scans in
  `+0x2DE..+0x2ED` order) and `k_bios_pos_bit[16]` (the kb-word bit each
  position contributes) replace the old hardcoded switch;
- `host_kb_bit(scan)` returns the OR of every position whose *effective* scan
  (the record's, or the default when the record byte is 0) equals `scan`, plus
  the fixed F1/F2 bits (0x3B → 0x0100, 0x3C → 0x0001; 0x1B850/0x1B870 read
  `+0x28F`/`+0x290`). A duplicate binding therefore sets every bit the raw's
  per-position combine would.
- `host_key_bits` keeps the override check as its first statement (the seam
  `host_set_key_bits_override`, host.c:104-108, is untouched) and its SDL
  loops; the per-key result is `host_kb_bit`. `host.c` now includes `mem.h` and
  `symbols.h` to read `[DS_00101514]+0x2DE..+0x2ED`; `host.h`'s two doc
  comments were updated to describe the configured binding.
- PORT deviation, named: the raw has no fallback (a zero scan indexes state
  byte 0); the port substitutes that position's default scan so the no-config
  case (host calls before `game_init`, the unit suite) reproduces the previous
  behaviour exactly. In a run of the port the record is never zero after
  `game_init`'s `config_validate` → `config_set_defaults` →
  `config_keys_apply_defaults`, so the fallback is not reached in play.

**Tests (RED then GREEN).** `port/tests/test_platform.c` `test_host`, same
block, seed-then-post-condition: a scratch record at `0x03F00000+0x2DE`
(local `DSB`/`DSD` save/restore of the base and both byte ranges) is seeded
with a rotation of the 16 default scans (position i holds position (i+1) mod
16's default), then zeroed. RED at base — the assertions ran against the old
switch, this change's exact inverse — 4 failures at
`test_platform.c:2978/2979/2980/2985`: `16384 != 32768` (host_kb_bit(0x2D)),
`32768 != 8` ((0x1F)), `64 != 128` ((0x50)), `64 != 160` (two positions, one
scan). GREEN after the change: all pass,
including the duplicate (`0x50` bound at positions 8 and 10 → `0x0080 | 0x0020`),
F1/F2 fixed, and the zeroed-record case reproducing the 18 default scan→bit
pairs and the three non-pad scans (Q, Alt, '5').

**No pinned path moved.** `PR_ORACLE_REQUIRED=1 ./build/run_tests` with the
replay seam: the gp-pads replay + `gp_compare --report` (private dump
`GP_DUMP=/tmp/pr_gp_dump_j3`, T3 owns `/tmp/pr_gp_dump`) is byte-identical to
the base run of the same command (diffs empty; "0 restart(s) landed", the two
`0x5D812` miss rows, the frame/trace report all equal). The `PR_GP_DUMP` driver
holds the override, and `host_key_bits` returns before SDL/mem are touched.
Suite environment note (this Linux/aarch64 box, independent of the change): a
direct `run_tests` aborts after `test_host` in `test_p45_p5_spawns`
(`actor_spawn`, `actors.c:3865`) — measured the same at base (the pre-change
test file rebuilt from `git stash`) — and `test_platform.c:757` fails because
the resource index names are lowercase while the checkout's files are uppercase
(case-insensitive filesystems only). Under gdb (ASLR off, the only way it
completes here) the whole suite runs: base `FAILURES: 5` (the resource one +
the 4 RED), after the change `FAILURES: 1` (the pre-existing resource one).

**SDL failure-branch reachability on this host (Linux/aarch64, SDL3 3.4.18),
deterministic runs, no forced tests:**

- Reachable: the windowed path opens with the dummy driver
  (`SDL_VIDEODRIVER=dummy`), audio opens (`49716 Hz`), the game runs; a
  SIGTERM (what `timeout` sends, which SDL3's default handler turns into
  `SDL_EVENT_QUIT` → the port's quit flag) ends the loop cleanly and
  `game_shutdown`'s `config_file_save` writes the 2040-byte save — measured,
  not the idle reboot: `timeout -s KILL` leaves no file, `timeout -s TERM`
  (1 s is enough) writes it.
- Reachable (forced, deterministic): the no-display failure branch — both
  `SDL_VIDEODRIVER=doesnotexist` and `DISPLAY=:99 SDL_VIDEODRIVER=x11` give
  `prageport: no window/display available`, exit 1 (host_init's `SDL_Init`
  arm); the audio-init failure arm — `SDL_AUDIODRIVER=doesnotexist` gives
  `audio unavailable (SDL_InitSubSystem(SDL_INIT_AUDIO): ...); running silent`
  and the game runs on (the host has an HDMI ALSA sink, so the natural default
  is success, unlike the macOS `-66681`).
- Not reachable here: `SDL_CreateWindow`/`SDL_CreateSurface` failure after a
  successful video init (the dummy driver accepts everything),
  `SDL_OpenAudioDeviceStream` returning NULL or `SDL_ResumeAudioStreamDevice`
  failing after a successful audio-subsystem init, and the defensive
  `SDL_GetKeyboardState(NULL) == NULL` arm. These stay unexercised rather than
  forced (brief: no forced tests for unreachable branches).

**Live-play notes (notes only; assembled from the records and the runs above).**
The playable key map is the boot binding: P1 S X Z C (up/down/left/right),
U I N M (the four attack buttons), F1 (start, also button 0); P2 the arrows,
Home/PgUp/End/PgDn and F2 (start/button 0), and F2 mid-match joins the second
human side (0x2CA93);
Enter confirms menus, ESC backs out (and at the title/menu prompts reaches
QUIT TO DOS?, whose yes quits; elsewhere it reaches ABANDON CONQUEST?, which
saves and soft-restarts), Space pauses a match, Backspace is DEL in name entry,
Alt-Q/S/M/J are the int 16h shortcuts, and closing the window is the host's
quit (it works even while a movie would swallow ESC; on this host SDL turns a
SIGTERM into that same quit, and the clean exit writes the save). After this
change a rebind set in the service menu's CONFIGURE KEYBOARD takes effect for
the next frames, and it is saved/loaded with the session (the image tail mirror
round trip); the limit, named: the host samples the game's pad keys and the
letters only, so a CONFIGURE KEYBOARD binding to any other scan (a digit,
F3–F10, Tab, or Backspace's 0x0E) is inert in the windowed port; the idle timer
soft-restarts into the boot logos after ~20 s with
no latched key (0x2EBB3; record named-gaps A §A.6 — record-based, not re-measured
here); "credits" remains today's behaviour — no coin key, credits come from the
config image (INSERT 1 COIN with no way in except the hidden service
menu/config), not implemented.
