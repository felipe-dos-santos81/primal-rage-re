# Design: Primal Rage (DOS) → SDL3 port — sub-project 2b: Smacker video + streamed-audio machinery

Sub-project 2b of the five-part decomposition (see
`2026-09-16-engine-core-port-design.md`, "Sub-project split"). This spec covers
**sub-project 2 only**. Sub-projects 1 (engine core) and 2a (audio/AIL) are
complete and merged on `main` (`aeda053`); 4 (menus/EEPROM) and 5 (fight engine)
remain.

## 1. Context and the correction that shaped this scope

The engine-core design listed sub-project 2 as "Smacker video: `twi5.smk`,
`twg.smk` decoder + present", and 2a left four `0x5dd*` AIL stubs whose comment
said they were the movie's audio streaming path. Exploration during
brainstorming **corrected that premise**, and the correction is load-bearing:

* `data/game/C/TWI5.SMK` (1,208,576 B) and `TWG.SMK` (31,048 B) are `SMK2`,
  320×200, 121 and 41 frames.
* Both have `flags = 0` (every audio track disabled) and `audio_size[0..6] = 0`.
  **The movies are silent.**
* Both are byte-identical to the CD copies (`RAGE.S16/TWI5.SMK`,
  `RAGE.S16/TWG.SMK`; `md5 c1ae2173…`, `e6c6962f…`). The CD copy has no audio
  either.
* Therefore the four AIL streaming stubs are **not movie audio**. They are the
  RAD Smacker SDK's sound adapter (`FUN_00010024`/`FUN_00010034`/
  `FUN_0001013c`/`FUN_000102b8` at `0x10000`), decoding blocks through
  `FUN_00060898` and feeding AIL. Its call sites are per-frame streamed audio in
  game states that are not ported; the shipped movies never exercise it.

Decisions taken during brainstorming, with this correction in hand:

| # | Decision |
|---|---|
| 1 | **DoD = full movie path (video) + the general streamed-audio machinery.** Video plays; the streamed-audio path is built even though no shipped, ported caller drives it. |
| 2 | **Fidelity = pixel-exact against frames captured from the original** in DOSBox-X, the same oracle shape 2a used for the OPL register stream. |
| 3 | **Decoder = in-repo, clean-room, fixed-profile.** No permissive Smacker decoder exists upstream (libsmacker LGPL-2.1; ffmpeg `smacker.c` LGPL; ScummVM GPL), and this repo has no licence of its own and vendored only MIT/public-domain code in 2a (opal). So the decoder is written here, accepts only what the two shipped files use, and rejects everything else loudly. |
| 4 | **Streamed audio = machinery proven at unit level, caller declared missing.** No shipped asset contains a Smacker-audio stream to check against; this is stated plainly, not hidden. |

## 2. Scope

### In scope

* Decode both SMK2 movies (container, Huffman trees, MMAP/MCLR/FULL blocks, RLE,
  per-frame palette) into the port's 320×200 index buffer.
* Port the movie player `FUN_0001C740` (`0x1C740`) and the intro/attract state
  entry `FUN_00010C30` (`0x10C30`) / `FUN_00011000` (`0x11000`, case 0 plays the
  two logos via two `0x1C740` calls), wired ahead of menus in the boot order.
* Open the movies by name from `--game-dir` (they are **not** in `INDEX`; see
  §4).
* Make the four AIL streaming stubs real: two buffer halves, consumed counters,
  callback registry, driver-start semantics, plus a fixed-profile Smacker-audio
  block decode (`FUN_00060898`) behind the adapter.
* Pixel-exact frame oracle + unit tests + negative controls, integrated into
  `make verify`.

### Non-goals

* **In-game streamed audio call sites.** Sub-project 2b lands the machinery, but
  the states that call it (the per-frame service `FUN_000102b8` has 11 callers)
  are sub-projects 4/5. Until one is ported, nothing shipped drives the stream;
  this is a declared gap, not a hidden one.
* Menus / character select / localisation / EEPROM (4).
* Fight engine, in-game animation, stages, demo AI (5).
* Any new third-party dependency, and anything copyleft.
* Switching the present path. `gfx_present`, `gfx_dac`, `gfx_flush_palette`,
  `swap_buffers`, `run_process_table` and `res_*` keep their current contracts.

## 3. Success criteria (DoD)

1. Boot plays both logos, right frame count and pacing, then reaches the
   existing menus/title path with no regression.
2. Each decoded frame and its palette is **pixel-exact** against the frame
   captured from the original for the same frame index.
3. The four AIL streaming stubs are non-inert and unit-proven at the
   buffer/format/rate/callback level over a synthetic stream. The test record
   states plainly: no shipped, ported caller; audibility unverified.
4. `make verify` green, including the new frame-oracle stage and the existing
   byte-exact gates; negative controls show the fixed-profile rejection and the
   bounds checks are falsifiable.

## 4. Evidence gathered (addresses and facts the plan builds on)

**Movie chain.** `0x11000`/`0x11D04` (state machine; `FUN_00011000` case 0 calls
`FUN_0001C740` twice) → `0x1C740` (`FUN_0001C740`, 322 B, VBlank-gated player,
blits `DAT_000E87A4` through `FUN_00065340`) → SDK `0x6345C` (`FUN_0006345C`,
1538 B, open/decode) and `0x63180` (`FUN_00063180`, 673 B, stream setup) →
AIL adapter `0x10000` → AIL `0x5dxxx`. The SDK region runs roughly
`0x60898`–`0x63e88`.

**Streamed-audio chain.** `FUN_0001013C` (`0x1013C`) allocates a buffer via
`FUN_0001C884` and a sample handle via `FUN_0005DBCB`, then registers it with
`FUN_0005DDAD`; `FUN_000102B8` (`0x102B8`) is the per-frame feeder
(`0x5DD5D` index → `FUN_00060898` decode → `0x5DD86` feed). `FUN_00010610`
(`0x10610`) is the separate 250 Hz streaming timer (2a Task 3 carry-forward;
the 60 Hz game tick is separate). The four stubs to replace are
`0x5DD2C`, `0x5DD5D`, `0x5DD86`, `0x5DDAD` (`ail.c`).

**Assets.** `data/game/C/TWI5.SMK` 1,208,576 B, 121 frames;
`data/game/C/TWG.SMK` 31,048 B, 41 frames. `INDEX` has 69 entries and lists
**no** `.smk`, so `res_load_index` does not load them; the game opens them by
name. Filenames `twi5.smk`/`twg.smk` exist as strings at data `0x80038`/`0x80044`
but Ghidra reports no trustworthy code xref; `\RAGE.S16` (data `0x8002C`) is
referenced near `0x10C30`. Header fields observed: `trees_size` `0x34`,
then `0x38`, `0x3C`, `0x40`, `0x44` (TWI5 `40763, 31472, 2760, 109136, 1984`;
TWG `5835, 1088, 1824, 17328, 1488`). The header field order is to be confirmed
during Task 1, not assumed.

**Existing port seams to reuse.** `gfx_dac[256][3]` (`gfx.h`); `gfx_present`;
`gfx_flush_palette` (ports `0x1C470`); double buffer `DS_000E87A4`/`DS_000E87A0`
with `swap_buffers` (`0x50188`); `game_loop`'s present/swap tail; `res.c`'s
case-insensitive directory scan (private `res_open`); `mem[]` (`0x1C740` and
`DAT_000E87A4` live there).

## 5. Architecture

Three new modules, each independently testable, mirroring the way 2a split
`opl`/`sequencer`/`mixer`/`samples`. No module takes SDL; SDL stays in
`host.c`/`main.c`. The decoder never writes `mem[0xA0000]` (the aperture rule).

### `port/src/platform/smacker.{c,h}` — container and frame decode

```c
/* Caller-owned and allocation-free: the struct carries a fixed-capacity working
 * area for the parsed trees and frame state, sized for the shipped files plus
 * headroom. smk_open rejects a movie whose tables exceed it rather than
 * allocating (fixed-profile). Only one movie decodes at a time. */
typedef struct SmkMovie SmkMovie;

int  smk_open(const u8 *data, u32 len, SmkMovie *out);   /* 0 = reject, reason logged */
u32  smk_width(const SmkMovie *m);
u32  smk_height(const SmkMovie *m);
u32  smk_frames(const SmkMovie *m);
u32  smk_frame_delay_us(const SmkMovie *m);
/* Decodes the next frame into `frame` (w*h bytes). The previous frame must
 * already be in `frame` (Smacker deltas reference it), so callers pass the same
 * buffer each time. Returns 1, or 0 on a malformed/unsupported block. */
int  smk_decode_frame(SmkMovie *m, u8 *frame);
void smk_palette_to(const SmkMovie *m, u8 dac[256][3]);   /* -> gfx_dac */
```

Fixed-profile: `smk_open` accepts only the features the two shipped files use
(§8). Everything else is rejected with a named reason; there are no silent
fallbacks and no guessing. No `malloc`, no `mem[]`, no SDL.

### `port/src/platform/audio/stream.{c,h}` — streaming backing store

```c
void stream_reset(void);
s32  stream_buffer_size(u32 format, u32 rate, u32 len);       /* 0x5DD2C */
s32  stream_index(StreamHandle h);                            /* 0x5DD5D: 0/1/-1 */
void stream_feed(StreamHandle h, int half, const u8 *buf, u32 len); /* 0x5DD86 */
void stream_set_callback(StreamHandle h, void (*cb)(void *), void *ctx); /* 0x5DDAD */
```

`StreamHandle` is the AIL sample handle `ail.c` already owns; `stream.c` treats
it as an opaque key and never dereferences its fields.

`ail.c`'s four stubs become thin delegates; no other signature changes. The
Smacker-audio block decode behind the adapter is fixed-profile for the same
reason as video, and shares `smacker.{c,h}`'s bounds discipline.

### `port/src/game/movie.{c,h}` — player glue

Opens the movie by name, drives the frame loop with the port's tick clock,
passes `DS_000E87A4` as the decode target and `gfx_dac` as the palette target,
and returns to the boot state machine at end. Wired into the `0x11000` /
`0x1C740` sites engine core left stubbed.

### `port/src/platform/res.{c,h}` — extend, do not duplicate

`res.c` already owns game-dir file access and the case-insensitive scan. Add a
name-based loader
(`int res_load_file(const char *game_dir, const char *name, u32 *out_off, u32 *out_size)`,
returning 1/0 and writing the `mem[]` offset and byte count) that reuses the
existing scan, rather than letting `movie.c` open files itself. This is the only
change to a shared module.

## 6. Data flow

**Boot (video).** `game_init` → boot state machine `0x11000` case 0 → open
`twi5.smk` then `twg.smk` by name (`res_load_file`; whole file into `mem[]`) →
`smk_open` (header, trees) → per frame: `smk_decode_frame(m, mem + DS_000E87A4)`
then `smk_palette_to(m, gfx_dac)` → VBlank-gated → `gfx_present`/`swap_buffers`
(the existing `game_loop` tail) → next frame → on end, continue boot to menus.

Pacing is derived from the original's player (`0x1C740`/`0x10C30`) rather than
guessed from the header (open item 1). `ESC` leaves a logo the way the original
does.

**Streamed audio.** `ail.c` stubs → `stream.c` halves/callbacks; the ported SDK
adapter (`FUN_0001013C` setup, `FUN_000102B8` feed, `FUN_00060898` decode) drives
`stream_feed`. With no ported caller, the path is exercised only by
`test_stream.c`.

## 7. Error handling and edge cases

* **Missing / unreadable movie file** → clear stderr message, movie skipped,
  boot continues. No crash, no hang.
* **Corrupt or truncated SMK** → `smk_open` validates that every declared chunk
  size fits the buffer before use (the `seq_bank_size` lesson from 2a: never
  trust a declared size); `smk_decode_frame` bounds-checks each block and
  returns 0 on violation. The player aborts the movie and continues boot — it
  never presents a partially decoded frame as if correct and never over-reads.
* **Unsupported feature** → loud rejection naming the feature and frame index,
  never a silent approximation (same honesty as `samples_load` rejecting a
  non-8-bit shape). What "supported" means is enumerated in §8.
* **End of movie / ESC skip** → clean return to the boot order.
* **Stream args out of range** (rate 0, bad half, null buffer) → clamp or reject
  per the 2a stub conventions (return 0 / -1), no partial writes.

## 8. Fixed profile (the decoder's supported set) and open RE items

Task 1 of the plan produces the **inventory**: parse both files and enumerate the
SMK2 features actually present (block types, which Huffman trees, RLE variants,
delta vs full frames, palette changes). The decoder supports exactly that set and
rejects the rest. Until Task 1 completes, the set is unknown and the decoder must
not claim more than it proves.

Open items carried into the plan (to be resolved by RE, not guessed):

1. **Frame pacing.** Header rate fields read `0xFFFFE444` (TWI5) and
   `0xFFFFC888` (TWG) at `0x10`, which are implausible as microseconds. Determine
   the original's actual pacing from `0x1C740`/`0x10C30` (VBlank gating). Decide
   whether to use the header value or the original's rule.
2. **Fixed-profile inventory** (§8, above).
3. **Open-by-name call site.** Locate the real open and whether the `\RAGE.S16`
   string is used as a prefix or directory. The strings have no trustworthy
   Ghidra xref.
4. **Palette semantics.** Per-frame palette vs palette deltas; confirm the
   decoder's output against the captured frames.
5. **`0x1C740` is shared** with in-game animation and the general
   `DAT_000E87A4` render path. The port must not break the existing present path
   while wiring the movie player into it.

## 9. Risks

* **Top risk — the streamed-audio decoder has no real input.** No shipped asset
  contains a Smacker-audio stream (the movies are silent; `s16snd2.gra` is
  unexplored). The machinery and its fixed-profile decode would be validated only
  against synthetic blocks derived from the public format. This is speculative
  code by construction. Decision 1/4 accepts it knowingly; if this is
  unacceptable, the alternative is to defer the audio half until a calling state
  is ported. Flagged here so the spec review can reverse it cheaply.
* **Oracle cost.** Capturing original frames requires a DOSBox-X frame-capture
  harness and a way to identify frame indices reliably. The mechanism is chosen
  in Task 0 of the plan; the capture artefacts stay git-ignored like
  `data/audio-captures/*.dro`.
* **Shared-player regression.** `0x1C740` also serves in-game animation;
  touching it risks the engine-core path. Mitigation: keep the movie player's
  entry narrow and keep the engine-core headless-frame gate green.
* **Copyright.** Fixed-profile reimplementation from the public Smacker format;
  no SDK code or data is copied.

## 10. Testing and verification

* **Frame oracle (governing).** `tools/smk_capture.py` plus a DOSBox-X harness
  capture the original's logo frames; the port's frames must match pixel-for-pixel
  (palette included). Wired into `make verify` as an oracle gate with the 2a
  semantics: absent capture ⇒ skip, `PR_ORACLE_REQUIRED=1` ⇒ fail. Captures are
  git-ignored and never committed.
* **Unit tests.** `test_smacker.c` (header parse, fixed-profile rejection, at
  least one known-good frame and its palette); `test_stream.c` (both halves,
  index, feed, consumed counters, callback registry, buffer-size helper).
* **Negative controls.** Strip a bounds check → the test must fail; feed a
  corrupt/oversized input → the decoder must reject. This proves the new checks
  are falsifiable (2a used the same technique).
* **Regression.** Existing gates stay byte-identical: engine-core headless
  frames + `symbols.h`, and the 2a audio oracle (9340 writes byte-exact).
* **Honesty records.** The sub-project report states plainly what is proven
  (video frames pixel-exact) and what is not (streamed-audio audibility and any
  in-game caller), in the same shape as the 2a report.

## 11. Stubs consumed

This sub-project removes the engine-core "sub-project 2 Smacker" stub
(`twi5.smk`/`twg.smk` logos) and the four `0x5dd*` AIL streaming stubs that 2a
assigned to 2b. It introduces no new stubs; anything deferred inside it is named
in its report with an owner.
