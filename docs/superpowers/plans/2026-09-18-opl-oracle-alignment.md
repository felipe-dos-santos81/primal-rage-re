# Capture-oracle alignment (re-scoped Task 3, errata #2)

Derivation for the capture oracle's symmetric reduction. The oracle lives in
`port/tests/test_sequencer.c` §8. Evidence: `tools/opl_trace.py
data/audio-captures/prage_000.dro`, `tools/opl_seq.py
data/game/C/S16TITLE.GRA --trace`, and the baselines below. Prior context:
`.superpowers/sdd/2026-09-18-opl-driver/task-3-report.md` (both sections),
`docs/superpowers/plans/2026-09-18-opl-patch-application.md`,
`port/spec/audio.md` "Known capture divergences".

## 1. The defect: two different reductions

Both sides are reduced before comparison, but by **different rules**:

* **Capture** (`:437-450`): `first_key` = index of the first `0xB0..0xB8` write
  with the key bit (`val & 0x20`). Everything **before** `first_key` is dropped
  (the driver's tick-0 reset *and* the first note's `A0`/`C0`/operators). From
  `first_key` on, `documented_excluded` (`0xBD`, carrier TLs) is dropped and
  `tick = (ms*120+500)/1000 + 60`. Result: **6380** writes.
* **Port** (`:459-461`): keep everything, including the tick-0 writes
  (`0x01=0x20`, `0x105=0x01`); during the lockstep walk skip only
  `tick == 0` and `documented_excluded`. Result: **9340** raw writes; first
  compared write is index 2.

The first compared write is therefore always the port's first tick-60
**operator** write (`0x20`) and the capture's first **key-on** (`0xB0`). Under
the driver's real order — key-on last (§1 of the patch-application doc) — these
can never align, so the reported "first difference at C write 2" is an artifact
of the reduction, not of the register stream.

## 2. Candidate evaluated and rejected: drop `tick == 0` on both sides

The #2 candidate is "drop only `tick == 0` writes and the documented-excluded
registers, on both sides, with no `first_key` slicing." It cannot work:

* The capture's first key-on is **at** `ms == 0` (line 147,
  `0 0x00b0 0x2b`), in the same tick as the driver reset. Dropping every
  `ms == 0` write deletes the capture's first note entirely while the port's
  first note survives at tick 60; the streams start at `(60, 0x20, 0x00)` and
  `(68, 0xB0, 0x0B)` — a permanent tick artifact.
* Interpreting "`tick == 0`" after the `+60` map drops nothing on the capture
  (no event maps below 60), so the original artifact is unchanged.

Neither reading reduces the two streams symmetrically. Rejected.

## 3. Chosen reduction: anchor both streams at the first key-on

Reduce **both** streams by the same rule:

1. Drop everything before the stream's first key-on — the first
   `0xB0..0xB8` write with `val & 0x20`.
2. Drop the documented-excluded registers (`0xBD`, carrier TLs).
3. Map capture `ms -> tick` with `(ms*120+500)/1000 + 60` (the `+60` anchors
   the driver's first key-on at `ms 0` to the port's first key-on at tick 60).

Justification: the driver folds its tick-0 reset **and** the first note's
patch/operators into one `ms == 0` block (spec divergence 4), so the first
note's preamble has no separately comparable capture writes. Anchoring both
streams at the first key-on discards that preamble on **both** sides instead of
discarding it on the capture only. The first compared write becomes the first
key-on on both sides, and every note-2+ operator/`C0`/`A0` preamble — which the
driver does emit per note and the port models — is still compared.

## 4. What the "normalised" count now means

* Capture **6380** = capture writes from its first key-on onward, excluding
  `0xBD` and the carrier-TL family. Unchanged: the capture side already
  anchored at `first_key`.
* Port **9340** raw = the full C stream printed by `tools/opl_seq.py`; the
  compared port subset is now the writes from **its** first key-on onward,
  excluding the same registers (previously every write from index 2).

## 5. Before / after

* Before: `capture oracle first difference at C write 2: C tick=60 reg=0x20
  val=0000 vs capture tick=60 reg=0xb0 val=0x2b (C 9340 writes, capture 6380
  normalised)` — artifact (operator vs key-on).
* After: `capture oracle first difference at C write 14: C tick=60 reg=0xb0
  val=002a vs capture tick=60 reg=0xb0 val=002b (C 9340 writes, capture 6380
  normalised)` — the first key-on on both sides, i.e. **divergence #6**
  (percussion note→fnum). Real divergence.

## 6. Negative control

Scratch-only: the port's first key-on value is perturbed to the capture's
`0xB0` value so the first compared write matches; the reported first difference
advances to the **next** real divergence (the tick-68 key-off, `0x0a` vs
`0x0b`). Restored after. A value flip that keeps the mismatch changes the
reported value at the same write. Both show the line is computed from the two
streams, never pinned. No perturbation committed.
