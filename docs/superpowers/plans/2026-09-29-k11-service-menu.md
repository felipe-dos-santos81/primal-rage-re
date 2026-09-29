# K11 MENU-CB — the options ("service") menu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the 50-function closure behind the menu tables `0xBCBDC`, `0xBCC1C`
and `0xBCCCC`: the 18 table callbacks, their sub-screens and helpers (15 304
reachable instruction bytes). After this, Enter in mode `0x27` reaches every
screen the raw has. That covers the START MENU mode setters, CONFIG OPTIONS,
STATISTICS, SOUND TEST, MUSIC TEST, MODIFY CONTROLS, CONFIGURE KEYBOARD, TEST
CONTROLS, ADJUST VOLUME and 2 PLAYER HANDICAP.

**Architecture:** A new module `port/src/game/svcmenu.{c,h}` holds one C function
per original function, with `/* 0xADDR — record §K11.N */` headers. The table
callbacks are registered with `fn_register` from `svcmenu_register()`, which
`game_init` calls. `menu_step`/`menu_run` (`menu.c`) already dispatch through
`fn_resolve`. Blocking loops keep the raw's shape: each pass is `0x2EA74`/`0x2EA78`
(the ported `config_screen_wait`). Unit tests script one input per presented
frame through a new host test seam, `host_set_pump_hook`. Seven cycles, each
under the size gate (< 4 KB, < 20 functions), ordered so each cycle's callees
are ported before its callers.

**Tech Stack:** C11, CMake, the repo's `run_tests` (`CHECK`/`CHECK_EQ_INT`),
Python 3 + capstone over the LE-loader mirror for the raw.

**Spec:** `docs/superpowers/plans/2026-09-29-all-gaps.md` (master plan, Task 6 "Large clusters"),
ledger `docs/superpowers/plans/2026-09-29-all-gaps-ledger.md` (§B.1 rows `0x2F464`/`0x319B0`/`0x31A78`,
§B.2 "Service menu", §F row 18), derivation record
`docs/superpowers/plans/2026-09-29-k11-service-menu-derivations.md` (§0 = the walk this plan is cut from).

## Global Constraints

Copied from the master plan; every task's requirements include them.

- **No enforced oracle claim moves.** Gate is `make verify` exit 0 with `PR_ORACLE_REQUIRED=1`. Record the pre-change values of every oracle line in Task 1 and compare after each cycle. If a correct fix moves one: **halt**, report old value, new value and the raw evidence; update the claim in the same commit only if justified. Raise the demo-fight / attract2 ratchet N only when it *improves*.
- **Never ship a fitted constant.** Every value comes from raw bytes or a capture, with the address. A value that cannot be pinned is a **named gap with its evidence**, not a plausible number.
- **On any plan-vs-raw conflict the raw wins** — record correction and address in the derivation record and report.
- **Address model:** Ghidra address == linear address; data global `DAT_0008xxxx` = `DS_000xxxx`. Use Ghidra (fixups applied) for data addresses, never raw-file disassembly. Never write `mem[0xA0000]`; composite into `mem + DSD(DS_000E87A4)` and present through `gfx_present()`.
- **One C function per original function**, header `/* 0xADDR — record §N */`. Deviations `/* PORT: ... */`, doubts `/* TODO(verify): ... */`; no other comment styles. Code addresses stored in data go through `fn_origin()`/`fn_resolve()`. SDL and file I/O only in `host.c`/`main.c`. Never shadow original state in a C global. Do not reformat files owned by another module.
- **`port/src/symbols.h` is generated** — never hand-edit; use a local `#define` with the raw address where the generator emits no name.
- **Tests:** one area file per subsystem (`test_platform.c`, `test_game.c`, `test_fight.c`, `test_audio.c`, `test_video.c`), registered once via `X(test_foo)` in `TEST_CASES` (`port/tests/test.h`). Only `CHECK`/`CHECK_EQ_INT`. **Assertions must be able to fail:** seed sentinels differing from the post-condition, prove the assertion fails under a mutation, never assert an unseeded BSS-zero. `game_init()` runs once per process — env-gate any test that calls it. Consolidating tests must not change an assertion.
- **Run from the repo root.** `data/` is read-only and git-ignored; oracles skip silently without `PR_ORACLE_REQUIRED=1`. No worktree (it lacks `data/`); branch `all-gaps`, in place. 0 warnings; no new dependency.
- **Commits:** style `<area>: <what changed>`, stage named files only (never `git add -A`), commit incrementally — **but only when the user has authorised committing** (`AGENTS.md`: "Only commit when asked"). Until then, steps that say "Commit" are checkpoints where the executor stops and asks.
- **Host-owned / deferred is a valid closure** for a function only with an evidence line in `tools/port_classification.txt` (`ADDR CLASS record-§N`) and a derivation-record row naming the port's replacement.
- **Docs:** append a paragraph per cycle to `docs/PROGRESS.md`; keep `README.md`'s `— Reverse Engineering NN%` and "N% of the original's D real functions" current via `python3 tools/port_progress.py`.

K11-specific constraints (from the walk, record §0):

- **Verdict: all 50 functions are `port`.** None is host-owned or deferred (§0.6). No `tools/port_classification.txt` row is added. The callees that are not ported stay as `PORT:` notes at the call site: `0x1B084` (deferred, row `1B084`), the language reload inside `0x47370`, and the runtime `0x655E4` (`itoa`) and `0x61A70` (`memset`).
- **No oracle reaches this code** (§0.7). Tests are unit tests with seeded `mem[]`, real `ENGLISH.TXT` strings (`ch_text_setup`) and scripted input. The only change on an oracle path is in `game_init`: two `0x2F9CC` stores and `svcmenu_register()` (Task 2). Task 2 proves the frames are byte-identical.
- **Scripted-frame budget.** `config_screen_wait` sleeps to the 60 Hz boundary on every pass (`host_wait_vblank`), about 33 ms per `0x2EA74` frame. Keep all K11 scripts to **≤ 160 frames in total**, about 5 s of suite time. Each task states its frame count.
- `S` below is `/private/tmp/claude-501/-Users-felipe-dos-santos-code-mine-primal-rage-reverse/6102b0fb-fcd6-4551-b6e4-2e65618a97ed/scratchpad`. Its `k11_*.py` scripts (walk, closure, disassembly `k11_dx.py ADDR LEN`, strings `k11_str.py ID..`) are the raw tools. `le.py` rebuilds the fixed-up image `k11_img.bin`.

## Review Focus

- **A blocking loop that never exits under scripted input** (a wrong exit bit, or a key read through the pad path when the raw reads the BIOS latch) hangs the suite. The harness pushes Esc and Enter once its script runs out, counts the extra frames, and after 600 of them exits the process with a message. Every scripted run ends with `sm_end(n)`, which asserts that exactly `n` frames were consumed and none were extra (Task 2 Step 1).
- **The nested `0x2FFC4` state** (`0x2CB74` re-initialises the one shared menu state onto START MENU). Esc from START MENU must return -1 with `DS_00107414 = 0`, so the next frame re-initialises MAIN MENU. Task 2's START test pins the Esc exit after the nested initialisation.
- **CONFIG OPTIONS with `DS_0010740C` unset** reads `[4]` and falls to the code-object bytes at `0x32700`. Task 2 adds the raw's `0x2FA01` store, and Task 3 asserts that `0x2CACC` hands `0xA2EB4` to `0x33578`.
- **Callback results mistaken for -5/-10**: the mode setters return the entry address with AH replaced. Task 2 asserts the exact EAX and that `menu_step` returns 0 after them.
- **A function ported twice or registered twice**: every task asserts `grep -c` = 1 per new header. `svcmenu_register` is idempotent, and Task 2 checks that two calls resolve to the same function.

---

### Task 1: Baseline and the derivation record

**Files:**
- Modify: `docs/superpowers/plans/2026-09-29-k11-service-menu-derivations.md` (append `§K11.0` baseline under the existing §0)

**Interfaces:**
- Consumes: record §0 (already written by the planner).
- Produces: `§K11.0` oracle lines (the comparison base for every task), `$S/k11_t1_or.txt`, `$S/k11_frames_before/`.

- [ ] **Step 1: Build and capture the baseline**

```bash
cmake --build build 2>&1 | grep -Ei "warning|error" ; echo build-exit=${PIPESTATUS[0]}
make verify 2>&1 | tee $S/k11_t1_verify.txt ; echo verify-exit=${PIPESTATUS[0]}
bash $S/orlines.sh $S/k11_t1_verify.txt > $S/k11_t1_or.txt
python3 tools/port_progress.py | tee $S/k11_t1_progress.txt
./build/prageport --game-dir data/game/C --check 8000 >/dev/null && rm -rf $S/k11_frames_before && cp -R frames $S/k11_frames_before
```
Expected: `build-exit=0` with no warning line, `verify-exit=0`, and `k11_t1_or.txt` equal to ledger §A. `port_progress` prints `762 1203 63` / `726 731 99` or the tree's current values; write down what it prints. If the baseline is red, stop and report.

- [ ] **Step 2: Spot-check §0 against the raw**

```bash
cd $S && python3 le.py k11_img.bin && cmp k11_img.bin img_rev.bin && python3 k11_closure.py > k11_closure_t1.txt && tail -1 k11_closure_t1.txt
python3 k11_tab.py | awk -F'|' '{s+=$4} END {print NR, s}'
```
Expected: `TOTAL 53 functions 15577 bytes` from the closure script. That total includes the ported `0x38B18`, `0x500BB` and `0x1AE28`. The table script prints `50 15304`. Any difference is a correction: write it into §K11.0 with the address.

- [ ] **Step 3: Write §K11.0**

Append to the record: the verbatim oracle lines (`k11_t1_or.txt`), the `port_progress` lines, the frame-dump path, and "§0 re-checked: 50 functions / 15 304 B" (or the correction).

- [ ] **Step 4: Checkpoint**

Commit only if authorised: `git add docs/superpowers/plans/2026-09-29-k11-service-menu.md docs/superpowers/plans/2026-09-29-k11-service-menu-derivations.md && git commit -m "docs: K11 service-menu plan and walk (record §0, §K11.0)"`.

---

### Task 2: Cycle 1 — seams, module, registration, the shell (10 functions, 282 B)

Functions: `0x2CB74`, `0x2CB94`, `0x2CBC4`, `0x2CBDC`, `0x2CBF4`, `0x2CC0C`, `0x2CC24`, `0x2CC3C`, `0x2CC54`, `0x2F99C`. Also the two `0x2F9CC` stores in `game_init` (`0x2FA01`, `0x2FA10..0x2FA1C`).

**Files:**
- Create: `port/src/game/svcmenu.c`, `port/src/game/svcmenu.h`
- Modify: `port/CMakeLists.txt:13` (add `src/game/svcmenu.c` to `prage_core`)
- Modify: `port/src/host.c` (pump hook), `port/src/host.h`
- Modify: `port/src/game/flow.c` (`game_init`: the stores and `svcmenu_register()`, near the `config_validate();` line, currently `flow.c:6564`; and case `0x27`'s comment, `flow.c:7169..7180`)
- Modify: `port/src/game/menu.c:41-43` and `port/src/game/menu.h` (the "callbacks are not ported" comments)
- Modify: `port/tests/test_fixtures.{h,c}` (move `mt_press` as `tf_menu_press`, with `MT_LAYOUT`), `port/tests/test_platform.c` (35 call sites renamed, local `MT_LAYOUT` define removed)
- Modify: `port/tests/test_game.c` (new `sm_*` harness, `test_svcmenu`; add `#include "game/svcmenu.h"` and `#include "game/menu.h"`), `port/tests/test.h` (register `X(test_svcmenu)` directly after `X(test_cfg_helpers)`: it relies on that test's one `actors_init()`)
- Modify: record, append `§K11.1` (seams) and `§K11.2` (cycle 1)

**Interfaces:**
- Consumes: `menu_step(u32,u32,u32)`, `menu_run(u32,u32,u32)` (`menu.h`); `config_credits_init(void)`, `config_field_get(u32)` (`config.h`); `frontend_input_reset`, `actors_reset`, `frontend_origin_zero`, `frontend_spawn_row` (as `menu_title_draw` calls them, `menu.c:90-93`).
- Produces (later tasks rely on these exact names):
  - `typedef void (*host_pump_hook_fn)(void *ctx); void host_set_pump_hook(host_pump_hook_fn fn, void *ctx);` (`host.h`)
  - `void tf_menu_press(u32 bits);` and `#define MT_LAYOUT 0x3E2D000u` (`test_fixtures.h`)
  - test harness in `test_game.c`: `typedef struct { u16 key; u32 pad; } sm_step_t; static void sm_begin(const sm_step_t *s, u32 n); static void sm_end(u32 frames, const char *what);` and `test_svcmenu` with one `sm_check_*` call per cycle.
  - `svcmenu.h`: `void svcmenu_register(void); u32 svc_start_menu(u32 entry); u32 svc_options_menu(u32 entry); u32 svc_start_arcade_left(u32); u32 svc_start_arcade_right(u32); u32 svc_start_training_left(u32); u32 svc_start_training_right(u32); u32 svc_start_tug_of_war(u32); u32 svc_start_endurance(u32); u32 svc_start_handicap(u32); void svc_screen_reset(void);`

- [ ] **Step 1: The test seams (no raw counterpart)**

`host.h`, after `host_wait_vblank`'s declaration:
```c
/* PORT: test seam, no raw counterpart. When set, host_pump() calls fn(ctx)
 * once at the end of every call. The blocking menu loops reach the host only
 * through config_screen_wait's host_wait_vblank(); the unit tests script one
 * input per presented frame through this hook. NULL (the default) disables it. */
typedef void (*host_pump_hook_fn)(void *ctx);
void host_set_pump_hook(host_pump_hook_fn fn, void *ctx);
```
`host.c`, next to the other file-scope state and at the end of `host_pump()`:
```c
static host_pump_hook_fn g_pump_hook;
static void *g_pump_hook_ctx;

void host_set_pump_hook(host_pump_hook_fn fn, void *ctx)
{
    g_pump_hook = fn;
    g_pump_hook_ctx = ctx;
}
/* ... at the end of host_pump(): */
    if (g_pump_hook != NULL) g_pump_hook(g_pump_hook_ctx);
```
Move `mt_press` byte-for-byte to `test_fixtures.c` as `tf_menu_press`. Move `#define MT_LAYOUT 0x3E2D000u` with it into `test_fixtures.h` (under a `/* ---- test_platform.c: the menu fixtures ---- */` line), delete the define from `test_platform.c`, and rename the 35 call sites (`sed -i '' 's/\bmt_press(/tf_menu_press(/g' port/tests/test_platform.c`). Check the move changed no assertion: `grep -c 'CHECK' port/tests/test_platform.c` is the same before and after.

The harness goes in `test_game.c`, after `ch_check_quit_prompt`:
```c
/* ---- the options ("service") menu, record 2026-09-29-k11-service-menu-derivations.md ---- */
typedef struct { u16 key; u32 pad; } sm_step_t;   /* key = (scan << 8) | ascii, 0 = none */
static const sm_step_t *sm_script;
static u32 sm_len, sm_frame, sm_last_a0, sm_extra;
static u32 sm_s_a0, sm_s_a4, sm_s_kb;
static u8 sm_s_gate;

/* One scripted step per presented frame: config_screen_wait swaps DS_000E87A0
 * before its tick passes, so the first hook call after a swap is a new frame. */
static void sm_hook(void *ctx)
{
    (void)ctx;
    if (DSD(DS_000E87A0) == sm_last_a0) return;
    sm_last_a0 = DSD(DS_000E87A0);
    if (sm_frame < sm_len) {
        const sm_step_t *s = &sm_script[sm_frame];
        if (s->key != 0u) input_push((u8)(s->key >> 8), (u8)s->key);
        tf_menu_press(s->pad);
    } else {
        /* Out of script: Esc, then alternately Enter, so any loop leaves. */
        sm_extra++;
        input_push(0x01, 0x1B);
        tf_menu_press((sm_extra & 1u) ? 0x2000000u : 0x1000000u);
        if (sm_extra > 600u) {
            printf("FAIL %s:%d: scripted menu loop never left\n", __FILE__, __LINE__);
            exit(1);
        }
    }
    sm_frame++;
}

/* The environment every options-menu test needs, with or without the hook. */
static void sm_env_begin(void)
{
    sm_s_a0 = DSD(DS_000E87A0); sm_s_a4 = DSD(DS_000E87A4);
    sm_s_kb = DSD(DS_00101514); sm_s_gate = DSB(0x00104B22u);
    DSD(DS_000E87A0) = CH_BUF_A;
    DSD(DS_000E87A4) = CH_BUF_B;
    DSB(0x00104B22u) = 0u;              /* not 1: the ISR tick model runs (0x1BDF8) */
    mem_fill(MT_LAYOUT, 0, 0x300u);
    DSD(DS_00101514) = MT_LAYOUT;
    input_clear();
    tf_menu_press(0u);
    DSD(CH_KEY_LATCH) = 0u;
    /* 0x2EB80's idle timeout (tick - DS_00105F2C > 0x4B0, `jbe` at 0x2EB9F)
     * clears DS_00107414 on a no-key poll; the suite's tick has run on, so
     * stamp the key time now. */
    DSD(CH_KEY_TIME) = DSD(CH_TICK);
}

static void sm_env_end(void)
{
    input_clear();
    tf_menu_press(0u);
    DSD(DS_000E87A0) = sm_s_a0; DSD(DS_000E87A4) = sm_s_a4;
    DSD(DS_00101514) = sm_s_kb; DSB(0x00104B22u) = sm_s_gate;
}

static void sm_begin(const sm_step_t *s, u32 n)
{
    sm_env_begin();
    sm_script = s; sm_len = n; sm_frame = 0u; sm_extra = 0u;
    sm_last_a0 = DSD(DS_000E87A0);
    host_set_pump_hook(sm_hook, NULL);
}

static void sm_end(u32 frames, const char *what)
{
    host_set_pump_hook(NULL, NULL);
    CHECK(sm_extra == 0u, what);
    CHECK_EQ_INT((int)sm_frame, (int)frames);
    sm_env_end();
}
```
Seam test, the first check in `test_svcmenu` (`sm_check_seam`): script `{ {0x011B, 0}, {0x1C0D, 0} }`, then call `config_screen_wait_zero()` twice. After the first call, `DSD(CH_KEY_LATCH) == 0x1B`. After the second it is `0x0D`. Seed `CH_KEY_LATCH = 0x77` first, and `sm_end(2, ...)`. For the mutation, remove the hook call from `host_pump`: both latch checks and the frame count fail.

- [ ] **Step 2: Write the failing cycle-1 tests (`sm_check_shell`)**

```c
static void sm_check_shell(void)
{
    static const struct { u32 (*fn)(u32); u32 addr, entry, mode, ah; } st[] = {
        { svc_start_arcade_left,    0x2CBC4u, 0xBCCDCu, 0x2Du, 0u },
        { svc_start_arcade_right,   0x2CBDCu, 0xBCCECu, 0x2Eu, 0u },
        { svc_start_training_left,  0x2CBF4u, 0xBCCFCu, 0x28u, 1u },
        { svc_start_training_right, 0x2CC0Cu, 0xBCD0Cu, 0x29u, 1u },
        { svc_start_tug_of_war,     0x2CC24u, 0xBCD1Cu, 0x2Au, 2u },
        { svc_start_endurance,      0x2CC3Cu, 0xBCD2Cu, 0x2Bu, 3u },
        { svc_start_handicap,       0x2CC54u, 0xBCD3Cu, 0x2Cu, 4u },
    };
    const u16 s_mode = DSW(DS_00104B00), s_mode_hi = DSW(DS_00104B00 + 2u);
    const u8 s_sub = DSB(DS_00104B1D);
    svcmenu_register();
    svcmenu_register();                                   /* idempotent */
    for (u32 i = 0; i < sizeof st / sizeof st[0]; i++) {
        /* record §0.2: the table's +8 holds this callback */
        CHECK_EQ_INT((int)DSD(st[i].entry + 8u), (int)st[i].addr);
        CHECK(fn_resolve(st[i].addr) == (void (*)(void))st[i].fn, "registered");
        DSW(DS_00104B00) = 0xBEEFu; DSW(DS_00104B00 + 2u) = 0x7777u; DSB(DS_00104B1D) = 0xA5u;
        u32 r = st[i].fn(st[i].entry);
        CHECK_EQ_INT((int)DSW(DS_00104B00), (int)st[i].mode);
        CHECK_EQ_INT((int)DSW(DS_00104B00 + 2u), 0x7777);  /* a word store */
        CHECK_EQ_INT((int)DSB(DS_00104B1D), (int)st[i].ah);
        CHECK_EQ_INT((int)r, (int)((st[i].entry & 0xFFFF00FFu) | (st[i].ah << 8)));
    }
    CHECK(fn_resolve(0x2CB74u) == (void (*)(void))svc_start_menu, "0x2CB74 registered");
    CHECK(fn_resolve(0x2CB94u) == (void (*)(void))svc_options_menu, "0x2CB94 registered");

    /* 0x2F99C: the reset menu_title_draw opens with (record §K11.2). */
    ch_text_setup();
    text_cursor_set(5, 5, (const u8 *)"Z", 0u);
    CHECK(ch_cell(5, 5) != 0u, "the pre-reset cell");
    DSB(DS_00104B15) = 1u; DSW(DS_00107A3A) = 0x77u; DSW(DS_00107A38) = 0x66u; DSD(DS_00107A1C) = 0u;
    svc_screen_reset();
    CHECK_EQ_INT((int)ch_cell(5, 5), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B15), 0);
    CHECK_EQ_INT((int)DSW(DS_00107A3A), 0);
    CHECK_EQ_INT((int)DSW(DS_00107A38), 0);
    CHECK(DSD(DS_00107A1C) != 0u, "the 0x9AD84 backdrop row is spawned");

    /* START: menu_step over the stock MAIN MENU, one pad press per call. No
     * hook: menu_step presents frames only at initialisation (0x2FFF1), and
     * the pad state is set here between calls. */
    sm_env_begin();
    mem_fill(DS_00107414, 0, 0x40u);
    DSW(DS_00104B00) = 0xBEEFu; DSB(DS_00104B1D) = 0xA5u;
    CHECK_EQ_INT((int)menu_step(0xBCBDCu, 0x10u, 4u), 0);            /* init */
    CHECK_EQ_INT((int)DSD(DS_0010741C), 0xBCBEC);
    tf_menu_press(0x1000000u);                                        /* Enter on "Start" */
    CHECK_EQ_INT((int)menu_step(0xBCBDCu, 0x10u, 4u), 0);
    CHECK_EQ_INT((int)DSD(DS_0010741C), 0xBCCDC);                     /* 0x2CB74 re-initialised */
    CHECK_EQ_INT((int)DSB(DS_00107414), 1);
    CHECK_EQ_INT((int)DSD(DS_00107418), 0);                           /* ECX = 0 at 0x2CB86 */
    tf_menu_press(0u);  (void)menu_step(0xBCBDCu, 0x10u, 4u);         /* redraw, release */
    tf_menu_press(0x40004000u); (void)menu_step(0xBCBDCu, 0x10u, 4u); /* Down */
    tf_menu_press(0u);  (void)menu_step(0xBCBDCu, 0x10u, 4u);
    tf_menu_press(0x40004000u); (void)menu_step(0xBCBDCu, 0x10u, 4u); /* Down */
    tf_menu_press(0u);  (void)menu_step(0xBCBDCu, 0x10u, 4u);
    tf_menu_press(0x1000000u);
    CHECK_EQ_INT((int)menu_step(0xBCBDCu, 0x10u, 4u), 0);             /* EAX = 0x000B01FC is not -5/-10 */
    CHECK_EQ_INT((int)DSW(DS_00104B00), 0x28);                        /* item 2: LEFT TRAINING, 0x2CBF4 */
    CHECK_EQ_INT((int)DSB(DS_00104B1D), 1);
    CHECK_EQ_INT((int)DSB(DS_00107414), 0);

    /* START then Esc: the nested state has flags 0 (DS_00107418), so Esc takes
     * 0x30449..0x30466: DS_0010742C (0) != DS_00107424 (7 items walked) gives
     * -5 with DS_00107414 = 0, and the next call re-initialises MAIN MENU. */
    mem_fill(DS_00107414, 0, 0x40u);
    tf_menu_press(0u);         (void)menu_step(0xBCBDCu, 0x10u, 4u);  /* init MAIN */
    tf_menu_press(0x1000000u); (void)menu_step(0xBCBDCu, 0x10u, 4u);  /* Start: nested init */
    tf_menu_press(0u);         (void)menu_step(0xBCBDCu, 0x10u, 4u);  /* redraw, release */
    tf_menu_press(0x2000000u);
    CHECK_EQ_INT((int)menu_step(0xBCBDCu, 0x10u, 4u), -5);            /* 0x30466 */
    CHECK_EQ_INT((int)DSB(DS_00107414), 0);                           /* 0x30456 */
    tf_menu_press(0u);
    CHECK_EQ_INT((int)menu_step(0xBCBDCu, 0x10u, 4u), 0);
    CHECK_EQ_INT((int)DSD(DS_0010741C), 0xBCBEC);                     /* MAIN MENU again */
    CHECK_EQ_INT((int)DSD(DS_00107418), 4);
    sm_env_end();

    /* GAME OPTIONS: 0x2CB94 = menu_run(0xBCC1C) (blocking), 0x1B084 (deferred), 0x2C304. */
    static const sm_step_t esc_now[] = { { 0u, 0x2000000u } };
    const u32 want_credits = ((config_field_get(0x29u) & 0xF0000u) >> 16) + 1u;
    DSD(DS_00105C00) = 0xDEADu;
    sm_begin(esc_now, 1u);
    CHECK_EQ_INT((int)svc_options_menu(0xBCBFCu), -1);                /* 0x2FD53..0x2FD60 */
    sm_end(1u, "GAME OPTIONS left on the first Esc");
    CHECK_EQ_INT((int)DSD(DS_00105C00), (int)want_credits);

    DSW(DS_00104B00) = s_mode; DSW(DS_00104B00 + 2u) = s_mode_hi; DSB(DS_00104B1D) = s_sub;
}
```
Before running, confirm each expected value above against the raw and record it in §K11.2. That covers `0x2FFC4`'s item order and its Down search (`0x3055E..0x305BE`). It also covers the Enter arm (`0x30480`, `0x304AA..0x304B3`) and the Esc arm (`0x3041E..0x30466`). Where the raw differs, fix the test and record the correction.

Register `X(test_svcmenu)` after `X(test_cfg_helpers)` in `test.h`:
```c
int test_svcmenu(void)
{
    int before = g_failures;
    sm_check_seam();
    sm_check_shell();
    return g_failures - before;
}
```

- [ ] **Step 3: Run the tests and confirm they fail**

Run: `cmake --build build 2>&1 | tail -3`
Expected: link errors for `svc_*`/`svcmenu_register` (red at link). Record the output in `$S/k11_t2_red.txt`.

- [ ] **Step 4: Implement the module and wiring**

`svcmenu.h` declares the Produces list above. Each declaration gets one comment line with its address and screen. `svcmenu.c`:
```c
/* port/src/game/svcmenu.c */
#include "game/svcmenu.h"

#include "game/actors.h"
#include "game/config.h"
#include "game/flow.h"
#include "game/menu.h"

#include "../mem.h"
#include "../symbols.h"

#define SVC_TABLE_START   0x000BCCCCu   /* START MENU (record §0.2) */
#define SVC_TABLE_OPTIONS 0x000BCC1Cu   /* OPTIONS MENU */
#define SVC_BACKDROP      0x0009AD84u   /* the descriptor 0x2F99C spawns */

/* 0x2CB74 — record §K11.2. */
u32 svc_start_menu(u32 entry)
{
    (void)entry;                                           /* EAX is reloaded at 0x2CB81 */
    return menu_step(SVC_TABLE_START, 0x10u, 0u);          /* 0x2CB77..0x2CB88 0x2FFC4 (EBX = 0xF000, unread) */
}

/* 0x2CB94 — record §K11.2. */
u32 svc_options_menu(u32 entry)
{
    (void)entry;
    u32 r = menu_run(SVC_TABLE_OPTIONS, 0x10u, 0u);        /* 0x2CB97..0x2CBA8 0x2FA40 */
    /* PORT: 0x2CBAF 0x1B084, the CMOS config writer, is deferred
     * (classification row 1B084, record §50-C); the settings stay in mem[]. */
    config_credits_init();                                 /* 0x2CBB4 0x2C304 */
    return r;                                              /* 0x2CBB9 mov eax,edx */
}

/* 0x2CBC4 — record §K11.2. LEFT PLAYER ARCADE. */
u32 svc_start_arcade_left(u32 entry)
{
    DSW(DS_00104B00) = 0x2Du;                              /* 0x2CBC5 edx, 0x2CBCC word store */
    DSB(DS_00104B1D) = 0u;                                 /* 0x2CBCA xor ah,ah; 0x2CBD3 */
    return entry & 0xFFFF00FFu;                            /* EAX: the entry with AH = 0 */
}

/* 0x2CBDC — record §K11.2. RIGHT PLAYER ARCADE. */
u32 svc_start_arcade_right(u32 entry)
{
    DSW(DS_00104B00) = 0x2Eu;                              /* 0x2CBDD, 0x2CBE4 */
    DSB(DS_00104B1D) = 0u;                                 /* 0x2CBE2 xor ah,ah; 0x2CBEB */
    return entry & 0xFFFF00FFu;
}

/* 0x2CBF4 — record §K11.2. LEFT PLAYER TRAINING. */
u32 svc_start_training_left(u32 entry)
{
    DSW(DS_00104B00) = 0x28u;                              /* 0x2CBF5, 0x2CBFC */
    DSB(DS_00104B1D) = 1u;                                 /* 0x2CBFA mov ah,1; 0x2CC03 */
    return (entry & 0xFFFF00FFu) | 0x0100u;
}

/* 0x2CC0C — record §K11.2. RIGHT PLAYER TRAINING. */
u32 svc_start_training_right(u32 entry)
{
    DSW(DS_00104B00) = 0x29u;                              /* 0x2CC0D, 0x2CC14 */
    DSB(DS_00104B1D) = 1u;                                 /* 0x2CC12, 0x2CC1B */
    return (entry & 0xFFFF00FFu) | 0x0100u;
}

/* 0x2CC24 — record §K11.2. TUG OF WAR. */
u32 svc_start_tug_of_war(u32 entry)
{
    DSW(DS_00104B00) = 0x2Au;                              /* 0x2CC25, 0x2CC2C */
    DSB(DS_00104B1D) = 2u;                                 /* 0x2CC2A, 0x2CC33 */
    return (entry & 0xFFFF00FFu) | 0x0200u;
}

/* 0x2CC3C — record §K11.2. ENDURANCE. */
u32 svc_start_endurance(u32 entry)
{
    DSW(DS_00104B00) = 0x2Bu;                              /* 0x2CC3D, 0x2CC44 */
    DSB(DS_00104B1D) = 3u;                                 /* 0x2CC42, 0x2CC4B */
    return (entry & 0xFFFF00FFu) | 0x0300u;
}

/* 0x2CC54 — record §K11.2. Start 2 PLAYER HANDICAP. */
u32 svc_start_handicap(u32 entry)
{
    DSW(DS_00104B00) = 0x2Cu;                              /* 0x2CC55, 0x2CC5C */
    DSB(DS_00104B1D) = 4u;                                 /* 0x2CC5A, 0x2CC63 */
    return (entry & 0xFFFF00FFu) | 0x0400u;
}

/* 0x2F99C — record §K11.2. The screen reset of every options screen. */
void svc_screen_reset(void)
{
    frontend_input_reset();                                /* 0x2F99F..0x2F9A1 0x4F1E4 (EAX = 0) */
    actors_reset();                                        /* 0x2F9A6..0x2F9AF 0x2BAF4 (EAX = 1) */
    frontend_origin_zero();                                /* 0x2F9B4..0x2F9B8 0x4F1D0 */
    frontend_spawn_row((const u32 *)(mem + SVC_BACKDROP), 0u, 0u);   /* 0x2F9BD..0x2F9C2 0x38B18 */
}

/* PORT: the menu tables 0xBCBDC/0xBCC1C/0xBCCCC hold these as code
 * addresses (record §0.2); menu_step/menu_run reach them through fn_resolve.
 * One-time: fn_register appends unconditionally. Later cycles add their roots. */
void svcmenu_register(void)
{
    static int done;
    if (done) return;
    done = 1;
    fn_register(0x2CB74u, (void (*)(void))svc_start_menu);
    fn_register(0x2CB94u, (void (*)(void))svc_options_menu);
    fn_register(0x2CBC4u, (void (*)(void))svc_start_arcade_left);
    fn_register(0x2CBDCu, (void (*)(void))svc_start_arcade_right);
    fn_register(0x2CBF4u, (void (*)(void))svc_start_training_left);
    fn_register(0x2CC0Cu, (void (*)(void))svc_start_training_right);
    fn_register(0x2CC24u, (void (*)(void))svc_start_tug_of_war);
    fn_register(0x2CC3Cu, (void (*)(void))svc_start_endurance);
    fn_register(0x2CC54u, (void (*)(void))svc_start_handicap);
}
```
Before writing, check the three `menu_title_draw` callee names (`frontend_input_reset`, `actors_reset`, `frontend_origin_zero`) against `menu.c`. `0x4F1E4` takes EAX = 0 at `0x2F99F`; confirm the port's `frontend_input_reset` has no argument that differs. Put `#include "game/svcmenu.h"` in `flow.c`. In `game_init`:
```c
    DSD(DS_0010740C) = 0x0001D2D0u;   /* 0x2FA01 (record §K11.2): 0x2F9CC's pointer; [0x1D2D4] = 0xA2EB4, CONFIG OPTIONS */
    config_validate();          /* 0x2F9CC's 0x2D6F8, before 0x20C5D */
    DSD(DS_00107410) = config_field_get(0x2Au) & 0xFFFFFFFCu;   /* 0x2FA10..0x2FA1C `and al,0xfc` */
    svcmenu_register();
```
Replace `menu.c:41-43`'s PORT note: "an address that was never registered is skipped and reads as result 0; the stock callbacks are registered by `svcmenu_register` (record §K11.2)". Update `menu.h`'s last paragraph to match. Rewrite case `0x27`'s "the raw's other setters are in the unported 0x2CBxx callbacks" to "the START MENU callbacks `0x2CBC4..0x2CC54` (svcmenu.c, record §K11.2) store modes 0x28..0x2E".

- [ ] **Step 5: Run the tests and confirm they pass**

Run: `cmake --build build 2>&1 | grep -Ei "warning|error"; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -2`
Expected: no warnings; `all checks passed`.

- [ ] **Step 6: Mutation proof (each rebuilt, run, reverted)**

1. `svc_start_training_left` stores 0x29 → the mode check fails. 2. It stores AH 0 → the `DS_00104B1D` and EAX checks fail. 3. The `0x2CBC4` store becomes a dword (`DSD`) → the `+2` sentinel check fails. 4. The `0x2CB74` registration is dropped → `DSD(DS_0010741C) == 0xBCCDC` fails. 5. `svc_start_menu` passes flags 4 → the `DS_00107418` check fails. 6. `config_credits_init` is dropped from `svc_options_menu` → the credits check fails. 7. `frontend_spawn_row` is dropped from `svc_screen_reset` → the backdrop check fails. 8. The hook call is removed from `host_pump` → the seam checks fail.
Log each result to `$S/k11_t2_mutations.txt` as "caught". A mutation that survives means an assertion is missing: add one.

- [ ] **Step 7: Gate**

```bash
make verify 2>&1 | tee $S/k11_t2_verify.txt; echo verify-exit=${PIPESTATUS[0]}
bash $S/orlines.sh $S/k11_t2_verify.txt > $S/k11_t2_or.txt && diff $S/k11_t1_or.txt $S/k11_t2_or.txt && echo ORACLES-UNCHANGED
./build/prageport --game-dir data/game/C --check 8000 >/dev/null && diff -rq $S/k11_frames_before frames && echo FRAMES-IDENTICAL
for a in 2CB74 2CB94 2CBC4 2CBDC 2CBF4 2CC0C 2CC24 2CC3C 2CC54 2F99C; do printf "%s " $a; grep -rh "/\* 0x$a\b" port/src | wc -l; done
```
Expected: `verify-exit=0`, `ORACLES-UNCHANGED`, `FRAMES-IDENTICAL`, and each address counted `1`. If frames or an oracle line move, **halt** and report (Global Constraints).

- [ ] **Step 8: Record, docs, checkpoint**

Append §K11.1 (the seams, the harness contract and the frame budget used: 1 frame here) and §K11.2 (the ten functions, the two stores, the START-Esc value and the mutation list) to the record. Append a PROGRESS.md paragraph. Commit (if authorised): `git add port/src/game/svcmenu.c port/src/game/svcmenu.h port/CMakeLists.txt port/src/host.c port/src/host.h port/src/game/flow.c port/src/game/menu.c port/src/game/menu.h port/tests/test_fixtures.c port/tests/test_fixtures.h port/tests/test_platform.c port/tests/test_game.c port/tests/test.h docs/superpowers/plans/2026-09-29-k11-service-menu-derivations.md docs/PROGRESS.md && git commit -m "game: port the options-menu shell 0x2CB74..0x2CC54, 0x2F99C (record §K11.2)"`

---

### Task 3: Cycle 2 — the option editor, CONFIG OPTIONS, SOUND TEST, MUSIC TEST (9 functions, 2253 B)

Functions: `0x2CC74` (89), `0x2CD30` (463), `0x2CF00` (995), `0x2CACC` (13), `0x33578` (321), `0x30EB4` (157), `0x30F54` (161), `0x2C9CC` (27), `0x2C9E8` (27).

**Files:**
- Modify: `port/src/game/svcmenu.{c,h}`, `port/tests/test_game.c` (`sm_check_options`), record (`§K11.3`)

**Interfaces:**
- Consumes: Task 2's harness and `svc_screen_reset`; `config_menu_default_bits(u32)`, `config_field_get/set`, `config_input_poll(u32,u8)`, `config_screen_wait_zero()`, `input_repeat_set(u32,u32,u32)` (`input.h`), `sound_voice(u32)` (`flow.h`), `game_string_table_load` (for the `0x47370` note), `text_cursor_set`, `text_cells_release`.
- Produces: `s32 svc_option_rows(u32 table, u32 bits, s32 first, u32 mode, u32 release)` (0x2CC74); `u32 svc_option_row(u32 opt, u32 bits, s32 row, u32 mode, u32 release)` (0x2CD30, returns the next record or 0); `u32 svc_option_edit(u32 table, u32 bits, u32 mask, u8 esc_cancels)` (0x2CF00); `u32 svc_config_options_entry(u32 entry)` (0x2CACC); `u32 svc_config_options(u32 table)` (0x33578); `u32 svc_sound_test(u32 entry)` (0x30EB4); `u32 svc_music_test(u32 entry)` (0x30F54); `u32 svc_play_tune(u32 i)` (0x2C9CC); `u32 svc_play_sample(u32 i)` (0x2C9E8). Registers `0x2CACC`, `0x30EB4`, `0x30F54`.

- [ ] **Step 1: Derive (record §K11.3)**

```bash
cd $S && for a in 2cc74:5c 2cd30:1d0 2cf00:3e4 2cacc:10 33578:142 30eb4:a0 30f54:94 2c9cc:1c 2c9e8:1c; do python3 k11_dx.py ${a%%:*} ${a##*:}; done > k11_t3_dis.txt
```
Write one block per function: the register and stack arguments (`ret 4` for `0x2CC74`/`0x2CD30`), every branch with its address, every store, and the return value. Include the option-record layout (`+0` id, `+4` shift, `+8` count, `+0xC` byte, `+0x10` list of `{char*, id}`) and the value mask (`0x2CD43..0x2CD68`, the smallest `2^n - 1` covering `count`). Include `0x2CF00`'s scroll arithmetic (window 6, "+ MORE +" `0x72` via `0x2CFAC..0x2CFDF`), its key handling (`0x2D020..0x2D2BB`) and its exits (`0x2D2C0..0x2D2DA`). Include `0x33578`'s bit-15 default path and the language test (`0x3369C..0x336AE`). Pin these test values from the raw:
  - (a) `0x2CD30` on CONFIG record 0 at `0xA2EB4`: string `0x46` "CREDITS", shift `0x10`, count 10, byte 1, list `0xA2CEC`. The list is all zero except entry 4, `{0x8088C "*", 0}`, the default marker. The mask loop (`0x2CD43..0x2CD57`) gives 4 bits (15). The label goes at (col 4, `row`). The value line is on `row + 1` (`0x2CE16 inc edx`): the index `itoa(v + 1)` at column 5, then the text and the string after it. With bits `3 << 0x10`: "CREDITS" at row 6 from column 4 in mode `0x2000`, then "4" at (5, 7) in `0x2000`, and nothing at (6, 7). With bits `4 << 0x10`: "*" switches the mode to `(0x2000 & 0x8000) | 0x1000` = `0x1000` before the index is drawn (`0x2CDE4..0x2CDFD`), so "5" is at (5, 7) in `0x1000`. It returns `0xA2EB4 + 0x14` = `0xA2EC8`.
  - (b) the out-of-range value (`value + 1 > count`, e.g. bits `0xF << 0x10`) → returns 0 (`0x2CD77`) and draws nothing.
  - (c) `0x2CF00`'s ESP bookkeeping: each `push` before a `ret 4` callee shifts `[esp+N]` by 4 until the call returns. For example, the store at `0x2CF6E` `[esp+0x24]` after `push 0` is the local `[esp+0x20]` (the current record, initialised to the table). The locals are `[esp+0x30]` the result bits, `[esp+0x34]` the working bits, `[esp+0x38]` the scroll top and `[esp+0x3C]` the last index. Left is `0x2D20D..0x2D238`: a zero field wraps to `count - 1`, else it subtracts `1 << shift`. Right is `0x2D23A..0x2D272`: a field at `count - 1` wraps to 0, else it adds `1 << shift`. Enter returns `[esp+0x30]`. Esc returns -1 with CL set (`0x2D2C8`).
  - (d) `0x2C9CC(0)` → `sound_voice(0x100)`, then `sound_voice(DSD(0xBC938))` = `0x1B`. Record `0xBBDC8 + 12*0x1B` is case 1 with handle `0x0D000008`, so `DS_00105D5C` becomes `0x0D000008`. `0x2C9E8(3)` → voice `DSD(0xBC9A0 + 12)` = `0xEA`.
  - (e) `0x2CACC` → `0x33578([[DS_0010740C] + 4])` = `0x33578(0xA2EB4)`. Set field `0x29` with bit 15 clear and CREDITS = 2, then script Right (BIOS `0x4D00`) and Enter (`0x1C0D`). The loop takes one `0x2EA74` frame per pass, so this is 2 frames. Afterwards field `0x29` = the seeded value + `1 << 16`. Bit 15 set instead → `config_menu_default_bits(0xA2EB4)` is written before the edit (`0x335C3..0x335D3`). Confirm from `config.c` that field `0x29` holds bits 0..26. `0x33578` tests bit 15 and bits 24..26.

- [ ] **Step 2: Write the failing tests (`sm_check_options`, ≤ 30 scripted frames)**

```c
static void sm_check_options(void)
{
    ch_text_setup();
    /* (a) 0x2CD30: CONFIG record 0 "CREDITS" (record §K11.3 (a)). */
    actors_reset();
    CHECK_EQ_INT((int)svc_option_row(0xA2EB4u, 3u << 0x10, 6, 0x2000u, 0u), 0xA2EC8);
    ch_expect(6, 4, 'C', 0x2000u, "label at column 4");
    ch_expect(7, 5, '4', 0x2000u, "itoa(3 + 1) on row + 1, column 5");
    CHECK_EQ_INT((int)ch_cell(7, 6), 0);                       /* no text, no string */
    actors_reset();
    (void)svc_option_row(0xA2EB4u, 4u << 0x10, 6, 0x2000u, 0u);
    ch_expect(7, 5, '5', 0x1000u, "the '*' default switches the mode to 0x1000");
    /* release = 1 (0x2CD7E): the same cells are released, not drawn */
    (void)svc_option_row(0xA2EB4u, 4u << 0x10, 6, 0x2000u, 1u);
    CHECK_EQ_INT((int)ch_cell(6, 4), 0);
    CHECK_EQ_INT((int)ch_cell(7, 5), 0);
    /* (b) out of range */
    actors_reset();
    CHECK_EQ_INT((int)svc_option_row(0xA2EB4u, 0xFu << 0x10, 6, 0x2000u, 0u), 0);
    CHECK_EQ_INT((int)ch_cell(6, 4), 0);
    /* (c) 0x2CF00 alone on the CONFIG table: Left from CREDITS 0 wraps to 9, Esc with CL = 1 → -1 */
    static const sm_step_t left_enter[] = { { 0x4B00u, 0u }, { 0x1C0Du, 0u } };
    sm_begin(left_enter, 2u);
    CHECK_EQ_INT((int)svc_option_edit(0xA2EB4u, 0x00000000u, 0x4000000u, 0u), (int)(9u << 16));
    sm_end(2u, "option editor: Left, Enter");
    static const sm_step_t esc_only[] = { { 0x011Bu, 0u } };
    sm_begin(esc_only, 1u);
    CHECK_EQ_INT((int)svc_option_edit(0xA2EB4u, 5u << 16, 0x4000000u, 1u), -1);
    sm_end(1u, "option editor: Esc cancels");
    sm_begin(esc_only, 1u);
    CHECK_EQ_INT((int)svc_option_edit(0xA2EB4u, 5u << 16, 0x4000000u, 0u), (int)(5u << 16));
    sm_end(1u, "option editor: Esc keeps");
    /* (d) the voice wrappers */
    DSD(DS_00105D5C) = 0xDEADu;
    (void)svc_play_tune(0u);
    CHECK_EQ_INT((int)DSD(DS_00105D5C), 0x0D000008);
    /* (e) CONFIG OPTIONS end to end, and 0x2CACC's table argument */
    const u32 s_740c = DSD(DS_0010740C);
    DSD(DS_0010740C) = 0x0001D2D0u;
    const u32 f0 = config_field_get(0x29u);
    const u32 v0 = (f0 & ~0x000F8000u) | (2u << 16);           /* bit 15 clear, CREDITS = 2 */
    config_field_set(0x29u, v0);
    static const sm_step_t right_enter[] = { { 0x4D00u, 0u }, { 0x1C0Du, 0u } };
    sm_begin(right_enter, 2u);
    (void)svc_config_options_entry(0xBCC2Cu);
    sm_end(2u, "CONFIG OPTIONS: Right, Enter");
    CHECK_EQ_INT((int)config_field_get(0x29u), (int)(v0 + (1u << 16)));
    config_field_set(0x29u, f0);
    DSD(DS_0010740C) = s_740c;
}
```
Step 1 must confirm every literal against the raw before the test is final: the Left wrap, the frame counts, and the `0x2C3FC` case-1 store. Record any correction in §K11.3. Add `sm_check_options();` to `test_svcmenu`. Frames: 8.

- [ ] **Step 3: Run the tests and confirm they fail**: `cmake --build build` → link errors for the new names (`$S/k11_t3_red.txt`).

- [ ] **Step 4: Port the nine functions**

Write one function per address in `svcmenu.c`, instruction block by instruction block, in `menu.c`'s style: a C statement per raw block with the address range in its comment. Required notes:
  - `0x2CD30`: `/* PORT: 0x2CE11 0x655E4 is the WATCOM itoa(value + 1, buf, 10) (runtime, not a target); formatted in C. */` Build the buffer on the C stack and draw it with `text_cursor_set`/`text_cells_release` (the `release` flag picks `0x2F280` over `0x2F198`, as at `0x2CD7E`).
  - `0x2CF00`: blocking; one `config_screen_wait_zero()` per pass (`0x2D00C`); `input_repeat_set(0xF000F000u, 0x1Eu, 0xFu)` (`0x2CFE4..0x2CFF3`); the `[0x2CC70]` blank string is `mem + DSD(0x2CC70u)` (`0x80A18`).
  - `0x33578`: `/* PORT: 0x336AE 0x47370(new >> 24) reloads the language file; the port's game_string_table_load is idempotent and English-only (host file I/O), so a changed language is stored in field 0x29 but not shown (named gap, record §K11.3). */`
  - `0x30F54` repeats `0x30EB4`'s tail (`0x30F42..0x30F50`: `sound_voice(0x100)`) in its own body, with the note `/* 0x30FD7 je 0x30F42: 0x30EB4's tail, repeated */`.
Add `fn_register(0x2CACCu, ...)`, `fn_register(0x30EB4u, ...)` and `fn_register(0x30F54u, ...)` to `svcmenu_register`.

- [ ] **Step 5: Run the tests and confirm they pass**: `PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -2` → `all checks passed`.

- [ ] **Step 6: Mutation proof**: the value mask off by one bit (`0x2CD65` `dec eax` dropped); the index drawn without `+1`; `0x2CF00`'s Right wrap (`0x2D250 cmp ecx,ebx`) as `>`; Esc with CL = 1 returning the bits; `0x2C9CC` indexing `0xBC9A0`; `0x33578` skipping the bit-15 default; `0x2CACC` reading `+0` instead of `+4`. Each must fail a check; log to `$S/k11_t3_mutations.txt`.

- [ ] **Step 7: Gate** (the Task 2 Step 7 commands with `t3` names and the addresses `2CC74 2CD30 2CF00 2CACC 33578 30EB4 30F54 2C9CC 2C9E8`). Expected: exit 0, `ORACLES-UNCHANGED`, each count `1`. The frame dump is not needed: nothing on the `--check` path changed.

- [ ] **Step 8: Record §K11.3, PROGRESS.md, checkpoint**: commit (if authorised) `game: port the option editor and CONFIG/SOUND/MUSIC screens 0x2CC74..0x33578 (record §K11.3)`, staging `port/src/game/svcmenu.c port/src/game/svcmenu.h port/tests/test_game.c docs/superpowers/plans/2026-09-29-k11-service-menu-derivations.md docs/PROGRESS.md`.

---

### Task 4: Cycle 3 — ADJUST VOLUME and 2 PLAYER HANDICAP (5 functions, 2798 B)

Functions: `0x2F464` (38, Ghidra), `0x30728` (95), `0x30864` (1616), `0x30FE8` (333), `0x31138` (716).

**Files:** `port/src/game/svcmenu.{c,h}`, `port/tests/test_game.c` (`sm_check_volume`), record `§K11.4`. Also `port/src/game/attract.{c,h}`, for one signature only: `0x30864` reads `0x2C8F0(-1)`'s EAX (`0x308C6..0x308CA`, `jge`). That EAX is `EDX` = field `0x35` as read at `0x2C902` (`0x2C9B1 mov eax,edx`), and -1 when unset. The ported `attract_config_volumes_unscaled` returns `void`. Change it to `u32` and return that value. Existing callers keep ignoring it. Add one assertion on the return to `sm_check_volume`: field `0x35` seeded to 0x40 returns 0x40.

**Interfaces:**
- Consumes: `text_number_format(s32,u8*,s32,u32)`, `text_cursor_set`, `text_number_set`, `text_cursor_next_line`, `text_cells_release(_count)`, `text_glyph_at`, `config_bar_draw(s32,s32,s32)`, `u32 attract_config_volumes_unscaled(void)` (changed here from `void`), `config_voice_gate(s32)`, `sound_music_volume`, `sound_sfx_volume`, `sound_voice`, `config_keys_pack(u32)`, `config_keys_apply(u32)`.
- Produces: `void text_number_cont(s32 value, s32 width, u32 pad, u32 mode)` (0x2F464; Tasks 7 uses it); `void svc_volume_error(void)` (0x30728); `u32 svc_adjust_volume(u32 entry)` (0x30864); `void svc_handicap_row(u32 value, s32 row)` (0x30FE8, argument roles per §K11.4); `u32 svc_handicap(u32 entry)` (0x31138). Registers `0x30864`, `0x31138`.

- [ ] **Step 1: Derive (record §K11.4)**: disassemble the five (`k11_dx.py 2f464 28`, `30728 60`, `30864 650`, `30fe8 150`, `31138 2d0`). Pin these: (a) `0x2F464`'s register roles (EAX value, EDX width, EBX pad, ECX mode; `0x2F468..0x2F480`); (b) the `0xBD441..0xBD459` layout bytes as the image holds them; (c) `0x308B7..0x308CC`: `0x2C8F0(-1)` is `attract_config_volumes_unscaled`. A negative return (field `0x35` unset, -1) runs `0x30728` and leaves with EAX = -1 (`0x308D1`). Test this arm with field `0x35` = -1 and an Esc script; (d) the up/down/left/right arms of `0x30864` and the fields its exit writes (`0x30E68..0x30EA3`: `config_voice_gate`, `0x2DA0C` ×3 with their field ids); (e) `0x31138`'s handicap bytes in the packed record and the exit stores `DS_00107468`/`DS_0010746C` (`0x313CE`/`0x313D7`).

- [ ] **Step 2: Failing tests (≤ 30 frames)**:
```c
static void sm_check_volume(void)
{
    ch_text_setup();
    /* 0x2F464: "  123" at the cursor, width 5, pad 1 (spaces), mode 0x1000. */
    actors_reset();
    DSW(DS_00105F34) = 3u; DSW(DS_00105F34 + 2u) = 10u;          /* row 3, column 10 */
    text_number_cont(123, 5, 1u, 0x1000u);
    CHECK_EQ_INT((int)ch_cell(3, 10), 0);                          /* ' ' draws no cell */
    ch_expect(3, 12, '1', 0x1000u, "digit 1");
    ch_expect(3, 14, '3', 0x1000u, "digit 3");
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2u), 15);                  /* the cursor moved past 5 cells */
    /* 0x30728: Esc leaves; the error cells are released (record §K11.4). */
    static const sm_step_t esc2[] = { { 0u, 0u }, { 0x011Bu, 0u } };
    sm_begin(esc2, 2u);
    svc_volume_error();
    sm_end(2u, "the volume error waits for Esc");
    /* 26 characters centred: column (0x2B - 26) >> 1 = 8 (text_cursor_set's
     * col -1 rule). Dropping the 0x3077E release leaves this cell set. */
    CHECK_EQ_INT((int)ch_cell(6, 8), 0);
    /* 0x30864 / 0x31138 scripted per §K11.4 (d)/(e): seed the fields and DS_00107468/6C
       with sentinels, script Up/Down/Left/Right/Esc, assert the stored values. */
}
```
Check each literal against Step 1. Three are expectations from `text_cursor_set`/`0x2F280` semantics in `actors.c`: that a ' ' pad draws no cell, the cursor column after the number, and the centred column 8. Read those functions before finalising. If the raw differs, correct the test and record it. Frames: about 20.

- [ ] **Step 3: Run the tests and confirm they fail** (link errors).
- [ ] **Step 4: Port the five functions.** `0x2F464` uses a `u8 buf[0x14]` (the raw's `sub esp,0x14`). `0x30864`'s packed layout reads are `DSD(0xBD441u)` etc. (unaligned, as the raw). Its byte store is `DSB(0xBD458u)`. The voice calls are wired: `sound_voice` is ported, and this screen is not on K12's list, so no `PORT:` note is needed. Register `0x30864`/`0x31138`.
- [ ] **Step 5: Run the tests and confirm they pass.**
- [ ] **Step 6: Mutation proof**: `0x2F464`'s row -1 as 0; the width off by one; the `0x30728` exit on Enter; `0x30864`'s left and right swapped; a dropped `0x2DA0C` save; the `0x31138` exit stores swapped.
- [ ] **Step 7: Gate** (addresses `2F464 30728 30864 30FE8 31138`); then `python3 tools/port_progress.py` → the ported count goes up by 1 (`0x2F464`).
- [ ] **Step 8: Record §K11.4, PROGRESS.md, checkpoint**: commit (if authorised) `game: port ADJUST VOLUME and 2 PLAYER HANDICAP 0x2F464..0x31138 (record §K11.4)`.

---

### Task 5: Cycle 4 — MODIFY CONTROLS and TEST CONTROLS (9 functions, 3269 B)

Functions: `0x2EF48` (123), `0x2F48C` (47), `0x314A0` (219), `0x319B0` (184, Ghidra), `0x31A78` (265, Ghidra), `0x31B94` (212), `0x31C78` (401), `0x31F24` (1076), `0x32358` (742).

**Files:** `port/src/game/svcmenu.{c,h}`, `port/tests/test_game.c` (`sm_check_controls`), record `§K11.5`.

**Interfaces:**
- Consumes: `config_option_row(u32,u32,u8)` (0x31E28), `config_key_name(u32,u8,u32)` (0x3157C), `config_keys_pack`/`config_keys_apply`, `config_screen_wait(s32)`, `config_key_latched()`, `text_glyph_at`, `text_cells_release_count`, and the ported `0x2F314`/`0x2F20C` (look up their C names with `grep -rn "0x2F314\|0x2F20C" port/src/game/actors.c`).
- Produces: `s32 text_hex_format(u32 value, u8 *buf, s32 width, u32 space_pad)` (0x2EF48); `void text_hex_set(s32 col, s32 row, u32 value, s32 width, u32 pad, u32 mode)` (0x2F48C, `ret 8`: the last two are stack arguments); `void svc_stick_draw(s32 col, s32 row, u32 bits)` (0x314A0); `void svc_buttons_clear(u32 side, s32 col, s32 row)` (0x319B0); `void svc_buttons_draw(u32 side, u32 keys, s32 col, s32 row)` (0x31A78); `void svc_dirs_clear(u32 side)` (0x31B94); `void svc_dirs_draw(u32 side, u32 rec)` (0x31C78); `u32 svc_modify_controls(u32 entry)` (0x31F24); `u32 svc_test_controls(u32 entry)` (0x32358). Pin the exact argument roles in §K11.5 from the register use; change these signatures if the raw says otherwise and record it. Registers `0x31F24`, `0x32358`.

- [ ] **Step 1: Derive (record §K11.5)**: the four jump tables (`0x319A0`, `0x31A68`, `0x31B84`, `0x31C68`) with their case bodies; the diamond offsets (`lea` at `0x319B6..0x319D2`, `0x31A8E..0x31AA8`); `0x314A0`'s glyph choice from the bits (`0x314AB..0x314E1`: base 0x10, left 8, right 0x20, up `>>3`, down `<<3`). Record `0x2EF48`'s digit table `0x2EF10` = "0123456789ABCDEF" and its pad (`0x20` when the flag is set, `0x30` when clear, `0x2EF65..0x2EF75`). Record `0x31F24`'s device cycle and blink timing (`0x500BB` deltas) and `0x32358`'s hex rows ("ADDRESS    RAW DATA" `0x80BAC`, `^^^` `0x80BC0`, "DIAGS" `0x80BD4`, `DS_00107410` at `0x32363`).
- [ ] **Step 2: Failing tests (≤ 30 frames)**, concrete where the walk already pins values:
```c
static void sm_check_controls(void)
{
    ch_text_setup();
    u8 *b = mem + CH_DEST;
    memset(b, 0xEE, 0x20);
    CHECK_EQ_INT((int)text_hex_format(0x2Au, b, 4, 1u), 2);        /* width - digits */
    CHECK(memcmp(b, "  2A", 4) == 0 && b[4] == 0, "space-padded hex");
    memset(b, 0xEE, 0x20);
    (void)text_hex_format(0xBEEFu, b, 6, 0u);
    CHECK(memcmp(b, "00BEEF", 6) == 0 && b[6] == 0, "zero-padded hex");
    /* 0x314A0 / 0x319B0 / 0x31A78 / 0x31B94 / 0x31C78: cells from §K11.5 via ch_expect/ch_cell;
       0x31F24 scripted: Right on player 1 cycles DSW([DS_00101514] + 0x2D4) (seeded 0) to the
       §K11.5 next value, Esc exits and 0x1AE28 applies the record (config_keys_apply's DS mirror);
       0x32358 scripted: Esc through the latched key (0x3254B) after one frame; DS_00107410 seeded. */
}
```
- [ ] **Step 3: Run the tests and confirm they fail.**
- [ ] **Step 4: Port the nine functions.** Each jump table becomes a `switch` with case comments giving the table entry address. The tick reads are `DSD(DS_00101500)` with the comment `/* 0x31F9F 0x500BB */`. Register the two roots.
- [ ] **Step 5: Run the tests and confirm they pass.**
- [ ] **Step 6: Mutation proof**: a dropped hex pad flag; a swapped jump-table case; `0x314A0`'s up and down swapped; the device cycle's wrap; the Esc test on `0x0D`.
- [ ] **Step 7: Gate** (addresses `2EF48 2F48C 314A0 319B0 31A78 31B94 31C78 31F24 32358`); `port_progress` goes up by 2 (`0x319B0`, `0x31A78`).
- [ ] **Step 8: Record §K11.5, PROGRESS.md, checkpoint**: commit `game: port MODIFY/TEST CONTROLS 0x2EF48..0x32358 (record §K11.5)`.

---

### Task 6: Cycle 5 — CONFIGURE KEYBOARD (4 functions, 2230 B)

Functions: `0x19C60` (145), `0x19D34` (164), `0x2EBBC` (16), `0x19DF0` (1905).

**Files:** `port/src/game/svcmenu.{c,h}`, `port/tests/test_game.c` (`sm_check_keyboard`), record `§K11.6`.

**Interfaces:**
- Consumes: `config_keys_pack`/`config_keys_apply`, `config_key_name`, `config_screen_wait`, `text_cursor_set`.
- Produces: `void svc_key_slot_set(u32 slot, u16 code)` (0x19C60), `u32 svc_key_slot_get(u32 slot)` (0x19D34), `u32 svc_raw_key_take(void)` (0x2EBBC), `u32 svc_configure_keyboard(u32 entry)` (0x19DF0). Registers `0x19DF0`.

- [ ] **Step 1: Derive (record §K11.6)**: the slot map (record §0.5) re-read from `0x19C71..0x19CE9` and `0x19D45..0x19DCC`, plus the out-of-range arms (`0x19C63 ja 0x19CF0 ret`; `0x19D37 ja 0x19DD5 xor eax,eax`). Record `0x19DF0`'s screen (strings `0x17`/`0x16`, the label table `0xA2C2C..0xA2C48`), its cursor movement, and what the Esc (`0x1A3A1`) and Enter (`0x1A3AA`) arms store, plus the final `0x1AE28`. Record `0x19DD8` as dead (no port).
- [ ] **Step 2: Failing tests (≤ 25 frames)**:
```c
static void sm_check_keyboard(void)
{
    static const u32 slot_addr[16] = {
        0x100CAEu, 0x100CB4u, 0x100CB0u, 0x100CB2u, 0x100CB6u, 0x100CB8u, 0x100CBAu, 0x100CBCu,
        0x100CC0u, 0x100CC6u, 0x100CC2u, 0x100CC4u, 0x100CC8u, 0x100CCAu, 0x100CCCu, 0x100CCEu,
    };
    u8 saved[0x28];
    memcpy(saved, mem + 0x100CACu, 0x28);
    memset(mem + 0x100CACu, 0x5A, 0x28);
    for (u32 s = 0; s < 16u; s++) {
        svc_key_slot_set(s, (u16)(0x1100u + s));
        CHECK_EQ_INT((int)DSW(slot_addr[s]), (int)(0x1100u + s));
        CHECK_EQ_INT((int)svc_key_slot_get(s), (int)(0x1100u + s));
    }
    CHECK_EQ_INT((int)DSW(0x100CACu), 0x5A5A);           /* +0 is no slot */
    CHECK_EQ_INT((int)DSW(0x100CBEu), 0x5A5A);           /* 0x100CBE is no slot */
    svc_key_slot_set(16u, 0x7777u);                      /* above 15: nothing */
    for (u32 s = 0; s < 16u; s++) CHECK(DSW(slot_addr[s]) != 0x7777u, "slot 16 writes nothing");
    CHECK_EQ_INT((int)svc_key_slot_get(16u), 0);
    DSD(CH_KEY_WORD) = 0x1C0Du;
    CHECK_EQ_INT((int)svc_raw_key_take(), 0x1C0D);
    CHECK_EQ_INT((int)DSD(CH_KEY_WORD), 0);
    memcpy(mem + 0x100CACu, saved, 0x28);
    /* 0x19DF0 scripted per §K11.6: rebind one slot to 'a' (scan 0x1E) and leave. */
}
```
- [ ] **Step 3: Run the tests and confirm they fail.** **Step 4: Port** (two `switch`es over the 16 slots, each case commented with its table entry). **Step 5: Run the tests and confirm they pass.**
- [ ] **Step 6: Mutation proof**: slots 9 and 10 swapped (the non-monotone map `0x100CC6`/`0x100CC2`); the getter returns `-1` above 15; `0x2EBBC` without its clear; a wrong Enter/Esc arm in `0x19DF0`.
- [ ] **Step 7: Gate** (addresses `19C60 19D34 2EBBC 19DF0`). **Step 8: Record §K11.6, PROGRESS.md, checkpoint**: commit `game: port CONFIGURE KEYBOARD 0x19C60..0x19DF0 (record §K11.6)`.

---

### Task 7: Cycle 6 — STATISTICS pages 1 and 2 (6 functions, 1638 B)

Functions: `0x328B8` (101), `0x32F54` (65), `0x32F98` (192), `0x33458` (261), `0x33058` (470), `0x33230` (549).

**Files:** `port/src/game/svcmenu.{c,h}`, `port/tests/test_game.c` (`sm_check_stats`), record `§K11.7`.

**Interfaces:**
- Consumes: `text_number_cont` (Task 4), `config_credit_zero()` (0x2CA78), `config_field_get/set`, `gfx_screen_reset(u32)` (0x52106), `config_key_latched`, `config_input_poll`.
- Produces: `void svc_draw_mmss(u32 secs, u32 w)` (0x328B8), `u32 svc_stats_avg(void)` (0x32F54), `void svc_stats_play(s32 col, s32 row, u32 mode)` (0x32F98, roles per §K11.7), `u32 svc_stats_rows(s32 row)` (0x33458), `void svc_stats_page1(u32 a)` (0x33058), `void svc_stats_page2(u32 a)` (0x33230).

- [ ] **Step 1: Derive (record §K11.7)**: the code-object tables `0x32644` (5 × `{id, field}`), `0x32674` and `0x326C4`, read from the image; `0x33058`'s exits (`0x3306B..0x330A7`); `0x33230`'s clear combination and the fields it writes; `0x32F54`'s formula `60 * (2*field5 + field4) / (c & 0xFFFF)` re-read from `0x32F61..0x32F90`. Where `gfx_screen_reset`'s argument comes from (`0x330B1`, EAX at that point) is pinned or a named gap.
- [ ] **Step 2: Failing tests (≤ 25 frames)**. `0x328B8` concretely: cursor row 4, column 5; `svc_draw_mmss(125, 5)` → "2:05" (minutes width 2, pad ' '; then ':'; then seconds width 2, pad '0'). The cells are `ch_expect(4, 6, '2', 0xF000u, ..)`, `(4, 7, ':')`, `(4, 8, '0')` and `(4, 9, '5')`; confirm each against `0x2EFD4`'s pad table before finalising. `0x32F54` concretely: seed field 4 = 3, field 5 = 2 and a `config_credit_zero` result c = 7 (seed whatever `0x2CA78` reads, per `config.c`). The expected value is `60 * 7 / 7 = 60`. With c = 0 it returns 0. Pages 1 and 2 are scripted: one Enter (the latched key `0x0D`) leaves page 1 after one frame. Page 2's clear combination zeroes the §K11.7 fields; seed them with sentinels.
- [ ] **Step 3: Run the tests and confirm they fail.** **Step 4: Port** (`0x33058`'s dead `field == 0x24` arm is ported as written, with `/* 0x33169: no row of 0x32644 has field 0x24 (record §K11.7) */`). **Step 5: Run the tests and confirm they pass.**
- [ ] **Step 6: Mutation proof**: `/ 60` as `/ 64`; the seconds pad as ' '; `0x32F54`'s `c == 0` test removed (it divides by zero, so it is caught as a crash); the page-1 exit on Esc only.
- [ ] **Step 7: Gate** (addresses `328B8 32F54 32F98 33458 33058 33230`). **Step 8: Record §K11.7, PROGRESS.md, checkpoint**: commit `game: port STATISTICS pages 1-2 0x328B8..0x33230 (record §K11.7)`.

---

### Task 8: Cycle 7 — the histograms and the STATISTICS entry (7 functions, 2834 B)

Functions: `0x2E218` (46), `0x2E11C` (99), `0x2E248` (921), `0x2E5E4` (847), `0x32BDC` (887), `0x33560` (24), `0x2CAC0` (10).

**Files:** `port/src/game/svcmenu.{c,h}`, `port/tests/test_game.c` (`sm_check_hist`), record `§K11.8`. Also `port/src/game/config.{c,h}`, only to export `config_storage_touch`. It is the declared no-op `0x2D4EC`, currently `static` at `config.c:63`. Drop `static` and declare it in `config.h` with its existing comment. The body is unchanged.

**Interfaces:**
- Consumes: Task 7's pages; `config_storage_touch(u32)` (0x2D4EC, the declared no-op); `text_number_format`; `mem_fill`.
- Produces: `u32 audit_digits(s32 v)` (0x2E218), `u32 audit_hist_clear(u32 group)` (0x2E11C), `u32 audit_hist_format(...)` (0x2E248, `ret 8`, roles per §K11.8), `u32 audit_hist_line(...)` (0x2E5E4), `void svc_stats_hist(u32 a)` (0x32BDC), `u32 svc_statistics(u32 a)` (0x33560), `u32 svc_statistics_entry(u32 entry)` (0x2CAC0). Registers `0x2CAC0`.

- [ ] **Step 1: Derive (record §K11.8)**: the descriptors `0x2D414` (3 × 0x10: template ptr, two counts, flags `0x140001`/`0x140002`/`0x70004`) and `0x2D444` (6 × 8: size word `+2`, pointer `+4`). Record the state block `DS_00105D64..` fields that `0x2E248` writes (`+0x18`, `+0x1C`, `+0x20`, ...), the tab-column parse (`0x2E2DC..0x2E2F8`, stop at 0 or `\t`), and `0x2E5E4`'s line layout. Record how `0x32BDC` pages through the three histograms and its clear combination.
- [ ] **Step 2: Failing tests (≤ 20 frames)**, concrete for the walk-pinned pieces:
```c
static void sm_check_hist(void)
{
    static const struct { s32 v; u32 n; } dg[] = {
        { 0, 1 }, { 9, 1 }, { 10, 2 }, { 99, 2 }, { 100, 3 }, { 999999999, 9 },
        { 1000000000, 10 }, { 0x7FFFFFFF, 10 }, { -5, 1 },
    };
    for (u32 i = 0; i < sizeof dg / sizeof dg[0]; i++)
        CHECK_EQ_INT((int)audit_digits(dg[i].v), (int)dg[i].n);   /* 0x2E227 jl is signed */
    u8 saved[48]; memcpy(saved, mem + 0x105ECDu, 48);     /* 0x105ECD..0x105EFC */
    const u8 s_dirty = DSB(DS_00105DD8);
    memset(mem + 0x105ECDu, 0xAA, 47);
    DSB(0x105EFCu) = 0x5Bu;                                /* one past group 2 (DS_00105EFC) */
    DSB(DS_00105DD8) = 0x00u;
    CHECK_EQ_INT((int)audit_hist_clear(0u), 0);
    for (u32 i = 0; i < 20u; i++) CHECK_EQ_INT((int)DSB(0x105ECDu + i), 0);   /* 20 bytes */
    CHECK_EQ_INT((int)DSB(0x105EE1u), 0xAA);                                  /* group 1 untouched */
    CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x08);                                /* bit (0+3) */
    CHECK_EQ_INT((int)audit_hist_clear(2u), 0);
    for (u32 i = 0; i < 7u; i++) CHECK_EQ_INT((int)DSB(0x105EF5u + i), 0);    /* 7 bytes */
    CHECK_EQ_INT((int)DSB(0x105EFCu), 0x5B);                                  /* past the end */
    CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x28);                                /* + bit 5 */
    memset(mem + 0x105ECDu, 0xAA, 47);
    CHECK_EQ_INT((int)audit_hist_clear(3u), -1);                              /* 0x2E123 jb */
    CHECK_EQ_INT((int)DSB(0x105ECDu), 0xAA);
    CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x28);                                /* untouched */
    memcpy(mem + 0x105ECDu, saved, 48); DSB(DS_00105DD8) = s_dirty;
    /* 0x2E248/0x2E5E4 over descriptor 0 with seeded counters: the §K11.8 values.
       0x2CAC0 scripted: Enter frames walk page 1 -> page 2 -> histograms -> out;
       the frame count is §K11.8's, at most 20. */
}
```
- [ ] **Step 3: Run the tests and confirm they fail.** **Step 4: Port.** `0x2E11C` uses `mem_fill(ptr, 0, size)` with the note `/* PORT: 0x2E16C 0x61A70 is the WATCOM memset (runtime) */`. It then calls `config_storage_touch(g + 3)`. Register `0x2CAC0`. **Step 5: Run the tests and confirm they pass.**
- [ ] **Step 6: Mutation proof**: `audit_digits` with `>` for `>=` (10 → 1); the dirty bit shift without `+3`; group 3 accepted; `0x2E248`'s tab stop removed; `0x33560` skipping page 2.
- [ ] **Step 7: Gate** (addresses `2E218 2E11C 2E248 2E5E4 32BDC 33560 2CAC0`); the §K9.9 note: `0x2E11C` is a writer of `0x105ECD..0x105EFB`. It writes only zeros, and only from the options menu, which no oracle reaches. Add one sentence to record §K11.8 revisiting §K9.9, and a pointer from the ledger's `2DF8C` row.
- [ ] **Step 8: Record §K11.8, PROGRESS.md, checkpoint**: commit `game: port the STATISTICS histograms and entry 0x2E218..0x2CAC0 (record §K11.8)`.

---

### Task 9: Reconciliation — ledger, PROGRESS, README count

**Files:** `docs/PROGRESS.md`, `README.md`, `docs/superpowers/plans/2026-09-29-all-gaps-ledger.md`, record (`§K11.9`).

- [ ] **Step 1: Closure re-check**

```bash
cd $S && python3 k11_closure.py > k11_closure_final.txt
grep -E "^[0-9a-f]+ U" k11_closure_final.txt | grep -v "^38b18\|^500bb\|^1ae28" ; echo "unported left: $?"
```
Expected: no lines (`grep` exit 1). The walker counts a function as ported by its header, so all 50 now read `P`.

- [ ] **Step 2: Counts**

```bash
python3 tools/port_progress.py
```
Expected: the ported count is the Task 1 value + 3 (`0x2F464`, `0x319B0`, `0x31A78`), plus whatever parallel clusters added since, e.g. `765 1203 64` from `762 1203 63`. Update the README title `— Reverse Engineering NN%` and the "**NN%** of the original's 1203 real functions" line to the printed numbers.

- [ ] **Step 3: Ledger rows**: mark the §B.1 rows `0x2F464`, `0x319B0` and `0x31A78` **closed** with their record sections. Add a closure note to §B.2 "Service menu": all 11 callbacks plus the 7 START MENU setters, 50 functions, record §K11.2..§K11.8. Mark §F row 18 **closed**. Add the named gaps to §E: the language reload, the joystick device choice, the play-time fields and the audit counters that stay 0 (record §0.6).
- [ ] **Step 4: PROGRESS.md**: one closing paragraph covering the cycle count, 50 functions and 15 304 B, the three Ghidra functions, the test seam, the frame budget used, and the named gaps.
- [ ] **Step 5: Final gate**

```bash
make verify 2>&1 | tee $S/k11_final_verify.txt; echo verify-exit=${PIPESTATUS[0]}
bash $S/orlines.sh $S/k11_final_verify.txt > $S/k11_final_or.txt && diff $S/k11_t1_or.txt $S/k11_final_or.txt && echo ORACLES-UNCHANGED
grep -rn 'TODO(verify)' port/src/game/svcmenu.c | wc -l
```
Expected: exit 0, `ORACLES-UNCHANGED`, `0` (any doubt left is a named gap in the record, not a marker).

- [ ] **Step 6: Checkpoint**: commit (if authorised) `docs: K11 service menu closed (record §K11.9)` with `docs/PROGRESS.md README.md docs/superpowers/plans/2026-09-29-all-gaps-ledger.md docs/superpowers/plans/2026-09-29-k11-service-menu-derivations.md`.
