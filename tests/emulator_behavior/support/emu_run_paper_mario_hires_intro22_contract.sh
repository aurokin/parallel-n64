#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
CAPTURE_RUNNER="$REPO_ROOT/run-paper-mario-hires-intro22-capture.sh"
COMPARE_RUNNER="$REPO_ROOT/run-paper-mario-hires-intro22-compare.sh"

if [[ ! -f "$CAPTURE_RUNNER" ]]; then
  echo "FAIL: missing intro22 capture runner at $CAPTURE_RUNNER" >&2
  exit 1
fi

if [[ ! -f "$COMPARE_RUNNER" ]]; then
  echo "FAIL: missing intro22 compare runner at $COMPARE_RUNNER" >&2
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

require_pattern "run-paper-mario-hires-intro22-capture.sh [options]" "$CAPTURE_RUNNER" \
  "usage text missing intro22 capture invocation"
require_pattern "--smoke-mode timed" "$CAPTURE_RUNNER" \
  "intro22 capture should force timed mode on parallel"
require_pattern "--screenshot-at 22" "$CAPTURE_RUNNER" \
  "intro22 capture should target the 22-second scene"
require_pattern "--timed-close-delay 10" "$CAPTURE_RUNNER" \
  "intro22 capture should use the safer timed close delay"
require_pattern "--require-hires" "$CAPTURE_RUNNER" \
  "intro22 capture should require hires validation on parallel"
require_pattern "--start-delay 40" "$CAPTURE_RUNNER" \
  "intro22 GLide capture should defer input past the screenshot"
require_pattern "--post-delay 2" "$CAPTURE_RUNNER" \
  "intro22 GLide capture should use the short post delay"
require_pattern "--profile intro22" "$COMPARE_RUNNER" \
  "intro22 compare wrapper should pin the intro22 profile"

if ! bash -n "$CAPTURE_RUNNER" "$COMPARE_RUNNER"; then
  echo "FAIL: intro22 wrappers failed bash -n" >&2
  exit 1
fi

echo "emu_run_paper_mario_hires_intro22_contract: PASS"
