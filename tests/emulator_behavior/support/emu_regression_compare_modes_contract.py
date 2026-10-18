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


def write_run(root: Path, mode: str, state: bytes, cue: str, scenarios=("probe",)) -> Path:
    run = root / mode
    records = []
    for scenario in scenarios:
        scenario_file = root / f"{scenario}.json"
        scenario_file.write_text(json.dumps({"id": scenario, **({"modes": ["off"]} if scenario.startswith("off-only") else {})}))
        bundle = f"bundles/{scenario}__{mode}__probe-class"
        states = run / bundle / "states" / "ParaLLEl N64"
        states.mkdir(parents=True)
        (states / "Game.state2").write_bytes(state)
        records.append(json.dumps({"scenario": scenario, "bundle": bundle, "scenario_file": str(scenario_file),
                                   "checkpoints": [{"ram": {"cue": cue}}]}) + "\n")
    (run / "results.jsonl").write_text("".join(records))
    return run


def compare(off: bytes, on: bytes, off_cue: str = "01", on_cue: str = "01", off_scenarios=("probe",), extra=()):
    with tempfile.TemporaryDirectory(prefix="compare-modes-") as directory:
        root = Path(directory)
        result = subprocess.run([sys.executable, str(COMPARE), str(write_run(root, "off", off, off_cue, off_scenarios)),
                                 str(write_run(root, "on", on, on_cue)), *extra],
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

    status, output = compare(bytes(off), bytes(on), extra=("--strict-rdram",))
    assert status == 1 and "RDRAM differs" in output, output
    status, output = compare(bytes(off), bytes(off), extra=("--strict-rdram",))
    assert status == 0, output

    status, output = compare(bytes(off), bytes(on), on_cue="02")
    assert status == 1 and "checkpoint RAM cues differ" in output, output

    status, output = compare(bytes(off), bytes(on), off_scenarios=("probe", "interrupted"))
    assert status == 1 and "interrupted: no on result" in output, output

    status, output = compare(bytes(off), bytes(on), off_scenarios=("probe", "off-only-menu"))
    assert status == 0 and "off-only-menu: declares no on-mode coverage" in output, output
    print("emu_regression_compare_modes_contract: PASS")


if __name__ == "__main__":
    main()
