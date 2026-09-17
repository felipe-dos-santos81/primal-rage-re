# Sub-project report — Primal Rage (DOS) → SDL3, sub-project 2b-i: Smacker video

Date: 2026-09-17
Branch: `smacker-video`
Base commit: `4bfe170` ("docs: sub-project 2b-i (Smacker video) implementation
plan")
Cycle commits: 19, `4bfe170..f8b5e6d`, plus the final documentation commit that
adds this report.
Spec: `docs/superpowers/specs/2026-09-17-smacker-video-design.md`
Plan: `docs/superpowers/plans/2026-09-17-smacker-video.md`
Ledger: `.superpowers/sdd/2026-09-17-smacker-video/progress.md`
Supersedes: sub-project 2 (Smacker) in
`docs/superpowers/plans/2026-09-16-engine-core-port-report.md` §4.

Scope note: this report covers **sub-project 2b-i (video)** only. The spec's
general **streamed-audio machinery** (the four `0x5dd*` AIL stubs and a
Smacker-audio block decoder) is plan **2b-ii** and was deliberately **not
implemented** in this cycle (§3 DoD 3, §8).

## 1. Summary

The two boot logos now decode with an in-repo, clean-room Smacker decoder and
play on the port's own boot path, pixel-exact against frames captured from the
original. The player is wired into the `0x11000` case-0 attract entry the
engine core left stubbed; after the logos it enters title state 1 exactly as
before. The decoder never allocates, never writes `mem[0xA0000]`, and rejects
anything outside the shipped fixed profile with a named reason.

What runs:

```
game_init (0x1BEC4)
  -> game_state_init (0x10E80), at the FUN_00011000 case-0 attract entry:
       movie_play(game_dir, "twi5.smk")
       movie_play(game_dir, "twg.smk")
     -> res_load_file (name-based; the movies are NOT in INDEX)
     -> smk_open (SMK2 container + the four Huffman trees)
     -> per payload frame:
          smk_decode_frame(m, mem + DSD(DS_000E87A4))   (in place, deltas)
          smk_palette_to(m, gfx_dac)
          gfx_present(mem + DSD(DS_000E87A4), 320, 200)
          pace one frame per smk_frame_delay_us (no VBlank wait headless)
  -> then enters title state 1, unchanged
```

The player owns the presentation rule: the decoder decodes all frames; the
final payload frame is presented only when it is a hold (byte-identical to the
frame already on screen). This reproduces the settled original counts — TWI5
120 of 121, TWG 41 of 41 (§6). `--check N` is otherwise unchanged: it runs the
same boot path headless (no window, no audio device) and writes its
`frame_NNNN.ppm/.pal/.idx` as before.

## 2. Verification ladder (run 2026-09-17, `smacker-video` @ `f8b5e6d`)

```bash
rm -rf /tmp/pr_smk_build && cmake -S port -B /tmp/pr_smk_build && cmake --build /tmp/pr_smk_build
make verify
python3 tools/gen_symbols.py port/decomp port/src/symbols.h   # must stay byte-identical
```

Observed:

```
$ rm -rf /tmp/pr_smk_build && cmake -S port -B /tmp/pr_smk_build && cmake --build /tmp/pr_smk_build
-- Configuring done (4.4s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/pr_smk_build
[100%] Built target run_tests
$ grep -ciE 'warning|error' /tmp/pr_smk_build.log
0

$ make verify
== headless frames (must precede the tests that read frame_*.idx) ==
./build/prageport --game-dir data/game/C --check 60
prageport 0.0.1 game-dir=data/game/C --check 60 (headless)
== tests (oracles required; consume the captured frames) ==
PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests
smk_open: truncated header
smk_open: bad magic (want SMK2)
smk_open: frame payload runs past the buffer
smk_open: tree value count exceeds the arena
movie: no-such-movie.smk: not found, skipping
oracle C-vs-Python: 9340 writes byte-exact
capture oracle first difference at C write 2: C tick=60 reg=0x20 val=0000 vs capture tick=60 reg=0xb0 val=0x2b (C 9340 writes, capture 6380 normalised)
all checks passed
== smacker frame oracle (pixel-exact) ==
smk_open: truncated header
smk_open: bad magic (want SMK2)
smk_open: frame payload runs past the buffer
smk_open: tree value count exceeds the arena
movie: no-such-movie.smk: not found, skipping
oracle C-vs-Python: 9340 writes byte-exact
capture oracle first difference at C write 2: C tick=60 reg=0x20 val=0000 vs capture tick=60 reg=0xb0 val=0x2b (C 9340 writes, capture 6380 normalised)
all checks passed
smk_compare: 120/120 frames match
smk_compare: 41/41 frames match
== symbols.h must regenerate byte-identically ==
python3 tools/gen_symbols.py port/decomp port/src/symbols.h
1304 globals, 1206 functions -> port/src/symbols.h
  dropped 11 globals, 1 functions outside the LE objects
all checks passed
make verify  3.33s user 0.83s system 53% cpu 7.719 total
exit 0
```

| Step | Result |
|---|---|
| clean out-of-tree build (`/tmp/pr_smk_build`) | exit 0, `grep -ciE 'warning\|error'` = **0** |
| `make verify` (warm) | exit 0, ~7.7 s wall; `all checks passed` |
| frame oracle (`make verify`) | `smk_compare: 120/120 frames match`, `41/41 frames match` |
| `PR_ORACLE_REQUIRED=1 ./build/run_tests` | `all checks passed`, exit 0 |
| `./build/prageport --game-dir data/game/C --check 120` | header line, exit 0 |
| `python3 tools/gen_symbols.py port/decomp port/src/symbols.h` | `1304 globals, 1206 functions`, `dropped 11 globals, 1 functions`, exit 0 |
| `git diff --exit-code -- port/src/symbols.h` | no diff (header byte-identical) |

Headless timings around the Task-7 pacing fix, as recorded in the task-7
report and the ledger (warm build, before the Task-8 oracle stage existed):

```
run_tests (PR_GAME_DIR set):   16.4 s  ->  2.2 s
--check 120:                   16.5 s  ->  2.2 s   exit 0
make verify:                   32.5 s  ->  3.9 s   exit 0
```

Current reproduction (warm, Task-8 oracle stage included): `run_tests`
(PR_ORACLE_REQUIRED=1) 4.6 s, `--check 120` 2.6 s, `make verify` 7.7 s. The
increase over the 2.2/3.9 s figures is the added second `run_tests` (the
`smk-oracle` dump) plus the two `smk_compare.py` passes and normal machine
variance — not a pacing regression. Headless playback no longer spends the
movies' real-time budget (`test_movie.c` asserts `< 100` host ticks where real
pacing would spend ~515 for TWI5).

`PR_ORACLE_REQUIRED=1` is mandatory for a real verification run. The governing
frame oracle reads the untracked `data/smk-captures/` (git-ignored) and the
same flag also gates the pre-existing audio/Ghidra oracles; without it the
comparisons skip and a green run does **not** exercise them.

## 3. Definition of done — status, stated to scope

The spec's DoD (`2026-09-17-smacker-video-design.md` §3):

### DoD 1 — boot plays both logos, right frame count and pacing, then the title path with no regression: **met for video.**

`game_state_init` calls `movie_play` for `twi5.smk` then `twg.smk` at the
`0x11000` case-0 site (`flow.c:277-279`), then assigns title state 1 as before.
The player presents the settled counts (TWI5 120, TWG 41; `test_movie.c`
asserts them on real playback). Pacing is derived, not guessed:
`smk_frame_delay_us = |pts_inc| * 10` (`smacker.c:292-294`) gives 71,000 us
(TWI5) and 142,000 us (TWG); the DOSBox-X capture measured 71,222 and 140,593
us per frame (`data/smk-captures/pacing.txt`), confirming the header rule. The
existing engine-core/audio gates stay green.

### DoD 2 — each decoded frame and its palette is pixel-exact against the original: **met for the presented prefix.**

Both movies' presented frames are byte-identical to the capture as 320×200
**RGB24** (192,000 bytes, palette included): TWI5 **120/120**, TWG **41/41**
(`smk_compare.py` exit 0, run twice per `make verify`). TWI5 decodes 121
payload frames; its 121st is real, distinct content the original never presents
(§6).

### DoD 3 — the four AIL streaming stubs are real: **NOT done — separate plan 2b-ii, by design.**

This cycle did not touch `ail.c`'s `0x5dd*` stubs, `stream.c`, or any
Smacker-audio block decode. The design's Decision 1 bundled video and streamed
audio into one sub-project; the planning split them, and this plan's brief
states streamed audio is explicitly out of scope (2b-ii). No part of this
report claims the streaming path exists.

### DoD 4 — `make verify` green including the frame oracle, with falsifiable negative controls: **met.**

`make verify` is exit 0 (§2). Negative controls were run and reverted in-cycle:
the decoder's run bound (stripped → 1,844 out-of-block byte writes → test
failed), the container/tree bounds (four named rejects fire in `run_tests`),
and the frame oracle (one byte flipped in `data/smk-captures/twg/frame_0020.raw`
→ `frame 20: MISMATCH ... first diff at 100`, `40/41 frames match`, verify exit
2; restored md5-verified). Task 4's arena-exceed control and Task 5's run-bound
control are recorded in the ledger.

## 4. Modules

`/` = new this cycle; `(reused)` = existing seam.

### `platform/smacker.h` / `smacker.c` — clean-room SMK2 decoder

`smk_open` / `smk_width` / `smk_height` / `smk_frames` / `smk_frame_delay_us` /
`smk_decode_frame` / `smk_palette_to`. `SmkMovie` is caller-owned and exposed
(like `SampleVoice`): it borrows `data`, carries a fixed `s32 words[65536]`
tree arena, `tree[4]`, `last[4][3]`, the live `pal[768]`, and a frame cursor.
No `malloc`, no `mem[]`, no SDL, no aperture write.

Stages, each bounded before use (the `seq_bank_size` lesson):

* **Container** (`smk_open`): magic `SMK2`, dims ≤ 32768, frames ≤ 0xFFFFFF,
  `treesize`, and `data_off + Σ(frame_size & ~3) == len` — the whole file must
  be accounted for. `*out` is staged in a local and committed only on success
  (test pins `*out` untouched on reject).
* **Trees** (LSB-first): four trees in header order (mmap, mclr, full, type),
  present-flag / byte-trees / three 16-bit escapes, preorder node words
  `0x80000000 | left_count`. Rejects an overrun, a value count over the arena,
  or recursion past the depth caps (27 small / 500 big).
* **Palette** (`smk_palette_update`): the chunk's first byte is its length in
  4-byte units including itself; RLE skip / copy / new-entry with the 6-bit
  expansion table; bounded to the chunk and to 256 entries.
* **Blocks** (`smk_video`): `type` from the type tree, `run` from the format's
  64-entry run table, MONO / FULL / SKIP / FILL per the corrected Format
  reference; in-place so SKIP and deltas work; every block write inside
  `width*height`, every read bounded by the sticky bit-reader error.

Verified: `test_smacker.c` opens TWI5/TWG, checks dims/counts, all four trees
inside the arena and in order, rejects truncated/bad-magic/oversized-payload/
arena-exceeded inputs, decodes every frame of both movies (121 / 41), rejects a
decode past the last frame, ends with a populated palette, and pins the 4×4
block-write bound (a synthetic 4×4 movie must touch no byte past the first 16).

### `game/movie.h` / `movie.c` — the player (`0x1C740` boot-logo path)

`movie_play(game_dir, name)` validates args, loads by name through
`res_load_file`, opens with `smk_open`, rejects a dimension above 320×200, then
loops all frames decoding into `mem + DSD(DS_000E87A4)`, writes `gfx_dac`,
presents through `gfx_present`, paces on the host tick clock, and returns on
end or ESC. Missing/unreadable/unsupported files and a rejected `smk_open` are
clean skips (return 1, one stderr line); invalid args and a failed frame decode
return 0. `movie_frames_presented()` exposes the count the player actually
presented (a check-only observable, like `game_audio_ticks()`).

Verified: `test_movie.c` asserts `movie_play` returns 1 and presents 120
(`twi5.smk`) then 41 (`twg.smk`), that headless playback does not spend the
VBlank clock (< 100 ticks), that a missing name is a clean skip returning 1 and
presenting nothing, and that NULL args return 0 without crashing.

### `platform/res.c` / `res.h` — name-based loader

`res_open_name` is factored out of the INDEX open path (same
case-insensitive directory scan) and `res_load_file` loads an arbitrary file by
name through the existing bump allocator, rejecting a size that is not a
positive u32 before allocating. Reused, not duplicated: no new directory logic.

Verified: loading `twi5.smk` (lowercase name, uppercase file) yields the
expected size; a missing name returns 0 with outputs untouched.

### `game/flow.c` — boot wiring

`game_state_init` plays the two logos at the `0x11000` case-0 entry, reports a
hard playback failure per movie to stderr and keeps booting, then enters title
state 1. The `0x11000` attract sub-machine's state-0/≥10 remainder stays
deferred.

### `tools/smk_info.py`, `tools/smk_capture.py`, `tools/smk_compare.py` — RE tooling

See §7 for the capture mechanism and §4.1 for `smk_info`'s fixed-profile
inventory.

## 5. `/* PORT: */` deviations and `/* TODO(verify): */` in the new code

The cycle's own markers, by file and line:

**`smacker.h`**
* `:11` — the original played these through its licensed Smacker library
  (`0x1C740`/`0x10C30`/`0x11000` case 0); the port reimplements the decoder
  rather than vendoring the LGPL/GPL alternatives.

**`smacker.c`**
* `:364` — the plan's original Format reference omitted the palette chunk's
  leading length byte; the code follows the corrected reference (`b2e20f4`):
  the first byte is the chunk length in 4-byte units, the byte included.
* `:456` — the pixel order follows the corrected Format reference (`b2e20f4`),
  not the plan's old wording: MONO uses all 16 bits of `map` (no 8-bit
  truncation) and FULL's first code paints columns 2-3, the second columns 0-1.

**`movie.h`**
* `:18` — presented-frame count of the last `movie_play()` (the trailing
  ring/hold rule is the player's, so a test asserts the real playback count).

**`movie.c`**
* `:14` — the shipped fixed profile: both logos are 320×200 (the decoder also
  accepts smaller synthetic grids for its own tests).
* `:20` — int 16h key packing: ESC is `0x011B`, the same code `game_loop()`
  tests.
* `:29` — `0x1C740` blits `DAT_000E87A4` to the screen aperture once per frame;
  the port's replacement is `gfx_present()` on the same buffer. The player does
  not own the double-buffer swap (`0x255CC` does): the decoder is in-place, so
  every frame decodes into the same buffer and swapping would lose that state.
* `:40` — pace one frame by `smk_frame_delay_us` converted to whole 60 Hz host
  ticks, carrying the sub-tick remainder; with no window open the real-time
  wait is skipped (the frame is still decoded and presented).
* `:92` — **`TODO(verify)`**: the original's `0x1C740` player-loop semantics are
  unproven (no working scriptable DOSBox-X debugger); the fixed-profile rule
  reproduces the settled capture counts (TWI5 120 of 121, TWG 41 of 41).

**`res.c`**
* `:15` — pre-existing, unchanged this cycle: the bump allocator replaces
  `FUN_0001C308`'s real block allocator with `free()`.

**`flow.c`** (this cycle's markers only)
* `:270` — `FUN_00011000` case 0 plays the two boot logos through two `0x1C740`
  calls; the rest of the attract sub-machine is still deferred, so the port
  plays the logos here and then enters state 1 directly.
* `:277-279` — the two `movie_play` calls report a hard failure to stderr; a
  missing or rejected movie is skipped, never fatal.
* `:620` — pre-existing deferral note (`0x11000` attract sub-machine state
  0/≥10); now partially superseded for state 0's logo playback, the remaining
  states stay deferred.

`flow.c`'s many other `/* PORT: */`/`TODO(verify)` markers belong to the
engine-core (sub-project 1) and audio (2a) cycles and are unchanged.

## 6. The settled presentation truth

The presentation rule is the player's, and it was **settled, not guessed**:

* **TWI5 presents 120 of its 121 payload frames; TWG presents 41 of 41.** The
  DOSBox-X capture contains exactly 120 TWI5 frames and 41 TWG frames
  (`data/smk-captures/`), and `pacing.txt` gives 8.618 s / 14.041 fps for TWI5 —
  121 periods of 71.2 ms with 120 images.
* **TWI5's 121st payload frame is real, distinct content the original never
  presents.** It is unique (its MD5 matches none of frames 0-119), ffmpeg
  decodes it as a real frame, and it appears in none of **three independent
  captures** taken during the human-directed re-capture (commit `199cad1`,
  which also hardened `smk_capture.py` to refuse a capture window that
  truncates the movie tail, so an early end cannot masquerade). TWG's frames
  37-40 decode to the same hold image, so the held frame *is* presented and the
  capture correctly emits 41.
* **Ownership.** The decoder never drops a frame; the oracle compares the
  captured **prefix** to the decoded sequence (TWI5 0..119, TWG 0..40). The
  player decides presentation: it decodes the final payload frame into a
  scratch buffer seeded from the drawn image (so deltas are correct) and
  presents it only if it is byte-identical to the image already on screen. The
  rule is content-based because the container carries no ring-frame marker
  (flags bit0 clear on both files).

The rule carries a `/* TODO(verify): */` (`movie.c:92`): the original
`0x1C740` loop's exact semantics remain unproven, and a third movie could need
a different trailing rule. See §9.

## 7. Fixed profile and the original-frame oracle

### 7.1 Fixed profile (inventory from `tools/smk_info.py`)

Both files are `SMK2`, 320×200, **silent**, and reconcile to the byte
(`data + Σ(frame_size & ~3) == filesize`):

| Field | TWI5.SMK | TWG.SMK |
|---|---|---|
| size | 1,208,576 B | 31,048 B |
| frames | 121 | 41 |
| `pts_inc` (signed) | −7100 | −14200 |
| header flags | 0 | 0 |
| `treesize` | 40763 | 5835 |
| tree sizes (mmap, mclr, full, type) | (31472, 2760, 109136, 1984) | (1088, 1824, 17328, 1488) |
| audio descriptors (7) | all `(0, 0)` | all `(0, 0)` |
| audio frame flags | `[]` | `[]` |
| keyframes (`frame_size & 1`) | `[]` | `[]` |
| palette-change frames | 3 (`0, 101, 102`) | 1 (`0`) |
| layout (tbl/trees/data/end) | `0x68 / 0x2c5 / 0xa200 / 0x127000` | `0x68 / 0x135 / 0x1800 / 0x7948` |

The decoder supports exactly this observed set: `SMK2`, all four trees present,
MONO/FULL/SKIP/FILL, a first-frame + delta palette, no keyframe bits, no audio.
Everything else is rejected by name.

### 7.2 Capture mechanism and its limits

`tools/smk_capture.py` captures the original in DOSBox-X with
**`DX-CAPTURE /V`**, which writes a lossless AVI+ZMBV; `ffmpeg` decodes every
`*.avi` in filename order to **320×200 RGB24**. ZMBV is lossless, so the
decoded RGB is the DAC output the original presented. Frames are aligned to
movie indices by **monotonic content match** against an ffmpeg decode of the
movie (option 3 of the plan's ladder). Output (all git-ignored):
`<out>/<movie>/frame_%04d.raw`, `palette.txt`, and `pacing.txt`.

**Limits, stated plainly:** the mechanism needs a real (windowless, scripted)
DOSBox-X run; it captures the whole boot, not just the logos; and it observes
only the DAC output (RGB), never the 8-bit index buffer. The **frame alignment
was bootstrapped on an ffmpeg reference decode**: the reference is *alignment
only* — the pixels written to the oracle are always the original's captured
pixels — and an interior reference frame that cannot be found aborts the tool
rather than emitting a misaligned oracle. The alignment therefore rests on the
reference decode's frame identity (a mismatch in the interior is fatal, a
trailing non-presented frame is dropped only after ≥1 s of post-movie capture).

RGB24 rather than indices because the shipped palettes contain duplicate
colours (TWI5 191 distinct of 256; TWG 96), so an RGB→index reduction would be
ambiguous; RGB is also stricter (it exercises the decoder's palette as well as
its indices).

### 7.3 The three tools

* **`tools/smk_info.py`** — container parser/inventory; the RE artefact that
  proves the layout and enumerates the fixed profile (read-only).
* **`tools/smk_capture.py`** — DOSBox-X `DX-CAPTURE /V` → AVI/ZMBV → ffmpeg
  RGB24, content-aligned to movie indices; a stdlib-only `--self-test` covers
  the alignment and palette logic; the truncation guard (`199cad1`) refuses a
  too-short post-movie window.
* **`tools/smk_compare.py`** — pixel-exact comparison of port frames against
  captured frames; absent capture skips (exit 0) unless `PR_ORACLE_REQUIRED=1`,
  then fails. The `smk-oracle` Makefile stage drives it with `--frames 120` /
  `--frames 41`.

## 8. What is NOT proven / accepted limits

1. **The presentation rule is content-based and unverified against the
   original's loop.** The original `0x1C740` semantics could not be read (no
   scriptable DOSBox-X debugger); the rule reproduces the settled counts for
   the two shipped movies but is not a ported algorithm. `movie.c:92` carries
   the `TODO(verify)`.
2. **The capture's frame alignment was bootstrapped on an ffmpeg reference
   decode** (§7.2). The oracle's *pixels* are the original's, but the *index
   mapping* depends on the reference frame identity (interior mismatches abort;
   trailing non-presented frames are dropped only after ≥1 s of post-movie
   capture). No interior misalignment is possible without aborting; a
   systematic reference/decode disagreement is not independently ruled out.
3. **Streamed audio (2b-ii) was not implemented.** The four `0x5dd*` AIL stubs
   and the Smacker-audio block decoder remain the declared gap; this report
   claims nothing about them.
4. **Asset-gated oracle.** A fresh checkout without `data/game/C` and the
   git-ignored `data/smk-captures/` cannot run the governing frame oracle
   (`PR_ORACLE_REQUIRED=1` then hard-fails, by 2a design).
5. **Hard-coded prefix counts.** `--frames 120`/`41` and the test constants
   mirror the settled capture; a re-capture with a different presented count
   must move both together.
6. **In-place decoder state.** A frame that fails mid-video can leave `m.pal`
   and the tree `last` slots half-mutated; no caller retries, and the shipped
   fixed profile never fails.

## 9. Unresolved risks

1. **`0x1C740` loop semantics unproven** (§8.1) — the highest-value open item;
   a third movie or a ring-frame marker would settle it.
2. **Alignment bootstrap** (§8.2).
3. **The capture window must not truncate the movie.** Mitigated by the
   `199cad1` guard (fails loudly below 1 s of post-movie capture); the guard
   itself is behavioural, not proof of the original's end rule.
4. **Streamed audio untouched** (§8.3).
5. **The windowed run is real-time by construction.** Pacing is skipped
   headless; a windowed run spends the movies' ~14.4 s. `host_has_window()`
   gates it (additive host API).

## 10. Deferred minors — disposition

Recorded in the ledger; none blocks this cycle.

* **Task 1 (4):** `smk_info.py` open without a context manager (tool-lifetime
  leak only); `SMK_PAL` dead in the tool (reserved for the decoder); the report
  asserts `make verify` green without showing output (no build touched);
  `pts_inc` printed unsigned (signed values tabulated in the report).
* **Task 2 (1):** `IMGMOUNT D` without `-ro` (ISO is read-only in practice).
* **Task 3 (4):** descriptive comments beyond the PORT/TODO rule (matches the
  `samples.h` convention); `tree_size[i]` not range-checked against `treesize`
  (deferred to Task 4); `frame_delay_us` not asserted (formula vs measured);
  `frame_delay_us` can wrap for an extreme `pts_raw` (harmless for the fixed
  profile).
* **Task 4 (3):** small-tree depth/leaf overflow mislabelled as an
  arena-exceed diagnostic; the committed test covers TWI5 only (TWG `smk_open`
  asserted via a throwaway harness); the arena limit includes the three slots
  and would benefit from a one-line comment.
* **Task 5 (4):** the decoder leaves `m->pal`/`m->last` half-mutated if the
  video stage fails after a palette update (no caller retries); `check_capture`
  skips when captures are absent unless required (Task 8 gate); explanatory
  comments exceed the PORT/TODO-only rule (matches Task 4 style); the dump
  names frames by payload index.
* **Task 6 (3):** a short `fread` leaks a bump block (the allocator has no
  `free`); the case-insensitive scan is not exercised on macOS APFS (exact
  `fopen` resolves first); the same `long`→`u32` cast pre-exists in
  `res_load_index`.
* **Task 7 (3):** comment-policy banners; `MOVIE_ESC` duplicates a `flow.c`
  literal; the dimension guard is area-only, not `w==320 && h==200`; ESC is
  checked after the pace (≤1 frame latency).
* **Task 8 (5):** the fresh-checkout clause is unsatisfiable (verify requires
  the oracles by 2a design; the clause was in the dispatch, not the plan); the
  build is phony, so `smk-oracle` re-runs configure+build (~2.2 s); `--frames`
  counts are duplicated between the `Makefile` and the test;
  `PR_ORACLE_REQUIRED` is tested non-NULL in the C test but literal `"1"` in
  `smk_compare.py`; a present-but-empty capture directory hard-fails rather than
  skipping.

**Coverage check.** Every `minor (deferred)` line in `progress.md` (Tasks
1×4, 2×1, 3×4, 4×3, 5×4, 6×3, 7×3, 8×5 = 27) is represented above.

## 11. Honesty statement

The project may claim:

* the container layout reconciles **byte-exactly** on both shipped movies
  (`data + Σ(frame_size & ~3) == filesize`), and the tree, palette and
  MONO/FULL/SKIP/FILL block decode are proven by pixel-exact agreement with the
  original;
* both movies' presented frames are **pixel-exact** against the DOSBox-X
  capture as RGB24 (palette included): TWI5 **120/120**, TWG **41/41**;
* the original presents **TWI5 120 of 121** and **TWG 41 of 41**, and TWI5's
  121st payload frame is real content it never presents (three independent
  captures, commit `199cad1`);
* pacing is derived from the header's `pts_inc` and confirmed by the measured
  capture (`|pts_inc| × 10 us`);
* the decoder is clean-room, allocation-free, fixed-profile, and rejects
  everything else by name;
* `make verify` is green including a falsifiable pixel-exact frame oracle.

The project may **not** claim:

* that the presentation rule is the original's — it is a content-based
  fixed-profile rule carrying a `TODO(verify)` on `0x1C740`'s loop;
* that the oracle's frame *alignment* is independent of the ffmpeg reference
  decode used to bootstrap it (the *pixels* are the original's);
* that streamed Smacker audio works — it was not implemented here (2b-ii);
* that the capture mechanism observes anything but the DAC output (RGB), or
  that it isolates the logos from the rest of the boot.

## 12. Final status

Sub-project 2b-i's video DoD is met as stated in §3: DoD 1 for video, DoD 2 for
the presented prefix, DoD 3 **not done and owned by 2b-ii**, DoD 4 met with
falsifiable controls. Everything on the Smacker video path is ported and
verified; the streamed-audio half of the original 2b scope is the named next
plan (2b-ii). Menus/EEPROM (4) and the fight engine (5) remain.
