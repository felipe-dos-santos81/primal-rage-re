# Reverse completion C3f: the frontier rows, part 6 (record)

**Scope.** Track P's ninth verification-only batch (the C3e record §C3e.7's 32-address tail): the
**leaf tier** this session measured — seven fighter leaves (`0x38BB0` `fighter_38bb0`, `0x38BC8`
`fighter_38bc8`, `0x3B038` `fighter_3b038`, `0x46534` `fighter_46534`, `0x3BDB0`
`fight_attack_ready`, `0x4649C` `fighter_input_scan`, `0x1AB10` `fighter_state_ok`), the string
pair (`0x1E75C` `string_lock`, `0x1E808` `string_unlock`, `0x474E4` `string_decode`, `0x1C500`
`game_string_get`) and List C's two (`0x1A6AC` `fighter_block_anim`, `0x3CF38`
`hit_chain_resolve`) — **thirteen rows, 56 mutants, all detected.** One raw-over-port correction
(the `0x1E808` +0x10 store), one port restructuring (`0x474E4` owns the lock, `0x1C500` is its
tail), the host-libc seam wrappers the text rows need, and the seams/exports the stubbed callees
need. **The tail verdict** (§C3f.7): after this batch the remaining frontier is **31 ported
addresses** — the **nineteen deferred C3f addresses** (the `0x38D90`/`0x38FEC` tree, the four other
first-wave rows' callees and the text tree) plus the **twelve ported addresses the `0x3CF38` row
exposes** — dependency-ordered with sizes and callers; `0x32BAC` is a named non-row. Plan:
`2026-10-05-reverse-c3f-frontier-rows-6.md`. Recipe: E3 record §E3.10; checklist:
`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**Status of the numbers.** Measured by the planner on 2026-10-07 on `reverse-c3f` at the base
`main` `0aa5eff` (= C3e merged), in this worktree: the baseline gates and a full prototype of the
thirteen rows (every gate run, then reverted). The image is `build/diffrun --exe
data/game/C/PRAGE.EXE --image-out FILE`, sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's,
E3's, P1-P8's, C1-C3e's). Every address below is capstone 5.0.7 over that image (fixups applied);
`unicorn` 2.1.4 runs the original side. Ghidra was not consulted. The prototype diff is embedded
in the plan; the prototype was reverted (`git checkout -- port tools`), leaving only this record
and the plan.

---

## §C3f.1 The members, re-derived from the raw

The C3e record §C3e.7's 32-address tail has three lists. This session measured thirteen of them
(the leaf tier); the members were re-checked against the raw's call scans (capstone over the
image) and the base table:

| row | entry | size / insns | the raw's direct callees (mode in this batch) |
|---|---|---|---|
| `fighter_38bb0` | `0x38BB0` | 24 / 12 | — |
| `fighter_38bc8` | `0x38BC8` | 34 / 14 | `0x38BB0` allow (leaf) |
| `fighter_3b038` | `0x3B038` | 72 / 34 | — |
| `fighter_46534` | `0x46534` | 93 / 29 | — |
| `fight_attack_ready` | `0x3BDB0` | 43 / 18 | `0x33950` allow (leaf) |
| `fighter_input_scan` | `0x4649C` | 101 / 45 | — |
| `fighter_state_ok` | `0x1AB10` | 74 / 33 | `0x33A10` allow (leaf) |
| `string_lock` | `0x1E75C` | 23 / 9 | — |
| `string_unlock` | `0x1E808` | 22 / 9 | `0x500BB` allow (the DPMI clock, named) |
| `string_decode` | `0x474E4` | 215 / 90 | `0x1E75C`, `0x1E808` stubs (their own rows) |
| `game_string_get` | `0x1C500` | 37 / 13 | `0x474E4` stub (its own row) |
| `fighter_block_anim` | `0x1A6AC` | 134 / 46 | `0x33A68` allow (leaf), `0x18B04` stub (rowed), `0x3C480` stub (rowed) |
| `hit_chain_resolve` | `0x3CF38` | 203 / 67 | `0x3CD44`/`0x3CE58`/`0x3C6A8` stubs (C3g), `0x32BAC` stub (named non-row: the raw is a one-byte RET) |

The measured rows (cases/blocks/mutants measured; `EAX mask` from the Spec):

| row | entry | cases | blocks | mutants | EAX mask | named unhit |
|---|---|---|---|---|---|---|
| `fighter_38bb0` | 0x38BB0 | 2 | 3/3 | 3/3 | 0 | — |
| `fighter_38bc8` | 0x38BC8 | 2 | 3/3 | 3/3 | 0 | — |
| `fighter_3b038` | 0x3B038 | 8 | 5/5 | 3/3 | 0xFF | — |
| `fighter_46534` | 0x46534 | 10 | 7/7 | 4/4 | 0 | — |
| `fight_attack_ready` | 0x3BDB0 | 7 | 4/4 | 3/3 | 0xFF | — |
| `fighter_input_scan` | 0x4649C | 10 | 12/12 | 4/4 | 0xFF | — |
| `fighter_state_ok` | 0x1AB10 | 8 | 14/14 | 3/3 | 0xFF | — |
| `string_lock` | 0x1E75C | 5 | 4/4 | 4/4 | 0xFFFFFFFF | — |
| `string_unlock` | 0x1E808 | 3 | 1/1 | 4/4 | 0 | — |
| `string_decode` | 0x474E4 | 8 | 17/17 | 7/7 | 0xFFFFFFFF | — |
| `game_string_get` | 0x1C500 | 3 | 3/3 | 4/4 | 0xFFFFFFFF | — |
| `fighter_block_anim` | 0x1A6AC | 11 | 8/8 | 6/6 | 0 | — |
| `hit_chain_resolve` | 0x3CF38 | 11 | 12/12 | 8/8 | 0xFF | — |

No row reads outside the image (the string tables and the handle live in the data object at
0x10AA00/0x10B000, the out buffer at 0x10A200, the input ring at 0x108270).

## §C3f.2 The rows: cases, seams, fixtures and the corrections

Each row is a `Spec` in `tools/diff_verify.py` (`C3F_SPECS` and its `c3f_*` fixture helpers), a
binding `b_*` and mutants `m_*` in `port/tests/diff_runner.c` (`k_bindings`), and the exact-set
extensions in `tools/tests/test_diff_verify.py` (`C3F_MASKS`, `C3F_KINDS`, the case-set test, the
clobber table, the counter line). The callee column of the full run's table:

| row | entry | cases/blocks/mutants | callees (each by its own check) |
|---|---|---|---|
| `fighter_38bb0` | 0x38BB0 | 2 / 3/3 / 3 | — |
| `fighter_38bc8` | 0x38BC8 | 2 / 3/3 / 3 | `38BB0` allow VERIFIED |
| `fighter_3b038` | 0x3B038 | 8 / 5/5 / 3 | — |
| `fighter_46534` | 0x46534 | 10 / 7/7 / 4 | — |
| `fight_attack_ready` | 0x3BDB0 | 7 / 4/4 / 3 | `33950` allow VERIFIED |
| `fighter_input_scan` | 0x4649C | 10 / 12/12 / 4 | — |
| `fighter_state_ok` | 0x1AB10 | 8 / 14/14 / 3 | `33A10` allow VERIFIED |
| `string_lock` | 0x1E75C | 5 / 4/4 / 4 | — |
| `string_unlock` | 0x1E808 | 3 / 1/1 / 4 | `500BB` allow unverified (named non-row) |
| `string_decode` | 0x474E4 | 8 / 17/17 / 7 | `1E75C` stub VERIFIED, `1E808` stub VERIFIED |
| `game_string_get` | 0x1C500 | 3 / 3/3 / 4 | `474E4` stub VERIFIED |
| `fighter_block_anim` | 0x1A6AC | 11 / 8/8 / 6 | `18B04` stub VERIFIED, `33A68` allow VERIFIED, `3C480` stub VERIFIED |
| `hit_chain_resolve` | 0x3CF38 | 11 / 12/12 / 8 | `32BAC`/`3C6A8`/`3CD44`/`3CE58` stubs unverified (C3g) |

**Seams and exports** (each seam a first statement; `E.callee_clobbers` re-derives every declared
clobber; a stub's call is declared with those clobbers in `C3F_SPECS`):

| function | address | change | conformance |
|---|---|---|---|
| `string_lock` | `0x1E75C` | loses `static`; `PR_SEAM_RET`; declared in `flow.h` | no clobbers (stub in `string_decode`'s row) |
| `string_unlock` | `0x1E808` | loses `static`; `PR_SEAM`; declared; **gains the `0x1E819` store** | no clobbers (stub in `string_decode`'s row) |
| `string_decode` | `0x474E4` | loses `static`; `PR_SEAM_RET`; declared; **owns the lock/unlock** | `ebx`, `edx` (stub in `game_string_get`'s row) |
| `text_glyph_emit` | `0x2F5A0` | `PR_SEAM_RET`, `col`/`row` by value (P2.7) | not yet a stub in any row |
| `text_render` | `0x2F830` | `PR_SEAM_RET` | — |
| `text_cursor_set` | `0x2F198` | `PR_SEAM`, string by value (P2.7) | — |
| `text_cells_release` | `0x2F280` | `PR_SEAM` | — |
| `text_cells_release_vertical` | `0x2F314` | `PR_SEAM` | — |
| `text_cursor_hold` | `0x2F4BC` | `PR_SEAM` | — |
| `text_vertical_set` | `0x2F20C` | `PR_SEAM` | — |
| `text_number_draw` | `0x2F4D0` | `PR_SEAM` | — |
| `text_number_format` | `0x2EFD4` | `PR_SEAM_RET`, buffer by value (P2.7); calls `text_number_core` and `host_memset` | — |
| `text_number_core` | `0x2EF24` | new (the `0x2EF24` body); `PR_SEAM_RET`, buffer by value; declared in `actors.h` | — |
| `host_sprintf` | `0x65546` | new host wrapper; `PR_SEAM_RET` | — |
| `host_memset` | `0x61A70` | new host wrapper; `PR_SEAM` | — |
| `fighter_38c5c` | `0x38C5C` | `PR_SEAM` | not yet a stub in any row |
| `fighter_38ed0` | `0x38ED0` | `PR_SEAM_RET` | not yet a stub in any row |
| `fighter_block_anim` | `0x1A6AC` | `PR_SEAM` | not yet a stub in any row |
| `hit_chain_resolve` | `0x3CF38` | `PR_SEAM_RET` | not yet a stub in any row |
| `hit_slot_seed` | `0x3C6A8` | `PR_SEAM` | `edx` (stub in `hit_chain_resolve`'s row) |
| `hit_scan` | `0x3CD44` | `PR_SEAM_RET` | no clobbers (stub in `hit_chain_resolve`'s row) |
| `hit_reaction_drive` | `0x3CE58` | `PR_SEAM_RET` | `edx`, `edi`, `ebp` (stub in `hit_chain_resolve`'s row) |
| `hit_sound` | `0x32BAC` | `PR_SEAM` (the named non-row: the raw is `ret`) | no clobbers (stub in `hit_chain_resolve`'s row) |
| `fighter_input_scan` | `0x4649C` | loses `static`; declared in `fighter.h` (its own row's binding) | — |

**Corrections and decisions during prototyping (each recorded here with its evidence):**

1. **`string_unlock` was a raw-over-port correction.** The raw `0x1E808` is
   `and [eax+0x15],~2; call 0x500BB; mov [edx+0x10],eax` — the `0x500BB` DPMI clock read stored at
   handle+0x10 (0x1E814/0x1E819; `0x500BB` is `mov eax,[0x101500]; ret`, the shadow the port
   already models). The port omitted both; the row's `@store`/`@clock` mutants and case `u0`'s
   +0x10 sentinel caught it. The port now stores `DSD(DS_00101500)`.
2. **`0x474E4`/`0x1C500` restructuring.** The raw `0x474E4` owns the lock: `0x474F2` loads the
   handle `DS_001082DC` and calls `0x1E75C`, `0x475A5` calls `0x1E808` (both always, even when the
   lock fails); `0x1C500` then only calls `0x474E4` and zeroes `out[0]` when the return is 0
   (`0x1C50C`/`0x1C511`, `0x1C515`, `0x1C51D` returns `0x102760`). The port had `game_string_get`
   doing the lock around a `string_decode(base, ...)` body; a row with `0x474E4` stubbed would have
   seen the lock's +0x15 write on the port side only (a call-memory mismatch). `string_decode` is
   now `(id, out, outlen)` with the lock/unlock inside, and `game_string_get` is `(id)` with the
   tail. Behavior outside the row is unchanged (the extra +0x10 store is correction 1; the lock
   failure path now runs the raw's off-zero reads, which the port's zeroed low memory makes
   harmless). Fix wave 2026-10-05: the port's `outlen` compare is now the raw's signed form
   (`(s32)len < (s32)outlen`, the `0x47556 JGE`; the code compared `u32`); `len` is a byte and
   `outlen` the caller's `0x100`, so the domain is unchanged and the `string_decode` row stays
   VERIFIED, 7/7.
3. **The host-libc wrappers were a session decision, not a port behavior change.** The WATCOM
   bodies at `0x65546` (sprintf) and `0x61A70` (memset) cannot run under unicorn. Reproducible
   probe (fix wave 2026-10-05; `tools/diff_emu.run_original`, image from `build/diffrun --exe
   data/game/C/PRAGE.EXE --image-out`): with its seven direct callees `{0x6C8EB, 0x6CB6C, 0x6CC9F,
   0x6CE84, 0x72D20, 0x72CD6, 0x6CCFA}` allowed, `run_original(image, 0x65546, regs={"s0":
   0x10A200, "s1": 0x80B40, "s2": 42}, allow_calls=...)` leaves the image for the zero low memory
   (EIP reaches 0 and walks up in two-byte steps; 0xFF3C is one of them) and stops `unmodeled:
   undecodable bytes at 0xFF56`; with no allows the first stop is `call 0x6C8EB from 0x65562`, and
   `0x61A70`'s first stop is `call 0x65490 from 0x61A80`. The earlier note's "`hlt` at `0xFF3C`"
   was not reproducible: `0xFF3C` executes there as a zero byte, not `hlt`. Any row whose original
   calls them must stub them, but the port's C calls the host
   libc directly and could not be intercepted: `host_sprintf`/`host_memset` are the seam wrappers
   (inert outside `build/diffrun`); `text_number_core` (the `0x2EF24` port) is the new function
   `text_number_format` calls. Recorded because the pattern is new for this repo (runtime callees
   as call-set stubs). Fix wave 2026-10-05: `host_memset` takes the `u8 *dest` its body memsets
   (it was a `u32` offset the body re-derived as `mem + dest` — a round-trip that happened to
   reconstruct the pointer on this host); the seam keeps the exact three recorded args
   `(u32)(dest - mem), fill, len` (the raw's EAX/EDX/EBX at `0x61A70`), so no row's semantics
   changed (no row records `0x61A70` today). Codegen proof (arm64 `-O2`; object
   `build/CMakeFiles/prage_core.dir/src/game/actors.c.o`): `nm` shows the wrapper out of line
   (`_host_memset` at `0xd7ac`); its only callers are `0xd6f8`/`0xd728` (both in
   `_text_number_format`, passing `dest` and `dest + len`); the production path ends in
   `___memset_chk` at `0xd864` with x0 the incoming pointer, and the `(u32)(dest - mem)` subtract
   survives only on the seam path (`0xd800`, taken when `pr_seam != NULL`). The same fix wave
   corrected the `actors.c` comment's claim that `0x65546`'s stack-passed `dest` could use the
   P2.7 by-value form: each seam records its pointer arguments as mem[] offsets; the by-value
   dwords are the text rows' own register-passed buffers.
4. **The P2.7 by-value seams.** `0x2F198`'s string and `0x2EFD4`/`0x2EF24`'s buffers are on their
   caller's stack (0x2F4D0's `buf[0x14]`), so the seam passes their bytes as little-endian dwords
   and the `E.Call` names `[ebx]`/`[edx]` (record §P2.7); `0x2F5A0`'s `col`/`row` pointers are
   dereferenced the same way (`[edx]`/`[ebx]`), because the callee's advance writes back through
   them. Both sides seed the buffers zero, so the recorded dwords agree and the call's stall
   (a stub cannot write the caller's stack) is symmetric. Fix wave 2026-10-05: `0x2EFD4`'s own
   format buffer is `0x14` bytes in the port too (it was `0x10`, with `host_sprintf` capped at
   `0x10`), matching the raw's `sub esp,0x14` at `0x2EFD7` and the `0x14` of every caller; the
   four-dword seams are unchanged (they record the `dest` argument, not this buffer) and `%i`'s
   12-byte maximum makes the old size unobservable.
5. **`hit_sound` gained a seam.** `0x32BAC` is a one-byte RET (the recorded orphaned-body
   correction at `fighter.c:4229`); the port's `hit_sound` no-ops it. The `0x3CF38` row stubs it so
   the `@sound` mutant (the wrong +0x63 condition) is visible in the call list; the address is a
   **named non-row** (its own row cannot exist — there is no body to verify).

**Fixtures.** The rows reuse the C3d scratch records and slot pokes (`c3d_slot_pokes`,
`C3D_REC0/1`); the string pair uses handles at 0x10AA00 with the table at 0x10B000 and the out
buffer at 0x10A200; the input ring is the image's own 0x108270. No fixture pokes more than 64
bytes per region (the driver's per-poke cap; the first draft's 0xC0-byte 0x107A80 poke failed
`bad poke line`).

## §C3f.3 The mutants

56 mutants, all detected (`python3 tools/diff_verify.py --self-check`: `1087/1087`); what alone
catches each is pinned by `test_each_c3f_mutant_is_caught_by_what_it_breaks` (`C3F_KINDS` plus the
measured case-set table; the exact dicts are the ones committed in `tools/tests/test_diff_verify.py`).
The kinds (measured; the record's copy of the test's `C3F_KINDS`):

```
C3F_KINDS = {
    "fight_attack_ready@s53": {'eax'},
    "fight_attack_ready@s54": {'eax'},
    "fight_attack_ready@side": {'eax'},
    "fighter_38bb0@len": {'byte'},
    "fighter_38bb0@off": {'byte'},
    "fighter_38bb0@stride": {'byte'},
    "fighter_38bc8@clear": {'byte'},
    "fighter_38bc8@off": {'byte'},
    "fighter_38bc8@word": {'byte'},
    "fighter_3b038@s1": {'eax'},
    "fighter_3b038@s2": {'eax'},
    "fighter_3b038@sum": {'eax'},
    "fighter_46534@cap": {'byte'},
    "fighter_46534@floor": {'byte'},
    "fighter_46534@order": {'byte'},
    "fighter_46534@sign": {'byte'},
    "fighter_block_anim@bit": {'call #1'},
    "fighter_block_anim@call": {'call #1'},
    "fighter_block_anim@mask": {'byte'},
    "fighter_block_anim@rec": {'call #1'},
    "fighter_block_anim@s54": {'byte', 'call #1'},
    "fighter_block_anim@stream": {'call #1'},
    "fighter_input_scan@mask": {'eax'},
    "fighter_input_scan@n1": {'eax'},
    "fighter_input_scan@n2": {'eax'},
    "fighter_input_scan@wrap": {'eax'},
    "fighter_state_ok@s53": {'eax'},
    "fighter_state_ok@s54": {'eax'},
    "fighter_state_ok@sign": {'eax'},
    "game_string_get@call": {'byte', 'call #0'},
    "game_string_get@id": {'call #0'},
    "game_string_get@ret": {'eax'},
    "game_string_get@zero": {'byte'},
    "hit_chain_resolve@drive": {'byte', 'call #2', 'call #3', 'eax'},
    "hit_chain_resolve@guard": {'byte', 'call #1', 'call #2', 'call #3', 'eax'},
    "hit_chain_resolve@inc": {'byte', 'call #2 memory', 'call #3 memory'},
    "hit_chain_resolve@ret": {'eax'},
    "hit_chain_resolve@s55": {'byte'},
    "hit_chain_resolve@s5f": {'byte'},
    "hit_chain_resolve@seed": {'call #2', 'call #3'},
    "hit_chain_resolve@sound": {'call #3'},
    "string_decode@link": {'byte', 'call #1 memory', 'eax'},
    "string_decode@off": {'byte', 'call #1 memory', 'eax'},
    "string_decode@ret": {'eax'},
    "string_decode@skip": {'byte', 'call #1 memory', 'eax'},
    "string_decode@term": {'byte', 'call #1 memory'},
    "string_decode@trunc": {'byte', 'call #1 memory', 'eax'},
    "string_decode@xor": {'byte', 'call #1 memory'},
    "string_lock@base": {'byte', 'eax'},
    "string_lock@bit": {'byte', 'eax'},
    "string_lock@len": {'byte', 'eax'},
    "string_lock@or": {'byte'},
    "string_unlock@and": {'byte'},
    "string_unlock@clock": {'byte'},
    "string_unlock@off": {'byte'},
    "string_unlock@store": {'byte'},
}
```

The exact case set that alone catches each mutant (measured on the prototype; the record's copy of
the test's table):

```
C3F_CASES = [
        ("fight_attack_ready@s53", ['r2', 'r3']),
        ("fight_attack_ready@s54", ['r0', 'r1', 'r3', 'r5', 'r6']),
        ("fight_attack_ready@side", ['r1', 'r2', 'r4', 'r5']),
        ("fighter_38bb0@len", ['b0', 'b1']),
        ("fighter_38bb0@off", ['b0', 'b1']),
        ("fighter_38bb0@stride", ['b1']),
        ("fighter_38bc8@clear", ['b0', 'b1']),
        ("fighter_38bc8@off", ['b1']),
        ("fighter_38bc8@word", ['b0', 'b1']),
        ("fighter_3b038@s1", ['w3', 'w7']),
        ("fighter_3b038@s2", ['w1', 'w2', 'w5', 'w6']),
        ("fighter_3b038@sum", ['w1', 'w2', 'w6']),
        ("fighter_46534@cap", ['v0', 'v1', 'v2', 'v5', 'v6', 'v7']),
        ("fighter_46534@floor", ['v4', 'v8']),
        ("fighter_46534@order", ['v8']),
        ("fighter_46534@sign", ['v3', 'v9']),
        ("fighter_block_anim@bit", ['a1']),
        ("fighter_block_anim@call", ['a0', 'a10', 'a2', 'a7', 'a9']),
        ("fighter_block_anim@mask", ['a10', 'a9']),
        ("fighter_block_anim@rec", ['a0', 'a10', 'a9']),
        ("fighter_block_anim@s54", ['a2', 'a3', 'a4', 'a5', 'a6', 'a8']),
        ("fighter_block_anim@stream", ['a0', 'a10', 'a7', 'a9']),
        ("fighter_input_scan@mask", ['s1', 's4', 's5', 's6', 's7', 's8', 's9']),
        ("fighter_input_scan@n1", ['s5']),
        ("fighter_input_scan@n2", ['s6']),
        ("fighter_input_scan@wrap", ['s6']),
        ("fighter_state_ok@s53", ['o3', 'o5']),
        ("fighter_state_ok@s54", ['o2', 'o4', 'o7']),
        ("fighter_state_ok@sign", ['o4', 'o5']),
        ("game_string_get@call", ['c0', 'c1', 'c2']),
        ("game_string_get@id", ['c0', 'c1', 'c2']),
        ("game_string_get@ret", ['c0', 'c1', 'c2']),
        ("game_string_get@zero", ['c1']),
        ("hit_chain_resolve@drive", ['h1', 'h10', 'h2', 'h3', 'h4', 'h6', 'h8', 'h9']),
        ("hit_chain_resolve@guard", ['h5', 'h7']),
        ("hit_chain_resolve@inc", ['h3', 'h4', 'h6', 'h8', 'h9']),
        ("hit_chain_resolve@ret", ['h1', 'h10', 'h2']),
        ("hit_chain_resolve@s55", ['h1', 'h10', 'h2']),
        ("hit_chain_resolve@s5f", ['h10', 'h2']),
        ("hit_chain_resolve@seed", ['h3', 'h4', 'h6', 'h8', 'h9']),
        ("hit_chain_resolve@sound", ['h3', 'h4', 'h6', 'h8', 'h9']),
        ("string_decode@link", ['n2', 'n3']),
        ("string_decode@off", ['n0', 'n1', 'n2', 'n3', 'n4', 'n4b', 'n6']),
        ("string_decode@ret", ['n0', 'n1', 'n2', 'n3', 'n6']),
        ("string_decode@skip", ['n1', 'n3']),
        ("string_decode@term", ['n0', 'n1', 'n2', 'n3', 'n5', 'n6']),
        ("string_decode@trunc", ['n4b']),
        ("string_decode@xor", ['n0', 'n1', 'n2', 'n3', 'n6']),
        ("string_lock@base", ['l0', 'l2', 'l3', 'l4']),
        ("string_lock@bit", ['l0', 'l3', 'l4']),
        ("string_lock@len", ['l1', 'l3']),
        ("string_lock@or", ['l0', 'l4']),
        ("string_unlock@and", ['u0', 'u2']),
        ("string_unlock@clock", ['u0', 'u1', 'u2']),
        ("string_unlock@off", ['u0', 'u1', 'u2']),
        ("string_unlock@store", ['u0', 'u1', 'u2']),
]
```

The mutants whose catch includes a call or call-memory kind: `fighter_block_anim` at `0x18B04`/
`0x3C480`, `game_string_get` at `0x474E4`, `hit_chain_resolve` at `0x3CE58`/`0x3C6A8`/`0x32BAC`.
The string pair's mutants are caught on EAX or bytes alone (their callees are stubs whose effects
the lock/unlock fixtures fix). Fix wave 2026-10-05: `c3f_unlock_case`'s `flags` byte now lands at
`+0x15`, the byte the clear reads (it was at `+0x16`, inert), and the `+0x14` byte is seeded `0x02`
(bit 1 already set, so the `@off` mutant's wrong-offset clear is visible even when `flags` is
`0x00`); only `string_unlock@and`'s set re-measured, to `['u0', 'u2']` (`u1`'s `flags=0x00` makes
the clear — and the wrong-mask mutant — a no-op), every other set and all kinds unchanged.

## §C3f.4 Counters, measured

| state | diff-verify counter | E2 |
|---|---|---|
| base `0aa5eff` | `278/278 functions VERIFIED; 1031/1031 mutants detected; 1 named gaps; 196/217 rows with callees closed (61 have none)` | `233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted 30`; voice `0 / 115 / 19` |
| C3f prototype (thirteen rows) | `291/291 functions VERIFIED; 1087/1087 mutants detected; 1 named gaps; 205/225 rows with callees closed (66 have none)` | byte-identical |

The arithmetic: +13 functions, +56 mutants (3+3+3+4+3+4+3+4+4+7+4+6+8), rows with callees
217 -> 225 (eight of the thirteen have callees: `38bc8`, `fight_attack_ready`, `fighter_state_ok`,
`string_unlock`, `string_decode`, `game_string_get`, `fighter_block_anim`, `hit_chain_resolve`; the
five without are `38bb0`, `3b038`, `46534`, `fighter_input_scan`, `string_lock`), no-callee
61 -> 66, closed 196 -> 205 (+9): the six new rows that close (`38bc8`, `fight_attack_ready`,
`fighter_state_ok`, `string_decode`, `game_string_get`, `fighter_block_anim`) and the three
existing rows the new leaves close — `fighter_state_36bc8` (on `0x38BB0`/`0x38BC8`),
`fighter_3b080` (on `0x3B038`) and `fighter_4f434` (on `0x46534`). `string_unlock` and
`hit_chain_resolve` stay open on the named `0x500BB` and the C3g tree respectively.

## §C3f.5 Named gaps and limits

- **The runtime stubs.** `0x65546`/`0x61A70` are stubbed in the rows that call them (the WATCOM
  bodies cannot run under unicorn; §C3f.2 correction 3 has the reproducible probe — the `0x65546`
  chain stops `undecodable bytes at 0xFF56` once its direct callees are followed, `0x61A70`'s
  first stop is `call 0x65490 from 0x61A80`); their own effects are not compared by any row. The
  same applies to `0x500BB` (allow: it is the shadow read the port models). A C3g text row would
  intercept them through these seams; the recorded form for `0x65546`'s stack-passed `dest` is
  still an open row-design question (`0x61A70`'s pointer form is settled by correction 3's codegen
  proof).
- **`0x32BAC` is a named non-row** (a one-byte RET; the port's `hit_sound` no-ops it). The
  `0x3CF38` row stubs it; the row is VERIFIED but not closed until the C3g tree is.
- **The P2.7 by-value seams.** The recorded dwords of a stack buffer are what both sides held at
  arrival; the stub cannot write the caller's stack, so a stubbed `0x2F198`/`0x2EFD4`/`0x2EF24`/
  `0x2F5A0` does not advance `col`/`row` or fill the buffer on either side. The rows' cases stay
  inside that symmetric stall.
- **`string_unlock`'s +0x10 store.** It writes the `DS_00101500` shadow, which the port never
  reads; the correction makes the row byte-exact, not the game behavior different.
- **`string_decode` with a failing lock** now runs the raw's off-zero reads (the port's low memory
  is zero, so it returns 0); the case domain always supplies a base through the lock stub.
- **`hit_chain_resolve`'s `0x32BAC` stub** is a named non-row, and its other three callees are the
  C3g tree: the row is the last one this batch that stays open by construction.
- E3's, P1-P8's and C1-C3e's limits stand: seeds are hand pokes; the memory at a call is `mem[]`
  only; the callee column is one level deep.

## §C3f.6 What the planner ran

- The baseline on the clean `0aa5eff` worktree: `make diff-verify` -> the §C3f.4 base counter and
  table; `make entry-triage` -> `233/262`; `python3 tools/port_progress.py` -> `771 1203 64` /
  `731 731 100`.
- The prototype, in this worktree: the fighter leaves first, then the string pair (the `0x1E808`
  correction was found by case `u0`; the `0x474E4`/`0x1C500` split by the `0x474E4` row's
  call-memory), then List C. Each row was measured with `python3 tools/diff_verify.py --function
  NAME --self-check` until VERIFIED with every mutant detected, then the full `python3
  tools/diff_verify.py --self-check` (§C3f.4 counter), `python3 -m unittest
  tools.tests.test_diff_verify` (the extended exact sets), `make entry-triage` (byte-identical),
  `PR_ORACLE_REQUIRED=1 ./build/run_tests` (`all checks passed`) and a `symbols.h` regeneration
  (byte-identical). Then `git checkout -- port tools`, leaving only this record and the plan.
- The prototype's own corrections: §C3f.2 corrections 1-2 (corrections 3-5 are recorded design
  decisions, not raw-over-port fixes).
- The full `make verify` (~25 min) was not run by the planner: the brief's task-scoped gates are
  the ones above. The `port/src` changes are the correction, the restructuring, the seams and the
  wrappers; the executor runs the full gate in Task 4.

## §C3f.7 The tail verdict (required): the frontier after C3f and what a C3g would need

**Measured on the prototype** (capstone over the image; a "row" is any `Spec` entry; runtime
addresses `>= 0x5D000` and the named non-rows excluded per AGENTS.md's host-libc rule). Row the
thirteen measured rows and the frontier is **31 ported addresses**, in this dependency order:

**Wave 1 (9)** — the deferred C3f first wave plus the three `0x3CF38` callees this batch exposed:

| address | port function | size / insns | callers (rowed) |
|---|---|---|---|
| `0x1A7CC` | `fighter_block_start` | 296 / 84 | `fighter_input_mask` |
| `0x2A690` | `actor_pset_point` | 397 / 125 | `pset_write` |
| `0x37178` | `fighter_37178` | 746 / 243 | `fighter_379c4` |
| `0x38D90` | `fighter_38d90` | 318 / 87 | `fighter_39040` |
| `0x38FEC` | `fighter_38fec` | 81 / 32 | `fighter_39040` |
| `0x3BDDC` | `fighter_attack_consume` | 401 / 127 | `fight_command_map` |
| `0x3C6A8` | `hit_slot_seed` | 61 / 21 | `hit_chain_resolve` |
| `0x3CD44` | `hit_scan` | 66 / 33 | `hit_chain_resolve` |
| `0x3CE58` | `hit_reaction_drive` | 224 / 66 | `hit_chain_resolve` |

**Wave 2 (12)** — the C3f text list's first wave (the `0x38D90`/`0x38FEC` tree) and the exposed
`0x3CF38` tree:

| address | address | address | address |
|---|---|---|---|
| `0x2F20C` (115 insns) | `0x2F4BC` (20) | `0x2F4D0` (61) | `0x38C5C` (125 B) |
| `0x38ED0` (284 B) | `0x34E2C` (548 B) | `0x3C600` (165 B) | `0x3CBC4` (146 B) |
| `0x3CC58` (146 B) | `0x3CCEC` (88 B) | `0x3CE24` (52 B) | `0x4CE70` (176 B) |

**Wave 3 (8)** — the text tree's second wave and the exposed tree's third:

| address | address | address | address |
|---|---|---|---|
| `0x2EFD4` (283 B) | `0x2F0F0` (105) | `0x2F198` (115) | `0x2F280` (146) |
| `0x2F314` (113) | `0x2F830` (239 B) | `0x1922C` (147 B) | `0x3CD94` (141 B) |

**Wave 4 (2)**: `0x2EF24` (34 B), `0x2F5A0` (615 B).

Every one is ported (a `/* 0xADDR` header); sizes are `port/decomp/prage.functions.csv` bytes (or
insns where marked). **The C3g list is therefore**: the nineteen C3f addresses not measured here —
the fifteen remaining List A text/tree addresses (`0x38D90`, `0x38FEC`, `0x2F20C`, `0x2F4BC`,
`0x2F4D0`, `0x38C5C`, `0x38ED0`, `0x2EFD4`, `0x2F0F0`, `0x2F198`, `0x2F280`, `0x2F314`, `0x2F830`,
`0x2EF24`, `0x2F5A0`; `0x1C500`, `0x474E4`, `0x1E75C` and `0x1E808` are rowed here) and the four
remaining List B addresses (`0x1A7CC`, `0x2A690`, `0x37178`, `0x3BDDC`) — plus the twelve ported
addresses the `0x3CF38` row exposes (`0x3C6A8`, `0x3CD44`, `0x3CE58`; `0x34E2C`, `0x3C600`,
`0x3CBC4`, `0x3CC58`, `0x3CCEC`, `0x3CE24`, `0x4CE70`; `0x1922C`, `0x3CD94`) — **31 ported
addresses in total**, with `0x32BAC` named.

**Verdict.** After this batch the frontier is **not** just the named non-rows: it is the 31 ported
addresses above. The named non-rows stay: `0x2EA30` (the inert interrupt lock), `0x1B544` (host
`res_resolve`), `0x5D812`/`0x29D60`/`0x2EA64`/`0x32BAC` (bare stubs/rets), `0x500BB` (the DPMI
clock), `0x5DEED` and the AIL wrappers `0x5DC0F`/`0x5DC8B`/`0x5DD03`/`0x5DEAF`. Everything else the
scan reaches is runtime (`>= 0x5D000`). The rows that stay open on them: `string_unlock`
(`0x500BB`), `hit_chain_resolve` (`0x32BAC` plus the C3g tree), and the seven rows the deferred
first wave blocks (`fighter_input_mask`, `pset_write`, `fighter_379c4`, `fighter_39040`,
`fight_command_map`, and the two this batch closed — `fighter_state_36bc8`, `fighter_3b080`,
`fighter_4f434` are now closed).

**What a C3g would need, precisely:** (a) the rows for the 31 ported addresses above, each with a
`Spec`, binding, mutants and exact-set pins, exactly as this batch's rows, in the wave order (a row
closes when its listed callees have rows); (b) seams for the in-batch callees that are stubbed
(the `0x38D90` row's `0x38C5C`/`0x1C500`/`0x2F4BC`/`0x2F4D0`/`0x2F20C` are seamed now; the C3g
tree's `0x3C600`/`0x3CBC4`/`0x3CC58`/`0x3CE24`/`0x4CE70`/`0x1922C`/`0x3CD94` need theirs); and
(c) a re-measure of this section's table after each wave.

## §C3f.8 Results (the executed tree)

The plan's Tasks 2-4 were executed on `reverse-c3f` at the base `main` `0aa5eff` (= C3e merged):
the plan+record commit `ca789bd`, Task 2 `b6dd747` (the thirteen rows, the two corrections, the
seams/exports and the host-libc wrappers), Task 3 `3de0fa0` (the review sweep: the `c3f_38b_case`
sentinel and the `fighter_38bc8@word` case-set re-pin), the fix wave `96f5d99` (the `host_memset`
pointer, the signed `outlen` compare, the `0x14` buffer, the `c3f_unlock_case` seed) and this
closure commit (its sha is in the batch report named below). Every row was re-measured in the tree;
the planner's prototype values held.

| state | diff-verify counter | E2 |
|---|---|---|
| base `0aa5eff` | `278/278 functions VERIFIED; 1031/1031 mutants detected; 1 named gaps; 196/217 rows with callees closed (61 have none)` | `targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30`; voice `0 / 115 / 19` |
| final (`96f5d99` + this closure commit) | `291/291 functions VERIFIED; 1087/1087 mutants detected; 1 named gaps; 205/225 rows with callees closed (66 have none)` | byte-identical |

The task gates, measured on this tree (Task 1's baseline in `verify-base.log`; Task 2's, Task 3's
and the fix wave's re-measures in the ledger directory):

```
diff-verify: 291/291 functions VERIFIED; 1087/1087 mutants detected; 1 named gaps; 205/225 rows with callees closed (66 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

`make entry-triage` is byte-identical (no ported function, no `fn_register`); `python3 -m unittest
tools.tests.test_diff_verify` 110 tests OK on the final tree (the `make diff-verify` phase-1
invocation runs 175 across `test_diff_emu` + `test_diff_verify`); `PR_ORACLE_REQUIRED=1
./build/run_tests` `all checks passed`; `symbols.h` regeneration is byte-identical; `python3
tools/port_progress.py` stays `771 1203 64` / `731 731 100`; README untouched.

**The closure-value accounting.** +13 functions (the thirteen rows), +56 mutants
(3+3+3+4+3+4+3+4+4+7+4+6+8); rows with callees 217 -> 225 (the eight with-callee rows `38bc8`,
`fight_attack_ready`, `fighter_state_ok`, `string_unlock`, `string_decode`, `game_string_get`,
`fighter_block_anim`, `hit_chain_resolve`), no-callee 61 -> 66 (the five: `38bb0`, `3b038`,
`46534`, `fighter_input_scan`, `string_lock`), closed 196 -> 205 (+9): the six new rows that close
(`38bc8`, `fight_attack_ready`, `fighter_state_ok`, `string_decode`, `game_string_get`,
`fighter_block_anim`) and the three existing rows the new leaves close — `fighter_state_36bc8` (on
`0x38BB0`/`0x38BC8`), `fighter_3b080` (on `0x3B038`) and `fighter_4f434` (on `0x46534`).
`string_unlock` and `hit_chain_resolve` stay open on the named `0x500BB` and the C3g tree
respectively.

**The review sweep and the fix wave.** Task 3's store sweep found one store its fixtures could not
observe — `fighter_38bc8`'s side-1 word at `0x38BCC` (`0x107D26/0x107D27`, pre/post `00 00`) — and
`c3f_38b_case` now seeds `0x107D24..28 = 1234 5678 9ABC` (the C3b pattern), so a dropped or
too-wide side-1 store fails the row; the measured catch set of `fighter_38bc8@word` moved `['b0']`
-> `['b0', 'b1']` (`@off` `['b1']`, `@clear` `['b0','b1']` and all kinds unchanged; the §C3f.3
copy is synced by this closure commit). The fix wave then (a) made `host_memset` take its
`u8 *dest` (seam args unchanged; codegen proof in §C3f.2 correction 3), (b) made the
`string_decode` outlen compare the raw's signed `(s32)len < (s32)outlen` (`0x47556`), (c) grew
`0x2EFD4`'s format buffer to `0x14` (the raw's `sub esp,0x14` at `0x2EFD7`), (d) fixed
`c3f_unlock_case`'s seed (`+0x15 = flags`, `+0x14 = 0x02`) and (e) replaced the unreproducible
`hlt`-at-`0xFF3C` note with the probe §C3f.2 correction 3 carries and §C3f.5 cites.

**The corrections' neutrality.** The batch's behavioral changes are the `0x1E808` +0x10 store
(§C3f.2 correction 1) and the `0x474E4`/`0x1C500` restructuring (correction 2); the full closure
ladder — the 45 oracle lines equal to the k7-k12 baseline, every gp ratchet at its pin and the WAV
`cmp`-equal to `before-t2.wav` — is the evidence that no oracle/gp-visible path moved, not that no
driver path reaches them.

**The tail-verdict re-measure (Task 4 Step 2).** On the final tree none of §C3f.7's 31 frontier
addresses (the nineteen deferred C3f — the fifteen List A/tree plus the four List B — and the
twelve the `0x3CF38` row exposes) has a `Spec`: the final `tools/diff_verify.py`'s 265 `Spec`
entries were scanned against the list; the thirteen rows' unrowed callees and every row's frontier
addresses are those §C3f.7 names. `0x32BAC` stays the named non-row (a one-byte RET cannot have a
row). The list stands unchanged.

**The full gate** on this closure commit (the plan's parallel-safe overrides, `T=c3f`; log
`/tmp/pr_c3f_final.log`): the run's exact lines are recorded in the batch report
`.superpowers/sdd/2026-10-05-reverse-c3f-frontier-rows-6/task-4-report.md`. The plan's expected
values are `EXIT=0`, the 45 oracle lines equal to the k7-k12 baseline (`ORACLES-EQUAL`), the WAV
`cmp`-equal to `before-t2.wav` (`WAV-SAME`; sha256
`df74acfb65d345fb72cb214102089f2a0ab8d4b271ddc17e2a5f5c4f1a380844`), every gp ratchet at its pin
(gp-idle-loss 2064/8320; gp-u5-charsel 516/1513; gp-u6-moves-b 2139/3248/3248; gp-keys 11 effects;
gp-twop 612/1506/1506; U8 RA 1072/2274, LT 1076/2338, RT 1098/2402, TW 1107/2466, HC 1022/2274,
EN 278/1174, AS 1087/2018; gp-u9-win 346/3503, path 8, win 3503; gp-u10-ending 331/9954, path 30,
win 9954) and `symbols.h` idempotent.

**The C3g tail is the next batch**: §C3f.7's 31 addresses with the measured sizes, dependency order
and seam notes; `0x32BAC` stays named and nothing else is open after C3f beyond the standing named
non-rows and the earlier batches' gaps.
