# Automated GitHub releases for Windows, Linux and macOS — design

Status: draft, 2026-11-05 (builder-checked; owner answers folded in).

## Intent

The port is fully playable. People who **own Primal Rage (DOS, 1995)** should be
able to download a build for their machine, point it at their own game folder,
and play — no compiler, no terminal, no DOSBox. Pushing a version tag should
produce that build for all three desktop platforms with no manual steps.

The player experience, in the owner's words: *"they point it at their game
folder and it just runs"*, through *"a folder picker that remembers it"*.

## Goals

1. **A tag push makes a release.** Pushing a version tag `vMAJOR.MINOR.PATCH`
   (e.g. `v1.0.0`; a suffix such as `v1.1.0-rc1` makes a GitHub *pre-release*)
   runs a GitHub Actions workflow that builds, tests, packages and publishes a
   GitHub Release for that tag. The existing milestone tag `v1.0-final` must
   **not** match the trigger. Nothing is published unless every platform
   builds and passes its checks.
2. **Three downloads plus checksums** on each release:
   | Asset | Contents |
   |---|---|
   | `primal-rage-port-<ver>-windows-x64.zip` | `prageport.exe`, `SDL3.dll` (and any runtime DLL the toolchain needs), `README.txt`, licence notices |
   | `primal-rage-port-<ver>-linux-x64.tar.gz` | the binary (RPATH `$ORIGIN`, so no launcher script is needed), `libSDL3.so.0`, `README.txt`, licence notices |
   | `primal-rage-port-<ver>-macos-universal.zip` | `Primal Rage.app` (arm64 + x86_64, SDL3 embedded in the bundle), `README.txt`, licence notices |
   | `SHA256SUMS` | sha256 of the three archives |
3. **First-run folder picker** (windowed run only):
   - Game folder precedence: `--game-dir DIR` → the saved folder → the dev
     default `data/game/C` *if it holds the game files* → the folder picker.
   - The picker is SDL3's `SDL_ShowOpenFolderDialog` (available since SDL 3.2.0;
     asynchronous, callback may run on another thread, needs the event loop
     pumped — on Linux it goes through XDG portals/DBus).
   - **What players have** (owner): the folder the DOS installer created (the
     shape of the repo's `data/game/C`) and/or the original CD. The port reads
     only the installed folder's files; nothing is read from a CD drive path.
   - A folder is valid when it holds **every file the game loads at boot**
     (`PRAGE.EXE`, `INDEX`, every file `INDEX` lists, the string table(s), the
     two movies, `FAT.OPL`, the sound driver files — the builder enumerates the
     exact set from `game_init` and `res_load_index`; note `res_load_index`
     counts missing files instead of failing, so the check must not rely on it)
     **and** `PRAGE.EXE` is the supported build.
   - **One supported `PRAGE.EXE`** (owner: there is only one version). The check
     compares its size and CRC-32 (`SDL_crc32`, SDL ≥ 3.2.0 — a version
     fingerprint, not a security check, so no sha256 code is added) against constants taken from the owner's copy
     at implementation time (the builder computes them from `data/game/C/PRAGE.EXE`
     in the main checkout and records them in the derivation record/PROGRESS);
     a mismatch shows "This PRAGE.EXE is not the supported version" instead of
     loading it.
   - **File-name case (my call, first release):** the check matches
     `PRAGE.EXE` and `INDEX` with the exact case `game_init` opens them
     (`flow.c:7045`, `:7058`), so it never accepts a folder the game cannot
     load; other files are checked the way the game itself finds them
     (`res_open_name`'s case-insensitive scan, the string tables' upper-then-lower
     try). A folder whose `PRAGE.EXE`/`INDEX` exist only in another case gets a
     message saying to rename them to upper case. Full case-insensitive startup
     is a later change (it touches `game_init`, on the oracle-measured path).
   - Invalid folder → a plain message box naming what is wrong (missing files,
     wrong-case names, unsupported `PRAGE.EXE`) and the picker opens again.
     Cancel → exit quietly with status 0.
   - A valid picked folder is saved to a text file in the per-user directory the
     save file already uses (`SDL_GetPrefPath(NULL, "primal-rage")`, see
     `host_save_set_default_path` in `port/src/host.c`), and used on later
     launches. If the saved folder stops being valid, the picker opens again.
4. **Visible fatal errors.** A launched-from-Finder/Explorer program has no
   terminal, so the windowed run's fatal init errors (`game_fatal` in
   `port/src/game/flow.c`) also show an SDL message box. Headless runs keep
   writing to stderr only.
5. **Windows builds.** The code has never compiled on Windows. Make it build
   there without changing behaviour on macOS/Linux.
6. **Release notes** = GitHub's auto-generated notes plus a fixed "How to play"
   section: you need your own copy of the game; unzip; run; pick the folder
   your DOS installer created (e.g. `C:\PRAGE`) — if you only have the CD, pick
   the CD folder that holds `PRAGE.EXE` and the other game files, or, if the
   check rejects it, install the game once under DOSBox and pick that folder
   (my call: the CD layout is not verified; the builder states in the notes
   only what the check actually accepts);
   how to get past the unsigned-app warnings (macOS: right-click → Open, or
   `xattr -dr com.apple.quarantine "Primal Rage.app"`; Windows SmartScreen:
   More info → Run anyway); where saves live.
7. **Licensing.** The port's own code is **MIT** (owner). Add a top-level
   `LICENSE` (MIT, copyright Felipe dos Santos). Every archive ships the port's
   MIT notice, the vendored Opal OPL emulator's MIT notice
   (`port/src/platform/audio/LICENSE.opal.txt`) and SDL3's zlib licence. The
   release notes state that the game itself is not included and remains its
   rights holders' property.
8. **The version string comes from the tag.** `main.c` prints a hard-coded
   `prageport 0.0.1`; the build takes the version from CMake (e.g.
   `-DPRAGE_VERSION=1.0.0`, default `0.0.0-dev`).

## Non-goals

- Code signing or notarization (macOS or Windows) — unsigned is accepted for now.
- Installers, auto-update, Homebrew/Flathub/winget packages.
- 32-bit anything. (ARM Linux and ARM Windows were non-goals here; the owner
  added both on 2026-11-05: `linux-arm64` on `ubuntu-22.04-arm`,
  `windows-arm64` on `windows-11-arm`.)
- Running `make verify` or any oracle in CI (they need the game's own bytes,
  which never go to CI).
- Building `run_tests` or `diffrun` on Windows (see Constraints).

## Anti-goals (a failure even if it "works")

- **Any game file in a release, a CI cache, an artifact or the repo.** The
  workflow never downloads, caches or uploads `data/`, `PRAGE.EXE`, `INDEX`,
  `CMOS`, captures or `port/tests/ghidra_data.bin`.
- **Any behaviour change on the paths the oracles measure.** `--check`, every
  env-gated `PR_*` driver, `run_tests` and `diffrun` must never open a picker,
  read the saved-folder file or show a message box. `make verify` stays green
  with every pin unchanged.
- Weakening a local gate to make CI pass.
- Breaking `make run` / `make check`, which pass `--game-dir` explicitly.

## Constraints

- **Porting rules (AGENTS.md).** SDL and file/asset I/O live only in
  `port/src/host.c` and `port/src/main.c`; the picker, the saved-folder file and
  message boxes go there. `game_fatal` (flow.c) may call a host hook, not SDL.
  Comment styles in `port/src`: `/* PORT: ... */` for deliberate deviations,
  no others. `symbols.h` is generated — never hand-edit.
- **POSIX-only code found by recon** (2026-11-05), to fix for Windows:
  - `port/src/platform/res.c` — `opendir`/`readdir`/`strncasecmp` (dirent).
  - `mkdir(path, mode)` two-argument form in `port/src/main.c:189`,
    `port/src/mem.c:179`, `port/src/game/flow.c:4528-4529`.
  - `port/CMakeLists.txt` passes GCC/Clang flags unconditionally
    (`-Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing`) and defaults to
    a Debug build.
  - The test suite uses `fork`, `sys/wait.h`, `unistd.h`, `popen`, `unsetenv`
    (`port/tests/test_platform.c`, `test_game.c`, `test_video.c`,
    `test_audio.c`, `diff_runner.c`). These stay POSIX; on Windows only the
    `prageport` target is built.
- **The flat `mem[]` is 64 MB** (`MEM_SIZE 0x4000000`); fine on all targets.
- **SDL3 is not packaged** by Ubuntu LTS or the Windows runners. Build it from
  a **pinned** SDL3 release tag (≥ 3.2.0) on each runner, or use the official
  SDL3 release archives for that tag; record the version in the workflow.
- **Linux portability**: build on the oldest supported GitHub Ubuntu runner
  (currently `ubuntu-22.04`) so the glibc floor is low; SDL3 must be built with
  its X11, Wayland, PulseAudio/PipeWire/ALSA and DBus backends dynamically
  loaded so the binary runs on desktops that lack some of them.
- **macOS**: a universal (`arm64;x86_64`) build; set a deployment target no
  higher than SDL3's own minimum. The `.app` needs an `Info.plist` and SDL3
  embedded with a working `@rpath`. Launched from Finder the working directory
  is `/`, so the dev default `data/game/C` will not resolve — that is expected
  and falls through to the picker.

## Decided

- Trigger: tag push matching `vMAJOR.MINOR.PATCH[-suffix]`; suffix → pre-release;
  `v1.0-final` excluded. (My call after the builder check; owner agreed to tag
  releases.) The first release's number is the owner's to pick when tagging.
- Unsigned binaries with a workaround in the release notes. (Owner.)
- Folder picker on first launch, remembered. (Owner.)
- Asset set and packaging forms in Goal 2. (My call; owner agreed.)
- CI checks before publish (my call, corrected by measurement 2026-11-05):
  `run_tests` **cannot run in CI** — with no `data/game/C` it fails
  `test_platform.c:887` ("PRAGE.EXE loads") and then segfaults in `test_res`
  (exit 139, measured on this checkout, which has no `data/`). So on all three
  platforms CI builds `prageport` (Release) and smoke-runs it: `--help` exits 0,
  and `--check 1 --game-dir <empty dir>` exits 1 with
  `Primal Rage: fatal init error` on stderr (the binary starts, links SDL3 and
  reaches the fatal path headless, with no message box). Game behaviour is
  gated by the owner's local `make verify` before tagging, not by CI.
- Windows toolchain: MinGW-w64 (MSYS2 UCRT64 or equivalent), which accepts the
  existing GCC flags and provides `strncasecmp`; `dirent` and `mkdir` get
  small portable replacements. (My call — MSVC would need more source churn.)

## Left to the builder

- Workflow structure (one job per platform + a publish job is the expected
  shape), action versions, caching of the SDL3 build.
- Exactly how the Windows `mkdir`/`dirent` fix is shaped (an `#ifdef _WIN32`
  shim in the files that need it, or SDL's `SDL_EnumerateDirectory`/
  `SDL_CreateDirectory` in host-side code — note `res.c` is not host code, so
  its scan must stay plain C or go through a host hook).
- Name and format of the saved-folder file.
- How the picker waits for its async callback (a small loop pumping
  `SDL_PumpEvents` until the callback sets a flag is sufficient).
- The `.app` icon (none is fine), the Linux launcher's exact form.
- Windows console vs GUI subsystem, behaviour when the Linux portal dialog is
  unavailable (fall back to a message box that tells the player to use
  `--game-dir`), long/non-ASCII paths.
- Whether to also produce a manual `workflow_dispatch` dry run that builds
  without publishing (recommended for testing the workflow before the first tag).

## Done when

- Pushing `v0.1.0` (or a test tag) produces a GitHub Release with the four
  assets, and each archive, unpacked on a clean machine of its platform with a
  legitimate game folder, launches, shows the picker, plays, and on the second
  launch goes straight to the game.
- A folder without the game files shows the message and re-opens the picker.
- `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify` is green with
  no pin changed, and `python3 tools/port_progress.py` is unchanged.
- README gains a short "Download and play" section linking to Releases;
  `docs/PROGRESS.md` gets a paragraph.
