"""Exercise tools/regression/run_scenarios.py against a fake adapter, without an emulator."""
import gzip
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
core_states = bundle / "states/ParaLLEish"

def emit(line):
    with log.open("a") as out:
        out.write(line + "\n")

if sub == "start":
    assert "--start-paused" in flags and opts["--mode"] in ("off", "on")
    (bundle / "captures").mkdir(parents=True, exist_ok=True)
    if "--state-source" in opts:
        source = Path(opts["--state-source"])
        shutil.copytree(source, bundle / "states" / source.name)
    if "--savefile-source" in opts:
        savefiles = bundle / "savefiles/ParaLLEish"
        savefiles.mkdir(parents=True)
        shutil.copyfile(opts["--savefile-source"], savefiles / (Path(opts["--rom"]).stem + ".srm"))
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
    body += f"|frame={st['frame']}|inputs={st['inputs']}|{plan.get('state', '')}".encode()
    packed = zlib.compress(body)
    core_states.mkdir(parents=True, exist_ok=True)
    (core_states / f"{st['stem']}.state{opts['--slot']}").write_bytes(
        b"#RZIPv\x01#" + struct.pack("<IQ", 1 << 17, len(body)) + struct.pack("<I", len(packed)) + packed)
elif sub == "stop":
    if st["mode"] == "on":
        counts = plan.get("lookups", "lookups=5 hits=4 misses=1")
        draw_hits = plan.get("draw_hits", 3)
        emit(f"[INFO] Hi-res keying summary: {counts} filtered=0 block_probe_hits=0 "
             f"compat_draw_hits={draw_hits} compat_draw_ci_hits=0 compat_draw_ci_attempts=0 compat_draw_bank0_hits=0 "
             "provider=on entries=12 native_sampled=0 compat=12 sampled_index=0 sampled_dupe_keys=0 "
             "sampled_dupe_entries=0 sampled_families=0 compat_low32_families=12 sources(phrb=12) "
             "descriptor_paths(sampled=0 native_checksum=0 generic=0 compat=3) "
             "sampled_detail(family_singleton=0 ordered_surface_singleton=0 exact_selector=0) "
             "generic_detail(identity_assisted=0 plain=0 native=0 compat=0 unknown=0).")
        if "jabo" in plan:
            emit(f"[INFO] Hi-res Jabo summary: {plan['jabo']} md5=1 memo_hits=0.")
    if plan.get("fail") == "stop":
        print("[interactive] stop failed.", file=sys.stderr)
        state_file.write_text(json.dumps(st))
        raise SystemExit(1)
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
        self.state = state = b"M64+SAVE" + b"\x01" * 32
        self.srm = srm = b"\x00" * 64
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

    def run_runner(self, out, *extra, plan=None, mode="off", packs=None):
        argv = [sys.executable, str(RUNNER), "--scenarios", str(self.scenarios),
                "--blobs", str(self.root / "blobs"), "--core", str(self.root / "core.so"),
                "--retroarch-bin", str(self.root / "retroarch"), "--base-config", str(self.root / "base.cfg"),
                "--runtime-class", "fake-4x", "--mode", mode, "--out", str(self.root / out),
                "--adapter", str(self.adapter), "--orphan-grace", "0.5", "--step-chunk", "50", *extra]
        if mode == "on":
            argv += packs if packs is not None else ["--pack", str(self.root / "pack.phrb")]
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
        saved = self.root / "golden-off/bundles/field-state__off__fake-4x/states/ParaLLEish"
        rzip = next(saved.glob("*.state2")).read_bytes()
        self.assertEqual(entry["end_state_sha256"], sha256(zlib.decompress(rzip[24:])))

        result, records = self.run_runner("clean", "--expectations", str(expectations))
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        for record in records.values():
            self.assertEqual(record["exit_class"], "clean")
            self.assertEqual({s for s in self.gates(record).values()}, {"pass"}, record["gates"])
        commands = (self.root / "clean/bundles/field-state__off__fake-4x/commands.log").read_text().split("\n")
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
        self.assertEqual((gates["hires.provider"], gates["hires.entries"], gates["hires.served"]),
                         ("pass", "pass", "pass"))
        self.assertEqual(gates["end_state"], "pass")
        self.assertEqual(record["hires"]["draw_hits"], 3)
        report = (self.root / "on/report.md").read_text()
        review = report.split("## Review queue")[1]
        self.assertIn(record["checkpoints"][0]["capture_path"], review)

    def pack_map(self, name, mapping):
        path = self.root / name
        path.write_text(json.dumps({sha: str(self.root / pack) for sha, pack in mapping.items()}))
        return ["--pack-map", str(path)]

    def test_pack_map_runs_each_game_with_its_own_pack(self):
        other_rom = b"other fake rom"
        (self.root / "blobs/roms" / f"{sha256(other_rom)}.z64").write_bytes(other_rom)
        self.write_scenario({"schema": "n64-regression-scenario/1", "id": "other-game",
                             "game": {"rom_sha256": sha256(other_rom)}, "start": {"kind": "power-on"},
                             "program": ["STEP_FRAME 30"], "checkpoints": [{"after_frames": 30}],
                             "end": {"save_state": True}, "tags": {"tier": ["nightly"]}})
        result, records = self.run_runner("mapped", mode="on",
                                          packs=self.pack_map("packs.json", {sha256(b"fake rom"): "pack.phrb"}))
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertEqual(set(records), {"field-state", "boot-power-on"})
        self.assertIn(f"no pack mapped for ROM {sha256(other_rom)[:12]}: 1 scenario(s) not run", result.stdout)
        self.assertEqual({r["pack_sha256"] for r in records.values()}, {sha256(b"pack.phrb")})

    def test_malformed_pack_map_is_a_configuration_error(self):
        for value in ([], {"abc": None}, {"abc": 3}):
            path = self.root / "bad-packs.json"
            path.write_text(json.dumps(value))
            argv = [sys.executable, str(RUNNER), "--scenarios", str(self.scenarios), "--blobs", str(self.root / "blobs"),
                    "--core", str(self.root / "core.so"), "--retroarch-bin", str(self.root / "retroarch"),
                    "--base-config", str(self.root / "base.cfg"), "--runtime-class", "fake-4x", "--mode", "on",
                    "--out", str(self.root / "bad"), "--adapter", str(self.adapter), "--pack-map", str(path)]
            result = subprocess.run(argv, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertNotIn("Traceback", result.stderr)

    def test_off_only_scenarios_are_not_run_in_on_mode(self):
        path = self.scenarios / "boot-power-on.json"
        scenario = json.loads(path.read_text())
        path.write_text(json.dumps({**scenario, "modes": ["off"]}))
        result, records = self.run_runner("off-only", mode="on")
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertEqual(set(records), {"field-state"})
        self.assertIn("declare no on-mode coverage and are not run: boot-power-on", result.stdout)
        _, records = self.run_runner("off-only-off")
        self.assertEqual(set(records), {"field-state", "boot-power-on"})
        path.write_text(json.dumps({**scenario, "modes": ["off", "off"]}))
        result = subprocess.run([sys.executable, str(RUNNER), "--scenarios", str(self.scenarios),
                                 "--core", str(self.root / "core.so"), "--retroarch-bin", str(self.root / "retroarch"),
                                 "--base-config", str(self.root / "base.cfg"), "--runtime-class", "fake-4x",
                                 "--out", str(self.root / "bad-modes"), "--adapter", str(self.adapter)],
                                capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 2)
        self.assertIn("modes must be", result.stderr)

    def test_on_mode_golden_from_another_pack_is_review(self):
        expectations = self.record_golden(mode="on")
        (self.root / "pack2.phrb").write_bytes(b"pack2")
        result, records = self.run_runner("swapped", "--expectations", str(expectations), "--select", "field-state",
                                          mode="on", packs=self.pack_map("swap.json", {sha256(b"fake rom"): "pack2.phrb"}))
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertEqual(self.gates(records["field-state"])["pack"], "review")

    def test_on_mode_end_state_change_fails(self):
        expectations = self.record_golden(mode="on")
        result, records = self.run_runner("on-state", "--expectations", str(expectations), "--select", "field-state",
                                          plan={"state": "drifted"}, mode="on")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertEqual(self.gates(records["field-state"])["end_state"], "fail")

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
        marker = str(self.root / "orphan/bundles/boot-power-on__off__fake-4x/fake-frontend")
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            listing = subprocess.run(["ps", "-axww", "-o", "args="], capture_output=True, text=True).stdout
            if marker not in listing:
                break
            time.sleep(0.1)
        self.assertNotIn(marker, listing)

    def test_served_gate_skips_sessions_without_lookups(self):
        _, records = self.run_runner("idle", "--select", "boot-power-on", mode="on",
                                     plan={"lookups": "lookups=0 hits=0 misses=0", "draw_hits": 0})
        self.assertEqual(self.gates(records["boot-power-on"])["hires.served"], "skip")
        _, records = self.run_runner("unserved", "--select", "boot-power-on", mode="on",
                                     plan={"lookups": "lookups=3 hits=0 misses=3", "draw_hits": 0})
        self.assertEqual(self.gates(records["boot-power-on"])["hires.served"], "fail")

    def test_served_gate_counts_jabo_hits(self):
        _, records = self.run_runner("jabo", "--select", "boot-power-on", mode="on",
                                     plan={"lookups": "lookups=3 hits=0 misses=3", "draw_hits": 0,
                                           "jabo": "entries=9 hits=2 misses=1"})
        record = records["boot-power-on"]
        self.assertEqual(self.gates(record)["hires.served"], "pass")
        self.assertEqual(record["hires"]["jabo"], {"entries": 9, "hits": 2, "misses": 1})

    def test_failed_stop_fails_the_exit_gate(self):
        result, records = self.run_runner("stop", "--select", "boot-power-on", plan={"fail": "stop"})
        self.assertEqual(result.returncode, 1)
        record = records["boot-power-on"]
        self.assertEqual(record["exit_class"], "nonzero")
        self.assertIn("stop-failed", record["log_flags"])
        self.assertEqual(self.gates(record)["exit"], "fail")

    def test_bundles_are_kept_per_runtime_class(self):
        self.run_runner("classes", "--select", "boot-power-on")
        self.run_runner("classes", "--select", "boot-power-on", "--runtime-class", "other-4x", "--resume")
        records = [json.loads(line) for line in (self.root / "classes/results.jsonl").read_text().splitlines()]
        self.assertEqual(len({record["bundle"] for record in records}), 2)
        for record in records:
            self.assertTrue((self.root / "classes" / record["bundle"] / "logs").is_dir(), record["bundle"])

    def compress_blobs(self, kind, content=None):
        """Replace each raw blob of `kind` with `<name>.gz` holding its bytes (or `content`)."""
        for path in list((self.root / "blobs" / kind).iterdir()):
            data = path.read_bytes() if content is None else content
            path.with_name(f"{path.name}.gz").write_bytes(gzip.compress(data))
            path.unlink()

    def test_compressed_blobs_stage_their_raw_bytes(self):
        _, raw = self.run_runner("raw", "--select", "field-state")
        self.compress_blobs("states")
        self.compress_blobs("savefiles")
        result, packed = self.run_runner("packed", "--select", "field-state")
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        record = packed["field-state"]
        self.assertEqual(record["exit_class"], "clean", record["error"])
        self.assertEqual(record["end_state_sha256"], raw["field-state"]["end_state_sha256"])
        bundle = self.root / "packed/bundles/field-state__off__fake-4x"
        self.assertEqual(next((bundle / "states/ParaLLEish").glob("*.state1")).read_bytes(), self.state)
        self.assertEqual(next((bundle / "savefiles/ParaLLEish").glob("*.srm")).read_bytes(), self.srm)

    def test_compressed_blob_must_decompress_to_its_name(self):
        self.compress_blobs("states", content=self.state + b"tampered")
        result, records = self.run_runner("tampered", "--select", "field-state")
        self.assertEqual(result.returncode, 1, result.stdout)
        record = records["field-state"]
        self.assertEqual(record["exit_class"], "not-run")
        self.assertIn(".state.gz does not match its name", record["error"])

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
