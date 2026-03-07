#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
RUNNER="$REPO_ROOT/run-paper-mario-scaling-compare.sh"
TOOL="$REPO_ROOT/tools/paper_mario_scaling_compare.py"

if [[ ! -f "$RUNNER" ]]; then
  echo "FAIL: missing run-paper-mario-scaling-compare.sh at $RUNNER" >&2
  exit 1
fi

if [[ ! -f "$TOOL" ]]; then
  echo "FAIL: missing paper_mario_scaling_compare.py at $TOOL" >&2
  exit 1
fi

require_pattern() {
  local pattern="$1"
  local file="$2"
  local message="$3"
  if ! rg -n --fixed-strings -- "$pattern" "$file" >/dev/null; then
    echo "FAIL: $message" >&2
    exit 1
  fi
}

require_pattern "run-paper-mario-scaling-compare.sh [options]" "$RUNNER" \
  "usage text missing scaling compare invocation"
require_pattern "--candidate PNG" "$RUNNER" "scaling compare should accept an explicit candidate"
require_pattern "--tag NAME" "$RUNNER" "scaling compare should resolve captures by tag"
require_pattern "/tmp/parallel-n64-paper-mario-scaling-compare" "$RUNNER" \
  "scaling compare should use the standard output root"
require_pattern 'exec python3 "$COMPARE_TOOL"' "$RUNNER" \
  "scaling compare should exec the python tool"
require_pattern "DEFAULT_ORACLE = Path(" "$TOOL" \
  "python compare tool should define the default oracle"
require_pattern "aligned-side-by-side.png" "$TOOL" \
  "python compare tool should emit a side-by-side image"
require_pattern "summary.txt" "$TOOL" \
  "python compare tool should emit summary text"

if ! python3 -m py_compile "$TOOL"; then
  echo "FAIL: python compare tool failed to compile" >&2
  exit 1
fi

echo "emu_run_paper_mario_scaling_compare_contract: PASS"
