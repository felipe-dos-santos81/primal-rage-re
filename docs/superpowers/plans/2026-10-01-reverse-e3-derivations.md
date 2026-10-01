# Reverse completion E3: call stubs for the differential harness (record)

**Scope.** The call-stub extension of the differential-emulation harness that E1 built
(`2026-09-30-reverse-completion-e1-derivations.md`) and E2 asked for (`2026-10-01-reverse-e2-derivations.md`
§E2.6, §E2.10). It implements spec `2026-09-30-reverse-completion-design.md` §5.1(3) ("the ordered list of
outgoing calls with arguments") and §5.2 ("A callee is stubbed identically on both sides and its call
recorded ... each callee is proven by its own check ... The port side uses the existing `fn_resolve` miss log
for the call record ... not exercisable ... becomes a named gap naming the blocking instruction"). It gates
track P: the 98 unported targets E2 counted (callbacks 26, finishers 6, animation targets 63, other 3) and
the six span dispatchers of table `0x80C8C` (E2 decision D2). Plan: `2026-10-01-reverse-e3-call-stubs.md`.

**Status of the numbers.** Everything below was measured by the planner on 2026-10-01 in two scratch copies of
the tree at `main` `a935e91`: a prototype (every change of the plan at once, `make verify` run on it) and a
replay (the plan's eight tasks applied in order, each red and green run and each mutation quoted in the plan
taken from it; `make verify` run again on the final state), and a second replay that ran the plan's own check
commands step by step and printed every expected block the plan quotes. **The image** is the one E2 used: `build/diffrun
--exe data/game/C/PRAGE.EXE --image-out FILE` at `a935e91`, 1 028 304 bytes from `0x10000`, sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947`. Every address and instruction below is capstone 5.0.7 over that
image (fixups applied). Ghidra was not consulted: the raw bytes decide every fact in this record.
`unicorn` 2.1.4 runs the original side.

---

## §E3.1 What E1 and E2 leave open

- E1 §E.6.1: a call outside the case's allow-list stops the run (`unmodeled`); no stubs. §E.6.3: stack
  arguments (`ret N`) have no binding form. §E.6.8: the call list of spec §5.1(3) is not compared.
- E2 §E2.6: of the 329 unported targets, 89 are `stubs` (the tree contains something the emulator cannot
  run), 85 of them outside the span code; each has a clean own body, the blocker sits in a ported callee's
  tree (the animation dispatcher's `call [0x105BD4]` at `0x2B56D` in `0x2B2A0`, 58 rows; `actor_spawn`'s
  `call [ebx+0xBB9DC]` at `0x2B0E9` in `0x2AE14`, 22 rows; others below in §E3.7).
- **Direct callees of the 85 rows, re-counted with E1's `static_scan`** (scratch `rows.py`, the rows of
  `2026-10-01-reverse-e2-triage.md` whose E1 column starts `stubs`, ported `no`, batch not `span-writers`):
  35 distinct direct `call` targets, 8 of them unported (`0x1BBAC 0x22404 0x23960 0x2BDB8 0x2BDE8 0x2BEF4
  0x2D62C 0x3A9D8`, E2's eight), 27 ported. By rows: `2C3FC` 35, `2BC30` 31, `2AE14` 22, `3C4CC` 21, `339AC`
  11, `1A570` 7, `33950` 6, `39834` 5, `2A17C` 4, `3A95C` 4, `3C190` 3, then 24 callees with 1 or 2 rows. The
  top four equal E2's figures. E2's "43 distinct (35 ported)" came from E2's extended scan (through bounded
  switches, §E2.2) and is not re-derived here; the difference is the scan's extent, not a disagreement about
  any call.

## §E3.2 The original side: interception at the callee's entry

**Rule.** `diff_emu.run_original(..., calls=(Call, ...))`. In the code hook, when execution arrives at an
address of the call set and the instruction executed just before was a `call` or a `jmp` (any form), the
arrival is a call: the hook records `(address, values)`, `values` being the registers and stack slots the
`Call` names, read at that moment (`s0` is the dword at `[esp+4]`, above the return address). For a **stub**
it then applies the declared writes, sets EAX, and returns: `ESP += 4 + pop`, `EIP = [old ESP]`; the
callee's bytes never run. For a **real** call it records and lets the bytes run.

**Why at the entry, not at the call site.** One rule covers the four ways the original reaches a callee: a
direct `call rel32`, an indirect `call` through a register or memory (`0x2B56D` `call dword ptr
[0x105bd4]`, `0x2B0E9` `call dword ptr [ebx + 0xbb9dc]`), and a tail `jmp`. The caller's own instruction is
never decoded for its target. A fall-through into a call-set address is not a call (tested).

**Indirect calls are resolved at run time.** At an indirect `call`, the hook computes its target from the
registers and memory as they are then (`[base + index*scale + disp]` or the register); a target in the
allow-list or the call set proceeds, anything else stops the run as `indirect call to 0x<target> at
0x<site>`. With neither a call set nor an allow-list the run stops exactly as E1 did (`indirect call at
0x<site>`, E1's test unchanged). A direct call to a call-set target is accepted at the call site, so E1's
"call outside the allow-list" check still applies to everything else.

**Unicorn.** Writing `EIP` and `ESP` from inside the `UC_HOOK_CODE` hook redirects execution in unicorn
2.1.4: the stub tests return to the caller and finish `ok` (Task 3's `test_a_stubbed_call_is_recorded_and_its
_bytes_do_not_run`, `test_a_stub_pops_its_stack_arguments_and_records_them`, `test_a_tail_jump_to_a_stubbed
_entry_returns_to_the_caller`). A stub write uses `mem_write`, which fires no write hook, so the hook records
the bytes' old values itself (mutation P7 fails a test).

## §E3.3 The port side: the `PR_SEAM` call seam

**Requirement.** The port's C functions call each other directly. A call into the call set must be
recorded with its arguments, in order, interleaved correctly with the calls of real callees, and a stubbed
callee must not run. Static functions are among the callees (`0x3C480` is `static void hit_anim_start_a` in
`fighter.c`; `0x3C4CC` was `static void hit_anim_start_b`).

**Alternatives, measured and rejected.**
- Link-time wrapping: this host's linker has none. `clang w.c -Wl,--wrap=foo` and `-Wl,-wrap,foo` both fail
  with `ld: unknown options` (`ld-27037.1`). A second strong definition of a symbol in the same static
  archive is a duplicate-symbol error.
- Renaming with `-D` per translation unit, or `llvm-objcopy --redefine-sym` (present only as Homebrew's
  `/opt/homebrew/opt/llvm/bin/llvm-objcopy`, not a repo dependency): neither reaches a call inside the
  callee's own translation unit nor a `static` callee.
- A second core library compiled with a define (`-DPR_DIFF_SEAMS`) so the shipped object holds no seam:
  cheap (a clean build of everything takes 2.31 s at `-j8`, 4.87 s at `-j1`), but then the object code
  `diffrun` verifies is not the object code the game ships, and `run_tests` (linked with `prage_core`) could
  not test the seam with `CHECK`. Kept as decision D1's alternative.

**Chosen: a NULL-by-default hook in `prage_core`.** `mem.h` declares `pr_seam_fn pr_seam` and two macros;
`mem.c` defines `pr_seam` (NULL). A seamed function's first statement is `PR_SEAM(0xADDR, args...)` (void) or
`PR_SEAM_RET(0xADDR, args...)`: when `pr_seam` is set it passes the original address and the C arguments, in
the C signature's order; a hook answer of 1 returns at once (`PR_SEAM_RET` returns the hook's EAX), 0 runs
the body. Only `build/diffrun` sets the hook (`port/tests/diff_runner.c`), and `run_tests` sets and clears it
inside `test_call_seam`. Precedent for an inert test seam in `prage_core`: `sound_voice`'s voice log
(`flow.c`, "PORT: a test seam, not original state"), `res_set_screen_hook` (`res.c:82`), the `fn_resolve`
miss log (`mem.c`). Shipped behaviour: with the hook NULL every seam is one untaken branch; the prototype's
`make verify` exited 0 with the 45 oracle lines equal to `oracle-lines-base.txt` and the `make audio-render`
WAV identical to `before-t2.wav` (§E3.11).

**Indirect calls on the port side.** The port makes them through `fn_resolve(addr)` (`anim_indirect` in
`actors.c`, `actor_spawn`'s type callbacks, the fighter slot callbacks). A registered target that carries a
seam records itself with its arguments. An **unregistered** target is a miss: the port skips the call. Spec
§5.2 names the miss log as the port's call record, but the log cannot be one: it keeps distinct
`(address, caller)` pairs with hit counts (`fn_miss[]` in `mem.c`), with no order and no arguments. So
`fn_resolve_from` also reports a miss (non-zero address) to `pr_seam` with no arguments, whether or not the log
is armed: the call is recorded in order, by address only (named limit §E3.8). A registered target without a
seam records nothing on the port side while the original records it: the comparison reports
`call #i: original ..., port ...` (fail-closed; tested on `0x33950`, which has no seam).

## §E3.4 The call set: what a spec declares, and why it is sound

A `Spec` gains `calls`, a tuple of `E.Call(addr, args, mode, eax, writes, pop)`:

| mode | original side | port side | recorded |
|---|---|---|---|
| allow (E1's `allow_calls`) | the callee's bytes run | the C callee runs | no |
| `real` | the bytes run | the C body runs (hook answers 0) | yes |
| `stub` | bytes skipped: `writes` applied, EAX = `eax`, return popping `pop` | body skipped: same `writes`, `PR_SEAM_RET` returns `eax` | yes |

`args` names the original's registers (`eax ebx ecx edx esi edi ebp`) or stack slots (`s0`-`s3`) in the
**order of the port's C signature**; that order is what both sides print. `writes` are `(base, offset,
bytes)`: base `None` is an absolute address, an integer `k` is argument `k`'s value; a write outside the image
stops the original (`unmodeled`) and is a port error. `allow` and `calls` may not share an address (refused).

**Soundness, per spec §5.2.** Verifying F with callee G stubbed proves F's C equivalent to F's bytes for every
case, **given** G behaves as declared: G is called with the same arguments in the same order on both sides,
and returns the declared EAX after the declared writes on both sides. The assumption is discharged only by G's
own row: the table's last column names each callee with its own verdict, and the counter line reports how
many VERIFIED rows have every callee VERIFIED ("8/13 with every callee VERIFIED" after E3). A stub's EAX and
writes are **harness values**, chosen per spec and stated there, never a claim about G (for example `VOICE`
returns 1, the AL `0x2C3FC` returns for a played id per the comment on `sound_voice` in `flow.c`). Allow-mode
is for callees whose arguments cannot be compared across the two sides, today the two context builders
`0x33950` and `0x339AC`, whose EAX is a stack buffer in every original caller: they are run, not recorded, and
proven by their own rows (§E3.6).

## §E3.5 Stack arguments and `ret N`

Cases name stack arguments as `regs["s0"]`..`regs["s3"]` (`diff_emu.STACK_ARGS`); the original side writes
them above the sentinel return address, `diffrun` reads `reg s0 ...` lines into four more binding slots. A
stub's `pop` is the callee's own `ret N`, read from the bytes. Every `ret` in each stubbed callee's
`static_scan`:

| callee | `ret` forms in its scan | where | arguments, by the bytes |
|---|---|---|---|
| `0x2C3FC` | `ret` only | - | EAX = the voice id (`mov edx,eax; test eax,eax` at `0x2C3FF`) |
| `0x2BC30` | `ret 4` only | `0x2BCEF` | EAX = rec (`mov ecx,eax` `0x2BC36`), EDX = stream (`mov [eax+8],edx` `0x2BC52`), `s0` = frame (`push 0x40400000` before each call, e.g. `0x45885`) |
| `0x3C4CC` | `ret 4` only | `0x3C51C` | EAX = rec (to `0x339AC` as EDX, `0x3C4D2`), EDX = stream (`mov ebx,edx` `0x3C4D0`, back to EDX at `0x3C4FC`/`0x3C50D`), `s0` (`push dword ptr [esp+0x20]` `0x3C4FE`) |
| `0x3C480` | `ret 4` only | `0x3C4C8` | EAX = rec (`mov ecx,eax` `0x3C486`), EDX = stream (`mov esi,edx` `0x3C488`), `s0` (`push dword ptr [esp+0x28]` `0x3C4A4`) |
| `0x2AE14` | `ret 4` only | `0x2B14A` | EAX = desc (`mov ebp,eax` `0x2AE1A`), EDX (`[esp+4]` `0x2AE1C`), EBX (`[esp]` `0x2AE20`), ECX (`[esp+8]` `0x2AE23`), `s0` (`[esp+0x28]` `0x2AE27`); the port's `actor_spawn(desc, a2, a3, a4, a5)` maps them EAX, EDX, ECX, EBX, `s0` (actors.c's header, record `2026-09-17-actor-system-args.md`) |

So `ANIM_BEGIN`, `HIT_B`, `HIT_A` and `SPAWN` declare `pop=4` and `VOICE` `pop=0`. A wrong `pop` makes the
caller return through a stack argument: mutation P2 fails the stub tests and every real spec that stubs a
`ret 4` callee.

## §E3.6 The worked batch

Four E2 `stubs` rows that are already ported (their callees are the most frequent: voice `0x2C3FC`,
`0x2BC30`, `0x2AE14`, `0x3C4CC`), plus three of their callees by their own checks. Porting unported rows is
track P's (decision D2). Each C function is reached by a binding in `diff_runner.c`; the animation targets are
`static` and registered, so `diffrun` registers the port's code pointers once, at every `fn_register` call site of `port/src` (the
list in §E3.8: `actors_init`, after pointing the two pool pointers at a scratch range for the call and
restoring them, then `effects_init`'s `camera_register`, `attract_scene_tick`'s `attract_register` and
`svcmenu_register`; the image is checked unchanged or restored from the dump).

| row (E2 class, evidence) | C function | calls | allow | cases | blocks | EAX mask, evidence |
|---|---|---|---|---|---|---|
| `0x23130` (move-callback, `dword A3CA8`, char 1 reaction 0x20; voice site `0x2316B`) | `fighter_23130` (EAX slot, EDX rec, EBX side) | `HIT_B` stub, `VOICE` stub | `0x33950` | 2 (side 0, 1; slot `+0x52..+0x54` and `+0x0C` seeded) | 1/1 | `0xFF`: `mov al,1` at `0x23170`; the C returns 1. The second case (`v1`) cannot differ from the first: EBX (the side) only feeds `0x33950`'s stack buffer, which is allow-mode and not compared; it is kept as a second run of the same blocks, not as extra evidence |
| `0x45878` (move-callback, `dword A4BF8`, char 4 reaction 0x24) | `fighter_45878` (EAX slot, EDX rec) | `ANIM_BEGIN` stub | - | 2 (`+0x42` 0x00 and 0xFB; every written field seeded) | 1/1 | `0`: the C is void; its callers are `0x35045` in `0x34E2C` ("whose AL is ignored": `actors.c`'s reaction-callback wrappers, records §42-A/§43-C; not re-derived here) and `0x46148` in `0x46138`, whose EAX returns to `0x2B2A0`'s indirect call sites, which overwrite EAX at once (`mov eax,ecx` at `0x2B575`, `0x2B59A`, `0x2B5F0`; `diff_runner.c`'s 3640c note) |
| `0x10FA8` (anim-target, `dword E88D4` after `D100`) | `anim_code_10FA8` via `fn_resolve` (rec, arg) | `SPAWN` stub | - | 2 (no registers; EAX, EDX set) | 1/1 | `0`: an animation target; no direct caller (a `call rel32` scan finds 0), reached from `0x2B2A0`'s indirect sites |
| `0x3E4E4` (anim-target, `dword E7BF4` after `D500`) | `anim_code_3E4E4` via `fn_resolve` | `ANIM_BEGIN` stub | - | 2 (`rec+0x14` 0 and a slot; `+0x36`, `+0x44`, `slot+0x57` seeded) | 3/3 | `0`, as `0x10FA8` (no direct caller) |
| callee `0x33950` | `fighter_ctx_same(out, side)`; binding copies `out` to `mem[EAX]` | - | - | 2 (side 0, 1; 24 bytes at EAX seeded `0xAA`; slot pointers seeded) | 1/1 | `0`: the C is void (the original leaves EAX = `out`) |
| callee `0x339AC` | `hit_anim_ctx(out, rec)`, the same binding form | - | - | 2 (`rec+0x51` 0, 1; `+0x50` the other value) | 1/1 | `0`, as `0x33950` |
| callee `0x3C4CC` | `hit_anim_start_b(rec, stream, frame)` (made non-static) | `ANIM_BEGIN` stub, `HIT_A` stub | `0x339AC` | 26: every slot state `0..0x16` and `0x7F`, `0x80`, `0xFF`, side = state parity for `0..0x16` (Task 8 widened Task 7's 7 cases, so the dispatch set `{0,1,2,5,0xE,0x15}` -> `0x2BC30`, else `0x3C480` is pinned value by value) | 10/10 | `0`: void; EAX at return is the stubbed callee's (named limit §E3.8) |
| `0x1B890` (E2's `in` blocker, host-owned) | none: a named gap | - | - | 1 | 1/9 | `NAMED_GAP`: `in at 0x1B899` |

**Result** (`python3 tools/diff_verify.py --self-check` on the replay's final state, 3.61 s real):

```
| function | original | cases | blocks hit/total | verdict | callees (each by its own check) |
|---|---|---|---|---|---|
| rng_next | 0x5D7DC | 4 | 1/1 | VERIFIED | - |
| fighter_slot_flag | 0x3C570 | 5 | 3/3 | VERIFIED | - |
| config_credit_spend | 0x2CA7C | 5 | 7/7 | VERIFIED | - |
| config_codeword_len | 0x2D4B4 | 5 | 4/4 | VERIFIED | - |
| fighter_3640c | 0x3640C | 2 | 3/3 | VERIFIED | - |
| fighter_37dcc | 0x37DCC | 2 | 1/1 | VERIFIED | - |
| fighter_23130 | 0x23130 | 2 | 1/1 | VERIFIED | 2C3FC stub unverified, 33950 allow VERIFIED, 3C4CC stub VERIFIED |
| fighter_45878 | 0x45878 | 2 | 1/1 | VERIFIED | 2BC30 stub unverified |
| anim_10fa8 | 0x10FA8 | 2 | 1/1 | VERIFIED | 2AE14 stub unverified |
| anim_3e4e4 | 0x3E4E4 | 2 | 3/3 | VERIFIED | 2BC30 stub unverified |
| fighter_ctx_same | 0x33950 | 2 | 1/1 | VERIFIED | - |
| hit_anim_ctx | 0x339AC | 2 | 1/1 | VERIFIED | - |
| hit_anim_start_b | 0x3C4CC | 26 | 10/10 | VERIFIED | 2BC30 stub unverified, 339AC allow VERIFIED, 3C480 stub unverified |
| host_1b890 | 0x1B890 | 1 | 1/9 | NAMED_GAP (in at 0x1B899) | - |
```

followed by the 16 mutant rows (all `MISMATCH`; Task 8 added `hit_anim_start_b@set`) and the counter line:

```
diff-verify: 13/13 functions VERIFIED; 16/16 mutants detected; 1 named gaps; 8/13 with every callee VERIFIED. Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
```

E1's six rows and seven mutants are unchanged apart from the new last column (`-`).

**Callees without a row, and the callee column's depth.** Four stubbed callees have no row of their own and
read `unverified` in the last column: `0x2C3FC` (voice), `0x2BC30` (animation begin), `0x2AE14` (`actor_spawn`)
and `0x3C480` (`hit_anim_start_a`); P verifies them. The column is **one level deep**: it names the callees
of the row's own function, each with the verdict of its own row, and says nothing of that callee's callees
(a row whose callee is itself VERIFIED with an `unverified` stub of its own, such as `0x23130` -> `0x3C4CC`
-> `0x2BC30`/`0x3C480`, shows only `3C4CC stub VERIFIED`). "8/13 with every callee VERIFIED" counts that one level.

**Seeds are hand pokes, not snapshots.** Every case's `pokes` are chosen bytes written into the image's zero
BSS (`E3_SLOT`, `E3_REC`, ... at `0x10A200`..), not state captured from a run. Spec
`2026-09-30-reverse-completion-design.md` §5.3 asks for real snapshots; this is a named deviation, the same
one E1 made (E1 §E.7), and the claim stays "equivalence on the exercised blocks and inputs only".

**Unhit blocks:** none in the seven functions (every leader of each `static_scan` executed). `0x1B890`'s eight
unhit blocks lie after the `in` the gap names.

**The nine new mutants and what catches each** (Task 7's `test_each_e3_mutant_is_caught_by_what_it_breaks`):
`fighter_23130@voice` (voice id `0x7D`) and `@novoice` (no voice call) only by `call #1`;
`fighter_45878@mutant` (frame `0x40000000`), `anim_10fa8@mutant` (layer `0xE5`) only by `call #0`;
`hit_anim_start_b@mutant` (always `0x2BC30`) only by `call #0`, and only on the cases whose state is outside
`{0,1,2,5,0xE,0x15}` (the `0x3C480` arm: 20 of the 26); `hit_anim_start_b@set` (state 0 leaves the `0x2BC30` set) only by `call #0` on case
`h0`; `anim_3e4e4@mutant` (skips the `+0x14` test) by `call #0` and bytes; `fighter_ctx_same@mutant`
and `hit_anim_ctx@mutant` by bytes. Six of them are invisible to E1's EAX-and-bytes comparison. Ported-source mutations of the dispatch set itself (Task 8, scratch script, `fighter.c` restored after each) are each reported `MISMATCH` on the real row: dropping any one of the six members, adding `3`, adding `0x80`, and replacing the set by `st <= 1u`.

**Two observations the batch makes on real code.** (1) `0x23130` stores the slot state `9` before calling
`0x3C4CC`, so with the slot being fighter slot 0 `0x3C4CC` takes its `0x3C480` arm: the run with `0x3C4CC` in
`real` mode records `0x3C4CC, 0x3C480, 0x2C3FC` on both sides (test `test_a_real_callee_runs_on_both_sides_
and_its_own_calls_are_recorded`), which is what the port's comment on `fighter_23130` claims. (2) In the
`VERIFIED` spec the slot (`0x10A200`) is not a fighter slot, so the stubbed `0x3C4CC` reads nothing of it;
the cases exercise `0x23130`'s own block, not `0x3C4CC`'s dispatch, which its own row covers.

## §E3.7 Indirect calls and jump tables

**Bounded switches.** `static_scan(..., switches=True)` (used by `diff_verify`; E2's `entry_triage` keeps the
default, so its committed table cannot move) follows `jmp dword ptr [R*4 + T]` (no base register) under this
rule (Tasks 3-4's fix round; `diff_emu.switch_cases`):

1. Among the up to five instructions that precede the `jmp` on its own straight run there is a `cmp r, imm`
   with `r` in `R`'s register family (`al`/`ax`/`eax` all bound `eax`) **immediately followed by the `ja`**:
   the `ja` is the instruction directly after that `cmp`, and tests that `cmp`.
2. Between the `ja` and the `jmp` there is only an `and idx32, imm` or a `movzx idx32, r` that masks the
   index (both clear its upper bits), or a `nop`, or a `mov`/`movzx`/`movsx`/`lea`/`xor` whose destination is
   a register of **another** family (it leaves the index alone). Any other instruction, including any other
   write to the index's own family, breaks the bound and the jump stays unknown.
3. A **narrow** compare (`cmp al`, `cmp ax`) needs the register's upper bits cleared by that point (the
   `and eax,0xff` / `and edi,0xffff` forms); otherwise the compare does not bound the 32-bit index.

The table then holds `imm + 1` dwords (`imm` masked to the compared width). This is stricter than E2's rule
(E2 §E2.2 checks neither the register, nor the `ja`'s place, nor the path between the `ja` and the jump); the
first implementation (`d34a7ce`) was looser than this and accepted a `ja` that tested another `cmp` and an
index rewritten between the `ja` and the jump, which could hide blocks (a false "every block hit"), so the fix
round (`4127800`) tightened it. **Join-point limit:** the walk is over the straight run the scan itself followed;
it does not check that no other path joins that run between the `ja` and the `jmp`, so a branch into the middle
of the mask or move would reach the `jmp` with an unbounded index. It is not reachable in the functions checked
here (`0x2C3FC`, `0x2B2A0`, `0x1BD06` inside `0x1BBAC`) and is a named limit (§E3.8). On the image:

| function | switch | E1 scan | with switches |
|---|---|---|---|
| `0x2C3FC` (voice) | `cmp al,6; ja; and eax,0xff; jmp [eax*4+0x2c3e0]` at `0x2C422..0x2C42F` | 7 leaders, unknown jump `0x2C42F` | 100 leaders, none unknown |
| `0x2B2A0` (animation dispatcher) | `cmp di,0x2e; ja; and edi,0xffff; jmp [edi*4+0x2b1e4]` at `0x2B2EC..0x2B2FC` | 6 leaders, `0x2B2FC` | 85 leaders; left: the indirect **calls** `0x2B56D`, `0x2B594`, `0x2B5EA` |
| `0x1BBAC` | `0x1BD06` bounded (`cmp ax,6`); `0x1BD7C` is `cmp ax,6; ja; xor edx,edx; mov dx,ax; jmp [edx*4+...]` | 35, two unknown | 42, `0x1BD7C` unknown: the compare bounds `ax`, the `mov dx,ax` copies it to `edx` and the `jmp` indexes `edx`, a different family from the compared one; the rule does not follow a bound across a register copy (conservative) |
| `0x18350` | `lea esi,[eax*4]; ... jmp cs:[esi+0x18334]` at `0x18384` | 6, `0x18384` | unchanged: a pre-scaled base, not an index; stays indirect, conservatively |

**An indirect call no longer keeps a function `PARTIAL`.** It returns to the next instruction, which the scan
follows, so it hides no block; and a run that reaches it either resolves the target into the allow-list or
the call set, or stops (`NOT_EXERCISABLE`, naming the target). Only an indirect **jump** that is not a bounded
switch keeps a function `PARTIAL` (mutation V4 fails a test).

**E2's blockers (§E2.6), each settled:**

| blocker | what it is | with E3 |
|---|---|---|
| `0x2B56D` in `0x2B2A0` (58 rows) | `call dword ptr [0x105bd4]`, the animation code pointer (also `0x2B594`, `0x2B5EA`) | the 85 rows stub `0x2BC30`/`0x3C4CC` (the callees whose trees reach it), so it never runs; verifying `0x2B2A0` itself needs one `Call` per code pointer its cases reach, resolved at run time; port side: `anim_indirect` -> `fn_resolve` -> a seamed registered target, or the miss report |
| `0x2B0E9` in `0x2AE14` (22 rows) | `call dword ptr [ebx + 0xbb9dc]`, the actor type's callback (12-byte records) | stubbing `0x2AE14` (`SPAWN`) removes it; verifying `0x2AE14` resolves it as above (port: `actor_spawn`'s `fn_resolve(cb)`) |
| `0x18384` in `0x18350` (2 rows: `0x36114`, `0x40170`) | a jump table through a pre-scaled base | the rows stub their direct callees; `0x18350`'s own row stays `PARTIAL` until the bound is shown (named limit) |
| `0x6811A` in `0x680F0`, `0x62006` in `0x62003` (1 row each) | `call dword ptr [esi + 0x84c]`, `call dword ptr [0xef934]`: runtime code above `0x5D000` (WATCOM/DOS4GW; `0x62003` is the error path `fighter.c` names) | stubbed at the direct-callee boundary; the runtime functions are not targets (AGENTS.md: the port serves them from the host libc) |
| `in al, dx` at `0x1B899` in `0x1B890` (row `0x1BDF4`, via the host-owned `0x1BBAC`) | port I/O | the `NAMED_GAP` row `host_1b890`; `0x1BDF4`'s spec will stub `0x1BBAC` (host I/O, spec §5.2), whose port function then needs a seam |

## §E3.8 Named gaps and limits

- **`0x1B890`: `in at 0x1B899`**, the first `NAMED_GAP` row. A gap spec runs the original only: every case
  must stop on exactly the named instruction; a case that returns, or stops elsewhere, makes the row a
  `MISMATCH` (the gap is stale or misnamed), and `make diff-verify` fails.
- **Indirect calls to unregistered targets are compared by address only.** The port skips them and cannot
  know their arguments; such a target's `Call` must declare `args=()`.
- **EAX of a void port function is not compared** (mask 0). For `0x3C4CC` the original's EAX at return is its
  last stubbed callee's. A straight-line scan of its 67 direct call sites (scratch `callers.py`: from the
  return address to the first full write or read of EAX, a branch or a `ret`) finds 31 that rewrite EAX
  first, 19 that reach a `ret` (EAX flows to their own caller), 15 whose first access reads AL or AH (most
  after a partial write such as `mov ah,[...]`, which the scan does not separate) and 2 that branch. Whether
  any caller depends on `0x3C4CC`'s EAX is therefore open; P settles it for each caller it verifies (a caller
  that reads it shows up as a `MISMATCH` when the stub's EAX changes what the original computes, since the
  port's void function cannot pass it on).
- **A stub's declared effect is the harness's model of the callee.** A caller path that depends on memory
  the real callee writes is exercised only as far as the declared writes and the cases' pokes produce those
  values.
- **Switch bounds:** an index moved to another register (`0x1BD7C`) or pre-scaled into a base (`0x18384`)
  stays an unknown jump, conservatively (§E3.7). **Join points (m5):** the guard walk does not check for a
  second path joining the straight run between the `ja` and the `jmp`; not reachable in the checked functions.
- **Seeds are hand pokes into zero BSS, not real snapshots** (spec §5.3; named deviation, E1 precedent; §E3.6).
- **Four stubbed callees have no row** (`0x2C3FC`, `0x2BC30`, `0x2AE14`, `0x3C480`) and the callee column is one
  level deep (§E3.6).
- **`0x2AE14` reports `desc` as a mem[] offset only when `desc` lies in `mem[]`.** `0x2F5A0` passes a C
  stack array, which has no offset: the seam reports `0xFFFFFFFF` (above `MEM_SIZE`, so no spec can name it)
  instead of computing `desc - mem` across objects. A spec under `0x2F5A0` therefore allows `0x2AE14` (it
  runs the body on both sides) rather than stubbing it (Task 7, from Task 6's review).
- **`diffrun` registers every `fn_register` call site of `port/src`** (`actors_init`, `effects_init`'s
  `camera_register`, `attract_scene_tick`'s `attract_register`, `svcmenu_register`), so D3's fail-closed
  rule holds for each ported code pointer; a test greps the sources for the call sites and probes each
  registered address (Task 7).
- **A seam must be the callee's first statement**, and the original function must be one C function: a
  callee the port inlined or split cannot be stubbed until it is one function (PORTING.md's rule).
- E1's limits stand: flags and the other registers are not compared; all of a spec's cases share one
  `diffrun` process (static C state persists between cases, E1 §E.7); the default image path is shared
  between worktrees (E1 §E.6.9; `make verify` takes `DIFF_IMAGE`).

## §E3.9 Cost

On the planning host (macOS arm64, Apple clang 21, no other load): the whole self-check (14 real rows, 15
mutants, 29 `diffrun` launches) 3.61 s real, against E1's 1.35 s for 6 + 7; the 116 Python tests 15.2 s; a
clean build of the tree 2.31 s at `-j8`. `make verify` on the prototype: 743 s real, exit 0.

## §E3.10 What P does with it

For each target F of a P batch: port F; give each direct callee G a seam (`PR_SEAM`/`PR_SEAM_RET` as G's
first statement, G's original address, its C arguments) unless G is a leaf whose arguments cannot be compared
(allow it, and give it its own row); write F's `Spec` with `calls` (`args` in G's C order, `pop` from G's `ret
N`, a stated EAX), cases that hit every block of F, and at least one mutant that only the call list catches;
regenerate the E2 table in the same commit (AGENTS.md, decision D3). After E3 five callees carry a seam
(`0x2C3FC`, `0x2BC30`, `0x2AE14`, `0x3C4CC`, `0x3C480`) and two are verified allow-mode builders (`0x33950`,
`0x339AC`). The other ported direct callees of the 85 rows still need a seam when a row that calls them is
ported: `1A570 39834 2A17C 3A95C 3C190 39F40 34D8C 35838 39280 13244 2A148 2BCF4 1890C 1883C 29BC8 3BF70
29C08 37D18 3C208 13C70 3AA54` (21); the eight unported ones (§E3.1) must be ported first, or, for the
host-owned `0x1BBAC`, given a seamed host-side C function.

## §E3.11 Facts verified by running

- The image sha1 `ff3b8cb1...` (dumped by `diffrun` at `a935e91`), `unicorn` 2.1.4, `capstone` 5.0.7.
- `ld: unknown options: --wrap=foo` / `-wrap`; `llvm-objcopy` only under `/opt/homebrew/opt/llvm`.
- Clean build 2.31 s (`-j8`) / 4.87 s (`-j1`).
- The prototype's `make verify`: exit 0, 743 s, the 45 oracle lines equal to `oracle-lines-base.txt`, the
  `make audio-render` WAV equal to `before-t2.wav`, `entry-triage` unchanged (579 / 329 unported, 166 ported /
  134: 48, 67, 19), `771 1203 64` / `731 731 100`.
- The replay: every red and green output and every mutation result quoted in the plan; the final
  self-check above; `make verify` on the replay's final state (quoted in the plan's Task 8).
- The real-image switch figures and the `ret N` table above; the direct-callee counts of §E3.1.
- **Closure (Task 8, on the implemented tree, head `9ea965c` plus this commit):** `make diff-verify` runs 131
  Python tests OK and prints `13/13 functions VERIFIED; 16/16 mutants detected; 1 named gaps; 8/13 with every
  callee VERIFIED`; `PR_ORACLE_REQUIRED=1 ./build/run_tests` all checks passed; `make entry-triage` unchanged
  (329 unported, 166 ported; 48 / 67 / 19); the full `make verify` (parallel-safe overrides) exit 0 with the 45
  oracle lines equal to `oracle-lines-base.txt`, the `make audio-render` WAV equal to `before-t2.wav`,
  `771 1203 64` / `731 731 100`. `grep 'pr_seam = '` finds the hook set only in `diff_runner.c` and, inside
  `test_call_seam`, `test_platform.c`.
