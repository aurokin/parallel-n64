"""Check bounded HTS reads and lossless, opt-in per-asset PHRB compression."""
import copy
import hashlib
import json
import os
import random
import resource
import stat
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / 'tools'))
from hires_pack_emit_binary_package import emit_binary_package_from_manifest
from hires_pack_inspect_binary_package import inspect_binary_package

TXCACHE_FORMAT_VERSION = 0x08000000
GL_TEXFMT_GZ = 0x80000000
GL_RGBA8 = 0x8058
GL_RGBA = 0x1908
GL_UNSIGNED_BYTE = 0x1401
# Larger than the address-space limit below, so reading the cache into memory fails.
HOLE_BYTES = 5 << 30
ADDRESS_SPACE_LIMIT = 1 << 30


def write_sparse_hts(path: Path, textures, compressed=True):
    """Write a .hts whose records sit before and after a sparse multi-gigabyte hole."""
    index = []
    with path.open('wb') as fp:
        fp.write(struct.pack('<iiq', TXCACHE_FORMAT_VERSION, 0, 0))
        for position, (checksum64, rgba) in enumerate(textures):
            if position:
                fp.seek(HOLE_BYTES, 1)
            blob = zlib.compress(rgba) if compressed else rgba
            index.append((checksum64, fp.tell() | (0x0200 << 48)))
            fp.write(struct.pack('<IIIHHBHI', 2, 2, GL_RGBA8 | (GL_TEXFMT_GZ if compressed else 0),
                                 GL_RGBA, GL_UNSIGNED_BYTE, 1, 0x0200, len(blob)))
            fp.write(blob)
        storage_pos = fp.tell()
        fp.write(struct.pack('<i', len(index)))
        for checksum64, packed in index:
            fp.write(struct.pack('<Qq', checksum64, packed))
        fp.seek(8)
        fp.write(struct.pack('<q', storage_pos))


def limit_address_space():
    resource.setrlimit(resource.RLIMIT_AS, (ADDRESS_SPACE_LIMIT, ADDRESS_SPACE_LIMIT))


@unittest.skipUnless(sys.platform.startswith('linux'), 'RLIMIT_AS is only enforced on Linux')
class StreamedHtsConversion(unittest.TestCase):
    def test_writer_streams_more_raw_bytes_than_its_memory_budget(self):
        # Ten 64 MiB assets exceed this child's 256 MiB address-space cap.
        # The loader generates one asset at a time; only small zlib output remains.
        command = '''
import resource
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
from emu_hts2phrb_bounded_memory_contract import mixed_manifest
from hires_pack_emit_binary_package import emit_binary_package_from_manifest
resource.setrlimit(resource.RLIMIT_AS, (256 << 20, 256 << 20))
manifest, _ = mixed_manifest()
record = manifest['records'][0]
candidate = dict(record['asset_candidates'][0], width=4096, height=4096, data_size=64 << 20)
record['asset_candidates'] = [dict(candidate, replacement_id=f'large-{i}') for i in range(10)]
record['asset_candidate_count'] = 10
manifest['records'] = [record]
output = Path(sys.argv[2])
result = emit_binary_package_from_manifest(manifest, output,
    asset_blob_loader=lambda *_: bytes(64 << 20), asset_storage_mode='legacy', compress_raw_rgba='1')
assert result['raw_rgba_compression']['source_blob_bytes'] == 640 << 20
assert result['raw_rgba_compression']['compressed_assets'] == 10
assert output.stat().st_size < 4 << 20
'''
        with tempfile.TemporaryDirectory() as tmp:
            result = subprocess.run([sys.executable, '-B', '-c', command, str(Path(__file__).parent),
                                     str(Path(tmp) / 'large.phrb')], capture_output=True, text=True, timeout=60,
                                    check=False)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_cache_larger_than_memory_limit_converts(self):
        textures = [(0x11111111, bytes((10, 20, 30, 255)) * 4), (0x22222222, bytes((40, 50, 60, 128)) * 4)]
        with tempfile.TemporaryDirectory() as tmp:
            cache = Path(tmp) / 'SPARSE_HIRESTEXTURES.hts'
            write_sparse_hts(cache, textures)
            out = Path(tmp) / 'out'
            result = subprocess.run(
                [sys.executable, str(REPO / 'tools/hts2phrb.py'), '--cache', str(cache), '--output-dir', str(out),
                 '--stdout-format', 'json'],
                capture_output=True, text=True, preexec_fn=limit_address_space, check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            inspected = subprocess.run(
                [sys.executable, str(REPO / 'tools/hires_pack_inspect_binary_package.py'), '--package', str(out / 'package.phrb')],
                capture_output=True, text=True, check=True,
            )
            package = json.loads(inspected.stdout)
            data = (out / 'package.phrb').read_bytes()
        blob_section = data[len(data) - package['blob_bytes']:]
        blobs = {
            int(candidate['legacy_checksum64'], 16): zlib.decompress(
                blob_section[candidate['rgba_blob_offset']:candidate['rgba_blob_offset'] + candidate['rgba_blob_size']]
            )
            for record in package['records']
            for candidate in record['asset_candidates']
        }
        self.assertEqual(blobs, dict(textures))

    def test_raw_sparse_cache_cli_and_reuse_follow_compression_setting(self):
        textures = [(0x11111111, bytes((10, 20, 30, 255)) * 4), (0x22222222, bytes((40, 50, 60, 128)) * 4)]
        with tempfile.TemporaryDirectory() as tmp:
            cache = Path(tmp) / 'RAW_HIRESTEXTURES.hts'
            write_sparse_hts(cache, textures, compressed=False)
            self.assertGreater(cache.stat().st_size, 1 << 32)
            self.assertLess(cache.stat().st_blocks * 512, 1 << 20)
            out = Path(tmp) / 'out'
            original = None
            previous_setting = 'initial'
            for setting in (None, 'off', '1', '6', 'off'):
                command = [sys.executable, str(REPO / 'tools/hts2phrb.py'), '--cache', str(cache),
                           '--output-dir', str(out), '--stdout-format', 'json', '--reuse-existing']
                if setting is not None:
                    command += ['--compress-raw-rgba', setting]
                result = subprocess.run(command, capture_output=True, text=True, preexec_fn=limit_address_space,
                                        check=False)
                self.assertEqual(result.returncode, 0, result.stderr)
                report = json.loads(result.stdout)
                self.assertEqual(report['reused_existing'], setting == 'off' and previous_setting is None)
                if setting is None:
                    for name in ('pre_request_signature', 'request_signature'):
                        self.assertNotIn('compress_raw_rgba', report[name])
                data = (out / 'package.phrb').read_bytes()
                if setting in (None, 'off'):
                    if original is None:
                        original = data
                    self.assertEqual(data, original)
                    self.assertNotIn('raw_rgba_compression', report['binary_package'])
                else:
                    self.assertFalse(report['reused_existing'])
                    self.assertEqual(report['binary_package']['raw_rgba_compression']['compressed_assets'], 2)
                previous_setting = setting
                package = inspect_binary_package(out / 'package.phrb')
                blobs = stored_blobs(data, package)
                self.assertEqual([zlib.decompress(blob) if asset['format'] & GL_TEXFMT_GZ else blob
                                  for asset, blob in blobs], [rgba for _, rgba in textures])


def mixed_manifest(jabo=False):
    """Include untouched storage controls, raw fallbacks and malformed RGBA extents."""
    rgba = bytes((10, 20, 30, 128)) * 1024
    # Fixed encoded input keeps the prior-emitter digests independent of zlib's encoder version.
    precompressed = bytes.fromhex('789cedc3b10d00000803a0bdb3b3b7f674ff309090d9465555557dfd001080f01f')
    cases = [
        ('solid', rgba, 32, 32, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE),
        ('noise', random.Random(42).randbytes(4096), 32, 32, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE),
        ('equal-size', bytes(11) + b'\x01', 3, 1, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE),
        ('precompressed', precompressed, 32, 32, GL_RGBA8 | GL_TEXFMT_GZ, GL_RGBA, GL_UNSIGNED_BYTE),
        ('packed', bytes((0x12, 0x34)) * 1024, 32, 32, 0x8056, GL_RGBA, 0x8033),
        ('indexed', bytes(range(256)) * 4, 32, 32, 0x80E5, 0x1900, GL_UNSIGNED_BYTE),
        ('rgb', bytes((10, 20, 30)) * 1024, 32, 32, 0x8051, 0x1907, GL_UNSIGNED_BYTE),
        ('zero', b'', 0, 32, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE),
        ('short', rgba[:-1], 32, 32, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE),
        ('overlong', rgba + b'x', 32, 32, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE),
        ('overflow', rgba, 0xFFFFFFFF, 0xFFFFFFFF, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE),
    ]
    candidates, blobs = [], {}
    for index, (name, blob, width, height, fmt, texture_format, pixel_type) in enumerate(cases):
        candidates.append({
            'replacement_id': name, 'legacy_source_path': 'cache.hts',
            'materialized_path': f'assets/{name}', 'variant_group_id': f'group-{index}',
            'legacy_checksum64': f'{index + 1:016x}', 'legacy_texture_crc': f'{index + 1:08x}',
            'legacy_palette_crc': '00000000', 'selector_checksum64': f'{index + 100:016x}',
            'legacy_formatsize': 0x200, 'width': width, 'height': height,
            'format': fmt, 'texture_format': texture_format, 'pixel_type': pixel_type,
            'data_size': len(blob),
        })
        blobs[f'assets/{name}'] = blob
    record = {
        'policy_key': 'policy', 'sampled_object_id': 'family', 'record_flags': 4,
        'canonical_identity': {'fmt': 0, 'siz': 2, 'off': 0, 'stride': 32, 'wh': '32x32',
                               'formatsize': 0x200, 'sampled_low32': '12345678'},
        'asset_candidate_count': len(candidates), 'asset_candidates': candidates,
    }
    if jabo:
        record['jabo_md5'] = '0123456789abcdef0123456789abcdef'
    deferred = copy.deepcopy(record)
    deferred.update(policy_key='deferred', sampled_object_id='deferred-family', runtime_ready=False,
                    asset_candidate_count=1, asset_candidates=[dict(candidates[0], replacement_id='deferred')])
    final = copy.deepcopy(record)
    final.update(policy_key='last', sampled_object_id='last-family', asset_candidate_count=1,
                 asset_candidates=[dict(candidates[0], replacement_id='last')])
    return {'records': [record, deferred, final]}, blobs


def stored_blobs(data, package):
    start = struct.unpack_from('<4sIIIIIII', data)[-1]
    return [(candidate, data[start + candidate['rgba_blob_offset']:
                             start + candidate['rgba_blob_offset'] + candidate['rgba_blob_size']])
            for record in package['records'] for candidate in record['asset_candidates']]


class OptionalRawCompression(unittest.TestCase):
    @unittest.skipUnless(os.name == 'posix', 'POSIX permissions and umask')
    def test_output_uses_normal_creation_permissions_and_retains_existing_mode(self):
        manifest, blobs = mixed_manifest()
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'out.phrb'
            previous_umask = os.umask(0o027)
            try:
                emit_binary_package_from_manifest(manifest, output, blobs, asset_storage_mode='legacy')
                self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o640)
                output.chmod(0o604)
                staged_modes = set()

                def record_staged_mode(_index, _record, candidate):
                    staged_modes.update(stat.S_IMODE(path.stat().st_mode) for path in Path(tmp).glob('.phrb-*.tmp'))
                    return blobs[candidate['materialized_path']]

                emit_binary_package_from_manifest(manifest, output, asset_blob_loader=record_staged_mode,
                                                  asset_storage_mode='legacy')
                self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o604)
                # Package contents stay private until the published mode is applied.
                self.assertEqual(staged_modes, {0o600})
            finally:
                os.umask(previous_umask)

    @unittest.skipUnless(os.name == 'posix', 'POSIX symlinks')
    def test_output_symlink_updates_target_and_preserves_link(self):
        manifest, blobs = mixed_manifest()
        with tempfile.TemporaryDirectory() as tmp:
            target_dir = Path(tmp) / 'target'
            target_dir.mkdir()
            target = target_dir / 'out.phrb'
            target.write_bytes(b'previous package')
            output = Path(tmp) / 'link.phrb'
            output.symlink_to(Path('target') / target.name)
            for setting in ('off', '1', '6'):
                expected = Path(tmp) / 'expected.phrb'
                emit_binary_package_from_manifest(manifest, expected, blobs,
                    asset_storage_mode='legacy', compress_raw_rgba=setting)
                emit_binary_package_from_manifest(manifest, output, blobs,
                    asset_storage_mode='legacy', compress_raw_rgba=setting)
                self.assertTrue(output.is_symlink())
                self.assertEqual(target.read_bytes(), expected.read_bytes())
                self.assertEqual(list(target_dir.iterdir()), [target])

    def test_mixed_assets_preserve_content_metadata_order_and_source_manifest(self):
        for jabo in (False, True):
            for level in ('1', '6'):
                with self.subTest(jabo=jabo, level=level), tempfile.TemporaryDirectory() as tmp:
                    manifest, blobs = mixed_manifest(jabo)
                    original = copy.deepcopy(manifest)
                    baseline, output = Path(tmp) / 'baseline.phrb', Path(tmp) / 'output.phrb'
                    emit_binary_package_from_manifest(manifest, baseline, blobs, asset_storage_mode='legacy')
                    calls = []

                    def load(index, _record, candidate, loaded=calls, source_blobs=blobs):
                        loaded.append(index)
                        return source_blobs[candidate['materialized_path']]

                    result = emit_binary_package_from_manifest(manifest, output, asset_blob_loader=load,
                                                              asset_storage_mode='legacy', compress_raw_rgba=level)
                    self.assertEqual(manifest, original)
                    self.assertEqual(calls, [0] * 11 + [2])
                    before, after = inspect_binary_package(baseline), inspect_binary_package(output)
                    self.assertEqual(after['version'], 8 if jabo else 7)
                    self.assertEqual(before['record_count'], after['record_count'])
                    self.assertEqual(before['asset_count'], after['asset_count'])
                    for old, new in zip(before['records'], after['records']):
                        self.assertEqual({k: v for k, v in old.items() if k != 'asset_candidates'},
                                         {k: v for k, v in new.items() if k != 'asset_candidates'})
                    offset = 0
                    for (old, raw), (new, encoded) in zip(stored_blobs(baseline.read_bytes(), before),
                                                        stored_blobs(output.read_bytes(), after)):
                        storage = {'format', 'rgba_blob_offset', 'rgba_blob_size'}
                        self.assertEqual({k: v for k, v in old.items() if k not in storage},
                                         {k: v for k, v in new.items() if k not in storage})
                        self.assertEqual(new['rgba_blob_offset'], offset)
                        offset += len(encoded)
                        if new['replacement_id'] in ('solid', 'last'):
                            self.assertEqual(new['format'], old['format'] | GL_TEXFMT_GZ)
                            self.assertLess(len(encoded), len(raw))
                            self.assertEqual(zlib.decompress(encoded), raw)
                        else:
                            self.assertEqual(new['format'], old['format'])
                            self.assertEqual(encoded, raw)
                        if old['format'] & GL_TEXFMT_GZ:
                            self.assertEqual(zlib.decompress(encoded), zlib.decompress(raw))
                    self.assertEqual(result['blob_bytes'], offset)
                    self.assertEqual(result['raw_rgba_compression'], {
                        'level': int(level), 'eligible_assets': 4, 'compressed_assets': 2, 'raw_fallback_assets': 2,
                        'source_blob_bytes': before['blob_bytes'], 'saved_bytes': before['blob_bytes'] - offset,
                    })

    def test_default_off_matches_previous_emitter_bytes(self):
        # Fixed manifests emitted by the pre-pilot 89b1936e emitter, for both layouts.
        expected = {False: '763d7760dc355fc02146a8d44864a156b234d0ff3d4a043110319d29ac853005', True: '63550a516d2c5ce2ba77f777b1d1ab9b2508f77c10a6a0f6d3571271b4e36d8a'}
        for jabo in (False, True):
            with self.subTest(jabo=jabo), tempfile.TemporaryDirectory() as tmp:
                manifest, blobs = mixed_manifest(jabo)
                output = Path(tmp) / 'out.phrb'
                emit_binary_package_from_manifest(manifest, output, blobs, asset_storage_mode='legacy')
                original = output.read_bytes()
                self.assertEqual(hashlib.sha256(original).hexdigest(), expected[jabo])
                emit_binary_package_from_manifest(manifest, output, blobs, asset_storage_mode='legacy',
                                                  compress_raw_rgba='off')
                self.assertEqual(output.read_bytes(), original)

    def test_abort_preserves_previous_output_and_cleans_temporary_file(self):
        for setting in ('off', '1', '6'):
            for existing in (False, True):
                with self.subTest(setting=setting, existing=existing), tempfile.TemporaryDirectory() as tmp:
                    manifest, blobs = mixed_manifest()
                    output = Path(tmp) / 'out.phrb'
                    if existing:
                        output.write_bytes(b'previous package')
                        output.chmod(0o640)
                        previous_mode = stat.S_IMODE(output.stat().st_mode)
                    calls = 0

                    def fail_after_one(_index, _record, candidate, source_blobs=blobs):
                        nonlocal calls
                        calls += 1
                        if calls == 2:
                            raise OSError('input read failed')
                        return source_blobs[candidate['materialized_path']]

                    with self.assertRaisesRegex(OSError, 'input read failed'):
                        emit_binary_package_from_manifest(manifest, output, asset_blob_loader=fail_after_one,
                                                          asset_storage_mode='legacy', compress_raw_rgba=setting)
                    self.assertEqual(list(Path(tmp).iterdir()), [output] if existing else [])
                    if existing:
                        self.assertEqual(output.read_bytes(), b'previous package')
                        self.assertEqual(stat.S_IMODE(output.stat().st_mode), previous_mode)


if __name__ == '__main__':
    unittest.main()
