#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
RUNNER="$REPO_ROOT/run-n64-smoke-start.sh"

if [[ ! -f "$RUNNER" ]]; then
  echo "FAIL: missing run-n64-smoke-start.sh at $RUNNER" >&2
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

require_pattern "run-n64-smoke-start.sh [options] [RUN_N64_ARGS...]" \
  "usage text missing run-n64-smoke-start invocation"
require_pattern "--start-delay SEC       Seconds before input automation begins (default: 20)" \
  "usage text missing --start-delay"
require_pattern "--post-delay SEC        Seconds to keep running after first input tick (default: 10)" \
  "usage text missing --post-delay"
require_pattern "--interval SEC          Seconds between input ticks (default: 5)" \
  "usage text missing --interval"
require_pattern "--button-hold-ms MS     Milliseconds to hold each button tap (default: 140)" \
  "usage text missing --button-hold-ms"
require_pattern "--buttons CSV           Buttons to tap each tick (default: start,a)" \
  "usage text missing --buttons"
require_pattern "--max-presses N         Cap total input ticks (default: 99)" \
  "usage text missing --max-presses"
require_pattern "--vpad-socket PATH      UNIX socket path for virtual gamepad daemon" \
  "usage text missing --vpad-socket"

require_pattern 'input_interval=5' "default interval contract missing"
require_pattern 'buttons_csv="start,a"' "default buttons contract missing"
require_pattern 'button_hold_ms=140' "default hold-ms contract missing"
require_pattern 'VPAD_TOOL="$SCRIPT_DIR/tools/virtual_gamepad.py"' "virtual gamepad tool path missing"
require_pattern 'python3 "$VPAD_TOOL" daemon --socket "$vpad_socket"' "daemon start contract missing"
require_pattern 'python3 "$VPAD_TOOL" send --socket "$vpad_socket" tap "$button" "$button_hold_ms"' \
  "virtual button tap contract missing"
require_pattern 'python3 "$VPAD_TOOL" stop --socket "$vpad_socket"' "daemon stop contract missing"
require_pattern 'trap cleanup EXIT' "cleanup trap contract missing"

require_pattern 'write_smoke_input_override() {' "smoke override writer missing"
require_pattern 'append_runner_passthrough() {' "runner passthrough helper missing"
require_pattern 'input_player1_start_btn = "9"' "virtual pad start mapping missing"
require_pattern 'input_player1_a_btn = "0"' "virtual pad a mapping missing"
require_pattern 'append_runner_passthrough --appendconfig "$smoke_input_cfg"' "appendconfig injection missing"
require_pattern 'echo "Smoke-start: retroarch input override=$smoke_input_cfg"' "override log line missing"
require_pattern 'rm -f "$smoke_input_cfg" || true' "override cleanup missing"

echo "emu_run_n64_smoke_start_contract: PASS"
