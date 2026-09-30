# Gameplay U2 — Port Replay v2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An env-gated port driver that replays a U1 port script v2 keyed by the master frame counter `DS_000EF6DC`, dumps every displayed frame as indices plus DAC, and writes one state-trace line per master-loop iteration in the capture's field names.

**Architecture:** A new driver `test_gp_replay` in `port/tests/test_game.c`, beside the K11 driver (which stays untouched), selected by `PR_GP_DUMP`/`PR_GP_SCRIPT`. It runs `game_init()` once, steps `game_loop_step()`, and before the iteration that raises the counter to `F` queues that frame's BIOS keys (`input_push`) and applies its key bitmap (the K11 driver's `k11_key_bits`). A pump hook, the loader-screen hook and an after-iteration call dump each new displayed image to `frame_%05d.ipx`; `trace.txt` holds the `T` lines U3 compares with the capture's `S` records. No production file changes.

**Tech Stack:** C11, the port's CMake build, `run_tests` (`CHECK`/`CHECK_EQ_INT`), `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md` §4.2 (this unit), §4.1 (the script v2 format), §5.

**Derivation record:** `docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md`, sections `§G.9..§G.12` (U1 used `§G.0..§G.8`).

**Where to run.** Main checkout, branch `gameplay-ground-truth`, after U1. `S=/tmp/gameplay-u2`. Every command assumes `cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse`.

---

## Global Constraints

- Spec §5: "Raw wins; never a fitted constant; a value that cannot be pinned is a named gap with its evidence. Harness values (…, `GP_LOOP_SLACK`) are named as harness values with their source, never presented as game values."
- Spec §5: "`make verify` is the gate after every task; the oracle lines equal the U1 Task 0 baseline; the enforced front-end oracle, the demo-fight and attract cycle-2 ratchets and the K11 oracles stay green."
- Spec §5: "C only `CHECK`/`CHECK_EQ_INT`; every assertion can fail and each new test is shown failing under a named mutation; the driver runs alone (`game_init()` once)."
- Spec §4.2: "No production code changes: the seams exist (`host_set_key_bits_override`, `host_set_pump_hook`, `res_set_screen_hook`, `host_set_fault_hook`)." `git diff` of `port/src` must stay empty in U2.
- AGENTS.md: "**`game_init()` may run only once per process** … Any test calling it must be env-gated, and `run_tests.c` must run that driver alone." Register the driver only in `TEST_DRIVERS`, before `test_restart_drive` (which stays last, commit `3504dfc`).
- AGENTS.md: "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions … never assert an unseeded BSS-zero."
- AGENTS.md: "**`port/src/symbols.h` is generated** … where the generator emits no name, use a local `#define` with the raw address." `DS_0010810D` has no name: use `GP_DS_0010810D 0x0010810Du`.
- AGENTS.md: "Commit style: `<area>: <what changed>`. **Never `git add -A`**." Trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **Keys applied one iteration off.** The script's `key F` must be queued before the iteration that raises the counter to `F`, the same rule as K11's `enter_frame - 1`. Task 1's red/green smoke and the mutation `gp_step[gp_next].f <= f + 1u` → `<= f` pin it (`mode_after` stays 3).
2. **A malformed script replayed anyway.** An unsorted script, or one whose first step is not the Enter at `enter_frame`, must fail the parse check rather than replay a shifted input. Task 1 Step 6 (b). A key the port leaves queued after its iteration (the port consumed it later than the original) is logged to `gp.log` as `left-queued`, a divergence for U3/U4 to see, not a driver failure.
3. **The dump missing a present.** A present inside an iteration that never pumps (a catch-up iteration) must still be dumped: Task 2 dumps after every iteration as well as at each pump, and its check compares `frames.txt` against the trace's mode changes.
4. **A trace line in different names or widths from the capture's `S`.** U3 parses both with `gp_session.parse`; Task 2 Step 5 parses the port's `trace.txt` with it and checks every `TRACE_FIELDS` name is present.
5. **The driver leaking into the unit run or moving an oracle line.** Task 3's gate: `ORACLES-EQUAL` and the K11 dumps byte-identical to Task 0's.

---

### Task 1: The driver: script v2, frame-keyed keys and bits, the Enter checks

**Files:**
- Modify: `port/tests/test_game.c` (a new section after `test_k11_oracle`, before `/* ---- named-gaps B: the 0x65431 soft restart`)
- Modify: `port/tests/test.h` (`TEST_DRIVERS`)
- Modify: record §G.9

**Interfaces:**
- Consumes: `k11_key_bits(u32)`, `k11_fnv(u32, const u8 *, u32)` (static, same file, defined above); `game_set_game_dir`, `game_init`, `game_loop_step`, `game_restart_arm`, `actors_pin_anim_tick_zero`, `input_push`, `host_set_pump_hook`, `res_set_screen_hook`, `host_set_fault_hook`, `host_set_key_bits_override`
- Produces: `int test_gp_replay(void)` under `PR_GP_DUMP` (dump dir) and `PR_GP_SCRIPT` (v2 script); files `<dir>/gp.log`

- [ ] **Step 1: Baseline for this unit**

```bash
S=/tmp/gameplay-u2; mkdir -p $S
cmake --build build 2>&1 | tail -1
make verify > "$S/t1_base.txt" 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|k11_compare)' "$S/t1_base.txt" | diff - /tmp/gameplay-u1/or_base.txt && echo ORACLES-EQUAL
rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l
for s in walk menuesc; do (cd /tmp/pr_k11_dump/$s && shasum -a 256 frame_*.raw) > "$S/k11_$s.sha256"; done
```

Expected: `verify-exit=0`, `ORACLES-EQUAL` (if `/tmp/gameplay-u1/or_base.txt` is gone, regenerate it from this run and say so in §G.9), the assertion-site count (record it), two sha256 lists (empty if the K11 captures are absent; record which).

- [ ] **Step 2: Write the smoke script (the failing test)**

`$S/smoke.script` — the K11 smoke's Enter frame (record named-gaps-a §A.3.4: `enter_frame 300`, state `0000`), then MAIN MENU "Start" and START MENU row 0 (spec §3.3), then 300 frames:

```
# gp port script v2: smoke (not a capture)
enter_frame 300
enter_state FFFF
key 300 1C 0D
key 450 1C 0D
key 600 1C 0D
end 900
```

`enter_state FFFF` is deliberately wrong (the red run). Register the driver and add a stub that fails, so the run is red for the right reason. In `port/tests/test.h`:

```c
#define TEST_DRIVERS(X)                       \
    X(test_attract,  "PR_ATTRACT_DUMP")       \
    X(test_title,    "PR_TITLE_DUMP")         \
    X(test_frontend, "PR_FRONTEND_DUMP")      \
    X(test_k11_oracle, "PR_K11_DUMP")    \
    X(test_gp_replay, "PR_GP_DUMP")      \
    X(test_restart_drive, "PR_RESTART")
```

- [ ] **Step 3: Write the driver**

Insert after `test_k11_oracle`'s closing brace:

```c
/* ---- the gameplay replay driver (gameplay ground truth U2, spec
 * docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md §4.2,
 * record §G.9) ----
 * PR_GP_DUMP=<dir> PR_GP_SCRIPT=<file> (tools/gp_session.py port-script, v2).
 * As the K11 driver it runs game_init() and the master loop from boot with
 * the title pin's opcode-8 mirror. Before the iteration that raises the frame
 * counter DS_000EF6DC to F it queues the script's `key F` lines and applies
 * its last `bits F` (spec §3.1: 0x500C4 samples the bitmap before 0x24CDB
 * increments the counter; the capture's S record of F shows what iteration F
 * read). It stops after the iteration that raises the counter to `end`. */
#define GP_LOOP_SLACK 600u          /* harness bound past `end` (record §G.9), not a game value */
#define GP_STALL_PUMPS 200000u      /* the K11 driver's pump-stall guard */
#define GP_DS_0010810D 0x0010810Du  /* no symbols.h name: the winner-side byte */
typedef struct { char op; u32 f, a, b; } GpStep;
static GpStep *gp_step;
static u32 gp_n, gp_next, gp_nkeys, gp_keys_sent, gp_enter_frame, gp_enter_state, gp_end;
static u32 gp_dumped, gp_hash_last, gp_idle_pumps, gp_missed, gp_iters;
static int gp_armed, gp_done, gp_failed;
static char gp_dir[1024];
static FILE *gp_log, *gp_frames, *gp_trace;
static jmp_buf gp_end_jb;

static int gp_parse(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) return 0;
    char line[256];
    u32 lines = 0;
    while (fgets(line, sizeof line, f) != NULL) lines++;
    rewind(f);
    gp_step = calloc(lines + 1u, sizeof *gp_step);   /* PORT: sized from the script, no fixed cap */
    if (gp_step == NULL) { fclose(f); return 0; }
    int have_frame = 0, have_state = 0, ok = 1;
    u32 last_f = 0u;
    gp_n = gp_nkeys = 0u;
    while (fgets(line, sizeof line, f) != NULL) {
        unsigned a = 0u, b = 0u, c = 0u;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "enter_frame %u", &a) == 1) { gp_enter_frame = a; have_frame = 1; continue; }
        if (sscanf(line, "enter_state %x", &a) == 1) { gp_enter_state = a; have_state = 1; continue; }
        GpStep *s = &gp_step[gp_n];
        if (sscanf(line, "key %u %x %x", &a, &b, &c) == 3) { s->op = 'k'; gp_nkeys++; }
        else if (sscanf(line, "bits %u %x", &a, &b) == 2) s->op = 'b';
        else if (sscanf(line, "end %u", &a) == 1) { s->op = 'e'; gp_end = a; }
        else { ok = 0; break; }
        if (a < last_f) { ok = 0; break; }        /* the generator sorts by frame */
        last_f = a;
        s->f = a; s->a = b; s->b = c;
        gp_n++;
    }
    fclose(f);
    return ok && have_frame && have_state && gp_n > 0u && gp_step[gp_n - 1u].op == 'e'
        && gp_step[0].op == 'k' && gp_step[0].f == gp_enter_frame;
}

/* Applies every step for the iteration about to run (the one that raises the
 * counter from `f` to f + 1). A step whose frame is already behind is a miss. */
static void gp_apply(u32 f)
{
    while (gp_next < gp_n && gp_step[gp_next].f <= f + 1u) {
        const GpStep *s = &gp_step[gp_next];
        if (s->f < f + 1u) {
            gp_missed++;
            fprintf(gp_log, "missed %c at f=%u (counter %u)\n", s->op, s->f, f);
        }
        if (s->op == 'k') {
            input_push((u8)s->a, (u8)s->b);
            fprintf(gp_log, "key %u f=%u scan=%02X ascii=%02X mode=%04X\n",
                    gp_keys_sent, s->f, s->a, s->b, DSW(DS_00104B00));
            gp_keys_sent++;
        } else if (s->op == 'b') {
            k11_key_bits(s->a);
            fprintf(gp_log, "bits f=%u kb=%04X\n", s->f, s->a);
        } else {
            break;                                   /* 'e' is checked after the iteration */
        }
        gp_next++;
    }
}

static void gp_fault(u32 exc, u32 eip)
{
    fprintf(gp_log, "fault %02X at %08X\n", (unsigned)exc, (unsigned)eip);
    gp_failed = 1;
    longjmp(gp_end_jb, 1);
}

static void gp_hook(void *ctx)
{
    (void)ctx;
    if (!gp_armed || gp_done || gp_failed) return;
    if (++gp_idle_pumps > GP_STALL_PUMPS) {
        printf("FAIL %s:%d: the gp replay stalled at f=%u\n", __FILE__, __LINE__, (unsigned)DSW(DS_000EF6DC));
        exit(1);
    }
}

int test_gp_replay(void)
{
    const int before = g_failures;
    const char *dump = getenv("PR_GP_DUMP");
    const char *script = getenv("PR_GP_SCRIPT");
    if (dump == NULL || dump[0] == '\0') return 0;
    CHECK(script != NULL && script[0] != '\0' && gp_parse(script),
          "PR_GP_SCRIPT names a parsable gp port script v2");
    if (g_failures != before) return g_failures - before;
    snprintf(gp_dir, sizeof gp_dir, "%s", dump);
    mkdir(dump, 0777);
    char p[1200];
    snprintf(p, sizeof p, "%s/gp.log", dump);
    gp_log = fopen(p, "w");
    CHECK(gp_log != NULL, "the gp log opens");
    if (gp_log == NULL) return g_failures - before;

    const char *dir = getenv("PR_GAME_DIR");
    if (dir == NULL || dir[0] == '\0') dir = "data/game/C";
    game_set_game_dir(dir);
    game_init();
    actors_pin_anim_tick_zero(1);
    host_set_pump_hook(gp_hook, NULL);
    host_fault_hook_fn prev_fault = host_set_fault_hook(gp_fault);

    /* Sentinels: none is a mode, frame or state the checks below accept. */
    static u32 mode_before, mode_after, frame_after, state_after;
    mode_before = 0xFFFFu; mode_after = 0xFFFFu; frame_after = 0xFFFFFu; state_after = 0xFFFFFu;
    const u32 limit = gp_end + GP_LOOP_SLACK;
    if (setjmp(gp_end_jb) == 0)
    for (gp_iters = 0; gp_iters < limit && !gp_done && !gp_failed; gp_iters++) {
        const u32 f = DSW(DS_000EF6DC);
        const int enter_now = !gp_armed && f + 1u == gp_enter_frame;
        if (enter_now) mode_before = DSW(DS_00104B00);
        if (gp_armed || enter_now) gp_apply(f);
        if (enter_now) gp_armed = 1;
        gp_idle_pumps = 0u;
        game_loop_step();                            /* exactly one game_loop iteration */
        if (gp_armed && input_has_key())
            fprintf(gp_log, "left-queued after f=%u\n", (unsigned)DSW(DS_000EF6DC));
        if (enter_now) {
            mode_after = DSW(DS_00104B00);
            frame_after = DSW(DS_000EF6DC);
            state_after = DSW(DS_000F0A64);
        }
        if (gp_armed && DSW(DS_000EF6DC) >= gp_end) gp_done = 1;
    }
    (void)game_restart_arm(NULL);
    host_set_pump_hook(NULL, NULL);
    (void)host_set_fault_hook(prev_fault);
    k11_key_bits(0u);
    fclose(gp_log);
    free(gp_step);

    CHECK(gp_armed, "the loop reached the script's Enter frame");
    CHECK_EQ_INT((int)mode_before, 3);               /* 0x24ECF: the Enter arm needs mode 3 */
    CHECK_EQ_INT((int)mode_after, 0x27);             /* 0x24EE0 */
    CHECK_EQ_INT((int)frame_after, (int)gp_enter_frame);
    CHECK_EQ_INT((int)state_after, (int)gp_enter_state);
    CHECK_EQ_INT((int)gp_keys_sent, (int)gp_nkeys);
    CHECK_EQ_INT((int)gp_missed, 0);
    CHECK(gp_done, "the gp script ran to its end frame");
    CHECK(!gp_failed, "no fault ended the gp replay");
    return g_failures - before;
}
```

Notes for the implementer: `game_restart_arm(NULL)` mirrors the K11 driver (a `longjmp` out of `game_loop()` leaves its restart point armed). The frame counter is a word; scripts stay below `0x10000` frames (the idle run ends near `0x22BA`, spec §3.5).

- [ ] **Step 4: Run red**

```bash
cmake --build build 2>&1 | tail -1
rm -rf $S/smoke; PR_GP_DUMP=$S/smoke PR_GP_SCRIPT=$S/smoke.script PR_GAME_DIR=data/game/C ./build/run_tests; echo "exit=$?"
```

Expected: `FAIL …/port/tests/test_game.c:<line>: 0 != 65535` (the `enter_state` check), `FAILURES: 1`, `exit=1`. (The planner compiled Tasks 1–2's code as written in a scratch copy of the port and ran this: that exact line; the build had no warning.)

- [ ] **Step 5: Run green**

Set `enter_state` to the value the red run printed (the K11 smoke measured `0000`; if the red line shows another value, use it and record why). Then:

```bash
rm -rf $S/smoke; PR_GP_DUMP=$S/smoke PR_GP_SCRIPT=$S/smoke.script PR_GAME_DIR=data/game/C ./build/run_tests; echo "exit=$?"
cat $S/smoke/gp.log
```

Expected (the planner's scratch run): `all checks passed`, `exit=0`; `gp.log` is exactly

```
key 0 f=300 scan=1C ascii=0D mode=0003
key 1 f=450 scan=1C ascii=0D mode=0027
key 2 f=600 scan=1C ascii=0D mode=0027
```

with no `left-queued` line. Record the lines in §G.9. (What the port does after the third key — mode `0x2D`, the wipe — is observed in Task 2's trace, not asserted here.)

- [ ] **Step 6: Mutation proofs**

(a) In `gp_apply`'s `while` condition use `gp_step[gp_next].f <= f` (keys one iteration late): the Enter is not queued at the arm, so expected `FAIL … 3 != 39` (`mode_after`), exit 1. (b) A copy of the smoke script with `key 299 1C 0D` inserted after `key 300 1C 0D`: expected the parse `CHECK` to fail (`FAILURES: 1`). Restore; green. Record both verbatim in §G.9. `gp_missed` is defensive: each `game_loop_step()` raises the counter by exactly one and the parser rejects unsorted steps, so no script can trip it; record it under "Not tested" with that reason.

- [ ] **Step 7: Commit**

```bash
git add port/tests/test_game.c port/tests/test.h docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
tests: gp replay driver, frame-keyed keys and bits (PR_GP_DUMP, record §G.9)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Dump every displayed frame and the per-iteration trace

**Files:**
- Modify: `port/tests/test_game.c` (the gp section)
- Modify: record §G.10

**Interfaces:**
- Consumes: Task 1's driver; `gfx_display()`, `gfx_dac`, `res_set_screen_hook`
- Produces (U3 relies on these exactly): `<dir>/frame_%05d.ipx` (64 000 index bytes then 768 DAC bytes); `<dir>/frames.txt` lines `%05u f=%04X tick=%08X mode=%04X`; `<dir>/trace.txt` lines `T f=%04X mode=%04X st=%04X tick=%08X t508=%08X t50c=%08X raw=%08X pad=%08X new=%08X held=%08X e0=%04X e2=%04X rng=%08X cred=%08X fp=%02X b1d=%02X b1f=%02X b25=%02X w10d=%02X cnt=%02X s0_52=%02X s0_54=%02X s0_5a=%02X s1_52=%02X s1_54=%02X s1_5a=%02X ent=%08X`, one per iteration after the Enter arm

- [ ] **Step 1: The failing check**

Add to `test_gp_replay`'s checks (after `CHECK(!gp_failed …)`):

```c
    CHECK(gp_dumped > 1u, "the gp frames were written");
    CHECK_EQ_INT((int)gp_trace_lines, (int)(gp_iters_armed));
```

and, where `gp.log` is opened, open `frames.txt` and `trace.txt` next to it with one check:

```c
    snprintf(p, sizeof p, "%s/frames.txt", dump);
    gp_frames = fopen(p, "w");
    snprintf(p, sizeof p, "%s/trace.txt", dump);
    gp_trace = fopen(p, "w");
    CHECK(gp_frames != NULL && gp_trace != NULL, "the gp dump files open");
    if (gp_frames == NULL || gp_trace == NULL) return g_failures - before;
```

and declare `static u32 gp_trace_lines, gp_iters_armed;` beside the other statics. Build and run the green smoke of Task 1: expected `FAIL … gp frames were written` (nothing dumps yet) — the red run.

- [ ] **Step 2: Implement the dump and the trace**

Add above `gp_fault` (so `gp_hook` below can call `gp_dump_if_new`):

```c
static void gp_dump_if_new(void)
{
    const u8 *fb = gfx_display();
    if (fb == NULL) fb = mem + DSD(DS_000E87A0);
    const u32 h = k11_fnv(k11_fnv(2166136261u, fb, 64000u), &gfx_dac[0][0], 768u);
    if (gp_dumped > 0u && h == gp_hash_last) return;
    char path[1200];
    snprintf(path, sizeof path, "%s/frame_%05u.ipx", gp_dir, gp_dumped);
    FILE *f = fopen(path, "wb");
    int ok = f != NULL && fwrite(fb, 1, 64000u, f) == 64000u && fwrite(&gfx_dac[0][0], 1, 768u, f) == 768u;
    if (f != NULL && fclose(f) != 0) ok = 0;
    if (!ok) { gp_failed = 1; return; }
    fprintf(gp_frames, "%05u f=%04X tick=%08X mode=%04X\n", gp_dumped,
            (unsigned)DSW(DS_000EF6DC), (unsigned)DSD(DS_00101500), (unsigned)DSW(DS_00104B00));
    gp_hash_last = h;
    gp_dumped++;
}

static void gp_loader(void)
{
    if (gp_armed && !gp_done && !gp_failed) gp_dump_if_new();
}

/* One T line per master-loop iteration, the capture's S field names and
 * widths (tools/gp_session.py SNAP_FIELDS, spec §4.1). */
static void gp_trace_line(void)
{
    fprintf(gp_trace,
            "T f=%04X mode=%04X st=%04X tick=%08X t508=%08X t50c=%08X raw=%08X pad=%08X new=%08X held=%08X "
            "e0=%04X e2=%04X rng=%08X cred=%08X fp=%02X b1d=%02X b1f=%02X b25=%02X w10d=%02X cnt=%02X "
            "s0_52=%02X s0_54=%02X s0_5a=%02X s1_52=%02X s1_54=%02X s1_5a=%02X ent=%08X\n",
            DSW(DS_000EF6DC), DSW(DS_00104B00), DSW(DS_000F0A64), DSD(DS_00101500),
            DSD(DS_00101508), DSD(DS_0010150C), DSD(DS_000E1C30), DSD(DS_000E1C34),
            DSD(DS_001088E4), DSD(DS_001088D8), DSW(DS_001088E0), DSW(DS_001088E2),
            DSD(DS_000EF6D8), DSD(DS_00105C00), DSB(DS_00105D60), DSB(DS_00104B1D),
            DSB(DS_00104B1F), DSB(DS_00104B25), DSB(GP_DS_0010810D), DSB(DS_00108110),
            DSB(DS_00107802), DSB(DS_00107804), DSB(DS_0010780A),
            DSB(DS_00107896), DSB(DS_00107898), DSB(DS_0010789E), DSD(DS_0010741C));
    gp_trace_lines++;
}
```

In `gp_hook`, after the stall guard: `gp_dump_if_new();`. In `test_gp_replay`: install `res_set_screen_hook(gp_loader)` after the pump hook and clear it with `res_set_screen_hook(NULL)` at the end; after `game_loop_step()`:

```c
        if (gp_armed) {
            gp_iters_armed++;
            gp_trace_line();
            gp_dump_if_new();                        /* a present in an iteration that never pumped */
        }
```

Close both files after the loop. (A `printf`-width note for the implementer: every `DSW` is `u16` and every `DSB` `u8`; cast to `unsigned` in the argument list if the compiler warns.)

- [ ] **Step 3: Run green and inspect**

```bash
cmake --build build 2>&1 | tail -1
rm -rf $S/smoke; PR_GP_DUMP=$S/smoke PR_GP_SCRIPT=$S/smoke.script PR_GAME_DIR=data/game/C ./build/run_tests; echo "exit=$?"
ls $S/smoke | head; ls $S/smoke/*.ipx | wc -l; wc -l < $S/smoke/trace.txt
stat -f %z $S/smoke/frame_00000.ipx
awk '{print $3}' $S/smoke/trace.txt | uniq -c
```

Expected (the planner's scratch run): `all checks passed`; 145 `.ipx` files of `64768` bytes; `trace.txt` holds 601 lines, `f=012C` .. `f=0384` (each line is the counter the iteration raised: the Enter's iteration to 300 through the end's to 900); the mode runs are `300 mode=0027`, `1 mode=002D`, `18 mode=001A`, `18 mode=001B`, `264 mode=0010`. Record them verbatim in §G.10: it is the port's first view of spec §3.5's table (the capture's `0x1A` also lasts 18 frames, `0x248..0x259`).

- [ ] **Step 4: The frame format against the RGB writer**

The first dumped frame expanded through its DAC must equal what `fe_write_frame` would write. Check it in Python:

```bash
python3 - <<'EOF'
d = open('/tmp/gameplay-u2/smoke/frame_00000.ipx', 'rb').read()
idx, dac = d[:64000], d[64000:]
assert len(dac) == 768
rgb = bytearray(192000)
for c in range(3):
    rgb[c::3] = idx.translate(bytes(dac[3 * i + c] for i in range(256)))
print(len(rgb), rgb[:6].hex(), sum(1 for i in range(64000) if idx[i]) )
EOF
```

Expected (the planner's scratch run): `192000 cb92e3aa69d3 64000`. Record. (U3 Task 1 uses the same `translate` expansion.)

- [ ] **Step 5: The trace parses with the capture parser**

```bash
python3 - <<'EOF'
import sys; sys.path.insert(0, 'tools')
import gp_session as gs
recs = [gs.parse(l) for l in open('/tmp/gameplay-u2/smoke/trace.txt')]
names = set(recs[0]) - {'kind'}
print(recs[0]['kind'], len(recs), sorted(set(n for n, _, _ in gs.SNAP_FIELDS) - names), all(t in names for t in gs.TRACE_FIELDS))
EOF
```

Expected: `T 601 [] True` (no `SNAP_FIELDS` name missing).

- [ ] **Step 6: Mutation proofs**

(a) Remove the after-iteration `gp_dump_if_new()` and the hook's: expected `FAIL … gp frames were written`. (b) Skip `gp_trace_line()` on odd iterations: expected `FAIL … <n> != <m>` from the trace-count check. Restore; green. Record in §G.10.

- [ ] **Step 7: Commit**

```bash
git add port/tests/test_game.c docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
tests: gp replay dumps every displayed frame (.ipx) and a per-iteration trace (record §G.10)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: `make gp-replay` and the gate

**Files:**
- Modify: `Makefile` (a variable `GP_DUMP = /tmp/pr_gp_dump` beside `K11_DUMP`; a `gp-replay` target after `gp-capture`; `.PHONY`)
- Modify: record §G.11

**Interfaces:**
- Produces: `make gp-replay scenario=<gp-…>`: `$(GP_DUMP)/<scenario>.script` and the dump `$(GP_DUMP)/<scenario>/` from `data/k11-captures/<scenario>/poll.log`; skips with a message when the capture is absent. U3's `gp-oracle` calls it.

- [ ] **Step 1: The target**

```make
GP_DUMP = /tmp/pr_gp_dump

# Gameplay replay (spec 2026-09-30-gameplay-ground-truth-design.md §4.2): the
# v2 port script from the capture's poll.log, then the PR_GP_DUMP driver alone.
gp-replay: build ## Replay a gameplay capture in the port (scenario=gp-…; dump in $(GP_DUMP)/<scenario>)
	@if [ -d $(K11_CAPTURES)/$(scenario) ]; then \
		rm -rf $(GP_DUMP)/$(scenario); mkdir -p $(GP_DUMP); \
		$(PYTHON) tools/gp_session.py port-script --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) --out $(GP_DUMP)/$(scenario).script && \
		PR_GP_DUMP=$(GP_DUMP)/$(scenario) PR_GP_SCRIPT=$(GP_DUMP)/$(scenario).script PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "gp-replay: no capture at $(K11_CAPTURES)/$(scenario)"; \
	fi
```

Add `gp-replay` to `.PHONY`.

- [ ] **Step 2: Run it on the U1 capture**

```bash
make gp-replay scenario=gp-pads 2>&1 | tail -3
wc -l < /tmp/pr_gp_dump/gp-pads/trace.txt; ls /tmp/pr_gp_dump/gp-pads/*.ipx | wc -l
```

Expected: `gp_session: port-script: gp-pads: wrote …`, `all checks passed`; the counts recorded in §G.11. If the port-script step fails, stop: U1's capture checks should have caught it; record the message and fix the capture (U1 Task 8), not the generator.

- [ ] **Step 3: The gate**

```bash
S=/tmp/gameplay-u2
make verify > "$S/t3_verify.txt" 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|k11_compare)' "$S/t3_verify.txt" | diff - /tmp/gameplay-u1/or_base.txt && echo ORACLES-EQUAL
for s in walk menuesc; do (cd /tmp/pr_k11_dump/$s && shasum -a 256 frame_*.raw) | diff - "$S/k11_$s.sha256" && echo "K11-$s-IDENTICAL"; done
git diff --stat 934992a -- port/src
rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l
```

Expected: `verify-exit=0`; `ORACLES-EQUAL`; `K11-walk-IDENTICAL`, `K11-menuesc-IDENTICAL` (or both absent, as in Step 1 of Task 1); empty `port/src` diff; the assertion-site count = Task 1 Step 1's + 14 (11 from Task 1, 3 from Task 2). Record in §G.11.

- [ ] **Step 4: Commit**

```bash
git add Makefile docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
build: make gp-replay (the v2 script and the PR_GP_DUMP driver) (record §G.11)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: U2 closure

**Files:**
- Modify: record §G.12; `docs/PROGRESS.md` (one paragraph)

- [ ] **Step 1: Record**

§G.12: the driver's contract (the frame rule, the dump and trace formats, `GP_LOOP_SLACK = 600` as a harness bound, no fixed step cap), the smoke's mode runs (Task 2 Step 3), and "Not tested": a step missed behind a blocking loop (Task 1 Step 6 (c)), a fault during replay (no scripted path faults), a script beyond `0xFFFF` frames (the counter is a word).

- [ ] **Step 2: Progress and commit**

Append a `docs/PROGRESS.md` paragraph "Gameplay U2: port replay v2 …" with the commit range and the gate result.

```bash
git add docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md docs/PROGRESS.md
git commit -m "$(cat <<'EOF'
docs: gameplay U2 closure: port replay v2 (record §G.12)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review

- Spec §4.2 coverage: replay keyed by frame (Task 1), no fixed caps and a script-derived loop bound (Task 1), every displayed frame as `.ipx` with `frames.txt` (Task 2), the trace in the capture's names (Task 2 Step 5), no production change (Task 3 Step 3), `make gp-replay` for U3 (Task 3).
- Names: `GpStep`, `gp_parse`, `gp_apply`, `gp_hook`, `gp_fault`, `gp_dump_if_new`, `gp_loader`, `gp_trace_line`, `test_gp_replay`, `GP_LOOP_SLACK`, `GP_STALL_PUMPS`, `GP_DS_0010810D`, `GP_DUMP`, `gp-replay` — defined once each; `k11_key_bits` and `k11_fnv` are the K11 driver's statics, reused unchanged.
- Review Focus 2's mutation (c) is not reachable by the smoke and is recorded as "Not tested" with the reason, rather than claimed.
</content>
</invoke>
