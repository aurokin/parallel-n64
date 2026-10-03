"""Exercise metadata and child cleanup through the batch adapter's real FIFO."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[3]
ADAPTER = REPO / 'tools/adapters/retroarch_stdin_session.sh'


class BatchLifecycleContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='batch adapter ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bundle = self.root / 'bundle'
        self.bundle.mkdir()
        self.metadata = self.bundle / 'bundle.json'
        self.metadata.write_text(json.dumps({'status': {'scenario_state': 'runtime_prepared', 'runtime_executed': False}, 'unrelated': {'value': 'retained'}}))
        self.binary = self.root / 'fake-frontend'
        self.binary.write_text('''#!/usr/bin/env python3
import os
from pathlib import Path
import sys
Path(__file__).with_suffix('.pid').write_text(str(os.getpid()))
for line in sys.stdin:
    if line.strip() == 'QUIT':
        break
''')
        self.binary.chmod(0o755)
        for name in ('rom', 'core', 'config'):
            (self.root / name).write_text('fixture')
        self.bin_dir = self.root / 'bin'
        self.bin_dir.mkdir()
        real_sed = shutil.which('sed')
        # Reject the GNU-only form even when this contract runs on Linux.
        sed = self.bin_dir / 'sed'
        sed.write_text(f'''#!/usr/bin/env python3
import os
import sys
if '-i' in sys.argv[1:]:
    raise SystemExit('GNU-only sed edit refused')
os.execv({real_sed!r}, [{real_sed!r}, *sys.argv[1:]])
''')
        sed.chmod(0o755)
        self.env = {**os.environ, 'PATH': str(self.bin_dir) + os.pathsep + os.environ['PATH'], 'TMPDIR': str(self.root), 'EXIT_WAIT': '1'}

    def run_adapter(self, *commands):
        args = ['bash', str(ADAPTER), '--bundle-dir', str(self.bundle), '--rom', str(self.root / 'rom'), '--core', str(self.root / 'core'), '--base-config', str(self.root / 'config'), '--retroarch-bin', str(self.binary), '--startup-wait', '0']
        for command in commands:
            args.extend(['--command', command])
        return subprocess.run(args, env=self.env, capture_output=True, text=True, timeout=15)

    def assert_child_gone(self):
        pid = int(self.binary.with_suffix('.pid').read_text())
        with self.assertRaises(ProcessLookupError):
            os.kill(pid, 0)

    def test_clean_exit_updates_metadata_portably(self):
        result = self.run_adapter('QUIT')
        self.assertEqual(result.returncode, 0, result.stderr)
        metadata = json.loads(self.metadata.read_text())
        self.assertEqual(metadata['status']['scenario_state'], 'runtime_completed')
        self.assertTrue(metadata['status']['runtime_executed'])
        self.assertEqual(metadata['unrelated'], {'value': 'retained'})
        self.assert_child_gone()

    def test_command_failure_reaps_owned_child(self):
        result = self.run_adapter('WAIT 0.1', 'UNSUPPORTED')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Unsupported', result.stderr)
        self.assert_child_gone()
        self.assertFalse((self.bundle / 'retroarch.stdin').exists())
        metadata = json.loads(self.metadata.read_text())
        self.assertEqual(metadata['status']['scenario_state'], 'runtime_failed')
        self.assertFalse(metadata['status']['runtime_executed'])

    def test_invalid_metadata_fails_before_launch(self):
        self.metadata.write_text('{invalid')
        result = self.run_adapter('QUIT')
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.binary.with_suffix('.pid').exists())


if __name__ == '__main__':
    unittest.main()
