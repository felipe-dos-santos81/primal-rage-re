# Track G derivations record — widen the proven picture (2026-10-14)

Plan: `docs/superpowers/plans/2026-10-14-reverse-g-widening.md`.
Spec: `docs/superpowers/specs/2026-10-14-track-g-design.md`.
Sections are per batch (§G1..§G7); the bare §G.n prefix stays reserved for the
gameplay-ground-truth record. Lane A workers own §G1-§G4; lane B owns §G5-§G7;
each appends only its own section, byte-evidence inline.

## §G1 — exit model (pending, lane A task 1)

## §G2 — save-row exit blocks + rowing 0x2EA68 (pending, lane A task 2)

## §G3 — the six non-rows (pending, lane A task 3)

## §G4 — game_loop's RNG pokes (pending, lane A task 4)

## §G5 — gp-keys-fight frame ratchet (lane B task 5)

**Measurement before any change (Task 5 Step 1).** Base `5f874678`, worktree
`reverse-g2`, `make build`; `make gp-report scenario=gp-keys-fight` (the `PR_GP_DUMP`
replay + `tools/gp_compare.py --report`) over `data/k11-captures/gp-keys-fight`
(`poll.log` sha256 `8425afbc46d51173532f6f4c27a8f16c594e2bc572b4273e056bdbf51a9678ae`,
re-verified on disk, 1166 frames — the same capture U11 pinned; the
`GP_KEYS_CAPTURE_SHA256` pin is NOT re-pinned)
and its dump at `/tmp/pr_gp_dump/gp-keys-fight` (789 `.ipx` frames, `frames.txt` 00592 =
`f=0x832`, 00593 = `f=0x833`, last 00788 = `f=0x872`; the replay's script ends at the
capture's X record `f=0873 step=19 end`, poll.log line 2223). The report lines equal
§K.12's Step-6 lines byte-for-byte on the shared fields — nothing moved between the U11
measurement and this base:

```
gp_compare: gp-keys-fight: frames: window from capture 100 (raw 1747); 1028 classified: 660 clean, 361 splice, 2 transition, 5 unexplained, 12 all-black
gp_compare: gp-keys-fight: frames: FIRST UNEXPLAINED capture 835 (raw 3934): nearest port 593, rows 0..91, x 0..319 (17138 px)
gp_compare: gp-keys-fight: trace: 1841 frames compared (f 13A..), 8 without a capture snapshot; first tick difference f=13B (reported, not ratcheted)
gp_compare: gp-keys-fight: trace: 0 differing through 2163
```

A full pass (the report caps at `REPORT_MAX` 5 unexplained) gives the complete unexplained
set of the capture: **[835] + [1136..1165]**. 1136..1165 are the capture's 30-frame tail
after the replay's end — the restart's boot movies (§K.12 "Captures 1136–1139 …"), which
the port's script never ran, like gp-twop's 612 and gp-charsel's 516: how far the port
got, not a divergence. Inside the replay's range the ONLY unexplained frame is 835.

**The pause/ABANDON/QUIT screens without any allowance.** An open pause freezes `f`
(§K.10 item 3), so its screen simply repeats in the capture and repeats of a dumped port
frame classify clean. Measured (a per-frame classification pass over the dump, no
allowance): capture 780..834 (the pause `c=0x809`, alt-s, and the ABANDON-prompt path
`c=0x81E`, ending just before the altq-n event at `c=0x833`): 25 clean, 30 splice, 0
unexplained; capture 836..899 (past the QUIT answer): 25 clean, 38 splice, 1 black, 0
unexplained; capture 1090..1135 (up to the replay's end): 35 clean, 9 splice, 2 black, 0
unexplained. So the pause and ABANDON screens need NO allowance, and everything after the
835 splice (the QUIT band's own rows aside) needs none either — 835 alone blocks a ratchet
past it.

**The 835 byte anatomy (re-measured from this dump; §K.12's finding holds exactly).**
Capture 835 (raw 3934): rows 0..80 (bytes 0..77759) are byte-equal to port 592
(`f=0x832`) and capture 834 equals port 592 whole; rows 92..199 (bytes 88320..191999) are
byte-equal to port 593 (`f=0x833`) and capture 836's rows 0..91 equal port 593. The band
rows 81..91 (bytes 77760..88319, sha256
`d9c0ac62f7e0c0d4082fba8d195eae4f767c56a7887bdf7346450511f998cad8`): of its 10 560 bytes
1918 equal BOTH bound frames at the same offset, 5714 equal only 592's, 486 equal only
593's, and **2442 equal NEITHER** (per-row neither-x-ranges: x 56..187 in rows 81..88,
x 0..189 in rows 89..91 — around and in Sauron's sprite, no prompt text). No splice point
s makes the band `592[:s] ++ 593[s:]`; no port frame in the whole 789-frame dump holds
those 11 rows. The band is therefore **not port-derivable** — it is the third, never-dumped
present state (§K.10 item 2: `0x2EA78(-1)` presents the prompt's one frame with text and
the dump holds one frame per `f`; §K.12's other candidate, a sprite drawn during scan-out,
stays open — the cause is not isolated, and this task does not decide it). Exactly ONE
adjacent pair in the dump matches the top+bottom binding: (592, 593).

**Decision (Step 2): explain 835 with a named per-frame allowance.** Leaving the ratchet
below it (N = 835) would claim the pause and ABANDON frames but stop at the QUIT event;
the allowance (band pinned byte-exactly, top/bottom byte-bound to the port's CURRENT dump,
unique-pair required — see `tools/gp_compare.py` `BANDS_ALLOWED`) explains exactly this
one frame at exactly these bytes and lifts the claim to the script end. The band pin is
seconded by the capture-identity pins (`--capture-sha256`/`--capture-frames` reject any
other capture before comparison starts).

**The pins (Step 3).** `GP_KEYS_MIN_FIRST = 1136` (measured: with the allowance active the
enforced pass reports `first unexplained 1136, ratchet N 1136 ok` — 1136 is the end of the
port's script, per the precedent above, not a divergence; raise only when the script is
lengthened). `GP_KEYS_MAX_START = 100` (window start, raw 1747; the slide guard).
`GP_KEYS_TRACE_MIN_FIRST = 2164` (the trace's compared range ends at the replay's `f =
0x873`, so 0x873 + 1 = 2164 = the exact pin: `0 differing through 2163`).
`GP_KEYS_CAPTURE_FRAMES = 1166` (measured count of `frame_*.raw.gz`, §K.14's 1166).
`GP_KEYS_CAPTURE_SHA256` stays `8425afbc…678ae` — same capture, NOT re-pinned. No moves
pin: the capture records the move fields and the report pass showed 0 differing, but
pinning the moves claim is outside this task's frame-ratchet scope (gp_compare runs it
only with `--moves-min-first`). The limits of the claim are the standard gp_compare ones
(record gameplay-ground-truth §G.16, §K.12): it cannot detect an under-rendering port,
the order of the port's frames and that every port frame appears are not claimed
(coverage is reported: 13 non-black port frames up to 788 unexhibited), frames at/after
the pinned window start are classified from the port's own dump, and the band of 835 is
capture-pinned bytes with no port binding — its content and cause stay the named gap of
§K.10 item 2 / §K.12. §H.4 carve-out: this allowance injects no keys (the replay script
and its key count are unchanged) and reads no screen/data overlap bytes — the §H.4 hang
limit and `[0xA0000,0xAFA00)` overlap do not bind it, named here per the plan's rule.

**Implementation (Step 2/3).** `tools/gp_compare.py`: `BANDS_ALLOWED` (per-scenario named
per-frame allowance list, the `title_compare.ATTRACT2_SPLICE3_ALLOWED` style — never a
class), `band_pair` (the full-dump search for the unique adjacent port pair that byte-binds
the capture frame outside the band) and `band_explain` (band sha256 first, then the pair;
zero or multiple matches reject); `frame_claim` consults the scenario's entries only when
the two-frame model leaves a named frame unexplained, prints `allowed by name as a band
splice` on success, a loud `FAIL: allowed capture <j> is not its named band splice`
otherwise (counted into the enforced rc via `allow_bad`), and a `retire the name` note when
the model explains a named frame again (the 3545 precedent). The classification becomes
kind `band`, exhibiting P and P+1 (the same exhibition rule as a splice), and the summary
line gained the `band-allowed` count. `exhibited`/`main` carry the table keyed by scenario
name; every other gp scenario passes no bands → unchanged, and no other tool parses the
changed summary line (`gp_win.py` uses only `ratchet`/`trace_claim`; k11/title/attract
print their own lines). The Makefile target runs `gp_compare` after the two `gp_keys`
steps with the pins above; `make gp-keys-oracle` green (rc=0): `captured frame 835 (raw
3934) allowed by name as a band splice: port 592/593 rows 0..80/92..199, band rows 81..91
the named bytes`; `1025 classified: 660 clean, 361 splice, 2 transition, 1 band-allowed,
1 unexplained, 12 all-black`; `first unexplained 1136, ratchet N 1136 ok`; `trace: 0
differing through 2163; ratchet N 2164 ok`; `poll.log sha256 8425afbc..9678ae, 1166
frames: matches the pin`.

**Review Focus #1 / Step 4 proofs.** Unit (`tools/tests/test_gp_compare.py BandAllowance`,
6 cases, in the `make verify` gp tool-test step): the named bytes explain; a mutated
BOTTOM-bind port frame un-explains (the allowance FAILs and the ratchet catches it);
different band bytes over the same top/bottom are not the named frame; two matching pairs
(ambiguous) are not the named frame; a model-explained named frame prints the stale-name
note; and the table's `gp-keys-fight` entry is pinned, so a silent name change is a test
failure. Disabling the mechanism (bands forced empty) fails 5 of the 6 — the tests can
fail. Real data on a COPY of the dump (the original dump and the capture untouched):
poking port 592's row 40 index byte breaks 835's top bind — `band_explain` on capture 835
answers `no unique adjacent port pair binds its rows [0,81)/[92,200)`, and the enforced
run FAILs: rc=1, `FIRST UNEXPLAINED capture 834 (raw 3933): nearest port 592, rows
40..40, x 160..160 (1 px)` with `first unexplained 834 < ratchet N 1136` (834's clean
sample of the mutated frame breaks with it). Poking port 593's row 150 breaks only the
bottom bind: the enforced run stops exactly at 835 with rc=1 and the allowance's own
`FAIL: allowed capture 835 ... no unique adjacent port pair` line. Step 4 on a PLAIN
explained frame (not the allowed one): a single index-byte poke on port 300 was absorbed
by the splice model (the neighbour frame holds the old pixel — the two-frame model's
documented power, not a hole in the allowance), so the recorded proof pokes port 300's DAC
entry for its most-used index (52, 2284 px): every capture frame exhibiting that frame's
exact bytes breaks, the enforced run rc=1 with `FIRST UNEXPLAINED capture 479 (raw 2970)`
(the first frame exhibiting port 300) while `band_explain` still returns P=592 — the
failure is the plain regression, not the band. The allowance silences exactly one capture
frame at exactly its named bytes.

**AGENTS.md.** The keys-paragraph claim sentence moved from "no frame is compared (the
pause and prompt screens are a named gap)" to the narrow frame claim with its named
limits (the 835 band's cause, the script-end N, under-render/order/coverage unclaimed).

## §G6 — the mode-8 three-frame scan-out (lane B task 6)

**Base and measurements.** Base `93b24171` (`reverse-g2`), `make build`; the three
re-measured with fresh `make gp-replay scenario=…` runs on this tree — dumps at
`/tmp/pr_gp_dump/{gp-idle-loss, gp-u9-win, gp-u10-ending}` (6301 / 1977 / 4664
presented frames; every `fn-miss` set on its pinned known-set, `all checks passed`).
The per-frame tick log the brief asks for is the driver's own `frames.txt` tick
column (one line per presented frame: `index f=… tick=… mode=…`) — no extra
instrumentation was needed, and its ticks advance **exactly +1 per frame** across
every stretch (idle-loss 1665..1670: 0xCCF..0xCD3; U9 215..221: 0x4E1..0x4E7; U10
213..221: 0x4DA..0x4E2).

**The composite anatomy, re-measured per row against these dumps** (a row-run pass:
for each stored capture frame, which dump frames equal each row exactly):

- **gp-idle-loss capture 2064 (raw 5190)**: rows 0..2 = port 1665/1666 (f=0xC70/0xC71,
  identical there), rows 3..97 = **port 1669 (f=0xC74) exclusively**, rows 98..199 =
  **port 1670 (f=0xC75) exclusively**. `window.txt` 2062..2065 = raws 5185, 5187,
  5190, 5191: raw 5186 was deduplicated against 5185 and raws 5188/5189 against 5187,
  so the original held **port 1666's screen for three consecutive scan-outs** (~43 ms
  of refresh time) and then put **two new frames' blits inside one scan-out**. Port
  1667/1668 (f=0xC72/0xC73) differ from every neighbour (row-anatomy `next_diff`) and
  match no row of any capture frame in the window: their blits never reached VRAM in
  any form the CRTC could show.
- **gp-u9-win capture 346 (raw 2830)**: rows 0..4 = port 218 (f=0x48A, the first
  mode-8 frame, held by dedup across raws 2827..2829), rows 5..199 = port 221 (f=0x48D)
  exclusively. Ports 219/220 (f=0x48B/0x48C) skipped; capture 347 (raw 2831) shows
  port 222 whole — the next tick's blit landed after this scan-out ended, so this
  composite is two segments.
- **gp-u10-ending capture 331 (raw 2831)**: rows 0..1 = port 216 (f=0x483, the first
  mode-8 frame, held across raws 2828..2830), rows 2..63 = port 218 (f=0x485)
  exclusively, rows 64..199 = port 219 (f=0x486) exclusively. Port 217 (f=0x484)
  skipped; capture 332 (raw 2832) = the 219/220 splice — normal cadence resumes.

**The original-side cadence, from each capture's own `poll.log`** (S records are
logged only while the loop is caught in its spin state `t50c - 1 == t508`,
`gp_capture.py` `spinning`/`accept`; P records mark (mode, st) changes):

- idle-loss: `S f=0C70 ms=74174 tick=0x10F1 t508=0x544` → `S f=0C74 ms=74250
  tick=0x10F5 t508=0x548` → `S f=0C75 ms=74255 tick=0x10F6 t508=0x549`; no S for
  f=0C71..0C73; `P f=0C71 mode=8 ms=74192` (the poller saw f=0C71 **not** in the
  spin state — it could log only the P). Δtick = Δt508 = Δf = 4 across the stretch,
  then 1/1; the f=0C74→0C75 pair was observed 5 wall-ms apart.
- U9: `S f=0489 … tick=0x908 t508=0xCE` → `S f=048D ms=40520 tick=0x90C t508=0xD2`
  (Δ4/Δ4/Δ4 across the stretch), `P f=048A ms=40483 tick=0x909` (tick 1:1 on the
  mode-change frame); no S for 048A..048C.
- U10: `S f=0482 … t508=0xCE` → `P f=0483 ms=40492 tick=0x90C` → `S f=0485 ms=40534
  t508=0xD1` → `S f=0486 ms=40538 t508=0xD2`; no S for 0483/0484.

At every observed endpoint the ISR counters advanced exactly 1 per frame — **no
counter jump anywhere** — and the missing S records are the loop being caught
outside its spin state (or the poller starved by the same host CPU; the 76 ms and
55 ms observation gaps are the poller's wall clock, the blit structure below comes
from dedup'd raw indices, which count guest refreshes, not wall time).

**The loader re-sync hypothesis, tested and rejected** (0x1B45F/0x1B464 — the
mechanism the attract2 3545 name rests on, Makefile:288-295, title_compare.py:510-524).
Three independent grounds on the unpoked gp-idle-loss evidence; U9/U10 share the
shape (same mode-6→8 entry, same anatomy, same cadence):

1. A mid-game INDEX read goes through 0x1B3AC's draw=1 lazy path, which **always
   ends with a blit**: the loader draws string 489 (`- LOADING -`) into the aperture,
   the disk read stalls the loop with the ISR ticking, 0x1B464 re-syncs t50c := t508
   and the gate (0x25643) then passes on the reading frame — res.c's own comment
   names that captured presents ("without it the loader's frame — which the capture
   holds — would never be presented"). Every hold window here is **byte-identical
   across its raws** (5188/5189 dedup'd against 5187; U9 2828/2829 against 2827;
   U10 2829/2830 against 2828): no blit landed during any hold, so no read completed
   inside it.
2. **The mode-8 entry path reads no INDEX resource at all.** The port implements the
   same dispatch and models every draw=1 read as a tick advance (res.c); the port
   replay's tick column advances exactly +1 per frame across each stretch — a read
   anywhere in f=0C71..0C75 (or the U9/U10 analogues) would jump it. 0x1B3AC is not
   on the path.
3. A re-sync changes the *relation* between the counters (t50c pulled up to t508
   mid-frame, an observable t508-Δ vs f-Δ asymmetry). Observed on all three captures:
   Δtick = Δt508 = Δf = 1:1 at every endpoint.

**The mechanism: the master loop's own gate + catch-up (0x25643 / 0x256C5).** The
ported and differentially-verified loop (flow.c:7141-7193): each iteration computes
(`game_frame` 0x24C5C, then the render passes), blits **only when t50c == t508**
(sort, render, flush, aperture→VRAM copy, swap — all inside the gate), then t50c++
(0x256C0) and spins while `t50c - 1 == t508` (0x256C5). Steady state: one present
per ISR tick, its phase inside the 16.65 ms tick window equal to that iteration's
own compute time W. When an iteration's compute exceeds the tick gap the gate
**fails** (t508 has advanced past t50c): that frame's present is skipped entirely
and the spin condition fails too — the loop burns the backlog without waiting, and
no blit touches VRAM until t50c catches t508 again. Mode-8's entry chain is exactly
such a stretch: `game_mode_08_step` (0x28468) walks fight_slot_clear (0x3C5CC), the
two camera_screen_base/camera_project passes (0x16D58/0x17FA0), camera_decay, the
fighter passes, fight_slot_pass, two fight_hud_pass walks, fight_effects_pass
(0x49C78) and camera_y_commit, plus the mode-change setup inside `game_frame`; the
skipped-iteration chain's compute spans ~2–3 tick intervals on every one of the
three captures. The reconstruction that fits the idle-loss bytes and counters:
0C71 blits normally (the held screen, raws 5187..5189), 0C72's compute overruns its
tick and skips its present, the backlog carries 0C73 (also skipped) and 0C74 —
whose gate re-opens late in tick 0x548's window (≈10–16 ms in: the skipped chain's
compute tail), blitting 1669's buffer at raw 5190 row ~3 — and 0C75's blit follows
≈5–7 ms later just after tick 0x549 (a light iteration at the natural post-tick
phase), inside the **same** scan-out. 14.27 ms refresh < 16.65 ms tick: two
sub-tick-spaced blits fit in one capture frame and the rows above them still show
the held screen; U9's tail blit (0x48E) instead landed after raw 2830's scan ended,
hence two segments there. Which skipped iteration first overruns a tick is **not**
isolable from the byte record (no S inside the chain) and the allowance does not
depend on that attribution.

This is the same mechanism family task 5 named for 3545 — "a present that follows a
re-sync … or a catch-up (the gate skips presents with no spin while DS_0010150C is
behind) lands at an arbitrary phase of the ISR tick" (title_compare.py:517-521) —
and idle-loss's instance is the **catch-up arm driven by the mode-8 entry's own
compute**, with no read, no re-sync and no poke: its 2064 rows come from the clean
fight→results transition, as required. **Not a port divergence and not a wrong
port tick model**: the port's gate/catch-up C is the raw's (differential rows), its
driver advances exactly one modelled tick per spin, host-side work never overruns a
modelled tick in a replay, so the port presents every f (its dump, keyed on new
displayed images sampled at the pump and loader hooks, can hold MORE than one
image per f but never fewer — the model part below records the U9/U10 dump lines
that showed this, correcting the shorthand this paragraph first carried), and it
can never express the skipped presents the original's mode-8 entry had — a
comparison-model limit, now with its
cause isolated (§W.12/§W.14's suspicion confirmed, escalated only in the sense of
"isolated, modelled by name"; the plan's MISMATCH-escalation carve-out for a wrong
port tick model does not apply).

**Bound measurements for the named allowance** (full-dump searches over these
current dumps; the frames themselves and their row boundaries are as above):

| capture frame | top rows [0,r1) equal port | unique adjacent pair (B,B+1) binding [r1,r2)+[r2,200) | top-segment sha256 |
|---|---|---|---|
| gp-idle-loss 2064 (r1=3, r2=98) | {1665, 1666} (6301 frames scanned) | **(1669, 1670)** — exactly one | `7bdf2c561c38a264337bc74abd093172ac99ceebb8c818635e189530cf1c58dc` |
| gp-u9-win 346 (r1=r2=5) | {218} (1977 frames scanned) | **(220, 221)** — exactly one (vacuous middle) | `703828af123edf1a8c3bade8d42bdd56a68576cd87e13de7339bdd203dc8f0b6` |
| gp-u10-ending 331 (r1=2, r2=64) | {216, 243, 1127} (4664 frames scanned) | **(218, 219)** — exactly one | `2d6fb6ad829baae61c65a844fb44f05cb51393322afdce4ab8117960e1306ddd` |

(§H.4 names for this allowance, per the plan's rule: it injects no keys — the
replay scripts and their key counts are unchanged — and reads no screen/data
bytes in `[0xA0000, 0xAFA00)`; it reads capture `.raw.gz` files and dump `.ipx`
files only, so the §H.4 hang limit and overlap bound do not bind it.)

**Model (step 2): `SCAN3_ALLOWED` in `tools/gp_compare.py`.** The plan's file list
pointed at `title_compare.py`, but the brief blesses reusing task 5's mechanism and
its home moved to `gp_compare.py` with `BANDS_ALLOWED` (§G5); the scan-out table
lives beside it. An entry is `(capture j, r1, r2, top_sha)`; the named frame is
explained ONLY if (1) the sha256 of its rows `[0,r1)` equals `top_sha` (the still-held
old screen — the segment no *unique* port frame answers for), (2) EXACTLY ONE adjacent
pair `(B, B+1)` in the CURRENT dump holds: the capture's rows `[r1,r2)` equal port[B]'s
and its rows `[r2,200)` equal port[B+1]'s — the middle test is vacuous when `r1 == r2`
(the U9 two-segment shape), where uniqueness is therefore of the single bottom holder
and the pair is (holder−1, holder) — and (3) at least one dump frame with index
A < B holds the capture's rows `[0,r1)`: the old screen being scanned IS one of the
port's own earlier frames. Classification kind `scan3` exhibits the top holders plus
B+1, and additionally B when the middle range is non-vacuous (a frame whose bound
range has no pixels is never exhibited). Like `BANDS_ALLOWED`: consulted only when the
two-frame model leaves a named frame unexplained; a named frame the model explains is
reported as a stale name (`retire the name`); a named frame that is not its recipe
prints `FAIL: allowed capture … is not its named three-frame scan-out: …` and fails
the enforced run in both modes (`allow_bad`); the summary line gained the
`scan3-allowed` count (re-checked: no other tool parses that line). Every other
scenario passes no entries — keys/moves/twop/modes/charsel behaviour is unchanged,
and task 5's band path is untouched (`make gp-keys-oracle` re-ran green).

**The entries (step 3, measured on the dumps re-played on `93b24171`).**

| scenario | capture (raw) | r1/r2 | top holders A<B (rows [0,r1)) | unique pair (B,B+1) | top sha256 |
|---|---|---|---|---|---|
| gp-idle-loss | 2064 (5190) | 3/98 | {1665, 1666} | (1669, 1670) of 6301 | `7bdf2c561c38a264337bc74abd093172ac99ceebb8c818635e189530cf1c58dc` |
| gp-u9-win | 346 (2830) | 5/5 | {218} (vacuous middle) | (220, 221) of 1977 | `703828af123edf1a8c3bade8d42bdd56a68576cd87e13de7339bdd203dc8f0b6` |
| gp-u10-ending | 331 (2831) | 2/64 | {216} of {216,243,1127} | (218, 219) of 4664 | `2d6fb6ad829baae61c65a844fb44f05cb51393322afdce4ab8117960e1306ddd` |

**The pins (step 3).** `make gp-replay` on this tree, then the enforced
`tools/gp_compare.py` calls with the Makefile pins:

- `GP_IDLE_LOSS_MIN_FIRST 2064 -> 7559`. Measured: `captured frame 2064 (raw 5190)
  allowed by name as a three-frame scan-out: top rows 0..2 held by port 1665 1666,
  port 1669 rows 3..97, port 1670 rows 98..199 the named bytes`; `7455 classified:
  4093 clean, 3352 splice, 8 transition, 1 scan3-allowed, 1 unexplained, 15
  all-black`; `first unexplained 7559, ratchet N 2064 ok (improved: raise N)`; N =
  7560 prints `FAIL: first unexplained 7559 < ratchet N 7560` — the pin is exact.
  7559 (raw 12207) is the first fully-content frame past the replayed script: the
  port's replay ends `0 restart(s) landed` with last presented f=0x207D while the
  X record is ms=174412 f=0x207F step=4 end, and the capture goes on to the soft
  restart's screens (7557 all-black, 7558 six non-black rows still matched). How far
  the port got, not a divergence — the gp-keys 1136 class. `GP_IDLE_LOSS_MAX_START
  = 90` unchanged (the window still starts at 90, exact); TRACE 8320 unchanged
  (`0 differing through 8319; ratchet N 8320 ok`).
- `GP_WIN_MIN_FIRST 346 -> 2209`. Measured: allowed line `top rows 0..4 held by port
  218, port 221 rows 5..199 the named bytes`; `2098 classified: 1105 clean, 988
  splice, 3 transition, 1 scan3-allowed, 1 unexplained, 12 all-black`; first
  unexplained `capture 2209 (raw 4953): nearest port 1777, rows 192..197, x 0..85
  (166 px)`; N = 2210 fails (exact). 2209 is NOT the §G6 class: its strip (rows
  192..197, sha256 `a886c0f3db8b2a973dfce49b8b3d3829958d788fe221da24f7d14cd02b2f23aa`)
  is held by NO frame of the 1977-frame dump — including the loader/pump-hook
  samples of the mode-0x17→0x12 entry (dumps 1775..1777 all at f=0xBA1 with its
  tick advancing 0xBFA→0xC00 inside the iteration, then one static dedup'ed image
  through f=0xBBF). The strip lands in the (0,192) text-row region — but the port
  DID dump that entry's sampled states and none matches, so unlike §K.10 item 2
  the state is not merely undumped: cause NOT attributed here, a named gap, and
  the ratchet stops at it. The strip's bytes are IDENTICAL to gp-u10-ending's
  2062 (same sha) — one state recurring at the mode-0x12 entry of both poked
  captures. §W.12's then-next-two (356/1374) are explained on the current dump
  (the P4–P7 batches ported animation targets since §W.12 measured; per-frame
  re-attribution is not done here); the report's next behind 2209 are
  2452..2455, past the 1977-frame dump. MAX_START 100,
  TRACE/WIN/MILESTONES 3503/3503/8 unchanged (`ratchet ok` lines above).
- `GP_ENDING_MIN_FIRST 331 -> 2062`. Measured: allowed line `top rows 0..1 held by
  port 216, port 218 rows 2..63, port 219 rows 64..199 the named bytes`; `1968
  classified: 965 clean, 997 splice, 4 transition, 1 scan3-allowed, 1 unexplained,
  12 all-black`; first unexplained `capture 2062 (raw 4954): nearest port 1686,
  rows 192..197, x 0..85 (166 px)` — §W.16's already-named `2062 (port 1686,
  f=0xB9A, mode 0x12, 166 px)`, the SAME strip bytes (sha above) at the same
  class of place (dumps 1684..1686 all at f=0xB9A, its tick 0xBF3→0xBF9 inside
  the iteration, then one static dedup'ed image through f=0xBB8); N = 2063 fails
  (exact). Not allowed. §W.16's other next-one, 3686, is explained on the current
  dump since the P4–P7 batches (the report's next unexplained after 2062 are
  4113..4116). MAX_START 83,
  TRACE/WIN/MILESTONES 9954/9954/30 unchanged and at the replay's end.

**Proofs (step 4).** Unit (`tools/tests/test_gp_compare.py Scan3Allowance`, 8 cases):
the named bytes explain (with the message asserted); a mutated MIDDLE-bind dump frame
un-explains it again (allowance FAILs `no unique adjacent port pair…` and the ratchet
catches, rc=1, `first unexplained 1 < ratchet N 4`); a mutated top holder answers
`no port frame before 1 holds its named top rows`; different top bytes (capture side)
are `not the named…` and fail; two matching pairs are not the named frame; the
vacuous-middle shape explains with the message `top rows 0..4 held by port 0, port 2
rows 5..199` and does NOT exhibit the middle-less B; a model-explained named frame
prints the stale-name note; and the table's three entries are pinned, so a silent
name change is a test failure. (`test_gp_compare` 46/46; the `make verify` gp tool
line — k11/gp session/capture/compare/moves/keys — 198/198; `test_title_compare`
10/10.) Real data on COPIES (`/tmp/g6_mut`, the sources untouched): poking port 1669's
row 50 index byte (42→0, DAC-differing) gives
`FAIL: allowed capture 2064 (raw 5190) is not its named three-frame scan-out: no
unique adjacent port pair binds its rows [3,98) and [98,200)` +
`FIRST UNEXPLAINED capture 2064 …` + `FAIL: first unexplained 2064 < ratchet N 7559`,
EXIT 1; the same for 1670's row 150 (bottom bind), U9's port 221 row 100 (FAIL at 346,
`< ratchet N 2209`, EXIT 1) and U10's port 218 row 30 (FAIL at 331, `< ratchet N 2062`,
EXIT 1) — per-frame per-bytes, and the traces stayed `ratchet ok` throughout. Mechanism
sensitivity on the real dumps with `SCAN3_ALLOWED` emptied in-process: all three enforce
runs print the FIRST UNEXPLAINED at their named frame and `FAIL: first unexplained <
ratchet N` with rc=1 — the pins depend on the names, and the no-mechanism lines
reproduce §U6.21/§W.12/§W.14's measured report lines byte-for-byte (idle-loss `1967
classified: 1024 clean, 941 splice, 1 transition, 1 unexplained, 8 all-black`, 346
`nearest port 221, rows 0..4, x 0..319 (933 px)`, 331 `nearest port 219, rows 0..63,
x 0..319 (10336 px)`), so the allowance adds no silent absorption.

**Named limits of the claim.** (a) the top strip is bound by EXISTENCE, not uniqueness:
several frames share the old screen's first rows (idle-loss {1665,1666}, U10 {216,243,
1127}), so a mutation of ONE holder while another holds the same rows is not felt by
THIS frame — the holders' full frames are claimed by the captures below the ratchet
(2062/2063, 345, 330) and a change to the strip in every earlier holder un-explains
(rule 3, unit-tested); (b) where `r1 == r2` (U9) the bound is on the single bottom
holder — the "pair" is (holder−1, holder) and holder−1 contributes no pixels and is
not exhibited; a bottom holder at dump index 0 would have no predecessor and fail
closed; (c) the skipped frames (idle-loss 1667/1668, U9 219/220, U10 217) stay
unexhibited by design — §G6's finding is that the ORIGINAL never blitted them; their
content is claimed by NO frame comparison, and the coverage line lists them;
(d) the standard gp_compare limits (§G.16): no under-render detection, order and
full coverage not claimed, frames at/after the pinned start classified against the
port's own dump.

**Development notes (fail-first, honest).** The first test run failed 8/8 in this
class for two test-side bugs found and fixed before green: a doubly-wrapped table
(`s3 = ((entry,),)` keyed the guard dict by the whole entry instead of the frame —
the enforced loop then never consulted the recipe: fixed to `(entry,)` as in task 5)
and a vacuous-shape expectation that printed the bind's dump INDEX (2) not value;
the model itself needed no behaviour change after those test fixes — the real-data
enforce lines above were measured before them (the allowance consulted path was
already exercised there via `main()`) and re-measured after, identical.

## §G7 — the gp-moves 2139 diagnosis (pending, lane B task 7)
