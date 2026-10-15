## §J2 — Host rebind: `host_key_bits` reads the configured binding scans

Task 4 of `docs/superpowers/plans/2026-10-15-reverse-j-closeout.md`. Files:
`port/src/host.c` (`host_kb_bit`, the default table kept as the fallback),
`port/src/host.h` (the two comments), `port/tests/test_platform.c` (`test_host`),
`port/spec/game_flow.md` (the one stale "default binding" sentence). The replay
seam `host_set_key_bits_override` (host.c:101-108 pre-change) is untouched and
still answers first: no driver's pad word can move.

### J2.1 The raw: where the binding scans live

The port's sampler stand-in reads the *same bytes the raw's sampler reads*: the
device-0 keyboard arms of `0x1BBAC` take their scans out of the key-config
block at `[DS_00101514]`, by position, and OR the position's bit into
`+0x2D8`/`+0x2D9` (record gameplay-ground-truth §G.1.2):

| routine | block bytes | bits (of the player's byte) |
|---|---|---|
| `0x1B610` | `+0x2DE`, `+0x2DF`, `+0x2E0`, `+0x2E1` | `0x80 0x40 0x20 0x10` |
| `0x1B730` | `+0x2E2`..`+0x2E5` | `1 2 4 8` |
| `0x1B6A0` | `+0x2E6`..`+0x2E9` | `0x80 0x40 0x20 0x10` |
| `0x1B7C0` | `+0x2EA`..`+0x2ED` | `1 2 4 8` |
| `0x1B850` | `+0x28F` (fixed) | bit 0 |
| `0x1B870` | `+0x290` (fixed) | bit 0 |

(Decompiled from the Ghidra project `primal-rage` `/PRAGE.EXE`: each routine
tests `(*(byte *)(DAT_00101514 + 0x254 + scan) & 0x80) == 0` with
`scan = *(byte *)(DAT_00101514 + <offset>)` — the block byte is the scan, and
`0x1BBAC`'s case 0 combines `0x1B610|0x1B730|0x1B850` into `+0x2D8` and
`0x1B6A0|0x1B7C0|0x1B870` into `+0x2D9`.) The kb word the port's `game_loop`
writes is `(+0x2D8 << 8) | +0x2D9` (host.h, flow.c), so the position table in
`port/src/host.c` is `k_cfg_bit[16]` = `0x8000 0x4000 0x2000 0x1000 0x0100
0x0200 0x0400 0x0800 0x0080 0x0040 0x0020 0x0010 0x0001 0x0002 0x0004 0x0008`.

The block is filled by `config_keys_apply` (`0x1AE28`, bytes `+0x2D4..+0x2ED`
from a 0x28-byte record) from either the default record at `0xA2C62`
(`0x1AE20`, called by `config_set_defaults` `0x2CADC`, the no-magic arm of
`config_validate` `0x2D6F8`) or the service menu's edited record
(record §50-C). At boot the tail of the restart point re-loads it from the
**config image's mirror**: `0x20CF0 mov eax,esp`, `0x20CF2 call 0x1AEE0`
(`config_keys_pack`, mirror `0x1014AC..` → the record), `0x20CF7 mov eax,esp`,
`0x20CF9 call 0x1AF64` (`config_keys_load`, the record → `+0x2DE..+0x2ED`),
disassembled from `/PRAGE.EXE`; the mirror sits inside the image
`0x100CE4..0x1014DB` that `config_file_load` (`0x1AFE8`) reads, at
`0x1014AE + 2i` (P1) and `0x1014C0 + 2i` (P2) — not contiguous, which is why
the binding scans are named by their *block* offsets `+0x2DE..+0x2ED`
(the port follows the block, the bytes the sampler reads; reading the mirror
directly would read a different copy the raw never samples live).

Default record bytes (obj-1 file offset = VA + 0x46E54; `0xA2C62` →
`0xE9AB6`): `00 00 73 1f 78 2d 7a 2c 63 2e 75 16 69 17 6e 31 6d 32 | 00 00
| 00 48 00 50 00 4b 00 4d 00 47 00 49 00 4f 00 51 | 64 00 64 00` — P1 scans
`1F 2D 2C 2E 16 17 31 32`, P2 scans `48 50 4B 4D 47 49 4F 51`, i.e. exactly
the default binding `host_kb_bit`'s switch encoded (record §G.1.2, §K.10
item 8).

### J2.2 The port change

`host_kb_bit(scan)` now reads the 16 scans at
`mem + DSD(DS_00101514) + 0x2DE .. +0x2ED` (whole table inside `mem[]`;
`base == 0` or `base > MEM_SIZE - 0x2EE` is treated as absent) and ORs
`k_cfg_bit[i]` for every position whose scan equals `scan` — the sampler tests
each position independently, so a scan twice in the table gets both bits —
then ORs the fixed `+0x28F` bit for `0x3B` (`0x0100`) and `+0x290` for `0x3C`
(`0x0001`). `host_key_bits()` is unchanged: it still walks the pressed SDL keys
through `k_bios_letter`/`k_bios_pad` and calls `host_kb_bit` for each.

**PORT deviation (named).** With no block (no `game_init`, or all 16 scans
zero) the function falls back to the default binding — the switch that was the
whole body before. The raw's sampler would report no pad key for an empty
block; the fallback exists because the port's headless callers and any run
whose image records no binding would otherwise have no keyboard at all. In the
windowed run the block is never empty: the no-magic path writes the default
record (`config_set_defaults`), and an image with the magic carries its own
mirror through the `0x20CF2`/`0x20CF9` pair, so the fallback is the
absent-block case only.

`host_set_key_bits_override` (host.c) still short-circuits `host_key_bits()`
before any SDL or block read.

### J2.3 Tests (`test_host`, `test_platform.c`)

Seeded, both directions, all able to fail (planting uses the suite's free
scratch `0x03F00000`; `DS_00101514` is saved and restored):

- the default pins (18 scans + three zero pins) now run with an **absent**
  block (`DS_00101514 = 0`) so they cannot depend on whatever a previous test
  left in a block (test_platform.c:2961-2967);
- a non-default table (scans `0x11..0x14, 0x21..0x24, 0x31..0x34, 0x41..0x44`
  — no default scan in it): the 16 position checks (:2990), the three
  not-in-table checks (`0x1F`, `0x48`, `0x50` → 0, :2991-2993), the fixed F1/F2
  bits (:2995-2996), the duplicate-position OR (:3000), F1 in the up slot
  (:3003) and the slot-gone re-read (:3004);
- a **zeroed** table falls back to the default (two pins, :3008-3009);
- the override seam with a non-default table planted (:3110-3121): the sentinel
  `0x1234` still wins, and is not returned with the seam off.

Evidence (all on this worktree):

| run | result |
|---|---|
| `./build/run_tests` before the change | `FAILURES: 22` — the 16 positions and 6 of the configured checks fail against the old default-only body (`0 != 32768` …) |
| `./build/run_tests` after | exit 0, `all checks passed` |
| `PR_ORACLE_REQUIRED=1 ./build/run_tests` before vs after | **byte-identical output**, `all checks passed`, `oracle C-vs-Python: 9866 writes byte-exact` (12 lines, no pinned line moved) |

Mutation proofs (each restored; `M1` = the fallback removed, `M2` = bits 0/1 of
`k_cfg_bit` swapped, `M3` = the table cached after the first call, `M4` = the
`+0x28F`/`+0x290` lines dropped, `M5` = no `base == 0` guard and the empty
table authoritative):

| mutant | failing checks |
|---|---|
| M1 | :3008, :3009 (the zeroed-table fallback) |
| M2 | :2990 ×2 (the swapped positions), :3000, :3003 |
| M3 | :3000, :3003, :3004 (stale slot), :3008, :3009 |
| M4 | :2995, :2996, :3003 |
| M5 | :2963 ×18 (the absent-block default pins) |

### J2.4 The seam and the three controlled cases

Per the controller's resolution, no gp replay/oracle ran (lane `j2` holds
`/tmp/pr_gp_dump`); the seam is proven by unit case instead:

- **(a) override set, a non-default table planted** — `host_key_bits()` returns
  the override `0x1234` (test_platform.c:3116-3117): the replays' pad words
  cannot read the configured scans.
- **(b) override unset, configured scans win** — proven through `host_kb_bit`,
  the mapping `host_key_bits()` applies to every pressed key (the planner's
  `host_kb_bit` is the sampler's per-key combine; `host_key_bits` is the SDL
  walk). The SDL side itself cannot be driven headless: `SDL_GetKeyboardState`
  returns SDL's internal all-zero array and no public SDL3 call seeds it (the
  standing named gap: no test seeds the SDL keyboard state, §K.13 item 1 /
  §K.10 item 1).
- **(c) zeroed/absent scans → defaults** — `DS_00101514 = 0` (:2963) and a
  zeroed block (:3008-3009), including `M1`/`M3` proving both can fail.

No pinned path moved, and none can: every oracle driver is headless (SDL
reports no key pressed, so the mapping is never consulted) and the two replay
drivers hold the bitmap through `host_set_key_bits_override`; the
oracle-required run's byte-identical output is the measured form of that
argument. The gp replays themselves were left to the lane that holds the
dump; `make verify` will re-measure them at integration.

### J2.5 SDL failure-branch reachability (report, no forced tests)

Exercised with a scratch probe (`host_init`/`host_audio_open` linked against
`build/libprage_core.a`, run in the OS temp dir; none of it is repo content):

| branch | reachable here? | evidence |
|---|---|---|
| `host_init` w/h guard | yes, already tested headless | `host_init("pr-test",0,0) == 0` |
| `host_init` `SDL_Init(VIDEO)` fails | **yes, deterministically** | `SDL_VIDEODRIVER=nonexistent` → `host_init -> 0  has_window=0  sdl_error="nonexistent not available"`; the windowed binary prints `prageport: no window/display available`, exit 1 |
| `host_init` success (window + surface) | yes, headless with the dummy driver | `SDL_VIDEODRIVER=dummy` → `host_init -> 1 has_window=1`, `present+pump ok` |
| `SDL_CreateWindow`/`SDL_CreateSurface` NULL | no deterministic trigger here (resource exhaustion only; the dummy driver creates a window) | not reached |
| `host_audio_open` profile guard | yes, already tested | `host_audio_open(0,2)/(44100,0)` → 0 |
| `SDL_InitSubSystem(AUDIO)` fails | **yes, deterministically** | `SDL_AUDIODRIVER=nonexistent` → `err="SDL_InitSubSystem(SDL_INIT_AUDIO): Audio target 'nonexistent' not available"` |
| `SDL_OpenAudioDeviceStream`/`SDL_ResumeAudioStreamDevice` success | yes, headless with `SDL_AUDIODRIVER=dummy`, and on this host's real driver | `host_audio_open(49716,2) -> 1 rate=49716`, submit accepted |
| `SDL_OpenAudioDeviceStream` NULL (working driver) | no deterministic trigger here | not reached |
| `SDL_ResumeAudioStreamDevice` fails | **not reproducible here today** — the standing `-66681` (`AudioQueueStart`) observation (2026-09-16 audio report) does not reproduce: this host now reports `current audio driver: coreaudio`, 3 playback devices (LC49G95T, Realtek USB2.0 Audio, `Alto-falantes (MacBook Pro)`), and open+resume succeed; the windowed binary (dummy video, scratch `--cmos`) prints `prageport: audio device open at 49716 Hz` | measured |

That last row is a corrected fact, not a change: the "SDL audio cannot open
here (-66681) / the windowed run is silent" sentence in `AGENTS.md`, PROGRESS
and the track-G/J designs is stale on this host as of 2026-10-15 (the machine
gained a working playback route since 2026-09-16). Audio bring-up stays queued
per track J's ruling — a device-capable host is now available.

### J2.6 Live-play notes (notes only)

**Keys and behaviour of the windowed run.** In a match the default binding
gives player 1 up/down/left/right on `S`/`X`/`Z`/`C`, the four buttons on
`U`/`I`/`N`/`M` and start on `F1`; player 2 takes the arrows, `Home`/`PgUp`/
`End`/`PgDn` and `F2` — and the service menu's key config now changes those
(the port follows the block the game writes, defaulting to the same list).
Menus and prompts take `Enter`, `Esc`, `Space`, `Backspace` and the letters as
BIOS words. `Esc` in the front-end asks `QUIT TO DOS? Y/N` (yes ends the run),
in a match asks `ABANDON CONQUEST? Y/N`, and does nothing in the service menu;
`Space` shows `- PAUSED -` outside modes 3 and `0x27` (another space resumes);
`Alt-Q` asks `QUIT TO DOS?` in every mode, `Alt-S` toggles the samples,
`Alt-M` the music; closing the window is the host's own quit signal and is
distinct from any key. Left alone in a menu the game reboots itself: the
blocking waits' 20-second idle timeout (0x4B0 ticks at 60 Hz, `0x2EB80`)
soft-restarts through the game's own longjmp back to the attract/title. The
title's `CREDITS:<n>` count comes from the config image at boot (field 0x29's
nibble + 1; 5 on the shipped zero image); starting a game takes one credit,
there is no coin key and no free-play key in this build (record §K.4), and the
count is per-run (the debit is RAM, not written back by the debit itself).

### J2.7 Named gaps that stay

- The SDL scancode → BIOS scan table (36 keys) is still untested headless and
  is still the only way a pressed key reaches the map: a rebind to a key
  outside `k_bios_letter`/`k_bios_pad` (Enter, digits, F3..) is not reachable
  in the windowed port (§K.10 items 1 and 7's family).
- `+0x28F`/`+0x290` (F1/F2) are not part of the 16-scan config record — the
  record's 0x28 bytes carry the two devices and the 16 key words only
  (`config_keys_apply` `0x1AE28`) — so their bits stay fixed in the port as
  the raw's fixed +0x28F/+0x290 reads are; no writer of those two block bytes
  is established here.
- Typematic repeat and the joystick device words `+0x2D4`/`+0x2D6` (a
  non-zero device switches `0x1BBAC` to the game-port arms) stay out, as
  before (§K.10 items 9 and 10).
