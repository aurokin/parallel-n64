"""Read conformance-ROM verdicts out of a saved `M64+SAVE` body.

None of the public suites writes a single pass/fail word to RDRAM, so each
decoder reads the evidence the suite does leave in machine state:

* `krom`: PeterLemon's tests draw 8x8 "PASS" (white on green) and "FAIL"
  (white on red) cells with the CPU into a 32bpp framebuffer. The decoder
  finds those cells in RDRAM, pixel-exact, and returns their positions.
* `dillon`: Dillonb's n64-tests leave r30 = -1 when every case passed, or the
  1-based number of the failing case; r30 stays 0 while the test runs.
* `systemtest`: n64-systemtest (nemu64-test) formats a summary string
  ("Finished in ... Base: Failed F of N tests") and one line per failing test
  into heap memory, which the decoder scans for.

Decoders work on the unwrapped body (`savestate_bridge.unwrap`), so they need
no running emulator and are unit-tested with synthetic bodies.
"""

from __future__ import annotations

import array
import re
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from savestate_bridge import FLASH, RDRAM, RDRAM_SIZE  # noqa: E402

MAGIC = b"M64+SAVE"
VI_ORIGIN = 252                  # VI section: STATUS, ORIGIN, WIDTH, ...
VI_WIDTH = 256
# use_flashram, flash mode/status/offsets (24 bytes), both TLB LUTs, llbit.
GPRS = FLASH + 24 + 2 * 0x400000 + 4

# krom font glyphs (white pixels on a coloured 8x8 cell), from FontGreen8x8.bin.
GLYPHS = {
    "P": (".XXXXX..", ".XX..XX.", ".XXXXXX.", ".XXXXX..", ".XX.....", ".XX.....", "........", "........"),
    "A": ("...XX...", "..XXXX..", "..X..X..", ".XXXXXX.", ".XX..XX.", ".XX..XX.", "........", "........"),
    "S": ("..XXXX..", ".XX...X.", ".XXXXX..", "..XXXXX.", ".X...XX.", "..XXXX..", "........", "........"),
    "F": (".XXXXXX.", ".XX.....", ".XXXXX..", ".XX.....", ".XX.....", ".XX.....", "........", "........"),
    "I": (".XXXXXX.", "...XX...", "...XX...", "...XX...", ".XXXXXX.", ".XXXXXX.", "........", "........"),
    "L": (".XX.....", ".XX.....", ".XX.....", ".XX.....", ".XXXXXX.", ".XXXXXX.", "........", "........"),
}
WHITE = 0xFFFFFFFF
GREEN = 0x008000FF
RED = 0xFF0000FF
KROM_HEIGHT = 480                # tallest krom screen; shorter screens just end in black

SUMMARY_RE = re.compile(rb"Finished in [0-9.]+s\. Base: Failed (\d+) of (\d+) tests")
FAILED_TEST_RE = re.compile(rb"Test '([^'\x00\n]{1,120})'([^\x00\n]{0,200}?) failed")


class DecodeError(ValueError):
    """The body is not a state this decoder can read."""


@dataclass
class Verdict:
    """`status` is pass, fail or incomplete (the test had not finished)."""

    status: str
    detail: str
    observed: dict = field(default_factory=dict)


def _check(body: bytes) -> None:
    if body[:8] != MAGIC or len(body) < GPRS + 32 * 8:
        raise DecodeError("not an M64+SAVE body from this core")


def rdram(body: bytes) -> bytes:
    """RDRAM in N64 (big-endian) byte order; the state stores little-endian words."""
    _check(body)
    words = array.array("I")
    words.frombytes(body[RDRAM:RDRAM + RDRAM_SIZE])
    if sys.byteorder == "little":
        words.byteswap()
    return words.tobytes()


def vi_framebuffer(body: bytes) -> tuple[int, int]:
    """(physical origin, width in pixels) from the VI registers."""
    _check(body)
    origin, width = struct.unpack_from("<II", body, VI_ORIGIN)
    return origin & 0x00FFFFFF, width & 0xFFF


def gpr(body: bytes, index: int) -> int:
    _check(body)
    return struct.unpack_from("<q", body, GPRS + 8 * index)[0]


def _cell_pattern(text: str, background: int) -> tuple[bytes, ...]:
    rows = []
    for y in range(8):
        pixels = [WHITE if GLYPHS[ch][y][x] == "X" else background for ch in text for x in range(8)]
        rows.append(struct.pack(f">{len(pixels)}I", *pixels))
    return tuple(rows)


PATTERNS = {"PASS": _cell_pattern("PASS", GREEN), "FAIL": _cell_pattern("FAIL", RED)}


def find_krom_cells(fb: bytes, width: int) -> list[tuple[int, int, str]]:
    """Every PASS/FAIL cell in a big-endian 32bpp framebuffer, as (y, x, text) in raster order."""
    stride = width * 4
    found = []
    for text, rows in PATTERNS.items():
        start = fb.find(rows[0])
        while start != -1:
            if start % 4 == 0:
                y, x = divmod(start // 4, width)
                if x + 32 <= width and all(
                        fb[start + r * stride:start + r * stride + len(rows[r])] == rows[r] for r in range(1, 8)):
                    found.append((y, x, text))
            start = fb.find(rows[0], start + 1)
    return sorted(found)


def decode_krom(body: bytes, expected: list[list]) -> Verdict:
    """Pass when the drawn PASS/FAIL counts equal the reference screenshot's.

    Counts, not positions, are compared: several upstream screenshots are
    resampled a pixel or two off the framebuffer grid, while the ROM always
    draws at its source coordinates. Positions are kept for triage.
    """
    origin, width = vi_framebuffer(body)
    if width not in (320, 400, 640):
        return Verdict("incomplete", f"VI width {width} is not a krom screen (origin 0x{origin:06x})")
    memory = rdram(body)
    fb = memory[origin:origin + width * KROM_HEIGHT * 4]
    observed = [[x, y, text] for y, x, text in find_krom_cells(fb, width)]
    passes = sum(c[2] == "PASS" for c in observed)
    fails = [c for c in observed if c[2] == "FAIL"]
    want_pass = sum(c[2] == "PASS" for c in expected)
    want_fail = sum(c[2] == "FAIL" for c in expected)
    summary = {"pass_cells": passes, "fail_cells": len(fails), "reference_pass_cells": want_pass,
               "reference_fail_cells": want_fail, "cells": observed}
    if (passes, len(fails)) == (want_pass, want_fail):
        return Verdict("pass", f"{passes} PASS cells, as in the reference", summary)
    if len(fails) > want_fail:
        where = ", ".join(f"({x},{y})" for x, y, _ in fails[:8])
        return Verdict("fail", f"{len(fails)} FAIL cell(s) at {where}; {passes} of {want_pass} PASS", summary)
    if passes < want_pass:
        return Verdict("incomplete", f"{passes} of {want_pass} PASS cells drawn", summary)
    return Verdict("fail", f"{passes} PASS and {len(fails)} FAIL cells; the reference has "
                           f"{want_pass} and {want_fail}", summary)


def decode_dillon(body: bytes) -> Verdict:
    r30 = gpr(body, 30)
    if r30 == -1:
        return Verdict("pass", "r30 = -1 (all cases passed)", {"r30": r30})
    if r30 == 0:
        return Verdict("incomplete", "r30 = 0 (test still running)", {"r30": r30})
    return Verdict("fail", f"r30 = {r30} (first failing case)", {"r30": r30})


def decode_systemtest(body: bytes) -> Verdict:
    memory = rdram(body)
    summaries = {(int(f), int(n)) for f, n in SUMMARY_RE.findall(memory)}
    names = sorted({(name + rest).decode("ascii", "replace").strip()
                    for name, rest in FAILED_TEST_RE.findall(memory)})
    observed = {"failed_tests": names}
    if not summaries:
        return Verdict("incomplete", "no 'Finished in' summary in RDRAM", observed)
    if len(summaries) > 1:
        return Verdict("fail", f"conflicting summaries in RDRAM: {sorted(summaries)}", observed)
    failed, total = summaries.pop()
    observed.update({"failed": failed, "total": total})
    if failed == 0:
        return Verdict("pass", f"0 of {total} tests failed", observed)
    return Verdict("fail", f"{failed} of {total} tests failed", observed)


def decode(kind: str, body: bytes, test: dict) -> Verdict:
    if kind == "krom":
        return decode_krom(body, test["reference_cells"])
    if kind == "dillon":
        return decode_dillon(body)
    if kind == "systemtest":
        return decode_systemtest(body)
    raise DecodeError(f"unknown decoder {kind!r}")
