# Storage layer: the EEPROM image, the audit counters and the `cmos` file — design

Date: 2026-10-13. Track S. Supersedes §7 of `2026-09-19-eeprom-config-design.md`
("No save/load I/O, hence no storage image") for the functions listed here.

## 1. Goal

Replace the port's declared no-op storage layer with the original's code. The
game's settings, high scores and audit counters then live in the `0x7F8`-byte
image at `0x100CE4`, as in the original, and persist in a save file.

Success is:

- the 14 deferred storage functions are ported, one C function per original
  function, and each has a differential row with mutants;
- `0x2D4EC`, today a ported declared no-op, does what the raw does;
- the `PORT:` notes that stand in for these calls are replaced by the calls;
- a windowed run keeps its settings and high scores across runs;
- `make verify` is green. Its oracle sections are identical to the last gate
  log, or every moved pin is re-measured against the captures and recorded with
  the address that proves the correction.

## 2. Scope

| Group | Addresses | Today |
|---|---|---|
| Image and validation | `0x2E990`, `0x2D498`, `0x2D638`, `0x2DE98`, `0x2DF8C` | deferred |
| | `0x2D4EC` | ported as a no-op (`config_storage_touch`) |
| Audit counters | `0x2DAE4`, `0x2E934`, `0x2E180`, `0x2E0A4`, `0x2E034`, `0x32B94`, `0x32BB0` | deferred |
| File read and write | `0x1AFE8`, `0x1B084` | deferred |

Call sites, from a rel32 `call`/`jmp` scan of the fixed-up code object. Each
one that the port omits today under a `PORT:` note becomes the raw's call:

| Callee | Sites |
|---|---|
| `0x1AFE8` | `0x2F9CF` (in `0x2F9CC`, which the port inlines in `game_init` before `config_validate`) |
| `0x1B084` | `0x1BEB5` (teardown `0x1BE30`, `game_shutdown`), `0x24AA1` (`flow.c`), `0x2CBAF` (`svcmenu.c`) |
| `0x2D638` | `0x2D71B` |
| `0x2E990` | `0x2D64E`, `0x2D673`, `0x2D6D0` (`0x2D638`); `0x2D729`, `0x2D745`, `0x2D8DF` (`0x2D6F8`); `0x2DF08` (`0x2DE98`); `0x2DFC0` (`0x2DF8C`) |
| `0x2DE98`, `0x2DF8C` | `0x2D912`, `0x2D919` |
| `0x2D498` | `0x2D612` (in `0x2D4EC`) |
| `0x2DAE4` | 20: `0x1E9BB`, `0x1E9EB`, `0x1EC50`, `0x1EC84`, `0x270A9`, `0x27A5F`, `0x27F1F`, `0x28E53`, `0x2D962`, `0x32A17`, `0x32A85`, `0x32A9B`, `0x32AAA`, `0x32AB6`, `0x32ADE`, `0x32AF5`, `0x32BA3`, `0x32BC0`, `0x32BD2`, `0x42425` |
| `0x2E934` | `0x32B35`, `0x32B7E`, `0x43E77` (`fight.c`) |
| `0x2E180`, `0x2E0A4`, `0x2E034` | `0x2E983`, `0x2E1EB`, `0x2E20B` |
| `0x32B94`, `0x32BB0` | `0x2786B`, `0x27E28` (`flow.c`) |

`0x2D4EC` has 18 sites. The port already calls `config_storage_touch` at the
ones it reaches, so making the body real changes every one of them at once.
Four sites in `config_validate`'s version arms (`0x2D7A9`, `0x2D7C0`, `0x2D7EB`,
`0x2D802`) are not named anywhere in the port today and are derived in task 1.

Out of scope:

- the 13 movie-audio functions (`0x10024` … `0x10A68`) and movie sound;
- the run clock `0x32970` (host-owned: the host owns wall time). Its own audit
  add at `0x32A17` stays omitted and stays named;
- the CD-drive wrappers `0x10D34`/`0x10D0C` that bracket the file I/O
  (host-owned, record §50-E; they stay no-ops);
- the error printer `0x2EA68`, a named no-op.

## 3. What the bytes say so far

From Ghidra's listing and decompilation (fixups applied). Task 1 re-derives it
and runs the original in the harness before any port code changes. None of
this is a port constant yet.

- `0x1AFE8` opens `cmos` (`0xA2C5C`, mode `"rb"` `0x80598`), reads `0x7F8` bytes
  to `0x100CE4` and XORs bytes `0..0x7F6` against the last byte `0x1014DB`. A
  missing file, a short read or a checksum mismatch fills the image with `0xFF`
  (`0x61A70(0x100CE4, 0xFF, 0x7F8)`).
- `0x1B084` opens `cmos` for `"wb"` (`0x8059C`), calls `0x2D4EC(1)` and
  `0x2D4EC(2)`, stores the XOR of bytes `0..0x7F6` in `0x1014DB` and writes
  `0x7F8` bytes.
- `0x2E990(offset, dst, len)` is not host I/O. It decodes a record from the RAM
  image, one stored byte per two image bytes, with a Hamming-style syndrome over
  eight parity bytes. It returns 1 when the syndrome is zero (`0x2EA1C`) and -1
  otherwise (`0x2EA23`); the listing has no third value. The `< -1` tests in
  `0x2D638` and `0x2DE98` therefore test a value the caller sets itself (record
  §49-Y.5: `0x2DE98` uses -2 for a skipped read); task 1 pins each one.
- `0x2D4EC(kind)` is the encoder: it locates the record through the kind table
  `0x2D444`, rejects a write past `0x7F8`, and stores each byte through the
  bounds-checked `0x2D498` (`0x100CE4 <= p < 0x1014DC`).

**The condition every capture ran under.** `data/game/C/CMOS` is 2040 zero
bytes, and `tools/k11_capture.py` `check_cmos` refuses any other content. The
game dir is mounted so the save cannot write. The XOR of an all-zero image is 0
and equals its last byte, so the original kept the zero image, and a zero image
gives `0x2E990` a zero syndrome (every byte it XORs at `0x2E9E8` and `0x2E9FC`
is zero): **the read returns 1, not the -1 the port declares today.** `config_validate` still reaches the defaults arm, because the
magic `0x9C94D2C4` is absent from a zero image, but through a different test
than the port's comment claims. Task 1 settles whether any state differs.

## 4. Design

### 4.1 Code

All 14 functions and the `0x2D4EC` body go in `port/src/game/config.c`, which
already owns the kind table walk (`0x2D4B4`), `hiscore_locate` (`0x2DB58`) and
the callers. `0x1AFE8` and `0x1B084` go there too: they are game code that
calls the host for bytes.

The image is original state at its original address. No C global shadows it.

A function whose raw decodes into its own stack frame (`0x2D638`'s two
`0x51`-byte buffers) follows the existing convention for the differential
harness: the buffer is passed by value at the seam (record
`2026-10-02-reverse-p2-derivations.md` §P2.7).

### 4.2 The save file

SDL and file I/O stay in `host.c` and `main.c`. `config.c` gets the bytes
through two host calls and never sees a path:

```c
int  host_save_read(u8 *dst, u32 len);        /* 1 = len bytes read */
int  host_save_writable(void);                /* 1 = the "wb" open would succeed */
int  host_save_write(const u8 *src, u32 len); /* 1 = len bytes written */
void host_save_set_path(const char *path);    /* NULL = no save file */
void host_save_set_fallback(const char *dir); /* the game dir, read only */
```

- `host_save_read` tries the save path, then `<game-dir>/CMOS`. With neither it
  returns 0 and `0x1AFE8`'s own failure path runs.
- `host_save_writable` stands for `0x1B084`'s `fopen`: the raw calls
  `0x2D4EC(1)`, `0x2D4EC(2)` and computes the checksum only after the open
  succeeds. With no save path it returns 0 and the port skips them, as the
  original did in the captures, whose mount refused the open.
- `host_save_write` writes the save path only.
- The fallback needs the game dir, which only `game_set_game_dir` knows. That
  function also hands the host the game dir as the fallback
  (`host_save_set_fallback`). The fallback tries the raw's name `cmos` and then
  `CMOS`, the name on disk, so a case-sensitive file system finds it too.
- `main.c` sets the path in the windowed run only: `--cmos PATH` when given,
  else `SDL_GetPrefPath(NULL, "primal-rage")` + `cmos`. SDL creates that
  directory, the string ends with a separator and is released with `SDL_free`;
  a NULL result means no save path (SDL 3.2.0 or later).
- The windowed run saves where the original does: the teardown, the hard-quit
  prompt and the service menu. A window close reaches the teardown (it sets the
  quit flag `0xA81A8`); the two `exit(1)` fault paths in `flow.c` do not save.
- `--check`, `run_tests`, `diffrun` and every env-gated driver never set a
  path. They read the game dir's `CMOS` and drop writes: the captures'
  condition. Nothing writes under `data/`.

Both are `PORT:` deviations and are named as such in `0x1AFE8`/`0x1B084`: the
original's path is the current directory's `cmos`.

### 4.3 Ground truth for a real save: the `save-reload` capture

Every existing capture ran over the zero image with the save blocked, so none
shows the original writing a save or booting from one. That is the normal case
from the port's second run on. One new capture closes it:

- **Boot 1** runs the original with the staging copy mounted writable (it is a
  temp dir already; `data/game` is still never written). The scenario walks the
  service menu, changes one option in the OPTIONS MENU and leaves it, which
  saves at `0x2CBAF`. The harness copies the `cmos` the original wrote to
  `data/k11-captures/save-reload/CMOS`.
- **Boot 2** starts the original again from that staging copy and records
  frames and `poll.log` as the other K11 captures do, with the image
  `0x100CE4..0x1014DB`, the config block `0x105DB0` and the high-score tables
  added to the polled fields.

`tools/k11_capture.py` gains the scenario and a writable-mount switch; its
zero-`CMOS` check stays on for every other scenario.

`make save-reload-oracle` (in `make verify`, skipping without the capture)
makes two claims, both exact:

1. *Write.* The port replays boot 1's keys with a scratch save path and writes
   a file byte-identical to the captured `CMOS`.
2. *Load.* The port boots with the captured `CMOS` as its save and, at the
   pinned frame of boot 2, holds the same image, config block and high-score
   tables as the capture.

Its claim is narrow: one changed option and whatever the audit counters
accumulated. A save holding an entered high score is not captured and stays a
named gap. A DOSBox-X capture needs the machine left alone while it runs, so
it is run once, on the user's go, not by a batch.

### 4.4 Order of work

1. **Task 1, serial, no port change.** Derive `0x2E990`, `0x2D638`, `0x2D4EC`,
   `0x2D498`, `0x2DE98`, `0x2DF8C` and `0x1AFE8` from the raw. Run the original
   in the harness over the zero image and over the `0xFF` image and record
   every byte `config_validate`'s chain leaves behind. Compare with what the
   port leaves today. Output: a derivation record
   (`docs/superpowers/plans/2026-10-13-reverse-s-derivations.md`) stating, per
   difference, the address and whether a pin can move.
2. **Three port batches in parallel**, one per group of §2, each in its own
   worktree, each with rows, mutants and its part of the record.
3. **Integration** on one branch: compose merge, the pin pre-checks, the full
   harness suite.
4. **One read-only audit** re-deriving the corrections from the bytes, before
   merge.
5. **The `save-reload` capture and its oracle** (§4.3), after the audit: the
   harness change, the capture on the user's go, the two ratchets.
6. **Gate and docs**: `make verify`; the 14 rows leave
   `tools/port_classification.txt`; the E2 table is regenerated; README,
   AGENTS.md and `docs/PROGRESS.md` get the new counts.

That is four agents: three port batches and one audit.

### 4.5 Tests

- A differential row and mutants for each of the 15 functions (the checklist is
  record `2026-10-01-reverse-e3-derivations.md` §E3.10).
- The row of `0x2D6F8` is partial today because the port omits the storage
  calls. It becomes a full row where nothing is omitted any more; any call that
  stays omitted stays named. The row of `0x2CADC` is partial for three display
  calls, not for storage, and does not change.
- Unit tests in `port/tests/test_game.c`, each mutation-proved:
  - a non-default image written by `0x1B084` and reloaded by `0x1AFE8` is
    byte-identical, and `config_validate` over it takes the stored-image arm
    and restores the same config fields, high scores and counters;
  - a corrupt checksum and a missing file both give the `0xFF` image;
  - a write past `0x7F8` is refused (`0x2D4EC`'s bound).
- No test writes a save file outside the scratch directory it creates.

## 5. Risks

- **A pin moves.** The zero-image read returns 1 where the port assumed -1. If
  that changes state a driver reads, the raw wins: the correction is recorded
  and the pin re-measured against the capture. A pin that moves away from the
  capture is a port bug, not a re-pin.
- **The audit counters become real state.** Twenty `0x2DAE4` sites and three
  `0x2E934` sites start writing counters and the dirty bits of `0x105DD8`, and
  the service menu's audit screens read them. The K11 and gameplay captures ran
  the original with those counters live, so a screen or traced field that moves
  must move towards the capture.
- **`0x2D4EC` changes 18 sites at once**, most of them in functions that
  already have a differential row with `0x2D4EC` stubbed. Those rows keep their
  stub; the new `0x2D4EC` row is what verifies the body.
- **`fn_resolve` miss sets.** A driver that now reaches a new code pointer fails
  its pinned miss set; register the target or pin the miss with its evidence.
- **Loading a real save.** Until the `save-reload` capture exists, the write
  and load paths have differential rows and unit tests but no recording of the
  original doing either. If the capture cannot be made, this is the track's
  main named gap and the docs say so.
- **The `0xFF` image.** No capture ran with `CMOS` absent, so that path has
  differential rows but no oracle. It is a named gap.
- **Shared file.** Two batches edit `config.c`; the integration uses the compose
  merge from the merge base, as track D did.

## 6. Counts after the track

`tools/port_progress.py` is expected to print `785 1203 65` and `745 745 100`
(14 more ported, 14 fewer deferred). The differential counter gains up to 15
rows: the 14, and `0x2D4EC`, which is an unrowed named gap today. The exact line
is measured at the gate, not predicted here.
