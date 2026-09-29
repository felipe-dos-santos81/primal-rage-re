# K7 + K12: the sample-start path and the omitted voice sites. Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the raw's sample path, which is the four-slot records, `0x1D0BC`'s
buffers, `0x1CC28`'s slot choice and `0x1CB18`'s start. Then wire all 218 omitted
`0x2C3FC` voice calls in ported code, with no oracle claim moving.

**Architecture:** K7 comes first, in two cycles:
- **Task 2** adds the infrastructure. It ports `0x1D0BC` over `res.c`'s existing
  `0x1C308` replacement, models the ISR clock `DS_00101500`, and fixes the AIL end
  status.
- **Task 3** is the core. It ports `0x1CC28`'s choice and `0x1CB18` behind
  `0x1CF20`, and retires the title announcer stand-in, which the raw contradicts.

K12 follows in eight batches of 21-33 wiring points each, ordered by risk:
- **Tasks 4-6:** pure state.
- **Task 7:** samples on oracle paths.
- **Tasks 8-11:** samples reached only in real play.

A port-only voice log (a test seam) and one table-driven fixture prove each site
mechanically.

**Tech Stack:** C over flat `mem[]` (SDL3 port), CMake, the `CHECK`/`CHECK_EQ_INT`
suite, Ghidra MCP (project `rage`, `/PRAGE.EXE`), Python 3 + capstone for raw
scans.

**Spec:** `docs/superpowers/plans/2026-09-29-k7-k12-derivations.md`. §0 holds
the site classification, the K7 design, the measured oracle risk and the user
decisions. The master plan is `docs/superpowers/plans/2026-09-29-all-gaps.md`
(its Global Constraints bind this plan). The ledger is
`docs/superpowers/plans/2026-09-29-all-gaps-ledger.md` (§A, §B.1 `0x1CB18`, §E
rows 5/6, §F rows 10/19). Prior K7 derivation:
`docs/superpowers/plans/2026-09-29-k4-k6-k7-derivations.md` §K7.1-§K7.5.

## Global Constraints

- **No enforced oracle claim moves.** The gate is `make verify` exit 0, with
  every oracle line equal to the baseline Task 1 saves (it must equal ledger
  §A). If a correct fix moves one, **halt** and report the old value, the new
  value and the raw evidence.
- **Never ship a fitted constant.** Every value comes from the raw bytes or a
  capture, with its address. A value that cannot be pinned is a named gap with
  its evidence.
- **On any plan-vs-raw conflict, the raw wins.** Record the correction and the
  address in the derivation record, in the task's section.
- **Address model:** Ghidra address == linear address, and `DAT_0008xxxx` =
  `DS_000xxxx`. Use Ghidra (fixups applied) for data addresses. Never write
  `mem[0xA0000]`.
- **One C function per original function**, with a header `/* 0xADDR — record
  §N */`. Mark deviations `/* PORT: ... */` and doubts `/* TODO(verify): ... */`;
  no other comment styles in `port/src`. Code addresses stored in data go
  through `fn_origin()`/`fn_resolve()`. SDL and file I/O live only in
  `host.c`/`main.c`. Never shadow original state in a C global. Do not
  reformat files owned by another module.
- **`port/src/symbols.h` is generated.** Never hand-edit it. Where the
  generator emits no name, use a local `#define` with the raw address.
- **Tests:**
  - One area file per subsystem (`test_platform.c`, `test_game.c`,
    `test_fight.c`, `test_audio.c`, `test_video.c`), plus the shared
    `test_fixtures.{h,c}`.
  - Register a test once, via `X(test_foo)` in `TEST_CASES`
    (`port/tests/test.h`).
  - Use only `CHECK`/`CHECK_EQ_INT`.
  - **Assertions must be able to fail.** Seed sentinels that differ from the
    post-condition, prove each new assertion fails under a mutation, and never
    assert an unseeded BSS-zero.
  - `game_init()` runs once per process.
- **Run from the repo root.** `data/` is read-only and git-ignored. Oracles skip
  silently without `PR_ORACLE_REQUIRED=1`. No worktree: work in place on branch
  `all-gaps`. 0 warnings, and no new dependency.
- **Commits:** style `<area>: <what changed>`. Stage named files only (never
  `git add -A`). Commit only when the user or controller has authorised it;
  "Checkpoint" steps are where the executor stops and asks.
- **Host-owned / deferred is a valid closure** only with a
  `tools/port_classification.txt` row and a derivation-record row naming the
  port's replacement. Task 2 **removes** such a row (`1D0BC`). That is a
  user-approved scope change (derivations §0.9.1).
- **Scratch:** all probes, dumps and saved outputs go under
  `.superpowers/sdd/2026-09-29-k7-k12/scratch/` (git-ignored). Below, `K` means
  that directory.
- **Audio outputs that oracles or `make audio-render` exercise stay unchanged.**
  `make audio-render` renders only the FM path (opl/mixer/seq, no samples).
  Its WAV must be byte-identical before and after Tasks 2 and 3.

## Review Focus

- **A forced eviction or a bufferless slot must never copy a sample to `mem[0]`.**
  The raw would copy to linear 0. The port refuses to start, and Task 3 test F
  asserts it with a seeded `mem[0]` sentinel.
- **A one-shot that ends in a windowed run must free its slot.** Otherwise
  `0x1CE70` reports it playing forever and the voice never replays. Task 2's
  `test_ail` additions assert status 2 after the mixer finishes a count-1
  sample, and status 4 while a count-0 sample loops.
- **The oldest-slot choice must be the raw's.** It uses an unsigned
  `min > +0x14`, scans slots 3..0, and on ties or "all at now" leaves the
  candidate at slot 0. Task 3 test E covers the strict-less choice and the
  all-equal case.
- **The attract/title sample sequence a player hears.** The attract's two
  s16title loops play, and the title's first entry stops them. Task 3 rewrites
  `main.c`'s `--check` probe to assert both. It runs in `make verify`
  (820 frames, title entry f = 690).
- **A K12 driver row that never reaches its site (a vacuous proof).**
  `tf_voice_sites` requires each row's ids to appear in the log, and every
  batch pins its row count. Each batch removes three wired calls (first,
  middle, last row) and sees the named row fail.

---

### Task 1: Verify §0, the per-site drive table, the baseline

**Files:**
- Modify: `docs/superpowers/plans/2026-09-29-k7-k12-derivations.md` (append §1)
- Create: `.superpowers/sdd/2026-09-29-k7-k12/scratch/dumps.sh` (git-ignored)
- Create: `.superpowers/sdd/2026-09-29-k7-k12/progress.md` (git-ignored SDD ledger)

**Interfaces:**
- Consumes: derivations §0 (the 218-row site table §0.4 and the 85 outside sites §0.5).
- Produces:
  - Derivations §1: the **drive table**, one row per §0.4 row, with columns
    `row | call addr (normalised) | enclosing C function | public entry | seed (mem writes) | expected ids, raw order | existing test that already reaches it`.
  - The §0.5 placement verdict.
  - `$K/dumps.sh <tag>`.
  - `$K/oracle-lines-base.txt` and `$K/checks-base.txt`.

- [ ] **Step 1: Baseline**

```bash
K=.superpowers/sdd/2026-09-29-k7-k12/scratch; mkdir -p $K
make build && make verify 2>&1 | tee $K/verify-base.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $K/verify-base.txt > $K/oracle-lines-base.txt
rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l > $K/checks-base.txt
python3 tools/port_progress.py | tee $K/progress-base.txt
rg -c 'not wired' port/src | sort > $K/notwired-base.txt
```
Expected: `EXIT=0`, and every line of `$K/oracle-lines-base.txt` appears
verbatim in ledger §A's block. If a line differs, stop and report. Another
cluster may have moved it legitimately, and the controller decides which
baseline binds.

- [ ] **Step 2: Write the dump script**

`$K/dumps.sh`:
```sh
#!/bin/sh
# usage (from the repo root): sh .superpowers/sdd/2026-09-29-k7-k12/scratch/dumps.sh <tag>
# The four frame dumps the oracles read, into $K/<tag>: --check 8000, the
# front-end det driver (run1/run2), the attract dump and the title dump.
R=$(pwd); K=$R/.superpowers/sdd/2026-09-29-k7-k12/scratch; D=$K/$1
rm -rf "$D"; mkdir -p "$D/check"
(cd "$D/check" && "$R/build/prageport" --game-dir "$R/data/game/C" --check 8000 > log.txt 2>&1) \
  || echo "dumps.sh: --check exited non-zero (see $D/check/log.txt)"
PR_FRONTEND_DET="$D/fe" PR_GAME_DIR=data/game/C ./build/run_tests > "$D/fe.log" 2>&1
PR_ATTRACT_DUMP="$D/attract" PR_GAME_DIR=data/game/C ./build/run_tests > "$D/attract.log" 2>&1
PR_TITLE_DUMP="$D/title" PR_GAME_DIR=data/game/C ./build/run_tests > "$D/title.log" 2>&1
echo "dumps.sh: $D done"
```
and its comparator, `$K/dumpcmp.sh`:
```sh
#!/bin/sh
# usage: dumpcmp.sh <before-tag> <after-tag>; prints nothing and exits 0 when identical
K=$(pwd)/.superpowers/sdd/2026-09-29-k7-k12/scratch
diff -rq "$K/$1/check/frames" "$K/$2/check/frames" && \
diff -rq "$K/$1/fe/run1" "$K/$2/fe/run1" && diff -rq "$K/$1/fe/run2" "$K/$2/fe/run2" && \
diff -rq "$K/$1/attract" "$K/$2/attract" && diff -rq "$K/$1/title" "$K/$2/title"
```
Run `sh $K/dumps.sh base && sh $K/dumpcmp.sh base base; echo CMP=$?`.
Expected: `CMP=0`, `ls $K/base/check/frames | wc -l` = 24000, and
`ls $K/base/fe/run1 | wc -l` = 1384. Those counts were measured at `e57d344`.
If they differ, record the new values in §1 and use them.

- [ ] **Step 3: Verify §0 through Ghidra**

For each K7 body, read the raw through the Ghidra MCP (`/PRAGE.EXE`) and confirm
§0.7's instruction citations: `0x1D0BC`, `0x1CC28`, `0x1CB18`, `0x1CF20`,
`0x500BB`, and the ISR `0x1BDF4..0x1BE2F`. Then confirm the voice-table values
§0.1 uses, with Ghidra's `read_memory` at `0xBBDC8`. Spot-check these rows: ids
`0x40`, `0x41`, `0x42`, `0x43`, `0x3A`, `0xCD`, `0x100`→0 and `0x53`. Next,
confirm the word tables in §0.2: `0xC888A`, `0xBDFFA`, `0xBDAA8`, `0xE9308`,
`0xE933C`, `0xBE008`, `0xE9358`, `0xC75AA` and `0xBDAD4`. Also confirm that the
title's two calls are at `0x121CE` (id `0x41`) and `0x121D8` (id `0x43`), and
the attract's at `0x11160` (`0x40`) and `0x1116C` (`0x42`). Record each check
under §1 with its address. A mismatch is a raw-wins correction: record it, and
fix §0 in the same edit.

- [ ] **Step 4: Normalise the site table and place the 85 outside sites**

For every §0.4 row, record the `call`/`jmp` address the comment's `mov` leads
to. For example, `0x1F526` leads to `0x1F52B`, `0x4EA01` leads to `0x4EB72`,
and `0x1F580`/`0x1F58E` share `0x1F593`. Then, for each of the 85 §0.5 sites,
use Ghidra's `get_function_containing` or the nearest non-Ghidra entry to name
the containing code, and check whether `port/src` ports it (a `/* 0xADDR`
header, or a `fn_register`). Verdicts:
- **outside**: the containing code is unported. List the site with its
  containing entry. It is wired when that code is ported.
- **silent**: the code is ported but the call is dropped with no comment. Add
  the site to batch D4's rows (Task 11) if ≤ 10 are found. Otherwise add
  "Task 11b" rows and tell the controller.

- [ ] **Step 5: The drive table (§1.3)**

For each §0.4 row, read the enclosing port function and every public caller
(`rg -n '<fn>\(' port/src port/tests`), and write one drive-table row:
- the public entry that reaches the call (for example, `game_state_step()` with
  `DSW(DS_000F0A64) = 6` for `game_state_6`'s `0x11A94`);
- the minimal seed that takes the path (the port's own branch conditions, each
  with its raw address);
- the expected id sequence, in raw call order, on that path. Include every
  §0.4 id on the path. For table ids, fix the index the seed selects;
- an existing test that already drives that path, if any.

Rows whose site is on the same path share one driver. For example,
`game_coin_divert(1)` covers rows 139 and 140 with ids `0x100`, `0x53`. Where
no public entry exists short of `game_frame()`, the entry is `game_frame()`
with the mode word `DS_00104B00` seeded. Where the path needs a fight context,
name the `test_fixtures.c` fixture that builds it (`rg -n '^void tf_'
port/tests/test_fixtures.c`).

- [ ] **Step 6: Checkpoint**

Append §1 (with §1.1 verification, §1.2 placement and §1.3 drive table) to the
derivations. Write the SDD ledger's first entry. Stop and report to the
controller:
- the three §0.9 decisions, still unanswered;
- any raw-wins correction;
- the §0.5 verdict counts.

If authorised, the controller commits `docs: k7-k12 derivation §1 (drive
table, placement)`.

---

### Task 2: K7 infrastructure: `0x1D0BC`, the ISR clock, the AIL end status

**Files:**
- Modify: `port/src/platform/res.c`, `port/src/platform/res.h` (export `res_block_alloc`; the stall uses `game_isr_ticks`)
- Modify: `port/src/game/flow.c` (`sound_buffers_alloc` = `0x1D0BC`, `game_isr_ticks`, `game_init`'s `0x1C0B1` call, the spin, `game_audio_init`'s comment)
- Modify: `port/src/game/flow.h` (declarations)
- Modify: `port/src/platform/audio/mixer.c`, `mixer.h` (`mixer_sample_active`), `port/src/platform/audio/ail.c` (`AIL_sample_status`)
- Modify: `tools/port_classification.txt` (delete the line `1D0BC host-owned record-§50-D`)
- Test: `port/tests/test_game.c` (`check_sound_buffers`, called in `test_flow`), `port/tests/test_platform.c` (res stall), `port/tests/test_audio.c` (`test_ail`)

**Interfaces:**
- Consumes: `res_alloc` (static in `res.c`, "PORT: replaces FUN_0001C308").
- Produces:
  - `u32 res_block_alloc(u32 size);` (res.h): the port's `0x1C308`, returning a
    `mem[]` offset or 0.
  - `u32 sound_buffers_alloc(void);` (flow.h): `0x1D0BC`, returning AL.
  - `void game_isr_ticks(u32 n);` (flow.h).
  - `int mixer_sample_active(const void *owner);` (mixer.h).

- [ ] **Step 0: Before-dumps and the WAV**

```bash
K=.superpowers/sdd/2026-09-29-k7-k12/scratch
sh $K/dumps.sh before-t2
make audio-render AUDIO_WAV=$(pwd)/$K/before-t2.wav
```

- [ ] **Step 1: Write the failing tests**

In `port/tests/test_game.c`, add this before `int test_flow(void)`:

```c
/* Record §K7 (2026-09-29-k7-k12-derivations.md §0.7.1, Task 2 §2): 0x1D0BC.
 * Every asserted post-value differs from its seed. */
static void check_sound_buffers(void)
{
    u32 s[4], i;
    const u32 s_c0 = DSD(DS_001028C0), s_c4 = DSD(DS_001028C4);
    const u32 s_d0 = DSD(DS_001028D0), s_c8 = DSD(DS_001028C8);
    const u8 s_b0 = DSB(DS_000A2CB0);
    for (i = 0; i < 4u; i++) s[i] = DSD(DS_00102870 + i * 0x18u);

    /* Already run (DS_000A2CB0 set, 0x1D0BF): AL = 0, nothing allocated. */
    DSB(DS_000A2CB0) = 1u;
    DSD(DS_001028C8) = 1u;
    for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = 0;
    CHECK_EQ_INT((int)sound_buffers_alloc(), 0);
    CHECK_EQ_INT((int)DSD(DS_00102870), 0);

    /* First run, DIG set, no sequence: the MIDI arm is skipped (C0 = 0,
     * 0x1D0D5) and slots 0..3 get 0x8C00, 0x6000, 0x6000, 0x6000 in order
     * (0x1D14D/0x1D154, the bump allocator returns consecutive blocks). */
    DSB(DS_000A2CB0) = 0;
    DSD(DS_001028C0) = 0;
    DSD(DS_001028C4) = 0;
    DSD(DS_001028D0) = 0xD0D0D0D0u;
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK_EQ_INT((int)DSB(DS_000A2CB0), 1);
    CHECK_EQ_INT((int)DSD(DS_001028C8), 1);
    CHECK_EQ_INT((int)DSD(DS_001028D0), (int)0xD0D0D0D0u);
    CHECK(DSD(DS_00102870) != 0u, "0x1D163 gives slot 0 a buffer");
    CHECK_EQ_INT((int)(DSD(DS_00102870 + 0x18u) - DSD(DS_00102870)), 0x8C00);
    CHECK_EQ_INT((int)(DSD(DS_00102870 + 0x30u) - DSD(DS_00102870 + 0x18u)), 0x6000);
    CHECK_EQ_INT((int)(DSD(DS_00102870 + 0x48u) - DSD(DS_00102870 + 0x30u)), 0x6000);

    /* The MIDI arm (0x1D0CC..0x1D10C): a sequence handle and no buffer yet
     * allocate 0x5100 bytes into DS_001028D0; no DIG, so the slots keep their
     * sentinels (0x1D132). */
    DSB(DS_000A2CB0) = 0;
    DSD(DS_001028C0) = 0x1234u;
    DSD(DS_001028C4) = 0x5678u;
    DSD(DS_001028D0) = 0;
    DSD(DS_001028C8) = 0;
    for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = 0x0BADu;
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK(DSD(DS_001028D0) != 0u, "0x1D0F7 stores the MIDI buffer");
    CHECK_EQ_INT((int)DSD(DS_001028C0), 0x1234);
    CHECK_EQ_INT((int)DSD(DS_00102870), 0x0BAD);

    /* Slot 0's buffer already set at entry: the loop is skipped (0x1D147),
     * ecx stays 0 and 0x1D195 turns the DIG driver off, as the raw does. */
    DSB(DS_000A2CB0) = 0;
    DSD(DS_001028C0) = 0;
    DSD(DS_001028C4) = 0;
    DSD(DS_001028C8) = 1u;
    DSD(DS_00102870) = 0x0BADu;
    DSD(DS_00102870 + 0x18u) = 0;
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK_EQ_INT((int)DSD(DS_001028C8), 0);
    CHECK_EQ_INT((int)DSD(DS_00102870 + 0x18u), 0);

    /* The ISR's counter pair (PORT, 0x1BE0E..0x1BE16). */
    DSD(DS_00101508) = 0x10u;
    DSD(DS_00101500) = 0x2000u;
    game_isr_ticks(3u);
    CHECK_EQ_INT((int)DSD(DS_00101508), 0x13);
    CHECK_EQ_INT((int)DSD(DS_00101500), 0x2003);

    DSD(DS_001028C0) = s_c0; DSD(DS_001028C4) = s_c4;
    DSD(DS_001028D0) = s_d0; DSD(DS_001028C8) = s_c8;
    DSB(DS_000A2CB0) = s_b0;
    for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = s[i];
}
```
Call it in `test_flow` on the line after `CHECK_EQ_INT((int)DSD(DS_001028C8), 1);`
(the one that follows `game_audio_init();`):
```c
    check_sound_buffers();
```
In `port/tests/test_platform.c`'s res stall block, seed `DSD(DS_00101500) =
0x9ABCu;` next to `DSD(DS_00101508) = 0x5678;`, and add this after the
`DS_00101508` delta assertion:
```c
        CHECK_EQ_INT((int)DSD(DS_00101500),
                     0x9ABC + (int)((res_size(0u) + RES_READ_BYTES_PER_TICK - 1u)
                                    / RES_READ_BYTES_PER_TICK));   /* the ISR's 0x1BE16 pair */
```
In `port/tests/test_audio.c`'s `test_ail`, add a line after
`CHECK_EQ_INT(ail_out[2], 0);` (the count-1 case):
```c
        CHECK_EQ_INT(AIL_sample_status(hs[0]), 2);   /* 0x6F28F: the DIG service ended it */
```
and one after the count-0 case's `CHECK_EQ_INT(ail_out[2], 18432);`:
```c
        CHECK_EQ_INT(AIL_sample_status(hs[0]), 4);   /* count 0 still loops */
```

- [ ] **Step 2: Run them to see them fail**

Run: `cmake --build build 2>&1 | tail -3`
Expected: a compile error, because `sound_buffers_alloc` and `game_isr_ticks`
are undeclared. After Step 3's declarations alone (bodies not yet written), a
link error appears instead. Either counts as failing.

- [ ] **Step 3: Implement**

`port/src/platform/res.c`: add this after `res_alloc`:
```c
/* PORT: the port's 0x1C308 (the paged allocator, host-owned record-§50-D)
 * exported for 0x1D0BC: a mem[] offset, or 0 when the block does not fit. */
u32 res_block_alloc(u32 size) { return res_alloc(size); }
```
and replace the stall's two lines in `res_load_present`:
```c
        u32 size = res_size(index);
        game_isr_ticks((size + RES_READ_BYTES_PER_TICK - 1u) / RES_READ_BYTES_PER_TICK);
        DSD(DS_0010150C) = DSD(DS_00101508);   /* 0x1B45F/0x1B464 */
```
`port/src/platform/res.h`: add `u32 res_block_alloc(u32 size);` with a
one-line comment.

`port/src/game/flow.h`: add the following next to `game_audio_init`:
```c
/* 0x1D0BC (record k7-k12 §0.7.1): the MIDI buffer and the four sample-slot
 * buffers (+0x10), once. Returns AL (0 when DS_000A2CB0 was already set). */
u32 sound_buffers_alloc(void);
/* PORT: the timer ISR 0x1BDF4's counter pair, n ticks (0x1BE0E..0x1BE16). */
void game_isr_ticks(u32 n);
```
`port/src/game/flow.c`: add the following next to `game_loop_begin`:
```c
/* PORT: the timer ISR 0x1BDF4's counter pair, n ticks: DS_00101508 and
 * DS_00101500 (0x1BE0E..0x1BE16); 0x500BB reads the latter as the time
 * (record k7-k12 §0.7.2). The master loop's spin and the read stall (res.c)
 * are where the port models the ISR's ticks; config.c's key-wait loop models
 * them itself. The ISR's DS_00104B22 gate is not modelled (todo-verify
 * record §1). */
void game_isr_ticks(u32 n)
{
    DSD(DS_00101508) += n;                                 /* 0x1BE0E/0x1BE10 */
    DSD(DS_00101500) += n;                                 /* 0x1BE0F/0x1BE16 */
}
```
In `game_loop`'s spin, replace `DSD(DS_00101508)++;` with `game_isr_ticks(1u);`.

Add `0x1D0BC` next to `sound_music_volume` in the sound module:
```c
/* 0x1D0BC — record k7-k12 §0.7.1. Once (DS_000A2CB0, 0x1D0BF/0x1D19D): with
 * a sequence and MDI handle and no MIDI buffer, 0x5100 bytes into
 * DS_001028D0, zeroed (a failure drops C4/C0/CC); with a DIG driver and slot
 * 0 unallocated, slot i's +0x10 = 0x8C00 (i = 0) or 0x6000 bytes until a
 * failure or an allocated slot; slot 0 empty turns the DIG driver off. The
 * allocator 0x1C308 is res_block_alloc. PORT: the two 0x62734 messages (the
 * runtime's printf) are not printed. */
u32 sound_buffers_alloc(void)
{
    if (DSB(DS_000A2CB0) != 0u) return 0;                  /* 0x1D0BF/0x1D1A9 */
    if (DSD(DS_001028C4) != 0u && DSD(DS_001028C0) != 0u
        && DSD(DS_001028D0) == 0u) {                       /* 0x1D0CC..0x1D0E6 */
        u32 b = res_block_alloc(0x5100u);                  /* 0x1D0E8..0x1D0F2 0x1C308 */
        DSD(DS_001028D0) = b;                              /* 0x1D0F7 */
        if (b != 0u) {
            memset(mem + b, 0, 0x5100u);                   /* 0x1D100..0x1D107 0x61A70 */
        } else {
            DSD(DS_001028C4) = 0;                          /* 0x1D113 */
            DSD(DS_001028C0) = 0;                          /* 0x1D11E */
            DSD(DS_001028CC) = 0;                          /* 0x1D124 */
        }
    }
    if (DSD(DS_001028C8) != 0u) {                          /* 0x1D132 */
        u32 i = 0;                                         /* 0x1D141 */
        if (DSD(DS_00102870) == 0u) {                      /* 0x1D13B..0x1D147 */
            do {
                u32 b = res_block_alloc(i == 0u ? 0x8C00u : 0x6000u);   /* 0x1D149..0x1D15E */
                DSD(DS_00102870 + i * SND_SLOT_STRIDE) = b;             /* 0x1D163 */
                if (b == 0u) break;                        /* 0x1D16B */
                i++;                                       /* 0x1D16D */
            } while (i < 4u && DSD(DS_00102870 + i * SND_SLOT_STRIDE) == 0u);   /* 0x1D171..0x1D17D */
        }
        if (i == 0u) DSD(DS_001028C8) = 0;                 /* 0x1D17F..0x1D195 */
    }
    DSB(DS_000A2CB0) = 1u;                                 /* 0x1D19B/0x1D19D */
    return 1;
}
```
In `game_init`, replace the four-line comment that starts `/* PORT: 0x1D0BC
allocates the MIDI sequence buffer and the four sample` with:
```c
    (void)sound_buffers_alloc();  /* 0x1C0B1 0x1D0BC (record k7-k12 §0.7.1) */
```
In `game_audio_init`'s `DS_001028C8` comment, replace the sentence `0x1D0BC,
which zeroes it when no sample buffer can be allocated, is not ported (see
game_init).` with `0x1D0BC (sound_buffers_alloc, which game_init calls later,
as the raw's 0x1C0B1 does) zeroes it when slot 0 gets no buffer.`

`port/src/platform/audio/mixer.c`/`.h`: add
```c
/* 1 when an active voice belongs to `owner` (an AIL sample handle). */
int mixer_sample_active(const void *owner)
{
    for (int i = 0; i < MIXER_VOICES; i++)
        if (g_voices[i].active && g_voices[i].owner == owner) return 1;
    return 0;
}
```
`port/src/platform/audio/ail.c`, `AIL_sample_status`:
```c
    if (sample == NULL || !sample->used)
        return 0;
    /* Record k7-k12 §0.7.6: the DIG service 0x6F120 marks a sample done at
     * its buffer end (0x6F28F). PORT: the mixer owns the voice, so a
     * playing handle with no live voice has ended. With no device the mixer
     * is not rendered and a started sample stays 4 (named gap, §0.7.6). */
    if (sample->state == 4 && !mixer_sample_active(sample))
        sample->state = 2;
    return sample->state;
```
`tools/port_classification.txt`: delete the line `1D0BC host-owned record-§50-D`.

- [ ] **Step 4: Run the suite**

Run: `cmake --build build 2>&1 | grep -E 'warning|error'; PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -3`
Expected: no warnings and `all checks passed`.

- [ ] **Step 5: Mutation proof**

Apply each mutation alone, rebuild, run the suite, and expect the named check
to fail. Then restore.
- (a) `0x8C00u` becomes `0x6000u` in `sound_buffers_alloc`: the `0x8C00`
  spacing check fails.
- (b) Delete `if (i == 0u) DSD(DS_001028C8) = 0;`: the "slot 0 set at entry"
  check fails.
- (c) Delete `DSD(DS_00101500) += n;`: the `0x2003` check and the res-stall
  `0x9ABC` check fail.
- (d) Delete the `mixer_sample_active` test in `AIL_sample_status`: `test_ail`'s
  status-2 check fails.

- [ ] **Step 6: Gate**

```bash
make verify 2>&1 | tee $K/verify-t2.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $K/verify-t2.txt | diff $K/oracle-lines-base.txt - && echo ORACLES-EQUAL
sh $K/dumps.sh after-t2 && sh $K/dumpcmp.sh before-t2 after-t2 && echo DUMPS-IDENTICAL
make audio-render AUDIO_WAV=$(pwd)/$K/after-t2.wav && cmp $K/before-t2.wav $K/after-t2.wav && echo WAV-IDENTICAL
python3 tools/port_progress.py
grep -c '/\* 0x1D0BC' port/src/game/flow.c
```
Expected: `EXIT=0`, `ORACLES-EQUAL`, `DUMPS-IDENTICAL`, `WAV-IDENTICAL`, and a
header count of `1`. `port_progress` shows ported +1, and the portable
denominator +1, against the Task-1 value. At `e57d344` that is `763 1203 63`
and `727 732 99`.

- [ ] **Step 7: Docs and checkpoint**

Append §2 to the derivations: the body, the four test vectors, the mutation
results and the gate outputs. Record that the `1D0BC` row was removed (user
decision §0.9.1). If authorised, commit `audio: port 0x1D0BC, the ISR clock
pair and the AIL end status (record k7-k12 §2)`, staging `port/src/platform/res.c
port/src/platform/res.h port/src/game/flow.c port/src/game/flow.h
port/src/platform/audio/mixer.c port/src/platform/audio/mixer.h
port/src/platform/audio/ail.c tools/port_classification.txt
port/tests/test_game.c port/tests/test_platform.c port/tests/test_audio.c
docs/superpowers/plans/2026-09-29-k7-k12-derivations.md`.

---

### Task 3: K7 core. `0x1CC28`'s choice, `0x1CB18`, `0x1CF20`, and the stand-in retired

**Files:**
- Modify: `port/src/game/flow.c`: `snd_sample_queue`, the new
  `sound_sample_start`, and `game_audio_service`. Delete `game_sample_play`,
  `game_sample_request`, `s_pending_sample`, `s_sample_request`,
  `SND_ANNOUNCER_ID`, `SOUND_RES` and the `samples.h` include if unused. Also
  change `game_state_title`'s first entry and rewrite the sound-module comment.
- Modify: `port/src/game/flow.h` (declare `sound_sample_start`)
- Modify: `port/src/game/attract.c` (rows 11/12: `0x11160` voice `0x40`, `0x1116C` voice `0x42`)
- Modify: `port/src/main.c` (the `--check` probe)
- Test: `port/tests/test_game.c` (`check_sample_slots`, the Task-12 block in `test_flow`, and `check_sound_voice` section E)

**Interfaces:**
- Consumes: `sound_buffers_alloc`, `game_isr_ticks` and `mixer_sample_active` (Task 2).
- Produces: `void sound_sample_start(u32 slot);` (flow.h): `0x1CB18`, slot 0..3.

- [ ] **Step 0: Before-dumps and the WAV**

```bash
sh $K/dumps.sh before-t3
make audio-render AUDIO_WAV=$(pwd)/$K/before-t3.wav
```

- [ ] **Step 1: Write the failing tests**

In `port/tests/test_game.c`, add these after `check_sound_voice`:
```c
/* Record §K7 (k7-k12 derivations §0.7.3/§0.7.4, Task 3 §3): 0x1CC28's slot
 * choice and 0x1CB18's start on the live handles and 0x1D0BC's buffers.
 * 0x42 = 0x03837440 (0x1D85 bytes, loop byte 1), 0x40 = 0x0383B6F4 (0x5FAE,
 * loop byte 1), 0x3A = 0x03022554 (0x8320 > 0x6000, loop byte 0). */
static void ss_seed(void)
{
    DSD(DS_001028C8) = 1u;
    DSB(DS_001028DB) = 0;
    for (u32 i = 0; i < 4u; i++) {
        DSD(DS_00102864 + i * 0x18u) = 0;
        DSB(DS_00102868 + i * 0x18u) = 0x77u;
        DSD(DS_0010286C + i * 0x18u) = 0;
        DSD(DS_00102874 + i * 0x18u) = 0x10u + i;
        AIL_init_sample(sound_slot_handle(i));
    }
    mixer_stop_samples();
    DSD(DS_00101500) = 0x100u;
}

static void check_sample_slots(void)
{
    static u8 ss_ap[320u * 200u], ss_dac[256][3];
    static s16 ss_buf[4096 * 2];
    u32 i;
    memcpy(ss_ap, gfx_aperture(), sizeof ss_ap);
    memcpy(ss_dac, gfx_dac, sizeof ss_dac);
    for (i = 0; i < 4u; i++)
        CHECK(DSD(DS_00102870 + i * 0x18u) != 0u, "0x1D0BC gave every slot a buffer");

    /* A: size <= 0x6000 takes the first free slot from 3 down (0x1CCC3). */
    ss_seed();
    CHECK_EQ_INT((int)sound_voice(0x42u), 1);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0x03837440);
    CHECK_EQ_INT((int)DSB(DS_00102868 + 3u * 0x18u), 1);
    CHECK_EQ_INT((int)DSD(DS_00102874 + 3u * 0x18u), 0x100);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 2u * 0x18u), 0);
    CHECK_EQ_INT((int)sound_voice(0x40u), 1);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 2u * 0x18u), 0x0383B6F4);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 1u * 0x18u), 0);

    /* B: 0x1CF20 -> 0x1CB18 per slot: copy, start, +0x0C = +0x04, +0x04 = 0. */
    game_audio_service();
    CHECK_EQ_INT((int)DSD(DS_0010286C + 3u * 0x18u), 0x03837440);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0);
    CHECK_EQ_INT((int)DSD(DS_0010286C + 2u * 0x18u), 0x0383B6F4);
    CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle(3u)), 4);
    CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle(0u)), 2);
    {
        const u8 *p = (const u8 *)res_resolve(0x03837440u);
        CHECK(p != NULL && memcmp(mem + DSD(DS_00102870 + 3u * 0x18u), p + 4, 0x1D85u) == 0,
              "0x1CB18 copied the sample into the slot's buffer");
    }
    CHECK_EQ_INT(mixer_active_voices(), 2);
    for (i = 0; i < 24u; i++) mixer_render(ss_buf, 4096, MIXER_OPL_RATE);
    CHECK_EQ_INT(mixer_active_voices(), 2);                 /* loop byte 1: count 0 */

    /* C: 0x41 stops 0x40's handle (0x2C7A5 -> 0x1CE04), 0x43 the other. */
    CHECK_EQ_INT((int)sound_voice(0x41u), 1);
    CHECK_EQ_INT((int)DSD(DS_0010286C + 2u * 0x18u), 0);
    CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle(2u)), 2);
    CHECK_EQ_INT(mixer_active_voices(), 1);
    CHECK_EQ_INT((int)sound_voice(0x43u), 1);
    CHECK_EQ_INT(mixer_active_voices(), 0);

    /* D: size > 0x6000 only on slot 0 (0x1CC68); slot 0 busy forces it. */
    ss_seed();
    CHECK_EQ_INT((int)sound_voice(0x3Au), 1);
    CHECK_EQ_INT((int)DSD(DS_00102864), 0x03022554);
    CHECK_EQ_INT((int)DSB(DS_00102868), 0);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0);
    DSD(DS_00101500) = 0x180u;
    CHECK_EQ_INT((int)sound_voice(0x3Au), 1);
    CHECK_EQ_INT((int)DSD(DS_00102874), 0x180);

    /* E: none free: the smallest +0x14 below now (0x1CD22 `jbe`, unsigned),
     * scanned 3..0; all at now leaves the candidate at slot 0 (0x1CC64). */
    ss_seed();
    {
        static const u32 t[4] = { 0x90u, 0x50u, 0x70u, 0x60u };
        for (i = 0; i < 4u; i++) {
            DSD(DS_00102864 + i * 0x18u) = 0x44440000u + i;
            DSD(DS_00102874 + i * 0x18u) = t[i];
        }
    }
    CHECK_EQ_INT((int)sound_voice(0x42u), 1);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 1u * 0x18u), 0x03837440);
    CHECK_EQ_INT((int)DSD(DS_00102874 + 1u * 0x18u), 0x100);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0x44440003);
    CHECK_EQ_INT((int)DSD(DS_00102864), 0x44440000);
    ss_seed();
    for (i = 0; i < 4u; i++) {
        DSD(DS_00102864 + i * 0x18u) = 0x44440000u + i;
        DSD(DS_00102874 + i * 0x18u) = 0x100u;
    }
    CHECK_EQ_INT((int)sound_voice(0x42u), 1);
    CHECK_EQ_INT((int)DSD(DS_00102864), 0x03837440);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0x44440003);

    /* F: PORT guard: a bufferless slot is not started (the raw would copy
     * to linear 0). mem[0] carries a sentinel the sample bytes differ from. */
    ss_seed();
    {
        u32 b3 = DSD(DS_00102870 + 3u * 0x18u), m0 = DSD(0u);
        DSD(0u) = 0x5A5A5A5Au;
        DSD(DS_00102870 + 3u * 0x18u) = 0;
        DSD(DS_00102864 + 3u * 0x18u) = 0x03837440u;
        sound_sample_start(3u);
        CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0x03837440);
        CHECK_EQ_INT((int)DSD(DS_0010286C + 3u * 0x18u), 0);
        CHECK_EQ_INT((int)DSD(0u), 0x5A5A5A5A);
        DSD(DS_00102870 + 3u * 0x18u) = b3;
        DSD(0u) = m0;
    }
    ss_seed();
    memcpy(gfx_aperture(), ss_ap, sizeof ss_ap);
    memcpy(gfx_dac, ss_dac, sizeof ss_dac);
}
```
In `test_flow`, replace the block from `/* Task 12: the title state queued the
announcer sample` up to its `host_wait_vblank(); game_audio_service();` and
the two announcer `CHECK`s with the following (keep the 40-service loop and
the two music `CHECK`s that follow unchanged):
```c
    /* Record §K7 (k7-k12 derivations §0.7.5): the raw's title plays no
     * sample. The sample path is proved end to end on the attract's looping
     * 0x40 (s16title 0x0383B6F4, loop byte 1): 0x1D0BC's buffers, 0x1CC28's
     * queue, 0x1CF20 -> 0x1CB18's start. The title bank's first note is at
     * XMIDI tick 59, so this single-tick render is before the FM sounds and
     * any non-silence is the sample's. */
    DSB(DS_000A2CB0) = 0;
    for (u32 k = 0; k < 4u; k++) DSD(DS_00102870 + k * 0x18u) = 0;
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK_EQ_INT((int)sound_voice(0x40u), 1);
    host_wait_vblank();
    game_audio_service();
    CHECK(mixer_active_voices() > 0, "the queued 0x40 became an active mixer voice");
    {
        static s16 abuf[4096 * 2];
        mixer_render(abuf, 4096, MIXER_OPL_RATE);
        int nz = 0;
        for (int i = 0; i < 4096 * 2; i++) if (abuf[i]) { nz = 1; break; }
        CHECK(nz, "mixer rendered non-silence with the 0x40 sample active");
    }
```
Then replace the block from `/* The announcer is sound id 0xCD` up to its
`CHECK_EQ_INT(mixer_active_voices(), 0);\n    }` with:
```c
    /* 0x40's record: case 2, handle 0x0383B6F4, loop byte 1, so 0x1CB18 sets
     * loop count 0 and the voice outlives ~98k output frames; 0x41 (case 5,
     * 0x2C7A5) stops it. */
    CHECK_EQ_INT((int)DSD(DS_000BBDC8 + 0x40u * 12u + 4u), 0x0383B6F4);
    {
        static s16 abuf2[4096 * 2];
        for (int i = 0; i < 24; i++) mixer_render(abuf2, 4096, MIXER_OPL_RATE);
        CHECK_EQ_INT(mixer_active_voices(), 1);
        CHECK_EQ_INT((int)sound_voice(0x41u), 1);
        CHECK_EQ_INT(mixer_active_voices(), 0);
    }
    check_sample_slots();
```
In `check_sound_voice` section E, replace the loop that asserts every slot's
`+0x04`/`+0x0C` is 0 after `sound_voice(3u)` with:
```c
    /* 0x1CD9C cleared every slot, then 0x1CC28 queued 0x180122FD (0x79C0
     * bytes > 0x6000, so only slot 0; free after the stop) (record k7-k12 §3). */
    CHECK_EQ_INT((int)DSD(DS_00102864), 0x180122FD);
    for (i = 0; i < 4u; i++) {
        if (i != 0u) CHECK_EQ_INT((int)DSD(DS_00102864 + i * 0x18u), 0);
        CHECK_EQ_INT((int)DSD(DS_0010286C + i * 0x18u), 0);
    }
```

- [ ] **Step 2: Run them to see them fail**

Run: `cmake --build build 2>&1 | tail -3`
Expected: a compile error, because `sound_sample_start` is undeclared. With
only a declaration stub added, the suite fails in `check_sample_slots` A (slot
3's `+0x04` is 0) and in section E.

- [ ] **Step 3: Implement `0x1CC28`'s choice**

Replace the header and body of `snd_sample_queue` with:
```c
/* 0x1CC28 — record k7-k12 §0.7.3. EAX = the resource handle of a sample, DL
 * = its loop byte. Without a DIG driver (DS_001028C8) or while samples are
 * paused (DS_001028DB) AL = 0 and nothing is read. Otherwise the time (0x500BB)
 * and the resolve (0x1B544; a bank's first read draws the loader, record
 * §45-A); a slot is free with a buffer (+0x10), nothing queued (+0x04) and a
 * 0x5DD03 status other than 4. Above 0x6000 bytes only slot 0; otherwise the
 * first free of slots 3..0, else the one whose queue time +0x14 is the
 * smallest below now (unsigned; slot 0 when none is). A forced slot is ended
 * and re-inited. Queueing stores +0x04, +0x08 and +0x14; AL = 1. */
static u32 snd_sample_queue(u32 h, u32 loop)
{
    if (DSD(DS_001028C8) == 0u) return 0;                  /* 0x1CC37 */
    if (DSB(DS_001028DB) != 0u) return 0;                  /* 0x1CC44 */
    u32 now = DSD(DS_00101500);                            /* 0x1CC51 0x500BB */
    const u8 *p = (const u8 *)res_resolve(h);              /* 0x1CC5D 0x1B544 */
    /* PORT: the raw dereferences the resolve unconditionally; a handle past
     * the loaded INDEX (unit fixtures) resolves to NULL and queues nothing. */
    if (p == NULL) return 1;
    u32 size = DSD((u32)(p - mem));                        /* 0x1CC62 */
    u32 cand = 0;                                          /* 0x1CC5B/0x1CC64 */
    if (size > 0x6000u) {                                  /* 0x1CC68 `jbe` */
        if (DSD(DS_00102870) != 0u && DSD(DS_00102864) == 0u
            && snd_slot_status(0u) != 4) {                 /* 0x1CC70..0x1CC94 */
            DSD(DS_00102864) = h;                          /* 0x1CC99 */
            DSB(DS_00102868) = (u8)loop;                   /* 0x1CCA2 */
            DSD(DS_00102874) = DSD(DS_00101500);           /* 0x1CCA7/0x1CCAC 0x500BB */
            return 1;                                      /* 0x1CCB1 */
        }
    } else {
        u32 min = now;
        for (u32 k = 4u; k-- > 0u;) {                      /* 0x1CCC3..0x1CD32 */
            u32 off = k * SND_SLOT_STRIDE;
            if (DSD(DS_00102870 + off) != 0u && DSD(DS_00102864 + off) == 0u
                && snd_slot_status(off) != 4) {            /* 0x1CCCD..0x1CCF1 */
                DSD(DS_00102864 + off) = h;                /* 0x1CCF6 */
                DSB(DS_00102868 + off) = (u8)loop;         /* 0x1CD00 */
                DSD(DS_00102874 + off) = DSD(DS_00101500); /* 0x1CD06/0x1CD0B 0x500BB */
                return 1;                                  /* 0x1CD11 */
            }
            u32 t = DSD(DS_00102874 + off);                /* 0x1CD1C */
            if (min > t) { cand = k; min = t; }            /* 0x1CD22 `jbe`, 0x1CD26/0x1CD2A */
        }
    }
    u32 off = cand * SND_SLOT_STRIDE;                      /* 0x1CD34..0x1CD41 */
    AIL_stop_sample(s_samples[cand]);                      /* 0x1CD4F 0x5DC8B */
    AIL_init_sample(s_samples[cand]);                      /* 0x1CD5E 0x5DC0F */
    DSD(DS_00102864 + off) = h;                            /* 0x1CD69 */
    DSB(DS_00102868 + off) = (u8)loop;                     /* 0x1CD73 */
    DSD(DS_00102874 + off) = DSD(DS_00101500);             /* 0x1CD79/0x1CD7E 0x500BB */
    return 1;                                              /* 0x1CD84 */
}
```
Remove `(void)loop;`, the old `PORT:` slot-choice paragraph and the old body.
`snd_sample_queue` is defined above `sound_voice`, and `snd_slot_status` above
it. Keep that order.

- [ ] **Step 4: Implement `0x1CB18` and the `0x1CF20` loop, and retire the stand-in**

Replace `game_sample_play` (the whole function and its `FUN_0001cb18` comment)
with the function below. Place it after `snd_sample_queue`, and forward-declare
it above `game_audio_service` if needed (`void sound_sample_start(u32 slot);`
is in flow.h):
```c
/* 0x1CB18 — record k7-k12 §0.7.4. EAX = the slot (0x1CF20 calls it for 0..3).
 * With a queued handle (+0x04): resolve it, copy its [size] bytes from +4
 * into the slot's buffer (+0x10), set the AIL sample up (init, address, the
 * SFX volume DS_000A2CB4, 11025 Hz, 8-bit mono, loop count 0 when the loop
 * byte +0x08 is 1) and start it; +0x0C = the handle, +0x04 = 0. */
void sound_sample_start(u32 slot)
{
    u32 off = slot * SND_SLOT_STRIDE;                      /* 0x1CB25..0x1CB2E */
    u32 h = DSD(DS_00102864 + off);                        /* 0x1CB31 */
    if (h == 0u) return;                                   /* 0x1CB37/0x1CB39 */
    const u8 *p = (const u8 *)res_resolve(h);              /* 0x1CB41 0x1B544 */
    u32 buf = DSD(DS_00102870 + off);                      /* 0x1CB4B */
    /* PORT: a slot without a buffer (0x1D0BC not run, or its allocation
     * failed) is not started, and a NULL resolve (unit fixtures) neither:
     * the raw would copy to linear 0 / dereference it. */
    if (buf == 0u || p == NULL) return;
    u32 size = DSD((u32)(p - mem));                        /* 0x1CB49 */
    memcpy(mem + buf, p + 4, size);                        /* 0x1CB51..0x1CB61 rep movsd/movsb */
    HSAMPLE s = s_samples[slot];
    AIL_init_sample(s);                                    /* 0x1CB6B 0x5DC0F */
    AIL_set_sample_address(s, mem + buf, size);            /* 0x1CB87 0x5DC2A */
    AIL_set_sample_volume(s, (s32)DSD(DS_000A2CB4));       /* 0x1CB9C 0x5DCC5 */
    AIL_set_sample_rate(s, 0x2B11u);                       /* 0x1CBB0 0x5DCA6 */
    AIL_set_sample_type(s, 0, 0);                          /* 0x1CBC3 0x5DC4D */
    if (DSB(DS_00102868 + off) == 1u)                      /* 0x1CBCA..0x1CBD6 */
        AIL_set_sample_loop_count(s, 0);                   /* 0x1CBE1 0x5DCE4 */
    AIL_start_sample(s);                                   /* 0x1CBFE 0x5DC70 */
    DSD(DS_0010286C + off) = DSD(DS_00102864 + off);       /* 0x1CC03/0x1CC0A */
    DSD(DS_00102864 + off) = 0;                            /* 0x1CC11/0x1CC16 */
}
```
In `game_audio_service`, replace
```c
    /* 0x1CF20 plays queued samples before it starts the pending song. */
    if (s_sample_request) {
        s_sample_request = 0;
        game_sample_play();
    }
```
with
```c
    for (u32 i = 0; i < 4u; i++) sound_sample_start(i);    /* 0x1CF21..0x1CF2E 0x1CB18 */
```
In `game_state_title`'s `DSB(DS_000F0A6F) == 0` arm, replace the `0x121C9/0x121D3`
comment, `s_music_request = 1;` and `game_sample_request();` with:
```c
        (void)sound_voice(0x41u);               /* 0x121C9/0x121CE 0x2C3FC: stop 0x40's loop */
        (void)sound_voice(0x43u);               /* 0x121D3/0x121D8 0x2C3FC: stop 0x42's loop */
        /* PORT: the S16TITLE bank is requested here (s_music_request), for
         * the raw's music request 0x54/0x56 at the attract's 0x111FF, whose
         * 0x1CA14 arm needs the sequence handle DS_001028C0 the port keeps 0
         * (todo-verify record §22). The raw's title plays no sample (record
         * k7-k12 §0.7.5). */
        s_music_request = 1;
```
Then delete:
- `game_sample_request`;
- `s_pending_sample` and `s_sample_request`, with their comment, and rewrite the
  `s_samples` comment's last sentence to "Slot i's handle; 0x1CC28/0x1CB18 use
  the slot records at DS_00102860.";
- `#define SND_ANNOUNCER_ID`;
- `#define SOUND_RES`, if `rg -n 'SOUND_RES' port/src/game/flow.c` shows no
  other use;
- `#include "platform/audio/samples.h"`, if `rg -n 'SampleVoice|samples_'
  port/src/game/flow.c` shows no other use.

In the sound-module comment above `SND_SLOT_STRIDE`, replace the paragraph that
starts `Named gap (spec §7): 0x1CC28's slot choice` with:
```
 * 0x1CC28 (snd_sample_queue), 0x1CB18 (sound_sample_start, called for each
 * slot by 0x1CF20) and 0x1D0BC (sound_buffers_alloc) are ported (record
 * k7-k12 §0.7); the time is DS_00101500 (0x500BB), advanced by
 * game_isr_ticks.
```
Add `void sound_sample_start(u32 slot);` to flow.h, with the comment `/* 0x1CB18
(record k7-k12 §0.7.4): start slot's queued sample; called by 0x1CF20. */`.

`port/src/game/attract.c`, phase 2: replace the `PORT: 0x2C3FC(0x40) and
0x2C3FC(0x42) voices, not wired` comment with:
```c
        /* 0x11160/0x1116C: the looping s16title samples 0x40 and 0x42 (loop
         * byte 1; record k7-k12 §0.7.5). The raw keeps ecx = 0x2D and bh = 3
         * live across them (item 2); those are the DS_000F0A60 / DS_000F0A70
         * values stored below. */
        (void)sound_voice(0x40u);                   /* 0x1115B/0x11160 0x2C3FC */
        (void)sound_voice(0x42u);                   /* 0x11167/0x1116C 0x2C3FC */
```

- [ ] **Step 5: Rewrite the `--check` probe**

In `port/src/main.c`, add `#include "platform/audio/ail.h"`. Replace
`probe_announcer_audio` and its comment with:
```c
/* Record §K7 (k7-k12 derivations §0.7.5): the attract's phase 2 queues the
 * looping s16title sample 0x40 (handle 0x0383B6F4, loop byte 1) at 0x11160,
 * 0x1CF20 -> 0x1CB18 starts it, and the title's first entry stops it (0x121CE,
 * voice 0x41). 1 when a slot holds that handle as playing (+0x0C and AIL
 * status 4). No device is open, so the mixer is not rendered and a started
 * voice stays live, which is the loop's own state. */
static int attract_loop_playing(void)
{
    for (u32 i = 0; i < 4u; i++)
        if (DSD(DS_0010286C + i * 0x18u) == 0x0383B6F4u &&
            AIL_sample_status(sound_slot_handle(i)) == 4)
            return 1;
    return 0;
}
```
In `run_check`, rename `announced` to `probed`. Add `int loop_before_title =
0;`, and after `u16 st = DSW(DS_000F0A64);` add
`if (st == 0) loop_before_title = attract_loop_playing();`. Then replace the
`title_entry + 2` block with:
```c
        /* The title's first entry stopped the attract's 0x40 loop, which
         * played on the last attract frame. */
        if (title_entry != 0 && !probed && i == title_entry + 2) {
            if (!loop_before_title) {
                fprintf(stderr, "prageport: --check the attract's 0x40 loop "
                                "was not playing before the title\n");
                fail++;
            }
            if (attract_loop_playing()) {
                fprintf(stderr, "prageport: --check the title did not stop "
                                "the attract's 0x40 loop\n");
                fail++;
            }
            probed = 1;
        }
```
Update the file-top comment that says "Task 12: after the title state queues
the announcer sample" to describe the new probe.

- [ ] **Step 6: Run the suite and `--check`**

Run: `cmake --build build 2>&1 | grep -E 'warning|error'; PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -3; ./build/prageport --game-dir data/game/C --check 820; echo CHECK=$?`
Expected: no warnings, `all checks passed`, and `CHECK=0`. Any other
assertion that fails must be one whose premise was the stubbed queue or the
stand-in. For each one, record the raw address that sets the new value in §3
and change it. Stop if a failing assertion is not of that kind.

- [ ] **Step 7: Mutation proof**

Apply each mutation alone, then restore.
- (a) `min > t` becomes `min >= t`: E's "all at now" case fails.
- (b) The scan runs 0..3 instead of 3..0: A fails (slot 0 instead of 3).
- (c) Delete the loop-byte test in `sound_sample_start`: B's loop check after
  24 renders fails.
- (d) Delete `if (buf == 0u || p == NULL) return;`: F fails.
- (e) Delete the `sound_voice(0x41u)` line in `game_state_title`:
  `./build/prageport --game-dir data/game/C --check 820` exits 1 with "the
  title did not stop".
- (f) Delete `(void)sound_voice(0x40u);` in `attract.c`: the same run exits 1
  with "was not playing before the title".

- [ ] **Step 8: Gate**

```bash
make verify 2>&1 | tee $K/verify-t3.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $K/verify-t3.txt | diff $K/oracle-lines-base.txt - && echo ORACLES-EQUAL
sh $K/dumps.sh after-t3 && sh $K/dumpcmp.sh before-t3 after-t3 && echo DUMPS-IDENTICAL
make audio-render AUDIO_WAV=$(pwd)/$K/after-t3.wav && cmp $K/before-t3.wav $K/after-t3.wav && echo WAV-IDENTICAL
python3 tools/port_progress.py
grep -c '/\* 0x1CB18' port/src/game/flow.c
rg -n 'game_sample_play|game_sample_request|s_pending_sample|SND_ANNOUNCER_ID' port/src
```
Expected: `EXIT=0`, `ORACLES-EQUAL`, `DUMPS-IDENTICAL`, `WAV-IDENTICAL`, a header
count of `1`, and nothing from the last `rg`. `port_progress` shows ported +1
over Task 2's value; at `e57d344` that is `764 1203 64` and `728 732 99`.
`python3 tools/port_progress.py --unported | grep 1CB18` prints nothing.

- [ ] **Step 9: Docs and checkpoint**

Append §3 to the derivations with the body, the test vectors A-F, the rewritten
assertions (old value, new value and raw address for each), the mutations and
the gate. Close §0.4 rows 11, 12, 170 and 171. If authorised, commit `audio:
port 0x1CC28's slot choice and 0x1CB18, retire the announcer stand-in (record
k7-k12 §3)`, staging `port/src/game/flow.c port/src/game/flow.h
port/src/game/attract.c port/src/main.c port/tests/test_game.c
docs/superpowers/plans/2026-09-29-k7-k12-derivations.md`.

---

### Task 4: K12 batch A. `0x100` stop-alls and the case-0/6 sites (22 points), plus the voice log

**Files:**
- Modify: `port/src/game/flow.c` (`sound_voice` logs its id; rows 139-141, 166, 172-176, 178, 182, 191, 194, 195), `port/src/game/flow.h` (log API), `port/src/game/attract.c` (rows 8-10), `port/src/game/fight.c` (rows 27, 29, 44), `port/src/game/menu.c` (rows 196, 197)
- Modify: `port/tests/test_fixtures.h`, `port/tests/test_fixtures.c` (`TfVoiceSite`, `tf_voice_sites`)
- Test: `port/tests/test_game.c` (`test_voice_sites`, `k12_a[]`), `port/tests/test.h` (register `X(test_voice_sites)` after `X(test_key_loop)`)

**Interfaces:**
- Consumes: derivations §1.3 rows whose batch is A.
- Produces:
  - `void sound_voice_log_reset(void); u32 sound_voice_log_count(void); u32 sound_voice_log_at(u32 i);` (flow.h).
  - `typedef struct { u32 row; void (*drive)(void); u32 n; u32 ids[4]; } TfVoiceSite;` and `void tf_voice_sites(const TfVoiceSite *t, u32 count);` (test_fixtures.h).
  - `int test_voice_sites(void)` in test_game.c. Tasks 5, 7, 11 add tables to it.

- [ ] **Step 0: Before-dumps**

`sh $K/dumps.sh before-t4; rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l > $K/checks-before-t4.txt`

- [ ] **Step 1: The seam and the runner**

In `port/src/game/flow.c`, add this before `sound_voice`:
```c
/* PORT: a test seam, not original state (the pattern of res.c's
 * res_set_screen_hook): the ids sound_voice was entered with since the last
 * reset, the first SOUND_VOICE_LOG_CAP of them. The K12 site tests (record
 * k7-k12 §4) read it to prove each wired 0x2C3FC call. */
#define SOUND_VOICE_LOG_CAP 16u
static u32 s_voice_log[SOUND_VOICE_LOG_CAP];
static u32 s_voice_log_n;
void sound_voice_log_reset(void) { s_voice_log_n = 0; }
u32 sound_voice_log_count(void) { return s_voice_log_n; }
u32 sound_voice_log_at(u32 i)
{
    return (i < s_voice_log_n && i < SOUND_VOICE_LOG_CAP) ? s_voice_log[i] : 0xFFFFFFFFu;
}
```
and make the first statement of `sound_voice`:
```c
    if (s_voice_log_n < SOUND_VOICE_LOG_CAP) s_voice_log[s_voice_log_n] = id;
    s_voice_log_n++;
```
Declare the three functions in flow.h with a `PORT: test seam` comment.

In `port/tests/test_fixtures.h`, add:
```c
/* Record k7-k12 §4: one row per wired 0x2C3FC call site (derivations §0.4
 * row number). drive() reaches the site from a public entry with seeded
 * mem[]; ids are the voices the raw calls on that path, in order. */
typedef struct { u32 row; void (*drive)(void); u32 n; u32 ids[4]; } TfVoiceSite;
/* Runs every row with the data object, both actor pools, the aperture and
 * the DAC restored after each, DS_001028C8 = 0 (no DIG: no case reads a bank,
 * 0x1CE78/0x1CC37), and checks the log holds ids[0..n) in order (other wired
 * voices on the same path may interleave). */
void tf_voice_sites(const TfVoiceSite *t, u32 count);
```
In `port/tests/test_fixtures.c`, add this (include `game/flow.h`,
`platform/gfx.h`, `<stdio.h>` and `<string.h>` if they are not already
included):
```c
void tf_voice_sites(const TfVoiceSite *t, u32 count)
{
    static u8 d[0x8B0D0], pa[0x4880], pb[0xEBA0], ap[320u * 200u], dac[256][3];
    const u32 a = DSD(DS_001014EC), b = DSD(DS_001014F4);
    for (u32 k = 0; k < count; k++) {
        tf_snap(d, DATA_BASE, 0x8B0D0u);
        if (a != 0u) tf_snap(pa, a, 0x4880u);
        if (b != 0u) tf_snap(pb, b, 0xEBA0u);
        memcpy(ap, gfx_aperture(), sizeof ap);
        memcpy(dac, gfx_dac, sizeof dac);
        DSD(DS_001028C8) = 0;
        sound_voice_log_reset();
        t[k].drive();
        u32 j = 0;
        for (u32 i = 0; i < sound_voice_log_count() && j < t[k].n; i++)
            if (sound_voice_log_at(i) == t[k].ids[j]) j++;
        if (j != t[k].n)
            fprintf(stderr, "voice site row %u: %u of %u ids in order\n",
                    (unsigned)t[k].row, (unsigned)j, (unsigned)t[k].n);
        CHECK_EQ_INT((int)j, (int)t[k].n);
        tf_put(d, DATA_BASE, 0x8B0D0u);
        if (a != 0u) tf_put(pa, a, 0x4880u);
        if (b != 0u) tf_put(pb, b, 0xEBA0u);
        memcpy(gfx_aperture(), ap, sizeof ap);
        memcpy(gfx_dac, dac, sizeof dac);
    }
}
```

- [ ] **Step 2: Write the failing batch table**

In `port/tests/test_game.c`, add one `static void vs_<name>(void)` per distinct
driver that §1.3 names for batch A, and the table. Transcribe one row per §1.3
row whose batch is A: 22 wiring points over the drivers §1.3 lists. Its first
entries are these (the seeds and the rest of the rows are §1.3's):
```c
/* Record k7-k12 §4, batch A: the 0x100 stop-alls and the case-0/6 sites. */
static void vs_coin_divert(void) { game_coin_divert(1u); }   /* rows 139, 140 */
static void vs_state6(void) { DSW(DS_000F0A64) = 6u; game_state_step(); }   /* row 175 */
static const TfVoiceSite k12_a[] = {
    { 139u, vs_coin_divert, 2u, { 0x100u, 0x53u } },   /* 0x257AD, 0x25820 */
    { 175u, vs_state6,      1u, { 0x100u } },          /* 0x11A94 */
    /* ...one entry per remaining §1.3 batch-A row, in §0.4 row order */
};

int test_voice_sites(void)
{
    int before = g_failures;
    /* 22 wiring points; a row may cover two (rows 139/140 share one path). */
    CHECK_EQ_INT((int)(sizeof k12_a / sizeof k12_a[0]), K12_A_ROWS);
    tf_voice_sites(k12_a, (u32)(sizeof k12_a / sizeof k12_a[0]));
    return g_failures - before;
}
```
Here `K12_A_ROWS` is a local `#define` equal to the number of table entries
§1.3 gives for batch A. Register `X(test_voice_sites)` in `TEST_CASES` after
`X(test_key_loop)`.
Run: `cmake --build build && PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep -c 'voice site row'`
Expected: one `voice site row` line per table entry, and the suite fails.

- [ ] **Step 3: Wire the 22 points**

At each batch-A comment, replace the `PORT: ... not wired (record §45-A)`
text with the call, placed where the comment is (the raw's position). Keep any
other sentence of the comment (for example, EDX notes) and delete only the
"not wired" claim. Worked examples:
```c
/* flow.c game_coin_divert, row 139 (was: PORT: 0x257AD 0x2C3FC(0x100) voice, not wired ...) */
    (void)sound_voice(0x100u);                          /* 0x257A8/0x257AD 0x2C3FC */
    actors_reset_al(0u);                                /* 0x257B2/0x257B4 0x2BAF4 */
/* row 140 keeps its AL paragraph, rewritten to start: */
    /* 0x25820 0x2C3FC(0x53), case 0. Its EAX is 0x257A4's return value, which no
     * caller reads: ... (the rest of the paragraph unchanged) */
    (void)sound_voice(0x53u);                           /* 0x2581B/0x25820 0x2C3FC */
/* menu.c menu_run, row 196 */
    (void)sound_voice(0x100u);                          /* 0x2FA68/0x2FA6D 0x2C3FC */
/* flow.c game_state_step case 7 timer exit, row 195 (a tail `jmp`) */
                /* 0x29D60 is a ret-only no-op; then 0x11BF0 `jmp 0x2C3FC`. */
                (void)sound_voice(0x100u);              /* 0x11BF0 0x2C3FC */
```
Use the raw's `mov`/`call` addresses from §1.3 in each trailing comment.
`fight.c:4751`'s `0xDE` is case 6: the call returns AL = 0 and writes nothing.
It is still made.

- [ ] **Step 4: Run the suite**

Run: `cmake --build build 2>&1 | grep -E 'warning|error'; PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -2`
Expected: no warnings, no `voice site row` line, and `all checks passed`.

- [ ] **Step 5: Mutation proof**

Delete the wired call of the first, middle and last table rows, one at a time.
Each run prints `voice site row <that row>` and fails. Restore after each.

- [ ] **Step 6: Gate**

```bash
make verify 2>&1 | tee $K/verify-t4.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $K/verify-t4.txt | diff $K/oracle-lines-base.txt - && echo ORACLES-EQUAL
sh $K/dumps.sh after-t4 && sh $K/dumpcmp.sh before-t4 after-t4 && echo DUMPS-IDENTICAL
rg -c 'not wired' port/src | sort | diff $K/notwired-base.txt - ; python3 tools/port_progress.py
```
Expected:
- `EXIT=0`, `ORACLES-EQUAL` and `DUMPS-IDENTICAL`. Batch A holds 9 oracle-path
  points (rows 8-10, 139, 140, 172, 175, 194, 195), all state-only.
- The `not wired` count drops by exactly the batch-A comment lines that
  carried the phrase: attract.c −3, fight.c −3, flow.c −10 and menu.c −2.
  `flow.c:5476` keeps its phrase until batch B2 wires row 192, and the
  split-phrase rows 176/178/182 carry no counted line. Record the per-file
  before and after in §4.
- `port_progress` is unchanged.

- [ ] **Step 7: Docs and checkpoint**

Append §4 to the derivations: the seam, the runner, the batch-A rows with their
drivers, the mutations and the gate. Mark the batch-A rows closed in §0.4. If
authorised, commit `game: wire the 0x100 stop-all and case-0/6 voices, the
voice-log seam (record k7-k12 §4)`, staging the files above.

---

### Task 5: K12 batch B1. The match-flow music requests and stops (21 points)

**Files:**
- Modify: `port/src/game/flow.c` (rows 130, 134, 136-138, 142-144, 146-153, 156-160)
- Test: `port/tests/test_game.c` (table `k12_b1[]`, run from `test_voice_sites`)

**Interfaces:**
- Consumes: `TfVoiceSite`/`tf_voice_sites` and `sound_voice_log_*` (Task 4), and the §1.3 batch-B1 rows.
- Produces: `k12_b1[]`.

- [ ] **Step 0: Before-dumps**: `sh $K/dumps.sh before-t5`

- [ ] **Step 1: Failing table**

Add the drivers and `static const TfVoiceSite k12_b1[]`, transcribing every
§1.3 batch-B1 row, and a local `#define K12_B1_ROWS` equal to their count. In
`test_voice_sites`, add:
```c
    CHECK_EQ_INT((int)(sizeof k12_b1 / sizeof k12_b1[0]), K12_B1_ROWS);
    tf_voice_sites(k12_b1, (u32)(sizeof k12_b1 / sizeof k12_b1[0]));
```
One row from §0.4, with its driver:
```c
/* rows 146/147: flow_arena_ko_check's second arm, 0x27347 0x27 then 0x27351 0x22
 * (the side's DS_0010780A below 0x78, DS_00104B12's at or above; §1.3 seed). */
static void vs_arena_ko_b(void)
{
    DSB(DS_0010810D) = 0u;
    DSB(DS_0010780A) = 0x10u;                  /* side 0 below 0x78: first arm skipped */
    DSB(DS_00104B12) = 1u;
    DSB(DS_0010780A + 0x94u) = 0x78u;          /* side 1 at 0x78: second arm */
    flow_arena_ko_check();
}
    { 146u, vs_arena_ko_b, 2u, { 0x27u, 0x22u } },
```
Run the suite. Expected: one `voice site row` line per B1 entry, and a failure.

- [ ] **Step 2: Wire the 21 points**

Replace each B1 comment with the call at its position. For example, in
`flow_arena_ko_check`:
```c
    (void)sound_voice(0x27u);                           /* 0x27342/0x27347 0x2C3FC */
    (void)sound_voice(0x22u);                           /* 0x2734C/0x27351 0x2C3FC */
    w = (s32)(s8)DSB(DS_0010810D);                      /* 0x27356/0x2735C */
```
and in `game_mode_0d_step`:
```c
    /* 0x277B0 0x2C3FC(0x25 + (n != 0)); it pushes and pops EDX, so the mode
     * word below is its EDX = 0xC. */
    (void)sound_voice(0x25u + (n != 0u ? 1u : 0u));     /* 0x277A0..0x277B0 0x2C3FC */
```
Take each id expression from §0.4. For `25/26`, compute it from the raw's
`setne` operand, as in the second example.

- [ ] **Step 3: Suite passes** with no `voice site row` line and `all checks passed`.

- [ ] **Step 4: Mutation proof.** Delete the first, middle and last row's call, one at a time. Each named row fails. Restore.

- [ ] **Step 5: Gate**

```bash
make verify 2>&1 | tee $K/verify-t5.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $K/verify-t5.txt | diff $K/oracle-lines-base.txt - && echo ORACLES-EQUAL
sh $K/dumps.sh after-t5 && sh $K/dumpcmp.sh before-t5 after-t5 && echo DUMPS-IDENTICAL
rg -c 'not wired' port/src | sort
```
Expected: `EXIT=0`, `ORACLES-EQUAL`, `DUMPS-IDENTICAL`. flow.c's count drops by
15, the B1 comment lines that carried the phrase: 377, 813, 930, 1364, 1384,
1635, 1641, 1653, 1721, 1900, 1963, 2020, 2073, 2120 and 2569 at `e57d344`.
Rows 156-159 are split-phrase.

- [ ] **Step 6: Docs and checkpoint.** Append §5. If authorised, commit `flow: wire the match-flow music voices (record k7-k12 §5)` with `port/src/game/flow.c port/tests/test_game.c docs/superpowers/plans/2026-09-29-k7-k12-derivations.md`.

---

### Task 6: K12 batch B2. The remaining pure-state voices (30 points)

**Files:**
- Modify: `port/src/game/flow.c` (rows 164, 165, 167, 169, 177, 179-181, 183-190, 192), `port/src/game/fight.c` (rows 18, 19, 24-26, 28, 30, 31), `port/src/game/actors.c` (rows 4, 5), `port/src/game/fighter.c` (rows 64, 65), `port/src/game/attract.c` (row 13)
- Test: `port/tests/test_game.c` (`k12_b2_game[]`, the flow/attract rows), `port/tests/test_fight.c` (`test_fight_voice_sites`, `k12_b2_fight[]`, the fight/actors/fighter rows), `port/tests/test.h` (register `X(test_fight_voice_sites)` after `X(test_voice_sites)`)

**Interfaces:**
- Consumes: Task 4's fixture and seam, and the §1.3 batch-B2 rows.
- Produces: `int test_fight_voice_sites(void)` in test_fight.c. Tasks 7-10 add tables to it.

- [ ] **Step 0: Before-dumps**: `sh $K/dumps.sh before-t6`

- [ ] **Step 1: Failing tables**

`test_game.c`: `k12_b2_game[]`, run from `test_voice_sites`, with a
`K12_B2_GAME_ROWS` count check. `test_fight.c`:
```c
/* Record k7-k12 §6..§10: the fight-area voice sites (tables per batch). */
int test_fight_voice_sites(void)
{
    int before = g_failures;
    CHECK_EQ_INT((int)(sizeof k12_b2_fight / sizeof k12_b2_fight[0]), K12_B2_FIGHT_ROWS);
    tf_voice_sites(k12_b2_fight, (u32)(sizeof k12_b2_fight / sizeof k12_b2_fight[0]));
    return g_failures - before;
}
```
Row 13 (`attract_step` phase 4, `0x111FF`) is an oracle-path row. Its driver:
```c
static void vs_attract_4(void)
{
    u32 rec = DSD(DS_000F0A50);                 /* §1.3: a live phase-3 actor */
    DSB(DS_000F0A6F) = 4u;
    DSW(rec + 0x2Cu) = 0x1100u;                 /* 0x1100 - 0x100 = 0x1000 < 0x1001 */
    DSD(DS_000F0A5C) = 1u;
    attract_step();
}
    { 13u, vs_attract_4, 1u, { 0x56u } },       /* DS_000F0A5C != 0 -> 0x56 (0x111FA) */
```
§1.3 gives the phase-3 actor seed (`DS_000F0A50`). Check the id against the
raw `jne 0x111FA` sense in Ghidra, and record it in §6. Run the suite and
expect one failure line per row.

- [ ] **Step 2: Wire the 30 points.** For example, `fight.c`'s `fight_hook_430e8` tail:
```c
    DSD(DS_00104AE4) = FN_000430C0;                     /* 0x43284 */
    (void)sound_voice(0x2Du);                           /* 0x43285/0x4328A 0x2C3FC */
    (void)sound_voice(0x2Fu);                           /* 0x4328F/0x43294 0x2C3FC */
```
and `actors.c`'s `0x3D784` (a tail `jmp`):
```c
    (void)sound_voice(0x4Fu);                           /* 0x3D784/0x3D789 `jmp 0x2c3fc` */
```
Row 186/187 (`game_mode_1e_step`'s merged path over four raw pairs) wires one
call pair on the port's one path:
`(void)sound_voice(0xE3u); (void)sound_voice(0xE2u);`. List all eight raw
addresses in the trailing comment.

- [ ] **Step 3: Suite passes.**
- [ ] **Step 4: Mutation proof** on the first, middle and last row of each table. Each named row fails. Restore.
- [ ] **Step 5: Gate**

```bash
make verify 2>&1 | tee $K/verify-t6.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $K/verify-t6.txt | diff $K/oracle-lines-base.txt - && echo ORACLES-EQUAL
sh $K/dumps.sh after-t6 && sh $K/dumpcmp.sh before-t6 after-t6 && echo DUMPS-IDENTICAL
rg -c 'not wired' port/src | sort
```
Expected: `EXIT=0`, `ORACLES-EQUAL` and `DUMPS-IDENTICAL`. Row 13 is on every
oracle run and is case 1: it writes `DS_00105D5C`/`D4`/`D9` only.
- [ ] **Step 6: Docs and checkpoint.** Append §6. If authorised, commit `game: wire the remaining pure-state voices (record k7-k12 §6)` with the modified files above.

---

### Task 7: K12 batch C. Sample voices on oracle paths (22 points)

**Files:**
- Modify: `port/src/game/attract.c` (rows 6, 7), `port/src/game/actors.c` (row 1), `port/src/game/fight.c` (rows 33-35, 38), `port/src/game/fighter.c` (rows 58, 60, 61, 66-72, 75, 76, 92-94)
- Test: `port/tests/test_game.c` (`k12_c_game[]`: rows 6, 7), `port/tests/test_fight.c` (`k12_c_fight[]`: the rest)

**Interfaces:**
- Consumes: the Task 3 slot path (these voices now queue and start samples in `--check` and the fe run) and the Task 4/6 fixtures.
- Produces: `k12_c_game[]`, `k12_c_fight[]`.

- [ ] **Step 0: Before-dumps**: `sh $K/dumps.sh before-t7`

- [ ] **Step 1: Failing tables.** Transcribe the §1.3 batch-C rows. Worked rows:
```c
/* rows 6/7: attract_voice_tick's two countdowns expire in one call. */
static void vs_voice_tick(void)
{
    DSW(DS_000F0A60) = 1u;
    DSW(DS_000F0A62) = 1u;
    rng_seed(0xABCDu);                          /* §1.3: the pick this seed draws */
    attract_voice_tick();
}
    { 6u, vs_voice_tick, 2u, { 0xBDu, 0xBEu } }, /* the second id per §1.3's pick */
```
`fight_4a634` (rows 33-35) is static: its driver goes through the public caller
§1.3 names, with slot `+0x42` bit 2 and `DS_001088A8[side]` seeded.

- [ ] **Step 2: Wire the 22 points.**

Where a draw selects the id, keep the draw and use its result:
```c
            if (r >= 0x20u && r <= 0x3Fu) {                 /* 0x4A655/0x4A65A */
                u32 d = rng_next(3u);                       /* 0x4A664 */
                (void)sound_voice(d == 1u ? 0xCEu : d == 2u ? 0xCFu : 0xCDu);  /* 0x4A669..0x4A688, 0x4A6D2 */
            } else if (r >= 0x10u && r <= 0x17u) {          /* 0x4A692/0x4A697 */
                if (rng_next(2u) != 0u) {                   /* 0x4A69E */
                    if (rng_next(2u) != 0u) {               /* 0x4A6A9/0x4A6B0 */
                        (void)sound_voice(0xC9u);           /* 0x4A6B7 */
                        (void)sound_voice(0xDAu);           /* 0x4A6D2 */
                    } else {
                        (void)sound_voice(0xCAu);           /* 0x4A6C8 */
                        (void)sound_voice(0xDBu);           /* 0x4A6D2 */
                    }
                }
            }
```
The same applies in `attract.c`:
`(void)sound_voice(pick ? 0xBEu : 0xBFu);  /* 0x10F8B 0x2C3FC */`, deleting
`(void)pick;`. For table ids, read the word the raw reads, for example
`(void)sound_voice((u32)DSW(0xBDFFAu + (u32)DSB(slot + 0x7Au) * 2u));  /*
0x3B9A2..0x3B9B8 */`. Use a local `#define` for each table address that has no
symbols.h name. Row 1 (`0x2B8D7`, opcode `0x2E`) is `(void)sound_voice((u32)ax
& 0xFFFFu);  /* 0x2B8D2/0x2B8D7 */`, where `ax` is `spawn_anim_opcode`'s
operand word, as its other cases use it.

- [ ] **Step 3: Suite passes.**
- [ ] **Step 4: Mutation proof** on rows 6, 33 and 94. Each named row fails. Restore.
- [ ] **Step 5: Gate. This batch is the oracle risk.**

```bash
make verify 2>&1 | tee $K/verify-t7.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $K/verify-t7.txt | diff $K/oracle-lines-base.txt - && echo ORACLES-EQUAL
sh $K/dumps.sh after-t7 && sh $K/dumpcmp.sh before-t7 after-t7 && echo DUMPS-IDENTICAL
./build/prageport --game-dir data/game/C --check 8000; echo CHECK=$?
```
Expected: `EXIT=0`, `ORACLES-EQUAL`, `DUMPS-IDENTICAL` and `CHECK=0`. The
§0.2 probe measured every one of these voices resolving an already-read bank;
the `--check` probe still finds the 0x40 loop before the title. If a dump
differs, **halt**. Find the first differing frame, log `res_resolve`'s first
reads in a scratch build (as §0.2 did), and report old frame, new frame and
the voice and bank.
- [ ] **Step 6: Docs and checkpoint.** Append §7 (rows, drivers, gate outputs). If authorised, commit `game: wire the oracle-path sample voices (record k7-k12 §7)`.

---

### Task 8: K12 batch D1. fight.c, camera.c and actors.c real-play samples (31 points)

**Files:**
- Modify: `port/src/game/fight.c` (rows 20-23, 32, 36, 37, 39-43, 45-57), `port/src/game/camera.c` (rows 14-17), `port/src/game/actors.c` (rows 2, 3)
- Test: `port/tests/test_fight.c` (`k12_d1[]`)

**Interfaces:**
- Consumes: Task 4/6 fixtures and the §1.3 batch-D1 rows.
- Produces: `k12_d1[]`.

- [ ] **Step 0: Before-dumps**: `sh $K/dumps.sh before-t8`
- [ ] **Step 1: Failing table** `k12_d1[]` from §1.3. `fight_4e99c`'s eight paths are eight rows, each with its seeded `r` (`DS_001088BD`) and `bx`. For example:
```c
    { 50u, vs_4e99c_r0_bx0_odd, 1u, { 0x5Du } },   /* 0x4E9FC/0x4EA01 -> 0x4EB72 */
```
- [ ] **Step 2: Wire the 31 points.** Each `fight_4e99c` path ends with its call:
```c
            DSB(DS_001088BD) = save;                                          /* 0x4E9F4/0x4E9F7 */
            (void)sound_voice(0x5Du);                                         /* 0x4E9FC/0x4EA01 -> 0x4EB72 0x2C3FC */
```
`fight.c:4007`'s three calls ("out of scope (spec §7)") are wired as the raw
makes them. Its comment is replaced by the calls and the raw addresses. The
`0x4C85C` id is `0xD4` when the ball's `(u8)+0x48 - 0x20 < 3`, else `0xD5`.
- [ ] **Step 3: Suite passes.**
- [ ] **Step 4: Mutation proof** on rows 20, 45 and 57.
- [ ] **Step 5: Gate**, using the Task 7 Step 5 block with `t8` in place of `t7`. Expected: the same four outputs. These rows are real-play only (§0.3), so any dump change is a defect: halt.
- [ ] **Step 6: Docs and checkpoint.** Append §8. If authorised, commit `fight: wire the real-play sample voices (record k7-k12 §8)`.

---

### Task 9: K12 batch D2. fighter.c real-play samples, part 1 (28 points)

**Files:**
- Modify: `port/src/game/fighter.c` (rows 59, 62, 63, 73, 74, 77-91, 95-102)
- Test: `port/tests/test_fight.c` (`k12_d2[]`)

**Interfaces:**
- Consumes: the §1.3 batch-D2 rows and the fight fixtures.
- Produces: `k12_d2[]`.

- [ ] **Step 0: Before-dumps**: `sh $K/dumps.sh before-t9`
- [ ] **Step 1: Failing table** `k12_d2[]` from §1.3.
- [ ] **Step 2: Wire the 28 points.** Table-id example (row 95):
```c
    /* 0x3E30C 0x2C3FC(word 0xC75AA[ctx[3]'s char]). */
    (void)sound_voice((u32)DSW(FIGHTER_C75AA + (u32)DSB(ctx[3] + 0x7Au) * 2u));   /* 0x3E2F9..0x3E30C */
```
Define `#define FIGHTER_C75AA 0x000C75AAu` (with "no symbols.h name") if
symbols.h has no name. Row 63 (`0x3923D`) takes its id from the draws already
in `fighter_39040`: `0xCD/0xCE/0xCF` by the `rng_next(3)` value and
`0xDA/0xDB` by `rng_next(2)` (`0x391FE..0x39238`). Capture the existing draws'
results, as batch C did.
- [ ] **Step 3: Suite passes.**
- [ ] **Step 4: Mutation proof** on rows 59, 85 and 102.
- [ ] **Step 5: Gate**, using the Task 7 Step 5 block with `t9`. Expected: the same four outputs.
- [ ] **Step 6: Docs and checkpoint.** Append §9. If authorised, commit `fighter: wire the real-play sample voices, part 1 (record k7-k12 §9)`.

---

### Task 10: K12 batch D3. fighter.c real-play samples, part 2 (27 points)

**Files:**
- Modify: `port/src/game/fighter.c` (rows 103-129)
- Test: `port/tests/test_fight.c` (`k12_d3[]`)

**Interfaces:**
- Consumes: the §1.3 batch-D3 rows.
- Produces: `k12_d3[]`.

- [ ] **Step 0: Before-dumps**: `sh $K/dumps.sh before-t10`
- [ ] **Step 1: Failing table** `k12_d3[]` from §1.3. Rows 115-117 and 128/129 have header-only comments. §1.3 names the body position, which is the raw call's place in the port's statement order.
- [ ] **Step 2: Wire the 27 points.** Rows 126/127 are one raw call (`0x45F8C`) reached with `0xAB` or `0xAC`, so the port wires one call on each of its two paths. Row 104 (`0x46`, case 3) and rows 105/106 (`0x6C` then `0x72` on the other branch) follow the raw's branch (`0x459AF`/`0x459B9`). In the header-only rows, rewrite the header's "not wired" sentence to cite the call.
- [ ] **Step 3: Suite passes.**
- [ ] **Step 4: Mutation proof** on rows 103, 115 and 129.
- [ ] **Step 5: Gate**, using the Task 7 Step 5 block with `t10`. Expected: the same four outputs.
- [ ] **Step 6: Docs and checkpoint.** Append §10. If authorised, commit `fighter: wire the real-play sample voices, part 2 (record k7-k12 §10)`.

---

### Task 11: K12 batch D4. nameentry.c and flow.c real-play samples (33 points, plus any §1.2 "silent" sites)

**Files:**
- Modify: `port/src/game/nameentry.c` (rows 198-218), `port/src/game/flow.c` (rows 131-133, 135, 145, 154, 155, 161-163, 168, 193), and the files of any §1.2 "silent" site
- Test: `port/tests/test_game.c` (`k12_d4[]`)

**Interfaces:**
- Consumes: the §1.3 batch-D4 rows and §1.2's silent-site rows.
- Produces: `k12_d4[]`.

- [ ] **Step 0: Before-dumps**: `sh $K/dumps.sh before-t11`
- [ ] **Step 1: Failing table** `k12_d4[]` from §1.3 and §1.2.
- [ ] **Step 2: Wire the points.** For example, `nameentry_step`'s cursor move:
```c
        if (ne_pad(side, 0x10u) || ne_repeat(DSD(DS_001044E0))) {   /* 0x1F4DF..0x1F520 */
            (void)sound_voice(0xE6u);                               /* 0x1F526/0x1F52B 0x2C3FC */
```
and `game_mode_12_step`'s portrait flash (row 131):
```c
            u32 n = DSB(DS_00108112);                         /* 0x41D1A */
            DSB(DS_00108112) = (u8)(n + 1u);                  /* 0x41D23 */
            (void)sound_voice(n + 0x34u);                     /* 0x41D28/0x41D2B 0x2C3FC (DL = n, unread) */
```
`nameentry.c`'s paired `mov`s that share one `call` (for example,
`0x1F580`/`0x1F58E` -> `0x1F593`) are one call on each port path.
- [ ] **Step 3: Suite passes.**
- [ ] **Step 4: Mutation proof** on rows 131, 206 and 218.
- [ ] **Step 5: Gate**, using the Task 7 Step 5 block with `t11`. Also run `rg -n 'not wired' port/src`. Expected: the same four outputs, and no output from `rg`.
- [ ] **Step 6: Docs and checkpoint.** Append §11. If authorised, commit `game: wire the name-entry and flow sample voices (record k7-k12 §11)`.

---

### Task 12: Reconciliation. PROGRESS, the ledger and the README count

**Files:**
- Modify: `docs/PROGRESS.md` (append one paragraph)
- Modify: `docs/superpowers/plans/2026-09-29-all-gaps-ledger.md` (§B.1 row `0x1CB18`; §E rows 5 and 6; §F rows 10 and 19; the "about 180 'not wired'" paragraph)
- Modify: `README.md` (the title percentage and the "N% of the original's D real functions" line)
- Modify: `docs/superpowers/plans/2026-09-29-k7-k12-derivations.md` (§12, the closing summary)

**Interfaces:**
- Consumes: §1-§11, `python3 tools/port_progress.py`.
- Produces: the closed ledger rows.

- [ ] **Step 1: Final sweep**

```bash
rg -n 'not wired' port/src
rg -n '0x2C3FC' port/src | rg -i 'out of scope|not wired|stand-in'
python3 tools/port_progress.py; python3 tools/port_progress.py --unported | grep -v runtime
rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l
```
Expected: the first two print nothing, and `1CB18` is absent from the unported
list. The CHECK total is the Task-1 value plus the sites added in Tasks 2-11.
§2-§11 each record their delta, and the sum must match.

- [ ] **Step 2: Full gate from clean**

```bash
make clean && make build && make verify 2>&1 | tee $K/verify-final.txt; echo EXIT=${PIPESTATUS[0]}
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $K/verify-final.txt | diff $K/oracle-lines-base.txt - && echo ORACLES-EQUAL
sh $K/dumps.sh final && sh $K/dumpcmp.sh base final && echo DUMPS-IDENTICAL
```
Expected: `EXIT=0`, `ORACLES-EQUAL` and `DUMPS-IDENTICAL` (against Task 1's
`base`).

- [ ] **Step 3: Ledger rows**

Update the ledger:
- §B.1 row `0x1CB18`: in the class column, "**closed**: ported (Task 3, record
  `2026-09-29-k7-k12-derivations.md` §3)". Add in the notes: "`0x1D0BC` ported
  too (§2; its host-owned row removed)".
- §E row 5 (`game/flow.c` sound-module comment) and row 6
  (`snd_sample_queue`): "**closed** (record k7-k12 §3): `0x1CC28`'s choice and
  `0x1CB18` are ported; the comments cite §0.7".
- §F row 10 (K7): "Task 3 — **closed** (plan `2026-09-29-k7-k12-audio-voice.md`,
  §2/§3)".
- §F row 19 (K12): "Task 6 — **closed** (§4-§11: 218 wiring points over 213
  raw calls; 85 raw calls in unported code listed in §1.2)".
- Replace the "about 180 sites carry 'not wired (record §45-A)'" bullet with
  one naming this record, §0.3's counts (75 pure-state, 143 sample, 36
  oracle-path, 182 real-play only) and "closed".

- [ ] **Step 4: PROGRESS and README**

Append one paragraph to `docs/PROGRESS.md`, in the style of the existing
entries, headed `**all-gaps K7 + K12 AUDIO-SMP / VOICE-WIRE: the sample slots
and the 218 omitted voice calls (record `2026-09-29-k7-k12-derivations.md`).**`.
It covers:
- `0x1D0BC`, `0x1CC28` and `0x1CB18` ported, and the ISR clock pair;
- the AIL end status;
- the announcer stand-in retired (raw wins, §0.7.5);
- the 218 points by batch, and the 85 outside sites;
- the oracle outcome (all dumps byte-identical, every line equal to §A);
- the counter before and after.

Then set `README.md`'s title `— Reverse Engineering NN%` and its "N% of the
original's D real functions" line to `port_progress.py`'s first line.

- [ ] **Step 5: Checkpoint**

Report to the controller: the counter, the batch closure and the three §0.9
decisions as executed. If authorised, commit `docs: k7-k12 closure (ledger
§B.1/§E/§F, PROGRESS, README)`, staging `docs/PROGRESS.md README.md
docs/superpowers/plans/2026-09-29-all-gaps-ledger.md
docs/superpowers/plans/2026-09-29-k7-k12-derivations.md`.
