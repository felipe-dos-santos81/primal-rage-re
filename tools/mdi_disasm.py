#!/usr/bin/env python3
"""Disassemble an AIL3MDI driver (here `SBPRO2.MDI`, the Sound Blaster Pro 2 FM driver).

The file is a Miles Design Audio Interface Library 3.x VDI driver: an 8-byte
signature `AIL3MDI\\x1A`, then the standard MSS 3.X `VDI_HDR` (MSS.H, pack(1)),
then the driver's own tables and 16-bit real-mode code. `VDI_HDR` layout and the
field values observed in `data/game/C/SBPRO2.MDI`:

    0x00  char[8]  ID              "AIL3MDI\\x1A"
    0x08  u32      driver_version  0x00000112  (v1.12; the version that added dev_name)
    0x0C  far      common_IO_configurations  0 (runtime)
    0x10  u16      num_IO_configurations     0
    0x12  far      environment_string        0 (runtime)
    0x16  IO_PARMS 24 bytes                  IO/IRQ/DMA words + reserved
    0x2E  s16      service_rate     runtime: timer Hz the driver declares at DRV_INIT (0x300)
    0x30  u16      busy             runtime: AIL zeroes it
    0x32  u16      driver_num       runtime: AIL stores the driver handle index
    0x34  u16      this_ISR         0x0132  -- offset of the INT 66h dispatcher
    0x36  far      prev_ISR         runtime: previous INT 66h handler
    0x3A  char[128] scratch         driver scratch
    0xBA  char[80]  dev_name        "Creative Labs Sound Blaster Pro (new version)"
    end   0x10A

**Load origin (evidence).** The driver does not carry a load segment. AIL loads
it into low memory it allocates at runtime: `FUN_00065b7b` calls `FUN_0001074b`
for `(size+0xF)>>4` paragraphs and stores the returned real-mode `segment<<4`
as the image base in the driver handle (`port/decomp/prage.c`). Every address in
the file is therefore image-relative, not absolute:

  * `this_ISR` (0x34) is the plain image offset `0x0132`, and the bytes there are
    the DRV dispatcher -- `cmp ax, 0x300` (`DRV_INIT`, per MSS.H), bounding AX to
    `0x300..0x5FF` and loading `cs:[0x32]` (the runtime `driver_num`) -- so the
    code body begins at image offset 0x0132.
  * AIL writes runtime fields at image offsets `0x30`, `0x32`, `0x36`, and the
    driver itself writes scratch at `0x126`/`0x128`/.../`0x130` via `cs:`/DS; all
    are image-relative.
  * The 14-word entry table at `0x10A` ends in an image-relative offset to a
    7-byte `mov word [0x126],0xFFFF; ret` stub at the image's last 7 bytes (same
    in all seven shipped `.MDI` files).

Because the runtime base cannot be known statically, the harness linearises the
image at its own `seg:0000` base, so a listing address equals the driver's image
offset and `code_origin` is the flat address of the first disassembled byte,
`0x0132`. Rebase the listing by adding the runtime segment<<4 if a real run's
absolute addresses are needed.

usage:
  mdi_disasm.py FILE.mdi             # listing: address  bytes  instruction
  mdi_disasm.py --info FILE.mdi      # parsed header fields only
"""
import argparse
import struct
import sys

import capstone

MAGIC = b"AIL3MDI"
SIGNATURE = b"AIL3MDI\x1a"
VDI_HDR_SIZE = 0x10A
DEV_NAME_OFF = 0xBA


class MdiImage:
    """A parsed AIL3MDI driver image plus its 16-bit disassembly."""

    def __init__(self, data):
        self.data = data
        self.magic = data[:7]
        self.size = len(data)
        self.driver_version = struct.unpack_from("<I", data, 0x08)[0]
        self.service_rate = struct.unpack_from("<h", data, 0x2E)[0]
        self.busy = struct.unpack_from("<H", data, 0x30)[0]
        self.driver_num = struct.unpack_from("<H", data, 0x32)[0]
        self.this_isr = struct.unpack_from("<H", data, 0x34)[0]
        self.dev_name = data[DEV_NAME_OFF:DEV_NAME_OFF + 80].split(b"\0", 1)[0].decode("latin-1")
        self.code_offset = self.this_isr
        # Linearised at the image's own seg:0000 base (see module docstring).
        self.code_origin = self.code_offset

    def _byte_at(self, addr):
        return self.data[self.code_offset + (addr - self.code_origin)]

    def disassemble(self):
        """Return [(addr, raw_bytes, text), ...] from the code body to EOF.

        Bytes that do not decode are emitted one per entry as `db 0xNN`, the way
        a debugger skips them, so the listing stays monotonic and complete.
        """
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
        body = self.data[self.code_offset:]
        out = []
        addr = self.code_origin
        end = self.code_origin + len(body)
        while addr < end:
            insn = next(md.disasm(body[addr - self.code_origin:][:32], addr), None)
            if insn is None or insn.address != addr:
                # capstone stops at, or skips over, invalid bytes: advance one.
                out.append((addr, bytes([self._byte_at(addr)]), "db %#04x" % self._byte_at(addr)))
                addr += 1
                continue
            raw = body[addr - self.code_origin:][:insn.size]
            text = ("%s %s" % (insn.mnemonic, insn.op_str)).strip()
            out.append((insn.address, raw, text))
            addr += insn.size
        return out


def load(path):
    """Parse FILE.mdi into an MdiImage; raise ValueError if it is not AIL3MDI."""
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < VDI_HDR_SIZE or data[:7] != MAGIC:
        raise ValueError("%s: not an AIL3MDI driver" % path)
    return MdiImage(data)


def cmd_info(path):
    img = load(path)
    print("%s: magic %s, %d bytes" % (path, img.magic.decode("ascii"), img.size))
    print("  driver_version: %#06x (v%d.%02x)"
          % (img.driver_version, img.driver_version >> 8, img.driver_version & 0xFF))
    print("  dev_name:       %s" % img.dev_name)
    print("  service_rate:   %d (runtime field; 0 in the image)" % img.service_rate)
    print("  driver_num:     %d (runtime field; 0 in the image)" % img.driver_num)
    print("  busy:           %d (runtime field; 0 in the image)" % img.busy)
    print("  this_ISR:       %#06x  (INT 66h dispatcher = code body start)" % img.this_isr)
    print("  code_origin:    %#06x  (image-relative linearisation, seg:0000)" % img.code_origin)


def cmd_list(path):
    for addr, raw, text in load(path).disassemble():
        print("%#06x  %-20s  %s" % (addr, raw.hex(), text))


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--info", action="store_true", help="print parsed header fields only")
    ap.add_argument("file")
    a = ap.parse_args(argv)
    try:
        if a.info:
            cmd_info(a.file)
        else:
            cmd_list(a.file)
    except (OSError, ValueError) as e:
        print("mdi_disasm: %s" % e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
