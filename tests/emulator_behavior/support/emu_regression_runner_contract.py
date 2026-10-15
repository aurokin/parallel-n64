"""Exercise tools/regression/run_scenarios.py against a fake adapter, without an emulator."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
import zlib

REPO = Path(__file__).resolve().parents[3]
RUNNER = REPO / "tools/regression/run_scenarios.py"

# Mimics the interactive adapter's CLI and outputs. FAKE_ADAPTER_PLAN selects
# a fault: ram/capture overrides, or fail=step|load|orphan.
FAKE_ADAPTER = r'''#!/usr/bin/env python3
import json, os, shutil, struct, subprocess, sys, zlib
from pathlib import Path

plan = json.loads(os.environ.get("FAKE_ADAPTER_PLAN", "{}"))
sub, rest, opts, flags = sys.argv[1], sys.argv[2:], {}, set()
while rest:
    if rest[0] in ("--start-paused", "--paused"):
        flags.add(rest.pop(0))
    else:
        opts[rest[0]] = rest[1]
        del rest[:2]
if sub == "doctor":
    print("DOCTOR clean")
    raise SystemExit(0)
bundle = Path(opts["--bundle-dir"])
state_file, log = bundle / "fake.json", bundle / "logs/retroarch.log"
core_states = bundle / "states/ParaLLEl N64"

def emit(line):
    with log.open("a") as out:
        out.write(line + "\n")

if sub == "start":
    assert "--start-paused" in flags and opts["--mode"] in ("off", "on")
    (bundle / "captures").mkdir(parents=True, exist_ok=True)
    if "--state-source" in opts:
        source = Path(opts["--state-source"])
        shutil.copytree(source, bundle / "states" / source.name)
    st = {"mode": opts["--mode"], "frame": 0, "stem": Path(opts["--rom"]).stem, "body": None, "inputs": []}
    state_file.write_text(json.dumps(st))
    print("[interactive] session ready")
    raise SystemExit(0)

st = json.loads(state_file.read_text())
if sub == "status":
    print(f"GET_STATUS PAUSED n64,{st['stem']},crc32=1,frame={st['frame']}")
elif sub == "send":
    command = opts["--command"]
    verb, *args = command.split()
    with (bundle / "commands.log").open("a") as out:
        out.write(command + "\n")
    if verb == "STEP_FRAME":
        if plan.get("fail") == "step" and st["body"] is not None:
            print(f"STEP_FRAME {args[0]} did not complete (still draining, or the session is not paused).",
                  file=sys.stderr)
            raise SystemExit(1)
        st["frame"] += int(args[0])
        print(command)
    elif verb == "READ_CORE_MEMORY":
        size = int(args[1])
        data = bytes.fromhex(plan["ram"]) if "ram" in plan else bytes(
            (int(args[0], 16) + st["frame"] + k) & 0xFF for k in range(size))
        print(f"READ_CORE_MEMORY {args[0]} " + data.hex(" "))
    else:
        st["inputs"].append(command)
        print(command)
elif sub == "load-slot":
    staged = core_states / f"{st['stem']}.state{opts['--slot']}"
    if plan.get("fail") == "load" or not staged.is_file() or "--paused" not in flags or st["frame"] < 1:
        emit("[State] Failed to load state")
        print("LOAD_STATE_SLOT_PAUSED failed to load state after retry.", file=sys.stderr)
        raise SystemExit(1)
    st["body"] = staged.read_bytes().hex()
    emit("[State] Loading state")
elif sub == "screenshot":
    capture = bundle / "captures" / f"shot-{st['frame']:06d}.png"
    capture.write_bytes(f"png:{st['frame']}:{plan.get('capture', 'base')}".encode())
    print(capture)
elif sub == "save-slot":
    body = bytes.fromhex(st["body"]) if st["body"] else b"M64+SAVE"
    body += f"|frame={st['frame']}|inputs={st['inputs']}".encode()
    packed = zlib.compress(body)
    core_states.mkdir(parents=True, exist_ok=True)
    (core_states / f"{st['stem']}.state{opts['--slot']}").write_bytes(
        b"#RZIPv\x01#" + struct.pack("<IQ", 1 << 17, len(body)) + struct.pack("<I", len(packed)) + packed)
elif sub == "stop":
    if st["mode"] == "on":
        emit("[INFO] Hi-res keying summary: lookups=5 hits=4 misses=1 filtered=0 block_probe_hits=0 "
             "compat_draw_hits=3 compat_draw_ci_hits=0 compat_draw_ci_attempts=0 compat_draw_bank0_hits=0 "
             "provider=on entries=12 native_sampled=0 compat=12 sampled_index=0 sampled_dupe_keys=0 "
             "sampled_dupe_entries=0 sampled_families=0 compat_low32_families=12 sources(phrb=12) "
             "descriptor_paths(sampled=0 native_checksum=0 generic=0 compat=3) "
             "sampled_detail(family_singleton=0 ordered_surface_singleton=0 exact_selector=0) "
             "generic_detail(identity_assisted=0 plain=0 native=0 compat=0 unknown=0).")
    if plan.get("fail") == "orphan":
        subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)", str(bundle / "fake-frontend")],
                         start_new_session=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("[interactive] session exited cleanly.")
state_file.write_text(json.dumps(st))
'''


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class RegressionRunnerContract(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.adapter = self.root / "fake_adapter.py"
        self.adapter.write_text(FAKE_ADAPTER)
        self.adapter.chmod(0o755)
        for name in ("core.so", "retroarch", "base.cfg", "pack.phrb"):
            (self.root / name).write_bytes(name.encode())
        rom = b"fake rom"
        state = b"M64+SAVE" + b"\x01" * 32
        srm = b"\x00" * 64
        blobs = self.root / "blobs"
        for kind, data, suffix in (("roms", rom, ".z64"), ("states", state, ".state"), ("savefiles", srm, ".srm")):
            (blobs / kind).mkdir(parents=True)
            (blobs / kind / f"{sha256(data)}{suffix}").write_bytes(data)
        self.scenarios = self.root / "scenarios"
        self.scenarios.mkdir()
        common = {"schema": "n64-regression-scenario/1", "game": {"rom_sha256": sha256(rom)},
                  "end": {"save_state": True}}
        self.write_scenario({
            **common, "id": "field-state",
            "start": {"kind": "state", "state_sha256": sha256(state), "savefile_sha256": sha256(srm)},
            "program": ["SET_INPUT_PORT 0 0x100", "STEP_FRAME 12", "CLEAR_INPUT_PORT 0", "STEP_FRAME 138"],
            "checkpoints": [
                {"after_frames": 42, "capture": True,
                 "ram": [{"name": "cue", "address": "0x800740A8", "length": 8}]},
                {"after_frames": 150, "capture": True},
            ],
            "tags": {"tier": ["nightly"]},
        })
        self.write_scenario({
            **common, "id": "boot-power-on", "start": {"kind": "power-on"},
            "program": ["STEP_FRAME 30"],
            "checkpoints": [{"after_frames": 30, "ram": [{"name": "cue", "address": "0x80000000", "length": 4}]}],
            "tags": {"tier": ["smoke", "nightly"]},
        })

    def tearDown(self):
        self.tmp.cleanup()

    def write_scenario(self, scenario):
        (self.scenarios / f"{scenario['id']}.json").write_text(json.dumps(scenario, indent=2))

    def run_runner(self, out, *extra, plan=None, mode="off"):
        argv = [sys.executable, str(RUNNER), "--scenarios", str(self.scenarios),
                "--blobs", str(self.root / "blobs"), "--core", str(self.root / "core.so"),
                "--retroarch-bin", str(self.root / "retroarch"), "--base-config", str(self.root / "base.cfg"),
                "--runtime-class", "fake-4x", "--mode", mode, "--out", str(self.root / out),
                "--adapter", str(self.adapter), "--orphan-grace", "0.5", "--step-chunk", "50", *extra]
        if mode == "on":
            argv += ["--pack", str(self.root / "pack.phrb")]
        env = {**os.environ, "FAKE_ADAPTER_PLAN": json.dumps(plan or {})}
        result = subprocess.run(argv, capture_output=True, text=True, env=env, timeout=120)
        records = [json.loads(line) for line in (self.root / out / "results.jsonl").read_text().splitlines()]
        return result, {r["scenario"]: r for r in records}

    def gates(self, record):
        return {g["name"]: g["status"] for g in record["gates"]}

    def record_golden(self, mode="off"):
        result, _ = self.run_runner(f"golden-{mode}", "--record-expectations", mode=mode)
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        return self.root / f"golden-{mode}" / "expectations"

    def test_records_expectations_then_passes_clean_run(self):
        before = {p.name: p.read_bytes() for p in self.scenarios.iterdir()}
        expectations = self.record_golden()
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.scenarios.iterdir()})

        golden = json.loads((expectations / "field-state.expect.json").read_text())
        entry = golden["conditions"]["off:fake-4x"]
        self.assertEqual(entry["core_sha256"], sha256(b"core.so"))
        self.assertEqual(set(entry["checkpoints"]), {"42", "150"})
        self.assertIn("capture_sha256", entry["checkpoints"]["42"])
        self.assertRegex(entry["checkpoints"]["42"]["ram"]["cue"], r"^[0-9a-f]{16}$")
        saved = self.root / "golden-off/bundles/field-state__off/states/ParaLLEl N64"
        rzip = next(saved.glob("*.state2")).read_bytes()
        self.assertEqual(entry["end_state_sha256"], sha256(zlib.decompress(rzip[24:])))

        result, records = self.run_runner("clean", "--expectations", str(expectations))
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        for record in records.values():
            self.assertEqual(record["exit_class"], "clean")
            self.assertEqual({s for s in self.gates(record).values()}, {"pass"}, record["gates"])
        commands = (self.root / "clean/bundles/field-state__off/commands.log").read_text().split("\n")
        steps = [int(c.split()[1]) for c in commands if c.startswith("STEP_FRAME")]
        self.assertEqual(steps, [1, 12, 30, 50, 50, 8])  # bootstrap, then chunks split at checkpoint 42
        self.assertEqual(records["field-state"]["frames_run"], 150)

        rerun, _ = self.run_runner("clean", "--expectations", str(expectations), "--resume")
        self.assertEqual(rerun.returncode, 0)
        self.assertEqual(len((self.root / "clean/results.jsonl").read_text().splitlines()), 2)

    def test_off_mode_mismatches_fail(self):
        expectations = self.record_golden()
        result, records = self.run_runner("ram", "--expectations", str(expectations),
                                          "--select", "field-state", plan={"ram": "deadbeefdeadbeef"})
        self.assertEqual(result.returncode, 1)
        self.assertEqual(self.gates(records["field-state"])["ram@42:cue"], "fail")

        result, records = self.run_runner("capture", "--expectations", str(expectations),
                                          plan={"capture": "changed"})
        self.assertEqual(result.returncode, 1)
        gates = self.gates(records["field-state"])
        self.assertEqual((gates["capture@42"], gates["capture@150"], gates["end_state"]), ("fail", "fail", "pass"))
        self.assertIn("capture@42", (self.root / "capture/report.md").read_text())

    def test_on_mode_capture_change_is_review_not_failure(self):
        expectations = self.record_golden(mode="on")
        golden = json.loads((expectations / "field-state.expect.json").read_text())
        self.assertNotIn("capture_sha256", golden["conditions"]["on:fake-4x"]["checkpoints"]["42"])

        result, records = self.run_runner("on", "--expectations", str(expectations),
                                          plan={"capture": "changed"}, mode="on")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        record = records["field-state"]
        gates = self.gates(record)
        self.assertEqual(gates["capture@42"], "review")
        self.assertEqual((gates["hires.provider"], gates["hires.entries"], gates["hires.draw_hits"]),
                         ("pass", "pass", "pass"))
        self.assertEqual(gates["end_state"], "skip")
        self.assertEqual(record["hires"]["draw_hits"], 3)
        report = (self.root / "on/report.md").read_text()
        review = report.split("## Review queue")[1]
        self.assertIn(record["checkpoints"][0]["capture_path"], review)

    def test_step_timeout_is_classified(self):
        result, records = self.run_runner("step", "--select", "field-state", plan={"fail": "step"})
        self.assertEqual(result.returncode, 1)
        self.assertEqual(records["field-state"]["exit_class"], "step-timeout")
        self.assertEqual(self.gates(records["field-state"])["capture@42"], "skip")

    def test_surviving_frontend_is_reaped(self):
        result, records = self.run_runner("orphan", "--select", "tier=smoke", plan={"fail": "orphan"})
        self.assertEqual(result.returncode, 1)
        self.assertEqual(list(records), ["boot-power-on"])
        record = records["boot-power-on"]
        self.assertEqual(record["exit_class"], "orphan-reaped")
        self.assertIn("orphan-reaped", record["log_flags"])
        marker = str(self.root / "orphan/bundles/boot-power-on__off/fake-frontend")
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            listing = subprocess.run(["ps", "-axww", "-o", "args="], capture_output=True, text=True).stdout
            if marker not in listing:
                break
            time.sleep(0.1)
        self.assertNotIn(marker, listing)

    def test_load_failure_is_classified(self):
        result, records = self.run_runner("load", plan={"fail": "load"})
        self.assertEqual(result.returncode, 1)
        record = records["field-state"]
        self.assertEqual(record["exit_class"], "load-failed")
        self.assertEqual(self.gates(record)["load"], "fail")
        self.assertIn("state-load-failed", record["log_flags"])
        self.assertEqual(records["boot-power-on"]["exit_class"], "clean")


if __name__ == "__main__":
    unittest.main()
