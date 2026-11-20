"""Exercise tools/sweep/run.py against a fake adapter, without an emulator."""
import io
import json
import os
from pathlib import Path
import contextlib
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools/sweep"))
import report  # noqa: E402
import run  # noqa: E402

FIXTURES = REPO / "tests/emulator_behavior/fixtures/sweep"

# Mimics the interactive adapter subcommands run_scenarios uses. FAKE_SWEEP_PLAN
# picks the picture ("vary", "static", "black") and faults ("step_fail_at", "stop_fail").
FAKE_ADAPTER = r'''#!/usr/bin/env python3
import json, os, sys
from pathlib import Path
from PIL import Image

plan = json.loads(os.environ.get("FAKE_SWEEP_PLAN", "{}"))
sub, rest, opts = sys.argv[1], sys.argv[2:], {}
while rest:
    if rest[0] in ("--start-paused", "--paused"):
        rest.pop(0)
    else:
        opts[rest[0]] = rest[1]
        del rest[:2]
if sub == "doctor":
    print("DOCTOR clean")
    raise SystemExit(0)
bundle = Path(opts["--bundle-dir"])
state_file = bundle / "fake.json"
with (bundle.parents[3] / "adapter-calls.log").open("a") as calls:
    calls.write(sub + "\n")
if sub == "start":
    (bundle / "captures").mkdir(parents=True, exist_ok=True)
    (bundle / "start-options.json").write_text(json.dumps(opts))
    (bundle / "logs/retroarch.log").write_text(plan.get("log", "[INFO] Content loaded\n"))
    state_file.write_text(json.dumps({"frame": 0}))
    raise SystemExit(0)
st = json.loads(state_file.read_text())
if sub == "status":
    print(f"GET_STATUS PAUSED n64,content,crc32=1,frame={st['frame']}")
elif sub == "send":
    command = opts["--command"]
    with (bundle / "commands.log").open("a") as out:
        out.write(command + "\n")
    if command.startswith("STEP_FRAME"):
        if "step_fail_at" in plan and st["frame"] >= plan["step_fail_at"]:
            print(f"{command} did not complete (still draining, or the session is not paused).", file=sys.stderr)
            raise SystemExit(1)
        st["frame"] += int(command.split()[1])
    print(command)
elif sub == "screenshot":
    shade = {"vary": st["frame"] * 5 % 200, "static": 90, "black": 0}[plan.get("image", "vary")]
    image = Image.new("RGB", (64, 48), (0, 0, 0))
    if plan.get("image") != "black":
        for i in range(4):
            image.paste((40 + shade, 60 * i, 200 - 40 * i), (16 * i, 0, 16 * i + 16, 48))
    capture = bundle / "captures" / f"shot-{st['frame']:06d}.png"
    image.save(capture)
    print(capture)
elif sub == "stop":
    if plan.get("stop_fail"):
        print("[interactive] stop failed.", file=sys.stderr)
        raise SystemExit(1)
    print("[interactive] session exited cleanly.")
state_file.write_text(json.dumps(st))
'''

PATTERN = {"schema": "n64-sweep-input/1", "frames": 40, "captures": [10, 20, 30, 40], "hold_frames": 2,
           "frozen_min_captures": 3, "presses": [{"frame": 5, "input": "down"}, {"frame": 8, "input": "a"}]}


class SweepRun(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.adapter = self.root / "fake_adapter.py"
        self.adapter.write_text(FAKE_ADAPTER)
        self.adapter.chmod(0o755)
        for name in ("core.so", "retroarch"):
            (self.root / name).write_bytes(name.encode())
        self.library = self.root / "library"
        self.library.mkdir()
        (self.library / "Alpha (USA).z64").write_bytes(b"\x80\x37\x12\x40alpha")
        with zipfile.ZipFile(self.library / "Beta (Europe).zip", "w") as archive:
            archive.writestr("Beta (Europe).n64", b"\x40\x12\x37\x80beta")
            archive.writestr("readme.txt", b"not a rom")
        self.pattern = self.root / "pattern.json"
        self.pattern.write_text(json.dumps(PATTERN))
        self.out = self.root / "out"

    def tearDown(self):
        self.tmp.cleanup()

    def sweep(self, *extra, plan=None, check_options=False):
        argv = ["--roms", str(self.library), "--core", str(self.root / "core.so"), "--profile", "paralleish",
                "--retroarch-bin", str(self.root / "retroarch"), "--out", str(self.out),
                "--runtime-class", "fake-1x", "--input", str(self.pattern), "--adapter", str(self.adapter),
                *([] if check_options else ["--no-option-check"]), *extra]
        stdout, stderr = io.StringIO(), io.StringIO()
        with mock.patch.dict(os.environ, {"FAKE_SWEEP_PLAN": json.dumps(plan or {})}), \
                contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            status = run.main(argv)
        return status, stdout.getvalue() + stderr.getvalue()

    def records(self):
        return {r["rom"]["name"]: r for r in (json.loads(p.read_text())
                                              for p in sorted(self.out.glob("runs/*/record.json")))}

    def starts(self):
        log = self.out / "adapter-calls.log"
        return log.read_text().split().count("start") if log.is_file() else 0

    def test_records_options_inputs_and_captures_per_rom(self):
        status, output = self.sweep()
        self.assertEqual(status, 0, output)
        records = self.records()
        self.assertEqual(sorted(records), ["Alpha (USA).z64", "Beta (Europe).zip"])
        beta = records["Beta (Europe).zip"]
        self.assertEqual((beta["verdict"], beta["frames_reached"], len(beta["captures"])), ("pass", 40, 4))
        self.assertEqual(beta["rom"]["member"], "Beta (Europe).n64")
        self.assertEqual(beta["options"]["requested"]["parallel-n64-pak1"], "memory")
        run_dir = self.out / "runs" / beta["key"]
        self.assertIn('parallel-n64-pak1 = "memory"', (run_dir / "core-options.opt").read_text())
        self.assertTrue((run_dir / "contact.jpg").is_file())
        bundle = run_dir / beta["bundle"]
        # The ROM is staged under a fixed content name; the frontend config points at the run's options.
        start = json.loads((bundle / "start-options.json").read_text())
        self.assertTrue(start["--rom"].endswith("/content.n64"))
        self.assertIn(f'core_options_path = "{run_dir / "core-options.opt"}"', (run_dir / "frontend.cfg").read_text())
        commands = (bundle / "commands.log").read_text().splitlines()
        self.assertEqual(commands[:3], ["STEP_FRAME 5", "SET_INPUT_PORT 0 0x20 0 32767 0 0", "STEP_FRAME 2"])
        self.assertEqual(beta["input"]["source"], "default")
        # report.py reads the records run.py writes.
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(report.main([str(self.out), "--out", str(self.root / "report")]), 0)
        self.assertIn("| out | paralleish |", (self.root / "report/summary.md").read_text())

    def test_rerun_skips_complete_runs_and_restarts_incomplete_ones(self):
        self.assertEqual(self.sweep()[0], 0)
        self.assertEqual(self.starts(), 2)
        status, output = self.sweep()
        self.assertEqual((status, self.starts()), (0, 2), output)
        self.assertIn("skipped 2 complete", output)
        alpha = self.records()["Alpha (USA).z64"]
        (self.out / "runs" / alpha["key"] / "record.json").unlink()
        self.assertEqual(self.sweep()[0], 0)
        self.assertEqual(self.starts(), 3)
        moved = [p.name for p in (self.out / "runs").iterdir() if ".incomplete-" in p.name]
        self.assertEqual(len(moved), 1)
        self.assertEqual(self.sweep("--retry", "pass")[0], 0)
        self.assertEqual(self.starts(), 5)

    def test_override_changes_only_that_roms_key_and_program(self):
        self.sweep()
        before = {name: r["key"] for name, r in self.records().items()}
        alpha_sha = self.records()["Alpha (USA).z64"]["rom_sha256"]
        overrides = self.root / "overrides"
        overrides.mkdir()
        (overrides / f"{alpha_sha}.json").write_text(json.dumps({**PATTERN, "presses": [{"frame": 3, "input": "start"}]}))
        self.sweep("--input-overrides", str(overrides))
        keys = {}
        for path in self.out.glob("runs/*/record.json"):
            record = json.loads(path.read_text())
            keys.setdefault(record["rom"]["name"], set()).add(record["key"])
        self.assertEqual(keys["Beta (Europe).zip"], {before["Beta (Europe).zip"]})
        self.assertEqual(len(keys["Alpha (USA).z64"]), 2)
        self.assertEqual(self.starts(), 3)

    def test_failures_are_classified(self):
        self.sweep("--limit", "1", plan={"step_fail_at": 20})
        hang = self.records()["Alpha (USA).z64"]
        self.assertEqual((hang["verdict"], hang["sub_class"], hang["frames_reached"]), ("hang", "step", 20))
        self.out = self.root / "quit"
        self.sweep("--limit", "1", plan={"stop_fail": True, "image": "static"})
        quit_fail = self.records()["Alpha (USA).z64"]
        self.assertEqual((quit_fail["verdict"], quit_fail["sub_class"], quit_fail["screen"], quit_fail["screen_sub"]),
                         ("quit-fail", "stop-failed", "frozen", "static-from-start"))
        self.out = self.root / "black"
        self.sweep("--limit", "1", plan={"image": "black"})
        self.assertEqual(self.records()["Alpha (USA).z64"]["verdict"], "blank")

    def test_profile_options_the_core_does_not_declare_stop_the_sweep(self):
        declared = {"parallel-n64-gfxplugin": {"default": "auto", "values": ["auto", "parallel"]},
                    "parallel-n64-parallel-rdp-upscaling": {"default": "1x", "values": ["1x", "2x"]},
                    "parallel-n64-pak1": {"default": "none", "values": ["none", "rumble"]}}
        fake = {"library": {"name": "Fake", "version": "1"}, "options": declared}
        with mock.patch.object(run, "query_core", return_value=fake):
            status, output = self.sweep(check_options=True)
        self.assertEqual((status, self.starts()), (2, 0))
        self.assertIn("parallel-n64-pak1='memory'", output)
        declared["parallel-n64-pak1"]["values"].append("memory")
        declared["parallel-n64-cpucore"] = {"default": "dynamic_recompiler", "values": ["dynamic_recompiler"]}
        with mock.patch.object(run, "query_core", return_value=fake):
            self.assertEqual(self.sweep("--limit", "1", check_options=True)[0], 0)
        effective = next(iter(self.records().values()))["options"]["effective"]
        self.assertEqual((effective["parallel-n64-pak1"], effective["parallel-n64-cpucore"]),
                         ("memory", "dynamic_recompiler"))


class Identity(unittest.TestCase):
    def identity(self, **change):
        args = {"profile_name": "paralleish", "requested": {"k": "v"}, "frontend": {"append_config": {}},
                "frontend_sha256": "f" * 64, "base_config": 'config_save_on_exit = "false"\n',
                "env": {"RETROARCH_VIDEO_CONTEXT_DRIVER": "headless_vk", "HOME": "/home/x"}}
        args.update(change)
        return run.canonical_hash(run.config_identity(**args))

    def test_config_hash_covers_conditions_but_not_host_details(self):
        same = self.identity()
        self.assertEqual(same, self.identity(env={"RETROARCH_VIDEO_CONTEXT_DRIVER": "headless_vk", "HOME": "/y"}))
        for change in ({"requested": {"k": "w"}}, {"frontend_sha256": "e" * 64}, {"base_config": ""},
                       {"env": {}}, {"profile_name": "other"}):
            self.assertNotEqual(same, self.identity(**change), change)

    def test_run_key_names_core_rom_and_config(self):
        key, config = run.run_key("c" * 64, "r" * 64, "h" * 64, "i" * 64)
        self.assertEqual(key, f"{'r' * 16}-{'c' * 12}-{config[:12]}")
        self.assertNotEqual(config, run.run_key("c" * 64, "r" * 64, "h" * 64, "j" * 64)[1])

    def test_rom_list_file_resolves_relative_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            listing = Path(directory) / "tier1.txt"
            listing.write_text("# tier 1\nroms/a.z64\n\n/abs/b.zip\n")
            self.assertEqual(run.list_roms(listing), [Path(directory) / "roms/a.z64", Path("/abs/b.zip")])


class Profiles(unittest.TestCase):
    def test_every_profile_sets_the_same_port1_accessory(self):
        profiles = run.load_profiles(run.PROFILES)["cores"]
        self.assertEqual({p["port1_accessory"]["value"] for p in profiles.values()}, {"memory"})

    def test_profiles_match_recorded_core_option_metadata(self):
        # Declared options recorded from the cores swept on November 16.
        declared = json.loads((FIXTURES / "declared-options.json").read_text())
        profiles = run.load_profiles(run.PROFILES)["cores"]
        for name, options in declared.items():
            requested = run.requested_options(profiles[name], {})
            self.assertEqual(run.option_problems(requested, options), [], name)

    def test_overrides_follow_the_accessory(self):
        profile = {"options": {"a": "1", "pak": "rumble"}, "port1_accessory": {"option": "pak", "value": "memory"}}
        self.assertEqual(run.requested_options(profile, {}), {"a": "1", "pak": "memory"})
        self.assertEqual(run.requested_options(profile, {"pak": "none"})["pak"], "none")


if __name__ == "__main__":
    unittest.main()
