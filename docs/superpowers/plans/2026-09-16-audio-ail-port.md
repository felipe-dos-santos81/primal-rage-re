# Audio (AIL/Miles) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reimplement the Miles/AIL audio surface the game calls, on top of SDL audio and a vendored OPL core, so the original's title/attract music and an announcer sample play through the game's own call path.

**Architecture:** The original loads DOS driver binaries (`SB16.DIG`, `SBPRO2.MDI`) and calls them through AIL. The port cannot run those, so it reimplements (a) the AIL call surface the game uses, preserving the original's contract, and (b) the driver behaviour beneath it — a timer-paced sequencer emitting OPL register writes, and 16-bit sample voices — mixed in software and handed to SDL through a host seam. The port presents a **fixed** audio profile; it never probes hardware.

**Tech Stack:** C11, CMake ≥ 3.20, SDL3 (audio device, owned by `host.c` only), a vendored single-file OPL core, Python 3 for the independent register-stream oracle, DOSBox-X for ground truth.

## Global Constraints

- C11. Every file compiles warning-free with `-Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing`.
- SDL appears **only** in `src/host.c` and `src/main.c`. The mixer, sequencer, samples and AIL layers must not include an SDL header; they reach audio through the host seam.
- The **vendored OPL core is private**: only files under `port/src/platform/audio/opl/` may include its header, and its licence is preserved in that directory and noted in `README.md` and the sub-project report.
- **No hardware probing.** The port presents the SBPRO2 OPL music profile and the SB16 16-bit sample profile unconditionally. Every short-circuit of the original's probe/fallback path is marked `/* PORT: fixed audio profile, no hardware probe */` and listed in the report.
- **The test suite must not open an audio device.** Audio is verified by rendering into buffers; the SDL device is exercised only by the binary, never by `run_tests`.
- Fidelity is proven at the **register level**. Comparing waveforms or spectra, or any tolerance tuned until it passes, is **not** an acceptable substitution — if the trace cannot be captured, the byte-exact Python register-stream oracle governs and the trace is recorded as unachievable.
- Integer widths `u8/s8/u16/s16/u32/s32` exactly as the original register widths; cast at every truncating step.
- Original state lives in `mem[]`; do not shadow it in long-lived C globals. Audio bookkeeping that has no original counterpart may be `static` (`port/PORTING.md` permits it explicitly).
- Assets under `data/game/C/` are read-only and never committed; `*.ppm`/`*.pal`/`*.idx`/`*.wav`/`*.mem` and every generated oracle are git-ignored.
- Deviations marked `/* PORT: ... */`; doubts marked `/* TODO(verify): ... */` and listed in the task report.

---

## File Structure

| File | Responsibility |
|---|---|
| `port/src/platform/audio/ail.h/.c` | the AIL call surface the game uses, ported call-for-call; the fixed-profile decisions live here |
| `port/src/platform/audio/sequencer.h/.c` | music data → timed events → OPL register writes, timer-paced |
| `port/src/platform/audio/samples.h/.c` | SB16 16-bit PCM loading, format handling, voice model |
| `port/src/platform/audio/mixer.h/.c` | mixes sample voices and the OPL core's output into frames; touches no SDL |
| `port/src/platform/audio/opl/` | the vendored OPL core, single file plus its licence, plus a thin wrapper header |
| `port/src/host.h/.c` | gains the audio device seam (`host_audio_open/close/submit`) — the only SDL audio code |
| `port/src/platform/audio/patches.h/.c` | `FAT.OPL` patch-bank loading into the OPL core's instrument state |
| `tools/opl_seq.py` | independent Python sequencer: music data → register-write stream (the oracle) |
| `tools/opl_trace.py` | parses and normalises a captured original trace, if the spike yields one |
| `port/tests/test_opl.c` | vendored core: determinism, silence, a known write's effect |
| `port/tests/test_mixer.c` | mix/voice arithmetic, clipping, bounds (renders to buffers, no device) |
| `port/tests/test_samples.c` | sample decode, format rejection, bounds |
| `port/tests/test_sequencer.c` | event stream and register writes vs `tools/opl_seq.py` (byte-exact) |
| `port/spec/audio.md` | new subsystem spec: AIL surface, event grammar, formats, timer rate |

---

### Task 0: Branch and baseline

**Files:** none (repository state only).

**Interfaces:**
- Consumes: the merged sub-project 1 on `main`.
- Produces: branch `audio-ail` with a verified-green baseline, so every later task's diffs are attributable to this cycle.

- [ ] **Step 1: Confirm the baseline is green before branching**

```bash
git branch --show-current          # expect: main
git status --short                 # expect: clean
make verify                        # expect: exit 0, "all checks passed"
```

If the ladder is not green, stop and report — do not branch from a red baseline.

- [ ] **Step 2: Create the cycle branch**

```bash
git checkout -b audio-ail
git branch --show-current          # expect: audio-ail
```

- [ ] **Step 3: Record the baseline commit for later review ranges**

```bash
git rev-parse HEAD                 # note this as BASE0; review packages start here
```

No commit in this task.

---

### Task 1: Where do the songs and samples actually live? (investigation)

**Deliverable:** a new `port/spec/audio.md` with a "Data locations" section, each
claim marked `verified` with the command that showed it, and the answer to the
one question this cycle hinges on: **is the music a sequenced FM track, or Red
Book/CD audio?**

**Files:**
- Create: `port/spec/audio.md`

**Interfaces:**
- Consumes: the shipped install (`data/game/C`), the CD image (`data/game/CD/RAGECD.ISO`), `INDEX`, the decompilation.
- Produces: the decided answer that Task 8's sequencer design depends on, and the sample-location answer Task 7 depends on.

- [ ] **Step 1: Confirm `INDEX` really holds no audio**

```bash
python3 tools/le_info.py --index data/game/C/INDEX | grep -viE "\.gra|name" || echo "no non-.gra entries"
```

Expected: only `.gra` entries, confirming audio is read from the directory, not
the resource container. Record the observation.

- [ ] **Step 2: Look inside the two suspicious GRA files**

`S16SND2.GRA` and `S16RAD.GRA` may hold sample data. Walk their chunks and
inspect the first bytes of each body:

```bash
python3 tools/gra_chunks.py data/game/C/S16SND2.GRA data/game/C/S16RAD.GRA
python3 tools/gra_render.py data/game/C/S16SND2.GRA 0 /tmp/snd2.ppm   # expect a failure or nonsense if it is not an image
xxd -l 64 data/game/C/S16SND2.GRA
```

Record whether their content looks like palettes/descriptors (an image set) or
like PCM/"DIG" sample data or a music sequence.

- [ ] **Step 3: Look at the CD image**

```bash
ls -la data/game/CD/
python3 - <<'EOF'
import struct
d = open('data/game/CD/RAGECD.ISO','rb').read(0x10000)
# ISO9660 primary volume descriptor is at sector 16 (0x8000)
pvd = d[16*2048:16*2048+2048]
print('PVD type', pvd[0], 'id', pvd[1:6])
root = pvd[156:156+34]
ext = struct.unpack_from('<I', root, 2)[0]
print('root extent (sector)', ext, 'size', struct.unpack_from('<I', root, 10)[0])
EOF
```

Then list the root directory's entries (a small ISO9660 walk: 2048-byte sectors,
records with a length byte, name at offset 33) and record what audio-looking
files exist and their sizes. `RAGE.SND` is the prime candidate.

- [ ] **Step 4: Decide, from evidence, what the music is**

Answer explicitly, with the evidence that decides it:
- Is there a sequenced music payload (an event stream the driver would play), or
  is the music CD audio (large, Red-Book-shaped tracks), or something else?
- Where do the **samples** live (a GRA, `RAGE.SND`, or the data object)?
- Which files does the game actually open? Cross-check the decompilation: find
  the callers that open audio files and the strings naming them.

If the music is **Red Book/CD audio**, say so and **stop** — report it, because
the sequencer half of this spec is then wrong and the spec must be revised rather
than stretched.

- [ ] **Step 5: Record and commit**

Write the findings into `port/spec/audio.md` (data locations, the FM-vs-CD
answer, the sample location, each with its command and output).

```bash
git add port/spec/audio.md
git commit -m "audio: locate the music and sample data"
```

---

### Task 2: Can DOSBox-X log OPL register writes? (spike)

**Deliverable:** a yes/no with evidence, recorded in `port/spec/audio.md`. This
decides whether the cycle's primary oracle is available or whether the byte-exact
Python fallback governs. It is deliberately early, because its outcome shapes the
acceptance bar — and because a "no" must never be quietly absorbed.

**Files:**
- Modify: `port/spec/audio.md`
- Create (only if a route works): `tools/opl_trace.py`

**Interfaces:**
- Consumes: `dosbox-x` 2026.08.31, the game's own launcher `data/game/run-window.sh`.
- Produces: either a documented capture procedure plus `tools/opl_trace.py`, or a recorded "not achievable" with the three attempted routes.

- [ ] **Step 1: Try the three routes, cheapest first, and record each result**

1. `[capture]` config: does DOSBox-X's capture section cover OPL, or only audio/video?
   ```bash
   dosbox-x -defaultconf -printconf 2>/dev/null | sed -n '/\[capture\]/,/^\[/p'
   ```
2. `-opencaptures`: what does it launch, and does it include anything register-level?
   ```bash
   dosbox-x --help 2>&1 | grep -A2 -i opencaptures
   ```
3. The debugger's I/O logging: Task 13 recorded that this build's debugger refuses
   a scriptable session. Re-confirm that specific claim in one run rather than
   trusting the note, and record the exact error.

- [ ] **Step 2: If any route yields register writes, prove it end to end**

Run the original with that mechanism while it plays music, and show the captured
stream. Then write `tools/opl_trace.py` to parse it into the same normalised form
the C sequencer's output will use: an ordered sequence of `(tick, register,
value)` with `tick` derived from the capture's own timing, not wall-clock.

- [ ] **Step 3: Record the outcome either way**

In `port/spec/audio.md`: which route worked, the exact command, and a sample of
the captured stream — or, for each of the three, why it did not work. A "no" is a
complete result for this task, not a failure; it makes Task 9's fallback the
governing oracle.

- [ ] **Step 4: Commit**

```bash
git add port/spec/audio.md tools/opl_trace.py 2>/dev/null || git add port/spec/audio.md
git commit -m "audio: OPL register trace capture spike"
```

---

### Task 3: Enumerate the AIL surface the game calls (investigation)

**Deliverable:** a table in `port/spec/audio.md` of every call the game makes into
the audio layer, with its address, its observed signature, and what it must do.

**Files:**
- Modify: `port/spec/audio.md`

**Interfaces:**
- Consumes: `port/decomp/prage.c`, `port/decomp/prage.calls.csv`, `port/spec/game_flow.md`.
- Produces: the call list Task 10 implements, and the stub inventory for anything deferred.

- [ ] **Step 1: Start from the known entry points**

`0x10034` (sound-driver init, loads `SB16.DIG`), `0x1CF40` (reached from `main`),
and the driver-side functions `0x5D7DC`, `0x5D87E`, `0x5D973`, `0x5DB9E`. Read each
body and record what it does and what it calls.

- [ ] **Step 2: Walk outwards to the whole surface**

```bash
awk -F',' '$1=="00010034" || $3=="00010034"' port/decomp/prage.calls.csv
grep -n "SD_0005d7dc\|SD_0005d87e\|SD_0005d973\|SD_0005db9e" port/spec/game_flow.md 2>/dev/null
```

For every function the game calls in this layer, record: address, arguments and
return as used at the call sites, and the behaviour that must be reproduced. The
boundary is the line where the game stops and the driver begins — the timer
callback, the sample-play call, the music start/stop calls, the patch loads.

- [ ] **Step 3: Find the timer rate**

Something feeds the sequencer. Determine the rate the driver was expected to run
at (a `PIT` reprogram, an AIL timer setup, or the paced call site), because Task 8
paces the sequencer with it and Task 9's comparison groups writes per tick. If it
cannot be determined statically, mark it `TODO(verify)` and plan to infer it from
the trace, or from the music's own tempo events.

- [ ] **Step 4: Record and commit**

```bash
git add port/spec/audio.md
git commit -m "audio: enumerate the AIL call surface"
```

---

### Task 4: Vendor the OPL core

**Files:**
- Create: `port/src/platform/audio/opl/` (the core's single source file, its licence, and a wrapper header)
- Create: `port/tests/test_opl.c`
- Modify: `port/CMakeLists.txt`, `port/tests/run_tests.c`, `port/tests/test.h`, `.gitignore` if needed

**Interfaces:**
- Consumes: nothing.
- Produces: `opl_reset(void)`, `opl_write(u16 reg, u8 value)`, `opl_render(s16 *out, u32 frames)` in the wrapper header; the core itself stays private to `opl/`. Deterministic: the same writes produce the same samples.

- [ ] **Step 1: Choose and vendor the core**

Pick a small, permissively-licensed OPL core (a single-file OPL2/OPL3 emulator).
Copy its source and **its licence text** into `port/src/platform/audio/opl/`, and
note the origin, version and licence in a comment at the top of the wrapper and in
the task report. Do not modify the core's arithmetic — if it needs a build tweak,
isolate that in the wrapper.

- [ ] **Step 2: Write the failing test**

`port/tests/test_opl.c` must assert: silence after reset with no writes;
determinism (identical write sequences produce identical sample buffers across two
runs); and that a note-on style write sequence produces non-silent output. Sketch:

```c
static s16 buf_a[2048], buf_b[2048];

int test_opl(void)
{
    int before = g_failures;

    opl_reset();
    opl_render(buf_a, 1024);
    int silent = 1;
    for (int i = 0; i < 2048; i++) if (buf_a[i] != 0) silent = 0;
    CHECK(silent, "a reset core with no register writes renders silence");

    opl_reset();
    opl_write(0x20, 0x01); opl_write(0x40, 0x10); opl_write(0x60, 0xF0);
    opl_write(0x80, 0x77); opl_write(0xA0, 0x98); opl_write(0xB0, 0x31);
    opl_render(buf_a, 1024);
    int any = 0;
    for (int i = 0; i < 2048; i++) if (buf_a[i] != 0) any = 1;
    CHECK(any, "a key-on write sequence produces audio");

    opl_reset();
    opl_write(0x20, 0x01); opl_write(0x40, 0x10); opl_write(0x60, 0xF0);
    opl_write(0x80, 0x77); opl_write(0xA0, 0x98); opl_write(0xB0, 0x31);
    opl_render(buf_b, 1024);
    CHECK(memcmp(buf_a, buf_b, sizeof buf_a) == 0, "the core is deterministic");

    return g_failures - before;
}
```

- [ ] **Step 3: Add the wrapper and make it compile**

The wrapper exposes exactly the three functions above and includes the core only
from within `opl/`. Add the sources to `prage_core` and `tests/test_opl.c` to
`run_tests`.

```bash
cmake -S port -B build && cmake --build build && ./build/run_tests
```

Expected: `all checks passed`, and no other file in the tree includes the core's
header (`grep -rn "opl" port/src --include=*.c --include=*.h | grep -v "platform/audio/opl/"` shows only wrapper calls).

- [ ] **Step 4: Commit**

```bash
git add port .gitignore
git commit -m "audio: vendor the OPL core behind a private wrapper"
```

---

### Task 5: Host audio seam

**Files:**
- Modify: `port/src/host.h`, `port/src/host.c`
- Create: `port/tests/test_host_audio.c` (or extend `port/tests/test_host.c`)
- Modify: `port/CMakeLists.txt`, `port/tests/run_tests.c`, `port/tests/test.h`

**Interfaces:**
- Consumes: SDL3 (only inside `host.c`).
- Produces:
  - `int host_audio_open(int rate, int channels);` — returns 0 on failure (no device, bad rate) and never aborts; safe to call headless.
  - `void host_audio_close(void);`
  - `void host_audio_submit(const s16 *frames, int frame_count);` — a no-op when no device is open.
  - `u32 host_audio_rate(void);`

- [ ] **Step 1: Write the failing test**

Assert the headless contract, which is what the suite depends on: with no device
opened, `host_audio_submit` is a safe no-op; `host_audio_open` with an impossible
rate fails by returning 0 rather than aborting; and `host_audio_close` is
idempotent. Do **not** open a real device in the test.

- [ ] **Step 2: Implement in `host.c` only**

Use SDL's audio device with a callback (or a queue) that pulls from the mixer; the
seam is the only place SDL audio appears. Guard every entry on "device open" so
the suite and `--check` never need a device.

- [ ] **Step 3: Build and run**

```bash
cmake -S port -B build && cmake --build build && ./build/run_tests
```

Expected: `all checks passed` with no audio device opened; the existing suite
still passes.

- [ ] **Step 4: Commit**

```bash
git add port && git commit -m "audio: host audio seam (SDL confined to host.c)"
```

---

### Task 6: Mixer

**Files:**
- Create: `port/src/platform/audio/mixer.h/.c`, `port/tests/test_mixer.c`
- Modify: `port/CMakeLists.txt`, `port/tests/run_tests.c`, `port/tests/test.h`

**Interfaces:**
- Consumes: `opl_render` from Task 4; `samples.h`'s voice type from Task 7 (use a forward-declared minimal voice struct here and let Task 7 complete it).
- Produces: `mixer_reset(void)`, `mixer_add_sample(const s16 *pcm, u32 frames, int rate, int volume, int loop)`, `mixer_stop_samples(void)`, `mixer_render(s16 *out, u32 frames, u32 out_rate)`.

- [ ] **Step 1: Write the failing test**

Assert, rendering into buffers with no device: silence when nothing is playing;
one voice produces non-silence; two voices together are louder than one; a stopped
voice stops; output never exceeds `s16` range (clipping is applied, not wrapped);
and a voice with a rate different from the output rate is resampled without
reading out of bounds.

- [ ] **Step 2: Implement**

Mix the OPL core's output and the active sample voices into the output buffer,
with explicit saturation at the `s16` limits (a wrap here would be audible
garbage). Document the mixing arithmetic — headroom, per-voice volume, and whether
the original's driver mixed in software or in hardware — and mark anything
invented `/* PORT: ... */`.

- [ ] **Step 3: Build and run, then commit**

```bash
cmake -S port -B build && cmake --build build && ./build/run_tests; echo "exit=$?"
git add port && git commit -m "audio: software mixer with clipping and bounds"
```

---

### Task 7: Samples (SB16 16-bit)

**Files:**
- Create: `port/src/platform/audio/samples.h/.c`, `port/tests/test_samples.c`
- Modify: `port/CMakeLists.txt`, `port/tests/run_tests.c`, `port/tests/test.h`

**Interfaces:**
- Consumes: Task 1's answer about where sample data lives; `mem[]` and the resource manager if the data arrives through a GRA.
- Produces: `int samples_load(const u8 *data, u32 len, SampleVoice *out);` and whatever the sample-play AIL call needs, per Task 3's table.

- [ ] **Step 1: Write the failing test**

From Task 1's real data: a valid sample loads with the correct rate/channel count
and frame count; a truncated sample is rejected rather than read past its end; an
unknown format is rejected; a zero-length sample is rejected. Use the real bytes
you located, not a synthetic shape you invented.

- [ ] **Step 2: Implement, using the real format**

Implement the format the data actually uses (confirm it against the bytes — do not
assume a header layout). If the sample data arrives inside a GRA, read it through
`res_resolve`/`gra_*` rather than opening the file directly, matching how the game
reaches it.

- [ ] **Step 3: Build and run, then commit**

```bash
cmake -S port -B build && cmake --build build && ./build/run_tests; echo "exit=$?"
git add port && git commit -m "audio: SB16 16-bit sample loading"
```

---

### Task 8: Sequencer (music → register writes)

**Files:**
- Create: `port/src/platform/audio/sequencer.h/.c`, `port/src/platform/audio/patches.h/.c`
- Modify: `port/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1's music data and its container; Task 2's timer-rate answer; Task 4's `opl_write`.
- Produces: `int seq_load(const u8 *data, u32 len);`, `void seq_tick(void);` (one driver tick), `void seq_start(void); void seq_stop(void);`, and `int seq_active_track(void);`.

- [ ] **Step 1: Decode `FAT.OPL` first, since both the sequencer and the patches need it**

The bank is a table of `(value, offset)` pairs (verified: `0x400`, `0x44E`,
`0x45C`, … stepping `0xE`). Determine what each entry's payload is (an instrument
name, a patch byte block, or both) and record the layout in `FORMATS.md` with the
command that showed it. Load it into the core's instrument state.

- [ ] **Step 2: Determine the music event grammar from the data, not from assumption**

Read the music payload located in Task 1, alongside every consumer of it in the
decompilation, and write the event grammar into `port/spec/audio.md` (note
on/off, program/patch, controller, tempo, end-of-track) with evidence. Mark
anything still unclear `likely` or `TODO(verify)` — do not promote a guess.

- [ ] **Step 3: Implement, paced by the driver's tick**

`seq_tick()` advances the event stream by one driver tick and emits the register
writes that tick implies. Keep the tick rate a single named constant whose value
cites Task 2/3's finding, and mark it `TODO(verify)` if it is still inferred.

- [ ] **Step 4: Commit**

```bash
git add port FORMATS.md
git commit -m "audio: FAT.OPL patch bank and the sequencer"
```

---

### Task 9: The oracle — `tools/opl_seq.py`, and the trace comparison

**Files:**
- Create: `tools/opl_seq.py`, `port/tests/test_sequencer.c`
- Modify: `port/CMakeLists.txt`, `port/tests/run_tests.c`, `port/tests/test.h`

**Interfaces:**
- Consumes: the music data (Task 1), the grammar (Task 8), `tools/opl_trace.py` if the spike succeeded.
- Produces: `tools/opl_seq.py <music> --trace` emitting the normalised `(tick, reg, value)` stream; `test_sequencer.c` comparing the C sequencer's stream against it byte-for-byte.

- [ ] **Step 1: Write the Python sequencer independently**

It may not mirror the C code's structure; it decodes the same data to the same
normalised stream. Its output format must be exactly what `tools/opl_trace.py`
produces, so the three can be compared pairwise.

- [ ] **Step 2: Write the failing test**

`test_sequencer.c` runs the C sequencer over one complete piece of music, captures
its `(tick, reg, value)` stream, and compares it to `tools/opl_seq.py`'s — reporting
the **first** difference. If the spike produced a trace, the test also compares
against it, with a documented normalisation (per-tick grouping; wall-clock ignored).

- [ ] **Step 3: Iterate until byte-exact, or report the first difference**

Run it, fix the first difference, repeat:

```bash
python3 tools/opl_seq.py data/game/C/<music> --trace > /tmp/opl_seq.txt
cmake --build build && ./build/run_tests
```

Do not add a tolerance, do not skip a region, and do not weaken the comparison to
make it pass. If it cannot be made byte-exact, report the first differing tick and
what remains — that is a `BLOCKED` outcome, not a completed task.

- [ ] **Step 4: Commit**

```bash
git add tools port && git commit -m "audio: byte-exact register-stream oracle"
```

---

### Task 10: The AIL surface

**Files:**
- Create: `port/src/platform/audio/ail.h/.c`
- Modify: `port/CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 3, 4, 6, 7, 8.
- Produces: one C function per AIL call in Task 3's table, with the original's signatures and return semantics, wired to the sequencer/samples/mixer; plus the fixed-profile short-circuits.

- [ ] **Step 1: Port call-for-call**

One C function per original function, header comment `/* 0xADDR — spec section */`.
Where the original asks the driver to probe or select hardware, short-circuit to
the fixed profile and mark it
`/* PORT: fixed audio profile, no hardware probe */`.

- [ ] **Step 2: Replace the real-mode boundary**

The original's calls cross into a DOS driver through real-mode/DPMI reflection.
Those crossings become direct C calls; each replacement is marked `/* PORT: ... */`
and listed in the report, because it is the one place the port's audio path has no
original instruction behind it.

- [ ] **Step 3: Build and run, then commit**

```bash
cmake -S port -B build && cmake --build build && ./build/run_tests; echo "exit=$?"
git add port && git commit -m "audio: the AIL call surface"
```

---

### Task 11: Wire it into the game's init and the title

**Files:**
- Modify: `port/src/game/flow.c`, `port/src/game/flow.h`, `port/src/main.c`
- Modify: `port/tests/test_flow.c`

**Interfaces:**
- Consumes: Task 10.
- Produces: the init chain's audio calls running the ported path, and the title/attract states starting and driving music.

- [ ] **Step 1: Replace the audio stubs in the init chain**

Sub-project 1 stubbed the audio calls on the init path with `/* PORT: <sub-project> */`
markers. Replace them with the ported calls, keeping the original's order, and list
every stub you replaced in the report.

- [ ] **Step 2: Start music where the original does**

Find where the original starts the title/attract music and call the sequence
start/stop at the same point, through the same state — not on a port-side timer.

- [ ] **Step 3: Prove it runs, without a device in the suite**

```bash
cmake --build build && ./build/run_tests                      # must not open a device
./build/prageport --game-dir data/game/C --check 120           # headless: asserts the sequencer advanced
./build/prageport --game-dir data/game/C                       # windowed: music audible
```

State what you observed in each case, including whether the music was audible —
and if you cannot hear it in this environment, say so rather than claiming it.

- [ ] **Step 4: Commit**

```bash
git add port && git commit -m "audio: run the ported audio path from the init chain and title"
```

---

### Task 12: One announcer sample through the game's own call path

**Files:**
- Modify: `port/src/game/flow.c` (only the call site the game itself uses), `port/tests/test_flow.c`

**Interfaces:**
- Consumes: Tasks 7, 10, 11.
- Produces: the third definition-of-done item, proven.

- [ ] **Step 1: Find the game's own sample-play call site**

Locate where the original plays an announcer/speech sample (the sample-play AIL
call with a located sample's handle) and drive it from the same place, not from a
port-side hook added for the test.

- [ ] **Step 2: Assert it headlessly**

In `--check`, assert the sample became an active voice and that the mixer rendered
non-silence while it was active — the observable form of "it played", with no
device open.

- [ ] **Step 3: Run and report**

```bash
./build/prageport --game-dir data/game/C --check 120; echo "exit=$?"
./build/prageport --game-dir data/game/C      # listen: the announcer sample
```

- [ ] **Step 4: Commit**

```bash
git add port && git commit -m "audio: announcer sample through the game's call path"
```

---

### Task 13: Verification ladder, docs and report

**Files:**
- Create: `docs/superpowers/plans/2026-09-16-audio-ail-port-report.md`
- Modify: `README.md`, `FORMATS.md`, `port/spec/audio.md`, `port/RE_GUIDE.md`, `port/PORTING.md` (only if the rules needed clarifying), `docs/superpowers/plans/2026-09-16-engine-core-port-report.md` (supersede its sub-project 3 entry)

- [ ] **Step 1: Run the full ladder and record it verbatim**

```bash
cmake -S port -B build && cmake --build build
PR_ORACLE_REQUIRED=1 ./build/run_tests
PR_ORACLE_REQUIRED=1 ./build/prageport --game-dir data/game/C --check 120
python3 tools/gen_symbols.py port/decomp port/src/symbols.h   # must stay byte-identical
```

- [ ] **Step 2: Write the sub-project report**

Per module: what is ported and verified; what is stubbed and by which sub-project;
every `/* PORT: ... */` deviation (including every fixed-profile short-circuit and
every replaced real-mode crossing); every `/* TODO(verify): ... */`; the
verification commands and outcomes; the vendored core's origin, version and
licence; whether the OPL trace was capturable and which oracle therefore governs;
and the unresolved risks.

- [ ] **Step 3: Update the docs**

`README.md` gets the audio status, how to run with sound, and the licence note;
`FORMATS.md` gets the music and sample container layouts and `FAT.OPL`; `port/spec/audio.md`
gets the AIL surface, the event grammar, the timer rate and the data locations;
`port/RE_GUIDE.md` gets the new landmarks. Claims must not exceed what was proven:
if the trace was not captured, say the Python register-stream oracle governs.

- [ ] **Step 4: Commit**

```bash
git add -A port tools docs README.md FORMATS.md
git status --short     # confirm nothing under data/ or _tools/ghidra_proj/ is staged
git commit -m "docs: audio cycle report and updated status"
```

---

## Self-Review

**Spec coverage.** Design §1 DoD 1 (init path) → Tasks 10, 11. DoD 2 (music) →
Tasks 8, 9, 11. DoD 3 (sample) → Tasks 7, 12. DoD 4 (fidelity proven) → Tasks 2,
9, 13. Gating discovery → Task 1. Non-goals are excluded by omission and stated in
the spec. Design §2 (driver boundary, layout, SDL confinement, no probing) → Tasks
0 (branch), 4, 5, 10. §3 (data flow, pacing, resampling) → Tasks 6, 8, 9. §4
(oracle, spike, fallback) → Tasks 2, 9. §5 (verification) → Tasks 4–9, 13. §6
risks → 1 (Task 1), 2 (Task 2), 3 (Tasks 3, 8), 4 (Task 4, 13), 5 (Task 1), 6
(Task 3). §7 docs → Task 13. No uncovered requirement.

**Placeholder scan.** No `TBD`. The two investigation tasks (1, 2, 3) and the
grammar step (8.2) deliberately carry no pre-baked code or invented format — their
deliverable *is* the finding, and each states its acceptance criterion and what
must be reported if the finding is "no". That is the same shape sub-project 1 used
for its investigation tasks, and it is not a placeholder: the value is the
evidence, and a "no" is an explicitly acceptable result.

**Type consistency.** `opl_reset/opl_write/opl_render` are used identically in
Tasks 4, 6 and 8. `host_audio_open/close/submit/rate` are used identically in Tasks
5, 6 and 11. `mixer_render(s16 *, u32, u32)` is the single mixing entry point in
Tasks 6, 11 and 12. `seq_load/seq_tick/seq_start/seq_stop` are consistent between
Tasks 8 and 11. `samples_load(const u8 *, u32, SampleVoice *)` matches Task 7 and
Task 12's use. `tools/opl_seq.py` and `tools/opl_trace.py` emit the same normalised
`(tick, reg, value)` stream that Task 9 compares.

**Dependency note.** Task 6 (mixer) forward-declares a minimal voice struct and
Task 7 completes it; the implementer must not invert those tasks. Task 9's
comparison cannot start before Task 8's grammar exists, and Task 1 gates the whole
cycle — if the music turns out to be CD audio, stop and revise the spec instead of
continuing down the FM path.
