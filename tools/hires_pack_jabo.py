#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Load a Project64/Jabo texture pack and build its PHRB records.

Jabo names each replacement `<MD5>.<ext>`: 32 hex digits for the MD5 of the
texture as Jabo uploaded it to Direct3D (see
mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_jabo_key.hpp).
Packs keep these files in arbitrary category folders. This module scans such
a folder offline:

- Image names must be exactly 32 hex digits plus `.png`, `.dds`, `.jpg` or
  `.jpeg`, in any case; the key is the lowercase MD5. Suffixed copies
  (`<MD5> - Copy.png`, `<MD5>xx.png`), other image formats (`.pspimage`,
  `.bmp`), `Thumbs.db` and every other file are skipped and reported.
  `pack.xml` and `roms.ini` are listed in the report only.
- Traversal is depth-first in case-insensitive name order without following
  symlinks, as in hires_pack_loose_png. The first loadable file for an MD5 wins.
- Content decides the decoder: PNG and JPEG via Pillow (RGB gets alpha 255),
  DDS DXT1/DXT3/DXT5 or uncompressed 32-bit RGB(A) top mip via the decoder
  below. Alpha is kept verbatim.

Records are emitted flat, one per MD5, with no Rice family migration.
"""
import os
import re
import struct
import zlib
from pathlib import Path

from hires_pack_common import GL_RGBA, GL_RGBA8, GL_TEXFMT_GZ, GL_UNSIGNED_BYTE, JABO_STORAGE, BlobSpool
from hires_pack_loose_png import BLOB_ZLIB_LEVEL, GLIDEN64_MAX_TEXTURE_DIM, _decode_png, load_tasks_to_spool

JABO_IMAGE_EXTENSIONS = ("png", "dds", "jpg", "jpeg")
JABO_NAME_RE = re.compile(r"(?P<md5>[0-9a-f]{32})(?P<suffix>.*)\.(?P<ext>[^./]+)", re.IGNORECASE | re.DOTALL)
PACK_METADATA_NAMES = ("pack.xml", "roms.ini")


def parse_jabo_texture_name(filename: str):
    """Return (md5, None) for a loadable Jabo texture name, else (None, skip reason)."""
    lower = filename.lower()
    if lower in PACK_METADATA_NAMES:
        return None, "pack-metadata"
    if lower == "thumbs.db":
        return None, "thumbs-db"
    match = JABO_NAME_RE.fullmatch(filename)
    if match is None:
        return None, "not-jabo-name"
    if match.group("suffix"):
        return None, "suffixed-copy"
    if match.group("ext").lower() not in JABO_IMAGE_EXTENSIONS:
        return None, "unsupported-image-format"
    return match.group("md5").lower(), None


def is_jabo_texture_name(filename: str) -> bool:
    return parse_jabo_texture_name(filename)[0] is not None


def iter_jabo_pack_files(root: Path):
    """Yield (relative path, skip reason or None) depth-first in case-insensitive order."""
    def walk(directory: Path, relative: Path):
        try:
            names = sorted(os.listdir(directory), key=lambda name: (name.upper(), name))
        except OSError:
            return
        for name in names:
            path = directory / name
            rel = relative / name
            if path.is_symlink():
                yield rel, "symlink-not-followed"
            elif path.is_dir():
                yield from walk(path, rel)
            elif path.is_file():
                yield rel, None

    yield from walk(Path(root), Path())


def _expand565(value):
    red, green, blue = value >> 11 & 31, value >> 5 & 63, value & 31
    return (red << 3 | red >> 2, green << 2 | green >> 4, blue << 3 | blue >> 2)


def _dxt_color_block(block, four_color):
    c0, c1, bits = struct.unpack("<HHI", block)
    p0, p1 = _expand565(c0), _expand565(c1)
    if four_color or c0 > c1:
        p2 = tuple((2 * a + b + 1) // 3 for a, b in zip(p0, p1))
        p3 = tuple((a + 2 * b + 1) // 3 for a, b in zip(p0, p1))
        palette = (p0 + (255,), p1 + (255,), p2 + (255,), p3 + (255,))
    else:
        # DXT1 three-colour mode: index 3 is transparent black.
        p2 = tuple((a + b) // 2 for a, b in zip(p0, p1))
        palette = (p0 + (255,), p1 + (255,), p2 + (255,), (0, 0, 0, 0))
    return [palette[bits >> (2 * i) & 3] for i in range(16)]


def _dxt5_alpha(block):
    a0, a1 = block[0], block[1]
    bits = int.from_bytes(block[2:8], "little")
    if a0 > a1:
        levels = [a0, a1] + [((6 - i) * a0 + (1 + i) * a1 + 3) // 7 for i in range(6)]
    else:
        levels = [a0, a1] + [((4 - i) * a0 + (1 + i) * a1 + 2) // 5 for i in range(4)] + [0, 255]
    return [levels[bits >> (3 * i) & 7] for i in range(16)]


def decode_dds(data: bytes):
    """Decode the top mip of a DXT1/DXT3/DXT5 or uncompressed 32-bit DDS to (width, height, RGBA8 bytes)."""
    if len(data) < 128 or data[:4] != b"DDS " or struct.unpack_from("<I", data, 4)[0] != 124:
        raise ValueError("not a DDS file")
    height, width = struct.unpack_from("<II", data, 12)
    pf_flags, fourcc, bit_count = struct.unpack_from("<I4sI", data, 80)
    masks = struct.unpack_from("<4I", data, 92)
    if width == 0 or height == 0 or width > 16384 or height > 16384:
        raise ValueError(f"unsupported DDS size {width}x{height}")
    offset = 128
    if pf_flags & 0x4:  # DDPF_FOURCC
        if fourcc not in (b"DXT1", b"DXT3", b"DXT5"):
            raise ValueError(f"unsupported DDS fourcc {fourcc!r}")
        block_size = 8 if fourcc == b"DXT1" else 16
        blocks_x, blocks_y = (width + 3) // 4, (height + 3) // 4
        if len(data) < offset + blocks_x * blocks_y * block_size:
            raise ValueError("truncated DDS data")
        out = bytearray(width * height * 4)
        for by in range(blocks_y):
            for bx in range(blocks_x):
                block = data[offset:offset + block_size]
                offset += block_size
                if fourcc == b"DXT1":
                    texels, alpha = _dxt_color_block(block, False), None
                else:
                    texels = _dxt_color_block(block[8:], True)
                    if fourcc == b"DXT3":
                        bits = int.from_bytes(block[:8], "little")
                        alpha = [(bits >> (4 * i) & 15) * 17 for i in range(16)]
                    else:
                        alpha = _dxt5_alpha(block)
                for i in range(16):
                    x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
                    if x < width and y < height:
                        red, green, blue, opacity = texels[i]
                        index = 4 * (y * width + x)
                        out[index:index + 4] = bytes((red, green, blue, opacity if alpha is None else alpha[i]))
    elif pf_flags & 0x40 and bit_count == 32:  # DDPF_RGB
        # Rows may be padded: DDSD_PITCH names the row pitch in bytes.
        flags, pitch = struct.unpack_from("<I", data, 8)[0], struct.unpack_from("<I", data, 20)[0]
        row_bytes = width * 4
        if flags & 0x8 and pitch:
            if pitch < row_bytes:
                raise ValueError(f"DDS row pitch {pitch} below {row_bytes}")
        else:
            pitch = row_bytes
        if len(data) < offset + pitch * (height - 1) + row_bytes:
            raise ValueError("truncated DDS data")
        out = bytearray(width * height * 4)

        def channel(value, mask):
            if not mask:
                return 255
            shift = (mask & -mask).bit_length() - 1
            return ((value & mask) >> shift) * 255 // (mask >> shift)

        has_alpha = bool(pf_flags & 0x1)
        for y in range(height):
            row = offset + y * pitch
            for x, (value,) in enumerate(struct.iter_unpack("<I", data[row:row + row_bytes])):
                i = y * width + x
                out[4 * i:4 * i + 4] = bytes((channel(value, masks[0]), channel(value, masks[1]),
                                              channel(value, masks[2]), channel(value, masks[3]) if has_alpha else 255))
    else:
        raise ValueError("unsupported DDS pixel format")
    return width, height, bytes(out)


def sniff_image_format(head: bytes):
    if head.startswith(b"\x89PNG\r\n\x1a\n"):
        return "png"
    if head.startswith(b"\xff\xd8\xff"):
        return "jpeg"
    if head.startswith(b"DDS "):
        return "dds"
    return None


def decode_jabo_image(path: Path):
    """Decode one texture file by content to (format, width, height, RGBA8 bytes)."""
    data = Path(path).read_bytes()
    content = sniff_image_format(data[:8])
    if content == "dds":
        width, height, rgba = decode_dds(data)
        return content, width, height, rgba
    if content == "png":
        image = _decode_png(path)
    elif content == "jpeg":
        from PIL import Image

        with Image.open(path) as source:
            if source.format != "JPEG":
                raise ValueError("not a JPEG file")
            image = source.convert("RGBA")
    else:
        raise ValueError("unrecognized image content")
    return content, image.size[0], image.size[1], image.tobytes()


def load_jabo_texture(task):
    """Process-pool worker: decode one file to a zlib RGBA8 blob or a failure reason."""
    try:
        content, width, height, rgba = decode_jabo_image(Path(task["path"]))
    except Exception as exc:  # untrusted input: report, never abort the pack
        return {"ok": False, "reason": "decode-failed", "detail": str(exc)}
    return {
        "ok": True,
        "content_format": content,
        "width": width,
        "height": height,
        "blob": zlib.compress(rgba, BLOB_ZLIB_LEVEL),
        "oversize": width > GLIDEN64_MAX_TEXTURE_DIM or height > GLIDEN64_MAX_TEXTURE_DIM,
    }


def scan_jabo_pack(root: Path, workers: int | None = None, spool: BlobSpool | None = None):
    """Load a Jabo pack directory into entries keyed by `jabo_md5` plus a report.

    Blobs are written to `spool` as they are decoded; entries locate them there.
    """
    root = Path(root)
    spool = spool if spool is not None else BlobSpool()
    if not root.is_dir():
        raise ValueError(f"Jabo pack is not a directory: {root}")
    if workers is None:
        workers = os.cpu_count() or 1

    skipped = []
    pack_metadata = []
    candidates = {}  # md5 -> [rel] in traversal order
    file_count = 0
    for rel, skip_reason in iter_jabo_pack_files(root):
        file_count += 1
        if skip_reason is None:
            md5, skip_reason = parse_jabo_texture_name(rel.name)
            if md5 is not None:
                candidates.setdefault(md5, []).append(rel)
                continue
        if skip_reason == "pack-metadata":
            pack_metadata.append({"path": str(rel), "bytes": (root / rel).stat().st_size})
        skipped.append({"path": str(rel), "reason": skip_reason})

    # First loadable candidate wins; a failed load lets the next duplicate try.
    winners = {}
    pending = {md5: 0 for md5 in candidates}
    while pending:
        keys = list(pending)
        tasks = [{"path": str(root / candidates[md5][pending[md5]])} for md5 in keys]
        results = load_tasks_to_spool(tasks, workers, spool, load_jabo_texture)
        next_pending = {}
        for md5, result in zip(keys, results):
            position = pending[md5]
            if result["ok"]:
                winners[md5] = (position, result)
                continue
            skipped.append({"path": str(candidates[md5][position]), "reason": result["reason"], "detail": result["detail"]})
            if position + 1 < len(candidates[md5]):
                next_pending[md5] = position + 1
        pending = next_pending

    entries = []
    oversize = []
    content_counts = {}
    for md5, items in candidates.items():
        if md5 not in winners:
            continue
        position, result = winners[md5]
        rel = items[position]
        for later in items[position + 1:]:
            skipped.append({"path": str(later), "reason": "duplicate-key", "winner": str(rel)})
        if result["oversize"]:
            oversize.append({"path": str(rel), "width": result["width"], "height": result["height"]})
        content_counts[result["content_format"]] = content_counts.get(result["content_format"], 0) + 1
        entries.append({
            "jabo_md5": md5,
            "width": result["width"],
            "height": result["height"],
            "format": GL_RGBA8 | GL_TEXFMT_GZ,
            "texture_format": GL_RGBA,
            "pixel_type": GL_UNSIGNED_BYTE,
            "data_size": result["data_size"],
            "source_path": str(root),
            "source_file": str(rel),
            "content_format": result["content_format"],
            "storage": JABO_STORAGE,
            "inline_blob": False,
            "blob_spool": result["blob_spool"],
            "data_offset": result["data_offset"],
        })
    entries.sort(key=lambda entry: entry["jabo_md5"])

    reason_counts = {}
    for item in skipped:
        reason_counts[item["reason"]] = reason_counts.get(item["reason"], 0) + 1
    report = {
        "root": str(root),
        "file_count": file_count,
        "entry_count": len(entries),
        "content_format_counts": dict(sorted(content_counts.items())),
        "skipped_count": len(skipped),
        "skip_reason_counts": dict(sorted(reason_counts.items())),
        "skipped": skipped,
        "pack_metadata": pack_metadata,
        "oversize_count": len(oversize),
        "oversize": oversize,
    }
    return entries, report


def build_jabo_package_manifest(entries):
    """One runtime-ready record per MD5, each with a single RGBA8 asset (PHRB version 8)."""
    records = []
    for entry in entries:
        name = f"jabo-{entry['jabo_md5']}"
        records.append({
            "policy_key": name,
            "sampled_object_id": name,
            "jabo_md5": entry["jabo_md5"],
            "runtime_ready": True,
            "canonical_identity": {"fmt": 0, "siz": 0, "off": 0, "stride": 0, "wh": "0x0", "formatsize": 0},
            "asset_candidate_count": 1,
            "asset_candidates": [{
                "replacement_id": name,
                "legacy_source_path": entry["source_file"],
                "materialized_path": name,
                "variant_group_id": name,
                "legacy_checksum64": "",
                "legacy_texture_crc": "",
                "legacy_palette_crc": "",
                "legacy_formatsize": 0,
                "width": entry["width"],
                "height": entry["height"],
                "format": entry["format"],
                "texture_format": entry["texture_format"],
                "pixel_type": entry["pixel_type"],
                "data_size": entry["data_size"],
            }],
        })
    return {"record_count": len(records), "records": records}
