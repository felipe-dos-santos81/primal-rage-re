# EEPROM/config validate `0x2D6F8` — raw-byte derivation (Task 3)

Register/write derivation for the no-storage path of `0x2D6F8` (`config_validate`),
`0x2CADC` (`config_set_defaults`) and `0x2CCD0` (`config_menu_default_bits`), read
from the shipped `data/game/C/PRAGE.EXE` with capstone (32-bit, GNU syntax).

Method, as in `2026-09-19-effects-producers-derivations.md`:

- object-0 code maps as `file_offset = va + 0x52E54`;
- a raw immediate `I` in obj-0 is linear `I + 0x10000`, in obj-1 is linear
  `I + 0x80000` (the LE fixup base; `mem_load_le_fixups` adds the object-table
  `rel` base). A `DS_*` symbol is already the linear address.
- The obj-1 file base is `0xC6E54` (`0x80000` runtime), so obj-1
  `file_offset = linear + 0x46E54`.

Reproduction (read-only):

```python
from capstone import *
d = open('data/game/C/PRAGE.EXE','rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32)
def dis(va, n):
    for i in md.disasm(d[va + 0x52E54: va + 0x52E54 + n], va):
        print(hex(i.address), i.bytes.hex(), i.mnemonic, i.op_str)
dis(0x2d6f8, 633)   # config_validate
dis(0x2cadc, 150)   # config_set_defaults
dis(0x2ccd0, 93)    # config_menu_default_bits
```

## Corrections to the controller's summary (raw wins)

1. **Menu table address is `0xA2EB4`, not `0x32EB4`.** `0x2CAF8  b8b42e0200  mov
   eax, 0x22eb4` (file `0x7F94C`) is an **obj-1** immediate: fixup record source
   `0x2CAF9` (obj-0) has target object 2, target offset `0x22EB4` → value
   `0xA2EB4` (`0x80000 + 0x22EB4`). The same value is fixed up at source
   `0x1D2D4`. Reading the obj-0 offset as runtime (`0x32EB4`) lands in code
   (`ffffff ff e8 db c2 ff ff …` at file `0x85D08`), not a record list: its
   presence word is `DSD(0x32EB4) = 0xFFFFFFFF`, so the walk would enter and
   dereference a bogus entries pointer rather than return `0`. The spec's
   "`0x22EB4` … obj-0 (`+0x10000`) → `0x32EB4`" is wrong; the raw immediate is
   obj-1.
2. **Record linkage is contiguous stride `0x14`, not a pointer chase.**
   `0x2CD17 8b4e14 mov ecx,[esi+0x14]` then `0x2CD1A 83c614 add esi,0x14` then
   `0x2CD1D 85c9 test ecx,ecx; jne` — the loop advances `ESI` by `0x14` and only
   uses the loaded value as a nonzero test. Since `[rec+0x14]` *is* the next
   record's `[+0]` after the add, this is exactly `rec += 0x14; while (DSD(rec)
   != 0)`. The brief's `rec = DSD(rec + 20u)` chases the next record's presence
   word (`0x1F3`, `0x1F4`, …) as a pointer and stops after one record. The raw
   walks ten records; the correct total `bits` is `0x00142095` (below).
3. The early-out at `0x2D754` returns `min(read1, read2)` in `EAX`
   (`0x2D75D..0x2D76A`), not a bare return. `config_validate` is `void`, so the
   value is discarded; the port returns plain. No behavioural difference.

Everything else in the controller's summary is confirmed by the raw.

---

## 1. Writes on the no-storage path

With the storage layer absent, `read1 = read2 = -1` (item 5), so execution takes
the magic-mismatch/defaults path. Every write, in order:

| address | instruction (file offset) | port effect |
|---|---|---|
| `0x2D70F` | `8825a55d0800 mov [0x85da5], ah` (`0x80563`) | `DSB(DS_00105DA5) = 0` |
| `0x2D715` | `8815a45d0800 mov [0x85da4], dl` (`0x80569`) | `DSB(DS_00105DA4) = 0` |
| `0x2D84F` | `880dd85d0800 mov [0x85dd8], cl` (`0x806A3`), after `80c906 or cl,6` | `DSB(DS_00105DD8) \|= 6` |
| `0x2D86F` | `882dd85d0800 mov [0x85dd8], ch` (`0x806C3`), after `80cd01 or ch,1` | `DSB(DS_00105DD8) \|= 1` |
| `0x2D881` | `a22f5e0800 mov [0x85e2f], al` (`0x806D5`), `al=0` | `DSB(DS_00105E2F) = 0` |
| `0x2D886` | `e851f2ffff call 0x2cadc` (`0x806DA`) | `config_set_defaults()` |
| `0x2D88D..0x2D89A` | loop `8888305e0800 mov [eax+0x85e30], cl; shr ecx,8` (`0x806E1..0x806EE`) | magic four LE bytes |
| `0x2D940` | `8835a75d0800 mov [0x85da7], dh` (`0x80794`), `dh=1` | `DSB(DS_00105DA7) = 1` |

After the defaults write, `DSB(DS_0002D490) != 0` (item 5) selects the three
`0x2D4EC` no-op calls (`0x2D8A5`, `0x2D8AF`, `0x2D909`); they are declared
no-ops (spec §7).

## 2. Exact magic test and rewrite

`0x2D807..0x2D820`: the loop consumes `eax=3,2,1,0`:

```
0x2D813  8a90305e0800   mov dl, byte [eax + 0x85e30]   ; file 0x80667
0x2D819  48             dec eax
0x2D81A  09d1           or ecx, edx                    ; after shl ecx,8
0x2D820  81f9c4d2949c   cmp ecx, 0x9c94d2c4            ; file 0x80674
```

so `ecx = b3<<24 | b2<<16 | b1<<8 | b0` where `bN = *DSB(DS_00105E30 + N)`,
i.e. `DSD(DS_00105E30)` little-endian. The branch condition
(`0x2D826 jne 0x2D82D`, `0x2D828 cmp esi,-1; jge 0x2D832`) sets `ebp=1` — the
defaults flag — when `(v != 0x9C94D2C4) || (read1 < -1)`. With `read1 = -1` and
`v = 0` this fires.

The rewrite (`0x2D88B..0x2D89A`) stores `0x9C94D2C4` as four LE bytes: `ecx`
starts `0x9C94D2C4`, and `0x2D88D 8888305e0800 mov [eax+0x85e30], cl; 0x2D894
c1e908 shr ecx,8` for `eax=0..3` writes `C4 D2 94 9C` at `DS_00105E30..+3`,
equivalent to `DSD(DS_00105E30) = 0x9C94D2C4u` on little-endian.

## 3. The two `0x61A70` calls

Called at `0x2D855` (file `0x806A9`) and `0x2D875` (file `0x806C9`), both between
the flag writes, both with `eax`/`ebx`/`edx` pointing at the config region
(`0x85DE1`/`0x53`, then `0x85DB0`/`0x26`). Disassembly of `0x61A70` (file
`0xB48C4`):

```
0x61A72  88d6     mov dh, dl
0x61A74  c1e208   shl edx, 8
0x61A77  88f2     mov dl, dh
0x61A79  c1e208   shl edx, 8
0x61A7C  88f2     mov dl, dh
0x61A7E  89d9     mov ecx, ebx
0x61A80  e80b3a0000 call 0x65490
```

It packs `dl` into a repeated-byte `edx` and tail-calls `0x65490`. It reads and
writes **no** config address — only the pset pool. Verdict: side-effect on the
unported pset pool, no config effect. Declared a no-op this cycle.

## 4. `0x2CCD0` record layout and `'*'` test

```
0x2CCD8  89c6           mov esi, eax          ; table
0x2CCDC  8b18           mov ebx, [eax]        ; +0x00 presence
0x2CCE1  85db           test ebx, ebx
0x2CCE3  743c           je end
0x2CCE5  8b4610         mov eax, [esi+0x10]   ; +0x10 entries pointer
0x2CCEE  8b38           mov edi, [eax]        ; entry[0] char*
0x2CCF0  0fbe3f         movsx edi, byte [edi]
0x2CCF3  83ff2a         cmp edi, 0x2a         ; '*'
0x2CCF8  b501           mov ch, 1             ; found
0x2CCFA  8a4e04         mov cl, [esi+4]       ; +0x04 shift (low byte)
0x2CD00  d3e3           shl ebx, cl           ; ebx = i at this point
0x2CD02  09df           or edi, ebx
0x2CD08  83c008         add eax, 8            ; entry stride 8
0x2CD0B  0fbfda         movsx ebx, dx        ; signed 16-bit i
0x2CD0E  3b5e08         cmp ebx, [esi+8]     ; +0x08 count
0x2CD17  8b4e14         mov ecx, [esi+0x14]
0x2CD1A  83c614         add esi, 0x14         ; stride 0x14, no pointer chase
0x2CD1D  85c9           test ecx, ecx
0x2CD1F  75c4           jne 0x2CCE5
```

- record stride `0x14`: `[0]` presence, `[4]` shift (only `cl` used; x86 masks
  to 5 bits), `[8]` count, `[0x10]` entries pointer, `[0x14]` next record's
  presence (contiguous);
- entry stride `8`, `[0]` is a `char *`; inner loop stops at the first `'*'`;
- `bits |= i << shift`.

With the shipped table at `0xA2EB4` (file `0xE9D08`) this returns
`0x00142095`: records `(shift 16, i 4)`, `(20, 1)`, `(0, 5)`, `(4, 9)`,
`(8, 0)`, `(24, 0)`, `(10, 0)`, `(13, 1)`, `(14, 0)`, `(15, 0)`. **Nonzero.**

## 5. `0x2CADC` order and the unreachable paths

`0x2CADC` (file `0x7F930`) calls the no-op message draw `0x2F198(-1, 0xE,
0x809C8, 0xB000)` (`0x2CAF3`), then `set(0x29, 0x2CCD0(0xA2EB4))`,
`set(0x35, 0xA0)`, `set(0x37, 0xA0)`,
`set(0x2A, (get(0x2A) & 0xFC) | 3)`, then the no-op screen setup `0x1AE20`
(`0x2CB50`), the no-op storage write `0x2EA78(0xB4, 0x0E)` (`0x2CB5F`) and the
no-op cursor restore `0x2F280(-1)` (`0x2CB69`). Confirmed; the only correction is
the menu-table address (`0xA2EB4`).

On the no-storage path `read1 = read2 = -1`:

- the early gate `0x2D74C..0x2D76A` fires only for `(read1<0 || read2<0) &&
  DSB(DS_0002D490)==0`. `DSB(DS_0002D490)` is `4` (file `0x802E4`; obj-0
  offset `0x1D490`, never written in the binary — only compared), so the gate
  does not fire;
- version arms: `0x2D76F cmp edx,esi; jle 0x2D7C9`. The `read2 > read1` arm
  (`0x2D786 or al,2`, file `0x805DA`) and the `read1 > read2` arm
  (`0x2D7DC or bh,4`, file `0x80630`) both require a strict inequality. With both
  `-1` neither runs, and the `movsd`/`movsb` stack-buffer copy into
  `DS_00105DE1` (`0x2D78B..0x2D79A`) is not reached — there is no stored image
  to copy, so the port has nothing to copy;
- the tail `0x2DE98` (`0x2D912`, file `0x80766`) and `0x2DF8C` (`0x2D919`, file
  `0x8076D`) are called unconditionally and are deferred no-ops (spec §6/§7);
- `DS_00105DA7` is set at `0x2D940` guarded by `read1 >= 0 || DSB(DS_0002D490)
  != 0` (`0x2D91E..0x2D929`); `read1=-1` and `DSB(DS_0002D490)=4` take the set
  branch. After it, `0x2D974(0x24)` (`0x2D946`) discards its result, and
  `0x2DAE4(0x24)` (`0x2D962`) runs only if `DS_00105DA4 + DS_00105DA5 != 0` —
  both are `0` here, so it is not called.
