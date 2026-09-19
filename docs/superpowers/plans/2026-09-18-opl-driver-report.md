# Report: OPL driver fidelity (sub-project 2a-ii)

Cycle: reproduce `SBPRO2.MDI`'s FM note-setup path and make the capture
comparison measure real divergence. Plan:
`docs/superpowers/plans/2026-09-18-opl-driver.md`. Design:
`docs/superpowers/specs/2026-09-18-opl-driver-design.md`.

**What this cycle proves.** The port's OPL register stream now walks the captured
original from its first key-on through the second note's operator preamble; the
first divergence is at C write 24. The comparison's reported first difference is
a **real** register divergence (the
E0-family value skip) rather than an artifact of the comparison. Two real
divergences were derived and fixed: the percussion note→fnum remap and the
18-slot channel rotation. One derivation (`velocity→TL`) ended in
record-and-stop because its input is engine-supplied. Every remaining difference
is named.

**No audio-fidelity claim is made.** The oracle is the register **write stream**
(`data/audio-captures/prage_000.dro` decoded by `tools/opl_trace.py`), never
rendered and never heard. Nothing here claims the port's FM sounds like the
original's.

## The five derivation docs (durable record)

The SDD workspace that dispatched these tasks is deleted after this cycle; these
committed derivations are the surviving evidence. Each contains the driver
instructions, image offsets (`code_origin = 0x132`) and the executable
expression.

| Doc | Derives |
|---|---|
| `docs/superpowers/plans/2026-09-18-opl-velocity-tl.md` | the driver's carrier-TL function and its engine-supplied input `V`; record-and-stop (§4–§7) |
| `docs/superpowers/plans/2026-09-18-opl-patch-application.md` | note-setup family order at `0x3184`; the driver re-emits every operator family on every note-on (the "once up front" premise is falsified) |
| `docs/superpowers/plans/2026-09-18-opl-oracle-alignment.md` | the capture oracle's asymmetric reduction and its symmetric fix |
| `docs/superpowers/plans/2026-09-18-opl-percussion-fnum.md` | the note→fnum routine `0x35fa`-`0x36a6`; melodic vs percussion base handling |
| `docs/superpowers/plans/2026-09-18-opl-channel-assignment.md` | the melodic channel allocator `0x3095`-`0x30d8` (monotonic 18-slot rotation) |

## Harness (Task 1, commit `bd1f08e`)

`tools/mdi_disasm.py` parses the `AIL3MDI` container and disassembles the
16-bit body with capstone: `load(path) -> MdiImage`,
`MdiImage.disassemble() -> list[(addr, raw_bytes, text)]`, CLI listing and
`--info`; test `tools/tests/test_mdi_disasm.py`.
`data/game/C/SBPRO2.MDI` is 16541 bytes, magic `AIL3MDI`, 16-bit real-mode.

**Load origin evidence (not assumption):** the MSS 3.X header field
`this_ISR@0x34 = 0x0132`, and the bytes at `0x132` are `cmp ax,0x300` — the
`INT 66h` dispatcher. The driver is position-independent (AIL allocates low
memory at runtime), so it is linearised at image offset `0x0132` and every
address in the derivations is an image offset on that base. Caveat: a linear
disassembly decodes embedded data as code; 151 bytes fall back to a `db ` marker.

## Before / after: the first-difference chain

The oracle's first-difference line moved through four states. Write 2 was an
**artifact of the reduction**, not of the stream; the rest are real.

| state | first difference | what it is |
|---|---|---|
| before the cycle | `C write 2: C tick=60 reg=0x20 val=0000 vs capture tick=60 reg=0xb0 val=0x2b` | **artifact**: the capture was sliced from its first key-on (dropping that note's operators/`C0`/`A0`) while the port dropped only `tick==0`, so the compared pair was always port-operator vs capture-key-on. Cannot advance by any stream change. |
| after Task 3 (oracle fix) | `C write 14: C tick=60 reg=0xb0 val=0x2a vs capture tick=60 reg=0xb0 val=0x2b` | **real divergence #6**: percussion note 47 mapped by the melodic `NOTE_TAB` (`0x28B`) instead of the patch base (`0x3CF`). |
| after Task 4 — fixed #6 (`9a22d83`) | `C write 16: C tick=68 reg=0x20 val=0000 vs capture tick=68 reg=0x21 val=0000` | **real divergence #5** (channel half): the port reused lowest-free OPL ch0; the capture rotates ch1. |
| after Task 5 — fixed #5/#7 (`ce2210d`) — **current** | `C write 24: C tick=68 reg=0xe1 val=0000 vs capture tick=68 reg=0xc1 val=0x34` | **named divergence #8**: the E0-family value skip (next section). |

The line advanced **write 2 → 14 → 16 → 24**; the write 2→14 step is the metric
fix, the 14→24 steps are two derived-and-implemented driver behaviours.

## Write counts (not narrowed — and not like-for-like)

`oracle C-vs-Python: 9340 writes byte-exact` throughout; the capture count stays
**6380 normalised**. Both counts are **unchanged** across the cycle, so the
9340/6380 number did not converge. They are not measuring the same thing: 9340
is the port's **raw** C stream (including its tick-0 enable writes and every
family it emits), while 6380 is the capture **reduced** from its first key-on and
excluding `documented_excluded`. The comparison metric is the first-difference
line above, not the count ratio. The port's 9340-writes-against-Python gate is
the tolerance-free check that both implementations agree; the capture is the
independent original. Per design §1.1 decision 2, any residual that is
structural is named and never tuned away.

## Every divergence, with disposition

Spec item numbers are `port/spec/audio.md`'s. (The cycle plan called channel
assignment "divergence #7"; in the spec the channel half is **item 5** and item 7
is the `0xBD` register. The spec update in Task 6 makes this unambiguous.)

| # | divergence | disposition |
|---|---|---|
| 1 | carrier-TL velocity attenuation | **named** (excluded): true formula derived; its input `V` is engine/config-supplied, not driver-derivable — owned by the config workstream |
| 2 | `0x105 = 0x01` (OPL3 enable) | **matched** (earlier claim withdrawn) |
| 3 | parser / XMIDI running status | **matched** (not present in any shipped bank) |
| 4 | driver cached-state init block | **matched under the symmetric reduction** (both sides anchored at first key-on) |
| 5 | per-note patch re-application + channel reuse | **matched**: the "applies once up front" premise is falsified; the rotation is implemented |
| 6 | percussion note→fnum | **matched** (Task 4) |
| 7 | OPL rhythm register `0xBD` never written | **excluded by name**, with cause |
| 8 | E0-family value skip | **named** (the current first divergence) |

### #1 — velocity→TL: derived but not implementable from the driver

Record-and-stop. The driver's function (derivation doc §1–§2):

```
base      = 0x3f - (p[10] & 0x3f)          ; p[10] = FAT.OPL carrier KSL/TL
V         = scale7(scale7(cc7, cc11), VEL_CURVE[velocity >> 3])
scal      = (base * V) / 0x7f
carrier   = ((~scal) & 0x3f) | (p[10] & 0xc0)

scale7(a,b)  = ((a*b) << 1) >> 8, then +1 unless 0        ; driver 0x31a8-0x31c4
VEL_CURVE    = 52 55 58 5b 5e 61 64 67 6a 6d 70 73 76 79 7c 7f   ; driver 0xc27
```

`V` is the product of the *received* CC7, CC11 and the velocity curve, and the
received CC7 is set by the **engine** before dispatch: the original scales
controller 7 by the sequence volume (`prage.c` around `0x2D974` /
`0x2C8F0`, music volume = `query(0x35)/2`) and sequence init sends
`CC7 = DAT_00108D94`, `CC11 = 0x7f`. `V` is therefore an engine/config parameter
and is **not derivable from `SBPRO2.MDI`**. The capture's effective `V = 0x53`
was a capture fit (commit `7bf339a`) and has been reverted (`f15b588`); the port
writes `p[10]` verbatim. **Provenance of `V` is owned by the config workstream.**
One captured row remains unexplained by the code path read: patch `0x34`
(`p[10] = 0x83`) is written `0x9a` where the derivation gives `0x98`, while the
same low-6-bits patch `0x74` (`0x03`) matches. This is a **named residual**, not
fitted.

### #5 — patch-application timing: premise falsified

The original Task 3 premise was "the driver emits the operator families once, up
front". It is false: `0x30c8` sets `[si+0x1539] = 0xf9` unconditionally on every
note-on, driving the state machine at `0x3184` to emit the whole family run
`20 23 40 43 60 63 80 83 E0 E3 C0 A0 B0` (**key-on `0xB0` last**) for every
note. The port's per-note `apply_patch` already matched this for notes 2+; only
the **first** note's operators are folded into the driver's tick-0 init (reset
`0x2c47` + tables `0xa3d`/`0xb32`), which is why the capture's first note has no
operator writes of its own. No register-order change was warranted. The
*channel-reuse* half of item 5 was real and is fixed by Task 5.

### #6 — percussion note→fnum: fixed (commit `9a22d83`)

One frequency routine `0x35fa`-`0x36a6`; the patch's base byte
`[di+2]` feeds it differently by channel (`0x3aac`-`0x3ac1`):

* **melodic** (`channel != 9`): index `NOTE_TAB[note + base]`; all 128 melodic
  `FAT.OPL` entries have `base == 0`, so `NOTE_TAB[note]` stands.
* **percussion** (`channel == 9`): index `NOTE_TAB[base]` alone — the MIDI note
  does not enter the frequency.

MIDI note 47's percussion key `0x7F2F` has payload `[2] = 54`;
`NOTE_TAB[54] = (block 2, fnum 0x3CF)` → `0xA0 = 0xCF`, `0xB0 = 0x2B`, matching
the capture's first key-on. Mirrored in `tools/opl_seq.py`.

### #7→item 5 — channel assignment: fixed (commit `ce2210d`)

The melodic allocator `0x3095`-`0x30d8` is a **monotonic rotation cursor**
`[0x1408]` (init `0xffff` at `0x306e`) over the 18 OPL3 owner bytes
`[0x1a49]`: probe next, wrap at 18, store the cursor on every probe, take the
first free slot. Freeing (`0x3162`) does **not** move the cursor, so freeing a
low channel does not pull the next note back to it. The port now models 18 OPL3
channels, a banked `OPL_SLOT` and a `ch_reg` mapping recomposed byte-for-byte
from the driver tables (`0xc37`/`0xc49` → `0xc5b`/`0xc7f`; `0xca3`/`0xcb5`);
carrier = modulator + 3. Every shipped `FAT.OPL` payload (all 181, melodic and
percussion keys alike) has type `[0] = 0x0e`, so the percussion allocator
`0x30e1` (voice type 3) is **never reached** and is not modelled.

### #8 — E0-family value skip: current first divergence, named

At C write 24 the port writes `0xE1 = 0x00` where the capture writes
`0xC1 = 0x34`. The disassembly's E0 handler `0x3542` writes E0 unconditionally,
but the capture emits the E0 family only where the patch's E0 byte is non-zero /
changes (`0x3542` writes it always; the capture's E0 writes occur only for
non-zero E0 values). This is a **value-level skip** the listing does not expose
(either a shadow path the linear disassembly did not decode, or a differing
driver build). Modelling it would be a guess, not a derivation, so it is
recorded and left. It is the next thing a future cycle would derive.

## Named gaps (not fitted)

* **Velocity→TL `V`** — engine/config-supplied; owned by the config workstream
  (`0x2D974`). See #1.
* **`0x34` residual** — captured `0x9a` vs derived `0x98`; unexplained by the
  code path read. See #1.
* **E0-family value skip** — see #8.
* **Voice-steal policy `0x36f6`-`0x3816`** — the driver steals by quietest voice
  using per-MIDI-channel counts `[ch+0x1a39]` and priorities `[ch+0x1979]`; the
  port keeps its documented oldest-voice steal. With 18 slots and this bank's
  density the allocator was not observed to exhaust in the compared window, so
  the policy does not affect the compared stream.
* **Percussion allocator `0x30e1`** — unreachable for the shipped bank (all
  payloads type `0x0e`); a bank with type-`0x19` percussion payloads would need
  it.
* **Pitch bend / `[ch+0x18f9]`** — unmodelled; no bend in the capture window.
  `TODO(verify)` if a later capture exercises bend.

## Verification (Task 6, Step 3)

`rm -rf build && make verify` — **exit 0**:

* 0 compiler warnings (full clean build).
* `oracle C-vs-Python: 9340 writes byte-exact`.
* `capture oracle first difference at C write 24: C tick=68 reg=0xe1 val=0000
  vs capture tick=68 reg=0xc1 val=0x34 (C 9340 writes, capture 6380
  normalised)`.
* `smk_compare: 120/120 frames match`, `smk_compare: 41/41 frames match`.
* title oracle: both captures 0 unexplained, determinism 54 agree / 0 disagree.
* `gra_extract` oracle tests: `Ran 32 tests ... OK`.
* `symbols.h` regenerates byte-identically (`1304 globals, 1206 functions`).
* `all checks passed`.

## Negative control (oracle still fails)

Scratch-only, from the oracle-alignment doc §6: perturbing the port's first
key-on value to match the capture moved the reported first difference to the
**next** real divergence (tick-68 key-off), and a value flip that keeps the
mismatch changed the reported value at the same write. The line is computed from
the two streams, never pinned. No perturbation committed. The governing
C-vs-Python gate is untouched by the oracle change and stays byte-exact.

## Outcome against the design's DoD (§6/§7)

* `make verify` exit 0, 0 warnings, `symbols.h` byte-identical, no `data/`
  change, title/smk/GRA oracles unchanged — **met**.
* The capture comparison's first-difference line demonstrably advanced from
  write 2 — **met** (to write 24).
* Every divergence carries a disposition — **met** (spec update, Task 6 Step 2).
* The TL cases all match — **not met**: record-and-stop, because the input `V`
  is engine-supplied (design §4 assumed a driver-derivable function). This is the
  cycle's honest negative result and is carried in the spec and here.
