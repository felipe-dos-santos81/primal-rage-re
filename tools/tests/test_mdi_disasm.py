# tools/tests/test_mdi_disasm.py
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import mdi_disasm  # noqa: E402


def test_loads_the_shipped_driver():
    img = mdi_disasm.load("data/game/C/SBPRO2.MDI")
    assert img.magic == b"AIL3MDI"
    assert img.size == 16541
    assert img.code_origin > 0          # the real-mode load origin, derived in Step 3


def test_disassembles_to_instructions():
    img = mdi_disasm.load("data/game/C/SBPRO2.MDI")
    ins = img.disassemble()
    assert len(ins) > 1000
    # addresses are monotonic and inside the image
    addrs = [a for a, _b, _t in ins]
    assert addrs == sorted(addrs)
