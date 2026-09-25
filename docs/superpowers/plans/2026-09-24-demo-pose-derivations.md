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

---

## 9. The 43 px differential (Task 2b)

**Result in one line.** The 43 px is **not** the T-rex's velocity: it is a
transcription error in the ported pose handler `0x3A43C`. `0x2BC30` returns
with `RET 4` (`0x2BCEF`), popping the `0x3A471 PUSH 0x40400000`, so every ESP
offset from `0x3A48E` on names **ctx[1] (the side) and ctx[5] (rec_self)**; the
port read ctx[0]/ctx[4] (the other side). With the raw's indices the handler
re-anchors the T-rex (not the raptor) and reads `B[0] = 9` / `A[0] = -3968`
(the setter's own globs), so the `hit_anchor_x` snap fires and places the
T-rex 45 px right of the port's position. A second, adjacent gap — the
`0x35829 0x186C4` re-latch of both slots at `fight_hud_pass`'s tail — then
decides the camera: without it the camera reads the pre-snap `slot+0x34` and
steps −339 at `f = 73`. With both, **port 490 ↔ capture 843 and 491 ↔ 844 are
0 B**.

### 9.1 Units and the per-frame port trace (measured, baseline `713e5f8`)

Temporary `getenv("PR_2B")` trace (reverted): `fighter_3b080` after its
`0x3B0D3` store, `motion_step` entry for the two fighter records,
`camera_mode_track_pair` entry, `hit_record_x`, and a per-side line at the end
of `game_frame`'s `DS_00104B15` tail. `f = DS_0010150C`; `dump = f + 417`
re-derived (f=73's frame is dump 490, the first frame whose T-rex sprite is
`0x1075`; 489 = f=72 byte-matches capture 842).

`rec+0x18` is in 1/64 px; the pset x is `psx = rec+0x18 − cam + 0xA800`
(every trace line satisfies it), so the screen column is `psx >> 6` less the
sprite origin. `slot+0x2C = rec+0x18 + DS_00100AB0[side]` (`0x186D0`), where
`AB0` is the **current sprite's anchor offset** (`0x18350`), not motion.

| f | writer, in frame order | T-rex `rec+0x18` end | v (`rec+0x34`) end | `AB0` | `slot+0x2C`/`+0x34` end | cam `0xF0AF0` |
|---|---|---|---|---|---|---|
| 68–70 | — | −6144 | 0 | 0 | −6144 | 0 |
| 71 | tail latch (new crouch sprite) | −6144 | 0 | 2176 | −3968 | 0 |
| 72 | `fighter_pass_a` → `0x3B714` → `0x3B080` (v = −230, `+0x43 = 8`); setter `0x3A504` (`B[0]=9`, `A[0]=−3968`); `fight_hud_pass` → `0x35813` → `motion_step` (−230, drag → −222) | −6374 | −222 | 2176 | −4198 | 0 |
| 73 | handler phase 1 (roar `0x1075`); motion (−222 → −214); tail latch | −6596 | −214 | −448 | −7044 | 0 |
| 74 | motion; camera reads −7044 | −6810 | −206 | −448 | −7258 | −450 |
| 75..80 | motion, drag +8/frame | −7016 … −7926 | −198 … −158 | −448 | −7464 … −8374 | −685 … −1160 |

The velocity is **3.6 px/frame** (230/64), not 43. The record's §1.3 "43 px ≈
one frame of motion (`slot+0x2C` falls ≈ `0xB1E`)" conflated `slot+0x2C`'s
**anchor jump** at the sprite change (`AB0` 2176 → −448 = −2624 = 41 px) with
motion. **Correction recorded.**

### 9.2 The capture's T-rex x (measured)

Per-frame shift search (`shift.py`, scratch): the T-rex's colours (40 RGB
values present in the port's T-rex box and absent from the background band
and the right half) are matched at every `dx ∈ [−80, 80]` against the capture;
the background shift is the best `dx` of the top band (rows 0–59, x 40–279).

| port ↔ capture | T-rex `dx` (match / pixels) | background `dx` |
|---|---|---|
| 489 ↔ 842 | 0 (3737/3737) | 0 |
| 490 ↔ 843 | **+43** (3842/4368; ±1 px ≤ 1340) | 0 |
| 490 ↔ 844 | +40 (3865/4368) | 0 |
| 491 ↔ 844 | +36 (3841/4287) | 0 |
| 491 ↔ 845 | +32; 492 ↔ 845 +31 (3441/4472) | 0 / −1 |
| 493 ↔ 846 | +30 (3054/4676) | −2 |
| 494 ↔ 848 | +28 (3515/4724) | −2 |
| 496 ↔ 850 | +27 (3273/4788) | −2 |

The capture's T-rex moves left at ≈ 3.5 px per 60 Hz frame — the port's own
velocity — so the offset is a **constant displacement at f = 73**, not a
velocity error (the "gap shrinks" in the Task 2 report was the pairing and the
port's camera drag). The capture's background did not move at 843/844.

### 9.3 The candidates

* **(a) velocity applied one frame early — falsified.** Raw order:
  `0x2647D 0x1958C` (`fighter_pass_a` → `0x193B0` → `0x3B714`, the seed) runs
  before `0x2651B 0x35658` (`fight_hud_pass` → `0x35813 0x2A1FC`, the
  integration), exactly the port's order (`fight_arena_frame`). A one-frame
  shift of a 3.6 px/frame motion cannot make 43 px.
* **(b) magnitude or sign — falsified.** `0x3B7FA..0x3B81D`: `EBX =
  byte[anim[0]+2]` (`0x3B7FE`/`0x3B805`), `EDX = byte[anim[0]+3]`
  zero-extended (`0x3B80B`/`0x3B817`), `ECX = 1`, `EAX = [ESP+0x28]`; gated by
  `param_1+0x54 != 2` (`0x3B7F5`). `0x3B0A1 LEA EBX,[EAX*2]`,
  `0x3B0AC CALL 0x1A570` on `1 − side`, `0x3B0B5 NEG EBX`, `0x3B0D3 MOV
  [ECX+0x34],BX`, `0x3B0E1` `+0x43`; the mirror arm `0x3B0EF..0x3B129` (EDX =
  1 − side survives the calls). `0x2A4FC` re-checked instruction by
  instruction (`0x2A516..0x2A61A`): the integration, the `+0x42` accelerate
  arm and the `0x2A567` `+0x43` drag match `motion_step`. The trace's −230 =
  115 × 2 with the sign from the raptor's bit 15.
* **(c) the camera — falsified as the owner.** The port's camera is 0 through
  f = 73 and capture 843's background matches port 490 at `dx = 0`
  (record §1.3: byte-exact outside the T-rex box). The camera mode 1 split arm
  (`0x12ED7`, whose `0x18714` rec-x write the port skips, `camera.c` §7.5 gap)
  never fires in f = 68..80 (pair distance ≤ 14 096 < `word[0x9AF28]` =
  20 480), so that gap is inert here.
* **(d) the raw shows: the handler's stack offsets — PINNED.** Raw
  `0x3A43C` (capstone over `read_memory`, fixups applied):

  ```
  0x3a46d mov eax,[esp+0xc]        ; ctx[3] slot_self
  0x3a471 push 0x40400000          ; ESP -= 4
  0x3a47e mov edx,[eax*4+0xc8fe0]
  0x3a485 mov eax,[esp+0x18]       ; ctx[5] rec_self (0x14 + 4)
  0x3a489 call 0x2bc30             ; returns RET 4 (0x2BCEF): ESP += 4
  0x3a48e mov edx,[esp+0x14]       ; ctx[5] rec_self
  0x3a494 mov eax,[esp+4]          ; ctx[1] side
  0x3a498 mov edx,[edx+0x18]
  0x3a49b call 0x188ac             ; hit_anchor_set(side, rec_self+0x18, 0)
  0x3a4a8 mov edx,[esp+4]          ; ctx[1] side
  0x3a4ac mov ebx,[edx*2+0x107d12] ; >>16 = A[side] (0x107D14)
  0x3a4b3 mov edx,[edx*2+0x107d0e] ; >>16 = B[side] (0x107D10)
  0x3a4ea mov eax,[ecx+4]          ; ctx[1] side -> 0x188DC
  ```

  The stack must balance at `0x3A4FD ADD ESP,0x18`, which is only true if
  `0x2BC30` pops its argument — and it does (`0x2BCEF RET 0x4`). The Task 2
  port (and record §2.2) read `[esp+4]`/`[esp+0x14]` as if the push were
  still there: ctx[0]/ctx[4], the **other** side. Consequences in the demo:
  the handler re-anchored the raptor and read `B[1] = A[1] = 0` (the Task 2
  trace's `b=0 a=0`), so the snap never fired. With the raw's indices:
  `B[0] = 9` (the setter's `BX`, §0.4.4), `A[0] = −3968` (the setter's
  `slot+0x2C` word at f = 72), `+0x90 = 0` opens the table gate, and
  `hit_anchor_x(0, −3968)` writes `slot+0x2C = −3968` and `rec+0x18 =
  −3968 − AB0(−448) = −3520` (the traced `hit_record_x` return). After the
  f = 73 motion, `rec+0x18 = −3742` vs the port's −6596: **+2854 = +44.6 px**,
  i.e. +45 columns at `>> 6`.

  **The camera half: `0x35829 CALL 0x186C4`.** With the handler alone, port
  490 ↔ 843 is 23 166 B / 8 352 px: the T-rex sits **−5 px** from the capture
  and the camera stepped −339 at f = 73. Cause: `hit_anchor_set`'s own
  `0x186D0` latch wrote `slot+0x34 = rec+0x18 + AB0 = −6374 − 448 = −6822`
  before the snap; `hit_anchor_x` moves `+0x2C` but not `+0x34`; the tail's
  `camera_dispatch` (`0x25422`, before the `0x25438` latch) reads −6822, beyond
  the `0x1800` dead zone → `arg = −339`. The raw's `fight_hud_pass` tail is
  `0x35818..0x35829`: `0x354F0(side)`, `0x354F0(1 − side)`, **`0x186C4`**, and
  `0x186C4` is `XOR EAX,EAX; CALL 0x186D0; MOV EAX,1` falling into `0x186D0`
  — a re-latch of both slots after the `0x35813` sync. It gives
  `slot+0x34 = −3742 − 448 = −4190`, inside the dead zone: the camera stays 0,
  as the capture shows. (The port had named `0x186C4` a skipped gap.)

* **Also checked, inert here:** the `hit_record_x` (`0x18714`) omission of
  `0x18540`/`0x18350` — made raw-faithful temporarily, 490 ↔ 843 unchanged
  (22 919 B): at the f = 72/73 calls `DS_00100AF0[0] == slot+0x20` because the
  preceding `0x186D0` latch has just run them; it first differs at f = 90.
  `0x354F0` (the arena-wall clamp, `|slot+0x2C| > DS_000BE018 = 0x7C00`):
  `|slot+0x2C| ≤ 8 374` over f = 68..80, so inert in this step; it stays a
  named gap (`fight.c`).

### 9.4 The fix and its measurement

* `fighter_pose_3a43c`: `hit_anchor_set(ctx[1], DSD(ctx[5]+0x18), 0)`
  (`0x3A48E..0x3A49B`); `B`/`A` indexed by `ctx[1]` (`0x3A4A8..0x3A4BF`).
* `fighter_slot_latch_both` (`0x186C4`, 12 B, one new function) called at
  `fight_hud_pass`'s `0x35829`. Size gate: 1 function / 12 B — does not trip.

| measurement | before (`713e5f8`) | handler fix only | both |
|---|---|---|---|
| 490 ↔ 843 | 22 919 B / 7 879 px | 23 166 B / 8 352 px | **0 B** |
| 491 ↔ 844 | — | — | **0 B** |
| 492 ↔ 845 | — | — | 722 B / 254 px |
| demo oracle first unexplained | 843 (raw 3750) | — | **851 (raw 3758)** |
| front-end oracle window | `[560..842]` / 283 / 125 clean, 154 splice, 2 unexplained (832, 833) | — | **`[560..850]` / 291 / 130 clean, 157 splice, 0 transition, 2 unexplained (832, 833)** |

**The enforced front-end claim moves** (the window's tail extends by the eight
captures 843..850 the fix makes explained; the unexplained set is unchanged).
Per the Global Constraints this is a **halt-and-report**: old
`[560..842]`/283/`125 clean, 154 splice`, new `[560..850]`/291/`130 clean,
157 splice`, evidence §9.3(d). **Ruling and landing:** the controller ruled
to land the fix. It landed in **`dad2712`** (`game: fix the 0x3A43C stack
offsets and port the 0x186C4 re-latch`), and the enforced front-end claim moved
in that same commit: `[560..842]` / 283 / `125 clean, 154 splice, 0
transition, 2 unexplained (832, 833)` became `[560..850]` / 291 / `130 clean,
157 splice, 0 transition, 2 unexplained (832, 833)`. The reason is recorded in
the commit and in every live copy of the claim: the window comes from the
port's own dump, and the raw's `0x2BC30` `RET 4` (`0x2BCEF`) frame plus the
`0x35829` `0x186C4` re-latch explain captures 843..850. The unexplained set is
unchanged. On the landed tree, `make demo-oracle` gives
`first unexplained captured frame 851 (raw 3758)`.
Before the ruling, the fix was held as
`.superpowers/sdd/2026-09-24-demo-pose/task-2b-fix.patch` (git-ignored ledger;
now landed as `dad2712`). It contained the two code changes, the corrected §7.2/§7.3 test
seeds (B/A at `0x107D10`/`0x107D14`, i.e. `side = 0`; the self record's
`+0x1C`; "no snap" observed on `rec+0x18`, since `hit_anchor_set`'s relatch now
rewrites `slot+0x2C`), and `check_hud_latch`. With it: `cmake --build build`
0 warnings, `PR_ORACLE_REQUIRED=1 ./build/run_tests` all checks passed,
`make verify` exit 0 with title `54/55/2/0` and `54/57/0/0`, attract 215/215,
smk 120/120 + 41/41, C-vs-Python 9866, `symbols.h` byte-identical — only the
front-end window line moved.

### 9.5 The next residual (with the fix applied; characterised, not fixed)

The demo oracle's first unexplained captured frame becomes **851** (raw 3758).
Best port frame 497 (f = 80): **2 456 px**, bbox x 0..125, y 77..192 — the
T-rex alone; the background matches (`dx = 0`). The capture's T-rex at 851
matches port **496**'s sprite (f = 79) shifted −3 px (4 199/5 033), better than
port 497's own sprite at `dx = 0` (3 076/5 042): the capture still shows the
roar's previous frame, moved one frame of motion, while the port has already
advanced the roar animation (`0x9076 → 0x9077` at f = 80, §1.1). The port's
roar stream advances one frame early at f = 80. **Likely owner:** the roar
stream's frame-hold timing — `0x39A34`'s `rec+0x24` rescale (−121 / 10 =
−12.1 f, Task 2 report §3.4) consumed by `frame_timer` (`0x2AA70`), or the
`+0x20`/`+0x24` hold that `actors_anim_begin` seeds (3.0 f). Named for Task 3.

---

## 10. Outcome and the gap inventory (Task 4, measured on the landed tree)

Measured (`make demo-oracle`, `make demo-fight-oracle`, and the direct compare of
`/tmp/pr_frontend_dump/run1` frames against the capture): port 490 <-> capture 843,
491 <-> 844, 496 <-> 850 = **0 B**; port 497 <-> capture 851 = 6 544 B / 2 456 px.
Demo window `[851..3616]`, 2766 frames, 2760 unexplained, first unexplained **851
(raw 3758)**; fight window `[851..1884]` 1034 frames, 0 explained; ratchet N = 851.
This supersedes §6.1-§6.4's "expected"/"Task 2 measures" wording.

**Closed.**
* The T-rex's pose freeze (`0x3A43C`): assertions in `test_fight.c` (mutation-proven,
  Task 2 report §2), ladder green at `dad2712`.
* The 43 px x offset (§9): `0x3A43C`'s stack offsets + the `0x186C4` re-latch;
  490 <-> 843 = 0 B. The front-end window's tail move was real after all, by this fix:
  `[560..842]`/283 -> `[560..850]`/291 (§0.4.1's "not-a-move" applied to the handler
  alone).
* The camera/scene drag from port 491: a consequence of the x offset, gone with it.

**Re-scoped.**
* Capture 851 (the residual): the port's roar animation advances one frame early at
  f = 80 (§9.5); owner: the roar stream's frame-hold timing. Not fixed; next cycle.
* `hit_record_x/y` (`0x18714`/`0x18788`) omit `0x18540`/`0x18350` (inert until f = 90);
  the `0x354F0` arena-wall clamp; the camera split-arm `0x18714` write: named, inert
  in the window so far.
* The sibling pose handlers `0x3A588`/`0x3A6D4`/`0x3A820` and the `0x39F40`/`0x39CC8`
  family: not reached in the demo's first fight (measured); owner: the pose family.

**Carried, untouched** (owners as §6.3): the demo loop's other content (logo, title
screens, fight 2); the state-9 hold animation (fidelity-gaps §7.6); the presented-DAC
frames (§7.11, 832/833; demo-fight-closure §9.5); the interactive match (§7.12); the
audio gaps (§7.13, the `0x2C3FC` voice stub, RNG-neutral per §3.4); `0x38154`;
`0x14590` (camera path).


---

## 11. The roar timing at capture 851 (roar-timing Task 1)

**Result in one line.** The roar's early advance is **not** the frame timer. It
is a transcription error in the operand the pose setter `0x3A504` receives:
`0x3AAFC` loads it at `0x3AD23..0x3AD2E` as the **signed byte at the anim3
row's +6**, and the port read +3. That makes the demo's `slot+0x7E` `0x87`
instead of `0x11`, so `0x39A34` sets the roar's frame hold to −12.1, not +1.7.
§9.5's named suspects (`0x39A34`, `0x2AA70`, the `actors_anim_begin` seed) are
faithful; they only turned the wrong byte into the wrong timing. This
**supersedes §9.5's and §10's "likely owner"**.

### 11.1 The trace (measured, temporary, reverted)

The trace was `getenv("PR_ROAR")`-gated and has been reverted. It printed lines
from `actor_sync` around the `frame_timer` call, from `anim_code_39A34` after
its store and from `actors_anim_begin`'s tail, all for the side-0 fighter
record, plus the anim3 row at `fighter_reaction_apply`'s `0x3AD23` site. Here
`f = DS_0010150C` and `dump = f + 417`. The table shows the unmodified port;
each line is the state after that frame's sync.

| f | writer | stream `rec+8` | sprite | `+0x20` | `+0x24` |
|---|---|---|---|---|---|
| 72 | setter `0x3A504`: `+0x24 = 0` (`0x3A517`), `slot+0x7E = 0x87` | (crouch) | `0x9022` | 4 | 0 |
| 73 | handler phase 1 → `0x2BC30` (3.0f, `0x3A471`); timer 3 → 2 | `0xE7332` | `0x9075` | 2 | 3 |
| 74 / 75 | timer | `0xE7332` | `0x9075` | 1 / 0 | 3 |
| 76 | timer advances (old 0): `0xE7334` | `0xE7334` | `0x9076` | 2 | 3 |
| 77 / 78 | timer | `0xE7334` | `0x9076` | 1 / 0 | 3 |
| 79 | advance: `0xD100` → `0x39A34(rec, 10)`: `+0x24 = −121/10`; id `0x1077` | `0xE733E` | `0x9077` | −13.1 | −12.1 |
| 80..88 | `+0x24 ≤ 0`, so each frame advances one sprite (`0x2AC2D` `JNC` → return) | … | `0x9078`..`0x9080` | −26.2 … | −12.1 |

The anim3 row at f = 72 is `a0 = 0xDE95F`, with bytes
`08 18 08 73 02 0e fd 11`. The port's operand was `(s8)byte[+3] = 0x73 = 115`.
`0x3A549..0x3A554` store `slot+0x7E = byte[0xBECF8] + CL`, where
`byte[0xBECF8] = 0x14` (read_memory `0xBECF8`: `14 00 00 0e`). That gives
`0x14 + 0x73 = 0x87 = −121`.

Captures 848, 849 and 850 match port 494, 495 and 496 cleanly, and 851 ↔ 497
differs. So the port's sprite at dump 496 (`0x9077`, f = 79) is right, and the
error is that `0x9077` holds only one frame: at f = 80 the port already shows
`0x9078`. That matches §9.5's "the capture still shows the previous frame".

### 11.2 The raw, re-read (Ghidra, fixups applied)

* **`0x2AA70` (frame timer)** was checked instruction by instruction against
  `frame_timer`. `0x2AB2E..0x2AB44` do `+0x20 -= 1` and return while the old
  value is > 0 (`FLDZ; FCOMP [esp]; JC 0x2AC47`). The walk loop is
  `0x2ABAB..0x2ABD4` (EBP = 2). The id path is `0x2A39C`. `0x2AC15..0x2AC21`
  do `+0x20 = +0x24 + +0x20`. The tail returns if `+0x24 ≤ 0`
  (`FLDZ; FCOMP [ecx+0x24]; JNC`) and reloops while `+0x20 < 0` (`JA 0x2AB4F`).
  Operand widths, compare directions and float handling all match. **Faithful.**
* **`0x39A34`** (48 B: `53 83ec08 89c3 8b4014 85c0 741d 660fbe407e 89442404
  31c0 6689d0 890424 df442404 db0424 def9 d95b24 83c408 5b c3`) does
  `movsx ax,[eax+0x7e]`, then `FILD word` (s16 of the s8), then `FILD dword` of
  `(u16)dx`, then `FDIVP st(1),st` (value / operand), then `FSTP [ebx+0x24]`.
  This is the port's `anim_code_39A34`. **Faithful.**
* **`0x2BC30`** (`actors_anim_begin`): `0x2BC61 XOR EAX,EAX` /
  `0x2BC66 AND EAX,0xFFFF` / `JZ 0x2BC87` always takes the frame-bits arm, which
  stores `[esp+0x14]` into `+0x24` and `+0x20`. The pre-walk (`0x2BC96..0x2BCCA`)
  and `RET 4` (`0x2BCEF`) are faithful. The roar stream's first word is the
  literal `0x1075`, so nothing is pre-walked.
* **`0x2A1FC`'s gate** `TEST [ebx+0x24],0x7FFFFFFF` (`0x2A22B`) is faithful.
* **The roar stream `0xE7332`** reads `1075 1076 | D100 9A34 0003 000A |
  1077..1080 | D500 6870 0003 | …`. So `0x39A34` runs at the second advance
  and scales the hold of `0x1077..0x1080`.
* **The pinned site: `0x3AD15..0x3AD34`** (capstone over `read_memory`):

  ```
  0x3ad15 8d5c2418    lea ebx,[esp+0x18]      ; the anim3 triple's buffer
  0x3ad1e e8a1020000  call 0x3afc4            ; 0x3B010 mov [ebx],edx: anim3[0]
  0x3ad23 8b742418    mov esi,[esp+0x18]      ; anim3[0], the 11-byte row
  0x3ad27 8b7603      mov esi,[esi+3]         ; the DWORD at row+3 (bytes +3..+6)
  0x3ad2a 8b5c240c    mov ebx,[esp+0xc]
  0x3ad2e c1fe18      sar esi,0x18            ; its top byte, sign-extended: row+6
  0x3ad31 8a5b52      mov bl,[ebx+0x52]
  0x3ad34 89f2        mov edx,esi             ; -> 0x3A504/0x3A650/0x3A79C/0x3A8E8
  ```

  The port had `(s32)(s8)DSB(anim3[0] + 3u)`. The other readers of this row are
  byte loads and match the port: `0x3985F mov bl,[ebx+1]`, `0x3AE51..0x3AE5A`
  `mov dl,[ebx+4]` / `mov bl,[ebx+5]`, and `0x3B7FE`/`0x3B80B`
  `mov bl,[ebx+2]` / `mov dl,[edx+3]`, which is §9.3(b)'s 115. So the error is
  only at `0x3AD27`.

With the raw's byte, the demo's operand is `(s8)0xFD = −3`. Then
`slot+0x7E = 0x14 + 0xFD = 0x11`, and `0x39A34`'s hold is `17 / 10 = 1.7f`.

### 11.3 The fix and its assertion

* **Fix.** One line in `fighter_reaction_apply`:
  `edx3 = (u32)(s32)(s8)DSB(anim3[0] + 6u)`, with the raw's reading in a
  comment. It adds no new function, so the size gate does not apply.
* **Measured trace after the fix.** At f = 79 the log shows
  `39a34 … h24=1.7` and the sprite becomes `0x9077` with `+0x20 = 0.7`. At
  f = 80 `0x9077` holds (`+0x20 = −0.3`). f = 81 shows `0x9078`, f = 83
  `0x9079`, and f = 85 `0x907B`. At f = 85 `+0x20 = −0.2 < 0`, so the timer
  reloops (`0x2AC3F JA`) and skips `0x107A`.
* **Assertion (`test_fight.c` `check_pose_entry`).** The 11-byte row at
  `0xDE114` (char 0, reaction 0) is seeded with distinct bytes `0x70 + i`, with
  row+6 = `0xB0`, `byte[0xBECF8] = 0x14` and `slot+0x7E = 0x5A` as a sentinel.
  After `fighter_reaction_apply(s0, 0)` the test checks
  `DSB(s0 + 0x7E) == 0xC4`. The seeds and save/restores that set edx3 through
  `pose_chain_setup` and the check_reaction/winner_body tests move from
  `0xDE117` to `0xDE11A`. No existing assertion changed.
  * Before the fix (the +3 read), `test_fight.c:3039: 135 != 196` (`0x87`).
  * Mutation +7: `test_fight.c:3039: 139 != 196`.
  * Mutation +5: `test_fight.c:3039: 137 != 196`.
  * All three were reverted, and the suite then reported `all checks passed`.

### 11.4 Measured

| measurement | before (`895b42d`) | after |
|---|---|---|
| port 497 ↔ capture 851 | 6 544 B / 2 456 px | **0 B (clean)** |
| captures 851..857 | unexplained | 851 clean 497, 852..854 splice (497..499), 855..857 clean 500..502 |
| demo oracle first unexplained | 851 (raw 3758); window `[851..3616]` 2766 / 2760 unexpl. | **858 (raw 3765)**; `[858..3616]` 2759 / 2753 unexpl. |
| demo-fight ratchet | `[851..1884]` 1034, N = 851 | **`[858..1884]` 1027**, "ratchet improved: 858 > 851", **N raised to 858** |
| front-end oracle | `[560..850]` / 291 / 130 clean, 157 splice, 0 transition, 2 unexpl. (832, 833) | **`[560..857]` / 298 / 134 clean, 160 splice, 0 transition, 2 unexpl. (832, 833)** |

The front-end move is the one the brief allowed: the window comes from the
port's own dump, and captures 851..857 are now explained. The unexplained set
is unchanged.

**The new first unexplained frame, 858 (characterised, not fixed).** Capture
857 matches port 502 (f = 85) cleanly. Capture 858 is a tear frame. Measured
in row bands against the port, rows 0–66 equal port 502 (0 B) and rows 175–199
equal port 503 (0 B). Rows 136–174 match 503 except one worshipper sprite near
x 85–140 (1 114 B). Rows 67–135, which hold the fighters' upper bodies and the
backdrop band around them, match neither frame: 3 957 + 9 522 B against 503
and 7 185 + 5 521 B against 502. The best splice, 502/503 at byte 131 205
(row 136), leaves 13 617 B / 5 253 px, with a bbox of x 4–319 and y 67–174.
The difference mask covers the raptor's neck and back outline, the band around
both heads and that worshipper. In the next captures the ground rows keep
matching port N+1 exactly (859 ↔ 504, 860 ↔ 505 and 861 ↔ 506 in rows
175–199). The fighters' rows diverge more each frame, and the sky rows 0–66
differ by 500–3 000 B from 859 on. The owner is **not derived**. The
candidates are the fighters' upper-body animation or positions from f ≈ 86,
and the backdrop/sky scroll that starts at f ≈ 86 in the port (port 503's rows
0–66 differ from 502's by 24 201 B, while capture 858's equal 502's).

---

## 12. The mode-1 x operands at capture 858 (roar-timing Task 2, `b2cb490`)

**Result in one line.** Capture 858 is neither the fighters' animation nor the
camera. It is the arena's **mode-1 props** (temple, crowd, mountains), which
the port scrolled wrongly once the ramp became non-zero. The pset x writer
`0x2A690` reads both mode-1 operands as the **high word of a dword load**, and
the port read the low words: `+0x44` for the ramp entry at `+0x46`, and `+0x32`
for the velocity word at `+0x34`. This supersedes §11.4's "owner not derived"
for 858.

### 12.1 The trace (measured, temporary, reverted)

The trace was `getenv("PR_858")`-gated and has been reverted. It printed lines
from `game_loop` around `0x389C4`/`0x38A38` and `game_frame`, from
`camera_dispatch`'s call site, from `render_list` (each drawn pset) and from
`actor_pset_point` (each mode-1 record). Here `f = DS_0010150C` in state 7 and
`dump = f + 417`.

* **Camera.** `DS_000F0AF0` is 0 through f = 84. At f = 85 `camera_mode_track_pair`
  moves it to −86: the T-rex's `slot+0x34 = −6230` passes the `0x1800` dead zone
  by 86 (`a = 0`, `l0 = −86`, pair distance 12 118 ≤ `0x3000`). Then f = 86..92
  give −204, −259, −336, −383, −426, −465, −500.
* **Ramp.** The fill runs before `game_frame` (`0x25601`/`0x25606`), so it sees
  the previous frame's camera. At f = 86 it writes `0x107900[0] = −6`,
  `[83] = −43`, `0x107A46 = −10`, `DS_00107A3E` 344 → 343 and `DS_00107A3A`
  88 → 87. Up to f = 85 the table is all zero.
* **Layers at f = 85 → 86** (render list):

  | pset | layer | what | x f=85 → f=86 |
  |---|---|---|---|
  | `0x2BE1` | 1 | sea (sheared) | −328 → −327 |
  | `0x2BE0` | 2 | sky | −84 → −83 |
  | `0x02F4`, `0x02F5`, `0x02F6` | 94 | mountains (child of `0x02F4`) | 17 → 23, 174 → 180, 338 → 343 |
  | `0x02F1`..`0x02F3`, `0x02EF`/`0x02F0` | 174..192 | temple, props | unchanged |
  | `0x0878`, `0x07E1`, `0x8AAD`, `0x89CD` | 198..207 | crowd | their own walk only |

* **The mode-1 records at f = 86** (`actor_pset_point`, `rec+0x28` bit 12 set):

  | sprite | `+0x64` | `+0x32` | `+0x34` | `+0x44` word | `+0x46` word | port x |
  |---|---|---|---|---|---|---|
  | `0x02F4` mountain | −1 | 9362 | 320 | 0 | 0 | 6669 (6304 at f = 85) |
  | `0x02F2` temple | 14 | 3904 | 0 | 0 | −12 | 512 (unchanged) |
  | `0x0878` worshipper | 33 | 2688 | 128 | 0 | −21 | 7668 |

  The ramp arm (`+0x64 ≥ 0`) subtracted `2 × word[+0x44] = 0`, so the temple,
  props and crowd never scrolled. The velocity arm multiplied `0x107A46 = −10`
  by `word[+0x32] = 9362`, giving −365, so the mountain jumped 365/64 = 5.7 px.

### 12.2 What capture 858 shows (measured)

Capture 858 is a tear frame: rows 0–66 equal port 502 and rows 67–199 come
from the next frame. In rows 67–199, the sky, sea, fighters and ground match
port 503. The mountains in rows 83–137 sit where port 502 has them (row-band
offset 0), and so does the temple. Port 503 moves the mountains 6 px right.
Captures 859 and 860 show the mountains 1 px right of 502 (region x 215–300,
rows 40–90), with diffs growing row by row in rows 49–136 against port 504/505.
That is what a ramp scroll of a few 1/64 px per frame gives, not a 5.7 px jump.

### 12.3 The raw, re-read (Ghidra, fixups applied)

`0x2A690` (capstone over `read_memory` and `disassemble_function`):

```
0x2a6d3 807b6400      cmp byte [ebx+0x64],0     ; ramp index
0x2a6d7 7d48          jge 0x2a721
0x2a6d9 66837b3400    cmp word [ebx+0x34],0     ; velocity word
0x2a6de 742c          je  0x2a70c
0x2a6e0 8b4332        mov eax,[ebx+0x32]        ; dword at +0x32 ...
0x2a6e3 8b15447a1000  mov edx,[0x107a44]
0x2a6e9 c1f810        sar eax,0x10              ; ... top word: +0x34
0x2a6ec c1fa10        sar edx,0x10              ; 0x107A46
0x2a6ef 0fafd0        imul edx,eax
0x2a6f4 c1fa1f        sar edx,0x1f              ; 0x2A6F4..0x2A6FC: /256, truncating
0x2a6f7 c1e208        shl edx,8
0x2a6fa 1bc2          sbb eax,edx
0x2a6fc c1f808        sar eax,8
0x2a6ff..0x2a708      edi = [ebx+0x18] + 0x2a00 - eax
0x2a70c..0x2a71d      edi = [ebx+0x18] + 0x2a00 - ([0x107a44] >> 16)
0x2a721 8b4361        mov eax,[ebx+0x61]
0x2a724 c1f818        sar eax,0x18              ; byte +0x64
0x2a727 668b04450079  mov ax,[eax*2+0x107900]
0x2a72f 66894346      mov [ebx+0x46],ax         ; the ramp entry, stored at +0x46
0x2a733 8b4344        mov eax,[ebx+0x44]        ; dword at +0x44 ...
0x2a736 8b7b18        mov edi,[ebx+0x18]
0x2a739 c1f810        sar eax,0x10              ; ... top word: +0x46
0x2a73c 81c7002a0000  add edi,0x2a00
0x2a742 01c0          add eax,eax
0x2a744 29c7          sub edi,eax
```

The port had `(s32)(s16)DSW(rec + 0x44)` and `(s32)(s16)DSW(rec + 0x32)`. The
rest of `0x2A690` matches the port: the `0x2A6D9` gate, the `0x107A46` read,
the truncating `/256`, the `0x2A70C` arm, the y (`0x2A765..0x2A7A2`), the
layer (`0x2A7A5 cmp word [ebx+0x32],0` is a word compare and matches) and the
tail. `0x2A620` (`mode1_cursor`) also matches: `pset+0x14`, `0x107A4C`, the
`0x80` clamp. `0x2BE5C` already read `[rec+0x44] >> 16` (§0.3.11).

With the raw's words at f = 86: the mountain gets `p = −10 × 320 = −3200`,
`−3200 / 256 = −12`, so `x = −4448 + 0x2A00 + 12 = 6316` (0.19 px). The temple
gets `x = −10240 + 0x2A00 + 24 = 536`.

### 12.4 The fix and its assertions

* **Fix.** Two operand reads in `actor_pset_point` (`port/src/game/actors.c`):
  `(s32)DSD(rec + 0x44) >> 16` and `(s32)DSD(rec + 0x32) >> 16`, each with the
  raw's address in a comment. No function was added, so the size gate does not
  apply.
* **Assertions** (`test_fight.c` `check_type_callbacks`, a new block after the
  `0x2BE5C` one). Seeds are `rec+0x28 = 0x1000` and `rec+0x38 = 0`, with the
  saved and restored ramp entry `0x107906` and `DS_00107A44`.
  * Ramp arm: `+0x64 = 3`, entry `0xFFF4` (−12), `DSD(rec+0x44) = 0x55550777`
    (the `+0x46` sentinel 0x5555 and the low word 0x0777), `rec+0x18 = −10240`,
    `pset+4 = 0xDEADBEEF`. It checks `word[+0x46] == −12` and `pset+4 == 536`.
  * Velocity arm: `+0x64 = 0xFF`, `+0x32 = 9362`, `+0x34 = 320`,
    `0x107A46 = −10`, `rec+0x18 = −4448`. It checks `pset+4 == 6316`.
* **Mutations** (each reverted; the suite then passed):
  * Pre-fix `+0x44` read: `test_fight.c:4293: -3310 != 536`
  * Pre-fix `+0x32` read: `test_fight.c:4302: 6669 != 6316`
  * Unsigned `+0x46` word: `test_fight.c:4293: -130536 != 536`
  * Flooring `p >> 8` for the truncating `/256`: `test_fight.c:4302: 6317 != 6316`

### 12.5 Measured

| measurement | before (`efdc214`) | after |
|---|---|---|
| capture 858 ↔ port 502/503 | best splice 13 617 B / 5 253 px | **0 B** (splice at byte 64 254, row 66) |
| capture 859 ↔ port 504 | 8 339 px | 1 433 px; best splice 503/504 at byte 90 582 leaves 449 B / 159 px |
| demo oracle first unexplained | 858 (raw 3765); `[858..3616]` 2759 / 2753 unexpl. | **859 (raw 3766)**; `[859..3616]` 2758 / 2752 unexpl. |
| demo-fight ratchet | `[858..1884]` 1027, N = 858 | **`[859..1884]` 1026**, "ratchet improved: 859 > 858", **N raised to 859** |
| front-end oracle | `[560..857]` / 298 / 134 clean, 160 splice, 0 transition, 2 unexpl. (832, 833) | **`[560..858]` / 299 / 134 clean, 161 splice, 0 transition, 2 unexpl. (832, 833)** |

The front-end move is the one the brief allowed: the window comes from the
port's own dump, and capture 858 is now explained. The unexplained set is
unchanged. Title `54/55/2/0` and `54/57/0/0` (determinism 54), smk 120/120 and
41/41, attract 215/216 (expected divergence at 215), C-vs-Python 9866 and
`symbols.h` are unmoved.

**The new first unexplained frame, 859 (characterised, not fixed).** Capture
859 (f = 87) is a tear frame: its best splice, port 503/504 at byte 90 582
(row 94), leaves 449 B / 159 px. All of it lies in x 86–111, rows 137–174. That
is the worshipper behind the T-rex, sprite `0x07E1` at layer 208 (pset x 84,
y 135, 26 × 40 at f = 87). The capture shows a different pose from port 504's,
not a shift: the best offset over dx ∈ [−4, 4], dy ∈ [−2, 2] is (0, 0) at
159 px. Ports 503 and 505 are worse at their best offsets (547 and 648 px). In the next captures the
same box stays unexplained (254 px at 860 ↔ 505, 367 at 861 ↔ 506). The owner is
**not derived**. The candidate is the crowd actor's animation timing (the
`0x07E0` → `0x07E1` stream advanced at f = 86 in the port). The named gaps
§10 lists are not implicated here: `hit_record_x/y` first differ at f = 90, and
the `0x354F0` clamp and the split-arm write are inert at f = 87 (pair distance
12 346 < `word[0x9AF28]` = 20 480; `|slot+0x2C| ≤ 6 458 < 0x7C00`).

**Observed, not derived.** At f = 93 the port's camera steps −500 → −256
because side 0's `DS_00100AF0` anchor drops to 0 (`AB0` −448 → 320). This is
past the current residual and was not compared against the capture.

## 13. The worshipper's walk arrival at capture 859 (roar-timing Task 3, `afa47b3`)

**Result in one line.** Capture 859 is the type-0x20 worshipper (record
`0x2A7ED80`, fight-effect entry `0x1083CC`, sprite `0x07E1` in the port) reaching
the end of its walk at f = 87. The raw stops it there and starts its arrival
stream. The port never did, because `0x49C78`'s **type-1 case** (`0x49D2F`) was
an unported named gap (§7.4). That case and its callee `0x4AC38` are now ported:
one new function and one case body, about 170 raw bytes, well inside the size
gate. This supersedes §12.5's "owner not derived" for 859.

### 13.1 The trace (measured, temporary, reverted)

Three temporary traces were used and then reverted: `actor_sync` (gated by
`PR_859`), `render_list` (`PR_859R`) and `fight_effects_pass` (`PR_859E`).
`git status` was clean afterwards. Here `f = DS_0010150C` in state 7.

* **The actor.** It is spawned at f = 35 by the crowd path with
  `desc = 0xBB470`, stream `0xEE02C`, **type `0x20`** (the raw's `si = 0`),
  `a2 = −7185` and `a3 = 0x82D`. At f = 65 `0x4B144` gives it a walk: entry
  type 1, target `entry+0x14 = −4303`, `+0x34 = 0x80`, stream `0xC95D4[0]`.
  The walk loops `0xEE138`: `cd40 07da` (id `0x07DA + var`),
  `b840 0008 → 0xEE138` (opcode `0x18`, 8 frames), `8e40`, `c300 → 0xEE138`.
  That is 8 sprites `0x07DA..0x07E1` at a hold of 3.0, with no exit in the
  stream itself.
* **In the port** the entry stayed type 1 from f = 66 to the end, so the walker
  never stopped. At f = 86 `x18 = −4497` (distance 194) and at f = 87
  `x18 = −4369` (distance **66 ≤ 0x80**), which is the raw's arrival frame. At
  f = 88 it was still walking.
* **Hypothesis tests (overrides, reverted).** Forcing the `0x07E1` pset to any
  other walk id (`0x07D2..0x07E8`, `0x07AD`) or shifting it by ±1–3 px left
  119–170 px. Forcing the T-rex (`0x107B`) or the neighbouring worshipper
  (`0x0878`) made it worse. So the residual was not a pose or phase error
  inside the walk.

### 13.2 The raw, re-read (Ghidra, fixups applied)

The jump table at `0x49C2C` sends type 1 to `0x49D2F` and type 2 to `0x49D90`.
The listing below is capstone over `read_memory`:

```
0x49d2f 803dc288100000  cmp byte [0x1088c2],0
0x49d36 7414            je  0x49d4c
0x49d3f e808200000      call 0x4bd4c            ; retarget, type 8, if it fires
0x49d44 85c0 / 0f851c070000  test eax,eax / jne 0x4a468
0x49d4c 8b5108          mov edx,[ecx+8]         ; actor
0x49d4f 8b4114          mov eax,[ecx+0x14]      ; target
0x49d52 8b5218          mov edx,[edx+0x18]
0x49d55 29c2            sub edx,eax
0x49d59 7d02 / f7da     jge / neg edx           ; |x - target|
0x49d60 6683783400      cmp word [eax+0x34],0
0x49d67 8b4032          mov eax,[eax+0x32]      ; dword at +0x32 ...
0x49d6a c1f810 / f7d8   sar eax,0x10 / neg eax  ; ... top word +0x34, negated if < 0
0x49d71 8b4032 / c1f810 mov eax,[eax+0x32] / sar eax,0x10
0x49d77 39c2            cmp edx,eax
0x49d79 0f8fe9060000    jg  0x4a468             ; signed: stays walking if d > step
0x49d86 e8ad0e0000      call 0x4ac38

0x4ac38 53 / 89d3       push ebx / mov ebx,edx  ; EAX = entry, EDX = index
0x4ac3e 806229bf        and byte [edx+0x29],0xbf
0x4ac45 804a2910        or  byte [edx+0x29],0x10
0x4ac4c 66c742340000    mov word [edx+0x34],0
0x4ac55 66c742360000    mov word [edx+0x36],0
0x4ac5e 66c742380000    mov word [edx+0x38],0
0x4ac64 c6401e00        mov byte [eax+0x1e],0
0x4ac68 8b149d44950c00  mov edx,[ebx*4+0xc9544]
0x4ac72 680000a040      push 0x40a00000         ; hold 5.0
0x4ac77 e8b40ffeff      call 0x2bc30
```

`0xC9544` holds `0xEE02C, 0xEE3E6, 0xEE726, 0xEEAE2, 0xEEED4, 0xEF28A` for types
`0x20..0x25`. Entry 0 is the worshipper's own spawn stream (first sprite
`0x0836`). `0x4BD4C` was already ported (`fight_4bd4c`).

### 13.3 The fix and its assertions

* **Fix** (`port/src/game/fight.c`). There is a new `fight_4ac38` (`0x4AC38`)
  and a `case 1` in `fight_effects_pass` that transcribes `0x49D2F..0x49D8B`.
  The type-2 case (`0x49D90`, a countdown into the same `0x4AC38`) is not
  reached here and stays a named gap. Its sibling types 4..12 stay gaps as well.
* **Assertions** (`test_fight.c`, `check_effects_arrival`, run after
  `check_effects_rng`). `0xC9544[0]` is seeded with a scratch stream holding the
  literal id `0x0123`, and it is saved and restored.
  * Arrival with the demo's f = 87 values: `x = −4369`, target `−4303`,
    `DSD(+0x32) = 0x00801234` (step `0x80`, low word sentinel `0x1234`),
    `+0x36 = 0x5555`, `+0x38 = 0x6666`, `+0x29 = 0x4B`. It checks type 0, the
    three velocity words 0, `+0x29 == 0x13` (0xBF/0x10 here, 0x08 by `0x2BC30`),
    `rec+8 == stream`, `+0x24 == 0x40A00000` and pset id `0x0123`.
  * Still walking: `x = −4497` (distance 194). It checks type 1, `+0x34 == 0x80`,
    stream and pset unchanged.
  * Leftward walker: `+0x34 = −0x80`, distance 100. It checks that it arrives.
  * `d == step` (128) arrives.
* **Mutations** (each reverted; the suite then passed):
  * No case 1 (pre-fix): `test_fight.c:1083: 1 != 0`
  * `(s16)DSW(+0x32)` for the step: `test_fight.c:1099: 0 != 1`
  * No negation: `test_fight.c:1109: 1 != 0`
  * `d < step`: `test_fight.c:1117: 1 != 0`
  * Hold 3.0: `test_fight.c:1089: 1077936128 != 1084227584`
  * No hflip clear: `test_fight.c:1087: 83 != 19`
  * Each of the `0x4AC45`/`0x4AC55`/`0x4AC5E`/`0x4AC64` stores dropped: lines
    1087/1085/1086/1083 fail.

### 13.4 Measured

| measurement | before (`5aa1ac6`) | after |
|---|---|---|
| capture 859 ↔ port 503/504 | best splice 449 B / 159 px | **0 B** (splice at byte 90 582, row 94) |
| demo oracle first unexplained | 859 (raw 3766); `[859..3616]` 2758 / 2752 unexpl. | **860 (raw 3767)**; `[860..3616]` 2757 / 2751 unexpl. |
| demo-fight ratchet | `[859..1884]` 1026, N = 859 | **`[860..1884]` 1025**, "ratchet improved: 860 > 859", **N raised to 860** |
| front-end oracle | `[560..858]` / 299 / 134 clean, 161 splice, 0 transition, 2 unexpl. (832, 833) | **`[560..859]` / 300 / 134 clean, 162 splice, 0 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. That is the move the brief allowed.

**The new first unexplained frame, 860 (characterised, not fixed).** Capture
860 (f = 88) is a tear. Its best splice, port 504/505 at byte 117 108 (row 121),
leaves 476 B / 164 px, all in x 86–106, rows 136–172: the same worshipper. The
capture still shows its arrival sprite `0x0836`. Forcing `0x0836` at f = 88
gives 0 B. At f = 88 the port's entry is type 0 again, so `0x4AAD0` runs. Its
`0x4AB7F test byte [slot+0x42],1` gate on the side-0 fighter slot `0x1077B0`
fires (`+0x42 = 0x01`) and `0x4B430` retargets the actor. That sets entry
type 8 and stream `0xC958C[0] = 0xEE0F0` (sprites `0x079F..`). The port matches
the raw gate (`0x4AAD0` was re-diffed and matches). Suppressing that one gate
(a temporary `PR_NO42` switch, reverted) made captures 860..863 explained at
0 B. The first residual then moved to 864 (495 B, x 0–23, rows 99–118).
So in the raw, bit 0 of `slot+0x42` is clear by f = 88. The port sets it at
f = 72 (`0x3ABD0..0x3ABDA`, the roar reaction, which matches the raw) and
never clears it. The owner, the raw clear that the port lacks, is **not
derived**. The candidates come from a capstone sweep of byte writes to `+0x42`.
The unported functions that store 0 or a register there are `0x361C8`,
`0x370F0`, `0x37464`, `0x37B54`, `0x3C208`, `0x3C358`, `0x3D0C0`, `0x3EE00`,
`0x3EA24`, `0x44798`, `0x4505C` and `0x48AAC`. The ported `0x3C148`,
`0x385B0`, `0x36870` and `0x3BDDC` also clear it, but the port's trace shows
`+0x42 = 0x01` on every frame from f = 72 through f = 100, so none of them
fires in this window. The f = 93 camera step (§12.5) is still not
compared.

## 14. The slot `+0x42` reset at capture 860 (roar-timing Task 4, `b915712`)

**Result in one line.** The raw clears bit 0 of the side-0 slot's `+0x42` in the
same frame it is set. The clear is the effects pass's own tail: `0x49C78` calls
`0x4A634` unconditionally at `0x4A591`. `0x4A634` clears `+0x42` bits 0/1 of
both fighter slots and zeroes the `0x10889E`/`0x1088B2` bytes every frame. The
port had skipped the call as a named gap, "the mode tail". So the roar's bit 0,
set at f = 72, stayed set, and at f = 88 `0x4AB7F` retargeted the worshipper to
type 8. The fix is one function of 211 raw bytes, well inside the size gate.
This supersedes §13.4's "owner not derived" and its twelve candidates.

### 14.1 How it was found

§13.4's sweep only looked at writes of the form `[reg+0x42]`. None of those
clears bit 0 of a *slot* (`0x1077B0 + side·0x94`):

* `0x3C148`, `0x385B0`, `0x36870` and `0x3BDDC` write the *record* (`[slot]`)
  `+0x42`. For example, `0x3C155 mov eax,[eax*4+0x1077b0]` is followed by
  `0x3C166 mov byte [eax+0x42],0`.
* The read-modify-write sites (`0x362B7`, `0x37126`, `0x376B7`, `0x37CE2`,
  `0x3C3AB`, `0x3EC89`, `0x44DF3`, `0x458CB`, `0x4890D`, `0x48C29`) only OR in
  bits 2/3/7 or clear bit 2.

A second capstone sweep over the raw code object looked for memory operands
whose displacement covers the slot fields directly, pre-fixup `0x877F2` /
`0x87886` (runtime `0x1077F2` / `0x107886`). It found one read-modify-write that
clears bits 0/1: `0x4A6D7..0x4A6E0`. That code is inside `FUN_0004a634`, whose
only caller is `0x4A591` in `FUN_00049c78` (Ghidra `get_xrefs_to`).

### 14.2 The raw (Ghidra `read_memory`, fixups applied, capstone)

```
0x4a591 e89e000000      call 0x4a634            ; unconditional; mode 9 joins here too
0x4a596..0x4a5a0        mov byte [0x1088c2],0   ; (bl = 0) the port's existing tail

0x4a637 31d2 / 31db     xor edx,edx / xor ebx,ebx   ; EDX = side, EBX = side*0x94
0x4a63b b902000000      mov ecx,2
0x4a640 f683f277100002  test byte [ebx+0x1077f2],2  ; slot+0x42 bit 1
0x4a647 0f848a000000    je 0x4a6d7
0x4a64f 8a82a8881000    mov al,[edx+0x1088a8]       ; the side's reaction byte (zero-extended)
0x4a655 83f820 / 7c30   cmp eax,0x20 / jl 0x4a68a
0x4a65a 83f83f / 7f2b   cmp eax,0x3f / jg 0x4a68a
0x4a65f b803000000      mov eax,3
0x4a664 e873310100      call 0x5d7dc                ; rng(3) -> voice 0xCD/0xCE/0xCF
0x4a675..0x4a688        (voice id by the draw) jmp 0x4a6d2
0x4a692 83f810 / 7c40   cmp eax,0x10 / jl 0x4a6d7
0x4a697 83f817 / 7f3b   cmp eax,0x17 / jg 0x4a6d7
0x4a69c 89c8 / e839310100  mov eax,ecx / call 0x5d7dc   ; rng(2)
0x4a6a3 85c0 / 7430     test eax,eax / je 0x4a6d7
0x4a6a7 89c8 / e82e310100  mov eax,ecx / call 0x5d7dc   ; rng(2) again
0x4a6b2..0x4a6cd        0x2C3FC(0xC9 or 0xCA), then eax = 0xDA or 0xDB
0x4a6d2 e8251dfeff      call 0x2c3fc                ; voice
0x4a6d7 8a83f2771000    mov al,[ebx+0x1077f2]
0x4a6dd 24fc            and al,0xfc                 ; clear bits 0/1
0x4a6df 42              inc edx
0x4a6e0 8883f2771000    mov [ebx+0x1077f2],al
0x4a6e6 30e4            xor ah,ah
0x4a6e8 81c394000000    add ebx,0x94
0x4a6ee 88a29d881000    mov [edx+0x10889d],ah       ; 0x10889E[side] = 0 (edx already +1)
0x4a6f4 88a2b1881000    mov [edx+0x1088b1],ah       ; 0x1088B2[side] = 0
0x4a6fa 83fa02 / 0f8c3dffffff  cmp edx,2 / jl 0x4a640
```

`0x2C3FC` is the voice subsystem. It is out of scope (spec §7) like every other
voice call in the port. Its callees (`0x1CA14`..`0x1D244`) do not include
`0x5D7DC`, so the draws are the only state that matters besides the clears.

### 14.3 The fix and its assertions

* **Fix** (`port/src/game/fight.c`). There is a new `fight_4a634`, and the
  `/* PORT: 0x4A591 0x4A634 … named gap */` line in `fight_effects_pass` is
  replaced by the call. It sits before `DS_001088C2 = 0` (`0x4A5A0`), as in the
  raw. `fight.h`'s gap list no longer names `0x4A634`.
* **Measured in the demo** (a temporary `fprintf`, reverted). The tail sees
  set bits only at f = 72: `side 0 +0x42 = 0x01, 0x1088A8[0] = 0x0B` and
  `side 1 +0x42 = 0x02, 0x1088A8[1] = 0x01`. Neither reaction byte is in
  `0x10..0x17` or `0x20..0x3F`, so the demo draws no RNG here. The fix changes
  exactly the clears.
* **Assertions** (`test_fight.c`).
  * The new `check_effects_tail` runs with an empty list and restores every
    byte it seeds. Its sentinels are:
    * `s0+0x42 = 0x5B` and `s1+0x42 = 0xA5`, which become `0x58` and `0xA4`
    * `0x10889E[0..2] = 11 22 77` and `0x1088B2[0..2] = 33 44 88`, which become
      `0 0 77` and `0 0 88`
  * The RNG state is the proof of each draw. Seed `0x1234` steps to
    `0xBAC6D4B3` and then `0x5589507A` (its first rng(2) is 1). Seed `0x1235`
    steps to `0x73D3E76C` (its first rng(2) is 0). The cases are:

    | reaction byte | draws |
    |---|---|
    | `0x18` with bit 1 | none |
    | `0x20` without bit 1 | none |
    | `0x3F` | one rng(3) |
    | `0x10` | rng(2) = 1, then a second rng(2) |
    | `0x17` | rng(2) = 0, no second draw |
    | `0x1F`, `0x40` | none |

    Each case seeds the other side's reaction byte with a value that would
    draw differently.
  * `check_effects_arrival` gains the missing `DS_001088C2` pre-gate cases
    (`0x49D2F`). With `DS_001088C2 = 1` and a scratch `0xC9754[0]` stream (id
    `0x0456`), a `d == step` walker is retargeted by `0x4BD4C`: type 8, hold
    `0x40800000` and pset `0x0456`, and the tail zeroes `DS_001088C2`. With
    `0xC9754[0] = 0` it arrives: type 0, hold 5.0. `0xC9754[0]` is saved and
    restored.
* **Mutations** (each reverted by the script; `fight.c` was compared
  byte-for-byte after the runs, and the suite then passed):

  | mutation | first failure |
  |---|---|
  | no `0x4A634` call (pre-fix) | `test_fight.c:1199: 91 != 88`, `:1200`, `:1201` |
  | `&= 0xFE` | `:1199: 90 != 88`, `:1217: 2 != 0` |
  | no rng(3) | `:1216: 4660 != -1161374541` |
  | `<= 0x3E` | `:1216` |
  | `>= 0x1F` | `:1242: -1161374541 != 4660` |
  | `<= 0x18` | `:1207: 1435062394 != 4660` |
  | `>= 0x11` | `:1225: 4660 != 1435062394` |
  | second rng(2) always drawn | `:1233: -1615734229 != 1943267180` |
  | the other side's reaction byte | `:1207`, `:1216`, `:1225` |
  | no bit-1 gate | `:1207`, `:1216` |
  | three sides | `:1203: 0 != 119`, `:1206: 0 != 136` |
  | no `0x10889E` clear | `:1201`/`:1202` |
  | no `0x1088B2` clear | `:1204`/`:1205` |
  | no `DS_001088C2` → `0x4BD4C` pre-gate | `:1136: 0 != 8`, `:1137`, `:1138` |

### 14.4 Measured

| measurement | before (`bcf8ce5`) | after |
|---|---|---|
| capture 860 ↔ port 504/505 | 476 B / 164 px | **0 B** (splice at byte 117 108, row 121) |
| captures 861..863 | unexplained | 861 splice 505/506 (row 149); 862 = port 506, 863 = port 507 (0 B) |
| demo oracle first unexplained | 860 (raw 3767); `[860..3616]` 2757 / 2751 unexpl. | **864 (raw 3771)**; `[864..3616]` 2753 / 2747 unexpl. |
| demo-fight ratchet | `[860..1884]` 1025, N = 860 | **`[864..1884]` 1021**, "ratchet improved: 864 > 860", **N raised to 864** |
| front-end oracle | `[560..859]` / 300 / 134 clean, 162 splice, 0 transition, 2 unexpl. (832, 833) | **`[560..863]` / 304 / 136 clean, 164 splice, 0 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved, which is the move the brief allowed.
These were unmoved:

* title `54/55/2/0` and `54/57/0/0`, determinism 54
* smk 120/120 and 41/41
* attract 215/216 (expected divergence at 215)
* C-vs-Python 9866
* `symbols.h`

The front-end "endpoints BAD" line is the same as on `bcf8ce5`.

**The new first unexplained frame, 864 (characterised, not fixed).** Capture
864 (f = 91) matches port 508 except for 495 B / 165 px in x 0–23, rows 99–118.
§13.4's `PR_NO42` experiment had predicted this residual exactly. The capture
shows a grey figure at the left screen edge in front of the temple columns. It
is small, with a long thin horizontal limb reaching to x 0. It stays in
captures 865 and 866 (498 B at 865 ↔ 508/509). The port draws no such
figure in ports 508..511 (inspected at 4×). The changes against port 507 in
that box (49 px at 508, 416–421 px at 509/510, 91–122 px from 511) are
scene changes (the blood splash visibly moves at 510/511); none of them is the
figure. So the raw has an actor (or an
effect-list entry) visible from f = 91 that the port never spawns or never
draws. The owner is **not derived**. Its candidates are:

* an unported fight-effect type (2, 4..12, or the case-13/14 bodies §7.4 left
  as gaps)
* an unported spawn in the crowd/prop path

From capture 866 on, the fighters' rows (97–192) also diverge, by 27 645 B at
866 ↔ 509/510.

## 15. The grey flier at capture 864 (roar-timing Task 5, `7147288`)

**Result in one line.** The grey figure at the left edge of captures 864/865 is
the flying creature `0x1282C` spawns from descriptor `0xBB254` (type `0x01`,
stream `0xE8A94`, sprites `0x0281..`, effects palette `0x105FF3C`). The port
never showed it for two reasons. First, state 6's reset `0x20DF4` calls
`0x12750` at `0x20E33`, and that call builds the node list the actor's cb1
`0x127C0` pops. The port had left the call as part of the `0x20DF4` named gap,
so every such spawn was refused. Second, the front-end driver entered state 2
with the frame counter `DS_000EF6DC` at 0. The raw has counted every loop
iteration since boot by then, so the spawn gate `(DS_000EF6DC & 0x3F) == 0`
opened at the wrong frames. The fix is one new function of 0x4D raw bytes
(`0x12750..0x1279C`), its call, and the driver seed, well inside the size gate.
This supersedes §14.4's "owner not derived" for 864.

### 15.1 The sprite (measured)

* **Colours.** The figure's pixels (x 0–23, rows 99–118 of capture 864) are
  DAC `81` (154,154,154), `82` (113,113,113), `109` (65,48,16) and black. At
  f = 91 the port's palette table (`0x107618..`) maps `0x48..0x77` to entry
  `0x107638`, handle `0x105FF3C`, which is the effects palette the blood splash
  `0x8511` and ring `0x0089` use.
* **Search.** A temporary hook (reverted) decoded every resolvable sprite id
  `1..0x7FFF` at f = 91 through `sprite_blit` with pal `0x107638`: 18 360
  sprites, 743 of which contain indices 81, 82 and 109. Each was then placed
  over the box, with and without hflip, at every position. The only near-exact
  fit is `0x8281` (`0x0281` hflipped, 31 × 20) at screen (−7, 99), with 1 px
  left over. The next best leaves 157 px.
* **Descriptor.** In the data object, `0x0281` is the first id of stream
  `0xE8A94` (`cd40 0281 b840 0008 8a94 000e 8e40 c300 …`). Its only descriptor
  is `0xBB254`: stream `0xE8A94`, type `0x01`, frame 4, `+6 = 0x11`,
  flags `0x80`, handle `0x105FF3C`. Ghidra `get_xrefs_to 0xBB254` returns
  `0x128B6` (in `0x1282C`, the port's `camera_dust_spawn`), `0x1295D` (in
  `0x128D4`, reached only from `0x26C8C`, not in the demo frame) and the type
  table `0xBB9E4`.

### 15.2 Why the port never drew it (measured, temporary trace reverted)

* **The list.** `camera_dust_spawn` did fire in the port: at f = 81,
  `DS_000EF6DC = 0x440`, rng(7) = 0, and it called `actor_spawn(0xBB254,
  −10240, 1482, 7292, 0x4000)`. That call returned 0. Type `0x01`'s cb1
  `0x127C0` pops the `0xF0A78` list and returns `0xFF` when it is empty
  (`0x127C4..0x127E8`). `DSD(0xF0A78)` was 0 because nothing had built the
  list.
* **Who builds it.** Ghidra `get_xrefs_to 0xF0A78` names only `0x12750` as a
  writer. Its callers are `0x20E33`, in `0x20DF4` (the state-6 reset the port
  transcribes in `game_state_6`), and `0x20EDA`, in the sibling reset
  `0x20EB8`, which the demo does not reach.

  ```
  0x20e2e e85db50000  call 0x2c390
  0x20e33 e81819ffff  call 0x12750      ; the node lists
  0x20e38 e8c3840200  call 0x49300      ; fight_list_init (already ported)

  0x12753 bae00a0f00  mov edx,0xf0ae0
  0x12758 b9780a0f00  mov ecx,0xf0a78
  0x1275d bb800a0f00  mov ebx,0xf0a80
  0x12762 8915e40a0f00 mov [0xf0ae4],edx ; in-use sentinel: prev
  0x12768 8915e00a0f00 mov [0xf0ae0],edx ;                  next
  0x1276e 890d7c0a0f00 mov [0xf0a7c],ecx ; free sentinel:   prev
  0x12774 890d780a0f00 mov [0xf0a78],ecx ;                  next
  0x1277a 81fbe00a0f00 cmp ebx,0xf0ae0
  0x12780 7317        jnc 0x12799
  0x12782 b8780a0f00  mov eax,0xf0a78
  0x12787 89da        mov edx,ebx
  0x12789 83c30c      add ebx,0xc
  0x1278c e82f220100  call 0x249c0      ; insert before the sentinel: tail-append
  0x12791 81fbe00a0f00 cmp ebx,0xf0ae0
  0x12797 72e9        jc 0x12782
  ```

  That is eight 12-byte nodes, `0xF0A80..0xF0AD4`, on the free list.
  `0x127C0`/`0x12800` (`actor_type_127C0`/`actor_type_12800`) and
  `0x1282C` were already ported and match the raw: `0x1282C` was re-diffed
  instruction by instruction (`0x12834..0x128C5`).
* **With the list built but the counter unseeded** (the fix alone), the flier
  spawns at f = 81 and is visible from capture 858 on. That made 862..865
  unexplained (370, 297, 764 and 741 B), and the ratchet would have failed.
  So the spawn frame is wrong, and the gate's only input is `DS_000EF6DC`.

### 15.3 The frame counter

* **Raw.** `DS_000EF6DC` is a word, 0 in the image (Ghidra `read_memory
  0xEF6DC`). Its only writer is `0x24C5C`'s per-iteration increment
  (`0x24CCD mov di,[0xef6dc]` / `0x24CD4 inc edi` / `0x24CDB mov [0xef6dc],di`;
  `get_xrefs_to` lists no other write). So it counts loop iterations since
  boot.
* **The driver.** The front-end driver (`test_game.c`) skips the boot attract
  and the title and enters state 2 directly. It already re-seeds the LCG to the
  attract's post-state (`FRONTEND_RNG_AFTER_ATTRACT`), but it left the counter
  at 0, so its first state-2 frame counts 1.
* **The port's own boot run.** `prageport --check 2500`, traced temporarily and
  then reverted, runs state 0 for n = 1..690 and the title state 1 for
  n = 691..886. The title is 1 phase-0 frame, 95 phase-1 steps of `0x10`
  from `0x600`, and the `0x3E688` fade. The first state-2 frame counts 887,
  so **886 iterations precede state 2**.
* **Capture bound.** Each frontend capture frame was matched to the natural
  run's frames, using `window.txt`'s raw index.
  * Over the attract (n = 2..689), `raw − n·70.09/60.05` stays at
    1365 ± 1, so the port's attract is iteration-exact.
  * From the title's exact frame n = 769 (capture 309) through state 2
    (capture 391, n = 980) and state 3 (capture 562, n = 1476), it stays at
    1384.4..1387.2.
  * The only uncounted time is n = 689..769. There the capture freezes over raw
    2174..2192 (the title load), which is 16 game-frame times, and animates
    continuously from 2192 on.
  * So the original's count before state 2 lies in **886 + [−4, +17]**.
* **Mod-64 pin.** The flier pins the count mod 64. With the counter seeded to
  886, the gate opens at f = 91, and captures 864 and 865 become exact at 0 B.
  A temporary probe (reverted) added k to the counter at the gate for
  k = 53..57, the residues mod 64 around 886's 54. Only k = 54 explains both
  frames: k = 53 leaves 864 at 495 B, k = 55 makes 863 unexplained (492 B) and
  k = 56/57 make 862 unexplained.
* **Why the other 59 residues are excluded.** Only residues 53..57 were probed.
  The rest are excluded by argument, not by run.
  * **One opening per residue.** The flier can spawn only in state 7:
    `0x1282C` runs in the arena frame, and the list is built in state 6. The
    gate opens every 64 iterations, so each residue r gives exactly one
    state-7 opening in f = 65..128.
  * **The captured flier is the left-edge variant.** It is the hflipped
    `0x8281` (`a5 = 0x4000`, x = camera − `0x2800`), showing its stream's first
    sprite `0x0281` at (−7, 99) at f = 91. The probes show this variant on
    screen from its spawn onward (k = 55 → capture 863, k = 56/57 → 862).
  * **Openings at f = 65..90.** If such an opening spawns the flier, the flier
    shows before capture 864. But captures 843..863 are all explained without
    it. If the draw spawns nothing (rng(7) & 3 ≠ 0), the next opening is
    f ≥ 129, and 864 has no flier.
  * **Openings at f = 92..128.** These leave 864 without the flier.
  * **Result.** Only the opening at f = 91 matches, which is r = 54. Within the
    capture's bound (882..903), 886 is the only count with that residue.
* **Caveat (`TODO(verify)` in `test_game.c`).** The original's live counter is
  **unread**: there is no live-RAM dump (§1.6). The seed is the port's own
  boot-run count, which is the driver's alignment, not a measured original
  value. The capture bounds the original to 882..903. The mod-64 pin maps the
  capture's spawn frame back to a state-2 count only through the port's
  modelled iteration count from state 2 to f = 91. That count includes the
  state-6 loader, whose read-stall tick count (`0x1B3AC`'s tail re-sync at
  `0x1B45F`/`0x1B464`) `port/spec/game_flow.md`'s residual 1 records as
  un-derivable. If the original ran a different number of iterations there,
  its state-2 count differs from 886 by that amount mod 64, while the demo's
  spawn frame still matches.
* **Cross-check in-tree.** `test_attract`'s continuous `PR_ATTRACT_DUMP` run
  asserts `DSW(DS_000EF6DC) == 690` at the title entry. It then drives the
  title to its state-2 handoff (`0x12459`) and asserts
  `== FRONTEND_FRAMES_BEFORE_STATE2` (886). This is the counter's counterpart
  of the run's `handoff_lcg == 0x4308698B` check for the LCG seed.
  * Mutations: a `+2` increment gives `1380 != 690` and `1772 != 886`. A title
    step of `0x11` instead of `0x10` gives `881 != 886`.
  * The extra title iterations dump 100 more title frames (196 in total, under
    the hook's 200 cap). `tools/attract_compare.py`'s output is byte-identical
    before and after.
* **Side effect: the ported readers only.** Ghidra lists 28 references to
  `0xEF6DC`: 27 reads and the one write. The claim here covers only the
  **ported** readers:
  * the parity readers (`frame_timer`'s `0x2AAFC`, the fighter's `0x375BF`
    and the `0x35Axx` arm) see an even offset
  * the overlay's `0x2BF27`/`0x2BFCE`/`0x2BFE8` blink is gated off by
    `CREDITS:5`
  * the anim-spawn child bit (`0x2B504`) sees parity only
  * `0x1282C` is the gate this section is about

  The unported readers, for example `0x128DC` (`0x128D4`, not in the demo
  frame), `0x254B7`, `0x266AC`'s `0x2686F`/`0x2688D`, `0x27ACD`, `0x3838E` and
  `0x44643`, are not measured under the seed.

### 15.4 The fix and its assertions

* **Fix.**
  * `camera_dust_list_init` (`port/src/game/camera.c`, `0x12750`) is new and
    exported in `camera.h`.
  * `game_state_6` calls it at the raw position, before `fight_list_init`
    (`0x20E33`, then `0x20E38`). The `PORT:` gap comment now names only the
    remaining calls.
  * The front-end driver seeds `DSW(DS_000EF6DC) = 886`
    (`FRONTEND_FRAMES_BEFORE_STATE2`) beside its LCG seed.
* **Assertions.**
  * **`check_dust_list`** (`test_fight.c`) fills `0xF0A78..0xF0AE7` with
    `0xA5`, then checks:
    * both sentinels
    * the eight-node order and prev links
    * the untouched node `+8`
    * the free tail `0xF0AD4`

    It then checks that an `0xBB254` spawn is accepted: type 1, `+0x14 =
    0xF0A80`, the node's owner is the record, the in-use head is `0xF0A80` and
    the free head is `0xF0A8C`. On an empty free list the spawn returns 0. The
    snapshot of `0xF0A78` grows from 0x10 to 0x68 bytes so that the nodes are
    restored.
  * **`check_state6`** fills the same range with `0xA5` before the state-6
    step and checks the four sentinel links after it.
  * **The driver** records the loop frame on which a type-`0x01` actor is first
    live. It checks loop frame 1097 (dumped 508, capture 864), with
    `DS_0010150C` reading 92 after that iteration; the spawn ran at f = 91
    inside it. The sentinel is −1.
* **Mutations.** Each was reverted by the script; the suite then passed.

  | mutation | failures |
  |---|---|
  | no `0x12750` call in state 6 | `test_fight.c` state-6 links (4 lines); driver `-1 != 92`, `-1 != 1097` |
  | head insert (`0x249B0`) instead of `0x249C0` | state-6 tail/head, `check_dust_list` order and tail |
  | node stride `0x10` | order, tail, `n: 6 != 8` |
  | bound one node past | links, `+8` of the ninth node, `n: 9 != 8` |
  | no `0x12768` in-use next | `0xF0AE0` link in both checks |
  | no `0x12762` in-use prev | `0xF0AE4` link in both checks |
  | driver: no counter seed | `82 != 92`, `1087 != 1097` (spawn at f = 81) |
  | driver: seed 885 / 887 | `93`/`91 != 92`, `1098`/`1096 != 1097` |

### 15.5 Measured

| measurement | before (`60ad24b`) | after (`7147288`) |
|---|---|---|
| capture 864 ↔ port 508 | 495 B / 165 px | **0 B** |
| capture 865 ↔ port 508/509 | 498 B / 166 px | **0 B** (splice at byte 66 513, row 69) |
| demo oracle first unexplained | 864 (raw 3771); `[864..3616]` 2753 / 2747 unexpl. | **866 (raw 3773)**; `[866..3616]` 2751 / 2745 unexpl. |
| demo-fight ratchet | `[864..1884]` 1021, N = 864 | **`[866..1884]` 1019**, "ratchet improved: 866 > 864", **N raised to 866** |
| front-end oracle | `[560..863]` / 304 / 136 clean, 164 splice, 0 transition, 2 unexpl. (832, 833) | **`[560..865]` / 306 / 137 clean, 165 splice, 0 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved, which is the move the brief allowed.

**Counter width (fix round 1).** `flow.c` incremented the counter as a dword
(`DSD(DS_000EF6DC)++`), but the raw increments a word
(`0x24CCD mov di,[0xef6dc]` / `0x24CD4 inc edi` / `0x24CDB mov [0xef6dc],di`),
and `0xEF6DE` is a separate global (Ghidra: read at `0x1BE21`, written at
`0x5D808`). The increment is now `DSW(DS_000EF6DC) = (u16)(DSW(...) + 1)`.
`check_game_frame_tail` seeds `0xFFFF` with a `0x5A5A` sentinel at `0xEF6DE`
and checks the wrap to 0 with the sentinel untouched. The dword mutation fails
it with `23131 != 23130`. The two widths differ only on a wrap, and no oracle
run reaches one. Re-measured on the full ladder, every enforced oracle is
unmoved (fix-round report in `task-5-report.md`).

**The new first unexplained frame, 866 (characterised, not fixed).** Capture
866 is the f = 92/93 tear. Its best splice, port 509/510 at byte 131 847 (row
137), leaves 27 858 B / 9 891 px spread over x 0–319 and rows 97–192. Both
fighters and the ground differ. Against port 509 the capture differs in
2 971 px above row 137 and 8 881 px below it. The ground rows 185–199 match
no port frame from 508 to 511 at any horizontal shift in [−8, 8] (best 821 px
against 509), so the scroll itself differs.

At f = 93 the port changes three things at once:

* its camera steps −500 → −256 (§12.5's still-uncompared jump: side 0's
  `DS_00100AF0` anchor drops to 0)
* the T-rex's sprite switches `0x907E` → `0x96B5` and moves to x −75
* the shadow `0x9E04` leaves the list

The capture shows the T-rex still in its hit pose. The owner is **not
derived**. The candidates are the f = 93 camera anchor and the T-rex's f = 93
animation switch.
(Derived since, §16: both are one port error, `0x36870`'s case 0 restarting
the other side's record.)

## 16. The wrong record in `0x36870`'s case 0 at capture 866 (roar-timing Task 6, `2137bce`)

**Result in one line.** The f = 93 camera step and the T-rex's `0x96B5` are one
error, and it is the port's. At f = 93 the raptor's (side 1, char 3) own stream
reaches its `0x36870` opcode. `0x36870`'s `+0x54 == 0` arm restarts the calling
side's **own** record at `0xC8950[char]` and sets its `+0x4D = 0x1E`. The port
restarted the **other** side's record (`rec_o`). So the T-rex was put on the
raptor's stance stream `0xD2136` (sprite `0x16B5`, hflipped `0x96B5`), and its
`0x18540` anchor (sprite − `0xE6DD0`'s camera constant) fell out of range to 0.
That gave `AB0` −448 → 320 and the camera −500 → −256. The fix is two operands in
an already-ported function; the size gate does not apply. This supersedes
§15.5's "owner not derived" for 866.

### 16.1 The trace (measured, temporary, reverted)

The trace was `getenv("PR_T6")`-gated and has been reverted. It printed lines
from `actors_update` (both slots and records, f = 85..97) and from
`actors_anim_begin`/`actors_anim_seek` for either fighter record, with a
`backtrace()`. `git status` was clean afterwards.

| f | side 0 (T-rex, char 0) | side 1 (raptor, char 3) |
|---|---|---|
| 92 | `0x907E`, stream `0xE734C`, anchor 410, `AB0` −448 | `0x1764`, stream `0xD2316`, `+0x52/+0x53` = 9/8 |
| 93 (port) | **`0x96B5`, stream `0xD2136`**, anchor **0**, `AB0` **320** | `0x1764`, stream `0xD2326`, `+0x52/+0x53` = 0/0, `+0x5F` = `0xFF` |

The one `actors_anim_begin` at f = 93 was `rec = 0x2A7ECB0` (the T-rex),
`stream = 0xD2136`, hold 3.0. Its call chain was `game_loop` → `game_frame` →
`fight_arena_frame` → `fight_hud_pass` → `actor_sync` (the **raptor's**
record) → `frame_timer` → `spawn_anim_opcode` → `anim_indirect` →
`anim_code_36870` → `fighter_36870`. `0xC8950[3] = 0xD2136` (`read_memory
0xC8950`: `0xE6DD2, 0xE39D2, 0xECBDA, 0xD2136, …`), and `0xD2136`'s first word
is the literal `0x16B5`. So the stream is the raptor's own stance; only the
record was wrong.

### 16.2 What capture 866 shows (measured)

Capture 866 shows the T-rex still in its hit pose and the raptor changing
(montage of captures 865..868 against ports 509..512). The raptor's `0x36870`
restart is visible in the capture. The T-rex's switch and the camera step are
not.

### 16.3 The raw, re-read (Ghidra `disassemble_function 0x36870`; bytes by `read_memory` + capstone)

```
0x368e3 8b00           mov eax,[eax]            ; [S]  (EAX = [esp+8] = S)
0x368e5 89442410       mov [esp+0x10],eax       ; rec_s
0x368e9 8b02           mov eax,[edx]            ; [So] (EDX = So)
0x368eb 89442414       mov [esp+0x14],eax       ; rec_o: no later read
...
0x36a72 8b542410       mov edx,[esp+0x10]
0x36a76 8b442408       mov eax,[esp+0x8]
0x36a7a e8b9fbffff     call 0x36638             ; (S, rec_s)
0x36a7f 84c0 / 7536    test al,al / jnz 0x36ab9
0x36a83 8b442408       mov eax,[esp+0x8]
0x36a87 31d2           xor edx,edx
0x36a89 8a507a         mov dl,[eax+0x7a]        ; S+0x7A
0x36a8c 8b442410       mov eax,[esp+0x10]       ; rec_s
0x36a90 8b149550890c00 mov edx,[edx*4+0xc8950]
0x36a97 6800004040     push 0x40400000
0x36a9c e88f51ffff     call 0x2bc30             ; RET 4 pops the hold
0x36aa1 8b542410       mov edx,[esp+0x10]       ; rec_s again
0x36aa5 b81e000000     mov eax,0x1e
0x36aaa 88424d         mov [edx+0x4d],al
0x36aad..0x36ab5       S+0x52 = 0, S+0x53 = 0
```

The stack frame is `[esp] = side`, `[esp+4] = other`, `[esp+8] = S`,
`[esp+0xC] = So`, `[esp+0x10] = rec_s`, `[esp+0x14] = rec_o`. `[esp+0x14]` is
stored at `0x368EB` and never read. The rest of `0x36870` was re-diffed against
the port and matches: the jump table `0x3685C` (`read_memory`: `0x36A04`,
`0x36B02`, `0x36B8B`, `0x36BC1`, `0x36BB8`), the `0x365C8`/`0x36638`/`0x36BC8`/
`0x37D18` operands, the `0x36ADC..0x36AF6` `0x102900[rec_s+0x51]` restart with
`0xE906A` at 1.0, case 1's `0xC89A0` and case 2's `0xC89F0`.

### 16.4 The fix and its assertions

* **Fix** (`port/src/game/fighter.c`, `fighter_36870`). The case-0
  `actors_anim_begin` and the `+0x4D = 0x1E` store now use `rec_s`, with the
  raw's `0x36A8C`/`0x36AA1` reads in a comment. The unused `rec_o` local is
  gone (its `0x368E9` load is noted as dead), and the header comment is
  corrected.
* **Assertions** (`test_fight.c`, `check_deep_callees` case E). `fighter_36870`
  is called with side 1's record, `S+0x54 = 0`, mode 3, `S+0x7A = 3` and
  `So+0x7A = 0`. `0xC8950[3]` and `0xC8950[0]` point at distinct scratch
  streams (literal ids `0x0123` and `0x0456`); both are saved and restored,
  along with `0x100CE0`, `0x100AF8`, `0xFD148`, `0x107D20..0x107D2F` and
  `0x107A80..0x107AFF`. The checks:
  * `r1+8` is the char-3 stream, its pset is `0x0123`, `r1+0x4D = 0x1E`
    (seeded `0x55`)
  * `r0+8` keeps its sentinel `0xABCDEF`, its pset keeps `0x7777` and
    `r0+0x4D` keeps `0x55`
  * `S+0x52`/`+0x53` go from `0x66` to 0
* **Mutations** (each reverted by the script; `fighter.c` was compared
  byte-for-byte after the runs, and the suite then passed):

  | mutation | failures |
  |---|---|
  | pre-fix (`rec_o` for both) | 6: `:3166` `11259375 != 66271232`, `:3167`, `:3168` `85 != 30`, `:3169`, `:3170`, `:3171` |
  | `rec_o` for the begin only | 4: `:3166`, `:3167`, `:3169`, `:3170` |
  | `rec_o` for `+0x4D` only | 2: `:3168`, `:3171` |
  | `So+0x7A` for the table index | 2: `:3166` `66271248 != 66271232`, `:3167` `1110 != 291` |
  | `+0x4D = 0x14` | 1: `:3168` `20 != 30` |

### 16.5 Measured

| measurement | before (`364cb3d`) | after (`2137bce`) |
|---|---|---|
| capture 866 ↔ port 509/510 | best splice 27 858 B / 9 891 px | **0 B** (splice at byte 92 874, row 96) |
| port f = 93..96, side 0 | `0x96B5`, anchor 0, camera −256 | `0x907F`, `0x907F`, `0x9080`, `0x9080`; anchor 411/412 |
| demo oracle first unexplained | 866 (raw 3773); `[866..3616]` 2751 / 2745 unexpl. | **867 (raw 3774)**; `[867..3616]` 2750 / 2744 unexpl. |
| demo-fight ratchet | `[866..1884]` 1019, N = 866 | **`[867..1884]` 1018**, "ratchet improved: 867 > 866", **N raised to 867** |
| front-end oracle | `[560..865]` / 306 / 137 clean, 165 splice, 0 transition, 2 unexpl. (832, 833) | **`[560..866]` / 307 / 137 clean, 166 splice, 0 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved, which is the move the brief allowed.
These were unmoved:

* title `54/55/2/0` and `54/57/0/0`, determinism 54
* smk 120/120 and 41/41
* attract 215/216 (expected divergence at 215)
* C-vs-Python 9866
* `symbols.h`

The front-end "endpoints BAD" line is the same as before. The exhibition set
grows by one port frame (0..510, 274 exhibited).

**The new first unexplained frame, 867 (characterised, not fixed).** Capture
867 is a tear. Its best splice, port 510/511 at byte 119 388 (row 124), leaves
11 184 B / 3 955 px, all in x 185–319 and rows 125–194. That box is the
raptor's body and legs. The T-rex, the ground and the camera now match. Captures
868 and 869 keep the same box (14 505 B and 14 034 B), and 870 spreads over the
whole frame (30 389 B). The capture shows the raptor lowering out of its stance.
The port holds the stance sprite `0x16B5` from f = 93 through f = 96. No port
frame 509..514 matches that box at any horizontal shift in [−12, 12]; the best
is 3 955 px. Neither did the pre-fix port, whose raptor kept its old stream
(best 4 514 px). In the port, side 1's slot reads `+0x52 = 3`, `+0x53 = 4`,
`+0x54 = 2` from f = 94, and no animation start reaches its record until f = 97
(`0xD2140`, `0x16D2`). The owner is **not derived**. The candidates are:

* the side-1 `+0x52 == 3` / `+0x53 == 4` path at f = 94 (`0x3531C`'s dispatch
  and `0x35D7C`), which should start the raptor's next animation
* the command that set that state

The named gaps §10 lists are not implicated at 867: the camera and side 0's
anchor match the capture.
(Derived since, §17: `0x3BDDC`'s unported `0x3C480` call, plus `0x18714`'s
omitted `0x18540`/`0x18350` calls; the second is one of §10's named gaps after
all, reached here through `0x3C480`'s `0x188DC` tail.)

## 17. The raptor's crouch and its anchor at capture 867 (roar-timing Task 7, `a76414d`)

**Result in one line.** Capture 867 has two causes, both the port's. At f = 94
side 1's command reaches `0x3BDDC`, whose raw calls
`0x3C480(rec, [0xC8B30 + slot+0x7A·4], 1.0f)` (`0x3BEF7..0x3BF05`) before
storing state 3/4/2; the port had that call as a gap (§7.16 of the
demo-fight record), so the raptor held its stance `0x16B5`. With the call in,
the sprites are right (`0x1746` at f = 94, `0x1747` at f = 95) but 7 px left:
`0x3C480`'s `0x188DC` tail reaches `0x18714`, whose `0x18540`/`0x18350` calls
(§10's named gap "`hit_record_x/y` omit `0x18540`/`0x18350`") re-derive
`DS_00100AB0` for the new sprite. Both are small (one call; two calls in each
of two already-ported functions); the size gate does not apply. This
supersedes §16.5's "owner not derived" for 867.

### 17.1 The trace (measured, temporary, reverted)

`getenv("PR_T7")`-gated lines after `fight_hud_pass`'s `actor_sync` (slot
`+0x52/+0x53/+0x54`, `+0x5F`, the command word, `rec+8`, the pset sprite,
`rec+0x20/+0x24`, `slot+0x2C`, `rec+0x18`, pset x) and at `fighter_attack_consume`'s
transition. Reverted; `git status` was clean afterwards apart from the fix.

| f | side 1 (raptor, char 3), unmodified port (`980a52e`) |
|---|---|
| 93 | 0/0/0, stream `0xD2136`, `0x16B5`, `slot+0x2C` 5760, `rec+0x18` 6144 |
| 94 | **`0x3BDDC` fires**: `cmd = 0xA0A0`, ch 3, `0xC8B30[3] = 0xD2274`; 3/4/2, stream still `0xD2136`, `0x16B5` |
| 95..96 | 3/4/2, `0x16B5` (the stance's hold runs out) |
| 97 | 3/4/2, stream `0xD2140`, `0x16D2` (the stance stream's own next sprite) |

The caller is `0x349C8`'s `0x34A9F` (`+0x52 == 0`). `0x35D7C` (the
`+0x52 == 3` handler) was re-diffed against `disassemble_function 0x35D7C` and
matches the port.

### 17.2 What captures 867..869 show (measured)

A temporary render hook (reverted) decoded every char-3 sprite id
`0x1600..0x17FF` at the raptor's f = 94 and f = 95 screen position into a
scratch buffer (`sprite_blit_at`, sentinel 0x00 and 0xFF passes for the mask),
and a Python fit composited each over the port frame with the raptor removed
(`PR_T7F` forcing id 0, reverted), at every dx ∈ [−8, 8], dy ∈ [−6, 6], using
the palette read off the port's own raptor pixels.

* Capture 867, rows 125–194 (the half after the tear): best **`0x1746` at
  dx = +7, dy = 0**, 592 px left (palette entries the fit could not map; the
  next best is 2 450 px).
* Capture 868, rows 60–194 against f = 95: best **`0x1747` at dx = +7, dy = 0**,
  550 px; the next best is 2 640 px.

So the raw shows the crouch stream from f = 94 at one sprite per frame, and the
raptor 7 px (448 units) right of where the port draws it.

### 17.3 The raw, re-read (Ghidra, fixups applied)

`disassemble_function 0x3BDDC`:

```
0x3bee8 bbb0771000     mov ebx,0x1077b0
0x3bef0 01c3           add ebx,eax              ; the slot (EAX = side*0x94)
0x3bef4 8a537a         mov dl,[ebx+0x7a]        ; char
0x3bef7 8b03           mov eax,[ebx]            ; the slot's record
0x3bef9 8b1495308b0c00 mov edx,[edx*4+0xc8b30]  ; 0xC8B30[char]
0x3bf00 680000803f     push 0x3f800000          ; hold 1.0
0x3bf05 e876050000     call 0x3c480
0x3bf0a c6435203       mov byte [ebx+0x52],3
```

`read_memory 0xC8B30`: `0xE6F28, 0xE3AF0, 0xECCEC, 0xD2274, …`; `0xD2274`
reads `1746 1747 D000 5E04 0003 DA00 17D8 000D DC00 12D8 000D 8E40 …`. The
rest of `0x3BDDC` matches the port (the `0x3BE55` `slot+0x53` gate, the
`0x10782A` table index, the `0x107D40` store, the `+0x4E` arms).

`0x3C480` (port `hit_anim_start_a`) and `0x339AC` (`hit_anim_ctx`) match the
port: `0x188AC(side, rec+0x18, 0)` (`0x3C497 xor ebx,ebx`: `rec+0x1C = 0`),
`0x2BC30`, then `0x188DC(side, slot+0x2C)`. (The raw loads `slot+0x2C` at
`0x3C4B0`, before `0x2BC30`; the port reads it after. They differ only if
`0x2BC30`'s pre-walk runs an opcode that writes `slot+0x2C`; `0xD2274` starts
with a literal id, so nothing is pre-walked here. Not changed; noted.)

`disassemble_function 0x18714` (`0x18788` is the same with `+0x1C`, `0x1077E0`
and `0x100AB4`):

```
0x18729 f682f277100008 test byte [edx+0x1077f2],8   ; slot+0x42 bit 3
0x18730 740b           je 0x1873d
0x18732..0x1873b       eax = [[slot]+0x18]; jmp 0x18782
0x1873f e8fcfdffff     call 0x18540                 ; the anchor for the CURRENT sprite
0x1874b 8bb2d0771000   mov esi,[edx+0x1077d0]       ; slot+0x20
0x18751 8b88f00a1000   mov ecx,[eax+0x100af0]       ; DS_00100AF0[side]
0x18757 39f1 / 7409    cmp ecx,esi / je 0x18764
0x1875f e8ecfbffff     call 0x18350                 ; (side, anchor); slot+0x20 not stored
0x18772 8b3cddb00a1000 mov edi,[ebx*8+0x100ab0]
0x18779 8b0485dc771000 mov eax,[eax*4+0x1077dc]     ; slot+0x2C
0x18780 29f8           sub eax,edi
```

Ghidra `get_xrefs_to 0x18788` lists one caller, `0x18896` in `0x1883C`, which
latches both slots first (`0x186D0` stores the anchor into `slot+0x20`), so
`0x18788`'s two calls cannot change anything there. It is transcribed as the
raw has it, and no assertion can observe it.

Char-3 anchor data (`read_memory`): `word[0xD2134] = 0x16B5`,
`0xE6DB4[3] = 0x2AE`; `0xD033B + 2·0` = `fc 30` (x −4, y 48) for `0x16B5`;
`0xD033B + 2·145` = `f5 32` (x −11, y 50) for `0x1746` and `0x1747`. So
`0x188AC`'s latch gives `slot+0x2C = 6144 − 256 = 5888`, and `0x18714` returns
`5888 + 704 = 6592`: +448 units, the capture's 7 px.

### 17.4 The fix and its assertions

* **Fix** (`port/src/game/fighter.c`).
  * `fighter_attack_consume` calls
    `hit_anim_start_a(DSD(self), DSD(0xC8B30 + ch·4), 0x3F800000)` at the raw
    position, before the 3/4/2 stores (`FIGHT_ANIM_3BDDC`). The header comments
    here and in `fighter.h` no longer name the call as a gap.
  * `hit_record_x`/`hit_record_y` call `fighter_18540(side)` and, when
    `DS_00100AF0[side] != slot+0x20`, `fighter_18350(side, anchor)`, as the raw
    does. The `PORT:` omission comments are gone; `camera.c`'s split-arm gap
    comments no longer call `0x18714` unported (they still skip its call).
* **Assertions** (`test_fight.c`, `check_deep_callees` cases F and G). Both
  save and restore `0xC8B30[0]`, `0xC8B30[3]`, `DS_00100AF0..+7`,
  `DS_00100AB0..+0xF`, `DS_00107D40..+7` and `DS_001078F8..+1`.
  * F: side 1, char 3, `S+0x53 = 1` (skips the `0x3CF38` chain at `0x3BE55`),
    command `0x8000`, pset `0x16B5`, `rec+0x18 = 6144`, `rec+0x1C = 0x5555`,
    `S+0x20 = 0x77`; `0xC8B30[3]` and `0xC8B30[0]` point at scratch streams
    with literals `0x1746` and `0x0456`. Checks: `rec+8` is the char-3 stream,
    pset `0x1746`, `rec+0x24 = 1.0f`, `rec+0x1C = 0`, `S+0x2C = 5888`,
    `rec+0x18 = 6592`, `DS_00100AF0[1] = 145`, `DS_00100AB0[1] = −704`,
    `DS_00100AB4[1] = 3200`, `S+0x20 = 0` (the latch's, not `0x18714`'s),
    side 0's record/pset/`AF0`/`AB0` sentinels unchanged, `S+0x52 = 3`.
  * G: pset `0x96B5` (hflipped stance, anchor 0: the latch's `0x18350`
    negates x, `AB0 = +256`), stream literal `0x16B5` (anchor 0 again). The
    anchor equals the latched `slot+0x20`, so `0x18757` skips `0x18350`:
    `S+0x2C = 6400`, `AB0[1] = +256`, `rec+0x18 = 6144`.
* **Mutations** (each applied by a script, `fighter.c` restored and compared
  byte for byte afterwards, the suite then passed):

  | mutation | failures |
  |---|---|
  | no `0x3C480` call (pre-fix) | ≥ 8, first `:3227` `11259375 != 66271232` |
  | `0xC8B30` indexed by the other slot's `+0x7A` | 7, first `:3227` `66271248 != 66271232` |
  | the other slot's record | ≥ 8, first `:3227` |
  | hold 3.0 | 1: `:3229` `1077936128 != 1065353216` |
  | `0x18714` without `0x18540`/`0x18350` (pre-fix) | 4: `:3232` `6144 != 6592`, `:3233`, `:3234`, `:3235` |
  | `0x18714` without `0x18350` | 3: `:3232`, `:3234`, `:3235` |
  | `0x18714` always calls `0x18350` | 3: `:3263` `-256 != 256`, `:3264` `6656 != 6144`, `:3629` (§7.3's pose snap, `17505 != 13089`) |
  | `0x18714` stores `slot+0x20` | 1: `:3236` `145 != 0` |
  | `0x18788` without `0x18540`/`0x18350` | 0 (inert through `0x1883C`, above) |

### 17.5 Measured

| measurement | before (`980a52e`) | after (`a76414d`) |
|---|---|---|
| capture 867 ↔ port 510/511 | 11 184 B / 3 955 px | **0 B** |
| capture 867, `0x3C480` call alone | — | 9 597 B / 3 380 px (the 7 px offset) |
| captures 868, 869 | 14 505 B, 14 034 B | **0 B** (511/512 splice; port 512) |
| port f = 94..96, side 1 | `0x16B5`, `rec+0x18` 6144 | `0x1746`, `0x1747`, `0x174E`; `rec+0x18` 6592 |
| demo oracle first unexplained | 867 (raw 3774); `[867..3616]` 2750 / 2744 unexpl. | **870 (raw 3777)**; `[870..3616]` 2747 / 2741 unexpl. |
| demo-fight ratchet | `[867..1884]` 1018, N = 867 | **`[870..1884]` 1015**, "ratchet improved: 870 > 867", **N = 870** |
| front-end oracle | `[560..866]` / 307 / 137 clean, 166 splice, 0 transition, 2 unexpl. (832, 833) | **`[560..869]` / 310 / 138 clean, 168 splice, 0 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved, which is the move the brief allowed.
These were unmoved:

* title `54/55/2/0` and `54/57/0/0`, determinism 54
* smk 120/120 and 41/41
* attract 215/216 (expected divergence at 215)
* C-vs-Python 9866
* `symbols.h`

The front-end "endpoints BAD" line is the same as before. The exhibition set
grows to port frames 0..512 (276 exhibited).

**The new first unexplained frame, 870 (characterised, not fixed).** Capture
870 is a tear. Its best splice, port 512/513 (split at row 49), leaves
12 025 B / 4 306 px. Of these, 3 942 px lie in x 240–319, rows 28–197: the
raptor, which the capture shows in a different pose from the port's `0x174E`
(against port 511..515 the box leaves 7 495, 7 369, 3 951, 4 031 and
4 100 px). The rest is 285 px around a worshipper (x 92–239, rows 138–197) and
79 px at the left edge (x 0–44). Capture 871 differs across the frame
(30 449 px). In the port, the raptor's stream advances at f = 96 from `0x1747`
(`0xD2276`) through `D000 5E04 0003`, `DA00 17D8 000D`, `DC00 12D8 000D`,
`8E40`, `FF20 0040 0000 1000`, `FF21 0040 0000 2000` and `ED40 22B0 000D` to
`0x174E` (the word at `0xD22B0`) with hold 2, after which `rec+8` reads
`0xD22A0`. The owner is **not derived**. The candidates are those opcodes'
handlers (`D000` with the dword `0x00035E04`, where Ghidra has no function,
the `DA00`/`DC00` pair, the `FF20`/`FF21`
command tests against side 1's `0xA0A0`, and the `ED40` branch) and the
raptor's position from f = 96 (`slot+0x2C` 5888 → 6528 at f = 97).
