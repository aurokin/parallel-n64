# SPDX-License-Identifier: MIT
"""Offline ROM identity, pack pairing and installation; no emulator dependencies."""
import configparser
import json
import os
import re
import shutil
import tempfile
import xml.etree.ElementTree as ET
import zipfile
import zlib
from pathlib import Path

from hires_pack_common import LOOSE_PNG_STORAGE, resolve_legacy_cache_path
from hires_pack_loose_png import infer_rom_name, iter_pack_files, port_specific_summary

RDB_KEY_RE = re.compile(r"(?<![0-9A-Za-z])([0-9a-f]{8})-([0-9a-f]{8})-C:([0-9a-f]{1,2})(?![0-9A-Za-z])", re.IGNORECASE)


def hires_game_ident(header: bytes):
    """Apply the core's MIT ident rule to a canonical 64-byte ROM header."""
    name = header[0x20:0x34].split(b"\0", 1)[0].rstrip(b" ").replace(b":", b"-").replace(b"/", b"-")
    code = header[0x3B:0x3F]
    if name and all(0x20 <= c <= 0x7E and c not in b'<>"\\|?*' for c in name) and code not in (b"CPSJ", b"CP2J"):
        return name.decode("ascii")
    return code.decode("ascii") if len(code) == 4 and all(0x20 <= c <= 0x7E for c in code) else None


def read_rom_header(path: Path):
    """Read only the header, or the first .z64/.v64/.n64 member in ZIP order."""
    path = Path(path)
    if path.suffix.lower() == ".zip":
        try:
            with zipfile.ZipFile(path) as archive:
                member = next((m for m in archive.infolist() if not m.is_dir() and
                               Path(m.filename).suffix.lower() in (".z64", ".v64", ".n64")), None)
                if member is None:
                    raise ValueError(f"No .z64/.v64/.n64 ROM member found in {path}.")
                with archive.open(member) as fp:
                    header = fp.read(64)
        except (zipfile.BadZipFile, RuntimeError, NotImplementedError, EOFError, zlib.error) as exc:
            raise ValueError(f"Cannot read ZIP ROM {path}: {exc}") from exc
    else:
        with path.open("rb") as fp:
            header = fp.read(64)
    if len(header) != 64:
        raise ValueError(f"ROM header is truncated (need 64 bytes): {path}")
    if header[:4] == b"\x37\x80\x40\x12":
        header = bytes(header[i ^ 1] for i in range(64))
    elif header[:4] == b"\x40\x12\x37\x80":
        header = bytes(header[i ^ 3] for i in range(64))
    elif header[:4] != b"\x80\x37\x12\x40":
        raise ValueError(f"Unrecognized N64 ROM byte order: {path}")
    return header


def parse_rdb_keys(text: str):
    return sorted({f"{a.upper()}-{b.upper()}-C:{int(c, 16):02X}" for a, b, c in RDB_KEY_RE.findall(text)})


def read_source_allowlist(root: Path):
    """Read Jabo's supported/rdb XML nodes and [Supported] CRC INI list.

    Prefer metadata inside the source directory, then its immediate parent.
    Never infer support from descriptions, comments or unrelated INI sections.
    """
    keys = set()
    paths = [root / rel for rel, skipped in iter_pack_files(root)
             if not skipped and rel.name.lower() in ("pack.xml", "roms.ini")]
    if not paths:
        paths = [p for p in root.parent.iterdir() if p.is_file() and p.name.lower() in ("pack.xml", "roms.ini")]
    for path in paths:
        try:
            if path.name.lower() == "pack.xml":
                document = ET.parse(path)
                values = [node.text or "" for node in document.findall("./supported/rdb")]
                if document.find("./supported") is not None and not values:
                    raise ValueError(f"Supported ROM list in {path} contains no valid RDB keys.")
            else:
                document = configparser.ConfigParser(interpolation=None)
                document.read_string(path.read_text(encoding="utf-8-sig"))
                values = [document.get(section, "crc", fallback="") for section in document.sections()
                          if section.lower() == "supported"]
            parsed = parse_rdb_keys("\n".join(values))
            if values and not parsed:
                raise ValueError(f"Supported ROM list in {path} contains no valid RDB keys.")
            keys.update(parsed)
        except (ET.ParseError, configparser.Error, UnicodeError) as exc:
            raise ValueError(f"Cannot read supported ROM list in {path}: {exc}") from exc
    return sorted(keys)


def check_pairing(source_label, source_allowlist, header, force=False):
    ident = hires_game_ident(header)
    if ident is None:
        raise ValueError("This ROM has no usable name or game code in its header; cannot install a pack.")
    key = f"{header[0x10:0x14].hex().upper()}-{header[0x14:0x18].hex().upper()}-C:{header[0x3E]:02X}"
    if source_allowlist:
        if key in source_allowlist:
            return "allowlist"
        reason = f'This pack is labelled "{source_label}"; your ROM is "{ident}" ({key}), which is not in its supported ROM list.'
    else:
        if source_label is not None and source_label.isascii() and source_label.lower() == ident.lower():
            return "exact"
        reason = f'This pack is labelled "{source_label or "(unknown)"}"; your ROM is "{ident}".'
    if force:
        return "forced"
    raise ValueError(reason + " If this pack is for this game, rerun with --force-target.")


def _filename_component(value, option):
    # The core ignores dotfiles, and header fallback codes can contain path
    # separators even though they satisfy the shared printable-ASCII rule.
    if not value or value.startswith(".") or any(ord(c) < 32 or c in '<>:"/\\|?*' for c in value):
        raise ValueError(f"{option} is not a usable install filename: {value!r}")
    return value


def prepare_install(args):
    """Validate pairing and destination before any texture decoding/conversion."""
    if not args.install:
        if args.rom or args.name or args.force_target or args.replace:
            raise ValueError("--rom, --name, --force-target and --replace require --install.")
        return None
    if not args.rom:
        raise ValueError("--install requires --rom <file>.")
    resolution = resolve_legacy_cache_path(Path(args.cache))
    source = Path(resolution["resolved_path"])
    storage = resolution["resolved_storage"]
    if storage == LOOSE_PNG_STORAGE:
        label, _counts = infer_rom_name(source)
        if not label:
            note = port_specific_summary(rel for rel, skip in iter_pack_files(source) if not skip)
            raise ValueError(f"No Rice-named textures found under {source}. {note}".rstrip())
    else:
        label = source.name if source.is_dir() else re.sub(r"_HIRESTEXTURES$", "", source.stem, flags=re.IGNORECASE)
    source_root = Path(args.cache)
    allowlist = read_source_allowlist(source_root) if source_root.is_dir() else []
    header = read_rom_header(Path(args.rom))
    method = check_pairing(label, allowlist, header, args.force_target)
    ident = _filename_component(hires_game_ident(header), "ROM identity")
    name = _filename_component(args.name or source.stem, "--name")
    if not name.lower().endswith(".phrb"):
        name += ".phrb"
    target = Path(args.install).resolve() / "parallel-n64" / "hires" / ident / name
    if os.path.lexists(target) and not args.replace:
        raise ValueError(f"Pack already exists: {target}. Rerun with --replace to overwrite it.")
    hires = target.parent.parent
    entries = list(hires.iterdir()) if hires.is_dir() else []
    others = sorted(p.name for p in entries if p.is_file() and p.name.lower() == f"{ident}.phrb".lower())
    others += sorted(str(p.relative_to(hires)) for folder in entries
                     if folder.is_dir() and folder.name.lower() == ident.lower()
                     for p in folder.iterdir() if p.is_file() and not p.name.startswith(".") and
                     p.suffix.lower() == ".phrb" and p != target)
    return {
        "source_label": label,
        "source_allowlist": allowlist,
        "source_path": str(source.resolve()),
        "rom": {"path": str(Path(args.rom).resolve()),
                "name": header[0x20:0x34].split(b"\0", 1)[0].rstrip(b" ").decode("ascii", errors="backslashreplace"),
                "code": header[0x3B:0x3F].decode("ascii", errors="backslashreplace"),
                "rev": header[0x3F], "crc1": header[0x10:0x14].hex().upper(),
                "crc2": header[0x14:0x18].hex().upper()},
        "ident": ident, "target_path": str(target), "target_check": method,
        "forced": method == "forced", "other_packages": others,
    }


def finish_install(report, install, replace=False):
    """Publish a completed package atomically, preserving an existing target by default."""
    if report["gate_failures"]:
        return
    target = Path(install["target_path"])
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=target.parent, prefix=".hts2phrb-", delete=False) as fp:
        temporary = Path(fp.name)
    try:
        shutil.copyfile(report["binary_package"]["output_path"], temporary)
        if replace:
            os.replace(temporary, target)
        else:
            try:
                os.link(temporary, target)
            except FileExistsError as exc:
                raise ValueError(f"Pack already exists: {target}. Rerun with --replace to overwrite it.") from exc
    finally:
        temporary.unlink(missing_ok=True)
    report["install"] = install
    Path(report["report_path"]).write_text(json.dumps(report, indent=2) + "\n")
    with Path(report["summary_path"]).open("a") as fp:
        fp.write(f"\nInstalled: `{target}` (target check: {install['target_check']}).\n")
