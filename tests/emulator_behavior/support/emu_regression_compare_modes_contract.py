"""Check the off/on presentation-only comparison on synthetic run directories."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

TOOLS = Path(__file__).resolve().parents[3] / "tools"
sys.path.insert(0, str(TOOLS))
import savestate_bridge as bridge

COMPARE = TOOLS / "regression" / "compare_modes.py"
SIZE = bridge.HEADER_SIZE + bridge.OURS_BASE_SIZE + 4


def write_run(root: Path, mode: str, state: bytes, cue: str) -> Path:
    run = root / mode
    states = run / "bundles" / f"probe__{mode}" / "states" / "ParaLLEl N64"
    states.mkdir(parents=True)
    (states / "Game.state2").write_bytes(state)
    record = {"scenario": "probe", "checkpoints": [{"ram": {"cue": cue}}]}
    (run / "results.jsonl").write_text(json.dumps(record) + "\n")
    return run


def compare(off: bytes, on: bytes, off_cue: str = "01", on_cue: str = "01"):
    with tempfile.TemporaryDirectory(prefix="compare-modes-") as directory:
        root = Path(directory)
        result = subprocess.run([sys.executable, str(COMPARE), str(write_run(root, "off", off, off_cue)),
                                 str(write_run(root, "on", on, on_cue))],
                                capture_output=True, text=True, check=False)
        return result.returncode, result.stdout


def main():
    off = bytearray(SIZE)
    on = bytearray(off)
    framebuffer = bridge.RDRAM + 0x392000
    on[framebuffer:framebuffer + 64] = b"\x7f" * 64
    status, output = compare(bytes(off), bytes(on))
    assert status == 0 and "non-RDRAM state identical" in output and "0x392000-0x393000" in output, output

    register = bytearray(on)
    register[bridge.CP0_STATUS] ^= 1
    status, output = compare(bytes(off), bytes(register))
    assert status == 1 and "state outside RDRAM differs" in output, output

    status, output = compare(bytes(off), bytes(on), on_cue="02")
    assert status == 1 and "checkpoint RAM cues differ" in output, output
    print("emu_regression_compare_modes_contract: PASS")


if __name__ == "__main__":
    main()
