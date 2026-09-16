"""Dump GRA header fields for the S16*.GRA files in a directory.

usage: gra_headers.py DATA_DIR
"""
import glob
import os
import struct
import sys

paths = sorted(glob.glob(os.path.join(sys.argv[1], 'S16*.GRA')))
for path in paths:
    d = open(path, 'rb').read()
    name = os.path.basename(path)
    ver, = struct.unpack_from('<H', d, 0)
    magic = d[2:4]
    f1, = struct.unpack_from('<I', d, 4)
    print(f'{name:20s} size={len(d):8d} ver={ver} magic={magic!r} f1={f1:#010x}({f1:8d}) '
          f'size-f1={len(d)-f1}')
