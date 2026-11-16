#!/usr/bin/env bash
# Only executable gate expressions are checked; discussion of the policy is allowed.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)"
python3 - "$ROOT" <<'PYTHON'
import ast
import re
import sys
from pathlib import Path
root = Path(sys.argv[1])
paths = [path for directory in ("tools/scenarios", "tools/adapters", "tools/regression", "tests")
         for path in (root / directory).rglob("*.py")]
for path in paths:
    tree = ast.parse(path.read_text())
    for node in ast.walk(tree):
        gate_call = isinstance(node, ast.Call) and (
            getattr(node.func, "id", "") in ("gate", "compare") or
            getattr(node.func, "attr", "").startswith("assert"))
        if isinstance(node, (ast.Compare, ast.Assert)) or gate_call:
            expression = ast.unparse(node)
            if re.search(r"ssim|psnr|perceptual_hash|image_similarity|gliden64.*sha256|sha256.*gliden64", expression, re.I):
                raise SystemExit(f"{path}:{node.lineno}: reference output or similarity in gate expression")
PYTHON
