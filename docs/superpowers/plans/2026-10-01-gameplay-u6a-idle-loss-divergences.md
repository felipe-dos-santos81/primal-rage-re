# Gameplay U6a — Port What the Idle-Loss Run Proves Missing, Re-pin Its Ratchets — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port, from the raw, the four functions the `gp-idle-loss` replay reaches unregistered (`0x3640C`, `0x23208`, `0x37DCC`, `0x3A588`), each in its own task with seeded tests and mutation proofs, shrink the replay driver's pinned miss set by exactly the ported function at every step, and re-measure and re-pin the idle-loss ratchets (the trace N rises 2088 → 2161; the frame N 203 and the window start 90 stay).

**Architecture:** Each function becomes one C function in `port/src/game/fighter.c` with a `/* 0xADDR — record gameplay-u6 §U6.x */` header, registered in `actors_init` (`port/src/game/actors.c`) so `fn_resolve` finds it; the two leaves also get differential-emulation specs (`tools/diff_verify.py`, `port/tests/diff_runner.c`). The `gp-idle-loss` capture and the U1–U4 tools are unchanged; only the ratchet values and the replay's pinned set move.

**Tech Stack:** C (the port), Python 3 (`tools/diff_verify.py`, unittest), capstone 5.0.7 / unicorn 2.1.4 (the E1 harness), `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track G (O2, O7), §6 (G exit criteria), §8; `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md` §2 (U6), §5. Evidence: `docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md` §G.20, §G.23, §G.24, §J; `.superpowers/sdd/2026-09-30-gameplay-scope/u4-report.md`.

**Derivation record:** `docs/superpowers/plans/2026-10-01-gameplay-u6-derivations.md` §U6.1–§U6.9 (U6a), §U6.19–§U6.20. It ships with this plan; execution appends §U6.21 (U6a execution).

**Why two plans.** U6a is evidenced work on an existing capture: four ports the replay's miss log already proves missing, and a re-pin; it needs no capture and no user decision, and its result (round 1 becomes a KO) is the base U6b's dry runs and replay stand on. U6b (`2026-10-01-gameplay-u6b-moves-capture.md`) adds snapshot fields, a tool, a scenario, a capture behind a storage decision, and miss-log-driven ports whose set only the capture fixes. Executed in order: U6a, merge, then U6b.

## Decisions needed from the user

None. (U6a writes no capture and moves no oracle line; the only ratchet that moves rises.)

---

## Global Constraints

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." and "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`. Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`." and "Code addresses stored in data go through `fn_origin()` / `fn_resolve()`."
- AGENTS.md: "**`port/src/symbols.h` is generated** … Never hand-edit it; where the generator emits no name, use a local `#define` with the raw address."
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero."
- AGENTS.md: "Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and must record exactly the pinned known-set … A port that makes a driver reach a new unregistered code pointer fails it: register the target or pin the miss with its evidence."
- AGENTS.md (gameplay oracle): the N values "and provenance are in the Makefile (with the capture's `poll.log` sha256: another capture fails)"; demo-fight precedent: a ratchet "fails if the first unexplained … frame moves earlier than its pinned N (raise N when it improves)".
- Common brief: "`make verify` is THE gate; the 45 oracle lines (grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the `make audio-render` WAV must be byte-identical to `before-t2.wav`; `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100`". U6a ports four functions none of which is in `port/decomp/prage.functions.csv` (record §U6.19 item 8), so **the counters must still print `771 1203 64` and `731 731 100`**.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." "Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files." Trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **A port that differs from the raw in an untraced field** (the replay cannot see slot `+0x53`, the voice id, the stream hold). Pinned by the seeded tests of Tasks 1–4 and their mutation proofs (each mutation listed must fail).
2. **The pinned miss set drifting** (a function ported but still listed, or a new miss accepted silently). Pinned by `test_fn_misslog_driver` under the replay of each task: the expected `distinct=` count and lines are given per task.
3. **A ratchet raised past what the port shows, or lowered.** Task 5 pins only measured values, shows each fails at N+1 (and the start at 89), and the trace N may only rise.
4. **Breaking another driver or oracle** by registering an address another path reaches. Every other driver's pinned set is the base pair (record gameplay-u0 §U0.2), so none reaches these four; pinned by the full `make verify` of Task 5 (45 oracle lines, WAV, K11, gp-pads, demo-fight, attract cycle-2).
5. **Reading the new first divergence as fixed or as U6's.** Task 5 classifies `f = 0x871` (crowd-effect walk one frame late, record §U6.8) as a named gap with an owner outside U6, with the throw-away backtrace reverted (`git diff port/src/game/rng.c` empty).

## Where to run

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse
git worktree add .worktrees/gameplay-u6a -b gameplay-u6a main
cd .worktrees/gameplay-u6a
ln -s /Users/felipe.dos.santos/code/mine/primal-rage-reverse/data data
ln -s /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.superpowers .superpowers
cp ../../port/tests/ghidra_data.bin ../../port/tests/title_screen_ref.ppm port/tests/
make build
export S=/tmp/pr_u6a; mkdir -p $S
export V="SMK_DUMP=/tmp/pr_u6a_smk TITLE_DUMP=/tmp/pr_u6a_title ATTRACT_DUMP=/tmp/pr_u6a_att FRONTEND_DUMP=/tmp/pr_u6a_fe TITLE_PIN_DIR=/tmp/pr_u6a_pin AUDIO_WAV=/tmp/pr_u6a.wav K11_DUMP=/tmp/pr_u6a_k11 GP_DUMP=/tmp/pr_u6a_gp DIFF_IMAGE=/tmp/pr_u6a_diffimg DIFF_TABLE=/tmp/pr_u6a_diff.md"
```

Every command below runs from `.worktrees/gameplay-u6a` (the binaries read the relative `data/game/C`). The replay is `make gp-replay scenario=gp-idle-loss GP_DUMP=/tmp/pr_u6a_gp TITLE_PIN_DIR=/tmp/pr_u6a_pin` (about 140 s, about 410 MB in `/tmp`, record §G.24 item 6).

---

### Task 0: Baseline

**Files:** none (measurement only; appended to record §U6.21 in Task 6).

- [ ] **Step 1: The replay and its report on the unchanged base**

```bash
make gp-replay scenario=gp-idle-loss GP_DUMP=/tmp/pr_u6a_gp TITLE_PIN_DIR=/tmp/pr_u6a_pin > $S/t0_replay.txt 2>&1; echo "rc=$?"
grep fn-miss $S/t0_replay.txt
python3 tools/gp_compare.py --report --scenario gp-idle-loss --capture data/k11-captures/gp-idle-loss --port /tmp/pr_u6a_gp/gp-idle-loss | tee $S/t0_report.txt
python3 tools/port_progress.py
```

Expected: `rc=0`, `all checks passed`, the seven lines of record §U6.1 (`0x23208 hit_reaction_apply hits=1`, `0x3A588 fighter_state_3531c hits=5353`, `0x3640C anim_indirect hits=1`, `distinct=7 dropped=0`), `FIRST UNEXPLAINED capture 203 (raw 2359)`, `window from capture 90 (raw 1744)`, `trace: first difference f=828 (2088) in rng`, `771 1203 64`, `731 731 100`. If any differs, stop and record it: the plan's expected values assume `main` with U0–U4 and E1 merged.

- [ ] **Step 2: The gate baseline**

```bash
make verify $V > $S/t0_verify.txt 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $S/t0_verify.txt | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
make audio-render AUDIO_WAV=/tmp/pr_u6a.wav >/dev/null && cmp /tmp/pr_u6a.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-EQUAL
grep -E '^(gp_compare|k11_compare|diff-verify)' $S/t0_verify.txt > $S/t0_gate_lines.txt
```

Expected: `verify-exit=0`, `ORACLES-EQUAL`, `WAV-EQUAL`. Keep `$S/t0_gate_lines.txt` for Task 5.

---

### Task 1: `0x3640C` (the `0xD000` animation-opcode target)

Record §U6.4, §U6.6, §U6.7. Ported first: on the base path it is reached once (round 2, `f = 0x173A`), and porting it changes no traced field before that.

**Files:**
- Modify: `port/tests/test_fight.c` (append the U6 block at the end of the file)
- Modify: `port/tests/test.h` (`TEST_CASES`: one line after `X(test_table_reached)`)
- Modify: `port/src/game/fighter.c` (insert before the line `/* 0x36300. The +0x52 == 13 handler. */`)
- Modify: `port/src/game/fighter.h` (after `void fighter_36280(u32 rec);`)
- Modify: `port/src/game/actors.c` (the `anim_code_36280` prototype, its `fn_register`, its definition)
- Modify: `port/tests/test_platform.c` (`k_miss_gp_idle_loss` and its comment)
- Modify: `port/tests/diff_runner.c`, `tools/diff_verify.py`, `tools/tests/test_diff_verify.py`

**Interfaces:**
- Produces: `void fighter_3640c(u32 rec);` (fighter.h); `static void anim_code_3640C(u32 rec, u32 arg);` registered as `0x3640Cu`; `int test_u6_idle_loss_callbacks(void);`; the diff spec `fighter_3640c` and the mutant binding `fighter_3640c@mutant`.
- Consumes: `fn_resolve`, `tf_snap`/`tf_put`, `mem_fill`, `FIGHT_RECS` (test_fight.c fixtures).

- [ ] **Step 1: Write the failing test.** Append to the end of `port/tests/test_fight.c`:

```c

/* ---- gameplay-u6 §U6.2-§U6.5: the four idle-loss callbacks ----------------
 * The functions the gp-idle-loss replay reached unregistered (record
 * gameplay-ground-truth §G.24) or, once those were ported, reached next
 * (0x37DCC, record gameplay-u6 §U6.5). Each check seeds sentinels that differ
 * from every post-condition, asserts the raw's references in the image, and
 * calls the function both directly and through its fn_resolve registration. */

typedef void (*u6_anim_fn)(u32 rec, u32 arg);

/* §U6.4: 0x3640C, EAX = rec: +0x52 = 0, hold 3.0, +0x4D = 0x14, and the owner
 * slot rec+0x14's +0x52 = 5 when it is set; the seven 0xD000 stream dwords. */
static void check_u6_3640c(void)
{
    static const u32 site[7] = {
        0xD2156u, 0xD3E2Au, 0xE063Eu, 0xE39F2u, 0xE6DF2u, 0xEA626u, 0xECBFAu,
    };
    u32 rec = FIGHT_RECS, own = FIGHT_RECS + 0x200u;
    u8 sv_rec[0x60], sv_own[0x60];
    u6_anim_fn fn = (u6_anim_fn)(void *)fn_resolve(0x3640Cu);
    u32 k, pass;
    for (k = 0; k < 7u; k++) {
        CHECK_EQ_INT((int)DSD(site[k]), 0x0003640C);
        CHECK_EQ_INT((int)DSW(site[k] - 2u), 0xD000);
    }
    CHECK(fn != NULL, "actors_init registered 0x3640C");
    tf_snap(sv_rec, rec, sizeof sv_rec);
    tf_snap(sv_own, own, sizeof sv_own);
    for (pass = 0; pass < 2u; pass++) {          /* direct, then through fn_resolve */
        for (k = 0; k < 2u; k++) {               /* no owner, then an owner slot */
            mem_fill(rec, 0xA5, sizeof sv_rec);
            mem_fill(own, 0xC3, sizeof sv_own);
            DSB(rec + 0x52u) = 0x7Fu;
            DSD(rec + 0x24u) = 0xDEADBEEFu;
            DSB(rec + 0x4Du) = 0x99u;
            DSD(rec + 0x14u) = k ? own : 0u;
            DSB(own + 0x52u) = 0x33u;
            if (pass == 0u) fighter_3640c(rec);
            else if (fn != NULL) fn(rec, 0xFFFFu);
            CHECK_EQ_INT((int)DSB(rec + 0x52u), 0);
            CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
            CHECK_EQ_INT((int)DSB(rec + 0x4Du), 0x14);
            CHECK_EQ_INT((int)DSB(rec + 0x4Cu), 0xA5);  /* the neighbours stay */
            CHECK_EQ_INT((int)DSB(rec + 0x4Eu), 0xA5);
            CHECK_EQ_INT((int)DSB(own + 0x52u), k ? 5 : 0x33);
            CHECK_EQ_INT((int)DSB(own + 0x53u), 0xC3);
        }
    }
    tf_put(sv_rec, rec, sizeof sv_rec);
    tf_put(sv_own, own, sizeof sv_own);
}

int test_u6_idle_loss_callbacks(void)
{
    int before = g_failures;
    check_u6_3640c();
    return g_failures - before;
}
```

In `port/tests/test.h`, after the line `    X(test_table_reached) \` add `    X(test_u6_idle_loss_callbacks) \`.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build 2>&1 | grep -E "error" | head -3`
Expected: a compile error naming `fighter_3640c` (undeclared function).

- [ ] **Step 3: Implement.** In `port/src/game/fighter.c`, insert before `/* 0x36300. The +0x52 == 13 handler. */`:

```c
/* 0x3640C — record gameplay-u6 §U6.4. The 0xD000 stream target (opcode 0x10)
 * at the seven dwords 0xD2156, 0xD3E2A, 0xE063E, 0xE39F2, 0xE6DF2, 0xEA626 and
 * 0xECBFA (each after a 0xD000 word; Ghidra has no function here). EAX = rec:
 * its +0x52 = 0, its hold 3.0 and +0x4D = 0x14; with the owner slot rec+0x14
 * set, that slot's +0x52 = 5. EDX is pushed and popped (0x3640C/0x3642B);
 * the EAX it leaves (the owner slot) is not read by the three 0x2B2A0 call
 * sites, which each reload EAX at once (0x2B573, 0x2B59A, 0x2B5F0). */
void fighter_3640c(u32 rec)
{
    u32 slot;
    DSB(rec + 0x52u) = 0u;                              /* 0x3640D */
    DSD(rec + 0x24u) = 0x40400000u;                     /* 0x36416 */
    DSB(rec + 0x4Du) = 0x14u;                           /* 0x36411/0x3641D */
    slot = DSD(rec + 0x14u);                            /* 0x36420 */
    if (slot == 0u) return;                             /* 0x36423/0x36425 */
    DSB(slot + 0x52u) = 5u;                             /* 0x36427 */
}

```

In `port/src/game/fighter.h`, after `void fighter_36280(u32 rec);`:

```c
/* 0x3640C (record gameplay-u6 §U6.4): the 0xD000 target of seven streams.
 * EAX = rec: +0x52 = 0, hold 3.0, +0x4D = 0x14; the owner slot (rec+0x14),
 * when set, +0x52 = 5. */
void fighter_3640c(u32 rec);
```

In `port/src/game/actors.c`: after `static void anim_code_36280(u32 rec, u32 arg);` add `static void anim_code_3640C(u32 rec, u32 arg);`; after `    fn_register(0x36280u, (void (*)(void))anim_code_36280);` add

```c
    /* PORT: record gameplay-u6 §U6.4. The 0xD000 target 0x3640C (opcode 0x10,
     * mode 0x4000) of seven streams (the dwords at 0xD2156, 0xD3E2A, 0xE063E,
     * 0xE39F2, 0xE6DF2, 0xEA626 and 0xECBFA). */
    fn_register(0x3640Cu, (void (*)(void))anim_code_3640C);
```

and after the definition of `anim_code_36280` (the block ending `    fighter_36280(rec);\n}`) add

```c

/* 0x3640C — the animation-opcode target shape. PORT: anim_indirect calls every
 * code pointer as (rec, arg); the raw 0x3640C takes EAX = rec and does not
 * read EDX (pushed at 0x3640C, DL overwritten at 0x36411, popped at 0x3642B),
 * so this wrapper drops the operand (record gameplay-u6 §U6.4). */
static void anim_code_3640C(u32 rec, u32 arg)
{
    (void)arg;
    fighter_3640c(rec);
}
```

- [ ] **Step 4: Run the suite**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -1`
Expected: `all checks passed`.

- [ ] **Step 5: Mutation proofs** (each applied alone, rebuilt, `run_tests` run, then restored):
  - `DSB(slot + 0x52u) = 5u;` → `(void)slot;` ⇒ `FAIL … test_fight.c:…: 51 != 5`.
  - `DSB(rec + 0x4Du) = 0x14u;` → `0x15u` ⇒ `FAIL …: 21 != 20`.
  - the `fn_register(0x3640Cu, …)` line → `(void)anim_code_3640C;` ⇒ `FAIL …: actors_init registered 0x3640C`.

- [ ] **Step 6: The differential spec.** In `port/tests/diff_runner.c` after `static void b_codeword_len(...)` add:

```c
/* 0x3640C and 0x37DCC (record gameplay-u6 §U6.4/§U6.5): animation-opcode targets with no C return
 * value. Their callers, the three `call [0x105BD4]` sites of 0x2B2A0 (0x2B56D, 0x2B594, 0x2B5EA),
 * each overwrite EAX at once (`mov eax,ecx` at 0x2B573, 0x2B59A, 0x2B5F0), so no caller reads it:
 * the binding mask is 0 and the comparison is the changed bytes. */
static void b_3640c(const u32 *r, u32 *eax)            { fighter_3640c(r[R_EAX]); *eax = 0u; }
```

before `static const binding_t k_bindings[] = {` add

```c
static void m_3640c(const u32 *r, u32 *eax)            /* forgets the owner slot's +0x52 */
{
    DSB(r[R_EAX] + 0x52u) = 0u;
    DSD(r[R_EAX] + 0x24u) = 0x40400000u;
    DSB(r[R_EAX] + 0x4Du) = 0x14u;
    *eax = 0u;
}

```

and as the last two rows of `k_bindings`:

```c
    { "fighter_3640c",            b_3640c,        0x00000000u },
    { "fighter_3640c@mutant",     m_3640c,        0x00000000u },
```

In `tools/diff_verify.py` after `DS_NODEBIT = 0x104B1F` add
`U6_REC, U6_OWNER = 0x10A000, 0x10A100   # inside the image's zero BSS (record gameplay-u6 §U6.6)`, and as the last element of `SPECS`:

```python
    # Record gameplay-u6 §U6.6. Mask 0: the three 0x2B2A0 call sites reload EAX at once (0x2B573,
    # 0x2B59A, 0x2B5F0), so the comparison is the changed bytes alone.
    Spec("fighter_3640c", 0x3640C, [
        Case("k0", {"eax": U6_REC, "edx": 0x11223344},
             {U6_REC + 0x52: b"\x7f", U6_REC + 0x24: le32(0xDEADBEEF), U6_REC + 0x4D: b"\x99",
              U6_REC + 0x14: le32(0)}),
        Case("k1", {"eax": U6_REC, "edx": 0x11223344},
             {U6_REC + 0x52: b"\x7f", U6_REC + 0x24: le32(0xDEADBEEF), U6_REC + 0x4D: b"\x99",
              U6_REC + 0x14: le32(U6_OWNER), U6_OWNER + 0x52: b"\x33"}),
    ], eax_mask=0),
```

In `tools/tests/test_diff_verify.py`: add `"fighter_3640c",` to the sorted list in `test_the_four_ported_functions_agree_with_the_original_on_every_block` (after `"config_credit_spend",`), `"fighter_3640c@mutant",` to the list in `test_every_mutant_is_reported_as_a_mismatch` (after `"config_credit_spend@signed",`), and `"fighter_3640c": 0,` to the dict in `test_the_eax_mask_is_stated_by_the_spec_and_only_slot_flag_narrows_it`.

Run: `cmake --build build && python3 tools/diff_verify.py --image /tmp/pr_u6a_diffimg --self-check | grep -E 'fighter_3640c|diff-verify:' && python3 -m unittest tools.tests.test_diff_verify 2>&1 | tail -1`
Expected: `| fighter_3640c | 0x3640C | 2 | 3/3 | VERIFIED |`, `| fighter_3640c@mutant | 0x3640C | 2 | 3/3 | MISMATCH |`, `diff-verify: 5/5 functions VERIFIED; 6/6 mutants detected. …`, `OK`. Mutation: in `fighter_3640c` change `0x40400000u` to `0x40000000u`, `cmake --build build`, `python3 tools/diff_verify.py --image /tmp/pr_u6a_diffimg --function fighter_3640c` ⇒ `fighter_3640c: k0: byte 0x10A026: original 0x40, port 0x00` and `0/1 functions VERIFIED`; restore and rebuild.

- [ ] **Step 7: The pinned miss set.** In `port/tests/test_platform.c` replace

```c
 * (measured on the merged base, the full replay to f = 0x207F) adds five
 * pairs, each classified from the raw in §G.24:
```
with
```c
 * (measured on the merged base, the full replay to f = 0x207F) adds four
 * pairs, each classified from the raw in §G.24:
```
replace
```c
 *     pointer 0x3A650 stores), 5353 hits from f = 0x927;
 *   0x3640C anim_indirect: an UNPORTED animation-opcode target, f = 0x173A.
```
with
```c
 *     pointer 0x3A650 stores), 5353 hits from f = 0x927.
 * Ported since (record gameplay-u6 §U6.7): 0x3640C (anim_indirect, f = 0x173A).
```
and delete the row `    { 0x3640Cu, "anim_indirect" },`.

- [ ] **Step 8: Replay**

```bash
make gp-replay scenario=gp-idle-loss GP_DUMP=/tmp/pr_u6a_gp TITLE_PIN_DIR=/tmp/pr_u6a_pin > $S/t1_replay.txt 2>&1; echo "rc=$?"; grep fn-miss $S/t1_replay.txt
python3 tools/gp_compare.py --report --scenario gp-idle-loss --capture data/k11-captures/gp-idle-loss --port /tmp/pr_u6a_gp/gp-idle-loss | grep -E 'FIRST UNEX|window|trace: first diff'
```

Expected (record §U6.7 row `3640C`): `rc=0`, `all checks passed`, `distinct=6`, the lines `0x23208 hit_reaction_apply hits=1` and `0x3A588 fighter_state_3531c hits=5356`, no `0x3640C`; `FIRST UNEXPLAINED capture 203`, `window from capture 90`, `trace: first difference f=828 (2088)`. Mutation: restore the deleted row ⇒ `FAIL …test_platform.c:…: 6 != 7`; delete it again.

- [ ] **Step 9: Commit**

```bash
git add port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/test_fight.c port/tests/test.h port/tests/test_platform.c port/tests/diff_runner.c tools/diff_verify.py tools/tests/test_diff_verify.py
git commit -m "$(cat <<'EOF'
game: port the 0xD000 animation-opcode target 0x3640C (record gameplay-u6 §U6.4)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: `0x23208` (character 1's reaction-0x26 callback)

Record §U6.2, §U6.7. The miss four frames before the first trace difference (record §G.24).

**Files:** `port/tests/test_fight.c` (a check before `int test_u6_idle_loss_callbacks(void)` and one call); `port/src/game/fighter.c` (insert before `/* 0x23250 — record §48-P. Character 6's slot +0x18 hook`); `port/src/game/fighter.h` (after `void fighter_47bfc(u32 slot, u32 rec, u32 side);`); `port/src/game/actors.c` (after `    fn_register(0x47BFCu, (void (*)(void))fighter_47bfc);`); `port/tests/test_platform.c`.

**Interfaces:** Produces `void fighter_23208(u32 slot, u32 rec, u32 side);` registered as `0x23208u`. Consumes the test fixtures `mz_save`/`mz_restore`, `sc_seed`, `sc_stream`, `Z_S1`/`Z_R0`/`Z_R1`, `sound_voice_log_reset/_count/_at`.

- [ ] **Step 1: Write the failing test.** Insert before `int test_u6_idle_loss_callbacks(void)`:

```c
/* §U6.2: 0x23208, character 1's reaction-0x26 entry, as 0x34E2C calls it
 * (slot, rec, side) on side 1: the record on 0xE4900 (first word patched to a
 * plain frame id) at 3.0, the slot 9/7/0 and +0x0C = 0, the voice 0x79 once;
 * +0x57/+0x18/+0x1C untouched. */
static void check_u6_23208(void)
{
    if (!mz_save()) { CHECK(0, "the gameplay-u6 snapshot allocates"); return; }
    CHECK_EQ_INT((int)DSD(0x000A3528u + (1u * 64u + 0x26u) * 20u), 0x00023208);
    CHECK(fn_resolve(0x23208u) == (void (*)(void))fighter_23208,
          "0x23208 is registered as fighter_23208");
    sc_seed(1u, 1u, 0);
    DSW(0x000E4900u) = 0x1561u;
    DSB(Z_S1 + 0x52u) = 0x0Cu;
    DSB(Z_S1 + 0x53u) = 0x33u;
    DSB(Z_S1 + 0x54u) = 0x44u;
    DSD(Z_S1 + 0x0Cu) = 0x0C0C0C0Cu;
    DSB(Z_S1 + 0x57u) = 0x57u;
    DSD(Z_S1 + 0x18u) = 0x18181818u;
    DSD(Z_S1 + 0x1Cu) = 0x1C1C1C1Cu;
    DSD(Z_R0 + 0x08u) = 0x00ABCDEFu;
    sound_voice_log_reset();
    fighter_23208(Z_S1, Z_R1, 1u);
    sc_stream(Z_R1, 0x000E4900u, 0x40400000u, 0x1561u);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x57u), 0x57);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x18u), 0x18181818);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x1Cu), 0x1C1C1C1C);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x08u), 0x00ABCDEF);   /* the other record */
    CHECK_EQ_INT((int)sound_voice_log_count(), 1);
    CHECK_EQ_INT((int)sound_voice_log_at(0), 0x79);
    sound_voice_log_reset();
    mz_restore();
}

```

and in `test_u6_idle_loss_callbacks` add `    check_u6_23208();` after `    check_u6_3640c();`.

- [ ] **Step 2: Run to verify it fails**: `cmake --build build 2>&1 | grep error | head -2` ⇒ a compile error naming `fighter_23208`.

- [ ] **Step 3: Implement.** In `fighter.c`, insert before `/* 0x23250 — record §48-P. Character 6's slot +0x18 hook`:

```c
/* PORT: a data-object address symbols.h does not name. */
#define FIGHT_ANIM_23208 0x000E4900u  /* 0x2321A: 0x23208's stream */

/* 0x23208 — record gameplay-u6 §U6.2. Character 1's reaction-0x26 callback
 * (the dword at 0xA3D20, its only reference; Ghidra has no function here).
 * 0x34E2C calls it at 0x35045 with EAX = slot, EDX = rec, EBX = side and does
 * not read its AL (0x35049 `add esp,0x28`). The context 0x33950(side) is built
 * and never read; the record on 0xE4900 at 3.0 (0x3C4CC, whose RET 4 pops the
 * 0x23221 push), the slot's +0x52/+0x53/+0x54 = 9/7/0 and +0x0C = 0, then the
 * voice 0x79. */
void fighter_23208(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                            /* 0x23211..0x23215 0x33950 */
    hit_anim_start_b(rec, FIGHT_ANIM_23208, 0x40400000u);   /* 0x2321A..0x23226 0x3C4CC */
    DSB(slot + 0x52u) = 9u;                                 /* 0x2322B */
    DSB(slot + 0x53u) = 7u;                                 /* 0x2322F */
    DSB(slot + 0x54u) = 0u;                                 /* 0x23233 */
    DSD(slot + 0x0Cu) = 0u;                                 /* 0x2323C */
    (void)sound_voice(0x79u);                               /* 0x23237/0x23243 0x2C3FC */
}

```

In `fighter.h` after `void fighter_47bfc(u32 slot, u32 rec, u32 side);`:

```c
/* 0x23208 (record gameplay-u6 §U6.2). Character 1's reaction-0x26 callback
 * (slot, rec, side): the record on 0xE4900 at 3.0, the slot 9/7/0 with +0x0C =
 * 0, the voice 0x79. */
void fighter_23208(u32 slot, u32 rec, u32 side);
```

In `actors.c` after `    fn_register(0x47BFCu, (void (*)(void))fighter_47bfc);`:

```c
    /* PORT: record gameplay-u6 §U6.2. Character 1's reaction-0x26 callback
     * 0x23208 (the dword at 0xA3D20; 0x34E2C, (slot, rec, side)). */
    fn_register(0x23208u, (void (*)(void))fighter_23208);
```

- [ ] **Step 4: Run the suite**: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -1` ⇒ `all checks passed`.

- [ ] **Step 5: Mutation proofs** (each alone, restored after): `sound_voice(0x79u)` → `0x7Au` ⇒ `122 != 121`; `DSD(slot + 0x0Cu) = 0u;` → `(void)0;` ⇒ `202116108 != 0`; `DSB(slot + 0x53u) = 7u;` → `8u` ⇒ `8 != 7`; the hold `0x40400000u` → `0x40000000u` ⇒ `1073741824 != 1077936128` (in `sc_stream`); the `fn_register(0x23208u, …)` line → `(void)fighter_23208;` ⇒ `0x23208 is registered as fighter_23208`.

- [ ] **Step 6: The pinned miss set.** In `test_platform.c`: `adds four\n * pairs` → `adds three\n * pairs`; delete the two lines

```c
 *   0x23208 hit_reaction_apply: an UNPORTED move-table callback (character 1,
 *     reaction 0x26; U0 §U0.12's list), f = 0x824, the one hit;
```
replace ` * Ported since (record gameplay-u6 §U6.7): 0x3640C (anim_indirect, f = 0x173A).` with

```c
 * Ported since (record gameplay-u6 §U6.7): 0x3640C (anim_indirect, f = 0x173A),
 * 0x23208 (hit_reaction_apply, f = 0x824).
```
and delete the row `    { 0x23208u, "hit_reaction_apply" },`.

- [ ] **Step 7: Replay** (Task 1 Step 8's commands, files `t2_*`). Expected (record §U6.7 row `3640C 23208`): `all checks passed`, `distinct=5`, `0x3A588 fighter_state_3531c hits=4626` the only non-harmless line; `FIRST UNEXPLAINED capture 203`, `window from capture 90`, **`trace: first difference f=871 (2161) in rng: capture 6A6BD8FA, port AEE5C3BD`**. Keep a copy: `cp /tmp/pr_u6a_gp/gp-idle-loss/trace.txt $S/t2_trace.txt`.

- [ ] **Step 8: Commit** (`git add` the five files of this task's **Files**; message `game: port character 1's reaction-0x26 callback 0x23208 (record gameplay-u6 §U6.2)` with the trailer).

---

### Task 3: `0x37DCC` (the `0xD100` animation-opcode target)

Record §U6.5, §U6.7. Not reached yet: `0x3A588` (Task 4) makes it reachable, so it is ported first and the pinned set stays the same.

**Files:** `port/tests/test_fight.c`; `port/src/game/fighter.c` (insert before `/* 0x36300. The +0x52 == 13 handler. */`, i.e. right after `fighter_3640c`); `fighter.h` (after the `fighter_3640c` prototype); `actors.c`; `port/tests/diff_runner.c`; `tools/diff_verify.py`; `tools/tests/test_diff_verify.py`.

**Interfaces:** Produces `void fighter_37dcc(void);`, `static void anim_code_37DCC(u32 rec, u32 arg);` registered as `0x37DCCu`; the diff spec `fighter_37dcc` and `fighter_37dcc@mutant`.

- [ ] **Step 1: Write the failing test.** Insert before `int test_u6_idle_loss_callbacks(void)`:

```c
/* §U6.5: 0x37DCC sets DS_001078FC = 1 and reads nothing; its seventeen 0xD100
 * stream dwords. */
static void check_u6_37dcc(void)
{
    static const u32 site[17] = {
        0xD2B98u, 0xD329Cu, 0xD4816u, 0xD4F30u, 0xE113Cu, 0xE18DCu, 0xE4502u,
        0xE502Cu, 0xE78A4u, 0xE8676u, 0xE871Eu, 0xEB154u, 0xEB724u, 0xEB7D8u,
        0xEB8AEu, 0xED520u, 0xEDB4Au,
    };
    u8 sv = DSB(DS_001078FC);
    u32 rec = FIGHT_RECS;
    u8 sv_rec[0x60];
    u6_anim_fn fn = (u6_anim_fn)(void *)fn_resolve(0x37DCCu);
    u32 k;
    for (k = 0; k < 17u; k++) {
        CHECK_EQ_INT((int)DSD(site[k]), 0x00037DCC);
        CHECK_EQ_INT((int)DSW(site[k] - 2u), 0xD100);
    }
    tf_snap(sv_rec, rec, sizeof sv_rec);
    DSB(DS_001078FC) = 0x5Au;
    fighter_37dcc();
    CHECK_EQ_INT((int)DSB(DS_001078FC), 1);
    CHECK(fn != NULL, "actors_init registered 0x37DCC");
    DSB(DS_001078FC) = 0x5Au;
    mem_fill(rec, 0xA5, sizeof sv_rec);
    if (fn != NULL) fn(rec, 0x1234u);
    CHECK_EQ_INT((int)DSB(DS_001078FC), 1);
    CHECK_EQ_INT((int)DSB(rec + 0x52u), 0xA5);      /* the record is not read or written */
    tf_put(sv_rec, rec, sizeof sv_rec);
    DSB(DS_001078FC) = sv;
}

```

and `    check_u6_37dcc();` after `    check_u6_23208();`.

- [ ] **Step 2: Run to verify it fails**: compile error naming `fighter_37dcc`.

- [ ] **Step 3: Implement.** In `fighter.c` after `fighter_3640c`'s closing brace (before `/* 0x36300. The +0x52 == 13 handler. */`):

```c
/* 0x37DCC — record gameplay-u6 §U6.5. The 0xD100 stream target (opcode 0x11)
 * at seventeen dwords (0xD2B98 .. 0xEDB4A, each after a 0xD100 word; Ghidra
 * has no function here): `mov byte [0x1078FC],1; ret`. It reads no register. */
void fighter_37dcc(void)
{
    DSB(DS_001078FC) = 1u;                              /* 0x37DCC */
}

```

`fighter.h`, after the `fighter_3640c` prototype:

```c
/* 0x37DCC (record gameplay-u6 §U6.5): the 0xD100 target of seventeen
 * streams; sets the byte DS_001078FC = 1. */
void fighter_37dcc(void);
```

`actors.c`: after `static void anim_code_3640C(u32 rec, u32 arg);` add `static void anim_code_37DCC(u32 rec, u32 arg);`; after the `fn_register(0x3640Cu, …)` line add

```c
    /* PORT: record gameplay-u6 §U6.5. The 0xD100 target 0x37DCC (opcode 0x11,
     * mode 0x4000) of seventeen streams (the dwords 0xD2B98 .. 0xEDB4A). */
    fn_register(0x37DCCu, (void (*)(void))anim_code_37DCC);
```

and after `anim_code_3640C`'s definition:

```c

/* 0x37DCC — the animation-opcode target shape. PORT: anim_indirect calls every
 * code pointer as (rec, arg); the raw 0x37DCC reads neither (one store and
 * RET), so this wrapper drops both (record gameplay-u6 §U6.5). */
static void anim_code_37DCC(u32 rec, u32 arg)
{
    (void)rec;
    (void)arg;
    fighter_37dcc();
}
```

- [ ] **Step 4: Run the suite** ⇒ `all checks passed`.

- [ ] **Step 5: Mutation proofs**: `DSB(DS_001078FC) = 1u;` → `2u` ⇒ `2 != 1`; the `fn_register(0x37DCCu, …)` line → `(void)anim_code_37DCC;` ⇒ `actors_init registered 0x37DCC`.

- [ ] **Step 6: The differential spec.** `diff_runner.c`: after `b_3640c` add
`static void b_37dcc(const u32 *r, u32 *eax)            { (void)r; fighter_37dcc(); *eax = 0u; }`; after `m_3640c` add

```c
static void m_37dcc(const u32 *r, u32 *eax)            /* stores 2 */
{
    (void)r;
    DSB(DS_001078FC) = 2u;
    *eax = 0u;
}
```

and the rows `    { "fighter_37dcc",            b_37dcc,        0x00000000u },` (after the `fighter_3640c` row) and `    { "fighter_37dcc@mutant",     m_37dcc,        0x00000000u },` (last). `diff_verify.py`: after the `U6_REC` line add `DS_1078FC = 0x1078FC         # the byte 0x37DCC stores (record gameplay-u6 §U6.5)` and after the `fighter_3640c` spec:

```python
    Spec("fighter_37dcc", 0x37DCC, [
        Case("d0", {}, {DS_1078FC: b"\x00"}),
        Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
    ], eax_mask=0),
```

`test_diff_verify.py`: `"fighter_37dcc",` after `"fighter_3640c",`, `"fighter_37dcc@mutant",` after `"fighter_3640c@mutant",`, and `"fighter_37dcc": 0` in the mask dict. Run as in Task 1 Step 6. Expected: `| fighter_37dcc | 0x37DCC | 2 | 1/1 | VERIFIED |`, **`6/6 functions VERIFIED; 7/7 mutants detected`**, `OK` (`Ran 44`).

- [ ] **Step 7: Replay.** Expected: the same output as Task 2 Step 7 (`distinct=5`, `f=871 (2161)`), and `cmp /tmp/pr_u6a_gp/gp-idle-loss/trace.txt $S/t2_trace.txt` silent (an unreached registration changes nothing). The pinned set is unchanged.

- [ ] **Step 8: Commit** (the seven files; `game: port the 0xD100 animation-opcode target 0x37DCC (record gameplay-u6 §U6.5)`).

---

### Task 4: `0x3A588` (the `0x3A650` pose family's case-10 handler)

Record §U6.3, §U6.7, §U6.8. The 5 353 misses that kept P1 in state `0x10`.

**Files:** `port/tests/test_fight.c`; `port/src/game/fighter.c` (after `fighter_pose_3a6d4`'s body, before `/* ---- the 0x39F40 knockback pose's handler 0x39CC8`); `fighter.h` (after `void fighter_pose_3a6d4(u32 slot, u32 side);`); `actors.c` (after `    fn_register(0x3A6D4u, (void (*)(void))fighter_pose_3a6d4);`); `port/tests/test_platform.c`.

**Interfaces:** Produces `void fighter_pose_3a588(u32 slot, u32 side);` registered as `0x3A588u`; the test fixture `static void pose_3a588_seed(u32 s0, u32 s1, u32 r0, u32 r1)` (U6b's `0x3A820` check reuses it). Consumes `pose_handler_seed` (test_fight.c, record §41-A), `fighter_state_3531c`.

- [ ] **Step 1: Write the failing test.** Insert before `int test_u6_idle_loss_callbacks(void)`:

```c
/* §U6.3: the 0x3A650 family's handler 0x3A588. pose_handler_seed with
 * character 3 (0xC9030[3] = 0xD26C8, first word the sprite 0x17DB), the
 * 0x3A650 setter's B/A words (B = 0x107D00 + side*2, A = 0x107D0C + side*2)
 * zeroed, and the sibling families' pairs armed as traps (0x3A6D4's
 * 0x107D04/0x107D08 and 0x3A43C's 0x107D10/0x107D14: B = 3, A = 0x4321 would
 * snap if read). hit_anchor_x is made inert as in check_pose_handler_3a6d4. */
static void pose_3a588_seed(u32 s0, u32 s1, u32 r0, u32 r1)
{
    pose_handler_seed(s0, s1, r0, r1);
    DSB(s0 + 0x7Au) = 3;
    DSB(s1 + 0x7Au) = 3;
    DSB(s1 + 0x58u) = 1;
    DSD(r1 + 8u) = 0xDEADBEEFu;
    DSD(r1 + 0x24u) = 0xDEADBEEFu;
    DSD(r1 + 0x18u) = 0x5678;
    DSW(r1 + 0x56u) = 1;
    DSD(s0 + 0x2Cu) = 0x1234;
    DSD(s1 + 0x2Cu) = 0x1234;
    DSW(0x00107D00u) = 0;                    /* B[0] = 0: the gate closed */
    DSW(0x00107D02u) = 0;                    /* B[1] */
    DSW(0x00107D0Cu) = 0;                    /* A[0] */
    DSW(0x00107D0Eu) = 0;                    /* A[1] */
    DSW(0x00107D04u) = 3;                    /* 0x3A6D4's B[0]: a trap */
    DSW(0x00107D08u) = 0x4321;
    DSW(0x00107D10u) = 3;                    /* 0x3A43C's B[0]: a trap */
    DSW(0x00107D14u) = 0x4321;
    DSD(DS_001077A8) = 0;
    DSD(DS_001077A8 + 4u) = 0;
    DSD(DS_00100AF0) = DSD(s0 + 0x20u);
    DSD(DS_00100AF0 + 4u) = DSD(s1 + 0x20u);
    DSD(DS_00100AB0) = 0x1000;
    DSD(DS_00100AB0 + 8u) = 0x2000;
}

static void check_u6_3a588(void)
{
    u32 s0 = DS_001077B0;
    u32 s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS;
    u32 r1 = FIGHT_RECS + 0x100u;
    u8 sv_slots[0x160];
    u8 sv_d00[0x30];
    u8 sv_ab0[0x50];
    u16 sv_78f6 = DSW(DS_001078F6);
    u32 sv_4ec = DSD(DS_001014EC);
    u16 sv_4b00 = DSW(DS_00104B00);
    u8 sv_4b1d = DSB(DS_00104B1D);
    u8 sv_5b38 = DSB(DS_00105B38);
    u8 sv_5b36 = DSB(DS_00105B36);
    u8 sv_5b3a = DSB(DS_00105B3A);
    u32 sv_4abc = DSD(DS_00104ABC);
    u16 sv_w0 = DSW(0x000A6728u);
    u32 sv_d8 = DSD(0x000A3528u + 8u);
    u8 sv_e11a = DSB(0x000DE11Au);
    tf_snap(sv_slots, 0x001077A0u, 0x160u);
    tf_snap(sv_d00, 0x00107D00u, 0x30u);
    tf_snap(sv_ab0, 0x00100AB0u, 0x50u);

    /* The raw's references: 0x3A650 stores 0x3A588 (the dword at 0x3A689). */
    CHECK_EQ_INT((int)DSD(0x0003A689u), 0x0003A588);

    /* 0x3A5A5/0x3A5B1: phase 0 arms +0x58 and touches nothing else. */
    pose_3a588_seed(s0, s1, r0, r1);
    DSB(s0 + 0x58u) = 0;
    DSB(s0 + 0x90u) = 0x55;
    fighter_pose_3a588(s0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 0x55);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)0xDEADBEEFu);

    /* 0x3A59F/0x3A5A1: +0x58 above 1 returns before anything. */
    pose_3a588_seed(s0, s1, r0, r1);
    DSB(s0 + 0x58u) = 2;
    DSB(s0 + 0x90u) = 0x55;
    fighter_pose_3a588(s0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 2);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 0x55);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), (int)0xDEADBEEFu);

    /* Phase 1 with B[0] = 0: the 0xC9030[3] stream at 3.0, the re-anchor
     * (x kept, y = 0), +0x58 = 2, +0x90 = 2 and no snap. */
    pose_3a588_seed(s0, s1, r0, r1);
    fighter_pose_3a588(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000D26C8);    /* 0xC9030[3] */
    CHECK_EQ_INT((int)DSD(r0 + 0x20u), 0x40400000);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSB(r0 + 0x52u), 0);
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS), 0x17DB);   /* the stream's first id */
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 2);          /* 0x3A642, not 1 or 3 */
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);     /* B[0] = 0: no snap */
    CHECK_EQ_INT((int)DSD(r1 + 8u), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), (int)0xDEADBEEFu);

    /* Character 0 reads 0xC9030[0] = 0xE7372 (first word 0x1083). */
    pose_3a588_seed(s0, s1, r0, r1);
    DSB(s0 + 0x7Au) = 0;
    fighter_pose_3a588(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000E7372);
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS), 0x1083);

    /* B[0] = 3, A[0] = 0x4321, +0x90 = 0: the snap (0x3A634/0x3A639). */
    pose_3a588_seed(s0, s1, r0, r1);
    DSW(0x00107D00u) = 3;
    DSW(0x00107D0Cu) = 0x4321;
    fighter_pose_3a588(s0, 0u);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x4321);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x4321 - 0x1000);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 2);

    /* A negative A word is sign-extended (0x3A60B `sar ebx,0x10`). */
    pose_3a588_seed(s0, s1, r0, r1);
    DSW(0x00107D00u) = 3;
    DSW(0x00107D0Cu) = 0x8001;
    fighter_pose_3a588(s0, 0u);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), (int)0xFFFF8001u);

    /* The 0x3A578 table: +0x90 = 1 and 4 skip the snap; 5 is past it. */
    pose_3a588_seed(s0, s1, r0, r1);
    DSB(s0 + 0x90u) = 1;
    DSW(0x00107D00u) = 3;
    DSW(0x00107D0Cu) = 0x4321;
    fighter_pose_3a588(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 2);

    pose_3a588_seed(s0, s1, r0, r1);
    DSB(s0 + 0x90u) = 4;
    DSW(0x00107D00u) = 3;
    DSW(0x00107D0Cu) = 0x4321;
    fighter_pose_3a588(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);

    pose_3a588_seed(s0, s1, r0, r1);
    DSB(s0 + 0x90u) = 5;
    DSW(0x00107D00u) = 3;
    DSW(0x00107D0Cu) = 0x4321;
    fighter_pose_3a588(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x4321 - 0x1000);

    /* B[0] = 5 closes the snap (0x3A612 `cmp edx,5; je`). */
    pose_3a588_seed(s0, s1, r0, r1);
    DSW(0x00107D00u) = 5;
    DSW(0x00107D0Cu) = 0x4321;
    fighter_pose_3a588(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);

    /* The B/A words are the self side's: B[1] = 3, A[1] = 0x4321 leave side 0's
     * gate closed (and the seed's traps stay unread). */
    pose_3a588_seed(s0, s1, r0, r1);
    DSW(0x00107D02u) = 3;
    DSW(0x00107D0Eu) = 0x4321;
    fighter_pose_3a588(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);

    /* The side-1 mirror: EBX = 1 drives slot 1 and record 1 from B[1]/A[1]. */
    pose_3a588_seed(s0, s1, r0, r1);
    DSW(0x00107D02u) = 3;
    DSW(0x00107D0Eu) = 0x6543;
    fighter_pose_3a588(s1, 1u);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x000D26C8);
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x20u), 0x17DB);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 2);
    CHECK_EQ_INT((int)DSB(s1 + 0x90u), 2);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0x6543);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0x6543 - 0x2000);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 1);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)0xDEADBEEFu);

    /* The wiring: actors_init registered 0x3A588, and 0x3531C case 10 resolves
     * slot+0x10 and calls it with (EAX = slot, EBX = side). */
    CHECK(fn_resolve(0x3A588u) == (void (*)(void))fighter_pose_3a588,
          "actors_init registered 0x3A588 as fighter_pose_3a588");
    pose_3a588_seed(s0, s1, r0, r1);
    DSB(s0 + 0x53u) = 0x0A;
    DSD(s0 + 0x10u) = 0x0003A588u;
    DSW(DS_001078F6) = 0;
    DSD(DS_001077A8) = s0;
    fighter_state_3531c(0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000D26C8);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 2);

    tf_put(sv_slots, 0x001077A0u, 0x160u);
    tf_put(sv_d00, 0x00107D00u, 0x30u);
    tf_put(sv_ab0, 0x00100AB0u, 0x50u);
    DSW(DS_001078F6) = sv_78f6;
    DSD(DS_001014EC) = sv_4ec;
    DSW(DS_00104B00) = sv_4b00;
    DSB(DS_00104B1D) = sv_4b1d;
    DSB(DS_00105B38) = sv_5b38;
    DSB(DS_00105B36) = sv_5b36;
    DSB(DS_00105B3A) = sv_5b3a;
    DSD(DS_00104ABC) = sv_4abc;
    DSW(0x000A6728u) = sv_w0;
    DSD(0x000A3528u + 8u) = sv_d8;
    DSB(0x000DE11Au) = sv_e11a;
}

```

and `    check_u6_3a588();` after `    check_u6_37dcc();`.

- [ ] **Step 2: Run to verify it fails**: compile error naming `fighter_pose_3a588`.

- [ ] **Step 3: Implement.** In `fighter.c`, after the closing brace of `fighter_pose_3a6d4` (the line `    DSB(ctx[3] + 0x90u) = 3u;                               /* 0x3A78E */` then `}`), insert:

```c

/* PORT: 0xC9030 (the 0x3A650 family's per-character animation-stream table,
 * read at 0x3A5CA) has no symbols.h name. */
#define FIGHT_ANIM_3A588  0x000C9030u

/* 0x3A588 — record gameplay-u6 §U6.3. The 0x3A650 pose family's per-frame
 * handler 0x3531C case 10 calls through slot+0x10 (0x3A650 stores it at
 * 0x3A686, the dword at 0x3A689 its only reference). The body is 0x3A43C's
 * with the 0xC9030 stream table, the 0x3A650 setter's globs (B = 0x107D00 +
 * side*2, A = 0x107D0C + side*2) and +0x90 = 2 at the end. Phase 0 sets +0x58
 * = 1; phase 1 starts the self record's 0xC9030[char] stream at 3.0,
 * re-anchors the self record (x kept, y = 0), sets +0x58 = 2 and +0x90 = 2,
 * and, when B[side] is neither 0 nor 5 and (u8)(+0x90 - 1) > 3, snaps the self
 * x to A[side] (the jump table at 0x3A578 sends 1..4 to 0x3A63E, past the
 * snap). Phases above 1 return. The raw takes EAX = slot, EBX = side; the ctx
 * swap overwrites EAX, so only the side is read. 0x2BC30 returns with RET 4,
 * popping the 0x3A5BD push, so from 0x3A5DA on the ESP offsets name ctx[1]
 * (the side) and ctx[5] (rec_self). */
void fighter_pose_3a588(u32 slot, u32 side)
{
    u32 ctx[6];
    u8 phase;
    (void)slot;
    fighter_ctx_swap(ctx, side);                            /* 0x3A58B..0x3A58F 0x33A10 */
    phase = DSB(ctx[3] + 0x58u);                            /* 0x3A594/0x3A598 */
    if (phase == 0u) {                                      /* 0x3A59D/0x3A5A5 */
        DSB(ctx[3] + 0x58u) = 1u;                           /* 0x3A5B1 */
        return;
    }
    if (phase != 1u) return;                                /* 0x3A59F/0x3A5A1 */
    actors_anim_begin(ctx[5],                                /* 0x3A5BD..0x3A5D5 0x2BC30 */
                      DSD(FIGHT_ANIM_3A588
                          + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                      0x40400000u);
    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x18u), 0u);        /* 0x3A5DA..0x3A5E7 0x188AC */
    DSB(ctx[3] + 0x58u) = 2u;                               /* 0x3A5F0 */
    {
        s32 a = (s32)(s16)DSW(DS_00107D0C + ctx[1] * 2u);   /* 0x3A5F8/0x3A60B */
        s32 b = (s32)(s16)DSW(DS_00107D00 + ctx[1] * 2u);   /* 0x3A5FF/0x3A608 */
        if (b != 0 && b != 5) {                             /* 0x3A60E..0x3A615 */
            if ((u8)(DSB(ctx[3] + 0x90u) - 1u) > 3u)        /* 0x3A61B..0x3A625 */
                hit_anchor_x(ctx[1], (u32)a);               /* 0x3A634..0x3A639 0x188DC */
        }
    }
    DSB(ctx[3] + 0x90u) = 2u;                               /* 0x3A642 */
}
```

`fighter.h` after `void fighter_pose_3a6d4(u32 slot, u32 side);`:

```c

/* 0x3A588 (record gameplay-u6 §U6.3). The 0x3A650 pose family's per-frame
 * handler: 0x3A43C's body with the 0xC9030[char] stream, the 0x107D00/0x107D0C
 * B/A words and +0x90 = 2. 0x3531C case 10 resolves it from slot+0x10;
 * registered in actors_init. EAX = slot (dead), EBX = side. */
void fighter_pose_3a588(u32 slot, u32 side);
```

`actors.c` after `    fn_register(0x3A6D4u, (void (*)(void))fighter_pose_3a6d4);`:

```c
    /* PORT: record gameplay-u6 §U6.3. Their sibling 0x3A588, which the
     * 0x3A650 setter stores in slot+0x10 at 0x3A686 (the dword at 0x3A689 is
     * its only reference). */
    fn_register(0x3A588u, (void (*)(void))fighter_pose_3a588);
```

- [ ] **Step 4: Run the suite** ⇒ `all checks passed`.

- [ ] **Step 5: Mutation proofs**: `DSB(ctx[3] + 0x90u) = 2u;` → `3u` ⇒ `3 != 2`; the A read `DS_00107D0C` → `DS_00107D08` ⇒ `17185 != -32767`; the B read `DS_00107D00` → `DS_00107D04` ⇒ `-4096 != 22136`; `FIGHT_ANIM_3A588 0x000C9030u` → `0x000C9008u` ⇒ `861862 != 861896`; the `fn_register(0x3A588u, …)` line → `(void)fighter_pose_3a588;` ⇒ `actors_init registered 0x3A588 as fighter_pose_3a588`.

- [ ] **Step 6: The pinned miss set.** In `test_platform.c`: `adds three\n * pairs` → `adds two\n * pairs`; replace

```c
 *   0x5D812 frontend_mode_1b_step: the runtime stub again, f = 0x77A;
 *   0x3A588 fighter_state_3531c: an UNPORTED state-10 callback (the +0x10
 *     pointer 0x3A650 stores), 5353 hits from f = 0x927.
```
with
```c
 *   0x5D812 frontend_mode_1b_step: the runtime stub again, f = 0x77A.
```
replace ` * 0x23208 (hit_reaction_apply, f = 0x824).` with

```c
 * 0x23208 (hit_reaction_apply, f = 0x824), 0x37DCC (anim_indirect, reached
 * only once 0x3A588 is ported) and 0x3A588 (fighter_state_3531c, from f = 0x927).
```
and delete the row `    { 0x3A588u, "fighter_state_3531c" },`. The array keeps the two harmless rows.

- [ ] **Step 7: Replay** (files `t4_*`). Expected (record §U6.7 row "all four"): `all checks passed`, `distinct=4` (`0x5D812 actor_spawn hits=6302`, `0x5D812 set_dead hits=5915`, `0x29D60 …`, `0x5D812 frontend_mode_1b_step …`); `FIRST UNEXPLAINED capture 203`, `window from capture 90`, `trace: first difference f=871 (2161)`. The path (record §U6.8):

```bash
python3 - <<'EOF'
import sys; sys.path.insert(0, 'tools'); import gp_session as gs
prev = None
for l in open('/tmp/pr_u6a_gp/gp-idle-loss/trace.txt'):
    r = gs.parse(l)
    if r and r['kind'] == 'T' and r['mode'] != prev:
        print('%X mode=%X s0_5a=%X s1_5a=%X' % (r['f'], r['mode'], r['s0_5a'], r['s1_5a'])); prev = r['mode']
EOF
```

Expected lines include `CD0 mode=8 s0_5a=78`, `1BB9 mode=7`, `1C2A mode=9`, `1D95 mode=13`, `2040 mode=1E`, `2045 mode=17`. Interpret any other outcome with record §U6.9's table (stop on the rows that say stop). Mutation: restore the deleted `0x3A588` row ⇒ `FAIL …test_platform.c:…: 4 != 5`; delete it again.

- [ ] **Step 8: Commit** (the five files; `game: port the 0x3A650 pose family's case-10 handler 0x3A588 (record gameplay-u6 §U6.3)`).

---

### Task 5: Re-pin the idle-loss ratchets, classify the new first difference, the gate

Record §U6.8, §U6.9.

**Files:** `Makefile` (the `GP_IDLE_LOSS_TRACE_MIN_FIRST` block and its comment; the `GP_IDLE_LOSS_MIN_FIRST` comment's last line); record §U6.21 (appended in Task 6).

- [ ] **Step 1: Measure the three values on the Task 4 head** (the `gp_compare --report` lines of Task 4 Step 7). Expected: frames `203`, start `90`, trace `2161`. Pin only what is measured; if a value differs, apply record §U6.9.

- [ ] **Step 2: Edit the Makefile.** Replace the `TRACE_MIN_FIRST` comment and value block (from `# TRACE_MIN_FIRST: first differing f=0x828` through `GP_IDLE_LOSS_TRACE_MIN_FIRST = 2088`) with:

```make
# TRACE_MIN_FIRST: first differing f=0x871 (decimal 2161) in rng, port against the capture
# (capture 6A6BD8FA, port AEE5C3BD), measured after U6a ported 0x3640C, 0x23208, 0x37DCC and
# 0x3A588 (record gameplay-u6 §U6.7/§U6.8; it was 0x828 = 2088 with 0x23208 unregistered).
# The port's type-0 crowd-effect walk (fight_4b144 <- fight_4aad0 <- fight_effects_pass) draws
# the same rng one frame later than the original (0x872 against 0x871); cause not isolated, not
# an unregistered callback (the replay's miss log holds only the harmless pairs); a named gap
# owned by a fight-effects unit. No run-to-run bound (record §G.19); raise it when it improves.
GP_IDLE_LOSS_TRACE_MIN_FIRST = 2161
```

- [ ] **Step 3: The pins hold and each can fail**

```bash
make gp-oracle $V > $S/t5_oracle.txt 2>&1; echo "rc=$?"; grep -E 'ratchet N|capture:' $S/t5_oracle.txt
make gp-oracle $V GP_IDLE_LOSS_TRACE_MIN_FIRST=2162 2>&1 | grep FAIL; echo "rc=$?"
make gp-oracle $V GP_IDLE_LOSS_MIN_FIRST=204 2>&1 | grep FAIL
make gp-oracle $V GP_IDLE_LOSS_MAX_START=89 2>&1 | grep FAIL
```

Expected: `rc=0` with `first unexplained 203, ratchet N 203 ok`, `first differing 2161, ratchet N 2161 ok`, `capture: poll.log sha256 773e2647..8c8447, 8173 frames: matches the pin`; then `trace: FAIL: first differing 2161 < ratchet N 2162`, `frames: FAIL: first unexplained 203 < ratchet N 204`, `frames: FAIL: window starts at capture 90 (raw 1744) > pinned start 89`.

- [ ] **Step 4: Classify `f = 0x871` with a throw-away backtrace (never committed).** Apply to `port/src/game/rng.c`:

```python
p = 'port/src/game/rng.c'; s = open(p).read()
s = s.replace('#include "symbols.h"\n', '#include "symbols.h"\n#include <execinfo.h>\n#include <stdio.h>\n')
s = s.replace('u32 rng_next(u32 range)\n{\n', 'u32 rng_next(u32 range)\n{\n    { u32 f = DSW(DS_000EF6DC); if (f >= 0x868u && f <= 0x87Cu) { void *b[8]; int n = backtrace(b, 8); fprintf(stderr, "RNG f=%X range=%X\\n", (unsigned)f, (unsigned)range); backtrace_symbols_fd(b + 1, n - 1, 2); } }\n')
open(p, 'w').write(s)
```

then

```bash
cmake --build build >/dev/null
python3 tools/gp_session.py port-script --scenario gp-idle-loss --capture data/k11-captures/gp-idle-loss --out $S/bt.script --end 2200
PR_GP_DUMP=$S/btdump PR_GP_SCRIPT=$S/bt.script PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep -A5 '^RNG' | awk '{print $1, $2, $3, $4, $NF}' > $S/t5_bt.txt
git checkout port/src/game/rng.c && cmake --build build >/dev/null && git diff --stat port/src/game/rng.c
```

Expected in `$S/t5_bt.txt` (record §U6.8): `RNG f=872` draws with `fight_4b144`, `fight_4aad0`, `fight_effects_pass`, `game_mode_04_step` frames (ranges `2`, `1200`, `1200`), `RNG f=877 range=64` from `ai_pick`; the `git diff --stat` is empty. Record verbatim in §U6.21.

- [ ] **Step 5: The full gate**

```bash
make verify $V > $S/t5_verify.txt 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $S/t5_verify.txt | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
make audio-render AUDIO_WAV=/tmp/pr_u6a.wav >/dev/null && cmp /tmp/pr_u6a.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-EQUAL
grep -E '^(k11_compare)' $S/t5_verify.txt | diff - <(grep -E '^(k11_compare)' $S/t0_gate_lines.txt) && echo K11-EQUAL
grep -E '^gp_compare' $S/t5_verify.txt; grep -E 'diff-verify:' $S/t5_verify.txt
python3 tools/port_progress.py
```

Expected: `verify-exit=0`, `ORACLES-EQUAL`, `WAV-EQUAL`, `K11-EQUAL`, the gp-oracle lines of Step 3 (the trace line now `2161 … ok`), `diff-verify: 6/6 functions VERIFIED`, `771 1203 64`, `731 731 100`.

- [ ] **Step 6: Commit** (`git add Makefile`; `build: re-pin the gp-idle-loss trace ratchet to 2161 after the four U6a ports (record gameplay-u6 §U6.8)`).

---

### Task 6: Closure

**Files:** `docs/superpowers/plans/2026-10-01-gameplay-u6-derivations.md` (append §U6.21); `docs/PROGRESS.md` (append one paragraph).

- [ ] **Step 1: Record §U6.21 "U6a execution"**: the commits; per task the replay's `fn-miss` lines and the three gp_compare values (Tasks 0–4); the path table (Task 4 Step 7) against record §U6.8; Task 5's outputs verbatim; every mutation's first `FAIL` line; any plan-vs-raw correction with its address (raw wins). Restate the named gaps that remain from §U6.19 items 1–3 and 8.

- [ ] **Step 2: `docs/PROGRESS.md`**: append "Gameplay U6a: the idle-loss divergences" — four functions ported from the raw (`0x3640C`, `0x23208`, `0x37DCC`, `0x3A588`; none in the Ghidra function list, so `771 1203 64` / `731 731 100` unchanged), the replay's pinned set back to the harmless pairs, round 1 now a CPU KO (1243 frames against the original's 1148), the trace ratchet 2088 → 2161, the frame ratchet 203 and the start 90 unchanged, divergence 2b (the crowd-effect walk one frame late) named with its owner, `0x23208`/`0x3A588` not E1-verifiable (closures with indirect transfers).

- [ ] **Step 3: Gate and commit**

```bash
make verify $V > $S/t6_verify.txt 2>&1; echo "verify-exit=$?"
git add docs/superpowers/plans/2026-10-01-gameplay-u6-derivations.md docs/PROGRESS.md
git commit -m "$(cat <<'EOF'
docs: gameplay U6a closure: four idle-loss callbacks ported, trace ratchet 2161 (record gameplay-u6 §U6.21)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

## Shared-file touch points (additive; for the controller's merge order)

| file | region | what |
|---|---|---|
| `port/src/game/fighter.c` | before `/* 0x36300. …` (2 functions); after `fighter_pose_3a6d4`; before `/* 0x23250 — record §48-P.` | four new functions and two `#define`s |
| `port/src/game/fighter.h`, `actors.c` | next to `fighter_36280`, `fighter_pose_3a6d4`, `fighter_47bfc` | prototypes, two wrappers, four `fn_register` lines |
| `port/tests/test_fight.c` | end of file | the U6 test block (U5/U7/U8/U11 may also append: textual, keep both) |
| `port/tests/test.h` | `TEST_CASES` after `X(test_table_reached)` | one line |
| `port/tests/test_platform.c` | `k_miss_gp_idle_loss` and its comment | three rows removed (U4's set); a unit that adds rows for another scenario does not touch these |
| `port/tests/diff_runner.c`, `tools/diff_verify.py`, `tools/tests/test_diff_verify.py` | the binding table, `SPECS`, the three name lists | two specs (E2 or P may add specs: append-only lists) |
| `Makefile` | `GP_IDLE_LOSS_TRACE_MIN_FIRST` block | value 2088 → 2161 and its comment |
| `docs/PROGRESS.md` | end | one paragraph |

## Execution notes

- **Order:** 0 → 1 → 2 → 3 → 4 → 5 → 6; the order of 1–4 is the record's §U6.7 (each step shrinks the pinned set by exactly the ported function). Do not reorder.
- **Model tier:** Tasks 1–4 (transcription of given code plus measured checks): a mid tier (Sonnet). Task 5 (re-pin judgement, the §U6.9 table, the backtrace) and the per-task reviews: a strong tier (Opus).
- **Gate:** each task's suite and replay as written; `make verify` with `$V` in Tasks 0, 5 and 6 (and before any merge). Cost per verify on this host: about 13 minutes; per replay about 140 s and 410 MB of `/tmp`.
- **Never** re-capture `gp-idle-loss` (record §G.24 item 5).
