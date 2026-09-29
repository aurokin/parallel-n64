"""Check fixture relocation and caller overrides without launching an emulator."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[3]


class RuntimePaths(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="title inputs ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.scenarios = self.root / "tools/scenarios"
        (self.scenarios / "lib").mkdir(parents=True)
        for name in ("paper-mario-title-screen.sh", "paper-mario-title-screen.runtime.env", "lib/common.sh"):
            shutil.copyfile(REPO / "tools/scenarios" / name, self.scenarios / name)
        self.bundle = self.root / "must-not-be-created"
        self.env = {"PATH": os.environ["PATH"]}
        for name in ("RETROARCH_BIN", "RETROARCH_BASE_CONFIG", "CORE_PATH", "ROM_PATH"):
            path = self.root / name
            path.write_text("#!/bin/sh\nexit 91\n" if name == "RETROARCH_BIN" else name)
            if name == "RETROARCH_BIN":
                path.chmod(0o755)
            self.env[name] = str(path)

    def check_title(self, *args, env=None):
        return subprocess.run(
            ["bash", str(self.scenarios / "paper-mario-title-screen.sh"),
             "--bundle-dir", str(self.bundle), *args],
            env=env or self.env, cwd="/", text=True, capture_output=True,
        )

    def test_title_check_uses_explicit_config_without_staged_pack(self):
        pack = self.root / "operator pack.phrb"
        pack.write_bytes(b"pinned synthetic pack")
        config = self.root / "operator runtime.env"
        fixture = self.scenarios / "paper-mario-title-screen.runtime.env"
        config.write_text(f"source {shlex.quote(str(fixture))}\nPARALLEL_RDP_HIRES_CACHE_PATH={shlex.quote(str(pack))}\n")
        result = self.check_title("--mode", "on", "--check-inputs", env={**self.env, "RUNTIME_ENV_OVERRIDE": str(config)})
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["inputs"]["pack"], {"path": str(pack), "sha256": hashlib.sha256(pack.read_bytes()).hexdigest()})
        self.assertEqual(report["inputs"]["frontend"]["path"], self.env["RETROARCH_BIN"])
        self.assertFalse(self.bundle.exists())

    def test_title_legacy_and_explicit_pack_inputs_match(self):
        pack = self.root / "artifacts/hts2phrb-review/local-pm64-exact-variant-set/package.phrb"
        pack.parent.mkdir(parents=True)
        pack.write_bytes(b"same pinned pack")
        legacy = self.check_title("--mode", "on", "--check-inputs")
        explicit = self.check_title("--mode", "on", "--check-inputs", env={**self.env, "PARALLEL_RDP_HIRES_CACHE_PATH": str(pack)})
        self.assertEqual(legacy.returncode, 0, legacy.stderr)
        self.assertEqual(explicit.returncode, 0, explicit.stderr)
        self.assertEqual(json.loads(legacy.stdout), json.loads(explicit.stdout))
        self.assertFalse(self.bundle.exists())

    def test_title_off_inputs_do_not_require_a_pack(self):
        result = self.check_title("--check-inputs")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIsNone(json.loads(result.stdout)["inputs"]["pack"])
        self.assertFalse(self.bundle.exists())

    def test_default_dry_run_defers_pack_validation(self):
        result = self.check_title('--mode', 'on', env={
            **self.env, 'PARALLEL_RDP_HIRES_CACHE_PATH': str(self.root / 'not-staged.zip')})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('dry-run complete', result.stdout)
        self.assertTrue((self.bundle / 'bundle.json').is_file())

    def test_title_bad_inputs_fail_before_bundle_staging(self):
        invalid_pack = self.root / "invalid.zip"
        invalid_pack.write_bytes(b"wrong pack format")
        non_executable = self.root / "frontend-not-executable"
        non_executable.write_text("not executable")
        cases = [
            ({"RUNTIME_ENV_OVERRIDE": str(self.root / "missing.env")}, [], "runtime configuration"),
            ({"RETROARCH_BIN": str(self.root / "missing-frontend")}, [], "RETROARCH_BIN"),
            ({"RETROARCH_BIN": str(non_executable)}, [], "must be executable"),
            ({"CORE_PATH": str(self.root)}, [], "CORE_PATH"),
            ({"RETROARCH_BASE_CONFIG": str(self.root / "missing.cfg")}, [], "RETROARCH_BASE_CONFIG"),
            ({"PARALLEL_RDP_HIRES_CACHE_PATH": str(self.root / "missing.phrb")}, ["--mode", "on"], "PARALLEL_RDP_HIRES_CACHE_PATH"),
            ({"PARALLEL_RDP_HIRES_CACHE_PATH": str(invalid_pack)}, ["--mode", "on"], ".phrb"),
        ]
        for overrides, args, diagnostic in cases:
            for action in ("--check-inputs", "--run"):
                with self.subTest(overrides=overrides, action=action):
                    result = self.check_title(*args, action, env={**self.env, **overrides})
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(diagnostic, result.stderr)
                    self.assertFalse(self.bundle.exists())

    def test_relocated_fixtures_and_explicit_inputs(self):
        with tempfile.TemporaryDirectory(prefix="renderer paths ") as temp:
            root = Path(temp)
            scenarios = root / "tools/scenarios"
            scenarios.mkdir(parents=True)
            for source in (REPO / "tools/scenarios").glob("*.runtime.env"):
                fixture = scenarios / source.name
                shutil.copyfile(source, fixture)
                command = ['bash', '-c', 'source "$1"; printf "%s\\n" "$CORE_PATH" "$ROM_PATH" "$RETROARCH_BIN" "$RETROARCH_BASE_CONFIG"', 'fixture', str(fixture)]
                env = {"PATH": os.environ["PATH"], "RETROARCH_BIN": "/test/frontend", "RETROARCH_BASE_CONFIG": "/test/base config"}
                result = subprocess.run(command, env=env, cwd="/", text=True, capture_output=True, check=True)
                self.assertEqual(result.stdout.splitlines(), [str(root / "parallel_n64_libretro.so"), str(root / "assets/Paper Mario (USA).zip"), "/test/frontend", "/test/base config"])
                env.update(CORE_PATH="/caller/core", ROM_PATH="/caller/rom")
                result = subprocess.run(command, env=env, cwd="/", text=True, capture_output=True, check=True)
                self.assertEqual(result.stdout.splitlines()[:2], ["/caller/core", "/caller/rom"])


if __name__ == "__main__":
    unittest.main()
