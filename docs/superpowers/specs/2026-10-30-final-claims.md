# Final claims — the terminal claim register

**Status:** implemented by Final truth Task 8; the final gate (Task 9) passed
on 2026-10-31 — two `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make
verify` runs plus `make audio-render`; the phase merged as `43b79b30`. Every
remaining gap in the project has one row below with a measured status; no row
is "open". This register records what Tasks 1–7 measured; it adds no claim, no
capture and no pin.
**Plan:** `docs/superpowers/plans/2026-10-30-final-truth.md` (Task 8).
**Design:** `docs/superpowers/specs/2026-10-30-final-truth-design.md` §3, §9,
§10.
**Derivation record:** `docs/superpowers/plans/2026-10-30-final-truth-derivations.md`
§F1–§F8.
**Docs test:** `tools/tests/test_final_claims.py` (in `make verify`), which
also guards the `AGENTS.md` gap statements.

## 1. How to read a row

| status | meaning |
|---|---|
| **closed** | The gap was closed by a fix, exhibit or oracle in this phase (or by the earlier phase named in the evidence); the closing evidence is named. |
| **gated** | An oracle or counter enforces the claim; no remaining gap is claimed outside it. |
| **terminal** | A measured limit of the port or the comparison model. The terminal reason and the evidence that proves it are named; no further work is authorized by this phase. |
| **declared** | A real branch or route was measured but is not claimed (no oracle or pin), or is blocked by a missing capture. The evidence is named and the residual claim is the narrow one stated. |

Rules, in the order the docs test enforces them:

1. There is no `open` row. A gap that cannot be closed in the phase is
   terminal or declared, with its measured reason.
2. Every terminal and declared row names an address (`0x…`), a file with a line
   (`path:line`), a `data/k11-captures/…` capture, or a `.failed` artifact.
   "Prose only" is not evidence.
3. A `.failed` artifact publishes no oracle, pin, miss set, manifest entry or
   `CAPTURE_DIRS` path; its row is game evidence only.
4. A new measured gap needs a new row and the task result that measured it;
   the guard fails when an `AGENTS.md` gap statement has no row.
5. The register does not widen the pixel, audio or input claims. The narrow
   claim in the last column is the whole claim.

## 2. The register

| id | gap | status | evidence | terminal reason | residual claim |
|---|---|---|---|---|---|
| R01 | `0x2CADC` boot-defaults message, wait and release | closed | `port/src/game/config.c:268` (`config_set_defaults`), `port/src/game/config.c:910` (`config_screen_wait`), `tools/diff_verify.py:32971` (the ten-call row), record §F1.1–§F1.6 | closed by Task 1; the three calls are wired with the raw's arguments (`0x2F198(-1,0xE,0x809C8,0xB000)`, `0x2EA78(0xB4)`, `0x2F280(-1,0xE,0x809F0,0xB000)`) | The calls and their derived arguments. The message's rendered pixels are not gated (the wait's present is outside every capture's frame set); no frame pin was added. |
| R02 | 5120 band (13 pixels at capture 5120) | terminal | `data/k11-captures/gp-u13-band-dac` frame 05130, the S record at `f=0x1826` with `bb = 32×8, 2E, 4D×4`, record §F2.5; the allowance `(5120,1,5,56b900b1…)` is unchanged | **PASS-A:** the band's index state is the port's and the displayed pixels follow the game's DAC; the old `00001c` is the capture/present-side class. The exact palette/scan-out transient is not named. | The content/DAC claim and the unchanged allowance. The diagnostic capture is outside `CAPTURE_DIRS` and adds no pin. |
| R03 | U14 two-human fought win (`gp-u14-twop-win`) | declared | `data/k11-captures/gp-u14-twop-win.failed` (round-1 KO `f=0xB1B`, match result `f=0x13A8`, mode `0x12` at `f=0x1693`, zero `W`), record §F3.5; `gp_twop.py check --win` | The one authorised run failed `port script v2` (no `S(0xB1A)`; 15 of 5,820 snapshots missed), so it publishes no oracle, pin, miss set, manifest entry or `CAPTURE_DIRS` path. | Game evidence only: the game's own 2-0 two-human win. It is not an arcade win. |
| R04 | U14 R1 arcade fought win | terminal | `data/k11-captures/gp-u14-win-live.failed` (arcade `b1f=1`, P1 SAURON `c0=0`, P2 VERTIGO `c1=3`), record §F3.7 (round 1: P1 2/25 vs AI 10/120; round 2: P1 8/92 vs AI 9/120; sources `f=0x73F`/`0x77C`, `0x470..0x86C`, `0xBC1..0xE85`, `0xB85..0xEE5`); `gp_twop.py check --ko --arcade` | The arcade AI's damage race (120 in each round vs the script's 25/92) decides the match; no won arcade round is measured in three attempts. The `b0c` write is the final's `0x37FF6` chain and cannot rescue a match-1 win. | The one-human arcade KO is game evidence (live truth §L8). The fought win is not claimed. |
| R05 | `0x21` volleyball | declared | Record §F6: `h(n) = 568n - 8n²`, 71 motion steps, the practical window `n = 65..71`; four measured `0x4B470` calls (`gp-idle-loss` `f=2238` ×2 and the demo `f=2582` ×2, all `via=1`/`+0x1F=1`); the gate chain `fight.c:3080-3086` and the entry `0x4BD98`; census `0x4BD98`/`0x4BF18` `NOT-HIT` | Declared: the mechanism is reachable (the design's count-max-1 blocker is refuted) but no frame-exact route is derivable from the checked-in bytes (a 1-in-7 stage, a crowd lie, eight strikes in a 7-step window). The one capture was not spent. | The mechanism and the four first-hit measurements. No mode exhibit; the route sketch and the port-side search are future work. |
| R06 | `0x25` bowling | closed | `data/k11-captures/gp-bowling-2` (1,491 frames; entry capture 1,352; `f=0x832`: `b892` 1/3/5/7 then 0, `s0_53=0x0C`, `camx=-606`, `b1f=3`, `c0=c1=4`, zero `W`), `gp-bowling-oracle` (N 1,422, trace/moves 2,159), record §F5.5–§F5.6; census 16 mode-`0x25` rows HIT | closed by Task 5: the count, not the camera, was the residue; the get-up's `0x35D6C` clear supplied the gate's zero | The entry stores, the counter's growth rules (at most +2 a frame, only the `>= 6` rejection's -1, a visible maximum `>= 6`) and the **entry frame's** `camx` inside `±0x3300` (the wider `-1433..4887` span is a record measurement, not a gate). The frame claim reaches capture 1,421 (entry +70); the scoreboard is not claimed. |
| R07 | `0x0E` taken-continue (`0x2791C`) | closed | `data/k11-captures/gp-u29-continue-taken` (4,446 frames; `f=0x1561`: mode `0x0E`→`0x0C`, `cred` 4→3, `b1f` 0→1, `+0x5A` `0x78`→0, `cnt` `0x0D`), `gp-u29-continue-taken-oracle` (N 4,380, trace/moves 5,569), record §F4.4–§F4.5; the interim census 659/96/4; census `0x2791C`/`0x33B00` HIT | closed by Task 4: the take is the game's own `0x2CA9C` debit and `0x2791C` restart | The take and restart. The route's KOs are harness writes, not a fought KO or win. |
| R08 | `0x0E` continue screen, countdown and expiry | closed | `data/k11-captures/gp-u28-continue` `f=0x18C2` mode `0x15` `b25=0` (transition `f=0x18C0`), `gp-u28-continue-oracle` (N 4,596, trace/moves 6,339), live truth §L5 | closed by live truth Task 5: every countdown value 15→0 at 64-frame spacing, PRESS START at 13, the mode-`0x15` expiry | The expiry branch. The poked KOs are harness writes; the taken path is R07. |
| R09 | Audio: 12 whole-section-claimed banks | gated | `tools/audio_compare.py` `TRACE_CLAIMED_BANKS`/`TRACE_PINS`/`TRACE_PREFIX_PINS` (74 diff-allowed prefixes, 29,656 ticks), record §F7.2, `make audio-oracle` | gated: the whole-section matches and the leading prefixes are pinned; a one-tick-short prefix fails | Whole-section matches for 12 of the 26 code-reachable banks. The register stream is the game-owned claim. |
| R10 | Audio: 10 requested-unclaimed banks | terminal | `tools/audio_compare.py` `TRACE_RESIDUE` (10 rows, first-diff ticks 0..1,437, causes `voice-rotation`/`tail-lag`), record §F7.2 | The mismatch is the driver's voice-rotation/tail model, not a missing capture. The rows are pinned in the `trace-residue` region and the one-tick-earlier mutation fails. | The measured first-diff tick and cause per bank; the bounded prefix claims. |
| R11 | Audio: 2 requested-but-unattributable (`grave+0x0A8C`, `grave+0x05E4`) | declared | Record §F7.2 (the `gp-u10-ending` S rows: `0x0A8C` at frames 5323/5363/5403; `0x05E4` at frames 5311..5433), the live truth §L11.3 superseded note | Each start writes only the two tick-0 reset registers before the played song's first key-on, so the section model cannot attribute it. No capture-side claim. | The runtime request is proven; no section is claimed. |
| R12 | Audio: 2 never requested (`sound+0x7714` `0xE4`, `konsd+0x1CCB0` `0xEB`) | declared | Record §F7.3 (three reaction callbacks `0x37640`/`0x37774`/`0x37898`, the shared gate `0x3766E`/`0x37678`; t9-r5's 11 S records do not request `0xE4`; the `0x24559` site in `0x24508`, data pointer `0xD50E2`) | Needs a capture whose `+0x42` bit-5/`+0x43` bit-3 gate passes (the U16/finisher family), and the `0x24508` stream. No new capture was taken. | The callbacks and gates; no audio section. |
| R13 | Rendered audibility | terminal | Record §F7.4; `port/src/platform/audio/opl/opal.c:3` (Opal 2.0.3) and `port/src/platform/audio/mixer.h:50` (`MIXER_OPL_RATE` 49716) vs DOSBox-X at 48 kHz; `make audio-render` selmo peak 2790 sha `530a9430…`, title `515b7c94…` | The PCM is the emulator/mixer model (core, rate, mixer and voice allocation differ), not a game claim. Exact equality is not measurable. | The register stream and the PCM determinism pin, not audibility. |
| R14 | U17 wall-paced device-2 walk | terminal | `data/k11-captures/gp-u17-device2`, `gp-u17-oracle`, record §H6.9; `AGENTS.md:464` | The walk is wall-paced and has no tick in its `I` records, so the frame-keyed replay cannot follow it. The screens are already covered by the K11 captures. | The no-pad clear and the device-2 screen proof; no frame/trace ratchet. |
| R15 | U16 R2 case-3 gate operand | terminal | Record §L4 and claim-coverage §C5: the t9-r5 set `f=0x1531`, `y=0x0EAE < 0x1400` (the clear is strict) | The original's exit state is below the gate; the `.failed` artifact is not that measurement. No fix and no third capture. | The measured operand only; the finisher gap is R16. |
| R16 | U16 finisher reaction | declared | `data/k11-captures/gp-u23-u16-finisher`, the `gp_u23` moves ratchet (`r0=0x27` vs the capture's `0x33`), record §E2; `AGENTS.md:393` | The live KO could not be reconciled with the narrow `0x36F10` gate window (six attempts); the capture is the labelled poked-damage fallback. | The ending reaction content. The KO by hits and the six earlier won matches are not claimed. |
| R17 | Differential proof debt | terminal | `tools/tests/test_diff_verify.py` `TERMINAL_CENSUS`/`WEAKER_CENSUS`/`UNROWED_CENSUS`; the counter `1033/1033`, `14993/14993`, 0 named gaps, `825/845` with callees, `810/845` full, 8 unrowed, `WEAKER` 12; 20 open rows over 19 addresses | Each open row is a declared terminal (15 AIL thunks, `0x1D0BC`, `0x1BBAC`, the three exit-flush rows on `0x6C4CF`). No row blocks on a portable function. | The counter and the per-row reasons. The 20 rows are not full rows. |
| R18 | Coverage census | terminal | `tools/coverage_baseline.txt` HIT 675 / NOT-HIT 80 / UNINSTRUMENTED 4; the 4 are `0x29D60`, `0x2EA30`, `0x2EA64`, `0x500BB`; `make coverage` | The 4 are inlined one-liners with VERIFIED rows; the NOT-HIT set is the measured not-exercised set. | The measured split; no instrumentable row is added. |
| R19 | Storage | terminal | The `0x2EA68` row (`config_image_store`), record §S.8/§S.10, `port/src/game/config.c`, `tools/port_classification.txt`; `AGENTS.md:775` | `0x2EA68` returns where the original exits; a short `CMOS` gives the defaults; the slot-13..35 low-byte record is written nowhere; the four PLAUSIBLE items are declared. | The save-reload capture's five ranges and the declared terminal differences. |
| R20 | Classification (53 host-owned/deferred, 37 replaced, 315 runtime) | terminal | `tools/port_classification.txt`; `python3 tools/port_progress.py` prints `ported 798 (759 portable + 39 runtime) host-owned 41 deferred 12 replaced 37 runtime 315`; record claim-coverage §C4 | The 405 unported functions are non-targets, one evidence line each. The AIL runtime set is named unreproducible. | The measured partition; no new portable function. |
| R21 | Front-end/K11 window starts | terminal | `AGENTS.md:277-286` (the front-end window START is port-derived), `AGENTS.md:305` (K11 `K11_OPEN_END`), `tools/k11_compare.py`; record §G.16 | The window start is port-derived and interior coverage is not ratcheted. The claim is bounded-missing, not complete coverage. | No content-bearing capture frame inside the window is unexplained; a mid-window port frame no capture exhibits is ignored. |
| R22 | Gameplay order/coverage | terminal | `AGENTS.md:328`, record §G.16, `tools/gp_compare.py` | The order of the port's frames and whether every port frame appears are not claimed. A coverage count is reported, not gated. | No unexplained content-bearing capture frame in the window; under-render inside the window is not detectable. |
| R23 | U10 death streams and mode-`0xF` content | terminal | `AGENTS.md:364`; record gameplay-u9-u10 §W.16; the replay reaches `0x29C78`/`0x37DD4` but the traced fields cannot see the per-hit death streams | The clean trace to `f=0x26E1` does not claim the death streams or the mode-`0xF` content are reproduced, and is no evidence past the frame ratchet's 5437. | The trace/WIN fields to the replay's end; not the death content. |
| R24 | gp-keys capture 835 band | terminal | `AGENTS.md:334-346`, `tools/gp_compare.py` `SCAN3_ALLOWED` and the gp-keys model, record §K.10 | Capture 835's 11-row band holds a present state no port frame carries (the §K.10 item 2 named gap), pinned by sha256 and rows 0..80/92..199. | The byte-pinned band only; the present state is not isolated. |
| R25 | Label-shift flake | terminal | `docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md` §F6.3 (the same register/value data with 180 `W` frame labels one lower), quoted in final-truth §F7.4; `gp-u18-talon-ending` | A host-load capture-label shift moves tick labels without changing the game's output. No pin is weakened for it. | The register/value data; the labels are the capture model. |
| R26 | `memsize=64` | terminal | `AGENTS.md:365-366`, record §W.13, `data/k11-captures/gp-u10-ending` | The original ran out of memory in the poked final at 16 MB. `memsize=64` is a harness value and claims nothing about the game's memory need. | The poked-final content only. |
| R27 | Capture inventory and the declared U14 absence | declared | `make gate-check` reads 43/44 present (`data/k11-captures/gp-u14-win-live` absent), `tools/capture_manifest.json` 44 entries, record §F3.6 | U14 R1 is terminal (R04), so the path stays absent; `GATE_NAMED_GAPS` declares it. | 43 present, one declared skip. The two new captures are `gp-u29-continue-taken` and `gp-bowling-2`. |
| R28 | Music-truth Task 6 optional items | declared | `AGENTS.md:520-522`, the music-truth plan/record §M6.6 | The service-menu language reload is optional, the coin-countdown capture is forbidden by the no-new-capture constraint, and the proof-completion housekeeping is not a claim. | None. The in-match/menu music and the 74 prefixes are R09. |
| R29 | Audio unmatched diff-allowed remainders and later sections | terminal | `AGENTS.md:509-511`, `tools/audio_compare.py` `TRACE_PREFIX_PINS` (74 prefixes / 29,656 ticks), record §L11 | Only the leading matched prefixes are pinned; the unmatched remainder and the later `0x31`/match sections the capture's cursor advanced past are not claimed. | The prefix pins; no whole-section claim for the remainder. |
| R30 | `0x21`/`0x25` poked 1P exhibits | declared | Record §F6.8/§F6.9, the `0x4BD98`/`0x4E11C` gates (`DS_00104ABC = (b1f == 3) + 1`, `port/src/game/flow.c:1632`/`2416`), the state-coverage §9 stop rule | No poke writes `b1f`/`DS_00104ABC`; a poked two-human exhibit needs an explicit operator reversal not taken. | The real-input routes only (R05/R06). |
| R31 | `0x1C930`/`0x1CF40` audio `UNROWED` | terminal | `AGENTS.md:511-513`, `tools/tests/test_diff_verify.py` `UNROWED_CENSUS`, record §P10 | A row would fabricate the AIL handle and could not compare the hand-off. | The ported functions; no differential row. |
| R32 | The 4 `UNINSTRUMENTED` coverage addresses | terminal | `0x29D60`, `0x2EA30`, `0x2EA64`, `0x500BB`; `tools/coverage_baseline.txt`; record §D7.5 | Each is an inlined one-liner with a VERIFIED row; instrumenting them is low value. | The 675/80/4 split (R18). |
| R33 | Movie audio (zero-payload) | closed | `tools/smk_info.py` prints `audio_descriptors=[(0,0) x7]` and `audio_frame_flags=[]` for `TWI5.SMK` (121 frames) and `TWG.SMK` (41); `0x1C740` asks for all seven tracks (`0x1C76C-0x1C774`), `0x63180` opens a stream only for bit `0x40` (`0x6319D`), and `0x65240` takes the no-audio time path (`0x65246`, `0x652BC..0x6532D`); record `2026-10-21-host-input-truth-derivations.md` §H1 | zero-payload finding: neither shipped movie declares an audio track, so no stream opens and the 13 deferred movie-audio rows carry zero payload; the report-only Alt-J/boot probe is not movie audio | No movie-audio oracle is possible on the shipped data; the port plays silent through the time path. |
| R34 | Audio carrier-TL latch (Task 7a) | closed | Record §F7.1: the raw logs the raw CC7 at `0x68B0E` (`0x688E0`), scales it at `0x68C8B` by the sequence volume, and re-sends the raw log at `0x69320`; `port/src/platform/audio/sequencer.c:206-216,283-291,437-446`; `port/tests/test_audio.c` `test_sequencer` 4f (100/97/93/86/80/73/66/59/53/49; two mutations fail) | The raw and port agree; the carrier-TL exclusion stays exactly the residual rows of the residual channels | No fitted constant; the one sequence-volume exclusion is the measured residual, not a claim. |

## 3. Measured counters (not gap rows)

These are the values Task 9 re-measures; none is a claim beyond the measured
counter.

| counter | measured value | evidence |
|---|---|---|
| Ported | `798 1203 66` raw, `759 759 100` portable | `python3 tools/port_progress.py`; `README.md:1` keeps 66% raw |
| Differential | `1033/1033` functions VERIFIED, `14993/14993` mutants detected, 0 named gaps, `825/845` rows with callees closed, `810/845` full, 8 unrowed, `WEAKER` 12 | `make diff-verify`; `tools/tests/test_diff_verify.py` |
| Coverage | HIT 675 / NOT-HIT 80 / UNINSTRUMENTED 4 (sums to 759) | `tools/coverage_baseline.txt` |
| Captures | 44 explicit paths, 43/44 present, one declared skip | `make gate-check`; `tools/capture_manifest.json` |
| Pin baseline | `d3b6969f60a489c910ca1fa0f8daafb0dbead9b0fc172dda193260d3b6cd3c07` | `python3 tools/capture_manifest.py --check-pins` |
| Audio | 74 diff-allowed prefixes / 29,656 ticks; 12 claimed + 10 residue + 2 unattributable + 2 never requested | `tools/audio_compare.py`; record §F7.2 |
| README title | 66% raw (the portable figure is 100%) | `README.md:1` |

## 4. Superseded wording (the docs reconciliation)

The rows above replace these earlier statements. Each superseded phrase is
either removed from the authoritative docs or marked superseded beside the
historical text; the docs test pins the current gap statements.

| superseded wording | superseded by | where reconciled |
|---|---|---|
| "`0x25` stays declared / never opened / no oracle or pin"; "the precise remaining blocker is unresolved" | R06, Final truth Task 5 (record §F5.5–§F5.6): `gp-bowling-2` opened the mode at `f=0x832`; the count, not the camera, was the residue | `AGENTS.md` 0x21/0x25 block, `docs/PROGRESS.md` live-truth Task 6/state-coverage Task 4/Task 12a, `port/RE_GUIDE.md` 0x25 passage, design §3.3 |
| the taken-continue `0x2791C` path "is not claimed" and "expected to become HIT" is wrong | R07, Final truth Task 4 (record §F4.4–§F4.5): `gp-u29-continue-taken` takes the prompt and the census improves `0x2791C`/`0x33B00` | `AGENTS.md` continue-screen block, `docs/PROGRESS.md` live-truth Task 5/Task 12a |
| "U14 R1 stays declared"; "U14's `live_death` rule mismatch" | R04, Final truth Task 3 (record §F3.1–§F3.7): the rule is split, the two-human win is `.failed` game evidence, R1 is terminal | `AGENTS.md` U14 blocks, `README.md`, `docs/PROGRESS.md` state-coverage Task 1/Task 13, `port/RE_GUIDE.md` |
| "the `b0c` setter runs on the **ending** path" (and "the ending path" as the byte's consumer) | R04, Final truth Task 3 (record §F3.1; design §3.1): the setter's only consumer is `game_mode_0d_step` (`0x274FC`), entered from the **final's** seven-opponent arena (`0x272DC`/`0x27380`) — the final's `0x0C→0x0D` chain, not an ending path and not a match-1 win (`0x09→0x17→0x12`) | `docs/superpowers/specs/2026-10-29-live-truth-design.md` §2.7, `docs/superpowers/plans/2026-10-29-live-truth-derivations.md` §L1.4, `docs/PROGRESS.md` state-coverage Task 13, `AGENTS.md` U14 block |
| "the 5120 band stays a declared gap"; the old "INCONCLUSIVE" status as the final verdict | R02, Final truth Task 2 (record §F2.5): `gp-u13-band-dac` proves PASS-A; the old blue is the capture/present-side class; the exact transient is unnamed | `AGENTS.md` 5120 lines, `docs/PROGRESS.md` state-coverage Task 11/Task 14, `port/RE_GUIDE.md`, design §3.4 |
| "4 never requested" (grave+0x0A8C, grave+0x08, sound+0x7714, konsd+0x1CCB0) and the "10/4" bank split | R09–R12, Final truth Task 7 (record §F7.2): 12 claimed + 10 residue + 2 requested-but-unattributable (`0x0A8C`, `0x05E4`) + 2 never (`0x7714`, `0x1CCB0`); `grave+0x08` is the claimed 1,200-tick section | `AGENTS.md` music-truth bullet, `docs/PROGRESS.md` live-truth Task 11/Task 7, `port/RE_GUIDE.md`, design §3.5, live-truth design §2.6 and plan Task 10/12 |
| "the count can only reach 1"; "structurally unreachable/blocked" for `0x21` | R05, Final truth Task 6 (record §F6.4): `h(n) = 568n - 8n²`, 71 motion steps; the first motion step adds `0x230`; the latch returns on the 35 `vy < 0` passes; each hit re-launches | `AGENTS.md` 0x21 block, `docs/PROGRESS.md` final-truth Task 6, design §3.2, plan Task 6 |
| "a character with a non-zero stage base" as the `0x21` route sketch | R05: the two-human pick is `rng(7)` at `0x2587C` (1-in-7); the base arm is the 1P pick | record §F6.8 |
| "the count is zeroed only on landing" without the spawn/rebuild clears | R05: `0x49FAF` on landing and `0x49622`/`0x4D061`/`0x4E4E8` on spawn/rebuild | record §F6.4, design §3.2 |
| "`+0x53` is held at 7 for 91 frames"; the 91st-frame clear model | R06, Final truth Task 5 (record §F5.5): the measured lattice is `7`, `0x0A` (pose), `0x0B` (floor), `0`; the get-up `0x35D6C` supplied the gate's zero and the `0x367DC` arm never fired | `port/RE_GUIDE.md`, design §3.3, plan Task 5 |
| "`0x2CADC` is a terminal deviation" and "the 180-tick wait" | R01, Final truth Task 1 (record §F1.3): `0x2EA78(0xB4)` runs `n + 2` = 182 passes and one present; the calls are wired and the row is full | `AGENTS.md` differential counter, `docs/PROGRESS.md` final-truth Task 1, design §3.7, plan Task 1 |
| "13 `WEAKER`, 809/845 full" | `WEAKER` 12, `810/845` full (Task 1 removed the `0x2CADC` WEAKER row) | `README.md` status, `port/RE_GUIDE.md` differential block, `AGENTS.md` proof-debt block |
| "42 explicit capture paths, 41/42 present" | 44 explicit paths, 43/44 present (Tasks 4–5 added `gp-u29-continue-taken` and `gp-bowling-2`) | `README.md`, `AGENTS.md` continue-screen/capture lines, `docs/PROGRESS.md` live-truth Task 12a, `port/RE_GUIDE.md` |
| "the `0xE4` bank: reaction 0x32 never runs"; "the `0xE4` section is not settled" | R12: three callbacks request it (`0x37640`/`0x37774`/`0x37898`); t9-r5 applies `r0=0x33` on 641 S rows and returns at the shared `+0x42`/`+0x43` gate | record §F7.3, `AGENTS.md` music-truth bullet, `docs/PROGRESS.md` final-truth Task 7 |
| "15 missed frames" for the U14 run | 15 of 5,820 **snapshots** missed by the capture model (the port-script miss is one S record at `f=0xB1A`) | design §3.1, `AGENTS.md` U14 block, record §F3.5 |
| "unladen" / "committed poll.logs" (Task 2 cosmetics) | "not late" / "published poll.logs" | `tools/gp_band.py`, `tools/tests/test_gp_band.py`, task-2 report |
| The earlier "10/4 audio banks" wording | The measured 12/10/2/2 (record §F7.2) | `docs/superpowers/plans/2026-10-30-final-truth.md` Task 7 item 2; `docs/superpowers/plans/2026-10-30-final-truth-derivations.md` §F7.2 |

## 5. Maintenance

- Add a row when a task measures a new gap. Name the address, the file:line or
  the capture; if it is `.failed`, say "game evidence only".
- Do not change a terminal/declared row to "closed" without the fix's
  differential row or oracle and a pin/re-pin recorded in the phase record.
- Run `python3 -m unittest tools.tests.test_final_claims` after any edit; it
  fails on a missing row, a changed expected status, an unsupported terminal
  row, an `AGENTS.md` gap statement with no row, and a status that disagrees
  with its row outside a historical/superseded clause.
- The final gate (Task 9) re-measured the counters in §3; only a measured
  improvement may change them, and it must carry its reason here.
