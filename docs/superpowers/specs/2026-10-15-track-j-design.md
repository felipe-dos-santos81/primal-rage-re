# Track J design — close the ledger, run the captures (2026-10-15)

Status: DRAFT — written from the four 2026-10-14 recon reports plus a 2026-10-15
re-measurement; builder-checked 2026-10-15 (its three questions folded below as
calls). The open calls are the controller's, overturnable.

Evidence base: `.superpowers/planning/2026-10-14-next-phase/`
(01-differential-completeness, 02-port-coverage, 03-gameplay-frontier,
04-host-readiness), each citation carried through; the track-G records
(`2026-10-14-reverse-g-derivations.md` §G3–§G7, `2026-10-14-reverse-g-widening.md`
Task 8); `AGENTS.md`'s counter and census sentences; the SDD ledger
`.superpowers/sdd/2026-10-14-reverse-g-widening/progress.md`.

## Intent and the why

Track G widened what is *proven*: the exit model, six former non-rows, the
first pixel claim on the pause/ABANDON/QUIT screens, and the mode-8 scan-out
model that moved three ratchets (idle-loss 2064→7559, U9 346→2209, U10
331→2062). It stopped with two wave-1 tasks unlanded — `game_loop`'s RNG stubs
(§G4) and the gp-moves 2139 diagnosis plus the two U9/U10 166-px strips (§G7) —
and its whole wave 2 (the capture sessions: ENDURANCE, save-reload, host input)
untouched. Meanwhile the differential ledger stands at `1007/1007; 14899/14899;
716/832 closed (175 none)` with one policy decision open (the closure rule) and
two grunt batches (handler-table rows, renderer rows) that are engineering, not
novel RE.

The next phase finishes what G opened and takes the two evidence-free axes that
do not need new hardware: **a save the original itself wrote**, and **the fights
past the first round** (ENDURANCE, a second ending, a real won match). It lands
the two small promises still outstanding (doc truth, rebind), decides the
differential counter honestly instead of by inflation, and keeps audio queued
for a device-capable host as already ruled.

Success looks like: G closed with the counter and pins re-measured; the capture
wave landed as new pinned scenarios; the doc set truthful; a stated policy for
the open differential rows; no oracle line or existing pin moved without a
recorded reason.

## Goals

1. **Finish track G's wave 1.** Land §G4 (`game_loop`'s two `mov eax,0` pokes at
   `0x256B1`/`0x256D6` → recorded stubs on the VERIFIED `0x5D7DC` row, per-case
   EAX from the consumed values, a mutant for a wrong RNG value, `0x25680`
   staying named unhit per §H.3.4; ~3–5 agent-h) and §G7 (classify the gp-moves
   N=2139 pixels and the two 166-px six-row strips — rows 192..197, x 0..85,
   sha `a886c0f3..`, identical at U9 2209 and U10 2062 — model artifact,
   named allowance, or real divergence; fix from bytes if real, escalate if the
   tick/catch-up model itself is wrong, re-pin only with provenance). Then the
   T8 audit: fulfil or formally close the deferred-minor list (m-a..m-c, m-e,
   m-h; g6-m2..m6; n-a..n-c), close the ledger, and run the final
   `PR_DIFF_FULLSCAN=1 make verify` before merge + push.
2. **Doc truth.** README.md:44-48 (the "1000/1000 … 14885/14885" sentence, the
   "732 of 747 … 9 … 15" rows sentence, the "174 corrections" figure → verify
   against §D.12's 172), Makefile:901's superseded "replay still misses
   `0x29C78`" line, and PROGRESS.md's missing track-G paragraph plus its
   "Current state (2026-10-12)" header refresh (PROGRESS.md:11).
3. **The closure decision, landed.** Adopt the closure-rule amendment
   (recon-01 §4/§6 item 4): a row may count closed when its only open callees
   are outside the portable set — runtime ≥0x5D000, host-owned per the
   classification table, or E2-class non-functions — with each declared limit
   named and the record stating what each row still cannot see. Re-pin the
   counter with the new sentence and report "closed on full rows" (today: 12
   weaker of 739 rowed) as the secondary number. ~6 agent-h. Alternative, not
   recommended: extend F's runtime model to the `0x61A70`/`0x655FF`/`0x65624`
   libc class, 24–40 agent-h, risk that the model becomes the spec for those
   callees.
4. **The capture wave (wave 2; quiet DOSBox-X sessions on your machine; new
   scenario names only — never re-capture an existing one).** Queue, to be
   ordered by evidence-per-hour in the plan: (a) `save-reload` — the first
   recording of the original writing and loading a real CMOS; closes §S.8 F7;
   the Task-8 design already exists (`2026-10-13-reverse-s-storage-layer.md`);
   (b) ENDURANCE past the D3 team-select cut — the only ownerless named gap
   (§U8.9) and the last unproven game mode; (c) `gp-u10-ending` for char≠0 —
   the ending-table entries §W.6 leaves open; (d) a real won match without
   pokes — kills §W.9's "what a poke replaced is not claimed" for one match;
   (e) mid-match join (`0x28CC8`); (f) extended `gp-twop` + finisher; (g) the
   game-port joystick capture that settles faithful-vs-host gamepad — scoped as
   a probe: a human-assisted DOSBox-X session with a mapped joystick
   (report-only evidence, not a replay fixture); if it cannot produce
   interpretable evidence, the omission is formalized as a named gap
   (recon-01 item 8's decline), not a new tooling project. Items (a)–(d) are
   the tops of recon-03's and recon-04's rankings. If G7 explains
   the strips, the U9/U10/idle-loss re-pins extend the win/ending pixel claim
   past f=0x48A.
5. **Conditional grunt batch (your scope call; off by default).** Differential
   rows for the 13 ported handler-table entries (`0x24568` family; closes ~18
   rows; 50–80 agent-h in 2–3 batches); the setjmp/longjmp model (`0x653FC`/
   `0x65431`) for the restart arms (8–14 agent-h); the renderer rows for
   `0x51ED8`'s allow callees (12–16 agent-h). All engineering; worth it only as
   behaviour coverage, not as metric.
6. **Host small batch.** Rebind: `host_key_bits` reads the configured scans
   with a defaults fallback (host.c:57-61; keep `host_set_key_bits_override` so
   no pinned pad word moves). Exercise the SDL failure branches where this host
   permits. Live-play notes (idle-reboot, credits, quit). Optional: one Linux
   container build (`cmake -S port -B build`, `make check frames=60`).

## Non-goals

- Audio bring-up — the `-66681` open failure is stale on this host (2026-10-15:
  `host_audio_open(49716,2) -> 1`, coreaudio; record `2026-10-15-reverse-j-derivations.md`
  §J2.5), so a device-capable host is available and the item stays queued
  (G's ruling stands).
- The SDL-gamepad→pad-word map (returns when the joystick capture exists).
- Porting anything in the runtime region or the deferred Smacker-movie-audio
  addresses (verified paper categories, 02 §1–3).
- Rewriting any oracle's narrow claim into a broad one; general "N-models".

## Anti-goals (failure even if green)

- A closure-rule change that inflates the counter without pinning what remains
  open and what each row still cannot see.
- Ratchet numbers raised by loosening the comparison instead of carrying
  evidence forward.
- A capture session with stray input contaminating a pinned `poll.log`; a
  re-capture that invalidates existing pins.
- G4/G7 touching a pinned row without re-measuring every affected pin.

## Constraints

- Byte-exact oracle lines and every existing pin move upward only with a
  recorded reason; the counter sentence is re-measured and re-pinned per batch.
- Captures are git-ignored; `data/` is read-only; one user of the shared
  `/tmp/pr_gp_dump` at a time (§G7's hold reason).
- SDD flow: parallel workers in their own worktrees + read-only audit before
  merge; fullscan gate before push; SSH push workaround.
- Evidence discipline unchanged: derived values only, raw wins on
  plan-vs-raw conflict, named gaps carry the blocking instruction.
- Byte checks for rows/cases go through Ghidra MCP (project `primal-rage`);
  captures through DOSBox-X.

## Calls (the controller's; overturn any of these)

- **Closure = the amendment** (goal 3), not the runtime model; the counter
  sentence pins the declared non-goals; closed-on-full stays the secondary
  number.
- **Wave 2 = the capture queue as listed**, new scenarios only; ENDURANCE = a
  new scenario name; save-reload first if only one session is possible.
- **Goal 5 = conditional, off by default** until you say the behaviour coverage
  is worth 70–110 agent-h.
- **Audio stays queued; gamepad stays deferred; rebind is in-phase.**
- **Commitment level:** the standing rule — a MISMATCH is a finding fixed from
  the bytes in-phase, one correction per commit; a tick-model finding in G7
  escalates and stops, with ratchets pinned where the evidence honestly lands.
- **Capture mechanics:** the capture lane starts on your word; until then the
  plan orders it so no other goal waits on it, and if no session materializes
  goal 4 is a recorded queue, not a phase failure. Harness captures are
  unattended (you only stay off the machine); the joystick probe may need you
  present.
- **Live-play notes = notes only** (a short session if convenient, else
  assembled from the records): a plain key-map/behaviour paragraph. No new
  features beyond rebind; "credits" is documented as today's behaviour (config
  image; no coin key), not implemented.

## Capture go-ahead (scheduling, not scope)

Goal 4's sessions need quiet DOSBox-X runs on your machine; nothing may be
typed while a harness capture runs. Say when that's convenient and the plan
orders the queue around it; the save-reload capture additionally carries the
storage plan's Task-8 checkpoint (show the design, ask go-ahead). Nothing in
goals 1–3 and 6 requires a session.

## What's left to the builder

- Lane/agent splits and ordering within each wave; the capture queue's
  dependency graph (which pin moves after which capture).
- The exact G7 classification method and the re-pin math for U9/U10/idle-loss.
- The closure amendment's exact wording and the counter sentence's new text.
- Which of goal 5's items get budget; which of goal 4's captures fund a session.
- The rebind change's wiring against the config image; the SDL failure-branch
  experiments this host can actually run.
- Lower-priority frontier items to pick up only if budget remains: capture
  835's cause note, U6b's 4-of-12 unshown moves, U8 rows' fights past their
  cuts, the under-render blind spots (03 §6-§8).
