# Design: Primal Rage (DOS) → SDL3 port — sub-project 2a-ii: reproduce `SBPRO2.MDI`'s FM note path

Sub-project 2a ported AIL/Miles audio and proved the *port's own* OPL
reconstruction against a Python reference (9340 writes byte-exact). Its report
(`docs/superpowers/plans/2026-09-16-audio-ail-port-report.md`) records that the
reconstruction is **not** the original's: the port's stream diverges from the
captured original structurally, first at write 2. This cycle closes that gap.

## 1. Context

* The captured original lives at `data/audio-captures/prage_000.dro` (7201 FM
  register writes over 28117 ms) and `prage_001.dro` (1052 over 9433 ms),
  decoded by `tools/opl_trace.py`. `make verify` reports the divergence as:
  `capture oracle first difference at C write 2: C tick=60 reg=0x20 val=0x0000
  vs capture tick=60 reg=0xb0 val=0x2b (C 9340 writes, capture 6380 normalised)`.
* `0xb0` is the channel feedback/connection register and `0x2b` has the key-on
  bit set, so the original keys a note on at write 2 while the port is still
  emitting operator registers — a note-setup **ordering** difference, not a
  single wrong value.
* The driver that produced the capture is shipped: `data/game/C/SBPRO2.MDI`
  (16541 bytes, magic `AIL3MDI`), the Sound Blaster Pro 2 FM driver. It is
  16-bit real-mode code and has never been analysed in this repo.
* The capture shows the driver's cached dual-OPL2 state: `0x01=0x20`,
  `0x105=0x01`, a second register set (`0x120`/`0x121`/…), and key-ons on
  `0xB0`–`0xB8` and `0x1B0`–`0x1B8`. It also shows `0xC0 |= 0x30` and a carrier
  TL velocity term.
* `port/spec/audio.md` ("Known capture divergences") marks one divergence
  **open**: the exact velocity→TL function. A dominant form
  `[10] + 0x16 + ((127 - velocity) >> 3)` fits the `[10] = 0x00`/`0x40`
  patches, but patch `0x34` is +1 and `0x74` is −1, so the port writes `[10]`
  verbatim. The spec names exactly what would settle it: *"disassemble
  `SBPRO2.MDI`'s velocity→TL path, or capture a velocity sweep"*.

### 1.1 Decisions taken during brainstorming

| # | Decision |
|---|---|
| 1 | **Approach A, driver-driven.** Recover the behaviour from `SBPRO2.MDI`, not by fitting the capture, because the ±1 TL residual proves the capture alone is underdetermined and rare write paths would be guessed. |
| 2 | **DoD = the driver's note-setup path + the TL term.** Full byte-exactness of all 9340 writes is *not* required; a residual that is structural (e.g. voice allocation) is named, never tuned away. |
| 3 | **The TL function goes first.** It is isolated, closes the one divergence the spec marks open, and proves the harness before the larger write-order work. |
| 4 | **The oracle is the write stream**, not rendered audio: the existing `test_sequencer.c` capture comparison already compares register writes, so no listening or audio oracle is needed. |
| 5 | **No audio-fidelity claim.** This cycle proves the register stream matches further than before; it does not claim the rendered FM is audibly identical. |

## 2. Scope

### In scope

* `tools/mdi_disasm.py` — parse the `AIL3MDI` header, load the driver body at its
  real-mode origin, disassemble 16-bit x86 with capstone to a listing.
* The velocity→TL function, derived from the driver and transcribed so the
  port's carrier TL matches every captured case.
* The note-setup register sequence: operator/patch writes, the `0xB0` key-on
  placement, `0xC0 |= 0x30`, and the second-OPL2-set mirror.
* Updating `port/spec/audio.md`'s divergence list with dispositions.

### Non-goals

* Voice allocation and steal policy **beyond what the note-setup path needs**.
* Rendered-audio or audibility verification (no device oracle exists here; the
  audibility blocker from 2a is unchanged by this cycle).
* The other shipped `.MDI`/`.DIG` drivers.
* Any change to `tools/opl_trace.py`, the `.dro` captures, or `data/`.

## 3. Architecture

Two pieces, each where its invariant belongs.

**`tools/mdi_disasm.py` (new)** — the disassembly harness. It owns parsing the
`AIL3MDI` container (magic + header), locating the code body and its load
origin, and emitting a listing. It is a tool, not port code: it is used to
derive facts, and its output is committed as evidence in the cycle report.

**`port/src/platform/audio/sequencer.c`** — the transcribed note-setup writer and
the TL function. The velocity→TL transform is applied **at note setup in
`sequencer.c`**, where the velocity is known, not in `patches.c` at decode time
(patch decode has no velocity). The existing sequencer already maps program
changes through the FAT.OPL bank; this cycle changes how a note's registers are
computed and in what order they are written.

## 4. Task one — the velocity→TL function

Derive, from the driver, the exact function mapping a note's velocity (and
whatever per-patch state the ±1 residual depends on) to the carrier TL byte the
capture shows. The residual is the point: a formula that only fits the
`[10] = 0x00`/`0x40` patches is the already-rejected dominant form.

Verified against every captured case the spec lists:
`0x49` (127/122→`0x16`, 113→`0x17`, 104→`0x18`), `0x1e`/`0x58` (127→`0x16`),
`0x24` (120/127→`0x56`), `0x34` (127→`0x9a`), `0x74` (115→`0x19`,
126/127→`0x18`).

## 5. Task two — the note-setup write order

Reproduce the driver's register sequence for a note so the C stream aligns with
the capture from write 2. This includes the operator/patch write order, where
the `0xB0` (and second-set `0x1B0`) key-on falls, and the `0xC0 |= 0x30`
output-enable. The DoD is that the test's **first-difference line advances past
write 2**, and as far beyond as the derived slice reaches.

## 6. Oracle and Definition of Done

* `make verify` exit 0, 0 warnings, `symbols.h` byte-identical, no `data/`
  change, and the title/smk/GRA oracles unchanged.
* The TL cases above all match the capture.
* The capture comparison's first-difference line **demonstrably advances** from
  write 2, or reaches `all compared and matched`.
* Every divergence in `port/spec/audio.md` carries a disposition: matched, or
  excluded by name with its cause.
* The cycle report records the disassembly evidence for each derived fact.

## 7. Residuals and honest limits

* If the C/capture write-count gap (9340 vs 6380) is voice-allocation rather
  than note setup, it **survives this cycle** and is recorded as a named
  residual with its cause — not hidden and not tuned away.
* Audibility remains unverified: this cycle changes register writes, and no
  human has confirmed the port's FM against the original by ear.
* The parallel-stream question (`prage_001.dro`) is in scope only as a second
  capture for the same comparison; if it needs its own analysis it is named.

## 8. Verification limits

Proof for this cycle is the write-stream comparison plus the derived TL cases.
It is **not** a claim that the port's FM matches the original's audio, that
voice allocation is faithful, or that any path the captures do not exercise is
correct. Each is named above rather than implied green.
