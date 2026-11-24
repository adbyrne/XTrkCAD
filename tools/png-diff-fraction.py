#!/usr/bin/env python3
"""Print the percentage of pixels that differ between two PNG files.

Usage: png-diff-fraction.py <a.png> <b.png>

Both files must be the same size, 8 bits per channel, RGB or RGBA, not
interlaced: what XTrackCAD's bitmap export writes. Only the standard library
is used, so the export check runs wherever python3 does.
tools/check-export-test.sh uses it to compare a bitmap export with the snap
grid shown against one without it (Development Issues #82).
"""

import struct
import sys
import zlib


def read_png(path):
    """Return (width, height, rows), each row a bytes object of pixels."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(f"{path}: not a PNG")
    pos = 8
    idat = b""
    width = height = channels = None
    while pos < len(data):
        length, ctype = struct.unpack(">I4s", data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(
                ">IIBBBBB", chunk)
            if depth != 8 or color not in (2, 6) or interlace != 0:
                sys.exit(f"{path}: unsupported PNG (depth {depth}, "
                         f"color type {color}, interlace {interlace})")
            channels = 3 if color == 2 else 4
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break
    raw = zlib.decompress(idat)
    stride = width * channels
    rows = []
    prev = bytearray(stride)
    i = 0
    for _ in range(height):
        ftype = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - channels] if x >= channels else 0
            b = prev[x]
            c = prev[x - channels] if x >= channels else 0
            if ftype == 1:
                line[x] = (line[x] + a) & 0xFF
            elif ftype == 2:
                line[x] = (line[x] + b) & 0xFF
            elif ftype == 3:
                line[x] = (line[x] + (a + b) // 2) & 0xFF
            elif ftype == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[x] = (line[x] + pred) & 0xFF
        rows.append(bytes(line))
        prev = line
    return width, height, channels, rows


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    w1, h1, c1, rows1 = read_png(sys.argv[1])
    w2, h2, c2, rows2 = read_png(sys.argv[2])
    if (w1, h1, c1) != (w2, h2, c2):
        sys.exit(f"sizes differ: {w1}x{h1}x{c1} and {w2}x{h2}x{c2}")
    differ = 0
    for r1, r2 in zip(rows1, rows2):
        for x in range(0, len(r1), c1):
            if r1[x:x + c1] != r2[x:x + c1]:
                differ += 1
    print(f"{100.0 * differ / (w1 * h1):.1f}")


if __name__ == "__main__":
    main()
