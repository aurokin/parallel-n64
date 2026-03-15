#!/usr/bin/env python3
import argparse
import collections
import pathlib
import sys

from hires_draw_debug_report import collect_records


def build_filter_set(values: list[str]) -> set[str]:
    return set(values)


def load_records(path: pathlib.Path,
                 repl_archetype_filter: set[str],
                 key_wh_filter: set[str],
                 repl_wh_filter: set[str],
                 flags_filter: set[str]) -> list[dict[str, str]]:
    return collect_records(
        path=path,
        desc_filter=set(),
        repl_key_filter=set(),
        repl_source_filter=set(),
        repl_origin_filter=set(),
        draw_owner_filter=set(),
        repl_owner_filter=set(),
        repl_archetype_filter=repl_archetype_filter,
        key_wh_filter=key_wh_filter,
        repl_wh_filter=repl_wh_filter,
        flags_filter=flags_filter,
        call_min=None,
        call_max=None,
    )


def signature(record: dict[str, str], fields: list[str]) -> tuple[str, ...]:
    return tuple(record.get(field, "") for field in fields)


def summarize(counter_a: collections.Counter,
              counter_b: collections.Counter,
              fields: list[str],
              limit: int,
              mode: str) -> None:
    if mode == "shared":
        items = []
        for key in (counter_a.keys() & counter_b.keys()):
            items.append((min(counter_a[key], counter_b[key]), counter_a[key], counter_b[key], key))
        items.sort(key=lambda item: (-item[0], -item[1], item[3]))
        header = "shared_signatures"
    elif mode == "left-only":
        items = [(counter_a[key], counter_a[key], 0, key) for key in (counter_a.keys() - counter_b.keys())]
        items.sort(key=lambda item: (-item[0], item[3]))
        header = "left_only_signatures"
    else:
        items = [(counter_b[key], 0, counter_b[key], key) for key in (counter_b.keys() - counter_a.keys())]
        items.sort(key=lambda item: (-item[0], item[3]))
        header = "right_only_signatures"

    print(f"\n## {header}")
    print(f"count={len(items)}")
    for score, count_a, count_b, key in items[:limit]:
        parts = [f"{field}={value}" for field, value in zip(fields, key)]
        print(f"{score:6d}  left={count_a:6d} right={count_b:6d} " + " ".join(parts))


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare draw-shape signatures across two HIRES debug logs.")
    parser.add_argument("--left-log", required=True, help="Path to the left run.log.")
    parser.add_argument("--right-log", required=True, help="Path to the right run.log.")
    parser.add_argument(
        "--fields",
        default="repl0_archetype,repl_key,screen_x,screen_y,flags",
        help="Comma-separated fields that define a shape signature.",
    )
    parser.add_argument("--repl-archetype", action="append", default=[], help="Filter by repl0_archetype. Repeatable.")
    parser.add_argument("--key-wh", action="append", default=[], help="Filter by birth key width/height, e.g. 32x32.")
    parser.add_argument("--repl-wh", action="append", default=[], help="Filter by replacement dimensions, e.g. 320x320.")
    parser.add_argument("--flags", action="append", default=[], help="Filter by raster flags, e.g. 21844108.")
    parser.add_argument("--limit", type=int, default=25, help="Rows per section.")
    args = parser.parse_args()

    left_path = pathlib.Path(args.left_log)
    right_path = pathlib.Path(args.right_log)
    if not left_path.is_file():
        print(f"missing log: {left_path}", file=sys.stderr)
        return 1
    if not right_path.is_file():
        print(f"missing log: {right_path}", file=sys.stderr)
        return 1

    fields = [field.strip() for field in args.fields.split(",") if field.strip()]
    repl_archetype_filter = build_filter_set(args.repl_archetype)
    key_wh_filter = build_filter_set(args.key_wh)
    repl_wh_filter = build_filter_set(args.repl_wh)
    flags_filter = {value.removeprefix("0x").lower() for value in args.flags}

    left_records = load_records(left_path, repl_archetype_filter, key_wh_filter, repl_wh_filter, flags_filter)
    right_records = load_records(right_path, repl_archetype_filter, key_wh_filter, repl_wh_filter, flags_filter)

    left_counter = collections.Counter(signature(record, fields) for record in left_records)
    right_counter = collections.Counter(signature(record, fields) for record in right_records)

    print(f"left_records={len(left_records)}")
    print(f"right_records={len(right_records)}")
    summarize(left_counter, right_counter, fields, args.limit, "shared")
    summarize(left_counter, right_counter, fields, args.limit, "left-only")
    summarize(left_counter, right_counter, fields, args.limit, "right-only")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
