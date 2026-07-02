#!/usr/bin/env python3
"""Exact uniformly-black check for a PNG capture.

Decodes the PNG (stdlib only) and reports whether every color sample is
zero. Alpha channels are ignored: an opaque black frame is still black.
This is a real pixel decode, not a file-size or checksum heuristic —
checksum-shaped runtime evidence is suspect by project policy.

Exit codes:
  0  every color sample is zero (uniformly black)
  1  at least one non-black pixel
  2  no judgment (not a PNG, or a layout this checker does not decode)
"""
import struct
import sys
import zlib

CHANNELS = {0: 1, 2: 3, 4: 2, 6: 4}


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <capture.png>", file=sys.stderr)
        return 2
    try:
        data = open(sys.argv[1], "rb").read()
    except OSError as err:
        print(f"unreadable: {err}", file=sys.stderr)
        return 2
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return 2

    ihdr = None
    idat = bytearray()
    pos = 8
    while pos + 8 <= len(data):
        length, typ = struct.unpack(">I4s", data[pos : pos + 8])
        chunk = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if typ == b"IHDR":
            ihdr = struct.unpack(">IIBBBBB", chunk)
        elif typ == b"IDAT":
            idat += chunk
        elif typ == b"IEND":
            break
    if ihdr is None or not idat:
        return 2

    width, height, depth, ctype, _comp, _filt, interlace = ihdr
    if depth != 8 or interlace != 0 or ctype not in CHANNELS:
        return 2
    channels = CHANNELS[ctype]
    color_channels = 3 if ctype in (2, 6) else 1

    try:
        raw = zlib.decompress(bytes(idat))
    except zlib.error:
        return 2
    stride = width * channels
    if len(raw) != (stride + 1) * height:
        return 2

    prev = bytearray(stride)
    for y in range(height):
        base = y * (stride + 1)
        filt = raw[base]
        row = bytearray(raw[base + 1 : base + 1 + stride])
        if filt == 1:  # Sub
            for i in range(channels, stride):
                row[i] = (row[i] + row[i - channels]) & 0xFF
        elif filt == 2:  # Up
            for i in range(stride):
                row[i] = (row[i] + prev[i]) & 0xFF
        elif filt == 3:  # Average
            for i in range(stride):
                left = row[i - channels] if i >= channels else 0
                row[i] = (row[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif filt == 4:  # Paeth
            for i in range(stride):
                a = row[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                row[i] = (row[i] + pred) & 0xFF
        elif filt != 0:
            return 2
        if channels == color_channels:
            if any(row):
                return 1
        else:
            for x in range(0, stride, channels):
                if any(row[x : x + color_channels]):
                    return 1
        prev = row
    return 0


if __name__ == "__main__":
    sys.exit(main())
