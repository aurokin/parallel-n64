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
require_pattern "--start-delay SEC       Seconds before sending Start input (default: 20)" \
  "usage text missing --start-delay"
require_pattern "--post-delay SEC        Seconds to keep running after Start input (default: 10)" \
  "usage text missing --post-delay"
require_pattern "--start-key KEY         Key name to inject (default: auto-detect from RetroArch cfg)" \
  "usage text missing --start-key"
require_pattern "--window-name NAME      Window title match for xdotool (default: RetroArch)" \
  "usage text missing --window-name"
require_pattern "--start-burst-count N   Max number of Start key attempts (default: 99)" \
  "usage text missing --start-burst-count"
require_pattern "--start-burst-interval SEC" \
  "usage text missing --start-burst-interval"

require_pattern 'start_delay=20' "default start delay missing"
require_pattern 'post_delay=10' "default post delay missing"
require_pattern 'start_key=""' "default auto-detect start key missing"
require_pattern 'window_name="RetroArch"' "default window name missing"
require_pattern 'start_burst_count=99' "default burst count missing"
require_pattern 'start_burst_interval=5' "default burst interval missing"

require_pattern 'detect_retroarch_start_key()' "retroarch start-key detection missing"
require_pattern 'if ! command -v xdotool >/dev/null 2>&1; then' \
  "xdotool dependency guard missing"
require_pattern 'if [[ -z "${DISPLAY:-}" ]]; then' \
  "DISPLAY guard missing"
require_pattern '"$RUNNER" "${runner_args[@]}" &' \
  "run-n64 launch handoff missing"
require_pattern 'if send_start_key_once "$run_pid" "$key" "$window_name"; then' \
  "start-key injection path missing"
require_pattern 'kill -INT "$run_pid" 2>/dev/null || true' \
  "graceful SIGINT stop path missing"
require_pattern 'if (( rc == 130 || rc == 143 )); then' \
  "expected signal-exit normalization missing"

echo "emu_run_n64_smoke_start_contract: PASS"
