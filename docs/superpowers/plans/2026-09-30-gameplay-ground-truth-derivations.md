# Gameplay ground truth (U1–U4): derivation record

Spec: `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md`.
Plans: `docs/superpowers/plans/2026-09-30-gameplay-u{1,2,3,4}-*.md`. Every
claim cites a raw address, a `poll.log` line
(`data/k11-captures/<scenario>/poll.log:<n>`) or a capture frame
(`<scenario> frame <i> (raw <r>)`). On any conflict the raw wins; corrections
are recorded here with their address.

## §G.0 Baseline (U1 Task 0)

Commands (the plan's Task 0, run in the worktree `.worktrees/gameplay-u1` on
branch `gameplay-u1` from `gameplay-ground-truth`; `data` and `.superpowers`
are symlinks to the main checkout's, the two git-ignored test fixtures
`port/tests/ghidra_data.bin` and `port/tests/title_screen_ref.ppm` copied from
it). `make verify` ran with the per-agent overrides `SMK_DUMP=/tmp/pr_u1_smk
TITLE_DUMP=/tmp/pr_u1_title ATTRACT_DUMP=/tmp/pr_u1_att FRONTEND_DUMP=/tmp/pr_u1_fe
TITLE_PIN_DIR=/tmp/pr_u1_pin AUDIO_WAV=/tmp/pr_u1.wav K11_DUMP=/tmp/pr_u1_k11`
(other agents run gates concurrently; the defaults are shared /tmp paths).

- HEAD `ff686d6dc13460cdefa7440ef0f59c692781cd38` (docs: gameplay ground-truth plans U1-U4).
- `verify-exit=0`.
- Oracle lines: `grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)'`
  → `/tmp/gameplay-u1/or_base.txt`, 45 lines, sha256
  `eaca80cf8d4a3bffe980472ea110ddf4bf038975003ed9ec6a76da6d5a92454a`; identical
  (`diff`, no output) to the controller's baseline
  `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`.
  (Correction to the plan's Step 2 grep: the gate's pattern includes
  `== demo-fight` and keeps the K11 lines apart; the K11 lines are
  `/tmp/gameplay-u1/k11_base.txt`, 12 `k11_compare:` lines: walk `81 frames in
  window: 47 clean, 2 splice, 0 transition, 2 unexplained, 30 all-black`,
  `0 unexplained in the window`; menuesc `283 frames in window: 184 clean, 92
  splice, 3 transition, 0 unexplained, 4 all-black`, `END 388 must be >= 388: ok`.)
- K11 tool unit tests: `Ran 40 tests` (the verify line for
  `tools.tests.test_k11_{fields,session,capture,compare}`).
- `python3 tools/port_progress.py`: `771 1203 64` and
  `731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)`.
- `shasum -a 256 data/game/C/PRAGE.EXE`:
  `eecba701576d36d1a217a271acd90e9aa4473121db8d51e8c8085c194ce0e91b`.
- CMOS: `2040 0` (2040 bytes, none non-zero: the defaults path).
- `make title-pin TITLE_PIN_DIR=/tmp/pr_u1_pin`: the pinned copy's sha256
  `8120f1bd1df389ed94cb329c030f95d9e38caad193bbd9840717af557161a68d` (as the plan expects).
- `dosbox-x -version`: `DOSBox-X version 2026.08.31 SDL2, copyright 2011-2026 The DOSBox-X Team.`

## §G.1 Raw facts (U1 Task 1)
## §G.2 gp_session: constants, pads, log format (U1 Task 2)
## §G.3 The scheduler (U1 Task 3)
## §G.4 Port script v2 and trace diff (U1 Task 4)
## §G.5 gp_capture (U1 Tasks 5–6)
## §G.6 Make targets (U1 Task 7)
## §G.7 The gp-pads capture (U1 Task 8)
## §G.8 U1 closure (U1 Task 9)
