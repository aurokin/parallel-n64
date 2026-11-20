"""Check boot-sweep input patterns and the scenario programs they generate."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools/sweep"))
import input_pattern  # noqa: E402

spec = importlib.util.spec_from_file_location("regression_runner", REPO / "tools/regression/run_scenarios.py")
runner = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runner
spec.loader.exec_module(runner)

DEFAULT = REPO / "tools/sweep/patterns/default.json"


def replay(program):
    """(frame, joypad mask, stick axes) at each SET_INPUT_PORT, and the total frames stepped."""
    frame, presses = 0, []
    for command in program:
        verb, *args = command.split()
        if verb == "STEP_FRAME":
            frame += int(args[0])
        elif verb == "SET_INPUT_PORT":
            presses.append((frame, int(args[1], 16), tuple(int(a) for a in args[2:]) or (0, 0, 0, 0)))
    return presses, frame


class DefaultPattern(unittest.TestCase):
    def setUp(self):
        self.pattern = input_pattern.load(DEFAULT)
        self.program = input_pattern.program(self.pattern)

    def test_presses_land_on_their_frames_and_release(self):
        presses, total = replay(self.program)
        self.assertEqual(total, self.pattern["frames"])
        self.assertEqual([p[0] for p in presses], [p["frame"] for p in self.pattern["presses"]])
        self.assertEqual(self.program.count("CLEAR_INPUT_PORT 0"), len(presses))
        self.assertEqual(input_pattern.program(input_pattern.load(DEFAULT)), self.program)

    def test_directions_press_dpad_and_stick_and_no_cancel_or_select(self):
        presses, _ = replay(self.program)
        down, right = input_pattern.INPUTS["down"], input_pattern.INPUTS["right"]
        self.assertIn((2400, down[0], down[1]), presses)
        self.assertIn((3300, right[0], right[1]), presses)
        self.assertTrue(down[0] and down[1][1] > 0 and right[0] and right[1][0] > 0)
        used = {mask for _, mask, _ in presses}
        self.assertFalse(any(mask & (input_pattern.INPUTS["b"][0] | 1 << 2) for mask in used))  # B, Select
        self.assertEqual(presses[0][0], 1200)  # boot runs untouched

    def test_generated_scenario_satisfies_the_regression_schema(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "s.json"
            path.write_text(json.dumps({
                "schema": runner.SCENARIO_SCHEMA, "id": "sweep-x", "game": {"rom_sha256": "0" * 64},
                "start": {"kind": "power-on"}, "program": self.program,
                "checkpoints": [{"after_frames": f, "capture": True} for f in self.pattern["captures"]]}))
            scenario = runner.parse_scenario(path)
        self.assertEqual(scenario.total_frames, 4800)
        plan = runner.step_plan(scenario, 120)
        self.assertEqual([c.after_frames for kind, c in plan if kind == "checkpoint"], self.pattern["captures"])


class Validation(unittest.TestCase):
    def base(self):
        return {"schema": input_pattern.SCHEMA, "frames": 100, "captures": [50, 100], "hold_frames": 4,
                "frozen_min_captures": 2, "presses": [{"frame": 10, "input": "a"}]}

    def rejected(self, change):
        data = self.base()
        change(data)
        with self.assertRaises(input_pattern.PatternError):
            input_pattern.validate(data)

    def test_rejects_malformed_patterns(self):
        self.rejected(lambda d: d["presses"].append({"frame": 12, "input": "start"}))  # overlaps the hold
        self.rejected(lambda d: d["presses"].append({"frame": 98, "input": "start"}))  # ends past the run
        self.rejected(lambda d: d["presses"].append({"frame": 40, "input": "select"}))
        self.rejected(lambda d: d["presses"].append({"frame": 40, "input": ["up", "down"]}))
        self.rejected(lambda d: d.update(captures=[100, 50]))
        self.rejected(lambda d: d.update(frozen_min_captures=3))

    def test_combined_inputs_and_custom_hold(self):
        data = self.base()
        data["presses"] = [{"frame": 0, "input": ["down", "a"], "hold": 20}]
        input_pattern.validate(data)
        self.assertEqual(input_pattern.program(data),
                         ["SET_INPUT_PORT 0 0x21 0 32767 0 0", "STEP_FRAME 20", "CLEAR_INPUT_PORT 0", "STEP_FRAME 80"])

    def test_digest_tracks_behavior_not_notes(self):
        data = self.base()
        noted = copy.deepcopy(data)
        noted["description"] = "other words"
        noted["presses"][0]["why"] = "because"
        self.assertEqual(input_pattern.digest(data), input_pattern.digest(noted))
        moved = copy.deepcopy(data)
        moved["presses"][0]["frame"] = 11
        self.assertNotEqual(input_pattern.digest(data), input_pattern.digest(moved))

    def test_override_replaces_default_for_its_rom_only(self):
        default = self.base()
        with tempfile.TemporaryDirectory() as directory:
            override = self.base()
            override["presses"] = [{"frame": 20, "input": "right"}]
            (Path(directory) / f"{'ab' * 32}.json").write_text(json.dumps(override))
            chosen, source = input_pattern.for_rom(default, Path(directory), "ab" * 32)
            self.assertEqual((chosen["presses"], source), (override["presses"], f"override:{'ab' * 32}.json"))
            self.assertEqual(input_pattern.for_rom(default, Path(directory), "cd" * 32), (default, "default"))


if __name__ == "__main__":
    unittest.main()
