# Design: Primal Rage (DOS) → SDL3 port — sub-project 2a, AIL/Miles audio

Date: 2026-09-16
Status: approved design, pending user review of this document

Second cycle of the port. Sub-project 1 (engine core) is merged: the port boots,
runs the original frame loop and renders the title from real assets (see
`2026-09-16-engine-core-port-design.md` and
`../plans/2026-09-16-engine-core-port-report.md`).

This cycle covers **audio only**. Smacker video is sub-project **2b**, sequenced
after this one so that its soundtracks can use the layer built here.

## Context: what the shipped install says about audio

Verified from `data/game/C`:

* `DIG.INI` selects `DEVICE Creative Labs Sound Blaster 16 or AWE32` /
  `DRIVER SB16.DIG` — the **sample** path is 16-bit DMA.
* `MDI.INI` selects `DEVICE Creative Labs Sound Blaster(TM) 16` /
  `DRIVER SBPRO2.MDI` — the **music** path is **Sound Blaster Pro 2 = OPL FM**,
  not wavetable and not an external synth.
* `FAT.OPL` (and the same-shaped `FAT.AD`) are patch banks: a table of
  `(value, offset)` pairs at `0x400`, `0x44E`, `0x45C`, … stepping `0xE`.
* The full Miles/AIL set is present (332 KB): `*.DIG`, `*.MDI`, `RM.DRV`,
  `DIG.INI`, `MDI.INI`, plus variants for other hardware.
* **None of it is in `INDEX`** — `INDEX` holds only `.gra` entries, so sound
  files are read directly from the directory rather than through the resource
  manager. `S16SND2.GRA` and `S16RAD.GRA` exist and may hold sample data.

The original does not contain a sound engine: it loads **DOS driver binaries**
(`SB16.DIG`, `SBPRO2.MDI`) and calls them through the Miles AIL interface, using
real-mode/DPMI reflection (`int 21h`/`int 31h`, `swi(0x21)`/`swi(0x31)` in the
decompilation). **The port cannot run those binaries.** That boundary is the
central design fact of this sub-project.

## Section 1 — Scope

### Definition of done

1. **The game's own init path completes.** `0x10034`'s driver load and the AIL
   init (`0x1CF40`) run through the ported code and reach "audio ready" with no
   DOS driver involved. Observable, not implied: the port reports that state (and
   the tests can assert it) rather than merely failing to crash. Ported calls keep
   the original's signatures, ordering and return-value semantics.
2. **Title/attract music plays.** The sequencer reads the game's music data,
   loads `FAT.OPL` patches, drives the vendored OPL core, and reaches SDL audio.
   The mix rate is **documented** — the original's, if the gating/discovery work
   determines it, otherwise a stated choice — and the report says which it is.
3. **One announcer sample plays through the game's own call path** (`SB16.DIG`
   profile, 16-bit).
4. **Fidelity is proven, not asserted.** Our OPL register-write stream for a
   given piece of music matches the original's captured stream byte-for-byte
   under a stated pacing normalisation — or, if capture proves impossible, the
   byte-exact Python register-stream fallback in Section 4 governs and the trace
   is recorded as unachievable rather than quietly dropped.

### Gating discovery, before the FM design is committed

**Where the songs and samples actually live is not yet known.** `INDEX` holds
only `.gra` entries, so the music is either inside the CD's `RAGE.SND`, inside a
GRA (e.g. `S16SND2.GRA`), or embedded in the data object. The first plan task
settles it.

**If the music turns out to be Red Book / CD audio rather than a sequenced FM
track, the FM path is wrong for music** and this spec must be revised: the
sequencer and `FAT.OPL` would then be irrelevant to music (they would still
serve the sample path's needs). That outcome is a design change, not a detail,
and is called out here so it cannot be discovered late and quietly absorbed.

### Non-goals

* Smacker audio tracks (sub-project 2b).
* AWE32/wavetable, MPU401, MT32 and other external-synth paths — the shipped
  config selects SBPRO2 OPL, and the port targets that profile.
* General MIDI mapping, 3D/positional audio, CD-audio playback.

## Section 2 — Architecture

**The driver boundary.** The port reimplements (a) the **AIL API surface the game
calls** — functions around `0x5D7DC`/`0x5D87E`/`0x5D973`/`0x5DB9E` and the AIL
init at `0x1CF40` — preserving the original's call contract, and (b) beneath it,
the **driver behaviour** those calls imply (timer-paced sequencer, OPL register
writes, sample voice management), implemented against SDL audio. The game's
*calls* stay faithful; the driver's *internals* are ours. That is precisely why
the register-level oracle in Section 4 exists: it is what proves our internals
produce what the real driver produced.

**File structure.** A subtree, because this is genuinely more than one file:

```
port/src/platform/audio/ail.{c,h}        the AIL API surface the game calls, ported call-for-call
port/src/platform/audio/sequencer.{c,h}  music event stream -> OPL register writes, timer-paced
port/src/platform/audio/samples.{c,h}    SB16 16-bit sample load/format/voice model
port/src/platform/audio/mixer.{c,h}      mixes sample voices + OPL output into frames
port/src/platform/audio/opl/             vendored single-file OPL core + its licence
port/tests/test_sequencer.c              event stream vs the music data + register-write comparison
port/tests/test_samples.c                sample decode and bounds
```

**No hardware probing: the port presents a fixed profile.** The original probes
for a sound card and falls back through `SB16.DIG → SBPRO.DIG → SBLASTER.DIG`, and
its AdLib detection pokes real hardware. The port cannot and should not pretend to
detect anything: it presents the **SBPRO2 OPL music profile and the SB16 16-bit
sample profile** unconditionally, so the probe paths that would otherwise select
other hardware are short-circuited. Every such short-circuit is a deliberate
deviation marked `/* PORT: fixed audio profile, no hardware probe */`, and the
report lists them — the point is that the game's *later* calls behave exactly as
they would on the selected hardware, while the probing itself is removed.

**SDL stays confined to `host.c`/`main.c`**, as the branch's existing constraint
requires. The mixer therefore never touches SDL: `host.c` owns the audio device
and pulls mixed frames through a seam, exactly as `gfx_present` reaches the window
through `host_present_rgb`.

**Ownership.** One responsibility per file, and the vendored OPL core is wrapped so
no other module includes it: nothing outside `opl/` knows the core's API, and
nothing inside `opl/` knows about the game.

## Section 3 — Data flow

**Music.** AIL init → select the SBPRO2 profile → load `FAT.OPL` patches → read
the music data (location settled by the gating task) → `sequencer.c` parses it
into timed events (note on/off, patch/program change, controllers, tempo) → on
each tick the sequencer writes OPL registers → the vendored core renders at the
chip's rate → `mixer.c` → SDL device.

**Samples.** The game calls the sample-play API with a handle/length →
`samples.c` reads SB16-style 16-bit PCM, taking rate and channel count from the
sample's header or the play call → `mixer.c` mixes the voices with the OPL
output.

**Pacing is load-bearing.** The original drives music from a timer. AIL's tick
rate may not equal the game's 60 Hz tick, so **the sequencer is paced at the rate
the original's driver used**; determining that rate is itself a discovery item,
because the trace comparison in Section 4 depends on it.

**Resampling is a playback choice, not a fidelity claim.** The OPL core renders
at its native rate and its output is resampled to whatever the SDL device wants.
Because fidelity is proven at the **register** level, resampling cannot flatter
or weaken the claim.

## Section 4 — The oracle, and its feasibility risk

**Primary, if capturable.** Capture the **original's OPL register-write stream**
for a given piece of music from DOSBox-X, and require our sequencer's stream to
match byte-for-byte under a stated normalisation: the **ordered write sequence and
its per-tick grouping** are compared, not wall-clock timestamps, because the two
runtimes pace differently.

**Feasibility risk, stated plainly.** The capture mechanism is **unproven**. The
controller checked: `dosbox-x --help` exposes `-opencaptures` but no OPL logging
option, and Task 13 already found this build's debugger refuses a scriptable
session. So the **first plan task is a spike** — determine whether writes to the
OPL ports (`0x388`/`0x389`) can be logged, by any of: a capture option in the
`[capture]` section, the debugger's I/O logging, or the `-opencaptures` route.
The spike reports yes or no with evidence. If no route works, it says so and the
fallback governs.

**Verified fallback, and it is not weak.** An independent **Python
OPL-sequencer**, `tools/opl_seq.py`, decodes the same music data to a
register-write stream, and the C sequencer must produce a byte-identical stream.
This is the same shape as `tools/gra_render.py` for the `.GRA` decoders: it
verifies the **data path** byte-exactly, and the vendored OPL core is trusted
separately as a known-good emulator. The following are explicitly **not**
acceptable substitutes: comparing waveforms, spectrum matching, or any tolerance
tuned until it passes.

**What each comparison proves.** The register-stream comparison proves the port
*plays what the original played* given the same data. It does not prove the
vendored core's synthesis is bit-identical to real hardware, and the spec does not
claim that.

## Section 5 — Verification

Cheapest sufficient proof, in order:

1. Each file compiles warning-free
   (`-Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing`).
2. The existing suite still passes, and `--check` stays headless and deterministic.
3. **Sequencer:** its event stream and register writes match the Python
   OPL-sequencer byte-for-byte for at least one complete piece of music, and to
   the captured original trace if the spike succeeded.
4. **Samples:** decode and bounds assertions (a sample longer than the buffer,
   an unknown format, a zero-length sample), plus one sample reaching the mixer
   through the game's own call path.
5. **No audio in the test process:** the suite must not open an audio device, so
   the mixer is tested by rendering into a buffer, not by playing.
6. Deviations marked `/* PORT: ... */`, doubts marked `/* TODO(verify): ... */`
   and listed in the sub-project report.

## Section 6 — Risks

| # | Risk | Mitigation |
|---|---|---|
| 1 | The music may be Red Book/CD audio, making the FM sequencer irrelevant to music. | The gating discovery task runs before the FM design is committed; if it lands that way, this spec is revised rather than quietly stretched. |
| 2 | The OPL register trace may be uncapturable on this build. | Spike first; the byte-exact Python register-stream fallback was accepted in advance, and the trace is recorded as unachievable rather than dropped. |
| 3 | The driver's timer rate may be unknown, making pacing wrong and the trace comparison misleading. | Determine the rate as an explicit discovery item; compare per-tick grouping, not wall-clock, so a wrong absolute rate cannot hide. |
| 4 | Vendoring an OPL core is a new dependency with a licence to honour. | Single file, kept private behind a wrapper, licence preserved and noted in the sub-project report and `README.md`. |
| 5 | Sample data may live on the CD (`RAGE.SND`) rather than in `data/game/C`. | The gating task checks the CD image too; the ISO is already present at `data/game/CD/RAGECD.ISO`. |
| 6 | The original's AIL surface may be larger than the functions identified so far. | Enumerate the game's calls into the driver from the decompilation and call graph as a task, and port call-for-call; stubs must be marked, never silent. |

## Section 7 — Docs this cycle updates

* `port/spec/audio.md` — new subsystem spec: the AIL surface, the sequencer's
  event grammar, the OPL patch loading, the sample format, the measured timer
  rate, and whatever the gating discovery finds about where the data lives.
* `FORMATS.md` — the music and sample container formats, and the `FAT.OPL` patch
  bank layout, once decoded.
* `README.md` — the port's audio status, how to run with sound, and the vendored
  core's licence note.
* `docs/superpowers/plans/2026-09-16-engine-core-port-report.md`'s handoff
  section — the sub-project 3 entry is superseded by this cycle.
