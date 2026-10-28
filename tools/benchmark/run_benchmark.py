#!/usr/bin/env python3
"""Serial, state-matched RetroArch throughput measurements (standard library only)."""
from __future__ import annotations

import argparse
import fcntl
import itertools
import json
import math
import os
import platform
import re
import shlex
import shutil
import signal
import socket
import statistics
import subprocess
import sys
import time
import uuid
import zipfile
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from regression import run_scenarios as regression

ENGINES = {
    "dynamic_recompiler": "Dynamic Recompiler",
    "cached_interpreter": "Cached Interpreter",
    "pure_interpreter": "Pure Interpreter",
}
SCHEMA = "paralleish-benchmark/1"
# Explicit normal-playback controls: never toggle fast-forward or STEP_FRAME.
CONTROLS = {
    "video_vsync": "false", "video_adaptive_vsync": "false", "video_threaded": "false",
    "audio_sync": "false", "audio_enable": "false", "audio_driver": "null",
    "microphone_enable": "false", "vrr_runloop_enable": "false",
    "run_ahead_enabled": "false", "run_ahead_frames": "0",
    "preemptive_frames_enable": "false", "preemptive_frames": "0",
    "video_frame_delay": "0", "video_frame_delay_auto": "false", "rewind_enable": "false",
    "fastforward_ratio": "0.0", "slowmotion_ratio": "1.0", "pause_nonactive": "false",
    "pause_on_disconnect": "false", "menu_throttle_framerate": "false",
    "config_save_on_exit": "false", "auto_overrides_enable": "false",
    "auto_remaps_enable": "false", "auto_shaders_enable": "false", "video_shader_enable": "false",
    "game_specific_options": "false", "global_core_options": "true",
    "savestate_auto_load": "true", "savestate_auto_save": "false", "savestate_auto_index": "false",
    "savestate_thumbnail_enable": "false", "sort_savefiles_enable": "false",
    "sort_savestates_enable": "false", "sort_savefiles_by_content_enable": "false",
    "sort_savestates_by_content_enable": "false", "history_list_enable": "false",
    "content_runtime_log": "false", "content_runtime_log_aggregate": "false",
    "autosave_interval": "0", "cheevos_enable": "false", "state_slot": "0",
    "video_font_enable": "false", "menu_enable_widgets": "false", "confirm_quit": "false",
    "stdin_cmd_enable": "false", "network_cmd_enable": "false", "log_verbosity": "true",
    "libretro_log_level": "0",
}


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")


def config_text(values):
    # RetroArch quoted values have no portable escaping for embedded quotes/newlines.
    if any(any(c in str(v) for c in '\r\n"') for v in values.values()):
        raise ValueError("config values cannot contain quotes or newlines")
    return "".join(f'{k} = "{v}"\n' for k, v in values.items())


def merged_config(base, values):
    # Remove all old assignments: RetroArch can give the first duplicate precedence.
    keys = "|".join(re.escape(key) for key in values)
    base = re.sub(r"^[ \t]*(?:" + keys + r")[ \t]*=.*(?:\n|$)", "", base, flags=re.MULTILINE)
    return base + "\n" + config_text(values)


def paired(short, long):
    frames = long["frames"] - short["frames"]
    seconds = long["wall_seconds"] - short["wall_seconds"]
    if frames <= 0 or seconds <= 0 or not math.isfinite(seconds):
        raise ValueError("pair needs positive frame and wall-time differences")
    cpu = long["user_seconds"] + long["sys_seconds"] - short["user_seconds"] - short["sys_seconds"]
    if cpu < 0:
        raise ValueError("negative paired CPU time; increase run length")
    return {"frames": frames, "seconds": seconds, "fps": frames / seconds,
            "cpu_percent": 100 * cpu / seconds, "max_rss_kib": long["max_rss_kib"]}


def cell_key(row):
    return tuple(row[k] for k in ("scene", "engine", "scale", "pack_sha256"))


def summarize(samples):
    groups = defaultdict(list)
    for row in samples:
        groups[cell_key(row)].append(row)
    cells = []
    for rows in groups.values():
        fps = [r["fps"] for r in rows]
        cell = {k: rows[0][k] for k in ("scene", "engine", "scale", "pack_sha256")}
        cell.update(savefile_sha256=rows[0].get("savefile_sha256"), state_sha256=rows[0]["state_sha256"],
                    rom_content_sha256=rows[0]["rom_content_sha256"], repetitions=len(rows), median_fps=statistics.median(fps),
                    min_fps=min(fps), max_fps=max(fps))
        for metric in ("cpu_percent", "max_rss_kib"):
            values = [r[metric] for r in rows if r[metric] is not None]
            cell[metric] = statistics.median(values) if values else None
        cells.append(cell)
    return cells


def compare(a, b):
    """Disjoint observed ranges flag changes; this is not a significance test."""
    if a["schema"] != SCHEMA or b["schema"] != SCHEMA:
        raise ValueError("unsupported summary schema")
    if a["method"] != b["method"]:
        raise ValueError("cannot compare Linux paired and Android window measurements")
    left, right = ({cell_key(c): c for c in s["cells"]} for s in (a, b))
    for field in (("short_frames", "long_frames") if a["method"] == "linux-paired"
                  else ("warmup_seconds", "window_seconds")):
        if a[field] != b[field]:
            raise ValueError(f"different measurement setting: {field}")
    result = []
    for key in sorted(left.keys() | right.keys(), key=str):
        old, new = left.get(key), right.get(key)
        row = dict(zip(("scene", "engine", "scale", "pack_sha256"), key))
        if old is None or new is None:
            row["status"] = "added" if old is None else "removed"
        else:
            if any(old.get(k) != new.get(k) for k in ("state_sha256", "savefile_sha256", "rom_content_sha256")):
                raise ValueError(f"different input bytes for cell {key}")
            delta = new["median_fps"] - old["median_fps"]
            row.update(status="matched", delta_fps=delta, delta_percent=100 * delta / old["median_fps"],
                       beyond_spread=(new["min_fps"] > old["max_fps"] or new["max_fps"] < old["min_fps"]),
                       before=old, after=new)
        result.append(row)
    return result


def parse_time(text):
    def number(pattern):
        match = re.search(pattern, text, re.MULTILINE)
        if not match:
            raise ValueError(f"missing GNU time metric: {pattern}")
        return float(match[1])
    # GNU time -v emits m:ss or h:mm:ss with a locale-independent LC_ALL=C.
    match = re.search(r"Elapsed \(wall clock\) time .*?:\s*([\d:.]+)\s*$", text, re.MULTILINE)
    if not match:
        raise ValueError("missing GNU time elapsed metric")
    wall = 0.0
    for part in match[1].split(":"):
        wall = wall * 60 + float(part)
    return {"wall_seconds": wall, "user_seconds": number(r"User time \(seconds\): ([\d.]+)"),
            "sys_seconds": number(r"System time \(seconds\): ([\d.]+)"),
            "max_rss_kib": number(r"Maximum resident set size \(kbytes\): (\d+)")}


def qualify_log(text, engine, scale, pack):
    selected = re.findall(r"Starting R4300 emulator: ([^\r\n]+)", text)
    if not selected or any(s.strip() != ENGINES[engine] for s in selected):
        raise ValueError(f"requested engine {engine} not confirmed in core log")
    if not any("Auto-loading" in line and "succeeded" in line for line in text.splitlines()):
        raise ValueError("state autoload not confirmed")
    if scale != "1x" and f"Enabling upscaling: {scale}" not in text:
        raise ValueError("upscale not confirmed")
    hires = regression.parse_hires(text)
    if pack and (hires["cache_load_failed"] or not hires["summary_count"]):
        raise ValueError("pack loading not confirmed")
    return {"engine_confirmed": True, "autoload_confirmed": True, "hires": hires}


def parse_status(reply, sent, received):
    match = regression.STATUS_RE.search(reply.strip())
    if not match or match[1] != "PLAYING" or received < sent:
        raise ValueError(f"expected PLAYING GET_STATUS, got {reply!r}")
    return {"frame": int(match[2]), "t": (sent + received) / 2, "rtt": received - sent,
            "reply": reply}


def window_metrics(start, end):
    frames, seconds = end["frame"] - start["frame"], end["t"] - start["t"]
    if frames <= 0 or seconds <= 0:
        raise ValueError("frame counter reset/stalled or nonpositive interval")
    return {"frames": frames, "seconds": seconds, "fps": frames / seconds}


def sample_window(status, seconds, clock=time.monotonic, sleep=time.sleep):
    """Schedule against an absolute deadline; network RTT stays in raw endpoints."""
    start = status()
    deadline = start["t"] + seconds
    sleep(max(0, deadline - clock()))
    end = status()
    return window_metrics(start, end) | {"start": start, "end": end}


def prepare_scene(scene, args, root):
    root.mkdir(parents=True)
    scenario = regression.Scenario(
        path=Path(scene["scenario"]), data={}, id=scene["scene"], rom_sha256=scene["rom_sha256"],
        start={"kind": "state", "state_sha256": scene["state_sha256"],
               "savefile_sha256": scene.get("savefile_sha256")},
        program=(), checkpoints=(), save_end=False, tags={})
    ctx = regression.RunContext(args, Path(), root, "benchmark", {})
    inputs = regression.resolve_inputs(scenario, ctx, root)
    rom = inputs.rom
    if zipfile.is_zipfile(rom):
        with zipfile.ZipFile(rom) as archive:
            members = [m for m in archive.namelist() if m.lower().endswith((".z64", ".n64", ".v64"))]
            if len(members) != 1:
                raise ValueError(f"ROM archive must contain exactly one ROM: {rom}")
            rom = root / Path(members[0]).name
            with archive.open(members[0]) as source, rom.open("wb") as target:
                shutil.copyfileobj(source, target)
    return {"rom": rom, "state": inputs.state, "savefile": inputs.savefile,
            "rom_content_sha256": regression.sha256_file(rom)}


def options(engine, scale, pack):
    return {"parallel-n64-cpucore": engine, "parallel-n64-gfxplugin": "parallel",
            "parallel-n64-rspplugin": "cxd4", "parallel-n64-parallel-rdp-synchronous": "enabled",
            "parallel-n64-parallel-rdp-upscaling": scale,
            "parallel-n64-parallel-rdp-hirestex": "enabled" if pack else "disabled",
            "parallel-n64-parallel-rdp-native-tex-rect": "enabled",
            "parallel-n64-parallel-rdp-native-texture-lod": "enabled"}


def setup_run(out, inputs):
    out.mkdir(parents=True)
    for folder in ("states", "saves", "system", "screenshots", "logs", "playlists", "cache"):
        (out / folder).mkdir()
    shutil.copyfile(inputs["state"], out / "states" / (inputs["rom"].stem + ".state.auto"))
    if inputs["savefile"]:
        shutil.copyfile(inputs["savefile"], out / "saves" / (inputs["rom"].stem + ".srm"))
    return CONTROLS | {"core_options_path": str(out / "core-options.opt")} | {
        k: str(out / v) for k, v in (("savestate_directory", "states"), ("savefile_directory", "saves"),
        ("system_directory", "system"), ("screenshot_directory", "screenshots"), ("log_dir", "logs"),
        ("playlist_directory", "playlists"), ("cache_directory", "cache"))}


def linux_run(args, inputs, engine, scale, pack, frames, out):
    cfg = setup_run(out, inputs)
    cfg.update(video_driver="vulkan", video_context_driver=args.video_context,
               input_driver="null", input_joypad_driver="null", video_fullscreen="false",
               video_windowed_fullscreen="false", video_max_frame_latency="1",
               video_window_custom_size_enable="true", video_windowed_position_width="1280",
               video_windowed_position_height="960", video_window_auto_width_max="1280",
               video_window_auto_height_max="960", video_scale="4.0", aspect_ratio_index="22",
               video_force_aspect="true", custom_viewport_width="1280", custom_viewport_height="960",
               custom_viewport_x="0", custom_viewport_y="0")
    (out / "core-options.opt").write_text(config_text(options(engine, scale, pack)))
    (out / "append.cfg").write_text(config_text(cfg))
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("PARALLEL_", "RETROARCH_", "VK_", "MESA_"))}
    env.update(LC_ALL="C", XDG_CACHE_HOME=str(args.out / "cache"),
               RETROARCH_VIDEO_CONTEXT_DRIVER=args.video_context)
    if args.vulkan_icd:
        env["VK_ICD_FILENAMES"] = str(args.vulkan_icd)
    if pack:
        env["PARALLEL_RDP_HIRES_CACHE_PATH"] = str(pack)
    argv = [str(args.gnu_time), "-v", "-o", str(out / "time.txt"), str(args.retroarch_bin),
            "--verbose", "--config", str(args.base_config), "--appendconfig", str(out / "append.cfg"),
            "--max-frames", str(frames), "-L", str(args.core), str(inputs["rom"])]
    write_json(out / "command.json", {"argv": argv, "environment": {k: env[k] for k in env
               if k.startswith(("PARALLEL_", "RETROARCH_", "VK_", "XDG_CACHE")) or k == "LC_ALL"}})
    started = time.monotonic()
    with (out / "retroarch.log").open("w") as log:
        proc = subprocess.Popen(argv, env=env, stdin=subprocess.DEVNULL, stdout=log,
                                stderr=subprocess.STDOUT, start_new_session=True)
        try:
            rc = proc.wait(timeout=args.timeout)
        finally:
            if proc.poll() is None:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait()
    result = {"frames": frames, "returncode": rc, "parent_wall_seconds": time.monotonic() - started,
              "log": str(out / "retroarch.log")}
    result.update(parse_time((out / "time.txt").read_text()))
    write_json(out / "result.json", result)
    if rc:
        raise ValueError(f"RetroArch exited {rc}; see {out}")
    result.update(qualify_log((out / "retroarch.log").read_text(errors="replace"), engine, scale, pack))
    write_json(out / "result.json", result)
    return result


class Android:
    """Use a separate external directory/config; never replace the installed core."""
    def __init__(self, args):
        self.args = args
        self.remote = args.android_root.rstrip("/") + "/bench-" + uuid.uuid4().hex

    def adb(self, *argv, check=True):
        return subprocess.run([self.args.adb, "-s", self.args.android, *argv],
                              check=check, capture_output=True, timeout=self.args.timeout).stdout.decode().strip()

    def shell(self, *argv, check=True):
        return self.adb("shell", shlex.join(argv), check=check)

    def status(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(3)
            sock.connect((self.args.status_host, self.args.status_port))
            sent = time.monotonic()
            sock.send(b"GET_STATUS")
            reply = sock.recv(65535).decode()
            return parse_status(reply, sent, time.monotonic())

    def resources(self):
        pid = self.shell("pidof", self.args.android_package).split()[0]
        stat = self.shell("run-as", self.args.android_package, "cat", f"/proc/{pid}/stat")
        status = self.shell("run-as", self.args.android_package, "cat", f"/proc/{pid}/status")
        rss = re.search(r"^VmHWM:\s+(\d+) kB$", status, re.MULTILINE)
        if not rss:
            raise ValueError("Android /proc status lacks VmHWM")
        return {"pid": pid, "t": time.monotonic(), "ticks": parse_proc_stat(stat),
                "max_rss_kib": int(rss[1])}

    def run(self, inputs, engine, scale, pack, out):
        try:
            return self._run(inputs, engine, scale, pack, out)
        finally:
            # Also cover staging/config failures before activity launch.
            self.shell("am", "force-stop", self.args.android_package)
            if out.exists():
                self.adb("pull", self.remote + "/" + out.name + "/logs/retroarch.log",
                         str(out / "retroarch.log"), check=False)
            self.shell("rm", "-rf", self.remote)

    def _run(self, inputs, engine, scale, pack, out):
        cfg = setup_run(out, inputs)
        remote = self.remote + "/" + out.name
        self.shell("mkdir", "-p", remote)
        self.adb("push", str(inputs["rom"]), remote + "/" + inputs["rom"].name)
        # All writable paths in the copied config must point to device storage.
        for key, value in list(cfg.items()):
            if str(out) in value:
                cfg[key] = value.replace(str(out), remote)
        cfg.update(network_cmd_enable="true", network_cmd_port=str(self.args.status_port),
                   video_driver="vulkan", input_overlay_enable="false", log_to_file="true",
                   log_to_file_timestamp="false", video_gpu_screenshot="false")
        opts = options(engine, scale, pack)
        if pack:
            # The core searches this documented system-directory layout on Android.
            with inputs["rom"].open("rb") as rom_file:
                rom_header = rom_file.read(0x40)
            header = rom_header[0x20:0x34].rstrip(b"\0 ").decode("ascii")
            if rom_header[:4] != b"\x80\x37\x12\x40":
                raise ValueError("Android packs require a big-endian .z64 ROM")
            dest = remote + "/system/parallel-n64/hires/" + header + ".phrb"
            self.shell("mkdir", "-p", remote + "/system/parallel-n64/hires")
            self.adb("push", str(pack), dest)
        # Write a standalone copy with benchmark assignments replacing all old values.
        (out / "session.cfg").write_text(merged_config(self.args.base_config.read_text(), cfg))
        (out / "core-options.opt").write_text(config_text(opts))
        for folder in ("states", "saves", "system", "logs", "cache", "screenshots", "playlists"):
            self.shell("mkdir", "-p", remote + "/" + folder)
        for name in ("states", "saves", "core-options.opt", "session.cfg"):
            self.adb("push", str(out / name), remote + "/")
        argv = ["am", "start", "-n", self.args.android_activity]
        extras = {"ROM": remote + "/" + inputs["rom"].name, "LIBRETRO": str(self.args.core),
                  "CONFIGFILE": remote + "/session.cfg", "DATADIR": self.args.android_datadir,
                  "SDCARD": "/storage/emulated/0", "EXTERNAL": self.args.android_root}
        for k, v in extras.items():
            argv += ["-e", k, v]
        write_json(out / "command.json", {"adb_serial": self.args.android, "argv": argv})
        self.shell("input", "keyevent", "KEYCODE_WAKEUP")
        self.shell(*argv)
        deadline = time.monotonic() + self.args.timeout
        ready = None
        while time.monotonic() < deadline:
            try:
                ready = self.status()
                break
            except (OSError, ValueError):
                time.sleep(.2)
        if ready is None:
            raise ValueError("Android never reported PLAYING")
        time.sleep(self.args.warmup_seconds)
        ticks_per_second = int(self.shell("getconf", "CLK_TCK"))
        before = self.resources()
        result = sample_window(self.status, self.args.window_seconds)
        after = self.resources()
        if before["pid"] != after["pid"] or after["ticks"] < before["ticks"]:
            raise ValueError("Android process changed during measurement")
        result.update(cpu_percent=100 * (after["ticks"] - before["ticks"]) / ticks_per_second /
                      (after["t"] - before["t"]), max_rss_kib=after["max_rss_kib"],
                      resources_start=before, resources_end=after, ticks_per_second=ticks_per_second)
        self.shell("am", "force-stop", self.args.android_package)
        self.adb("pull", remote + "/logs/retroarch.log", str(out / "retroarch.log"))
        result.update(qualify_log((out / "retroarch.log").read_text(errors="replace"), engine, scale, pack))
        write_json(out / "result.json", result)
        return result


def parse_proc_stat(text):
    # comm is parenthesized and can contain spaces or parentheses. utime/stime are fields 14/15.
    fields = text[text.rindex(")") + 2:].split()
    return int(fields[11]) + int(fields[12])


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--compare", nargs=2, type=Path, metavar=("A.json", "B.json"))
    p.add_argument("--core", type=Path, help="Linux core file, or installed Android core's absolute device path")
    p.add_argument("--retroarch-bin", default="retroarch")
    p.add_argument("--base-config", type=Path)
    p.add_argument("--rom-map", type=Path)
    p.add_argument("--blobs", type=Path)
    p.add_argument("--manifest", type=Path, default=Path(__file__).with_name("scenes.json"))
    p.add_argument("--scenes", nargs="+", help="scene IDs; default all")
    p.add_argument("--engines", nargs="+", choices=ENGINES, default=["dynamic_recompiler"])
    p.add_argument("--scales", nargs="+", choices=["1x", "2x", "4x", "8x"], default=["1x"])
    p.add_argument("--pack-map", type=Path, help="JSON: ROM SHA-256 -> local .phrb; includes off and on cells")
    p.add_argument("--repetitions", type=int, default=3)
    p.add_argument("--short-frames", type=int, default=16)
    p.add_argument("--long-frames", type=int, default=1216)
    p.add_argument("--out", type=Path, help="new output directory (must not exist)")
    p.add_argument("--timeout", type=float, default=900)
    p.add_argument("--gnu-time", type=Path, default=Path("/usr/bin/time"))
    p.add_argument("--video-context", default="headless_vk")
    p.add_argument("--vulkan-icd", type=Path, help="explicit Vulkan ICD JSON")
    p.add_argument("--android", metavar="ADB_SERIAL")
    p.add_argument("--adb", default="adb")
    p.add_argument("--status-host", help="device IP reachable via UDP (adb forward is TCP only)")
    p.add_argument("--status-port", type=int, default=55355)
    p.add_argument("--android-package", default="com.retroarch")
    p.add_argument("--android-activity", default="com.retroarch/com.retroarch.browser.retroactivity.RetroActivityFuture")
    p.add_argument("--android-datadir", default="/data/data/com.retroarch")
    p.add_argument("--android-root", default="/sdcard/Android/data/com.retroarch/files/benchmark")
    p.add_argument("--warmup-seconds", type=float, default=5)
    p.add_argument("--window-seconds", type=float, default=23)
    return p


def main(argv=None):
    p = parser()
    args = p.parse_args(argv)
    if args.compare:
        print(json.dumps(compare(*(json.loads(f.read_text()) for f in args.compare)), indent=2))
        return
    if not all((args.core, args.base_config, args.rom_map, args.blobs, args.out)):
        p.error("--core, --base-config, --rom-map, --blobs and --out are required")
    if (args.repetitions < 1 or args.short_frames < 1 or args.long_frames <= args.short_frames
            or args.timeout <= 0 or args.window_seconds <= 0 or args.warmup_seconds < 0):
        p.error("invalid repetition, frame or time bounds")
    if args.android and not args.status_host:
        p.error("--android needs --status-host (UDP device address)")
    for key in ("base_config", "rom_map", "blobs", "manifest", "out", "vulkan_icd"):
        value = getattr(args, key)
        if value:
            setattr(args, key, value.resolve())
    if not args.android:
        args.core = args.core.resolve(strict=True)
        executable = shutil.which(args.retroarch_bin)
        if not executable:
            p.error("RetroArch binary not found")
        args.retroarch_bin = Path(executable).resolve()
    args.rom = None  # regression resolve_inputs selects the explicit ROM map.
    manifest = json.loads(args.manifest.read_text())
    if manifest["schema"] != "paralleish-benchmark-scenes/1":
        p.error("unsupported scenes schema")
    scenes = manifest["scenes"]
    ids = [s["scene"] for s in scenes]
    if len(set(ids)) != len(ids) or any(not regression.ID_RE.fullmatch(s) for s in ids):
        p.error("scene IDs must be unique safe filenames")
    if args.scenes:
        if set(args.scenes) - set(ids):
            p.error("unknown scene ID")
        scenes = [s for s in scenes if s["scene"] in args.scenes]
    pack_map = json.loads(args.pack_map.read_text()) if args.pack_map else {}
    packs = {sha: Path(path).resolve(strict=True) for sha, path in pack_map.items()}
    # Both the interactive adapter and the reference benchmark use this lock.
    with (Path(os.environ.get("TMPDIR", "/tmp")) / "parallel-n64-retroarch-runtime.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        android = Android(args) if args.android else None
        if android:
            if android.shell("pidof", args.android_package, check=False):
                raise ValueError("RetroArch already running on Android")
        elif subprocess.run(["pgrep", "-x", "retroarch"], stdout=subprocess.DEVNULL, check=False).returncode == 0:
            raise ValueError("another RetroArch is active")
        args.out.mkdir(parents=True)  # Refuse accidental resume/mixing of binaries or settings.
        (args.out / "cache").mkdir()
        metadata = {"schema": SCHEMA, "method": "android-window" if android else "linux-paired",
                    "created": regression.utc_now(), "platform": platform.platform(),
                    "manifest_sha256": regression.sha256_file(args.manifest),
                    "base_config_sha256": regression.sha256_file(args.base_config),
                    "short_frames": args.short_frames, "long_frames": args.long_frames,
                    "warmup_seconds": args.warmup_seconds, "window_seconds": args.window_seconds,
                    "core_sha256": android.shell("run-as", args.android_package, "sha256sum", str(args.core)).split()[0]
                    if android else regression.sha256_file(args.core),
                    "frontend_sha256": None if android else regression.sha256_file(args.retroarch_bin),
                    "scenes": scenes, "cells": []}
        write_json(args.out / "summary.json", metadata)
        samples = []
        with (args.out / "runs.jsonl").open("x") as raw:
            def record(row):
                raw.write(json.dumps(row, allow_nan=False) + "\n")
                raw.flush()
            for scene in scenes:
                inputs = prepare_scene(scene, args, args.out / "inputs" / scene["scene"])
                pack = packs.get(scene["rom_sha256"])
                pack_sha = regression.sha256_file(pack) if pack else None
                for engine, scale, selected_pack in itertools.product(args.engines, args.scales, [None, pack] if pack else [None]):
                    identity = {"scene": scene["scene"], "engine": engine, "scale": scale,
                                "pack_sha256": pack_sha if selected_pack else None,
                                "rom_content_sha256": inputs["rom_content_sha256"],
                                "state_sha256": scene["state_sha256"], "savefile_sha256": scene.get("savefile_sha256")}
                    tag = f'{scene["scene"]}-{engine}-{scale}-' + (pack_sha[:12] if selected_pack else "off")
                    if not android:
                        warm = linux_run(args, inputs, engine, scale, selected_pack, args.short_frames,
                                         args.out / "runs" / (tag + "-warmup"))
                        record(identity | {"kind": "warmup", "raw": warm})
                    for rep in range(args.repetitions):
                        out = args.out / "runs" / f"{tag}-r{rep + 1}"
                        if android:
                            metrics = android.run(inputs, engine, scale, selected_pack, out)
                        else:
                            pair = {}
                            order = [args.short_frames, args.long_frames]
                            for frames in reversed(order) if rep % 2 else order:
                                run = linux_run(args, inputs, engine, scale, selected_pack, frames,
                                                out / str(frames))
                                record(identity | {"kind": "process", "repetition": rep + 1, "raw": run})
                                pair[frames] = run
                            metrics = paired(pair[args.short_frames], pair[args.long_frames])
                        row = identity | {"kind": "sample", "repetition": rep + 1} | metrics
                        record(row)
                        samples.append(row)
                        metadata["cells"] = summarize(samples)
                        write_json(args.out / "summary.json", metadata)
                        print(f'{tag} r{rep + 1}: {row["fps"]:.2f} fps', flush=True)
        metadata["complete"] = True
        write_json(args.out / "summary.json", metadata)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, regression.RunFailure, subprocess.SubprocessError) as error:
        sys.exit(f"benchmark: {error}")
