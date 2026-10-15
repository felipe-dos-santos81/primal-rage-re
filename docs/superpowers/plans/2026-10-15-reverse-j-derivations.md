# Track J derivations record — close the ledger, run the captures (2026-10-15)

Plan: `docs/superpowers/plans/2026-10-15-reverse-j-closeout.md`.
Spec: `docs/superpowers/specs/2026-10-15-track-j-design.md`.
Sections are per task (§J1, §J2, …). Lane j1 owns §J1 and lane j3 owns §J2;
each created this file independently, and the integration merges the sections
in order. Byte evidence is inline; every address below was read from the
committed Ghidra decompilation (`port/decomp/prage.c`), the fixed-up image
record (`docs/superpowers/plans/2026-10-13-reverse-s-derivations.md`) or the
raw file bytes, as cited.

## §J2 — host rebind: `host_key_bits` reads the configured binding scans (lane j3, task 4)

**The address (plan correction).** The task brief and the plan's Task 4
Interfaces say "track S's config image at `0x100CE4` (binding scans
`+0x2DE..+0x2ED`)". The raw disagrees with that base, and per the standing
rule the raw wins:

- The sampler reads the scans through the BIOS key record pointer:
  `FUN_0001b610` (`prage.c:7312`) tests `[DAT_00101514 + 0x254 + *[DAT_00101514
  + 0x2de]]`, `FUN_0001b730` (`prage.c:7362`) `+0x2e2..+0x2e5`, and
  `FUN_0001b6a0`/`FUN_0001b7c0` (`prage.c:7337`, `7384`) `+0x2e6..+0x2ed`;
  `FUN_0001b850`/`FUN_0001b870` (`prage.c:7406`) test the fixed F1/F2 state
  bytes `+0x28F`/`+0x290`. `DAT_00101514` is the dword holding the DOS block
  base (`flow.c:6862` sets it to `GAME_BIOS_BASE`, the port's flat scratch),
  so the scans are at `[DS_00101514]+0x2DE..+0x2ED`, not at
  `0x100CE4+0x2DE` (= `0x100FC2`).
- The probe agrees: `probe.log` line 3 is
  `K ms=5785 ptr=0000FE20 2d4..2ed=000000000000000000001f2d2c2e1617313248504b4d47494f51`
  — the dump is `ptr+0x2d4..0x2ed` of the `[0x101514]` block, and the 16 scans
  (`1F 2D 2C 2E 16 17 31 32 48 50 4B 4D 47 49 4F 51`) sit exactly at `+0x2DE`.
- `config_keys_apply` (`config.c:1282`; 0x1AE85/0x1AEC0) writes the
  player's scans to `DSB(bios + 0x2DE + i)` / `+ 0x2E6 + i` from a 0x28-byte
  record; `config_keys_load` (0x1AF64; 0x1AF99/0x1AFBE) does the same from the
  packed record in the 0x20CF0/0x20CF9 round trip. The boot record is the
  constant at `0xA2C62` (0x1AE20, `config_keys_apply_defaults`): its bytes
  `00 00 73 1F 78 2D 7A 2C 63 2E 75 16 69 17 6E 31 6D 32 00 00 00 48 00 50 00
  4B 00 4D 00 47 00 49 00 4F 00 51 64 00 64 00` (file offset `0xA2C62 +
  0x46E54`, read for this closure) have the scans as their high bytes.
- `0x100CE4+0x2DE` is a different region: image offset 0x2DE lies inside kind
  6's codeword (image bytes `0x100ED0..0x100FCF`; §S.1's kind table), i.e. the
  encoded high-score table, not scans. The image *does* carry a key-config
  mirror in its unmanaged tail at `0x1014AC..` (scan at `+0x1014AE+2i` and
  `+0x1014C0+2i`; `config.c`'s §50-C comment), and a saved rebind round-trips
  through it (0x20CF2 packs it to a record, 0x20CF9 loads the BIOS scans back),
  which is likely what the plan's "the config image holds the scans" meant; the
  authoritative read for the sampler is still the BIOS record. A raw-file
  search finds the contiguous default-scan sequence nowhere in PRAGE.EXE.

**What changed.** `host_key_bits` is the host's stand-in for 0x1BBAC; it now
resolves each key's scan against the configured binding, with the boot record's
scans as the per-position fallback (`port/src/host.c`):

- `k_bios_default_scan[16]` (the 16 positions' boot scans in
  `+0x2DE..+0x2ED` order) and `k_bios_pos_bit[16]` (the kb-word bit each
  position contributes) replace the old hardcoded switch;
- `host_kb_bit(scan)` returns the OR of every position whose *effective* scan
  (the record's, or the default when the record byte is 0) equals `scan`, plus
  the fixed F1/F2 bits (0x3B → 0x0100, 0x3C → 0x0001; 0x1B850/0x1B870 read
  `+0x28F`/`+0x290`). A duplicate binding therefore sets every bit the raw's
  per-position combine would.
- `host_key_bits` keeps the override check as its first statement (the seam
  `host_set_key_bits_override`, host.c:104-108, is untouched) and its SDL
  loops; the per-key result is `host_kb_bit`. `host.c` now includes `mem.h` and
  `symbols.h` to read `[DS_00101514]+0x2DE..+0x2ED`; `host.h`'s two doc
  comments were updated to describe the configured binding.
- PORT deviation, named: the raw has no fallback (a zero scan indexes state
  byte 0); the port substitutes that position's default scan so the no-config
  case (host calls before `game_init`, the unit suite) reproduces the previous
  behaviour exactly. In a run of the port the record is never zero after
  `game_init`'s `config_validate` → `config_set_defaults` →
  `config_keys_apply_defaults`, so the fallback is not reached in play.

**Tests (RED then GREEN).** `port/tests/test_platform.c` `test_host`, same
block, seed-then-post-condition: a scratch record at `0x03F00000+0x2DE`
(local `DSB`/`DSD` save/restore of the base and both byte ranges) is seeded
with a rotation of the 16 default scans (position i holds position (i+1) mod
16's default), then zeroed. RED at base — the assertions ran against the old
switch, this change's exact inverse — 4 failures at
`test_platform.c:2978/2979/2980/2985`: `16384 != 32768` (host_kb_bit(0x2D)),
`32768 != 8` ((0x1F)), `64 != 128` ((0x50)), `64 != 160` (two positions, one
scan). GREEN after the change: all pass,
including the duplicate (`0x50` bound at positions 8 and 10 → `0x0080 | 0x0020`),
F1/F2 fixed, and the zeroed-record case reproducing the 18 default scan→bit
pairs and the three non-pad scans (Q, Alt, '5').

**No pinned path moved.** `PR_ORACLE_REQUIRED=1 ./build/run_tests` with the
replay seam: the gp-pads replay + `gp_compare --report` (private dump
`GP_DUMP=/tmp/pr_gp_dump_j3`, T3 owns `/tmp/pr_gp_dump`) is byte-identical to
the base run of the same command (diffs empty; "0 restart(s) landed", the two
`0x5D812` miss rows, the frame/trace report all equal). The `PR_GP_DUMP` driver
holds the override, and `host_key_bits` returns before SDL/mem are touched.
Suite environment note (this Linux/aarch64 box, independent of the change): a
direct `run_tests` aborts after `test_host` in `test_p45_p5_spawns`
(`actor_spawn`, `actors.c:3865`) — measured the same at base (the pre-change
test file rebuilt from `git stash`) — and `test_platform.c:757` fails because
the resource index names are lowercase while the checkout's files are uppercase
(case-insensitive filesystems only). Under gdb (ASLR off, the only way it
completes here) the whole suite runs: base `FAILURES: 5` (the resource one +
the 4 RED), after the change `FAILURES: 1` (the pre-existing resource one).

**SDL failure-branch reachability on this host (Linux/aarch64, SDL3 3.4.18),
deterministic runs, no forced tests:**

- Reachable: the windowed path opens with the dummy driver
  (`SDL_VIDEODRIVER=dummy`), audio opens (`49716 Hz`), the game runs; a
  SIGTERM (what `timeout` sends, which SDL3's default handler turns into
  `SDL_EVENT_QUIT` → the port's quit flag) ends the loop cleanly and
  `game_shutdown`'s `config_file_save` writes the 2040-byte save — measured,
  not the idle reboot: `timeout -s KILL` leaves no file, `timeout -s TERM`
  (1 s is enough) writes it.
- Reachable (forced, deterministic): the no-display failure branch — both
  `SDL_VIDEODRIVER=doesnotexist` and `DISPLAY=:99 SDL_VIDEODRIVER=x11` give
  `prageport: no window/display available`, exit 1 (host_init's `SDL_Init`
  arm); the audio-init failure arm — `SDL_AUDIODRIVER=doesnotexist` gives
  `audio unavailable (SDL_InitSubSystem(SDL_INIT_AUDIO): ...); running silent`
  and the game runs on (the host has an HDMI ALSA sink, so the natural default
  is success, unlike the macOS `-66681`).
- Not reachable here: `SDL_CreateWindow`/`SDL_CreateSurface` failure after a
  successful video init (the dummy driver accepts everything),
  `SDL_OpenAudioDeviceStream` returning NULL or `SDL_ResumeAudioStreamDevice`
  failing after a successful audio-subsystem init, and the defensive
  `SDL_GetKeyboardState(NULL) == NULL` arm. These stay unexercised rather than
  forced (brief: no forced tests for unreachable branches).

**Live-play notes (notes only; assembled from the records and the runs above).**
The playable key map is the boot binding: P1 S X Z C (up/down/left/right),
U I N M (the four attack buttons), F1 (start, also button 0); P2 the arrows,
Home/PgUp/End/PgDn and F2 (start/button 0), and F2 mid-match joins the second
human side (0x2CA93);
Enter confirms menus, ESC backs out (and at the title/menu prompts reaches
QUIT TO DOS?, whose yes quits; elsewhere it reaches ABANDON CONQUEST?, which
saves and soft-restarts), Space pauses a match, Backspace is DEL in name entry,
Alt-Q/S/M/J are the int 16h shortcuts, and closing the window is the host's
quit (it works even while a movie would swallow ESC; on this host SDL turns a
SIGTERM into that same quit, and the clean exit writes the save). After this
change a rebind set in the service menu's CONFIGURE KEYBOARD takes effect for
the next frames, and it is saved/loaded with the session (the image tail mirror
round trip); the idle timer soft-restarts into the boot logos after ~20 s with
no latched key (0x2EBB3; record named-gaps A §A.6 — record-based, not re-measured
here); "credits" remains today's behaviour — no coin key, credits come from the
config image (INSERT 1 COIN with no way in except the hidden service
menu/config), not implemented.
