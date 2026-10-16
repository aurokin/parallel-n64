#!/usr/bin/env python3
"""Check that hi-res replacement is presentation-only across a regression run pair.

Given an off-mode and an on-mode output directory from `run_scenarios.py` over the
same scenarios, compare each scenario's end savestate and checkpoint RAM cues:

* every byte outside RDRAM (CPU, coprocessor, RSP/RDP and device registers, TLB,
  event queue) must be identical — a difference fails;
* checkpoint RAM cues must be identical — a difference fails;
* a scenario present in only one run fails, so an interrupted run cannot pass;
* RDRAM differences are reported as address ranges. Replacement changes the
  native framebuffer and depth-buffer contents the renderer writes back, so
  those ranges are expected; anything else is for review.

Exit status is nonzero when any scenario fails.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import savestate_bridge as bridge  # noqa: E402

PAGE = 0x1000


def results(run: Path) -> dict[str, dict]:
    records = {}
    for line in (run / "results.jsonl").read_text().splitlines():
        if line.strip():
            record = json.loads(line)
            records[record["scenario"]] = record
    return records


def end_state(run: Path, scenario: str, mode: str) -> bytes | None:
    matches = sorted((run / "bundles" / f"{scenario}__{mode}" / "states").glob("*/*.state2"))
    return bridge.unwrap(matches[-1].read_bytes()) if matches else None


def rdram_ranges(off: bytes, on: bytes) -> list[tuple[int, int, int]]:
    """Differing RDRAM pages merged into (start, end, differing words) ranges."""
    ranges: list[list[int]] = []
    for page in range(bridge.RDRAM_SIZE // PAGE):
        start = bridge.RDRAM + page * PAGE
        a, b = off[start:start + PAGE], on[start:start + PAGE]
        if a == b:
            continue
        words = sum(a[i:i + 4] != b[i:i + 4] for i in range(0, PAGE, 4))
        if ranges and ranges[-1][1] == page * PAGE:
            ranges[-1][1] += PAGE
            ranges[-1][2] += words
        else:
            ranges.append([page * PAGE, (page + 1) * PAGE, words])
    return [tuple(r) for r in ranges]


def compare(off_run: Path, on_run: Path) -> list[dict]:
    off_results, on_results = results(off_run), results(on_run)
    report = []
    for scenario in sorted(set(off_results) | set(on_results)):
        entry: dict = {"scenario": scenario, "status": "pass", "detail": []}
        if scenario not in off_results or scenario not in on_results:
            entry["status"] = "fail"
            entry["detail"].append(f"no {'off' if scenario not in off_results else 'on'} result")
            report.append(entry)
            continue
        off_cues = [c.get("ram", {}) for c in off_results[scenario].get("checkpoints", [])]
        on_cues = [c.get("ram", {}) for c in on_results[scenario].get("checkpoints", [])]
        if off_cues != on_cues:
            entry["status"] = "fail"
            entry["detail"].append("checkpoint RAM cues differ")
        off_state, on_state = end_state(off_run, scenario, "off"), end_state(on_run, scenario, "on")
        if off_state is None or on_state is None:
            entry["status"] = "fail"
            entry["detail"].append("end state missing")
        elif len(off_state) != len(on_state):
            entry["status"] = "fail"
            entry["detail"].append("end state sizes differ")
        else:
            outside = (off_state[:bridge.RDRAM] != on_state[:bridge.RDRAM] or
                       off_state[bridge.RDRAM + bridge.RDRAM_SIZE:] != on_state[bridge.RDRAM + bridge.RDRAM_SIZE:])
            if outside:
                entry["status"] = "fail"
                entry["detail"].append("state outside RDRAM differs")
            entry["rdram_ranges"] = [{"start": f"{s:#08x}", "end": f"{e:#08x}", "words": w}
                                     for s, e, w in rdram_ranges(off_state, on_state)]
        report.append(entry)
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("off_run", type=Path)
    parser.add_argument("on_run", type=Path)
    parser.add_argument("--json", type=Path, help="write the full report here")
    args = parser.parse_args(argv)
    report = compare(args.off_run, args.on_run)
    for entry in report:
        ranges = ", ".join(f"{r['start']}-{r['end']}({r['words']})" for r in entry.get("rdram_ranges", []))
        print(f"{entry['status']:4} {entry['scenario']}: {'; '.join(entry['detail']) or 'non-RDRAM state identical'}"
              f"{'; RDRAM ' + ranges if ranges else ''}")
    if args.json:
        args.json.write_text(json.dumps(report, indent=2) + "\n")
    return 1 if any(entry["status"] != "pass" for entry in report) or not report else 0


if __name__ == "__main__":
    sys.exit(main())
