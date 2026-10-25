#!/usr/bin/env python3
"""Stage the pinned public conformance test ROMs listed in test_roms.json.

Downloads go to `<dest>/<suite>/<path>` and are kept only when their sha256
matches the manifest; a mismatch is an error, never a silent replacement.
Already staged files with the right hash are not downloaded again. Suites
whose ROMs are built from source (n64-systemtest) are staged by their build
script, named in the manifest.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

DEFAULT_MANIFEST = Path(__file__).resolve().with_name("test_roms.json")
DEFAULT_DEST = Path(__file__).resolve().parents[2] / "assets" / "conformance-roms"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def download(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "parallel-n64-conformance-fetch"})
    with urllib.request.urlopen(request, timeout=120) as response:
        return response.read()


def store(path: Path, data: bytes, expected: str) -> None:
    actual = sha256(data)
    if actual != expected:
        raise RuntimeError(f"{path.name}: sha256 {actual} does not match the pinned {expected}")
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as handle:
        handle.write(data)
    Path(handle.name).replace(path)


def staged(path: Path, expected: str) -> bool:
    return path.is_file() and sha256(path.read_bytes()) == expected


def fetch_suite(name: str, suite: dict, tests: list[dict], dest: Path) -> int:
    fetch = suite["fetch"]
    pending = [t for t in tests if not staged(dest / name / t["path"], t["sha256"])]
    print(f"[fetch] {name}: {len(tests) - len(pending)} of {len(tests)} already staged")
    if not pending:
        return 0
    if fetch["kind"] == "github-raw":
        base = f"https://raw.githubusercontent.com/{fetch['repo']}/{fetch['commit']}/"
        for test in pending:
            url = base + urllib.request.quote(test["path"])
            store(dest / name / test["path"], download(url), test["sha256"])
            print(f"[fetch]   {test['path']}")
    elif fetch["kind"] == "zip":
        archive = download(fetch["url"])
        if sha256(archive) != fetch["sha256"]:
            raise RuntimeError(f"{fetch['url']}: archive sha256 {sha256(archive)} does not match "
                               f"the pinned {fetch['sha256']}")
        with zipfile.ZipFile(io.BytesIO(archive)) as bundle:
            for test in pending:
                store(dest / name / test["path"], bundle.read(test["path"]), test["sha256"])
                print(f"[fetch]   {test['path']}")
    elif fetch["kind"] == "build":
        print(f"[fetch] {name}: built from source; run {fetch['script']} --dest {dest}")
        return 1
    else:
        raise RuntimeError(f"{name}: unknown fetch kind {fetch['kind']!r}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--dest", type=Path, default=DEFAULT_DEST,
                        help="staging directory (default: assets/conformance-roms, ignored by git)")
    parser.add_argument("--suite", action="append", help="suite to stage (default: every downloadable suite)")
    args = parser.parse_args(argv)
    manifest = json.loads(args.manifest.read_text())
    suites = manifest["suites"]
    names = args.suite or [n for n, s in suites.items() if s["fetch"]["kind"] != "build"]
    unknown = sorted(set(names) - set(suites))
    if unknown:
        parser.error(f"unknown suite(s): {', '.join(unknown)}")
    status = 0
    for name in names:
        tests = [t for t in manifest["tests"] if t["suite"] == name]
        try:
            status |= fetch_suite(name, suites[name], tests, args.dest)
        except (OSError, RuntimeError, KeyError, zipfile.BadZipFile) as error:
            print(f"[fetch] {name}: {error}", file=sys.stderr)
            status = 1
    return status


if __name__ == "__main__":
    sys.exit(main())
