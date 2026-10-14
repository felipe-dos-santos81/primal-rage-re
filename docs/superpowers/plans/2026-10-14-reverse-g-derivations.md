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

## §G6 — the mode-8 three-frame scan-out (pending, lane B task 6)

## §G7 — the gp-moves 2139 diagnosis (pending, lane B task 7)
