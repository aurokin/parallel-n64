#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
RUNNER="$REPO_ROOT/run-paper-mario-scaling-capture.sh"

if [[ ! -f "$RUNNER" ]]; then
  echo "FAIL: missing run-paper-mario-scaling-capture.sh at $RUNNER" >&2
  exit 1
fi

require_pattern() {
  local pattern="$1"
  local message="$2"
  if ! rg -n --fixed-strings -- "$pattern" "$RUNNER" >/dev/null; then
    echo "FAIL: $message" >&2
    exit 1
  fi
}

require_pattern "run-paper-mario-scaling-capture.sh [options] [-- RUNNER_ARGS...]" \
  "usage text missing scaling capture invocation"
require_pattern "--smoke-mode state" "scaling capture must force state mode"
require_pattern "--core-option parallel-n64-parallel-rdp-hirestex=disabled" \
  "scaling capture must force HIRES off"
require_pattern "--core-option parallel-n64-parallel-rdp-upscaling=4x" \
  "scaling capture must force 4x upscaling"
require_pattern "--core-option parallel-n64-parallel-rdp-downscaling=disable" \
  "scaling capture must disable VI downscaling"
require_pattern 'exec "${cmd[@]}"' "scaling capture should exec the underlying runner"

echo "emu_run_paper_mario_scaling_capture_contract: PASS"
