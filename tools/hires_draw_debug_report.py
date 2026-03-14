#!/usr/bin/env python3
import argparse
import collections
import pathlib
import re
import sys


PROGRAM_RE = re.compile(
    r"descs=\[(?P<desc>\d+),"
    r".*?raster=0x(?P<raster>[0-9a-fA-F]+)"
    r" norm=0x(?P<norm>[0-9a-fA-F]+)"
    r" depth=0x(?P<depth>[0-9a-fA-F]+)"
    r".*?shade=\{(?P<shade>[0-9,]+)\}"
    r".*?b0=\{(?P<b0>[0-9,]+)\}"
    r" b1=\{(?P<b1>[0-9,]+)\}"
)

DERIVED_RE = re.compile(
    r"c0_mul=\{(?P<c0_mul>[0-9,]+)\}"
    r".*?c1_mul=\{(?P<c1_mul>[0-9,]+)\}"
    r".*?c1_add=\{(?P<c1_add>[0-9,]+)\}"
    r".*?prim=0x(?P<prim>[0-9a-fA-F]+)"
    r" env=0x(?P<env>[0-9a-fA-F]+)"
)

DRAW_RE = re.compile(
    r"call=(?P<call>\d+)"
    r".*?flags=0x(?P<flags>[0-9a-fA-F]+)"
    r".*?copy=(?P<copy>\d)"
    r".*?tex0=(?P<tex0>\d)"
    r".*?tex1=(?P<tex1>\d)"
    r".*?pipe1=(?P<pipe1>\d)"
    r".*?screen=\{valid=(?P<screen_valid>\d) x=(?P<screen_x>[^ ]+) y=(?P<screen_y>[^ ]+)\}"
    r".*?st=\{s=(?P<s>-?\d+) t=(?P<t>-?\d+) dsdx=(?P<dsdx>-?\d+) dtdy=(?P<dtdy>-?\d+)"
    r".*?repl0_desc=(?P<repl0_desc>\d+)"
    r"(?: repl0_key=0x(?P<repl_key>[0-9a-fA-F]+))?"
    r".*?key=(?P<key_wh>\d+x\d+)\}"
    r" repl0_orig=(?P<orig_wh>\d+x\d+)"
    r" repl0=(?P<repl_wh>\d+x\d+)"
    r" repl1_desc=(?P<repl1_desc>\d+)"
    r"(?: repl1_key=0x(?P<repl1_key>[0-9a-fA-F]+))?"
)


def norm_key(value: str | None) -> str:
    if not value:
        return "0x0000000000000000"
    return f"0x{int(value, 16):016x}"


def collect_records(path: pathlib.Path, desc_filter: set[int], repl_key_filter: set[str]):
    records = []
    counter: collections.Counter = collections.Counter()
    pending_program = None
    pending_derived = None

    with path.open("r", errors="ignore") as handle:
        for line in handle:
            if "Hi-res debug program:" in line:
                match = PROGRAM_RE.search(line)
                pending_program = match.groupdict() if match else None
                pending_derived = None
                continue

            if pending_program is not None and "Hi-res derived constants:" in line:
                match = DERIVED_RE.search(line)
                pending_derived = match.groupdict() if match else None
                continue

            if pending_program is None or pending_derived is None or "Hi-res draw state:" not in line:
                continue

            match = DRAW_RE.search(line)
            if not match:
                pending_program = None
                pending_derived = None
                continue

            draw = match.groupdict()
            record = {}
            record.update(pending_program)
            record.update(pending_derived)
            record.update(draw)
            record["repl_key"] = norm_key(record.get("repl_key"))
            record["repl1_key"] = norm_key(record.get("repl1_key"))
            if desc_filter and int(record["desc"]) not in desc_filter:
                pending_program = None
                pending_derived = None
                continue
            if repl_key_filter and record["repl_key"] not in repl_key_filter:
                pending_program = None
                pending_derived = None
                continue

            records.append(record)
            pending_program = None
            pending_derived = None

    return records


def summarize_log(path: pathlib.Path, group_by: list[str], limit: int, desc_filter: set[int], repl_key_filter: set[str]):
    counter: collections.Counter = collections.Counter()
    records = collect_records(path, desc_filter, repl_key_filter)
    for record in records:
        key = tuple(record[field] for field in group_by)
        counter[key] += 1

    print(f"\n## {path}")
    print(f"matched_draws={len(records)}")
    for key, count in counter.most_common(limit):
        parts = [f"{field}={value}" for field, value in zip(group_by, key)]
        print(f"{count:6d}  " + " ".join(parts))


def dump_records(path: pathlib.Path, fields: list[str], limit: int, desc_filter: set[int], repl_key_filter: set[str]):
    records = collect_records(path, desc_filter, repl_key_filter)
    print(f"\n## {path}")
    print(f"matched_draws={len(records)}")
    for record in records[:limit]:
        parts = [f"{field}={record[field]}" for field in fields]
        print(" ".join(parts))


def main() -> int:
    parser = argparse.ArgumentParser(description="Summarize joined HIRES debug program/derived/draw triplets.")
    parser.add_argument("--log", action="append", required=True, help="Path to run.log. Repeat for multiple logs.")
    parser.add_argument(
        "--group-by",
        default="desc,repl_key,raster,norm,depth,b0,b1,shade,prim,c1_mul,c1_add,repl1_desc,repl1_key,key_wh,repl_wh",
        help="Comma-separated grouping fields.",
    )
    parser.add_argument("--desc", action="append", type=int, default=[], help="Filter by desc. Repeatable.")
    parser.add_argument("--repl-key", action="append", default=[], help="Filter by replacement checksum64 key. Repeatable.")
    parser.add_argument("--limit", type=int, default=25, help="Rows per log.")
    parser.add_argument("--dump-records", action="store_true", help="Dump matched records in call order instead of grouped counts.")
    args = parser.parse_args()

    group_by = [field.strip() for field in args.group_by.split(",") if field.strip()]
    if not group_by:
        parser.error("--group-by must specify at least one field")

    desc_filter = set(args.desc)
    repl_key_filter = {norm_key(value.removeprefix("0x")) for value in args.repl_key}

    for path_str in args.log:
        path = pathlib.Path(path_str)
        if not path.is_file():
            print(f"Missing log: {path}", file=sys.stderr)
            return 1
        if args.dump_records:
            dump_records(path, group_by, args.limit, desc_filter, repl_key_filter)
        else:
            summarize_log(path, group_by, args.limit, desc_filter, repl_key_filter)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
