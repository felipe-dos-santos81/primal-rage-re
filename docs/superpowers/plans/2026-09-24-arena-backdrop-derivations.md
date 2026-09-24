# The demo arena backdrop — derivation record (Task 1, cycle 5)

**Deliverable.** The raw-byte and runtime derivation of the state-7 arena render
gap, the missing layer's owner chain, the porting plan, the size-gate verdicts,
the measurement plan, the named gaps, the unit-test values and the provenance.
**No porting code**: this record is the only artefact.

**Result in one line.** The missing layer is the **crowd actor 0's two mountain
children** (sprite ids **757** = 228×113 and **758** = 0×0) plus the crowd actor
0's own sprite (**756** = 155×54); the port spawns all three and then kills them
because `actor_spawn`'s per-type callback test (`0x2B0D4`) recognises only the
stub `0x5D812` instead of calling the type's real callback — and type `0x1B`'s
callback (`0x412FC`) returns **0** (visible). Making the dispatch faithful
renders the backdrop and the port's frame 482 byte-matches capture 834
(**0 / 192 000 B**, from 18 294 B / 6 194 px). The size gate does **not** trigger
(17 new functions / 1 148 B; closure 25 f / 1 544 B).

---

## 0. Addressing, reproduction and corrections

### 0.1 The image, the conventions and the two formulas

* Ghidra address == linear address == object base + offset (no segment:offset).
  Code object `0x10000`–`0x73B14`; data object `0x80000`–`0x10B0CF`.
* A global Ghidra calls `DAT_0008xxxx` is at **DS offset `0xxxx`** (subtract
  `0x80000`).
* **Data VA → runtime linear = VA + `0x1E6000`** (the DOS/4GW relocation base of
  the data object is `0x266000`; the demo-fight-closure record §9.5 derived the
  same delta from a live-RAM dump, and this record re-confirms it independently:
  `DSD(0xBB9D8)` reads `0x2A0B74` = `0x266000 + 0x3AB74`, and `DSD(0xBDE50)`
  reads `0x2BE0`).
* **Code VA → runtime linear = VA + `0x1F1000`** (code object base `0x201000`,
  demo-fight-closure §9.5).
* The sprite-id table is `DS_000A8B30[id & 0x7FFF]` → a resource handle; the
  handle's `>>23` is the INDEX entry and `& 0x7FFFFF` the file offset inside it.
  INDEX index 21 = `s16beach.gra` (`0xA838xxx` handles).

### 0.2 Reproduction

```bash
# the port's instrumented dump (the design's instrumentation + a type log), reverted before the commit
cmake --build build
rm -rf /tmp/pr_rl
PR_FRONTEND_DET=/tmp/pr_rl PR_RL_DUMP=1 PR_GAME_DIR=data/game/C ./build/run_tests >/dev/null 2>&1
# /tmp/pr_rl/run1.log carries the RL/SPAWN/ACT lines; /tmp/pr_rl/run1/frame_NNNN.raw the frames

# the cycle's gate
python3 -c "a=open('/tmp/pr_frontend_dump/run1/frame_0482.raw','rb').read(); b=open('data/title-captures/frontend/frame_0834.raw','rb').read(); d=[i for i in range(len(a)) if a[i]!=b[i]]; print(len(d),'bytes',len({i//3 for i in d}),'px')"

# the enforced claims
make verify
```

### 0.3 Corrections against the plan/brief/records (raw and measurement win)

1. **Cycle 4's "the port never renders the arena backdrop" is false.**
   `pose-freeze-derivations.md` §11.4 and the README's gap inventory say the
   residual is a backdrop "which the port never renders". Measured: the port
   spawns and draws both scene actors (layer 2 = sky, id 11232; layer 1 = sea,
   id 11233) at frame 482. The missing content is a **third** element. The
   correction is recorded in §1 and the design's Evidence already carries it.

2. **Cycle 4's owner is wrong.** §11.4 assigns the residual to "the
   demo-fight-closure subsystem — `platform/render.c`'s scroll/zoom path
   (`0x38730` → `0x387F4`/`0x38890`/`0x38A38`)". The owner is
   **`port/src/game/actors.c`'s `actor_spawn` tail (`0x2B0D4`)** — the per-type
   callback dispatch. The render path, the scene actors, the sprite resolves and
   the positions are all correct and unchanged.

3. **The port's own comment is refuted.** `actors.c`'s `actor_spawn` tail says
   "No object the port spawns reaches them (the title's 0x9AC30/0x9AC94, the
   fighters' descriptors and the dust's 0xBB4C0 are the stub)". Measured: the
   demo's crowd actor 0 (`desc=0xC7850`) carries type `0x1B`, whose callback is
   `0x412FC` — not the stub. Five further spawns carry type `0x01`
   (`cb=0x127C0`) and are killed the same way. Task 2 replaces the comment.

4. **The design's acceptance #2 ("every enforced claim unmoved") is false.**
   Measured: the front-end oracle's window moves from `[560..830]` / 271 /
   `0 unexplained` to `[560..842]` / 283 / **2 unexplained (832, 833)**. The
   other four enforced claims are unmoved. The move is justified and is recorded
   in §1.6/§6; §3.5 gives the handling.

5. **The scene-actor descriptor tables overlap.** The 16 descriptors are
   `0x14`-byte records at `0xBDE3C + i*0x14` (`0xBDE3C`..`0xBDF7B`); the 16-entry
   pointer table is at `0xBDF7C`, and `0xBDF9C` is `0xBDF7C + 0x20` — i.e.
   `render_scroll_scene_b(i)` reads `0xBDF7C[i+8]`, the same table's second
   half. The port's two macros (`DS_000BDF7C`/`DS_000BDF9C`) are correct; the
   record notes the layout because the sprite ids (even = scene actor A, odd =
   scene actor B) follow from it.

6. **Pre-fixup displacements are not runtime addresses** (AGENTS.md's warning,
   re-confirmed twice this cycle). Disassembling the code object without the LE
   fixups read `jmp dword ptr cs:[eax*4 + 0x38D80]` at `0x48DB7` and
   `call dword ptr [ebx + 0x3B9E0]` at `0x2B185`; the fixup-applied values are
   `0x48D80` and `0xBB9E0` (Ghidra `disassemble_bytes`/`read_memory`). Every
   data address in this record is Ghidra's (fixup-applied) value.

7. **`DS_000F0A64` is not the demo's state word** — it is `0` in the live dump
   at the arena. The state the design refers to ("state 7") is carried by the
   flow's own counters; the record uses the *frame index* (port 482 ↔ capture
   834) as the content point, which is measurable, rather than a state word.

8. **Eight address strings in this record's first edition of §3.3 violated rule 6**
   (the `0x127C0`, `0x48CD8`, `0x40684`, `0x3FC90`, `0x48D3C` and `0x49444`
   rows). **Seven are genuine pre-fixup displacements** (`0x70A78`, `0x84AE8`,
   `0x88398`, `0x87EF8`, `0x88080`, `0x8839C`, `0x883C4` — each is the corrected
   value minus `0x80000`) and are corrected in §3.3 against Ghidra (`0xF0A78`,
   `0x104AE8`, `0x108398`, `0x107EF8`, `0x108080`, `0x10839C`, `0x1083C4`).
   **The eighth, `0x10882E0`, is a transcription slip, not a pre-fixup form**:
   the corrected value is `0x1082E0`, whose pre-fixup form is `0x882E0`;
   `0x10882E0` is the fixup-applied value with an extra `0x80000`. The
   correction matters because the wrong forms land on **different, non-zero**
   globals (`DSB(0x88398)` reads `0x80`, `DSB(0x84AE8)` reads `0x72`) while the
   correct ones are zero, and `port/src/mem.h:22` is `DSB(o) = mem[o]` — a
   transcription of the wrong value would corrupt unrelated state. The same rule
   also caught `0x2BE5C`'s `word[0x107900]` (first read as `0x87900`, a genuine
   pre-fixup form of `0x107900`).

9. **`0x2C3FC` is not a stub.** The size-gate method's "five known stubs"
   (`0x2C3FC`, `0x2EA64`, `0x62002`/`0x62003`/`0x6201B`, cycle-3 §10.4) treats
   `0x2C3FC` as one. The raw: `FUN_0002c3fc`, **1268 B, 206 callers, 8
   callees** — the voice dispatcher, which the *port* has as an out-of-scope
   stub. The exclusion is kept (it is the method's rule and the port's status),
   but the record states the raw fact and the sensitivity in §4.3: counting it
   would put the group **over** the gate (190 f / 22 795 B closure, 29 new).

10. **§7.3's teardown seed names the wrong type (Task 2 correction).** The seed
    says `DSB(rec+0x48) = 0x01`, but type 0x01's cb2 is `0x12800`
    (`DSD(0xBB9E0 + 0x01*0xC)`, the `0xF0A78` return), which neither clears
    `rec+0x48` nor touches the `0x100C20` list the same bullet names. The
    bullet's assertions describe type 0x06's cb2 `0x19928` (`DSD(0xBB9E0 +
    0x06*0xC)`), which clears the type at `0x1994E` and returns the node to
    `0x100C20` at `0x1993A`. Task 2 seeds type 0x06 (and tests type 0x01
    separately against its real cb2's post-state). The bullet's "or 0"
    alternative is also unreachable for the `rec+0x48 == 0` assertion: the
    `JZ 0x19931` returns before it (Task 1's review already noted this).

11. **§7.5's `0x2BE5C` mode-1 value needs the ramp seed (Task 2 correction).**
    The mode-1 arm writes `word[rec+0x46]` (the high word of the dword at
    `rec+0x44`) from the `0x107900` ramp at `0x2BEBC` *before* reading
    `rec+0x44` at `0x2BEC0`, so the dword's high word is the ramp entry, not
    anything seeded at `rec+0x44`; and `mode1_cursor` writes `rec+0x64` first
    (`0x2A620`), which is the byte the ramp index `(s32)DSD(rec+0x61) >> 24`
    reads. A seed of `pset+4`/`DS_000F0AF0`/bit 12 alone therefore cannot
    observe the mode-1 term. Task 2 seeds the ramp entry at the index
    `mode1_cursor` produces (`DS_000F0AEC = 0xFFFFC580`, `DS_00107A4C = 0` ->
    index 5).

12. **The five type-0x01 demo spawns move no claim (Task 2 measurement).** With
    the 16-callback dispatch the five `cb = 0x127C0` spawns survive, but a
    targeted isolation (type 0x01 only killed) leaves **0 of 1381** dumped
    frames different. They change no oracle claim.

---

## 1. The runtime differential (Step 1)

### 1.1 The port's list at frame 482 (measured, pre-fix)

`PR_RL_DUMP=1` instrumentation (the design's, extended with the pset's sprite
descriptor handle and its 12 bytes), reverted before the commit:

```
RL f=482 layer=1 pset=02a79f70 id=11233 pal=00107668 px=0 py=420 x=-326 y=137 W=975 H=64  ph=0a81a84e h=0a838e24 d12=ffc0fc31,00000000,0a81a84e
RL f=482 layer=2 pset=02a79f50 id=11232 pal=00107628 px=0 py=0   x=-82  y=-76 W=549 H=213 ph=0a806c9f h=0a838cf8 d12=00d50225,00000000,0a806c9f
```

* The list carries **80** nodes: the two scene actors, the 5 props (ids
  751–755, layers 174/179/187/192/192), the 2 crowd actors (ids 364/353, layers
  180/188), four off-screen actors (ids 2172/35505/35281/2011, layers
  198/199/207/208, `px` −543/−580/883/903), the fighters/HUD (layers
  220/222/223/225) and **63 × layer 255** (the text glyphs). The design's
  baseline is reproduced exactly.
* `y=-76` is the layer-2 rule's **pre-clip** value (`last_mode1_y` 137 − rows
  213); the design's "y = 0" is the same node **post-clip** (`clip_t` 76). Both
  are correct; they are the same draw.
* The sea's descriptor `0xA838E24` reads `{0xFC31, 0xFFC0, 0, 0}` — i.e.
  **width −975, height −64**, which `sprite_node_build` (0x14268) negates into
  975×64 with `type |= 2`. Not a divergence.

### 1.2 The absent entries (measured)

`grep -c "id=756 \|id=757 \|id=758 "` over the whole 1 381-frame dump = **0**.
The port's render list never carries the mountain sprites, in any frame.

### 1.3 The capture's side (measured)

`data/title-captures/frontend/frame_0834.raw` carries the dark mountain
silhouette. The port's `frame_0482.raw` differs by **18 294 B / 6 194 px**
(bounding box x 30..319, y 49..136) — the design's residual reproduced exactly.

### 1.4 The spawn trace (measured)

The instrumentation's `SPAWN desc=%08x type=%02x cb=%08x id=%u a5=%08x` line at
`actor_spawn`'s tail (before the type test), over the demo run:

```
SPAWN desc=000c7864 type=00 cb=0005d812 id=757 a5=00000407   <- mountain A
SPAWN desc=000c7878 type=00 cb=0005d812 id=758 a5=00000407   <- mountain B
SPAWN desc=000c7850 type=1b cb=000412fc id=756 a5=00000000   <- the parent
SPAWN desc=000bb254 type=01 cb=000127c0 id=33409 a5=00004000  (x4, later)
```

**The two mountain children ARE spawned** (inside the parent's own
`actor_spawn`, before the parent's `id` is written), and **the parent IS spawned
with type `0x1B`**. The children carry `a5 = 0x407` = `parent_slot(7) | 0x400`
(the child path). So the missing layer is not a missing spawn.

### 1.5 The cascade (measured)

An `ACT` dump of the active actor list at frame 480 (all 80 records, with each
record's slot and pset id) shows slots **7, 8 and 9 absent**. Slots 0–6 and
10–82 are present. So the three records were spawned and then **released**.

The chain, from the raw:

1. `actor_spawn`'s tail (`0x2B0D4`) marks the parent dead because the port's
   test (`cb == FN_0005D812`) fails for `0x412FC` — it sets `rec+0x48 = 0` and
   `rec+0x28 |= 8` and returns 0 (`0x2B0F3`–`0x2B102`).
2. On the next `actors_update`, `actor_sync`'s tail (`0x2A1FC`, port
   `actors.c:1303`) sees `rec+0x28 & 8` and calls `release_record` (`0x2AD40`),
   which unlinks the parent and pushes it to the free list.
3. The children's `pset_write` (`0x2A820`) takes the parent-relative arm
   (`rec+0x28 >> 8 & 4`, from `0x1E00`) and hits
   `if ((DSW(parent+0x28) & 8) != 0) goto dead` — the parent is dead, so each
   child is marked dead in turn (`0x2A8xx`), and the next `actor_sync` releases
   them.

So the whole mountain layer disappears through **one wrong predicate**.

### 1.6 The fix's effect (measured, temporary and reverted)

Replacing the predicate with a faithful call to the callback (temporarily
implementing only `0x412F0` and `0x412FC`, the two callbacks the demo's spawns
reach) changes:

| frame pair | pre-fix | post-fix |
|---|---|---|
| port 482 ↔ capture 834 | 18 294 B / 6 194 px | **0 B / 0 px** |
| port 483 ↔ capture 835 | (differed) | **0 B / 0 px** |
| port 484 ↔ capture 836 | (differed) | **0 B / 0 px** |
| port 485 ↔ capture 837 | (differed) | **0 B / 0 px** |
| port 486 ↔ capture 838 | (differed) | **0 B / 0 px** |
| port 487 ↔ capture 839 | (differed) | **0 B / 0 px** |

and the list at 482 gains exactly three nodes (83 total):

```
RL f=482 layer=94 pset=02a7a030 id=756 pal=00107628 px=152 py=92 x=69  y=83 W=155 H=54   ph=0a832d6a h=0a838d10
RL f=482 layer=94 pset=02a7a050 id=757 pal=00107628 px=320 py=92 x=226 y=24 W=228 H=113  ph=0a833929 h=0a838d04
RL f=482 layer=94 pset=02a7a070 id=758 pal=00107628 px=440 py=92 x=389 y=50 W=0   H=0    ph=0a800000 h=0a838e30
```

The children's `px` are the parent's `px` + 168 and + 288 (the stream's inline
`a2` values `0x00A8`/`0x0120`), and all three share layer **94** and `py` 92.
The layer is `0xF0 − (a3 >> 6) + (slot >> 24)` (`actor_pset_point`, `0x2A690`);
the crowd record's `a3 = 0x2492` gives `0xF0 − 146 = 94`.

### 1.7 The original's live RAM — attempted, not obtained

The demo-fight-closure record §9.5's method was re-attempted:

* `/opt/homebrew/bin/dosbox-x` (2026.08.31) with `-break-start` **does** present
  the ncurses debugger on a pty; `BPLM <linear>` (linear memory-change
  breakpoint), `RUN`, `BPLIST` and `MEMDUMPBIN <addr> <len>` are all accepted,
  and one cycle produced a 16 MiB dump with the game image loaded — verified by
  `"RAGE.S16"` at linear `0x26602D`, `DSD(0xBB9D8) = 0x2A0B74` and
  `DSD(0xBDE50) = 0x2BE0` (i.e. the data object's runtime base is `0x266000`).
* **But the needed break did not fire in the session budget.** The intended
  trigger was `DS_00105C08` (linear `0x2EBC08`, the crowd-table store at
  `0x2C339`); the session's `RUN`+poll loop never confirmed the break, and
  `[dosbox] memory file` is written only at debugger start, so no game-state dump
  of the arena frame was obtained.

**Consequence (and the fallback the brief allows):** the original's render list
was **not read from live RAM**. The original's side is **derived** from the raw
bytes (Ghidra) plus the capture; the *decisive* evidence is the capture itself —
the original's own recorded output — which the fixed port now reproduces
byte-exactly for six consecutive frames. Marked as measured/derived:

| claim | source |
|---|---|
| the port's list lacks 756/757/758; the children are spawned; the parent is type 0x1B | **measured** (port trace) |
| the port kills the parent and releases all three | **measured** (port trace) + **derived** (raw `0x2B0D4`/`0x2A1FC`/`0x2AD40`) |
| the type-0x1B callback is `0x412FC` and returns 0 | **derived** (raw `0xBB9DC + 0x1B*0xC`, `0x412FC`) |
| the mountains are ids 756/757/758, from `s16beach.gra`, at layer 94, `px` 152/320/440 | **derived** (raw) + **measured** (post-fix port) |
| the original's own list at capture 834 | **derived** (raw) + **measured** (the capture is byte-exact against the port's post-fix frame) |

---

## 2. The missing layer's owner chain (Step 2)

### 2.1 The chain

```
game_state_6 (0x20E86)                       port: flow.c game_state_6
  0x412A0  fight_scene_props(scene=0)        port: fight.c:98
    0xC7F78  the 5 prop triples → the temple  (ids 751–755; correct)
    0x2C320  fight_scene_crowd(scene=0)      port: fight.c:69
      0xBBDA8[0] = 0xBBC18, n = DSW(0xBBD98) = 3
      record 0: a2 = 0xFFFFEEA0 (−4448), a4 = 0, a3 = 0x2492 (9362),
                DSB(e+0xA) = 0x1B (the type/desc index), DSB(e+0xB) = 0
      desc = DSD(0xBB9D8 + 0x1B*0xC) = 0x0C7850
      0x2AE14  actor_spawn(0xC7850, −4448, 9362, 0, 0)   ← the owner's root
        0x2B00A  the initial animation-stream walk over 0x0E8EB2
          0x2B484  opcode 0x0C, DS_00105BD4 = 0xC7864, a2 = 0x00A8
                   → actor_spawn(0xC7864, 168, 0, 0, 7|0x400)  = id 757
          0x2B484  opcode 0x0C, DS_00105BD4 = 0xC7878, a2 = 0x0120
                   → actor_spawn(0xC7878, 288, 0, 0, 7|0x400)  = id 758
          walk stops at the word 0x02F4 (bit 0x8000 clear)
        0x2A408  anim_next_sprite_id → DSW(rec+8) = 0x02F4 = 756 (the literal-id flag 0x0800)
        0x2B0D4  the per-type render check  ← THE DIVERGENCE
```

### 2.2 The descriptors (raw, Ghidra, fixup-applied)

The scene-crowd descriptor table `0xBB9D8` is an array of 12-byte records
`{desc, cb1, cb2}` indexed by the **type byte**; the descriptor's `dp[4]` is the
same index. The two callback tables are therefore `0xBB9DC` (= `0xBB9D8 + 4`,
cb1) and `0xBB9E0` (= `+8`, cb2). Record `0x1B`:

| field | value | proving address |
|---|---|---|
| `DSD(0xBB9D8 + 0x1B*0xC)` | `0x0C7850` | the crowd record's `DSB(e+0xA) = 0x1B` at `0xBBC22` |
| `DSD(0xBB9DC + 0x1B*0xC)` | `0x000412FC` | `0x2B0E9 CALL dword ptr [EBX + 0xbb9dc]` (EBX = type*0xC) |
| `DSD(0xBB9E0 + 0x1B*0xC)` | `0x0005D812` (the stub) | `0x2B185 CALL dword ptr [EBX + 0xbb9e0]` |

The three descriptors the chain reaches (all in the data object):

| desc | dword0 | dp[4] type | dp[8..9] flags | dp[0x0A] extent | dp[0x10] palette |
|---|---|---|---|---|---|
| `0xC7850` (the parent) | `0x000E8EB2` (the stream) | `0x1B` | `0x1200` | 64 | `0x0A838B44` |
| `0xC7864` (child A) | `0x000002F5` = 757 | `0x00` | `0x1A00` | 64 | `0x0A838B44` |
| `0xC7878` (child B) | `0x000002F6` = 758 | `0x00` | `0x1A00` | 64 | `0x0A838B44` |

The animation stream `0x0E8EB2` (the words, `DSW`):

| offset | word | meaning |
|---|---|---|
| `+0x00` | `0xCC01` | opcode `(>>8)&0x1F` = `0x0C`, operand `0x01`, mode `0x4000` |
| `+0x02` | `0x7864` | the low word of the inline dword `0x000C7864` → `DS_00105BD4` |
| `+0x04` | `0x000C` | the dword's high word |
| `+0x06` | `0x00A8` | `a2` = 168 |
| `+0x08` | `0x0000` | `a4` = 0 |
| `+0x0A` | `0xCC01` | opcode `0x0C` again |
| `+0x0C` | `0x7878` / `+0x0E 0x000C` | the inline dword `0x000C7878` |
| `+0x10` | `0x0120` | `a2` = 288 |
| `+0x12` | `0x0000` | `a4` = 0 |
| `+0x14` | `0x02F4` | bit `0x8000` clear → the walk stops; `anim_next_sprite_id` returns 756 |

### 2.3 The sprite ids (raw, fixup-applied)

| id | `DSD(0xA8B30 + id*4)` | INDEX | file offset | sprite | `0xA8B30` proof |
|---|---|---|---|---|---|
| 756 | `0x0A838D10` | 21 `s16beach.gra` | `0x38D10` | 155×54, org (76,−6) | table read |
| 757 | `0x0A838D04` | 21 | `0x38D04` | 228×113, org (79,53) | table read |
| 758 | `0x0A838E30` | 21 | `0x38E30` | 0×0, org (30,27) | table read |

The two mountain silhouettes were confirmed by decoding the RLE at
`s16beach.gra + 0x38D04` (228×113) and `+0x38D10` (155×54) — both render as
mountain ranges. `758` is the degenerate 0×0 sentinel (its `pixel_handle`
`0x0A800000` points at the GRA chunk header); it contributes no pixels and is
harmless once the parent survives.

### 2.4 The blocking function and its ported status

| addr | size | role | ported? |
|---|---|---|---|
| `0x2AE14` | 825 | `actor_spawn` — the walk, the pset write and the `0x2B0D4` type check | **yes** (`actors.c:1333`), the tail is wrong |
| `0x2B0D4` (site) | — | `CALL dword ptr [EBX + 0xbb9dc]`, `TEST AL,AL`, dead on non-zero | **no** — the port tests `== FN_0005D812` |
| `0x2B150` | 148 | `set_dead` — `rec+0x28 |= 8`, then `CALL dword ptr [EBX + 0xbb9e0]` and `rec+0x2b &= ~0x40` | **yes** (`actors.c:1227`), the teardown call is a documented no-op |
| `0x2A820` | — | `pset_write` — the parent-dead cascade | **yes** |
| `0x2A1FC` | — | `actor_sync` — `if (rec+0x28 & 8) release_record` | **yes** (`actors.c:1303`) |
| `0x2AD40` | — | `release_record` | **yes** |
| `0x2A690` | — | `actor_pset_point` — the layer formula | **yes** |
| `0x2C320` | 102 | `fight_scene_crowd` | **yes** (`fight.c:69`) |
| `0x412A0` | 77 | `fight_scene_props` | **yes** (`fight.c:98`) |
| `0x1C390`/`0x1C3A0`/`0x1C458`/`0x1C3D0` | — | the render list | **yes** (`render.c`) |
| `0x249B0`/`0x249D0` | 15/31 | list insert-after / unlink | **yes** |

### 2.5 The two dispatch sites, verbatim (fixup-applied)

`0x2B0D4` (the spawn check, inside `0x2AE14`; ECX = rec, ESI = slot):

```
0x2B0D4  XOR EDX,EDX
0x2B0D6  MOV DL,byte ptr [ECX + 0x48]      ; type
0x2B0D9  LEA EBX,[EDX*4]
0x2B0E0  SUB EBX,EDX                       ; EBX = type*3
0x2B0E2  MOV EAX,ECX                       ; EAX = rec
0x2B0E4  SHL EBX,0x2                       ; EBX = type*12
0x2B0E7  MOV EDX,ESI                       ; EDX = slot
0x2B0E9  CALL dword ptr [EBX + 0xBB9DC]    ; cb1(rec, slot)
0x2B0EF  TEST AL,AL
0x2B0F1  JZ 0x2B104                        ; AL == 0 → visible
0x2B0F3  MOV DH,byte ptr [ECX + 0x28]
0x2B0F6  MOV byte ptr [ECX + 0x48],0x0     ; type = 0
0x2B0FA  OR DH,0x8
0x2B0FD  XOR EAX,EAX
0x2B0FF  MOV byte ptr [ECX + 0x28],DH      ; rec+0x28 |= 8 (dead)
0x2B102  JMP 0x2B144                       ; return 0
0x2B104  MOV AH,byte ptr [ECX + 0x2B]
0x2B107  OR AH,0x40
0x2B10A  MOV DL,byte ptr [ESP + 0x29]      ; a5's high byte
0x2B10E  MOV byte ptr [ECX + 0x2B],AH
0x2B111  TEST DL,0x4                        ; a5 & 0x400 (the child flag)
0x2B114  JNZ 0x2B119
0x2B116  MOV byte ptr [ECX + 0x4A],AL      ; AL == 0
0x2B119  CALL 0x1C390                      ; pop a render node
0x2B11E  TEST EAX,EAX
0x2B120  JZ 0x2B142
0x2B122  ...                               ; node+4 = pset, splice (0x1C3A0)
0x2B142  MOV EAX,ECX                       ; return rec
```

`0x2B185` (the teardown, inside `0x2B150`; EDX = rec):

```
0x2B155  MOV AH,byte ptr [EDX + 0x28]
0x2B158  OR AH,0x8
0x2B15B  MOV byte ptr [EDX + 0x28],AH      ; rec+0x28 |= 8
0x2B15E  MOV AX,word ptr [EDX + 0x2A]
0x2B162  XOR AL,AL
0x2B164  AND AH,0x40
0x2B167  AND EAX,0xFFFF
0x2B16C  JZ 0x2B18F                        ; only when rec+0x2A bit 14 set
0x2B16E  XOR EBX,EBX
0x2B170  MOV BL,byte ptr [EDX + 0x48]      ; type
0x2B173  LEA EAX,[EBX*4]
0x2B17A  SUB EAX,EBX
0x2B17C  LEA EBX,[EAX*4]                   ; EBX = type*12
0x2B183  MOV EAX,EDX                       ; EAX = rec
0x2B185  CALL dword ptr [EBX + 0xBB9E0]    ; cb2(rec)
0x2B18B  AND byte ptr [EDX + 0x2B],0xBF    ; rec+0x2B &= ~0x40
```

---

## 3. The porting plan (Step 3)

**Home:** `port/src/game/actors.c` (the dispatch, the callbacks, the
registration), one C function per original, header comment `/* 0xADDR — spec
section */`. No `render.c` change, no `fight.c` change, no new file, no new
dependency.

### 3.1 Piece 1 — the spawn dispatch (`actor_spawn`'s tail, `0x2B0D4`)

Replace the port's `if (DSD(DS_000BB9DC + type*0xC) == FN_0005D812)` test
(`actors.c:1443`) with the raw's call:

* `cb = DSD(0xBB9DC + (u32)DSB(rec+0x48) * 0xC)`; invoke it through
  `fn_resolve(cb)` with `(rec, slot)` — the same `fn_register`/`fn_resolve`
  mechanism the animation opcodes already use (`anim_indirect`, `actors.c:519`).
* If the returned byte is **non-zero, as a whole byte** (`0x2B0EF TEST AL,AL` /
  `0x2B0F1 JZ 0x2B104`): `DSB(rec+0x48) = 0`, `DSW(rec+0x28) |= 8`, return 0.
  The test is `AL != 0`, not "bit 0 set": `#12` (`0x3B9C4`) returns with
  `EAX = rec2` (`0x3B9CB MOV dword ptr [EAX+8],0` on a mem[] offset), so its low
  byte is arbitrary and a bit-0 test would call an even offset "visible". (For
  the 0 / `0xFF` returns the two tests coincide, which is why the port's current
  `== FN_0005D812` form has looked correct.) Only cb1's return is tested; the
  cb2 dispatch (`0x2B185`) discards it.
* Else: `DSB(rec+0x2b) |= 0x40`; `if ((a5 & 0x400) == 0) DSB(rec+0x4a) = 0`;
  `render_list_insert(pset)`; return `rec`.
* **Unregistered callback:** the port must not guess. The raw would call
  whatever address the table holds; the port's `fn_resolve` returns NULL. The
  faithful-and-safe form is: if `fn_resolve` is NULL, fall back to the *raw
  table's* address identity test (stub → visible) **and** mark the record dead
  otherwise — i.e. keep today's behaviour — but the 16 callbacks in §3.3 are
  registered, so no reachable type takes that path. A `/* TODO(verify) */` is
  *not* needed: the reachable types are enumerated in §3.4.

### 3.2 Piece 2 — the teardown dispatch (`set_dead`, `0x2B185`)

`set_dead` (`actors.c:1227`) currently carries a PORT note and skips the
callback. Port it: when `(DSW(rec+0x2a) >> 8 & 0x40) != 0`, call
`fn_resolve(DSD(0xBB9E0 + type*0xC))` with `rec` and then
`DSB(rec+0x2b) &= ~0x40`. The three existing writes (the `rec+0x28 |= 8` store,
the palette release and the render-list remove) stay in their current order
*after* the callback — the raw's order is: store `rec+0x28 |= 8` (0x2B155),
dispatch (0x2B185), clear 0x40 (0x2B18B), then the pset/palette work.

### 3.3 Piece 3 — the sixteen callbacks

The type table's distinct non-stub callbacks. `cb1` = `DSD(0xBB9DC + t*0xC)`
(the spawn check); `cb2` = `DSD(0xBB9E0 + t*0xC)` (the teardown). All are
register-argument (`EAX` = rec, `EDX` = slot for cb1; `EAX` = rec for cb2),
returning a byte in `AL`. **Every data address below is Ghidra's
(fixup-applied) value**; the pre-fixup forms differ by `0x80000` (§0.3.6).

| # | half | addr | types | size | body (raw, address-proved) |
|---|---|---|---|---|---|
| 1 | cb1 | `0x127C0` | 0x01 | 62 | pop the head of the `DS_000F0A78` list (unlink `0x249D0`); if empty return `0xFF`; else `rec2+8 = rec`, `rec+0x14 = rec2`, insert `rec2` before `0xF0AE0` (`0x249B0`), return 0. Proving: `0x127C4 MOV EDX,dword ptr [0x000F0A78]`, `0x127CA CMP EDX,0xF0A78`, `0x127EC MOV EAX,0xF0AE0`, `0x127F1 MOV [EBX+0x14],EDX`, `0x127E1 MOV EAX,0xFFFFFFFF` |
| 2 | cb1 | `0x198E8` | 0x06/0x26/0x27/0x28 | 62 | the same with `DS_00100C20` and `0x100C28` (`0x198EC MOV EDX,dword ptr [0x00100C20]`, `0x198F2 CMP EDX,0x100C20`, `0x19914 MOV EAX,0x100C28`) |
| 3 | cb1 | `0x28F64` | 0x19 | 182 | the same with `DS_00104888`/`0x104880` (`0x28F69`, `0x28F8F`), then `byte[rec2+0xC] = 0` (`0x28F9B`), `word[rec+0x32] = word[0xBD898]` (`0x28F9F`/`0x28FA8`), `byte[rec+0x29] \|= 0x10` (`0x28FAC`), `rec+0x14 = rec2` (`0x28FB2 MOV [EBX+0x14],ECX`, `EBX = rec`, `ECX = rec2`). **Tail** (`0x28FB5`–`0x29019`): `0x2BE5C(rec)` (`0x28FB5`); `ECX = rng_next(0x20)` (`0x28FBA`/`0x28FBF`); `EAX = rng_next(0x80)` (`0x28FC6`/`0x28FCB`); `ECX += 0x20` (`0x28FD4`); `EAX += 0xC0` (`0x28FD9`); `DX = word[rec+0x28] & 0x4000` (`0x28FD0`/`0x28FDE`/`0x28FE1`); if clear → `word[rec+0x34] = CX` (`0x28FF7`); if set → `word[rec+0x34] = -ECX` (`0x28FE9`–`0x28FED`) and `byte[rec+0x29] \|= 0x40` (`0x28FF1`); then `word[rec+0x44] = 0xC` (`0x28FFB`), `word[rec+0x36] = AX` (`0x29007`), `byte[DS_00104AE8] \|= 0x80` (`0x29001`/`0x2900B`/`0x29010`); return 0 (`0x2900E`). **Empty list → `0x28F86 MOV EAX,0xFFFFFFFF`, `0x28F8E RET` — the tail is not reached** |
| 4 | cb1 | `0x2901C` | 0x0A | 179 | as #3's head (`0x29053`–`0x2906A`); **tail** (`0x2906D`–`0x290CE`): `0x2BE5C(rec)` (`0x2906D`); `ECX = rng_next(0x80)` (`0x29072`/`0x29077`); `EAX = rng_next(0x80)` (`0x2907E`/`0x29083`); `EAX += 0xC0` (`0x2908E`); `DX = word[rec+0x28] & 0x4000`; **reversed polarity** (`0x2909C JNZ`): if set → `word[rec+0x34] = CX` (`0x290AC`); if clear → `word[rec+0x34] = -ECX` (`0x2909E`–`0x290A2`) and `byte[rec+0x29] \|= 0x40` (`0x290A6`); then `word[rec+0x44] = 0xC` (`0x290B0`), `word[rec+0x36] = AX` (`0x290BC`), `byte[DS_00104AE8] \|= 0x80` (`0x290C5`); return 0 (`0x290C3`). **No `ECX += 0x20`** — #3 and #4 differ in the first rng range (`0x20` vs `0x80`) and the branch polarity. **Empty list → `0x2903E MOV EAX,0xFFFFFFFF`, `0x29046 RET` — the tail is not reached** |
| 5 | cb1 | `0x48CD8` | 0x2D | 100 | the same with `DS_001082E0`/`0x108368` (`0x48CDD MOV EBX,dword ptr [0x001082E0]`, `0x48CE3 CMP EBX,0x1082E0`, `0x48D03 MOV EAX,0x108368`, `0x48D0A`), then `byte[DS_00104AE8] \|= 2` (`0x48D0F MOV AH,byte ptr [0x00104AE8]`, `0x48D22 OR AH,2`, `0x48D2A`), `byte[DS_00108398]++` (`0x48D19 MOV DL,byte ptr [0x00108398]`, `0x48D25 INC DL`, `0x48D30`), `byte[rec2+0xC] = 0` (`0x48D15`), `rec2+8 = rec` (`0x48D1F`) |
| 6 | cb1 | `0x412F0` | 0x16 | 9 | `word[rec+0x34] = 0x200; return 0` (`0x412F0`/`0x412F6`) |
| 7 | cb1 | `0x412FC` | **0x1B** | 9 | `word[rec+0x34] = 0x140; return 0` (`0x412FC`/`0x41302`) |
| 8 | cb2 | `0x12800` | 0x01 | 41 | if `rec+0x14 != 0`: unlink it (`0x249D0`), insert before `0xF0A78` (`0x12812 MOV EAX,0xF0A78`, `0x249B0`), `rec+0x14 = 0` (`0x12804`–`0x1281F`) |
| 9 | cb2 | `0x19928` | 0x06/0x26/0x27/0x28 | 45 | the same with `0x100C20` (`0x1993A MOV EAX,0x100C20`) and `byte[rec+0x48] = 0` (`0x1994E`) |
| 10 | cb2 | `0x290D0` | 0x0A/0x19 | 41 | the same with `0x104888` (`0x290E2 MOV EAX,0x104888`) |
| 11 | cb2 | `0x40684` | 0x1A | 45 | the same with `0x107EF8` (`0x40696 MOV EAX,0x107EF8`) and `byte[rec+0x48] = 0` (`0x406AA`) |
| 12 | cb2 | `0x3B9C4` | 0x02/0x03/0x04/0x05/0x08 | 19 | if `rec+0x14 != 0`: `[rec+0x14+8] = 0`, `byte[rec+0x14+0x64] = 0xFF` (`0x3B9C4`–`0x3B9D2`) |
| 13 | cb2 | `0x3FC90` | 0x10 | 29 | if `rec+0x14 != 0`: `dword[0x108080 + byte[rec+0x14+0x51]*4] = 0` (`0x3FCA3 MOV dword ptr [EAX*0x4 + 0x108080], EBX`) |
| 14 | cb2 | `0x3D784` | 0x09 | 10 | `EAX = 0x4F; jmp 0x2C3FC` (`0x3D784`/`0x3D789`) — a **tail call into the voice dispatcher** `FUN_0002c3fc` (1268 B, 206 callers), which the port carries as an out-of-scope stub (§4.3); its return (the table byte `DSB(0xBBdc8 + 0x4F*12) = 0x5`) is **not** tested by the cb2 dispatch |
| 15 | cb2 | `0x48D3C` | 0x2D | 67 | if `rec+0x14 != 0`: unlink it, insert before `0x1082E0` (`0x48D4E MOV EAX,0x1082E0`), `rec+0x14 = 0`, `byte[DS_00108398]--` (`0x48D5B`/`0x48D61`/`0x48D6A`), and if it underflows to `0xFF` (`0x48D70`/`0x48D73`), `byte[DS_00104AE8] &= ~2` (`0x48D75 AND byte ptr [0x00104AE8],0xFD`) |
| 16 | cb2 | `0x49444` | 0x20..0x25 | 97 | if `rec+0x14 != 0`: if `word[rec2+0x1C] & 2` (`0x49451`–`0x4945E`), `dword[0x10839C + ((s32)DSD(rec2+0x18) >> 16)*4] = 0` (`0x49468 MOV dword ptr [EAX*0x4 + 0x10839C], ECX`); if `DSD(rec2+0x10) != 0`, `set_dead` it (`0x49478 CALL 0x2B150`) and clear (`0x4947D`); unlink `rec+0x14`, insert before `0x1083C4` (`0x4948C MOV EAX,0x1083C4`), `rec+0x14 = 0` (`0x49499`) |

Registration: `fn_register(0x127C0, ...)` … `fn_register(0x49444, ...)` in
`actors_init` (`actors.c:95`, next to the existing three animation-code
registrations). `symbols.h` names `0x49444` (`FN_00049444`, symbols.h:2026) and
`0x2BE5C` (`FN_0002BE5C`, symbols.h:1670); the CSV lists both
(`00049444,97,FUN_00049444,2,3,ok`; `0002be5c,151,FUN_0002be5c,3,1,ok`). The
other 15 callbacks have no name in `symbols.h` and no CSV entry, so the plan
uses local `#define`s (or the literals with the address in the header comment)
for those — the repo's rule for a generator gap. **The CSV's callee edges are
incomplete for this group**: it records no caller of `0x2BE5C` although the raw
calls it at `0x28FB5`, `0x2906D` and `0x37FCF`, and no callee edge at all for
the 15 unlisted callbacks — the closure's edges for the 16 roots therefore come
from the disassembly (§4.1).

### 3.3b `0x2BE5C` — the mode-1 position/pset updater (151 B)

`FUN_0002be5c` (`prage.functions.csv`: 151 B, 3 callers, 1 callee). Called by
the two cb1 tails above (`0x28FB5` in #3, `0x2906D` in #4) and by an **unported
dispatcher at `0x37E40`** (`0x37FCF CALL 0x2BE5C`; Ghidra created no function
there and the CSV has no entry between `0x37D18` and `0x380C4`). It is the
mode-1 sibling of `actor_pset_point` (`0x2A690`) and calls `mode1_cursor`
(`0x2A620`); both are already ported in `actors.c`. **Home:**
`port/src/game/actors.c`. Body (Ghidra, fixup-applied):

* `ECX = pset = DSD(0x1014EC) + word[rec+0x56]*0x20` (`0x2BE64`–`0x2BE71`);
* `rec+0x1C = (DSD(0x000F0AEC) + 0x3BC0 − DSD(pset+8)) − ((s32)DSD(rec+0x30) >> 16)`
  (`0x2BE73`–`0x2BE92`);
* if `word[rec+0x28] & 0x1000` (`0x2BE95 AND AH,0x10`, `0x2BE9D JZ`):
  * `DSD(pset+0x14) = DSD(pset+8)` (`0x2BEA4`), `mode1_cursor(rec, pset)` (`0x2BEA9`);
  * `word[rec+0x46] = word[0x107900 + ((s32)DSD(rec+0x61) >> 24)*2]` (`0x2BEB4 MOV AX,word ptr [EAX*0x2 + 0x107900]`, `0x2BEBC`);
  * `rec+0x18 = DSD(pset+4) + ((s32)DSD(rec+0x44) >> 16)*2 − 0x2A00` (`0x2BEC0`–`0x2BED2`);
* else (`0x2BED7`): `rec+0x18 = DSD(0x000F0AF0) + (DSD(pset+4) − 0x2A00)` (`0x2BED7`–`0x2BEE7`);
* `byte[rec+0x29] &= ~0x20` (`0x2BEEA`); return (`0x2BEF2`).

**Wiring site:** the two cb1 tails (#3/#4). The `0x37E40` caller is unported and
stays out of scope — it is a *caller*, not a callee, so it does not enter the
gate's closure. **Unit-test value:** §7.5.

### 3.4 Which types are reachable (so nothing is left unregistered)

The demo run's measured spawn types (the `SPAWN` trace, all 18 225 spawns):
`0x00`, `0x01`, `0x1B`, `0x20`, `0x21`, `0x23`, `0x24`, `0x2E`, `0x2F`. The
types the *table* can select at all are `0x00`–`0x2F`; the 16 callbacks above
cover every non-stub entry, so every reachable type is registered. The types
whose cb1 is the stub (`0x00`, `0x02`–`0x05`, `0x07`–`0x09`, `0x0B`–`0x15`,
`0x17`, `0x18`, `0x1A`, `0x1C`–`0x1F`, `0x20`–`0x25`, `0x29`–`0x2C`, `0x2E`,
`0x2F`) keep the stub's `return 0` — register nothing, `fn_resolve` NULL, and
the port's fallback must return 0 for them. **The plan's fallback is therefore
not "dead": it is the raw table's identity test** (`cb == FN_0005D812` →
visible), which is exact for the stub and only reached for the stub.

### 3.5 The claim move (the plan must carry it)

The fix moves the front-end oracle's enforced window (§1.6/§6.2). Task 2 must,
per the design's Verification rule, halt-and-report and — because the move is
justified — update the claim in the same commit with the reason. The concrete
options are named in §6.2; the *default* (no move) is not available, because the
window is derived from the port's own dump and any correct arena render extends
it. This is a **cycle-level decision**, not a Task-2 code choice.

### 3.6 Wiring sites and what must NOT change

* The wiring site is `actor_spawn`'s tail only. `fight_scene_props` /
  `fight_scene_crowd` / the scene actors / `render_scroll_*` / `render_list` are
  correct as ported and must not be touched.
* `pset_write`'s parent-dead check and `actor_sync`'s `release_record` are
  correct; they are the *cascade*, not the cause.
* No change to `res.c`, `sprite.c`, `render.c` or any sprite id, layer, position
  or palette.

---

## 4. The size-gate verdicts (Step 4)

### 4.1 The method (cycle-3 §10.4, verbatim)

From `port/decomp/prage.calls.csv` (callee edges) + `prage.functions.csv`
(sizes). BFS from the roots, inclusive. **"new"** = in the closure AND not the
`0x6xxxx` runtime, not the RNG (`0x5Dxxx`), not the five known stubs
(`0x2C3FC`, `0x2EA64`, `0x62002`/`0x62003`/`0x6201B`), and **not named anywhere
in `port/src` except the generated `symbols.h`** (the naive heuristic: the
address appears as `0xADDR` in a `.c`/`.h`). **"true-new"** = naive + the roots
that are named-but-unported.

**Method note (raw):** **15 of the 16 callbacks are not in
`prage.functions.csv`** — Ghidra created no function at those addresses because
they are reachable only through the data table (`CALL dword ptr [EBX +
0xbb9dc]`). The exception is `0x49444` (`00049444,97,FUN_00049444,2,3,ok`),
whose flow-derived extent (97 B) and callees agree with the CSV, so the figures
are unaffected. For the other 15 the extents and callees were derived by
**control-flow-reachable disassembly** of the code object (all branches
followed, terminated at `RET`), and their indirect calls/jumps were checked
(none). The CSV is also missing the `0x2BE5C` callee edges (§3.3/§3.3b), which
the disassembly supplies. This is a correction to the method's inputs, not to
its rule.

### 4.2 Per group

| group | roots | closure | naive-new | true-new |
|---|---|---|---|---|
| **the dispatch (cb1 + cb2), the cycle's owner group** | the 16 callbacks | **25 f / 1 544 B** | **17 f / 1 148 B** | **17 f / 1 148 B** |
| cb1 only (the spawn check) | 7 | 12 f / 925 B | 8 f / 754 B | 8 f / 754 B |
| the minimal type-0x1B callback | `0x412FC` | 1 f / 9 B | 1 f / 9 B | 1 f / 9 B |

The 17 true-new members (sizes in bytes):

```
0x127C0  62   0x12800  41   0x198E8  62   0x19928  45   0x28F64 182
0x2901C 179   0x290D0  41   0x2BE5C 151   0x3B9C4  19   0x3D784  10
0x3FC90  29   0x40684  45   0x412F0   9   0x412FC   9   0x48CD8 100
0x48D3C  67   0x49444  97
```

The 8 already-ported closure members (`0x1C3D0`, `0x1C458`, `0x249B0`,
`0x249D0`, `0x2A620`, `0x2B150`, `0x33864`, `0x5D7DC`) are named in
`port/src`; `0x5D7DC` is additionally in the excluded `0x5Dxxx` range.

### 4.3 Verdicts

* **The dispatch group is under the gate by both measures** — 17 new functions
  (< the ~20-function line) and 1 148 B (< the ~4 KB line). No follow-on cycle;
  the cycle proceeds to Task 2.
* **The verdict's one sensitivity (raw correction to the method's inputs).** The
  method excludes `0x2C3FC` as one of "the five known stubs". The raw says it is
  **not a stub**: `FUN_0002c3fc`, **1268 B, 206 callers, 8 callees** — the voice
  dispatcher, which the *port* carries as an out-of-scope stub
  (`fidelity-gaps` §7.13; the port documents the same no-op at `fighter.c`'s
  `0x37D5C`). It enters this group only through `#14` (`0x3D784`'s tail call),
  a **cb2 whose return is discarded**. Measured both ways:
  * with `0x2C3FC` excluded (the method as written): **25 f / 1 544 B** closure,
    **17 f / 1 148 B** new — under the gate;
  * with `0x2C3FC` counted (the raw's truth): **190 f / 22 795 B** closure,
    **29 f / 2 752 B** new — **over the gate** (29 ≥ 20; the closure ≥ 4 KB).

  The plan keeps it excluded, and `#14` is ported as the raw's tail call into the
  port's *existing* `0x2C3FC` stub (a documented no-op, exactly as
  `fighter_37d18` already does at `0x37D5C`), so the voice subsystem is not
  pulled in. **If the cycle wants `#14` behaviourally faithful, the gate
  triggers and the voice dispatcher becomes a follow-on cycle** — the figure is
  recorded here so that decision is mechanical, not a judgement.
* The 16-callback set is the *whole* faithful dispatch closure (both halves).
  The `cb1`-only group (8 f / 754 B) and the minimal fix (1 f / 9 B) are smaller
  but leave the teardown half unported and the port's stub-only test in place for
  the other types; the plan ships the full 16.
* **A second group is *not* in this cycle's scope but is now *visible*:** the
  two frames the fix surfaces (832, 833, §6.2) belong to the loader/held-frame
  presentation and the state-9 animation — different owners. Their sizes are not
  bounded by this record; they are named with their owners, not sized, and the
  cycle must decide whether to absorb them (see §6.2).

---

## 5. The measurement plan (Step 5)

Task N+1 (the outcome) establishes the byte-exact with:

1. **Invocation.** `make demo-oracle` builds and runs
   `PR_FRONTEND_DET=/tmp/pr_frontend_dump PR_GAME_DIR=data/game/C
   ./build/run_tests` (the determinism driver writes
   `/tmp/pr_frontend_dump/run1/frame_NNNN.raw`, 320×200 RGB24) and then
   `python3 tools/title_compare.py --demo --capture data/title-captures/frontend
   --port /tmp/pr_frontend_dump/run1`.
2. **The cycle's own gate** — the 482/834 per-pixel compare:

```bash
python3 -c "a=open('/tmp/pr_frontend_dump/run1/frame_0482.raw','rb').read(); b=open('data/title-captures/frontend/frame_0834.raw','rb').read(); d=[i for i in range(len(a)) if a[i]!=b[i]]; print(len(d),'bytes',len({i//3 for i in d}),'px')"
```

   **Before:** 18 294 bytes / 6 194 px. **After (measured, temporary fix):**
   0 bytes / 0 px. Report both.
3. **The demo oracle's fallback** (`tools/title_compare.py:484`) is
   **report-only** and always exits 0. Measured post-fix: it still takes the
   fallback — the port's demo frames after the front-end window (491+) do not
   explain the capture's 843+ (the first residual there is the **T-rex pose**,
   §6.3), and the port's demo is shorter than the capture's. Acceptance #3: the
   fallback is a consequence, not a gate.
4. **The enforced ladder** — `make verify`. Measured post-fix: title
   `54 clean, 55 splice, 2 transition, 0 unexplained` and `54 clean, 57 splice,
   0`; attract `FIRST DIVERGENCE at capture frame 215` (498 B); smk `120/120` +
   `41/41` and `oracle C-vs-Python: 9866 writes byte-exact`; `symbols.h`
   byte-identical; **front-end `[560..842]` / 283 / 2 unexplained (832, 833)** —
   MOVED (§6.2).
5. **The instrumented differential** (Task 1's, reverted; *not* part of the
   shipped tree). The source, with `#include <stdio.h>`/`<stdlib.h>` added:

   ```c
   /* render.c, in render_list() before the clip computation */
   if (getenv("PR_RL_DUMP"))
       fprintf(stderr, "RL f=%u layer=%u pset=%08x id=%u pal=%08x px=%d py=%d x=%d y=%d W=%d H=%d\n",
               DSD(DS_0010150C), layer, pset, DSW(pset + 0x00), DSD(pset + 0x18),
               px, py, x, y, n.width, n.rows);
   /* render.c, in render_scroll_setup() */
   if (getenv("PR_RL_DUMP")) fprintf(stderr, "SCENE i=%u\n", i);
   /* actors.c, in actor_spawn() just before the type test */
   if (getenv("PR_RL_DUMP"))
       fprintf(stderr, "SPAWN desc=%08x type=%02x cb=%08x id=%u a5=%08x\n",
               (u32)((const u8 *)desc - mem), DSB(rec + 0x48),
               DSD(DS_000BB9DC + (u32)DSB(rec + 0x48) * 0xCu), DSW(pset), a5);
   ```

   Task 1 additionally printed the pset's sprite-descriptor handle and its 12
   bytes in the `RL` line, an `ACT` dump of the active list at frames 480–483,
   and (for the fix's measurement only) treated `tcb == 0x412FC / 0x412F0` as
   returning 0. Run with
   `PR_FRONTEND_DET=/tmp/pr_rl PR_RL_DUMP=1 PR_GAME_DIR=data/game/C
   ./build/run_tests`; the driver redirects the child's stderr into
   `/tmp/pr_rl/run1.log`.

---

## 6. The named gaps (Step 6)

### 6.1 Closed by this cycle's fix (measured)

* **The demo's state-7 arena backdrop** — the mountain silhouette. The port
  renders it; port 482–487 byte-match captures 834–839. The README's gap
  inventory entry and `pose-freeze-derivations.md` §11.4's residual are closed.

### 6.2 Exposed by the fix: captures 832 and 833 (the front-end window's 2 unexplained)

The front-end oracle's window is **derived from the port's own dump**, so
under-rendering *shrinks* it. Before the fix the window ended at 830 (the port
explained nothing past the state-9 hold); after the fix the port explains
834–842 and the window grows to 842, which brings the port's frames 480/481 into
the window's coverage — and they do not explain captures 832/833:

* **832**: the capture is black + the loader's `LOADING` text (166 non-black
  bytes); the port's 480 is the state-9 globe (40 716 non-black). **Owner:** the
  state-9 hold's animation (`fidelity-gaps` §7.6: "the state-9 screen's
  actor-animation advance") plus the loader presentation's *frame* — the same
  class as `demo-fight-closure` §9.4/§9.5's "load point inside the frame".
* **833**: the capture is the arena **dark** (a mid-fade presented DAC) with the
  `LOADING` text overlaid; the port's 481 is the arena at full palette with no
  `LOADING` text. **Owner:** the "presented DAC/palette state at the frame where
  the draw lands" gap (`demo-fight-closure` §9.5, `fidelity-gaps` §7.11).

Neither is this cycle's layer; both were carried in the design's Out list
(`831/832's held-frame presentation (§7.11)`). The fix **surfaces** them by
extending the window. The design's acceptance #2 assumed the claim would not
move; it does. The justified readings:

* the new window is *more* faithful — it explains 8 capture frames
  (834–842) the old window did not, at 0 bytes of error;
* the 2 unexplained are pre-existing gaps with named owners, not regressions;
* the cycle must either (a) also close 832/833 (out of the design's scope), or
  (b) update the front-end claim in the same commit with this record's reason
  (the design's rule), or (c) re-scope the acceptance. **This is a cycle-level
  decision for Task 2/N+1, flagged here.**

**Task 2 outcome (2026-09-24): the move was absorbed (option b).** The claim is
updated in `tools/title_compare.py` (832/833 allowed by name with this reason;
any other unexplained frame still fails), `port/tests/test_frontend.c` and this
cycle's spec. Measured on the full 16-callback port: window `[560..842]`, 283
frames, 2 unexplained (832, 833), `make verify` exit 0; title/attract/smk/
C-vs-Python unmoved. The full port also revives the five type-0x01 demo spawns;
measured, they change **0 of 1381** dumped frames (§0.3.12), so they move no
claim.

### 6.3 The next residual: capture 843 (outside the window)

Measured with the temporary fix: capture 843 is the first capture frame the
port does not explain (best byte splice port 490 @ 182 435 still differs by
16 064 bytes; the mask covers **only the T-rex's body** — a different animation
pose). Everything else matches: the mountains, temple, sky, sea, HUD, the
"NO ANIMALS WERE HARMED…" text and `CREDITS: 5`. **Owner:** the pose selector —
`combat-fidelity-derivations.md` §3's named gap ("the pose selector
(`0x34B14` handlers, `0x36870`, the pose family); a subsystem, not a single
derivable state"), i.e. the demo's state-7 pose advance. **Not this cycle's
layer; re-scoped with its size unknown (out of scope).**

### 6.4 Carried, untouched (each with its owner)

* The state-9 hold's animation (`fidelity-gaps` §7.6) — the 169-tick static
  hold; owner: the state-9 screen's actor-animation advance.
* The loader's presented DAC/palette state (`demo-fight-closure` §9.5) — owner:
  the render/palette-fidelity path.
* The interactive match (`fidelity-gaps` §7.12) — unowned by any cycle.
* The audio gaps (`fidelity-gaps` §7.13) — owner: the audio sub-project.
* The `0x38154` arm (`fidelity-gaps` §7.7) — no in-scope path reaches it.
* The type-0x01 spawns the port still kills (5 in the demo, measured; their
  cb1 `0x127C0` is in this cycle's 16, so Task 2 fixes them too). **This is a
  second live consequence of the same predicate**, not a separate gap: after
  Task 2 they spawn. Their effect on the demo frames after 489 is *not* measured
  here (they occur after the arena in the demo run) and Task 2 must measure it.

### 6.5 Named gaps of the derivation itself

* **The original's live-RAM render list was not obtained** (§1.7). The
  dosbox-x debugger is drivable (`-break-start`, `BPLM`, `MEMDUMPBIN`) but the
  required break was not confirmed in the session budget. The original's state
  is derived from the raw + the capture; the byte-exact capture match is the
  decisive measurement.
* **The pre-fixup/fixup distinction** cost two false readings (§0.3.6) and is
  recorded so the next task does not repeat it.

---

## 7. Unit-test values (the substitutions for Task 2)

Home: `port/tests/test_fight.c` (the crowd/spawn area; the design says no new
test file). Each assertion is raw-derived, seeded, and mutation-proven.

### 7.1 The type-0x1B callback (`0x412FC`) — the smallest failing unit

* **Seed:** a spawned record at any slot with `DSB(rec+0x48) = 0x1B` and
  `DSW(rec+0x34)` sentinel-seeded to a value that is *not* `0x140` (e.g.
  `0x7FFF`).
* **Call:** the callback for type `0x1B` — via the dispatch
  (`actor_spawn(desc = a 0x1B descriptor)`) or directly.
* **Assert:** `DSW(rec+0x34) == 0x140` (`0x412FC` `MOV word ptr [EAX+0x34],0x140`);
  the callback's `AL == 0`; `DSW(rec+0x28) & 8 == 0` and `DSB(rec+0x48) == 0x1B`
  (not killed, type not cleared); the pset is in the render list.
* **Mutation proof:** restore the port's stub-only test → the record is killed
  (`rec+0x28 & 8` set, `rec+0x48` cleared) and `DSW(rec+0x34) != 0x140`; the
  assertions fail.

### 7.2 The mountain chain (the cycle's own invariant)

* **Seed:** the demo's state-6 setup for scene 0 (the port's
  `render_scroll_setup(0)` + `fight_scene_props(0)`), or a synthetic
  `actor_spawn(mem + 0xC7850, −4448, 9362, 0, 0)`.
* **Assert (post-conditions, raw-derived):**
  * three records exist with pset ids **756**, **757**, **758**;
  * all three at layer **94** (`actor_pset_point`'s
    `0xF0 − (a3>>6) + (slot>>24)`, `0x2492 >> 6 = 146`);
  * the parent's `word[rec+0x34] == 0x140` (the type-0x1B callback ran);
  * child A's `DSD(rec+0x32) >> 16 == 0xA8` (168) and child B's `== 0x120` (288)
    — the stream's inline `a2` values at `0x0E8EB8` and `0x0E8EC2` (`DSW(0x0E8EB8)
    = 0x00A8`; `DSW(0x0E8EC2) = 0x0120`; the word between them, `0x0E8EC0`, is
    the second inline dword's high word `0x000C`);
  * the parent's `DSB(rec+0x4A) == 0` (the top-level spawn clears the parent slot
    at `0x2B116`) and each child's `DSB(rec+0x4A) == 7` (the parent slot);
  * all three nodes are in the render list (`render_list_count()` +3).
* **Mutation proof:** with the stub-only test, the three records are released
  (slots absent from the active list) and the assertions fail.

### 7.3 The teardown dispatch (`0x2B185`)

* **Seed:** a record with `DSB(rec+0x48) = 0x06` (Task 2 correction, §0.3.10:
  the type whose cb2 `0x19928` the assertions describe; `0x01`'s cb2 is
  `0x12800`), `DSW(rec+0x2a) |= 0x4000`, and `rec+0x14` = a record on the
  `DS_00100C20` list (a non-zero linked node: the `JZ 0x19931` returns before
  the assertions otherwise).
* **Assert:** the cb2 (`0x19928`) ran — `rec+0x14 == 0` and `DSB(rec+0x48) == 0`
  (`0x19947`/`0x1994E`) — and `DSB(rec+0x2b) & 0x40 == 0` (`0x2B18B`).
* **Mutation proof:** skip the dispatch (today's PORT note) → `rec+0x14`
  unchanged and `rec+0x48` still `0x06`; the assertions fail.

### 7.4 The dispatch's return contract (a table-driven case)

* **Seed:** one record per distinct callback address, each with the type byte
  the table maps to it (the §3.3 table); for the five cb1s that pop a list
  (`#1`–`#5`) the corresponding head must be seeded non-empty
  (`[0xF0A78] != 0xF0A78`, `[0x100C20] != 0x100C20`, `[0x104888] != 0x104888`,
  `[0x1082E0] != 0x1082E0`) or the callback returns `0xFFFFFFFF` early
  (`0x127E1`, `0x19909`, `0x28F86`, `0x2903E`, `0x48CFA`) and the visibility
  assertion tests the wrong path.
* **Assert:** for the 16 registered types the callback's `AL` decides
  visibility; for every stub type the record is visible (`rec+0x28 & 8 == 0`).
* **Mutation proof:** swap any registered callback for the stub → the affected
  type's visibility flips and the assertion fails.

### 7.5 The `#3`/`#4` tails and `0x2BE5C` (§3.3/§3.3b)

* **Seed:** a record with `rec+0x28` bit 14 **clear** and a sentinel
  `word[rec+0x34] = 0x7FFF`; `DS_00104AE8` seeded to a value with bit 7 clear;
  `word[rec+0x56]` = a slot whose pset is seeded; **and the head's list seeded
  non-empty with a node actually linked into it** — `[0x104888]` must not be
  its own sentinel (`0x104888`) and must be a node `0x249D0` can unlink, or the
  head returns `0xFFFFFFFF` at `0x28F86`/`0x28F8E` (resp. `0x2903E`/`0x29046`)
  **before** the tail at `0x28FB5`/`0x2906D`, and none of the assertions below is
  reached. `rec+0x14` is *written* by the head (`0x28FB2`/`0x2906A`,
  `rec+0x14 = rec2`), never read on that path, so seeding it does not reach the
  tail. The path, per the raw: `0x28F75 JNZ` (non-sentinel) → `0x28F7D` unlink →
  `0x28F84 JNZ` (non-zero) → `0x28F8F`–`0x28FB2` → `0x28FB5 CALL 0x2BE5C`.
* **Assert (`#3`, `0x28F64`, bit 14 clear):** `word[rec+0x34] == rng_next(0x20)
  + 0x20` (the first draw, **before** the second — the order is RNG-visible);
  `word[rec+0x44] == 0xC` (`0x28FFB`); `word[rec+0x36] == rng_next(0x80) + 0xC0`
  (`0x29007`); `byte[DS_00104AE8] & 0x80` (`0x29010`); `byte[rec+0x29] & 0x40
  == 0` (the clear-arm does not set it).
* **Assert (`#3`, bit 14 set):** `word[rec+0x34] == -(rng_next(0x20) + 0x20)`
  (`0x28FEB`/`0x28FED`) and `byte[rec+0x29] & 0x40` (`0x28FF1`).
* **Assert (`#4`, `0x2901C`, polarity reversed):** bit 14 **set** →
  `word[rec+0x34] == rng_next(0x80)` (`0x290AC`, no `+0x20`); bit 14 **clear** →
  `word[rec+0x34] == -rng_next(0x80)` and `byte[rec+0x29] & 0x40` (`0x290A6`).
  Both draws are `rng_next(0x80)` (`0x29072`/`0x2907E`).
* **Mutation proof:** swap the two arms' polarity in `#4` (or add `+0x20` to its
  first draw) → the assertion fails; seed bit 14 the other way and the same
  assertion flips.
* **`0x2BE5C` (§3.3b):** seed `DS_00104AE8 = 0`, `pset+4 = X`, `DS_000F0AF0 = Y`,
  `rec+0x28` bit 12 clear → `DSD(rec+0x18) == Y + X − 0x2A00` (`0x2BED7`–`0x2BEE7`)
  and `byte[rec+0x29] & 0x20 == 0` (`0x2BEEA`). **Mutation proof:** set
  `rec+0x28` bit 12 → the mode-1 arm runs (`0x2BEA9` `mode1_cursor`,
  `0x2BEBC`, `0x2BED2`) and `DSD(rec+0x18)` no longer equals `Y + X − 0x2A00`.
  **Task 2 correction (§0.3.11):** in the mode-1 arm `mode1_cursor` writes
  `rec+0x64` (the ramp index) and `0x2BEBC` writes `rec+0x46` (the dword at
  `rec+0x44`'s high word) before `0x2BEC0` reads it, so the mode-1 test must
  seed the `0x107900` ramp entry at the index `mode1_cursor` produces.

---

## 8. Provenance

### 8.1 Ghidra (project `rage`, program `/PRAGE.EXE`; addresses are linear)

| call | what it proved |
|---|---|
| `decompile_function 0x2AE14` | the walk, the pset write, the `0x2B0D4` type check, the `a5 & 0x400` child path |
| `disassemble_function 0x2B0D4` | the check's instruction stream (the `CALL [EBX+0xbb9dc]`, the dead path, the visible path) |
| `disassemble_bytes 0x2B17F` | `CALL dword ptr [EBX + 0xbb9e0]` (fixup-applied) |
| `disassemble_bytes 0x48D94` | `JMP dword ptr CS:[EAX*0x4 + 0x48D80]` (fixup-applied; the pre-fixup read said `0x38D80`) |
| `disassemble_bytes 0x412EC` | `0x412F0`/`0x412FC` (`MOV word [EAX+0x34], 0x200`/`0x140`, `XOR EAX,EAX`, `RET`) |
| `disassemble_bytes 0x127C0`, `0x198E8`, `0x28F64`, `0x2901C`, `0x3FC90`, `0x3D784`, `0x3B9C4` | the callback bodies of §3.3 |
| `disassemble_bytes 0x12800`, `0x19928`, `0x290D0`, `0x40684`, `0x48CD8`, `0x48D3C`, `0x49444` | the remaining callback bodies and the corrected data addresses of §3.3 (fix-round 1) |
| `disassemble_bytes 0x2BE5C`, `0x37FC0` | `0x2BE5C`'s body (§3.3b) and its third caller at `0x37FCF` |
| `read_memory 0x0E8EB2` | the animation stream's words (the `a2` values at `+0x06`/`+0x10`, the stop word `0x02F4` at `+0x14`) |
| `read_memory 0xBDF7C`, `0xBDF9C`, `0xBDE50`, `0xBDEF0`, `0xA8B30`, `0xB3AA0`, `0xBBB1C`, `0xBBC18`, `0xC77EC`, `0xC7850`, `0xC7864`, `0xC7878`, `0xC7F78`, `0xBB9D8`, `0xBB9E0`, `0xBBD98`, `0xBBDA8`, `0x48D80` | the descriptor/tables/stream values of §2 |

### 8.2 The raw file and the local disassembler

* `port/tests/ghidra_data.bin` (the fixup-applied data object, 0x8B0D0 bytes)
  was the Python-side source for the tables; every value it gave was re-checked
  against a Ghidra `read_memory`.
* The code object was reconstructed from `data/game/C/PRAGE.EXE`'s LE page map
  (object 0, 100 pages, page size 0x1000) with Python + `capstone 5.0.7` for the
  callback extents and the closure; **fixup-bearing displacements from this
  source are wrong by construction** (§0.3.6) and were re-read from Ghidra.
* `port/decomp/prage.functions.csv` / `prage.calls.csv` supplied the closure's
  sizes and edges for the CSV-listed functions.

### 8.3 dosbox-x

* `/opt/homebrew/bin/dosbox-x` 2026.08.31, `-break-start -conf <config>` driven
  over a pty (`pty.openpty` + `select`). The debugger's ncurses UI responds to
  `BPLM <linear>`, `RUN`, `BPLIST`, `MEMDUMPBIN <addr> <len>`, `EV`.
* One 16 MiB dump was obtained and verified (the data object at `0x266000`, code
  at `0x201000`, `DSD(0xBB9D8) = 0x2A0B74`). The arena-frame break
  (`DS_00105C08` = linear `0x2EBC08`) was not confirmed in the budget; **no
  game-state dump of the arena was obtained** (§1.7).
* The `[dosbox] memory file` setting is written at debugger start, not at
  `MEMDUMPBIN`; `MEMDUMPBIN` writes `memdump.bin` in the process's cwd.

### 8.4 The port's runs

| run | purpose | result |
|---|---|---|
| `PR_FRONTEND_DET=/tmp/pr_rl PR_RL_DUMP=1 ./build/run_tests` (pre-fix) | the differential | 80 nodes at 482, no 756/757/758, the children spawned, slots 7/8/9 released |
| the same with the temporary dispatch fix | the fix's effect | 83 nodes, 482–487 byte-exact, the front-end window moved |
| `make verify` (post-fix) | the enforced claims | title/attract/smk/oracle-C unmoved; front-end moved |
| `make demo-oracle` (post-fix) | the report-only oracle | still the `res is None` fallback (the T-rex pose at 843) |

**Instrumentation:** `port/src/platform/render.c` (`render_list`'s node line, a
`render_list_contains` helper, a four-frame `ACT` list dump, `render_scroll_setup`
scene line) and `port/src/game/actors.c` (the `SPAWN` line and the temporary
`tcb == 0x412FC / 0x412F0` dispatch). All reverted; `git status --short` is
clean except this record.
