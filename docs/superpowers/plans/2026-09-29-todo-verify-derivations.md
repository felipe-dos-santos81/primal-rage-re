# The `TODO(verify)` sweep — raw-byte derivation (Task 2 of `2026-09-29-all-gaps.md`)

**Scope.** One section per `TODO(verify)` site in the ledger's §C
(`2026-09-29-all-gaps-ledger.md`), numbered as the ledger numbers them. Each
section states the doubt, the raw that settles it, and the verdict:

* **resolved-as-is**: the raw confirms the port's behaviour; the marker becomes
  a plain or `PORT:` statement citing this section. A behaviour-neutral
  structural correction (a signature or a no-op call site) is noted, and has no
  test because nothing observable changes.
* **resolved-with-fix**: the raw contradicts the port; the port is fixed, and a
  test seeded with a sentinel that differs from the post-condition fails
  before the fix (RED) and passes after it (GREEN). Every fix is
  mutation-proven (report: `.superpowers/sdd/2026-09-29-all-gaps/task-2-report.md`).
* **named-gap-with-evidence**: the raw pins the behaviour but the port does not
  reproduce it, or no raw/capture can decide the question. The marker becomes a
  `PORT:` named gap (a known deviation) citing this section.
* **PORT-reclassified**: a host-only doubt with no raw counterpart.

**Tooling.** The Ghidra MCP bridge was not reachable (no tool exposed). Every
address was read from a Python mirror of `mem_load_le` + `mem_load_le_fixups`
(`port/src/mem.c`), so each dword has the LE fixups applied, disassembled with
capstone (32-bit); `port/decomp/prage.c` (Ghidra) was used for bodies.
`SBPRO2.MDI` was disassembled with capstone in 16-bit mode at file offsets (the
same addressing `port/spec/audio.md` uses for it, e.g. its E0 handler
`0x3542`). The XMIDI controller counts come from `tools/opl_seq.py`'s
`find_evnt`/`decode_events` run over every `FORM XMID` in `data/game/C/*`.

**Oracle gate.** After each batch `make verify` exited 0 and every oracle line
equals ledger §A (only the Python unittest timings differ).

---

## Platform and audio (§1–§19)

### §1 `host.c:26` — which vector installs `0x2D62C`? — resolved-as-is

None. `0x1CF40` (the audio init, called once from `0x1BEC4` at `0x1BFDB`)
registers `0x1BDF4` as an AIL timer callback: `0x1CFED push 0x1bdf4`,
`0x1CFF2 call 0x5DA12` (`AIL_register_timer`), `0x1CFFA push 0x3c` /
`0x1CFFF call 0x5DA87` (`AIL_set_timer_frequency`, 60 Hz), `0x1D008 call
0x5DAA6` (start). `0x1BDF4` is the tick: unless the byte `DS_00104B22` is 1
(`0x1BDF8 mov al,[0x104b22]; cmp eax,1; je 0x1be2d`) it increments
`DS_00101508`/`DS_00101500` (`0x1BE0E..0x1BE16`), calls the sampler `0x1BBAC`,
increments the word `DS_000EF6DE` (`0x1BE21`) and calls `0x2D62C` at
`0x1BE28` (`inc dword [0x105d88]; ret`). The dword `0x2D62C` at `0x1BF5C` is
`push 0x2d62c` for `0x109A0` (`0x1BF60`), which is `0x10830(addr, addr+size)`,
the DPMI lock of the ISR's code, not an install (`0x1BF6D` locks `0x1BDF4` the
same way). The port's `g_tick` ignores the `DS_00104B22` gate; that is
unobservable because `DS_00105D88`'s only readers are the lock (`0x1BFA4`) and
the host-owned run clock `0x32970` (ledger §B row `0x2D62C`). The comment is
rewritten as a `PORT:` statement.

### §2 `host.c:282` — SDL audio-open failure branches untested — PORT-reclassified

The branches (NULL from `SDL_OpenAudioDeviceStream`, a failed
`SDL_ResumeAudioStreamDevice`) are host behaviour. The original's device probe
is the AIL driver install, which the port replaces with a fixed profile
(`ail.c` "PORT: fixed audio profile"), so no raw byte or capture can pin what a
host without an audio device does. Rewritten as `PORT:`.

### §3 `gfx.c:83` — the palette flush clamp and zero count — resolved-with-fix

Raw `0x1C470`:

```
1c48b mov ecx,[ebx+4]      ; first (dword)
1c48e add ecx,[ebx+0xc]    ; + the flag dword, not the count
1c491 cmp ecx,0x100
1c497 jle 0x1c4a2          ; signed
1c499 sub ecx,0x100
1c49f sub [ebx+0xc],ecx    ; the excess comes off the flag dword
1c4a2 mov edx,0x3c8
1c4a7 mov eax,[ebx+4] / 1c4aa out dx,al     ; DAC write index = low byte of first
1c4ad cmp byte [ebx+0xc],0 ; handle test on the (clamped) flag's low byte
1c4bd mov esi,[ebx+8]      ; count, unclamped
1c4c5..1c4d5               ; three out 0x3c9 per word
1c4d6 dec esi / 1c4d7 jg 0x1c4c5            ; do-while
```

So the count is never clamped, a count below 1 still writes one entry, and the
DAC's 8-bit write index (set once at `0x1C4AA`) auto-increments and wraps past
`0xFF`. The writers store only the flag's low byte: `0x33734`
`0x3373F mov byte [eax-4],0` and `0x33714` `0x3371F mov byte [eax-4],1`; the
list init `0x336C0` marks only `+4 = -1` (`0x336E9`) and never clears `+0xC`.
On the shipped image the upper three bytes are BSS zero and the flag is 0/1, so
the clamp fires only for a first of `0x100` or more.

Fix: `gfx_flush_palette` clamps first+flag into the flag dword, tests the
flag's low byte, runs the loop as a do-while over the unclamped count with the
`u8` index wrapping (PORT: a bound at the end of `mem[]` instead of reading
past the host buffer); `palette_record` stores the flag's low byte. Tests
(`test_gfx`): the wrap (entries 0/1 take words 3/4 of a first-`0xFE` count-8
record; two existing assertions changed from "no wrap" to the raw's wrap), the
zero-count write, the flag-dword clamp (`0x100 + 0x200 -> 0`), the byte store
(`0xAABBCC00 -> 0xAABBCC01`). One existing `test_game` assertion that
`palette_list_init` leaves `+0xC == 0` over a `0xDEADBEEF` sentinel now
expects `0xDEADBE00` (the raw's byte store). Oracles unchanged.

### §4 `gra.c:93` — the "negative dimension" sentinel — resolved-as-is

`0x14268` (the sprite-node build) reads the descriptor `{s16 w; s16 h; s16 xorg;
s16 yorg; u32 pixels}`: `0x1428F mov si,[eax+2]; test si,si; jge 0x142b7`. A
negative height makes the node type 2 with width and rows negated
(`0x1429A..0x142B2`: `sar esi,0x10; neg esi` -> `+0x14`; `movsx eax,word
[eax]; neg` -> `+0x18`; `mov [ebx+0x10],2`), a non-negative one type 1. Type 2
is the uncompressed blit (`sprite.c` `case 0x02: sprite_render_raw`, table
`PTR_LAB_00080C8C`). The span blit `0x51E5C` returns at once for a zero width
or row count (`0x51E5C test [eax+0x18],-1; je`, `0x51E65 test [eax+0x14],-1;
je`). So `0xFEC0` (-320) is a 320-wide uncompressed bitmap, not a "blit/clear
sentinel". `gra.c`'s decoder is the port's inspection decoder (the game draws
through `sprite.c`, which already implements type 2); its rejection of
non-positive dimensions stays, and the comment now says what they are.

### §5 `gra.c:104` — which DAC bank a sprite selects — resolved-as-is

None: the sprite descriptor carries no palette. `0x14328` copies the pset's
`+0x18` (the `0x33754` palette-table entry) into the node at `0x1435D`; the
blit `0x51E5C` indexes `DS_00081310` by that entry's `+8` byte
(`0x51E86 mov ebx,[ebp+0x1c]; 0x51E89 mov eax,[ebx+8]; and eax,0xff;
0x51E91 mov ebx,[eax*4+0x81310]`), which `0x33754` set to the DAC start it
uploaded the resource at. Returning the bank flat is correct.

### §6 `gra.c:151` — meaning of the s16 x/y anchor — resolved-as-is

The sprite's origin inside its box. `0x14268` copies `word[+4]`/`word[+6]` to
node `+8`/`+0xC` (`0x142D2..0x142E6`), mirrors x as `width - x - 1` on hflip
(`0x142F2..0x142FF`), and `0x14328` subtracts them from the projected point
(`0x143E4 sub ecx,[esp+8]`, `0x143F3 sub ebx,eax` with `eax = [esp+0xc]`),
the node being at `esp` (`0x14353 mov eax,esp; call 0x14268`). The port's
`render_list` does the same.

### §7 `mixer.c:15` — voice exhaustion — resolved-as-is

In AIL a sample handle is one voice. `0x1CF40` sets preference 4 to 4
(`0x1CF62 push 4; push 4; call 0x5D87E`); `AIL_set_preference`
(`0x65B43`) stores `DAT_00108D64[4]` = `DAT_00108D74`; the DIG install
(`FUN_00067730`) reads it at `0x67B74` into the driver's `+0x60` and allocates
that many `0x894`-byte SAMPLE structures ("Could not allocate SAMPLE
structures"). `0x1CF40` then allocates exactly four handles
(`0x1CF99..0x1CFBD`, `esi` 0..0x60 step 0x18). Exhaustion is only
`AIL_allocate_sample_handle`'s "Out of sample handles" (`0x67E60`), which
`ail.c` reproduces. A start on a playing handle restarts that handle. `ail.c`
stops a handle's voice before adding it (`AIL_start_sample`), so at most four
voices exist and the mixer's drop is unreachable. Rewritten as `PORT:`.

### §8 `ail.h:151` — the name of AIL row 21 (`0x5DD2C`) — PORT-reclassified

The AIL library is statically linked with no symbols; a `strings` scan of
`PRAGE.EXE` finds AIL error strings ("Out of sample handles", "Could not
allocate SAMPLE structures") but no API names, exports or trace formats. No
raw byte can name the entry, so the name is the port's own (`PORT:`). The
behaviour is in `port/spec/audio.md` row 21. The third argument is the 0..3
format code `0x1013C` derives from its two flags (`prage.c` `FUN_0001013c`), so
the parameter is renamed `len` -> `format` (no behaviour; the stub is inert).

### §9 `mixer.h:15` — as §7 — resolved-as-is

### §10 `ail.h:156` — the name of row 22 (`0x5DD5D`) — PORT-reclassified (as §8)

### §11 `ail.h:161` — the name of row 23 (`0x5DD86`) — PORT-reclassified (as §8)

### §12 `mixer.h:46` — `MIXER_OPL_RATE` tied to the vendored core — resolved-with-fix

Not a raw question: the constant mirrors `OPAL_OPL3_SAMPLE_RATE` (49716,
`opl/opal/opal.h:49`). Fix: `opl.c`, the one file that sees both headers,
`_Static_assert(MIXER_OPL_RATE == OPAL_OPL3_SAMPLE_RATE, ...)`. The test is the
build: setting `MIXER_OPL_RATE` to 49717 fails compilation with "static
assertion failed due to requirement '49717 == 49716'".

### §13 `sequencer.c:264` — ctrl 64 and the driver's `0x3B1E` — named-gap-with-evidence

`SBPRO2.MDI` (file offsets): ctrl 64 stores the value at `[ch+0x1969]` and,
below `0x40`, calls `0x3B1E(ch)` (`0x3C7D mov [di+0x1969],cl; 0x3C81 cmp
cl,0x40; jl 0x3c89; 0x3C8A call 0x3b1e`). Ctrl 121 zeroes it and calls
`0x3B1E` too (`0x3CC0..0x3CC6`). `0x3B1E` walks the 20 voices and, for each
active voice on `ch` whose held flag `[v+0x1525]` is set, calls the note-off
`0x39CC`. `0x39CC` (the `0x80` handler, `0x3BA6`, and ctrl 123's per-voice
walk, `0x3C9A..0x3CB1`) holds a voice instead of releasing it while the
channel's sustain is `0x40` or more (`0x39F6 cmp byte [bx+0x1969],0x40; jge
0x3a23; 0x3A23 mov byte [si+0x1525],1`). The port stores the value only.

Reachability: of the 26 shipped XMIDI sequences only `S16KONSD.GRA` (4 events)
and `S16SOUND.GRA` (2) send ctrl 64; `S16TITLE.GRA`, the only bank the port
plays (`flow.c` `title_music_bank`), sends none, so no oracle can check an
implementation and none of the port's playback reaches it. The behaviour is
fully pinned above and becomes a live gap when the stage music is wired.
Rewritten as a `PORT:` named gap.

### §14 `sequencer.c:427` — the "RBRN loop range" — resolved-as-is (plus a named gap)

The end meta halts in the raw too. In the service `0x69372` the `FF 2F` arm
restarts the stream only while the sequence's loop count `seq[10]` is 0
(forever) or is non-zero after its decrement; otherwise it calls the end
routine `0x5DE94` and the callback (`prage.c` `FUN_00069372`, the `== 0x2f`
branch). `AIL_init_sequence` (`0x6A410`) sets `seq[10] = 1`, and the game's only
sequence path `0x1C930` calls just `0x5DEAF` (stop), `0x5DE48` (init),
`0x5DECA` (volume) and `0x5DE79` (start), never a loop-count setter; so
`1 - 1 = 0` ends the sequence at `FF 2F`, as the port's `halt()` does.

Named gap found on the way: AIL's own FOR/NEXT loop controllers are handled in
`FUN_00068AB0` before the driver: 116 pushes `(count, stream position)` on a
four-deep stack (`param_1[0x20..0x23]`, `[0x1c..0x1f]`), 117 at `0x40` or more
pops/decrements and jumps back (count 0 loops forever). The port passes them to
the driver dispatch, whose default ignores them. `S16TITLE.GRA` sends one 116
and no 117, so the title never loops; over all shipped banks 116 appears 24
times and 117 17 times (every bank with a 117 — `S16BEACH`, `S16CAVES`,
`S16CITYS`, `S16CONTI`, `S16GRAVE`, `S16HGHSC`, `S16HIMAL`, `S16JUNGL`,
`S16SELMO`, `S16SND2`, `S16SOUND`, `S16STONE` — also has a 116). Recorded as a
`PORT:` named gap at the controller dispatch.

### §15 `sequencer.h:22` — the MDI driver's `+0x2E` rate — resolved-as-is

`SBPRO2.MDI`'s header `+0x2E` is `0` in the file, and the driver's only store
to it is `0x3DBB mov word [0x2e],0xffff` (every offset of the file was
disassembled; no other instruction references `[0x2e]`), in the routine that
fills the header's I/O fields (`[0xc]`, `[0x10]`, `[0x12]`, `[0x126]`). In
`0x65B7B`, after the `0x300` call, `*(short*)(hdr+0x2e) < 1` registers no
driver timer. The XMIDI service timer is the MDI install's: `0x6A29B mov
edi,[0x108d8c]` (preference 10) `-> 0x6A2A9 call 0x5DA87`. `AIL_startup`'s
defaults (`0x6603E`) set preference 10 to `0x78` (`0x660CF push 0x78; 0x660D1
push 0xa; 0x660D3 call 0x5D87E`), and the game's preference writes are only
`(4,4)`, `(1,0x2B11)`, `(3,0x14)`, `(0xB,1)` (`0x1CF40`) and `(7,1)`
(`0x10034`). So the tick is 120 Hz from the raw, matching the capture fit.

### §16–§18 `ail.c:367/376/383` — as §8/§10/§11 (the definitions) — PORT-reclassified

### §19 `ail.c:431` — a failed re-init of a playing sequence — resolved-with-fix

`0x6A410`: `0x6A42E mov dword [esi+4],2` (status 2) runs before the data check
`0x6A435 call 0x687C0`; on failure it returns 0 with the status left at 2. The
service `0x69372` advances only a status-4 sequence (`if (seq[1] == 4)`), so
the sequence stops where it is: no note-off is sent and its keyed voices keep
sounding. The port kept the engine running and reported 4. Fix: a new
`seq_suspend()` (stop advancing, key nothing off), and the failure branch sets
status 2. Test `test_ail` §4d: a keyed voice, a bad re-init -> status 2, 40
ticks write nothing, the voice still sounds. Reachability: `0x1C930` always
stops before it inits, and every shipped bank is valid, so the game never takes
this branch.
