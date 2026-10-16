#!/usr/bin/env python3
"""Run portable regression scenarios through the interactive RetroArch adapter.

Each selected scenario runs in a fresh frontend process: start paused, stage and
load the start state (bootstrap frame, paused load, barrier and log check), run
the frame program in bounded steps, read RAM cues and capture at checkpoints,
save the end state, stop, and verify no frontend process from the bundle
survives. Observations become one result record per scenario; gates follow
tools/regression/SCENARIO_SCHEMA.md and the README's gate policy.

The runner never edits scenario files. `--record-expectations` writes observed
values to `<out>/expectations/<id>.expect.json` for an explicit golden review.
"""

from __future__ import annotations

import argparse
import contextlib
import datetime as dt
import fnmatch
import hashlib
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

TOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIR))
from savestate_bridge import unwrap  # noqa: E402

SCENARIO_SCHEMA = "n64-regression-scenario/1"
EXPECT_SCHEMA = "n64-regression-expectations/1"
DEFAULT_ADAPTER = TOOLS_DIR / "adapters" / "retroarch_interactive_session.sh"
CORE_STATE_DIR = "ParaLLEl N64"
START_SLOT, END_SLOT = 1, 2
STATE_MAGIC = b"M64+SAVE"
# Floor used to size step timeouts; 4x headless runs are well above it.
MIN_FPS = 15.0
ID_RE = re.compile(r"^[A-Za-z0-9._-]+$")
SHA_RE = re.compile(r"^[0-9a-f]{64}$")
PROGRAM_RES = {
    "SET_INPUT_PORT": re.compile(r"^SET_INPUT_PORT \d+ (0x[0-9a-fA-F]+|\d+)(( -?\d+){4})?$"),
    "CLEAR_INPUT_PORT": re.compile(r"^CLEAR_INPUT_PORT \d+$"),
    "STEP_FRAME": re.compile(r"^STEP_FRAME ([1-9]\d*)$"),
}
STATUS_RE = re.compile(r"GET_STATUS\s+(\w+)\b.*?frame=(\d+)\s*$")
SUMMARY_RE = re.compile(r"Hi-res keying summary[^:]*:\s*(.*)$")
FILTERED_RE = re.compile(r"Hi-res keying filtered: reason=(\S+)")
CACHE_FAILED = "Hi-res replacement cache load failed"
STATE_LOAD_FAILED = "[State] Failed to load state"


class ScenarioError(ValueError):
    """A scenario file does not satisfy the v1 schema."""


class RunFailure(Exception):
    """A session step failed; `exit_class` classifies the run."""

    def __init__(self, exit_class: str, message: str) -> None:
        super().__init__(message)
        self.exit_class = exit_class


@dataclass(frozen=True)
class RamCue:
    name: str
    address: int
    length: int


@dataclass(frozen=True)
class Checkpoint:
    after_frames: int
    capture: bool
    ram: tuple[RamCue, ...]


@dataclass(frozen=True)
class Scenario:
    path: Path
    data: dict[str, Any]
    id: str
    rom_sha256: str
    start: dict[str, Any]
    program: tuple[str, ...]
    checkpoints: tuple[Checkpoint, ...]
    save_end: bool
    tags: dict[str, Any]

    @property
    def total_frames(self) -> int:
        return sum(int(cmd.split()[1]) for cmd in self.program if cmd.startswith("STEP_FRAME"))


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# --------------------------------------------------------------------------
# Scenario loading and selection


def parse_scenario(path: Path) -> Scenario:
    try:
        data = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as err:
        raise ScenarioError(f"{path}: unreadable scenario: {err}") from None

    def need(cond: bool, message: str) -> None:
        if not cond:
            raise ScenarioError(f"{path}: {message}")

    need(isinstance(data, dict) and data.get("schema") == SCENARIO_SCHEMA,
         f"schema must be {SCENARIO_SCHEMA!r}")
    scenario_id = data.get("id")
    need(isinstance(scenario_id, str) and bool(ID_RE.match(scenario_id)),
         "id must match [A-Za-z0-9._-]+")
    rom_sha = (data.get("game") or {}).get("rom_sha256")
    need(isinstance(rom_sha, str) and bool(SHA_RE.match(rom_sha)), "game.rom_sha256 must be a sha256")

    start = data.get("start")
    need(isinstance(start, dict) and start.get("kind") in ("power-on", "state"),
         "start.kind must be power-on or state")
    if start["kind"] == "state":
        need(bool(SHA_RE.match(str(start.get("state_sha256", "")))), "start.state_sha256 must be a sha256")
        srm = start.get("savefile_sha256")
        need(srm is None or bool(SHA_RE.match(str(srm))), "start.savefile_sha256 must be a sha256 or null")

    program = data.get("program")
    need(isinstance(program, list), "program must be a list")
    for command in program:
        verb = str(command).split(" ", 1)[0]
        need(verb in PROGRAM_RES and bool(PROGRAM_RES[verb].match(str(command))),
             f"unsupported program command: {command!r}")
    total = sum(int(c.split()[1]) for c in program if c.startswith("STEP_FRAME"))

    checkpoints: list[Checkpoint] = []
    seen_frames: set[int] = set()
    for raw in data.get("checkpoints") or []:
        after = raw.get("after_frames")
        need(isinstance(after, int) and 0 <= after <= total,
             f"checkpoint after_frames must be within 0..{total}: {after!r}")
        need(after not in seen_frames, f"duplicate checkpoint at {after}")
        seen_frames.add(after)
        cues = []
        for cue in raw.get("ram") or []:
            try:
                address = int(str(cue["address"]), 16)
                length = int(cue["length"])
                name = str(cue["name"])
            except (KeyError, TypeError, ValueError):
                raise ScenarioError(f"{path}: malformed RAM cue {cue!r}") from None
            need(address % 4 == 0 and length > 0 and length % 4 == 0,
                 f"RAM cue {name} must be word-aligned with a length that is a multiple of 4")
            cues.append(RamCue(name, address, length))
        checkpoints.append(Checkpoint(after, bool(raw.get("capture")), tuple(cues)))

    tags = data.get("tags") or {}
    need(isinstance(tags, dict), "tags must be an object")
    return Scenario(
        path=path, data=data, id=scenario_id, rom_sha256=rom_sha, start=start,
        program=tuple(program), checkpoints=tuple(sorted(checkpoints, key=lambda c: c.after_frames)),
        save_end=bool((data.get("end") or {}).get("save_state")), tags=tags,
    )


def load_scenarios(paths: list[Path]) -> list[Scenario]:
    files: list[Path] = []
    for path in paths:
        if path.is_dir():
            files += sorted(p for p in path.rglob("*.json") if not p.name.endswith(".expect.json"))
        else:
            files.append(path)
    scenarios = [parse_scenario(p) for p in files]
    ids = [s.id for s in scenarios]
    duplicates = sorted({i for i in ids if ids.count(i) > 1})
    if duplicates:
        raise ScenarioError(f"duplicate scenario ids: {', '.join(duplicates)}")
    return scenarios


def matches_filter(scenario: Scenario, selector: str) -> bool:
    """`key=a,b` matches a tag value (or list member); anything else globs the id."""
    if "=" not in selector:
        return fnmatch.fnmatchcase(scenario.id, selector)
    key, _, wanted = selector.partition("=")
    options = wanted.split(",")
    if key == "id":
        return any(fnmatch.fnmatchcase(scenario.id, o) for o in options)
    value = scenario.tags.get(key)
    values = value if isinstance(value, list) else [value]
    return any(str(v) == o for v in values if v is not None for o in options)


# --------------------------------------------------------------------------
# Adapter session


@dataclass
class Reply:
    rc: int
    stdout: str
    stderr: str
    timed_out: bool = False

    @property
    def ok(self) -> bool:
        return self.rc == 0 and not self.timed_out

    def describe(self) -> str:
        text = (self.stderr.strip() or self.stdout.strip()).splitlines()
        tail = text[-1] if text else ""
        return "timed out" if self.timed_out else f"rc={self.rc} {tail}".strip()


class Session:
    """One bundle's adapter calls; every call is logged into the bundle."""

    def __init__(self, adapter: Path, bundle: Path, env: dict[str, str]) -> None:
        self.adapter = adapter
        self.bundle = bundle
        self.env = env
        self.log_path = bundle / "logs" / "regression.adapter.log"
        self.step_seconds = 0.0

    def call(self, subcommand: str, *args: str, timeout: float) -> Reply:
        argv = [str(self.adapter), subcommand, "--bundle-dir", str(self.bundle), *args]
        # Files rather than pipes: a frontend that inherits a pipe must not
        # keep this call waiting after the adapter itself has exited.
        with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
            proc = subprocess.Popen(argv, stdout=out, stderr=err, env=self.env, start_new_session=True)
            timed_out = False
            try:
                proc.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                timed_out = True
                with contextlib.suppress(ProcessLookupError, PermissionError):
                    os.killpg(proc.pid, signal.SIGKILL)
                proc.wait()
            out.seek(0)
            err.seek(0)
            reply = Reply(proc.returncode, out.read().decode(errors="replace"),
                          err.read().decode(errors="replace"), timed_out)
        with self.log_path.open("a") as log:
            log.write(f"{utc_now()} {subcommand} {' '.join(args)} -> {reply.describe()}\n")
            for line in (reply.stdout + reply.stderr).splitlines()[-20:]:
                log.write(f"    {line}\n")
        return reply

    def send(self, command: str, timeout: float = 30.0) -> Reply:
        return self.call("send", "--command", command, timeout=timeout)

    def status(self) -> tuple[str, int] | None:
        reply = self.call("status", timeout=15)
        if not reply.ok and "not running" in reply.stderr:
            raise self.fail(reply, "nonzero", "GET_STATUS")
        for line in reversed(reply.stdout.splitlines()):
            match = STATUS_RE.search(line)
            if match:
                return match.group(1), int(match.group(2))
        return None

    def session_ended(self) -> bool:
        """True when the adapter's recorded session group no longer exists."""
        try:
            pgid = int((self.bundle / "session.pid").read_text().strip())
            os.killpg(pgid, 0)
        except (OSError, ValueError) as err:
            return isinstance(err, ProcessLookupError)
        return False

    def fail(self, reply: Reply, default_class: str, what: str) -> RunFailure:
        """Classify a failed call: a dead session outranks the step's own class."""
        if "not running" in reply.stderr or self.session_ended():
            end_reason = self.bundle / "logs" / "session.end-reason"
            ttl = end_reason.is_file() and "ttl" in end_reason.read_text()
            return RunFailure("timeout" if ttl else "nonzero", f"{what}: session ended ({reply.describe()})")
        return RunFailure(default_class, f"{what}: {reply.describe()}")

    def step(self, frames: int, expected_frame: int) -> None:
        started = time.monotonic()
        # Outlast the adapter's own lock, ack and completion waits.
        reply = self.send(f"STEP_FRAME {frames}", timeout=frames / MIN_FPS + 90)
        if not reply.ok:
            raise self.fail(reply, "step-timeout", f"STEP_FRAME {frames}")
        # The adapter already waits for PAUSED at >= target; require the exact
        # frame so an over-step cannot pass silently.
        deadline = time.monotonic() + frames / MIN_FPS + 10
        observed: tuple[str, int] | None = None
        while time.monotonic() < deadline:
            observed = self.status()
            if observed and observed[0] == "PAUSED" and observed[1] >= expected_frame:
                break
            time.sleep(0.2)
        self.step_seconds += time.monotonic() - started
        if not observed or observed[0] != "PAUSED" or observed[1] < expected_frame:
            raise RunFailure("step-timeout", f"not PAUSED at frame {expected_frame} (last status {observed})")
        if observed[1] != expected_frame:
            raise RunFailure("step-timeout", f"stepped past frame {expected_frame} to {observed[1]}")


# --------------------------------------------------------------------------
# Process survival


def bundle_processes(bundle: Path) -> list[tuple[int, int, str]]:
    """Processes whose command line names this bundle, excluding this runner's ancestry."""
    listing = subprocess.run(["ps", "-axww", "-o", "pid=,ppid=,pgid=,args="],
                             capture_output=True, text=True, check=False).stdout
    rows: dict[int, tuple[int, int, str]] = {}
    for line in listing.splitlines():
        parts = line.split(None, 3)
        if len(parts) == 4 and parts[0].isdigit():
            rows[int(parts[0])] = (int(parts[1]), int(parts[2]), parts[3])
    own: set[int] = set()
    pid = os.getpid()
    while pid > 1 and pid not in own:
        own.add(pid)
        pid = rows.get(pid, (0, 0, ""))[0]
    needle = re.compile(re.escape(str(bundle)) + r"(?=/|\s|$)")
    return [(p, pgid, args) for p, (_, pgid, args) in rows.items()
            if p not in own and needle.search(args)]


def signal_process(pid: int, pgid: int, sig: signal.Signals) -> None:
    with contextlib.suppress(ProcessLookupError, PermissionError):
        if pgid > 1 and pgid != os.getpgrp():
            os.killpg(pgid, sig)
        else:
            os.kill(pid, sig)


def reap_survivors(bundle: Path, adapter: Path, grace: float) -> tuple[list[str], list[str]]:
    """Return (reaped, remaining) command lines of frontend processes left after stop.

    The adapter's own TTL watchdog also names the bundle; it exits by itself
    shortly after the session, so it is terminated quietly and not counted.
    """
    def frontend() -> list[tuple[int, int, str]]:
        found = []
        for pid, pgid, args in bundle_processes(bundle):
            if str(adapter) in args:
                with contextlib.suppress(ProcessLookupError, PermissionError):
                    os.kill(pid, signal.SIGTERM)
            else:
                found.append((pid, pgid, args))
        return found

    deadline = time.monotonic() + grace
    survivors = frontend()
    while survivors and time.monotonic() < deadline:
        time.sleep(0.25)
        survivors = frontend()
    if not survivors:
        return [], []
    for sig in (signal.SIGTERM, signal.SIGKILL):
        for pid, pgid, _ in frontend():
            signal_process(pid, pgid, sig)
        settle = time.monotonic() + 3
        while frontend() and time.monotonic() < settle:
            time.sleep(0.2)
    return [args for *_, args in survivors], [args for *_, args in frontend()]


# --------------------------------------------------------------------------
# Log evidence


def parse_hires(log_text: str) -> dict[str, Any]:
    """Summarize hi-res evidence; the last keying summary line wins.

    Periodic summaries (PARALLEL_RDP_HIRES_SUMMARY_INTERVAL_FRAMES, where the
    core supports it) and the shutdown summary share one prefix, so a session
    that was killed before shutdown still has its latest periodic line.
    """
    lines = [m.group(0) for m in map(SUMMARY_RE.search, log_text.splitlines()) if m]
    hires: dict[str, Any] = {
        "summary_lines": lines[-3:], "summary_count": len(lines),
        "cache_load_failed": CACHE_FAILED in log_text,
        "provider": None, "entries": None, "hits": None, "misses": None, "draw_hits": None,
        "fallback_reasons": {},
    }
    for reason in FILTERED_RE.findall(log_text):
        hires["fallback_reasons"][reason] = hires["fallback_reasons"].get(reason, 0) + 1
    if not lines:
        return hires
    body = SUMMARY_RE.search(lines[-1]).group(1)
    fields: dict[str, int] = {}
    for key, value in re.findall(r"(\w+)=(\d+)", body):
        fields.setdefault(key, int(value))  # top-level keys precede nested repeats
    provider = re.search(r"\bprovider=(\w+)", body)
    hires.update(
        provider=provider.group(1) if provider else None,
        entries=fields.get("entries"),
        hits=fields.get("hits"),
        misses=fields.get("misses"),
        # Draw-time replacement hits; older summaries without the field fall
        # back to lookup hits.
        draw_hits=fields.get("compat_draw_hits", fields.get("hits")),
    )
    return hires


# --------------------------------------------------------------------------
# Running one scenario


@dataclass
class RunContext:
    args: argparse.Namespace
    adapter: Path
    out: Path
    condition: str
    identities: dict[str, str | None]
    rom_sha_cache: dict[Path, str] = field(default_factory=dict)

    def rom_sha(self, path: Path) -> str:
        if path not in self.rom_sha_cache:
            self.rom_sha_cache[path] = sha256_file(path)
        return self.rom_sha_cache[path]


def resolve_rom(scenario: Scenario, ctx: RunContext) -> Path:
    args = ctx.args
    if args.rom:
        candidates = [args.rom]
    elif args.rom_map:
        mapped = json.loads(args.rom_map.read_text()).get(scenario.rom_sha256)
        candidates = [Path(mapped)] if mapped else []
    elif args.blobs:
        candidates = sorted((args.blobs / "roms").glob(f"{scenario.rom_sha256}.*"))
    else:
        candidates = []
    if not candidates or not candidates[0].is_file():
        raise RunFailure("not-run", f"no ROM available for {scenario.rom_sha256}")
    rom = candidates[0].resolve()
    if ctx.rom_sha(rom) != scenario.rom_sha256:
        raise RunFailure("not-run", f"ROM {rom.name} does not match game.rom_sha256")
    return rom


def resolve_blob(blobs: Path | None, kind: str, sha: str, suffix: str) -> Path:
    path = blobs / kind / f"{sha}{suffix}" if blobs else None
    if path is None or not path.is_file():
        raise RunFailure("not-run", f"missing blob {kind}/{sha}{suffix}")
    if sha256_file(path) != sha:
        raise RunFailure("not-run", f"blob {kind}/{sha}{suffix} does not match its name")
    return path


def step_plan(scenario: Scenario, chunk: int) -> list[tuple[str, Any]]:
    """Expand the program into sends, bounded steps and checkpoint stops."""
    stops = {c.after_frames: c for c in scenario.checkpoints}
    plan: list[tuple[str, Any]] = []
    frames = 0
    if 0 in stops:
        plan.append(("checkpoint", stops[0]))
    for command in scenario.program:
        if not command.startswith("STEP_FRAME"):
            plan.append(("send", command))
            continue
        remaining = int(command.split()[1])
        while remaining:
            next_stop = min((f for f in stops if f > frames), default=None)
            size = min(remaining, chunk)
            if next_stop is not None:
                size = min(size, next_stop - frames)
            plan.append(("step", size))
            frames += size
            remaining -= size
            if frames in stops:
                plan.append(("checkpoint", stops[frames]))
    return plan


def read_ram(session: Session, cue: RamCue) -> tuple[str | None, str | None]:
    reply = session.send(f"READ_CORE_MEMORY {cue.address:x} {cue.length}")
    if not reply.ok:
        raise session.fail(reply, "timeout", f"READ_CORE_MEMORY {cue.name}")
    line = next((l for l in reversed(reply.stdout.splitlines()) if l.startswith("READ_CORE_MEMORY")), "")
    fields = line.split()
    if len(fields) < 3 or fields[2] == "-1":
        return None, line or "no reply"
    value = "".join(fields[2:]).lower()
    if not re.fullmatch(r"[0-9a-f]*", value) or len(value) != 2 * cue.length:
        return None, f"unexpected reply: {line}"
    return value, None


def take_checkpoint(session: Session, cp: Checkpoint, out: Path) -> dict[str, Any]:
    observed: dict[str, Any] = {"after_frames": cp.after_frames, "ram": {}, "ram_errors": {},
                                "capture_path": None, "capture_sha256": None, "uniform_black": None}
    for cue in cp.ram:
        value, error = read_ram(session, cue)
        observed["ram"][cue.name] = value
        if error:
            observed["ram_errors"][cue.name] = error
    if cp.capture:
        reply = session.call("screenshot", timeout=40)
        if not reply.ok:
            raise session.fail(reply, "timeout", f"screenshot at {cp.after_frames}")
        capture = Path(reply.stdout.strip().splitlines()[-1])
        warnings = session.bundle / "logs" / "capture.warnings.log"
        observed["capture_path"] = os.path.relpath(capture, out)
        observed["capture_sha256"] = sha256_file(capture)
        observed["uniform_black"] = warnings.is_file() and str(capture) in warnings.read_text()
    return observed


def end_state_sha(session: Session) -> str:
    reply = session.call("save-slot", "--slot", str(END_SLOT), timeout=90)
    if not reply.ok:
        raise session.fail(reply, "timeout", "save-slot")
    saved = [p for p in (session.bundle / "states").rglob(f"*.state{END_SLOT}") if p.is_file()]
    if not saved:
        raise RunFailure("timeout", f"save-slot {END_SLOT} produced no .state{END_SLOT} file")
    body = unwrap(max(saved, key=lambda p: p.stat().st_mtime_ns).read_bytes())
    if body[:8] != STATE_MAGIC:
        raise RunFailure("timeout", "saved end state is not an M64+SAVE body")
    return sha256_bytes(body)


@dataclass(frozen=True)
class Inputs:
    rom: Path
    state: Path | None
    savefile: Path | None


def resolve_inputs(scenario: Scenario, ctx: RunContext) -> Inputs:
    rom = resolve_rom(scenario, ctx)
    if scenario.start["kind"] != "state":
        return Inputs(rom, None, None)
    blobs = ctx.args.blobs
    state = resolve_blob(blobs, "states", scenario.start["state_sha256"], ".state")
    srm_sha = scenario.start.get("savefile_sha256")
    return Inputs(rom, state, resolve_blob(blobs, "savefiles", srm_sha, ".srm") if srm_sha else None)


def execute(scenario: Scenario, ctx: RunContext, session: Session, record: dict[str, Any],
            inputs: Inputs, stage_dir: Path) -> None:
    args = ctx.args
    start = ["--rom", str(inputs.rom), "--core", str(args.core), "--mode", args.mode,
             "--retroarch-bin", str(args.retroarch_bin), "--base-config", str(args.base_config),
             "--ttl-seconds", str(args.ttl or max(600, 180 + scenario.total_frames // 10)),
             "--start-paused"]
    if args.extra_append_config:
        start += ["--extra-append-config", str(args.extra_append_config)]
    if inputs.state:
        # The adapter copies a "ParaLLEl N64" source into the core's sorted
        # state directory; RetroArch names slots after the content basename.
        state_dir = stage_dir / CORE_STATE_DIR
        state_dir.mkdir(parents=True)
        shutil.copyfile(inputs.state, state_dir / f"{inputs.rom.stem}.state{START_SLOT}")
        start += ["--state-source", str(state_dir)]
    if inputs.savefile:
        start += ["--savefile-source", str(inputs.savefile)]

    reply = session.call("start", *start, timeout=180)
    if not reply.ok:
        raise RunFailure("nonzero", f"start: {reply.describe()}")
    status = session.status()
    if not status:
        raise RunFailure("timeout", "no GET_STATUS reply after start")
    frame = status[1]
    if scenario.start["kind"] == "state":
        # IL-19: the paused load needs one presented core frame first.
        session.step(1, frame + 1)
        reply = session.call("load-slot", "--slot", str(START_SLOT), "--paused", timeout=60)
        if not reply.ok:
            raise RunFailure("load-failed", f"load-slot: {reply.describe()}")
        record["load"] = "ok"
        status = session.status()
        if not status or status[0] != "PAUSED":
            raise RunFailure("timeout", f"not paused after load (status {status})")
        frame = status[1]

    base = frame
    for kind, item in step_plan(scenario, args.step_chunk):
        if kind == "send":
            reply = session.send(item)
            if not reply.ok:
                raise session.fail(reply, "timeout", item)
        elif kind == "step":
            frame += item
            session.step(item, frame)
            record["frames_run"] = frame - base
        else:
            record["checkpoints"].append(take_checkpoint(session, item, ctx.out))
    if scenario.save_end:
        record["end_state_sha256"] = end_state_sha(session)


def run_scenario(scenario: Scenario, ctx: RunContext) -> dict[str, Any]:
    args = ctx.args
    # One bundle per condition, so runs of another runtime class in the same
    # output directory never replace the evidence an earlier record points to.
    runtime_slug = re.sub(r"[^A-Za-z0-9._-]+", "_", args.runtime_class)
    bundle = ctx.out / "bundles" / f"{scenario.id}__{args.mode}__{runtime_slug}"
    if bundle.exists():
        bundle.rename(bundle.with_name(f"{bundle.name}.interrupted-{int(time.time())}"))
    (bundle / "logs").mkdir(parents=True)
    env = dict(os.environ)
    if args.mode == "on":
        env["PARALLEL_RDP_HIRES_CACHE_PATH"] = str(args.pack)
        env["PARALLEL_RDP_HIRES_CACHE_SHA256"] = ctx.identities["pack_sha256"] or ""
        env["PARALLEL_RDP_HIRES_SUMMARY_INTERVAL_FRAMES"] = str(args.hires_summary_interval)
    else:
        for key in ("PARALLEL_RDP_HIRES_CACHE_PATH", "PARALLEL_RDP_HIRES_CACHE_SHA256"):
            env.pop(key, None)
    session = Session(ctx.adapter, bundle, env)

    record: dict[str, Any] = {
        "scenario": scenario.id, "condition": ctx.condition, "mode": args.mode,
        "runtime_class": args.runtime_class, **ctx.identities, "rom_sha256": scenario.rom_sha256,
        "scenario_file": str(scenario.path), "bundle": os.path.relpath(bundle, ctx.out),
        "start_kind": scenario.start["kind"], "started": utc_now(), "ended": None,
        "exit_class": "clean", "error": None, "load": None, "frames_run": 0,
        "wall_seconds": None, "fps": None, "checkpoints": [], "end_state_sha256": None,
        "hires": None, "orphans": [], "gates": [], "log_flags": [],
    }
    flags: set[str] = set()
    started = time.monotonic()
    stage_dir = Path(tempfile.mkdtemp(prefix="regression-stage-"))
    launched = False
    try:
        inputs = resolve_inputs(scenario, ctx)
        launched = True
        execute(scenario, ctx, session, record, inputs, stage_dir)
    except RunFailure as failure:
        record["exit_class"] = failure.exit_class
        record["error"] = str(failure)
        if failure.exit_class == "load-failed":
            record["load"] = "failed"
    finally:
        shutil.rmtree(stage_dir, ignore_errors=True)
        if launched:
            stop = session.call("stop", timeout=60)
            if "QUIT timed out" in stop.stdout or stop.timed_out:
                flags.add("quit-timeout")
                if record["exit_class"] == "clean":
                    record["exit_class"] = "forced"
            elif not stop.ok:
                flags.add("stop-failed")
                if record["exit_class"] == "clean":
                    record["exit_class"] = "nonzero"
                    record["error"] = f"stop: {stop.describe()}"
            reaped, remaining = reap_survivors(bundle, ctx.adapter, args.orphan_grace)
            record["orphans"] = reaped
            if reaped:
                flags.add("orphan-reaped")
                if record["exit_class"] in ("clean", "forced"):
                    record["exit_class"] = "orphan-reaped"
            if remaining:
                flags.add("orphan-survived")
                record["orphans_remaining"] = remaining

    record["ended"] = utc_now()
    record["wall_seconds"] = round(time.monotonic() - started, 3)
    if session.step_seconds > 0 and record["frames_run"]:
        record["fps"] = round(record["frames_run"] / session.step_seconds, 2)

    log_path = bundle / "logs" / "retroarch.log"
    log_text = log_path.read_text(errors="replace") if log_path.is_file() else ""
    if launched:
        record["hires"] = parse_hires(log_text)
        if record["hires"]["cache_load_failed"]:
            flags.add("hires-cache-load-failed")
        if args.mode == "on" and not record["hires"]["summary_count"]:
            flags.add("hires-summary-missing")
    if STATE_LOAD_FAILED in log_text:
        flags.add("state-load-failed")
    end_reason = bundle / "logs" / "session.end-reason"
    if end_reason.is_file() and "ttl" in end_reason.read_text():
        flags.add("ttl-expired")
    if any(cp.get("uniform_black") for cp in record["checkpoints"]):
        flags.add("uniform-black-capture")
    record["log_flags"] = sorted(flags)
    return record


# --------------------------------------------------------------------------
# Expectations and gates


def load_expectation(scenario: Scenario, ctx: RunContext) -> tuple[dict[str, Any], str | None]:
    """Sibling expectation files (explicit golden runs) override embedded `expect`."""
    if ctx.args.expectations:
        path = ctx.args.expectations / f"{scenario.id}.expect.json"
        if path.is_file():
            entry = (json.loads(path.read_text()).get("conditions") or {}).get(ctx.condition)
            if entry:
                return entry, str(path)
    entry = (scenario.data.get("expect") or {}).get(ctx.condition)
    return (entry, "scenario") if entry else ({}, None)


def gate(name: str, status: str, detail: str = "") -> dict[str, str]:
    return {"name": name, "status": status, "detail": detail}


def compare(name: str, observed: str | None, expected: str | None, on_mismatch: str = "fail") -> dict[str, str]:
    if expected is None:
        return gate(name, "skip", "no expectation")
    if observed is None:
        return gate(name, "fail", f"not observed; expected {expected}")
    if observed == expected:
        return gate(name, "pass")
    return gate(name, on_mismatch, f"observed {observed}, expected {expected}")


def evaluate(scenario: Scenario, record: dict[str, Any], expected: dict[str, Any], mode: str) -> list[dict[str, str]]:
    gates = [gate("exit", "pass" if record["exit_class"] == "clean" else "fail",
                  "" if record["exit_class"] == "clean" else f"{record['exit_class']}: {record['error'] or ''}".strip())]
    if scenario.start["kind"] == "state":
        load = record["load"]
        gates.append(gate("load", {"ok": "pass", "failed": "fail"}.get(load, "skip"),
                          {"ok": "", "failed": record["error"] or ""}.get(load, "not reached")))
    observed_by_frame = {cp["after_frames"]: cp for cp in record["checkpoints"]}
    expected_cps = expected.get("checkpoints") or {}
    for cp in scenario.checkpoints:
        seen = observed_by_frame.get(cp.after_frames)
        want = expected_cps.get(str(cp.after_frames)) or {}
        for cue in cp.ram:
            name = f"ram@{cp.after_frames}:{cue.name}"
            if seen is None:
                gates.append(gate(name, "skip", "not reached"))
            elif cue.name in seen["ram_errors"]:
                gates.append(gate(name, "fail", f"read failed: {seen['ram_errors'][cue.name]}"))
            else:
                gates.append(compare(name, seen["ram"].get(cue.name), (want.get("ram") or {}).get(cue.name)))
        if cp.capture:
            name = f"capture@{cp.after_frames}"
            if seen is None:
                gates.append(gate(name, "skip", "not reached"))
            elif mode == "off":
                # ADR-0002: digest equality is a valid gate only feature-off.
                gates.append(compare(name, seen["capture_sha256"], want.get("capture_sha256")))
            else:
                # ADR-0004: a changed hi-res-on capture is nominated for review, never failed.
                reference = want.get("reference_capture_sha256") or want.get("capture_sha256")
                gates.append(compare(name, seen["capture_sha256"], reference, on_mismatch="review"))
    if scenario.save_end:
        observed, wanted = record["end_state_sha256"], expected.get("end_state_sha256")
        if mode == "off":
            gates.append(compare("end_state", observed, wanted))
        else:
            verdict = ("no expectation" if wanted is None else "not observed" if observed is None
                       else "matches" if observed == wanted else f"differs: observed {observed}, expected {wanted}")
            gates.append(gate("end_state", "skip", f"report-only in on mode; {verdict}"))
    hires = record["hires"]
    if hires is not None:
        if mode == "on":
            loaded = not hires["cache_load_failed"] and hires["provider"] == "on"
            gates.append(gate("hires.provider", "pass" if loaded else "fail",
                              "" if loaded else ("cache load failed" if hires["cache_load_failed"]
                                                 else f"no provider=on summary (provider {hires['provider']})")))
            entries = hires["entries"]
            gates.append(gate("hires.entries", "pass" if (entries or 0) >= 1 else "fail", f"entries={entries}"))
            # A replacement is served when bound at upload (hits) or at draw time
            # (draw_hits); scenes served entirely at upload, such as boot logos,
            # report no draw-time hits.
            # A session that looked up no texture at all (a few frames after
            # power-on) has nothing to serve; misses without hits still fail.
            served = max(hires["hits"] or 0, hires["draw_hits"] or 0)
            looked_up = served + (hires["misses"] or 0)
            detail = f"hits={hires['hits']} draw_hits={hires['draw_hits']} misses={hires['misses']}"
            gates.append(gate("hires.served", "pass" if served >= 1 else "skip" if looked_up == 0 else "fail",
                              detail if looked_up else "no texture lookups"))
        else:
            active = hires["provider"] == "on"
            gates.append(gate("hires.inert", "fail" if active else "pass",
                              "provider=on in a feature-off session" if active else ""))
    return gates


def expectation_entry(scenario: Scenario, record: dict[str, Any], mode: str) -> dict[str, Any]:
    capture_key = "capture_sha256" if mode == "off" else "reference_capture_sha256"
    checkpoints = {}
    for cp in record["checkpoints"]:
        entry: dict[str, Any] = {}
        if cp["ram"]:
            entry["ram"] = dict(cp["ram"])
        if cp["capture_sha256"]:
            entry[capture_key] = cp["capture_sha256"]
        checkpoints[str(cp["after_frames"])] = entry
    entry = {
        "recorded": record["ended"],
        "core_sha256": record["core_sha256"],
        "frontend_sha256": record["frontend_sha256"],
        "rom_sha256": record["rom_sha256"],
        "pack_sha256": record["pack_sha256"],
        "checkpoints": checkpoints,
    }
    if scenario.save_end:
        entry["end_state_sha256"] = record["end_state_sha256"]
    return entry


def record_expectation(scenario: Scenario, record: dict[str, Any], ctx: RunContext) -> str:
    complete = record["exit_class"] == "clean" and not any(
        cp["ram_errors"] for cp in record["checkpoints"])
    if not complete:
        return "not recorded: run was not clean"
    path = ctx.out / "expectations" / f"{scenario.id}.expect.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    document = json.loads(path.read_text()) if path.is_file() else {
        "schema": EXPECT_SCHEMA, "scenario": scenario.id, "conditions": {}}
    document["conditions"][ctx.condition] = expectation_entry(scenario, record, ctx.args.mode)
    path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n")
    return os.path.relpath(path, ctx.out)


# --------------------------------------------------------------------------
# Results and report


def read_results(path: Path) -> list[dict[str, Any]]:
    if not path.is_file():
        return []
    return [json.loads(line) for line in path.read_text().splitlines() if line.strip()]


def latest_records(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    latest: dict[tuple[str, str], dict[str, Any]] = {}
    for rec in records:
        latest[(rec["scenario"], rec["condition"])] = rec
    return list(latest.values())


def outcome(rec: dict[str, Any]) -> str:
    statuses = {g["status"] for g in rec["gates"]}
    return "fail" if "fail" in statuses else "review" if "review" in statuses else "pass"


def write_report(out: Path, records: list[dict[str, Any]]) -> None:
    lines = ["# Regression scenario report", ""]
    counts = {k: sum(outcome(r) == k for r in records) for k in ("pass", "review", "fail")}
    lines += [f"Generated {utc_now()}. Scenarios: {len(records)}; pass {counts['pass']}, "
              f"review {counts['review']}, fail {counts['fail']}.", ""]
    for rec in records[:1]:
        lines += [f"Core `{rec['core_sha256']}`, frontend `{rec['frontend_sha256']}`, "
                  f"pack `{rec['pack_sha256'] or '-'}`.", ""]
    lines += ["## Summary", "", "| Scenario | Condition | Outcome | Exit | Frames | FPS | Gates (pass/fail/review/skip) |",
              "|---|---|---|---|---:|---:|---|"]
    for rec in records:
        tally = "/".join(str(sum(g["status"] == s for g in rec["gates"])) for s in ("pass", "fail", "review", "skip"))
        lines.append(f"| {rec['scenario']} | {rec['condition']} | {outcome(rec)} | {rec['exit_class']} | "
                     f"{rec['frames_run']} | {rec['fps'] if rec['fps'] is not None else '-'} | {tally} |")
    failures = [(r, g) for r in records for g in r["gates"] if g["status"] == "fail"]
    lines += ["", "## Failures", ""]
    lines += [f"- `{r['scenario']}` {r['condition']} **{g['name']}**: {g['detail']} (bundle `{r['bundle']}`)"
              for r, g in failures] or ["None."]
    lines += ["", "## Review queue", "",
              "Changed hi-res-on captures are nominated for visual review (ADR-0004); they never fail a run.", ""]
    reviews = []
    for rec in records:
        by_frame = {str(cp["after_frames"]): cp for cp in rec["checkpoints"]}
        for g in rec["gates"]:
            if g["status"] == "review":
                frame = g["name"].partition("@")[2]
                path = (by_frame.get(frame) or {}).get("capture_path")
                reviews.append(f"- `{rec['scenario']}` {rec['condition']} {g['name']}: `{path}` ({g['detail']})")
    lines += reviews or ["None."]
    report_only = [(r, g) for r in records for g in r["gates"]
                   if g["name"] == "end_state" and "differs" in g["detail"] and g["status"] == "skip"]
    if report_only:
        lines += ["", "## Report-only differences", ""]
        lines += [f"- `{r['scenario']}` {r['condition']} end state {g['detail']}" for r, g in report_only]
    flagged = [r for r in records if r["log_flags"]]
    lines += ["", "## Log flags", ""]
    lines += [f"- `{r['scenario']}` {r['condition']}: {', '.join(r['log_flags'])}" for r in flagged] or ["None."]
    timed = [r for r in records if r["fps"] is not None]
    if timed:
        lines += ["", "## Performance", "",
                  "Frames per second while stepping (reported, not gated).", "",
                  "| Scenario | Condition | Frames | Step FPS | Wall s |", "|---|---|---:|---:|---:|"]
        lines += [f"| {r['scenario']} | {r['condition']} | {r['frames_run']} | {r['fps']} | {r['wall_seconds']} |"
                  for r in timed]
    (out / "report.md").write_text("\n".join(lines) + "\n")


# --------------------------------------------------------------------------
# Entry point


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--scenarios", type=Path, nargs="+", required=True, help="scenario files or directories")
    parser.add_argument("--blobs", type=Path, help="blob store with states/, savefiles/, roms/")
    rom = parser.add_mutually_exclusive_group()
    rom.add_argument("--rom", type=Path, help="ROM for every scenario (sha must match)")
    rom.add_argument("--rom-map", type=Path, help="JSON object mapping ROM sha256 to a path")
    parser.add_argument("--core", type=Path, required=True)
    parser.add_argument("--retroarch-bin", type=Path, required=True)
    parser.add_argument("--base-config", type=Path, required=True)
    parser.add_argument("--extra-append-config", type=Path,
                        help="config loaded after the adapter's generated settings (e.g. capture profile)")
    parser.add_argument("--mode", choices=("off", "on"), default="off")
    parser.add_argument("--pack", type=Path, help="hi-res .phrb package (required with --mode on)")
    parser.add_argument("--runtime-class", required=True, help="caller-declared runtime class label")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--select", action="append", default=[],
                        help="id glob or tag=value[,value]; repeated filters must all match")
    parser.add_argument("--record-expectations", action="store_true",
                        help="write observed values to OUT/expectations/<id>.expect.json")
    parser.add_argument("--expectations", type=Path, help="directory of <id>.expect.json files to gate against")
    parser.add_argument("--step-chunk", type=int, default=120, help="largest single STEP_FRAME (default 120)")
    parser.add_argument("--ttl", type=int, help="session TTL seconds (default scales with program length)")
    parser.add_argument("--resume", action="store_true", help="skip scenarios already recorded in OUT")
    parser.add_argument("--adapter", type=Path, default=DEFAULT_ADAPTER, help=argparse.SUPPRESS)
    parser.add_argument("--orphan-grace", type=float, default=3.0,
                        help="seconds to wait for bundle processes to exit after stop")
    parser.add_argument("--hires-summary-interval", type=int, default=600,
                        help="frames between periodic hi-res summaries where the core supports them")
    args = parser.parse_args(argv)
    if args.step_chunk < 1:
        parser.error("--step-chunk must be positive")
    if args.mode == "on" and (not args.pack or not args.pack.is_file()):
        parser.error("--mode on requires an existing --pack")
    if "|" in str(args.out.resolve()):
        parser.error("--out must not contain '|'")
    for name in ("core", "retroarch_bin", "base_config"):
        if not getattr(args, name).is_file():
            parser.error(f"--{name.replace('_', '-')} not found: {getattr(args, name)}")
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        scenarios = load_scenarios(args.scenarios)
    except ScenarioError as err:
        print(f"[regression] {err}", file=sys.stderr)
        return 2
    selected = [s for s in scenarios if all(matches_filter(s, f) for f in args.select)]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    results_path = out / "results.jsonl"
    condition = f"{args.mode}:{args.runtime_class}"
    done = {(r["scenario"], r["condition"]) for r in read_results(results_path)} if args.resume else set()
    pending = [s for s in selected if (s.id, condition) not in done]
    print(f"[regression] {len(selected)} selected, {len(selected) - len(pending)} already recorded, "
          f"condition {condition}")

    adapter = args.adapter.resolve()
    ctx = RunContext(args=args, adapter=adapter, out=out, condition=condition, identities={
        "core_sha256": sha256_file(args.core),
        "frontend_sha256": sha256_file(args.retroarch_bin),
        "pack_sha256": sha256_file(args.pack) if args.mode == "on" else None,
    })
    if pending:
        doctor = subprocess.run([str(adapter), "doctor"], capture_output=True, text=True, check=False)
        if doctor.returncode != 0:
            print(f"[regression] host is not idle; refusing to start:\n{doctor.stdout}{doctor.stderr}",
                  file=sys.stderr)
            return 2

    for scenario in pending:
        record = run_scenario(scenario, ctx)
        expected, source = load_expectation(scenario, ctx)
        record["expectation_source"] = source
        record["expectation_core_sha256"] = expected.get("core_sha256")
        record["gates"] = evaluate(scenario, record, expected, args.mode)
        if args.record_expectations:
            record["expectation_recorded"] = record_expectation(scenario, record, ctx)
        with results_path.open("a") as results:
            results.write(json.dumps(record, sort_keys=True) + "\n")
        print(f"[regression] {scenario.id}: {outcome(record)} ({record['exit_class']})")
        if "orphan-survived" in record["log_flags"]:
            print("[regression] a frontend process survived reaping; stopping the run.", file=sys.stderr)
            break

    records = latest_records(read_results(results_path))
    write_report(out, records)
    failed = any(outcome(r) == "fail" for r in records)
    print(f"[regression] report: {out / 'report.md'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
