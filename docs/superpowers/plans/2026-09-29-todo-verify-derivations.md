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

---

## Game (§20–§31)

### §20 `flow.c:1002` — can an unregistered `DS_00104AE4` hook reach `0x4F9A0`? — resolved-as-is

No. Every instruction that stores to `DS_00104AE4` was found by scanning the
code object for the fixed-up dword `0x00104AE4` and decoding each hit: 42
stores, all `mov dword [0x104ae4],reg`, each fed by a `mov reg,imm32` at most 34
bytes before it in the same block; no `mov dword [..],imm32` form exists, and
the static dword is 0. The 19 distinct values:

| value | stored at (first sites) | port |
|---|---|---|
| `0x430E8` | `0x1F447`, `0x43AE8`, `0x43C18`, `0x44824` | registered (`actors.c:627`) |
| `0x259CC` | `0x253AE`, `0x417A5`, `0x418D5`, `0x423C4` | registered |
| `0x4367C` | `0x25810` | registered |
| `0x25BBC` | `0x25A46`, `0x25A73`, `0x285CB`, `0x285F3` | registered |
| `0x10E80` | `0x25B6E` | registered |
| `0x24B54` | `0x25B97` | registered |
| `0x26998` | `0x2698B` | registered |
| `0x27134` | `0x27074`, `0x27233` | registered |
| `0x270BC` | `0x2715C` | registered |
| `0x29B74` | `0x278A4`, `0x2862A`, `0x2898F`, `0x28AB9`, `0x28B57` | registered (`actors.c:615`) |
| `0x4142C` | `0x28BC6` | registered |
| `0x43738` | `0x28D73`, `0x28D95` | registered (`actors.c:359`) |
| `0x28D80` | `0x28E60` | registered (`actors.c:621`) |
| `0x25AE8` | `0x296AF`, `0x42ED9` | registered |
| `0x26978` | `0x41854` | registered |
| `0x28D68` | `0x42D04`, `0x42D40`, `0x42D8F`, `0x42FBD` | registered (`actors.c:620`) |
| `0x430C0` | `0x43284` | registered |
| `0x29D60` | `0x43805`, `0x44541` | a bare `ret`: a miss is exact |
| `0x5D812` | `0x25C13`, `0x26A36`, `0x2712C`, `0x29630` | `xor eax,eax; ret`: a miss is exact |

The comment listed 13 of the 17 non-trivial values; the other four
(`0x29B74`, `0x43738`, `0x28D68`, `0x28D80`) are registered too.

### §21 `flow.c:5772` — `0x1CF40`'s `param_1`/`param_2` gates — resolved-as-is

`0x1CF40` tests DL at `0x1CF5E` (the DIG install and its preferences) and the
saved AL at `0x1CFBF` (the MDI install). Its only caller is `0x1BEC4`
(`prage.calls.csv`; the only `call rel32` to it in the code object), which
passes `0x1BFD1 mov edx,1; 0x1BFD9 mov eax,edx; 0x1BFDB call 0x1cf40`. Both
gates are always taken, so the port's unconditional installs are exact.
Rewritten as `PORT:`.

### §22 `flow.c:5828` — the sound-table id to resource mapping — resolved-as-is

`DS_000BBDC8` is in the image and `sound_voice` already reads it. Scanning its
records for case 1 with a handle in resource 7 (`S16TITLE.GRA`, INDEX entry 7)
finds only ids `0x54` and `0x56`, both handle `0x03836102` = resource 7 +
`0x36102`. There the file holds the size dword `0x1338` (4920) and then the
XMIDI file: `FORM XDIR` at `0x36106`, `CAT XMID` at `0x3611C`, `FORM XMID` at
`0x36128`. `0x1C930` copies the `size` bytes that follow the size dword and
`AIL_init_sequence` finds the sequence in them; `title_music_bank`'s scan
returns the same `FORM XMID` at `0x36128`. So the port's bank is the table's.
What remains unported is the request that names id `0x54`/`0x56` (the title
state's music trigger, already a named port choice at `flow.c`
`game_state_title`).

### §23 `flow.c:5900` — the sample loop flag — resolved-with-fix

`0x1CB18` calls `AIL_set_sample_loop_count(h, 0)` only when the slot's loop
byte is 1 (`0x1CBCA mov al,[ebp+0x102868]; 0x1CBD3 cmp eax,1; jne`;
`0x1CBD8..0x1CBE1`). `0x1CC28` queued that byte from the voice record's `+8`
(`0x2C3FC` case 2). AIL's loop count is a count, not a flag:
`AIL_init_sample`'s body sets `+0x30 = 1` (`0x67F4B`); `0x5DCE4` -> `0x68050`
stores it; at the buffer end the DIG service `0x6F120` restarts a count-0
sample forever (`0x6F289 cmp [ecx+0x30],0`), stops a count-1 one
(`0x6F28F cmp [ecx+0x30],1`, status 2) and decrements a larger one
(`0x6F295 dec [ecx+0x30]`). The announcer the port plays is voice id `0xCD`
(`DS_000BBDC8[0xCD]` = case 2, handle `0x02824B0F` = `S16SOUND.GRA` +
`0x24B0F`, the RIFF blob with 19327 8-bit frames at 11025 Hz, loop byte 0), so
the raw plays it once through the default count 1.

The port had both halves inverted: `ail.c` treated the count as a loop flag
(default 1 = loop) and `game_sample_play` always set 0 (one-shot), which
cancelled out audibly. Fix: `AIL_start_sample` loops only a count-0 sample
(PORT: a count above 1 plays once; the game never sets one), and
`game_sample_play` sets 0 only when id `0xCD`'s loop byte is 1. Tests:
`test_ail` (a 1-frame sample: the default plays once, count 0 repeats; the
tone handle in §6 now uses count 0 to keep looping, an argument change) and
`test_flow` (id `0xCD`'s handle, and no voice left after ~98k output
frames).

### §24 `flow.c:6417` — the credit countdown under input — named-gap-with-evidence

The decrementers `0x2CA48`/`0x2CA7C` are ported and unit-tested against the
raw (`test_game.c`, `config_credit_take`/`config_credit_spend`). What is open
is oracle coverage: the three captures (`data/title-captures/title`, `title2`,
`frontend`) have no coin input. A DOSBox-X capture with coins inserted would
cover it. Rewritten as a `PORT:` named gap.

### §25 `fight.c:1586` — meanings of slot `+0x5A/+0x5D/+0x5E/+0x63` — resolved-as-is

A naming doubt; `0x1D540` is transcribed per instruction. The roles from the
writers: `+0x5A` is meter A's target, the health (`0x36E90` sets `0x78 -
[+0x5B]`, `0x33B85` restores it; `0x1D56A` steps meter `DS_0010290C` toward it
and `0x1D2F0` draws it); `+0x5D` is meter B's target (`0x36C87` sets `0x44`,
`0x33D9C`/`0x36EA3`/`0x36E10` clear it; `0x1D5AD` steps `DS_0010290E` toward it,
`0x1D464` draws it; `0x1D6D1..0x1D71F` decay it); `+0x5E` is the timer that
paces that decay (`0x1D5E6` increments, `0x1D6A8`/`0x1D6D1`/`0x1D711` set or
clear it); `+0x63` is set by the character select (`0x41385`) and cleared at
`0x34EFC`/`0x39C7F`.

### §26 `config.c:112` — the raw EDX at `0x2DACA`/`0x2DAD4` — resolved-as-is (signature correction)

`0x2D4EC` takes EAX only: `0x2D4EE push edx` then `0x2D4F5 mov edx,eax`
overwrites EDX before any read, and the pushed value is restored on exit. The
EDX values at the call sites cannot matter. The port's `config_storage_touch`
lost its phantom second parameter; behaviour-neutral (the function is a
declared no-op), so there is nothing observable to test.

### §27 `config.c:200` — the three omitted `0x2D4EC` calls — resolved-as-is (call sites added)

`0x2D6F8`'s defaults arm ends `0x2D89C cmp byte [0x2d490],0; je 0x2d910;
0x2D8A5 mov eax,1; call 0x2d4ec; 0x2D8AF mov eax,2; call 0x2d4ec; 0x2D8B9 jmp
0x2d909` -> `0x2D909 xor eax,eax; call 0x2d4ec`. The other arm (`0x2D834 je
0x2d8bb`) reads the image through the deferred `0x2E990` and reaches
`0x2D909` only when that read fails with `DS_0002D490` set. The port now makes
the defaults arm's three no-op calls under the same gate; the other arm is a
`PORT:` note (no stored image). Behaviour-neutral, no test.

### §28 `fighter.c:155` — character above 6 reads caller registers — resolved-as-is

`0x18428`: `0x1842F mov al,[eax+0x7a]; 0x18432 cmp al,6; 0x18434 ja 0x18408;
0x1843B jmp cs:[eax*4+0x1840c]`. The fixed-up table `0x1840C` holds seven
entries, all `0x18408`, which is `ret`. So `0x18428` has no effect for any
input, and whatever the register-dependent `[lo, hi)` test in `0x18460`
decides for a character above 6 changes no state. The port's return value is
port-only. Rewritten as `PORT:`.

### §29 `fighter.c:2297` — `DSD(0x1078DC)` indirection at `0x374E3` — resolved-with-fix

`0x37464`: `0x374E3 mov edx,[0x1078dc]` (the pointer), `+ 14*char`
(`lea eax,[edx*8]; sub eax,edx; add eax,eax`), `+ 2*other char`, then
`0x37502 mov si,[edx]` (and in the other arm `0x3751D mov edx,[0x1078dc]` ...
`0x37534 mov cx,[edx+eax*2]`). `0x36F10` stores the table `0xBD89C` at
`0x37043`, and `0x37D7B` stores it too. The port read the word at `0x1078DC`
itself. Fix: `DSW(DSD(DS_001078DC) + …)`. The `check_gap_handlers` D seeds
move (the pointer `0x3F54000` whose own low word `0x4000` differs from the
table word `0x10`); its assertions are unchanged, and with the old read they
fail (RED: `256 != 240`, `0 != 9`).

### §30 `fighter.c:3883` — load order of slot `+0x2C` vs `0x2BC30` — resolved-with-fix

`0x3C480`: `0x3C4A8 mov ebx,[esp+0xc]` (slot, after the push) `; 0x3C4B0 mov
ebx,[ebx+0x2c]; 0x3C4B3 call 0x2bc30; 0x3C4BB mov edx,ebx; 0x3C4BD call
0x188dc`. `0x2BC30` preserves EBX (`0x2BC30 push ebx`). So the value passed to
`0x188DC` is `+0x2C` from before the animation start, which can change it
when the stream's opening opcodes run code (op 0x10/0x11/0x15 indirect calls).
Fix: load before `actors_anim_begin`. Test `check_block` H5: a stream whose
first word is an indirect call (`0xD000` + a 32-bit code address) to a test
hook registered at `0x00F0F000` (outside both LE objects) that rewrites slot
0's `+0x2C` to `0x7777`; with `+0x42` bit 3 set `0x18714` returns the record's
`+0x18` (`0x12340`), and `+0x2C` must end as that (RED: `30583 != 74560`).

### §31 `fighter.c:11005` — the immediate `0x1F874610` — resolved-as-is

It is a resource handle (`index << 23 | offset`), so "no LE fixup" is expected.
Index `0x1F874610 >> 23 = 63`; INDEX entry 63 is `s16spift.gra`, 478720 =
`0x74E00` bytes, and the offset `0x74610` lies inside it. The bytes there are a
palette block: the count dword `0x1F` (31) followed by 31 colour words
(`0x00272727`, `0x00212121`, …). The port already passes it to
`actor_pset_palette` as the raw does (`0x45B94..0x45BA4`), so the acquire
records a 31-colour palette, not an empty entry. Only the comment was wrong.

---

## Verdict tally

| verdict | sections |
|---|---|
| resolved-as-is | §1, §4, §5, §6, §7, §9, §14, §15, §20, §21, §22, §25, §26, §27, §28, §31 (16) |
| resolved-with-fix | §3, §12, §19, §23, §29, §30 (6) |
| named-gap-with-evidence | §13, §24 (2) |
| PORT-reclassified | §2, §8, §10, §11, §16, §17, §18 (7) |

New named gaps found on the way: AIL FOR/NEXT loop controllers 116/117 (§14),
the `0x1BDF4` tick gate on `DS_00104B22`, which the host tick ignores
(unobservable, §1), and `0x2D6F8`'s non-defaults arm (§27, deferred with
`0x2E990`). Frame dumps of the front-end/demo-fight/cycle-2 run (7386 files)
and the attract/title run (887 files) are byte-identical before (`1a287b0`)
and after the whole sweep.
