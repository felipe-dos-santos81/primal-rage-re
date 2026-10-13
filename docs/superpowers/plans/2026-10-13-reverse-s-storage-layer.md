# Track S: the storage layer — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the port's declared no-op storage layer (14 deferred functions and the no-op `0x2D4EC`) with the original's code, with a persistent save file and one capture of the original saving and reloading.

**Architecture:** The `0x7F8`-byte image stays in `mem[]` at `0x100CE4`. The 15 functions go in `port/src/game/config.c`, one C function per original function, each with a differential row. File bytes move through four new `host.c` calls; `main.c` resolves the save path. The work is one serial preparation (tasks 0-2), three parallel port batches (tasks 3-5), an integration, a read-only audit, a capture with its oracle, and the gate.

**Tech Stack:** C11 over `mem[]`, SDL3 (`SDL_GetPrefPath`, 3.2.0+), Python 3 with `unicorn` 2.1.4 and `capstone` 5.0.7 (`tools/requirements-diff.txt`), DOSBox-X for the one capture, Ghidra MCP read-only.

**Spec:** `docs/superpowers/specs/2026-10-13-storage-layer-design.md`. Executors read both.

## Global Constraints

- Ghidra address == linear address. A fixed-up image is `./build/diffrun --exe data/game/C/PRAGE.EXE --image-out <file>`; image offset = address − `0x10000`. Never read data addresses from a raw-file disassembly.
- Every value is derived from the bytes with the address that proves it. No fitted constant. On a plan-vs-raw conflict the raw wins and the correction is recorded with its address. **This is why the port function bodies are not written in this plan:** each is written from its derivation in the record, and a body copied from a plan would be a fitted one.
- Comment styles in `port/src`: `/* 0xADDR — … */`, `/* PORT: … */`, `/* TODO(verify): … */` only.
- All original state lives in `mem[]` at its original address (`DSB/DSW/DSD`). No long-lived C global shadows it.
- SDL and file I/O live only in `port/src/host.c` and `port/src/main.c`.
- Never hand-edit `port/src/symbols.h`. Never write under `data/`. Never write `mem[0xA0000]`.
- Every binding and mutant stays inside `mem[]`; a deliberate read above the image is named in `P45_OUTSIDE`.
- Tests use only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`; a new test is registered once in `port/tests/test.h`; every new assertion is proved to fail under a mutation of the code it tests; no assertion on an unseeded BSS zero.
- Commit style `<area>: <what changed>`; stage named files, never `git add -A`; every message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Agents commit on their own branch only. Nothing is merged to `main` or pushed without the user's explicit request.
- Agents never run `make verify`, the full `python3 -m unittest tools.tests.test_diff_verify`, `make diff-verify`, any oracle target or any capture target. The integrator runs them.
- Agents use the Ghidra MCP read-only: no rename, retype, comment or save.
- At most 4 agents in the track: batches `sa`, `sb`, `sc` (parallel) and one audit. No other agent is started.
- The DOSBox-X capture (task 8) runs once, by the integrator, on the user's go at that point.

## Review Focus

Conditions the spec implies that a person using the port will meet, most likely first. Each has its test in the task named.

1. **A save file of the wrong length** (truncated by a crash, or an unrelated file named `cmos`). Expected: treated as a failed read, the image becomes `0xFF` as at `0x1B05D`, no read past the buffer. Test: task 5, `test_config_file` cases `short` and `long`.
2. **The save path cannot be written** (directory removed, read-only volume, `--cmos` naming a directory). Expected: the game runs and quits normally, `host_save_writable()` is 0, nothing is written, no crash. Test: task 2, `test_host_save` case `unwritable`.
3. **A save whose first config copy is damaged but whose second is good.** Expected: the original's fallback at `0x2D673` uses the second copy. Test: task 3, a differential case and the unit case `copy2`.
4. **`--cmos` with no argument.** Expected: the usage text and a non-zero exit, as for the other options. Test: task 2, the `main.c` argument check run by hand (step 6).
5. **A corrupt checksum byte.** Expected: the `0xFF` image, the defaults path, and the next save overwrites the bad file. Test: task 5, case `corrupt`.

---

## File Structure

| File | Responsibility | Tasks |
|---|---|---|
| `docs/superpowers/plans/2026-10-13-reverse-s-derivations.md` | the track's derivation record (§S.1 …) | 1, 3-7, 9 |
| `port/src/host.h`, `port/src/host.c` | the four `host_save_*` calls | 2 |
| `port/src/main.c` | `--cmos PATH`, the default save path | 2 |
| `port/src/game/flow.c` | `game_set_game_dir` hands the host the fallback dir; the call sites `0x2F9CF`, `0x1BEB5`, `0x24AA1`; the audit sites in `flow.c` | 2, 4, 5 |
| `port/src/game/config.c`, `config.h` | the 15 functions | 3, 4, 5 |
| `port/src/game/svcmenu.c`, `fight.c` | the sites `0x2CBAF` and `0x43E77` | 5, 4 |
| `port/tests/test_platform.c` | `test_host_save` | 2 |
| `port/tests/test_game.c` | `test_config_storage`, `test_audit`, `test_config_file` | 3, 4, 5 |
| `port/tests/test.h` | the registry lines | 2-5 |
| `tools/diff_verify.py`, `port/tests/diff_runner.c`, `tools/tests/test_diff_verify.py` | specs, bindings, mutants, pins | 3-6 |
| `tools/port_classification.txt`, `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md` | the 14 rows leave; the table is regenerated | 6 |
| `tools/k11_session.py`, `tools/k11_capture.py`, `tools/save_reload.py`, `Makefile` | the `save-reload` capture and oracle | 8 |
| `README.md`, `AGENTS.md`, `docs/PROGRESS.md` | counts and narrative | 9 |

Ledger (git-ignored): `.superpowers/sdd/2026-10-13-reverse-s-storage-layer/progress.md`. Append one dated line per finished step; read its tail first when resuming.

**C names fixed for the whole track** (the registers and the bounds below were read from the entry instructions while reviewing this plan; the record confirms each, and where it differs the record wins and the table is corrected in the same commit):

| Address | C function | Original arguments |
|---|---|---|
| `0x2E990` | `s32 config_storage_read(u32 cell, u32 dst, u32 len)` | EAX, EDX, EBX; returns 1 or -1 |
| `0x2D498` | `void config_image_store(u32 addr, u32 value)` | EAX, DL |
| `0x2D4EC` | `config_storage_touch(u32 kind)` (exists) | EAX |
| `0x2D638` | `void config_storage_load(void)` | none |
| `0x2DE98` | `void hiscore_storage_load(u32 defaults)` | EAX |
| `0x2DF8C` | `void config_storage_validate(u32 defaults)` | EAX |
| `0x2DAE4` | `s32 audit_add(u32 field, u32 n)` | EAX, EDX; its callers read EAX (record §S.2); reads the field through `0x2D974` (`config_field_get`) and returns early when that is negative (`0x2DAF0`) |
| `0x2E934` | `s32 audit_post(u32 kind, u32 value)` | EAX, EDX; -1 when `kind > 3` (`0x2E93D`) |
| `0x2E180` | `s32 audit_counter_add(u32 table, u32 value)` | EAX, EDX; -1 path when `table >= 3` (`0x2E18B`) |
| `0x2E0A4` | `s32 audit_counters_halve(u32 table)` | EAX; -1 when `table >= 3` (`0x2E0AD`) |
| `0x2E034` | `s32 audit_counter_store(u32 table, u32 value)` | EAX, EDX; -1 when `table >= 3` (`0x2E03D`) |
| `0x32B94` | `void audit_add_if_bit0(u32 flags)` | AL |
| `0x32BB0` | `void audit_characters(u32 c0, u32 c1)` | EAX, EDX |
| `0x1AFE8` | `u32 config_file_load(void)` | none; returns the failure flag |
| `0x1B084` | `void config_file_save(void)` | none |

---

### Task 0: Ledger, integration worktree, baseline

**Files:** Create `.superpowers/sdd/2026-10-13-reverse-s-storage-layer/progress.md`; worktree `.worktrees/reverse-sint` (branch `reverse-sint` off `main`).

- [ ] **Step 1: Create the worktree and build it**

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse
git worktree add .worktrees/reverse-sint -b reverse-sint main
ln -s ../../data .worktrees/reverse-sint/data
ln -s ../../.superpowers .worktrees/reverse-sint/.superpowers
cp port/tests/ghidra_data.bin .worktrees/reverse-sint/port/tests/
cd .worktrees/reverse-sint && make build
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_s_img.bin
```

Expected: the build ends without error; `/tmp/pr_s_img.bin` exists.

- [ ] **Step 2: Baseline**

```bash
python3 tools/port_progress.py
ls -la /tmp/claude-501/d6int_verify.log
```

Expected: `771 1203 64` and `731 731 100 …`. The log is the last gate's output and is the oracle baseline. If it is gone, run `make verify > /tmp/claude-501/s_base_verify.log 2>&1; echo EXIT=$?` on this untouched tree (EXIT=0) and use that file as the baseline in tasks 6 and 9.

- [ ] **Step 3: Start the ledger** with the date, the branch, the baseline counter line (`963/963 functions VERIFIED; 14349/14349 mutants detected; 1 named gaps; 604/796 rows with callees closed (167 have none)`) and the baseline log's path.

---

### Task 1: Derivation and measurement (serial, no port change)

**Files:** Create `docs/superpowers/plans/2026-10-13-reverse-s-derivations.md`.

**Interfaces:** Produces the record sections the batches implement from: §S.1 (the image and its cell layout), §S.2 (per function: entry registers, every block, `ret N`, what each caller reads of EAX, callees), §S.3 (the zero-image measurement), §S.4 (the call-site table with each caller's port status).

- [ ] **Step 1: Read what track D already measured.** `docs/superpowers/plans/2026-10-13-reverse-d6a-derivations.md` §4 and the block `# ---- 0x2D6F8: the configuration validate` in `tools/diff_verify.py` (the partial row: both reads return 1 over the image's zero storage, the two validators nopped, six more `0x2D4EC` calls). Also record `2026-09-24-demo-pose-derivations.md` §49-Y.5 and §50-C, and `2026-09-29-k1-k9-derivations.md` §K9.6-§K9.12.

- [ ] **Step 2: Derive the 15 functions** from capstone over `/tmp/pr_s_img.bin` and the Ghidra listing. For each, write in §S.2: the entry registers, each block with its addresses, the `ret` form, the callees, and the caller instructions that read its EAX (or that none does). Confirm or correct the name table above.

- [ ] **Step 3: Measure what the full chain leaves behind over the zero image.** Save as `scratchpad/s_measure.py` and run it from the worktree root:

```python
import sys
sys.path.insert(0, "tools")
import diff_emu as E

img = E.Image.load(sys.argv[1])
NOP5 = b"\x90" * 5
STUBS = (E.Call(0x2CADC, (), clobbers=("edi",)), E.Call(0x2D974, ("eax",)))
TODAY = (0x2D638, 0x2E990, 0x61A70, 0x65490, 0x654C7, 0x2D4B4)       # the d6a row's closure
FULL = TODAY + (0x2D4EC, 0x2D498, 0x2DE98, 0x2DF8C, 0x2DB58, 0x2EA68)

def run(pokes, allow, calls):
    r = E.run_original(img, 0x2D6F8, regs={}, pokes=pokes, allow_calls=allow, calls=calls)
    assert r.outcome == "ok", (r.outcome, r.detail)
    return r

base = {} if len(sys.argv) < 3 else {0x100CE4: b"\xff" * 0x7F8}     # any second argument: the 0xFF image
today = run({**base, 0x2D912: NOP5, 0x2D919: NOP5}, TODAY, STUBS + (E.Call(0x2D4EC, ("eax",)),))
full = run(dict(base), FULL, STUBS)
IMG_LO, IMG_HI = 0x100CE4, 0x1014DC
delta = {a: v for a, v in full.writes.items() if today.writes.get(a) != v}
gone = sorted(a for a in today.writes if a not in full.writes)
inside = sorted(a for a in delta if IMG_LO <= a < IMG_HI)
outside = sorted(a for a in delta if not IMG_LO <= a < IMG_HI)
print("today calls:", [(hex(a), args) for a, args in today.calls])
print("full  calls:", [(hex(a), args) for a, args in full.calls])
print("image bytes written:", len(inside), "first", hex(inside[0]) if inside else None,
      "last", hex(inside[-1]) if inside else None)
print("bytes outside the image that differ from today's model:", len(outside))
for a in outside:
    print("  0x%X = 0x%02X (today %s)" % (a, delta[a], today.writes.get(a)))
print("written today but not by the full chain:", [hex(a) for a in gone])
print("reads outside image/stack:", full.outside)
```

Run: `python3 -P scratchpad/s_measure.py /tmp/pr_s_img.bin`
If a run stops with `unmodeled` naming a call target, read that target's bytes, add it to `FULL` (a storage or libc helper) or to `STUBS` (anything else), note it in §S.3, and rerun.

Expected, from the dry run made while reviewing this plan (on `main` at `f55b92c5`): `image bytes written: 22 first 0x100ce4 last 0x100e4e`, `bytes outside the image that differ from today's model: 0`, nothing written today that the full chain does not write, no read outside the image. A different output is a finding.

The limit of this measurement, to state in §S.3: `0x2CADC` (`config_set_defaults`) is a stub on both sides, so the records are encoded from a zeroed config block. It shows that `0x2DE98`, `0x2DF8C` and the real `0x2D4EC` add no state outside the image on this path; it does not show the image's final bytes at boot. Those are measured after integration by the unit case `validate_zero` and, for a real save, by task 8.

- [ ] **Step 4: Classify every byte outside the image** (none is expected). For each address in the second list, find which ported code reads it (`grep -rn` the `DS_` name in `port/src`, and the Ghidra cross-references). Write §S.3: the address, the value, the reader, and one of *no reader in the port*, *read only by the storage layer*, or *read by … : a pin can move*. If any is the third kind, stop and report to the user before task 2: the spec's risk "a pin moves" has fired.

- [ ] **Step 5: Repeat over the `0xFF` image**: `python3 -P scratchpad/s_measure.py /tmp/pr_s_img.bin ff`. Expected from the dry run: `image bytes written: 464 first 0x100ce4 last 0x101082` and again 0 bytes outside. Record it in §S.3, marked as having no capture behind it: the `session.txt` of all 22 captures that record it says `cmos=zero`.

- [ ] **Step 6: Write §S.4**, the call-site table of the spec's §2 with, per site, the containing function, whether that function is ported (`grep -n "0xADDR" port/src`, `tools/port_classification.txt`), and the port line that holds the `PORT:` note today. The two sites in `0x1E988` were not checked when the spec was written.

- [ ] **Step 7: Commit**

```bash
git add docs/superpowers/plans/2026-10-13-reverse-s-derivations.md
git commit -m "docs: track S record: the storage layer derived, the zero-image measurement, the call-site table"
```

---

### Task 2: The host save calls and `--cmos`

**Files:**
- Modify: `port/src/host.h`, `port/src/host.c`, `port/src/main.c`, `port/src/game/flow.c` (`game_set_game_dir`, line ~6808)
- Test: `port/tests/test_platform.c`, `port/tests/test.h`

**Interfaces:**
- Consumes: `host_read_file(path, dst, max, &len)` and `host_write_file(path, src, len)` (existing, return 1 on success).
- Produces, for task 5:

```c
/* PORT: the original opens "cmos" in the current directory (0xA2C5C). */
void host_save_set_path(const char *path);     /* NULL = no save file */
void host_save_set_fallback(const char *dir);  /* the game dir, read only; NULL = none */
int  host_save_read(u8 *dst, u32 len);         /* 1 = exactly len bytes read */
int  host_save_writable(void);                 /* 1 = the "wb" open would succeed */
int  host_save_write(const u8 *src, u32 len);  /* 1 = len bytes written to the save path */
```

- [ ] **Step 1: Write the failing test.** Add to `port/tests/test_platform.c` and register `X(test_host_save)` in `TEST_CASES` (`port/tests/test.h`):

```c
int test_host_save(void)
{
    char dir[] = "/tmp/pr_save_XXXXXX";
    char save[300], fb[300], sub[300];
    u8 buf[8], want[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    u8 other[8] = {9, 9, 9, 9, 9, 9, 9, 9};

    CHECK(mkdtemp(dir) != NULL, "scratch dir");
    snprintf(save, sizeof save, "%s/save/cmos", dir);
    snprintf(sub, sizeof sub, "%s/save", dir);
    snprintf(fb, sizeof fb, "%s/CMOS", dir);

    /* nothing set: no read, not writable, no write */
    host_save_set_path(NULL);
    host_save_set_fallback(NULL);
    memset(buf, 0xEE, sizeof buf);
    CHECK_EQ_INT(host_save_read(buf, 8), 0);
    CHECK_EQ_INT(host_save_writable(), 0);
    CHECK_EQ_INT(host_save_write(want, 8), 0);

    /* the fallback alone: read from <dir>/CMOS (the upper-case name on disk) */
    CHECK_EQ_INT(host_write_file(fb, other, 8), 1);
    host_save_set_fallback(dir);
    CHECK_EQ_INT(host_save_read(buf, 8), 1);
    CHECK_EQ_INT(memcmp(buf, other, 8), 0);
    CHECK_EQ_INT(host_save_writable(), 0);          /* a fallback is never written */
    CHECK_EQ_INT(host_save_write(want, 8), 0);

    /* unwritable: the save path's directory does not exist */
    host_save_set_path(save);
    CHECK_EQ_INT(host_save_writable(), 0);
    CHECK_EQ_INT(host_save_write(want, 8), 0);
    CHECK_EQ_INT(host_save_read(buf, 8), 1);        /* still the fallback */
    CHECK_EQ_INT(memcmp(buf, other, 8), 0);

    /* writable: the save path wins over the fallback from then on */
    CHECK_EQ_INT(mkdir(sub, 0700), 0);
    CHECK_EQ_INT(host_save_writable(), 1);
    CHECK_EQ_INT(host_save_write(want, 8), 1);
    memset(buf, 0xEE, sizeof buf);
    CHECK_EQ_INT(host_save_read(buf, 8), 1);
    CHECK_EQ_INT(memcmp(buf, want, 8), 0);

    /* a wrong length is a failed read, and the fallback does not stand in for it */
    CHECK_EQ_INT(host_save_read(buf, 7), 0);

    host_save_set_path(NULL);
    host_save_set_fallback(NULL);
    return 0;
}
```

Add `#include <sys/stat.h>` and `#include <unistd.h>` to the file if absent.

- [ ] **Step 2: Run it and see it fail to link**

Run: `cmake --build build 2>&1 | tail -5`
Expected: an undefined-symbol error naming `host_save_read`.

- [ ] **Step 3: Implement in `port/src/host.c`** (declarations with the comments above in `host.h`):

```c
static char s_save_path[1024];
static char s_save_fallback[1024];

void host_save_set_path(const char *path)
{
    snprintf(s_save_path, sizeof s_save_path, "%s", path ? path : "");
}

void host_save_set_fallback(const char *dir)
{
    snprintf(s_save_fallback, sizeof s_save_fallback, "%s", dir ? dir : "");
}

static int save_read_exact(const char *path, u8 *dst, u32 len)
{
    u32 got = 0;
    return host_read_file(path, dst, len, &got) && got == len;
}

int host_save_read(u8 *dst, u32 len)
{
    char path[1100];
    FILE *f;

    if (s_save_path[0] && (f = fopen(s_save_path, "rb")) != NULL) {
        fclose(f);
        return save_read_exact(s_save_path, dst, len);   /* a bad save is a failed read */
    }
    if (!s_save_fallback[0])
        return 0;
    snprintf(path, sizeof path, "%s/cmos", s_save_fallback);
    if (save_read_exact(path, dst, len))
        return 1;
    snprintf(path, sizeof path, "%s/CMOS", s_save_fallback);
    return save_read_exact(path, dst, len);
}

int host_save_writable(void)
{
    FILE *f;

    if (!s_save_path[0] || (f = fopen(s_save_path, "ab")) == NULL)
        return 0;
    fclose(f);
    return 1;
}

int host_save_write(const u8 *src, u32 len)
{
    return s_save_path[0] ? host_write_file(s_save_path, src, len) : 0;
}
```

`host_save_writable` opens for append, so probing never truncates an existing save. It can leave an empty file behind when the later write fails; `host_save_read` then reports a failed read for it, which is the original's behaviour for a damaged `cmos`.

Check `host_read_file`'s contract before relying on it: it fails when the file is larger than `max`, so a longer file is a failed read too. If the body shows otherwise, compare the size explicitly.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build && PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -3`
Expected: the suite passes; the CHECK total rose by exactly this test's assertions.

- [ ] **Step 5: Mutation proofs.** One at a time, apply, rebuild, see `test_host_save` fail, revert: (a) `host_save_writable` returns 1 unconditionally; (b) `host_save_read` drops the `got == len` test; (c) `host_save_read` tries the fallback before the save path; (d) `host_save_write` writes to the fallback dir. Record the four failures in the ledger.

- [ ] **Step 6: Wire the fallback and the option.**

In `port/src/game/flow.c`, `game_set_game_dir` gains one line after `s_game_dir = dir;`:

```c
    host_save_set_fallback(dir);   /* PORT: a run with no save file reads the game dir's cmos */
```

In `port/src/main.c`: parse `--cmos PATH` beside `--game-dir` (same form: `strcmp(argv[i], "--cmos") == 0 && i + 1 < argc`), add it to the `--help` text, and make a trailing `--cmos` with no value fall into the existing unknown-argument error. In `run_windowed` only, before `game_main()`:

```c
    if (cmos_path) {
        host_save_set_path(cmos_path);
    } else {
        char *pref = SDL_GetPrefPath(NULL, "primal-rage");   /* ends with a separator; NULL on failure */
        if (pref) {
            char path[1024];
            snprintf(path, sizeof path, "%scmos", pref);
            host_save_set_path(path);
            SDL_free(pref);
        }
    }
```

`run_check` sets no path. Check by hand: `./build/prageport --cmos; echo $?` prints the usage and a non-zero status; `./build/prageport --help` lists `--cmos`.

- [ ] **Step 7: Commit**

```bash
git add port/src/host.h port/src/host.c port/src/main.c port/src/game/flow.c port/tests/test_platform.c port/tests/test.h
git commit -m "host: the save-file calls and --cmos (no caller yet)"
```

- [ ] **Step 8: Create the three batch worktrees** off `reverse-sint`, as in task 0 step 1, named `reverse-sa`, `reverse-sb`, `reverse-sc`, each built, each with its own image `/tmp/pr_<id>_img.bin`. Write each batch's queue file `.superpowers/sdd/2026-10-13-reverse-s-storage-layer/<id>-queue.txt` from §S.2 and §S.4 (address, size, C name, callees, call sites).

---

### Tasks 3-5: the three port batches (parallel, one agent each)

**Method, common to the three.** Read `AGENTS.md`, the spec, the record's §S.1-§S.4, `.superpowers/sdd/2026-10-11-reverse-d1/brief-wave5.md` (the per-row method, the hard limits, the report format) and record `2026-10-01-reverse-e3-derivations.md` §E3.10. What differs from that brief:

- You port functions as well as row them. Per function, in this order: derive from the bytes (the record's §S.2 is the start, not the authority: re-read the bytes); write the unit test for the behaviour and see it fail; write the C function with its `/* 0xADDR — record §S.n */` header and a `PR_SEAM`/`PR_SEAM_RET` opening where another row stubs it; see the test pass; add the binding, the `Spec` and the mutants; run `python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE --image /tmp/pr_<id>_img.bin --function <name> --self-check`; commit.
- A port commit that adds a `/* 0xADDR` header does **not** regenerate the E2 table or touch `tools/port_classification.txt`; the integrator does both once in task 6.
- A callee owned by another batch is stubbed in your row and its call site in your functions is written as the real call only if the C function already exists in your tree (`config_storage_touch` does). Otherwise keep the `PORT:` note and list the site in your report for the integrator.
- A call site that becomes a real call is a behaviour change in a function that already has a differential row. Update that row's `calls=` and cases in the same commit, and keep the stub-clobber dictionary consistent (`tools/tests/test_diff_verify.py`).
- Harness files: insertions in one contiguous `<id>` block per file, as the wave-5 brief says. A mechanism change in `tools/diff_emu.py` or the runner is its own commit and leads your report.
- One full `--self-check` at the end with `PR_DIFF_JOBS=4`, in the background (it exceeds a 10-minute foreground limit).
- Record: your section of `docs/superpowers/plans/2026-10-13-reverse-s-derivations.md` (`sa` §S.5, `sb` §S.6, `sc` §S.7), same shape as the d5/d6 batch records.

### Task 3: Batch `sa` — the image and its validators

**Files:** Modify `port/src/game/config.c`, `config.h`, `port/tests/test_game.c`, `port/tests/test.h`, the three harness files, the record §S.5.

**Interfaces:**
- Consumes: nothing from tasks 4-5.
- Produces: `config_storage_read`, `config_image_store`, the real `config_storage_touch`, `config_storage_load`, `hiscore_storage_load`, `config_storage_validate` with the signatures of the name table.

Queue, in this order (leaves first):

| # | Address | Notes |
|---|---|---|
| 1 | `0x2D498` | bounds `0x100CE4 <= addr < 0x1014DC`; outside calls the named no-op `0x2EA68` |
| 2 | `0x2E990` | decodes into `dst`; the row passes `dst` as a `mem[]` offset |
| 3 | `0x2D4EC` | replaces the no-op body; kinds 0..8, the kind table `0x2D444`, the bound `0x7F8`; its 18 call sites are untouched |
| 4 | `0x2D638` | two `0x51`-byte stack buffers: pass by value at the seam (record `2026-10-02-reverse-p2-derivations.md` §P2.7) or keep the buffers inside the function if no row stubs through them |
| 5 | `0x2DE98` | three high-score blocks through `hiscore_locate` (`0x2DB58`) |
| 6 | `0x2DF8C` | tables 3..5 |
| 7 | `config_validate` (`0x2D6F8`) | replace the declared `-1` reads, the dead version arms (`0x2D7A9`..`0x2D802`) and the stored-image arm (`0x2D8BB`) with the raw's code; call `0x2DE98(…)` at `0x2D912` and `0x2DF8C(…)` at `0x2D919`; leave `0x2D962` (`0x2DAE4`) as its `PORT:` note for the integrator |

- [ ] **Step 1: Unit test `test_config_storage`** in `port/tests/test_game.c` (register `X(test_config_storage)`), grown function by function. Its cases, each with seeded sentinels that differ from the post-condition:
  - `store`: `config_image_store` writes at `0x100CE4` and at `0x1014DB`, and leaves `0x100CE3` and `0x1014DC` untouched;
  - `roundtrip`: for each kind 0..8, set the record's source bytes to a non-zero pattern, `config_storage_touch(kind)`, clear the source, read it back with `config_storage_read` at the kind's cell and length: the bytes return and the result is 1;
  - `parity`: flip one image byte of an encoded record: the read returns -1;
  - `bound`: a kind table entry poked so the record would end past `0x7F8` writes nothing and the image is unchanged;
  - `copy2` (Review Focus 3): an image whose first config copy has a parity error and whose second is good: `config_storage_load` leaves the second copy's bytes where the raw puts them;
  - `validate_stored`: an image saved from non-default config: `config_validate` takes the stored-image arm and the config fields equal the saved ones;
  - `validate_zero`: the zero image: `config_validate` leaves exactly the bytes §S.3 lists.
- [ ] **Step 2: Port and row the queue**, one function per commit (`port: <c name> (0xADDR) from the bytes, with its row`). Mutants are the plausible wrong ports: off-by-one in the cell stride, the parity bit order, `<` for `<=` in the bound, a skipped fallback, the two reads swapped.
- [ ] **Step 3: The existing `config_validate` row.** Remove the pins `0x2D912`/`0x2D919` and the `unhit_named` entries that the port now reaches; add cases for the version arms and the stored-image arm. A block that is still unhit stays named with its reason. The row of `config_set_defaults` (`0x2CADC`) is not yours: it is partial because of three display calls (`0x2F198`, `0x2EA78(0xB4)`, `0x2F280`), not because of storage, and stays as it is.
- [ ] **Step 3b: The existing unit assertions.** `test_config` and any other test that encodes the no-storage model (a `-1` read, an untouched image) is updated to the raw's behaviour, one assertion at a time, each with the address that proves the new value and each still mutation-proved. List every changed assertion in your report.
- [ ] **Step 4: Mutation-prove every new assertion** of `test_config_storage` (list the mutation per case in the record).
- [ ] **Step 5: Full `--self-check`, the record §S.5, the final report** (per queue entry: full row / partial row with the reason; every correction with its address; every call site left for the integrator).

### Task 4: Batch `sb` — the audit counters

**Files:** Modify `port/src/game/config.c`, `config.h`, `flow.c`, `fight.c`, `port/tests/test_game.c`, `port/tests/test.h`, the three harness files, the record §S.6.

**Interfaces:**
- Consumes: `config_storage_touch(u32 kind)` (the seam exists; in your tree its body is still the no-op, so stub it in every row and do not depend on the image).
- Produces: `audit_add`, `audit_post`, `audit_counter_add`, `audit_counters_halve`, `audit_counter_store`, `audit_add_if_bit0`, `audit_characters`.

Queue: `0x2E034`, `0x2E0A4`, `0x2E180`, `0x2E934`, `0x2DAE4`, `0x32B94`, `0x32BB0`, then the call sites.

Call sites to turn from `PORT:` notes into calls (§S.4 gives the port line of each): `0x2DAE4` at `0x1E9BB`, `0x1E9EB`, `0x1EC50`, `0x1EC84`, `0x270A9`, `0x27A5F`, `0x27F1F`, `0x28E53`, `0x32A85`, `0x32A9B`, `0x32AAA`, `0x32AB6`, `0x32ADE`, `0x32AF5`, `0x42425`; `0x2E934` at `0x32B35`, `0x32B7E`, `0x43E77`; `0x32B94` at `0x2786B`; `0x32BB0` at `0x27E28`. Not yours: `0x2D962` (in `config_validate`, batch `sa`'s file region; the integrator wires it) and `0x32A17` (inside the host-owned `0x32970`; it stays omitted and named).

- [ ] **Step 1: Unit test `test_audit`** (register `X(test_audit)`), cases:
  - `add`: `audit_add(field, n)` raises the config field by `n` from a seeded non-zero value, and writes every other byte the record's §S.2 shows it writes (seed them all);
  - `negative`: a field for which `config_field_get` returns a negative value is left untouched (`0x2DAF0`);
  - `field3`: field 3 on both sides of the `0x100` test at `0x2DAF5..0x2DB01`;
  - `clamp`: each clamp the raw applies (the record names the limit and its address) at the limit and one below;
  - `halve`: the overflow path of `0x2E180` halves the table through `0x2E0A4` exactly as the raw does;
  - `post`: `audit_post` for each `kind` the three call sites pass, and `kind = 4` returns -1 and writes nothing;
  - `tables`: `audit_counter_add`, `audit_counters_halve` and `audit_counter_store` with `table = 3` return -1 and write nothing;
  - `bit0`: `audit_add_if_bit0` adds for an odd AL and not for an even one, with the upper bytes of EAX set;
  - `characters`: `audit_characters(c0, c1)` for a character below 7, at 7, and `c1 = -1`.
- [ ] **Step 2: Port and row the seven functions**, one per commit.
- [ ] **Step 3: The call sites**, one commit per containing function (`port: <function> calls audit_add at 0xSITE (raw order)`), each with the argument registers proved from the site's bytes, the containing function's row updated (`calls=` gains the `E.Call`, a mutant drops or reorders the call), and its `PORT:` note removed. A site whose containing function has no row gets the call and a line in your report.
- [ ] **Step 4: Mutation proofs, full `--self-check`, the record §S.6, the final report.**

### Task 5: Batch `sc` — the save file

**Files:** Modify `port/src/game/config.c`, `config.h`, `flow.c`, `svcmenu.c`, `port/tests/test_game.c`, `port/tests/test.h`, the three harness files, the record §S.7.

**Interfaces:**
- Consumes: the five `host_save_*` calls of task 2; `config_storage_touch(u32 kind)` (a no-op body in your tree; stub it in rows).
- Produces: `u32 config_file_load(void)` and `void config_file_save(void)`.

The raw, to re-derive: `0x1AFE8` reads `0x7F8` bytes to `0x100CE4`, XORs bytes `0..0x7F6`, compares with `0x1014DB`, and on a failed open, a short read or a mismatch fills the image with `0xFF` (`0x1B05D`); it returns the failure flag. `0x1B084` on a successful open calls `0x2D4EC(1)`, `0x2D4EC(2)`, stores the XOR in `0x1014DB` and writes `0x7F8` bytes. The `fopen`/`fread`/`fwrite`/`fclose` calls map to the host calls: open-for-read plus read is `host_save_read(mem + 0x100CE4, 0x7F8)`; open-for-write is `host_save_writable()`; the write is `host_save_write`. The wrappers `0x10D34`/`0x10D0C` stay omitted under one `PORT:` note each. For the differential rows the libc calls are stubbed on the original's side and the host calls are driven on the port's side through a runner hook: read how `d6a_scratch_hook` does it in `port/tests/diff_runner.c` and reuse that mechanism.

- [ ] **Step 1: Unit test `test_config_file`** (register `X(test_config_file)`). Each case uses a `mkdtemp` scratch dir and resets `host_save_set_path(NULL)`/`host_save_set_fallback(NULL)` at its end:
  - `roundtrip`: seed the image with a non-zero pattern, `config_file_save()`, snapshot the image, check the file is `0x7F8` bytes and its last byte is the XOR of the first `0x7F7`; fill the image with `0x55`; `config_file_load()` returns 0 and the image equals the snapshot;
  - `corrupt` (Review Focus 5): flip one byte of the file: the load returns non-zero and every image byte is `0xFF`; a following `config_file_save()` leaves a file that loads clean;
  - `missing`: no file and no fallback: non-zero and `0xFF`;
  - `short` and `long` (Review Focus 1): files of `0x7F7` and `0x7F9` bytes: non-zero and `0xFF`, and the byte `0x1014DC` (seeded `0xA5`) is untouched;
  - `nopath`: with no save path, `config_file_save()` writes no file, leaves `0x1014DB` (seeded) unchanged and does not reach `config_storage_touch` (count the seam's calls with the fixture the other tests use, or check that the image is unchanged);
  - `fallback`: a zero `CMOS` in the fallback dir loads as the zero image with result 0.
- [ ] **Step 2: Port and row `0x1AFE8` and `0x1B084`**, one per commit.
- [ ] **Step 3: The four call sites**, one commit each: `0x2F9CF` (`game_init`, before `config_validate`, in the raw's order within the inlined `0x2F9CC`), `0x1BEB5` (`game_shutdown`), `0x24AA1` (`flow.c`, before `game_restart_longjmp`), `0x2CBAF` (`svcmenu.c`). Remove each `PORT:` note; update the containing function's row where it has one.
- [ ] **Step 4: Mutation proofs, full `--self-check`, the record §S.7, the final report.**

---

### Task 6: Integration

**Files:** branch `reverse-sint`; `tools/port_classification.txt`; `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md`; `tools/tests/test_diff_verify.py` (the counter pin).

- [ ] **Step 1: Merge `sa`, then `sb`, then `sc`.** On a conflict in the harness files or `config.c`, resolve with the compose merge: `python3 .superpowers/sdd/2026-10-11-reverse-d1/compose.py $(git merge-base HEAD reverse-<id>) <file> HEAD reverse-<id>`. Remove duplicate keys from the stub-clobber dictionary.
- [ ] **Step 2: Wire the cross-batch sites** the batches reported: `0x2D962` (`config_validate` → `audit_add(0x24, …)`, arguments from the bytes) and any other. One commit per site, with the row's `calls=` and a mutant.
- [ ] **Step 3: Rebuild and run the cheap checks**

```bash
cmake --build build && PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -3
python3 -P /tmp/pr_d5int/pins_check.py; python3 -P /tmp/pr_d5int/outside_check.py; python3 -P /tmp/pr_d5int/soft_asserts.py
```

Expected: the suite passes; the three pre-checks print no failure. If the scripts are gone from `/tmp`, their logic is in the ledger of track D (`.superpowers/sdd/2026-10-11-reverse-d1/progress.md`, search `pins_check`).

- [ ] **Step 4: Classification and E2.** Delete the 14 storage rows from `tools/port_classification.txt` (`1AFE8 1B084 2D638 2DE98 2E990 2DAE4 2E934 2E180 2E0A4 2E034 2DF8C 2D498 32B94 32BB0`; the 13 movie-audio rows stay). Then:

```bash
python3 tools/port_progress.py
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/e2img && python3 tools/entry_triage.py --image /tmp/e2img --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
```

Expected: `785 1203 65` and `745 745 100 …`. A different pair is a finding: a header is missing or duplicated.

- [ ] **Step 5: The harness suite and the counter pin.** Run `python3 -m unittest tools.tests.test_diff_verify 2>&1 | tail -5` in the background. Update the counter line pinned in `tools/tests/test_diff_verify.py` to the measured one and note in its comment `(after track S)`. Every other pin failure is fixed at its cause, not re-pinned.
- [ ] **Step 6: Commit** the classification, the E2 table and the pin (`tools: track S integration: the classification, the E2 table, the counter pin`).

### Task 7: Audit (one read-only agent)

- [ ] **Step 1: Dispatch one agent** with: the image `/tmp/pr_s_img.bin`, the diff `git diff main...reverse-sint -- port/src`, the record, and this brief: "Re-derive from capstone over the image, independently of the record, every function body and every call-site change in the diff. Report: (a) any instruction the port does not reproduce (address, what the raw does, what the port does); (b) any `PORT:` note that hides a difference a differential row should have caught; (c) any row that passes only because of a poke, a nop pin or a stub that hides the changed behaviour; (d) any binding or mutant that can read outside `mem[]`. Do not edit any file. Do not run `make verify` or any oracle or capture target."
- [ ] **Step 2: Fix every confirmed finding** on `reverse-sint`, one correction per commit with its address and a mutant that is the old code. Record the audit and its findings in the record (§S.8). Nothing is merged to `main` with a confirmed finding open.

### Task 8: The `save-reload` capture and its oracle

This task changes capture tooling and runs DOSBox-X. Its first step is a short design shown to the user together with the request for the capture go-ahead; nothing in it runs before that.

**Files:** Modify `tools/k11_session.py`, `tools/k11_capture.py`, `Makefile`, `port/tests/test_game.c` (the `PR_K11_DUMP` driver); create `tools/save_reload.py`, `tools/tests/test_save_reload.py`.

**Interfaces:**
- Consumes: `config_file_load`/`config_file_save`, `host_save_set_path`.
- Produces: `data/k11-captures/save-reload/` (`CMOS`, boot 2's frames and `poll.log`), `make save-reload-oracle`.

- [ ] **Step 1: Read** `tools/k11_capture.py` (staging, `MOUNT … -ro`, `check_cmos`, the poller), `tools/k11_session.py` (`SCENARIOS`, the `port-script` output, the `poll.log` format), `tools/k11_compare.py`, the `k11-capture`/`k11-oracle` Makefile targets, and the `PR_K11_DUMP` driver. Derive the key list that reaches the OPTIONS MENU (`0x2CB94`), changes one option and leaves it, from `svcmenu.c` and record §K11.2; the save is the call at `0x2CBAF`.
- [ ] **Step 2: Write the task's design** in the record (§S.9): the scenario's keys and time limit; how boot 1 mounts the staging copy writable and how boot 2 reuses it; the three polled ranges (`0x100CE4..0x1014DB`, `0x105DB0..0x105DD5`, the high-score tables located through `0x2DB58`); the pinned frame of boot 2; the port driver's save path (an env var read by the `PR_K11_DUMP` driver); what each of the two claims compares. **Show it to the user and ask for the capture go-ahead.**
- [ ] **Step 3: Implement the tooling test-first** (`tools/tests/test_save_reload.py` for the comparison logic and the scenario parsing; `check_cmos` stays enforced for every other scenario, with a test that says so).
- [ ] **Step 4: Run the capture** (`make k11-capture scenario=save-reload`), nobody typing in the DOSBox-X window, then `python3 tools/gp_capture.py check-input data/k11-captures/save-reload` or the K11 equivalent the tool provides. Record the `CMOS` file's sha256 and the `poll.log` sha256 in the Makefile as the other captures do.
- [ ] **Step 5: The oracle.** `make save-reload-oracle`: *write* (the port's replay of boot 1 with a scratch save path writes a file byte-identical to the captured `CMOS`) and *load* (the port booted from the captured `CMOS` holds, at the pinned frame, the same three ranges as boot 2's `poll.log`). It skips without the capture, even under `PR_ORACLE_REQUIRED`; add it to `make verify` and to the Commands list of `AGENTS.md`.
- [ ] **Step 6: A difference is a finding.** Fix the port from the bytes, with the address; never re-pin towards the port. If the capture cannot be made or the user declines it, record "loading and writing a real save has no recording of the original" as the track's main named gap in the record, `AGENTS.md` and `docs/PROGRESS.md`, and skip steps 3-5.
- [ ] **Step 7: Commit** (`tools: the save-reload capture and oracle`).

### Task 9: Gate, docs, hand-over

- [ ] **Step 1: The gate**

```bash
make verify > /tmp/claude-501/sint_verify.log 2>&1; echo EXIT=$?
```

Expected: `EXIT=0`. Compare the oracle sections with the baseline log of task 0, normalised as in track D's gates (the command is in `.superpowers/sdd/2026-10-11-reverse-d1/progress.md`, search `normalised`). Expected: identical, apart from the new `save-reload-oracle` section and the `diff-verify` counter line. Any other moved line is explained by an address in the record or it is a defect.

- [ ] **Step 2: Docs.** The record's closing section (§S.10: the counter line, the rows, the corrections, the named gaps, the audit, the gate). `AGENTS.md`: the counter bullet, the `port_progress.py` lines (`785 1203 65`, `745 745 100`, 67 unported host-owned/deferred rows), the new make target, and the "captures ran with a zero `CMOS`" fact. `README.md`: the title percentage and the counts. `docs/PROGRESS.md`: one paragraph. `docs/superpowers/specs/2026-09-19-eeprom-config-design.md` §7 gets one line pointing at this track.
- [ ] **Step 3: Commit the docs** (`docs: track S: the storage layer ported (the record's closing section, PROGRESS, AGENTS, README)`).
- [ ] **Step 4: Report to the user**: the counter before and after, the corrections with their addresses, every named gap, the audit's findings, the gate's result. Merge `reverse-sint` to `main` and push only when the user asks.

---

## Self-Review (done while writing)

- **Spec coverage:** §2 scope → tasks 3-5 and the name table; §3 → task 1; §4.1 → tasks 3-5; §4.2 → task 2; §4.3 → task 8; §4.4 → tasks 0-9 in order; §4.5 → the three unit tests and the rows; §5 risks → task 1 step 4 (a pin moves), task 4 (counters become state), task 6 step 5 (pins), task 8 step 6 (no capture); §6 → task 6 step 4.
- **Names:** the name table is the single source; tasks 3-5 and the tests use those names.
- **Not written here on purpose:** the 15 function bodies and the capture tooling's internals (task 8 step 2 designs them against the tool code and shows the user before anything runs).
