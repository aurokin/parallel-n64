#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Load a loose Rice-named PNG texture pack the way GLideNHQ does.

GLideNHQ (GLideN64 src/GLideNHQ/TxHiResCache.cpp, TxHiResLoader.cpp) loads
`<texture pack path>/<ROM name>/` recursively when building a `.hts`/`.htc`
cache. This module reproduces that load offline and returns cache-shaped
entries (see hires_pack_common.parse_htc_entries) so the rest of the
converter treats a PNG directory exactly like the cache GLideNHQ would have
written from it:

- File names must be exactly `<ROM>#<CRC>#<fmt>#<siz>[#<PALCRC>]_<suffix>`
  with 8 uppercase-or-lowercase hex digits per CRC and one hex digit each for
  fmt and siz, or one of the `$` wildcard forms `<ROM>#<CRC>#<fmt>#<siz>#$_`
  and `<ROM>#$#<fmt>#<siz>#<PALCRC>_`. Accepted suffixes are `_all.png`,
  `_allciByRGBA.png`, `_ciByRGBA.png`, `_rgb.png`, `_rgb.bmp`, `_a.png` and
  `_a.bmp` and `_ci.bmp`. Matching is case-insensitive, as GLideNHQ does on Windows.
- The key is `PALCRC << 32 | CRC`, or `PALCRC` alone for the `#$#` form; the
  formatsize is `fmt | siz << 8` (N64FormatSize).
- Entries and hidden (`.`) or `~!~`-prefixed names are skipped; directories
  recurse in place. The first loadable file for a (key, formatsize) pair wins.
  Traversal is depth-first in case-insensitive name order, the order NTFS
  returns; GLideNHQ on other filesystems uses unspecified readdir order.
- `_rgb` + `_a` pairs take alpha from the red channel of `_a`; `_rgb` alone is
  opaque; `_a` alone is rejected.
- fmt 4 (I) textures with no alpha below 0xFE get alpha = (R + G + B) / 3.
- `alpha_mode="rice"` additionally applies GLideNHQ's legacy Rice alpha
  rounding and border fill to fmt 0/siz 2 and fmt 2 textures, which GLideN64
  does when txHiresFullAlphaChannel is off (the libretro default).

Known deliberate differences from current GLideNHQ are listed in
assets/TEXTURE_PACKS.md.
"""
import itertools
import multiprocessing
import os
import re
import struct
import sys
import zlib
from concurrent.futures import FIRST_COMPLETED, ProcessPoolExecutor, wait
from pathlib import Path

from hires_pack_common import GL_COLOR_INDEX, GL_COLOR_INDEX8_EXT, GL_RGBA, GL_RGBA8, GL_TEXFMT_GZ, GL_UNSIGNED_BYTE, LOOSE_PNG_STORAGE, BlobSpool

ALPHA_MODES = ("full", "rice")
GLIDEN64_MAX_TEXTURE_DIM = 4096
BLOB_ZLIB_LEVEL = 6

# GLideNHQ supported_ends, compared case-insensitively.
SUPPORTED_ENDS = ("all.png", "allciByRGBA.png", "ciByRGBA.png", "rgb.png", "rgb.bmp", "a.png", "a.bmp", "ci.bmp")
_SUPPORTED_ENDS_LOWER = {end.lower() for end in SUPPORTED_ENDS}
_IMAGE_SUFFIXES = (".png", ".bmp")

_NAME_TAIL_RE = re.compile(
    r"#(?:"
    r"(?P<crc>[0-9a-f]{8})#(?P<fmt>[0-9a-f])#(?P<siz>[0-9a-f])(?:#(?P<pal>[0-9a-f]{8}|\$))?"
    r"|\$#(?P<wfmt>[0-9a-f])#(?P<wsiz>[0-9a-f])#(?P<wpal>[0-9a-f]{8})"
    r")_(?P<end>[^.]+)\.(?P<ext>[^.]+)",
    re.IGNORECASE,
)
_IDENT_PREFIX_RE = re.compile(r"^(?P<ident>[^#]*)#")


def is_skipped_name(name: str) -> bool:
    """GLideNHQ checkFolderName: hidden and `~!~`-prefixed names are ignored."""
    return name.startswith(".") or name.startswith("~!~")


def parse_rice_texture_name(filename: str, ident: str):
    """Parse one file name with GLideNHQ checkFileName rules.

    Returns (fields, None) on success or (None, reason) when GLideNHQ would
    skip the file.
    """
    if not filename.lower().endswith(_IMAGE_SUFFIXES):
        return None, "not-png-or-bmp"
    if not filename.lower().startswith(ident.lower()) or not ident:
        return None, "rom-name-mismatch"
    match = _NAME_TAIL_RE.fullmatch(filename[len(ident):])
    if match is None:
        return None, "not-rice-naming"
    end = f"{match.group('end')}.{match.group('ext')}".lower()
    if end not in _SUPPORTED_ENDS_LOWER:
        return None, "unsupported-suffix"

    if match.group("crc") is not None:
        texture_crc = int(match.group("crc"), 16)
        fmt = int(match.group("fmt"), 16)
        siz = int(match.group("siz"), 16)
        pal = match.group("pal")
        if pal == "$":
            wildcard = "palette"
            palette_crc = 0
        else:
            wildcard = None
            palette_crc = int(pal, 16) if pal is not None else 0
    else:
        wildcard = "texture"
        texture_crc = 0
        fmt = int(match.group("wfmt"), 16)
        siz = int(match.group("wsiz"), 16)
        palette_crc = int(match.group("wpal"), 16)

    if end == "ci.bmp" and (fmt != 2 or siz not in (0, 1)):
        return None, "invalid-ci-formatsize"
    if texture_crc == 0 and wildcard is None:
        return None, "zero-crc"
    # TxHiResCache composes the key this way; a `#$#` name keys on PALCRC alone.
    checksum64 = ((palette_crc << 32) | texture_crc) if texture_crc else palette_crc
    if checksum64 == 0:
        # TxCache::add rejects a zero key, which aborts GLideNHQ's whole load.
        return None, "zero-checksum"
    kind = end.split(".", 1)[0]
    return {
        "checksum64": checksum64,
        "texture_crc": checksum64 & 0xFFFFFFFF,
        "palette_crc": (checksum64 >> 32) & 0xFFFFFFFF,
        "name_texture_crc": texture_crc,
        "name_palette_crc": palette_crc,
        "fmt": fmt,
        "siz": siz,
        "formatsize": (fmt & 0xFF) | ((siz & 0xFF) << 8),
        "kind": kind.lower(),
        "ext": match.group("ext").lower(),
        "wildcard": wildcard,
    }, None


def iter_pack_files(root: Path):
    """Yield (relative path, skipped) in GLideNHQ traversal order.

    Symlinks are not followed: packs are untrusted archives.
    """
    def walk(directory: Path, relative: Path):
        try:
            names = sorted(os.listdir(directory), key=lambda name: (name.upper(), name))
        except OSError:
            return
        for name in names:
            path = directory / name
            rel = relative / name
            if is_skipped_name(name):
                yield rel, "hidden-or-skip-prefix"
                continue
            if path.is_symlink():
                yield rel, "symlink-not-followed"
                continue
            if path.is_dir():
                yield from walk(path, rel)
            elif path.is_file():
                yield rel, None

    yield from walk(root, Path())


def infer_rom_name(root: Path):
    """Pick the ROM name GLideNHQ would need for most image names in the pack."""
    counts = {}
    for rel, skipped in iter_pack_files(root):
        if skipped or not rel.name.lower().endswith(_IMAGE_SUFFIXES):
            continue
        match = _IDENT_PREFIX_RE.match(rel.name)
        if match and match.group("ident"):
            ident = match.group("ident")
            key = ident.upper()
            bucket = counts.setdefault(key, {"count": 0, "spellings": {}})
            bucket["count"] += 1
            bucket["spellings"][ident] = bucket["spellings"].get(ident, 0) + 1
    if not counts:
        return None, {}
    best_key = max(counts, key=lambda key: (counts[key]["count"], key == root.name.upper(), key))
    spellings = counts[best_key]["spellings"]
    best = max(spellings, key=lambda spelling: (spellings[spelling], spelling))
    return best, {key: value["count"] for key, value in counts.items()}


def _decode_png(path: Path):
    """Decode like TxImage::readPNG: always RGBA8, 16-bit channels keep the high byte."""
    from PIL import Image

    with Image.open(path) as image:
        if image.format != "PNG":
            raise ValueError("not a PNG file")
        image.load()
        # 16-bit grayscale: libpng png_set_strip_16 keeps the high byte, while
        # Pillow's own conversion would clamp. A tRNS key names a full 16-bit
        # sample, so transparency is decided before the reduction.
        if image.mode in ("I;16", "I;16L", "I;16B", "I"):
            data = image.tobytes()
            if image.mode == "I":
                samples = [min(max(value, 0), 0xFFFF) for value in image.getdata()]
            else:
                order = ">" if image.mode == "I;16B" else "<"
                samples = struct.unpack(f"{order}{len(data) // 2}H", data)
            gray = Image.frombytes("L", image.size, bytes(sample >> 8 for sample in samples))
            key = image.info.get("transparency")
            if not isinstance(key, int):
                return gray.convert("RGBA")
            alpha = Image.frombytes("L", image.size, bytes(0 if sample == key else 0xFF for sample in samples))
            return Image.merge("RGBA", (gray, gray, gray, alpha))
        return image.convert("RGBA")


def _decode_bmp(path: Path):
    """Decode a 24/32-bit uncompressed BMP; palette BMPs are rejected."""
    from PIL import Image

    with Image.open(path) as image:
        if image.format != "BMP":
            raise ValueError("not a BMP file")
        if int(image.info.get("compression", 0) or 0) != 0:
            raise ValueError("compressed BMP")
        if image.mode not in ("RGB", "RGBA"):
            raise ValueError("palette or grayscale BMP is not 32-bit color")
        image.load()
        return image.convert("RGBA")


def _decode_ci_bmp(path: Path, siz: int):
    """Read packed Rice indices directly, ignoring the BMP preview colour table."""
    with path.open("rb") as source:
        header = source.read(54)
        if len(header) < 54 or header[:2] != b"BM" or struct.unpack_from("<I", header, 14)[0] < 40:
            raise ValueError("unsupported indexed BMP header")
        width, signed_height, planes, bits, compression = struct.unpack_from("<iiHHI", header, 18)
        if planes != 1 or bits not in (4, 8) or compression != 0 or width <= 0 or signed_height == 0:
            raise ValueError("expected an uncompressed 4- or 8-bit BMP")
        height = abs(signed_height)
        stride = ((width * bits + 31) // 32) * 4
        offset = struct.unpack_from("<I", header, 10)[0]
        if offset < 14 + struct.unpack_from("<I", header, 14)[0]:
            raise ValueError("invalid indexed BMP pixel offset")
        if offset + stride * height > path.stat().st_size:
            raise ValueError("truncated indexed BMP")
        source.seek(offset)
        rows = []
        for _ in range(height):
            row = source.read(stride)
            if len(row) != stride:
                raise ValueError("truncated indexed BMP row")
            if bits == 4:
                row = bytes((row[x >> 1] >> (4 if x % 2 == 0 else 0)) & 15 for x in range(width))
            rows.append(row[:width])
        if signed_height > 0:
            rows.reverse()
        indices = b"".join(rows)
        if siz == 0 and max(indices, default=0) > 15:
            raise ValueError("CI4 index exceeds 15")
        return (width, height), indices


def _decode_image(path: Path):
    if path.suffix.lower() == ".bmp":
        return _decode_bmp(path)
    return _decode_png(path)


def _try_decode(paths):
    for path in paths:
        if path is None:
            continue
        try:
            return _decode_image(path)
        except Exception:
            continue
    return None


def _apply_rice_alpha_fixup(rgba: bytearray, width: int, height: int):
    """Binarize alpha and borrow RGB from originally opaque adjacent pixels."""
    original_alpha = rgba[3::4]
    for pixel, alpha in enumerate(original_alpha):
        if alpha == 255:
            continue

        offset = 4 * pixel
        rgba[offset + 3] = 0
        row, column = divmod(pixel, width)
        left = max(0, column - 1)
        right = min(width, column + 2)
        red = green = blue = count = 0
        for adjacent_row in range(max(0, row - 1), min(height, row + 2)):
            row_start = adjacent_row * width
            for adjacent in range(row_start + left, row_start + right):
                if original_alpha[adjacent] != 255:
                    continue
                # Contributors keep their RGB, so earlier writes cannot propagate.
                source = 4 * adjacent
                red += rgba[source]
                green += rgba[source + 1]
                blue += rgba[source + 2]
                count += 1
        if count:
            rgba[offset:offset + 3] = bytes((red // count, green // count, blue // count))


def load_rice_texture(task):
    """Load one texture and return compressed RGBA8 or Rice indices.

    `task` is a dict so it can cross a process boundary. Returns
    {"ok": True, width, height, blob, ...} or {"ok": False, "reason": ...}.
    """
    kind = task["kind"]
    fmt = task["fmt"]
    siz = task["siz"]
    try:
        if kind == "ci":
            (width, height), indices = _decode_ci_bmp(Path(task["path"]), siz)
            if width * height < 4:
                return {"ok": False, "reason": "too-small"}
            return {
                "ok": True, "width": width, "height": height,
                "blob": zlib.compress(indices, BLOB_ZLIB_LEVEL),
                "oversize": width > GLIDEN64_MAX_TEXTURE_DIM or height > GLIDEN64_MAX_TEXTURE_DIM,
            }
        if kind in ("rgb", "a"):
            rgb_paths = [Path(path) for path in task["rgb_paths"]]
            alpha_paths = [Path(path) for path in task["alpha_paths"]]
            if not rgb_paths:
                return {"ok": False, "reason": "alpha-without-rgb"}
            alpha_image = _try_decode(alpha_paths)
            image = _try_decode(rgb_paths)
            if image is None:
                return {"ok": False, "reason": "decode-failed"}
            if alpha_image is not None:
                if alpha_image.size != image.size:
                    return {"ok": False, "reason": "rgb-alpha-size-mismatch"}
                image.putalpha(alpha_image.getchannel("R"))
            else:
                image.putalpha(0xFF)
        else:
            image = _try_decode([Path(task["path"])])
            if image is None:
                return {"ok": False, "reason": "decode-failed"}
    except Exception as exc:  # untrusted input: report, never abort the pack
        return {"ok": False, "reason": "decode-failed", "detail": str(exc)}

    width, height = image.size
    if width * height < 4:
        return {"ok": False, "reason": "too-small"}

    rgba = bytearray(image.tobytes())
    if task["alpha_mode"] == "rice" and ((fmt == 0 and siz == 2) or fmt == 2):
        _apply_rice_alpha_fixup(rgba, width, height)
    if fmt == 4:
        alpha_min = min(rgba[3::4])
        if alpha_min >= 0xFE:
            # Rice I format: I = (R + G + B) / 3 becomes the alpha channel.
            rgba[3::4] = bytes(
                (red + green + blue) // 3
                for red, green, blue in zip(rgba[0::4], rgba[1::4], rgba[2::4])
            )
    return {
        "ok": True,
        "width": width,
        "height": height,
        "blob": zlib.compress(bytes(rgba), BLOB_ZLIB_LEVEL),
        "oversize": width > GLIDEN64_MAX_TEXTURE_DIM or height > GLIDEN64_MAX_TEXTURE_DIM,
    }


def _pair_paths(root: Path, rel: Path, listing_for):
    """Resolve the `_rgb`/`_a` partner files in the same directory, PNG before BMP."""
    name = rel.name
    stem = name[: name.rfind("_")]
    directory = rel.parent
    lookup = listing_for(directory)

    def find(suffixes):
        found = []
        for suffix in suffixes:
            real = lookup.get((stem + suffix).lower())
            path = root / directory / real if real is not None else None
            # Same rule as iter_pack_files: never follow a symlink out of the pack.
            if path is not None and not path.is_symlink() and path.is_file():
                found.append(str(path))
        return found

    return find(("_rgb.png", "_rgb.bmp")), find(("_a.png", "_a.bmp"))


def _imap_tasks_unordered(tasks, workers, loader=load_rice_texture):
    """Yield (index, loader(task)) for each task as it completes.

    At most two tasks per worker are queued or finished but unconsumed, so
    decoded textures never accumulate beyond that window whatever the pack
    size, and a slow texture does not stall the other workers.
    """
    if workers <= 1 or len(tasks) <= 1:
        for index, task in enumerate(tasks):
            yield index, loader(task)
        return
    # fork needs no importable __main__, so library callers without a
    # main guard work; other platforms keep their default start method.
    context = multiprocessing.get_context("fork") if sys.platform.startswith("linux") else None
    task_iter = enumerate(tasks)
    with ProcessPoolExecutor(max_workers=workers, mp_context=context) as pool:
        def submit(item):
            index, task = item
            return pool.submit(loader, task), index

        in_flight = dict(submit(item) for item in itertools.islice(task_iter, 2 * workers))
        while in_flight:
            done, _ = wait(in_flight, return_when=FIRST_COMPLETED)
            for future in done:
                index = in_flight.pop(future)
                next_item = next(task_iter, None)
                if next_item is not None:
                    future_next, next_index = submit(next_item)
                    in_flight[future_next] = next_index
                yield index, future.result()


def load_tasks_to_spool(tasks, workers, spool, loader=load_rice_texture):
    """Run `loader` over `tasks`, moving each blob into `spool`; returns results in task order."""
    results = [None] * len(tasks)
    for index, result in _imap_tasks_unordered(tasks, workers, loader):
        if result["ok"]:
            result.update(spool.entry_location(result.pop("blob")))
        results[index] = result
    return results


def scan_loose_png_pack(root: Path, rom_name: str | None = None, alpha_mode: str = "full", workers: int | None = None,
                        spool: BlobSpool | None = None):
    """Load a loose PNG pack directory into cache-shaped entries plus a report.

    Each texture's zlib RGBA8 blob is written to `spool` as it is decoded;
    entries locate it there (read it with hires_pack_common.read_entry_blob).
    """
    root = Path(root)
    spool = spool if spool is not None else BlobSpool()
    if alpha_mode not in ALPHA_MODES:
        raise ValueError(f"Unsupported loose PNG alpha mode: {alpha_mode}")
    if not root.is_dir():
        raise ValueError(f"Loose PNG pack is not a directory: {root}")
    inferred_rom_name, rom_name_counts = infer_rom_name(root)
    ident = rom_name or inferred_rom_name
    if not ident:
        raise ValueError(f"No Rice-named textures (<ROM>#<CRC>#...) found under {root}; pass --rom-name.")
    if workers is None:
        workers = os.cpu_count() or 1

    directory_listings = {}

    def listing_for(directory: Path):
        if directory not in directory_listings:
            try:
                names = sorted(os.listdir(root / directory))
            except OSError:
                names = []
            directory_listings[directory] = {name.lower(): name for name in reversed(names)}
        return directory_listings[directory]

    skipped = []
    candidates = {}  # (checksum64, formatsize) -> [(rel, fields)] in traversal order
    file_count = 0
    image_count = 0
    for rel, skip_reason in iter_pack_files(root):
        file_count += 1
        is_image = rel.name.lower().endswith(_IMAGE_SUFFIXES)
        image_count += int(is_image)
        if skip_reason:
            skipped.append({"path": str(rel), "reason": skip_reason})
            continue
        fields, reason = parse_rice_texture_name(rel.name, ident)
        if fields is None:
            skipped.append({"path": str(rel), "reason": reason})
            continue
        candidates.setdefault((fields["checksum64"], fields["formatsize"]), []).append((rel, fields))

    def make_task(rel, fields):
        task = {
            "path": str(root / rel),
            "kind": fields["kind"],
            "fmt": fields["fmt"],
            "siz": fields["siz"],
            "alpha_mode": alpha_mode,
        }
        if fields["kind"] in ("rgb", "a"):
            task["rgb_paths"], task["alpha_paths"] = _pair_paths(root, rel, listing_for)
        return task

    # First loadable candidate wins; a failed load lets the next duplicate try.
    winners = {}
    pending = {key: 0 for key in candidates}
    while pending:
        keys = list(pending)
        tasks = [make_task(*candidates[key][pending[key]]) for key in keys]
        results = load_tasks_to_spool(tasks, workers, spool)
        next_pending = {}
        for key, result in zip(keys, results):
            position = pending[key]
            rel, fields = candidates[key][position]
            if result["ok"]:
                winners[key] = (position, result)
                continue
            skip = {"path": str(rel), "reason": result["reason"]}
            if result.get("detail"):
                skip["detail"] = result["detail"]
            skipped.append(skip)
            if position + 1 < len(candidates[key]):
                next_pending[key] = position + 1
        pending = next_pending

    entries = []
    oversize = []
    for key, items in candidates.items():
        winner = winners.get(key)
        if winner is None:
            continue
        position, result = winner
        rel, fields = items[position]
        for later_rel, later_fields in items[position + 1:]:
            paired = fields["kind"] in ("rgb", "a") and later_fields["kind"] in ("rgb", "a") and later_rel.parent == rel.parent \
                and later_rel.name[: later_rel.name.rfind("_")].lower() == rel.name[: rel.name.rfind("_")].lower()
            skipped.append({
                "path": str(later_rel),
                "reason": "rgb-alpha-pair-member" if paired else "duplicate-key",
                "winner": str(rel),
            })
        if result["oversize"]:
            oversize.append({"path": str(rel), "width": result["width"], "height": result["height"]})
        entries.append({
            "checksum64": fields["checksum64"],
            "palette_crc": fields["palette_crc"],
            "texture_crc": fields["texture_crc"],
            "formatsize": fields["formatsize"],
            "width": result["width"],
            "height": result["height"],
            "format": (GL_COLOR_INDEX8_EXT if fields["kind"] == "ci" else GL_RGBA8) | GL_TEXFMT_GZ,
            "texture_format": GL_COLOR_INDEX if fields["kind"] == "ci" else GL_RGBA,
            "pixel_type": GL_UNSIGNED_BYTE,
            "is_hires": True,
            "data_size": result["data_size"],
            "source_path": str(root),
            "source_file": str(rel),
            "wildcard": fields["wildcard"],
            "storage": LOOSE_PNG_STORAGE,
            "inline_blob": False,
            "blob_spool": result["blob_spool"],
            "data_offset": result["data_offset"],
        })

    reason_counts = {}
    for item in skipped:
        reason_counts[item["reason"]] = reason_counts.get(item["reason"], 0) + 1
    report = {
        "root": str(root),
        "rom_name": ident,
        "rom_name_source": "argument" if rom_name else "inferred",
        "rom_name_prefix_counts": rom_name_counts,
        "alpha_mode": alpha_mode,
        "file_count": file_count,
        "image_file_count": image_count,
        "entry_count": len(entries),
        "wildcard_entry_count": sum(1 for entry in entries if entry["wildcard"]),
        "skipped_count": len(skipped),
        "skip_reason_counts": dict(sorted(reason_counts.items())),
        "skipped": skipped,
        "oversize_count": len(oversize),
        "oversize": oversize,
    }
    return entries, report
