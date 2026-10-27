#!/usr/bin/env python3
"""Convert savestates between this core and mupen64plus-next (GLideN64).

Both cores descend from mupen64plus and share most of the `M64+SAVE` layout,
so a state saved here can be opened in the GlideN64 reference core at the same
game moment, and a state found there can be brought back. The differences:

* This core writes version 1.0 with an extra `ai.last_read` word inside the AI
  section (no version bump), so a stock 1.0 parser reads every later field four
  bytes off. It ends with a variable-length event queue.
* mupen64plus-next reads a fixed 1024-byte event queue, a `using_tlb` word from
  version 1.1, and a 4096-byte extra block from 1.2. Event 0x800 is CART_INT
  here and RSP_DMA_EVT there. SPECIAL_INT is scheduled at count 0 here, while
  next re-derives it at every half wrap on load and saves that point.

`to-gliden64` writes version 1.1: next then rebuilds PIF channels, video timing
and other extra state with its own defaults, as it does for 1.0 and 1.1 states.
With CP0 Status.FR clear, this core saves each FPR in its own 64-bit slot while
next packs odd FPRs into the high word of the even slot; both directions swap
the middle words of each register pair. Conversions refuse states saved while
the RSP is running or an RSP DMA, flash erase or flash program is in flight.

Inputs may be RetroArch RZIP/RASTATE files or raw `M64+SAVE` data; outputs are
raw, which RetroArch loads as-is.
"""

from __future__ import annotations

import argparse
import struct
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path

MAGIC = b"M64+SAVE"
HEADER_SIZE = 44                 # magic, version, ROM MD5
NEXT_BASE_SIZE = 16788244        # next's fixed body after the header
OURS_BASE_SIZE = NEXT_BASE_SIZE + 4
QUEUE_SIZE = 1024
EXTRA_SIZE = 4096
NEXT_STATE_SIZE = HEADER_SIZE + NEXT_BASE_SIZE + QUEUE_SIZE + 4 + EXTRA_SIZE

OURS_VERSION = 0x00010000
NEXT_OUTPUT_VERSION = 0x00010100

AI_LAST_READ = 380               # absolute offset of our extra AI word
SP_STATUS = 192
SP_STATUS_HALT = 0x1
RDRAM = 448                      # absolute RDRAM offset in our layout
RDRAM_SIZE = 0x800000
FLASH = RDRAM + RDRAM_SIZE + 0x2000 + 64          # use_flashram, then our flash fields
TLB_ENTRIES = FLASH + 24 + 2 * 0x400000 + 4 + 256 + 128 + 16 + 256 + 8
TLB_ENTRY_SIZE = 52
TLB_V_EVEN, TLB_V_ODD = 18, 26
CP1_REGS = TLB_ENTRIES - 8 - 256                  # 32 x int64 FPRs, then FCR0/FCR31
CP0_STATUS = CP1_REGS - 16 - 128 + 12 * 4         # CP0 registers precede MULT_LO/HI
CP0_STATUS_FR = 0x04000000

VI_INT = 0x001
RSP_DMA_EVT = 0x800              # next's numbering
SPECIAL_INT = 0x020              # count wrap: 0 here; next re-derives a half-wrap point
CART_INT = 0x800                 # ours; next uses 0x800 for RSP_DMA_EVT
OURS_EVENTS = frozenset(1 << bit for bit in range(12))
EVENT_END = 0xFFFFFFFF

# mupen64plus-next flashram modes and their closest equivalents here. This core
# keeps a 64-bit status whose high word is the chip status register.
NEXT_FLASH_READ_ARRAY, NEXT_FLASH_SILICON_ID, NEXT_FLASH_STATUS = 0, 1, 2
OURS_FLASH_ERASE, OURS_FLASH_WRITE, OURS_FLASH_READ, OURS_FLASH_STATUS = 1, 2, 3, 4
OURS_FLASH_IDLE_STATUS = 0x11118004F0000000

# Offsets inside next's 1.3+ extra block (main/savestates.c writer order).
EXTRA_TPAKS = 48                 # after ai, cart_rom, af-rtc, controller and rumble state
TPAK_RECORD = 16 + 0x1C          # four words and the GB cart fingerprint
TPAK_GB_CART = 5 * 4 + 8 + 2 * 5 + 0x36             # MBC/RTC/camera state when a cart is present
EXTRA_FLASH_AFTER_TPAKS = 987 - (EXTRA_TPAKS + 4 * TPAK_RECORD)   # page_buf without GB carts


class BridgeError(ValueError):
    """A state cannot be converted faithfully."""


@dataclass(frozen=True)
class State:
    version: int
    body: bytes          # bytes after the 44-byte header, our or next layout

    @property
    def header(self) -> bytes:
        return MAGIC + struct.pack(">I", self.version)


def unwrap(data: bytes) -> bytes:
    """Return raw M64+SAVE bytes from RZIP/RASTATE containers or raw data."""
    if data[:8] == b"#RZIPv\x01#":
        offset, out = 20, bytearray()
        while offset < len(data):
            (size,) = struct.unpack_from("<I", data, offset)
            offset += 4
            out += zlib.decompress(data[offset:offset + size])
            offset += size
        data = bytes(out)
    if data[:8] == b"RASTATE\x01":
        offset = 8
        while offset + 8 <= len(data):
            tag = data[offset:offset + 4]
            (size,) = struct.unpack_from("<I", data, offset + 4)
            if tag == b"MEM ":
                return data[offset + 8:offset + 8 + size]
            offset += 8 + ((size + 7) & ~7)
        raise BridgeError("RASTATE container has no MEM block")
    return data


def parse(data: bytes) -> tuple[int, bytes, bytes]:
    """Split raw state bytes into (version, md5, body)."""
    raw = unwrap(data)
    if raw[:8] != MAGIC:
        raise BridgeError("not an M64+SAVE state")
    (version,) = struct.unpack_from(">I", raw, 8)
    if version >> 16 != 1:
        raise BridgeError(f"unsupported state version {version:08x}")
    return version, raw[12:HEADER_SIZE], raw[HEADER_SIZE:]


def read_queue(blob: bytes, offset: int, limit: int) -> list[tuple[int, int]]:
    events = []
    while offset + 4 <= limit:
        (kind,) = struct.unpack_from("<I", blob, offset)
        if kind == EVENT_END:
            return events
        events.append(struct.unpack_from("<2I", blob, offset))
        offset += 8
    raise BridgeError("event queue has no terminator")


def pack_queue(events: list[tuple[int, int]]) -> bytes:
    return b"".join(struct.pack("<2I", kind, count) for kind, count in events) + struct.pack("<I", EVENT_END)


def u32(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def swap_fr0_pairs(cp1: bytes) -> bytes:
    """Convert FR=0 FPRs between this core's per-slot layout and next's packed one.

    Here FPR i is the low word of slot i; next keeps FPR 2k+1 in the high word of
    slot 2k. Swapping words 1 and 2 of each 16-byte pair maps one onto the other
    and is its own inverse."""
    words = list(struct.unpack("<64I", cp1))
    for pair in range(16):
        words[4 * pair + 1], words[4 * pair + 2] = words[4 * pair + 2], words[4 * pair + 1]
    return struct.pack("<64I", *words)


def convert_fpr_layout(blob: bytearray, cp1_at: int, status_at: int) -> None:
    if not u32(blob, status_at) & CP0_STATUS_FR:
        blob[cp1_at:cp1_at + 256] = swap_fr0_pairs(bytes(blob[cp1_at:cp1_at + 256]))


def to_gliden64(data: bytes) -> bytes:
    """Convert one of this core's states to mupen64plus-next version 1.1."""
    version, md5, body = parse(data)
    if version != OURS_VERSION or len(body) < OURS_BASE_SIZE + 4:
        raise BridgeError("expected a ParaLLEish version 1.0 state")
    raw = MAGIC + struct.pack(">I", version) + md5 + body
    if not u32(raw, SP_STATUS) & SP_STATUS_HALT:
        raise BridgeError("RSP was running when the state was saved; save again at another frame")
    use_flashram, flash_mode = struct.unpack_from("<2i", raw, FLASH)
    if use_flashram == 1 and flash_mode in (OURS_FLASH_ERASE, OURS_FLASH_WRITE):
        raise BridgeError("flashram was mid-erase or mid-program; save again at another frame")
    events = [event for event in read_queue(raw, HEADER_SIZE + OURS_BASE_SIZE, len(raw))
              if event[0] != CART_INT]
    queue = pack_queue(events)
    if len(queue) > QUEUE_SIZE:
        raise BridgeError("event queue exceeds 1024 bytes")
    using_tlb = any(raw[TLB_ENTRIES + i * TLB_ENTRY_SIZE + TLB_V_EVEN] or
                    raw[TLB_ENTRIES + i * TLB_ENTRY_SIZE + TLB_V_ODD] for i in range(32))
    base = bytearray(raw[:AI_LAST_READ] + raw[AI_LAST_READ + 4:HEADER_SIZE + OURS_BASE_SIZE])
    convert_fpr_layout(base, CP1_REGS - 4, CP0_STATUS - 4)
    out = (MAGIC + struct.pack(">I", NEXT_OUTPUT_VERSION) + base[12:]
           + queue.ljust(QUEUE_SIZE, b"\xff") + struct.pack("<I", int(using_tlb)))
    return out.ljust(NEXT_STATE_SIZE, b"\0")


def next_extra(version: int, body: bytes) -> bytes | None:
    """Return next's extra block for versions with the 1.3+ layout."""
    if version < 0x00010300:
        return None
    start = NEXT_BASE_SIZE + QUEUE_SIZE + 4
    return body[start:start + EXTRA_SIZE]


def extra_flash_offset(extra: bytes) -> int:
    """Offset of flashram page_buf in next's extra block; GB carts lengthen it."""
    offset = EXTRA_TPAKS
    for _ in range(4):
        cart_present = extra[offset + 16] != 0
        offset += TPAK_RECORD + (TPAK_GB_CART if cart_present else 0)
    return offset + EXTRA_FLASH_AFTER_TPAKS


def flash_fields(version: int, body: bytes) -> bytes:
    """Our flashram fields (mode, status, erase_offset, write_pointer)."""
    slots = FLASH - HEADER_SIZE - 4 + 4
    extra = next_extra(version, body)
    if extra is None or version < 0x00010800:
        # Our own 1.1 output keeps our fields in the base slots next ignores.
        return body[slots:slots + 20]
    if u32(body, slots - 4) != 1:          # use_flashram
        return bytes(20)
    flash = extra_flash_offset(extra)
    silicon_id = struct.unpack_from("<2I", extra, flash + 128)
    status = u32(extra, flash + 136) & 0xFF
    (mode,) = struct.unpack_from("<H", extra, flash + 142)
    if mode == NEXT_FLASH_READ_ARRAY:
        return struct.pack("<iQ2I", OURS_FLASH_READ, OURS_FLASH_IDLE_STATUS, 0, 0)
    if mode == NEXT_FLASH_STATUS:
        return struct.pack("<iQ2I", OURS_FLASH_STATUS, ((0x11118000 | status) << 32) | 0x00C20000, 0, 0)
    if mode == NEXT_FLASH_SILICON_ID:
        return struct.pack("<iQ2I", OURS_FLASH_STATUS, (silicon_id[0] << 32) | silicon_id[1], 0, 0)
    raise BridgeError("flashram was mid-erase or mid-program; save again at another frame")


def to_parallel(data: bytes) -> bytes:
    """Convert a mupen64plus-next state (versions 1.0-1.9) to this core's 1.0 layout."""
    version, md5, body = parse(data)
    if len(body) < NEXT_BASE_SIZE + 4:
        raise BridgeError("truncated mupen64plus-next state")
    if not u32(body, SP_STATUS - HEADER_SIZE) & SP_STATUS_HALT:
        raise BridgeError("RSP was running when the state was saved; save again at another frame")
    base = bytearray(body[:NEXT_BASE_SIZE])
    queue_end = min(len(body), NEXT_BASE_SIZE + QUEUE_SIZE)
    events = read_queue(body, NEXT_BASE_SIZE, queue_end)
    if any(kind == RSP_DMA_EVT for kind, _ in events):
        raise BridgeError("an RSP DMA was in flight; save again at another frame")
    dropped = sorted({kind for kind, _ in events if kind not in OURS_EVENTS})
    events = [(kind, 0 if kind == SPECIAL_INT else count) for kind, count in events
              if kind in OURS_EVENTS]
    if all(kind != SPECIAL_INT for kind, _ in events):
        events.append((SPECIAL_INT, 0))
    vi = [count for kind, count in events if kind == VI_INT]
    if not vi:
        raise BridgeError("state has no pending VI interrupt")
    struct.pack_into("<I", base, NEXT_BASE_SIZE - 8, vi[0])       # next stores 0 for next_vi
    flash_at = FLASH - HEADER_SIZE - 4 + 4
    base[flash_at:flash_at + 20] = flash_fields(version, body)
    convert_fpr_layout(base, CP1_REGS - HEADER_SIZE - 4, CP0_STATUS - HEADER_SIZE - 4)
    extra = next_extra(version, body)
    last_read = u32(extra, 0) if extra is not None else 0
    split = AI_LAST_READ - HEADER_SIZE
    out_body = bytes(base[:split]) + struct.pack("<I", last_read) + bytes(base[split:]) + pack_queue(events)
    if dropped:
        print("dropped mupen64plus-next-only events: " + ", ".join(f"0x{kind:x}" for kind in dropped),
              file=sys.stderr)
    return MAGIC + struct.pack(">I", OURS_VERSION) + md5 + out_body


def inspect(data: bytes) -> str:
    version, md5, body = parse(data)
    ours = version == OURS_VERSION and len(body) < NEXT_BASE_SIZE + QUEUE_SIZE
    base_size = OURS_BASE_SIZE if ours else NEXT_BASE_SIZE
    pc, next_interrupt, next_vi, field = struct.unpack_from("<4I", body, base_size - 16)
    raw = MAGIC + struct.pack(">I", version) + md5 + body
    events = read_queue(body, base_size, min(len(body), base_size + QUEUE_SIZE))
    shift = 0 if ours else 4
    status = u32(raw, CP0_STATUS - shift)
    return "\n".join([
        f"layout      {'parallel-n64' if ours else 'mupen64plus-next'} version {version:08x}",
        f"rom md5     {md5.decode('ascii', 'replace')}",
        f"pc          {pc:08x}  next_interrupt {next_interrupt:08x}  next_vi {next_vi:08x}  field {field}",
        f"rsp halted  {bool(u32(raw, SP_STATUS) & SP_STATUS_HALT)}  cp0 status {status:08x} "
        f"(FR={int(bool(status & CP0_STATUS_FR))})",
        "events      " + " ".join(f"{kind:#x}@{count:08x}" for kind, count in events),
        *([f"rdp noise   base primitive index {rdp_noise}"] if (rdp_noise := rdp_trailer(body, base_size)) is not None else []),
    ])


def rdp_trailer(body: bytes, queue_at: int) -> int | None:
    """The ParaLLEl-RDP noise-seed counter from a "PRDP" trailer after the event queue, if present."""
    offset = queue_at
    while offset + 4 <= len(body):
        if struct.unpack_from("<I", body, offset)[0] == EVENT_END:
            tail = body[offset + 4:]
            if len(tail) == 12 and tail[:4] == b"PRDP" and struct.unpack_from("<I", tail, 4)[0] == 1:
                return struct.unpack_from("<I", tail, 8)[0]
            return None
        offset += 8
    return None


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    for name, help_text in (("to-gliden64", "convert a ParaLLEish state for mupen64plus-next"),
                            ("to-parallel", "convert a mupen64plus-next state for ParaLLEish")):
        command = sub.add_parser(name, help=help_text)
        command.add_argument("source", type=Path)
        command.add_argument("destination", type=Path)
    sub.add_parser("inspect", help="print key fields of a state").add_argument("source", type=Path)
    args = parser.parse_args(argv)
    try:
        data = args.source.read_bytes()
        if args.command == "inspect":
            print(inspect(data))
            return 0
        convert = to_gliden64 if args.command == "to-gliden64" else to_parallel
        args.destination.write_bytes(convert(data))
    except BridgeError as error:
        print(f"{args.source}: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
