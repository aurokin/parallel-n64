"""Check loose Rice PNG and Glide64 .dat ingestion against GLideNHQ load semantics."""
import gzip
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

from hires_pack_common import parse_glide64_dat, read_entry_blob, resolve_legacy_cache_path  # noqa: E402
from hires_pack_loose_png import parse_rice_texture_name, scan_loose_png_pack  # noqa: E402

ROM = 'SUPER MARIO 64'


def save(path: Path, mode, size, color):
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.new(mode, size, color).save(path)


def rgba(entry):
    return zlib.decompress(read_entry_blob(None, entry))


class RiceNameParsing(unittest.TestCase):
    def parse(self, name, ident=ROM):
        return parse_rice_texture_name(name, ident)

    def test_key_and_formatsize_composition(self):
        fields, _ = self.parse(f'{ROM}#1234ABCD#2#1#89ABCDEF_ciByRGBA.png')
        self.assertEqual(fields['checksum64'], 0x89ABCDEF1234ABCD)
        self.assertEqual(fields['formatsize'], 2 | (1 << 8))
        fields, _ = self.parse(f'{ROM}#1234ABCD#0#2_all.png')
        self.assertEqual((fields['checksum64'], fields['formatsize']), (0x1234ABCD, 512))

    def test_wildcards_follow_txhirescache_key_rules(self):
        any_palette, _ = self.parse(f'{ROM}#1234ABCD#2#1#$_ciByRGBA.png')
        self.assertEqual((any_palette['checksum64'], any_palette['wildcard']), (0x1234ABCD, 'palette'))
        any_texture, _ = self.parse(f'{ROM}#$#2#1#89ABCDEF_ciByRGBA.png')
        self.assertEqual((any_texture['checksum64'], any_texture['wildcard']), (0x89ABCDEF, 'texture'))

    def test_accepts_every_glidenhq_suffix_case_insensitively(self):
        for suffix in ('all.png', 'allciByRGBA.png', 'ciByRGBA.png', 'rgb.png', 'rgb.bmp', 'a.png', 'a.bmp',
                       'ALL.PNG', 'cibyrgba.png'):
            fields, reason = self.parse(f'{ROM}#1234ABCD#2#1_{suffix}')
            self.assertIsNotNone(fields, (suffix, reason))
        self.assertIsNotNone(self.parse(f'super mario 64#1234abcd#2#1_all.png')[0])

    def test_rejects_names_glidenhq_would_skip(self):
        cases = {
            f'{ROM}#1234ABC#0#2_all.png': 'not-rice-naming',
            f'{ROM}#1234ABCD#00#2_all.png': 'not-rice-naming',
            f'{ROM}#1234ABCD#0#2_all - Copy.png': 'unsupported-suffix',
            f'{ROM}#1234ABCD#0#2_all.bmp': 'unsupported-suffix',
            f'{ROM}#1234ABCD#2#0#89ABCDEF_ci.bmp': 'rice-ci-bmp-unsupported',
            f'{ROM}#00000000#0#2_all.png': 'zero-crc',
            f'MARIOKART64#1234ABCD#0#2_all.png': 'rom-name-mismatch',
            f'{ROM}#1234ABCD#0#2_all.dds': 'not-png-or-bmp',
            'tex1_32x32_0123456789abcdef_3.png': 'rom-name-mismatch',
        }
        for name, reason in cases.items():
            self.assertEqual(self.parse(name), (None, reason), name)


class LoosePngLoad(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name) / ROM

    def tearDown(self):
        self.tmp.cleanup()

    def scan(self, **kwargs):
        entries, report = scan_loose_png_pack(self.root, workers=1, **kwargs)
        return {(entry['checksum64'], entry['formatsize']): entry for entry in entries}, report

    def test_rgb_alpha_pair_takes_alpha_from_red_channel(self):
        save(self.root / f'{ROM}#11111111#0#2_rgb.png', 'RGB', (2, 2), (10, 20, 30))
        save(self.root / f'{ROM}#11111111#0#2_a.png', 'RGB', (2, 2), (77, 5, 5))
        save(self.root / f'{ROM}#22222222#0#2_rgb.png', 'RGBA', (2, 2), (1, 2, 3, 9))
        save(self.root / f'{ROM}#33333333#0#2_a.png', 'L', (2, 2), 50)
        save(self.root / f'{ROM}#44444444#0#2_rgb.png', 'RGB', (2, 2), (1, 2, 3))
        save(self.root / f'{ROM}#44444444#0#2_a.png', 'L', (4, 4), 50)
        entries, report = self.scan()
        self.assertEqual(rgba(entries[(0x11111111, 512)]), bytes((10, 20, 30, 77)) * 4)
        self.assertEqual(rgba(entries[(0x22222222, 512)]), bytes((1, 2, 3, 255)) * 4)
        self.assertNotIn((0x33333333, 512), entries)
        self.assertNotIn((0x44444444, 512), entries)
        self.assertEqual(report['skip_reason_counts'], {
            'alpha-without-rgb': 1,
            'rgb-alpha-pair-member': 1,
            # Each file of a mismatched pair retries the pair load and fails.
            'rgb-alpha-size-mismatch': 2,
        })

    def test_intensity_alpha_and_rice_alpha_mode(self):
        save(self.root / f'{ROM}#11111111#4#1_all.png', 'RGB', (2, 2), (30, 60, 91))
        texel = Image.new('RGBA', (2, 2), (200, 100, 50, 255))
        texel.putpixel((1, 1), (0, 0, 0, 128))
        texel.save(self.root / f'{ROM}#22222222#2#1#AAAAAAAA_ciByRGBA.png')
        full, _ = self.scan()
        self.assertEqual(rgba(full[(0x11111111, 4 | (1 << 8))]), bytes((30, 60, 91, 60)) * 4)
        self.assertEqual(rgba(full[(0xAAAAAAAA22222222, 258)])[12:], bytes((0, 0, 0, 128)))
        rice, _ = self.scan(alpha_mode='rice')
        # Rice mode rounds alpha to 0/255 and fills the hole with the opaque neighbours' average.
        self.assertEqual(rgba(rice[(0xAAAAAAAA22222222, 258)])[12:], bytes((200, 100, 50, 0)))

    def test_first_loadable_file_wins_in_case_insensitive_depth_first_order(self):
        save(self.root / 'a' / f'{ROM}#11111111#0#2_all.png', 'RGB', (2, 2), (1, 1, 1))
        save(self.root / 'B' / f'{ROM}#11111111#0#2_all.png', 'RGB', (2, 2), (2, 2, 2))
        (self.root / 'A0').mkdir(parents=True)
        (self.root / 'A0' / f'{ROM}#22222222#0#2_all.png').write_bytes(b'not a png')
        save(self.root / 'b2' / f'{ROM}#22222222#0#2_all.png', 'RGB', (2, 2), (3, 3, 3))
        save(self.root / '.hidden' / f'{ROM}#33333333#0#2_all.png', 'RGB', (2, 2), (4, 4, 4))
        save(self.root / '~!~off' / f'{ROM}#44444444#0#2_all.png', 'RGB', (2, 2), (5, 5, 5))
        entries, report = self.scan()
        self.assertEqual(sorted(entries), [(0x11111111, 512), (0x22222222, 512)])
        self.assertEqual(entries[(0x11111111, 512)]['source_file'], f'a/{ROM}#11111111#0#2_all.png')
        self.assertEqual(rgba(entries[(0x22222222, 512)])[:4], bytes((3, 3, 3, 255)))
        self.assertEqual(report['skip_reason_counts'], {
            'decode-failed': 1,
            'duplicate-key': 1,
            'hidden-or-skip-prefix': 2,
        })

    def test_parallel_scan_matches_serial_scan(self):
        # Workers finish out of order and spool blobs as they arrive; the
        # entries, blobs and skip report must still follow traversal order.
        for index in range(12):
            save(self.root / f'{ROM}#{index + 1:08X}#0#2_all.png', 'RGB', (2 + index, 3), (index, 2 * index, 3))
        (self.root / 'a').mkdir()
        (self.root / 'a' / f'{ROM}#00000001#0#2_all.png').write_bytes(b'not a png')
        save(self.root / 'b' / f'{ROM}#00000002#0#2_all.png', 'RGB', (2, 2), (9, 9, 9))

        def loaded(workers):
            entries, report = scan_loose_png_pack(self.root, workers=workers)
            located = ('blob_spool', 'data_offset')
            return [({k: v for k, v in entry.items() if k not in located}, rgba(entry)) for entry in entries], report

        serial = loaded(1)
        self.assertEqual(loaded(3), serial)
        self.assertEqual(serial[1]['skip_reason_counts'], {'decode-failed': 1, 'duplicate-key': 1})

    def test_sixteen_bit_gray_keeps_its_transparency_key(self):
        image = Image.new('I;16', (2, 2), 0x12FF)
        image.putpixel((1, 1), 0xABCD)
        image.save(self.root.parent / 'gray16.png', transparency=0xABCD)
        self.root.mkdir(parents=True)
        (self.root.parent / 'gray16.png').rename(self.root / f'{ROM}#11111111#0#2_all.png')
        entries, _ = self.scan()
        self.assertEqual(rgba(entries[(0x11111111, 512)]), bytes((0x12, 0x12, 0x12, 0xFF)) * 3 + bytes((0xAB, 0xAB, 0xAB, 0)))

    def test_pair_partner_symlink_is_not_followed(self):
        outside = Path(self.tmp.name) / 'outside.png'
        save(outside, 'RGB', (2, 2), (9, 9, 9))
        self.root.mkdir(parents=True)
        (self.root / f'{ROM}#11111111#0#2_rgb.png').symlink_to(outside)
        save(self.root / f'{ROM}#11111111#0#2_a.png', 'L', (2, 2), 50)
        entries, report = self.scan()
        self.assertEqual(entries, {})
        self.assertEqual(report['skip_reason_counts']['symlink-not-followed'], 1)

    def test_renaming_a_nested_texture_changes_the_reuse_fingerprint(self):
        from hts2phrb import fingerprint_path
        save(self.root / 'sub' / f'{ROM}#11111111#0#2_all.png', 'RGB', (2, 2), (1, 1, 1))
        before = fingerprint_path(str(self.root))
        (self.root / 'sub' / f'{ROM}#11111111#0#2_all.png').rename(self.root / 'sub' / f'{ROM}#22222222#0#2_all.png')
        self.assertNotEqual(fingerprint_path(str(self.root)), before)

    def test_same_key_with_different_formatsize_is_kept(self):
        save(self.root / f'{ROM}#11111111#0#2_all.png', 'RGB', (2, 2), (1, 1, 1))
        save(self.root / f'{ROM}#11111111#3#1_all.png', 'RGB', (2, 2), (2, 2, 2))
        entries, _ = self.scan()
        self.assertEqual(sorted(entries), [(0x11111111, 3 | (1 << 8)), (0x11111111, 512)])


def write_glide64_dat(path: Path, config, records):
    with gzip.open(path, 'wb') as fp:
        fp.write(struct.pack('<I', config))
        for checksum64, width, height, gr_format, pixels in records:
            payload = zlib.compress(pixels)
            fp.write(struct.pack('<QiiHiiiiiiBi', checksum64, width, height, gr_format | 0x8000,
                                 0, 0, 0, 0, 0, 0, 1, len(payload)))
            fp.write(payload)


class Glide64DatAndConverter(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_glide64_dat_records_decode_to_rgba8(self):
        dat = self.dir / f'{ROM}_HIRESTEXTURES.dat'
        write_glide64_dat(dat, 0x40820000, [
            (0xAAAAAAAA11111111, 2, 2, 0x12, bytes((30, 20, 10, 40)) * 4),  # ARGB8888 stored as B, G, R, A
            (0x22222222, 2, 2, 0x0D, bytes((90, 7)) * 4),                     # AI88 stored as I, A
            (0x33333333, 2, 2, 0x16, bytes(8)),                               # DXT1: unsupported
        ])
        entries, report = parse_glide64_dat(dat)
        by_key = {entry['checksum64']: entry for entry in entries}
        self.assertEqual(rgba(by_key[0xAAAAAAAA11111111]), bytes((10, 20, 30, 40)) * 4)
        self.assertEqual(rgba(by_key[0x22222222]), bytes((90, 90, 90, 7)) * 4)
        self.assertEqual({entry['formatsize'] for entry in entries}, {0})
        self.assertEqual(report['skip_reason_counts'], {'unsupported-glide-format-0x16': 1})
        memory_cache = self.dir / f'{ROM}_MEMORYCACHE.dat'
        write_glide64_dat(memory_cache, 0x00400000, [])
        with self.assertRaises(ValueError):
            parse_glide64_dat(memory_cache)
        self.assertEqual(resolve_legacy_cache_path(self.dir)['resolved_path'], str(dat))

    def run_hts2phrb(self, cache, out, *extra):
        return subprocess.run(
            [sys.executable, str(REPO / 'tools/hts2phrb.py'), '--cache', str(cache), '--output-dir', str(out),
             '--stdout-format', 'json', *extra],
            capture_output=True, text=True,
        )

    def test_hts2phrb_converts_loose_png_directory(self):
        pack = self.dir / 'hires_texture' / ROM
        save(pack / f'{ROM}#11111111#0#2_all.png', 'RGBA', (4, 4), (10, 20, 30, 128))
        save(pack / 'sub' / f'{ROM}#22222222#2#1#AAAABBBB_ciByRGBA.png', 'RGB', (4, 4), (200, 0, 0))
        save(pack / f'{ROM}#11111111#0#2_all - Copy.png', 'RGB', (4, 4), (0, 0, 0))
        out = self.dir / 'out'
        result = self.run_hts2phrb(pack, out)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads((out / 'hts2phrb-report.json').read_text())
        self.assertEqual(report['resolved_cache_storage'], 'loose-png')
        self.assertEqual(report['source_input_report']['entry_count'], 2)
        self.assertEqual(report['source_input_report']['skip_reason_counts'], {'unsupported-suffix': 1})
        self.assertEqual(report['conversion_outcome'], 'promotable-runtime-package')
        inspected = subprocess.run(
            [sys.executable, str(REPO / 'tools/hires_pack_inspect_binary_package.py'), '--package', str(out / 'package.phrb')],
            capture_output=True, text=True, check=True,
        )
        package = json.loads(inspected.stdout)
        candidates = [candidate for record in package['records'] for candidate in record['asset_candidates']]
        self.assertEqual(
            sorted((candidate['legacy_checksum64'], candidate['legacy_formatsize']) for candidate in candidates),
            [('0000000011111111', 512), ('aaaabbbb22222222', 258)],
        )
        # The blob section ends the file; candidate offsets are relative to it.
        data = (out / 'package.phrb').read_bytes()
        blob_section = data[len(data) - package['blob_bytes']:]
        candidate = next(item for item in candidates if item['legacy_checksum64'] == '0000000011111111')
        blob = blob_section[candidate['rgba_blob_offset']:candidate['rgba_blob_offset'] + candidate['rgba_blob_size']]
        self.assertEqual(zlib.decompress(blob), bytes((10, 20, 30, 128)) * 16)

    def test_hts2phrb_requires_opt_in_for_glide64_dat(self):
        dat = self.dir / f'{ROM}_HIRESTEXTURES.dat'
        write_glide64_dat(dat, 0x40820000, [(0x11111111, 2, 2, 0x12, bytes((1, 2, 3, 4)) * 4)])
        refused = self.run_hts2phrb(dat, self.dir / 'refused')
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn('--accept-glide64-pow2-padding', refused.stderr)
        accepted = self.run_hts2phrb(dat, self.dir / 'accepted', '--accept-glide64-pow2-padding')
        self.assertEqual(accepted.returncode, 0, accepted.stderr)
        report = json.loads((self.dir / 'accepted' / 'hts2phrb-report.json').read_text())
        self.assertEqual(report['source_input_report']['entry_count'], 1)


if __name__ == '__main__':
    unittest.main()
