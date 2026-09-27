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
`0x3EA24`, `0x44798`, `0x4505C` and `0x48AAC` (`0x37B54` is wrong, the
raw wins: it writes only `[[slot]+0x53]`; the `+0x42` write in that range is
`0x37CE2`, in `0x37CD4`; §46-D.6). The ported `0x3C148`,
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
(Derived since, §18: `D000 5E04 0003` is the one cause. Its target
`0x35E04` was unregistered, so the port's dispatch skipped the raptor's
launch. The `FF20`/`FF21` words are not command tests. They are the `0x1F`
prefix with opcodes `0x20`/`0x21`, the `rec+0x18`/`rec+0x1C` adds from the
`0xD17D8` table. The f = 97 in "5888 → 6528 at f = 97" is where Task 7's probe
first saw the value. The latch writes it during f = 96, §18.1.)

## 18. The raptor's launch at capture 870 (roar-timing Task 8, `0a8346b`)

**Result in one line.** Capture 870 has one cause, and it is the port's. The
raptor's attack stream `0xD2274` reaches `D000 5E04 0003` at `0xD2278` at
f = 96. That is opcode `0x10`, mode `0x4000`, with the inline dword
`0x00035E04`. The port had no function registered at `0x35E04`, so
`anim_indirect`'s `fn_resolve` miss skipped the call. The raw `0x35E04` sets
hold 3.0 and calls `0x3BC70`, which launches the raptor: state 4/0/2, gravity
23, vertical speed 550, horizontal speed −150. So the capture shows the raptor
in the air, and the port drew it on the ground. This takes two small functions
(`0x35E04`, 60 B; `0x3BC70`, 112 B; `0x3BC70`'s one caller is `0x35E21`), so
the size gate does not apply. This supersedes §17.5's "owner not derived" for
870.

### 18.1 The trace (measured, temporary, reverted)

The trace was `getenv("PR_T8")`-gated and has been reverted:
- a line per side at the end of `fight_hud_pass`, after the `0x186C4` latch, with slot `+0x52/+0x53/+0x54`, `+0x4E`, `rec+8`, the pset id, `rec+0x20/+0x24`, `rec+0x18/+0x1C`, `rec+0x34/+0x36/+0x44` and `slot+0x2C/+0x30`
- a line per `spawn_anim_opcode` call for side 1's record
- a line per `anim_indirect` call with its target and whether it resolved

The arena-frame counter was offset from f by 65. It was aligned on the first
`0x1746` (f = 94, §17.1).

| f | side 1 (raptor), unmodified port (`d47de53`) |
|---|---|
| 95 | 3/4/2, `rec+8 = 0xD2276`, `0x1747`, `rec+0x18` 6592, `rec+0x1C` 0, `slot+0x2C` 5888 |
| 96 | the walk runs `D000` (**`anim_indirect` target `0x35E04`, not resolved**), then `DA00`, `DC00`, `8E40`, `FF20`, `FF21`, `ED40`; 3/4/2, `0x174E`, `rec+0x18` 6592, `rec+0x1C` 5888, speeds 0/0/0, `slot+0x2C` 6528 |
| 97..100 | 3/4/2, `0x174E`, position unchanged |
| 101 | `0x174F`, `rec+0x18` 6720 |

`slot+0x2C` becomes 6528 during f = 96. The `0x186C4` latch at the end of
`fight_hud_pass` writes it (`0x35829`). A probe placed before the latch sees it
first at f = 97. This reconciles §17.5 ("5888 → 6528 at f = 97") with the
Task 7 report ("from f = 97"). The change belongs to f = 96.

With the fix (same probe):

| f | side 1 | `rec+0x18` | `rec+0x1C` | speeds `+0x34/+0x36/+0x44` |
|---|---|---|---|---|
| 96 | **4/0/2**, `0x174E` | 6442 | 6438 | −150 / 527 / 23 |
| 97 | 4/0/2, `0x174E` | 6292 | 6965 | −150 / 504 / 23 |
| 100 | 4/0/2, `0x174E` | 5842 | 8408 | −150 / 435 / 23 |
| 101 | 4/0/2, `0x174F` | 5820 | 8843 | −150 / 412 / 23 |

- At f = 96, `rec+0x1C` = 5888 + 550: one `0x2A4FC` step after the launch. `rec+0x18` = 6592 − 150.
- At f = 101, `FF20`'s `0xD17D8[2]` adds +2·64.
- Side 0 hits `0x35E04` too, on its own attack stream at f = 109.
- No other unresolved `anim_indirect` target fires before f = 165 (`0x35938`, side 1).

### 18.2 What captures 870..879 show (measured)

A splice search took every port pair (p, p+1) for p in 505..529. The top came
from p and the bottom from p+1, split at any pixel. The search ran against
the fixed port's dump:

- 870 = port 513 (f = 96), whole, 0 px
- 871..875 = the splices 513/514 … 517/518, 0 px
- 876 = port 518
- 877..879 = the splices 518/519 … 520/521, 0 px

So the capture shows the raptor launched at f = 96 and in the air from then on.
It draws `0x174E` from `rec+0x18` 6442 and `rec+0x1C` 6438. Before the fix,
capture 870 left 12 025 B / 4 306 px (§17.5).

### 18.3 The raw, re-read (Ghidra `read_memory` + capstone; `disassemble_function 0x3BC70`)

The stream (`read_memory 0xD2274`):

```
D2274 1746 1747
D2278 D000 5E04 0003      op 0x10, mode 0x4000: DS_00105BD4 = 0x00035E04; call it
D227E DA00 17D8 000D      op 0x1A: rec+0x0C = 0xD17D8 (00 5c 02 00 00 00 ff 00 ...)
D2284 DC00 12D8 000D      op 0x1C: rec+0x10 = 0xD12D8 (02 02 02 03 03 04 05 04 ...)
D228A 8E40                op 0x0E: var 0x40 = 0
D228C FF20 0040 0000 1000 0x1F prefix, op 0x20: rec+0x18 += (s8)[rec+0x0C + 2*var40]*64
D2294 FF21 0040 0000 2000 0x1F prefix, op 0x21: rec+0x1C += (s8)[rec+0x0C + 2*var40+1]*64
D229C ED40 22B0 000D      id = word[0xD22B0 + 2*var40]
D22A2 B840 000F 228C 000D op 0x18: var40++, loop to 0xD228C while 15 > var40
D22AA 9B00 9E00 8100
```

`0x35E04` (no Ghidra function; bytes `53 52 8b 50 14 85 d2 74 30 …`; Ghidra
`get_xrefs_to 0x35E04`: none, since it is reached only through the stream
dword):

```
0x35e04 53 / 52              push ebx / push edx
0x35e06 8b5014               mov edx,[eax+0x14]         ; the owner slot
0x35e09 85d2 / 7430          test edx,edx / je 0x35e3d
0x35e0d c7402000004040       mov dword [eax+0x20],0x40400000   ; 3.0f
0x35e14 31db                 xor ebx,ebx
0x35e16 d94020               fld dword [eax+0x20]
0x35e19 8a5851               mov bl,[eax+0x51]          ; side
0x35e1c d95824               fstp dword [eax+0x24]
0x35e1f 89d8                 mov eax,ebx
0x35e21 e84a5e0000           call 0x3bc70
0x35e26 31c0 / 8a427a        xor eax,eax / mov al,[edx+0x7a]
0x35e2b 668b0445a8da0b00     mov ax,[eax*2+0xbdaa8]     ; voice id (0x49 for char 3)
0x35e33 25ffff0000           and eax,0xffff
0x35e38 e8bf65ffff           call 0x2c3fc               ; voice
0x35e3d 5a / 5b / c3
```

`0x3BC70` (`get_xrefs_to 0x3BC70`: one caller, `0x35E21`):

```
0x3bc74..0x3bc83   eax = 0x1077B0 + side*0x94      ; the slot
0x3bc88  mov ebx,[ebx*4+0x107d40]                   ; the row 0x3BDDC stored
0x3bc8f  mov byte [eax+0x54],2
0x3bc93  mov byte [eax+0x52],4
0x3bc97  mov byte [eax+0x53],0
0x3bc9b  mov edx,[eax]                              ; the record
0x3bc9d..0x3bca0  [edx+0x44] = word [ebx]           ; gravity
0x3bca4..0x3bca8  [edx+0x36] = word [ebx+2]         ; vertical speed
0x3bcac  mov cx,[eax+0x4e] / test cx,cx / jle 0x3bcc2
0x3bcb5..0x3bcb9  [edx+0x34] = word [ebx+4]          ; +0x4E > 0
0x3bcc2  jge 0x3bcd5
0x3bcc4..0x3bccc  [edx+0x34] = -word [ebx+4]         ; +0x4E < 0 (neg edi)
0x3bcd5  mov word [edx+0x34],0                       ; +0x4E == 0
```

The row is 3 words at `0xBEF28` or `0xBEF64` + char·6 (`0x3BED4`). For
char 3 they read (23, 550, 150) and (35, 700, 336). The demo's row is
0xBEF28's, whose gravity is 23 and vertical speed 550 (§18.1).

### 18.4 The fix and its assertions

* **Fix.**
  * `port/src/game/fighter.c`: `fighter_35e04` (`0x35E04`) and the static `fighter_3bc70` (`0x3BC70`), transcribed as above. The `0x2C3FC` voice call is a `PORT:` out-of-scope note (spec §7), as at the other voice sites.
  * `port/src/game/actors.c`: the `(rec, arg)` wrapper `anim_code_35E04` is registered with `fn_register(0x35E04u, …)`, as `0x36870`'s wrapper is.
* **Assertions** (`test_fight.c`).
  * `check_deep_callees` case H calls `fighter_35e04(r1)` on the scratch row (23, 550, 150). Seeded values: `rec+0x20`/`+0x24` `0x11111111`/`0x22222222`, speeds `0x5555`, slot bytes `0x66`, side 0's `+0x52`/`+0x44` sentinels. Checks: 3.0f in both holds, slot 2/4/0, `+0x44` 23, `+0x36` 550, `+0x34` −150 with `+0x4E` = −1, +150 with +1, 0 with 0; side 0 untouched; with `rec+0x14 = 0`, nothing written.
  * `check_anim_hold_scaler` checks the registration (non-NULL and not the bare function). It then walks a scratch stream `D000 5E04 0003 1746` through `actors_anim_begin` at 1.0f and checks `rec+0x20` = 3.0f, slot `+0x52` = 4, `rec+0x36` = 550 and pset `0x1746`.
  * Both restore `DS_00107D40` (8 B). The second also restores slot 0 (0x94 B), and case H restores `s1+0x4E`.
* **Mutations.** A script applied each one. `fighter.c` and `actors.c` were restored and compared byte for byte, and the suite then passed.

  | mutation | failures |
  |---|---|
  | `0x35E04` unregistered (pre-fix) | 4: `:3772` registration, `:3834` `1065353216 != 1077936128`, `:3835` `102 != 4`, `:3836` |
  | no `0x3BC70` call | 10 |
  | `0x3BC70(0)` instead of `rec+0x51` | 10 |
  | no `rec+0x14` gate | 3 |
  | hold 2.0f | 3 |
  | `rec+0x24` not copied | 1 |
  | no negation for `+0x4E` < 0 | 1 (`150 != -150`) |
  | `+0x4E` == 0 keeps the row | 1 |
  | `+0x44`/`+0x36` swapped | 3 |
  | `+0x53` = 4 | 1 |
  | `+0x54` = 1 | 1 |
  | `+0x4E` read as the byte `+0x4F` | 1 |

### 18.5 Measured

| measurement | before (`d47de53`) | after (`0a8346b`) |
|---|---|---|
| capture 870 | best 512/513 splice, 12 025 B / 4 306 px | **0 B** (port 513) |
| captures 871..879 | (not reached) | **0 B** (513/514 … 520/521 splices; 876 is port 518) |
| port f = 96..101, side 1 | 3/4/2, `0x174E`/`0x174F` at `rec+0x18` 6592/6720, `rec+0x1C` 5888 | 4/0/2, `rec+0x18` 6442 → 5820, `rec+0x1C` 6438 → 8843 |
| demo oracle first unexplained | 870 (raw 3777); `[870..3616]` 2747 / 2741 unexpl. | **880 (raw 3787)**; `[880..3616]` 2737 / 2731 unexpl.; demo port frames `[522..1380]` |
| demo-fight ratchet | `[870..1884]` 1015, N = 870 | **`[880..1884]` 1005**, "ratchet improved: 880 > 870", **N = 880** |
| front-end oracle | `[560..869]` / 310 / 138 clean, 168 splice, 0 transition, 2 unexpl. (832, 833) | **`[560..879]` / 320 / 140 clean, 176 splice, 0 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved, which is the move the brief allowed.
These were unmoved:

* title `54/55/2/0` and `54/57/0/0`, determinism 54
* smk 120/120 and 41/41
* attract 215/216 (expected divergence at 215)
* C-vs-Python 9866
* `symbols.h`

The front-end "endpoints BAD" line is the same as before. The exhibition set
grows to port frames 0..521 (285 exhibited). The ladder
(`cmake --build build && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify
&& make demo-oracle`) exited 0. The compiler gave 0 warnings.

**The new first unexplained frame, 880 (characterised, not fixed).** Capture
880 is a tear. Its best splice, port 521/522 (split at row 101), leaves
6 073 px, all in x 0–149, rows 118–199. That is the T-rex (side 0). The capture
shows it lowered, head and body down toward the ground line. The port draws it
upright, sprite `0x8F35`. Captures 881..884 leave 8 207, 13 681, 12 517 and
12 590 px and spread across the frame. Here is side 0 in the port's trace
(§18.1's probe):

* f = 97..100: the stance, 0/0/0, stream `0xE6DD2`, `0x8F02`
* f = 101: state 5/0/1, stream `0xE6DEA`, `0x8F34`
* f = 104: state 9/0/0, `0x8F35`
* f = 107: state 3/4/2 on its own attack stream `0xC8B30[0] = 0xE6F28`, `0x8FA1`, which reaches `0x35E04` at f = 109

No unresolved `anim_indirect` target fires on side 0 in f = 96..107. The owner
is **not derived**. The candidates are:

* side 0's f = 101 and f = 104 transitions (their `+0x52` writers and animation starts)
* the command that side 0 receives while the raptor is in the air

(Derived since, §19: neither candidate. Side 0's f = 101 crouch and f = 104
stand-up match the raw. At f = 105 the T-rex's `0x3CF38` chain drives reaction
`0x2B`, whose `0x34E2C` entry holds only the callback `0x3E62C`. The port
skipped that callback call.)

## 19. The T-rex's reaction-0x2B leap at capture 880 (roar-timing Task 9, `cfff063`)

**Result in one line.** Capture 880 has one cause, and it is the port's. At
f = 105 the T-rex's `0x350D0` → `0x3CF38` chain finds hit-scan index 5 and
drives reaction `0x2B` (`word[0xC61C8]` = `0x2B`) into `0x34E2C`. The
(char 0, `0x2B`) entry at `0xA3884` has no stream, only the callback
`*(u32*)0xA3884 = 0x0003E62C`, and the port skipped `0x34E2C`'s callback call
at `0x35045`. The raw `0x3E62C` starts the `0xE7BDE` stream (`0x11B3`…, the
lowered pose), puts the slot in state 9/7/2 with `+0x57 = 2`, and arms the
`+0x0C` callback `0x3E524`, which `0x3531C` case 7 then runs every frame. The
stream's `D500 E4E4 0003` reaches `0x3E4E4`, the leap. So the capture shows the
T-rex lowering and then leaping, and the port stood it up. The closure is the
callback call, `0x3E62C` (106 B), `0x3E524` (263 B), `0x3C190` (53 B),
`0x3E4E4` (47 B) and three registrations. The size gate does not apply. This
supersedes §18.5's "owner not derived" for 880.

### 19.1 The trace (measured, temporary, reverted)

The trace was `getenv("PR_T9")`-gated and has been reverted. `git status` was
clean afterwards apart from the fix. It had these parts:

- a line per side at the end of `fight_hud_pass`, after the `0x186C4` latch. It printed slot `+0x52/+0x53/+0x54`, `+0x43`, `+0x40`, `+0x5F`, the command word, `rec+8`, the pset id, `rec+0x20/+0x24`, `rec+0x18/+0x1C`, the speeds, `slot+0x2C/+0x30` and the side's AI block (`DS_001081F0` + side·0x40: state, active flag, move id, cursor, timer, step pointer).
- a line per `0x46F4C` pick
- a line per `0x34E2C` call, with the anim triple, the stream word and the callback dword
- later, one line per `hit_scan` result and per `0x1975C` gate (for 891)

The frame counter `f` is `DS_0010150C` itself, the same `f` §17/§18 use.

| f | side 0 (T-rex), unmodified port (`38b3e74`) |
|---|---|
| 97..100 | 0/0/0 stance `0xE6DD2`, `0x8F02`; AI move 61 (`0x8A4D4`: `0000`×1, `FFFF`), command 0 |
| 101 | AI move 17 (`0x8A73C`: `4040`×2, `0980`×2, `FFFF`); command `0x4040` → `0x349C8` crouch, 5/0/1, `0xE6DEA`, `0x8F34` |
| 104 | command `0x0980`: `0x36430` (neither `0x40` nor `0x80` in the high byte) starts `0xC89C8` → 9/0/0, `0xE6E30`, `0x8F35` |
| 105 | **`0x34E2C(0, 0x2B)`**: anim `0xDE2ED/0xA3884/0xA682A`, stream word 0, **callback `0x3E62C`**; the port stores `+0x5F = 0x2B` and does nothing else; 9/0/0, `0x8F35` |
| 106 | 9/0/0, `0x8F34` (the stand-up stream) |
| 107 | AI move 50 (`0xA0A0`): `0x3BDDC` → 3/4/2, `0xE6F28`, `0x8FA1` |

`0x36430` (`disassemble_function 0x36430`) matches the port, so f = 104's
stand-up is the raw's too. At f = 105 the path is `0x3531C` default →
`0x350D0`. The high byte `0x09` of `0x0980` has bits in both `&3` and `&0xC`,
so `bvar2` skips `0x3BDDC`. `0x3CF38`'s `hit_scan` then returns 5, and
`0x3CE58` reads the reaction `word[0xC619C + (0·0x20 + 5)·8 + 4]`.
`read_memory 0xC61C4` gives `1c 00 0c 00 2b 00 01 00`, so the reaction is
`0x2B`.

With the fix, the same probe gives:

| f | side 0 |
|---|---|
| 105 | **9/7/2**, `rec+8 = 0xE7BE6`, `0x91B3`, `rec+0x1C` 0, `slot+0x2C` −6512 |
| 106..111 | 9/7/2, `0x91B4` … `0x91B7` |
| 112 | `rec+8 = 0xE7C1A`, `0x91B8`, speeds 0/765/35, `rec+0x1C` 6880 |
| 113 | speeds 1092/730/35: the `+0x57 = 0` arm's 10·765/7 = 1092 (truncated), positive because the pset is hflipped; at 114, 10·730/7 = 1042 |
| 118 | `0x91B9`, `rec+0x18` 4794, `rec+0x1C` 10945 |

The sprite ids have the hflip bit set (`0x91B3` is `0x11B3`).

### 19.2 What captures 880..890 show (measured)

A splice search took every port pair (p, p+1) for p in 515..554. The top came
from p and the bottom from p+1, split at any pixel. The search ran against
the fixed port's dump:

- 880 = the 521/522 splice (row 101), 0 px
- 881, 882 = 522/523 and 523/524, 0 px
- 883 = port 524, whole
- 884..889 = the 524/525 … 529/530 splices, 0 px
- 890 = port 530, whole

Before the fix, 880 left 6 073 px (x 0–149, rows 118–199). Matching the
T-rex's box, rows 110–199 and x 0–149, against any port frame 515..544 left
at least 6 073 px for every capture 880..891. No pose in the unfixed port
matched.

### 19.3 The raw, re-read (Ghidra `disassemble_function` / `read_memory` + capstone)

**`0x34E2C`'s callback call.** `disassemble_function 0x34E2C`:

```
0x34f71  mov eax,[esp+0x1c] / mov eax,[eax] / mov [esp+0x24],eax  ; *(u32*)anim[1]
0x35012  cmp dword [esp+0x24],0 / je 0x35049
0x35019..0x35032  the 0xE9308 voice through 0x2C3FC
0x35037  mov eax,[esp+0x8]      ; the slot (0x1077B0 + side*0x94)
0x3503b  mov ebx,[esp]          ; side
0x3503e  mov edx,[esp+0x10]     ; the slot's record
0x35042  mov [eax+0x5f],cl      ; +0x5F = reaction
0x35045  call dword [esp+0x24]
```

`read_memory 0xA3884` gives `2c e6 03 00 00 00 00 00 cc 8c 0e 00 56 8e 0e 00`.
So the callback is `0x3E62C` and the stream pointer is 0.

**`0x3E62C`.** It has no Ghidra xrefs; the table dword is its only reference.
`read_memory 0x3E62C`, 106 B:

```
0x3e632  mov ebx,eax ; mov ecx,edx       ; EBX = slot (the caller's EBX is lost), ECX = rec
0x3e638  call 0x339ac                    ; ctx from rec
0x3e63d  push 0x40400000
0x3e642  mov esi,[esp+0xc]               ; ctx[2], the slot
0x3e646  mov edx,0xe7bde
0x3e64d  mov esi,[esi+0x2c]              ; x, read before the call
0x3e650  call 0x3c4cc                    ; (rec, 0xE7BDE, 3.0)
0x3e655  mov eax,[esp] ; mov edx,esi ; call 0x188dc    ; (ctx[0], x)
0x3e65f  mov byte [ebx+0x57],2 ; [ebx+0x52],9 ; [ebx+0x53],7 ; [ebx+0x54],2
0x3e66f  mov dword [ebx+0xc],0x3e524 ; [ebx+0x18],0x3e484 ; [ebx+0x1c],0x3e4c4
0x3e687  or ah,0x80 -> [ebx+0x41]
0x3e68d  mov al,1 ; ret
```

**`0x3531C` case 7.** The `+0x0C` call is at `0x35431`:
`mov eax,ecx ; call [ecx+0xc]`. Here ECX is the slot from
`DS_001077A8[side]`, EDX is still `[ecx]` (loaded at `0x35396`), and EBX is the
side (`0x35325`).

**`0x3E524`** (`get_function_callees`: `0x33950`, `0x3C190`, `0x188AC`,
`0x3C148`, `0x3C16C`, `0x3C4CC`, `0x62003`). Its jump table is at `0x3E514`;
`read_memory` gives `6d e5 03 00 ba e5 03 00 24 e6 03 00 24 e6 03 00`.

```
0x3e534  call 0x33950 (side)
0x3e53d  if byte [ctx[4]+0x63] >= 10: byte [ctx[2]+0x8a] = 0
0x3e555  switch byte [slot+0x57] (> 3 -> 0x62003)
case 0 (0x3e56d): edx = [rec+0x34] >> 16 ; edx = 10*edx ; idiv 7 ; 0x3C190(side, q)
                  if word [rec+0x36] >= 0: return
                  slot+0x57 = 1 ; word [[slot]+0x44] = 0xF ; 0x3C190(side, 0)
case 1 (0x3e5ba): if (dword [0xBD882 + char*2] >> 16) > slot+0x30: land (jg)
                  elif word [rec+0x36] != 0: return
                  land: slot+0x54 = 0 ; 0x188AC(side, [[slot]+0x18], 0) ; 0x3C148 ; 0x3C16C
                        0x3C4CC([slot], [0xC8B58 + char*4], 3.0) ; slot+0x57 = 3
case 2, 3 (0x3e624): return
```

**`0x3C190`:** `0x1A570(side)` is 1 when the pset is not hflipped. In that case
the value is negated (`0x3C1B4 neg edx`). Then `word [rec+0x34] = dx`.

**`0x3E4E4`:** `read_memory 0xE7BDE` gives this stream:

```
DC00 55B8 000E   CD40 11B3   B840 0005 7BE4 000E   9E00
D500 E4E4 0003   8E40 DA00 5FD8 000E DC00 55D8 000E FF20 ... ED40 7C2A 000E ...
```

`D500 E4E4 0003` is opcode `0x15`, mode `0x4000`, with the dword `0x0003E4E4`.
`0x3E4E4` checks `ecx = [eax+0x14]`, then calls
`0x2BC30(rec, 0xE7BFA, 3.0)`. `0xE7BFA` is the word after the opcode. It then
stores `word [rec+0x36] = 0x320`, `word [rec+0x44] = 0x23` and
`byte [ecx+0x57] = 0`.

**`0x3E484`/`0x3E4C4`.** (Corrected in §19.6; the first version of this
paragraph said their only callers were the `0x1975C` think chain, which is
wrong.) `get_function_callers` gives `0x1958C` (the port's `fighter_pass_a`)
as the only caller of both `0x19020` and `0x193B0`. `0x19020` calls
`[slot+0x18]` at `0x1903F`, and `0x193B0` calls `[slot+0x1C]` at `0x19505`.
`0x3E4C4` is ported in §19.6. `0x3E484` stays a named gap there, with
evidence that it is inert in the demo.

### 19.4 The fix and its assertions

* **Fix.**
  * `port/src/game/fighter.c`:
    * `hit_reaction_apply` calls the callback through `fn_resolve` with `(slot, rec, side)`. A new `fighter_slot_cb` typedef carries a `PORT:` note on the register shape.
    * `fighter_state_3531c` case 7 calls `+0x0C` with the same shape. It called it with no arguments before, and no registered target existed.
    * `fighter_3e62c`, `fighter_3e524`, `fighter_3e4e4` and the static `fighter_3c190` are transcribed as above.
    * The `0xE7BDE`/`0xE7BFA` stream addresses are local `#define`s.
  * `port/src/game/actors.c`:
    * `0x3E62C` and `0x3E524` are registered as the bare functions, because the call sites use the register shape.
    * `0x3E4E4` is registered through the `(rec, arg)` wrapper `anim_code_3E4E4`.
  * The review nit in `fighter_35e04`'s voice note now reads `word[0xBDAA8 + byte[slot+0x7A]*2]`, with slot = `rec+0x14`.
* **Assertions** (`test_fight.c`):
  * `check_anim_hold_scaler` checks the three registrations. `0x3E4E4` must go through the wrapper; `0x3E62C` and `0x3E524` must be the functions themselves.
  * The new `check_trex_leap` has four parts:
    * **A** drives `hit_reaction_apply(0, 0x2B)` on the real `0xA3884` entry. The slot is in state 9/0/0, and `+0x42` bit 3 makes the x path pass `rec+0x18` through. The seeded slot x is `0x1111` and the record's is `0x2222`, so the value `0x3E62C` reads first differs from the one the `0x3C480` latch leaves. It checks:
      * `+0x5F` = `0x2B`, `+0x57` = 2, state 9/7/2
      * the three callback dwords and `+0x41` bit 7
      * `slot+0x2C` = `0x1111`, `rec+0x18` = `0x2222`
      * `rec+0x1C` = 0 (the `0x3C480` arm)
      * `rec+0x10` = `0xE55B8`
      * the hold 1.0f (`byte[0xE55B8]` = 1 replaces the 3.0 argument)
      * pset `0x11B3`, `rec+8` inside `0xE7BDE`
      * side 1 untouched
    * **B** runs `fighter_state_3531c(0)` with `+0x53 = 7`. It checks:
      * `rec+0x34` = −1001 (10·701/7, truncated, negated while unflipped)
      * `+0x57` kept at 0
      * `+0x8A` cleared by `rec+0x63` = 10
      * `+0x43` bits `0x30` cleared
      * when flipped, +1001, and `+0x63` = 9 keeps `+0x8A`
      * when falling, `+0x57` = 1, gravity 15, `+0x34` = 0
    * **C** checks the landing gate `0x1600` > `slot+0x30`:
      * 6000 does not land, and neither does the equal value 5632 (`jg`)
      * speed 0 lands, and 5631 lands
      * landing zeroes `+0x54`, `rec+0x1C`, `+0x43`, `+0x34` and `+0x44`
      * it gives hold 3.0f and pset `0x0FA7` (`0xC8B58[0]` = `0xE6F82`) and sets `+0x57` = 3
      * `+0x57` = 3 and 2 return with nothing written
    * **D** walks the crafted stream `D500 E4E4 0003 1746`. It checks:
      * speed `0x320`, gravity `0x23`, `+0x57` = 0
      * `rec+0x0C`/`+0x10` = `0xE5FD8`/`0xE55D8`
      * hold 5.0f (`byte[0xE55D8]` = 5)
      * `rec+8` inside `0xE7BFA`
      * without `rec+0x14`, nothing is written
* **Mutations.** A script applied each one. `fighter.c` and `actors.c` were restored and compared byte for byte (`cmp` OK), and the suite then passed. Counts are real `FAIL` lines, without the `FAILURES:` total. The line numbers are `test_fight.c` lines at `cfff063`; the first batch ran before the five case-2 lines were added at `:4005`, and its later line numbers are restated at `cfff063`.

  | mutation | failures |
  |---|---|
  | `0x34E2C` callback not called (pre-fix) | 15 (first `:3906` `102 != 2`) |
  | `0x3E62C` unregistered | 1 (`:3779`) |
  | `0x3E524` unregistered | 1 (`:3781`) |
  | `0x3E4E4` unregistered | 8 (`:3775`, `:4028` `21845 != 800`, …) |
  | case 7 callback not called | 2 (`:3940`, `:3942`) |
  | x read after `0x3C4CC` | 1 (`:3914` `8738 != 4369`) |
  | `0x3E62C` through `0x2BC30` instead of `0x3C4CC` | 1 (`:3916`) |
  | `0x3E62C` `+0x53` = 8 | 3 |
  | `0x3E62C` `+0x57` = 0 | 1 |
  | `0x3E62C` without `+0x41` bit 7 | 1 |
  | `0x3C190` sign inverted | 2 |
  | case 0 quotient off by one | 2 |
  | case 0 `v` read as the word `+0x34` | 2 |
  | case 0 gravity `0x23` | 1 |
  | case 0 without the zero horizontal speed | 1 |
  | `+0x63` gate `> 10` | 1 |
  | case 1 `>=` instead of `>` | 5 |
  | case 1 without the speed-0 landing | 8 |
  | case 1 without `0x3C148` / `0x3C16C` / `0x188AC` | 2 / 1 / 1 |
  | case 1 `+0x57` = 2 | 2 |
  | case 1 keeps `+0x54` | 1 |
  | case 2 runs case 0 / case 1 | 1 / 2 |
  | `0x3E4E4` without its gate | 2 |
  | `0x3E4E4` speed `0x300` | 1 |
  | `0x3E4E4` without the restart | 4 |

### 19.5 Measured

| measurement | before (`38b3e74`) | after (`cfff063`) |
|---|---|---|
| capture 880 | best 521/522 splice, 6 073 px | **0 px** (521/522 splice, row 101) |
| captures 881..890 | 8 207 … 18 881 px | **0 px** (883 is port 524, 890 is port 530) |
| port f = 105..112, side 0 | 9/0/0 `0x8F35` → `0x8F34`, then 3/4/2 at f = 107 | 9/7/2, `0x91B3` … `0x91B8`, leap at f = 112 |
| demo oracle first unexplained | 880 (raw 3787); `[880..3616]` 2737 / 2731 unexpl.; port `[522..1380]` | **891 (raw 3798)**; `[891..3616]` 2726 / 2720 unexpl.; port `[531..1380]` (850, 0 exhibited) |
| demo-fight ratchet | `[880..1884]` 1005, N = 880 | **`[891..1884]` 994**, "ratchet improved: 891 > 880", **N = 891** |
| front-end oracle | `[560..879]` / 320 / 140 clean, 176 splice, 0 transition, 2 unexpl. (832, 833) | **`[560..890]` / 331 / 142 clean, 185 splice, 0 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved, which is the move the brief allowed.
These were unmoved:

* title `54/55/2/0` and `54/57/0/0`, determinism 54
* smk 120/120 and 41/41
* attract 215/216 (expected divergence at 215)
* C-vs-Python 9866
* `symbols.h`

The front-end "endpoints BAD" line is the same as before. The exhibition set
grows to port frames 0..530 (294 exhibited). The ladder was:

```
cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify && make demo-oracle
```

It exited 0, with 0 compiler warnings in the clean rebuild. The 240
case-insensitive "warning" hits in the verify log are the usual `gra_extract.py`
ResourceWarnings.

**Unresolved code targets after the fix (a temporary `PR_T9` probe on
`anim_indirect` and the `0x35045` call, reverted).**

* The animation-opcode target **`0x35938`** is first hit at **f = 173** on
  side 1 (50 hits in the run; Task 8 measured f = 165 before this fix moved the
  run), and `0x3640C` at f = 712.
* The reaction callbacks `0x3D17C` (reaction `0x20`, first at f = 350, 3 hits),
  `0x3ECF8` (reaction `0x2C`, f = 688) and `0x3C0A4` (reaction `0x3E`, f = 832)
  are still unregistered, so they are skipped.

None of these fires before 891's frame (f = 114).

**The new first unexplained frame, 891 (characterised, not fixed).** Capture
891 is a tear. Its best splice, port 530/531 (split at row 29), leaves 2 188 px
in x 100–287, rows 29–199:

* the raptor, which the capture shows struck in mid-air by the leaping T-rex, with a red hit spray and a different pose (the port's `0x1753` flies on untouched)
* a worshipper at the bottom left

Captures 892 onwards differ across the whole frame (37 649 px at 892, then about
45 000–52 000). In the port at f = 114, the T-rex (`slot+0x2C` 934 → 3760 by
f = 117) closes on the raptor (4190 → 3740). Yet side 1's `0x3CF38`
`hit_scan` returns −1 at every f = 105..125, and `DS_00100AD0` stays 0 on both
sides through f = 120. `get_xrefs_to 0x100AD0/0x100AD4` names its writers:
`0x17CB0` (`0x17CDC`/`0x17CE2`) and `0x176CC` (`0x178EF`, which stores the
overlap count `DAT_00100B54` from the fighters' sprite-overlap test).
`0x17CB0` calls `0x176CC` at `0x17D0E`/`0x17D21`, and `0x1975C` calls
`0x17CB0` first, at `0x19763` (demo-fight record §5.1). The port's `fighter_think` does
not call it, so the whole `0x3B464` think chain never runs. The owner is
**not derived**. The candidate named here was that unported collision step.
(Derived since, §19.6: capture 891 is the T-rex's winner body at f = 114,
whose `+0x1C` callback `0x3E4C4` the port resolved to NULL; the collision
step is not 891's owner. This paragraph's closure estimate was also
overstated: of the callees it listed, `0x140E4`, `0x15C30`, `0x17EEC`,
`0x181D0` and `0x16DA4` are already ported. §19.6 measures the closure.)

## 19.6 Fix round: `0x3E4C4`, `0x3E484` and capture 891 (roar-timing Task 9, `6a48972`)

**Result in one line.** Capture 891 has one cause, and it is the port's. At
f = 114 `0x1958C`'s tail runs the winner body `0x193B0` for the T-rex. Its
slot `+0x1C` holds `0x3E4C4`, which `0x3E62C` stored at `0x3E680`. The port's
`fn_resolve` returned NULL, so it skipped the reaction that the raw applies
through `0x3B714`, and the raptor was not struck. This corrects §19.3/§19.5:
the callbacks' callers are `0x1958C`'s `0x19020` and `0x193B0`, not the
`0x1975C` think chain. Porting `0x3E4C4` (31 B; its callee is already ported)
explains 891. `0x3E484` stays a named gap, and it is inert in the demo (below).

### 19.6.1 The raw (Ghidra, fixups applied)

`get_function_callers FUN_00019020` and `get_function_callers FUN_000193b0`
each return one caller, `FUN_0001958c`.

`0x19020` (70 B, `read_memory` + capstone):

```
0x19032  cmp dword [eax+0x1077c8],0 ; je 0x19062   ; slot+0x18 (0x1077B0 + side*0x94 + 0x18)
0x1903b  mov ebx,eax ; mov eax,edx                 ; EAX = side
0x1903f  call dword [ebx+0x1077c8]
0x19048  test eax,eax ; jne 0x1905a
0x1904c  mov dword [edx*4+0x100af8],1 ; ret        ; zero result: AF8[side] = 1
0x1905a  mov dword [edx*4+0x100af8],0              ; non-zero:    AF8[side] = 0
```

`0x193B0`'s `+0x1C` arm (`0x194B6`..`0x19526`):

```
0x194f7  mov byte [ctx[3]+0x90],5
0x194fe  mov edx,[esp+8] ; mov eax,[esp]           ; EDX = ctx[2], EAX = ctx[0] = side
0x19505  call dword [edx+0x1c]
0x1950c  [ctx[2]+0x18] = 0 ; 0x19517 [ctx[2]+0x1c] = 0
0x19520  mov edx,eax ; mov eax,[esp+0xc] ; 0x19526 call 0x3b714   ; the +0x1C == 0 arm
```

`0x3E4C4` (31 B):

```
0x3e4c8  mov edx,eax ; mov eax,esp ; call 0x33950  ; ctx for side (ctx[2] = slot[side], ctx[3] = slot[1-side])
0x3e4d1  mov edx,[esp+8] ; mov eax,[esp+0xc]
0x3e4d9  call 0x3b714                              ; 0x3B714(ctx[3], ctx[2])
```

This is the same call as `0x19526`, which the port already runs as
`fighter_reaction(ctx[3], ctx[2])`.

`0x3E484` (63 B):

```
0x3e48a  mov edx,eax ; mov eax,esp ; xor ecx,ecx ; call 0x33950   ; ctx for side
0x3e495  lea eax,[esp+0x18] ; xor ebx,ebx ; call 0x18bd4          ; 16 flag bytes = 2
0x3e4a0  flags[0] = 1, flags[1] = 0, flags[8] = 0
0x3e4b4  mov eax,[esp] ; call 0x18c14                             ; (side, flags, EBX = 0, ECX = 0)
```

Its result is `0x18C14`'s EAX. `0x18C14` (1035 B, unported; the demo-fight
record §5.7 placed it off the demo path, which `0x3E484` now contradicts) walks
16 flag bytes. For each byte, 2 skips the check. 1 returns 1 when the
condition holds. 0 returns 1 (`0x19014`) when the condition fails. Only the
all-checks-pass path returns 0 (`0x1900C`, with `[esp+0x18]` = 1 for side
≠ `0x29A`). With `0x3E484`'s flags it reduces to three checks:

* flag 0 = 1 (`0x18C46`): return 1 when `DS_00100AF8[side]` ≤ 0 (`setle`)
* flag 1 = 0 (`0x18CBC`): return 1 when the other slot's word `+0x74` ≠ 0 (`ja`) or its word `+0x76` > 1 (`jg`)
* flag 8 = 0 (`0x18E4A`): return 1 when the other slot's `+0x42` bit 3 is set

Otherwise it returns 0.

`get_xrefs_to 0x100AF8` names only two readers: `0x1958C` (`0x19632`, `0x19720`,
both `!= 0` tests) and `0x18C14` (`0x18C49`, `<= 0`). The value itself is
never read, only its zero-ness and sign.

### 19.6.2 `0x3E484` is inert in the demo (measured; temporary trace, reverted)

A `PR_T9` probe at `0x195B6` evaluated the reduction above on the port's state
whenever the slot's `+0x18` was set, over the whole run, both before and after
the `0x3E4C4` fix:

| f | hook | `AF8[0]` | other `+0x74/+0x76/+0x42` | raw `0x18C14` → raw `AF8[0]` | port `AF8[0]` |
|---|---|---|---|---|---|
| 106..113 | `0x3E484` | 0 | 0 / 0 / `0x00` | 1 → 0 | 0 |
| 114 | `0x3E484` | 6 | 0 / 0 / `0x00` | 0 → 1 | 6 (non-zero) |

The raw's and the port's `AF8[0]` agree in zero-ness on every frame, and no
reader distinguishes 1 from 6. So leaving `0x19020`/`0x3E484` unported changes
nothing in this run.

**The gap, measured.** Its closure is:

* `0x19020` 70 B
* `0x3E484` 63 B
* `0x18BD4` 64 B
* `0x18C14` 1035 B
* its unported callees `0x189FC` 78 B and `0x18A4C` 98 B

That is 1 408 B in 6 functions. `0x18C14`'s other callees are already ported:
`0x1DDF4`, `0x39EFC`, `0x3B298`, `0x18B44`, `0x33950`, and through
`0x189FC`/`0x18A4C` also `0x1A570` and `0x33A10`. The closure is inside the
size gate. It is not ported here because it moves no measured frame, and
`0x18C14` has 37 call sites whose flag sets each need their own derivation.
It stays a `PORT:` gap at `fighter_pass_a`'s `0x195B6` note and at
`fighter_3e62c`'s store.

**The collision step (§19.5's candidate), measured.** `DS_00100AD0` stays 0
because `0x1975C`'s `0x19763 call 0x17CB0` is unported. The genuinely new
closure (`get_function_callees`, sizes from `prage.functions.csv`) is:

* `0x17CB0` 125 B
* `0x176CC` 574 B
* `0x17BC8` 231 B
* `0x3B938` 140 B, a §7.12 gap that `0x17BC8` and `0x1975C` both call

That is 1 070 B in 4 functions. Their other callees are already ported:
`0x140E4`, `0x15C30`, `0x16DA4`, `0x17EEC`, `0x181D0`, `0x15F48`, `0x1B544`,
`0x2B150` and `0x33950`, and `0x2C3FC` is the voice, out of scope. This is
also inside the size gate. Note that `fighter_think_side` returns at
`0x3B49F` while the other slot's `+0x64` is `0xFF`, which it is in the demo
through f = 120 (the probe), so a live `DS_00100AD0` would reach `0x1922C`,
`0x3962C`/`0x396AC`, `0x3B938` and `0x39278` but not the rest of `0x3B464`.

### 19.6.3 The fix and its assertions

* **Fix** (`6a48972`):
  * `fighter_3e4c4(side)` in `fighter.c` (`0x3E4C4`), registered bare at `0x3E4C4` in `actors.c`, because `0x193B0` already calls `fn(ctx[0])`.
  * `fighter_3c190` now calls `0x1A570` before it reads the slot's record, in the raw's order (`0x3C194`, then `0x3C1AE`). This is not observable: `0x1A570` writes nothing, and a mutation back to the old order fails 0 checks.
  * The `PORT:` notes at `0x195B6` and in `fighter_3e62c` now name `0x19020`/`0x18C14` instead of the think chain.
  * Two `test_fight.c` comments had continuation lines at column 1; they are re-indented. No `fighter.c` block comment had that defect (an `awk` scan of every indented block comment in the touched files found none).
* **Assertions.**
  * `check_anim_hold_scaler`: `0x3E4C4` is registered as `fighter_3e4c4`.
  * `check_winner_body` adds a third block, side 0 winning with `+0x18/+0x1C` = `0x3E484`/`0x3E4C4`. It checks:
    * the pose lands on slot 1 through `0x3E4C4`: `+0x52` = `0x10`, `+0x53` = `0x0A`, `+0x10` = `0x3A43C`
    * slot 0's `+0x52` sentinel `0x66` is kept
    * slot 1's `+0x90` = 5 (from the `0xEE` sentinel)
    * slot 0's `+0x18`/`+0x1C` are zeroed
    * the `0x19472` latch reads `0x1234`
* **Mutations.** A script applied each one. `fighter.c` and `actors.c` were restored and compared byte for byte, and the suite then passed.

  | mutation | failures |
  |---|---|
  | `0x3E4C4` unregistered (pre-fix) | 1 (the registration check; the test's fallback registers it for the body) |
  | `0x3B714` arguments swapped | 4 (`:4396` `0 != 16`, …) |
  | `ctx_swap` instead of `ctx_same` | 4 |
  | empty body | 3 |
  | `0x3C190` back to the old read order | 0 (not observable, as stated) |

### 19.6.4 Measured

| measurement | before (`c07f71c`) | after (`6a48972`) |
|---|---|---|
| capture 891 | best 530/531 splice, 2 188 px | **0 px** (530/531 splice, row 28) |
| demo oracle first unexplained | 891 (raw 3798); `[891..3616]` 2726 / 2720 unexpl.; port `[531..1380]` | **892 (raw 3799)**; `[892..3616]` 2725 / 2719 unexpl.; port `[532..1380]` (849, 0 exhibited) |
| demo-fight ratchet | `[891..1884]` 994, N = 891 | **`[892..1884]` 993**, "ratchet improved: 892 > 891", **N = 892** |
| front-end oracle | `[560..890]` / 331 / 142 clean, 185 splice, 0 transition, 2 unexpl. | **`[560..891]` / 332 / 142 clean, 186 splice, 0 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. These were unmoved:

* title `54/55/2/0` and `54/57/0/0`, determinism 54
* smk 120/120 and 41/41
* attract 215/216
* C-vs-Python 9866
* `symbols.h`
* "endpoints BAD"

The exhibition set grows to port frames 0..531 (295 exhibited).

**The new first unexplained frame, 892 (characterised, not fixed).** Capture
892 is a tear. Its best splice, port 531/532 (split at row 56), leaves 35 921 px
across rows 56–199. From 892 on, the whole scene differs because the
capture's camera drops relative to the port's. A shift search over the
background (x 20–119, rows 60–179, dx and dy in [−12, 12]) gives the best
match at dx = 0 and these dy:

| capture | port frame | dy |
|---|---|---|
| 892 (lower half) | 532 | +3 |
| 893 | 533 | +5 |
| 894 | 534 | +7 (next best +4) |
| 896 | 536 | +11 |

So the capture's view moves down by a few pixels per frame after the
raptor is struck at f = 114, and the port's camera does not. The fighters
differ as well. The owner is **not derived**. The candidates are:

* the camera's vertical follow of the two airborne fighters after the hit
* the reaction `0x3B714` applied (the struck raptor's new state and speeds)

The unported `0x19020`/`0x3E484` hook is not a candidate. The winner body
cleared the T-rex's `+0x18`/`+0x1C` at f = 114 (`0x1950C`/`0x19517`), and the
whole-run probe shows no hook set after that frame.
(Derived since, §20: the second candidate. The reaction put the raptor in
the `0x39F40` knockback pose, whose per-frame handler `0x39CC8` the port did
not have, so the raptor was never launched and the camera never followed it.)

## 20. The knockback pose's handler `0x39CC8` at capture 892 (roar-timing Task 10, `a51685d`)

**Result in one line.** Capture 892 has one cause, and it is the port's. At
f = 114 the reaction `0x3B714` → `0x3AAFC` finds the struck raptor airborne
(`slot+0x54` = 2) and calls the pose setter `0x39F40` (`0x3AC89`, EDX =
`0xFFFFFFB0`, EBX = `0x46`, ECX = `0x0C`, frame `0x14`), which stores the
per-frame handler `0x39CC8` in `slot+0x10` (`0x39F8F`: `c7 40 10 c8 9c 03 00`).
`0x3531C` case 10 calls it every frame (`0x354E2 call [ecx+0x10]`), and the
port's `fn_resolve(0x39CC8)` returned NULL. The raw handler launches the
raptor upward on the next frame (gravity 62, vertical speed 744, horizontal
160), and the camera, which follows the higher fighter's y (`0x12DA8`), rises
0x100 a frame from f = 115; the port's raptor hung at its hit position and the
camera barely moved. Porting `0x39CC8` (561 B, no Ghidra function) with its
new callees `0x39B30` (388 B), `0x35050` (125 B), `0x39AC8` (74 B) and
`0x39B14` (4 B), 1 152 B in 5 functions, explains captures 892..949.

### 20.1 The measurement and the trace (temporary, reverted)

**The capture.** A background shift search (x 20–119, rows 60–179, every
frame against port frame 530 as the reference) gives each frame's vertical
offset in background pixels:

| frame | 530/890 | 531/891 | 532/892 | 533/893 | 534/894 | 536/896 | 537/897 | 538/898 |
|---|---|---|---|---|---|---|---|---|
| port (`74e0158`) | 0 | 1 | 1 | 2 | 3 | 3 | 6 | 10 |
| capture | 0 | 1 | 4 | 7–8 | 10–11 | 14 | 17 | 20 |

So the capture's camera starts rising one frame after the hit and climbs about
3 background pixels (one 0x100 camera step) a frame; the port's starts only at
f = 121. Side by side (capture 894 vs port 534), the capture's raptor flies up
and to the right out of the T-rex's jaws while the port's stays at the hit.

**The port.** A `getenv("PR_T10")` line at the end of `fight_arena_frame`
(after `0x12DA8`) printed `f`, `DS_000F0AEC`, `DS_000F0AF0`, `DS_001078F2` and
per side `+0x52/+0x53/+0x54`, `+0x57`, `+0x5F`, `+0x40..+0x43`, `slot+0x2C/+0x30`,
`rec+8`, `rec+0x18/+0x1C`, `rec+0x34/+0x36/+0x44`, the `+0x0C/+0x18/+0x1C`
callbacks, `+0x64` and `+0x90`; the dump hook printed the dump index per `f`
(dump index = f + 416). It has been reverted.

| f | `F0AEC` before | raptor (side 1) before | `F0AEC` after | raptor after |
|---|---|---|---|---|
| 114 | 1722 | 10/0A/02, y 12021, v 113, x 4340 | 1722 | same |
| 115 | 1779 | y 12134, v 90, `rec+8` `0xD22A0` | **1978** | y 12765, v 682, g 62, h +160, `rec+8` `0xD2A88` |
| 116 | 1825 | y 12224 | **2234** | y 13447 |
| 120 | 2149 | y 12354, v −25 | **3258** | y 16643 |
| 128 | — | — | 5306 | phase 3, g 64 |
| 147 | — | — | 3691 | lands: y 2560, `+0x58` 4 |

`y` is `slot+0x30`; the camera target is `(y − 0x1400)² · C` (`0x1317C`), so
the raptor's y above ~14 600 is what makes the capture's camera climb. After
the fix `F0AEC` matches the capture's slope (1978 at f = 115, 3258 at f = 120).
A temporary probe inside `fighter_39cc8` (reverted) gave the phase sequence
0 (f = 114), 1 (115), 2 (116..128), 3 (129..147, lands at 147), 4 (148 on),
all inside the explained window.

### 20.2 The raw (Ghidra `read_memory` + capstone, fixups applied)

`0x39CC8` has no Ghidra function (`0x39B30` ends at `0x39CB4`); its jump table
is `0x39CB4`: `fa 9c 03 00 07 9d 03 00 1d 9d 03 00 dc 9d 03 00 e4 9e 03 00`
(cases 0..4, `ja 0x39EF4` above 4). `get_xrefs_to 0x39CC8` lists only data
references: `0x39F8F` (the store), `0x39F1A` (`0x39EFC`'s gate) and `0x145A1`
(`0x14590`, a predicate: 1 when the other slot's `+0x10` is `0x39CC8` or its
`+0x52` is `0x11` and `word[0x107D2C + side*2]` > 0; its six callers
`0x1461C`, `0x146F0`, `0x14814`, `0x14988`, `0x14A5C` and `0x14B90` are
unported, so the port does not reach it).

```
0x39cc8  push ecx ; sub esp,0x20 ; mov edx,ebx ; mov eax,esp ; call 0x33a10   ; ctx_swap(side = EBX)
0x39cd9  call 0x35050(side)
0x39ce2  switch byte [slot+0x58]
case 0 (0x39cfa): +0x58 = 1
case 1 (0x39d07): call 0x39b30(side) ; +0x58 = 2
case 2 (0x39d1d): if word [rec+0x36] >= 0 (setl): return
                  rec+0x63 = 0 ; +0x58 = 3
                  hold = FILD [0x107A70+side*4] / FILD [0xBED38+ch*4]     ; FDIVRP ST(1)
                  d = (s16)(word [slot+0x30] - word [0xBD884+ch*2]) / 64  ; sar/shl/sbb/sar: truncating
                  word [rec+0x44] = 0x39AC8(|d|, word [0x107A70+side*4])
                  0x2BC30(rec, [0xBED88+ch*4], hold)
case 3 (0x39ddc): byte [rec+0x28] |= 0x20
                  land = ([0xBD882+ch*2] >> 16) >= slot+0x30   (setge, BEFORE the latch)
                  0x186D0(side) ; if !land: return
                  rec+0x63 = 0 ; x = slot+0x2C
                  0x2BC30(rec, [0xBEDB0+ch*4], 3.0) ; 0x186D0 ; 0x3C16C
                  0x1890C(side, [0xBECF8+ch*2] >> 16) ; 0x188DC(side, x)
                  rec+0x43 = 0x14 ; word [0x107824 + rec.0x51*0x94] = 0x29A ; +0x58 = 4
                  0x2AE14(0xBB1DC, EDX = slot+0x2C, ECX = rec+0x30 >> 16, EBX = 0, push 0)
                  0x2C3FC(0x6C)   ; voice
case 4 (0x39ee4): byte [rec+0x28] &= 0xDF ; slot+0x54 = 0
```

`0x39B30` (`disassemble` via `read_memory`; callees per `prage.calls.csv`:
`0x33A10`, `0x3C148`, `0x3C16C`, `0x3C520`, `0x3C480`, `0x1890C`, `0x39AC8`,
`0x39B14`, `0x3C190`):

```
0x39b3a  ctx_swap(side) ; 0x3C148(side) ; 0x3C16C(side)
0x39b64  hold = FILD [0x107A60+side*4] / FILD [0xBED10+ch*4]           ; FDIVRP ST(1)
0x39b7e  if slot+0x54 == 2: 0x3C520(rec, [0xBED60+ch*4], hold)
         else: 0x3C480(rec, [0xBED60+ch*4], hold) ; 0x1890C(side, [0xBD882+ch*2] >> 16)
0x39be8  if ([0xBD882+ch*2] >> 16) > slot+0x30 (jle skips): 0x1890C(side, that)
0x39c0d  word [rec+0x44] = 0x39AC8([0x107A76+side*4] >> 16, [0x107A5E+side*4] >> 16)   ; (a, n)
0x39c32  word [rec+0x36] = 0x39B14([rec+0x42] >> 16, [0x107A5E+side*4] >> 16)          ; g * n
0x39c6d  q = (s16)word [0x107A68+side*4] * 64 / (s16)(word [0x107A60+..] + word [0x107A70+..])
0x39c76  0x3C190(side, (s16)q)
0x39c7f  rec+0x63 = 0 ; slot+0x41 |= 0x80 ; slot+0x68++ ; if slot+0x68 >= 3: word [slot+0x74] = 0x29A
```

`0x39AC8` (74 B): `q = ((s16)a << 7) / ((s16)n * (s16)n)` (IDIV), then
`FILD q + double[0x80BFA]` against `FILD (s16)q + 1` (`FCOMPP`, `JC`): `q` when
below, else `q + 1`. `read_memory 0x80BFA` = `00 00 00 00 00 00 e0 3f` (0.5).
`0x39B14` is `imul eax,edx ; ret`. `0x35050` (125 B) calls `[slot[side]+0x14]`
(EAX = EDX = the slot) when non-zero and zeroes it on a non-zero return, the
`0x1952F` shape.

**FDIVRP.** `DE F1` is `FDIVRP ST(1),ST(0)`: `ST(1) = ST(0) / ST(1)`. `ST(0)`
is the second `FILD`, the pose word, so the hold is word / table: 12 / 7 at
launch and 20 / 7 in the fall. The reverse (7 / 12) fails three assertions
(§20.3), and the landing 3.0 is a plain push.

**Tables (`read_memory`), char 3 (the demo raptor, `slot+0x7A`):**
`0xBED10[3]` = 7, `0xBED38[3]` = 7, `0xBED60/88/B0[3]` =
`0xD2A6E`/`0xD2AA4`/`0xD2ADA`, `word[0xBD884 + 3*2]` = `0x1600` (5632),
`word[0xBECFA + 3*2]` = 2560. The three streams begin `DA00 …`, `FF20 …`,
`FF21 …`, `ED40 xxxx 000D` (the first id from the indirection: `0x17F5`,
`0x18AD`, `0x18B3`), with no hold opcode, so the pushed holds stand.

**Derived launch** (side 1's words from `0x3AC89`): gravity
`0x39AC8(0x46, 0x0C)` = 8960 / 144 = 62.2 → 62; vertical 62 · 12 = 744;
horizontal −80 · 64 / (12 + 20) = −160, negated by `0x3C190` while the raptor's
pset is unflipped → +160. The trace's f = 115 row (v 682 = 744 − 62 after one
integration, g 62, h +160) is exactly this.

### 20.3 The fix and its assertions

* **Fix** (`port/src/game/fighter.c`, `fighter.h`, `actors.c`):
  `fighter_39cc8(slot, side)` (exported; registered bare at `0x39CC8`, the
  same case-10 shape as `0x3A43C`), the static `fighter_39b30`,
  `fighter_35050` (a `PORT:` note: the raw's EAX = EDX = slot; no ported
  writer stores a non-zero `+0x14`, so it takes `0x1952F`'s shape),
  `fighter_39ac8`, `fighter_39b14`, and the helper `fighter_hold_ratio` for the
  shared FILD/FDIVRP/FSTP idiom (a quotient of two 32-bit integers rounds to
  the same float from double as from the x87's extended format). Table
  addresses are local `#define`s. The voice `0x2C3FC(0x6C)` is a `PORT:` gap.
  The review minor: `fighter_pass_a`'s `0x195B6` note carries a run-specific
  `TODO(verify):` (the `0x3E484` gap is proven inert only for f = 106..114).
* **Assertions** (`test_fight.c`): `check_anim_hold_scaler` checks the
  registration; the new `check_knockback_pose` seeds the demo raptor (side 1,
  char 3, `+0x42` bit 3 so y anchors show on `rec+0x1C`, side 0's pose words
  distinct sentinels) and checks:
  * phase 0: `+0x58` 0 → 1, nothing else
  * phase 1 through `fighter_state_3531c(1)` (case 10): `+0x58` = 2, the
    `0xD2A6E` stream, hold `0x3FDB6DB7` (12/7), id `0x17F5`, `rec+0x1C` 3000 →
    5632 (the conditional ground anchor), g 62, v 744, h +160, `rec+0x43`/`+0x63`
    cleared, `+0x41` bit 7, `+0x68` 1 → 2 with `+0x74` kept
  * airborne above the ground: `rec+0x1C` 9000 kept, flipped pset → −160,
    `+0x68` 2 → 3 sets the side's `+0x74` = `0x29A` (the other side's kept)
  * grounded (`+0x54` = 0): `rec+0x1C` → 5632 through `0x3C480` + `0x39BC5`;
    with `+0x42` bit 3 clear and `DS_00100AB4[1]` = 8000 only the
    unconditional anchor moves y (`rec+0x1C` = 5632 − 8000, `slot+0x30` = 5632)
  * phase 2: `v` = 0 waits (`setl`); `v` = −62 from 12 993 above the ground
    gives g 64, `+0x58` 3, the `0xD2AA4` stream, hold `0x4036DB6E` (20/7), id
    `0x18AD`; 12 993 below gives 64 (truncating, then `abs`); 204 · 64 above
    gives 65 (`0x39AC8`'s q + 1 arm is not taken, q = 65)
  * phase 3: 6000 does not land although the latch then lowers `+0x30` to
    1000 (the compare precedes `0x186D0`); 5632 (equal, `setge`) and 5000 with
    the record at 9000 land: `+0x58` 4, the `0xD2ADA` stream at 3.0, id
    `0x18B3`, `rec+0x1C` 2560, `rec+0x18` kept, `+0x36`/`+0x44` 0, `rec+0x43`
    `0x14`, the side's `+0x74` `0x29A`, `rec+0x28` bit 5, and the dust in a
    three-record scratch pool at `(0x4321, 9, 0)`
  * phase 4 clears `rec+0x28` bit 5 and `+0x54`; phase 5 returns
  * `0x35050`: a test-only target registered at `0x7FFF0000` (outside the
    code object) is called once and cleared on 1, kept on 0, and the other
    slot's `+0x14` is not called
  * `0x107A60..0x107A7F`, `DS_00104B00`, `DS_001078F6`, the pool globals and
    the `0x100AB0`/`0x100AF0`/`DS_001077A8` seeds are restored
* **Mutations.** A script applied each one to `fighter.c`/`actors.c`, rebuilt
  and ran the suite; both files were restored and compared byte for byte
  (identical), and the suite then passed. Counts are real `FAIL` lines.

  | mutation | failures |
  |---|---|
  | `0x39CC8` unregistered (pre-fix) | 1 (the registration check; the test's fallback registers it) |
  | case 0 sets 2 | 1 |
  | case 1 without `0x39B30` | 18 |
  | hold den/num swapped (FDIVP) | 3 |
  | `0x39B30` `+0x54` test inverted | 2 |
  | no conditional ground anchor | 1 |
  | no grounded-path anchor | 2 |
  | conditional anchor `>=` | 0 (not observable: at equality `0x1890C` adds 0) |
  | `0x39AC8` always q + 1 | 5 |
  | `0x39AC8` a · 64 | 5 |
  | `0x39B14` as an add | 1 |
  | horizontal divisor n only | 2 |
  | no `0x3C190` (unsigned store) | 1 |
  | `+0x68` threshold 4 | 1 |
  | no `+0x41` bit 7 | 1 |
  | launch words read for the other side | 2 |
  | case 2 gate `> 0` | 3 |
  | case 2 floor division | 1 |
  | case 2 without `abs` | 1 |
  | case 2 `m` from the other side | 3 |
  | case 2 without `+0x58` = 3 | 1 |
  | case 3 latch before the compare | 16 |
  | case 3 `>` instead of `>=` | 13 |
  | case 3 without the dust | 6 |
  | case 3 dust a4 = y | 2 |
  | case 3 y from `0xBD882` | 2 |
  | case 3 without `0x3C16C` | 4 |
  | case 3 `+0x74` of the other side | 4 |
  | case 3 without `rec+0x28` bit 5 | 3 |
  | case 4 keeps `+0x54` | 1 |
  | `0x35050` not called / other side / always clears | 4 / 4 / 1 |

### 20.4 Measured

| measurement | before (`74e0158`) | after (`a51685d`) |
|---|---|---|
| captures 892..949 | 892: best 531/532 splice, 35 921 px; later ~45 000–52 000 px | **all explained** (clean or splice; `--frontend` finds no unexplained frame in them) |
| demo oracle first unexplained | 892 (raw 3799); `[892..3616]` 2725 / 2719 unexpl.; port `[532..1380]` | **950 (raw 3857)**; `[950..3616]` 2667 / 2661 unexpl.; port `[582..1380]` (799, 0 exhibited) |
| demo-fight ratchet | `[892..1884]` 993, N = 892 | **`[950..1884]` 935**, "ratchet improved: first unexplained 950 > 892", **N = 950** |
| front-end oracle | `[560..891]` / 332 / 142 clean, 186 splice, 0 transition, 2 unexpl. | **`[560..949]` / 390 / 150 clean, 236 splice, 0 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved, which is the move the brief allowed.
These were unmoved:

* title `54/55/2/0` and `54/57/0/0`, determinism 54
* smk 120/120 and 41/41
* attract 215/216 (expected divergence at 215)
* C-vs-Python 9866
* `symbols.h`
* the front-end "endpoints BAD" line

The exhibition set grows to port frames 0..581 (345 exhibited). The ladder
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify && make demo-oracle`
exited 0 on the fix's tree, with 0 compiler warnings in the clean rebuild and
"all checks passed".

**Unresolved code targets after the fix** (a temporary `fn_resolve`-miss probe
over the whole dump run, reverted; `f` is `DS_0010150C`): the animation-opcode
target **`0x347B8`** at f = 165 (1 hit, §20.5), `0x35938` at f = 201 (53
hits; §19.5 measured f = 173 before this fix moved the run), `0x4AC18` at
f = 206 (3 hits) and `0x370F0` at f = 291 (1 hit), besides the known
front-end effect sites `0x29B74`/`0x41578` and the type-table stub `0x5D812`.
The reaction callbacks `0x3D17C`, `0x3ECF8` and `0x3C0A4` that §19.5 listed no
longer miss in this run.

### 20.5 The new first unexplained frame, 950 (characterised, not fixed)

Capture 950 is a tear. Its best splice, port 581/582 (split at row 122),
leaves 1 731 px in x 132–264, rows 161–199 (951: 2 948 px, 952: 3 217 px,
all in the raptor's box near the ground). Side by side, the capture's raptor
stays lying where it landed while the port's gets up. Port frame 582 is
f = 166. The raptor's landing stream `0xD2ADA` (`0xBEDB0[3]`, started at
f = 147) reaches `D500 47B8 0003` at `0xD2B00` at f = 165 (its `rec+8` moves
`0xD2AF4` → `0xD2B04`): opcode `0x15`, mode `0x4000`, the dword `0x000347B8`,
which the port has not registered, so the dispatch skipped it (the probe's
only miss near that frame). `0x347B8` has no Ghidra function; with
`DS_00104B00` = 3 (≠ 7, `0x347E4`) it jumps to `0x34827`: `0x39A10(rec,
0x29A)`, `0x3C148`, `0x3C16C`, `+0x54` = 0, `+0x52` = 9, `+0x53` = `0x0B`,
`+0x43` &= `0xFC`, then `0x340BC`/`0x34168` and a per-character stream from
the table at `0x34780`. That is the candidate owner (the raptor held down in
state 9/0x0B instead of finishing the stand-up stream); it is **not derived
here**, and its closure is not measured.

**Correction (roar-timing Task 11, raw wins).** `0x347B8` does not take its
stream from `0x34780` alone: at `0x3485F` (`test al,al ; je 0x348C6`) the
`0x340BC` result picks the table. Non-zero runs `0x34168` and jumps through
`0x34780` (`0x34878`); zero jumps through `0x3479C` (`0x348D6`). The demo
raptor takes the zero arm (§21.2), so its stream is `0x3479C[3]` = `0xD28FC`,
not `0x34780[3]` = `0xD2940`.

## 21. The knockdown floor `0x347B8` at capture 950 (roar-timing Task 11, `3fee8d0`)

**Result in one line.** Capture 950 has one cause, and it is the port's. At
f = 165 the landed raptor's stream `0xD2ADA` reaches `D500 47B8 0003` at
`0xD2B00` (opcode `0x15`, mode `0x4000`, the dword `0x000347B8`), and the
port's `fn_resolve(0x347B8)` returned NULL, so the dispatcher skipped it and
walked on into the ids `0x18B3..` that follow, which play as a get-up. The raw
`0x347B8` puts the slot in state 9/`0x0B`/0 and restarts the record on the
per-character floor stream `0x3479C[3]` = `0xD28FC`, a 20-pass lying loop
that ends in the second opcode-`0x15` target `0x346F8` (the get-up) at
f = 206. Porting `0x347B8` (448 B), its gate `0x340BC` (172 B), the stun start
`0x34168` (276 B) and `0x346F8` (133 B), 1 029 B in 4 functions with all their
callees already ported, explains captures 950..991.

### 21.1 The measurement and the trace (temporary, reverted)

**The capture.** Per-capture nearest port frame (`near.py`, whole frame) on
`f1023d5`: 950/581 16 754 px, 952/583 5 132, 953/584 3 205, 960/590 2 799,
968/597 1 788, 975/603 4 376, 983/610 4 642, each residual in the raptor's box
(x ≈ 100–300, rows ≈ 115–199). Side by side the capture's raptor lies flat on
the sand through 950..967 while the port's rises.

**The port.** A `getenv("PR_T11")` line at the end of `fight_arena_frame`
printed per side `+0x52/+0x53/+0x54`, `+0x5A`, `+0x8C`, `+0x63`, `+0x43`,
`+0x58`, `+0x10`, `+0x0C`, `rec+8`, `rec+0x24`, `+0x2C/+0x30`, `+0x74` and the
other side's `+0x5A` for f = 140..320, and `anim_indirect` printed every
`fn_resolve` miss with its target, record, `rec+8` and operand. Both have been
reverted. On `f1023d5` the raptor (side 1) sat in 0x10/0x0A/0 with `+0x58` = 4
from its landing at f = 147; at f = 165 the miss line read
`tgt=347b8 rec8=d2b04 arg=10`, and its `rec+8` then walked
`0xD2B06`, `0xD2B08`, … `0xD2B14` (the ids `0x18B3..0x18B8`, `CD40 1912`).
The gate's inputs at f = 165: `+0x8C` 0, the raptor's `+0x63` = 1, its `+0x5A`
= 22 against the T-rex's 9.

### 21.2 The raw (Ghidra `read_memory` + capstone, fixups applied)

`0x347B8` has no Ghidra function and no code xrefs; a scan of the data object
(`read_memory 0x80000..0x10B0CF`) finds the dword `0x000347B8` 53 times
(`0xD273C` … `0xED49A`, including `0xD2B02`), `0x000346F8` 13 times (including
`0xD2914`) and `0x00034530` once (`0xD2958`). `get_xrefs_to` gives
`0x340BC` one caller (`0x34858`) and `0x34168` one (`0x34863`).

```
0x347b8  push ebx,ecx,edx,esi ; ebx = rec ; esi = byte [rec+0x51] (side)
         ecx = 0x1077B0 + side*0x94 (slot) ; eax = side*0x94
0x347db  if word [0x104B00] == 7:                                   ; 0x347e4 jne 0x34827
           dl = byte [0x104B16]
           if dl == 2: if esi != dword [0x104AD4]: goto freeze      ; 0x347f6 jne 0x3480f
                       byte [slot+0x41] |= 0x10 ; goto floor        ; 0x347fe (0x1077F1 + eax)
           elif dl == (side ^ 1): goto freeze                       ; 0x34807..0x3480d
           else goto floor
freeze   0x3480f  dword [rec+0x24] = 0 ; +0x54 = 3 ; +0x52 = 9 ; +0x53 = 3 ; ret
floor    0x34827  0x39A10(rec, 0x29A) ; 0x3C148(side) ; 0x3C16C(side)
         0x34841  +0x54 = 0 ; +0x52 = 9 ; +0x53 = 0x0B ; +0x43 &= 0xFC
         0x34858  if 0x340BC(side): 0x34168(side) ; stream = jmp [0x34780 + char*4]
                  else:                     stream = jmp [0x3479C + char*4]
                  (char > 6: 0x34962, edx = 0xE4290, both arms)
         0x34967  0x2BC30(rec, stream, push 3.0) ; ret
```

The two jump tables (`read_memory 0x34780`, 0x38 bytes) and each case's
`mov edx,imm32`:

| char | `0x34780` (stun) | `0x3479C` (plain) |
|---|---|---|
| 0 | `0x34880` → `0xE7656` | `0x348DE` → `0xE761A` |
| 1 | `0x3488A` → `0xE42F6` | `0x34962` → `0xE4290` |
| 2 | `0x34894` → `0xED302` | `0x348F4` → `0xED2CE` |
| 3 | `0x3489E` → `0xD2940` | `0x3490A` → `0xD28FC` |
| 4 | `0x348A8` → `0xEAF0E` | `0x34920` → `0xEAEDE` |
| 5 | `0x348B2` → `0xD45D0` | `0x34936` → `0xD4594` |
| 6 | `0x348BC` → `0xE0EF4` | `0x3494C` → `0xE0E8E` |

`0x340BC` (EAX = side; `[esp+8]` = its slot, `[esp+0xC]` the other's): 0 when
`word [me+0x8C]` > 0 (`jg`, signed), `[me+0x63]` ≠ 0, `[oth+0x63]` ≠ 0,
`[me+0x5A]` ≤ 0x54 (`jle`), `[me+0x5A] − [oth+0x5A]` ≤ 0x3C (`jle`) or
`[me+0x5A]` ≥ 0x78 (`jge`); else 1. The demo raptor fails at `+0x63`, and
also at `+0x5A` = 22.

`0x34168` (EAX = side): `word [me+0x8C]` = `0x4B0`; `0x2AE14(desc =
[0xBDBC8 + char*4], EDX = side ? 0x4200 : 0x200, ECX = 0xFF, EBX = 0xD80,
push 0)` (`0x2AE14` ends `ret 4` at `0x2B14A`, so `[esp+0x18]` is the side
again after it); `[0x1077A0 + side*4]` = the returned record; for side ≠ 0
`0x2A17C(record, 0x74, 0)`; `[me+0x5D]` = 0; `[me+0x43]` &= `0xFB`;
`byte [0x1078FF]` = side; `byte [0x104AE9]` |= 1. `read_memory 0xBDBC8`:
`0xBDB3C`, `0xBDB50`, `0xBDB64`, `0xBDB78`, `0xBDB8C`, `0xBDBA0`, `0xBDBB4`.

`0x346F8` (EAX = rec): `word [slot(rec+0x51)+0x76]` = `word [0xBDBE6]` + 1
(`read_memory`: `03 00`, so 4), then `0x36870(rec)`.

**The streams.** `0xD28FC`: `DC00 000D1478` (hold bytes `02 02 …`),
`ED40 000D2918` (ids `0x18B9..0x18CC` by variable), `B840 0014 000D2902`
(20 passes back to the `ED40`), `9E00`, `D500 000346F8`. `0xD2940` (stun):
`DC00 000D1618`, `ED40 000D295C`, `B840 0020 000D2946`, `9E00`,
`D500 00034530` (unregistered; not reached in the demo).

### 21.3 The fix and its assertions

* **Fix** (`port/src/game/fighter.c`, `fighter.h`, `actors.c`):
  `fighter_347b8(rec)`, `fighter_340bc(side)` (exported for the gate test),
  the static `fighter_34168(side)` and `fighter_346f8(rec)`; the two jump
  tables are `static const` arrays with each case's address; `0xBDBC8`,
  `0xBDBE6` and `0xE4290` are local `#define`s. `actors.c` registers
  `0x347B8` and `0x346F8` through `(rec, arg)` wrappers (both raws save EDX
  and read only EAX).
* **Review minors from frame 892.** The slot `+0x14` callback now takes the
  slot at all three raw call sites (`0x19537`, `0x3514C`, `0x350B8`: EAX = EDX
  = slot, zeroed on a non-zero return), through one `fighter_slot14_cb`
  typedef whose `PORT:` note names the raw writers (`0x22B6F` in `0x22B28`,
  and `0x22CD7`) and their target `0x29D04`, which reads EAX as the slot
  (`0x29D16`, `0x29D25`). `0x350D0`'s site used to call `void (*)(void)` and
  never zeroed the field; it now matches the raw. `fighter_35050`'s header
  names both callers (`0x39CD9`, `0x22C60`) and no longer calls `0x39CC8` a
  `+0x14` callback. `check_knockback_pose` zeroes `s0+0x14` at its end and
  gains a char-0 seed (`0xBED10[0]` = 5, `0xBED38[0]` = 8: holds 12/5 =
  `0x4019999A` and 20/8 = `0x40200000`).
* **Assertions** (`test_fight.c`): the registrations (through wrappers) in
  `check_anim_hold_scaler`; the new `check_knockdown_floor`:
  * A, the gate: the demo seed fails; each bound (`+0x8C` 1 fails and −1
    passes, either `+0x63`, 0x54/0x55, a lead of 0x3C/0x3D, 0x78/0x77) and
    the side's own slot
  * B, f = 165 through the dispatcher (`D500 47B8 0003` walked by
    `actors_anim_begin`): 9/`0x0B`/0, `+0x43` `0xFF` → `0xFC`, `+0x74`
    `0x29A` (the other side's kept), the record's speeds and `+0x42/+0x43`
    zeroed, `rec+0x10` = `0xD1478`, `rec+8` in `0xD28FC`, hold 2.0 (the
    stream's), id `0x18B9`; `+0x8C`, `DS_001077A0[1]`, `DS_001078FF` and
    `+0x5D` untouched
  * C, the plain table: char 0 → `0xE761A` (`rec+0x10` `0xE5598`), 1 and 7
    → `0xE4290` (`0xE2528`)
  * D, the stun arm on both sides in a three-record scratch pool: `+0x8C`
    `0x4B0`, `DS_001077A0[side]` = the spawn (the other entry kept), its
    `+0x18` = `0x4200`/`0x200` and `+0x1C` = `0xD80`, `+0x5D` 0, `+0x43`
    `0xF8`, `DS_001078FF` = side, `DS_00104AE9` `0x02` → `0x03`; side 1's
    spawn pset word `0x874` (`0x74` | `0x800`, the spawn's `+0x5F` = 1) and
    the `0xD2940` stream (`rec+0x10` `0xD1618`); side 0 not `0x874` and
    `0xE7656` (`0xE57F8`)
  * E, mode 7: `B16` = 2 with `AD4` ≠ side freezes (hold 0, 9/3/3, nothing
    else), `AD4` = side sets `+0x41` bit 4 and floors; `B16` = side ^ 1
    freezes (both sides), another value floors
  * F, `0x346F8` through the dispatcher: `+0x76` 4 (the other side's kept),
    `0x36870`'s `+0x74`/`+0x10` cleared and `+0x5F` `0xFF`, the walk goes on
    to the next id
  * the `+0x14` shape: `kb_stub_14` records its argument; `0x35050`,
    `0x350D0` and `0x193B0` each pass the slot and zero the field on 1
* **Mutations.** A script applied each one to `fighter.c`/`actors.c`, rebuilt
  and ran the suite; both files were restored and compared byte for byte
  (identical), and the suite then passed. Counts are real `FAIL` lines.

  | mutation | failures |
  |---|---|
  | `0x347B8` / `0x346F8` unregistered | 1 / 1 (the registration checks; the test's fallback registers them) |
  | gate `+0x8C` `>= 0` / unsigned | 25 / 1 |
  | gate without `me+0x63` / `oth+0x63` | 1 / 1 |
  | gate `< 0x54` / `< 0x3C` / `> 0x78` | 1 / 1 / 1 |
  | gate reads the other slot as its own | 26 |
  | `0x34168` EDX sides swapped / ECX, EBX swapped | 2 / 2 |
  | `0x34168` without `+0x8C` / `+0x5D` / `+0x43` mask / `DS_001078FF` | 2 / 2 / 2 / 2 |
  | `0x34168` `DS_00104AE9 = 1` | 2 |
  | `0x34168` `0x2A17C` on both sides / on side 0 only | 1 / 2 |
  | `0x34168` stores into the other side's `DS_001077A0` | 4 |
  | `0x347B8` mode test `!= 7` | 39 |
  | mode 7: `AD4 ==` freezes / no `+0x41` bit 4 / `B16 == side` | 10 / 1 / 5 |
  | freeze without hold 0 / `+0x54 = 0` / falls through | 3 / 1 / 10 |
  | floor without `0x39A10` / `0x3C148` / `0x3C16C` | 2 / 3 / 2 |
  | floor keeps `+0x54` / `+0x53 = 0x0A` / `+0x43 &= 0xFE` | 1 / 5 / 3 |
  | gate inverted / no `0x34168` / tables swapped | 29 / 17 / 3 |
  | default at char > 7 / plain[1] = `0xE42F6` | 1 / 1 |
  | floor hold 1.0 instead of 3.0 | 0 (not observable: every floor stream's `DC00` replaces the hold) |
  | `0x346F8` without `+1` / other side / no `0x36870` | 1 / 2 / 3 |
  | `0x35050` / `0x350D0` passes the side | 1 / 1 |
  | `0x350D0` / `0x193B0` never zeroes `+0x14` | 1 / 1 |
  | `0x193B0` passes its own slot (`ctx[2]`) | 1 |
  | `0x39B30` divides by `0xBED38` / case 2 by `0xBED10` | 1 / 1 |

### 21.4 Measured

With the fix, the `PR_T11` trace (temporary, reverted) reads: f = 165
state 9/`0x0B`/0, `+0x74` 666, `rec+8` `0xD2906` (the lying loop) at hold 2.0
through f = 205; f = 206 `0x346F8` → `0x36870` (`+0x74` 0, `+0x10` 0), then
9/8 from f = 207. The whole-run miss probe now lists `0x35938` (61 hits, first
f = 201), `0x4AC18` (f = 206, 302, 362) and `0x3640C` (f = 598); `0x347B8`,
`0x346F8` and `0x370F0` no longer miss.

| measurement | before (`f1023d5`) | after (`3fee8d0`) |
|---|---|---|
| captures 950..991 | 950: best 581/582 splice, 1 731 px; 951..983 1 788–16 754 px nearest-frame | **all explained** (clean or splice) |
| demo oracle first unexplained | 950 (raw 3857); `[950..3616]` 2667 / 2661 unexpl.; port `[582..1380]` | **992 (raw 3899)**; `[992..3616]` 2625 / 2619 unexpl.; port `[618..1380]` (763, 0 exhibited) |
| demo-fight ratchet | `[950..1884]` 935, N = 950 | **`[992..1884]` 893**, "ratchet improved: first unexplained 992 > 950", **N = 992** |
| front-end oracle | `[560..949]` / 390 / 150 clean, 236 splice, 0 transition, 2 unexpl. | **`[560..991]` / 432 / 170 clean, 258 splice, 0 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. Unmoved: title `54/55/2/0` and
`54/57/0/0`, determinism 54; smk 120/120 and 41/41; attract 215/216
(expected divergence at 215); C-vs-Python 9866; `symbols.h`; the front-end
"endpoints BAD" line. The exhibition set grows to port frames 0..617 (381
exhibited). The ladder, on `3fee8d0`'s tree,
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify && make demo-oracle`
exited 0 with 0 compiler warnings and "all checks passed".

**Not confirmed by the oracle.** `0x346F8` runs at f = 206 (port frame 622),
after the new first unexplained frame; its effect is asserted by the unit
test only.

### 21.5 The new first unexplained frame, 992 (characterised, not fixed)

Capture 991 is a clean 616/617 splice. Capture 992 is a tear: its best
splice, port 617/618 (split at row 134), leaves 6 342 px in x 64–319, rows
134–192; 993's best (618/619, row 188) leaves 13 800 px in rows 84–199, and
from 994 the residual covers the whole frame (37 260 px). A background shift
search (rows 60–119, x 40–279) puts the capture's scene 1 px right of the
port's at 991/992 and 1–2 px left from 994 to 1 000, with 2 094–4 615 px of
that band still differing at the best shift (the fighters and the parallax
layers): a horizontal camera and position divergence, not the raptor's lying
pose. Port frame 617 is f = 201, the frame of
the first `fn_resolve` miss after this fix: the T-rex's record (side 0,
`rec+8` `0xE6EEC`) reaches the unregistered animation-opcode target
`0x35938` (operand `0x0C`), which then misses 61 times through the run. That is
the candidate owner; the next miss, `0x4AC18` at f = 206, is also inside
993..996. Neither is derived here, and the closure is not measured.
(Derived since, §22: `0x35938` is the cause, and porting it explains
captures 992..997.)

## 22. The walk entry `0x35938` at capture 992 (roar-timing Task 12, `ff38dcc`)

**Result in one line.** Capture 992 has one cause, and it is the port's. At
f = 201 the T-rex's record (side 0, in state `0x0E`) reaches
`D500 5938 0003` at `0xE6EE8` (opcode `0x15`, mode `0x4000`, the dword
`0x00035938` at `0xE6EEA`), and the port's `fn_resolve(0x35938)` returned
NULL, so the dispatcher skipped it and the stream played on into the ids
`0x0F7F`, `0x0F7E` and `D500 7068 0003` (`0x36870`, which put the slot in
state 0 at f = 208), a loop that kept the T-rex in place. The raw `0x35938` puts the slot in state 1/0 and seeks the
record to the literal sprite id of `0x35C1C`'s frame table, so the state-1
handler `0x359E0` walks the T-rex forward and the camera follows. Porting
`0x35938` (166 B, one function, its only callee `0x2BCF4` already ported)
explains captures 992..997.

### 22.1 The measurement and the trace (temporary, reverted)

**The capture.** `splice.py` on `5b79136` (port frames 605..640): 990 and 991
explained; 992 617/618 (row 134) 6 342 px in x 64–319, rows 134–192; 993
618/619 13 800 px; 994..1000 no splice closer than 638/639 at 18 612–38 709 px
(whole frame). §21.5's background shift search already put this down as a
horizontal camera and fighter-position divergence.

**The port.** A `getenv("PR_T12")` line at the end of `fight_arena_frame`
printed for f = 180..240 the camera (`DS_000F0AF0`, `DS_000F0AEC`) and per
side `+0x52/+0x53/+0x54`, `+0x10`, `+0x2C/+0x30`, `rec+8`, `rec+0x18`,
`rec+0x20/+0x24`, `rec+0x52`, `rec+0x58`, `rec+0x28`, `+0x41` and `+0x43`,
and `anim_indirect` printed every opcode-target call (frame, target, record,
`rec+8`, operand, resolved or `MISS`). Both have been reverted.

| f | T-rex (side 0) on `5b79136` | camera x | T-rex with the fix | camera x |
|---|---|---|---|---|
| 199 | `0E/00/00`, x 14 602, `rec+8` `0xE6ED8` | 7 882 | same | 7 882 |
| 200 | x 14 858, `0xE6EDA` | 8 458 | same | 8 458 |
| 201 | miss `tgt=35938 rec8=e6eec`; stays `0E`, `rec+8` `0xE6EEC` | 8 714 | **`01/00/00`**, x 15 370, `rec+8` `0x0F80`, `rec+0x28` `0x0901`, `rec+0x58` 1 | 8 714 |
| 202..207 | `0E`, x 14 858 → 14 602 | 8 714 | `01`, frames `0x0F81`..`0x0F83`, x 15 690 → 16 330 | 9 226 → 10 186 |
| 208 | `D500 7068` → `0x36870`: state `00`, x 13 706 | 8 714 | `01`, x 16 650 | 10 186 |
| 209..219 | the `00`/`0E` loop again (miss at 211, 221, …) | 8 714 → 9 127 | `01`, x 16 650 → 17 994 | 10 506 → 11 850 |

**`rec+0x28` at f = 201.** `0x35938` leaves `0x0905` (`0x0101 | 0x804`, as
check A's direct call gives from `0x0115`), and the trace's `0x0901` is read
after the rest of that frame's record sync. A temporary watch (reverted) on
the T-rex's `rec+0x28` at f = 200..202 logged: the seek inside `0x35938`
`0901 → 0901`, the opcode target `0101 → 0905`, then `0x2A39C` `0905 → 0901`,
and `0901` at the end of `fight_arena_frame`. The raw per-record sync
`0x2A1FC` (the port's `actor_sync`) runs `0x2AA70` (the frame timer, which
dispatches the opcode) and then tests `rec+0x28` bit 2 (`0x2A262 and al,4`)
and, when set, calls `0x2A39C` (`0x2A26E`), whose `0x2A3B2 and ah,0xfb`
clears it while reloading the id. So both values are right: `0x35938`'s
`|= 0x804` sets bit 2 to request exactly that reload.

On `5b79136` the whole run missed `0x35938` 61 times, from both fighters'
streams (`rec+8` after the dword: `0xE6EEC` 12, `0xE6E9C` 28, `0xD21CA` 15,
`0xD222A` 6); with the fix it is called 10 times (f = 201, 293, 330, 399, …,
927), all resolved (the run's later frames differ, so the counts are not
comparable one for one).

### 22.2 The raw (Ghidra `read_memory` + capstone, fixups applied)

`0x35938` has no Ghidra function, and `get_xrefs_to 0x35938` returns no
references (0). A scan of the data object finds the dword `0x00035938` 14
times, each after a `D500` word (two per character): `0xD21C8`, `0xD2228`,
`0xD3EBE`, `0xD3F0E`, `0xE06AC`, `0xE06FC`, `0xE3A60`, `0xE3AB0`, `0xE6E9A`,
`0xE6EEA`, `0xEA69A`, `0xEA6E8`, `0xECC7E`, `0xECCBE`. The T-rex's stream
around it (`read_memory 0xE6ED8`): ids `0x0F7E 0x0F7F`, `DC00 000E5418`
(`0xE6EDC`), `DA00 000E58D8`, `D500 00035938` (`0xE6EE8`), ids
`0x0F7F 0x0F7E`, `D500 00036870` (`0xE6EF2`), then `0x0F80 0x0F81 …`
(`0xE6EF8`).

```
0x35938  push ebx ; push ecx ; push edx ; ebx = rec
0x3593d  eax = [rec+0x14] (the owner slot) ; je 0x359da if 0 (return)
0x35948  dl = [slot+0x54] ; cmp dl,4 ; je 0x3595a
0x35950  [slot+0x52] = 1 ; [slot+0x53] = 0 ; jmp 0x3595e
0x3595a  [slot+0x52] = 8                                   ; +0x53 kept
0x3595e  test byte [slot+0x43],2 ; je 0x35988
0x35964  tbl = [0xC8A68 + char*4] ; [rec+0x29] |= 8 ; i = [rec+0x4F] sar 24
0x35988  tbl = [0xC8AE0 + char*4] ; [rec+0x29] |= 8 ; i = [rec+0x4F] sar 24
0x359aa  edx = word [tbl + i*2] & 0xFFFF ; 0x2BCF4(rec, edx)
0x359b7  [rec+0x52] = 0 ; dword [rec+0x20] = 0 ; [rec+0x58] = 1
0x359c6  dx = [rec+0x28] ; fld [rec+0x20] ; or edx,0x804 ; fstp [rec+0x24] ; [rec+0x28] = dx
0x359da  pop edx ; pop ecx ; pop ebx ; ret
```

`i` is the signed byte `rec+0x52` (`sar 0x18` of the dword at `rec+0x4F`).
The tables are `0x35C1C`'s (the port's `FIGHT_35C1C_SEEK_A/B`):
`0xC8AE0[0]` = `0xE6EF8` (`0x0F80 0x0F81 0x0F82 0x0F83 …`),
`0xC8A68[0]` = `0xE6EA8` (`0x0F66 …`), `0xC8AE0[3]` = `0xD2238` (`0x1727 …`).
`0x2BCF4` stores EDX in `rec+8`, clears `rec+0x28` bits 2/4 and loads the id
through `0x2A408`, whose `rec+0x29` bit 3 arm returns the word at `rec+8`
itself, so `rec+8` is a literal id from here on (the per-frame `0x35C1C`
seeks the next one). The fld/fstp copies `+0.0` into the hold. EDX is not an
input: DL is written at `0x35948` and EDX at `0x3596F`/`0x35993` before any
read, so the port's `(rec, arg)` wrapper drops the operand.

**Dispatch.** Opcode `0x15` (`0x2B5E3`) returns 2. In the per-frame walk
(`0x2AA70`) that returns before the id path, so the seek's id stands (the
trace's `rec+8` `0x0F80` at f = 201). In `0x2BC30` the status-2 arm adds 2 to
`rec+8` (`0x2BCC0 add dword [ecx+8],2`) before `0x2A408`, so a begin that
walks straight into `0x35938` loads id + 2 (asserted in §22.3 E).

### 22.3 The fix and its assertions

* **Fix** (`port/src/game/fighter.c`, `fighter.h`, `actors.c`):
  `fighter_35938(rec)` (exported), registered at `0x35938` through the
  `anim_code_35938` `(rec, arg)` wrapper. It reuses `0x35C1C`'s two table
  `#define`s.
* **Frame-950 review minors.** (1) `fighter_347b8`'s header (and
  `fighter.h`): the dword `0x000347B8` is at `0xD2B02`, after the `D500` word
  at `0xD2B00`. (2) The `0x347B8`/`0x346F8` wrappers now say the raw "does not
  read EDX" and name the pushes (`0x347B8` pushes EBX, ECX, EDX, ESI and
  zeroes EDX at `0x347CE`; `0x346F8` pushes EBX, EDX and overwrites EDX at
  `0x346FD`). (3) README and `game_flow.md` keep `0x370F0` among the open gaps
  as "still unregistered; not reached (no longer misses) in this run". (4)
  `task-11-report.md` §2 is corrected: `0xED2CE` begins `8E40 DA00 … DC00`
  and `0xEAEDE` begins `DA00 … DC00`, so not every floor stream *starts* with
  `DC00` (§21.3's wording, "every floor stream's `DC00` replaces the hold",
  was right). (5) `check_knockdown_floor` part D asserts the stun spawn's
  ECX layer directly: the descriptor's `+0x08` word is `0x2200`
  (`read_memory 0xBDB3C`/`0xBDB78`), bit 13 set, so `0x2AE14` stores ECX =
  `0xFF` (`0x3420D mov ecx,0xff`) in the record's `+0x49`, seeded `0x77`
  in part D's per-side setup (fix round 1, `c36880f`: the seed had landed in
  `check_knockback_pose`, so side 1 inherited side 0's `0xFF`).
* **Assertions** (`test_fight.c`): the registration (through the wrapper) in
  `check_anim_hold_scaler`, and the new `check_walk_entry` with `we_seed`
  (the demo T-rex at f = 201: side 0, char 0, `0x0E`/`0x66` with `+0x54` a
  `0x22` sentinel for the trace's 0 (fix round 1), `+0x43`
  `0x81`, `rec+0x52` 0, `rec+0x58` `0xFF`, hold 2.0, `rec+0x28` `0x0101`;
  side 1 seeded to prove it untouched):
  * A, the direct call: state 1/0 with `+0x54` kept (`0x22`), `rec+8` and the pset id
    `0x0F80`, `rec+0x28` `0x0115` → `0x0905` (the seek's bits 2/4 cleared,
    then `0x804`), `rec+0x52` 0, `rec+0x20`/`+0x24` 0, `rec+0x58` 1; side 1
    untouched
  * B, the index and the tables: frame 3 → `0x0F83`; frame −1 → `0x0003`
    (the word at `0xE6EF6`, not `0xE6EF8 + 0x1FE`); `+0x43` bit 1 →
    `0x0F66`; char 3 on side 1 → `0x1727` with side 0 untouched
  * C, `+0x54` = 4: state 8, `+0x53` kept, the seek and step still run
  * D, `rec+0x14` = 0: nothing written
  * E, through the dispatcher (`D500 5938 0003` walked by `0x2BC30`): state
    1/0, `rec+0x20`/`+0x24` 0 over the begin's 1.0, `rec+8` and the pset id
    `0x0F82` (`0x2BCC0`'s +2)
* **Mutations.** `mut12.py` applied each one to `fighter.c`/`actors.c`,
  rebuilt and ran the suite; both files were restored byte for byte, and the
  suite then passed. Counts are real `FAIL` lines (the summary line
  excluded).

  | mutation | failures |
  |---|---|
  | `0x35938` unregistered | 1 (the registration check; the test's fallback registers it) |
  | no `rec+0x14` = 0 return (slot 0 instead) | 7 |
  | `+0x54 == 4` never taken / inverted | 2 / 8 |
  | state 8 also clears `+0x53` / state 1 keeps it | 1 / 3 |
  | table select inverted | 9 |
  | no `rec+0x29` bit 3 / set after the seek | 2 / 2 |
  | unsigned frame index / index from the slot's `+0x52` | 1 / 10 |
  | no `rec+0x52` reset | 1 |
  | no `rec+0x20` zero / no `rec+0x24` copy | 4 / 2 |
  | `rec+0x58` = 0 | 2 |
  | `rec+0x28 |= 0x800` / no `|= 0x804` | 1 / 1 |
  | slot of the other side (from `rec+0x51`) | 24 |
  | `0x34168` layer ECX `0xFE` (minor 5) | 2 |
  | fix round 1: `0x2AE14` skips the `+0x49` layer store | 9 (both sides of part D, `0x77` ≠ `0xFF`, plus seven `test_game.c` layer checks) |
  | fix round 1: the state-1 arm also writes `+0x54` = 0 | 1 |

### 22.4 Measured

| measurement | before (`5b79136`) | after (`ff38dcc`) |
|---|---|---|
| captures 992..997 | 992: 617/618 splice, 6 342 px; 993 13 800 px; 994.. whole-frame | **all explained** (992 617/618, 993 618/619 and 994 619/620 splices at rows 134, 160, 188; 995 = port 620 and 996 = port 621 clean; 997 621/622 at row 81) |
| demo oracle first unexplained | 992 (raw 3899); `[992..3616]` 2625 / 2619 unexpl.; port `[618..1380]` | **998 (raw 3905)**; `[998..3616]` 2619 / 2613 unexpl.; port `[623..1380]` (758, 0 exhibited) |
| demo-fight ratchet | `[992..1884]` 893, N = 992 | **`[998..1884]` 887**, "ratchet improved: first unexplained 998 > 992", **N = 998** |
| front-end oracle | `[560..991]` / 432 / 170 clean, 258 splice, 0 transition, 2 unexpl. | **`[560..997]` / 438 / 172 clean, 262 splice, 0 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. Unmoved: title `54/55/2/0` and
`54/57/0/0`, determinism 54; smk 120/120 and 41/41; attract 215/216
(expected divergence at 215); C-vs-Python 9866; `symbols.h`; the front-end
"endpoints BAD" line. The exhibition set grows to port frames 0..622 (386
exhibited). The ladder
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify && make demo-oracle`
exited 0 with 0 compiler warnings and "all checks passed", with N = 998 in the
Makefile.

**Unresolved code targets after the fix** (a temporary whole-run probe in
`fn_resolve` itself, reverted; `f` is `DS_0010150C`): the animation-opcode
targets `0x4AC18` (8 hits, first f = 206) and `0x3640C` (2, f = 304), and
`0x3C0A4` (1, f = 333), `0x14EF8` (1, f = 400) and `0x3A820` (491, first
f = 473), besides the known front-end effect sites `0x29B74`/`0x41578` and the
type-table stub `0x5D812`. `0x35938` no longer misses. `0x370F0` is still
unregistered and is not reached (no longer misses) in this run; `0x347B8`'s
stun-stream target `0x34530` is unregistered and not reached. The run moved,
so these first frames differ from §21.4's.

### 22.5 The new first unexplained frame, 998 (characterised, not fixed)

Capture 997 is a 621/622 splice. Capture 998's best splice, port 622/623
(split at row 107), leaves 259 px, all in the left-edge worshipper (x < 40,
rows ≈ 130–175); 999..1006 leave 481–651 px there on consecutive splices.
Side by side (998/622, 1002/626, 1006/630) the capture's worshipper turns and
walks while the port's keeps cheering, arms up. Port frame 622 is f = 206, the
frame of the first `fn_resolve` miss after the fix: a pool record (`rec+8`
`0xEE0A0`, after `D500 AC18 0004` at `0xEE09C`) reaches the unregistered
animation-opcode target `0x4AC18`. The dword `0x0004AC18` occurs 24 times in
`0xEE09E..0xEF62E`; `0x4AC18` (no Ghidra function) takes `rec+0x14` (the
effects-list entry `0x49617` stores there) and calls `0x4AC38(entry,
rec+0x48 − 0x20)`, which clears the entry's actor's `+0x29` bit 6, sets bit 4,
zeroes its `+0x34/+0x36/+0x38`, clears `entry+0x1E` and takes a stream from
`0xC9544`. That is the candidate owner; it is **not derived here**, and its
closure is not measured. From capture 1007 (port 630, f = 214) a second,
whole-frame divergence joins: the port's raptor, back up from its get-up at
f = 206, enters state 3 at f = 207 and 4 at f = 209 and leaps forward, and the
capture's raptor rises differently; not characterised further.
(Corrected in §23.4: the 1007 divergence is the T-rex's, side 0, not the
raptor's. It follows from the RNG draws `0x4AC18` adds at f = 207.)
(Derived since, §23: `0x4AC18` is the cause, and porting it explains
captures 998..1357, including the 1007 divergence.)

## 23. The worshipper arrival target `0x4AC18` at capture 998 (roar-timing Task 13, `b5a48a0`)

**Result in one line.** Capture 998 has one cause, and it is the port's. At
f = 206 the left-edge worshipper's actor (pool record `0x2A7ED80`, its
fight-effect entry `0x1083CC` in type 8, a cheer) reaches `D500 AC18 0004` at
`0xEE09C` (opcode `0x15`, mode `0x4000`, the dword `0x0004AC18` at
`0xEE09E`). The port's `fn_resolve(0x4AC18)` returned NULL, so the dispatcher
skipped it and walked on into the next cheer stream at `0xEE0A2`, and the
worshipper kept cheering. The raw `0x4AC18` calls the already-ported arrival
`0x4AC38` for the actor's `+0x14` entry: the entry returns to type 0, and the
actor begins its `0xC9544` stream. The entry's type-0 handler `0x4AAD0` then
walks it. Porting `0x4AC18` (29 B, one function, its only callee already
ported) explains captures 998..1357.

### 23.1 The measurement and the trace (temporary, reverted)

**The capture.** `splice.py` on `0e489c6` (port frames 615..640): 996 and 997
explained; 998 622/623 (row 107) 259 px; 999..1006 481–651 px on consecutive
splices, all in the left-edge worshipper (x ≈ 0–33, rows ≈ 130–176, plus a
few pixels of the splice row); 1007 3 514 px and 1008 11 573 px (whole
frame).

**The port.** Two `getenv("PR_T13")` probes printed data, and both have been
reverted:
* One sat at the end of `fight_arena_frame`. For f = 195..240 it printed the
  camera, `DS_00108874` and every `DS_0010884C` entry: type, actor, `rec+8`,
  x, `+0x34`, `+0x29`, `+0x48`, hold, `entry+0x14`/`+0x21` and the actor's
  `+0x14`.
* One sat in `anim_indirect`. It printed every opcode-target call: frame,
  target, record, `rec+8`, operand, `rec+0x14`, `rec+0x48`, and whether the
  target resolved or missed.

At f = 204..206 the entry `0x1083CC` (side 0) was in type 8 with its actor
`0x2A7ED80` at x −3 729, `+0x34` 0, `+0x48` `0x20`, `rec+8` `0xEE090`..`0xEE0A0`
and hold 3.0. The miss line read
`f=206 tgt=4ac18 rec=2a7ed80 rec8=ee0a0 arg=e r14=1083cc r48=20 MISS`, and
`rec+8` then walked `0xEE0A4`, `0xEE0A6` (the stream at `0xEE0A2`, the next
cheer), with the entry in type 8 through f = 240.
The actor's `+0x14` is the entry (`0x49617`), and `entry+8` is the actor
(`0x49614`). On `0e489c6` the whole run missed `0x4AC18` 8 times (f = 206,
302, 343, 351, 362, 411, 451, 481) from four worshippers (`+0x48` `0x20`,
`0x21`, `0x23`, `0x24`).

### 23.2 The raw (local dump of Ghidra `read_memory`, fixups applied; capstone)

`0x4AC18` has no Ghidra function, and `get_xrefs_to 0x4AC18` returns no
references (0). `read_memory 0x4AC18` begins `53 52 8b 50 14 85 d2 74 11 89 d3
31 d2 8a 50 48`. A scan of the data object finds the dword `0x0004AC18` 24
times, each after a `D500` word: `0xEE09E`, `0xEE0EC`, `0xEE10A`, `0xEE3E2`,
`0xEE42E`, `0xEE47C`, `0xEE4AC`, `0xEE722`, `0xEE798`, `0xEE7E6`, `0xEE816`,
`0xEEADE`, `0xEEB3C`, `0xEEB8A`, `0xEEBDE`, `0xEEED0`, `0xEEF30`, `0xEEF7E`,
`0xEEFB8`, `0xEF286`, `0xEF2FE`, `0xEF34C`, `0xEF39A`, `0xEF62E`.
`read_memory 0xEE09C` gives `00 d5 18 ac 04 00 40 cd`.

```
0x4ac18  push ebx ; push edx
0x4ac1a  edx = [eax+0x14] (the entry) ; test ; je 0x4ac32 (return)
0x4ac21  ebx = edx ; edx = 0 ; dl = [eax+0x48] ; edx -= 0x20 ; eax = ebx
0x4ac2d  call 0x4ac38                       ; EAX = entry, EDX = index
0x4ac32  pop edx ; pop ebx ; ret
```

The index is `(u32)(u8)[rec+0x48] − 0x20`, a 32-bit value (`xor edx,edx`
then `mov dl`, zero-extended). It is read from EAX's record, not from
`entry+8`. The effects pass's own index is the u16 `si` (`0x49CE1`); the
two agree for `+0x48` ≥ `0x20`. EDX is pushed and overwritten
(`0x4AC1A`) before any read, so the port's `(rec, arg)` wrapper drops the
operand.

`0x4AC38` was already ported as `fight_4ac38`, for the effects pass's
type-1 arrival at `0x49D86`. It is unchanged:
* `entry+8`'s actor: `+0x29 &= 0xBF`, `|= 0x10`, `+0x34/+0x36/+0x38` = 0
* `entry+0x1E` = 0
* `0x2BC30(actor, [0xC9544 + index*4], push 5.0)`

`get_xrefs_to 0x4AC38` lists nine calls:
* `0x4AC2D` (this function)
* `0x49D86` and `0x49DA9` (`0x49C78`)
* `0x4C074` and `0x4C097` (`0x4BF18`)
* `0x4D382`, `0x4D412`, `0x4D435` and `0x4D6E0` (`0x4D2D0`)

`read_memory 0xC9544` begins `0xEE02C`, `0xEE3E6`, `0xEE726`, `0xEEAE2`,
`0xEEED4`, `0xEF28A`, one idle stream per worshipper descriptor.

**Dispatch.** Opcode `0x15` (`0x2B5E3`) returns 2. In the per-frame walk
(`0x2AA70`) that returns before the id path, so the id that `0x4AC38`'s nested
`0x2BC30` loaded stands. In a `0x2BC30` begin, the status-2 arm adds 2 to
`rec+8` (`0x2BCC0 add dword [ecx+8],2`) after the nested begin, so it loads
the new stream's second word (asserted in §23.3 F).

### 23.3 The fix and its assertions

* **Fix** (`port/src/game/fight.c`, `fight.h`, `actors.c`):
  * `fight_4ac18(rec)` (exported) sits next to the static `fight_4ac38`.
  * It is registered at `0x4AC18` through the `anim_code_4AC18` `(rec, arg)`
    wrapper. For that, `actors.c` now includes `game/fight.h`.
* **Assertions** (`test_fight.c`):
  * The registration (through the wrapper) is checked in
    `check_anim_hold_scaler`.
  * The new `check_worshipper_arrival` uses `wa_seed`: the demo worshipper at
    f = 206, with entry type 8, `+0x48` `0x20`, and sentinels in `rec+8`, the
    hold 3.0, `+0x34/+0x36/+0x38`, `+0x29` `0x4B` and the pset word. A second
    actor `oth` is seeded too. The `0xC9544` table and the dwords at
    `0xC9540`/`0xC9744`/`0xC9344` hold crafted literal-id streams and are
    restored afterwards. Its parts:
    * A, the direct call: the entry goes to type 0, the speeds to 0 and
      `+0x29` to `0x13`. `rec+8` is the index-0 stream, the hold is 5.0 and
      the pset id is `0x0120`. `oth` is untouched.
    * B: `+0x48` `0x22` begins `0xC9544[2]`.
    * C: with `entry+8` = `oth`, the arrival acts on `oth` using `rec`'s
      `+0x48` (`0x21`), and `rec` is untouched.
    * D, the index width: `0x1F` reads `0xC9540` (index −1), and `0xA0` reads
      `0xC9744`, not the sign-extended `0xC9344`.
    * E: `rec+0x14` = 0 writes nothing, including linear `0x1E`/`8`, which the
      missing test would write through entry 0.
    * F, through the dispatcher (`D500 AC18 0004` walked by `0x2BC30` at 1.0):
      type 0, hold 5.0, `rec+8` = the stream + 2, pset id `0x0220`.
* **Mutations.** `mut13.py` applied each one to `fight.c`/`actors.c`,
  rebuilt and ran the suite. Both files were restored byte for byte, and the
  suite then passed. Counts are real `FAIL` lines (the summary line
  excluded).

  | mutation | failures |
  |---|---|
  | `0x4AC18` unregistered | 1 (the registration check; the test's fallback registers it) |
  | no `rec+0x14` = 0 return | 2 |
  | index without `− 0x20` | 12 |
  | index sign-extended / u16 | 2 / 2 |
  | index from `entry+8`'s `+0x48` / from `rec+0x49` | 2 / 12 |
  | acts on `rec`, not `entry+8` | 12 |
  | entry from `rec+0x10` | 27 |
  | `0x4AC38` hold 4.0 / keeps the entry type | 5 / 7 |

### 23.4 Measured

With the fix, port frames 0..622 are byte-identical to `0e489c6`'s, and
623 (f = 207) is the first that differs. The `anim_indirect` probe shows
`0x4AC18` called 3 times (f = 206, 439, 849), all resolved. The only
opcode-target miss left is `0x3640C` (f = 741).

| measurement | before (`0e489c6`) | after (`b5a48a0`) |
|---|---|---|
| captures 998..1357 | 998 259 px; 999..1006 481–651 px; 1007 3 514; 1008 11 573 | **all explained** (998..1010 splices at 0 px; the front-end window now ends at 1357) |
| demo oracle first unexplained | 998 (raw 3905); `[998..3616]` 2619 / 2613 unexpl.; port `[623..1380]` | **1358 (raw 4265)**; `[1358..3616]` 2259 / 2253 unexpl.; port `[931..1380]` (450, 0 exhibited) |
| demo-fight ratchet | `[998..1884]` 887, N = 998 | **`[1358..1884]` 527**, "ratchet improved: first unexplained 1358 > 998", **N = 1358** |
| front-end oracle | `[560..997]` / 438 / 172 clean, 262 splice, 0 transition, 2 unexpl. | **`[560..1357]` / 798 / 307 clean, 484 splice, 3 transition, 2 unexpl. (832, 833)** |

The three transition frames (`port832@row177`, `port862@row189`,
`port906@row31`) all lie in the new span. Port frames 0..622 are unchanged,
so the old window's classification is unchanged. Only the front-end window
and N moved. Unmoved: title `54/55/2/0` and `54/57/0/0`, determinism 54;
smk 120/120 and 41/41; attract 215/216 (expected divergence at 215);
C-vs-Python 9866; `symbols.h`; the front-end "endpoints BAD" line. The
exhibition set grows to port frames 0..930 (694 exhibited). The ladder
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify && make demo-oracle`
exited 0 with 0 compiler warnings and "all checks passed", with N = 1358 in the
Makefile.

**The 1007 divergence.** §22.5 attributed it to the raptor's leap. That
was wrong: it belongs to the T-rex, and the same fix explains it through the
shared RNG. The mechanism was measured with temporary probes in `rng_next`
and at the end of `fight_arena_frame` (both reverted), over f = 200..220 on
two builds: with the fix, and with only `0x4AC18`'s registration removed.

* **f = 207.** The fixed port makes 3 draws, against 1 in the unfixed one.
  Both builds make the `rng(0x64)` draw. The fixed build adds the
  worshipper's two `rng(0x1200)` draws in `0x4B144`, which run after its
  entry returns to type 0.
* **f = 208..213.** No other draw differs, and both fighters' `+0x52/+0x53/+0x54`
  are identical in both builds.
* **f = 214, the next draw.** This is exactly the capture-1007 frame (port
  630). Both builds draw `rng(0x64)`, but from different generator states.
  * Side 0, the T-rex, leaves state 1 in the fixed port: `00/00/00` at
    f = 214, `05/00/01` at 215, `09/00/00` at 217, then `09/07/02` from
    218.
  * In the unfixed port side 0 stays in `01/00/00`.
  * Side 1's states are the same in both builds (`04/08/02` at f = 213/214).

The dumped port frames are identical through 622, and the first difference
is `frame_0623` (f = 207). The worshipper's walk accounts for the pixels from
there. The T-rex's divergence from f = 214 accounts for capture 1007.

**Unresolved code targets after the fix** (a temporary whole-run probe in
`fn_resolve`, reverted):
* `0x3D17C` (3 hits, first f = 559)
* `0x14EF8` (f = 582)
* `0x3640C` (f = 741)
* `0x3A588` (180 hits, first f = 784)
* the known front-end sites `0x29B74`/`0x41578` and the stub `0x5D812`

`0x4AC18` no longer misses. `0x3C0A4` and `0x3A820` are no longer reached in
this run. `0x370F0` and `0x34530` are still unregistered and not reached.
The run moved, so these frames differ from §22.4's.

### 23.5 The new first unexplained frame, 1358 (characterised, not fixed)

Capture 1357 is a 929/930 splice (row 12). Capture 1358's best splice, port
930/931 (row 39), leaves 2 743 px in rows 39–128, in the T-rex at the right
edge. From there the residual grows: 1359 leaves 5 249 px, 1360 11 355 px,
1361 24 335 px, and from 1362 it covers the whole frame. A background shift
search (rows 60–119) matches at dx 0 / dy 0 except dx 1–3 / dy 1 at
1360/1361. Side by side (1358/931, 1361/934, 1364/936), the capture's T-rex
comes down from its leap and stands with its tail out by 1361, while the
port's is still in the air.

Port frame 930 is f = 514. A `PR_T13` fighter trace (temporary, reverted)
shows the T-rex (side 0) in state 4/0/2 up to f = 507. At f = 508 it enters
4/8/2 with hold 2.0 and `rec+8` fixed at `0xE72A2`: the `B840 000E 000E7290`
loop over `FF20`/`FF21` variable writes. It moves +218 per frame and leaves
the air only at f = 521 (`0x14/04/02`), then 9/0/0 at f = 522. The raptor
sits in 9/8. No `fn_resolve` miss other than the stub `0x5D812` falls in
f = 1..558 (the first is `0x3D17C` at f = 559), so this is not an
unregistered target. It is **not derived here**, and no candidate owner is named.
(Derived since, §24: the T-rex is not airborne too long. From f = 515 the
capture draws it two frames' worth of x (436) to the left, because the
mode-1 camera's split arm `0x12E3C` pulls it back to its `+0x38` latch and
rewrites its record's `+0x18` through `0x18714`; the port clamped only the
slot.)

## 24. The camera split arm's `0x18714` writes at capture 1358 (roar-timing Task 14, `c78dc97`)

**Result in one line.** Capture 1358 has one cause, and it is the port's. At
the end of f = 514 and f = 515 the fighters' `+0x34` latches are 20 526
apart (T-rex 26 674, raptor 6 148), more than `word[0x9AF28]` = `0x5000`
(20 480). The mode-1 camera `0x12E3C` then pulls the T-rex (slot b, the
right one, `+0x34` > `+0x38`) back to its `+0x38` latch, storing `+0x34`
and `+0x2C`, and rewrites the T-rex record's `+0x18` through `0x18714`
(`0x12F22`/`0x12F29`). The port stored the slot fields but skipped the two
`0x18714` calls, a named gap since the demo-fight cycle (demo-fight record
§2.6/§7.5). The next latch then rebuilt `+0x2C` from the unclamped record,
so the clamp had no lasting effect, and the port's T-rex ran two frames'
worth of x (2 × 218 = 436) further right. `0x18714` was already ported
(`hit_record_x`), so the fix is two calls. It explains captures
1358..1410. §23.5's "stays in the air" reading was wrong: the landing is
not late, the T-rex is misplaced in x.

### 24.1 The measurement (temporary, reverted)

**Stale probe lists re-run first.** A whole-run `fn_resolve` probe on
`306eacf`'s behaviour (temporary, in `mem.c`, reverted) gives exactly
§23.4's list: `0x3D17C` (3 hits, first f = 559), `0x14EF8` (f = 582),
`0x3640C` (f = 741), `0x3A588` (180 hits, first f = 784), plus
`0x29B74`/`0x41578` and the stub `0x5D812`. Nothing but the stub misses
before f = 559, so 1358 is not an unregistered target.

**The capture.** A splice search (`splice.py`, top from port N and bottom
from N + 1, split at any pixel; scratchpad) on the `306eacf` dump:

| capture | best splice | residual |
|---|---|---|
| 1354..1357 | 927/928 … 929/930 | 0 px |
| 1358 | 930/931 (row 39) | 2 743 px, the T-rex, rows 39–128 |
| 1359 / 1360 / 1361 | 931/932, 932/933, 933/934 | 5 249 / 11 355 / 24 335 px |
| 1362+ | – | ≥ 9 952 px, the whole frame |

On 1358 against port 931, the regions outside the T-rex are equal (x 0–79,
rows 60–198: 0 px; x 80–229: 0 px), so the camera and the raptor agree. A
shift search on the T-rex's gold pixels (x 250–315, rows 40–134; R > 90,
G > 60, R − B > 40, G − B > 20) matches port 931's T-rex exactly (0
mismatches over 425 + 347 pixels) at dx = −4, dy = 0. On a larger box
(x 236–311, rows 35–189, which also holds sand) the best shift for 13 of
the 15 captures 1358..1372 is dx −4 or −3 at dy 0, before and after the
landing, though not exact there; 1367 and 1369 fit best at dx 0 against
another port frame. So the capture draws the same sprite in the same pose,
only further left. That rules out a pose, hold or landing cause.

**The port** (`PR_T14` probes at the end of `fight_arena_frame`, in
`0x34E2C`, in `0x3531C` case 8, in the animation dispatcher for the T-rex's
record, and in `fn_resolve`; all reverted). The T-rex, side 0:

| f | state | slot `+0x2C` | `rec+0x18` | notes |
|---|---|---|---|---|
| 507 | 4/0/2 | 25 148 | 25 020 | the jump from `0x3BC70`, vx 218, gravity 28 |
| 508 | 4/8/2 | 25 366 | 25 046 | reaction `0x17` in the air (`0x34E2C` → `0x3C520`, stream `0xE7284`, hold 2.0); the raptor takes `0x10` at f = 509 |
| 509..520 | 4/8/2 | +218 per frame | +218 per frame | `rec+8` held at `0xE72A2` by `B840 000E 000E7290`; its `FF20`/`FF21` read `0xE5B58`, which is all zero |
| 521 | `0x14`/4/2 | 27 982 | 27 662 | the `0x35F84` landing gate |

The raptor's latch stays at 6 148 through f = 515 and moves to 7 748 at
f = 516, when its reaction stream re-anchors it. The case-8 gate
(`word[0xBDBE8]` = 3) runs `0x3CF38` only at f = 509/510, and it finds no
hit. `DS_00100B5A` is 0 for both sides throughout, and the anchor table
`0xCEB00` reads (5, 9) for all of the T-rex's anchors 360..380, so neither
the stance timer nor an anchor change moves it.

### 24.2 The raw (Ghidra `disassemble_function 0x12E3C`, `read_memory`)

```
0x12ed7  movzx ebp, word [0x9af28]        ; 0x5000
0x12ee2  cmp eax, ebp ; jle 0x12f44       ; |slot0+0x34 - slot1+0x34| > 0x5000
0x12ee6  eax = [edx+0x34] ; ebp = [edx+0x38] ; [0xF0AF0] = ebx
0x12ef2  cmp eax, ebp ; jge 0x12f0c       ; slot a (the lower +0x34): pulled back when +0x34 < +0x38
0x12ef6  89 6a 34   mov [edx+0x34], ebp
0x12ef9  89 6a 2c   mov [edx+0x2c], ebp
0x12efc  89 f8      mov eax, edi          ; EAX = a
0x12efe  89 6c bc 08  mov [esp+edi*4+8], ebp
0x12f02  e8 0d 58 00 00  call 0x18714
0x12f07  8b 12      mov edx, [edx]        ; slot a's record
0x12f09  89 42 18   mov [edx+0x18], eax
0x12f0c  eax = [ecx+0x34] ; edx = [ecx+0x38] ; cmp ; jle 0x12f2c   ; slot b: when +0x34 > +0x38
0x12f16  89 51 34 / 89 51 2c / 89 f0 (EAX = b) / 89 54 b4 08
0x12f22  e8 ed 57 00 00  call 0x18714
0x12f27  8b 11      mov edx, [ecx]
0x12f29  89 42 18   mov [edx+0x18], eax
```

`read_memory 0x9AF28` gives `00 50 00 17 …`, the word `0x5000`. Here
a = `(slot0+0x34 >= slot1+0x34)` (`0x12E50 setge`), EDX = slot a and ECX =
slot b. `0x18714` saves and restores EBX, ECX, EDX, ESI and EDI
(`0x18714..0x18718`, `0x18782..0x18786`), so EDX/ECX still hold the slots
after the call. With `+0x42` bit 3 clear, it returns
`slot+0x2C − DS_00100AB0[side]` after `0x18540` and, on an anchor change,
`0x18350` (§17). Since `+0x2C` has just been set to the latch, the record
lands at `latch − AB0`, and the next `0x186D0` latch keeps it there.

`get_xrefs_to 0x18714` lists seven calls: `0x12F02`, `0x12F22` (this
function), `0x13087`, `0x130E5` (`0x12FD8`), `0x18886` (`0x1883C`), `0x18AD0`
(`0x18B04`) and `0x188F8` (`0x188DC`). `0x12FD8`, the other camera caller, is
reached only from `0x24C5C` at `0x25513`/`0x25598`, for `DS_00104B00` =
`0x21` or `0x25` (the decompiled `0x24C5C`); the demo runs 3, so it stays
unported. `0x12E3C` runs from the `0x25422` camera dispatch in the
`DS_00104B15` tail, after `actors_update`.

The `+0x38` latch is the previous frame's `+0x34` (`0x2642C`/`0x2643A`,
`fight_arena_frame`). So the clamp holds the T-rex where it was one frame
earlier. It bites at the end of f = 514 and of f = 515. At f = 516 the
raptor's latch moves to 7 748, and the pair is 18 926 apart, so it stops.

### 24.3 The fix and its assertions

* **Fix** (`c78dc97`):
  * `camera.c`'s `camera_mode_track_pair` now writes
    `DSD(DSD(sa) + 0x18) = hit_record_x(a)` after the slot-a stores and
    `DSD(DSD(sb) + 0x18) = hit_record_x(b)` after the slot-b stores, in the
    raw's order. The two `PORT:` gap notes are gone, and the header comment
    states the arm.
  * `fighter.c`'s `hit_record_x` (`0x18714`) is no longer static; it is
    declared in `fighter.h`.
* **Assertions** (`test_fight.c`, the new `check_camera_split` with its
  `cs_seed`). Each case seeds both sides: `+0x34`/`+0x38`, char 0, the actor
  sprite id `word[0xE6DD0]` (`0x0EE4`) + the anchor, slot `+0x20` =
  `0xFFFFFFFF` (so `0x18350` re-derives AB0), `+0x42` = 0, and sentinels in
  `+0x2C`, `rec+0x18`, `DS_00100AF0` and `DS_00100AB0/AB4`. The anchors are
  370 → (5, 9), AB0 320, for side 0, and 381 → (4, 48), AB0 256, for side 1,
  so a wrong side argument changes the value.
  * **A, the demo at f = 514:** 26 674/26 456 against 6 148/6 148. Side 0's
    `+0x34` and `+0x2C` become 26 456, `rec+0x18` becomes 26 136, AF0 370,
    AB0/AB4 320/576, and `+0x20` stays −1. Side 1's `+0x2C`, `rec+0x18`, AF0
    and AB0 keep their sentinels. The camera commits `0x4000 − 82`.
  * **B, slot a:** side 1 at 6 000 against its latch 6 148, side 0 at
    27 000/27 000. Side 1 gets 6 148, `rec+0x18` 5 892, AF0 381 and
    AB0/AB4 256/3 072; side 0's sentinels stay.
  * **C, the `jle` gate:** exactly `0x5000` apart does not split. Nothing is
    written.
  * **D:** with `+0x42` bit 3 set, the slot is still pulled back, but
    `rec+0x18` and AF0 keep their sentinels.
* **Mutations** (`mut14.py`; `camera.c` was restored byte for byte, and the
  suite passed afterwards). Counts are real `FAIL` lines, excluding the
  summary line.

  | mutation | failures |
  |---|---|
  | no call in arm a | 4 |
  | no call in arm b | 4 |
  | arm a passes b | 6 |
  | arm b passes a | 7 |
  | arm b writes slot a's record | 2 |
  | arm b calls before its `+0x2C` store | 1 |
  | gate `>=` instead of `>` | 4 |

### 24.4 Measured

Port frames 0..930 are byte-identical to `306eacf`'s, and 931 (f = 515) is
the first that differs. With the fix, the T-rex's `rec+0x18` (probed at the
end of `fight_arena_frame`, before the `0x25422` camera tail) reads 26 354
at f = 515 and f = 516, then +218 per frame, landing at f = 521
at 27 226 (27 662 before). Captures 1358..1410 are explained: the splice
search gives 0 px for every capture 1354..1372.

| measurement | before (`306eacf`) | after (`c78dc97`) |
|---|---|---|
| captures 1358..1410 | 1358 2 743 px; 1359 5 249; 1360 11 355; 1361 24 335; whole frame from 1362 | **all explained** |
| demo oracle first unexplained | 1358 (raw 4265); `[1358..3616]` 2259 / 2253 unexpl.; port `[931..1380]` | **1411 (raw 4318)**; `[1411..3616]` 2206 / 2200 unexpl.; port `[977..1380]` (404, 0 exhibited) |
| demo-fight ratchet | `[1358..1884]` 527, N = 1358 | **`[1411..1884]` 474**, "ratchet improved: first unexplained 1411 > 1358", **N = 1411** |
| front-end oracle | `[560..1357]` / 798 / 307 clean, 484 splice, 3 transition, 2 unexpl. | **`[560..1410]` / 851 / 337 clean, 507 splice, 3 transition, 2 unexpl. (832, 833)** |

The three transition frames are the same as before (`port832@row177`,
`port862@row189`, `port906@row31`). Only the front-end window and N moved.
Unmoved: title `54/55/2/0` and `54/57/0/0`, determinism 54; smk 120/120 and
41/41; attract 215/216 (expected divergence at 215); C-vs-Python 9866;
`symbols.h`; the front-end "endpoints BAD" line. The exhibition set grows to
port frames 0..976 (740 exhibited). The ladder
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify && make demo-oracle`
exited 0 with 0 compiler warnings and "all checks passed", with N = 1411 in
the Makefile.

**Unresolved code targets after the fix** (a temporary whole-run probe in
`fn_resolve`, reverted):
* `0x3D17C` (3 hits, first f = 559)
* `0x3A588` (157 hits, first f = 807)
* the known front-end sites `0x29B74`/`0x41578` and the stub `0x5D812`

`0x14EF8` and `0x3640C` no longer miss in this run.

### 24.5 The new first unexplained frame, 1411 (characterised, not fixed)

Capture 1410 is a 975/976 splice (row 195). Capture 1411's best splice,
port 975/976 (row 0), leaves 6 363 px in rows 90–192. Both fighters differ:
the raptor at the left and the T-rex at the right. From there the residual
grows to 10 914 px at 1413 and 13 407 px at 1416. Port frame 976 is f = 560.
At f = 559 the T-rex (side 0, state 9/0/0) takes reaction `0x20` in
`0x34E2C`. Its `(char 0, 0x20)` entry at `0xA37A8` reads `7c d1 03 00 00 00
00 00`, the callback `0x0003D17C` with no stream. `0x3D17C` is still
unregistered (the §6.9 gap; `get_xrefs_to 0x3D17C` returns 0), and it is the
run's first `fn_resolve` miss after the stub (f = 559). So the port only
stores `+0x5F` = `0x20`, and at f = 560 the T-rex is back in `0/0/0` on the
stance stream `0xE6DD2`. The unregistered reaction callback `0x3D17C` is the
candidate owner. It is **not derived here**.
(Derived since, §25: the owner is `0x3D17C`, as named. Porting it with its
stream's two opcode targets `0x3D214`/`0x3D26C` explains captures
1411..1477.)

## 25. The T-rex's reaction-0x20 breath `0x3D17C` at capture 1411 (roar-timing Task 15, `4065c1d`)

**Result in one line.** Capture 1411 has one cause, and it is the port's:
§24.5's candidate is the owner. At f = 559 `0x34E2C` applies reaction `0x20`
to the T-rex. Its `(char 0, 0x20)` entry `0xA37A8` holds only the callback
`0x3D17C`, which the port had not registered, so it skipped it and the T-rex
fell back into its stance at f = 560. The raw `0x3D17C` starts the breath
stream `0xE84C8` (state `0xB/6/0`) and stores `0x100` in the word
`0x1080AC[side]`. That stream's `D100` target `0x3D214` spawns an emitter
(`0xBB27C`), whose own `D100` target `0x3D26C` spawns the projectile
(`0xBB268`, the purple ring) into the slot's `+0x08` at the speed
`−0x1080AC[side]`. The closure is three functions (112 + 88 + 186 B); their
callees are already ported (`0x3C4CC`, `0x2AE14`, `0x1A570`) or the
out-of-scope voice `0x2C3FC`. Porting them explains captures 1411..1477.

### 25.1 The measurement (temporary, reverted)

**Stale probe list re-run first.** A whole-run `fn_resolve` probe on
`db2f0f1` (temporary, in `mem.c`, reverted) gives exactly §24.4's list:
`0x3D17C` (3 hits, f = 559, 642, 716), `0x3A588` (157 hits, first f = 807),
plus `0x29B74`/`0x41578` and the stub `0x5D812`. A probe in
`hit_reaction_apply` shows the three `0x3D17C` misses are the T-rex's
reaction `0x20`, each in state `9/0/0` with slot `+0x08` = 0.

**The capture.** `splice.py` on `db2f0f1`'s dump: 1408..1410 splice at 0 px;
1411 (975/976, row 0) leaves 6 363 px in rows 90–192, 1412 6 961 px, 1413
10 914 px and 1416 13 407 px. Side by side (1411/976, 1414/980, 1418/984),
the capture's T-rex opens into the breath pose at 1411 and a purple ring
leaves its mouth from 1414, while the port's T-rex stands. (The raptor's
share of 1411's residual is a splice artefact: it is gone once the T-rex is
fixed.)

**The port** (`PR_T15` probes at the end of `fight_arena_frame`, in
`hit_reaction_apply`, `fn_resolve` and `anim_indirect`; all reverted):
`f=559 react side=0 r=20 st=09/00/00 s8=0`, then `MISS 3d17c`. The T-rex ends
f = 559 in `09/00/00` with `+0x5F` `0x20`, and at f = 560 it is in `00/00/00`
on the stance stream `0xE6DD2`, hold 3.0.

**Staged.** With only `0x3D17C` registered, captures 1411..1413 are
explained, 1414..1417 leave 168 px each (x ≈ 211–225, rows 98–123: the
ring), and `0x3D214` misses at f = 562, 675 and 741. With `0x3D214` and
`0x3D26C` registered too, every capture 1411..1440 splices at 0 px.

### 25.2 The raw (Ghidra `read_memory`, fixups applied; capstone)

`0x3D17C` has no Ghidra function; `get_xrefs_to 0x3D17C` returns 0, and the
dword `0x0003D17C` occurs once in the data object, at `0xA37A8`.
`read_memory 0x3D17C` begins `53 51 56 89 c3 89 d1 0f b6 72 51 83 78 08 00
74 06`.

```
0x3d17c  push ebx ; push ecx ; push esi
0x3d17f  ebx = eax (slot) ; ecx = edx (rec)
0x3d183  0f b6 72 51     movzx esi, byte [edx+0x51]   ; the record's side
0x3d187  83 78 08 00     cmp dword [eax+8], 0 ; je 0x3d193
0x3d18d  xor al,al ; pop esi/ecx/ebx ; ret            ; a live projectile
0x3d193  b8 91 00 00 00 / ba c8 84 0e 00 / e8 5a f2 fe ff
                         ; 0x2C3FC(0x91), EDX = 0xE84C8
0x3d1a2  mov eax,ecx ; 68 00 00 40 40 push 3.0 ; e8 1e f3 ff ff call 0x3C4CC
0x3d1ae  c6 43 52 0b / c6 43 53 06 / c6 43 54 00      ; state 0xB/6/0
0x3d1ba  c7 43 0c 00000000 / 0x3d1c1 +0x18 / 0x3d1c8 +0x1C = 0
0x3d1cf  8a 43 5f  al = [slot+0x5f] ; 0x3d1d2 c6 43 5f ff ; 0x3d1db 88 43 64
0x3d1d6  edx = 0x100 ; 0x3d1de mov al,1
0x3d1e0  66 89 14 75 ac 80 10 00   mov word [esi*2+0x1080ac], dx
0x3d1e8  pop esi/ecx/ebx ; ret
```

* **EDX reaches `0x3C4CC` as the stream.** `0x2C3FC` pushes EBX, EDX and EDI
  (`0x2C3FC..0x2C3FE`), and each of the 30 RETs in its 1 268 B follows a
  `pop edx`. So `0x3C4CC` (EAX = rec, EDX = stream, the pushed hold;
  `0x3C4D0 mov ebx,edx`) gets EDX = `0xE84C8`. With the slot's `+0x52` still
  9, it takes the `0x3C480` arm (the anchor re-latch).
* **The index is the record's side.** It is read from `rec+0x51` at
  `0x3D183`. The EBX side that `0x34E2C` passes is overwritten at `0x3D17F`.
* **The return is ignored.** `0x34E2C`'s `0x35045 call [esp+0x24]` is followed
  by `add esp,0x28 ; pop ; ret`. Neither of its callers reads EAX: `0x352CD`
  (→ `0x352D2 add esp,0x30`) and `0x3CF2E` (→ `0x3CF33 mov al,1`).
* **The twin.** `0x3D10C` is the same body, storing `0x80`
  (`0xA37BC`, the next table entry). It is not reached in this demo.
* **`0x1080AC`.** `get_xrefs_to 0x1080AC` lists `0x3D170` (the twin),
  `0x3D1E0` (this function), and `0x3D294`/`0x3D2AC` (`0x3D26C`). No
  `symbols.h` name exists, so the port uses a local `#define`.

**The stream `0xE84C8`** (`read_memory`): `1131 D100 D214 0003 0000 1132 DC00
5558 000E ED40 84F0 000E B840 0012 84DA 000E 9E00 D000 6870 0003 …`. That is
the literal id `0x1131`, then opcode `0x11` (mode `0x4000`) with the dword
`0x0003D214` at `0xE84CC`, and later `D000 6870 0003` (`0x36870`, already
registered). The data scan finds `0x0003D214` only at `0xE84CC`.

**`0x3D214`** (88 B):

```
0x3d214  push ebx, ecx, edx, esi, edi ; esi = eax (rec)
0x3d21b  8b 78 14   edi = [eax+0x14] (the slot) ; test ; je 0x3d265
0x3d222  ax = [eax+0x56] ; or ah,4 ; and eax,0xffff   ; a5 = rec+0x56 | 0x400
0x3d22e  ecx = 0 ; ebx = 0 ; push eax ; 0x3d233 xor edx,edx ; eax = 0xBB27C
0x3d23a  call 0x2AE14
0x3d23f  c6 40 59 02   [new+0x59] = 2
0x3d243  dl = [new+0x56] ; 0x3d246 [new+0x14] = edi ; 0x3d249 [rec+0x4b] = dl
0x3d24c  c6 40 60 01   [new+0x60] = 1
0x3d250  cmp byte [rec+0x51],0 ; je 0x3d265
0x3d256  dx = [new+0x2e] ; 0x3d25a [new+0x4e] = 1 ; add edx,4 ; 0x3d261 [new+0x2e] = dx
```

**The emitter.** Its descriptor `0xBB27C` reads `98 85 0e 00 00 00 08 00 00
01 20 00 00 10 00 00 3c ff 05 01`: the stream `0xE8598`, type 0, `+0x2E` = 8
and flags `0x0100`. The stream `0xE8598` reads `9302 CD40 03E8 B840 0003
000E859A D100 D26C 0003 0000 CD40 03E8 B840 0008 000E85AE 8000`:
* opcode `0x13` (`+0x59` = 2, so `0x3D214`'s own store is redundant in the demo)
* the id `0x3E8` and a 3-pass loop
* `0x3D26C` (the dword at `0xE85A8`, its only data site)
* an 8-pass loop, then opcode 0, which kills the emitter

**`0x3D26C`** (186 B):

```
0x3d26c  push ebx..ebp ; sub esp,4
0x3d275  8b 70 14   esi = [eax+0x14] (the slot) ; test ; je 0x3d31c
0x3d280  edi = [esi] (the fighter record) ; 0x3d282 xor edx,edx ; dl = [edi+0x51]
0x3d289  call 0x1A570 (EAX = side)      ; AL = 1 when the pset is not hflipped
0x3d28e  add edx,edx
         AL != 0: 0x3d294 ax = [edx+0x1080ac] ; neg -> [esp] ; edx = 0xFFFFE800
         AL == 0: 0x3d2ac ax = [edx+0x1080ac] -> [esp] ;       edx = 0x1800
0x3d2bb  ax = [edi+0x28] ; al = 0 ; ah &= 0x40 ; -> eax = 0x4000 or 0      ; a5
0x3d2d5  movsx edx,dx ; ecx = [edi+0x30] sar 16 (a3) ; ebx = [edi+0x1c] + 0x1600 (a4)
         edx += [edi+0x18] (a2) ; push eax ; eax = 0xBB268 ; 0x3d2f2 call 0x2AE14
0x3d2fa  [esi+8] = eax ; 0x3d2fd word [eax+0x34] = dx (the speed) ; 0x3d304 [[esi+8]+0x14] = esi
0x3d307  cmp byte [edi+0x51],0 ; je ; 0x3d310 add word [+0x2e],4 ; 0x3d318 [+0x4e] = 1
```

* **The sign.** `0x1A570` ends `0x1A5A4 sete al`, so AL = 1 when the pset
  word's bit 15 is clear. That is the port's `fighter_actor_bit15_clear`.
* **The projectile.** Its descriptor `0xBB268` reads `bc 85 0e 00 02 02 08 00
  80 00 20 00 00 10 00 00 3c ff 05 01`: the stream `0xE85BC`, type 2, frame 2.
  Type 2's cb2 is `0x3B9C4`, already registered. When the projectile dies, it
  clears the slot's `+0x08` and sets `+0x64` = `0xFF`, and that re-opens
  `0x3D17C`'s gate.
* **EDX is never an input.** Both targets push EDX and overwrite it before
  any read (`0x3D233`, `0x3D282 xor edx,edx`), so the `(rec, arg)` wrappers
  drop the operand.

### 25.3 The fix and its assertions

* **Fix** (`port/src/game/fighter.c`, `fighter.h`, `actors.c`):
  * `fighter_3d17c`, `fighter_3d214` and `fighter_3d26c` (exported).
  * `actors.c` registers `0x3D17C` directly, as it does `0x3E62C` (the
    `(slot, rec, side)` callback shape). `0x3D214`/`0x3D26C` go through the
    `anim_code_3D214`/`anim_code_3D26C` `(rec, arg)` wrappers.
  * The data addresses are local `#define`s, because `symbols.h` names none:
    `0xE84C8`, `0x1080AC`, `0xBB27C` and `0xBB268`.
  * `hit_reaction_apply`'s header no longer calls `0x3D17C` a gap.
* **Assertions** (`test_fight.c`):
  * The three registrations are checked in `check_anim_hold_scaler`.
  * The new `check_trex_breath` uses `tb_seed`: both sides' slots and records
    carry sentinels in every field `0x3D17C` writes, and `0x1080AC`/`AE`
    carry sentinels too. Its parts:
    * A, the demo's f = 559 through `hit_reaction_apply(0, 0x20)`: state
      `0xB/6/0`, the three callbacks 0, `+0x5F` `0xFF`, `+0x64` `0x20`,
      `0x1080AC` = `0x100` (`AE` kept), `rec+8` = `0xE84C8`, hold 3.0, id
      `0x1131`, and `rec+0x1C` = 0 (the `0x3C480` arm). Side 1 is untouched.
    * B: a live projectile writes nothing.
    * C: side 1's record with EBX = 0 writes `0x1080AE`, `+0x64` takes the
      old `+0x5F`, and `+0x52` = 0 takes `0x3C4CC`'s plain arm.
    * D, `0x3D214` on pool records, per side: the emitter is a child
      (`+0x4A`, `+0x28` bit `0x400`), with `+0x14`, `+0x60`, `+0x2E`/`+0x4E`,
      `rec+8` = `0xE859C` (the walk stops on `CD40 03E8`'s operand) and its
      index in `rec+0x4B`. It also covers the no-slot return. Its `+0x59` is
      not asserted, because the walk's opcode `0x13` stores 2 anyway.
    * E: `D100 D214 0003 0000` walked by `0x2BC30` spawns the emitter.
    * F, `0x3D26C` in three cases, plus the no-slot return: side 0
      unflipped, side 0 hflipped and side 1. Each asserts x ∓ `0x1800`, y +
      `0x1600`, the speed `∓0x1080AC[side]`, `+0x32` = a3, `+0x28`
      (`a5` = `0x4000` or 0), type 2, `rec+8` = `0xE85C0`, and `+0x2E`/`+0x4E`
      for side 1.
  * **Two existing checks adjusted.** `check_hit_reaction_drive` part A and
    `check_hit_chain` drive char 0's reaction `0x20` through `0x34E2C` and
    assert `+0x5F` = `0x20` (and `+0x52` kept). They now seed slot `+0x08`
    non-zero, so that `0x3D17C` takes its `0x3D187` return. Their assertions
    are unchanged and remain true to the raw.
* **Mutations** (`mut15.py`, 26 of them). Each was applied to `fighter.c` or
  `actors.c` in turn; the suite was rebuilt and run each time. Both files were
  restored and `cmp`-verified, and the suite then passed. Counts are real
  `FAIL` lines:

  | mutation | failures |
  |---|---|
  | `0x3D17C` unregistered | 1 (the registration check; the test's fallback registers it) |
  | no `+0x08` gate / gate on `+0x0C` | 14 / 24 |
  | index from the EBX side | 2 |
  | stores `0x80` | 2 |
  | `+0x64` after `+0x5F` = `0xFF` / no `+0x64` | 2 / 2 |
  | begins without `0x3C4CC` / hold 2.0 | 1 / 1 |
  | `+0x53` = 7 / keeps `+0x18` | 2 / 1 |
  | `0x3D214` unregistered | 4 |
  | `0x3D214` no slot gate / a5 without `0x400` | 2 / 2 |
  | `0x3D214` side test inverted / no `+0x60` / `+0x4B` = the record's own index | 4 / 2 / 10 |
  | `0x3D26C` unregistered / no slot gate | 1 / 1 |
  | `0x3D26C` sign by side / offsets swapped / no `+0x1600` | 4 / 2 / 1 |
  | `0x3D26C` a5 always `0x4000` / word index 0 / no `+0x14` / side 1 without +4 | 1 / 1 / 2 / 1 |

  The `+0x30` shift (`sar` against `shr`) is not distinguishable through the
  16-bit `+0x32`, so it is not asserted.

### 25.4 Measured

Port frames 0..975 are byte-identical to `db2f0f1`'s, and 976 (f = 560) is
the first that differs. Temporary probes (reverted) show the calls:
* `0x3D17C` at f = 559, 672 and 738. At f = 738 it returns at `0x3D187`,
  because slot `+0x08` still holds the projectile, so the gate runs in the
  demo.
* `0x3D214` at f = 562 and 675.
* `0x3D26C` at f = 572 and 685.

| measurement | before (`db2f0f1`) | after (`4065c1d`) |
|---|---|---|
| captures 1411..1477 | 1411 6 363 px; 1412 6 961; 1413 10 914; 1416 13 407 | **all explained** |
| demo oracle first unexplained | 1411 (raw 4318); `[1411..3616]` 2206 / 2200 unexpl.; port `[977..1380]` | **1478 (raw 4385)**; `[1478..3616]` 2139 / 2133 unexpl.; port `[1034..1380]` (347, 0 exhibited) |
| demo-fight ratchet | `[1411..1884]` 474, N = 1411 | **`[1478..1884]` 407**, "ratchet improved: first unexplained 1478 > 1411", **N = 1478** |
| front-end oracle | `[560..1410]` / 851 / 337 clean, 507 splice, 3 transition, 2 unexpl. | **`[560..1477]` / 918 / 382 clean, 529 splice, 3 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. The three transition frames are the
same as before (`port832@row177`, `port862@row189`, `port906@row31`). Unmoved:
title `54/55/2/0` and `54/57/0/0`, determinism 54; smk 120/120 and 41/41;
attract 215/216 (expected divergence at 215); C-vs-Python 9866; `symbols.h`;
the front-end "endpoints BAD" line. The exhibition set grows to port frames
0..1033 (797 exhibited). The ladder
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify`
exited 0 with 0 compiler warnings and "all checks passed", with N = 1478 in
the Makefile.

**Unresolved code targets after the fix** (a temporary whole-run probe in
`fn_resolve`, reverted):
* `0x14814` (f = 625): the `(char 3, 0x22)` reaction callback at `0xA46D0`,
  reached by the raptor's reaction `0x22` that frame
* `0x3ECF8` (f = 888): the `(char 0, 0x2C)` callback at `0xA3898`
* the known front-end sites `0x29B74`/`0x41578` and the stub `0x5D812`

`0x3D17C` no longer misses, and `0x3A588` is no longer reached. The run moved,
so these frames are not comparable one for one with §24.4's. The projectile
stream's `D100 7CFC 0003` (`0x37CFC`, unregistered) is not reached.

### 25.5 The new first unexplained frame, 1478 (characterised, not fixed)

Capture 1477 is a 1032/1033 splice. Capture 1478's best splice, port
1033/1034 (row 103), leaves 7 749 px in rows 103–195; 1479 leaves 13 638 px
and 1480 14 417 px, and from there it covers the whole frame. Side by side
(1478/1034, 1480/1036, 1483/1038), the capture's ring reaches the raptor at
the left edge and bursts into a vertical flash as the raptor recoils. The
port's ring flies on through the raptor's head, and the raptor keeps its
pose. The T-rex at the right edge differs too. The port takes reaction
`0x08` for it at f = 619, and that part is not separated here.

Port frame 1034 is f = 618. A `PR_T15` trace (temporary, reverted) follows
the projectile: slot 0's `+0x08`, the pool record `0x2A809F0`, type 2, its
stream looping at `0xE85CE`. It moves −256 per frame: x 11 098 at f = 615,
10 586 at f = 617 and 10 330 at f = 618, against the raptor's record x of
10 052. No hit is tested, and no `fn_resolve` miss falls in f = 560..624
(the next is `0x14814` at f = 625).

The candidate owner is the unported collision step `0x17CB0`. `0x1975C` calls
it first (`0x19763`), and the port's `fighter_think` omits that call. After
`0x15F48` it tests two live projectiles through `0x17BC8` (when both
`[0x1077B8]` and `[0x10784C]` are set), then each side's projectile through
`0x176CC` (`0x17D0E` for `[0x1077B8]`, `0x17D21` for `[0x10784C]`). §19.6
sized it at 1 070 B in 4 functions. It is **not derived here**.
(Derived since, §26: the owner is `0x17CB0`, as named. Porting it with its
`0x176CC`/`0x17BC8` tests and the `0x3B464`/`0x3B938` hit it wakes explains
captures 1478..1480.)

## 26. The projectile collision step `0x17CB0` at capture 1478 (roar-timing Task 16, `3d64c61`)

**Result in one line.** Capture 1478 has one cause, and it is the port's:
§25.5's candidate is the owner. `0x1975C`'s first call, `0x19763 call
0x17CB0`, was not ported, so `DS_00100AD0/AD4` (the per-thrower projectile
overlap counts) stayed 0 and the think step never applied a projectile hit.
With `0x17CB0` → `0x176CC` ported, at f = 617 the T-rex's breath ring
(slot 0's `+0x08`) over the raptor gives `AD0[0]` = 17 > 2; `0x1975C` then
runs `0x3B464` for the raptor (the `0x39834` pose driver and the grounded
`0x3A95C` stagger, stream `0xC8FE0[3]` = `0xD267E`) and bursts the ring
(`0x3B938`, stream `0xBDFC8[0]` = `0xE85E0`). This explains captures
1478..1480. The new closure is `0x17CB0` 125 B, `0x176CC` 574 B, `0x17BC8`
231 B, `0x3B938` 140 B and `0x3A95C` 122 B (1 192 B in 5 functions), plus
the mode-0..3 row arms of the already-ported `0x16DA4`; every other callee
was already ported. §19.6's "4 functions" missed `0x3A95C`, which the woken
`0x3B464` reaches at `0x3B5A9`.

### 26.1 The measurement (temporary, reverted)

**Stale probe list re-run first.** A whole-run `fn_resolve` probe on
`8f2d86b` (temporary `PR_T16`, in `mem.c`, reverted) gives exactly §25.4's
list: `0x14814` (f = 625), `0x3ECF8` (f = 888), `0x29B74`/`0x41578` and the
stub `0x5D812`. Nothing misses in f = 560..624, so 1478 is not an
unregistered target.

**The port** (`PR_T16` probe at the end of `fight_arena_frame`, reverted):
slot 0's projectile `0x2A809F0` (type 2, `+0x48` = 2, `+0x56` = `0x54`)
moves −256 per frame, x 11 098 at f = 615 to 10 330 at f = 618, against the
raptor's record x 10 052; `DS_00100AD0/AD4` read 0/0 on every frame and the
T-rex's slot `+0x64` stays `0x20` (from `0x3D17C`, §25).

**Staged, after the port** (the same probe plus prints in `fighter_think`,
reverted): at f = 617 `think i=0 ad0=17`, `0x3B464` for side 1 with stance
`0x20`, `+0x54` 0, projectile `+0x48` 2, no block. The raptor ends f = 617 in
`0x10/0x0A/0` on `0xD267E` with `rec+0x34` = −210, then −200, −190, … per
frame; the T-rex's `+0x08` is 0 and `+0x64` `0xFF`. `0x1975C`'s loop runs
exactly once in the whole run.

### 26.2 The raw (Ghidra `disassemble_function`, `read_memory` + capstone)

`0x1975C` (`get_xrefs_to`: `0x264CC`, plus `0x29B2F`/`0x2633D`/`0x26629`/
`0x274C7` in unported modes):

```
0x19763  call 0x17cb0
0x19778  call 0x33950                 ; ctx(i): ctx[0]=i, ctx[1]=1-i, ctx[2]=&slot[i], ctx[3]=&slot[1-i]
0x19780  cmp esi(2),[eax*4+0x100ad0] ; jge 0x19804   (signed: runs when AD0[i] > 2)
0x19791  call 0x1922c (ctx[1])
0x1979b  call 0x3962c (ctx[0], edx=1) ; true: [ctx[2]+0x8a]=0, 0x18b44(ctx[2]), return
0x197bf  call 0x396ac (ctx[0], edx=1) ; same
0x197e8  mov [ctx[3]+0x67],cl(1) ; 0x197ef call 0x3b464 (ctx[1])
0x197f8  call 0x3b938 (ctx[2]) ; 0x197ff call 0x39278 (esi=2)
```

`0x17CB0`: `0x15F48(0x128, 0x25, 0x100BD3, 0xFF)` (0x25 bytes of `0xFF`),
`[0x100AD0]` = `[0x100AD4]` = 0 (`0x17CDC`/`0x17CE2`); with both
`[0x1077B8]` and `[0x10784C]` set, `AL = 0x17BC8()`; unless AL,
`0x176CC(0)` for `[0x1077B8]` (`0x17D0E`) and `0x176CC(1)` for `[0x10784C]`
(`0x17D21`).

`0x176CC` (574 B; EAX = side, EDI = 1 − side): the other slot's `+0x74`
word (`[ecx+0x107824]`, `ja`) and `+0x76` word (`> 1`) guards; the
`0x140E4` overlap of the projectile's actor (`[side*0x94+0x1077B8]+0x56`)
and the other fighter's (`[ecx+0x1077B0]+0x56`); the other's
`DS_00100AC0` box through `0x15C30` when its bytes 2/3 are ≥ 1, else
`{0, 0, 0x20, 0x20}` (`0x17784..0x17792`); the other's sprite through
`0x17EEC`/`0xA8B30`/`0x1B544`; then two `0x181D0` syncs:

```
0x17844  181D0(0x20, (s16)w, AA8[side] - (box0 + B08[other]), B1C, B14, B28, B34, B24)
0x1787e  181D0(0x20, h, -(B00[other] + box1 - AA0[side]),     B18, B10, B2C, B30, B20)
0x17897  flag = B14 > B34 ; 0x178a8 B38 = |B14 - B34| ; 0x178b7 B40 = B1C
0x178bf  save byte[side+0x100b62] ; 0x178cf clear it
0x178e5  16DA4(flag, EDX = -1, EBX = AF0[other], ECX = B10, push B30, push 0, push side)
0x178ef  AD0[side] = B54 ; 0x178fa restore byte[side+0x100b62]
```

`AA0/AA8` (side 0) and `AA4/AAC` (side 1) are the projectile anchors
`0x17FA0` already writes (`get_xrefs_to 0x100AA0`: `0x17FDD` write,
`0x17C0E`/`0x17827` reads). The `0x33950` context at `0x176F3` is never read.

`0x16DA4`'s mode argument (`[esp+0x28]`): `-1` (`0x170A0`) decodes both
sprites; `0` replaces the side's row with the 4 bytes at `0xA173C` (`ff ff
ff ff`, `0x16E2C..0x16E4F`, width 4); `1` copies them into the other's row
before its row copy (`0x16E74`); `2`/`3` use the 6 bytes at `0xA1740`/
`0xA1746` for the other's row (`0x16EA5`/`0x16ED3`). The port had only the
`-1` arm. (`get_xrefs_to 0x16DA4`: `0x17565` in `0x170A0`, `0x178E5` in
`0x176CC`, `0x17BAC` in the unported `0x1790C`.)

`0x17BC8` (231 B): `0x140E4` of the two projectiles' actors; `0x181D0(0x20,
0x20, AA8 − AAC, B1C, B14, B28, B34, B24)` and `0x181D0(0x20, 0x20, AA0 −
AA4, B18, B10, B2C, B30, B20)`; `B1C > 0` and `B18 > 0` (`jle`); then the
voice `0x2C3FC(0x64)`, `0x3B938(0x1077B0)`, `0x2B150([0x10784C])`, AL = 1.

`0x3B938` (140 B; EAX = the thrower's slot, ECX = its `+0x08`): `+0x48` =
4 starts `0xE1898` at 2.0; else `0xBDFC8[slot+0x7A]` at the float of the
byte `0xBDFF0[slot+0x7A]` (`0x3B96B fild word`), both through `0x2BC30`;
then slot `+0x64` = `0xFF`, the projectile's `+0x48` = 0, `+0x34/+0x36` =
0, slot `+0x08` = 0, and the voice `0x2C3FC(word[0xBDFFA + char*2])`.
`read_memory 0xBDFC8`: `e0850e00 ce4f0e00 e0850e00 80310d00 …`; `0xBDFF0`:
`02 01 02 02 02 01 01 02 02 02`.

`0x3B464` (608 B), the woken arms (§7.12's gaps): the block arm
(`0x3B298` true) clears the thrower's `+0x8A` (`0x3B669`) and runs
`0x3B080(side, byte[anim0+3], byte[anim0+2], 0)` when `+0x54` ≠ 2, then
`0x3AD98(side, &anim)`. Otherwise: `0x2BD44` by the record's `+0x4B`
(`0x3B4D4..0x3B50A`), the slot's `+0x14` callback (`0x3B51B`, cleared on
non-zero), then the projectile's `+0x48`: 5 → `0x36D20(slot)`, 8 →
`0x1922C(side)` + `0x235C4(side)`, else `0x39834(side, stance)` and either
(`+0x54` = 2) `0x18B04` + `0x39F40(side, −0x50, 0x64, 0xF, 0x14)` or
`0x3A95C(side, (s8)byte[anim2[0]+6], EBX = slot+0x2C)`, then, when the
thrower's `+0x64` is neither 0 nor 5 and `(u8)(+0x90 − 1) > 3`,
`0x188DC(side, EBX)` (the jump table `0x3B454` holds `0x3B5EA` four times,
so `≤ 3` does nothing), and `0x3B080(…, 0)` when `+0x54` ≠ 2.

`0x3A95C` (122 B; EAX = side, EDX = b): `0x33A10` context,
`0x188AC(side, rec+0x18, 0)`, slot `+0x52/+0x53/+0x54` = `0x10/0x0A/0`,
`+0x10` = 0, `0x2BC30(rec, 0xC8FE0[char], 3.0)`, `+0x7E` = `byte[0xBECF8]`
(`0x14`) + b. `0x2BC30` saves ECX, so CL is still b at `0x3A9CB`.

### 26.3 The fix and its assertions

* **Fix** (`3d64c61`):
  * `camera.c`: `camera_projectile_step` (`0x17CB0`, exported),
    `camera_projectile_hit` (`0x176CC`) and `camera_projectile_clash`
    (`0x17BC8`); `camera_winner_height` (`0x16DA4`) gains the mode 0..3 row
    arms.
  * `fighter.c`: `fighter_think` calls `0x17CB0` first, uses the signed
    gate, and runs `0x1922C`, the `0x3962C`/`0x396AC` hold gates with
    `0x18B44`, `0x3B938` and `0x39278`; `fighter_think_side` wires every
    `0x3B464` arm above except `0x235C4` (still a `PORT:` gap: no demo
    projectile has `+0x48` = 8). New `fighter_3b938` and `fighter_3a95c`.
    The stale "every writer of slot+0x64 sets 0xFF / unexercised by the
    demo" note is gone (`0x3D17C` writes the old `+0x5F`).
  * Data addresses as local `#define`s: `0xE1898`, `0xBDFC8`, `0xC8FE0`.
  * `actors.c`: `actor_alloc`'s `TODO(verify)` on the `0x400` tail-insert
    arm is replaced by the raw's `0x2ACB6..0x2ACD7` (`and ch,4`; non-zero →
    `0x249C0`); `0x3D214`'s child spawn reaches it in the demo.
* **Assertions** (`test_fight.c`):
  * `check_think_chain` now asserts the raw: with seeded `AD0/AD4` = 3 and
    no projectile, `0x17CB0` zeroes them and no driver runs (every sentinel
    kept). Its previous assertions (both drivers ran) were true only
    because the port skipped `0x17CB0`.
  * The new `check_projectile_step` with `pc_seed` (check_unfreeze's fake
    8 x 8 all-on sprite for the four actors, every collision global
    seeded, the slots, rows, `0x107D20..`, `0x107A80..`, the input ring
    and `0x100CE0` saved and restored):
    * A, the hit: `AD0[0]` = `B54` = 5, derived from the raw: 8 rows of
      popcount 8 = 64, `(64 << 12) / 0xF3D` = 67, `(67 << 12) / 0xD56` = 80,
      80 / 16 = 5; `AD4` 0, `B1C`/`B18` 8, `B62[0]` restored to `0x5A`; the
      struck `+0x67`/`+0x41`/`+0x86`, the thrower's `+0x64`/`+0x08`/`+0x8A`,
      the projectile's `+0x48/+0x34/+0x36`, its stream `0xE85E4` at 2.0 and
      `DS_001088EC` = 3.
    * A2, the block arm (command word `0x1000`): `+0x43` bit 5, the
      thrower's `+0x8A` cleared, the tail and the burst.
    * B, the `0x17700` guard; C, the `0x17BC8` clash (`B1C`/`B18` `0x20`,
      side 0 burst, side 1 dead); D, `0x3B938` directly (the `+0x48` = 4
      arm `0xE189A` at 2.0; char 1: `0xE4FCE` at 1.0); E, `0x3A95C`
      directly (the raptor's `0xD267E` at 3.0, `0x10/0x0A/0`, `+0x7E` =
      `0x19`); F, the two `0x181D0` offsets (a 4-px y or x offset gives
      `B18`/`B30` = 4 or `B1C`/`B34`/`B38` = 4 and `AD0` = 2, which the gate
      rejects).
  * Carry-overs: `check_hit_reaction_drive` restores the word `0x1080AC`
    its last block's `0x3D17C` writes; `check_trex_breath` part D seeds every
    free pool record's `+0x48` to `0x5A`, so the emitter's `+0x48` = 0 is
    the descriptor's type byte (removing `actor_spawn`'s `+0x48` store makes
    part D fail).
* **Mutations** (`mut16.py`, 29; `camera.c`/`fighter.c` restored and
  `cmp`-verified, the suite passed afterwards). Real `FAIL` lines:

  | mutation | failures |
  |---|---|
  | no `0x17CB0` call / no `AD0` zero / no `0x15F48` fill | 35 / 7 / 21 |
  | `0x176CC` guard off / mode −1 / row not `0xA173C` / `AD0` to the other side | 5 / 21 / 21 / 21 |
  | `0x176CC` `B62` not restored | 1 |
  | `0x176CC` y sign / x sign | 5 / 3 |
  | clash skipped / no kill / no burst / `0x176CC` after a clash | 7 / 1 / 2 / 3 |
  | gate `> 5` instead of `> 2` | 16 |
  | no `0x3B938` in the step / no `0x39278` | 7 / 1 |
  | `0x3B938`: no `+0x48` = 4 arm / constant hold / no `+0x64` / no `+0x34` | 1 / 1 / 2 / 1 |
  | `0x3B464`: no `+0x8A` clear / `+0x48` = 8 arm without the tail | 1 / 1 |
  | `0x3A95C`: `+0x7E` without b / no `+0x54` / no `0x188AC` / `ctx_same` | 1 / 1 / 1 / 9 |

  One mutation survives, as it must: not clearing `B62[side]` for the call
  cannot be seen, because `0xA173C`'s `ff ff ff ff` is invariant under
  `0x15EC0`'s bit reversal.

### 26.4 Measured

Port frames 0..1033 are byte-identical to `8f2d86b`'s, and 1034 (f = 618)
is the first that differs. The whole-run probe shows one think pass, at
f = 617.

| measurement | before (`8f2d86b`) | after (`3d64c61`) |
|---|---|---|
| captures 1478..1480 | 1478 7 749 px; 1479 13 638; 1480 14 417 | **all explained** (0 px splices) |
| demo oracle first unexplained | 1478 (raw 4385); `[1478..3616]` 2139 / 2133 unexpl.; port `[1034..1380]` | **1481 (raw 4388)**; `[1481..3616]` 2136 / 2130 unexpl.; port `[1036..1380]` (345, 0 exhibited) |
| demo-fight ratchet | `[1478..1884]` 407, N = 1478 | **`[1481..1884]` 404**, "ratchet improved: first unexplained 1481 > 1478", **N = 1481** |
| front-end oracle | `[560..1477]` / 918 / 382 clean, 529 splice, 3 transition, 2 unexpl. | **`[560..1480]` / 921 / 383 clean, 531 splice, 3 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. The three transition frames are the
same (`port832@row177`, `port862@row189`, `port906@row31`). Unmoved: title
`54/55/2/0` and `54/57/0/0`, determinism 54; smk 120/120 and 41/41;
attract 215/216 (expected divergence at 215); C-vs-Python 9866;
`symbols.h`; the front-end "endpoints BAD" line. The exhibition set grows
to port frames 0..1035 (799 exhibited). The ladder
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify`
exited 0 with 0 compiler warnings, with N = 1481 in the Makefile.

**Unresolved code targets after the fix** (a temporary whole-run probe in
`fn_resolve`, reverted):
* `0x3C048` (f = 652): the reaction-`0x3D` callback of every character
  (`0xA39EC`, `0xA3EEC`, … `0xA57EC`, i.e. `0xA3528 + (char·0x40 + 0x3D)·20`)
* `0x3E3A8` (f = 920): the `(char 0, 0x2A)` callback at `0xA3870`
* the known front-end sites `0x29B74`/`0x41578` and the stub `0x5D812`

`0x14814` and `0x3ECF8` no longer miss; the run moved from f = 618, so these
frames are not comparable one for one with §25.4's.

### 26.5 The new first unexplained frame, 1481 (characterised, not fixed)

Captures 1478..1480 splice at 0 px (1033/1034 row 103, 1034/1035 row 127,
1034/1035). Capture 1481's best splice, port 1035/1036 (row 0), leaves
3 626 px in rows 86–192, almost all in the two fighters (against port
1036: x 0–79 1 578 px, x 240–319 2 038 px, x 80–199 0 px, x 200–239 10 px;
the background rows 45–75 match port 1036 exactly);
1482 leaves 3 615 px, 1483 2 001, 1485 4 503, and 1499 14 719. A shift
search per fighter on 1481 against port 1036 (f = 620) puts the capture's
raptor 1 px left (teal mask over x 0–69, rows 60–189: 84 mismatches at
dx −1 against 450 at dx 0) and the T-rex 1 px right (gold mask over x
236–317, rows 40–189: 170 mismatches at dx +1 against 473 at dx 0, not
exact: the pose also differs slightly); at 1482 the raptor matches port 1037
exactly and the T-rex leaves 158. So from f ≈ 619 the capture's fighters
stand slightly further apart and the T-rex's frames drift. In the port the
raptor is in the `0x3A95C` stagger (`rec+0x34` −200, −190, … per frame, no
`+0x10` handler) and the T-rex takes reaction `0x08` at f = 619 (state
`9/8/1`). The owner is **not derived**; the candidates are the struck
raptor's knock-back (`0x39834`'s `0x392A0`/`0x3B080` seeds or the `0xD267E`
stream's velocity) and the T-rex's reaction `0x08`.

## 27. The `0x349C8` command gate at capture 1481 (roar-timing Task 17, `219691e`)

**Result in one line.** Capture 1481 has one cause, and it is the port's:
neither §26.5 candidate. `0x349C8` (the `+0x52` == 0 handler) returns at
`0x34A8D` when the side's command word has a bit in both `(cmd>>8)&3` and
`(cmd>>8)&0xC`; the port only skipped the `0x3BDDC` consume there and still
ran the `0x4000` arm and `0x35838`. At f = 619 the T-rex's word `0x6F6F`
took the port's `0x4000` arm, which restarted its record on `0xC8978[0]`
(sprite `0x0F02` → `0x0F34`); the reaction-`0x08` hit that follows in the
same frame (`0x350D0` → `0x34E2C` → `0x18B04`) re-derived its record x from
the new sprite's anchor, 64 units (1 px) left of the raw's. A one-line gate;
no new function. This explains captures 1481..1545.

### 27.1 The measurement (temporary, reverted)

**Where the capture differs.** Captures 1476..1480 splice at 0 px (1480 is
port 1035 exactly). 1481's best splice, 1035/1036 row 0, leaves 3 626 px,
all in the two fighters: the burst, the worshippers, the HUD and every
background band match port 1036 exactly, so the camera is not the owner.
The T-rex's reaction-`0x08` crouch first shows in port 1036, whose state the
per-frame probe prints as f = 619; a zoomed crop shows the capture's T-rex
in the same crouch sprite, 1 px to the right; the raptor is 1 px left.

**Checkpoints** (temporary `PR_T17` prints between every call of
`fight_arena_frame` and inside `fight_hud_pass`, reverted): the raptor's
record x moves only by its `0x2A4FC` velocity from f = 617 on
(−220, −210, … with the `+0x43` = 10 friction) — nothing re-anchors it. The
T-rex's record x moves 28 250 → 28 186 (−64) at f = 619 inside
`fight_hud_pass(0)`: `fight_health_sync(0)` → `0x349C8` changes its actor's
sprite `0x0F02` → `0x0F34` (stream `0xE6DD2` → `0xE6DEA`, the `0xC8978[0]`
start, `+0x52/+0x54` = 5/1); then `0x3531C` → `0x350D0` → `0x34E2C(8)` →
`0x18B04` → `0x18714`/`0x18540` recompute the anchor from `0x0F34`
(`DS_00100AF0[0]` 30 → 80, `DS_00100AB0[0]` −320 → −256) and write
rec+0x18 = slot+0x2C (27 930) − (−256) = 28 186. With sprite `0x0F02` the
anchor stays 30 = slot+0x20, `0x18350` is skipped, AB0 stays −320 and
rec+0x18 stays 28 250: exactly the capture's 1 px.

**Checked and matching** (Ghidra `disassemble_function` / `read_memory` +
capstone): `0x188DC`, `0x188AC`, `0x18714`, `0x18540` (and its `0x18524`
table), `0x18B04` with its shared tail `0x18AAE..0x18AF5`, `0x2A4FC` (the
velocity and the `+0x43` friction), `0x3B464`'s middle and tail (`0x3B612
xor ecx,ecx`: no mirror onto the thrower), `0x3531C`'s case 10 (`0x354DA`
null-checks `+0x10`), `0x354F0` (the ±`0x7C00` wall clamp, not reached:
both fighters inside), and `0x350D0`'s own command gate (`0x35209 75 1C
jne 0x35227` skips only the consume, as the port has it).

### 27.2 The raw

`0x349C8`, after the `0x365C8`/`0x36638` calls:

```
0x34A62  66 8B 04 7D E0 88 10 00   mov ax, [edi*2 + 0x1088E0]
0x34A6A  89 C2                     mov edx, eax
0x34A6C  30 C2                     xor dl, al
0x34A6E  80 E6 03                  and dh, 3
0x34A71  81 E2 FF FF 00 00         and edx, 0xFFFF
0x34A77  74 10                     je 0x34A89
0x34A79  30 C0                     xor al, al
0x34A7B  80 E4 0C                  and ah, 0xC
0x34A7E  25 FF FF 00 00            and eax, 0xFFFF
0x34A83  74 04                     je 0x34A89
0x34A85  B0 01                     mov al, 1
0x34A87  EB 02                     jmp 0x34A8B
0x34A89  30 C0                     xor al, al
0x34A8B  84 C0                     test al, al
0x34A8D  0F 85 78 00 00 00         jne 0x34B0B        ; the epilogue
0x34A93  66 8B 14 7D E0 88 10 00   mov dx, [edi*2 + 0x1088E0]
0x34A9D  30 D2                     xor dl, dl
0x34A9F  E8 38 73 00 00            call 0x3BDDC
0x34AA4  80 E6 F0                  and dh, 0xF0
0x34AA7  84 C0                     test al, al
0x34AA9  75 60                     jne 0x34B0B
0x34AAB  ...                       cmd & 0x4000 -> 0x2BC30(0xC8978[ch], 2.0), +0x52 = 5, +0x54 = 1
0x34ADD  ...                       (+0x53 == 0 && cmd & 0x1000) || cmd & 0x2000 -> 0x35838
0x34B0B  5F 5E 5A 59 5B C3         pop edi/esi/edx/ecx/ebx; ret
```

So all three arms sit behind `0x34A8D`. The port's `if (!bvar2) { consume }`
followed by the unconditional arms is the defect. `0x6F6F`: `0x6F & 3` = 3,
`0x6F & 0xC` = 0xC, so the raw returns at `0x34A8D`.

### 27.3 The fix and its assertions

`fighter_state_default` returns when `bvar2` holds (`/* 0x34A8D jne
0x34B0B */`), before the consume and both arms; the header comment names the
gate. New case I in `test_fight.c`'s `check_deep_callees`: side 1 (char 3)
with `0xC8978[3]`/`[0]` pointed at distinct scratch streams, So+0x43 = 0 and
S+0x43 = 0 (so `0x365C8`/`0x36638` return 0), sentinels `+0x52`/`+0x54` =
`0x66`, rec+8 = `0xABCDEF`, rec+0x20 = `0x11111111`, actor sprite `0x7777`.
Command `0x6F6F`: every sentinel survives. Command `0x4040` (`(cmd>>8)&3` = 0,
bit 15 clear so `0x3BDDC` returns 0): the `0x4000` arm runs (`+0x52/+0x54` =
5/1, rec+8 = the char-3 stream, rec+0x20 = 2.0, sprite `0x0123`). The table
entries and `DS_001088E2` are restored; the slot region is the enclosing
test's snapshot.

Mutations (each run, then reverted): removing the gate fails the five
`0x6F6F` assertions; an unconditional return fails the five `0x4040`
assertions (and three existing `test_fight` checks at 2444..2452).

### 27.4 Measured

Port frames 0..1035 are byte-identical to `0f4d415`'s, and 1036 (the f = 619
state) is the first that differs.

| measurement | before (`0f4d415`) | after (`219691e`) |
|---|---|---|
| captures 1481..1545 | 1481 3 626 px; 1482 3 615; 1483 2 001 | **all explained** |
| demo oracle first unexplained | 1481 (raw 4388); `[1481..3616]` 2136 / 2130 unexpl. | **1546 (raw 4453)**; `[1546..3616]` 2071 / 2065 unexpl.; port `[1092..1380]` (289, 0 exhibited) |
| demo-fight ratchet | `[1481..1884]` 404, N = 1481 | **`[1546..1884]` 339**, "ratchet improved: first unexplained 1546 > 1481", **N = 1546** |
| front-end oracle | `[560..1480]` / 921 / 383 clean, 531 splice, 3 transition, 2 unexpl. | **`[560..1545]` / 986 / 410 clean, 569 splice, 3 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. The three transition frames are the
same (`port832@row177`, `port862@row189`, `port906@row31`). The exhibition
set grows to port frames 0..1091 (855 exhibited). The ladder
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify`
exited 0 with 0 compiler warnings, with N = 1546 in the Makefile. Unmoved: title
`54/55/2/0` and `54/57/0/0`, determinism 54; smk 120/120 and 41/41; attract
215/216 (expected divergence at 215); C-vs-Python 9866; `symbols.h`; the
front-end "endpoints BAD" line.

**Unresolved code targets after the fix** (a temporary whole-run probe in
`fn_resolve`, reverted): `0x3C0A4` (f = 850), `0x3ECF8` (f = 887) and
`0x3E3A8` (f = 896), plus the known front-end sites `0x29B74`/`0x41578` and
the stub `0x5D812`. `0x3C048` (f = 652 in §26.4) no longer misses: the run
moved from f = 619, so these frames are not comparable one for one with
§26.4's.

### 27.5 The new first unexplained frame, 1546 (characterised, not fixed)

Captures 1542..1545 splice at 0 px (1088/1089 row 148, 1088/1089, 1089/1090,
1090/1091 row 40). Capture 1546's best splice, port 1091/1092 row 67, leaves
148 px, all in x 221–234, rows 180–199: a small teal-clad worshipper beside
the right-hand group. In the capture it has dropped into a crouch (a smaller,
lower figure) while in the port it keeps its upright pose; 1547 leaves 297 px (x 209–235, rows 177–199), 1548 323
and 1549/1550 372 px (x 209–237, rows 175–199), with the capture's figure
lying lower each frame. The fighters, the burst and the background match.
Port 1091 is the f ≈ 674 state. The owner is **not derived**; no `fn_resolve` miss falls in f = 620..849.

(Derived since, §28: the owner is the effects pass's case 3 `0x49DB3`,
which lands the two side-1 worshippers on their `0xC9634` crouch stream at
f = 675/676; the port had only its gate and draw. Porting cases 3..5
explains 1546..1562.)

## 28. The worshippers' fall, lie and climb (`0x49C78` cases 3..5) at capture 1546 (roar-timing Task 18, `27c95c0`)

**Result in one line.** Capture 1546 has one cause, and it is the port's: the
effects pass `0x49C78`'s type-3 handler (`0x49DB3`) was ported only as its
`DS_000BD898` gate and the `rng(0x3C)` draw. The two side-1 worshippers
`0x4B5A8` scared at f ≈ 650 (their slot 1 in state 7/2: type 3, `+0x38` =
−64) fall 64 units a
frame; the raw lands each one when its y reaches `0x400` — the `0xC9634[si]`
crouch stream at 2.0, the velocities zeroed, a `rng(0x3C) + 0x3C` lie timer
and type 4 — while the port kept the entry at type 3, kept the upright
stream, and drew `rng(0x3C)` again on every later frame. Entry `0x108414`
lands at f = 675 (port frame 1092) and `0x108438` at f = 676. The fix ports
case 3 whole and, because it hands the entry to them, cases 4 (`0x49E5A`, the
lie timer and the rise) and 5 (`0x49EC0`, the climb back to type 0); no new
callee (`0x496AC`, `0x2BE1C`, `0x2BC30` and `0x5D7DC` were ported). `0x496AC`
is signed in the raw; the port's `fight_dust_clamp` compared unsigned (the
same result for y ≥ 0) and is corrected. This explains captures 1546..1562.

### 28.1 The measurement (temporary, reverted)

Captures 1542..1545 splice at 0 px. A temporary `PR_T18` print of every
effects-list entry in `fight_effects_pass` (entry, type, `si`, the actor's x,
its y `+0x30 >> 16`, `+0x38`, the pset screen x/y, `+0x1A`/`+0x1C`, the
slot's `+0x52/+0x53/+0x54/+0x42` and the `0x1088B6/0x1088B2/0x10889E` bytes)
for f = 600..700 (dump reverted) shows four entries: `0x1083CC`/`0x1083F0`
(side 0, types 1/0, screen x 254..280) and `0x108414`/`0x108438` (side 1,
`si` 3/4). The side-1 pair turned type 3 at f ≈ 650 (`+0x38` = `0xFFC0`,
`+0x1A` = 2120/2633, slot 1's `+0x52/+0x53` = 7/2), and their y falls 64 per
frame. At f = 673..675 their screen x is 234..236 and 224..226 — capture
1546's residual box (x 221–234). `0x108414`'s y is 1032 at f = 674 and 968
at f = 675; `0x108438`'s 1033 at f = 675 and 969 at f = 676.

### 28.2 The raw (Ghidra `read_memory` + capstone, fixups applied)

The jump table `0x49C2C` (`read_memory`) sends type 3 to `0x49DB3`, 4 to
`0x49E5A`, 5 to `0x49EC0`. `DS_000BD898` is the word `0x0400` (`00 04`).

```
0x49DB3  8B 41 08                   mov eax, dword ptr [ecx + 8]
0x49DB6  8B 40 30                   mov eax, dword ptr [eax + 0x30]
0x49DB9  C1 F8 10                   sar eax, 0x10
0x49DBC  E8 EB F8 FF FF             call 0x496ac
0x49DC1  8B 51 08                   mov edx, dword ptr [ecx + 8]
0x49DC4  66 89 42 2C                mov word ptr [edx + 0x2c], ax
0x49DC8  31 C0                      xor eax, eax
0x49DCA  8B 53 30                   mov edx, dword ptr [ebx + 0x30]
0x49DCD  66 A1 98 D8 0B 00          mov ax, word ptr [0xbd898]
0x49DD3  C1 FA 10                   sar edx, 0x10
0x49DD6  39 C2                      cmp edx, eax
0x49DD8  0F 8F 8A 06 00 00          jg 0x4a468
0x49DDE  8B 51 0C                   mov edx, dword ptr [ecx + 0xc]
0x49DE1  8B 41 08                   mov eax, dword ptr [ecx + 8]
0x49DE4  8B 12                      mov edx, dword ptr [edx]
0x49DE6  E8 31 20 FE FF             call 0x2be1c
0x49DEB  8D 53 28                   lea edx, [ebx + 0x28]
0x49DEE  85 C0                      test eax, eax
0x49DF0  7E 0A                      jle 0x49dfc
0x49DF2  C7 44 24 04 00 40 00 00    mov dword ptr [esp + 4], 0x4000
0x49DFA  EB 06                      jmp 0x49e02
0x49DFC  31 FF                      xor edi, edi
0x49DFE  89 7C 24 04                mov dword ptr [esp + 4], edi
0x49E02  31 C0                      xor eax, eax
0x49E04  8B 7C 24 04                mov edi, dword ptr [esp + 4]
0x49E08  66 8B 02                   mov ax, word ptr [edx]
0x49E0B  09 F8                      or eax, edi
0x49E0D  66 89 02                   mov word ptr [edx], ax
0x49E10  31 C0                      xor eax, eax
0x49E12  66 89 F0                   mov ax, si
0x49E15  8B 14 85 34 96 0C 00       mov edx, dword ptr [eax*4 + 0xc9634]
0x49E1C  8B 41 08                   mov eax, dword ptr [ecx + 8]
0x49E1F  68 00 00 00 40             push 0x40000000
0x49E24  E8 07 1E FE FF             call 0x2bc30
0x49E29  66 C7 43 38 00 00          mov word ptr [ebx + 0x38], 0
0x49E2F  B8 3C 00 00 00             mov eax, 0x3c
0x49E34  66 C7 43 34 00 00          mov word ptr [ebx + 0x34], 0
0x49E3A  E8 9D 39 01 00             call 0x5d7dc
0x49E3F  05 3C 00 00 00             add eax, 0x3c
0x49E44  8A 51 1C                   mov dl, byte ptr [ecx + 0x1c]
0x49E47  C6 41 1E 04                mov byte ptr [ecx + 0x1e], 4
0x49E4B  80 CA 80                   or dl, 0x80
0x49E4E  66 89 41 18                mov word ptr [ecx + 0x18], ax
0x49E52  88 51 1C                   mov byte ptr [ecx + 0x1c], dl
0x49E55  E9 0E 06 00 00             jmp 0x4a468
0x49E5A  66 8B 41 18                mov ax, word ptr [ecx + 0x18]
0x49E5E  48                         dec eax
0x49E5F  66 89 41 18                mov word ptr [ecx + 0x18], ax
0x49E63  66 85 C0                   test ax, ax
0x49E66  0F 8F FC 05 00 00          jg 0x4a468
0x49E6C  31 D2                      xor edx, edx
0x49E6E  66 89 F2                   mov dx, si
0x49E71  8B 41 08                   mov eax, dword ptr [ecx + 8]
0x49E74  8B 14 95 EC 95 0C 00       mov edx, dword ptr [edx*4 + 0xc95ec]
0x49E7B  68 00 00 40 40             push 0x40400000
0x49E80  E8 AB 1D FE FF             call 0x2bc30
0x49E85  66 C7 43 38 40 00          mov word ptr [ebx + 0x38], 0x40
0x49E8B  8B 41 0C                   mov eax, dword ptr [ecx + 0xc]
0x49E8E  8B 00                      mov eax, dword ptr [eax]
0x49E90  66 8B 40 28                mov ax, word ptr [eax + 0x28]
0x49E94  30 C0                      xor al, al
0x49E96  80 E4 40                   and ah, 0x40
0x49E99  25 FF FF 00 00             and eax, 0xffff
0x49E9E  74 08                      je 0x49ea8
0x49EA0  66 C7 43 34 C0 FF          mov word ptr [ebx + 0x34], 0xffc0
0x49EA6  EB 06                      jmp 0x49eae
0x49EA8  66 C7 43 34 40 00          mov word ptr [ebx + 0x34], 0x40
0x49EAE  8A 61 1C                   mov ah, byte ptr [ecx + 0x1c]
0x49EB1  C6 41 1E 05                mov byte ptr [ecx + 0x1e], 5
0x49EB5  80 E4 7F                   and ah, 0x7f
0x49EB8  88 61 1C                   mov byte ptr [ecx + 0x1c], ah
0x49EBB  E9 A8 05 00 00             jmp 0x4a468
0x49EC0  8B 41 08                   mov eax, dword ptr [ecx + 8]
0x49EC3  8B 40 30                   mov eax, dword ptr [eax + 0x30]
0x49EC6  C1 F8 10                   sar eax, 0x10
0x49EC9  E8 DE F7 FF FF             call 0x496ac
0x49ECE  8B 51 08                   mov edx, dword ptr [ecx + 8]
0x49ED1  66 89 42 2C                mov word ptr [edx + 0x2c], ax
0x49ED5  66 8B 43 32                mov ax, word ptr [ebx + 0x32]
0x49ED9  66 3B 41 1A                cmp ax, word ptr [ecx + 0x1a]
0x49EDD  0F 8C 85 05 00 00          jl 0x4a468
0x49EE3  31 D2                      xor edx, edx
0x49EE5  66 89 F2                   mov dx, si
0x49EE8  8B 41 08                   mov eax, dword ptr [ecx + 8]
0x49EEB  8B 14 95 44 95 0C 00       mov edx, dword ptr [edx*4 + 0xc9544]
0x49EF2  68 00 00 40 40             push 0x40400000
0x49EF7  E8 34 1D FE FF             call 0x2bc30
0x49EFC  66 C7 43 38 00 00          mov word ptr [ebx + 0x38], 0
0x49F02  66 C7 43 34 00 00          mov word ptr [ebx + 0x34], 0
0x49F08  C6 41 1E 00                mov byte ptr [ecx + 0x1e], 0
```

`EBX` is the entry's actor (`0x49CD8 mov ebx,[ecx+8]`), `ECX` the entry and
`SI` the `si` index (`0x49CE1`). `0x496AC`:

```
0x496AD  3D 00 0B 00 00   cmp eax, 0xb00 ; jl 0x496bb   -> 0xC00 when >= 0xB00
0x496BB  3D 00 04 00 00   cmp eax, 0x400 ; jg 0x496c9   -> 0xF80 when <= 0x400
0x496C9  (0xB00 - eax) sar 1 + 0xC00
```

So the port's case 3 (`if (y <= (s16)BD898) rng(0x3C)`) had the gate and the
draw only: no `+0x2C` write, no landing, no type change — the landed entry
stayed type 3 and drew `rng(0x3C)` on every later frame. Cases 4 and 5 were
the `default:` gap. The tables (`read_memory`): `0xC9634` = `0xEE1B2,
0xEE53C, 0xEE8CE, 0xEEC96, 0xEF060, 0xEF42A`; `0xC95EC` = `0xEE14C, 0xEE4FE,
0xEE868, 0xEEC30, 0xEEFFA, 0xEF3EC`; `0xC9544` = `0xEE02C, 0xEE3E6, 0xEE726,
0xEEAE2, 0xEEED4, 0xEF28A`.

### 28.3 The fix and its assertions

`fight_effects_pass` gains cases 3, 4 and 5 transcribed from the listing
(the gate word zero-extended, the counter and the climb compare signed, the
hflip an OR before `0x2BC30`); `fight_dust_clamp` (`0x496AC`) takes an `s32`
and compares signed. The dispatch comments now name types 2 and 6..12 as the
remaining gaps. New `check_effects_fall` in `test_fight.c` (entry `si` = 3,
the three tables' entries 0 and 3 pointed at distinct literal-sprite scratch
streams, restored after; `DS_000BD898` seeded then restored):

* case 3 still falling (y `0x800`, and `0x401`): only `+0x2C` moves (`0xD80`);
  type, `+0x38`, the timer sentinel `0x7777`, the stream, the pset and the RNG
  state are kept;
* the landing at y `0x400` with `0x2BE1C` > 0: type 4, `+0x28` = `0x4001`
  (`0x0011 | 0x4000`, then `0x2BC30`'s `and word [rec+0x28], 0xF7EB`), the
  `0xC9634[3]` stream at 2.0 with the pset sprite `0x8321` (the hflip bit, so
  the OR precedes `0x2BC30`), `+0x34`/`+0x38` = 0, the timer =
  `rng(0x3C) + 0x3C` from the seeded state, `+0x1C` = `0x85`, `+0x2C` =
  `0xF80`;
* `0x2BE1C` < 0 keeps hflip as it was (set stays set, clear stays clear), and
  y = −0x100 gives `+0x2C` = `0xF80` (signed `0x496AC`; unsigned gives
  `0xC00`);
* `DS_000BD898` = `0x8000` with y = `0x100` lands (zero-extended gate);
* case 4: 2 → 1 stays, 1 → 0 rises (`0xC95EC[3]` at 3.0, `+0x38` = `0x40`,
  `+0x34` = `0x40` with the fighter's `+0x28` bit `0x4000` clear, `+0x1C`
  `0x85` → `0x05`, type 5, no draw); `0x8001` → `0x8000` rises at once
  (signed) and the set bit gives `+0x34` = `0xFFC0`;
* case 5: y `0x800` < `+0x1A` `0x900` keeps climbing (`+0x2C` = `0xD80`),
  y `0x900` arrives (`0xC9544[3]` at 3.0, `+0x2C` = `0xD00`, velocities 0,
  type 0); `+0x32` = `0x8000` stays below `0x100` (signed).

`check_effects_rng`'s case-3 entry now lands, so it gains seeds only (the
entry's `+0x0C` slot and a literal-sprite `0xC9634[0]` stream, restored); its
assertions are unchanged and still hold (the landing draws exactly one
`rng(0x3C)`).

Mutations (31, each built and run, then `cmp`-restored): every one fails
1..9 assertions — the `+0x2C` writes (cases 3, 5), the gate `>=`, the gate
sign-extended, no hflip, hflip assigned instead of OR-ed, hflip after
`0x2BC30`, table index 0 (each case), the holds, each velocity store, no
`+0x3C`, no draw, no type 4/5/0, the `+0x1C` set and clear, the case-4
counter unsigned or `>= 0`, the facing inverted, the case-5 compare unsigned
or `<=`, and `0x496AC` unsigned.

### 28.4 Measured

Port frames 0..1091 are byte-identical to the pre-fix dump, and 1092 (the
f = 675 state) is the first that differs. Captures 1546..1562 splice at 0 px
(1546 is port 1091/1092 row 67).

| measurement | before (`f49384c`) | after (`27c95c0`) |
|---|---|---|
| captures 1546..1562 | 1546 148 px; 1547 297; 1549 372 | **all explained** |
| demo oracle first unexplained | 1546 (raw 4453); `[1546..3616]` 2071 / 2065 unexpl. | **1563 (raw 4470)**; `[1563..3616]` 2054 / 2048 unexpl.; port `[1106..1380]` (275, 0 exhibited) |
| demo-fight ratchet | `[1546..1884]` 339, N = 1546 | **`[1563..1884]` 322**, "ratchet improved: first unexplained 1563 > 1546", **N = 1563** |
| front-end oracle | `[560..1545]` / 986 / 410 clean, 569 splice, 3 transition, 2 unexpl. | **`[560..1562]` / 1003 / 413 clean, 583 splice, 3 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. The three transition frames are the
same (`port832@row177`, `port862@row189`, `port906@row31`). The exhibition
set grows to port frames 0..1105 (869 exhibited). The ladder
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify`
exited 0 with 0 compiler warnings, with N = 1563 in the Makefile. Unmoved: title
`54/55/2/0` and `54/57/0/0`, determinism 54; smk 120/120 and 41/41; attract
215/216 (expected divergence at 215); C-vs-Python 9866; `symbols.h`; the
front-end "endpoints BAD" line.

**Unresolved code targets after the fix** (a temporary whole-run probe in
`fn_resolve`, reverted): `0x14F50` (f = 865 and 871; Ghidra
`get_xrefs_to` lists no code reference, so it is reached through data), plus
the known front-end sites `0x29B74`/`0x41578` and the stub `0x5D812`, and
`fn_resolve(0)` (a null lookup at f = 617, 49 calls, before the first frame
this fix changes). `0x3C0A4`, `0x3ECF8` and `0x3E3A8` (§27.4) no longer
miss: the run moved from f = 675.

### 28.5 The new first unexplained frame, 1563 (characterised, not fixed)

Captures 1546..1562 splice at 0 px. Capture 1563's best splice, port
1106/1107 row 154, leaves 514 px in x 37–232, rows 158–191, in two places:
x 200–232 (≈ 370 px), a worshipper beside the right-hand (gold) fighter drawn
in a different pose in the capture, and x 37–70, rows ≈ 180–191 (≈ 144 px),
a dark blob under the left (blue) fighter whose shape differs. 1564 leaves
514 px, 1565 498, 1566 284 and 1568 522. Port 1106 is the f ≈ 689 state. The
owner is **not derived**. A candidate, not a derivation: the worshippers
landed by this fix now carry the entry's `+0x1C` bit 7 (type 4), and that
bit gates the effects pass's unported per-entry prelude `0x4B69C`
(`0x4B6AC and al,0x80`), which runs `0x17D30` — a point test against both
fighters' boxes through `0x1790C` — and on a hit `0x4B788`/`0x4B470`.

(Derived since, §29: the candidate holds. The effects pass's prelude
`0x4B69C` finds both landed side-1 worshippers inside side 0's `0x100AC8`
box at f = 689 (the gold fighter's tail) and tramples them through
`0x4B470`; the port had no prelude. Porting it with its point test, the
trample and case 6 explains 1563..1658.)

## 29. The worshippers' trample (`0x4B69C`, `0x17D30`/`0x1790C`, `0x4B470`, case 6) at capture 1563 (roar-timing Task 19, `cc38a38`)

**Result in one line.** Capture 1563 has one cause, and it is the port's: the
§28.5 candidate. `0x49C78` calls `0x4B69C(entry, si)` for every entry before
its type dispatch (`0x49CFE`), and the port did not. An entry whose `+0x1C`
bit 7 is set (a worshipper lying after case 3's landing, or falling from a
trample) tests its actor's pset point against both fighters (`0x17D30`,
which runs `0x1790C` per side); on a hit that `0x4B788` does not claim for a
grab, `0x4B470` tramples it: the `0xC9604[si]` tumble stream at 3.0, a shadow
actor from `0xBB920[si]`, `+0x34` = ±`0x80`, `+0x36` = `0x240`, type 6. Case 6
(`0x49F11`) flies it and lands it on `0xC973C[si]` at 2.0 as type 8. At
f = 689 both side-1 worshippers (`0x108438`, `0x108414`, type 4) fall inside
side 0's box — the gold fighter's tail sweep — so the capture's worshipper is
thrown (x 200–232). The pre-fix and post-fix port frames 1107 differ in
exactly capture 1563's two regions (x 37–64, rows 182–191, 144 px; x 199–232,
rows 158–187, 370 px), so the dark blob is drawn by the new path too (the
tumble streams and the two `0xBB920` shadow actors are its only new sprites;
which one draws the blob is not attributed further). This explains captures
1563..1658.

### 29.1 The measurement (temporary, reverted)

A temporary `PR_T19` print in `fight_effects_pass` computed
`camera_point_hit` (the port of `0x17D30`, written first) for every entry with
`+0x1C` bit 7 on the pre-fix tree and printed the `0x4B788` gate inputs. The
first non-zero result is at f = 689 (port frame 1106, capture 1563's splice):
both side-1 entries (`si` 4/3, type 4, lie timers 84/87) return 1 (side 0),
with slot 0's `+0x5F` = `0x17` against its grab move `0xC97F2[ch]` = `0x2D`
(so `0x4B788` returns 1: trample), `DS_00105B3A` = 0 and the other side's
`+0x54` = 0. With the fix, the same print showed `TRAMPLE` for both at
f = 689 and nowhere else in the run, and probes at the named gaps (the grab
arm, the eighth-hit tail, case 8's held body, the default types) never fired.

### 29.2 The raw (Ghidra `disassemble_function` / `read_memory` + capstone, fixups applied)

`0x49CD1..0x49CFE` (the call): `mov di, si` after `movzx si, byte [ebx+0x48]`
/ `sub esi, 0x20` — the u16 `si` — then `mov edx, edi` / `mov eax, ecx` /
`call 0x4b69c`; `0x49D03 mov al, [ecx+0x1e]` reads the type after it.

`0x4B69C`: `0x4B6A6..0x4B6B3` return unless `word [entry+0x1c] & 0x80`;
`0x4B6B9..0x4B6DD` the actor's pset (`[0x1014EC] + (rec+0x56) << 5`), its words
`+4` (x) and `+8` (y) sign-extended; `0x4B6E0 xor ebx,ebx`; `call 0x17d30`;
zero returns; `0x4B6F2 cmp eax,2 / jle` else 1 (both sides count as side 0);
`0x4B705 call 0x4b788` (EAX = hit, EDX = entry, EBX = si), zero returns;
`0x4B713` `+0x20` = hit − 1, `0x4B716 inc byte [ecx+0x1f]`; when the actor's
`+0x4A` is set (`0x4B71C`): `[0x1014F4] + k * 0x68 + 0x4B` = 0 (`0x4B73B`,
`13k * 8`), `+0x2A &= 0xF7`, `+0x29 &= 0xBF`, `+0x4A` = 0, the entry's `+0x1C
&= 0xBF`, `inc byte [0x1088ae + side]`, `[0x1088b2 + side]` = 1; then
`0x4B779 call 0x4b470` (EAX = entry, EDX = si).

`0x17D30` (EAX = x, EDX = y, EBX = `tall`): saves `0x100B08/0B0C/0B00/0B04`;
x and y divided by 64 with the Watcom truncating idiom (`sar edx,0x1f; shl
edx,6; sbb eax,edx; sar eax,6`), x − `0x18`, y − `0x38` (`0x30` when `BX` ≠
0); `0x17DAE` `0x15F48(0x128, 0x25, 0x100BD3, 0xFF)`; saves and clears
`0x100B63`, tests side 0 through `0x1790C(0, x, y, tall)` unless mode `0x22`
and `byte [0x104b1a]` ≠ 0 (bit 0); saves and clears `0x100B62`, restores
`0x100B63`, tests side 1 unless mode `0x22` and `[0x104b1a]` ≠ 1 (bit 1);
restores the four dwords and `0x100B62`.

`0x1790C` (EAX = side, EDX = x, EBX = y, ECX = `tall`) is `0x176CC`'s shape with
the point as the other box: `0x17934/0x1793E` need the side's `0x100AC8` box
`+2`/`+3` ≥ 1; `0x17957..0x17965` the side's sprite (`0x16308`); the box copy
clipped by `0x15C30`; `0x179E8 0x181D0(box2, 0x30, box0 + B08[side] − x, B1C;
B14, B28, B34, B24)` (`RET 0x10`; `ECX` = `0x100B1C` survives `0x15C30`'s
`push ecx`); the column plane (`0x61A70` zero, `0x15F48(box2, ⌈w/8⌉)`,
`0x15FD4` by box0 when non-zero); `0x17AB4 0x181D0(w, 0x30, B08[side] − x,
…)`; `0x17B0B 0x181D0(box3, tall ? 0x30 : 0x38, box1 + B00[side] − y, B18;
B10, B2C, B30, B20)`; `B10 += box1`, the `setg` flag `B14 > B34`, `B38 =
|B14 − B34|`, `B40 = B1C`; `0x17BAC 0x16DA4(flag, AF0[side], 0, B10; B30,
tall ? 3 : 2, side)` (mode 2/3: the other row is `0xA1740`/`0xA1746`,
`0f ff ff ff ff f0` / `00 ff ff ff ff 00`); `0x17BB8` 1 iff `B54 > 0`.
Every callee was already ported (`camera_box_clip`, `camera_sync_visible`,
`camera_bitplane_pixel/shift`, `camera_winner_height`, `camera_resolve_sprite`).
`0x1790C` has one caller (`0x17D30`); `0x17D30` has five (`0x129FC`,
`0x4B69C`, `0x4C60C`, `0x4D7A4`, `0x4E5A4`).

`0x4B788` (EAX = hit, EDX = entry, EBX = si) returns 1 at `0x4B7A0` when
`byte [0x105b3a]` > 1, at `0x4B7E0` when the other side's slot `+0x54` is 3,
and at `0x4B7F5` when the side's slot `+0x5F` ≠ `0xC97F2[(s8) slot+0x7A]`
(`2d` for chars 0..6); otherwise it returns 0 — at once when the entry's
`+0x1C` bit 6 is set, the fighter record's `+0x52` is outside the signed
`[0xC97E4[ch], 0xC97EB[ch]]` or its `+0x4B` is set, else after the grab
(`0x4B83D..0x4B98F`: `+0x1C |= 0x40`, the shadow killed, the worshipper placed
at the fighter's `0xC977D/0xC977E` offsets, type 8 on `0xC97AC[ch][si]`,
`0x2BD20`, the fighter on `0xC9790[ch]` at `0xC97C8[ch]`, a voice).

`0x4B470` (EAX = entry, EDX = si; three callers: `0x4B69C` at `0x4B779`,
case 8's held body at `0x4A10B`, and `0x4D7A4` at `0x4D873`): the voice `0x2C3FC(si < 3 ? 0xD1 : 0xD0)`;
`0x4B4B1 0x2BC30(rec, 0xC9604[si], 3.0)`; when `+0x10` is 0,
`0x4B4D2 0x2AE14(0xBB920[si], rec+0x18, rec+0x30 >> 16, 0; 0)` into `+0x10`;
unless mode `0x22` (`0x4B4E5`): on the first hit (`+0x1F` = 1)
`0x1A570(+0x20)` true gives `+0x34` = `0xFF80`, false `0x80`; on a later hit
`[rec+0x32] >> 16` = −`0x80` gives `0x80`, else `0xFF80`; `+0x36` = `0x240`,
type 6, `+0x1C &= 0x7F`; then the eighth-hit tail (`0x4B547..0x4B59E`: not
`0x104B1D` 2/3, `0x104AFC` = 0, `0x1088C1` = 0, `[0x1088ef] >> 24` > 1,
`+0x1F` > 7, `0x1088C5` = 0, `0x108864` = 0 → `0x4BD98`, `0x4CB18`).

Case 6, `0x49F11`: `+0x36` < 0 sets `+0x1C` bit 7; the shadow (`+0x10`)
takes `rec+0x18` and the word `rec+0x32`; `[rec+0x34] >> 16` + `rec+0x1C` > 0
subtracts `0x10` from `+0x36` (`0x49FB8`); otherwise the shadow dies
(`0x2B150`) and `+0x10` clears, the actor takes `0xC973C[si]` at 2.0 as type
8 (or, when that entry is 0, `0xC9544[si]` at 3.0 as type 4), and `+0x1C`
(the height dword), `+0x36`, `+0x34` and the entry's `+0x1F` are zeroed.
Case 8, `0x4A08A`: nothing without `+0x1C` bit 6. Tables (`read_memory`):
`0xC9604` = `0xEE160, 0xEE512, 0xEE87C, 0xEEC44, 0xEF00E, 0xEF400`;
`0xBB920` = `0xBB524, 0xBB538, 0xBB54C, 0xBB560, 0xBB574, 0xBB588`;
`0xC973C` = `0xEE39A, 0xEE6DA, 0xEEA96, 0xEEE88, 0xEF23E, 0xEF5E6`.

### 29.3 The fix and its assertions

`camera.c` gains `camera_point_side` (`0x1790C`) and `camera_point_hit`
(`0x17D30`, exported; `0x104B1A` is a local `#define`). `fight.c` gains
`fight_4b69c`, `fight_4b788` (the three return-1 gates and the return-0 gates;
the grab body is a named gap), `fight_4b470` (the voice a `PORT:` note; the
eighth-hit tail's `0x4BD98`/`0x4CB18` a named gap behind its gates), case 6
and case 8's bit-6 gate (its held body a named gap: bit 6 is set by the grab
arms — `0x4B788`'s, and `0x4D898`'s (`0x4D963 or byte [ecx+0x1c],0x40`) in
`0x4D2D0`'s pass, reached through `0x4D7A4` from `0x26C8C`/`0x26F58`, not the
`0x263F4` path, and not ported). The prelude runs before the type is read. About 0x7F0 raw bytes
(`0x4B69C` 0xE9, `0x17D30` 0x19D, `0x1790C` 0x2BB, `0x4B788`'s gates 0xB5,
`0x4B470` 0x138, case 6 0xB1, case 8's gate 0x13): five functions and two
cases, every callee already ported — inside the size gate.

New `check_point_trample` in `test_fight.c` (fixture `ph_seed` on
`pc_seed`'s 8x8 sprite; side 0's box raw (0, 36, 2, 3) clips to (0, 1, 8, 7)):
`camera_point_hit` at side 0's screen point returns 1 with B54 = 2 (7 rows ×
popcount(`0xFF & 0x0F`) = 28 → 29 → 34 → 2), B1C 8, B18 7, B30 1, B10 1 and the
flip bytes restored; a point `0x40` units right returns 0 and writes nothing;
box (4, 1, 4, 7) at p3 = `0x32` misses before writing B1C; both boxes give 3,
mode `0x22` with `0x104B1A` = 1/0 gives 2/1; `tall` misses on the `0xA1746`
row (B54 = 0) where the same y without it hits (B30 9); y at p3 = `0x34`
hits only with the `0x38`-row box (B18 4, B54 1). Through
`fight_effects_pass`: bit 7 clear skips the test (B54 sentinel) and case 4
counts; the grab move with `+0x52` in range does not trample; `0x105B3A` = 2
and the other `+0x54` = 3 do; the demo's trample (`+0x20` 0, `+0x1F` 1,
`+0x1C` `0x05`, the lie timer untouched because the type is read after the
prelude, `0xC9604[3]` at 3.0, `+0x34` `0xFF80`, `+0x36` `0x230` after case 6's
first step, the shadow from `0xBB920[3]` by its `+0x2C` word, no RNG draw);
case 6 airborne (the shadow follows, `0x100` → `0xF0`, `-0x20` → `-0x30` and
bit 7 set again); the landing (shadow dead, `0xC973C[3]` at 2.0, type 8, the
four zeroes) and its `0xC9544[3]` fallback; the second hit's reversal both
ways and no second shadow; both sides as side 0; mode `0x22` keeps `+0x34`;
the `+0x4A` release. The tables' entries 0 and 3, `0x104B00`, `0x104B1A`,
`0x105B3A`, `0x1088AE`, the held record's `+0x4B` and pc_seed's regions are
restored; the spawned shadows are killed and freed.

Mutations (`scratchpad/mut19.py`, 50 single-site edits plus one by hand,
each built and run, then `cmp`-restored): every one fails 1..43 assertions —
in `0x17D30` the `- 0x18`, the `tall` y offset, the `0x30`/`0x38` row box
swapped, mode 2/3 swapped, no `B10 += box1`, either flip byte not restored,
either mode-`0x22` gate inverted or retargeted, side 1 reported as bit 0, `>=
0` for `> 0`, box0 dropped from the first x sync, box1 dropped from dy; in
`fight.c` no bit-7 gate, no hit clamp, each of `0x4B788`'s three return-1
gates removed, `+0x20` = hit, no `+0x1F` count, table index 0 for
`0xC9604`/`0xBB920`/`0xC973C`/`0xC9544`, no spawn store, the first-hit arms
swapped, the reversal compare inverted, no mode-`0x22` gate, no `+0x36`, no
type 6, no `+0x1C &= 0x7F`, no prelude, the type read before the prelude, no
bit-7 set in case 6, no shadow follow (x, y), `>= 0` for the landing, no
gravity, no shadow kill or clear, type 4 / 3.0 for the `0xC973C` landing,
none of the four zeroes, and each of the five `+0x4A` release writes.
### 29.4 Measured

Port frames 0..1105 are byte-identical to the pre-fix dump, and 1106 (the
f = 689 state) is the first that differs. Captures 1563..1658 splice at 0 px
(1563 is port 1106/1107 row 154).

| measurement | before (`d9c5a1f`) | after (`cc38a38`) |
|---|---|---|
| captures 1563..1658 | 1563 514 px; 1564 514; 1566 284 | **all explained** |
| demo oracle first unexplained | 1563 (raw 4470); `[1563..3616]` 2054 / 2048 unexpl. | **1659 (raw 4566)**; `[1659..3616]` 1958 / 1952 unexpl.; port `[1189..1380]` (192, 0 exhibited) |
| demo-fight ratchet | `[1563..1884]` 322, N = 1563 | **`[1659..1884]` 226**, "ratchet improved: first unexplained 1659 > 1563", **N = 1659** |
| front-end oracle | `[560..1562]` / 1003 / 413 clean, 583 splice, 3 transition, 2 unexpl. | **`[560..1658]` / 1099 / 447 clean, 645 splice, 3 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. The three transition frames are the
same (`port832@row177`, `port862@row189`, `port906@row31`). The exhibition
set grows to port frames 0..1188 (952 exhibited). The ladder
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify`
exited 0 with 0 compiler warnings, with N = 1659 in the Makefile. Unmoved: title
`54/55/2/0` and `54/57/0/0`, determinism 54; smk 120/120 and 41/41; attract
215/216 (expected divergence at 215); C-vs-Python 9866; `symbols.h`; the
front-end "endpoints BAD" line.

**Unresolved code targets after the fix** (a temporary whole-run probe in
`fn_resolve`, reverted): `0x4AC80` (f = 820 and 841; Ghidra `get_xrefs_to`
lists no reference: a worshipper stream callback, EAX = the actor, its entry
at `+0x14`) and `0x3BF70` (f = 850), plus the known front-end sites
`0x29B74`/`0x41578`, the stub `0x5D812` and `fn_resolve(0)` (f = 617..767).
`0x14F50` (§28.4) no longer misses: the run moved from f = 689.

### 29.5 The new first unexplained frame, 1659 (characterised, not fixed)

Captures 1563..1658 splice at 0 px. Capture 1659's best splice, port
1188/1189 row 135, leaves 57 px in x 111–124, rows 135–142: the top-left
edge of the dark ring (the blue-black vortex left of the worshippers) differs.
From 1660 the gold fighter's leap and then the camera y diverge: 1660 leaves
5 279 px, 1661 10 954 and 1662 42 868 (at 1662 the capture's view sits
between port 1191's and 1192's and the gold fighter's tail is lower). Port 1188 is the
f ≈ 771 state. The owner is **not derived**; no `fn_resolve` miss falls
before f = 820 (`0x4AC80`, a worshipper stream callback reached after the
trample).

(Derived since, §30: the residual is not the ring. It is the gold fighter's
(the T-rex's) claw, 1 px left of the capture's, and the raptor 1 px right:
the two fighters are pushed apart in the raw. The owner is game_frame's
body push `0x3BB90` at `0x2541D`, which the port did not call; its first
push in the run is at f = 772, the T-rex's leap onto the raptor. Porting it
explains 1659..1714.)

## 30. The fighters' body push `0x3BB90` at capture 1659 (roar-timing Task 20, `1268371`)

**Result in one line.** Capture 1659 has one cause, and it is the port's.
game_frame's `DS_00104B15` tail calls `0x3BB90` at `0x2541D`, before
`0x12D48`, every frame of the demo fight; the port had the call as a
`PORT:` note ("cycle 2's") and no code. `0x3BB90` latches both slots, and
when the two latched points (slot `+0x2C`/`+0x30`) are closer than the sum
of the characters' `0xBEEF8` widths (each halved when the side's `+0x54` is
2), `0x4FB20`'s distance estimate gives the penetration and `0x3BAEC` moves
each side away from the other by half through `0x3B9D8` → `0x1883C`. The
first push in the run is at f = 772 (port frame 1189), the T-rex's
reaction-`0x2B` leap onto the flying raptor: width 1920, distance 1812,
penetration 108, so the T-rex moves +54 and the raptor −54 and the
raptor's speed is zeroed. Before f = 772 the fighters never overlap, so
the missing call moved nothing, which is why it went unnoticed. This
explains captures 1659..1714. `game_flow.md`'s "Cycle 2 landed ... `0x3BB90`,
`0x4FB20`, `0x3BAEC`, `0x3B9D8`" was stale: none of the four was in
`port/src`.

### 30.1 The measurement (temporary, reverted)

Capture 1659's 57 px (x 111–124, rows 135–142, below the 1188/1189 split
row 135, so they compare against port 1189) are gold claw pixels, not the
ring: 49 of the 57 capture pixels appear in port 1189 one pixel to the
left. In capture 1660 (split 1189/1190 row 163) a colour-split match over
the rows from port 1189 puts the gold fighter 1 px left in the port (2 999
of 3 074 gold pixels at dx −1) and the teal raptor 1 px right (672 of 715
at dx +1); in capture 1661 the gold fighter is 17 px left in the port with
the background at (0, 0), so the camera is the same. The fighters' own
streams agree: the capture's poses at f = 772/773 are the port's (the
`0xE5FD8`-driven frames `0x11B9`/`0x11BA`).

A `PR_T20` trace (in `fight_arena_frame`, `fight_hud_pass`, `game_frame`,
`fighter_pass_a` and the stream dispatcher; reverted) gives port 1188/1189
= f 771/772 (the T-rex's stream step to the upright frame is at f = 773 =
port 1190, confirming the offset 417). The T-rex (side 0) is in state
9/7/2 with `0x3E524` case 0 setting its speed to 10·v/7 each frame; the
raptor (side 1) flies at −160 per frame in state 17 → 16. The winner body
at f = 768 froze the T-rex's stream (`rec+0x24` = 0) until pass_b's
`0x1910E` arm at f = 772. The raw steps the capture implies are, at f =
772, [−800, −672] for the T-rex (port −842), and at f = 773 [−287, −161]
(port −1240 = speed −792 + the stream's op-`0x20` dx −7 from the
`0xE5FD8` table). The `0x19020`/`0x3E484` hook is inert again (f =
760..768: the other slot's `+0x74`, `+0x76` and `+0x42` are 0, and
`AF8[0]` is 24 at 768, so the raw's 1 and the port's 24 are both
non-zero).

With the fix, a probe at `0x3BC45` shows the first non-zero distance at f
= 772 (w 1920 = `0x800 >> 1` + `0x700 >> 1`, both `+0x54` = 2; d 1812),
then f = 773/774/775 and 895/897/900. Port frames 0..1188 are
byte-identical to the unfixed dump; captures 1655..1680 splice at 0 px.

### 30.2 The raw (Ghidra `disassemble_function` / `read_memory` + capstone, fixups applied)

`0x25414 cmp byte [0x104b15],0` / `je 0x2545c`; `0x2541D e8 6e 67 01 00
call 0x3bb90`; `0x25422 call 0x12d48`; then the per-side `0x186D0` /
`0x2A690` loop and `0x33F08`. `0x3BB90`'s only other caller is `0x25509`,
the `DS_00104B00` = `0x21` arm of the same function (unported with that
mode).

`0x3BB90`: `0x3BB95` `DS_00107D30` = 0; `0x3BBA2` return 0 unless `byte
[0x1078fa]` = 2; `0x3BBAB`/`0x3BBB9` EDX = `[0x1077a8]`, EBX = `[0x1077ac]`,
each non-zero; `0x3BBC9`/`0x3BBD3` `0x186D0(0)`, `0x186D0(1)` (it saves
EBX/EDX); `0x3BBD8`/`0x3BBE2` return 0 when either slot's `+0x42` bit 2 is
set; `0x3BBEF..0x3BC09` `0xD3388/0xD338C/0xD3390/0xD3394` = slot 0 `+0x2C`,
`+0x30`, slot 1 `+0x2C`, `+0x30`; `0x3BC16/0x3BC1E` the words
`0xBEEF8[char·4]` (`read_memory`: `0x800, 0x700, 0x900, 0x700, 0x900,
0x580, 0x600` for chars 0..6), each `shr 1` when its side's `+0x54` is 2,
summed into the word `0xD33A8`; `0x3BC40 call 0x4fb20`, `test ax,ax` →
return 0; else `0x3BC5C 0x3BAEC(word 0xD33A8 − d)`, return 1.

`0x4FB20`: dx = `0xD3388` − `0xD3390` → `0xD33A0`, |dx| → `0xD3398`, `jg`
return 0 when |dx| > w; dy likewise → `0xD33A4`/`0xD339C`, return 0 when
|dy| > w; then `sar 2` of the smaller, `sar 1` of that, plus the larger
(`0x4FB6D cmp ebx,eax / jg` picks the branch: |dy| ≤ |dx| uses |dy|'s
shifts); return that when w > it, else 0.

`0x3BAEC` (AX = penetration, `movsx`): returns when slot 0's or slot 1's
`+0x42` bit 2 is set; saves each slot's word `+0x40` bit 15; side 1 first
(`0x3B9D8(1)`, then `(0)`) when slot 0's `+0x43` bit 1 is set and slot 1's
is not, else side 0 first; ORs the saved bits back into the dwords
`0x1077F0`/`0x107884`.

`0x3B9D8` (EAX = side, EDX = penetration): `0x33950` context; returns when
the side's `+0x42` bit 5 is set; `+0x41 |= 0x80` on both slots; when the
other slot's `+0x43` bit 1 is set, `0x1883C(side, [other+0x4a] >> 16, 0)`
and `DS_00107D30` = 1; otherwise the half penetration (the truncating
`sar edx,0x1f / sub / sar 1`) away from the other (`0x3BA37 jl`: equal x
counts as the right side), unless `0x3B8D8` says that reaches the wall, in
which case the other side takes the opposite half; then, when the side's
`+0x54` is 2, the record's word `+0x34` is zeroed unless it points away
from the other (right side and < 0, or left side and > 0).

`0x3B8D8` (EAX = side, EDX = delta): 1 when slot `+0x2C` + delta ≥
`[0xbe018]` (`0x7C00`) or ≤ −`0x7C00`.

`get_xrefs_to`: `0x107D30` is written by `0x3BB90` and `0x3B9D8` only (no
reader); `0xD3388..0xD33A8` belong to `0x3BB90`/`0x4FB20` only.

### 30.3 The fix and its assertions

`fighter.c` gains `fighter_body_push` (`0x3BB90`, exported) and the statics
`fighter_4fb20`, `fighter_3baec`, `fighter_3b9d8` and `fighter_3b8d8`
(`0xD338C` is a local `#define`); `flow.c` calls it at `0x2541D`. About
0x33A raw bytes in five functions (`0x3BB90` 0xDD, `0x4FB20` 0x75,
`0x3BAEC` 0xA3, `0x3B9D8` 0x111, `0x3B8D8` 0x34), every callee already ported (`0x186D0`,
`0x1883C`, `0x33950`): inside the size gate.

New `check_body_push` in `test_fight.c` (fixture `bp_seed`: both slots
live, chars 0/3, `+0x54` = 2, the anchor path with the demo's f = 772
`DS_00100AB0/AB4` offsets 192/448 and 64/3200, the demo's speeds −842/−160,
`DS_00107D30` = 1, the scratch `0xA5`). A: the demo's f = 772 push (the
four scratch points, w 1920, dx/dy 936/1461, the side-0 +54 and side-1 −54
on the slots and records, the raptor's speed zeroed and the T-rex's kept,
both `+0x41` bit 7, `DS_00107D30` cleared), plus the speed gate's other
arms (+160 on the left kept, +7 on the right zeroed). B/C/D: the `|dx|`
gate at equality (dy still written) and past it (dy not read), the
estimate equal to w (no push), the `|dy|` gate, and the estimate at 1650
(push 135) and 1925 (none). E: the other branch with one halved width
(2816) and an odd penetration (665: −332/+332). F/G: `+0x42` bit 2 on each
slot (after the latches, before the scratch copy) and bit 5 on side 0. H:
the `+0x43` bit-1 arm and `0x3BAEC`'s order both ways. I: both walls at
equality and one unit inside, with an odd penetration (107). K: equal x.
J: `DS_001078FA` and each `DS_001077A8` slot. `check_game_frame_tail`
asserts the call (its `DS_00107D30` sentinel is cleared with the tail and
kept without it). The `0xD3388` scratch is restored.

Mutations (`scratchpad/mut20.py`, 47 single-site edits in `fighter.c` and
`flow.c`, each built and run, then `cmp`-restored): 46 fail 1..54
assertions after two assertions were added for the first run's survivors
(the side's own `+0x41` store and the `> 0` keep arm). The 47th, dropping
`0x4FB20`'s `|dy| > w` gate, is equivalent in the reachable domain: past it the estimate is at
least |dy| > w, so the final gate returns 0 and nothing is written after
it. Also unobservable, so not asserted: `0x3BAEC`'s own `+0x42` bit-2
gates (`0x3BB90` returned on the same bits first), its "slot 1's bit
clear" order term (with both bits set both calls take the additive `+0x4C`
arm) and the bit-15 OR-back (nothing in the push clears the bit).

### 30.4 Measured

| measurement | before (`6d8c382`) | after (`1268371`) |
|---|---|---|
| captures 1659..1714 | 1659 57 px; 1660 5 279; 1661 10 954; 1662 42 868 | **all explained** |
| demo oracle first unexplained | 1659 (raw 4566); `[1659..3616]` 1958 / 1952 unexpl. | **1715 (raw 4622)**; `[1715..3616]` 1902 / 1896 unexpl.; port `[1237..1380]` (144, 0 exhibited) |
| demo-fight ratchet | `[1659..1884]` 226, N = 1659 | **`[1715..1884]` 170**, "ratchet improved: first unexplained 1715 > 1659", **N = 1715** |
| front-end oracle | `[560..1658]` / 1099 / 447 clean, 645 splice, 3 transition, 2 unexpl. | **`[560..1714]` / 1155 / 455 clean, 693 splice, 3 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. The three transition frames are the
same (`port832@row177`, `port862@row189`, `port906@row31`). The exhibition
set grows to port frames 0..1236 (1000 exhibited). The ladder
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify`
exited 0 with 0 compiler warnings, with N = 1715 in the Makefile. Unmoved: title
`54/55/2/0` and `54/57/0/0`, determinism 54; smk 120/120 and 41/41; attract
215/216 (expected divergence at 215); C-vs-Python 9866; `symbols.h`; the
front-end "endpoints BAD" line.

**Unresolved code targets after the fix** (a temporary whole-run probe in
`fn_resolve`, reverted): `0x4AC80` (f = 820 and 841, as before) and
`0x3C0A4` (f = 850; no code cross-reference in Ghidra; it missed at f = 850
on the frame-1481 fix's run too), plus the known front-end sites
`0x29B74`/`0x41578`, the stub `0x5D812` and `fn_resolve(0)` (f = 617..767).
`0x3BF70` (f = 850 on the frame-1563 fix) no longer misses: the run moved
from f = 772.

### 30.5 The new first unexplained frame, 1715 (characterised, not fixed)

Captures 1659..1714 splice at 0 px. Capture 1715's best splice, port
1236/1237 row 144, leaves 383 px, almost all in x 40–99, rows 144–190: a
standing teal-clad worshipper at x ≈ 45–60 in the capture sits at x ≈
75–88 in port 1237 and is gone from port 1238; a lying worshipper beside
it matches. Port 1237 is the f = 820 state, and f = 820 is the first
`fn_resolve` miss of the run (§29.4: `0x4AC80`, a worshipper stream
callback with no code cross-reference, EAX = the actor, its entry at
`+0x14`). That callback is the candidate owner; it is not derived.

(Derived since, §31: the owner is `0x4AC80`, the worshipper landing
streams' opcode-`0x15` target. At f = 820 and 841 it ends a landed
worshipper's type-8 landing with the climb (type 5, the `0xC95EC` stream);
the port skipped it, so the worshipper stayed on its landing stream. Porting
it explains 1715..1749.)

## 31. The worshipper landing target `0x4AC80` at capture 1715 (roar-timing Task 21, `2287114`)

**Result in one line.** Capture 1715 has one cause, and it is the port's.
The six worshipper landing streams end in `D500 AC80 0004` (opcode `0x15`,
mode `0x4000`), and the port had not registered `0x4AC80`, so
`anim_indirect` skipped it. In the demo the call comes at f = 820 and 841,
once for each side-1 worshipper that the trample (§29) threw and case 6
landed as type 8 on its `0xC973C` stream. Both calls take `0x4AC80`'s
climb arm: the worshipper takes its `0xC95EC` rising stream with `+0x38` =
`0x40`, the entry becomes type 5 and `+0x1C` is masked with `0x3F`. Case 5
(§28) then brings it back to its standing stream. The port left both
worshippers on the landing stream in type 8. This explains captures
1715..1749.

### 31.1 The measurement (temporary, reverted)

A `PR_T21` trace (in `fight_4ac80`, `fight_arena_frame` and `fn_resolve`;
reverted from pre-trace copies) shows the two calls:
- f = 820 (arena frame 756): entry `0x108438`, index 4, `+0x1C` = `0x80`,
  type 8, side (`+0x21`) 1
- f = 841: entry `0x108414`, index 3, the same flags

Both have `+0x1C` bit 5 clear, `DS_00104B00` = 3 and `DS_001088C5` = 0.

With the fix, port frames 0..1236 are byte-identical to `0d7c865`'s dump.
Port 1237 (f = 820) is the first that differs, and captures 1715..1749
splice at 0 px.

### 31.2 The raw (Ghidra `read_memory` + capstone, fixups applied)

`0x4AC80` is not a Ghidra function. It spans `0x4AC80..0x4AEAA` (0x22B
bytes) and returns at `0x4ADB7`, `0x4ADF0`, `0x4AE40`, `0x4AE56` and
`0x4AEAA`. A scan for the dword `0x0004AC80` over the code object and the
data object finds six sites, each after a `0xD500` word: `0xEE3BC`,
`0xEE6FC`, `0xEEAB8`, `0xEEEAA`, `0xEF260` and `0xEF608`. There is no code
reference. Each site closes one of the six `0xC973C[i]` streams, the case-6
landing streams, 0x22 bytes after its start (`read_memory` of `0xC973C`:
`0xEE39A`, `0xEE6DA`, `0xEEA96`, `0xEEE88`, `0xEF23E`, `0xEF5E6`).

**Entry.** EAX = the record. EDX is pushed at `0x4AC82` and loaded from
`rec+0x14` at `0x4AC8B` before any read. The routine returns when that entry
is 0 (`0x4AC90`). The index is the 32-bit `(u8)rec+0x48 − 0x20` (`0x4AC98`
`xor edx,edx` / `0x4AC9A mov dl` / `0x4ACB1 sub edx,0x20`). The entry's
actor (`+8`) gets `+0x29 &= 0xBF` and then `|= 0x10` (`0x4ACA0`/`0x4ACA7`).

**Arm selection.** The byte `DS_001088C5` (`0x4ACAB`) and the entry's
`+0x1C` bit 5 (`0x4ACBC..0x4ACC9`, `and al,0x20`) pick one of three arms:

- **Bit 5 set, `DS_001088C5` ≠ 0** (`0x4ACCF..0x4ADB7`). Let pos =
  `0x2BE00(actor)` and base = `0x2BE00([0x108868])`. The walk target is
  - base − `0x1740` when pos < base − `0x1740`, or when base − `0xBA0` <
    pos < base
  - base + `0x1740` when pos > base + `0x1740`, or when base < pos < base +
    `0xBA0`
  - 0 otherwise

  All compares are signed (`jge`/`jle`/`jg`). A non-zero target is stored
  in `+0x14` and the entry becomes type 1. The actor's `+0x34` is `0x40`
  with bit 6 clear when pos < target (`0x4AD4F jge`), else `0xFFC0` with
  bit 6 set, and the stream is `0xC95D4[i]`. A zero target calls
  `0x2C3FC(0xC8)` (a voice), sets type 8 and actor `+0x55` = 1, and takes
  the `0xC958C[i]` stream. Both then begin the stream at 3.0 and zero
  `+0x36`.
- **Bit 5 set, `DS_001088C5` = 0** (`0x4ADC7..0x4ADF0`). The `DS_00108864`
  entry gets word `+0x18` = 0, `+0x1C &= 0xDF` and type 4, and
  `DS_00108864` is cleared.
- **Bit 5 clear** (`0x4ADF1..0x4AEAA`). The word `DS_00104B00` is
  zero-extended; EAX's high half is already 0 from `0x4ADC0`.
  - Modes 8, 9 and `0x17`: the actor's `+0x38`, `+0x34` and `+0x36` are
    zeroed. Then `0x4B3F0` runs when the byte `DS_00104B16` equals the
    entry's `+0x21`, else `0x4B430`, both with EBX = 1 (`0x4AE2B`,
    `0x4AE41`).
  - Any other mode: the climb. The actor begins `0xC95EC[i]` at 3.0. EBX
    (the record, not the actor) gets `+0x38` = `0x40` (`0x4AE6B`) and
    `+0x34` = `0xFFC0` or `0x40` by the fighter record's `+0x28` bit
    `0x4000` (`0x4AE86`/`0x4AE8E`). The entry becomes type 5, and `+0x1C &=
    0x3F` (`0x4AE9B`) clears bits 7 and 6, where case 4 at `0x49EB8` clears
    only bit 7.

**Correction (raw wins).** The port's `fight_4b3f0`/`fight_4b430` wrote a
hard-coded 0 into the actor's `+0x55`. The raw tests EBX (`0x4B3FA`/`0x4B43A`
`test ebx,ebx`) and writes 1 when EBX is non-zero (`0x4B401`/`0x4B441`).
`0x4AAD0`'s four calls zero EBX (`0x4AAFC`, `0x4AB18`, `0x4AB6D`, `0x4AB97`
`xor ebx,ebx`; re-read), so their behaviour does not change.

**Globals** (`get_xrefs_to`):
- `DS_001088C5` is written only by `0x4BD98` and `0x4CD98`.
- `DS_00108868` is written only by `0x4BD98` (`0x4BEA3`).
- `DS_00108864` is written by `0x4BD98`, `0x4CD98`, `0x4C784`, `0x27BA4` and
  `0x4AC80` itself.

All of these writers are unported apart from `0x4AC80`. In the demo only the
climb arm runs.

### 31.3 The fix and its assertions

The fix adds `fight_4ac80` to `fight.c` (exported) and the wrapper
`anim_code_4AC80` with `fn_register(0x4AC80)` to `actors.c`.
`fight_4b3f0`/`fight_4b430` take the raw's EBX flag, and `0x4AAD0`'s calls
pass 0. That is 0x22B raw bytes in one new function, and every callee
(`0x2BE00`, `0x2BC30`, `0x4B3F0`, `0x4B430`) was already ported, so the
change is inside the size gate.

`check_worshipper_landing` is new in `test_fight.c`. Its fixture `wl_seed`
extends `wa_seed` with the entry's slot and fighter record, `+0x1C` =
`0xD0`, side 1, actor `+0x55` = `0x5A`, a `DS_00108868` base record (pset 5),
a `DS_00108864` held entry with sentinel fields, the mode word with
`DS_00104B02` = `0x1234`, and crafted literal-id streams in the four tables.
The parts:
- **A.** The demo's f = 820 climb (index 4).
- **A2.** The `+0x28` bit `0x4000` arm, and index `0xFFFFFFFF`.
- **A3.** `entry+8` ≠ rec: the stream and `+0x29` go to the actor, and
  `+0x38`/`+0x34` go to EAX's record.
- **B.** Modes 8, 9 and `0x17` hold through `0x4B430` (side ≠
  `DS_00104B16`) and through `0x4B3F0` (side equal), with `+0x55` = 1.
  Modes 7 and `0x18` climb.
- **B2.** An empty hold stream.
- **C.** The `DS_00108864` release.
- **D.** The twelve band points at base `0x10000`: each band edge and one
  unit inside it, plus a far point.
- **D2.** The signed compare (base `0x2000`, pos −`0x100`), and the zero
  target that holds (base `0x1740`, pos −5).
- **D3.** The walk on `entry+8`.
- **E.** No entry.
- **F.** The call through the dispatcher (`D500 AC80 0004`).

The registration check also asserts that `0x4AC80` resolves to the
`(rec, arg)` wrapper. The tables, the mode word, pset 5 and the globals are
restored afterwards.

**Mutations.** `scratchpad/mut21.py` made 59 single-site edits in
`fight.c` and `actors.c`. Each was built and run, and both files were then
restored with `cmp`. 58 fail 1..115 assertions. The 59th, `0x4AD4F`'s `jge` taken at equality (`<` made `<=`), is equivalent in the reachable domain: every band's target differs from pos (pos < base − `0x1740` = target; base − `0xBA0` < pos < base against base − `0x1740`; pos > base + `0x1740` = target; base < pos < base + `0xBA0` against base + `0x1740`), so pos = target never reaches the compare. Not asserted: `0x4AAD0`'s EBX = 0 (its hard-coded 0 is unchanged and no existing test drives its hold arms), and `0x4AD81`'s voice `0x2C3FC(0xC8)`, a `PORT:` skip like every other voice (spec §7).

### 31.4 Measured

| measurement | before (`0d7c865`) | after (`2287114`) |
|---|---|---|
| captures 1715..1749 | 1715 383 px | **all explained** |
| demo oracle first unexplained | 1715 (raw 4622); `[1715..3616]` 1902 / 1896 unexpl. | **1750 (raw 4657)**; `[1750..3616]` 1867 / 1861 unexpl.; port `[1267..1380]` (114, 0 exhibited) |
| demo-fight ratchet | `[1715..1884]` 170, N = 1715 | **`[1750..1884]` 135**, "ratchet improved: first unexplained 1750 > 1715", **N = 1750** |
| front-end oracle | `[560..1714]` / 1155 / 455 clean, 693 splice, 3 transition, 2 unexpl. | **`[560..1749]` / 1190 / 468 clean, 715 splice, 3 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. The three transition frames are the
same (`port832@row177`, `port862@row189`, `port906@row31`). The exhibition
set grows to port frames 0..1266 (1030 exhibited). The ladder
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify`
exited 0 with 0 compiler warnings on `2287114`, with N = 1750 in the Makefile. Unmoved: title
`54/55/2/0` and `54/57/0/0`, determinism 54; smk 120/120 and 41/41; attract
215/216 (expected divergence at 215); C-vs-Python 9866; `symbols.h`; the
front-end "endpoints BAD" line.

**Unresolved code targets after the fix.** A temporary whole-run probe in
`fn_resolve` (reverted) found:
- `0x3C0A4` at f = 850, as before
- new later misses: `0x14F50` (f = 929) and `0x3A820` (f = 962/963)
- the known front-end sites `0x29B74`/`0x41578`, the stub `0x5D812`, and
  `fn_resolve(0)`

`0x4AC80` no longer misses.

### 31.5 The new first unexplained frame, 1750 (characterised, not fixed)

Captures 1715..1749 splice at 0 px. Capture 1750's best splice, port
1266/1267 at row 153, leaves 5 544 px in x 0–286, rows 153–199. Captures
1751 and 1752 leave 10 703 and 11 165 px. Below row 153 the capture compares
against port 1267, the f = 850 state:
- The capture's gold T-rex keeps port 1266's place and upright pose.
- Port 1267 has the T-rex further left in another pose, and port 1268 has
  it crouched.
- The unshifted comparison of rows 153–199 is the best match (63%), so the
  camera is not the cause.

f = 850 is the run's `0x3C0A4` `fn_resolve` miss (no code cross-reference;
§30.4), which makes that miss the candidate owner. It is not derived.

(Derived since, §32: the candidate holds. `0x3C0A4` is `0x34E2C`'s
reaction-`0x3E` callback. At f = 850 it starts the T-rex's `0xC8B30` attack
through `0x3BF70` and turns its `+0x4E` facing; the port skipped it, so the
T-rex kept its place and pose. Porting it explains 1750..1762.)

## 32. The reaction callbacks `0x3C0A4`/`0x3BF70` at capture 1750 (roar-timing Task 22, `c7320b2`)

**Result in one line.** Capture 1750 has one cause, and it is the port's.
`0x34E2C`'s `(char, reaction)` table holds `0x3C0A4` as reaction `0x3E`'s
callback, and the port had not registered it, so the `0x35045` call
skipped it. At f = 850 the T-rex (side 0) takes reaction `0x3E`.
`0x3C0A4` runs `0x3BF70`, the forced attack: the `0xC8B30[char]` stream at
hold 2.0 through `0x3C4CC`, the `0xBEFA0` row in `DS_00107D40`, and state
3/4/2. It then turns the slot's `+0x4E` facing the other way. The port left
the T-rex where it was. This explains captures 1750..1762.

### 32.1 The measurement (temporary, reverted)

A `PR_T22` trace printed each `fn_resolve` miss with its return address
(`dladdr`) and `DS_0010150C`. It was reverted from a pre-trace copy of
`mem.c`.
- `0x3C0A4` misses once, at f = 850, from `hit_reaction_apply` (`0x34E2C`).
  That is the reaction callback call at `0x35045` (`call [esp+0x24]`), with
  EAX = ctx[2] (the slot, `0x35037`), EBX = ctx[0] (the side, `0x3503B`) and
  EDX = ctx[4] (the record, `0x3503E`).
- The later misses were `0x14F50` (f = 929, the same site), `0x3A820`
  (f = 962/963, `0x3531C`), the front-end `0x29B74`/`0x41578`, and the stub
  `0x5D812`. `0x3BF70` does not miss.

A second trace in `fighter_3c0a4`, after the fix, shows one call at f = 850:
- slot `0x1077B0` (side 0, char 0), and `0x3BF70` returned 1
- the side's pset is flipped (`0x1A570` = 0), so `0x3BF70` wrote `+0x4E` = 1
  and `0x3C0A4` changed it to `0xFFFF`
- the slot's `+0x52` was 3 after the call, and the record's stream was at
  `0xE6F28` (`0xC8B30[0]`)

With the fix, port frames 0..1266 are byte-identical to the unfixed dump.
Port 1267 (f = 850) is the first that differs, and captures 1750..1762
splice at 0 px.

### 32.2 The raw (Ghidra `read_memory` + capstone, fixups applied)

Neither `0x3C0A4` nor `0x3BF70` is a Ghidra function.

**Where the callbacks live.** A scan for the dword `0x0003C0A4` over both
objects finds seven sites: `0xA3A00`, `0xA3F00`, `0xA4400`, `0xA4900`,
`0xA4E00`, `0xA5300` and `0xA5800`. They are one per character, at stride
`0x500` (64 records of `0x14` bytes) in `0x34E2C`'s `0xA3528` table.
`0xA3A00` = `0xA3528 + 0x3E * 0x14` is char 0's reaction `0x3E`, and its
stream word `+4` is 0. The dword `0x0003BF70` is in the next record of
each character (reaction `0x3F`, `0xA3A14`, also with no stream), and
`0x0003C048` is in the previous one (reaction `0x3D`). `get_xrefs_to` finds
no code reference to `0x3C0A4` or `0x3C048`. `0x3BF70` has two calls, at
`0x3C05E` (in `0x3C048`) and `0x3C0BD` (in `0x3C0A4`).

**`0x3C0A4`** (`0x3C0A4..0x3C0E9`, 0x46 bytes). The registers are EAX =
slot (moved to ESI), EDX = rec (EDI) and EBX = side (ECX).
- `0x3C0B4` builds `0x33950(esp, side)`. The context is not read.
- `0x3C0BD` calls `0x3BF70(slot, rec, side)`, and DL keeps the result.
- When the result is non-zero, `0x3C0CA` calls `0x1A570(side)`. Non-zero
  gives word `slot+0x4E` = 1 (`0x3C0D3`), and zero gives `0xFFFF`
  (`0x3C0DB`). This is the reverse of `0x3BF70`'s sense.
- It returns AL = DL.

**`0x3BF70`** (`0x3BF70..0x3C046`, 0xD7 bytes). The registers are EAX =
slot (moved to ECX), EDX = rec (ESI) and EBX = side.
- ctx = `0x33950(side)`. When `ctx[2]+0x40` bit 7 is set, it returns AL = 0
  (`0x3BF86`/`0x3BF8C`). The side's slot is tested, not the EAX slot.
- The side's record (`DSD(0x1077B0 + side*0x94)`, `0x3BFA4`) gets word
  `+0x34` = 0 and bytes `+0x43`/`+0x42` = 0 (`0x3BFAB..0x3BFB5`).
- `ctx[2]+0x5F` = `0xFF` (`0x3BFBF`).
- `DSD(0x107D40 + side*4)` = `0xBEFA0 + 6 * DSB(0x10782A + side*0x94)`
  (`0x3BFD3..0x3BFEA`). `0x3BFD1`'s `xor edx,ebx` zeroes EDX first, because
  EDX = EBX at `0x3BFBD`. The byte is the side's char. `0xBEFA0` is a third
  7×3-word row table after `0xBEF28` and `0xBEF64`; the T-rex's row is
  `0x34`, `0x2D8`, `0x156`.
- `0x3C4CC(rec, DSD(0xC8B30 + 4 * DSB(slot+0x7A)), 2.0)`
  (`0x3BFF3..0x3C004`). This uses the EAX slot's char. `0x3C4CC` returns
  with `ret 4` (`0x3C51C`), and it reads `+0x52` before the next write.
- The EAX slot gets `+0x52` = 3, `+0x54` = 2, `+0x53` = 4 and `+0x40 |=
  0x80` (`0x3C009..0x3C01D`). Byte `0x1078F8+side` = 1 (`0x3C020`).
- `0x1A570(side)` (`0x3C028`): non-zero gives `slot+0x4E` = `0xFFFF`
  (`0x3C031`), and zero gives 1 (`0x3C039`).
- It returns AL = 1.

`0x33950` changes EDX (`0x3399B..0x339A5`), and both routines reload it.
This is `0x3BDDC`'s attack state (§17) without the command word or the
`0x3CF38` chain. The attack stream's `0xD000` target `0x35E04` (§18) then
launches it with the `0xBEFA0` row.

### 32.3 The fix and its assertions

The fix adds `fighter_3bf70` and `fighter_3c0a4` to `fighter.c`, after
`fighter_3c190`, with a local `FIGHT_ROW_3BF70` = `0xBEFA0` (`symbols.h`
names no global there). Both are exported in `fighter.h`. `actors.c` gets
the void wrappers `reaction_cb_3BF70`/`reaction_cb_3C0A4` and registers
both. The wrappers drop AL, which the `0x35045` call ignores (`0x35049`
only adds to ESP). That is 0x11D raw bytes in two new functions, and every
callee (`0x33950`, `0x3C4CC`, `0x1A570`) was already ported, so the change
is inside the size gate. `0x3C048` (reaction `0x3D`) is not reached in the
current run and is not ported.

`check_reaction_attack` is new in `test_fight.c`. Its fixture `ra_seed`
extends `tb_seed` with sentinels in:
- both records' `+0x34`/`+0x43`/`+0x42`
- the slots' `+0x40`/`+0x4E`, `+0x52` = 0 and `+0x7A` = chars 1 and 2
- `DS_00107D40` and the two `DS_001078F8` bytes
- crafted literal-id streams (id `0x1100` + char) in the seven `0xC8B30`
  entries

The parts:
- **A/A2.** `0x3BF70`, unflipped and flipped. The flip comes from the
  record's `+0x29` bit 6 through the begin, over an unflipped seeded pset,
  so `0x1A570` must follow `0x3C4CC`.
- **B.** `0x3C0A4` in both senses.
- **C.** The bit-7 reject, for both routines: nothing is written.
- **D/D2.** EAX = slot 1, EDX = rec 1, EBX = side 0. The gate, the cleared
  record, `+0x5F`, the row, `DS_001078F8` and `0x1A570` are side 0's. The
  stream, the state, `+0x40` and `+0x4E` are slot 1's. `0x3C4CC` takes its
  `0x3C480` arm by record 1's side (`+0x52` = 9), so `+0x1C` is zeroed.
- **E.** Side 1 on its own slot: `DS_00107D44` and `DS_001078F9`.
- **F.** Through `hit_reaction_apply`, reactions `0x3E` and `0x3F` on the
  real `0xA3A00`/`0xA3A14` records.

The registration check asserts that both addresses resolve to wrappers.
The table, the globals and the fixture state are restored afterwards.

**Mutations.** `scratchpad/mut22.py` made 37 single-site edits in
`fighter.c` and `actors.c`. Each was built and run, and both files were
then restored with `cmp`. 36 fail 1..17 assertions. The 37th, the
`0x3BF70` wrapper passing `DSD(slot)` for rec, is equivalent at the only
call site, because `0x35045`'s EDX is ctx[4] = `[ctx[2]]`
(`0x3399B..0x3399D`), the EAX slot's own record.

**Task 21's parked minors** are fixed in `1bb5b9e`:
- `check_worshipper_arrival` E and `check_worshipper_landing` E now seed
  and restore linear byte `0x1C`. In `0x4AC80`'s E it is 0, bit 5 clear, so
  a missing entry test writes `+0x1E`.
- The dead fallback `fn_register(0x4AC80, fight_4ac80)` is gone. Removing
  the real registration now also fails part F.
- The `0x4FB4B`/`0x3BBF7` comment columns in `fighter.c` are realigned.

The `fight.c` minors are still parked, since this task did not touch
`fight.c`. They are the `0x4AAD0` flag-0 test, the s32 band arithmetic at
`0x4ACFA..0x4AD37`, and the `0x4B3F0`/`0x4AC80` comments.

### 32.4 Measured

| measurement | before (`4944d2b`) | after (`c7320b2`) |
|---|---|---|
| captures 1750..1762 | 1750 5 544 px | **all explained** |
| demo oracle first unexplained | 1750 (raw 4657); `[1750..3616]` 1867 / 1861 unexpl. | **1763 (raw 4670)**; `[1763..3616]` 1854 / 1848 unexpl.; port `[1278..1380]` (103, 0 exhibited) |
| demo-fight ratchet | `[1750..1884]` 135, N = 1750 | **`[1763..1884]` 122**, "ratchet improved: first unexplained 1763 > 1750", **N = 1763** |
| front-end oracle | `[560..1749]` / 1190 / 468 clean, 715 splice, 3 transition, 2 unexpl. | **`[560..1762]` / 1203 / 473 clean, 723 splice, 3 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. The three transition frames are the
same (`port832@row177`, `port862@row189`, `port906@row31`). The exhibition
set grows to port frames 0..1277 (1041 exhibited). The ladder
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify`
exited 0 with 0 compiler warnings on the `c7320b2` tree, with N = 1763 in
the Makefile. Unmoved: title `54/55/2/0` and `54/57/0/0`, determinism 54;
smk 120/120 and 41/41; attract 215/216 (expected divergence at 215);
C-vs-Python 9866; `symbols.h`.

**Unresolved code targets after the fix.** The same probe found:
- the new miss `0x3E3A8` at f = 962, from `hit_reaction_apply`
- the front-end `0x29B74`/`0x41578`, and the stub `0x5D812`

`0x3C0A4`, `0x14F50` and `0x3A820` no longer miss. The T-rex's changed
fight from f = 850 no longer reaches the old two.

### 32.5 The new first unexplained frame, 1763 (characterised, not fixed)

Captures 1750..1762 splice at 0 px. Capture 1763's best splice, port
1277/1278 at row 130, leaves 153 px, all in x 15–22, rows 53–98. Captures
1764..1766 leave the same 153 px in the same box. Every differing capture
pixel is green (0, 203, 0). The capture draws a vertical "2 HIT COMBO" at
the left edge, and the port draws nothing there. The rest of the frame,
including both fighters and the camera, matches.

"COMBO" is at `0xBE01D`, and its `0x1B`-prefixed string at `0xBE01C` has
one cross-reference, `0x38E1D` in `0x38D90`. `0x38D90`'s one caller is
`0x39040`, and a temporary trace shows the port's `fighter_39040` gated
body running once, at f = 860 (port 1277), for side 0 with
`DSW(0x107D2C)` = 2: the T-rex's second hit. The port skips `0x390E8`'s
`0x38D90(side)` and `0x390EF`'s `0x38FEC(side)` as a `PORT:` named gap,
the `0x2F4D0`/`0x2EFD4` text-grid formatter that `fighter_39040`'s
comment declares. That draw is the candidate owner. It is not derived.

(Derived since, §33: the candidate holds. `0x38D90` draws the hit count,
the HIT glyph and "COMBO" down the text grid's column 2, and `0x38FEC`
checks the combo names; the port skipped both. Porting them, with the
`0x2F4D0`/`0x2EFD4` formatter and the vertical `0x2F20C`/`0x2F314`,
explains 1763..1880.)

## 33. The combo text `0x38D90`/`0x38FEC` at capture 1763 (roar-timing Task 23, `bcce10b`)

**Result in one line.** Capture 1763 has one cause, and it is the port's.
`0x39040` draws the combo text through `0x38D90` and checks the combo
names through `0x38FEC`, and the port skipped both as a `PORT:` named gap
(the "`0x2F4D0`/`0x2EFD4` text-grid formatter"). At f = 860 the T-rex
(side 0) lands its second hit, and the raw draws "2", the HIT glyph and
"COMBO" down the text grid's column 2. The closure is small: the glyph
grid (`0x2F0F0`/`0x2F198`/`0x2F280`/`0x2F4BC`/`0x2F5A0`/`0x2F830`) and the
string table (`0x1C500`/`0x474E4`) were already ported, so the gap was ten
functions. This explains captures 1763..1880.

### 33.1 The measurement (temporary, reverted)

A `PR_T23` trace printed each `fn_resolve` miss with `DS_0010150C`, and
the new routines' inputs. It was reverted from pre-trace copies of `mem.c`
and `fighter.c`, and the dump with the trace is byte-identical to the one
without it.
- `0x38D90` runs once, at f = 860 (port frame 1277), for side 0, with the
  hit count `DSW(0x107D2C)` = 2, `DSW(0x107D20)` = 28 and slot `+0x63` = 1.
  So the raw draws no "28%" row.
- `0x38FEC` walks all seven T-rex records, and none names a combo: every
  threshold (7, 6, 4) is above 2 hits.
- `0x38D24`'s timer (`0xB4`, set at `0x390E1`) does not reach 0 before the
  dump ends.
- The only code-target miss left is `0x3E3A8`, at f = 962.

With the fix, port frames 0..1276 are byte-identical to Task 22's dump.
Port 1277 (f = 860) differs in exactly 153 px, in x 15–22 and rows 53–98,
which is the capture's miss box. Frames 1277..1380 all differ, because the
text stays up.

### 33.2 The raw (Ghidra disassembly, fixups applied)

**`0x38D90`** (`0x38D90..0x38ECD`). EAX = side, kept in EBP; c = side *
0x25.
- `0x38D98` calls `0x38C5C(side)`, the clear.
- `0x38DAC`/`0x38DCC` call `0x1C500(0xE5)`, and `0x2F4BC(-1, 6 / 7, s,
  0x3000)` draws it centred on rows 6 and 7. `0x1C500` keeps EDX and ECX
  (`push edx` at `0x1C501`, and `0x474E4` saves ECX at `0x474E4`).
  String `0xE5` is 60 spaces, so both draws are clears.
- `0x38E13` calls `0x2F4D0(c + 2, 8, value, 2)` with the stack pad 3 and
  mode `0x1000` (pushed `0x1000`, then 3). The value is the hit count,
  `[esi+0x107D2A] sar 0x10` with ESI = side * 2, i.e. the signed word at
  `0x107D2C + side*2`.
- `0x38E29` calls `0x2F20C(c + 2, 9, 0xBE01C, 0x1000)`. The string is
  `1B 43 4F 4D 42 4F` ("\x1bCOMBO"), and `0x1B` is the font's HIT glyph.
- When slot `+0x63` (`0x107813 + side*0x94`, `0x38E3E`) is 0, it calls
  `0x2F4D0(c, 0x10, (s16)DSW(0x107D20 + side*2), 3)` with pad 1 and mode
  `0x2000`, then `0x2F4BC(c + 3, 0x10, 0x80BF0 "%", 0x2000)`. The `cmp
  edx,0x64` at `0x38E5C` and `cmp edx,0xa`/`cmp edx,0x64` at
  `0x38E89`/`0x38E8E` feed no branch: both arms pass col EDI - 2 = c.

**`0x38C5C`** (`0x38C5C..0x38CD8`). c = side * 0x25.
`0x2F280(c + 2, 8, 0x80BE0 "  ", 0x2000)`,
`0x2F314(c + 2, 9, 0x80BE4 (six spaces))`,
`0x2F280(c, 0x10, 0x80BEC "   ", 0x2000)` and
`0x2F280(c + 3, 0x10, 0x80BEC, 0x2000)`.

**`0x38D24`** (`0x38D24..0x38D8F`). When `DSW(0x107D18 + side*2)` is non-zero
(`0x38D35`), it is decremented (`0x38D3E`). The step that reaches 0
(`0x38D45`) calls `0x38C5C(side)` and redraws string `0xE5` on rows 6 and 7.
Its callers are `0x35658` (`0x357F5`, the port's `fight_hud_pass`) and
`0x384F8` (from `0x266AC`, not in the demo).

**`0x38FEC`** (`0x38FEC..0x3903C`). ctx = `0x33950(side)`, char =
`DSB(ctx[2] + 0x7A)`. The count is the dword at `0xBEBB6 + char*2` `sar
0x10` (`0x39008`/`0x39011`), i.e. the signed word at `0xBEBB8 + char*2`.
The records are `DSD(0xBEB90 + char*4)`, stride `0x44`. It calls
`0x38ED0(ctx[0], rec)` for each until AL is non-zero (`0x39029`).

**`0x38ED0`** (`0x38ED0..0x38FEB`). EAX = side, EDX = the record.
- For i = 0..0x13, the id byte `rec[0x1B + i]`:
  - not `0xFF`: when `DSB(0x107A80 + side*0x40 + id)` is below
    `rec[0x2F + i]` (`0x38FC3..0x38FD5`), it returns 0.
  - `0xFF`: for j = 0..2, the first threshold `rec[0x18 + j]` at or below
    the hit count (`0x38F12` `cmp dx,cx; jg`) names the combo. With slot
    `+0x63` clear it redraws `0xE5` on rows 6/7, then string `DSD(rec +
    4j)` on row 6 and `DSD(rec + 0xC + 4j)` on row 7, centred, mode
    `0x3000`. It returns 1 either way (`0x38FA2`). No threshold met
    returns 0.
- 0x14 ids with no terminator return 0.
- The T-rex's first record (`0xBE024`) has thresholds 7/6/4, the list
  [42] with need 1, and the names `0xE6`/`0xE7` ("TAKE A BITE"/"OUTTA
  CRIME!"), `0xE8`/`0xE9` ("EXTRA CRUNCHY"/" ") and `0xEA`/`0xEB`
  ("CRUNCHY"/" ").

**`0x2F4D0`** (`0x2F4D0..0x2F50A`, `ret 8`). EAX = col, EDX = row, EBX =
value, ECX = width, `[esp+0x24]` = pad (the last push) and `[esp+0x28]` =
mode. It saves the cursor `DS_00105F34` (`0x2F4E4`), formats into a
0x14-byte stack buffer through `0x2EFD4(value, buf, width, pad)`, draws with
`0x2F198(col, row, buf, mode)` and restores the cursor (`0x2F4FE`).

**`0x2EFD4`** (`0x2EFD4..0x2F0EE`). EAX = value, EDX = dest, EBX = width,
ECX = pad. `0x2EF24` is `sprintf(buf, "%i", value)` (format at `0x80B40`,
libc `0x65546`) into a 0x0C-byte stack buffer, and it returns the length L.
- width ≤ L (`0x2EFEF`): the last `width` characters.
- pad > 3 (`0x2F015` `ja`): nothing but the terminator.
- The jump table at `0x2EFC4`: 0 right-justifies with '0' (`0x2F026`,
  memset `0x61A70`), 1 with ' ' (`0x2F059`), 2 left-justifies with ' '
  (`0x2F08C`), 3 copies the digits alone (`0x2F0C2`).
- The terminator goes at dest[width], or at dest[L] for pad 3 (`0x2F0D8`).
  It returns L.

**`0x2F20C`** (`0x2F20C..0x2F27E`). `0x2F198`'s vertical twin: the same
cursor reload for row -1 and centring for col -1, then `0x2F830` with the
stack byte 1 (`0x2F259`). The cursor gets {row, col + glyph count}
(`0x2F270`/`0x2F274`). `get_xrefs_to` finds two call sites: `0x38E29` in
`0x38D90`, and `0x31DE6` (mode ECX = `0x4000`), which sits in no Ghidra
function. Only `0x38D90`'s is ported.

**`0x2F314`** (`0x2F314..0x2F384`). EAX = col, EDX = row, EBX = string.
The count is `strlen` (`repne scasb`; no `0x2F0F0` and no mode; ECX is
overwritten). It releases each non-empty cell down the column through
`0x2AD40`, and it stops after the cell of a row above `0x1E` (`0x2F36D`
`cmp ecx,0x1e; jg`, tested after the release). `get_xrefs_to` finds two
call sites: `0x38CA1` in `0x38C5C`, and `0x31C45` (its string from
`0x1C500(0x22C)` at `0x31C38`), which sits in no Ghidra function. Only
`0x38C5C`'s is ported.

The glyphs are 8×8 in the `0xBCD7C` font ('2' `0x3F47`, HIT `0x3F30`,
'C' `0x3F58`, 'O' `0x3F64`, 'M' `0x3F62`, 'B' `0x3F57`), so the run fills
rows 8..14 of column 2. That is 7 rows of 8 × `0xD56`/`0x1000` ≈ 6.7 px,
46 px in all: the capture's rows 53–98.

Two raw behaviours show up only in the tests. String `0xE5`'s clear from
col 0 runs 60 cells through `0x2F280`, whose wrap (col `0x2B` is the next
row's col 0, and then that row again from 0) also clears the next row's
cols 0..15. So the row-7 clear takes side 0's hit count at (8, 2) with it
whenever `0x38ED0` names a combo or `0x38D24` fires. And vertical text
that starts at row `0x1E` would make `0x2F830` write the byte before the
string (`m[k - 1]` with k = 0). `0x38D90`'s rows are 8..14, so that is
never reached.

### 33.3 The fix and its assertions

`actors.c` gets `text_vertical_set` (`0x2F20C`),
`text_cells_release_vertical` (`0x2F314`), `text_number_format` (`0x2EFD4`
with `0x2EF24`; `snprintf "%i"`) and `text_number_draw` (`0x2F4D0`), all
exported. `fighter.c` gets `fighter_38c5c`, `fighter_38d24`,
`fighter_38d90`, `fighter_38ed0` and `fighter_38fec`, with local defines for
the five strings and `0xBEB90`. `fighter_39040` now calls `0x38D90` and
`0x38FEC` where the `PORT:` gap was, and `fight_hud_pass` calls `0x38D24` at
`0x357F5` (`0x34038` stays a gap). That is ten functions and about 0x5E0 raw
bytes, inside the size gate. The `0x2F4D0` buffer is zeroed (`PORT:`): the
raw's is uninitialised stack, which only a pad above 3 could show.

The tests:
- **`check_text_vertical_number`** (`test_platform.c`, from `test_text`).
  `0x2EFD4` over every pad, truncation, pad > 3 and width = L. `0x2F4D0`'s
  cells and cursor restore. `0x2F20C`'s column, centring and cursor
  reload. `0x2F314`'s column-only release and its row-`0x1E` stop (it
  releases the dword past the grid's last row and stops before the next).
- **`check_combo_text`** (`test_fight.c`, after `test_fight`'s restores so
  the real actor pool is in place). `0x38D90` in both `+0x63` arms and for
  side 1. `0x38D24`'s countdown, clear and side 1. `0x38ED0`/`0x38FEC`'s
  thresholds, needs, `+0x63`, side-1 indexing, char, and the stop at the
  first record that names. `0x39040`'s two calls and the `0xB4` timer.
- **`check_hud_pass_machine`** seeds `DSW(0x107D18)` = 2/5 and asserts 1/5
  (the `0x357F5` wiring).

Every cleared cell is seeded with a planted non-glyph record (sprite
`0x2C11`), and "kept" is asserted by sprite id. A released record is
reused by the next glyph spawn, so a pointer comparison could not fail.

**Mutations.** `scratchpad/mut23.py` made 55 single-site edits in
`actors.c`, `fighter.c` and `fight.c`. The first sweep killed 49. The six
survivors were test gaps: the pointer-equality checks above, and the
missing width = L pad-4, timer-3, row-12/14/15 and side-1 col-38/40/`0x10`,42
cases and a two-namer case. With those added, all 55 fail 1..16 assertions
(`scratchpad/mut23c.log`).

**Parked minors.**
- The record's §32.3 wording is now "not reached in the current run".
- "backward" is now "reversed-facing" in `fighter.c` and `test_game.c`.
- `fighter.h`'s `0x3BF70` comment names the side's record (ctx[4]).
- `fight.c`'s `0x4AC80` bands are computed in u32 (`0x4ACED..0x4AD27`
  wrap before the signed compares).
- The `0x4B3F0`/`0x4B430` comments name every call site from
  `get_xrefs_to`, with its EBX. EBX = 1: `0x4AC80`'s `0x4AE32`/`0x4AE48`,
  `0x49C78`'s `0x4A14F` (`0x4B430`), and `FUN_0004a708`'s `0x4A75C`
  (`0x4B3F0`, `0x4A751`) and `0x4A76E` (`0x4B430`, `0x4A763`). EBX = 0:
  `0x4AAD0`'s four, `0x4BFA7`/`0x4BFC3`, `0x4C5D3`/`0x4C5E7` and
  `0x4D27A`/`0x4D2A0` (review round 1 added `FUN_0004a708` and the zeroing
  sites).
- The `0x4AC80` header names the `DS_00104B16`-against-`+0x21` choice
  (`0x4AE26`).

The `0x4AAD0` flag-0 test stays parked. `fight_4aad0` is static, and only
a type-0 entry through `fight_effects_pass` reaches it, which is not a
trivial harness.

### 33.4 Measured

| measurement | before (`5bf6e7f`) | after (`bcce10b`) |
|---|---|---|
| captures 1763..1880 | 1763 153 px | **all explained** |
| demo oracle first unexplained | 1763 (raw 4670); `[1763..3616]` 1854 / 1848 unexpl.; port `[1278..1380]` | **1881 (raw 4788)**; `[1881..3616]` 1736 / 1730 unexpl.; port `[1379..1380]` (2, 0 exhibited) |
| demo-fight ratchet | `[1763..1884]` 122, N = 1763 | **`[1881..1884]` 4**, "ratchet improved: first unexplained 1881 > 1763", **N = 1881** |
| front-end oracle | `[560..1762]` / 1203 / 473 clean, 723 splice, 3 transition, 2 unexpl. | **`[560..1880]` / 1321 / 516 clean, 798 splice, 3 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. The three transition frames are the
same (`port832@row177`, `port862@row189`, `port906@row31`). The exhibition
set grows to port frames 0..1378 (1142 exhibited); the dump ends at 1380.
The ladder
`cmake --build build --clean-first && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify`
exited 0 with 0 compiler warnings on the `bcce10b` tree, with N = 1881 in
the Makefile. Unmoved: title `54/55/2/0` and `54/57/0/0`, determinism 54;
smk 120/120 and 41/41; attract 215/216 (expected divergence at 215);
C-vs-Python 9866; `symbols.h`.

**Unresolved code targets after the fix.** The same probe finds only
`0x3E3A8` (f = 962, from `hit_reaction_apply`), the front-end
`0x29B74`/`0x41578` and the stub `0x5D812`, as before. It also prints
`fn_resolve(0)` once a frame at f = 617..648 and from f = 751 on: a null
callback probe that was already there and draws nothing.

### 33.5 The new first unexplained frame, 1881 (characterised, not fixed)

Captures 1763..1880 splice at 0 px. Capture 1881's best splice, port
1378/1379 at row 137, leaves 3 791 px in x 0–204, rows 137–192. Captures
1882..1884 leave 8 429, 10 839 and 11 140 px in x 0–319, rows 67–194, and
1885 is the first all-black frame, so the fight window holds only these 4.
Below the split the capture's gold T-rex drops into a new pose: lower, legs
apart, its tail flat on the sand. Port 1379 (f = 962) keeps it upright, as
in 1880. The raptor, the worshippers, the combo text and the camera match.
f = 962 is the run's one remaining code-target miss, `0x3E3A8`, a reaction
callback that `hit_reaction_apply` skips because it is unregistered. That
is the candidate owner. It is not derived.

(Derived since, §34: the candidate holds. `0x3E3A8` is the T-rex's reaction
`0x2A` callback; registering it explains 1881/1882, and dumping the exit
frame (loop 1970), which the driver had dropped, explains 1883/1884.)

## 34. The reaction callback `0x3E3A8` and the exit frame at capture 1881 (roar-timing Task 24, `8d538d5`)

**Result in one line.** Capture 1881 has one cause, and it is the port's.
`0x34E2C`'s `(char 0, 0x2A)` record `0xA3870` holds `0x3E3A8` as its
callback, and the port had not registered it, so the `0x35045` call skipped
it. At f = 962 the T-rex (side 0) takes reaction `0x2A`, and `0x3E3A8`
restarts its `0xC8950` stream at hold 2.0 with state 9/7/0. Captures 1883
and 1884 also need the exit frame (loop 1970), which the front-end driver
did not dump. With both, captures 1881..1884 are explained. 1885 is the
capture's first all-black frame after the demo, so the capture's demo fight
is explained to its end.

### 34.1 The measurement (temporary, reverted)

A `PR_T24` trace printed each `fn_resolve` miss with `DS_0010150C`, and the
new routine's inputs. It was reverted from pre-trace copies of `mem.c` and
`fighter.c`.
- `0x3E3A8` runs once, at f = 962, from `hit_reaction_apply`, for side 0
  (slot `0x1077B0`, char 0). The slot's `+0x52` is 9 before the call, so
  `0x3C4CC` takes its `0x3C480` arm. The stream is `0xC8950[0]` =
  `0xE6DD2`.
- At f = 963, `0x3531C` case 7 resolves `+0x0C` = `0x3E328` with `+0x57` =
  0 and `+0x86` = `0x10001` (`>> 16` = 1, not above 3). With only `0x3E3A8`
  registered, that was the run's one new miss. The demo's fight logic ends
  there.

With the fix, port frames 0..1378 are byte-identical to Task 23's dump.
Port 1379 (f = 962) is the first that differs. The front-end window grows
to `[560..1882]`: captures 1881/1882 are explained. But that window now
exhibits the dump's last frame, 1380, so `--demo-fight` had no port frame
left and failed ("no port frames after the front-end window").

**The exit frame.** The driver dumped a frame only when the state after the
iteration was >= 3. `0x11BCC`'s timer exit drops `DS_000F0A64` to 0 inside
`game_frame` (`0x24C5C`) in loop 1970, and `0x24C5C` runs before the
`0x25643` present in the same iteration. So loop 1970's presented frame was
never dumped, although its iteration started in state 7.

The port already has a convention for this. The title and attract dump hooks
(`flow.c`, `state_before`, at the `0x25680` copy) credit each presented
frame to the state that started its iteration. The attract's phase-`0xB`
handoff sets state 1 inside `game_frame` in the same way, and its frame is
still an attract frame. The front-end driver now applies that rule with
`state_in`, read before `game_loop()`. It keeps the post-state only for the
entry: loop 589 starts in state 2 and ends in state 3, and it was already
dumped frame 0. So a frame is dumped when its iteration starts or ends in a
state >= 3. The start is unchanged, and the end gains loop 1970, the frame
presented in the iteration that starts in state 7 and exits it. Loop 1971 on
start in state 0 (the attract sub-machine `0x11000`), so they are not in the
state >= 3 window. This says nothing about what the raw presents there.

The measurement (temporary `PR_T24X` and `PR_T24R` gates, reverted):
- Port 1381 (loop 1970) has 63 328 non-black px, and it differs from 1380.
  With it, the dump is 0..1381 (1382 frames), and the front-end window
  becomes `[560..1884]`, 1325 frames: every capture up to 1885.
- For reference only, outside the window: the frame presented at loop 1971
  has 15 600 non-zero pixels, at 1972 none, and from 1973 on 166.
- The select.log hash is not a hash of the presented frame. It is taken on
  `DS_000E87A4` after `game_loop()` returns, and after `swap_buffers`
  (`0x256AA`) that is the next draw buffer. The just-presented buffer is
  `DS_000E87A0`, the one the dump writes. `PR_T24R` shows the difference.
  At loop 1971, `DS_000E87A0` (= the aperture) has 15 600 non-zero bytes
  and `DS_000E87A4` has 0. At 1972 both are 0, and at 1973 they have 166
  and 0. So the log's all-zero hashes for loops 1971..1973 (the FNV-1a of
  64 000 zero bytes is 952198597) do not contradict the 15 600-px frame. An
  earlier draft of this section read them as the presented frames, which
  was wrong. The `test_game.c` comment on the hash is corrected.

A final probe on the fixed tree (dump byte-identical to the one without it)
finds only the front-end `0x29B74`/`0x41578`, the stub `0x5D812` and the
pre-existing `fn_resolve(0)` probes.

### 34.2 The raw (Ghidra `read_memory` + capstone, fixups applied)

Neither `0x3E3A8` nor `0x3E328` is a Ghidra function, and `get_xrefs_to`
finds no code reference to either. The callback dword is at `0xA3870` =
`0xA3528 + 0x2A * 0x14`, and `read_memory` (fixups applied) there gives `a8
e3 03 00 00 00 00 00`: the fixed-up `0x0003E3A8` and no stream. (A scan of
the raw file, before fixups, finds it as the object offset `0x0002E3A8`.) The
sibling `0x3ECF8` is at `0xA3898` (reaction `0x2C`).

**`0x3E3A8`** (`0x3E3A8..0x3E423`, 0x7C bytes).
- `0x3E3AB mov edx,ebx` / `0x3E3AD mov eax,esp`: ctx = `0x33950(side)`.
  EAX and EDX are overwritten.
- `0x3E3B8` pushes `0x40000000` (2.0). The stream is `DSD(0xC8950 + 4 *
  ctx[2]+0x7A)` (`0x3E3C5`, ctx[2] loaded before the push), and EAX =
  `[esp+0x14]` = ctx[4] (`0x3E3CC`, after the push). Then it calls
  `0x3C4CC`.
- ctx[2] gets `+0x53` = 7 (`0x3E3D9`), `+0x52` = 9 (`0x3E3E1`), `+0x54` = 0
  (`0x3E3E9`), `+0x0C` = `0x3E328` (`0x3E3F1`), `+0x18` = `0x3E1D0`
  (`0x3E3FC`), `+0x1C` = `0x3E244` (`0x3E407`), `+0x57` = 0 (`0x3E412`) and
  `+0x41 |= 0x80` (`0x3E41A`).
- It returns AL = 1, which `0x35049` ignores.

**`0x3E328`** (`0x3E328..0x3E3A5`, the `+0x0C` callback, `0x3531C` case 7
at `0x35431`). ctx = `0x33950(EBX)`, and the byte is ctx[2]'s `+0x57`
(`0x3E338`).
- 0 (`0x3E345`): when `(s32)DSD(ctx[2]+0x86) >> 16 > 3` (`sar` at
  `0x3E353`, `jle` at `0x3E359`), `+0x57` = 1 (`0x3E35F`).
- 1 (`0x3E33F jbe`): ctx[2]'s `+0x8A` = 0 (`0x3E36B`), then
  `0x3C4CC(ctx[4], 0xE84B6, 4.0)` (`0x3E372..0x3E380`), then
  `0x3E0F0(ctx[4])`. The returned child gets `+0x53` = 1 (`0x3E38E`: EAX is
  `0x3E0F0`'s return). Then ctx[2]'s `+0x52` = 9 and `+0x57` = 2.
- Above 1: it returns (`0x3E341`).

**`0x3E0F0`** (`0x3E0F0..0x3E15C`). EAX = rec, and side = rec`+0x51`.
- `0x29C08(side, DSB(0x10782A + side*0x94))` (`0x3E10D`/`0x3E116`; EDX is
  zeroed by `xor edx,ebx` with EDX = EBX). The result is stored at
  `DSD(0xC760C + side*4) + 0x10` (`0x3E120`/`0x3E127`).
- `0x2AE14(DSD(0xC760C + side*4), 0, 0, 0, (u16)(rec+0x56 | 0x400))`
  (`0x3E12A..0x3E149`).
- rec`+0x4B` = the child's `+0x56` byte, and the child's `+0x60` = 1. It
  returns the child.
- `0xC760C` holds `0xBB3F8`/`0xBB40C`. Both descriptors' stream is
  `0xE8472`, which holds no `0xD0xx`/`0xD1xx`/`0xD5xx` word.

**`0x29C08`** (0x16 bytes): `DSD(DSD(0xA8A98 + char*4) + DSB(0x105B34 +
side)*4)`. `0xA8A98[c]` = `0xA8A28 + 16c`: four handles per character, one
per variant.

**Not ported.**
- `0x3E1D0` is the `+0x18` hook. `0x19020` (`0x1903F call [ebx+0x1077C8]`,
  EAX = side; its one caller is `0x1958C`) calls it. When `+0x86 >> 16` is
  in 1..3 it calls `0x18C14` (1035 bytes, 7 callees) with the `0xC75F5`/
  `0xC75FF` boxes. `0x19020` is the port's §7.6 named gap. `0x1958C` runs
  before the hit chain in `0x263F4` (`0x2647D`), so in the raw the hook first
  runs at f = 963. The port skips it there. That is not evaluated, and the
  captures to the demo's end are explained.
- `0x3E244` is the `+0x1C` callback, called by `0x193B0` at `0x19505`
  (`call [edx+0x1C]`, EAX = ctx[0]). It needs `0x3C208` (292 bytes, 8
  callees), `0x3C358`, `0x18AF8` and the `0x2C3FC` voice. It is not reached
  in the port's run.

The streams:
- `0xE84B6`'s only code target is `D500 6870 0003`, and `0x36870` is
  registered.
- `0xE6DD2`, the T-rex's `0xC8950` stream, holds `D000 7A58 0003`
  (`0x37A58`, registered) and `D000 640C 0003` (`0x3640C`, unregistered but
  not reached, since it does not miss).

### 34.3 The fix and its assertions

The fix is in `fighter.c`:
- `fighter_3e3a8` and `fighter_3e328`, exported and registered in
  `actors.c`
- the static `fighter_3e0f0` and `fighter_29c08`
- the defines `FIGHT_ANIM_3E328` (`0xE84B6`) and `FIGHT_DESC_3E0F0`
  (`0xC760C`)

`0x3E1D0`/`0x3E244` are stored under a `PORT:` comment. The `0x19020` gap's
`TODO(verify)` now names the f = 963 hook. That is four functions and about
0x1D5 raw bytes, inside the size gate.

The front-end driver (`test_game.c`) records `state_in` before
`game_loop()` and dumps when `state_in >= 3 || state >= 3`. Its dump count
is now 1382, and the old gate's 1381 fails the `CHECK_EQ_INT`.

`title_compare.py` changes:
- `--demo` and `--demo-fight` no longer stop when the front-end window
  exhibits the dump's last frame. The region after it is classified by the
  existing no-match fallback (content-bearing = unexplained).
- `--demo-fight` treats `end == lo` (the front-end window reaches the frame
  before the first all-black capture frame) with no unexplained front-end
  frame outside `FRONTEND_ALLOWED_UNEXPLAINED` as an empty fight window.
  That prints "0 unexplained" and passes for N <= end + 1.

It was shown to fail with N = 1886:
- the pre-fix dump: "first unexplained 1881 < 1886"
- the fix with the 1381-frame dump: fight window `[1883..1884]`, "1883 <
  1886"
- N = 1887: "N is unreachable"

`check_trex_grab` is new in `test_fight.c`, after `check_reaction_attack`.
Its fixture `tg_seed` extends `tb_seed` with sentinels in the slots' `+0x41`,
`+0x52` = 0, `+0x57`, `+0x7A` (chars 1/2), `+0x86` and `+0x8A`, and with
crafted literal-id streams (id `0x1200` + char) in the seven `0xC8950`
entries. The parts:
- **A.** Through `hit_reaction_apply(0, 0x2A)` on the real `0xA3870`.
- **B.** EAX = slot 1, EDX = rec 1, EBX = side 0: every write is side 0's.
- **C.** Side 1.
- **D.** `0x3E328`'s first arm, with `+0x86 >> 16` = 3 and negative kept and
  4 moving, read from ctx[2], not EAX.
- **E.** `+0x57` = 2 and 5 return and spawn nothing.
- **F/F2.** The second arm with pool records. The `0x29C08` values are
  literals from `read_memory 0xA8A58`: char 3 variant 1 = `0x10A50EF0`,
  char 2 variant 3 = `0x20A13DE0`. Also the child's `+0x53`/`+0x60`/
  `+0x4B`/`a5`, and the other side's descriptor kept.
- **G.** Through `fighter_state_3531c` case 7.

Both registrations are asserted, and every global the test touches is
restored.

**Mutations.** `scratchpad/mut24.py` made 36 single-site edits in
`fighter.c` and `actors.c`, and all 36 fail 1..19 assertions
(`mut24.log`). The sources were restored and checked with `cmp`.

### 34.4 Measured

| measurement | before (`9abbdd0`) | after (`8d538d5`) |
|---|---|---|
| captures 1881..1884 | 1881 3 791 px; 1882..1884 8 429..11 140 px | **all explained** (1 clean, 3 splice) |
| front-end dump | 1381 frames (0..1380) | **1382 (0..1381)**, the exit frame added |
| demo oracle first unexplained | 1881 (raw 4788); `[1881..3616]` 1736 / 1730 unexpl.; port `[1379..1380]` | **1886 (raw 4795)**; `[1885..3616]` 1732 / 1726 unexpl.; no port frame left |
| demo-fight ratchet | `[1881..1884]` 4, N = 1881 | **fight window empty** (front-end reaches 1884; 1885 all-black), "0 unexplained", **N = 1886** (= end + 1) |
| front-end oracle | `[560..1880]` / 1321 / 516 clean, 798 splice, 3 transition, 2 unexpl. | **`[560..1884]` / 1325 / 517 clean, 801 splice, 3 transition, 2 unexpl. (832, 833)** |

Only the front-end window and N moved. The three transition frames are the
same (`port832@row177`, `port862@row189`, `port906@row31`). The exhibition
set is the whole dump, port frames 0..1381 (1145 exhibited). The ladder
`cmake --build build && PR_ORACLE_REQUIRED=1 ./build/run_tests && make verify`
exited 0 with 0 compiler warnings, with N = 1886 in the Makefile. Unmoved:
title `54/55/2/0` and `54/57/0/0`, determinism 54; smk 120/120 and 41/41;
attract 215/216 (expected divergence at 215); C-vs-Python 9866;
`symbols.h`.

**The end of the demo-fight capture.** The capture holds no fight frame
after 1884, so N can rise no further with this capture. What the ratchet
still proves is narrow: every content-bearing capture frame from the
front-end window's start up to the all-black 1885 is explained. A shrink of
the front-end window reopens the fight window and fails below N, and the
front-end oracle alone cannot see that, because its window is derived from
the port's own dump. It does not prove that the fight is reproduced beyond
the capture, or that the port's last-frame state matches the raw's (the
`0x19020` hook at f = 963 is not evaluated).

### 34.5 The new first unexplained frame, 1886 (characterised, not fixed)

Capture 1885 is all-black, and 1886 is the first content-bearing frame
after the demo. From 1886 on, the capture shows the next attract cycle's
logo: a shaded red sphere on white. By 1930 it has shrunk, with the
"...ME WARNER" lettering sweeping in, i.e. the Time Warner Interactive logo
movie. The port's dump ends with the demo at loop 1970. The state drops to
0 (`0x11BCC` restores `DS_000F0A64` = `DS_000F0A6C` = 0, the attract
sub-machine `0x11000`), and the driver stops dumping, so no port frame is
compared there. This is a dump-window boundary, not a derived port defect.
Whether the port reproduces the attract's second cycle is not measured.

## 35. The slot `+0x18` hook `0x19020` and its targets `0x3E484`/`0x3E1D0` (roar-timing Task 25, `ec8e132`)

**Result in one line.** After §34 no oracle-visible unexplained frame has a
raw-code owner, and every named gap but one is unreached in the demo run.
The one that runs is `0x1958C`'s `0x19020` hook (§7.6, §19.6): 37 frames
call it. It is now ported with both hooks the run stores and the check
walk `0x18C14` they share, 7 functions. It moves no frame: the dump is
byte-identical, as the evaluation predicted.

### 35.1 The survey (baseline `a9a2e07`)

`make verify` (EXIT 0) and `make demo-oracle`:
- title: `[216..326]`, 54/55/2/0 and 54/57/0/0; determinism 54; smk
  120/120, 41/41; C-vs-Python 9866.
- attract: 215/216 explained on both captures; capture 215 (raw
  2180/2175) differs from the best splice by 498 B from row 192.
- front-end: `[560..1884]`/1325: 517 clean, 801 splice, 3 transition,
  2 unexplained allowed by name (832, 833); 16 all-black captures excluded.
- demo-fight: fight window empty, 0 unexplained, N = 1886.
- demo (report-only): `[1885..3616]`, 1726 unexplained, first 1886, the
  next attract cycle, which the dump does not cover (§34.5).

Attract 215 and front-end 832 are both the lazy loader's 498-byte
`- LOADING -` frame (rows 192..197), and 833 is the arena mid-fade under the
same text. The frame is in the port's buffer. It is not presented because
the `0x25643` tick gate passes in the same iteration. Holding it needs the
read stall's post-read PIT ticks (`0x1BDF4`), a host property that the
game_flow "Captures 831/832" section proves cannot be derived. So none of
the three has a raw-code owner.

A temporary `PR_T25` probe (reverted; dump byte-identical to the baseline)
printed every `fn_resolve` miss with `DS_0010150C`, and every frame on which
a slot's `+0x18` is set at `fighter_pass_a`'s `0x195B6` gap.
- Misses: `0x29B74` and `0x41578` once each, the stub `0x5D812` (36 164),
  and the null probes (49). `0x3E244`, `0x3640C`, `0x235C4`, `0x370F0`,
  the grab arms and tails, `0x3C048` and the worshipper types 2/7/9..12 are
  not reached.
- The hook is set on 37 frames, all for side 0:
  - `0x3E484` at f = 106..114, 219..227, 332..340 and 760..768 (four
    reaction-`0x2B` leaps)
  - `0x3E1D0` at f = 963

A scan of the code object for `mov dword [reg+0x18], imm32` with a code
address (fixups applied) finds the fighter hook stores `0x3D484`,
`0x3D858`, `0x3DD14`, `0x3E1D0`, `0x3E484`, `0x3E924`, `0x3ED78`,
`0x3EFE0`, `0x3F1F0` (at `0x3F439` and `0x40C68`), `0x3F4B8`, `0x3F7F4` and
`0x3FD30`. Only `0x3E1D0` and `0x3E484` are stored in this run.

### 35.2 The raw (Ghidra `read_memory` + capstone, fixups applied)

**`0x19020`** (70 B, §19.6.1). When `DSD(0x1077C8 + side*0x94)` is set, it
calls it with EAX = side (`0x1903F`). A zero result stores 1 in
`DS_00100AF8[side]` (`0x1904C`), and any other result stores 0 (`0x1905C`).

**`0x3E484`** (63 B, §19.6.1). ctx = `0x33950(side)`, 16 flags =
`0x18BD4`, flag 1 = 0, 8 = 0 and 0 = 1 (`0x3E4A4`/`0x3E4A8`/`0x3E4B0`).
Then `0x18C14(ctx[0], flags, EBX = 0, ECX = 0)`, whose result it returns.

**`0x3E1D0`** (`0x3E1D0..0x3E242`, 116 B). ctx = `0x33950(side)`, flags =
`0x18BD4`. Then flags 1, 8, 4 and 0xE = 0 (`0x3E1EC`..`0x3E1F8`), 5 = 1
(`0x3E1FC`, `mov ah,1`), and 7 and 0xD = 0 (`0x3E204`/`0x3E208`). With
`t = (s32)DSD(ctx[2]+0x86) >> 16` (`sar`, `0x3E212`), `t > 3` (`jg`) or
`t < 1` returns 1 (`0x3E21F`). Otherwise it returns `0x18C14(ctx[0], flags,
EBX = 0xC75F5, ECX = 0xC75FF)` (`0x3E226..0x3E237`).

**`0x18BD4`** (64 B) stores 2 in all 16 flag bytes.

**`0x18C14`** (`0x18C14..0x1901E`, 1035 B). ESI = flags, EDI = EBX
(box a), EBX = ECX (box b).
- A zero box selects the defaults `0xA1818`/`0xA1822` (`0x18C38`/
  `0x18C41`).
- The local `[esp+0x18]` is set to 1 only for side != `0x29A` (`0x18C26`),
  and `0x19005` returns 0 only when it is set.
- ctx = `0x33950(side)`.

For each flag, 2 skips the check (`cmp al,1; jne`). The other values run
it. The checks, in the raw's order:

| flag | check | returns 1 when |
|---|---|---|
| 0 | `(s32)AF8[side] <= 0` (`setle`, `0x18C51`) | 0 and false: rewrite 4 (`0x18C7C`); 1 and true: rewrite 3 (`0x18C6B`) |
| 1 | ctx[3] words `+0x74`/`+0x76` | 0: `+0x74 != 0` or `+0x76 > 1`; 1: `+0x74 < 1` or `+0x76 < 2` |
| 0xF | ctx[2] `+0x43` bit 2 | 0: set; 1: clear |
| 2 / 3 / 6 | ctx[3] `+0x54` == 0 / 1 / 7 | 0: equal; 1: not equal |
| 5 | `0x1DDF4(ctx[1], box a, box b)` (EAX, EDX = EDI, EBX) | 0: non-zero; 1: zero |
| 7 | ctx[3] `+0x62` | 0: non-zero; 1: zero. Either way ctx[2] `+0x8A` = 0 and `0x18B44(ctx[2])` |
| 8 | ctx[3] `+0x42` bit 3 | 0: set; 1: clear |
| 9 | `0x189FC(ctx[1])` | 0: non-zero; 1: zero |
| 0xA | `0x18A4C(ctx[0])` | 0: non-zero; 1: zero |
| 0xB | ctx[4] `+0x61` | 0: non-zero; 1: zero |
| 0xD | `0x39EFC(ctx[1])` | 0: non-zero; 1: zero. Either way `+0x8A` = 0 and `0x18B44(ctx[2])` |
| 0xC | ctx[3] `+0x53 == 0x0A` | 0: equal; 1: not equal |
| 0xE | `r = 0x3B298(ctx[1], ctx[2] +0x5F)`, called for every value but 2 | 0: `r != 0` (`+0x8A` = 0); 1: always (`+0x8A` = r when r = 0, else 0) |
| 4 | ctx[3] `+0x54 == 2` | 0: equal; 1: not equal |

At the end it returns 0 when `[esp+0x18]` is set (`0x1900C`), else 1.

**`0x189FC`** (78 B). ctx = `0x33A10(side)`. When `0x1A570(ctx[0])` holds,
it returns `(s32)ctx[3]+0x2C < (s32)ctx[2]+0x2C` (`jge`, `0x18A23`).
Otherwise it returns `>` (`jle`, `0x18A3A`).

**`0x18A4C`** (98 B). ctx = `0x33950(side)`, and `c0`/`c1` are the two
sides' `0x1A570`.
- With `c0`: it returns 1 when `0x189FC(ctx[1])` holds and `!c1`.
- Without `c0`: it returns 1 when `0x189FC(ctx[1])` holds and `c1`.

This is symmetric in the side, as the mutations confirmed (§35.3).

The box tables (`read_memory`): `0xC75F5` holds `6e` for chars 0..6,
`0xC75FF` holds `63`, and `0xA1818` and `0xA1822` hold `e7`. Every other
callee is already ported: `0x33950`, `0x33A10`, `0x1A570`, `0x1DDF4`
(`hit_geometry`), `0x39EFC`, `0x3B298` (`fighter_command_dispatch`) and
`0x18B44`.

**Evaluated on the demo's state (the probe).**
- `0x3E484` returns 1 whenever `AF8[0]` = 0, and the port's `AF8[0]` stays
  0.
- It returns 0 on each leap's last frame (f = 114, 227, 340 and 768). There
  `AF8[0]` is 6, 14, 3 and 24, and the other slot's `+0x74`, `+0x76` and
  `+0x42` are all 0. The raw's `AF8[0]` becomes 1 where the port kept the
  old value. That is the same zero-ness and sign.
- `0x3E1D0` at f = 963 sees `+0x86` = `0x00000001` (`>> 16` = 0: the
  `0x3531C` case-7 read later in the frame sees `0x10001`, §34.1). So it
  returns 1 without `0x18C14`, and `AF8[0]` stays 0, which it already was.

`get_xrefs_to 0x100AF8` gives the readers `0x19632` and `0x19720`
(`!= 0`) and `0x18C49` (`<= 0`). Besides `0x19020` and `0x1958C`'s own
writers (`0x195C6`, `0x19618`, `0x196B1`, `0x196DE`, `0x19705`, `0x19719`),
the other writers `0x175EA`, `0x1756F`, `0x36928` and `0x38625` store 0 or
`DS_00100B54`. No reader tells 1 from
6. §19.6.2's measurement (f = 106..114) now covers every hook frame of the
run.

### 35.3 The fix and its assertions

The fix is in `fighter.c`:
- the static `fighter_189fc`, `fighter_18a4c` and `fighter_18bd4`
- the exported `fighter_18c14` (all 16 flags, not the demo's reduction),
  `fighter_19020`, `fighter_3e484` and `fighter_3e1d0`
- the `fighter_hook_cb` register shape (EAX = side, EAX returned)
- the defines `FIGHTER_A1818`/`A1822`/`C75F5`/`C75FF`

`fighter_pass_a` calls `fighter_19020(side)` at `0x195B6`, which replaces
the §7.6 `PORT:` gap and its f = 963 `TODO(verify)`. `actors.c` registers
`0x3E484` and `0x3E1D0`. That is 7 functions, about 1.5 KB of raw code,
inside the gate.

Two deviations are marked `PORT:`:
- An unregistered hook is skipped and leaves `AF8[side]` alone. That is
  the port's behaviour before, and no unregistered hook is stored in this
  run.
- For side `0x29A`, the raw reads an uninitialised stack byte at
  `0x19005`. The port returns 1, and no caller the port reaches passes that
  side.

`0x3E244` stays stored and unported (not reached). The `test_game.c`
comments that said `0x19020` is unported and the pose is unreachable are
corrected. The driver's `pose10 1, pose0a 1` were already 1 before this fix:
the pose became reachable when `0x193B0`'s chain was ported, and the dump is
byte-identical with `0x19020`.

`check_slot_hook` (in `test_fight.c`, after `check_trex_grab`) seeds with
`sh_seed` and `sh_walk`. It checks:
- every flag in both values, and in each "self vs other" read. For flag 0xE
  the fix round (review round 1) added M2: `0x3B298` returns 1 through a
  seeded command word (side 1's `0x1000` against `0x1AB5C`'s base `0x3000`,
  as `check_think_chain`'s A2), so flag 0xE = 0 fires and the `0x18FC9`
  store is asserted. The `0x18FB0` store (flag 1 with r = 0) writes r, so
  its value is 0 either way.
- the rewrites of flag 0
- 0x18B44 (through `DS_00100C1D` 0 -> 1) and `+0x8A`
- 0xE's call, seen through `0x3B2D6`'s copy, with value 3
- the orders 0xD < 0xC, 0xE < 4, 7 < 8 and 5 < 7
- 0x3E484 in the demo's f = 106/114 states and for side 1
- 0x3E1D0's `+0x86` gate (0, 1, 3, 4, -1 and 2.FFFF), its box-table
  boundaries (7040/7041 on `0xC75F5`, 6336/6337 on `0xC75FF`), and its
  flags 1/8/0xD/0xE/4 past flag 7
- 0x19020 (unset, unregistered `0x3D484`, both results, side 1)
- `fighter_pass_a`'s `0x195B6`
- the two registrations and the four box-table bytes

**Mutations.** `scratchpad/t25/mut25.py` made 59 single-site edits:
- In round 1, 50 failed and 9 survived.
- The assertions added for 7 of them (default box b, `+0x76` = 1 under flag
  1 = 1, the side-1 hook with slot 0 cleared, 0x3E1D0's flags past 7) make
  all 7 fail in round 2.
- The 2 survivors are equivalent mutants:
  - `0x18A4C(ctx[1])`, because `0x18A4C` is side-symmetric
  - dropping `0x19020`'s zero test, because `fn_resolve(0)` is NULL

Sources restored and checked with `cmp`.

**Review round 1.** Flag 0xE's `r != 0` arm (`0x18FC1..0x18FD0`) was
unexercised: `0x3B298` was held at 0, so flag 0xE = 0 never fired. M2 adds
it, and 5 new mutations of that arm (dropping the `0x18FC9` store or its
return, narrowing it to flag 0 or flag 1, widening it to flag 3) all fail.
The full re-run of all 64 fails 62; the 2 survivors are the equivalent
mutants above.

**What the test restores.** It restores the globals it seeds itself:
`DS_00100AF8`/`AFC`, `DS_001077A8`, `DS_00100AB0`/`AB4`, `DS_00100C1D`,
`DS_001078FA`, `DS_001088E0`/`E2`, the ring `DS_00108270`, the `0xA6728`
word, `DS_001014EC`, `0x1080AC`/`AE` and `DS_00104B00`. It leaves what
`tf_hit_fixture`/`tb_seed` write (the slot pair, the fixture records and
actors, `DS_00100B5A`/`B5E`, `DS_00107EE4`, `DS_00107ED8`/`EDC`), as
`check_trex_grab` does. It also leaves the two dust actors that part G's
`0x18B44` spawns.

### 35.4 Measured

| measurement | before (`a9a2e07`) | after (`ec8e132`) |
|---|---|---|
| front-end dump | 1382 frames | **byte-identical** (all frames, `select.log`) |
| `0x19020` hook calls | 0 (gap) | 37: `0x3E484` x36 (32 `AF8` 0 -> 0; f = 114/227/340/768 6/14/3/24 -> 1), `0x3E1D0` x1 (0 -> 0) |
| every oracle | as §35.1 | unchanged |

Nothing moved, as predicted. The demo-fight ratchet stays exhausted at N =
1886. The remaining unported `+0x18` hooks (`0x3D484` .. `0x3FD30`) and the
`+0x1C` callback `0x3E244` are not stored or not reached in this run. When a later run
reaches one, `0x19020` resolves it and skips it with `AF8[side]` unchanged,
as before.

## 36. The attract's second cycle after the demo: `0x52106`/`0x1C740`, `0x4F228`, the lightning stream (roar-timing Task 26, `fc8e775`)

**Result in one line.** The front-end capture runs on past the demo into the
attract's second cycle: the two logos, the attract, the title card with
lightning, and a second demo from 2385. The port now dumps that cycle into a
separate `cycle2/` dump, and with four faithful fixes captures 1886..2383 are
explained. The new first unexplained frame is 2384, the `- LOADING -` frame
before the second demo. A new ratchet pins N = 2384.

### 36.1 Why the dump stopped at loop 1970

The front-end driver dumped a frame when its iteration started or ended in a
state >= 3 (§34.1). 0x11BCC drops the state to 0 inside loop 1970, so loops
1971 on fall outside that rule. That rule was the front-end/demo window, not
an alignment trick. The capture holds 3617 frames (0..3616), and 1886..3616
follow the demo.

Dumping those frames into the same dump is not legitimate. A probe (`PR_T26`,
reverted) dumped every frame after the state-3 entry for 3700 loops (3111
frames). The front-end window then grew to `[1..2231]`, because the attract's
second-cycle frames content-match the capture's own first attract (capture
1..~560). So the extension writes the frames presented after the exit frame
to `<dump>/cycle2/` and classifies them separately. It keeps the same loop
convention: one frame per iteration, credited to the state that started it.

Inside loop 1971 the attract's phase 0 plays both logos through `0x1C740`,
which writes the screen itself (VBlank-gated blits, not the `0x25643`
present). A per-iteration dump cannot see those screens, so the player gets
a `PORT:` dump seam (`movie_set_screen_hook`). It runs after each screen the
player writes, and the driver appends each one to `cycle2/` in order.

### 36.2 The measurement (probes reverted)

- **The phase byte.** 0x11BCC (`read_memory 0x11BCC`: `xor ah,ah; mov
  [0x104B1B],ah; mov [0x104B15],ah; mov ax,[0xF0A6C]; mov [0xF0A64],ax; call
  0x29D60; mov eax,0x100; jmp 0x2C3FC`) leaves `DS_000F0A6F` at 0. So loop
  1971 runs `0x11000` phase 0, which plays both movies.
- **The capture's logo region** (md5 against `data/smk-captures`): 1885 is
  black. 1886 splices black (row 0) into TWI5 frame 0 from byte 960. 1887..2094
  are TWI5 frames 0..119, clean or spliced, about 2 capture frames per movie
  frame. 2094 is `twi5[119][0..b) ++ twi5[120][b..)` with b in
  91687..150111. 2095 is black. 2096..2132 are TWG frames 0..34; the capture
  skips 13..15, and frames 35..40 are holds equal to 28. 2133 is black. 2134 is
  the credits on black, the collapsed phase-`0xC` countdown. 2135 on is the
  jungle fade-in, the same as capture 2.
- **The first divergence after the logos.** With the logo frames dumped, the
  port's loop 2155 (the first post-countdown frame) drew the wall texture
  full screen and no jungle, where capture 2135 shows the dim jungle. A
  data-object diff at attract phase 3 (the boot attract against the second
  cycle; `PR_T26DS`, reverted) found the projection gate `DS_00107A54` = 1 in
  the second cycle. The demo's state 6 sets it (`0x20E7F` -> `0x38730`,
  `0x387E2`), and the port never cleared it.
- **The cycle counter.** The port's second cycle took phase 9's `0xF0` arm and
  handed off to the title. `0x10E80` stores `DS_000F0A5C` = 4, and the boot
  attract's phase 2 wraps it to 0 (`0x11089` inc, `0x110A1` jl, `0x110A5`
  store). The driver skips the boot attract, so it kept 4, and cycle 2 wrapped
  it to 0. In the raw, cycle 2 counts 1: phase 9 takes its `0x78` arm, phase
  `0xA` spawns the lightning actor `0x9AD30` (`0x11478`), and phase `0xB`
  hands off to state 6 with `DS_000F0A72` = 5 (`0x1150E`). The capture shows
  that second demo from 2385.
- **The lightning.** With the gate and the counter fixed, 2134..2303 are
  explained. 2304..2309 and 2315..2322 are white flashes on the title card.
  `0x9AD30`'s stream is `0xE890A` (its first dword). It holds
  `D100 F83C 0004 0000` (opcode `0x11`, target `0x4F83C`; the dwords are at
  `0xE8916`, `0xE893C`, `0xE8958` and `0xE896E`) and `D100 0FC4 0001 0000`
  (dword at `0xE892E`, target `0x10FC4`). Neither target was registered, so
  `anim_indirect` skipped both. Registering `0x4F83C` explained 2304..2309, and
  `0x10FC4` explained 2315..2322 as well.
- **What is left.** 2383 is black. 2384 is the `- LOADING -` frame before the
  second demo, the loader's read-stall class. Like attract 215 and front-end
  832 it has no raw-code owner (§35.1). 2385 is the second demo's first frame,
  which the port matches (loop 2783, the state-6 -> 7 iteration; same stage
  and characters). From 2386 on the second demo diverges. Its state-7 run
  misses `0x1490C`, `0x15350`, `0x3640C` and `0x3C32C`.

In every probe the dump's frames 0..1381 stayed byte-identical.

### 36.3 The raw (Ghidra decompile/disassembly, `read_memory`, fixups applied)

**`0x1C740`** (the logo player). It calls `0x52106(0)` at `0x1C74D`. Then
`0x62756` and `DS_000A81A8` gate the open (`0x6345C`) and the setup
(`0x649B0`, into `DAT_000E87A4`). The loop runs uVar7 = 1..`piVar3[3]`:
- VBlank, then the palette (`0x65340`) when `piVar3[0x1A]` is set
- decode (`0x64130`) and the dirty-rectangle blits (`0x64ED8`/`0x50D23`)
- `0x643CC` only when uVar7 is not the last frame
- the wait loop (`0x62756`/`0x50161`/`0x65240`); a key leaves the loop

The last frame is decoded and blitted as well. After the close (`0x63CE8`) it
calls `0x52106(0)` again at `0x1C873`, inside the opened arm.

**`0x52106`**:
- `0x52108`/`0x5210D`: both tick counters = EAX.
- `0x52112`..`0x52123`: EDX = EAX, and `0x51F72` fills `DS_001014E8` and
  `DS_001014E4` with the dword EDX (200 rows of 0x140 bytes = 0xFA00).
- `0x52133`..`0x52149`: 256 zero DAC entries after a VBlank spin.
- `0x5214C`: `0x51F72` fills 0xA0000 with the same dword.

The ported callers pass 0: `0x2BAF4` at `0x2BBE8`/`0x2BBEA`, and `0x1C740`'s
two. (Corrected in Task 27, §37.1: an earlier text said every caller passes 0.
`0x52106` has 7 callers. The unported sites `0x32E93` and `0x332E4` pass
`0x2EDE0`'s return after a bit test has found it non-zero; `0x330B1` passes it
either with bit 25 clear, possibly 0, or with bit 24 set (§37.1). `0x32BF5`
passes its function's EAX.)

**`0x4F228`**:
- `xor ah,ah; xor edx,edx; xor ebx,ebx`
- `mov [0x107A54],ah`, `mov [0x107A55],al`, `mov word [0x107A3A],dx`,
  `mov word [0x107A38],bx`
- EBX and EDX are pushed and restored.

Its callers are `0x2BAF4` at `0x2BBC4` (after `xor eax,eax` at `0x2BBC0`),
`0x20C10` at `0x20C49`, and `0x43818` at `0x43822`. `actors_reset` had
skipped it under a `PORT:` note that called it input state. These are the
render projection's gate and words, which `render.c` owns.

**`0x4F83C`** (ported as `attract_palette_start`; fidelity-gaps record §4.7).
It reads neither EAX nor EDX before it overwrites them: `0x4F83E` loads DL,
`0x4F847` zeroes AH, and `0x4F867` loads EAX. **`0x10FC4`** is
`mov dword [eax+0x18],0; ret`, with EAX = rec.

### 36.4 The fix and its assertions

- `gfx.c`: `gfx_screen_reset` (`0x52106`). `actors_reset` calls it in place
  of its inline copy (the same writes).
- `render.c`: `render_projection_reset` (`0x4F228`). `actors_reset` calls it
  at `0x2BBC4` with 0.
- `actors.c`: the wrappers `anim_code_4F83C` and `anim_code_10FC4`, both
  registered.
- `movie.c`: `0x52106` at `0x1C74D` and `0x1C873`. Every decoded frame is
  presented, which replaces the "final frame only when a hold" rule and its
  `TODO(verify)`: TWI5 presents 121, TWG 41. Also the `PORT:` seam
  `movie_set_screen_hook`.
- The driver (`test_game.c`):
  - `FRONTEND_ATTRACT_CYCLE_AFTER_BOOT` = 0 is the boot attract's post-state,
    like the RNG and frame-counter seeds.
  - The loop runs to `FE_LOOPS` = 2800. Every earlier measurement and
    end-of-run read keeps the `FE_DEMO_LOOPS` = 2000 window. The picks, the
    variant and the handle are read at loop 1999, where the run used to end,
    because the second demo draws new picks.
  - The cycle-2 dump: 829 loop frames (1971..2799) plus 166 player screens
    (2 x (blank + frames + blank)) = 995.

That is 5 functions (4 ported, 1 seam), about 130 lines, inside the gate.

The assertions:
- `test_gfx`: `0x52106`'s dword fill, counters, DAC and aperture, with
  sentinels past each fill, for EAX = 0x12345678 and 0.
- `test_render` `check_projection_reset`: the four targets and the neighbours
  `0x107A53`/`56`/`36`/`3C`.
- `test_actors`: the four targets through `actors_reset`.
- `test_attract` E2: both registrations, called as `anim_indirect` calls them.
- `test_movie`: 121/41 presented and 123/43 hook calls. The first screen is
  black, the second lit and the last black. The counters, the DAC and the
  aperture are zero after the play. A missing movie gets 1 call (the entry
  blank), and invalid arguments get 0.
- The driver: cycle 2 from loop 1971; 995 frames; `DS_00107A54` = 0 after
  loop 1971; `DS_00104AD0` bit 0 first at loop 2547; state 6 at loop 2782
  with `DS_000F0A72` = 5.

**Mutations.** `scratchpad/t26/mut/mut26.py` made 24 single-site edits in
`gfx.c`, `render.c`, `actors.c`, `movie.c` and the driver seed: 20 at unit
level and 4 through the driver. All 24 fail 1..8 assertions. The sources were
restored and checked with `cmp`.

### 36.5 Measured

| measurement | before (`6d2c4a3`) | after (`fc8e775`) |
|---|---|---|
| front-end dump | 1382 frames | **byte-identical** (0..1381) |
| front-end oracle | `[560..1884]`/1325: 517/801/3/2 (832, 833) | unchanged |
| demo-fight ratchet | fight window empty, N = 1886 | unchanged |
| capture after the demo | not compared (no port frame) | **`--attract2`**: region `[1885..3616]`, 1732 frames: 355 clean, 138 splice, 3 transition, 1230 unexplained, 6 all-black |
| captures 1885..2383 | — | **499: 354 clean, 138 splice, 3 transition, 4 all-black, 0 unexplained** |
| attract2 first unexplained | — | **2384 (raw 6759)**, N = 2384 |

The three transition frames are 1949 (cycle-2 frames 35/36, row 6), 2230
(501/502, row 155) and 2288 (625/626, row 101). 2385 is clean (cycle-2 frame
978).

**What the new ratchet proves.** Every content-bearing capture frame from
1885 to 2383 is explained by the cycle-2 dump. That covers both logos, the
attract's second cycle and its title card with the lightning. The cycle-2
window is derived from the port's own dump by content alignment, the same as
the front-end oracle's, so it cannot see a port that under-renders.

**The staged next step.** The second demo (DS_000F0A72 = 5; from 2386) is the
next owner. Its state 7 reaches `0x1490C`, `0x15350`, `0x3640C` and
`0x3C32C`, none of them registered. 2384 is a loader frame with no raw-code
owner, so N can move past it only when a frame after it is explained and
2384 is allowed by name with this reason, as 832 is.

## 37. The second demo: capture 2384 allowed by name, character 3's reaction `0x1490C` (roar-timing Task 27, `c0edb4c`, `a7ccc86`)

**Result in one line.** Capture 2384 is byte-identical to front-end capture
832, so it is allowed by name the same way, and the attract2 ratchet moves
to N = 2386 (`c0edb4c`). The second demo's first state-7 frame reaches
character 3's reaction callback `0x1490C`, which was not registered. It is
now ported with its closure (`a7ccc86`). The raptor's later poses follow the
capture, but 2386 stays the first unexplained frame, because both fighters
differ on it. N stays 2386.

### 37.1 The parked corrections (`7f42495`)

- **`0x52106`'s callers.** `get_xrefs_to 0x52106` lists 7 calls:
  `0x2BBEA`, `0x1C74D`, `0x1C873`, `0x32BF5`, `0x32E93`, `0x330B1` and
  `0x332E4`. The ported three pass 0. The others need not:
  - `0x32E93` and `0x332E4` follow `call 0x2EDE0; test eax,0x1000000; je`
    (`0x32E83..0x32E8D`, `0x332CC..0x332D6`), so EAX has bit 24 set.
  - `0x330B1` has two paths. `0x33092 test eax,0x2000000; 0x33097 je 0x330AD`
    reaches it with bit 25 clear, so EAX can be 0. The fall-through path
    calls `0x2EDE0` again and reaches it only after `0x330A2 test
    eax,0x1000000; je 0x33204` has found bit 24 set.
  - `0x32BF5` is a loop head (`0x32F43 jl`). Its first pass carries the
    function's incoming EAX (`0x32BE5 mov [esp+0x2c],eax`, then no write to
    EAX before `0x32F43`).
  `gfx.c`, `test_platform.c` and §36.3 now say this.
- **`game_init`.** `0x20C10` makes `0x20C47 xor eax,eax; 0x20C49 call
  0x4F228`. `game_init` now calls `render_projection_reset(0)`. The four
  targets are already 0 there: the front-end `select.log` is byte-identical,
  and the title oracle is unchanged.
- **Stale claims.** In `test_video.c`, TWI5's last frame is presented
  (§36.2, capture 2094); the smacker capture just does not hold it. In
  `game_flow.md`, the driver loops 2800 with a 2000-loop measurement window.
- **The E2 gate.** Outside the isolated `PR_ATTRACT_DUMP` run, `test_attract`
  now requires the pool, so E2 cannot skip silently.
- **The Makefile.** The demo-fight provenance comment is rewrapped. The words
  are unchanged.

### 37.2 Capture 2384 (`c0edb4c`)

`frame_2384.raw` and `frame_0832.raw` in `data/title-captures/frontend` are
byte-identical (0 differing bytes). 832 is the front-end oracle's allowed
LOADING frame (the loader's read-stall class, no raw-code owner, §35.1). So
`title_compare --attract2` allows 2384 by index through
`ATTRACT2_ALLOWED_UNEXPLAINED = {2384: 832}`. The allowance holds only while
the two captures stay byte-identical. Otherwise the tool fails. The tool
also fails now when N is at or below the region's start, because such an N
guards no frame (tested with N = 1885).

On `7f42495`'s dump, which is byte-identical to `01a7f70`'s, the region is
`[1885..3616]`: 355 clean, 138 splice, 3 transition, 1230 unexplained,
6 all-black. 2384 is allowed, and the first unexplained frame is 2386
(raw 6794). N = 2386.

### 37.3 The measurement (probes reverted)

- **The miss log.** A temporary `fn_resolve` hook logged every miss with the
  driver's loop index. In 2800 loops the only miss other than the
  type-table stub `0x5D812` is `0x1490C`, once, at loop 2784. That loop is
  the second demo's first state-7 iteration; loop 2783 is the state-6 -> 7
  frame, cycle-2 frame 978, which is capture 2385. `0x15350`, `0x3640C` and
  `0x3C32C` (named in §36.5) are not reached before loop 2800.
- **Where the pointers live.** A read of the data object (Ghidra
  `read_memory`, fixups applied) finds the dword `0x0001490C` once, at
  `0xA4734`, which reads `0c 49 01 00 00 00 00 00`. That is a callback with
  no stream. `0xA4734 - 0xA3528` = `0x120C` = 231 records of `0x14` =
  3 * 64 + `0x27`, so it is character 3's reaction `0x27` in `0x34E2C`'s
  table. `0x15350` is at `0xA470C` (character 3, `0x25`). `0x3640C` has 7
  stream sites and `0x3C32C` has 9.
- **The fighters.** Side 0 is character 3 (the green raptor). Side 1 is
  character 1 (the white ape).

### 37.4 The raw (Ghidra `read_memory` + capstone, fixups applied)

- **`0x1490C`** (EAX = slot, EDX = rec, EBX pushed and not read):
  - `0x339AC(rec)` builds the context.
  - `0x14814(slot, rec)`, with EBX = rec (`0x14913`); `0x14814` pushes EBX
    and overwrites it at `0x14819` before any read.
  - `0x1492A mov [eax+0xFD11C],dl` with DL = 1 sets FD11C[side].
  - It returns AL = 1.
- **`0x14814`**:
  - `0x339AC(rec)`, then the voice `0x2C3FC(0xB0)` (out of scope, spec §7).
  - With `0x396AC(ctx[0], 0)` true:
    - `0x18B44(ctx[2])`. It pushes and pops EDX, so 0xD2E86 survives.
    - `0x3C4CC(rec, 0xD2E86, [0x9AFE4])`. The hold is the dword at
      `0x9AFE4` = `0x40200000`.
    - The slot state is 9/4/0.
  - Otherwise:
    - The stream is `0xD2EDC` when `0x14590(ctx[0])` holds, with ctx[3]
      +0x68 decremented (`0x1487F`). Otherwise it is `0xD2E9A`.
    - It starts through `0x3C4CC` at hold 1.0.
    - The slot's +0x57 = 0 and its state is 9/7/0.
    - The slot's +0x0C/+0x18/+0x1C = `0x1461C`/`0x145CC`/`0x145E4`.
    - The slot's +0x40 dword `or 0x48000`.
    - The record's +0x55 = 0.
    - FD108[side] is `0x2000` when ctx[2]'s +0x2C is above ctx[3]'s
      (signed, `0x148DE jle`), else `0x1000`.
    - FD11C[side] = 0. Through `0x1490C`, which overwrites it with 1, this
      store is invisible. But `0x14814` has a second entrance: the dword
      `0x00014814` at `0xA46D0` (`14 48 01 00 00 00 00 00`, record 226 =
      3 * 64 + `0x22`) makes it character 3's reaction `0x22` callback,
      which `0x34E2C` calls directly (§25's f = 625 miss). Through that
      entrance FD11C stays 0, so `0x146F0` later takes its `0x3C480` arm.
      (Corrected in review round 1: an earlier text called `0x1490C` the
      only caller.)
- **`0x14590`**: `0x33950(side)`. When ctx[3] +0x10 == `0x39CC8` or
  ctx[3] +0x52 == `0x11`, it returns whether the side's `0x107D2C` word is
  greater than 0 (signed, `0x145BC jle`). Otherwise it returns 0.
- **`0x1461C`** (EAX = slot, EDX = rec, EBX = side; `0x33950(side)`). The
  jump table at `0x1460C` holds `0x1464A`, `0x14699`, `0x146D7` and
  `0x146E8`. It switches on ctx[2] +0x57:
  - 0 waits for +0x42 bit 3. Then it sets +0x57 = 1 and sets FD114[side]
    to the word at `0x9AFDA` (4), with `0x4F944(1)`, when `0x14590` holds.
    Otherwise it uses the word at `0x9AFD8` (`0x14`).
  - 1 decrements FD114 and calls `0x3F720(ctx[0], old)`. Once the new value
    is below 1 (`0x146BA` reads it as the dword at `0xFD112` >> 16), it sets
    +0x57 = 2.
  - 2 calls `0x146F0(slot, rec)`, then sets +0x57 = 3.
  - Anything above that returns.
- **`0x146F0`**. It overwrites EAX without reading it (`0x146F8`) and uses
  `0x339AC(rec)`:
  - The stream is `0xD2EFC` or `0xD2EBA`, chosen by `0x14590`.
  - The direction bits are the `0x1088E0` word, ORed with FD108 unless
    `ch & 0x30`.
  - With FD11C[side] set:
    - `0x3C148`.
    - `0x3C520` at 1.0.
    - `0x1890C(ctx[0], [0x9AFDC] >> 16 = 0x3200)`.
    - ctx[2] +0x54 = 2.
    - The record's +0x36 = `[0x9AFE0]` * 3 and +0x44 = `[0x9AFE0]`
      (`0x19`).
    - `0x1078F8[side]` = 1.
  - Otherwise `0x3C480` at 1.0.
  - Then `0x3C148` and `0x188DC(ctx[0], ...)`. The value is ctx[3] +0x2C
    minus `[0x9AFDA] >> 16` (`0x1180`) when `ch & 0x20`, else plus it.
  - Then `0x18B04(ctx[0])`, ctx[2] +0x42 `and 0xFB`, and +0x57 = 3.
  - The voice `0x2C3FC(0xB1)` is out of scope.
  - `0x3C148`, `0x3C480`, `0x3C520` and `0x1890C` preserve ECX, so the bits
    survive.
- **`0x3F720`** (EAX = side, EDX = n):
  - `0x33950`, and n < 1 becomes 1.
  - `0x187FC` is called once for the sign and once for the value. Then
    `idiv` gives |d| / n.
  - `0x189FC(ctx[1])` preserves EDX. When it returns 0, the value is 100.
  - Then `0x3C190(ctx[0], v)`.
- **`0x145CC`**: `0x33950(side)`, then EAX = 1.
- **`0x145E4`**: `0x39834(ctx[1], ctx[2] +0x5F)` on `0x33950(side)`.
- **`0x37CD4`** (an anim target: EAX = rec; EDX is pushed and not read).
  When the record's +0x14 (its slot) is set, it XORs the slot's +0x42 with
  8. Then the slot's +0x74 = `0x378` when bit 3 is now set, else 0. Its
  dwords are at `0xD2EB0`, `0xD2EBC`, `0xD2EF2`, `0xD2EFE` (and `0xD2F80`,
  `0xD2F8C`, `0xD2FC2`, `0xD2FCE`, `0xD4B9C`, `0xD4BA8`).
- **Parameter words** (`read_memory 0x9AFD8`, 16 bytes): `0014 0004 1180
  3200 0019 DC00 0000 4020`.
- **`0x396AC`'s threshold** for (character 3, `0x27`) is the word at
  `0xA6C96` = 1. So the first reaction bumps `0x107A80[0x27]` to 1 and
  takes the false arm, and a second one takes the true arm.

### 37.5 The fix, its assertions, and 2386

**The fix** (`a7ccc86`) is 8 functions in `fighter.c` (`fighter_14590`,
`_3f720`, `_146f0`, `_14814`, `_1490c`, `_1461c`, `_145cc`, `_145e4`) and
`anim_code_37CD4` in `actors.c`. Six of them are registered: `0x1490C`,
`0x14814` (review round 1: its `0xA46D0` entrance, with the (slot, rec,
side) shape and side unread), `0x1461C`, `0x145CC`, `0x145E4` and
`0x37CD4`. That is about 0x300 bytes of
raw code, inside the gate. The voices are `PORT:` (spec §7), as in the other
reaction callbacks.

**`check_char3_reaction`** (in `test_fight.c`, after `check_trex_grab`)
patches the five stream heads to plain frame words and restores them. It
checks:
- A: through `0x34E2C` (reaction `0x27`, character 3): the counter bump, the
  stream, the hold, the pset, state 9/7/0, the callbacks, +0x40, +0x55,
  FD108 = `0x2000`, FD11C, and the other side's sentinels.
- A2 (review round 1): through `0x34E2C` with reaction `0x22`, the
  registered `0x14814` entrance: the counter `0x107A80[0x22]` (threshold
  word `0xA6C78` = 1), the stream `0xD2E9A`, the arming, and FD11C[0] from
  its sentinel to 0.
- B/B2: the `0x14590` arms (`0x39CC8`, `0x11`, the word at 0), FD108 =
  `0x1000`, and EBX unread.
- C: the `0x396AC`-true arm, with hold `0x40200000` and state 9/4/0.
- D: `0x1461C` with EAX/EDX from the other side:
  - case 0 with and without bit 3, and both loads, with `0x4F944`'s HUD
    writes
  - case 1 with both `0x189FC` results (`0xFF9C`, `0x20`), the 1 -> 0 exit,
    and n = 0 -> 1 (with rec +0x34 re-seeded, review round 1)
  - case 2 through `0x146F0` in both arms (x minus and plus `0x1180`, the
    anchor y `0x3200`, +0x36/+0x44, `0x1078F8`, `0x3C148`)
  - case 3 returning
- E: `0x145CC` through `0x19020`.
- F: `0x145E4`'s `0x39834(1, ...)`, which counts on side 0, and (review
  round 1) its second operand through `0x39953`'s store `DS_00107D28 = b`:
  ctx[2]'s +0x5F (0x27), not ctx[3]'s or +0x64; and `0x145FC and edx,0xff`
  zero-extends (0xA7 stays 0xA7; 0xA7 is outside `0x3AFC4`'s 0..0x3F,
  which is the raw's `0x62003` error and the port's zero triple, but the
  store at `0x39953` still happens).
- G: `0x37CD4` as `anim_indirect` calls it.

**Mutations.** `scratchpad/t27/mut/mut27.py` made 37 single-site edits:
36 fail 1..16 assertions. One survives, and it is equivalent: `0x146F0`'s
FD11C arm calling `0x3C480` in place of `0x3C520`. Both begin the same
stream. The only state they leave differently is rec +0x1C, which the
following `0x1890C` sets absolutely (`y - AB4[side]` after its latch). The
sources were restored and checked with `cmp`. Review round 1 added 7
(`0x14814`'s registration, its FD11C store removed and changed, `0x145E4`'s
ctx[3], +0x64 and sign extension, and `0x3F720`'s call skipped at n = 0):
all 7 fail 1..6 assertions (`mut27d.log`).

**Measured (the dump with the fix).**

| measurement | before (`c0edb4c`) | after (`a7ccc86`) |
|---|---|---|
| fn_resolve misses (non-stub), 2800 loops | `0x1490C` at loop 2784 | none |
| front-end / demo-fight | `[560..1884]` 517/801/3/2; empty, N 1886 | unchanged |
| attract2 `[1885..3616]` | 355/138/3/1230/6, first 2386 | **unchanged**, first 2386 |
| sum of best-match pixel diffs, captures 2386..2402 | 439742 | 419003 |

`select.log` changes only from loop 2784.

**2386, characterised.** Capture 2386's rows 0..127 equal cycle-2 frame 978
(the state-6 -> 7 frame). Its rows 128 on hold both fighters in poses that
no port frame 978..994 shows. The raptor region differs from frame 979 in
2128 pixels and the ape region in 1892. No shift within +-8 pixels explains
either, and the background rows match. The port's state after loop 2784
(probe, reverted):
- The raptor takes reaction `0x27` on the false arm: 9/7/0, stream
  `0xD2E9A`.
- The ape is at +0x52/53/54 = 3/4/2 on stream `0xE3AF0` at hold 1.0, and
  jumps (+0x52 = 4) after loop 2786.

The capture agrees on the later course: the ape jumps at 2389 (about port
frame 981.4), and the raptor curls at 2393 (port 985). The first state-7
frame's poses still differ, and the ape is not touched by this fix. Only 16
second-demo frames are in the dump (cycle-2 978..994, `FE_LOOPS` = 2800).

**The staged next step.**
1. Identify the two sprites in capture 2386's rows 128 on. Render the
   ape's `0xE3AF0` frames and the raptor's `0xD2E9A`/`0xD2F1E` sequence
   (`0x181F..0x1825`) with `tools/gra_render.py` and match the crops.
2. Trace who starts the ape's `0xE3AF0` at loop 2784 (the demo AI's first
   command). Compare with the first demo's first state-7 frame, which is
   explained (Task 3b).
3. Check the raptor stream's opcodes before the first sprite: `0xDC00`
   (operand `0xD1658`), `0xED40` (the sprite list `0xD2F1E`) and `0xB840`.
4. Raise `FE_LOOPS` only when a frame after 2386 can be explained.
5. Known entrances to this family: `0x1490C` (character 3, `0x27`,
   `0xA4734`) and `0x14814` itself (character 3, `0x22`, `0xA46D0`), both
   registered now. `0x14814` was reached at f = 625 in an earlier run of the
   first demo (§25) and is not reached in the current 2800-loop run.

## 38. The second demo's fighter passes (`0x34978`) and the raptor's block (`0x1A7CC` family), captures 2386..2673 (roar-timing Task 28, `5448e09`, `9469a30`)

**Result in one line.** Capture 2386 was not a pose difference: the
original's second-demo logic equals the port's frame for frame (a DOSBox-X
live-RAM poll), and the owner is state 6's unported reset call `0x34978`,
which restarts the live-fighter count. With it the second demo's fighter
passes run and 2386..2460 are explained (N = 2461, `5448e09`). At 2461 the
raptor blocks the ape's punch through the block family `0x1A7CC`/`0x1A6AC`/
`0x1A8F4`, which were named gaps, plus the already-ported `0x1A640` whose
call in `0x1A978` was unwired. With them 2461..2673 are explained
(N = 2674, `9469a30`; corrected in review round 1, §38.6).

### 38.1 The ground truth: a DOSBox-X live-RAM poll

The §37.5 plan guessed that the fighters' poses differ. A measurement of the
original settles it. `scratchpad/t28/dbpoll.py` runs the pinned original
(`make title-pin`, the same binary the front-end capture was taken from)
under DOSBox-X with `-set "dosbox memory file=..."` and no input, as
`make frontend-capture` does. It finds the data object's base from the
string `"RAGE.S16"` (data VA `0x8002D`) and checks it against the parameter
words at `0x9AFD8` (`14 00 04 00 80 11 00 32`). The base was `0x266000`
(delta `0x1E6000`) in both runs. It then polls the guest RAM every ~0.5 ms
and logs, per logic frame (`DS_000EF6DC`) while the state is >= 6, the last
sample before the counter changes:
- the state, `DS_000F0A72`, the LCG `DS_000EF6D8`, the command words
  `DS_001088E0/E2` and `DS_00104B1B`
- per slot: `+0x52/+0x53/+0x54`, `+0x57`, `+0x63`, `+0x41`, `+0x7A`, the
  record's stream `+0x08`, timer `+0x20`, `+0x52` and the pset sprite id
- both AI blocks `0x1081F0 + side * 0x40`

A port probe (reverted) prints the same fields after each driver loop.
Findings:
- The first demo's f = 1956..1960 match the port exactly (LCG
  `0x8612D6C5` -> `0x10F7DB07`, commands `1010/0002`, the T-rex 0E/00/00 then
  09/08/00, the raptor 09/08/00 on `0xD2316`).
- The second demo's f = 3669..3734 match too, including the ape's stale AI
  block: side 1 is still active on the first demo's raptor move (step
  `0x93068`, move `0x0C`), so the ape's first command `A0A0` is that stale
  move in the original as well. `0x46670`, the only other writer of the AI
  blocks' `+0/+4/+0x2E`, is reached only from `0x20EF8` <- `0x25C1C`/
  `0x26A50`, which are `0x24C5C` mode cases other than 3. So the staleness
  is faithful.
- The only differences in the AI fields are one frame early, because the
  command block runs before `0x24CDB` increments the counter. The stream
  field reads negative when `rec+0x08` holds an id rather than a pointer.
  Both are sampling artifacts.

So 2386's difference is drawing, not logic. Per pixel: its rows 89..127
equal port frame 978, rows 191 on equal 979, and rows 128..188 carry 979's
silhouettes with other interior pixels. The RGB deltas are symmetric, and no
shift within +-16 explains them.

### 38.2 `0x34978` (raw, `read_memory` + capstone)

`0x20DF4` (state 6's reset, `0x11AC4`) makes its pre-branch calls in this
order: `0x29B70`, `0x2C390`, `0x12750`, `0x49300`, `0x28E98`, `0x34978`
(`0x20E42`), `0x2C074`. It also stores `[0xF0A48]`, `[0x100B4C]`,
`[0x104AE8]`, byte `[0x1088EC]` and byte `[0x104B15]` = 0.

`0x34978`:
- `mov ecx,2; mov eax,0x1077A8; xor edx,edx; call 0x654C7`. `0x654C7` is a
  dword fill: it stores one dword at a time while `al & 0x1F` is non-zero
  (`0xA8`, then `0xAC`), so exactly `DS_001077A8[0]` and `[1]`.
- `mov word [0x1078F6],dx` and `mov byte [0x1078FA],ah` (0).

`DS_001078FA` is the live-fighter count. `0x33C78` (`fighter_spawn_slot`)
increments it per spawn (`0x33CDA..0x33CEA`). `0x1958C` (`fighter_pass_a`,
`0x1959C`) and `0x34D8C` run only when it is 2. The first demo starts from
BSS 0 and counts 2. The port had no reset, so the second demo counted 4.

**The fix** (`5448e09`): `fighter_slots_reset` (fighter.c), called from
`game_state_6` after `fight_list_init`, at the raw's position. The other
unported parts of `0x20DF4` stay a named gap:
- `0x2C390` and `0x2C074` touch only globals the port never reads
  (`0x105C0C`/`0x105C14`, `0x105BF0`/`0x105BF4`).
- `0x28E98` re-links the `0x104880`/`0x104888` lists that type-`0x0A`/`0x19`
  actors use. None is spawned in the measured windows.

### 38.3 The block family (raw, `read_memory` + capstone)

With `0x34978`, the second demo's logic matches the poll until f = 3735
(loop 2848). There the original's raptor goes to 6/1/0 on stream `0xD2636`
(`*(u32*)0xC8F4C` = `0xC8F40[3]`) with an extra LCG draw, against the ape's
punch. The port stayed 9/0/0. A `fn_resolve` miss probe over the run finds
only the `0x5D812` stub, so the owner is an unported path.

- **`0x1AB5C`** (`fighter_input_mask`). The block arm calls `0x18B04(side)`
  at `0x1AC06` (when the other's `+0x5F != 0xFF` and the mask overlaps the
  facing base; BL = 1 first). It then calls `0x1A7CC(side)` at `0x1AC7A`
  after the `+0x54` store. Both were `PORT:` gaps (§7.12). `0x18B04` pushes
  EBX, so BL survives.
- **`0x1A7CC`** (EAX = side, `0x33A10` context):
  - `+0x43 &= 0xFD`, then `0x18B04(ctx[1])`.
  - `+0x61/+0x60/+0x62` = 0/`0x1E`/1.
  - When the other's `+0x5F <= 0x3F` (`0x1A810 jg`, a byte), `0x3AFC4
    (ctx[0], +0x5F)` and `+0x60` = byte `+0xA` of `triple[0]`. The `+0x64`
    alternative (`0x1A842..0x1A86B`) cannot run, because `+0x5F < 0x40` was
    just tested. It is transcribed verbatim.
  - `0x1A6AC(ctx[3], ctx[5])`.
  - The other side's word `0x100CE0[ctx[0]]` + 1 = k (read back as the
    dword at `0x100CDE` `sar 16`). With k in 0..6 (`0x1A893 jae`,
    `0x1A89D jge`), `+0x60` = `(s16)word[0xA2C4C + 2k]` * `(s8)+0x60` / 100
    (`idiv`). Otherwise it is 2. The table at `0xA2C4C` is `0064 0064 0055
    0041 0032 001E 0014`.
  - `+0x52/+0x53` = 6/1.
- **`0x1A6AC`** (EAX = slot, EDX = rec; `0x33A68` builds `0x33A10`'s
  context from `rec+0x51`):
  - `0x18B04(ctx[1])`.
  - With `+0x54` == 0 (`jbe`) and `+0x43` bit `0x20` clear, `0x3C480(rec,
    0xC8F40[char], 3.0)`, then `+0x43 = (+0x43 & 0xCF) | 0x20`.
  - With `+0x54` == 1 and bit `0x10` clear, the same with `0xC8F90[char]`
    and `| 0x10`.
- **`0x1A640`** (EAX = side): the record's `+0x28` bit `0x4000` clear gives
  `0x1000` when the command word has `0x1000`. Set, it gives `0x2000` when
  the word has `0x2000`. Otherwise 0. EDX is pushed and popped. It was
  already ported as `fighter_1a640` (its callers `0x359E0`, `0x36430` and
  `0x364FC` use it); only its call in `0x1A978` (`0x1AA93`) was unwired, and
  the old `PORT:` comment there was stale (review round 1).
- **`0x1A8F4`** (EDX = rec; EAX is overwritten at `0x1A8F7`):
  - `0x2BC30(ctx[5], 0xC8F68[char], 3.0)`, or `0xC8FB8[char]` when
    `+0x54` == 1.
  - `+0x43 &= 0xCF`, and `+0x52/+0x53/+0x62/+0x60` = 9/0/0/0.
- **`0x1A978`**, the `+0x52 == 6` handler (`0x34C73`). `0x1AA24` calls
  `0x1A6AC(self, ctx[5])` after both `+0x54` stores. In the `+0x62 == 0`
  arm (`0x1AA8C..0x1AB04`), `0x1A8F4(ctx[5])` runs when `0x1A640(side)` is
  0, or the command has `0x8000`, or it has a `0x000F` bit, or (`(s8)+0x60
  < 1` and the other's `+0x5F` and `+0x64` are both `0xFF`).
- **A raw-wins correction.** At `0x1AA5F` the raw has `cmp dx,[other+0x84];
  jne 0x1AA6F`, so `other+0x8A = 0` runs when `self+0x86 == other+0x84`.
  The port had `!=`. No block happened in the first demo, so no run had
  reached it.
- **Tables** (read from the data object): `0xC8F40` = `0xE72EA 0xE3F5E
  0xED04C 0xD2636 0xEAB94 0xD4258 0xE0B70`; `0xC8F68[3]` = `0xD2650`;
  `0xC8F90[3]` = `0xD265A`; `0xC8FB8[3]` = `0xD2674`.

**The fix** (`9469a30`, corrected in `9db3951`): `fighter_block_anim`,
`fighter_block_start` and `fighter_block_end` in fighter.c, with `0x33A68`
through the existing `fighter_ctx_rec_swap`. They are wired at
`0x1AC06`/`0x1AC7A` and `0x1AA24` (twice)/`0x1AB04`, and `0x1AA93` calls the
existing `fighter_1a640`, with the `0x1AA5F` correction. That is 3 new
functions, about `0x234` raw bytes (corrected in §39.5), inside the gate.
(`9469a30` had added a duplicate `fighter_block_dir` for `0x1A640`; `9db3951`
removed it.)

### 38.4 The assertions and mutations

- `check_state6` now seeds `DS_001078FA = 2` (the first demo's leftover) and
  `DS_001078F6 = 0x1234`, and asserts 2 and 0.
- `check_slots_reset`: the four targets, and the neighbours `0x1077A4`,
  `0x1077B0`, `0x1078F4`, `0x1078F8` and `0x1078FB`.
- `check_block` (test_fight.c, after `check_char3_reaction`). The char-3
  entries of the four tables point at crafted one-word streams, restored
  afterwards.
  - A..A4: `0x1A7CC`'s `+0x43`, `+0x61/+0x62/+0x52/+0x53`, the stream and
    hold, the other side's counter, and `+0x60` for k = 2 (from the
    record's byte and the `0xA2C4C` word, both read from the data), k = 7,
    k = -1, the `jg` skip with `+0x64 < 0x40`, and character 6's move
    `0x24` (byte 55: 46, where `/99` gives 47).
  - B/B2: `0x1A6AC`'s two arms and its skips.
  - C: `0x1A8F4`'s two tables and its stores.
  - D: `0x1A640`'s four cases, through `fighter_1a640`.
  - E: the `+0x62 == 0` arm, held and released (a `0x000F` bit, `0x8000`,
    `+0x60` 0 with no attacker, and not held back).
  - F: `0x1AA5F`, equal and unequal.
  - G: the `+0x61` arm's two `0x1A6AC` calls.
- The driver: the second demo's 6 -> 7 at loop 2783 with `DS_001078FA` = 2,
  the raptor's first block at loop 2848 (the poll's f = 3735), and 1295
  cycle-2 frames.

**Mutations.** `scratchpad/t28/mut28.py` made 10 edits (`0x34978`) and
`mut28b.py` made 35 (the block family). After three assertions were added
(A3's `+0x64`, A4, and E's `0x8000`), 44 fail 1..6 assertions. The one
survivor is equivalent: dropping `0x1AC06`'s `0x18B04`. BL = 1 always leads
to `0x1A7CC`, which calls `0x18B04` for the same side twice more, and
nothing between them changes the positions it reads. The sources were
restored and checked with `cmp`.

### 38.5 Measured

| measurement | before (`741a5f5`) | `5448e09` | `9469a30` |
|---|---|---|---|
| `FE_LOOPS`, cycle-2 frames | 2800, 995 | 2900, 1095 | 3100, 1295 |
| attract2 `[1885..3616]` clean/splice/trans/unexpl/black | 355/138/3/1230/6 | 383/182/6/1155/6 | 547/270/7/902/6 |
| attract2 first unexplained (2384 allowed) | 2386 | **2461** | **2674** (raw 7093) |
| front-end `[560..1884]`, demo-fight | 517/801/3/2; empty, N 1886 | unchanged | unchanged |
| polled logic equal to the original through | f = 3734 | f = 3734 | f = 3926 |

`FE_LOOPS` is a measurement window, not a raw value. Each value keeps the
current first unexplained frame inside the dump: 2461 is about loop 2848,
and 2674 about loop 3031. One run of the driver takes about 50 s at 3100
loops (47 s at 2800).

**2674, characterised.** (Corrected in §39.1: the scene is not offset, and
2674 is the ape's pose.) From about capture 2670 the whole scene is offset:
the background and both fighters shift, and the diffs span rows 65..199 and
all columns. That is a camera or position difference. The polled fields do
not include positions or the camera, and they stay equal through f = 3926.
The first polled difference is f = 3927 (loop 3040): the original restarts
the ape's block stream at `0xE3F5E` (`rec+0x52` 0, id `0x141E`), while the
port runs on at `0xE3F66` (`+0x52` 2, id `0x1422`).

**The staged next step.**
1. Extend the poll with both records' `+0x2C/+0x30`, the slots' `+0x2C` and
   the camera words (`DS_00100AB0`, `DS_000F0AEC`/`F0AF0`). Find the first
   frame where they differ; it should lie at or before f = 3918.
2. Find who restarts the ape's block stream at f = 3927. `0x1A6AC` skips
   while `+0x43` bit `0x20` is set, so look for a writer that clears it
   (`0x1AB9F`, `0x1A8F4`) or another `0x3C480`/`0x2BC30` caller with
   `0xC8F40`.
3. The block family's `0x100CE0` counter is also written by `0x36870`
   (`0x368F9`) and `0x392A0` (`0x3931F`), both ported.

### 38.6 Review round 1 (`9db3951`)

- **`0x1A640` was ported twice.** `fighter_block_dir` duplicated
  `fighter_1a640`. It is removed; `0x1A978`'s `0x1AA93` calls
  `fighter_1a640(ctx[1])`, and check_block D asserts through it. The claims
  that `0x1A640` was a named gap (§38 opening, §38.3, README, `game_flow.md`)
  are corrected: only its call site was unwired. The demo-fight record's
  §7.11 also lists `0x1A734`, which stays a named gap (`0x3B443`), so §38
  closes the block helpers of §7.11, not §7.11 as a whole (`fight.h`).
- **`0x33A68`.** `0x1A6AC` and `0x1A8F4` now call the existing
  `fighter_ctx_rec_swap` instead of inlining it.
- **check_block's shared state.** It now snapshots and restores what
  `c3_seed` and the block path write: the five stream words (`0xD2E86`,
  `0xD2E9A`, `0xD2EDC`, `0xD2EBA`, `0xD2EFC`), `0x107A80[0x80]`, `0x107D2C`,
  `FD108..FD11F`, `0x1078F8`, `0x1080AC/AE` (`tb_seed`), and `0x18B04` ->
  `0x18714`'s `0x100AF0..0x100AF7` and `0x100AB0..0x100ABF`. A temporary
  whole-data-object diff around `check_block` (reverted) left only the two
  fighter slots' fixture bytes, which `check_char3_reaction` does not
  restore either.
- **Mutations re-run** (`scratchpad/t28/mut28c.py`): 7 edits on
  `fighter_1a640`, its `0x1AA93` call and the two `0x33A68` sites. 6 failed
  at once. The survivor built `0x1A6AC`'s context from the wrong side, so its
  `0x18B04` ran for the other side; B now seeds side 1's record `+0x18` and
  asserts it unchanged, which kills it (1 new assertion). mut28b's two
  `fighter_block_dir` edits no longer apply. The block family's count is
  now 3 + 43 applicable edits, all failing except the one equivalent
  (`0x1AC06`).
- **Other writers of `DS_001078FA`.** Besides `0x33C78` (increment) and
  `0x34978` (reset), `0x296B8` and `0x274FC` write it, and so do four
  unowned sites: `0x25BC5`, `0x270C7`, `0x269A5` and `0x295CB`. They lie in
  the interactive modes, which the demo does not run.
- The `0x1A842..0x1A86B` comment is now a plain address comment (it marks a
  verbatim transcription, not a deviation), and the `0x1490C` header is
  rewrapped to 80 columns.

## 39. The ape's block restart `0x1A734` at capture 2674 (roar-timing Task 29, `c9875d1`)

**Result in one line.** Capture 2674 is not a scene offset. The second demo's
positions and camera equal the original's frame for frame, and 2674 differs
only in the ape's pose: at f = 3927 the original restarts the ape's block
stream through `0x1A734`, which `0x3B298` calls at `0x3B443` and which was a
named gap (§7.11 of the demo-fight record). With it 2674..2762 are explained
(N = 2763, `c9875d1`).

### 39.1 The poll, extended

`scratchpad/t29/dbpoll.py` is §38.1's poll with more fields. Per slot it adds
`+0x2C/+0x30/+0x34/+0x38` and `+0x43`, and the record's `+0x18/+0x1C`.
Globally it adds the camera `DS_000F0AF0`/`DS_000F0AEC`, its mode bytes
`DS_000F0AFE`/`DS_000F0AFF`, `0x100AB0..0x100ABF`, `DS_001078F2` and the
dword at `0x100CE0`. The data base was `0x266000` again. A port probe
(reverted) prints the same line after each driver loop.

- **Positions and the camera match** through the whole run the port logged
  (f = 3986, loop 3099). §38.5's step 1 expected a difference at or before
  f = 3918. There is none.
- The only difference that is not a sampling tear is the ape's (side 1)
  record at f = 3927 (loop 3040). The original restarts its block stream at
  `0xE3F5E` (`rec+0x52` 0, sprite `0x941E`). The port runs on at `0xE3F66`
  (`+0x52` 2, `0x9422`). The word `0x100CE0[0]` goes 1 -> 2 on that frame
  in both. The ape's `0x1A7CC` increments the other side's word, so it ran
  in both. The ape's `+0x43` reads `0xA0`
  before and after in both, and `0x1A6AC` skips while bit `0x20` is set. So
  the restart is not `0x1A6AC`'s.

**A correction to §38.5.** Its "whole scene offset from about 2670" came from
a capture-to-port frame mapping about 8 frames off. Searching the whole
cycle-2 dump (rows 60..179), each of captures 2655..2673 equals one port
frame exactly (2672 = cycle-2 frame 1233, 0 px). Capture 2674 against frame
1235 (loop 3040 = f 3927) differs in 4985 px. The difference is the ape's
body, plus a tear strip at the right edge. The background and the raptor
match.

### 39.2 `0x1A734` (Ghidra disassembly)

`0x3B298` (`fighter_command_dispatch`) sets `+0x43 = (+0x43 & 0xCF) | 0x20`
(`0x3B405/0x3B40D`) or `| 0x10` (`0x3B433/0x3B43B`). Both arms then fall
into `0x3B43F`: `mov eax,[esp+4]` (ctx[1], the side) and `call 0x1A734` at
`0x3B443`. The port had a `PORT:` comment there.

`0x1A734` (151 bytes, `0x1A734..0x1A7CA`; EAX = side, EDX pushed):
- `0x33A10` builds the context (ctx[1] the side, ctx[3] its slot, ctx[5] its
  record), then `0x18B04(ctx[1])`.
- `+0x61` = `0x0C` (`0x1A74E`). When `(s8)+0x61 > (s8)+0x60` (`0x1A75D cmp
  al,[edx+0x60]; jle`) and `+0x62 != 0` (`0x1A762`), `+0x61` = `+0x60`.
- With bit `0x20` of `+0x43` (`0x1A779`): `+0x54` = 0 and `0x3C480(ctx[5],
  0xC8F40[char], 0x40400000)`. Otherwise, with bit `0x10` (`0x1A79C`):
  `+0x54` = 1 and the same with `0xC8F90[char]`. `char` is `+0x7A`. The push
  shifts the frame by 4, so `[esp+0x18]` at `0x1A7BD` is ctx[5].
- The restart is not gated on the bit it tests, which is the difference from
  `0x1A6AC`. `0xC8F40[1]` = `0xE3F5E` (§38.3), the poll's value.

**Only caller.** `get_xrefs_to(0x1A734)` gives one reference, the call at
`0x3B443`. A rel32 scan of the code object finds only that call. No dword
`0x1A734` appears in the code or the data object.

**The fix** (`c9875d1`): `fighter_block_hit` (fighter.c), called at `0x3B443`.
That is one new function of 151 raw bytes. The demo-fight record's §7.11
listed `0x1A6AC`, `0x1A640`, `0x1A8F4` and `0x1A734`. With §38 and this one,
all four are ported and wired.

### 39.3 The assertions and mutations

- `check_block` H (test_fight.c): both arms (the streams, `+0x54`, the
  3.0 hold and the pset id), the cap, the signed compare (`+0x60 = 0x80`
  caps to `0x80`), the `+0x62` gate, and neither bit set (no stream,
  `+0x54` kept). It also asserts the `0x18B04` side: side 0's record `+0x18`
  takes `hit_record_x(0)`, and side 1's keeps its sentinel.
- The driver: after loop 3040 the ape's record `+0x08` is `0xE3F5E` and its
  `+0x52` is 0 (the poll's f = 3927). There are 1395 cycle-2 frames.

**Mutations** (`scratchpad/t29/mut29.py`): 18 edits. 17 fail. In the first
pass, dropping `0x1A745`'s `0x18B04` survived, so H now asserts the record's
`+0x18`, and that kills it. The context-builder edit first ran in driver
mode, where the unit tests do not run. In unit mode it fails 13 assertions.
The one survivor is equivalent: `>` -> `>=` at `0x1A75D`. When `+0x60` is
`0x0C`, the copy writes the value that is already there.

### 39.4 Measured

| measurement | before (`2cb9a41`) | `c9875d1` |
|---|---|---|
| `FE_LOOPS`, cycle-2 frames | 3100, 1295 | 3200, 1395 |
| attract2 `[1885..3616]` clean/splice/trans/unexpl/black | 547/270/7/902/6 | 575/291/7/853/6 |
| attract2 first unexplained (2384 allowed) | 2674 | **2763** (raw 7182) |
| front-end `[560..1884]`, demo-fight | 517/801/3/2; empty, N 1886 | unchanged |
| polled logic (with positions and camera) equal to the original through | f = 3926 | f = 4002 |

`FE_LOOPS` is a measurement window. A probe at 3840 loops found 2763 at
loop 3116, and 3200 keeps it inside the dump.

**2763, characterised.** At f = 4003 (loop 3116) the original's raptor enters
9/7/0 on stream `0xD3028` under command `0x0510`. The port's raptor stays in
its stance. A `fn_resolve` miss probe (reverted) misses `0x14E44` at loop
3116. That is the dword at `0xA46E4`, the reaction table `0xA3528` + 227 *
`0x14`: character 3's reaction `0x23`. Ghidra has no function at
`0x14E44`. The same probe misses `0x15350` (`0xA470C`, character 3's `0x25`)
at loops 3256 and 3343, and `0x3C32C` at 3448.

**The staged next step.** Decode `0x14E44` from `read_memory` + capstone,
since Ghidra has no function there, in the shape of the `0x1490C` family
(§37), and register it.

### 39.5 §38.3's byte count

§38.3 said the block family was about `0x250` raw bytes. The functions are
134 (`0x1A6AC`), 296 (`0x1A7CC`) and 129 (`0x1A8F4`) bytes, `0x22F` in all.
With alignment padding they span `0x234` bytes (`0x1A6AC..0x1A734`,
`0x1A7CC..0x1A8F4` and `0x1A8F4..0x1A978`). §38.3 now says about `0x234`.

## 40. The raptor's grab `0x14E44` at capture 2763 (roar-timing Task 30, `ce5f295`)

**Result in one line.** At f = 4003 (loop 3116) the original's raptor takes
character 3's reaction `0x23`, whose callback `0x14E44` was not registered.
It is now ported with the `+0x18` hook it stores, `0x14CC4`, and its grab
stream's `0xD100` target `0x14EA4`. The live-RAM poll matches the port through
f = 4179, 2763..2949 are explained, and N = 2950 (`ce5f295`). The throw
`0x14D7C` and the stream target `0x14E80` are not reached and stay named gaps.

### 40.1 The raw (Ghidra `read_memory` + capstone, fixups applied)

Ghidra has no function at `0x14E44`. The bytes `0x14E44..0x14E7E` decode
cleanly between the `ret`/`nop` of the preceding code and `0x14E80`
(`scratchpad/t30/d14e44.txt`).

- **The reaction record.** `read_memory 0xA46E4` reads `44 4e 01 00 00 00 00
  00 ...`: the callback `0x14E44` and no stream word. `0xA46E4 - 0xA3528` =
  227 records of `0x14` = 3 * 64 + `0x23`. So `0x34E2C` only stores `+0x5F`
  and calls the callback at `0x35045` with EAX = slot, EDX = rec, EBX =
  side.
- **`0x14E44`** (59 bytes):
  - `push ebx; mov ebx,eax` (EBX is overwritten before any read), then
    `0x3C4CC(rec, 0xD3026, 0x40000000)` (`0x14E47..0x14E53`). `0x3C4CC` ends
    `ret 4` and preserves EBX.
  - The slot's `+0x52` = 9 (`0x14E58`), `+0x54` = 0 (`0x14E5C`), `+0x53` = 7
    (`0x14E60`).
  - `+0x18` = `0x14CC4` (`0x14E64`), `+0x1C` = `0x14D7C` (`0x14E6E`), and
    `+0x42 |= 4` (`0x14E6B..0x14E78`).
  - `mov al,1; pop ebx; ret`. It writes neither `+0x0C` nor `+0x57`; the poll
    shows the raptor's `+0x57` still 3 from reaction `0x27` (§37).
- **`0x14CC4`** (the `+0x18` hook, 181 bytes; EAX = side, `0x19020`'s call):
  - `0x33950(side)`. When ctx[4]'s (the side's record) byte `+0x61` is 0
    (`0x14CD7`), it returns 1.
  - Otherwise `0x18BD4` fills 16 flag bytes with 2 (`lea eax,[esp+0x18];
    mov cl,1; call 0x18BD4`). `0x18BD4` is 16 byte stores through EAX and
    `ret`, so CL is still 1. Then flags 1, 8, 4, 0xE and 0xD = DH = 0 and
    flag 9 = CL = 1 (`0x14CF9..0x14D0D`), and `0x18C14(ctx[0], flags, EBX
    = 0, ECX = 0)` (the default box tables).
  - A 0 result goes to the distance: `0x187FC` is called once for the sign
    and once for the value (`0x14D22..0x14D34`). |d| > `0x3200` (`jg`) or
    |d| < `0x1900` (`jl`) gives 1, else 0.
  - A 1 (from either) starts `0x2BC30(ctx[4], 0xD3062, 0x40400000)`
    (`0x14D55..0x14D63`; `0x2BC30` ends `ret 4`, so `[esp+0x10]` is ctx[4]
    again at `0x14D68`).
  - ctx[4]'s `+0x61` = 0 and the result is returned. `0x19020` stores
    `DS_00100AF8[side]` = (result == 0).
- **`0x14EA4`** (81 bytes; the `0xD100` target, EAX = rec; EDX is pushed,
  used as scratch and popped):
  - The other side's slot pointer `DS_001077A8[rec+0x51 ^ 1]`; 0 returns.
  - `ebx = [rec+0x4F] sar 24`, the signed byte `+0x52`; `mov bx, word
    [ebx*2 + 0x9AFA8]`; `shl ebx,6`; `movsx edx,bx`. So the step is
    (s16)(word << 6).
  - With `0x1A570(rec+0x51)` non-zero it adds the step to the other slot's
    record's `+0x18`; else it subtracts it.
  - The words at `0x9AFA8` read `2D00 3840 2F80 2D00 29C0` and then zeros
    (`read_memory`). The raptor's record `+0x52` is 8 when the port reaches
    it at f = 4019, so the step there is 0.
- **The streams** (`read_memory 0xD3026`, words): `CD40 1835`, `B840 0008
  000D3026` (the head loop), `8201`, `D100 00014EA4 0000` at `0xD3034`,
  `CD40 1835`, `B840 0014 000D3034` (a loop back to the `D100`), `CD40 1835`,
  `B840 0020 000D3048`, `D100 00014E80 0000` at `0xD3054`, `D500 00036870`.
  The miss stream `0xD3062` starts `8F40 000B CD40 1835 B940 ...`.

### 40.2 Entrances

- `0x14E44`: the data object holds the dword once, at `0xA46E4`. The code
  object holds no such dword and no `call`/`jmp`/`jcc` rel32 to it.
  `get_xrefs_to 0x14E44` is empty (Ghidra has no function there).
- `0x14CC4`: one dword, the `0x14E44` store (`0x14E67`); none in the data
  object, no rel32. `get_xrefs_to` gives the same one DATA reference
  (`0x14E64`).
- `0x14D7C`: one dword at `0x14E71`, likewise (`get_xrefs_to`: `0x14E6E`).
- `0x14EA4`: one dword, at `0xD3036` in the grab stream. `0x14E80`: one, at
  `0xD3056`. Neither appears in the code object.

### 40.3 The named gaps

- **`0x14D7C`** (the `+0x1C` throw, EAX = side; `0x193B0` calls it at
  `0x19505` only for a winner, and 0x14CC4 must return 0 for that):
  `0x33950`, `0x18B04(ctx[1])`, `0x1088BF` = 4 when the side's `0x107D2C`
  word is at least 4, `0x39A10(ctx[4], 0x309)` and `0x39A10(ctx[5],
  0x309)`, `0x3C208(side, (s16)word[0x9AFA4 + 2 * ctx[3]'s char])`,
  `0x39834(ctx[1], ctx[2]'s +0x5F)`, `0x2BC30(ctx[5], 0xC91C0[ctx[3]'s
  char], 3.0)`, ctx[3]'s `+0x41 |= 0x80` and state 9/4, and the voice
  `0x2C3FC(0xB3)`. `0x3C208` (10 call sites) is unported and needs the
  unported `0x18AF8`, `0x3B8D8` and `0x3B90C`.
- **`0x14E80`** (the second `0xD100` target): the other slot's record
  `+0x55` = 1 and the other slot's `+0x74` = 0. It lies after the `0xD3034`
  loop, which the hook's miss restart cuts.
- A `fn_resolve` miss probe over 3840 loops (reverted) reaches neither. In
  the second demo the hook returns 1 at f = 4020 (loop 3133) and the raptor
  runs the miss stream, as the poll shows (`0xD3068`, the ape still 6/1/1).

### 40.4 The assertions and mutations

`check_char3_grab` (`test_fight.c`, after `check_slot_hook`) patches the two
stream heads to plain frame words and restores them. It snapshots and
restores the two slots, `DS_001077A8`, the `c3_seed` globals, `0x100AB0..
0x100ABF`, `DS_00100AF8/AFC`, the command words, `DS_001088A8`,
`0xA6728 + 2` and `0x1080AC/AE`. A temporary whole-data-object diff around it
(reverted) first found `DS_001088A8` (`0x34E2C`'s store); after adding it
the diff was empty.
- A/A2: `0x14E44` through `0x34E2C` (reaction `0x23`, character 3) and
  directly with EBX = 1: `+0x5F`, the stream and its 2.0 hold, the sprite,
  9/7/0, `+0x18`/`+0x1C`, `+0x42` (`0x09` -> `0x0D`, `0x04` kept), and
  `+0x0C`/`+0x57` and the other side's sentinels unchanged.
- B..B6: `0x14CC4`. B: `+0x61` = 0 returns 1 before `0x18C14` (its flag-0xE
  mark stays) and the other record's `+0x61` is not the gate. B2: every check
  passes and |d| = `0x2000` gives 0 with `+0x61` cleared. B3: the edges
  `0x3200`/`0x1900` in, `0x3201`/`0x18FF` out (the miss stream at 3.0). B4:
  a negative d. B5: flags 1, 4, 8, 9 and 0xD each fire (1 and the miss
  stream). B6: through `0x19020`.
- C: `0x14EA4` as `anim_indirect` calls it: the 16-bit truncation (index 1
  gives `0x1000`, not `0xE1000`), the sign (index 2 gives `-0x2000`), the
  subtract arm, a negative index (`0x9AFA6`), side 1 on actor 1's bit, and no
  other slot.
- The driver: after loop 3116 the raptor's record is at `0xD3028` with slot
  state 9/7/0 and hook `0x14CC4`; after loop 3133 at `0xD3068`; 1495 cycle-2
  frames.

**Mutations** (`scratchpad/t30/mut30.py`, `mut30.log`): 41 single-site
edits, 39 in unit mode and the two registrations of `0x14E44`/`0x14CC4`
again in driver mode. 40 failed at once (1..17 assertions; in driver mode
the loop-3116/3133 samples fail). The survivor dropped `0x14EA4`'s
null-slot return: C's "no other slot" case could not see the dereference of
slot 0. C now plants a pointer to the side-0 record at `mem[0]` (restored)
and asserts that record's x unchanged, which kills it. All 41 fail. The
sources were restored and checked with `cmp`.

### 40.5 Measured

| measurement | before (`c0ec8b1`) | `ce5f295` |
|---|---|---|
| `FE_LOOPS`, cycle-2 frames | 3200, 1395 | 3300, 1495 |
| attract2 `[1885..3616]` clean/splice/trans/unexpl/black | 575/291/7/853/6 | 684/369/7/666/6 |
| attract2 first unexplained (2384 allowed) | 2763 | **2950** (raw 7389) |
| front-end `[560..1884]`, demo-fight | 517/801/3/2; empty, N 1886 | unchanged |
| polled logic (with positions and camera) equal to the original through | f = 4002 | f = 4179 |
| non-stub `fn_resolve` misses, 3840 loops (probe) | `0x14E44` 3116, ... | `0x3C32C` 3293, `0x3A6D4` 3295 on |

The poll comparison ignores the one-frame command-word tears (f = 3912,
3929, 4051, 4056, 4132). `0x15350`, which §39.4's probe missed at loops 3256
and 3343, is no longer reached.

**2950, characterised.** Capture 2949 equals the splice of cycle-2 frames
1486/1487 (row 191, 0 px). Capture 2950 differs from its best splice
(1488/1489, row 144) in 4949 px, rows 88..192. Frame 1488 is loop 3293
(f = 4180). There the original's raptor leaves its reaction stream `0xD24F0`
for its stance: 00/00/00, stream `0xD2136` at 3.0, `+0x41` = 0 and
`DS_00100AB0` = `0xFFFFFF00`. The port misses `fn_resolve(0x3C32C)`: the
`0xD500` target at `0xD24FC` (the dword at `0xD24FE`, one of 9 sites) in
that stream. It runs on at `0xD2500` in 9/8/0 and later enters 0x10/0x0A,
missing `0x3A6D4` (a code pointer stored at `0x3A7D5`) from loop 3295.

**The staged next step.** Ghidra has no function at `0x3C32C` (nor at
`0x3A6D4`), so decode it from `read_memory` with capstone, as here, and
register it as the opcode-`0x15` target (EAX = rec). Then re-run the poll
comparison past f = 4180; `0x3A6D4` should drop out once the raptor returns
to its stance.

## 41. The raptor's stance return `0x3C32C` at capture 2950 (roar-timing Task 31, `fcce893`)

**Result in one line.** At f = 4180 (loop 3293) the original's raptor ends
its reaction stream `0xD24F0` on the `0xD500` target `0x3C32C`, which was not
registered. It is now ported. The live-RAM poll matches the port through
f = 4308, 2950..3098 are explained, and N = 3099 (`fcce893`). `0x3A6D4`,
which §40.5's probe missed on the diverged path, is no longer reached (ported and
unit-tested since, §41-A).

### 41.1 The raw (Ghidra `read_memory` + capstone, fixups applied)

Ghidra has no function at `0x3C32C`. The bytes `0x3C32C..0x3C355` (42 bytes)
decode cleanly up to the `ret`; `0x3C356` is `mov eax,eax` padding and
`0x3C358` starts another function (`scratchpad/t31/d3c32c.txt`).

- `push ebx; push edx; mov edx,eax` (EAX = rec; EDX only saves it, so the
  operand is never read).
- `xor ebx,ebx; mov bl,[eax+0x51]` (the side), then `eax = side * 37`
  (`lea eax,[ebx*8]; add eax,ebx; shl eax,2; add eax,ebx`).
- `xor bl,bl; mov [eax*4 + 0x107804],bl` (`0x3C343/0x3C345`): the byte at
  `0x107804 + side * 0x94`, the side's slot `+0x54`, is set to 0.
- `mov eax,edx; call 0x36870` (`0x3C34C/0x3C34E`), `pop edx; pop ebx; ret`.

So `0x3C32C` forces the slot's `+0x54` to 0 before `0x36870`, whose
`+0x54 == 0` arm restarts the side's record on its stance `0xC8950[char]` at
3.0 and sets state 0/0 (§7 of the demo-fight record; `check_deep_callees` E).
For the raptor that is `0xD2136`, the poll's value.

### 41.2 Entrances

- The data object holds the dword `0x0003C32C` 9 times: `0xD24FE`,
  `0xE4856`, `0xE48A0`, `0xE492E`, `0xEA92A`, `0xEA9DE`, `0xEB4A0`, `0xECEFC`
  and `0xECF58`. Each follows a `0xD500` word (opcode `0x15`, mode `0x4000`),
  so each is the same animation-opcode target and one registration covers
  all 9.
- The code object holds no such dword and no `call`/`jmp`/`jcc` rel32 to it.
  `get_xrefs_to 0x3C32C` is empty.
- `0x36870` was already ported (`fighter_36870`) and registered as its own
  `0xD500` target.

### 41.3 The fix, its assertions and mutations

`anim_code_3C32C` (actors.c) clears `DS_00107804 + side * 0x94` and calls
`fighter_36870(rec)`; it is registered next to `0x14EA4`.

- `check_stance_return` (`test_fight.c`, after `check_char3_grab`) calls the
  registered target as `anim_indirect` does, `(rec, 0xFFFFFFFF)`, for each
  side. Both slots' `+0x54` are seeded 3. `0x36870` still runs its resets
  before the switch, but its case-3 arm does nothing, so only the clear
  reaches the stance arm. Both words of `DS_00107D2C` are seeded 0, so
  `0x39040(other)` (gate word `0x107D2C + other * 2`) skips its body for
  either side. It checks that `0x3C32C` resolves and differs from
  `fn_resolve(0x36870)`, the registered wrapper. It asserts the clear, the
  restart (the stream `0xC8950[char]`, the pset's sprite id, `+0x4D` =
  `0x1E`, state 0/0) and the other side's sentinels. It snapshots and
  restores the slots with `DS_001077A8`, `DS_001014EC`, `DS_00104B00`,
  `DS_00107D2C`, `DS_00100CE0`, `DS_00100AF8`, `DS_000FD148`, `DS_00107D20`,
  `0x107A80` and the two `0xC8950` entries. A temporary whole-data-object
  diff around it (reverted) was empty. The same diff found 37 bytes when the
  slot restore was dropped, so it would have seen a leak.
- Review fix (`41e0f46`): the first version seeded only `DS_00107D2C`, so
  for side 0 the gate read the unseeded `0x107D2E`. With `0x107D2E` = 5
  planted before the check (the diff, reverted), the version without the
  second seed leaked 193 bytes, the LCG `0xEF6D8` among them. With it the
  diff is empty. The first registration check compared against
  `fighter_36870`, which is not what `0x36870` resolves to, so no mutation
  could fail it. It now compares against `fn_resolve(0x36870)`.
- The driver: after loop 3293 the raptor's record is at `0xD2136`, its slot
  `+0x52/+0x53/+0x54/+0x41` are 0, and `DS_00100AB0` is `0xFFFFFF00`. There
  are 1695 cycle-2 frames.

**Mutations** (`scratchpad/t31/mut31b.py`, `mut31b.log`, re-run after the
review fix): 12 single-site edits, 11 in unit mode and the registration
again in driver mode. All 12 fail. The twelfth registers `0x3C32C` to
`0x36870`'s wrapper and fails the registration check (and 12 more). In unit mode 1..14 assertions fail: dropping the clear, writing the
side or 1, a fixed or the other slot, the neighbouring byte, a wrong side
byte, dropping or reordering the `0x36870` call, and the registration. In
driver mode the loop-3293 samples read `0xD2500`, `0x09080080` and
`0x180`. The sources were restored and checked with `cmp`.

### 41.4 Measured

| measurement | before (`38c4efc`) | `fcce893` |
|---|---|---|
| `FE_LOOPS`, cycle-2 frames | 3300, 1495 | 3500, 1695 |
| attract2 `[1885..3616]` clean/splice/trans/unexpl/black | 684/369/7/666/6 | 771/430/8/517/6 |
| attract2 first unexplained (2384 allowed) | 2950 | **3099** (raw 7539) |
| front-end `[560..1884]`, demo-fight | 517/801/3/2; empty, N 1886 | unchanged |
| polled logic (with positions and camera) equal to the original through | f = 4179 | f = 4308 |
| non-stub `fn_resolve` misses, 3840 loops (probe) | `0x3C32C` 3293, `0x3A6D4` 3295 on | `0x15350` 3422, `0x151C0` 3551 |

The probe (`scratchpad/t31/probe.py`, reverted and checked with `cmp`) is
§39.1's poll print, `FE_LOOPS` 3840 and a `fn_resolve` miss print with the
frame counter. The poll comparison ignores the command-word tear at f = 4132,
as before. `FE_LOOPS` is a measurement window: 3500 keeps 3099 (loop 3422)
inside the dump, and the 3840-loop probe dump gives the same first
unexplained frame and counts.

**3099, characterised.** Capture 3098 equals the splice of cycle-2 frames
1615/1616 (0 bytes). Capture 3099 differs from its best splice (1616/1617,
row 193) in 4854 bytes, rows 180..196. Frame 1617 is loop 3422 (f = 4309).
There the original's raptor enters 9/7/0 on stream `0xD311C` with `+0x57` =
0. The port's stays in 9/0/0 with `+0x57` = 3 and misses
`fn_resolve(0x15350)`: the callback of character 3's reaction `0x25` (the
dword at `0xA470C` reads `50 53 01 00 00 00 00 00`, the callback and no
stream word), which §39.4's probe had already missed at loops 3256
and 3343 before the grab was ported. On the diverged path it later misses
`0x151C0` (loop 3551). The grab's throw `0x14D7C` and stream target
`0x14E80` are still not reached.

**The staged next step.** Decode `0x15350` from `read_memory` with capstone
(`decompile_function 0x15350` finds no function), in the shape of `0x14E44`
(§40), and register it as the reaction-`0x25` callback. Then re-run the poll
comparison past f = 4309.

## 41-A. The `0x3A79C` family's pose handler `0x3A6D4` (named-gap batch, branch `gap-3a6d4`)

**Result in one line.** `0x3A6D4`, the per-frame handler the pose setter
`0x3A79C` stores in `slot+0x10`, is ported and registered. It is §2.3's
sibling of `0x3A43C` with its own stream table, glob pair and final `+0x90`
store, and it calls only ported functions. §40.5's probe missed it from loop
3295 on the diverged path; after §41's `0x3C32C` no oracle window reaches it,
so it is ported byte-faithfully and unit-tested only.

### 41-A.1 The raw (Ghidra `read_memory` + capstone, fixups applied)

Ghidra has no function at `0x3A6D4` (`disassemble_function` fails). The bytes
`0x3A6D4..0x3A798` (197 B) decode cleanly up to the `ret` at `0x3A798`;
`0x3A799` is the `lea eax,[eax]` padding and `0x3A79C` is the setter. The
body is `0x3A43C`'s instruction for instruction, except for four operands:

| site | `0x3A43C` | `0x3A6D4` |
|---|---|---|
| the stream table (`mov edx,[eax*4 + T]`) | `0x3A47E`: `0xC8FE0` | `0x3A716`: **`0xC9008`** |
| A (`mov ebx,[edx*2 + a-2]; sar ebx,16`) | `0x3A4AC`: `0x107D12` = word `0x107D14` | `0x3A744`: `0x107D06` = word **`0x107D08`** |
| B (`mov edx,[edx*2 + b-2]; sar edx,16`) | `0x3A4B3`: `0x107D0E` = word `0x107D10` | `0x3A74B`: `0x107D02` = word **`0x107D04`** |
| the jump table (`jmp cs:[eax*4 + J]`) | `0x3A4E0`: `0x3A42C` | `0x3A778`: `0x3A6C4` |
| the final `+0x90` store | `0x3A4F6`: 1 | `0x3A78E`: **3** |

- `sub esp,0x18; mov edx,ebx; mov eax,esp; call 0x33A10` (the ctx swap on
  EBX = side; the incoming EAX = slot is overwritten unread).
- `+0x58` (`0x3A6E4`): 0 stores 1 (`0x3A6FD`) and returns; above 1 returns
  (`0x3A6EB`/`0x3A6ED`); 1 runs the body.
- `push 3.0f; call 0x2BC30(ctx[5], 0xC9008[slot+0x7A], 3.0)`. `0x2BC30` ends
  `ret 4`, so from `0x3A726` `[esp+4]` = ctx[1] (the side) and
  `[esp+0x14]` = ctx[5] (rec_self), as §9 found for `0x3A43C`.
- `0x188AC(ctx[1], rec_self+0x18, 0)` (`0x3A726..0x3A733`), `+0x58` = 2
  (`0x3A73C`).
- B = (s16) word `0x107D04 + side*2`, A = (s16) word `0x107D08 + side*2`.
  When B is neither 0 (`0x3A75C`) nor 5 (`0x3A761`) and `(u8)(+0x90 - 1)`
  is above 3 (`0x3A771`, `ja`), `0x188DC(ctx[1], A)` (`0x3A785`). The four
  dwords at `0x3A6C4` all read `0x3A78A` (`read_memory`), so `+0x90` in
  1..4 skips the snap.
- `+0x90` = 3 (`0x3A78E`).
- These are the globs the setter writes: `0x3A7FB` stores the slot's `+0x2C`
  word at `0x107D08 + side*2` (A) and `0x3A803` stores BX at
  `0x107D04 + side*2` (B), matching §2.3's table.
- `0xC9008` (`read_memory`, 10 dwords, up to `0x3A588`'s table at
  `0xC9030`): `0xE7358`, `0xE3FD6`, `0xED0BE`, `0xD26A6`, `0xEAC02`,
  `0xD42C6`, `0xE0BE8`, then `0xED0BE`, `0xD26A6`, `0xEAC02` again. Every
  stream opens with two sprite words, then `D100 00039A34` (the ported and
  registered hold scaler, §2.4) and later `D500 00036870` (the ported and
  registered `0x36870`). The raptor's (char 3) is `0xD26A6`:
  `1886 1887 D100 9A34 0003 0008 1888 .. 188F D500 6870 0003 ...`.

**Callees.** `0x33A10` (`fighter_ctx_swap`), `0x2BC30`
(`actors_anim_begin`), `0x188AC` (`hit_anchor_set`), `0x188DC`
(`hit_anchor_x`), and through the streams `0x39A34`/`0x36870`: all ported
and registered. **No callee is new.**

### 41-A.2 Entrances

- `0x3A6D4`: one dword in the whole image, in the code object at `0x3A7D5`
  (the setter's `mov dword [ecx+0x10], 0x3A6D4` at `0x3A7D2`). None in the
  data object; no `call`/`jmp`/`jcc` rel32 to it (scan of both objects,
  `scratchpad/scan.py`). `get_xrefs_to 0x3A6D4`: one DATA reference, from
  `0x3A7D2`.
- The setter `0x3A79C`: one rel32 call, `0x3AD6B` in `0x3AAFC` (the `ecx & 8`
  arm, port `fighter_reaction_apply`); `get_xrefs_to` agrees.
- `0x3A6C4` (the jump table): one dword, at `0x3A77C`. `0xC9008`: one dword,
  at `0x3A719`; `get_xrefs_to` gives the same one DATA reference (`0x3A716`).
- So the handler runs only through `0x3531C` case 10's `slot+0x10` call
  (`0x354E2`, EAX = slot, EBX = side). It is registered in `actors_init`
  next to `0x3A43C`, and the case-10 resolve now finds it.

### 41-A.3 The assertions and mutations

`check_pose_handler_3a6d4` (`test_fight.c`, after `check_anim_hold_scaler`
so that `actors_init` has run) seeds character 3 on both slots, sentinels on
both records, the 0x3A79C pair zeroed and `0x3A43C`'s pair armed as a trap
(B = 3, A = `0x4321`). It saves and restores the slots, `0x107D00..0x107D2F`,
`0x100AB0..0x100AFF`, `DS_001078F6`, `DS_001014EC` and every global
`pose_chain_setup` writes. A temporary whole-data-object diff around the
check (reverted) was empty; dropping the slot restore made it report 16
bytes, so the diff could see a leak.
- Phase 0 sets `+0x58` = 1 and leaves `+0x90` and the stream; `+0x58` = 2
  returns untouched.
- Phase 1, B = 0: the stream `0xD26A6` (char 3; char 0 gives `0xE7358` and
  sprite `0x11D1`), the 3.0 hold, sprite `0x1886`, `+0x58` = 2, `+0x90` = 3,
  `rec+0x1C` = 0, no snap, the other record untouched.
- The snap: B = 3, A = `0x4321` gives `slot+0x2C` = A and `rec+0x18` =
  A - `DS_00100AB0[0]`; A = `0x8001` is sign-extended; `+0x90` = 1 and 4 skip
  it, 5 and 0 take it; B = 5 closes it; the other side's B/A do not open it.
- The side-1 mirror (EBX = 1): slot 1, record 1, pset 1, B[1]/A[1] and
  `DS_00100AB0[8]`; slot 0 untouched.
- The registration (`fn_resolve(0x3A6D4)` is `fighter_pose_3a6d4`) and the
  `0x3531C` case-10 call through `slot+0x10` = `0x3A6D4`.

**Mutations** (`scratchpad/mut.py`, `mut.log`): 19 single-site edits of the
new code: the phase-0 store, the phase-above-1 return, the table address,
the char index, the hold, the anchor y, the `+0x58` = 2 store, the `+0x90`
= 3 store, each glob address, each B gate, both edges of the jump-table
range, A's sign extension, each side index, the ctx-swap side and the
registration. All 19 fail (1..8 assertions each). The sources were restored
and checked with `cmp`.

### 41-A.4 Measured, and the gaps

- On the batch base (`38c4efc`, before §41's `0x3C32C`) the port reaches
  `0x3A6D4` from loop 3295 only on the diverged path after N = 2950's frame
  (loop 3293), so the first unexplained attract2 frame cannot move earlier;
  the counts past it may change. After §41 the 3840-loop probe does not
  reach it, so it should change no oracle. The oracles were not run here
  (the batch controller runs the ladder after merging).
- No gap remains inside `0x3A6D4`. Its siblings `0x3A588` (the `0x3A650`
  setter) and `0x3A820` (the `0x3A8E8` setter) are separate batch items.

## 41-B. The raptor's throw `0x14D7C`, the placement `0x3C208` and the stream target `0x14E80` (named-gap batch, branch `gap-3c208`)

**Result in one line.** The two named gaps §40.3 left in character 3's grab,
the `+0x1C` throw `0x14D7C` and the grab stream's second `0xD100` target
`0x14E80`, are ported and registered, with the placement `0x3C208` the throw
calls and its unported callees `0x18AF8` and `0x3B90C` (`0x3B8D8` was ported
in §30). None of them is reached in the port's run, so no oracle moves.

### 41-B.1 The raw (Ghidra `read_memory` + capstone, fixups applied)

Ghidra has functions at `0x3C208`, `0x14D7C` (`FUN_00014d7c`), `0x3B90C`
(`FUN_0003b90c`) and `0x18AF8` (`FUN_00018af8`), all in the checked-in
decomp export; the decompilations agree with the disassembly below. Only
`0x14E80` has no Ghidra function.

- **`0x3C208`** (292 bytes, `0x3C208..0x3C32B`; EAX = side, EDX = dist):
  - `0x186D0(0)`, `0x186D0(1)`, `0x18AF8()`. All three keep EDX (`0x186D0`
    and `0x18B04` push it), so the `test edx,edx` at `0x3C24D` tests the
    argument: EDI = |dist|.
  - `mov eax,[0x1077B0]` (the side-0 record, the slot's `+0`): word `+0x34`,
    bytes `+0x43`, `+0x42` = 0 (`0x3C22C..0x3C236`); the same for
    `[0x107844]` (`0x3C23F..0x3C249`). These are record fields, not slot
    fields.
  - ECX = 1 - side (`0x3C253`/`0x3C25D`). `0x187FC` is called once for the
    sign and once for the value (EAX = |d|; `0x187FC` and `0x186D0` keep ECX).
    ESI = |(|d| - EDI)|.
  - `cmp edx,edi; jl 0x3C29D` (signed). Not below: `0x1A570(side)` (EAX =
    EBX) non-zero gives `0x1883C(other, ESI, EBX = 0)` (`0x3C31B`), else
    `0x1883C(other, -ESI, 0)` (`0x3C290..0x3C298`).
  - Below (`0x3C29D`): EBP = other * `0x94`. With `0x1A570(side)` non-zero
    ESI is negated (`0x3C2B9`). `0x3B8D8(other, ESI)` = 0 goes to the
    `0x1883C(other, ESI, 0)` tail. Otherwise `0x188DC(other, 0x3B90C(other,
    ESI))` and `0x188DC(side, [EBP + 0x1077DC] ± EDI)`: `+` on the
    `0x1A570` arm (`0x3C2E2`), `-` on the other (`0x3C312`). `[EBP +
    0x1077DC]` is the other slot's `+0x2C`, read after the first `0x188DC`
    wrote it. `0x188DC` keeps EBX/ECX and does not touch EBP; `0x18714` keeps
    EBX/ECX/EDX/ESI/EDI.
- **`0x18AF8`** (12 bytes): `xor eax,eax; call 0x18B04; mov eax,1` and falls
  into `0x18B04`. So `0x18B04(0)`, `0x18B04(1)` (the port's
  `hit_facing_flag`).
- **`0x3B90C`** (44 bytes; EAX = side, EDX = delta): x = slot `+0x2C` +
  delta; W = `[0xBE018]`; `jge` gives W, else `neg edx`, `jg` gives x, else
  -W. Its twin `0x3B8D8` returns AL = 1 for the same x at or past either
  wall, so through `0x3C208` the clamp always returns ±W.
- **`0x14D7C`** (199 bytes, `0x14D7C..0x14E42`; EAX = side):
  - `0x33950(side)` context: `0x18B04(ctx[1])` (`0x14D90`) on the slots as
    they are (no latch first).
  - `mov eax,[ebx*2 + 0x107D2A]; sar eax,16; cmp eax,4; jl`: the side's
    `0x107D2C` word, signed, at least 4 sets `DS_001088BF` = 4.
  - `0x39A10(ctx[4], 0x309)`, `0x39A10(ctx[5], 0x309)`.
  - EDX = `[0x9AFA2 + 2 * byte ctx[3]+0x7A] sar 16`, the s16 at `0x9AFA4 +
    2c` (`read_memory 0x9AFA4`: `2D00 2D00 2D00 3840 2F80 2D00 29C0` for
    c = 0..6); `0x3C208(side, EDX)` (`0x14DDF`).
  - `0x39834(ctx[1], byte ctx[2]+0x5F)`.
  - `0x2BC30(ctx[5], [0xC91C0 + 4 * byte ctx[3]+0x7A], 3.0)` (`push
    0x40400000`, then `[esp+0x18]` = ctx[5]). The char byte is read again at
    `0x14E03`. `read_memory 0xC91C0`: `0xE756A 0xE41E0 0xED21E 0xD284C
    0xEAE36 0xD44E4 0xE0DDE`.
  - ctx[3] `+0x41 |= 0x80`, `+0x52` = 9, `+0x53` = 4, then the voice
    `0x2C3FC(0xB3)` (out of scope, spec §7).
- **`0x14E80`** (36 bytes; EAX = rec, EDX pushed and popped): `al = [eax +
  0x51]; xor al,1; and eax,0xff`, EAX = `DS_001077A8[that]`; 0 returns; else
  the slot's record `+0x55` = 1 and the slot's word `+0x74` = 0.

### 41-B.2 Entrances

A rel32 (`E8`/`E9`/`0F 8x`) and rel8 scan of the code object and a dword scan
of both objects (dumped through `read_memory`), cross-checked with
`get_xrefs_to`:

- `0x3C208`: 10 `call`s, `0x14DDF`, `0x21257`, `0x22466`, `0x225D4`,
  `0x3E2AF`, `0x401CC`, `0x44C0E`, `0x47ADB`, `0x47D90`, `0x48112`; no dword.
  Only `0x14DDF` is in ported code; `0x3E2AF` is in the unported `0x3E244`,
  the other eight in code the port does not have.
- `0x18AF8`: 10 `call`s (`0x21239`, `0x21B1D`, `0x21BFE`, `0x22595`,
  `0x3C222`, `0x3E2B4`, `0x3E9B9`, `0x44C13`, `0x479AC`, `0x47D95`), no
  dword. (`prage.functions.csv`'s "1 caller" undercounts.)
- `0x3B90C`: `0x3C2CC` and `0x3C2FC` only. `0x3B8D8`: `0x3BA4E`, `0x3BA85`
  (§30) and `0x3C2BF`, `0x3C2EF`.
- `0x14D7C`: no rel32; one dword, at `0x14E71` (the `0x14E44` store),
  `get_xrefs_to`: `0x14E6E` DATA.
- `0x14E80`: no rel32, no dword in the code object; one dword in the data
  object, `0xD3056` (`get_xrefs_to` is empty).

### 41-B.3 The port

- `fighter.c`: `fighter_3c208` (public), `fighter_3b90c` (public for its
  unit test), `fighter_18af8` (static) next to `fighter_3b8d8`;
  `fighter_14d7c` (public) after `fighter_14e44`. The table bases are local
  `#define`s (`FIGHT_DIST_14D7C` = `0x9AFA2`, the dword the raw reads;
  `FIGHT_ANIM_14D7C` = `0xC91C0`).
- `actors.c`: `anim_code_14E80`, and `fn_register` of `0x14D7C`
  (`fn(side)`, as `0x193B0`'s `0x19505` calls it) and `0x14E80` (the
  opcode-`0x11` `(rec, operand)` shape).
- `0x3E244` still needs the unported `0x3C358` (and is not reached); its
  `PORT:` note in `fighter_3e3a8` is updated.

### 41-B.4 The assertions and mutations

`check_throw_3c208` (`test_fight.c`, after `check_body_push`) saves and
restores the slots, `DS_001077A8`, `DS_00104B00`, `0x100AF0..0x100AF7`,
`DS_00100AB0..ABF`, `0xD3388..0xD33AB`, `0x107D20..0x107D33`,
`0x100B5A..0x100B5F`, `0x107ED8..0x107EE4`, `0x107D58..0x107ED7`, the
`c3_seed` globals, `DS_001088BF`, `DS_001078FA`, the `0xEAE36` stream head
and `mem[0..0x77]`. A temporary whole-data-object diff around it (reverted)
first found `0x100B5A/B` and `0x107D30`; after widening the saves it was
empty. `check_char3_grab`'s two "stays unregistered" checks become
registration checks.

- A..G: `0x3C208` on `bp_seed` positions (walls `0x7C00`): the far arm both
  ways and for side 1, the record clears and `0x18AF8`'s facing bits (and
  mode `0x22`), the magnitude of a negative dist, the `jl` at equality at the
  right wall, the closer arm both ways, and both wall clamps with the side's
  `0x188DC` placement.
- H: `0x3B90C` directly, including x = ±W and ±(W + 1).
- I: `0x14D7C(0)` for the raptor throwing character 4 (distance `0x2F80`,
  stream `0xEAE36` patched to a plain frame word): the other side's stale
  slot `+0x2C` shows that `0x18B04(1)` runs before any latch; the
  `0x1088BF` gate at 3, 4 and `0x8004` (signed); both `+0x74`; `0x39834`'s
  count; the other record's stream and 3.0 hold; 9/4 and `+0x41` bit 7.
- J: `0x14E80` both ways, and no other slot (a pointer planted at `mem[0]`
  is not followed).

**Mutations** (`scratchpad/g3c208/mut.py`, `mut.log`, `mut2.log`): 71
single-site edits over `0x3B90C` (6 + 1), `0x18AF8` (2), `0x3C208` (30),
`0x14D7C` (24), `0x14E80` (6) and the two registrations, each rebuilt and
run with `PR_ORACLE_REQUIRED=1`. The first run had 70; 69 failed at once (2..35
`FAIL` lines). The survivor moved `0x3B90C`'s `jge` to `x > W + 1`, which
differs from the raw only at x = W + 1; H now asserts ±(W + 1) (one past each
wall). The second run re-ran that mutation and `ret wall` and added its
mirror (`x > -W - 2`); all three fail. All 71 fail.
The sources were restored and the suite re-run green.

### 41-B.5 Measured and remaining gaps

- Build with 0 warnings; `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all
  checks pass. The oracle drivers were not run in this batch (shared
  `/tmp/pr_frontend_dump`); none of the new code is reached in the port's
  run (§40.3's probe: `0x14CC4` misses at f = 4020, and `0x14E80` lies past
  the loop the miss restart cuts), so the front-end, demo-fight and attract2
  lines are expected unchanged.
- Remaining named gaps: `0x3E244` (needs `0x3C358`). `0x3C208`'s other
  nine call sites (`0x3E2AF` in `0x3E244` among them) and `0x18AF8`'s other
  nine lie in code the port does not have (no `port/src` reference to any of
  those addresses or their functions). The `0x2C3FC(0xB3)` voice is out of
  scope.

## 41-D. The type-`0x0A`/`0x19` node lists `0x28E98` and the character-screen setup `0x43818` (named-gap batch, branch `gap-28e98`)

**Result in one line.** `0x28E98`, the call in state 6's reset `0x20DF4`
that builds the lists types `0x0A`/`0x19` pop (a named gap in §38.2), is
ported as `actor_type_0a19_list_init` and called at its raw position.
`0x43818`, the unported caller of `0x4F228` at `0x43822` (§36.3, §37.1), is
ported as `fight_char_screen_setup`. Its callers are unported `0x24C5C`-mode
code, so it is unit-tested only. No new callee was needed: `0x249C0`,
`0x4F228`, `0x2BAF4`, `0x2AE14` and `0x33754` were already ported, and
`0x29D60` is a bare `ret`.

### 41-D.1 `0x28E98` (Ghidra disassembly, fixups applied)

- `push ebx; push ecx; push edx`, then EDX = `0x104880`, ECX = `0x104888`,
  EBX = `0x104780`.
- `0x28EAA`/`0x28EB0`: `[0x104884]` = `[0x104880]` = `0x104880`.
  `0x28EB6`/`0x28EBC`: `[0x10488C]` = `[0x104888]` = `0x104888`. Both
  sentinels are self-linked.
- `0x28EC2 cmp ebx,0x104880; jnc` (never taken), then the loop
  `0x28ECA..0x28EDF`: EAX = `0x104888`, EDX = EBX, EBX += `0x10`,
  `call 0x249C0`, `cmp ebx,0x104880; jc`. That is 16 nodes,
  `0x104780..0x104870`, each inserted before the free sentinel `0x104888`
  (at the tail). So the free list runs in address order, and `0x28F64`/
  `0x2901C` pop `0x104780` first. `0x249C0` writes only the node's
  `{next; prev}`.
- `pop edx; pop ecx; pop ebx; ret`. The region `0x104780..0x10488F` is zero
  in the image (BSS).

**Entrances.** `get_xrefs_to 0x28E98` gives `0x20E3D` (in `0x20DF4`) and
`0x20EE4`. A rel32 scan of the code object finds the same two `call`s and no
`jmp`/`jcc`. No dword `0x00028E98` appears in the code or data object.
`0x20EE4` lies in a block at `0x20EB8` that Ghidra has no function for:
`push edx; xor edx,edx; xor ah,ah`, the four `0x20DF4` stores, the calls
`0x2C390`, `0x12750`, `0x49300`, `0x28E98`, `0x34978` and `0x2C074`, then
`pop edx; ret`. It is dead. No rel32 and no dword references `0x20EB8`, and
the `mov eax,eax` at `0x20EB6` is padding after the `ret` of the block at
`0x20E90`, whose one caller is `0x4256A`. `0x20DF4` has six callers
(`0x11AC4`, `0x25A95`, `0x25BE6`, `0x269BE`, `0x270E0`, `0x295E4`). Only
`0x11AC4` (state 6) is ported, so `game_state_6` is the one wiring site.

**The list users.** The type-`0x19`/`0x0A` cb1s `0x28F64`/`0x2901C` pop
the free list (type table `0xBB9DC`, entries `0x19` and `0x0A`), and the
teardown `0x290D0` returns the node. Process-table entry 7, `0x2910C`
(`DS_000A8644[7]`), walks the in-use list at `0x104880`; the cb1s set
`DS_00104AE8` bit 7 to enable it. `0x2910C` is not registered, so
`run_process_table` skips it. It stays a named gap, outside this batch
(since ported and registered, §42-A).

### 41-D.2 `0x43818` (Ghidra disassembly, fixups applied)

- `push ebx; push ecx; push edx`.
- `0x4381B xor eax,eax`, `0x4381D mov ecx,0xE0`, `0x43822 call 0x4F228`.
- `0x43827 mov eax,1`, `0x4382C xor ebx,ebx`, `0x4382E call 0x2BAF4`, then
  `0x43833 call 0x29D60` (a bare `ret`).
- `push 0; xor edx,edx; mov eax,0xC885C; call 0x2AE14` (`0x43841`). ECX =
  `0xE0` and EBX = 0 reach this call intact. `0x4F228` never writes ECX,
  and `0x2BAF4` pushes and pops EBX, ECX and EDX (`0x2BAF4..0x2BAF6`,
  `0x2BC2C..0x2BC2E`). `0x2BAF4` loads EBX at `0x2BB05`, after the push,
  so EBX = 0 is not an argument to it.
- `push 0; ecx = 0xE2; ebx = 0x3900; edx = 0x1500; eax = 0xC87F8; call
  0x2AE14` (`0x4385C`). Then `push 0; ecx = 0xE2; ebx = 0x3900; edx =
  0x3F00; mov [0x10814C],eax; eax = 0xC87F8; call 0x2AE14` (`0x4387C`) and
  `mov [0x108150],eax` (`0x43881`). `0x2AE14` ends `ret 4` (`0x2B14A`), so
  each pushed 0 is its a5. The port maps a2 = EDX, a3 = ECX and a4 = EBX,
  so the spawns are `(0xC885C, 0, 0xE0, 0, 0)`, `(0xC87F8, 0x1500, 0xE2,
  0x3900, 0)` and `(0xC87F8, 0x3F00, 0xE2, 0x3900, 0)`.
- `0x33754` with EAX = `0x98EC50C`, `0x98EC514`, `0x8099AC`, `0x809984`
  (`0x4388B..0x438A9`).
- `pop edx; pop ecx; pop ebx; ret`. The last acquire's EAX is dead: both
  callers overwrite EAX next (`0x4377F lea eax,[edx+1]`, `0x4450F mov
  eax,edx`).
- The descriptors (data object):
  - `0xC885C`: desc[0] = `000046F8`, type 0, word `+8` = `0x2800`, palette
    `+0x10` = `0x098EC71C`.
  - `0xC87F8`: desc[0] = `0000032A`, type 0, `0x2800`, the same palette.
  - Flag `0x0800` skips `0x2AE14`'s stream walk, and `0x2000` puts a3 in
    `rec+0x49`.

**Entrances.** `get_xrefs_to 0x43818` gives `0x43778` (in `0x43738`) and
`0x44508` (in `0x444C8`). The rel32 scan finds the same two calls, and no
dword `0x00043818` appears in either object. None of the callers is ported:
- `0x43738` is called at `0x4372E`. `0x28D68`/`0x28D80` store it as the
  `DS_00104AE4` frame hook (immediates at `0x28D6A`/`0x28D87`).
- Immediates in the code around `0x42CF3` store `0x28D68` (dwords at
  `0x42CF4`, `0x42D35`, `0x42D7D` and `0x42FB7`), and `0x28E4F` stores
  `0x28D80`.
- `0x444C8` is called only at `0x4462B` (in `0x4454C`).

This is `0x24C5C`-mode code: the per-frame `call [0x104AE4]` hook is the
unported mode-0x17 handler (`flow.c`), so no oracle reaches `0x43818`. After
it, both callers fill the two sides through `0x43964`. That function spawns
each side's entries from `0xC8870`/`0xC8878[side]` with the character index
byte of `0x108163[side]` and the character palette (`0x2A17C`), hence the
port's name.

**The `0x43822` call has no effect.** `0x2BAF4` calls `0x4F228` again at
`0x2BBC4` with EAX = 0 (`0x2BBC0`). Between the two calls run only
`0x2EA30`, `0x13DF0`, `0x61A70`, `0x249C0` and `0x13ADC`. Their transitive
callees (`0x13420`, `0x249B0/C0/D0`, `0x33714`, `0x33734`, `0x65490`,
`0x654C7`) do not touch the four targets. The code object references
`0x107A54/55/3A/38` only in `0x121A0`, `0x12484`, `0x14328`, `0x255CC`,
`0x387xx..0x38Bxx` and `0x4F1xx..0x4F2xx`. So the `0x43822` writes are
always overwritten before anything reads them. The port still makes the
call, at its raw position.

### 41-D.3 The fix

- `actors.c`: `actor_type_0a19_list_init` (`0x28E98`), with the file's
  `list_insert_before` (`0x249C0`).
- `flow.c`: `game_state_6` calls it after `fight_list_init` (`0x49300`)
  and before `fighter_slots_reset` (`0x34978`), in the raw order
  (`0x20E38`, `0x20E3D`, `0x20E42`). The `PORT:` note now lists what
  remains: `0x29B70`, `0x2C390`, `0x2C074` and the stores.
- `fight.c`: `fight_char_screen_setup` (`0x43818`), which calls
  `render_projection_reset`, `actors_reset`, `actor_spawn` and
  `palette_acquire`. No port code calls it yet.
- `render.c`: the `0x4F228` comment named only one call site. It now names
  all three (`0x2BBC4`, `0x20C49`, `0x43822`), each with EAX = 0.
- `game_flow.md`: the `0x20DF4` paragraph says which resets are ported.

That is 2 functions, `0x4D` + `0x9A` raw bytes, and no new registration
(neither address is stored in data).

### 41-D.4 The assertions and mutations

- `check_type_0a19_list_init` (`test_fight.c`, after `check_list_init`)
  fills `0x104770..0x10489F` with `0xA5`, and saves and restores it. It
  asserts:
  - both sentinels, and the free list's next and prev (`0x104780`/
    `0x104870`);
  - each node's next and prev;
  - the nodes' `+8`/`+0xC` and the 0x10-byte margins keep the `0xA5`.
- `check_state6` fills `0x104780..0x10488F` with `0xA5` before the step,
  then asserts the two sentinels' four links. The `test_fight` harness
  snapshot `s_4880` now spans `0x104780..0x10488F` (it held only the
  sentinels), because state 6 now writes the nodes. These assertions pass,
  so the unit state-6 step pops no type-`0x0A`/`0x19` node.
- `check_char_screen_setup` runs last in `test_fight`, after the harness
  restores, because the earlier fixtures replace the resource table.
  - It saves and restores the data object, both pools, both offscreen
    buffers, the resource table (a first resolve sets an entry's loaded
    flag), the DAC and the aperture.
  - Seeds: two live records in the pool, `DS_0010814C`/`150` =
    `0xDEADBEEF`, `DS_00104AE8` = all ones, a fifth palette entry's handle,
    and the projection sentinels.
  - The projection's four targets and `DS_00104AE8` read 0.
  - Exactly three records are active, newest first, at base, base + `0x68`
    and base + `0xD0`. `DS_0010814C`/`150` hold base + `0x68`/`0xD0`.
  - Each record's `+8` (desc[0], read from the data), `+0x18`, `+0x1C`
    and `+0x49` (`0xE0`, `0xE2`, `0xE2`).
  - The palette table. Entry 0 is the descriptors' handle (read from
    `+0x10`), with refcount 3 and start 1. Entries 1..4 are the four
    literals in order, each with refcount 1, start = the previous start +
    len, and len = the resolved count. Entry 5 reads 0. Measured lens are
    99, 1, 1, 7, 1 and starts 1, 100, 101, 102, 109.

**Mutations** (`scratchpad/g/mut/mut.py`): 20 single-site edits.
- `0x28E98`:
  - insert-after for insert-before: 36 failures;
  - stride `0xC`: 56;
  - bound `0x104870`: 5;
  - dropping the `0x104880` or the `0x104884` store: 2 each;
  - dropping the `0x10488C` store: the run crashes (SIGSEGV; the first
    `0x249C0` follows the `0xA5` prev), which fails it.
- `flow.c`: dropping the call fails `check_state6` (4).
- `0x43818`:
  - dropping `actors_reset`: 16;
  - the first spawn's a3 `0xE0` -> `0xE2`: 1;
  - its a4 0 -> `0x3900`: 1;
  - the second spawn's a3/a4 swapped: 2;
  - the third spawn's x `0x3F00` -> `0x1500`: 1;
  - dropping the `0x10814C` store: 1;
  - storing the third spawn to `0x10814C`: 2;
  - either wrong descriptor: 1 each;
  - the first two acquires swapped: 10;
  - the last acquire dropped: 4.
- Two edits survive: `render_projection_reset(1)` and dropping the
  `0x43822` call. Both are equivalent, because `0x2BBC4` overwrites the four
  targets before anything reads them (§41-D.2). All sources were restored
  and checked.

### 41-D.5 Oracles and remaining gaps

No driver reaches `0x43818`. `0x28E98` now runs in both demos' state 6.
Until now the port's `0x104888` list was zero, so it refused every
type-`0x0A`/`0x19` spawn: the record died with no RNG draw. After a state 6
the raw accepts such a spawn and draws twice (`rng(0x20)`/`rng(0x80)`, or
`rng(0x80)` twice). Before its first state 6 the raw refuses as well
(review: `0x28F64` on the zero list returns -1). §38.2's "none is spawned in
the measured windows" cites no evidence, so this section does not rely on
it. The evidence is below.

**Which paths spawn these types** (dword and rel32 scans, fixups applied).
The type table is `{desc; cb1; cb2}` at `0xBB9D8`. Entry `0x0A`'s
descriptor is `0xBB0C4` (dword at `0xBBA50`; its byte 4 is `0x0A`), and
entry `0x19`'s is `0xA89AC` (`0xBBB04`; byte 4 `0x19`).
- **Type `0x19`.** `0xA89AC` is referenced only by the type table and by
  two code immediates: `0x28F54` in `0x28F08`, and `0x4912D` in `0x48F98`
  (reached through `0x490A1 jle 0x490F9`). `0x48F98` and `0x28F08` are
  process-table entries 1 and 10 (the dwords at `0xA8648`/`0xA866C`, their
  only references). Neither is registered in the port, so the port cannot
  spawn type `0x19` (both since ported and registered, §46-D).
- **The scene tables.** The crowd table's `e+0xA` indices over all eight
  scenes (`0xBBD98`/`0xBBDA8`) are `7, 0xB..0xD, 0xF, 0x11, 0x12, 0x14,
  0x16..0x18, 0x1B..0x1F, 0x29, 0x2A, 0x2E, 0x2F`. Their descriptors' type
  bytes are the same values. The `0xC82CC` prop descriptors are all type 0.
  Neither uses `0x0A` or `0x19`.
- **Type `0x0A`.** `0xBB0C4` is referenced by the type table and three
  times by the stream at `0xE8CCC`: `CC00 000BB0C4 0000 0000` at
  `0xE8CDA`/`0xE8CE6`/`0xE8CF2` (dwords `0xE8CDC`/`0xE8CE8`/`0xE8CF4`).
  The reaction table at `0xA3528` references that stream 32 times
  (`0xA3544..0xA4F5C`). So a fighter reaction in a demo fight can spawn a
  type-`0x0A` actor.

**Measured** (a temporary `fprintf` in the two cb1s and the teardown
`0x290D0`, reverted). The front-end driver was dumped to a scratch directory,
not `/tmp/pr_frontend_dump`, with no other test or verify process running.
- **This branch** (base `38c4efc`, `FE_LOOPS` 3300): two type-`0x0A` cb1
  calls in state 7 of the second demo, at f = 4182 and f = 4185. Both
  were accepted (heads `0x104780`, then `0x104790`), and `0x290D0` never
  ran. f = 4182 is after 2950's f = 4180, on the port's diverged path
  (§40.5). The oracles are unchanged from §40.5: front-end 2 allowed, no
  other unexplained frame; the demo-fight window is empty; attract2
  684/369/7/666/6 with first unexplained frame 2950.
- **`main` at `48611ea`** (`FE_LOOPS` 3500, with §41's `0x3C32C`, `0x3A6D4`
  and the throw): no type-`0x0A`/`0x19` cb1 call in the whole driver run,
  so the raptor's return to its stance removed that path. With this
  branch's code commit cherry-picked onto it (a throwaway branch, deleted),
  the driver still makes no such call. The dump (top level and `cycle2/`)
  is byte-identical to `main`'s, and the oracles are unchanged: front-end
  2 allowed; demo-fight fully explained; attract2 771/430/8/517/6 with first
  unexplained frame 3099 = N.

So `0x28E98` moves no oracle at the current pins. A later window that
reaches the `0xE8CCC` reaction will now spawn the type-`0x0A` actors, as
the raw does.

**The unregistered `0x2910C` limits that.** `0x2910C` (process entry 7,
`DS_000A8644[7]`, which the cb1s enable with `DS_00104AE8` bit 7) walks
the in-use list:
- For each node whose actor has `rec+0x1C` + the signed word `rec+0x36`
  < 0 (landed; `mov edx,[eax+0x34]; sar edx,16; add edx,[eax+0x1C]`,
  `0x29126..0x29135`), it zeroes `+0x1C`, `+0x34`, `+0x36` and `+0x44`, sets
  the type `+0x48` = 0 and `+0x59` = `0xFE`, starts stream `0xE8D50` at
  3.0, and returns the node to `0x104888` (`0x249D0`, then `0x249B0`,
  `0x29187..0x2919B`).
- Otherwise it picks a pose stream from the velocities by the node's `+0xC`
  phase (`0xE8D14`, `0xE8D28`, ...; jump table `0x290FC`).

In the port an accepted actor keeps its spawn stream and gets neither the
landing nor the pose changes. Its node goes back only if the record is
released while still type `0x0A`/`0x19` (the teardown `0x290D0`). If that
never happens, after 16 accepted spawns the list is empty and the port
refuses again, while the raw, which recycles each node on landing, keeps
accepting (and drawing). That would be a new RNG divergence. The fix is to
port `0x2910C` (done in §42-A).

**Named gaps.**
- The type-`0x19` spawners `0x48F98`/`0x28F08` (process entries 1/10) are
  unregistered (since ported and registered, §46-D). (`0x2910C`, process
  entry 7, was too; §42-A ports and registers it.)
- In `0x20DF4`: the calls `0x29B70`, `0x2C390` and `0x2C074`, the five zero
  stores (dword `[0xF0A48]` at `0x20DFB`, dword `[0x100B4C]`, dword
  `[0x104AE8]`, byte `[0x1088EC]`, byte `[0x104B15]` at
  `0x20E16..0x20E28`), and the two word stores `DS_000F0AFA`/`DS_000F0AF8`.
  Only the two word stores are BSS-zero (net-faithful). `[0x104AE8]` is not
  zero at the second demo's state 6. `flow.c` and `game_flow.md` list them
  all.
- `0x43738`, `0x444C8`, `0x43964`, `0x43A08` and the `0x104AE4` hook chain
  (`0x28D68`, `0x28D80`) are unported mode code.

## 41-C. The projectile freeze `0x235C4` and the stream target `0x370F0` (named-gap batch, branch `gap-235c4`)

**Result in one line.** The two named gaps `0x235C4` (`0x3B464`'s projectile
`+0x48` == 8 arm, record §26) and `0x370F0` (the unregistered stream target
§20.4 saw once at f = 291) are ported from the raw with their unported
callees `0x33ACC`, `0x22B28`, `0x22BEC` and `0x29D04`. The demo reaches none
of them, so they are unit-tested; no oracle is expected to move.

### 41-C.1 The raw (Ghidra decompile/disassembly, `read_memory` + capstone, fixups applied)

Ghidra has functions at `0x235C4`, `0x22B28`, `0x33ACC` and `0x370F0`, and none
at `0x22BEC` or `0x29D04`; those two were decoded with capstone from
`read_memory` (`scratchpad/g235/d22bec.txt`, `d29d04.txt`).

- **`0x235C4`** (151 bytes; EAX = side; `push ebx; push edx`, EDX then
  overwritten): the `0x33A10` context on the stack (`mov edx,eax; mov
  eax,esp`). EBX = `0x104658 + side * 0x68` (`0x235D6..0x235EC`: `(side * 3
  * 4 + side) * 8`), EDX = `0x104530 + side * 0x94` (`0x235EE..0x23604`),
  `0x33ACC(side)` (`0x2360A`). Then `0x39834(side, EDX = 0x2A)`
  (`0x2360F..0x23618`), and on ctx[3] (`[esp+0xC]`, &slot[side]): `+0x52` =
  0x10, `+0x53` = 0x0A, `+0x10` = `0x22BEC`, `+0x18` = 0, `+0x1C` = 0
  (`0x23621..0x23647`). Last, `0x22B28(EAX = &ctx)` (`0x23650`).
- **`0x33ACC`** (51 bytes; EAX = side, EDX = dst, EBX = dst2): `rep movsd`
  of 0x25 dwords from `0x1077B0 + side * 0x94` to EDX, then 0x1A dwords from
  `[0x1077B0 + side * 0x94]` (the slot's record) to EBX. Ghidra's decompile
  shows the EBX use as `unaff_EBX`.
- **`0x22B28`** (179 bytes; EAX = &ctx, kept in ECX; `0x13C70`, `0x3C16C`,
  `0x3C148` and `0x2C3FC` all preserve ECX):
  - `0x13C70(EAX = [DS_001014EC + word[rec + 0x56] * 0x20 + 0x18], DL = 1,
    EBX = 0x105FDB0)` (`0x22B2D..0x22B67`), rec = `[0x1077B0 + ctx[1] *
    0x94]`. The bytes at `0x22B59` read `bb b0 fd 05 01`: an immediate, a
    resource handle (index 2, offset `0x5FDB0`), not a fixup.
  - ctx[3]'s `+0x14` = `0x29D04` (`0x22B6F`); word `0x10474C[ctx[1]]` = 0
    (`0x22B7B`); `0x3C16C(ctx[1])`, `0x3C148(ctx[1])`; ctx[5]'s `+0x24` = 0
    (`0x22B96`); ctx[3]'s `+0x58` = 1, `+0x43 &= 0xFD` (`0x22BA0/0x22BA7`);
    the voice `0x2C3FC(0xB5)` (`0x22BB0`).
  - `0x10476C[ctx[1]]` = 1 when ctx[2]'s (the other slot's) `+0x53` is 0x0A,
    else 0 (`0x22BB5..0x22BD1`).
- **`0x22BEC`** (247 bytes; `push ecx; sub esp,0x1C; mov edx,ebx; mov
  eax,esp; call 0x33A10`: only EBX = side is read; `0x3531C` case 10 calls
  it at `0x354E2` with EAX = ECX = slot):
  - word `0x10474C[side]` += 1 (`0x22BFF`), then the switch on ctx[3]'s
    `+0x58` (`cmp dl,3; ja 0x22CDE`; table `0x22BDC` reads `0x22CDE`,
    `0x22C24`, `0x22C5C`, `0x22CDE`).
  - Phase 1 (`0x22C24`): a second `+= 1` when `0x10476C[side]` != 0
    (EAX still holds side * 2), then `mov eax,[side*2 + 0x10474A]; sar
    eax,16; cmp eax,0x78; jle`: when the signed word exceeds 0x78, `+0x58` =
    2.
  - Phase 2 (`0x22C5C`): `0x35050(side)`; AL = (ctx[3]'s `+0x14` == 0), kept
    at `[esp+0x18]`; ECX = ctx[3]'s `+0x2C` (`0x22CB0`, before the call);
    `0x33B00(side, 0x104530 + side * 0x94, EBX = 0x104658 + side * 0x68)`
    (`0x22CB3`; `0x33B00` pushes ECX); `0x188DC(side, EDX = ECX)`;
    `0x39280(side)`; and, when the saved flag is 0 (`+0x14` still set after
    `0x35050`), `+0x14` = `0x29D04` (`0x22CD7`).
- **`0x29D04`** (93 bytes; EAX = slot): `cmp byte [0x9AF3D],0; je` else
  `xor eax,eax; ret`. Then EBX = `[[slot] + 0x51]` (the side), CL = slot
  `+0x7A`, EDX = EBX ^ EBX = 0 then DL = `0x105B34[side]`, ECX =
  `[0xA8A98 + char * 4]`, EAX = `[0x1077B0 + side * 0x94]`, EBX =
  `[ECX + EDX * 4]`, EDX = 0, `call 0x2A17C`, `mov eax,1`. It is `0x29BC8`'s
  lookup inlined, gated on no live palette effect (`DS_0009AF3D` is
  `0x13C70`'s count).
- **`0x370F0`** (118 bytes; EAX = rec, EDX pushed, DL loaded at `0x37105`
  and masked at `0x3710B` before use): self = `DS_001077A8[rec + 0x51]`,
  other = `DS_001077A8[(rec + 0x51) ^ 1]`; either 0 returns. Self `+0x54`
  = 3, `+0x42 |= 4`, `+0x52` = 0x0A. With `DS_00104B14` != 0 (CH), `0x2BC30(
  rec, [0xBDC2C + self+0x7A * 4], 1.0)` and return. Else other `+0x42 |=
  0x40`, `mov [ebx+0x53],ch` with CH = 0 (the RECORD's `+0x53`, EBX = rec;
  Ghidra's decompile agrees), `DS_000F0AFE` = 2.
- `0x39834`, `0x3C16C`, `0x3C148`, `0x13C70`, `0x35050`, `0x33B00`,
  `0x188DC`, `0x39280`, `0x2A17C` and `0x2BC30` were already ported; the
  voice `0x2C3FC(0xB5)` stays a `PORT:` stub (spec §7).

### 41-C.2 Entrances (`get_xrefs_to`, a rel32 scan and a dword scan of both objects)

- `0x235C4`: one rel32 `call` at `0x3B65E` (`0x3B464`, ECX = side); no dword.
- `0x22B28`: `call`s at `0x23650` (`0x235C4`) and `0x22D78` (`0x22CE4`).
- `0x33ACC`: `call`s at `0x2360A`, `0x22D2D` (`0x22CE4`) and `0x27272`
  (`0x27254`).
- `0x22BEC`: no rel32. Code dwords at `0x22D67` and `0x23634` (the `+0x10`
  stores of `0x22CE4` and `0x235C4`) and at `0x22DDE`, `0x23094`, `0x2368D`,
  `0x468AC`, `0x468EF` (`cmp [reg+0x10],0x22BEC` identity tests; the port's
  `0x46898`/`0x468D8` already test it). None in the data object. The code
  object's only `call [reg+0x10]` is `0x354E2` (`0x3531C` case 10), so it runs
  only through case 10.
- `0x29D04`: no rel32; code dwords at `0x22B72` and `0x22CDA` only (the two
  `+0x14` stores above), none in data.
- `0x370F0`: rel32 `call`s at `0x48BC3` (`0x48AAC`) and `0x48F36`
  (`0x48D94`), both unported; 21 data dwords, each the operand of a `D000`
  or `D100` stream word: `0xD2B20`/`0xD2BF4`/`0xD2C10`, `0xD47A6`/`0xD4876`/
  `0xD4892`, `0xE10DA`/`0xE11A6`/`0xE11C2`, `0xE44A0`/`0xE4570`/`0xE458C`,
  `0xE780C`/`0xE793C`/`0xE7958`, `0xEB0EA`/`0xEB1B0`/`0xEB1CC`,
  `0xED4BA`/`0xED5B4`/`0xED5D0`. The `0xBDC2C` table it indexes reads
  `0xE7914 0xE4548 0xED58C 0xD2BCC 0xEB188 0xD484E 0xE117E 0xBB150`, and
  each character's second and third sites follow its own `0xBDC2C` stream.
- Related, not callees and not ported (all three since ported, §42-A):
  `0x22CE4` (a second setter of the
  same freeze, called at `0x22E64`: `0x33ACC`, `0x39834(side, the other
  slot's +0x5F)`, the other slot's `+0x57` = 2, 0x10/0x0A/`0x22BEC`, its own
  `+0x5F` = 0xFF, `0x22B28`, and byte `0x10476A + (side ^ 1)` = 1), the
  `+0x18` hook `0x22D8C` (stored at
  `0x22F87`), and the reaction callback `0x2365C` (`*(u32*)0xA3D70`,
  character 1's reaction 0x2A), which returns 0 while the other slot's
  `+0x10` is `0x22BEC`.

### 41-C.3 The port

- `fighter.c`: `fighter_33acc`, `fighter_22b28` (static) and the exported
  `fighter_235c4`, `fighter_22bec`, `fighter_29d04`, `fighter_370f0`.
  `fighter_think_side` calls `fighter_235c4(side)` after `0x1922C` in place
  of the `PORT:` gap. Local `#define`s name `0x104530`, `0x104658`, the
  handle `0x105FDB0` and `0xBDC2C` (no `symbols.h` names).
- `actors.c`: `fn_register` of `0x22BEC` (the case-10 `(slot, side)` shape,
  as `0x39CC8`), `0x29D04` (the `+0x14` `fn(slot)` shape) and `0x370F0`
  through the wrapper `anim_code_370F0(rec, arg)`, which drops the operand.

### 41-C.4 The assertions and mutations

`check_freeze_235c4` (`test_fight.c`, after `check_char3_grab`; 106
assertions) runs on its own pool records `r0`/`r1` (`DS_001014F4` = r0, so
`0x2A17C` finds the psets), with every written field on a sentinel, the
snapshot area on `0xEE`, `DS_001014F0` = 0 (`res_resolve` returns NULL) and a
crafted effect free list. It saves and restores the slots, `DS_001077A8`,
`0x104530..0x104727`, `0x10474C`, `0x10476C`, `0xFCCE0..0xFCCEF`,
`0x9AF3C/3D`, the palette table `0x107618..0x10779B`, `0xA8A98[0..2]`,
`0x105B34`, `0x107D20..0x107EE7` (with `tf_hit_fixture`'s `0x107D58`
arrays and `0x107ED8/EDC/EE4`), `0x104B00`, `0x100AF0..0x100AF7`,
`0x100B5A..0x100B5F`, `DS_001088E0/E2`, `DS_001078FA`,
`DS_001014EC/F0/F4`, `0xBDC2C[2]`, `DS_001078F6`, `DS_00104B14` and
`DS_000F0AFE`. `fz_seed` sets `DS_00104B14` = 1, so `0x39834`'s tail gate
`0x399AC` never runs `0x4F434` whatever `DS_00104ABC` holds. A temporary
whole-data-object diff around it (reverted) first found `0x100AF4`
(`0x188DC`'s `0x18714` latch). After adding it, and with non-zero sentinels
planted beforehand in every `tf_hit_fixture` global above, in
`DS_00104B14` (0) and in `DS_00104ABC` (1), the diff was empty. It was not
empty when the `0x107D20` restore was cut back to 0x40 bytes.
- A/A2: `0x235C4` for side 1 and side 0: both snapshot halves byte-equal to
  the pre-call slot and record, the other side's halves still `0xEE`,
  `DS_00107D28` = 0x2A (`0x39834`'s `0x39953` store), 0x10/0x0A, `+0x10`,
  `+0x14`, `+0x18`/`+0x1C`, `+0x58`, `+0x43`, the record's `+0x24`/`+0x34`/
  `+0x36`/`+0x42`/`+0x43`/`+0x44`, both per-side words and bytes, the
  spawned effect (its source is side 1's pset `+0x18`, byte 1,
  `DS_0009AF3D` + 1), and no spawn on an empty free list.
- B..B5: `0x22BEC` through `0x3531C` case 10 (phase 0 ticks only), phase 1's
  0x78 edge, the signed 0x7FFF -> 0x8000 hold, the double tick and its side,
  phases 3 and 5, and phase 2: the restore from side 1's half (side 0's is
  marked `+0x52` = 0x0C), the live x 0x4000 kept over the snapshot's
  0x1000, `+0x5A` kept, `+0x5D` cleared, and `+0x14` re-armed only while
  `0x29D04` returns 0.
- C: `0x29D04`: a live effect returns 0 with the pset untouched; otherwise
  the `0xA8A98[2]` row's entry `0x105B34[1]` (a handle palette entry 0
  holds) replaces the pset's old entry (released) and 1 is returned.
- D..D3: `0x370F0` through its registered target: each missing
  `DS_001077A8` slot returns; with `DS_00104B14` = 0 the other slot's bit 6,
  the record's (not the slot's) `+0x53` and `DS_000F0AFE` = 2; with it set,
  `0xBDC2C[slot char 2]` (patched to a one-word stream) at 1.0.
- `check_projectile_step` A (`pc_seed`'s `+0x48` = 8) now also asserts the
  struck side's 0x10/0x0A, `+0x10` = `0x22BEC` and `+0x58` = 1. It seeds the
  raw's empty effect free list, a self-linked head `DS_000FCCE8` (the
  `0x13C7F`/`0x13C85` compare, so `0x13C70` never reads `pc_seed`'s zero pset
  entry; a zero head is only the port's unbuilt-pool guard), restores the
  head, and saves/restores the snapshot area and the two per-side arrays; a
  whole-data-object diff around it is the same before and after the wiring.

**Mutations** (`scratchpad/g235/mut.py`, `mut.log`): 59 single-site edits of
the new code, its registrations and the `0x3B65E` call (copy sizes and
sources, each side index, each constant, each dropped store or call, the
signedness and edge of the 0x78 test, the phase table, the palette gate and
lookup, the `0x370F0` gates and targets, the wrapper's record). The first
sweep killed all 59, but 7 only by a crash (a restore from the `0xEE`
snapshot, a zero effect source, a call through the unregistered target), so
the test gained valid snapshot images (`fz_snap_live`), a second pset source
at `+0x1C` and NULL-guarded target calls. On the re-run all 59 fail 1..18
assertions and none crashes. The script lists a sixtieth entry that matches
no line (logged `SKIP`, never applied). The sources were restored by the
script.

### 41-C.5 Measured and remaining gaps

- `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all checks passed, 0 compiler
  warnings. The drivers and `make verify` were not run (the batch controller
  runs them after the merge).
- No oracle is expected to move. The demo never reaches `0x3B464` with a
  `+0x48` == 8 projectile (record §26). The last whole-run `fn_resolve`
  probes (§35, §40) no longer reach `0x370F0`. The controller's ladder is
  the check.
- Remaining named gaps: the voice `0x2C3FC(0xB5)` in `0x22B28` (spec §7); the
  unported siblings `0x22CE4`, `0x22D8C` and `0x2365C` and the other
  `0x370F0` callers `0x48AAC`/`0x48D94`.

## 42-E. The front-end darken sites `0x29B74`/`0x41578`, the audit close `0x32A3C`, and the stub `0x5D812` (named-gap batch 2, branch `gap2-frontend`)

**Result in one line.** `0x29B74` is ported as `frontend_darken_all` and
`0x41578` as `frontend_darken_marked` (both `flow.c`). Their one unported
callee with state, `0x32A3C`, is ported as `config_play_time_close`
(`config.c`). `0x29B74` is registered in `actors_init` because it is a
`DS_00104AE4` pointer. No oracle path reaches any of them: every caller is
unported `0x24C5C`-mode code, so they are unit-tested only. The stub
`0x5D812` is `xor eax,eax; ret`. It stays unregistered, and every holder of
it in the image is now accounted for (§42-E.4). One old claim is corrected:
`0x41578`'s `0x88874B0` compare is live, not dead (§42-E.2).

### 42-E.1 `0x29B74` (Ghidra disassembly, fixups applied)

- `push ebx; push ecx; push edx`, then `0x29B77 call 0x13DF0` (effects
  clear). EAX is never read: the walk starts with `0x29B7C xor eax,eax`.
- `0x29B7E call 0x33904`. EBX holds the cursor. The loop `0x29B89..0x29BA0`
  runs `0x13D4C(EAX = entry, EDX = 3)` for **every** live entry, with no
  predicate, then `0x33904(entry)`.
- `0x29BAC`/`0x29BB3`: word `[0x1088EE]` = word `[0x104AFE]` = `0x78`.
  `0x29BBA`: word `[0x104B00]` = `0x15`. Then `pop edx/ecx/ebx; ret`.
- **Entrances.** `get_xrefs_to` and a rel32 scan agree: one `call` at
  `0x27B17` (in `0x27A2C`), plus five dword immediates `74 9b 02 00` in code,
  at `0x2788C` (in `0x277C0`), `0x2861F` (the dead `0x2861C` region, which has
  no rel32 and no dword entrance), and `0x28979`/`0x28A9E`/`0x28B3D` (in
  `0x28788`). Each one is stored into `DS_00104AE4`. There is no dword in the
  data object. `DS_00104AE4` is called by `call [0x104ae4]` at `0x4F302`,
  `0x4F373`, `0x4F6F1`, `0x4F70D`, `0x4F9AA` and `0x4F9D1`. Those sites are
  in `0x4F2B0`, `0x4F318`, `0x4F6E8`, `0x4F704`, `0x4F9A0` and `0x4F9C8`,
  which `0x24C5C` calls at `0x253E7..0x2540A` (and `0x4F318` also at
  `0x425E5`). Some callers let the handler's EAX escape. `0x4F373` (`pop
  edx; pop ebx; ret`) and `0x4F70D` (`ret`) return straight to `0x24C5C`, and
  `0x4F9B0` clears only AH. The C type `void(void)` still holds, because both
  values the code stores into `DS_00104AE4` return EAX = 0. `0x29B74` leaves
  its loop only when `test eax,eax` is zero, and its tail never writes EAX.
  `0x5D812` is `xor eax,eax`. So no consumer can see a difference.

### 42-E.2 `0x41578` and the handle correction

- `push ebx/ecx/edx/esi`. `0x33904(0)`, then the loop `0x41589..0x415B2`:
  `edx = [entry]`, `cmp edx,0x3e688; je`, `cmp edx,0x88874b0; jne skip`, then
  `0x13D4C(entry, 2)`, and `0x33904(entry)`. There is no effects clear.
- `ECX = 0x13`, `ESI = 0x15`, `EDX = byte [0x104B19]`, `EAX = [0x104ABC]`,
  `EBX = 0`, `call 0x32A3C` (`0x415CD`). Then `0x2C3FC(0x33, EDX = 0x78)`
  (`0x415DC`, the voice, out of scope, spec §7).
- Stores: word `[0x104AFE]` = DX (`0x78`), word `[0x1088EE]` = BX (0), word
  `[0x104AFA]` = CX (`0x13`), `xor ah,ah`, word `[0x104B00]` = SI (`0x15`),
  byte `[0x104B25]` = AH (0).
- **The registers survive.** `0x32A3C` pushes EBX, ECX and ESI. `0x2C3FC`
  pushes EBX, EDX and EDI, and all 30 of its `ret`s are preceded by
  `pop edi; pop edx; pop ebx`. It never names ECX or ESI itself. The review
  audited every function reachable from it: the direct callees `0x1CA14`,
  `0x1CA6C`, `0x1CC28`, `0x1CD9C`, `0x1CE04`, `0x1CE70`, `0x1D238`,
  `0x1D244`, and all of their descendants. Each one either pushes ECX and
  ESI at entry or never writes them. The AIL code that does write ECX sits
  under `0x1CA40` and `0x1CA6C`, and both push ECX. So the stores' BX (0),
  CX (`0x13`), DX (`0x78`, reloaded at `0x415D7` after `0x32A3C` clobbers
  EDX) and SI (`0x15`) hold.
- **Entrances.** Four rel32 `call`s: `0x41755` (in `0x416D4`) and `0x41DE6`,
  `0x42337`, `0x42352` (in `0x41C28`). No `jmp`/`jcc`. No dword `0x00041578`
  in either object, so the function stays unregistered.
- **Correction: the `0x88874B0` half is live.** `port/spec/game_flow.md`
  called it dead because `0x88874B0` is above `MEM_SIZE`. But the compare
  reads `[entry]`, the `0x33754` palette-table entry's `+0` **resource
  handle** (`index << 23 | offset`, decoded by `0x1B544`'s `shr eax,0x17` /
  `and edx,0x7fffff`). It is not an address. `0x088874B0` is index `0x11`,
  `s16win.gra` (INDEX size `0x8915C`), offset `0x874B0`. `0x0003E688` is
  index 0, `s16slabs.gra` (size `0x3E924`), offset `0x3E688`. The data
  object holds `0x088874B0` once, at `0xC86EC`. That is `+0x10` (the palette
  handle) of the descriptor `0xC86DC`, which `0x413C8` spawns
  (`0x413DE mov eax,0xc86dc; call 0x2AE14`). So an actor of that descriptor
  puts the handle in the list, and `0x41578` darkens it. `0x3E688` is
  `+0x10` of `0xC86F0` (at `0xC8700`). That is the row the function-less
  block around `0x4144C` passes to `0x38B18`, the same block that calls
  `0x413C8` at `0x41456`. The port keeps both halves. The unit test seeds
  both handles and a third that must not match.

### 42-E.3 `0x32A3C` (the play-time audit close)

- `push ebx/ecx/esi`, then `ECX = EDX` (the flag), `EDX = EAX & 3`,
  `EAX = 0`, `ESI = 0`, `call 0x32970` (the run clock; it pushes EDX, so
  EDX survives).
- `0x32A4F`/`0x32A56`: `EBX = [0x10746C + idx*4]`, then that dword = 0.
  `0x32A5F`: idx 0 exits here.
- idx 1: flag 0 → `0x2DAE4(8, 1)`, `0x2DAE4(0xA, t)`; flag ≠ 0 →
  `0x2DAE4(6, 1)`, `0x2DAE4(0xB, 1)`, `0x2DAE4(0xC, t)`; then
  `0x2DAE4(0x12, t)`. idx 2/3: `0x2DAE4(flag ? 7 : 9, 1)`,
  `0x2DAE4(0x13, t)`. Here t = ticks / `0x3C` (unsigned `div`).
- `0x32970` adds the elapsed `DS_00105D88` ticks into
  `DS_00107470 + 4k` for each set bit k of `DS_00107494` (that is,
  `DS_0010746C[k+1]`), so these are per-mode play-time accumulators. The
  port already leaves `0x32970` out of scope (the host owns wall time;
  `attract.c`, `flow.c`), and `0x2DAE4` is a deferred audit no-op
  (`config.c`, eeprom-config design). So the port runs only the take-and-zero
  and lists the audit arms in a `PORT:` note.
- Entrances (xrefs and rel32 agree): `0x27094` (`0x26F58`), `0x27886`
  (`0x277C0`), `0x289AF`/`0x28AD5`/`0x28B78` (`0x28788`), `0x28656` (the dead
  `0x2861C` region), `0x415CD` (`0x41578`) and `0x4240E` (`0x41C28`). Only
  `0x41578` is ported.

### 42-E.4 The stub `0x5D812`

- The bytes are `33 c0 c3`: `xor eax,eax; ret`. They sit between `0x5D808`'s
  `ret` (`0x5D811`) and the `push ebx` at `0x5D815` that starts the next
  function. The only effect is EAX = 0.
- **Every holder** (a dword scan of both objects, fixups applied): 142
  dwords in the data object and 4 immediates in the code object. There is no
  rel32 entrance.
  - Update table `DS_000A8644`: 15 entries (3, `0x12..0x1F`), dispatched by
    `0x24CED mov eax,edx; 0x24CEF call [eax+0xa8644]`. Render table
    `DS_000A86C4`: 28 entries (2, `5..0x1F`), dispatched by
    `0x25621 mov eax,ebx; 0x25623 call [eax+0xa86c4]`. The return is
    discarded at both.
  - Attract table `DS_000A8744`: 31 entries (`1..0x1F`), dispatched at
    `0x292C1`. The return is discarded.
  - Type table `0xBB9DC`: 38 cb1 and 26 cb2 slots. cb1 is called at
    `0x2B0E9` and its AL is tested (`0x2B0EF test al,al`). cb2 is called at
    `0x2B185`, and the return is discarded (`0x2B18B`).
  - Scene table `0xC7F58`: entries 3, 4, 6 and 7 (the others are
    `0x412EC`, a bare `ret`). It is called at `0x412DD` (in `0x412A0`,
    not issued by the port, `fight.c`) and at `0x425D1` (unported).
  - `DS_00104AE4` "no handler": `mov edx,0x5d812` at `0x25C04`, `0x26A27`,
    `0x27122` and `0x29626`, each followed by `mov [0x104ae4],edx`, all
    unported.
- **Port decision: document and keep the stub unregistered.** The only
  reader of its return is cb1's `test al,al`, and the port's identity test
  (`cb == FN_0005D812` → AL = 0, visible) reproduces it. Everywhere else the
  return is discarded, so skipping a `fn_resolve` miss leaves the state
  identical. A registration would have to be called through `u8(u32,u32)`
  (cb1), `void(u32)` (cb2) and `void(void)` (process, attract and
  `DS_00104AE4`) pointer types. The port's registry calls each function
  through one exact type, so a single registration cannot serve all five
  table types. Consequence for the probes: the stub's `fn_resolve` misses
  (36 164 in §35) are expected and are not a gap. A future `DS_00104AE4`
  dispatch must treat `0x5D812` as "no handler" in the same way.
- `check_type_table` (`test_fight.c`) now pins the five bytes
  `0x5D811..0x5D815` and the four code immediates (with their `0xBA`
  `mov edx` opcodes). This also proves that the port's fixups rewrite
  `0x4D812` to `0x5D812`.

### 42-E.5 Tests and mutations

- `test_effects` (`test_game.c`): a five-slot list (A `0x1111`, B `0x3E688`,
  dead C `0x3E688`, D `0x088874B0`, E `0x2222`), the resource count forced
  to 0 (so there is no lazy load and no copy), and one earlier live effect.
  - `0x29B74`: count 4 (the clear ran), records E, D, B, A from the head,
    each type 4 byte 3, and no fifth record. `0x1088EE`/`0x104AFE` = `0x78`.
    `DS_00104B00` = `0xDEAD0015`, which proves a word store.
  - `0x41578`: count 3 (no clear), D then B at byte 2, then the earlier
    effect. `DS_0010746C` is seeded `0x1000..0x1003` with mode 5, and only
    index 1 is zeroed. The five stores are checked against sentinels.
  - `0x32A3C` alone: mode 4 zeroes index 0, and indices 1..3 are untouched.
  - Every touched global is saved and restored: the list, the resource
    table, the eight globals and the accumulators.
- `test_actors`: `fn_resolve(0x29B74) == frontend_darken_all` after
  `actors_init`. `test_frontend`'s old "0x29B74/0x41578 not registered" guard
  keeps only its `0x41578` half, because `0x29B74` is now registered on
  purpose. This is the one assertion removed, and it is replaced by the
  `test_actors` check.
- **Mutations** (`scratchpad/gap2fe/mutate.py`, `mutations.txt`), 16
  single-site edits, each killed (1..5 assertions):
  - `0x29B74`: no clear; byte 3→2; a handle predicate; a dword mode store;
    the `0x1088EE` value.
  - `0x41578`: the `0x88874B0` half dropped; a clear added; byte 2→3; no
    `0x32A3C`; no `0x104B25` store; the `0x104AFA` value; `0x1088EE` =
    `0x78`.
  - `0x32A3C`: no `& 3`; mode 0 skips the zeroing.
  - The `0x29B74` registration dropped, and a wrong stub-immediate address.
  The script restored the sources, and the suite passed afterwards.

### 42-E.6 Measured and remaining gaps

- `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all checks passed, 0 compiler
  warnings. The drivers and `make verify` were not run (the controller runs
  them after the merge).
- No oracle is expected to move. Nothing in the ported frame path calls the
  new functions. The one new registration (`0x29B74`) is reachable only
  through `DS_00104AE4`, which no ported code dispatches. So the only
  whole-run probe change is that `0x29B74` can no longer miss.
- Remaining named gaps:
  - The unported callers: the six `DS_00104AE4` dispatchers `0x4F2B0`..
    `0x4F9C8`, `0x27A2C`, the `0x277C0`/`0x28788` stores, and `0x416D4`/
    `0x41C28`.
  - `0x2C3FC(0x33)` (the voice, spec §7).
  - `0x32A3C`'s `0x32970` run clock and its `0x2DAE4` audit adds (spec §7).
  - The descriptor spawns `0x413C8` and the `0x4144C` block, which produce
    the two handles.
## 42-F. The character screen's entries `0x43738`/`0x444C8` and their callees (named-gap batch 2, branch `gap2-charselect`)

**Result in one line.** The two callers of `0x43818` (§41-D), `0x43738` and
`0x444C8`, are ported as `fight_char_screen_open` and
`fight_char_screen_open_both`, together with every callee that was not yet
ported: `0x43964` (`fight_char_entry_spawn`), `0x43A08`
(`fight_char_select_actor`), `0x1D810` (`fight_select_marker_release`),
`0x1D7B8` (`fight_select_marker_spawn`), `0x2F528` (`text_number_draw_font2`)
and the `eax = -1` arm of `0x2C8F0` (`attract_config_volumes_unscaled`).
`0x43738` is stored in data as the `DS_00104AE4` frame hook, so it is
registered. No driver reaches either entry, so they are unit-tested only. The
port covers about 880 raw bytes: `0xDE` + `0x84` + `0xA4` + `0xA2` + `0x27` +
`0x57` + `0x48`, plus the `0x2C8F8..0x2C934` arm.

**Corrections to §41-D** (the raw wins):
- §41-D.2 says `0x43964` takes "the character index byte of
  `0x108163[side]`". The byte is at `0x108166 + side`. `0x43972` loads the
  dword at `0x108163 + side` and `0x4397C` does `sar esi,0x18`, which keeps
  its top byte, the one at `0x108166 + side` (signed). `0x43A10`/`0x43A16`
  read it the same way.
- §41-D.2 calls the per-frame `call [0x104AE4]` hook "the unported mode-0x17
  handler". That is wrong for `0x43738`. `0x43738` is dispatched by the mode
  `0x1A`/`0x1B` handlers `0x4F9A0`/`0x4F9C8` (the calls at `0x4F9AA`/
  `0x4F9D1`), which `0x24C5C` reaches at `0x25403`/`0x2540A`. The mode-`0x17`
  hook is a different target, `0x29B74`.

### 42-F.1 The raw (Ghidra `disassemble_function`, `read_memory` + capstone, fixups applied)

**`0x43738`** (`0x43738..0x43815`) runs these steps in order:
- `push ebx/ecx/edx/esi`;
- `0x2C3FC(0x30)` (the voice, `0x43741`);
- `0x13DF0` (`0x43746`);
- `0x2C8F0(-1)` (`0x43750`);
- `xor ah,ah`, then byte `[0x104B1B]` = byte `[0x104B24]` = 0
  (`0x43757`/`0x4375D`);
- `xor eax,eax; xor edx,edx; call 0x1D810`, then `eax = 1; xor ebx,ebx;
  call 0x1D810` (`0x43767`/`0x43773`);
- `call 0x43818` (`0x43778`).

The loop `0x4377D..0x437BB` runs with EDX = side:
- `xor ecx,ecx; lea eax,[edx+1]; mov cl,[0x104B1F]; test ecx,eax`. The mask
  is side + 1 (1, then 2), not `1 << side`. Both give the same two values;
  see the equivalent mutation below.
- Bit set: `mov ch,1; mov [edx+0x108170],ch`, then `0x43964(side)` and
  `0x43A08(side)`. Bit clear: `xor cl,cl; mov [edx+0x108170],cl`.
- `add ebx,4; xor ecx,ecx; inc edx; mov [ebx+0x108140],ecx`. EBX is 0 from
  `0x43771`, because `0x1D810` and `0x43818` push and pop EBX and EDX, and
  `0x43964`/`0x43A08` push EBX..EDI. So the stores hit `0x108144` and
  `0x108148`.

After the loop:
- word `[0x10816C]` = 5 if byte `[0x108173]` != 0, else `0xF`
  (`0x437C6`/`0x437D1`);
- `push 0x4000; ecx = 2; edx = 1; eax = 0x13; ebx = [0x10816A] sar 16;
  push 0; esi = 0x29D60; call 0x2F528`. That draws col `0x13`, row 1, the
  signed word at `0x10816C`, width 2, pad 0 and mode `0x4000`. The pad is
  the last push, so the callee reads it at `[esp+0x28]`;
- `xor ah,ah`, `[0x104AE4]` = `0x29D60` (a bare `ret`), byte `[0x108174]` =
  0.

**`0x444C8`** (`0x444C8..0x4454B`) runs the same prologue up to `0x44508`.
The loop `0x4450D..0x44532` then fills both sides with no test:
`[edx+0x108170]` = 1, `0x43964`, `add ebx,4`, `0x43A08`, `[ebx+0x108140]`
= 0. After the loop, byte `[0x108174]` = 0 and `[0x104AE4]` = `0x29D60`.
There is no countdown.

**`0x43964`** takes the side in EAX.
- `esi = [eax+0x108163] sar 24` is the signed byte at `0x108166 + side`,
  the character. `0x4454C` and the block before `0x4372E` write that byte.
- It spawns `0xC8870[side]` into `[side*4+0x108154]`, with a2 = word
  `0xC8898[ch]`, a3 = `0xFF`, a4 = word `0xC88A6[ch]` (both words
  zero-extended) and a5 = 0. The two descriptors are `0xC880C`/`0xC8820`:
  ids `0x333`/`0x335`, flags `0x2800`, palettes `0x98EC50C`/`0x98EC514`.
- It spawns `0xC8878[side]` (`0xC8834`/`0xC8848`) into
  `[side*4+0x10815C]`, with a2 = a4 = 0 and a3 = `0xFE`. That record then
  gets `0x2BCF4(rec, [ch*4+0xC88DC])` and `0x2A17C(rec, word 0xC88F8[ch],
  [ch*4+0xC8908])`.

**`0x43A08`** takes the side in EAX. The character comes from the same byte.
- The class is `cl = [ch+0xC8882]`, then `movsx ebp,cl`.
- The spawn takes a5 = 0 for side 1 and `0x4000` for side 0 (masked with
  `0xFFFF`, then pushed), a2 = word `0xC88B4[side]`, a4 = word
  `0xC88B8[side]`, a3 = `0xFF` and desc = `[side*4 + class*8 + 0xBB938]`.
- The record goes into `[side*4+0x10813C]`, and its `+0x4D` = `0x1E`.
- Then `0x1D7B8(side, class)` with EBX = `0x1800`, `or byte
  [[side*4+0x10814C]+0x29],8` and `0x2BCF4([side*4+0x10814C], 0x32B)`.

**`0x1D810`** loads `ebx = [eax*4+0x1028E0]`. If that is non-zero, it runs
`xor ecx,ecx; 0x2B150(ebx)`, then `[edx+0x1028E0] = ecx`. `0x2B150` pushes
and pops EBX, ECX and EDX, so the store writes 0.

**`0x1D7B8`** does the same release, with EBP = 0 as the stored value.
Then it spawns:
- `push 0` (a5);
- `eax = [ecx*4+0xA78B0]`, where ECX holds the class copied from EDX;
- `ecx = 0xFD` (a3);
- EDX = `0x4200` for side 1, `0x200` for side 0 (a2);
- EBX is the caller's y: `0x1D7B8` never writes it.

The record goes into `[esi*4+0x1028E0]`.

**`0x2F528`** is `0x2F4D0` (`text_number_draw`) with `or ebp,2` on the
mode. The buffer is `0x14` bytes at ESP. The cursor is saved at
`[esp+0x14]` and restored before the `ret 8`.

**`0x2C8F0` with `eax = -1`** (`0x2C8F8..0x2C934`):
- `0x2D974(0x35)`: -1 gives 8, else `sar eax,1`; then `0x1CAB8`.
- `0x2D974(0x37)`: -1 gives `0x10`, else `sar 1`; then `0x1CED4`.
- `0x1CAB8`/`0x1CED4` store into `0xA2CB8`/`0xA2CB4`, then push the value
  to AIL.
- The returned EAX is dead at both call sites, where `xor ah,ah` and byte
  stores follow.

**The tables** (data object):

| table | contents |
|---|---|
| `0xC8882` classes | `0,2,3,1,6,4,5` |
| `0xC8898` x | `0x1000,0x1C80,0x2900,0x3580,0x1640,0x22C0,0x2F40` |
| `0xC88A6` y | `0x540` for characters 0..3, `0xEC0` for 4..6 |
| `0xC88B4` side x | `0x1500,0x3F00` |
| `0xC88B8` side y | `0x3200` for both sides |
| `0xC88DC` sprite ids | `0x32C,0x32E,0x32F,0x32D,0x332,0x330,0x331` |
| `0xC88F8` pset words | `0x1C,0x20,0x14,0x18,0x24,0x28,0x2C` |
| `0xC8908` palettes | `0x98ECC8C,0x98ECD8C,0x98EC10C,0x98EC18C,0x98EC20C,0x98ECD0C,0x98ECC0C` |
| `0xBB938` side actors | 14 descriptors `0xBAEF8..0xBAFFC`: flags `0x2200`; desc[1] `0x300` for side 0, `0x40300` for side 1 |
| `0xA78B0` markers | `0xA7824..0xA789C`: ids `0x4030,0x402D,0x4031,0x4032,0x402C,0x402F,0x402E`, flags `0x2A00` |

All the descriptors are type 0.

### 42-F.2 Entrances (`get_xrefs_to`, rel32 scan of the code object, dword scan of both objects)

**`0x43738`** has one direct call and two data stores:
- a `call` at `0x4372E`. It sits in a block Ghidra has no function for,
  after the cursor setup at `0x43691..0x43724`.
- the dwords `0x28D6A`/`0x28D87`. `0x28D68` and `0x28D80` store the address
  into `DS_00104AE4`, then call `0x4F980(0x10)`.

The hook then runs through the mode handlers:
- `0x4F980` sets `DS_00104AFA` = `0x10` and the mode `DS_00104B00` =
  `0x1A`.
- `0x4F9A0` (called at `0x25403`) runs `call [0x104AE4]` once `0x4F9E4`
  reports done, then sets mode `0x1B`.
- `0x4F9C8` (mode `0x1B`) calls the hook again, which is now `0x29D60`, and
  restores the mode from `DS_00104AFA`.

Two immediates load the setters' addresses:
- `0x28D68` is loaded into EBX just before a call to `0x42FE0` (dwords
  `0x42CF4`, `0x42D35`, `0x42D7D`, `0x42FB7`).
- `0x28D80` is loaded into ESI before `call 0x2DAE4` (`0x28E4E`).

(Superseded by §43-B: both are themselves `DS_00104AE4` hooks, and each
immediate above is followed by a store into `DS_00104AE4`. `0x42FB7` is in
the unreferenced stub `0x42FB0`, not in `0x42CB4`.)

The other entrances:

| target | rel32 calls | dwords |
|---|---|---|
| `0x444C8` | `0x4462B` only (in `0x4454C`, itself called only at `0x43688`) | none |
| `0x43964` | `0x43796`, `0x43B59` (in `0x43B24`), `0x44517` | none |
| `0x43A08` | `0x4379D`, `0x43B60`, `0x44521` | none |
| `0x1D810` | `0x25C64` (in `0x25C1C`), `0x43767`, `0x43773`, `0x444F7`, `0x44503` | none |
| `0x1D7B8` | `0x43A84`, `0x44035` (in `0x43FBC`), `0x4432D` (in `0x442A0`) | none |
| `0x2F528` | `0x437FE`, `0x43B18`, `0x4C418`, `0x4C45D`, `0x4F422`, `0x4F5BC`, `0x4F610` | none |
| `0x2C8F0` | `0x110B0` (the ported -2 arm), `0x20C2B` (`game_init`, -1), `0x2FA2D`, `0x2FA37`, `0x308C1`, `0x43750`, `0x444E0` | none |
| `0x29D60` | 15 calls | the immediates `0x437FA`/`0x44537` |

### 42-F.3 The port

- `fight.c`: the four `0x43xxx` functions and the two `0x1Dxxx` ones, after
  `fight_char_screen_setup`.
  - The hook store writes the raw's value `0x29D60` (`FN_00029D60`), as
    `fighter_14814` stores its callbacks. Nothing dispatches `DS_00104AE4`
    yet.
  - `0x2C3FC(0x30)` is a `PORT:` voice note (spec §7).
  - `0x43A08` writes `+0x4D` and `0x10814C[side]+0x29` with no null test, as
    the raw does.
- `actors.c`: `text_number_draw_font2` (`0x2F528`) after
  `text_number_draw`, and `fn_register(0x43738, fight_char_screen_open)` in
  `actors_init`.
- `attract.c`: `attract_config_volumes_unscaled`, next to the -2 arm.

### 42-F.4 The assertions and mutations

`check_char_screen_open` (`test_fight.c`, after `check_char_screen_setup`)
saves and restores the data object, both pools, both offscreen buffers, the
resource table, the DAC and the aperture. Each run starts from that snapshot.

`chs_seed` seeds:
- a sentinel in every global the entries write;
- live records in both marker slots;
- `DS_0009AF3C/3D` = 1/5, the two volumes, the text cursor and the two
  countdown cells;
- config fields `0x2A`/`0x35`/`0x37` = 0/`0xA1`/`0x41`.

The runs:
- **(a)** `0x43738` with `DS_00104B1F` = 2, characters 3/6 and
  `DS_00108173` = 1. Only side 1 is filled:
  - side 0's four pointers keep `0xDEADBEEF`, its marker slot is 0, and its
    `0x10814C` record keeps `0x32A`;
  - the countdown is 5, drawn "05": `0x3FA0` at col `0x13`, nothing at
    `0x14` and `0x3FA5` at `0x15`, because the `0xBD048` digits are 16
    wide. The palette is `0x8099AC`;
  - 9 records are active.
- **(b)** `0x43738` with `DS_00104B1F` = `0xFD` and `DS_00108173` = 0. Only
  side 0 is filled. The countdown is `0xF`, drawn "15" (`0x3FA1`,
  `0x3FA5`), and 9 records are active.
- **(c)** `0x444C8` with `DS_00104B1F` = 0 and characters 5/0. Both sides
  are filled. The countdown word keeps `0x7777`, its cells are 0, and 11
  records are active.
- **(d)** the callees alone:
  - `0x1D810` on an empty slot and on a live one (the dead bit is set and
    the slot is zeroed);
  - `0x43A08`'s `+0x29 |= 8` on a cleared bit, its side-1 class-3 actor
    (palette `0x98EC10C`) and its marker `0x4032`;
  - `0x1D7B8` on a live marker: the class-3 marker just spawned, with
    palette `0x98ECBEC`. Its release arm sets the old record's dead bit and
    zeroes its pset palette. The class-5 marker (`0x402F`) lands in the slot
    at x `0x4200` and at the caller's y (`0x1234`). This is the only run that
    exercises the arm: in (a)..(c) `0x1D810` has already emptied the slot
    each time `0x1D7B8` runs.

`chs_check_side` checks each filled side:
- the four records at their exact addresses, in spawn order;
- each record's `+8`, `+0x18`, `+0x1C` and `+0x49`;
- the panel's pset id, pset word and palette;
- the side actor's `+0x4D`, `+0x28` bit `0x4000`, `+0x2E` (the side),
  `+0x2C` (the class) and palette;
- the marker's id and position;
- `0x10814C[side]+8` = `0x32B`.

`chs_check_common` checks:
- the prologue's byte stores, the effect lock and count, and the volumes
  `0x50`/`0x20`;
- the hook `0x29D60`, `0x108174`, `0x108144`/`0x108148` and the restored
  cursor;
- no RNG draw;
- `0x43818`'s three records not dead, each with palette `0x98EC71C`.

Two further checks: `fn_resolve(0x43738)` returns the port, and
`test_game.c` checks the -1 arm directly (scale 0, `0xA1` -> `0x50`, `0x41`
-> `0x20`).

**Mutations** (`scratchpad/cs/mut.py`, logs `mut.log`/`mut2.log`/
`mut3.log`): 63 single-site edits. They cover each index, constant and table,
each dropped store or call, the call order, the loop bound, the hook value
and the registration. 60 fail 1..60 assertions. Three survive, and all three
are equivalent:
- `O1` replaces the mask `side + 1` with `1 << side`. The two are equal for
  sides 0 and 1.
- `O8`/`B7` drop the direct `0x13DF0` call in either entry. `0x43818`'s
  `0x2BAF4` calls `0x13DF0` again (`0x2BB2B`) before anything reads the
  effect state. It also rebuilds the palette table and the render list that
  the `0x1D810` calls in between touch.

In the first sweep, dropping either `0x444C8` release also survived (`R4`,
`B8`). The pool rebuild re-allocates the stale slot's record, and `0x1D7B8`
would then kill that live record. The check that `0x43818`'s three records
stay alive with their palette was added for this. Those two mutations, and
the new `R5` and `O15`, now fail.

In review, deleting `0x1D7B8`'s release arm also passed, because no run gave
it a live record. The (d) case for that arm was added. Deleting the whole
arm (`M5`) now fails 2 assertions, and so does dropping only its
`0x2B150` call (`M6`). A wrong hook value (`O6b`, `0x29D64`) fails 2.

### 42-F.5 Oracles and remaining gaps

No driver reaches either entry. The mode `0x1A` hook dispatch (`0x4F9A0`,
`0x4F9C8`) and every caller are `0x24C5C`-mode code the port does not run:
`DS_00104B00` stays 3 on the ported path. `0x43738` appears as a dword only
in the code object, so no stream walk resolves it. No oracle is expected to
move.

The named gaps are all mode code:
- the hook installers `0x28D68`/`0x28D80`, `0x4F980`, the mode
  `0x1A`/`0x1B` handlers `0x4F9A0`/`0x4F9C8` (with `0x4F9E4`/`0x4FA88`),
  and the code at `0x42CF3..0x42FB7` and `0x28E4E` that loads the setters;
- the entries' callers. One is the block ending at `0x4372E`, which has no
  Ghidra function. The other is `0x4454C`, which is small and
  self-contained, but its only caller `0x43688` is in that same unported
  block;
- the select screen's per-frame code, and the remaining call sites of the
  callees ported here:
  - `0x43B24`, which calls `0x43964`, `0x43A08` and `0x2F528` again;
  - `0x43FBC` and `0x442A0` (its call at `0x4432D`), which call `0x1D7B8`;
  - `0x25C1C`, which calls `0x1D810`;
  - the other `0x2F528` callers `0x4C418`, `0x4C45D`, `0x4F422`, `0x4F5BC`
    and `0x4F610`;
- `game_init` still does not call `0x2C8F0(-1)` at `0x20C2B`. The port's
  `flow.c` sets its own startup SFX volume;
- the -1 sentinels in `0x2C8F0` are transcribed but untested. `0x2D974`
  cannot return -1 for these fields: the descriptor dwords at `0x2D3D4`/
  `0x2D3DC` (`0xE0C0`/`0x62C0`) give widths of 4 and 2 nibbles;
- `0x2C3FC(0x30)`, the voice (spec §7).

## 42-D. The worshipper types 2, 7 and 9..12 and the mode tail `0x2545C` (named-gap batch 2, branch `gap2-worship`)

**Result in one line.** `0x49C78`'s type handlers 2, 7 and 9..12 (the named
gaps §7.4 left in `fight_effects_pass`) are ported with their unported
callees `0x4B2AC` (type 10's walk) and `0x4A7D4` (type 11's arrival test).
`0x24C5C`'s mode switch at `0x2545C` is ported whole: the `0x25509`
mode-`0x21` arm (§30.2) with its unported camera `0x12FD8`, and the `0x0C`,
`0x22`/`0x23` and `0x25` arms, whose callees were all ported already. No
port path reaches any of it, so it is unit-tested and no oracle is expected
to move.

### 42-D.1 The raw (Ghidra `disassemble_function`, fixups applied)

- **The jump table** `0x49C2C` (`read_memory`, 15 dwords): type 0
  `0x49D1E`, 1 `0x49D2F`, 2 `0x49D90`, 3 `0x49DB3`, 4 `0x49E5A`, 5
  `0x49EC0`, 6 `0x49F11`, 7 `0x49FC2`, 8 `0x4A08A`, 9 `0x4A115`, 10
  `0x4A131`, 11 `0x4A17A`, 12 `0x4A1EB`, 13 `0x4A24A`, 14 `0x4A346`. The
  dispatch at `0x49D0F` leaves EDX = si * 4 (`lea edx,[edi*4]`), EDI = si
  (zero-extended at `0x49CDB`/`0x49CF2`), ECX = entry.
- **Type 2** (`0x49D90`): `mov bx,[ecx+0x18]; dec ebx; mov [ecx+0x18],bx;
  test bx,bx; jg` out, else `0x4AC38(ecx, dx = si)`.
- **Type 7** (`0x49FC2`): EDX = actor `+0x3C` (actor = `[ecx+8]`). With
  `0 < x < 0x5400` (`jle`/`jge`) and entry `+0x1C` bit 2 clear: `+0x1C |= 4`
  and `0x2C3FC(0xDE)` (`0x49FF1`; it pushes EDX, so x survives). With
  `0 <= x <= 0x5400` (`jl`/`jg`) and `+0x1C` bit 0: EAX = `[actor+0x32] sar
  16` (the `+0x34` word); negative: `cmp eax,-0x100; jle` `dec word
  [+0x34]`, else `+0x34` = `0xFF00`; non-negative: `cmp eax,0x100; jle`
  `inc word [+0x34]`, else `0x100`. Then, re-reading the speed, `== -0x100`
  with `x < -0x300` (`jl`) or `== 0x100` with `x > 0x5700` (`jle` skips):
  `or byte [actor+0x28],0x80` (`0x4A081`).
- **Type 9** (`0x4A115`): `cmp word [0x1088b4],0; jz` `inc dword
  [esp+0xc]`, else `+0x1E` = `0x0A`.
- **Type 10** (`0x4A131`): when byte `[0x1088c7]` or `[0x1088c8]` is non-zero,
  `0x4B430(ecx, dx = si, ebx = 1)` and out on a non-zero EAX. Then
  `0x4B2AC(ecx, dx = si)`, `+0x1E` = `0x0B`, `+0x1C &= 0x7F`.
- **Type 11** (`0x4A17A`): `mov byte [esp+0x14],1`; `+0x2C` =
  `0x496AC([actor+0x30] sar 16)`; `0x4A7D4(ecx)` non-zero: `+0x38`, `+0x34`,
  `+0x36` = 0 and `0x2BC30(actor, [edx + 0xC9544], 5.0)`. EDX is still
  si * 4: `0x496AC` (`push edx`/`pop edx`) and `0x4A7D4` (`push ebx; push
  edx`) preserve it. Else, with `+0x34` non-zero, `xor ah,ah; mov
  [esp+0x18],ah`.
- **Type 12** (`0x4A1EB`): byte `[0x1088ca]` zero: `+0x29 |= 0x40`, `+0x34` =
  `0xFF80`; else `+0x29 &= 0xBF`, `+0x34` = `0x80`. `0x2BC30(actor,
  [si*4 + 0xC95D4], 3.0)`, then `+0x1E` = `0x0E` when byte `[0x1088c6]` is
  non-zero, else `0x0D`.
- **The frame locals.** Mode 9 initialises them at `0x49C8E..0x49CA6`:
  `[esp+0x18]` = 1, `[esp+0x14]` = 0, `[esp+8]` = `[esp+0x10]` = 0 and
  `[esp+0xc]` = EDX with DX = 0. Outside mode 9 they are not initialised.
  Only the mode-9 block reads them: `[esp+0xc]` as AX at `0x4A549`/`0x4A554`,
  `[esp+0x18]` and `[esp+0x14]` at `0x4A567`/`0x4A56E`.
- **`0x4A7D4`** (49 bytes): `|[eax+8]+0x18 − [eax+0x14]| <= |2 ·
  ([eax+8]+0x32 sar 16)|` (`add eax,eax`), `setle`, `and eax,0xff`.
- **`0x4B2AC`** (322 bytes; EBX = entry, ESI = si): s = `movsx(byte
  [0x1088c9] ^ 1)`; `[0x108878]` = byte `[s·0x94 + 0x107831]` (slot `+0x81`)
  − (byte `[0x1088cc]` + `[s·0x94 + 0x1077ec]` (slot `+0x3C`) `div` dword
  `[0xc9520]`). The divide is unsigned (`xor edx,edx; div edi`) and
  `[0xC9520]` reads `0xC350` (50000). The value is stored, capped at 7
  (`cmp eax,7; jl`) and stored again. Then ECX = `[0x1088c6] sar 0x18`
  (sign-extended byte `0x1088C9`) against the zero-extended entry `+0x21`.
  Equal: target = `[0x108870]`. Otherwise target = `[0x10887c]`, plus
  `rng(0xC00)` when byte `[0x1088c6]` is set. A zero target does `+0x34` = 0
  (`0x4B3E2`, EAX = actor from `0x4B316`). Otherwise `+0x29 &= 0xBF`,
  d = |x − target| and step = `[+0x32] sar 16`, negated when the word
  `+0x34` is negative. `d < step` (`jl`) gives `+0x34` = 0. Else `+0x14` =
  `0x2BE4C(actor, target)`; when `+0x14 > x` (`jle`), `+0x34` = `0x80` and
  `+0x29 &= 0xBF`, else `0xFF80` and `+0x29 |= 0x40`. Last,
  `0x2BC30(actor, [esi*4 + 0xC95D4], 3.0)`.
- **`0x2545C`** (in `0x24C5C`, after the `0x25414` tail): `mov ax,[0x104b00];
  cmp ax,0x21; jc` → `cmp ax,0xc; jnz` out; `jbe 0x25509`; `cmp ax,0x23; jbe
  0x2554B`; `cmp ax,0x25; jz 0x25591`; else out. Every arm ends the
  function.
  - `0x25487` (mode `0x0C`): i = byte `[0x104b12]`. With slot i's `+0x41`
    bit 0 (`test byte [eax+0x1077f1],1`) and bit 1 of the word `[0xef6dc]`,
    ps = `[0x1014ec]` + word rec `+0x56` · 0x20. Word `[0x104af6]` = word
    `[ps]`, then word `[ps]` = (word & 0x8000) | 0x1E1.
  - `0x25509` (mode `0x21`): `0x3BB90`, `0x12FD8(al = 1)`. Then, for side 0
    and 1, `0x186D0(side)` unconditionally and `0x2A690(rec)` when the slot's
    record is non-zero. Then `0x33F08`.
  - `0x2554B` (modes `0x22`/`0x23`): `0x12D48`, `0x186D0(byte [0x104b1a])`,
    that slot's `0x2A690` when its record is non-zero, `0x33F08`.
  - `0x25591` (mode `0x25`): as `0x25509` without `0x3BB90`, with
    `0x12FD8(al = 0)`.
- **`0x12FD8`** (346 bytes): with AL = 0, `[0xf0af0]` = `[0x108884]`.
  Otherwise a = `setge([0x1077e4], [0x107878])` (slot 0 `+0x34` against slot
  1's, signed), b = 1 − a. The stack copies of both `+0x34`s are read once,
  before any pull.
  - The left slot a is pulled when x < mid − `0x2E80` and `+0x34 < +0x38`,
    or when x >= mid − `0xA80` and `+0x34 > +0x38`.
  - The right slot b is pulled when x > mid + `0x2E80` and `+0x34 > +0x38`,
    or when x <= mid + `0xA80` and `+0x34 < +0x38`.
  - A pull stores the latch in `+0x34` and `+0x2C` and the record's `+0x18` =
    `0x18714(side)` (EAX = a at `0x13087`, `mov eax,ecx` = b at `0x130E3`).
  - Then `[0xf0af0]` is clamped to mid ± `0x1500`. Every compare is signed
    after a 32-bit add.
  - Mid is `DS_00108884`. Its only writers are `0x4BD98` (`0x4BE5C` and
    `0x4BE8A`) and `0x4E11C` (`0x4E214`).

### 42-D.2 Entrances (`get_xrefs_to`, a rel32 scan of the code object and a dword scan of both objects)

- `0x4B2AC`: one `call` at `0x4A163`. `0x4A7D4`: one `call` at `0x4A196`.
  No dword for either.
- `0x12FD8`: `call`s at `0x25513` and `0x25593`, no dword.
- `0x25509`: one `jbe` at `0x25468`. The handler addresses `0x49D90`,
  `0x49FC2` and `0x4A115..0x4A1EB` appear only as jump-table dwords
  (`0x49C34`, `0x49C48`, `0x49C50..0x49C5C`), with no rel32.
- Who sets the types. A capstone linear sweep of the code object lists every
  store whose range covers `+0x1E` (372; review of this branch). The
  immediate stores of 2, 7 and 9..12 and the one register-source type setter
  are:
  - type 2: `0x4E672` in `0x4E5A4` (called at `0x4E84E`), and `0x4DDB6 mov
    [esi+0x1e],al` in `0x4DBEC`. There AL = `[esp+0xc]` + 1, and `[esp+0xc]`
    cycles 1, 2, 0 (`0x4DC75..0x4DC85`), so it stores types 2, 3, 1 in
    rotation; on its `[esp+0x1c]` gate `0x4DDAA` stores 4 instead. The
    entries come off the `0x1083C4` free list. `0x4DBEC` is called only at
    `0x2766D` (in `0x274FC`, the mode-`0x0D` arm) and `0x29838` (in
    `0x296B8`, the mode-`0x32` arm);
  - type 7: `0x49BF3` in `0x4987C` (called at `0x4A616`, the effects
    tail's gated rng(2) arm, and at `0x4D792`);
  - type 9: `0x4AAE6` (`0x4AAD0` in mode 9);
  - type 10: `0x4A11F`; type 11: `0x4A16B`;
  - type 12: `0x4A583` (the mode-9 block, every entry).
  The other register-source byte stores to `[reg+0x1e]` are not type
  setters: `0x30881` and `0x3DD44` store to the stack, and `0x58784` and
  `0x59206` sit in unrolled library fill/blit loops. The register dword
  stores covering `+0x1E` write an actor's `+0x1C` (`0x4B8E8`, `0x4DA19`,
  EDX = `[ecx+8]`) or the stack.
  `0x4E5A4`, `0x4DBEC` and `0x4987C` are unported, and the port never runs
  modes 9, `0x0D` or `0x32`. So no port path reaches the six types. Type 7
  is reachable in mode 3 in the raw, through `0x4A5A6` → `0x4A616` →
  `0x4987C` whenever `DS_001088BF` is 1..4 (set by the ported `0x391CA` and
  by `0x14DA4`). Only the port's `0x4987C` gap keeps it out.
- `DS_001088B4` is written only by the mode-9 block (`0x4A54D` = 0, `0x4A559`
  = 1). `DS_00108870`/`DS_0010887C` are written only by `0x4A928`, which is
  reached from the mode-9 block (`0x4A562`).

### 42-D.3 The port

- `fight.c`:
  - The six `case` bodies in `fight_effects_pass`.
  - `fight_4a7d4` and `fight_4b2ac` (static).
  - The three frame locals as C locals, set in mode 9 as the raw does. They
    start at 0 elsewhere (`PORT:`: nothing reads them there), and
    `[esp+0xc]`'s high word is 0, since the reader compares only AX.
  - The locals are `(void)`-read at the mode-9 gap, which is their only
    reader.
  - The `default:` label no longer names a gap; every type 1..14 has a case.
  - Local `#define`s for `0x1088C7/C8/C9` (no `symbols.h` names).
- `camera.c`/`camera.h`: `camera_pair_hold` (`0x12FD8`).
- `flow.c`: the `0x2545C` switch at the end of `game_frame`, with a local
  `#define` for `0x104B1A`.
- The type-7 voice `0x2C3FC(0xDE)` is a `PORT:` stub (spec §7).
- No new registration: none of the new addresses is stored in data.

### 42-D.4 The assertions and mutations

- `check_effects_worship` (`test_fight.c`, after `check_effects_tail`)
  saves and restores:
  - the `0xC9544`/`0xC95D4`/`0xC958C` entries 0 and 3;
  - `0x108840..0x10893F`, `0x107688..0x1078D7` (both slots and the s = −2
    slot `0x107688`), the `0x100BD3` plane (`0x17D30`'s pixel fill);
  - `DS_00104B00`, `0x104B1A`, `DS_001014EC` and the RNG.
  It covers:
  - type 2's countdown, arrival and signed edge;
  - type 7's latch, the four steer arms, the four gate edges and the two exit
    flags, with a `0xFF01` speed that must not flag;
  - type 9's word gate;
  - type 10: the cap and its value, the unsigned divide with the signed cap
    (−42931), both targets, a zero target on either side, s = −2 with the
    zero-extended `0xFF` side, the `rng(0xC00)` arm, d = step − 1 and d =
    step, the negative step, the hold through `0x4B430` (C7, C8 and a
    declining zero stream), and bit 7 cleared (mode `0x22` with
    `DS_00104B1A` = 2, so `0x4B69C`'s `0x17D30` tests neither side);
  - type 11's arrival, doubling and magnitudes;
  - type 12's two directions and the 13/14 choice on `DS_001088C6`, not on
    `DS_001088CA`.
- `check_mode_tail` (after `check_game_frame_tail`) saves and restores whole
  ranges: `mem[0..0x1F]` (pset 0, which `actor_pset_point` writes for the
  out-of-pool scratch records), `0x9AD50..0x9AD5F`, `0xEF6D8..0xEF6DF`,
  `0xF0A60..0xF0AFF`, `0x100A70..0x100B6F`, `0x1014E0..0x1014F7`,
  `0x104AE0..0x104B2F`, `0x105BC0..0x105D6F`, `0x1077A0..0x10790F`,
  `0x107D20..0x107D3F` and `0x108840..0x10893F`. These cover everything the
  seeds, `tf_demo_fixture` and `game_frame` write. A temporary whole-memory
  diff around both new checks (reverted) planted sentinels first: an XOR in
  the slots `0x1077A8..0x1078D7`, `0x100A70..0x100B6F`, `mem[0..0x1F]` and 34
  scalar globals. The diff was empty outside the `FIGHT_*` scratch. With the
  slot range cut to 0x10 bytes it listed the slot fields, so the probe can
  fail. It covers:
  - `0x12FD8` directly: AL only, each band and direction on each side with
    the record x through `0x18714`, the far band's edge, `setge`, and both
    clamp edges with a signed case;
  - `game_frame` in modes `0x21`, `0x25`, `0x22`, `0x23`, `0x0C` (acting and
    not acting) and `0x24` (no arm).
- **Mutations** (`scratchpad/w/mut.py`, `mut.log`): 116 single-site edits
  of the new code. 111 fail 1..22 assertions. None crashes, and the two first
  written as bare deletions (build failures) were re-run as `;` and fail.
  Two assertions were added after the first sweep: `0xFF01` for the type-7
  speed pairing and the far band's edge for `0x12FD8`. The 5 survivors are
  equivalent:
  - type 7 without the bit-2 gate: the OR is idempotent and the only other
    effect is the stubbed voice. It is equivalent only while `0x2C3FC(0xDE)`
    stays a stub;
  - `0x4B2AC`'s second `+0x29 &= 0xBF`: `0x4B35D` has already cleared the
    bit;
  - the three frame-local writes, which only the unported mode-9 block
    reads.
  The sources were restored and checked with `cmp`.

### 42-D.5 Measured and remaining gaps

- `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all checks passed, with 0
  compiler warnings. The drivers and `make verify` were not run; the batch
  controller runs them after the merge.
- No oracle is expected to move. No port path sets types 2, 7 or 9..12
  (42-D.2; type 7 waits on the `0x4987C` gap). The port only runs modes that
  take no `0x2545C` arm: 3, and `0x15` once `gap2-frontend`'s `0x29B74`
  stores it.
- Remaining named gaps in `0x49C78`:
  - the mode-9 block `0x4A487..0x4A58F` with `0x4A928`/`0x4AA6C` (the only
    readers of the frame locals and the only writers of `DS_001088B4`,
    `DS_00108870`/`DS_0010887C`, `DS_001088C7`/`C8`/`C9`/`CA`);
  - the prelude's per-side words `[esp]`/`[esp+2]` and count `[esp+8]`;
  - case 14's `[esp+0x10]`;
  - the type-8 held body and the case-13/14 bodies;
  - the type setters `0x4E5A4`, `0x4DBEC` (modes `0x0D`/`0x32`) and
    `0x4987C`.

## 42-C. The grab arms `0x4B788`/`0x4D898`, case 8's held body and `0x4B470`'s eighth-hit tail (named-gap batch 2, branch `gap2-grab`)

**Result in one line.** The named gaps §29 left in the effects pass, `0x4B788`'s
grab body, case 8's held body (`0x4AF04`) and `0x4B470`'s eighth-hit tail
(`0x4BD98` with `0x13134`, then `0x4CB18`), are ported from the raw, with the
mode-`0x22` twin prelude `0x4D7A4` and its grab arm `0x4D898` and the shared
callee `0x2BD20`. The demo reaches none of them (§29: its fighters never touch
a lying worshipper in their grab move, and no entry gets eight hits), so they
are unit-tested; no oracle is expected to move.

### 42-C.1 Entrances (get_xrefs_to, a rel32 CALL/JMP/Jcc scan and a dword scan of both fixed-up objects)

- `0x4B788`: one call, `0x4B705` in `0x4B69C`. No dword.
- `0x4D898`: one call, `0x4D80B` in `0x4D7A4`. `0x4D7A4`: one call, `0x4D323`
  in `0x4D2D0`, made only when the word `0x104B00` is `0x22` (`0x4D317 cmp
  edx,0x22; jne`), with EDX = (u16)(`+0x48` − `0x20`). `0x4D2D0` is called at
  `0x26D28` (`0x26C8C`) and `0x26FF0` (`0x26F58`), which `0x24C5C`'s mode
  table `0x24B8C` reaches for modes `0x22` (`0x25321`) and `0x24`
  (`0x2532B`). None of these is ported.
- `0x4BD98`: one call, `0x4B590`. `0x13134`: one call, `0x4BDBA`.
- `0x4CB18`: `0x4B59E` (`0x4B470`) and `0x4C760`/`0x4C774` (`0x4C60C`, EBX = 1
  when the side's slot `+0x5F` is `0xC..0xF`, else 0; `0x4C60C`'s one caller
  is `0x4C21B` in `0x4BF18`; not ported).
- `0x4AF04`: one call, `0x4A0A2` (case 8). `0x2BD20`: `0x4B956`, `0x4DA85`.
- No dword in either object equals any of these addresses.

### 42-C.2 The raw (Ghidra disassembly, fixups applied; tables by `read_memory`)

- **`0x4B788`'s grab** (`0x4B83D..0x4B98F`; ECX = entry, EBX = slot, DL =
  ch, EDI = si): `+0x1C |= 0x40`; `+0x10` killed (`0x2B150`) and zeroed;
  DH = 1 when the fighter record's `+0x28` word lacks `0x4000`: then the
  actor's `+0x29 &= 0xBF`, else `|= 0x40`; `+0x29 &= 0xEF`; `+0x20` = hit − 1;
  x: `mov eax,[ch*2 + 0xC977D]; sar eax,0x18; shl eax,6` — the signed byte
  `0xC9780 + ch * 2` × 64 — subtracted from the fighter's `+0x18` when DH, else
  added; height `[ch*2 + 0xC977E] >> 24` (the byte `0xC9781 + ch * 2`) × 64 +
  the fighter's `+0x1C`; the `+0x32` word copied; `+0x2C` = `0x496AC(+0x30 >>
  16)` after that copy; `+0x38`, `+0x36`, `+0x34` = 0; type 8;
  `0x2BC30(actor, [[ch*4 + 0xC97AC] + si*4], 0)`; the actor's `+0x4A` = the
  fighter's `+0x56` byte; `0x2BD20(actor, word +0x56)`; `0x2BC30(fighter,
  [ch*4 + 0xC9790], [ch*4 + 0xC97C8])`; the voice `0x2C3FC(si < 3 ? 0xD4 :
  0xD5)`; return 0. `0x2B150` preserves EBX/ECX/EDX, `0x2BC30` EBX/ECX/ESI,
  `0x2C3FC` EBX/EDX/EDI and does not write ECX, so the live registers hold.
- **`0x4D898`** (`0x4D898..0x4DBB3`): the slot of hit − 1; `[esp+4]` = its
  `+0x5F` (mv), `[esp+8]` = its `+0x7A` (ch, read back as `[esp+5] >> 24`,
  signed). Accepted when mv = `0xC97F2[ch]`, or ch ∈ {0, 5, 4} and mv = `0xA`
  (`0x4D8E2..0x4D8FC`), or ch ∈ {1, 6} and mv = `0xB` (`0x4D8FE..0x4D91C`);
  else return 1. No `0x105B3A` or other-side `+0x54` gate. Then the same
  return-0 gates and the same grab body as `0x4B788` (`0x4D922..0x4DABE`,
  DL for DH). Then the award to the slot `0x1077B0 + byte[0x104B1A] * 0x94`,
  `+0x5B`: for mv = `0xC97F2[ch]`, AL by (u8)(actor `+0x48` − `0x20`) through
  the table `0x4D880` = `0x4DAF3, 0x4DAF7, 0x4DAFB, 0x4DAFF, 0x4DB03, 0x4DB07`
  (AL = `0x10, 0xE, 0x15, 0xC, 0xA, 0xD`; above 5 → `0x4DB07`), then
  `(AL * 0x78) / 0x64` (`shl 4; sub; *8; idiv 0x64`); else 1. When `+0x5B` +
  the award > `0x78` (signed) it becomes `0x78`, else it adds. Return 0.
- **`0x4D7A4`**: `0x4B69C`'s body with `0x4D898` for `0x4B788`, and without the
  `0x1088B2[+0x21]` = 1 store of the release (`0x4D869` goes straight to
  `0x4D86F`).
- **Case 8** (`0x4A08A..0x4A110`): bit 6 clear → skip; `0x4AF04(+0x20)`
  non-zero → skip; the actor's `+0x4A` zero → skip; else `0x1014F4 + k *
  0x68 + 0x4B` = 0, `+0x2A &= 0xF7`, `+0x29 &= 0xBF`, `+0x4A` = 0, `+0x1C &=
  0xBF`, `0x1088AE[+0x21]` += 1, `0x1088B2[+0x21]` = 1, `0x4B470(entry, EDX =
  EDI = si)` — without `0x4B69C`'s `+0x1F` count.
- **`0x4AF04`** (AL = side, zero-extended): ch = the slot's `+0x7A`; `ja` 6 →
  1; else the table `0x4AEE8` (`0x4AF39, 0x4AF7F, 0x4AFC1, 0x4AFE0, 0x4AFFF,
  0x4AF5C, 0x4AFA2`) compares the fighter record's `+8` (its stream cursor)
  unsigned against `[0xE7B02, 0xE7B50]`, `[0xE4744, 0xE47FC]`, `[0xED7AE,
  0xED80C]`, `[0xD2DEC, 0xD2E06]`, `[0xEB38A, 0xEB3E4]`, `[0xD4A3C,
  0xD4A8A]`, `[0xE137A, 0xE1432]` for ch 0..6: inside → 1, else 0. Each range
  starts at `0xC9790[ch]`.
- **`0x4B470`'s tail** (`0x4B584..0x4B59E`): EBX = `[0x108864]` (0 past the
  gate); `0x4BD98(EAX = entry)`, which pushes and pops EBX; then
  `0x4CB18(EAX = entry, EDX = EDI = si, EBX = 0, ECX = byte +0x20)`,
  unconditionally.
- **`0x4BD98`**: returns when `[0x104AD8]` > 0 (`jg`), `[0x104ABC]` < 2
  (`jc`) or `0x13134()` ≠ 0. Else `0x108864` = entry, type 6, `0x1088C1` =
  1, word `0x104B00` = `0x21`, `0x1088C5` = 1, word `0x1088AA` = `0x1E`, words
  `0x1088A0`/`0x108898`/`0x1088AC` = 0, bytes `0x10889D`/`0x10889C` = 0,
  `0x104AEC &= 0xFE`, `+0x1C |= 0x20`; `0x108884` = the midpoint of
  `[0x1077E4]`/`[0x107878]` (signed `jge`: min + ((max − min) `sar` 1)), then
  less `[0x108854]`; `0x2AE14(0xBAB88, EDX = [0x108884], ECX = word[0xBD898],
  EBX = [0x108880] − 0x3140; 0)` into `0x108868` and the same from `0xBAB9C`
  into `0x10886C`; `0x2BC30(0x10886C's actor, 0xEF66A, 1.0)`; both `+0x36` =
  `0x1A4`. (Ghidra's decompile of the midpoint and the spawn arguments is
  wrong; the disassembly is followed.)
- **`0x13134`**: 1 when both fighter records' `+0x18` are below −`0x3300`, or
  both above `0x3300` (signed); else 0.
- **`0x4CB18`** (EAX = entry, EDX = si, EBX = flag, ECX = side): the voice
  `0x2C3FC(si < 3 ? 0xD1 : 0xD0)`; `0x2BC30(actor, 0xC9604[si], 3.0)`; d =
  |`[0x1077E4]` − `[0x107878]`| capped at `0x3F00`; h = `0x3BC0` − the actor's
  `+0x1C`; `0x1A570(side)` true negates d; `+0x34` = d / (flag ? `0x38` :
  `0x70`); `+0x36` = flag ? 0 : h / `0x16` (both `cdq; idiv`); type 6;
  `+0x1C &= 0x7F`.
- **`0x2BD20`** (EAX = rec, EDX = v): `[0x1014F4] + rec.+0x4A * 0x68 + 0x4B` =
  BL; return 0.
- Tables (`read_memory`): `0xC9780` pairs (x, y) = (`0x55`, 3), (`0x57`,
  `0xC`), (4, 3), (`0x3F`, 0), (`0x4C`, −1), (`0x55`, 3), (`0x48`, 3);
  `0xC97AC` = `0xC967C, 0xC96AC, 0xC96C4, 0xC96DC, 0xC9694, 0xC96F4, 0xC970C`;
  `0xC9790` = `0xE7B02, 0xE4744, 0xED7AE, 0xD2DEC, 0xEB38A, 0xD4A3C, 0xE137A`;
  `0xC97C8` = 3.0, 5.0, 4.0, 4.0, 4.0, 3.0, 5.0; `0xC97E4` = 3 3 2 3 2 3 3,
  `0xC97EB` = 6 6 5 5 4 6 6, `0xC97F2` = `0x2D` ×7. The descriptors
  `0xBAB88`/`0xBAB9C` start with the sprite ids `0x788`/`0x780` and carry
  `+0x28` bit `0x800` (no stream walk at the spawn).

### 42-C.3 The port

`fight.c`: `fight_4b788` completed; new `fight_13134`, `fight_4bd98`,
`fight_4cb18` (exported for the future `0x4C60C`), `fight_4af04`,
`fight_4d898` and `fight_4d7a4` (exported for the future `0x4D2D0`); case 8's
held body and `0x4B470`'s tail wired. `actors.c`: `actors_link_held`
(`0x2BD20`), next to `0x2BCF4`. The voices are `PORT:` notes (spec §7). The
grab body is written out twice, as the raw has it.

### 42-C.4 The assertions

`check_point_trample`'s case F asserted the old gap (the grab move with
`+0x52` in range left the entry to case 4); it now asserts the grab (type 8,
`+0x1C` `0xC5`, `+0x20` 0, the lie timer untouched), with the streams below
patched. New `check_grab_arms` (`test_fight.c`) on the same fixture. The real
hold streams start with opcode words, so `gr_patch` gives `0xE7B02`/`0xE4744`
a plain sprite word 4 in place (inside `0x4AF04`'s ranges, and the fighters'
pset id, so the hit test keeps its sprite), points `0xC97AC[0][3]`/`[1][3]`
at scratch streams and gives `0xEF66A` a plain word; all are restored. It
covers: `0x4B788`'s grab for characters 0 (unflipped and flipped) and 1, every
field above, the return-0 gates and the `+0x52` bounds; case 8 held at both
ends of each character's range and released one past either end, characters
above 6 always held, the release's stores, no `+0x4A` link, the `+0x20` side;
the eighth hit (every `0x4BD98` store, both spawns, the `0x4CB18` launch
after case 6's step), the signed midpoint of −`0x1001` and 0, each `0x4BD98`
gate stopping only `0x4BD98` (both signednesses and every `0x13134` edge), a
side-1 hitter, each tail gate; `0x4CB18` directly (both flags, both sides,
negative height, the cap); `0x4D898`'s move gates for every character
(directly: the hit test's box moves for characters 5/6, `0x15B90`'s
adjustment), hit 2's slot, `0x4D7A4`'s missing gates, bit 7, both sides, the
release without the `0x1088B2` store, the full grab with its award, every
`0x4D880` weight, the cap for both awards, and character 1 with `0xB`.

The `0x1088B2[+0x21]` = 1 stores of case 8's release (`0x4A100`) and of
`0x4B69C`'s (`0x4B76E`, §29) are cleared by `0x4A634` only after the list
walk (`0x4A591`), but inside the walk `0x4AAD0` reads the byte for a later
type-0 entry of the same side (`0x4AB60`) and then takes `0x4B3F0`. The test
adds such an entry (`GR_SECOND`: side 1, si 3, `0xC955C[3]` a scratch
stream, 0x1000 from side 1's record so `0x4B144` stays out) after the
released one and asserts it becomes type 8 on `0xC955C[3]` with `+0x55` = 0,
for both stores, with a control where the holder still holds (type 0, the
stream untouched). The type-6 store in `0x4BD98` is always overwritten by
`0x4CB18`'s.

`check_grab_arms` frees the two `0x4BD98` spawns second-first, so the actor
free list `0x105B3C` keeps its order (a whole-`mem[]` diff around the test
shows the list's first three records `0x2A7E1C8, 0x2A7E230, 0x2A7E298`
before and after). It also restores the render node 21 pset field `0x1015E8`
(the node pool `0x10153C`) and the palette table's third refcount `0x10763C`
(`0x107618 + 0x24`), which the spawns and kills change; the diff then shows
only the fixture scratch, the two freed pool records' contents and the
pset-sync scratch `0x105BDC`/`0x105BE0`.

**Mutations** (`scratchpad/grab_mut.py`, `grab_mutlist.py`, `grab_mut2.out`):
146 single-site edits of the new code, its wiring and `0x2BD20` (each
dropped store or call, each table index forced to character 0, each sign,
bound and signedness, each gate, each `0x4D880` weight, the award's slot,
scale and cap, each `0x4AF04` edge and two swapped ranges). The first sweep
left five survivors (`0x4D898`'s `+0x52` bounds and `+0x4B` gate, then
untested) and one crash (`0x4D7A4` without its `+0x20` store left the seed
`0x77` as `0x1A570`'s side); the test gained those gates and a valid `+0x20`
seed (1). On that re-run 144 failed and two survived: calling `0x4CB18`
before `0x4BD98`, and dropping case 8's `0x1088B2` store, then wrongly called
unobservable. The review showed the store is read within the pass (above);
with the second entry added, that mutation fails 3 assertions and dropping
`0x4B69C`'s `0x4B76E` store fails 2 (`grab_mutlist2.py`). On the final
re-run (`grab_mut3.out`) 145 of the 146 fail 1..46 assertions and none crashes. The one survivor is equivalent: the
two functions write disjoint state apart from type 6, `0x4CB18`'s `&= 0x7F`
leaves `0x4BD98`'s bit 5, and their `0x2BC30` pre-walks act on different
records. The sources were restored by the script.

### 42-C.5 Measured and remaining gaps

- `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all checks passed, 0 compiler
  warnings. The drivers and `make verify` were not run (the batch controller
  runs them after the merge).
- No oracle is expected to move: the grab needs the fighter's slot `+0x5F`
  at its grab move while a lying worshipper's point is in its box (§29 found
  none in the demo), and the tail needs a worshipper's eighth hit and
  `[0x1088EF] >> 24` > 1.
- Remaining named gaps: the voices (`0x2C3FC` in `0x4B788`, `0x4D898`,
  `0x4CB18`); the mode-`0x21` frame `0x26540` that `0x4BD98`'s mode starts
  (and everything reading `0x108864`/`0x108868`/`0x10886C`/`0x1088C5` there);
  the mode-`0x22`/`0x24` pass `0x4D2D0` (so `0x4D7A4`/`0x4D898` have no port
  caller); `0x4C60C` (`0x4CB18`'s flag-1 caller).

## 42-B. The throw hold `0x3E244`/`0x3C358`, the fall landing `0x36280` and character 2's finishers `0x48AAC`/`0x48D94` (named-gap batch 2, branch `gap2-3c358`)

**Result in one line.** The named gap `0x3E244` (the `+0x1C` callback
`0x3E3A8` stores, §34/§41-B) is ported with its one unported callee
`0x3C358`; the unregistered stream target `0x36280` is ported; and the two
unported `0x370F0` callers `0x48AAC`/`0x48D94` (§41-C) are ported with their
unported callees `0x380C4` and `0x48C4C` and their only writers, the finisher
entries `0x48BE0`/`0x48F54`. No oracle window reaches any of them, so they
are unit-tested; no oracle is expected to move.

### 42-B.1 The raw (Ghidra disassembly/decompile, `read_memory` + capstone, fixups applied)

Ghidra has functions at `0x3C358`, `0x48AAC`, `0x48D94`, `0x380C4` and
`0x48C4C`, and none at `0x3E244`, `0x36280`, `0x48BE0` or `0x48F54`; those
were decoded with capstone from a `read_memory` dump of both objects
(`scratchpad/g3c358/tool.py`). The decompilations agree with the
disassembly except where noted.

- **`0x3C358`** (171 bytes, `0x3C358..0x3C402`; EAX = side; `push ebx;
  push edx`, EDX = EAX at `0x3C35F`, so the caller's EDX is unread):
  `0x33950(side)` on the stack. EDX = `0x1077B0 + side * 0x94`, EAX =
  `0x1077B0 + (1 - side) * 0x94`. The side's slot `+0x52` = 9, `+0x53` = 7,
  `+0x42 |= 4` (`0x3C390..0x3C3AB`); the other slot `+0x52` = 0x10, `+0x53`
  = 0x0A, `+0x54` = 0, `+0x0C` = 0 (`0x3C3AE..0x3C3BA`). The records
  `[0x1077B0]` and `[0x107844]` clear word `+0x34` and bytes `+0x43`/`+0x42`
  (`0x3C3C1..0x3C3E3`), then ctx[4]/ctx[5] (`[esp+0x10]`/`[esp+0x14]`) clear
  `+0x1C` (`0x3C3E7..0x3C3F6`). No other callee.
- **`0x3E244`** (226 bytes, `0x3E244..0x3E325`, `mov eax,eax` padding, then
  `0x3E328`; EAX = side; `push edx`, EDX = EAX at `0x3E248`):
  `0x33950(side)` (ctx[0] side, ctx[1] other, ctx[2]/ctx[3] the slots,
  ctx[4]/ctx[5] their records).
  - `0x34D8C(ctx[0])` with EDX = `0xE843A` (`0x3E254`); `0x34D8C` pushes and
    pops EDX (`0x34D8D`/`0x34DD7`), so `0x3C4CC(ctx[4], 0xE843A, 2.0)`
    (`push 0x40000000`, `0x3E267`) gets it.
  - `0x3E0F0(ctx[4])` (`0x3E270`).
  - `0x3C4CC(ctx[5], [0xC90F8 + 4 * ctx[3]+0x7A], 2.0)` (`0x3E275..0x3E291`;
    after the push `[esp+0x18]` is ctx[5]).
  - `0x3C208(ctx[0], word [0xC759C + 2 * ctx[3]+0x7A])`, zero-extended
    (`xor edx,edx; mov dx,...`, `0x3E2A2/0x3E2A4`). `read_memory 0xC759C`:
    `1300 0F00 1280 1400 1700 1300 0F00`.
  - `0x18AF8` (`0x3E2B4`), `0x39834(ctx[1], byte ctx[2]+0x5F)` (`0x3E2CA`).
  - `0x3C358(ctx[0])` with EDX = 0x309 (`0x3E2D2`; unread by `0x3C358`), then
    `0x39A10(ctx[4], 0x309)` and `0x39A10(ctx[5], 0x309)` (`0x3E2E0`/`0x3E2EE`).
  - The voice `0x2C3FC(word [0xC75AA + 2 * ctx[3]+0x7A])` (`0x3E30C`; out of
    scope, spec §7), then ctx[2]'s `+0x57` = 2 and ctx[3]'s `+0x53` = 0x0F
    (`0x3E315`/`0x3E31D`).
- **`0x36280`** (111 bytes, `0x36280..0x362EE`; the `nop` at `0x362EF`, then
  `0x36300`'s jump table `0x362F0`; EAX = rec; `push edx` and DL is loaded at
  `0x3628F` before its one use, EDX reloaded at `0x362A3`): ECX = rec+0x14
  (the owner slot); 0 returns. `+0x52 == 0x0D` sets `+0x58` = 3, else `inc
  byte [ecx+0x58]`. `0x188AC(byte rec+0x51, rec+0x18, 0)` (`0x362A8`;
  `0x188AC` pushes/pops ECX). Slot `+0x54` = 0, `+0x42 &= 0xFB`; rec `+0x36`
  word = 0, `+0x24` = 3.0 (`0x40400000`). `0x2AE14(0xBB1DC, EDX = slot+0x2C,
  ECX = [rec+0x30] sar 16, EBX = 0, a5 = 0)` (`0x362C7..0x362DB`; the slot's
  `+0x2C` is read after `0x188AC`'s latch). The voice `0x2C3FC(0x6F)`
  (`0x362E5`). `0xBB1DC`: stream `0xE8CA8`, type 0 (a stub cb1), handle
  `0x0105FF3C`.
- **`0x48AAC`** (306 bytes; EAX = ECX = slot, EDX = ESI = rec, EBX = side:
  the `0x3531C` case-7 registers): `0x33950(side)`. On slot `+0x57`:
  - 0 (`0x48AEA`): EAX = slot+0x2C - ctx[3]+0x2C, `neg` when negative, `cmp
    eax,0x100; jg` (signed) returns. Else the voice `0x2C3FC(word [0xC75AA +
    2 * ctx[3]+0x7A])`, `0x188DC(EBX = side, ctx[3]+0x2C)`, `DS_00104AE9 |=
    4`, rec `+0x34` word = 0, the voice `0x2C3FC(0x59)`, word
    `DS_00108390` = 0xF0, `inc byte [ecx+0x57]`.
  - 1 (`0x48ACF jbe` after `cmp al,1`): DX = word `0x108390` - 1 is stored;
    `test dx,dx; jg` (signed) returns. Else `0x380C4(ctx[1])` (`[esp+4]`),
    `0x3C190(byte rec+0x51, 0xFFFFFF80)`, `DS_00104AE9 &= 0xFB`, timer =
    0x3C, `inc byte [ecx+0x57]`.
  - 2: the same store and signed test, then `0x2BC30(rec, 0xEDD64, 2.0)`,
    `0x370F0(ctx[5])` (`[esp+0x14]` after the `ret 4`), slot `+0x53` = 3,
    `+0x52` = 9, byte `DS_001078FC` = 1 (AH).
  - above 2: returns, the decrement not stored (`0x48ADD jmp 0x48BD8`).
  - ECX = slot is used after the voice calls: `0x2C3FC` writes no ECX and each
    of its callees that does pushes/pops it (`0x1CA6C`, `0x1CC28`, `0x1CD9C`,
    `0x1CE04`, `0x1CE70`).
- **`0x380C4`** (144 bytes; EAX = side; its one call is `0x48B75`): rec =
  `[0x1077B0 + side * 0x94]`, char = the slot's `+0x7A`. `0x2BC30(rec,
  [0xBDDC4 + 4c], 1.0)`; a5 = (word rec+0x56 `| 0x400`) | (rec+0x28 bit 14 ?
  0x4000 : 0); `0x2AE14([0xBDDE0 + 4c], 0, 0, 0, a5)`; the spawn's `+0x59` =
  1 with no null test (`0x3814C`).
- **`0x48D94`** (446 bytes; the same registers, EAX = ESI = slot, EDX = EDI =
  rec): `0x33950(side)` in a 0x20-byte frame. `+0x57` above 4 returns; the
  jump table `0x48D80` reads `0x48F4B 0x48DBF 0x48DCF 0x48EFE 0x48F21`.
  - 0 returns (`0x48F4B`).
  - 1: `0x48C4C`, `+0x57` = 2.
  - 2 (`0x48DCF`): rec+0x28 bit 14 set: speed = rng(0x80) + 0x80, EBX =
    -0x2A00; clear: speed = -(rng(0x80) + 0x80), EBX = 0x2A00. y word =
    word rec+0x32 - 0x200 + rng(0x200) (`0x48E19..0x48E34`), read back as
    `[esp+0x16] sar 16` (sign-extended). `[0xC951C]` (0xC950C's `+0x10`) =
    `0x29C08(byte rec+0x51, that slot's +0x7A)`. `0x2AE14(0xC950C, EDX =
    movsx bx + [0xF0AF0], ECX = y, EBX = 0, a5 = rec+0x28 & 0x4000)`. A null
    spawn returns (`0x48E8F`). Else `0x2A17C(spawn, word rec+0x2E, 0)`, the
    spawn's word `+0x34` = speed, then `mov eax,[0x108395]; sar eax,0x18;
    cmp eax,8; jl` (the signed byte `0x108398`): at least 8 sets `+0x57` = 3,
    timer 0xB4, byte `0x108399` = 4; else timer = rng(4) + 4, `0x108399` = 2,
    `+0x57` = 3. Ghidra's decompile drops the >= 8 arm as "unreachable"
    (it does not model the byte); the raw bytes have it.
  - 3 (`0x48EFE`): the timer - 1 stored; signed `jg` returns; else `+0x57` =
    byte `0x108399`.
  - 4 (`0x48F21`): `0x2BC30(rec, 0xED4FA, 3.0)`, `0x370F0(ctx[5])`, `+0x53`
    = 3, `+0x52` = 9, `DS_001078FC` = 1.
  - `0xC950C`: stream `0xEDCEA`, type **0x2D** (byte 4), so its cb1 is
    `0x48CD8`, which pops the `0x1082E0` free list and counts `0x108398`.
- **`0x48C4C`** (107 bytes; its one call is `0x48DBF`): `[0x10836C]` =
  `[0x108368]` = `0x108368`, `[0x1082E4]` = `[0x1082E0]` = `0x1082E0`, then
  EBX = `0x1082E8` step 0x10 while below `0x108368`: `0x249C0(0x1082E0,
  EBX)` (eight nodes, in address order at the tail); bytes `0x108398`,
  `0x108397`, `0x108396` = 0; the voice `0x2C3FC(0xEF)`. It is `0x28E98`'s
  (§41-D) twin for type 0x2D.
- **`0x48BE0`** (106 bytes; EAX = slot, EDX = rec): `0x2BC30(rec, 0xEDD34,
  2.0)`, slot 7/9/0 (`+0x53`/`+0x52`/`+0x54`), `+0x0C` = `0x48AAC`, `+0x57`
  = 0, `+0x18`/`+0x1C`/`+0x14` = 0, `+0x42 |= 8`, `0x3C190(byte rec+0x51,
  0x80)`, the voice `0x2C3FC(0x4B)`, `mov al,1`.
- **`0x48F54`** (68 bytes): `0x2BC30(rec, 0xEDC82, 2.0)`, 7/9/0, `+0x0C` =
  `0x48D94`, `+0x57` = 0, `+0x18`/`+0x1C`/`+0x14` = 0, `mov al,1`. Its
  stream carries `D100 000156D4` (`0xEDCA4`); `0x156D4` is `mov eax,[eax+0x14];
  test; je; inc byte [eax+0x57]; ret`, the step that takes `0x48D94` from 0
  to 1.

### 42-B.2 Entrances (`get_xrefs_to`, a rel32/jcc scan of the code object and a dword scan of both objects)

- `0x3C358`: six rel32 calls, `0x2127A`, `0x225AE`, `0x3E2D7`, `0x44C36`,
  `0x47AA5`, `0x47DB3`; no dword. `get_xrefs_to` agrees. Only `0x3E2D7` is
  in code the port has.
- `0x3E244`: one dword, in the code object at `0x3E40A` (the immediate of
  `0x3E3A8`'s `+0x1C` store at `0x3E407`); no rel32, none in data.
  `get_xrefs_to` is empty. So it runs only through `0x193B0`'s `+0x1C` call
  (`0x19505`, EAX = side), the `fn(side)` shape of `0x3E4C4`/`0x14D7C`.
- `0x36280`: no rel32 and no code dword; 13 data dwords, each the operand of
  a `D000` word: `0xD2750`, `0xD2774`, `0xD435A`, `0xD43B0`, `0xE0C8E`,
  `0xE0CDA`, `0xE4098`, `0xE40E4`, `0xE73EC`, `0xE744A`, `0xEACF6`,
  `0xEAD40`, `0xED156`. They lie in the streams of `0xC90A8` (0x361C8's
  `+0x52` = 12 table: `0xE73E0 0xE4074 0xED14A 0xD2764 0xEACB2 0xD434E
  0xE0C6A`) and `0xC90D0` (0x36300's `+0x52` = 13 table: `0xE7424 0xE40C0
  0xED14A 0xD2740 0xEAD36 0xD438A 0xE0CB6`); char 2 shares `0xED14A`, hence
  13. `get_xrefs_to` is empty.
- `0x48AAC`: one code dword `0x48C06` (`0x48BE0`'s `+0x0C` store at
  `0x48C03`); `0x48D94`: one code dword `0x48F77` (`0x48F54`'s store at
  `0x48F74`). No rel32, nothing in data. `get_xrefs_to` gives the same.
- `0x48BE0`: one data dword `0xBDAEC` = `0xBDAE4[2]`; `0x48F54`: one data
  dword `0xBDB08` = `0xBDB00[2]`. `0xBDAE4` reads `0 0 0x48BE0 0x1567C
  0x45C10 0 0x23BF8` and `0xBDB00` reads `0x402FC 0 0x48F54 0x15908 0x45D14
  0x40BBC 0x23EC0`, each read once: at `0x377D6` in `0x37774` and at `0x378FB`
  in `0x37898`, which store the entry in `DS_001078E8`. `0x37774`/`0x37898`
  are the reaction callbacks 0x33/0x34 of every character (dwords at
  `0xA3924`/`0xA3938` + 0x500 * c, no rel32, no Ghidra function). The
  ported `0x379C4` calls `[0x1078E8]` at `0x379E8` with EAX = slot, EDX =
  rec and tests the whole EAX (`0x379EE`); both setters return a non-zero EAX
  (`mov al,1`), so the port's wrapper returns 1.
- `0x380C4`: one rel32, `0x48B75`. `0x48C4C`: one rel32, `0x48DBF`. No
  dword for either.
- `0x156D4`: 8 data dwords, each after a `D100` word (`0xD32BE`, `0xD32D6`,
  `0xD3326`, `0xD334A`, `0xD3362`, `0xE1BE2`, `0xEB83A`, `0xEDCA4`); no code
  reference.

### 42-B.3 The port

- `fighter.c`: `fighter_3c358` and `fighter_3e244` (public, after
  `fighter_3c208`), `fighter_36280` (public, before `fighter_state_36300`),
  and at the end `fighter_380c4` (static), `fighter_48aac`, `fighter_48d94`,
  `fighter_48be0`, `fighter_48f54`. Local `#define`s name `0xBB1DC`,
  `0xE843A`, `0xC90F8`, `0xC759C`, `0xBDDC4`, `0xBDDE0`, `0xEDD64`,
  `0xED4FA`, `0xC950C`, `0xEDD34` and `0xEDC82`. The voices stay `PORT:`
  stubs. `fighter_3e3a8`'s `PORT:` note on `0x3E244` is removed.
- `actors.c`: `actor_type_2d_list_init` (`0x48C4C`, next to `0x28E98`'s
  port), `anim_code_36280` (drops the operand), and `fn_register` of
  `0x3E244` (`fn(side)`), `0x36280` (the opcode `(rec, operand)` shape),
  `0x48AAC`/`0x48D94` (`(slot, rec, side)`, `0x3531C` case 7) and
  `0x48BE0`/`0x48F54` (`0x379C4`'s `(slot, rec)` returning non-zero).

### 42-B.4 The assertions and mutations

Three checks in `test_fight.c`, after `check_throw_3c208`. Each saves the
whole data object (`0x80000..0x10B0CF`) and both real pools (records
`0xEBA0`, psets `0x4880` bytes) at entry and restores them at exit
(`g2_save`/`g2_restore`, the `check_char_screen_setup` method), so every
global, table patch and pool record they touch is put back; the rest of their
writes land in the test-owned `FIGHT_RECS`/`FIGHT_ACTORS` scratch. Streams
the functions start are patched (inside that restore) to one-word frames, so
`0x2BC30` stops at once; descriptors whose palette handle is not under test
get handle 0.

- `check_hold_3e244` (94 assertion sites). `g2h_seed` builds on `c3_seed`
  with the T-rex (char 0) on side a and char 4 on the other side, x 5000 and
  1000 over the f = 772 offsets, `DS_001078FA` = 2 and sentinels in every
  written field; the `0xC90F8` table and `0xE843A`'s first word are frame 0
  (sprite 0 keeps `0x18540`'s anchor at 0, so the latched x are the seeded
  ones; the real sprite `0x10BA` moved them). A/A2: `0x3C358` both ways. B:
  `0x3E244(0)`: the flash pair, both streams and holds, the child (count,
  `+0x4B`, `+0x60`, `0xBB3F8`'s `+0x10`), the placement (want `0x1700`: the
  closer arm moves side 1 from 1000 by -1888 to -888), `0x39834`'s
  `0x107D28`/`0x107D2C`, both `+0x74`, `0x3C358`'s writes, `+0x57` = 2 and
  `+0x53` = 0x0F. C: the side-1 mirror. D: a crossing (the T-rex at 4000,
  side 1 at 5000 moved by -4888 to 112) so that `0x3E244`'s own `0x18AF8`
  flips both `+0x29` bit 6 after `0x3C208`'s pre-move call.
- `check_land_36280` (39). Through the registered target: `+0x58` = 3 at
  `+0x52` 0x0D, `+0x58` + 1 at 0x0C, the anchor and latch (x 3000 over the
  `0xDEADBEEF` sentinel), `+0x54`, `+0x42`, `+0x36`, `+0x24`, the spawn's
  x/y/`+0x32` (0xFEDC, the `sar 16` of `0xFEDC1234`) and a5 = 0; no owner
  slot writes nothing; a third record owned by slot 0 with `+0x51` = 1
  anchors side 1 and spawns at slot 0's unlatched x.
- `check_finisher_48aac` (161). `0x48AAC`: the 0x100 window both ways and at
  the edge, the signed compare (`0x80000000`), the passed record (not the
  slot's) taking `+0x34`, the timer ticks with the signed test (`0x8001`),
  `0x380C4` (stream, 1.0, the child's `+0x59`, `+0x4A` = the record's
  `+0x56`, `+0x28` bits 14/10, the parent's `+0x4F`), `0x3C190`'s side from
  `rec+0x51`, state 2's `0x370F0(ctx[5])`, above 2 not storing the tick, and
  the `0x3531C` case-7 call. `0x48D94`: 0 and 5 store nothing and draw
  nothing; 1 builds the lists (every link, the untouched `+8`s and margins,
  bytes `0x108396..98`); 2 spawns with the draws replayed from the seed
  `0xA1943569` (chosen so that its first draw's high word is `0xFFFF`,
  where ranges 0x7F/0x80/0x81 give different values), both flip arms, the
  pset word `0x0923`, the palette entry held (refcount 5 -> 6),
  `DS_000C951C`, the count 7 -> 8 arm (no third draw), the signed count
  (0x7F + 1 = -128 takes the rng arm), and a refused spawn; 3's ticks and
  signed test; 4; and the whole machine through `0x3531C` case 7 from
  `+0x57` = 1: eight spawns, count 8, the `0xB4` wait, state 4 (+0x53 = 3).
  `0x48BE0`/`0x48F54` through their registrations as `0x379C4` calls them.

**Mutations** (`scratchpad/g3c358/mut.py`, `mut.log`): 173 single-site edits of the new
code and its registrations (each constant, side index, context slot, field
offset, signedness, dropped store or call, table base and registration),
each rebuilt and run with `PR_ORACLE_REQUIRED=1`.
- The first sweep (146) left five survivors and two crashes. Dropping
  `0x3E244`'s own `0x18AF8` survived because `0x3C208` had just set the same
  flags; case D (the crossing) now kills it. `rng(0x81)`/`rng(0x7F)` for
  `rng(0x80)` survived the seed `0x12345678` (the draws agree), so the seed
  is `0xA1943569`. `0x29C08(1 - side, ...)` survived because both sides'
  `0x105B34` bytes matched; E sets side 1's to 0. Dropping the `0x108368` or
  `0x1082E4` store in `0x48C4C` crashed on the `0xA5` fill; the region now
  holds a scratch address (and `g2f_seed` zeroes it), so both fail cleanly.
  One mutation did not build and was rewritten.
- The re-runs (the `0x48D94`/`0x48C4C` groups, the fixed survivors and 27
  edits of the two entries `0x48BE0`/`0x48F54`) fail 2..38 assertions each.
- One survivor is equivalent: dropping `0x48C4C`'s `[0x1082E0]` = `0x1082E0`
  store (`0x48C70`). The first `0x249C0` reads the prev link `[0x1082E4]`
  (the sentinel itself) and writes the sentinel's next, so the store is
  always overwritten before any read, as §41-D found for `0x28E98`'s twin.
- A final full sweep of all 173 on the final tests: 172 fail (1..32
  assertions each, none by a crash), the one equivalent survives.
The sources were restored by the script and checked with `cmp`.

### 42-B.5 Measured and remaining gaps

- Build with 0 warnings; `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all checks
  pass. The drivers were not run (the batch controller runs the ladder).
- No oracle is expected to move. `0x3E244` needs `0x193B0`'s `+0x1C` call
  after `0x3E3A8`, which the port's run does not reach (§34/§35);
  `0x36280` needs a `+0x52` = 12/13 fall stream to reach its `D000` word,
  and no whole-run `fn_resolve` probe (§20.4 through §40) reported it missing;
  the finisher chain needs reaction 0x33/0x34, whose callbacks are unported,
  so `DS_001078E8` stays 0 and `0x379C4`'s call arm is dead.
- Remaining named gaps:
  - `0x37774`/`0x37898`, the reaction-0x33/0x34 callbacks that arm the
    finisher entries (and the rest of the finisher flow: `DS_001078E4`,
    `0x104B02`, `0x41310`, `0x37D18` for char 6).
  - `0x156D4`, the `D100` target that steps `+0x57` (8 sites, among them
    `0x48F54`'s `0xEDCA4`), so `0x48D94` waits at 0 in the port.
  - The process-table entries the finishers enable: `0x48AAC` sets
    `DS_00104AE9` bit 2 (entry 10, `0x28F08`) and the type-0x2D cb1 sets
    `DS_00104AE8` bit 1 (entry 1, `0x48F98`); both are unregistered (§41-D;
    since ported and registered, §46-D).
  - The other five `0x3C358` callers (`0x2127A`, `0x225AE`, `0x44C36`,
    `0x47AA5`, `0x47DB3`) lie in code the port does not have.
  - The voices `0x2C3FC(0x6F/0x59/0x4B/0xEF)` and the `0xC75AA` character
    voices (spec §7).

  unported siblings `0x22CE4`, `0x22D8C` and `0x2365C` (all three since
  ported, §42-A) and the other `0x370F0` callers `0x48AAC`/`0x48D94`.

## 42-A. Update-table entry 7 `0x2910C` and character 1's second freeze `0x22CE4` (named-gap batch 2, branch `gap2-2910c`)

**Result in one line.** `0x2910C`, the type-`0x0A`/`0x19` node walk that
§41-D.5 left unregistered, is ported and registered as update-table entry 7.
The rest of the projectile-freeze family is ported too: `0x22CE4` (the second
way into the `0x22BEC` freeze), the `+0x18` hook `0x22D8C` and the reaction
callback `0x2365C`. Those three are reachable only through code that §41-C did
not name, so that code is ported with them: character 1's reaction-`0x29`
callback `0x22F74` (it stores `0x22D8C`), its `+0x1C` callback `0x22E44` (the
only caller of `0x22CE4`), its `+0x0C` callback `0x22F14`, update-table entry 5
`0x22FE8` (which `0x22E44` enables) and `0x2A148`. That is 9 functions and
1 593 raw bytes. A probe run of the front-end driver's window is identical,
frame for frame, to the base's, so no oracle is expected to move.

### 42-A.1 The raw (`read_memory` + capstone; Ghidra disassembly where it has a function)

Ghidra has functions at `0x22CE4` and `0x22D8C` only. The others were decoded
with capstone from `read_memory` (fixups applied).

- **`0x2910C`** (415 bytes, `0x2910C..0x292AA`; pushes EBX, ECX, EDX, ESI and
  EDI; EAX, the table index, is not read):
  - EBX = `[0x104880]`; the walk ends when it is `0x104880` (`0x29111..0x2911D`,
    `0x29297..0x2929F`). ESI = the node's next, read at `0x29131` before any
    call.
  - EAX = the node's `+8` (the record). EDX = `[rec+0x34] sar 16` (the signed
    word `+0x36`) + `[rec+0x1C]`, a 32-bit add; `test edx,edx; jge 0x291A7`
    (`0x29126..0x29135`).
  - Below 0 (landed): `+0x1C` = 0 (dword), words `+0x34`, `+0x36`, `+0x44` = 0,
    bytes `+0x48` = 0 and `+0x59` = `0xFE` (`0x29137..0x29163`), each store
    re-reading the node's `+8`; `0x2BC30(rec, 0xE8D50, 0x40400000)`
    (`0x29167..0x29174`; `0x2BC30` keeps EBX, ECX and ESI). Then EBX = the
    node's `+8`, EDI = `[rec+0x14]`; when non-zero, `0x249D0(EDI)`,
    `0x249B0(0x104888, [rec+0x14])` and `[rec+0x14]` = 0 (`0x2917C..0x2919B`),
    the teardown `0x290D0`'s body.
  - Otherwise (`0x291A7`): DX = word `+0x34`, AX = word `+0x36`, CL = the node's
    `+0x0C`; `cmp cl,3; ja 0x29297`; `jmp [0x290FC + ecx*4]`. The table reads
    `0x291C9`, `0x29219`, `0x29261`, `0x29297`.
  - Phase 0 (`0x291C9`): ECX = |movsx AX|, EAX = |movsx DX|; `cmp ecx,eax; jge`
    skips; else `0x2BC30(rec, 0xE8D14, 0x40000000)` and phase = 1.
  - Phase 1 (`0x29219`): EAX = |cwde AX|, EDX = |movsx DX|; `cmp eax,edx; jle`
    skips; else `0xE8D28` at 2.0 and phase = 2.
  - Phase 2 (`0x29261`): EDX = 2 |movsx DX|, EAX = |cwde AX|; `cmp eax,edx;
    jle` skips; else `0xE8D3C` at 2.0 and phase = 3. Phase 3 (`0x29297`) holds.
  - The absolute values are 32-bit (`neg` after `movsx`), so |`0x8000`| is 32768.
- **`0x22CE4`** (167 bytes; EAX = side, kept in ECX; EDX pushed and
  overwritten): the `0x33A10` context (`0x22CF0`); `0x33ACC(ctx[1], 0x104530 +
  ctx[1] * 0x94, EBX = 0x104658 + ctx[1] * 0x68)` (`0x22CF5..0x22D2D`);
  `0x39834(ctx[1], byte ctx[2]+0x5F)` (`0x22D32..0x22D43`); ctx[2]'s `+0x57` = 2
  (`0x22D4C`); ctx[3]'s `+0x52` = `0x10`, `+0x53` = `0x0A`, `+0x10` =
  `0x22BEC`, `+0x5F` = `0xFF` (`0x22D50..0x22D6F`); `xor cl,1`; `0x22B28(EAX =
  &ctx)` (`0x22D78`); byte `[ecx + 0x10476A]` = 1 (`0x22D7D`). `0x33A10`,
  `0x33ACC` (`push ecx`), `0x39834` (`push ecx`) and `0x22B28` (`push ecx`)
  preserve ECX, so the byte is `0x10476A[side ^ 1]`. Unlike `0x235C4` it does
  not clear `+0x18`/`+0x1C`.
- **`0x22D8C`** (181 bytes; EAX = side): the `0x33950` context; EDX = 3 when
  ctx[2]'s word `+0x76` is non-zero, else 6 (`0x22D9F..0x22DAD`); `0x18BD4` on
  the flags at `[esp+0x18]` (it writes only the 16 flag bytes, so EDX survives);
  flags 1, 8, 0xE, 0xD = BL = 0 and 5, 9 = AH = 1 (`0x22DBB..0x22DD7`).
  - ctx[3]'s `+0x10` == `0x22BEC` returns 1 (`0x22DDB..0x22DE9`).
  - EBX = `[side * 2 + 0x10474E] sar 16` (the signed word `0x104750[side]`);
    `cmp ebx,0x10; jg` returns 1; `cmp ebx,edx; jge 0x22E0E`, else 1
    (`0x22DF0..0x22E0D`).
  - `cmp word [side * 2 + 0x107D2C],0; jle`: positive gives ECX = `0xA8332`,
    EBX = `0xA8328`, else ECX = `0xA831E`, EBX = `0xA8314`; then
    `0x18C14(side, &flags, EBX, ECX)` and its EAX is returned (`0x22E0E..
    0x22E35`). The four tables read `69`x7, `96`x7, `8C`x7 and `C7`x7.
- **`0x22E44`** (207 bytes; EAX = side): the `0x33950` context; ctx[2]'s `+0x57`
  = 2; `0x22CE4(ctx[1])` with EDX = `0x29A` (`0x22E5B..0x22E64`), which
  `0x22CE4` pushes and pops, so `0x39A10(ctx[4], 0x29A)` follows (`0x22E69`).
  AL = `0x1A570(ctx[0])` gives AX = `0xF000` when non-zero, else `0x1000`. Then
  `push 0; ECX = [ctx[4]+0x30] sar 16; EDX = movsx AX + [ctx[4]+0x18]; EAX =
  0xBB3E4; EBX = 0xFFFFCC00; call 0x2AE14` (`0x22E8A..0x22EAC`, `ret 4`).
  `0x104728[ctx[0]]` = the record; its `+0x14` = ctx[2], word `+0x36` = `0x200`,
  byte `+0x59` = `0xFE`; byte `0x10476A[ctx[0]]` = 0; `0x2A148(it, EDX = 0)`;
  the voice `0x2C3FC(0xB6)`; `[0x104AE8] |= 0x20` (`0x22EB1..0x22F05`). The
  spawn result is not checked.
- **`0x22F14`** (96 bytes; EAX = slot, EDX = rec, EBX = side; only EBX is
  read): the `0x33950` context; `inc word [side * 2 + 0x104750]`; on ctx[2]'s
  `+0x57`: below 1 or above 2 `0x62003(1)`; 2 returns; 1 returns while
  `[side * 2 + 0x10474E] sar 16` <= `0x10`, else `+0x57` = 2 and `+0x8A` = 0.
- **`0x22F74`** (115 bytes; only EBX = side is read): the `0x33950` context;
  ctx[2]'s `+0x18` = `0x22D8C`, `+0x1C` = `0x22E44`, `+0x0C` = `0x22F14`,
  `+0x57` = 1, `+0x52` = 9, `+0x53` = 7, `+0x54` = 0; word `0x104750[side]` =
  0; `0x3C4CC(ctx[4], 0xE4952, 0x40400000)`; AL = 1.
- **`0x22FE8`** (240 bytes; pushes EBX..EBP; EAX not read): `[esp]` = 1, EBP =
  `0xE8E66`; for ECX = 0, 1 (EBX = ECX * 4):
  - ESI = `0x104728[ecx]`; 0 skips. EAX = it, ESI = its `+0x14`, `[esp]` = 0
    (`0x23010..0x23017`), then a 0 slot skips.
  - EDX = `[0x1077A8 + ((byte [[slot]+0x51] ^ 1) & 0xFF) * 4]`; 0 jumps to the
    epilogue `0x230CE`, past the bit clear.
  - EDI = `[rec+0x34] sar 16` + `[rec+0x1C]`; `jl` skips; else `+0x1C` = 0 and
    word `+0x36` = 0 (`0x2303F..0x23059`).
  - AL = `0x10476A[ecx]`: 0 retires unless the slot's `+0x53` is 7; 1 retires
    unless EDX's `+0x10` is `0x22BEC`; other values skip. Retiring is
    `0x2BC30(0x104728[ecx], 0xE8E66, 0x40400000)` and `0x104728[ecx]` = 0.
  - After the loop, `[esp]` != 0 clears `[0x104AE8]` bit 5 (`0x230C1..0x230C7`).
- **`0x2365C`** (121 bytes; EAX = slot in ECX, EDX = rec in ESI; EBX not read):
  a non-zero `[slot+8]` returns AL = 0; EDX = byte `rec+0x51` ^ 1 (EDX was the
  zero `[slot+8]`), the slot `0x1077B0 + EDX * 0x94`; its `+0x10` ==
  `0x22BEC` returns 0; `0x3C4CC(rec, 0xE4996, 0x40400000)`; `+0x52` = `0x0B`,
  `+0x53` = 6, `+0x54` = 0, AL = `+0x5F`, `+0x0C` = 0, `+0x64` = AL, `+0x5F` =
  `0xFF`; the voice `0x2C3FC(0xB4)`; AL = 1.
- **`0x2A148`** (51 bytes; EAX = rec, DL = flag): `+0x5F` = DL; the pset's `+2`
  word = word `+0x2E` OR (`+0x5F` != 0 ? `0x800` : 0).
- The streams: `0xE8D50`, `0xE8D14`, `0xE8D28`, `0xE8D3C`, `0xE8E66` and
  `0xE4996` begin with the word `0xCD40`, and `0xE4952` with `0xDC00` (its dword
  `0xE25A8`) and then `0xED40`. `0xBB3E4` is `{0xE8E64, type 0, frame 0, +0x2E
  = 0x18, flags 0x0100, extent 0x28, +0x2C = 0x1000, handle 0x0105FF3C}`, and
  `0xE8E64` starts with the sprite word `0x0732`.

### 42-A.2 Entrances (`get_xrefs_to`, a rel32 call/jmp/jcc scan of the code object and a dword scan of both objects)

| address | references |
|---|---|
| `0x2910C` | data dword `0xA8660` only (update table `0xA8644` entry 7) |
| `0x22FE8` | data dword `0xA8658` only (entry 5) |
| `0x22F74` | data dword `0xA3D5C` only: `0xA3528 + 0x500 + 0x29 * 0x14`, character 1's reaction `0x29`, stream pointer 0 |
| `0x2365C` | data dword `0xA3D70` only: character 1's reaction `0x2A` |
| `0x22D8C` | code dword `0x22F87` only (the `+0x18` store in `0x22F74`) |
| `0x22E44` | code dword `0x22F92` only (the `+0x1C` store) |
| `0x22F14` | code dword `0x22F9D` only (the `+0x0C` store) |
| `0x22CE4` | one rel32 `call` at `0x22E64` (`0x22E44`) |
| `0x2A148` | rel32 `call`s at `0x22EF6`, `0x23F4D`, `0x40A4E` |

The update table's dispatch is `0x24CE6..0x24CFC` (`call [eax + 0xA8644]` with
EAX = index * 4), so entries 5 and 7 take the port's `fn()` shape. The
reaction callbacks take 0x34E2C's `(slot, rec, side)` shape and their AL is
dropped, as for `0x3BF70`. `0x22D8C` is `0x19020`'s `fn(side)`, `0x22E44` is
`0x193B0`'s `fn(side)` at `0x19505`, and `0x22F14` is `0x3531C` case 7's
`(slot, rec, side)`.

### 42-A.3 The port

- `actors.c`: `actor_type_0a19_update` (`0x2910C`, static) and
  `actor_pset_flag_5f` (`0x2A148`). The wrappers `reaction_cb_22F74` and
  `reaction_cb_2365C` drop the AL. `actors_init` registers `0x2910C`,
  `0x22FE8`, `0x22F74`, `0x2365C`, `0x22F14`, `0x22D8C` and `0x22E44`.
  `ACTOR_2910C_*` name the four streams.
- `fighter.c`: `fighter_22ce4`, `fighter_22d8c`, `fighter_22e44`,
  `fighter_22f14`, `fighter_22f74`, `fighter_22fe8` and `fighter_2365c`, with
  local `#define`s for `0x104728`, `0x104750`, `0x10476A`, the four box tables,
  the descriptor `0xBB3E4` and the streams `0xE4952`, `0xE8E66` and `0xE4996`.
  `symbols.h` names none of them. The voices `0xB6`/`0xB4` and `0x22F14`'s
  `0x62003(1)` are `PORT:` stubs.

That is 9 functions and 1 593 raw bytes (415 + 167 + 181 + 207 + 96 + 115 + 240
+ 121 + 51), and 7 new registrations.

### 42-A.4 The assertions and mutations

Four groups in `test_fight.c`, run after `check_freeze_235c4`. Each group saves
the whole data object, `FIGHT_RECS..+0x6400` and the psets `FIGHT_ACTORS..+0x100`,
and puts them back.

- `check_type_0a19_update` (`0x2910C` through its registration, and
  `0x2A148`). The update-table dword and the four streams' `0xCD40` heads are
  read from the data. `0x2BC30` stops its pre-walk on the `0xCD40` word
  (opcode `0x0D`), and `0x2A408` reads that word as a variable sprite whose
  operand follows, so `rec+8` ends at the stream + 2. Round A:
  - A lands: `+0x34` is positive, and the signed `+0x36` (-`0x20`) against y
    `0x10` gives -`0x10`. Every zeroed field was seeded non-zero. The stream is
    `0xE8D50` at 3.0, and the node moves to the head of a one-node free list.
  - B sits on the 0 edge and moves from phase 0 to 1.
  - C holds on |vy| == |vx|.
  - D holds on |`0x7FFF`| < |`0x8000`|.
  - E moves from phase 2 to 3.

  Round A2:
  - A lands from y -`0x100` + `0xFF` into an empty free list.
  - B moves from phase 1 to 2, and C holds at 2 on |vy| == 2|vx|.
  - D (phase 3) and E (phase 5) hold.
  - G lands with `+0x14` = 0, so its node stays in the in-use list.
  - H holds at phase 1 on |vy| == |vx|.

  B: `0x2A148` with flag 1 and flag 0.
- `check_freeze_22ce4`: `0x22CE4(1)` and `(0)` on §41-C's `fz_seed` plus
  sentinels on `+0x57`, `+0x5F` and the three arrays. It checks both snapshot
  halves, `DS_00107D28` = the other slot's `+0x5F` (`0x28`/`0x29`), the
  `0x39834` side: its `ctx[0]` is `side ^ 1`, so only that side's
  `DS_00107D2C` hit word advances (seeded 3/5, which keeps `0x39865`'s scale
  on the `0xBEBF8` arm and `0x39973`'s store off) and the other side's
  `DS_00107D2C`/`DS_00107D20` words keep their sentinels; the other
  slot's `+0x57`, the frozen slot's fields with `+0x18`/`+0x1C` kept,
  `0x22B28`'s writes, and `0x10476A[side ^ 1]` = 1 with the other byte kept.
  Then `0x22E44` through its registration, sides 0 and 1, with a one-record
  actor free list at pool index 3. It checks:
  - the freeze of the other side, and `+0x57` and `+0x74` = `0x29A`;
  - `0x104728[side]` with the other entry kept;
  - the record's `+8`, `+0x14`, x at `-0x1000` (side 0's actor bit 15 clear)
    and `+0x1000` (set), y `0xFFFFCC00`, height `0x42`, `+0x36`, `+0x59`,
    `+0x5F` = 0 and pset `+2` = `0x18`;
  - `0x10476A[side]` = 0, and `DS_00104AE8` `0x41` -> `0x61` and 0 -> `0x20`.
- `check_hook_22d8c`: `0x22D8C` on `check_slot_hook`'s `sh_seed`. Slot 0 is
  latched from its record, and slot 1 through `0x18540` (`DS_001077A8[1]` = 0,
  anchor current, zero screen offsets). `0x3B298` is held at 0. The group
  checks:
  - the frozen gate (the other slot, not the self);
  - `0x19020` storing `hook == 0`;
  - the box pair against `DS_00107D2C[0]` = 1, 0 and `0x8000`, using |x| at
    `0x2000`/`0x1800` and |y| at `0x2400`/`0x2800`/`0x31C1`;
  - the tick window, 2/3/`0x10`/`0x11`/`0xFFFF` with `+0x76` set and 5/6/`0x10`
    with it clear, while side 1's tick `0x40` is not read;
  - flags 8, 9, 1, `0xD` and `0xE` each firing (`0xE` through `0x3B298`
    returning 1 with side 1's command word `0x2000`).

  `0x22F14` goes through its registration with slot 1 and record 1 in EAX/EDX
  and side 0 in EBX. It checks the tick, the `0x10` -> `0x11` edge with
  `+0x8A`, the signed `0x7FFF` -> `0x8000` hold, `+0x57` = 0 only ticking, and
  side 1's own word.
- `check_char1_reactions`: the table dwords `0xA3D5C`/`0xA3D70`/`0xA8658`.
  - `0x22F74` through its registration: the arming. Record 0's `+0x24` =
    3.0, and `rec+8` = `0xE495C`: the `0xDC00` opcode takes its dword, then
    `0x2A408` steps over the `0xED40` table sprite and its dword.
  - `0x2365C`:
    - the `+8` gate;
    - the other slot taken from the record's `+0x51` (EBX = 0 is ignored);
    - the slot's own `+0x10` not gating;
    - the arming, with `+0x5F` moved to `+0x64`.
  - `0x22FE8` through its registration:
    - both entries empty clears bit 5;
    - retiring at the floor edge;
    - the `+0x53` = 7 hold, with nothing zeroed above the floor;
    - the `0x10476A` = 1 gate on the other slot's freeze, and 2 holding;
    - the missing-slot early return, which leaves entry 1 untouched and keeps
      bit 5;
    - a slot-less entry keeping bit 5, with entry 1 live and alone (C7b);
    - entry 1's other slot taken through its record's `+0x51`.

The four groups hold 255 assertion sites.

**Mutations** (`scratchpad/g2910c/mut.py`, `mut.out`/`mut2.out`): 125
single-site edits of the new code and its seven registrations. They cover
every store, constant, index, gate, edge, signedness, stream and frame bit,
each flag of `0x22D8C`, each box pair, and most `ctx` slot choices. They
missed one: the review found that `0x22CE4`'s `0x39834(ctx[1], ...)` ->
`ctx[0]` passed the suite. The group asserted only `DS_00107D28`, which
`0x39834` writes for either side.
- The first sweep: 119 failed 1..32 assertions. Two were killed only by a
  fault:
  - `0x2910C` without its `+0x14` test: `0x249D0(0)` faults (SIGBUS).
  - `0x22E44`'s `0x104728` index moved to `ctx[1]`: it dereferenced the
    `0x77777777` sentinel.

  One was killed by a hang: `0x2910C` reading the next node after the landing.
  The walk then follows the moved node into the free list, which never reaches
  `0x104880`, so the run timed out at 900 s. Three survived.
- The test was then changed:
  - The `0x104728` sentinels became zeroed scratch records
    (`FIGHT_RECS + 0x6300`/`0x6380`), so the index mutation now fails 8
    assertions.
  - `0x22FE8`'s C7b (a slot-less entry alone keeps bit 5) was added. It kills
    moving 0x23017's `[esp]` = 0 after the slot test, which the first sweep
    missed because entry 1 was live in C7.

  The `0x22E44` and `0x22FE8` mutations (60..80, 94..110) were re-run on the
  final test. 37 of the 38 fail 1..14 assertions, with no fault and no
  timeout; the survivor is the equivalent `0x22E44` `+0x57` drop below.
- Two equivalent survivors remain:
  - `0x22D8C`'s tick read unsigned: every value outside 3..`0x10` returns 1
    whether it reads as negative or as large.
  - Dropping `0x22E44`'s own `+0x57` = 2 (`0x22E57`): the `0x22CE4(ctx[1])`
    call that follows stores 2 to the same byte at `0x22D4C`. Its ctx[2] is
    `slot[1 - (1 - side)]`, the caller's own slot.

- After the review, `q42_fz_seed` puts sentinels on both words of
  `DS_00107D2C` and `DS_00107D20`. The group now asserts which word
  `0x22CE4(1)`/`(0)` advances and that the other side's words are kept. The
  `0x39834` side mutation (126th) now fails 6 assertions. §41-C's
  `fighter_235c4` has the same unpinned `0x39834(ctx[1], 0x2A)` side (review
  note, left as follow-up).

  The remaining fault-killed and hang-killed mutations cannot be turned into
  assertion failures without touching low memory or bounding the walk. The
  script restored the sources, and `git diff` shows only the intended
  changes.

### 42-A.5 Measured, and the remaining gaps

- `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all checks passed, 0 compiler
  warnings (Debug and Release). The drivers and `make verify` were not run.
- **The probe.** A scratch harness (not in the repo; it writes no dump)
  repeats `test_frontend`'s setup: `game_init`, the anim-tick pin, the RNG,
  frame and cycle seeds, state 2 phase 0 and the pick bytes. It then runs its
  `FE_LOOPS` = 3500 loops. It was linked once against `libprage_core.a` built
  at `632f3cd` and once against this branch. Each loop logs the state, a hash
  of the presented buffer, a hash of the whole data object `0x80000..0x10B0CF`
  and the §42-A observables. The two 3500-line logs are byte-identical.
  - `DS_00104AE8`'s low byte is 0 on every loop, so entries 5 and 7 never run.
  - `0x104728` stays 0.
  - The second demo's side 1 (character 1, the ape) never has `+0x5F` =
    `0x29`/`0x2A`.

  So no oracle is expected to move at the current pins. The controller's
  ladder is the check.
- **Named gaps.**
  - The voices: `0x2C3FC(0xB6)` at `0x22F00` and `0x2C3FC(0xB4)` at
    `0x236CB` (spec §7).
  - `0x22F14`'s `0x62003(1)` for `+0x57` outside 1..2 (the runtime's error
    exit).
  - `0x236D8`, the `0xD100` target at `0xE49A2` in `0x2365C`'s stream
    `0xE4996` (the dword at `0xE49A4` is its only reference). It is
    unregistered, so the port's `anim_indirect` skips it. It spawns the
    `0xBB3BC` actor (a5 = `+0x56 | 0x400`, `+0x59` = 2, `+0x14`/`+0x24` from
    the record, `+0x4E`/`+0x2E` adjusted for side 1). That actor's stream
    `0xE4F94` carries the `0xD100` target `0x2372C`, which `0x2463E` also
    calls. Both are unported.
  - Character 1's other reaction callbacks in this code range, `0x23178`
    (`0xA3D48`, reaction `0x28`), `0x230F0` (`0xA3D0C`, `0x25`) and `0x23130`
    (`0xA3CA8`, `0x20`), are unported. They were not part of this item.
  - The type-`0x19` spawners `0x48F98`/`0x28F08` (update-table entries 1/10,
    §41-D.5) are outside this item (since ported and registered, §46-D).

## 43-B. The mode `0x1A`/`0x1B` wipe and the `DS_00104AE4` hooks `0x28D68`/`0x28D80` (named-gap batch 3, branch `gap3-modes`)

**Result in one line.** The hook installers `0x28D68`/`0x28D80`, the mode
`0x1A` arm `0x4F980`, the mode `0x1A`/`0x1B` handlers `0x4F9A0`/`0x4F9C8`
and the callees they needed (the wipes `0x4F9E4`/`0x4FA88` and `0x10D70`) are
ported: `frontend_char_screen_hook`, `frontend_char_screen_hook_voice`,
`frontend_wipe_arm`, `frontend_mode_1a_step`, `frontend_mode_1b_step`,
`frontend_wipe_in`, `frontend_wipe_out` (`flow.c`) and `actor_pset_word_set`
(`actors.c`). Both hooks are registered. Each handler's `call [0x104AE4]` goes
through `fn_resolve`. **The dispatch in `game_frame` is not wired**: nothing on
a ported path stores mode `0x1A` (§43-B.2), so cases `0x1A`/`0x1B` stay a named
gap and the chain is unit-tested only.

**Correction to §42-F.2.** It says `0x28D68` and `0x28D80` "store the address
into `DS_00104AE4`, then call `0x4F980(0x10)`", and lists the code that loads
the setters as their callers. The raw shows that both are **themselves
`DS_00104AE4` values**, with no rel32 entrance. Every immediate that loads them
is followed by a store into `DS_00104AE4`: `0x42CF3 mov ebx` → `0x42D04`,
`0x42D34 mov edx` → `0x42D40`, `0x42D7C mov ecx` → `0x42D8F` (these three in
`0x42CB4`), `0x42FB6 mov edx` → `0x42FBD` (in the unreferenced stub
`0x42FB0`, §43-B.2), and `0x28E4E mov esi` → `0x28E60` (in
`0x28DA4`, which also stores CX, loaded with `0x17` at `0x28E3A`, as the
mode at `0x28E66`). So the chain is: a
dispatcher calls the hook `0x28D68`/`0x28D80`, which installs `0x43738` and
arms mode `0x1A`. Then `0x4F9A0` calls `0x43738` through the same pointer.

### 43-B.1 The raw (Ghidra `disassemble_function`; `read_memory` + capstone where Ghidra has no function; fixups applied)

- **`0x28D68`** (no Ghidra function): `push edx; mov edx,0x43738; mov
  eax,0x10; mov [0x104ae4],edx; call 0x4F980; pop edx; ret`.
- **`0x28D80`** (no Ghidra function): `push edx; mov eax,0x2e; mov
  edx,0x43738; call 0x2C3FC` (the voice; it pushes EBX/EDX/EDI, §42-E.2, so
  EDX survives); `mov eax,0x10; mov [0x104ae4],edx; call 0x4F980; pop edx;
  ret`.
- **`0x4F980`**: `push edx; xor dl,dl; mov [0x1088f5],dl; mov edx,0x1a; mov
  [0x104afa],ax; mov word [0x104b00],dx; pop edx; ret`. Both stores are words.
- **`0x4F9A0`**: `push edx; call 0x4F9E4; test eax,eax; jz ret; call
  [0x104ae4]; xor ah,ah; mov edx,0x1b; mov [0x1088f5],ah; mov word
  [0x104b00],dx; pop edx; ret`.
- **`0x4F9C8`**: `call 0x4FA88; test eax,eax; jz ret; call [0x104ae4]; mov
  ax,[0x104afa]; mov [0x104b00],ax; ret`.
- **`0x4F9E4`** (the wipe-in step; it pushes and pops EBX/ECX/EDX/ESI):
  - If `[0xC98F0]` == 0: `ecx = 0xFF; push edx` (0, the a5); `eax = 0xC98F4;
    xor ebx,ebx; call 0x2AE14`. So the spawn is (a2 = EDX = 0, a3 = 0xFF,
    a4 = 0, a5 = 0), stored to `[0xC98F0]`, then `[0x10150C]` =
    `[0x101508]`.
  - `mov eax,[0x1088f2]; sar eax,0x18; cmp eax,0x10; jle`. This is the
    signed byte `0x1088F5`.
  - Above `0x10`: `0x2B150([0xC98F0])`, `ecx = 0`, `eax = 1`, `[0xC98F0]` =
    0, `call 0x2BAF4` (EAX = 1, the arm `actors_reset` ports), byte
    `[0x1088F4]` = 1, return 1.
  - Otherwise: `al = [0x1088F5]`, `ebx = [0xC98F0]`, `movsx esi,al`, `push
    ebx`, `si = [esi*2+0xC991C]`, `and esi,0xffff`, `inc al`, `[0x1088F5]` =
    al, `eax = esi`, `call 0x10D70`, return 0.
- **`0x4FA88`** (the wipe-out step) is the same shape with the descriptor
  `0xC9908` and the table `0xC993E`. There are two differences:
  - The spawn arm also clears byte `[0x1088F4]` (`0x4FAB7 xor ah,ah; 0x4FAB9`),
    so only on the spawn frame.
  - The done arm returns 1 with no `0x2BAF4` and no `0x1088F4` store.
- **`0x10D70`**: `ebx = [esp+4]` (the record), then
  `edx = [0x1014EC] + word [ebx+0x56] << 5`, `and byte [ebx+0x28],0xfb`,
  `mov [edx],ax`. Then `bx = [ebx+0x28]; xor bl,bl; and bh,0x40`. If that
  is non-zero it does `or ah,0x80`, else `and ah,0x7f`. Then `mov [edx],ax;
  ret 4`. The first store is overwritten at the same address with no read in
  between, so the port keeps only the second.
- **Tables** (data object):

| address | contents |
|---|---|
| `0xC98F0` | 0 (the wipe actor's slot, BSS-like) |
| `0xC98F4` | desc `0x2DD9, 0, 0x802800, 0x1000, 0x3E638` |
| `0xC9908` | desc `0x2DE9, 0, 0x802800, 0x1000, 0x3E638` |
| `0xC991C` | 17 words `0x2DD9..0x2DE8, 0x3F13` |
| `0xC993E` | 17 words `0x2DE9..0x2DF8, 0x3F14` |

  The counter runs 0..0x10, one word per frame. The 18th frame (counter
  0x11) is the done frame.
- **The `0x24C5C` cases** (jump table `0x24B8C`, on the word `[0x104B00]`,
  `cmp ax,0x33; ja`): entry `0x1A` = `0x25403` (`call 0x4F9A0`), entry
  `0x1B` = `0x2540A` (`call 0x4F9C8`). Both then jump to `0x2540F`.

### 43-B.2 Entrances (`get_xrefs_to`, a rel32 call/jmp/jcc scan of the code object, a dword scan of both objects)

| target | rel32 | dwords |
|---|---|---|
| `0x28D68` | none | code `0x42CF4`, `0x42D35`, `0x42D7D` (in `0x42CB4`) and `0x42FB7` (in the stub `0x42FB0`); none in data |
| `0x28D80` | none | code `0x28E4F` (in `0x28DA4`); none in data |
| `0x4F980` | `0x1F44D` (`0x1EEB0`), `0x25816` (`0x257A4`), `0x25A4C`, `0x25A79`, `0x26991`, `0x27162`, `0x28D79`, `0x28D9B`, `0x43AEE` (`0x43AAC`), `0x43C1E` (`0x43B24`), `0x4482A` (`0x44798`) | none |
| `0x4F9A0` | `0x25403` only | none |
| `0x4F9C8` | `0x2540A` only | none |
| `0x4F9E4` | `0x4F9A1` only | none |
| `0x4FA88` | `0x4F9C8` only | none |
| `0x10D70` | `0x1D3F5`, `0x1D42C`, `0x1D458` (`0x1D2F0`), `0x1D4C6` (`0x1D464`), `0x1DAE0` (`0x1DA84`), `0x4FA79`, `0x4FB14` | none |
| `0xC98F0` | only inside `0x4F9E4`/`0x4FA88` (10 code immediates) | none |
| `0x257A4` | `0x11D41`, `0x11EB8` (in `0x11D04`); `jmp` at `0x11CD4` (the stub `0x11CC8`); `0x24F5C`, `0x24FBA`, `0x25014`, `0x25067`, `0x250C4`, `0x25121`, `0x2517D`, `0x25194` (in `0x24C5C`) | none |
| `0x1088F5` | only `0x4F980`, `0x4F9A0`, `0x4F9E4`, `0x4FA88` (6 immediates, plus the two `[0x1088F2]` dword reads) | none |

**Why the dispatch is not wired.** The only store of mode `0x1A` is
`0x4F980`. None of its eleven callers runs in the port:
- `0x28D79`/`0x28D9B` are the two hooks. Their storers are all unported:
  `0x42CB4` (called only at `0x425DE`, in `0x424E8`, mode `0x13`, `0x253C4`),
  `0x28DA4` (called at `0x25269`/`0x25353`, cases 6 and `0xC`), and the
  unreferenced stub `0x42FB0` (below).
- `0x25816` is in `0x257A4`, the coin divert. It stores the hook `0x4367C`,
  which is unported and has no Ghidra function. `0x11D04` reaches it at
  `0x11D41`, after a coin is accepted, and at `0x11EB8`, in state 8
  (`0x11CDC[8]` = `0x11EAC`). The port stubs both. `game_state_step` returns
  after `frontend_coin_poll` accepts, with a `PORT:` note, and state 8 is
  empty. State 8 is also unreachable in the port: attract phase `0xB` enters
  it only when `DS_00108173` != 0, and no ported code writes that byte (its
  image value is 0; its writers are in unported code).
- The rest are in unported mode or select-screen code: `0x1EEB0` (mode
  `0x1E`), `0x26991`, `0x27162`, `0x25A4C`/`0x25A79`, and `0x43AAC`/
  `0x43B24`/`0x44798`.

**The fourth `0x28D68` storer.** Ghidra's `FUN_00042cb4` ends at `0x42F5C`,
and `get_xrefs_to 0x28D68` lists only its three DATA references (`0x42CF3`/
`0x42D34`/`0x42D7C`). The fourth immediate (`0x42FB6`) sits in a separate
stub, `0x42FB0`, which Ghidra has no function for: `push edx; call 0x42FE0;
mov edx,0x28d68; mov ah,5; mov [0x104ae4],edx; mov edx,0x78; mov
[0x104b25],ah; mov word [0x1088ee],dx; mov word [0x104afe],dx; pop edx; ret`.
Nothing enters it: there is no rel32 to `0x42FB0`/`0x42FB1`/`0x42FB6` and no
dword of it in either object. So `0x28D68` has three storers: `0x42CB4`,
`0x28DA4` and the unreferenced `0x42FB0`.

**`0x257A4`'s other entrances** (the table row above) are all unported too:
- the tail `jmp` at `0x11CD4`, from the stub `0x11CC8`, which has no rel32 or
  dword entrance;
- eight calls in `0x24C5C`'s mode cases `0x28`..`0x2F` (jump-table entries
  `0x24F09`..`0x2519E`).

So `DS_00104B00` never becomes `0x1A` on a ported path. `game_frame` keeps its
`switch` on mode 3 and names cases `0x1A`/`0x1B` in its `PORT:` note.

### 43-B.3 The port

- `flow.c`, after `frontend_darken_marked`, has one C function per original.
  The spawns pass `actor_spawn(desc, 0, 0xFF, 0, 0)`. The counter is read as
  `s8` and compared `> 0x10`. The done arm of `0x4F9E4` calls
  `actors_reset()`, the EAX = 1 arm of `0x2BAF4`.
- The hook call is `fn_resolve(DSD(DS_00104AE4))`, and a miss is skipped
  (`PORT:`). A miss is a no-op only for `0x29D60` (a bare `ret`, stored by
  `0x43738`/`0x444C8`, so it is the hook `0x4F9C8` calls after the character
  screen) and `0x5D812` (§42-E.4).
  - **Correction (review):** the first version of this record said those two
    were the only unregistered values the image stores in `DS_00104AE4`. That
    is false. There are 13 more unregistered, non-trivial ones: `0x430E8`,
    `0x4367C`, `0x25BBC`, `0x26998`, `0x270BC`, `0x259CC`, `0x10E80`,
    `0x24B54`, `0x27134`, `0x4142C`, `0x25AE8`, `0x26978` and `0x430C0`.
  - Five of them are the hooks that `0x4F980`'s own callers install just
    before arming mode `0x1A`, so `0x4F9A0`/`0x4F9C8` would call them. They
    are the chain's named gaps:

    | installs the hook | calls `0x4F980` | hook |
    |---|---|---|
    | `0x1F447`, `0x43AE8`, `0x43C18`, `0x44824` | `0x1F44D`, `0x43AEE`, `0x43C1E`, `0x4482A` | `0x430E8` |
    | `0x25810` | `0x25816` | `0x4367C` |
    | `0x25A46`/`0x25A73` | `0x25A4C`/`0x25A79` | `0x25BBC` |
    | `0x2698B` | `0x26991` | `0x26998` |
    | `0x2715C` | `0x27162` | `0x270BC` |

  - The silent skip is harmless today, because nothing dispatches modes
    `0x1A`/`0x1B`. Both handlers carry a `TODO(verify)`: once the dispatch is
    wired, a miss on any value but the two no-ops is a missing port.
  - EAX is dead after both calls: `xor ah,ah` plus AH/DX stores at `0x4F9B0`,
    and the load at `0x4F9D7`.
- The voice `0x2C3FC(0x2E)` in `0x28D80` is a `PORT:` note (spec §7).
- `actors.c`: `actor_pset_word_set` (`0x10D70`) after `actor_set_dead`. It has
  no pool check, as in the raw. `fn_register(0x28D68/0x28D80)` is at the end
  of `actors_init`.
- `0x10D70`'s other callers (`0x1D2F0`, `0x1D464`, `0x1DA84`) stay unported.

### 43-B.4 The assertions and mutations

`check_char_screen_modes` (`test_fight.c`, run after
`check_char_screen_open`) uses the same snapshot: the data object, both pools,
both buffers, the resource table, the DAC and the aperture. Its runs:
- **(a)** `fn_resolve(0x28D68/0x28D80)` returns the ports.
- **(b)** `0x4F980(0x1234)` alone, then each hook, from sentinels:
  - `0x1088F5` becomes 0 and its neighbours `0x1088F4`/`0x1088F6` are
    untouched;
  - the `0x104AF8` dword becomes `ret_mode << 16 | 0xA5A5`, a word store;
  - `0x104B00` becomes `0xBEEF001A`, a word store;
  - `DS_00104AE4` becomes `0x43738` for the hooks and is untouched for the
    bare arm.
- **(c)** `0x10D70` alone:
  - `+0x28` = `0x4004` with word `0x1234` gives `0x9234` and `+0x28` =
    `0x4000`;
  - `+0x28` = `0x2804` with word `0x8123` gives `0x0123` (bit 15 cleared, and
    bit 13 is not the source);
  - pset `+2` is untouched.
- **(d)** The whole chain from `0x28D68`, with `chs_seed(3, 6, 2, 1)`:
  - Frame 1 spawns id `0x2DD9` (`+0x49` = `0xFF`), resyncs `0x10150C` to
    `0x123` and sets the counter to 1.
  - Frame 2 gives `0x2DDA | 0x8000`, after `+0x28` bits 14 and 2 are seeded;
    bit 2 is cleared.
  - Frame 17 gives `0x3F13 | 0x8000` and counter `0x11`. There is no resync,
    the mode is still `0x1A`, the gate keeps its sentinel, and the hook has
    not run (`DS_00104AE4` is still `0x43738`, `0x108174` is still `0x77`).
  - Frame 18: the slot is 0, the gate is 1, the counter is 0, the mode is
    `0x1B` and `AFA` is `0x10`. `0x43738`'s whole result holds
    (`chs_check_common`, and `chs_check_side` for side 1), with 9 records
    active and both tick counters zeroed by `0x52106`.
  - The `0x1B` wipe: id `0x2DE9`, the gate cleared and the resync on the spawn
    frame only (sentinels `0x55`/`0x300` are kept on later frames), and
    `0x3F14` at frame 17.
  - `DS_00104AE4` = `0x29B74` (visible: `DS_00104AFE` = `0x78`, mode `0x15`).
    It stays unrun on frames 2..17. On frame 18 it runs, and the mode then
    becomes `0x10` (the copy comes after the hook). The actor is dead, and the
    active count stays 10 (no `0x2BAF4`).
- **(e)** A done frame of `0x4F9A0` with the no-op hook `0x29D60`: the active
  list is empty (`0x4F9E4`'s own `0x2BAF4`), `0x10150C` is 0, and the mode
  dword is `0xBEEF001B`.
- **(f)** The signed counter: `0xFF` is not done, becomes 0, and writes the
  word before the table (`0xC991A` = 3).

**Mutations** (`scratchpad/g3m/mut.py`, `mut.log`): 38 single-site edits.
37 fail 2..60 assertions. They cover:
- each store and its width, the hook value, the return mode;
- the resyncs, the signedness and the threshold, the tables and descriptors,
  a3;
- the `0x2BAF4` (dropped, or added to the wipe-out), the gate value and its
  placement;
- the kill and the slot clear in the wipe-out;
- the hook dropped, called every frame, or called after the mode copy;
- the three `0x10D70` operations and both registrations.

One survivor, `M12b`, is equivalent: it drops the `0x2B150` in `0x4F9E4`'s
done arm. The next call, `0x2BAF4`, zeroes the record and pset pools,
re-inits the render list and the palette ownership table (`0x336C0`), so
nothing `0x2B150` wrote survives. Its `cb2` arm needs `+0x2B` bit 6, which the
descriptor's flags `0x802800` do not set. The same kill in `0x4FA88` (`M33`,
no reset after it) fails 2 assertions.

### 43-B.5 Measured and remaining gaps

- `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all checks passed, with 0
  compiler warnings. `make verify` and the drivers were not run, as the brief
  requires.
- No oracle is expected to move. No ported path reaches the new functions or
  sets mode `0x1A` (§43-B.2). The two new registrations are code immediates
  only, so no stream walk resolves them.
- Remaining named gaps:
  - the `game_frame` cases `0x1A`/`0x1B` (they wait for a ported caller of
    `0x4F980`);
  - the five hooks `0x4F980`'s callers install, which the mode `0x1A`/`0x1B`
    handlers would call (§43-B.3): `0x430E8`, `0x4367C` (no Ghidra function),
    `0x25BBC`, `0x26998` and `0x270BC`;
  - `0x4F980`'s other callers, most directly the coin divert `0x257A4`, with
    `0x11D04`'s two stubbed routes into it (`0x11D41`, and state 8 at
    `0x11EB8`) and its other entrances (the stub `0x11CC8`'s `jmp`, and
    `0x24C5C`'s mode cases `0x28`..`0x2F`);
  - the hooks' storers `0x42CB4` (mode `0x13`), `0x28DA4` (cases 6/`0xC`)
    and the unreferenced stub `0x42FB0`;
  - `0x10D70`'s other callers `0x1D2F0`/`0x1D464`/`0x1DA84`;
  - `0x2C3FC(0x2E)`, the voice (spec §7);
  - the port's `game_frame` switch reads `DS_00104B00` as a dword, where the
    raw reads a word (`0x24EEC mov ax,[0x104b00]`). This is harmless while
    `0x104B02` stays 0 on ported paths, but `0x257A4` passes that
    address to `0x65490` (`0x257FC mov eax,0x104b02`). Any future wiring of these
    cases should switch on the word.

## 43-C. Character 1's reaction callbacks `0x230F0`/`0x23130`/`0x23178` and the `0x2365C` stream chain `0x236D8` -> `0x2372C` (named-gap batch 3, branch `gap3-char1`)

**Result in one line.** §42-A.5 named five gaps in character 1's code range, and
all five are now ported, registered and unit-tested. The first three are the
reaction callbacks `0x230F0` (reaction `0x25`), `0x23130` (`0x20`) and
`0x23178` (`0x28`). The other two are the `0xD100` stream targets `0x236D8`
(in `0x2365C`'s stream `0xE4996`) and `0x2372C` (in the stream `0xE4F94` of
the child that `0x236D8` spawns). They need no unported callee. That is 5
functions and 451 raw bytes (62 + 72 + 72 + 83 + 162). This section
supersedes the `0x236D8` and "other reaction callbacks" items of §42-A.5. A
probe of the front-end driver's 3500-loop window is byte-identical to the
base's (§43-C.5).

### 43-C.1 The raw (`read_memory` + capstone, fixups applied; Ghidra has no function at any of the five)

- **`0x230F0`** (62 bytes, `0x230F0..0x2312D`). It pushes ECX and ESI and
  takes EAX = slot (kept in ECX), EDX = rec (kept in ESI) and EBX = side. It
  builds the `0x33950(esp, EBX)` context (`0x230FD`), which it never reads.
  `0x33950` (`0x33950..0x339A9`) writes only the six context dwords. Then:
  - `0x3C4CC(rec, 0xE483E, 0x40400000)` (`0x23102..0x2310E`);
  - `[ecx+0x52]` = 9, `+0x53` = 7, `+0x54` = 0 (`0x23113..0x2311B`);
  - AL = 1 and `+0x0C` = 0 (`0x2311F`/`0x23121`).

  `0x3C4CC` reads the slot's `+0x52` before this store.
- **`0x23130`** (72 bytes) has the same registers and the same dead context
  (`0x2313D`). It stores `+0x52` = 9, `+0x53` = 7, `+0x54` = 0 and `+0x0C` = 0
  (`0x23142..0x2315A`, interleaved with the argument set-up) before
  `0x3C4CC(rec, 0xE4872, 0x40400000)` (`0x23161`). So `0x3C4CC` sees `+0x52`
  = 9 and takes its `0x3C480` arm. Then the voice `0x2C3FC(0x7C)`
  (`0x23166..0x2316B`) and AL = 1.
- **`0x23178`** (72 bytes) is `0x23130` instruction for instruction except
  for the stream `0xE48DC` (`0x23197`). The rel32 call encodings differ, since
  they are position-dependent.
- **`0x236D8`** (83 bytes; it pushes EBX, ECX, EDX, ESI and EDI; EAX = rec;
  EDX is zeroed at `0x236F7` before any read). EDI = `[rec+0x14]` (the slot),
  and 0 returns (`0x236DF..0x236E4`). Otherwise
  `0x2AE14(0xBB3BC, EDX = 0, ECX = 0, EBX = 0, push (word [rec+0x56] | 0x400)
  & 0xFFFF)` (`0x236E6..0x236FE`). On the result: `+0x59` = 2, `+0x14` = EDI,
  `+0x24` = `[rec+0x24]` (`0x23703..0x2370D`). When byte `[rec+0x51]` is not
  0: `+0x4E` = 1 and word `+0x2E` += 4 (`0x23710..0x23721`, a 16-bit add).
  The spawn result is not checked.
- **`0x2372C`** (162 bytes; it pushes EBX, ECX, EDX, ESI, EDI and EBP; EAX =
  rec; EDX is overwritten at `0x2374D`/`0x2375E` before any read). ESI =
  `[rec+0x14]` (the slot), and 0 returns (`0x23732..0x23737`).
  - It tests REC's own word `+0x28` for bit 14 (`0x2373D..0x2374B`). Clear
    gives EDI = `0xFFFFFEED` and EDX = `0xFFFFF400`; set gives EDI = `0x113`
    and EDX = `0xC00`.
  - The pushed a5 is `0x4000` when the SLOT RECORD's (`[esi]`) `+0x28` has bit
    14, else 0 (`0x23763..0x2377F`).
  - Then `0x2AE14(0xBB3D0, EDX = movsx DX + [frec+0x18], ECX = [frec+0x30]
    sar 16, EBX = [frec+0x1C] + 0x1480, a5)` (`0x23780..0x2379E`).
  - `[esi+8]` = the result, whose word `+0x34` = DI (`0x237A3/0x237A6`), and
    `[[esi+8]+0x14]` = ESI (`0x237AA/0x237AD`).
  - When byte `[[esi]+0x51]` is not 0: word `+0x2E` += 4 and `+0x4E` = 1
    (`0x237B0..0x237C3`).
- **The data.**
  - The reaction entries `0xA3528 + 0x500 + r * 0x14` for character 1 read
    `{0x23130, 0, 0xE8CDA, 0xE8E56, 0x9C4}` (`0x20`) and `{0x230F0, 0, ...}`
    (`0x25`). `0x28`'s callback dword is `0x23178`. Each entry's stream dword
    is 0, so `0x34E2C` starts no stream and only the callback runs.
  - `0xBB3BC` is `{stream 0xE4F94, type 0, frame 0, +0x2E 8, flags 0x0080,
    extent 0x20, +0x2C 0x1000, handle 0x0105FF3C}`.
  - `0xBB3D0` is `{0xE4FB8, type 8, frame 2, 8, 0x0080, 0x20, 0x1000,
    0x0105FF3C}`. Type 8's row of `0xBB9D8` is `{0xBB3BC, 0x5D812, 0x3B9C4}`,
    so its cb1 is the stub. Type 8 is the projectile type that `0x3B464`'s
    `+0x48` = 8 arm freezes on (`0x235C4`, §41-C).
  - `0xE4F94` reads `9302 CD40 071D B840 0001 <0xE4F96> D100 <0x2372C> 0000
    CD40 071E B840 000C <0xE4FAA> 8000`.
  - `0xE4FB8` reads `9302 CD40 072A B840 0008 <0xE4FBA> 8E40 C300 <0xE4FBA>`,
    a loop with no code target.
  - The three reaction streams open with `DC00 <table>`: `0xE22C8`, `0xE22E8`
    and `0xE2328`, whose first bytes are 2, 1 and 2. Their only code targets
    are `0xD500 0x3C32C` (registered) at `0xE4854`, `0xE489E` and `0xE492C`.
  - `0xE4996`'s targets are `0x236D8` (`0xE49A2`) and `0xD000 0x36870`
    (`0xE49BE`, registered).

### 43-C.2 Entrances (`get_xrefs_to`, a rel32 call/jmp/jcc scan of the code object and a dword scan of both objects)

| address | references |
|---|---|
| `0x230F0` | data dword `0xA3D0C` only (character 1, reaction `0x25`) |
| `0x23130` | data dword `0xA3CA8` only (reaction `0x20`) |
| `0x23178` | data dword `0xA3D48` only (reaction `0x28`) |
| `0x236D8` | data dword `0xE49A4` only (after the `0xD100` word at `0xE49A2`) |
| `0x2372C` | rel32 `call` at `0x2463E` and data dword `0xE4FA4` (after `0xD100` at `0xE4FA2`) |
| `0xBB3BC` | code dword `0x236FA` and data dword `0xBBA38` (the type table's type-8 row) |
| `0xBB3D0` | code dword `0x2378F` only |

`get_xrefs_to` lists only the `0x2463E` call, because Ghidra has no code or
data references at the others. `0x2463E` lies in `0x24568`, which is entry 1
of the table `0xA8628` (the dword at `0xA862C`). Three unported sites dispatch
it:
- `0x25F27`: `call [edx*4 + 0xA8628]`;
- `0x27732`: `call [edx*4 + 0xA8628]`;
- `0x2989C`: `ff148d28860a00`, `call [ecx*4 + 0xA8628]`.

`0x24568` passes EAX = `0x1077B0[side]`'s record, whose `+0x14` is its slot
(`0x33D62`).

`0xBB3BC`'s second reference, `0xBBA38`, is row 8 of `0xBB9D8`. The only code
reference to that table is the crowd spawner `0x2C320` (`fight_scene_crowd`),
which reads the row's first dword at `0x2C36F`. No scene's crowd table uses
index 8. The indices at `+0xA` of each `0xBBDA8[scene]` record (counts at
`0xBBD98`) are:

| scene | indices |
|---|---|
| 0 | 27, 46, 47 |
| 1 | 17, 18, 24, 7, 7, 20 |
| 2, 3, 5 | none |
| 4 | 22 |
| 6 | 42, 41, 42, 41, 41 |
| 7 | 12, 11, 15, 28, 28, 30, 23, 23, 13, 31, 31, 23, 29, 29, 29, 28, 29 |

So `0xE4F94`, and with it the `0xD100` call of `0x2372C`, is reached only
through `0x236D8`.

The small body at `0x230D8..0x230ED` (state 9/7/0, `+0x0C` = 0, AL = 1, no
prologue) has no reference in either scan and is not ported.

### 43-C.3 The port

- `fighter.c` has `fighter_230f0`, `fighter_23130`, `fighter_23178`,
  `fighter_236d8` and `fighter_2372c`, with local `#define`s for the three
  streams and the descriptors `0xBB3BC`/`0xBB3D0`.
  - The dead `0x33950` context is kept as a `fighter_ctx_same` call.
  - The voices `0x2C3FC(0x7C)` are `PORT:` stubs (spec §7).
  - `fighter_2365c`'s `PORT:` gap note is gone.
- `actors.c` has the wrappers `reaction_cb_230F0`/`23130`/`23178` (the
  `(slot, rec, side)` shape; `0x35045` ignores AL) and
  `anim_code_236D8`/`2372C` (the `(rec, arg)` shape; the operand is never
  read). `actors_init` registers all five.

### 43-C.4 The assertions and mutations

Two groups in `test_fight.c` run after `check_char1_reactions`. Each saves
and restores the whole data object, `FIGHT_RECS..+0x6400` and the psets
(`q42_save`/`q42_restore`).

- `check_char1_reactions_b` (52 sites):
  - the table dwords and their zero stream dwords;
  - each callback through its registration, with EBX not the side;
  - state 9/7/0 and `+0x0C` = 0 against the `q42_fz_seed` sentinels, with the
    other slot kept;
  - the stream identity: rec`+0x10` = the `DC00` table and `+0x24` = its first
    byte (2.0/1.0/2.0; the 3.0 argument is replaced). rec`+8` ends at the
    first sprite's operand: `0xE4848`, `0xE4892` and `0xE4892`, since
    `0xE48DC`'s `C300` jumps into `0xE4872`;
  - the order against `0x3C4CC`. `0x230F0` on a `+0x52` = 0 seed keeps
    rec`+0x1C` (the plain arm), while `0x23130`/`0x23178` on the same seed
    zero it (`0x3C480`'s `0x188AC`);
  - the returned AL;
  - `0x34E2C` (`hit_reaction_apply(0, 0x25)` with slot 0's `+0x7A` = 1)
    reaching `0x230F0` through the `0xA3528` table.
- `check_char1_chain` (58 sites), with a one-record actor free list:
  - the stream and descriptor dwords;
  - `0x236D8` through its registration and directly: the child's `+8`
    (`0xE4F98`), `+0x14`, `+0x24` (copied, not the spawn's 0), `+0x59`,
    `+0x4A` (the parent's index), `+0x28` bit 10, `+0x49` (the parent pset's
    layer + 2), the parent's `+0x4F` + 1, and `+0x2E`/`+0x4E` for sides 1
    and 0;
  - the no-slot return;
  - a crafted `D100 36D8 0002 0000` stream through `actors_anim_begin`;
  - `0x2372C` through its registration and directly: the slot's `+0x08` (the
    other slot's kept), the active list, x `0x5000` -/+ `0xC00`, y `0x40` +
    `0x1480`, height 5, `+0x34` `0xFEED`/`0x113`, `+0x14`, `+0x28` =
    `0x4080`/`0x0080`, type 8, `+8` = `0xE4FBC` and `+0x2E`/`+0x4E` for both
    sides;
  - the rec-versus-slot-record split: the sign comes from rec's bit 14 and
    a5 from the slot record's;
  - the no-slot return;
  - a crafted `D100 372C 0002 0000` stream.

**Mutations** (`scratchpad/c1/mut.py`, `mut.out`/`mut2.out`): 57 single-site
edits of the new code and its registrations. They cover every store,
constant, stream, descriptor, gate, side test, the call-versus-store order in
all three callbacks, rec versus slot record in `0x2372C`, each registration
and two wrapper swaps. All 57 fail the suite (1..14 assertions each), with no
fault and no timeout. Changing `0x236D8`'s `+0x59` to 3 fails. Dropping that
store is equivalent, because the spawn walk's `9302` (opcode `0x13`) at
`0xE4F94` already stores 2. The script restored the sources, and `git diff`
shows only the intended changes.

### 43-C.5 Measured, and the remaining gaps

- `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all checks passed, 0 compiler
  warnings. The drivers and `make verify` were not run.
- **The probe.** This is §42-A.5's scratch harness, which writes no dump. It
  was linked once against `libprage_core.a` built from `713833c` and once
  against this branch. Each of the `FE_LOOPS` = 3500 loops logs the state, the
  presented-frame hash, the data-object hash, each side's character,
  `DS_001088A8[side]` (the last reaction `0x34E2C` applied), `+0x5F`, the
  slot's `+0x08` and the RNG. The two logs are byte-identical.
  - The second demo's character-1 side (side 1) applies only reactions
    `0x01`, `0x08`, `0x0E`, `0x11` and `0xFF`. It never applies `0x20`,
    `0x25`, `0x28` or `0x2A`.
  - `0x2372C`'s other caller `0x24568` is unported.

  So no oracle is expected to move at the current pins. The controller's
  ladder is the check.
- **Named gaps.**
  - The voices `0x2C3FC(0x7C)` at `0x23166`/`0x231AE` (spec §7).
  - `0x2372C`'s other caller: `0x24568` (`0xA8628` entry 1, the call at
    `0x2463E`). Its dispatches `0x25F27`/`0x27732`/`0x2989C` are unported
    mode code.
  - The type-8 projectile `0xBB3D0` is spawned. What moves it and hits with
    it is the existing type-8 machinery (`0x3B464`'s `+0x48` = 8 arm, §41-C)
    and was not re-audited here.
  - `0x230D8` has no reference and is not ported.

## 44-A. Character 3's reactions `0x25`/`0x24`, the process `0x2910C` and the opcode-`0x0C` hflip at capture 3099 (roar-timing Task 32, `b947895`)

**Result in one line.** At f = 4309 (loop 3422) the original's raptor takes
character 3's reaction `0x25`, whose callback `0x15350` was not registered;
at f = 4438 (loop 3551) it takes reaction `0x24` (`0x151C0`). Both are ported
with the callbacks they store and their streams' `0xD100` targets. The blood
of the hit at f = 4425 then exposed two more gaps: the process-table entry
`0x2910C` (§41-D's named gap) and a port bug in the animation opcode `0x0C`
(the child's `a5` lost the parent's hflip). With all of them the live-RAM poll
matches the port through f = 4570, the second demo's last frame; 3099..3256
are explained and N = 3257 (`b947895`). 3257 is the loader's `- LOADING -`
overlay for a sound bank the port does not load (the voice path, spec §7).

### 44-A.1 The raw (Ghidra `read_memory` + capstone, fixups applied)

Ghidra has no function at any of the addresses below (`decompile_function`
fails). Each block decodes cleanly up to its `ret`, with padding after it
(`scratchpad/t32/d15350.txt`, `d151c0.txt`, `d2910c.txt`).

- **`0x15350..0x153D6`** (`*(u32*)0xA470C` reads `50 53 01 00 00 00 00 00`:
  the callback, no stream word). `push ebx; push ecx; sub esp,0x18`; EBX =
  EAX (the slot), ECX = EDX (the record); the EBX 0x34E2C passes is never
  read. `0x339AC(esp, rec)`. `mov al,[ecx+0x51]; xor al,1; and eax,0xff;
  call 0x468D8`: when AL is non-zero it returns AL = 0 and writes nothing.
  Otherwise `0x3C4CC(rec, 0xD311A, 3.0)`; the slot's `+0x57` = 0 (stored
  twice, `0x15388` and `0x153BB`), `+0x52/+0x53/+0x54` = 9/7/0, `+0x0C` =
  `0x152D4`, `+0x18` = `0x15208`, `+0x1C` = `0x1527C`, `+0x42 |= 4`;
  `0x2C3FC(0xAF)` (the voice, out of scope); the byte `0xFD118 + ctx[0]` = 0
  (`0x153C4..0x153C9`); AL = 1.
- **`0x152D4`** (+0x0C, 0x3531C case 7: EAX = slot, EDX = rec, EBX = side;
  only EBX is read). `0x33950(esp, side)`. Returns unless ctx[2]'s `+0x57`
  is 1 and the byte at `0x9AFF8` (0x28) is below ctx[2]'s word `+0x88`
  (`cmp dx,[eax+0x88]; jge`, signed). Then `+0x57` = 2; when ctx[4]'s
  `+0x4B` is non-zero, `0x2BD44(ctx[4], [0x1014F4] + 0x68 * +0x4B)` (no
  `+0x60` test, unlike `0x3B844`); `0x2BC30(ctx[4], 0xD315A, 3.0)`.
- **`0x15208`** (+0x18, 0x19020's `fn(side)`, EAX returned). `0x33950`, then
  `0x18BD4`, flags 1/8/4/0xD/0xE/7 = 0 and 5/9 = 1, `0x18C14(side, flags,
  EBX = 0x9AFFA, ECX = 0x9B001)` (ECX is loaded before `0x33950`, EBX before
  `0x18BD4`; both keep them). 0x18C14 moves EBX into EDI (box a, the x test)
  and ECX into EBX (box b), as `0x3E1D0`'s `0xC75F5`/`0xC75FF` do. The result
  is returned unless the byte at `0x9AFF9` (1) is above ctx[2]'s `+0x88`
  (`jle` skips `mov eax,1`).
- **`0x1527C`** (+0x1C, 0x193B0's `0x19505`, `fn(side)`). `0x33950`;
  `0x39834(ctx[1], ctx[2]'s +0x5F)`; `0x36D20(ctx[3])`; `0x188AC(ctx[1],
  ctx[5]'s +0x18, EBX = 0)`; ctx[3]'s `+0x43 &= 0xCF`; ctx[2]'s `+0x57` = 1.
- **`0x151C0`** (`*(u32*)0xA46F8` reads `c0 51 01 00 00 00 00 00`: reaction
  `0x24`). EBX = slot, EAX = rec: `0x3C4CC(rec, 0xD3078, 3.0)`, `+0x57` = 0,
  9/7/0, `+0x0C` = 0, `+0x18` = `0x15160`, `+0x1C` = `0x151A0`, `+0x42 |= 4`,
  AL = 1.
- **`0x15160`** (+0x18): `0x33950`, `0x18BD4`, flags 1/8 = 0, flag 0 = 1,
  `0x18C14(side, flags, 0, 0)`, its EAX returned.
- **`0x151A0`** (+0x1C): `0x33950`, `0x3B714(ctx[3], ctx[2])`.
- **`0x153D8`** (the `0xD100` target at `0xD312A` in `0xD311A`). ESI = rec,
  EDX = the operand (read): descriptor `0x9B008` when non-zero, else
  `0xBB394`. Returns when the record's `+0x14` is 0. `0x2AE14(desc, 0, 0, 0,
  a5 = +0x56 | 0x400)`; the child's `+0x59` = 2, `+0x14` = the record's
  `+0x14`; for side 1 the child's `+0x4E` = 1 and `+0x2E += 4`; the record's
  `+0x4B` = the child's `+0x56` byte; the child's `+0x60` = 1. The operand
  at `0xD312A` is `DSW(0xD312E)` = 0 (opcode 0x11 reads the word after the
  target dword): the child is `0xBB394` (stream `0xD318E`).
- **`0x1543C`** (the `0xD100` target at `0xD308E` in `0xD3078`). EDX pushed,
  never read. `0x2AE14(0xBB3A8, 0, 0, 0, +0x56 | 0x400)`; the record's `+0x4B`
  = the child's `+0x56`; the child's `+0x60` = 1; `0x2C3FC(0x4D)`. No slot
  gate, no `+0x14`, no side adjust.
- **`0x37CFC`** (the `0xD100` target at `0xD3192` in `0xD318E`). `push edx;
  mov edx,[eax+0x14]`: with that slot set and its `+0x53` not 7, the record's
  `+0x55` = 1 and `0x2B150(rec)`.
- **`0x2910C`** (process entry 7, `DS_000A8644[7]` = the dword at
  `0xA8660`). No input registers. It walks the in-use list at `0x104880`
  (reading each node's next before the body). An actor whose `+0x1C` plus
  `(s32)[+0x34] >> 16` is negative lands: `+0x1C`, the words `+0x34/+0x36/
  +0x44` and `+0x48` are zeroed, `+0x59` = `0xFE`, `0x2BC30(rec, 0xE8D50,
  3.0)`, and its `+0x14` node goes back to `0x104888` (`0x249D0`, `0x249B0`)
  with `+0x14` = 0. Otherwise the node's `+0xC` phase (jump table `0x290FC`,
  entries `0x291C9/0x29219/0x29261/0x29297`) advances on the word velocities:
  0 -> 1 when |vy| < |vx| (`0xE8D14`), 1 -> 2 when |vy| > |vx| (`0xE8D28`),
  2 -> 3 when |vy| > 2|vx| (`0xE8D3C`), each at 2.0; 3 and above do nothing.
- **The opcode `0x0C` spawn** (`0x2B484`). In the non-`0x400` arm,
  `0x2B4C4..0x2B4CD` is `mov ax,[esi+0x28]; xor al,al; and ah,0x40; and
  eax,0xffff; push eax`: a5 = the parent's `+0x28 & 0x4000`, which
  `0x2AE14` copies into the child's `+0x28`. The port computed
  `(W >> 8) & 0x40` = `0x40`: no hflip, and a5's low 7 bits named actor 64
  as the parent until the spawn's tail cleared `+0x4A`. **Raw wins:
  corrected.** The first demo never spawned from a flipped parent here.

### 44-A.2 Entrances

A scan of both objects (dwords, `call`/`jmp` rel32 and `jcc` rel32) and
`get_xrefs_to`:
- `0x15350`: only the data dword `0xA470C`; `0x151C0`: only `0xA46F8`.
- `0x152D4`, `0x15208`, `0x1527C`: only the `imm32` stores in `0x15350`
  (`0x1539B`, `0x153A2`, `0x153AC`); `0x15160`, `0x151A0`: only those in
  `0x151C0` (`0x151EE`, `0x151F8`). No rel32.
- `0x153D8`: data `0xD312A`, `0xD321E`, `0xD32B6`, `0xD3342`, each after a
  `0xD100` word (the last three with operand 1, so `0x9B008`). The code
  dword at `0x24634` is the rel32 of `call 0x39a10` at `0x24633`, not a
  reference.
- `0x1543C`: `0xD308E`, `0xD3266`. `0x37CFC`: `0xD3192`, `0xD4E38`,
  `0xD4E9E`, `0xE85F6`, each after a `0xD100` word.
- `0x2910C`: only `0xA8660`.

### 44-A.3 The measurement (probes reverted, sources checked with `cmp`)

The probe is §41.4's (`scratchpad/t32/probe.py`: the per-loop poll print,
`FE_LOOPS` 3900, a `fn_resolve` miss print with the frame counter and the
caller's offset). The DOSBox-X poll log `t29/db.log` (f 1716..5771) is the
reference; `t32/dbpoll.py` adds each record's `+0x28` and whole-RAM
snapshots at chosen frames.

1. With `0x15350`/`0x151C0` and their callees: the poll equals the port
   through f = 4570, except single-frame sampling tears (f 3703, 3862, 3912,
   3929, 4051, 4056, 4132, 4380; each sample has equal tick counts, `tk
   a/a`, mid-frame). f = 4571 is the demo's exit. The raptor enters 9/7/0 on
   `0xD311C` at f = 4309, returns to its stance at f = 4408 and enters 9/7/0
   on `0xD3082` at f = 4438, as the original. (The port's `0x153D8` spawns
   `0xBB394`'s child at f = 4318, actor `0x6A` on `0xD318E`; the poll does
   not sample it.) New miss: `0x2910C` from
   f = 4426 (146 frames, `run_process_table`). attract2: 3099 -> 3235.
2. With `0x2910C`: no miss is left but the known `0x29B74`/`0x41578`; the
   frames change from cycle-2 frame 1742 on, 3235 does not move.
3. 3235 is cycle-2 frame 1733 (loop 3538, f = 4425), 888 bytes off, rows
   77..117: the blood splash is mirrored. Whole-RAM snapshots of the
   original (f = 4424..4432) against the port's `mem` at the same frames:
   the type-`0x0A` particle (the pool's actor 106, stream `0xE8D02`, node
   `0x104780`) has `+0x34` = `+0x58` in the original and `-0x58` in the port;
   every other field matches. `0x2901C` negates the draw when the child's
   `+0x28` bit 14 is clear at its cb1. The parent (`0xBB09C`'s actor 107,
   stream `0xE8CE2`: `CC00 B0C4 000B 0000 0000`) has bit 14 set in both;
   the port's opcode `0x0C` dropped it (§44-A.1). Corrected: 3235 -> 3257.
4. 3257 is cycle-2 frame 1751 except rows 192..197, cols 0..85 (166
   pixels): the `- LOADING -` string over the game frame; 3258 (a splice)
   carries it too. The original's snapshots at f = 4443/4445: the INDEX
   entry 64 (`s16spisd.gra`, `0x179BA` bytes, flags `0x020179BA`) gains the
   loaded bit `0x20000000`, and the sound module's area at `0x10289C` (the
   `0x102860..0x1028DC` block that `0x1C8E9..0x1CD0D` address, which
   `0x2C3FC` calls through `0x1CA14`/`0x1CC28`) takes the handle
   `0x2001513C` (entry 64). The port never resolves entries 63..65 (a
   `res_resolve` probe). So 3257 is a lazy load by the voice path, which is
   out of scope (spec §7), shown by the loader's read-stall presentation
   (§35.1). The call that triggers it is not pinned: `0x1543C`'s
   `0x2C3FC(0x4D)` runs in the same window (the raptor's `+0x4B` = `0x6B`
   by f = 4445). **Named gap.**

`FE_LOOPS` 3500 -> 3900: a measurement window. 3257 is loop 3556, and 3900
reaches past the capture's last frame 3616 (about loop 3865 at
60.05/70.09 Hz from the exit's capture frame 3406, cycle-2 frame 1879 =
loop 3684). The cycle-2 dump holds 2095 frames.

### 44-A.4 The fix, its assertions and mutations

`fighter_15350`, `fighter_152d4`, `fighter_15208`, `fighter_1527c`,
`fighter_151c0`, `fighter_15160`, `fighter_151a0` (fighter.c) and
`anim_code_153D8`, `anim_code_1543C`, `anim_code_37CFC`, `actor_proc_2910C`
(actors.c) are registered in `actors_init`; `spawn_anim_opcode`'s case `0x0C`
takes `a5 = W(+0x28) & 0x4000`.

- `check_char3_2425` (`test_fight.c`, last in `test_fight`): A 0x15350
  through `hit_reaction_apply(0, 0x25)` and directly on side 1, and the
  `0x468D8` gate (other side's `+0x52` = 7; `0x22BEC` with `+0x24` = 0 and
  `+0x54` != 2; `+0x54` = 2 fails the arm). B 0x152D4's gates (`+0x57`,
  the 0x28 boundary, a negative `+0x88`), its stream, and the `0x2BD44` row.
  C 0x15208 on `gr_seed`'s passing context: the box edges 0x1B80/0x1B81
  (x, box a) and 0x18C0/0x18C1 (y, box b), the `+0x88` override (0, -1, 2),
  flags 1/4/7/8/9/0xD each firing alone (0xD with slot 1 in the `0x39CC8`
  pose, which also clears slot 0's `+0x8A`; 0xE's pass shows as the
  `+0x86` = 0x1234 mark, flag 5 as the box edges), and 0x19020. D 0x1527C (the `0x39834` count and
  `b`, `0x36D20` on the other slot, `0x188AC`'s y = 0, the `+0x43` mask,
  `+0x57`). E/F/G 0x151C0, 0x15160, 0x151A0 (check_reaction's seeds). H the
  three stream targets on pool records. I the opcode-0x0C a5, with and
  without the parent's bit. J `0x2910C` over eight hand-linked nodes. It
  snapshots and restores every data-object byte it writes. A temporary
  whole-data-object diff around it (reverted) showed 37 bytes, the same 37
  as a bare `actors_reset()` there: its pool tests leave the pool reset, as
  `check_trex_breath` does.
- The driver: after loop 3422 the raptor is on `0xD311C` in 9/7/0/`+0x57` 0
  with `0x152D4/0x15208/0x1527C`; after loop 3431 its `+0x4B` = `0x6A`, the
  child on `0xD318E` with `+0x14` = the slot; after loop 3538 the first
  particle's `+0x34` = `0x58`; after loop 3547 its node is in phase 1 on
  `0xE8D16`; after loop 3551 the raptor is on `0xD3082` in 9/7/0 with `+0x0C`
  = 0 and `0x15160/0x151A0`; after loop 3557 its `+0x4B` = `0x6B`, the child
  on `0xD30F4`; after loop 3631 the in-use node list is empty.
  After loop 3567 the ape is on `0xE4406` in 0x10/0x0A/2 (`0x151A0`).
  `fe_cyc2_n` 1695 -> 2095.

**Mutations** (`scratchpad/t32/mut32.py`; `mut32a..g.log`): 79 single-site
edits, 69 in unit mode and 10 in driver mode, sources restored and checked
with `cmp`. 75 fail an assertion: the gate's side, every stream, hold and
store of the seven callbacks, the `0x152D4` gates (a `+0x57` = 0 case was
added after the `!= 1 -> > 1` mutant first survived), the row stride, both
box tables, each of `0x15208`'s eight flag stores deleted (1, 4, 5, 7, 8, 9,
0xD, 0xE; the review found the deletion of `flags[0xD] = 0` surviving the
first suite, whose script had only changed flags 5/7/9, so C4 gained the
flag-0xD case), `0x15160`'s flags 0/1/8, the `0x3B714` argument
order, the three stream targets' gates and fields, the opcode-`0x0C` a5, and
`0x2910C`'s landing boundary, node return, phase compares, streams, hold and
`+0x59`; in driver mode the unregistrations of `0x15350`, `0x151C0`,
`0x151A0`, `0x15208`, `0x153D8`, `0x1543C`, `0x2910C` and the old a5. One
hangs the suite (`0x2910C` reading the next node after the body walks the
circular free list forever; caught by the timeout). One is equivalent: the
first of `0x15350`'s two `+0x57` = 0 stores (`0x15388`), which `0x153BB`
rewrites with no read between. Unregistering `0x15160` or `0x152D4` changes
nothing the driver samples (and the attract2 counts are identical without
`0x15160`); their unit-mode twins fail the registration checks.

### 44-A.5 Measured

| measurement | before (`632f3cd`) | `b947895` |
|---|---|---|
| `FE_LOOPS`, cycle-2 frames | 3500, 1695 | 3900, 2095 |
| attract2 `[1885..3616]` clean/splice/trans/unexpl/black | 771/430/8/517/6 | 946/554/15/211/6 |
| attract2 first unexplained (2384 allowed) | 3099 | **3257** (raw 7697) |
| front-end `[560..1884]`, demo-fight | 517/801/3/2; empty, N 1886 | unchanged |
| polled logic equal to the original through | f = 4308 | f = 4570 (the demo's end) |
| non-stub `fn_resolve` misses in the window | `0x15350` 3422, `0x151C0` 3551 | none (`0x29B74`, `0x41578` once each) |

`make verify` on `b947895` (EXIT 0, 0 compiler warnings): title 54/55/2/0
and 54/57/0/0; smk 120/120, 41/41; C-vs-Python 9866; front-end
`[560..1884]` 517/801/3/2 with 832/833 allowed; demo-fight empty, N 1886;
attract2 exhibited window `[1886..3405]`, first unexplained 3257 (raw 7697)
= N; symbols.h idempotent.

Attribution (single `PR_FRONTEND_DUMP` runs with one registration dropped,
reverted): without `0x2910C` the first unexplained frame is 3246; without
`0x151A0` it stays 3257 but 349 frames are unexplained (211 with it);
without `0x15160` the classification is identical.

**3257, characterised.** See 44-A.3 (4). 3258..3405 are explained, and 3406
is all-black (dropped as an artifact). 3407 on follows the second demo's exit
(cycle-2 frame 1879, loop 3684) into the attract's third cycle, the next
region (capture 3408 differs from the port's static frames 1879..1881 by 6786
bytes).

**Reach** (a temporary print probe, reverted): `0x15208` and `0x152D4` run
99 times (f = 4310..4408), `0x15160` 15 times (f = 4439..4453), `0x151A0`
once at f = 4453 (its `0x3B714` puts the ape in 0x10/0x0A/2 on `0xE4406`,
poll-equal; the driver samples it after loop 3567), `0x37CFC` 25 times,
always on its `+0x53 == 7` return. `0x1527C` (the `+0x1C` the winner body
calls) and so `0x152D4`'s `+0x57 == 1` arm, and `0x37CFC`'s kill arm, are
not reached: ported and unit-tested only.

**Named gaps.** 3257's lazy load of `s16spisd.gra` by the voice path
(`0x2C3FC`, spec §7), which the port does not run, and its read-stall
presentation (§35.1). The call that triggers it is not pinned.
(Closed by §45-A: the trigger is `0x1543C`'s `0x2C3FC(0x4D)`, now ported,
and 3257 is explained.)

**Merge note.** `0x2910C` was ported twice in parallel: here as `actor_proc_2910C` and on the `gap2-2910c` branch (§42-A) as `actor_type_0a19_update`. The merge kept §42-A's reviewed port and dropped this branch's copy; the two are the same walk over the raw (same stores, phases and streams). The ratchet N = 3257 was re-measured on the merged tree.

## 43-A. The mode-`0x22`/`0x24` pass `0x4D2D0`, its refill `0x4987C`, and the volleyball's `0x4C60C` (named-gap batch 3, branch `gap3-4d2d0`)

**Result in one line.** The named gaps §42-C left, the mode-`0x22`/`0x24`
effects pass `0x4D2D0` (the caller of `0x4D7A4`) and `0x4CB18`'s flag-1 caller
`0x4C60C`, are ported from the raw with every unported callee: `0x4D224`,
`0x4D108`, `0x4D150` and `0x4987C` under `0x4D2D0`; `0x4C784`, `0x4CC0C` and
the text call `0x2F510` under `0x4C60C`. `0x4987C` is also the effects tail's
callee (`0x4A616`), which is now wired. `0x4D2D0` and `0x4C60C` have no port
caller (their mode frames are unported), so they are unit-tested; the
`0x4A616` wiring is the one change a demo run can see (43-A.5).

### 43-A.1 The raw (Ghidra `disassemble_function`, `read_memory`, fixups applied)

- **`0x4D2D0`** (no arguments; a 4-byte frame of two words zeroed at
  `0x4D2E1`/`0x4D2E6`). It walks `DS_0010884C` (EBX = entry, EDI = next, ECX
  = the actor `+8`). Per entry: `inc word [esp + eax*2]` with AL = byte
  `0x104B1A` (`0x4D313`); SI = (u16)(`+0x48` − `0x20`) (`movzx si` then `sub
  esi,0x20`: only the low word is passed, `mov dx,si` after `xor edx,edx`);
  `0x4D7A4(entry, si)` when the word `0x104B00` is `0x22` (`0x4D317`). Then,
  when `[0x104AC4]` ≤ 1 (`cmp …,1; jg`), the type is not 6 and `+0x1C & 0x44`
  is 0 (`0x4D33B..0x4D348`), the stop: the actor's `+0x38`/`+0x34`/`+0x36`
  = 0, `+0x1C |= 4`, `0x4AC38(actor.+0x14, (u8)actor.+0x48 − 0x20)` when
  `+0x14` is non-zero (a 32-bit index, as `0x4AC18`), type 8. Otherwise the
  jump table `0x4D2AC` (`ja` above 8 to `0x4D3A4`): 0 and 7 `0x4D3A4`
  (`0x4D224`), 1 `0x4D3B5`, 2 `0x4D41C`, 3 `0x4D43F`, 4 `0x4D502`, 5
  `0x4D5F8`, 6 `0x4D649`, 8 `0x4D747` (nothing).
  - 1: `0x49C78`'s case 1 (`0x4BD4C` first while `DS_001088C2`; the arrival
    `0x4AC38` when |x − `+0x14`| ≤ the `+0x34` step, signed `jg`).
  - 2: the `+0x18` word counts down (signed `jg`), then `0x4AC38`.
  - 3: `+0x2C` = `0x496AC(y)`; at or below the zero-extended word `0xBD898`
    (`jg`): `+0x28 |= 0x4000` when `0x2BE1C(actor, the slot's record)` > 0;
    `rng(2)` non-zero → `0xC9634[si]` at 2.0 (`push 0x40000000`), else
    `0xC9724[si]` at 5.0 (`push 0x40a00000`); `+0x38`/`+0x34` = 0; `+0x18` =
    `rng(0x3C)` + `0x3C`; type 4; `+0x1C |= 0x80`. (`0x49C78`'s case 3 has no
    `rng(2)` and no `0xC9724`.)
  - 4: `+0x18` counts down (signed `jg`); at zero `rng(2)` non-zero re-arms
    `+0x18` = `rng(0x3C)` + `0x3C`; else `rng(2)` non-zero with the actor's
    x in (−`0x4D00`, `0x4D00`) (`cmp ebp,0xffffb300; jle`, `cmp ebp,0x4d00;
    jge`) walks: `rng(2)` non-zero → `+0x34` = `0x80`, `+0x29 &= 0xBF`, else
    `0xFF80`, `|= 0x40`; `0xC95D4[si]` at 3.0; type 4 (`0x4D5B0`); `+0x18` =
    `rng(0x1E)` + `0x3C`. Otherwise the climb: `0xC95EC[si]` at 3.0, `+0x38`
    = `0x20` (`0x49C78`'s is `0x40` with a facing `+0x34`), type 5, `+0x1C &=
    0x7F`.
  - 5: `0x49C78`'s case 5.
  - 6: `0x49C78`'s case 6, with the stop above inserted after the shadow's
    kill when `[0x104AC4]` ≤ 1 (`0x4D69A..0x4D6E5`); the landing stream and
    type (`0xC973C[si]` at 2.0 type 8, else `0xC9544[si]` at 3.0 type 4)
    follow and overwrite its type.
  After the walk (`0x4D755`): `DS_001088C2` = 0; DL = the byte
  `0x1077B0 + side * 0x94 + 0x81` (`[eax*4 + 0x107831]` with EAX = side *
  0x25, after `xor edx,ecx` zeroes EDX), SI = the side's word; when (i16)(DL
  − SI) > 0 (`test ax,ax; jle`), `0x4987C(EAX = side, EDX = movsx ax, EBX =
  0)`. The only writers of `0x104B1A` are `0x269D0` (0) and `0x269DF` (1)
  (`get_xrefs_to`), so the word index stays inside the frame.
- **`0x4D224`** (EAX = entry, EDX = si): `DS_001088C2` and `0x4BD4C` ≠ 0 →
  return; `0x4D108` (AL) ≠ 0 → return; `0x4D150` ≠ 0 → return; the slot's
  `+0x42` bit 1 or `DS_001088B2[+0x21]` → `0x4B3F0(EBX = 0)`, non-zero →
  return; `+0x42` bit 0 or `DS_0010889E[+0x21]` → `0x4B430(EBX = 0)`.
- **`0x4D108`**: `rng(0x3C)` ≠ 0 → AL = 0. Else `0xC95BC[si]` at 3.0, `+0x1A`
  = the actor's word `+0x32`, `+0x38` = `0xFFE0`, type 3, AL = 1.
- **`0x4D150`**: `rng(0x3C)` ≠ 0 → 0. Else ESI = `rng(4)` * 3 << 10
  (`rng(2)` non-zero) or its negation; `+0x14` = `0x2BE4C(actor,
  0x2BE00(slot[byte 0x104B1A].record) + ESI)`; type 1; `+0x34` = `0x80` and
  `+0x29 &= 0xBF` when `+0x14` > the actor's x (signed `jle`), else `0xFF80`
  and `|= 0x40`; `0xC95D4[si]` at 3.0 (ECX = si); 1.
- **`0x4987C`** (EAX = side, EDX = count, EBX = kind; `[esp+0x28]` the
  direction flag): nothing for count ≤ 0 (`jle`). Per entry (`jl` on the
  count): the free list `0x1083C4`'s head (empty → return), `0x249D0`, then
  `0x249B0(0x10884C, entry)`; word `+0x1C` = 0. Kind ≠ 0: count 1 →
  `[kind*4 + 0xC9538]`; kind 1 → `[i*4 + 0xC953C]`; else `[(i ^ 1)*4 +
  0xC953C]` with `+0x1C |= 1` for i = 0 (both). Kind 0: `0xC9524[0x49388(side)]`
  with `+0x10` = `0x29CDC(side, slot +0x7A)`. Mode `0x22`: x =
  `0x2BE00(side's record)` ∓ `0x5780` by `rng(2)` (non-zero: −, flag 1),
  through `0x2BE4C(side's record, ·)`; else − when the side's `0x2BE00` is
  below the other's (signed `jge`, flag 1), + otherwise. A pair's first
  flyer (kind ≠ 0, count 2, i = 0) goes `0x780` further out. y = the word
  `0xBD898` (mode `0x22`) or it − `0x200` for kind ≠ 0; for kind 0 the
  record's `+0x30 >> 16` + `0x400` + `rng(0x300)`. `0x2AE14(desc, x, ECX =
  y, EBX = 0; 0)`; entry `+8` = actor, actor `+0x14` = entry, `+0x0C` = the
  slot, `+0x21` = side, `+0x1F` = 0, actor `+0x2C` = `0x496AC(y)`, actor
  `+0x29 |= 0x10`, entry `+0x10` = 0; the record's `+0x51` non-zero or mode
  `0x22` → actor `+0x2E += 4`, `+0x4E` = 1. Kind ≠ 0: `+0x34` = flag ?
  `0xC0` : `0xFF40` (entry `+0x1C` bit 0) or flag ? `0x100` : `0xFF00`;
  flag 0 → `+0x29 |= 0x40`; type 7, `+0x1C |= 0x10`. Kind 0: `0x4B144(entry,
  (u16)(+0x48 − 0x20))`. `0x5D7DC` pushes EBX/EDX, so the x survives its
  draws.
- **The effects tail** (`0x4A5A6..0x4A61B`): outside modes 7/8/9, BH =
  `DS_001088BF`; BH − 1 ≤ 3 → the table `0x49C68` = `0x4A5DC, 0x4A5E3,
  0x4A5F4, 0x4A605` sets (EDX, EBX) = (1, 1), (1, 2), (2, 1), (2, 2), then
  `rng(2)` into EAX and `0x4987C`; then `DS_001088BF` = 0.
- **`0x4C60C`** (EAX = entry, EDX = si): `0x17D30` on the actor's pset point
  (BX = 0, ECX = 0 preserved by `0x17D30`'s `push ecx`); 0 → return; above 2
  → 1. With side = hit − 1 and ch = the slot's `+0x7A`: above 6 (`ja`)
  nothing; the table `0x4C5F0` = `0x4C68E` (ch 0, 3, 5), `0x4C6B0` (1, 4,
  6), `0x4C69E` (2): at `0x4C68E` `+0x5F` = 0 → `0x4C784(side)` and return,
  at `0x4C69E` `+0x5F` = 1 → `0x4C784(side)` and return, else (and at
  `0x4C6B0`) ECX = 1: word `0x108898` = 0, `+0x20` = side, `+0x1C &= 0xF7`,
  `+0x1F += 1`, the `+0x4A` release (`0x1014F4` + k * `0x68` + `0x4B` = 0,
  `+0x2A &= 0xF7`, `+0x29 &= 0xBF`, `+0x4A` = 0, `+0x1C &= 0xBF`,
  `0x1088AE[+0x21] += 1`; no `0x1088B2` store), then by the slot's `+0x5F`
  in `0xC..0xF` `0x4CB18(entry, si, 1, side)` and `+0x1C |= 8`, else
  `0x4CB18(entry, si, 0, side)` (ECX = hit − 1 in both).
- **`0x4C784`** (EAX = side; the ball is `[0x108864]`): hs = the ball's
  `+0x21`; `0x2AE14(0xC976C, the ball actor's +0x18, ECX = word 0xBD898 −
  0x100, EBX = its +0x1C; 0x4000 unless the side's record +0x28 has 0x4000)`
  then `0x2BC30(that actor, 0xEF65A, 3.0)` (the actor is not kept); the
  ball's shadow `+0x10` killed and zeroed, the ball's actor killed; the
  voices `0x2C3FC(0xD4/0xD5)`, `0x2C3FC(0xD6)`, `0x2C3FC(0xCE)`; other =
  side ^ 1: `0x10889C[other] += 1`; `0x2BC30([0x10886C], [0x108884] >
  other's record +0x18 (jle) ? 0xEF680 : 0xEF6AC, 3.0)`; word `0x1088AC` =
  `0x69`; word `0x108898` = other ? 2 : 1 (`setnz`, `inc`). Score ≥ 3
  (`jl`): the ball's `+0x1F` = 0, `0x4CC0C()` when the word `0x1088A0` is 0,
  `0x108864` = 0, return. Else `0x2F510(−1, 6, 0x1C500(0x56), 0x4000)`
  ("BALL EATEN!"); a free entry (none → return) onto `0x10884C`;
  `0xC9524[0x49388(other)]` with `+0x10` = `0x29CDC(side, [0x1077A8 +
  side*4].+0x7A)`; base = `0x2BE00([0x108868])`; the side's `0x2BE00` below
  the other's (signed) → x = base − `0x2580`, target base + `0x1740`, else
  x = base + `0x2580`, target base − `0x1740`; `0x2AE14(desc,
  0x2BE4C([0x108868], x), ECX = word 0xBD898, 0; 0)`; the entry and actor
  links as `0x4987C`'s, `+0x21` = hs, `+0x0C` = hs's slot, actor `+0x28`
  word = 0, word `+0x1C` = 0; record `+0x51` → `+0x2E += 4`, `+0x4E` = 1
  (no mode clause); type 1, `0x108864` = entry, `+0x14` = target, `+0x1C |=
  0x20`; `+0x34`/`+0x29` by `0x2BE00(actor)` < target (signed `jge`);
  `0xC95D4[si]` at 3.0.
- **`0x4CC0C`**: `[0x108868]`/`[0x10886C]` `+0x36` = `0xFE5C`, word
  `0x1088A0` = `0x3C`; `0x2F510` (col, row, string, mode) (`0xA`, 8, `0x57`,
  `0x5000`), (`0x13`, 4, `0x58`, 0); `0x2F4BC` (1, 7, `0x59`, 0), (`0x26`, 7,
  `0x5A`, 0); `0x2F510` (`0x13`, 1, `0x5B`, `0x5000`), (7, 8, `0x5C`,
  `0x5000`), (`0x16`, 8, `0x5D`, `0x5000`); equal bytes `0x10889C`/`0x10889D`
  → (−1, 6, `0x5E`, `0x4000`), (−1, 9, `0x5F`, `0x4000`); else (−1, 6, below
  (`setbe`) ? `0x16` : `0x17`, `0x4000`), (−1, 9, `0x5E`, `0x4000`).
  `0x1C500` pushes EDX and `0x474E4` pushes ECX, so the row and mode survive
  the string fetch. The strings (`ENGLISH.TXT`): `0x16` "RIGHT PLAYER",
  `0x17` "LEFT PLAYER", `0x56` "BALL EATEN!", `0x57..0x5D` spaces, `0x5E`
  "VOLLEYBALL GAME", `0x5F` "TIED".
- **`0x2F510`**: `push esi; or cl,2; mov esi,[0x105F34]; call 0x2F198; mov
  [0x105F34],esi` — `0x2F4BC` with the mode ORed with 2.
- Tables (`read_memory`): `0xC9524` = `0xBB470..0xBB4D4` (step `0x14`,
  `+0x48` bytes `0x20..0x25`), `0xC9538` = `0xBB4D4` (so kind 1/2 read
  `0xC953C` = `0xBB59C`, `0xBB5B0`: `+0x48` `0x20`/`0x23`, `+6` word `0x40`,
  `+8` word `0x1000`); `0xC9724` = `0xEE36E, 0xEE6AE, 0xEEA7E, 0xEEE5C,
  0xEF226, 0xEF5BA`; `0xC976C` = { `0xEF65A`, …, `+0x10` `0x105FF3C` }.
  `0xEF65A`, `0xEF680` and `0xEF6AC` start with opcode words.

### 43-A.2 Entrances (`get_xrefs_to`, a rel32 CALL/JMP/Jcc scan of the code object and a dword scan of both fixed-up objects)

- `0x4D2D0`: `0x26D28` (`0x26C8C`) and `0x26FF0` (`0x26F58`), the mode
  `0x22`/`0x24` frames; neither is ported. No dword.
- `0x4D224`: `0x4D3AB`. `0x4D108`: `0x4D247`. `0x4D150`: `0x4D254`. No dword.
- `0x4987C`: `0x4A616` (`0x49C78`) and `0x4D792` (`0x4D2D0`). No dword.
- `0x4C60C`: `0x4C21B` in `0x4BF18`, whose one caller is `0x26687` in the
  mode-`0x21` frame `0x26540`; neither is ported. No dword.
- `0x4C784`: `0x4C697`, `0x4C6A9` (both `0x4C60C`). `0x4CC0C`: `0x4C356`,
  `0x4C429` (`0x4BF18`) and `0x4C90D` (`0x4C784`). No dword.
- `0x2F510`: 67 call sites (`0x1F7F4..0x4F13A`); none was ported before.
- The jump tables `0x4D2AC`, `0x4C5F0` and `0x49C68` are each read by one
  `jmp` (`0x4D3A0`, `0x4C68A`, `0x4A5D8`); their targets have no other
  dword.

### 43-A.3 The port

- `fight.c`: `fight_4987c`, `fight_4d2d0`, `fight_4cc0c`, `fight_4c784` and
  `fight_4c60c` (exported, for the unported mode frames and `0x4BF18`), and
  the static `fight_4d108`, `fight_4d150`, `fight_4d224`; local `#define`s
  for `0xC9724` and `0xC976C`. The effects tail calls `fight_4987c` with the
  `0x49C68` pairs in place of the lone `rng_next(2)`.
- `actors.c`/`actors.h`: `text_cursor_hold_font2` (`0x2F510`).
- `PORT:` notes: the voices; `0x4D2D0`'s count word and refill for a
  `0x104B1A` above 1 (it would index the raw's saved registers; the two
  writers store 0 and 1).
- No registration: none of the addresses is stored in data.

### 43-A.4 The assertions and mutations

Three checks in `test_fight.c` (after `check_arena_backdrop`), on
`check_point_trample`'s fixture (`ph_seed`) with an entry E (si 3, side byte
1) whose actor R is pset 5, the eleven stream tables' entry 3 pointed at
distinct scratch streams, and RNG seeds picked for their draws with the LCG
`0x5D7DC` (`scratchpad/g3-4d2d0/seeds.py`, `seeds2.py`; each chosen draw
also differs from the draw of a range one larger). Each check saves and
restores the whole data object, the actor record pool, the running pset
pool, the `FIGHT_*` scratch and `mem[0..0x3F]`. `mz_seed` first takes the pool records 0..8
off the actor free list `DS_00105B3C` (their psets are the fixture's), so no
spawn depends on the tests run before (review: after main's
`check_char3_2425` the head was record 2 and a flyer overwrote fighter 1's
pset; reproduced by pushing records 2..5 to the head, 3 failures, none with
the seeding). A temporary whole-`mem[]`
diff around the three showed only `mem[0x18..0x1B]` (pset 0's `+0x18`, the
out-of-pool kill of §42-D) before that range was added, and nothing after
(re-run on the final tests). The low range also carries sentinels: `0x1E`
(`0x4AC38` on entry 0), `0x28` bit 3 (`0x2B150` on record 0), `0x29` (a
`[entry − 8]` slip) and `0x32` (a shadow 0 followed).

- `check_mode22_pass`: the empty list; the stop (both signs of
  `DS_00104AC4`, every field, `0x4AC38`'s stream and `+0x29`), each of its
  four gates, `+0x1C` bit 0 not gating, and the stop without an owner (its
  own zeroing; no `0x4AC38`); type 8; type 2's countdown and signed edge;
  type 1's arrival edges (|d| = step, step + 1, a negative d, a negative step
  at step and step + 1) and `0x4BD4C` with and without `DS_001088C2`; type 3
  above and at the layer, both landing streams, the `0x2BE1C` flip (positive,
  negative, zero; the slot's record, not another), the timer and draw count;
  type 4's countdown, re-arm, both walk directions at x inside both edges,
  the climb at both x edges and on the second `rng(2)`; type 5's signed
  edges; type 6 airborne both ways (`+0x36` 0 is not falling, a height sum of
  1 still airborne), the shadow, the landing for both `0xC973C` arms with and
  without the stop, and without a shadow or owner; types 0, 7 and 9 into
  `0x4D108`; `0x4D150` both directions, `DS_00104B1A`'s side, the signed
  `jle` edge; the `0x4B3F0`/`0x4B430` gates (each flag, the fall-through,
  the other side's bytes) and `0x4BD4C` first; mode `0x22`'s prelude against
  mode `0x24`; the refill (one entry for side 1 with its descriptor, x, y,
  `+0x2C` and links; none at equality or on the other side's `+0x81`; two
  for side 0).
- `check_flyers_4987c`: kind 1 and 2 for one flyer (kind 1 from a copy of
  `0xBB59C` with a scratch stream and `+0x28` word `0x4000`, so `+0x29 |=
  0x10` is `0x4987C`'s own and keeps the spawn's bit 6), every entry and
  actor field including `+0x1C` (EBX) and `+0x4A`; the pair for kind 1 and 2
  and side 1 (order, bit 0, the `0x780`, the direction and the whole
  `+0x29`); mode `0x22`'s `rng(2)` both ways (seed `0x103`: `rng(3)` would be
  1), its pair and the `+0x51` adjustment; equal fighter x; counts 0 and −1
  and an empty free list; kind 0 in mode 3 (descriptor, `0x29CDC` value, y,
  `0x4B144`'s stream by the actor's si and type); the effects tail for
  `DS_001088BF` 1..5 and in modes 7, 8 and 9.
- `check_volleyball`: `0x4C60C` struck with the slot `+0x5F` at `0`, `0xB`,
  `0xC`, `0xF`, `0x10` (the flag's `+0x36` and `+0x1C` bit 3; no release
  without `+0x4A`), the release, both sides hitting (3 → side 0), side 1
  alone (`+0x20` = 1, side 1's move), no hit, characters 7 and `0xFF`, and
  the eater table for all of 0..6 (characters 5 and 6 need the side's raw box
  x 4 and the point 8 units left: `0x15B90` shifts their box off the
  fixture's 8-pixel sprite, and a scan of the point found hits for x offsets
  −37..−1); `0x4C784` for both sides (the spit actor's fields, flip and
  `+0x4A`, the kills, the score, `0xEF680`/`0xEF6AC` with the `jle` edge,
  `0x1088AC`, `0x108898`, row 6 equal to `0x2F4BC`'s own drawing of string
  `0x56` in mode `0x4002` and the cursor kept, the free list emptied, the new
  ball's descriptor from the other side's `0x108860` range, `+0x10` from
  `0x1077A8[side]`, x, target, the equal-x `jge` edge, `+0x2C`, `+0x2E`/`+0x4E`
  by the owner's `+0x51`, links, stream and both walk directions), a ball
  without a shadow, the empty free list, the third point with and without
  `DS_001088A0`; `0x4CC0C`'s seven space strings (one planted cell each only
  that string covers, and a cell between two runs that must stay) and rows
  6/9 compared with `0x2F4BC`'s drawing of the expected strings for the three
  score cases; `0x2F510` against `0x2F4BC` with and without the `| 2`.

**Mutations** (`scratchpad/g3-4d2d0/mutgen.py`, `mut.py`, `mut4.out`): 1041
single-line edits of the new code in `fight.c` (from `0x4987C` to
`0x4C60C`), the effects-tail wiring and `0x2F510` (each simple statement
deleted, up to two literals + 1, up to two operator flips per line). 12 do
not compile. Of the 1029 others, 977 fail the suite (1..249 assertions, 76 by
a crash, 1 by a hang); the first sweep's 118 survivors led to the added
assertions above. The 52 left are equivalent:

- a count ≤ 0 reaching `0x4987C` does nothing (2: `count <= 0` → `< 0`, the
  refill's `> 0` → `>= 0`);
- reading `+0x1D` right after the word `+0x1C` was zeroed (2);
- `flag = 1` deleted leaves it uninitialised (1; the compiled code keeps 1);
- a non-zero 1 → 2 read only for its truth (8: `flag`, the `return 1`s,
  `struck`, `0x4CB18`'s flag);
- `0x2AE14`'s stack flag 0 → 1 or `0x4000` → `0x4001`/inverted (5): the
  port's `actor_spawn` reads only its `0x400` bit, bits 8..15 `& 0x44` and the
  high word, so bit 0 is not observable;
- the `PORT:` side guard (5: `0x104B1A` is only ever 0 or 1);
- negating a zero (4);
- case 4's walk re-storing type 4 (2, one of them at entry − `0x1E`, the
  preceding scratch);
- case 6's stop zeroing `+0x34`/`+0x36`, which the landing zeroes again (6);
- case 6's stop `0x4AC38` stream index and type 8, both overwritten by the
  landing (8; one writes `+0x1F`, which the landing zeroes);
- `a <= b` → `<` in `0x4CC0C` (1; equality is handled first);
- the spit's `0x2BC30` restarting the stream the spawn just started (1);
- the new ball's `+0x29` edits right after its `+0x28` word is zeroed (6:
  every dust descriptor's `+0x28` low byte is 0, and one writes the
  preceding pool record's `+0x3F`, which nothing reads);
- `0x2BE00(new ball) < target` → `<=` (1): the two differ by ±`0x3CC0` by
  construction.

The sources were restored by the script and compared with `cmp`.

### 43-A.5 Measured and remaining gaps

- `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all checks passed, 0 compiler
  warnings. The drivers and `make verify` were not run (the batch controller
  runs them after the merge).
- **The effects-tail wiring does not move the demo (measured in review).**
  With `DS_001088BF` in 1..4 the port now spawns `0x4987C`'s type-7 flyers
  where it drew `rng(2)` alone. In the demo run `DS_001088BF` stays 0 on
  every fight frame: neither setter is reached (`0x391CA`'s combo block at
  `0x39195` is never entered, and `0x14D7C`, the `0x14DA4` setter, is never
  called), so the `0x4A5A6` gate never fires. The front-end driver's dumps
  (`PR_FRONTEND_DET`) are byte-identical to 713833c's, and main with this
  branch merged is byte-identical to main. N is unchanged: demo-fight exact
  (N = 1886); attract cycle-2 3099 on 713833c and 3257 on main. The wiring
  is one hunk (the `0x4A5A6` block of `fight_effects_pass`) with its test
  block F and the mode 7/8/9 loop in `check_flyers_4987c`.
- `0x4D2D0` and `0x4C60C` are unreachable until the mode frames `0x26C8C`,
  `0x26F58` (modes `0x22`/`0x24`) and `0x26540` with its pass `0x4BF18`
  (mode `0x21`) are ported; `0x4CC0C`'s two `0x4BF18` sites wait on the same.
- Named gaps: the voices (`0x2C3FC` at `0x4C85C`, `0x4C866`, `0x4C875`);
  `0x4D2D0`'s frame words for a `0x104B1A` above 1 (not reachable, 43-A.1).
- The report is in this worktree's `.superpowers/sdd/2026-09-25-roar-timing/`
  (the isolation guard refused the main checkout's path).

## 45-A. The loader's `- LOADING -` screen at capture 3257: the voice dispatcher `0x2C3FC`, the sound module's sample path and the spawns' sound banks (roar-timing Task 33, `b05adcc`)

**Result in one line.** Capture 3257 is the loader's `- LOADING -` text drawn
straight onto the VGA aperture over the frame the screen holds, when
`0x1543C`'s voice `0x2C3FC(0x4D)` reads `s16spisd.gra` (INDEX entry 64) for
the first time at f = 4444 (loop 3557). The port now runs that path: the voice
dispatcher `0x2C3FC` and the sound module's memory-visible parts, the fighter
spawn's sound-bank tail, the DIG driver handle `DS_001028C8`, and a dump seam
for the loader's screen. Capture 3257 is explained (clean), and so is 2384,
whose allowance by name is dropped. The first unexplained frame is now 3408,
the attract's third cycle: the high-score table after the second demo, which
the port does not draw. N 3257 -> 3408 (`b05adcc`). Two raw-wins
corrections come with it: the image is mapped before the init chain, and the
read-stall rate of §9.6 is re-derived from the bytes the original reads.

### 45-A.1 The raw (Ghidra `decompile_function`/`disassemble_function`, fixups applied)

- **`0x2C3FC`** (1268 B). EAX = the voice id. Id 0 returns 0 (`0x2C401`); id
  `0x100` becomes 0 (`0x2C409`/`0x2C410`). The record is the 12-byte
  `DS_000BBDC8[id]`: +0 the case, +4 a handle, +8 a byte. `cmp al,6; ja
  0x2C8E8` returns 0 above 6, else `jmp [0x2C3E0 + 4*case]`. The table reads
  `0x2C8DD, 0x2C437, 0x2C473, 0x2C4C6, 0x2C89B, 0x2C57F, 0x2C8E8`.
  - Case 0 (`0x2C8DD`): AL = 1.
  - Case 1 (`0x2C437`): `DS_00105D5C` = the handle; `0x1CA14(DS_00105D5C,
    byte)`; AL = 1.
  - Case 2 (`0x2C473`): `0x1CE70(handle)`; when it returns non-zero, AL = 0;
    otherwise `0x1CC28(handle, byte)` and AL = 1.
  - Case 3 (`0x2C4C6`): the paired ids. `0x46`: `0x1CE70(0x2886158)`, then
    `0x1CC28(0x28847C9, 0)` and `0x1CC28(0x2886158, 0)`. `0x4D`:
    `0x1CE70(0x1201D606)`, then `0x1CC28(0x1201D606, 0)` and
    `0x1CC28(0x2001513C, 0)`. `0x5D`: `0x1CE70(0x281A726)`, then
    `0x1CC28(0x281A726, 0)` and `0x1CC28(0x2819183, 0)`. A playing first
    sample, or any other id, gives AL = 0.
  - Case 4 (`0x2C89B`): `0x1D238`, `0x1D244`, `DS_00105D5C` = `0x21`,
    `0x1CA14(0x2803E64, 0)`. Unless `0x1CE70(0x180122FD)` answers 1, it runs
    `0x1CD9C` and `0x1CC28(0x180122FD, 1)`. AL = 1.
  - Case 5 (`0x2C57F`): a compare tree on the id. Id 0 runs `0x1CA6C` and
    `0x1CD9C`. `0x22` runs `0x1CA6C` when `DS_00105D5C` is `0x1B..0x21`,
    `0x25` or `0x26`; `0x2B`/`0x2A`, `0x2D`/`0x2C`, `0x2F`/(`0x2E`, `0x30`),
    `0x33`/`0x32`, `0x3C`/`0x3B`, `0x55`/`0x54`, `0x57`/`0x56`, `0xE0`/`0xDF`
    and `0xE2`/(`0xE1`, `0xE3`) likewise. `0x3F`, `0x41`, `0x43`, `0x4C`,
    `0x4F`, `0x5B` and `0xF1` run `0x1CE04` on `0x1800EBC9`, `0x383B6F4`,
    `0x3837440`, `0x22008696`, `0x1501053C`, `0x1B01AF00` and `0x22018405`.
    AL = 1. The `cmp edx,0x100; jz 0x2C69C` after `0xF1` is dead: EDX was
    zeroed for id `0x100`.
  - Case 6 (`0x2C8E8`): AL = 0.
  EBX, EDX and EDI are pushed and popped. The shipped table has five case-0
  ids, 29 case-1, 154 case-2, three case-3 (`0x46`, `0x4D`, `0x5D`), one
  case-4 (3), 18 case-5 and 34 case-6 ids in `0..0xF3`.
- **The slots.** Four 0x18-byte records at `0x102860`: +0 the AIL sample
  handle (`0x1CF40`), +4 the queued resource handle, +8 its loop byte, +0xC
  the playing handle (moved there by `0x1CB18`), +0x10 the slot's buffer
  (`0x1D0BC`) and +0x14 the queue time (`0x500BB` = `DS_00101500`).
  `DS_001028C8` is the DIG driver (`0x1CF8E`: `AIL_install_DIG_INI`'s
  return), `DS_001028C0`/`C4` the MDI sequence/driver, `DS_001028CC` the
  pending song, `DS_001028D4`/`D9` the current song and its byte,
  `DS_001028DA`/`DB` the music and sample pause bytes.
- **`0x1CE70`**: AL = 0 without `DS_001028C8`. For each slot whose +0xC is
  the handle, `0x5DD03` status 4 answers AL = 1; any other status clears +0xC
  and the scan goes on (`0x1CEA5`).
- **`0x1CE04`**: the same scan. The first matching slot whose status is not 2
  is ended (`0x5DC8B`), re-inited (`0x5DC0F`) and cleared, AL = 1
  (`0x1CE5B`).
- **`0x1CD9C`**: AL = 0 without `DS_001028C8`. Otherwise each slot's +4 and
  +0xC are cleared, a slot whose status is not 2 is ended and re-inited, and
  AL = 1.
- **`0x1CC28`**: AL = 0 without `DS_001028C8` or with `DS_001028DB` set
  (`0x1CC37`/`0x1CC44`). Otherwise `EBP = 0x500BB()` and `0x1B544(handle)`
  (`0x1CC5D`). Then the slot choice (`0x1CC62..0x1CD8D`): a sample of at most
  `0x6000` bytes takes a slot with a buffer, no queued handle and a status
  other than 4; a larger one takes slot 0 on the same test. Failing that, the
  oldest slot (smallest +0x14) is ended and re-inited. Then +4 = the handle,
  +8 = the byte, +0x14 = the time; AL = 1.
- **`0x1CA14`**: `DS_001028D9` = DL, `DS_001028D4` = EAX; unless
  `DS_001028DA` == 1 or `DS_001028C0` == 0, `DS_001028CC` = EAX, AL = 1.
  **`0x1CA40`**: AL = 0 without `DS_001028C0`, else `0x5DEED` status == 4.
  **`0x1CA6C`**: `DS_001028D4` = 0, `DS_001028D9` = 0; when `DS_001028C0` is
  set and `0x1CA40` answers 1, `DS_001028CC` = 0 and `0x5DEAF` stops the
  sequence, AL = 1. **`0x1D238`**/**`0x1D244`**: clear `DS_001028DA` /
  `DS_001028DB`.
- **`0x33C78`'s tail** (`0x33E48..0x33EA6`). When `0x1CEBC` passes
  (`DS_001028C8` set, `DS_001028DB` clear), it reads `DL = [ESI+0x7A]` (ESI
  is the slot, `0x33C84..0x33C95`; +0x7A is the character, `0x33CB8`).
  `xor eax,eax; cmp dl,6; ja 0x33E98`, then the jump table `0x33C5C` (`0x33E69
  .. 0x33E93`) loads `DS_000BDB1C[ch]`. A non-zero handle goes through
  `0x1B544`, and so does the fixed `0x287B2F5` (entry 5, `s16sound.gra`).
  `DS_000BDB1C` = `0x1E005ABC, 0x1B007C54, 0x22000008, 0x1200808B, 0x2000410C,
  0x1501053C, 0x18004092` (entries 60, 54, 68, 36, 64, 42, 48, the sd banks).
- **`0x1543C`**'s tail (`0x15469`/`0x1546E`): `mov eax,0x4D; call 0x2C3FC`.
  AL is not read.
- **`0x1D018`** (the teardown): clears `DS_001028C8` at `0x1D0A9` after it
  stops the slots.

### 45-A.2 Entrances

`get_xrefs_to`: `0x1CC28`, `0x1CE70`, `0x1CE04`, `0x1CA14`, `0x1CA6C`,
`0x1D238` and `0x1D244` are called only from `0x2C3FC` (8, 5, 7, 2, 11, 1
and 1 sites). `0x1CD9C` is also called from `0x1D220`/`0x1D250`, the pause
toggles, which are unported. `0x1CA40` is also called from `0x1D1B0`. `0x1CEBC` is called
only at `0x33E48`. A rel32 scan of the code object finds 299 `call`s and 4
`jmp`s to `0x2C3FC`, 303 sites. Ghidra's list is complete at 300 references
from 104 functions (`total` 300 at `limit` 1000); the three it has no xref
for are `0x11C38` (a call, in the ported `0x11BCC`), `0x1550B` (a `jmp`) and
`0x30B34` (a call). No dword points at `0x2C3FC`. Only `0x1546E` is wired
here; the other 302 keep their `PORT:` comments, which now read "not wired
(record §45-A)" where they said "out of scope (spec §7)": the dispatcher is
ported, the call is not.

### 45-A.3 The measurement

1. **The original's reads** (`scratchpad/t33/idxpoll.py`: DOSBox-X with a
   memory file, polling every INDEX entry's +0xC loaded bit `0x20000000` with
   `DS_000EF6DC`, the state and the tick pair; `idx.log`). `DS_001028C8` is
   `0x2F2140` from boot. The loads are: 1, 2 and 7 at boot (f 0); 0 and 8 at
   f 691 (state 1). At the first demo's state-6 entry (f 1957): 21 (tk 3), 55
   (31), **60** (32), **5** (36), 33 (54) and **36** (55), then 32, 57 and 34.
   At the second demo's (f 3670): 27, 49, **54** and 51. At **f 4444, 64**.
   At f 4571, 20. The bold entries are sound banks. Every one follows the
   fighter bank it belongs to, which is `0x33C78`'s tail. At f 4444 the tick
   pair reads 802/802 before and 803/803 after the load, so the ~0.7-tick read
   leaves no gap in the tick count.
2. **The original's slots** (Task 32's whole-RAM snapshots `snap_4443/4445`).
   Slots 2 and 3 take the playing handles `0x2001513C` and `0x1201D606`
   (both queued at time `0x196A`, where the older queues read `0x1957`).
   Only `0x2C3FC(0x4D)` queues that pair. Entry 64 alone gains the loaded bit,
   because entry 36 was read at the first demo's spawn.
3. **The port before the fix** (a `res_resolve` print, reverted). There were
   no sound-bank reads: `DS_001028C8` read 0 at run time. The port's order
   was `game_audio_init` first, then `mem_load_le`, which rewrote the data
   object's BSS: the store was lost, and so were `DS_00101504/10/14`,
   `DS_000A2CAC` and `DS_000A2CB1` (the image holds zeros there).
4. **The port after the fix.** Its sound banks and fight reads are the
   original's, in the same order at the same frames: 21, 55, 60, 5, 33, 36,
   32, 57, 34 at f 1957; 27, 49, 54, 51 at f 3670; 64 at f 4444; 20 at
   f 4571. Two reads differ, neither a sound bank: s16title (7), which the
   original reads at boot and the port at the title state (cycle 2's loop
   1973, f 2860), and s16slabs/s16attrc (0, 8), which the original reads at
   f 691 in state 1 and the port at f 887 in state 2. The second predates this
   task (the front-end dump's allowed 832/833 are probably those screens).
5. **The screen.** 0x1B3AC draws the text through `0x51ED8` onto the aperture,
   which holds the last presented frame, and the gate presents the next frame
   in the same master-loop iteration (§35.1). A per-iteration dump never
   holds that screen. The new seam `res_set_screen_hook` runs right after the
   text is drawn, as movie.c's does for `0x1C740`. The front-end driver
   points it at the cycle-2 dump only (the movie seam's rule), which gains
   seven screens (2095 -> 2102 frames): loop 1973 (entry 7), four in loop
   2783 (27, 49, 54, 51), loop 3557 (64) and loop 3684 (20). The classifier:
   3256 = port 1756, **3257 = port 1757** (1756 with the text), 3258 = splice
   1758/1759; **2384** (text on black) and **3407** (raw 7853, text on black)
   are clean. 3406 is all-black.
6. **3408** (raw 7896) and every later frame to 3415 are unexplained. The
   best splice is port 1885 with ~19 000 bytes off from row 13: the
   high-score table (`1 TEENY WEENY GAMES 500000` ... `10 DVD 100`) over the
   bone pile with the T-rex figure. The port shows the bone pile and the
   credit line with neither (`scratchpad/t33/s3408.png`). The original read
   `s16hghsc.gra` at f 4571 (state 5), and so does the port. A different
   cause: the attract's high-score screen is unported. **N = 3408.**

### 45-A.4 The raw-wins corrections

- **The init order.** `game_init` mapped PRAGE.EXE after `game_audio_init`.
  The loader maps both objects before `0x1BEC4` runs, so the image is now
  mapped first and the chain's stores survive. Three readers see different
  values: `DS_00101514`, the BIOS base (reads of the zeroed
  `GAME_BIOS_BASE + 0x2D4/0x2D8` instead of `mem[0x2D4/0x2D8]`: the same
  zeros); `DS_000A2CB1` = 1 (so `game_shutdown` runs its `0x1D018` arm); and
  `DS_001028C8`. The top-level dump stays byte-identical (1382 frames).
- **The read-stall rate** (§9.6, `res.h`). The state-6 entry's 55 ticks
  covered six reads, not three. The original re-syncs after each one at
  ticks 3, 31, 32, 36, 54 and 55, the values §9.6's own `150C` distribution
  lists. The reads are s16beach 233128 + s16rex 3812084 + s16rexsd 81192 +
  s16sound 586942 + s16cob 2438316 + s16cobsd 145435 = 7297097 bytes (each
  size is its INDEX entry's, `res_size()`), and 7297097 / 55 = 132674.49,
  floored as 117882 was: **132674 bytes/tick**. Only the tick pair moves.
  The flier's `DS_0010150C` reads 93 (92 before); its frame (loop 1097,
  `DS_000EF6DC`'s gate) does not move.
- **Named gap: the tick model's drift.** The per-read ceiling does not
  reproduce the original's tick after each read. At the first demo's state-6
  entry the port's re-syncs land at 2, 31, 32, 37, 56 and 58 (the original's
  3, 31, 32, 36, 54, 55), and the state-7 reads at 61, 63 and 65 (58, 59,
  61): up to 4 ticks late, where the pre-change model was 56 against 55 and
  60/62/64, up to 3. No frame depends on the absolute tick, because the
  loader's re-sync makes the gate pass either way; a closer model needs the
  reads' own timing, which the poll does not resolve below a tick.

### 45-A.5 The port

`sound_voice` (0x2C3FC) and the static `snd_*` functions (0x1CA14, 0x1CA40,
0x1CA6C, 0x1CC28, 0x1CD9C, 0x1CE04, 0x1CE70, 0x1D238, 0x1D244) are in
flow.c, next to the AIL handles. A slot's +0 is the port's `s_samples[i]`
(`PORT:`), and the status, end and init calls go to the port's AIL.
`game_audio_init` stores 1 in `DS_001028C8` as the stand-in for the host
handle (`PORT:`: every reader tests it for zero). `game_shutdown` clears it
(`0x1D0A9`). `fighter_spawn_slot` runs the tail. `anim_code_1543C` calls
`sound_voice(0x4D)`. `res_set_screen_hook` is the seam.

**Named gaps.**
- `0x1CC28`'s slot choice and `0x1CB18`'s start, which copies the sample into
  the slot's `0x1D0BC` buffer. The port allocates no buffers, so no port path
  writes a slot's +4/+0xC/+0x14; the original's snapshot values above are the
  evidence.
- `DS_001028C0`/`C4` stay 0. The port's music runs through `s_music_request`,
  so the dispatcher's music arms (`0x1CA14`'s store, `0x1CA6C`'s stop) are
  inert in runs.
- The other 302 `0x2C3FC` call sites are not wired. They are reached (the
  original's f 4443 slots hold other voices' handles, queued at `0x1957`),
  but in the captured window they read no bank that is not already loaded:
  every sound-bank read the original makes there is accounted for above. In
  the original, `0x1CE70` makes a repeat `0x4D` return 0 while the pair
  plays; the port returns 1 (no slot state), which `0x1546E` ignores.
- The original reads `s16title` (7) at boot; the port reads it at the title
  state (loop 1973 in cycle 2). The driver pins that screen as a known
  divergence. The port reads entries 0 and 8 at f 887 (state 2) where the
  original does at f 691 (state 1), predating this task.
- The tick model's drift (45-A.4).
- The front-end dump has no loader seam, so 832/833 stay allowed by name.
- 3408, the high-score screen.

### 45-A.6 Tests and mutations

- `check_sound_voice` (`test_game.c`, in `test_flow` on the live handles
  `game_audio_init` allocated) covers every case: id 0, case 0 and 6; case 1
  with and without a sequence handle and the pause byte; case 2's queue,
  playing refusal, the DIG and pause gates, `0x1CE70`'s clear-and-scan-on;
  case 3's three ids (each first-sample test) and an unlisted id (a retyped
  record); case 4's pauses, voice, song, `0x1CD9C` (the handles and one voice
  stopped) and queue, and its playing arm; case 5's id `0x100` with and
  without the DIG driver and with the title music playing (`0x1CA6C` stops
  it), the 15 keyed music stops against 15 neighbours, and the seven
  `0x1CE04` stops (stopped slot kept, first match only, no DIG). It
  snapshots and restores the data object, the INDEX table, both pools, the
  DAC and the aperture. After `game_shutdown`, a released handle (status 0) shows
  `0x1CE70` tests status 4 exactly. `DS_001028C8` is 1 after
  `game_audio_init` and 0 after `game_shutdown`.
- `test_res`: the seam runs once per first read, with the 166 text pixels on
  the aperture and before the stall; not on a second resolve or when unset.
  The rate pin is 132674.
- `check_spawn_sound` (`test_fight.c`, first in `test_fight` while the
  shipped INDEX is in place): characters 0..6 each read only their own bank
  and s16sound; no DIG driver, or paused samples, read neither; `0x1543C`
  reads 36 and 64 once (two screens), then nothing, and nothing without the
  driver.
- The driver: the banks after loops 1069/1070 (60, 5, 36), 2782/2783 (54)
  and 3556/3557 (64); the seven screens' loops and cycle-2 indices; frame 1757
  differs from 1756 only inside the text box; `fe_cyc2_n` 2102; the flier's
  tick 93.

**Mutations** (`scratchpad/t33/mut33.py`; `mut33u.log`, `mut33b.log`):
78 single-site edits (73 in unit mode, 5 in driver mode; two
placeholder rows of the script are skipped). Every source was restored and
checked against the intended diff. 77 fail an assertion. One is equivalent:
#12 drops `0x1CA6C`'s own `DS_001028C0 == 0` return, which `0x1CA40`
repeats. Three survived the first suite and are killed now:
- #16 turns `0x1CE70`'s `== 4` into `!= 2`. It is killed by the
  released-handle case (status 0).
- #22 and #26 drop the end call (`0x5DC8B`) in `0x1CE04` and `0x1CD9C`,
  which the re-init's status 2 hid. They are killed by the mixer's
  active-voice count.
The bound mutant `c <= 6` -> `c < 6` fails the character-6 case. In driver
mode each of these fails the driver: no voice (#72), no seam (#73),
`DS_001028C8` wiped after `game_audio_init` as the old init order did (#74),
no spawn bank read (#75), and the old rate (#76, the flier's tick 100).

### 45-A.7 Measured

| measurement | before (`54394e9`) | `b05adcc` |
|---|---|---|
| cycle-2 dump frames | 2095 | 2102 (+7 loader screens) |
| attract2 `[1885..3616]` clean/splice/trans/unexpl/black | 946/554/15/211/6 (2384 allowed) | 950/554/15/207/6 (none allowed) |
| attract2 first unexplained | 3257 (raw 7697) | **3408** (raw 7896) |
| front-end `[560..1884]`, demo-fight | 517/801/3/2; empty, N 1886 | unchanged (top-level dump byte-identical) |
| sound banks read (entries 5/36/54/60/64) | none | as the original, same frames (ticks drift, 45-A.4) |

`make verify` on `b05adcc` (EXIT 0, 0 compiler warnings):
- title 54/55/2/0 and 54/57/0/0, determinism 54;
- smk 120/120 and 41/41; C-vs-Python 9866;
- front-end `[560..1884]` 517/801/3/2 with 832/833 allowed;
- demo-fight empty, N 1886;
- attract2 950/554/15/207/6, first unexplained 3408 (raw 7896) = N;
- attract prefix 215/215;
- symbols.h idempotent.

### 45-A.8 Review round 1 (`task-33-review.md`)

- The rate used s16sound = 587966; INDEX entry 5, the file and `res_size(5)`
  say 586942. Corrected to 7297097 bytes, 132674 bytes/tick (`res.h`,
  `res.c`, `test_res`'s pin, 45-A.4). No tick in the run changes: each of the
  run's 21 reads has the same ceiling at both rates.
- The tick model's drift is now a named gap (45-A.4, `res.c`).
- The call sites are 303, so 302 are unwired; Ghidra's 300 are complete and
  the three it lacks are named (45-A.2). The 61 `PORT:` comments in
  `port/src` and four `game_flow.md` lines that called the voice "out of
  scope" now say "not wired (record §45-A)".
- The load claim is narrowed to the sound banks and the fight reads; the
  s16title and entries 0/8 differences are stated (45-A.3).
- `test_res`'s seam block now restores the data object (the tick pair,
  `DS_001014FC`, the row pointer, the palette records), entries 4 and 6's
  +0xC, the aperture and the DAC.
- Two test comments: `0x5D` tests its first sample, not its second; the
  driver's loop-1973 screen is a pinned known divergence.

## 46-C. Character 1's entrance `0x24568` (entry 1 of the per-character table `0xA8628`), its stream target `0x246D4` and `0x3BCE0` (named-gap batch 5, branch `gap5-24568`)

**Result in one line.** §43-C.5 named `0x24568` (the last caller of `0x2372C`)
and its three dispatch sites `0x25F27`/`0x27732`/`0x2989C` as unported.
`0x24568` is now ported, registered and unit-tested. So are the one code
target its entrance stream reaches, `0x246D4`, and that target's callee
`0x3BCE0`: 3 functions and 661 raw bytes (363 + 128 + 170), with no
unported callee except the voice. The three dispatch sites are not ported.
Each lies in an interactive mode's handler (modes 5, `0x0D` and `0x32`). The
port's `0x24C5C` never runs those modes, and the handlers need 13 unported
callees. They remain a named gap, with the evidence in 46-C.2. No port path
reaches any of the new code, so no oracle is expected to move (46-C.5).

### 46-C.1 The raw (Ghidra `read_memory` + capstone, fixups applied; Ghidra has no function at `0x24568` or `0x246D4`)

- **The table `0xA8628`.** It holds seven dwords, one per character 0..6:
  `0x40CB0`, `0x24568`, `0x49150`, `0x15A34`, `0x45FE8`, `0x40E64` and
  `0x24804`. `0xA8644` starts the next table, the update table
  (`DS_000A8644`, `run_process_table`). Ghidra has no function at any of the
  seven entries, and the port has none of the other six.
- **`0x24568`** (363 bytes, `0x24568..0x246D2`; it pushes EBX, ECX, EDX and
  ESI; EAX = the entering side, kept in ESI).
  - **The placement.** It calls `0x1A570(byte [0x10810D])` (`0x2456E..0x2457C`).
    - The left arm (`0x2457E`) reads x0 = `+0x18` of the fighter record of
      the side `DS_0010810D` names, recomputed from the byte each time. It
      takes x = x0 - `0x5000` when x >= `-DS_000BE018` (`neg edx`; `cmp`/`jl`,
      signed), with a5 = `0x4000` (`0x245AF`).
    - The right arm (`0x245B6`) takes x = x0 + `0x5000` when x <=
      `DS_000BE018` (`cmp`/`jg`, signed), with a5 = 0 (`0x245E5`).
    - A true `0x1A570` enters the left arm first. The left arm falls to the
      right on failure, and the right arm jumps back to the left on failure
      (`0x245E3 jg 0x2457E`) without calling `0x1A570` again. With the
      image's bound `0x7C00`, one arm always passes; a bound below `0x5000`
      could spin, in the raw and in the port alike.
  - **The spawn.** `0x33C78(EAX = side, EDX = x, ECX = word [0xBD898], EBX =
    0, push a5 & 0xFFFF)` (`0x245E7..0x245FD`). `0x33C78` hands the
    caller's EBX to `0x2AE14` untouched (no write to EBX between `0x33C78`
    and `0x33CD3`), where it becomes the record's y (`+0x1C`). `0x33C78` has
    nine callers (`get_xrefs_to` and the rel32 scan agree): `0x15AC9`,
    `0x245FD`, `0x24899`, `0x33EE0`, `0x357D6`, `0x40D48`, `0x40EF9`,
    `0x4607D` and `0x491E5`. Eight zero EBX. `0x40D48` does not: it is in
    `0x40CB0` (entry 0 of `0xA8628`), and both of its placement arms pass
    through `mov ebx, 0x4C00` at `0x40D2F`. The port's `fighter_spawn_slot`
    passes a literal 0, which is correct for its two port callers, `0x33EB4`
    (`0x33ED7`) and `0x24568` (`0x245EF`). A port of `0x40CB0` must pass EBX
    through. `fighter.c` carries a `PORT:` note there, and 46-C.5 names the
    gap. (Corrected in review round 1: the first version said `0x33C78` had
    two callers.)
  - **The record.** `0x2BC30(slot record, 0xE453A, 0x40800000)`
    (`0x24613..0x24623`), `0x39A10(record, 0x309)` (`0x24628..0x24633`), and
    `0x2372C(record)` (`0x24638/0x2463E`). The record is re-read from
    `[side*0x94 + 0x1077B0]` before each call.
  - **The blink.** The mask is `0x40` when `DS_0010810D` is set, else `0x80`
    (`0x24643..0x24653`). When `DS_00104B03 & mask` is set, the slot's
    `+0x41` gets bit 0 (`0x24658..0x24677`). Mode `0x0C`'s tail, which is
    ported, blinks a slot with that bit.
  - **The voice** `0x2C3FC(0xB4)` (`0x2467F/0x24684`). Its record at
    `0xBC638` is case 2 with handle `0x1B007C54`.
  - **The state.** The slot's `+0x52` = 9 (`0x2469B`), `+0x53` = 3
    (`0x246A2`), `+0x64` = `0x2A` (`0x246B2`), dword `+0x40` |= `0x81000`
    (read at `0x246AB`, stored at `0x246BF`), and `+0x63` = 1 (`0x246C6`).
    EAX is clobbered. The dispatchers do not read it (46-C.2).
- **The stream `0xE453A`.** It reads `12A2 C410 <0xE453A> D500 <0x246D4>`: a
  sprite, then opcode 4's counted loop back to the start (the dword at
  `0xE453E`), then opcode `0x15` on `0x246D4`. Opcode `0x15` calls its target
  and stops the walk (`0x2B5E3`). `0x246D4` re-points the record through
  `0x3BCE0`'s `0x2BC30`. So the words after it (`D100 <0x37DD4>` at `0xE4548`,
  the entry that `0xBDC30` and `0xE4560` name) belong to another stream.
- **`0x246D4`** (128 bytes, `0x246D4..0x24753`; it pushes EBX, EDX and EDI;
  EAX = rec; EDX is zeroed at `0x246D7` before any read).
  - side = byte `[rec+0x51]`.
  - `DS_001088E0[side]` = `0xA000` when `0x1A570(side)` holds, else `0x9000`
    (`0x246DE..0x246F9`).
  - `0x3BCE0(side)` (`0x24704`).
  - Then the slot's `+0x54` = 2 (`0x24717`, which repeats `0x3BCE0`'s store,
    so dropping it is an equivalent mutant) and word `+0x74` = 0
    (`0x24728`). Dword `+0x40` &= `0xFFF7EFFF` (`0x24721..0x24736`), which
    clears exactly the bits `0x24568` set.
  - `DS_000F0AFE` = 1 (`0x24744`) and `DS_000F0AFC` = `0x100` (`0x2474A`).
- **`0x3BCE0`** (170 bytes, `0x3BCE0..0x3BD89`; it pushes EBX, ECX and EDX;
  EAX = side, kept in ECX).
  - `DS_00107D40[side]` = `0xBEF64` + char * 6 (`0x3BCF5..0x3BD13`).
  - `0x2BC30(slot record, DS_000C8B30[char], 0x40000000)` (`0x3BD1A..0x3BD2B`).
    `0xC8B30` holds `0xE6F28`, `0xE3AF0`, `0xECCEC`, `0xD2274`, `0xEA720`,
    `0xD3F4C` and `0xE073C`. Characters 1 and 3 start with a sprite word
    (`0x134F`, `0x1746`).
  - The slot's `+0x52` = 3, `+0x54` = 2, `+0x53` = 4 (`0x3BD30..0x3BD3A`),
    and `DS_001078F8[side]` = 1 (`0x3BD3E`).
  - The word `+0x4E` depends on the high byte of `DS_001088E0[side]`: 1 when
    it has bit 4 (`0x3BD4F..0x3BD59`), else `0xFFFF` when it has bit 5
    (`0x3BD6C..0x3BD76`), else 0 (`0x3BD80`). `0x246D4`'s `0xA000` gives
    `0xFFFF` and its `0x9000` gives 1.

### 46-C.2 Entrances (`get_xrefs_to`, a rel32 CALL/JMP/Jcc scan of the code object and a dword scan of both fixed-up objects)

| address | references |
|---|---|
| `0x24568` | data dword `0xA862C` only (Ghidra: the same, DATA) |
| `0x246D4` | data dword `0xE4544` only (Ghidra: none) |
| `0x3BCE0` | rel32 `call` at `0x24704` only |
| `0xE453A` | code dword `0x24614` (the `mov edx` in `0x24568`) and data dword `0xE453E` (its own loop) |
| `0xE4542` (the `D500` word) | none: it is reached only by falling through from `0xE453A` |
| `0xA8628` | code dwords `0x25F2A`, `0x27735` and `0x2989F`: the three `call [reg*4 + 0xA8628]` |
| `0xA862C` | none |
| `0x25C88` | rel32 `call` at `0x2524C` only (the `0x24C5C` jump table `0x24B8C`, case 5) |
| `0x274FC` | rel32 `call` at `0x25367` only (case `0x0D`). The scan's dword at `0x14C48` is the rel32 operand of `call 0x3C148` at `0x14C47`, not a reference |
| `0x296B8` | rel32 `call` at `0x251B2` only (case `0x32`) |

The three dispatch sites:
- **`0x25F27`**, in `0x25C88` (804 bytes, mode 5's handler), runs in its
  `DS_00104B25` - 1 == 2 arm when `DS_00104B14` is set. It sets
  `DS_00104B00` = `0x0C` (`0x25F20`), then EAX = `DS_00104B12`, EDX = char =
  byte `[0x10816A + side]` (`mov edx,[eax+0x108167]; sar edx,0x18`), then
  `0x1D838(side, char)`.
- **`0x27732`**, in `0x274FC` (708 bytes, mode `0x0D`), runs when
  `DS_00104B0C` is set and the incremented `DS_00104B21` is not 7. It first
  clears the record's `+0x48`, frees the slot's `+4` actor (`0x2B150`),
  decrements `DS_001078FA`, and calls `0x2716C`, `0x46534(side, 1)` and
  `0x33C18(side)`. Then comes the dispatch with the same EAX/EDX. Afterwards
  `DS_000F0AFF` = `DS_0010810D` (AL is reloaded at `0x27739`), then `0x1D764`
  and `0x1D838`.
- **`0x2989C`**, in `0x296B8` (696 bytes, mode `0x32`), runs when
  `DS_00104B0C` is set, `DS_00104B09` is not `0xFF`, and neither side's
  `DS_00104AF0` count has reached 4. It first calls `0x292D4`, `0x46534` and
  `0x33C18`. Then EAX = `DS_00104B09`, EBX = 0, ECX = char = byte `[0x108134
  + side*4 + DS_00104AF0[side]]` (the side's next roster entry, after the
  increment at `0x2978D`). After the call it stores the slot's `+0x63` = 0
  and `+0x7A` = CL, so it relies on the entry preserving ECX, which
  `0x24568` does.

**Why the dispatchers stay a gap.**
- The port's `game_frame` dispatches only case 3 of `0x24C5C`.
- Beyond the handlers' own 2208 bytes, they call 13 unported functions, 2384
  bytes before their own callees:
  - `0x25C88`: `0x25C1C` (91), `0x256F4` (173), `0x4F37C` (182), `0x32970`
    (203), `0x32B00` (73), `0x32B4C` (70) and `0x1D838` (87);
  - `0x274FC`: `0x28130` (404), `0x2C2B0` (82), `0x4DBEC` (755, the
    mode-`0x0D`/`0x32` spawner of §42-D.2), `0x2716C` (115), `0x1D764` (82)
    and `0x1D838`;
  - `0x296B8`: `0x28130`, `0x4DBEC`, `0x292D4` (67), `0x1D764` and `0x1D838`.
- The handlers' ported callees are `0x2AE14`, `0x2C3FC`, `0x2F4BC`,
  `0x2B150`, `0x3C5CC`, `0x16D58`, `0x35658`, `0x19068`, `0x12DA8`,
  `0x1C500`, `0x2F198`, `0x41310`, `0x46534`, `0x33C18` and `0x3CB68`
  (`fight_slot_pass`). The first version counted `0x3CB68` as unported
  (14 functions, 2475 bytes); corrected in review round 1.

That is the interactive match flow (continue/challenger, team and endurance
modes), a sub-project of its own. Ported, the dispatch is one line:
`fn_resolve(DSD(DS_000A8628 + char * 4))` called with the side.

### 46-C.3 The port

- `fighter.c` has `fighter_24568`, `fighter_246d4` and `fighter_3bce0`, right
  after `fighter_2372c`, with local `#define`s for `0xE453A` and `0x104B03`
  (`symbols.h` names neither). The stance table `0xC8B30` reuses the file's
  existing `FIGHT_ANIM_3BDDC`, which `0x3BDDC` also reads. `fighter.h`
  declares them after `fighter_2372c`.
  - `fighter_spawn_slot` (`0x33C78`) gains a `PORT:` note at its
    `actor_spawn`: the EBX pass-through and `0x40D48`'s `0x4C00` (46-C.1).
  - `fighter_24568` calls the static spawn core `fighter_spawn_slot`
    (`0x33C78`) directly.
  - The voice `0x2C3FC(0xB4)` is a `PORT:` note, "not wired (record §45-A)",
    like the other 302 sites. `sound_voice` would queue the sample
    `0x1B007C54`, so wiring it is the §45-A gap's decision, not this one's.
- `actors.c` registers `0x24568` directly (`fn(side)`, the table's shape:
  EAX = side, nothing else read). It registers `0x246D4` through
  `anim_code_246D4`, the `(rec, arg)` wrapper, which drops the operand the
  raw zeroes.

### 46-C.4 The assertions and mutations

`check_char1_entry` (`test_fight.c`) runs right after `check_spawn_sound`, on
the same live INDEX and pools. It snapshots and restores the data object,
the INDEX table, both actor pools, a scratch decoy record, the DAC and the
aperture. Every scenario starts from the snapshot with `actors_reset`, both
sides set to character 1 and the audio gate closed (`DS_001028C8` = 0).

- **The data.** `0xA862C` = `0x24568`; `0xE453A`'s first word is `0x12A2`;
  the `D500` word and its dword `0x246D4`; `0xC8B30[1]`/`[3]`. The table
  dword resolves to `fighter_24568`, and `0x246D4` resolves to a wrapper,
  not to `fighter_246d4`.
- **A: `0x24568` through the table dword**, in eleven rows:
  - each arm: left, left failing to right, and the bound read (`0x8000`);
  - right, and right failing to left;
  - both equalities (x0 - `0x5000` == `-bound`, x0 + `0x5000` == bound);
  - both signed compares (a positive left x, a negative right x);
  - two rows where the entering side is `DS_0010810D`'s own side (its old
    record is the one placed against). These separate the mask's
    `DS_0010810D` from the entering side.

  The rows also vary `DS_0010810D` between 0 and 1 and the mask byte. When
  the two sides differ, the entering side's old slot record is a decoy
  whose pset bit 15 is the opposite of `DS_0010810D`'s side, so `0x1A570` on
  the wrong side fails. Each row checks:
  - the spawned record: its slot, `DS_001077A8`, character, `+0x51`, x, the
    word `+0x32` = the layer `word [0xBD898]` (seeded `0x0500` on the
    "right" row, so a literal `0x400` fails too), the a5 bit, `+8` =
    `0xE453A` and `+0x20`/`+0x24` = 4.0;
  - the slot: `+0x74` = `0x309`, `+0x41` bit 0, `+0x52`/`+0x53`/`+0x64`, the
    exact `+0x40` (`0x80081000`, or `0x80081100` with the blink; the spawn
    leaves `0x80000000`) and `+0x63` against a sentinel;
  - the projectile: type 8, `+0x14`, speed `0x113`/`0xFEED`, and x ±
    `0xC00`;
  - the other slot's pointer and `+0x52`, kept (when the sides differ).
- **B: `0x3BCE0` directly**, on side 1 with characters 1 and 3, over six
  `DS_001088E0[1]` values (bit 4, bits 4 and 5, bit 5, 0, low byte only,
  other high bits). It checks `DS_00107D40[1]`, the stream and 2.0, the state
  3/4/2, `DS_001078F8[1]` and `+0x4E`. Side 0's sentinels are kept.
- **C: `0x246D4`** on side 0 through its registration (bit 15 clear, then
  set) and through the real `D500` word at `0xE4542` via `0x2BC30`'s
  pre-walk. It checks `DS_001088E0[0]` (`0xA000`/`0x9000`) and the `+0x4E` it
  yields, `DS_00107D40[0]`, the state, `+0x74` = 0, `+0x40` = `0xFFF7EFFF`
  from `0xFFFFFFFF`, `DS_000F0AFE` and `DS_000F0AFC` against sentinels, and
  side 1's word kept.

`check_char1_entry` has 56 assertion sites.

**Mutations** (`scratchpad/g5-24568/mut.py`; `mut3.out`, plus `mut4.out` for
review round 1): 56 single-site edits of the new code, its registrations and
the wrapper. They cover every store, constant, side index, gate and bit test
of the three functions, both arms' offsets, compares and signedness, the
bound read, the mask's source, the spawn's x and layer, the stream and rate,
the `0x39A10` value, the `0x2372C` call, both registrations and the
wrapper's call.
- 53 fail an assertion (1..31 each), with no fault. That includes the two
  added in review round 1, which fail 11 and 1: the spawn's layer as a
  literal 0, and as a literal `0x400`. The first version's sweep missed the
  layer argument; review round 1 found that it survived.
- 2 hang, and the script kills them at 90 s: the unsigned forms of the two
  compares (#30, #35). On the signed rows neither arm then passes, so the
  loop spins, which is the raw's own behaviour for an unplaceable x. They
  count as failing.
- 1 is equivalent: #11, `0x3BCE0`'s bit-5 test widened to `0x30`. Bit 4
  has already returned by then.
- Also equivalent (not in the sweep): dropping `0x246D4`'s `+0x54` = 2,
  which repeats `0x3BCE0`'s store.

Two survivors of the first run were killed by strengthening the test:
- `0x1A570` on the entering side instead of `DS_0010810D`'s side, killed by
  the decoy record;
- the mask taken from the entering side, killed by the two rows that enter
  on sel's own side.

A third, the stream `0xE453A` + 2, converges back to `0xE453A` through the
`C410` loop. The sweep replaced it with a different stream (`0xE3AF0`),
which fails. The script restored every source, and `git diff` shows only
the intended changes.

### 46-C.5 Measured, and the remaining gaps

- `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all checks passed, 0 compiler
  warnings. The drivers and `make verify` were not run (batch rules).
- **Reachability.** Nothing in the port reads `0xA8628` or calls
  `fighter_24568`, except the test. `0x246D4` is reached only through the
  `D500` word at `0xE4542`, which only `0xE453A` falls into, and only
  `0x24568` starts `0xE453A`. `0x3BCE0`'s only caller is `0x246D4`. So no
  port run executes the new code, and no oracle is expected to move at the
  current pins. The controller's ladder is the check.
- **Base fix.** The base `1151646` did not compile: the frame-3257 merge
  dropped `check_volleyball`'s closing brace in `test_fight.c`. `0e49057`
  restores it. §46-B's branch carries the same one-line fix, and main has
  it since `df59113` (the same blob).
- **Named gaps.**
  - The dispatchers `0x25F27`/`0x27732`/`0x2989C` and their handlers
    `0x25C88`/`0x274FC`/`0x296B8` (modes 5, `0x0D`, `0x32`), with the 13
    unported callees of 46-C.2. This is the interactive match flow.
  - `0x33C78`'s EBX. The raw hands the caller's EBX to `0x2AE14` as the
    record's y. `fighter_spawn_slot` passes 0, which is right for its port
    callers. `0x40D48` (in `0x40CB0`) passes `0x4C00`, so a port of `0x40CB0`
    must thread EBX through. The `PORT:` note at `fighter_spawn_slot`'s
    `actor_spawn` records this.
  - The other six entries of `0xA8628`: `0x40CB0`, `0x49150`, `0x15A34`,
    `0x45FE8`, `0x40E64` and `0x24804`. They are the other characters'
    entrances, reached only through the same dispatchers.
  - The voice `0x2C3FC(0xB4)` at `0x24684` (§45-A's unwired sites).
  - What follows `0x246D4` in the stance stream `DS_000C8B30[char]` is the
    existing per-character machinery and was not re-audited here.
## 46-B. The five mode-`0x1A` hooks `0x430E8`, `0x4367C`, `0x25BBC`, `0x26998` and `0x270BC` (named-gap batch 5, branch `gap5-hookcallers`)

**Result in one line.** The five hooks that `0x4F980`'s callers install
before arming mode `0x1A` (§43-B.3) are ported, with the hook `0x430C0` that
`0x430E8` installs for the mode-`0x1B` call and every callee they need:
- in `flow.c`: `game_fight_reset` (`0x20DF4`, now ported whole), its callees
  `0x2C390`/`0x2C074`, `flow_screen_reset` (`0x4F200`), `flow_stage_pick`
  (`0x25848`), `0x46504`, and the hooks `game_hook_25bbc`/`game_hook_26998`/
  `game_hook_270bc`;
- in `fight.c`: `fight_hook_430e8`, `fight_hook_430c0`, `fight_hook_4367c` and
  `0x4454C`.
All six hooks are registered. State 6 now calls `0x20DF4` whole instead of
its hand-picked subset. That is the one change a ported path reaches. A
headless `--check 8000` run, which enters state 6 four times, gives
byte-identical `.idx`/`.pal` frames before and after the change (§46-B.5).
Modes `0x1A`/`0x1B` are still not dispatched, because no ported path stores
mode `0x1A`.

**The base did not build.** `1151646`'s `test_fight.c` had lost the `}`
after `mz_restore();` that closes the function before "the spawn's sound
banks" (§45-A), in the frame-3257 merge. `dc8d682` restores the brace. No
assertion changed.

### 46-B.1 The raw (`read_memory` + capstone; Ghidra has no function at any of the six hooks; fixups applied)

- **`0x430E8`** (to `0x4329E`) pushes EBX/ECX/EDX/ESI/EDI. In order:
  - the voice `0x2C3FC(0x31)`, `0x4F200` with EAX = 0 (`0x430F7 xor
    eax,eax`), then `0x25848`;
  - with byte `[0x108173]` != 0, bytes `[0x107813]` and `[0x1078A7]` = 1 (DL).
    These are the two slots' `+0x63` think gates, which `0x41350` also sets;
  - with byte `[0x104B1D]` == 1 (BL), `[0x104B1F]` = `[0x104AB8]`
    (`0x43125/0x4312A`). **`0x25848` runs before this copy**, so it reads the
    old `[0x104B1F]`;
  - unless `[0x104B1F]` == 3: `0x41350` with EAX = `[0x104B1F]` - 1 with AL
    xored by 1 (`0x4313D dec eax; 0x43145 xor al,bl`, BL = 1, or `0x4315F xor
    al,1`), which is `(b1f - 1) ^ 1`, and EDX = the zero-extended word
    `[0x104AFC]`;
  - six `0x2AE14` spawns (a2 = EDX, a3 = ECX, a4 = EBX, a5 = the pushed
    dword):

    | # | descriptor | a2 | a3 | a4 | a5 | kept |
    |---|---|---|---|---|---|---|
    | 1 | `0xC8364` (id `0x351`) | 0 | `0xF0` | 0 | 0 | `[0x1080B4]` |
    | 2 | `0xC8378` (id `0x352`) | `0x2A00` | `0xF1` | 0 | 0 | `[0x1080B8]` |
    | 3 | `0xC84FC[c0]` | `0xF` | `0xF4` | 7 | pa \| `0x400` | |
    | 4 | `0xC84FC[c1]` | `0xF` | `0xF4` | 7 | pb \| `0x400` | |
    | 5 | `0xC84E0[c0]` | `0x9B` | `0xE0` | `0x36` | pa \| `0x4400` | |
    | 6 | `0xC84E0[c1]` | `0xD` | `0xE0` | `0x36` | pb \| `0x400` | ESI |

    pa is the word `+0x56` of `[0x1080B4]` (reloaded at `0x43198`) and pb the
    word `+0x56` of spawn 2. c0/c1 are the signed bytes `[0x10816A]`/
    `[0x10816B]` (`mov eax,[0x108167]` / `[0x108168]`; `sar eax,0x18`).
    `0xC84FC` = `0xC83C8..0xC8440` and `0xC84E0` = `0xC8454..0xC84CC`, stride
    `0x14`;
  - `0x4F1D0` (EAX/EBX/ECX/EDX zeroed at `0x43254..0x4325A`; `0x4F1D0` pushes
    EDX and names no other), then `0x38B18(0xC87BC)` with EDX = EBX = 0;
  - spawn 6's word `+0x2E` += 4 (`0x4326B..0x43277`), byte `+0x4E` = 1
    (`0x43280`), `[0x104AE4]` = `0x430C0` (`0x43284`), then the voices `0x2D`
    and `0x2F`.
- **`0x430C0`** pushes EBX/ECX/EDX, calls `0x29D60` (a bare `ret`), then
  `0x2F198` with EAX = `0x13`, EDX = 2, EBX = `0x80C04` (the string "VS") and
  ECX = `0x4002`.
- **`0x4367C`** pushes EBX/ECX/EDX:
  - with byte `[0x104B1D]` == 3 it calls `0x4454C` and returns;
  - otherwise the voice `0x100`. Then, with `[0x108173]` != 0, for EAX = 1, 2
    (`xor eax,eax` before a loop that starts with `inc eax` and ends at `cmp
    eax,2; jl`): `[0x10816D+EAX]` = `0xFF`, `[0x108165+EAX]` =
    `[0x108167+EAX]`, `[0x105B33+EAX]` = 0. Then byte `[0x108169]` += 1; at 7
    it becomes 0 and `[0x108168]` += 1, which becomes 0 at 7;
  - with `[0x108173]` == 0, the same loop with `[0x108165+EAX]` =
    `[0xC887F+EAX]` (the bytes `0xC8880`/`0xC8881` = 0, 5) and no step;
  - then the voice `0x2E` and `call 0x43738` (`0x4372E`).
- **`0x4454C`** (Ghidra function) pushes EBX..EBP. The voice `0x100`, then:
  - with `[0x108173]` != 0, `0x4367C`'s first arm (the same stores and step);
  - otherwise, per side s (ESI), `[0x10816E+s]` = `0xFF`, `[0x108166+s]` =
    `[0xC8880+s]`, `[0x105B34+s]` = 0, the four bytes `[0x108134+4s..]` =
    `0xFF`, the four dwords `[0x108114+16s..]` = 0 (`0x445F5..0x4460B`: EAX =
    s << 4, pre-incremented by 4 before the `[EAX+0x108110]` store, until it
    equals EBP = 16(s+1)), and `[0x108164+s]` = 0 (ESI incremented before the
    `0x44616` store at `ESI+0x108163`);
  - then the voice `0x2E` and `0x444C8`.
- **`0x25BBC`** pushes EDX:
  - bytes `[0x1078FA]` = `[0x104B13]` = 0 (AH), and `[0x104B1E]` += 1 (a byte);
  - `0x20DF4` with EAX = the word `[0x104AFC]` and EDX = 1;
  - `0x33EB4(0)`, `0x33EB4(1)`;
  - `0x4F714` with EAX = the word `[0x104AFC]` and EDX = `0x5D812`, then
    `0x46504`;
  - `[0x104AE4]` = EDX (`0x25C13`), which is `0x5D812`, because `0x4F714`
    tail-jumps into `0x2C3FC` (it pushes EBX/EDX/EDI) and `0x46504` moves only
    EAX.
- **`0x26998`** pushes EBX/ECX/EDX:
  - the same round/`0x1078FA` stores, but no `0x104B13` store;
  - `0x20DF4` on the stage word with EDX = 1;
  - byte `[0x104B1A]` = 0 and EAX = 0 when `[0x1078A7]` != 0, else 1 and 1,
    then `0x33EB4`;
  - with EDX = `[0x104B1A]`, the byte at `0x10780B + EDX * 0x94` (`lea
    eax,[edx*8]; add eax,edx; shl eax,2; add eax,edx; shl eax,2`, i.e. 148 =
    `0x94`) += 2. It is capped at `0x78` by `cmp edx,0x78; jle` on the
    zero-extended new byte, so a wrap from `0xFF` to 1 is not capped;
  - the voice `0x28` with EDX = `0x5D812`, then `0x46504`, then `[0x104AE4]` =
    EDX.
- **`0x270BC`** pushes EDX:
  - the round/`0x1078FA` stores;
  - `0x20DF4` on the stage word with EDX = 1;
  - `0x33EB4` with EAX = the signed byte `[0x10810D]` (`mov eax,[0x10810a];
    sar eax,0x18`);
  - the voice `0x25`, after `xor dh,dh`. `0x2C3FC` preserves EDX, so the store
    below is 0;
  - byte `[0x104B0A]` = DH = 0, byte `[0x104B0B]` = the byte at `0x10780B +
    that side * 0x94`, and `[0x104AE4]` = `0x5D812`. There is no `0x46504`.
- **`0x20DF4`** (Ghidra function) pushes EBX/ECX/ESI. EBX = EAX, clamped by
  the signed `cmp eax,7; jl` to 7. In order:
  - `[0xF0A48]` = 0 and `0x29B70` (a bare `ret`);
  - dwords `[0x100B4C]`, `[0x104AE8]` = 0, bytes `[0x1088EC]`, `[0x104B15]` =
    0;
  - `0x2C390`, `0x12750`, `0x49300`, `0x28E98`, `0x34978`, `0x2C074`;
  - dwords `[0xF0AEC]`, `[0xF0AF0]` = 0, words `[0xF0AFA]`, `[0xF0AF8]` = 0;
  - `0x12C70`;
  - then, if EDX != 0, `0x2BAF4(1)`, `0x38730(EBX)` and `0x412A0(EBX)`.

  EDX survives to `0x20E6F`: `0x2C390`, `0x12750`, `0x49300`, `0x28E98`,
  `0x34978` and `0x2C074` push and pop it, and `0x12C70` does not name it.
- **`0x2C390`** sets `[0x105C10]` = `[0x105C0C]` = `0x105C0C` and
  `[0x105C18]` = `[0x105C14]` = `0x105C14`. Then it `0x249C0`-inserts the
  sixteen `0x14`-byte nodes `0x105C1C..0x105D48` before `0x105C14`, while EBX
  < `0x105D5C`.
- **`0x2C074`** sets `[0x105BF4]` = `[0x105BF0]` = 0.
- **`0x4F200`** runs `and eax,0xff; mov edx,eax; xor dh,ah`, then `[0x107A55]`
  = AL and `[0x107A54]` = DH (= 0), then `0x4F1D0` and `0x2BAF4(1)`.
- **`0x46504`** sets `[0x1082C0]` = `[0x1082C8]` and `[0x1082C4]` =
  `[0x1082CC]`.
- **`0x4F714`** is `mov ax,[eax*2+0xc9888]; and eax,0xffff; jmp 0x2C3FC`. The
  words at `0xC9888` are `0x20, 0x21, 0x1B, 0x1C, 0x1E, 0x1D, 0x1F, 0x1F`.
- **`0x25848`** (Ghidra function) sets up a 4-byte frame (`sub esp,4`) and
  switches on byte `[0x104B17]`:
  - **0:** with `[0x104B1F]` == 3, the word `[0x104AFC]` = rng(7). Otherwise
    c = the signed byte at `0x108169 + [0x104B1F]` (`mov
    eax,[eax+0x108166]; sar eax,0x18`), and the word = `0xA87C4[c]` + rng(6)
    + 1, less 7 once if >= 7. `0xA87C4` = 0, 4, 2, 3, 6, 1, 5. EDX's upper
    half is the caller's (`xor dh,dh; mov dl,..`) but only AX is stored;
  - **2:** return;
  - **1 or above 2:** `[esp]` = `[0x10782A]` and `[esp+1]` = `[0x1078BE]` |
    `0x40`. With n = the zero bytes among `[0x108106..0x10810C]`, n != 0
    stores the index of the rng(n)-th zero byte. Otherwise, or if that scan
    runs out, it stores the first index whose byte & `0x7F` is neither frame
    byte. Failing that, with `[0x104AD4]` == 2 the word goes +1 (a 16-bit
    `inc`) and wraps to 0 at 7. Otherwise it stores the first index whose
    byte & `0x7F` equals `[esp + ([0x104AD4] ^ 1)]` (`xor cl,1`).
- **The storers and their return modes** (`EAX` into `0x4F980`):

  | hook | stored at | return mode |
  |---|---|---|
  | `0x430E8` | `0x1F447`, `0x43AE8`, `0x43C18`, `0x44824` | `0x11` |
  | `0x4367C` | `0x25810` (after `0x65490(0x104B02)`) | `0x10` |
  | `0x25BBC` | `0x25A46` (in `0x259CC`, the arm with byte CH == 3) | `0x30` |
  | `0x25BBC` | `0x25A73` (in `0x259CC`, the other arm) | 5 |
  | `0x25BBC` | `0x285CB` (in `0x28468`) | none: that path stores mode `0x16` and the word `[0x104AFA]` = `0x30` itself (`0x285DC`/`0x285E3`) |
  | `0x25BBC` | `0x285F3` (in `0x28468`) | none: mode `0x16` and `[0x104AFA]` = 5 (`0x285FF`, stored at `0x28609`/`0x2860F`) |
  | `0x26998` | `0x2698B` (in `0x26978`) | `0x23` |
  | `0x270BC` | `0x2715C` (in `0x27134`, which zeroes `[0x104B1E]`, `[0x104AF3]`, `[0x104AF2]` first) | 5 |

  `0x259CC`, `0x26978` and `0x27134` are themselves `DS_00104AE4` values
  (§43-B.3's list). On the two `0x28468` paths the hook is called by mode
  `0x16`'s handler `0x4F2B0` (`call [0x104AE4]` at `0x4F302`, once the
  countdown `[0x104AFE]` runs out), not by `0x4F9A0`/`0x4F9C8`. **Correction to §43-B.3:** that table omits `0x25BBC`'s two storers
  in `0x28468`. They are found by the dword scan (`0x285C0`/`0x285ED`) and
  by Ghidra's `get_xrefs_to`.

### 46-B.2 Entrances (`get_xrefs_to`, a rel32 CALL/JMP/Jcc scan of the code object, a dword scan of both fixed-up objects)

| target | rel32 | dwords |
|---|---|---|
| `0x430E8` | none | code `0x1F43E`, `0x43ADF`, `0x43C0F`, `0x44816`; no data |
| `0x4367C` | none | code `0x25807`; no data |
| `0x25BBC` | none | code `0x25A35`, `0x25A65`, `0x285C0`, `0x285ED`; no data |
| `0x26998` | none | code `0x2697C`; no data |
| `0x270BC` | none | code `0x2713B`; no data |
| `0x430C0` | none | code `0x4327C` (in `0x430E8`); no data |
| `0x20DF4` | `0x11AC4`, `0x25A95`, `0x25BE6`, `0x269BE`, `0x270E0`, `0x295E4` | none |
| `0x4F200` | `0x430F9` only | none |
| `0x25848` | `0x430FE`, `0x4177D`, `0x418A8`, `0x42390` | none |
| `0x46504` | `0x25C0E`, `0x26A31` | none |
| `0x4454C` | `0x43688` only | none |
| `0x4F714` | `0x25C09` only | none |
| `0x2C390` | `0x20E2E`, `0x20ED5` | none |
| `0x2C074` | `0x20E47`, `0x20EEE` | none |

Ghidra's `get_xrefs_to` agrees on every row: DATA references at the
immediates' instruction starts (`0x1F43D`, `0x43ADE`, `0x43C0E`, `0x44815`,
`0x25806`, `0x25A34`, `0x25A64`, `0x285BF`, `0x285EC`, `0x2697B`,
`0x2713A`, `0x4327B`) and the CALLs above. So:
- the hooks are reached only through `DS_00104AE4`;
- `0x4F200`, `0x4454C` and `0x4F714` have a single caller each, inside the
  hooks;
- `0x20DF4`'s only ported caller is state 6. Every caller, `0x295E4`
  included (`0x295D7 xor eax,eax; 0x295DE mov ax,[0x104afc]`), passes the
  zero-extended word `[0x104AFC]` with EDX = 1.

### 46-B.3 The port

- `game_fight_reset(stage, full)` is `0x20DF4` in raw order. The clamp is
  `(s32)stage < 7 ? stage : 7`, and the byte store `[0x1088EC]` stays a byte
  (`fighter.c`'s `0x39278` writes that address as a word).
- **State 6** (`game_state_6`, `0x11AC4`) now calls `game_fight_reset(draw1,
  1)`. It replaces the inline subset and its `PORT:` gap list (`0x29B70`,
  `0x2C390`, `0x2C074`, five zero stores, two word stores). What state 6 now
  does in addition:
  - zeroes `[0xF0A48]`, `[0x100B4C]` and `[0x104AE8]`. `0x2BAF4` re-zeroes the
    last two later on the same path;
  - zeroes byte `[0x1088EC]`, which is 3 at the second, third and fourth
    demos' state 6 in a headless run;
  - zeroes `[0x104B15]`, which `0x11B14` then sets to 1;
  - rebuilds the `0x105C0C`/`0x105C14` lists and clears `0x105BF0`/`F4`
    (neither has a ported reader);
  - zeroes the words `0xF0AF8`/`0xF0AFA`.
- The voices (`0x31`, `0x2D`, `0x2F`, `0x100`, `0x2E`, `0x28`, `0x25`) and
  `0x4F714`'s stage voice are `PORT:` notes, "not wired" (§45-A's rule for
  the other 302 sites).
- `0x25848`'s last arm reads `[esp + ([0x104AD4] ^ 1)]`. The writers of
  `[0x104AD4]` (`0x25A0E`, `0x27C3D`, `0x283AE`/`0x283EE`/`0x28409`,
  `0x288F3` = 2, `0x28961` = 0, `0x28973`) store only -1, 0, 1 or 2; -1
  comes from `0x25A0E` (EBX = -1 from `0x259D2`) and `0x27C3D`. 2 takes the
  step arm, so the index is 1, 0 or -2. For -1 the raw reads `[esp-2]`, a
  leftover stack byte below the frame, and stores only if that byte equals
  one of the two frame bytes. The port returns without a store, the raw's
  no-match exit (a `PORT:` note: the stack byte is not modelled).
- New local names with no `symbols.h` entry: `DS_000A87C4`, `DS_0010810D`,
  `DS_00104B1A` (`flow.c`); `DS_000C8364`/`C8378`/`C84E0`/`C84FC`/`C87BC`,
  `DS_00080C04`, the loop bases `DS_0010816D`/`DS_00108165`/`DS_00105B33`,
  and `FN_000430C0` (`fight.c`).
- `actors_init` registers `0x430E8`, `0x4367C`, `0x25BBC`, `0x26998`,
  `0x270BC` and `0x430C0`. The `PORT:` notes of `0x4F9A0`/`0x4F9C8` now list
  the 7 values still unregistered: `0x259CC`, `0x10E80`, `0x24B54`,
  `0x27134`, `0x4142C`, `0x25AE8` and `0x26978`.

### 46-B.4 The assertions and mutations

`check_mode_1a_hooks` (`test_fight.c`, run after `check_char_screen_modes`)
takes the same snapshot (the data object, both pools, both buffers, the
resource table, the DAC and the aperture) and restores it between runs and at
the end. Fighter runs seed `DS_001028C8` = 0 (no sound bank is read) and
`DS_00104B14` = 1 (the value `0x25A51` stores after arming), so no dust is
built.
- **(a)** `fn_resolve` of the six hooks returns their ports.
- **(b)** `0x20DF4(9, 0)`, from sentinels:
  - every pre-branch store is checked, the byte `[0x1088EC]` with its
    neighbour intact;
  - the `0x105C0C` sentinel and the `0x105C14` list (16 nodes in order,
    closed, with the sentinel's prev at `0x105D48`);
  - the `0x2C074` pair with its neighbours, `0xF0AEC`/`F0` and the words
    `0xF0AF8`/`FA`;
  - the `0x12C70` seed and one link from each of `0x12750`, `0x49300`,
    `0x28E98` and `0x34978`;
  - no `0x2BAF4` (two seeded records survive) and no `0x38730`
    (`DS_00107A40` keeps its sentinel).
- **(c)** `0x20DF4(9, 1)`:
  - `0x38730` runs on the clamped 7: `DS_00107A40` = `0xBDDFC[7]` = `0x50`,
    not `[9]` = `0x700`;
  - the seeded record is gone and the scene spawned;
  - `(6, 1)` gives `0x40`.
- **(d)** `0x25BBC`:
  - the round byte wraps `0xFF` to 0, `0x104B13` = 0;
  - two spawns counted, both slot pointers and characters;
  - scene 2 (`0x74`), `[0xF0A48]` = 0, the `0x46504` pair, the hook `0x5D812`.
- **(e)** `0x26998`:
  - side 1 when `DS_001078A7` == 0: its `+0x5B` goes `0x77` to `0x78`
    (capped), slot 0's is untouched, `0x104B13` is not stored, one spawn;
  - side 0 otherwise: `0x10` to `0x12`;
  - `0xFF` to 1: the wrap is not capped.
- **(f)** `0x270BC`: side = the signed byte `DS_0010810D` = 1. `0x104B0A` = 0,
  `0x104B0B` = that slot's `+0x5B`, and the `0x46504` pair keeps its
  sentinels.
- **(g)** `0x4367C` with `DS_00104B1D` = 3 runs `0x4454C`'s second arm:
  - the stores per side, the eight `0xFF` bytes and eight zero dwords;
  - the neighbours `0x108110`, `0x10816D` and `0x105B33` are untouched, and
    there is no step;
  - `0x444C8`, not `0x43738`: `DS_0010816C` keeps its sentinel, and both
    `DS_00108170` bytes are 1.
- **(h)** `0x4454C`'s first arm: the copies from `0x108168`/`9`, and the step
  6 to 0 carries 2 to 3.
- **(i)** `0x4367C`'s own arms end in `0x43738` (countdown 5 / `0xF`):
  - the copies from `0x108168`/`9` and 5 to 6 with no carry;
  - the double wrap 6/6 to 0/0;
  - the `0xC887F` arm (0, 5) with no step.
- **(j)** `0x430E8`, with `DS_00104B1D` = 1, `DS_00104B1F` = 2 and
  `DS_00104AB8` = 1:
  - `0x25848` runs on the old `DS_00104B1F` (the model's rng(6) +
    `0xA87C4[3]` + 1), and then the copy gives 1;
  - side 0's think gate `0x107813` = 1;
  - `0x4F200`'s `0x2BAF4` drops a seeded record, and `0x107A55`/`54` are 0;
  - the six spawns: ids `0x351`/`0x352`, layers `0xF0`/`0xF1`, x `0x2A00`,
    the children's a2/a3/a4 in `+0x34`/`+0x49`/`+0x36`, and spawns 5 and 6's
    parents pa/pb (`+0x4A`);
  - spawn 6's `+0x2E` = spawn 5's + 4 (same descriptor, characters 3/3) and
    only spawn 6's `+0x4E` = 1;
  - the `0x38B18` row (id `0x3F11`) and the hook `0x430C0`.
- **(k)** `DS_00104B1D` = 0, `DS_00104B1F` = 2: arm 0 reads
  `DS_0010816B`, and `0x41350` stores `0xC835A[stage]` for side 0 only.
  `0x1078A7` is untouched with `DS_00108173` == 0.
- **(k2)** `DS_00104B1F` = 3: no `0x41350` and both think gates come from
  `0x430E8`. Characters 0/6: spawns 3 and 4 (found by parent) have different
  `+8`.
- **(l)** `0x25848`'s other arms:
  - rng(7);
  - the reduction at exactly 7 (a seed found by stepping the model);
  - arm 2 stores nothing;
  - the rng(2)-th zero byte;
  - the first non-matching byte (`0x42` = `DS_001078BE` | `0x40` is skipped);
  - the `DS_00104AD4` == 2 step (6 to 0, 3 to 4);
  - the frame byte `[DS_00104AD4 ^ 1]` for `DS_00104AD4` = 0 and 1.
- **(m)** The chain:
  - `DS_00104AE4` = `0x430E8` with `frontend_wipe_arm(0x2A)`;
  - 18 `frontend_mode_1a_step` frames run the hook (`0x430C0` installed, mode
    `0x1B`);
  - 17 `0x1B` frames draw nothing at row 2, and the 18th draws "VS" at row 2,
    col `0x13` (rows 1/3 are empty) and returns to mode `0x2A`;
  - the glyph equals a direct `0x2F198` call with mode `0x4002` and differs
    from mode `0x4000`'s.

`check_state6` also seeds `[0xF0A48]`, byte `[0x1088EC]` = 3, the
`0x2C074` pair and the `0x105C0C` sentinel before state 6 and checks that
`0x20DF4` zeroed or relinked them. It restores `[0xF0A48]`/`[0x1088EC]`,
which lie outside `test_fight`'s restore windows.

**Mutations** (`scratchpad/g5hook/mut.py`, `mut.log`/`mut2.log`): 76
single-site edits of the new code. 73 fail at least one assertion (M7, which
drops the `0x2C390` call, fails by a crash: run (b) walks the unlinked `0xA5`
fill). The three survivors are equivalent:
- M15 drops `0x4F200`'s `[0x107A55]` store, M16 its `[0x107A54]` store, and
  M18 its `0x4F1D0` call;
- the `0x2BAF4` that follows in the same function reaches `0x4F228`
  (`0x2BBC4`), which rewrites `0x107A54` = 0, `0x107A55` = AL = 0 and the
  words `0x107A38`/`0x107A3A` = 0 before any read, whatever `0x4F200`'s EAX.

The first run left five survivors that were not equivalent. Assertions were
added for each, and each mutation then failed:
- M24, the reduction's `>=`;
- M54, `c1` read from `0x10816A`;
- M73, `0x430E8`'s `0x1078A7` store, masked in (j) by `0x41350`'s side-1
  gate;
- M76, `pb` from spawn 1;
- M8 (a build error in the first run).

### 46-B.5 Measured and remaining gaps

- `PR_ORACLE_REQUIRED=1 ./build/run_tests`: all checks passed, with 0
  compiler warnings. The branch itself did not run `make verify` or the
  drivers, as the brief requires. The controller's review run did: `make
  verify` green, the front-end, demo-fight and attract2 oracles unchanged (N
  1886 and 3408 hold), and the full driver dump byte-identical to main.
- **State 6's change.** The headless `prageport --check 8000` was run from a
  scratch directory, with the base binary (`1151646` plus the test brace) and
  with this branch's. It gives byte-identical `frame_*.idx`/`.pal` for all
  8000 frames. A probe build (not committed) printed state 6's entries at
  ticks 1957, 3670, 4872 and 6585, with byte `[0x1088EC]` = 3 before the
  last three. Zeroing it has no visible effect in that window, and the
  controller's oracle run above agrees.
- The hooks themselves stay unreachable: modes `0x1A`/`0x1B` and `0x16`
  (`0x4F2B0`, which calls `0x25BBC` on the `0x28468` paths) are not
  dispatched (§43-B.5), and `0x28468` is unported.
- Remaining named gaps:
  - the voices, including `0x4F714`'s stage voice (§45-A's rule);
  - `0x25848`'s last arm for `[0x104AD4]` = -1, which reads a leftover
    stack byte (46-B.3);
  - `0x25848`'s other callers `0x4177D`, `0x418A8` and `0x42390`, and
    `0x20DF4`'s callers `0x25A95` and `0x295E4`;
  - the storers of the five hooks (`0x1EEB0`, `0x43AAC`, `0x43B24`,
    `0x44798`, `0x257A4`, `0x259CC`, `0x26978`, `0x27134`,
    `0x28468`);
  - the 7 `DS_00104AE4` values still unregistered (46-B.3);
  - the `game_frame` cases `0x1A`/`0x1B`, which still switch on a dword where
    the raw reads a word (§43-B.5).

### 46-B.6 Review round 1 (`gap5-hookcallers-review.md`)

Five corrections, none of which changes behaviour:
- 46-B.1: the `0x285F3` path stores the return mode 5, not `0x30` (only
  `0x285CB` stores `0x30`). On both `0x28468` paths mode `0x16`'s `0x4F2B0`
  calls the hook (`0x4F302`).
- 46-B.2: `0x295E4` passes the zero-extended word `[0x104AFC]`, like every
  other `0x20DF4` caller. The first version said it was the dword.
- 46-B.3: `[0x104AD4]` holds only -1, 0, 1 or 2. With -1, `0x25848` reads
  `[esp-2]`, a leftover byte below its frame, not an uninitialised frame byte
  or a saved register. The port's return is unchanged. Its note is now a
  `PORT:` note with this evidence, not a `TODO(verify)`.
- `flow.c` defined `DS_00104B1A` twice. The later copy (before `game_frame`)
  is removed and its comment is merged into the first.
- `check_state6`'s comment said `0x1088EC` was outside `test_fight`'s restore
  windows. It is inside `s_88` (`0x108840..0x10893F`); only `0xF0A48` is
  outside.

## 46-D. The type-`0x19` spawners `0x48F98`/`0x28F08` (update-table entries 1 and 10) and `0x37B54` (named-gap batch 5, branch `gap5-48f98`)

**Result in one line.** §41-D.5 named the update table's entries 1
(`0x48F98`) and 10 (`0x28F08`) as unregistered, so the port could not spawn
type `0x19`. Both are now ported, registered and unit-tested. So is their
one unported callee, `0x37B54`, which is also registered as the `0xD000`
target it is in two streams. That is 3 functions and 559 raw bytes
(439 + 92 + 28). Every other callee was already ported: `0x2BC30`
(`actors_anim_begin`), `0x2AE14` (`actor_spawn`) and `0x5D7DC`
(`rng_next`). The two voice calls are `PORT:` notes (§45-A's rule). No port
path enables either entry or starts either `0x37B54` stream, so no oracle is
expected to move (46-D.5).

### 46-D.1 The raw (Ghidra `read_memory` + capstone, fixups applied; Ghidra has no function at `0x48F98` or `0x28F08`)

**`0x48F98`** (`0x48F98..0x4914E`, `push ebx/ecx/edx/esi`). It walks the
type-`0x2D` in-use list `0x108368` (the sentinel `0x48C4C` self-links). ESI
= the node's next, read at `0x48FB1` before any call. The node's `+0x0C`
byte is the phase (`cmp al,1; jb; jbe`, `0x48FB3..0x48FBD`). Phases above 1
go straight to the loop tail `0x4913C`.
- The distance, in both phases: `|[node+8]+0x18 - [0x1077B0 +
  byte [0x1078FD] * 0x94]+0x18|`. That is the actor's x against the record
  of slot `DS_001078FD` (`imul edx,edx,0x94; mov edx,[edx+0x1077B0]`). The
  dword is negated when negative (`jge; neg`) and compared **signed** with
  `0x1000` (`jg`/`jle`).
- **Phase 0** (`0x48FCA..0x49077`). Beyond `0x1000` (`jg 0x4913C`) it skips.
  Otherwise:
  - `0x2BC30(actor, 0xEDD20, 0x40000000)` (stream `0xEDD20` at 2.0);
  - the actor's word `+0x34` = `cwd; sub ax,dx; sar ax,1`, a signed halving
    toward 0 (-3 -> -1, not -2);
  - AL = `[0x108396] & 0x80` (read first); then `inc byte [0x108397]` and
    the node's `+0x0C` += 1;
  - when AL was 0: `0x37B54([0x1077B0 + [0x104AD4] * 0x94])` (a 32-bit
    `imul` of the dword), `or byte [0x108396],0x80`, and the voice
    `0x2C3FC(0xF0)`. Its return is not read.
- **Phase 1** (`0x49078..0x4913B`).
  - Beyond `0x1000`: `0x2BC30(actor, 0xEDCEA, 0x40000000)`; the word
    `+0x34` = `add edx,edx` of it (the low 16 bits of the doubled word);
    `[0x108397]` -= 1; the node's `+0x0C` += 1; and, when the new
    `[0x108397]` is <= 0 **signed** (`test dl,dl; jg`), the voice
    `0x2C3FC(0xF1)`.
  - Within `0x1000` (`jle 0x490F9`): when `rng(0x14)` != 0 it skips.
    Otherwise it draws a5 = `rng(2)` ? `0x4000` : 0 and calls
    `0x2AE14(0xA89AC, EDX = actor+0x18, ECX = actor+0x30 SAR 16, EBX = 0,
    [esp] = a5)`. That is the `0x4912D` immediate §41-D.5 found.

`0x108396`/`0x108397` are the bytes `0x48C4C` zeroes (§42-B). `0x48CD8`, the
type-`0x2D` cb1, sets `DS_00104AE8` bit 1 (`0x48D22`), and `0x48D3C` clears
it when the count `0x108398` underflows (`0x48D75`).

**`0x28F08`** (`0x28F08..0x28F63`, `push ebx/ecx/edx`). EDX =
`[0x104AD4]` with `xor dl,1` (the dword with its low bit flipped), then EDX
= `[edx*4 + 0x1077A8]`, the other side's slot pointer. It is read before
the draws and never tested for 0. When `rng(6)` != 0 it returns. Otherwise
a5 = `rng(2)` ? `0x4000` : 0, and it calls `0x2AE14(0xA89AC, EDX =
slot+0x2C, ECX = [slot]+0x30 SAR 16, EBX = 0xC00, [esp] = a5)` (the
`0x28F54` immediate). `0x2AE14` pops its stack argument (no `add esp`
follows either call).

**`0x37B54`** (`0x37B54..0x37B6F`, 28 bytes). `mov al,[eax+0x51]; xor al,1;
and eax,0xff; mov eax,[eax*4+0x1077A8]; test; jz; mov eax,[eax]; mov byte
[eax+0x53],1; ret`. That is the other side's slot pointer; when it is not 0,
its record's `+0x53` = 1. Ghidra has this function (`FUN_00037b54`,
`__regparm3`), and its decompile agrees.

**`[0x104AD4]`'s values.** It holds -1, 0, 1 or 2 (46-B.3). The port keeps
the raw's 32-bit arithmetic in both functions (`(DSD ^ 1) * 4` and
`DSD * 0x94`, wrapping), so -1 and 2 read the same addresses the raw does:
`0x1077A0`/`0x1077B4` in `0x28F08`, and `0x10771C`/`0x1078D8` in
`0x48F98`'s `0x37B54` argument.

### 46-D.2 Entrances (`get_xrefs_to`, a rel32 CALL/JMP/Jcc scan of the code object and a dword scan of both fixed-up objects)

| address | references |
|---|---|
| `0x48F98` | data dword `0xA8648` only (= `0xA8644` + 1*4; Ghidra: the same, DATA) |
| `0x28F08` | data dword `0xA866C` only (= `0xA8644` + 10*4; Ghidra: the same, DATA) |
| `0xA8648`, `0xA866C` | none (the table is indexed from `0xA8644`) |
| `0x37B54` | rel32 `call` at `0x45B43`, `0x45FD3` and `0x4904F` (Ghidra: the same three); data dwords `0xE8564` and `0xEDAFC`, each after a `0xD000` word (`0xE8562`/`0xEDAFA`) |

**What enables the entries.** A capstone sweep of the code object for every
absolute operand at `0x104AE8..0x104AEB` finds these writes of bit 1
(entry 1) and bit 10 (`0x104AE9` bit 2):
- bit 1: set only by `0x48D22` (`0x48CD8`), cleared only by `0x48D75`
  (`0x48D3C`);
- bit 10: set by `0x48B3C` (`0x48AAC`, ported, §42-B) and `0x4013C`, and
  cleared by `0x48B89` (`0x48AAC`) and `0x40165`. `0x4013C` lies in
  `0x400EC`, the `0xD100` target at `0xE86F6`; `0x40165` lies in `0x40148`,
  the `0xD100` target at `0xE8716`. Neither is ported (out of this item's
  scope; `0x40148` calls `0x37D18`, which is ported as `fighter_37d18`).
- The whole-dword stores (`0x20E1C`, `0x20EC3`, `0x28DC4`, `0x2BB13`,
  `0x41435`) all store 0 (an `xor` of the source register just before).
- The other read-modify-writes touch other bits: `0x104AE8` 0x01 (the
  `0x13268..0x13275` clear), 0x04, 0x10, 0x20, 0x40 and 0x80; `0x104AE9` 0x01, 0x02, 0x08, 0x10, 0x20, 0x40 and
  0x80.

**Reach in the port.** `0xC950C`, the only type-`0x2D` descriptor (the type
table's entry `0x2D` at `0xBBBF4`; its other reference is the `0x48E7E`
immediate in `0x48D94`), is spawned only by `0x48D94`'s state 2.
`0x48D94` and `0x48AAC` are reached only through `DS_001078E8`, which the
unported `0x37774`/`0x37898` fill (§42-B). So no port path sets either
bit. The two `0x37B54` stream sites are reached as follows:
- `0xEDAFA` is in `0xEDAB2` (`0xEDAB2..0xEDAF9` is five loops, one `8E40`
  word and the `D000 0x489DC` at `0xEDAF4`). `0xEDAB2` is the stream of
  `0xC9288[2]`, `0xC92B0[2]`, `0xC9288[7]` and `0xC92B0[7]` (dwords
  `0xC9290`, `0xC92B8`, `0xC92A4` and `0xC92CC`).
- `0xE8562` is in `0xE8550` (after a `9301` word and eight sprite words).
  `0xE8550` is the stream of descriptor `0xBB114`, which stream `0xE8616`
  (`0xC9288[0]`) spawns at `0xE8646` (`CC01 000BB114`).

The readers of `0xC9288`/`0xC92B0`/`0xC92D8` (`0x3769B`, `0x377DD`,
`0x37902`) are unported finisher code: `0x3769B` stores to `DS_001078E4`.
The port reads `0x1078E4` only in `0x379C4`'s arm, and no port path writes
it.

### 46-D.3 The port

- `actors.c`, after `actor_type_0a19_update` (`0x2910C`), has:
  - `fighter_37b54` and its `(rec, arg)` wrapper `anim_code_37B54`, which
    drops the arg the raw never reads;
  - `actor_type_2d_update` (`0x48F98`) and `actor_type_19_spawn`
    (`0x28F08`), `fn()` like the other update-table entries (the EAX index
    is not read).
- Local `#define`s name `0xA89AC`, `0xEDD20`, `0xEDCEA`, `0x108396` and
  `0x108397`, which `symbols.h` does not name.
- The distance is computed only for phases 0 and 1, as in the raw, so a
  held node's `+8` is never dereferenced.
- The registration block appends the three after §46-C's.
- The voices `0xF0`/`0xF1` are `PORT:` notes ("not wired (record §45-A)").
  Because they are unwired, phase 1's signed `[0x108397]` <= 0 test (which
  only gates `0xF1`) has no port effect. The decrement it tests is kept.

### 46-D.4 The assertions and mutations

`check_update_48f98` (`test_fight.c`, after `check_volleyball`) saves and
restores the data object and the scratch records and psets (the §42-A
`q42_save`/`q42_restore`). Every scenario starts from that snapshot
(`r46_seed`). The reference-state compare also covers `mem[0..0xFF]`, where
a write through a zero slot would land.
- **The data.** `0xA8648` = `0x48F98`, `0xA866C` = `0x28F08`, `0xE8564`/
  `0xEDAFC` = `0x37B54` after `0xD000` words, `0xA89AC`'s type byte `0x19`,
  and all three resolve.
- **The streams.** The cursors `0x2BC30` leaves on `0xEDD20` and `0xEDCEA`
  (a scratch record) differ, and each start is checked against its own
  cursor with `+0x24` = `0x40000000`.
- **`0x37B54`, through its registration.**
  - `+0x51` 0 marks record 5, `+0x51` 1 marks record 4, and the other
    record keeps its sentinel. `+0x52` is seeded non-zero (1, `0x80`), so
    only the byte index reaches the slot table.
  - With a zero slot the whole reference state is unchanged.
- **Phase 0.**
  - Actors at +`0x1000` (the edge), +`0x1001` and -`0x1000`, a phase-2
    actor, and `+0x34` = -3 -> -1, 5 -> 2 and `0x8000` -> `0xC000`.
  - `0x108397` `0x7F` -> `0x82`, `0x108396` 5 -> `0x85`, and `0x37B54`'s
    target, with no spawn.
  - With bit 7 preset there is no `0x37B54`. With `DS_00104AD4` = 1 the
    other record is marked.
- **Phase 1 beyond `0x1000`.**
  - At -`0x1001` and +`0x2000` from the watched record, `+0x34` `0x4001` ->
    `0x8002` and `0xFFF0` -> `0xFFE0`; with the next item's two actors,
    `0x108397` goes 2 - 3 + 1 = 0.
  - A phase-3 actor holds, and nothing draws.
- **The watched slot.** With `DS_001078FD` = 0 the watched record is record
  4 (x `0x90000`). A phase-0 actor at `0x90000` starts `0xEDD20` and calls
  `0x37B54`, and a phase-1 actor at record 5's x `0x20000` starts `0xEDCEA`.
  Under slot 1 both outcomes would flip.
- **Phase 1 within `0x1000`, and `0x28F08`.**
  - When the gate draw is not 0, the whole state equals one draw.
  - When it is 0, the whole state equals the port's `0x2AE14` run from the
    same state after the two draws with the raw's arguments, and differs
    from it with a3/a4 swapped. This is done for a5 `0x4000` and 0, and for
    `0x28F08` with `DS_00104AD4` 0 and 1. The spawned record is the free
    one, with type `0x19` and `+0x28` bit 14 = a5.
  - `0x28F08` with a zero slot 1 spawns anyway, from `mem[0x2C]` (x) and
    the dword at `mem[0]` (`0x40`, so the height comes from `mem[0x70]`),
    as the untested raw pointer does. The test saves `mem[0..0xFF]` and
    puts it back after `q42_restore`.
- **The rng seeds** sit at the gate's edge: the zero draw is one that
  `rng(r + 1)` would not give, and the one draw is one that `rng(r - 1)`
  would make 0. So a gate on any other range fails.

Mutations (each built and run with `PR_ORACLE_REQUIRED=1`; the count is the
suite's FAIL lines, re-measured after review round 1's test additions). All
45 fail, and every source was restored.
- `0x37B54`:
  - `^1` dropped: 10;
  - null check dropped: 1;
  - store 2: 5;
  - `+0x52`: 5;
  - a word index (`DSW(rec + 0x51)`) for the byte: 2;
  - unregistered: 1.
- `0x48F98`, phase 0:
  - edge `>=`: 8;
  - no `abs`: 9;
  - the watched slot read from `DS_00104AD4`: 15;
  - the watched slot hard-coded to 1: 9;
  - `sar` for the halving: 1;
  - no `0x108397` increment: 3;
  - no phase increment: 4;
  - the once-gate inverted: 6;
  - no `0x108396` set: 2;
  - `0x37B54`'s slot from `DS_001078FD`: 2;
  - `0xEDCEA` for `0xEDD20`: 5;
  - 3.0 for 2.0: 1.
- `0x48F98`, phase 1:
  - edge `>=`: 12;
  - no doubling: 3;
  - no decrement: 1;
  - decrement by 2: 1;
  - no phase increment: 3;
  - `0xEDD20` for `0xEDCEA`: 3;
  - 3.0 for 2.0: 1;
  - phases above 1 treated as 1: 1.
- `0x48F98`, the spawn:
  - `rng(0x10)` for `rng(0x14)`: 1;
  - `rng(0x15)`: 7;
  - `rng(3)` for the a5 draw: 2;
  - a5 inverted: 4;
  - a2 from `+0x1C`: 2;
  - a3/a4 swapped: 4;
  - `>> 15`: 2;
  - unregistered: 42.
- `0x28F08`:
  - `^1` dropped: 3;
  - `rng(5)`: 1;
  - `rng(7)`: 9;
  - the gate inverted: 10;
  - a4 0: 3;
  - a2 from `+0x28`: 3;
  - the height from the slot, not its record: 3;
  - a5 inverted: 5;
  - a3/a4 swapped: 5;
  - a zero-slot `return` added: 2;
  - unregistered: 11.

The first run left two survivors, `rng(0x10)` and `rng(5)`: the first seeds
drew the same zero/non-zero outcome on both ranges. The edge seeds kill
them. Review round 1 found three more (the hard-coded watched slot, the word
index, the zero-slot `return`); the tests added in 46-D.6 kill them.

### 46-D.5 Measured and remaining gaps

Not measured with the drivers (this batch does not run them). The claim
that no oracle moves rests on 46-D.2's reach argument:
- the two bits' only port-reachable setters are in `0x48CD8` (spawned only
  by `0x48D94`) and `0x48AAC`, both behind the unported `0x37774`/`0x37898`;
- the two `0x37B54` stream sites are reached only through the unported
  `0xC9288`/`0xC92B0` readers.

Before this change, the port's `run_process_table` skipped entries 1 and 10
through the `fn_resolve` miss. If a later port reaches either, the
type-`0x19` spawns (and their draws) now happen as in the raw.

Remaining named gaps:
- the voices `0x2C3FC(0xF0)`/`(0xF1)` (§45-A's rule);
- `0x400EC`/`0x40148`, the `0xD100` targets that set and clear entry 10's
  bit (dwords `0xE86F6`/`0xE8716`);
- `0x37B54`'s other callers `0x45B43`/`0x45FD3`, in code the port does not
  have;
- the finisher flow that reaches all of this (`0x37774`/`0x37898`, the
  `0xC9288`/`0xC92B0`/`0xC92D8` readers, `DS_001078E4`; §42-B).

### 46-D.6 Review round 1 (`gap5-48f98-review.md`)

No port behaviour changed; the fixes are in the tests and the record.
- **The watched slot `DS_001078FD` was not pinned.** Hard-coding it to 1
  survived: every scenario used FD = 1 except C, whose actors were far from
  both records. C's comment also described a "node 3" that did not exist,
  and 46-D.4 claimed the case was tested. C now has a phase-0 actor at
  record 4's x and a phase-1 actor at record 5's x (46-D.4, "The watched
  slot"); the mutation fails 9.
- **`0x37B54`'s byte index was not pinned.** The test comment claimed a
  seeded `+0x52`, but none was seeded. `+0x52` is now 1/`0x80`; a word index
  fails 2.
- **`mem[0..0xFF]`** was zeroed by `r46_seed` and not restored. The test now
  saves it at entry and puts it back after `q42_restore`.
- **`0x37D18` is ported** (`fighter_37d18`, called at `0x34A29`). 46-D.2
  and 46-D.5 called it unported; corrected. `0x40148` itself stays
  unported.
- **Stale "unregistered" text** at §41-D (twice), §42-B and §42-A now
  carries "(since ported and registered, §46-D)".
- **Raw-wins correction (§13's `+0x42` sweep).** §13 listed `0x37B54` among
  the unported functions that store 0 or a register to `slot+0x42`. The raw
  disagrees: `0x37B54..0x37B6F` writes only `[[slot]+0x53]` (`0x37B6B`). The
  `+0x42` read-modify-write in that range is `0x37CDC`/`0x37CE2`, inside
  `0x37CD4` (`0x37CD4..0x37CF1`, an animation target, since ported). The
  misattribution came from taking the nearest preceding Ghidra function:
  Ghidra has none between `0x37B70` and `0x37D18`. §13's list now says so.
- Recommended, done: `0x28F08` with a zero slot 1 is tested (it reads
  `mem[0x2C]` and the dword at `mem[0]`, as the raw does); a zero-slot
  `return` fails 2. The port's comments now call a3 the height (`+0x32`) and
  a4 the y (`+0x1C`): `0x28F08`'s y is `0xC00`, and `0x48F98`'s is 0.
