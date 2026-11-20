#!/usr/bin/env python3
"""Boot every ROM in a list on one libretro core and record a mechanical verdict per ROM.

Each ROM runs once in a fresh frontend session through the regression runner's
adapter session (tools/regression/run_scenarios.py): power-on, the input
pattern's presses, a capture at each pattern capture frame, stop. The core
profile sets the port-1 accessory and renderer options explicitly; the run
records the options in effect.

Runs live in OUT/runs/<key>, one directory per (core sha256, ROM sha256, sweep
config hash). A directory with a record.json is complete and is skipped on
rerun unless its verdict is passed to --retry; a directory without one is
moved aside and run again. Compare cores afterwards with report.py.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import zipfile
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
TOOLS_DIR = HERE.parent
sys.path.insert(0, str(TOOLS_DIR / "regression"))
sys.path.insert(0, str(HERE))
import run_scenarios as rs  # noqa: E402
import input_pattern  # noqa: E402
import sheets  # noqa: E402
import verdicts  # noqa: E402

PROFILES = HERE / "profiles.json"
DEFAULT_PATTERN = HERE / "patterns" / "default.json"
RECORD_SCHEMA = "n64-sweep-record/1"
CONFIG_SCHEMA = "n64-sweep-config/1"
ROM_SUFFIXES = (".z64", ".v64", ".n64")
# Caller environment that changes what the frontend presents, so it belongs to the condition.
CONDITION_ENV_PREFIXES = ("RETROARCH_VIDEO_", "MVK_")
BASE_CONFIG = 'config_save_on_exit = "false"\n'
STEP_CHUNK = 120


class SetupError(Exception):
    """The sweep cannot start: bad profile, options, ROM list or paths."""


def canonical_hash(value: Any) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


# --------------------------------------------------------------------------
# Profiles and options


def load_profiles(path: Path) -> dict[str, Any]:
    data = json.loads(path.read_text())
    if data.get("schema") != "n64-sweep-profiles/1":
        raise SetupError(f"{path}: schema must be 'n64-sweep-profiles/1'")
    for name, profile in data["cores"].items():
        accessory = profile.get("port1_accessory") or {}
        if not (isinstance(accessory.get("option"), str) and isinstance(accessory.get("value"), str)):
            raise SetupError(f"{path}: profile {name} must set port1_accessory.option and .value")
        if not all(isinstance(k, str) and isinstance(v, str) for k, v in profile.get("options", {}).items()):
            raise SetupError(f"{path}: profile {name} options must map strings to strings")
    return data


def requested_options(profile: dict[str, Any], overrides: dict[str, str]) -> dict[str, str]:
    """Profile options, then the port-1 accessory, then caller overrides."""
    accessory = profile["port1_accessory"]
    return {**profile.get("options", {}), accessory["option"]: accessory["value"], **overrides}


def option_problems(requested: dict[str, str], declared: dict[str, dict[str, Any]]) -> list[str]:
    """Requested options the core does not declare, or values it does not offer."""
    problems = []
    for key, value in sorted(requested.items()):
        if key not in declared:
            problems.append(f"{key}: not declared by the core")
        elif declared[key]["values"] and value not in declared[key]["values"]:
            problems.append(f"{key}={value!r}: core offers {declared[key]['values']}")
    return problems


def effective_options(requested: dict[str, str], declared: dict[str, dict[str, Any]] | None) -> dict[str, str]:
    """Every declared option at its default unless requested; unknown requests kept as given."""
    defaults = {k: v["default"] for k, v in (declared or {}).items()}
    return dict(sorted({**defaults, **requested}.items()))


def query_core(core: Path) -> dict[str, Any]:
    result = subprocess.run([sys.executable, "-I", str(HERE / "core_info.py"), str(core)],
                            capture_output=True, text=True, timeout=120, check=False)
    if result.returncode != 0:
        raise SetupError(f"core option query failed (rc={result.returncode}): {result.stderr.strip()[-400:]}")
    return json.loads(result.stdout)


def parse_option_file(text: str) -> dict[str, str]:
    options = {}
    for line in text.splitlines():
        key, sep, value = line.partition("=")
        value = value.strip()
        if sep and key.strip() and len(value) >= 2 and value[0] == value[-1] == '"':
            options[key.strip()] = value[1:-1]
    return options


def option_file(options: dict[str, str]) -> str:
    return "".join(f'{key} = "{value}"\n' for key, value in sorted(options.items()))


# --------------------------------------------------------------------------
# Identity and resume keys


def config_identity(*, profile_name: str, requested: dict[str, str], frontend: dict[str, Any],
                    frontend_sha256: str, base_config: str, env: dict[str, str]) -> dict[str, Any]:
    """Everything shared by a sweep's runs except core, ROM and input pattern.

    Paths, the runtime-class label and host do not belong here, so one library
    split across hosts with the same frontend and settings shares a config hash.
    """
    return {
        "schema": CONFIG_SCHEMA, "profile": profile_name, "options": dict(sorted(requested.items())),
        "frontend_settings": frontend, "frontend_sha256": frontend_sha256,
        "base_config_sha256": hashlib.sha256(base_config.encode()).hexdigest(),
        "env": {k: v for k, v in sorted(env.items()) if k.startswith(CONDITION_ENV_PREFIXES)},
    }


def run_key(core_sha256: str, rom_sha256: str, config_hash: str, input_hash: str) -> tuple[str, str]:
    """(directory name, run config hash) for one run; the input pattern is part of the config."""
    run_config = hashlib.sha256(f"{config_hash}:{input_hash}".encode()).hexdigest()
    return f"{rom_sha256[:16]}-{core_sha256[:12]}-{run_config[:12]}", run_config


def resume_action(run_dir: Path, retry: set[str]) -> str:
    """skip (complete), rerun (complete but its verdict is retried), restart (incomplete) or new."""
    record = run_dir / "record.json"
    if record.is_file():
        try:
            verdict = json.loads(record.read_text()).get("verdict")
        except json.JSONDecodeError:
            return "restart"
        return "rerun" if verdict in retry else "skip"
    return "restart" if run_dir.exists() else "new"


# --------------------------------------------------------------------------
# ROMs


def list_roms(source: Path) -> list[Path]:
    """A directory (searched recursively) or a text file of paths, one per line."""
    if source.is_dir():
        return sorted(p for p in source.rglob("*") if p.is_file() and p.suffix.lower() in ROM_SUFFIXES + (".zip",))
    roms = []
    for line in source.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            path = Path(line)
            roms.append(path if path.is_absolute() else source.parent / path)
    return roms


def zip_member(archive: zipfile.ZipFile) -> zipfile.ZipInfo:
    members = [i for i in archive.infolist() if Path(i.filename).suffix.lower() in ROM_SUFFIXES]
    if not members:  # some dumps carry the ROM as .bin
        members = [i for i in archive.infolist() if Path(i.filename).suffix.lower() == ".bin"]
    if len(members) != 1:
        raise SetupError(f"{archive.filename}: expected one ROM member, found {[m.filename for m in members]}")
    return members[0]


def rom_identity(path: Path, index: dict[str, Any]) -> dict[str, Any]:
    """sha256 of the ROM bytes as supplied (the zip member for a zip), cached by path, size and mtime."""
    stat = path.stat()
    key = str(path.resolve())
    cached = index.get(key)
    if cached and cached["size"] == stat.st_size and cached["mtime_ns"] == stat.st_mtime_ns:
        return cached
    digest = hashlib.sha256()
    member = None
    if path.suffix.lower() == ".zip":
        with zipfile.ZipFile(path) as archive:
            info = zip_member(archive)
            member = info.filename
            with archive.open(info) as stream:
                for chunk in iter(lambda: stream.read(1 << 20), b""):
                    digest.update(chunk)
    else:
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1 << 20), b""):
                digest.update(chunk)
    index[key] = {"size": stat.st_size, "mtime_ns": stat.st_mtime_ns, "sha256": digest.hexdigest(), "member": member}
    return index[key]


def stage_rom(path: Path, identity: dict[str, Any], stage: Path) -> Path:
    """Copy the ROM to `stage/content<ext>` so every run sees the same content name."""
    if identity["member"]:
        target = stage / ("content" + Path(identity["member"]).suffix.lower())
        with zipfile.ZipFile(path) as archive, archive.open(identity["member"]) as src, target.open("wb") as dst:
            shutil.copyfileobj(src, dst)
    else:
        target = stage / ("content" + path.suffix.lower())
        shutil.copyfile(path, target)
    if rs.sha256_file(target) != identity["sha256"]:
        raise SetupError(f"{path}: staged ROM does not match its recorded sha256")
    return target


# --------------------------------------------------------------------------
# One run


def observe(record: dict[str, Any], run_dir: Path) -> dict[str, Any]:
    """The verdict inputs from a run_scenarios record and its bundle."""
    from PIL import Image

    exit_class, error = record["exit_class"], record["error"]
    captures = []
    for checkpoint in record["checkpoints"]:
        if not checkpoint.get("capture_path"):
            continue
        try:
            with Image.open(run_dir / checkpoint["capture_path"]) as image:
                blank = verdicts.capture_is_blank(image)
        except OSError as err:  # an unreadable capture is a harness failure, not a picture
            exit_class, error, blank = "capture-failed", f"capture at {checkpoint['after_frames']}: {err}", None
        captures.append({"frame": checkpoint["after_frames"], "sha256": checkpoint["capture_sha256"],
                         "blank": blank, "path": checkpoint["capture_path"]})
    log = run_dir / record["bundle"] / "logs" / "retroarch.log"
    log_text = log.read_text(errors="replace") if log.is_file() else ""
    return {"exit_class": exit_class, "error": error, "log_flags": record["log_flags"],
            "frames_run": record["frames_run"], "captures": captures,
            "load_failure_logged": verdicts.load_failure_logged(log_text)}


def write_contact(run_dir: Path, title: str, plan: dict[str, Any], captures: list[dict[str, Any]]) -> None:
    by_frame = {c["frame"]: run_dir / c["path"] for c in captures}
    sheets.save(sheets.run_row(title, [(f, by_frame.get(f)) for f in plan["captures"]]), run_dir / "contact.jpg")


def run_one(rom: Path, identity: dict[str, Any], run_dir: Path, pattern: dict[str, Any], pattern_source: str,
            sweep: dict[str, Any], ctx: rs.RunContext) -> dict[str, Any]:
    run_dir.mkdir(parents=True)
    options_path = run_dir / "core-options.opt"
    options_path.write_text(option_file(sweep["requested"]))
    frontend_cfg = run_dir / "frontend.cfg"
    frontend_cfg.write_text(
        f'core_options_path = "{options_path}"\nglobal_core_options = "true"\ngame_specific_options = "false"\n'
        + "".join(f'{k} = "{v}"\n' for k, v in sweep["frontend"]["append_config"].items()))
    plan = {"frames": pattern["frames"], "captures": pattern["captures"],
            "frozen_min_captures": pattern["frozen_min_captures"]}
    scenario_data = {
        "schema": rs.SCENARIO_SCHEMA, "id": "sweep-" + identity["sha256"][:16],
        "game": {"rom_sha256": identity["sha256"], "title": rom.name},
        "start": {"kind": "power-on", "savefile_sha256": None}, "program": input_pattern.program(pattern),
        "checkpoints": [{"after_frames": f, "capture": True, "ram": []} for f in pattern["captures"]],
        "end": {"save_state": False}, "modes": ["off"],
    }
    scenario_path = run_dir / "scenario.json"
    scenario_path.write_text(json.dumps(scenario_data, indent=1) + "\n")
    scenario = rs.parse_scenario(scenario_path)

    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="sweep-rom-") as stage:
        ctx.args.rom = stage_rom(rom, identity, Path(stage))
        ctx.args.extra_append_config = frontend_cfg
        ctx.out = run_dir
        stage_seconds = time.monotonic() - started
        record = rs.run_scenario(scenario, ctx)

    observation = observe(record, run_dir)
    result = verdicts.classify(observation, plan)
    written_back = parse_option_file(options_path.read_text()) if options_path.is_file() else {}
    full = {
        "schema": RECORD_SCHEMA, "key": run_dir.name,
        "core_sha256": record["core_sha256"], "rom_sha256": identity["sha256"],
        "config_hash": sweep["config_hash"], "input_hash": input_pattern.digest(pattern),
        "run_config_hash": sweep["run_config"],
        **result,
        "frames_planned": pattern["frames"], "frames_reached": record["frames_run"],
        "captures": observation["captures"],
        "exit_class": observation["exit_class"], "error": observation["error"], "log_flags": record["log_flags"],
        "orphans": record["orphans"],
        "rom": {"name": rom.name, "path": str(rom), "member": identity["member"], "size": identity["size"]},
        "core": {"profile": sweep["profile"], "path": str(ctx.args.core), **sweep["library"]},
        "frontend": {"path": str(ctx.args.retroarch_bin), "sha256": record["frontend_sha256"]},
        "runtime_class": ctx.args.runtime_class, "host": socket.gethostname(),
        "input": {"source": pattern_source, "pattern": pattern},
        "options": {"requested": sweep["requested"], "effective": sweep["effective"],
                    "declared": sweep["declared"] is not None,
                    # RetroArch rewrites the file on a clean exit with every option the core declared.
                    "written_back": written_back if written_back != sweep["requested"] else None},
        "timings": {"stage_seconds": round(stage_seconds, 3), "session_seconds": record["wall_seconds"],
                    "fps": record["fps"], "wall_seconds": round(time.monotonic() - started, 3)},
        "started": record["started"], "ended": record["ended"], "bundle": record["bundle"],
    }
    title = f"{sweep['profile']} | {rom.name} | {result['verdict']}" + (
        f" ({result['sub_class']})" if result["sub_class"] else "")
    write_contact(run_dir, title, plan, observation["captures"])
    tmp = run_dir / "record.json.tmp"
    tmp.write_text(json.dumps(full, indent=1, sort_keys=True) + "\n")
    tmp.replace(run_dir / "record.json")
    return full


# --------------------------------------------------------------------------
# Entry point


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--roms", type=Path, required=True,
                        help="directory of .z64/.v64/.n64/.zip ROMs, or a text file with one path per line")
    parser.add_argument("--core", type=Path, required=True)
    parser.add_argument("--profile", required=True, help="core profile name in --profiles")
    parser.add_argument("--retroarch-bin", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True, help="result directory (use one per core)")
    parser.add_argument("--runtime-class", required=True, help="caller-declared runtime class label (recorded)")
    parser.add_argument("--base-config", type=Path,
                        help="frontend base config (default: OUT/base.cfg with config_save_on_exit off)")
    parser.add_argument("--profiles", type=Path, default=PROFILES)
    parser.add_argument("--input", type=Path, default=DEFAULT_PATTERN, help="input pattern for every ROM")
    parser.add_argument("--input-overrides", type=Path,
                        help="directory of <rom_sha256>.json patterns that replace --input for that ROM")
    parser.add_argument("--option", action="append", default=[], metavar="KEY=VALUE",
                        help="core option applied after the profile (repeatable; part of the config hash)")
    parser.add_argument("--retry", action="append", default=[], choices=verdicts.VERDICTS,
                        help="rerun complete runs with this verdict (repeatable)")
    parser.add_argument("--limit", type=int, help="run at most N ROMs that need a run")
    parser.add_argument("--no-option-check", action="store_true",
                        help="do not query the core's declared options (effective options then unknown)")
    parser.add_argument("--adapter", type=Path, default=rs.DEFAULT_ADAPTER, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    overrides = {}
    for entry in args.option:
        key, sep, value = entry.partition("=")
        if not sep or not key:
            parser.error("--option requires KEY=VALUE")
        overrides[key] = value
    args.option = overrides
    for name in ("core", "retroarch_bin"):
        if not getattr(args, name).is_file():
            parser.error(f"--{name.replace('_', '-')} not found: {getattr(args, name)}")
    if any(ch in str(args.out.resolve()) for ch in '|"'):
        parser.error("--out must not contain '|' or '\"'")
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    out = args.out.resolve()
    try:
        profiles = load_profiles(args.profiles)
        if args.profile not in profiles["cores"]:
            raise SetupError(f"unknown profile {args.profile!r}; known: {', '.join(profiles['cores'])}")
        profile = profiles["cores"][args.profile]
        requested = requested_options(profile, args.option)
        default_pattern = input_pattern.load(args.input)
        declared, library = None, {"library_name": None, "library_version": None}
        if not args.no_option_check:
            info = query_core(args.core)
            declared = info["options"]
            library = {"library_name": info["library"]["name"], "library_version": info["library"]["version"]}
            problems = option_problems(requested, declared)
            if problems:
                raise SetupError("profile options do not match the core:\n  " + "\n  ".join(problems))
        roms = list_roms(args.roms)
        if not roms:
            raise SetupError(f"no ROMs found in {args.roms}")
    except (SetupError, input_pattern.PatternError, OSError, ValueError) as err:
        print(f"[sweep] {err}", file=sys.stderr)
        return 2

    out.mkdir(parents=True, exist_ok=True)
    base_config = args.base_config
    if base_config is None:
        base_config = out / "base.cfg"
        base_config.write_text(BASE_CONFIG)
    frontend = profiles["frontend"]
    frontend_settings = {"append_config": frontend["append_config"], "env": frontend["env"]}
    frontend_sha = rs.sha256_file(args.retroarch_bin)
    identity = config_identity(profile_name=args.profile, requested=requested, frontend=frontend_settings,
                               frontend_sha256=frontend_sha, base_config=base_config.read_text(),
                               # The profile's frontend env replaces these caller values in the session.
                               env={k: v for k, v in os.environ.items() if k not in frontend["env"]})
    config_hash = canonical_hash(identity)
    (out / "configs").mkdir(exist_ok=True)
    (out / "configs" / f"{config_hash[:16]}.json").write_text(json.dumps(identity, indent=1) + "\n")
    core_sha = rs.sha256_file(args.core)
    adapter = args.adapter.resolve()
    runner_args = argparse.Namespace(
        core=args.core.resolve(), retroarch_bin=args.retroarch_bin.resolve(), base_config=base_config.resolve(),
        env=dict(frontend["env"]), extra_append_config=None, mode="off", pack=None, pack_map=None, rom=None,
        rom_map=None, blobs=None, ttl=None, step_chunk=STEP_CHUNK, orphan_grace=3.0, hires_summary_interval=600,
        hires_budget_mb=8192, runtime_class=args.runtime_class, renderer_change=False)
    ctx = rs.RunContext(args=runner_args, adapter=adapter, out=out, condition=f"off:{args.runtime_class}",
                        identities={"core_sha256": core_sha, "frontend_sha256": frontend_sha})
    sweep = {"profile": args.profile, "requested": requested, "declared": declared,
             "effective": effective_options(requested, declared) if declared is not None else None,
             "library": library, "frontend": frontend_settings, "config_hash": config_hash}
    print(f"[sweep] {len(roms)} ROMs; profile {args.profile}; core {core_sha[:12]}; config {config_hash[:12]}")

    index_path = out / "rom-index.json"
    index = json.loads(index_path.read_text()) if index_path.is_file() else {}
    retry = set(args.retry)
    counts = {"skip": 0, "run": 0, "failed": 0}
    for rom in roms:
        if args.limit is not None and counts["run"] >= args.limit:
            break
        try:
            rom_id = rom_identity(rom, index)
            pattern, source = input_pattern.for_rom(default_pattern, args.input_overrides, rom_id["sha256"])
        except (SetupError, input_pattern.PatternError, OSError, zipfile.BadZipFile) as err:
            print(f"[sweep] {rom.name}: not run: {err}", file=sys.stderr)
            counts["failed"] += 1
            continue
        index_path.write_text(json.dumps(index, indent=1, sort_keys=True) + "\n")
        key, run_config = run_key(core_sha, rom_id["sha256"], config_hash, input_pattern.digest(pattern))
        run_dir = out / "runs" / key
        action = resume_action(run_dir, retry)
        if action == "skip":
            counts["skip"] += 1
            continue
        if action in ("rerun", "restart"):
            run_dir.rename(run_dir.with_name(f"{key}.{'previous' if action == 'rerun' else 'incomplete'}-{int(time.time())}"))
        doctor = subprocess.run([str(adapter), "doctor"], capture_output=True, text=True, check=False)
        if doctor.returncode != 0:
            print(f"[sweep] host is not idle; stopping:\n{doctor.stdout}{doctor.stderr}", file=sys.stderr)
            return 1
        try:
            record = run_one(rom, rom_id, run_dir, pattern, source, {**sweep, "run_config": run_config}, ctx)
        except (SetupError, OSError, zipfile.BadZipFile) as err:
            # No record is written, so the next sweep moves this directory aside and retries.
            print(f"[sweep] {rom.name}: not run: {err}", file=sys.stderr)
            counts["failed"] += 1
            continue
        counts["run"] += 1
        sub = f" ({record['sub_class']})" if record["sub_class"] else ""
        print(f"[sweep] {rom.name}: {record['verdict']}{sub} frames {record['frames_reached']}/"
              f"{record['frames_planned']} in {record['timings']['wall_seconds']}s", flush=True)
        if "orphan-survived" in record["log_flags"]:
            print("[sweep] a frontend process survived reaping; stopping.", file=sys.stderr)
            return 1
    print(f"[sweep] ran {counts['run']}, skipped {counts['skip']} complete, {counts['failed']} not runnable")
    return 1 if counts["failed"] else 0


if __name__ == "__main__":
    sys.exit(main())
