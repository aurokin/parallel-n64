#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
CAPTURE_RUNNER="$REPO_ROOT/run-paper-mario-hires-intro22-capture.sh"
COMPARE_RUNNER="$REPO_ROOT/run-paper-mario-hires-intro22-compare.sh"
REFRESH_RUNNER="$REPO_ROOT/run-paper-mario-hires-intro22-refresh.sh"
SCENE_LIST_RUNNER="$REPO_ROOT/run-paper-mario-scenes.sh"

if [[ ! -f "$CAPTURE_RUNNER" ]]; then
  echo "FAIL: missing intro22 capture runner at $CAPTURE_RUNNER" >&2
  exit 1
fi

if [[ ! -f "$COMPARE_RUNNER" ]]; then
  echo "FAIL: missing intro22 compare runner at $COMPARE_RUNNER" >&2
  exit 1
fi

if [[ ! -f "$REFRESH_RUNNER" ]]; then
  echo "FAIL: missing intro22 refresh runner at $REFRESH_RUNNER" >&2
  exit 1
fi

if [[ ! -f "$SCENE_LIST_RUNNER" ]]; then
  echo "FAIL: missing Paper Mario scene list runner at $SCENE_LIST_RUNNER" >&2
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
require_pattern 'parallel_screenshot_at="22"' "$CAPTURE_RUNNER" \
  "intro22 capture should default parallel timing to the 22-second scene"
require_pattern 'glide_screenshot_at="19"' "$CAPTURE_RUNNER" \
  "intro22 capture should default glide timing to the aligned 19-second scene"
require_pattern "--parallel-screenshot-at SEC" "$CAPTURE_RUNNER" \
  "intro22 capture should allow parallel timing offsets"
require_pattern "--glide-screenshot-at SEC" "$CAPTURE_RUNNER" \
  "intro22 capture should allow glide timing offsets"
require_pattern "--pause-before-shot" "$CAPTURE_RUNNER" \
  "intro22 capture should expose pre-screenshot pause"
require_pattern "--timed-close-delay 10" "$CAPTURE_RUNNER" \
  "intro22 capture should use the safer timed close delay"
require_pattern "--require-hires" "$CAPTURE_RUNNER" \
  "intro22 capture should require hires validation on parallel"
require_pattern "--start-delay 40" "$CAPTURE_RUNNER" \
  "intro22 GLide capture should defer input past the screenshot"
require_pattern "--post-delay 2" "$CAPTURE_RUNNER" \
  "intro22 GLide capture should use the short post delay"
require_pattern "--pause-before-shot-delay \"\$pause_before_shot_delay\"" "$CAPTURE_RUNNER" \
  "intro22 capture should forward the pause delay"
require_pattern "--profile intro22" "$COMPARE_RUNNER" \
  "intro22 compare wrapper should pin the intro22 profile"
require_pattern 'run-paper-mario-hires-intro22-capture.sh' "$REFRESH_RUNNER" \
  "intro22 refresh runner should call the intro22 capture wrapper"
require_pattern 'run-paper-mario-hires-intro22-compare.sh' "$REFRESH_RUNNER" \
  "intro22 refresh runner should call the intro22 compare wrapper"
require_pattern 'run-paper-mario-open-compare.sh' "$REFRESH_RUNNER" \
  "intro22 refresh runner should reopen the compare viewer"
require_pattern '--profile intro22' "$REFRESH_RUNNER" \
  "intro22 refresh runner should reopen the canonical intro22 compare profile"
require_pattern "intro22" "$SCENE_LIST_RUNNER" \
  "scene list should include intro22"
require_pattern "file-select-state" "$SCENE_LIST_RUNNER" \
  "scene list should include file-select-state"

if ! bash -n "$CAPTURE_RUNNER" "$COMPARE_RUNNER" "$REFRESH_RUNNER" "$SCENE_LIST_RUNNER"; then
  echo "FAIL: intro22 wrappers failed bash -n" >&2
  exit 1
fi

echo "emu_run_paper_mario_hires_intro22_contract: PASS"
