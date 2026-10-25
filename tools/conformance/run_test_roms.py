#!/usr/bin/env python3
"""Run public N64 conformance test ROMs and gate on the verdicts they leave in machine state.

Each selected ROM runs in a fresh paused frontend session through the regression
runner's adapter path: power on, step a fixed frame budget, capture, save the end
state and stop. The suite's decoder (tools/conformance/decoders.py) reads the
verdict from the saved state, so no screenshot comparison enters a gate.

Outcomes per test and CPU engine:

  pass        the decoder found the suite's pass evidence
  fail        failing evidence, an unfinished test, a session error or a wrong engine
  known-fail  a failure, crash or hang the manifest lists for this engine (reported,
              not gating); a session on the wrong engine still fails
  fixed       a listed known failure that now passes (reported; update the manifest)
  missing     the ROM is not staged (skip, never a pass)

Exit status: 0 when nothing gating failed, 1 when a test failed or a suite is
only partially staged, 2 for invalid arguments or a busy host, 77 when no
selected ROM is staged.
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import os
import shutil
import sys
from pathlib import Path
from typing import Any

TOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIR))
sys.path.insert(0, str(TOOLS_DIR / "regression"))
import run_scenarios as rs  # noqa: E402
from conformance.decoders import DecodeError, decode  # noqa: E402
from savestate_bridge import unwrap  # noqa: E402

MANIFEST_SCHEMA = "n64-conformance-roms/1"
DEFAULT_MANIFEST = Path(__file__).resolve().with_name("test_roms.json")
ENGINES = {
    "dynamic_recompiler": "Dynamic Recompiler",
    "cached_interpreter": "Cached Interpreter",
    "pure_interpreter": "Pure Interpreter",
}
GATING_FAILURES = {"fail"}


def load_manifest(path: Path) -> dict[str, Any]:
    manifest = json.loads(path.read_text())
    if manifest.get("schema") != MANIFEST_SCHEMA:
        raise ValueError(f"{path}: schema must be {MANIFEST_SCHEMA!r}")
    return manifest


def known_failure(manifest: dict[str, Any], test_id: str, cpucore: str) -> dict[str, Any] | None:
    for entry in manifest.get("known_failures", []):
        if entry["test"] == test_id and ("*" in entry["cpucores"] or cpucore in entry["cpucores"]):
            return entry
    return None


def selected(test: dict[str, Any], selectors: list[str]) -> bool:
    """Every selector must match: `suite=a,b` or an id glob."""
    for selector in selectors:
        if selector.startswith("suite="):
            if test["suite"] not in selector.partition("=")[2].split(","):
                return False
        elif not fnmatch.fnmatchcase(test["id"], selector):
            return False
    return True


def classify(verdict_status: str, known: dict[str, Any] | None, observed: dict[str, Any]) -> str:
    """Apply the manifest's known failures to a decoded verdict."""
    if known is None:
        return "pass" if verdict_status == "pass" else "fail"
    if verdict_status == "pass":
        return "fixed"
    # A suite that reports named failures stays gating for names the manifest
    # does not list, so a known failure cannot hide a new one.
    listed = known.get("failing_tests")
    if listed is not None and observed.get("failed_tests") is not None:
        return "known-fail" if set(observed["failed_tests"]) <= set(listed) else "fail"
    return "known-fail"


def engine_started(log_text: str) -> str | None:
    marker = "Starting R4300 emulator: "
    for line in log_text.splitlines():
        if marker in line:
            return line.split(marker, 1)[1].strip()
    return None


def run_one(test: dict[str, Any], suite: dict[str, Any], rom: Path, ctx: rs.RunContext,
            manifest: dict[str, Any], args: argparse.Namespace) -> dict[str, Any]:
    frames = int(test.get("frames", suite["frames"]))
    capture = (rs.Checkpoint(frames, True, ()),) if not args.no_capture else ()
    scenario = rs.Scenario(
        path=args.manifest, data={}, id=f"{test['id']}.{args.cpucore}", rom_sha256=test["sha256"],
        start={"kind": "power-on"}, program=(f"STEP_FRAME {frames}",), checkpoints=capture,
        save_end=True, tags={"suite": test["suite"]}, modes=("off",))
    # Frames are VI-paced: a suite that keeps the CPU busy between VI interrupts
    # steps slowly, so its manifest entry can shorten steps and extend the TTL.
    ctx.args.rom = rom
    ctx.args.step_chunk = int(suite.get("step_chunk", 600))
    ctx.args.ttl = suite.get("ttl_seconds")
    record = rs.run_scenario(scenario, ctx)
    bundle = ctx.out / record["bundle"]
    log_path = bundle / "logs" / "retroarch.log"
    engine = engine_started(log_path.read_text(errors="replace")) if log_path.is_file() else None
    result: dict[str, Any] = {
        "test": test["id"], "suite": test["suite"], "cpucore": args.cpucore, "engine_logged": engine,
        "rom_sha256": test["sha256"], "frames": frames, "core_sha256": record["core_sha256"],
        "frontend_sha256": record["frontend_sha256"], "runtime_class": args.runtime_class,
        "exit_class": record["exit_class"], "wall_seconds": record["wall_seconds"],
        "bundle": record["bundle"], "log_flags": record["log_flags"], "verdict": None, "detail": "",
        "observed": {},
    }
    known = known_failure(manifest, test["id"], args.cpucore)
    # A logged wrong engine gates even when the session then crashed or hung,
    # so a known failure cannot hide a run on the wrong engine.
    if engine is not None and engine != ENGINES[args.cpucore]:
        result.update(verdict="wrong-engine", detail=f"engine {engine!r} started, expected {ENGINES[args.cpucore]!r}")
    elif record["exit_class"] != "clean":
        result.update(verdict="error", detail=f"{record['exit_class']}: {record['error'] or ''}".strip())
    elif engine is None:
        result.update(verdict="wrong-engine", detail=f"no engine logged, expected {ENGINES[args.cpucore]!r}")
    else:
        saved = sorted((bundle / "states").rglob(f"*.state{rs.END_SLOT}"), key=lambda p: p.stat().st_mtime_ns)
        try:
            verdict = decode(suite["decoder"], unwrap(saved[-1].read_bytes()), test)
            result.update(verdict=verdict.status, detail=verdict.detail, observed=verdict.observed)
        except (IndexError, DecodeError, ValueError) as error:
            result.update(verdict="error", detail=f"end state unreadable: {error}")
    # A known failure may be a crash or hang (a session error); a session on the
    # wrong engine says nothing about the test and always fails.
    result["outcome"] = ("fail" if result["verdict"] == "wrong-engine"
                         else classify(result["verdict"], known, result["observed"]))
    if known:
        result["known_failure"] = known["reason"]
    if result["outcome"] in ("pass", "known-fail", "fixed") and not args.keep_bundles:
        shutil.rmtree(bundle, ignore_errors=True)
        result["bundle"] = None
    return result


def write_report(out: Path, results: list[dict[str, Any]], missing: list[dict[str, Any]],
                 manifest: dict[str, Any]) -> None:
    counts: dict[str, int] = {}
    for r in results:
        counts[r["outcome"]] = counts.get(r["outcome"], 0) + 1
    lines = ["# Conformance test ROM report", ""]
    if results:
        first = results[0]
        lines += [f"Core `{first['core_sha256']}`, frontend `{first['frontend_sha256']}`, "
                  f"engine `{first['cpucore']}`, runtime class `{first['runtime_class']}`.", ""]
    lines += ["Outcomes: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items()))
              + (f", missing {len(missing)}" if missing else "") + ".", ""]
    for suite_id, suite in manifest["suites"].items():
        rows = [r for r in results if r["suite"] == suite_id]
        absent = [t for t in missing if t["suite"] == suite_id]
        if not rows and not absent:
            continue
        lines += [f"## {suite['title']}", ""]
        if absent:
            lines += [f"{len(absent)} ROM(s) not staged (skipped): "
                      + ", ".join(t["id"] for t in absent[:12]) + (" ..." if len(absent) > 12 else ""), ""]
        if rows:
            lines += ["| Test | Outcome | Verdict | Detail |", "|---|---|---|---|"]
            for r in rows:
                detail = r["detail"].replace("|", "\\|")
                if r.get("known_failure"):
                    detail += f" (known: {r['known_failure']})"
                if r["bundle"]:
                    detail += f" [bundle `{r['bundle']}`]"
                lines.append(f"| {r['test']} | {r['outcome']} | {r['verdict']} | {detail} |")
            lines.append("")
    (out / "report.md").write_text("\n".join(lines) + "\n")


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--roms", type=Path, required=True,
                        help="staging directory written by fetch_test_roms.py (<roms>/<suite>/<path>)")
    parser.add_argument("--core", type=Path, required=True)
    parser.add_argument("--retroarch-bin", type=Path, required=True)
    parser.add_argument("--base-config", type=Path, required=True)
    parser.add_argument("--extra-append-config", type=Path)
    parser.add_argument("--runtime-class", required=True, help="caller-declared runtime class label")
    parser.add_argument("--cpucore", choices=sorted(ENGINES), default="dynamic_recompiler",
                        help="parallel-n64-cpucore for every session (default dynamic_recompiler)")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--select", action="append", default=[], help="id glob or suite=a,b; all must match")
    parser.add_argument("--keep-bundles", action="store_true",
                        help="keep bundles of passing tests too (failing bundles are always kept)")
    parser.add_argument("--no-capture", action="store_true", help="skip the final screenshot")
    parser.add_argument("--adapter", type=Path, default=rs.DEFAULT_ADAPTER, help=argparse.SUPPRESS)
    parser.add_argument("--orphan-grace", type=float, default=3.0, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    for name in ("core", "retroarch_bin", "base_config"):
        if not getattr(args, name).is_file():
            parser.error(f"--{name.replace('_', '-')} not found: {getattr(args, name)}")
    args.manifest = args.manifest.resolve()
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    manifest = load_manifest(args.manifest)
    tests = [t for t in manifest["tests"] if selected(t, args.select)]
    if not tests:
        print("[conformance] no test matches the selection", file=sys.stderr)
        return 2
    staged = [(t, args.roms / t["suite"] / t["path"]) for t in tests]
    missing = [t for t, path in staged if not path.is_file()]
    present = [(t, path) for t, path in staged if path.is_file()]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    print(f"[conformance] {len(present)} of {len(tests)} selected ROMs staged; engine {args.cpucore}")
    if missing:
        print(f"[conformance] not staged (skipped): {', '.join(t['id'] for t in missing[:12])}"
              + (" ..." if len(missing) > 12 else "")
              + f"\n[conformance] stage them with tools/conformance/fetch_test_roms.py --dest {args.roms}")
    if not present:
        write_report(out, [], missing, manifest)
        return 77

    adapter = args.adapter.resolve()
    doctor = rs.subprocess.run([str(adapter), "doctor"], capture_output=True, text=True, check=False)
    if doctor.returncode != 0:
        print(f"[conformance] host is not idle; refusing to start:\n{doctor.stdout}{doctor.stderr}", file=sys.stderr)
        return 2
    os.environ["PARALLEL_N64_CPUCORE_OVERRIDE"] = args.cpucore
    runner_args = argparse.Namespace(
        core=args.core, retroarch_bin=args.retroarch_bin, base_config=args.base_config,
        extra_append_config=args.extra_append_config, mode="off", pack=None, pack_map=None,
        rom=None, rom_map=None, blobs=None, ttl=None, step_chunk=600, orphan_grace=args.orphan_grace,
        hires_summary_interval=600, runtime_class=f"{args.runtime_class}.{args.cpucore}")
    ctx = rs.RunContext(args=runner_args, adapter=adapter, out=out, condition=f"off:{runner_args.runtime_class}",
                        identities={"core_sha256": rs.sha256_file(args.core),
                                    "frontend_sha256": rs.sha256_file(args.retroarch_bin)})
    suites = manifest["suites"]
    results = []
    aborted = False
    with (out / "results.jsonl").open("a") as sink:
        for test, rom in present:
            if rs.sha256_file(rom) != test["sha256"]:
                result = {"test": test["id"], "suite": test["suite"], "cpucore": args.cpucore, "verdict": "error",
                          "outcome": "fail", "detail": f"{rom} does not match the manifest sha256", "bundle": None,
                          "core_sha256": ctx.identities["core_sha256"],
                          "frontend_sha256": ctx.identities["frontend_sha256"], "runtime_class": args.runtime_class}
            else:
                result = run_one(test, suites[test["suite"]], rom, ctx, manifest, args)
            results.append(result)
            sink.write(json.dumps(result, sort_keys=True) + "\n")
            sink.flush()
            print(f"[conformance] {test['id']} [{args.cpucore}]: {result['outcome']} ({result['detail']})")
            if "orphan-survived" in result.get("log_flags", []):
                print("[conformance] a frontend process survived reaping; stopping the run.", file=sys.stderr)
                aborted = True
                break
    write_report(out, results, missing, manifest)
    print(f"[conformance] report: {out / 'report.md'}")
    # Staging is per suite, so a suite with only some ROMs staged is a broken
    # staging rather than an opt-out: it fails instead of passing with gaps.
    partial = sorted({t["suite"] for t in missing} & {t["suite"] for t, _ in present})
    if partial:
        print(f"[conformance] partially staged suite(s): {', '.join(partial)}", file=sys.stderr)
    # An aborted run left staged ROMs unrun, so it cannot pass.
    return 1 if aborted or partial or any(r["outcome"] in GATING_FAILURES for r in results) else 0


if __name__ == "__main__":
    sys.exit(main())
