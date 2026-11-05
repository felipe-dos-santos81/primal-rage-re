# Automated GitHub Releases Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A `vX.Y.Z` tag push publishes a GitHub Release with Windows x64, Linux x64 and macOS universal builds of `prageport` that, on first launch, ask for the player's Primal Rage folder, check it, remember it, and play.

**Architecture:** Three source changes, then CI. (1) Make the tree build with MinGW-w64 and take its version from CMake. (2) Add a folder check: `res_dir_check` (plain C, in `res.c`) checks that the files are present, and `host_file_crc32` (SDL, in `host.c`) fingerprints `PRAGE.EXE`. (3) Add the windowed-run flow in `main.c`: resolve the folder from the command line, then the saved folder, then the dev default, then the picker. Fatal errors show a message box when a window is open. (4) Add one GitHub Actions workflow that builds SDL3 from a pinned tag, builds and smoke-tests the port per platform, packages it, and publishes it.

**Tech Stack:** C11, CMake ≥ 3.20, SDL 3.4.20 (`SDL_ShowOpenFolderDialog`, `SDL_ShowSimpleMessageBox`, `SDL_crc32`, `SDL_GetPrefPath`: all ≥ 3.2.0), MinGW-w64 (MSYS2 UCRT64), GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-11-05-github-release-design.md`

## Global Constraints

- The release, CI caches, artifacts and the repo never contain a game file (`data/`, `PRAGE.EXE`, `INDEX`, `CMOS`, captures, `port/tests/ghidra_data.bin`).
- `--check`, every `PR_*` driver, `run_tests` and `diffrun` never open a picker, read the saved-folder file or show a message box. Their stdout/stderr stay byte-identical, except that the version string comes from `PRAGE_VERSION`. Its default must stay `0.0.1` so that existing output does not move.
- SDL and file I/O go only in `port/src/host.c` and `port/src/main.c`. `res.c` already does plain-C `fopen`/`opendir` and may keep doing so. It must not use SDL.
- Comments in `port/src` are either plain prose or `/* PORT: ... */` for a deliberate deviation. New functions have no `/* 0xADDR` header: they are host code, not ports. `symbols.h` is untouched, and `python3 tools/port_progress.py` must still print `798 1203 66` and `759 759 100`.
- Register each test once in `TEST_CASES` (`port/tests/test.h`). Use only `CHECK`/`CHECK_EQ_INT`. Seed sentinels that differ from the expected results. Use mutation-proof assertions.
- The local gate is `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify`. It needs `data/game/C` and `port/tests/ghidra_data.bin`. **This checkout has no `data/` (measured 2026-11-05).** Run the gate where the data exists. If it is absent, stop and ask the owner to run it. Never skip it silently.
- `run_tests` cannot run without game data: it fails `test_platform.c:887` and then segfaults in `test_res` (exit 139, measured). CI therefore never runs it.
- SDL3 is pinned to tag `release-3.4.20` (the owner's local version) on every CI runner.
- Release tag pattern: `v[0-9]+.[0-9]+.[0-9]+` with an optional `-suffix`. A suffix makes a pre-release. `v1.0-final` must not match.
- The port's licence is MIT, copyright 2026 Felipe dos Santos. Every archive carries `LICENSE`, `port/src/platform/audio/LICENSE.opal.txt` and SDL3's `LICENSE.txt`.
- Commit style: `<area>: <what changed>`. Stage named files only, never `git add -A`.

## Review Focus

1. **Paths with spaces or non-ASCII characters** (`C:\Games\Primal Rage\`, `~/Jeux/Primal Rage é`). The game should load. On Windows this needs the UTF-8 active-code-page manifest (Task 1). The temp folder in `test_res_dir_check` has a space and `é` in its name (Task 2).
2. **A picked path with a trailing separator** (`/Volumes/RAGE/`, `D:\`). The check and the game should both accept it. `test_res_dir_check` passes a path with a trailing `/` (Task 2).
3. **A saved-folder file edited by hand** (CRLF line ending, trailing spaces) or pointing at a folder that has since disappeared (ejected CD). The first should be trimmed; the second should reopen the picker. `test_host_game_dir_file` covers the trimming (Task 3), and Task 3's manual check covers the stale case.
4. **Lower-case `prage.exe`/`index`** on a case-sensitive filesystem. The player should get a message saying to rename them to upper case, not a crash. `test_res_dir_check` covers the wrong-case result (Task 2).
5. **Linux without a working dialog backend** (no XDG portal and no zenity). The player should get a message saying to run with `--game-dir`, and the program exits 1. This is covered by Task 3's manual check (the callback's `NULL` error branch).

---

### Task 1: Windows-buildable tree, version from CMake

**Files:**
- Create: `port/src/compat.h`
- Create: `port/win/prageport.manifest`, `port/win/prageport.rc`
- Modify: `port/src/main.c:4,16,189,336` and `run_windowed`'s `printf` (two version strings, one `mkdir`, `#include <SDL3/SDL_main.h>`)
- Modify: `port/src/mem.c:179`, `port/src/game/flow.c:4528-4529` (`mkdir`)
- Modify: `port/CMakeLists.txt` (flags guarded, `PRAGE_VERSION`, Windows-only `prageport`)

**Interfaces:**
- Produces: `pr_mkdir(path, mode)` (in `compat.h`; `_mkdir(path)` on `_WIN32`, `mkdir(path, mode)` elsewhere) and the CMake cache variable `PRAGE_VERSION` (default `0.0.1`), seen in C as the string macro `PRAGE_VERSION`.

- [ ] **Step 1: Prove the Windows build fails today.** Download the SDL3 MinGW development archive `SDL3-devel-3.4.20-mingw.tar.gz` from the `release-3.4.20` GitHub release and unpack it into the scratchpad. Then run `cmake -S port -B build-win -DCMAKE_TOOLCHAIN_FILE=<scratch>/mingw-toolchain.cmake -DSDL3_DIR=<scratch>/SDL3-3.4.20/x86_64-w64-mingw32/lib/cmake/SDL3 && cmake --build build-win --target prageport`. The toolchain file sets `CMAKE_SYSTEM_NAME Windows` and `CMAKE_C_COMPILER x86_64-w64-mingw32-gcc`; that compiler is installed at `/opt/homebrew/bin`. Expected: FAIL at `mkdir` (too many arguments) or at configure time (`run_tests`/`diffrun` POSIX headers).
- [ ] **Step 2: Implement.**
  - Add `compat.h` with `pr_mkdir` and switch the four `mkdir` sites to it.
  - In CMake, wrap `add_compile_options(...)` in `if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")`.
  - On `WIN32`, build only `prageport` (skip `run_tests`, `diffrun`, `enable_testing`). Set `WIN32_EXECUTABLE TRUE`, link `-static-libgcc`, and add `port/win/prageport.rc`. The `.rc` embeds the manifest: `<activeCodePage>UTF-8</activeCodePage>` plus Windows 10 `supportedOS`.
  - Add `set(PRAGE_VERSION "0.0.1" CACHE STRING ...)` and `target_compile_definitions(prageport PRIVATE PRAGE_VERSION="${PRAGE_VERSION}")`. Replace both `"prageport 0.0.1 ..."` literals with `"prageport " PRAGE_VERSION " ..."`.
  - Add `#include <SDL3/SDL_main.h>` once, in `main.c` only.
- [ ] **Step 3: Verify Windows.** Rerun the Step 1 commands. Expected: `build-win/prageport.exe` exists, and `x86_64-w64-mingw32-objdump -p build-win/prageport.exe | grep "DLL Name"` lists only `SDL3.dll` and Windows system DLLs (no `libgcc_s*`/`libwinpthread*`). Fix any new `-Wall` warning that MinGW reports about `long`/pointer width, since Windows is LLP64.
- [ ] **Step 4: Verify nothing moved on macOS.** Run `cmake -S port -B build && cmake --build build`. Then `./build/prageport --help` exits 0 and its usage line is unchanged. `./build/prageport --check 1 --game-dir /tmp/claude-501/empty` exits 1 and prints `Primal Rage: fatal init error: PRAGE.EXE image load failed` followed by `prageport 0.0.1 game-dir=... --check 1 (headless)`. Then run the local gate (Global Constraints). It must be green with no pin changed.
- [ ] **Step 5: Commit.** `git add port/src/compat.h port/win/prageport.manifest port/win/prageport.rc port/src/main.c port/src/mem.c port/src/game/flow.c port/CMakeLists.txt && git commit -m "build: MinGW-w64 Windows build of prageport; version from CMake"`

### Task 2: Game-folder check and `PRAGE.EXE` fingerprint

**Files:**
- Modify: `port/src/platform/res.c`, `port/src/platform/res.h` (`res_dir_check`)
- Modify: `port/src/host.c`, `port/src/host.h` (`host_file_crc32`)
- Modify: `port/src/main.c` (the two fingerprint constants, `#define`s next to where Task 3 uses them)
- Test: `port/tests/test_platform.c` (`test_res_dir_check`, `test_host_file_crc32`), registered in `port/tests/test.h`

**Interfaces:**
- Produces, in `res.h`:
  ```c
  #define RES_DIR_OK          0
  #define RES_DIR_NO_EXE      1  /* <dir>/PRAGE.EXE does not open in any case */
  #define RES_DIR_NO_INDEX    2  /* <dir>/INDEX does not open in any case, or its size is 0 or not a multiple of 0x14 */
  #define RES_DIR_WRONG_CASE  3  /* PRAGE.EXE or INDEX exists only in another case; `name` = the on-disk name */
  #define RES_DIR_MISSING     4  /* a required file is absent; `name` = the first missing name */
  int res_dir_check(const char *game_dir, char *name, size_t n);
  ```
  Required files, in this order: `PRAGE.EXE`, then `INDEX` (both exact case, opened as `game_init` opens them, `flow.c:7045`/`:7058`). Next, each `INDEX` record's 12-byte name, matched the way `res_open_name` matches it. Then `FAT.OPL`, `TWI5.SMK` and `TWG.SMK` (`res_open_name`). Last, `english.txt`, matched as `game_string_table_load` opens it: upper case, then lower case. `name` is always NUL-terminated; it is `""` for `OK`/`NO_EXE`/`NO_INDEX`.
- Produces, in `host.h`: `int host_file_crc32(const char *path, u32 *size, u32 *crc);`. It returns 1 and sets both outputs, or returns 0 and leaves both untouched if the file can't be opened or read. It uses `SDL_crc32` (initial 0).
- Produces, in `main.c`: `#define PRAGE_EXE_SIZE <n>u` and `#define PRAGE_EXE_CRC32 0x<...>u`.

- [ ] **Step 1: Write `test_res_dir_check`.** Work in a temp folder made with `mkdtemp("<base>/pr dir é XXXXXX")`, where `<base>` is `getenv("PR_CASE_TMP")` or `/tmp`. **macOS's default APFS is case-insensitive**, so the two case rows below (`English.txt`, `prage.exe`) run only when the temp folder is case-sensitive (probe: create `probe`, check that `PROBE` does not open); otherwise they print a `SKIP` line. For a macOS run of those rows: `hdiutil create -size 20m -fs "Case-sensitive APFS" -volname prcase /tmp/prcase.dmg && hdiutil attach /tmp/prcase.dmg`, then run with `PR_CASE_TMP=/Volumes/prcase`. Write `INDEX` as 2 records of 0x14 bytes, named `a.gra` and `b.gra`, with all other bytes 0. Create `PRAGE.EXE`, `A.GRA` (upper case, so the case-insensitive match is exercised), `b.gra`, `FAT.OPL`, `TWI5.SMK`, `TWG.SMK` and `ENGLISH.TXT`. Seed `name` with `"XXXX"` before each call. Assertions, each followed by restoring the folder:
  - All files present → `RES_DIR_OK`, and `name` equals `""`.
  - The same call with the path plus a trailing `/` → `RES_DIR_OK`.
  - Delete `b.gra` → `RES_DIR_MISSING`, `name == "b.gra"`.
  - Delete `TWG.SMK` → `RES_DIR_MISSING`, `"TWG.SMK"`.
  - Rename `ENGLISH.TXT` to `English.txt` → `RES_DIR_MISSING`, `"english.txt"` (the game's upper-then-lower try would not find it).
  - Rename `PRAGE.EXE` to `prage.exe` → `RES_DIR_WRONG_CASE`, `"prage.exe"`.
  - Delete `PRAGE.EXE` → `RES_DIR_NO_EXE`.
  - Truncate `INDEX` to 0x13 bytes → `RES_DIR_NO_INDEX`.
  - Delete `INDEX` → `RES_DIR_NO_INDEX`.
  - If `PR_GAME_DIR` (default `data/game/C`) holds `INDEX`, then `res_dir_check(dir)` returns `RES_DIR_OK`, and `host_file_crc32(dir/PRAGE.EXE)` returns size `PRAGE_EXE_SIZE` and crc `PRAGE_EXE_CRC32`. Copy the two constants into the test as local `#define`s with the same values, because `main.c` is not linked into `run_tests`. This assertion guards the fingerprint against the real file. Otherwise print a `SKIP` line in the existing style. Under `PR_ORACLE_REQUIRED=1`, a missing file is a `CHECK` failure.
- [ ] **Step 2: Write `test_host_file_crc32`.** Write `"123456789"` to a temp file → returns 1, size 9, crc `0xCBF43926`. Then try a missing path → returns 0, and both outputs keep their `0xEEEEEEEE` seeds.
- [ ] **Step 3: Run and see both fail.** Run `cmake --build build && ./build/run_tests 2>&1 | grep -E "res_dir_check|host_file_crc32"`. Expected: a compile failure (undefined functions).
- [ ] **Step 4: Implement `res_dir_check` and `host_file_crc32`.** Reuse `res_open_name` for the case-insensitive lookups. Find the wrong-case name with the same `opendir` scan. Read the `INDEX` records with plain `fread`; never call `res_load_index`, which allocates from the `mem[]` bump heap.
- [ ] **Step 5: Measure the fingerprint.** On the owner's copy, run `python3 -c "import zlib,sys;b=open(sys.argv[1],'rb').read();print(len(b),hex(zlib.crc32(b)))" data/game/C/PRAGE.EXE`. Put the two values into `main.c`'s defines and the test's copies, and record them in `docs/PROGRESS.md`'s Task 4 paragraph. If `data/game/C` is absent here, ask the owner for the output; never invent the values.
- [ ] **Step 6: Run the tests where the data exists.** Run `PR_ORACLE_REQUIRED=1 ./build/run_tests`. Expected: both new tests pass. Run again with `PR_CASE_TMP=/Volumes/prcase` (no case `SKIP` lines). Mutation proof (under `PR_CASE_TMP`): flip `RES_DIR_WRONG_CASE` to `RES_DIR_MISSING` in the implementation, and change the CRC initial value to 1. Each mutation must fail a `CHECK`. Revert both, then run the local gate (green, no pin changed).
- [ ] **Step 7: Commit.** `git add port/src/platform/res.c port/src/platform/res.h port/src/host.c port/src/host.h port/src/main.c port/tests/test_platform.c port/tests/test.h && git commit -m "res: game-folder check and PRAGE.EXE fingerprint"`

### Task 3: First-run folder picker, saved folder, visible fatal errors

**Files:**
- Modify: `port/src/host.c`, `port/src/host.h`
- Modify: `port/src/main.c` (`main`, `run_windowed`, new static `resolve_game_dir`)
- Modify: `port/src/game/flow.c:139-143` (`game_fatal` calls `host_fatal_message`)
- Test: `port/tests/test_platform.c` (`test_host_game_dir_file`), registered in `port/tests/test.h`

**Interfaces:**
- Consumes: `res_dir_check`, `host_file_crc32`, `PRAGE_EXE_SIZE`/`PRAGE_EXE_CRC32` (Task 2).
- Produces, in `host.h`:
  - `int host_pick_folder(char *out, size_t n);` returns 1 when a folder is picked (UTF-8 path in `out`), 0 when the dialog is cancelled, and -1 on a dialog error. It needs `host_init`. It calls `SDL_ShowOpenFolderDialog(cb, &state, window, NULL, false)` and then loops `SDL_PumpEvents(); SDL_Delay(10);` until the callback stores its result in an `SDL_AtomicInt`. The callback may run on another thread, so it copies the path before it returns.
  - `void host_message(const char *title, const char *text);` shows `SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, ...)` parented to the window, and writes `text` to stderr if that fails.
  - `void host_fatal_message(const char *text);` calls `host_message("Primal Rage", text)` only when `host_has_window()`. It does nothing headless.
  - `int host_pref_file(const char *name, char *out, size_t n);` builds `SDL_GetPrefPath(NULL, "primal-rage") + name`. It returns 0 on failure.
  - `int host_game_dir_load(const char *file, char *out, size_t n);` reads the file and strips trailing `\r`, `\n`, space and tab. It returns 1 when the result is non-empty, otherwise 0 with `out[0] = '\0'`.
  - `int host_game_dir_save(const char *file, const char *dir);` writes `dir` followed by `\n`. It returns 1 on success.
- The saved-folder file is `game-dir.txt`, next to the `cmos` save.

- [ ] **Step 1: Write `test_host_game_dir_file`.** In a temp folder:
  - Save `"/a b/é"` → returns 1. Loading it into a buffer seeded with `"ZZZZ"` → returns 1, `"/a b/é"`.
  - A hand-written `"C:\\PRAGE \r\n"` loads as `"C:\\PRAGE"`.
  - A file holding only `"\r\n"` → returns 0, `out == ""`.
  - A missing file → returns 0, `out == ""`.
  - Saving into a folder that doesn't exist → returns 0.
- [ ] **Step 2: Run and see it fail** (undefined functions), as in Task 2 Step 3.
- [ ] **Step 3: Implement the host functions.** Build `host_game_dir_load`/`_save` on the existing `host_read_file`/`host_write_file`.
- [ ] **Step 4: Implement `resolve_game_dir` in `main.c`** and call it from `run_windowed` after `host_init` and before `game_set_game_dir`:
  - In `main`, `game_dir` starts as `NULL`. The `--check` branch uses `game_dir ? game_dir : "data/game/C"` (unchanged behaviour). `run_windowed` gets the possibly-`NULL` value.
  - The order is: `--game-dir` (used as-is, unchecked, so dev/oracle runs behave as before) → the saved folder, if `game_dir_ok` passes → `"data/game/C"`, if `game_dir_ok` passes → the picker loop.
  - `game_dir_ok(dir, msg, n)` is `res_dir_check` plus the fingerprint compare. It writes the player-facing text:
    - `NO_EXE`/`NO_INDEX`/`MISSING`: `"<folder> does not contain the Primal Rage game files (<name or PRAGE.EXE/INDEX> is missing). Choose the folder where Primal Rage is installed; if you only have the CD, copy the CD's RAGE.S16 folder contents into it."`
    - `WRONG_CASE`: `"Rename <name> to upper case (PRAGE.EXE, INDEX) and choose the folder again."`
    - Fingerprint mismatch: `"This PRAGE.EXE is not the supported version of Primal Rage."`
  - Picker loop: picked and valid → `host_game_dir_save(pref game-dir.txt)` and use it. Picked and invalid → `host_message`, then loop. Cancelled → `host_shutdown()`, return 0. Error → `host_message("Primal Rage", "Could not open the folder chooser. Run prageport --game-dir <folder> instead.")`, return 1.
  - `game_fatal` (flow.c): add `host_fatal_message(what)` before `exit(1)`, with a `/* PORT: */` line saying the windowed run has no terminal.
- [ ] **Step 5: Run the tests.** `./build/run_tests` (with data) passes. Mutation: drop the `\r` from the strip set → `test_host_game_dir_file` fails. Revert. Then run the local gate (green, no pin changed, since `--check` and the drivers never reach this code).
- [ ] **Step 6: Manual check on macOS (owner's machine).** Delete `~/Library/Application Support/primal-rage/game-dir.txt`, then run `./build/prageport` from a folder that has no `data/`.
  - The picker opens. Choose an empty folder → the message appears and the picker reopens.
  - Choose `data/game/C` → the game runs. Quit and relaunch → it goes straight to the game.
  - Edit `game-dir.txt` to a missing path → the picker opens.
  - Press Cancel → the program exits 0 with no message.
  Do the same on a Linux box if one is available. If the dialog backend is missing there, expect the "Could not open the folder chooser" message and exit code 1.
- [ ] **Step 7: Commit.** `git add port/src/host.c port/src/host.h port/src/main.c port/src/game/flow.c port/tests/test_platform.c port/tests/test.h && git commit -m "host: first-run game-folder picker, remembered folder, fatal message box"`

### Task 4: Release workflow, packaging, licence and docs

**Files:**
- Create: `.github/workflows/release.yml`
- Create: `tools/release/README.txt` (the player-facing how-to-play; copied into every archive and used as the release body's lower half)
- Create: `tools/release/Info.plist` (`CFBundleExecutable prageport`, `CFBundleIdentifier io.github.felipe-dos-santos81.primal-rage-port`, `CFBundleName Primal Rage`, `CFBundleShortVersionString` replaced at package time, `LSMinimumSystemVersion 11.0`, `NSHighResolutionCapable true`)
- Create: `LICENSE` (MIT, `Copyright (c) 2026 Felipe dos Santos`)
- Modify: `README.md` (a "Download and play" section linking to Releases, three sentences), `docs/PROGRESS.md` (one paragraph: the release pipeline, the fingerprint values, the measured fact that `run_tests` needs game data), `AGENTS.md` (one line under Workflow: how to cut a release, and that a release needs a green local `make verify` first)

**Interfaces:**
- Consumes: Task 1's `PRAGE_VERSION`, Windows CMake path, `.rc` manifest; Task 3's behaviour, described in `README.txt`.

**Workflow shape** (`release.yml`):
- Triggers:
  - `push: tags: ['v[0-9]+.[0-9]+.[0-9]+', 'v[0-9]+.[0-9]+.[0-9]+-*']`
  - `workflow_dispatch` (a dry run: build and package, upload workflow artifacts, never publish).
- Environment: `SDL_TAG: release-3.4.20`. The version is `${GITHUB_REF_NAME#v}` on a tag and `0.0.0-dev` on dispatch.
- `permissions: contents: write` on the publish job only.
- Job `linux` (`ubuntu-22.04`):
  - Install SDL3's Ubuntu 22.04 build dependencies (the list in SDL's `docs/README-linux.md`, including X11, Wayland, libdecor, PulseAudio, PipeWire, ALSA, DBus and udev dev packages).
  - Build SDL3 at `SDL_TAG` as a shared library with its defaults, which load backends dynamically, and install it to a prefix.
  - Build the port: `-DCMAKE_BUILD_TYPE=Release -DPRAGE_VERSION=$VER -DCMAKE_BUILD_RPATH='$ORIGIN' -DCMAKE_PREFIX_PATH=<prefix>`, target `prageport`.
  - Smoke: `--help` → 0. `--check 1 --game-dir "$RUNNER_TEMP/empty"` → exit 1 and stderr contains `Primal Rage: fatal init error`.
  - Package `primal-rage-port-$VER-linux-x64.tar.gz` (a top-level folder holding `prageport`, `libSDL3.so.0`, `README.txt`, `LICENSE`, `LICENSE.opal.txt`, `LICENSE.SDL3.txt`).
  - Check: `readelf -d prageport | grep -q 'RUNPATH.*\$ORIGIN'`.
- Job `macos` (`macos-14`):
  - SDL3 and the port are built with `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0`. The port also gets `-DCMAKE_BUILD_RPATH=@executable_path/../Frameworks`.
  - Smoke as Linux.
  - Assemble `Primal Rage.app/Contents/{MacOS/prageport,Frameworks/libSDL3.0.dylib,Info.plist}`.
  - Check: `lipo -archs` on both binaries prints `x86_64 arm64`, and `otool -L` shows `@rpath/libSDL3.0.dylib`.
  - Ad-hoc sign it (`codesign --force --deep -s - "Primal Rage.app"`; Apple Silicon refuses to run unsigned arm64 code at all, and an ad-hoc signature is free and isn't Developer ID signing).
  - Zip with `ditto -c -k --keepParent` into `primal-rage-port-$VER-macos-universal.zip`, which also holds `README.txt` and the licences.
- Job `windows` (`windows-2022`, `msys2/setup-msys2` with `UCRT64` and the `mingw-w64-ucrt-x86_64-{gcc,cmake,ninja}` packages):
  - Build SDL3 at `SDL_TAG` and the port with the same options.
  - Smoke as Linux, from the MSYS2 shell.
  - Check: `objdump -p` lists no `libgcc`/`libwinpthread` DLL.
  - Zip `primal-rage-port-$VER-windows-x64.zip` (`prageport.exe`, `SDL3.dll`, `README.txt`, licences).
- Job `publish`:
  - `needs: [linux, macos, windows]`, `if: startsWith(github.ref, 'refs/tags/v')`.
  - Download the three artifacts and write `SHA256SUMS` (`sha256sum *.zip *.tar.gz`).
  - Run `gh release create "$GITHUB_REF_NAME" --verify-tag --generate-notes --notes-file tools/release/README.txt`, plus `--prerelease` when the tag contains `-`, plus the four files.
  - Before uploading, check that no staged file name matches `(?i)(prage\.exe|^index$|\.gra$|\.smk$|\.opl$|cmos|ghidra_data)`, and fail if one does. This enforces the no-game-files anti-goal.

**`README.txt` content** (the spec's Goal 6, verbatim facts, plain words):
- You need your own copy of Primal Rage (DOS, 1995); the download contains no game files.
- Unzip, then run `prageport.exe` / `Primal Rage.app` / `./prageport`.
- Choose the folder where the game is installed (the one holding `PRAGE.EXE`). If you only have the CD, copy the CD's `RAGE.S16` folder contents into the install folder, or install it once under DOSBox and choose that folder.
- The folder is remembered. Saves live in the per-user folder (macOS `~/Library/Application Support/primal-rage/`, Windows `%APPDATA%\primal-rage\`, Linux `~/.local/share/primal-rage/`).
- Unsigned-app warnings:
  - macOS: right-click → Open, or `xattr -dr com.apple.quarantine "Primal Rage.app"`.
  - Windows: More info → Run anyway.
- `--game-dir <folder>` overrides the saved folder.
- Licence: MIT. Primal Rage is the property of its rights holders.

- [ ] **Step 1: Write the files above.**
- [ ] **Step 2: Lint the workflow.** Run `actionlint .github/workflows/release.yml` (install with `brew install actionlint` if missing). Expected: no output.
- [ ] **Step 3: Dry run on GitHub. Ask the owner first, because this pushes a branch.** Push the branch, then `gh workflow run release.yml --ref <branch>` and `gh run watch`. Expected: the three build jobs are green, `publish` is skipped, and the three archives are downloadable as artifacts. Download each one: no game file inside, and the macOS `.app` opens the picker on the owner's Mac.
- [ ] **Step 4: Check the tag filter.** GitHub's filter syntax treats `+` as "one or more of the preceding character" and `.` literally. So `v[0-9]+.[0-9]+.[0-9]+` and `v[0-9]+.[0-9]+.[0-9]+-*` both require three numbers, and `v1.0-final` matches neither. Confirm this against GitHub's "Filter pattern cheat sheet" (via Context7 or the web) before committing. No test tag is pushed.
- [ ] **Step 5: Run the local gate once more** (docs changed, AGENTS.md included; `tools/tests/test_final_claims.py` reads AGENTS.md). Green.
- [ ] **Step 6: Commit.** `git add .github/workflows/release.yml tools/release/README.txt tools/release/Info.plist LICENSE README.md docs/PROGRESS.md AGENTS.md && git commit -m "release: tag-driven GitHub release for Windows, Linux and macOS"`
- [ ] **Step 7: First release (the owner's action).** After merging to `main`, the owner pushes a tag of their choosing (e.g. `git tag v1.1.0 && git push origin v1.1.0`) and checks that the release has four assets.
