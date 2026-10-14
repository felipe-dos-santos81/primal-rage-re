# Track G design — widen the proven picture (2026-10-14)

Status: DRAFT for approval. Evidence base: the four recon reports in
`.superpowers/planning/2026-10-14-next-phase/` (01-differential-completeness,
02-port-coverage, 03-gameplay-frontier, 04-host-readiness), each citation
carried through from there.

## Intent and the why

The port has reached an odd state: everything *portable* is ported (747/747
portable functions, 1000/1000 differential rows VERIFIED), but the *proven*
picture is narrower than the code. Screens behind the frame ratchets (round 2
of a win, the ending sequence, the pause prompts, ENDURANCE's fight, the
high-score entry) are trace/milestone-verified or not verified at all, the
weaker-row list is frozen by harness limits rather than by unknown bytes, and
rendered audio has zero evidence. The user chose (this session's ruling) to
make the next phase **widen what is proven** rather than chase host/UX polish
first — because the forensic work compounds (every unlocked ratchet is
permanent regression cover) and because the biggest player-track item (audio)
is blocked on hardware this host lacks.

Success looks like: three stalled ratchets un-stalled, two new pixel claims
that did not exist, ~80 open differential rows closed, one game mode fully
proven, and the save-game story backed by a recording of the original doing
it — with the byte-exact oracles and every existing pin intact.

## Goals

1. Close the harness-limited differential rows that are pure engineering, not
   RE: the six named non-rows' callee rows (closes 77+ open rows — measured,
   01 §3), the `0x62003` exit model (unblocks both save rows' exit blocks and
   turns gap `0x2EA68` into a row), `game_loop`'s two RNG pokes → recorded
   stubs against the VERIFIED `0x5D7DC` row.
2. Un-stall the mode-8 three-frame scan-out: isolate the cause with a
   derivation of its own, extend the splice/allowance model honestly, and
   raise idle-loss (N 2064), U9 (346) and U10 (331) frame ratchets past round
   1's mode-8 entry — as far as the model can honestly carry them.
3. First-ever pixel claims for the pause/prompt screens via a gp-keys-fight
   frame ratchet (no new capture; tool + re-pin only).
4. Diagnose and ratchet the gp-moves N=2139 pixel gap — the only frame
   ratchet below its replay end with unexplained pixels (Makefile:534; 02 §6).
5. ENDURANCE: capture past the D3 team-select cut and extend
   `gp-u8-endurance`'s ratchets — closes the only ownerless named gap (03 §5).
6. Save game: DOSBox-X CMOS save→reload capture closing track S's main named
   gap §S.8 F7 (the design exists: storage plan Task 8), then re-run the
   storage rows against it if it disagrees with the modelled path.
7. Host input completion: read the configured binding words (rebind is
   cosmetic today) and a gamepad map validated against a game-port ground-truth
   capture (04 §5).
8. Doc truth: the stale AGENTS.md sentence "the replay still misses
   `0x29C78`" (contradicted by its own §W.16 line and actors.c:1082) — fold
   into the first commit on main after the pending gate.

## Non-goals

- Porting anything in the runtime region (≥0x5D000) or the 12 deferred
  Smacker-movie-audio lines (verified paper categories: 02 §1–3).
- Audio bring-up on this host — SDL audio cannot open here (-66681); the item
  is **queued for a device-capable host**, not dropped.
- CI/packaging for other OSes (recorded as a v0-release unknown, not this phase).
- Rewriting any oracle's narrow claim into a broad one. Every new claim states
  what it cannot see, in the existing house style.

## Anti-goals (failure even if green)

- Ratchet numbers raised by loosening the comparison instead of carrying it
  forward (the mode-8 work must add named allowances with evidence, never a
  general "N-model").
- The §C3g.7 "vacuous rows" reversal producing rows whose VERIFIED word means
  more than the claim supports.
- A capture session with stray input that quietly contaminates a pinned
  `poll.log` (the check-input rules must be re-read before each).

## Constraints

- The pending `PR_DIFF_FULLSCAN=1 make verify` gate for pushed track F
  (`e7ce4f1f`) must report green before any Track G dispatch; if red, the
  incident (triage harness-vs-port, fix-forward or revert) is the first work
  item and this design updates honestly.
- The byte-exact oracle lines and every existing ratchet pin may only move
  upward with a recorded reason; the counter sentence (1000/1000, 14885/14885,
  named-gap and closed-row counts) is re-measured and re-pinned per batch with
  provenance, per repo convention.
- Captures are git-ignored, `data/` is read-only; each capture pins its
  `poll.log` sha256; nobody may type during `gp-capture`/`k11-capture` runs.
- Evidence discipline unchanged and binding: derived values only, named gaps
  with the blocking instruction, raw-wins on plan-vs-raw conflict.
- Multi-task work runs under subagent-driven development; four-agent pattern
  (parallel workers in their own worktrees + read-only audit before merge),
  which the user directed for this phase.

## The parts decided in the conversation

- **The fork:** forensic track as the spine, player-track items as the side
  batch, audio queued for a device host. (User: delegated with "ok"/"yes";
  recorded as the controller's call, ratified by the user's yes.)
- **Order:** wave 1 = items 1–4 + 8 (no new captures needed, pure repo work);
  wave 2 = items 5–7 (each needs a live DOSBox-X session on the user's
  machine). Doc-truth item 8 rides the post-gate cleanup commit.
- **The §C3g.7 reversal is proposed here, decided by the user** — see
  "Needs your call".

## Needs your call (answered before the plan is final)

1. Reversing §C3g.7's "vacuous rows" judgment for the six non-rows (their
   callees get rows with weaker, honestly-worded claims) — a prior recorded
   judgment the user's earlier tracks ratified; needs explicit yes/no. This
   also sets the phase's completeness metric: **closed rows** (the counter's
   current definition) or **closed-on-full rows** (01 §5 argues any
   completeness plan must pick). Item 1 and the "~80 rows" success line stand
   or shrink to ~5 rows on this answer.
2. Commitment level of goals 2–4 when diagnosis finds a *port-side* divergence
   instead of a model limit: default **name + escalate** (ratchets land where
   they honestly land; a port fix that moves every frame index is a named
   follow-up, not silent in-phase rework), or fix-in-phase at days-to-weeks
   risk? (Builder-check Q3; goal 3's wording currently promises more than
   tooling alone can guarantee either way.)
3. Gamepad (goal 7's second half): there is no deterministic game-port capture
   primitive today (01/04; injection is keyboard/pad-bits/pokes). Choose
   (a) **additive host feature** — SDL-pad→pad-word, observational session,
   not capture-pinned, or (b) build the capture capability for analog/game-port
   state as part of this phase (a DOSBox I/O-emulation project), or
   (c) **defer** gamepad; keep only the configured-binding-words fix in-phase.

## Settled by the builder check (recorded as calls, yours to overturn)

- **ENDURANCE capture = new scenario name, not a re-capture** of
  `gp-u8-endurance` — re-capturing invalidates its pinned `poll.log` sha256,
  frame count and N/F/MAX_START pins (03 §3's own warning). The new scenario
  extends the fight past the D3 cut alongside the old one; old evidence stays
  enforced, and the new ratchet is ordered *after* goal 2 (it would otherwise
  stop at the same mode-8 entry).
- The stale AGENTS.md U10 sentence (goal 8) was fixed and pushed early, at
  gate-close: `8b389786`.

## Capture go-ahead (scheduling, not scope)

Goals 5–7 (wave 2) each need a quiet DOSBox-X session on your machine —
nothing may be typed while `gp-capture` runs. Say when that's convenient; the
plan will order the session queue around it.

## What's left to the builder

- Agent split and sizes within each wave (the recon costs are given in
  agent-hours: 12–18 for item 1's rows, 10–16 for the exit model, 3–5 for the
  RNG stubs, days for the mode-8 isolation, S–M for the rest).
- The exact comparison-model change for mode-8 (named-allowed splices vs a
  three-frame window model) — must be derived from the captures' bytes first.
- Wording of each new row's/case's claim, the record sections, and the
  re-pin math.
- Whether items 3 and 4 merge into one capture-comparison batch or two.
