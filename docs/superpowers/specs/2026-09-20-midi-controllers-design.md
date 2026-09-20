# MIDI Controllers and the Mid-Note Frequency Path — Design

**Status:** design approved, spec for review.
**Cycle:** audio controller cycle. Predecessors: the OPL driver/oracle cycles
(`docs/superpowers/plans/2026-09-18-opl-*.md`) — all merged on `main`.

## 1. Goal and scope

Port `SBPRO2.MDI`'s MIDI-controller handling so a note's OPL registers are
re-applied when a controller changes, the way the driver does at runtime. This is
what closes the two remaining named audio divergences:

- **item 10** — the mid-note frequency change: the capture rewrites a sounding
  voice's `0xA0`/`0xB0` (fnum) without a key-on. The driver does this on a pitch
  bend, via the family re-apply below.
- **item 1** — the TL level term: the driver folds channel volume and expression
  into the total level. The spec previously called the input "engine-supplied and
  not driver-derivable"; the driver's handler shows the full formula, so it is
  derivable and the `documented_excluded` TL family is retired.

**In scope.**

| raw | role |
|---|---|
| `0x3b54` | the channel-controller handler (`FUN_0000_3b54`): status + 2 data bytes |
| `0x3184` | the mask-driven register-family applier (`FUN_0000_3184`), re-run per voice after a controller |
| `0x35fa`-`0x36a6` | the note-frequency routine: wheel + bend scale + patch base → block/fnum |
| `0x7fd`, `0x97d`, `0x9dd` | the driver's pitch tables (192 words, 96 B octave, 96 B semitone) |

Controllers covered: `0xE0` (bend), ctrl 6 (bend scale), ctrl 7 (volume),
ctrl 11 (expression), ctrl 1 (mod wheel), ctrl 10 (pan), ctrl 64 (sustain),
ctrl 121 (reset all controllers), ctrl 123 (all notes off). Bank select (ctrl 0)
already exists.

**Out of scope.** The percussion allocator, the voice-steal policy, the parser's
status set, and the sample/mixer paths are unchanged.

## 2. Raw evidence

- `S16TITLE.GRA` (the title bank) contains **83** `0xEn` pitch-bend events and
  **9** controller-6 events, plus ctrl 0/7/10/11/91/93/100/101/116/121/123. The
  port currently ignores all of them except bank select.
- The Ghidra decompilation of `data/game/C/SBPRO2.MDI` gives `FUN_0000_3b54`
  (dispatch) and `FUN_0000_3184` (applier). The applier gates each register
  family on a mask bit in `[si+0x1539]`: `0x80` → `0x20` family, `0x40` → TL
  (`0x40`), `0x20` → `0x60`/`0x80`, `0x10` → `0xE0`, `0x08` → `0xC0`, `0x01` →
  `0xA0`/`0xB0`.
- After a controller is handled, `0x3b54` (lines `0x3c53`-`0x3c63`) loops every
  voice record, and for each **active** voice on the same MIDI channel ORs the
  selected family bit into `[si+0x1539]` and calls `FUN_0000_3184`. That is the
  mid-note re-apply.
- The pitch tables are the driver's own data: `0x7fd` is 192 little-endian words
  (12 semitones × 16 fine steps; entry 0 is `690 = 0x2B2`, the known note-84
  anchor), `0x97d` is the 96-entry octave table, `0x9dd` the 96-entry semitone
  table.

## 3. Architecture and ownership

The port's `apply_patch()` writes every family unconditionally; the driver is
mask-driven. The design adopts the driver's shape.

- **`fam_apply(opl_ch, mask)`** replaces `apply_patch`'s body. It writes only the
  families present in `mask`, in the driver's order, reading the voice's patch
  payload and the channel's controller state. `key_on` becomes *load the patch
  into the voice, then `fam_apply(ch, FAM_ALL)`*.
- **Controller state** lives in the sequencer's `S`, one set per MIDI channel.
- **`midi_control(status, ch, a, b)`** ports `0x3b54`: store, then re-apply the
  selected family across the channel's active voices.
- **`port/src/platform/audio/pitch.{c,h}`** (new) owns the driver's table blob
  and a pure `pitch_lookup(index, bend, *block, *fnum)`, with no access to `S`.
  The existing `NOTE_TAB` retires into it.

`sequencer.c` keeps the voice pool, the event loop, the controller state and
`fam_apply`; `pitch.c` is pure and independently testable; `mixer`/`opl` are
untouched.

## 4. Controller state and dispatch

Per MIDI channel, with the driver's byte addresses for traceability:

| state | driver | set by |
|---|---|---|
| wheel LSB / MSB | `0x1929` / `0x1939` | `0xE0` |
| bend scale | `0x18f9` | ctrl 6 |
| volume | `0x1909` | ctrl 7 |
| expression | `0x1949` | ctrl 11 |
| pan | `0x1919` | ctrl 10 |
| mod wheel | `0x1959` | ctrl 1 |
| sustain | `0x1969` | ctrl 64 |

Defaults come from the driver's reset block (`0x3c41`-`0x3c53`: `0x1929 = 0`,
`0x1939 = 0x40`, `0x1949 = 0x7f`, `0x1959 = 0`, `0x1969 = 0`) and BSS zero for
the rest. ctrl 121 runs that reset then re-applies families `0xC1`.

`midi_control` mapping (from `0x3b54`):

| input | store | family mask | re-apply |
|---|---|---|---|
| `0xE0` | wheel LSB/MSB | `0x01` | yes |
| ctrl 6 | bend scale | — | no (returns) |
| ctrl 7 | volume | `0x40` | yes |
| ctrl 11 | expression | `0x40` | yes |
| ctrl 1 | mod wheel | `0x80` | yes |
| ctrl 10 | pan | `0x08` | yes |
| ctrl 64 | sustain | — | `0x3b1e` sustain path |
| ctrl 121 | reset block | `0xC1` | yes |
| ctrl 123 | — | — | all notes off (`0x39cc` per voice) |
| anything else | — | — | no |

## 5. The frequency path (`pitch_lookup`)

The routine `0x35fa`-`0x36a6`, with the wheel `msb`/`lsb`, the bend scale `s`, and
the pitch index `index` (`note + base` melodic, `base` percussion):

```
w    = (msb << 7 | lsb) - 0x2000          # 14-bit wheel, signed about centre
bend = (w >> 5) * s                        # imul, signed; s is ctrl 6
bx   = index                               # already note+base / base
ax   = (bend + ((bx & 0xff) << 8) + 8) >> 4   # signed
ax   = clamp(ax, 0, 0x5ff)
idx  = ax >> 4
v    = pitch_tbl[16 * t9dd[idx] + ((2*ax) & 0x1f)]   # one octave of fnum, 16 steps/semitone
block = t97d[idx] - 1 + (v < 0)
if block < 0: block++, v >>= 1
a0 = v & 0xff
b0 = (block << 2) | ((v >> 8) & 0x03)
```

At `s == 0` (or a centred wheel) this reduces to `NOTE_TAB[index]` for every
index — that equivalence is the test that pins the port. The tables are copied
into `pitch.c` as literals derived from `SBPRO2.MDI`; `data/` stays read-only.

## 6. The family apply and the level/pan/mod formulas

`fam_apply(ch, mask)` writes, in the driver's order:

- **`0x20` (AM/VIB/EG/KSR/MULT)** — patch bytes `[3]`/`[9]`, with the AM bit set
  when the channel's mod wheel `0x1959 >= 0x40`.
- **TL (`0x40`)** — patch bytes `[4]`/`[10]` combined with the channel level:
  `factor = ((volume * expression * 2) >> 8)`, then per operator
  `TL = patch_TL | ~scaled(factor)`, where the scaling and the per-voice enable
  flag (`0x18e5`) follow `0x3184` lines `0x30`-`0x44`. The exact fold is derived
  in the plan; the opcode is `FUN_0000_2aa4(uVar, 0x40, ...)`.
- **`0x60`/`0x80` (AD/SR)** — patch bytes `[5]/[6]`, `[11]/[12]`.
- **`0xE0` (wave)** — patch bytes `[7]`/`[13]`.
- **`0xC0` (connection/pan)** — `patch[8] | pan_bits`, where
  `pan_bits` is `0x20` when `0x1919 < 0x1c`, `0x10` when `> 99`, else `0x30`.
- **`0xA0`/`0xB0` (frequency)** — `pitch_lookup` (section 5), writing `0xA0 = a0`
  and `0xB0 = b0 | 0x20` for a key-on, or `b0` (key state preserved) for a
  mid-note re-apply.

## 7. Data flow

`process()` parses an event; `0x90/0x80` key on/off stay as they are today. The
controller portion of the driver's dispatcher is what this design adds: `0xE0`
(bend) and `0xB0` (controllers) call `midi_control`, which mutates `S`'s
controller state and calls `fam_apply` for each active voice on the channel.
`0xA0` (aftertouch) and `0xD0` (channel pressure) are **ignored** by `0x3b54`
(its `else` chain returns for them), so the port keeps skipping them — no change,
and no new gap.

## 8. Verification

1. **`pitch_lookup` unit tests** — centred equals the retired `NOTE_TAB` for all
   128 indices; the anchors (note 84 → block 5 fnum `0x2B2`, note 79 → `0x205`);
   at least one bent case checked against the driver's arithmetic.
2. **`midi_control` unit tests** — wheel store, ctrl-6 scale, the mask each
   controller selects, and that only active voices of that channel are
   re-applied.
3. **C-vs-Python byte-exact** — `tools/opl_seq.py` gains the same controller and
   pitch models, written independently (its own structure, as it is today), and
   the gate stays byte-exact over the larger stream.
4. **Capture oracle** — the first difference must advance past C write 302
   (tick 969); the `documented_excluded` TL family `0x40-0x55` is removed; any new
   first divergence is named, not tuned.
5. **Audibility** — `make audio-render` regenerated, and the listening checklist
   re-run (bend and level are audible changes).

## 9. Sequencing

Each step is independently provable, and steps 1-2 must leave both oracles
unmoved:

1. `pitch.{c,h}` with the tables and `pitch_lookup`; retire `NOTE_TAB` (prove the
   equivalence and that the capture oracle is unmoved while bend is centred).
2. `fam_apply(mask)` replaces `apply_patch` (prove both oracles unmoved).
3. Controller state + `midi_control` + the re-apply loop.
4. TL / pan / mod formulas; drop the TL exclusion; advance the capture oracle.
5. Update `tools/opl_seq.py` in lockstep with steps 1-4.
6. Docs and the audibility checklist.

## 10. Risks and declared gaps

- **Driver defaults.** BSS-zero for controllers the reset block does not set
  (bend scale, volume, pan) must be confirmed against the capture's first
  controller events, not assumed.
- **Unexercised controllers.** ctrl 64/91/93/100/101/116 are not in the title
  bank; port the dispatch, and name any whose behaviour stays unproven.
- **`0x3b1e` (sustain) and `0x39cc` (all-notes-off)** need their own derivation;
  if they are not exercised they are named gaps with dispatch-level unit proof.
- **Audibility.** This changes what the port sounds like. The old
  "slow announcer / wrong tempo" report must be re-checked after the change.
- **Oracle cost.** `tools/opl_seq.py` grows; keeping it an independent
  implementation (not a transliteration of `sequencer.c`) is a review criterion.
