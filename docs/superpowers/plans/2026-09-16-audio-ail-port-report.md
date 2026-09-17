# Sub-project report — Primal Rage (DOS) → SDL3, sub-project 2a: AIL/Miles audio

Date: 2026-09-17
Branch: `audio-ail`
Base commit: `4cf3b9f` ("docs: implementation plan for sub-project 2a")
Cycle commits: 27, `4cf3b9f..588a2de`, plus the final documentation commit that
adds this report.
Spec: `docs/superpowers/specs/2026-09-16-audio-ail-design.md`
Plan: `docs/superpowers/plans/2026-09-16-audio-ail-port.md`
Ledger: `.superpowers/sdd/2026-09-16-audio-ail-port/progress.md`
Supersedes: sub-project 3 (audio) in
`docs/superpowers/plans/2026-09-16-engine-core-port-report.md` §4.

## 1. Summary

The port now runs the game's own audio path with no DOS driver involved: the
init chain's AIL calls (`0x1CF40`) complete, the title/attract XMIDI bank is
decoded and sequenced into OPL register writes through a vendored FM core, one
located announcer sample is played through the game's own sample request/play
split, and mixed stereo frames reach an SDL audio device when one opens. On this
machine no device opens, so the windowed run is **silent** (§8.2).

What runs:

```
game_init (0x1BEC4)
  -> game_audio_init (0x1CF40): mixer_reset, AIL_startup, prefs (4=4,
     1=11025, 3=0x14, 0xb=1), install DIG (fixed profile), 4x
     AIL_allocate_sample_handle + AIL_init_sample, install MDI (fixed),
     AIL_allocate_sequence_handle, register/start the 60 Hz timer slot
  -> res_load_index, surface_setup, palette_list_init, game_state_init
game_loop (0x255CC)
  -> ... gfx_present, swap_buffers
  -> game_audio_service (0x1CF20): play queued samples (0x1CB18), start pending
     song (0x1C930), seq_tick x2 per measured host tick, mixer_render +
     host_audio_submit
title state (0x121A0) first entry: requests the S16TITLE bank and queues the
  located S16SOUND sample; the frame service plays/starts them
main.c run_windowed: host_audio_open(MIXER_OPL_RATE, 2) with the window
```

`--check N` remains headless: no window and no audio device are opened; it
asserts the announcer became a live voice rendering non-silence, that the
sequencer ticked without outrunning the host clock, and that a run past XMIDI
tick 59 keyed a note.

## 2. Verification ladder (run 2026-09-17, `audio-ail` @ `588a2de`)

```bash
cmake -S port -B build && cmake --build build
PR_ORACLE_REQUIRED=1 ./build/run_tests
PR_ORACLE_REQUIRED=1 ./build/prageport --game-dir data/game/C --check 120
python3 tools/gen_symbols.py port/decomp port/src/symbols.h   # must stay byte-identical
```

Observed:

```
$ cmake -S port -B build && cmake --build build
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /Users/felipe.dos.santos/code/mine/primal-rage-reverse/build
[  2%] Built target symbols
[ 47%] Built target prage_core
[ 52%] Built target prageport
[100%] Built target run_tests
exit 0

$ PR_ORACLE_REQUIRED=1 ./build/run_tests
oracle C-vs-Python: 9340 writes byte-exact
capture oracle first difference at C write 2: C tick=60 reg=0x20 val=0000 vs capture tick=60 reg=0xb0 val=0x2b (C 9340 writes, capture 6380 normalised)
all checks passed
exit 0

$ PR_ORACLE_REQUIRED=1 ./build/prageport --game-dir data/game/C --check 120
prageport 0.0.1 game-dir=data/game/C --check 120 (headless)
exit=0

$ python3 tools/gen_symbols.py port/decomp port/src/symbols.h
1304 globals, 1206 functions -> port/src/symbols.h
  dropped 11 globals, 1 functions outside the LE objects
exit=0
# git diff --exit-code -- port/src/symbols.h: no diff (byte-identical)
```

| Step | Result |
|---|---|
| configure + build (cached) | exit 0, no output beyond the target list |
| clean out-of-tree build (`/tmp/prbuild-audio`, removed after) | exit 0, `grep -ciE 'warning\|error'` = **0** |
| `PR_ORACLE_REQUIRED=1 ./build/run_tests` | `all checks passed`, exit 0; the C-vs-Python oracle line and the (informational) capture first-difference line |
| `PR_ORACLE_REQUIRED=1 ./build/prageport --game-dir data/game/C --check 120` | header line, exit 0 |
| `python3 tools/gen_symbols.py port/decomp port/src/symbols.h` | `1304 globals, 1206 functions`, `dropped 11 globals, 1 functions`, exit 0 |
| `git diff --exit-code -- port/src/symbols.h` | no diff (header byte-identical) |

`PR_ORACLE_REQUIRED=1` is mandatory for a real verification run. The governing
C-vs-Python gate reads the untracked `data/game/C` assets (`S16TITLE.GRA`,
`FAT.OPL`) and the untracked `.dro` capture; without the variable or the assets
the test prints a SKIP line and the suite still passes, so a green run there
does **not** exercise the oracle. The `--check 120` run writes `frame_*.ppm`,
`*.pal` and `*.idx` into the repo root; they are git-ignored (`*.ppm`, `*.pal`,
`*.idx`) and none is committed. No `.dro` capture is committed (`*.dro` ignored).

## 3. Definition of done — status, stated to scope

The spec's DoD (`2026-09-16-audio-ail-design.md` §1):

### DoD 1 — the game's own init path completes: **met.**

`game_audio_init` (`flow.c:284`) runs the original `0x1CF40` calls in order
through the ported AIL surface and reaches "audio ready" with the fixed
SBPRO2-FM + SB16-sample profile; no DOS driver is loaded. Observable: the
`--check`/test path runs the init and the audio service, and the suite asserts
the sequence loaded and the sample became a voice. Ported calls keep the
original signatures and return-value meanings (`AIL_sequence_status` 4/2,
allocation 0).

### DoD 2 — title/attract music plays: **met at the register/data level; not heard.**

The title bank is located in `S16TITLE.GRA` (first `FORM`/`XMID` container),
decoded, sequenced and written to the OPL core; the frame service also renders
and, when a device is open, submits it. The mix rate is **documented and
derived, not chosen**: `MIXER_OPL_RATE = 49716` (`mixer.h:48`), the vendored
core's native rate, so the FM path needs no resampling at the device; the
device is opened at that rate (`main.c:28`). The XMIDI tick is **120 Hz**
(`SEQ_TICK_MS = 1000/120`, `sequencer.h:31`), measured from the capture, so the
60 Hz frame loop drives two sequencer ticks per host tick.

### DoD 3 — one announcer sample plays through the game's own call path: **met, with a scope limit.**

The title state's first entry queues the located `S16SOUND.GRA` RIFF/WAVE blob
(`game_sample_request`, `flow.c:212`) and the master-loop service plays it
through `AIL_init_sample` → address/volume/rate/type/loop → `AIL_start_sample`
in the original's order (`game_sample_play`, `flow.c:383`). The headless proof
is that a mixer voice became active **and** the mixer rendered non-silence while
it was (`main.c:117`, `test_flow.c`), not merely that a function ran. **The
scope limit:** the original's sample-id selection goes through a runtime-only
table (`DAT_000bbdc8`, zero in `PRAGE.EXE`, populated at run time), which was not
resolved statically. The port therefore binds the one Task-1-located blob
directly. Which sample the original would pick at that moment is **not proven**
— the port proves "a located sample plays through the game's own call path",
which is what DoD 3 asks, not "the announcer the original would pick".

### DoD 4 — fidelity is proven, not asserted: **partially; stated precisely below.**

**What is proven.** The music data's **decode and transcription** are proven
byte-exactly: the C sequencer's OPL register stream equals a separate Python
implementation's stream over the whole title bank — **9340 writes, zero
differences** in `(tick, reg, value, order)` (`tools/opl_seq.py`; run_tests
governs it under `PR_ORACLE_REQUIRED=1`). Two anchors are additionally derived
from the captured original: note 84 → block 5 fnum `0x2B2` at capture ms 1624
and note 79 → block 5 fnum `0x205` at ms 4100 (`sequencer.c:30`,
`tools/opl_seq.py --capture-anchors`), and the FAT.OPL patch byte layout for
program `0x7A` matches the capture (`FORMATS.md`). The register trace **was**
captured (Task 2), so the captured stream governs the comparison; the Python
fallback is not the sole bar.

**What is NOT proven.** The **reconstruction semantics** are the port's own
design, not the driver's: voice allocation and steal policy, the tick-decrement
rule, whole-patch re-application per key-on, and write ordering. They are
**not verified against the original**, because the captured original's stream
diverges **structurally**: the first difference is at normalised write 2, where
the port emits `0x20` and the capture emits `0xB0` (`run_tests` line above).
Matching further would require reproducing the DOS driver's patch/channel state
machine, which the cycle did not do. Divergences 1 and 4–7 in
`port/spec/audio.md` ("Known capture divergences") are each excluded or
reported, never tuned away. One capture divergence remains **open** by name:
**carrier total-level velocity attenuation**. A dominant formula
`p[10] + 0x16 + ((127 - velocity) >> 3)` fits the `[10] = 0x00`/`0x40` patches
but others are off by ±1 (patch `0x34` is +1, patch `0x74` is −1), so the port
writes `p[10]` verbatim (`sequencer.c:110-115`). Settling it needs disassembly
of `SBPRO2.MDI`'s velocity→TL path or a single-program velocity sweep.

**Inherent ceiling.** Two implementations of the **same recorded format** cannot
catch a shared misinterpretation (chunk-offset parity, VLQ rule and patch stride
are identical in both). Mitigations: the computed note table (0/128 mismatches),
the two capture-derived anchors, and the FAT.OPL byte-for-byte capture match.

The vendored core's **synthesis** is not claimed bit-identical to real hardware.
Fidelity is claimed at the **register** level only, as the spec allows.

## 4. Modules

`/` = ported and verified; `(stub SPn)` = deferred to sub-project n.

### `platform/audio/opl/` — vendored FM core + private wrapper

`opl_reset` / `opl_write(u16 reg, u8 value)` / `opl_render(s16*, u32)`
(`opl.c`), accepting the full OPL3 register range `0x000–0x1FF`. The core's
`opal.h` is reachable only from `opl.c`; nothing outside `opl/` knows the core.
Determinism is a tested property.

Verified (`test_opl.c`): silence after reset with no writes (exact zeros);
a key-on sequence produces non-silence; identical write sequences produce
byte-identical buffers; and the `0xC0`-dependent behaviour (§6).

`/* PORT: */`: the chip instance `g_opl` is one process-wide value, deliberately
not `static`, because the wrapper's interface has no handle and the original
owns exactly one FM chip (`opl.c:3`).

### `host.c` — audio seam (SDL confined)

`host_audio_open(rate, channels)` / `host_audio_close` / `host_audio_submit` /
`host_audio_rate` / `host_audio_error` (`host.h:55-75`). The seam owns the only
SDL audio device; it opens the default playback device at the caller's
rate/channels, resumes it (SDL leaves the stream paused), and tears down on any
failure; every entry is guarded so the suite and `--check` need no device. SDL
still appears only in `host.c`/`main.c`.

Verified (`test_host.c`): with no device `host_audio_submit` is a safe no-op;
`host_audio_open` with an impossible profile returns 0 without aborting;
`host_audio_close` is idempotent; the suite opens no device.

`/* PORT: fixed audio profile, no hardware probe. */` (`host.c:221`);
`/* TODO(verify): */` the failure branches (NULL stream, failed resume on a
host with no device) are not exercised by the suite (`host.c:224`). Pre-existing
`/* TODO(verify): */` the tick interrupt vector (`host.c:26`) is unchanged.

### `platform/audio/mixer.h/.c` — software mixer

**PORT: the whole subsystem is invented** (`mixer.h:4`): the original mixed
sample voices in SB16 hardware and drove a separate OPL chip; the port replaces
both with one software mixer. Three public entry points plus reset and a
read-only voice count. Arithmetic: per-voice Q8 volume (256 = unity, clamped to
`[0,1024]`), nearest-neighbour resampling for every source off a 16.16 phase
accumulator, `s32` accumulation saturated to `s16` (no wrap). PCM is
referenced, never copied; `owner` identifies the AIL handle so
`mixer_stop_sample(owner)` is stale-safe. Voice pool: four, matching the four
AIL sample handles the original allocates.

Verified (`test_mixer.c`): silence, one voice, two louder than one, stop,
saturation (bounds test proven falsifiable by stripping the clamp — 3 failures,
experiment reverted), and rate-different resampling stays in bounds.

`/* PORT: */`: the invented mixer, and `mixer_active_voices` as a check-only
observable. `/* TODO(verify): */` voice-exhaustion policy (drop/steal/error) is
not established (`mixer.h:15`, `mixer.c:15`); `MIXER_OPL_RATE` must track the
core's rate (`mixer.h:46`).

### `platform/audio/samples.h/.c` — sample decode

`samples_load` walks the RIFF container chunk by chunk (bounded by the buffer;
the RIFF size field is deliberately not trusted), accepts exactly the shipped
shape (WAVE_FORMAT_PCM, mono, 8-bit unsigned), and rejects anything else,
zeroing `*out` on failure. `samples_to_s16` is the port's single 8-bit-unsigned
→ s16 conversion.

Verified (`test_samples.c`) against the real blob
(`S16SOUND.GRA@0x24b13`): tag 1, 1 channel, 11025 Hz, 8-bit, 19327 frames;
all 19370 prefix lengths swept with zero accepted short prefixes; truncated /
unknown-format / zero-length rejected; no binary blob committed.

`/* PORT: */` parsing the RIFF container is a port invention — the original
loaded samples through the driver, which owned the container (`samples.h:10`).

### `platform/audio/patches.h/.c` — FAT.OPL patch bank

`patches_load` / `patches_count` / `patches_lookup`, data-in and no I/O. Decodes
the `(u16 key, u32 offset)` table terminated by `0xFFFF`, then copies each
14-byte payload after validating that the offset is inside the buffer and does
not overlap the table. Verified: 181 records; the decoded payload for program
`0x7A` matches the capture byte-for-byte (driver transforms excepted).

### `platform/audio/sequencer.h/.c` — XMIDI → OPL register writes

`seq_bank_size` / `seq_load` / `seq_start` / `seq_stop` / `seq_tick` /
`seq_active_track` / `seq_playing`. Decodes the `EVNT` stream per the grammar in
`port/spec/audio.md` (single-byte delta, explicit status, `0x9n` note/vel + VLQ
duration, meta `0xFF`, sysex `0xF0`/`0xF7`), maps programs through FAT.OPL
(`(bank << 8) | program`; channel 9 → `0x7F00 | note`), and owns a fixed
9-channel OPL voice pool. The note→`{block, fnum}` table is computed for the
49716 Hz clock and pinned to the capture anchors.

Verified: the C-vs-Python byte-exact gate (9340 writes), the two capture
anchors, and `test_sequencer.c` (load/parse, tick advance, stuck-note and
booking checks).

`/* PORT: */` the driver stand-in itself: nine voices, **steal the oldest** on
exhaustion (`sequencer.c:168`); the original's policy is not established.
`/* TODO(verify): */` the `RBRN` loop range is not reproduced
(`sequencer.c:235`) — playback stops at the end meta; and the MDI driver's
declared rate at offset `+0x2e` (`sequencer.h:22`) — 120 Hz is behavioural
evidence from the capture, not that field. The port's known capture divergences
are 1, 4, 5, 6 and 7 in `port/spec/audio.md`; divergence 2 (the `0x105` claim)
is withdrawn and corrected (§6).

### `platform/audio/ail.h/.c` — the AIL call surface

One C function per game→AIL call in `port/spec/audio.md` "AIL surface (Task 3)":
all 33 AIL thunks (`0x5d851–0x5dfeb`), with the original address in each header
comment as the durable identity. State is port-only bookkeeping, so it is
`static` (`ail.c`); the original kept it inside the AIL engine, which cannot
run here.

Two PORT rules hold across the file: **no file I/O or asset resolution**
(`ail.h:19` — the caller resolves `DIG.INI`/`MDI.INI`/`FAT.OPL`/the XMI bank and
passes bytes/handles in), and **fixed audio profile, no hardware probe**
(`ail.h:25`). Verified (`test_ail.c`): the fixed-profile installs succeed, the
handle pools allocate and release, `AIL_init_sequence` derives the bank length,
and the sequence status codes match.

### `game/flow.c` / `flow.h` — the init chain's audio wiring

`game_audio_init` (`0x1CF40`), `title_music_start` (`0x1C930`),
`game_sample_play` (`0x1CB18`), `game_audio_service` (`0x1CF20`),
`game_music_bank_find` (the FORM/XMID scan with a wrap-free size guard), and the
`0x1D018` teardown via `game_shutdown`. Pacing derives both the sequencer tick
count and the rendered frame count from one measured host-tick delta clamped to
`HOST_TICK_MAX_CATCHUP`, so music and game share one clock policy; render is
bounded to `AUDIO_FRAMES_MAX`.

Verified (`test_flow.c`, `--check 120`): the audio init runs in the original's
init slot; the title bank loads and keys notes; the sample becomes a voice
rendering non-silence; the sequencer does not outrun the host clock; the
bank-size guard is negative-controlled.

`/* PORT: */` audio state is port-only and does not live in `mem[]`
(`flow.c:79`); the title binds the `S16TITLE` bank directly and the announcer
binds the located blob, because the runtime sound table `DAT_000bbdc8` is not
extracted (`flow.c:89`, `flow.c:323`); `0x1D0BC`'s init-time buffers are
replaced by referencing resource bytes plus a per-handle conversion buffer;
master SFX volume defaults to the shipped `0x7f` (`flow.c:293`);
`/* TODO(verify): */` `FUN_0001cf40`'s DIG/MDI param gating (`flow.c:286`).

### `main.c` — device and headless probe

The device is opened in `run_windowed` only (`MIXER_OPL_RATE`, stereo), so
`--check` never opens one. `probe_announcer_audio` (`main.c:117`) asserts the
announcer's voice and non-silence before the first FM note (frame 5).

## 5. `/* PORT: */` deviations (audio cycle)

The AIL surface's central deviations:

* **Fixed audio profile, no hardware probe** — the three install calls:
  `AIL_install_DIG_INI` (`0x5db7c`), `AIL_install_DIG_driver_file`
  (`0x5db9e`), `AIL_install_MDI_INI` (`0x5ddd0`) short-circuit `DIG.INI`/`MDI.INI`
  and the `SB16.DIG → SBPRO.DIG → SBLASTER.DIG` fallback; the first install
  succeeds, so later calls behave as on the shipped hardware.
* **The real-mode/DPMI crossing is gone** — in the original every call reached
  the loaded `.DIG`/`.MDI` through AIL's dispatcher `0x5d973` (`swi 0x31`); the
  port calls sequencer/samples/mixer directly (`ail.c:9`). `AIL_start_sample`
  (`0x5dc70`) is the clearest replacement: driver call `0x401` (DMA start)
  becomes `samples_to_s16()` + `mixer_add_sample()` (`ail.c:281`).
* `AIL_start_timer` (`0x5daa6`): no PIT/ISR; the callback is stored, never
  fired; the frame loop paces (`ail.c:145`).
* `AIL_init_sequence` (`0x5de48`): the third argument keeps the original's
  meaning (the sequence number, passed as 0); the bank length is derived from
  the container (`ail.c:401`).
* `AIL_set_sequence_volume` (`0x5deca`): the value is recorded, not applied;
  there is no master-volume stage and the fade is not modelled
  (`ail.c:435`).
* Sample volume `0..0x7f` maps to the mixer's Q8 with `0x7f` = unity 256; the
  DOS driver scaled in hardware (`ail.c:293`).
* `AIL_start_sample` pre-stops the handle's prior voice (per-handle stop
  semantics; one handle = one DMA voice).

Deferred stubs (owner: **sub-project 2b**, Smacker video streaming), each
safe-inert and marked in `ail.h`:

| address | port function | returns |
|---|---|---|
| `0x5dd2c` | `AIL_sample_buffer_size` | `0` |
| `0x5dd5d` | `AIL_stream_buffer_index` | `-1` (idle) |
| `0x5dd86` | `AIL_stream_feed` | no-op |
| `0x5ddad` | `AIL_register_sample_callback` | no-op |

Other audio-cycle PORT markers: `mixer.h:4` (whole mixer invented),
`mixer.h:43`/`mixer.c` rate mirror, `mixer.h:71` check-only observable,
`samples.h:10` RIFF parsing invented, `sequencer.c:168` oldest-steal,
`opl.c:3` non-static chip instance, `host.c:221` fixed profile,
`flow.c:79/89/235/279/293/323/372`, `flow.h:55/59`, `main.c:138`.

## 6. The `0x105` register story — corrected

The cycle first recorded that writing the OPL3-mode register `0x105 = 0x01`
**silenced** the vendored core. **That claim is false.** Root cause: the probe
behind it omitted the driver's `0xC0 = patch | 0x30` output-enable write. In
OPL2 mode the core's `channelMix` forces every channel's output enable on,
masking a missing `0xC0`; in OPL3 mode the `0xC0` bits gate the mix, so the
`0xC0`-less probe rendered zero. With the `0xC0` write present — as every real
note setup has — `0x105 = 0x01` is output-neutral and byte-identical. The port
now writes `0x105 = 0x01` after `0x01 = 0x20`, matching the capture; the core
is unmodified. The withdrawal and correction are recorded in
`port/spec/audio.md` ("Known capture divergences", item 2), and
`port/tests/test_opl.c` pins the `0xC0`-dependent behaviour (non-silent and
byte-identical with and without `0x105` when `0xC0` is written). Task 4's three
original assertions remain intact.

## 7. Vendored core — origin, version, licence

* **opal 2.0.3**, MIT (C11 port), `https://github.com/RealBitdancer/opal`
  commit `7e829f3334e2a0b6aadc370c28c66acc83032d42`.
* Synthesis core by **Shayde/Reality** (Reality Adlib Tracker 2), **public
  domain**.
* Licence text preserved in-tree at
  `port/src/platform/audio/opl/LICENSE.opal.txt` (identical to upstream).
* The vendored source files are **byte-identical to upstream** apart from an
  8-line provenance banner each (stripped for the comparison), a property worth
  more than fixing their comment paths. Their banners cite upstream-relative
  paths (`../LICENSE.opal.txt`, `../../LICENSE.opal.txt`) and a
  `THIRD_PARTY_LICENSES.md` that did not exist in this tree. Those cite paths do
  not resolve in this tree; the accurate in-tree path is recorded here, in
  `opl.h:6`, and in a new root `THIRD_PARTY_LICENSES.md`. The vendored files
  were deliberately not edited.
* The core's accuracy against real hardware is **unvalidated**; the register
  diff is the fidelity gate, as above.

## 8. The oracle

### 8.1 The capture was obtained; it governs

Task 2 proved the DOSBox-X route and captured the original: `DX-CAPTURE /O`
produces a DBRAWOPL `.dro`; `tools/opl_trace.py` decodes it to a normalised
`(tick_ms, reg, value)` stream. `prage_000.dro` holds 7201 FM register writes
over 28117 ms and `prage_001.dro` 1052 over 9433 ms. The port's title bank halts
at tick 3434 = 28117 ms after the first note-on, so the capture spans the title
bank end to end. **The captured original stream governs the comparison**; the
byte-exact Python fallback is not the sole acceptance bar.

### 8.2 Audibility was never verified

This environment has a CoreAudio device, but `AudioQueueStart` fails with
**`-66681`** at 49716, 44100 and 48000 Hz, so the windowed run is **silent**.
With `SDL_AUDIODRIVER=dummy` the seam opens at 49716 and the path (open →
render → submit) runs clean. The path is live and exercised; **no human has
heard the port's audio**, and this report does not claim otherwise.

### 8.3 What governs vs what diverges

* Governing, tolerance-free: **C vs the Python implementation, byte-exact,
  9340 writes.** That implementation is a separate second implementation with a
  computed note table; it is not a transliteration, but it implements the same
  recorded grammar, so it cannot catch a shared misinterpretation (§3).
* Against the capture: the port is **not** byte-exact. Excluded/reported
  divergences 1 (carrier TL velocity) and 4–7 (init block; per-note patch
  re-application/channel reuse; percussion note mapping; `0xBD` not written).
  The first difference is structural at normalised write 2. None is tuned away.
* Asset-gated: both comparisons need the untracked assets and `.dro`, so
  `PR_ORACLE_REQUIRED=1` is required for a meaningful run.

## 9. Unresolved risks

1. **Reconstruction semantics are unverified against the original.** Voice
   allocation/steal, tick decrement, whole-patch re-application per key-on, and
   write ordering are the port's design; the capture diverges structurally.
2. **Audibility is unverified** (§8.2).
3. **Carrier TL velocity attenuation** remains open (±1 residual); percussion
   note→fnum mapping and the `RBRN` loop are un-modelled.
4. **Original sample-id selection is unproven** — the runtime table
   `DAT_000bbdc8` was not resolved; the port binds one verified blob.
5. **Vendored core synthesis is unvalidated vs hardware** — register-level
   fidelity only.
6. **Voice-exhaustion policy is a guess** (drop for samples, steal-oldest for
   FM); the original's is not established.
7. **Sample data shape.** The shipped blob is 8-bit unsigned although the
   selected profile is `SB16.DIG` (16-bit DMA); only one RIFF/WAVE blob exists
   in `data/game/C` — other in-play sound effects are likely headerless AIL
   blocks and were not located (`port/spec/audio.md` "Samples",
   `TODO(verify)`).
8. **`seq_bank_size` trusts the declared FORM size.** The production path is
   guarded (`game_music_bank_find` rejects an oversized bank before
   `AIL_init_sequence`), so this is only reachable with a corrupt asset; the
   AIL entry itself is not defensively bounded.
9. **Asset-gated tests** — a fresh checkout cannot run the governing oracle
   without `data/game/C`.

## 10. Deferred minors — disposition

Recorded in the ledger; none blocks this cycle.

* **Task 1 doc minors** (audio.md wording at `:224`; elided scan commands at
  `:113`; stated-but-not-shown outputs at `:138/:199/:276`): documentation
  completeness, non-blocking.
* **Task 4 (`g_opl` non-static)**: accepted. It is port-only bookkeeping with
  no `mem[]` counterpart and no handle in the wrapper interface; the PORT
  comment argues the deviation (`opl.c:3`).
* **Task 4 (vendored banner paths)**: resolved by adding the root
  `THIRD_PARTY_LICENSES.md` and leaving the files byte-identical (§7).
* **Task 5 (deferred-pacing `TODO(verify)` marker)**: disposed — the audio
  service now derives tick and frame counts from one clamped host-tick delta and
  bounds the render, which is the substantive concern.
* **Task 6 (mixer pool citation at `mixer.c:15` restated weakly)**: the full
  citation lives in `mixer.h`.
* **Task 7 minors** (zero-length test uses NULL; `channels` is `int`; duplicate
  chunks unspecified): harmless for the real blob.
* **Task 8 minor** (spec `:948-961` probe numbers whose standalone source was
  deleted): the assertion is reproducible via `run_tests`.
* **Task 8 minor** (task-8 report stale false `0x105` claim): `.superpowers` is
  git-ignored; the correction is in the spec and this report.
* **Task 9 minor** (anchor provenance only in `tools/opl_seq.py`): re-derivable
  with `--capture-anchors`.
* **Task 10 minor** (`seq_bank_size` unclamped): production-guarded (§9.8).
* **Task 11 minor** (`--check` bound would not catch a sub-2× tick regression):
  `test_flow.c`'s controlled tick-growth assertion covers it.
* **Task 12 minors** (loop-count gate unmodelled; non-silence attribution;
  drifted file:line cites): low risk; the sample is one-shot in the shipped
  data.
* **Whole-branch deferred minor**: `0x5d7dc` is **not audio** — it is the
  Watcom `rand()` LCG (33 game callers, no AIL caller). Game code/determinism,
  not this cycle (`port/spec/audio.md` "AIL surface" correction).

## 11. Honesty statement

The project may claim:

* the AIL surface's 33 calls are ported one-for-one with the original's
  call-site signatures and return-value meanings, with the fixed profile and no
  DOS driver;
* the music data's decode and transcription are proven byte-exactly against a
  separate Python implementation (9340 register writes, zero differences), with
  two capture-derived anchors and the FAT.OPL capture match as independent
  evidence;
* the register trace was captured, so the captured original stream governs the
  comparison;
* one located sample plays through the game's own request/play call path, with a
  headless voice + non-silence proof.

The project may **not** claim:

* that the port's reconstruction of the driver (voice alloc/steal, tick
  decrement, per-key-on patch re-application, write ordering) matches the
  original — the capture diverges structurally and only the data path was
  proven;
* that the port's audio was heard — no device opens in this environment
  (`AudioQueueStart` `-66681`);
* that the port plays "the announcer the original would pick" — the original's
  sample-id table is runtime-only and was not extracted;
* synthesis bit-fidelity to real OPL hardware.

## 12. Final status

Sub-project 2a's DoD is met as stated in §3: DoD 1 fully; DoD 2 at the
data/register level with audibility unverified; DoD 3 for a located sample with
the original-pick limit; DoD 4 for decode/transcription with the reconstruction
explicitly unproven. Everything on the audio path is either ported and verified
or stubbed and marked. Smacker audio streaming (the four `0x5dd*` stubs) is
sub-project 2b; menus/EEPROM and the fight engine remain sub-projects 4–5.
