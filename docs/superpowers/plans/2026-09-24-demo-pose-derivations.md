# The demo fight's pose advance — derivation record (Task 1, cycle 6)

**Deliverable.** The raw-byte and runtime derivation of the state-7 T-rex pose
gap, the missing piece's owner chain, the porting plan, the size-gate verdicts,
the measurement plan, the named gaps, the unit-test values and the provenance.
**No porting code**: this record is the only artefact; the temporary trace
instrumentation in `port/src/game/fighter.c` was reverted before the commit
(`git status --short` clean except this file).

**Result in one line.** The design's premise is **wrong on the handler**: the
demo's T-rex pose handler is **`0x3A43C`** (the `0x3A504` setter family), not
`0x39CC8` (the `0x39F40` family) — the raw's `0x3A504` writes `+0x10 = 0x3A43C`
and `+0x54 = 0` (`0x3A53A`/`0x3A532`), and the port's state-7 trace measures
exactly that. `0x39F40` is never called in the demo. Porting `0x3A43C`
temporarily makes port frames 486–489 byte-match captures 838, 839, 841 and 842
(**0 B** each, capture 840 splice-explained) and changes the T-rex's pose at
capture 843 to the correct upright roar; the next divergence is the T-rex's
**screen x (43 px ≈ one frame of its world motion)** at that same frame, which
then drags the camera/scene (port frame 491 diverges scene-wide). The closure is
**2 genuinely-new functions / 245 B** (`0x3A43C` 197 B + `0x39A34` 48 B) plus
registering the already-ported `0x36870` — **the size gate does not trigger**.

---

## 0. Addressing, reproduction and corrections

### 0.1 Conventions

* Ghidra address == linear address == object base + offset (no segment:offset).
  Code object `0x10000`–`0x73B14`; data object `0x80000`–`0x10B0CF`.
* A global Ghidra calls `DAT_0008xxxx` is at **DS offset `0xxxx`** (subtract
  `0x80000`); the port's `DS_000xxxxx` names are the *mem[] index*, i.e. the
  Ghidra VA (`mem.h`: `DSB(o) = mem[o]`, data at `0x80000`).
* **Data pointers stored in the data object are pre-fixup in the raw file**:
  the LE fixup adds the *target object's base* — `+0x80000` for a data target,
  `+0x10000` for a code target. Verified this cycle on the animation streams:
  the raw-file dword `0x00027A58` loads as `0x00037A58` (the idle tick
  `0x37A58`, whose registration the port's byte-exact crouch proves live), and
  `0x00029A34` loads as `0x00039A34`, `0x00026870` as `0x00036870`.
* The sprite-id table is `DS_000A8B30[id & 0x7FFF]` → a resource handle; the
  handle's `>>23` is the INDEX entry and `& 0x7FFFFF` the descriptor record's
  file offset inside it. `S16REX.GRA` is INDEX 55.
* **The demo's state-7 tick counter is `DS_0010150C`, not the test's loop
  index.** `0x2BAF4` (the arena spawn/reset, port `actors.c:210`) zeroes both
  tick counters, so from the state-6 entry the counter restarts; the trace below
  uses `f = DS_0010150C`. The dump index maps as `dump = f + 417` for this run
  (proved in §1.2).

### 0.2 Reproduction

```bash
# the port's temporary, getenv-gated trace (reverted before the commit)
cmake --build build
rm -rf /tmp/pr_frontend_dump
PR_FRONTEND_DET=/tmp/pr_frontend_dump PR_POSE_DUMP=1 \
  PR_GAME_DIR=data/game/C ./build/run_tests
# the driver redirects the child's stderr into /tmp/pr_frontend_dump/run1.log;
# run1/frame_NNNN.raw are the 1381 presented frames (loop 589..1969)

# the first-divergence compare (design §5's witness, re-verified)
python3 -c "a=open('/tmp/pr_frontend_dump/run1/frame_0490.raw','rb').read(); b=open('data/title-captures/frontend/frame_0843.raw','rb').read(); d=[i for i in range(len(a)) if a[i]!=b[i]]; print(len(d),'bytes',len({i//3 for i in d}),'px')"
# -> 16101 bytes 5793 px   (the design's value, re-verified on the unmodified port)
```

The trace instrumentation was three blocks in `port/src/game/fighter.c`,
`getenv("PR_POSE_DUMP")`-gated: a per-side line at the head of
`fighter_state_3531c` (the slot fields + the pset id/position + the two pose
word banks + `AF8`/`AFC`), a line at `fighter_pose_start`, a line at
`fighter_state_3531c` case 10, and a temporary direct call of the raw's
`0x3A43C` body with the raw's `(EAX=slot, EBX=side)` convention (plus a
one-shot DAC dump and the `POSE_3A43C` gate trace). All reverted.

### 0.3 Corrections against the design/brief/records (raw and measurement win)

1. **The handler is `0x3A43C`, not `0x39CC8`.** The design's Evidence says the
   port enters the pose with `+0x10 = 0x39CC8` written by `fighter_pose_start`
   (`0x39F40`) at `0x39F8F`. Measured (port trace, §1.2): the state-7 T-rex
   enters with `+0x52 = 0x10`, `+0x53 = 0x0A`, **`+0x54 = 0`**,
   **`+0x10 = 0x3A43C`**, `+0x58 = 0`, and `fighter_pose_start` **never runs**
   (the `POSE_START` trace never fires). The raw agrees: `0x3A504` writes
   `+0x54 = 0` and `+0x10 = 0x3A43C` (`0x3A532`/`0x3A53A`); `0x39F40` writes
   `+0x54 = 2` and `+0x10 = 0x39CC8` (`0x39F87`/`0x39F8F`). The design's
   `+0x54 = 2` is the other family's signature. The design's `s7_last_change`
   (loop 1078) and the 843/490 compare (16 101 B / 5793 px) **re-verify
   exactly**; only the handler attribution was a source-read slip.
2. **`0x39CC8`'s table and shape are as the design pinned** (re-verified, so
   the correction is only *which* handler the demo uses): the 5-entry table at
   `0x39CB4` = `[0x39CFA, 0x39D07, 0x39D1D, 0x39DDC, 0x39EE4]` (bytes
   `fa9c0300 079d0300 1d9d0300 dc9d0300 e49e0300`), the body
   `0x39CC8..0x39EF9` (last `RET` at `0x39EF8`), cases 0→`+0x58=1`,
   1→`0x39B30`, 2→the `rec+0x36<0` gate/`0x39AC8`/`0x2BC30`, 3→the death pose
   (`0x186D0`/`0x3C16C`/`0x1890C`/`0x188DC`/`0x2AE14(0xBB1DC)`/`0x2C3FC(0x6C)`),
   4→clear `rec+0x28` bit 0x20 and `+0x54`.
3. **The port's pose-setter `glob_b` write is wrong.** `fighter_pose_commit`
   (`fighter.c:3586`) stores `(u16)(edx >> 16)` into `glob_b + side*2`. The raw
   at `0x3A56B` stores **`BX`**, and at every `0x3A5xx`-family call site
   `EBX = slot+0x52`: `0x3AD2A MOV EBX,[ESP+0xC]` (self slot),
   `0x3AD31 MOV BL,[EBX+0x52]`, `0x3AD3D AND EBX,0xFF` — so
   `B[side] = slot+0x52` at the call, not the edx high word. The existing
   assertion `check_pose_entry` (`test_fight.c:2957`, "edx3 >> 16") pins the
   port's wrong value; it passes only because its seed (`edx3 = 0`,
   `slot+0x52 = 0`) makes both forms 0. **Task 2 must fix the store and update
   that assertion with this evidence** (§7.4).
4. **`0x2C3FC` is not a stub — and its demo-reached calls are RNG- and
   fight-state-neutral** (arena-backdrop §0.3.9, extended this cycle). The raw
   is 1268 B / 206 callers — the voice dispatcher, which the port carries as an
   out-of-scope no-op. The brief's Step-3 question is answered in **§3.4**: the
   dispatcher consumes **no game RNG** (the LCG state `0xEF6D8` is read/written
   only by `0x5D7DC` and the seeder `0x20C10`, and no member of `0x2C3FC`'s
   190-function closure calls `0x5D7DC`) and writes **no fight-visible state**
   (its writes land in the voice-state block `0x102860..0x1028DB`, whose only
   xrefs are the `0x1Cxxx` voice family). The method's `0x5Dxxx`-is-the-RNG
   exclusion mislabels the audio helpers (`0x5DD03` → `0x67FA0`, a field
   getter; `0x5D86A` → `0x6616A`, a cleanup; `0x5DC0F`/`0x5DC8B`/`0x5DEAF`/
   `0x5DEED` → the AIL sound driver). It enters the closure through the
   `0x36870 → 0x37D18 → 0x2C3FC` path (made live by Task 2's registration) and
   through `0x39CC8` case 3's tail (the design's family, not reached by the
   demo). The method keeps it excluded; §4 reports the sensitivity.
5. **The `0x36870` animation-opcode target is ported but never registered.**
   The roar stream reaches it (§2.5); the port's `anim_indirect`
   (`actors.c:565`) resolves through `fn_resolve` and skips an unregistered
   target. The port's `fighter_36870` (`fighter.c:2370`) is a complete port
   with tests (`check_winner_body`), but `fn_register(0x36870u, ...)` is absent
   (`actors.c:122-124` registers only `0x10FA8`, `0x12720`, `0x37A58`).
6. **The `0x39A34` animation-opcode target is unported.** It is the roar
   stream's *first* opcode (`0xD100`, op 0x11) and scales the record's frame
   hold `rec+0x24`; the port skips it (§2.4).
7. **The demo's live-RAM trace was attempted, not obtained** (§1.6, the
   arena-backdrop §1.7 precedent).

### 0.4 Corrections from Task 2's measurements (commits `6e78b31`/`e6451ed`)

Task 2 ported `0x3A43C`, `0x39A34`, the two registrations and the `glob_b`
store, and measured. Where its measurements or the raw contradict this record,
**the measurement and the raw win**; each correction is also marked in place.
Source: `.superpowers/sdd/2026-09-24-demo-pose/task-2-report.md` §2.5, §3, §4.

1. **§1.2/§5.4/§6.2 — "the fix extends the front-end tail" is false.**
   Measured on the stashed (unmodified) tree: port 488/489 **already**
   byte-matched captures 841/842 (0 B each) before any Task 2 change, and the
   front-end oracle's numbers are identical before and after
   (`[560..842]` / 283 / `125 clean, 154 splice, 0 transition,
   2 unexplained (832, 833)`). The §1.2 table's "(splice)" cells for the
   unmodified port were not measured as direct pairs; they are 0 B. No
   enforced claim moved, so no halt-and-report was needed.
2. **§7.1 — the phase-0 seed `+0x58 = 0x7F` cannot fire phase 0.** The raw's
   dispatch returns for any `+0x58 > 1` (`0x3A453 JBE` / `0x3A455 RET`). The
   test seeds `+0x58 = 0` (distinct from the post-condition 1).
3. **§7.3 — the mutation "flip `> 3` to `>= 3` with `+0x90 = 2`" cannot fail**:
   `(u8)(2 - 1) >= 3` is false under both forms. The distinguishing seed is
   `+0x90 = 4` (`(u8)(4 - 1) = 3`: `> 3` false, `>= 3` true); the test asserts
   the `+0x90 = 4` arm does not snap. Also, the §7.3 "open" seed's
   `slot+0x42` bit 3 makes `hit_record_x` return the record's own `+0x18`, so
   deleting `hit_anchor_x` could not move the latch assertion; the test clears
   bit 3 and seeds `DS_00100AB0 = 0x1000`, `DS_001077A8[0] = 0` (`0x1854F JZ
   0x18620`, `0x18540`'s early-out) and `DS_00100AF0[0] = slot+0x20`
   (`0x18759 JZ 0x18764`, skipping `0x18350`) so the raw and the port both
   compute `0x4321 - 0x1000`.
4. **§7.4 — the seed `slot+0x52 = 0x07` fires the `0x468D8` predicate**
   (`0x4691E CMP byte [EAX*4+0x107802], 7`), so `0x36D98` writes
   `+0x52 = 9` (`0x36E14`) before the setter reads it. The raw's at-call `BX`
   is **9**, not 7; the test asserts `DSW(0x107D10) == 0x0009`. The second
   call's `BX` is `0x10` (the first setter's `0x3A522` write), so
   `0x107D04 == 0x10`. The demo's own run measures the same: the setter runs
   exactly once, `f=72 side=0 cb=3a43c bx=9`.
5. **§2.6 — the owner hypothesis for the 43 px is falsified by measurement.**
   With all three pieces ported, the 843/490 compare is **22 919 B / 7 879 px**,
   exactly the §1.2 temporary-port value. The two named suspects act **after**
   the divergence: `0x39A34` runs once and its effect (one sprite per frame,
   `0x9077..0x9080`) starts at `f≈80`; `0x36870` runs from `f≈90`; the
   `glob_b` correction is inert here (the handler runs once with
   `B[1] = 0`, so the snap never fires). The divergence frame is `f = 73`
   (port 490). The 43 px is re-derived in **§9**.
6. **§6.2 first bullet — re-owned** to the T-rex's node-x motion at the pose
   entry (§9), not the two animation targets.

---

## 1. The runtime differential (Step 1)

### 1.1 The trace (measured)

`PR_POSE_DUMP` over the whole run; the state-7 window is `f = 65..967`
(`DS_0010150C`, the arena-reset tick). Side 0 is the T-rex (its stream head is
`0xE6DD2`, cursor `0xE7114`, char `+0x7A = 0`); side 1 the raptor (cursor
`0xD2316`, char 3). Key lines (verbatim, fields trimmed):

```
f=70 side=0 s52=09 s53=08 s54=00 s58=00 s10=00000000 r52=00 cur=000e7114 pid=9021
f=71 side=0 s52=09 s53=08 s54=00 s58=00 s10=00000000 r52=00 cur=000e7114 pid=9021
f=72 side=0 s52=10 s53=0a s54=00 s58=00 s10=0003a43c r52=01 cur=000e7114 pid=9022
f=73 side=0 s52=10 s53=0a s54=00 s58=01 s10=0003a43c r52=01 cur=000e7114 pid=9022
f=74 side=0 s52=10 s53=0a s54=00 s58=02 s10=0003a43c r52=00 cur=000e7332 pid=9075
f=75 side=0 ... s58=02 ... r52=00 cur=000e7332 pid=9075
f=76 side=0 ... s58=02 ... r52=00 cur=000e7332 pid=9075
f=77 side=0 ... s58=02 ... r52=00 cur=000e7334 pid=9076
f=80 side=0 ... cur=000e733e pid=9077
f=83 side=0 ... cur=000e7340 pid=9078
f=89 side=0 ... cur=000e7344 pid=907a
```

* The **setter runs in `fighter_pass_a`** (`fight_arena_frame` order:
  `fighter_pass_a` at `fight.c:943` precedes `fight_hud_pass` at `fight.c:959`),
  so `f=72`'s line (sampled at the head of `fighter_state_3531c`) still shows
  `s58=00`; the handler's phase 0 then runs in the same frame, and phase 1 at
  `f=73`. The sprite changes at `f=74` (`cur=0xE7332`, the roar stream's first
  word `0x1075`).
* `AF8`/`AFC`: `AFC=5,3,2` at `f=72,73,74` (the camera's `B54` chain, pose/freeze
  Task 6), `AF8=0` throughout — so `fighter_winner_body(1)` (the 0x193B0 winner
  body) runs for those three frames and the pose chain is entered through the
  ported `0x3B714 → 0x3AAFC` path. The trace of the handler's phase-1 frame:
  `POSE_3A43C f=73 side=0 other=1 b=0000 a=0000 s90=00 s2c=ef9a r18=ffffe71a`
  — `b = B[1] = 0`, so `hit_anchor_x` is not called (§2.2's gate).
* **`+0x52` never changes again** for either side through `f=967` (the state-7
  end): the handler is unregistered, `fn_resolve(0x3A43C)` is NULL, so
  `+0x58` stays 2 and `fighter_39efc` (`+0x58 == 4`) never opens the pose exit.
  The suite's `s7_last_change = 1078` (loop index) re-verified on the
  unmodified port.

### 1.2 The temporary port of `0x3A43C` (measured, reverted)

The raw's handler body (§2.2) was implemented temporarily and called from
`fighter_state_3531c` case 10 with the raw's arguments. Results:

| frame pair | unmodified port | with `0x3A43C` |
|---|---|---|
| port 486 ↔ capture 838 | 0 B / 0 px | **0 B / 0 px** |
| port 487 ↔ capture 839 | 0 B / 0 px | **0 B / 0 px** |
| port 488 ↔ capture 841 | (splice) | **0 B / 0 px** |
| port 489 ↔ capture 842 | (splice) | **0 B / 0 px** |
| capture 840 | — | splice of port 487/488 |
| port 490 ↔ capture 843 | 16 101 B / 5 793 px (crouch) | 22 919 B / 7 879 px (upright, x off) |
| port 491 | — | 11 601 px vs its best (whole mid-band) |
| port 492 | — | 23 697 px, full frame |

Precise matches (full-frame pixel diff, the first-difference table):

```
p486 ↔ cap838 0      p487 ↔ cap839 0      p488 ↔ cap841 0      p489 ↔ cap842 0
p490 ↔ cap843 7879 (bbox x 0..145, y 81..191 = the T-rex alone)
p491 ↔ best   11601 (bbox x 0..319, y 76..192 = the scene band)
p492 ↔ best   23697 (full frame)
```

So the fix **extends the byte-exact front-end tail by two frames** (captures
840–842 become clean/splice-explained; the design's risk §"the fix may move the
front-end claim's tail" is realized) and moves the first divergence to capture
843, where the pose is now correct but the T-rex's **screen x** is wrong.

**Correction (§0.4.1, Task 2 measured):** the tail claim is false — the
unmodified port's 488/489 already byte-match captures 841/842 (0 B), and the
front-end oracle is identical before and after. The fix changes the pose at
843 only; it does not move the front-end claim.

### 1.3 The 843 divergence is the T-rex's screen x (measured)

The sprite at the divergence was identified by rendering the candidate sprites
from `S16REX.GRA` with the port's palette (the DAC the trace dumped at `f=76`,
bank = `sprite_bank_offset(142) = 141`, i.e. `DAC[141+index]`) and the hflip bit
the pset id carries:

* **port frame 490** draws sprite **`0x1075`** (the roar stream's first id) —
  the sprite matches at a uniform position with a **0.88** pixel fraction (the
  residual is the separate blood effect).
* **capture 843** draws the *same* sprite `0x1075`: a per-row search finds a
  **uniform `+43 px` x-shift with a 0.86–1.00 fraction on every row** (no shear
  profile: every row's best shift is +43).
* So the sprite, the animation frame, the world x (the camera/background
  matches byte-for-byte outside the T-rex's box) and the raptor all agree; the
  T-rex's **node x** differs by 43 px. 43 px ≈ one frame of the T-rex's world
  motion (`slot+0x2C` falls ≈ `0xB1E` = 44.5 px per frame at the pose entry).
* From port 491 the divergence is scene-wide (the camera follows the fighters'
  midpoint, so a 43 px fighter offset drags the whole band): the *root* is the
  T-rex's position at 490, not a separate scene bug.

### 1.4 The scoping facts re-verified (measured)

* `s7_last_change` (loop index) = **1078** — the suite's `test_frontend`
  printf, re-run on the unmodified port.
* The state-7 window is loops 1070..1969 (`s7_last = 1969`); `DS_0010150C`
  restarts at the arena reset and runs 65..967 over it.
* The 843/490 compare: **16 101 B / 5 793 px** (the design's value).
* The capture's fight window `[843..1884]` and the port's demo frames
  `[490..1380]` (891) — the design's ratio 1042/891 = 1.169.
* The capture's T-rex rises at 843: capture 842 is still the crouch (its T-rex
  yellow mask is 156 px, y 120..192 — the port's 489 matches it byte-exactly);
  capture 843 is upright (910 px, y 81..190).

### 1.5 Which of the design's roots the demo actually needs (measured + raw)

The trace shows the demo's T-rex pose is `0x3A43C` only; `0x39F40` (and so
`0x39CC8`, `0x35050`, `0x39B30`, `0x39AC8`, `0x39B14`, `0x3C190`) are **never
reached** in the state-7 window. `0x14590` (the pose-state predicate with 6
callers in `0x14xxx`) is likewise not reached by the pose entry: its callers
are the camera path (`0x14xxx`), and the differential's camera matches through
490. All of them stay out of scope, named with their owner (§6).

### 1.6 The original's live RAM — attempted, not obtained

`dosbox-x` (2026.08.31) was driven under a Python `pty` with `-break-start`,
`BPLM 2EBC08` (the crowd-table store the arena-backdrop record used) and
`MEMDUMPBIN`. The emulator boots (`COMMAND.COM`'s PSP logged, the mount and
`PRAGE.EXE` command accepted) but the ncurses debugger's prompt/dump was not
confirmed within the session budget: no `pr_dos_dump.bin` was written and the
pty log carries only the emulator's stdout. **Consequence (the arena-backdrop
§1.7 fallback):** the original's state is derived from the raw (Ghidra) plus
the capture; the *decisive* evidence is the capture itself — the original's own
recorded output — which the port reproduces byte-exactly for captures 838–842
once `0x3A43C` runs. Marked measured/derived:

| claim | source |
|---|---|
| the port enters `0x3A43C`, `+0x54=0`, `fighter_pose_start` never runs | **measured** (port trace) |
| captures 838–842 byte-match port 486–489 with `0x3A43C` | **measured** (port) |
| capture 843 = sprite `0x1075` at x −13; port 490 = `0x1075` at x −56 | **measured** (capture + port render match) |
| the handler's body, gate and callees | **derived** (raw `0x3A43C`) |
| the roar stream's opcodes and their targets | **derived** (raw `0xE7332` + the fixup rule) |
| the original's own live state at capture 843 | **not obtained** (dosbox-x attempt) |

---

## 2. The missing pieces' owner chain (Step 2)

### 2.1 The chain

```
game_state_7 (0x11BCC's fight)                     port: flow.c game_state_step case 7
  fight_arena_frame (0x263F4)                      port: fight.c:928
    fighter_pass_a (0x1958C)                       port: fighter.c:312
      fighter_winner_body(1) (0x193B0)             port: fighter.c:4402
        fighter_reaction (0x3B714)                 port: fighter.c:4236
          fighter_reaction_apply (0x3AAFC)         port: fighter.c:3955
            fighter_pose_3a504 (0x3A504)           port: fighter.c:3595
              +0x52=0x10 +0x53=0x0A +0x54=0
              +0x10 = 0x3A43C   ← THE DIVERGENCE (unported)
    fight_hud_pass (0x35658)                       port: fight.c:556
      fighter_state_3531c (0x3531C) case 10        port: fighter.c:2803
        fn_resolve(slot+0x10) -> NULL today        the handler never runs
```

The setter is reached because the camera's unfreeze chain writes `AFC = 5/3/2`
at `f=72/73/74` (pose/freeze Task 6), `fighter_pass_a`'s tail runs
`fighter_winner_body(1)`, and the pose lands on side 0 (`0x3AAFC` swaps the
sides: `side = DSB(DSD(slot)+0x51)`, so the winner body's `other` gets the
pose). The port's chain to the setter is measured faithful (the pose state
lands at `f=72`, matching the design's loop-1078 scoping).

### 2.2 `0x3A43C` — the pose handler (the missing root)

**Extent:** `0x3A43C..0x3A501` (the last `RET` at `0x3A500`; `0x3A501..0x3A503`
is the alignment `LEA EAX,[EAX]`) — **197 B**. No Ghidra function (reachable
only through `CALL [ECX+0x10]`), so no CSV row.

**Call convention (raw, proved):** `fighter_state_3531c` case 10,
`0x354DA..0x354E2`: `CMP dword [ECX+0x10],0` / `JZ` / `MOV EAX,ECX` /
`CALL dword [ECX+0x10]` — **EAX = slot, EBX = side** (EBX = the function's
`MOV EBX,EAX` at `0x35325`). The handler starts `MOV EDX,EBX; MOV EAX,ESP;
CALL 0x33A10` — `0x33A10` is `fighter_ctx_swap` (EDX = side), so the handler's
only input is the side; EAX (the slot) is overwritten.

**Body (Ghidra, fixup-applied), switch on `slot+0x58` (no table; a two-arm
compare `CMP AL,1 / JC / JBE / RET` at `0x3A44F..0x3A458`):**

| phase | raw | body |
|---|---|---|
| 0 | `0x3A461` | `slot+0x58 = 1`; return |
| 1 | `0x3A46D` | `actors_anim_begin(rec_self, DSD(0x000C8FE0 + char*4), 0x40400000)` (`0x3A476`/`0x3A47E`/`0x3A489`; `char = slot+0x7A`); `hit_anchor_set(other, DSD(rec_other+0x18), 0)` (`0x3A498`/`0x3A49B`); `slot+0x58 = 2` (`0x3A4A4`); `b = (s16)DSW(0x107D10 + other*2)` (`0x3A4AC`/`0x3A4BF`), `a = (s16)DSW(0x107D14 + other*2)` (`0x3A4B3`/`0x3A4BC`); if `b != 0 && b != 5` (`0x3A4C2`/`0x3A4C6`) and `(u8)(slot+0x90 - 1) > 3` (`0x3A4CF`..`0x3A4D9`; the 4-entry table at `0x3A42C` = `[0x3A4F2 ×4]`, bytes `f2a40300` ×4, is the no-op arm for 1..4) then `hit_anchor_x(side, a)` (`0x3A4E8`/`0x3A4ED`); `slot+0x90 = 1` (`0x3A4F6`) |
| >1 | `0x3A455` | return |

**Callees (all ported):** `0x33A10` `fighter_ctx_swap` (`fighter.c:40`),
`0x2BC30` `actors_anim_begin` (`actors.c:919`), `0x188AC` `hit_anchor_set`
(`fighter.c:3233`), `0x188DC` `hit_anchor_x` (`fighter.c:3268`). **No new
callee.** The only data it needs: `DS_000C8FE0[char]` (the roar stream table,
§2.5), `DS_00107D10`/`DS_00107D14` (the setter's globs), and the existing
`slot+0x7A/0x58/0x90/0x2C`.

### 2.3 The setter family (`0x3A504` and its three siblings)

All four setters share the shape (`PUSH ECX; SUB ESP,0x18; MOV ECX,EDX;
MOV EDX,EAX; MOV EAX,ESP; CALL 0x33A10` then the stores), taking EAX = side,
EDX = the reaction animation's byte 3 (`(s32)(s8)DSB(anim3[0]+3)`,
`0x3AD23..0x3AD2E`), and EBX = the caller's BX:

| setter | handler | stream table | glob_a (`= slot+0x2C`) | glob_b (`= BX = slot+0x52`) | proof |
|---|---|---|---|---|---|
| `0x3A504` | `0x3A43C` | `0xC8FE0` | `0x107D14` | `0x107D10` | `0x3A53A`/`0x3A563`/`0x3A56B` |
| `0x3A650` | `0x3A588` | `0xC9030` | `0x107D0C` | `0x107D00` | `0x3A686`, port `fighter.c:3597` |
| `0x3A79C` | `0x3A6D4` | (see `0x3A6D4`) | `0x107D08` | `0x107D04` | `0x3A7D2`, port `fighter.c:3603` |
| `0x3A8E8` | `0x3A820` | — | `0x107CF8` | `0x107CFC` | port `fighter.c:3609` |

The **handlers are siblings** with the same body shape: `0x3A588`
(`0x3A588..0x3A64C`) differs from `0x3A43C` only in the stream table
(`0xC9030`), the globs (`0x107D0A`/`0x107CFE` = B `0x107D00`, A `0x107D0C`) and
the final `slot+0x90 = 2` (`0x3A642`, vs 1). `0x3A6D4` (`0x3A6D4..0x3A795`) is
the same shape. The demo's first pose is the `0x3A504` arm; whether the fight
window later reaches the others is a Task-2 measurement (the port's trace
cannot tell while the pose is frozen).

### 2.4 `0x39A34` — the roar stream's frame-hold scaler (the second new root)

**Extent:** `0x39A34..0x39A64` — **48 B**. It is *not* a Ghidra function (its
neighbour `0x39A10`'s `RET` at `0x39A31` + the 2-byte alignment `MOV EAX,EAX`
at `0x39A32`); it is reached only through the animation dispatcher's indirect
`CALL EAX` (`0x2B2A0`'s opcodes 0x10/0x11/0x15, port `anim_indirect`,
`actors.c:565`).

**Body (raw):** `EAX = [EAX+0x14]` (`0x39A3A`); if 0 return (`0x39A3F`);
`rec+0x24 = (float)(s8)[that+0x7E] / (float)(u16)edx` (`0x39A41`..`0x39A5B`,
`FILD`/`FDIVP`/`FSTP`). I.e. it rescales the record's animation frame hold
`rec+0x24` from the linked record's `+0x7E` and the opcode's operand. The
roar stream's first opcode is exactly this call (§2.5), so the port's
`3.0f` hold is not the original's once this runs — a measured divergence
candidate for the T-rex's phase/position after 843.

### 2.5 The roar stream `0xE7332` and the animation targets the port skips

`DS_000C8FE0[char]` (the handler's stream table, 16 dwords; char 0 =
`0xE7332`, raw-file `0x67332` + `0x80000`): the words are the sprite ids
`0x1075..0x1080` (12) and `0x11D1..0x11D6`, plus the opcodes:

| word | op (mode) | inline dword | runtime target | port |
|---|---|---|---|---|
| `0xD100` | 0x11 (0x4000) | `0x00029A34` | **`0x39A34`** | unregistered → skipped |
| `0xD500` | 0x15 (0x4000) | `0x00026870` | **`0x36870`** | ported (`fighter.c:2370`) but unregistered → skipped |

The same rule applies to the *idle* stream `0xE6DD2` (`0xD000` → `0x37A58`
registered; `0xD500` → `0x36870` unregistered), which is why the port's crouch
is byte-exact anyway (those calls' effects are nil in that window) — the
**registration gap is real and measured**, not a new discovery of the crouch.

### 2.6 The next divergence behind the pose (measured)

With `0x3A43C` ported: captures 838–842 byte-match; capture 843 draws the
correct sprite `0x1075` at a **43 px** wrong screen x (§1.3); from port 491 the
camera drags the whole band. The owner is the **T-rex's per-frame position at
the pose entry**, and the two measured candidates are the animation targets the
port skips (`0x39A34`'s hold rescale and `0x36870`'s `+0x54` machine) — the
hold rescale is the first opcode of the roar stream, so Task 2 must port it and
re-measure before concluding. If the x still differs, the next suspects (with
their evidence) are the pose setter's `glob_b` bug (§0.3.3) and the
`0x36870` registration; the camera/scene path is **already ported** (the
arena-backdrop cycle) and matches through 490, so it is not the cause.

**Falsified (§0.4.5, Task 2 measured).** With `0x39A34`, both registrations and
the `glob_b` fix ported, 843/490 is still 22 919 B / 7 879 px: `0x39A34` acts
from `f≈80`, `0x36870` from `f≈90`, and the `glob_b` store is inert (the snap's
`B[1] = 0` gate stays closed), all after the divergence at `f = 73`. The 43 px
is re-derived in §9.

**The `0x36870` registration's own live call (verified, §3.4).** The ported
`fighter_36870`'s case-0 arm (`fighter.c`'s `0x36A1A`) calls `fighter_37d18`
when `slot+0x42` bit 0x20 holds, and `0x37D18` (`0x37D5C`) is a `0x2C3FC`
caller: so registering `0x36870` makes the voice path live. Its RNG- and
fight-state-neutrality is proved in §3.4, so the registration does not grow the
closure.

---

## 3. The porting plan (Step 3)

**Home:** `port/src/game/fighter.c` (the pose family) and
`port/src/game/actors.c` (the animation-code registration). No new file, no new
dependency, no `render.c`/`camera.c` change.

### 3.1 Piece 1 — the pose handler `0x3A43C` (new)

One C function per original, `/* 0x3A43C — demo-pose record §2.2 */`:

```c
/* 0x3A43C. The 0x3A504 pose family's per-frame body: phase 0 -> +0x58=1;
 * phase 1 -> start the char's roar stream, re-anchor the other record, then
 * (when B[other] is not 0/5 and the +0x90 gate opens) snap the self x to
 * A[other]; +0x90=1. EAX = slot, EBX = side (0x354E0). */
static void fighter_pose_3a43c(u32 slot, u32 side) { ... }
```

* Wired at `fighter_state_3531c` case 10 (`fighter.c:2803`): replace the
  `void (*fn)(void)` call with a typed resolve+call carrying the raw's
  arguments. The raw passes EAX=slot, EBX=side; only the side is read. Use
  `void (*fn)(u32, u32) = (void (*)(u32, u32))(void *)fn_resolve(...)` and
  `fn(slot, side)` — the same typed-cast pattern `fighter_winner_body` already
  uses (`fighter.c:4433`).
* Registered at the one-time init site: `fn_register(0x3A43Cu, ...)` in
  `actors_init` (`actors.c:122`, next to the three animation codes) — the
  `0x3531C` case-10 resolve then finds it. (Direct-call wiring is also
  acceptable per the record, but the registration is the repo's pattern for
  data-held code addresses.)
* The **three sibling handlers** (`0x3A588`, `0x3A6D4`, `0x3A820`) are the same
  shape; port them in the same commit **only if** Task 2's re-measurement shows
  the window reaching them (they are the same arm family, ~200 B each). The
  minimal fix is `0x3A43C` alone.

### 3.2 Piece 2 — the frame-hold scaler `0x39A34` (new) + the registrations

```c
/* 0x39A34. Scale the record's animation frame hold rec+0x24 by the linked
 * record's +0x7E over the opcode operand (raw 0x39A34, the roar stream's
 * first opcode). EAX = rec, EDX = operand. */
static void anim_code_39A34(u32 rec, u32 arg) { ... }
```

* Registered as an animation code: `fn_register(0x39A34u, ...)` in
  `actors_init` (the `anim_indirect` path, `actors.c:565`).
* `fn_register(0x36870u, (void (*)(void))fighter_36870)` in the same place —
  the port already has the function and its tests; only the registration is
  missing. **Careful with the signature:** the animation dispatcher calls
  `(rec, arg)`; `fighter_36870(rec)` reads only `rec` (`0x36870`'s body uses
  `EAX = rec`; the arg is unused), so the existing signature is faithful.

### 3.3 Piece 3 — the setter's `glob_b` correction (§0.3.3)

`fighter_pose_commit` (`fighter.c:3586`): `DSW(glob_b + side*2u) =
(u16)(edx >> 16)` → `DSW(glob_b + side*2u) = (u16)DSB(ctx[3] + 0x52u)`. Mark it
`/* PORT: raw 0x3A56B stores BX; EBX = slot+0x52 at every 0x3A5xx call site
(0x3AD2A/0x3AD31/0x3AD3D) */`. Update `check_pose_entry`'s `0x107D10`
assertion with the raw evidence and add a distinguishing seed (§7.4).

### 3.4 The `0x2C3FC` voice calls — the RNG/fight-visible verdict

The brief's Step 3 asks whether the original's dispatcher consumes RNG or
writes fight-visible state. **Verdict: neither, for every path the demo can
reach; the stub stays and the closure does not grow.**

* **The reached codes (raw).** `0x37D18`'s call (the path Task 2 makes live by
  registering `0x36870`) passes `word[0xBDAD4 + char*2]` — char 0 (the T-rex)
  = **0x92**, char 3 (the raptor) = **0xA1**; `0x35E6C`'s = **0x6D**;
  `0x36710`'s = **0x6E**; `0x39040`'s `0x3923D` = **0xDA**/**0xDB** (the
  `rng_next(2)` draw at `0x39228` picks one); `0x3531C`'s = **0xEC**/**0xE0**
  (the inert `DS_001078F6` countdown); `0x3D784`'s = **0x4F**; the animation
  opcode `0x2E` (`0x2B8D2`: `AND EAX,0xFFFF; CALL 0x2C3FC`) = the stream's
  operand; `0x39CC8` case 3's = **0x6C** (the design's family, not reached by
  the demo's handler). Every one of these is **type 2** in the dispatcher's
  table `DS_000BBDC8 + code*12` (0x92/0xA1/0x6C/0x6D/0x6E/0xDA/0xDB) except
  **0xEC** (type 1) and **0xE0**/**0x4F** (type 5, and 0x4F's arm calls
  nothing).
* **The type-2 arm's callees** (the dispatcher's `case 2`, `0x2C3FC`'s
  decompilation): `0x1CE70` scans the voice table `0x10286C` and calls
  `0x5DD03`; on a miss it calls `0x1CC28`, which allocates a voice entry in
  `0x102860..0x102874` and calls `0x5DD03`/`0x5DC0F`/`0x5DC8B`/`0x1B544`
  (the resource resolve, ported). The type-1 arm (`0x1CA14`) calls nothing.
* **The decisive RNG check.** The game's LCG state `0xEF6D8` has exactly three
  xrefs: `0x20C62` (WRITE, the seeder `FUN_00020C10`), `0x5D7E5` (READ) and
  `0x5D7F6` (WRITE) — both in `0x5D7DC`, the game RNG. **No member of
  `0x2C3FC`'s 190-function closure calls `0x5D7DC`** (the direct-caller set of
  `0x5D7DC` intersected with the closure is empty). The 21 `0x5Dxxx` functions
  the method's exclusion range treats as "the RNG" are the AIL sound driver's
  helpers: `0x5DD03` → `0x67FA0` (a field getter: `return p ? p[4] : 0`),
  `0x5D86A` → `0x6616A` (a cleanup loop), `0x5DEAF` → `0x6A8A0`,
  `0x5DC0F`/`0x5DC8B`/`0x5DEED` likewise runtime wrappers. They draw from the
  audio subsystem's own state, not `0xEF6D8`.
* **The decisive fight-state check.** The dispatcher's writes land in the
  voice-state block: the xrefs to `0x102860`, `0x1028C8` and `0x1028D4` are
  only the `0x1Cxxx` voice family (`0x1CA14`, `0x1CA6C`, `0x1CB18`, `0x1CC28`,
  `0x1CD9C`, `0x1CE04`, `0x1CE70`, `0x1CED4`, `0x1CF40`, `0x1D018`,
  `0x1D1B0`). No fighter slot (`0x1077A8`/`0x1077B0`), record, camera
  (`0x100AB0`), command word (`0x1088E0`), pset/render-list or actor/effect
  state is written. The only in-range write in the closure is `0x2D498`'s
  guarded store into `0x100CE3..0x1014DC` (the loader/voice buffers).
* **Consequence.** The port's stubs are RNG- and fight-neutral: the fight's
  byte-exact evolution (the LCG the oracles and `s7_entry_post` check) is
  unaffected. The divergence the stub does cause is the **audio** (the voice
  playback), already the audio sub-project's named gap (`fidelity-gaps`
  §7.13). The callers' post-call fight-visible writes are already modelled by
  the port (`0x37D18`'s `DS_001078DC = DS_000BD89C` at `fighter.c:2177`; the
  `0x391FE`/`0x39228` draws around `0x3923D` at `fighter.c:2580`).
  `0x2C3FC` stays pruned as one of the method's five stubs; its raw truth
  (1268 B, 206 callers) would add 190 f / 22 795 B only if the cycle wanted
  the voice behaviour (§4.3).

### 3.5 What must NOT change

* The ported chain up to the setter (`fighter_reaction_apply`, the four
  setters, `fighter_winner_body`, `0x193B0`), `fight_hud_pass`, the camera and
  the render/scene path.
* No sprite id, layer, palette or position of any *other* actor.

---

## 4. The size-gate verdicts (Step 4)

### 4.1 The method (cycle-3 §10.4, verbatim, with the arena-backdrop §4.1 note)

From `port/decomp/prage.calls.csv` (callee edges) + `prage.functions.csv`
(sizes). BFS from the roots, inclusive; the traversal stops at the excluded
nodes (the `0x6xxxx` runtime, the RNG `0x5Dxxx`, the five stubs `0x2C3FC`,
`0x2EA64`, `0x62002`/`0x62003`/`0x6201B`). **"new"** = in the closure AND not
excluded AND **not named anywhere in `port/src` except the generated
`symbols.h`** (the address appears as `0xADDR` in a `.c`/`.h`). **"true-new"** =
naive + the roots that are named-but-unported.

**Input correction (raw):** `0x3A43C` and `0x39A34` have **no CSV rows**
(they are reached only through indirect calls), so their extents are the
control-flow-reachable disassembly extents above (197 B, 48 B) and they enter
the figures as explicit roots. The CSV's callee edges for `0x36870` are
complete (11 rows).

### 4.2 Per group

| group | roots | closure | naive-new | true-new |
|---|---|---|---|---|
| **the demo's pose chain (this cycle's owner group)** | `0x3A43C` + `0x36870` | **121 f / 21 170 B** | **12 f / 1 960 B** | **13 f / 2 008 B** |
| the design's mis-attributed chain | `0x39CC8`, `0x35050`, `0x39B30`, `0x39AC8`, `0x39B14`, `0x3C190`, `0x14590` | 95 f / 14 585 B | 13 f / 1 911 B | 13 f / 1 911 B |

The 12 naive-new members of the owner group (sizes in bytes; `0x3A43C` enters
with its 197 B):

```
0x3A43C 197  0x1C528 190  0x1E30C 329  0x1E458 465  0x1E62C 169
0x2D498  27  0x2D4B4  17  0x2EA68  10  0x2EF24  34  0x2F314 113
0x38C5C 125  0x38ED0 284
```

The true-new set adds `0x39A34` (48 B, no CSV row) and subtracts nothing: the
11 non-`0x3A43C` naive members are the **resource read/gate family**
(`0x1Exxx`/`0x1C5xx`) the port *models* in `platform/res.c` and the
`0x2Dxxx`/`0x2Fxxx`/`0x38xxx` callees of `0x36870`/`0x2BC30` that the port's
own functions already call — the naive heuristic cannot see a ported function
that carries no raw address in its comments (the same limitation the
arena-backdrop record §4.1 documented). **Measured both ways:** the genuinely
new functions are **2 / 245 B** (`0x3A43C` 197 B + `0x39A34` 48 B), plus the
`fn_register` of the already-ported `0x36870`.

### 4.3 Verdicts

* **The owner group is under the gate by both measures** — naive 12 functions
  (< the ~20-function line) and 1 960 B (< the ~4 KB line); true-new 13
  functions / 2 008 B, of which only 2 / 245 B are genuinely new. **The gate
  does not trigger; the cycle proceeds to Task 2.**
* **The design's chain is also under** (13 f / 1 911 B) — so the design's
  scoping was not over the gate either; the correction is only *which* handler.
* **The sensitivity (raw correction to the method's inputs):** counting
  `0x2C3FC` (the voice dispatcher, not a stub, §0.3.4) would add its 190 f /
  22 795 B subtree — the arena-backdrop §4.3's measured figure. It stays
  excluded because the port keeps the voice subsystem as an out-of-scope stub
  **and** the path that reaches it (`0x36870 → 0x37D18 → 0x2C3FC`, made live by
  Task 2's registration) is RNG- and fight-state-neutral (§3.4) — porting it
  would add size without changing the fight's byte-exact evolution.
* **Not in this cycle but now visible:** the fight window's continuation past
  capture 843 (the T-rex's position, the camera drag) — measured here (§2.6)
  and owned by the same pose/anim chain; if Task 2's re-measurement shows the
  window needs the `0x3A58x`/`0x3A6D4`/`0x3A820` siblings or `0x36870`'s
  behaviour, the group is still **12+3 = 15 f** under the line.

---

## 5. The measurement plan (Step 5)

Tasks 2–4 establish the gate with:

1. **Invocation.** `make demo-oracle` builds and runs
   `PR_FRONTEND_DET=/tmp/pr_frontend_dump PR_GAME_DIR=data/game/C
   ./build/run_tests` (the driver writes `/tmp/pr_frontend_dump/run1/frame_NNNN.raw`)
   then `python3 tools/title_compare.py --demo --capture
   data/title-captures/frontend --port /tmp/pr_frontend_dump/run1`.
2. **The first-divergence direct compare** (design §5's witness):

```bash
python3 -c "a=open('/tmp/pr_frontend_dump/run1/frame_0490.raw','rb').read(); b=open('data/title-captures/frontend/frame_0843.raw','rb').read(); d=[i for i in range(len(a)) if a[i]!=b[i]]; print(len(d),'bytes',len({i//3 for i in d}),'px')"
```

   **Before:** 16 101 B / 5 793 px. **After (measured, temporary `0x3A43C`
   only):** 22 919 B / 7 879 px (the sprite is right; the x is 43 px off). With
   `0x39A34` + the `0x36870` registration the diff must fall; the target is
   **0** (or a splice explanation in the oracle). Report both.
3. **The per-frame classification** (`tools/title_compare.py`'s
   `check_capture`/`explain`): the fight window's bounds are derived from the
   front-end pass (`fe_res['window']`, `port_lo = max(covered)+1`) and the
   capture's all-black artifact run (1885). The measured window is
   `[843..1884]` (1042 content frames); the port's demo frames are
   `[490..1380]`.
4. **The enforced ladder:** `make verify`. **Expected move:** the front-end
   oracle's window tail. The temporary `0x3A43C` makes captures 840–842
   clean/splice-explained (the port's 488/489 byte-match 841/842), so the
   window extends and the port's 480/481 enter its coverage — the design's risk
   ("the fix may move the front-end claim's tail") is realized. Task 2 must
   halt-and-report and absorb the move in the same commit with this reason (the
   arena-backdrop §6.2 precedent: 832/833 allowed by name). Every other claim
   (title, attract, smk, C-vs-Python, `symbols.h`) must be unmoved.
   **Correction (§0.4.1):** no move — 488/489 already matched 841/842 on the
   unmodified port, and the front-end oracle is identical before and after.
5. **The instrumented differential** (Task 1's, reverted; not part of the
   shipped tree): the `PR_POSE_DUMP` blocks of §0.2 — the per-side line at
   `fighter_state_3531c`'s head, the `POSE_3A43C` gate line, and the temporary
   direct call. Task 2 should re-use it to watch `+0x58` reach 2 and the roar
   stream start.

---

## 6. The named gaps (Step 6)

### 6.1 Closed by this cycle's fix (expected, measured in Task 1)

* **The demo's state-7 T-rex pose freeze** — the missing handler `0x3A43C`.
  Measured: with it, captures 838–842 byte-match and capture 843 shows the
  correct upright roar sprite. The assertions are §7's; the README's gap
  inventory entry and the arena-backdrop §6.3 residual's owner chain are
  corrected to `0x3A43C` (not `0x39CC8`).

### 6.2 Re-scoped (measured, with owner)

* **The T-rex's screen x at capture 843** (43 px ≈ one frame of world motion) —
  owner: the pose entry's per-frame position path, the two skipped animation
  targets (`0x39A34`, `0x36870`) first (§2.6). Task 2 measures after porting.
  **Re-owned (§0.4.6):** Task 2 measured both targets acting after the
  divergence; the owner is the T-rex's node-x motion at the pose entry (the
  `0x3B080` velocity seed, the `0x2A4FC` integration) — see §9.
* **The camera/scene drag from port 491** — a *consequence* of the x offset
  (the camera midpoint follows), not a separate owner; re-measured in Task 2.
* **The front-end window's tail move** (captures 840–842 become clean) — the
  design's expected claim move; absorbed by Task 2 per §5.4.
  **Closed as not-a-move (§0.4.1):** measured, the tail did not move.
* **The `0x39F40`/`0x39CC8` pose family** (the death/fall poses and the
  `0x2C3FC(0x6C)` voice) — **not reached** by the demo's first fight (measured:
  `fighter_pose_start` never runs, the handler stays `0x3A43C` through
  `f=967`). Owner: the pose family; out of scope, named. If a later fight
  window reaches it, the closure is the design's chain (13 f / 1 911 B, §4.2)
  — still under the gate.
* **The `0x3A58x`/`0x3A6D4`/`0x3A820` sibling handlers** — same arm family,
  reached only if the window's later reactions select their `ecx&4`/`ecx&8`/
  `slot+0x54` arms; Task 2 measures.

### 6.3 Carried, untouched (each with its owner)

* The demo loop's other content — the Time Warner logo (capture 1886+), the
  title/logo screens, fight 2 — the `--demo` tail stays report-only.
* The state-9 hold's animation (`fidelity-gaps` §7.6); the loader's presented
  DAC state (§7.11, the front-end's 832/833); the attract-215 presented-DAC
  mechanism (demo-fight-closure §9.5); the interactive match (§7.12); the audio
  gaps (§7.13, the `0x2C3FC` voice calls stay stub calls — §3.4 proves they are
  RNG- and fight-state-neutral, so only the voice *playback* is silenced);
  `0x38154`.
* `0x14590` (the pose-state predicate, 6 callers in `0x14xxx`) — the camera
  path's; not reached by the pose entry (measured, §1.5). Owner: the camera
  path.

### 6.4 Named gaps of the derivation itself

* **The original's live RAM was not obtained** (§1.6). The dosbox-x debugger
  boots but the break/dump was not confirmed in the session budget; the
  original's state is derived from the raw + the capture, and the byte-exact
  capture match is the decisive measurement.
* **The 43 px mechanism is not yet pinned to one raw site** — the record names
  the two measured candidates (`0x39A34`'s hold rescale, `0x36870`'s `+0x54`
  machine) and the setter's `glob_b` bug; Task 2's re-measurement decides. This
  is a named gap with its evidence, not a fitted value. **Task 2 decided
  (§0.4.5):** none of the three; see §9.
* **The exact dump-index mapping** (`dump = f + 417` for this run) is derived
  from the pose-entry match; Task 2 should re-derive it from its own trace if it
  needs frame-level alignment.

---

## 7. Unit-test values (the substitutions for Task 2)

Home: `port/tests/test_fight.c` (the pose/reaction area; the design says no new
test file). Each assertion is raw-derived, seeded with a sentinel that differs
from the post-state, and mutation-proven.

### 7.1 The pose handler `0x3A43C` — phase 0 (the smallest failing unit)

* **Seed:** `slot+0x58 = 0x7F` (a sentinel that is neither 0 nor 1); the two
  slots and records reset as `pose_chain_setup` does.
* **Call:** the ported `fighter_pose_3a43c(slot, side)` (or through
  `fighter_state_3531c(side)` with `+0x53 = 0x0A`).
* **Assert:** `DSB(slot+0x58) == 1`. Mutation: delete the store → fails.
* **Correction (§0.4.2):** the seed `0x7F` returns at `0x3A455` without
  writing; the valid seed is `+0x58 = 0`.

### 7.2 The pose handler — phase 1 (the animation start)

* **Seed:** `slot+0x58 = 1`; `slot+0x7A = 0` (char 0); `slot+0x90 = 0`;
  `slot+0x2C = 0x1234`; `rec+0x08 = 0xDEADBEEF`; `rec+0x52 = 0x7F`;
  `rec+0x24 = 0xDEADBEEF`; the other slot's `+0x52 = 0x07`;
  `DSW(0x107D10 + other*2) = 0` and `DSW(0x107D14 + other*2) = 0` (the gate
  closed); the pset's id sentinel-seeded (e.g. `pset+0 = 0xFFFF`).
* **Assert:** `DSD(rec+0x08) == 0xE7332` (the char-0 stream; the raw-file
  `0x67332` + `0x80000`), `DSD(rec+0x20) == 0x40400000` and
  `DSD(rec+0x24) == 0x40400000` (3.0f), `DSB(rec+0x52) == 0`,
  `DSW(pset+0) == 0x1075` (the first id; `|0x8000` when the flip applies),
  `DSB(slot+0x58) == 2`, `DSB(slot+0x90) == 1`, and the other record's
  `+0x18`/`+0x1C` were rewritten (`hit_anchor_set`). Mutation: delete the
  `actors_anim_begin` call → `rec+0x08` stays `0xDEADBEEF`.

### 7.3 The handler's `hit_anchor_x` gate

* **Seed (open):** as §7.2 plus `DSB(slot+0x90) = 0`,
  `DSW(0x107D10 + other*2) = 3` (B[other] ≠ 0/5), `DSW(0x107D14 + other*2) =
  0x4321`, `slot+0x2C = 0x1234`, and `slot+0x42` bit 3 set (so `hit_record_x`
  returns the record's own `+0x18`).
* **Assert:** `DSW(slot+0x2C) == 0x4321` and `DSD(rec+0x18) == <the seeded
  rec+0x18>` (the latch's post-state). Mutation: flip the gate's `> 3` to
  `>= 3` with `slot+0x90 = 2` seeded → the snap must not happen.
* **Seed (closed):** `DSW(0x107D10 + other*2) = 5` → `slot+0x2C` unchanged.
* **Correction (§0.4.3):** the `+0x90 = 2` mutation cannot fail; use
  `+0x90 = 4`. Clear `slot+0x42` bit 3 and seed `DS_00100AB0 = 0x1000`,
  `DS_001077A8[0] = 0`, `DS_00100AF0[0] = slot+0x20`, so the latch is
  `0x4321 - 0x1000` under both the raw and the port.

### 7.4 The setter's `glob_b` correction (§0.3.3)

* **Seed:** `slot+0x52 = 0x07`, the reaction triple's byte 3 = `0xFFFFFFB0`
  (so `edx3 >> 16 == 0xFFFF`), `slot+0x2C = 0x2222`; call
  `fighter_reaction_apply(slot, 0)` with `ecx = 0` and `slot+0x54 = 0`.
* **Assert (raw):** `DSW(0x107D10) == 0x0007` (BX = slot+0x52) and
  `DSW(0x107D14) == 0x2222` (A = slot+0x2C). **The existing
  `check_pose_entry` assertion `DSW(0x107D10) == 0` (line 2957, "edx3 >> 16")
  is the port's wrong form and must be replaced by this seed + the raw
  evidence**; the mutation (restoring `edx >> 16`) then fails it.
* **Correction (§0.4.4):** `+0x52 = 7` fires `0x468D8` (`0x4691E`), so
  `0x36D98` writes `+0x52 = 9` (`0x36E14`) first: assert `0x0009`, and the
  second call's `0x107D04 == 0x10`.

### 7.5 `0x39A34` — the hold scaler

* **Seed:** `rec+0x14 = a scratch record`; `scratch+0x7E = 6`;
  `rec+0x24 = 0xDEADBEEF`; arg (`edx`) = 2.
* **Assert:** `DSD(rec+0x24) == 0x40400000` (6/2 = 3.0f). Mutation: delete the
  store → stays `0xDEADBEEF`.
* **Seed (the early-out):** `rec+0x14 = 0` → `rec+0x24` unchanged (the
  sentinel). This distinguishes "wrote 3.0f" from "never touched it".

### 7.6 The registration

* **Assert:** after `actors_init`/`fighter`'s init, `fn_resolve(0x39A34u) !=
  NULL` and `fn_resolve(0x36870u) != NULL`. Mutation: drop either
  `fn_register` → NULL.

### 7.7 The end-to-end observable

* The §5.2 direct compare must fall (the mutation proof of the whole chain: the
  unmodified port fails at 16 101 B / 5 793 px on 490↔843; the ported chain
  must reduce it, and the oracle's first unexplained must move past 843).
* `check_pose_entry`'s existing assertions (`0x3A43C`, the `+0x52/0x53/0x54`
  stores, the `0x3A79C` arm, the side-1 mirror) stay as the arm-selection
  regression.

---

## 8. Provenance (Step 8)

* **Ghidra** (project `rage`, `/PRAGE.EXE`, linear addresses):
  `decompile_function` at `0x3A504`, `0x39F40`, `0x2BC30`, and — the §3.4
  verdict — `0x2C3FC`, `0x1CA14`, `0x1CA6C`, `0x1CC28`, `0x1CE70`, `0x5D7DC`,
  `0x5DD03`, `0x5D86A`, `0x5DEAF`, `0x67FA0`, `0x6616A`, `0x500BB`,
  `0x1ADE4`, `0x2D498`, `0x2EA68`;
  `disassemble_bytes` at `0x3A43C` (197 B), `0x3A588`, `0x3A6D4`, `0x3A504`,
  `0x3A650`, `0x3A79C`, `0x39CC8` (and its `0x39CB4` table via `read_memory`),
  `0x39A10`/`0x39A34`, `0x3531C` (head + case 10 at `0x354DA`), `0x354B0`,
  `0x3AAFC`'s tail (`0x3AD00`..`0x3AD98`), `0x2BC30` (full), `0x2BCE0`'s
  `RET 4`, `0x37D40` (the `0x37D5C` voice call), `0x2B8D2` (the anim opcode
  `0x2E`), `0x39220` (the `0x3923D` codes 0xDA/0xDB);
  `get_function_by_address` at `0x188AC`/`0x188DC`/`0x2BC30`/`0x29A34`
  (inside `FUN_000299e8`);
  `get_xrefs_to` at `0xEF6D8` (the LCG state: only `0x5D7DC` + `0x20C10`),
  `0x5D7DC` (its direct callers), `0x102860`/`0x1028C8`/`0x1028D4` (the
  voice-state block: only the `0x1Cxxx` family).
* **The raw LE image** (Python, the port's page-map + fixup rule):
  the roar stream `0xE7332`'s words; `DS_000C8FE0`/`DS_000C9030`; the sprite
  table `DS_000A8B30`; the `S16REX.GRA` descriptors (`0x3A20F8` = id 0x1075,
  159×110, org (41,105)) and their RLE blobs.
* **The port's runs** (temporary instrumentation, reverted):
  `PR_FRONTEND_DET=/tmp/pr_frontend_dump PR_POSE_DUMP=1 ... ./build/run_tests`
  (the trace at `run1.log`, the frames at `run1/frame_NNNN.raw`); the
  per-frame diff tables (§1.2), the sprite-render matches (§1.3), and the
  unmodified-port re-verifications (§1.4).
* **dosbox-x** (2026.08.31): the pty attempt and its outcome (§1.6).
* **The size-gate script** (the method of §4.1) over
  `port/decomp/prage.functions.csv`/`prage.calls.csv` + the `port/src` name
  scan.
