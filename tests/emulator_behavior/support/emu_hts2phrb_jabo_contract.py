"""Check Project64/Jabo pack ingestion and the PHRB version-8 package format."""
import hashlib
import json
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / 'tools'))

from hires_pack_common import resolve_legacy_cache_path  # noqa: E402
from hires_pack_emit_binary_package import emit_binary_package_from_manifest  # noqa: E402
from hires_pack_inspect_binary_package import inspect_binary_package  # noqa: E402
from hires_pack_jabo import build_jabo_package_manifest, decode_dds, decode_jabo_image, parse_jabo_texture_name, scan_jabo_pack  # noqa: E402

MD5_A = '0123456789abcdef0123456789abcdef'
MD5_B = 'fedcba9876543210fedcba9876543210'


def save(path: Path, size, color, mode='RGBA', fmt=None, **kwargs):
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.new(mode, size, color).save(path, format=fmt, **kwargs)


def dds(width, height, payload, fourcc=b'', bit_count=0, masks=(0, 0, 0, 0), pitch=0):
    header = bytearray(128)
    header[:4] = b'DDS '
    struct.pack_into('<IIIII', header, 4, 124, 0x1007 | (0x8 if pitch else 0), height, width, pitch)
    pf_flags = 0x4 if fourcc else 0x40 | (0x1 if masks[3] else 0)
    struct.pack_into('<II4sI4I', header, 76, 32, pf_flags, fourcc.ljust(4, b'\0'), bit_count, *masks)
    return bytes(header) + payload


def texels(width, height, rgba):
    return [tuple(rgba[i:i + 4]) for i in range(0, width * height * 4, 4)]


class JaboNameParsing(unittest.TestCase):
    def test_accepts_md5_names_with_image_extensions_in_any_case(self):
        for name in (f'{MD5_A.upper()}.png', f'{MD5_A}.PNG', f'{MD5_A.upper()}.JPG', f'{MD5_A}.jpeg', f'{MD5_A}.dds'):
            self.assertEqual(parse_jabo_texture_name(name), (MD5_A, None), name)

    def test_reports_names_jabo_would_not_load(self):
        cases = {
            f'{MD5_A.upper()} - Copy.png': 'suffixed-copy',
            f'{MD5_A.upper()}xx.png': 'suffixed-copy',
            f'{MD5_A.upper()} .png': 'suffixed-copy',
            f'{MD5_A.upper()}.pspimage': 'unsupported-image-format',
            f'{MD5_A.upper()}.bmp': 'unsupported-image-format',
            f'{MD5_A[:31]}.png': 'not-jabo-name',
            'SUPER MARIO 64#1234ABCD#0#2_all.png': 'not-jabo-name',
            'Thumbs.db': 'thumbs-db',
            'pack.xml': 'pack-metadata',
            'roms.ini': 'pack-metadata',
            'Read Me .txt': 'not-jabo-name',
        }
        for name, reason in cases.items():
            self.assertEqual(parse_jabo_texture_name(name), (None, reason), name)


class JaboPackScan(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name) / 'MARIOKART64'

    def tearDown(self):
        self.tmp.cleanup()

    def scan(self):
        entries, report = scan_jabo_pack(self.root, workers=1)
        return {entry['jabo_md5']: entry for entry in entries}, report

    def test_first_loadable_file_wins_depth_first_case_insensitive(self):
        save(self.root / 'b' / f'{MD5_A.upper()}.png', (2, 2), (2, 2, 2, 255))
        save(self.root / 'A' / f'{MD5_A}.png', (2, 2), (1, 1, 1, 255))
        (self.root / 'A' / 'x').mkdir()
        (self.root / 'A' / 'x' / f'{MD5_B}.png').write_bytes(b'not an image')
        save(self.root / 'a2' / f'{MD5_B.upper()}.JPG', (2, 2), (200, 100, 50), mode='RGB', fmt='JPEG')
        save(self.root / f'{MD5_A.upper()} - Copy.png', (2, 2), (9, 9, 9, 255))
        (self.root / 'Thumbs.db').write_bytes(b'x')
        (self.root / 'pack.xml').write_text('<pack/>')
        entries, report = self.scan()
        self.assertEqual(sorted(entries), [MD5_A, MD5_B])
        self.assertEqual(entries[MD5_A]['source_file'], f'A/{MD5_A}.png')
        self.assertEqual(zlib.decompress(entries[MD5_A]['blob']), bytes((1, 1, 1, 255)) * 4)
        self.assertEqual(entries[MD5_B]['source_file'], f'a2/{MD5_B.upper()}.JPG')
        self.assertEqual(report['skip_reason_counts'], {
            'decode-failed': 1,
            'duplicate-key': 1,
            'pack-metadata': 1,
            'suffixed-copy': 1,
            'thumbs-db': 1,
        })
        self.assertEqual(report['pack_metadata'], [{'path': 'pack.xml', 'bytes': 7}])
        self.assertEqual(report['content_format_counts'], {'jpeg': 1, 'png': 1})

    def test_symlinks_are_not_followed(self):
        outside = Path(self.tmp.name) / 'outside.png'
        save(outside, (2, 2), (1, 2, 3, 4))
        self.root.mkdir(parents=True)
        (self.root / f'{MD5_A}.png').symlink_to(outside)
        entries, report = self.scan()
        self.assertEqual(entries, {})
        self.assertEqual(report['skip_reason_counts'], {'symlink-not-followed': 1})

    def test_directory_detection_prefers_the_majority_naming(self):
        save(self.root / f'{MD5_A}.png', (2, 2), (1, 1, 1, 255))
        self.assertEqual(resolve_legacy_cache_path(self.root)['resolved_storage'], 'jabo-md5')
        save(self.root / 'SUPER MARIO 64#1234ABCD#0#2_all.png', (2, 2), (1, 1, 1, 255))
        save(self.root / 'SUPER MARIO 64#2234ABCD#0#2_all.png', (2, 2), (1, 1, 1, 255))
        self.assertEqual(resolve_legacy_cache_path(self.root)['resolved_storage'], 'loose-png')


class ImageDecoding(unittest.TestCase):
    def test_dxt1_four_and_three_colour_blocks(self):
        # Red/blue endpoints, texels 0..3 use indices 0..3, the rest index 0.
        four = struct.pack('<HHI', 0xF800, 0x001F, 0b11100100)
        three = struct.pack('<HHI', 0x001F, 0xF800, 0b11100100)
        width, height, rgba = decode_dds(dds(8, 4, four + three, b'DXT1'))
        self.assertEqual((width, height), (8, 4))
        pixels = texels(8, 4, rgba)
        self.assertEqual(pixels[:4], [(255, 0, 0, 255), (0, 0, 255, 255), (170, 0, 85, 255), (85, 0, 170, 255)])
        self.assertEqual(pixels[4:8], [(0, 0, 255, 255), (255, 0, 0, 255), (127, 0, 127, 255), (0, 0, 0, 0)])

    def test_dxt3_and_dxt5_alpha_and_partial_blocks(self):
        color = struct.pack('<HHI', 0x07E0, 0x07E0, 0)  # solid green
        # A 2x2 image uses block texels 0, 1, 4 and 5.
        dxt3_alpha = sum(value << (4 * i) for i, value in ((0, 0), (1, 5), (4, 15), (5, 8))).to_bytes(8, 'little')
        _, _, rgba = decode_dds(dds(2, 2, dxt3_alpha + color, b'DXT3'))
        self.assertEqual(texels(2, 2, rgba), [(0, 255, 0, 0), (0, 255, 0, 85), (0, 255, 0, 255), (0, 255, 0, 136)])
        dxt5_alpha = bytes((255, 0)) + sum(value << (3 * i) for i, value in enumerate((0, 1, 2, 7))).to_bytes(6, 'little')
        _, _, rgba = decode_dds(dds(4, 1, dxt5_alpha + color, b'DXT5'))
        self.assertEqual([pixel[3] for pixel in texels(4, 1, rgba)], [255, 0, 219, 36])

    def test_uncompressed_bgra_and_unsupported_formats(self):
        masks = (0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
        _, _, rgba = decode_dds(dds(1, 1, bytes((30, 20, 10, 40)), bit_count=32, masks=masks))
        self.assertEqual(rgba, bytes((10, 20, 30, 40)))
        with self.assertRaises(ValueError):
            decode_dds(dds(4, 4, bytes(16), b'ATI2'))
        with self.assertRaises(ValueError):
            decode_dds(dds(8, 8, bytes(8), b'DXT1'))
        # A header claiming a huge image without its payload fails before allocating it.
        with self.assertRaises(ValueError):
            decode_dds(dds(16384, 16384, b'', bit_count=32, masks=masks))

    def test_uncompressed_rows_follow_the_declared_pitch(self):
        masks = (0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
        row = lambda value: bytes((value, value, value, 255)) * 2 + bytes(8)  # 2 texels + 8 padding bytes
        _, _, rgba = decode_dds(dds(2, 2, row(1) + row(2), bit_count=32, masks=masks, pitch=16))
        self.assertEqual(rgba, bytes((1, 1, 1, 255)) * 2 + bytes((2, 2, 2, 255)) * 2)

    def test_jpeg_decodes_opaque_by_content(self):
        with tempfile.TemporaryDirectory() as tmp:
            # A JPEG under a .png name decodes by content, as D3DX does.
            path = Path(tmp) / f'{MD5_A}.png'
            save(path, (16, 8), (200, 100, 50), mode='RGB', fmt='JPEG', quality=95)
            content, width, height, rgba = decode_jabo_image(path)
        self.assertEqual((content, width, height), ('jpeg', 16, 8))
        self.assertEqual(set(rgba[3::4]), {255})
        for channel, expected in enumerate((200, 100, 50)):
            self.assertLessEqual(max(abs(value - expected) for value in rgba[channel::4]), 3)


def rice_manifest():
    records = []
    for index, (checksum64, formatsize) in enumerate(((0x89ABCDEF1234ABCD, 258), (0x1234ABCD, 512))):
        records.append({
            'policy_key': f'policy-{index}',
            'sampled_object_id': f'family-{index}',
            'canonical_identity': {'fmt': 0, 'siz': 0, 'off': 0, 'stride': 0, 'wh': '0x0', 'formatsize': formatsize},
            'asset_candidate_count': 1,
            'asset_candidates': [{
                'replacement_id': f'replacement-{index}',
                'legacy_source_path': 'cache.hts',
                'materialized_path': f'assets/{index}.rgba',
                'variant_group_id': f'group-{index}',
                'legacy_checksum64': f'{checksum64:016x}',
                'legacy_texture_crc': f'{checksum64 & 0xFFFFFFFF:08x}',
                'legacy_palette_crc': f'{checksum64 >> 32:08x}',
                'legacy_formatsize': formatsize,
                'width': 2,
                'height': 2,
                'data_size': 16,
            }],
        })
    blobs = {f'assets/{index}.rgba': bytes((index, 2, 3, 4)) * 4 for index in range(2)}
    return {'records': records}, blobs


class PackageFormat(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_rice_only_output_stays_byte_identical_version_7(self):
        manifest, blobs = rice_manifest()
        path = self.dir / 'rice.phrb'
        emit_binary_package_from_manifest(manifest, path, blobs)
        # Digest of this manifest from the version-7 emitter before Jabo support (88bad189).
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),
                         'e4ac1ccd3699c34e9cf99bdc7b49f6da9c19c2a8c26107a741407153f94afa0f')
        self.assertEqual(inspect_binary_package(path)['version'], 7)

    def test_version_8_round_trips_jabo_and_rice_records(self):
        manifest, blobs = rice_manifest()
        jabo = build_jabo_package_manifest([{
            'jabo_md5': MD5_A, 'source_file': 'HUD/x.png', 'width': 2, 'height': 2, 'format': 0x80008058,
            'texture_format': 0x1908, 'pixel_type': 0x1401, 'data_size': 5,
        }])
        manifest['records'] += jabo['records']
        blobs[f'jabo-{MD5_A}'] = b'\x01\x02\x03\x04\x05'
        path = self.dir / 'mixed.phrb'
        emit_binary_package_from_manifest(manifest, path, blobs, asset_storage_mode='legacy')
        package = inspect_binary_package(path)
        self.assertEqual(package['version'], 8)
        self.assertEqual([record['key_kind'] for record in package['records']], ['rice', 'rice', 'jabo-md5'])
        self.assertEqual(package['records'][2]['jabo_md5'], MD5_A)
        self.assertEqual(package['records'][1]['asset_candidates'][0]['legacy_checksum64'], '000000001234abcd')
        candidate = package['records'][2]['asset_candidates'][0]
        self.assertEqual((candidate['legacy_source_path'], candidate['width'], candidate['legacy_formatsize']),
                         ('HUD/x.png', 2, 0))
        data = path.read_bytes()
        blob_section = data[len(data) - package['blob_bytes']:]
        offset = candidate['rgba_blob_offset']
        self.assertEqual(blob_section[offset:offset + candidate['rgba_blob_size']], b'\x01\x02\x03\x04\x05')

    def test_hts2phrb_converts_a_jabo_directory(self):
        pack = self.dir / 'hires_textures' / 'MARIOKART64'
        save(pack / 'HUD' / f'{MD5_A.upper()}.png', (4, 4), (10, 20, 30, 128))
        (pack / f'{MD5_B.upper()}.dds').write_bytes(dds(4, 4, struct.pack('<HHI', 0xF800, 0xF800, 0), b'DXT1'))
        save(pack / f'{MD5_A.upper()}xx.png', (4, 4), (0, 0, 0, 255))
        out = self.dir / 'out'
        result = subprocess.run(
            [sys.executable, str(REPO / 'tools/hts2phrb.py'), '--cache', str(pack), '--output-dir', str(out),
             '--stdout-format', 'json', '--require-promotable'],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads((out / 'hts2phrb-report.json').read_text())
        self.assertEqual(report['resolved_cache_storage'], 'jabo-md5')
        self.assertEqual(report['binary_package_version'], 8)
        self.assertEqual(report['source_input_report']['skip_reason_counts'], {'suffixed-copy': 1})
        self.assertTrue((out / 'source-input-report.json').exists())
        package = inspect_binary_package(out / 'package.phrb')
        self.assertEqual([record['jabo_md5'] for record in package['records']], [MD5_A, MD5_B])
        data = (out / 'package.phrb').read_bytes()
        blob_section = data[len(data) - package['blob_bytes']:]
        decoded = []
        for record in package['records']:
            candidate = record['asset_candidates'][0]
            offset = candidate['rgba_blob_offset']
            decoded.append(zlib.decompress(blob_section[offset:offset + candidate['rgba_blob_size']]))
        self.assertEqual(decoded, [bytes((10, 20, 30, 128)) * 16, bytes((255, 0, 0, 255)) * 16])
        refused = subprocess.run(
            [sys.executable, str(REPO / 'tools/hts2phrb.py'), '--cache', str(pack), '--output-dir', str(out),
             '--low32', '1234abcd', '--formatsize', '258'],
            capture_output=True, text=True,
        )
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn('--low32', refused.stderr)


if __name__ == '__main__':
    unittest.main()
