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
