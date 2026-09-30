# Named-gaps D derivation record: the virtual mixer clock (G6)

Plan: `docs/superpowers/plans/2026-09-30-named-gaps-d-virtual-clock.md`.
Spec: `docs/superpowers/specs/2026-09-30-named-gaps-design.md` §3.3, §4.D, §8.
Ledger: `docs/superpowers/plans/2026-09-29-all-gaps-ledger.md` §H.3 row 6.
Scratch (git-ignored): `D=.superpowers/sdd/2026-09-30-named-gaps-d-virtual-clock/scratch`,
`K=.superpowers/sdd/2026-09-29-k7-k12/scratch`.

## §D.0 Baseline and the raw

### Baseline

- Commit `af135ec` (main; contains `a169296` and `0247a1a`; ledger §H.3 row 6
  "Headless sample slots never end" present at line 619).
- `make build`: no warning or error lines.
- `make verify` (with the `pr_d1_*` overrides): `EXIT=0` (`$D/verify-base.txt`).
- Oracle lines equal `$K/oracle-lines-base.txt`: **ORACLES-EQUAL**.
- Frame dumps (`dumps.sh d-base` + `dumpsha.sh d-base`) equal `$K/base.sha256`:
  **DUMPS-IDENTICAL**.
- `make audio-render` WAV equals `$K/before-t2.wav`: **WAV-IDENTICAL**.
- Assertion sites (`rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l`):
  **13545**.
- `python3 tools/port_progress.py`: `767 1203 64` / `731 731 100 (portable:
  excludes 81 host-owned/deferred and runtime >= 5D000)`.

### The raw (fixup-applied image, `$K/dx.py`; `$D/dx-*.txt`)

- `0x1CB18` (`sound_sample_start`):
  - `1cb49: mov ecx, dword ptr [eax]` reads the resource's size dword.
  - `1cba4: push 0x2b11` / `1cbb0: call 0x5dca6` sets the rate to 11025 Hz.
  - `1cbca: mov al, byte ptr [ebp + 0x102868]`, `1cbd3: cmp eax, 1`,
    `1cbd6: jne 0x1cbe9`, `1cbd8: push 0`, `1cbe1: call 0x5dce4`: loop count
    0 only when the slot's loop byte is 1.
- `0x1CF40..0x1D0BC` (`0x1CF20`'s init neighbour):
  - `1cf6e: push 0x2b11` (preference 1, the DIG rate).
  - `1cfed: push 0x1bdf4`, `1cff2: call 0x5da12` registers the ISR;
    `1cffa: push 0x3c`, `1cfff: call 0x5da87` sets its frequency to **60 Hz**.
- `0x1BDF4` (the ISR): `1be08: mov ebx, dword ptr [0x101500]`, `1be0f: inc ebx`,
  `1be16: mov dword ptr [0x101500], ebx` is the clock's increment (and
  `1be02..1be10` the same for `0x101508`).
- `0x2C3FC` case 2: `2c483: call 0x1ce70`, `2c488: test al, al`,
  `2c48a: jne 0x2c8e8` precede `2c4b6: call 0x1cc28`: the queue is skipped
  when `0x1CE70` returns 1.
- Voice records `DS_000BBDC8 + id*12` read from `PRAGE.EXE` at file offset
  `0xBBDC8 + 0x46E54 + id*12`, sizes from `S16TITLE.GRA` at `handle & 0x7FFFFF`
  (`$D/records.txt`, verbatim):

```
0x40 case 2 handle 0x383b6f4 byte 1 entry 7 size 24494
0x42 case 2 handle 0x3837440 byte 1 entry 7 size 7557
0xbd case 2 handle 0x3837440 byte 0 entry 7 size 7557
0xbe case 2 handle 0x38391c9 byte 0 entry 7 size 9511
0xbf case 2 handle 0x38416a6 byte 0 entry 7 size 10904
```

### End ticks (Finding F9)

The mixer reads a voice at step `(0x2B11 << 16) / 49716 = 14533` (16.16,
`rate_step`, `MIXER_OPL_RATE` 49716). A one-shot of `len` frames is read
`reads = ceil(len*65536/14533) + 1` times (the last read finds `idx >= frames`
and clears the voice, `voice_read`). `k` service ticks render
`floor((frac + 49716*k)/60)` frames, `frac` the sub-tick remainder in 0..59.
The end tick is the least `k` with `floor((frac + 49716*k)/60) >= reads`,
measured for every `frac` 0..59 (the same value for all 60):

| Sample | len (bytes) | end tick | `ceil(len*60/11025)` |
|---|---|---|---|
| BD (0x42's handle, one-shot) | 7557 | 42 | 42 |
| BE | 9511 | 52 | 52 |
| BF | 10904 | 60 | 60 |
| 0x40 (as if one-shot) | 24494 | 134 | 134 |
| synthetic 0x2B11 bytes | 11025 | **61** | 60 |
| synthetic 64 bytes | 64 | 1 | 1 |

The 0x2B11-byte buffer ends one tick after its ideal length: the step's floor
(14533 < 11025*65536/49716 = 14533.4...) makes one second of source take
slightly more than one second of output.

### Correction to the spec (raw wins): game code reads sample status

§4.D says "no game code reads sample status (only `main.c`'s probe does)".
The raw and the port disagree (spec §8 already records this): `0x1CE70`,
`0x1CE04`, `0x1CD9C`, `0x1CED4` and `0x1CC28` (twice) call `0x5DD03`
(`AIL_sample_status`), and `0x2C3FC` cases 2/3 branch on `0x1CE70` (case 4
stops all through `0x1CD9C`). The audit (`$D/readers.txt`, verbatim):

```
port/src/main.c:137:            AIL_sample_status(sound_slot_handle(i)) == 4)
port/src/game/flow.c:6102:static s32 snd_slot_status(u32 off)
port/src/game/flow.c:6104:    return AIL_sample_status(s_samples[off / SND_SLOT_STRIDE]);
port/src/game/flow.c:6115:        if (snd_slot_status(off) == 4) return 1;           /* 0x1CE92/0x1CE9A */
port/src/game/flow.c:6129:        if (snd_slot_status(off) == 2) continue;           /* 0x1CE26/0x1CE2E */
port/src/game/flow.c:6147:        if (snd_slot_status(off) == 2) continue;           /* 0x1CDC2/0x1CDCA */
port/src/game/flow.c:6214:        if (snd_slot_status(off) != 4) continue;           /* 0x1CEEF 0x1CEF7 */
port/src/game/flow.c:6297:            && snd_slot_status(0u) != 4) {                 /* 0x1CC70..0x1CC94 */
port/src/game/flow.c:6308:                && snd_slot_status(off) != 4) {            /* 0x1CCCD..0x1CCF1 */
port/src/platform/audio/ail.h:151:s32 AIL_sample_status(HSAMPLE sample);
port/src/platform/audio/ail.c:362:s32 AIL_sample_status(HSAMPLE sample)
port/src/platform/audio/ail.c:370:    if (sample->state == 4 && !mixer_sample_active(sample))
port/src/platform/audio/mixer.h:76:int mixer_sample_active(const void *owner);
port/src/platform/audio/mixer.h:81:int mixer_active_voices(void);
port/src/platform/audio/mixer.c:122:int mixer_sample_active(const void *owner)
port/src/platform/audio/mixer.c:129:int mixer_active_voices(void)
port/src/main.c:136:        if (DSD(DS_0010286C + i * 0x18u) == h &&
port/src/game/flow.h:187: * DS_00102860 + i*0x18 and the port in flow.c; NULL out of range. Exposed so
22:#define RES_FLAG_LOADED  0x20000000u
211:            if (read_ok) DSD(table + i * RES_REC + 12) |= RES_FLAG_LOADED;
292:    if ((flags & (RES_FLAG_PRELOAD | RES_FLAG_LOADED)) == 0u) {
301:        DSD(entry + 12) = flags | RES_FLAG_LOADED;    /* 0x1B47A */
port/src/game/svcmenu.c:359:    return sound_voice(0x100u);                             /* 0x30F42..0x30F47 0x2C3FC */
port/src/game/svcmenu.c:378:    return sound_voice(0x100u);                             /* 0x30F42..0x30F47 0x2C3FC */
port/src/game/svcmenu.c:385:    return sound_voice(DSD(SVC_TUNE_VOICES + i * 4u));      /* 0x2C9D9..0x2C9E0 0x2C3FC */
port/src/game/svcmenu.c:392:    return sound_voice(DSD(SVC_SAMPLE_VOICES + i * 4u));    /* 0x2C9F5..0x2C9FC 0x2C3FC */
port/src/game/flow.c:6481:    return sound_voice((u32)DSW(SND_STAGE_VOICES + stage * 2u));  /* 0x4F71C..0x4F721, the read at 0x4F714 */
```

The set is the plan's: `AIL_sample_status` is called only by `snd_slot_status`
(six sites, all in the sound module) and `main.c`'s probe; the slot records
are read outside `flow.c` only by that probe (and `flow.h`'s comment);
`RES_FLAG_LOADED` has the define, two setters and the test, no clear;
`sound_voice`'s AL is read only by `sound_voice_stage` and `svcmenu.c`.

Bounded exposure (F11): a freed slot changes the slot records
`DS_00102860..DS_001028BF`, the slot buffers, and whether a case-2/3 voice
queues. A re-queue resolves a handle already resolved on its first queue
(`res_resolve` draws the loader and stalls the tick only when neither
`RES_FLAG_PRELOAD` nor `RES_FLAG_LOADED` is set, and `RES_FLAG_LOADED` is
never cleared), so no new loader draw or stall. The dispatcher reads no rng
(k7-k12 §0.6). So the frame dumps should not move. This is an argument; the
Task 3 gate is the proof.

### Correction to the brief: the tick is 60 Hz

The raw sets the timer at `0x1CFFA push 0x3c` = 60 Hz. The 60.05 Hz figure
(`host.c:22-35`) is a DOSBox measurement, not a raw constant.

### Correction to spec §8 (the tree wins): the port's spin does advance `DS_00101500`

Spec §8's G1 bullet says "The port's master-loop spin never advances
`DS_00101500`". On this tree (`af135ec`) it does: `game_loop`'s spin
(`flow.c:6806-6809`, the `0x256C5` loop) calls `game_isr_ticks(1u)`, which
adds to both `DS_00101508` and `DS_00101500` (`flow.c:6720-6724`,
`0x1BE0F/0x1BE16`), and res.c's modelled read stall calls it too
(`res.c:91`). The plan's Finding F6 matches the tree. The virtual clock
depends on this: it is what makes `--check` renders happen.

### Before occupancy (`$D/occ-before.txt`, verbatim)

```
OCCUPANCY frames=8000 max=4 full_frames=5430 mean_x100=331 loop_slot_moves=6
CHECK=0
```

The probe (`$D/occ.sh`) inserts, after `game_loop();` in `run_check`, a block
that counts per frame the slots whose `AIL_sample_status` is 4 (max, frames
with all four busy, mean x100) and the frames on which a slot that held
0x40's or 0x42's handle (+0x0C) changed. It restores `main.c` byte-for-byte
(`MAIN-RESTORED`). It cannot perturb the run: it runs after the frame, writes
no `mem[]`, and its only side effect is `AIL_sample_status`'s 4 -> 2 latch,
which the game's own next read would compute identically (the state is a
function of `mixer_sample_active`, which the probe does not change).
`max=4` with `full_frames=5430` of 8000 measures what k7-k12 §7.6 inferred:
the four slots saturate.

## §D.1 The clock

A virtual mixer clock inside `game_audio_service` (`0x1CF20`, `flow.c`). When
`host_audio_rate()` is 0, the service no longer returns before the render. It
takes the frames due from the ISR tick `DS_00101500` (`game_isr_ticks`,
60 Hz) at the fixed profile rate `MIXER_OPL_RATE`, and runs the **same**
`mixer_render` -> `host_audio_submit` path as a device run.
`host_audio_submit` is already a no-op without a device, so the frames are
discarded. A one-shot's voice goes inactive in `voice_read` at its buffer end,
and `AIL_sample_status` then reports 2 through its existing
`mixer_sample_active` check.

The code (`flow.c`, `game_audio_service`, replacing `if (rate == 0) return;`):

```c
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

plus `static u32 s_last_isr_tick` (a `PORT:` bookkeeping static beside
`s_last_host_tick`; the tick itself stays in `mem[]`) and its base
`s_last_isr_tick = DSD(DS_00101500);` in `game_audio_init`. The sequencer's
tick count is computed from the host-tick `elapsed` before this block, so the
music path is unchanged; with a device `elapsed` stays the host-tick delta and
only `s_last_isr_tick` moves.

- **Why the ISR tick (F6).** `host_tick_count()` is wall clock (`host_pump`
  advances it by elapsed `now_ns()` intervals), so a clock on it would free
  slots at jittered frames and make `--check`/driver runs nondeterministic.
  `DS_00101500` is advanced once per master-loop spin (`flow.c`'s `0x256C5`
  loop, `game_isr_ticks(1u)`) and by res.c's modelled read stall; `0x500BB`
  already reads it as the sound module's time.
- **Why no host.c change (F7).** With a device the port pushes
  (`host_audio_submit` -> `SDL_PutAudioStreamData`); voices advance when
  `mixer_render` runs, not when SDL consumes. There is no "frames consumed"
  count to mirror, so the clock lives in the service.
- **The time base.** 60 Hz at `0x1CFFA push 0x3c` / `0x1CFFF call 0x5DA87`
  (AIL_set_timer_frequency). `MIXER_OPL_RATE` 49716 (`mixer.h:50`), the rate
  `main.c:30` opens the device with.
- **The clamp** is the existing `HOST_TICK_MAX_CATCHUP` (30, `host.h:15`),
  shared with the host clock. `rate * elapsed` is at most 49716 x 30 and fits
  a `u32`.
- **The backward guard** (`isr_elapsed > 0x7FFFFFFF` -> 0) is a `PORT:` guard
  with no raw counterpart: in the raw the ISR counter only grows. In the port
  a test restoring the data object moves `DS_00101500` back.

## §D.2 Tests and mutations

`int test_virtual_clock(void)` in `port/tests/test_audio.c`, registered once as
the last `TEST_CASES` line (after the WAV render `test_sequencer` and every
slot reader). It runs on the unit-suite process (no `game_init()`), snapshots
and restores the data object (`tf_voice_snap`/`tf_voice_put`) and stops every
voice before it returns. Assertion sites: 13545 -> **13585** (+40, the plan's
count).

- **V1** (`:1668..1671`): a 0x2B11-byte one-shot on slot 0's handle is 4 one
  tick before its computed end (61, `vc_end_tick`, the same for remainder 0
  and 59, within `[ceil(len*60/0x2B11), +1]`) and 2 at it; the mixer voice is
  gone.
- **V2** (`:1681..1682`): the same buffer with loop count 0 (`0x1CBE1`) still
  plays after 3 x 61 ticks.
- **V3** (`:1694`, `:1696`): the tick moved back by 5 renders nothing (64
  bytes still 4); the next forward tick renders and they end (2).
- **V6** (`:1717..1730`): 0x40 and 0x42 through the dispatcher, then six
  one-shots BE/BF alternating: each is accepted by `0x2C483`, takes a slot
  that is neither loop's, is 4 after its start and 2 at its end tick (52/60);
  afterwards both loops keep their slots, handles and status 4.
- **V7** (`:1744..1750`): while BD (0x42's handle as a one-shot) plays,
  `0x2C483` refuses 0x42 (AL 0); after BD's 42 ticks 0x42 queues (AL 1),
  starts, and is still 4 a BD length later.
- The raw records and sizes (`:1636..1648`): BD/BE/BF handles, case 2, byte 0,
  and the resolved lengths 7557/9511/10904.

There is no V4/V5; the names follow the plan's Review Focus.

### Correction to the plan (the tree wins): the unit suite's slot handles are released

The plan assumed the unit-suite process still holds the four slot handles
`game_audio_init` allocated. It does not: `test_ail` (registered after
`test_flow`) ends with `AIL_shutdown()` and one `AIL_allocate_sample_handle`
(`test_audio.c`, "7. Shutdown releases everything"), so only `g_samples[0]`
is live and slots 1..3 report status 0 (`AIL_sample_status` on an unused
handle). The first red run showed it: 26 failures, among them V6's
`the one-shot took a free slot`, `0 != 4` / `0 != 2` on the slot status and
`58988198 != 58963700` (BF's handle in 0x40's slot, taken by `0x1CC28`'s
forced arm because all four slots read "not 4"). The test now re-takes the
released pool entries at its start (`AIL_allocate_sample_handle` takes the
first free entry, and the pool is exactly these four handles) and releases
them again at its end. No assertion was added or changed for this; the
handles' liveness is covered by V1's and V6's status-4 checks.

The plan's red `grep -c '^FAIL port/tests/test_audio.c'` matches nothing here:
the harness prints `FAIL <absolute path>:<line>`. The lists below strip the
prefix to `port/` and exclude the closing `FAILURES: N` line.

### Red (the code before Step 3; `$D/t2-red.fail`): 14 lines, the plan's 14

```
FAIL port/tests/test_audio.c:1670: 4 != 2
FAIL port/tests/test_audio.c:1671: 1 != 0
FAIL port/tests/test_audio.c:1696: 4 != 2
FAIL port/tests/test_audio.c:1724: 4 != 2
FAIL port/tests/test_audio.c:1724: 4 != 2
FAIL port/tests/test_audio.c:1717: 0 != 1
FAIL port/tests/test_audio.c:1724: 4 != 2
FAIL port/tests/test_audio.c:1717: 0 != 1
FAIL port/tests/test_audio.c:1724: 4 != 2
FAIL port/tests/test_audio.c:1717: 0 != 1
FAIL port/tests/test_audio.c:1724: 4 != 2
FAIL port/tests/test_audio.c:1717: 0 != 1
FAIL port/tests/test_audio.c:1724: 4 != 2
FAIL port/tests/test_audio.c:1744: 0 != 1
```

V1's end and voice, V3's forward tick, V6's end at n = 0 and 1, then at
n = 2..5 the dispatcher's refusal of the stuck handle and the end, and V7's
refused 0x42.

### Green

`--check 30` exits 0; `PR_ORACLE_REQUIRED=1 ./build/run_tests` exits 0 with
0 FAIL lines (`$D/t2-green.txt`); 0 compiler warnings.

### Mutations (`$D/mut-*.fail`; each restored byte-for-byte, `git diff --stat` unchanged, re-run green with 0 FAIL)

| Mutation | What it breaks | Measured FAIL lines |
|---|---|---|
| M1 `if (rate == 0) return;` | no render without a device (the old code) | the 14 red lines, identical |
| M2 drop `elapsed = isr_elapsed;` | virtual rate on the host-tick delta (the suite never pumps it) | the 14 red lines, identical |
| M3 drop the backward guard | the moved-back tick renders ~0xFFFFFFFB -> clamped 30 ticks | 1: `FAIL port/tests/test_audio.c:1694: 2 != 4` (no `test_game.c` line) |
| M4 `elapsed = isr_elapsed * 2u;` | a doubled clock | 1: `FAIL port/tests/test_audio.c:1668: 2 != 4` |
| M5 `if (0) {` in `voice_read` | loops end like one-shots | 24 lines, below |

M5 (verbatim):

```
FAIL port/tests/test_game.c:1625: 0 != 1
FAIL port/tests/test_game.c:972: 1 != 2
FAIL port/tests/test_game.c:980: 2 != 3
FAIL port/tests/test_game.c:982: 0 != 2
FAIL port/tests/test_game.c:988: 58963700 != 0
FAIL port/tests/test_game.c:990: 0 != 1
FAIL port/tests/test_audio.c:357: no sample wraps past the s16 limits
FAIL port/tests/test_audio.c:402: a stop for an inactive owner leaves the live voice alone
FAIL port/tests/test_audio.c:1471: 2 != 4
FAIL port/tests/test_audio.c:1474: stop_sample stops one handle's voice, not every sample voice
FAIL port/tests/test_audio.c:1512: 0 != 18432
FAIL port/tests/test_audio.c:1513: 2 != 4
FAIL port/tests/test_audio.c:1681: 2 != 4
FAIL port/tests/test_audio.c:1682: 0 != 1
FAIL port/tests/test_audio.c:1720: the one-shot took a free slot
FAIL port/tests/test_audio.c:1720: the one-shot took a free slot
FAIL port/tests/test_audio.c:1720: the one-shot took a free slot
FAIL port/tests/test_audio.c:1720: the one-shot took a free slot
FAIL port/tests/test_audio.c:1720: the one-shot took a free slot
FAIL port/tests/test_audio.c:1727: 58988198 != 58963700
FAIL port/tests/test_audio.c:1728: 0 != 58946624
FAIL port/tests/test_audio.c:1729: 2 != 4
FAIL port/tests/test_audio.c:1730: 2 != 4
FAIL port/tests/test_audio.c:1750: 2 != 4
```

The plan named V2, V6's loop block, V7's last check and `test_flow`'s 0x40
loop check (`test_game.c:1625`); all fail. The other lines are pre-existing
loop assertions that the mutation also breaks: `check_sample_slots`
(`test_game.c:972-990`, the loop-byte-1 voice count), `test_mixer`
(`:357`, `:402`) and `test_ail` (`:1471-1513`). At n = 1..5 V6's one-shot
lands in a loop's freed slot (0x42 at 42 ticks, 0x40 at 134), so the "free
slot" check fails five times.

### Not tested

- The device path (`host_audio_rate() != 0`): SDL audio cannot open on this
  host (`-66681`). Its only change is that `s_last_isr_tick` moves.
- The 30-tick clamp on `isr_elapsed`: a res.c stall of more than 30 ticks
  happens only on a large first read. The clamp is `HOST_TICK_MAX_CATCHUP`,
  shared with the host clock, with no separate test here.
- `s_last_isr_tick`'s init in `game_audio_init`: `game_init` runs once per
  process. V3's rebase covers the equivalent path.
- `sound_sfx_volume`, `snd_sample_stop` and `snd_samples_stop_all` on a
  naturally ended slot. Their status reads are covered by `test_game.c` on
  forced statuses.

## §D.3 Gate and closure

On Task 2's commit `017a37d`, after `make clean` (the two oracle fixtures
backed up and restored byte-identical) and `make build` (no warning or error
lines):

- `make verify` with the `pr_d3_*` overrides: **`EXIT=0`**
  (`$D/verify-t3.txt`). A first run was killed by an external SIGTERM
  (`make[1]: *** [frontend-oracle] Terminated: 15`, `$D/verify-t3-terminated.txt`,
  not an oracle failure: every oracle line it had printed matched); the re-run
  on the same tree completed.
- Oracle lines equal `$K/oracle-lines-base.txt`: **ORACLES-EQUAL**.
- Frame dumps (`dumps.sh d-after` + `dumpsha.sh d-after`) equal
  `$K/base.sha256`: **DUMPS-IDENTICAL**. The `--check 8000` inside the dump
  exited 0 (no `--check exited non-zero`; `$D/check-after-log.txt` holds only
  the banner line, no assertion line).
- `make audio-render` WAV (`$D/after.wav`) equals `$K/before-t2.wav`:
  **WAV-IDENTICAL**.
- `python3 tools/port_progress.py` equals `$D/progress-base.txt`:
  **PROGRESS-UNCHANGED** (`767 1203 64`; no function added).

This is the proof of Finding F11: game code reads the status (§D.0), and the
gate, not the argument, shows the frames do not move.

### Occupancy in `--check 8000` (`$D/occ.sh`)

| Run | max | full_frames | mean_x100 | loop_slot_moves | CHECK |
|---|---|---|---|---|---|
| before (`$D/occ-before.txt`, `af135ec`) | 4 | 5430 | 331 | 6 | 0 |
| after (`$D/occ-after.txt`) | 4 | 506 | 147 | 6 | 0 |
| after2 (`$D/occ-after2.txt`) | 4 | 506 | 147 | 6 | 0 |

`diff occ-after.txt occ-after2.txt` is empty: **DETERMINISTIC**. `main.c` was
restored byte-for-byte after each probe (`MAIN-RESTORED` three times;
`git diff --stat -- port/src/main.c` empty). The persistent saturation is
gone: the frames with all four slots busy fall from 5430 to 506 and the mean
occupancy from 3.31 to 1.47. `max=4` remains because four concurrent
one-shots can legitimately fill the slots for a few frames.
`loop_slot_moves` is unchanged (6): the title's legitimate stops of 0x40/0x42.

### Closure

G6 closed. With no device the mixer renders on the ISR tick's virtual clock
(`game_audio_service`), so a one-shot ends at its length (V1: 61 ticks for
0x2B11 bytes; BD/BE/BF at 42/52/60) and its slot frees; the attract loops
keep playing (V2, V6, V7, the `--check` probe).

Raw wins: spec §4.D's "no game code reads sample status" is false. `0x1CE70`,
`0x1CE04`, `0x1CD9C`, `0x1CED4` and `0x1CC28` call `0x5DD03`, and `0x2C3FC`
branches on `0x1CE70` (§D.0). The exposure is real but, measured by the gate
above, moves no frame, oracle line or WAV byte.

Ledger §H.3 (`2026-09-29-all-gaps-ledger.md`): row 6's Evidence cell gains
the **Closed** text. The count sentence before:

> **Ten named gaps** are open: the six carried by K7+K12 and K11 (#1..#6),
> `fight_health_sync`'s case 18 (#7), the two new `not modelled` deviations of
> §H.1a (#8, #9) and `movie.c`'s decode-failure exit (#10).

after:

> **Nine named gaps** are open: the six carried by K7+K12 and K11 (#1..#6),
> `fight_health_sync`'s case 18 (#7), the two new `not modelled` deviations of
> §H.1a (#8, #9) and `movie.c`'s decode-failure exit (#10) (#6 closed by
> named-gaps D).

§H.5's "Ten named gaps remain" is the Task 7 final gate's snapshot and is
left as written.
