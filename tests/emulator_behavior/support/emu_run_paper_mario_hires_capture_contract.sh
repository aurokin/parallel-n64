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
require_pattern "--smoke-mode MODE       Capture path: buttons|state (default: buttons)" \
  "usage text missing --smoke-mode"
require_pattern "--screenshot-at SEC     Seconds after launch to send SCREENSHOT (default: 27)" \
  "usage text missing --screenshot-at"
require_pattern "--state-load-delay SEC  Delay before sending state command in state mode (default: 2.2)" \
  "usage text missing --state-load-delay"
require_pattern "--state-pause-delay SEC Delay after state load before PAUSE_TOGGLE (default: 0.2)" \
  "usage text missing --state-pause-delay"
require_pattern "--state-shot-delay SEC  Delay after state load/pause before SCREENSHOT (default: 1.2)" \
  "usage text missing --state-shot-delay"
require_pattern "--state-close-delay SEC Delay after SCREENSHOT before close in state mode (default: 0.2)" \
  "usage text missing --state-close-delay"
require_pattern "--state-cmd CMD         Command to send for state load in state mode (default: LOAD_STATE)" \
  "usage text missing --state-cmd"
require_pattern "--state-pause           Send PAUSE_TOGGLE before screenshot in state mode" \
  "usage text missing --state-pause"
require_pattern "--no-state-pause        Skip PAUSE_TOGGLE in state mode (default)" \
  "usage text missing --no-state-pause"
require_pattern "--core-option K=V       Override a ParaLLEl core option in the temp options file" \
  "usage text missing --core-option"
require_pattern "--debug-hires           Enable PARALLEL_RDP_HIRES_DEBUG=1 for the run" \
  "usage text missing --debug-hires"
require_pattern 'SMOKE_START_RUNNER="$SCRIPT_DIR/run-n64-smoke-start.sh"' \
  "missing button smoke runner path"
require_pattern 'SMOKE_STATE_RUNNER="$SCRIPT_DIR/run-n64-smoke-state.sh"' \
  "missing state smoke runner path"
require_pattern 'smoke_mode="buttons"' "default smoke mode missing"
require_pattern 'DEFAULT_CORE_OPTIONS_FILE="$HOME/.config/retroarch/config/ParaLLEl N64/ParaLLEl N64.opt"' \
  "default core options path missing"
require_pattern 'DEFAULT_SCREENSHOT_DIR="$HOME/.config/retroarch/screenshots"' \
  "default screenshot path missing"
require_pattern 'force_fullscreen="${RUN_N64_FULLSCREEN:-0}"' \
  "default windowed fullscreen policy missing"
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
require_pattern 'smoke_cmd+=("--state-cmd" "$state_cmd")' "state command forwarding missing"
require_pattern 'smoke_cmd+=("--load-delay" "$state_load_delay")' "state load delay forwarding missing"
require_pattern 'smoke_cmd+=("--shot-delay" "$state_shot_delay")' "state shot delay forwarding missing"
require_pattern 'echo "Smoke mode: state"' "state mode logging missing"
require_pattern 'echo "Smoke mode: buttons"' "buttons mode logging missing"
require_pattern 'smoke_cmd+=(-- --appendconfig "$capture_cfg")' "appendconfig forwarding missing"
require_pattern 'find "$DEFAULT_SCREENSHOT_DIR" -maxdepth 1 -type f -name '\''*.png'\'' -newer "$stamp_file"' \
  "default screenshot fallback missing"

echo "emu_run_paper_mario_hires_capture_contract: PASS"
