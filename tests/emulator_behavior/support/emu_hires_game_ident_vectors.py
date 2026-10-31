# SPDX-License-Identifier: MIT
"""Shared ROM-header vectors for the MIT core and future converter install mode."""
import json
from pathlib import Path
import subprocess
import sys

probe, fixture = sys.argv[1:]
for vector in json.loads(Path(fixture).read_text()):
    actual = subprocess.check_output([probe, vector["header_hex"]], text=True).removesuffix("\n")
    assert actual == (vector["ident"] or ""), f"{vector['label']}: {actual!r} != {vector['ident']!r}"
