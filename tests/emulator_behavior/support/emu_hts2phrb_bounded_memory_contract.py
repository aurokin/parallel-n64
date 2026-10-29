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
from contextlib import nullcontext
from pathlib import Path
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / 'tools'))
from hires_pack_emit_binary_package import (
    ASSET_STRUCT,
    emit_binary_package_from_manifest,
)
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
            for setting in (None, 'off', '1', '6', 'off'):
                command = [sys.executable, str(REPO / 'tools/hts2phrb.py'), '--cache', str(cache),
                           '--output-dir', str(out), '--stdout-format', 'json', '--reuse-existing']
                if setting is not None:
                    command += ['--compress-raw-rgba', setting]
                result = subprocess.run(command, capture_output=True, text=True, preexec_fn=limit_address_space,
                                        check=False)
                self.assertEqual(result.returncode, 0, result.stderr)
                report = json.loads(result.stdout)
                data = (out / 'package.phrb').read_bytes()
                if setting in (None, 'off'):
                    if original is None:
                        original = data
                    self.assertEqual(data, original)
                    self.assertNotIn('raw_rgba_compression', report['binary_package'])
                else:
                    self.assertFalse(report['reused_existing'])
                    self.assertEqual(report['binary_package']['raw_rgba_compression']['compressed_assets'], 2)
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


@unittest.skipUnless(os.name == 'posix', 'POSIX file permission modes and umask')
class OutputPermissions(unittest.TestCase):
    def emit_with_umask(self, mask, existing_mode=None):
        # Isolate the process-wide umask from the test runner and other tests.
        command = '''
import json
import os
import stat
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
from emu_hts2phrb_bounded_memory_contract import mixed_manifest
from hires_pack_emit_binary_package import emit_binary_package_from_manifest
manifest, blobs = mixed_manifest()
directory = Path(sys.argv[2])
existing_mode = int(sys.argv[4]) if sys.argv[4] else None
previous_umask = os.umask(int(sys.argv[3]))
try:
    results = []
    for setting in (None, 'off', '1', '6'):
        output = directory / f'{setting}.phrb'
        if existing_mode is not None:
            output.write_bytes(b'previous package')
            output.chmod(existing_mode)
        options = {} if setting is None else {'compress_raw_rgba': setting}
        result = emit_binary_package_from_manifest(manifest, output, blobs,
            asset_storage_mode='legacy', **options)
        results.append({'mode': stat.S_IMODE(output.stat().st_mode),
            'compressed': result.get('raw_rgba_compression', {}).get('compressed_assets', 0)})
    assert len(list(directory.iterdir())) == 4, 'temporary output leaked'
    print(json.dumps(results))
finally:
    os.umask(previous_umask)
'''
        with tempfile.TemporaryDirectory(prefix='phrb permissions ') as tmp:
            result = subprocess.run([sys.executable, '-B', '-c', command, str(Path(__file__).resolve().parent),
                                     '.', str(mask), '' if existing_mode is None else str(existing_mode)], cwd=tmp,
                                    capture_output=True, text=True, timeout=30, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            return json.loads(result.stdout)

    def test_new_output_uses_ordinary_creation_mode_under_umask(self):
        for mask in (0o000, 0o022, 0o027, 0o077, 0o207, 0o222, 0o777):
            with self.subTest(umask=oct(mask)):
                results = self.emit_with_umask(mask)
                self.assertEqual([result['mode'] for result in results], [0o666 & ~mask] * 4)
                self.assertEqual([result['compressed'] for result in results], [0, 0, 2, 2])

    def test_existing_output_preserves_mode_despite_restrictive_umask(self):
        for mode in (0o640, 0o604, 0o440):
            with self.subTest(mode=oct(mode)):
                results = self.emit_with_umask(0o077, mode)
                self.assertEqual([result['mode'] for result in results], [mode] * 4)
                self.assertEqual([result['compressed'] for result in results], [0, 0, 2, 2])

    def test_setgid_parent_preserves_group_at_creation_and_publication(self):
        for clear_setgid in (False, True):
            with self.subTest(simulated_nonmember_kernel=clear_setgid):
                self.check_setgid_parent(clear_setgid)

    def check_setgid_parent(self, clear_setgid):
        groups = sorted(set(os.getgroups()) - {os.getegid()})
        if not groups:
            self.skipTest('no supplementary group differs from the effective group')
        shared_gid = groups[0]
        # Only an owned disposable fixture changes group; credentials stay intact.
        with tempfile.TemporaryDirectory(prefix='phrb setgid ') as tmp:
            directory = Path(tmp) / 'shared output'
            directory.mkdir()
            try:
                os.chown(directory, -1, shared_gid)
            except PermissionError as error:
                self.skipTest(f'cannot assign supplementary group to owned fixture: {error}')
            directory.chmod(0o2770)
            parent_stat = directory.stat()
            if parent_stat.st_gid != shared_gid or not parent_stat.st_mode & stat.S_ISGID:
                self.skipTest('fixture filesystem does not retain the requested group and setgid')
            command = '''
import json
import os
import stat
import sys
from pathlib import Path
from unittest.mock import patch
import tempfile
sys.path.insert(0, sys.argv[1])
from emu_hts2phrb_bounded_memory_contract import mixed_manifest
from hires_pack_emit_binary_package import emit_binary_package_from_manifest
directory = Path(sys.argv[2])
credentials = (os.geteuid(), os.getegid(), os.getgroups())
parent = directory.stat()
parent_policy = (parent.st_uid, parent.st_gid, stat.S_IMODE(parent.st_mode))
replace = Path.replace
chmod = Path.chmod
create_temporary_file = tempfile.NamedTemporaryFile
manifest, blobs = mixed_manifest()
previous_umask = os.umask(0o027)
try:
    results = []
    for setting in (None, 'off', '1', '6'):
        for mask, existing_mode in ((mask, mode) for mask in (0o027, 0o207, 0o777) for mode in (None, 0o604)):
            os.umask(mask)
            output = directory / f'{setting}-{mask}-{existing_mode}.phrb'
            if existing_mode is not None:
                output.write_bytes(b'previous package')
                output.chmod(existing_mode)
            creation = {}
            publication = {}
            private_writes = []

            def load(_index, _record, candidate):
                if not creation:
                    inner, = (path for path in directory.iterdir() if path != output and path.name.startswith('.'))
                    inner_stat = inner.stat()
                    creation.update(gid=inner_stat.st_gid, mode=stat.S_IMODE(inner_stat.st_mode))
                return blobs[candidate['materialized_path']]

            def publish(path, target):
                value = path.stat()
                publication.update(gid=value.st_gid, mode=stat.S_IMODE(value.st_mode))
                return replace(path, target)

            def change_mode(path, mode, *args, **kwargs):
                # Model the kernel clearing setgid for a nonmember, on this owned
                # supplementary-group fixture only. Assert real file results below.
                if sys.argv[3] == '1' and path.is_dir():
                    mode &= ~stat.S_ISGID
                return chmod(path, mode, *args, **kwargs)

            def private_temporary_file(*args, **kwargs):
                fp = create_temporary_file(*args, **kwargs)
                write = fp.file.write

                def private_write(data):
                    mode = stat.S_IMODE(os.fstat(fp.fileno()).st_mode)
                    assert mode & 0o077 == 0, 'unfinished payload is public'
                    private_writes.append(mode)
                    return write(data)

                fp.file.write = private_write
                return fp

            options = {} if setting is None else {'compress_raw_rgba': setting}
            with patch.object(Path, 'replace', publish), patch.object(Path, 'chmod', change_mode), patch(
                    'tempfile.NamedTemporaryFile', private_temporary_file):
                result = emit_binary_package_from_manifest(manifest, output, asset_blob_loader=load,
                    asset_storage_mode='legacy', **options)
            final = output.stat()
            results.append(dict(setting=setting, umask=mask, existing_mode=existing_mode, creation=creation,
                private_writes=private_writes,
                publication=publication, gid=final.st_gid, mode=stat.S_IMODE(final.st_mode),
                compressed=result.get('raw_rgba_compression', {}).get('compressed_assets', 0)))
    assert len(list(directory.iterdir())) == 24, 'temporary output leaked'
    final_parent = directory.stat()
    assert (final_parent.st_uid, final_parent.st_gid, stat.S_IMODE(final_parent.st_mode)) == parent_policy
    assert (os.geteuid(), os.getegid(), os.getgroups()) == credentials
    print(json.dumps(results))
finally:
    os.umask(previous_umask)
'''
            result = subprocess.run([sys.executable, '-B', '-c', command,
                str(Path(__file__).resolve().parent), directory.name, str(int(clear_setgid))], cwd=tmp,
                capture_output=True, text=True, timeout=30, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            results = json.loads(result.stdout)
            self.assertEqual(len(results), 24)
            for emitted in results:
                with self.subTest(setting=emitted['setting'], umask=oct(emitted['umask']),
                                  existing_mode=emitted['existing_mode']):
                    creation = emitted['creation']
                    self.assertEqual(creation['gid'], shared_gid)
                    creation_mode = 0o666 & ~emitted['umask']
                    self.assertEqual(creation['mode'] & 0o077, 0)
                    self.assertTrue(emitted['private_writes'])
                    self.assertTrue(all(mode & 0o077 == 0 for mode in emitted['private_writes']))
                    expected_mode = emitted['existing_mode'] if emitted['existing_mode'] is not None else creation_mode
                    self.assertEqual(emitted['publication'], {'gid': shared_gid, 'mode': expected_mode})
                    self.assertEqual(emitted['gid'], shared_gid)
                    self.assertEqual(emitted['mode'], expected_mode)
                    self.assertEqual(emitted['compressed'], 2 if emitted['setting'] in ('1', '6') else 0)


class OptionalRawCompression(unittest.TestCase):
    @unittest.skipUnless(sys.maxsize > 0xFFFFFFFF, 'large lengths require a 64-bit Python')
    def test_compression_respects_runtime_decoded_size_boundary(self):
        # RGBA byte counts are multiples of four: these straddle UINT32_MAX.
        # Only length and write sinks are simulated; the real emitter builds and
        # backpatches the tables without allocating or writing a multi-GiB blob.
        class SizedBlob:
            def __init__(self, size):
                self.size = size

            def __len__(self):
                return self.size

        create_temporary_file = tempfile.NamedTemporaryFile
        for width in (0x3FFFFFFF, 0x40000000):
            for setting in ('off', '1', '6'):
                with self.subTest(width=width, setting=setting), tempfile.TemporaryDirectory() as tmp:
                    manifest, _ = mixed_manifest()
                    record = manifest['records'][0]
                    candidate = dict(record['asset_candidates'][0], width=width, height=1, data_size=width * 4)
                    record.update(asset_candidates=[candidate], asset_candidate_count=1)
                    manifest['records'] = [record]
                    blob = SizedBlob(width * 4)
                    raw_writes = []

                    def bounded_temporary_file(*args, source_blob=blob, passthrough=raw_writes, **kwargs):
                        fp = create_temporary_file(*args, **kwargs)
                        write = fp.file.write

                        def bounded_write(data):
                            if data is source_blob:
                                passthrough.append(data)
                                return len(data)
                            return write(data)

                        fp.file.write = bounded_write
                        return fp

                    output = Path(tmp) / 'boundary.phrb'
                    with patch('tempfile.NamedTemporaryFile', bounded_temporary_file), patch(
                            'hires_pack_emit_binary_package.zlib.compress', return_value=b'encoded') as compress:
                        result = emit_binary_package_from_manifest(manifest, output,
                            asset_blob_loader=lambda *_, source_blob=blob: source_blob,
                            asset_storage_mode='legacy', compress_raw_rgba=setting)
                    emitted = output.read_bytes()
                    asset_table_offset = struct.unpack_from('<4sIIIIIII', emitted)[5]
                    fields = ASSET_STRUCT.unpack_from(emitted, asset_table_offset)
                    eligible = setting != 'off' and width == 0x3FFFFFFF
                    if eligible:
                        compress.assert_called_once_with(blob, int(setting))
                    else:
                        compress.assert_not_called()
                    self.assertEqual(raw_writes, [] if eligible else [blob])
                    self.assertEqual(fields[7], GL_RGBA8 | (GL_TEXFMT_GZ if eligible else 0))
                    self.assertEqual(fields[-2:], (0, len(b'encoded') if eligible else len(blob)))
                    self.assertEqual(result['blob_bytes'], fields[-1])
                    if setting != 'off':
                        self.assertEqual(result['raw_rgba_compression']['eligible_assets'], int(eligible))
                    self.assertLess(len(emitted), 4096)

    def test_jabo_cli_retains_already_compressed_v8_output(self):
        from PIL import Image

        with tempfile.TemporaryDirectory() as tmp:
            pack = Path(tmp) / 'jabo'
            pack.mkdir()
            md5 = '0123456789abcdef0123456789abcdef'
            Image.new('RGBA', (4, 4), (10, 20, 30, 128)).save(pack / f'{md5}.png')
            out = Path(tmp) / 'output'
            baseline = None
            for setting in ('off', '1', '6'):
                result = subprocess.run([sys.executable, str(REPO / 'tools/hts2phrb.py'),
                    '--cache', str(pack), '--output-dir', str(out), '--compress-raw-rgba', setting,
                    '--stdout-format', 'json', '--loose-png-workers', '1'],
                    capture_output=True, text=True, check=False)
                self.assertEqual(result.returncode, 0, result.stderr)
                data = (out / 'package.phrb').read_bytes()
                if baseline is None:
                    baseline = data
                self.assertEqual(data, baseline)
                package = inspect_binary_package(out / 'package.phrb')
                self.assertEqual(package['version'], 8)
                self.assertEqual(package['records'][0]['jabo_md5'], md5)
                asset, blob = stored_blobs(data, package)[0]
                self.assertTrue(asset['format'] & GL_TEXFMT_GZ)
                self.assertEqual(zlib.decompress(blob), bytes((10, 20, 30, 128)) * 16)
                if setting != 'off':
                    self.assertEqual(json.loads(result.stdout)['binary_package']['raw_rgba_compression']['eligible_assets'], 0)

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

    @unittest.skipUnless(os.name == 'posix', 'POSIX mode bits for the read-only deletion model')
    def test_readonly_staged_output_is_cleaned_after_failed_publication(self):
        manifest, blobs = mixed_manifest()
        unlink = Path.unlink

        def readonly_unlink(path, *args, **kwargs):
            # Model Windows' read-only deletion rule using the real staged mode.
            if path.is_file() and not path.stat().st_mode & stat.S_IWUSR:
                raise PermissionError('read-only staged output')
            return unlink(path, *args, **kwargs)

        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'out.phrb'
            output.write_bytes(b'previous package')
            output.chmod(0o440)
            try:
                with patch.object(Path, 'unlink', readonly_unlink), patch.object(
                        Path, 'replace', side_effect=OSError('publication failed')), self.assertRaisesRegex(
                            OSError, 'publication failed'):
                    emit_binary_package_from_manifest(manifest, output, blobs, asset_storage_mode='legacy')
                self.assertEqual(output.read_bytes(), b'previous package')
                self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o440)
                self.assertEqual(list(Path(tmp).iterdir()), [output])
            finally:
                output.chmod(0o600)

    def test_source_encode_write_and_publication_failures_do_not_publish(self):
        manifest, blobs = mixed_manifest()
        create_temporary_file = tempfile.NamedTemporaryFile
        open_file = Path.open
        stat_file = Path.stat
        chmod_file = Path.chmod

        def failing_chmod(path, *args, **kwargs):
            if path.is_file():
                raise OSError('injected output mode failure')
            return chmod_file(path, *args, **kwargs)

        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'out.phrb'
            cases = [(setting, failure) for setting in ('off', '1', '6')
                     for failure in ('probe', 'create', 'size', 'encode', 'write', 'backpatch', 'close', 'stat', 'mode', 'publish')
                     if setting != 'off' or failure not in ('encode', 'backpatch')]
            for setting, failure in cases:
                with self.subTest(setting=setting, failure=failure):
                    output.write_bytes(b'previous package')
                    output.chmod(0o640)
                    previous_mode = stat.S_IMODE(output.stat().st_mode)
                    bad_blobs = dict(blobs, **{'assets/noise': b'short'}) if failure == 'size' else blobs

                    def failing_temporary_file(*args, failure_kind=failure, **kwargs):
                        fp = create_temporary_file(*args, **kwargs)
                        write = fp.file.write
                        close = fp.file.close
                        written = 0

                        def fail_write(data):
                            nonlocal written
                            if ((failure_kind == 'write' and written + len(data) > 64)
                                    or (failure_kind == 'backpatch' and fp.tell() < written)):
                                raise OSError('injected output write failure')
                            written += len(data)
                            return write(data)

                        fp.file.write = fail_write
                        if failure_kind == 'close':
                            def fail_close():
                                fp.file.close = close
                                close()
                                raise OSError('injected output close failure')

                            fp.file.close = fail_close
                        return fp

                    def failing_probe(path, *args, **kwargs):
                        if path.name == 'mode':
                            raise OSError('injected probe creation failure')
                        return open_file(path, *args, **kwargs)

                    def failing_stat(path, *args, **kwargs):
                        if path == output:
                            raise OSError('injected destination stat failure')
                        return stat_file(path, *args, **kwargs)

                    if failure == 'probe':
                        failure_context = patch('pathlib.Path.open', failing_probe)
                    elif failure == 'create':
                        failure_context = patch('tempfile.NamedTemporaryFile', side_effect=OSError('injected creation failure'))
                    elif failure == 'stat':
                        failure_context = patch('pathlib.Path.stat', failing_stat)
                    elif failure in ('write', 'backpatch', 'close'):
                        failure_context = patch('tempfile.NamedTemporaryFile', failing_temporary_file)
                    elif failure == 'size':
                        failure_context = nullcontext()
                    elif failure == 'mode':
                        failure_context = patch('pathlib.Path.chmod', failing_chmod)
                    else:
                        target = {'encode': 'hires_pack_emit_binary_package.zlib.compress',
                                  'publish': 'pathlib.Path.replace'}[failure]
                        failure_context = patch(target, side_effect=OSError('injected failure'))
                    with failure_context, self.assertRaises((OSError, ValueError)):
                        emit_binary_package_from_manifest(manifest, output, bad_blobs,
                                                          asset_storage_mode='legacy', compress_raw_rgba=setting)
                    self.assertEqual(output.read_bytes(), b'previous package')
                    self.assertEqual(stat.S_IMODE(output.stat().st_mode), previous_mode)
                    self.assertEqual(list(Path(tmp).iterdir()), [output])


if __name__ == '__main__':
    unittest.main()
