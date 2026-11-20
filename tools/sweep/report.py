#!/usr/bin/env python3
"""Compare boot-sweep result directories: verdict CSV, disagreements and contact sheets.

Each result directory (normally one per core) contributes one column per
distinct (core sha256, config hash) found in it, so a rebuilt core or a changed
profile never silently mixes with older runs. Within a column the latest run
of each ROM counts. `--shed` then deletes the bundles (captures, logs, states,
savefiles) of every ROM whose verdict is the same in all columns, keeping
record.json and contact.jpg.
"""

from __future__ import annotations

import argparse
import collections
import csv
import json
import re
import shutil
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

# Run directories are <rom16>-<core12>-<config12>; moved-aside runs carry a suffix.
RUN_DIR_RE = re.compile(r"^[0-9a-f]{16}-[0-9a-f]{12}-[0-9a-f]{12}$")
NOT_RUN = "not-run"


@dataclass
class Column:
    label: str
    core_sha256: str
    config_hash: str
    runs: dict[str, dict[str, Any]] = field(default_factory=dict)  # rom sha256 -> record


def load_records(result_dir: Path) -> list[dict[str, Any]]:
    records = []
    for path in sorted((result_dir / "runs").glob("*/record.json")):
        if RUN_DIR_RE.match(path.parent.name):
            record = json.loads(path.read_text())
            record["_dir"] = str(path.parent)
            records.append(record)
    return records


def columns_for(label: str, records: list[dict[str, Any]]) -> list[Column]:
    """One column per (core, config) in a result directory; latest run per ROM."""
    groups: dict[tuple[str, str], list[dict[str, Any]]] = collections.defaultdict(list)
    for record in records:
        groups[(record["core_sha256"], record["config_hash"])].append(record)
    columns = []
    for (core, config), group in sorted(groups.items(), key=lambda item: max(r["ended"] or "" for r in item[1])):
        name = label if len(groups) == 1 else f"{label}@{core[:8]}.{config[:8]}"
        column = Column(name, core, config)
        for record in sorted(group, key=lambda r: r["ended"] or ""):
            column.runs[record["rom_sha256"]] = record
        columns.append(column)
    return columns


def compare(columns: list[Column]) -> list[dict[str, Any]]:
    """One row per ROM in any column, with each column's verdict and the agreement status.

    status: `agree` (every column ran it, same verdict), `disagree` (two
    verdicts differ among the columns that ran it), `incomplete` (some column
    did not run it and the rest agree) or `single` (only one column given).
    """
    roms: dict[str, str] = {}
    for column in columns:
        for sha, record in column.runs.items():
            roms.setdefault(sha, record["rom"]["name"])
    rows = []
    for sha, name in sorted(roms.items(), key=lambda item: (item[1].lower(), item[0])):
        verdicts = {c.label: c.runs[sha]["verdict"] if sha in c.runs else NOT_RUN for c in columns}
        present = {v for v in verdicts.values() if v != NOT_RUN}
        if len(columns) < 2:
            status = "single"
        elif len(present) > 1:
            status = "disagree"
        elif NOT_RUN in verdicts.values():
            status = "incomplete"
        else:
            status = "agree"
        rows.append({"rom_sha256": sha, "rom_name": name, "status": status, "verdicts": verdicts})
    return rows


def csv_rows(rows: list[dict[str, Any]], columns: list[Column], sheet_for: dict[str, str]) -> list[dict[str, Any]]:
    out = []
    for row in rows:
        line: dict[str, Any] = {"rom_sha256": row["rom_sha256"], "rom_name": row["rom_name"], "status": row["status"]}
        for column in columns:
            record = column.runs.get(row["rom_sha256"])
            line[column.label] = row["verdicts"][column.label]
            line[f"{column.label}_sub"] = (record or {}).get("sub_class") or ""
            line[f"{column.label}_screen"] = (record or {}).get("screen") or ""
            line[f"{column.label}_frames"] = (record or {}).get("frames_reached", "")
            static = (record or {}).get("static_from_frame")
            line[f"{column.label}_static_from"] = "" if static is None else static
            line[f"{column.label}_sheet"] = str(Path(record["_dir"]) / "contact.jpg") if record else ""
        line["comparison_sheet"] = sheet_for.get(row["rom_sha256"], "")
        out.append(line)
    return out


def write_csv(path: Path, lines: list[dict[str, Any]], columns: list[Column]) -> None:
    keys = ["rom_sha256", "rom_name", "status"]
    for column in columns:
        keys += [column.label] + [f"{column.label}_{s}" for s in ("sub", "screen", "frames", "static_from", "sheet")]
    keys.append("comparison_sheet")
    with path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, keys)
        writer.writeheader()
        writer.writerows(lines)


def run_image(record: dict[str, Any] | None, label: str):
    import sheets

    if record is None:
        return sheets.placeholder(label, NOT_RUN)
    contact = Path(record["_dir"]) / "contact.jpg"
    if contact.is_file():
        from PIL import Image
        with Image.open(contact) as image:
            return image.convert("RGB")
    return sheets.placeholder(f"{label} | {record['rom']['name']} | {record['verdict']}", "contact sheet missing")


def write_sheets(out: Path, rows: list[dict[str, Any]], columns: list[Column], rows_per_page: int) -> dict[str, str]:
    """Overview pages per column and one side-by-side sheet per disagreement; returns ROM -> comparison sheet."""
    import sheets

    for column in columns:
        ran = [row for row in rows if row["rom_sha256"] in column.runs]
        for start in range(0, len(ran), rows_per_page):
            images = [run_image(column.runs[r["rom_sha256"]], column.label) for r in ran[start:start + rows_per_page]]
            sheets.save(sheets.stack(images), out / "sheets" / column.label.replace("/", "_") / f"page-{start // rows_per_page + 1:03d}.jpg")
    comparison = {}
    for row in rows:
        if row["status"] != "disagree":
            continue
        images = [sheets.labelled(f"{c.label}: {row['verdicts'][c.label]}", run_image(c.runs.get(row["rom_sha256"]), c.label))
                  for c in columns]
        path = out / "sheets" / "disagreements" / f"{row['rom_sha256'][:16]}.jpg"
        sheets.save(sheets.stack(images, gap=8), path)
        comparison[row["rom_sha256"]] = str(path)
    return comparison


def summary(rows: list[dict[str, Any]], columns: list[Column], comparison: dict[str, str]) -> str:
    lines = ["# Boot sweep comparison", "",
             "Verdicts are mechanical (see tools/sweep/README.md). `pass` does not assert a title, menu or gameplay.", "",
             "| Column | Profile | Library | Core sha256 | Config | Runs |", "|---|---|---|---|---|---:|"]
    for column in columns:
        any_record = next(iter(column.runs.values()))
        core = any_record["core"]
        library = " ".join(filter(None, (core.get("library_name"), core.get("library_version")))) or "unknown"
        lines.append(f"| {column.label} | {core['profile']} | {library} | {column.core_sha256[:12]} | "
                     f"{column.config_hash[:12]} | {len(column.runs)} |")
    found = sorted({v for row in rows for v in row["verdicts"].values()})
    lines += ["", "| Column | " + " | ".join(found) + " |", "|---|" + "---:|" * len(found)]
    for column in columns:
        counts = collections.Counter(row["verdicts"][column.label] for row in rows)
        lines.append(f"| {column.label} | " + " | ".join(str(counts.get(v, 0)) for v in found) + " |")
    statuses = collections.Counter(row["status"] for row in rows)
    lines += ["", "ROMs by status: " + ", ".join(f"{k} {v}" for k, v in sorted(statuses.items())) + ".", ""]
    disagreements = [row for row in rows if row["status"] == "disagree"]
    if disagreements:
        lines += ["## Disagreements", "", "| ROM | " + " | ".join(c.label for c in columns) + " | Sheet |",
                  "|---|" + "---|" * (len(columns) + 1)]
        for row in disagreements:
            lines.append(f"| {row['rom_name']} | " + " | ".join(row["verdicts"][c.label] for c in columns)
                         + f" | {comparison.get(row['rom_sha256'], '')} |")
    return "\n".join(lines) + "\n"


def shed(rows: list[dict[str, Any]], columns: list[Column]) -> tuple[int, int]:
    """Delete bundles of runs whose ROM agrees in every column; returns (runs shed, bytes freed)."""
    runs = freed = 0
    for row in rows:
        if row["status"] != "agree":
            continue
        for column in columns:
            bundles = Path(column.runs[row["rom_sha256"]]["_dir"]) / "bundles"
            if bundles.is_dir():
                freed += sum(p.stat().st_size for p in bundles.rglob("*") if p.is_file())
                shutil.rmtree(bundles)
                runs += 1
    return runs, freed


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("results", nargs="+", help="result directory, or LABEL=DIR (default label: directory name)")
    parser.add_argument("--out", type=Path, required=True, help="report directory")
    parser.add_argument("--shed", action="store_true",
                        help="delete captures and logs of runs whose verdict agrees across all columns")
    parser.add_argument("--rows-per-page", type=int, default=10, help="runs per overview sheet (default 10)")
    args = parser.parse_args(argv)
    args.results = [(Path(r.partition("=")[2]), r.partition("=")[0]) if "=" in r else (Path(r), Path(r).resolve().name)
                    for r in args.results]
    if args.rows_per_page < 1:
        parser.error("--rows-per-page must be positive")
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    columns: list[Column] = []
    for directory, label in args.results:
        records = load_records(directory)
        if not records:
            print(f"[report] no completed runs in {directory}", file=sys.stderr)
            return 2
        columns += columns_for(label, records)
    labels = [c.label for c in columns]
    if len(set(labels)) != len(labels):
        print(f"[report] duplicate column labels {labels}; name them with LABEL=DIR", file=sys.stderr)
        return 2
    if args.shed and len(columns) < 2:
        print("[report] --shed needs at least two columns to agree", file=sys.stderr)
        return 2
    rows = compare(columns)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    comparison = write_sheets(out, rows, columns, args.rows_per_page)
    lines = csv_rows(rows, columns, comparison)
    write_csv(out / "verdicts.csv", lines, columns)
    write_csv(out / "disagreements.csv", [l for l in lines if l["status"] == "disagree"], columns)
    (out / "summary.md").write_text(summary(rows, columns, comparison))
    print(f"[report] {len(rows)} ROMs, {len(columns)} columns, "
          f"{sum(r['status'] == 'disagree' for r in rows)} disagreements: {out / 'summary.md'}")
    if args.shed:
        runs, freed = shed(rows, columns)
        print(f"[report] shed {runs} agreeing runs, {freed / 1e6:.1f} MB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
