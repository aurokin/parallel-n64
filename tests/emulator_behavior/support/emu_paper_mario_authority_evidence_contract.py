"""Exercise the reuse wrapper's evidence policy without launching a frontend."""
import ast
import copy
from pathlib import Path
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[3]
WRAPPER = REPO / 'tools/scenarios/paper-mario-phrb-authority-validation.sh'


class AuthorityEvidenceContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # The wrapper embeds its Python policy; load those actual functions so
        # this contract checks reuse validation independently of fixture checks.
        source = WRAPPER.read_text().split("<<'PY'\n", 1)[1].split('\nPY\n', 1)[0]
        tree = ast.parse(source)
        functions = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in (
            'to_int', 'require_provider_owned_evidence', 'require_named_scene_verification',
            'require_reuse_provenance',
        )]
        namespace = {'expected_source_mode': 'phrb-only', 'Path': Path}
        exec(compile(ast.Module(body=functions, type_ignores=[]), str(WRAPPER), 'exec'), namespace)
        cls.validate = staticmethod(namespace['require_provider_owned_evidence'])
        cls.validate_scene = staticmethod(namespace['require_named_scene_verification'])
        cls.validate_reuse = staticmethod(namespace['require_reuse_provenance'])

    def setUp(self):
        self.evidence = {
            'available': True,
            'cache_loaded': True,
            'summary': {
                'provider': 'on',
                'source_mode': 'phrb-only',
                'compat_entry_count': 66,
                'native_sampled_entry_count': 0,
                'compat_draw_hits': 4,
                'source_counts': {'phrb': 66},
            },
        }

    def test_compatibility_replacement_is_valid(self):
        self.assertEqual(self.validate(self.evidence, 'evidence.json'), [])

    def test_upload_hits_do_not_satisfy_draw_activity(self):
        self.evidence['summary']['hits'] = 99
        self.evidence['summary']['compat_draw_hits'] = 0
        failures = self.validate(self.evidence, 'evidence.json')
        self.assertTrue(any('draw-time replacement' in failure for failure in failures), failures)

    def test_malformed_or_missing_draw_counts_fail(self):
        for value in ('4', True, 4.5, -1, None):
            with self.subTest(value=value):
                evidence = copy.deepcopy(self.evidence)
                evidence['summary']['compat_draw_hits'] = value
                self.assertTrue(self.validate(evidence, 'evidence.json'))
        del self.evidence['summary']['compat_draw_hits']
        self.assertTrue(self.validate(self.evidence, 'evidence.json'))

    def test_disabled_provider_or_failed_load_fails(self):
        for field, value in [('cache_loaded', False), ('cache_load_failed', True), ('missing_cache_path', True), ('disabled_reason', 'unsupported')]:
            with self.subTest(field=field):
                evidence = copy.deepcopy(self.evidence)
                evidence[field] = value
                self.assertTrue(self.validate(evidence, 'evidence.json'))
        self.evidence['summary']['provider'] = 'off'
        self.assertTrue(self.validate(self.evidence, 'evidence.json'))

    def test_legacy_passed_verdict_cannot_qualify_named_scene(self):
        record = {'fixture_id': 'paper-mario-kmr-03-entry-2', 'passed': True,
                  'checks': {'scene_identity_match': True}}
        failures = self.validate_scene(record, record['fixture_id'])
        self.assertTrue(any('scene_decoder_qualified' in failure for failure in failures), failures)
        # The aggregation provenance path must apply the same policy even before
        # an early return for incomplete bundle metadata.
        with tempfile.TemporaryDirectory() as directory:
            failures = self.validate_reuse(Path(directory), record, record['fixture_id'], {})
        self.assertTrue(any('scene_decoder_qualified' in failure for failure in failures), failures)

    def test_corrected_named_scene_verdict_is_admissible(self):
        record = {'passed': True, 'checks': {'scene_decoder_qualified': True, 'scene_identity_match': True}}
        self.assertEqual(self.validate_scene(record, 'paper-mario-kmr-03-entry-2'), [])

    def test_named_scene_checks_require_literal_true(self):
        for check in ('scene_decoder_qualified', 'scene_identity_match'):
            for value in (False, None, 'true', '1', 1, 1.0, [], {}):
                with self.subTest(check=check, value=value):
                    record = {'passed': True, 'checks': {'scene_decoder_qualified': True, 'scene_identity_match': True}}
                    record['checks'][check] = value
                    failures = self.validate_scene(record, 'paper-mario-kmr-03-entry-2')
                    self.assertTrue(any(check in failure for failure in failures), failures)
            del record['checks'][check]
            self.assertTrue(self.validate_scene(record, 'paper-mario-kmr-03-entry-2'))

    def test_missing_or_malformed_check_records_reject_named_scene(self):
        for checks in (None, [], True, 'passed'):
            with self.subTest(checks=checks):
                self.assertTrue(self.validate_scene({'passed': True, 'checks': checks}, 'paper-mario-kmr-03-entry-2'))
        self.assertTrue(self.validate_scene({'passed': True}, 'paper-mario-kmr-03-entry-2'))

    def test_callback_fixtures_do_not_require_named_scene_verdicts(self):
        for fixture in ('paper-mario-title-screen', 'paper-mario-file-select'):
            with self.subTest(fixture=fixture):
                self.assertEqual(self.validate_scene({'passed': True}, fixture), [])
                self.assertEqual(self.validate_scene({'checks': {'scene_decoder_qualified': False}}, fixture), [])


if __name__ == '__main__':
    unittest.main()
