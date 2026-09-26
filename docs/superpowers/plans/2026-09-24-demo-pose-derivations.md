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
