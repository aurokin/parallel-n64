"""Exercise the reuse wrapper's evidence policy without launching a frontend."""
import ast
import copy
from pathlib import Path
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
        functions = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in ('to_int', 'require_provider_owned_evidence')]
        namespace = {'expected_source_mode': 'phrb-only'}
        exec(compile(ast.Module(body=functions, type_ignores=[]), str(WRAPPER), 'exec'), namespace)
        cls.validate = staticmethod(namespace['require_provider_owned_evidence'])

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


if __name__ == '__main__':
    unittest.main()
