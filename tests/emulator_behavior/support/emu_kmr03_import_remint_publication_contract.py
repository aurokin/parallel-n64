"""Exercise kmr03 import staging and publication through the remint CLI without an emulator."""
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[3]
REMINT = REPO / 'tools/scenarios/remint-paper-mario-kmr-03-entry-2-authority.sh'
BODY = b'M64+SAVE' + struct.pack('>I', 0x00010000) + b'kmr_03 landing-control body'
EXISTING = b'existing authority must survive failed verification'
CAPTURE = b'mock feature-off capture'
WRONG_CAPTURE = b'wrong captured scene'


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def rastate(body):
    """RetroArch's uncompressed RASTATE container with one MEM block."""
    padding = b'\0' * (-len(body) % 8)
    return b'RASTATE\x01' + b'MEM ' + struct.pack('<I', len(body)) + body + padding + b'END \0\0\0\0'


class ImportRemintPublicationContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='kmr03-remint-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = self.root / 'mock owner'
        scenarios = self.repo / 'tools/scenarios'
        (scenarios / 'lib').mkdir(parents=True)
        # The helper, shared shell library and savestate unwrapping are real;
        # only the emulator-facing scenario below is mocked.
        self.script = scenarios / REMINT.name
        shutil.copyfile(REMINT, self.script)
        shutil.copyfile(REPO / 'tools/scenarios/lib/common.sh', scenarios / 'lib/common.sh')
        shutil.copyfile(REPO / 'tools/savestate_bridge.py', self.repo / 'tools/savestate_bridge.py')
        self.bundle = self.root / 'evidence bundle'
        self.output = self.root / 'authority output/state'
        self.output.parent.mkdir()
        self.output.write_bytes(EXISTING)
        self.output.chmod(0o640)
        self.source = self.root / 'recorded landing-control.state'
        self.source.write_bytes(BODY)
        self.fixture_env = scenarios / 'paper-mario-kmr-03-entry-2.runtime.env'
        self.write_fixture_env(sha256(CAPTURE))
        # A caller's runtime override tries to relax every pin; the helper must
        # keep using the tracked fixture profile for identities and scene.
        self.runtime = self.root / 'operator runtime.env'
        self.runtime.write_text(
            f'AUTHORITY_SOURCE_STATE_BODY_SHA256={sha256(b"other body")}\n'
            f'EXPECTED_SCREENSHOT_SHA256_OFF={sha256(WRONG_CAPTURE)}\n'
            'EXPECTED_SCENE_AREA_ID=\nEXPECTED_SCENE_MAP_NAME=\nEXPECTED_SCENE_ENTRY_ID=\n'
        )
        verifier = scenarios / 'paper-mario-kmr-03-entry-2.sh'
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
assert candidate.read_bytes() == Path(os.environ['TEST_SOURCE_PATH']).read_bytes()
assert output.read_bytes() == b'existing authority must survive failed verification'
assert args[args.index('--mode') + 1] == 'off'
assert args[args.index('--authority-mode') + 1] == 'authoritative'
assert os.environ['RUNTIME_ENV_OVERRIDE'] == os.environ['TEST_RUNTIME_ENV']
bundle.mkdir(parents=True)
(bundle / 'candidate.json').write_text(json.dumps({'path': str(candidate)}))
case = os.environ['TEST_CASE']
(bundle / 'bundle.json').write_text(json.dumps({'fixture_authority': {
    'authority_mode_used': 'bootstrap' if case == 'wrong-authority-mode' else 'authoritative',
    'active_state_sha256': hashlib.sha256(
        b'different state selected by runtime profile' if case == 'wrong-active-state' else candidate.read_bytes()
    ).hexdigest(),
}}))
# The retired authority decoded as area 6/map 5/entry 2; a relaxed profile
# would let that verdict pass without a scene expectation.
scene = (6, None, 2) if case == 'wrong-scene' else (0, 'kmr_03', 2)
(bundle / 'traces').mkdir()
(bundle / 'traces/fixture-verification.json').write_text(json.dumps({
    'passed': True,
    'checks': {'scene_decoder_qualified': True},
    'actual': dict(zip(('scene_area_id', 'scene_map_name', 'scene_entry_id'), scene)),
}))
captures = bundle / 'captures'
captures.mkdir()
(captures / 'capture.png').write_bytes(
    b'wrong captured scene' if case == 'digest-failure' else b'mock feature-off capture'
)
if case == 'verification-failure':
    raise SystemExit(42)
if case == 'candidate-changed':
    candidate.chmod(0o644)
    candidate.write_bytes(b'unverified replacement candidate')
''')
        verifier.chmod(0o755)

    def write_fixture_env(self, capture_sha256):
        self.fixture_env.write_text(
            f'AUTHORITY_SOURCE_STATE_BODY_SHA256={sha256(BODY)}\n'
            f'EXPECTED_SCREENSHOT_SHA256_OFF={shlex.quote(capture_sha256)}\n'
            'EXPECTED_SCENE_AREA_ID=0\nEXPECTED_SCENE_MAP_NAME=kmr_03\nEXPECTED_SCENE_ENTRY_ID=2\n'
        )

    def run_remint(self, case):
        environment = dict(os.environ, RUNTIME_ENV_OVERRIDE=str(self.runtime),
                           TEST_RUNTIME_ENV=str(self.runtime), TEST_SOURCE_PATH=str(self.source),
                           TEST_OUTPUT_PATH=str(self.output), TEST_CASE=case)
        return subprocess.run(
            ['bash', str(self.script), '--source-state', str(self.source),
             '--output-path', str(self.output), '--bundle-root', str(self.bundle)],
            env=environment, capture_output=True, text=True, check=False,
        )

    def assert_preserved(self):
        self.assertEqual(self.output.read_bytes(), EXISTING)
        self.assertEqual(self.output.stat().st_mode & 0o777, 0o640)
        self.assertEqual(list(self.output.parent.glob('.state.*')), [])

    def test_source_body_mismatch_is_rejected_before_verification(self):
        self.source.write_bytes(BODY + b' from another recording')
        result = self.run_remint('success')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('body sha256 mismatch', result.stderr)
        self.assertFalse((self.bundle / 'verify').exists())
        self.assert_preserved()

    def test_retroarch_container_is_identified_by_body(self):
        self.source.write_bytes(rastate(BODY))
        result = self.run_remint('success')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.output.read_bytes(), rastate(BODY))

    def test_pending_digest_reports_capture_without_publishing(self):
        self.write_fixture_env('')
        result = self.run_remint('success')
        self.assertEqual(result.returncode, 3, result.stderr)
        self.assertIn('digest is pending', result.stderr)
        self.assertIn(sha256(CAPTURE), result.stderr)
        self.assert_preserved()

    def test_verification_failure_preserves_existing_authority(self):
        result = self.run_remint('verification-failure')
        self.assertEqual(result.returncode, 42, result.stderr)
        self.assert_preserved()

    def test_capture_digest_failure_preserves_existing_authority(self):
        result = self.run_remint('digest-failure')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('capture hash mismatch', result.stderr)
        self.assert_preserved()

    def test_wrong_decoded_scene_cannot_publish(self):
        result = self.run_remint('wrong-scene')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('did not qualify scene', result.stderr)
        self.assert_preserved()

    def test_successful_verifier_using_other_bytes_cannot_publish(self):
        result = self.run_remint('wrong-active-state')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('did not use the staged authoritative candidate', result.stderr)
        self.assert_preserved()

    def test_bootstrap_verification_cannot_publish_as_authority(self):
        result = self.run_remint('wrong-authority-mode')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('did not use the staged authoritative candidate', result.stderr)
        self.assert_preserved()

    def test_success_publishes_the_verified_candidate_atomically(self):
        old_inode = self.output.stat().st_ino
        result = self.run_remint('success')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.output.read_bytes(), BODY)
        self.assertNotEqual(self.output.stat().st_ino, old_inode)
        self.assertEqual(self.output.stat().st_mode & 0o777, 0o640)
        staged = self.bundle / 'stage/authority-candidate.state'
        self.assertEqual(staged.read_bytes(), BODY)
        self.assertEqual(staged.stat().st_mode & 0o222, 0)
        self.assertIn(sha256(BODY), result.stdout)
        self.assertEqual(list(self.output.parent.glob('.state.*')), [])

    def test_changed_candidate_cannot_publish_unverified_bytes(self):
        result = self.run_remint('candidate-changed')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('candidate changed after staging', result.stderr)
        self.assert_preserved()


if __name__ == '__main__':
    unittest.main()
