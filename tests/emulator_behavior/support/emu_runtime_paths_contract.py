"""Check fixture relocation and caller overrides without launching an emulator."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[3]


class RuntimePaths(unittest.TestCase):
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
