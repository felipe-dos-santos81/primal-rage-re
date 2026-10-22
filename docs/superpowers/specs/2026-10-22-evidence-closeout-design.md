# Evidence Closeout and Truth Pass — Design

Status: approved (builder-checked 2026-10-22; its five questions are settled in
the design: state-proof evidence checks, measured miss sets, per-scenario
allowances, the already-enforced U13 frame count, and the citation sweep)
Date: 2026-10-22
Branch: from `main` at `2304b677`

## Intent and why

Gameplay Truth II (GT2) was designed around five DOSBox-X captures that were
never taken: `gp-u12-endurance`, `gp-u13-ending2`, `gp-u14-win-live`,
`gp-u15-join`, `gp-u16-twop2`. The scenarios, the Makefile oracle blocks and
the tools already exist; every `GP_U12_*` … `GP_U16_*` pin is empty, so each
oracle skips and the phase is formally open (GT2 design acceptance #2, the
ledger ruling "the captures remain the phase's gate"). Those five captures are
the only open evidence work left: the port is 759/759 portable, and the
remaining unported functions are host-libc/runtime substitutes.

Separately, the port's own documents have drifted from the code in ways that
cost a reader real work. `port/spec/game_flow.md` still says the storage,
config and run-clock layers are unported when track S ported them and
`save-reload-oracle` pins them. `port/src/platform/sprite.h` still describes
the byte-wise carry that the U10 span-writer fix corrected — the one stale
comment that points the wrong way about fixed arithmetic, so a future
re-derivation could regress it. `make deps` and the README omit fixtures and
tools the gate needs, and AGENTS' command list omits the five new oracle
targets.

This phase closes the evidence gap and makes the durable docs truthful. It
does not port anything, audit the differential counter, or do host/product
work.

## Goals

1. **Take all five captures** in `data/k11-captures/`, with a clean
   `check-input`, a replay that reaches each scenario's script end, and a
   measured pin set for each oracle.
2. **Add the missing evidence checks** that would otherwise let a capture
   "pass" without proving it reached the state the claim names (see below).
3. **Correct the stale durable docs and comments** named below, with no
   change to behavior.
4. **Record and close**: extend the GT2 derivation record with the capture
   results (§J4–§J8 or the record's own numbering), mark the GT2
   plan/design status truthfully, and run the full `make verify` gate green
   with every existing pin unchanged.

## Non-goals

- No new ported functions, no differential-row audit, no setjmp/longjmp or
  renderer rows, no `gp-u17-device2` tick-keyed replay driver.
- No packaging, install, CI, release build, LICENSE or cross-platform work.
- No audio-fidelity work (sustain, FOR/NEXT, sample format) and no new audio
  oracle; the movie-audio zero-payload finding stands.
- No re-litigation of the documented comparison-model limits (70.09 Hz vs
  60.05 Hz splices, port-derived window starts, loader stall frames).
- No new capture scenario beyond the five. The optional Alt-J/boot probe and
  a fresh demo-fight capture are not taken.
- No change to any existing oracle line, pin, capture, or the differential
  counter.

## Constraints

- **Session-gated.** Each capture needs a DOSBox-X session. Nobody may type
  in the window while `make gp-capture` runs; the harness injects every key.
  `check-input` must be clean; `data/` is read-only (captures are the one
  write, into `data/k11-captures/`).
- **Evidence discipline.** Every pin is measured from the capture with
  provenance, and must fail at pin + 1. No fitted constants. A press or
  gate the capture cannot pin is a named gap, never a plausible number.
- **The gate is the regression line.** `make verify` must stay green, every
  existing oracle line and pin must not move, and `port_progress` must stay
  `799 1203 66` / `759 759 100`. `diff-verify` stays `1019/1019`,
  `14956/14956`, 0 named gaps, `774/833` (186 have none), 8 unrowed.
- **Capture claims are narrow.** A green oracle proves only that the capture's
  content-bearing frames inside the exhibited window are explained and that
  the scripted end was reached; it does not claim port frame order, interior
  coverage, or anything past the capture.
- **One failure does not stop the rest.** The five are independent; a capture
  that does not reach its end leaves that scenario open with a named gap and
  the others proceed.

## The five captures

Order is low-risk first so a bad session still lands the cheap claims; the
unmeasured-press scenarios run last.

| # | Scenario | Time limit | Claim unlocked | Main risk |
|---|---|---|---|---|
| 1 | `gp-u15-join` | 75 s | mid-round F2 join (`0x28CC8`): mode 6→0x17, joining side's start mask, `b1f 1→3`, no credit debit, `gp_twop.py check --join` | the join must land inside its ~0x78-frame window |
| 2 | `gp-u12-endurance` | 70 s | ENDURANCE team pass + fight (modes 0x30/0x31), the only ownerless named gap (§U8.9) | none measured; currently has no evidence check |
| 3 | `gp-u13-ending2` | 240 s | BLIZZARD (character 1) ending: a second endings-table entry (§W.6) and a second death stream; poked final, `memsize=64` | poked-final OOM/`memsize=64` is a harness value; replay must reach mode 0x1F |
| 4 | `gp-u16-twop2` | 240 s | two-human claim past its script end + first finisher evidence (reactions 0x32/0x33/0x34, starters `0x37640`/`0x37774`/`0x37898`), match result and conquered-lands screen | finisher presses and the `0x3766E`/`0x37678` gate are unmeasured; may need a moves pin |
| 5 | `gp-u14-win-live` | 200 s | a fought KO and the game's own death machinery (`0x37EA0`→`0x37FF6`, `b0c`) without pokes, closing "what a poke replaced is not claimed" for one match | unmeasured alternating 1A/1B presses; win may need a measured `SCAN3_ALLOWED` entry at the mode-8 entry or a narrower claim |

Per capture: run it, `check-input`, replay, measure the frame/trace/moves/
reach pins the oracle block expects, add the evidence check if missing, pin
the Makefile block with provenance, run the oracle, record the result.

Every replay also needs its `PR_GP_DUMP` `fn_resolve` miss set measured and
pinned in `test_platform.c`, as every earlier gp scenario did; the driver
fails on an unexpected miss before any oracle pin can be measured. Pinning a
miss with its evidence is not porting. If a replay genuinely needs an
unported target to run, that scenario is a named gap.

Comparison-model artifacts (`SCAN3_ALLOWED`, `OVERLAY_ALLOWED`) are measured
per scenario, byte-checked and mutation-tested; a U10 entry cannot be copied
to U13 because the frames and bytes differ. If an artifact cannot be pinned,
the claim narrows to before it and says so.

### Evidence checks to add or fix

- `gp-u12`: add an evidence check (in the `gp_modes.py` style) that the
  capture actually reached modes 0x30 and 0x31, so the frame/trace claim is
  not asserted by the replay alone. Pin the pick block / exact last step so
  the scenario test cannot pass on the u8 prefix + D3 cut.
- `gp-u16`: add the `--moves-min-first` pin (report-only today) and enforce
  it, but a port-vs-capture ratchet is not a state proof: add a capture-side
  check that the run reached mode 9 and the end state and that an S record
  carries a finisher reaction byte (`r0`/`r1` in 0x32..0x34). Extend
  `gp_win.py`/`gp_twop.py`/`gp_modes.py` as needed and test the new check
  against a negative fixture.
- `gp-u13`: assert the BLIZZARD label (a swapped label must fail). Check the
  capture-frame count: `gp_compare` already receives
  `--capture-frames $(GP_U13_CAPTURE_FRAMES)` (Makefile:1257), the same as
  U10; if the only gap is `gp_win.py`'s lack of a frames pin, add it there
  consistently for U9/U10/U13 or drop the item.
- `gp-u14`: the record already cites `0x37FF6`; the stale `0x37FF2`
  citations are in `tools/gp_win.py`, `tools/gp_session.py`, the GT2 design
  and plan, and `port/src/game/fighter.c`'s comment — sweep those. The
  zero-writer list omits `0x2703D`; the negative test clears `b0c` at a frame
  no raw writer does; the second anchor press's hold is unpinned. Close
  these in the same task as the capture.
- `gp-u15`: verify `gp_twop.py --join`'s no-before path is closed (the guard
  appears at `tools/gp_twop.py:121`); add a test if it is not.

## Truth pass (docs and comments)

Load-bearing, must fix:

- `port/spec/game_flow.md`: the storage/config/clock passages that still say
  unported or "deferred"/"no-op" (`:663-666`, `:794-796`, `:889-892`,
  `:912-913`, `:957-958`, `:969-970`, `:984-986`), and the one remaining
  host-binding sentence at `:935-936` (the pipeline paragraph above it was
  already fixed). Every claim must match `port/src/game/config.c` and the
  `save-reload-oracle`.
- `port/src/platform/sprite.h:104-106`: the "byte-wise" comment must describe
  the dword carry the code implements (`sprite.c:116-124`), so no one
  re-derives the old bug.
- `AGENTS.md`: add `gp-u12-oracle` … `gp-u16-oracle` to the command list; fix
  the fixture list (keep `ghidra_data.bin`; drop or explain the unused
  `title_screen_ref.ppm`).
- `Makefile` `deps` / `README.md` "Verify": name the git-ignored
  `port/tests/ghidra_data.bin`, the `unicorn`/`capstone` requirement and the
  all-zero `data/game/C/CMOS` precondition, so a fresh clone does not fail
  the gate mid-suite.
- GT2 status: the design still says "Status: DRAFT", the plan has no status,
  and the derivation record has no capture/closeout sections. Make all three
  say the same true thing: the five captures are taken and pinned, or name
  exactly which remain open.

Cheap, same pass:

- `port/RE_GUIDE.md:232-270`: the "Next steps" list still says the fight
  engine, attract, effect renderer, menus/EEPROM and streamed audio remain.
  Replace with the real remaining list (host/product, the narrow limits).
- Host-input record nits: `derivations.md:1524` "26 the six keyboard and two
  F readers" (reads as 8), the §H2.5 citation for the 26 split (should be
  §H2.3/§H2.6), and `design.md:71`'s "game_loop writes both bytes".
- `port/src/host.h:129-131`'s "through `host_kb_bit`" comment, if it still
  contradicts the fill.

Left alone deliberately:

- The three different "no key-config block" answers in `host.c`: production
  always builds the block before `game_loop`; consolidating them is behavior
  work for a host phase, not a doc fix.
- `gp-u17-device2`'s empty ratchets: a tick-keyed replay driver is its own
  phase.
- The six known-wrong §W1 numbers in the GT2 record beyond those this phase
  touches: correcting them is a separate record audit, unless a fix here
  would otherwise rest on a wrong number.

## Acceptance

1. All five captures exist under `data/k11-captures/`, each with
   `check-input` clean and a replay that reaches its script end (or a named
   gap with its evidence).
2. Each `gp-u12-oracle` … `gp-u16-oracle` is green with measured pins, and
   every pin fails when moved by one. The five are in `make verify`.
3. The new evidence checks exist and fail when their claim is false; the
   assertion-level ones are mutation-proven.
4. The truth-pass items are corrected; `git grep` finds no remaining
   "unported"/"deferred"/"no-op" claim contradicted by the tree in the named
   passages.
5. `make verify` is green on the final tree; every existing pin and the
   differential counter are unchanged; `port_progress` is unchanged.
6. The GT2 record, plan and design agree on the phase status.

## What is left to the builder

The task breakdown, exact capture commands and environment (`DISPLAY=:1`),
the per-oracle pin procedure, the Makefile provenance comments, the record
section numbering and the commit granularity. The builder also decides
whether a failed high-risk capture is retried once with a narrower script or
left as a named gap — the design forbids fitted pins either way.
