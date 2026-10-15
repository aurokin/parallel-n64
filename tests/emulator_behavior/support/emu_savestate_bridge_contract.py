"""Check savestate conversion between this core and mupen64plus-next on synthetic states."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

TOOLS = Path(__file__).resolve().parents[3] / "tools"
sys.path.insert(0, str(TOOLS))
import savestate_bridge as bridge

MD5 = b"A722F8161FF489943191330BF8416496"
VI_COUNT, AI_COUNT, CART_COUNT = 0x6A7670C0, 0x6A6EEF98, 0x6A700000
LAST_READ = 0x8A0
FLASH_FIELDS = struct.pack("<iQ2I", 0, 0x1111800400C20000, 37632, 901056)
FPR_WORDS = [0xF0000000 + i for i in range(64)]


def expect_refusal(convert, data, reason):
    try:
        convert(data)
    except bridge.BridgeError:
        return
    raise AssertionError(f"conversion must refuse {reason}")


def ours_state(sp_status=bridge.SP_STATUS_HALT, valid_tlb=True, fr=False, flash_mode=0):
    raw = bytearray(bridge.HEADER_SIZE + bridge.OURS_BASE_SIZE)
    raw[:12] = bridge.MAGIC + struct.pack(">I", bridge.OURS_VERSION)
    raw[12:44] = MD5
    struct.pack_into("<I", raw, bridge.SP_STATUS, sp_status)
    struct.pack_into("<I", raw, bridge.AI_LAST_READ, LAST_READ)
    struct.pack_into("<I", raw, bridge.AI_LAST_READ - 4, 0xA1A1A1A1)   # fifo[0].length
    struct.pack_into("<I", raw, bridge.AI_LAST_READ + 4, 0xD0D0D0D0)   # DPC start
    raw[bridge.RDRAM:bridge.RDRAM + 8] = b"RDRAM\x00\x01\x02"
    struct.pack_into("<i", raw, bridge.FLASH, 1)                        # use_flashram
    raw[bridge.FLASH + 4:bridge.FLASH + 24] = FLASH_FIELDS
    struct.pack_into("<i", raw, bridge.FLASH + 4, flash_mode)
    if valid_tlb:
        raw[bridge.TLB_ENTRIES + 3 * bridge.TLB_ENTRY_SIZE + bridge.TLB_V_ODD] = 1
    struct.pack_into("<I", raw, bridge.CP0_STATUS, 0x2000FF03 | (bridge.CP0_STATUS_FR if fr else 0))
    struct.pack_into("<64I", raw, bridge.CP1_REGS, *FPR_WORDS)
    tail = bridge.HEADER_SIZE + bridge.OURS_BASE_SIZE
    struct.pack_into("<4I", raw, tail - 16, 0x80000180, AI_COUNT, VI_COUNT, 0)
    queue = bridge.pack_queue([(0x40, AI_COUNT), (bridge.CART_INT, CART_COUNT), (bridge.VI_INT, VI_COUNT)])
    return bytes(raw) + queue


def wrap_rzip(raw, chunk=1 << 17):
    """RetroArch's RASTATE container inside multi-chunk RZIP, as written for real slots."""
    rastate = b"RASTATE\x01" + b"MEM " + struct.pack("<I", len(raw)) + raw
    rastate += b"\0" * (-len(rastate) % 8) + b"END " + struct.pack("<I", 0)
    out = b"#RZIPv\x01#" + struct.pack("<IQ", chunk, len(rastate))
    for start in range(0, len(rastate), chunk):
        compressed = zlib.compress(rastate[start:start + chunk])
        out += struct.pack("<I", len(compressed)) + compressed
    return out


def next_19_state(base_from, mode, events=None, gb_cart_in_pak=None, use_flashram=1):
    """A mupen64plus-next 1.9 state: base from a converted state, 1.3+ extra block."""
    out = bytearray(bridge.to_gliden64(base_from))
    out[8:12] = struct.pack(">I", 0x00010900)
    body = bridge.HEADER_SIZE
    struct.pack_into("<I", out, body + bridge.NEXT_BASE_SIZE - 8, 0)          # next writes 0 for next_vi
    struct.pack_into("<i", out, bridge.FLASH - 4, use_flashram)
    events = events or [(0x40, AI_COUNT), (bridge.VI_INT, VI_COUNT), (bridge.SPECIAL_INT, 0x80000000),
                        (0x2000, 9)]
    queue = bridge.pack_queue(events)
    out[body + bridge.NEXT_BASE_SIZE:body + bridge.NEXT_BASE_SIZE + bridge.QUEUE_SIZE] = \
        queue.ljust(bridge.QUEUE_SIZE, b"\xff")
    extra = body + bridge.NEXT_BASE_SIZE + bridge.QUEUE_SIZE + 4
    struct.pack_into("<I", out, extra, 0x1234)                                  # ai.last_read
    flash = extra + 987
    if gb_cart_in_pak is not None:
        out[extra + 48 + gb_cart_in_pak * 44 + 16] = ord("P")                  # fingerprint[0]
        flash += 92
    struct.pack_into("<2I", out, flash + 128, 0x11118001, 0x00C2001E)          # silicon_id
    struct.pack_into("<I", out, flash + 136, 0x08)                              # erase done
    struct.pack_into("<H", out, flash + 142, mode)
    return bytes(out)


def check_layout_constants():
    # Independent literals from the two cores, not derived from the bridge.
    assert bridge.HEADER_SIZE + bridge.NEXT_BASE_SIZE == 16788288, "next's fixed body size"
    assert bridge.NEXT_STATE_SIZE == 16788288 + 1024 + 4 + 4096, "next's retro_serialize_size"
    assert bridge.TLB_ENTRIES + 32 * 52 + 16 == bridge.HEADER_SIZE + bridge.OURS_BASE_SIZE, \
        "TLB entries are followed only by pc, next_interrupt, next_vi and field"
    assert bridge.CP1_REGS + 256 + 8 == bridge.TLB_ENTRIES
    assert bridge.EXTRA_TPAKS + 4 * bridge.TPAK_RECORD + bridge.EXTRA_FLASH_AFTER_TPAKS == 987


def check_to_gliden64(source):
    converted = bridge.to_gliden64(wrap_rzip(source))
    assert len(converted) == bridge.NEXT_STATE_SIZE, "output must match next's serialize size"
    version, md5, body = bridge.parse(converted)
    assert version == bridge.NEXT_OUTPUT_VERSION and md5 == MD5
    shift = bridge.HEADER_SIZE
    assert bridge.u32(body, bridge.AI_LAST_READ - shift - 4) == 0xA1A1A1A1
    assert bridge.u32(body, bridge.AI_LAST_READ - shift) == 0xD0D0D0D0, "last_read word must be removed"
    assert body[bridge.RDRAM - shift - 4:bridge.RDRAM - shift + 4] == b"RDRAM\x00\x01\x02", \
        "RDRAM moves up by the removed word"
    pc, next_interrupt, next_vi, _ = struct.unpack_from("<4I", body, bridge.NEXT_BASE_SIZE - 16)
    assert (pc, next_interrupt, next_vi) == (0x80000180, AI_COUNT, VI_COUNT)
    queue = bridge.read_queue(body, bridge.NEXT_BASE_SIZE, bridge.NEXT_BASE_SIZE + bridge.QUEUE_SIZE)
    assert queue == [(0x40, AI_COUNT), (bridge.VI_INT, VI_COUNT)], "CART_INT collides with RSP_DMA_EVT"
    assert set(body[bridge.NEXT_BASE_SIZE + 20:bridge.NEXT_BASE_SIZE + bridge.QUEUE_SIZE]) == {0xFF}
    assert bridge.u32(body, bridge.NEXT_BASE_SIZE + bridge.QUEUE_SIZE) == 1, "valid TLB entries set using_tlb"
    assert bridge.u32(bridge.to_gliden64(ours_state(valid_tlb=False))[44:],
                      bridge.NEXT_BASE_SIZE + bridge.QUEUE_SIZE) == 0

    # FR=0: next packs FPR 2k+1 into the high word of slot 2k.
    fprs = struct.unpack_from("<64I", body, bridge.CP1_REGS - shift - 4)
    for k in range(16):
        assert fprs[4 * k:4 * k + 4] == (FPR_WORDS[4 * k], FPR_WORDS[4 * k + 2],
                                         FPR_WORDS[4 * k + 1], FPR_WORDS[4 * k + 3]), "FR=0 FPR packing"
    fr1 = bridge.to_gliden64(ours_state(fr=True))
    assert list(struct.unpack_from("<64I", fr1, bridge.CP1_REGS - 4)) == FPR_WORDS, "FR=1 FPRs are unchanged"

    expect_refusal(bridge.to_gliden64, ours_state(sp_status=0), "a running RSP")
    expect_refusal(bridge.to_gliden64, ours_state(flash_mode=bridge.OURS_FLASH_ERASE), "a flash erase")
    expect_refusal(bridge.to_gliden64, ours_state(flash_mode=bridge.OURS_FLASH_WRITE), "a flash program")
    expect_refusal(bridge.to_gliden64, converted, "next's layout")
    expect_refusal(bridge.to_gliden64, b"not a state", "foreign data")
    return converted


def check_to_parallel(source, converted):
    # Round trip keeps everything this core stores except last_read, which
    # version 1.1 has no field for; flash fields ride in slots next ignores.
    back = bridge.to_parallel(converted)
    expected = bytearray(source)
    struct.pack_into("<I", expected, bridge.AI_LAST_READ, 0)
    del expected[bridge.HEADER_SIZE + bridge.OURS_BASE_SIZE:]
    expected += bridge.pack_queue([(0x40, AI_COUNT), (bridge.VI_INT, VI_COUNT), (bridge.SPECIAL_INT, 0)])
    assert back == bytes(expected), "round trip must reproduce the source layout, FPRs included"

    # A state written by mupen64plus-next: last_read and flash come from the
    # extra block, next_vi from the VI event, SPECIAL_INT returns to count 0
    # (next's half-wrap point stalls this core) and 64DD events are dropped.
    tail = bridge.HEADER_SIZE + bridge.OURS_BASE_SIZE
    from_next = bridge.to_parallel(next_19_state(source, bridge.NEXT_FLASH_READ_ARRAY))
    assert bridge.u32(from_next, bridge.AI_LAST_READ) == 0x1234
    assert struct.unpack_from("<I", from_next, tail - 8)[0] == VI_COUNT
    assert from_next[bridge.FLASH + 4:bridge.FLASH + 24] == struct.pack(
        "<iQ2I", bridge.OURS_FLASH_READ, bridge.OURS_FLASH_IDLE_STATUS, 0, 0)
    assert bridge.read_queue(from_next, tail, len(from_next)) == \
        [(0x40, AI_COUNT), (bridge.VI_INT, VI_COUNT), (bridge.SPECIAL_INT, 0)]
    assert list(struct.unpack_from("<64I", from_next, bridge.CP1_REGS)) == FPR_WORDS

    def flash_after(**kwargs):
        state = bridge.to_parallel(next_19_state(source, **kwargs))
        return struct.unpack_from("<iQ", state, bridge.FLASH + 4)

    assert flash_after(mode=bridge.NEXT_FLASH_STATUS) == \
        (bridge.OURS_FLASH_STATUS, 0x1111800800C20000), "status register byte rides in the high word"
    assert flash_after(mode=bridge.NEXT_FLASH_SILICON_ID) == \
        (bridge.OURS_FLASH_STATUS, 0x1111800100C2001E), "silicon ID fills the 64-bit status"
    assert flash_after(mode=bridge.NEXT_FLASH_STATUS, gb_cart_in_pak=2) == \
        (bridge.OURS_FLASH_STATUS, 0x1111800800C20000), "a GB cart in a Transfer Pak moves the flash block"
    assert flash_after(mode=5, use_flashram=0) == (0, 0), "flash state is ignored when the cart has none"

    expect_refusal(bridge.to_parallel, next_19_state(source, 5), "a flash program in flight")
    running = bytearray(next_19_state(source, 0))
    struct.pack_into("<I", running, bridge.SP_STATUS, 0)
    expect_refusal(bridge.to_parallel, bytes(running), "a running RSP")
    expect_refusal(bridge.to_parallel,
                   next_19_state(source, 0, events=[(bridge.VI_INT, VI_COUNT), (bridge.RSP_DMA_EVT, 3)]),
                   "an RSP DMA in flight")
    expect_refusal(bridge.to_parallel, next_19_state(source, 0, events=[(0x40, AI_COUNT)]),
                   "a state without a pending VI")


def check_cli(source, converted):
    with tempfile.TemporaryDirectory(prefix="savestate-bridge-") as directory:
        root = Path(directory)
        (root / "ours.state").write_bytes(wrap_rzip(source))
        cli = [sys.executable, str(TOOLS / "savestate_bridge.py")]
        subprocess.run(cli + ["to-gliden64", str(root / "ours.state"), str(root / "next.state")], check=True)
        assert (root / "next.state").read_bytes() == converted
        report = subprocess.run(cli + ["inspect", str(root / "next.state")],
                                check=True, capture_output=True, text=True).stdout
        assert "mupen64plus-next version 00010100" in report and "FR=0" in report
        (root / "running.state").write_bytes(ours_state(sp_status=0))
        failed = subprocess.run(cli + ["to-gliden64", str(root / "running.state"), str(root / "x.state")],
                                capture_output=True, text=True, check=False)
        assert failed.returncode == 1 and "RSP was running" in failed.stderr
        assert not (root / "x.state").exists()


def main():
    check_layout_constants()
    source = ours_state()
    converted = check_to_gliden64(source)
    check_to_parallel(source, converted)
    check_cli(source, converted)
    print("emu_savestate_bridge_contract: PASS")


if __name__ == "__main__":
    main()
