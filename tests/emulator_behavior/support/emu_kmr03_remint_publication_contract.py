"""Exercise authority publication through the remint CLI without an emulator."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[3]
REMINT = REPO / 'tools/scenarios/remint-paper-mario-kmr-03-entry-5-authority.sh'
CANDIDATE = b'new candidate from the bootstrap save'
EXISTING = b'existing authority must survive failed verification'
DIGEST = '35213195f6ffca55458f76821198390880f135f4331efbca2cb139885b3ad879'


class RemintPublicationContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='kmr03-remint-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = self.root / 'mock owner'
        scenarios = self.repo / 'tools/scenarios'
        adapters = self.repo / 'tools/adapters'
        (scenarios / 'lib').mkdir(parents=True)
        adapters.mkdir(parents=True)
        self.script = scenarios / REMINT.name
        shutil.copyfile(REMINT, self.script)
        self.bundle = self.root / 'evidence bundle'
        self.output = self.root / 'authority output/state'
        self.output.parent.mkdir()
        self.output.write_bytes(EXISTING)
        self.output.chmod(0o640)
        bootstrap = self.root / 'bootstrap input.state'
        bootstrap.write_bytes(b'original bootstrap input')
        self.runtime = self.root / 'runtime.env'
        self.runtime.write_text(
            f'BOOTSTRAP_STATE_PATH={shlex.quote(str(bootstrap))}\n'
            'RETROARCH_BIN=unused\nRETROARCH_BASE_CONFIG=unused\nCORE_PATH=unused\n'
            'ROM_PATH=unused\nSTARTUP_WAIT=0\nPOST_LOAD_SETTLE_FRAMES=3\n'
        )
        # Mock only transport/verifier/hash boundaries. The actual remint CLI
        # controls candidate staging, failure propagation and publication.
        (scenarios / 'lib/common.sh').write_text('''
scenario_source_runtime_env() { set -a; source "$1"; set +a; }
scenario_sha256_file() {
  python3 - "$1" <<'PY'
import hashlib
from pathlib import Path
import sys

data = Path(sys.argv[1]).read_bytes()
print("''' + DIGEST + '''" if data == b'mock capture satisfying the pinned digest'
      else hashlib.sha256(data).hexdigest())
PY
}
''')
        adapter = adapters / 'retroarch_stdin_session.sh'
        adapter.write_text('''#!/usr/bin/env python3
from pathlib import Path
import sys

args = sys.argv[1:]
bundle = Path(args[args.index('--bundle-dir') + 1])
state = bundle / 'states/ParaLLEl N64/Paper Mario (USA).state'
state.write_bytes(b'new candidate from the bootstrap save')
''')
        adapter.chmod(0o755)
        verifier = scenarios / 'paper-mario-kmr-03-entry-5.sh'
        verifier.write_text('''#!/usr/bin/env python3
import hashlib
import json
import os
from pathlib import Path
import sys

args = sys.argv[1:]
bundle = Path(args[args.index('--bundle-dir') + 1])
candidate = Path(os.environ['AUTHORITATIVE_STATE_PATH'])
output = Path(os.environ['TEST_OUTPUT_PATH'])
assert candidate != output, 'verification must load the staged candidate'
assert candidate.read_bytes() == b'new candidate from the bootstrap save'
assert output.read_bytes() == b'existing authority must survive failed verification'
assert '--authority-mode' in args and args[args.index('--authority-mode') + 1] == 'authoritative'
bundle.mkdir(parents=True)
(bundle / 'candidate.json').write_text(json.dumps({'path': str(candidate)}))
case = os.environ['TEST_CASE']
(bundle / 'bundle.json').write_text(json.dumps({'fixture_authority': {
    'authority_mode_used': 'bootstrap' if case == 'wrong-authority-mode' else 'authoritative',
    'active_state_sha256': hashlib.sha256(
        b'different state selected by runtime profile' if case == 'wrong-active-state' else candidate.read_bytes()
    ).hexdigest(),
}}))
captures = bundle / 'captures'
captures.mkdir()
(captures / 'capture.png').write_bytes(
    b'wrong captured scene' if case == 'digest-failure' else b'mock capture satisfying the pinned digest'
)
if case == 'verification-failure':
    raise SystemExit(42)
if case == 'candidate-changed':
    candidate.chmod(0o644)
    candidate.write_bytes(b'unverified replacement candidate')
''')
        verifier.chmod(0o755)

    def run_remint(self, case):
        environment = dict(os.environ, RUNTIME_ENV_OVERRIDE=str(self.runtime),
                           TEST_OUTPUT_PATH=str(self.output), TEST_CASE=case)
        return subprocess.run(
            ['bash', str(self.script), '--output-path', str(self.output),
             '--bundle-root', str(self.bundle)],
            env=environment, capture_output=True, text=True, check=False,
        )

    def test_verification_failure_preserves_existing_authority(self):
        result = self.run_remint('verification-failure')
        self.assertEqual(result.returncode, 42, result.stderr)
        self.assertEqual(self.output.read_bytes(), EXISTING)
        self.assertEqual(self.output.stat().st_mode & 0o777, 0o640)

    def test_capture_digest_failure_preserves_existing_authority(self):
        result = self.run_remint('digest-failure')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('capture hash mismatch', result.stderr)
        self.assertEqual(self.output.read_bytes(), EXISTING)

    def test_successful_verifier_using_other_bytes_cannot_publish(self):
        result = self.run_remint('wrong-active-state')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('did not use the staged authoritative candidate', result.stderr)
        self.assertEqual(self.output.read_bytes(), EXISTING)

    def test_bootstrap_verification_cannot_publish_as_authority(self):
        result = self.run_remint('wrong-authority-mode')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('did not use the staged authoritative candidate', result.stderr)
        self.assertEqual(self.output.read_bytes(), EXISTING)

    def test_success_publishes_the_verified_candidate_atomically(self):
        old_inode = self.output.stat().st_ino
        result = self.run_remint('success')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.output.read_bytes(), CANDIDATE)
        self.assertNotEqual(self.output.stat().st_ino, old_inode)
        self.assertEqual(self.output.stat().st_mode & 0o777, 0o640)
        receipt = json.loads((self.bundle / 'verify/candidate.json').read_text())
        staged = Path(receipt['path'])
        self.assertEqual(staged.read_bytes(), CANDIDATE)
        self.assertEqual(staged.stat().st_mode & 0o222, 0)
        self.assertIn(hashlib.sha256(CANDIDATE).hexdigest(), result.stdout)
        self.assertEqual(list(self.output.parent.glob('.state.*')), [])

    def test_changed_candidate_cannot_publish_unverified_bytes(self):
        result = self.run_remint('candidate-changed')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('candidate changed after staging', result.stderr)
        self.assertEqual(self.output.read_bytes(), EXISTING)
        self.assertEqual(list(self.output.parent.glob('.state.*')), [])


if __name__ == '__main__':
    unittest.main()
