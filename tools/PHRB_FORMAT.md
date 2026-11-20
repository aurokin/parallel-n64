# PHRB package format

A `.phrb` file holds the hi-res replacements for one game: identity records,
image assets and their pixel payloads. [`hts2phrb.py`](hts2phrb.py) writes it
through [`hires_pack_emit_binary_package.py`](hires_pack_emit_binary_package.py);
the core reads it in
`mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_replacement.cpp`.
This document is the reference for both, and is MIT like the converter.

Versions 7 and 8 are current. They differ only in the record size: version 8
appends a key kind and a 16-byte key to every record. The converter writes
version 8 when any record carries a Jabo MD5, otherwise version 7. The core
rejects every other version.

## Layout

All integers are little-endian and unaligned. Five sections follow each
other with no gaps, in this order:

| Section | Offset | Size |
| --- | --- | --- |
| Header | 0 | 32 |
| Record table | 32 | `record_count` × 56 (v7) or 76 (v8) |
| Asset table | `asset_table_offset` | `asset_count` × 76 |
| String table | `string_table_offset` | `blob_offset − string_table_offset` |
| Payload section | `blob_offset` | to the end of the file |

### Header (32 bytes)

| Offset | Type | Field |
| --- | --- | --- |
| 0 | char[4] | magic `PHRB` |
| 4 | u32 | `version`: 7 or 8 |
| 8 | u32 | `record_count` |
| 12 | u32 | `asset_count` |
| 16 | u32 | `record_table_offset`: always 32 |
| 20 | u32 | `asset_table_offset` |
| 24 | u32 | `string_table_offset` |
| 28 | u32 | `blob_offset`: start of the payload section |

### Record (56 bytes in v7, 76 in v8)

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u32 | `policy_key_offset` (string) |
| 4 | u32 | `sampled_object_id_offset` (string) |
| 8 | u32 | `record_flags`: bit 0 = runtime-ready; other bits are ignored |
| 12 | u32 | `fmt`: N64 texel format of the sampled view |
| 16 | u32 | `siz`: texel size |
| 20 | u32 | `tex_offset`: TMEM offset |
| 24 | u32 | `stride` |
| 28 | u32 | `width` of the sampled view |
| 32 | u32 | `height` of the sampled view |
| 36 | u32 | `formatsize`: Rice `siz << 8 \| fmt` (258 is CI8); must fit 16 bits |
| 40 | u32 | `sampled_low32`: Rice texture CRC |
| 44 | u32 | `sampled_entry_pcrc`: palette CRC over the used entries, or 0 |
| 48 | u32 | `sampled_sparse_pcrc`: palette CRC over the sparse range, or 0 |
| 52 | u32 | `asset_candidate_count` |
| 56 | u32 | v8 only: `key_kind`: 0 Rice, 1 Jabo MD5 |
| 60 | u8[16] | v8 only: `key`: the MD5 digest bytes for kind 1, zero for kind 0 |

A record owns the next `asset_candidate_count` assets: record *n*'s assets
follow those of records 0 to *n*−1 in the asset table.

### Asset (76 bytes)

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u32 | `record_index`: the owning record |
| 4 | u32 | `replacement_id_offset` (string) |
| 8 | u32 | `legacy_source_path_offset` (string, provenance only) |
| 12 | u32 | `rgba_rel_path_offset` (string, provenance only) |
| 16 | u32 | `variant_group_id_offset` (string, provenance only) |
| 20 | u32 | `width` of the replacement image |
| 24 | u32 | `height` |
| 28 | u32 | `format`: bit 31 set = zlib payload; low bits name the GL internal format and are informational |
| 32 | u32 | `texture_format` (GL) |
| 36 | u32 | `pixel_type` (GL) |
| 40 | u32 | `legacy_formatsize`: Rice formatsize for family records, or 0; must fit 16 bits |
| 44 | u64 | `selector_checksum64`: variant selector, or 0 |
| 52 | u64 | `legacy_checksum64`: Rice key for family records (`palette_crc << 32 \| texture_crc`) |
| 60 | u64 | `rgba_blob_offset`: relative to `blob_offset` |
| 68 | u64 | `rgba_blob_size` |

### Strings

A string field is a byte offset into the string table and names the
NUL-terminated bytes starting there. The table's last byte is NUL. Text is
UTF-8 from the converter, which stores each distinct string once; the core
does not interpret the bytes.

### Payloads and pixel encodings

An asset's payload is `rgba_blob_size` bytes at
`blob_offset + rgba_blob_offset`. A raw payload is exactly
`width × height × bytes per pixel`. A zlib payload (`format` bit 31) is a
zlib stream (RFC 1950) that inflates to exactly that many bytes. Pixels are
row-major from the top-left. Supported `texture_format`/`pixel_type` pairs:

| Pair | Bytes per pixel | Decoded RGBA8 |
| --- | --- | --- |
| `GL_RGBA 0x1908` / `GL_UNSIGNED_BYTE 0x1401` | 4 | bytes R, G, B, A |
| `GL_RGB 0x1907` / `GL_UNSIGNED_BYTE` | 3 | bytes R, G, B; A = 255 |
| `GL_RGB` / `GL_UNSIGNED_SHORT_5_6_5 0x8363` | 2 | LE u16: R 15–11, G 10–5, B 4–0; A = 255 |
| `GL_RGBA` / `GL_UNSIGNED_SHORT_5_5_5_1 0x8034` | 2 | LE u16: R 15–11, G 10–6, B 5–1, A = bit 0 ? 255 : 0 |
| `GL_RGBA` / `GL_UNSIGNED_SHORT_4_4_4_4 0x8033` | 2 | LE u16: R 15–12, G 11–8, B 7–4, A 3–0 |
| `GL_LUMINANCE 0x1909` / `GL_UNSIGNED_BYTE` | 1 | R = G = B = byte; A = 255 |
| `GL_COLOR_INDEX 0x1900` / `GL_UNSIGNED_BYTE` | 1 | palette index, coloured with the draw's live TLUT |

Channels widen by bit replication (5 bits `v << 3 | v >> 2`, and so on).
An indexed asset decodes only when its entry's formatsize is 2 (CI4, indices
0–15) or 258 (CI8, 0–255) and the draw supplies a palette of that size, and
every index must be in range. The
converter writes RGBA8 (`format` 0x8058) or CI8 (`format` 0x80E5), raw or
zlib; legacy-cache conversion copies the cache's stored encoding.

## Entries

Each asset of a loadable record becomes lookup entries. A record is loadable
when bit 0 of its flags is set and its key kind is 0 or 1 (other kinds are
reserved for newer converters); the assets of other records are checked as
described below but produce no entries.

- Key kind 1 (Jabo): one entry keyed by the 16-byte MD5, read as two
  little-endian u64 halves (bytes 0–7, then 8–15).
- Family record (`fmt`, `siz`, `tex_offset`, `stride`, `width` and `height`
  all 0): one entry keyed by `legacy_checksum64`, `legacy_formatsize` (or the
  record's formatsize when that is 0) and `selector_checksum64`. An asset
  whose `legacy_checksum64` is 0 produces no entry.
- Any other record: one entry per distinct nonzero palette CRC among
  `sampled_sparse_pcrc` then `sampled_entry_pcrc`, or a single entry with
  palette 0 when both are 0. The key is `palette_crc << 32 | sampled_low32`
  with the record's formatsize and the asset's selector, plus the record's
  sampled view fields.

Packages in one directory load in byte order of their file names. How lookups
choose between entries, including duplicates and later packages, is the
provider's policy in `texture_replacement.cpp`, not part of the format.

## Load checks and limits

The core checks the following before using any value and rejects the whole
package if one fails, logging `Hi-res PHRB <path> rejected: <reason>` and
loading nothing from it. Payloads are not read at load.

- The file is at least 32 bytes, starts with `PHRB` and has version 7 or 8.
- `record_count` and `asset_count` are each at most 262,144 (2^18).
- `record_table_offset` is 32, `asset_table_offset` and `string_table_offset`
  are exactly the end of the preceding table, and
  `string_table_offset ≤ blob_offset ≤` 128 MiB and the file size.
- The string table is empty or ends with NUL, and every string offset in every
  record and asset is inside it.
- Record `formatsize` fits 16 bits, and the records' asset counts never exceed
  the assets left and sum to `asset_count`.
- Every asset names the record that owns it, and its payload range lies inside
  the payload section (computed without overflow).
- For assets of loadable records: `legacy_formatsize` fits 16 bits, `width`
  and `height` are 1 to 16,384, `width × height` is at most 2^26 (256 MiB
  as RGBA8), the pixel pair is in the table above, a raw payload has exactly
  the image's size, and a zlib payload has 1 to `compressBound(decoded size)`
  bytes.
- At least one entry results.

Allowed and not checked: trailing bytes after the last payload, payloads that
overlap or are shared, unused string-table bytes, and the `format` low bits.
At decode the core also requires the zlib stream to inflate to exactly the
decoded size and indexed assets to meet the rules above; a failed decode
leaves that texture unreplaced. The converter refuses to write a package that
would fail a load check. It writes payloads in asset order from offset 0, the
last ending at the end of the file.

An asset yields at most two entries. Loading peaks at about 700 bytes per
entry, indices included, so a package at the count limits (524,288 entries)
peaks near 360 MB of process memory and the largest converted pack (51,211
entries) near 40 MB. The string table is kept once per package. Payloads are
read and decoded one texture at a time, at most 256 MiB each.
