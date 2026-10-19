#!/usr/bin/env python3
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
  `_a.bmp`. Matching is case-insensitive, as GLideNHQ does on Windows.
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
import multiprocessing
import os
import re
import struct
import sys
import zlib
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

from hires_pack_common import GL_RGBA, GL_RGBA8, GL_TEXFMT_GZ, GL_UNSIGNED_BYTE, LOOSE_PNG_STORAGE

ALPHA_MODES = ("full", "rice")
GLIDEN64_MAX_TEXTURE_DIM = 4096
BLOB_ZLIB_LEVEL = 6

# GLideNHQ supported_ends, compared case-insensitively.
SUPPORTED_ENDS = ("all.png", "allciByRGBA.png", "ciByRGBA.png", "rgb.png", "rgb.bmp", "a.png", "a.bmp")
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
        if end == "ci.bmp":
            return None, "rice-ci-bmp-unsupported"
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


_BINARY_ALPHA = bytes(0xFF if value == 0xFF else 0 for value in range(256))


def _apply_rice_alpha_fixup(rgba: bytearray, width: int, height: int):
    """GLideNHQ's !LET_TEXARTISTS_FLY path: binary alpha plus opaque-neighbour colour fill."""
    rgba[3::4] = bytes(rgba[3::4]).translate(_BINARY_ALPHA)
    source = bytes(rgba)
    for y in range(height):
        for x in range(width):
            index = y * width + x
            if source[4 * index + 3] == 0xFF:
                continue
            red = green = blue = count = 0
            for dy, dx in ((-1, 0), (-1, -1), (-1, 1), (1, 0), (1, -1), (1, 1), (0, -1), (0, 1)):
                ny, nx = y + dy, x + dx
                if 0 <= ny < height and 0 <= nx < width:
                    neighbour = 4 * (ny * width + nx)
                    if source[neighbour + 3] == 0xFF:
                        red += source[neighbour]
                        green += source[neighbour + 1]
                        blue += source[neighbour + 2]
                        count += 1
            if count:
                rgba[4 * index] = red // count
                rgba[4 * index + 1] = green // count
                rgba[4 * index + 2] = blue // count


def load_rice_texture(task):
    """Load one GLideNHQ texture and return a compressed RGBA8 blob.

    `task` is a dict so it can cross a process boundary. Returns
    {"ok": True, width, height, blob, ...} or {"ok": False, "reason": ...}.
    """
    kind = task["kind"]
    fmt = task["fmt"]
    siz = task["siz"]
    try:
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


def _map_tasks(tasks, workers):
    if workers > 1 and len(tasks) > 1:
        # fork needs no importable __main__, so library callers without a
        # main guard work; other platforms keep their default start method.
        context = multiprocessing.get_context("fork") if sys.platform.startswith("linux") else None
        with ProcessPoolExecutor(max_workers=workers, mp_context=context) as pool:
            return list(pool.map(load_rice_texture, tasks, chunksize=8))
    return [load_rice_texture(task) for task in tasks]


def scan_loose_png_pack(root: Path, rom_name: str | None = None, alpha_mode: str = "full", workers: int | None = None):
    """Load a loose PNG pack directory into cache-shaped entries plus a report."""
    root = Path(root)
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
        results = _map_tasks(tasks, workers)
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
        blob = result["blob"]
        entries.append({
            "checksum64": fields["checksum64"],
            "palette_crc": fields["palette_crc"],
            "texture_crc": fields["texture_crc"],
            "formatsize": fields["formatsize"],
            "width": result["width"],
            "height": result["height"],
            "format": GL_RGBA8 | GL_TEXFMT_GZ,
            "texture_format": GL_RGBA,
            "pixel_type": GL_UNSIGNED_BYTE,
            "is_hires": True,
            "data_size": len(blob),
            "source_path": str(root),
            "source_file": str(rel),
            "wildcard": fields["wildcard"],
            "storage": LOOSE_PNG_STORAGE,
            "inline_blob": True,
            "blob": blob,
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
