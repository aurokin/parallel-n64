#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
RUNNER="$REPO_ROOT/run-paper-mario-hires-zoom-compare.sh"
TOOL="$REPO_ROOT/tools/paper_mario_hires_zoom_compare.py"

if [[ ! -f "$RUNNER" ]]; then
  echo "FAIL: missing run-paper-mario-hires-zoom-compare.sh at $RUNNER" >&2
  exit 1
fi

if [[ ! -f "$TOOL" ]]; then
  echo "FAIL: missing paper_mario_hires_zoom_compare.py at $TOOL" >&2
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

require_pattern "run-paper-mario-hires-zoom-compare.sh [options]" "$RUNNER" \
  "usage text missing hires zoom compare invocation"
require_pattern "--candidate PNG" "$RUNNER" "hires zoom compare should accept an explicit candidate"
require_pattern "--tag NAME" "$RUNNER" "hires zoom compare should resolve captures by tag"
require_pattern "--profile NAME" "$RUNNER" "hires zoom compare should accept scene profiles"
require_pattern "Defaults to the newest PNG under /tmp/parallel-n64-paper-mario-captures" "$RUNNER" \
  "hires zoom compare should default to the latest capture"
require_pattern "/tmp/parallel-n64-paper-mario-hires-compare" "$RUNNER" \
  "hires zoom compare should use the standard output root"
require_pattern 'latest_link="$OUTPUT_ROOT/latest-$profile"' "$RUNNER" \
  "hires zoom compare should maintain a latest-profile link"
require_pattern 'ln -sfn "$output_dir" "$latest_link"' "$RUNNER" \
  "hires zoom compare should refresh the latest-profile link"
require_pattern 'profile="intro22"' "$RUNNER" \
  "hires zoom compare should default to the intro22 profile"
require_pattern 'python3 "$COMPARE_TOOL"' "$RUNNER" \
  "hires zoom compare should invoke the python tool"
require_pattern 'DEFAULT_PROFILE = "intro22"' "$TOOL" \
  "python zoom compare tool should default to the intro22 profile"
require_pattern '"intro22": {' "$TOOL" \
  "python zoom compare tool should define the intro22 profile"
require_pattern '"noinput16": {' "$TOOL" \
  "python zoom compare tool should retain the legacy noinput16 profile"
require_pattern '"top_banner"' "$TOOL" \
  "python zoom compare tool should define the top banner crop"
require_pattern '"story_text"' "$TOOL" \
  "python zoom compare tool should define the intro story text crop"
require_pattern '"bottom_stage_grid"' "$TOOL" \
  "python zoom compare tool should define the bottom stage crop"
require_pattern '"left_stage_grid"' "$TOOL" \
  "python zoom compare tool should define the left stage crop"
require_pattern "summary.png" "$TOOL" \
  "python zoom compare tool should emit a summary image"
require_pattern "summary.txt" "$TOOL" \
  "python zoom compare tool should emit summary text"
require_pattern "summary.json" "$TOOL" \
  "python zoom compare tool should emit summary json"
require_pattern "candidate:" "$TOOL" \
  "python zoom compare tool should stamp candidate identity into the summary image"
require_pattern "oracle:" "$TOOL" \
  "python zoom compare tool should stamp oracle identity into the summary image"
require_pattern "whole_image:" "$TOOL" \
  "python zoom compare tool should report whole-image diff metadata"
require_pattern "candidate_sha256" "$TOOL" \
  "python zoom compare tool should record the candidate sha256"
require_pattern "oracle_sha256" "$TOOL" \
  "python zoom compare tool should record the oracle sha256"
require_pattern "exact_equal" "$TOOL" \
  "python zoom compare tool should record exact equality data"
require_pattern "diff_bbox" "$TOOL" \
  "python zoom compare tool should record diff bounding boxes"

if ! python3 -m py_compile "$TOOL"; then
  echo "FAIL: python zoom compare tool failed to compile" >&2
  exit 1
fi

echo "emu_run_paper_mario_hires_zoom_compare_contract: PASS"
