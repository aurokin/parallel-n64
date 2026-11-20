#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import argparse
import json
import struct
import sys
from pathlib import Path

MAGIC = b'PHRB'
HEADER_STRUCT = struct.Struct('<4sIIIIIII')
RECORD_STRUCT = struct.Struct('<IIIIIIIIIIIIII')
RECORD_KEY_STRUCT_V8 = struct.Struct('<I16s')
ASSET_STRUCT = struct.Struct('<IIIIIIIIIIIQQQQ')
RECORD_FLAG_RUNTIME_READY = 1 << 0


def read_c_string(blob, offset):
    end = blob.find(b'\0', offset)
    if end < 0:
        raise ValueError(f'Missing NUL terminator for string at offset {offset}')
    return blob[offset:end].decode('utf-8')


def inspect_binary_package(path: Path):
    """Describe a version 7 or 8 package (tools/PHRB_FORMAT.md), reading its tables but not its payloads."""
    with path.open('rb') as fp:
        header = fp.read(HEADER_STRUCT.size)
        if len(header) < HEADER_STRUCT.size:
            raise ValueError('Package too small for header.')
        magic, version, record_count, asset_count, record_off, asset_off, string_off, blob_off = HEADER_STRUCT.unpack(header)
        if magic != MAGIC:
            raise ValueError(f'Unexpected magic: {magic!r}')
        if version not in (7, 8):
            raise ValueError(f'Unsupported version: {version}')
        fp.seek(0)
        data = fp.read(blob_off)
        if len(data) < blob_off:
            raise ValueError('Package ends inside its tables.')
        fp.seek(0, 2)
        file_size = fp.tell()

    string_blob = data[string_off:blob_off]
    records = []
    record_size = RECORD_STRUCT.size + (RECORD_KEY_STRUCT_V8.size if version >= 8 else 0)
    for i in range(record_count):
        off = record_off + i * record_size
        (policy_key_off, sampled_object_id_off, record_flags, fmt, siz, tex_off, stride, width, height, formatsize,
         sampled_low32, sampled_entry_pcrc, sampled_sparse_pcrc, asset_candidate_count) = RECORD_STRUCT.unpack_from(data, off)
        records.append({
            'record_index': i,
            'policy_key': read_c_string(string_blob, policy_key_off),
            'sampled_object_id': read_c_string(string_blob, sampled_object_id_off),
            'record_flags': record_flags,
            'runtime_ready': bool(record_flags & RECORD_FLAG_RUNTIME_READY),
            'canonical_identity': {
                'fmt': fmt,
                'siz': siz,
                'off': tex_off,
                'stride': stride,
                'wh': f'{width}x{height}',
                'formatsize': formatsize,
                'sampled_low32': sampled_low32,
                'sampled_entry_pcrc': sampled_entry_pcrc,
                'sampled_sparse_pcrc': sampled_sparse_pcrc,
            },
            'asset_candidate_count': asset_candidate_count,
            'asset_candidates': [],
        })
        if version >= 8:
            key_kind, key = RECORD_KEY_STRUCT_V8.unpack_from(data, off + RECORD_STRUCT.size)
            records[-1]['key_kind'] = {0: 'rice', 1: 'jabo-md5'}.get(key_kind, key_kind)
            if key_kind == 1:
                records[-1]['jabo_md5'] = key.hex()

    for i in range(asset_count):
        (record_index, replacement_id_off, legacy_source_path_off, rgba_rel_path_off, variant_group_id_off,
         width, height, format_value, texture_format, pixel_type, legacy_formatsize, selector_checksum64, legacy_checksum64,
         rgba_blob_off, rgba_blob_size) = ASSET_STRUCT.unpack_from(data, asset_off + i * ASSET_STRUCT.size)
        records[record_index]['asset_candidates'].append({
            'replacement_id': read_c_string(string_blob, replacement_id_off),
            'legacy_source_path': read_c_string(string_blob, legacy_source_path_off),
            'rgba_rel_path': read_c_string(string_blob, rgba_rel_path_off),
            'variant_group_id': read_c_string(string_blob, variant_group_id_off),
            'width': width,
            'height': height,
            'format': format_value,
            'texture_format': texture_format,
            'pixel_type': pixel_type,
            'legacy_formatsize': legacy_formatsize,
            'selector_checksum64': f'{selector_checksum64:016x}',
            'legacy_checksum64': f'{legacy_checksum64:016x}',
            'rgba_blob_offset': rgba_blob_off,
            'rgba_blob_size': rgba_blob_size,
        })

    return {
        'path': str(path),
        'version': version,
        'record_count': record_count,
        'asset_count': asset_count,
        'string_table_bytes': len(string_blob),
        'blob_bytes': file_size - blob_off,
        'records': records,
    }


def main():
    parser = argparse.ArgumentParser(description='Inspect a PHRB binary hi-res package.')
    parser.add_argument('--package', required=True, help='Path to .phrb package.')
    parser.add_argument('--output', help='Optional output JSON path.')
    args = parser.parse_args()

    result = inspect_binary_package(Path(args.package))
    serialized = json.dumps(result, indent=2) + '\n'
    if args.output:
        Path(args.output).write_text(serialized)
    else:
        sys.stdout.write(serialized)


if __name__ == '__main__':
    main()
