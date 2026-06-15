#!/usr/bin/env python3
"""Promote an interactive RetroArch scratch savestate into a durable bundle."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_env(path: Path) -> dict[str, str]:
    data: dict[str, str] = {}
    if not path.exists():
        return data
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        data[key] = value
    return data


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def newest_file(root: Path) -> Path | None:
    files = [p for p in root.rglob("*") if p.is_file()]
    if not files:
        return None
    return max(files, key=lambda p: (p.stat().st_mtime_ns, p.name))


def state_suffix(slot: int) -> str:
    return "" if slot == 0 else str(slot)


def rom_stem(session: dict[str, str], fallback: str = "Paper Mario (USA)") -> str:
    rom_path = session.get("ROM_PATH", "")
    if rom_path:
        return Path(rom_path).stem
    return fallback


def find_state(bundle_dir: Path, session: dict[str, str], slot: int) -> Path:
    suffix = state_suffix(slot)
    expected = (
        bundle_dir
        / "states"
        / "ParaLLEl N64"
        / f"{rom_stem(session)}.state{suffix}"
    )
    if expected.exists():
        return expected

    matches = sorted((bundle_dir / "states").rglob(f"*.state{suffix}"))
    if len(matches) == 1:
        return matches[0]
    if not matches:
        raise FileNotFoundError(f"No state found for slot {slot} under {bundle_dir / 'states'}")
    raise RuntimeError(f"Multiple candidate states for slot {slot}: {matches}")


def optional_hash(path_value: str) -> str:
    if not path_value:
        return ""
    path = Path(path_value)
    if not path.is_file():
        return ""
    return sha256_file(path)


def copy_if_file(src_value: str, dest_dir: Path) -> dict[str, str] | None:
    if not src_value:
        return None
    src = Path(src_value)
    if not src.is_file():
        return None
    dest_dir.mkdir(parents=True, exist_ok=True)
    dest = dest_dir / src.name
    shutil.copy2(src, dest)
    return {
        "source_path": str(src),
        "bundle_path": str(dest),
        "sha256": sha256_file(dest),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Promote a numbered interactive savestate slot into a named durable state bundle."
    )
    parser.add_argument("--bundle-dir", required=True, type=Path)
    parser.add_argument("--slot", required=True, type=int, choices=range(0, 10))
    parser.add_argument("--id", required=True)
    parser.add_argument("--output-root", type=Path, default=repo_root() / "artifacts" / "durable-states")
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--parent-id", default="")
    parser.add_argument("--scene-map", default="")
    parser.add_argument("--scene-entry", default="")
    parser.add_argument("--settle-frames", type=int, default=3)
    parser.add_argument("--notes", default="")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    if not re.fullmatch(r"[A-Za-z0-9_.-]+", args.id):
        raise SystemExit("--id may only contain letters, digits, '.', '_', and '-'")

    bundle_dir = args.bundle_dir.resolve()
    session_env = bundle_dir / "retroarch.session.env"
    session = parse_env(session_env)
    state_src = find_state(bundle_dir, session, args.slot)

    capture = args.capture
    if capture is None:
        capture = newest_file(bundle_dir / "captures")
    if capture is None or not capture.is_file():
        raise SystemExit("No capture found. Take a screenshot first or pass --capture.")

    output_dir = (args.output_root / args.id).resolve()
    if output_dir.exists() and not args.force:
        raise SystemExit(f"Output already exists: {output_dir} (use --force to replace)")
    if output_dir.exists():
        shutil.rmtree(output_dir)

    state_dir = output_dir / "state" / "ParaLLEl N64"
    capture_dir = output_dir / "captures"
    log_dir = output_dir / "logs"
    config_dir = output_dir / "config"
    savefile_dir = output_dir / "savefiles"
    state_dir.mkdir(parents=True, exist_ok=True)
    capture_dir.mkdir(parents=True, exist_ok=True)
    log_dir.mkdir(parents=True, exist_ok=True)
    config_dir.mkdir(parents=True, exist_ok=True)

    normalized_state = state_dir / f"{rom_stem(session)}.state"
    shutil.copy2(state_src, normalized_state)

    promoted_capture = capture_dir / capture.name
    shutil.copy2(capture, promoted_capture)

    copied_configs = []
    for key in ("BASE_CONFIG", "APPEND_CONFIG", "CORE_OPTIONS_FILE", "CORE_OPTIONS_LAUNCH_FILE"):
        copied = copy_if_file(session.get(key, ""), config_dir)
        if copied:
            copied["session_key"] = key
            copied_configs.append(copied)

    copied_savefiles = []
    copied_savefile = copy_if_file(session.get("SAVEFILE_SOURCE", ""), savefile_dir)
    if copied_savefile:
        copied_savefile["session_key"] = "SAVEFILE_SOURCE"
        copied_savefiles.append(copied_savefile)

    copied_logs = []
    for candidate in [
        bundle_dir / "logs" / "retroarch.log",
        bundle_dir / "logs" / "interactive.commands.log",
        bundle_dir / "retroarch.session.env",
    ]:
        if candidate.is_file():
            dest = log_dir / candidate.name
            shutil.copy2(candidate, dest)
            copied_logs.append(
                {
                    "source_path": str(candidate),
                    "bundle_path": str(dest),
                    "sha256": sha256_file(dest),
                }
            )

    manifest = {
        "id": args.id,
        "created_at_utc": dt.datetime.now(dt.UTC).isoformat(timespec="seconds"),
        "source": {
            "bundle_dir": str(bundle_dir),
            "slot": args.slot,
            "state_path": str(state_src),
            "state_sha256": sha256_file(state_src),
            "capture_path": str(capture),
            "capture_sha256": sha256_file(capture),
        },
        "promoted": {
            "state_path": str(normalized_state),
            "state_sha256": sha256_file(normalized_state),
            "capture_path": str(promoted_capture),
            "capture_sha256": sha256_file(promoted_capture),
            "state_source_layout": "ParaLLEl N64 slot-0 normalized",
            "load_hint": f"--state-source {output_dir / 'state'} then load-slot --slot 0 --paused",
        },
        "scene_identity": {
            "map": args.scene_map,
            "entry": args.scene_entry,
            "settle_frames": args.settle_frames,
        },
        "lineage": {
            "parent_id": args.parent_id,
            "notes": args.notes,
        },
        "runtime_identity": {
            "retroarch_bin": session.get("RETROARCH_BIN", ""),
            "base_config": session.get("BASE_CONFIG", ""),
            "append_config": session.get("APPEND_CONFIG", ""),
            "core_options_file": session.get("CORE_OPTIONS_LAUNCH_FILE", session.get("CORE_OPTIONS_FILE", "")),
            "rom_path": session.get("ROM_PATH", ""),
            "core_path": session.get("CORE_PATH", ""),
            "hires_cache_path": session.get("HIRES_CACHE_PATH", ""),
            "savefile_source": session.get("SAVEFILE_SOURCE", ""),
            "mode": session.get("MODE", ""),
            "mvk_config_use_metal_argument_buffers": session.get("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", ""),
            "parallel_rdp_disable_hires_shader": session.get("PARALLEL_RDP_DISABLE_HIRES_SHADER", ""),
        },
        "hashes": {
            "rom_sha256": session.get("ROM_SHA256", optional_hash(session.get("ROM_PATH", ""))),
            "core_sha256": session.get("CORE_SHA256", optional_hash(session.get("CORE_PATH", ""))),
            "base_config_sha256": session.get("BASE_CONFIG_SHA256", optional_hash(session.get("BASE_CONFIG", ""))),
            "append_config_sha256": session.get("APPEND_CONFIG_SHA256", optional_hash(session.get("APPEND_CONFIG", ""))),
            "core_options_sha256": session.get("CORE_OPTIONS_FILE_SHA256", optional_hash(session.get("CORE_OPTIONS_LAUNCH_FILE", ""))),
            "hires_cache_sha256": session.get("HIRES_CACHE_SHA256", optional_hash(session.get("HIRES_CACHE_PATH", ""))),
            "savefile_source_sha256": session.get("SAVEFILE_SOURCE_SHA256", optional_hash(session.get("SAVEFILE_SOURCE", ""))),
        },
        "copied_configs": copied_configs,
        "copied_savefiles": copied_savefiles,
        "copied_logs": copied_logs,
    }

    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (output_dir / "notes.md").write_text(
        f"# {args.id}\n\n{args.notes or 'No notes provided.'}\n",
        encoding="utf-8",
    )

    print(output_dir)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"promote_interactive_state.py: {exc}", file=sys.stderr)
        raise SystemExit(1)
