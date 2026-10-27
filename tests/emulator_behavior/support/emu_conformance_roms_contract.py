"""Check the conformance-ROM decoders, manifest and runner without an emulator.

The decoders read verdicts out of synthetic M64+SAVE bodies laid out like this
core's states (little-endian RDRAM words, VI registers, GPRs); the runner runs
against a fake adapter that serves prepared end states.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[3]
TOOLS = REPO / "tools"
sys.path.insert(0, str(TOOLS))
from conformance import decoders  # noqa: E402
from conformance.decoders import GPRS, PATTERNS, VI_ORIGIN, VI_WIDTH  # noqa: E402
from savestate_bridge import RDRAM  # noqa: E402

RUNNER = TOOLS / "conformance" / "run_test_roms.py"
MANIFEST = TOOLS / "conformance" / "test_roms.json"
FB = 0x100000


class Body:
    """A synthetic state body; RDRAM writes take N64 (big-endian) bytes."""

    def __init__(self) -> None:
        self.data = bytearray(GPRS + 32 * 8)
        self.data[:8] = b"M64+SAVE"

    def rdram(self, address: int, payload: bytes) -> None:
        assert address % 4 == 0 and len(payload) % 4 == 0
        for i in range(0, len(payload), 4):
            (word,) = struct.unpack(">I", payload[i:i + 4])
            struct.pack_into("<I", self.data, RDRAM + address + i, word)

    def vi(self, origin: int, width: int) -> None:
        struct.pack_into("<I", self.data, VI_ORIGIN, origin)
        struct.pack_into("<I", self.data, VI_WIDTH, width)

    def gpr(self, index: int, value: int) -> None:
        struct.pack_into("<q", self.data, GPRS + 8 * index, value)

    def cell(self, x: int, y: int, text: str, width: int) -> None:
        for row, pixels in enumerate(PATTERNS[text]):
            self.rdram(FB + ((y + row) * width + x) * 4, pixels)

    def bytes(self) -> bytes:
        return bytes(self.data)


def krom_body(cells, width=640) -> bytes:
    body = Body()
    body.vi(0xA0000000 | FB, width)
    for x, y, text in cells:
        body.cell(x, y, text, width)
    return body.bytes()


class DecoderContract(unittest.TestCase):
    REFERENCE = [[528, 32, "PASS"], [528, 56, "PASS"], [88, 56, "PASS"]]

    def test_krom_matches_reference_cells(self):
        verdict = decoders.decode_krom(krom_body(self.REFERENCE), self.REFERENCE)
        self.assertEqual(verdict.status, "pass", verdict.detail)
        self.assertEqual(verdict.observed["pass_cells"], 3)

    def test_krom_fail_cell_fails(self):
        cells = self.REFERENCE[:2] + [[88, 56, "FAIL"]]
        verdict = decoders.decode_krom(krom_body(cells), self.REFERENCE)
        self.assertEqual(verdict.status, "fail")
        self.assertIn("(88,56)", verdict.detail)

    def test_krom_compares_counts_not_positions(self):
        # Upstream screenshots can sit a pixel or two off the framebuffer grid.
        shifted = [[x, y + 1, text] for x, y, text in self.REFERENCE]
        self.assertEqual(decoders.decode_krom(krom_body(self.REFERENCE), shifted).status, "pass")

    def test_krom_missing_rows_are_incomplete(self):
        verdict = decoders.decode_krom(krom_body(self.REFERENCE[:1]), self.REFERENCE)
        self.assertEqual(verdict.status, "incomplete")

    def test_krom_uses_vi_width_as_stride(self):
        cells = [[16, 8, "PASS"]]
        self.assertEqual(decoders.decode_krom(krom_body(cells, width=400), cells).status, "pass")
        # A cell drawn with another stride is not a cell.
        body = Body()
        body.vi(FB, 640)
        body.cell(16, 8, "PASS", 400)
        self.assertEqual(decoders.decode_krom(body.bytes(), cells).status, "incomplete")

    def test_krom_cell_split_across_rows_is_not_found(self):
        width = 320
        fb = bytearray(width * 16 * 4)
        for row, pixels in enumerate(PATTERNS["PASS"]):
            start = ((row + 1) * width - 8) * 4  # starts 8 pixels before the right edge
            fb[start:start + len(pixels)] = pixels
        self.assertEqual(decoders.find_krom_cells(bytes(fb), width), [])

    def test_dillon_reads_r30(self):
        for value, status in ((-1, "pass"), (0, "incomplete"), (7, "fail")):
            body = Body()
            body.gpr(30, value)
            body.gpr(29, -1)
            self.assertEqual(decoders.decode_dillon(body.bytes()).status, status, value)

    def systemtest_body(self, *texts: str) -> bytes:
        body = Body()
        address = 0x200000
        for text in texts:
            raw = text.encode() + b"\x00" * (-len(text) % 4 or 4)
            body.rdram(address, raw)
            address += len(raw) + 64
        return body.bytes()

    def test_systemtest_summary_and_failed_names(self):
        passed = self.systemtest_body(
            "n64-systemtest 3.0.0 (base=1 timing=0 cycle=0 cp0-hazards=0)\n"
            "Finished in 12.40s. Base: Failed 0 of 1125 tests (100% success rate)\n")
        verdict = decoders.decode_systemtest(passed)
        self.assertEqual((verdict.status, verdict.observed["total"]), ("pass", 1125))

        failed = self.systemtest_body(
            "Test 'TLB: Read after 4k page' failed: expected TLBL\n",
            "Test 'Syscall'(1) failed with exception: Reserved\n",
            "Finished in 12.40s. Base: Failed 2 of 1125 tests (99% success rate)\n")
        verdict = decoders.decode_systemtest(failed)
        self.assertEqual(verdict.status, "fail")
        self.assertEqual(verdict.observed["failed_tests"], ["Syscall(1)", "TLB: Read after 4k page"])

        self.assertEqual(decoders.decode_systemtest(Body().bytes()).status, "incomplete")

    def test_rejects_foreign_body(self):
        with self.assertRaises(decoders.DecodeError):
            decoders.decode_dillon(b"RASTATE\x01" + bytes(64))


class ManifestContract(unittest.TestCase):
    def test_manifest_is_consistent(self):
        manifest = json.loads(MANIFEST.read_text())
        self.assertEqual(manifest["schema"], "n64-conformance-roms/1")
        suites = manifest["suites"]
        ids = [t["id"] for t in manifest["tests"]]
        self.assertEqual(len(ids), len(set(ids)), "duplicate test ids")
        for test in manifest["tests"]:
            self.assertRegex(test["id"], r"^[A-Za-z0-9._-]+$")
            self.assertRegex(test["sha256"], r"^[0-9a-f]{64}$")
            self.assertIn(test["suite"], suites)
            self.assertFalse(Path(test["path"]).is_absolute() or ".." in Path(test["path"]).parts)
            if suites[test["suite"]]["decoder"] == "krom":
                cells = test["reference_cells"]
                self.assertTrue(cells, test["id"])
                self.assertTrue(all(c[2] in ("PASS", "FAIL") for c in cells), test["id"])
        for name, suite in suites.items():
            self.assertIn(suite["decoder"], ("krom", "dillon", "systemtest"), name)
            self.assertIn(suite["fetch"]["kind"], ("github-raw", "zip", "build"), name)
            for key in ("source", "version", "license", "redistribution"):
                self.assertTrue(suite.get(key), f"{name}.{key}")
        engines = {"dynamic_recompiler", "cached_interpreter", "pure_interpreter", "*"}
        for entry in manifest["known_failures"]:
            self.assertIn(entry["test"], ids)
            self.assertTrue(set(entry["cpucores"]) <= engines, entry)
            self.assertTrue(entry["reason"], entry)


FAKE_ADAPTER = r'''#!/usr/bin/env python3
import json, os, shutil, sys
from pathlib import Path

plan = json.loads(os.environ.get("FAKE_ADAPTER_PLAN", "{}"))
sub, rest, opts = sys.argv[1], sys.argv[2:], {}
while rest:
    if rest[0].startswith("--") and len(rest) > 1 and not rest[1].startswith("--"):
        opts[rest[0]] = rest[1]
        del rest[:2]
    else:
        del rest[:1]
if sub == "doctor":
    raise SystemExit(0)
bundle = Path(opts["--bundle-dir"])
state = bundle / "fake.json"
if sub == "start":
    stem = Path(opts["--rom"]).stem
    engines = {"dynamic_recompiler": "Dynamic Recompiler", "cached_interpreter": "Cached Interpreter",
               "pure_interpreter": "Pure Interpreter"}
    engine = plan.get("engine", {}).get(stem, engines[os.environ["PARALLEL_N64_CPUCORE_OVERRIDE"]])
    (bundle / "logs").mkdir(parents=True, exist_ok=True)
    (bundle / "logs/retroarch.log").write_text(f"[libretro INFO] mupen64plus: Starting R4300 emulator: {engine}\n")
    state.write_text(json.dumps({"stem": stem, "frame": 0}))
    raise SystemExit(0)
st = json.loads(state.read_text())
if sub == "status":
    print(f"GET_STATUS PAUSED n64,{st['stem']},crc32=1,frame={st['frame']}")
elif sub == "send":
    verb, *args = opts["--command"].split()
    if verb == "STEP_FRAME":
        st["frame"] += int(args[0])
    print(opts["--command"])
elif sub == "save-slot":
    target = bundle / "states/ParaLLEish" / f"{st['stem']}.state{opts['--slot']}"
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(plan["bodies"][st["stem"]], target)
elif sub == "stop":
    print("[interactive] session exited cleanly.")
state.write_text(json.dumps(st))
'''


class RunnerContract(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.adapter = self.root / "fake_adapter.py"
        self.adapter.write_text(FAKE_ADAPTER)
        self.adapter.chmod(0o755)
        for name in ("core.so", "retroarch", "base.cfg"):
            (self.root / name).write_bytes(name.encode())
        self.roms = self.root / "roms"
        (self.roms / "dillon").mkdir(parents=True)
        tests, bodies = [], {}
        for stem, r30 in (("good", -1), ("bad", 3), ("known", 2), ("wrongengine", -1)):
            rom = stem.encode() * 8
            (self.roms / "dillon" / f"{stem}.z64").write_bytes(rom)
            tests.append({"id": f"dillon.{stem}", "suite": "dillon", "path": f"{stem}.z64",
                          "sha256": hashlib.sha256(rom).hexdigest()})
            body = Body()
            body.gpr(30, r30)
            bodies[stem] = str(self.root / f"{stem}.body")
            Path(bodies[stem]).write_bytes(body.bytes())
        tests.append({"id": "dillon.unstaged", "suite": "dillon", "path": "unstaged.z64", "sha256": "0" * 64})
        self.plan = {"bodies": bodies, "engine": {"wrongengine": "Pure Interpreter"}}
        self.manifest = self.root / "manifest.json"
        self.manifest.write_text(json.dumps({
            "schema": "n64-conformance-roms/1",
            "suites": {"dillon": {"title": "Dillon", "decoder": "dillon", "frames": 30,
                                  "fetch": {"kind": "zip"}}},
            "tests": tests,
            "known_failures": [{"test": "dillon.known", "cpucores": ["dynamic_recompiler"], "reason": "listed"}],
        }))

    def tearDown(self):
        self.tmp.cleanup()

    def run_runner(self, out, *select):
        argv = [sys.executable, str(RUNNER), "--manifest", str(self.manifest), "--roms", str(self.roms),
                "--core", str(self.root / "core.so"), "--retroarch-bin", str(self.root / "retroarch"),
                "--base-config", str(self.root / "base.cfg"), "--runtime-class", "fake",
                "--out", str(self.root / out), "--adapter", str(self.adapter), "--orphan-grace", "0.2",
                "--no-capture"]
        for selector in select:
            argv += ["--select", selector]
        env = {**os.environ, "FAKE_ADAPTER_PLAN": json.dumps(self.plan)}
        env.pop("PARALLEL_N64_CPUCORE_OVERRIDE", None)
        result = subprocess.run(argv, capture_output=True, text=True, env=env, timeout=120)
        path = self.root / out / "results.jsonl"
        records = [json.loads(l) for l in path.read_text().splitlines()] if path.is_file() else []
        return result, {r["test"]: r for r in records}

    def test_outcomes_and_skips(self):
        result, records = self.run_runner("all")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        outcomes = {test: r["outcome"] for test, r in records.items()}
        self.assertEqual(outcomes, {"dillon.good": "pass", "dillon.bad": "fail", "dillon.known": "known-fail",
                                    "dillon.wrongengine": "fail"})
        self.assertIn("engine 'Pure Interpreter' started", records["dillon.wrongengine"]["detail"])
        # Passing and known-failing bundles are removed; failing ones stay for triage.
        self.assertIsNone(records["dillon.good"]["bundle"])
        self.assertTrue((self.root / "all" / records["dillon.bad"]["bundle"]).is_dir())
        report = (self.root / "all" / "report.md").read_text()
        self.assertIn("1 ROM(s) not staged (skipped): dillon.unstaged", report)
        self.assertNotIn("dillon.unstaged", records)

    def test_known_failure_that_passes_is_reported_fixed(self):
        self.plan["bodies"]["known"] = self.plan["bodies"]["good"]
        result, records = self.run_runner("fixed", "dillon.known")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(records["dillon.known"]["outcome"], "fixed")

    def test_partially_staged_suite_fails(self):
        result, records = self.run_runner("partial", "dillon.[gu]*")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertEqual(records["dillon.good"]["outcome"], "pass")
        self.assertIn("partially staged suite(s): dillon", result.stderr)

    def test_nothing_staged_skips(self):
        result, records = self.run_runner("unstaged", "dillon.unstaged")
        self.assertEqual(result.returncode, 77, result.stdout + result.stderr)
        self.assertEqual(records, {})
        self.assertRegex(result.stdout, re.compile(r"not staged \(skipped\): dillon.unstaged"))


if __name__ == "__main__":
    unittest.main()
