#!/usr/bin/env python3
"""Config-field image codec for the K11 harness: 0x2D974 (get) and the image
half of 0x2DA0C (set), over a bytearray window of the data object.

A descriptor is the dword at code VA 0x2D300 + 4 * field (0x2D987 `mov
esi,[eax*4+0x2D300]`). It is a bitfield and not a fixup target, so the raw file
bytes at VA + 0x52E54 are the runtime bytes. Bits 0..5 give a byte index into
DS_00105DAF (0 = none), which holds the value's low byte. Bits 6..13 give a
nibble offset into DS_00105DE1. Bits 14..16 give the nibble count - 1.

set_() writes only the two image arrays. It does not write the dirty bits
DS_00105DD8 (0x2DA3B/0x2DA4E) or call the EEPROM mirror 0x2D4EC: a live poke of
the original must change the fields and nothing else. Stdlib only."""
import struct

WIN_LO = 0x105DAF          # DS_00105DAF: the byte table's base (index 0 unused)
WIN_HI = 0x105E30          # past the last nibble byte any descriptor reaches (§A.3)
BYTES_DS = 0x105DAF
NIB_DS = 0x105DE1
DD8_DS = 0x105DD8
DESC_VA = 0x2D300
N_FIELDS = 0x3F
CODE_FILE_DELTA = 0x52E54  # obj-0 raw file offset = VA + 0x52E54 (AGENTS.md)


def load_descriptors(exe_path):
    with open(exe_path, 'rb') as f:
        data = f.read()
    return list(struct.unpack_from('<%dI' % N_FIELDS, data, DESC_VA + CODE_FILE_DELTA))


def width(desc):
    """The field's bits: 4 per nibble, plus 8 for a byte-table entry."""
    return 4 * (((desc >> 14) & 7) + 1) + (8 if desc & 0x3F else 0)


def _at(ds):
    return ds - WIN_LO


def get(img, descs, field):
    """0x2D974. 0xFFFFFFFF above field 0x3E (0x2D978 `cmp eax,0x3e; jbe`)."""
    if field > 0x3E:
        return 0xFFFFFFFF
    d = descs[field]
    edx = ((d >> 14) & 7) + 1                        # 0x2D992..0x2D9A0
    eax = ((d >> 6) & 0xFF) + edx                    # 0x2D995..0x2D9A1
    ebx = eax >> 1                                   # 0x2D9A5 sar (eax >= 0)
    if eax & 1:                                      # 0x2D9A7
        v = img[_at(NIB_DS + ebx)] & 0xF             # 0x2D9AB..0x2D9B5
        edx -= 1                                     # 0x2D9BA
    else:
        v = 0                                        # 0x2D9BF
    while edx:                                       # 0x2D9C1
        ebx -= 1                                     # 0x2D9C5
        if edx == 1:                                 # 0x2D9C6
            v = (v << 4) | ((img[_at(NIB_DS + ebx)] >> 4) & 0xF)        # 0x2D9CD..0x2D9DC
            break
        v = ((v << 8) | img[_at(NIB_DS + ebx)]) & 0xFFFFFFFF           # 0x2D9E2..0x2D9EE
        edx -= 2                                     # 0x2D9EB
    if d & 0x3F:                                     # 0x2D9F2
        v = ((v << 8) | img[_at(BYTES_DS + (d & 0x3F))]) & 0xFFFFFFFF  # 0x2D9F9..0x2DA02
    return v & 0xFFFFFFFF


def set_(img, descs, field, value):
    """0x2DA0C's image writes. -1 above field 0x3E, else 0."""
    if field > 0x3E:
        return -1
    d = descs[field]
    value &= 0xFFFFFFFF
    if d & 0x3F:                                     # 0x2DA29
        img[_at(BYTES_DS + (d & 0x3F))] = value & 0xFF                 # 0x2DA34
        value >>= 8                                  # 0x2DA3E
    bitpos = (d >> 6) & 0xFF                         # 0x2DA59..0x2DA5F
    n = ((d >> 14) & 7) + 1                          # 0x2DA5C..0x2DA6A
    ebx = bitpos >> 1                                # 0x2DA6B
    if bitpos & 1:                                   # 0x2DA6D
        low = img[_at(NIB_DS + ebx)] & 0xF           # 0x2DA72..0x2DA78
        ebx += 1                                     # 0x2DA8A
        n -= 1                                       # 0x2DA86
        img[_at(NIB_DS - 1 + ebx)] = low | ((value & 0xF) << 4)        # 0x2DA90 [ebx+0x105DE0]
        value >>= 4                                  # 0x2DA8D
    while n:                                         # 0x2DA96
        if n == 1:                                   # 0x2DA9A
            img[_at(NIB_DS + ebx)] = (img[_at(NIB_DS + ebx)] & 0xF0) | (value & 0xF)  # 0x2DA9F..0x2DAAE
            break
        n -= 2                                       # 0x2DAB6
        img[_at(NIB_DS + ebx)] = value & 0xFF        # 0x2DAB9
        ebx += 1                                     # 0x2DABF
        value >>= 8                                  # 0x2DAC0
    return 0
