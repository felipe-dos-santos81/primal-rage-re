# Sub-project report — Primal Rage (DOS) → SDL3, sub-project 4a-ii: the actor system and the title

Date: 2026-09-18
Branch: `actor-system-2` (cut from `main` at `d5ef82a`)
Base: `d5ef82a` ("plan: 4a-ii plan errata …"; the point where the merged
sub-project 4a-i work ends and this cycle's branch starts)
Cycle commits: 41, `d5ef82a..c2c80f0` — 9 `actors:`, 9 `tools:`, 8 `title:`,
1 `rng:`, 14 `plan:`/`docs:` errata. The final commit adds this report and the
docs it invalidates.
Spec: `docs/superpowers/specs/2026-09-17-actor-system-design.md`
Plan: `docs/superpowers/plans/2026-09-17-actor-system.md` (revised in flight by
14 errata commits)
Ledger: `.superpowers/sdd/2026-09-17-actor-system/progress.md`
Args binding: `docs/superpowers/plans/2026-09-17-actor-system-args.md`

Scope note: this branch carries only this cycle's commits; no unrelated plan
shares it. This report is the authoritative record of the cycle; the briefs that
drove it were wrong repeatedly and this document records each reversal, not just
the landing.

## 1. Summary

The actor system is ported and the title is proven pixel-exact against the
original for its 96-frame window. Six units landed:

* `game/rng.{c,h}` — the `0x5D7DC` LCG (`state = state*0xB90D12B9 + 0x38CE051F`,
  return `((state>>16) * (range & 0xffff)) >> 16`), seeded `0xABCD` in
  `game_init` (the `0x20C62` store in `0x20C10`), stepped once per master-loop
  iteration where `0x255CC` calls it.
* `game/actors.{c,h}` — the `0x68`-byte record pool and its free/active lists
  (`0x249B0`/`0x249C0`/`0x249D0`), `actor_alloc` (`0x2AC80`), spawn `0x2AE14`,
  the state-begin reset `0x2BAF4`, the pset sync `0x2A31C` → `0x2A1FC` →
  `0x2A820`/`0x2A690`, the motion step `0x2A4FC`, `actor_set_dead` `0x2B150`,
  the animation-stream readers/writer (`0x2A408`/`0x29F34`/`0x29DB8`), the entry
  helpers (`0x2BC30`/`0x2BCF4`), the 47-opcode dispatcher `0x2B2A0`, the text
  grid (`0x2F0F0`/`0x2F198`/`0x2F280`/`0x2F4BC`) and the glyph renderer
  (`0x2F830`/`0x2F5A0`).
* `game/flow.c` — the real `0x121A0` title state, the `0x1C500` caption chain
  (with `0x474E4` decode and a direct `ENGLISH.TXT` read for `0x47370`), and the
  `PR_TITLE_DUMP` hook.
* `platform/gfx.{c,h}` — the shared `palette_record` (`0x33734`) moved out of
  `flow.c`, and the 6-bit VGA DAC truncation/expansion.
* `tools/title_pin.py` — the five-site pinned capture copy of `PRAGE.EXE`.
* `tools/title_capture.py` + `tools/title_compare.py` — the capture and the
  tear-aware, stdlib-only pixel oracle.

Files created: `port/src/game/rng.{c,h}`, `port/src/game/actors.{c,h}`,
`port/tests/test_rng.c`, `port/tests/test_actors.c`, `port/tests/test_anim.c`,
`port/tests/test_text.c`, `port/tests/test_title.c`, `tools/title_pin.py`,
`tools/title_capture.py`, `tools/title_compare.py`,
`tools/tests/test_title_pin.py`, `docs/superpowers/plans/2026-09-17-actor-system-args.md`.
Files extended: `port/src/game/flow.{c,h}`, `port/src/platform/gfx.{c,h}`,
`port/src/platform/sprite.c` (the palette-bank fix), `port/tests/test.h`,
`port/tests/run_tests.c`, `port/CMakeLists.txt`, `Makefile`, `port/tests/test_flow.c`,
`port/tests/test_gfx.c` (fake removed), `port/tests/test_sprite.c`,
`port/tests/test_render.c`.

`make verify` is green with zero compiler warnings (§11). No SDL outside
`host.c`/`main.c`, no new dependency (the oracle comparator is stdlib-only), and
`data/` untouched (the pin writes `/tmp` only).

## 2. The pin as implemented

### 2.1 Five sites, not three

The pin replaces exactly the RNG draws whose **values are consumed** by the
pinned window, in place, leaving the rest of the LCG real. `tools/title_pin.py`
applies, from `PRAGE.EXE`:

| file offset | VA | original bytes | replacement | value | purpose |
|---|---|---|---|---|---|
| `0x650E9` | `0x12295` | `e8 42 b5 04 00` | `b8 0c 00 00 00` | 12 | title draw 1 (`0x5A`, start X) |
| `0x650F5` | `0x122A1` | `e8 36 b5 04 00` | `b8 6f 00 00 00` | 111 | title draw 2 (`0x7E`, speed) |
| `0x6510B` | `0x122B7` | `e8 20 b5 04 00` | `b8 00 00 00 00` | 0 | title draw 3 (`2`, gravity sign) |
| `0x7E289` | `0x2B435` | `e8 a2 23 03 00` | `b8 00 00 00 00` | 0 | anim-stream opcode-8 draw |
| `0x7ED5C` | `0x2BF08` | `53` | `c3` | — | `ret`: inert the deferred `0x2BF08` |

The first three are the only `0x5D7DC` call sites inside `0x121A0`. The values
`12`/`111`/`0` are exactly what the port's own LCG produces from seed `0xABCD`
for ranges `0x5A`/`0x7E`/`2`, so the port reproduces them honestly by seeding
and drawing — there is **no stub and no RNG instrument in the port**. The patch
model is variable-length `(offset, original bytes, replacement)`; every site's
original bytes are verified before anything is written, and the tool refuses to
write over the source or anywhere under `data/` (casefolded and `samefile`
checked, because APFS resolves `Data/` and `data/` alike).

### 2.2 The failed first draft, and why it was wrong

The plan's first draft stubbed `0x5D7DC` to a constant. Verified by disassembly
of the shipped EXE: a constant-returning `0x5D7DC` zeroes the three draws, so
the logo's start X, speed **and** gravity all become 0 — the logo never moves
and the 96-frame window would compare a near-static image. The correct rule is
narrower: pin the consumed values as immediates and let the rest of the LCG run.
The fourth site (`0x7E289`) was added while chasing a two-run divergence
attributed to the anim stream's opcode-8 `rand()` (see §7 — that explanation was
later disproved). The fifth (the `0x2BF08` `ret`) was added while chasing a
still-wider divergence.

### 2.3 There is no title-entry re-seed — and a re-seed was deliberately removed

The brief for this cycle said the port needed "`rng_seed(0xABCD)` at title entry
plus the real draws". That is wrong. Task 9 established by disassembly that
`0x121A0` contains **no store** to `DS_000EF6D8`; the only `0xABCD` store is
`0x20C62` in `0x20C10`, which Task 3 transcribes as `game_init`'s seed. A
title-entry re-seed was implemented and then **removed** (fix round 1,
`f55e548`) because it would have forced the pinned values by construction and
hidden a real divergence. Task 9 then *measured* the honest path: at the draws
`DS_000EF6D8 == 0xABCD`, yielding `12`/`111`/`0` and
`DS_00107A50 == 0x2420`. The task-10 oracle driver, likewise, must not re-seed
(plan errata `f9c616c`); it calls `game_init()` and nothing after it.

## 3. The capture contract

Captures are the original's bytes and are git-ignored under
`data/title-captures/`. `tools/title_capture.py` reuses `smk_capture`'s
`run_dosbox`/`read_avi_frames`/`align` by import; staging is symlinks only (the
real pinned `PRAGE.EXE` from `title_pin.py` plus `CD/RAGECD.ISO`), so
`data/game/C` (44 MB) is never copied.

### 3.1 One file per distinct game frame

A capture index **is not** a game-frame index: the game's logic runs at 60 Hz
while mode 13h is sampled at 70.09 Hz, so a game frame is displayed for one or
two captured frames. The tool decodes the post-logo run (320×200 RGB24),
collapses consecutive identical captured frames into one file per distinct game
frame, and records each file's raw capture index in `window.txt`. The current
captures: capture 1 is 582 distinct frames over raw `1377..3151`
(`twi5_last=966`, `twg_last=1326`); capture 2 is 591 distinct over raw
`1366..3151` (`twi5_last=955`, `twg_last=1315`) — the boot-region jitter means
raw indices differ run to run.

### 3.2 The window is located by content, not by "after the second logo"

`FUN_00011000` — the boot sub-machine, `prage.c:775` — runs its own countdown
machine (180/64/240-frame waits) **between** the two logos and the title, so
"the first frame after the second logo" is wrong. The title window is located by
aligning the port's `frame_0000.raw` into the capture (`--port-anchor`), and the
dump driver runs as part of `make title-oracle`. The port's dump is an input to
the anchor, not a consumer of it.

### 3.3 The Task 2 gate was superseded, not met (consented DoD change)

The plan's two-run, port-free byte-identity gate was **not met** and cannot be:
the raw AVI differs from raw frame 31 onward (boot/movie sampling jitter), and a
port-free window locator is circular — its only trustworthy anchor is the port's
own frame 0. An earlier locator derived from a pinned-vs-unpinned
RNG-sensitivity diff also died once the in-window consumers were pinned. The
human approved the reorder (`428c649`): build the port side first, let the
oracle's content alignment define the window, and make the **port-vs-two-captures**
comparison the determinism proof. `--verify-reproducible` was demoted to a
diagnostic (exit 0; it reported 323/586 shared, first divergence at distinct
index 29, raw 1587 vs 1581 — the boot region). The stronger replacement is
Task 10's oracle.

## 4. The oracle

`tools/title_compare.py` (`make title-oracle`, wired into `verify`) is a
**zero-pixel-tolerance**, stdlib-only comparator. It is tear-aware because the
original updates the VGA aperture progressively at `0x255CC` with no retrace
wait while DX-CAPTURE samples at 70.09 Hz, so about one captured frame in six is
a tear.

### 4.1 The refinement chain and its causes

Three human-approved DoD #2 refinements:

1. **tear-aware splice, zero tolerance** (`80b94d3`) — a captured frame is
   explained as `port[N][0..b) ++ port[N+1][b..192000)`; a clean frame is `b=0`.
2. **the splice is a byte offset, not a row boundary** (`37b16d4`) — measured
   `row 144 = port32[:732] ++ port31[732:]`; coverage tightened to `1..94` by
   both captures with at most one unexhibited frame at each end (only `0` and
   `95`, the boot→title and title→state-2 transitions, may be lost, and only
   when their neighbour is exhibited exactly).
3. **one byte-wise transition row** (`6bee3c5`) — the original's copy writes
   dwords, so a row caught mid-write is dword-interleaved; if no single splice
   explains a frame, it is accepted only as byte-exact outside exactly **one**
   transition row, whose every byte must equal the byte at the same offset in
   one of the two adjacent port frames. No third source. If the model is still
   insufficient (two rows, or a byte from a third frame) the frame is reported
   and the oracle fails; the model is not extended.

No threshold, mask, crop, frame-skip or per-frame allowance exists.

### 4.2 The strictness rule

With `PR_ORACLE_REQUIRED=1` and fewer than two captures, the tool **exits
non-zero** and says the determinism proof is incomplete (`5fb107b`/`c2c80f0`).
A green `make verify` therefore means the proof was completed; it is not enough
to print INCOMPLETE and exit 0.

### 4.3 Final measured result

Run on this cycle (`make verify`, exit 0):

```
title_compare: capture 1: window distinct [214..323] (raw 2201..2311)
title_compare: capture 1: 110 frames in window: 53 clean, 56 splice, 1 transition, 0 unexplained
title_compare: capture 1: transition rows (N, row, from_N, from_N+1) [1 frames]: ['port17@row54(28/22)']
title_compare: capture 1: port frames exhibited 95/96; missing [95]; endpoints OK
title_compare: capture 2: window distinct [215..324] (raw 2190..2299)
title_compare: capture 2: 110 frames in window: 53 clean, 57 splice, 0 transition, 0 unexplained
title_compare: capture 2: port frames exhibited 94/96; missing [0, 95]; endpoints OK
title_compare: determinism: clean samples of 36 port frame(s) agree, 0 disagree
```

Both captures satisfy clauses A (splice) and B (transition) with **zero
unexplained frames**; every frame `1..94` is exhibited by both; capture 1's one
transition row is `port17@row54`, 28 bytes from port frame 17 and 22 from 18.
The exclusivity of the transition row to capture 1, and the loss of a different
endpoint per capture, are exactly the run-to-run sampler behaviour the model
predicts.

### 4.4 The exact commands

The oracle is driven end to end by:

```bash
make title-pin
python3 tools/title_capture.py --out data/title-captures/title --port-anchor /tmp/pr_title_dump/title
python3 tools/title_capture.py --out data/title-captures/title2
make title-oracle          # needs the port dump at /tmp/pr_title_dump/title
make verify                # the full ladder, exit 0
```

`title-oracle` runs the `test_title.c` driver alone with `PR_TITLE_DUMP` set
(it is the only test that calls `game_init()`, so it cannot share a process with
the unit suite), then compares against every `data/title-captures/*` directory
present.

## 5. Register-argument binding

`__regparm3` hides `0x2AE14`'s register arguments from the decompiler, so they
were pinned by disassembly of the shipped EXE, not inferred. The full binding is
`docs/superpowers/plans/2026-09-17-actor-system-args.md`, with raw bytes for
every claim:

```c
u32 actor_spawn(const u32 *desc, u32 a2, u32 a3, u32 a4, u32 a5); /* 0x2AE14 */
/* desc=EAX, a2=EDX, a3=ECX, a4=EBX (Ghidra's unaff_EBX), a5=stack [esp+0x28] */
```

Evidence: the prologue at `0x2AE14`/file `0x7E068` (`mov ebp,eax`,
`mov [esp+4],edx`, `mov [esp],ebx`, `mov [esp+8],ecx`, then `[esp+0x28]`), the
four title call sites (`0x1221D`, `0x122F1`, `0x1233F`, `0x123BF`), `0x38B18`'s
site (`0x38B5D`: `actor_spawn(desc, a2<<3, 2, a3<<3, 0)`), and the `0x2AC80`
free-list flag (§3 of the args doc: `0x2AC84 mov ecx,eax` / `0x2ACB6 xor cl,cl`
/ `0x2ACBA and ch,4`, so the flag is **arg 5's low 16 bits**, bit `0x400`). The
reviewer re-decoded all of it, which caught a real bug: `actor_alloc` had been
fed `a3`; it takes `a5` (plan errata `66c989c`).

## 6. Integration and transcription defects the oracle forced

Three defects were found only by building the pixel oracle; each is verified
against the decompilation.

### 6.1 `sprite_bank` resolved `pset+0x18` wrongly — the composite was black

Sub-project 4a-i's `sprite_bank` treated `pset+0x18` as a resource handle and
called `res_resolve`. `0x33754` actually returns a **palette-table entry**
address (`{handle@0, refcount@4, start@8, len@12}`, walked over
`&DAT_00107618` in `0x10`-byte steps), and the original's blit reads the entry's
`+8` (`mov eax,[ebx+8]; and eax,0xff`) — the DAC `start` the palette was
uploaded at. The composite was uniformly black until `sprite_bank` was changed
to read the entry's `start` byte (`f55e548`); the header contract and the two
bug-encoding tests were rewritten around fake entries.

### 6.2 The VGA DAC keeps 6 bits

The original's `0x1C470` writes 8-bit gun values to port `0x3C9`, but the VGA
keeps only the top 6 bits. The port stored the values unexpanded/unmasked, so
the composite's colours were wrong. `platform/gfx.c` now masks `&0x3F` and
expands `(v<<2)|(v>>4)` (`0cdb045`, `test_gfx` pins `0x50<<2`).

### 6.3 Actor velocity is 16.16 fixed point

`0x2A4FC` integrates velocity as `+= *(int*)(rec+0x32) >> 16` — a 16.16
fixed-point integer step — not as the low 16-bit word. Read as a word, the
logo moved `(0,-81)` instead of the original's `(-81,+8)`; pset_write already
read the dwords the same way. `motion_step` now sign-extends the dword and
shifts (`actors.c:930`, `0cdb045`).

Reviewers independently re-derived both code fixes from `prage.c` (the palette
blit at `prage.c:7984-7989`, the velocity at `:15484-15485`).

## 7. A confirmed finding that contradicts an earlier cycle's RE

The in-window opcode-8 count is **0**. Task 7's instrumented walk reached only
`0x00`/`0x01`/`0x0D`/`0x0E`/`0x12`/`0x18` for both title streams and never
opcode 8; Task 10's probe on the real title run confirmed the count is zero
regardless of the pin. Consequences, stated plainly:

* Task 1's fourth pin site (`0x7E289`) is **inert** — harmless, since it only
  replaces an unreached draw, but it pins nothing.
* The `task-2-anchor-re.md` account of the title's original nondeterminism —
  that the second title object's first `0x600` bytes hold ten opcode-8 words,
  so in-window `rand()` consumed a host-timed RNG state — is **wrong**. The
  pin's *effect* on the capture is real (it is applied), but its *explanation*
  was not. The two-run divergence that motivated it has no confirmed cause in
  the RNG-draw path; the oracle's determinism proof is what now carries the
  argument.

No committed code depends on the earlier explanation; the port's opcode 8 is
transcribed from the dispatcher and the TEST-ONLY seam remains for the capture
mirror.

## 8. Spec deviations, seams and deliberate omissions

* **`res_alloc` was not added to `res.h`** (spec §5.3 deviation, human-approved
  pre-flight). `res_load_index` already performs exactly the original's two
  allocations — `DS_001014EC` (`0x4880`) and `DS_001014F4` (`0xEBA0`) at
  `port/src/platform/res.c:120-121`; a second allocator entry point would
  allocate the pools twice. `actors_init()` validates what exists.
* **`rng_step`** is the port's own name for `0x255CC`'s discarded draw, placed
  after `swap_buffers()` and before `game_audio_service()`.
* **Capture-API changes from the second draft**: the tool's default mode emits
  the whole post-logo run collapsed to distinct consecutive frames and
  hard-errors when it cannot align; `--port-anchor` aligns the port's frames;
  `--verify-reproducible` is a diagnostic. The dead port-free RNG-sensitivity
  locator was removed.
* **`mem_load_le` was added to `game_init`** (not `res_load_index`): the data
  object holds the title descriptors, streams and tables `0x121A0` reads, and is
  disjoint from `RES_HEAP` (the data object's end), so the two never overlap.
* **The string buffer is at `0x3800000`** (`STRING_HANDLE`/`STRING_DATA`); the
  previous `0x2B00000` collided with `movie_play`'s TWI5 load.
* **`0x47370`'s loader was replaced by a direct `ENGLISH.TXT` read** (a
  permitted port-asset pattern): the original's `0x1C308`/`0x1E6D8` paged-memory
  manager and DOS file I/O are not ported. `DS_00104528` is pinned to `0` — the
  `0x2D974(0x29)` result on the shipped image, which selects the title's text
  branch (`text_cursor_set` → `0x2F198`).
* **`actors_pin_anim_tick_zero(int)` is TEST-ONLY**, set by the Task 10 oracle
  driver, and draws `on ? 0 : rng_next(range)` at the single opcode-8 site.
* **The fake-title machinery was deleted** (`TITLE_FRAMES`, `s_title_*`,
  `title_load`, the `gra_decode_frame` full-screen path) and so was
  `test_gfx.c`'s S16TITLE frame-10 comparison with `s16title_frame10.idx`; both
  are superseded by the oracle.

### PORT labels introduced (62 added lines; grouped)

* `actors.c` (31): pool/lists — empty-sentinel report, pool-range invariant,
  two-pool validation, loader-assumed free list, `0x13DF0` effect-list free,
  free-list rebuild, `0x13ADC`, `0x4F228`, `0x38B70`, `0x52106`,
  `0x2EA30`; spawn — per-type render-check switch (only case `0x00` reachable),
  per-type teardown, unreachable ESI id, layer zero-extend at `0x2A7C5`/`0x2A8D9`;
  sync/motion — the 16.16 velocity note, the two signed layer clamps,
  `0x2B150`; anim — the `0x10`/`0x11`/`0x15` stream-directed call,
  opcode `0x2E`→`0x2C3FC`, opcodes `0x23`/`0x24`→`0x2B8E8`, the TEST-ONLY seam;
  text — the Format-H misnomer note and the id-from-character note.
* `flow.c` (21): `0x4F1E4`, `0x38910`, `0x1E75C`, `0x1E808`, `0x474E4`,
  `0x47370`, `0x1C500`, `0x38B18`, `0x33904`, the dump hook, the voice-cancel
  correction, `0x13C70` deferred, the image-load note, `rng_seed`, the
  `DS_00104528` pin, `PR_TITLE_DUMP`, `0x10DB0`/`0x10E18`, `0x2BF08`.
* `platform/gfx.c` (3): the shared `palette_record` ownership, the moved
  dirty-list head, the 6-bit DAC note.
* `platform/sprite.c` (1): the palette-table-entry contract.

## 9. Deferred operations

Deferred at the time and now owned: Task 5's `0x2A820`/`0x2A620` and
`0x2B2A0`/`0x2A408` stubs were replaced by Tasks 6 and 7; Task 8's
`text_layout_seam()` was replaced by Task 8b's renderer. Still deferred with
`/* PORT: */` markers: `0x13C70` (the `0x13xxx` effect/spawn subsystem, reached
once inside the window when `DS_000F0A66 < 0x11`), `0x10DB0`/`0x10E18` and
`0x2BF08` (`0x11D04`'s tail), `0x292AC`, `0x389C4`/`0x38A38`, `0x134C0`,
`0x4F644`, and the player/fight-engine paths.

## 10. Open items and residuals

### Verdicts on the plan's open items

* **`0x2BF08`** — inerted in the capture (one-byte `ret`). It was suspected as
  the spread source of the two-run divergence; the oracle now proves the
  **ported subset** with `0x2BF08` removed. Its own behaviour is not proven and
  is carried to a later cycle. No in-window divergence is attributable to it
  now that it is inert.
* **`DS_00107900`'s producer (`0x38A38`)** — still unported (4a-i consumes the
  table); out of this cycle, named in `RE_GUIDE.md`.
* **Bank byte 0** — whether any shipped asset ever reaches it is unknown. The
  port pins `DAT_00081310[0]` and maps bank 0 to no offset; a divergence in
  4-pixel groups is latent. 4a-i's `TODO(verify)` stands.
* **`DS_00107A3E`/`3A`/`38` projection form** — the three mode-offset projections
  use the original's uncorrected `(v*num + 0x800) >> 12` with a `(s16)` load the
  decompiler renders as `(uint)`; the globals are non-zero in the title
  (`0x2420`/`0x121`), so the oracle exercises them, but the load width is still
  a `TODO(verify)`.
* **`DS_000F0AEC`** (absolute frame/time base) — left unconfirmed by Task 2's
  RE; the oracle's two-capture agreement is consistent with it being inert for
  the window, but it is not independently disproved.

### Deferred minors, aggregated with disposition

All are non-blocking; none is a data-loss or security path. Grouped by task
(three earlier groups were closed by later tasks' evidence, as noted):

| Task | Item | Disposition |
|---|---|---|
| 2 | `--anchor` unvalidated; capture `--out` has no `data/` guard; one bare `open().read()`; `diagnose()` uses run 1's logs; `--verify-reproducible --port-anchor` raises; AVI decoded twice | Carry. Non-default/diagnostic paths; the Makefile path is safe. `--out` guard should mirror `title_pin`'s. |
| 2 | Fifth pin site absent from Format A2; stale report headers | Carry (doc traceability). |
| 3 | `flow.c:555` provenance comment is a bare address; `test_rng.c:25` redundant CHECK | Carry; cosmetic. |
| 4a | `DS_00101508/150C` written 0 — closed by `0x2BBE4` disassembly (param_1 = 0) | Closed. |
| 4b | `actor_alloc` `0x400` tail-insert path unexercised — unreachable for the title | Carry; documented. |
| 4c/d | stale `test_actors.c` comment; asymmetric reset guard; double free benign | Carry. |
| 6.1 | two `TODO(verify)` layer clamps with a wrong rationale — Task 7 reworded; clamps correct | Closed (reason fixed). |
| 6.2 | C99 UB `((s32)x >> 16) << 6` in four spots — Task 7 fixed | Closed. |
| 6.3 | `set_dead` omits per-type teardown + `rec+0x2b &= 0xbf` — unreachable while the dispatcher was stubbed; now transcribed in `actor_set_dead` | Closed for the title; the per-type table's other cases remain unported. |
| 7a | `actors.c:1243` comment names the wrong status-2 opcode set | Carry; cosmetic (direct `0x1F` falls through to `0x20`; status 2 is `0x00/0x01/0x05/0x07/0x08/0x15`). |
| 7b | opcode `0x15` callee convention approximated (ECX vs the second `anim_code_fn` slot) | Carry; pre-existing seam, title streams do not reach it. |
| 8 | `text_cells_release` wrap/clamp branches untested | Carry; the title passes `col = -1`. |
| 8 | `text_cursor_set` is a seam-stub note stale in `actors.h` — Task 8b replaced it | Closed. |
| 8b | unguarded truncation write `m[k-1]` for `col > 0x2A` — verified identical in the raw, not title-reachable | Carry. |
| 8b | writable-string test cases missing; dead initialiser | Carry. |
| 9 | loader drops `0x33340001` signature check; `logo` deref without guard (faithful); two tests share scratch `0x3F00000`; boot-path RNG state not test-covered; `string_decode` unbounded; glyph-count assertion `>= 10` against 12; `DS_00104B15` pinned 0 unverified | Carry. The signature check is cheap to restore. |
| 10 | clause B returns the first passing pair, not all | Carry; verdicts identical on the real captures. |
| 10 | the two transcription fixes (gfx DAC, velocity) sit outside the brief's original file list | Recorded; in scope by necessity. |
| 9 | `Makefile`'s `verify` comment still says `test_gfx.c` consumes `frames/frame_*.idx`; that comparison was deleted in Task 9 | Carry (comment only; `make verify` is correct). |

### Residuals carried into later sub-projects

* **4b (menus/EEPROM)**: the `0x2D974` save/config record subsystem — the port
  pins its `0x29` result to 0 (`DS_00104528`, `DS_00105B3A`, `DS_001088D0`,
  `DS_0010452C`) and must revisit the pin when the extractor is ported.
* **4c (fight engine)**: the two `0x94`-byte player records and `0x24C5C`'s
  input loop; `0x2BF08`; `0x10DB0`/`0x10E18`; the unported subsets of the
  per-type render/teardown tables (`DS_000BB9DC`/`DS_000BB9E0`).
* **4d (attract/scene)**: `0x11000`'s attract sub-machine, `0x13C70`/the
  `0x13xxx` effect subsystem, `0x292AC`, `0x389C4`/`0x38A38` (`DS_00107900`'s
  producer), `0x134C0`, `0x4F644`, and the `0x121A0` mode-1 sprite branch
  (`DS_00104528` bit 9 set) which the shipped profile never takes.
* **4a-i**: bank byte 0 and the mode-offset projection load width (§10).
* **2b-ii**: streamed Smacker audio (unaffected).

## 11. Verification

`make verify` on this cycle (clean rebuild, exit 0):

```
== headless frames … ==   ./build/prageport --game-dir data/game/C --check 60
== tests (oracles required) ==   PR_ORACLE_REQUIRED=1 … ./build/run_tests
  oracle C-vs-Python: 9340 writes byte-exact
  all checks passed
== smacker frame oracle ==   smk_compare: 120/120 frames match; 41/41 frames match
== title oracle ==   title_compare: 110 frames in window: 53…/53… clean, 0 unexplained;
  determinism: 36 port frames agree, 0 disagree
== gra_extract oracle tests ==   Ran 32 tests; OK
== symbols.h must regenerate byte-identically ==
  1304 globals, 1206 functions -> port/src/symbols.h
  all checks passed
```

Zero compiler warnings in the full build (the only diagnostics are Python
`ResourceWarning`s from the pre-existing `gra_extract` tooling, not the C build).
`symbols.h` regenerates byte-identically. The title oracle cannot be skipped
under `PR_ORACLE_REQUIRED=1` and now fails when the determinism proof is
incomplete.

## 12. Honesty statement

The project may claim:

* the actor pool, its lists, spawn (`0x2AE14`) with the register arguments
  pinned by disassembly, the pset sync/motion, the animation-stream interpreter
  (including all 47 dispatcher opcodes), the text grid and glyph renderer, the
  real `0x121A0` title state and the `0x1C500` caption chain are ported and
  unit-tested;
* the title composite is **pixel-exact** against two independent captures over
  the 96-frame window: every captured frame is explained as a byte-offset splice
  of two adjacent port frames (or one byte-wise transition row), with zero
  unexplained frames, and the clean samples of 36 port frames agree between the
  captures with zero disagreements;
* three integration/transcription defects found by the oracle (palette-table
  entry resolution, 6-bit DAC expansion, 16.16 velocity) are fixed and verified
  against the decompilation;
* `make verify` is green with zero warnings, `data/` untouched, no new
  dependency, and the oracle's determinism proof completed.

The project may **not** claim:

* that the two-run divergence originally attributed to in-window opcode-8
  `rand()` is explained — the in-window opcode-8 count is 0, pin site 4 is
  inert, and `task-2-anchor-re.md`'s account is wrong;
* that the original's window was compared frame-for-frame without a tear model
  — ~1 captured frame in 6 is a tear, modelled as a byte-offset splice with zero
  tolerance, not as a tolerance;
* that `0x2BF08`, `0x13C70`, `0x10DB0`/`0x10E18`, `DS_00107900`'s producer, bank
  byte 0, the mode-offset projection load width or `DS_000F0AEC` are settled —
  each is named as a residual;
* that the port's title path is the original's for the deferred attract
  sub-machine or the mode-1 sprite branch, which the shipped profile never
  reaches.

## 13. Final status

Sub-project 4a-ii's scope is complete: the RNG, the actor system, the text
renderer and the real title state are ported; the DOSBox title oracle is in
`make verify` and proves the composite against two captures with zero
unexplained frames. The cycle's DoD — the port-vs-two-captures determinism
proof, a green ladder with no new warnings, no `data/` writes and no new
dependency — is met. The named residuals in §10 pass to sub-projects 4b/4c/4d,
with `0x2BF08` and the deferred attract/effect subsystem the largest.
