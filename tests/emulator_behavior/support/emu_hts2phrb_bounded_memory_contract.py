"""Check that hts2phrb reads .hts blobs by offset instead of loading the whole cache."""
import json
import resource
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]

TXCACHE_FORMAT_VERSION = 0x08000000
GL_TEXFMT_GZ = 0x80000000
GL_RGBA8 = 0x8058
GL_RGBA = 0x1908
GL_UNSIGNED_BYTE = 0x1401
# Larger than the address-space limit below, so reading the cache into memory fails.
HOLE_BYTES = 3 << 30
ADDRESS_SPACE_LIMIT = 1 << 30


def write_sparse_hts(path: Path, textures):
    """Write a .hts whose records sit before and after a sparse multi-gigabyte hole."""
    index = []
    with path.open('wb') as fp:
        fp.write(struct.pack('<iiq', TXCACHE_FORMAT_VERSION, 0, 0))
        for position, (checksum64, rgba) in enumerate(textures):
            if position:
                fp.seek(HOLE_BYTES, 1)
            blob = zlib.compress(rgba)
            index.append((checksum64, fp.tell() | (0x0200 << 48)))
            fp.write(struct.pack('<IIIHHBHI', 2, 2, GL_RGBA8 | GL_TEXFMT_GZ, GL_RGBA, GL_UNSIGNED_BYTE, 1, 0x0200, len(blob)))
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
    def test_cache_larger_than_memory_limit_converts(self):
        textures = [(0x11111111, bytes((10, 20, 30, 255)) * 4), (0x22222222, bytes((40, 50, 60, 128)) * 4)]
        with tempfile.TemporaryDirectory() as tmp:
            cache = Path(tmp) / 'SPARSE_HIRESTEXTURES.hts'
            write_sparse_hts(cache, textures)
            out = Path(tmp) / 'out'
            result = subprocess.run(
                [sys.executable, str(REPO / 'tools/hts2phrb.py'), '--cache', str(cache), '--output-dir', str(out),
                 '--stdout-format', 'json'],
                capture_output=True, text=True, preexec_fn=limit_address_space,
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


if __name__ == '__main__':
    unittest.main()
