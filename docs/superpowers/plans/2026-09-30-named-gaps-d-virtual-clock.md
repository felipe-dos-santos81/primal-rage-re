# Virtual Mixer Clock (named-gaps sub-project D) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Close G6. With no audio device, a started one-shot sample ends at
its real length, so `AIL_sample_status` reports 2 and the four sample slots
free, as the raw DIG service does at the buffer end (`0x6F28F`). The
saturation measured in `--check 8000` (BD/BE/BF fire 41/14/18 times; four
slots fill) no longer occurs. The looping attract samples `0x40`/`0x42` keep
playing. The FM WAV, the frame dumps and every oracle line stay identical.

**Architecture:** A virtual mixer clock inside `game_audio_service` (`0x1CF20`,
`flow.c`). When `host_audio_rate()` is 0, the service no longer returns before
the render. It takes the frames due from the ISR tick `DS_00101500`
(`game_isr_ticks`, 60 Hz) at the fixed profile rate `MIXER_OPL_RATE`, and runs
the **same** `mixer_render` → `host_audio_submit` path as a device run.
`host_audio_submit` is already a no-op without a device, so the frames are
discarded. A one-shot's voice goes inactive in `voice_read` at its buffer end.
`AIL_sample_status` then reports 2 through its existing
`mixer_sample_active` check.
- No change to `host.c`, `mixer.c` or `ail.c` code (comments only in `ail.c`
  and `main.c`). SDL stays in `host.c`/`main.c`.
- The ISR tick is used, not the host tick, because the host tick is wall
  clock (F6). A headless run must stay deterministic.

**Tech Stack:** C over flat `mem[]` (SDL3 port), CMake, the `CHECK`/`CHECK_EQ_INT`
suite. Raw reads come from the fixup-applied LE image via capstone
(`$K/dx.py`) and from the raw data files. The Ghidra MCP is unavailable
(spec §5).

**Spec:** `docs/superpowers/specs/2026-09-30-named-gaps-design.md`, §3 decision 3
and §4.D (gap G6). Raw source: `docs/superpowers/plans/2026-09-29-k7-k12-derivations.md`
§0.7.6 (the AIL end status), §2.1 (`AIL_sample_status`, `game_isr_ticks`) and
§7.6 (the headless slot saturation). Ledger:
`docs/superpowers/plans/2026-09-29-all-gaps-ledger.md` §H.3 row 6.

## Global Constraints

- **Branch prerequisites.** The ledger row this plan closes (§H.3 row 6,
  "Headless sample slots never end") exists only from commit `0247a1a`
  (branch `all-gaps-final`, head `0c131db` when this plan was written). That
  commit is not an ancestor of the spec commit `a169296`
  (main / `named-gaps-spec`). Work in the worktree the controller names. It
  must satisfy all of these:
  - It contains both commits.
  - `git branch --contains 0247a1a` lists its branch.
  - `data` and `.superpowers` are symlinks to the main checkout's (as
    `.worktrees/all-gaps-final` has them).

  Task 1 Step 1 checks this and **halts** if it does not hold. If
  sub-projects B or C have merged first, their commits are fine. Only the two
  ancestors are required.
- **Run everything from that worktree's root.** `data/` is read-only.
- **Variables:**
  - `K=.superpowers/sdd/2026-09-29-k7-k12/scratch` holds the baselines:
    `oracle-lines-base.txt`, `base.sha256`, `before-t2.wav`, `dumps.sh`,
    `dumpsha.sh`, `dx.py`.
  - `D=.superpowers/sdd/2026-09-30-named-gaps-d-virtual-clock/scratch` is this
    plan's scratch.
  - Both are git-ignored.
- **No oracle claim moves.** The gate is `make verify` exit 0, and the oracle
  lines must equal `$K/oracle-lines-base.txt`. `make verify` always runs with
  these parallel-safety overrides, where `N` is the task number:
  `SMK_DUMP=/tmp/pr_dN_smk TITLE_DUMP=/tmp/pr_dN_title ATTRACT_DUMP=/tmp/pr_dN_attract FRONTEND_DUMP=/tmp/pr_dN_frontend TITLE_PIN_DIR=/tmp/pr_dN_pin AUDIO_WAV=/tmp/pr_dN_fm.wav`
  Two more conditions:
  - The frame dumps must match `$K/base.sha256` (`dumps.sh` + `dumpsha.sh`).
  - The `make audio-render` WAV must be byte-identical to `$K/before-t2.wav`.

  If anything moves, **halt** and report the old value, the new value and the
  diff. Do not "fix" an oracle, a manifest or an existing assertion.
- **Raw wins; never a fitted constant.** Every rate, length and tick below
  traces to a listed raw address or raw file byte (Findings F3, F4, F6, F9).
  On a plan-vs-raw conflict, record the correction with its address in the
  record and follow the raw.
- **Porting rules:**
  - In `port/src`, comments are plain text or `/* PORT: ... */`. No new
    `TODO(verify)` is needed.
  - Do not reformat `flow.c`, `ail.c` or `main.c`. Touch only the lines named.
  - `symbols.h` is generated; do not touch it. Every `DS_` name used below
    already exists in it.
  - SDL and file I/O stay in `host.c`/`main.c`. This plan adds no host code.
  - **No original state in a C global.** `s_last_isr_tick` is port
    bookkeeping beside `s_last_host_tick`, marked `PORT:`. The tick itself
    stays in `mem[]` (`DS_00101500`).
- **Tests:**
  - Use only `CHECK`/`CHECK_EQ_INT`.
  - Seed sentinels that differ from the post-conditions. Never assert an
    unseeded BSS zero.
  - Every new assertion group must fail under a named mutation (Task 2's
    table).
  - The new function `test_virtual_clock` goes in `port/tests/test_audio.c`.
    It is registered once, as the **last** line of `TEST_CASES` in
    `port/tests/test.h`. That places it after the WAV render
    (`test_sequencer`) and after every test that could read the slots.
  - It runs on the shared unit-suite process whose resources, slot handles
    and slot buffers `test_res`/`test_flow` set up. `game_init()` is not
    called.
- **Assertion-site count:** `rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l`.
  Expect exactly **+40** after Task 2 (the CHECK sites in the code block of
  Task 2 Step 1).
- **Commits:**
  - Style: `<area>: <what changed>`.
  - Trailer: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
  - Stage named files only (never `git add -A`).
  - Commit only where a step says so and the controller has authorised
    commits. Otherwise stop at that step and ask.
- **0 compiler warnings** (`-Wall -Wextra`). No new dependency.
- **SDD ledger:** `.superpowers/sdd/2026-09-30-named-gaps-d-virtual-clock/progress.md`.
  Append one line per completed step group.

## Review Focus

1. **The clock is wall time, so headless runs become nondeterministic.** A
   clock on `host_tick_count()` would free slots at jittered frames. **Task 2**
   covers this with mutation M2: the virtual branch on the host tick fails the
   end-tick checks, because the suite never pumps the host clock. **Task 3**
   runs the `--check 8000` occupancy probe twice and requires identical lines.
2. **A one-shot ends at the wrong time.** Too early would mean a doubled clock
   or a wrong rate; too late would mean no render. **Task 2** V1 covers this:
   it asserts status 4 one tick before the computed end, 2 at it, and the end
   within `[ceil(len*60/0x2B11), +1]` ticks. Mutations M1 and M4 fail it. V6
   and V7 do the same on the shipped BD/BE/BF lengths.
3. **Looping samples end or get evicted.** **Task 2** V2 (a loop lives 3× a
   one-shot's length), V6's end state (both attract loops keep their slots and
   status 4 across six one-shots) and V7 (0x42 queues once BD ends and then
   keeps looping) cover this. Mutation M5 fails them. **Task 3** re-runs
   `--check 8000`, whose probe requires 0x40/0x42 playing before the title.
4. **A frame, oracle line or the WAV moves.** Game code does read the status
   (F5, a spec conflict), so a freed slot changes the dispatcher's path.
   **Task 1** re-proves F11 with `rg`. **Task 3** runs the full gate:
   `make verify`, the oracle lines, the dumps against `base.sha256`, and
   `cmp` of the WAV.
5. **A backward tick or a long stall bursts the render.** A test restoring
   `mem[]` moves `DS_00101500` back, and a res.c stall can add many ticks.
   Without a guard the service would render 30 ticks at once and end short
   samples early. **Task 2** V3 covers this: the tick goes back 5, and the
   next service renders nothing. Mutation M3 fails it. The 30-tick clamp is
   the existing `HOST_TICK_MAX_CATCHUP` (not re-tested; it is in the record's
   "Not tested" list with its reason).

## Findings so far

- Spec: `docs/superpowers/specs/2026-09-30-named-gaps-design.md` §3.3 and §4.D (G6).
- F1 (end decision). `AIL_sample_status` (`port/src/platform/audio/ail.c:362-373`)
  turns state 4 into 2 when `mixer_sample_active(sample)` is 0. The mixer clears
  a one-shot voice in `voice_read` (`mixer.c:154-169`) on the first read with
  `idx >= frames`; `voice_read` is reached only from `mixer_render`
  (`mixer.c:171-192`). So the end is decided inside `mixer_render`.
- F2 (why nothing advances headless). `game_audio_service` (`flow.c:5984-6016`)
  returns at `if (rate == 0) return;` (`flow.c:6008`) when
  `host_audio_rate()` is 0, before `mixer_render`. With no device no voice's
  `idx` moves, so a started one-shot's state stays 4 forever.
- F3 (the output rate). The mixer renders at `MIXER_OPL_RATE` 49716
  (`mixer.h:50`, mirrored from `OPAL_OPL3_SAMPLE_RATE`, guarded by a
  `_Static_assert` in `opl.c`); `main.c:30` opens the device at that rate. The
  per-service frame count is `rate * elapsed / 60` with a `/60` remainder
  (`flow.c:6009-6011`), elapsed host ticks clamped to `HOST_TICK_MAX_CATCHUP`
  (30, `host.h:15`), frames clamped to `AUDIO_FRAMES_MAX` (`flow.c:83`).
- F4 (the sample's own rate and length). `sound_sample_start` (0x1CB18,
  `flow.c:6329-6353`) sets rate `0x2B11` = 11025 Hz (`0x1CBB0`) and the length
  `size` = the resource's first dword (`0x1CB49`), 8-bit mono, so a one-shot
  lasts `size` source frames = `size / 11025` s; loop count 0 only when the
  slot's loop byte is 1 (`0x1CBCA..0x1CBE1`). `AIL_start_sample`
  (`ail.c:283-311`) converts `len` bytes to `frames` s16 and adds a mixer
  voice with `loop = (sample->loop == 0)`.
- F5 (SPEC CONFLICT: game code does read sample status). The spec (§4.D) says
  "no game code reads sample status (only `main.c`'s probe does)". The raw and
  the port disagree: `snd_slot_status` (0x5DD03 call, `flow.c:6099`) is read by
  `snd_sample_playing` 0x1CE70 (`flow.c:6112`), `snd_sample_stop` 0x1CE04
  (`:6126`), `snd_samples_stop_all` 0x1CD9C (`:6144`), `sound_sfx_volume`
  0x1CED4 (`:6211`) and `snd_sample_queue` 0x1CC28 (`:6294`, `:6305`); and the
  voice dispatcher 0x2C3FC returns early when `snd_sample_playing` is 1
  (`flow.c:6395/6400/6406/6412/6423`). So a freed slot changes which slot a
  queue takes, whether a case-2/3 voice is queued at all, and whether case 4
  stops all samples. Raw wins: these readers are real. The oracle exposure is
  bounded to mem[] slot records `DS_00102860..DS_001028BF` and the slot
  buffers, plus `res_resolve` calls on a re-queued handle (the same handle was
  resolved on its first queue, so no first-read loader draw is new). That is a
  claim the gate must prove. Task 1 Step 4 re-audits the readers, and
  Task 3 Step 1 runs the full oracle gate.
- F6 (the time base). `host_tick_count()` is wall clock: `host_pump`
  (`host.c:222-241`) advances `g_tick` by elapsed `now_ns()` intervals, and
  `host_wait_vblank` sleeps to the boundary (`host.c:244-256`). A virtual clock
  on it would free slots at wall-clock-jittered frames, i.e. nondeterministic
  `--check`/driver runs. The deterministic 60 Hz clock is the ISR tick
  `DS_00101500` (`game_isr_ticks`, `flow.c:6717-6721`; 0x1BE0F/0x1BE16),
  advanced once per master-loop spin (`flow.c:6803-6806`) and by res.c's
  modelled read stalls; 0x500BB reads it as the sound module's "time"
  (`snd_sample_queue`, `flow.c:6285`). Its rate is the raw's AIL timer
  frequency `0x3C` = 60 Hz (0x1CFFA push 0x3c; 0x1CFFF
  AIL_set_timer_frequency; `host.c:22-35`). The 60.05 Hz figure is a
  measured DOSBox counter, not a raw constant: raw wins, 60.
- F7 (render path has no pull). With a device the port *pushes*
  (`host_audio_submit` -> `SDL_PutAudioStreamData`, `host.c:374-382`); voices
  advance when `mixer_render` runs, not when SDL consumes. So there is no
  device "frames consumed" count to mirror; the same code path headless is
  "run `mixer_render` for the frames due and discard them".
- F8 (WAV). `make audio-render` is `PR_AUDIO_WAV=... run_tests`
  (`Makefile:328-331`); the WAV is written by `test_audio.c:1199-1260`, which
  calls `opl_reset`, `mixer_reset`, `seq_start` then `mixer_render` directly;
  it never calls `game_audio_service`. Byte-identity holds if `mixer_render`'s
  arithmetic and `voice_read` are unchanged and nothing leaves a voice live
  across `mixer_reset` (it clears all voices).
- F9 (raw sample facts, read from `data/game/C`). Voice records at
  `DS_000BBDC8 + id*12` (raw file offset `0xBBDC8 + 0x46E54`; the handle dwords
  carry no fixup): `0x40` case 2 `0x0383B6F4` byte 1; `0x42` case 2
  `0x03837440` byte 1; `0xBD` case 2 `0x03837440` byte **0** (0x42's sample as
  a one-shot); `0xBE` case 2 `0x038391C9` byte 0; `0xBF` case 2 `0x038416A6`
  byte 0. All are INDEX entry 7 (`s16title.gra`). The size dwords at the
  handle offsets in `S16TITLE.GRA`: `0x0383B6F4` 24494 (`0x5FAE`, matching
  `test_game.c`'s comment), `0x03837440` 7557, `0x038391C9` 9511,
  `0x038416A6` 10904. With step `(0x2B11<<16)/49716 = 14533` the end ticks
  are 42 (BD), 52 (BE), 60 (BF), 134 (0x40 as if one-shot), 61 for a
  synthetic 0x2B11-byte buffer, 1 for 64 bytes; each is `ceil(len*60/11025)`
  or one more (11025 bytes: 60 -> 61, the 16.16 step's floor), for every
  sub-tick remainder 0..59.
- F10 (why saturation happened, and what the clock changes). BD shares 0x42's
  handle: while a stuck BD slot reads 4, 0x2C483 refuses 0x42 (dispatcher
  returns 0 before `0x1CC28`), so the attract's second-cycle 0x42 loop is lost,
  and BD/BE/BF stuck at 4 hold three slots. With the clock they end at
  42/52/60 ticks.
- F11 (oracle exposure is bounded, by construction). `res_resolve` draws the
  loader and stalls the tick only on an entry's first resolve
  (`res.c:290-300`); `RES_FLAG_LOADED` is never cleared (`rg` finds only the
  two setters). A re-queue after a slot frees resolves a handle that was
  resolved on its first queue, so no new loader draw or stall. First queues
  are gated only by the same handle's own playing check, so their timing does
  not move. No reader of the slot records outside `flow.c`'s sound module
  exists except `main.c`'s probe (`rg DS_0010286|DS_0010287|DS_001028[89AB]`).
  `sound_voice`'s AL is read only by `sound_voice_stage` (case 1, no status)
  and `svcmenu.c:359-392` (service menu, no oracle). The dispatcher reads no
  rng (k7-k12 §0.6). So the frame dumps should not move; the gate proves it.

---

### Task 1: Baseline, raw re-read, reader audit, and the "before" slot occupancy

**Files:**
- Create: `docs/superpowers/plans/2026-09-30-named-gaps-d-derivations.md` (§D.0).
- Create (git-ignored): `$D/`, `.superpowers/sdd/2026-09-30-named-gaps-d-virtual-clock/progress.md`.
- Temporarily modify, then restore byte-for-byte (never committed): `port/src/main.c` (the occupancy probe).

**Interfaces:**
- Consumes: the `$K` baselines; `game_audio_service` (`flow.c:5984-6016`),
  `AIL_sample_status` (`ail.c:362-373`), `run_check` (`main.c:157-264`).
- Produces: `$D/verify-base.txt`, `$D/checks-base.txt`, `$D/progress-base.txt`,
  `$D/dx-*.txt`, `$D/readers.txt`, `$D/occ-before.txt`, record §D.0.

- [ ] **Step 1: Prerequisites.**

```bash
git merge-base --is-ancestor a169296 HEAD && git merge-base --is-ancestor 0247a1a HEAD && echo BRANCH-OK
git branch --contains 0247a1a --format='%(refname:short)' | grep -qx "$(git branch --show-current)" && echo CONTAINS-OK
rg -n "Headless sample slots never end" docs/superpowers/plans/2026-09-29-all-gaps-ledger.md
test -L data && test -d .superpowers/sdd/2026-09-29-k7-k12/scratch && echo LINKS-OK
git status --short
```
Expected:
- `BRANCH-OK` and `CONTAINS-OK`.
- One ledger hit, at the `| 6 |` row of §H.3.
- `LINKS-OK`.
- A clean status.

If any of these fails, **halt** and report to the controller. Without the
ledger row this plan has nothing to close.

- [ ] **Step 2: Baseline gate (before any change).**

```bash
K=.superpowers/sdd/2026-09-29-k7-k12/scratch; D=.superpowers/sdd/2026-09-30-named-gaps-d-virtual-clock/scratch; mkdir -p $D
make build 2>&1 | grep -E 'warning|error'
make verify SMK_DUMP=/tmp/pr_d1_smk TITLE_DUMP=/tmp/pr_d1_title ATTRACT_DUMP=/tmp/pr_d1_attract FRONTEND_DUMP=/tmp/pr_d1_frontend TITLE_PIN_DIR=/tmp/pr_d1_pin AUDIO_WAV=/tmp/pr_d1_fm.wav 2>&1 | tee $D/verify-base.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $D/verify-base.txt | diff $K/oracle-lines-base.txt - && echo ORACLES-EQUAL
rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l | tee $D/checks-base.txt
python3 tools/port_progress.py | tee $D/progress-base.txt
sh $K/dumps.sh d-base && sh $K/dumpsha.sh d-base && echo DUMPS-IDENTICAL
make audio-render AUDIO_WAV=$D/before.wav >/dev/null 2>&1; cmp $K/before-t2.wav $D/before.wav && echo WAV-IDENTICAL
```
Expected:
- No warning or error lines.
- `EXIT=0`, `ORACLES-EQUAL`, `DUMPS-IDENTICAL` and `WAV-IDENTICAL`.

If any baseline differs, **halt**: the tree is not the base this plan was
written against. Keep `$K/d-base/check/log.txt` (copy it to
`$D/check-base-log.txt`), then `rm -rf $K/d-base` (the dump is large; the sha
check is what counts).

- [ ] **Step 3: Re-read the raw.** Each line below is evidence the code or the
  tests encode (Findings F3, F4, F6, F9).

```bash
python3 $K/dx.py 1CB18 1CC28 | tee $D/dx-1CB18.txt | rg -n "0x2b11|1cb49|1cbb0|1cbca|1cbe1"
python3 $K/dx.py 1CF40 1D0BC | tee $D/dx-1CF40.txt | rg -n "0x2b11|0x3c$|0x1bdf4|1cffa|1cfff"
python3 $K/dx.py 1BDF4 1BE30 | tee $D/dx-1BDF4.txt | rg -n "0x101500|0x101508"
python3 $K/dx.py 2C473 2C4C6 | tee $D/dx-2C473.txt | rg -n "1ce70|1cc28"
python3 - <<'PY' | tee $D/records.txt
import struct
d = open('data/game/C/PRAGE.EXE', 'rb').read()
g = open('data/game/C/S16TITLE.GRA', 'rb').read()
for i in (0x40, 0x42, 0xBD, 0xBE, 0xBF):
    c, h, b = struct.unpack_from('<III', d, 0xBBDC8 + 0x46E54 + i * 12)
    print(hex(i), 'case', c, 'handle', hex(h), 'byte', b & 0xFF, 'entry', h >> 23,
          'size', struct.unpack_from('<I', g, h & 0x7FFFFF)[0])
PY
```
Confirm:
- `1cb49: mov ecx, dword ptr [eax]` reads the size dword.
  `1cba4: push 0x2b11` / `1cbb0: call 0x5dca6` sets the rate to 11025 Hz.
  `1cbca: mov al, byte ptr [ebp + 0x102868]` .. `1cbe1: call 0x5dce4` sets
  loop count 0 only for loop byte 1.
- `1cf6e: push 0x2b11` (preference 1, the DIG rate). `1cfed: push 0x1bdf4`,
  `1cffa: push 0x3c` and `1cfff: call 0x5da87` set the timer frequency to
  60 Hz.
- `1be08: mov ebx, dword ptr [0x101500]` .. `1be16: mov dword ptr [0x101500], ebx`
  is the ISR's increment of the clock.
- `2c483: call 0x1ce70` precedes `2c4b6: call 0x1cc28`. Case 2 skips the
  queue when `0x1CE70` returns 1.
- `records.txt` is exactly:
```
0x40 case 2 handle 0x383b6f4 byte 1 entry 7 size 24494
0x42 case 2 handle 0x3837440 byte 1 entry 7 size 7557
0xbd case 2 handle 0x3837440 byte 0 entry 7 size 7557
0xbe case 2 handle 0x38391c9 byte 0 entry 7 size 9511
0xbf case 2 handle 0x38416a6 byte 0 entry 7 size 10904
```
Any difference is a plan-vs-raw conflict: **halt** and report it. The tests
in Task 2 encode these values.

- [ ] **Step 4: Audit the status readers (F5, F11).**

```bash
{ rg -n "AIL_sample_status|snd_slot_status\(|mixer_sample_active|mixer_active_voices" port/src
  rg -n "DS_0010286|DS_0010287|DS_001028[89AB]" port/src -g '!symbols.h' -g '!flow.c'
  rg -n "RES_FLAG_LOADED" port/src/platform/res.c
  rg -n "=[^=;]*sound_voice(_stage)?\(|if \(sound_voice|return sound_voice" port/src; } | tee $D/readers.txt
```
Expected (line numbers may shift by the merged B/C work; the set may not):
- `AIL_sample_status` is called only at `flow.c` (`snd_slot_status`) and
  `main.c` (`attract_loop_playing`).
- `snd_slot_status(` has six call sites, all in `flow.c`'s sound module:
  `0x1CE70`, `0x1CE04`, `0x1CD9C`, `0x1CED4`, and `0x1CC28` twice.
- The slot-record `rg` hits only `main.c`'s probe (and `flow.h`'s comment).
- `RES_FLAG_LOADED` shows only the define, the two setters and the test at
  `res_resolve`. There is no clear.
- `sound_voice` results are read only by `sound_voice_stage` and
  `svcmenu.c`'s four returners.

Any other reader is a new oracle exposure: **halt** and report it.

- [ ] **Step 5: Measure the "before" slot occupancy in `--check 8000`.**
  This step adds a scratch probe to `main.c`, measures, and restores
  `main.c` byte-for-byte.

Write the probe script once (Task 3 reuses it):

```bash
D=.superpowers/sdd/2026-09-30-named-gaps-d-virtual-clock/scratch
cat > $D/occ.sh <<'SH'
#!/bin/sh
# usage (repo root): sh $D/occ.sh <tag>. Adds the scratch occupancy probe to
# main.c, runs --check 8000 into $D/occ-<tag>/, restores main.c byte-for-byte.
D=.superpowers/sdd/2026-09-30-named-gaps-d-virtual-clock/scratch; R=$(pwd)
cp port/src/main.c $D/main.c.keep
python3 - <<'PY'
p = 'port/src/main.c'
s = open(p).read()
anchor = "        game_loop();                     /* frame i */\n"
assert s.count(anchor) == 1
probe = r'''        {   /* SCRATCH occupancy probe (named-gaps D), never committed */
            static int occ_max, occ_full, occ_moves;
            static long occ_sum;
            static u32 occ_prev[4];
            int busy = 0;
            for (u32 s = 0; s < 4u; s++) {
                u32 hs = DSD(DS_0010286C + s * 0x18u);
                if (AIL_sample_status(sound_slot_handle(s)) == 4) busy++;
                if ((occ_prev[s] == 0x0383B6F4u || occ_prev[s] == 0x03837440u)
                    && hs != occ_prev[s]) occ_moves++;
                occ_prev[s] = hs;
            }
            if (busy > occ_max) occ_max = busy;
            if (busy == 4) occ_full++;
            occ_sum += busy;
            if (i == frames)
                fprintf(stderr, "OCCUPANCY frames=%d max=%d full_frames=%d "
                        "mean_x100=%ld loop_slot_moves=%d\n", frames, occ_max,
                        occ_full, occ_sum * 100 / frames, occ_moves);
        }
'''
open(p, 'w').write(s.replace(anchor, anchor + probe))
PY
cmake --build build 2>&1 | grep -E 'warning|error'
rm -rf $D/occ-$1; mkdir -p $D/occ-$1
(cd $D/occ-$1 && "$R/build/prageport" --game-dir "$R/data/game/C" --check 8000 > log.txt 2>&1; echo CHECK=$? >> log.txt)
grep -E '^OCCUPANCY|^CHECK=' $D/occ-$1/log.txt | tee $D/occ-$1.txt
cp $D/main.c.keep port/src/main.c && cmp -s $D/main.c.keep port/src/main.c && echo MAIN-RESTORED
cmake --build build 2>&1 | grep -E 'warning|error'; rm -rf $D/occ-$1/frames
SH
sh $D/occ.sh before
```
Expected:
- `CHECK=0`.
- An `OCCUPANCY` line. k7-k12 §7.6 *inferred* that the four slots fill
  (BD/BE/BF stuck at 4, then the second attract's 0x40). It never measured
  it. This line is that measurement. Expect `max=4` with `full_frames` > 0.
- `MAIN-RESTORED`.

Why the probe cannot change what it measures: `AIL_sample_status` computes
the state from `mixer_sample_active`, and it is called here only after
`game_loop()`. `loop_slot_moves` counts the frames on which a slot that held
0x40's or 0x42's handle changed. That includes the title's legitimate stops
(0x41/0x43), so it is compared before/after, not read as a count of
evictions. If `max` is below 4, do not halt. Record it in §D.0 as a
correction to §7.6's inference (the measurement wins). Task 3's acceptance
then rests on `mean_x100` and on V6/V7.

- [ ] **Step 6: Start the record.** Create
  `docs/superpowers/plans/2026-09-30-named-gaps-d-derivations.md` with a title
  line `# Named-gaps D derivation record: the virtual mixer clock (G6)` and a
  section `## §D.0 Baseline and the raw`. Fill it from this task's outputs:
  - **Baseline:** the commit (`git rev-parse --short HEAD`), `EXIT=0`, the
    four identity results, the CHECK-site count, and the
    `port_progress.py` line.
  - **The raw:** the confirmed lines of Step 3 with addresses, and
    `records.txt` verbatim. Add the end-tick table of Finding F9: BD 42,
    BE 52, BF 60, 0x2B11 bytes 61, 64 bytes 1. State their arithmetic: the
    step `(0x2B11 << 16) / 49716 = 14533`; `reads = ceil(len*65536/14533) + 1`;
    `k` is the least with `floor((frac + 49716*k)/60) >= reads`, the same for
    `frac` 0 and 59.
  - **Correction to the spec (raw wins):** §4.D says "no game code reads
    sample status (only `main.c`'s probe does)". The raw's `0x1CE70`,
    `0x1CE04`, `0x1CD9C`, `0x1CED4` and `0x1CC28` call `0x5DD03`, and
    `0x2C3FC` cases 2/3/4 branch on `0x1CE70`. Paste `$D/readers.txt`. Then
    give the bounded-exposure argument of Finding F11 (loader draws happen
    only on first resolve; no other slot reader; no rng). This is the claim
    Task 3's gate proves.
  - **Correction to the brief:** the tick is 60 Hz (`0x1CFFA push 0x3c`).
    The 60.05 Hz is a DOSBox measurement (`host.c:22-35`), not a raw
    constant.
  - **Before occupancy:** `$D/occ-before.txt` verbatim, with the probe's text
    and why it cannot perturb the run.

- [ ] **Step 7: Commit (if authorised).**

```bash
git add docs/superpowers/plans/2026-09-30-named-gaps-d-derivations.md
git commit -m "docs: named-gaps D baseline, raw and status readers (record §D.0)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```
Append `Task 1 done: baseline, raw, readers, occ-before` to the SDD ledger.

---

### Task 2: The virtual mixer clock, test first

**Files:**
- Modify: `port/tests/test_audio.c`: five includes, and the new block with
  `int test_virtual_clock(void)` appended at the end of the file.
- Modify: `port/tests/test.h`: `X(test_virtual_clock)` as the last `TEST_CASES` line.
- Modify: `port/src/game/flow.c`, only these lines:
  - the `s_last_isr_tick` static after `s_last_host_tick` (line ~96);
  - one line in `game_audio_init` (after `s_last_host_tick = host_tick_count();`, line ~5912);
  - `game_audio_service`'s header comment (lines ~5981-5983) and body (lines ~6007-6008).
- Modify (comments only): `port/src/platform/audio/ail.c` (lines ~366-369) and `port/src/main.c` (lines ~130-132).
- Modify: `docs/superpowers/plans/2026-09-30-named-gaps-d-derivations.md` (§D.1, §D.2).

**Interfaces:**
- Consumes: `game_audio_service`, `game_isr_ticks`, `sound_voice`,
  `sound_slot_handle`, `sound_buffers_alloc` (`game/flow.h`); `res_resolve`
  (`platform/res.h`); `tf_voice_snap`/`tf_voice_put` (`test_fixtures.h`); the
  AIL and mixer calls (`ail.h`, `mixer.h`).
- Produces: `int test_virtual_clock(void)` (registered). In `flow.c`,
  `static u32 s_last_isr_tick`. No new public API.

- [ ] **Step 1: Write the failing test.** In `port/tests/test_audio.c`, add these
  includes after `#include "platform/audio/ail.h"`:

```c
#include "mem.h"
#include "symbols.h"
#include "game/flow.h"
#include "platform/res.h"
#include "test_fixtures.h"
```

Append this block at the end of the file:

```c
/* ---- named-gaps D: the virtual mixer clock (record
 * 2026-09-30-named-gaps-d-derivations.md §D.1) ----------------------------- */

/* Service ticks after which a one-shot of `len` 8-bit bytes at 0x2B11 Hz
 * (0x1CBA4/0x1CBB0) has ended, for a sub-tick remainder `frac0` in [0, 59].
 * It restates mixer.c: step = (0x2B11 << 16) / MIXER_OPL_RATE (rate_step);
 * N reads leave idx = (N * step) >> 16, and the voice ends on the read after
 * the first N with N * step >= len << 16 (voice_read); k service ticks render
 * (frac0 + MIXER_OPL_RATE * k) / 60 frames (game_audio_service). */
static u32 vc_end_tick(u32 len, u32 frac0)
{
    const unsigned long long step = (0x2B11ull << 16) / MIXER_OPL_RATE;
    const unsigned long long reads =
        (((unsigned long long)len << 16) + step - 1u) / step + 1u;
    u32 k = 0;
    while ((frac0 + (unsigned long long)MIXER_OPL_RATE * k) / 60u < reads) k++;
    return k;
}

/* The raw's length in ticks: len / 0x2B11 s at 60 ticks/s (0x1CFFA), rounded
 * up. */
static u32 vc_ideal_ticks(u32 len)
{
    return (len * 60u + 0x2B10u) / 0x2B11u;
}

/* n timer ticks (0x1BE16, game_isr_ticks), each followed by the master loop's
 * audio service 0x1CF20. */
static void vc_ticks(u32 n)
{
    for (u32 i = 0; i < n; i++) {
        game_isr_ticks(1u);
        game_audio_service();
    }
}

/* The slot whose playing handle (+0x0C) is `h`, or -1. */
static int vc_slot_of(u32 h)
{
    for (u32 i = 0; i < 4u; i++)
        if (DSD(DS_0010286C + i * 0x18u) == h) return (int)i;
    return -1;
}

/* The size dword `h` resolves to (0x1CB49); 0 when it does not resolve. */
static u32 vc_len(u32 h)
{
    const u8 *p = (const u8 *)res_resolve(h);
    return (p != NULL) ? DSD((u32)(p - mem)) : 0u;
}

/* Every voice stopped and each slot handle inited (status 2), nothing queued
 * or playing, the DIG driver on and samples unpaused; then one service call
 * with no new tick rebases the clock. */
static void vc_quiet(void)
{
    mixer_stop_samples();
    for (u32 i = 0; i < 4u; i++) {
        AIL_init_sample(sound_slot_handle(i));
        DSD(DS_00102864 + i * 0x18u) = 0;
        DSD(DS_0010286C + i * 0x18u) = 0;
        DSD(DS_00102874 + i * 0x18u) = 0;
    }
    DSD(DS_001028C8) = 1u;
    DSB(DS_001028DB) = 0;
    game_audio_service();
}

int test_virtual_clock(void)
{
    int before = g_failures;
    static u8 vc_pcm[0x2B11];           /* one second at 0x2B11 Hz */
    static u8 vc_short[64];
    u32 i;

    for (i = 0; i < 4u; i++)
        CHECK(sound_slot_handle(i) != NULL, "game_audio_init allocated the slot handles");
    if (sound_slot_handle(0u) == NULL) return g_failures - before;
    tf_voice_snap();                    /* restored by tf_voice_put below */
    DSD(DS_001028C8) = 1u;
    if (DSD(DS_00102870) == 0u) {       /* 0x1D0BC's buffers, as test_flow makes them */
        DSB(DS_000A2CB0) = 0;
        CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    }
    for (i = 0; i < 4u; i++)
        CHECK(DSD(DS_00102870 + i * 0x18u) != 0u, "0x1D0BC gave every slot a buffer");

    /* The shipped records and sizes (record §D.0): BD is 0x42's handle
     * 0x03837440 as a one-shot, BE/BF are one-shots, all case 2 in entry 7
     * (S16TITLE.GRA). Resolved here, before any clock rebase. */
    CHECK_EQ_INT((int)DSD(DS_000BBDC8 + 0xBDu * 12u + 4u), 0x03837440);
    CHECK_EQ_INT((int)DSD(DS_000BBDC8 + 0xBEu * 12u + 4u), 0x038391C9);
    CHECK_EQ_INT((int)DSD(DS_000BBDC8 + 0xBFu * 12u + 4u), 0x038416A6);
    for (i = 0xBDu; i <= 0xBFu; i++) {
        CHECK_EQ_INT((int)DSB(DS_000BBDC8 + i * 12u), 2);
        CHECK_EQ_INT((int)DSB(DS_000BBDC8 + i * 12u + 8u), 0);
    }
    const u32 len_bd = vc_len(0x03837440u);
    const u32 len_be = vc_len(0x038391C9u);
    const u32 len_bf = vc_len(0x038416A6u);
    CHECK_EQ_INT((int)len_bd, 7557);
    CHECK_EQ_INT((int)len_be, 9511);
    CHECK_EQ_INT((int)len_bf, 10904);

    /* V1: a 0x2B11-byte one-shot (AIL's default loop count 1) on slot 0's
     * handle still plays one tick before its computed end and has ended at it:
     * 60 ticks of length, + 1 for rate_step's floor (record §D.0). */
    HSAMPLE h = sound_slot_handle(0u);
    const u32 lo = vc_end_tick(sizeof vc_pcm, 59u);
    const u32 hi = vc_end_tick(sizeof vc_pcm, 0u);
    CHECK_EQ_INT((int)hi, 61);
    CHECK(lo == hi && hi >= vc_ideal_ticks(sizeof vc_pcm)
          && hi <= vc_ideal_ticks(sizeof vc_pcm) + 1u,
          "a one-shot ends within one tick of len / 0x2B11 s, for any remainder");
    memset(vc_pcm, 0x90, sizeof vc_pcm);
    vc_quiet();
    AIL_init_sample(h);
    AIL_set_sample_address(h, vc_pcm, sizeof vc_pcm);
    AIL_set_sample_rate(h, 0x2B11u);
    AIL_start_sample(h);
    CHECK_EQ_INT((int)AIL_sample_status(h), 4);
    vc_ticks(lo - 1u);
    CHECK_EQ_INT((int)AIL_sample_status(h), 4);
    vc_ticks(hi - (lo - 1u));
    CHECK_EQ_INT((int)AIL_sample_status(h), 2);
    CHECK_EQ_INT(mixer_sample_active(h), 0);

    /* V2: the same buffer with loop count 0 (0x1CBE1) still plays after three
     * one-shot lengths. */
    vc_quiet();
    AIL_init_sample(h);
    AIL_set_sample_address(h, vc_pcm, sizeof vc_pcm);
    AIL_set_sample_loop_count(h, 0u);
    AIL_start_sample(h);
    vc_ticks(3u * hi);
    CHECK_EQ_INT((int)AIL_sample_status(h), 4);
    CHECK_EQ_INT(mixer_sample_active(h), 1);

    /* V3: a clock moved back (a mem[] restore) renders nothing; the next
     * forward tick renders, and 64 bytes end inside it. */
    memset(vc_short, 0x90, sizeof vc_short);
    CHECK_EQ_INT((int)vc_end_tick(sizeof vc_short, 0u), 1);
    vc_quiet();
    AIL_init_sample(h);
    AIL_set_sample_address(h, vc_short, sizeof vc_short);
    AIL_start_sample(h);
    DSD(DS_00101500) -= 5u;
    game_audio_service();
    CHECK_EQ_INT((int)AIL_sample_status(h), 4);
    vc_ticks(1u);
    CHECK_EQ_INT((int)AIL_sample_status(h), 2);

    /* V6: six one-shots (BE, BF alternating) across ticks with both attract
     * loops live, eight starts on four slots: each fires (0x2C483 finds it
     * ended), takes a free slot and frees it at its length (the dispatcher
     * never reaches 0x1CC28's forced arm), and the loops keep their slots. */
    vc_quiet();
    CHECK_EQ_INT((int)sound_voice(0x40u), 1);
    CHECK_EQ_INT((int)sound_voice(0x42u), 1);
    vc_ticks(1u);
    const int s40 = vc_slot_of(0x0383B6F4u);
    const int s42 = vc_slot_of(0x03837440u);
    CHECK(s40 >= 0 && s42 >= 0 && s40 != s42, "the attract loops started in two slots");
    for (u32 n = 0; n < 6u; n++) {
        const u32 id = (n & 1u) ? 0xBFu : 0xBEu;
        const u32 hd = (n & 1u) ? 0x038416A6u : 0x038391C9u;
        const u32 len = (n & 1u) ? len_bf : len_be;
        CHECK(vc_end_tick(len, 0u) == vc_end_tick(len, 59u)
              && vc_end_tick(len, 0u) >= vc_ideal_ticks(len)
              && vc_end_tick(len, 0u) <= vc_ideal_ticks(len) + 1u,
              "the one-shot's end is within one tick of len / 0x2B11 s");
        CHECK_EQ_INT((int)sound_voice(id), 1);
        vc_ticks(1u);
        const int s = vc_slot_of(hd);
        CHECK(s >= 0 && s != s40 && s != s42, "the one-shot took a free slot");
        if (s < 0) break;
        CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle((u32)s)), 4);
        vc_ticks(vc_end_tick(len, 0u) - 1u);
        CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle((u32)s)), 2);
    }
    if (s40 >= 0 && s42 >= 0) {
        CHECK_EQ_INT((int)DSD(DS_0010286C + (u32)s40 * 0x18u), 0x0383B6F4);
        CHECK_EQ_INT((int)DSD(DS_0010286C + (u32)s42 * 0x18u), 0x03837440);
        CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle((u32)s40)), 4);
        CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle((u32)s42)), 4);
    }

    /* V7: while BD (0x42's handle, one-shot) plays, 0x2C483 refuses 0x42;
     * once BD has ended 0x42 queues, and its loop outlives a BD length. */
    vc_quiet();
    CHECK(vc_end_tick(len_bd, 0u) == vc_end_tick(len_bd, 59u)
          && vc_end_tick(len_bd, 0u) >= vc_ideal_ticks(len_bd)
          && vc_end_tick(len_bd, 0u) <= vc_ideal_ticks(len_bd) + 1u,
          "BD's end is within one tick of len / 0x2B11 s");
    CHECK_EQ_INT((int)sound_voice(0xBDu), 1);
    vc_ticks(1u);
    CHECK_EQ_INT((int)sound_voice(0x42u), 0);
    vc_ticks(vc_end_tick(len_bd, 0u) - 1u);
    CHECK_EQ_INT((int)sound_voice(0x42u), 1);
    vc_ticks(1u);
    const int sl = vc_slot_of(0x03837440u);
    CHECK(sl >= 0, "0x42 started once BD ended");
    vc_ticks(vc_end_tick(len_bd, 0u));
    if (sl >= 0)
        CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle((u32)sl)), 4);

    mixer_stop_samples();
    for (i = 0; i < 4u; i++) AIL_init_sample(sound_slot_handle(i));
    tf_voice_put();
    return g_failures - before;
}
```

In `port/tests/test.h`, change the last `TEST_CASES` line from
`    X(test_fight_voice_sites)` to:

```c
    X(test_fight_voice_sites) \
    X(test_virtual_clock)
```

- [ ] **Step 2: Run it and watch it fail.**

```bash
D=.superpowers/sdd/2026-09-30-named-gaps-d-virtual-clock/scratch
cmake --build build 2>&1 | grep -E 'warning|error'
./build/run_tests > $D/t2-red.txt 2>&1; echo EXIT=$?
grep -c '^FAIL port/tests/test_audio.c' $D/t2-red.txt; grep '^FAIL' $D/t2-red.txt | grep -v test_audio.c
```
Expected:
- No warning or error lines. The run exits non-zero.
- **14** `FAIL port/tests/test_audio.c:` lines, from this code's missing
  render (the M1 state):
  - V1: `4 != 2` (the end check) and `1 != 0` (`mixer_sample_active`).
  - V3: `4 != 2` (the forward tick).
  - V6: `4 != 2` at n = 0 and n = 1. At n = 2..5 there is `0 != 1` (the
    dispatcher refuses a stuck handle), then `4 != 2`. That is 10 lines.
  - V7: `0 != 1` (0x42 refused after BD's length).
- No `FAIL` outside `test_audio.c`.

If the count differs, record the measured list in §D.2 and explain each
line before going on.

- [ ] **Step 3: Implement the clock in `flow.c`.**

(a) After `static u32 s_last_host_tick;     /* host clock at the last service call */` add:

```c
/* PORT: the ISR tick DS_00101500 at the last service call; the virtual
 * mixer clock's base (game_audio_service). */
static u32 s_last_isr_tick;
```

(b) In `game_audio_init`, after `    s_last_host_tick = host_tick_count();` add:

```c
    s_last_isr_tick = DSD(DS_00101500);
```

(c) In the header comment of `game_audio_service`, replace

```c
 * wall time without bursting. With no
 * device (host_audio_rate() == 0, e.g. --check) the sequencer still advances
 * but nothing is rendered or submitted. */
```
with
```c
 * wall time without bursting. With no
 * device (host_audio_rate() == 0, e.g. --check) the sequencer still advances
 * and the mixer renders on the virtual clock below; nothing is submitted. */
```

(d) In the body, replace

```c
    u32 rate = host_audio_rate();
    if (rate == 0) return;
```
with
```c
    /* PORT: the virtual mixer clock (record named-gaps-d §D.1). With no
     * device the mixer still renders, so a one-shot's voice reaches its
     * buffer end and AIL_sample_status reports it ended, as the DIG service
     * does at 0x6F28F. The frames due come from the ISR tick DS_00101500
     * (0x1BE16; 60 Hz, 0x1CFFA push 0x3c) at MIXER_OPL_RATE, the fixed
     * profile rate run_windowed opens a device with; mixer_render and
     * host_audio_submit (a no-op without a device) are the device path's
     * own. The host tick is wall time; the ISR tick is the loop's
     * deterministic clock (the spin 0x256C5 and res.c's read stall advance
     * it). A tick that moved back (a test restoring mem[]) rebases without
     * rendering; a stall is clamped like the host clock's. */
    u32 isr = DSD(DS_00101500);
    u32 isr_elapsed = isr - s_last_isr_tick;
    s_last_isr_tick = isr;
    if (isr_elapsed > 0x7FFFFFFFu) isr_elapsed = 0;
    if (isr_elapsed > HOST_TICK_MAX_CATCHUP) isr_elapsed = HOST_TICK_MAX_CATCHUP;

    u32 rate = host_audio_rate();
    if (rate == 0) {
        rate = MIXER_OPL_RATE;
        elapsed = isr_elapsed;
    }
```
The lines after it (`s_audio_frac += rate * elapsed;` through
`host_audio_submit(s_audio_buf, (int)n);`) stay as they are. With a device
nothing changes: `elapsed` is still the host-tick delta, and only the
bookkeeping `s_last_isr_tick` moves.

- [ ] **Step 4: Update the two stale comments.**

In `port/src/platform/audio/ail.c` (`AIL_sample_status`), replace
```c
     * playing handle with no live voice has ended. With no device the mixer
     * is not rendered and a started sample stays 4 (named gap, §0.7.6). */
```
with
```c
     * playing handle with no live voice has ended. With no device the mixer
     * renders on game_audio_service's virtual clock, so the end is reached
     * headless too (record named-gaps-d §D.1). */
```

In `port/src/main.c` (`attract_loop_playing`'s comment), replace
```c
 * 1 when a slot holds `h` as playing (+0x0C and AIL status 4). No device is
 * open, so the mixer is not rendered and a started voice stays live, which is
 * the loop's own state. */
```
with
```c
 * 1 when a slot holds `h` as playing (+0x0C and AIL status 4). No device is
 * open, so the mixer renders on the virtual clock (record named-gaps-d §D.1):
 * a one-shot ends at its length, and these loops (loop count 0, 0x1CBE1)
 * stay live until the title stops them. */
```

- [ ] **Step 5: Run it green.**

```bash
D=.superpowers/sdd/2026-09-30-named-gaps-d-virtual-clock/scratch
cmake --build build 2>&1 | grep -E 'warning|error'
./build/prageport --game-dir data/game/C --check 30 >/dev/null 2>&1; echo CHECK30=$?
PR_ORACLE_REQUIRED=1 ./build/run_tests > $D/t2-green.txt 2>&1; echo EXIT=$?
grep -c '^FAIL' $D/t2-green.txt
rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l
```
Expected:
- No warning or error lines. `CHECK30=0`, `EXIT=0`, and `0` FAIL lines.
- The CHECK-site count is `$D/checks-base.txt` + 40.

If an **existing** assertion now fails, **halt**. That would be a pre-existing
test that relied on a one-shot never ending headless. Report the file:line
and the values; do not edit the existing assertion.

- [ ] **Step 6: Mutation proofs.** Each mutation runs on a copy and is
  restored byte-for-byte. `git diff --stat` must match Step 5's afterwards.

```bash
D=.superpowers/sdd/2026-09-30-named-gaps-d-virtual-clock/scratch
cp port/src/game/flow.c $D/flow.c.good; cp port/src/platform/audio/mixer.c $D/mixer.c.good
mut() { # file old new tag
python3 - "$1" "$2" "$3" <<'PY'
import sys
p, old, new = sys.argv[1], sys.argv[2].encode().decode('unicode_escape'), sys.argv[3].encode().decode('unicode_escape')
s = open(p).read(); assert s.count(old) == 1, old; open(p, 'w').write(s.replace(old, new))
PY
cmake --build build 2>&1 | grep -E 'warning|error'
./build/run_tests > $D/mut-$4.txt 2>&1; grep '^FAIL' $D/mut-$4.txt | tee $D/mut-$4.fail | wc -l
cp $D/flow.c.good port/src/game/flow.c; cp $D/mixer.c.good port/src/platform/audio/mixer.c
}
mut port/src/game/flow.c '    if (rate == 0) {\n        rate = MIXER_OPL_RATE;\n        elapsed = isr_elapsed;\n    }\n' '    if (rate == 0) return;\n' M1
mut port/src/game/flow.c '        elapsed = isr_elapsed;\n' '' M2
mut port/src/game/flow.c '    if (isr_elapsed > 0x7FFFFFFFu) isr_elapsed = 0;\n' '' M3
mut port/src/game/flow.c '        elapsed = isr_elapsed;\n' '        elapsed = isr_elapsed * 2u;\n' M4
mut port/src/platform/audio/mixer.c '        if (v->loop) {' '        if (0) {' M5
cmake --build build 2>&1 | grep -E 'warning|error'; git diff --stat
```
Expected (the FAIL line text is `FAIL port/tests/test_audio.c:<line>: <a> != <b>`):

| Mutation | What it breaks | Expected FAIL lines |
|---|---|---|
| M1 | no render without a device (today's code) | the same 14 as Step 2 |
| M2 | virtual rate, but the host-tick delta (wall clock; the suite never pumps it) | the same 14 as Step 2 |
| M3 | no backward-tick guard | V3 `2 != 4` (the call after `DS_00101500 -= 5`). A `test_game.c` line may also fail, where a data-object restore moves the tick back before a service call; list and explain any such line |
| M4 | a doubled clock (ends too early) | 1: V1 `2 != 4` (one tick before the end) |
| M5 | loops end like one-shots | V2 `2 != 4` and `0 != 1`; V6's loop block (0x42 ends at 42 ticks, so a one-shot can take `s42`); V7's last `2 != 4`; plus `test_game.c`'s existing 0x40 loop check in `test_flow` |

Record each measured `$D/mut-*.fail` in §D.2 verbatim. A mutation with
**zero** FAIL lines means the assertion cannot fail: **halt**. After the last
restore, rebuild and re-run Step 5's `run_tests`, which must give 0 FAIL.

- [ ] **Step 7: Record §D.1 and §D.2.** In the derivation record, add these
  two sections.

  `## §D.1 The clock`:
  - The design sentence from this plan's Architecture paragraph.
  - The code of Step 3(d).
  - Why the ISR tick (F6) and why no host.c change (F7). The device path
    pushes, so no "frames consumed" count exists to mirror.
  - The time base: 60 Hz at `0x1CFFA`, with `MIXER_OPL_RATE` from
    `mixer.h:50` and `main.c:30`.
  - The clamp is the existing `HOST_TICK_MAX_CATCHUP`.
  - The backward guard is a `PORT:` guard with no raw counterpart. In the
    raw the ISR counter only grows.

  `## §D.2 Tests and mutations`:
  - V1..V7 (there is no V4/V5; the names follow this plan's Review Focus).
  - Step 2's red list, and Step 6's table with the measured FAIL lines.
  - A **Not tested** list:
    - The device path: SDL audio cannot open on this host.
    - The 30-tick clamp: a res.c stall of more than 30 ticks happens only on
      a large first read. The clamp is shared with the host clock and has no
      separate test here.
    - `s_last_isr_tick`'s init in `game_audio_init`: `game_init` runs once
      per process. V3's rebase covers the equivalent path.
    - `sound_sfx_volume`, `snd_sample_stop` and `snd_samples_stop_all` on a
      naturally ended slot. Their status reads are covered by `test_game.c`
      on forced statuses.

- [ ] **Step 8: Commit (if authorised).**

```bash
git add port/src/game/flow.c port/src/platform/audio/ail.c port/src/main.c port/tests/test_audio.c port/tests/test.h docs/superpowers/plans/2026-09-30-named-gaps-d-derivations.md
git commit -m "audio: virtual mixer clock ends headless one-shots at their length (record named-gaps-d §D.1)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```
Append `Task 2 done: red 14, green 0, mutations M1-M5` to the SDD ledger.

---

### Task 3: Final gate, the "after" occupancy, ledger, PROGRESS, closure

**Files:**
- Modify: `docs/superpowers/plans/2026-09-30-named-gaps-d-derivations.md` (§D.3).
- Modify: `docs/superpowers/plans/2026-09-29-all-gaps-ledger.md` (§H.3 row 6 and the open-gap count sentence).
- Modify: `docs/PROGRESS.md` (one appended paragraph).
- Temporarily modify, then restore byte-for-byte (never committed): `port/src/main.c` (the occupancy probe, as in Task 1 Step 5).

**Interfaces:**
- Consumes: Task 2's tree; the `$K` baselines; `$D/occ-before.txt`.
- Produces: `$D/verify-t3.txt`, `$D/occ-after.txt`, `$D/occ-after2.txt`, record §D.3, ledger/PROGRESS closure.

- [ ] **Step 1: The full gate.**

```bash
K=.superpowers/sdd/2026-09-29-k7-k12/scratch; D=.superpowers/sdd/2026-09-30-named-gaps-d-virtual-clock/scratch
make clean >/dev/null 2>&1; make build 2>&1 | grep -E 'warning|error'
make verify SMK_DUMP=/tmp/pr_d3_smk TITLE_DUMP=/tmp/pr_d3_title ATTRACT_DUMP=/tmp/pr_d3_attract FRONTEND_DUMP=/tmp/pr_d3_frontend TITLE_PIN_DIR=/tmp/pr_d3_pin AUDIO_WAV=/tmp/pr_d3_fm.wav 2>&1 | tee $D/verify-t3.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $D/verify-t3.txt | diff $K/oracle-lines-base.txt - && echo ORACLES-EQUAL
sh $K/dumps.sh d-after && sh $K/dumpsha.sh d-after && echo DUMPS-IDENTICAL
grep -E 'CHECK|assertion' $K/d-after/check/log.txt | head -5
make audio-render AUDIO_WAV=$D/after.wav >/dev/null 2>&1; cmp $K/before-t2.wav $D/after.wav && echo WAV-IDENTICAL
python3 tools/port_progress.py | diff $D/progress-base.txt - && echo PROGRESS-UNCHANGED
```
Expected:
- No warning or error lines.
- `EXIT=0`, `ORACLES-EQUAL`, `DUMPS-IDENTICAL`, `WAV-IDENTICAL` and
  `PROGRESS-UNCHANGED`. No function is added, so the README percentage does
  not move.
- The `--check 8000` log inside the dump has no assertion-failure line.
  `dumps.sh` prints `--check exited non-zero` otherwise, which must not
  appear.

If any line moves, **halt** and report the old value, the new value and the
diff. This is the proof of Finding F11: game code reads the status, so it is
the gate, not an argument, that shows the frames do not move. Then
`rm -rf $K/d-after`.

- [ ] **Step 2: Measure the "after" occupancy, twice.** Run the Task 1
  Step 5 script twice. It restores `main.c` byte-for-byte from the copy it
  takes, so Task 2's comment edit survives.

```bash
D=.superpowers/sdd/2026-09-30-named-gaps-d-virtual-clock/scratch
sh $D/occ.sh after; sh $D/occ.sh after2
diff $D/occ-after.txt $D/occ-after2.txt && echo DETERMINISTIC
git diff --stat -- port/src/main.c
```
Expected:
- `CHECK=0` in both, `MAIN-RESTORED` twice, and `DETERMINISTIC`. The
  `git diff --stat` is empty when Task 2 was committed; otherwise it shows
  only Task 2's comment edit.
- `mean_x100` is lower than `$D/occ-before.txt`'s. When the before line
  had `full_frames` > 0, the after line's `full_frames` is lower too.

If either condition fails, the saturation still occurs: **halt** and report
both lines. Four concurrent one-shots can legitimately fill the slots for a
few frames, so `max=4` alone is not a failure. The criterion is that the
persistent occupancy is gone: slots held by ended one-shots used to stay at
4 for the rest of the run, and now they return to 2.

- [ ] **Step 3: Record §D.3.** Append `## §D.3 Gate and closure` to the record:
  - `EXIT=0` and the four identities, with the file names.
  - A before/after occupancy table: `$D/occ-before.txt`, `$D/occ-after.txt`,
    and the determinism diff.
  - The `--check 8000` exit code.
  - The closure statement: "G6 closed. With no device the mixer renders on
    the ISR tick's virtual clock (`game_audio_service`), so a one-shot ends
    at its length (V1: 61 ticks for 0x2B11 bytes; BD/BE/BF at 42/52/60) and
    its slot frees; the attract loops keep playing (V2, V6, V7, the
    `--check` probe)."
  - The spec correction of §D.0 (game code reads the status), as a
    "raw wins" line.

- [ ] **Step 4: Close the ledger row.** In
  `docs/superpowers/plans/2026-09-29-all-gaps-ledger.md` §H.3, append this to
  row 6's Evidence cell:
  ` **Closed** (named-gaps D, 2026-09-30): with no device the mixer renders on a virtual clock from the ISR tick DS_00101500 at MIXER_OPL_RATE (flow.c game_audio_service); one-shots end at their length and slots free (record 2026-09-30-named-gaps-d-derivations.md §D.1-§D.3; test_virtual_clock).`
  Then find the sentence that counts the open named gaps (on `all-gaps-final`
  it reads `**Ten named gaps** are open`). Lower its count by one, and add
  "(#6 closed by named-gaps D)" to its list. If B or C already changed the
  count, lower whatever count is there by one. Record the before/after
  sentence in §D.3.

```bash
rg -n "Headless sample slots never end|named gaps\*\* are open" docs/superpowers/plans/2026-09-29-all-gaps-ledger.md
```
Expected: the row shows the Closed text; the count sentence has the new
number.

- [ ] **Step 5: PROGRESS paragraph.** Append one paragraph to `docs/PROGRESS.md`.
  Take its numbers from `$D/occ-before.txt`, `$D/occ-after.txt` and
  `$D/verify-t3.txt`:

  > **Named-gaps D: the virtual mixer clock (G6 closed).** With no audio
  > device, `game_audio_service` (`0x1CF20`) no longer returns before the
  > render. It takes the frames due from the ISR tick `DS_00101500`
  > (`0x1BE16`, 60 Hz from `0x1CFFA push 0x3c`) at the fixed profile rate
  > `MIXER_OPL_RATE`, and runs the device path's own `mixer_render` and
  > `host_audio_submit`. A one-shot therefore ends at its length, and
  > `AIL_sample_status` frees its slot, as the DIG service does at
  > `0x6F28F`. The host tick is not used because it is wall clock; a tick
  > that moves back rebases without rendering.
  >
  > In `--check 8000` the four slots were full on F1 frames (mean occupancy
  > M1/100) and are now full on F2 (M2/100). The measurement is identical
  > across two runs.
  >
  > The spec's "no game code reads sample status" was wrong: `0x1CE70`,
  > `0x1CE04`, `0x1CD9C`, `0x1CED4` and `0x1CC28` read it, and `0x2C3FC`
  > branches on `0x1CE70`. The gate shows the frames do not move.
  > `test_virtual_clock` pins the end tick from the raw lengths (BD/BE/BF:
  > 7557/9511/10904 bytes at `0x2B11` Hz, ending at 42/52/60 ticks), with
  > five mutations. `make verify` exits 0 with every oracle line equal to
  > §A. The frame dumps match the k7-k12 base manifest, and the
  > `make audio-render` WAV is byte-identical. The README's percentage
  > stands.

  F1, M1, F2 and M2 are the `full_frames` and `mean_x100` values of
  `occ-before.txt` and `occ-after.txt`. Write the numbers, not the letters.

- [ ] **Step 6: Commit (if authorised).**

```bash
git add docs/superpowers/plans/2026-09-30-named-gaps-d-derivations.md docs/superpowers/plans/2026-09-29-all-gaps-ledger.md docs/PROGRESS.md
git commit -m "docs: named-gaps D closure, G6 closed (record §D.3, ledger §H.3 row 6)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
git status --short
```
Expected: a clean status. Append `Task 3 done: gate green, G6 closed` to the
SDD ledger.

---

## Self-Review

- **Spec coverage (§4.D).**
  - "When `host_audio` has no device, the mixer's sample position advances
    from the game tick at the profile's sample rate": Task 2 Step 3.
  - The FM WAV is byte-identical: Task 1 Step 2 and Task 3 Step 1. The WAV
    path calls `mixer_render` directly (F8), and this plan does not change
    `mixer.c`.
  - The `--check` loop probes for `0x40`/`0x42`: Task 2 Step 5 (`--check
    30`) and Task 3 Step 1 (`--check 8000` inside `dumps.sh`).
  - Oracle lines do not move: Task 3 Step 1.
  - Acceptance "a headless run frees a slot after a one-shot's length":
    V1, V6 and V7.
  - Acceptance "the saturation observed in `--check` no longer occurs":
    Task 3 Step 2, against Task 1 Step 5.
  - Acceptance "tests assert the end time from the sample's length": the
    `vc_end_tick`/`vc_ideal_ticks` bounds, on the raw lengths.
- **Spec conflicts, recorded rather than silently followed.**
  - §4.D's "no game code reads sample status" is false (F5). The plan
    audits the readers (Task 1 Step 4), bounds the exposure (F11) and lets
    the gate prove it (Task 3 Step 1).
  - The brief's 60.05 Hz is a DOSBox measurement. The raw sets 60 Hz (F6).
  - The brief suggested that the host seam supply a "frames consumed"
    count. The device path pushes and has no such count (F7), so the clock
    lives in the service, and `host.c` is untouched.
- **Placeholders.** None. Every code block is complete. The only values
  left to fill are measurements, each with the exact command that produces
  it.
- **Type consistency.**
  - `s_last_isr_tick`, `isr` and `isr_elapsed` are `u32`, like `elapsed`
    and `s_last_host_tick`.
  - `HOST_TICK_MAX_CATCHUP` is `30u`.
  - `MIXER_OPL_RATE` is an int literal, promoted in `rate = MIXER_OPL_RATE`
    exactly as `main.c:30` passes it.
  - `rate * elapsed` is at most 49716 × 30, which fits a `u32`.
  - In the test, `HSAMPLE` comes from `ail.h`, `res_resolve` from `res.h`,
    and the `DS_` names from `symbols.h`.
- **Test hygiene.**
  - Every status assertion's sentinel differs from its post-condition. The
    slots are inited to 2 before each start, 4 is asserted after the start,
    and 2 at the end.
  - No assertion reads an unseeded BSS zero.
  - The test snapshots and restores the data object (`tf_voice_snap`/`put`)
    and stops every voice before it returns.
  - It is registered last, after the WAV render.
  - Each assertion group has a mutation that fails it: M1..M5.
