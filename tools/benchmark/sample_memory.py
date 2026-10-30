#!/usr/bin/env python3
"""Usage: sample_memory.py PID CSV [--duration SECONDS]. MB means 1,000,000 bytes."""
import argparse
import csv
import time
from collections import defaultdict
from pathlib import Path


def read_memory(proc):
    values = {k: float(v.split()[0]) * 1024 / 1e6
              for k, v in (line.split(":", 1) for line in (proc / "status").read_text().splitlines())
              if k in ("VmRSS", "VmHWM")}
    seen = set()
    for fd in (proc / "fdinfo").glob("*"):
        try:
            fields = dict(line.split(":", 1) for line in fd.read_text().splitlines() if ":" in line)
        except (FileNotFoundError, ProcessLookupError):
            continue  # Descriptors can close while the process is alive.
        client = (fields.get("drm-pdev"), fields.get("drm-client-id", str(fd)))
        if client in seen:
            continue
        seen.add(client)  # Duplicate descriptors share the same DRM accounting.
        for key, value in fields.items():
            if key.startswith(("drm-memory-", "drm-total-", "drm-resident-")):
                number, unit = value.split()
                values[key] = values.get(key, 0) + float(number) * {
                    "B": 1, "kB": 1024, "KiB": 1024, "MiB": 1024**2, "GiB": 1024**3}[unit] / 1e6
    return values


def sample(proc, output, duration=float("inf")):
    series = defaultdict(list)
    start = deadline = time.monotonic()
    writer = csv.writer(output)
    writer.writerow(("elapsed_seconds", "metric", "MB"))
    while (elapsed := time.monotonic() - start) < duration:
        try:
            values = read_memory(proc)
        except (FileNotFoundError, ProcessLookupError):
            break
        if "VmRSS" not in values:  # Zombies no longer have resident memory.
            break
        for key, value in values.items():
            writer.writerow((f"{elapsed:.3f}", key, f"{value:.6f}"))
            series[key].append((elapsed, value))
        output.flush()
        deadline += 1
        time.sleep(max(0, min(deadline, start + duration) - time.monotonic()))
    end = max((points[-1][0] for points in series.values()), default=0)
    for key, points in sorted(series.items()):
        tail = [(t, v) for t, v in points if t >= end / 2]
        mt, mv = (sum(p[i] for p in tail) / len(tail) for i in (0, 1)) if tail else (0, 0)
        variance = sum((t - mt)**2 for t, v in tail)
        slope = f"{60 * sum((t - mt) * (v - mv) for t, v in tail) / variance:.6f}" if variance else "n/a"
        print(f"{key}: peak={max(v for t, v in points):.6f} MB slope={slope} MB/min (final half)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--duration", type=float, default=float("inf"))
    args = parser.parse_args()
    with args.csv.open("w", newline="") as output:
        sample(Path("/proc") / str(args.pid), output, args.duration)
