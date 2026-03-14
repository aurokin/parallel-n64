#!/usr/bin/env python3
import argparse
import collections
import pathlib
import re
import sys


DRAW_STATE_RE = re.compile(
    r"flags=0x(?P<flags>[0-9a-fA-F]+)"
    r".*?copy=(?P<copy>\d)"
    r".*?tex0=(?P<tex0>\d)"
    r".*?tex1=(?P<tex1>\d)"
    r".*?pipe1=(?P<pipe1>\d)"
    r".*?screen=\{valid=(?P<screen_valid>\d) x=(?P<screen_x>[^ ]+) y=(?P<screen_y>[^ ]+)\}"
    r".*?repl0_desc=(?P<desc>\d+)"
    r"(?: repl0_key=0x(?P<repl_key>[0-9a-fA-F]+))?"
    r" repl0_source=(?P<repl_source>\w+)"
    r" repl0_origin=(?P<repl_origin>\w+)"
    r" repl0_birth=\{load_tile=(?P<load_tile>\d+)"
    r" load_fs=0x(?P<load_fs>[0-9a-fA-F]+)"
    r" lookup_tile=(?P<lookup_tile>\d+)"
    r" lookup_fs=0x(?P<lookup_fs>[0-9a-fA-F]+)"
    r" key=(?P<key_w>\d+)x(?P<key_h>\d+)\}"
    r" repl0_orig=(?P<orig_w>\d+)x(?P<orig_h>\d+)"
    r" repl0=(?P<repl_w>\d+)x(?P<repl_h>\d+)"
    r" repl1_desc=(?P<repl1_desc>\d+)"
    r"(?: repl1_key=0x(?P<repl1_key>[0-9a-fA-F]+))?"
    r" repl1_source=(?P<repl1_source>\w+)"
    r" repl1_origin=(?P<repl1_origin>\w+)"
    r" repl1_birth=\{load_tile=(?P<repl1_load_tile>\d+)"
    r" load_fs=0x(?P<repl1_load_fs>[0-9a-fA-F]+)"
    r" lookup_tile=(?P<repl1_lookup_tile>\d+)"
    r" lookup_fs=0x(?P<repl1_lookup_fs>[0-9a-fA-F]+)"
    r" key=(?P<repl1_key_w>\d+)x(?P<repl1_key_h>\d+)\}"
    r" repl1_orig=(?P<repl1_orig_w>\d+)x(?P<repl1_orig_h>\d+)"
    r" repl1=(?P<repl1_w>\d+)x(?P<repl1_h>\d+)"
)


def parse_optional_u32(value: str | None) -> int | None:
    if value is None:
        return None
    return int(value, 0)


def normalize_record(groups: dict[str, str]) -> dict[str, object]:
    record: dict[str, object] = dict(groups)
    for key in (
        "copy",
        "tex0",
        "tex1",
        "pipe1",
        "screen_valid",
        "desc",
        "load_tile",
        "lookup_tile",
        "key_w",
        "key_h",
        "orig_w",
        "orig_h",
        "repl_w",
        "repl_h",
        "repl1_desc",
        "repl1_load_tile",
        "repl1_lookup_tile",
        "repl1_key_w",
        "repl1_key_h",
        "repl1_orig_w",
        "repl1_orig_h",
        "repl1_w",
        "repl1_h",
    ):
        record[key] = int(record[key])
    for key in ("flags", "load_fs", "lookup_fs", "repl1_load_fs", "repl1_lookup_fs"):
        record[key] = f"0x{int(str(record[key]), 16):x}"
    record["key_wh"] = f"{record['key_w']}x{record['key_h']}"
    record["orig_wh"] = f"{record['orig_w']}x{record['orig_h']}"
    record["repl_wh"] = f"{record['repl_w']}x{record['repl_h']}"
    record["repl1_key_wh"] = f"{record['repl1_key_w']}x{record['repl1_key_h']}"
    record["repl1_orig_wh"] = f"{record['repl1_orig_w']}x{record['repl1_orig_h']}"
    record["repl1_wh"] = f"{record['repl1_w']}x{record['repl1_h']}"
    if record["repl_key"] is None:
        record["repl_key"] = "0x0000000000000000"
    else:
        record["repl_key"] = f"0x{int(str(record['repl_key']), 16):016x}"
    if record["repl1_key"] is None:
        record["repl1_key"] = "0x0000000000000000"
    else:
        record["repl1_key"] = f"0x{int(str(record['repl1_key']), 16):016x}"
    return record


def matches(record: dict[str, object], args: argparse.Namespace) -> bool:
    if args.load_fs is not None and int(str(record["load_fs"]), 0) != args.load_fs:
        return False
    if args.lookup_fs is not None and int(str(record["lookup_fs"]), 0) != args.lookup_fs:
        return False
    if args.lookup_tile is not None and record["lookup_tile"] != args.lookup_tile:
        return False
    if args.repl_source and record["repl_source"] != args.repl_source:
        return False
    if args.repl_origin and record["repl_origin"] != args.repl_origin:
        return False
    if args.repl1_source and record["repl1_source"] != args.repl1_source:
        return False
    if args.repl1_origin and record["repl1_origin"] != args.repl1_origin:
        return False
    if args.desc is not None and record["desc"] != args.desc:
        return False
    if args.flags and record["flags"] != f"0x{int(args.flags, 0):x}":
        return False
    if args.key_wh and record["key_wh"] != args.key_wh:
        return False
    if args.repl_wh and record["repl_wh"] != args.repl_wh:
        return False
    return True


def summarize_log(path: pathlib.Path, args: argparse.Namespace) -> tuple[int, collections.Counter]:
    counter: collections.Counter = collections.Counter()
    matched = 0
    with path.open("r", errors="ignore") as handle:
        for line in handle:
            match = DRAW_STATE_RE.search(line)
            if not match:
                continue
            record = normalize_record(match.groupdict())
            if not matches(record, args):
                continue
            matched += 1
            group_key = tuple(record[field] for field in args.group_by)
            counter[group_key] += 1
    return matched, counter


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Summarize HIRES draw-state provenance clusters from run.log files."
    )
    parser.add_argument("--log", action="append", required=True, help="Path to a run.log file. Repeat for multiple logs.")
    parser.add_argument("--group-by", default="desc,repl_key,flags,repl_source,repl_origin,key_wh,repl_wh",
                        help="Comma-separated grouping fields.")
    parser.add_argument("--limit", type=int, default=25, help="Rows per log.")
    parser.add_argument("--load-fs", type=parse_optional_u32, help="Filter by replacement birth load formatsize.")
    parser.add_argument("--lookup-fs", type=parse_optional_u32, help="Filter by replacement birth lookup formatsize.")
    parser.add_argument("--lookup-tile", type=int, help="Filter by replacement birth lookup tile.")
    parser.add_argument("--repl-source", help="Filter by repl0_source.")
    parser.add_argument("--repl-origin", help="Filter by repl0_origin.")
    parser.add_argument("--repl1-source", help="Filter by repl1_source.")
    parser.add_argument("--repl1-origin", help="Filter by repl1_origin.")
    parser.add_argument("--desc", type=int, help="Filter by repl0_desc.")
    parser.add_argument("--flags", help="Filter by draw raster flags, e.g. 0x21844118.")
    parser.add_argument("--key-wh", help="Filter by replacement birth key width/height, e.g. 32x16.")
    parser.add_argument("--repl-wh", help="Filter by replacement dimensions, e.g. 512x256.")
    args = parser.parse_args()

    args.group_by = [field.strip() for field in args.group_by.split(",") if field.strip()]
    if not args.group_by:
        parser.error("--group-by must specify at least one field")

    for path_str in args.log:
        path = pathlib.Path(path_str)
        if not path.is_file():
            print(f"Missing log: {path}", file=sys.stderr)
            return 1

        matched, counter = summarize_log(path, args)
        print(f"\n## {path}")
        print(f"matched_draws={matched}")
        if not counter:
            continue

        for key, count in counter.most_common(args.limit):
            parts = [f"{field}={value}" for field, value in zip(args.group_by, key)]
            print(f"{count:6d}  " + " ".join(parts))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
