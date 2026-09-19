# SBPRO2.MDI patch-application timing

Derivation for the re-scoped Task 3 of the OPL driver-fidelity cycle. All
addresses are image offsets (`code_origin = 0x132`, from Task 1's harness).
Evidence: `tools/mdi_disasm.py data/game/C/SBPRO2.MDI`,
`tools/opl_trace.py data/audio-captures/prage_000.dro`, `tools/opl_seq.py
data/game/C/S16TITLE.GRA --trace`, and `port/tests/test_sequencer.c`'s capture
oracle.

**Outcome: BLOCKED.** The driver's per-note timing is established, and it turns
out the port already matches it for every note after the first. The task's
premise — "the operator families are emitted once, up front" — is falsified by
the capture: the driver re-emits the whole operator set on every note-on
(§3). The write-2 divergence is caused by (a) the driver folding the *first*
note's setup into its tick-0 init, and (b) two value/state divergences the
port's plan does not yet address: the percussion note→fnum remap (#6, Task 4)
and the driver's channel rotation (#5). No register-order or family-timing
change can move the line on its own (§5).

---

## 1. Note-setup register order (already established)

The note-setup writer is **`0x3184`**, a flag-driven state machine over the
per-voice byte `[si+0x1539]`. The dispatch at `0x3313` tests the bits in this
order; each handler writes its family, clears its bit, and jumps to the next
test:

| test | jump | handler | registers (mod then car) |
|---|---|---|---|
| `0x80` | `0x33f9`→`0x3409` | mod/car `0x20` | `0x20` (slot tbl `0xc37`) then `0x23` (tbl `0xc49`) |
| `0x40` | `0x33fb`→`0x346a` | mod/car `0x40` | `0x40` then `0x43` |
| `0x20` | `0x33fd`→`0x34de` | `0x60`+`0x80` | `0x60`,`0x63`, then `0x80`,`0x83` |
| `0x10` | `0x3400`→`0x3542` | mod/car `0xE0` | `0xE0` then `0xE3` |
| `0x08` | `0x3403`→`0x3578` | `0xC0` | `((x>>4)&0xe)|bits|0x30` at `0xC0` |
| `0x01` | `0x3406`→`0x35c2` | `0xA0`,`0xB0` | fnum low at `0xA0`; block\|fnumhi\|`0x20` at `0xB0` |

Derived order for one note: `20 23 40 43 60 63 80 83 E0 E3 C0 A0 B0`
(**key-on `0xB0` last**). The mod/car slot tables are `0xc37`/`0xc49`; the
emitters are `0x2aa4` (operator families) and `0x2abe` (`0xC0`/`0xA0`/`0xB0`),
both funnelling into the straight `out` at `0x2ad6` (no shadow-register skip in
the emitter; the register low byte and bank come from the tables at `0xc5b`
and `0xc7f`).

## 2. `[si+0x1539]` — what sets and clears it

Every write/read of the byte (grep of the full listing):

| addr | insn | meaning |
|---|---|---|
| `0x30c8` | `mov byte [si+0x1539], 0xf9` | **note-on** (melodic branch `0x3095`, and the percussion branch `0x30e1` reaches the same `0x30c8` via `0x3125`) |
| `0x3802` | `mov byte [si+0x1539], 0xf9` | **percussion scheduler** entry |
| `0x39c2` | `mov byte [si+0x1539], 0xf9` | end of the patch-parameter load (`0x38f1`..`0x39cb` region) |
| `0x313c` | `or byte [si+0x1539], 1` | **note-release** (`0x3127`), only the `0xA0`/`0xB0` bit |
| `0x3c65` | `or byte [si+0x1539], al` | controller / patch-parameter handler; `al` is a single family bit |
| `0x3462` | `and byte [si+0x1539], 0x7f` | `0x80` family handler clears its bit |
| `0x34d6` | `and ..., 0xbf` | `0x40` cleared |
| `0x353a` | `and ..., 0xdf` | `0x20` cleared |
| `0x3570` | `and ..., 0xef` | `0x10` (E0) cleared |
| `0x35ba` | `and ..., 0xf7` | `0x08` (C0) cleared |
| `0x36ee` | `and ..., 0xfe` | `0x01` (A0/B0) cleared |

So a note-on sets **all six family bits** (`0xf9`) — it is **not** conditioned
on the patch having changed. By the disassembly, every note-on emits the whole
`20 23 40 43 60 63 80 83 E0 E3 C0 A0 B0` run. The percussion scheduler and the
patch-parameter load do the same.

**Unresolved discrepancy (recorded, not guessed).** The capture contradicts the
"E0 always" reading: `prage_000.dro`'s ch1 and ch2 note runs (ticks 63/123)
emit `0x21,0x24,0x41,0x61,0x64,0x81,0x84,0xC1,0xA1,0xB1` with **no `0xE0`/`0xE3`**
writes, while `0x3542` writes E0 unconditionally. E0-family writes in the whole
capture number 102 and occur only where the patch's `E0` byte is non-zero
(first at tick 192, ch3, `0xE8 = 0x02`). That is a value-level skip: the driver
emits the E0 family only when its value differs from the channel's current
state. The skip is **not visible in the disassembled emitter**, so either it
lives in a shadow path this listing does not decode, or the capture's driver
build differs from `data/game/C/SBPRO2.MDI`. Either way it is a *value* skip,
not a change to *when* the families are emitted, and it does not create a
"once up front" model.

## 3. Where the tick-0 block comes from — and what it means

The tick-0 block (`prage_000.dro` capture lines 1–147) is the driver's
**register reset plus the first voice's patch**, not a per-note state:

* The reset is `0x2c47` (called from `0x3e85`/`0x3e92` in the driver's
  OPL3-detect/init path): write `0x105=1`, `0x104=0`, then sweep
  registers `1..0xf5` and `0x101..0x1f5` from the tables at `0xa3d` and
  `0xb32`. Those tables hold `0x20-family=0x01`, `0x40=0x3f`, `0x60=0xff`,
  `0x80=0x0f`, `0xBD=0xc0`, `0x105=0x01` — and the observed capture values
  `0x01/0x3f/0xff/0x0f`.
* The capture's `0x60=0xf6`, `0x80=0x0c/0x06`, `0x43=0x16` for OPL slots 0/3
  are the *first note's patch* written over the reset defaults on ch0.
* The capture's first key-on (`0xA0=0xcf, 0xBD=0xc0, 0xC0=0x34, 0xB0=0x2b`,
  line 147) is at capture tick 0, the same tick as the reset.

The first note's run therefore has **no operator writes of its own**: they are
folded into the tick-0 init. Every later note re-emits them normally:

```
tick 63  ch0 off  0xB0=0x0b
tick 63  ch1 note  0x21 0x24 0x41 0x44 0x61 0x64 0x81 0x84 0xC1 0xA1 0xB1=0x2b
tick 123 ch2 note  0x22 0x25 0x42 0x45 0x62 0x65 0x82 0x85 0xC2 0xA2 0xB2=0x2a
tick 191 ch3 note  0x28 0x2b 0x48 0x4b 0x68 0x6b 0x88 0x8b 0xE8 0xC3 0xA3 0xB3=0x26
```

(`0x44`/`0x4b` are carrier-TL and are skipped by the oracle's
`documented_excluded`.) **The port's per-note `apply_patch` already reproduces
this for notes 2+.** The only "timing" difference is the first note, whose
operators the driver emitted as part of init at the same tick as its key-on.

## 4. How the capture oracle normalises (Step 2)

`port/tests/test_sequencer.c`, "8. Capture oracle":

* Capture side: `read_ev_stream` reads `tools/opl_trace.py`'s `"tick reg value"`
  lines. `first_key` = index of the first `0xB0..0xB8` write with `val & 0x20`.
  Everything **before** `first_key` is dropped (that is the tick-0 reset/init).
  From `first_key` on, `documented_excluded(reg)` (`0xBD`, carrier TLs) is
  dropped, and each tick is converted with
  `tick = (ms * 120 + 500)/1000 + 60`. Result: **6380** normalised writes.
* Port side: `capture_c_stream` tags `seq_start`'s writes as tick 0 and each
  `seq_tick`'s writes with that tick. It includes the tick-0 writes
  (`0x01=0x20`, `0x105=0x01`) and every register. Result: **9340** writes.
* Lockstep walk: while `c_ev[ci].tick == 0 || documented_excluded(c_ev[ci].reg)`
  skip `ci`. Compare `c_ev[ci]` with `cap_ev[pyi]`; report at the first
  mismatch. `ci` is a raw index into `c_ev`, so the two tick-0 writes are
  "C write 0" and "C write 1"; the first compared port write is "C write 2".

The normalisation is thus **asymmetric for the first note**: the capture's
tick-0 reset *and its first key-on's preceding `A0`/`C0`* are dropped (they
precede `first_key`), but the port's first note's `A0`/`C0`/operators are kept
(they are at port tick 60, not tick 0). The capture's first key-on `B0` survives
at tick 0→60. So `cap_ev[0] = (60, 0xB0, 0x2B)` while the port's first compared
write is its note's first operator write.

## 5. Why no family-timing change can advance the line

The oracle reports at the first `c_ev[ci]` that differs from `cap_ev[pyi]`
starting at `ci = 2`. For the line to advance past write 2, the port's first
compared write must equal `cap_ev[0] = (60, 0xB0, 0x2B)`. That needs **both**:

1. **The port's first post-tick-0 write must be `0xB0`.** Today it is the
   `0x20` family. Even if the port folded the first note's operators into
   `seq_start`, `key_on` still writes `0xA0` before `0xB0` (correct driver
   order, §1), so the first compared write would be `0xA0`, not `0xB0`. The
   driver's first note has no `A0`/`C0` before its `B0` *in the oracle's view*
   only because its `A0`/`C0` sit before `first_key` and are dropped. The port
   cannot reproduce that without moving its first note's `A0`/`C0` to tick 0 —
   which is not the driver's timing, it is the oracle's anchor.
2. **The `B0` value must be `0x2B`.** The port computes `0x2A` from
   `NOTE_TAB[47]` (block 2, fnum `0x28B`); the capture's note 47 is the driver's
   percussion remap to `(2, 0x3CF)`. This is divergence #6, Task 4.

So a timing change alone leaves the first difference at index 2 (at best the
register matches and the value still differs). This is exactly the prior Task 3
report's experiment 1 result.

Beyond the first note there is a second, independent blocker: **channel
rotation**. The port's `alloc_voice` returns the lowest free channel, so its
first four notes all play on OPL ch0 (slots 0/3 → registers `0x20`,`0x23`),
while the capture rotates ch0, ch1, ch2, ch3 (slots 0/3, 1/4, 2/5, 8/0xb →
`0x20/0x23`, `0x21/0x24`, `0x22/0x25`, `0x28/0x2b`). Even after #6 is fixed, the
next difference is that operator register (`0x20` vs `0x21`). This is the
"channel reuse" half of spec divergence #5 and is not addressed by a
pending-family model.

Finally, a faithful pending-family/shadow model *does* change the C stream (it
would suppress re-emission on a reused channel with an unchanged patch, e.g.
the port's notes 1–3 on ch0). The governing oracle #7 requires the C stream to
equal the independent Python oracle `tools/opl_seq.py` byte for byte; Task 2's
precedent (`7bf339a` touched `sequencer.c`, `test_sequencer.c`,
`tools/opl_seq.py`, and the plan doc) is that any C behaviour change must be
mirrored there. This brief's commit lists only three paths and would leave the
governing gate red, or leave `tools/opl_seq.py` uncommitted. Neither is
acceptable.

## 6. Recommendation

* **No code change is warranted for the "operator family timing" question.**
  The port's per-note `apply_patch` already matches the driver for every note
  after the first; the disassembly (`0x30c8` → `0xf9` → `0x3184`) and the
  capture agree.
* **Do Task 4 (#6) next** — it is the value that actually blocks write 2.
* **Add a channel-allocation task** for the "channel reuse" half of #5: rotate
  OPL channels like the capture instead of reusing the lowest free one. Without
  it the line stops at the second note's operator register.
* If a pending-family/shadow model is still wanted (to reproduce the E0 value
  skip), it must be mirrored in `tools/opl_seq.py`, and the commit must include
  that file to keep the governing C-vs-Python gate green.

## 7. Evidence / commands

* `python3 tools/mdi_disasm.py data/game/C/SBPRO2.MDI` — full listing (7578 lines).
* `python3 tools/opl_trace.py data/audio-captures/prage_000.dro` — 7201 events;
  first key-on line 147.
* `python3 tools/opl_seq.py data/game/C/S16TITLE.GRA --trace` — 9340 writes;
  first note at tick 60 on ch0.
* `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests` — baseline:
  `capture oracle first difference at C write 2: C tick=60 reg=0x20 val=0000 vs
  capture tick=60 reg=0xb0 val=0x2b (C 9340 writes, capture 6380 normalised)`.
* No `data/`, `.dro`, `tools/opl_trace.py`, or `tools/opl_seq.py` changes.
