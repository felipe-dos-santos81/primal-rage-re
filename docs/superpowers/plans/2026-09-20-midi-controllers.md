# MIDI Controllers and the Mid-Note Frequency Path — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port `SBPRO2.MDI`'s channel-controller handler so a note's OPL registers are re-applied when a controller changes — closing audio item 10 (mid-note frequency/pitch bend) and item 1 (the TL level term).

**Architecture:** The driver's `FUN_0000_3b54` stores controller state, then re-runs the mask-driven family applier `FUN_0000_3184` across the channel's active voices. The port adopts that shape: `apply_patch()` becomes `fam_apply(ch, mask)`, controller state and `midi_control()` live in `sequencer.c`, and a new pure `pitch.{c,h}` owns the driver's pitch tables and the `0x35fa` frequency routine, retiring `NOTE_TAB`.

**Tech Stack:** C11, the port's `mem[]` model, CMake, the repo's `make verify` ladder, Python 3 (`tools/opl_seq.py` oracle), capstone (derivation only).

**Spec:** `docs/superpowers/specs/2026-09-20-midi-controllers-design.md`.

## Global Constraints

- Fixed profile; register-level fidelity. Original state lives in `mem[]`; pointer globals are `mem[]` offsets and are consumed as `mem + DSD(...)`.
- SDL appears only in `port/src/host.c` and `port/src/main.c`.
- `data/` is read-only. `tools/` existing files are read-only **except `tools/opl_seq.py`**, which this cycle updates by explicit approval — and it must stay an independent implementation (its own structure), never a transliteration of `sequencer.c`.
- Comments in `port/src` are only `/* PORT: ... */`, `/* TODO(verify): */`, and address tags. No comments in `port/tests` beyond the existing style.
- Build with **0 warnings**. No new dependencies.
- Tests: one `int test_X(void)` per file, declared in `port/tests/test.h` and called from `port/tests/run_tests.c`; only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`. `port/src` is a PUBLIC include dir. `port/CMakeLists.txt` lists sources explicitly — a new source must be added there.
- `game_init()` may run only once per process; any test that calls it must be env-gated (`PR_*_DUMP` pattern).
- Governing oracle: `oracle C-vs-Python: <N> writes byte-exact`. The capture oracle is informational and must advance, with its first divergence named.
- `make verify` must pass at the end of every task whose change can move an oracle.
- On plan-vs-raw conflict, the RAW wins; record the correction.
- Never `git add -A`. Commit style: `<area>: <what changed>`.

## File Structure

| file | responsibility |
|---|---|
| `port/src/platform/audio/pitch.h` (new) | the frequency interface: `pitch_bend_of`, `pitch_lookup` |
| `port/src/platform/audio/pitch.c` (new) | the driver's 192-word + 2×96-byte tables and the `0x35fa` arithmetic; pure, no `mem[]`/`S` access |
| `port/src/platform/audio/sequencer.c` (modify) | `fam_apply(mask)` replaces `apply_patch`; controller state in `S`; `midi_control`; retire `NOTE_TAB` |
| `port/src/platform/audio/sequencer.h` (modify) | expose nothing new unless a test needs it |
| `port/CMakeLists.txt` (modify) | add `platform/audio/pitch.c` |
| `port/tests/test_pitch.c` (new) | `pitch_lookup` / `pitch_bend_of` units |
| `port/tests/test_sequencer.c` (modify) | controller units; capture oracle; retire the TL exclusion in the last task |
| `port/tests/test.h`, `port/tests/run_tests.c` (modify) | register `test_pitch` |
| `tools/opl_seq.py` (modify, approved) | the same controller/frequency model, independently written |
| `port/spec/audio.md` (modify) | items 1 and 10 dispositions; the reduction note |
| `docs/superpowers/plans/2026-09-20-midi-controllers-derivations.md` (new) | the raw-byte derivation record (Task 1) |

---

### Task 1: Derive the controller, family-apply and frequency semantics

Produce the raw-byte record every later task implements. No port code changes.

**Files:**
- Create: `docs/superpowers/plans/2026-09-20-midi-controllers-derivations.md`
- Read: `data/game/C/SBPRO2.MDI` (read-only), `/var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/opencode/ghidra_out/SBPRO2.MDI_decompiled.c` (regenerate if absent — see the repo's `make re-*` targets)

- [ ] **Step 1: Re-derive the controller dispatch**

Disassemble/read `0x3b54` and record, for each branch, the store and the family mask:

| input | store | mask |
|---|---|---|
| `0xE0` | `[0x1929]=a`, `[0x1939]=b` | `0x01` |
| ctrl 6 | `[0x18f9]=b` | none (returns) |
| ctrl 7 | `[0x1909]=b` | `0x40` |
| ctrl 11 | `[0x1949]=b` | `0x40` |
| ctrl 1 | `[0x1959]=b` | `0x80` |
| ctrl 10 | `[0x1919]=b` | `0x08` |
| ctrl 64 | `[0x1969]=b` | `0x3b1e` when `b < 0x40` |
| ctrl 121 | reset block | `0xC1` |
| ctrl 123 | per-voice all-notes-off `0x39cc` | none |

Confirm the reset block values (`0x1929=0`, `0x1939=0x40`, `0x1949=0x7f`, `0x1959=0`, `0x1969=0`) and the re-apply loop: for each voice `[0x1485][v] != 0` with `[0x14c1][v] & 0xf == ch`, `[0x1539][v] |= mask; FUN_0000_3184(v)`.

- [ ] **Step 2: Re-derive `0x3184`'s per-family writes**

For each mask bit, record the register, the source byte, and any controller input:

- `0x80` → `0x20` family: patch `[3]`/`[9]`, AM bit set when `[0x1959][ch] >= 0x40`.
- `0x40` → TL `0x40`: the exact fold from `0x3184` lines `0x30`-`0x44` + the `0x34xx` TL branch (disassemble with capstone, quote the bytes). Candidate: `factor = ((volume * expression * 2) >> 8)`, re-scaled by `[0x1511][v]`, then per operator `TL = patch_TL | (~scaled & 0x3f)` when `[0x18e5][v] & bit`, else `patch_TL`. **Record the exact ops, including which bit of `[0x18e5]` gates each operator.**
- `0x20` → `0x60`/`0x80`: patch `[5]`/`[6]`, `[11]`/`[12]`.
- `0x10` → `0xE0`: patch `[7]`/`[13]`, carrier written first.
- `0x08` → `0xC0`: `patch[8]`, with `0x30`/`0x20`/`0x10` from `[0x1919][ch]` (`<0x1c` → `0x20`, `>99` → `0x10`, else `0x30`).
- `0x01` → `0xA0`/`0xB0`: the `0x35fa` routine, with the key-on bit on a key-on and preserved on a re-apply.

- [ ] **Step 3: Re-derive `0x35fa` and record the tables**

Record the arithmetic exactly as in the spec §5, plus the table extents and how to regenerate them:

```sh
python3 - <<'EOF'
import struct
d=open('data/game/C/SBPRO2.MDI','rb').read()
tbl=[struct.unpack_from('<h',d,0x7fd+2*i)[0] for i in range(192)]  # signed
oct_=[b for b in d[0x97d:0x97d+96]]
sem=[b for b in d[0x9dd:0x9dd+96]]
print(len(tbl), tbl[:8], tbl[-4:])
print(len(oct_), sorted(set(oct_)), len(sem), sorted(set(sem)))
EOF
```

Expected: `192 [690, 692, 695, 697, 700, 702, 705, 707] [65189, 65192, 65194, 65197]` (the negatives are the signed fold the routine tests with `w < 0`), `96 [0..7]`, `96 [0..11]`.

**Block/octave convention — must be settled here.** A naive transcription
(`idx = ax>>4`, `block = T97D[idx] - 1 + (v < 0)`) yields **block 6** for note 84
and **block 3** for index 54, but the capture-pinned anchors are **block 5** and
**block 2** respectively — one too high. Work the block/octave convention out
against the anchors (note 84 → block 5 fnum `0x2B2`; index 54 → block 2 fnum
`0x3CF`, the first key-on) and record the corrected form, including whether the
fold is `block = oct - 1` with a different `oct` source, or the table/step
indexing differs (`t97d`/`t9dd` offset, `(2*ax) & 0x1f`). Task 2's test encodes
the anchors, so the routine must reproduce them.

- [ ] **Step 4: Confirm the defaults against the capture**

From `data/audio-captures/prage_000.dro` (via `tools/opl_trace.py`), record the first ctrl-7, ctrl-11, ctrl-10 and `0xE0` events and the register writes they trigger, so the pre-first-controller defaults are evidence, not assumption.

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/plans/2026-09-20-midi-controllers-derivations.md
git commit -m "docs: derive the MDI controller, family-apply and frequency semantics"
```

---

### Task 2: `pitch.{c,h}` — the frequency routine, retiring `NOTE_TAB`

**Files:**
- Create: `port/src/platform/audio/pitch.h`, `port/src/platform/audio/pitch.c`
- Create: `port/tests/test_pitch.c`
- Modify: `port/src/platform/audio/sequencer.c` (the two `NOTE_TAB[...]` uses in `key_on`)
- Modify: `port/CMakeLists.txt`, `port/tests/test.h`, `port/tests/run_tests.c`

**Interfaces:**
- Produces: `s32 pitch_bend_of(int wheel14, int scale);` and `void pitch_lookup(int index, s32 bend, u8 *a0, u8 *b0);`
- Consumes: nothing (pure).

- [ ] **Step 1: Write the failing test**

Create `port/tests/test_pitch.c`:

```c
#include "platform/audio/pitch.h"
#include "test.h"

int test_pitch(void)
{
    int before = g_failures;

    /* Centred: the routine must reproduce the capture-pinned anchors. b0 is the
     * payload without the key-on bit; a key-on ORs 0x20. */
    {
        u8 a0, b0;
        pitch_lookup(84, 0, &a0, &b0);
        CHECK_EQ_INT(a0, 0xB2);
        CHECK_EQ_INT(b0, 0x16);          /* block 5, fnum 0x2B2 */
        pitch_lookup(79, 0, &a0, &b0);
        CHECK_EQ_INT(a0, 0x05);
        CHECK_EQ_INT(b0, 0x16);          /* block 5, fnum 0x205 */
    }
    /* bend_of: a centred wheel (0x2000) is zero at any scale. */
    CHECK_EQ_INT(pitch_bend_of(0x2000, 12), 0);
    /* A full-up wheel with scale 1 is (0x1fff >> 5) * 1 = 0xff. */
    CHECK_EQ_INT(pitch_bend_of(0x3fff, 1), 0xFF);
    return g_failures - before;
}
```

- [ ] **Step 2: Run it and watch it fail**

Run: `make build && ./build/run_tests 2>&1 | grep -i pitch`
Expected: link error / FAIL — `pitch_lookup` undefined.

- [ ] **Step 3: Implement `pitch.{c,h}`**

`pitch.h`:

```c
#ifndef PR_PITCH_H
#define PR_PITCH_H
#include "types.h"

/* The driver's note-frequency routine (SBPRO2.MDI 0x35fa-0x36a6). `index` is
 * note+base (melodic) or base (percussion); `bend` is pitch_bend_of(). Writes the
 * 0xA0/0xB0 payload. bend == 0 reproduces the retired NOTE_TAB. */
s32  pitch_bend_of(int wheel14, int scale);
void pitch_lookup(int index, s32 bend, u8 *a0, u8 *b0);
#endif
```

`pitch.c`: the tables as literals generated from the Task-1 command (`static const s16 PITCH_TBL[192]`, `static const u8 T97D[96]`, `static const u8 T9DD[96]`), then:

> The `block`/`oct` lines below are the **naive** transcription and are known to be
> one octave high (block 6 for note 84, block 3 for index 54). Task 1 settled the
> corrected convention — use it here. The test's capture-pinned anchors (block 5,
> block 2) govern; do not adjust the test to match the stub.

```c
s32 pitch_bend_of(int wheel14, int scale)
{
    return (s32)((wheel14 - 0x2000) >> 5) * scale;
}

void pitch_lookup(int index, s32 bend, u8 *a0, u8 *b0)
{
    s32 ax = (bend + (s32)((index & 0xff) << 8) + 8) >> 4;
    int idx, sem, oct;
    s16 v;
    int block;

    if (ax < 0) ax = 0;
    if (ax > 0x5ff) ax = 0x5ff;
    idx = (int)(ax >> 4);
    sem = T9DD[idx];
    oct = T97D[idx];
    v = PITCH_TBL[16 * sem + ((int)(2 * ax) & 0x1f)];
    block = oct - 1 + (v < 0);
    if (block < 0) { block++; v = (s16)(v >> 1); }
    *a0 = (u8)(v & 0xff);
    *b0 = (u8)((block << 2) | ((v >> 8) & 0x03));
}
```

Replace `NOTE_TAB` in `sequencer.c`'s `key_on` with `pitch_lookup(idx, 0, &a0, &b0)` and delete `NOTE_TAB`.

- [ ] **Step 4: Run the tests**

Run: `PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | grep -iE "pitch|C-vs-Python|capture oracle"`
Expected: `pitch` PASS; **`oracle C-vs-Python` unchanged** (`9340 writes byte-exact`); the capture oracle's line unmoved. The capture oracle is the equivalence proof against the old `NOTE_TAB`: every note in the song now goes through `pitch_lookup(index, 0)`, so if it disagreed with `NOTE_TAB` for any used index the capture line would move. If it does move, the driver's routine wins — record the indices and treat the moved line as the new boundary, do **not** adjust the routine to match `NOTE_TAB`.

- [ ] **Step 5: Commit**

```bash
git add port/src/platform/audio/pitch.c port/src/platform/audio/pitch.h port/tests/test_pitch.c port/tests/test.h port/tests/run_tests.c port/CMakeLists.txt port/src/platform/audio/sequencer.c
git commit -m "audio: add the driver pitch routine and retire NOTE_TAB"
```

---

### Task 3: `fam_apply(mask)` replaces `apply_patch`

Pure refactor; both oracles must be unmoved.

**Files:**
- Modify: `port/src/platform/audio/sequencer.c` (`apply_patch` at `:140`)

**Interfaces:**
- Produces: `static void fam_apply(int opl_ch, u8 mask);` with masks `FAM_FREQ 0x01`, `FAM_CONN 0x08`, `FAM_WAVE 0x10`, `FAM_EG 0x20`, `FAM_TL 0x40`, `FAM_AMVIB 0x80`, and `FAM_ALL` = their OR.
- Consumes: `pitch_lookup` (Task 2).

- [ ] **Step 1: Record the pre-refactor oracle lines**

Run: `PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | grep -iE "C-vs-Python|capture oracle"`
Record both lines; they are Task 3's pass condition.

- [ ] **Step 2: Refactor**

In `sequencer.c`, rename `apply_patch`'s body to `fam_apply(int opl_ch, u8 mask)` and gate each write pair on its bit, keeping the existing order:

```c
static void fam_apply(int opl_ch, u8 mask)
{
    const u8 *p = S.voice[opl_ch].patch;   /* cached payload pointer */
    u16 base = OPL_SLOT[opl_ch];
    if (p == NULL) return;
    if (mask & FAM_AMVIB) { opl_write(0x20 + base, p[3]); opl_write(0x23 + base, p[9]); }
    if (mask & FAM_TL)    { opl_write(0x40 + base, p[4]); opl_write(0x43 + base, p[10]); }
    if (mask & FAM_EG)    { opl_write(0x60 + base, p[5]); opl_write(0x63 + base, p[11]);
                            opl_write(0x80 + base, p[6]); opl_write(0x83 + base, p[12]); }
    if (mask & FAM_WAVE)  { opl_write(0xE0 + base, p[7]); opl_write(0xE3 + base, p[13]); }
    if (mask & FAM_CONN)  { opl_write(ch_reg(opl_ch, 0xC0), (u8)(p[8] | 0x30)); }
    if (mask & FAM_FREQ)  { u8 a0, b0; pitch_lookup(idx_of(opl_ch), 0, &a0, &b0);
                            opl_write(ch_reg(opl_ch, 0xA0), a0);
                            opl_write(ch_reg(opl_ch, 0xB0), (u8)(b0 | key_bit(opl_ch))); }
}
```

Add the voice's cached payload pointer `const u8 *patch;` and its pitch `int index;` to `seq_voice` (set in `key_on` from `patches_lookup(key)` and the note+base rule). `key_on` becomes: resolve the patch, store it, then `fam_apply(v, FAM_ALL)` and the key-on writes. (The raw keeps two `ch_reg` orderings: the A0/B0 pair is written before the family block on a key-on and mid-note on a re-apply — keep the existing key-on order to stay byte-exact.)

- [ ] **Step 3: Run the tests**

Run: `PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | grep -iE "C-vs-Python|capture oracle"`
Expected: both lines **identical** to Step 1.

- [ ] **Step 4: Commit**

```bash
git add port/src/platform/audio/sequencer.c
git commit -m "audio: make the register write path mask-driven (fam_apply)"
```

---

### Task 4: Controller state, `midi_control`, and the re-apply loop

**Files:**
- Modify: `port/src/platform/audio/sequencer.c` (`S`, `seq_start`, `process`)
- Modify: `tools/opl_seq.py` (approved)
- Modify: `port/tests/test_sequencer.c`

**Interfaces:**
- Produces: `static void midi_control(u8 status, u8 a, u8 b);` and per-channel state in `S`.
- Consumes: `fam_apply` (Task 3), `pitch_lookup`/`pitch_bend_of` (Task 2).

- [ ] **Step 1: Write the failing test**

Append to `test_sequencer`'s synthetic-bank section (0a/0b style), before the asset gate:

```c
/* Controllers: a bend re-applies only the A0/B0 family, for active voices of
 * that channel only. */
{
    static const u8 ev[] = { 0x90, 0x30, 0x40, 0x7f, 0x00,   /* note on ch0 */
                             0xE0, 0x00, 0x30,                  /* bend ch0 */
                             0x00, 0xE1, 0x00, 0x30,            /* bend ch1 */
                             0x00, 0xB0, 0x07, 0x70 };          /* volume ch0 */
    opl_reset();
    len = build_xmi(bank, ev, sizeof ev);
    CHECK_EQ_INT(seq_load(bank, len), 1);
    seq_start();
    seq_tick();
    u32 before = opl_write_count();
    seq_tick();
    /* The bend and the volume changed families for ch0's voice; ch1's bend
     * changed nothing (no active voice). Record what families were written. */
    int saw_a0 = 0, saw_40 = 0;
    for (u32 i = before; i < opl_write_count(); i++) {
        u16 r = opl_trace_reg(i);
        if ((r & 0xf0) == 0xA0) saw_a0 = 1;
        if ((r & 0xf0) == 0x40) saw_40 = 1;
    }
    CHECK(saw_a0, "bend re-applied the frequency family");
    CHECK(saw_40, "volume re-applied the TL family");
}
```

- [ ] **Step 2: Run it and watch it fail**

Run: `make build && ./build/run_tests 2>&1 | grep -iE "bend|volume"`
Expected: FAIL — no `0xA0`/`0x40` writes yet.

- [ ] **Step 3: Implement the controller state and dispatch**

Add to `S` (`u8` arrays of `SEQ_MIDI_CHANNELS`): `wheel_lsb`, `wheel_msb`, `bend_scale`, `volume`, `expression`, `pan`, `mod`, `sustain`. Initialise in `seq_start`: `wheel_msb = 0x40`, `expression = 0x7f`, others 0.

```c
/* PORT: 0x3b54, the channel-controller handler. Stores the controller, then
 * re-applies the affected register family for every active voice on the same
 * MIDI channel (the raw's 0x3c53 loop). */
static void midi_control(u8 status, u8 a, u8 b)
{
    u8 hi = (u8)(status & 0xf0);
    int ch = status & 0x0f;
    u8 mask;

    if (ch >= SEQ_MIDI_CHANNELS) return;
    if (hi == 0xe0) {
        S.wheel_lsb[ch] = a; S.wheel_msb[ch] = b; mask = 0x01;
    } else if (hi == 0xb0) {
        switch (a) {
        case 6:  S.bend_scale[ch] = b; return;        /* no re-apply */
        case 7:  S.volume[ch] = b; mask = 0x40; break;
        case 11: S.expression[ch] = b; mask = 0x40; break;
        case 1:  S.mod[ch] = b; mask = 0x80; break;
        case 10: S.pan[ch] = b; mask = 0x08; break;
        case 64: S.sustain[ch] = b; return;           /* 0x3b1e path: Task 5 */
        case 121: S.wheel_lsb[ch] = 0; S.wheel_msb[ch] = 0x40;
                  S.expression[ch] = 0x7f; S.mod[ch] = 0; S.sustain[ch] = 0;
                  mask = 0xc1; break;
        case 123: return;                             /* all notes off: parser */
        default:  return;
        }
    } else {
        return;
    }
    for (int v = 0; v < SEQ_OPL_CHANNELS; v++)
        if (S.voice[v].note != SEQ_NOTE_FREE && S.voice[v].midi == ch)
            fam_apply(v, mask);
}
```

In `process()`: `hi == 0xE0` → read `a`,`b`, `midi_control(b_byte, a, b)` (pass the raw status with the channel nibble); `hi == 0xB0` → keep ctrl 0 as bank select, else `midi_control`.

Wire `pitch_bend_of(S.wheel_msb[ch] << 7 | S.wheel_lsb[ch], S.bend_scale[ch])` into `fam_apply`'s `FAM_FREQ` branch (replacing the `0` in Task 3).

- [ ] **Step 4: Update `tools/opl_seq.py` in lockstep**

Give `Sequencer` the same per-channel controller fields and an `apply(ch, mask)` that mirrors `fam_apply`; tokenise `0xE0` as `('bend', ch, lsb, msb)` instead of `('ignore',)`, and route controller tokens through the same dispatch. Keep its own structure (comprehension/table-driven) — it must not become a copy of `sequencer.c`.

- [ ] **Step 5: Run the tests**

Run: `make verify`
Expected: `all checks passed`; **`oracle C-vs-Python` byte-exact over a larger stream**; the capture oracle's first difference **advances past C write 302 (tick 969)**. Record the new line in `port/spec/audio.md` (Task 6).

- [ ] **Step 6: Commit**

```bash
git add port/src/platform/audio/sequencer.c tools/opl_seq.py port/tests/test_sequencer.c
git commit -m "audio: port the MDI channel-controller handler and re-apply loop"
```

---

### Task 5: TL, pan and mod formulas; retire the TL exclusion

**Files:**
- Modify: `port/src/platform/audio/sequencer.c` (`fam_apply`'s TL/C0/0x20 branches)
- Modify: `tools/opl_seq.py`
- Modify: `port/tests/test_sequencer.c` (`documented_excluded`)
- Modify: `port/src/platform/audio/pitch.c` (only if Task 1's derivation changes it)

**Interfaces:**
- Consumes: Task 4's controller state; Task 1's TL fold.

- [ ] **Step 1: Implement the TL fold**

Using Task 1's derivation, in the `FAM_TL` branch compute the channel level and fold it per operator:

```c
/* PORT: 0x3184 TL branch. factor = (volume * expression * 2) >> 8, rescaled by
 * the per-voice [0x1511]; the result attenuates via ~factor, gated per operator
 * by [0x18e5] bits. */
u8 tl_mod(const seq_voice *v, u8 patch_tl, int bit);
```

Apply it to `p[4]`/`p[10]` (the bit per operator comes from Task 1's record). Apply `pan` to the `0xC0` byte and the mod-wheel AM bit to the `0x20` family.

- [ ] **Step 2: Retire the TL exclusion**

In `port/tests/test_sequencer.c`, `documented_excluded` currently returns true for `0x40..0x55`. Reduce it to `0xBD` only, with the comment updated: the TL family is now modelled (item 1 closed).

- [ ] **Step 3: Update `tools/opl_seq.py`** the same way (TL fold, pan, mod).

- [ ] **Step 4: Run the tests**

Run: `make verify`
Expected: `all checks passed`; C-vs-Python byte-exact; **the capture oracle advances further** with the TL family now compared. Record the new first-difference line.

- [ ] **Step 5: Commit**

```bash
git add port/src/platform/audio/sequencer.c tools/opl_seq.py port/tests/test_sequencer.c port/src/platform/audio/pitch.c
git commit -m "audio: model the TL, pan and mod paths and drop the TL exclusion"
```

---

### Task 6: Spec, README and audibility

**Files:**
- Modify: `port/spec/audio.md`, `README.md`
- Read: `make audio-render` output

- [ ] **Step 1: Update the spec**

- Item 1: change to **matched** (derived from volume × expression), and delete the exclusion row.
- Item 10: change to **matched** (family `0x01` re-apply on bend), or name the new first divergence if one remains.
- Reduction: note that `documented_excluded` is now `0xBD` only.
- First-difference history: append the new line from Task 5.

- [ ] **Step 2: Update the README** audio paragraph to the new boundary and the closed items.

- [ ] **Step 3: Regenerate the render and refresh the checklist**

Run: `make audio-render AUDIO_SECONDS=20 AUDIO_WAV=/tmp/pr_title_fm.wav`
Expected: a non-silent WAV; the bend and level changes should be audible. Update the README's audio caveat if the announcer limitation still stands.

- [ ] **Step 4: Full ladder**

Run: `make verify`
Expected: exit 0, 0 warnings, `all checks passed`, `symbols.h` byte-identical.

- [ ] **Step 5: Commit**

```bash
git add port/spec/audio.md README.md
git commit -m "docs: record the controller model and the closed audio divergences"
```

---

## Self-Review

**Spec coverage.** Goal/scope → Tasks 1-5. Architecture (`fam_apply`, `pitch.{c,h}`, controller state) → Tasks 2-4. Controller table → Task 4. `pitch_lookup` → Task 2. TL/pan/mod → Task 5. Verification §8 → tasks' test steps. Sequencing §9 → task order. Risks §10 → Task 1 Step 4 (defaults), Task 4 Step 3 (sustain `0x3b1e` named), Task 6 Step 3 (audibility). Oracle update → Task 4/5 Step 4/3.

**Deferrals (explicit, not placeholders).** The exact TL fold and the `0x3b1e`/`0x39cc` bodies are derived in Task 1 and implemented in Task 5; Task 4 names sustain as a gap until then. Task 3's `idx_of`/`key_bit` helpers are introduced in that task's Step 2.

**Type consistency.** `pitch_bend_of(int,int) -> s32` and `pitch_lookup(int,s32,u8*,u8*)` are used identically in Tasks 2, 3 and 4. `fam_apply(int,u8)` and the `FAM_*` mask values are defined once (Task 3) and used in Tasks 4-5. `midi_control(u8,u8,u8)` is defined in Task 4 only.

