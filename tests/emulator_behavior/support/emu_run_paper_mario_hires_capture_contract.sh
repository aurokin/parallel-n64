#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
RUNNER="$REPO_ROOT/run-paper-mario-hires-capture.sh"

if [[ ! -f "$RUNNER" ]]; then
  echo "FAIL: missing run-paper-mario-hires-capture.sh at $RUNNER" >&2
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

require_pattern "run-paper-mario-hires-capture.sh [options] [-- RUN_N64_ARGS...]" \
  "usage text missing paper mario capture invocation"
require_pattern "--screenshot-at SEC     Seconds after launch to send SCREENSHOT (default: 27)" \
  "usage text missing --screenshot-at"
require_pattern "--core-option K=V       Override a ParaLLEl core option in the temp options file" \
  "usage text missing --core-option"
require_pattern "--debug-hires           Enable PARALLEL_RDP_HIRES_DEBUG=1 for the run" \
  "usage text missing --debug-hires"
require_pattern 'DEFAULT_CORE_OPTIONS_FILE="$HOME/.config/retroarch/config/ParaLLEl N64/ParaLLEl N64.opt"' \
  "default core options path missing"
require_pattern 'DEFAULT_SCREENSHOT_DIR="$HOME/.config/retroarch/screenshots"' \
  "default screenshot path missing"
require_pattern 'buttons_csv="start"' "default Paper Mario button sequence missing"
require_pattern 'max_presses=2' "default max presses missing"
require_pattern 'screenshot_at=27' "default screenshot timing missing"
require_pattern 'cp "$DEFAULT_CORE_OPTIONS_FILE" "$core_options_file"' \
  "temp core options copy missing"
require_pattern 'apply_core_option_overrides' "core option override application missing"
require_pattern 'screenshot_directory = "$capture_dir"' "capture appendconfig missing screenshot directory override"
require_pattern 'send_netcmd "SCREENSHOT"' "RetroArch screenshot command missing"
require_pattern 'export RUN_N64_CORE_OPTIONS_FILE="$core_options_file"' \
  "temp core options env export missing"
require_pattern 'export PARALLEL_RDP_HIRES_DEBUG=1' "hires debug export missing"
require_pattern 'core_option_overrides+=("${1:-}")' "core option override parsing missing"
require_pattern 'smoke_cmd+=("--buttons" "$buttons_csv")' "button forwarding missing"
require_pattern 'smoke_cmd+=(-- --appendconfig "$capture_cfg")' "appendconfig forwarding missing"
require_pattern 'find "$DEFAULT_SCREENSHOT_DIR" -maxdepth 1 -type f -name '\''*.png'\'' -newer "$stamp_file"' \
  "default screenshot fallback missing"

echo "emu_run_paper_mario_hires_capture_contract: PASS"
